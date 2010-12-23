#include "fitz.h"
#include "mupdf.h"

/* we need to combine all sub-streams into one for the content stream interpreter */

static fz_error
pdf_loadpagecontentsarray(fz_buffer **bigbufp, pdf_xref *xref, fz_obj *list)
{
	fz_error error;
	fz_buffer *big;
	fz_buffer *one;
	int i;

	pdf_logpage("multiple content streams: %d\n", fz_arraylen(list));

	/* TODO: openstream, read, close into big buffer at once */

	big = fz_newbuffer(32 * 1024);

	for (i = 0; i < fz_arraylen(list); i++)
	{
		fz_obj *stm = fz_arrayget(list, i);
		error = pdf_loadstream(&one, xref, fz_tonum(stm), fz_togen(stm));
		if (error)
		{
			fz_dropbuffer(big);
			return fz_rethrow(error, "cannot load content stream part %d/%d (%d %d R)", i + 1, fz_arraylen(list), fz_tonum(stm), fz_togen(stm));
		}

		if (big->len + one->len + 1 > big->cap)
			fz_resizebuffer(big, big->len + one->len + 1);
		memcpy(big->data + big->len, one->data, one->len);
		big->data[big->len + one->len] = ' ';
		big->len += one->len + 1;

		fz_dropbuffer(one);
	}

	*bigbufp = big;
	return fz_okay;
}

static fz_error
pdf_loadpagecontents(fz_buffer **bufp, pdf_xref *xref, fz_obj *obj)
{
	fz_error error;

	if (fz_isarray(obj))
	{
		error = pdf_loadpagecontentsarray(bufp, xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load content stream array (%d 0 R)", fz_tonum(obj));
	}
	else if (pdf_isstream(xref, fz_tonum(obj), fz_togen(obj)))
	{
		error = pdf_loadstream(bufp, xref, fz_tonum(obj), fz_togen(obj));
		if (error)
			return fz_rethrow(error, "cannot load content stream (%d 0 R)", fz_tonum(obj));
	}
	else
	{
		fz_warn("page contents missing, leaving page blank");
		*bufp = fz_newbuffer(0);
	}

	return fz_okay;
}

/* We need to know whether to install a page-level transparency group */

static int pdf_resourcesuseblending(fz_obj *rdb);

static int
pdf_extgstateusesblending(fz_obj *dict)
{
	fz_obj *obj;

	obj = fz_dictgets(dict, "BM");
	if (fz_isname(obj) && strcmp(fz_toname(obj), "Normal"))
		return 1;

	return 0;
}

static int
pdf_patternusesblending(fz_obj *dict)
{
	fz_obj *obj;

	obj = fz_dictgets(dict, "Resources");
	if (fz_isdict(obj) && pdf_resourcesuseblending(obj))
		return 1;

	obj = fz_dictgets(dict, "ExtGState");
	if (fz_isdict(obj) && pdf_extgstateusesblending(obj))
		return 1;

	return 0;
}

static int
pdf_xobjectusesblending(fz_obj *dict)
{
	fz_obj *obj;

	obj = fz_dictgets(dict, "Resources");
	if (fz_isdict(obj) && pdf_resourcesuseblending(obj))
		return 1;

	return 0;
}

static int
pdf_resourcesuseblending(fz_obj *rdb)
{
	fz_obj *dict;
	fz_obj *tmp;
	int i;

	/* stop on cyclic resource dependencies */
	if (fz_dictgets(rdb, ".useBM"))
		return fz_tobool(fz_dictgets(rdb, ".useBM"));

	tmp = fz_newbool(0);
	fz_dictputs(rdb, ".useBM", tmp);
	fz_dropobj(tmp);

	dict = fz_dictgets(rdb, "ExtGState");
	for (i = 0; i < fz_dictlen(dict); i++)
		if (pdf_extgstateusesblending(fz_dictgetval(dict, i)))
			goto found;

	dict = fz_dictgets(rdb, "Pattern");
	for (i = 0; i < fz_dictlen(dict); i++)
		if (pdf_patternusesblending(fz_dictgetval(dict, i)))
			goto found;

	dict = fz_dictgets(rdb, "XObject");
	for (i = 0; i < fz_dictlen(dict); i++)
		if (pdf_xobjectusesblending(fz_dictgetval(dict, i)))
			goto found;

	return 0;

found:
	tmp = fz_newbool(1);
	fz_dictputs(rdb, ".useBM", tmp);
	fz_dropobj(tmp);
	return 1;
}

fz_error
pdf_loadpage(pdf_page **pagep, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_page *page;
	fz_obj *obj;
	fz_bbox bbox;

	pdf_logpage("load page {\n");

	// TODO: move this to a more appropriate place
	/* Ensure that we have a store for resource objects */
	if (!xref->store)
		xref->store = pdf_newstore();

	page = fz_malloc(sizeof(pdf_page));
	page->resources = nil;
	page->contents = nil;
	page->transparency = 0;
	page->list = nil;
	page->text = nil;
	page->links = nil;
	page->annots = nil;

	obj = fz_dictgets(dict, "MediaBox");
	bbox = fz_roundrect(pdf_torect(obj));
	if (fz_isemptyrect(pdf_torect(obj)))
	{
		fz_warn("cannot find page bounds, guessing page bounds.");
		bbox.x0 = 0;
		bbox.y0 = 0;
		bbox.x1 = 612;
		bbox.y1 = 792;
	}

	obj = fz_dictgets(dict, "CropBox");
	if (fz_isarray(obj))
	{
		fz_bbox cropbox = fz_roundrect(pdf_torect(obj));
		bbox = fz_intersectbbox(bbox, cropbox);
	}

	page->mediabox.x0 = MIN(bbox.x0, bbox.x1);
	page->mediabox.y0 = MIN(bbox.y0, bbox.y1);
	page->mediabox.x1 = MAX(bbox.x0, bbox.x1);
	page->mediabox.y1 = MAX(bbox.y0, bbox.y1);

	if (page->mediabox.x1 - page->mediabox.x0 < 1 || page->mediabox.y1 - page->mediabox.y0 < 1)
		return fz_throw("invalid page size");

	page->rotate = fz_toint(fz_dictgets(dict, "Rotate"));

	pdf_logpage("bbox [%d %d %d %d]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);
	pdf_logpage("rotate %d\n", page->rotate);

	obj = fz_dictgets(dict, "Annots");
	if (obj)
	{
		pdf_loadlinks(&page->links, xref, obj);
		pdf_loadannots(&page->annots, xref, obj);
	}

	page->resources = fz_dictgets(dict, "Resources");
	if (page->resources)
		fz_keepobj(page->resources);

	obj = fz_dictgets(dict, "Contents");
	error = pdf_loadpagecontents(&page->contents, xref, obj);
	if (error)
	{
		pdf_freepage(page);
		return fz_rethrow(error, "cannot load page contents (%d %d R)", fz_tonum(obj), fz_togen(obj));
	}

	if (page->resources && pdf_resourcesuseblending(page->resources))
		page->transparency = 1;

	pdf_logpage("} %p\n", page);

	*pagep = page;
	return fz_okay;
}

void
pdf_freepage(pdf_page *page)
{
	pdf_logpage("drop page %p\n", page);
	if (page->resources)
		fz_dropobj(page->resources);
	if (page->contents)
		fz_dropbuffer(page->contents);
	if (page->list)
		fz_freedisplaylist(page->list);
	if (page->text)
		fz_freetextspan(page->text);
	if (page->links)
		pdf_freelink(page->links);
	if (page->annots)
		pdf_freeannot(page->annots);
	fz_free(page);
}
