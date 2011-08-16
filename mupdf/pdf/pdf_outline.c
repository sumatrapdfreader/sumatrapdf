#include "fitz.h"
#include "mupdf.h"

static pdf_outline *
pdf_load_outline_imp(pdf_xref *xref, fz_obj *dict)
{
	pdf_outline *node;
	fz_obj *obj;
	/* SumatraPDF: prevent potential stack overflow */
	pdf_outline *prev, *root = NULL;
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

	node = fz_malloc(sizeof(pdf_outline));
	node->title = NULL;
	node->link = NULL;
	node->child = NULL;
	node->next = NULL;
	node->count = 0;

	obj = fz_dict_gets(dict, "Title");
	if (obj)
		node->title = pdf_to_utf8(obj);

	obj = fz_dict_gets(dict, "Count");
	if (obj)
		node->count = fz_to_int(obj);

	if (fz_dict_gets(dict, "Dest") || fz_dict_gets(dict, "A"))
		node->link = pdf_load_link(xref, dict);

	obj = fz_dict_gets(dict, "First");
	if (obj)
		node->child = pdf_load_outline_imp(xref, obj);

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

pdf_outline *
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

void
pdf_free_outline(pdf_outline *outline)
{
	if (outline->child)
		pdf_free_outline(outline->child);
	if (outline->next)
		pdf_free_outline(outline->next);
	if (outline->link)
		pdf_free_link(outline->link);
	fz_free(outline->title);
	fz_free(outline);
}

void
pdf_debug_outline(pdf_outline *outline, int level)
{
	int i;
	while (outline)
	{
		for (i = 0; i < level; i++)
			putchar(' ');

		if (outline->title)
			printf("%s ", outline->title);
		else
			printf("<NULL> ");

		if (outline->link)
			fz_debug_obj(outline->link->dest);
		else
			printf("<NULL>\n");

		if (outline->child)
			pdf_debug_outline(outline->child, level + 2);

		outline = outline->next;
	}
}
