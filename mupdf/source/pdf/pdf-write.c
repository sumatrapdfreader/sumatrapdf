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
#include "pdf-annot-imp.h"

#include <zlib.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

#include <stdio.h> /* for debug printing */
/* #define DEBUG_HEAP_SORT */
/* #define DEBUG_WRITING */
/* #define DEBUG_MARK_AND_SWEEP */

#define SIG_EXTRAS_SIZE (1024)

#define SLASH_BYTE_RANGE ("/ByteRange")
#define SLASH_CONTENTS ("/Contents")
#define SLASH_FILTER ("/Filter")

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

	pdf_object_labels *labels;
	int num_labels;
	char *obj_labels[100];

	int bias; /* when saving incrementally to a file with garbage before the version marker */

	int crypt_object_number;
	char opwd_utf8[128];
	char upwd_utf8[128];
	int permissions;
	pdf_crypt *crypt;
	pdf_obj *crypt_obj;
	pdf_obj *metadata;
} pdf_write_state;

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

	for (i = opts->list_len; i < num; i++)
	{
		opts->use_list[i] = 0;
		opts->ofs_list[i] = 0;
		opts->gen_list[i] = 0;
		opts->renumber_map[i] = i;
	}
	opts->list_len = num;
}

/*
 * Garbage collect objects not reachable from the trailer.
 */

