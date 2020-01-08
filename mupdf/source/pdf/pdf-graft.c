#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "../fitz/fitz-imp.h"

#include <assert.h>

struct pdf_graft_map_s
{
	int refs;
	int len;
	pdf_document *src;
	pdf_document *dst;
	int *dst_from_src;
};

/*
	Prepare a graft map object to allow objects
	to be deep copied from one document to the given one, avoiding
	problems with duplicated child objects.

	dst: The document to copy objects to.

	Note: all the source objects must come from the same document.
*/
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

/*
	Return a deep copied object equivalent to the
	supplied object, suitable for use within the given document.

	dst: The document in which the returned object is to be used.

	obj: The object deep copy.

	Note: If grafting multiple objects, you should use a pdf_graft_map
	to avoid potential duplication of target objects.
*/
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

/*
	Return a deep copied object equivalent
	to the supplied object, suitable for use within the target
	document of the map.

	map: A map targeted at the document in which the returned
	object is to be used.

	obj: The object to be copied.

	Note: Copying multiple objects via the same graft map ensures
	that any shared children are not copied more than once.
*/
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
