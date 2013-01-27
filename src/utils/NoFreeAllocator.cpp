/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* NOTE: this is unfinished work in progress */

/* NoFreeAllocator (ScratchAllocator ?) is designed for quickly and easily
allocating temporary memory that doesn't outlive the stack frame
in which it was allocated.

Consider this piece of code:

bool NormalizedFileExists(const WCHAR *path)
{
    ScopedMem<WCHAR> normpath(Normalize(path));
    return FileExist(normpath.Get());
}

This is a simpler version but it leaks:

bool NormalizedFileExists(const WCHAR *path)
{
    WCHAR *normpath = Normalize(path);
    return FileExist(normpath);
}

What if could guarantee that normapth will be freed at some point
in the future? NoFreeAllocator has the necessary magic.

We have a thread-local global object used nf::alloc() and other
no-free functions.

AllocatorMark is an object placed on the stack to mark
a particular position in the allocation sequence. When this
object goes out of scope, we'll free all allocations done with
no-free allocatora from the moment in was constructed.

Allocation and de-allocation is fast because we allocate memory
in blocks. Allocation, most of the time, is just bumping a pointer.
De-allocation involves freeing just few blocks.

In the simplest scenario we can put AllocatorMark in WinMain.
The memory would be freed but we would grow memory used without bounds.

We can tweak memory used by using AllocatorMark in more
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

namespace nf {

struct MemBlock {
    struct MemBlock *   next;
    size_t              size;
    size_t              left;
    size_t Used() { return size - left; }
    // data follows
};

__declspec(thread) MemBlock *    gCurrMemBlock = NULL;
__declspec(thread) AllocStats *  gStats = NULL;

AllocatorMark::AllocatorMark()
{
    isFirst = false;
    if (!gStats) {
        CrashAlwaysIf(gCurrMemBlock);
        isFirst = true;
        gStats = AllocArray<AllocStats>(1);
    }
    block = gCurrMemBlock;
    pos = 0;
    if (block)
        pos = block->Used();
}

AllocatorMark::~AllocatorMark()
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

void *alloc(size_t size)
{
    // gStats is created on the first AllocatorMark. If it's not
    // there, we've been incorrectly called without AllocatorMark
    // somewhere above in the callstack chain
    CrashAlwaysIf(!gStats);

    MemBlock *m = gCurrMemBlock;

    if (!m || (size > m->left)) {
        size_t blockSize = max(MEM_BLOCK_SIZE, size + sizeof(MemBlock));
        MemBlock *block = (MemBlock*)malloc(blockSize);
        if (!block)
            return NULL;
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

void *memdup(const void *ptr, size_t size)
{
    void *res = alloc(size);
    if (res)
        memcpy(res, ptr, size);
    return res;
}

namespace str {

char *ToMultiByte(const WCHAR *txt, UINT codePage)
{
    CrashIf(txt);
    if (!txt) return NULL;

    int requiredBufSize = WideCharToMultiByte(codePage, 0, txt, -1, NULL, 0, NULL, NULL);
    if (0 == requiredBufSize)
        return NULL;
    char *res = (char *)alloc(requiredBufSize);
    if (!res)
        return NULL;
    WideCharToMultiByte(codePage, 0, txt, -1, res, requiredBufSize, NULL, NULL);
    return res;
}

char *ToMultiByte(const char *src, UINT codePageSrc, UINT codePageDest)
{
    CrashIf(src);
    if (!src) return NULL;

    if (codePageSrc == codePageDest)
        return Dup(src);

    return ToMultiByte(ToWideChar(src, codePageSrc), codePageDest);
}

WCHAR *ToWideChar(const char *src, UINT codePage)
{
    CrashIf(!src);
    if (!src) return NULL;

    int requiredBufSize = MultiByteToWideChar(codePage, 0, src, -1, NULL, 0);
    if (0 == requiredBufSize)
        return NULL;
    WCHAR *res = (WCHAR*)alloc(sizeof(WCHAR) * requiredBufSize);
    if (!res)
        return NULL;
    MultiByteToWideChar(codePage, 0, src, -1, res, requiredBufSize);
    return res;
}

} // namespace str

} // namespace nf
