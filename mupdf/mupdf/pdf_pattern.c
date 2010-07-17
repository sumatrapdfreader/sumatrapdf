#include "fitz.h"
#include "mupdf.h"

fz_error
pdf_loadpattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_pattern *pat;
	fz_obj *obj;

	if ((*patp = pdf_finditem(xref->store, pdf_droppattern, dict)))
	{
		pdf_keeppattern(*patp);
		return fz_okay;
	}

	pdf_logrsrc("load pattern (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

	pat = fz_malloc(sizeof(pdf_pattern));
	pat->refs = 1;
	pat->resources = nil;
	pat->contents = nil;

	/* Store pattern now, to avoid possible recursion if objects refer back to this one */
	pdf_storeitem(xref->store, pdf_keeppattern, pdf_droppattern, dict, pat);

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
		pat->matrix = fz_identity;

	pdf_logrsrc("matrix [%g %g %g %g %g %g]\n",
		pat->matrix.a, pat->matrix.b,
		pat->matrix.c, pat->matrix.d,
		pat->matrix.e, pat->matrix.f);

	pat->resources = fz_dictgets(dict, "Resources");
	if (pat->resources)
		fz_keepobj(pat->resources);

	error = pdf_loadstream(&pat->contents, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
	{
		pdf_removeitem(xref->store, pdf_droppattern, dict);
		pdf_droppattern(pat);
		return fz_rethrow(error, "cannot load pattern stream (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	pdf_logrsrc("}\n");

	*patp = pat;
	return fz_okay;
}

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
		if (pat->resources)
			fz_dropobj(pat->resources);
		if (pat->contents)
			fz_dropbuffer(pat->contents);
		fz_free(pat);
	}
}
