#include "fitz.h"
#include "mupdf.h"

static fz_error
runone(pdf_csi *csi, pdf_xref *xref, fz_obj *rdb, fz_obj *stmref)
{
	fz_error error;
	fz_stream *stm;
	fz_buffer *buf;

	pdf_logpage("simple content stream\n");

	error = pdf_loadstream(&buf, xref, fz_tonum(stmref), fz_togen(stmref));
	if (error)
		return fz_rethrow(error, "cannot load content stream (%d %d R)", fz_tonum(stmref), fz_togen(stmref));

	stm = fz_openrbuffer(buf);

	error = pdf_runcsi(csi, xref, rdb, stm);

	fz_dropstream(stm);
	fz_dropbuffer(buf);

	if (error)
		return fz_rethrow(error, "cannot interpret content stream (%d %d R)", fz_tonum(stmref), fz_togen(stmref));

	return fz_okay;
}

/* we need to combine all sub-streams into one for pdf_runcsi
 * to deal with split dictionaries etc.
 */
static fz_error
runmany(pdf_csi *csi, pdf_xref *xref, fz_obj *rdb, fz_obj *list)
{
	fz_error error;
	fz_stream *file;
	fz_buffer *big;
	fz_buffer *one;
	fz_obj *stm;
	int i, n;

	pdf_logpage("multiple content streams: %d\n", fz_arraylen(list));

	big = fz_newbuffer(32 * 1024);

	for (i = 0; i < fz_arraylen(list); i++)
	{
		stm = fz_arrayget(list, i);
		error = pdf_loadstream(&one, xref, fz_tonum(stm), fz_togen(stm));
		if (error)
		{
			fz_dropbuffer(big);
			return fz_rethrow(error, "cannot load content stream part %d/%d", i + 1, fz_arraylen(list));
		}

		n = one->wp - one->rp;

		while (big->wp + n + 1 > big->ep)
		{
			fz_growbuffer(big);
		}

		memcpy(big->wp, one->rp, n);

		big->wp += n;
		*big->wp++ = ' ';

		fz_dropbuffer(one);
	}

	file = fz_openrbuffer(big);

	error = pdf_runcsi(csi, xref, rdb, file);
	if (error)
	{
		fz_dropbuffer(big);
		fz_dropstream(file);
		return fz_rethrow(error, "cannot interpret content buffer");
	}

	fz_dropstream(file);
	fz_dropbuffer(big);
	return fz_okay;
}

static fz_error
loadpagecontents(fz_tree **treep, pdf_xref *xref, fz_obj *rdb, fz_obj *obj)
{
	fz_error error = fz_okay;
	pdf_csi *csi;

	error = pdf_newcsi(&csi, 0);
	if (error)
		return fz_rethrow(error, "cannot create interpreter");

	if (fz_isarray(obj))
	{
		if (fz_arraylen(obj) == 1)
			error = runone(csi, xref, rdb, fz_arrayget(obj, 0));
		else
			error = runmany(csi, xref, rdb, obj);
	}
	else if (pdf_isstream(xref, fz_tonum(obj), fz_togen(obj)))
		error = runone(csi, xref, rdb, obj);
	else
		fz_warn("page contents missing, leaving page blank");

	if (obj && error)
	{
		pdf_dropcsi(csi);
		return fz_rethrow(error, "cannot interpret page contents (%d %d R)", fz_tonum(obj), fz_togen(obj));
	}

	*treep = csi->tree;
	csi->tree = nil;

	pdf_dropcsi(csi);

	return fz_okay;
}

fz_error
pdf_loadpage(pdf_page **pagep, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_obj *obj;
	pdf_page *page;
	fz_obj *rdb;
	pdf_comment *comments = nil;
	pdf_link *links = nil;
	fz_tree *tree = nil;
	fz_rect bbox;
	int rotate;

	pdf_logpage("load page {\n");

	/*
	 * Sort out page media
	 */

	obj = fz_dictgets(dict, "CropBox");
	if (!obj)
		obj = fz_dictgets(dict, "MediaBox");
	if (!fz_isarray(obj))
		return fz_throw("cannot find page bounds");
	bbox = pdf_torect(obj);

	pdf_logpage("bbox [%g %g %g %g]\n",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1);

	if (bbox.x1 - bbox.x0 < 1 || bbox.y1 - bbox.y0 < 1)
		return fz_throw("invalid page size");

	obj = fz_dictgets(dict, "Rotate");
	if (fz_isint(obj))
		rotate = fz_toint(obj);
	else
		rotate = 0;

	pdf_logpage("rotate %d\n", rotate);

	/*
	 * Load annotations
	 */

	obj = fz_dictgets(dict, "Annots");
	if (obj)
	{
		pdf_loadannots(&comments, &links, xref, obj);
	}

	/*
	 * Create store for resource objects
	 */

	if (!xref->store)
	{
		xref->store = pdf_newstore();
	}

	/*
	 * Locate resources
	 */

	rdb = fz_dictgets(dict, "Resources");
	if (rdb)
		rdb = fz_keepobj(rdb);
	else
	{
		fz_warn("cannot find page resources, proceeding anyway.");
		rdb = fz_newdict(0);
	}

	/*
	 * Interpret content stream to build display tree
	 */

	obj = fz_dictgets(dict, "Contents");

	error = loadpagecontents(&tree, xref, rdb, obj);
	if (error)
	{
		fz_dropobj(rdb);
		return fz_rethrow(error, "cannot load page contents");
	}

	/*
	 * Create page object
	 */

	page = fz_malloc(sizeof(pdf_page));
	page->mediabox.x0 = MIN(bbox.x0, bbox.x1);
	page->mediabox.y0 = MIN(bbox.y0, bbox.y1);
	page->mediabox.x1 = MAX(bbox.x0, bbox.x1);
	page->mediabox.y1 = MAX(bbox.y0, bbox.y1);
	page->rotate = rotate;
	page->resources = rdb; /* we have already kept or created it */
	page->tree = tree;

	page->comments = comments;
	page->links = links;

	pdf_logpage("} %p\n", page);

	*pagep = page;
	return fz_okay;
}

void
pdf_droppage(pdf_page *page)
{
	pdf_logpage("drop page %p\n", page);
	/* if (page->comments) pdf_dropcomment(page->comments); */
	if (page->resources)
		fz_dropobj(page->resources);
	if (page->links)
		pdf_droplink(page->links);
	if (page->tree)
		fz_droptree(page->tree);
	fz_free(page);
}

