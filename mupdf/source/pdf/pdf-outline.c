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

#include <string.h>

/*
	The URI encoding format broadly follows that described in
	"Parameters for Opening PDF files" from the Adobe Acrobat SDK,
	version 8.1, which can, at the time of writing, be found here:

	https://web.archive.org/web/20170921000830/http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/pdf_open_parameters.pdf
*/

static int
pdf_test_outline(fz_context *ctx, pdf_document *doc, pdf_obj *dict, pdf_mark_list *mark_list, pdf_obj *parent, int fixed)
{
	pdf_obj *obj, *prev = NULL;

		while (dict && pdf_is_dict(ctx, dict))
		{
		if (pdf_mark_list_push(ctx, mark_list, dict))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Cycle detected in outlines");

		obj = pdf_dict_get(ctx, dict, PDF_NAME(Prev));
		if (pdf_objcmp(ctx, prev, obj))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Bad or missing pointer in outline tree");
		prev = dict;

		obj = pdf_dict_get(ctx, dict, PDF_NAME(Parent));
		if (pdf_objcmp(ctx, parent, obj))
			{
			if (fixed > 1)
				fz_throw(ctx, FZ_ERROR_GENERIC, "Bad or missing parent pointer in outline tree");
			fz_warn(ctx, "Bad or missing parent pointer in outline tree");
			pdf_dict_put(ctx, dict, PDF_NAME(Parent), parent);
			fixed = 1;
			}

			obj = pdf_dict_get(ctx, dict, PDF_NAME(First));
			if (obj)
			fixed = pdf_test_outline(ctx, doc, obj, mark_list, dict, fixed);

			dict = pdf_dict_get(ctx, dict, PDF_NAME(Next));
		}

	return fixed;
}

fz_outline *
pdf_load_outline(fz_context *ctx, pdf_document *doc)
{
	/* Just appeal to the fz_ level. */
	return fz_load_outline(ctx, (fz_document *)doc);
}

enum {
	MOD_NONE = 0,
	MOD_BELOW = 1,
	MOD_AFTER = 2
};

typedef struct pdf_outline_iterator {
	fz_outline_iterator super;
	fz_outline_item item;
	pdf_obj *current;
	int modifier;
} pdf_outline_iterator;

static int
pdf_outline_iterator_next(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_obj *next;

	if (iter->modifier != MOD_NONE || iter->current == NULL)
		return -1;
	next = pdf_dict_get(ctx, iter->current, PDF_NAME(Next));
	if (next == NULL)
	{
		iter->modifier = MOD_AFTER;
		return 1;
	}

	iter->modifier = MOD_NONE;
	iter->current = next;
	return 0;
}

static int
pdf_outline_iterator_prev(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_obj *prev;

	if (iter->modifier == MOD_BELOW || iter->current == NULL)
		return -1;
	if (iter->modifier == MOD_AFTER)
	{
		iter->modifier = MOD_NONE;
		return 0;
	}
	prev = pdf_dict_get(ctx, iter->current, PDF_NAME(Prev));
	if (prev == NULL)
		return -1;

	iter->modifier = MOD_NONE;
	iter->current = prev;
	return 0;
}

static int
pdf_outline_iterator_up(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_obj *up;

	if (iter->current == NULL)
		return -1;
	if (iter->modifier == MOD_BELOW)
	{
		iter->modifier = MOD_NONE;
		return 0;
	}
	up = pdf_dict_get(ctx, iter->current, PDF_NAME(Parent));
	if (up == NULL)
		return -1;

	iter->modifier = MOD_NONE;
	iter->current = up;
	return 0;
}

static int
pdf_outline_iterator_down(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_obj *down;

	if (iter->modifier != MOD_NONE || iter->current == NULL)
		return -1;
	down = pdf_dict_get(ctx, iter->current, PDF_NAME(First));
	if (down == NULL)
	{
		iter->modifier = MOD_BELOW;
		return 1;
	}

	iter->modifier = MOD_NONE;
	iter->current = down;
	return 0;
}

