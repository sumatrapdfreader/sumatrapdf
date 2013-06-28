#include "mupdf/pdf.h"

pdf_pattern *
pdf_keep_pattern(fz_context *ctx, pdf_pattern *pat)
{
	return (pdf_pattern *)fz_keep_storable(ctx, &pat->storable);
}

void
pdf_drop_pattern(fz_context *ctx, pdf_pattern *pat)
{
	fz_drop_storable(ctx, &pat->storable);
}

static void
pdf_free_pattern_imp(fz_context *ctx, fz_storable *pat_)
{
	pdf_pattern *pat = (pdf_pattern *)pat_;

	if (pat->resources)
		pdf_drop_obj(pat->resources);
	if (pat->contents)
		pdf_drop_obj(pat->contents);
	fz_free(ctx, pat);
}

static unsigned int
pdf_pattern_size(pdf_pattern *pat)
{
	if (pat == NULL)
		return 0;
	return sizeof(*pat);
}

pdf_pattern *
pdf_load_pattern(pdf_document *doc, pdf_obj *dict)
{
	pdf_pattern *pat;
	pdf_obj *obj;
	fz_context *ctx = doc->ctx;

	if ((pat = pdf_find_item(ctx, pdf_free_pattern_imp, dict)))
	{
		return pat;
	}

	pat = fz_malloc_struct(ctx, pdf_pattern);
	FZ_INIT_STORABLE(pat, 1, pdf_free_pattern_imp);
	pat->resources = NULL;
	pat->contents = NULL;

	/* Store pattern now, to avoid possible recursion if objects refer back to this one */
	pdf_store_item(ctx, dict, pat, pdf_pattern_size(pat));

	pat->ismask = pdf_to_int(pdf_dict_gets(dict, "PaintType")) == 2;
	pat->xstep = pdf_to_real(pdf_dict_gets(dict, "XStep"));
	pat->ystep = pdf_to_real(pdf_dict_gets(dict, "YStep"));

	obj = pdf_dict_gets(dict, "BBox");
	pdf_to_rect(ctx, obj, &pat->bbox);

	obj = pdf_dict_gets(dict, "Matrix");
	if (obj)
		pdf_to_matrix(ctx, obj, &pat->matrix);
	else
		pat->matrix = fz_identity;

	pat->resources = pdf_dict_gets(dict, "Resources");
	if (pat->resources)
		pdf_keep_obj(pat->resources);

	fz_try(ctx)
	{
		pat->contents = pdf_keep_obj(dict);
	}
	fz_catch(ctx)
	{
		pdf_remove_item(ctx, pdf_free_pattern_imp, dict);
		pdf_drop_pattern(ctx, pat);
		fz_rethrow_message(ctx, "cannot load pattern stream (%d %d R)", pdf_to_num(dict), pdf_to_gen(dict));
	}
	return pat;
}
