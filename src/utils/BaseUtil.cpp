/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

#include "utils/Log.h"

Kind kindNone = "none";

// if > 1 we won't crash when memory allocation fails
LONG gAllowAllocFailure = 0;

// returns previous value
int AtomicInt::Set(int n) {
    auto res = InterlockedExchange((LONG*)&val, n);
    return (int)res;
}

// returns value after increment
int AtomicInt::Inc() {
    return (int)InterlockedIncrement(&val);
}

// returns value after decrement
int AtomicInt::Dec() {
    return (int)InterlockedDecrement(&val);
}

// returns value after adding
int AtomicInt::Add(int n) {
    return (int)InterlockedAdd(&val, n);
}

// returns value after subtracting
int AtomicInt::Sub(int n) {
    return (int)InterlockedAdd(&val, -n);
}

int AtomicInt::Get() const {
    return (int)InterlockedCompareExchange((LONG*)&val, 0, 0);
}

bool AtomicBool::Get() const {
    return InterlockedCompareExchange((LONG*)&val, 0, 0) != 0;
}

// returns previous value
bool AtomicBool::Set(bool newValue) {
    auto res = InterlockedExchange((LONG*)&val, newValue ? 1 : 0);
    return res != 0;
}

// returns count after adding
int AtomicRefCount::Add() {
    return (int)InterlockedIncrement(&val);
}

int AtomicRefCount::Dec() {
    auto res = InterlockedDecrement(&val);
    return res;
}

void* Allocator::Alloc(Allocator* a, size_t size) {
    if (!a) {
        return malloc(size);
    }
    return a->Alloc(size);
}

void* Allocator::AllocZero(Allocator* a, size_t size) {
    void* m = Allocator::Alloc(a, size);
    if (m) {
        ZeroMemory(m, size);
    }
    return m;
}

void Allocator::Free(Allocator* a, void* p) {
    if (!p) {
        return;
    }
    if (!a) {
        free(p);
        return;
    }
    a->Free(p);
}

void* Allocator::Realloc(Allocator* a, void* mem, size_t size) {
    if (!a) {
        return realloc(mem, size);
    }
    return a->Realloc(mem, size);
}

// extraBytes will be zero, useful e.g. for creating zero-terminated strings
// by using extraBytes = sizeof(CHAR)
void* Allocator::MemDup(Allocator* a, const void* mem, size_t size, size_t extraBytes) {
    if (!mem) {
        return nullptr;
    }
    void* newMem = AllocZero(a, size + extraBytes);
    if (newMem) {
        memcpy(newMem, mem, size);
    }
    return newMem;
}

// -------------------------------------

// using the same alignment as windows, to be safe
// TODO: could use the same alignment everywhere but would have to
// align start of the Block, couldn't just start at malloc() address
constexpr size_t kPoolAllocatorAlign = sizeof(char*) * 2;

PoolAllocator::PoolAllocator() {
    InitializeCriticalSection(&cs);
}

void PoolAllocator::Free(const void*) {
    // does nothing, we can't free individual pieces of memory
}

void PoolAllocator::FreeAll() {
    ScopedCritSec scs(&cs);
    Block* curr = firstBlock;
    while (curr) {
        Block* next = curr->next;
        free(curr);
        curr = next;
    }
    currBlock = nullptr;
    firstBlock = nullptr;
    nAllocs = 0;
}

static size_t BlockHeaderSize() {
    return RoundUp(sizeof(PoolAllocator::Block), kPoolAllocatorAlign);
}

// for easier debugging, poison the freed data with 0xdd
// that way if the code tries to used the freed memory,
// it's more likely to crash
static void PoisonData(PoolAllocator::Block* curr) {
    char* d;
    size_t hdrSize = BlockHeaderSize();
    while (curr) {
        // optimization: don't touch memory if there were not allocations
        if (curr->nAllocs > 0) {
            d = (char*)curr + hdrSize;
            // the buffer is big so optimize to only poison the data
            // allocated in this block
            ReportIf(d > curr->freeSpace);
            size_t n = (curr->freeSpace - d);
            const char* dead = "dea_";
            const char* dea0 = "dea\0";
            u32 u32dead = *((u32*)dead);
            u32 u32dea0 = *((u32*)dea0);
            n = n / sizeof(u32);
            u32* w = (u32*)d;
            // fill with "dead", and every 4-th is 0-terminated
            // so that if it's shown in the debugger as a string
            // it shows as "dea_dea_dea_dea"
            for (size_t i = 0; i < n; i++) {
                if ((i & 0x3) == 0x3) {
                    *w++ = u32dea0;
                } else {
                    *w++ = u32dead;
                }
            }
        }
        curr = curr->next;
    }
}

