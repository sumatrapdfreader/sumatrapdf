// Copyright (C) 2004-2022 Artifex Software, Inc.
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
#include "pdf-annot-imp.h"

#include <zlib.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <stdio.h> /* for debug printing */
/* #define DEBUG_LINEARIZATION */
/* #define DEBUG_HEAP_SORT */
/* #define DEBUG_WRITING */
/* #define DEBUG_MARK_AND_SWEEP */

#define SIG_EXTRAS_SIZE (1024)

#define SLASH_BYTE_RANGE ("/ByteRange")
#define SLASH_CONTENTS ("/Contents")
#define SLASH_FILTER ("/Filter")


/*
	As part of linearization, we need to keep a list of what objects are used
	by what page. We do this by recording the objects used in a given page
	in a page_objects structure. We have a list of these structures (one per
	page) in the page_objects_list structure.

	The page_objects structure maintains a heap in the object array, so
	insertion takes log n time, and we can heapsort and dedupe at the end for
	a total worse case n log n time.

	The magic heap invariant is that:
		entry[n] >= entry[(n+1)*2-1] & entry[n] >= entry[(n+1)*2]
	or equivalently:
		entry[(n-1)>>1] >= entry[n]

	For a discussion of the heap data structure (and heapsort) see Kingston,
	"Algorithms and Data Structures".
*/

typedef struct {
	int num_shared;
	int page_object_number;
	int num_objects;
	int min_ofs;
	int max_ofs;
	/* Extensible list of objects used on this page */
	int cap;
	int len;
	int object[1];
} page_objects;

typedef struct {
	int cap;
	int len;
	page_objects *page[1];
} page_objects_list;

typedef struct
{
	fz_output *out;

	int do_incremental;
	int do_tight;
	int do_ascii;
	int do_expand;
	int do_compress;
	int do_compress_images;
	int do_compress_fonts;
	int do_garbage;
	int do_linear;
	int do_clean;
	int do_encrypt;
	int dont_regenerate_id;
	int do_snapshot;
	int do_preserve_metadata;
	int do_use_objstms;
	int compression_effort;

	int list_len;
	int *use_list;
	int64_t *ofs_list;
	int *gen_list;
	int *renumber_map;

	/* The following extras are required for linearization */
	int *rev_renumber_map;
	int start;
	int64_t first_xref_offset;
	int64_t main_xref_offset;
	int64_t first_xref_entry_offset;
	int64_t file_len;
	int hints_shared_offset;
	int64_t hintstream_len;
	pdf_obj *linear_l;
	pdf_obj *linear_h0;
	pdf_obj *linear_h1;
	pdf_obj *linear_o;
	pdf_obj *linear_e;
	pdf_obj *linear_n;
	pdf_obj *linear_t;
	pdf_obj *hints_s;
	pdf_obj *hints_length;
	int hint_object_num;
	int page_count;
	page_objects_list *page_object_lists;
	int crypt_object_number;
	char opwd_utf8[128];
	char upwd_utf8[128];
	int permissions;
	pdf_crypt *crypt;
	pdf_obj *crypt_obj;
	pdf_obj *metadata;
} pdf_write_state;

/*
 * Constants for use with use_list.
 *
 * If use_list[num] = 0, then object num is unused.
 * If use_list[num] & PARAMS, then object num is the linearisation params obj.
 * If use_list[num] & CATALOGUE, then object num is used by the catalogue.
 * If use_list[num] & PAGE1, then object num is used by page 1.
 * If use_list[num] & SHARED, then object num is shared between pages.
 * If use_list[num] & PAGE_OBJECT then this must be the first object in a page.
 * If use_list[num] & OTHER_OBJECTS then this must should appear in section 9.
 * Otherwise object num is used by page (use_list[num]>>USE_PAGE_SHIFT).
 */
enum
{
	USE_CATALOGUE = 2,
	USE_PAGE1 = 4,
	USE_SHARED = 8,
	USE_PARAMS = 16,
	USE_HINTS = 32,
	USE_PAGE_OBJECT = 64,
	USE_OTHER_OBJECTS = 128,
	USE_PAGE_MASK = ~255,
	USE_PAGE_SHIFT = 8
};

static void
expand_lists(fz_context *ctx, pdf_write_state *opts, int num)
{
	int i;

	/* objects are numbered 0..num and maybe two additional objects for linearization */
	num += 3;
	if (num <= opts->list_len)
		return;

	opts->use_list = fz_realloc_array(ctx, opts->use_list, num, int);
	opts->ofs_list = fz_realloc_array(ctx, opts->ofs_list, num, int64_t);
	opts->gen_list = fz_realloc_array(ctx, opts->gen_list, num, int);
	opts->renumber_map = fz_realloc_array(ctx, opts->renumber_map, num, int);
	opts->rev_renumber_map = fz_realloc_array(ctx, opts->rev_renumber_map, num, int);

	for (i = opts->list_len; i < num; i++)
	{
		opts->use_list[i] = 0;
		opts->ofs_list[i] = 0;
		opts->gen_list[i] = 0;
		opts->renumber_map[i] = i;
		opts->rev_renumber_map[i] = i;
	}
	opts->list_len = num;
}

/*
 * page_objects and page_object_list handling functions
 */
static page_objects_list *
page_objects_list_create(fz_context *ctx)
{
	page_objects_list *pol = fz_calloc(ctx, 1, sizeof(*pol));

	pol->cap = 1;
	pol->len = 0;
	return pol;
}

static void
page_objects_list_destroy(fz_context *ctx, page_objects_list *pol)
{
	int i;

	if (!pol)
		return;
	for (i = 0; i < pol->len; i++)
	{
		fz_free(ctx, pol->page[i]);
	}
	fz_free(ctx, pol);
}

static void
page_objects_list_ensure(fz_context *ctx, page_objects_list **pol, int newcap)
{
	int oldcap = (*pol)->cap;
	if (newcap <= oldcap)
		return;
	*pol = fz_realloc(ctx, *pol, sizeof(page_objects_list) + (newcap-1)*sizeof(page_objects *));
	memset(&(*pol)->page[oldcap], 0, (newcap-oldcap)*sizeof(page_objects *));
	(*pol)->cap = newcap;
}

static page_objects *
page_objects_create(fz_context *ctx)
{
	int initial_cap = 8;
	page_objects *po = fz_calloc(ctx, 1, sizeof(*po) + (initial_cap-1) * sizeof(int));

	po->cap = initial_cap;
	po->len = 0;
	return po;
}

static void
page_objects_insert(fz_context *ctx, page_objects **ppo, int i)
{
	page_objects *po;

	/* Make a page_objects if we don't have one */
	if (*ppo == NULL)
		*ppo = page_objects_create(ctx);

	po = *ppo;
	/* page_objects insertion: extend the page_objects by 1, and put us on the end */
	if (po->len == po->cap)
	{
		po = fz_realloc(ctx, po, sizeof(page_objects) + (po->cap*2 - 1)*sizeof(int));
		po->cap *= 2;
		*ppo = po;
	}
	po->object[po->len++] = i;
}

static void
page_objects_list_insert(fz_context *ctx, pdf_write_state *opts, int page, int object)
{
	page_objects_list_ensure(ctx, &opts->page_object_lists, page+1);
	if (object >= opts->list_len)
		expand_lists(ctx, opts, object);
	if (opts->page_object_lists->len < page+1)
		opts->page_object_lists->len = page+1;
	page_objects_insert(ctx, &opts->page_object_lists->page[page], object);
}

static void
page_objects_list_set_page_object(fz_context *ctx, pdf_write_state *opts, int page, int object)
{
	page_objects_list_ensure(ctx, &opts->page_object_lists, page+1);
	if (object >= opts->list_len)
		expand_lists(ctx, opts, object);
	opts->page_object_lists->page[page]->page_object_number = object;
}

static void
page_objects_sort(fz_context *ctx, page_objects *po)
{
	int i, j;
	int n = po->len;

	/* Step 1: Make a heap */
	/* Invariant: Valid heap in [0..i), unsorted elements in [i..n) */
	for (i = 1; i < n; i++)
	{
		/* Now bubble backwards to maintain heap invariant */
		j = i;
		while (j != 0)
		{
			int tmp;
			int k = (j-1)>>1;
			if (po->object[k] >= po->object[j])
				break;
			tmp = po->object[k];
			po->object[k] = po->object[j];
			po->object[j] = tmp;
			j = k;
		}
	}

	/* Step 2: Heap sort */
	/* Invariant: valid heap in [0..i), sorted list in [i..n) */
	/* Initially: i = n */
	for (i = n-1; i > 0; i--)
	{
		/* Swap the maximum (0th) element from the page_objects into its place
		 * in the sorted list (position i). */
		int tmp = po->object[0];
		po->object[0] = po->object[i];
		po->object[i] = tmp;
		/* Now, the page_objects is invalid because the 0th element is out
		 * of place. Bubble it until the page_objects is valid. */
		j = 0;
		while (1)
		{
			/* Children are k and k+1 */
			int k = (j+1)*2-1;
			/* If both children out of the page_objects, we're done */
			if (k > i-1)
				break;
			/* If both are in the page_objects, pick the larger one */
			if (k < i-1 && po->object[k] < po->object[k+1])
				k++;
			/* If j is bigger than k (i.e. both of its children),
			 * we're done */
			if (po->object[j] > po->object[k])
				break;
			tmp = po->object[k];
			po->object[k] = po->object[j];
			po->object[j] = tmp;
			j = k;
		}
	}
}

static int
order_ge(int ui, int uj)
{
	/*
	For linearization, we need to order the sections as follows:

		Remaining pages					(Part 7)
		Shared objects					(Part 8)
		Objects not associated with any page		(Part 9)
		Any "other" objects
							(Header)(Part 1)
		(Linearization params)				(Part 2)
					(1st page Xref/Trailer)	(Part 3)
		Catalogue (and other document level objects)	(Part 4)
		First page					(Part 6)
		(Primary Hint stream)			(*)	(Part 5)
		Any free objects

	Note, this is NOT the same order they appear in
	the final file!

	(*) The PDF reference gives us the option of putting the hint stream
	after the first page, and we take it, for simplicity.
	*/

	/* If the 2 objects are in the same section, then page object comes first. */
	if (((ui ^ uj) & ~USE_PAGE_OBJECT) == 0)
		return ((ui & USE_PAGE_OBJECT) == 0);
	/* Put unused objects last */
	else if (ui == 0)
		return 1;
	else if (uj == 0)
		return 0;
	/* Put the hint stream before that... */
	else if (ui & USE_HINTS)
		return 1;
	else if (uj & USE_HINTS)
		return 0;
	/* Put page 1 before that... */
	else if (ui & USE_PAGE1)
		return 1;
	else if (uj & USE_PAGE1)
		return 0;
	/* Put the catalogue before that... */
	else if (ui & USE_CATALOGUE)
		return 1;
	else if (uj & USE_CATALOGUE)
		return 0;
	/* Put the linearization params before that... */
	else if (ui & USE_PARAMS)
		return 1;
	else if (uj & USE_PARAMS)
		return 0;
	/* Put other objects before that */
	else if (ui & USE_OTHER_OBJECTS)
		return 1;
	else if (uj & USE_OTHER_OBJECTS)
		return 0;
	/* Put shared objects before that... */
	else if (ui & USE_SHARED)
		return 1;
	else if (uj & USE_SHARED)
		return 0;
	/* And otherwise, order by the page number on which
	 * they are used. */
	return (ui>>USE_PAGE_SHIFT) >= (uj>>USE_PAGE_SHIFT);
}

static void
heap_sort(int *list, int n, const int *val, int (*ge)(int, int))
{
	int i, j;

#ifdef DEBUG_HEAP_SORT
	fprintf(stderr, "Initially:\n");
	for (i=0; i < n; i++)
	{
		fprintf(stderr, "%d: %d %x\n", i, list[i], val[list[i]]);
	}
#endif
	/* Step 1: Make a heap */
	/* Invariant: Valid heap in [0..i), unsorted elements in [i..n) */
	for (i = 1; i < n; i++)
	{
		/* Now bubble backwards to maintain heap invariant */
		j = i;
		while (j != 0)
		{
			int tmp;
			int k = (j-1)>>1;
			if (ge(val[list[k]], val[list[j]]))
				break;
			tmp = list[k];
			list[k] = list[j];
			list[j] = tmp;
			j = k;
		}
	}
#ifdef DEBUG_HEAP_SORT
	fprintf(stderr, "Valid heap:\n");
	for (i=0; i < n; i++)
	{
		int k;
		fprintf(stderr, "%d: %d %x ", i, list[i], val[list[i]]);
		k = (i+1)*2-1;
		if (k < n)
		{
			if (ge(val[list[i]], val[list[k]]))
				fprintf(stderr, "OK ");
			else
				fprintf(stderr, "BAD ");
		}
		if (k+1 < n)
		{
			if (ge(val[list[i]], val[list[k+1]]))
				fprintf(stderr, "OK\n");
			else
				fprintf(stderr, "BAD\n");
		}
		else
				fprintf(stderr, "\n");
	}
#endif

	/* Step 2: Heap sort */
	/* Invariant: valid heap in [0..i), sorted list in [i..n) */
	/* Initially: i = n */
	for (i = n-1; i > 0; i--)
	{
		/* Swap the maximum (0th) element from the page_objects into its place
		 * in the sorted list (position i). */
		int tmp = list[0];
		list[0] = list[i];
		list[i] = tmp;
		/* Now, the page_objects is invalid because the 0th element is out
		 * of place. Bubble it until the page_objects is valid. */
		j = 0;
		while (1)
		{
			/* Children are k and k+1 */
			int k = (j+1)*2-1;
			/* If both children out of the page_objects, we're done */
			if (k > i-1)
				break;
			/* If both are in the page_objects, pick the larger one */
			if (k < i-1 && ge(val[list[k+1]], val[list[k]]))
				k++;
			/* If j is bigger than k (i.e. both of its children),
			 * we're done */
			if (ge(val[list[j]], val[list[k]]))
				break;
			tmp = list[k];
			list[k] = list[j];
			list[j] = tmp;
			j = k;
		}
	}
#ifdef DEBUG_HEAP_SORT
	fprintf(stderr, "Sorted:\n");
	for (i=0; i < n; i++)
	{
		fprintf(stderr, "%d: %d %x ", i, list[i], val[list[i]]);
		if (i+1 < n)
		{
			if (ge(val[list[i+1]], val[list[i]]))
				fprintf(stderr, "OK");
			else
				fprintf(stderr, "BAD");
		}
		fprintf(stderr, "\n");
	}
#endif
}

static void
page_objects_dedupe(fz_context *ctx, page_objects *po)
{
	int i, j;
	int n = po->len-1;

	for (i = 0; i < n; i++)
	{
		if (po->object[i] == po->object[i+1])
			break;
	}
	j = i; /* j points to the last valid one */
	i++; /* i points to the first one we haven't looked at */
	for (; i < n; i++)
	{
		if (po->object[j] != po->object[i])
			po->object[++j] = po->object[i];
	}
	po->len = j+1;
}

static void
page_objects_list_sort_and_dedupe(fz_context *ctx, page_objects_list *pol)
{
	int i;
	int n = pol->len;

	for (i = 0; i < n; i++)
	{
		page_objects_sort(ctx, pol->page[i]);
		page_objects_dedupe(ctx, pol->page[i]);
	}
}

#ifdef DEBUG_LINEARIZATION
static void
page_objects_dump(pdf_write_state *opts)
{
	page_objects_list *pol = opts->page_object_lists;
	int i, j;

	for (i = 0; i < pol->len; i++)
	{
		page_objects *p = pol->page[i];
		fprintf(stderr, "Page %d\n", i+1);
		for (j = 0; j < p->len; j++)
		{
			int o = p->object[j];
			fprintf(stderr, "\tObject %d: use=%x\n", o, opts->use_list[o]);
		}
		fprintf(stderr, "Byte range=%d->%d\n", p->min_ofs, p->max_ofs);
		fprintf(stderr, "Number of objects=%d, Number of shared objects=%d\n", p->num_objects, p->num_shared);
		fprintf(stderr, "Page object number=%d\n", p->page_object_number);
	}
}

static void
objects_dump(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int i;

	for (i=0; i < pdf_xref_len(ctx, doc); i++)
	{
		fprintf(stderr, "Object %d use=%x offset=%d\n", i, opts->use_list[i], (int)opts->ofs_list[i]);
	}
}
#endif

/*
 * Garbage collect objects not reachable from the trailer.
 */

/* Mark a reference. If it's been marked already, return NULL (as no further
 * processing is required). If it's not, return the resolved object so
 * that we can continue our recursive marking. If it's a duff reference
 * return the fact so that we can remove the reference at source.
 */
static pdf_obj *markref(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj *obj, int *duff)
{
	int num = pdf_to_num(ctx, obj);
	int xref_len = pdf_xref_len(ctx, doc);

	if (num <= 0 || num >= xref_len)
	{
		*duff = 1;
		return NULL;
	}
	expand_lists(ctx, opts, xref_len);
	*duff = 0;
	if (opts->use_list[num])
		return NULL;

	opts->use_list[num] = 1;

	/* Bake in /Length in stream objects */
	fz_try(ctx)
	{
		if (pdf_obj_num_is_stream(ctx, doc, num))
		{
			pdf_obj *len = pdf_dict_get(ctx, obj, PDF_NAME(Length));
			if (pdf_is_indirect(ctx, len))
			{
				int num2 = pdf_to_num(ctx, len);
				expand_lists(ctx, opts, num2+1);
				opts->use_list[num2] = 0;
				len = pdf_resolve_indirect(ctx, len);
				pdf_dict_put(ctx, obj, PDF_NAME(Length), len);
			}
		}
	}
	fz_catch(ctx)
	{
		/* Leave broken */
	}

	obj = pdf_resolve_indirect(ctx, obj);
	if (obj == NULL || pdf_is_null(ctx, obj))
	{
		*duff = 1;
		opts->use_list[num] = 0;
	}

	return obj;
}

