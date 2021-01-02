/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/LogDbg.h"
#include "utils/MinHook.h"

// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlallocateheap
PVOID(WINAPI* gRtlAllocateHeapOrig)(PVOID heapHandle, ULONG flags, SIZE_T size);
// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlfreeheap
BOOLEAN(WINAPI* gRtlFreeHeapOrig)(PVOID heapHandle, ULONG flags, PVOID heapBase);

#define TYPE_ALLOC 0
#define TYPE_FREE 1

struct AllocFreeEntry {
    void* heap;
    void* addr;
    u64 size;
    void* callstackInfo;
    DWORD threadId;
    u32 typ;
};

#define ENTRIES_PER_BLOCK 1024 * 16

struct AllocFreeEntryBlock {
    AllocFreeEntryBlock* next;
    int nUsed;
    AllocFreeEntry entries[ENTRIES_PER_BLOCK];
};

static CRITICAL_SECTION gMutex;
static int gRecurCount = 0;
static AllocFreeEntryBlock* gAllocFreeBlockFirst = nullptr;
static AllocFreeEntryBlock* gAllocFreeBlockCurr = nullptr;
static int gAllocs = 0;
static int gFrees = 0;
static PVOID gHeap = nullptr;

static void* AllocPrivate(size_t n) {
    // note: HeapAlloc and RtlHeapAlloc seem to be the same
    return gRtlAllocateHeapOrig(gHeap, 0, n);
}

template <typename T>
static T* AllocStructPrivate() {
    T* res = (T*)AllocPrivate(sizeof(T));
    if (res) {
        ZeroStruct(res);
    }
    return res;
}

int gTotalCallstacks = 0;

static bool CanStackWalk() {
    bool ok = DynSymCleanup && DynSymGetOptions && DynSymSetOptions && DynStackWalk64 && DynSymFunctionTableAccess64 &&
              DynSymGetModuleBase64 && DynSymFromAddr;
    return ok;
}

#define MAX_FRAMES 64
struct CallstackInfo {
    CONTEXT ctx;
    HANDLE hThread;
    int nFrames;
    DWORD callstack[MAX_FRAMES];
};

#define DWORDS_PER_CSI_BLOCK (1024 * 1020) / 8 // 1 MB
struct CallstackInfoBlock {
    CallstackInfoBlock* next;
    int nDwordsFree;
    // format of data is:
    // first DWORD is nFrames
    // followed by frames
    DWORD data[DWORDS_PER_CSI_BLOCK];
};

struct CallstackInfoShort {
    DWORD nFrames;
    DWORD frame[MAX_FRAMES];
};

static_assert(sizeof(CallstackInfoBlock) < 1024 * 1024);

static CallstackInfoBlock* gCallstackInfoBlockFirst = nullptr;
static CallstackInfoBlock* gCallstackInfoBlockCurr = nullptr;

static CallstackInfoShort* AllocCallstackInfoShort(CallstackInfo* ci) {
    int n = ci->nFrames;
    if (n == 0) {
        return nullptr;
    }
    // fast path
    CallstackInfoBlock* b = gCallstackInfoBlockFirst;
    if (b && b->nDwordsFree <= n + 1) {
        int off = DWORDS_PER_CSI_BLOCK - b->nDwordsFree;
        DWORD* res = &b->data[off];
        b->nDwordsFree -= (n + 1);
        return (CallstackInfoShort*)res;
    }
    b = AllocStructPrivate<CallstackInfoBlock>();
    if (!b) {
        return nullptr;
    }
    b->next = nullptr;
    b->nDwordsFree = DWORDS_PER_CSI_BLOCK;
    if (!gCallstackInfoBlockFirst) {
        gCallstackInfoBlockFirst = b;
    }
    if (gCallstackInfoBlockCurr) {
        gCallstackInfoBlockCurr->next = b;
    }
    gCallstackInfoBlockCurr = b;

    DWORD* res = &b->data[0];
    b->nDwordsFree -= (n + 1);
    return (CallstackInfoShort*)res;
}

static bool GetStackFrameInfo(CallstackInfo* cs, STACKFRAME64* stackFrame) {
#if defined(_WIN64)
    int machineType = IMAGE_FILE_MACHINE_AMD64;
#else
    int machineType = IMAGE_FILE_MACHINE_I386;
#endif
    HANDLE proc = GetCurrentProcess();
    BOOL ok = DynStackWalk64(machineType, proc, cs->hThread, stackFrame, &cs->ctx, nullptr, DynSymFunctionTableAccess64,
                             DynSymGetModuleBase64, nullptr);
    if (!ok) {
        return false;
    }

    DWORD64 addr = stackFrame->AddrPC.Offset;
    if (0 == addr) {
        return true;
    }
    if (addr == stackFrame->AddrReturn.Offset) {
        return false;
    }

    return true;
}

static void GetCallstackFrames(CallstackInfo* cs) {
    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(stackFrame));