static void ResetBlock(PoolAllocator::Block* block) {
    size_t hdrSize = BlockHeaderSize();
    char* start = (char*)block;
    block->nAllocs = 0;
    block->freeSpace = start + hdrSize;
    block->end = start + block->dataSize;
    block->next = nullptr;
    ReportIf(RoundUp(block->freeSpace, kPoolAllocatorAlign) != block->freeSpace);
}

void PoolAllocator::Reset(bool poisonFreedMemory) {
    ScopedCritSec scs(&cs);
    // free all but first block to
    // allows for more efficient re-use of PoolAllocator
    // with more effort we could preserve all blocks (not sure if worth it)
    Block* first = firstBlock;
    if (!first) {
        ReportIf(currBlock);
        return;
    }
    if (poisonFreedMemory) {
        PoisonData(firstBlock);
    }
    firstBlock = firstBlock->next;
    first->next = nullptr;
    FreeAll();
    ResetBlock(first);
    firstBlock = first;
    currBlock = first;
}

PoolAllocator::~PoolAllocator() {
    FreeAll();
    DeleteCriticalSection(&cs);
}

void* PoolAllocator::Realloc(void*, size_t) {
    // TODO: we can't do that because we don't know the original
    // size of memory piece pointed by mem. We could remember it
    // within the block that we allocate
    CrashMe();
    return nullptr;
}

static void printSize(const char* s, size_t size) {
    char buf[512]{};
    str::BufFmt(buf, dimof(buf), "%s%d\n", s, (int)size);
    OutputDebugStringA(buf);
}

// we allocate the value at the beginning of current block
// and we store a pointer to the value at the end of current block
// that way we can find allocations
void* PoolAllocator::Alloc(size_t size) {
    ScopedCritSec scs(&cs);
    // printSize("PoolAllocator: ", size);

    // need rounded size + space for index at the end
    size_t hdrSize = BlockHeaderSize();
    bool hasSpace = false;
    size_t sizeRounded = RoundUp(size, kPoolAllocatorAlign);
    size_t cbNeeded = sizeRounded + sizeof(i32);
    if (currBlock) {
        ReportIf(currBlock->freeSpace > currBlock->end);
        size_t cbAvail = (currBlock->end - currBlock->freeSpace);
        hasSpace = cbAvail >= cbNeeded;
    }

    if (!hasSpace) {
        cbNeeded += hdrSize;
        size_t dataSize = cbNeeded;
        size_t allocSize = hdrSize + cbNeeded;
        if (allocSize < minBlockSize) {
            allocSize = minBlockSize;
            dataSize = minBlockSize - hdrSize;
        }
        auto block = (Block*)AllocZero(nullptr, allocSize);
        if (!block) {
            return nullptr;
        }
        block->dataSize = dataSize;
        ResetBlock(block);
        if (!firstBlock) {
            ReportIf(currBlock);
            firstBlock = block;
        } else {
            currBlock->next = block;
        }
        currBlock = block;
    }
    char* res = currBlock->freeSpace;
    currBlock->freeSpace = res + sizeRounded;
    if (currBlock->freeSpace > currBlock->end) {
        size_t cbOvershot = currBlock->freeSpace - currBlock->end;
        printSize("PoolAllocator: ", size);
        printSize("overshot: ", cbOvershot);
        printSize("hdrSizet: ", hdrSize);
        ReportIf(true);
    }
    ReportIf(RoundUp(currBlock->freeSpace, kPoolAllocatorAlign) != currBlock->freeSpace);

    char* blockStart = (char*)currBlock;
    i32 offset = (i32)(res - blockStart);
    i32* index = (i32*)currBlock->end;
    index -= 1;
    index[0] = offset;
    currBlock->end = (char*)index;
    currBlock->nAllocs += 1;
    nAllocs += 1;
    return res;
}

void* PoolAllocator::At(int i) {
    ScopedCritSec scs(&cs);

    ReportIf(i < 0 || i >= nAllocs);
    if (i < 0 || i >= nAllocs) {
        return nullptr;
    }
    auto curr = firstBlock;
    while (curr && (size_t)i >= curr->nAllocs) {
        i -= (int)curr->nAllocs;
        curr = curr->next;
    }
    ReportIf(!curr);
    if (!curr) {
        return nullptr;
    }
    ReportIf((size_t)i >= curr->nAllocs);
    i32* index = (i32*)curr->end;
    // elements are in reverse
    size_t idx = curr->nAllocs - i - 1;
    i32 offset = index[idx];
    char* d = (char*)curr + offset;
    return (void*)d;
}