#ifdef DEBUG_MARK_AND_SWEEP
static int depth = 0;

static
void indent()
{
	while (depth > 0)
	{
		int d  = depth;
		if (d > 16)
			d = 16;
		printf("%s", &"                "[16-d]);
		depth -= d;
	}
}
#define DEBUGGING_MARKING(A) do { A; } while (0)
#else
#define DEBUGGING_MARKING(A) do { } while (0)
#endif

/* Recursively mark an object. If any references found are duff, then
 * replace them with nulls. */
static int markobj(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj *obj)
{
	int i;

	DEBUGGING_MARKING(depth++);

	while (pdf_is_indirect(ctx, obj))
	{
		int duff;
		DEBUGGING_MARKING(indent(); printf("Marking object %d\n", pdf_to_num(ctx, obj)));
		obj = markref(ctx, doc, opts, obj, &duff);
		if (duff)
		{
			DEBUGGING_MARKING(depth--);
			return 1;
		}
	}

	if (pdf_is_dict(ctx, obj))
	{
		int n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			DEBUGGING_MARKING(indent(); printf("DICT[%d/%d] = %s\n", i, n, pdf_to_name(ctx, pdf_dict_get_key(ctx, obj, i))));
			if (markobj(ctx, doc, opts, pdf_dict_get_val(ctx, obj, i)))
				pdf_dict_put_val_null(ctx, obj, i);
		}
	}

	else if (pdf_is_array(ctx, obj))
	{
		int n = pdf_array_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			DEBUGGING_MARKING(indent(); printf("ARRAY[%d/%d]\n", i, n));
			if (markobj(ctx, doc, opts, pdf_array_get(ctx, obj, i)))
				pdf_array_put(ctx, obj, i, PDF_NULL);
		}
	}

	DEBUGGING_MARKING(depth--);

	return 0;
}

/*
 * Scan for and remove duplicate objects (slow)
 */

static void removeduplicateobjs(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int num, other;
	int xref_len = pdf_xref_len(ctx, doc);

	expand_lists(ctx, opts, xref_len);
	for (num = 1; num < xref_len; num++)
	{
		/* Only compare an object to objects preceding it */
		for (other = 1; other < num; other++)
		{
			pdf_obj *a, *b;
			int newnum;

			if (num == other || num >= opts->list_len || !opts->use_list[num] || !opts->use_list[other])
				continue;

			/* TODO: resolve indirect references to see if we can omit them */

			a = pdf_get_xref_entry_no_null(ctx, doc, num)->obj;
			b = pdf_get_xref_entry_no_null(ctx, doc, other)->obj;
			if (opts->do_garbage >= 4)
			{
				if (pdf_objcmp_deep(ctx, a, b))
					continue;
			}
			else
			{
				if (pdf_objcmp(ctx, a, b))
					continue;
			}

			/* Keep the lowest numbered object */
			newnum = fz_mini(num, other);
			opts->renumber_map[num] = newnum;
			opts->renumber_map[other] = newnum;
			opts->rev_renumber_map[newnum] = num; /* Either will do */
			opts->use_list[fz_maxi(num, other)] = 0;

			/* One duplicate was found, do not look for another */
			break;
		}
	}
}

/*
 * Renumber objects sequentially so the xref is more compact
 *
 * This code assumes that any opts->renumber_map[n] <= n for all n.
 */

static void compactxref(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int num, newnum;
	int xref_len = pdf_xref_len(ctx, doc);

	/*
	 * Update renumber_map in-place, clustering all used
	 * objects together at low object ids. Objects that
	 * already should be renumbered will have their new
	 * object ids be updated to reflect the compaction.
	 */

	if (xref_len > opts->list_len)
		expand_lists(ctx, opts, xref_len-1);

	newnum = 1;
	for (num = 1; num < xref_len; num++)
	{
		/* If it's not used, map it to zero */
		if (!opts->use_list[opts->renumber_map[num]])
		{
			opts->renumber_map[num] = 0;
		}
		/* If it's not moved, compact it. */
		else if (opts->renumber_map[num] == num)
		{
			opts->rev_renumber_map[newnum] = opts->rev_renumber_map[num];
			opts->renumber_map[num] = newnum++;
		}
		/* Otherwise it's used, and moved. We know that it must have
		 * moved down, so the place it's moved to will be in the right
		 * place already. */
		else
		{
			opts->renumber_map[num] = opts->renumber_map[opts->renumber_map[num]];
		}
	}
}

/*
 * Update indirect objects according to renumbering established when
 * removing duplicate objects and compacting the xref.
 */

static void renumberobj(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj *obj)
{
	int i;
	int xref_len = pdf_xref_len(ctx, doc);

	if (pdf_is_dict(ctx, obj))
	{
		int n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, obj, i);
			pdf_obj *val = pdf_dict_get_val(ctx, obj, i);
			if (pdf_is_indirect(ctx, val))
			{
				int o = pdf_to_num(ctx, val);
				if (o >= xref_len || o <= 0 || opts->renumber_map[o] == 0)
					val = PDF_NULL;
				else
					val = pdf_new_indirect(ctx, doc, opts->renumber_map[o], 0);
				pdf_dict_put_drop(ctx, obj, key, val);
			}
			else
			{
				renumberobj(ctx, doc, opts, val);
			}
		}
	}

	else if (pdf_is_array(ctx, obj))
	{
		int n = pdf_array_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *val = pdf_array_get(ctx, obj, i);
			if (pdf_is_indirect(ctx, val))
			{
				int o = pdf_to_num(ctx, val);
				if (o >= xref_len || o <= 0 || opts->renumber_map[o] == 0)
					val = PDF_NULL;
				else
					val = pdf_new_indirect(ctx, doc, opts->renumber_map[o], 0);
				pdf_array_put_drop(ctx, obj, i, val);
			}
			else
			{
				renumberobj(ctx, doc, opts, val);
			}
		}
	}
}

static void renumberobjs(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	pdf_xref_entry *newxref = NULL;
	int newlen;
	int num;
	int *new_use_list;
	int xref_len = pdf_xref_len(ctx, doc);

	new_use_list = fz_calloc(ctx, pdf_xref_len(ctx, doc)+3, sizeof(int));

	fz_var(newxref);
	fz_try(ctx)
	{
		/* Apply renumber map to indirect references in all objects in xref */
		renumberobj(ctx, doc, opts, pdf_trailer(ctx, doc));
		for (num = 0; num < xref_len; num++)
		{
			pdf_obj *obj;
			int to = opts->renumber_map[num];

			/* If object is going to be dropped, don't bother renumbering */
			if (to == 0)
				continue;

			obj = pdf_get_xref_entry_no_null(ctx, doc, num)->obj;

			if (pdf_is_indirect(ctx, obj))
			{
				obj = pdf_new_indirect(ctx, doc, to, 0);
				fz_try(ctx)
					pdf_update_object(ctx, doc, num, obj);
				fz_always(ctx)
					pdf_drop_obj(ctx, obj);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
			else
			{
				renumberobj(ctx, doc, opts, obj);
			}
		}

		/* Create new table for the reordered, compacted xref */
		newxref = Memento_label(fz_malloc_array(ctx, xref_len + 3, pdf_xref_entry), "pdf_xref_entries");
		newxref[0] = *pdf_get_xref_entry_no_null(ctx, doc, 0);

		/* Move used objects into the new compacted xref */
		newlen = 0;
		for (num = 1; num < xref_len; num++)
		{
			if (opts->use_list[num])
			{
				pdf_xref_entry *e;
				if (newlen < opts->renumber_map[num])
					newlen = opts->renumber_map[num];
				e = pdf_get_xref_entry_no_null(ctx, doc, num);
				newxref[opts->renumber_map[num]] = *e;
				if (e->obj)
					pdf_set_obj_parent(ctx, e->obj, opts->renumber_map[num]);
				e->obj = NULL;
				e->stm_buf = NULL;
				new_use_list[opts->renumber_map[num]] = opts->use_list[num];
			}
			else
			{
				pdf_xref_entry *e = pdf_get_xref_entry_no_null(ctx, doc, num);
				pdf_drop_obj(ctx, e->obj);
				e->obj = NULL;
				fz_drop_buffer(ctx, e->stm_buf);
				e->stm_buf = NULL;
			}
		}

		pdf_replace_xref(ctx, doc, newxref, newlen + 1);
		newxref = NULL;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, newxref);
		fz_free(ctx, new_use_list);
		fz_rethrow(ctx);
	}
	fz_free(ctx, opts->use_list);
	opts->use_list = new_use_list;

	for (num = 1; num < xref_len; num++)
	{
		opts->renumber_map[num] = num;
	}
}

static void page_objects_list_renumber(pdf_write_state *opts)
{
	int i, j;

	for (i = 0; i < opts->page_object_lists->len; i++)
	{
		page_objects *po = opts->page_object_lists->page[i];
		for (j = 0; j < po->len; j++)
		{
			po->object[j] = opts->renumber_map[po->object[j]];
		}
		po->page_object_number = opts->renumber_map[po->page_object_number];
	}
}

static void
swap_indirect_obj(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj **obj)
{
	pdf_obj *o = pdf_new_indirect(ctx, doc, opts->renumber_map[pdf_to_num(ctx, *obj)], 0);

	pdf_drop_obj(ctx, *obj);
	*obj = o;
}

static void
renumber_pages(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	fz_page *page;

	for (page = doc->super.open; page != NULL; page = page->next)
	{
		pdf_page *ppage = (pdf_page *)page;
		pdf_annot *annot;
		swap_indirect_obj(ctx, doc, opts, &ppage->obj);

		for (annot = ppage->annots; annot != NULL; annot = annot->next)
			swap_indirect_obj(ctx, doc, opts, &annot->obj);
		for (annot = ppage->widgets; annot != NULL; annot = annot->next)
			swap_indirect_obj(ctx, doc, opts, &annot->obj);
	}
}

static void
mark_all(fz_context *ctx, pdf_document *doc, pdf_mark_list *list, pdf_write_state *opts, pdf_obj *val, int flag, int page)
{
	if (pdf_mark_list_push(ctx, list, val))
		return;

	if (pdf_is_indirect(ctx, val))
	{
		int num = pdf_to_num(ctx, val);
		int bits = flag;
		if (num >= opts->list_len)
			expand_lists(ctx, opts, num);
		if (page >= 0)
			page_objects_list_insert(ctx, opts, page, num);
		if (opts->use_list[num] & USE_PAGE_MASK)
			/* Already used */
			bits = USE_SHARED;
		if ((opts->use_list[num] | bits) == opts->use_list[num])
		{
			/* Been here already */
			pdf_mark_list_pop(ctx, list);
			return;
		}
		opts->use_list[num] |= bits;
	}

	if (pdf_is_dict(ctx, val))
	{
		int i, n;
		n = pdf_dict_len(ctx, val);

		for (i = 0; i < n; i++)
		{
			pdf_obj *v = pdf_dict_get_val(ctx, val, i);
			pdf_obj *type = pdf_dict_get(ctx, v, PDF_NAME(Type));

			/* Don't walk through the Page tree, or direct to a page. */
			if (pdf_name_eq(ctx, PDF_NAME(Pages), type) || pdf_name_eq(ctx, PDF_NAME(Page), type))
				continue;

			mark_all(ctx, doc, list, opts, v, flag, page);
		}
	}
	else if (pdf_is_array(ctx, val))
	{
		int i, n = pdf_array_len(ctx, val);

		for (i = 0; i < n; i++)
		{
			pdf_obj *v = pdf_array_get(ctx, val, i);
			pdf_obj *type = pdf_dict_get(ctx, v, PDF_NAME(Type));

			/* Don't walk through the Page tree, or direct to a page. */
			if (pdf_name_eq(ctx, PDF_NAME(Pages), type) || pdf_name_eq(ctx, PDF_NAME(Page), type))
				continue;

			mark_all(ctx, doc, list, opts, v, flag, page);
		}
	}
	pdf_mark_list_pop(ctx, list);
}

static int
mark_pages(fz_context *ctx, pdf_document *doc, pdf_mark_list *list, pdf_write_state *opts, pdf_obj *val, int pagenum)
{
	if (pdf_mark_list_push(ctx, list, val))
		return pagenum;

	if (pdf_is_dict(ctx, val))
	{
		if (pdf_name_eq(ctx, PDF_NAME(Page), pdf_dict_get(ctx, val, PDF_NAME(Type))))
		{
			int num = pdf_to_num(ctx, val);
			pdf_mark_list_pop(ctx, list);

			mark_all(ctx, doc, list, opts, val, pagenum == 0 ? USE_PAGE1 : (pagenum<<USE_PAGE_SHIFT), pagenum);
			page_objects_list_set_page_object(ctx, opts, pagenum, num);
			pagenum++;
			opts->use_list[num] |= USE_PAGE_OBJECT;
			return pagenum;
		}
		else
		{
			int i, n = pdf_dict_len(ctx, val);

			for (i = 0; i < n; i++)
			{
				pdf_obj *key = pdf_dict_get_key(ctx, val, i);
				pdf_obj *obj = pdf_dict_get_val(ctx, val, i);

				if (pdf_name_eq(ctx, PDF_NAME(Kids), key))
					pagenum = mark_pages(ctx, doc, list, opts, obj, pagenum);
				else
					mark_all(ctx, doc, list, opts, obj, USE_CATALOGUE, -1);
			}

			if (pdf_is_indirect(ctx, val))
			{
				int num = pdf_to_num(ctx, val);
				opts->use_list[num] |= USE_CATALOGUE;
			}
		}
	}
	else if (pdf_is_array(ctx, val))
	{
		int i, n = pdf_array_len(ctx, val);

		for (i = 0; i < n; i++)
		{
			pagenum = mark_pages(ctx, doc, list, opts, pdf_array_get(ctx, val, i), pagenum);
		}
		if (pdf_is_indirect(ctx, val))
		{
			int num = pdf_to_num(ctx, val);
			opts->use_list[num] |= USE_CATALOGUE;
		}
	}
	pdf_mark_list_pop(ctx, list);

	return pagenum;
}

static void
mark_root(fz_context *ctx, pdf_document *doc, pdf_mark_list *list, pdf_write_state *opts, pdf_obj *dict)
{
	int i, n = pdf_dict_len(ctx, dict);

	if (pdf_mark_list_push(ctx, list, dict))
		return;

	if (pdf_is_indirect(ctx, dict))
	{
		int num = pdf_to_num(ctx, dict);
		opts->use_list[num] |= USE_CATALOGUE;
	}

	for (i = 0; i < n; i++)
	{
		pdf_obj *key = pdf_dict_get_key(ctx, dict, i);
		pdf_obj *val = pdf_dict_get_val(ctx, dict, i);

		if (pdf_name_eq(ctx, PDF_NAME(Pages), key))
			opts->page_count = mark_pages(ctx, doc, list, opts, val, 0);
		else if (pdf_name_eq(ctx, PDF_NAME(Names), key))
			mark_all(ctx, doc, list, opts, val, USE_OTHER_OBJECTS, -1);
		else if (pdf_name_eq(ctx, PDF_NAME(Dests), key))
			mark_all(ctx, doc, list, opts, val, USE_OTHER_OBJECTS, -1);
		else if (pdf_name_eq(ctx, PDF_NAME(Outlines), key))
		{
			int section;
			/* Look at PageMode to decide whether to
			 * USE_OTHER_OBJECTS or USE_PAGE1 here. */
			if (pdf_name_eq(ctx, pdf_dict_get(ctx, dict, PDF_NAME(PageMode)), PDF_NAME(UseOutlines)))
				section = USE_PAGE1;
			else
				section = USE_OTHER_OBJECTS;
			mark_all(ctx, doc, list, opts, val, section, -1);
		}
		else
			mark_all(ctx, doc, list, opts, val, USE_CATALOGUE, -1);
	}
	pdf_mark_list_pop(ctx, list);
}

static void
mark_trailer(fz_context *ctx, pdf_document *doc, pdf_mark_list *list, pdf_write_state *opts, pdf_obj *dict)
{
	int i, n = pdf_dict_len(ctx, dict);

	if (pdf_mark_list_push(ctx, list, dict))
		return;

	for (i = 0; i < n; i++)
	{
		pdf_obj *key = pdf_dict_get_key(ctx, dict, i);
		pdf_obj *val = pdf_dict_get_val(ctx, dict, i);

		if (pdf_name_eq(ctx, PDF_NAME(Root), key))
			mark_root(ctx, doc, list, opts, val);
		else
			mark_all(ctx, doc, list, opts, val, USE_CATALOGUE, -1);
	}
	pdf_mark_list_pop(ctx, list);
}