static void bake_stream_length(fz_context *ctx, pdf_document *doc, int num)
{
	if (pdf_obj_num_is_stream(ctx, doc, num))
	{
		pdf_obj *len;
		pdf_obj *obj = NULL;
		fz_var(obj);
		fz_try(ctx)
		{
			obj = pdf_load_object(ctx, doc, num);
			len = pdf_dict_get(ctx, obj, PDF_NAME(Length));
			if (pdf_is_indirect(ctx, len))
				pdf_dict_put_int(ctx, obj, PDF_NAME(Length), pdf_to_int(ctx, len));
		}
		fz_always(ctx)
			pdf_drop_obj(ctx, obj);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

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

static int removeduplicateobjs(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int num, other;
	int xref_len = pdf_xref_len(ctx, doc);
	int changed = 0;

	expand_lists(ctx, opts, xref_len);
	for (num = 1; num < xref_len; num++)
	{
		pdf_obj *a;

		if (num >= opts->list_len || !opts->use_list[num])
			continue;

		a = pdf_get_xref_entry_no_null(ctx, doc, num)->obj;

		/* Only compare an object to objects preceding it */
		for (other = 1; other < num; other++)
		{
			pdf_obj *b;
			int newnum;

			if (!opts->use_list[other])
				continue;

			/* TODO: resolve indirect references to see if we can omit them */

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

			/* Never common up pages! */
			if (pdf_name_eq(ctx, pdf_dict_get(ctx, a, PDF_NAME(Type)), PDF_NAME(Page)))
				continue;

			/* Keep the lowest numbered object */
			newnum = fz_mini(num, other);
			opts->renumber_map[num] = newnum;
			opts->renumber_map[other] = newnum;
			opts->use_list[fz_maxi(num, other)] = 0;

			/* One duplicate was found, do not look for another */
			changed = 1;
			break;
		}
	}

	return changed;
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

static void
renumber_stored_object_ref(fz_context *ctx, pdf_obj **objp, pdf_write_state *opts, pdf_document *doc, int xref_len)
{
	int o;
	pdf_obj *obj;

	if (objp == NULL || !pdf_is_indirect(ctx, *objp))
		return;

	o = pdf_to_num(ctx, *objp);
	if (o >= xref_len || o <= 0 || opts->renumber_map[o] == 0)
		obj = PDF_NULL;
	else
		obj = pdf_new_indirect(ctx, doc, opts->renumber_map[o], 0);
	pdf_drop_obj(ctx, *objp);
	*objp = obj;
}

static void renumberobjs(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	pdf_xref_entry *newxref = NULL;
	int newlen;
	int num;
	int *new_use_list;
	int xref_len = pdf_xref_len(ctx, doc);

	expand_lists(ctx, opts, xref_len);
	new_use_list = fz_calloc(ctx, opts->list_len, sizeof(int));

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
				int o = pdf_to_num(ctx, obj);
				if (o >= xref_len || o <= 0 || opts->renumber_map[o] == 0)
					obj = PDF_NULL;
				else
					obj = pdf_new_indirect(ctx, doc, opts->renumber_map[o], 0);
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

		/* Now walk the open pages, renumbering the page objects there. */
		{
			fz_page *page;

			for (page = doc->super.open; page != NULL; page = page->next)
			{
				if (page->doc == NULL)
					continue;
				renumber_stored_object_ref(ctx, &((pdf_page*)page)->obj, opts, doc, xref_len);
			}
		}

		/* And the OCGs store several. Just drop 'em all. */
		pdf_drop_ocg(ctx, doc);
		doc->ocg = NULL;

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
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
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
		fz_throw(ctx, FZ_ERROR_LIMIT, "Buffer too large to deflate");

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
		fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot deflate buffer");
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
			else if (do_deflate == 1)
			{
				tmp_comp = deflatebuf(ctx, data, len, opts->compression_effort);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(FlateDecode));
				pdf_dict_del(ctx, obj, PDF_NAME(DecodeParms));
			}
			else
			{
				size_t comp_len;
				int mode = (opts->compression_effort == 0 ? FZ_BROTLI_DEFAULT :
					FZ_BROTLI_BEST * opts->compression_effort / 100);
				unsigned char *comp_data = fz_new_brotli_data(ctx, &comp_len, data, len, mode);
				tmp_comp = fz_new_buffer_from_data(ctx, comp_data, comp_len);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(BrotliDecode));
				pdf_dict_del(ctx, obj, PDF_NAME(DecodeParms));
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
			pdf_print_encrypted_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii, opts->crypt, num, gen, NULL);
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
			else if (do_deflate == 1)
			{
				tmp_comp = deflatebuf(ctx, data, len, opts->compression_effort);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(FlateDecode));
			}
			else
			{
				size_t comp_len;
				int mode = (opts->compression_effort == 0 ? FZ_BROTLI_DEFAULT :
					FZ_BROTLI_BEST * opts->compression_effort / 100);
				unsigned char *comp_data = fz_new_brotli_data(ctx, &comp_len, data, len, mode);
				tmp_comp = fz_new_buffer_from_data(ctx, comp_data, comp_len);
				pdf_dict_put(ctx, obj, PDF_NAME(Filter), PDF_NAME(BrotliDecode));
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
			pdf_print_encrypted_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii, opts->crypt, num, gen, NULL);
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

int pdf_is_image_stream(fz_context *ctx, pdf_obj *obj)
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

static void writelabel(fz_context *ctx, void *arg, const char *label)
{
	pdf_write_state *opts = arg;
	if (opts->num_labels < (int)nelem(opts->obj_labels))
		opts->obj_labels[opts->num_labels++] = fz_strdup(ctx, label);
}

static int labelcmp(const void *aa, const void *bb)
{
	return fz_strverscmp(*(const char **)aa, *(const char **)bb);
}

static void writeobject(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int num, int gen, int skip_xrefs, int unenc)
{
	pdf_obj *obj = NULL;
	fz_buffer *buf = NULL;
	int do_deflate = 0;
	int do_expand = 0;
	int skip = 0;
	int i;

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
			if (opts->labels)
			{
				opts->num_labels = 0;
				pdf_label_object(ctx, opts->labels, num, writelabel, opts);
				if (opts->num_labels == 0)
				{
					fz_write_string(ctx, opts->out, "% unused\n");
				}
				else
				{
					qsort(opts->obj_labels, opts->num_labels, sizeof(char*), labelcmp);
					for (i = 0; i < opts->num_labels; ++i)
					{
						fz_write_printf(ctx, opts->out, "%% %s\n", opts->obj_labels[i]);
						fz_free(ctx, opts->obj_labels[i]);
						opts->obj_labels[i] = NULL;
					}
				}
			}

			if (pdf_obj_num_is_stream(ctx, doc, num))
			{
				do_deflate = opts->do_compress;
				do_expand = opts->do_expand;
				if (opts->do_compress_images && pdf_is_image_stream(ctx, obj))
					do_deflate = opts->do_compress ? opts->do_compress : 1, do_expand = 0;
				if (opts->do_compress_fonts && is_font_stream(ctx, obj))
					do_deflate = opts->do_compress ? opts->do_compress : 1, do_expand = 0;
				if (is_xml_metadata(ctx, obj))
					do_deflate = 0, do_expand = 0;
				if (is_jpx_stream(ctx, obj))
					do_deflate = 0, do_expand = 0;

				if (do_expand)
					expandstream(ctx, doc, opts, obj, num, gen, do_deflate, unenc);
				else
					copystream(ctx, doc, opts, obj, num, gen, do_deflate, unenc);
			}
			else
			{
				fz_write_printf(ctx, opts->out, "%d %d obj\n", num, gen);
				pdf_print_encrypted_obj(ctx, opts->out, obj, opts->do_tight, opts->do_ascii, unenc ? NULL : opts->crypt, num, gen, NULL);
				fz_write_string(ctx, opts->out, "\nendobj\n\n");
			}
		}
	}
	fz_always(ctx)
	{
		for (i = 0; i < opts->num_labels; ++i)
		{
			fz_free(ctx, opts->obj_labels[i]);
			opts->obj_labels[i] = NULL;
		}
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
			fz_write_printf(ctx, opts->out, "%010lu %05d n \n", opts->ofs_list[num] - opts->bias, opts->gen_list[num]);
		else
			fz_write_printf(ctx, opts->out, "%010lu %05d f \n", opts->ofs_list[num] - opts->bias, opts->gen_list[num]);
	}
}

