// Copyright (C) 2004-2023 Artifex Software, Inc.
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
#include "pdf-annot-imp.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void pdf_adjust_page_labels(fz_context *ctx, pdf_document *doc, int index, int adjust);

int
pdf_count_pages(fz_context *ctx, pdf_document *doc)
{
	int pages;
	if (doc->is_fdf)
		return 0;
	/* FIXME: We should reset linear_page_count to 0 when editing starts
	 * (or when linear loading ends) */
	if (doc->linear_page_count != 0)
		pages = doc->linear_page_count;
	else
		pages = pdf_to_int(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Pages/Count"));
	if (pages < 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid number of pages");
	return pages;
}

int pdf_count_pages_imp(fz_context *ctx, fz_document *doc, int chapter)
{
	return pdf_count_pages(ctx, (pdf_document*)doc);
}

static int
pdf_load_page_tree_imp(fz_context *ctx, pdf_document *doc, pdf_obj *node, int idx, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	pdf_obj *type = pdf_dict_get(ctx, node, PDF_NAME(Type));
	if (pdf_name_eq(ctx, type, PDF_NAME(Pages)))
	{
		pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
		int i, n = pdf_array_len(ctx, kids);
		if (pdf_cycle(ctx, &cycle, cycle_up, node))
			fz_throw(ctx, FZ_ERROR_FORMAT, "cycle in page tree");
		for (i = 0; i < n; ++i)
			idx = pdf_load_page_tree_imp(ctx, doc, pdf_array_get(ctx, kids, i), idx, &cycle);
	}
	else if (pdf_name_eq(ctx, type, PDF_NAME(Page)))
	{
		if (idx >= doc->map_page_count)
			fz_throw(ctx, FZ_ERROR_FORMAT, "too many kids in page tree");
		doc->rev_page_map[idx].page = idx;
		doc->rev_page_map[idx].object = pdf_to_num(ctx, node);
		doc->fwd_page_map[idx] = pdf_keep_obj(ctx, node);
		++idx;
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_FORMAT, "non-page object in page tree");
	}
	return idx;
}

static int
cmp_rev_page_map(const void *va, const void *vb)
{
	const pdf_rev_page_map *a = va;
	const pdf_rev_page_map *b = vb;
	return a->object - b->object;
}

void
pdf_load_page_tree(fz_context *ctx, pdf_document *doc)
{
	/* Noop now. */
}

void
pdf_drop_page_tree_internal(fz_context *ctx, pdf_document *doc)
{
	int i;
	fz_free(ctx, doc->rev_page_map);
	doc->rev_page_map = NULL;
	if (doc->fwd_page_map)
		for (i = 0; i < doc->map_page_count; i++)
			pdf_drop_obj(ctx, doc->fwd_page_map[i]);
	fz_free(ctx, doc->fwd_page_map);
	doc->fwd_page_map = NULL;
	doc->map_page_count = 0;
}

static void
pdf_load_page_tree_internal(fz_context *ctx, pdf_document *doc)
{
	/* Check we're not already loaded. */
	if (doc->fwd_page_map != NULL)
		return;

	/* At this point we're trusting that only 1 thread should be doing
	 * stuff that hits the document at a time. */
	fz_try(ctx)
	{
		int idx;

		doc->map_page_count = pdf_count_pages(ctx, doc);
		while (1)
		{
			doc->rev_page_map = Memento_label(fz_calloc(ctx, doc->map_page_count, sizeof(pdf_rev_page_map)), "pdf_rev_page_map");
			doc->fwd_page_map = Memento_label(fz_calloc(ctx, doc->map_page_count, sizeof(pdf_obj *)), "pdf_fwd_page_map");
			idx = pdf_load_page_tree_imp(ctx, doc, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Pages"), 0, NULL);
			if (idx < doc->map_page_count)
			{
				/* The document claims more pages that it has. Fix that. */
				fz_warn(ctx, "Document claims to have %d pages, but only has %d.", doc->map_page_count, idx);
				/* This put drops the page tree! */
				pdf_dict_putp_drop(ctx, pdf_trailer(ctx, doc), "Root/Pages/Count", pdf_new_int(ctx, idx));
				doc->map_page_count = idx;
				continue;
			}
			break;
		}
		qsort(doc->rev_page_map, doc->map_page_count, sizeof *doc->rev_page_map, cmp_rev_page_map);
	}
	fz_catch(ctx)
	{
		pdf_drop_page_tree_internal(ctx, doc);
		fz_rethrow(ctx);
	}
}

void
pdf_drop_page_tree(fz_context *ctx, pdf_document *doc)
{
	/* Historical entry point. Now does nothing. We drop 'just in time'. */
}

static pdf_obj *
pdf_lookup_page_loc_imp(fz_context *ctx, pdf_document *doc, pdf_obj *node, int *skip, pdf_obj **parentp, int *indexp)
{
	pdf_mark_list mark_list;
	pdf_obj *kids;
	pdf_obj *hit = NULL;
	int i, len;

	pdf_mark_list_init(ctx, &mark_list);

	fz_try(ctx)
	{
		do
		{
			kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
			len = pdf_array_len(ctx, kids);

			if (len == 0)
				fz_throw(ctx, FZ_ERROR_FORMAT, "malformed page tree");

			if (pdf_mark_list_push(ctx, &mark_list, node))
				fz_throw(ctx, FZ_ERROR_FORMAT, "cycle in page tree");

			for (i = 0; i < len; i++)
			{
				pdf_obj *kid = pdf_array_get(ctx, kids, i);
				pdf_obj *type = pdf_dict_get(ctx, kid, PDF_NAME(Type));
				if (type ? pdf_name_eq(ctx, type, PDF_NAME(Pages)) : pdf_dict_get(ctx, kid, PDF_NAME(Kids)) && !pdf_dict_get(ctx, kid, PDF_NAME(MediaBox)))
				{
					int count = pdf_dict_get_int(ctx, kid, PDF_NAME(Count));
					if (*skip < count)
					{
						node = kid;
						break;
					}
					else
					{
						*skip -= count;
					}
				}
				else
				{
					if (type ? !pdf_name_eq(ctx, type, PDF_NAME(Page)) : !pdf_dict_get(ctx, kid, PDF_NAME(MediaBox)))
						fz_warn(ctx, "non-page object in page tree (%s)", pdf_to_name(ctx, type));
					if (*skip == 0)
					{
						if (parentp) *parentp = node;
						if (indexp) *indexp = i;
						hit = kid;
						break;
					}
					else
					{
						(*skip)--;
					}
				}
			}
		}
		/* If i < len && hit != NULL the desired page was found in the
		Kids array, done. If i < len && hit == NULL the found page tree
		node contains a Kids array that contains the desired page, loop
		back to top to extract it. When i == len the Kids array has been
		exhausted without finding the desired page, give up.
		*/
		while (hit == NULL && i < len);
	}
	fz_always(ctx)
	{
		pdf_mark_list_free(ctx, &mark_list);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return hit;
}

pdf_obj *
pdf_lookup_page_loc(fz_context *ctx, pdf_document *doc, int needle, pdf_obj **parentp, int *indexp)
{
	pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pdf_obj *node = pdf_dict_get(ctx, root, PDF_NAME(Pages));
	int skip = needle;
	pdf_obj *hit;

	if (!node)
		fz_throw(ctx, FZ_ERROR_FORMAT, "cannot find page tree");

	hit = pdf_lookup_page_loc_imp(ctx, doc, node, &skip, parentp, indexp);
	if (!hit)
		fz_throw(ctx, FZ_ERROR_FORMAT, "cannot find page %d in page tree", needle+1);
	return hit;
}

pdf_obj *
pdf_lookup_page_obj(fz_context *ctx, pdf_document *doc, int needle)
{
	if (doc->fwd_page_map == NULL && !doc->page_tree_broken)
	{
		fz_try(ctx)
			pdf_load_page_tree_internal(ctx, doc);
		fz_catch(ctx)
		{
			doc->page_tree_broken = 1;
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			fz_warn(ctx, "Page tree load failed. Falling back to slow lookup");
		}
	}

	if (doc->fwd_page_map)
	{
		if (needle < 0 || needle >= doc->map_page_count)
			fz_throw(ctx, FZ_ERROR_FORMAT, "cannot find page %d in page tree", needle+1);
		if (doc->fwd_page_map[needle] != NULL)
			return doc->fwd_page_map[needle];
	}

	return pdf_lookup_page_loc(ctx, doc, needle, NULL, NULL);
}

static int
pdf_count_pages_before_kid(fz_context *ctx, pdf_document *doc, pdf_obj *parent, int kid_num)
{
	pdf_obj *kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
	int i, total = 0, len = pdf_array_len(ctx, kids);
	for (i = 0; i < len; i++)
	{
		pdf_obj *kid = pdf_array_get(ctx, kids, i);
		if (pdf_to_num(ctx, kid) == kid_num)
			return total;
		if (pdf_name_eq(ctx, pdf_dict_get(ctx, kid, PDF_NAME(Type)), PDF_NAME(Pages)))
		{
			pdf_obj *count = pdf_dict_get(ctx, kid, PDF_NAME(Count));
			int n = pdf_to_int(ctx, count);
			if (!pdf_is_int(ctx, count) || n < 0)
				fz_throw(ctx, FZ_ERROR_FORMAT, "illegal or missing count in pages tree");
			total += n;
		}
		else
			total++;
	}
	fz_throw(ctx, FZ_ERROR_FORMAT, "kid not found in parent's kids array");
}

static int
pdf_lookup_page_number_slow(fz_context *ctx, pdf_document *doc, pdf_obj *node)
{
	pdf_mark_list mark_list;
	int needle = pdf_to_num(ctx, node);
	int total = 0;
	pdf_obj *parent;

	if (!pdf_name_eq(ctx, pdf_dict_get(ctx, node, PDF_NAME(Type)), PDF_NAME(Page)))
	{
		fz_warn(ctx, "invalid page object");
		return -1;
	}

	pdf_mark_list_init(ctx, &mark_list);
	parent = pdf_dict_get(ctx, node, PDF_NAME(Parent));
	fz_try(ctx)
	{
		while (pdf_is_dict(ctx, parent))
		{
			if (pdf_mark_list_push(ctx, &mark_list, parent))
				fz_throw(ctx, FZ_ERROR_FORMAT, "cycle in page tree (parents)");
			total += pdf_count_pages_before_kid(ctx, doc, parent, needle);
			needle = pdf_to_num(ctx, parent);
			parent = pdf_dict_get(ctx, parent, PDF_NAME(Parent));
		}
	}
	fz_always(ctx)
		pdf_mark_list_free(ctx, &mark_list);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return total;
}

static int
pdf_lookup_page_number_fast(fz_context *ctx, pdf_document *doc, int needle)
{
	int l = 0;
	int r = doc->map_page_count - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = needle - doc->rev_page_map[m].object;
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return doc->rev_page_map[m].page;
	}
	return -1;
}

int
pdf_lookup_page_number(fz_context *ctx, pdf_document *doc, pdf_obj *page)
{
	if (doc->rev_page_map == NULL && !doc->page_tree_broken)
	{
		fz_try(ctx)
			pdf_load_page_tree_internal(ctx, doc);
		fz_catch(ctx)
		{
			doc->page_tree_broken = 1;
			fz_warn(ctx, "Page tree load failed. Falling back to slow lookup.");
		}
	}

	if (doc->rev_page_map)
		return pdf_lookup_page_number_fast(ctx, doc, pdf_to_num(ctx, page));
	else
		return pdf_lookup_page_number_slow(ctx, doc, page);
}

static void
pdf_flatten_inheritable_page_item(fz_context *ctx, pdf_obj *page, pdf_obj *key)
{
	pdf_obj *val = pdf_dict_get_inheritable(ctx, page, key);
	if (val)
		pdf_dict_put(ctx, page, key, val);
}

void
pdf_flatten_inheritable_page_items(fz_context *ctx, pdf_obj *page)
{
	pdf_flatten_inheritable_page_item(ctx, page, PDF_NAME(MediaBox));
	pdf_flatten_inheritable_page_item(ctx, page, PDF_NAME(CropBox));
	pdf_flatten_inheritable_page_item(ctx, page, PDF_NAME(Rotate));
	pdf_flatten_inheritable_page_item(ctx, page, PDF_NAME(Resources));
}

/* We need to know whether to install a page-level transparency group */

/*
 * Object memo flags - allows us to secretly remember "a memo" (a bool) in an
 * object, and to read back whether there was a memo, and if so, what it was.
 */
enum
{
	PDF_FLAGS_MEMO_BM = 0,
	PDF_FLAGS_MEMO_OP = 1
};

static int pdf_resources_use_blending(fz_context *ctx, pdf_obj *rdb, pdf_cycle_list *cycle_up);

static int
pdf_extgstate_uses_blending(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(BM));
	if (obj && !pdf_name_eq(ctx, obj, PDF_NAME(Normal)))
		return 1;
	return 0;
}

static int
pdf_pattern_uses_blending(fz_context *ctx, pdf_obj *dict, pdf_cycle_list *cycle_up)
{
	pdf_obj *obj;
	pdf_cycle_list cycle;
	if (pdf_cycle(ctx, &cycle, cycle_up, dict))
		return 0;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	if (pdf_resources_use_blending(ctx, obj, &cycle))
		return 1;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(ExtGState));
	return pdf_extgstate_uses_blending(ctx, obj);
}

