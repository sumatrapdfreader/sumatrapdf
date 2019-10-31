#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

pdf_obj *
pdf_xobject_resources(fz_context *ctx, pdf_obj *xobj)
{
	return pdf_dict_get(ctx, xobj, PDF_NAME(Resources));
}

fz_rect
pdf_xobject_bbox(fz_context *ctx, pdf_obj *xobj)
{
	return pdf_dict_get_rect(ctx, xobj, PDF_NAME(BBox));
}

fz_matrix
pdf_xobject_matrix(fz_context *ctx, pdf_obj *xobj)
{
	return pdf_dict_get_matrix(ctx, xobj, PDF_NAME(Matrix));
}

int pdf_xobject_isolated(fz_context *ctx, pdf_obj *xobj)
{
	pdf_obj *group = pdf_dict_get(ctx, xobj, PDF_NAME(Group));
	if (group)
		return pdf_dict_get_bool(ctx, group, PDF_NAME(I));
	return 0;
}

int pdf_xobject_knockout(fz_context *ctx, pdf_obj *xobj)
{
	pdf_obj *group = pdf_dict_get(ctx, xobj, PDF_NAME(Group));
	if (group)
		return pdf_dict_get_bool(ctx, group, PDF_NAME(K));
	return 0;
}

int pdf_xobject_transparency(fz_context *ctx, pdf_obj *xobj)
{
	pdf_obj *group = pdf_dict_get(ctx, xobj, PDF_NAME(Group));
	if (group)
		if (pdf_name_eq(ctx, pdf_dict_get(ctx, group, PDF_NAME(S)), PDF_NAME(Transparency)))
			return 1;
	return 0;
}

fz_colorspace *
pdf_xobject_colorspace(fz_context *ctx, pdf_obj *xobj)
{
	pdf_obj *group = pdf_dict_get(ctx, xobj, PDF_NAME(Group));
	if (group)
	{
		pdf_obj *cs = pdf_dict_get(ctx, group, PDF_NAME(CS));
		if (cs)
		{
			fz_colorspace *colorspace = NULL;
			fz_try(ctx)
				colorspace = pdf_load_colorspace(ctx, cs);
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "Ignoring XObject blending colorspace.");
			}
			if (!fz_is_valid_blend_colorspace(ctx, colorspace))
			{
				fz_warn(ctx, "Ignoring invalid XObject blending colorspace: %s.", colorspace->name);
				fz_drop_colorspace(ctx, colorspace);
				return NULL;
			}
			return colorspace;
		}
	}
	return NULL;
}

pdf_obj *
pdf_new_xobject(fz_context *ctx, pdf_document *doc, fz_rect bbox, fz_matrix matrix, pdf_obj *res, fz_buffer *contents)
{
	pdf_obj *ind = NULL;
	pdf_obj *form = pdf_new_dict(ctx, doc, 5);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, form, PDF_NAME(Type), PDF_NAME(XObject));
		pdf_dict_put(ctx, form, PDF_NAME(Subtype), PDF_NAME(Form));
		pdf_dict_put_rect(ctx, form, PDF_NAME(BBox), bbox);
		pdf_dict_put_matrix(ctx, form, PDF_NAME(Matrix), matrix);
		if (res)
			pdf_dict_put(ctx, form, PDF_NAME(Resources), res);
		ind = pdf_add_stream(ctx, doc, contents, form, 0);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, form);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return ind;
}

void
pdf_update_xobject(fz_context *ctx, pdf_document *doc, pdf_obj *form, fz_rect bbox, fz_matrix matrix, pdf_obj *res, fz_buffer *contents)
{
	pdf_dict_put_rect(ctx, form, PDF_NAME(BBox), bbox);
	pdf_dict_put_matrix(ctx, form, PDF_NAME(Matrix), matrix);
	if (res)
		pdf_dict_put(ctx, form, PDF_NAME(Resources), res);
	else
		pdf_dict_del(ctx, form, PDF_NAME(Resources));
	pdf_update_stream(ctx, doc, form, contents, 0);
}
