#include "fitz.h"
#include "mupdf.h"

fz_error *
pdf_loadxobject(pdf_xobject **formp, pdf_xref *xref, fz_obj *dict, fz_obj *ref)
{
	fz_error *error;
	pdf_xobject *form;
	fz_obj *obj;

	if ((*formp = pdf_finditem(xref->store, PDF_KXOBJECT, ref)))
	{
		pdf_keepxobject(*formp);
		return fz_okay;
	}

	form = fz_malloc(sizeof(pdf_xobject));
	if (!form)
		return fz_throw("outofmem: xobject struct");

	form->refs = 1;
	form->resources = nil;
	form->contents = nil;

	/* Store item immediately, to avoid infinite recursion if contained
	   objects refer again to this xobject */
	error = pdf_storeitem(xref->store, PDF_KXOBJECT, ref, form);
	if (error)
	{
		pdf_dropxobject(form);
		return fz_rethrow(error, "cannot store xobject resource");
	}

	pdf_logrsrc("load xobject (%d %d R) ptr=%p {\n", fz_tonum(ref), fz_togen(ref), form);

	obj = fz_dictgets(dict, "BBox");
	form->bbox = pdf_torect(obj);

	pdf_logrsrc("bbox [%g %g %g %g]\n",
			form->bbox.x0, form->bbox.y0,
			form->bbox.x1, form->bbox.y1);

	obj = fz_dictgets(dict, "Matrix");
	if (obj)
		form->matrix = pdf_tomatrix(obj);
	else
		form->matrix = fz_identity();

	pdf_logrsrc("matrix [%g %g %g %g %g %g]\n",
			form->matrix.a, form->matrix.b,
			form->matrix.c, form->matrix.d,
			form->matrix.e, form->matrix.f);

	obj = fz_dictgets(dict, "I");
	form->isolated = fz_tobool(obj);
	obj = fz_dictgets(dict, "K");
	form->knockout = fz_tobool(obj);

	pdf_logrsrc("isolated %d\n", form->isolated);
	pdf_logrsrc("knockout %d\n", form->knockout);

	obj = fz_dictgets(dict, "Resources");
	if (obj)
	{
		error = pdf_resolve(&obj, xref);
		if (error)
		{
			fz_dropobj(obj);
			error = fz_rethrow(error, "cannot resolve xobject resources");
			goto cleanup;
		}
		error = pdf_loadresources(&form->resources, xref, obj);
		fz_dropobj(obj);
		if (error)
		{
			error = fz_rethrow(error, "cannot load xobject resources");
			goto cleanup;
		}
	}

	error = pdf_loadstream(&form->contents, xref, fz_tonum(ref), fz_togen(ref));
	if (error)
	{
		error = fz_rethrow(error, "cannot load xobject content stream");
		goto cleanup;
	}

	pdf_logrsrc("stream %d bytes\n", form->contents->wp - form->contents->rp);

	pdf_logrsrc("}\n");

	*formp = form;
	return fz_okay;
cleanup:
	pdf_removeitem(xref->store, PDF_KXOBJECT, ref);
	pdf_dropxobject(form);
	return error;
}

pdf_xobject *
pdf_keepxobject(pdf_xobject *xobj)
{
	xobj->refs ++;
	return xobj;
}

void
pdf_dropxobject(pdf_xobject *xobj)
{
	if (--xobj->refs == 0)
	{
		if (xobj->contents) fz_dropbuffer(xobj->contents);
		if (xobj->resources) fz_dropobj(xobj->resources);
		fz_free(xobj);
	}
}

