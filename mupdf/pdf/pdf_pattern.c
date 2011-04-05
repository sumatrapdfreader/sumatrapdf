#include "fitz.h"
#include "mupdf.h"

fz_error
pdf_load_pattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_pattern *pat;
	fz_obj *obj;

	if ((*patp = pdf_find_item(xref->store, pdf_drop_pattern, dict)))
	{
		pdf_keep_pattern(*patp);
		return fz_okay;
	}

	pdf_log_rsrc("load pattern (%d %d R) {\n", fz_to_num(dict), fz_to_gen(dict));

	pat = fz_malloc(sizeof(pdf_pattern));
	pat->refs = 1;
	pat->resources = NULL;
	pat->contents = NULL;

	/* Store pattern now, to avoid possible recursion if objects refer back to this one */
	pdf_store_item(xref->store, pdf_keep_pattern, pdf_drop_pattern, dict, pat);

	pat->ismask = fz_to_int(fz_dict_gets(dict, "PaintType")) == 2;
	pat->xstep = fz_to_real(fz_dict_gets(dict, "XStep"));
	pat->ystep = fz_to_real(fz_dict_gets(dict, "YStep"));

	pdf_log_rsrc("mask %d\n", pat->ismask);
	pdf_log_rsrc("xstep %g\n", pat->xstep);
	pdf_log_rsrc("ystep %g\n", pat->ystep);

	obj = fz_dict_gets(dict, "BBox");
	pat->bbox = pdf_to_rect(obj);

	pdf_log_rsrc("bbox [%g %g %g %g]\n",
		pat->bbox.x0, pat->bbox.y0,
		pat->bbox.x1, pat->bbox.y1);

	obj = fz_dict_gets(dict, "Matrix");
	if (obj)
		pat->matrix = pdf_to_matrix(obj);
	else
		pat->matrix = fz_identity;

	pdf_log_rsrc("matrix [%g %g %g %g %g %g]\n",
		pat->matrix.a, pat->matrix.b,
		pat->matrix.c, pat->matrix.d,
		pat->matrix.e, pat->matrix.f);

	pat->resources = fz_dict_gets(dict, "Resources");
	if (pat->resources)
		fz_keep_obj(pat->resources);

	error = pdf_load_stream(&pat->contents, xref, fz_to_num(dict), fz_to_gen(dict));
	if (error)
	{
		pdf_remove_item(xref->store, pdf_drop_pattern, dict);
		pdf_drop_pattern(pat);
		return fz_rethrow(error, "cannot load pattern stream (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
	}

	pdf_log_rsrc("}\n");

	*patp = pat;
	return fz_okay;
}

pdf_pattern *
pdf_keep_pattern(pdf_pattern *pat)
{
	pat->refs ++;
	return pat;
}

void
pdf_drop_pattern(pdf_pattern *pat)
{
	if (pat && --pat->refs == 0)
	{
		if (pat->resources)
			fz_drop_obj(pat->resources);
		if (pat->contents)
			fz_drop_buffer(pat->contents);
		fz_free(pat);
	}
}