static void writexref(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int from, int to, int first, int64_t startxref)
{
	pdf_obj *trailer = NULL;
	pdf_obj *obj;

	fz_write_string(ctx, opts->out, "xref\n");

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
			pdf_dict_del(ctx, trailer, PDF_NAME(XRefStm));
			if (!opts->do_snapshot)
				doc->startxref = startxref - opts->bias;
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

				/* The encryption dictionary is kept in the writer state to handle
				   the encryption dictionary object being renumbered during repair.*/
				if (opts->crypt_obj)
				{
					/* If the encryption dictionary used to be an indirect reference from the trailer,
					   store it the same way in the trailer in the saved file. */
					if (pdf_is_indirect(ctx, opts->crypt_obj))
						pdf_dict_put_indirect(ctx, trailer, PDF_NAME(Encrypt), opts->crypt_object_number);
					else
						pdf_dict_put(ctx, trailer, PDF_NAME(Encrypt), opts->crypt_obj);
				}

				if (opts->metadata)
					pdf_dict_putp(ctx, trailer, "Root/Metadata", opts->metadata);
			}
		}

		fz_write_string(ctx, opts->out, "trailer\n");
		/* Trailer is NOT encrypted */
		pdf_print_obj(ctx, opts->out, trailer, opts->do_tight, opts->do_ascii);
		fz_write_string(ctx, opts->out, "\n");

		fz_write_printf(ctx, opts->out, "startxref\n%lu\n%%%%EOF\n", startxref - opts->bias);

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
			f2 = opts->ofs_list[num] - opts->bias;
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

