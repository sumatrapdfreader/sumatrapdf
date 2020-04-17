#ifndef MUPDF_FITZ_POOL_H
#define MUPDF_FITZ_POOL_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	Simple pool allocators.

	Allocate from the pool, which can then be freed at once.
*/
typedef struct fz_pool fz_pool;

/*
	Create a new pool to allocate from.
*/
fz_pool *fz_new_pool(fz_context *ctx);

/*
	Allocate a block of size bytes from the pool.
*/
void *fz_pool_alloc(fz_context *ctx, fz_pool *pool, size_t size);

/*
	strdup equivalent allocating from the pool.
*/
char *fz_pool_strdup(fz_context *ctx, fz_pool *pool, const char *s);

/*
	The current size of the pool.

	The number of bytes of storage currently allocated to the pool.
	This is the total of the storage used for the blocks making
	up the pool, rather then total of the allocated blocks so far,
	so it will increase in 'lumps'.
	from the pool, then the pool size may still be X
*/
size_t fz_pool_size(fz_context *ctx, fz_pool *pool);

/*
	Drop a pool, freeing and invalidating all storage returned from
	the pool.
*/
void fz_drop_pool(fz_context *ctx, fz_pool *pool);

#endif
