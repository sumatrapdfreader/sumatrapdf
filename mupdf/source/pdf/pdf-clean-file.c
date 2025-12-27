// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#include <string.h>

static int
string_in_names_list(fz_context *ctx, pdf_obj *p, pdf_obj *names_list)
{
	int n = pdf_array_len(ctx, names_list);
	int i;
	char *str = pdf_to_str_buf(ctx, p);

	for (i = 0; i < n ; i += 2)
	{
		if (!strcmp(pdf_to_str_buf(ctx, pdf_array_get(ctx, names_list, i)), str))
			return 1;
	}
	return 0;
}

/*
 * Recreate page tree to only retain specified pages.
 */

static void retainpage(fz_context *ctx, pdf_document *doc, pdf_obj *parentobj, pdf_obj *kids, int page, pdf_obj *structparents, pdf_obj *ostructparents)
{
	pdf_obj *pageref = pdf_lookup_page_obj(ctx, doc, page);

	pdf_flatten_inheritable_page_items(ctx, pageref);

	pdf_dict_put(ctx, pageref, PDF_NAME(Parent), parentobj);

	/* Store page object in new kids array */
	pdf_array_push(ctx, kids, pageref);

	if (structparents)
	{
		int parentnum = pdf_dict_get_int(ctx, pageref, PDF_NAME(StructParents));
		pdf_obj *parent = pdf_lookup_number(ctx, ostructparents, parentnum);
		pdf_obj *nums = pdf_dict_get(ctx, structparents, PDF_NAME(Nums));
		pdf_obj *limits = pdf_dict_get(ctx, structparents, PDF_NAME(Limits));
		int min, max;
		pdf_array_push_int(ctx, nums, parentnum);
		pdf_array_push(ctx, nums, parent);
		if (limits == NULL)
		{
			min = max = parentnum;
			limits = pdf_new_array(ctx, doc, 2);
			pdf_dict_put_drop(ctx, structparents, PDF_NAME(Limits), limits);
		}
		else
		{
			min = pdf_array_get_int(ctx, limits, 0);
			max = pdf_array_get_int(ctx, limits, 1);
			if (min > parentnum)
				min = parentnum;
			if (max < parentnum)
				max = parentnum;
		}
		pdf_array_put_int(ctx, limits, 0, min);
		pdf_array_put_int(ctx, limits, 1, max);
	}
}

static int dest_is_valid_page(fz_context *ctx, pdf_obj *obj, int *page_object_nums, int pagecount)
{
	int i;
	int num = pdf_to_num(ctx, obj);

	if (num == 0)
		return 0;
	for (i = 0; i < pagecount; i++)
	{
		if (page_object_nums[i] == num)
			return 1;
	}
	return 0;
}

static int dest_is_valid(fz_context *ctx, pdf_obj *o, int page_count, int *page_object_nums, pdf_obj *names_list)
{
	pdf_obj *p;

	p = pdf_dict_get(ctx, o, PDF_NAME(A));
	if (pdf_name_eq(ctx, pdf_dict_get(ctx, p, PDF_NAME(S)), PDF_NAME(GoTo)))
	{
		pdf_obj *d = pdf_dict_get(ctx, p, PDF_NAME(D));
		if (pdf_is_array(ctx, d) && !dest_is_valid_page(ctx, pdf_array_get(ctx, d, 0), page_object_nums, page_count))
			return 0;
		else if (pdf_is_string(ctx, d) && !string_in_names_list(ctx, d, names_list))
			return 0;
	}

	p = pdf_dict_get(ctx, o, PDF_NAME(Dest));
	if (p == NULL)
		return 1; /* A name with no dest counts as valid. */
	else if (pdf_is_string(ctx, p))
		return string_in_names_list(ctx, p, names_list);
	else if (!dest_is_valid_page(ctx, pdf_array_get(ctx, p, 0), page_object_nums, page_count))
		return 0;

	return 1;
}

