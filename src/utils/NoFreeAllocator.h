/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// Per-thread stats for no-free allocator. Knowing them allows
// tweaking the sizes of memory blocks and frequency of 
// NoFreeAllocatorMark placement.
struct AllocStats {
    size_t  allocsCount;
    size_t  maxMemUse;
    uint64  totalMemAlloced;
};

class NoFreeAllocatorMark {
    // NULL is valid and has a special meaning: it means the first
    // MemBlock. It allows us to delay allocating first MemBlock to
    // the moment of first mallocNF() calls (useful for threads that
    // might but not necessarily will allocate)
    struct MemBlock *   block;
    // position within block
    size_t              pos;
    // block can be NULL in more than one object but we need to
    // know which one was the very first (it has to free all memory)
    // We keep track of it with isFirst and we know that because only
    // the first allocates gStats
    bool                isFirst;
public:
    NoFreeAllocatorMark();
    ~NoFreeAllocatorMark();
};

void *mallocNF(size_t size);

