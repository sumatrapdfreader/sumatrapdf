#include "fitz.h"
#include "mupdf.h"

pdf_pattern *
pdf_keep_pattern(pdf_pattern *pat)
{
	return (pdf_pattern *)fz_keep_storable(&pat->storable);
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
		fz_drop_obj(pat->resources);
	if (pat->contents)
		fz_drop_buffer(ctx, pat->contents);
	fz_free(ctx, pat);
}

static unsigned int
pdf_pattern_size(pdf_pattern *pat)
{
	if (pat == NULL)
		return 0;
	return sizeof(*pat) + (pat->contents ? pat->contents->cap : 0);
}

pdf_pattern *
pdf_load_pattern(pdf_xref *xref, fz_obj *dict)
{
	pdf_pattern *pat;
	fz_obj *obj;
	fz_context *ctx = xref->ctx;

	if ((pat = fz_find_item(ctx, pdf_free_pattern_imp, dict)))
	{
		return pat;
	}

	pat = fz_malloc_struct(ctx, pdf_pattern);
	FZ_INIT_STORABLE(pat, 1, pdf_free_pattern_imp);
	pat->resources = NULL;
	pat->contents = NULL;

	/* Store pattern now, to avoid possible recursion if objects refer back to this one */
	fz_store_item(ctx, dict, pat, pdf_pattern_size(pat));

	pat->ismask = fz_to_int(fz_dict_gets(dict, "PaintType")) == 2;
	pat->xstep = fz_to_real(fz_dict_gets(dict, "XStep"));
	pat->ystep = fz_to_real(fz_dict_gets(dict, "YStep"));

	obj = fz_dict_gets(dict, "BBox");
	pat->bbox = pdf_to_rect(ctx, obj);

	obj = fz_dict_gets(dict, "Matrix");
	if (obj)
		pat->matrix = pdf_to_matrix(ctx, obj);
	else
		pat->matrix = fz_identity;

	pat->resources = fz_dict_gets(dict, "Resources");
	if (pat->resources)
		fz_keep_obj(pat->resources);

	fz_try(ctx)
	{
		pat->contents = pdf_load_stream(xref, fz_to_num(dict), fz_to_gen(dict));
	}
	fz_catch(ctx)
	{
		fz_remove_item(ctx, pdf_free_pattern_imp, dict);
		pdf_drop_pattern(ctx, pat);
		fz_throw(ctx, "cannot load pattern stream (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
	}
	return pat;
}
