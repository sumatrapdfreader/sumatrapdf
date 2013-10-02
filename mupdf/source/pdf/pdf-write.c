#include "mupdf/pdf.h"

/* #define DEBUG_LINEARIZATION */
/* #define DEBUG_HEAP_SORT */
/* #define DEBUG_WRITING */

typedef struct pdf_write_options_s pdf_write_options;

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

struct pdf_write_options_s
{
	FILE *out;
	int do_incremental;
	int do_ascii;
	int do_expand;
	int do_garbage;
	int do_linear;
	int *use_list;
	int *ofs_list;
	int *gen_list;
	int *renumber_map;
	int continue_on_error;
	int *errors;
	/* The following extras are required for linearization */
	int *rev_renumber_map;
	int *rev_gen_list;
	int start;
	int first_xref_offset;
	int main_xref_offset;
	int first_xref_entry_offset;
	int file_len;
	int hints_shared_offset;
	int hintstream_len;
	pdf_obj *linear_l;
	pdf_obj *linear_h0;
	pdf_obj *linear_h1;
	pdf_obj *linear_o;
	pdf_obj *linear_e;
	pdf_obj *linear_n;
	pdf_obj *linear_t;
	pdf_obj *hints_s;
	pdf_obj *hints_length;
	int page_count;
	page_objects_list *page_object_lists;
};

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
	*pol = fz_resize_array(ctx, *pol, 1, sizeof(page_objects_list) + (newcap-1)*sizeof(page_objects *));
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
		po = fz_resize_array(ctx, po, 1, sizeof(page_objects) + (po->cap*2 - 1)*sizeof(int));
		po->cap *= 2;
		*ppo = po;
	}
	po->object[po->len++] = i;
}

static void
page_objects_list_insert(fz_context *ctx, pdf_write_options *opts, int page, int object)
{
	page_objects_list_ensure(ctx, &opts->page_object_lists, page+1);
	if (opts->page_object_lists->len < page+1)
		opts->page_object_lists->len = page+1;
	page_objects_insert(ctx, &opts->page_object_lists->page[page], object);
}

