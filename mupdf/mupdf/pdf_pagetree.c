#include "fitz.h"
#include "mupdf.h"

struct stuff
{
	fz_obj *resources;
	fz_obj *mediabox;
	fz_obj *cropbox;
	fz_obj *rotate;
};

static void
getpagecount(pdf_xref *xref, fz_obj *node, int *pagesp)
{
	fz_obj *type;
	fz_obj *kids;
	fz_obj *count;
	char *typestr;
	int pages = 0;
	int i;

	if (!fz_isdict(node))
	{
		fz_warn("pagetree node is missing, igoring missing pages...");
		return;
	}

	type = fz_dictgets(node, "Type");
	kids = fz_dictgets(node, "Kids");
	count = fz_dictgets(node, "Count");

	if (fz_isname(type))
		typestr = fz_toname(type);
	else
	{
		fz_warn("pagetree node (%d %d R) lacks required type", fz_tonum(node), fz_togen(node));

		kids = fz_dictgets(node, "Kids");
		if (kids)
		{
			fz_warn("guessing it may be a pagetree node, continuing...");
			typestr = "Pages";
		}
		else
		{
			fz_warn("guessing it may be a page, continuing...");
			typestr = "Page";
		}
	}

	if (!strcmp(typestr, "Page"))
		(*pagesp)++;

	else if (!strcmp(typestr, "Pages"))
	{
		if (!fz_isarray(kids))
			fz_warn("page tree node contains no pages");

		pdf_logpage("subtree (%d %d R) {\n", fz_tonum(node), fz_togen(node));

		for (i = 0; i < fz_arraylen(kids); i++)
		{
			fz_obj *obj = fz_arrayget(kids, i);

			/* prevent infinite recursion possible in maliciously crafted PDFs */
			if (obj == node)
			{
				fz_warn("cyclic page tree");
				return;
			}

			getpagecount(xref, obj, &pages);
		}

		if (pages != fz_toint(count))
		{
			fz_warn("page tree node contains incorrect number of pages, continuing...");
			count = fz_newint(pages);
			fz_dictputs(node, "Count", count);
			fz_dropobj(count);
		}

		pdf_logpage("%d pages\n", pages);

		(*pagesp) += pages;

		pdf_logpage("}\n");
	}
}

int
pdf_getpagecount(pdf_xref *xref)
{
	fz_obj *ref;
	fz_obj *catalog;
	fz_obj *pages;
	int count;

	ref = fz_dictgets(xref->trailer, "Root");
	catalog = fz_resolveindirect(ref);

	pages = fz_dictgets(catalog, "Pages");
	pdf_logpage("determining page count (%d %d R) {\n", fz_tonum(pages), fz_togen(pages));

	count = 0;
	getpagecount(xref, pages, &count);

	pdf_logpage("}\n");

	return count;
}

static void
getpageobject(pdf_xref *xref, struct stuff inherit, fz_obj *node, int *pagesp, int pageno, fz_obj **pagep)
{
	char *typestr;
	fz_obj *type;
	fz_obj *kids;
	fz_obj *count;
	fz_obj *inh;
	int i;

	if (!fz_isdict(node))
	{
		fz_warn("pagetree node is missing, ignoring missing pages...");
		*pagep = nil;
		return;
	}

	type = fz_dictgets(node, "Type");
	kids = fz_dictgets(node, "Kids");
	count = fz_dictgets(node, "Count");

	if (fz_isname(type))
		typestr = fz_toname(type);
	else
	{
		fz_warn("pagetree node (%d %d R) lacks required type", fz_tonum(node), fz_togen(node));

		kids = fz_dictgets(node, "Kids");
		if (kids)
		{
			fz_warn("guessing it may be a pagetree node, continuing...");
			typestr = "Pages";
		}
		else
		{
			fz_warn("guessing it may be a page, continuing...");
			typestr = "Page";
		}
	}

	if (!strcmp(typestr, "Page"))
	{
		(*pagesp)++;
		if (*pagesp == pageno)
		{
			pdf_logpage("page %d (%d %d R)\n", *pagesp, fz_tonum(node), fz_togen(node));

			if (inherit.resources && !fz_dictgets(node, "Resources"))
			{
				pdf_logpage("inherited resources\n");
				fz_dictputs(node, "Resources", inherit.resources);
			}

			if (inherit.mediabox && !fz_dictgets(node, "MediaBox"))
			{
				pdf_logpage("inherit mediabox\n");
				fz_dictputs(node, "MediaBox", inherit.mediabox);
			}

			if (inherit.cropbox && !fz_dictgets(node, "CropBox"))
			{
				pdf_logpage("inherit cropbox\n");
				fz_dictputs(node, "CropBox", inherit.cropbox);
			}

			if (inherit.rotate && !fz_dictgets(node, "Rotate"))
			{
				pdf_logpage("inherit rotate\n");
				fz_dictputs(node, "Rotate", inherit.rotate);
			}

			*pagep = node;
		}
	}

	else if (!strcmp(typestr, "Pages"))
	{
		if (!fz_isarray(kids))
			fz_warn("page tree node contains no pages");

		if (*pagesp + fz_toint(count) < pageno)
		{
			(*pagesp) += fz_toint(count);
			return;
		}

		inh = fz_dictgets(node, "Resources");
		if (inh) inherit.resources = inh;

		inh = fz_dictgets(node, "MediaBox");
		if (inh) inherit.mediabox = inh;

		inh = fz_dictgets(node, "CropBox");
		if (inh) inherit.cropbox = inh;

		inh = fz_dictgets(node, "Rotate");
		if (inh) inherit.rotate = inh;

		pdf_logpage("subtree (%d %d R) {\n", fz_tonum(node), fz_togen(node));

		for (i = 0; !(*pagep) && i < fz_arraylen(kids); i++)
		{
			fz_obj *obj = fz_arrayget(kids, i);

			/* prevent infinite recursion possible in maliciously crafted PDFs */
			if (obj == node)
			{
				fz_warn("cyclic page tree");
				return;
			}

			getpageobject(xref, inherit, obj, pagesp, pageno, pagep);
		}

		pdf_logpage("}\n");
	}
}

