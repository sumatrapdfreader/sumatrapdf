/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* to use a custom allocator, define INC_CUSTOM_ALLOC="unzalloc.h" when
   compiling zlib and call unzSetAllocFuncs before opening a file */

typedef struct {
    void* (*zip_alloc)(void *opaque, size_t items, size_t size);
    void (*zip_free)(void *opaque, void *addr);
} UnzipAllocFuncs;

void unzSetAllocFuncs(UnzipAllocFuncs *funcs);

void *unzAlloc(size_t size);
void unzFree(void *p);

#define ALLOC(size) unzAlloc(size)
#define TRYFREE(p) unzFree(p)

#ifdef __cplusplus
}
#endif
