#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

int
pdf_count_pages(fz_context *ctx, pdf_document *doc)
{
	/* FIXME: We should reset linear_page_count to 0 when editing starts
	 * (or when linear loading ends) */
	if (doc->linear_page_count != 0)
		return doc->linear_page_count;
	return pdf_to_int(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Pages/Count"));
}

int pdf_count_pages_imp(fz_context *ctx, fz_document *doc, int chapter)
{
	return pdf_count_pages(ctx, (pdf_document*)doc);
}

static int
pdf_load_page_tree_imp(fz_context *ctx, pdf_document *doc, pdf_obj *node, int idx)
{
	pdf_obj *type = pdf_dict_get(ctx, node, PDF_NAME(Type));
	if (pdf_name_eq(ctx, type, PDF_NAME(Pages)))
	{
		pdf_obj *kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
		int i, n = pdf_array_len(ctx, kids);

		if (pdf_mark_obj(ctx, node))
			fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in page tree");
		fz_try(ctx)
			for (i = 0; i < n; ++i)
				idx = pdf_load_page_tree_imp(ctx, doc, pdf_array_get(ctx, kids, i), idx);
		fz_always(ctx)
			pdf_unmark_obj(ctx, node);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else if (pdf_name_eq(ctx, type, PDF_NAME(Page)))
	{
		if (idx >= doc->rev_page_count)
			fz_throw(ctx, FZ_ERROR_GENERIC, "too many kids in page tree");
		doc->rev_page_map[idx].page = idx;
		doc->rev_page_map[idx].object = pdf_to_num(ctx, node);
		++idx;
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "non-page object in page tree");
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
	if (!doc->rev_page_map)
	{
		doc->rev_page_count = pdf_count_pages(ctx, doc);
		doc->rev_page_map = Memento_label(fz_malloc_array(ctx, doc->rev_page_count, pdf_rev_page_map), "pdf_rev_page_map");
		pdf_load_page_tree_imp(ctx, doc, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/Pages"), 0);
		qsort(doc->rev_page_map, doc->rev_page_count, sizeof *doc->rev_page_map, cmp_rev_page_map);
	}
}

void
pdf_drop_page_tree(fz_context *ctx, pdf_document *doc)
{
	fz_free(ctx, doc->rev_page_map);
	doc->rev_page_map = NULL;
	doc->rev_page_count = 0;
}

enum
{
	LOCAL_STACK_SIZE = 16
};

static pdf_obj *
pdf_lookup_page_loc_imp(fz_context *ctx, pdf_document *doc, pdf_obj *node, int *skip, pdf_obj **parentp, int *indexp)
{
	pdf_obj *kids;
	pdf_obj *hit = NULL;
	int i, len;
	pdf_obj *local_stack[LOCAL_STACK_SIZE];
	pdf_obj **stack = &local_stack[0];
	int stack_max = LOCAL_STACK_SIZE;
	int stack_len = 0;

	fz_var(hit);
	fz_var(stack);
	fz_var(stack_len);
	fz_var(stack_max);

	fz_try(ctx)
	{
		do
		{
			kids = pdf_dict_get(ctx, node, PDF_NAME(Kids));
			len = pdf_array_len(ctx, kids);

			if (len == 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "malformed page tree");

			/* Every node we need to unmark goes into the stack */
			if (stack_len == stack_max)
			{
				if (stack == &local_stack[0])
				{
					stack = fz_malloc_array(ctx, stack_max * 2, pdf_obj*);
					memcpy(stack, &local_stack[0], stack_max * sizeof(*stack));
				}
				else
				{
					stack = fz_realloc_array(ctx, stack, stack_max * 2, pdf_obj*);
				}
				stack_max *= 2;
			}
			stack[stack_len++] = node;

			if (pdf_mark_obj(ctx, node))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in page tree");

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
		for (i = stack_len; i > 0; i--)
			pdf_unmark_obj(ctx, stack[i-1]);
		if (stack != &local_stack[0])
			fz_free(ctx, stack);
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find page tree");

	hit = pdf_lookup_page_loc_imp(ctx, doc, node, &skip, parentp, indexp);
	if (!hit)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find page %d in page tree", needle+1);
	return hit;
}

pdf_obj *
pdf_lookup_page_obj(fz_context *ctx, pdf_document *doc, int needle)
{
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
				fz_throw(ctx, FZ_ERROR_GENERIC, "illegal or missing count in pages tree");
			total += n;
		}
		else
			total++;
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "kid not found in parent's kids array");
}

static int
pdf_lookup_page_number_slow(fz_context *ctx, pdf_document *doc, pdf_obj *node)
{
	int needle = pdf_to_num(ctx, node);
	int total = 0;
	pdf_obj *parent, *parent2;

	if (!pdf_name_eq(ctx, pdf_dict_get(ctx, node, PDF_NAME(Type)), PDF_NAME(Page)))
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid page object");

	parent2 = parent = pdf_dict_get(ctx, node, PDF_NAME(Parent));
	fz_var(parent);
	fz_try(ctx)
	{
		while (pdf_is_dict(ctx, parent))
		{
			if (pdf_mark_obj(ctx, parent))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in page tree (parents)");
			total += pdf_count_pages_before_kid(ctx, doc, parent, needle);
			needle = pdf_to_num(ctx, parent);
			parent = pdf_dict_get(ctx, parent, PDF_NAME(Parent));
		}
	}
	fz_always(ctx)
	{
		/* Run back and unmark */
		while (parent2)
		{
			pdf_unmark_obj(ctx, parent2);
			if (parent2 == parent)
				break;
			parent2 = pdf_dict_get(ctx, parent2, PDF_NAME(Parent));
		}
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return total;
}

static int
pdf_lookup_page_number_fast(fz_context *ctx, pdf_document *doc, int needle)
{
	int l = 0;
	int r = doc->rev_page_count - 1;
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
	if (doc->rev_page_map)
		return pdf_lookup_page_number_fast(ctx, doc, pdf_to_num(ctx, page));
	else
		return pdf_lookup_page_number_slow(ctx, doc, page);
}

/*
	Find the page number of a named destination.

	For use with looking up the destination page of a fragment
	identifier in hyperlinks: foo.pdf#bar or foo.pdf#page=5.
*/
int
pdf_lookup_anchor(fz_context *ctx, pdf_document *doc, const char *name, float *xp, float *yp)
{
	pdf_obj *needle, *dest = NULL;
	char *uri;

	if (xp) *xp = 0;
	if (yp) *yp = 0;

	needle = pdf_new_string(ctx, name, strlen(name));
	fz_try(ctx)
		dest = pdf_lookup_dest(ctx, doc, needle);
	fz_always(ctx)
		pdf_drop_obj(ctx, needle);
	fz_catch(ctx)
		fz_rethrow(ctx);

	if (dest)
	{
		uri = pdf_parse_link_dest(ctx, doc, dest);
		return pdf_resolve_link(ctx, doc, uri, xp, yp);
	}

	if (!strncmp(name, "page=", 5))
		return fz_atoi(name + 5) - 1;

	return fz_atoi(name) - 1;
}

static void
pdf_flatten_inheritable_page_item(fz_context *ctx, pdf_obj *page, pdf_obj *key)
{
	pdf_obj *val = pdf_dict_get_inheritable(ctx, page, key);
	if (val)
		pdf_dict_put(ctx, page, key, val);
}

/*
	Make page self sufficient.

	Copy any inheritable page keys into the actual page object, removing
	any dependencies on the page tree parents.
*/
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

static int pdf_resources_use_blending(fz_context *ctx, pdf_obj *rdb);

static int
pdf_extgstate_uses_blending(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(BM));
	if (obj && !pdf_name_eq(ctx, obj, PDF_NAME(Normal)))
		return 1;
	return 0;
}

static int
pdf_pattern_uses_blending(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	if (pdf_resources_use_blending(ctx, obj))
		return 1;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(ExtGState));
	return pdf_extgstate_uses_blending(ctx, obj);
}

static int
pdf_xobject_uses_blending(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	if (pdf_name_eq(ctx, pdf_dict_getp(ctx, dict, "Group/S"), PDF_NAME(Transparency)))
		return 1;
	return pdf_resources_use_blending(ctx, obj);
}

static int
pdf_resources_use_blending(fz_context *ctx, pdf_obj *rdb)
{
	pdf_obj *obj;
	int i, n, useBM = 0;

	if (!rdb)
		return 0;

	/* Have we been here before and remembered an answer? */
	if (pdf_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_BM, &useBM))
		return useBM;

	/* stop on cyclic resource dependencies */
	if (pdf_mark_obj(ctx, rdb))
		return 0;

	fz_try(ctx)
	{
		obj = pdf_dict_get(ctx, rdb, PDF_NAME(ExtGState));
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			if (pdf_extgstate_uses_blending(ctx, pdf_dict_get_val(ctx, obj, i)))
				goto found;

		obj = pdf_dict_get(ctx, rdb, PDF_NAME(Pattern));
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			if (pdf_pattern_uses_blending(ctx, pdf_dict_get_val(ctx, obj, i)))
				goto found;

		obj = pdf_dict_get(ctx, rdb, PDF_NAME(XObject));
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			if (pdf_xobject_uses_blending(ctx, pdf_dict_get_val(ctx, obj, i)))
				goto found;
		if (0)
		{
found:
			useBM = 1;
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(ctx, rdb);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	pdf_set_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_BM, useBM);
	return useBM;
}

static int pdf_resources_use_overprint(fz_context *ctx, pdf_obj *rdb);

static int
pdf_extgstate_uses_overprint(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(OP));
	if (obj && pdf_to_bool(ctx, obj))
		return 1;
	return 0;
}

static int
pdf_pattern_uses_overprint(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	if (pdf_resources_use_overprint(ctx, obj))
		return 1;
	obj = pdf_dict_get(ctx, dict, PDF_NAME(ExtGState));
	return pdf_extgstate_uses_overprint(ctx, obj);
}

static int
pdf_xobject_uses_overprint(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_get(ctx, dict, PDF_NAME(Resources));
	return pdf_resources_use_overprint(ctx, obj);
}

static int
pdf_resources_use_overprint(fz_context *ctx, pdf_obj *rdb)
{
	pdf_obj *obj;
	int i, n, useOP = 0;

	if (!rdb)
		return 0;

	/* Have we been here before and remembered an answer? */
	if (pdf_obj_memo(ctx, rdb, PDF_FLAGS_MEMO_OP, &useOP))
		return useOP;

	/* stop on cyclic resource dependencies */
	if (pdf_mark_obj(ctx, rdb))
		return 0;

	fz_try(ctx)
	{
		obj = pdf_dict_get(ctx, rdb, PDF_NAME(ExtGState));
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			if (pdf_extgstate_uses_overprint(ctx, pdf_dict_get_val(ctx, obj, i)))
				goto found;

		obj = pdf_dict_get(ctx, rdb, PDF_NAME(Pattern));
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			if (pdf_pattern_uses_overprint(ctx, pdf_dict_get_val(ctx, obj, i)))
				goto found;

		obj = pdf_dict_get(ctx, rdb, PDF_NAME(XObject));
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
			if (pdf_xobject_uses_overprint(ctx, pdf_dict_get_val(ctx, obj, i)))
				goto found;
		if (0)
		{
found:
			useOP = 1;
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(ctx, rdb);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
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

	transition->duration = (obj ? pdf_to_real(ctx, obj) : 1);

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

/*
	Determine the size of a page.

	Determine the page size in user space units, taking page rotation
	into account. The page size is taken to be the crop box if it
	exists (visible area after cropping), otherwise the media box will
	be used (possibly including printing marks).
*/
fz_rect
pdf_bound_page(fz_context *ctx, pdf_page *page)
{
	fz_matrix page_ctm;
	fz_rect mediabox;
	pdf_page_transform(ctx, page, &mediabox, &page_ctm);
	return fz_transform_rect(mediabox, page_ctm);
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
pdf_page_obj_transform(fz_context *ctx, pdf_obj *pageobj, fz_rect *page_mediabox, fz_matrix *page_ctm)
{
	pdf_obj *obj;
	fz_rect mediabox, cropbox, realbox, pagebox;
	float userunit = 1;
	int rotate;

	if (!page_mediabox)
		page_mediabox = &pagebox;

	obj = pdf_dict_get(ctx, pageobj, PDF_NAME(UserUnit));
	if (pdf_is_real(ctx, obj))
		userunit = pdf_to_real(ctx, obj);

	mediabox = pdf_to_rect(ctx, pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(MediaBox)));
	if (fz_is_empty_rect(mediabox))
	{
		mediabox.x0 = 0;
		mediabox.y0 = 0;
		mediabox.x1 = 612;
		mediabox.y1 = 792;
	}

	cropbox = pdf_to_rect(ctx, pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(CropBox)));
	if (!fz_is_empty_rect(cropbox))
		mediabox = fz_intersect_rect(mediabox, cropbox);

	page_mediabox->x0 = fz_min(mediabox.x0, mediabox.x1);
	page_mediabox->y0 = fz_min(mediabox.y0, mediabox.y1);
	page_mediabox->x1 = fz_max(mediabox.x0, mediabox.x1);
	page_mediabox->y1 = fz_max(mediabox.y0, mediabox.y1);

	if (page_mediabox->x1 - page_mediabox->x0 < 1 || page_mediabox->y1 - page_mediabox->y0 < 1)
		*page_mediabox = fz_unit_rect;

	rotate = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(Rotate)));

	/* Snap page rotation to 0, 90, 180 or 270 */
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

	/* Translate page origin to 0,0 */
	realbox = fz_transform_rect(*page_mediabox, *page_ctm);
	*page_ctm = fz_concat(*page_ctm, fz_translate(-realbox.x0, -realbox.y0));
}

void
pdf_page_transform(fz_context *ctx, pdf_page *page, fz_rect *page_mediabox, fz_matrix *page_ctm)
{
	pdf_page_obj_transform(ctx, page->obj, page_mediabox, page_ctm);
}

static void
find_seps(fz_context *ctx, fz_separations **seps, pdf_obj *obj, pdf_obj *clearme)
{
	int i, n;
	pdf_obj *nameobj;

	/* Indexed and DeviceN may have cyclic references */
	if (pdf_is_indirect(ctx, obj))
	{
		if (pdf_mark_obj(ctx, obj))
			return; /* already been here */
		/* remember to clear this colorspace dictionary at the end */
		pdf_array_push(ctx, clearme, obj);
	}

	nameobj = pdf_array_get(ctx, obj, 0);
	if (pdf_name_eq(ctx, nameobj, PDF_NAME(Separation)))
	{
		fz_colorspace *cs;
		const char *name = pdf_to_name(ctx, pdf_array_get(ctx, obj, 1));

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
		pdf_obj *cols = pdf_dict_get(ctx, pdf_array_get(ctx, obj, 4), PDF_NAME(Colorants));
		n = pdf_dict_len(ctx, cols);
		for (i = 0; i < n; i++)
			find_seps(ctx, seps, pdf_dict_get_val(ctx, cols, i), clearme);
	}
}

static void
find_devn(fz_context *ctx, fz_separations **seps, pdf_obj *obj, pdf_obj *clearme)
{
	int i, j, n, m;
	pdf_obj *arr;
	pdf_obj *nameobj = pdf_array_get(ctx, obj, 0);

	if (!pdf_name_eq(ctx, nameobj, PDF_NAME(DeviceN)))
		return;

	arr = pdf_array_get(ctx, obj, 1);
	m = pdf_array_len(ctx, arr);
	for (j = 0; j < m; j++)
	{
		fz_colorspace *cs;
		const char *name = pdf_to_name(ctx, pdf_array_get(ctx, arr, j));

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

typedef void (res_finder_fn)(fz_context *ctx, fz_separations **seps, pdf_obj *obj, pdf_obj *clearme);

static void
scan_page_seps(fz_context *ctx, pdf_obj *res, fz_separations **seps, res_finder_fn *fn, pdf_obj *clearme)
{
	pdf_obj *dict;
	pdf_obj *obj;
	int i, n;

	if (pdf_mark_obj(ctx, res))
		return; /* already been here */

	/* remember to clear this resource dictionary at the end */
	pdf_array_push(ctx, clearme, res);

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
		fn(ctx, seps, pdf_dict_get(ctx, obj, PDF_NAME(ColorSpace)), clearme);
		/* Recurse on XObject forms. */
		scan_page_seps(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Resources)), seps, fn, clearme);
	}
}

