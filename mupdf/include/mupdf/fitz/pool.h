// Copyright (C) 2004-2021 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_POOL_H
#define MUPDF_FITZ_POOL_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/**
	Simple pool allocators.

	Allocate from the pool, which can then be freed at once.
*/
typedef struct fz_pool fz_pool;

/**
	Create a new pool to allocate from.
*/
fz_pool *fz_new_pool(fz_context *ctx);

/**
	Allocate a block of size bytes from the pool.
*/
void *fz_pool_alloc(fz_context *ctx, fz_pool *pool, size_t size);

/**
	strdup equivalent allocating from the pool.
*/
char *fz_pool_strdup(fz_context *ctx, fz_pool *pool, const char *s);

/**
	The current size of the pool.

	The number of bytes of storage currently allocated to the pool.
	This is the total of the storage used for the blocks making
	up the pool, rather then total of the allocated blocks so far,
	so it will increase in 'lumps'.
	from the pool, then the pool size may still be X
*/
size_t fz_pool_size(fz_context *ctx, fz_pool *pool);

/**
	Drop a pool, freeing and invalidating all storage returned from
	the pool.
*/
void fz_drop_pool(fz_context *ctx, fz_pool *pool);

/**
	Routines to handle a 'variable length array' within the pool.

	Appending to the array, and looking up items within the array
	are O(log n) operations.
*/
typedef struct fz_pool_array fz_pool_array;

/**
	Create a new pool array for a given type, with a given initial size.
*/
#define fz_new_pool_array(CTX, POOL, TYPE, INIT) \
	fz_new_pool_array_imp(CTX, POOL, sizeof(TYPE), INIT);

fz_pool_array *fz_new_pool_array_imp(fz_context *ctx, fz_pool *pool, size_t size, size_t initial);

/**
	Append an element to the end of the array.

	Returns a pointer to the new element (initially all 0's), and
	(optionally) the index of that element.
*/
void *fz_pool_array_append(fz_context *ctx, fz_pool_array *arr, size_t *idx);

/**
	Lookup an element in the array.
*/
void *fz_pool_array_lookup(fz_context *ctx, fz_pool_array *arr, size_t idx);

/**
	Get the length of the array.
*/
size_t fz_pool_array_len(fz_context *ctx, fz_pool_array *arr);

#endif
