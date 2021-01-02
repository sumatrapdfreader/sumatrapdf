/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/LogDbg.h"
#include "utils/MinHook.h"

// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlallocateheap
PVOID (WINAPI *gRtlAllocateHeapOrig)(PVOID heapHandle, ULONG flags, SIZE_T size);
// https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlfreeheap
BOOLEAN (WINAPI *gRtlFreeHeapOrig)(PVOID heapHandle, ULONG flags, PVOID heapBase);

#define TYPE_ALLOC 0
#define TYPE_FREE 1

struct AllocFreeEntry {
    void* heap;
    void* addr;
    u64 size;
    u32 typ;
};

#define ENTRIES_PER_BLOCK 1024

struct AllocFreeEntryBlock {
    AllocFreeEntryBlock* next;
    int nUsed;
    AllocFreeEntry entries[ENTRIES_PER_BLOCK];
};

static CRITICAL_SECTION gMutex;
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
        gAllocFreeBlockCurr = b;
    } else {
        gAllocFreeBlockCurr->next = b;
        gAllocFreeBlockCurr = b;
    }
    return &b->entries[0];
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
}

static void Lock() {
    EnterCriticalSection(&gMutex);
}

static void Unlock() {
    LeaveCriticalSection(&gMutex);
}

PVOID WINAPI RtlAllocateHeapHook(PVOID heapHandle, ULONG flags, SIZE_T size)
{
    Lock();
    gAllocs++;
    PVOID res = gRtlAllocateHeapOrig(heapHandle, flags, size);
    if (res != nullptr) {
        RecordAllocOrFree(TYPE_ALLOC, heapHandle, res, size);
    }
    Unlock();
    return res;
}

BOOLEAN WINAPI RtlFreeHeapHook(PVOID heapHandle, ULONG flags, PVOID heapBase) {
    Lock();
    gFrees++;
    BOOLEAN res = gRtlFreeHeapOrig(heapHandle, flags, heapBase);
    if (res) {
        RecordAllocOrFree(TYPE_FREE, heapHandle, heapBase, 0);
    }
    Unlock();
    return res;
}

bool MemLeakInit() {
    MH_STATUS status;
    CrashIf(gRtlAllocateHeapOrig != nullptr); // don't call me twice

    InitializeCriticalSection(&gMutex);
    gHeap = HeapCreate(0, 0, 0);
    MH_Initialize();

    status = MH_CreateHookApiEx(L"ntdll.dll", "RtlAllocateHeap", RtlAllocateHeapHook, (void**) &gRtlAllocateHeapOrig, nullptr);
    if (status != MH_OK) {
        return false;
    }
    status = MH_CreateHookApiEx(L"ntdll.dll", "RtlFreeHeap", RtlFreeHeapHook, (void**)&gRtlFreeHeapOrig,
                                nullptr);
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
