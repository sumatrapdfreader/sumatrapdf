#ifndef ARITFEX_EXTRACT_ALLOC_H
#define ARITFEX_EXTRACT_ALLOC_H

#include "memento.h"

#include <stdlib.h>

typedef struct extract_alloc_t extract_alloc_t;

typedef void extract_caller_context_t;

typedef void *(extract_realloc_fn_t)(extract_caller_context_t *context, void *prev, size_t size);

int extract_alloc_create(extract_realloc_fn_t *realloc_fn, void *realloc_state, extract_alloc_t **palloc);

void extract_alloc_destroy(extract_alloc_t **palloc);

int extract_malloc(extract_alloc_t *alloc, void **pptr, size_t size);

int extract_realloc(extract_alloc_t *alloc, void **pptr, size_t newsize);

void extract_free(extract_alloc_t *alloc, void **pptr);

#define extract_malloc(alloc, pptr, size)     (extract_malloc) (alloc, (void**)pptr, size)
#define extract_realloc(alloc, pptr, newsize) (extract_realloc)(alloc, (void**)pptr, newsize)
#define extract_free(alloc, pptr)             (extract_free)   (alloc, (void**)pptr)

typedef struct
{
	int num_malloc;
	int num_realloc;
	int num_free;
	int num_libc_realloc;
} extract_alloc_stats_t;

extract_alloc_stats_t *extract_alloc_stats(extract_alloc_t *alloc);

int extract_realloc2(extract_alloc_t *alloc, void **pptr, size_t oldsize, size_t newsize);

#define extract_realloc2(alloc, pptr, oldsize, newsize) (extract_realloc2)(alloc, (void**) pptr, oldsize, newsize)

void extract_alloc_exp_min(extract_alloc_t *alloc, size_t size);

#endif