static int
int_from_fragment(const char *uri, const char *needle, int val_nomatch, int val)
{
	const char *n = needle;
	if (*uri == '#')
		uri++;
	while (*uri)
	{
		if (*uri == *n)
		{
			/* Match, move on one char.*/
			uri++;
			n++;
			/* If the match hasn't ended yet, loop and keep going. */
			if (*n != 0)
				continue;
			if (*uri == '&' || *uri == 0)
				return val;
			if (*uri == '=')
			{
				const char *u = ++uri;
				int v = 0;
				while (*u >= '0' && *u <= '9')
				{
					v = (v*10)+ (*u++)-'0';
				}
				if (u == uri)
					return val;
				return v;
			}
		}
		/* Skip to next one. */
		while (*uri && *uri != '&')
			uri++;
		if (*uri)
			uri++;
		n = needle;
	}
	return val_nomatch;
}

typedef struct
{
	pdf_obj *name;
	char string[16];
	int len;
} namestring;

#define PDF_OUTLINE_TYPE(A) { PDF_NAME(A), # A, sizeof(#A)-1 }
static const namestring fit_types[] =
{
	PDF_OUTLINE_TYPE(Fit),
	PDF_OUTLINE_TYPE(FitB),
	PDF_OUTLINE_TYPE(FitBH),
	PDF_OUTLINE_TYPE(FitBV),
	PDF_OUTLINE_TYPE(FitH),
	PDF_OUTLINE_TYPE(FitR),
	PDF_OUTLINE_TYPE(FitV),
	{ NULL, "" }
};
#undef PDF_OUTLINE_TYPE

static pdf_obj *
type_match(const char **uri, const namestring *types)
{
	if (*uri == NULL)
		return NULL;

	while (types->name)
	{
		if (strncmp(*uri, types->string, types->len) == 0)
		{
			char c = (*uri)[types->len];
			if (c == ',' || c == '&')
			{
				(*uri) += types->len + 1;
				return types->name;
			}
			if (c == 0)
				return types->name;
		}
		types++;
	}
	return NULL;
}

static const char *
val_from_fragment(const char *uri, const char *needle)
{
	const char *n = needle;
	if (*uri == '#')
		uri++;
	while (*uri)
	{
		if (*uri == *n)
		{
			/* Match, move on one char.*/
			uri++;
			n++;
			/* If the match hasn't ended yet, loop and keep going. */
			if (*n != 0)
				continue;
			if (*uri == '&' || *uri == 0)
				return NULL;
			if (*uri == '=')
				return ++uri;
		}
		/* Skip to next one. */
		while (*uri && *uri != '&')
			uri++;
		if (*uri)
			uri++;
		n = needle;
	}
	return NULL;
}

static float
my_read_float(const char **str)
{
	float f;
	char *end;

	if (**str == 0)
		return 0;

	f = fz_strtof(*str, &end);
	*str = end;
	if (**str == ',')
		(*str)++;

	return f;
}

