#include "fitz.h"
#include "mupdf.h"

static fz_error
loadnametreenode(fz_obj *tree, pdf_xref *xref, fz_obj *node)
{
	fz_error error;
	fz_obj *names;
	fz_obj *kids;
	fz_obj *key;
	fz_obj *val;
	int i, len;

	names = fz_dictgets(node, "Names");
	if (names)
	{
		len = fz_arraylen(names) / 2;

		for (i = 0; i < len; ++i)
		{
			key = fz_arrayget(names, i * 2 + 0);
			val = fz_arrayget(names, i * 2 + 1);

			error = fz_dictput(tree, key, val);
			if (error)
				return fz_rethrow(error, "cannot insert name tree entry");

			fz_sortdict(tree);
		}
	}

	kids = fz_dictgets(node, "Kids");
	if (kids)
	{
		len = fz_arraylen(kids);
		for (i = 0; i < len; ++i)
		{
			error = loadnametreenode(tree, xref, fz_arrayget(kids, i));
			if (error)
				return fz_rethrow(error, "cannot load name tree node");
		}
	}

	return fz_okay;
}

fz_error
pdf_loadnametree(fz_obj **dictp, pdf_xref *xref, fz_obj *root)
{
	fz_error error;
	fz_obj *tree;

	error = fz_newdict(&tree, 128);
	if (error)
		return fz_rethrow(error, "cannot create name tree dictionary");

	error = loadnametreenode(tree, xref, root);
	if (error)
	{
		fz_dropobj(tree);
		return fz_rethrow(error, "cannot load name tree");
	}

	fz_sortdict(tree);

	*dictp = tree;
	return fz_okay;
}

fz_error
pdf_loadnametrees(pdf_xref *xref)
{
	fz_error error;
	fz_obj *names;
	fz_obj *dests;

	/* PDF 1.1 */
	dests = fz_dictgets(xref->root, "Dests");
	if (dests)
	{
		xref->dests = fz_keepobj(dests);
		return fz_okay;
	}

	/* PDF 1.2 */
	names = fz_dictgets(xref->root, "Names");
	if (names)
	{
		dests = fz_dictgets(names, "Dests");
		if (dests)
		{
			error = pdf_loadnametree(&xref->dests, xref, dests);
			if (error)
				return fz_rethrow(error, "cannot load name tree");
		}
	}

	return fz_okay;
}