static void writexrefstream(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int from, int to, int first, int64_t startxref)
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

			/* The encryption dictionary is kept in the writer state to handle
			   the encryption dictionary object being renumbered during repair.*/
			if (opts->crypt_obj)
			{
				/* If the encryption dictionary used to be an indirect reference from the trailer,
				   store it the same way in the xref stream in the saved file. */
				if (pdf_is_indirect(ctx, opts->crypt_obj))
					pdf_dict_put_indirect(ctx, dict, PDF_NAME(Encrypt), opts->crypt_object_number);
				else
					pdf_dict_put(ctx, dict, PDF_NAME(Encrypt), opts->crypt_obj);
			}
		}

		pdf_dict_put_int(ctx, dict, PDF_NAME(Size), to);

		if (opts->do_incremental)
		{
			pdf_dict_put_int(ctx, dict, PDF_NAME(Prev), doc->startxref);
			if (!opts->do_snapshot)
				doc->startxref = startxref - opts->bias;
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
		opts->ofs_list[num] = startxref;

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
		fz_write_printf(ctx, opts->out, "startxref\n%lu\n%%%%EOF\n", startxref - opts->bias);

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
dowriteobject(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, int num)
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
writeobjects(fz_context *ctx, pdf_document *doc, pdf_write_state *opts)
{
	int num;
	int xref_len = pdf_xref_len(ctx, doc);

	if (!opts->do_incremental)
	{
		int version = pdf_version(ctx, doc);
		fz_write_printf(ctx, opts->out, "%%PDF-%d.%d\n", version / 10, version % 10);
		fz_write_string(ctx, opts->out, "%\xC2\xB5\xC2\xB6\n");
	}

#ifdef CLUSTER
	fz_write_string(ctx, opts->out, "% Written by MuPDF CLUSTER\n\n");
#else
	fz_write_string(ctx, opts->out, "% Written by MuPDF " FZ_VERSION "\n\n");
#endif

	for (num = 0; num < xref_len; num++)
		dowriteobject(ctx, doc, opts, num);
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
						fz_throw(ctx, FZ_ERROR_FORMAT, "Failed to determine byte ranges while writing signature");

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

static void clean_content_streams(fz_context *ctx, pdf_document *doc, int sanitize, int ascii, int newlines)
{
	int n = pdf_count_pages(ctx, doc);
	int i;

	pdf_filter_options options = { 0 };
	pdf_sanitize_filter_options sopts = { 0 };
	pdf_filter_factory list[2] = { 0 };

	options.recurse = 1;
	options.ascii = ascii;
	options.newlines = newlines;
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
	opts->do_clean = in_opts->do_clean;
	opts->do_encrypt = in_opts->do_encrypt;
	opts->dont_regenerate_id = in_opts->dont_regenerate_id;
	opts->do_preserve_metadata = in_opts->do_preserve_metadata;
	opts->do_use_objstms = in_opts->do_use_objstms;

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

	expand_lists(ctx, opts, xref_len);
}

/* Free the resources held by the dynamic write options */
static void finalise_write_state(fz_context *ctx, pdf_write_state *opts)
{
	fz_free(ctx, opts->use_list);
	fz_free(ctx, opts->ofs_list);
	fz_free(ctx, opts->gen_list);
	fz_free(ctx, opts->renumber_map);
	pdf_drop_object_labels(ctx, opts->labels);
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
	"\tcompress=yes|flate|brotli: compress all streams, yes defaults to flate\n"
	"\tcompress-fonts: compress embedded fonts\n"
	"\tcompress-images: compress images\n"
	"\tcompress-effort=0|percentage: effort spent compressing, 0 is default, 100 is max effort\n"
	"\tascii: ASCII hex encode binary streams\n"
	"\tpretty: pretty-print objects with indentation\n"
	"\tlabels: print object labels\n"
	"\tlinearize: optimize for web browsers (no longer supported!)\n"
	"\tclean: pretty-print graphics commands in content streams\n"
	"\tsanitize: sanitize graphics commands in content streams\n"
	"\tgarbage: garbage collect unused objects\n"
	"\tor garbage=compact: ... and compact cross reference table\n"
	"\tor garbage=deduplicate: ... and remove duplicate objects\n"
	"\tincremental: write changes as incremental update\n"
	"\tobjstms: use object streams and cross reference streams\n"
	"\tappearance=yes|all: synthesize just missing, or all, annotation/widget appearance streams\n"
	"\tcontinue-on-error: continue saving the document even if there is an error\n"
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
	{
		if (fz_option_eq(val, "brotli"))
			opts->do_compress = 2;
		else if (fz_option_eq(val, "flate"))
			opts->do_compress = 1;
		else
			opts->do_compress = fz_option_eq(val, "yes");
	}
	if (fz_has_option(ctx, args, "compress-fonts", &val))
		opts->do_compress_fonts = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "compress-images", &val))
		opts->do_compress_images = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "compression-effort", &val))
		opts->compression_effort = fz_atoi(val);
	if (fz_has_option(ctx, args, "labels", &val))
		opts->do_labels = fz_option_eq(val, "yes");
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
	if (fz_has_option(ctx, args, "objstms", &val))
		opts->do_use_objstms = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "regenerate-id", &val))
		opts->dont_regenerate_id = fz_option_eq(val, "no");
	if (fz_has_option(ctx, args, "decrypt", &val))
		opts->do_encrypt = fz_option_eq(val, "yes") ? PDF_ENCRYPT_NONE : PDF_ENCRYPT_KEEP;
	if (fz_has_option(ctx, args, "encrypt", &val))
	{
		if (fz_option_eq(val, "none") || fz_option_eq(val, "no"))
			opts->do_encrypt = PDF_ENCRYPT_NONE;
		else if (fz_option_eq(val, "keep"))
			opts->do_encrypt = PDF_ENCRYPT_KEEP;
		else if (fz_option_eq(val, "rc4-40") || fz_option_eq(val, "yes"))
			opts->do_encrypt = PDF_ENCRYPT_RC4_40;
		else if (fz_option_eq(val, "rc4-128"))
			opts->do_encrypt = PDF_ENCRYPT_RC4_128;
		else if (fz_option_eq(val, "aes-128"))
			opts->do_encrypt = PDF_ENCRYPT_AES_128;
		else if (fz_option_eq(val, "aes-256"))
			opts->do_encrypt = PDF_ENCRYPT_AES_256;
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "unknown encryption in options");
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
	/* Make sure we have no pending annotation changes that need to be updated. */
	if (doc->recalculate)
		pdf_calculate_form(ctx, doc);
	if (doc->resynth_required)
		pdf_update_open_pages(ctx, doc);

	/* Rewrite (and possibly sanitize) the operator streams */
	if (in_opts->do_clean || in_opts->do_sanitize)
	{
		pdf_begin_operation(ctx, doc, "Clean content streams");
		fz_try(ctx)
		{
			clean_content_streams(ctx, doc, in_opts->do_sanitize, in_opts->do_ascii, in_opts->do_pretty);
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
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no input file for incremental write");

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
	int sep;
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
		data->sep = 0;
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
objstm_gather(fz_context *ctx, pdf_xref_entry *x, int i, pdf_document *doc, objstm_gather_data *data)
{
	size_t olen, len;

	if (i == data->root_num || i == data->info_num)
		return;

	/* Ensure the object is loaded! */
	if (i == 0)
		return; /* pdf_cache_object does not like being called for i == 0 which should be free. */
	(void) pdf_cache_object(ctx, doc, i);

	/* Both normal objects and stream objects can get put into objstms (because we've already
	 * unpacked stream objects from objstms earlier!) Stream objects that are non-incremental
	 * will be left as they are by the later check. */
	if ((x->type != 'n' && x->type != 'o') || x->stm_buf != NULL || x->stm_ofs != 0 || x->gen != 0)
		return; /* Objects with generation number != 0 cannot be put in objstms */
	if (i == data->opts->crypt_object_number)
		return; /* Encryption dictionaries can also not be put in objstms */

	/* If we are writing incrementally, then only the last one can be gathered. */
	if (data->opts->do_incremental && !pdf_obj_is_incremental(ctx, x->obj))
		return;

	/* FIXME: Can we do a pass through to check for such objects more exactly? */
	if (pdf_is_int(ctx, x->obj))
		return; /* In case it's a Length value. */
	if (pdf_is_indirect(ctx, x->obj))
		return; /* Bare indirect references are not allowed. */

	if (data->content_buf == NULL)
		data->content_buf = fz_new_buffer(ctx, 128);
	if (data->content_out == NULL)
		data->content_out = fz_new_output_with_buffer(ctx, data->content_buf);

	olen = data->content_buf->len;
	pdf_print_encrypted_obj(ctx, data->content_out, x->obj, 1, 0, NULL, 0, 0, NULL);
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
	int count, num;
	objstm_gather_data data = { 0 };

	data.opts = opts;
	data.root_num = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)));
	data.info_num = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info)));

	count = pdf_xref_len(ctx, doc);
	for (num = 1; num < count; ++num)
	{
		pdf_xref_entry *x = pdf_get_xref_entry_no_change(ctx, doc, num);
		if (x)
			objstm_gather(ctx, x, num, doc, &data);
	}

	flush_gathered(ctx, doc, &data);
}