static int
pdf_xobject_uses_blending(fz_context *ctx, pdf_obj *dict, pdf_cycle_list *cycle_up)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	pdf_cycle_list cycle;
	if (pdf_cycle(ctx, &cycle, cycle_up, dict))
		return 0;
	if (pdf_name_eq(ctx, pdf_dict_getp(ctx, dict, "Group/S"), PDF_NAME(Transparency)))
		return 1;
	if (pdf_name_eq(ctx, pdf_dict_get(ctx, dict, PDF_NAME(Subtype)), PDF_NAME(Image)) &&
		pdf_dict_get(ctx, dict, PDF_NAME(SMask)) != NULL)
		return 1;
	return pdf_resources_use_blending(ctx, obj, &cycle);
}

static int
pdf_resources_use_blending(fz_context *ctx, pdf_obj *rdb, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	pdf_obj *obj;
	int i, n, useBM = 0;

	if (!rdb)
		return 0;

	/* Have we been here before and remembered an answer? */
	if (pdf_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_BM, &useBM))
		return useBM;

	/* stop on cyclic resource dependencies */
	if (pdf_cycle(ctx, &cycle, cycle_up, rdb))
		return 0;

	obj = pdf_dict_get(ctx, rdb, PDF_NAME(ExtGState));
	n = pdf_dict_len(ctx, obj);
	for (i = 0; i < n; i++)
		if (pdf_extgstate_uses_blending(ctx, pdf_dict_get_val(ctx, obj, i)))
			goto found;

	obj = pdf_dict_get(ctx, rdb, PDF_NAME(Pattern));
	n = pdf_dict_len(ctx, obj);
	for (i = 0; i < n; i++)
		if (pdf_pattern_uses_blending(ctx, pdf_dict_get_val(ctx, obj, i), &cycle))
			goto found;

	obj = pdf_dict_get(ctx, rdb, PDF_NAME(XObject));
	n = pdf_dict_len(ctx, obj);
	for (i = 0; i < n; i++)
		if (pdf_xobject_uses_blending(ctx, pdf_dict_get_val(ctx, obj, i), &cycle))
			goto found;
	if (0)
	{
found:
		useBM = 1;
	}

	pdf_set_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_BM, useBM);
	return useBM;
}

