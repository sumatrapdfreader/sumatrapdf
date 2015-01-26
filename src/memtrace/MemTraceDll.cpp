/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

/*
memtrace.dll enables tracing memory allocations in arbitrary programs.
It hooks RtlFreeHeap etc. APIs in the process and sends collected information
(allocation address/size, address of freed memory, callstacks if possible etc.)
to an external collection and visualization process via named pipe.

The dll can either be injected into arbitrary processes or an app can load it
by itself (easier to integrate than injecting dll).

If the collection process doesn't run when memtrace.dll is initialized, we do
nothing.

The data sent via named pipe is framed as messages (packets):
uint16  messageLen; // length of the data follows
uint16  msgId;      // determines how the data is to be decoded
byte    data[];     // bytes for a given message
*/

#include "BaseUtil.h"
#include "MemTraceDll.h"
#include "nsWindowsDllInterceptor.h"
#include "Timer.h"
#include "WinUtil.h"

#define NOLOG 0
#include "DebugLog.h"

#define PIPE_NAME "\\\\.\\pipe\\MemTraceCollectorPipe"

#define MIN_BLOCK_SIZE      1024*32
#define MEMCOPY_THRESHOLD   1024

// a block of memory. data to be sent is appended at the end,
// sending thread consumes the data from the beginning
struct MemBlock {
    MemBlock *  next;
    size_t      size;
    size_t      used; // includes sent, if used == sent => everything has been sent
    size_t      sent;

    size_t      Left() const { return size - used; }
    byte *      Data() const { return (byte*)this + sizeof(MemBlock) + used; }
    size_t      UnsentLen() const { return used - sent; }
    byte *      UnsentData() const { return (byte*)this + sizeof(MemBlock) + sent; }
    void        Append(byte *data, size_t len);
    void        Reset() { used = 0; sent = 0; }
    // data follows here
};

struct PerThreadData {
    bool        inAlloc;
    bool        inFree;
};

static HANDLE           gModule;

// heap for doing our allocations
static HANDLE           gHeap;

// tls indes for per-thread data
static DWORD            gTlsIndex;

// unnamed pipe used to send data to the collector client
// it's nullptr if we failed to open the pipe in the first place
// or if we failed to write data to the pipe (e.g. collector
// client exited)
static HANDLE           gPipe;

// Data is written to gCurrBlock. If it's filled up, the block
// is appended to the end of gBlocksToSend.
// Sending thread first sends data from gBlocksToSend and
// if it's empty, from gCurrBlock
static MemBlock *       gCurrBlock;
static MemBlock *       gBlocksToSend;
static MemBlock *       gBlocksFreeList;
static int              gBlocksAllocated;

// protects access to gToSend and gBlocksFreList
static CRITICAL_SECTION gMemMutex;
// event that is notified when new data for sending
// is available
static HANDLE           gSendThreadEvent;
static HANDLE           gSendThread;

static PerThreadData *GetPerThreadData(PerThreadData *threadDataEmergency)
{
    void *data = TlsGetValue(gTlsIndex);
    if (data)
        return (PerThreadData*)data;
    data = HeapAlloc(gHeap, 0, sizeof(PerThreadData));
    if (!data) {
        lf("memtrace.dll: failed to allocate PerThreadData");
        return threadDataEmergency;
    }
    if (!TlsSetValue(gTlsIndex, data)) {
        HeapFree(gHeap, 0, data);
        lf("memtrace.dll: TlsSetValue() failed");
        return threadDataEmergency;
    }
    PerThreadData *tmp = (PerThreadData*)data;
    tmp->inAlloc = false;
    tmp->inFree = false;
    return tmp;
}

// the caller should ensure that block has enough data
void MemBlock::Append(byte *data, size_t len)
{
    ScopedCritSec cs(&gMemMutex);
    CrashIf(len > Left());
    memcpy(Data(), data, len);
    used += len;
    CrashIf(used > size);
}

static void InsertAtEnd(MemBlock **root, MemBlock *el)
{
    CrashIf(el->next);
    if (!*root) {
        *root = el;
        return;
    }
    MemBlock *curr = *root;
    while (curr->next) {
        curr = curr->next;
    }
    MemBlock **lastPtr = &curr;
    CrashIf((*lastPtr)->next);
    (*lastPtr)->next = el;
}