/*
	Get the separation details for a page.
*/
fz_separations *
pdf_page_separations(fz_context *ctx, pdf_page *page)
{
	pdf_obj *res = pdf_page_resources(ctx, page);
	pdf_obj *clearme = NULL;
	fz_separations *seps = NULL;

	clearme = pdf_new_array(ctx, page->doc, 100);
	fz_try(ctx)
	{
		/* Run through and look for separations first. This is
		 * because separations are simplest to deal with, and
		 * because DeviceN may be implemented on top of separations.
		 */
		scan_page_seps(ctx, res, &seps, find_seps, clearme);
	}
	fz_always(ctx)
	{
		int i, n = pdf_array_len(ctx, clearme);
		for (i = 0; i < n; ++i)
			pdf_unmark_obj(ctx, pdf_array_get(ctx, clearme, i));
		pdf_drop_obj(ctx, clearme);
	}
	fz_catch(ctx)
	{
		fz_drop_separations(ctx, seps);
		fz_rethrow(ctx);
	}

	clearme = pdf_new_array(ctx, page->doc, 100);
	fz_try(ctx)
	{
		/* Now run through again, and look for DeviceNs. These may
		 * have spot colors in that aren't defined in terms of
		 * separations. */
		scan_page_seps(ctx, res, &seps, find_devn, clearme);
	}
	fz_always(ctx)
	{
		int i, n = pdf_array_len(ctx, clearme);
		for (i = 0; i < n; ++i)
			pdf_unmark_obj(ctx, pdf_array_get(ctx, clearme, i));
		pdf_drop_obj(ctx, clearme);
	}
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

	fz_drop_document(ctx, &page->doc->super);
}