static int pdf_resources_use_overprint(fz_context *ctx, pdf_obj *rdb, pdf_cycle_list *cycle_up);

static int
pdf_extgstate_uses_overprint(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(OP));
	if (obj && pdf_to_bool(ctx, obj))
		return 1;
	return 0;
}

static int
pdf_pattern_uses_overprint(fz_context *ctx, pdf_obj *dict, pdf_cycle_list *cycle_up)
{
	pdf_obj *obj;
	pdf_cycle_list cycle;
	if (pdf_cycle(ctx, &cycle, cycle_up, dict))
		return 0;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	if (pdf_resources_use_overprint(ctx, obj, &cycle))
		return 1;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(ExtGState));
	return pdf_extgstate_uses_overprint(ctx, obj);
}

static int
pdf_xobject_uses_overprint(fz_context *ctx, pdf_obj *dict, pdf_cycle_list *cycle_up)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	pdf_cycle_list cycle;
	if (pdf_cycle(ctx, &cycle, cycle_up, dict))
		return 0;
	return pdf_resources_use_overprint(ctx, obj, &cycle);
}

static int
pdf_resources_use_overprint(fz_context *ctx, pdf_obj *rdb, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	pdf_obj *obj;
	int i, n, useOP = 0;

	if (!rdb)
		return 0;

	/* Have we been here before and remembered an answer? */
	if (pdf_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_OP, &useOP))
		return useOP;

	/* stop on cyclic resource dependencies */
	if (pdf_cycle(ctx, &cycle, cycle_up, rdb))
		return 0;

	obj = pdf_dict_get(ctx, rdb, PDF_NAME(ExtGState));
	n = pdf_dict_len(ctx, obj);
	for (i = 0; i < n; i++)
		if (pdf_extgstate_uses_overprint(ctx, pdf_dict_get_val(ctx, obj, i)))
			goto found;

	obj = pdf_dict_get(ctx, rdb, PDF_NAME(Pattern));
	n = pdf_dict_len(ctx, obj);
	for (i = 0; i < n; i++)
		if (pdf_pattern_uses_overprint(ctx, pdf_dict_get_val(ctx, obj, i), &cycle))
			goto found;

	obj = pdf_dict_get(ctx, rdb, PDF_NAME(XObject));
	n = pdf_dict_len(ctx, obj);
	for (i = 0; i < n; i++)
		if (pdf_xobject_uses_overprint(ctx, pdf_dict_get_val(ctx, obj, i), &cycle))
			goto found;
	if (0)
	{
found:
		useOP = 1;
	}

	pdf_set_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_OP, useOP);
	return useOP;
}

fz_transition *
pdf_page_presentation(fz_context *ctx, pdf_page *page, fz_transition *transition, float *duration)
{
	pdf_obj *obj, *transdict;

	*duration = pdf_dict_get_real(ctx, page->obj, PDF_NAME(Dur));

	transdict = pdf_dict_get(ctx, page->obj, PDF_NAME(Trans));
	if (!transdict)
		return NULL;

	obj = pdf_dict_get(ctx, transdict, PDF_NAME(D));

	transition->duration = pdf_to_real_default(ctx, obj, 1);

	transition->vertical = !pdf_name_eq(ctx, pdf_dict_get(ctx, transdict, PDF_NAME(Dm)), PDF_NAME(H));
	transition->outwards = !pdf_name_eq(ctx, pdf_dict_get(ctx, transdict, PDF_NAME(M)), PDF_NAME(I));
	/* FIXME: If 'Di' is None, it should be handled differently, but
	 * this only affects Fly, and we don't implement that currently. */
	transition->direction = (pdf_dict_get_int(ctx, transdict, PDF_NAME(Di)));
	/* FIXME: Read SS for Fly when we implement it */
	/* FIXME: Read B for Fly when we implement it */

	obj = pdf_dict_get(ctx, transdict, PDF_NAME(S));
	if (pdf_name_eq(ctx, obj, PDF_NAME(Split)))
		transition->type = FZ_TRANSITION_SPLIT;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Blinds)))
		transition->type = FZ_TRANSITION_BLINDS;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Box)))
		transition->type = FZ_TRANSITION_BOX;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Wipe)))
		transition->type = FZ_TRANSITION_WIPE;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Dissolve)))
		transition->type = FZ_TRANSITION_DISSOLVE;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Glitter)))
		transition->type = FZ_TRANSITION_GLITTER;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Fly)))
		transition->type = FZ_TRANSITION_FLY;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Push)))
		transition->type = FZ_TRANSITION_PUSH;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Cover)))
		transition->type = FZ_TRANSITION_COVER;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Uncover)))
		transition->type = FZ_TRANSITION_UNCOVER;
	else if (pdf_name_eq(ctx, obj, PDF_NAME(Fade)))
		transition->type = FZ_TRANSITION_FADE;
	else
		transition->type = FZ_TRANSITION_NONE;

	return transition;
}

fz_rect
pdf_bound_page(fz_context *ctx, pdf_page *page, fz_box_type box)
{
	fz_matrix page_ctm;
	fz_rect rect;
	pdf_page_transform_box(ctx, page, &rect, &page_ctm, box);
	return fz_transform_rect(rect, page_ctm);
}

fz_link *
pdf_load_links(fz_context *ctx, pdf_page *page)
{
	return fz_keep_link(ctx, page->links);
}

pdf_obj *
pdf_page_resources(fz_context *ctx, pdf_page *page)
{
	return pdf_dict_get_inheritable(ctx, page->obj, PDF_NAME(Resources));
}

pdf_obj *
pdf_page_contents(fz_context *ctx, pdf_page *page)
{
	return pdf_dict_get(ctx, page->obj, PDF_NAME(Contents));
}

pdf_obj *
pdf_page_group(fz_context *ctx, pdf_page *page)
{
	return pdf_dict_get(ctx, page->obj, PDF_NAME(Group));
}

