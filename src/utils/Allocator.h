/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Allocator_h
#define Allocator_h

#include "BaseUtil.h"

// Base class for allocators that can be provided to Vec class
// (and potentially others). Needed because e.g. in crash handler
// we want to use Vec but not use standard malloc()/free() functions
class Allocator {
public:
    Allocator() {}
    virtual ~Allocator() { };
    virtual void *Alloc(size_t size) = 0;
    virtual void *Realloc(void *mem, size_t size) = 0;
    virtual void Free(void *mem) = 0;

    // helper functions that fallback to malloc()/free() if allocator is NULL
    // helps write clients where allocator is optional
    static void *Alloc(Allocator *a, size_t size) {
        if (!a)
            return malloc(size);
        return a->Alloc(size);
    }

    static void Free(Allocator *a, void *p) {
        if (!a)
            free(p);
        else
            a->Free(p);
    }

    static void* Realloc(Allocator *a, void *mem, size_t size) {
        if (!a)
            return realloc(mem, size);
        return a->Realloc(mem, size);
    }

    static void *Dup(Allocator *a, void *mem, size_t size, size_t padding=0) {
        void *newMem = Allocator::Alloc(a, size + padding);
        if (newMem)
            memcpy(newMem, mem, size);
        return newMem;
    }
};

static inline size_t RoundUpTo8(size_t n)
{
    return ((n+8-1)/8)*8;
}

// BlockAllocator is for the cases where we need to allocate pieces of memory
// that are meant to be freed together. It simplifies the callers (only need
// to track this object and not all allocated pieces). Allocation and freeing
// is faster. The downside is that free() is a no-op i.e. it can't free memory
// for re-use.
//
// Note: we could be a bit more clever here by allocating data in 4K chunks
// via VirtualAlloc() etc. instead of malloc(), which would lower the overhead
class BlockAllocator : public Allocator {

    struct MemBlockNode {
        struct MemBlockNode *next;
        // data follows here
    };

    size_t          remainsInBlock;
    char *          currMem;
    MemBlockNode *  currBlock;

    void Init() {
        currBlock = NULL;
        currMem = NULL;
        remainsInBlock = 0;
    }

public:
    size_t  blockSize;

    BlockAllocator()  {
        blockSize = 4096;
        Init();
    }

    void FreeAll() {
        MemBlockNode *b = currBlock;
        while (b) {
            MemBlockNode *next = b->next;
            free(b);
            b = next;
        }
        Init();
    }

    virtual ~BlockAllocator() {
        FreeAll();
    }

    void AllocBlock(size_t minSize) {
        minSize = RoundUpTo8(minSize);
        size_t size = blockSize;
        if (minSize > size)
            size = minSize;
        MemBlockNode *node = (MemBlockNode*)calloc(1, sizeof(MemBlockNode) + size);
        currMem = (char*)node + sizeof(MemBlockNode);
        remainsInBlock = size;
        node->next = currBlock;
        currBlock = node;
    }

    // Allocator methods
    virtual void *Realloc(void *mem, size_t size) {
        // TODO: we can't do that because we don't know the original
        // size of memory piece pointed by mem. We can remember it
        // within the block that we allocate
        CrashAlwaysIf(true);
    }

    virtual void Free(void *mem) {
        // does nothing, we can't free individual pieces of memory
    }

    virtual void *Alloc(size_t size) {
        size = RoundUpTo8(size);
        if (remainsInBlock < size)
            AllocBlock(size);

        void *mem = (void*)currMem;
        currMem += size;
        remainsInBlock -= size;
        return mem;
    }

    // only valid for structs, could alloc objects with
    // placement new()
    template <typename T>
    T *AllocStruct() {
        return (T *)Alloc(sizeof(T));
    }
};

#endif
