#include "fitz.h"
#include "mupdf.h"

static fz_outline *
pdf_load_outline_imp(pdf_xref *xref, fz_obj *dict)
{
	pdf_link *link;
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
		obj = fz_new_null();
		fz_dict_puts(dict, ".seen", obj);
		fz_drop_obj(obj);

	node = fz_malloc(sizeof(fz_outline));
	node->title = NULL;
	node->page = -1;
	node->down = NULL;
	node->next = NULL;

	obj = fz_dict_gets(dict, "Title");
	if (obj)
		node->title = pdf_to_utf8(obj);

	/* SumatraPDF: support expansion states */
	node->is_open = fz_to_int(fz_dict_gets(dict, "Count")) >= 0;
	/* SumatraPDF: extended outline actions */
	node->data = node->free_data = NULL;

	if (fz_dict_gets(dict, "Dest") || fz_dict_gets(dict, "A"))
	{
		link = pdf_load_link(xref, dict);
		if (link->kind == PDF_LINK_GOTO)
			node->page = pdf_find_page_number(xref, fz_array_get(link->dest, 0));
		/* SumatraPDF: extended outline actions */
		node->data = link;
		node->free_data = pdf_free_link;
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
