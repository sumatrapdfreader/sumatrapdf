#include "fitz-internal.h"
#include "mupdf-internal.h"

static fz_outline *
pdf_load_outline_imp(pdf_document *xref, pdf_obj *dict)
{
	fz_context *ctx = xref->ctx;
	fz_outline *node, **prev, *first;
	pdf_obj *obj;
	pdf_obj *odict = dict;

	fz_var(dict);

	fz_try(ctx)
	{
		first = NULL;
		prev = &first;
		while (dict && pdf_is_dict(dict))
		{
			if (pdf_dict_mark(dict))
				break;
			node = fz_malloc_struct(ctx, fz_outline);
			node->title = NULL;
			node->dest.kind = FZ_LINK_NONE;
			node->down = NULL;
			node->next = NULL;
			*prev = node;
			prev = &node->next;

			obj = pdf_dict_gets(dict, "Title");
			if (obj)
				node->title = pdf_to_utf8(xref, obj);

			/* SumatraPDF: support expansion states */
			node->is_open = pdf_to_int(pdf_dict_gets(dict, "Count")) >= 0;

			if ((obj = pdf_dict_gets(dict, "Dest")))
				node->dest = pdf_parse_link_dest(xref, obj);
			else if ((obj = pdf_dict_gets(dict, "A")))
				node->dest = pdf_parse_action(xref, obj);

			obj = pdf_dict_gets(dict, "First");
			if (obj)
				node->down = pdf_load_outline_imp(xref, obj);

			dict = pdf_dict_gets(dict, "Next");
		}
	}
	fz_always(ctx)
	{
		for (dict = odict; dict && pdf_dict_marked(dict); dict = pdf_dict_gets(dict, "Next"))
			pdf_dict_unmark(dict);
	}
	fz_catch(ctx)
	{
		/* SumatraPDF: fix memory leak */
		fz_free_outline(ctx, first);
		fz_rethrow(ctx);
	}

	return first;
}

fz_outline *
pdf_load_outline(pdf_document *xref)
{
	pdf_obj *root, *obj, *first;

	root = pdf_dict_gets(xref->trailer, "Root");
	obj = pdf_dict_gets(root, "Outlines");
	first = pdf_dict_gets(obj, "First");
	if (first)
		return pdf_load_outline_imp(xref, first);

	return NULL;
}