static void
add_linearization_objs(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	pdf_obj *params_obj = NULL;
	pdf_obj *params_ref = NULL;
	pdf_obj *hint_obj = NULL;
	pdf_obj *hint_ref = NULL;
	pdf_obj *o;
	int params_num, hint_num;

	fz_var(params_obj);
	fz_var(params_ref);
	fz_var(hint_obj);
	fz_var(hint_ref);

	fz_try(ctx)
	{
		pdf_xref_entry *xe;

		/* Linearization params */
		params_obj = pdf_new_dict(ctx, doc, 10);
		params_ref = pdf_add_object(ctx, doc, params_obj);
		params_num = pdf_to_num(ctx, params_ref);

		opts->use_list[params_num] = USE_PARAMS;
		opts->renumber_map[params_num] = params_num;
		opts->rev_renumber_map[params_num] = params_num;
		opts->gen_list[params_num] = 0;
		pdf_dict_put_real(ctx, params_obj, PDF_NAME(Linearized), 1.0f);
		opts->linear_l = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, params_obj, PDF_NAME(L), opts->linear_l);
		opts->linear_h0 = pdf_new_int(ctx, INT_MIN);
		o = pdf_dict_put_array(ctx, params_obj, PDF_NAME(H), 2);
		pdf_array_push(ctx, o, opts->linear_h0);
		opts->linear_h1 = pdf_new_int(ctx, INT_MIN);
		pdf_array_push(ctx, o, opts->linear_h1);
		opts->linear_o = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, params_obj, PDF_NAME(O), opts->linear_o);
		opts->linear_e = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, params_obj, PDF_NAME(E), opts->linear_e);
		opts->linear_n = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, params_obj, PDF_NAME(N), opts->linear_n);
		opts->linear_t = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, params_obj, PDF_NAME(T), opts->linear_t);

		/* Primary hint stream */
		hint_obj = pdf_new_dict(ctx, doc, 10);
		hint_ref = pdf_add_object(ctx, doc, hint_obj);
		hint_num = pdf_to_num(ctx, hint_ref);

		opts->hint_object_num = hint_num;
		opts->use_list[hint_num] = USE_HINTS;
		opts->renumber_map[hint_num] = hint_num;
		opts->rev_renumber_map[hint_num] = hint_num;
		opts->gen_list[hint_num] = 0;
		pdf_dict_put_int(ctx, hint_obj, PDF_NAME(P), 0);
		opts->hints_s = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, hint_obj, PDF_NAME(S), opts->hints_s);
		/* FIXME: Do we have thumbnails? Do a T entry */
		/* FIXME: Do we have outlines? Do an O entry */
		/* FIXME: Do we have article threads? Do an A entry */
		/* FIXME: Do we have named destinations? Do a E entry */
		/* FIXME: Do we have interactive forms? Do a V entry */
		/* FIXME: Do we have document information? Do an I entry */
		/* FIXME: Do we have logical structure hierarchy? Do a C entry */
		/* FIXME: Do L, Page Label hint table */
		pdf_dict_put(ctx, hint_obj, PDF_NAME(Filter), PDF_NAME(FlateDecode));
		opts->hints_length = pdf_new_int(ctx, INT_MIN);
		pdf_dict_put(ctx, hint_obj, PDF_NAME(Length), opts->hints_length);
		xe = pdf_get_xref_entry_no_null(ctx, doc, hint_num);
		xe->stm_ofs = 0;
		/* Empty stream, required so that we write the object as
		 * a stream during the first pass. Without this, offsets
		 * for the xref will be wrong. */
		xe->stm_buf = fz_new_buffer(ctx, 1);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, params_obj);
		pdf_drop_obj(ctx, params_ref);
		pdf_drop_obj(ctx, hint_ref);
		pdf_drop_obj(ctx, hint_obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
lpr_inherit_res_contents(fz_context *ctx, pdf_obj *res, pdf_obj *dict, pdf_obj *text)
{
	pdf_obj *o, *r;
	int i, n;

	/* If the parent node doesn't have an entry of this type, give up. */
	o = pdf_dict_get(ctx, dict, text);
	if (!o)
		return;

	/* If the resources dict we are building doesn't have an entry of this
	 * type yet, then just copy it (ensuring it's not a reference) */
	r = pdf_dict_get(ctx, res, text);
	if (r == NULL)
	{
		o = pdf_resolve_indirect(ctx, o);
		if (pdf_is_dict(ctx, o))
			o = pdf_copy_dict(ctx, o);
		else if (pdf_is_array(ctx, o))
			o = pdf_copy_array(ctx, o);
		else
			o = NULL;
		if (o)
			pdf_dict_put_drop(ctx, res, text, o);
		return;
	}

	/* Otherwise we need to merge o into r */
	if (pdf_is_dict(ctx, o))
	{
		n = pdf_dict_len(ctx, o);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, o, i);
			pdf_obj *val = pdf_dict_get_val(ctx, o, i);

			if (pdf_dict_get(ctx, r, key))
				continue;
			pdf_dict_put(ctx, r, key, val);
		}
	}
}

static void
lpr_inherit_res(fz_context *ctx, pdf_obj *node, int depth, pdf_obj *dict)
{
	while (1)
	{
		pdf_obj *o;

		node = pdf_dict_get(ctx, node, PDF_NAME(Parent));
		depth--;
		if (!node || depth < 0)
			break;

		o = pdf_dict_get(ctx, node, PDF_NAME(Resources));
		if (o)
		{
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(ExtGState));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(ColorSpace));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(Pattern));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(Shading));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(XObject));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(Font));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(ProcSet));
			lpr_inherit_res_contents(ctx, dict, o, PDF_NAME(Properties));
		}
	}
}

static pdf_obj *
lpr_inherit(fz_context *ctx, pdf_obj *node, char *text, int depth)
{
	do
	{
		pdf_obj *o = pdf_dict_gets(ctx, node, text);

		if (o)
			return pdf_resolve_indirect(ctx, o);
		node = pdf_dict_get(ctx, node, PDF_NAME(Parent));
		depth--;
	}
	while (depth >= 0 && node);

	return NULL;
}

static int
lpr(fz_context *ctx, pdf_document *doc, pdf_mark_list *list, pdf_obj *node, int depth, int page)
{
	pdf_obj *kids;
	pdf_obj *o = NULL;
	int i, n;

	if (pdf_mark_list_push(ctx, list, node))
		return page;

	fz_var(o);

	fz_try(ctx)
	{
		if (pdf_name_eq(ctx, PDF_NAME(Page), pdf_dict_get(ctx, node, PDF_NAME(Type))))
		{
			pdf_obj *r; /* r is deliberately not cleaned up */

			/* Copy resources down to the child */
			o = pdf_keep_obj(ctx, pdf_dict_get(ctx, node, PDF_NAME(Resources)));
			if (!o)
			{
				o = pdf_keep_obj(ctx, pdf_new_dict(ctx, doc, 2));
				pdf_dict_put(ctx, node, PDF_NAME(Resources), o);
			}
			lpr_inherit_res(ctx, node, depth, o);
			r = lpr_inherit(ctx, node, "MediaBox", depth);
			if (r)
				pdf_dict_put(ctx, node, PDF_NAME(MediaBox), r);
			r = lpr_inherit(ctx, node, "CropBox", depth);
			if (r)
				pdf_dict_put(ctx, node, PDF_NAME(CropBox), r);
			r = lpr_inherit(ctx, node, "BleedBox", depth);
			if (r)
				pdf_dict_put(ctx, node, PDF_NAME(BleedBox), r);
			r = lpr_inherit(ctx, node, "TrimBox", depth);
			if (r)
				pdf_dict_put(ctx, node, PDF_NAME(TrimBox), r);
			r = lpr_inherit(ctx, node, "ArtBox", depth);
			if (r)
				pdf_dict_put(ctx, node, PDF_NAME(ArtBox), r);
			r = lpr_inherit(ctx, node, "Rotate", depth);
			if (r)
				pdf_dict_put(ctx, node, PDF_NAME(Rotate), r);
			page++;
		}
		else
		{
			kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
			n = pdf_array_len(ctx, kids);
			for(i = 0; i < n; i++)
			{
				page = lpr(ctx, doc, list, pdf_array_get(ctx, kids, i), depth+1, page);
			}
			pdf_dict_del(ctx, node, PDF_NAME(Resources));
			pdf_dict_del(ctx, node, PDF_NAME(MediaBox));
			pdf_dict_del(ctx, node, PDF_NAME(CropBox));
			pdf_dict_del(ctx, node, PDF_NAME(BleedBox));
			pdf_dict_del(ctx, node, PDF_NAME(TrimBox));
			pdf_dict_del(ctx, node, PDF_NAME(ArtBox));
			pdf_dict_del(ctx, node, PDF_NAME(Rotate));
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, o);
	fz_catch(ctx)
		fz_rethrow(ctx);

	pdf_mark_list_pop(ctx, list);

	return page;
}

static void
pdf_localise_page_resources(fz_context *ctx, pdf_document *doc, pdf_mark_list *list)
{
	if (doc->resources_localised)
		return;

	lpr(ctx, doc, list, pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), PDF_NAME(Pages), NULL), 0, 0);

	doc->resources_localised = 1;
}

static void
linearize(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int i;
	int n = pdf_xref_len(ctx, doc) + 2;
	int *reorder;
	int *rev_renumber_map;
	pdf_mark_list list;

	pdf_mark_list_init(ctx, &list);
	opts->page_object_lists = page_objects_list_create(ctx);

	/* Ensure that every page has local references of its resources */
	fz_try(ctx)
	{
		/* FIXME: We could 'thin' the resources according to what is actually
		 * required for each page, but this would require us to run the page
		 * content streams. */
		pdf_localise_page_resources(ctx, doc, &list);

		/* Walk the objects for each page, marking which ones are used, where */
		memset(opts->use_list, 0, n * sizeof(int));
		mark_trailer(ctx, doc, &list, opts, pdf_trailer(ctx, doc));
	}
	fz_always(ctx)
		pdf_mark_list_free(ctx, &list);
	fz_catch(ctx)
		fz_rethrow(ctx);

	/* Add new objects required for linearization */
	add_linearization_objs(ctx, doc, opts);

#ifdef DEBUG_WRITING
	fprintf(stderr, "Usage calculated:\n");
	for (i=0; i < pdf_xref_len(ctx, doc); i++)
	{
		fprintf(stderr, "%d: use=%d\n", i, opts->use_list[i]);
	}
#endif

	/* Allocate/init the structures used for renumbering the objects */
	reorder = fz_calloc(ctx, n, sizeof(int));
	rev_renumber_map = fz_calloc(ctx, n, sizeof(int));
	for (i = 0; i < n; i++)
	{
		reorder[i] = i;
	}

	/* Heap sort the reordering */
	heap_sort(reorder+1, n-1, opts->use_list, &order_ge);

#ifdef DEBUG_WRITING
	fprintf(stderr, "Reordered:\n");
	for (i=1; i < pdf_xref_len(ctx, doc); i++)
	{
		fprintf(stderr, "%d: use=%d\n", i, opts->use_list[reorder[i]]);
	}
#endif

	/* Find the split point */
	for (i = 1; (opts->use_list[reorder[i]] & USE_PARAMS) == 0; i++) {}
	opts->start = i;

	/* Roll the reordering into the renumber_map */
	for (i = 0; i < n; i++)
	{
		opts->renumber_map[reorder[i]] = i;
		rev_renumber_map[i] = opts->rev_renumber_map[reorder[i]];
	}
	fz_free(ctx, opts->rev_renumber_map);
	opts->rev_renumber_map = rev_renumber_map;
	fz_free(ctx, reorder);

	/* Apply the renumber_map */
	page_objects_list_renumber(opts);
	renumberobjs(ctx, doc, opts);
	renumber_pages(ctx, doc, opts);

	page_objects_list_sort_and_dedupe(ctx, opts->page_object_lists);
}

static void
update_linearization_params(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int64_t offset;
	pdf_set_int(ctx, opts->linear_l, opts->file_len);
	/* Primary hint stream offset (of object, not stream!) */
	pdf_set_int(ctx, opts->linear_h0, opts->ofs_list[pdf_xref_len(ctx, doc)-1]);
	/* Primary hint stream length (of object, not stream!) */
	offset = (opts->start == 1 ? opts->main_xref_offset : opts->ofs_list[1] + opts->hintstream_len);
	pdf_set_int(ctx, opts->linear_h1, offset - opts->ofs_list[pdf_xref_len(ctx, doc)-1]);
	/* Object number of first pages page object (the first object of page 0) */
	pdf_set_int(ctx, opts->linear_o, opts->page_object_lists->page[0]->object[0]);
	/* Offset of end of first page (first page is followed by primary
	 * hint stream (object n-1) then remaining pages (object 1...). The
	 * primary hint stream counts as part of the first pages data, I think.
	 */
	offset = (opts->start == 1 ? opts->main_xref_offset : opts->ofs_list[1] + opts->hintstream_len);
	pdf_set_int(ctx, opts->linear_e, offset);
	/* Number of pages in document */
	pdf_set_int(ctx, opts->linear_n, opts->page_count);
	/* Offset of first entry in main xref table */
	pdf_set_int(ctx, opts->linear_t, opts->first_xref_entry_offset + opts->hintstream_len);
	/* Offset of shared objects hint table in the primary hint stream */
	pdf_set_int(ctx, opts->hints_s, opts->hints_shared_offset);
	/* Primary hint stream length */
	pdf_set_int(ctx, opts->hints_length, opts->hintstream_len);
}

/*
 * Make sure we have loaded objects from object streams.
 */

static void preloadobjstms(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *obj;
	int num;
	pdf_xref_entry *x = NULL;
	int load = 1;

	/* If we have attempted a repair, then everything will have been
	 * loaded already. */
	if (doc->repair_attempted)
	{
		/* Bug 707112: But we do need to mark all our 'o' objects as being something else. */
		load = 0;
	}

	fz_var(num);
	fz_var(x);

	/* xref_len may change due to repair, so check it every iteration */
	for (num = 0; num < pdf_xref_len(ctx, doc); num++)
	{
		fz_try(ctx)
		{
			for (; num < pdf_xref_len(ctx, doc); num++)
			{
				x = pdf_get_xref_entry_no_null(ctx, doc, num);
				if (x->type == 'o')
				{
					if (load)
					{
						obj = pdf_load_object(ctx, doc, num);
						pdf_drop_obj(ctx, obj);
					}
					/* The object is no longer an objstm one. It's a regular object
					 * held in memory. Previously we used gen to hold the index of
					 * the obj in the objstm, so reset this to 0. */
					x->type = 'n';
					x->gen = 0;
				}
				x = NULL;
			}
		}
		fz_catch(ctx)
		{
			/* We need to clear the type even in the event of an error, lest we
			 * hit an assert later. Bug 707110. */
			if (x && x->type == 'o')
			{
				x->type = 'f';
				x->gen = 0;
			}
			/* Ignore the error, so we can carry on trying to load. */
			fz_warn(ctx, "%s", fz_caught_message(ctx));
		}
	}
}

/*
 * Save streams and objects to the output
 */

static int is_bitmap_stream(fz_context *ctx, pdf_obj *obj, size_t len, int *w, int *h)
{
	pdf_obj *bpc;
	pdf_obj *cs;
	int stride;
	if (pdf_dict_get(ctx, obj, PDF_NAME(Subtype)) != PDF_NAME(Image))
		return 0;
	*w = pdf_dict_get_int(ctx, obj, PDF_NAME(Width));
	*h = pdf_dict_get_int(ctx, obj, PDF_NAME(Height));
	stride = (*w + 7) >> 3;
	if ((size_t)stride * (*h) != len)
		return 0;
	if (pdf_dict_get_bool(ctx, obj, PDF_NAME(ImageMask)))
	{
		return 1;
	}
	else
	{
		bpc = pdf_dict_get(ctx, obj, PDF_NAME(BitsPerComponent));
		if (!pdf_is_int(ctx, bpc))
			return 0;
		if (pdf_to_int(ctx, bpc) != 1)
			return 0;
		cs = pdf_dict_get(ctx, obj, PDF_NAME(ColorSpace));
		if (!pdf_name_eq(ctx, cs, PDF_NAME(DeviceGray)))
			return 0;
		return 1;
	}
}

static inline int isbinary(int c)
{
	if (c == '\n' || c == '\r' || c == '\t')
		return 0;
	return c < 32 || c > 127;
}

static int isbinarystream(fz_context *ctx, const unsigned char *data, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		if (isbinary(data[i]))
			return 1;
	return 0;
}

static fz_buffer *hexbuf(fz_context *ctx, const unsigned char *p, size_t n)
{
	static const char hex[17] = "0123456789abcdef";
	int x = 0;
	size_t len = n * 2 + (n / 32) + 1;
	unsigned char *data = Memento_label(fz_malloc(ctx, len), "hexbuf");
	fz_buffer *buf = fz_new_buffer_from_data(ctx, data, len);

	while (n--)
	{
		*data++ = hex[*p >> 4];
		*data++ = hex[*p & 15];
		if (++x == 32)
		{
			*data++ = '\n';
			x = 0;
		}
		p++;
	}

	*data++ = '>';

	return buf;
}

