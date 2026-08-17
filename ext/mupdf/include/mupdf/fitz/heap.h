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

/* This header file declares some useful heap functions. (Heap
 * as in heap sort, not as in memory heap). It uses some
 * clever (read "hacky") multiple inclusion techniques to allow
 * us to generate multiple different versions of this code.
 * This is kinda like 'templating' in C++, but without language
 * support.
 */

/* For every instance of this code, we end up a heap structure:
 *
 *     typedef struct
 *     {
 *        int max;
 *        int len;
 *        <TYPE> *heap;
 *     } fz_<TYPE>_heap;
 *
 * This can be created and initialised on the stack in user code using:
 *
 *     fz_<TYPE>_heap heap = { 0 };
 *
 * and some functions.
 *
 * When <TYPE> is a simple int (or float or similar), the ordering required is
 * obvious, and so the functions are simple (Form 1):
 *
 * First some to insert elements into the heap:
 *
 *     void fz_<TYPE>_heap_insert(fz_context *ctx, fz_<TYPE>_heap *heap, <TYPE> v);
 *
 * Once all the elements have been inserted, the heap can be sorted:
 *
 *     void fz_<TYPE>_heap_sort(fz_context *ctx, fz_<TYPE>_heap *heap);
 *
 * Once sorted, repeated elements can be removed:
 *
 *     void fz_<TYPE>_heap_uniq(fz_context *ctx, fz_<TYPE>_heap *heap);
 *
 *
 * For more complex TYPEs (such as pointers) the ordering may not be implicit within the <TYPE>,
 * but rather depends upon the data found by dereferencing those pointers. For such types,
 * the functions are modified with a <COMPARE> function, of the form used by qsort etc:
 *
 *     int <COMPARE>(<TYPE>x, <TYPE>y) that returns 0 for x == y, +ve for x > y, and -ve for x < y.
 *
 * The functions are modified thus (Form 2):
 *
 *     void fz_<TYPE>_heap_insert(fz_context *ctx, fz_<TYPE>_heap *heap, <TYPE> v, <COMPARE> t);
 *     void fz_<TYPE>_heap_sort(fz_context *ctx, fz_<TYPE>_heap *heap, <COMPARE> t);
 *     void fz_<TYPE>_heap_uniq(fz_context *ctx, fz_<TYPE>_heap *heap, <COMPARE> t);
 *
 * Currently, we define:
 *
 *     fz_int_heap          Operates on 'int' values. Form 1.
 *     fz_ptr_heap          Operates on 'void *' values. Form 2.
 *     fz_int2_heap         Operates on 'typedef struct { int a; int b} fz_int2' values,
 *                          with the sort/uniq being done based on 'a' alone. Form 1.
 *     fz_intptr_heap       Operates on 'typedef struct { int a; void *b} fz_intptr' values,
 *                          with the sort/uniq being done based on 'a' alone. Form 1.
 */

/* Everything after this point is preprocessor magic. Ignore it, and just read the above
 * unless you are wanting to instantiate a new set of functions. */

#ifndef MUPDF_FITZ_HEAP_H

#define MUPDF_FITZ_HEAP_H

#define MUPDF_FITZ_HEAP_I_KNOW_WHAT_IM_DOING

/* Instantiate fz_int_heap */
#define HEAP_TYPE_NAME fz_int_heap
#define HEAP_CONTAINER_TYPE int
#define HEAP_CMP(a,b) ((*a) - (*b))
#define HEAP_DUMP(CTX, OUT, I, A) fz_write_printf(CTX, OUT, "%d: %d\n", I, *A)
#include "mupdf/fitz/heap-imp.h"

/* Instantiate fz_ptr_heap */
#define HEAP_TYPE_NAME fz_ptr_heap
#define HEAP_CONTAINER_TYPE void *
#include "mupdf/fitz/heap-imp.h"

/* Instantiate fz_int2_heap */
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
typedef struct
{
	int a;
	int b;
} fz_int2;
#endif
#define HEAP_TYPE_NAME fz_int2_heap
#define HEAP_CMP(A,B) (((A)->a) - ((B)->a))
#define HEAP_CONTAINER_TYPE fz_int2
#define HEAP_DUMP(CTX, OUT, I, A) fz_write_printf(CTX, OUT, "%d: %d %d\n", I, (A)->a, (A)->b)
#include "mupdf/fitz/heap-imp.h"

/* Instantiate fz_intptr_heap */
#ifndef MUPDF_FITZ_HEAP_IMPLEMENT
typedef struct
{
	int a;
	void *b;
} fz_intptr;
#endif
#define HEAP_TYPE_NAME fz_intptr_heap
#define HEAP_CONTAINER_TYPE fz_intptr
#define HEAP_CMP(A,B) (((A)->a) - ((B)->a))
#define HEAP_DUMP(CTX, OUT, I, A) fz_write_printf(CTX, OUT, "%d: %d %p\n", I, (A)->a, (A)->b)
#include "mupdf/fitz/heap-imp.h"

#endif /* MUPDF_FITZ_HEAP_H */
