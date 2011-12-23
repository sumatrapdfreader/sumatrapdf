#include "fitz.h"
#include "mupdf.h"

static fz_outline *
pdf_load_outline_imp(pdf_xref *xref, fz_obj *dict)
{
	fz_context *ctx = xref->ctx;
	fz_outline *node;
	fz_obj *obj;
	/* SumatraPDF: prevent potential stack overflow */
	fz_outline *prev, *root = NULL;
	fz_obj *origDict = dict;

	if (fz_is_null(dict))
		return NULL;

	/* SumatraPDF: prevent cyclic outlines */
	do
	{
		if (fz_dict_gets(dict, ".seen"))
			break;
		obj = fz_new_null(ctx);
		fz_dict_puts(dict, ".seen", obj);
		fz_drop_obj(obj);

	node = fz_malloc_struct(ctx, fz_outline);
	node->ctx = ctx;
	node->title = NULL;
	node->page = -1;
	node->down = NULL;
	node->next = NULL;

	obj = fz_dict_gets(dict, "Title");
	if (obj)
		node->title = pdf_to_utf8(ctx, obj);

	/* SumatraPDF: support expansion states */
	node->is_open = fz_to_int(fz_dict_gets(dict, "Count")) >= 0;
	node->link = NULL; /* SumatraPDF: extended outline actions */

	if (fz_dict_gets(dict, "Dest") || fz_dict_gets(dict, "A"))
	{
		fz_link_dest ld = pdf_parse_link_dest(xref, dict);
		node->page = ld.gotor.page;
		/* SumatraPDF: extended outline actions */
		node->link = pdf_load_link(xref, dict);
		/* SumatraPDF: fix page number detection */
		if (node->link && node->link->kind == FZ_LINK_GOTO)
			node->page = node->link->dest.gotor.page;
		else
			node->page = -1;
	}

	obj = fz_dict_gets(dict, "First");
	if (obj)
		node->down = pdf_load_outline_imp(xref, obj);

		/* SumatraPDF: prevent potential stack overflow */
		if (!root)
			prev = root = node;
		else
			prev = prev->next = node;

		dict = fz_dict_gets(dict, "Next");
	} while (dict && !fz_is_null(dict));
	node = root;

	/* SumatraPDF: prevent cyclic outlines */
	for (dict = origDict; dict && fz_dict_gets(dict, ".seen"); dict = fz_dict_gets(dict, "Next"))
		fz_dict_dels(dict, ".seen");

	return node;
}

fz_outline *
pdf_load_outline(pdf_xref *xref)
{
	fz_obj *root, *obj, *first;

	root = fz_dict_gets(xref->trailer, "Root");
	obj = fz_dict_gets(root, "Outlines");
	first = fz_dict_gets(obj, "First");
	if (first)
		return pdf_load_outline_imp(xref, first);

	return NULL;
}
