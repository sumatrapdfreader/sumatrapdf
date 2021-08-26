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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <assert.h>

struct pdf_graft_map
{
	int refs;
	int len;
	pdf_document *src;
	pdf_document *dst;
	int *dst_from_src;
};

pdf_graft_map *
pdf_new_graft_map(fz_context *ctx, pdf_document *dst)
{
	pdf_graft_map *map = NULL;

	map = fz_malloc_struct(ctx, pdf_graft_map);

	map->dst = pdf_keep_document(ctx, dst);
	map->refs = 1;
	return map;
}

pdf_graft_map *
pdf_keep_graft_map(fz_context *ctx, pdf_graft_map *map)
{
	return fz_keep_imp(ctx, map, &map->refs);
}

void
pdf_drop_graft_map(fz_context *ctx, pdf_graft_map *map)
{
	if (fz_drop_imp(ctx, map, &map->refs))
	{
		pdf_drop_document(ctx, map->src);
		pdf_drop_document(ctx, map->dst);
		fz_free(ctx, map->dst_from_src);
		fz_free(ctx, map);
	}
}

pdf_obj *
pdf_graft_object(fz_context *ctx, pdf_document *dst, pdf_obj *obj)
{
	pdf_document *src;
	pdf_graft_map *map;

	/* Primitive objects are not bound to a document, so can be re-used as is. */
	src = pdf_get_bound_document(ctx, obj);
	if (src == NULL)
		return pdf_keep_obj(ctx, obj);

	map = pdf_new_graft_map(ctx, dst);

	fz_try(ctx)
		obj = pdf_graft_mapped_object(ctx, map, obj);
	fz_always(ctx)
		pdf_drop_graft_map(ctx, map);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return obj;
}

pdf_obj *
pdf_graft_mapped_object(fz_context *ctx, pdf_graft_map *map, pdf_obj *obj)
{
	pdf_obj *val, *key;
	pdf_obj *new_obj = NULL;
	pdf_obj *new_dict;
	pdf_obj *new_array;
	pdf_obj *ref = NULL;
	fz_buffer *buffer = NULL;
	pdf_document *src;
	int new_num, src_num, len, i;

	/* Primitive objects are not bound to a document, so can be re-used as is. */
	src = pdf_get_bound_document(ctx, obj);
	if (!src)
		return pdf_keep_obj(ctx, obj);

	if (map->src && src != map->src)
		fz_throw(ctx, FZ_ERROR_GENERIC, "grafted objects must all belong to the same source document");

	if (pdf_is_indirect(ctx, obj))
	{
		src_num = pdf_to_num(ctx, obj);

		if (map->src == NULL)
		{
			fz_try(ctx)
			{
				map->src = pdf_keep_document(ctx, src);
				map->len = pdf_xref_len(ctx, src);
				map->dst_from_src = fz_calloc(ctx, map->len, sizeof(int));
			}
			fz_catch(ctx)
			{
				pdf_drop_document(ctx, map->src);
				map->src = NULL;
				fz_rethrow(ctx);
			}
		}

		if (src_num < 1 || src_num >= map->len)
			fz_throw(ctx, FZ_ERROR_GENERIC, "source object number out of range");

		/* Check if we have done this one.  If yes, then just
		 * return our indirect ref */
		if (map->dst_from_src[src_num] != 0)
		{
			int dest_num = map->dst_from_src[src_num];
			return pdf_new_indirect(ctx, map->dst, dest_num, 0);
		}

		fz_var(buffer);
		fz_var(ref);
		fz_var(new_obj);

		fz_try(ctx)
		{
			/* Create new slot for our src object, set the mapping and call again
			 * using the resolved indirect reference */
			new_num = pdf_create_object(ctx, map->dst);
			map->dst_from_src[src_num] = new_num;
			new_obj = pdf_graft_mapped_object(ctx, map, pdf_resolve_indirect(ctx, obj));

			/* Return a ref to the new_obj making sure to attach any stream */
			pdf_update_object(ctx, map->dst, new_num, new_obj);
			ref = pdf_new_indirect(ctx, map->dst, new_num, 0);
			if (pdf_is_stream(ctx, obj))
			{
				buffer = pdf_load_raw_stream_number(ctx, src, src_num);
				pdf_update_stream(ctx, map->dst, ref, buffer, 1);
			}
		}
		fz_always(ctx)
		{
			pdf_drop_obj(ctx, new_obj);
			fz_drop_buffer(ctx, buffer);
		}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, ref);
			fz_rethrow(ctx);
		}
		return ref;
	}
	else if (pdf_is_dict(ctx, obj))
	{
		len = pdf_dict_len(ctx, obj);
		new_dict = pdf_new_dict(ctx, map->dst, len);

		fz_try(ctx)
		{
			for (i = 0; i < len; i++)
			{
				key = pdf_dict_get_key(ctx, obj, i);
				val = pdf_dict_get_val(ctx, obj, i);
				pdf_dict_put_drop(ctx, new_dict, key, pdf_graft_mapped_object(ctx, map, val));
			}
		}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, new_dict);
			fz_rethrow(ctx);
		}
		return new_dict;
	}
	else if (pdf_is_array(ctx, obj))
	{
		/* Step through the array items handling indirect refs */
		len = pdf_array_len(ctx, obj);
		new_array = pdf_new_array(ctx, map->dst, len);

		fz_try(ctx)
		{
			for (i = 0; i < len; i++)
			{
				val = pdf_array_get(ctx, obj, i);
				pdf_array_push_drop(ctx, new_array, pdf_graft_mapped_object(ctx, map, val));
			}
		}
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, new_array);
			fz_rethrow(ctx);
		}
		return new_array;
	}
	else
	{
		assert("This never happens" == NULL);
		return NULL;
	}
}

