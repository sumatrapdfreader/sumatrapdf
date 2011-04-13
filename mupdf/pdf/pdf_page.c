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
pdf_count_pages(pdf_xref *xref)
{
	return xref->page_len;
}

int
pdf_find_page_number(pdf_xref *xref, fz_obj *page)
{
	int i, num = fz_to_num(page);
	for (i = 0; i < xref->page_len; i++)
		if (num == fz_to_num(xref->page_refs[i]))
			return i;
	return -1;
}

static void
pdf_load_page_tree_node(pdf_xref *xref, fz_obj *node, struct info info)
{
	fz_obj *dict, *kids, *count;
	fz_obj *obj, *tmp;
	int i, n;

	/* prevent infinite recursion */
	if (fz_dict_gets(node, ".seen"))
		return;

	kids = fz_dict_gets(node, "Kids");
	count = fz_dict_gets(node, "Count");

	if (fz_is_array(kids) && fz_is_int(count))
	{
		obj = fz_dict_gets(node, "Resources");
		if (obj)
			info.resources = obj;
		obj = fz_dict_gets(node, "MediaBox");
		if (obj)
			info.mediabox = obj;
		obj = fz_dict_gets(node, "CropBox");
		if (obj)
			info.cropbox = obj;
		obj = fz_dict_gets(node, "Rotate");
		if (obj)
			info.rotate = obj;

		tmp = fz_new_null();
		fz_dict_puts(node, ".seen", tmp);
		fz_drop_obj(tmp);

		n = fz_array_len(kids);
		for (i = 0; i < n; i++)
		{
			obj = fz_array_get(kids, i);
			pdf_load_page_tree_node(xref, obj, info);
		}

		fz_dict_dels(node, ".seen");
	}
	else
	{
		dict = fz_resolve_indirect(node);

		if (info.resources && !fz_dict_gets(dict, "Resources"))
			fz_dict_puts(dict, "Resources", info.resources);
		if (info.mediabox && !fz_dict_gets(dict, "MediaBox"))
			fz_dict_puts(dict, "MediaBox", info.mediabox);
		if (info.cropbox && !fz_dict_gets(dict, "CropBox"))
			fz_dict_puts(dict, "CropBox", info.cropbox);
		if (info.rotate && !fz_dict_gets(dict, "Rotate"))
			fz_dict_puts(dict, "Rotate", info.rotate);

		if (xref->page_len == xref->page_cap)
		{
			fz_warn("found more pages than expected");
			xref->page_cap ++;
			xref->page_refs = fz_realloc(xref->page_refs, xref->page_cap, sizeof(fz_obj*));
			xref->page_objs = fz_realloc(xref->page_objs, xref->page_cap, sizeof(fz_obj*));
		}

		xref->page_refs[xref->page_len] = fz_keep_obj(node);
		xref->page_objs[xref->page_len] = fz_keep_obj(dict);
		xref->page_len ++;
	}
}

fz_error
pdf_load_page_tree(pdf_xref *xref)
{
	struct info info;
	fz_obj *catalog = fz_dict_gets(xref->trailer, "Root");
	fz_obj *pages = fz_dict_gets(catalog, "Pages");
	fz_obj *count = fz_dict_gets(pages, "Count");

	if (!fz_is_dict(pages))
		return fz_throw("missing page tree");
	if (!fz_is_int(count))
		return fz_throw("missing page count");

	xref->page_cap = fz_to_int(count);
	xref->page_len = 0;
	xref->page_refs = fz_calloc(xref->page_cap, sizeof(fz_obj*));
	xref->page_objs = fz_calloc(xref->page_cap, sizeof(fz_obj*));

	info.resources = NULL;
	info.mediabox = NULL;
	info.cropbox = NULL;
	info.rotate = NULL;

	pdf_load_page_tree_node(xref, pages, info);

	return fz_okay;
}

/* We need to know whether to install a page-level transparency group */

static int pdf_resources_use_blending(fz_obj *rdb);

static int
pdf_extgstate_uses_blending(fz_obj *dict)
{
	fz_obj *obj = fz_dict_gets(dict, "BM");
	if (fz_is_name(obj) && strcmp(fz_to_name(obj), "Normal"))
		return 1;
	return 0;
}

static int
pdf_pattern_uses_blending(fz_obj *dict)
{
	fz_obj *obj;
	obj = fz_dict_gets(dict, "Resources");
	if (pdf_resources_use_blending(obj))
		return 1;
	obj = fz_dict_gets(dict, "ExtGState");
	if (pdf_extgstate_uses_blending(obj))
		return 1;
	return 0;
}

static int
pdf_xobject_uses_blending(fz_obj *dict)
{
	fz_obj *obj = fz_dict_gets(dict, "Resources");
	if (pdf_resources_use_blending(obj))
		return 1;
	return 0;
}

static int
pdf_resources_use_blending(fz_obj *rdb)
{
	fz_obj *dict;
	fz_obj *tmp;
	int i;

	if (!rdb)
		return 0;

	/* stop on cyclic resource dependencies */
	if (fz_dict_gets(rdb, ".useBM"))
		return fz_to_bool(fz_dict_gets(rdb, ".useBM"));

	tmp = fz_new_bool(0);
	fz_dict_puts(rdb, ".useBM", tmp);
	fz_drop_obj(tmp);

	dict = fz_dict_gets(rdb, "ExtGState");
	for (i = 0; i < fz_dict_len(dict); i++)
		if (pdf_extgstate_uses_blending(fz_dict_get_val(dict, i)))
			goto found;

	dict = fz_dict_gets(rdb, "Pattern");
	for (i = 0; i < fz_dict_len(dict); i++)
		if (pdf_pattern_uses_blending(fz_dict_get_val(dict, i)))
			goto found;

	dict = fz_dict_gets(rdb, "XObject");
	for (i = 0; i < fz_dict_len(dict); i++)
		if (pdf_xobject_uses_blending(fz_dict_get_val(dict, i)))
			goto found;

	return 0;

found:
	tmp = fz_new_bool(1);
	fz_dict_puts(rdb, ".useBM", tmp);
	fz_drop_obj(tmp);
	return 1;
}