static MemBlock *GetBlock(size_t len)
{
    ScopedCritSec cs(&gMemMutex);

    // first try current block
    if (gCurrBlock && (gCurrBlock->Left() >= len))
        return gCurrBlock;

    MemBlock *newBlock;
    size_t dataSize;
    if (gBlocksFreeList && gBlocksFreeList->size >= len) {
        newBlock = gBlocksFreeList;
        dataSize = newBlock->size;
        gBlocksFreeList = gBlocksFreeList->next;
    } else {
        // allocate new block
        dataSize = len;
        if (dataSize < MIN_BLOCK_SIZE)
            dataSize = MIN_BLOCK_SIZE;
        // TODO: have at least 2 MemBlocks allocated as a static memory?
        newBlock = (MemBlock*)HeapAlloc(gHeap, 0, sizeof(MemBlock) + dataSize);
        if (!newBlock) {
            lf("memtrace.dll: failed to allocate a block");
            return nullptr;
        }
    }

    //lf("memtrace.dll: allocated a new block");
    ++gBlocksAllocated;
    newBlock->next = nullptr;
    newBlock->size = dataSize;
    newBlock->used = 0;
    newBlock->sent = 0;

    if (gCurrBlock)
        InsertAtEnd(&gBlocksToSend, gCurrBlock);

    gCurrBlock= newBlock;
    return newBlock;
}

// put the block on a free list queue
// note: could free the whole free list if it grows too big,
// but then again it shouldn't happen often
static void FreeBlock(MemBlock *block)
{
    ScopedCritSec cs(&gMemMutex);
    CrashIf(0 != block->UnsentLen());
    block->next = gBlocksFreeList;
    gBlocksFreeList = block;
}

// done at the very end so no need to lock
static void FreeAllBlocks()
{
    HeapFree(gHeap, 0, gCurrBlock);
    MemBlock *next;
    MemBlock *curr = gBlocksFreeList;
    while (curr) {
        next = curr->next;
        HeapFree(gHeap, 0, curr);
        curr = next;
    }
    curr = gBlocksToSend;
    while (curr) {
        next = curr->next;
        HeapFree(gHeap, 0, curr);
        curr = next;
    }
}

enum SerializeMsgId {
    AllocDataMsgId  = 1,
    FreeDataMsgId   = 2
};

// TODO: since we use 32 bits, we don't support 64 builds
struct AllocData {
    uint32    size;
    uint32    addr;
};

struct FreeData {
    uint32    addr;
};

struct MemberSerializeInfo {
    enum Type { UInt16, Int32, UInt32, Int64, UInt64, Sentinel };
    Type    type;
    int     offset;

    bool IsSentinel() const { return Sentinel == type; };
};

struct TypeSerializeInfo {
    SerializeMsgId          msgId;
    MemberSerializeInfo *   members;
};

#define SERIALIZEINFO_SENTINEL { MemberSerializeInfo::Sentinel, 0 }

// TODO: manually defining serialization info is fine for few types
// but if we get more, this should be auto-generated by e.g. defining
// the types in C# and generating the C descriptions in C# by reflecting
// C# information.
MemberSerializeInfo allocDataSerMemberInfo[] = {
    { MemberSerializeInfo::UInt32, offsetof(AllocData, size) },
    { MemberSerializeInfo::UInt32, offsetof(AllocData, addr) },
    SERIALIZEINFO_SENTINEL
};

TypeSerializeInfo allocDataTypeInfo = {
    AllocDataMsgId,
    allocDataSerMemberInfo
};

MemberSerializeInfo freeDataSerMemberInfo[] = {
    { MemberSerializeInfo::UInt32, offsetof(FreeData, addr) },
    SERIALIZEINFO_SENTINEL
};

TypeSerializeInfo freeDataTypeInfo = {
    FreeDataMsgId,
    freeDataSerMemberInfo
};