static void
do_outline_update(fz_context *ctx, pdf_obj *obj, fz_outline_item *item, int is_new_node)
{
	int count;
	int open_delta = 0;
	pdf_obj *parent, *up;

	/* If the open/closed state changes, update. */
	count = pdf_dict_get_int(ctx, obj, PDF_NAME(Count));
	if ((count < 0 && item->is_open) || (count > 0 && !item->is_open))
	{
		pdf_dict_put_int(ctx, obj, PDF_NAME(Count), -count);
		open_delta = -count;
	}
	else if (is_new_node && item->is_open)
		open_delta = 1;

	up = obj;
	while ((parent = pdf_dict_get(ctx, up, PDF_NAME(Parent))) != NULL)
	{
		pdf_obj *cobj = pdf_dict_get(ctx, up, PDF_NAME(Count));
		count = pdf_to_int(ctx, cobj);
		if (open_delta || cobj == NULL)
			pdf_dict_put_int(ctx, up, PDF_NAME(Count), count > 0 ? count + open_delta : count - open_delta);
		up = parent;
	}

	if (item->title)
		pdf_dict_put_text_string(ctx, obj, PDF_NAME(Title), item->title);
	else
		pdf_dict_del(ctx, obj, PDF_NAME(Title));

	pdf_dict_del(ctx, obj, PDF_NAME(A));
	pdf_dict_del(ctx, obj, PDF_NAME(Dest));
	if (item->uri)
	{
		if (fz_is_external_link(ctx, item->uri))
		{
			pdf_obj *a = pdf_dict_put_dict(ctx, obj, PDF_NAME(A), 4);
			pdf_dict_put(ctx, a, PDF_NAME(Type), PDF_NAME(Action));
			pdf_dict_put(ctx, a, PDF_NAME(S), PDF_NAME(URI));
			pdf_dict_put_text_string(ctx, a, PDF_NAME(URI), item->uri);
		}
		else
		{
			pdf_document *doc = pdf_get_bound_document(ctx, obj);
			int page = int_from_fragment(item->uri, "page", 0, 0) - 1;
			const char *val = val_from_fragment(item->uri, "view");
			pdf_obj *type = type_match(&val, fit_types);
			pdf_obj *arr = pdf_dict_put_array(ctx, obj, PDF_NAME(Dest), 5);

			if (type == NULL)
			{
				val = val_from_fragment(item->uri, "viewrect");
				if (val)
					type = PDF_NAME(FitR);
			}
			if (type == NULL)
			{
				val = val_from_fragment(item->uri, "zoom");
				if (val)
					type = PDF_NAME(XYZ);
			}

			pdf_array_push(ctx, arr, pdf_lookup_page_obj(ctx, doc, page));
			if (type)
			{
				float a1, a2, a3, a4;
				a1 = my_read_float(&val);
				a2 = my_read_float(&val);
				a3 = my_read_float(&val);
				a4 = my_read_float(&val);
				pdf_array_push(ctx, arr, type);
				if (type == PDF_NAME(XYZ))
				{
					pdf_array_push_real(ctx, arr, a1);
					pdf_array_push_real(ctx, arr, a2);
					pdf_array_push_real(ctx, arr, a3);
				}
				else if (type == PDF_NAME(FitH) || type == PDF_NAME(FitBH) || type == PDF_NAME(FitV) || type == PDF_NAME(FitBV))
					pdf_array_push_real(ctx, arr, a1);
				else if (type == PDF_NAME(FitR))
				{
					pdf_array_push_real(ctx, arr, a1);
					pdf_array_push_real(ctx, arr, a2);
					pdf_array_push_real(ctx, arr, a3);
					pdf_array_push_real(ctx, arr, a4);
				}
			}
			else
				pdf_array_push(ctx, arr, PDF_NAME(Fit));
		}
	}
}

static int
pdf_outline_iterator_insert(fz_context *ctx, fz_outline_iterator *iter_, fz_outline_item *item)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_document *doc = (pdf_document *)iter->super.doc;
	pdf_obj *obj;
	pdf_obj *prev;
	pdf_obj *parent;
	int result;

	obj = pdf_add_new_dict(ctx, doc, 4);
	fz_try(ctx)
	{
		if (iter->modifier == MOD_BELOW)
			parent = iter->current;
		else if (iter->modifier == MOD_NONE && iter->current == NULL)
		{
			pdf_obj *outlines, *root;
			root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			outlines = pdf_dict_get(ctx, root, PDF_NAME(Outlines));
			if (outlines == NULL)
			{
				/* No outlines entry, better make one. */
				outlines = pdf_dict_put_dict(ctx, root, PDF_NAME(Outlines), 4);
				pdf_dict_put(ctx, outlines, PDF_NAME(Type), PDF_NAME(Outlines));
			}
			iter->modifier = MOD_BELOW;
			iter->current = outlines;
			parent = outlines;
		}
		else
			parent = pdf_dict_get(ctx, iter->current, PDF_NAME(Parent));

		pdf_dict_put(ctx, obj, PDF_NAME(Parent), parent);

		do_outline_update(ctx, obj, item, 1);

		switch (iter->modifier)
		{
		case MOD_BELOW:
			pdf_dict_put(ctx, iter->current, PDF_NAME(First), obj);
			pdf_dict_put(ctx, iter->current, PDF_NAME(Last), obj);
			iter->current = obj;
			iter->modifier = MOD_AFTER;
			result = 1;
			break;
		case MOD_AFTER:
			pdf_dict_put(ctx, obj, PDF_NAME(Prev), iter->current);
			pdf_dict_put(ctx, iter->current, PDF_NAME(Next), obj);
			pdf_dict_put(ctx, parent, PDF_NAME(Last), obj);
			iter->current = obj;
			result = 1;
			break;
		default:
			prev = pdf_dict_get(ctx, iter->current, PDF_NAME(Prev));
			if (prev)
			{
				pdf_dict_put(ctx, prev, PDF_NAME(Next), obj);
				pdf_dict_put(ctx, obj, PDF_NAME(Prev), prev);
			}
			pdf_dict_put(ctx, iter->current, PDF_NAME(Prev), obj);
			pdf_dict_put(ctx, obj, PDF_NAME(Next), iter->current);
			result = 0;
			break;
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, obj);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return result;
}