fz_obj *
pdf_getpageobject(pdf_xref *xref, int pageno)
{
	struct stuff inherit;
	fz_obj *ref;
	fz_obj *catalog;
	fz_obj *pages;
	fz_obj *page;
	int count;

	inherit.resources = nil;
	inherit.mediabox = nil;
	inherit.cropbox = nil;
	inherit.rotate = nil;

	ref = fz_dictgets(xref->trailer, "Root");
	catalog = fz_resolveindirect(ref);

	pages = fz_dictgets(catalog, "Pages");
	pdf_logpage("get page %d (%d %d R) {\n", pageno, fz_tonum(pages), fz_togen(pages));

	page = nil;
	count = 0;
	getpageobject(xref, inherit, pages, &count, pageno, &page);
	if (!page)
		fz_warn("cannot find page %d", pageno);

	pdf_logpage("}\n");

	return page;
}

static void
findpageobject(pdf_xref *xref, fz_obj *node, fz_obj *page, int *pagenop, int *foundp)
{
	char *typestr;
	fz_obj *type;
	fz_obj *kids;
	int i;

	if (!fz_isdict(node))
		return;

	type = fz_dictgets(node, "Type");
	kids = fz_dictgets(node, "Kids");

	if (fz_isname(type))
		typestr = fz_toname(type);
	else
	{
		fz_warn("pagetree node (%d %d R) lacks required type", fz_tonum(node), fz_togen(node));

		kids = fz_dictgets(node, "Kids");
		if (kids)
		{
			fz_warn("guessing it may be a pagetree node, continuing...");
			typestr = "Pages";
		}
		else
		{
			fz_warn("guessing it may be a page, continuing...");
			typestr = "Page";
		}
	}

	if (!strcmp(typestr, "Page"))
	{
		(*pagenop)++;
		if (fz_tonum(node) == fz_tonum(page))
		{
			pdf_logpage("page %d (%d %d R)\n", *pagenop, fz_tonum(node), fz_togen(node));
			*foundp = 1;
		}
	}

	else if (!strcmp(typestr, "Pages"))
	{
		if (!fz_isarray(kids))
			fz_warn("page tree node contains no pages");

		pdf_logpage("subtree (%d %d R) {\n", fz_tonum(node), fz_togen(node));

		for (i = 0; !(*foundp) && i < fz_arraylen(kids); i++)
		{
			fz_obj *obj = fz_arrayget(kids, i);

			/* prevent infinite recursion possible in maliciously crafted PDFs */
			if (obj == node)
			{
				fz_warn("cyclic page tree");
				return;
			}

			findpageobject(xref, obj, page, pagenop, foundp);
		}

		pdf_logpage("}\n");
	}
}

int
pdf_findpageobject(pdf_xref *xref, fz_obj *page)
{
	fz_obj *ref;
	fz_obj *catalog;
	fz_obj *pages;
	int pageno;
	int found;

	ref = fz_dictgets(xref->trailer, "Root");
	catalog = fz_resolveindirect(ref);

	pages = fz_dictgets(catalog, "Pages");
	pdf_logpage("find page object (%d %d R) (%d %d R) {\n", fz_tonum(page), fz_togen(page), fz_tonum(pages), fz_togen(pages));

	pageno = 0;
	found = 0;
	findpageobject(xref, pages, page, &pageno, &found);

	pdf_logpage("}\n");

	if (!found)
		fz_warn("cannot find page object (%d %d R)", fz_tonum(page), fz_togen(page));

	return pageno;
}

