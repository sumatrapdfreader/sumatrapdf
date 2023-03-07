#include "extract/alloc.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>


struct extract_alloc_t
{
	extract_realloc_fn_t     *realloc_fn;
	extract_caller_context_t *realloc_state;
	size_t                    exp_min_alloc_size;
	extract_alloc_stats_t     stats;
};

int
extract_alloc_create(	extract_realloc_fn_t *realloc_fn,
			void     *realloc_state,
			extract_alloc_t  **palloc)
{
	assert(realloc_fn);
	assert(palloc);
	*palloc = realloc_fn(realloc_state, NULL /*ptr*/, sizeof(**palloc));
	if (!*palloc) {
		errno = ENOMEM;
		return -1;
	}
	memset(*palloc, 0, sizeof(**palloc));
	(*palloc)->realloc_fn = realloc_fn;
	(*palloc)->realloc_state = realloc_state;
	(*palloc)->exp_min_alloc_size = 0;
	return 0;
}

void extract_alloc_destroy(extract_alloc_t **palloc)
{
	if (!*palloc) return;
	(*palloc)->realloc_fn((*palloc)->realloc_state, *palloc, 0 /*newsize*/);
	*palloc = NULL;
}

extract_alloc_stats_t *extract_alloc_stats(extract_alloc_t *alloc)
{
	return &alloc->stats;
}

static size_t round_up(extract_alloc_t *alloc, size_t n)
{
	size_t ret;

	if (alloc == NULL || alloc->exp_min_alloc_size || n == 0)
		return n;

	/* Round up to power of two. */
	ret = alloc->exp_min_alloc_size;
	while (ret < n) {
		size_t ret_old = ret;
		ret *= 2;
		if (ret <= ret_old)
			ret = n;
	}

	return ret;
}

int (extract_malloc)(extract_alloc_t *alloc, void **pptr, size_t size)
{
	void *p;

	size = round_up(alloc, size);
	p = (alloc) ? alloc->realloc_fn(alloc->realloc_state, NULL, size) : malloc(size);
	*pptr = p;
	if (!p && size)
	{
		if (alloc) errno = ENOMEM;
		return -1;
	}
	if (alloc)  alloc->stats.num_malloc += 1;
	return 0;
}

int (extract_realloc)(extract_alloc_t *alloc, void **pptr, size_t newsize)
{
	void *p = (alloc) ? alloc->realloc_fn(alloc->realloc_state, *pptr, newsize) : realloc(*pptr, newsize);
	if (!p && newsize)
	{
		if (alloc) errno = ENOMEM;
		return -1;
	}
	*pptr = p;
	if (alloc) alloc->stats.num_realloc += 1;
	return 0;
}

int (extract_realloc2)(extract_alloc_t *alloc, void **pptr, size_t oldsize, size_t newsize)
{
	/* We ignore <oldsize> if <ptr> is NULL - allows callers to not worry
	about edge cases e.g. with strlen+1. */
	oldsize = (*pptr) ? round_up(alloc, oldsize) : 0;
	newsize = round_up(alloc, newsize);
	if (newsize == oldsize) return 0;
	return (extract_realloc)(alloc, pptr, newsize);
}

void (extract_free)(extract_alloc_t *alloc, void **pptr)
{
	if (alloc)
		(void)alloc->realloc_fn(alloc->realloc_state, *pptr, 0);
	else
		free(*pptr);
	*pptr = NULL;
	if (alloc) alloc->stats.num_free += 1;
}

void extract_alloc_exp_min(extract_alloc_t *alloc, size_t size)
{
	alloc->exp_min_alloc_size = size;
}