void
pdf_page_obj_transform_box(fz_context *ctx, pdf_obj *pageobj, fz_rect *outbox, fz_matrix *page_ctm, fz_box_type box)
{
	pdf_obj *obj;
	fz_rect usedbox, tempbox, cropbox;
	float userunit = 1;
	int rotate;

	if (!outbox)
		outbox = &tempbox;

	userunit = pdf_dict_get_real_default(ctx, pageobj, PDF_NAME(UserUnit), 1);

	obj = NULL;
	if (box == FZ_ART_BOX)
		obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(ArtBox));
	if (box == FZ_TRIM_BOX)
		obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(TrimBox));
	if (box == FZ_BLEED_BOX)
		obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(BleedBox));
	if (box == FZ_CROP_BOX || !obj)
		obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(CropBox));
	if (box == FZ_MEDIA_BOX || !obj)
		obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(MediaBox));
	usedbox = pdf_to_rect(ctx, obj);

	if (fz_is_empty_rect(usedbox))
		usedbox = fz_make_rect(0, 0, 612, 792);
	usedbox.x0 = fz_min(usedbox.x0, usedbox.x1);
	usedbox.y0 = fz_min(usedbox.y0, usedbox.y1);
	usedbox.x1 = fz_max(usedbox.x0, usedbox.x1);
	usedbox.y1 = fz_max(usedbox.y0, usedbox.y1);
	if (usedbox.x1 - usedbox.x0 < 1 || usedbox.y1 - usedbox.y0 < 1)
		usedbox = fz_unit_rect;

	*outbox = usedbox;

	/* Snap page rotation to 0, 90, 180 or 270 */
	rotate = pdf_dict_get_inheritable_int(ctx, pageobj, PDF_NAME(Rotate));
	if (rotate < 0)
		rotate = 360 - ((-rotate) % 360);
	if (rotate >= 360)
		rotate = rotate % 360;
	rotate = 90*((rotate + 45)/90);
	if (rotate >= 360)
		rotate = 0;

	/* Compute transform from fitz' page space (upper left page origin, y descending, 72 dpi)
	 * to PDF user space (arbitrary page origin, y ascending, UserUnit dpi). */

	/* Make left-handed and scale by UserUnit */
	*page_ctm = fz_scale(userunit, -userunit);

	/* Rotate */
	*page_ctm = fz_pre_rotate(*page_ctm, -rotate);

	/* Always use CropBox to set origin to top left */
	obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(CropBox));
	if (!pdf_is_array(ctx, obj))
		obj = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(MediaBox));
	cropbox = pdf_to_rect(ctx, obj);
	if (fz_is_empty_rect(cropbox))
		cropbox = fz_make_rect(0, 0, 612, 792);
	cropbox.x0 = fz_min(cropbox.x0, cropbox.x1);
	cropbox.y0 = fz_min(cropbox.y0, cropbox.y1);
	cropbox.x1 = fz_max(cropbox.x0, cropbox.x1);
	cropbox.y1 = fz_max(cropbox.y0, cropbox.y1);
	if (cropbox.x1 - cropbox.x0 < 1 || cropbox.y1 - cropbox.y0 < 1)
		cropbox = fz_unit_rect;

	/* Translate page origin of CropBox to 0,0 */
	cropbox = fz_transform_rect(cropbox, *page_ctm);
	*page_ctm = fz_concat(*page_ctm, fz_translate(-cropbox.x0, -cropbox.y0));
}

void
pdf_page_obj_transform(fz_context *ctx, pdf_obj *pageobj, fz_rect *page_cropbox, fz_matrix *page_ctm)
{
	pdf_page_obj_transform_box(ctx, pageobj, page_cropbox, page_ctm, FZ_CROP_BOX);
}

void
pdf_page_transform_box(fz_context *ctx, pdf_page *page, fz_rect *page_cropbox, fz_matrix *page_ctm, fz_box_type box)
{
	pdf_page_obj_transform_box(ctx, page->obj, page_cropbox, page_ctm, box);
}

void
pdf_page_transform(fz_context *ctx, pdf_page *page, fz_rect *cropbox, fz_matrix *ctm)
{
	pdf_page_transform_box(ctx, page, cropbox, ctm, FZ_CROP_BOX);
}

static void
find_seps(fz_context *ctx, fz_separations **seps, pdf_obj *obj, pdf_mark_list *clearme)
{
	int i, n;
	pdf_obj *nameobj, *cols;

	if (!obj)
		return;

	// Already seen this ColorSpace...
	if (pdf_mark_list_push(ctx, clearme, obj))
		return;

	nameobj = pdf_array_get(ctx, obj, 0);
	if (pdf_name_eq(ctx, nameobj, PDF_NAME(Separation)))
	{
		fz_colorspace *cs;
		const char *name = pdf_array_get_name(ctx, obj, 1);

		/* Skip 'special' colorants. */
		if (!strcmp(name, "Black") ||
			!strcmp(name, "Cyan") ||
			!strcmp(name, "Magenta") ||
			!strcmp(name, "Yellow") ||
			!strcmp(name, "All") ||
			!strcmp(name, "None"))
			return;

		n = fz_count_separations(ctx, *seps);
		for (i = 0; i < n; i++)
		{
			if (!strcmp(name, fz_separation_name(ctx, *seps, i)))
				return; /* Got that one already */
		}

		fz_try(ctx)
			cs = pdf_load_colorspace(ctx, obj);
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			return; /* ignore broken colorspace */
		}
		fz_try(ctx)
		{
			if (!*seps)
				*seps = fz_new_separations(ctx, 0);
			fz_add_separation(ctx, *seps, name, cs, 0);
		}
		fz_always(ctx)
			fz_drop_colorspace(ctx, cs);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else if (pdf_name_eq(ctx, nameobj, PDF_NAME(Indexed)))
	{
		find_seps(ctx, seps, pdf_array_get(ctx, obj, 1), clearme);
	}
	else if (pdf_name_eq(ctx, nameobj, PDF_NAME(DeviceN)))
	{
		/* If the separation colorants exists for this DeviceN color space
		 * add those prior to our search for DeviceN color */
		cols = pdf_dict_get(ctx, pdf_array_get(ctx, obj, 4), PDF_NAME(Colorants));
		n = pdf_dict_len(ctx, cols);
		for (i = 0; i < n; i++)
			find_seps(ctx, seps, pdf_dict_get_val(ctx, cols, i), clearme);
	}
}

static void
find_devn(fz_context *ctx, fz_separations **seps, pdf_obj *obj, pdf_mark_list *clearme)
{
	int i, j, n, m;
	pdf_obj *arr;
	pdf_obj *nameobj = pdf_array_get(ctx, obj, 0);

	if (!obj)
		return;

	// Already seen this ColorSpace...
	if (pdf_mark_list_push(ctx, clearme, obj))
		return;

	if (!pdf_name_eq(ctx, nameobj, PDF_NAME(DeviceN)))
		return;

	arr = pdf_array_get(ctx, obj, 1);
	m = pdf_array_len(ctx, arr);
	for (j = 0; j < m; j++)
	{
		fz_colorspace *cs;
		const char *name = pdf_array_get_name(ctx, arr, j);

		/* Skip 'special' colorants. */
		if (!strcmp(name, "Black") ||
			!strcmp(name, "Cyan") ||
			!strcmp(name, "Magenta") ||
			!strcmp(name, "Yellow") ||
			!strcmp(name, "All") ||
			!strcmp(name, "None"))
			continue;

		n = fz_count_separations(ctx, *seps);
		for (i = 0; i < n; i++)
		{
			if (!strcmp(name, fz_separation_name(ctx, *seps, i)))
				break; /* Got that one already */
		}

		if (i == n)
		{
			fz_try(ctx)
				cs = pdf_load_colorspace(ctx, obj);
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
				fz_report_error(ctx);
				continue; /* ignore broken colorspace */
			}
			fz_try(ctx)
			{
				if (!*seps)
					*seps = fz_new_separations(ctx, 0);
				fz_add_separation(ctx, *seps, name, cs, j);
			}
			fz_always(ctx)
				fz_drop_colorspace(ctx, cs);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
	}
}

