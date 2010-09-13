#include "fitz.h"
#include "mupdf.h"

static fz_obj *
pdf_lookupnameimp(fz_obj *node, fz_obj *needle)
{
	fz_obj *kids = fz_dictgets(node, "Kids");
	fz_obj *names = fz_dictgets(node, "Names");

	if (fz_isarray(kids))
	{
		int l = 0;
		int r = fz_arraylen(kids) - 1;

		while (l <= r)
		{
			int m = (l + r) >> 1;
			fz_obj *kid = fz_arrayget(kids, m);
			fz_obj *limits = fz_dictgets(kid, "Limits");
			fz_obj *first = fz_arrayget(limits, 0);
			fz_obj *last = fz_arrayget(limits, 1);

			if (fz_objcmp(needle, first) < 0)
				r = m - 1;
			else if (fz_objcmp(needle, last) > 0)
				l = m + 1;
			else
				return pdf_lookupnameimp(kid, needle);
		}
	}

	if (fz_isarray(names))
	{
		int l = 0;
		int r = (fz_arraylen(names) / 2) - 1;

		while (l <= r)
		{
			int m = (l + r) >> 1;
			int c;
			fz_obj *key = fz_arrayget(names, m * 2);
			fz_obj *val = fz_arrayget(names, m * 2 + 1);

			c = fz_objcmp(needle, key);
			if (c < 0)
				r = m - 1;
			else if (c > 0)
				l = m + 1;
			else
				return val;
		}
	}

	return nil;
}

fz_obj *
pdf_lookupname(pdf_xref *xref, char *which, fz_obj *needle)
{
	fz_obj *root = fz_dictgets(xref->trailer, "Root");
	fz_obj *names = fz_dictgets(root, "Names");
	fz_obj *tree = fz_dictgets(names, which);
	return pdf_lookupnameimp(tree, needle);
}

fz_obj *
pdf_lookupdest(pdf_xref *xref, fz_obj *needle)
{
	fz_obj *root = fz_dictgets(xref->trailer, "Root");
	fz_obj *dests = fz_dictgets(root, "Dests");
	fz_obj *names = fz_dictgets(root, "Names");
	fz_obj *dest = nil;

	/* PDF 1.1 has destinations in a dictionary */
	if (dests)
	{
		if (fz_isname(needle))
			return fz_dictget(dests, needle);
		else
			return fz_dictgets(dests, fz_tostrbuf(needle));
	}

	/* PDF 1.2 has destinations in a name tree */
	if (names && !dest)
	{
		fz_obj *tree = fz_dictgets(names, "Dests");
		return pdf_lookupnameimp(tree, needle);
	}

	return nil;
}

static void
pdf_loadnametreeimp(fz_obj *dict, pdf_xref *xref, fz_obj *node)
{
	fz_obj *kids = fz_dictgets(node, "Kids");
	fz_obj *names = fz_dictgets(node, "Names");
	int i;

	if (kids)
	{
		for (i = 0; i < fz_arraylen(kids); i++)
			pdf_loadnametreeimp(dict, xref, fz_arrayget(kids, i));
	}

	if (names)
	{
		for (i = 0; i + 1 < fz_arraylen(names); i += 2)
		{
			fz_obj *key = fz_arrayget(names, i);
			fz_obj *val = fz_arrayget(names, i + 1);
			if (fz_isstring(key))
			{
				key = pdf_toutf8name(key);
				fz_dictput(dict, key, val);
				fz_dropobj(key);
			}
			else if (fz_isname(key))
			{
				fz_dictput(dict, key, val);
			}
		}
	}
}

fz_obj *
pdf_loadnametree(pdf_xref *xref, char *which)
{
	fz_obj *root = fz_dictgets(xref->trailer, "Root");
	fz_obj *names = fz_dictgets(root, "Names");
	fz_obj *tree = fz_dictgets(names, which);
	if (fz_isdict(tree))
	{
		fz_obj *dict = fz_newdict(100);
		pdf_loadnametreeimp(dict, xref, tree);
		return dict;
	}
	return nil;
}
