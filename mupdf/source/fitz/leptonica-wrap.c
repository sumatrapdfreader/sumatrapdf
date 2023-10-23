// Copyright (C) 2020-2023 Artifex Software, Inc.
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

#include "leptonica-wrap.h"

#ifdef HAVE_LEPTONICA

#include "allheaders.h"

/* When we build with our own leptonica, we want to intercept malloc/free etc.
 * Unfortunately we have to use a nasty global here. */
static fz_context *leptonica_mem = NULL;

void *leptonica_malloc(size_t size)
{
	void *ret = Memento_label(fz_malloc_no_throw(leptonica_mem, size), "leptonica_malloc");
#ifdef DEBUG_ALLOCS
	printf("%d LEPTONICA_MALLOC(%p) %d -> %p\n", event++, leptonica_mem, (int)size, ret);
	fflush(stdout);
#endif
	return ret;
}

void leptonica_free(void *ptr)
{
#ifdef DEBUG_ALLOCS
	printf("%d LEPTONICA_FREE(%p) %p\n", event++, leptonica_mem, ptr);
	fflush(stdout);
#endif
	fz_free(leptonica_mem, ptr);
}

void *leptonica_calloc(size_t numelm, size_t elemsize)
{
	void *ret = leptonica_malloc(numelm * elemsize);

	if (ret)
		memset(ret, 0, numelm * elemsize);
#ifdef DEBUG_ALLOCS
	printf("%d LEPTONICA_CALLOC %d,%d -> %p\n", event++, (int)numelm, (int)elemsize, ret);
	fflush(stdout);
#endif
	return ret;
}

/* Not currently actually used */
void *leptonica_realloc(void *ptr, size_t blocksize)
{
	void *ret = fz_realloc_no_throw(leptonica_mem, ptr, blocksize);

#ifdef DEBUG_ALLOCS
	printf("%d LEPTONICA_REALLOC %p,%d -> %p\n", event++, ptr, (int)blocksize, ret);
	fflush(stdout);
#endif
	return ret;
}

void
fz_set_leptonica_mem(fz_context *ctx)
{
	int die = 0;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	die = (leptonica_mem != NULL);
	if (!die)
		leptonica_mem = ctx;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (die)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Attempt to use Leptonica from 2 threads at once!");
	setPixMemoryManager(leptonica_malloc, leptonica_free);
}

void
fz_clear_leptonica_mem(fz_context *ctx)
{
	int die = 0;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	die = (leptonica_mem == NULL);
	if (!die)
		leptonica_mem = NULL;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (die)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Attempt to use Leptonica from 2 threads at once!");
	setPixMemoryManager(malloc, free);
}

#endif /* HAVE_LEPTONICA */