void pdf_graft_mapped_page(fz_context *ctx, pdf_graft_map *map, int page_to, pdf_document *src, int page_from)
{
	pdf_obj *page_ref;
	pdf_obj *page_dict = NULL;
	pdf_obj *obj;
	pdf_obj *ref = NULL;
	int i;
	pdf_document *dst = map->dst;

	/* Copy as few key/value pairs as we can. Do not include items that reference other pages. */
	static pdf_obj * const copy_list[] = {
		PDF_NAME(Contents),
		PDF_NAME(Resources),
		PDF_NAME(MediaBox),
		PDF_NAME(CropBox),
		PDF_NAME(BleedBox),
		PDF_NAME(TrimBox),
		PDF_NAME(ArtBox),
		PDF_NAME(Rotate),
		PDF_NAME(UserUnit)
	};

	fz_var(ref);
	fz_var(page_dict);

	fz_try(ctx)
	{
		page_ref = pdf_lookup_page_obj(ctx, src, page_from);

		/* Make a new page object dictionary to hold the items we copy from the source page. */
		page_dict = pdf_new_dict(ctx, dst, 4);

		pdf_dict_put(ctx, page_dict, PDF_NAME(Type), PDF_NAME(Page));

		for (i = 0; i < (int)nelem(copy_list); i++)
		{
			obj = pdf_dict_get_inheritable(ctx, page_ref, copy_list[i]);
			if (obj != NULL)
				pdf_dict_put_drop(ctx, page_dict, copy_list[i], pdf_graft_mapped_object(ctx, map, obj));
		}

		/* Add the page object to the destination document. */
		ref = pdf_add_object(ctx, dst, page_dict);

		/* Insert it into the page tree. */
		pdf_insert_page(ctx, dst, page_to, ref);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, page_dict);
		pdf_drop_obj(ctx, ref);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_graft_page(fz_context *ctx, pdf_document *dst, int page_to, pdf_document *src, int page_from)
{
	pdf_graft_map *map = pdf_new_graft_map(ctx, dst);
	fz_try(ctx)
		pdf_graft_mapped_page(ctx, map, page_to, src, page_from);
	fz_always(ctx)
		pdf_drop_graft_map(ctx, map);
	fz_catch(ctx)
		fz_rethrow(ctx);
}
