/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef void *(* custom_malloc_fn)(void *opaque, size_t size);
typedef void (* custom_free_fn)(void *opaque, void *ptr);

static void *default_malloc(void *opaque, size_t size) { (void)opaque; return malloc(size); }
static void default_free(void *opaque, void *ptr) { (void)opaque; free(ptr); }

static struct {
    custom_malloc_fn malloc;
    custom_free_fn free;
    void *opaque;
} gAllocator = {
    default_malloc,
    default_free,
    NULL,
};

void *ar_malloc(size_t size)
{
    return gAllocator.malloc(gAllocator.opaque, size);
}

void *ar_calloc(size_t count, size_t size)
{
    void *ptr = NULL;
    if (size <= SIZE_MAX / count)
        ptr = ar_malloc(count * size);
    if (ptr)
        memset(ptr, 0, count * size);
    return ptr;
}

void ar_free(void *ptr)
{
    gAllocator.free(gAllocator.opaque, ptr);
}

void ar_set_custom_allocator(custom_malloc_fn custom_malloc, custom_free_fn custom_free, void *opaque)
{
    gAllocator.malloc = custom_malloc ? custom_malloc : default_malloc;
    gAllocator.free = custom_free ? custom_free : default_free;
    gAllocator.opaque = opaque;
}
