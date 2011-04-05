#include "fitz.h"
#include "mupdf.h"

static pdf_outline *
pdf_load_outline_imp(pdf_xref *xref, fz_obj *dict)
{
	pdf_outline *node;
	fz_obj *obj;

	if (fz_is_null(dict))
		return NULL;

	node = fz_malloc(sizeof(pdf_outline));
	node->title = NULL;
	node->link = NULL;
	node->child = NULL;
	node->next = NULL;
	node->count = 0;

	pdf_log_page("load outline {\n");

	obj = fz_dict_gets(dict, "Title");
	if (obj)
	{
		node->title = pdf_to_utf8(obj);
		pdf_log_page("title %s\n", node->title);
	}

	obj = fz_dict_gets(dict, "Count");
	if (obj)
	{
		node->count = fz_to_int(obj);
	}

	if (fz_dict_gets(dict, "Dest") || fz_dict_gets(dict, "A"))
	{
		node->link = pdf_load_link(xref, dict);
	}

	obj = fz_dict_gets(dict, "First");
	if (obj)
	{
		node->child = pdf_load_outline_imp(xref, obj);
	}

	pdf_log_page("}\n");

	obj = fz_dict_gets(dict, "Next");
	if (obj)
	{
		node->next = pdf_load_outline_imp(xref, obj);
	}

	return node;
}

pdf_outline *
pdf_load_outline(pdf_xref *xref)
{
	pdf_outline *node;
	fz_obj *root, *obj, *first;

	pdf_log_page("load outlines {\n");

	node = NULL;

	root = fz_dict_gets(xref->trailer, "Root");
	obj = fz_dict_gets(root, "Outlines");
	if (obj)
	{
		first = fz_dict_gets(obj, "First");
		if (first)
			node = pdf_load_outline_imp(xref, first);
	}

	pdf_log_page("}\n");

	return node;
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
