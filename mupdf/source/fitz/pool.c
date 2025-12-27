// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <string.h>
#include <stdio.h>

typedef struct fz_pool_node
{
	struct fz_pool_node *next;
	char mem[FZ_FLEXIBLE_ARRAY];
} fz_pool_node;

#define POOL_SIZE (4<<10) /* default size of pool blocks */
#define POOL_SELF (1<<10) /* size where allocs are put into their own blocks */

struct fz_pool
{
	size_t size;
	fz_pool_node *head, *tail;
	char *pos, *end;
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
	size = (size + FZ_POINTER_ALIGN_MOD - 1) & ~(FZ_POINTER_ALIGN_MOD-1);

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

typedef struct fz_pool_array_section
{
	struct fz_pool_array_section *next;
	size_t len;
	char elements[FZ_FLEXIBLE_ARRAY];
} fz_pool_array_section;

struct fz_pool_array
{
	fz_pool *pool;
	size_t size;
	size_t len;
	fz_pool_array_section head;
};

fz_pool_array *fz_new_pool_array_imp(fz_context *ctx, fz_pool *pool, size_t size, size_t initial)
{
	fz_pool_array *arr;

	if (initial <= 0)
		initial = 4;

	arr = fz_pool_alloc(ctx, pool, sizeof(fz_pool_array) + size * initial);

	arr->pool = pool;
	arr->size = size;
	arr->len = 0;
	arr->head.len = initial;

	return arr;
}

void *fz_pool_array_append(fz_context *ctx, fz_pool_array *arr, size_t *idx)
{
	size_t len = arr->len;
	size_t base = 0;
	fz_pool_array_section *sec = &arr->head;

	while (base + sec->len < len && sec->next != NULL)
		base += sec->len, sec = sec->next;

	if (base + sec->len == len)
	{
		/* We need to allocate a new block */
		size_t newlen = sec->len * 2;
		sec->next = fz_pool_alloc(ctx, arr->pool, sizeof(fz_pool_array_section) + arr->size * newlen);
		base = len;
		sec = sec->next;
		sec->len = newlen;
	}

	/* Now insert */
	arr->len++;
	if (idx)
		*idx = len;

	return (void *)(&sec->elements[0] + arr->size * (len - base));
}

void *fz_pool_array_lookup(fz_context *ctx, fz_pool_array *arr, size_t idx)
{
	size_t base = 0;
	fz_pool_array_section *sec = &arr->head;

	if (idx >= arr->len)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "array is not that long");

	while (base + sec->len < idx)
		base += sec->len, sec = sec->next;

	return (void *)(&sec->elements[0] + arr->size * (idx - base));
}

size_t fz_pool_array_len(fz_context *ctx, fz_pool_array *arr)
{
	return arr->len;
}
