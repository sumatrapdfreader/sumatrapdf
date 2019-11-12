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
	return 1;
}

static fz_outline *
pdf_load_outline_imp(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	fz_outline *node, **prev, *first = NULL;
	pdf_obj *obj;
	pdf_obj *odict = dict;

	fz_var(dict);
	fz_var(first);

	fz_try(ctx)
	{
		prev = &first;
		while (dict && pdf_is_dict(ctx, dict))
		{
			if (pdf_mark_obj(ctx, dict))
				break;
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
				node->has_color = pdf_parse_color(ctx, obj, &(node->color[0]));
			}

			obj = pdf_dict_get(ctx, dict, PDF_NAME(First));
			if (obj)
			{
				node->down = pdf_load_outline_imp(ctx, doc, obj);

				obj = pdf_dict_get(ctx, dict, PDF_NAME(Count));
				if (pdf_to_int(ctx, obj) > 0)
					node->is_open = 1;
			}

			dict = pdf_dict_get(ctx, dict, PDF_NAME(Next));
		}
	}
	fz_always(ctx)
	{
		for (dict = odict; dict && pdf_obj_marked(ctx, dict); dict = pdf_dict_get(ctx, dict, PDF_NAME(Next)))
			pdf_unmark_obj(ctx, dict);
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
	pdf_obj *root, *obj, *first;
	fz_outline *outline = NULL;

	root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	obj = pdf_dict_get(ctx, root, PDF_NAME(Outlines));
	first = pdf_dict_get(ctx, obj, PDF_NAME(First));
	if (first)
	{
		/* cache page tree for fast link destination lookups */
		pdf_load_page_tree(ctx, doc);
		fz_try(ctx)
			outline = pdf_load_outline_imp(ctx, doc, first);
		fz_always(ctx)
			pdf_drop_page_tree(ctx, doc);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	return outline;
}