// This exits so that I can add temporary instrumentation
// to catch allocations of a given size and it won't cause
// re-compilation of everything caused by changing BaseUtil.h
void* AllocZero(size_t count, size_t size) {
    return calloc(count, size);
}

// extraBytes will be filled with 0. Useful for copying zero-terminated strings
void* memdup(const void* data, size_t len, size_t extraBytes) {
    // to simplify callers, if data is nullptr, ignore the sizes
    if (!data) {
        return nullptr;
    }
    void* dup = AllocZero(len + extraBytes, 1);
    if (dup) {
        memcpy(dup, data, len);
    }
    return dup;
}

bool memeq(const void* s1, const void* s2, size_t len) {
    return 0 == memcmp(s1, s2, len);
}

size_t RoundUp(size_t n, size_t rounding) {
    return ((n + rounding - 1) / rounding) * rounding;
}

int RoundUp(int n, int rounding) {
    if (rounding <= 1) {
        return n;
    }
    return ((n + rounding - 1) / rounding) * rounding;
}

char* RoundUp(char* d, int rounding) {
    if (rounding <= 1) {
        return d;
    }
    uintptr_t n = (uintptr_t)d;
    n = ((n + rounding - 1) / rounding) * rounding;
    return (char*)n;
}

size_t RoundToPowerOf2(size_t size) {
    size_t n = 1;
    while (n < size) {
        n *= 2;
        if (0 == n) {
            // TODO: no power of 2
            return (size_t)-1;
        }
    }
    return n;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static u32 hash_function_seed = 5381;

u32 MurmurHash2(const void* key, size_t len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const u32 m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    u32 h = hash_function_seed ^ (u32)len;

    /* Mix 4 bytes at a time into the hash */
    const u8* data = (const u8*)key;

    while (len >= 4) {
        u32 k = *(u32*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch (len) {
        case 3:
            h ^= data[2] << 16;
        case 2:
            h ^= data[1] << 8;
        case 1:
            h ^= data[0];
            h *= m;
    }

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// variation of MurmurHash2 which deals with strings that are
// mostly ASCII and should be treated case independently
u32 MurmurHashWStrI(const WCHAR* str) {
    size_t len = str::Len(str);
    auto a = GetTempAllocator();
    u8* data = (u8*)a->Alloc(len);
    WCHAR c;
    u8* dst = data;
    while (true) {
        c = *str++;
        if (!c) {
            break;
        }
        if (c & 0xFF80) {
            *dst++ = 0x80;
            continue;
        }
        if ('A' <= c && c <= 'Z') {
            *dst++ = (u8)(c + 'a' - 'A');
            continue;
        }
        *dst++ = (u8)c;
    }
    return MurmurHash2(data, len);
}

// variation of MurmurHash2 which deals with strings that are
// mostly ASCII and should be treated case independently
u32 MurmurHashStrI(const char* s) {
    char* dst = str::DupTemp(s);
    char c;
    size_t len = 0;
    while (*s) {
        c = *s++;
        len++;
        if ('A' <= c && c <= 'Z') {
            c = (c + 'a' - 'A');
        }
        *dst++ = c;
    }
    return MurmurHash2(dst - len, len);
}

int limitValue(int val, int min, int max) {
    if (min > max) {
        std::swap(min, max);
    }
    ReportIf(min > max);
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

DWORD limitValue(DWORD val, DWORD min, DWORD max) {
    if (min > max) {
        std::swap(min, max);
    }
    ReportIf(min > max);
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

float limitValue(float val, float min, float max) {
    if (min > max) {
        std::swap(min, max);
    }
    ReportIf(min > max);
    if (val < min) {
        return min;
    }
    if (val > max) {
        return max;
    }
    return val;
}

Func0 MkFunc0Void(funcVoidPtr fn) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = kFuncNoArg;
    return res;
}

#if 0
template <typename T>
Func0 MkMethod0Void(funcVoidPtr fn, T* self) {
    UINT_PTR fnTagged = (UINT_PTR)fn;
    res.fn = (void*)fn;
    res.userData = kFuncNoArg;
    res.self = self;
}
#endif
