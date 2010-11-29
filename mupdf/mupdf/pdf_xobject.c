#include "fitz.h"
#include "mupdf.h"

fz_error
pdf_loadxobject(pdf_xobject **formp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_xobject *form;
	fz_obj *obj;

	if ((*formp = pdf_finditem(xref->store, pdf_dropxobject, dict)))
	{
		pdf_keepxobject(*formp);
		return fz_okay;
	}

	form = fz_malloc(sizeof(pdf_xobject));
	form->refs = 1;
	form->resources = nil;
	form->contents = nil;
	form->colorspace = nil;

	pdf_logrsrc("load xobject (%d %d R) ptr=%p {\n", fz_tonum(dict), fz_togen(dict), form);

	/* Store item immediately, to avoid possible recursion if objects refer back to this one */
	pdf_storeitem(xref->store, pdf_keepxobject, pdf_dropxobject, dict, form);

	obj = fz_dictgets(dict, "BBox");
	form->bbox = pdf_torect(obj);

	pdf_logrsrc("bbox [%g %g %g %g]\n",
		form->bbox.x0, form->bbox.y0,
		form->bbox.x1, form->bbox.y1);

	obj = fz_dictgets(dict, "Matrix");
	if (obj)
		form->matrix = pdf_tomatrix(obj);
	else
		form->matrix = fz_identity;

	pdf_logrsrc("matrix [%g %g %g %g %g %g]\n",
		form->matrix.a, form->matrix.b,
		form->matrix.c, form->matrix.d,
		form->matrix.e, form->matrix.f);

	form->isolated = 0;
	form->knockout = 0;
	form->transparency = 0;

	obj = fz_dictgets(dict, "Group");
	if (obj)
	{
		fz_obj *attrs = obj;

		form->isolated = fz_tobool(fz_dictgets(attrs, "I"));
		form->knockout = fz_tobool(fz_dictgets(attrs, "K"));

		obj = fz_dictgets(attrs, "S");
		if (fz_isname(obj) && !strcmp(fz_toname(obj), "Transparency"))
			form->transparency = 1;

		obj = fz_dictgets(attrs, "CS");
		if (obj)
		{
			error = pdf_loadcolorspace(&form->colorspace, xref, obj);
			if (error)
				fz_catch(error, "cannot load xobject colorspace");
			pdf_logrsrc("colorspace %s\n", form->colorspace->name);
		}
	}

	pdf_logrsrc("isolated %d\n", form->isolated);
	pdf_logrsrc("knockout %d\n", form->knockout);
	pdf_logrsrc("transparency %d\n", form->transparency);

	form->resources = fz_dictgets(dict, "Resources");
	if (form->resources)
		fz_keepobj(form->resources);

	error = pdf_loadstream(&form->contents, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
	{
		pdf_removeitem(xref->store, pdf_dropxobject, dict);
		pdf_dropxobject(form);
		return fz_rethrow(error, "cannot load xobject content stream (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	pdf_logrsrc("stream %d bytes\n", form->contents->len);
	pdf_logrsrc("}\n");

	*formp = form;
	return fz_okay;
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
	if (xobj && --xobj->refs == 0)
	{
		if (xobj->colorspace)
			fz_dropcolorspace(xobj->colorspace);
		if (xobj->resources)
			fz_dropobj(xobj->resources);
		if (xobj->contents)
			fz_dropbuffer(xobj->contents);
		fz_free(xobj);
	}
}
