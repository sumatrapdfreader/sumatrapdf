#include "fitz.h"
#include "mupdf.h"

pdf_pattern *
pdf_keeppattern(pdf_pattern *pat)
{
	pat->refs ++;
	return pat;
}

void
pdf_droppattern(pdf_pattern *pat)
{
	if (pat && --pat->refs == 0)
	{
		if (pat->tree)
			fz_droptree(pat->tree);
		fz_free(pat);
	}
}

fz_error
pdf_loadpattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_pattern *pat;
	fz_stream *stm;
	fz_obj *resources;
	fz_obj *obj;
	pdf_csi *csi;

	if ((*patp = pdf_finditem(xref->store, PDF_KPATTERN, dict)))
	{
		pdf_keeppattern(*patp);
		return fz_okay;
	}

	pdf_logrsrc("load pattern (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

	pat = fz_malloc(sizeof(pdf_pattern));
	pat->refs = 1;
	pat->tree = nil;
	pat->ismask = fz_toint(fz_dictgets(dict, "PaintType")) == 2;
	pat->xstep = fz_toreal(fz_dictgets(dict, "XStep"));
	pat->ystep = fz_toreal(fz_dictgets(dict, "YStep"));

	pdf_logrsrc("mask %d\n", pat->ismask);
	pdf_logrsrc("xstep %g\n", pat->xstep);
	pdf_logrsrc("ystep %g\n", pat->ystep);

	obj = fz_dictgets(dict, "BBox");
	pat->bbox = pdf_torect(obj);

	pdf_logrsrc("bbox [%g %g %g %g]\n",
		pat->bbox.x0, pat->bbox.y0,
		pat->bbox.x1, pat->bbox.y1);

	obj = fz_dictgets(dict, "Matrix");
	if (obj)
		pat->matrix = pdf_tomatrix(obj);
	else
		pat->matrix = fz_identity();

	pdf_logrsrc("matrix [%g %g %g %g %g %g]\n",
		pat->matrix.a, pat->matrix.b,
		pat->matrix.c, pat->matrix.d,
		pat->matrix.e, pat->matrix.f);

	/* Store pattern now, to avoid possible recursion if objects refer back to this one */
	pdf_storeitem(xref->store, PDF_KPATTERN, dict, pat);

	/*
	 * Locate resources
	 */

	resources = fz_dictgets(dict, "Resources");
	if (!resources)
	{
		error = fz_throw("cannot find Resources dictionary");
		goto cleanup;
	}

	/*
	 * Content stream
	 */

	pdf_logrsrc("content stream\n");

	error = pdf_newcsi(&csi, pat->ismask);
	if (error)
	{
		error = fz_rethrow(error, "cannot create interpreter");
		goto cleanup;
	}

	error = pdf_openstream(&stm, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
	{
		pdf_dropcsi(csi);
		error = fz_rethrow(error, "cannot open pattern stream (%d %d R)", fz_tonum(dict), fz_togen(dict));
		goto cleanup;
	}

	error = pdf_runcsi(csi, xref, resources, stm);
	if (error)
	{
		fz_dropstream(stm);
		pdf_dropcsi(csi);
		error = fz_rethrow(error, "cannot interpret pattern stream (%d %d R)", fz_tonum(dict), fz_togen(dict));
		goto cleanup;
	}

	/*
	 *  Move display list to pattern struct
	 */

	pat->tree = csi->tree;
	csi->tree = nil;

	fz_dropstream(stm);
	pdf_dropcsi(csi);

	pdf_logrsrc("}\n");

	*patp = pat;
	return fz_okay;

cleanup:
	pdf_removeitem(xref->store, PDF_KPATTERN, dict);
	pdf_droppattern(pat);
	return error; /* already rethrown */
}

