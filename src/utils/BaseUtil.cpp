/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"

Kind kindNone = "none";

// if > 1 we won't crash when memory allocation fails
std::atomic<int> gAllowAllocFailure = 0;

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
    } else {
        a->Free(p);
    }
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
#if IS_32BIT
constexpr size_t kPoolAllocatorAlign = 8;
#else
constexpr size_t kPoolAllocatorAlign = 16;
#endif

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
            CrashIf(d > curr->freeSpace);
            size_t n = (curr->freeSpace - d);
            memset(d, 0xdd, n);
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
    CrashIf(RoundUp(block->freeSpace, kPoolAllocatorAlign) != block->freeSpace);
}

void PoolAllocator::Reset(bool poisonFreedMemory) {
    ScopedCritSec scs(&cs);
    // free all but first block to
    // allows for more efficient re-use of PoolAllocator
    // with more effort we could preserve all blocks (not sure if worth it)
    Block* first = firstBlock;
    if (!first) {
        return;
    }
    if (poisonFreedMemory) {
        PoisonData(firstBlock);
    }
    firstBlock = firstBlock->next;
    FreeAll();
    ResetBlock(first);
    firstBlock = first;
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

// we allocate the value at the beginning of current block
// and we store a pointer to the value at the end of current block
// that way we can find allocations
void* PoolAllocator::Alloc(size_t size) {
    ScopedCritSec scs(&cs);

    // need rounded size + space for index at the end
    bool hasSpace = false;
    size_t sizeRounded = RoundUp(size, kPoolAllocatorAlign);
    size_t cbNeeded = sizeRounded + sizeof(i32);
    if (currBlock) {
        CrashIf(currBlock->freeSpace > currBlock->end);
        size_t cbAvail = (currBlock->end - currBlock->freeSpace);
        hasSpace = cbAvail >= cbNeeded;
    }

    if (!hasSpace) {
        size_t hdrSize = BlockHeaderSize();
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
            firstBlock = block;
        }
        if (currBlock) {
            currBlock->next = block;
        }
        currBlock = block;
    }
    char* res = currBlock->freeSpace;
    currBlock->freeSpace = res + sizeRounded;
    // TODO: figure out why this hits
    CrashIf(RoundUp(currBlock->freeSpace, kPoolAllocatorAlign) != currBlock->freeSpace);

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

    CrashIf(i < 0 || i >= nAllocs);
    if (i < 0 || i >= nAllocs) {
        return nullptr;
    }
    auto curr = firstBlock;
    while (curr && (size_t)i >= curr->nAllocs) {
        i -= (int)curr->nAllocs;
        curr = curr->next;
    }
    CrashIf(!curr);
    if (!curr) {
        return nullptr;
    }
    CrashIf((size_t)i >= curr->nAllocs);
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

constexpr size_t RoundUp(size_t n, size_t rounding) {
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

int VecStrIndex::ItemsLeft() {
    return kVecStrIndexSize - nStrings;
}

int VecStr::Size() {
    VecStrIndex* idx = firstIndex;
    int n = 0;
    while (idx) {
        n += idx->nStrings;
        idx = idx->next;
    }
    return n;
}

std::string_view VecStr::at(int i) {
    CrashIf(i < 0);
    VecStrIndex* idx = firstIndex;
    while (idx) {
        if (idx->nStrings > i) {
            break;
        }
        i -= idx->nStrings;
        idx = idx->next;
    }
    if (idx == nullptr) {
        CrashMe();
        return {};
    }
    CrashIf(idx->nStrings <= i);
    if (idx->nStrings <= i) {
        return {};
    }
    const char* s = (const char*)idx->offsets[i];
    i32 size = idx->sizes[i];
    return {s, (size_t)size};
}

static bool allocateIndexIfNeeded(VecStr& v) {
    if (v.currIndex && v.currIndex->ItemsLeft() > 0) {
        return true;
    }

    VecStrIndex* idx = v.allocator.AllocStruct<VecStrIndex>();

    if (!idx) {
        CrashAlwaysIf(gAllowAllocFailure.load() == 0);
        return false;
    }

    idx->next = nullptr;
    idx->nStrings = 0;

    if (!v.firstIndex) {
        v.firstIndex = idx;
        v.currIndex = idx;
    } else {
        CrashIf(!v.firstIndex);
        CrashIf(!v.currIndex);
        v.currIndex->next = idx;
        v.currIndex = idx;
    }
    return true;
}

bool VecStr::Append(std::string_view sv) {
    bool ok = allocateIndexIfNeeded(*this);
    if (!ok) {
        return false;
    }
    constexpr size_t maxLen = (size_t)std::numeric_limits<i32>::max();
    CrashIf(sv.size() > maxLen);
    if (sv.size() > maxLen) {
        return false;
    }
    std::string_view res = str::Dup(&allocator, sv);

    int n = currIndex->nStrings;
    currIndex->offsets[n] = (char*)res.data();
    currIndex->sizes[n] = (i32)res.size();
    currIndex->nStrings++;
    return true;
}

void VecStr::Reset() {
    allocator.Reset();
    firstIndex = nullptr;
    currIndex = nullptr;
}