static void
unpack_objstm_objs(fz_context *ctx, pdf_document *doc, int xref_len)
{
	int num;

	/* At this point, all our objects are cached already. Let's change
	 * all the 'o' objects to be 'n' and get rid of the ObjStm objects
	 * they all came from. */
	for (num = 1; num < xref_len; ++num)
	{
		pdf_xref_entry *x = pdf_get_xref_entry_no_change(ctx, doc, num);
		if (!x || x->type != 'o')
			continue;

		/* Change the type of the object to 'n'. */
		x->type = 'n';
		/* This leaves x->ofs etc wrong, but that's OK as the object is
		 * in memory, and we'll fix it up after the write. */

		/* We no longer need the ObjStm that this object came from. */
		if (x->ofs != 0)
		{
			pdf_xref_entry *y = pdf_get_xref_entry_no_change(ctx, doc, x->ofs);
			/* The xref entry y for the objstm containing the object identified by
			xref entry x above must exist, otherwise that object would not be labelled
			'o' in the xref. */
			assert(y != NULL);
			y->type = 'f';
		}
	}
}

void
pdf_check_document(fz_context *ctx, pdf_document *doc)
{
	int num;

	if (doc->checked)
		return;
	doc->checked = 1;

	for (num = 1; num < pdf_xref_len(ctx, doc); ++num)
	{
		if (pdf_object_exists(ctx, doc, num))
		{
			fz_try(ctx)
				(void) pdf_cache_object(ctx, doc, num);
			fz_catch(ctx)
				fz_report_error(ctx);
		}
	}
}