static void
pdf_outline_iterator_update(fz_context *ctx, fz_outline_iterator *iter_, fz_outline_item *item)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;

	if (iter->modifier != MOD_NONE || iter->current == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't update a non-existent outline item!");

	do_outline_update(ctx, iter->current, item, 0);
}

static int
pdf_outline_iterator_del(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_obj *next, *prev, *parent;
	int count;

	if (iter->modifier != MOD_NONE)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Can't delete a non-existent outline item!");

	prev = pdf_dict_get(ctx, iter->current, PDF_NAME(Prev));
	next = pdf_dict_get(ctx, iter->current, PDF_NAME(Next));
	parent = pdf_dict_get(ctx, iter->current, PDF_NAME(Parent));
	count = pdf_dict_get_int(ctx, iter->current, PDF_NAME(Count));

	if (count > 0)
	{
		pdf_obj *up = parent;
		while (up)
		{
			int c = pdf_dict_get_int(ctx, up, PDF_NAME(Count));
			pdf_dict_put_int(ctx, up, PDF_NAME(Count), (c > 0 ? c - count : c + count));
			up = pdf_dict_get(ctx, up, PDF_NAME(Parent));
		}
	}

	if (prev)
	{
		if (next)
			pdf_dict_put(ctx, prev, PDF_NAME(Next), next);
		else
			pdf_dict_del(ctx, prev, PDF_NAME(Next));
	}
	if (next)
	{
		if (prev)
			pdf_dict_put(ctx, next, PDF_NAME(Prev), prev);
		else
			pdf_dict_del(ctx, next, PDF_NAME(Prev));
		iter->current = next;
	}
	else if (prev)
	{
		iter->current = prev;
		pdf_dict_put(ctx, parent, PDF_NAME(Last), prev);
	}
	else
	{
		iter->current = parent;
		iter->modifier = MOD_BELOW;
		pdf_dict_del(ctx, parent, PDF_NAME(First));
		pdf_dict_del(ctx, parent, PDF_NAME(Last));
	}

	return 0;
}

static fz_outline_item *
pdf_outline_iterator_item(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_obj *obj;
	pdf_document *doc = (pdf_document *)iter->super.doc;

	if (iter->modifier != MOD_NONE || iter->current == NULL)
		return NULL;

	fz_free(ctx, iter->item.title);
	iter->item.title = NULL;
	fz_free(ctx, iter->item.uri);
	iter->item.uri = NULL;

	obj = pdf_dict_get(ctx, iter->current, PDF_NAME(Title));
	if (obj)
		iter->item.title = Memento_label(fz_strdup(ctx, pdf_to_text_string(ctx, obj)), "outline_title");
	obj = pdf_dict_get(ctx, iter->current, PDF_NAME(Dest));
	if (obj)
		iter->item.uri = Memento_label(pdf_parse_link_dest(ctx, doc, obj), "outline_uri");
	else
	{
		obj = pdf_dict_get(ctx, iter->current, PDF_NAME(A));
		if (obj)
			iter->item.uri = Memento_label(pdf_parse_link_action(ctx, doc, obj, -1), "outline_uri");
	}

	obj = pdf_dict_get(ctx, iter->current, PDF_NAME(Count));

	iter->item.is_open = (pdf_to_int(ctx, obj) > 0);

	return &iter->item;
}

static void
pdf_outline_iterator_drop(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;

	if (iter == NULL)
		return;

	fz_free(ctx, iter->item.title);
	fz_free(ctx, iter->item.uri);
}

