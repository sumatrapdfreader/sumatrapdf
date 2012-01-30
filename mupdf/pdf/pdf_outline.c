#include "fitz.h"
#include "mupdf.h"

static fz_outline *
pdf_load_outline_imp(pdf_document *xref, fz_obj *dict)
{
	fz_context *ctx = xref->ctx;
	fz_outline *node, **prev, *first;
	fz_obj *obj;
	fz_obj *odict = dict;

	fz_var(dict);

	fz_try(ctx)
	{
		first = NULL;
		prev = &first;
		while (dict && fz_is_dict(dict))
		{
			if (fz_dict_mark(dict))
				break;
			node = fz_malloc_struct(ctx, fz_outline);
			node->title = NULL;
			node->dest.kind = FZ_LINK_NONE;
			node->down = NULL;
			node->next = NULL;
			*prev = node;
			prev = &node->next;

			obj = fz_dict_gets(dict, "Title");
			if (obj)
				node->title = pdf_to_utf8(ctx, obj);

			/* SumatraPDF: support expansion states */
			node->is_open = fz_to_int(fz_dict_gets(dict, "Count")) >= 0;

			if ((obj = fz_dict_gets(dict, "Dest")))
				node->dest = pdf_parse_link_dest(xref, obj);
			else if ((obj = fz_dict_gets(dict, "A")))
				node->dest = pdf_parse_action(xref, obj);

			obj = fz_dict_gets(dict, "First");
			if (obj)
				node->down = pdf_load_outline_imp(xref, obj);

			dict = fz_dict_gets(dict, "Next");
		}
	}
	fz_catch(ctx)
	{
		for (dict = odict; dict && fz_dict_marked(dict); dict = fz_dict_gets(dict, "Next"))
			fz_dict_unmark(dict);
		fz_rethrow(ctx);
	}

	for (dict = odict; dict && fz_dict_marked(dict); dict = fz_dict_gets(dict, "Next"))
		fz_dict_unmark(dict);

	return first;
}

fz_outline *
pdf_load_outline(pdf_document *xref)
{
	fz_obj *root, *obj, *first;

	root = fz_dict_gets(xref->trailer, "Root");
	obj = fz_dict_gets(root, "Outlines");
	first = fz_dict_gets(obj, "First");
	if (first)
		return pdf_load_outline_imp(xref, first);

	return NULL;
}
