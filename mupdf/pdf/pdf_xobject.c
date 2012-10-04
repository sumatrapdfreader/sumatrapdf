#include "fitz-internal.h"
#include "mupdf-internal.h"

pdf_xobject *
pdf_keep_xobject(fz_context *ctx, pdf_xobject *xobj)
{
	return (pdf_xobject *)fz_keep_storable(ctx, &xobj->storable);
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
	pdf_drop_obj(xobj->resources);
	pdf_drop_obj(xobj->contents);
	pdf_drop_obj(xobj->me);
	fz_free(ctx, xobj);
}

static unsigned int
pdf_xobject_size(pdf_xobject *xobj)
{
	if (xobj == NULL)
		return 0;
	return sizeof(*xobj) + (xobj->colorspace ? xobj->colorspace->size : 0);
}

pdf_xobject *
pdf_load_xobject(pdf_document *xref, pdf_obj *dict)
{
	pdf_xobject *form;
	pdf_obj *obj;
	fz_context *ctx = xref->ctx;

	if ((form = pdf_find_item(ctx, pdf_free_xobject_imp, dict)))
	{
		return form;
	}

	form = fz_malloc_struct(ctx, pdf_xobject);
	FZ_INIT_STORABLE(form, 1, pdf_free_xobject_imp);
	form->resources = NULL;
	form->contents = NULL;
	form->colorspace = NULL;
	form->me = NULL;

	/* Store item immediately, to avoid possible recursion if objects refer back to this one */
	pdf_store_item(ctx, dict, form, pdf_xobject_size(form));

	obj = pdf_dict_gets(dict, "BBox");
	form->bbox = pdf_to_rect(ctx, obj);

	obj = pdf_dict_gets(dict, "Matrix");
	if (obj)
		form->matrix = pdf_to_matrix(ctx, obj);
	else
		form->matrix = fz_identity;

	form->isolated = 0;
	form->knockout = 0;
	form->transparency = 0;

	obj = pdf_dict_gets(dict, "Group");
	if (obj)
	{
		pdf_obj *attrs = obj;

		form->isolated = pdf_to_bool(pdf_dict_gets(attrs, "I"));
		form->knockout = pdf_to_bool(pdf_dict_gets(attrs, "K"));

		obj = pdf_dict_gets(attrs, "S");
		if (pdf_is_name(obj) && !strcmp(pdf_to_name(obj), "Transparency"))
			form->transparency = 1;

		obj = pdf_dict_gets(attrs, "CS");
		if (obj)
		{
			/* SumatraPDF: fix memory leak */
			fz_try(ctx)
			{
			form->colorspace = pdf_load_colorspace(xref, obj);
			if (!form->colorspace)
				fz_throw(ctx, "cannot load xobject colorspace");
			}
			fz_catch(ctx)
			{
				pdf_drop_xobject(ctx, form);
				fz_rethrow(ctx);
			}
		}
	}

	form->resources = pdf_dict_gets(dict, "Resources");
	if (form->resources)
		pdf_keep_obj(form->resources);

	fz_try(ctx)
	{
		form->contents = pdf_keep_obj(dict);
	}
	fz_catch(ctx)
	{
		pdf_remove_item(ctx, pdf_free_xobject_imp, dict);
		pdf_drop_xobject(ctx, form);
		fz_throw(ctx, "cannot load xobject content stream (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict));
	}
	form->me = pdf_keep_obj(dict);

	return form;
}

pdf_obj *
pdf_new_xobject(pdf_document *xref, fz_rect *bbox, fz_matrix *mat)
{
	int idict_num;
	pdf_obj *idict = NULL;
	pdf_obj *dict = NULL;
	pdf_xobject *form = NULL;
	pdf_obj *obj = NULL;
	pdf_obj *res = NULL;
	pdf_obj *procset = NULL;
	fz_context *ctx = xref->ctx;

	fz_var(idict);
	fz_var(dict);
	fz_var(form);
	fz_var(obj);
	fz_var(res);
	fz_var(procset);
	fz_try(ctx)
	{
		dict = pdf_new_dict(ctx, 0);

		obj = pdf_new_rect(ctx, bbox);
		pdf_dict_puts(dict, "BBox", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		obj = pdf_new_int(ctx, 1);
		pdf_dict_puts(dict, "FormType", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		obj = pdf_new_int(ctx, 0);
		pdf_dict_puts(dict, "Length", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		obj = pdf_new_matrix(ctx, mat);
		pdf_dict_puts(dict, "Matrix", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		res = pdf_new_dict(ctx, 0);
		procset = pdf_new_array(ctx, 2);
		obj = pdf_new_name(ctx, "PDF");
		pdf_array_push(procset, obj);
		pdf_drop_obj(obj);
		obj = NULL;
		obj = pdf_new_name(ctx, "Text");
		pdf_array_push(procset, obj);
		pdf_drop_obj(obj);
		obj = NULL;
		pdf_dict_puts(res, "ProcSet", procset);
		pdf_drop_obj(procset);
		procset = NULL;
		pdf_dict_puts(dict, "Resources", res);

		obj = pdf_new_name(ctx, "Form");
		pdf_dict_puts(dict, "Subtype", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		obj = pdf_new_name(ctx, "XObject");
		pdf_dict_puts(dict, "Type", obj);
		pdf_drop_obj(obj);
		obj = NULL;

		form = fz_malloc_struct(ctx, pdf_xobject);
		FZ_INIT_STORABLE(form, 1, pdf_free_xobject_imp);
		form->resources = NULL;
		form->contents = NULL;
		form->colorspace = NULL;
		form->me = NULL;

		form->bbox = *bbox;

		form->matrix = *mat;

		form->isolated = 0;
		form->knockout = 0;
		form->transparency = 0;

		form->resources = res;
		res = NULL;

		idict_num = pdf_create_object(xref);
		pdf_update_object(xref, idict_num, dict);
		idict = pdf_new_indirect(ctx, idict_num, 0, xref);
		pdf_drop_obj(dict);
		dict = NULL;

		pdf_store_item(ctx, idict, form, pdf_xobject_size(form));

		form->contents = pdf_keep_obj(idict);
		form->me = pdf_keep_obj(idict);

		pdf_drop_xobject(ctx, form);
		form = NULL;
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(procset);
		pdf_drop_obj(res);
		pdf_drop_obj(obj);
		pdf_drop_obj(dict);
		pdf_drop_obj(idict);
		pdf_drop_xobject(ctx, form);
		fz_throw(ctx, "failed to create xobject)");
	}

	return idict;
}

void pdf_update_xobject_contents(pdf_document *xref, pdf_xobject *form, fz_buffer *buffer)
{
	fz_context *ctx = xref->ctx;
	pdf_obj *len = NULL;

	fz_var(len);

	fz_try(ctx)
	{
		len = pdf_new_int(ctx, buffer->len);
		pdf_dict_dels(form->contents, "Filter");
		pdf_dict_puts(form->contents, "Length", len);
		pdf_update_stream(xref, pdf_to_num(form->contents), buffer);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(len);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/* SumatraPDF: allow to synthesize XObjects (cf. pdf_create_annot) */
pdf_xobject *
pdf_create_xobject(fz_context *ctx, pdf_obj *dict)
{
	pdf_xobject *form = fz_malloc_struct(ctx, pdf_xobject);

	FZ_INIT_STORABLE(form, 1, pdf_free_xobject_imp);
	form->matrix = fz_identity;
	form->me = pdf_keep_obj(dict);

	return form;
}