// TODO: we should serialize numbers using variable-length encoding
// like e.g. in snappy.
// Assumes that data points to either int16 or uint16 value. Appends
// serialized data to serOut and returns number of bytes needed to serialize
static inline int SerNum16(byte *data, Vec<byte>& serOut)
{
    byte *dataOut = serOut.AppendBlanks(2);
    *dataOut++ = *data++; *dataOut++ = *data++;
    return 2;
}

static inline int SerNum32(byte *data, Vec<byte>& serOut)
{
    byte *dataOut = serOut.AppendBlanks(4);
    *dataOut++ = *data++; *dataOut++ = *data++;
    *dataOut++ = *data++; *dataOut++ = *data++;
    return 4;
}

static inline int SerNum64(byte *data, Vec<byte>& serOut)
{
    byte *dataOut = serOut.AppendBlanks(8);
    *dataOut++ = *data++; *dataOut++ = *data++;
    *dataOut++ = *data++; *dataOut++ = *data++;
    *dataOut++ = *data++; *dataOut++ = *data++;
    *dataOut++ = *data++; *dataOut++ = *data++;
    return 8;
}

// data is a pointer to a struct being serialized and typeInfo describes
// the struct. res is a result as a stream of bytes
static void SerializeType(byte *data, TypeSerializeInfo *typeInfo, Vec<byte>& msg)
{
    msg.Reset();
    // reserve space for the size of the message, which we only know
    // after serializeing the data. We're making an assumption here
    // that serialized data will be smaller than 65k
    // note: we can easily relax that by using uint32 for len
    uint16 *msgLenPtr = (uint16*)msg.AppendBlanks(2);
    uint16 msgId = (uint16)typeInfo->msgId;
    int msgLen = SerNum16((byte*)&msgId, msg);

    for (MemberSerializeInfo *member = typeInfo->members; !member->IsSentinel(); ++ member) {
        switch (member->type) {
            case MemberSerializeInfo::UInt16:
                msgLen += SerNum16(data + member->offset, msg);
                break;
            case MemberSerializeInfo::Int32:
            case MemberSerializeInfo::UInt32:
                msgLen += SerNum32(data + member->offset, msg);
                break;
            case MemberSerializeInfo::Int64:
            case MemberSerializeInfo::UInt64:
                msgLen += SerNum32(data + member->offset, msg);
                break;
        }
    }
    *msgLenPtr = (uint16)msgLen;
}

static bool OpenPipe()
{
    gPipe = CreateFileA(PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED, nullptr);
    if (INVALID_HANDLE_VALUE == gPipe) {
        gPipe = nullptr;
        return false;
    }
    return true;
}

static void ClosePipe()
{
    if (gPipe && (INVALID_HANDLE_VALUE != gPipe))
        CloseHandle(gPipe);
    gPipe = nullptr;
}

static bool WriteToPipe(const byte *s, size_t len)
{
    DWORD written;
    if (!gPipe)
        return false;
    DWORD toWrite = (DWORD)len;
    BOOL ok = WriteFile(gPipe, s, toWrite, &written, nullptr);
    if (!ok) {
        lf("memtrace.dll: WriteToPipe() failed");
        LogLastError();
        ClosePipe();
        return false;
    }

    if (written != toWrite) {
        lf("memtrace.dll: only wrote %d out of %d", (int)written, (int)toWrite);
        ClosePipe();
        return false;
    }
    return true;
}

// If the data to be sent fits in the buf, we copy it from MemBlock to buf
// Otherwise we return the whole MemBlock, to minimize the time spent inside
// the critical section to minimize blocking in hooked functions
static MemBlock* GetDataToSend(byte *buf, size_t bufSize, size_t *bufLenOut)
{
    ScopedCritSec cs(&gMemMutex);
    // sent queued blocks first
    if (gBlocksToSend) {
        MemBlock *tmp = gBlocksToSend;
        gBlocksToSend = gBlocksToSend->next;
        return tmp;
    }

    // no queued data - send gCurrBlock
    if (!gCurrBlock) {
        *bufLenOut = 0;
        return nullptr;
    }
    if (gCurrBlock->UnsentLen() > bufSize) {
        MemBlock *tmp = gCurrBlock;
        gCurrBlock = nullptr;
        return tmp;
    }
    size_t len = gCurrBlock->UnsentLen();
    memcpy(buf, gCurrBlock->UnsentData(), len);
    *bufLenOut = len;
    gCurrBlock->sent += len;
    CrashIf(gCurrBlock->sent > gCurrBlock->size);
    CrashIf(0 != gCurrBlock->UnsentLen());
    gCurrBlock->Reset();
    return nullptr;
}