static void addhexfilter(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *f, *dp, *newf, *newdp;

	newf = newdp = NULL;
	f = pdf_dict_get(ctx, dict, PDF_NAME(Filter));
	dp = pdf_dict_get(ctx, dict, PDF_NAME(DecodeParms));

	fz_var(newf);
	fz_var(newdp);

	fz_try(ctx)
	{
		if (pdf_is_name(ctx, f))
		{
			newf = pdf_new_array(ctx, doc, 2);
			pdf_array_push(ctx, newf, PDF_NAME(ASCIIHexDecode));
			pdf_array_push(ctx, newf, f);
			f = newf;
			if (pdf_is_dict(ctx, dp))
			{
				newdp = pdf_new_array(ctx, doc, 2);
				pdf_array_push(ctx, newdp, PDF_NULL);
				pdf_array_push(ctx, newdp, dp);
				dp = newdp;
			}
		}
		else if (pdf_is_array(ctx, f))
		{
			pdf_array_insert(ctx, f, PDF_NAME(ASCIIHexDecode), 0);
			if (pdf_is_array(ctx, dp))
				pdf_array_insert(ctx, dp, PDF_NULL, 0);
		}
		else
			f = PDF_NAME(ASCIIHexDecode);

		pdf_dict_put(ctx, dict, PDF_NAME(Filter), f);
		if (dp)
			pdf_dict_put(ctx, dict, PDF_NAME(DecodeParms), dp);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, newf);
		pdf_drop_obj(ctx, newdp);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static fz_buffer *deflatebuf(fz_context *ctx, const unsigned char *p, size_t n, int effort)
{
	fz_buffer *buf;
	uLongf csize;
	int t;
	uLong longN = (uLong)n;
	unsigned char *data;
	size_t cap;
	int mode;

	if (n != (size_t)longN)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Buffer too large to deflate");

	cap = compressBound(longN);
	data = Memento_label(fz_malloc(ctx, cap), "pdf_write_deflate");
	buf = fz_new_buffer_from_data(ctx, data, cap);
	csize = (uLongf)cap;
	if (effort == 0)
		mode = Z_DEFAULT_COMPRESSION;
	else
		mode = effort * Z_BEST_COMPRESSION / 100;
	t = compress2(data, &csize, p, longN, mode);
	if (t != Z_OK)
	{
		fz_drop_buffer(ctx, buf);
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot deflate buffer");
	}
	fz_try(ctx)
		fz_resize_buffer(ctx, buf, csize);
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
	return buf;
}

static int striphexfilter(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *f, *dp;
	int is_hex = 0;

	f = pdf_dict_get(ctx, dict, PDF_NAME(Filter));
	dp = pdf_dict_get(ctx, dict, PDF_NAME(DecodeParms));

	if (pdf_is_array(ctx, f))
	{
		/* Remove ASCIIHexDecode from head of filter list */
		if (pdf_array_get(ctx, f, 0) == PDF_NAME(ASCIIHexDecode))
		{
			is_hex = 1;
			pdf_array_delete(ctx, f, 0);
			if (pdf_is_array(ctx, dp))
				pdf_array_delete(ctx, dp, 0);
		}
		/* Unpack array if only one filter remains */
		if (pdf_array_len(ctx, f) == 1)
		{
			f = pdf_array_get(ctx, f, 0);
			pdf_dict_put(ctx, dict, PDF_NAME(Filter), f);
			if (dp)
			{
				dp = pdf_array_get(ctx, dp, 0);
				pdf_dict_put(ctx, dict, PDF_NAME(DecodeParms), dp);
			}
		}
		/* Remove array if no filters remain */
		else if (pdf_array_len(ctx, f) == 0)
		{
			pdf_dict_del(ctx, dict, PDF_NAME(Filter));
			pdf_dict_del(ctx, dict, PDF_NAME(DecodeParms));
		}
	}
	else if (f == PDF_NAME(ASCIIHexDecode))
	{
		is_hex = 1;
		pdf_dict_del(ctx, dict, PDF_NAME(Filter));
		pdf_dict_del(ctx, dict, PDF_NAME(DecodeParms));
	}

	return is_hex;
}

static fz_buffer *unhexbuf(fz_context *ctx, const unsigned char *p, size_t n)
{
	fz_stream *mstm = NULL;
	fz_stream *xstm = NULL;
	fz_buffer *out = NULL;
	fz_var(mstm);
	fz_var(xstm);
	fz_try(ctx)
	{
		mstm = fz_open_memory(ctx, p, n);
		xstm = fz_open_ahxd(ctx, mstm);
		out = fz_read_all(ctx, xstm, n/2);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, xstm);
		fz_drop_stream(ctx, mstm);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
	return out;
}

static void write_data(fz_context *ctx, void *arg, const unsigned char *data, size_t len)
{
	fz_write_data(ctx, (fz_output *)arg, data, len);
}

static void copystream(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj *obj_orig, int num, int gen, int do_deflate, int unenc)
{
	fz_buffer *tmp_unhex = NULL, *tmp_comp = NULL, *tmp_hex = NULL, *buf = NULL;
	pdf_obj *obj = NULL;
	pdf_obj *dp;
	size_t len;
	unsigned char *data;
	int w, h;

	fz_var(buf);
	fz_var(tmp_comp);
	fz_var(tmp_hex);
	fz_var(obj);

	fz_try(ctx)
	{
		buf = pdf_load_raw_stream_number(ctx, doc, num);
		obj = pdf_copy_dict(ctx, obj_orig);

		len = fz_buffer_storage(ctx, buf, &data);

		if (do_deflate && striphexfilter(ctx, doc, obj))
		{
			tmp_unhex = unhexbuf(ctx, data, len);
			len = fz_buffer_storage(ctx, tmp_unhex, &data);
		}

		if (do_deflate && !pdf_dict_get(ctx, obj, PDF_NAME(Filter)))
		{
			if (is_bitmap_stream(ctx, obj, len, &w, &h))
			{
				tmp_comp = fz_compress_ccitt_fax_g4(ctx, data, w, h, (w+7)>>3);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(CCITTFaxDecode));
				dp = pdf_dict_put_dict(ctx, obj, PDF_NAME(DecodeParms), 1);
				pdf_dict_put_int(ctx, dp, PDF_NAME(K), -1);
				pdf_dict_put_int(ctx, dp, PDF_NAME(Columns), w);
			}
			else
			{
				tmp_comp = deflatebuf(ctx, data, len, opts->compression_effort);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(FlateDecode));
			}
			len = fz_buffer_storage(ctx, tmp_comp, &data);
		}

		if (opts->do_ascii && isbinarystream(ctx, data, len))
		{
			tmp_hex = hexbuf(ctx, data, len);
			len = fz_buffer_storage(ctx, tmp_hex, &data);
			addhexfilter(ctx, doc, obj);
		}

		fz_write_printf(ctx, opts->out, "%d %d obj\n", num, gen);

		if (unenc)
		{
			pdf_dict_put_int(ctx, obj, PDF_NAME(Length), len);
			pdf_print_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii);
			fz_write_string(ctx, opts->out, "\nstream\n");
			fz_write_data(ctx, opts->out, data, len);
		}
		else
		{
			pdf_dict_put_int(ctx, obj, PDF_NAME(Length), pdf_encrypted_len(ctx, opts->crypt, num, gen, len));
			pdf_print_encrypted_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii, opts->crypt, num, gen);
			fz_write_string(ctx, opts->out, "\nstream\n");
			pdf_encrypt_data(ctx, opts->crypt, num, gen, write_data, opts->out, data, len);
		}

		fz_write_string(ctx, opts->out, "\nendstream\nendobj\n\n");
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, tmp_unhex);
		fz_drop_buffer(ctx, tmp_hex);
		fz_drop_buffer(ctx, tmp_comp);
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void expandstream(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj *obj_orig, int num, int gen, int do_deflate, int unenc)
{
	fz_buffer *buf = NULL, *tmp_comp = NULL, *tmp_hex = NULL;
	pdf_obj *obj = NULL;
	pdf_obj *dp;
	size_t len;
	unsigned char *data;
	int w, h;

	fz_var(buf);
	fz_var(tmp_comp);
	fz_var(tmp_hex);
	fz_var(obj);

	fz_try(ctx)
	{
		buf = pdf_load_stream_number(ctx, doc, num);
		obj = pdf_copy_dict(ctx, obj_orig);
		pdf_dict_del(ctx, obj, PDF_NAME(Filter));
		pdf_dict_del(ctx, obj, PDF_NAME(DecodeParms));

		len = fz_buffer_storage(ctx, buf, &data);
		if (do_deflate)
		{
			if (is_bitmap_stream(ctx, obj, len, &w, &h))
			{
				tmp_comp = fz_compress_ccitt_fax_g4(ctx, data, w, h, (w+7)>>3);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(CCITTFaxDecode));
				dp = pdf_dict_put_dict(ctx, obj, PDF_NAME(DecodeParms), 1);
				pdf_dict_put_int(ctx, dp, PDF_NAME(K), -1);
				pdf_dict_put_int(ctx, dp, PDF_NAME(Columns), w);
			}
			else
			{
				tmp_comp = deflatebuf(ctx, data, len, opts->compression_effort);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(FlateDecode));
			}
			len = fz_buffer_storage(ctx, tmp_comp, &data);
		}

		if (opts->do_ascii && isbinarystream(ctx, data, len))
		{
			tmp_hex = hexbuf(ctx, data, len);
			len = fz_buffer_storage(ctx, tmp_hex, &data);
			addhexfilter(ctx, doc, obj);
		}

		fz_write_printf(ctx, opts->out, "%d %d obj\n", num, gen);

		if (unenc)
		{
			pdf_dict_put_int(ctx, obj, PDF_NAME(Length), len);
			pdf_print_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii);
			fz_write_string(ctx, opts->out, "\nstream\n");
			fz_write_data(ctx, opts->out, data, len);
		}
		else
		{
			pdf_dict_put_int(ctx, obj, PDF_NAME(Length), pdf_encrypted_len(ctx, opts->crypt, num, gen, (int)len));
			pdf_print_encrypted_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii, opts->crypt, num, gen);
			fz_write_string(ctx, opts->out, "\nstream\n");
			pdf_encrypt_data(ctx, opts->crypt, num, gen, write_data, opts->out, data, len);
		}

		fz_write_string(ctx, opts->out, "\nendstream\nendobj\n\n");
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, tmp_hex);
		fz_drop_buffer(ctx, tmp_comp);
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static int is_image_filter(pdf_obj *s)
{
	return
		s == PDF_NAME(CCITTFaxDecode) || s == PDF_NAME(CCF) ||
		s == PDF_NAME(DCTDecode) || s == PDF_NAME(DCT) ||
		s == PDF_NAME(RunLengthDecode) || s == PDF_NAME(RL) ||
		s == PDF_NAME(JBIG2Decode) ||
		s == PDF_NAME(JPXDecode);
}

static int filter_implies_image(fz_context *ctx, pdf_obj *o)
{
	if (pdf_is_name(ctx, o))
		return is_image_filter(o);
	if (pdf_is_array(ctx, o))
	{
		int i, len;
		len = pdf_array_len(ctx, o);
		for (i = 0; i < len; i++)
			if (is_image_filter(pdf_array_get(ctx, o, i)))
				return 1;
	}
	return 0;
}

static int is_jpx_filter(fz_context *ctx, pdf_obj *o)
{
	if (o == PDF_NAME(JPXDecode))
		return 1;
	if (pdf_is_array(ctx, o))
	{
		int i, len;
		len = pdf_array_len(ctx, o);
		for (i = 0; i < len; i++)
			if (pdf_array_get(ctx, o, i) == PDF_NAME(JPXDecode))
				return 1;
	}
	return 0;
}

static int is_image_stream(fz_context *ctx, pdf_obj *obj)
{
	pdf_obj *o;
	if ((o = pdf_dict_get(ctx, obj, PDF_NAME(Type)), pdf_name_eq(ctx, o, PDF_NAME(XObject))))
		if ((o = pdf_dict_get(ctx, obj, PDF_NAME(Subtype)), pdf_name_eq(ctx, o, PDF_NAME(Image))))
			return 1;
	if (o = pdf_dict_get(ctx, obj, PDF_NAME(Filter)), filter_implies_image(ctx, o))
		return 1;
	if (pdf_dict_get(ctx, obj, PDF_NAME(Width)) != NULL && pdf_dict_get(ctx, obj, PDF_NAME(Height)) != NULL)
		return 1;
	return 0;
}

static int is_font_stream(fz_context *ctx, pdf_obj *obj)
{
	pdf_obj *o;
	if (o = pdf_dict_get(ctx, obj, PDF_NAME(Type)), pdf_name_eq(ctx, o, PDF_NAME(Font)))
		return 1;
	if (o = pdf_dict_get(ctx, obj, PDF_NAME(Type)), pdf_name_eq(ctx, o, PDF_NAME(FontDescriptor)))
		return 1;
	if (pdf_dict_get(ctx, obj, PDF_NAME(Length1)) != NULL)
		return 1;
	if (pdf_dict_get(ctx, obj, PDF_NAME(Length2)) != NULL)
		return 1;
	if (pdf_dict_get(ctx, obj, PDF_NAME(Length3)) != NULL)
		return 1;
	if (o = pdf_dict_get(ctx, obj, PDF_NAME(Subtype)), pdf_name_eq(ctx, o, PDF_NAME(Type1C)))
		return 1;
	if (o = pdf_dict_get(ctx, obj, PDF_NAME(Subtype)), pdf_name_eq(ctx, o, PDF_NAME(CIDFontType0C)))
		return 1;
	return 0;
}

static int is_jpx_stream(fz_context *ctx, pdf_obj *obj)
{
	pdf_obj *o;
	if (o = pdf_dict_get(ctx, obj, PDF_NAME(Filter)), is_jpx_filter(ctx, o))
		return 1;
	return 0;
}


static int is_xml_metadata(fz_context *ctx, pdf_obj *obj)
{
	if (pdf_name_eq(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Type)), PDF_NAME(Metadata)))
		if (pdf_name_eq(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Subtype)), PDF_NAME(XML)))
			return 1;
	return 0;
}