static pdf_page *
pdf_new_page(fz_context *ctx, pdf_document *doc)
{
	pdf_page *page = fz_new_derived_page(ctx, pdf_page);

	page->doc = (pdf_document*) fz_keep_document(ctx, &doc->super);

	page->super.drop_page = (fz_page_drop_page_fn*)pdf_drop_page_imp;
	page->super.load_links = (fz_page_load_links_fn*)pdf_load_links;
	page->super.bound_page = (fz_page_bound_page_fn*)pdf_bound_page;
	page->super.run_page_contents = (fz_page_run_page_fn*)pdf_run_page_contents;
	page->super.run_page_annots = (fz_page_run_page_fn*)pdf_run_page_annots;
	page->super.run_page_widgets = (fz_page_run_page_fn*)pdf_run_page_widgets;
	page->super.page_presentation = (fz_page_page_presentation_fn*)pdf_page_presentation;
	page->super.separations = (fz_page_separations_fn *)pdf_page_separations;
	page->super.overprint = (fz_page_uses_overprint_fn *)pdf_page_uses_overprint;

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
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
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
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
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
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
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
		page->super.incomplete = 1;
	}

	return default_cs;
}

/*
	Update default colorspaces for an xobject.
*/
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

/*
	Load a page and its resources.

	Locates the page in the PDF document and loads the page and its
	resources. After pdf_load_page is it possible to retrieve the size
	of the page using pdf_bound_page, or to render the page using
	pdf_run_page_*.

	number: page number, where 0 is the first page of the document.
*/
pdf_page *
pdf_load_page(fz_context *ctx, pdf_document *doc, int number)
{
	pdf_page *page;
	pdf_annot *annot;
	pdf_obj *pageobj, *obj;

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
			fz_rect page_mediabox;
			fz_matrix page_ctm;
			pdf_page_transform(ctx, page, &page_mediabox, &page_ctm);
			page->links = pdf_load_link_annots(ctx, doc, obj, number, page_ctm);
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
		else if (pdf_resources_use_blending(ctx, resources))
			page->transparency = 1;
		for (annot = page->annots; annot && !page->transparency; annot = annot->next)
			if (annot->ap && pdf_resources_use_blending(ctx, pdf_xobject_resources(ctx, annot->ap)))
				page->transparency = 1;
		if (pdf_resources_use_overprint(ctx, resources))
			page->overprint = 1;
		for (annot = page->annots; annot && !page->overprint; annot = annot->next)
			if (annot->ap && pdf_resources_use_overprint(ctx, pdf_xobject_resources(ctx, annot->ap)))
				page->overprint = 1;
	}
	fz_catch(ctx)
	{
		if (fz_caught(ctx) != FZ_ERROR_TRYLATER)
		{
			fz_drop_page(ctx, &page->super);
			fz_rethrow(ctx);
		}
		page->super.incomplete = 1;
	}

	return page;
}

