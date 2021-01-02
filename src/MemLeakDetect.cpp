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

#define MAX_FRAMES 64

struct CallstackInfoShort {
    DWORD64 nFrames;
    DWORD64 frame[MAX_FRAMES];
};

struct AllocFreeEntry {
    void* heap;
    void* addr;
    u64 size;
    CallstackInfoShort* callstackInfo;
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

// static int gTotalCallstacks = 0;

static bool CanStackWalk() {
    bool ok = DynStackWalk64 && DynSymFunctionTableAccess64 && 
              DynSymGetModuleBase64 && DynSymFromAddr;
    return ok;
}

struct CallstackInfo {
    CONTEXT ctx;
    HANDLE hThread;
    int nFrames;
    DWORD64 callstack[MAX_FRAMES];
};

#define DWORDS_PER_CSI_BLOCK (1024 * 1020) / sizeof(DWORD64) // 1 MB
struct CallstackInfoBlock {
    CallstackInfoBlock* next;
    int nDwordsFree;
    // format of data is:
    // first DWORD64 is nFrames
    // followed by frames
    DWORD64 data[DWORDS_PER_CSI_BLOCK];
};

static_assert(sizeof(CallstackInfoBlock) < 1024 * 1024);

static CallstackInfoBlock* gCallstackInfoBlockFirst = nullptr;
static CallstackInfoBlock* gCallstackInfoBlockCurr = nullptr;

static CallstackInfoShort* AllocCallstackInfoShort(int nFrames) {
    if (nFrames == 0) {
        return nullptr;
    }
    // fast path
    CallstackInfoBlock* b = gCallstackInfoBlockFirst;
    if (!b || b->nDwordsFree < nFrames + 1) {
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

    }
    int off = DWORDS_PER_CSI_BLOCK - b->nDwordsFree;
    CallstackInfoShort* res = (CallstackInfoShort*)&b->data[off];
    b->nDwordsFree -= (nFrames + 1);
    return res;
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
    STACKFRAME64 stackFrame{};
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
        DWORD64 addr = stackFrame.AddrPC.Offset;
        cs->callstack[cs->nFrames++] = addr;
    }
}

static bool gCanStackWalk;

// which cannot deal with Omit Frame Pointers optimization (/Oy explicitly, turned
// implicitly by e.g. /O2)
// http://www.bytetalk.net/2011/06/why-rtlcapturecontext-crashes-on.html
#pragma optimize("", off)
// we also need to disable warning 4748 "/GS can not protect parameters and local variables
// from local buffer overrun because optimizations are disabled in function)"
#pragma warning(push)
#pragma warning(disable : 4748)
__declspec(noinline)
bool GetCurrentThreadCallstack(CallstackInfo* cs) {
    if (!gCanStackWalk) {
        return false;
    }

    cs->nFrames = 0;
    cs->hThread = GetCurrentThread();
    DynRtlCaptureContext(&cs->ctx);
    GetCallstackFrames(cs);
    return true;
}
#pragma optimize("", off)

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

// to filter out our own code
#define FRAMES_TO_SKIP 3

CallstackInfoShort* RecordCallstack() {
    CallstackInfo csi;
    bool ok = GetCurrentThreadCallstack(&csi);
    if (!ok) {
        return nullptr;
    }
    int n = csi.nFrames;
    if (n <= FRAMES_TO_SKIP) {
        return nullptr;
    }
    CallstackInfoShort* res = AllocCallstackInfoShort(n - FRAMES_TO_SKIP);
    res->nFrames = (DWORD64)n - FRAMES_TO_SKIP;
    for (int i = FRAMES_TO_SKIP-1; i < n; i++) {
        res->frame[i] = csi.callstack[i];
    }
    return res;
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
    e->callstackInfo = RecordCallstack();
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

#include "utils/WinUtil.h"

static bool InitializeSymbols() {
    AutoFreeWstr symbolPath = GetExeDir();
    if (!dbghelp::Initialize(symbolPath.Get(), false)) {
        dbglog("InitializeSymbols: dbghelp::Initialize() failed\n");
        return false;
    }

    if (dbghelp::HasSymbols()) {
        dbglog("InitializeSymbols(): skipping because dbghelp::HasSymbols()\n");
        return true;
    }

    if (!dbghelp::Initialize(symbolPath.Get(), true)) {
        dbglog("InitializeSymbols: second dbghelp::Initialize() failed\n");
        return false;
    }

    if (!dbghelp::HasSymbols()) {
        dbglog("InitializeSymbols: HasSymbols() false after downloading symbols\n");
        return false;
    }
    return true;
}

bool MemLeakInit() {
    MH_STATUS status;
    CrashIf(gRtlAllocateHeapOrig != nullptr); // don't call me twice

    InitializeSymbols();
    gCanStackWalk = CanStackWalk();

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

static AllocFreeEntry* FindMatchingFree(AllocFreeEntryBlock* b, AllocFreeEntry* eAlloc, int nStart) {
    while (b) {
        for (int i = nStart; i < b->nUsed; i++) {
            AllocFreeEntry* e = &b->entries[i];
            if (e->typ == TYPE_ALLOC) {
                continue;
            }
            if (e->heap != eAlloc->heap) {
                continue;
            }
            if (e->addr != eAlloc->addr) {
                continue;
            }
            return e;
        }
        b = b->next;
    }
    return nullptr;
}

static void DumpAllocEntry(AllocFreeEntry* e) {
    static int nDumped = 0;
    if (nDumped > 25) {
        return;
    }
    nDumped++;
    dbglogf("\nunfreed entry: 0x%p, size: %d\n", e->addr, (int)e->size);
    str::Str s;
    CallstackInfoShort* cis = e->callstackInfo;
    if (!cis) {
        return;
    }
    for (int i = 0; i < (int)cis->nFrames; i++) {
        DWORD64 addr = cis->frame[i];
        s.Reset();
        dbghelp::GetAddressInfo(s, addr);
        dbglogf("  %s", s.Get());
    }
}

void DumpMemLeaks() {
    MH_DisableHook(MH_ALL_HOOKS);
    // TODO: Lock(), just in case?

    int nAllocs = gAllocs;
    int nFrees = gFrees;
    dbglogf("allocs: %d, frees: %d\n", nAllocs, nFrees);

    AllocFreeEntryBlock *b = gAllocFreeBlockFirst;
    int nUnfreed = 0;
    while (b) {
        for (int i = 0; i < b->nUsed; i++) {
            AllocFreeEntry* e = &b->entries[i];
            if (e->typ == TYPE_FREE) {
                continue;
            }
            AllocFreeEntry* eFree = FindMatchingFree(b, e, i + 1);
            if (eFree == nullptr) {
                nUnfreed++;
                DumpAllocEntry(e);
            }
        }
        b = b->next;
    }
    dbglogf("%d unfreed\n", nUnfreed);
    HeapDestroy(gHeap);
}