static void writeobject(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int num, int gen, int skip_xrefs, int unenc)
{
	pdf_obj *obj = NULL;
	fz_buffer *buf = NULL;
	int do_deflate = 0;
	int do_expand = 0;
	int skip = 0;

	fz_var(obj);
	fz_var(buf);

	if (opts->do_encrypt == PDF_ENCRYPT_NONE)
		unenc = 1;

	fz_try(ctx)
	{
		obj = pdf_load_object(ctx, doc, num);

		/* skip ObjStm and XRef objects */
		if (pdf_is_dict(ctx, obj))
		{
			pdf_obj *type = pdf_dict_get(ctx, obj, PDF_NAME(Type));
			if (type == PDF_NAME(ObjStm) && !opts->do_use_objstms)
			{
				if (opts->use_list)
					opts->use_list[num] = 0;
				skip = 1;
			}
			if (skip_xrefs && type == PDF_NAME(XRef))
			{
				if (opts->use_list)
					opts->use_list[num] = 0;
				skip = 1;
			}
		}

		if (!skip)
		{
			if (pdf_obj_num_is_stream(ctx, doc, num))
			{
				do_deflate = opts->do_compress;
				do_expand = opts->do_expand;
				if (opts->do_compress_images && is_image_stream(ctx, obj))
					do_deflate = 1, do_expand = 0;
				if (opts->do_compress_fonts && is_font_stream(ctx, obj))
					do_deflate = 1, do_expand = 0;
				if (is_xml_metadata(ctx, obj))
					do_deflate = 0, do_expand = 0;
				if (is_jpx_stream(ctx, obj))
					do_deflate = 0, do_expand = 0;

				if (do_expand && num != opts->hint_object_num)
					expandstream(ctx, doc, opts, obj, num, gen, do_deflate, unenc);
				else
					copystream(ctx, doc, opts, obj, num, gen, do_deflate, unenc);
			}
			else
			{
				fz_write_printf(ctx, opts->out, "%d %d obj\n", num, gen);
				pdf_print_encrypted_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii, unenc ? NULL : opts->crypt, num, gen);
				fz_write_string(ctx, opts->out, "\nendobj\n\n");
			}
		}
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void writexrefsubsect(fz_context *ctx, pdf_write_state *opts, int from, int to)
{
	int num;

	fz_write_printf(ctx, opts->out, "%d %d\n", from, to - from);
	for (num = from; num < to; num++)
	{
		if (opts->use_list[num])
			fz_write_printf(ctx, opts->out, "%010lu %05d n \n", opts->ofs_list[num], opts->gen_list[num]);
		else
			fz_write_printf(ctx, opts->out, "%010lu %05d f \n", opts->ofs_list[num], opts->gen_list[num]);
	}
}

static void writexref(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int from, int to, int first, int64_t main_xref_offset, int64_t startxref)
{
	pdf_obj *trailer = NULL;
	pdf_obj *obj;

	fz_write_string(ctx, opts->out, "xref\n");
	opts->first_xref_entry_offset = fz_tell_output(ctx, opts->out);

	if (opts->do_incremental)
	{
		int subfrom = from;
		int subto;

		while (subfrom < to)
		{
			while (subfrom < to && !pdf_xref_is_incremental(ctx, doc, subfrom))
				subfrom++;

			subto = subfrom;
			while (subto < to && pdf_xref_is_incremental(ctx, doc, subto))
				subto++;

			if (subfrom < subto)
				writexrefsubsect(ctx, opts, subfrom, subto);

			subfrom = subto;
		}
	}
	else
	{
		writexrefsubsect(ctx, opts, from, to);
	}

	fz_write_string(ctx, opts->out, "\n");

	fz_var(trailer);

	fz_try(ctx)
	{
		if (opts->do_incremental)
		{
			trailer = pdf_keep_obj(ctx, pdf_trailer(ctx, doc));
			pdf_dict_put_int(ctx, trailer, PDF_NAME(Size), pdf_xref_len(ctx, doc));
			pdf_dict_put_int(ctx, trailer, PDF_NAME(Prev), doc->startxref);
			if (!opts->do_snapshot)
				doc->startxref = startxref;
		}
		else
		{
			trailer = pdf_new_dict(ctx, doc, 5);

			pdf_dict_put_int(ctx, trailer, PDF_NAME(Size), to);

			if (first)
			{
				pdf_obj *otrailer = pdf_trailer(ctx, doc);
				obj = pdf_dict_get(ctx, otrailer, PDF_NAME(Info));
				if (obj)
					pdf_dict_put(ctx, trailer, PDF_NAME(Info), obj);

				obj = pdf_dict_get(ctx, otrailer, PDF_NAME(Root));
				if (obj)
					pdf_dict_put(ctx, trailer, PDF_NAME(Root), obj);


				obj = pdf_dict_get(ctx, otrailer, PDF_NAME(ID));
				if (obj)
					pdf_dict_put(ctx, trailer, PDF_NAME(ID), obj);

				if (opts->crypt_obj)
				{
					if (pdf_is_indirect(ctx, opts->crypt_obj))
						pdf_dict_put_drop(ctx, trailer, PDF_NAME(Encrypt), pdf_new_indirect(ctx, doc, opts->crypt_object_number, 0));
					else
						pdf_dict_put(ctx, trailer, PDF_NAME(Encrypt), opts->crypt_obj);
				}

				if (opts->metadata)
					pdf_dict_putp(ctx, trailer, "Root/Metadata", opts->metadata);
			}
			if (main_xref_offset != 0)
				pdf_dict_put_int(ctx, trailer, PDF_NAME(Prev), main_xref_offset);
		}

		fz_write_string(ctx, opts->out, "trailer\n");
		/* Trailer is NOT encrypted */
		pdf_print_obj(ctx, opts->out, trailer, opts->do_tight, opts->do_ascii);
		fz_write_string(ctx, opts->out, "\n");

		fz_write_printf(ctx, opts->out, "startxref\n%lu\n%%%%EOF\n", startxref);

		doc->last_xref_was_old_style = 1;
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, trailer);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void writexrefstreamsubsect(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, pdf_obj *index, fz_buffer *fzbuf, int from, int to)
{
	int num;

	pdf_array_push_int(ctx, index, from);
	pdf_array_push_int(ctx, index, to - from);
	for (num = from; num < to; num++)
	{
		int f1, f2, f3;
		pdf_xref_entry *x = pdf_get_xref_entry_no_null(ctx, doc, num);
		if (opts->use_list[num] == 0)
		{
			f1 = 0; /* Free */
			f2 = opts->ofs_list[num];
			f3 = opts->gen_list[num];
		}
		else if (x->type == 'o')
		{
			f1 = 2; /* Object Stream */
			f2 = opts->ofs_list[num];
			f3 = opts->gen_list[num];
		}
		else
		{
			f1 = 1; /* Object */
			f2 = opts->ofs_list[num];
			f3 = opts->gen_list[num];
		}
		fz_append_byte(ctx, fzbuf, f1);
		fz_append_byte(ctx, fzbuf, f2>>24);
		fz_append_byte(ctx, fzbuf, f2>>16);
		fz_append_byte(ctx, fzbuf, f2>>8);
		fz_append_byte(ctx, fzbuf, f2);
		fz_append_byte(ctx, fzbuf, f3);
	}
}

static void writexrefstream(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int from, int to, int first, int64_t main_xref_offset, int64_t startxref)
{
	int num;
	pdf_obj *dict = NULL;
	pdf_obj *obj;
	pdf_obj *w = NULL;
	pdf_obj *index;
	fz_buffer *fzbuf = NULL;

	fz_var(dict);
	fz_var(w);
	fz_var(fzbuf);
	fz_try(ctx)
	{
		num = pdf_create_object(ctx, doc);
		expand_lists(ctx, opts, num);

		dict = pdf_new_dict(ctx, doc, 6);
		pdf_update_object(ctx, doc, num, dict);

		opts->first_xref_entry_offset = fz_tell_output(ctx, opts->out);

		to++;

		if (first)
		{
			obj = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info));
			if (obj)
				pdf_dict_put(ctx, dict, PDF_NAME(Info), obj);

			obj = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			if (obj)
				pdf_dict_put(ctx, dict, PDF_NAME(Root), obj);

			obj = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(ID));
			if (obj)
				pdf_dict_put(ctx, dict, PDF_NAME(ID), obj);

			if (opts->do_incremental)
			{
				obj = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt));
				if (obj)
					pdf_dict_put(ctx, dict, PDF_NAME(Encrypt), obj);
			}
		}

		pdf_dict_put_int(ctx, dict, PDF_NAME(Size), to);

		if (opts->do_incremental)
		{
			pdf_dict_put_int(ctx, dict, PDF_NAME(Prev), doc->startxref);
			if (!opts->do_snapshot)
				doc->startxref = startxref;
		}
		else
		{
			if (main_xref_offset != 0)
				pdf_dict_put_int(ctx, dict, PDF_NAME(Prev), main_xref_offset);
		}

		pdf_dict_put(ctx, dict, PDF_NAME(Type), PDF_NAME(XRef));

		w = pdf_new_array(ctx, doc, 3);
		pdf_dict_put(ctx, dict, PDF_NAME(W), w);
		pdf_array_push_int(ctx, w, 1);
		pdf_array_push_int(ctx, w, 4);
		pdf_array_push_int(ctx, w, 1);

		index = pdf_new_array(ctx, doc, 2);
		pdf_dict_put_drop(ctx, dict, PDF_NAME(Index), index);

		/* opts->gen_list[num] is already initialized by fz_calloc. */
		opts->use_list[num] = 1;
		opts->ofs_list[num] = opts->first_xref_entry_offset;

		fzbuf = fz_new_buffer(ctx, (1 + 4 + 1) * (to-from));

		if (opts->do_incremental)
		{
			int subfrom = from;
			int subto;

			while (subfrom < to)
			{
				while (subfrom < to && !pdf_xref_is_incremental(ctx, doc, subfrom))
					subfrom++;

				subto = subfrom;
				while (subto < to && pdf_xref_is_incremental(ctx, doc, subto))
					subto++;

				if (subfrom < subto)
					writexrefstreamsubsect(ctx, doc, opts, index, fzbuf, subfrom, subto);

				subfrom = subto;
			}
		}
		else
		{
			writexrefstreamsubsect(ctx, doc, opts, index, fzbuf, from, to);
		}

		pdf_update_stream(ctx, doc, dict, fzbuf, 0);

		writeobject(ctx, doc, opts, num, 0, 0, 1);
		fz_write_printf(ctx, opts->out, "startxref\n%lu\n%%%%EOF\n", startxref);

		if (opts->do_snapshot)
			pdf_delete_object(ctx, doc, num);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, dict);
		pdf_drop_obj(ctx, w);
		fz_drop_buffer(ctx, fzbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	doc->last_xref_was_old_style = 0;
}

static void
padto(fz_context *ctx, fz_output *out, int64_t target)
{
	int64_t pos = fz_tell_output(ctx, out);

	assert(pos <= target);
	while (pos < target)
	{
		fz_write_byte(ctx, out, '\n');
		pos++;
	}
}

static void
dowriteobject(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int num, int pass)
{
	pdf_xref_entry *entry = pdf_get_xref_entry_no_null(ctx, doc, num);
	int gen = opts->gen_list ? opts->gen_list[num] : 0;
	if (entry->type == 'f')
		gen = entry->gen;
	if (entry->type == 'n')
		gen = entry->gen;

	/* If we are renumbering, then make sure all generation numbers are
	 * zero (except object 0 which must be free, and have a gen number of
	 * 65535). Changing the generation numbers (and indeed object numbers)
	 * will break encryption - so only do this if we are renumbering
	 * anyway. */
	if (opts->do_garbage >= 2)
		gen = (num == 0 ? 65535 : 0);

	/* For objects in object streams, the gen number gives us the index of
	 * the object within the stream. */
	if (entry->type == 'o')
		gen = entry->gen;

	if (opts->gen_list)
		opts->gen_list[num] = gen;

	if (opts->do_garbage && !opts->use_list[num])
		return;

	if (entry->type == 'o' && (!opts->do_incremental || pdf_xref_is_incremental(ctx, doc, num)))
	{
		assert(opts->do_use_objstms);
		opts->ofs_list[num] = entry->ofs;
		return;
	}

	if (entry->type == 'n')
	{
		if (pass > 0)
			padto(ctx, opts->out, opts->ofs_list[num]);
		if (!opts->do_incremental || pdf_xref_is_incremental(ctx, doc, num))
		{
			if (opts->ofs_list)
				opts->ofs_list[num] = fz_tell_output(ctx, opts->out);
			writeobject(ctx, doc, opts, num, gen, 1, num == opts->crypt_object_number);
		}
	}
	else if (opts->use_list)
		opts->use_list[num] = 0;
}

static void
writeobjects(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int pass)
{
	int num;
	int xref_len = pdf_xref_len(ctx, doc);

	if (!opts->do_incremental)
	{
		int version = pdf_version(ctx, doc);
		fz_write_printf(ctx, opts->out, "%%PDF-%d.%d\n", version / 10, version % 10);
		fz_write_string(ctx, opts->out, "%\xC2\xB5\xC2\xB6\n\n");
	}

	dowriteobject(ctx, doc, opts, opts->start, pass);

	if (opts->do_linear)
	{
		/* Write first xref */
		if (pass == 0)
			opts->first_xref_offset = fz_tell_output(ctx, opts->out);
		else
			padto(ctx, opts->out, opts->first_xref_offset);
		writexref(ctx, doc, opts, opts->start, pdf_xref_len(ctx, doc), 1, opts->main_xref_offset, 0);
	}

	for (num = opts->start+1; num < xref_len; num++)
		dowriteobject(ctx, doc, opts, num, pass);
	if (opts->do_linear && pass == 1)
	{
		int64_t offset = (opts->start == 1 ? opts->main_xref_offset : opts->ofs_list[1] + opts->hintstream_len);
		padto(ctx, opts->out, offset);
	}
	for (num = 1; num < opts->start; num++)
	{
		if (pass == 1)
			opts->ofs_list[num] += opts->hintstream_len;
		dowriteobject(ctx, doc, opts, num, pass);
	}
}

static int
my_log2(int x)
{
	int i = 0;
	const int sign_bit = sizeof(int)*8-1;

	if (x <= 0)
		return 0;

	while ((1<<i) <= x && i < sign_bit)
		i++;

	if (i >= sign_bit)
		return 0;

	return i;
}

static int64_t
offset_of_first_used_obj_after(const pdf_write_state *opts, int i, int len)
{
	/* The objects in the file are laid out as:
	 *
	 * start
	 * ...
	 * len-1
	 * 1
	 * ...
	 * start-1
	 *
	 * But, some may not be present...
	 */
	do
	{
		i++;
		if (i == len)
			i = 1;
		if (i == opts->start)
			return opts->main_xref_offset;
	}
	while (opts->use_list[i] == 0);

	return opts->ofs_list[i];
}

static void
make_page_offset_hints(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, fz_buffer *buf)
{
	int i, j;
	int min_objs_per_page, max_objs_per_page;
	int min_page_length, max_page_length;
	int objs_per_page_bits;
	int min_shared_object, max_shared_object;
	int max_shared_object_refs = 0;
	int min_shared_length, max_shared_length;
	page_objects **pop = &opts->page_object_lists->page[0];
	int page_len_bits, shared_object_bits, shared_object_id_bits;
	int shared_length_bits;
	int xref_len = pdf_xref_len(ctx, doc);

	min_shared_object = pdf_xref_len(ctx, doc);
	max_shared_object = 1;
	min_shared_length = opts->file_len;
	max_shared_length = 0;
	for (i=1; i < xref_len; i++)
	{
		int min, max, page;

		min = opts->ofs_list[i];
		max = offset_of_first_used_obj_after(opts, i, xref_len);

		assert(max > min);

		if (opts->use_list[i] & USE_SHARED)
		{
			page = -1;
			if (i < min_shared_object)
				min_shared_object = i;
			if (i > max_shared_object)
				max_shared_object = i;
			if (min_shared_length > max - min)
				min_shared_length = max - min;
			if (max_shared_length < max - min)
				max_shared_length = max - min;
		}
		else if (opts->use_list[i] & (USE_CATALOGUE | USE_HINTS | USE_PARAMS))
			page = -1;
		else if (opts->use_list[i] & USE_PAGE1)
		{
			page = 0;
			if (min_shared_length > max - min)
				min_shared_length = max - min;
			if (max_shared_length < max - min)
				max_shared_length = max - min;
		}
		else if (opts->use_list[i] == 0)
			page = -1;
		else
			page = opts->use_list[i]>>USE_PAGE_SHIFT;

		if (page >= 0)
		{
			pop[page]->num_objects++;
			if (pop[page]->min_ofs > min)
				pop[page]->min_ofs = min;
			if (pop[page]->max_ofs < max)
				pop[page]->max_ofs = max;
		}
	}

	min_objs_per_page = max_objs_per_page = pop[0]->num_objects;
	min_page_length = max_page_length = pop[0]->max_ofs - pop[0]->min_ofs;
	for (i=1; i < opts->page_count; i++)
	{
		int tmp;
		if (min_objs_per_page > pop[i]->num_objects)
			min_objs_per_page = pop[i]->num_objects;
		if (max_objs_per_page < pop[i]->num_objects)
			max_objs_per_page = pop[i]->num_objects;
		tmp = pop[i]->max_ofs - pop[i]->min_ofs;
		if (tmp < min_page_length)
			min_page_length = tmp;
		if (tmp > max_page_length)
			max_page_length = tmp;
	}

	for (i=0; i < opts->page_count; i++)
	{
		int count = 0;
		page_objects *po = opts->page_object_lists->page[i];
		for (j = 0; j < po->len; j++)
		{
			if (i == 0 && opts->use_list[po->object[j]] & USE_PAGE1)
				count++;
			else if (i != 0 && opts->use_list[po->object[j]] & USE_SHARED)
				count++;
		}
		po->num_shared = count;
		if (i == 0 || count > max_shared_object_refs)
			max_shared_object_refs = count;
	}
	if (min_shared_object > max_shared_object)
		min_shared_object = max_shared_object = 0;

	/* Table F.3 - Header */
	/* Header Item 1: Least number of objects in a page */
	fz_append_bits(ctx, buf, min_objs_per_page, 32);
	/* Header Item 2: Location of first pages page object */
	fz_append_bits(ctx, buf, opts->ofs_list[pop[0]->page_object_number], 32);
	/* Header Item 3: Number of bits required to represent the difference
	 * between the greatest and least number of objects in a page. */
	objs_per_page_bits = my_log2(max_objs_per_page - min_objs_per_page);
	fz_append_bits(ctx, buf, objs_per_page_bits, 16);
	/* Header Item 4: Least length of a page. */
	fz_append_bits(ctx, buf, min_page_length, 32);
	/* Header Item 5: Number of bits needed to represent the difference
	 * between the greatest and least length of a page. */
	page_len_bits = my_log2(max_page_length - min_page_length);
	fz_append_bits(ctx, buf, page_len_bits, 16);
	/* Header Item 6: Least offset to start of content stream (Acrobat
	 * sets this to always be 0) */
	fz_append_bits(ctx, buf, 0, 32);
	/* Header Item 7: Number of bits needed to represent the difference
	 * between the greatest and least offset to content stream (Acrobat
	 * sets this to always be 0) */
	fz_append_bits(ctx, buf, 0, 16);
	/* Header Item 8: Least content stream length. (Acrobat
	 * sets this to always be 0) */
	fz_append_bits(ctx, buf, 0, 32);
	/* Header Item 9: Number of bits needed to represent the difference
	 * between the greatest and least content stream length (Acrobat
	 * sets this to always be the same as item 5) */
	fz_append_bits(ctx, buf, page_len_bits, 16);
	/* Header Item 10: Number of bits needed to represent the greatest
	 * number of shared object references. */
	shared_object_bits = my_log2(max_shared_object_refs);
	fz_append_bits(ctx, buf, shared_object_bits, 16);
	/* Header Item 11: Number of bits needed to represent the greatest
	 * shared object identifier. */
	shared_object_id_bits = my_log2(max_shared_object - min_shared_object + pop[0]->num_shared);
	fz_append_bits(ctx, buf, shared_object_id_bits, 16);
	/* Header Item 12: Number of bits needed to represent the numerator
	 * of the fractions. We always send 0. */
	fz_append_bits(ctx, buf, 0, 16);
	/* Header Item 13: Number of bits needed to represent the denominator
	 * of the fractions. We always send 0. */
	fz_append_bits(ctx, buf, 0, 16);

	/* Table F.4 - Page offset hint table (per page) */
	/* Item 1: A number that, when added to the least number of objects
	 * on a page, gives the number of objects in the page. */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_append_bits(ctx, buf, pop[i]->num_objects - min_objs_per_page, objs_per_page_bits);
	}
	fz_append_bits_pad(ctx, buf);
	/* Item 2: A number that, when added to the least page length, gives
	 * the length of the page in bytes. */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_append_bits(ctx, buf, pop[i]->max_ofs - pop[i]->min_ofs - min_page_length, page_len_bits);
	}
	fz_append_bits_pad(ctx, buf);
	/* Item 3: The number of shared objects referenced from the page. */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_append_bits(ctx, buf, pop[i]->num_shared, shared_object_bits);
	}
	fz_append_bits_pad(ctx, buf);
	/* Item 4: Shared object id for each shared object ref in every page.
	 * Spec says "not for page 1", but acrobat does send page 1's - all
	 * as zeros. */
	for (i = 0; i < opts->page_count; i++)
	{
		for (j = 0; j < pop[i]->len; j++)
		{
			int o = pop[i]->object[j];
			if (i == 0 && opts->use_list[o] & USE_PAGE1)
				fz_append_bits(ctx, buf, 0 /* o - pop[0]->page_object_number */, shared_object_id_bits);
			if (i != 0 && opts->use_list[o] & USE_SHARED)
				fz_append_bits(ctx, buf, o - min_shared_object + pop[0]->num_shared, shared_object_id_bits);
		}
	}
	fz_append_bits_pad(ctx, buf);
	/* Item 5: Numerator of fractional position for each shared object reference. */
	/* We always send 0 in 0 bits */
	/* Item 6: A number that, when added to the least offset to the start
	 * of the content stream (F.3 Item 6), gives the offset in bytes of
	 * start of the pages content stream object relative to the beginning
	 * of the page. Always 0 in 0 bits. */
	/* Item 7: A number that, when added to the least content stream length
	 * (F.3 Item 8), gives the length of the pages content stream object.
	 * Always == Item 2 as least content stream length = least page stream
	 * length.
	 */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_append_bits(ctx, buf, pop[i]->max_ofs - pop[i]->min_ofs - min_page_length, page_len_bits);
	}

	/* Pad, and then do shared object hint table */
	fz_append_bits_pad(ctx, buf);
	opts->hints_shared_offset = (int)fz_buffer_storage(ctx, buf, NULL);

	/* Table F.5: */
	/* Header Item 1: Object number of the first object in the shared
	 * objects section. */
	fz_append_bits(ctx, buf, min_shared_object, 32);
	/* Header Item 2: Location of first object in the shared objects
	 * section. */
	fz_append_bits(ctx, buf, opts->ofs_list[min_shared_object], 32);
	/* Header Item 3: The number of shared object entries for the first
	 * page. */
	fz_append_bits(ctx, buf, pop[0]->num_shared, 32);
	/* Header Item 4: The number of shared object entries for the shared
	 * objects section + first page. */
	fz_append_bits(ctx, buf, max_shared_object - min_shared_object + pop[0]->num_shared, 32);
	/* Header Item 5: The number of bits needed to represent the greatest
	 * number of objects in a shared object group (Always 0). */
	fz_append_bits(ctx, buf, 0, 16);
	/* Header Item 6: The least length of a shared object group in bytes. */
	fz_append_bits(ctx, buf, min_shared_length, 32);
	/* Header Item 7: The number of bits required to represent the
	 * difference between the greatest and least length of a shared object
	 * group. */
	shared_length_bits = my_log2(max_shared_length - min_shared_length);
	fz_append_bits(ctx, buf, shared_length_bits, 16);

	/* Table F.6 */
	/* Item 1: Shared object group length (page 1 objects) */
	for (j = 0; j < pop[0]->len; j++)
	{
		int o = pop[0]->object[j];
		int64_t min, max;
		min = opts->ofs_list[o];
		if (o == opts->start-1)
			max = opts->main_xref_offset;
		else if (o < xref_len-1)
			max = opts->ofs_list[o+1];
		else
			max = opts->ofs_list[1];
		if (opts->use_list[o] & USE_PAGE1)
			fz_append_bits(ctx, buf, max - min - min_shared_length, shared_length_bits);
	}
	/* Item 1: Shared object group length (shared objects) */
	for (i = min_shared_object; i <= max_shared_object; i++)
	{
		int min, max;
		min = opts->ofs_list[i];
		if (i == opts->start-1)
			max = opts->main_xref_offset;
		else if (i < xref_len-1)
			max = opts->ofs_list[i+1];
		else
			max = opts->ofs_list[1];
		fz_append_bits(ctx, buf, max - min - min_shared_length, shared_length_bits);
	}
	fz_append_bits_pad(ctx, buf);

	/* Item 2: MD5 presence flags */
	for (i = max_shared_object - min_shared_object + pop[0]->num_shared; i > 0; i--)
	{
		fz_append_bits(ctx, buf, 0, 1);
	}
	fz_append_bits_pad(ctx, buf);
	/* Item 3: MD5 sums (not present) */
	fz_append_bits_pad(ctx, buf);
	/* Item 4: Number of objects in the group (not present) */
}

