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

	/* Store item immediately, to avoid possible recursion if objects refer back to this one */
	pdf_store_item(xref->store, pdf_keep_xobject, pdf_drop_xobject, dict, form);

	obj = fz_dict_gets(dict, "BBox");
	form->bbox = pdf_to_rect(obj);

	obj = fz_dict_gets(dict, "Matrix");
	if (obj)
		form->matrix = pdf_to_matrix(obj);
	else
		form->matrix = fz_identity;

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
		}
	}

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