static int strip_stale_annot_refs(fz_context *ctx, pdf_obj *field, int page_count, int *page_object_nums)
{
	pdf_obj *kids = pdf_dict_get(ctx, field, PDF_NAME(Kids));
	int len = pdf_array_len(ctx, kids);
	int j;

	if (kids)
	{
		for (j = 0; j < len; j++)
		{
			if (strip_stale_annot_refs(ctx, pdf_array_get(ctx, kids, j), page_count, page_object_nums))
			{
				pdf_array_delete(ctx, kids, j);
				len--;
				j--;
			}
		}

		return pdf_array_len(ctx, kids) == 0;
	}
	else
	{
		pdf_obj *page = pdf_dict_get(ctx, field, PDF_NAME(P));
		int page_num = pdf_to_num(ctx, page);

		for (j = 0; j < page_count; j++)
			if (page_num == page_object_nums[j])
				return 0;

		return 1;
	}
}

static int strip_outlines(fz_context *ctx, pdf_document *doc, pdf_obj *outlines, int page_count, int *page_object_nums, pdf_obj *names_list, pdf_mark_bits *marks);

static int strip_outline(fz_context *ctx, pdf_document *doc, pdf_obj *outlines, int page_count, int *page_object_nums, pdf_obj *names_list, pdf_obj **pfirst, pdf_obj **plast, pdf_mark_bits *marks)
{
	pdf_obj *prev = NULL;
	pdf_obj *first = NULL;
	pdf_obj *current;
	int count = 0;

	for (current = outlines; current != NULL; )
	{
		int nc;

		/* Strip any children to start with. This takes care of
		 * First/Last/Count for us. */
		nc = strip_outlines(ctx, doc, current, page_count, page_object_nums, names_list, marks);

		if (!dest_is_valid(ctx, current, page_count, page_object_nums, names_list))
		{
			if (nc == 0)
			{
				/* Outline with invalid dest and no children. Drop it by
				 * pulling the next one in here. */
				pdf_obj *next = pdf_dict_get(ctx, current, PDF_NAME(Next));
				if (!pdf_is_dict(ctx, next))
				{
					/* There is no next one to pull in */
					if (prev != NULL)
						pdf_dict_del(ctx, prev, PDF_NAME(Next));
				}
				else if (prev != NULL)
				{
					pdf_dict_put(ctx, prev, PDF_NAME(Next), next);
					pdf_dict_put(ctx, next, PDF_NAME(Prev), prev);
				}
				else
				{
					pdf_dict_del(ctx, next, PDF_NAME(Prev));
				}
				current = next;
			}
			else
			{
				/* Outline with invalid dest, but children. Just drop the dest. */
				pdf_dict_del(ctx, current, PDF_NAME(Dest));
				pdf_dict_del(ctx, current, PDF_NAME(A));
				current = pdf_dict_get(ctx, current, PDF_NAME(Next));
			}
		}
		else
		{
			/* Keep this one */
			if (first == NULL)
				first = current;
			prev = current;
			current = pdf_dict_get(ctx, current, PDF_NAME(Next));
			count++;
		}
	}

	*pfirst = first;
	*plast = prev;

	return count;
}

static int strip_outlines(fz_context *ctx, pdf_document *doc, pdf_obj *outlines, int page_count, int *page_object_nums, pdf_obj *names_list, pdf_mark_bits *marks)
{
	int nc;
	pdf_obj *first;
	pdf_obj *last;

	if (!pdf_is_dict(ctx, outlines))
		return 0;

	if (pdf_mark_bits_set(ctx, marks, outlines))
		fz_throw(ctx, FZ_ERROR_FORMAT, "Cycle detected in outlines");

	first = pdf_dict_get(ctx, outlines, PDF_NAME(First));
	if (!pdf_is_dict(ctx, first))
		nc = 0;
	else
		nc = strip_outline(ctx, doc, first, page_count, page_object_nums, names_list, &first, &last, marks);

	if (nc == 0)
	{
		pdf_dict_del(ctx, outlines, PDF_NAME(First));
		pdf_dict_del(ctx, outlines, PDF_NAME(Last));
		pdf_dict_del(ctx, outlines, PDF_NAME(Count));
	}
	else
	{
		int old_count = pdf_dict_get_int(ctx, outlines, PDF_NAME(Count));
		pdf_dict_put(ctx, outlines, PDF_NAME(First), first);
		pdf_dict_put(ctx, outlines, PDF_NAME(Last), last);
		pdf_dict_put_int(ctx, outlines, PDF_NAME(Count), old_count > 0 ? nc : -nc);
	}

	return nc;
}

