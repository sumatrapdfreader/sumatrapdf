/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

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

void* Allocator::MemDup(Allocator* a, const void* mem, size_t size, size_t padding) {
    void* newMem = Alloc(a, size + padding);
    if (newMem) {
        memcpy(newMem, mem, size);
    }
    return newMem;
}

char* Allocator::StrDup(Allocator* a, const char* s) {
    if (!s) {
        return nullptr;
    }
    size_t n = str::Len(s);
    return (char*)Allocator::MemDup(a, s, n + 1);
}

// allocates a copy of the source string inside the allocator.
// it's only safe in PoolAllocator because allocated data
// never moves in memory
std::string_view Allocator::AllocString(Allocator* a, std::string_view sv) {
    size_t n = sv.size();
    char* dst = (char*)Allocator::Alloc(a, n + 1);
    const char* src = sv.data();
    memcpy(dst, (const void*)src, n);
    dst[n] = 0; // we don't assume sv.data() is 0-terminated
    return std::string_view(dst, n);
}

#if OS_WIN
WCHAR* Allocator::StrDup(Allocator* a, const WCHAR* s) {
    if (!s) {
        return nullptr;
    }
    size_t n = (str::Len(s) + 1) * sizeof(WCHAR);
    return (WCHAR*)Allocator::MemDup(a, s, n);
}
#endif

void PoolAllocator::Free(const void*) {
    // does nothing, we can't free individual pieces of memory
}

void PoolAllocator::FreeAll() {
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

// optimization: frees all but first block
// allows for more efficient re-use of PoolAllocator
// with more effort we could preserve all blocks (not sure if worth it)
void PoolAllocator::Reset() {
    FreeAll();

    // TODO: optimize by not freeing the first block, to speed up re-use
#if 0
    if (!firstBlock) {
        return;
    }
    Block* first = firstBlock;
    firstBlock = first->next;
    FreeAll();

    firstBlock = first;
    first->next = nullptr;
    first->nAllocs = 0;
    // TODO: properly reset first
#endif
}

PoolAllocator::~PoolAllocator() {
    FreeAll();
}

// Allocator methods
void* PoolAllocator::Realloc(void*, size_t) {
    // TODO: we can't do that because we don't know the original
    // size of memory piece pointed by mem. We could remember it
    // within the block that we allocate
    CrashMe();
    return nullptr;
}

static bool BlockHasSpaceFor(PoolAllocator::Block* block, int size, int allocAlign) {
    if (!block) {
        return false;
    }
    char* d = RoundUp(block->curr, allocAlign);
    // data + i32 index of allocation
    d += (size + sizeof(i32));
    if (d > block->end) {
        return false;
    }
    return true;
}

void* PoolAllocator::Alloc(size_t size) {
    if (!BlockHasSpaceFor(currBlock, (int)size, allocAlign)) {
        int freeSpaceSize = (int)size + sizeof(i32); // + space for index
        int hdrSize = RoundUp((int)sizeof(PoolAllocator::Block), allocAlign);
        int blockSize = hdrSize + freeSpaceSize;
        if (blockSize < minBlockSize) {
            blockSize = minBlockSize;
            freeSpaceSize = minBlockSize - hdrSize;
        }
        // TODO: zero with calloc()? slower but safer
        auto block = (Block*)malloc(blockSize);
        char* start = (char*)block;

        block->nAllocs = 0;
        block->curr = start + hdrSize;
        block->dataSize = freeSpaceSize;
        block->end = start + block->dataSize;
        block->next = nullptr;
        if (!firstBlock) {
            firstBlock = block;
        }
        if (currBlock) {
            currBlock->next = block;
        }
        currBlock = block;
    }
    char* res = RoundUp(currBlock->curr, allocAlign);
    currBlock->curr = res + size;

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
    CrashIf(i < 0 || i >= nAllocs);
    if (i < 0 || i >= nAllocs) {
        return nullptr;
    }
    auto curr = firstBlock;
    while (curr && i >= curr->nAllocs) {
        i -= curr->nAllocs;
        curr = curr->next;
    }
    CrashIf(!curr);
    if (!curr) {
        return nullptr;
    }
    CrashIf(i >= curr->nAllocs);
    i32* index = (i32*)curr->end;
    // elements are in reverse
    int idx = curr->nAllocs - i - 1;
    i32 offset = index[idx];
    char* d = (char*)curr + offset;
    return (void*)d;
}

#if !OS_WIN
void ZeroMemory(void* p, size_t len) {
    memset(p, 0, len);
}
#endif

// This exits so that I can add temporary instrumentation
// to catch allocations of a given size and it won't cause
// re-compilation of everything caused by changing BaseUtil.h
void* AllocZero(size_t count, size_t size) {
    return calloc(count, size);
}

void* memdup(const void* data, size_t len) {
    void* dup = malloc(len);
    if (!dup) {
        return nullptr;
    }
    memcpy(dup, data, len);
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

    // for structures we want aligned allocation. 8 should be good enough for everything
    v.allocator.allocAlign = 8;
    VecStrIndex* idx = v.allocator.AllocStruct<VecStrIndex>();

    if (!idx && (gAllowAllocFailure.load() > 0)) {
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
    allocator.allocAlign = 1; // no need to align allocations for string
    std::string_view res = Allocator::AllocString(&allocator, sv);

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
