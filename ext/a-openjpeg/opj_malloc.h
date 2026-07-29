#ifndef OPJ_MALLOC_H
#define OPJ_MALLOC_H

#include <stddef.h>

void * opj_malloc(size_t size);

void * opj_calloc(size_t numOfElements, size_t sizeOfElements);

void * opj_aligned_malloc(size_t size);
void * opj_aligned_realloc(void *ptr, size_t size);
void opj_aligned_free(void* ptr);

void * opj_aligned_32_malloc(size_t size);
void * opj_aligned_32_realloc(void *ptr, size_t size);

void * opj_realloc(void * m, size_t s);

void opj_free(void * m);

#if defined(__GNUC__) && !defined(OPJ_SKIP_POISON)
#pragma GCC poison malloc calloc realloc free
#endif

#endif
