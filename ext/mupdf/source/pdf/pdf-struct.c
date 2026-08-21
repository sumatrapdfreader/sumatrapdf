// Copyright (C) 2026 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.


#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

static int
check_attributes(fz_context *ctx, pdf_obj *a)
{
	pdf_obj *o = pdf_dict_get(ctx, a, PDF_NAME(O));

	if (o == PDF_NAME(Table))
	{
		pdf_obj *rs, *cs;
		rs = pdf_dict_get(ctx, a, PDF_NAME(RowSpan));
		cs = pdf_dict_get(ctx, a, PDF_NAME(ColSpan));
		return PDF_STRUCT_HAS_TABLE_ATTRIBUTES | ((rs || cs) ? PDF_STRUCT_HAS_TABLE_SPAN_ATTRIBUTES : 0);
	}

	return 0;
}

static pdf_check_structure_result
ste(fz_context *ctx, pdf_obj *parent, pdf_obj *obj, pdf_cycle_list *cycle_up)
{
	pdf_obj *o;
	pdf_check_structure_result ret = 0;
	pdf_cycle_list cycle;

	if (pdf_cycle(ctx, &cycle, cycle_up, obj))
		return PDF_STRUCT_BROKEN | PDF_STRUCT_HAS_CYCLE;

	if (pdf_is_number(ctx, obj))
	{
		/* FIXME: */
		return 0;
	}

	o = pdf_dict_get(ctx, obj, PDF_NAME(Type));
	if (o == PDF_NAME(OBJR) || o == PDF_NAME(MCR))
	{
		/* Object reference or Marked Content Reference */
		return 0;
	}
	if (o != NULL && o != PDF_NAME(StructElem))
	{
		fz_warn(ctx, "Type should be StructElem");
		return PDF_STRUCT_BROKEN;
	}

	o = pdf_dict_get(ctx, obj, PDF_NAME(S));
	if (o == NULL)
	{
		fz_warn(ctx, "Missing structure type (S)");
		return PDF_STRUCT_BROKEN;
	}

	o = pdf_dict_get(ctx, obj, PDF_NAME(P));
	if (o == NULL)
	{
		fz_warn(ctx, "Repairing missing parent (P)");
		pdf_dict_put(ctx, obj, PDF_NAME(P), pdf_ensure_indirect(ctx, parent));
		ret |= PDF_STRUCT_FIXED;
	}
	else if (pdf_objcmp_resolve(ctx, o, parent) != 0)
	{
		fz_warn(ctx, "Bad parent (P)");
		return PDF_STRUCT_BROKEN;
	}

	o = pdf_dict_get(ctx, obj, PDF_NAME(A));
	if (o)
	{
		ret |= PDF_STRUCT_HAS_ATTRIBUTES;
		if (pdf_is_array(ctx, o))
		{
			int i, n = pdf_array_len(ctx, o);
			for (i = 0; i < n; i++)
				ret |= check_attributes(ctx, pdf_array_get(ctx, o, i));
		}
		else
			ret |= check_attributes(ctx, o);
	}

	o = pdf_dict_get(ctx, obj, PDF_NAME(K));
	if (o == 0)
		return 0;
	if (pdf_is_array(ctx, o))
	{
		int i, n = pdf_array_len(ctx, o);
		for (i = 0; i < n; i++)
			ret |= ste(ctx, obj, pdf_array_get(ctx, o, i), &cycle);
	}
	else
		ret |= ste(ctx, obj, o, &cycle);

	return ret;
}

static pdf_check_structure_result
check_num(fz_context *ctx, pdf_obj *str, pdf_obj *num)
{
	pdf_obj *p;

	if (num == NULL)
		return 0;

	/* We've reached a struct tree entry from the num tree.
	 * All too often, these aren't properly linked into the
	 * struct tree root. Fix them up as direct children of
	 * the root if we have no other choice. */
	while (1)
	{
		if (pdf_objcmp_resolve(ctx, num, str) == 0)
			return 0; /* We reached the root */
		p = pdf_dict_get(ctx, num, PDF_NAME(P));
		if (p == NULL)
		{
			fz_warn(ctx, "Repairing missing parent (P) in parent tree nodes");
			pdf_dict_put(ctx, num, PDF_NAME(P), pdf_new_indirect(ctx, pdf_get_bound_document(ctx, num), pdf_to_num(ctx, str), pdf_to_gen(ctx, str)));
			return PDF_STRUCT_FIXED;
		}
		num = p;
	}

	return 0;
}

static pdf_check_structure_result
check_nums(fz_context *ctx, pdf_obj *str, pdf_obj *nums)
{
	int i, n;
	pdf_check_structure_result ret = 0;

	if (pdf_is_array(ctx, nums))
	{
		n = pdf_array_len(ctx, nums);
		for (i = 0; i < n; i++)
			ret |= check_num(ctx, str, pdf_array_get(ctx, nums, i));
	}
	else
		ret = check_num(ctx, str, nums);

	return ret;
}

static pdf_check_structure_result
pte(fz_context *ctx, pdf_obj *str, pdf_obj *tree, pdf_cycle_list *cycle_up)
{
	pdf_obj *k, *nums;
	int i, n;
	pdf_check_structure_result ret = 0;
	pdf_cycle_list cycle;

	if (pdf_cycle(ctx, &cycle, cycle_up, tree))
		return PDF_STRUCT_BROKEN | PDF_STRUCT_HAS_CYCLE;

	k = pdf_dict_get(ctx, tree, PDF_NAME(Kids));
	if (k == NULL)
	{}
	else if (pdf_is_array(ctx, k))
	{
		n = pdf_array_len(ctx, k);
		for (i = 0; i < n; i++)
			ret |= pte(ctx, str, pdf_array_get(ctx, k, i), &cycle);
	}
	else
		ret |= pte(ctx, str, k, &cycle);

	nums = pdf_dict_get(ctx, tree, PDF_NAME(Nums));
	if (nums == NULL)
	{}
	else if (pdf_is_array(ctx, nums))
	{
		n = pdf_array_len(ctx, nums);
		for (i = 1; i < n; i += 2)
			ret |= check_nums(ctx, str, pdf_array_get(ctx, nums, i));
	}

	return ret;
}

pdf_check_structure_result
pdf_check_structure_tree(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *str, *k;
	int i, n;
	pdf_check_structure_result ret = PDF_STRUCT_PRESENT;

	if (doc == NULL)
		return PDF_STRUCT_NOT_PRESENT;

	if (doc->struct_tree_repaired)
		return doc->struct_tree_result;

	doc->struct_tree_repaired = 1;
	doc->struct_tree_result = PDF_STRUCT_BROKEN; // In case we throw.

	str = pdf_dict_get(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)), PDF_NAME(StructTreeRoot));
	if (str == NULL)
	{
		doc->struct_tree_result = PDF_STRUCT_NOT_PRESENT;
		return PDF_STRUCT_NOT_PRESENT;
	}

	k = pdf_dict_get(ctx, str, PDF_NAME(K));
	if (k == NULL)
	{}
	else if (pdf_is_array(ctx, k))
	{
		n = pdf_array_len(ctx, k);
		for (i = 0; i < n; i++)
		{
			ret |= ste(ctx, str, pdf_array_get(ctx, k, i), NULL);
		}
	}
	else
	{
		ret |= ste(ctx, str, k, NULL);
	}

	/* And fixup the parent tree entries. */
	ret |= pte(ctx, str, pdf_dict_get(ctx, str, PDF_NAME(ParentTree)), NULL);

	doc->struct_tree_result = ret;

	return ret;
}