static void
make_hint_stream(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	fz_buffer *buf;
	pdf_obj *obj = NULL;

	fz_var(obj);

	buf = fz_new_buffer(ctx, 100);
	fz_try(ctx)
	{
		make_page_offset_hints(ctx, doc, opts, buf);
		obj = pdf_load_object(ctx, doc, pdf_xref_len(ctx, doc)-1);
		pdf_update_stream(ctx, doc, obj, buf, 0);
		opts->hintstream_len = (int64_t)fz_buffer_storage(ctx, buf, NULL);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

#ifdef DEBUG_WRITING
static void dump_object_details(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int i;

	for (i = 0; i < pdf_xref_len(ctx, doc); i++)
	{
		fprintf(stderr, "%d@%ld: use=%d\n", i, opts->ofs_list[i], opts->use_list[i]);
	}
}
#endif

static void presize_unsaved_signature_byteranges(fz_context *ctx, pdf_document *doc)
{
	int s;

	for (s = 0; s < doc->num_incremental_sections; s++)
	{
		pdf_xref *xref = &doc->xref_sections[s];

		if (xref->unsaved_sigs)
		{
			/* The ByteRange objects of signatures are initially written out with
			* dummy values, and then overwritten later. We need to make sure their
			* initial form at least takes enough sufficient file space */
			pdf_unsaved_sig *usig;
			int n = 0;

			for (usig = xref->unsaved_sigs; usig; usig = usig->next)
				n++;

			for (usig = xref->unsaved_sigs; usig; usig = usig->next)
			{
				/* There will be segments of bytes at the beginning, at
				* the end and between each consecutive pair of signatures,
				* hence n + 1 */
				int i;
				pdf_obj *byte_range = pdf_dict_getl(ctx, usig->field, PDF_NAME(V), PDF_NAME(ByteRange), NULL);

				for (i = 0; i < n+1; i++)
				{
					pdf_array_push_int(ctx, byte_range, INT_MAX);
					pdf_array_push_int(ctx, byte_range, INT_MAX);
				}
			}
		}
	}
}

static void complete_signatures(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	pdf_obj *byte_range = NULL;
	char *buf = NULL, *ptr;
	int s;
	fz_stream *stm = NULL;

	fz_var(byte_range);
	fz_var(stm);
	fz_var(buf);

	fz_try(ctx)
	{
		for (s = 0; s < doc->num_incremental_sections; s++)
		{
			pdf_xref *xref = &doc->xref_sections[doc->num_incremental_sections - s - 1];

			if (xref->unsaved_sigs)
			{
				pdf_unsaved_sig *usig;
				size_t buf_size = 0;
				size_t i;
				size_t last_end;

				for (usig = xref->unsaved_sigs; usig; usig = usig->next)
				{
					size_t size = usig->signer->max_digest_size(ctx, usig->signer);
					buf_size = fz_maxz(buf_size, size);
				}

				buf_size = buf_size * 2 + SIG_EXTRAS_SIZE;

				buf = fz_calloc(ctx, buf_size, 1);

				stm = fz_stream_from_output(ctx, opts->out);
				/* Locate the byte ranges and contents in the saved file */
				for (usig = xref->unsaved_sigs; usig; usig = usig->next)
				{
					char *bstr, *cstr, *fstr;
					size_t bytes_read;
					int pnum = pdf_obj_parent_num(ctx, pdf_dict_getl(ctx, usig->field, PDF_NAME(V), PDF_NAME(ByteRange), NULL));
					fz_seek(ctx, stm, opts->ofs_list[pnum], SEEK_SET);
					/* SIG_EXTRAS_SIZE is an arbitrary value and its addition above to buf_size
					 * could cause an attempt to read off the end of the file. That's not an
					 * error, but we need to keep track of how many bytes are read and search
					 * for markers only in defined data */
					bytes_read = fz_read(ctx, stm, (unsigned char *)buf, buf_size);
					assert(bytes_read <= buf_size);

					bstr = fz_memmem(buf, bytes_read, SLASH_BYTE_RANGE, sizeof(SLASH_BYTE_RANGE)-1);
					cstr = fz_memmem(buf, bytes_read, SLASH_CONTENTS, sizeof(SLASH_CONTENTS)-1);
					fstr = fz_memmem(buf, bytes_read, SLASH_FILTER, sizeof(SLASH_FILTER)-1);

					if (!(bstr && cstr && fstr && bstr < cstr && cstr < fstr))
						fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to determine byte ranges while writing signature");

					usig->byte_range_start = bstr - buf + sizeof(SLASH_BYTE_RANGE)-1 + opts->ofs_list[pnum];
					usig->byte_range_end = cstr - buf + opts->ofs_list[pnum];
					usig->contents_start = cstr - buf + sizeof(SLASH_CONTENTS)-1 + opts->ofs_list[pnum];
					usig->contents_end = fstr - buf + opts->ofs_list[pnum];
				}

				fz_drop_stream(ctx, stm);
				stm = NULL;

				/* Recreate ByteRange with correct values. */
				byte_range = pdf_new_array(ctx, doc, 4);

				last_end = 0;
				for (usig = xref->unsaved_sigs; usig; usig = usig->next)
				{
					pdf_array_push_int(ctx, byte_range, last_end);
					pdf_array_push_int(ctx, byte_range, usig->contents_start - last_end);
					last_end = usig->contents_end;
				}
				pdf_array_push_int(ctx, byte_range, last_end);
				pdf_array_push_int(ctx, byte_range, xref->end_ofs - last_end);

				/* Copy the new ByteRange to the other unsaved signatures */
				for (usig = xref->unsaved_sigs; usig; usig = usig->next)
					pdf_dict_putl_drop(ctx, usig->field, pdf_copy_array(ctx, byte_range), PDF_NAME(V), PDF_NAME(ByteRange), NULL);

				/* Write the byte range into buf, padding with spaces*/
				ptr = pdf_sprint_obj(ctx, buf, buf_size, &i, byte_range, 1, 0);
				if (ptr != buf) /* should never happen, since data should fit in buf_size */
					fz_free(ctx, ptr);
				memset(buf+i, ' ', buf_size-i);

				/* Write the byte range to the file */
				for (usig = xref->unsaved_sigs; usig; usig = usig->next)
				{
					fz_seek_output(ctx, opts->out, usig->byte_range_start, SEEK_SET);
					fz_write_data(ctx, opts->out, buf, usig->byte_range_end - usig->byte_range_start);
				}

				/* Write the digests into the file */
				for (usig = xref->unsaved_sigs; usig; usig = usig->next)
					pdf_write_digest(ctx, opts->out, byte_range, usig->field, usig->contents_start, usig->contents_end - usig->contents_start, usig->signer);

				/* delete the unsaved_sigs records */
				while ((usig = xref->unsaved_sigs) != NULL)
				{
					xref->unsaved_sigs = usig->next;
					pdf_drop_obj(ctx, usig->field);
					pdf_drop_signer(ctx, usig->signer);
					fz_free(ctx, usig);
				}

				xref->unsaved_sigs_end = NULL;

				pdf_drop_obj(ctx, byte_range);
				byte_range = NULL;

				fz_free(ctx, buf);
				buf = NULL;
			}
		}
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, byte_range);
	}
	fz_catch(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_free(ctx, buf);
		fz_rethrow(ctx);
	}
}

static void clean_content_streams(fz_context *ctx, pdf_document *doc, int sanitize, int ascii)
{
	int n = pdf_count_pages(ctx, doc);
	int i;

	pdf_filter_options options = { 0 };
	pdf_sanitize_filter_options sopts = { 0 };
	pdf_filter_factory list[2] = { 0 };

	options.recurse = 1;
	options.ascii = ascii;
	options.filters = sanitize ? list : NULL;
	list[0].filter = pdf_new_sanitize_filter;
	list[0].options = &sopts;

	for (i = 0; i < n; i++)
	{
		pdf_annot *annot;
		pdf_page *page = pdf_load_page(ctx, doc, i);

		fz_try(ctx)
		{
			pdf_filter_page_contents(ctx, doc, page, &options);
			for (annot = pdf_first_annot(ctx, page); annot != NULL; annot = pdf_next_annot(ctx, annot))
			{
				pdf_filter_annot_contents(ctx, doc, annot, &options);
			}
		}
		fz_always(ctx)
			fz_drop_page(ctx, &page->super);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

/* Initialise the pdf_write_state, used dynamically during the write, from the static
 * pdf_write_options, passed into pdf_save_document */
static void initialise_write_state(fz_context *ctx, pdf_document *doc, const pdf_write_options *in_opts, pdf_write_state *opts)
{
	int xref_len = pdf_xref_len(ctx, doc);

	opts->do_incremental = in_opts->do_incremental;
	opts->do_ascii = in_opts->do_ascii;
	opts->do_tight = !in_opts->do_pretty;
	opts->do_expand = in_opts->do_decompress;
	opts->do_compress = in_opts->do_compress;
	opts->do_compress_images = in_opts->do_compress_images;
	opts->do_compress_fonts = in_opts->do_compress_fonts;
	opts->do_snapshot = in_opts->do_snapshot;
	opts->compression_effort = in_opts->compression_effort;
	if (opts->compression_effort < 0)
		opts->compression_effort = 0;
	else if (opts->compression_effort > 100)
		opts->compression_effort = 100;

	opts->do_garbage = in_opts->do_garbage;
	opts->do_linear = in_opts->do_linear;
	opts->do_clean = in_opts->do_clean;
	opts->do_encrypt = in_opts->do_encrypt;
	opts->dont_regenerate_id = in_opts->dont_regenerate_id;
	opts->do_preserve_metadata = in_opts->do_preserve_metadata;
	opts->do_use_objstms = in_opts->do_use_objstms;
	opts->start = 0;
	opts->main_xref_offset = INT_MIN;

	opts->permissions = in_opts->permissions;
	memcpy(opts->opwd_utf8, in_opts->opwd_utf8, nelem(opts->opwd_utf8));
	memcpy(opts->upwd_utf8, in_opts->upwd_utf8, nelem(opts->upwd_utf8));

	/* We deliberately make these arrays long enough to cope with
	* 1 to n access rather than 0..n-1, and add space for 2 new
	* extra entries that may be required for linearization. */
	opts->list_len = 0;
	opts->use_list = NULL;
	opts->ofs_list = NULL;
	opts->gen_list = NULL;
	opts->renumber_map = NULL;
	opts->rev_renumber_map = NULL;

	expand_lists(ctx, opts, xref_len);
}

/* Free the resources held by the dynamic write options */
static void finalise_write_state(fz_context *ctx, pdf_write_state *opts)
{
	fz_free(ctx, opts->use_list);
	fz_free(ctx, opts->ofs_list);
	fz_free(ctx, opts->gen_list);
	fz_free(ctx, opts->renumber_map);
	fz_free(ctx, opts->rev_renumber_map);
	pdf_drop_obj(ctx, opts->linear_l);
	pdf_drop_obj(ctx, opts->linear_h0);
	pdf_drop_obj(ctx, opts->linear_h1);
	pdf_drop_obj(ctx, opts->linear_o);
	pdf_drop_obj(ctx, opts->linear_e);
	pdf_drop_obj(ctx, opts->linear_n);
	pdf_drop_obj(ctx, opts->linear_t);
	pdf_drop_obj(ctx, opts->hints_s);
	pdf_drop_obj(ctx, opts->hints_length);
	page_objects_list_destroy(ctx, opts->page_object_lists);
}

const pdf_write_options pdf_default_write_options = {
	0, /* do_incremental */
	0, /* do_pretty */
	0, /* do_ascii */
	0, /* do_compress */
	0, /* do_compress_images */
	0, /* do_compress_fonts */
	0, /* do_decompress */
	0, /* do_garbage */
	0, /* do_linear */
	0, /* do_clean */
	0, /* do_sanitize */
	0, /* do_appearance */
	0, /* do_encrypt */
	0, /* dont_regenerate_id */
	~0, /* permissions */
	"", /* opwd_utf8[128] */
	"", /* upwd_utf8[128] */
	0 /* do_snapshot */
};

static const pdf_write_options pdf_snapshot_write_options = {
	1, /* do_incremental */
	0, /* do_pretty */
	0, /* do_ascii */
	0, /* do_compress */
	0, /* do_compress_images */
	0, /* do_compress_fonts */
	0, /* do_decompress */
	0, /* do_garbage */
	0, /* do_linear */
	0, /* do_clean */
	0, /* do_sanitize */
	0, /* do_appearance */
	0, /* do_encrypt */
	1, /* dont_regenerate_id */
	~0, /* permissions */
	"", /* opwd_utf8[128] */
	"", /* upwd_utf8[128] */
	1 /* do_snapshot */
};

const char *fz_pdf_write_options_usage =
	"PDF output options:\n"
	"\tdecompress: decompress all streams (except compress-fonts/images)\n"
	"\tcompress: compress all streams\n"
	"\tcompress-fonts: compress embedded fonts\n"
	"\tcompress-images: compress images\n"
	"\tascii: ASCII hex encode binary streams\n"
	"\tpretty: pretty-print objects with indentation\n"
	"\tlinearize: optimize for web browsers\n"
	"\tclean: pretty-print graphics commands in content streams\n"
	"\tsanitize: sanitize graphics commands in content streams\n"
	"\tgarbage: garbage collect unused objects\n"
	"\tincremental: write changes as incremental update\n"
	"\tcontinue-on-error: continue saving the document even if there is an error\n"
	"\tor garbage=compact: ... and compact cross reference table\n"
	"\tor garbage=deduplicate: ... and remove duplicate objects\n"
	"\tdecrypt: write unencrypted document\n"
	"\tencrypt=rc4-40|rc4-128|aes-128|aes-256: write encrypted document\n"
	"\tpermissions=NUMBER: document permissions to grant when encrypting\n"
	"\tuser-password=PASSWORD: password required to read document\n"
	"\towner-password=PASSWORD: password required to edit document\n"
	"\tregenerate-id: (default yes) regenerate document id\n"
	"\n";

pdf_write_options *
pdf_parse_write_options(fz_context *ctx, pdf_write_options *opts, const char *args)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	if (fz_has_option(ctx, args, "decompress", &val))
		opts->do_decompress = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "compress", &val))
		opts->do_compress = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "compress-fonts", &val))
		opts->do_compress_fonts = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "compress-images", &val))
		opts->do_compress_images = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "ascii", &val))
		opts->do_ascii = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "pretty", &val))
		opts->do_pretty = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "linearize", &val))
		opts->do_linear = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "clean", &val))
		opts->do_clean = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "sanitize", &val))
		opts->do_sanitize = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "incremental", &val))
		opts->do_incremental = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "regenerate-id", &val))
		opts->dont_regenerate_id = fz_option_eq(val, "no");
	if (fz_has_option(ctx, args, "decrypt", &val))
		opts->do_encrypt = fz_option_eq(val, "yes") ? PDF_ENCRYPT_NONE : PDF_ENCRYPT_KEEP;
	if (fz_has_option(ctx, args, "encrypt", &val))
	{
		opts->do_encrypt = PDF_ENCRYPT_UNKNOWN;
		if (fz_option_eq(val, "none") || fz_option_eq(val, "no"))
			opts->do_encrypt = PDF_ENCRYPT_NONE;
		if (fz_option_eq(val, "keep"))
			opts->do_encrypt = PDF_ENCRYPT_KEEP;
		if (fz_option_eq(val, "rc4-40") || fz_option_eq(val, "yes"))
			opts->do_encrypt = PDF_ENCRYPT_RC4_40;
		if (fz_option_eq(val, "rc4-128"))
			opts->do_encrypt = PDF_ENCRYPT_RC4_128;
		if (fz_option_eq(val, "aes-128"))
			opts->do_encrypt = PDF_ENCRYPT_AES_128;
		if (fz_option_eq(val, "aes-256"))
			opts->do_encrypt = PDF_ENCRYPT_AES_256;
	}
	if (fz_has_option(ctx, args, "owner-password", &val))
		fz_copy_option(ctx, val, opts->opwd_utf8, nelem(opts->opwd_utf8));
	if (fz_has_option(ctx, args, "user-password", &val))
		fz_copy_option(ctx, val, opts->upwd_utf8, nelem(opts->upwd_utf8));
	if (fz_has_option(ctx, args, "permissions", &val))
		opts->permissions = fz_atoi(val);
	else
		opts->permissions = ~0;
	if (fz_has_option(ctx, args, "garbage", &val))
	{
		if (fz_option_eq(val, "yes"))
			opts->do_garbage = 1;
		else if (fz_option_eq(val, "compact"))
			opts->do_garbage = 2;
		else if (fz_option_eq(val, "deduplicate"))
			opts->do_garbage = 3;
		else
			opts->do_garbage = fz_atoi(val);
	}
	if (fz_has_option(ctx, args, "appearance", &val))
	{
		if (fz_option_eq(val, "yes"))
			opts->do_appearance = 1;
		else if (fz_option_eq(val, "all"))
			opts->do_appearance = 2;
	}

	return opts;
}