fz_page *pdf_load_page_imp(fz_context *ctx, fz_document *doc, int chapter, int number)
{
	return (fz_page*)pdf_load_page(ctx, (pdf_document*)doc, number);
}

/*
	Delete a page from the page tree of
	a document. This does not remove the page contents
	or resources from the file.

	doc: The document to operate on.

	number: The page to remove (numbered from 0)
*/
void
pdf_delete_page(fz_context *ctx, pdf_document *doc, int at)
{
	pdf_obj *parent, *kids;
	int i;

	pdf_lookup_page_loc(ctx, doc, at, &parent, &i);
	kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
	pdf_array_delete(ctx, kids, i);

	while (parent)
	{
		int count = pdf_dict_get_int(ctx, parent, PDF_NAME(Count));
		pdf_dict_put_int(ctx, parent, PDF_NAME(Count), count - 1);
		parent = pdf_dict_get(ctx, parent, PDF_NAME(Parent));
	}
}

/*
	Delete a range of pages from the
	page tree of a document. This does not remove the page
	contents or resources from the file.

	doc: The document to operate on.

	start, end: The range of pages (numbered from 0)
	(inclusive, exclusive) to remove. If end is negative or
	greater than the number of pages in the document, it
	will be taken to be the end of the document.
*/
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

/*
	Create a pdf_obj within a document that
	represents a page, from a previously created resources
	dictionary and page content stream. This should then be
	inserted into the document using pdf_insert_page.

	After this call the page exists within the document
	structure, but is not actually ever displayed as it is
	not linked into the PDF page tree.

	doc: The document to which to add the page.

	mediabox: The mediabox for the page (should be identical
	to that used when creating the resources/contents).

	rotate: 0, 90, 180 or 270. The rotation to use for the
	page.

	resources: The resources dictionary for the new page
	(typically created by pdf_page_write).

	contents: The page contents for the new page (typically
	create by pdf_page_write).
*/
pdf_obj *
pdf_add_page(fz_context *ctx, pdf_document *doc, fz_rect mediabox, int rotate, pdf_obj *resources, fz_buffer *contents)
{
	pdf_obj *page_obj = pdf_new_dict(ctx, doc, 5);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, page_obj, PDF_NAME(Type), PDF_NAME(Page));
		pdf_dict_put_rect(ctx, page_obj, PDF_NAME(MediaBox), mediabox);
		pdf_dict_put_int(ctx, page_obj, PDF_NAME(Rotate), rotate);

		if (pdf_is_indirect(ctx, resources))
			pdf_dict_put(ctx, page_obj, PDF_NAME(Resources), resources);
		else if (pdf_is_dict(ctx, resources))
			pdf_dict_put_drop(ctx, page_obj, PDF_NAME(Resources), pdf_add_object(ctx, doc, resources));
		else
			pdf_dict_put_dict(ctx, page_obj, PDF_NAME(Resources), 1);

		if (contents)
			pdf_dict_put_drop(ctx, page_obj, PDF_NAME(Contents), pdf_add_stream(ctx, doc, contents, NULL, 0));
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, page_obj);
		fz_rethrow(ctx);
	}
	return pdf_add_object_drop(ctx, doc, page_obj);
}

/*
	Insert a page previously created by
	pdf_add_page into the pages tree of the document.

	doc: The document to insert into.

	at: The page number to insert at. 0 inserts at the start.
	negative numbers, or INT_MAX insert at the end. Otherwise
	n inserts after page n.

	page: The page to insert.
*/
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
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot insert page beyond end of page tree");

	if (count == 0)
	{
		pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
		parent = pdf_dict_get(ctx, root, PDF_NAME(Pages));
		if (!parent)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find page tree");
		kids = pdf_dict_get(ctx, parent, PDF_NAME(Kids));
		if (!kids)
			fz_throw(ctx, FZ_ERROR_GENERIC, "malformed page tree");
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
}
