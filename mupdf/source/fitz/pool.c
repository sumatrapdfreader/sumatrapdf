#include "mupdf/fitz.h"

#include <string.h>
#include <stdio.h>

typedef struct fz_pool_node_s fz_pool_node;

#define POOL_SIZE (4<<10) /* default size of pool blocks */
#define POOL_SELF (1<<10) /* size where allocs are put into their own blocks */

struct fz_pool_s
{
	size_t size;
	fz_pool_node *head, *tail;
	char *pos, *end;
};

struct fz_pool_node_s
{
	fz_pool_node *next;
	char mem[1];
};

fz_pool *fz_new_pool(fz_context *ctx)
{
	fz_pool *pool;
	fz_pool_node *node = NULL;

	pool = fz_malloc_struct(ctx, fz_pool);
	fz_try(ctx)
	{
		node = Memento_label(fz_calloc(ctx, offsetof(fz_pool_node, mem) + POOL_SIZE, 1), "fz_pool_block");
		pool->head = pool->tail = node;
		pool->pos = node->mem;
		pool->end = node->mem + POOL_SIZE;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, pool);
		fz_rethrow(ctx);
	}

	return pool;
}

static void *fz_pool_alloc_oversize(fz_context *ctx, fz_pool *pool, size_t size)
{
	fz_pool_node *node;

	/* link in memory at the head of the list */
	node = Memento_label(fz_calloc(ctx, offsetof(fz_pool_node, mem) + size, 1), "fz_pool_oversize");
	node->next = pool->head;
	pool->head = node;
	pool->size += offsetof(fz_pool_node, mem) + size;

	return node->mem;
}

void *fz_pool_alloc(fz_context *ctx, fz_pool *pool, size_t size)
{
	char *ptr;

	if (size >= POOL_SELF)
		return fz_pool_alloc_oversize(ctx, pool, size);

	/* round size to pointer alignment (we don't expect to use doubles) */
	size = ((size + sizeof(void*) - 1) / sizeof(void*)) * sizeof(void*);

	if (pool->pos + size > pool->end)
	{
		fz_pool_node *node = Memento_label(fz_calloc(ctx, offsetof(fz_pool_node, mem) + POOL_SIZE, 1), "fz_pool_block");
		pool->tail = pool->tail->next = node;
		pool->pos = node->mem;
		pool->end = node->mem + POOL_SIZE;
		pool->size += offsetof(fz_pool_node, mem) + POOL_SIZE;
	}
	ptr = pool->pos;
	pool->pos += size;
	return ptr;
}

char *fz_pool_strdup(fz_context *ctx, fz_pool *pool, const char *s)
{
	size_t n = strlen(s) + 1;
	char *p = fz_pool_alloc(ctx, pool, n);
	memcpy(p, s, n);
	return p;
}

size_t fz_pool_size(fz_context *ctx, fz_pool *pool)
{
	return pool ? pool->size : 0;
}

void fz_drop_pool(fz_context *ctx, fz_pool *pool)
{
	fz_pool_node *node;

	if (!pool)
		return;

	node = pool->head;
	while (node)
	{
		fz_pool_node *next = node->next;
		fz_free(ctx, node);
		node = next;
	}
	fz_free(ctx, pool);
}
