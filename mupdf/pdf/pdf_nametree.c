#include "fitz.h"
#include "mupdf.h"

static fz_obj *
pdf_lookup_name_imp(fz_context *ctx, fz_obj *node, fz_obj *needle)
{
	fz_obj *kids = fz_dict_gets(node, "Kids");
	fz_obj *names = fz_dict_gets(node, "Names");

	if (fz_is_array(kids))
	{
		int l = 0;
		int r = fz_array_len(kids) - 1;

		while (l <= r)
		{
			int m = (l + r) >> 1;
			fz_obj *kid = fz_array_get(kids, m);
			fz_obj *limits = fz_dict_gets(kid, "Limits");
			fz_obj *first = fz_array_get(limits, 0);
			fz_obj *last = fz_array_get(limits, 1);

			if (fz_objcmp(needle, first) < 0)
				r = m - 1;
			else if (fz_objcmp(needle, last) > 0)
				l = m + 1;
			else
			{
				fz_obj *obj;

				if (fz_dict_mark(node))
					break;
				obj = pdf_lookup_name_imp(ctx, kid, needle);
				fz_dict_unmark(node);
				return obj;
			}
		}
	}

	if (fz_is_array(names))
	{
		int l = 0;
		int r = (fz_array_len(names) / 2) - 1;

		while (l <= r)
		{
			int m = (l + r) >> 1;
			int c;
			fz_obj *key = fz_array_get(names, m * 2);
			fz_obj *val = fz_array_get(names, m * 2 + 1);

			c = fz_objcmp(needle, key);
			if (c < 0)
				r = m - 1;
			else if (c > 0)
				l = m + 1;
			else
				return val;
		}

		/* Spec says names should be sorted (hence the binary search,
		 * above), but Acrobat copes with non-sorted. Drop back to a
		 * simple search if the binary search fails. */
		r = fz_array_len(names)/2;
		for (l = 0; l < r; l++)
			if (!fz_objcmp(needle, fz_array_get(names, l * 2)))
				return fz_array_get(names, l * 2 + 1);
	}

	return NULL;
}

fz_obj *
pdf_lookup_name(pdf_xref *xref, char *which, fz_obj *needle)
{
	fz_context *ctx = xref->ctx;

	fz_obj *root = fz_dict_gets(xref->trailer, "Root");
	fz_obj *names = fz_dict_gets(root, "Names");
	fz_obj *tree = fz_dict_gets(names, which);
	return pdf_lookup_name_imp(ctx, tree, needle);
}

fz_obj *
pdf_lookup_dest(pdf_xref *xref, fz_obj *needle)
{
	fz_context *ctx = xref->ctx;

	fz_obj *root = fz_dict_gets(xref->trailer, "Root");
	fz_obj *dests = fz_dict_gets(root, "Dests");
	fz_obj *names = fz_dict_gets(root, "Names");
	fz_obj *dest = NULL;

	/* PDF 1.1 has destinations in a dictionary */
	if (dests)
	{
		if (fz_is_name(needle))
			return fz_dict_get(dests, needle);
		else
			return fz_dict_gets(dests, fz_to_str_buf(needle));
	}

	/* PDF 1.2 has destinations in a name tree */
	if (names && !dest)
	{
		fz_obj *tree = fz_dict_gets(names, "Dests");
		return pdf_lookup_name_imp(ctx, tree, needle);
	}

	return NULL;
}

static void
pdf_load_name_tree_imp(fz_obj *dict, pdf_xref *xref, fz_obj *node)
{
	fz_context *ctx = xref->ctx;
	fz_obj *kids = fz_dict_gets(node, "Kids");
	fz_obj *names = fz_dict_gets(node, "Names");
	int i;

	if (kids && !fz_dict_mark(node))
	{
		for (i = 0; i < fz_array_len(kids); i++)
			pdf_load_name_tree_imp(dict, xref, fz_array_get(kids, i));
		fz_dict_unmark(node);
	}

	if (names)
	{
		for (i = 0; i + 1 < fz_array_len(names); i += 2)
		{
			fz_obj *key = fz_array_get(names, i);
			fz_obj *val = fz_array_get(names, i + 1);
			if (fz_is_string(key))
			{
				key = pdf_to_utf8_name(ctx, key);
				fz_dict_put(dict, key, val);
				fz_drop_obj(key);
			}
			else if (fz_is_name(key))
			{
				fz_dict_put(dict, key, val);
			}
		}
	}
}

fz_obj *
pdf_load_name_tree(pdf_xref *xref, char *which)
{
	fz_context *ctx = xref->ctx;

	fz_obj *root = fz_dict_gets(xref->trailer, "Root");
	fz_obj *names = fz_dict_gets(root, "Names");
	fz_obj *tree = fz_dict_gets(names, which);
	if (fz_is_dict(tree))
	{
		fz_obj *dict = fz_new_dict(ctx, 100);
		pdf_load_name_tree_imp(dict, xref, tree);
		return dict;
	}
	return NULL;
}
