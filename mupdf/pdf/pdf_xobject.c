#include "fitz.h"
#include "mupdf.h"

fz_error
pdf_load_xobject(pdf_xobject **formp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_xobject *form;
	fz_obj *obj;

	if ((*formp = pdf_find_item(xref->store, pdf_drop_xobject, dict)))
	{
		pdf_keep_xobject(*formp);
		return fz_okay;
	}

	form = fz_malloc(sizeof(pdf_xobject));
	form->refs = 1;
	form->resources = NULL;
	form->contents = NULL;
	form->colorspace = NULL;

	pdf_log_rsrc("load xobject (%d %d R) ptr=%p {\n", fz_to_num(dict), fz_to_gen(dict), form);

	/* Store item immediately, to avoid possible recursion if objects refer back to this one */
	pdf_store_item(xref->store, pdf_keep_xobject, pdf_drop_xobject, dict, form);

	obj = fz_dict_gets(dict, "BBox");
	form->bbox = pdf_to_rect(obj);

	pdf_log_rsrc("bbox [%g %g %g %g]\n",
		form->bbox.x0, form->bbox.y0,
		form->bbox.x1, form->bbox.y1);

	obj = fz_dict_gets(dict, "Matrix");
	if (obj)
		form->matrix = pdf_to_matrix(obj);
	else
		form->matrix = fz_identity;

	pdf_log_rsrc("matrix [%g %g %g %g %g %g]\n",
		form->matrix.a, form->matrix.b,
		form->matrix.c, form->matrix.d,
		form->matrix.e, form->matrix.f);

	form->isolated = 0;
	form->knockout = 0;
	form->transparency = 0;

	obj = fz_dict_gets(dict, "Group");
	if (obj)
	{
		fz_obj *attrs = obj;

		form->isolated = fz_to_bool(fz_dict_gets(attrs, "I"));
		form->knockout = fz_to_bool(fz_dict_gets(attrs, "K"));

		obj = fz_dict_gets(attrs, "S");
		if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "Transparency"))
			form->transparency = 1;

		obj = fz_dict_gets(attrs, "CS");
		if (obj)
		{
			error = pdf_load_colorspace(&form->colorspace, xref, obj);
			if (error)
				fz_catch(error, "cannot load xobject colorspace");
			pdf_log_rsrc("colorspace %s\n", form->colorspace->name);
		}
	}

	pdf_log_rsrc("isolated %d\n", form->isolated);
	pdf_log_rsrc("knockout %d\n", form->knockout);
	pdf_log_rsrc("transparency %d\n", form->transparency);

	form->resources = fz_dict_gets(dict, "Resources");
	if (form->resources)
		fz_keep_obj(form->resources);

	error = pdf_load_stream(&form->contents, xref, fz_to_num(dict), fz_to_gen(dict));
	if (error)
	{
		pdf_remove_item(xref->store, pdf_drop_xobject, dict);
		pdf_drop_xobject(form);
		return fz_rethrow(error, "cannot load xobject content stream (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
	}

	pdf_log_rsrc("stream %d bytes\n", form->contents->len);
	pdf_log_rsrc("}\n");

	*formp = form;
	return fz_okay;
}

pdf_xobject *
pdf_keep_xobject(pdf_xobject *xobj)
{
	xobj->refs ++;
	return xobj;
}

void
pdf_drop_xobject(pdf_xobject *xobj)
{
	if (xobj && --xobj->refs == 0)
	{
		if (xobj->colorspace)
			fz_drop_colorspace(xobj->colorspace);
		if (xobj->resources)
			fz_drop_obj(xobj->resources);
		if (xobj->contents)
			fz_drop_buffer(xobj->contents);
		fz_free(xobj);
	}
}
