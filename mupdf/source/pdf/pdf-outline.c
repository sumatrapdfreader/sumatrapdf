// Copyright (C) 2004-2023 Artifex Software, Inc.
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
#include "mupdf/pdf.h"
#include "pdf-annot-imp.h"

#include <string.h>
#include <math.h>

/*
	The URI encoding format broadly follows that described in
	"Parameters for Opening PDF files" from the Adobe Acrobat SDK,
	version 8.1, which can, at the time of writing, be found here:

	https://web.archive.org/web/20170921000830/http://www.adobe.com/content/dam/Adobe/en/devnet/acrobat/pdfs/pdf_open_parameters.pdf
*/

static void
pdf_test_outline(fz_context *ctx, pdf_document *doc, pdf_obj *dict, pdf_mark_bits *marks, pdf_obj *parent, int *fixed)
{
	int parent_diff, prev_diff, last_diff;
	pdf_obj *first, *last, *next, *prev;
	pdf_obj *expected_parent = parent;
	pdf_obj *expected_prev = NULL;

	last = pdf_dict_get(ctx, expected_parent, PDF_NAME(Last));

	while (dict && pdf_is_dict(ctx, dict))
	{
		if (pdf_mark_bits_set(ctx, marks, dict))
			fz_throw(ctx, FZ_ERROR_FORMAT, "Cycle detected in outlines");

		parent = pdf_dict_get(ctx, dict, PDF_NAME(Parent));
		prev = pdf_dict_get(ctx, dict, PDF_NAME(Prev));
		next = pdf_dict_get(ctx, dict, PDF_NAME(Next));

		parent_diff = pdf_objcmp(ctx, parent, expected_parent);
		prev_diff = pdf_objcmp(ctx, prev, expected_prev);
		last_diff = next == NULL && pdf_objcmp(ctx, last, dict);

		if (fixed == NULL)
		{
			if (parent_diff)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Outline parent pointer still bad or missing despite repair");
			if (prev_diff)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Outline prev pointer still bad or missing despite repair");
			if (last_diff)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Outline last pointer still bad or missing despite repair");
		}
		else if (parent_diff || prev_diff || last_diff)
		{
			if (*fixed == 0)
				pdf_begin_operation(ctx, doc, "Repair outline nodes");
			*fixed = 1;
			doc->non_structural_change = 1;
			fz_try(ctx)
			{
				if (parent_diff)
				{
					fz_warn(ctx, "Bad or missing parent pointer in outline tree, repairing");
					pdf_dict_put(ctx, dict, PDF_NAME(Parent), expected_parent);
				}
				if (prev_diff)
				{
					fz_warn(ctx, "Bad or missing prev pointer in outline tree, repairing");
					if (expected_prev)
						pdf_dict_put(ctx, dict, PDF_NAME(Prev), expected_prev);
					else
						pdf_dict_del(ctx, dict, PDF_NAME(Prev));
				}
				if (last_diff)
				{
					fz_warn(ctx, "Bad or missing last pointer in outline tree, repairing");
					pdf_dict_put(ctx, expected_parent, PDF_NAME(Last), dict);
				}
			}
			fz_always(ctx)
				doc->non_structural_change = 0;
			fz_catch(ctx)
				fz_rethrow(ctx);
		}

		first = pdf_dict_get(ctx, dict, PDF_NAME(First));
		if (first)
			pdf_test_outline(ctx, doc, first, marks, dict, fixed);

		expected_prev = dict;
		dict = next;
	}
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
	pdf_obj *grandparent;

	if (iter->current == NULL)
		return -1;
	if (iter->modifier == MOD_BELOW)
	{
		iter->modifier = MOD_NONE;
		return 0;
	}
	/* The topmost level still has a parent pointer, just one
	 * that points to the outlines object. We never want to
	 * allow us to move 'up' onto the outlines object. */
	up = pdf_dict_get(ctx, iter->current, PDF_NAME(Parent));
	if (up == NULL)
		/* This should never happen! */
		return -1;
	grandparent = pdf_dict_get(ctx, up, PDF_NAME(Parent));
	if (grandparent == NULL)
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

static void
do_outline_update(fz_context *ctx, pdf_obj *obj, fz_outline_item *item, int is_new_node)
{
	int count;
	int open_delta = 0;
	pdf_obj *parent;

	/* If the open/closed state changes, update. */
	count = pdf_dict_get_int(ctx, obj, PDF_NAME(Count));
	if ((count < 0 && item->is_open) || (count > 0 && !item->is_open))
	{
		pdf_dict_put_int(ctx, obj, PDF_NAME(Count), -count);
		open_delta = -count;
	}
	else if (is_new_node)
		open_delta = 1;

	parent = pdf_dict_get(ctx, obj, PDF_NAME(Parent));
	while (parent)
	{
		pdf_obj *cobj = pdf_dict_get(ctx, parent, PDF_NAME(Count));
		count = pdf_to_int(ctx, cobj);
		if (open_delta || cobj == NULL)
			pdf_dict_put_int(ctx, parent, PDF_NAME(Count), count >= 0 ? count + open_delta : count - open_delta);
		if (count < 0)
			break;
		parent = pdf_dict_get(ctx, parent, PDF_NAME(Parent));
	}

	if (item->title)
		pdf_dict_put_text_string(ctx, obj, PDF_NAME(Title), item->title);
	else
		pdf_dict_del(ctx, obj, PDF_NAME(Title));

	pdf_dict_del(ctx, obj, PDF_NAME(A));
	pdf_dict_del(ctx, obj, PDF_NAME(Dest));
	if (item->uri)
	{
		pdf_document *doc = pdf_get_bound_document(ctx, obj);

		if (item->uri[0] == '#')
			pdf_dict_put_drop(ctx, obj, PDF_NAME(Dest),
				pdf_new_dest_from_link(ctx, doc, item->uri, 0));
		else if (!strncmp(item->uri, "file:", 5))
			pdf_dict_put_drop(ctx, obj, PDF_NAME(Dest),
				pdf_new_dest_from_link(ctx, doc, item->uri, 1));
		else
			pdf_dict_put_drop(ctx, obj, PDF_NAME(A),
				pdf_new_action_from_link(ctx, doc, item->uri));
	}
}

static int
pdf_outline_iterator_insert(fz_context *ctx, fz_outline_iterator *iter_, fz_outline_item *item)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_document *doc = (pdf_document *)iter->super.doc;
	pdf_obj *obj = NULL;
	pdf_obj *prev;
	pdf_obj *parent;
	pdf_obj *outlines = NULL;
	int result = 0;

	fz_var(obj);
	fz_var(outlines);

	pdf_begin_operation(ctx, doc, "Insert outline item");

	fz_try(ctx)
	{
		obj = pdf_add_new_dict(ctx, doc, 4);

		if (iter->modifier == MOD_BELOW)
			parent = iter->current;
		else if (iter->modifier == MOD_NONE && iter->current == NULL)
		{
			pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			outlines = pdf_dict_get(ctx, root, PDF_NAME(Outlines));
			if (outlines == NULL)
			{
				/* No outlines entry, better make one. */
				outlines = pdf_add_new_dict(ctx, doc, 4);
				pdf_dict_put(ctx, root, PDF_NAME(Outlines), outlines);
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
			else
				pdf_dict_put(ctx, parent, PDF_NAME(First), obj);
			pdf_dict_put(ctx, iter->current, PDF_NAME(Prev), obj);
			pdf_dict_put(ctx, obj, PDF_NAME(Next), iter->current);
			result = 0;
			break;
		}
		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
		pdf_drop_obj(ctx, outlines);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}

	return result;
}

static void
pdf_outline_iterator_update(fz_context *ctx, fz_outline_iterator *iter_, fz_outline_item *item)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_document *doc = (pdf_document *)iter->super.doc;

	if (iter->modifier != MOD_NONE || iter->current == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't update a non-existent outline item!");

	pdf_begin_operation(ctx, doc, "Update outline item");

	fz_try(ctx)
	{
		do_outline_update(ctx, iter->current, item, 0);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
}

static int
pdf_outline_iterator_del(fz_context *ctx, fz_outline_iterator *iter_)
{
	pdf_outline_iterator *iter = (pdf_outline_iterator *)iter_;
	pdf_document *doc = (pdf_document *)iter->super.doc;
	pdf_obj *next, *prev, *parent;
	int result = 0;
	int count;

	if (iter->modifier != MOD_NONE || iter->current == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't delete a non-existent outline item!");

	prev = pdf_dict_get(ctx, iter->current, PDF_NAME(Prev));
	next = pdf_dict_get(ctx, iter->current, PDF_NAME(Next));
	parent = pdf_dict_get(ctx, iter->current, PDF_NAME(Parent));
	count = pdf_dict_get_int(ctx, iter->current, PDF_NAME(Count));
	/* How many nodes visible from above are being removed? */
	if (count > 0)
		count++; /* Open children, plus this node. */
	else
		count = 1; /* Just this node */

	pdf_begin_operation(ctx, doc, "Delete outline item");

	fz_try(ctx)
	{
		pdf_obj *up = parent;
		while (up)
		{
			int c = pdf_dict_get_int(ctx, up, PDF_NAME(Count));
			pdf_dict_put_int(ctx, up, PDF_NAME(Count), (c > 0 ? c - count : c + count));
			if (c < 0)
				break;
			up = pdf_dict_get(ctx, up, PDF_NAME(Parent));
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
			{
				pdf_dict_put(ctx, parent, PDF_NAME(First), next);
				pdf_dict_del(ctx, next, PDF_NAME(Prev));
			}
			iter->current = next;
		}
		else if (prev)
		{
			iter->current = prev;
			pdf_dict_put(ctx, parent, PDF_NAME(Last), prev);
		}
		else if (parent)
		{
			iter->current = parent;
			iter->modifier = MOD_BELOW;
			pdf_dict_del(ctx, parent, PDF_NAME(First));
			pdf_dict_del(ctx, parent, PDF_NAME(Last));
			result = 1;
		}
		else
		{
			iter->current = NULL;
			result = 1;
		}
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}

	return result;
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

	iter->item.is_open = pdf_dict_get_int(ctx, iter->current, PDF_NAME(Count)) > 0;

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
	pdf_mark_bits *marks;
	pdf_outline_iterator *iter = NULL;
	int fixed = 0;

	/* Walk the outlines to spot problems that might bite us later
	 * (in particular, for cycles). */
	marks = pdf_new_mark_bits(ctx, doc);
	fz_try(ctx)
	{
		root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
		obj = pdf_dict_get(ctx, root, PDF_NAME(Outlines));
		first = pdf_dict_get(ctx, obj, PDF_NAME(First));
		if (first)
		{
			/* cache page tree for fast link destination lookups. This
			 * will be dropped 'just in time' on writes to the doc. */
			pdf_load_page_tree(ctx, doc);
			fz_try(ctx)
			{
				/* Pass through the outlines once, fixing inconsistencies */
				pdf_test_outline(ctx, doc, first, marks, obj, &fixed);

				if (fixed)
				{
					/* If a fix was performed, pass through again,
					 * this time throwing if it's still not correct. */
					pdf_mark_bits_reset(ctx, marks);
					pdf_test_outline(ctx, doc, first, marks, obj, NULL);
					pdf_end_operation(ctx, doc);
				}
			}
			fz_catch(ctx)
			{
				if (fixed)
					pdf_abandon_operation(ctx, doc);
				fz_rethrow(ctx);
			}
		}
	}
	fz_always(ctx)
		pdf_drop_mark_bits(ctx, marks);
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
