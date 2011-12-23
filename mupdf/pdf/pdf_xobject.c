#include "fitz.h"
#include "mupdf.h"

pdf_xobject *
pdf_keep_xobject(pdf_xobject *xobj)
{
	return (pdf_xobject *)fz_keep_storable(&xobj->storable);
}

void
pdf_drop_xobject(fz_context *ctx, pdf_xobject *xobj)
{
	fz_drop_storable(ctx, &xobj->storable);
}

static void
pdf_free_xobject_imp(fz_context *ctx, fz_storable *xobj_)
{
	pdf_xobject *xobj = (pdf_xobject *)xobj_;

	if (xobj->colorspace)
		fz_drop_colorspace(ctx, xobj->colorspace);
	if (xobj->resources)
		fz_drop_obj(xobj->resources);
	if (xobj->contents)
		fz_drop_buffer(ctx, xobj->contents);
	fz_free(ctx, xobj);
}

static unsigned int
pdf_xobject_size(pdf_xobject *xobj)
{
	if (xobj == NULL)
		return 0;
	return sizeof(*xobj) + (xobj->colorspace ? xobj->colorspace->size : 0) + (xobj->contents ? xobj->contents->len : 0);
}

pdf_xobject *
pdf_load_xobject(pdf_xref *xref, fz_obj *dict)
{
	pdf_xobject *form;
	fz_obj *obj;
	fz_context *ctx = xref->ctx;

	if ((form = fz_find_item(ctx, pdf_free_xobject_imp, dict)))
	{
		return form;
	}

	form = fz_malloc_struct(ctx, pdf_xobject);
	FZ_INIT_STORABLE(form, 1, pdf_free_xobject_imp);
	form->resources = NULL;
	form->contents = NULL;
	form->colorspace = NULL;

	/* Store item immediately, to avoid possible recursion if objects refer back to this one */
	fz_store_item(ctx, dict, form, pdf_xobject_size(form));

	obj = fz_dict_gets(dict, "BBox");
	form->bbox = pdf_to_rect(ctx, obj);

	obj = fz_dict_gets(dict, "Matrix");
	if (obj)
		form->matrix = pdf_to_matrix(ctx, obj);
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
			form->colorspace = pdf_load_colorspace(xref, obj);
			if (!form->colorspace)
				fz_throw(ctx, "cannot load xobject colorspace");
		}
	}

	form->resources = fz_dict_gets(dict, "Resources");
	if (form->resources)
		fz_keep_obj(form->resources);

	fz_try(ctx)
	{
		form->contents = pdf_load_stream(xref, fz_to_num(dict), fz_to_gen(dict));
	}
	fz_catch(ctx)
	{
		fz_remove_item(ctx, pdf_free_xobject_imp, dict);
		pdf_drop_xobject(ctx, form);
		fz_throw(ctx, "cannot load xobject content stream (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
	}

	return form;
}
