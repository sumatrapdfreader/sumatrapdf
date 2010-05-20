#include "fitz.h"
#include "mupdf.h"

/* we need to combine all sub-streams into one for the content stream interpreter */
static fz_error
pdf_loadpagecontentsarray(fz_buffer **bigbufp, pdf_xref *xref, fz_obj *list)
{
	fz_error error;
	fz_buffer *big;
	fz_buffer *one;
	int i, n;

	pdf_logpage("multiple content streams: %d\n", fz_arraylen(list));

	big = fz_newbuffer(32 * 1024);

	for (i = 0; i < fz_arraylen(list); i++)
	{
		fz_obj *stm = fz_arrayget(list, i);
		error = pdf_loadstream(&one, xref, fz_tonum(stm), fz_togen(stm));
		if (error)
		{
			fz_dropbuffer(big);
			return fz_rethrow(error, "cannot load content stream part %d/%d", i + 1, fz_arraylen(list));
		}

		n = one->wp - one->rp;
		while (big->wp + n + 1 > big->ep)
			fz_growbuffer(big);
		memcpy(big->wp, one->rp, n);
		big->wp += n;
		*big->wp++ = ' ';

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
			return fz_rethrow(error, "cannot load content stream array");
	}
	else if (pdf_isstream(xref, fz_tonum(obj), fz_togen(obj)))
	{
		error = pdf_loadstream(bufp, xref, fz_tonum(obj), fz_togen(obj));
		if (error)
			return fz_rethrow(error, "cannot load content stream");
	}
	else
	{
		fz_warn("page contents missing, leaving page blank");
		*bufp = fz_newbuffer(0);
	}

	return fz_okay;
}

fz_error
pdf_loadpage(pdf_page **pagep, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_page *page;
	fz_obj *obj;
	fz_rect bbox;

	pdf_logpage("load page {\n");

	// TODO: move this to a more appropriate place
	/* Ensure that we have a store for resource objects */
	if (!xref->store)
		xref->store = pdf_newstore();

	page = fz_malloc(sizeof(pdf_page));
	page->resources = nil;
	page->contents = nil;
	page->comments = nil;
	page->links = nil;

	obj = fz_dictgets(dict, "CropBox");
	if (!obj)
		obj = fz_dictgets(dict, "MediaBox");
	if (!fz_isarray(obj))
		return fz_throw("cannot find page bounds");
	bbox = pdf_torect(obj);
	if (bbox.x1 - bbox.x0 < 1 || bbox.y1 - bbox.y0 < 1)
		return fz_throw("invalid page size");

	page->mediabox.x0 = MIN(bbox.x0, bbox.x1);
	page->mediabox.y0 = MIN(bbox.y0, bbox.y1);
	page->mediabox.x1 = MAX(bbox.x0, bbox.x1);
	page->mediabox.y1 = MAX(bbox.y0, bbox.y1);
	page->rotate = fz_toint(fz_dictgets(dict, "Rotate"));

	pdf_logpage("bbox [%g %g %g %g]\n", bbox.x0, bbox.y0, bbox.x1, bbox.y1);
	pdf_logpage("rotate %d\n", page->rotate);

	obj = fz_dictgets(dict, "Annots");
	if (obj)
		pdf_loadannots(&page->comments, &page->links, xref, obj);

	page->resources = fz_dictgets(dict, "Resources");
	if (page->resources)
		fz_keepobj(page->resources);

	obj = fz_dictgets(dict, "Contents");
	error = pdf_loadpagecontents(&page->contents, xref, obj);
	if (error)
	{
		pdf_droppage(page);
		return fz_rethrow(error, "cannot load page contents");
	}

	pdf_logpage("} %p\n", page);

	*pagep = page;
	return fz_okay;
}

void
pdf_droppage(pdf_page *page)
{
	pdf_logpage("drop page %p\n", page);
	if (page->resources)
		fz_dropobj(page->resources);
	if (page->contents)
		fz_dropbuffer(page->contents);
	if (page->links)
		pdf_droplink(page->links);
//	if (page->comments)
//		pdf_dropcomment(page->comments);
	fz_free(page);
}
