/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#ifndef common_allocator_h
#define common_allocator_h

#ifdef USE_CUSTOM_ALLOCATOR

#include <stddef.h>

typedef void *(* custom_malloc_fn)(void *opaque, size_t size);
typedef void (* custom_free_fn)(void *opaque, void *ptr);

void ar_set_custom_allocator(custom_malloc_fn custom_malloc, custom_free_fn custom_free, void *opaque);

#define malloc(size) ar_malloc(size)
#define calloc(count, size) ar_calloc(count, size)
#define free(ptr) ar_free(ptr)

#define realloc(ptr, size) _use_malloc_memcpy_free_instead(ptr, size)
#define strdup(str) _use_malloc_memcpy_instead(str)

#elif !defined(NDEBUG) && defined(_MSC_VER)

#include <crtdbg.h>

#endif

#endif