int pdf_can_be_saved_incrementally(fz_context *ctx, pdf_document *doc)
{
	if (doc->repair_attempted)
		return 0;
	if (doc->redacted)
		return 0;
	return 1;
}

static void
prepare_for_save(fz_context *ctx, pdf_document *doc, const pdf_write_options *in_opts)
{
	/* Rewrite (and possibly sanitize) the operator streams */
	if (in_opts->do_clean || in_opts->do_sanitize)
	{
		pdf_begin_operation(ctx, doc, "Clean content streams");
		fz_try(ctx)
		{
			clean_content_streams(ctx, doc, in_opts->do_sanitize, in_opts->do_ascii);
			pdf_end_operation(ctx, doc);
		}
		fz_catch(ctx)
		{
			pdf_abandon_operation(ctx, doc);
			fz_rethrow(ctx);
		}
	}

	/* When saving a PDF with signatures the file will
	first be written once, then the file will have its
	digests and byte ranges calculated and and then the
	signature dictionary containing them will be updated
	both in memory and in the saved file. By setting this
	flag we avoid a new xref section from being created when
	the signature dictionary is updated. */
	doc->save_in_progress = 1;

	if (!in_opts->do_snapshot)
		presize_unsaved_signature_byteranges(ctx, doc);
}

static pdf_obj *
new_identity(fz_context *ctx, pdf_document *doc)
{
	unsigned char rnd[32];
	pdf_obj *id;

	fz_memrnd(ctx, rnd, nelem(rnd));

	id = pdf_dict_put_array(ctx, pdf_trailer(ctx, doc), PDF_NAME(ID), 2);
	pdf_array_push_string(ctx, id, (char *) rnd + 0, nelem(rnd) / 2);
	pdf_array_push_string(ctx, id, (char *) rnd + 16, nelem(rnd) / 2);

	return id;
}

static void
change_identity(fz_context *ctx, pdf_document *doc, pdf_obj *id)
{
	unsigned char rnd[16];
	if (pdf_array_len(ctx, id) >= 2)
	{
		/* Update second half of ID array with new random data. */
		fz_memrnd(ctx, rnd, 16);
		pdf_array_put_string(ctx, id, 1, (char *)rnd, 16);
	}
}

static void
create_encryption_dictionary(fz_context *ctx, pdf_document *doc, pdf_crypt *crypt)
{
	unsigned char *o, *u;
	pdf_obj *encrypt;
	int r;

	r = pdf_crypt_revision(ctx, crypt);

	encrypt = pdf_dict_put_dict(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt), 10);

	pdf_dict_put_name(ctx, encrypt, PDF_NAME(Filter), "Standard");
	pdf_dict_put_int(ctx, encrypt, PDF_NAME(R), r);
	pdf_dict_put_int(ctx, encrypt, PDF_NAME(V), pdf_crypt_version(ctx, crypt));
	pdf_dict_put_int(ctx, encrypt, PDF_NAME(Length), pdf_crypt_length(ctx, crypt));
	pdf_dict_put_int(ctx, encrypt, PDF_NAME(P), pdf_crypt_permissions(ctx, crypt));
	pdf_dict_put_bool(ctx, encrypt, PDF_NAME(EncryptMetadata), pdf_crypt_encrypt_metadata(ctx, crypt));

	o = pdf_crypt_owner_password(ctx, crypt);
	u = pdf_crypt_user_password(ctx, crypt);

	if (r < 4)
	{
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(O), (char *) o, 32);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(U), (char *) u, 32);
	}
	else if (r == 4)
	{
		pdf_obj *cf;

		pdf_dict_put_name(ctx, encrypt, PDF_NAME(StmF), "StdCF");
		pdf_dict_put_name(ctx, encrypt, PDF_NAME(StrF), "StdCF");

		cf = pdf_dict_put_dict(ctx, encrypt, PDF_NAME(CF), 1);
		cf = pdf_dict_put_dict(ctx, cf, PDF_NAME(StdCF), 3);
		pdf_dict_put_name(ctx, cf, PDF_NAME(AuthEvent), "DocOpen");
		pdf_dict_put_name(ctx, cf, PDF_NAME(CFM), "AESV2");
		pdf_dict_put_int(ctx, cf, PDF_NAME(Length), 16);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(O), (char *) o, 32);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(U), (char *) u, 32);
	}
	else if (r == 6)
	{
		unsigned char *oe = pdf_crypt_owner_encryption(ctx, crypt);
		unsigned char *ue = pdf_crypt_user_encryption(ctx, crypt);
		pdf_obj *cf;

		pdf_dict_put_name(ctx, encrypt, PDF_NAME(StmF), "StdCF");
		pdf_dict_put_name(ctx, encrypt, PDF_NAME(StrF), "StdCF");

		cf = pdf_dict_put_dict(ctx, encrypt, PDF_NAME(CF), 1);
		cf = pdf_dict_put_dict(ctx, cf, PDF_NAME(StdCF), 3);
		pdf_dict_put_name(ctx, cf, PDF_NAME(AuthEvent), "DocOpen");
		pdf_dict_put_name(ctx, cf, PDF_NAME(CFM), "AESV3");
		pdf_dict_put_int(ctx, cf, PDF_NAME(Length), 32);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(O), (char *) o, 48);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(U), (char *) u, 48);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(OE), (char *) oe, 32);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(UE), (char *) ue, 32);
		pdf_dict_put_string(ctx, encrypt, PDF_NAME(Perms), (char *) pdf_crypt_permissions_encryption(ctx, crypt), 16);
	}
}

static void
ensure_initial_incremental_contents(fz_context *ctx, fz_stream *in, fz_output *out, int64_t len)
{
	fz_stream *verify;
	unsigned char buf0[4096];
	unsigned char buf1[4096];
	size_t n0, n1;
	int64_t off = 0;
	int same;

	if (!in)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't copy contents for incremental write");

	verify = fz_stream_from_output(ctx, out);

	fz_try(ctx)
	{
		/* Compare current contents of output file (in case we append) */
		if (verify)
		{
			do
			{
				int64_t read = sizeof(buf0);
				if (off + read > len)
					read = len - off;
				fz_seek(ctx, in, off, SEEK_SET);
				n0 = fz_read(ctx, in, buf0, read);
				fz_seek(ctx, verify, off, SEEK_SET);
				n1 = fz_read(ctx, verify, buf1, read);
				same = (n0 == n1 && !memcmp(buf0, buf1, n0));
				off += (int64_t)n0;
			}
			while (same && n0 > 0 && off < len);

			if (same)
			{
				fz_seek_output(ctx, out, len, SEEK_SET);
				fz_truncate_output(ctx, out);
				break; /* return from try */
			}

			fz_seek_output(ctx, out, 0, SEEK_SET);
		}

		/* Copy old contents into new file */
		fz_seek(ctx, in, 0, SEEK_SET);
		off = 0;
		do
		{
			int64_t read = sizeof(buf0);
			if (off + read > len)
				read = len - off;
			n0 = fz_read(ctx, in, buf0, read);
			if (n0)
				fz_write_data(ctx, out, buf0, n0);
			off += n0;
		}
		while (n0 > 0 && off < len);

		if (verify)
		{
			fz_truncate_output(ctx, out);
			fz_seek_output(ctx, out, 0, SEEK_END);
		}
	}
	fz_always(ctx)
		fz_drop_stream(ctx, verify);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

#define OBJSTM_MAXOBJS 256
#define OBJSTM_MAXLEN 1<<24

typedef struct
{
	pdf_write_state *opts;
	int n;
	int objnum[OBJSTM_MAXOBJS];
	size_t len[OBJSTM_MAXOBJS];
	fz_buffer *content_buf;
	fz_output *content_out;
	int root_num;
	int info_num;
} objstm_gather_data;

static void
flush_gathered(fz_context *ctx, pdf_document *doc, objstm_gather_data *data)
{
	pdf_obj *obj;
	pdf_obj *ref = NULL;
	fz_buffer *newbuf = NULL;
	fz_output *out = NULL;
	int i;

	if (data->n == 0)
		return;

	obj = pdf_new_dict(ctx, doc, 4);

	fz_var(ref);
	fz_var(newbuf);
	fz_var(out);

	fz_try(ctx)
	{
		size_t pos = 0, first;
		int num;
		newbuf = fz_new_buffer(ctx, 128);

		out = fz_new_output_with_buffer(ctx, newbuf);

		for (i = 0; i < data->n; i++)
		{
			fz_write_printf(ctx, out, "%d %d ", data->objnum[i], pos);
			pos += data->len[i];
		}

		fz_close_output(ctx, out);
		first = fz_tell_output(ctx, out);
		fz_drop_output(ctx, out);
		out = NULL;

		pdf_dict_put_int(ctx, obj, PDF_NAME(First), first);
		pdf_dict_put_int(ctx, obj, PDF_NAME(N), data->n);
		pdf_dict_put(ctx, obj, PDF_NAME(Type), PDF_NAME(ObjStm));

		fz_close_output(ctx, data->content_out);
		fz_append_buffer(ctx, newbuf, data->content_buf);

		doc->xref_base = 0; /* Might have been reset by our caller */
		ref = pdf_add_object(ctx, doc, obj);
		pdf_update_stream(ctx, doc, ref, newbuf, 0);

		num = pdf_to_num(ctx, ref);
		expand_lists(ctx, data->opts, num);
		data->opts->use_list[num] = 1;

		/* Update all the xref entries for the objects to point into this stream. */
		for (i = 0; i < data->n; i++)
		{
			pdf_xref_entry *x = pdf_get_xref_entry_no_null(ctx, doc, data->objnum[i]);
			x->ofs = num; /* ofs = which objstm is this in */
			x->gen = i; /* gen = nth entry in the objstm */
			data->opts->ofs_list[data->objnum[i]] = i;
			data->opts->gen_list[data->objnum[i]] = i;
		}

		data->n = 0;
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, data->content_out);
		data->content_out = NULL;
		fz_drop_buffer(ctx, data->content_buf);
		data->content_buf = NULL;
		pdf_drop_obj(ctx, obj);
		pdf_drop_obj(ctx, ref);
		fz_drop_buffer(ctx, newbuf);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
objstm_gather(fz_context *ctx, pdf_xref_entry *x, int i, pdf_document *doc, void *arg)
{
	size_t olen, len;
	objstm_gather_data *data = (objstm_gather_data *)arg;

	/* If we are writing incrementally, then only the last one can be gathered. */
	if (data->opts->do_incremental && doc->xref_base != 0)
		return;

	if (i == data->root_num || i == data->info_num)
		return;

	/* Ensure the object is loaded! */
	if (i == 0)
		return; /* pdf_cache_object does not like being called for i == 0 which should be free. */
	pdf_cache_object(ctx, doc, i);

	if (x->type != 'n' || x->stm_buf != NULL || x->stm_ofs != 0 || x->gen != 0)
		return; /* Ineligible for using an objstm */

	/* FIXME: Can we do a pass through to check for such objects more exactly? */
	if (pdf_is_int(ctx, x->obj))
		return; /* In case it's a Length value. */
	if (pdf_is_indirect(ctx, x->obj))
		return; /* Bare indirect references are not allowed. */
	if (data->opts->do_linear && pdf_is_dict(ctx, x->obj))
	{
		pdf_obj *type = pdf_dict_get(ctx, x->obj, PDF_NAME(Type));
		if (pdf_name_eq(ctx, type, PDF_NAME(Pages)) ||
			pdf_name_eq(ctx, type, PDF_NAME(Page)))
			return;
	}

	if (data->content_buf == NULL)
		data->content_buf = fz_new_buffer(ctx, 128);
	if (data->content_out == NULL)
		data->content_out = fz_new_output_with_buffer(ctx, data->content_buf);

	olen = data->content_buf->len;
	pdf_print_encrypted_obj(ctx, data->content_out, x->obj, 1, 0, NULL, 0, 0);
	data->objnum[data->n] = i;
	len = data->content_buf->len;
	data->len[data->n] = len - olen;
	x->type = 'o';
	x->gen = data->n;
	data->n++;
	if (data->n == OBJSTM_MAXOBJS || len > OBJSTM_MAXLEN)
		flush_gathered(ctx, doc, data);
}

static void
gather_to_objstms(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int xref_len)
{
	objstm_gather_data data = { 0 };

	data.opts = opts;
	data.root_num = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)));
	data.info_num = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info)));

	pdf_xref_entry_map(ctx, doc, objstm_gather, &data);
	flush_gathered(ctx, doc, &data);
}