#ifdef _WIN64
    stackFrame.AddrPC.Offset = cs->ctx.Rip;
    stackFrame.AddrFrame.Offset = cs->ctx.Rbp;
    stackFrame.AddrStack.Offset = cs->ctx.Rsp;
#else
    stackFrame.AddrPC.Offset = cs->ctx.Eip;
    stackFrame.AddrFrame.Offset = cs->ctx.Ebp;
    stackFrame.AddrStack.Offset = cs->ctx.Esp;
#endif
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;

    cs->nFrames = 0;
    while (cs->nFrames < MAX_FRAMES) {
        if (!GetStackFrameInfo(cs, &stackFrame)) {
            break;
        }
        cs->callstack[cs->nFrames] = stackFrame.AddrPC.Offset;
        cs->nFrames++;
    }
}

static bool GetCurrentThreadCallstack(CallstackInfo* cs) {
    cs->nFrames = 0;
    cs->hThread = GetCurrentThread();
    if (!CanStackWalk()) {
        return false;
    }
    DynRtlCaptureContext(&cs->ctx);
    GetCallstackFrames(cs);
    return true;
}

static AllocFreeEntry* GetAllocFreeEntry() {
    // fast path
    AllocFreeEntryBlock* b = gAllocFreeBlockCurr;
    if (b && b->nUsed < ENTRIES_PER_BLOCK) {
        AllocFreeEntry* res = &b->entries[b->nUsed];
        b->nUsed++;
        return res;
    }
    b = AllocStructPrivate<AllocFreeEntryBlock>();
    if (!b) {
        return nullptr;
    }
    b->next = nullptr;
    b->nUsed = 1;
    if (!gAllocFreeBlockFirst) {
        gAllocFreeBlockFirst = b;
    }
    if (gAllocFreeBlockCurr) {
        gAllocFreeBlockCurr->next = b;
    }
    gAllocFreeBlockCurr = b;
    return &b->entries[0];
}

CallstackInfoShort* RecordCallstack() {
    CallstackInfo cs;
    bool ok = GetCurrentThreadCallstack(&cs);
    if (!ok) {
        return nullptr;
    }
    CallstackInfoShort* cis = AllocCallstackInfoShort(&cs);
    return cis;
}

static void RecordAllocOrFree(u32 typ, void* heap, void* addr, u64 size) {
    AllocFreeEntry* e = GetAllocFreeEntry();
    if (!e) {
        return;
    }
    e->heap = heap;
    e->addr = addr;
    e->size = size;
    e->typ = typ;
    e->threadId = GetCurrentThreadId();
    e->callstackInfo = (void*)RecordCallstack();
}

static void Lock() {
    EnterCriticalSection(&gMutex);
}

static void Unlock() {
    LeaveCriticalSection(&gMutex);
}

PVOID WINAPI RtlAllocateHeapHook(PVOID heapHandle, ULONG flags, SIZE_T size) {
    Lock();
    gRecurCount++;
    gAllocs++;
    PVOID res = gRtlAllocateHeapOrig(heapHandle, flags, size);
    if ((res != nullptr) && (gRecurCount == 1)) {
        RecordAllocOrFree(TYPE_ALLOC, heapHandle, res, size);
    }
    gRecurCount--;
    Unlock();
    return res;
}

BOOLEAN WINAPI RtlFreeHeapHook(PVOID heapHandle, ULONG flags, PVOID heapBase) {
    Lock();
    gRecurCount++;
    gFrees++;
    BOOLEAN res = gRtlFreeHeapOrig(heapHandle, flags, heapBase);
    if (res && (gRecurCount == 1)) {
        RecordAllocOrFree(TYPE_FREE, heapHandle, heapBase, 0);
    }
    gRecurCount--;
    Unlock();
    return res;
}

bool MemLeakInit() {
    MH_STATUS status;
    CrashIf(gRtlAllocateHeapOrig != nullptr); // don't call me twice

    InitializeCriticalSection(&gMutex);
    gHeap = HeapCreate(0, 0, 0);
    MH_Initialize();

    status = MH_CreateHookApiEx(L"ntdll.dll", "RtlAllocateHeap", RtlAllocateHeapHook, (void**)&gRtlAllocateHeapOrig,
                                nullptr);
    if (status != MH_OK) {
        return false;
    }
    status = MH_CreateHookApiEx(L"ntdll.dll", "RtlFreeHeap", RtlFreeHeapHook, (void**)&gRtlFreeHeapOrig, nullptr);
    if (status != MH_OK) {
        return false;
    }

    MH_EnableHook(MH_ALL_HOOKS);
    return true;
}

void DumpMemLeaks() {
    MH_DisableHook(MH_ALL_HOOKS);
    // TODO: Lock(), just in case?

    int nAllocs = gAllocs;
    int nFrees = gFrees;
    dbglogf("allocs: %d, frees: %d\n", nAllocs, nFrees);
    // TODO: figure out allocs without a free

    HeapDestroy(gHeap);
}
