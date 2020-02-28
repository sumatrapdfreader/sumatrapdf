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
    char* dst = (char*)Allocator::Alloc(a, n + 1);
    const char* src = str.data();
    memcpy(dst, (const void*)src, n);
    dst[n] = 0; // we don't assume str.data() is 0-terminated
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

PoolAllocatorFixed::~PoolAllocatorFixed() {
    auto curr = firstBlock;
    while (curr) {
        auto next = curr->next;
        free(curr);
        curr = next;
    }
}

void* PoolAllocatorFixed::Realloc(void* mem, size_t size) {
    // no-op
    return nullptr;
}

void PoolAllocatorFixed::Free(const void*) {
    // no-op
}

void* PoolAllocatorFixed::Alloc(size_t size) {
    // it's ok if size is smaller than element size
    // because of padding, but shouldn't be bigger than 16
    int diff = elementSize - (int)size;
    CrashIf(diff < 0);
    CrashIf(diff > 16);

    // allocate a block if needed
    if (currBlock == nullptr || currBlock->nUsed == elementsPerBlock) {
        // TODO: maybe round up to 16
        size_t sz = sizeof(PoolAllocatorFixed::Block) + (elementSize * elementsPerBlock);
        auto block = (PoolAllocatorFixed::Block*)malloc(sz);
        block->nUsed = 0;
        if (!firstBlock) {
            firstBlock = block;
        } else {
            currBlock->next = block;
        }
        block->next = nullptr;
        if (currBlock) {
            currBlock->next = block;
        }
        currBlock = block;
    }

    char* d = (char*)currBlock;
    // TODO: maybe round up sizeof() to 16 bytes
    d += sizeof(PoolAllocatorFixed::Block);
    d += currBlock->nUsed * elementSize;
    currBlock->nUsed += 1;
    return (void*)d;
}

void* PoolAllocatorFixed::At(int i) {
    auto curr = firstBlock;
    while (curr && i > curr->nUsed) {
        i -= curr->nUsed;
        curr = curr->next;
    }
    char* d = (char*)curr;
    // TODO: maybe round up sizeof() to 16 bytes
    d += sizeof(PoolAllocatorFixed::Block);
    d += i * elementSize;
    return (void*)d;
}

int PoolAllocatorFixed::Count() {
    int n = 0;
    auto curr = firstBlock;
    while (curr) {
        n += curr->nUsed;
        curr = curr->next;
    }
    return n;
}

size_t PoolAllocator::MemBlockNode::Used() {
    return size - free;
}

char* PoolAllocator::MemBlockNode::DataStart() {
    return (char*)this + sizeof(MemBlockNode);
}

// adjust DataStart() to be aligned at alignTo boundary
// alignTo should be a power of 2
void PoolAllocator::MemBlockNode::AlignTo(size_t alignTo) {
    if (alignTo < 2) {
        // 0 and 1 mean: no alignment
        return;
    }
    size_t n = Used();
    size_t off = n % alignTo;
    if (off == 0) {
        // already aligned
        return;
    }
    size_t pad = alignTo - off;
    if (pad >= free) {
        free = 0;
    } else {
        free -= pad;
    }
    CrashIf((size_t)DataStart() % alignTo != 0);
}

void PoolAllocator::SetMinBlockSize(size_t newMinBlockSize) {
    minBlockSize = newMinBlockSize;
}

void PoolAllocator::Free(const void*) {
    // does nothing, we can't free individual pieces of memory
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

// optimization: frees all but first block
// allows for more efficient re-use of PoolAllocator
// with more effort we could preserve all blocks (not sure if worth it)
void PoolAllocator::reset() {
    if (!firstBlock) {
        return;
    }
    MemBlockNode* first = firstBlock;
    firstBlock = first->next;
    FreeAll();
    firstBlock = first;
    first->next = nullptr;
    first->free = first->size;
}

PoolAllocator::~PoolAllocator() {
    FreeAll();
}

void PoolAllocator::AllocBlock(size_t minSize) {
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
    size_t minSize = RoundUp(size, currAlign);
    if (!currBlock || (currBlock->free < minSize)) {
        AllocBlock(minSize);
    }
    currBlock->AlignTo(currAlign);

    void* mem = (void*)(currBlock->DataStart() + currBlock->Used());
    currBlock->free -= size;
    return mem;
}

// assuming allocated memory was for pieces of uniform size,
// find the address of n-th piece
void* PoolAllocator::FindNthPieceOfSize(size_t size, size_t n) const {
    size = RoundUp(size, currAlign);
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

int VecStrIndex::nLeft() {
    return kVecStrIndexSize - nStrings;
}

int VecStr::size() {
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

bool VecStr::allocateIndexIfNeeded() {
    if (currIndex && currIndex->nLeft() > 0) {
        return true;
    }

    // for structures we want aligned allocation. 8 should be good enough for everything
    allocator.currAlign = 8;
    VecStrIndex* idx = allocator.AllocStruct<VecStrIndex>();

    if (allowFailure && !idx) {
        return false;
    }
    idx->next = nullptr;
    idx->nStrings = 0;

    if (!firstIndex) {
        firstIndex = idx;
        currIndex = idx;
    } else {
        CrashIf(!firstIndex);
        CrashIf(!currIndex);
        currIndex->next = idx;
        currIndex = idx;
    }
    return true;
}

bool VecStr::Append(std::string_view sv) {
    bool ok = allocateIndexIfNeeded();
    if (!ok) {
        return false;
    }
    constexpr size_t maxLen = (size_t)std::numeric_limits<i32>::max();
    CrashIf(sv.size() > maxLen);
    if (sv.size() > maxLen) {
        return false;
    }
    allocator.currAlign = 1; // no need to align allocations for string
    std::string_view res = Allocator::AllocString(&allocator, sv);

    int n = currIndex->nStrings;
    currIndex->offsets[n] = (char*)res.data();
    currIndex->sizes[n] = (i32)res.size();
    currIndex->nStrings++;
    return true;
}

void VecStr::reset() {
    allocator.reset();
    firstIndex = nullptr;
    currIndex = nullptr;
}
