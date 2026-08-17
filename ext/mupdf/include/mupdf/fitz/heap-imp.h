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

/* This file has preprocessor magic in it to instantiate both
 * prototypes and implementations for heap sorting structures
 * of various different types. Effectively, it's templating for
 * C.
 *
 * If you are including this file directly without intending to
 * be instantiating a new set of heap sort functions, you are
 * doing the wrong thing.
 */

#ifndef MUPDF_FITZ_HEAP_I_KNOW_WHAT_IM_DOING
#error Do not include heap-imp.h unless you know what you are doing
#endif

#define HEAP_XCAT(A,B) A##B
#define HEAP_CAT(A,B) HEAP_XCAT(A,B)

#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
typedef struct
{
  int max;
  int len;
  HEAP_CONTAINER_TYPE *heap;
} HEAP_TYPE_NAME;
#endif

void HEAP_CAT(HEAP_TYPE_NAME,_insert)(fz_context *ctx, HEAP_TYPE_NAME *heap, HEAP_CONTAINER_TYPE v
#ifndef HEAP_CMP
				, int (*HEAP_CMP)(HEAP_CONTAINER_TYPE *a, HEAP_CONTAINER_TYPE *b)
#endif
				)
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
;
#else
{
	int i;
	HEAP_CONTAINER_TYPE *h;

	if (heap->max == heap->len)
	{
		int m = heap->max * 2;

		if (m == 0)
			m = 32;

		heap->heap = (HEAP_CONTAINER_TYPE *)fz_realloc(ctx, heap->heap, sizeof(*heap->heap) * m);
		heap->max = m;
	}
	h = heap->heap;

	/* Insert it into the heap. Consider inserting at position i, and
	 * then 'heapify' back. We can delay the actual insertion to the
	 * end of the process. */
	i = heap->len++;
	while (i != 0)
	{
		int parent_idx = (i-1)/2;
		HEAP_CONTAINER_TYPE *parent_val = &h[parent_idx];
		if (HEAP_CMP(parent_val, &v) > 0)
			break;
		h[i] = h[parent_idx];
		i = parent_idx;
	}
	h[i] = v;
}
#endif

void HEAP_CAT(HEAP_TYPE_NAME,_sort)(fz_context *ctx, HEAP_TYPE_NAME *heap
#ifndef HEAP_CMP
				, int (*HEAP_CMP)(HEAP_CONTAINER_TYPE *a, HEAP_CONTAINER_TYPE *b)
#endif
				)
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
;
#else
{
	int j;
	HEAP_CONTAINER_TYPE *h = heap->heap;

	/* elements j to len are always sorted. 0 to j are always a valid heap. Gradually move j to 0. */
	for (j = heap->len-1; j > 0; j--)
	{
		int k;
		HEAP_CONTAINER_TYPE val;

		/* Swap max element with j. Invariant valid for next value to j. */
		val = h[j];
		h[j] = h[0];
		/* Now reform the heap. 0 to k is a valid heap. */
		k = 0;
		while (1)
		{
			int kid = k*2+1;
			if (kid >= j)
				break;
			if (kid+1 < j && (HEAP_CMP(&h[kid+1], &h[kid])) > 0)
				kid++;
			if ((HEAP_CMP(&val, &h[kid])) > 0)
				break;
			h[k] = h[kid];
			k = kid;
		}
		h[k] = val;
	}
}
#endif

void HEAP_CAT(HEAP_TYPE_NAME,_uniq)(fz_context *ctx, HEAP_TYPE_NAME *heap
#ifndef HEAP_CMP
				, int (*HEAP_CMP)(HEAP_CONTAINER_TYPE *a, HEAP_CONTAINER_TYPE *b)
#endif
				)
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
;
#else
{
	int n = heap->len;
	int i, j = 0;
	HEAP_CONTAINER_TYPE *h = heap->heap;

	if (n == 0)
		return;

	j = 0;
	for (i = 1; i < n; i++)
	{
		if (HEAP_CMP(&h[j], &h[i]) == 0)
			continue;
		j++;
		if (i != j)
			h[j] = h[i];
	}
	heap->len = j+1;
}
#endif

#ifdef HEAP_DUMP
void HEAP_CAT(HEAP_TYPE_NAME,_dump)(fz_context *ctx, fz_output *out, HEAP_TYPE_NAME *heap)
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
;
#else
{
	int n = heap->len;
	int i;
	HEAP_CONTAINER_TYPE *h = heap->heap;

	fz_write_printf(ctx, out, "Heap %p len %d:\n", heap, n);
	for (i = 0; i < n; i++)
		HEAP_DUMP(ctx, out, i, &h[i]);
}
#endif

void HEAP_CAT(HEAP_TYPE_NAME,_debug)(fz_context *ctx, HEAP_TYPE_NAME *heap)
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
;
#else
{
	HEAP_CAT(HEAP_TYPE_NAME,_dump)(ctx, fz_stddbg(ctx), heap);
}
#endif
#endif

#undef HEAP_CONTAINER_TYPE
#undef HEAP_TYPE_NAME
#undef HEAP_CMP
#undef HEAP_XCAT
#undef HEAP_CAT
#undef HEAP_DUMP