static void
page_objects_list_set_page_object(fz_context *ctx, pdf_write_options *opts, int page, int object)
{
	page_objects_list_ensure(ctx, &opts->page_object_lists, page+1);
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
			/* If j is bigger than k (i.e. both of it's children),
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
	/* Put objects not associated with any page (anything
	 * not touched by the catalogue) before that... */
	else if (ui == 0)
		return 1;
	else if (uj == 0)
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
			/* If j is bigger than k (i.e. both of it's children),
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
page_objects_dump(pdf_write_options *opts)
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
objects_dump(pdf_document *doc, pdf_write_options *opts)
{
	int i;

	for (i=0; i < pdf_xref_len(doc); i++)
	{
		fprintf(stderr, "Object %d use=%x offset=%d\n", i, opts->use_list[i], opts->ofs_list[i]);
	}
}
#endif

/*
 * Garbage collect objects not reachable from the trailer.
 */

static pdf_obj *sweepref(pdf_document *doc, pdf_write_options *opts, pdf_obj *obj)
{
	int num = pdf_to_num(obj);
	int gen = pdf_to_gen(obj);
	fz_context *ctx = doc->ctx;

	if (num < 0 || num >= pdf_xref_len(doc))
		return NULL;
	if (opts->use_list[num])
		return NULL;

	opts->use_list[num] = 1;

	/* Bake in /Length in stream objects */
	fz_try(ctx)
	{
		if (pdf_is_stream(doc, num, gen))
		{
			pdf_obj *len = pdf_dict_gets(obj, "Length");
			if (pdf_is_indirect(len))
			{
				opts->use_list[pdf_to_num(len)] = 0;
				len = pdf_resolve_indirect(len);
				pdf_dict_puts(obj, "Length", len);
			}
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* Leave broken */
	}

	return pdf_resolve_indirect(obj);
}

static void sweepobj(pdf_document *doc, pdf_write_options *opts, pdf_obj *obj)
{
	int i;

	if (pdf_is_indirect(obj))
		obj = sweepref(doc, opts, obj);

	if (pdf_is_dict(obj))
	{
		int n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
			sweepobj(doc, opts, pdf_dict_get_val(obj, i));
	}

	else if (pdf_is_array(obj))
	{
		int n = pdf_array_len(obj);
		for (i = 0; i < n; i++)
			sweepobj(doc, opts, pdf_array_get(obj, i));
	}
}

/*
 * Scan for and remove duplicate objects (slow)
 */

static void removeduplicateobjs(pdf_document *doc, pdf_write_options *opts)
{
	int num, other;
	fz_context *ctx = doc->ctx;
	int xref_len = pdf_xref_len(doc);

	for (num = 1; num < xref_len; num++)
	{
		/* Only compare an object to objects preceding it */
		for (other = 1; other < num; other++)
		{
			pdf_obj *a, *b;
			int differ, newnum, streama, streamb;

			if (num == other || !opts->use_list[num] || !opts->use_list[other])
				continue;

			/*
			 * Comparing stream objects data contents would take too long.
			 *
			 * pdf_is_stream calls pdf_cache_object and ensures
			 * that the xref table has the objects loaded.
			 */
			fz_try(ctx)
			{
				streama = pdf_is_stream(doc, num, 0);
				streamb = pdf_is_stream(doc, other, 0);
				differ = streama || streamb;
				if (streama && streamb && opts->do_garbage >= 4)
					differ = 0;
			}
			fz_catch(ctx)
			{
				/* Assume different */
				differ = 1;
			}
			if (differ)
				continue;

			a = pdf_get_xref_entry(doc, num)->obj;
			b = pdf_get_xref_entry(doc, other)->obj;

			a = pdf_resolve_indirect(a);
			b = pdf_resolve_indirect(b);

			if (pdf_objcmp(a, b))
				continue;

			if (streama && streamb)
			{
				/* Check to see if streams match too. */
				fz_buffer *sa = NULL;
				fz_buffer *sb = NULL;

				fz_var(sa);
				fz_var(sb);

				differ = 1;
				fz_try(ctx)
				{
					unsigned char *dataa, *datab;
					int lena, lenb;
					sa = pdf_load_raw_renumbered_stream(doc, num, 0, num, 0);
					sb = pdf_load_raw_renumbered_stream(doc, other, 0, other, 0);
					lena = fz_buffer_storage(ctx, sa, &dataa);
					lenb = fz_buffer_storage(ctx, sb, &datab);
					if (lena == lenb && memcmp(dataa, datab, lena) == 0)
						differ = 0;
				}
				fz_always(ctx)
				{
					fz_drop_buffer(ctx, sa);
					fz_drop_buffer(ctx, sb);
				}
				fz_catch(ctx)
				{
					fz_rethrow(ctx);
				}
				if (differ)
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

static void compactxref(pdf_document *doc, pdf_write_options *opts)
{
	int num, newnum;
	int xref_len = pdf_xref_len(doc);

	/*
	 * Update renumber_map in-place, clustering all used
	 * objects together at low object ids. Objects that
	 * already should be renumbered will have their new
	 * object ids be updated to reflect the compaction.
	 */

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
			opts->rev_gen_list[newnum] = opts->rev_gen_list[num];
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

static void renumberobj(pdf_document *doc, pdf_write_options *opts, pdf_obj *obj)
{
	int i;

	if (pdf_is_dict(obj))
	{
		int n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(obj, i);
			pdf_obj *val = pdf_dict_get_val(obj, i);
			if (pdf_is_indirect(val))
			{
				val = pdf_new_indirect(doc, opts->renumber_map[pdf_to_num(val)], 0);
				pdf_dict_put(obj, key, val);
				pdf_drop_obj(val);
			}
			else
			{
				renumberobj(doc, opts, val);
			}
		}
	}

	else if (pdf_is_array(obj))
	{
		int n = pdf_array_len(obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *val = pdf_array_get(obj, i);
			if (pdf_is_indirect(val))
			{
				val = pdf_new_indirect(doc, opts->renumber_map[pdf_to_num(val)], 0);
				pdf_array_put(obj, i, val);
				pdf_drop_obj(val);
			}
			else
			{
				renumberobj(doc, opts, val);
			}
		}
	}
}

static void renumberobjs(pdf_document *doc, pdf_write_options *opts)
{
	pdf_xref_entry *newxref = NULL;
	int newlen;
	int num;
	fz_context *ctx = doc->ctx;
	int *new_use_list;
	int xref_len = pdf_xref_len(doc);

	new_use_list = fz_calloc(ctx, pdf_xref_len(doc)+3, sizeof(int));

	fz_var(newxref);
	fz_try(ctx)
	{
		/* Apply renumber map to indirect references in all objects in xref */
		renumberobj(doc, opts, pdf_trailer(doc));
		for (num = 0; num < xref_len; num++)
		{
			pdf_obj *obj = pdf_get_xref_entry(doc, num)->obj;

			if (pdf_is_indirect(obj))
			{
				obj = pdf_new_indirect(doc, opts->renumber_map[pdf_to_num(obj)], 0);
				pdf_update_object(doc, num, obj);
				pdf_drop_obj(obj);
			}
			else
			{
				renumberobj(doc, opts, obj);
			}
		}

		/* Create new table for the reordered, compacted xref */
		newxref = fz_malloc_array(ctx, xref_len + 3, sizeof(pdf_xref_entry));
		newxref[0] = *pdf_get_xref_entry(doc, 0);

		/* Move used objects into the new compacted xref */
		newlen = 0;
		for (num = 1; num < xref_len; num++)
		{
			if (opts->use_list[num])
			{
				pdf_xref_entry *e;
				if (newlen < opts->renumber_map[num])
					newlen = opts->renumber_map[num];
				e = pdf_get_xref_entry(doc, num);
				newxref[opts->renumber_map[num]] = *e;
				if (e->obj)
				{
					pdf_set_obj_parent(e->obj, opts->renumber_map[num]);
					e->obj = NULL;
				}
				new_use_list[opts->renumber_map[num]] = opts->use_list[num];
			}
			else
			{
				pdf_xref_entry *e = pdf_get_xref_entry(doc, num);
				pdf_drop_obj(e->obj);
				e->obj = NULL;
			}
		}

		pdf_replace_xref(doc, newxref, newlen + 1);
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

static void page_objects_list_renumber(pdf_write_options *opts)
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
mark_all(pdf_document *doc, pdf_write_options *opts, pdf_obj *val, int flag, int page)
{
	fz_context *ctx = doc->ctx;

	if (pdf_mark_obj(val))
		return;

	fz_try(ctx)
	{
		if (pdf_is_indirect(val))
		{
			int num = pdf_to_num(val);
			if (opts->use_list[num] & USE_PAGE_MASK)
				/* Already used */
				opts->use_list[num] |= USE_SHARED;
			else
				opts->use_list[num] |= flag;
			if (page >= 0)
				page_objects_list_insert(ctx, opts, page, num);
		}

		if (pdf_is_dict(val))
		{
			int i, n = pdf_dict_len(val);

			for (i = 0; i < n; i++)
			{
				mark_all(doc, opts, pdf_dict_get_val(val, i), flag, page);
			}
		}
		else if (pdf_is_array(val))
		{
			int i, n = pdf_array_len(val);

			for (i = 0; i < n; i++)
			{
				mark_all(doc, opts, pdf_array_get(val, i), flag, page);
			}
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(val);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static int
mark_pages(pdf_document *doc, pdf_write_options *opts, pdf_obj *val, int pagenum)
{
	fz_context *ctx = doc->ctx;

	if (pdf_mark_obj(val))
		return pagenum;

	fz_try(ctx)
	{
		if (pdf_is_dict(val))
		{
			if (!strcmp("Page", pdf_to_name(pdf_dict_gets(val, "Type"))))
			{
				int num = pdf_to_num(val);
				pdf_unmark_obj(val);
				mark_all(doc, opts, val, pagenum == 0 ? USE_PAGE1 : (pagenum<<USE_PAGE_SHIFT), pagenum);
				page_objects_list_set_page_object(ctx, opts, pagenum, num);
				pagenum++;
				opts->use_list[num] |= USE_PAGE_OBJECT;
			}
			else
			{
				int i, n = pdf_dict_len(val);

				for (i = 0; i < n; i++)
				{
					pdf_obj *key = pdf_dict_get_key(val, i);
					pdf_obj *obj = pdf_dict_get_val(val, i);

					if (!strcmp("Kids", pdf_to_name(key)))
						pagenum = mark_pages(doc, opts, obj, pagenum);
					else
						mark_all(doc, opts, obj, USE_CATALOGUE, -1);
				}

				if (pdf_is_indirect(val))
				{
					int num = pdf_to_num(val);
					opts->use_list[num] |= USE_CATALOGUE;
				}
			}
		}
		else if (pdf_is_array(val))
		{
			int i, n = pdf_array_len(val);

			for (i = 0; i < n; i++)
			{
				pagenum = mark_pages(doc, opts, pdf_array_get(val, i), pagenum);
			}
			if (pdf_is_indirect(val))
			{
				int num = pdf_to_num(val);
				opts->use_list[num] |= USE_CATALOGUE;
			}
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(val);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	return pagenum;
}

static void
mark_root(pdf_document *doc, pdf_write_options *opts, pdf_obj *dict)
{
	fz_context *ctx = doc->ctx;
	int i, n = pdf_dict_len(dict);

	if (pdf_mark_obj(dict))
		return;

	fz_try(ctx)
	{
		if (pdf_is_indirect(dict))
		{
			int num = pdf_to_num(dict);
			opts->use_list[num] |= USE_CATALOGUE;
		}

		for (i = 0; i < n; i++)
		{
			char *key = pdf_to_name(pdf_dict_get_key(dict, i));
			pdf_obj *val = pdf_dict_get_val(dict, i);

			if (!strcmp("Pages", key))
				opts->page_count = mark_pages(doc, opts, val, 0);
			else if (!strcmp("Names", key))
				mark_all(doc, opts, val, USE_OTHER_OBJECTS, -1);
			else if (!strcmp("Dests", key))
				mark_all(doc, opts, val, USE_OTHER_OBJECTS, -1);
			else if (!strcmp("Outlines", key))
			{
				int section;
				/* Look at PageMode to decide whether to
				 * USE_OTHER_OBJECTS or USE_PAGE1 here. */
				if (strcmp(pdf_to_name(pdf_dict_gets(dict, "PageMode")), "UseOutlines") == 0)
					section = USE_PAGE1;
				else
					section = USE_OTHER_OBJECTS;
				mark_all(doc, opts, val, section, -1);
			}
			else
				mark_all(doc, opts, val, USE_CATALOGUE, -1);
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(dict);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
mark_trailer(pdf_document *doc, pdf_write_options *opts, pdf_obj *dict)
{
	fz_context *ctx = doc->ctx;
	int i, n = pdf_dict_len(dict);

	if (pdf_mark_obj(dict))
		return;

	fz_try(ctx)
	{
		for (i = 0; i < n; i++)
		{
			char *key = pdf_to_name(pdf_dict_get_key(dict, i));
			pdf_obj *val = pdf_dict_get_val(dict, i);

			if (!strcmp("Root", key))
				mark_root(doc, opts, val);
			else
				mark_all(doc, opts, val, USE_CATALOGUE, -1);
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(dict);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
add_linearization_objs(pdf_document *doc, pdf_write_options *opts)
{
	pdf_obj *params_obj = NULL;
	pdf_obj *params_ref = NULL;
	pdf_obj *hint_obj = NULL;
	pdf_obj *hint_ref = NULL;
	pdf_obj *o = NULL;
	int params_num, hint_num;
	fz_context *ctx = doc->ctx;

	fz_var(params_obj);
	fz_var(params_ref);
	fz_var(hint_obj);
	fz_var(hint_ref);
	fz_var(o);

	fz_try(ctx)
	{
		/* Linearization params */
		params_obj = pdf_new_dict(doc, 10);
		params_ref = pdf_new_ref(doc, params_obj);
		params_num = pdf_to_num(params_ref);

		opts->use_list[params_num] = USE_PARAMS;
		opts->renumber_map[params_num] = params_num;
		opts->rev_renumber_map[params_num] = params_num;
		opts->gen_list[params_num] = 0;
		opts->rev_gen_list[params_num] = 0;
		pdf_dict_puts_drop(params_obj, "Linearized", pdf_new_real(doc, 1.0));
		opts->linear_l = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(params_obj, "L", opts->linear_l);
		opts->linear_h0 = pdf_new_int(doc, INT_MIN);
		o = pdf_new_array(doc, 2);
		pdf_array_push(o, opts->linear_h0);
		opts->linear_h1 = pdf_new_int(doc, INT_MIN);
		pdf_array_push(o, opts->linear_h1);
		pdf_dict_puts_drop(params_obj, "H", o);
		o = NULL;
		opts->linear_o = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(params_obj, "O", opts->linear_o);
		opts->linear_e = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(params_obj, "E", opts->linear_e);
		opts->linear_n = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(params_obj, "N", opts->linear_n);
		opts->linear_t = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(params_obj, "T", opts->linear_t);

		/* Primary hint stream */
		hint_obj = pdf_new_dict(doc, 10);
		hint_ref = pdf_new_ref(doc, hint_obj);
		hint_num = pdf_to_num(hint_ref);

		opts->use_list[hint_num] = USE_HINTS;
		opts->renumber_map[hint_num] = hint_num;
		opts->rev_renumber_map[hint_num] = hint_num;
		opts->gen_list[hint_num] = 0;
		opts->rev_gen_list[hint_num] = 0;
		pdf_dict_puts_drop(hint_obj, "P", pdf_new_int(doc, 0));
		opts->hints_s = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(hint_obj, "S", opts->hints_s);
		/* FIXME: Do we have thumbnails? Do a T entry */
		/* FIXME: Do we have outlines? Do an O entry */
		/* FIXME: Do we have article threads? Do an A entry */
		/* FIXME: Do we have named destinations? Do a E entry */
		/* FIXME: Do we have interactive forms? Do a V entry */
		/* FIXME: Do we have document information? Do an I entry */
		/* FIXME: Do we have logical structure heirarchy? Do a C entry */
		/* FIXME: Do L, Page Label hint table */
		pdf_dict_puts_drop(hint_obj, "Filter", pdf_new_name(doc, "FlateDecode"));
		opts->hints_length = pdf_new_int(doc, INT_MIN);
		pdf_dict_puts(hint_obj, "Length", opts->hints_length);
		pdf_get_xref_entry(doc, hint_num)->stm_ofs = -1;
	}
	fz_always(ctx)
	{
		pdf_drop_obj(params_obj);
		pdf_drop_obj(params_ref);
		pdf_drop_obj(hint_ref);
		pdf_drop_obj(hint_obj);
		pdf_drop_obj(o);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
lpr_inherit_res_contents(fz_context *ctx, pdf_obj *res, pdf_obj *dict, char *text)
{
	pdf_obj *o, *r;
	int i, n;

	/* If the parent node doesn't have an entry of this type, give up. */
	o = pdf_dict_gets(dict, text);
	if (!o)
		return;

	/* If the resources dict we are building doesn't have an entry of this
	 * type yet, then just copy it (ensuring it's not a reference) */
	r = pdf_dict_gets(res, text);
	if (r == NULL)
	{
		o = pdf_resolve_indirect(o);
		if (pdf_is_dict(o))
			o = pdf_copy_dict(o);
		else if (pdf_is_array(o))
			o = pdf_copy_array(o);
		else
			o = NULL;
		if (o)
			pdf_dict_puts(res, text, o);
		return;
	}

	/* Otherwise we need to merge o into r */
	if (pdf_is_dict(o))
	{
		n = pdf_dict_len(o);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(o, i);
			pdf_obj *val = pdf_dict_get_val(o, i);

			if (pdf_dict_gets(res, pdf_to_name(key)))
				continue;
			pdf_dict_puts(res, pdf_to_name(key), val);
		}
	}
}

static void
lpr_inherit_res(fz_context *ctx, pdf_obj *node, int depth, pdf_obj *dict)
{
	while (1)
	{
		pdf_obj *o;

		node = pdf_dict_gets(node, "Parent");
		depth--;
		if (!node || depth < 0)
			break;

		o = pdf_dict_gets(node, "Resources");
		if (o)
		{
			lpr_inherit_res_contents(ctx, dict, o, "ExtGState");
			lpr_inherit_res_contents(ctx, dict, o, "ColorSpace");
			lpr_inherit_res_contents(ctx, dict, o, "Pattern");
			lpr_inherit_res_contents(ctx, dict, o, "Shading");
			lpr_inherit_res_contents(ctx, dict, o, "XObject");
			lpr_inherit_res_contents(ctx, dict, o, "Font");
			lpr_inherit_res_contents(ctx, dict, o, "ProcSet");
			lpr_inherit_res_contents(ctx, dict, o, "Properties");
		}
	}
}

static pdf_obj *
lpr_inherit(fz_context *ctx, pdf_obj *node, char *text, int depth)
{
	do
	{
		pdf_obj *o = pdf_dict_gets(node, text);

		if (o)
			return pdf_resolve_indirect(o);
		node = pdf_dict_gets(node, "Parent");
		depth--;
	}
	while (depth >= 0 && node);

	return NULL;
}

static int
lpr(pdf_document *doc, pdf_obj *node, int depth, int page)
{
	pdf_obj *kids;
	pdf_obj *o = NULL;
	int i, n;
	fz_context *ctx = doc->ctx;

	if (pdf_mark_obj(node))
		return page;

	fz_var(o);

	fz_try(ctx)
	{
		if (!strcmp("Page", pdf_to_name(pdf_dict_gets(node, "Type"))))
		{
			pdf_obj *r; /* r is deliberately not cleaned up */

			/* Copy resources down to the child */
			o = pdf_keep_obj(pdf_dict_gets(node, "Resources"));
			if (!o)
			{
				o = pdf_keep_obj(pdf_new_dict(doc, 2));
				pdf_dict_puts(node, "Resources", o);
			}
			lpr_inherit_res(ctx, node, depth, o);
			r = lpr_inherit(ctx, node, "MediaBox", depth);
			if (r)
				pdf_dict_puts(node, "MediaBox", r);
			r = lpr_inherit(ctx, node, "CropBox", depth);
			if (r)
				pdf_dict_puts(node, "CropBox", r);
			r = lpr_inherit(ctx, node, "BleedBox", depth);
			if (r)
				pdf_dict_puts(node, "BleedBox", r);
			r = lpr_inherit(ctx, node, "TrimBox", depth);
			if (r)
				pdf_dict_puts(node, "TrimBox", r);
			r = lpr_inherit(ctx, node, "ArtBox", depth);
			if (r)
				pdf_dict_puts(node, "ArtBox", r);
			r = lpr_inherit(ctx, node, "Rotate", depth);
			if (r)
				pdf_dict_puts(node, "Rotate", r);
			page++;
		}
		else
		{
			kids = pdf_dict_gets(node, "Kids");
			n = pdf_array_len(kids);
			for(i = 0; i < n; i++)
			{
				page = lpr(doc, pdf_array_get(kids, i), depth+1, page);
			}
			pdf_dict_dels(node, "Resources");
			pdf_dict_dels(node, "MediaBox");
			pdf_dict_dels(node, "CropBox");
			pdf_dict_dels(node, "BleedBox");
			pdf_dict_dels(node, "TrimBox");
			pdf_dict_dels(node, "ArtBox");
			pdf_dict_dels(node, "Rotate");
		}
	}
	fz_always(ctx)
	{
		pdf_drop_obj(o);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	pdf_unmark_obj(node);

	return page;
}

void
pdf_localise_page_resources(pdf_document *doc)
{
	if (doc->resources_localised)
		return;

	lpr(doc, pdf_dict_getp(pdf_trailer(doc), "Root/Pages"), 0, 0);

	doc->resources_localised = 1;
}

static void
linearize(pdf_document *doc, pdf_write_options *opts)
{
	int i;
	int n = pdf_xref_len(doc) + 2;
	int *reorder;
	int *rev_renumber_map;
	int *rev_gen_list;
	fz_context *ctx = doc->ctx;

	opts->page_object_lists = page_objects_list_create(ctx);

	/* Ensure that every page has local references of its resources */
	/* FIXME: We could 'thin' the resources according to what is actually
	 * required for each page, but this would require us to run the page
	 * content streams. */
	pdf_localise_page_resources(doc);

	/* Walk the objects for each page, marking which ones are used, where */
	memset(opts->use_list, 0, n * sizeof(int));
	mark_trailer(doc, opts, pdf_trailer(doc));

	/* Add new objects required for linearization */
	add_linearization_objs(doc, opts);

#ifdef DEBUG_WRITING
	fprintf(stderr, "Usage calculated:\n");
	for (i=0; i < pdf_xref_len(doc); i++)
	{
		fprintf(stderr, "%d: use=%d\n", i, opts->use_list[i]);
	}
#endif

	/* Allocate/init the structures used for renumbering the objects */
	reorder = fz_calloc(ctx, n, sizeof(int));
	rev_renumber_map = fz_calloc(ctx, n, sizeof(int));
	rev_gen_list = fz_calloc(ctx, n, sizeof(int));
	for (i = 0; i < n; i++)
	{
		reorder[i] = i;
	}

	/* Heap sort the reordering */
	heap_sort(reorder+1, n-1, opts->use_list, &order_ge);

#ifdef DEBUG_WRITING
	fprintf(stderr, "Reordered:\n");
	for (i=1; i < pdf_xref_len(doc); i++)
	{
		fprintf(stderr, "%d: use=%d\n", i, opts->use_list[reorder[i]]);
	}
#endif

	/* Find the split point */
	for (i = 1; (opts->use_list[reorder[i]] & USE_PARAMS) == 0; i++);
	opts->start = i;

	/* Roll the reordering into the renumber_map */
	for (i = 0; i < n; i++)
	{
		opts->renumber_map[reorder[i]] = i;
		rev_renumber_map[i] = opts->rev_renumber_map[reorder[i]];
		rev_gen_list[i] = opts->rev_gen_list[reorder[i]];
	}
	fz_free(ctx, opts->rev_renumber_map);
	fz_free(ctx, opts->rev_gen_list);
	opts->rev_renumber_map = rev_renumber_map;
	opts->rev_gen_list = rev_gen_list;
	fz_free(ctx, reorder);

	/* Apply the renumber_map */
	page_objects_list_renumber(opts);
	renumberobjs(doc, opts);

	page_objects_list_sort_and_dedupe(ctx, opts->page_object_lists);
}

static void
update_linearization_params(pdf_document *doc, pdf_write_options *opts)
{
	int offset;
	pdf_set_int(opts->linear_l, opts->file_len);
	/* Primary hint stream offset (of object, not stream!) */
	pdf_set_int(opts->linear_h0, opts->ofs_list[pdf_xref_len(doc)-1]);
	/* Primary hint stream length (of object, not stream!) */
	offset = (opts->start == 1 ? opts->main_xref_offset : opts->ofs_list[1] + opts->hintstream_len);
	pdf_set_int(opts->linear_h1, offset - opts->ofs_list[pdf_xref_len(doc)-1]);
	/* Object number of first pages page object (the first object of page 0) */
	pdf_set_int(opts->linear_o, opts->page_object_lists->page[0]->object[0]);
	/* Offset of end of first page (first page is followed by primary
	 * hint stream (object n-1) then remaining pages (object 1...). The
	 * primary hint stream counts as part of the first pages data, I think.
	 */
	offset = (opts->start == 1 ? opts->main_xref_offset : opts->ofs_list[1] + opts->hintstream_len);
	pdf_set_int(opts->linear_e, offset);
	/* Number of pages in document */
	pdf_set_int(opts->linear_n, opts->page_count);
	/* Offset of first entry in main xref table */
	pdf_set_int(opts->linear_t, opts->first_xref_entry_offset + opts->hintstream_len);
	/* Offset of shared objects hint table in the primary hint stream */
	pdf_set_int(opts->hints_s, opts->hints_shared_offset);
	/* Primary hint stream length */
	pdf_set_int(opts->hints_length, opts->hintstream_len);
}

/*
 * Make sure we have loaded objects from object streams.
 */

static void preloadobjstms(pdf_document *doc)
{
	pdf_obj *obj;
	int num;
	int xref_len = pdf_xref_len(doc);

	for (num = 0; num < xref_len; num++)
	{
		if (pdf_get_xref_entry(doc, num)->type == 'o')
		{
			obj = pdf_load_object(doc, num, 0);
			pdf_drop_obj(obj);
		}
	}
}

/*
 * Save streams and objects to the output
 */

static inline int isbinary(int c)
{
	if (c == '\n' || c == '\r' || c == '\t')
		return 0;
	return c < 32 || c > 127;
}

static int isbinarystream(fz_buffer *buf)
{
	int i;
	for (i = 0; i < buf->len; i++)
		if (isbinary(buf->data[i]))
			return 1;
	return 0;
}

static fz_buffer *hexbuf(fz_context *ctx, unsigned char *p, int n)
{
	static const char hex[17] = "0123456789abcdef";
	fz_buffer *buf;
	int x = 0;

	buf = fz_new_buffer(ctx, n * 2 + (n / 32) + 2);

	while (n--)
	{
		buf->data[buf->len++] = hex[*p >> 4];
		buf->data[buf->len++] = hex[*p & 15];
		if (++x == 32)
		{
			buf->data[buf->len++] = '\n';
			x = 0;
		}
		p++;
	}

	buf->data[buf->len++] = '>';
	buf->data[buf->len++] = '\n';

	return buf;
}

static void addhexfilter(pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *f, *dp, *newf, *newdp;
	pdf_obj *ahx, *nullobj;

	ahx = pdf_new_name(doc, "ASCIIHexDecode");
	nullobj = pdf_new_null(doc);
	newf = newdp = NULL;

	f = pdf_dict_gets(dict, "Filter");
	dp = pdf_dict_gets(dict, "DecodeParms");

	if (pdf_is_name(f))
	{
		newf = pdf_new_array(doc, 2);
		pdf_array_push(newf, ahx);
		pdf_array_push(newf, f);
		f = newf;
		if (pdf_is_dict(dp))
		{
			newdp = pdf_new_array(doc, 2);
			pdf_array_push(newdp, nullobj);
			pdf_array_push(newdp, dp);
			dp = newdp;
		}
	}
	else if (pdf_is_array(f))
	{
		pdf_array_insert(f, ahx, 0);
		if (pdf_is_array(dp))
			pdf_array_insert(dp, nullobj, 0);
	}
	else
		f = ahx;

	pdf_dict_puts(dict, "Filter", f);
	if (dp)
		pdf_dict_puts(dict, "DecodeParms", dp);

	pdf_drop_obj(ahx);
	pdf_drop_obj(nullobj);
	pdf_drop_obj(newf);
	pdf_drop_obj(newdp);
}

static void copystream(pdf_document *doc, pdf_write_options *opts, pdf_obj *obj_orig, int num, int gen)
{
	fz_buffer *buf, *tmp;
	pdf_obj *newlen;
	pdf_obj *obj;
	fz_context *ctx = doc->ctx;
	int orig_num = opts->rev_renumber_map[num];
	int orig_gen = opts->rev_gen_list[num];

	buf = pdf_load_raw_renumbered_stream(doc, num, gen, orig_num, orig_gen);

	obj = pdf_copy_dict(obj_orig);
	if (opts->do_ascii && isbinarystream(buf))
	{
		tmp = hexbuf(ctx, buf->data, buf->len);
		fz_drop_buffer(ctx, buf);
		buf = tmp;

		addhexfilter(doc, obj);

		newlen = pdf_new_int(doc, buf->len);
		pdf_dict_puts(obj, "Length", newlen);
		pdf_drop_obj(newlen);
	}

	fprintf(opts->out, "%d %d obj\n", num, gen);
	pdf_fprint_obj(opts->out, obj, opts->do_expand == 0);
	fprintf(opts->out, "stream\n");
	fwrite(buf->data, 1, buf->len, opts->out);
	fprintf(opts->out, "endstream\nendobj\n\n");

	fz_drop_buffer(ctx, buf);
	pdf_drop_obj(obj);
}

static void expandstream(pdf_document *doc, pdf_write_options *opts, pdf_obj *obj_orig, int num, int gen)
{
	fz_buffer *buf, *tmp;
	pdf_obj *newlen;
	pdf_obj *obj;
	fz_context *ctx = doc->ctx;
	int orig_num = opts->rev_renumber_map[num];
	int orig_gen = opts->rev_gen_list[num];
	int truncated = 0;

	buf = pdf_load_renumbered_stream(doc, num, gen, orig_num, orig_gen, (opts->continue_on_error ? &truncated : NULL));
	if (truncated && opts->errors)
		(*opts->errors)++;

	obj = pdf_copy_dict(obj_orig);
	pdf_dict_dels(obj, "Filter");
	pdf_dict_dels(obj, "DecodeParms");

	if (opts->do_ascii && isbinarystream(buf))
	{
		tmp = hexbuf(ctx, buf->data, buf->len);
		fz_drop_buffer(ctx, buf);
		buf = tmp;

		addhexfilter(doc, obj);
	}

	newlen = pdf_new_int(doc, buf->len);
	pdf_dict_puts(obj, "Length", newlen);
	pdf_drop_obj(newlen);

	fprintf(opts->out, "%d %d obj\n", num, gen);
	pdf_fprint_obj(opts->out, obj, opts->do_expand == 0);
	fprintf(opts->out, "stream\n");
	fwrite(buf->data, 1, buf->len, opts->out);
	fprintf(opts->out, "endstream\nendobj\n\n");

	fz_drop_buffer(ctx, buf);
	pdf_drop_obj(obj);
}

static int is_image_filter(char *s)
{
	if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF") ||
		!strcmp(s, "DCTDecode") || !strcmp(s, "DCT") ||
		!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL") ||
		!strcmp(s, "JBIG2Decode") ||
		!strcmp(s, "JPXDecode"))
		return 1;
	return 0;
}

static int filter_implies_image(pdf_document *doc, pdf_obj *o)
{
	if (!o)
		return 0;
	if (pdf_is_name(o))
		return is_image_filter(pdf_to_name(o));
	if (pdf_is_array(o))
	{
		int i, len;
		len = pdf_array_len(o);
		for (i = 0; i < len; i++)
			if (is_image_filter(pdf_to_name(pdf_array_get(o, i))))
				return 1;
	}
	return 0;
}

static void writeobject(pdf_document *doc, pdf_write_options *opts, int num, int gen, int skip_xrefs)
{
	pdf_xref_entry *entry;
	pdf_obj *obj;
	pdf_obj *type;
	fz_context *ctx = doc->ctx;

	fz_try(ctx)
	{
		obj = pdf_load_object(doc, num, gen);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		if (opts->continue_on_error)
		{
			fprintf(opts->out, "%d %d obj\nnull\nendobj\n", num, gen);
			if (opts->errors)
				(*opts->errors)++;
			fz_warn(ctx, "%s", fz_caught_message(ctx));
			return;
		}
		else
			fz_rethrow(ctx);
	}

	/* skip ObjStm and XRef objects */
	if (pdf_is_dict(obj))
	{
		type = pdf_dict_gets(obj, "Type");
		if (pdf_is_name(type) && !strcmp(pdf_to_name(type), "ObjStm"))
		{
			opts->use_list[num] = 0;
			pdf_drop_obj(obj);
			return;
		}
		if (skip_xrefs && pdf_is_name(type) && !strcmp(pdf_to_name(type), "XRef"))
		{
			opts->use_list[num] = 0;
			pdf_drop_obj(obj);
			return;
		}
	}

	entry = pdf_get_xref_entry(doc, num);
	if (!pdf_is_stream(doc, num, gen))
	{
		fprintf(opts->out, "%d %d obj\n", num, gen);
		pdf_fprint_obj(opts->out, obj, opts->do_expand == 0);
		fprintf(opts->out, "endobj\n\n");
	}
	else if (entry->stm_ofs < 0 && entry->stm_buf == NULL)
	{
		fprintf(opts->out, "%d %d obj\n", num, gen);
		pdf_fprint_obj(opts->out, obj, opts->do_expand == 0);
		fprintf(opts->out, "stream\nendstream\nendobj\n\n");
	}
	else
	{
		int dontexpand = 0;
		if (opts->do_expand != 0 && opts->do_expand != fz_expand_all)
		{
			pdf_obj *o;

			if ((o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "XObject")) &&
				(o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "Image")))
				dontexpand = !(opts->do_expand & fz_expand_images);
			if (o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "Font"))
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Type"), !strcmp(pdf_to_name(o), "FontDescriptor"))
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (pdf_dict_gets(obj, "Length1") != NULL)
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (pdf_dict_gets(obj, "Length2") != NULL)
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (pdf_dict_gets(obj, "Length3") != NULL)
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "Type1C"))
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Subtype"), !strcmp(pdf_to_name(o), "CIDFontType0C"))
				dontexpand = !(opts->do_expand & fz_expand_fonts);
			if (o = pdf_dict_gets(obj, "Filter"), filter_implies_image(doc, o))
				dontexpand = !(opts->do_expand & fz_expand_images);
			if (pdf_dict_gets(obj, "Width") != NULL && pdf_dict_gets(obj, "Height") != NULL)
				dontexpand = !(opts->do_expand & fz_expand_images);
		}
		fz_try(ctx)
		{
			if (opts->do_expand && !dontexpand && !pdf_is_jpx_image(ctx, obj))
				expandstream(doc, opts, obj, num, gen);
			else
				copystream(doc, opts, obj, num, gen);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			if (opts->continue_on_error)
			{
				fprintf(opts->out, "%d %d obj\nnull\nendobj\n", num, gen);
				if (opts->errors)
					(*opts->errors)++;
				fz_warn(ctx, "%s", fz_caught_message(ctx));
			}
			else
			{
				pdf_drop_obj(obj);
				fz_rethrow(ctx);
			}
		}
	}

	pdf_drop_obj(obj);
}

static void writexrefsubsect(pdf_write_options *opts, int from, int to)
{
	int num;

	fprintf(opts->out, "%d %d\n", from, to - from);
	for (num = from; num < to; num++)
	{
		if (opts->use_list[num])
			fprintf(opts->out, "%010d %05d n \n", opts->ofs_list[num], opts->gen_list[num]);
		else
			fprintf(opts->out, "%010d %05d f \n", opts->ofs_list[num], opts->gen_list[num]);
	}
}

static void writexref(pdf_document *doc, pdf_write_options *opts, int from, int to, int first, int main_xref_offset, int startxref)
{
	pdf_obj *trailer = NULL;
	pdf_obj *obj;
	pdf_obj *nobj = NULL;
	fz_context *ctx = doc->ctx;

	fprintf(opts->out, "xref\n");
	opts->first_xref_entry_offset = ftell(opts->out);

	if (opts->do_incremental)
	{
		int subfrom = from;
		int subto;

		while (subfrom < to)
		{
			while (subfrom < to && !pdf_xref_is_incremental(doc, subfrom))
				subfrom++;

			subto = subfrom;
			while (subto < to && pdf_xref_is_incremental(doc, subto))
				subto++;

			if (subfrom < subto)
				writexrefsubsect(opts, subfrom, subto);

			subfrom = subto;
		}
	}
	else
	{
		writexrefsubsect(opts, from, to);
	}

	fprintf(opts->out, "\n");

	fz_var(trailer);
	fz_var(nobj);

	fz_try(ctx)
	{
		if (opts->do_incremental)
		{
			trailer = pdf_keep_obj(pdf_trailer(doc));
			pdf_dict_puts_drop(trailer, "Size", pdf_new_int(doc, pdf_xref_len(doc)));
			pdf_dict_puts_drop(trailer, "Prev", pdf_new_int(doc, doc->startxref));
			doc->startxref = startxref;
		}
		else
		{
			trailer = pdf_new_dict(doc, 5);

			nobj = pdf_new_int(doc, to);
			pdf_dict_puts(trailer, "Size", nobj);
			pdf_drop_obj(nobj);
			nobj = NULL;

			if (first)
			{
				obj = pdf_dict_gets(pdf_trailer(doc), "Info");
				if (obj)
					pdf_dict_puts(trailer, "Info", obj);

				obj = pdf_dict_gets(pdf_trailer(doc), "Root");
				if (obj)
					pdf_dict_puts(trailer, "Root", obj);

				obj = pdf_dict_gets(pdf_trailer(doc), "ID");
				if (obj)
					pdf_dict_puts(trailer, "ID", obj);
			}
			if (main_xref_offset != 0)
			{
				nobj = pdf_new_int(doc, main_xref_offset);
				pdf_dict_puts(trailer, "Prev", nobj);
				pdf_drop_obj(nobj);
				nobj = NULL;
			}
		}
	}
	fz_always(ctx)
	{
		pdf_drop_obj(nobj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	fprintf(opts->out, "trailer\n");
	pdf_fprint_obj(opts->out, trailer, opts->do_expand == 0);
	fprintf(opts->out, "\n");

	pdf_drop_obj(trailer);

	fprintf(opts->out, "startxref\n%d\n%%%%EOF\n", startxref);

	doc->has_xref_streams = 0;
}

static void writexrefstreamsubsect(pdf_document *doc, pdf_write_options *opts, pdf_obj *index, fz_buffer *fzbuf, int from, int to)
{
	int num;

	pdf_array_push_drop(index, pdf_new_int(doc, from));
	pdf_array_push_drop(index, pdf_new_int(doc, to - from));
	for (num = from; num < to; num++)
	{
		fz_write_buffer_byte(doc->ctx, fzbuf, opts->use_list[num] ? 1 : 0);
		fz_write_buffer_byte(doc->ctx, fzbuf, opts->ofs_list[num]>>24);
		fz_write_buffer_byte(doc->ctx, fzbuf, opts->ofs_list[num]>>16);
		fz_write_buffer_byte(doc->ctx, fzbuf, opts->ofs_list[num]>>8);
		fz_write_buffer_byte(doc->ctx, fzbuf, opts->ofs_list[num]);
		fz_write_buffer_byte(doc->ctx, fzbuf, opts->gen_list[num]);
	}
}

static void writexrefstream(pdf_document *doc, pdf_write_options *opts, int from, int to, int first, int main_xref_offset, int startxref)
{
	fz_context *ctx = doc->ctx;
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
		num = pdf_create_object(doc);
		dict = pdf_new_dict(doc, 6);
		pdf_update_object(doc, num, dict);

		opts->first_xref_entry_offset = ftell(opts->out);

		to++;

		if (first)
		{
			obj = pdf_dict_gets(pdf_trailer(doc), "Info");
			if (obj)
				pdf_dict_puts(dict, "Info", obj);

			obj = pdf_dict_gets(pdf_trailer(doc), "Root");
			if (obj)
				pdf_dict_puts(dict, "Root", obj);

			obj = pdf_dict_gets(pdf_trailer(doc), "ID");
			if (obj)
				pdf_dict_puts(dict, "ID", obj);

			if (opts->do_incremental)
			{
				obj = pdf_dict_gets(pdf_trailer(doc), "Encrypt");
				if (obj)
					pdf_dict_puts(dict, "Encrypt", obj);
			}
		}

		pdf_dict_puts_drop(dict, "Size", pdf_new_int(doc, to));

		if (opts->do_incremental)
		{
			pdf_dict_puts_drop(dict, "Prev", pdf_new_int(doc, doc->startxref));
			doc->startxref = startxref;
		}
		else
		{
			if (main_xref_offset != 0)
				pdf_dict_puts_drop(dict, "Prev", pdf_new_int(doc, main_xref_offset));
		}

		pdf_dict_puts_drop(dict, "Type", pdf_new_name(doc, "XRef"));

		w = pdf_new_array(doc, 3);
		pdf_dict_puts(dict, "W", w);
		pdf_array_push_drop(w, pdf_new_int(doc, 1));
		pdf_array_push_drop(w, pdf_new_int(doc, 4));
		pdf_array_push_drop(w, pdf_new_int(doc, 1));

		index = pdf_new_array(doc, 2);
		pdf_dict_puts_drop(dict, "Index", index);

		opts->ofs_list[num] = opts->first_xref_entry_offset;

		fzbuf = fz_new_buffer(ctx, 4*(to-from));

		if (opts->do_incremental)
		{
			int subfrom = from;
			int subto;

			while (subfrom < to)
			{
				while (subfrom < to && !pdf_xref_is_incremental(doc, subfrom))
					subfrom++;

				subto = subfrom;
				while (subto < to && pdf_xref_is_incremental(doc, subto))
					subto++;

				if (subfrom < subto)
					writexrefstreamsubsect(doc, opts, index, fzbuf, subfrom, subto);

				subfrom = subto;
			}
		}
		else
		{
			writexrefstreamsubsect(doc, opts, index, fzbuf, from, to);
		}

		pdf_update_stream(doc, num, fzbuf);
		pdf_dict_puts_drop(dict, "Length", pdf_new_int(doc, fz_buffer_storage(ctx, fzbuf, NULL)));

		writeobject(doc, opts, num, 0, 0);
		fprintf(opts->out, "startxref\n%d\n%%%%EOF\n", startxref);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(dict);
		pdf_drop_obj(w);
		fz_drop_buffer(ctx, fzbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
padto(FILE *file, int target)
{
	int pos = ftell(file);

	assert(pos <= target);
	while (pos < target)
	{
		fputc('\n', file);
		pos++;
	}
}

static void
dowriteobject(pdf_document *doc, pdf_write_options *opts, int num, int pass)
{
	pdf_xref_entry *entry = pdf_get_xref_entry(doc, num);
	if (entry->type == 'f')
		opts->gen_list[num] = entry->gen;
	if (entry->type == 'n')
		opts->gen_list[num] = entry->gen;
	if (entry->type == 'o')
		opts->gen_list[num] = 0;

	/* If we are renumbering, then make sure all generation numbers are
	 * zero (except object 0 which must be free, and have a gen number of
	 * 65535). Changing the generation numbers (and indeed object numbers)
	 * will break encryption - so only do this if we are renumbering
	 * anyway. */
	if (opts->do_garbage >= 2)
		opts->gen_list[num] = (num == 0 ? 65535 : 0);

	if (opts->do_garbage && !opts->use_list[num])
		return;

	if (entry->type == 'n' || entry->type == 'o')
	{
		if (pass > 0)
			padto(opts->out, opts->ofs_list[num]);
		opts->ofs_list[num] = ftell(opts->out);
		if (!opts->do_incremental || pdf_xref_is_incremental(doc, num))
			writeobject(doc, opts, num, opts->gen_list[num], 1);
	}
	else
		opts->use_list[num] = 0;
}

static void
writeobjects(pdf_document *doc, pdf_write_options *opts, int pass)
{
	int num;
	int xref_len = pdf_xref_len(doc);

	if (!opts->do_incremental)
	{
		fprintf(opts->out, "%%PDF-%d.%d\n", doc->version / 10, doc->version % 10);
		fprintf(opts->out, "%%\316\274\341\277\246\n\n");
	}

	dowriteobject(doc, opts, opts->start, pass);

	if (opts->do_linear)
	{
		/* Write first xref */
		if (pass == 0)
			opts->first_xref_offset = ftell(opts->out);
		else
			padto(opts->out, opts->first_xref_offset);
		writexref(doc, opts, opts->start, pdf_xref_len(doc), 1, opts->main_xref_offset, 0);
	}

	for (num = opts->start+1; num < xref_len; num++)
		dowriteobject(doc, opts, num, pass);
	if (opts->do_linear && pass == 1)
	{
		int offset = (opts->start == 1 ? opts->main_xref_offset : opts->ofs_list[1] + opts->hintstream_len);
		padto(opts->out, offset);
	}
	for (num = 1; num < opts->start; num++)
	{
		if (pass == 1)
			opts->ofs_list[num] += opts->hintstream_len;
		dowriteobject(doc, opts, num, pass);
	}
}

static int
my_log2(int x)
{
	int i = 0;

	if (x <= 0)
		return 0;

	while ((1<<i) <= x && (1<<i) > 0)
		i++;

	if ((1<<i) <= 0)
		return 0;

	return i;
}

static void
make_page_offset_hints(pdf_document *doc, pdf_write_options *opts, fz_buffer *buf)
{
	fz_context *ctx = doc->ctx;
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
	int xref_len = pdf_xref_len(doc);

	min_shared_object = pdf_xref_len(doc);
	max_shared_object = 1;
	min_shared_length = opts->file_len;
	max_shared_length = 0;
	for (i=1; i < xref_len; i++)
	{
		int min, max, page;

		min = opts->ofs_list[i];
		if (i == opts->start-1 || (opts->start == 1 && i == xref_len-1))
			max = opts->main_xref_offset;
		else if (i == xref_len-1)
			max = opts->ofs_list[1];
		else
			max = opts->ofs_list[i+1];

		/* SumatraPDF: TODO: this assertion doesn't always hold (e.g. for files 1044 and 1103) */
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
	fz_write_buffer_bits(ctx, buf, min_objs_per_page, 32);
	/* Header Item 2: Location of first pages page object */
	fz_write_buffer_bits(ctx, buf, opts->ofs_list[pop[0]->page_object_number], 32);
	/* Header Item 3: Number of bits required to represent the difference
	 * between the greatest and least number of objects in a page. */
	objs_per_page_bits = my_log2(max_objs_per_page - min_objs_per_page);
	fz_write_buffer_bits(ctx, buf, objs_per_page_bits, 16);
	/* Header Item 4: Least length of a page. */
	fz_write_buffer_bits(ctx, buf, min_page_length, 32);
	/* Header Item 5: Number of bits needed to represent the difference
	 * between the greatest and least length of a page. */
	page_len_bits = my_log2(max_page_length - min_page_length);
	fz_write_buffer_bits(ctx, buf, page_len_bits, 16);
	/* Header Item 6: Least offset to start of content stream (Acrobat
	 * sets this to always be 0) */
	fz_write_buffer_bits(ctx, buf, 0, 32);
	/* Header Item 7: Number of bits needed to represent the difference
	 * between the greatest and least offset to content stream (Acrobat
	 * sets this to always be 0) */
	fz_write_buffer_bits(ctx, buf, 0, 16);
	/* Header Item 8: Least content stream length. (Acrobat
	 * sets this to always be 0) */
	fz_write_buffer_bits(ctx, buf, 0, 32);
	/* Header Item 9: Number of bits needed to represent the difference
	 * between the greatest and least content stream length (Acrobat
	 * sets this to always be the same as item 5) */
	fz_write_buffer_bits(ctx, buf, page_len_bits, 16);
	/* Header Item 10: Number of bits needed to represent the greatest
	 * number of shared object references. */
	shared_object_bits = my_log2(max_shared_object_refs);
	fz_write_buffer_bits(ctx, buf, shared_object_bits, 16);
	/* Header Item 11: Number of bits needed to represent the greatest
	 * shared object identifier. */
	shared_object_id_bits = my_log2(max_shared_object - min_shared_object + pop[0]->num_shared);
	fz_write_buffer_bits(ctx, buf, shared_object_id_bits, 16);
	/* Header Item 12: Number of bits needed to represent the numerator
	 * of the fractions. We always send 0. */
	fz_write_buffer_bits(ctx, buf, 0, 16);
	/* Header Item 13: Number of bits needed to represent the denominator
	 * of the fractions. We always send 0. */
	fz_write_buffer_bits(ctx, buf, 0, 16);

	/* Table F.4 - Page offset hint table (per page) */
	/* Item 1: A number that, when added to the least number of objects
	 * on a page, gives the number of objects in the page. */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_write_buffer_bits(ctx, buf, pop[i]->num_objects - min_objs_per_page, objs_per_page_bits);
	}
	fz_write_buffer_pad(ctx, buf);
	/* Item 2: A number that, when added to the least page length, gives
	 * the length of the page in bytes. */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_write_buffer_bits(ctx, buf, pop[i]->max_ofs - pop[i]->min_ofs - min_page_length, page_len_bits);
	}
	fz_write_buffer_pad(ctx, buf);
	/* Item 3: The number of shared objects referenced from the page. */
	for (i = 0; i < opts->page_count; i++)
	{
		fz_write_buffer_bits(ctx, buf, pop[i]->num_shared, shared_object_bits);
	}
	fz_write_buffer_pad(ctx, buf);
	/* Item 4: Shared object id for each shared object ref in every page.
	 * Spec says "not for page 1", but acrobat does send page 1's - all
	 * as zeros. */
	for (i = 0; i < opts->page_count; i++)
	{
		for (j = 0; j < pop[i]->len; j++)
		{
			int o = pop[i]->object[j];
			if (i == 0 && opts->use_list[o] & USE_PAGE1)
				fz_write_buffer_bits(ctx, buf, 0 /* o - pop[0]->page_object_number */, shared_object_id_bits);
			if (i != 0 && opts->use_list[o] & USE_SHARED)
				fz_write_buffer_bits(ctx, buf, o - min_shared_object + pop[0]->num_shared, shared_object_id_bits);
		}
	}
	fz_write_buffer_pad(ctx, buf);
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
		fz_write_buffer_bits(ctx, buf, pop[i]->max_ofs - pop[i]->min_ofs - min_page_length, page_len_bits);
	}

	/* Pad, and then do shared object hint table */
	fz_write_buffer_pad(ctx, buf);
	opts->hints_shared_offset = buf->len;

	/* Table F.5: */
	/* Header Item 1: Object number of the first object in the shared
	 * objects section. */
	fz_write_buffer_bits(ctx, buf, min_shared_object, 32);
	/* Header Item 2: Location of first object in the shared objects
	 * section. */
	fz_write_buffer_bits(ctx, buf, opts->ofs_list[min_shared_object], 32);
	/* Header Item 3: The number of shared object entries for the first
	 * page. */
	fz_write_buffer_bits(ctx, buf, pop[0]->num_shared, 32);
	/* Header Item 4: The number of shared object entries for the shared
	 * objects section + first page. */
	fz_write_buffer_bits(ctx, buf, max_shared_object - min_shared_object + pop[0]->num_shared, 32);
	/* Header Item 5: The number of bits needed to represent the greatest
	 * number of objects in a shared object group (Always 0). */
	fz_write_buffer_bits(ctx, buf, 0, 16);
	/* Header Item 6: The least length of a shared object group in bytes. */
	fz_write_buffer_bits(ctx, buf, min_shared_length, 32);
	/* Header Item 7: The number of bits required to represent the
	 * difference between the greatest and least length of a shared object
	 * group. */
	shared_length_bits = my_log2(max_shared_length - min_shared_length);
	fz_write_buffer_bits(ctx, buf, shared_length_bits, 16);

	/* Table F.6 */
	/* Item 1: Shared object group length (page 1 objects) */
	for (j = 0; j < pop[0]->len; j++)
	{
		int o = pop[0]->object[j];
		int min, max;
		min = opts->ofs_list[o];
		if (o == opts->start-1)
			max = opts->main_xref_offset;
		else if (o < xref_len-1)
			max = opts->ofs_list[o+1];
		else
			max = opts->ofs_list[1];
		if (opts->use_list[o] & USE_PAGE1)
			fz_write_buffer_bits(ctx, buf, max - min - min_shared_length, shared_length_bits);
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
		fz_write_buffer_bits(ctx, buf, max - min - min_shared_length, shared_length_bits);
	}
	fz_write_buffer_pad(ctx, buf);

	/* Item 2: MD5 presence flags */
	for (i = max_shared_object - min_shared_object + pop[0]->num_shared; i > 0; i--)
	{
		fz_write_buffer_bits(ctx, buf, 0, 1);
	}
	fz_write_buffer_pad(ctx, buf);
	/* Item 3: MD5 sums (not present) */
	fz_write_buffer_pad(ctx, buf);
	/* Item 4: Number of objects in the group (not present) */
}

static void
make_hint_stream(pdf_document *doc, pdf_write_options *opts)
{
	fz_context *ctx = doc->ctx;
	fz_buffer *buf = fz_new_buffer(ctx, 100);

	fz_try(ctx)
	{
		make_page_offset_hints(doc, opts, buf);
		pdf_update_stream(doc, pdf_xref_len(doc)-1, buf);
		opts->hintstream_len = buf->len;
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
}

#ifdef DEBUG_WRITING
static void dump_object_details(pdf_document *doc, pdf_write_options *opts)
{
	int i;

	for (i = 0; i < pdf_xref_len(doc); i++)
	{
		fprintf(stderr, "%d@%d: use=%d\n", i, opts->ofs_list[i], opts->use_list[i]);
	}
}
#endif

static void presize_unsaved_signature_byteranges(pdf_document *doc)
{
	if (doc->unsaved_sigs)
	{
		/* The ByteRange objects of signatures are initially written out with
		 * dummy values, and then overwritten later. We need to make sure their
		 * initial form at least takes enough sufficient file space */
		pdf_unsaved_sig *usig;
		int n = 0;

		for (usig = doc->unsaved_sigs; usig; usig = usig->next)
			n++;

		for (usig = doc->unsaved_sigs; usig; usig = usig->next)
		{
			/* There will be segments of bytes at the beginning, at
			 * the end and between each consecutive pair of signatures,
			 * hence n + 1 */
			int i;
			pdf_obj *byte_range = pdf_dict_getp(usig->field, "V/ByteRange");

			for (i = 0; i < n+1; i++)
			{
				pdf_array_push_drop(byte_range, pdf_new_int(doc, INT_MAX));
				pdf_array_push_drop(byte_range, pdf_new_int(doc, INT_MAX));
			}
		}
	}
}

static void complete_signatures(pdf_document *doc, pdf_write_options *opts, char *filename)
{
	fz_context *ctx = doc->ctx;
	pdf_unsaved_sig *usig;
	FILE *f;
	char buf[5120];
	int i;
	int flen;
	int last_end;

	if (doc->unsaved_sigs)
	{
		pdf_obj *byte_range;

		f = fopen(filename, "rb+");
		if (!f)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to open %s to complete signatures", filename);

		fseek(f, 0, SEEK_END);
		flen = ftell(f);

		/* Locate the byte ranges and contents in the saved file */
		for (usig = doc->unsaved_sigs; usig; usig = usig->next)
		{
			char *bstr, *cstr, *fstr;
			int pnum = pdf_obj_parent_num(pdf_dict_getp(usig->field, "V/ByteRange"));
			fseek(f, opts->ofs_list[pnum], SEEK_SET);
			(void)fread(buf, 1, sizeof(buf), f);
			buf[sizeof(buf)-1] = 0;

			bstr = strstr(buf, "/ByteRange");
			cstr = strstr(buf, "/Contents");
			fstr = strstr(buf, "/Filter");

			if (bstr && cstr && fstr && bstr < cstr && cstr < fstr)
			{
				usig->byte_range_start = bstr - buf + 10 + opts->ofs_list[pnum];
				usig->byte_range_end = cstr - buf + opts->ofs_list[pnum];
				usig->contents_start = cstr - buf + 9 + opts->ofs_list[pnum];
				usig->contents_end = fstr - buf + opts->ofs_list[pnum];
			}
		}

		/* Recreate ByteRange with correct values. Initially store the
		 * recreated object in the first of the unsaved signatures */
		byte_range = pdf_new_array(doc, 4);
		pdf_dict_putp_drop(doc->unsaved_sigs->field, "V/ByteRange", byte_range);

		last_end = 0;
		for (usig = doc->unsaved_sigs; usig; usig = usig->next)
		{
			pdf_array_push_drop(byte_range, pdf_new_int(doc, last_end));
			pdf_array_push_drop(byte_range, pdf_new_int(doc, usig->contents_start - last_end));
			last_end = usig->contents_end;
		}
		pdf_array_push_drop(byte_range, pdf_new_int(doc, last_end));
		pdf_array_push_drop(byte_range, pdf_new_int(doc, flen - last_end));

		/* Copy the new ByteRange to the other unsaved signatures */
		for (usig = doc->unsaved_sigs->next; usig; usig = usig->next)
			pdf_dict_putp_drop(usig->field, "V/ByteRange", pdf_copy_array(byte_range));

		/* Write the byte range into buf, padding with spaces*/
		i = pdf_sprint_obj(buf, sizeof(buf), byte_range, 1);
		memset(buf+i, ' ', sizeof(buf)-i);

		/* Write the byte range to the file */
		for (usig = doc->unsaved_sigs; usig; usig = usig->next)
		{
			fseek(f, usig->byte_range_start, SEEK_SET);
			fwrite(buf, 1, usig->byte_range_end - usig->byte_range_start, f);
		}

		fclose(f);

		/* Write the digests into the file */
		for (usig = doc->unsaved_sigs; usig; usig = usig->next)
			pdf_write_digest(doc, filename, byte_range, usig->contents_start, usig->contents_end - usig->contents_start, usig->signer);

		/* delete the unsaved_sigs records */
		while ((usig = doc->unsaved_sigs) != NULL)
		{
			doc->unsaved_sigs = usig->next;
			pdf_drop_obj(usig->field);
			pdf_drop_signer(usig->signer);
			fz_free(ctx, usig);
		}
	}
}

void pdf_write_document(pdf_document *doc, char *filename, fz_write_options *fz_opts)
{
	int lastfree;
	int num;
	pdf_write_options opts = { 0 };
	fz_context *ctx;
	int xref_len;
	fz_write_options fz_opts_defaults = { 0 };

	if (!doc)
		return;

	if (!fz_opts)
		fz_opts = &fz_opts_defaults;

	doc->freeze_updates = 1;
	ctx = doc->ctx;

	pdf_finish_edit(doc);
	presize_unsaved_signature_byteranges(doc);

	xref_len = pdf_xref_len(doc);

	if (fz_opts->do_incremental)
	{
		opts.out = fopen(filename, "ab");
		if (opts.out)
			fseek(opts.out, 0, SEEK_END);
	}
	else
	{
		opts.out = fopen(filename, "wb");
	}

	if (!opts.out)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open output file '%s'", filename);

	fz_try(ctx)
	{
		opts.do_incremental = fz_opts->do_incremental;
		opts.do_expand = fz_opts->do_expand;
		opts.do_garbage = fz_opts->do_garbage;
		opts.do_ascii = fz_opts->do_ascii;
		opts.do_linear = fz_opts->do_linear;
		opts.start = 0;
		opts.main_xref_offset = INT_MIN;
		/* We deliberately make these arrays long enough to cope with
		 * 1 to n access rather than 0..n-1, and add space for 2 new
		 * extra entries that may be required for linearization. */
		opts.use_list = fz_malloc_array(ctx, pdf_xref_len(doc) + 3, sizeof(int));
		opts.ofs_list = fz_malloc_array(ctx, pdf_xref_len(doc) + 3, sizeof(int));
		opts.gen_list = fz_calloc(ctx, pdf_xref_len(doc) + 3, sizeof(int));
		opts.renumber_map = fz_malloc_array(ctx, pdf_xref_len(doc) + 3, sizeof(int));
		opts.rev_renumber_map = fz_malloc_array(ctx, pdf_xref_len(doc) + 3, sizeof(int));
		opts.rev_gen_list = fz_malloc_array(ctx, pdf_xref_len(doc) + 3, sizeof(int));
		opts.continue_on_error = fz_opts->continue_on_error;
		opts.errors = fz_opts->errors;

		for (num = 0; num < xref_len; num++)
		{
			opts.use_list[num] = 0;
			opts.ofs_list[num] = 0;
			opts.renumber_map[num] = num;
			opts.rev_renumber_map[num] = num;
			opts.rev_gen_list[num] = pdf_get_xref_entry(doc, num)->gen;
		}

		/* Make sure any objects hidden in compressed streams have been loaded */
		if (!opts.do_incremental)
			preloadobjstms(doc);

		/* Sweep & mark objects from the trailer */
		if (opts.do_garbage >= 1)
			sweepobj(doc, &opts, pdf_trailer(doc));
		else
			for (num = 0; num < xref_len; num++)
				opts.use_list[num] = 1;

		/* Coalesce and renumber duplicate objects */
		if (opts.do_garbage >= 3)
			removeduplicateobjs(doc, &opts);

		/* Compact xref by renumbering and removing unused objects */
		if (opts.do_garbage >= 2 || opts.do_linear)
			compactxref(doc, &opts);

		/* Make renumbering affect all indirect references and update xref */
		if (opts.do_garbage >= 2 || opts.do_linear)
			renumberobjs(doc, &opts);

		if (opts.do_linear)
		{
			linearize(doc, &opts);
		}

		writeobjects(doc, &opts, 0);

#ifdef DEBUG_WRITING
		dump_object_details(doc, &opts);
#endif

		if (opts.do_incremental)
		{
			for (num = 0; num < xref_len; num++)
			{
				if (!opts.use_list[num] && pdf_xref_is_incremental(doc, num))
				{
					/* Make unreusable. FIXME: would be better to link to existing free list */
					opts.gen_list[num] = 65535;
					opts.ofs_list[num] = 0;
				}
			}
		}
		else
		{
			/* Construct linked list of free object slots */
			lastfree = 0;
			for (num = 0; num < xref_len; num++)
			{
				if (!opts.use_list[num])
				{
					opts.gen_list[num]++;
					opts.ofs_list[lastfree] = num;
					lastfree = num;
				}
			}
		}

		if (opts.do_linear)
		{
			opts.main_xref_offset = ftell(opts.out);
			writexref(doc, &opts, 0, opts.start, 0, 0, opts.first_xref_offset);
			opts.file_len = ftell(opts.out);

			make_hint_stream(doc, &opts);
			opts.file_len += opts.hintstream_len;
			opts.main_xref_offset += opts.hintstream_len;
			update_linearization_params(doc, &opts);
			fseek(opts.out, 0, 0);
			writeobjects(doc, &opts, 1);

			padto(opts.out, opts.main_xref_offset);
			writexref(doc, &opts, 0, opts.start, 0, 0, opts.first_xref_offset);
		}
		else
		{
			opts.first_xref_offset = ftell(opts.out);
			if (opts.do_incremental && doc->has_xref_streams)
				writexrefstream(doc, &opts, 0, xref_len, 1, 0, opts.first_xref_offset);
			else
				writexref(doc, &opts, 0, xref_len, 1, 0, opts.first_xref_offset);
		}

		fclose(opts.out);
		opts.out = NULL;
		complete_signatures(doc, &opts, filename);

		doc->dirty = 0;
	}
	fz_always(ctx)
	{
#ifdef DEBUG_LINEARIZATION
		page_objects_dump(&opts);
		objects_dump(doc, &opts);
#endif
		fz_free(ctx, opts.use_list);
		fz_free(ctx, opts.ofs_list);
		fz_free(ctx, opts.gen_list);
		fz_free(ctx, opts.renumber_map);
		fz_free(ctx, opts.rev_renumber_map);
		fz_free(ctx, opts.rev_gen_list);
		pdf_drop_obj(opts.linear_l);
		pdf_drop_obj(opts.linear_h0);
		pdf_drop_obj(opts.linear_h1);
		pdf_drop_obj(opts.linear_o);
		pdf_drop_obj(opts.linear_e);
		pdf_drop_obj(opts.linear_n);
		pdf_drop_obj(opts.linear_t);
		pdf_drop_obj(opts.hints_s);
		pdf_drop_obj(opts.hints_length);
		page_objects_list_destroy(ctx, opts.page_object_lists);
		if (opts.out)
			fclose(opts.out);
		doc->freeze_updates = 0;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

#define KIDS_PER_LEVEL 32

#if 0

// TODO: pdf_rebalance_page_tree(doc);

static pdf_obj *
make_page_tree_node(pdf_document *doc, int l, int r, pdf_obj *parent_ref, int root)
{
	fz_context *ctx = doc->ctx;
	int count_per_kid, spaces;
	pdf_obj *a = NULL;
	pdf_obj *me = NULL;
	pdf_obj *o = NULL;
	pdf_obj *me_ref = NULL;

	count_per_kid = 1;
	while(count_per_kid * KIDS_PER_LEVEL < r-l)
		count_per_kid *= KIDS_PER_LEVEL;

	fz_var(o);
	fz_var(me);
	fz_var(a);
	fz_var(me_ref);

	fz_try(ctx)
	{
		me = pdf_new_dict(doc, 2);
		pdf_dict_puts_drop(me, "Type", pdf_new_name(doc, "Pages"));
		pdf_dict_puts_drop(me, "Count", pdf_new_int(doc, r-l));
		if (!root)
			pdf_dict_puts(me, "Parent", parent_ref);
		a = pdf_new_array(doc, KIDS_PER_LEVEL);
		me_ref = pdf_new_ref(doc, me);

		for (spaces = KIDS_PER_LEVEL; l < r; spaces--)
		{
			if (spaces >= r-l)
			{
				o = pdf_keep_obj(doc->page_refs[l++]);
				pdf_dict_puts(o, "Parent", me_ref);
			}
			else
			{
				int j = l+count_per_kid;
				if (j > r)
					j = r;
				o = make_page_tree_node(doc, l, j, me_ref, 0);
				l = j;
			}
			pdf_array_push(a, o);
			pdf_drop_obj(o);
			o = NULL;
		}
		pdf_dict_puts_drop(me, "Kids", a);
		a = NULL;
	}
	fz_always(ctx)
	{
		pdf_drop_obj(me);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(a);
		pdf_drop_obj(o);
		pdf_drop_obj(me);
		fz_rethrow_message(ctx, "Failed to synthesize new page tree");
	}
	return me_ref;
}

static void
pdf_rebalance_page_tree(pdf_document *doc)
{
	pdf_obj *catalog;
	pdf_obj *pages;

	if (!doc || !doc->needs_page_tree_rebuild)
		return;

	catalog = pdf_dict_gets(pdf_trailer(doc), "Root");
	pages = make_page_tree_node(doc, 0, doc->page_len, catalog, 1);
	pdf_dict_puts_drop(catalog, "Pages", pages);

	doc->needs_page_tree_rebuild = 0;
}

#endif

static void
pdf_rebalance_page_tree(pdf_document *doc)
{
}

void pdf_finish_edit(pdf_document *doc)
{
	if (!doc)
		return;
	pdf_rebalance_page_tree(doc);
}