static bool gStopSendThread = false;

static void SendQueuedMessages()
{
    byte        buf[MEMCOPY_THRESHOLD];
    size_t      dataLen;
    MemBlock *  block;

    for (;;)
    {
        if (!gPipe)
            return;

        dataLen = 0;
        block = GetDataToSend(buf, dimof(buf), &dataLen);
        byte *dataToSend = buf;
        if (block) {
            dataToSend = block->UnsentData();
            dataLen = block->UnsentLen();
        }

        if (0 == dataLen) {
            CrashIf(block);
            return;
        }

        //lf("memtrace.dll: sending %d bytes", (int)dataLen);
        WriteToPipe(dataToSend, dataLen);
        if (block) {
            block->sent += dataLen;
            FreeBlock(block);
        }
    }
}

static DWORD WINAPI DataSendThreadProc(void* data)
{
    // if pipe was closed, we exit the thread
    while (gPipe) {
        DWORD res = WaitForSingleObject(gSendThreadEvent, INFINITE);
        if (gStopSendThread) {
            lf("memtrace.dll: DataSendThreadProc, gStopSendThread is true");
            return 0;
        }
        if (WAIT_OBJECT_0 != res)
            continue;

        SendQueuedMessages();
    }
    lf("memtrace.dll: DataSendThreadProc ended");
    return 0;
}

// transfers the data to a thread that does the actual sending
static void QueueMessageForSending(Vec<byte>& msg)
{
    size_t len = msg.Size();
    if (0 == len)
        return;

    MemBlock *block = GetBlock(len);
    if (block) {
        block->Append(msg.LendData(), len);
        SetEvent(gSendThreadEvent);
    } else {
        lf("memtrace.dll: QueueMessageForSending() couldn't queu %d bytes", (int)len);
    }
}

WindowsDllInterceptor gNtdllIntercept;

//http://msdn.microsoft.com/en-us/library/windows/hardware/ff552108(v=vs.85).aspx
PVOID (WINAPI *gRtlAllocateHeapOrig)(PVOID heapHandle, ULONG flags, SIZE_T size);
// http://msdn.microsoft.com/en-us/library/windows/hardware/ff552276(v=vs.85).aspx
BOOLEAN (WINAPI *gRtlFreeHeapOrig)(PVOID heapHandle, ULONG flags, PVOID heapBase);

PVOID WINAPI RtlAllocateHeapHook(PVOID heapHandle, ULONG flags, SIZE_T size)
{
    if (!gPipe || (gHeap == heapHandle) || gStopSendThread)
        return gRtlAllocateHeapOrig(heapHandle, flags, size);

    PerThreadData threadDataEmergency = { true, true };
    PerThreadData *threadData = GetPerThreadData(&threadDataEmergency);
    bool inAlloc = threadData->inAlloc;
    // prevent infinite recursion
    threadData->inAlloc = true;

    PVOID res = gRtlAllocateHeapOrig(heapHandle, flags, size);
    if (inAlloc)
        return res;

    AllocData d = { (uint32)size, (uint32)res };
    Vec<byte> msg;
    SerializeType((byte*)&d, &allocDataTypeInfo, msg);
    QueueMessageForSending(msg);
    threadData->inAlloc = false;
    return res;
}

BOOLEAN WINAPI RtlFreeHeapHook(PVOID heapHandle, ULONG flags, PVOID heapBase)
{
    if (!gPipe || (gHeap == heapHandle) || gStopSendThread)
        return gRtlFreeHeapOrig(heapHandle, flags, heapBase);

    PerThreadData threadDataEmergency = { true, true };
    PerThreadData *threadData = GetPerThreadData(&threadDataEmergency);
    bool inFree = threadData->inFree;
    // prevent infinite recursion
    threadData->inFree = true;
    BOOLEAN res = gRtlFreeHeapOrig(heapHandle, flags, heapBase);
    if (inFree)
        return res;

    FreeData d = { (uint32)heapBase };
    Vec<byte> msg;
    SerializeType((byte*)&d, &freeDataTypeInfo, msg);
    QueueMessageForSending(msg);
    threadData->inFree = false;
    return res;
}