static void pdf_rearrange_pages_imp(fz_context *ctx, pdf_document *doc, int count, const int *new_page_list, pdf_clean_options_structure structure)
{
	pdf_obj *oldroot, *pages, *kids, *olddests;
	pdf_obj *root = NULL;
	pdf_obj *names_list = NULL;
	pdf_obj *outlines;
	pdf_obj *ocproperties;
	pdf_obj *allfields = NULL;
	int pagecount, i;
	int *page_object_nums = NULL;
	pdf_obj *structtreeroot = NULL;
	pdf_obj *ostructparents = NULL;
	pdf_obj *structparents = NULL;
	pdf_mark_bits *marks = NULL;

	/* Keep only pages/type and (reduced) dest entries to avoid
	 * references to unretained pages */
	oldroot = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pages = pdf_dict_get(ctx, oldroot, PDF_NAME(Pages));
	olddests = pdf_load_name_tree(ctx, doc, PDF_NAME(Dests));
	outlines = pdf_dict_get(ctx, oldroot, PDF_NAME(Outlines));
	ocproperties = pdf_dict_get(ctx, oldroot, PDF_NAME(OCProperties));
	if (structure == PDF_CLEAN_STRUCTURE_KEEP)
	{
		structtreeroot = pdf_dict_get(ctx, oldroot, PDF_NAME(StructTreeRoot));
		ostructparents = pdf_dict_get(ctx, structtreeroot, PDF_NAME(ParentTree));
		if (structtreeroot)
			structparents = pdf_new_dict(ctx, doc, 3);
	}

	fz_var(root);
	fz_var(names_list);
	fz_var(allfields);
	fz_var(page_object_nums);
	fz_var(kids);
	fz_var(marks);

	fz_try(ctx)
	{
		root = pdf_new_dict(ctx, doc, 3);
		pdf_dict_put(ctx, root, PDF_NAME(Type), pdf_dict_get(ctx, oldroot, PDF_NAME(Type)));
		pdf_dict_put(ctx, root, PDF_NAME(Pages), pdf_dict_get(ctx, oldroot, PDF_NAME(Pages)));
		if (structtreeroot)
		{
			pdf_dict_put(ctx, root, PDF_NAME(StructTreeRoot), structtreeroot);
			pdf_dict_put(ctx, structtreeroot, PDF_NAME(ParentTree), structparents);
			pdf_dict_put_array(ctx, structparents, PDF_NAME(Nums), 2);
		}
		if (outlines)
			pdf_dict_put(ctx, root, PDF_NAME(Outlines), outlines);
		if (ocproperties)
			pdf_dict_put(ctx, root, PDF_NAME(OCProperties), ocproperties);

		pdf_update_object(ctx, doc, pdf_to_num(ctx, oldroot), root);

		/* Create a new kids array with only the pages we want to keep */
		kids = pdf_new_array(ctx, doc, 1);

		/* Retain pages specified */
		for (i = 0; i < count; ++i)
			retainpage(ctx, doc, pages, kids, new_page_list[i], structparents, ostructparents);

		/* Update page count */
		pdf_dict_put_int(ctx, pages, PDF_NAME(Count), pdf_array_len(ctx, kids));
		pdf_dict_put(ctx, pages, PDF_NAME(Kids), kids);

		pagecount = pdf_count_pages(ctx, doc);
		page_object_nums = fz_calloc(ctx, pagecount, sizeof(*page_object_nums));
		for (i = 0; i < pagecount; i++)
		{
			pdf_obj *pageref = pdf_lookup_page_obj(ctx, doc, i);
			page_object_nums[i] = pdf_to_num(ctx, pageref);
		}

		/* If we had an old Dests tree (now reformed as an olddests
		 * dictionary), keep any entries in there that point to
		 * valid pages. This may mean we keep more than we need, but
		 * it's safe at least. */
		if (olddests)
		{
			pdf_obj *names, *dests;
			int len = pdf_dict_len(ctx, olddests);

			names = pdf_dict_put_dict(ctx, root, PDF_NAME(Names), 1);
			dests = pdf_dict_put_dict(ctx, names, PDF_NAME(Dests), 1);
			names_list = pdf_dict_put_array(ctx, dests, PDF_NAME(Names), 32);

			for (i = 0; i < len; i++)
			{
				pdf_obj *key = pdf_dict_get_key(ctx, olddests, i);
				pdf_obj *val = pdf_dict_get_val(ctx, olddests, i);
				pdf_obj *dest = pdf_dict_get(ctx, val, PDF_NAME(D));

				dest = pdf_array_get(ctx, dest ? dest : val, 0);
				if (dest_is_valid_page(ctx, dest, page_object_nums, pagecount))
				{
					pdf_array_push_string(ctx, names_list, pdf_to_name(ctx, key), strlen(pdf_to_name(ctx, key)));
					pdf_array_push(ctx, names_list, val);
				}
			}

			pdf_drop_obj(ctx, olddests);
		}

		/* Edit each pages /Annot list to remove any links that point to nowhere. */
		for (i = 0; i < pagecount; i++)
		{
			pdf_obj *pageref = pdf_lookup_page_obj(ctx, doc, i);

			pdf_obj *annots = pdf_dict_get(ctx, pageref, PDF_NAME(Annots));

			int len = pdf_array_len(ctx, annots);
			int j;

			for (j = 0; j < len; j++)
			{
				pdf_obj *o = pdf_array_get(ctx, annots, j);

				if (!pdf_name_eq(ctx, pdf_dict_get(ctx, o, PDF_NAME(Subtype)), PDF_NAME(Link)))
					continue;

				if (!dest_is_valid(ctx, o, pagecount, page_object_nums, names_list))
				{
					/* Remove this annotation */
					pdf_array_delete(ctx, annots, j);
					len--;
					j--;
				}
			}
		}

		/* Locate all fields on retained pages */
		allfields = pdf_new_array(ctx, doc, 1);
		for (i = 0; i < pagecount; i++)
		{
			pdf_obj *pageref = pdf_lookup_page_obj(ctx, doc, i);

			pdf_obj *annots = pdf_dict_get(ctx, pageref, PDF_NAME(Annots));

			int len = pdf_array_len(ctx, annots);
			int j;

			for (j = 0; j < len; j++)
			{
				pdf_obj *f = pdf_array_get(ctx, annots, j);

				if (pdf_dict_get(ctx, f, PDF_NAME(Subtype)) == PDF_NAME(Widget))
					pdf_array_push(ctx, allfields, f);
			}
		}

		/* From non-terminal widget fields, strip out annot references not
		 * belonging to any retained page. */
		for (i = 0; i < pdf_array_len(ctx, allfields); i++)
		{
			pdf_obj *f = pdf_array_get(ctx, allfields, i);

			while (pdf_dict_get(ctx, f, PDF_NAME(Parent)))
				f = pdf_dict_get(ctx, f, PDF_NAME(Parent));

			strip_stale_annot_refs(ctx, f, pagecount, page_object_nums);
		}

		/* For terminal fields, if action destination is not valid,
		 * remove the action */
		for (i = 0; i < pdf_array_len(ctx, allfields); i++)
		{
			pdf_obj *f = pdf_array_get(ctx, allfields, i);

			if (!dest_is_valid(ctx, f, pagecount, page_object_nums, names_list))
				pdf_dict_del(ctx, f, PDF_NAME(A));
		}

		marks = pdf_new_mark_bits(ctx, doc);
		if (strip_outlines(ctx, doc, outlines, pagecount, page_object_nums, names_list, marks) == 0)
		{
			pdf_dict_del(ctx, root, PDF_NAME(Outlines));
		}
	}
	fz_always(ctx)
	{
		pdf_drop_mark_bits(ctx, marks);
		fz_free(ctx, page_object_nums);
		pdf_drop_obj(ctx, allfields);
		pdf_drop_obj(ctx, root);
		pdf_drop_obj(ctx, kids);
		pdf_drop_obj(ctx, structparents);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_rearrange_pages(fz_context *ctx, pdf_document *doc, int count, const int *new_page_list, pdf_clean_options_structure structure)
{
	if (structure < PDF_CLEAN_STRUCTURE_DROP || structure > PDF_CLEAN_STRUCTURE_KEEP)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid structure argument");

	pdf_begin_operation(ctx, doc, "Rearrange pages");
	fz_try(ctx)
	{
		pdf_rearrange_pages_imp(ctx, doc, count, new_page_list, structure);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		pdf_sync_open_pages(ctx, doc);
		fz_rethrow(ctx);
	}
	pdf_sync_open_pages(ctx, doc);
}

static void pdf_vectorize_pages_imp(fz_context *ctx, pdf_document *doc, int count, const int *new_page_list, pdf_clean_options_vectorize vectorize)
{
	int i;
	int n = pdf_count_pages(ctx, doc);
	pdf_page *page = NULL;

	fz_var(page);

	fz_try(ctx)
	{
		if (count == 0)
		{
			for (i = 0; i < n; i++)
			{
				page = pdf_load_page(ctx, doc, i);
				pdf_vectorize_page(ctx, page);
				pdf_drop_page(ctx, page);
				page = NULL;
			}
		}
		else
		{
			for (i = 0; i < count; i++)
			{
				page = pdf_load_page(ctx, doc, new_page_list[i]);
				pdf_vectorize_page(ctx, page);
				pdf_drop_page(ctx, page);
				page = NULL;
			}
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_page(ctx, page);
		fz_rethrow(ctx);
	}
}

void pdf_vectorize_pages(fz_context *ctx, pdf_document *doc, int count, const int *new_page_list, pdf_clean_options_vectorize vectorize)
{
	if (vectorize < PDF_CLEAN_VECTORIZE_NO || vectorize > PDF_CLEAN_VECTORIZE_YES)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid vectorize argument");

	if (vectorize == PDF_CLEAN_VECTORIZE_NO)
		return;

	pdf_begin_operation(ctx, doc, "Vectorize pages");
	fz_try(ctx)
	{
		pdf_vectorize_pages_imp(ctx, doc, count, new_page_list, vectorize);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		pdf_sync_open_pages(ctx, doc);
		fz_rethrow(ctx);
	}
	pdf_sync_open_pages(ctx, doc);
}

void pdf_clean_file(fz_context *ctx, char *infile, char *outfile, char *password, pdf_clean_options *opts, int argc, char *argv[])
{
	pdf_clean_options default_opts = { 0 };
	pdf_document *pdf = NULL;
	int *pages = NULL;
	int cap, len, page;

	fz_var(pdf);
	fz_var(pages);

	if (opts == NULL)
		opts = &default_opts;
	if (argc > 0 && argv == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "arguments array must be set if arguments exist");

	fz_try(ctx)
	{
		pdf = pdf_open_document(ctx, infile);
		if (pdf_needs_password(ctx, pdf))
			if (!pdf_authenticate_password(ctx, pdf, password))
				fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot authenticate password: %s", infile);

		/* First, we do a prepass across the document to load all the objects
		 * into memory. We do this to force any repairs to happen before we
		 * start to apply any edits (which would be lost if a repair is triggered). */
		pdf_check_document(ctx, pdf);

		len = cap = 0;

		/* Only retain the specified subset of the pages */
		if (argc)
		{
			int pagecount = pdf_count_pages(ctx, pdf);
			int argidx = 0;

			while (argc - argidx)
			{
				int spage, epage;
				const char *pagelist = argv[argidx];

				while ((pagelist = fz_parse_page_range(ctx, pagelist, &spage, &epage, pagecount)))
				{
					if (len + (epage - spage + 1) >= cap)
					{
						int n = cap ? cap * 2 : 8;
						while (len + (epage - spage + 1) >= n)
							n *= 2;
						pages = fz_realloc_array(ctx, pages, n, int);
						cap = n;
					}

					if (spage < epage)
						for (page = spage; page <= epage; ++page)
							pages[len++] = page - 1;
					else
						for (page = spage; page >= epage; --page)
							pages[len++] = page - 1;
				}

				argidx++;
			}

			pdf_rearrange_pages(ctx, pdf, len, pages, opts->structure);
		}

		/* Although the API supports passing a page list here, we don't
		 * need to (and in fact, must not) pass it, because the subset of
		 * the pages has been done already. */
		pdf_vectorize_pages(ctx, pdf, 0, NULL, opts->vectorize);

		pdf_rewrite_images(ctx, pdf, &opts->image);

		if (opts->subset_fonts)
			pdf_subset_fonts(ctx, pdf, len, pages);

		pdf_save_document(ctx, pdf, outfile, &opts->write);
	}
	fz_always(ctx)
	{
		fz_free(ctx, pages);
		pdf_drop_document(ctx, pdf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
