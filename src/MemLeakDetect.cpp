/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinDynCalls.h"
#include "utils/DbgHelpDyn.h"
#include "utils/WinUtil.h"
#include "utils/LogDbg.h"
#include "utils/MinHook.h"

// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlallocateheap
PVOID(WINAPI* gRtlAllocateHeapOrig)(PVOID heapHandle, ULONG flags, SIZE_T size) = nullptr;
// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlfreeheap
BOOLEAN(WINAPI* gRtlFreeHeapOrig)(PVOID heapHandle, ULONG flags, PVOID heapBase) = nullptr;

// https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapalloc
LPVOID(WINAPI* gHeapAllocOrig)(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) = nullptr;
// https://docs.microsoft.com/en-us/windows/win32/api/heapapi/nf-heapapi-heapfree
BOOL(WINAPI* gHeapFreeOrig)(HANDLE hHeap, DWORD dwFlags, _Frees_ptr_opt_ LPVOID lpMem) = nullptr;

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

static void* GetDllProcAddr(const char* dll, const char* name) {
    HMODULE mod = GetModuleHandleA(dll);
    if (mod == NULL)
        return nullptr;

    void* pTarget = (LPVOID)GetProcAddress(mod, name);
    return pTarget;
}

static void* AllocPrivate(size_t n) {
    // note: HeapAlloc and RtlHeapAlloc seem to be the same
    if (gRtlAllocateHeapOrig) {
        return gRtlAllocateHeapOrig(gHeap, 0, n);
    }
    if (gHeapAllocOrig) {
        return gHeapAllocOrig(gHeap, 0, n);
    }
    return HeapAlloc(gHeap, 0, n);
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
    bool ok = DynStackWalk64 && DynSymFunctionTableAccess64 && DynSymGetModuleBase64 && DynSymFromAddr;
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
    CallstackInfoBlock* b = gCallstackInfoBlockCurr;
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
__declspec(noinline) bool GetCurrentThreadCallstack(CallstackInfo* cs) {
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
    for (int i = 0; i < n - FRAMES_TO_SKIP; i++) {
        res->frame[i] = csi.callstack[i + FRAMES_TO_SKIP];
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
    BOOLEAN res = gRtlFreeHeapOrig(heapHandle, flags, heapBase);
    if (res && (gRecurCount == 1)) {
        gFrees++;
        RecordAllocOrFree(TYPE_FREE, heapHandle, heapBase, 0);
    }
    gRecurCount--;
    Unlock();
    return res;
}

LPVOID WINAPI HeapAllocHook(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes) {
    Lock();
    gRecurCount++;
    gAllocs++;
    PVOID res = gHeapAllocOrig(hHeap, dwFlags, dwBytes);
    if ((res != nullptr) && (gRecurCount == 1)) {
        RecordAllocOrFree(TYPE_ALLOC, hHeap, res, dwBytes);
    }
    gRecurCount--;
    Unlock();
    return res;
}

BOOL WINAPI HeapFreeHook(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem) {
    Lock();
    gRecurCount++;
    BOOLEAN res = gHeapFreeOrig(hHeap, dwFlags, lpMem);
    if (res && (gRecurCount == 1)) {
        gFrees++;
        RecordAllocOrFree(TYPE_FREE, hHeap, lpMem, 0);
    }
    gRecurCount--;
    Unlock();
    return res;
}

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

#if defined(DEBUG)
decltype(_malloc_dbg)* g_malloc_dbg_orig = nullptr;
decltype(_calloc_dbg)* g_calloc_dbg_orig = nullptr;
decltype(_free_dbg)* g_free_dbg_orig = nullptr;
decltype(_realloc_dbg)* g_realloc_dbg_orig = nullptr;
decltype(_recalloc_dbg)* g_recalloc_dbg_orig = nullptr;

void* __cdecl _malloc_dbg_hook(size_t const size, int const block_use, char const* const file_name,
                               int const line_number) {
    Lock();
    gRecurCount++;
    gAllocs++;

    void* res = g_malloc_dbg_orig(size, block_use, file_name, line_number);
    if ((res != nullptr) && (gRecurCount == 1)) {
        RecordAllocOrFree(TYPE_ALLOC, 0, res, size);
    }
    gRecurCount--;
    Unlock();
    return res;
}

void* __cdecl _calloc_dbg_hook(size_t const count, size_t const element_size, int const block_use,
                               char const* const file_name, int const line_number) {
    Lock();
    gRecurCount++;
    gAllocs++;

    void* res = g_calloc_dbg_orig(count, element_size, block_use, file_name, line_number);

    if ((res != nullptr) && (gRecurCount == 1)) {
        size_t size = count * element_size;
        RecordAllocOrFree(TYPE_ALLOC, 0, res, size);
    }
    gRecurCount--;
    Unlock();

    return res;
}

void __cdecl _free_dbg_hook(void* const block, int const block_use) {
    Lock();
    gRecurCount++;

    g_free_dbg_orig(block, block_use);
    if (gRecurCount == 1) {
        gFrees++;
        RecordAllocOrFree(TYPE_FREE, 0, block, 0);
    }
    gRecurCount--;
    Unlock();
}

void* __cdecl _realloc_dbg_hook(void* _Block, size_t _Size, int _BlockUse, char const* _FileName, int _LineNumber) {
    Lock();
    gRecurCount++;

    void* res = g_realloc_dbg_orig(_Block, _Size, _BlockUse, _FileName, _LineNumber);
    if (gRecurCount == 1) {
        RecordAllocOrFree(TYPE_FREE, 0, _Block, 0);
        if (res != nullptr) {
            RecordAllocOrFree(TYPE_ALLOC, 0, res, _Size);
        }
    }
    gRecurCount--;
    Unlock();
    return res;
}

void* __cdecl _recalloc_dbg_hook(void* _Block, size_t _Count, size_t _Size, int _BlockUse, char const* _FileName,
                                 int _LineNumber) {
    Lock();
    gRecurCount++;

    void* res = g_recalloc_dbg_orig(_Block, _Count, _Size, _BlockUse, _FileName, _LineNumber);
    if (gRecurCount == 1) {
        size_t size = _Count * _Size;
        RecordAllocOrFree(TYPE_FREE, 0, _Block, 0);
        if (res != nullptr) {
            RecordAllocOrFree(TYPE_ALLOC, 0, res, size);
        }
    }
    gRecurCount--;
    Unlock();
    return res;
}
#endif

decltype(_malloc_base)* g_malloc_base_orig = nullptr;
decltype(_calloc_base)* g_calloc_base_orig = nullptr;
decltype(_free_base)* g_free_base_orig = nullptr;
decltype(_realloc_base)* g_realloc_base_orig = nullptr;
decltype(_recalloc_base)* g_recalloc_base_orig = nullptr;

void* __cdecl _malloc_base_hook(size_t _Size) {
    Lock();
    gRecurCount++;
    gAllocs++;

    void* res = g_malloc_base_orig(_Size);
    if ((res != nullptr) && (gRecurCount == 1)) {
        RecordAllocOrFree(TYPE_ALLOC, 0, res, _Size);
    }
    gRecurCount--;
    Unlock();
    return res;
}

void* __cdecl _calloc_base_hook(size_t _Count, size_t _Size) {
    Lock();
    gRecurCount++;
    gAllocs++;

    void* res = g_calloc_base_orig(_Count, _Size);
    if ((res != nullptr) && (gRecurCount == 1)) {
        size_t size = _Count * _Size;
        RecordAllocOrFree(TYPE_ALLOC, 0, res, size);
    }
    gRecurCount--;
    Unlock();
    return res;
}

void __cdecl _free_base_hook(void* _Block) {
    Lock();
    gRecurCount++;

    g_free_base_orig(_Block);
    if (gRecurCount == 1) {
        gFrees++;
        RecordAllocOrFree(TYPE_FREE, 0, _Block, 0);
    }
    gRecurCount--;
    Unlock();
}

void* __cdecl _realloc_base_hook(void* _Block, size_t _Size) {
    Lock();
    gRecurCount++;

    void* res = g_realloc_base_orig(_Block, _Size);
    if (gRecurCount == 1) {
        RecordAllocOrFree(TYPE_FREE, 0, _Block, 0);
        if (res != nullptr) {
            RecordAllocOrFree(TYPE_ALLOC, 0, res, _Size);
        }
    }
    gRecurCount--;
    Unlock();
    return res;
}

void* __cdecl _recalloc_base_hook(void* _Block, size_t _Count, size_t _Size) {
    Lock();
    gRecurCount++;

    void* res = g_recalloc_base_orig(_Block, _Count, _Size);
    if (gRecurCount == 1) {
        RecordAllocOrFree(TYPE_FREE, 0, _Block, 0);
        if (res != nullptr) {
            size_t size = _Count * _Size;
            RecordAllocOrFree(TYPE_ALLOC, 0, res, size);
        }
    }
    gRecurCount--;
    Unlock();
    return res;
}

// TODO: optimize callstacks by de-duplicating them
//       (calc hash and bisect + linear search to find the callstack)

#define MAX_HOOKS 32
HOOK_ENTRY gHooks[MAX_HOOKS] = {0};
int gHooksCount = 0;

static void AddDllFunc(const char* dllName, const char* funcName, LPVOID func, LPVOID* ppOrig) {
    HOOK_ENTRY* pHook = &(gHooks[gHooksCount++]);
    pHook->pTarget = GetDllProcAddr(dllName, funcName);
    pHook->pDetour = func;
    pHook->ppOrig = ppOrig;
}

static void AddFunc(LPVOID pTarget, LPVOID func, LPVOID* ppOrig) {
    HOOK_ENTRY* pHook = &(gHooks[gHooksCount++]);
    pHook->pTarget = pTarget;
    pHook->pDetour = func;
    pHook->ppOrig = ppOrig;
}

bool MemLeakInit() {
    MH_STATUS status;
    CrashIf(gRtlAllocateHeapOrig != nullptr); // don't call me twice

    InitializeSymbols();
    // if we can't stack walk, it's pointless to do any work
    gCanStackWalk = CanStackWalk();
    if (!gCanStackWalk) {
        return false;
    }

    InitializeCriticalSection(&gMutex);
    gHeap = HeapCreate(0, 0, 0);
    MH_Initialize();

#if 0
    void* proc;
    proc = GetDllProcAddr("ntdll.dll", "RtlAllocateHeap");
    status = MH_CreateHook(proc, RtlAllocateHeapHook, (void**)&gRtlAllocateHeapOrig);
    if (status != MH_OK) {
        return false;
    }
    proc = GetDllProcAddr("ntdll.dll", "RtlFreeHeap");
    status = MH_CreateHook(proc, RtlFreeHeapHook, (void**)&gRtlFreeHeapOrig);
    if (status != MH_OK) {
        return false;
    }

    proc = GetDllProcAddr("kernel32.dll", "HeapAlloc";
    status = MH_CreateHook(proc, HeapAllocHook, (void**)&gHeapAllocOrig);
    if (status != MH_OK) {
        return false;
    }
    proc = GetDllProcAddr("kernel32.dll", "HeapFree");
    status = MH_CreateHook(proc, HeapFreeHook, (void**)&gHeapFreeOrig);
    if (status != MH_OK) {
        return false;
    }
#endif

#if defined(DEBUG)
    AddFunc(_malloc_dbg, _malloc_dbg_hook, (void**)&g_malloc_dbg_orig);
    AddFunc(_calloc_dbg, _calloc_dbg_hook, (void**)&g_calloc_dbg_orig);
    AddFunc(_free_dbg, _free_dbg_hook, (void**)&g_free_dbg_orig);
    AddFunc(_realloc_dbg, _realloc_dbg_hook, (void**)&g_realloc_dbg_orig);
    AddFunc(_recalloc_dbg, _recalloc_dbg_hook, (void**)&g_recalloc_dbg_orig);
#endif

    AddFunc(_malloc_base, _malloc_base_hook, (void**)&g_malloc_base_orig);
    AddFunc(_calloc_base, _calloc_base_hook, (void**)&g_calloc_base_orig);
    AddFunc(_free_base, _free_base_hook, (void**)&g_free_base_orig);
    AddFunc(_realloc_base, _realloc_base_hook, (void**)&g_realloc_base_orig);
    AddFunc(_recalloc_base, _recalloc_base_hook, (void**)&g_recalloc_base_orig);

    status = MH_CreateHooks(gHooks, gHooksCount);
    CrashIf(status != MH_OK);
    status = MH_EnableOrDisableHooks(gHooks, gHooksCount, TRUE);
    CrashIf(status != MH_OK);
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
    if (nDumped > 32) {
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
        dbghelp::GetAddressInfo(s, addr, true);
        dbglogf("  %s", s.Get());
    }
}

void DumpMemLeaks() {
    if (!gCanStackWalk) {
        return;
    }
    MH_Uninitialize(gHooks, gHooksCount);
    gEnableDbgLog = true;

    int nAllocs = gAllocs;
    int nFrees = gFrees;

    AllocFreeEntryBlock* b = gAllocFreeBlockFirst;
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
    dbglogf("allocs: %d, frees: %d\n", nAllocs, nFrees);
    dbglogf("%d unfreed\n", nUnfreed);
    HeapDestroy(gHeap);
}