typedef void (res_finder_fn)(fz_context *ctx, fz_separations **seps, pdf_obj *obj, pdf_mark_list *clearme);

static void
scan_page_seps(fz_context *ctx, pdf_obj *res, fz_separations **seps, res_finder_fn *fn, pdf_mark_list *clearme)
{
	pdf_obj *dict;
	pdf_obj *obj;
	int i, n;

	if (!res)
		return;

	// Already seen this Resources...
	if (pdf_mark_list_push(ctx, clearme, res))
		return;

	dict = pdf_dict_get(ctx, res, PDF_NAME(ColorSpace));
	n = pdf_dict_len(ctx, dict);
	for (i = 0; i < n; i++)
	{
		obj = pdf_dict_get_val(ctx, dict, i);
		fn(ctx, seps, obj, clearme);
	}

	dict = pdf_dict_get(ctx, res, PDF_NAME(Shading));
	n = pdf_dict_len(ctx, dict);
	for (i = 0; i < n; i++)
	{
		obj = pdf_dict_get_val(ctx, dict, i);
		fn(ctx, seps, pdf_dict_get(ctx, obj, PDF_NAME(ColorSpace)), clearme);
	}

	dict = pdf_dict_get(ctx, res, PDF_NAME(XObject));
	n = pdf_dict_len(ctx, dict);
	for (i = 0; i < n; i++)
	{
		obj = pdf_dict_get_val(ctx, dict, i);
		// Already seen this XObject...
		if (!pdf_mark_list_push(ctx, clearme, obj))
		{
			fn(ctx, seps, pdf_dict_get(ctx, obj, PDF_NAME(ColorSpace)), clearme);
			/* Recurse on XObject forms. */
			scan_page_seps(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Resources)), seps, fn, clearme);
		}
	}
}

fz_separations *
pdf_page_separations(fz_context *ctx, pdf_page *page)
{
	pdf_obj *res = pdf_page_resources(ctx, page);
	pdf_mark_list clearme;
	fz_separations *seps = NULL;

	pdf_mark_list_init(ctx, &clearme);
	fz_try(ctx)
	{
		/* Run through and look for separations first. This is
		 * because separations are simplest to deal with, and
		 * because DeviceN may be implemented on top of separations.
		 */
		scan_page_seps(ctx, res, &seps, find_seps, &clearme);
	}
	fz_always(ctx)
		pdf_mark_list_free(ctx, &clearme);
	fz_catch(ctx)
	{
		fz_drop_separations(ctx, seps);
		fz_rethrow(ctx);
	}

	pdf_mark_list_init(ctx, &clearme);
	fz_try(ctx)
	{
		/* Now run through again, and look for DeviceNs. These may
		 * have spot colors in that aren't defined in terms of
		 * separations. */
		scan_page_seps(ctx, res, &seps, find_devn, &clearme);
	}
	fz_always(ctx)
		pdf_mark_list_free(ctx, &clearme);
	fz_catch(ctx)
	{
		fz_drop_separations(ctx, seps);
		fz_rethrow(ctx);
	}

	return seps;
}

int
pdf_page_uses_overprint(fz_context *ctx, pdf_page *page)
{
	return page ? page->overprint : 0;
}

static void
pdf_drop_page_imp(fz_context *ctx, pdf_page *page)
{
	fz_drop_link(ctx, page->links);
	pdf_drop_annots(ctx, page->annots);
	pdf_drop_widgets(ctx, page->widgets);
	pdf_drop_obj(ctx, page->obj);
}

static pdf_page *
pdf_new_page(fz_context *ctx, pdf_document *doc)
{
	pdf_page *page = fz_new_derived_page(ctx, pdf_page, (fz_document*) doc);

	page->doc = doc; /* typecast alias for page->super.doc */

	page->super.drop_page = (fz_page_drop_page_fn*)pdf_drop_page_imp;
	page->super.load_links = (fz_page_load_links_fn*)pdf_load_links;
	page->super.bound_page = (fz_page_bound_page_fn*)pdf_bound_page;
	page->super.run_page_contents = (fz_page_run_page_fn*)pdf_run_page_contents;
	page->super.run_page_annots = (fz_page_run_page_fn*)pdf_run_page_annots;
	page->super.run_page_widgets = (fz_page_run_page_fn*)pdf_run_page_widgets;
	page->super.page_presentation = (fz_page_page_presentation_fn*)pdf_page_presentation;
	page->super.separations = (fz_page_separations_fn *)pdf_page_separations;
	page->super.overprint = (fz_page_uses_overprint_fn *)pdf_page_uses_overprint;
	page->super.create_link = (fz_page_create_link_fn *)pdf_create_link;
	page->super.delete_link = (fz_page_delete_link_fn *)pdf_delete_link;

	page->obj = NULL;

	page->transparency = 0;
	page->links = NULL;
	page->annots = NULL;
	page->annot_tailp = &page->annots;
	page->widgets = NULL;
	page->widget_tailp = &page->widgets;

	return page;
}

static void
pdf_load_default_colorspaces_imp(fz_context *ctx, fz_default_colorspaces *default_cs, pdf_obj *obj)
{
	pdf_obj *cs_obj;

	/* The spec says to ignore any colors we can't understand */

	cs_obj = pdf_dict_get(ctx, obj, PDF_NAME(DefaultGray));
	if (cs_obj)
	{
		fz_try(ctx)
		{
			fz_colorspace *cs = pdf_load_colorspace(ctx, cs_obj);
			fz_set_default_gray(ctx, default_cs, cs);
			fz_drop_colorspace(ctx, cs);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
		}
	}

	cs_obj = pdf_dict_get(ctx, obj, PDF_NAME(DefaultRGB));
	if (cs_obj)
	{
		fz_try(ctx)
		{
			fz_colorspace *cs = pdf_load_colorspace(ctx, cs_obj);
			fz_set_default_rgb(ctx, default_cs, cs);
			fz_drop_colorspace(ctx, cs);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
		}
	}

	cs_obj = pdf_dict_get(ctx, obj, PDF_NAME(DefaultCMYK));
	if (cs_obj)
	{
		fz_try(ctx)
		{
			fz_colorspace *cs = pdf_load_colorspace(ctx, cs_obj);
			fz_set_default_cmyk(ctx, default_cs, cs);
			fz_drop_colorspace(ctx, cs);
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
		}
	}
}

fz_default_colorspaces *
pdf_load_default_colorspaces(fz_context *ctx, pdf_document *doc, pdf_page *page)
{
	pdf_obj *res;
	pdf_obj *obj;
	fz_default_colorspaces *default_cs;
	fz_colorspace *oi;

	default_cs = fz_new_default_colorspaces(ctx);

	fz_try(ctx)
	{
		res = pdf_page_resources(ctx, page);
		obj = pdf_dict_get(ctx, res, PDF_NAME(ColorSpace));
		if (obj)
			pdf_load_default_colorspaces_imp(ctx, default_cs, obj);

		oi = pdf_document_output_intent(ctx, doc);
		if (oi)
			fz_set_default_output_intent(ctx, default_cs, oi);
	}
	fz_catch(ctx)
	{
		if (fz_caught(ctx) != FZ_ERROR_TRYLATER)
		{
			fz_drop_default_colorspaces(ctx, default_cs);
			fz_rethrow(ctx);
		}
		fz_ignore_error(ctx);
		page->super.incomplete = 1;
	}

	return default_cs;
}