static void
do_pdf_save_document(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, const pdf_write_options *in_opts)
{
	int lastfree;
	int num;
	int xref_len;
	pdf_obj *id1, *id = NULL;

	if (in_opts->do_incremental)
	{
		ensure_initial_incremental_contents(ctx, doc->file, opts->out, doc->file_size);

		/* If no changes, nothing more to write */
		if (!pdf_has_unsaved_changes(ctx, doc))
		{
			doc->save_in_progress = 0;
			return;
		}

		fz_write_string(ctx, opts->out, "\n");
	}

	xref_len = pdf_xref_len(ctx, doc);

	pdf_begin_operation(ctx, doc, "Save document");
	fz_try(ctx)
	{
		initialise_write_state(ctx, doc, in_opts, opts);

		if (!opts->dont_regenerate_id)
		{
			/* Update second half of ID array if it exists. */
			id = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(ID));
			if (id)
				change_identity(ctx, doc, id);
		}

		/* Remove encryption dictionary if saving without encryption. */
		if (opts->do_encrypt == PDF_ENCRYPT_NONE)
		{
			assert(!in_opts->do_snapshot);
			pdf_dict_del(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt));
		}

		/* Keep encryption dictionary if saving with old encryption. */
		else if (opts->do_encrypt == PDF_ENCRYPT_KEEP)
		{
			opts->crypt = doc->crypt;
		}

		/* Create encryption dictionary if saving with new encryption. */
		else
		{
			assert(!opts->do_snapshot);
			if (!id)
				id = new_identity(ctx, doc);
			id1 = pdf_array_get(ctx, id, 0);
			opts->crypt = pdf_new_encrypt(ctx, opts->opwd_utf8, opts->upwd_utf8, id1, opts->permissions, opts->do_encrypt);
			create_encryption_dictionary(ctx, doc, opts->crypt);
		}

		/* Stash Encrypt entry in the writer state, in case a repair pass throws away the old trailer. */
		opts->crypt_obj = pdf_keep_obj(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt)));

		/* If we're writing a snapshot, we can't be doing garbage
		 * collection, or linearisation, and must be writing
		 * incrementally. */
		assert(!opts->do_snapshot || (opts->do_garbage == 0 && !opts->do_linear));

		/* Make sure any objects hidden in compressed streams have been loaded */
		if (!opts->do_incremental)
		{
			pdf_ensure_solid_xref(ctx, doc, xref_len);
			preloadobjstms(ctx, doc);
		}

		/* If we're using objstms, then the version must be at least 1.5 */
		if (opts->do_use_objstms && pdf_version(ctx, doc) < 15)
		{
			pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			pdf_obj *version = pdf_dict_get(ctx, root, PDF_NAME(Version));
			doc->version = 15;
			if (opts->do_incremental || version != NULL)
			{
				pdf_dict_put(ctx, root, PDF_NAME(Version), PDF_NAME(1_5));
			}
		}

		if (opts->do_preserve_metadata)
			opts->metadata = pdf_keep_obj(ctx, pdf_metadata(ctx, doc));

		xref_len = pdf_xref_len(ctx, doc); /* May have changed due to repair */
		expand_lists(ctx, opts, xref_len);

		/* Sweep & mark objects from the trailer */
		if (opts->do_garbage >= 1 || opts->do_linear)
			(void)markobj(ctx, doc, opts, pdf_trailer(ctx, doc));
		else
			for (num = 0; num < xref_len; num++)
				opts->use_list[num] = 1;

		/* Coalesce and renumber duplicate objects */
		if (opts->do_garbage >= 3)
			removeduplicateobjs(ctx, doc, opts);

		/* Compact xref by renumbering and removing unused objects */
		if (opts->do_garbage >= 2 || opts->do_linear)
			compactxref(ctx, doc, opts);

		opts->crypt_object_number = 0;
		if (opts->crypt)
		{
			pdf_obj *crypt = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt));
			int crypt_num = pdf_to_num(ctx, crypt);
			opts->crypt_object_number = opts->renumber_map[crypt_num];
		}

		/* Make renumbering affect all indirect references and update xref */
		if (opts->do_garbage >= 2 || opts->do_linear)
			renumberobjs(ctx, doc, opts);

		if (opts->do_use_objstms)
			gather_to_objstms(ctx, doc, opts, xref_len);

		xref_len = pdf_xref_len(ctx, doc); /* May have changed due to repair */
		expand_lists(ctx, opts, xref_len);

		/* Truncate the xref after compacting and renumbering */
		if ((opts->do_garbage >= 2 || opts->do_linear) &&
			!opts->do_incremental)
		{
			while (xref_len > 0 && !opts->use_list[xref_len-1])
				xref_len--;
		}

		if (opts->do_linear)
			linearize(ctx, doc, opts);

		if (opts->do_incremental)
		{
			int i;

			doc->disallow_new_increments = 1;

			for (i = 0; i < doc->num_incremental_sections; i++)
			{
				doc->xref_base = doc->num_incremental_sections - i - 1;
				xref_len = pdf_xref_len(ctx, doc);

				writeobjects(ctx, doc, opts, 0);

#ifdef DEBUG_WRITING
				dump_object_details(ctx, doc, opts);
#endif

				for (num = 0; num < xref_len; num++)
				{
					if (!opts->use_list[num] && pdf_xref_is_incremental(ctx, doc, num))
					{
						/* Make unreusable. FIXME: would be better to link to existing free list */
						opts->gen_list[num] = 65535;
						opts->ofs_list[num] = 0;
					}
				}

				opts->first_xref_offset = fz_tell_output(ctx, opts->out);
				if (!doc->last_xref_was_old_style || opts->do_use_objstms)
					writexrefstream(ctx, doc, opts, 0, xref_len, 1, 0, opts->first_xref_offset);
				else
					writexref(ctx, doc, opts, 0, xref_len, 1, 0, opts->first_xref_offset);

				doc->xref_sections[doc->xref_base].end_ofs = fz_tell_output(ctx, opts->out);
			}

			doc->xref_base = 0;
			doc->disallow_new_increments = 0;
		}
		else
		{
			writeobjects(ctx, doc, opts, 0);

#ifdef DEBUG_WRITING
			dump_object_details(ctx, doc, opts);
#endif

			/* Construct linked list of free object slots */
			lastfree = 0;
			for (num = 0; num < xref_len; num++)
			{
				if (!opts->use_list[num])
				{
					opts->gen_list[num]++;
					opts->ofs_list[lastfree] = num;
					lastfree = num;
				}
			}

			if (opts->do_linear && opts->page_count > 0)
			{
				opts->main_xref_offset = fz_tell_output(ctx, opts->out);
				writexref(ctx, doc, opts, 0, opts->start, 0, 0, opts->first_xref_offset);
				opts->file_len = fz_tell_output(ctx, opts->out);

				make_hint_stream(ctx, doc, opts);
				if (opts->do_ascii)
				{
					opts->hintstream_len *= 2;
					opts->hintstream_len += 1 + ((opts->hintstream_len+63)>>6);
				}
				opts->file_len += opts->hintstream_len;
				opts->main_xref_offset += opts->hintstream_len;
				update_linearization_params(ctx, doc, opts);
				fz_seek_output(ctx, opts->out, 0, 0);
				writeobjects(ctx, doc, opts, 1);

				padto(ctx, opts->out, opts->main_xref_offset);
				if (opts->do_use_objstms)
					writexrefstream(ctx, doc, opts, 0, xref_len, 1, 0, opts->first_xref_offset);
				else
					writexref(ctx, doc, opts, 0, opts->start, 0, 0, opts->first_xref_offset);
			}
			else
			{
				opts->first_xref_offset = fz_tell_output(ctx, opts->out);
				if (opts->do_use_objstms)
					writexrefstream(ctx, doc, opts, 0, xref_len, 1, 0, opts->first_xref_offset);
				else
					writexref(ctx, doc, opts, 0, xref_len, 1, 0, opts->first_xref_offset);
			}

			doc->xref_sections[0].end_ofs = fz_tell_output(ctx, opts->out);
		}

		if (!in_opts->do_snapshot)
		{
			complete_signatures(ctx, doc, opts);
		}
		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
	{
#ifdef DEBUG_LINEARIZATION
		page_objects_dump(opts);
		objects_dump(ctx, doc, opts);
#endif
		finalise_write_state(ctx, opts);
		if (opts->crypt != doc->crypt)
			pdf_drop_crypt(ctx, opts->crypt);
		pdf_drop_obj(ctx, opts->crypt_obj);
		pdf_drop_obj(ctx, opts->metadata);
		doc->save_in_progress = 0;
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
}

int pdf_has_unsaved_sigs(fz_context *ctx, pdf_document *doc)
{
	int s;
	for (s = 0; s < doc->num_incremental_sections; s++)
	{
		pdf_xref *xref = &doc->xref_sections[doc->num_incremental_sections - s - 1];

		if (xref->unsaved_sigs)
			return 1;
	}
	return 0;
}

void pdf_write_document(fz_context *ctx, pdf_document *doc, fz_output *out, const pdf_write_options *in_opts)
{
	pdf_write_options opts_defaults = pdf_default_write_options;
	pdf_write_state opts = { 0 };

	if (!doc || !out)
		return;

	if (!in_opts)
		in_opts = &opts_defaults;

	if (in_opts->do_incremental && doc->repair_attempted)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes on a repaired file");
	if (in_opts->do_incremental && in_opts->do_garbage)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes with garbage collection");
	if (in_opts->do_incremental && in_opts->do_linear)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes with linearisation");
	if (in_opts->do_incremental && in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes when changing encryption");
	if (in_opts->do_snapshot)
	{
		if (in_opts->do_incremental == 0 ||
			in_opts->do_pretty ||
			in_opts->do_ascii ||
			in_opts->do_compress ||
			in_opts->do_compress_images ||
			in_opts->do_compress_fonts ||
			in_opts->do_decompress ||
			in_opts->do_garbage ||
			in_opts->do_linear ||
			in_opts->do_clean ||
			in_opts->do_sanitize ||
			in_opts->do_appearance ||
			in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Can't use these options when snapshotting!");
	}
	if (pdf_has_unsaved_sigs(ctx, doc) && !fz_output_supports_stream(ctx, out))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't write pdf that has unsaved sigs to a fz_output unless it supports fz_stream_from_output!");

	prepare_for_save(ctx, doc, in_opts);

	opts.out = out;

	do_pdf_save_document(ctx, doc, &opts, in_opts);
}

void pdf_save_document(fz_context *ctx, pdf_document *doc, const char *filename, const pdf_write_options *in_opts)
{
	pdf_write_options opts_defaults = pdf_default_write_options;
	pdf_write_state opts = { 0 };

	if (!doc)
		return;

	if (!in_opts)
		in_opts = &opts_defaults;

	if (in_opts->do_incremental && !doc->file)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes on a new document");
	if (in_opts->do_incremental && doc->repair_attempted)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes on a repaired file");
	if (in_opts->do_incremental && in_opts->do_garbage)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes with garbage collection");
	if (in_opts->do_incremental && in_opts->do_linear)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes with linearisation");
	if (in_opts->do_incremental && in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't do incremental writes when changing encryption");
	if (in_opts->do_snapshot)
	{
		if (in_opts->do_incremental == 0 ||
			in_opts->do_pretty ||
			in_opts->do_ascii ||
			in_opts->do_compress ||
			in_opts->do_compress_images ||
			in_opts->do_compress_fonts ||
			in_opts->do_decompress ||
			in_opts->do_garbage ||
			in_opts->do_linear ||
			in_opts->do_clean ||
			in_opts->do_sanitize ||
			in_opts->do_appearance ||
			in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Can't use these options when snapshotting!");
	}

	if (in_opts->do_appearance > 0)
	{
		int i, n = pdf_count_pages(ctx, doc);
		for (i = 0; i < n; ++i)
		{
			pdf_page *page = pdf_load_page(ctx, doc, i);
			fz_try(ctx)
			{
				pdf_annot *annot;
				for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
					if (in_opts->do_appearance > 1)
						pdf_annot_request_resynthesis(ctx, annot);
					else
						pdf_annot_request_synthesis(ctx, annot);
				for (annot = pdf_first_widget(ctx, page); annot; annot = pdf_next_widget(ctx, annot))
					if (in_opts->do_appearance > 1)
						pdf_annot_request_resynthesis(ctx, annot);
					else
						pdf_annot_request_synthesis(ctx, annot);
				pdf_update_page(ctx, page);
			}
			fz_always(ctx)
				fz_drop_page(ctx, &page->super);
			fz_catch(ctx)
				fz_warn(ctx, "could not create annotation appearances");
		}
	}

	prepare_for_save(ctx, doc, in_opts);

	if (in_opts->do_incremental)
	{
		opts.out = fz_new_output_with_path(ctx, filename, 1);
	}
	else
	{
		opts.out = fz_new_output_with_path(ctx, filename, 0);
	}
	fz_try(ctx)
	{
		do_pdf_save_document(ctx, doc, &opts, in_opts);
		fz_close_output(ctx, opts.out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, opts.out);
		opts.out = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_save_snapshot(fz_context *ctx, pdf_document *doc, const char *filename)
{
	pdf_save_document(ctx, doc, filename, &pdf_snapshot_write_options);
}

void pdf_write_snapshot(fz_context *ctx, pdf_document *doc, fz_output *out)
{
	pdf_write_document(ctx, doc, out, &pdf_snapshot_write_options);
}

char *
pdf_format_write_options(fz_context *ctx, char *buffer, size_t buffer_len, const pdf_write_options *opts)
{
#define ADD_OPT(S) do { if (!first) fz_strlcat(buffer, ",", buffer_len); fz_strlcat(buffer, (S), buffer_len); first = 0; } while (0)

	int first = 1;
	*buffer = 0;
	if (opts->do_decompress)
		ADD_OPT("decompress=yes");
	if (opts->do_compress)
		ADD_OPT("compress=yes");
	if (opts->do_compress_fonts)
		ADD_OPT("compress-fonts=yes");
	if (opts->do_compress_images)
		ADD_OPT("compress-images=yes");
	if (opts->do_ascii)
		ADD_OPT("ascii=yes");
	if (opts->do_pretty)
		ADD_OPT("pretty=yes");
	if (opts->do_linear)
		ADD_OPT("linearize=yes");
	if (opts->do_clean)
		ADD_OPT("clean=yes");
	if (opts->do_sanitize)
		ADD_OPT("sanitize=yes");
	if (opts->do_incremental)
		ADD_OPT("incremental=yes");
	if (opts->do_encrypt == PDF_ENCRYPT_NONE)
		ADD_OPT("decrypt=yes");
	else if (opts->do_encrypt == PDF_ENCRYPT_KEEP)
		ADD_OPT("decrypt=no");
	switch(opts->do_encrypt)
	{
	default:
	case PDF_ENCRYPT_UNKNOWN:
		break;
	case PDF_ENCRYPT_NONE:
		ADD_OPT("encrypt=no");
		break;
	case PDF_ENCRYPT_KEEP:
		ADD_OPT("encrypt=keep");
		break;
	case PDF_ENCRYPT_RC4_40:
		ADD_OPT("encrypt=rc4-40");
		break;
	case PDF_ENCRYPT_RC4_128:
		ADD_OPT("encrypt=rc4-128");
		break;
	case PDF_ENCRYPT_AES_128:
		ADD_OPT("encrypt=aes-128");
		break;
	case PDF_ENCRYPT_AES_256:
		ADD_OPT("encrypt=aes-256");
		break;
	}
	if (strlen(opts->opwd_utf8)) {
		ADD_OPT("owner-password=");
		fz_strlcat(buffer, opts->opwd_utf8, buffer_len);
	}
	if (strlen(opts->upwd_utf8)) {
		ADD_OPT("user-password=");
		fz_strlcat(buffer, opts->upwd_utf8, buffer_len);
	}
	{
		char temp[32];
		ADD_OPT("permissions=");
		fz_snprintf(temp, sizeof(temp), "%d", opts->permissions);
		fz_strlcat(buffer, temp, buffer_len);
	}
	switch(opts->do_garbage)
	{
	case 0:
		break;
	case 1:
		ADD_OPT("garbage=yes");
		break;
	case 2:
		ADD_OPT("garbage=compact");
		break;
	case 3:
		ADD_OPT("garbage=deduplicate");
		break;
	default:
	{
		char temp[32];
		fz_snprintf(temp, sizeof(temp), "%d", opts->do_garbage);
		ADD_OPT("garbage=");
		fz_strlcat(buffer, temp, buffer_len);
		break;
	}
	}
	switch(opts->do_appearance)
	{
	case 1:
		ADD_OPT("appearance=yes");
		break;
	case 2:
		ADD_OPT("appearance=all");
		break;
	}

#undef ADD_OPT

	return buffer;
}

typedef struct
{
	fz_document_writer super;
	pdf_document *pdf;
	pdf_write_options opts;
	fz_output *out;

	fz_rect mediabox;
	pdf_obj *resources;
	fz_buffer *contents;
} pdf_writer;

static fz_device *
pdf_writer_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	pdf_writer *wri = (pdf_writer*)wri_;
	wri->mediabox = mediabox; // TODO: handle non-zero x0,y0
	return pdf_page_write(ctx, wri->pdf, wri->mediabox, &wri->resources, &wri->contents);
}

static void
pdf_writer_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	pdf_writer *wri = (pdf_writer*)wri_;
	pdf_obj *obj = NULL;

	fz_var(obj);

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		obj = pdf_add_page(ctx, wri->pdf, wri->mediabox, 0, wri->resources, wri->contents);
		pdf_insert_page(ctx, wri->pdf, -1, obj);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		pdf_drop_obj(ctx, obj);
		fz_drop_buffer(ctx, wri->contents);
		wri->contents = NULL;
		pdf_drop_obj(ctx, wri->resources);
		wri->resources = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_writer_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	pdf_writer *wri = (pdf_writer*)wri_;
	pdf_write_document(ctx, wri->pdf, wri->out, &wri->opts);
	fz_close_output(ctx, wri->out);
}

static void
pdf_writer_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	pdf_writer *wri = (pdf_writer*)wri_;
	fz_drop_buffer(ctx, wri->contents);
	pdf_drop_obj(ctx, wri->resources);
	pdf_drop_document(ctx, wri->pdf);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_pdf_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	pdf_writer *wri;

	fz_var(wri);

	fz_try(ctx)
	{
		wri = fz_new_derived_document_writer(ctx, pdf_writer, pdf_writer_begin_page, pdf_writer_end_page, pdf_writer_close_writer, pdf_writer_drop_writer);
		pdf_parse_write_options(ctx, &wri->opts, options);
		wri->out = out;
		wri->pdf = pdf_create_document(ctx);
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		pdf_drop_document(ctx, wri->pdf);
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_pdf_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.pdf", 0);
	return fz_new_pdf_writer_with_output(ctx, out, options);
}

void pdf_write_journal(fz_context *ctx, pdf_document *doc, fz_output *out)
{
	if (!doc || !out)
		return;

	if (!doc->journal)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't write non-existent journal");

	pdf_serialise_journal(ctx, doc, out);
}

void pdf_save_journal(fz_context *ctx, pdf_document *doc, const char *filename)
{
	fz_output *out;

	if (!doc)
		return;

	out = fz_new_output_with_path(ctx, filename, 0);
	fz_try(ctx)
	{
		pdf_write_journal(ctx, doc, out);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void pdf_read_journal(fz_context *ctx, pdf_document *doc, fz_stream *stm)
{
	pdf_deserialise_journal(ctx, doc, stm);
}

void pdf_load_journal(fz_context *ctx, pdf_document *doc, const char *filename)
{
	fz_stream *stm;

	if (!doc)
		return;

	stm = fz_open_file(ctx, filename);
	fz_try(ctx)
		pdf_read_journal(ctx, doc, stm);
	fz_always(ctx)
		fz_drop_stream(ctx, stm);
	fz_catch(ctx)
		fz_rethrow(ctx);
}
