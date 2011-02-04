#include "fitz.h"
#include "mupdf.h"

static pdf_outline *
pdf_loadoutlineimp(pdf_xref *xref, fz_obj *dict)
{
	pdf_outline *node;
	fz_obj *obj;

	if (fz_isnull(dict))
		return nil;

	node = fz_malloc(sizeof(pdf_outline));
	node->title = nil;
	node->link = nil;
	node->child = nil;
	node->next = nil;
	node->count = 0;

	pdf_logpage("load outline {\n");

	obj = fz_dictgets(dict, "Title");
	if (obj)
	{
		node->title = pdf_toutf8(obj);
		pdf_logpage("title %s\n", node->title);
	}

	obj = fz_dictgets(dict, "Count");
	if (obj)
	{
		node->count = fz_toint(obj);
	}

	if (fz_dictgets(dict, "Dest") || fz_dictgets(dict, "A"))
	{
		node->link = pdf_loadlink(xref, dict);
	}

	obj = fz_dictgets(dict, "First");
	if (obj)
	{
		node->child = pdf_loadoutlineimp(xref, obj);
	}

	pdf_logpage("}\n");

	obj = fz_dictgets(dict, "Next");
	if (obj)
	{
		node->next = pdf_loadoutlineimp(xref, obj);
	}

	return node;
}

pdf_outline *
pdf_loadoutline(pdf_xref *xref)
{
	pdf_outline *node;
	fz_obj *root, *obj, *first;

	pdf_logpage("load outlines {\n");

	node = nil;

	root = fz_dictgets(xref->trailer, "Root");
	obj = fz_dictgets(root, "Outlines");
	if (obj)
	{
		first = fz_dictgets(obj, "First");
		if (first)
			node = pdf_loadoutlineimp(xref, first);
	}

	pdf_logpage("}\n");

	return node;
}

void
pdf_freeoutline(pdf_outline *outline)
{
	if (outline->child)
		pdf_freeoutline(outline->child);
	if (outline->next)
		pdf_freeoutline(outline->next);
	if (outline->link)
		pdf_freelink(outline->link);
	fz_free(outline->title);
	fz_free(outline);
}

void
pdf_debugoutline(pdf_outline *outline, int level)
{
	int i;
	while (outline)
	{
		for (i = 0; i < level; i++)
			putchar(' ');

		if (outline->title)
			printf("%s ", outline->title);
		else
			printf("<nil> ");

		if (outline->link)
			fz_debugobj(outline->link->dest);
		else
			printf("<nil>\n");

		if (outline->child)
			pdf_debugoutline(outline->child, level + 2);

		outline = outline->next;
	}
}