fz_default_colorspaces *
pdf_update_default_colorspaces(fz_context *ctx, fz_default_colorspaces *old_cs, pdf_obj *res)
{
	pdf_obj *obj;
	fz_default_colorspaces *new_cs;

	obj = pdf_dict_get(ctx, res, PDF_NAME(ColorSpace));
	if (!obj)
		return fz_keep_default_colorspaces(ctx, old_cs);

	new_cs = fz_clone_default_colorspaces(ctx, old_cs);
	fz_try(ctx)
		pdf_load_default_colorspaces_imp(ctx, new_cs, obj);
	fz_catch(ctx)
	{
		fz_drop_default_colorspaces(ctx, new_cs);
		fz_rethrow(ctx);
	}

	return new_cs;
}

pdf_page *
pdf_load_page(fz_context *ctx, pdf_document *doc, int number)
{
	return (pdf_page*)fz_load_page(ctx, (fz_document*)doc, number);
}

int
pdf_page_has_transparency(fz_context *ctx, pdf_page *page)
{
	return page->transparency;
}

fz_page *
pdf_load_page_imp(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	pdf_document *doc = (pdf_document*)doc_;
	pdf_page *page;
	pdf_annot *annot;
	pdf_obj *pageobj, *obj;

	if (doc->is_fdf)
		fz_throw(ctx, FZ_ERROR_FORMAT, "FDF documents have no pages");

	if (chapter != 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid chapter number: %d", chapter);

	if (number < 0 || number >= pdf_count_pages(ctx, doc))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid page number: %d", number);

	if (doc->file_reading_linearly)
	{
		pageobj = pdf_progressive_advance(ctx, doc, number);
		if (pageobj == NULL)
			fz_throw(ctx, FZ_ERROR_TRYLATER, "page %d not available yet", number);
	}
	else
		pageobj = pdf_lookup_page_obj(ctx, doc, number);

	page = pdf_new_page(ctx, doc);
	page->obj = pdf_keep_obj(ctx, pageobj);

	/* Pre-load annotations and links */
	fz_try(ctx)
	{
		obj = pdf_dict_get(ctx, pageobj, PDF_NAME(Annots));
		if (obj)
		{
			fz_rect page_cropbox;
			fz_matrix page_ctm;
			pdf_page_transform(ctx, page, &page_cropbox, &page_ctm);
			page->links = pdf_load_link_annots(ctx, doc, page, obj, number, page_ctm);
			pdf_load_annots(ctx, page, obj);
		}
	}
	fz_catch(ctx)
	{
		if (fz_caught(ctx) != FZ_ERROR_TRYLATER)
		{
			fz_drop_page(ctx, &page->super);
			fz_rethrow(ctx);
		}
		fz_ignore_error(ctx);
		page->super.incomplete = 1;
		fz_drop_link(ctx, page->links);
		page->links = NULL;
	}

	/* Scan for transparency and overprint */
	fz_try(ctx)
	{
		pdf_obj *resources = pdf_page_resources(ctx, page);
		if (pdf_name_eq(ctx, pdf_dict_getp(ctx, pageobj, "Group/S"), PDF_NAME(Transparency)))
			page->transparency = 1;
		else if (pdf_resources_use_blending(ctx, resources, NULL))
			page->transparency = 1;
		if (pdf_resources_use_overprint(ctx, resources, NULL))
			page->overprint = 1;
		for (annot = page->annots; annot && !page->transparency; annot = annot->next)
		{
			fz_try(ctx)
			{
				pdf_obj *ap;
				pdf_obj *res;
				pdf_annot_push_local_xref(ctx, annot);
				ap = pdf_annot_ap(ctx, annot);
				if (!ap)
					break;
				res = pdf_xobject_resources(ctx, ap);
				if (pdf_resources_use_blending(ctx, res, NULL))
					page->transparency = 1;
				if (pdf_resources_use_overprint(ctx, pdf_xobject_resources(ctx, res), NULL))
					page->overprint = 1;
			}
			fz_always(ctx)
				pdf_annot_pop_local_xref(ctx, annot);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		for (annot = page->widgets; annot && !page->transparency; annot = annot->next)
		{
			fz_try(ctx)
			{
				pdf_obj *ap;
				pdf_obj *res;
				pdf_annot_push_local_xref(ctx, annot);
				ap = pdf_annot_ap(ctx, annot);
				if (!ap)
					break;
				res = pdf_xobject_resources(ctx, ap);
				if (pdf_resources_use_blending(ctx, res, NULL))
					page->transparency = 1;
				if (pdf_resources_use_overprint(ctx, pdf_xobject_resources(ctx, res), NULL))
					page->overprint = 1;
			}
			fz_always(ctx)
				pdf_annot_pop_local_xref(ctx, annot);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
	}
	fz_catch(ctx)
	{
		if (fz_caught(ctx) != FZ_ERROR_TRYLATER)
		{
			fz_drop_page(ctx, &page->super);
			fz_rethrow(ctx);
		}
		fz_ignore_error(ctx);
		page->super.incomplete = 1;
	}

	return (fz_page*)page;
}

void
pdf_delete_page(fz_context *ctx, pdf_document *doc, int at)
{
	pdf_obj *parent, *kids;
	int i;

	pdf_begin_operation(ctx, doc, "Delete page");
	fz_try(ctx)
	{
		pdf_lookup_page_loc(ctx, doc, at, &parent, &i);
		kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
		pdf_array_delete(ctx, kids, i);

		while (parent)
		{
			int count = pdf_dict_get_int(ctx, parent, PDF_NAME(Count));
			pdf_dict_put_int(ctx, parent, PDF_NAME(Count), count - 1);
			parent = pdf_dict_get(ctx, parent, PDF_NAME(Parent));
		}

		/* Adjust page labels */
		pdf_adjust_page_labels(ctx, doc, at, -1);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}

	/* Adjust the fz layer of cached pages */
	fz_lock(ctx, FZ_LOCK_ALLOC);
	{
		fz_page *page, *next;

		for (page = doc->super.open; page != NULL; page = next)
		{
			next = page->next;
			if (page->number == at)
			{
				/* We have just 'removed' a page that is in the 'open' list
				 * (i.e. that someone is holding a reference to). We need
				 * to remove it so that no one else can load it now its gone.
				 */
				if (next)
					next->prev = page->prev;
				if (page->prev)
					*page->prev = page->next;
			}
			else if (page->number >= at)
				page->number--;
		}
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

void
pdf_delete_page_range(fz_context *ctx, pdf_document *doc, int start, int end)
{
	int count = pdf_count_pages(ctx, doc);

	if (end < 0 || end > count)
		end = count+1;
	if (start < 0)
		start = 0;
	while (start < end)
	{
		pdf_delete_page(ctx, doc, start);
		end--;
	}
}

pdf_obj *
pdf_add_page(fz_context *ctx, pdf_document *doc, fz_rect mediabox, int rotate, pdf_obj *resources, fz_buffer *contents)
{
	pdf_obj *page_obj = NULL;
	pdf_obj *page_ref = NULL;

	fz_var(page_obj);
	fz_var(page_ref);

	pdf_begin_operation(ctx, doc, "Add page");

	fz_try(ctx)
	{
		page_obj = pdf_new_dict(ctx, doc, 5);

		pdf_dict_put(ctx, page_obj, PDF_NAME(Type), PDF_NAME(Page));
		pdf_dict_put_rect(ctx, page_obj, PDF_NAME(MediaBox), mediabox);
		pdf_dict_put_int(ctx, page_obj, PDF_NAME(Rotate), rotate);

		if (pdf_is_indirect(ctx, resources))
			pdf_dict_put(ctx, page_obj, PDF_NAME(Resources), resources);
		else if (pdf_is_dict(ctx, resources))
			pdf_dict_put_drop(ctx, page_obj, PDF_NAME(Resources), pdf_add_object(ctx, doc, resources));
		else
			pdf_dict_put_dict(ctx, page_obj, PDF_NAME(Resources), 1);

		if (contents && contents->len > 0)
			pdf_dict_put_drop(ctx, page_obj, PDF_NAME(Contents), pdf_add_stream(ctx, doc, contents, NULL, 0));
		page_ref = pdf_add_object_drop(ctx, doc, page_obj);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, page_obj);
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
	return page_ref;
}

void
pdf_insert_page(fz_context *ctx, pdf_document *doc, int at, pdf_obj *page_ref)
{
	int count = pdf_count_pages(ctx, doc);
	pdf_obj *parent, *kids;
	int i;

	if (at < 0)
		at = count;
	if (at == INT_MAX)
		at = count;
	if (at > count)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot insert page beyond end of page tree");

	pdf_begin_operation(ctx, doc, "Insert page");

	fz_try(ctx)
	{
		if (count == 0)
		{
			pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			parent = pdf_dict_get(ctx, root, PDF_NAME(Pages));
			if (!parent)
				fz_throw(ctx, FZ_ERROR_FORMAT, "cannot find page tree");
			kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
			if (!kids)
				fz_throw(ctx, FZ_ERROR_FORMAT, "malformed page tree");
			pdf_array_insert(ctx, kids, page_ref, 0);
		}
		else if (at == count)
		{
			/* append after last page */
			pdf_lookup_page_loc(ctx, doc, count - 1, &parent, &i);
			kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
			pdf_array_insert(ctx, kids, page_ref, i + 1);
		}
		else
		{
			/* insert before found page */
			pdf_lookup_page_loc(ctx, doc, at, &parent, &i);
			kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
			pdf_array_insert(ctx, kids, page_ref, i);
		}

		pdf_dict_put(ctx, page_ref, PDF_NAME(Parent), parent);

		/* Adjust page counts */
		while (parent)
		{
			count = pdf_dict_get_int(ctx, parent, PDF_NAME(Count));
			pdf_dict_put_int(ctx, parent, PDF_NAME(Count), count + 1);
			parent = pdf_dict_get(ctx, parent, PDF_NAME(Parent));
		}

		/* Adjust page labels */
		pdf_adjust_page_labels(ctx, doc, at, 1);
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}

	/* Adjust the fz layer of cached pages */
	fz_lock(ctx, FZ_LOCK_ALLOC);
	{
		fz_page *page;

		for (page = doc->super.open; page != NULL; page = page->next)
		{
			if (page->number >= at)
				page->number++;
		}
	}
	fz_unlock(ctx, FZ_LOCK_ALLOC);
}

/*
 * Page Labels
 */

struct page_label_range {
	int offset;
	pdf_obj *label;
	int nums_ix;
	pdf_obj *nums;
};

static void
pdf_lookup_page_label_imp(fz_context *ctx, pdf_obj *node, int index, struct page_label_range *range)
{
	pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
	pdf_obj *nums = pdf_dict_get(ctx, node, PDF_NAME(Nums));
	int i;

	if (pdf_is_array(ctx, kids))
	{
		for (i = 0; i < pdf_array_len(ctx, kids); ++i)
		{
			pdf_obj *kid = pdf_array_get(ctx, kids, i);
			pdf_lookup_page_label_imp(ctx, kid, index, range);
		}
	}

	if (pdf_is_array(ctx, nums))
	{
		for (i = 0; i < pdf_array_len(ctx, nums); i += 2)
		{
			int k = pdf_array_get_int(ctx, nums, i);
			if (k <= index)
			{
				range->offset = k;
				range->label = pdf_array_get(ctx, nums, i + 1);
				range->nums_ix = i;
				range->nums = nums;
			}
			else
			{
				/* stop looking if we've already passed the index */
				return;
			}
		}
	}
}

static struct page_label_range
pdf_lookup_page_label(fz_context *ctx, pdf_document *doc, int index)
{
	struct page_label_range range = { 0, NULL };
	pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pdf_obj *labels = pdf_dict_get(ctx, root, PDF_NAME(PageLabels));
	pdf_lookup_page_label_imp(ctx, labels, index, &range);
	return range;
}

static void
pdf_flatten_page_label_tree_imp(fz_context *ctx, pdf_obj *node, pdf_obj *new_nums)
{
	pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
	pdf_obj *nums = pdf_dict_get(ctx, node, PDF_NAME(Nums));
	int i;

	if (pdf_is_array(ctx, kids))
	{
		for (i = 0; i < pdf_array_len(ctx, kids); ++i)
		{
			pdf_obj *kid = pdf_array_get(ctx, kids, i);
			pdf_flatten_page_label_tree_imp(ctx, kid, new_nums);
		}
	}

	if (pdf_is_array(ctx, nums))
	{
		for (i = 0; i < pdf_array_len(ctx, nums); i += 2)
		{
			pdf_array_push(ctx, new_nums, pdf_array_get(ctx, nums, i));
			pdf_array_push(ctx, new_nums, pdf_array_get(ctx, nums, i + 1));
		}
	}
}

static void
pdf_flatten_page_label_tree(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pdf_obj *labels = pdf_dict_get(ctx, root, PDF_NAME(PageLabels));
	pdf_obj *nums = pdf_dict_get(ctx, labels, PDF_NAME(Nums));

	// Already flat...
	if (pdf_is_array(ctx, nums) && pdf_array_len(ctx, nums) >= 2)
		return;

	nums = pdf_new_array(ctx, doc, 8);
	fz_try(ctx)
	{
		if (!labels)
			labels = pdf_dict_put_dict(ctx, root, PDF_NAME(PageLabels), 1);

		pdf_flatten_page_label_tree_imp(ctx, labels, nums);

		pdf_dict_del(ctx, labels, PDF_NAME(Kids));
		pdf_dict_del(ctx, labels, PDF_NAME(Limits));
		pdf_dict_put(ctx, labels, PDF_NAME(Nums), nums);

		/* No Page Label tree found - insert one with default values */
		if (pdf_array_len(ctx, nums) == 0)
		{
			pdf_obj *obj;
			pdf_array_push_int(ctx, nums, 0);
			obj = pdf_array_push_dict(ctx, nums, 1);
			pdf_dict_put(ctx, obj, PDF_NAME(S), PDF_NAME(D));
		}
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, nums);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static pdf_obj *
pdf_create_page_label(fz_context *ctx, pdf_document *doc, pdf_page_label_style style, const char *prefix, int start)
{
	pdf_obj *obj = pdf_new_dict(ctx, doc, 3);
	fz_try(ctx)
	{
		switch (style)
		{
		default:
		case PDF_PAGE_LABEL_NONE:
			break;
		case PDF_PAGE_LABEL_DECIMAL:
			pdf_dict_put(ctx, obj, PDF_NAME(S), PDF_NAME(D));
			break;
		case PDF_PAGE_LABEL_ROMAN_UC:
			pdf_dict_put(ctx, obj, PDF_NAME(S), PDF_NAME(R));
			break;
		case PDF_PAGE_LABEL_ROMAN_LC:
			pdf_dict_put(ctx, obj, PDF_NAME(S), PDF_NAME(r));
			break;
		case PDF_PAGE_LABEL_ALPHA_UC:
			pdf_dict_put(ctx, obj, PDF_NAME(S), PDF_NAME(A));
			break;
		case PDF_PAGE_LABEL_ALPHA_LC:
			pdf_dict_put(ctx, obj, PDF_NAME(S), PDF_NAME(a));
			break;
		}
		if (prefix && strlen(prefix) > 0)
			pdf_dict_put_text_string(ctx, obj, PDF_NAME(P), prefix);
		if (start > 1)
			pdf_dict_put_int(ctx, obj, PDF_NAME(St), start);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, obj);
		fz_rethrow(ctx);
	}
	return obj;
}

static void
pdf_adjust_page_labels(fz_context *ctx, pdf_document *doc, int index, int adjust)
{
	pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pdf_obj *labels = pdf_dict_get(ctx, root, PDF_NAME(PageLabels));

	// Skip the adjustment step if there are no page labels.
	// Exception: If we would adjust the label for page 0, we must create one!
	// Exception: If the document only has one page!
	if (labels || (adjust > 0 && index == 0 && pdf_count_pages(ctx, doc) > 1))
	{
		struct page_label_range range;
		int i;

		// Ensure we have a flat page label tree with at least one entry.
		pdf_flatten_page_label_tree(ctx, doc);

		// Find page label affecting the page that triggered adjustment
		range = pdf_lookup_page_label(ctx, doc, index);

		// Shift all page labels on and after the inserted index
		if (adjust > 0)
		{
			if (range.offset == index)
				i = range.nums_ix;
			else
				i = range.nums_ix + 2;
		}

		// Shift all page labels after the removed index
		else
		{
			i = range.nums_ix + 2;
		}


		// Increase/decrease the indices in the name tree
		for (; i < pdf_array_len(ctx, range.nums); i += 2)
			pdf_array_put_int(ctx, range.nums, i, pdf_array_get_int(ctx, range.nums, i) + adjust);

		// TODO: delete page labels that have no effect (zero range)

		// Make sure the number tree always has an entry for page 0
		if (adjust > 0 && index == 0)
		{
			pdf_array_insert_drop(ctx, range.nums, pdf_new_int(ctx, index), 0);
			pdf_array_insert_drop(ctx, range.nums, pdf_create_page_label(ctx, doc, PDF_PAGE_LABEL_DECIMAL, NULL, 1), 1);
		}
	}
}

void
pdf_set_page_labels(fz_context *ctx, pdf_document *doc,
	int index,
	pdf_page_label_style style, const char *prefix, int start)
{
	struct page_label_range range;

	pdf_begin_operation(ctx, doc, "Set page label");
	fz_try(ctx)
	{
		// Ensure we have a flat page label tree with at least one entry.
		pdf_flatten_page_label_tree(ctx, doc);

		range = pdf_lookup_page_label(ctx, doc, index);

		if (range.offset == index)
		{
			// Replace label
			pdf_array_put_drop(ctx, range.nums,
				range.nums_ix + 1,
				pdf_create_page_label(ctx, doc, style, prefix, start));
		}
		else
		{
			// Insert new label
			pdf_array_insert_drop(ctx, range.nums,
				pdf_new_int(ctx, index),
				range.nums_ix + 2);
			pdf_array_insert_drop(ctx, range.nums,
				pdf_create_page_label(ctx, doc, style, prefix, start),
				range.nums_ix + 3);
		}
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
}

void
pdf_delete_page_labels(fz_context *ctx, pdf_document *doc, int index)
{
	struct page_label_range range;

	if (index == 0)
	{
		pdf_set_page_labels(ctx, doc, 0, PDF_PAGE_LABEL_DECIMAL, NULL, 1);
		return;
	}

	pdf_begin_operation(ctx, doc, "Delete page label");
	fz_try(ctx)
	{
		// Ensure we have a flat page label tree with at least one entry.
		pdf_flatten_page_label_tree(ctx, doc);

		range = pdf_lookup_page_label(ctx, doc, index);

		if (range.offset == index)
		{
			// Delete label
			pdf_array_delete(ctx, range.nums, range.nums_ix);
			pdf_array_delete(ctx, range.nums, range.nums_ix);
		}
		pdf_end_operation(ctx, doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
}

static const char *roman_uc[3][10] = {
	{ "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX" },
	{ "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC" },
	{ "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM" },
};

static const char *roman_lc[3][10] = {
	{ "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix" },
	{ "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc" },
	{ "", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm" },
};

static void pdf_format_roman_page_label(char *buf, int size, int n, const char *sym[3][10], const char *sym_m)
{
	int I = n % 10;
	int X = (n / 10) % 10;
	int C = (n / 100) % 10;
	int M = (n / 1000);

	fz_strlcpy(buf, "", size);
	while (M--)
		fz_strlcat(buf, sym_m, size);
	fz_strlcat(buf, sym[2][C], size);
	fz_strlcat(buf, sym[1][X], size);
	fz_strlcat(buf, sym[0][I], size);
}

static void pdf_format_alpha_page_label(char *buf, int size, int n, int alpha)
{
	int reps = (n - 1) / 26 + 1;
	if (reps > size - 1)
		reps = size - 1;
	memset(buf, (n - 1) % 26 + alpha, reps);
	buf[reps] = '\0';
}

static void
pdf_format_page_label(fz_context *ctx, int index, pdf_obj *dict, char *buf, size_t size)
{
	pdf_obj *style = pdf_dict_get(ctx, dict, PDF_NAME(S));
	const char *prefix = pdf_dict_get_text_string(ctx, dict, PDF_NAME(P));
	int start = pdf_dict_get_int(ctx, dict, PDF_NAME(St));
	size_t n;

	// St must be >= 1; default is 1.
	if (start < 1)
		start = 1;

	// Add prefix (optional; may be empty)
	fz_strlcpy(buf, prefix, size);
	n = strlen(buf);
	buf += n;
	size -= n;

	// Append number using style (optional)
	if (style == PDF_NAME(D))
		fz_snprintf(buf, size, "%d", index + start);
	else if (style == PDF_NAME(R))
		pdf_format_roman_page_label(buf, size, index + start, roman_uc, "M");
	else if (style == PDF_NAME(r))
		pdf_format_roman_page_label(buf, size, index + start, roman_lc, "m");
	else if (style == PDF_NAME(A))
		pdf_format_alpha_page_label(buf, size, index + start, 'A');
	else if (style == PDF_NAME(a))
		pdf_format_alpha_page_label(buf, size, index + start, 'a');
}

void
pdf_page_label(fz_context *ctx, pdf_document *doc, int index, char *buf, size_t size)
{
	struct page_label_range range = pdf_lookup_page_label(ctx, doc, index);
	if (range.label)
		pdf_format_page_label(ctx, index - range.offset, range.label, buf, size);
	else
		fz_snprintf(buf, size, "%z", index + 1);
}

void
pdf_page_label_imp(fz_context *ctx, fz_document *doc, int chapter, int page, char *buf, size_t size)
{
	pdf_page_label(ctx, pdf_document_from_fz_document(ctx, doc), page, buf, size);
}
