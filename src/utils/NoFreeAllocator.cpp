/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* NOTE: this is unfinished work in progress */

/* NoFreeAllocator (ScratchAllocator ?) is designed for quickly and easily
allocating temporary memory that doesn't outlive the stack frame
in which it was allocated.

Consider this piece of code:

bool NormalizedFileExists(const TCHAR *path)
{
    ScopedMem<TCHAR> normpath(Normalize(path));
    return FileExist(normpath.Get());
}

This is a simpler version but it leaks:

bool NormalizedFileExists(const TCHAR *path)
{
    TCHAR *normpath = Normalize(path);
    return FileExist(normpath);
}

What if could guarantee that normapth will be freed at some point
in the future? NoFreeAllocator has the necessary magic.

We have a thread-local global object used mallocNF() and other
no-free functions.

NoFreeAllocatorMark is an object placed on the stack to mark
a particular position in the allocation sequence. When this
object goes out of scope, we'll free all allocations done with
no-free allocatora from the moment in was constructed.

Allocation and de-allocation is fast because we allocate memory
in blocks. Allocation, most of the time, is just bumping a pointer.
De-allocation involves freeing just few blocks.

In the simplest scenario we can put NoFreeAllocatorMark in WinMain.
The memory would be freed but we would grow memory used without bounds.

We can tweak memory used by using NoFreeAllocatorMark in more
places. The more often it's used, the lower max memory usage will be.
A natural place for placing them are window message loops.

They can also be used in threads by placing an object at the
beginning of a thread function.

We can optimize things by using a genereously sized blocks e.g. 1MB
is nothing by today's standards and the bigger our memory block,
the less mallocs/frees we do.

Another advantage is that this generates less executable code: every
time ScopedMem<T> is used, it adds a few bytes of code.

The downside is that we need to write new versions of functions that
allocate the memory.

Another downside is that this memory isn't visible to memory profilers
and doesn't have the debugging features of some allocators (we
could easily add most of them, like detection of memory over-writes)

This allocator bears some resemblance to NSAutoReleasePool and 
garbage-collection in general.
*/

#include "NoFreeAllocator.h"

enum {
    KB = 1024,
    MB = 1024 * KB,
    MEM_BLOCK_SIZE = 1 * MB
};

struct MemBlock {
    struct MemBlock *   next;
    size_t              size;
    size_t              left;
    size_t Used() { return size - left; }
    // data follows
};

__declspec(thread) MemBlock *    gCurrMemBlock = NULL;
__declspec(thread) AllocStats *  gStats = NULL;

NoFreeAllocatorMark::NoFreeAllocatorMark()
{
    isFirst = false;
    if (!gStats) {
        CrashAlwaysIf(gCurrMemBlock);
        isFirst = true;
        gStats = SAZA(AllocStats,1);
    }
    block = gCurrMemBlock;
    pos = 0;
    if (block)
        pos = block->Used();
}

NoFreeAllocatorMark::~NoFreeAllocatorMark()
{
    if (isFirst) {
        free(gStats);
        gStats = NULL;
        while (gCurrMemBlock) {
            MemBlock *toFree = gCurrMemBlock;
            gCurrMemBlock = gCurrMemBlock->next;
            free(toFree);
        }
        return;
    }

    while (gCurrMemBlock != block) {
        MemBlock *toFree = gCurrMemBlock;
        gCurrMemBlock = gCurrMemBlock->next;
        free(toFree);
    }
    if (gCurrMemBlock)
        gCurrMemBlock->left = gCurrMemBlock->size - pos;
}

void *mallocNF(size_t size)
{
    // gStats is created on the first NoFreeAllocatorMark. If it's not
    // there, we've been incorrectly called without NoFreeAllocatorMark
    // somewhere above in the callstack chain
    CrashAlwaysIf(!gStats);

    MemBlock *m = gCurrMemBlock;

    if (!m || (size > m->left)) {
        size_t blockSize = max(MEM_BLOCK_SIZE, size + sizeof(MemBlock));
        MemBlock *block = (MemBlock*)malloc(blockSize);
        block->size = blockSize - sizeof(MemBlock);
        block->left = block->size;
        block->next = gCurrMemBlock;
        gCurrMemBlock = block;
        size_t currMemUse = 0;
        block = gCurrMemBlock;
        while (block) {
            currMemUse += (block->size + sizeof(MemBlock));
            block = block->next;
        }
        if (currMemUse > gStats->maxMemUse)
            gStats->maxMemUse = currMemUse;
        m = gCurrMemBlock;
    }

    CrashAlwaysIf(!m || (size > m->left));
    char *res = (char*)m + sizeof(MemBlock) + m->Used();
    m->left -= size;

    gStats->allocsCount++;
    gStats->totalMemAlloced += size;

    return (void*)res;
}

namespace str {

WCHAR *ToWideCharNF(const char *src, UINT codePage)
{
    CrashIf(!src);
    if (!src) return NULL;

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, -1, NULL, 0);
    WCHAR *res = (WCHAR*)mallocNF(sizeof(WCHAR) * requiredBufSize);
    if (!res)
        return NULL;
    MultiByteToWideChar(codePage, 0, src, -1, res, requiredBufSize);
    return res;
}

}
