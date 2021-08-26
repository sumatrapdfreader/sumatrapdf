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

/* sumatrapdf: parse "C" color entry */
static int
pdf_parse_color(fz_context *ctx, pdf_obj *arr, float *color)
{
	int i;
	int n;
	pdf_obj *obj;

	if (!pdf_is_array(ctx, arr))
		return 0;
	n = pdf_array_len(ctx, arr);
	if (n < 3 || n > 4) {
		return 0;
	}
	for (i = 0; i < n; i++)
	{
		obj = pdf_array_get(ctx, arr, i);
		color[i] = pdf_to_real(ctx, obj);
	}
	return n;
}

static fz_outline *
pdf_load_outline_imp(fz_context *ctx, pdf_document *doc, pdf_obj *dict, pdf_obj *mark_list)
{
	fz_outline *node, **prev, *first = NULL;
	pdf_obj *obj;

	fz_var(first);

	fz_try(ctx)
	{
		prev = &first;
		while (dict && pdf_is_dict(ctx, dict))
		{
			if (pdf_mark_obj(ctx, dict))
				break;
			pdf_array_push(ctx, mark_list, dict);

			node = fz_new_outline(ctx);
			*prev = node;
			prev = &node->next;

			obj = pdf_dict_get(ctx, dict, PDF_NAME(Title));
			if (obj)
				node->title = Memento_label(fz_strdup(ctx, pdf_to_text_string(ctx, obj)), "outline_title");

			if ((obj = pdf_dict_get(ctx, dict, PDF_NAME(Dest))) != NULL)
				node->uri = Memento_label(pdf_parse_link_dest(ctx, doc, obj), "outline_uri");
			else if ((obj = pdf_dict_get(ctx, dict, PDF_NAME(A))) != NULL)
				node->uri = Memento_label(pdf_parse_link_action(ctx, doc, obj, -1), "outline_uri");
			else
				node->uri = NULL;

			if (node->uri && !fz_is_external_link(ctx, node->uri))
				node->page = pdf_resolve_link(ctx, doc, node->uri, &node->x, &node->y);
			else
				node->page = -1;

			/* sumatrapdf: support color and flags */
			node->flags = pdf_dict_get_int(ctx, dict, PDF_NAME(F));
			if ((obj = pdf_dict_get(ctx, dict, PDF_NAME(C))) != NULL) 
			{
				node->n_color = pdf_parse_color(ctx, obj, node->color);
			}

			obj = pdf_dict_get(ctx, dict, PDF_NAME(First));
			if (obj)
			{
				node->down = pdf_load_outline_imp(ctx, doc, obj, mark_list);

				obj = pdf_dict_get(ctx, dict, PDF_NAME(Count));
				if (pdf_to_int(ctx, obj) > 0)
					node->is_open = 1;
			}

			dict = pdf_dict_get(ctx, dict, PDF_NAME(Next));
		}
	}
	fz_catch(ctx)
	{
		fz_drop_outline(ctx, first);
		fz_rethrow(ctx);
	}

	return first;
}

fz_outline *
pdf_load_outline(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *root, *obj, *first, *mark_list;
	fz_outline *outline = NULL;
	int i;

	mark_list = pdf_new_array(ctx, doc, 100);
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
				outline = pdf_load_outline_imp(ctx, doc, first, mark_list);
		fz_always(ctx)
			pdf_drop_page_tree(ctx, doc);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	}
	fz_always(ctx)
	{
		for (i = 0; i < pdf_array_len(ctx, mark_list); ++i)
			pdf_unmark_obj(ctx, pdf_array_get(ctx, mark_list, i));
		pdf_drop_obj(ctx, mark_list);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return outline;
}