/* we need to combine all sub-streams into one for the content stream interpreter */

static fz_error
pdf_load_page_contents_array(fz_buffer **bigbufp, pdf_xref *xref, fz_obj *list)
{
	fz_error error;
	fz_buffer *big;
	fz_buffer *one;
	int i, n;

	big = fz_new_buffer(32 * 1024);

	n = fz_array_len(list);
	for (i = 0; i < n; i++)
	{
		fz_obj *stm = fz_array_get(list, i);
		error = pdf_load_stream(&one, xref, fz_to_num(stm), fz_to_gen(stm));
		if (error)
		{
			fz_catch(error, "cannot load content stream part %d/%d", i + 1, n);
			continue;
		}

		if (big->len + one->len + 1 > big->cap)
			fz_resize_buffer(big, big->len + one->len + 1);
		memcpy(big->data + big->len, one->data, one->len);
		big->data[big->len + one->len] = ' ';
		big->len += one->len + 1;

		fz_drop_buffer(one);
	}

	if (n > 0 && big->len == 0)
	{
		fz_drop_buffer(big);
		return fz_throw("cannot load content stream");
	}

	*bigbufp = big;
	return fz_okay;
}

static fz_error
pdf_load_page_contents(fz_buffer **bufp, pdf_xref *xref, fz_obj *obj)
{
	fz_error error;

	if (fz_is_array(obj))
	{
		error = pdf_load_page_contents_array(bufp, xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load content stream array");
	}
	else if (pdf_is_stream(xref, fz_to_num(obj), fz_to_gen(obj)))
	{
		error = pdf_load_stream(bufp, xref, fz_to_num(obj), fz_to_gen(obj));
		if (error)
			return fz_rethrow(error, "cannot load content stream (%d 0 R)", fz_to_num(obj));
	}
	else
	{
		fz_warn("page contents missing, leaving page blank");
		*bufp = fz_new_buffer(0);
	}

	return fz_okay;
}

fz_error
pdf_load_page(pdf_page **pagep, pdf_xref *xref, int number)
{
	fz_error error;
	pdf_page *page;
	pdf_annot *annot;
	fz_obj *pageobj, *pageref;
	fz_obj *obj;
	fz_bbox bbox;

	if (number < 0 || number >= xref->page_len)
		return fz_throw("cannot find page %d", number + 1);

	/* Ensure that we have a store for resource objects */
	if (!xref->store)
		xref->store = pdf_new_store();

	pageobj = xref->page_objs[number];
	pageref = xref->page_refs[number];

	page = fz_malloc(sizeof(pdf_page));
	page->resources = NULL;
	page->contents = NULL;
	page->transparency = 0;
	page->links = NULL;
	page->annots = NULL;

	obj = fz_dict_gets(pageobj, "MediaBox");
	bbox = fz_round_rect(pdf_to_rect(obj));
	if (fz_is_empty_rect(pdf_to_rect(obj)))
	{
		fz_warn("cannot find page size for page %d", number + 1);
		bbox.x0 = 0;
		bbox.y0 = 0;
		bbox.x1 = 612;
		bbox.y1 = 792;
	}

	obj = fz_dict_gets(pageobj, "CropBox");
	if (fz_is_array(obj))
	{
		fz_bbox cropbox = fz_round_rect(pdf_to_rect(obj));
		bbox = fz_intersect_bbox(bbox, cropbox);
	}

	page->mediabox.x0 = MIN(bbox.x0, bbox.x1);
	page->mediabox.y0 = MIN(bbox.y0, bbox.y1);
	page->mediabox.x1 = MAX(bbox.x0, bbox.x1);
	page->mediabox.y1 = MAX(bbox.y0, bbox.y1);

	if (page->mediabox.x1 - page->mediabox.x0 < 1 || page->mediabox.y1 - page->mediabox.y0 < 1)
	{
		fz_warn("invalid page size in page %d", number + 1);
		page->mediabox = fz_unit_rect;
	}

	page->rotate = fz_to_int(fz_dict_gets(pageobj, "Rotate"));

	obj = fz_dict_gets(pageobj, "Annots");
	if (obj)
	{
		pdf_load_links(&page->links, xref, obj);
		pdf_load_annots(&page->annots, xref, obj);
	}

	page->resources = fz_dict_gets(pageobj, "Resources");
	if (page->resources)
		fz_keep_obj(page->resources);

	obj = fz_dict_gets(pageobj, "Contents");
	error = pdf_load_page_contents(&page->contents, xref, obj);
	if (error)
	{
		pdf_free_page(page);
		return fz_rethrow(error, "cannot load page %d contents (%d 0 R)", number + 1, fz_to_num(pageref));
	}

	if (pdf_resources_use_blending(page->resources))
		page->transparency = 1;

	for (annot = page->annots; annot && !page->transparency; annot = annot->next)
		if (pdf_resources_use_blending(annot->ap->resources))
			page->transparency = 1;

	*pagep = page;
	return fz_okay;
}

void
pdf_free_page(pdf_page *page)
{
	if (page->resources)
		fz_drop_obj(page->resources);
	if (page->contents)
		fz_drop_buffer(page->contents);
	if (page->links)
		pdf_free_link(page->links);
	if (page->annots)
		pdf_free_annot(page->annots);
	fz_free(page);
}
