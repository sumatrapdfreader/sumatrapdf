#include "fitz.h"
#include "mupdf.h"

struct info
{
	fz_obj *resources;
	fz_obj *mediabox;
	fz_obj *cropbox;
	fz_obj *rotate;
};

int
pdf_getpagecount(pdf_xref *xref)
{
	return xref->pagelen;
}

fz_obj *
pdf_getpageobject(pdf_xref *xref, int number)
{
	if (number > 0 && number <= xref->pagelen)
		return xref->pageobjs[number - 1];
	return nil;
}

fz_obj *
pdf_getpageref(pdf_xref *xref, int number)
{
	if (number > 0 && number <= xref->pagelen)
		return xref->pagerefs[number - 1];
	return nil;
}

int
pdf_findpageobject(pdf_xref *xref, fz_obj *page)
{
	int num = fz_tonum(page);
	int gen = fz_togen(page);
	int i;
	for (i = 0; i < xref->pagelen; i++)
		if (num == fz_tonum(xref->pagerefs[i]) && gen == fz_togen(xref->pagerefs[i]))
			return i + 1;
	return 0;
}

static void
pdf_loadpagetreenode(pdf_xref *xref, fz_obj *node, struct info info)
{
	fz_obj *dict, *kids, *count;
	fz_obj *obj, *tmp;
	int i, n;

	/* prevent infinite recursion */
	if (fz_dictgets(node, ".seen"))
		return;

	kids = fz_dictgets(node, "Kids");
	count = fz_dictgets(node, "Count");

	if (fz_isarray(kids) && fz_isint(count))
	{
		obj = fz_dictgets(node, "Resources");
		if (obj)
			info.resources = obj;
		obj = fz_dictgets(node, "MediaBox");
		if (obj)
			info.mediabox = obj;
		obj = fz_dictgets(node, "CropBox");
		if (obj)
			info.cropbox = obj;
		obj = fz_dictgets(node, "Rotate");
		if (obj)
			info.rotate = obj;

		tmp = fz_newnull();
		fz_dictputs(node, ".seen", tmp);
		fz_dropobj(tmp);

		n = fz_arraylen(kids);
		for (i = 0; i < n; i++)
		{
			obj = fz_arrayget(kids, i);
			pdf_loadpagetreenode(xref, obj, info);
		}

		fz_dictdels(node, ".seen");
	}
	else
	{
		dict = fz_resolveindirect(node);

		if (info.resources && !fz_dictgets(dict, "Resources"))
			fz_dictputs(dict, "Resources", info.resources);
		if (info.mediabox && !fz_dictgets(dict, "MediaBox"))
			fz_dictputs(dict, "MediaBox", info.mediabox);
		if (info.cropbox && !fz_dictgets(dict, "CropBox"))
			fz_dictputs(dict, "CropBox", info.cropbox);
		if (info.rotate && !fz_dictgets(dict, "Rotate"))
			fz_dictputs(dict, "Rotate", info.rotate);

		if (xref->pagelen == xref->pagecap)
		{
			fz_warn("found more pages than expected");
			xref->pagecap ++;
			xref->pagerefs = fz_realloc(xref->pagerefs, xref->pagecap, sizeof(fz_obj*));
			xref->pageobjs = fz_realloc(xref->pageobjs, xref->pagecap, sizeof(fz_obj*));
		}

		xref->pagerefs[xref->pagelen] = fz_keepobj(node);
		xref->pageobjs[xref->pagelen] = fz_keepobj(dict);
		xref->pagelen ++;
	}
}

fz_error
pdf_loadpagetree(pdf_xref *xref)
{
	struct info info;
	fz_obj *catalog = fz_dictgets(xref->trailer, "Root");
	fz_obj *pages = fz_dictgets(catalog, "Pages");
	fz_obj *count = fz_dictgets(pages, "Count");

	if (!fz_isdict(pages))
		return fz_throw("missing page tree");
	if (!fz_isint(count))
		return fz_throw("missing page count");

	xref->pagecap = fz_toint(count);
	xref->pagelen = 0;
	xref->pagerefs = fz_calloc(xref->pagecap, sizeof(fz_obj*));
	xref->pageobjs = fz_calloc(xref->pagecap, sizeof(fz_obj*));

	info.resources = nil;
	info.mediabox = nil;
	info.cropbox = nil;
	info.rotate = nil;

	pdf_loadpagetreenode(xref, pages, info);

	return fz_okay;
}