static void
pdf_ensure_pages_are_pages(fz_context *ctx, pdf_document *doc)
{
	int i;

	if (!doc->fwd_page_map)
		return;

	for (i = 0; i < doc->map_page_count; i++)
	{
		pdf_obj *type = pdf_dict_get(ctx, doc->fwd_page_map[i], PDF_NAME(Type));
		if (type == NULL)
			pdf_dict_put(ctx, doc->fwd_page_map[i], PDF_NAME(Type), PDF_NAME(Page));
	}
}

static void
do_pdf_save_document(fz_context *ctx, pdf_document *doc, pdf_write_state *opts, const pdf_write_options *in_opts)
{
	int lastfree;
	int num;
	int xref_len;
	pdf_obj *id1, *id = NULL;
	int changed;
	int64_t current_offset;

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

	pdf_begin_operation(ctx, doc, "Save document");
	fz_try(ctx)
	{
		/* First, we do a prepass across the document to load all the objects
		 * into memory. We'll end up doing this later on anyway, but by doing
		 * it here, we force any repairs to happen before writing proper
		 * starts. */
		pdf_check_document(ctx, doc);

		xref_len = pdf_xref_len(ctx, doc);

		initialise_write_state(ctx, doc, in_opts, opts);

		if (in_opts->do_labels)
			opts->labels = pdf_load_object_labels(ctx, doc);

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
		assert(!opts->do_snapshot || opts->do_garbage == 0);

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

		if (opts->do_garbage >= 1)
		{
			pdf_ensure_pages_are_pages(ctx, doc);
			pdf_drop_page_tree_internal(ctx, doc);
			pdf_empty_store(ctx, doc);
		}

		do
		{
			changed = 0;
			/* Sweep & mark objects from the trailer */
			if (opts->do_garbage >= 1)
			{
				/* Start by removing indirect /Length attributes on streams */
				for (num = 0; num < xref_len; num++)
					bake_stream_length(ctx, doc, num);

				(void)markobj(ctx, doc, opts, pdf_trailer(ctx, doc));
			}
			else
			{
				for (num = 0; num < xref_len; num++)
					opts->use_list[num] = 1;
			}

			/* Coalesce and renumber duplicate objects */
			if (opts->do_garbage >= 3)
				changed = removeduplicateobjs(ctx, doc, opts);

			/* Compact xref by renumbering and removing unused objects */
			if (opts->do_garbage >= 2)
				compactxref(ctx, doc, opts);

			/* Make renumbering affect all indirect references and update xref */
			if (opts->do_garbage >= 2)
				renumberobjs(ctx, doc, opts);
		}
		while (changed);

		opts->crypt_object_number = 0;
		if (opts->crypt)
		{
			pdf_obj *crypt = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt));
			opts->crypt_object_number = pdf_to_num(ctx, crypt);
		}

		xref_len = pdf_xref_len(ctx, doc); /* May have changed due to repair */
		expand_lists(ctx, opts, xref_len);

		/* If we're about to do a non-incremental write, we can't
		 * afford to leave any objects in ObjStms. We might have
		 * changed the objects, and we won't know to update the
		 * stream. So pull all the objects into memory. */
		if (!opts->do_incremental)
			unpack_objstm_objs(ctx, doc, xref_len);

		if (opts->do_use_objstms)
			gather_to_objstms(ctx, doc, opts, xref_len);

		xref_len = pdf_xref_len(ctx, doc); /* May have changed due to the gather */
		expand_lists(ctx, opts, xref_len);

		/* Truncate the xref after compacting and renumbering */
		if ((opts->do_garbage >= 2) &&
			!opts->do_incremental)
		{
			while (xref_len > 0 && !opts->use_list[xref_len-1])
				xref_len--;
		}

		if (opts->do_incremental)
		{
			int i;

			doc->disallow_new_increments = 1;

			for (i = 0; i < doc->num_incremental_sections; i++)
			{
				doc->xref_base = doc->num_incremental_sections - i - 1;
				xref_len = pdf_xref_len(ctx, doc);

				writeobjects(ctx, doc, opts);

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

				current_offset = fz_tell_output(ctx, opts->out);
				if (!doc->last_xref_was_old_style || opts->do_use_objstms)
					writexrefstream(ctx, doc, opts, 0, xref_len, 1, current_offset);
				else
					writexref(ctx, doc, opts, 0, xref_len, 1, current_offset);

				doc->xref_sections[doc->xref_base].end_ofs = fz_tell_output(ctx, opts->out);
			}

			doc->xref_base = 0;
			doc->disallow_new_increments = 0;
		}
		else
		{
			writeobjects(ctx, doc, opts);

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
			opts->gen_list[0] = 0xffff;

			current_offset = fz_tell_output(ctx, opts->out);
			if (opts->do_use_objstms)
				writexrefstream(ctx, doc, opts, 0, xref_len, 1, current_offset);
			else
				writexref(ctx, doc, opts, 0, xref_len, 1, current_offset);

			doc->xref_sections[0].end_ofs = fz_tell_output(ctx, opts->out);
		}

		if (!in_opts->do_snapshot)
		{
			complete_signatures(ctx, doc, opts);
		}

		pdf_sync_open_pages(ctx, doc);

		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
	{
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
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes on a repaired file");
	if (in_opts->do_incremental && in_opts->do_garbage)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes with garbage collection");
	if (in_opts->do_linear)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Linearisation is no longer supported");
	if (in_opts->do_incremental && in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes when changing encryption");
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
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't use these options when snapshotting!");
	}
	if (pdf_has_unsaved_sigs(ctx, doc) && !fz_output_supports_stream(ctx, out))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't write pdf that has unsaved sigs to a fz_output unless it supports fz_stream_from_output!");

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
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes on a new document");
	if (in_opts->do_incremental && doc->repair_attempted)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes on a repaired file");
	if (in_opts->do_incremental && in_opts->do_garbage)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes with garbage collection");
	if (in_opts->do_linear)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Linearisation is no longer supported");
	if (in_opts->do_incremental && in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't do incremental writes when changing encryption");
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
			in_opts->do_clean ||
			in_opts->do_sanitize ||
			in_opts->do_appearance ||
			in_opts->do_encrypt != PDF_ENCRYPT_KEEP)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't use these options when snapshotting!");
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

	if (in_opts->do_incremental)
		opts.bias = doc->bias;

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
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't write non-existent journal");

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
