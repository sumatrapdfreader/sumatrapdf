#include "mupdf/pdf.h"

struct info
{
	pdf_obj *resources;
	pdf_obj *mediabox;
	pdf_obj *cropbox;
	pdf_obj *rotate;
};

typedef struct pdf_page_load_s pdf_page_load;

struct pdf_page_load_s
{
	int max;
	int pos;
	pdf_obj *node;
	pdf_obj *kids;
	struct info info;
};

static void
pdf_load_page_tree_node(pdf_document *doc, pdf_obj *node, struct info info)
{
	pdf_obj *dict, *kids, *count;
	pdf_obj *obj;
	fz_context *ctx = doc->ctx;
	pdf_page_load *stack = NULL;
	int stacklen = -1;
	int stackmax = 0;

	fz_try(ctx)
	{
		do
		{
			if (!node || pdf_mark_obj(node))
			{
				/* NULL node, or we've been here before.
				 * Nothing to do. */
			}
			else
			{
				kids = pdf_dict_gets(node, "Kids");
				count = pdf_dict_gets(node, "Count");
				if (pdf_is_array(kids) && pdf_is_int(count))
				{
					/* Push this onto the stack */
					obj = pdf_dict_gets(node, "Resources");
					if (obj)
						info.resources = obj;
					obj = pdf_dict_gets(node, "MediaBox");
					if (obj)
						info.mediabox = obj;
					obj = pdf_dict_gets(node, "CropBox");
					if (obj)
						info.cropbox = obj;
					obj = pdf_dict_gets(node, "Rotate");
					if (obj)
						info.rotate = obj;
					stacklen++;
					if (stacklen == stackmax)
					{
						stack = fz_resize_array(ctx, stack, stackmax ? stackmax*2 : 10, sizeof(*stack));
						stackmax = stackmax ? stackmax*2 : 10;
					}
					stack[stacklen].kids = kids;
					stack[stacklen].node = node;
					stack[stacklen].pos = -1;
					stack[stacklen].max = pdf_array_len(kids);
					stack[stacklen].info = info;
				}
				else if ((dict = pdf_to_dict(node)) != NULL)
				{
					if (info.resources && !pdf_dict_gets(dict, "Resources"))
						pdf_dict_puts(dict, "Resources", info.resources);
					if (info.mediabox && !pdf_dict_gets(dict, "MediaBox"))
						pdf_dict_puts(dict, "MediaBox", info.mediabox);
					if (info.cropbox && !pdf_dict_gets(dict, "CropBox"))
						pdf_dict_puts(dict, "CropBox", info.cropbox);
					if (info.rotate && !pdf_dict_gets(dict, "Rotate"))
						pdf_dict_puts(dict, "Rotate", info.rotate);

					if (doc->page_len == doc->page_cap)
					{
						fz_warn(ctx, "found more pages than expected");
						doc->page_refs = fz_resize_array(ctx, doc->page_refs, doc->page_cap+1, sizeof(pdf_obj*));
						doc->page_objs = fz_resize_array(ctx, doc->page_objs, doc->page_cap+1, sizeof(pdf_obj*));
						doc->page_cap ++;
					}

					doc->page_refs[doc->page_len] = pdf_keep_obj(node);
					doc->page_objs[doc->page_len] = pdf_keep_obj(dict);
					doc->page_len ++;
					pdf_unmark_obj(node);
				}
			}
			/* Get the next node */
			if (stacklen < 0)
				break;
			while (++stack[stacklen].pos == stack[stacklen].max)
			{
				pdf_unmark_obj(stack[stacklen].node);
				stacklen--;
				if (stacklen < 0) /* No more to pop! */
					break;
				node = stack[stacklen].node;
				info = stack[stacklen].info;
				pdf_unmark_obj(node); /* Unmark it, cos we're about to mark it again */
			}
			if (stacklen >= 0)
				node = pdf_array_get(stack[stacklen].kids, stack[stacklen].pos);
		}
		while (stacklen >= 0);
	}
	fz_always(ctx)
	{
		while (stacklen >= 0)
			pdf_unmark_obj(stack[stacklen--].node);
		fz_free(ctx, stack);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
pdf_load_page_tree(pdf_document *doc)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *catalog;
	pdf_obj *pages;
	pdf_obj *count;
	struct info info;

	if (doc->page_refs)
		return;

	catalog = pdf_dict_gets(pdf_trailer(doc), "Root");
	pages = pdf_dict_gets(catalog, "Pages");
	count = pdf_dict_gets(pages, "Count");

	if (!pdf_is_dict(pages))
		fz_throw(ctx, FZ_ERROR_GENERIC, "missing page tree");
	if (!pdf_is_int(count) || pdf_to_int(count) < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "missing page count");

	doc->page_cap = pdf_to_int(count);
	doc->page_len = 0;
	doc->page_refs = fz_malloc_array(ctx, doc->page_cap, sizeof(pdf_obj*));
	doc->page_objs = fz_malloc_array(ctx, doc->page_cap, sizeof(pdf_obj*));

	info.resources = NULL;
	info.mediabox = NULL;
	info.cropbox = NULL;
	info.rotate = NULL;

	pdf_load_page_tree_node(doc, pages, info);
}

int
pdf_count_pages(pdf_document *doc)
{
	pdf_load_page_tree(doc);
	return doc->page_len;
}

int
pdf_lookup_page_number(pdf_document *doc, pdf_obj *page)
{
	int i, num = pdf_to_num(page);

	pdf_load_page_tree(doc);
	for (i = 0; i < doc->page_len; i++)
		if (num == pdf_to_num(doc->page_refs[i]))
			return i;
	return -1;
}

/* We need to know whether to install a page-level transparency group */

static int pdf_resources_use_blending(pdf_document *doc, pdf_obj *rdb);

static int
pdf_extgstate_uses_blending(pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_gets(dict, "BM");
	/* SumatraPDF: properly support /BM arrays */
	if (pdf_is_array(obj))
	{
		int k;
		for (k = 0; k < pdf_array_len(obj); k++)
		{
			char *bm = pdf_to_name(pdf_array_get(obj, k));
			if (!strcmp(bm, "Normal") || fz_lookup_blendmode(bm) > 0)
				break;
		}
		obj = pdf_array_get(obj, k);
	}
	if (pdf_is_name(obj) && strcmp(pdf_to_name(obj), "Normal"))
		return 1;
	/* SumatraPDF: support transfer functions */
	obj = pdf_dict_getsa(dict, "TR", "TR2");
	if (obj && !pdf_is_name(obj))
		return 1;
	return 0;
}

static int
pdf_pattern_uses_blending(pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *obj;
	obj = pdf_dict_gets(dict, "Resources");
	if (pdf_resources_use_blending(doc, obj))
		return 1;
	obj = pdf_dict_gets(dict, "ExtGState");
	return pdf_extgstate_uses_blending(doc, obj);
}

static int
pdf_xobject_uses_blending(pdf_document *doc, pdf_obj *dict)
{
	pdf_obj *obj = pdf_dict_gets(dict, "Resources");
	return pdf_resources_use_blending(doc, obj);
}

static int
pdf_resources_use_blending(pdf_document *doc, pdf_obj *rdb)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *obj;
	int i, n, useBM = 0;

	if (!rdb)
		return 0;

	/* Have we been here before and remembered an answer? */
	if (pdf_obj_memo(rdb, &useBM))
		return useBM;

	/* stop on cyclic resource dependencies */
	if (pdf_mark_obj(rdb))
		return 0;

	fz_try(ctx)
	{
		obj = pdf_dict_gets(rdb, "ExtGState");
		n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
			if (pdf_extgstate_uses_blending(doc, pdf_dict_get_val(obj, i)))
				goto found;

		obj = pdf_dict_gets(rdb, "Pattern");
		n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
			if (pdf_pattern_uses_blending(doc, pdf_dict_get_val(obj, i)))
				goto found;

		obj = pdf_dict_gets(rdb, "XObject");
		n = pdf_dict_len(obj);
		for (i = 0; i < n; i++)
			if (pdf_xobject_uses_blending(doc, pdf_dict_get_val(obj, i)))
				goto found;
		if (0)
		{
found:
			useBM = 1;
		}
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(rdb);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	pdf_set_obj_memo(rdb, useBM);
	return useBM;
}

static void
pdf_load_transition(pdf_document *doc, pdf_page *page, pdf_obj *transdict)
{
	char *name;
	pdf_obj *obj;
	int type;

	obj = pdf_dict_gets(transdict, "D");
	page->transition.duration = (obj ? pdf_to_real(obj) : 1);

	page->transition.vertical = (pdf_to_name(pdf_dict_gets(transdict, "Dm"))[0] != 'H');
	page->transition.outwards = (pdf_to_name(pdf_dict_gets(transdict, "M"))[0] != 'I');
	/* FIXME: If 'Di' is None, it should be handled differently, but
	 * this only affects Fly, and we don't implement that currently. */
	page->transition.direction = (pdf_to_int(pdf_dict_gets(transdict, "Di")));
	/* FIXME: Read SS for Fly when we implement it */
	/* FIXME: Read B for Fly when we implement it */

	name = pdf_to_name(pdf_dict_gets(transdict, "S"));
	if (!strcmp(name, "Split"))
		type = FZ_TRANSITION_SPLIT;
	else if (!strcmp(name, "Blinds"))
		type = FZ_TRANSITION_BLINDS;
	else if (!strcmp(name, "Box"))
		type = FZ_TRANSITION_BOX;
	else if (!strcmp(name, "Wipe"))
		type = FZ_TRANSITION_WIPE;
	else if (!strcmp(name, "Dissolve"))
		type = FZ_TRANSITION_DISSOLVE;
	else if (!strcmp(name, "Glitter"))
		type = FZ_TRANSITION_GLITTER;
	else if (!strcmp(name, "Fly"))
		type = FZ_TRANSITION_FLY;
	else if (!strcmp(name, "Push"))
		type = FZ_TRANSITION_PUSH;
	else if (!strcmp(name, "Cover"))
		type = FZ_TRANSITION_COVER;
	else if (!strcmp(name, "Uncover"))
		type = FZ_TRANSITION_UNCOVER;
	else if (!strcmp(name, "Fade"))
		type = FZ_TRANSITION_FADE;
	else
		type = FZ_TRANSITION_NONE;
	page->transition.type = type;
}

pdf_page *
pdf_load_page(pdf_document *doc, int number)
{
	fz_context *ctx = doc->ctx;
	pdf_page *page;
	pdf_annot *annot;
	pdf_obj *pageobj, *pageref, *obj;
	fz_rect mediabox, cropbox, realbox;
	float userunit;
	fz_matrix mat;

	pdf_load_page_tree(doc);
	if (number < 0 || number >= doc->page_len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find page %d", number + 1);

	pageobj = doc->page_objs[number];
	pageref = doc->page_refs[number];

	page = fz_malloc_struct(ctx, pdf_page);
	page->resources = NULL;
	page->contents = NULL;
	page->transparency = 0;
	page->links = NULL;
	page->annots = NULL;
	page->deleted_annots = NULL;
	page->tmp_annots = NULL;
	page->me = pdf_keep_obj(pageobj);

	obj = pdf_dict_gets(pageobj, "UserUnit");
	if (pdf_is_real(obj))
		userunit = pdf_to_real(obj);
	else
		userunit = 1;

	pdf_to_rect(ctx, pdf_dict_gets(pageobj, "MediaBox"), &mediabox);
	if (fz_is_empty_rect(&mediabox))
	{
		fz_warn(ctx, "cannot find page size for page %d", number + 1);
		mediabox.x0 = 0;
		mediabox.y0 = 0;
		mediabox.x1 = 612;
		mediabox.y1 = 792;
	}

	pdf_to_rect(ctx, pdf_dict_gets(pageobj, "CropBox"), &cropbox);
	if (!fz_is_empty_rect(&cropbox))
		fz_intersect_rect(&mediabox, &cropbox);

	page->mediabox.x0 = fz_min(mediabox.x0, mediabox.x1) * userunit;
	page->mediabox.y0 = fz_min(mediabox.y0, mediabox.y1) * userunit;
	page->mediabox.x1 = fz_max(mediabox.x0, mediabox.x1) * userunit;
	page->mediabox.y1 = fz_max(mediabox.y0, mediabox.y1) * userunit;

	if (page->mediabox.x1 - page->mediabox.x0 < 1 || page->mediabox.y1 - page->mediabox.y0 < 1)
	{
		fz_warn(ctx, "invalid page size in page %d", number + 1);
		page->mediabox = fz_unit_rect;
	}

	page->rotate = pdf_to_int(pdf_dict_gets(pageobj, "Rotate"));
	/* Snap page->rotate to 0, 90, 180 or 270 */
	if (page->rotate < 0)
		page->rotate = 360 - ((-page->rotate) % 360);
	if (page->rotate >= 360)
		page->rotate = page->rotate % 360;
	page->rotate = 90*((page->rotate + 45)/90);
	if (page->rotate > 360)
		page->rotate = 0;

	fz_pre_rotate(fz_scale(&page->ctm, 1, -1), -page->rotate);
	realbox = page->mediabox;
	fz_transform_rect(&realbox, &page->ctm);
	fz_pre_scale(fz_translate(&mat, -realbox.x0, -realbox.y0), userunit, userunit);
	fz_concat(&page->ctm, &page->ctm, &mat);

	obj = pdf_dict_gets(pageobj, "Annots");
	if (obj)
	{
		/* SumatraPDF: ignore annotations in case of unexpected errors */
		fz_try(ctx)
		{
		page->links = pdf_load_link_annots(doc, obj, &page->ctm);
		page->annots = pdf_load_annots(doc, obj, page);
		}
		fz_catch(ctx)
		{
			fz_warn(ctx, "unexpectedly failed to load page annotations");
		}
	}

	page->duration = pdf_to_real(pdf_dict_gets(pageobj, "Dur"));

	obj = pdf_dict_gets(pageobj, "Trans");
	page->transition_present = (obj != NULL);
	if (obj)
	{
		pdf_load_transition(doc, page, obj);
	}

	page->resources = pdf_dict_gets(pageobj, "Resources");
	if (page->resources)
		pdf_keep_obj(page->resources);

	obj = pdf_dict_gets(pageobj, "Contents");
	fz_try(ctx)
	{
		page->contents = pdf_keep_obj(obj);

		if (pdf_resources_use_blending(doc, page->resources))
			page->transparency = 1;
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2107 */
		else if (!strcmp(pdf_to_name(pdf_dict_getp(pageobj, "Group/S")), "Transparency"))
			page->transparency = 1;

		for (annot = page->annots; annot && !page->transparency; annot = annot->next)
			if (annot->ap && pdf_resources_use_blending(doc, annot->ap->resources))
				page->transparency = 1;
	}
	fz_catch(ctx)
	{
		pdf_free_page(doc, page);
		fz_rethrow_message(ctx, "cannot load page %d contents (%d 0 R)", number + 1, pdf_to_num(pageref));
	}

	return page;
}

fz_rect *
pdf_bound_page(pdf_document *doc, pdf_page *page, fz_rect *bounds)
{
	fz_matrix mtx;
	fz_rect mediabox = page->mediabox;
	fz_transform_rect(&mediabox, fz_rotate(&mtx, page->rotate));
	bounds->x0 = bounds->y0 = 0;
	bounds->x1 = mediabox.x1 - mediabox.x0;
	bounds->y1 = mediabox.y1 - mediabox.y0;
	return bounds;
}

fz_link *
pdf_load_links(pdf_document *doc, pdf_page *page)
{
	return fz_keep_link(doc->ctx, page->links);
}

void
pdf_free_page(pdf_document *doc, pdf_page *page)
{
	if (page == NULL)
		return;
	pdf_drop_obj(page->resources);
	pdf_drop_obj(page->contents);
	if (page->links)
		fz_drop_link(doc->ctx, page->links);
	if (page->annots)
		pdf_free_annot(doc->ctx, page->annots);
	if (page->deleted_annots)
		pdf_free_annot(doc->ctx, page->deleted_annots);
	if (page->tmp_annots)
		pdf_free_annot(doc->ctx, page->tmp_annots);
	/* doc->focus, when not NULL, refers to one of
	 * the annotations and must be NULLed when the
	 * annotations are destroyed. doc->focus_obj
	 * keeps track of the actual annotation object. */
	doc->focus = NULL;
	pdf_drop_obj(page->me);
	fz_free(doc->ctx, page);
}

void
pdf_delete_page(pdf_document *doc, int page)
{
	pdf_delete_page_range(doc, page, page+1);
}

void
pdf_delete_page_range(pdf_document *doc, int start, int end)
{
	int i;

	if (start > end)
	{
		int tmp = start;
		start = end;
		end = tmp;
	}

	if (!doc || start >= doc->page_len || end < 0)
		return;

	for (i=start; i < end; i++)
		pdf_drop_obj(doc->page_refs[i]);
	if (doc->page_len > end)
	{
		memmove(&doc->page_refs[start], &doc->page_refs[end], sizeof(pdf_page *) * (doc->page_len - end + start));
		memmove(&doc->page_refs[start], &doc->page_refs[end], sizeof(pdf_page *) * (doc->page_len - end + start));
	}

	doc->page_len -= end - start;
	doc->needs_page_tree_rebuild = 1;
}

void
pdf_insert_page(pdf_document *doc, pdf_page *page, int at)
{
	if (!doc || !page)
		return;
	if (at < 0)
		at = 0;
	if (at > doc->page_len)
		at = doc->page_len;

	if (doc->page_len + 1 >= doc->page_cap)
	{
		int newmax = doc->page_cap * 2;
		if (newmax == 0)
			newmax = 4;
		doc->page_refs = fz_resize_array(doc->ctx, doc->page_refs, newmax, sizeof(pdf_page *));
		doc->page_objs = fz_resize_array(doc->ctx, doc->page_objs, newmax, sizeof(pdf_page *));
		doc->page_cap = newmax;
	}
	if (doc->page_len > at)
	{
		memmove(&doc->page_objs[at+1], &doc->page_objs[at], doc->page_len - at);
		memmove(&doc->page_refs[at+1], &doc->page_refs[at], doc->page_len - at);
	}

	doc->page_len++;
	doc->page_objs[at] = pdf_keep_obj(page->me);
	doc->page_refs[at] = NULL;
	doc->page_refs[at] = pdf_new_ref(doc, page->me);
	doc->needs_page_tree_rebuild = 1;
}

pdf_page *
pdf_create_page(pdf_document *doc, fz_rect mediabox, int res, int rotate)
{
	pdf_page *page = NULL;
	pdf_obj *pageobj, *obj;
	float userunit = 1;
	fz_context *ctx = doc->ctx;
	fz_matrix ctm, tmp;
	fz_rect realbox;

	page = fz_malloc_struct(ctx, pdf_page);
	obj = NULL;
	fz_var(obj);

	fz_try(ctx)
	{
		page->resources = NULL;
		page->contents = NULL;
		page->transparency = 0;
		page->links = NULL;
		page->annots = NULL;
		page->me = pageobj = pdf_new_dict(doc, 4);

		pdf_dict_puts_drop(pageobj, "Type", pdf_new_name(doc, "Page"));

		page->mediabox.x0 = fz_min(mediabox.x0, mediabox.x1) * userunit;
		page->mediabox.y0 = fz_min(mediabox.y0, mediabox.y1) * userunit;
		page->mediabox.x1 = fz_max(mediabox.x0, mediabox.x1) * userunit;
		page->mediabox.y1 = fz_max(mediabox.y0, mediabox.y1) * userunit;
		pdf_dict_puts_drop(pageobj, "MediaBox", pdf_new_rect(doc, &page->mediabox));

		/* Snap page->rotate to 0, 90, 180 or 270 */
		if (page->rotate < 0)
			page->rotate = 360 - ((-page->rotate) % 360);
		if (page->rotate >= 360)
			page->rotate = page->rotate % 360;
		page->rotate = 90*((page->rotate + 45)/90);
		if (page->rotate > 360)
			page->rotate = 0;
		pdf_dict_puts_drop(pageobj, "Rotate", pdf_new_int(doc, page->rotate));

		fz_pre_rotate(fz_scale(&ctm, 1, -1), -page->rotate);
		realbox = page->mediabox;
		fz_transform_rect(&realbox, &ctm);
		fz_pre_scale(fz_translate(&tmp, -realbox.x0, -realbox.y0), userunit, userunit);
		fz_concat(&ctm, &ctm, &tmp);
		page->ctm = ctm;
		obj = pdf_new_dict(doc, 4);
		page->contents = pdf_new_ref(doc, obj);
		pdf_drop_obj(obj);
		obj = NULL;
		pdf_dict_puts(pageobj, "Contents", page->contents);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(page->me);
		pdf_drop_obj(obj);
		fz_free(ctx, page);
		fz_rethrow_message(ctx, "Failed to create page");
	}

	return page;
}