static void InstallHooks()
{
    gNtdllIntercept.Init("ntdll.dll");
    bool ok = gNtdllIntercept.AddHook("RtlAllocateHeap", reinterpret_cast<intptr_t>(RtlAllocateHeapHook), (void**) &gRtlAllocateHeapOrig);
    if (ok)
        lf("memtrace.dll: Hooked RtlAllocateHeap");
    else
        lf("memtrace.dll: failed to hook RtlAllocateHeap");

    ok = gNtdllIntercept.AddHook("RtlFreeHeap", reinterpret_cast<intptr_t>(RtlFreeHeapHook), (void**) &gRtlFreeHeapOrig);
    if (ok)
        lf("memtrace.dll: Hooked RtlFreeHeap");
    else
        lf("memtrace.dll: failed to hook RtlFreeHeap");
}

// TODO: I should probably move all initalization inside the thread
// We should minimize the amount of work done in DLL_PROCESS_ATTACH as per
// http://blogs.msdn.com/b/oleglv/archive/2003/10/24/56141.aspx
static BOOL ProcessAttach()
{
    lf("memtrace.dll: ProcessAttach()");
    if (!OpenPipe()) {
        lf("memtrace.dll: couldn't open pipe");
        return FALSE;
    } else {
        lf("memtrace.dll: opened pipe");
    }

    gHeap = HeapCreate(0, 0, 0);
    if (!gHeap) {
        lf("memtrace.dll: failed to create heap");
        return FALSE;
    }

    InitializeCriticalSection(&gMemMutex);
    gSendThreadEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!gSendThreadEvent) {
        lf("memtrace.dll: couldn't create gSendThreadEvent");
        return FALSE;
    }
    gSendThread = CreateThread(nullptr, 0, DataSendThreadProc, nullptr, 0, 0);
    if (!gSendThread) {
        lf("memtrace.dll: couldn't create gSendThread");
        return FALSE;
    }
    InstallHooks();
    return TRUE;
}

static void TerminateSendingThread()
{
    if (!gSendThread)
        return;
    lf("memtrace.dll: TerminateSendingThread() setting gStopSendThread=TRUE and signaling gSendThreadEvent");
    gStopSendThread = true;
    SetEvent(gSendThreadEvent);
    lf("memtrace.dll: TerminateSendingThread() waiting for gSendThread");
    Timer t(true);
    DWORD res = WaitForSingleObject(gSendThread, 5*1024);
    t.Stop();
    lf("%.2f ms to terminate the thread", t.GetTimeInMs());
    if (WAIT_OBJECT_0 == res) {
        lf("memtrace.dll: thread quit by itself");
    } else {
        TerminateThread(gSendThread, 1);
        lf("memtrace.dll: terminated sending thread");
    }
    CloseHandle(gSendThread);
    SendQueuedMessages();
}

static BOOL ProcessDetach()
{
    lf("memtrace.dll: ProcessDetach()");
    TerminateSendingThread();
    ClosePipe();
    DeleteCriticalSection(&gMemMutex);
    CloseHandle(gSendThreadEvent);
    FreeAllBlocks();
    lf("memtrace.dll: allocated total %d blocks", gBlocksAllocated);
    HeapDestroy(gHeap);
    return TRUE;
}

static BOOL ThreadAttach()
{
    return TRUE;
}

static BOOL ThreadDetach()
{
    return TRUE;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
    gModule = hModule;
    if (DLL_PROCESS_ATTACH == dwReason)
        return ProcessAttach();
    if (DLL_PROCESS_DETACH == dwReason)
        return ProcessDetach();
    if (DLL_THREAD_ATTACH == dwReason)
        return ThreadAttach();
    if (DLL_THREAD_DETACH == dwReason)
        return ThreadDetach();

    return TRUE;
}
