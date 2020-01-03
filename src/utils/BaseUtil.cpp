/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"

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
std::string_view Allocator::AllocString(Allocator* a, std::string_view str) {
    size_t n = str.size();
    size_t toCopy = n + 1;
    const char* src = str.data();
    if (!src) {
        src = "";
    }
    void* dst = Allocator::Alloc(a, toCopy);
    memcpy(dst, (const void*)src, toCopy);
    return std::string_view((const char*)dst, n);
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

void PoolAllocator::SetMinBlockSize(size_t newMinBlockSize) {
    CrashIf(currBlock); // can only be changed before first allocation
    minBlockSize = newMinBlockSize;
}

void PoolAllocator::SetAllocRounding(size_t newRounding) {
    CrashIf(currBlock); // can only be changed before first allocation
    allocRounding = newRounding;
}

void PoolAllocator::FreeAll() {
    MemBlockNode* curr = firstBlock;
    while (curr) {
        MemBlockNode* next = curr->next;
        free(curr);
        curr = next;
    }
    currBlock = nullptr;
    firstBlock = nullptr;
}

PoolAllocator::~PoolAllocator() {
    FreeAll();
}

void PoolAllocator::AllocBlock(size_t minSize) {
    minSize = RoundUp(minSize, allocRounding);
    size_t size = minBlockSize;
    if (minSize > size) {
        size = minSize;
    }
    MemBlockNode* node = (MemBlockNode*)calloc(1, sizeof(MemBlockNode) + size);
    CrashAlwaysIf(!node);
    if (!firstBlock) {
        firstBlock = node;
    }
    node->size = size;
    node->free = size;
    if (currBlock) {
        currBlock->next = node;
    }
    currBlock = node;
}

// Allocator methods
void* PoolAllocator::Realloc(void* mem, size_t size) {
    UNUSED(mem);
    UNUSED(size);
    // TODO: we can't do that because we don't know the original
    // size of memory piece pointed by mem. We could remember it
    // within the block that we allocate
    CrashMe();
    return nullptr;
}

void* PoolAllocator::Alloc(size_t size) {
    size = RoundUp(size, allocRounding);
    if (!currBlock || (currBlock->free < size)) {
        AllocBlock(size);
    }

    void* mem = (void*)(currBlock->DataStart() + currBlock->Used());
    currBlock->free -= size;
    return mem;
}

// assuming allocated memory was for pieces of uniform size,
// find the address of n-th piece
void* PoolAllocator::FindNthPieceOfSize(size_t size, size_t n) const {
    size = RoundUp(size, allocRounding);
    MemBlockNode* curr = firstBlock;
    while (curr) {
        size_t piecesInBlock = curr->Used() / size;
        if (piecesInBlock > n) {
            char* p = (char*)curr + sizeof(MemBlockNode) + (n * size);
            return (void*)p;
        }
        n -= piecesInBlock;
        curr = curr->next;
    }
    return nullptr;
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
    return ((n + rounding - 1) / rounding) * rounding;
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
static uint32_t hash_function_seed = 5381;

uint32_t MurmurHash2(const void* key, size_t len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = hash_function_seed ^ (uint32_t)len;

    /* Mix 4 bytes at a time into the hash */
    const uint8_t* data = (const uint8_t*)key;

    while (len >= 4) {
        uint32_t k = *(uint32_t*)data;

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