fz_outline_iterator *pdf_new_outline_iterator(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *root, *obj, *first;
	pdf_mark_list mark_list;
	pdf_outline_iterator *iter = NULL;

	/* Walk the outlines to spot problems that might bite us later
	 * (in particular, for cycles). */
	pdf_mark_list_init(ctx, &mark_list);
	fz_try(ctx)
	{
	root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	obj = pdf_dict_get(ctx, root, PDF_NAME(Outlines));
	first = pdf_dict_get(ctx, obj, PDF_NAME(First));
	if (first)
	{
		/* cache page tree for fast link destination lookups */
		pdf_load_page_tree(ctx, doc);
		fz_try(ctx)
			{
				/* Pass through the outlines once, fixing them if we can.*/
				int fixed = pdf_test_outline(ctx, doc, first, &mark_list, obj, 0);
				/* If a fix was performed, pass through again, this time throwing
				 * if it's still not correct. */
				if (fixed)
				{
					pdf_mark_list_free(ctx, &mark_list);
					pdf_test_outline(ctx, doc, first, &mark_list, obj, 2);
				}
			}
		fz_always(ctx)
			pdf_drop_page_tree(ctx, doc);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	}
	fz_always(ctx)
		pdf_mark_list_free(ctx, &mark_list);
	fz_catch(ctx)
		fz_rethrow(ctx);

	iter = fz_new_derived_outline_iter(ctx, pdf_outline_iterator, &doc->super);
	iter->super.del = pdf_outline_iterator_del;
	iter->super.next = pdf_outline_iterator_next;
	iter->super.prev = pdf_outline_iterator_prev;
	iter->super.up = pdf_outline_iterator_up;
	iter->super.down = pdf_outline_iterator_down;
	iter->super.insert = pdf_outline_iterator_insert;
	iter->super.update = pdf_outline_iterator_update;
	iter->super.drop = pdf_outline_iterator_drop;
	iter->super.item = pdf_outline_iterator_item;
	iter->current = first;
	iter->modifier = MOD_NONE;

	return &iter->super;
}

int
pdf_resolve_link(fz_context *ctx, pdf_document *doc, const char *uri, float *xp, float *yp)
{
	if (uri && uri[0] == '#')
	{
		int page = int_from_fragment(uri, "page", 0, 0) - 1;
		const char *val = val_from_fragment(uri, "view");
		pdf_obj *type = type_match(&val, fit_types);

		if (type == NULL)
		{
			val = val_from_fragment(uri, "viewrect");
			if (val)
				type = PDF_NAME(FitR);
	}
		if (type == NULL)
		{
			val = val_from_fragment(uri, "zoom");
			if (val)
				type = PDF_NAME(XYZ);
		}

		/* Nasty: Map full details down to just x or y. */
		if (xp)
			*xp = -1;
		if (yp)
			*yp = -1;
		if (type)
		{
			fz_rect mediabox;
			fz_matrix pagectm;
			float w, h, a1, a2;
			/* Only a1 and a2 are currently used. In future we may want to read
			 * a3 and a4 too. */
			a1 = my_read_float(&val);
			a2 = my_read_float(&val);
			/* Link coords use a coordinate space that does not seem to respect Rotate or UserUnit. */
			/* All we need to do is figure out the page size to flip the coordinate space and
			 * clamp the coordinates to stay on the page. */
			pdf_page_obj_transform(ctx, pdf_lookup_page_obj(ctx, doc, page), &mediabox, &pagectm);
			mediabox = fz_transform_rect(mediabox, pagectm);
			w = mediabox.x1 - mediabox.x0;
			h = mediabox.y1 - mediabox.y0;

			if (type == PDF_NAME(FitH) || type == PDF_NAME(FitBH))
				if (yp) *yp = fz_clamp(h-a1, 0, h);
			if (type == PDF_NAME(FitV) || type == PDF_NAME(FitBV))
				if (xp) *xp = fz_clamp(a1, 0, w);
			if (type == PDF_NAME(FitR) || type == PDF_NAME(XYZ))
			{
				if (xp) *xp = fz_clamp(a1, 0, w);
				if (yp) *yp = fz_clamp(h-a2, 0, h);
		}
		}
		return page;
	}
	fz_warn(ctx, "unknown link uri '%s'", uri);
	return -1;
}
