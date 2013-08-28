#include "mupdf/pdf.h"

static fz_outline *
pdf_load_outline_imp(pdf_document *doc, pdf_obj *dict)
{
	fz_context *ctx = doc->ctx;
	fz_outline *node, **prev, *first;
	pdf_obj *obj;
	pdf_obj *odict = dict;

	fz_var(dict);
	fz_var(first);

	fz_try(ctx)
	{
		first = NULL;
		prev = &first;
		while (dict && pdf_is_dict(dict))
		{
			if (pdf_mark_obj(dict))
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
				node->title = pdf_to_utf8(doc, obj);

			/* SumatraPDF: support expansion states */
			node->is_open = pdf_to_int(pdf_dict_gets(dict, "Count")) >= 0;

			/* SumatraPDF: tolerate invalid link destinations and actions */
			fz_try(ctx)
			{

			if ((obj = pdf_dict_gets(dict, "Dest")))
				node->dest = pdf_parse_link_dest(doc, FZ_LINK_GOTO, obj);
			else if ((obj = pdf_dict_gets(dict, "A")))
				node->dest = pdf_parse_action(doc, obj);

			}
			fz_catch(ctx) { }

			obj = pdf_dict_gets(dict, "First");
			if (obj)
				node->down = pdf_load_outline_imp(doc, obj);

			dict = pdf_dict_gets(dict, "Next");
		}
	}
	fz_always(ctx)
	{
		for (dict = odict; dict && pdf_obj_marked(dict); dict = pdf_dict_gets(dict, "Next"))
			pdf_unmark_obj(dict);
	}
	fz_catch(ctx)
	{
		fz_free_outline(ctx, first);
		fz_rethrow(ctx);
	}

	return first;
}

fz_outline *
pdf_load_outline(pdf_document *doc)
{
	pdf_obj *root, *obj, *first;

	root = pdf_dict_gets(pdf_trailer(doc), "Root");
	obj = pdf_dict_gets(root, "Outlines");
	first = pdf_dict_gets(obj, "First");
	if (first)
		return pdf_load_outline_imp(doc, first);

	return NULL;
}
