#ifndef ARITFEX_EXTRACT_ALLOC_H
#define ARITFEX_EXTRACT_ALLOC_H

/*
	Allocation support.

	We use in/out parameters for pointers so that we can
	consistently return 0 or -1 with errno set.

	This also allows us to automatically set a caller's
	pointer to NULL when freeing or if malloc fails, and
	leave a caller's pointer unchanged if realloc fails.
*/

#include "memento.h"

#include <stdlib.h>

/*
	Abstract allocator, created by extract_alloc_create().
*/
typedef struct extract_alloc_t extract_alloc_t;

/*
	All calls to extract take a caller content argument. This will
	be parrotted back to the caller in any callbacks.
*/
typedef void extract_caller_context_t;

/*
	An externally-provided allocation function.

	Should behave like realloc(), except for taking the
	additional 'extract_caller_context *context' arg.
*/
typedef void *(extract_realloc_fn_t)(extract_caller_context_t *context, void *prev, size_t size);

/*
	Creates a new extract_alloc_t* for use with extract_malloc() etc.
*/
int extract_alloc_create(extract_realloc_fn_t *realloc_fn, void *realloc_state, extract_alloc_t **palloc);

/*
	Destroys an extract_alloc_t* that was created by
	extract_alloc_create().

	Returns with *palloc set to NULL. Does nothing if *palloc is
	already NULL.
*/
void extract_alloc_destroy(extract_alloc_t **palloc);

/*
	Sets *pptr to point to new allocated memory and returns 0. On
	error return -1 with errno set and *pptr=NULL.

	Uses malloc() if <alloc> is NULL, otherwise <alloc> must have
	been created by extract_alloc_create() and we use the
	extract_realloc_fn_t that was originally passed to
	extract_alloc_create().
*/
int extract_malloc(extract_alloc_t *alloc, void **pptr, size_t size);

/*
	Sets *pptr to point to reallocated memory and returns 0. On
	error return -1 with errno set and *pptr unchanged.

	Uses realloc() if <alloc> is NULL, otherwise <alloc> must
	have been created by extract_alloc_create() and we use the
	extract_realloc_fn_t that was originally passed to
	extract_alloc_create().
*/
int extract_realloc(extract_alloc_t *alloc, void **pptr, size_t newsize);

/*
	Frees block pointed to by *pptr and sets *pptr to NULL.

	Uses free() if <alloc> is NULL, otherwise <alloc> must have
	been created by extract_alloc_create() and we use the
	extract_realloc_fn_t that was originally passed to
	extract_alloc_create().
*/
void extract_free(extract_alloc_t *alloc, void **pptr);

/* These allow callers to use any pointer type, not just void*. */
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

/* Retrieve statistics. */
extract_alloc_stats_t *extract_alloc_stats(extract_alloc_t *alloc);

/*
	A realloc variant that takes the existing buffer size.

	If <oldsize> is not zero and *pptr is not NULL, <oldsize>
	must be the size of the existing buffer and may used
	internally to over-allocate in order to avoid too many
	calls to realloc(). See extract_alloc_exp_min() for more
	information.
*/
int extract_realloc2(extract_alloc_t *alloc, void **pptr, size_t oldsize, size_t newsize);

#define extract_realloc2(alloc, pptr, oldsize, newsize) (extract_realloc2)(alloc, (void**) pptr, oldsize, newsize)

void extract_alloc_exp_min(extract_alloc_t *alloc, size_t size);

#endif
