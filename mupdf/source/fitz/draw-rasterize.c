#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <string.h>

void fz_init_aa_context(fz_context *ctx)
{
#ifndef AA_BITS
	ctx->aa.hscale = 17;
	ctx->aa.vscale = 15;
	ctx->aa.scale = 256;
	ctx->aa.bits = 8;
	ctx->aa.text_bits = 8;
#endif
}

/*
	Get the number of bits of antialiasing we are
	using (for graphics). Between 0 and 8.
*/
int
fz_aa_level(fz_context *ctx)
{
	return fz_aa_bits;
}

/*
	Get the number of bits of antialiasing we are
	using for graphics. Between 0 and 8.
*/
int
fz_graphics_aa_level(fz_context *ctx)
{
	return fz_aa_bits;
}

/*
	Get the number of bits of antialiasing we are
	using for text. Between 0 and 8.
*/
int
fz_text_aa_level(fz_context *ctx)
{
	return fz_aa_text_bits;
}

int
fz_rasterizer_graphics_aa_level(fz_rasterizer *ras)
{
	return fz_rasterizer_aa_bits(ras);
}

int
fz_rasterizer_text_aa_level(fz_rasterizer *ras)
{
	return fz_rasterizer_aa_text_bits(ras);
}

void
fz_set_rasterizer_text_aa_level(fz_context *ctx, fz_aa_context *aa, int level)
{
#ifdef AA_BITS
	if (level != fz_aa_bits)
	{
		if (fz_aa_bits == 10)
			fz_warn(ctx, "Only the Any-part-of-a-pixel rasterizer was compiled in");
		else if (fz_aa_bits == 9)
			fz_warn(ctx, "Only the Centre-of-a-pixel rasterizer was compiled in");
		else
			fz_warn(ctx, "Only the %d bit anti-aliasing rasterizer was compiled in", fz_aa_bits);
	}
#else
	if (level > 8)
		aa->text_bits = 0;
	else if (level > 6)
		aa->text_bits = 8;
	else if (level > 4)
		aa->text_bits = 6;
	else if (level > 2)
		aa->text_bits = 4;
	else if (level > 0)
		aa->text_bits = 2;
	else
		aa->text_bits = 0;
#endif
}

void
fz_set_rasterizer_graphics_aa_level(fz_context *ctx, fz_aa_context *aa, int level)
{
#ifdef AA_BITS
	if (level != fz_aa_bits)
	{
		if (fz_aa_bits == 10)
			fz_warn(ctx, "Only the Any-part-of-a-pixel rasterizer was compiled in");
		else if (fz_aa_bits == 9)
			fz_warn(ctx, "Only the Centre-of-a-pixel rasterizer was compiled in");
		else
			fz_warn(ctx, "Only the %d bit anti-aliasing rasterizer was compiled in", fz_aa_bits);
	}
#else
	if (level == 9 || level == 10)
	{
		aa->hscale = 1;
		aa->vscale = 1;
		aa->bits = level;
	}
	else if (level > 6)
	{
		aa->hscale = 17;
		aa->vscale = 15;
		aa->bits = 8;
	}
	else if (level > 4)
	{
		aa->hscale = 8;
		aa->vscale = 8;
		aa->bits = 6;
	}
	else if (level > 2)
	{
		aa->hscale = 5;
		aa->vscale = 3;
		aa->bits = 4;
	}
	else if (level > 0)
	{
		aa->hscale = 2;
		aa->vscale = 2;
		aa->bits = 2;
	}
	else
	{
		aa->hscale = 1;
		aa->vscale = 1;
		aa->bits = 0;
	}
	aa->scale = 0xFF00 / (aa->hscale * aa->vscale);
	fz_set_rasterizer_text_aa_level(ctx, aa, level);
#endif
}

/*
	Set the number of bits of antialiasing we should
	use (for both text and graphics).

	bits: The number of bits of antialiasing to use (values are clamped
	to within the 0 to 8 range).
*/
void
fz_set_aa_level(fz_context *ctx, int level)
{
	fz_set_rasterizer_graphics_aa_level(ctx, &ctx->aa, level);
	fz_set_rasterizer_text_aa_level(ctx, &ctx->aa, level);
}

/*
	Set the number of bits of antialiasing we
	should use for text.

	bits: The number of bits of antialiasing to use (values are clamped
	to within the 0 to 8 range).
*/
void
fz_set_text_aa_level(fz_context *ctx, int level)
{
	fz_set_rasterizer_text_aa_level(ctx, &ctx->aa, level);
}

/*
	Set the number of bits of antialiasing we
	should use for graphics.

	bits: The number of bits of antialiasing to use (values are clamped
	to within the 0 to 8 range).
*/
void
fz_set_graphics_aa_level(fz_context *ctx, int level)
{
	fz_set_rasterizer_graphics_aa_level(ctx, &ctx->aa, level);
}

/*
	Set the minimum line width to be
	used for stroked lines.

	min_line_width: The minimum line width to use (in pixels).
*/
void
fz_set_graphics_min_line_width(fz_context *ctx, float min_line_width)
{
	ctx->aa.min_line_width = min_line_width;
}

/*
	Get the minimum line width to be
	used for stroked lines.

	min_line_width: The minimum line width to use (in pixels).
*/
float
fz_graphics_min_line_width(fz_context *ctx)
{
	return ctx->aa.min_line_width;
}

float
fz_rasterizer_graphics_min_line_width(fz_rasterizer *ras)
{
	return ras->aa.min_line_width;
}

fz_irect
fz_bound_rasterizer(fz_context *ctx, const fz_rasterizer *rast)
{
	fz_irect bbox;
	const int hscale = fz_rasterizer_aa_hscale(rast);
	const int vscale = fz_rasterizer_aa_vscale(rast);

	if (rast->bbox.x1 < rast->bbox.x0 || rast->bbox.y1 < rast->bbox.y0)
	{
		bbox = fz_empty_irect;
	}
	else
	{
		bbox.x0 = fz_idiv(rast->bbox.x0, hscale);
		bbox.y0 = fz_idiv(rast->bbox.y0, vscale);
		bbox.x1 = fz_idiv_up(rast->bbox.x1, hscale);
		bbox.y1 = fz_idiv_up(rast->bbox.y1, vscale);
	}
	return bbox;
}

fz_rect fz_scissor_rasterizer(fz_context *ctx, const fz_rasterizer *rast)
{
	fz_rect r;
	const int hscale = fz_rasterizer_aa_hscale(rast);
	const int vscale = fz_rasterizer_aa_vscale(rast);

	r.x0 = ((float)rast->clip.x0) / hscale;
	r.y0 = ((float)rast->clip.y0) / vscale;
	r.x1 = ((float)rast->clip.x1) / hscale;
	r.y1 = ((float)rast->clip.y1) / vscale;

	return r;
}

static fz_irect fz_clip_rasterizer(fz_context *ctx, const fz_rasterizer *rast)
{
	fz_irect r;
	const int hscale = fz_rasterizer_aa_hscale(rast);
	const int vscale = fz_rasterizer_aa_vscale(rast);

	r.x0 = fz_idiv(rast->clip.x0, hscale);
	r.y0 = fz_idiv(rast->clip.y0, vscale);
	r.x1 = fz_idiv_up(rast->clip.x1, hscale);
	r.y1 = fz_idiv_up(rast->clip.y1, vscale);

	return r;
}

int fz_reset_rasterizer(fz_context *ctx, fz_rasterizer *rast, fz_irect clip)
{
	const int hscale = fz_rasterizer_aa_hscale(rast);
	const int vscale = fz_rasterizer_aa_vscale(rast);

	if (fz_is_infinite_irect(clip))
	{
		rast->clip.x0 = rast->clip.y0 = BBOX_MIN;
		rast->clip.x1 = rast->clip.y1 = BBOX_MAX;
	}
	else {
		rast->clip.x0 = clip.x0 * hscale;
		rast->clip.x1 = clip.x1 * hscale;
		rast->clip.y0 = clip.y0 * vscale;
		rast->clip.y1 = clip.y1 * vscale;
	}

	rast->bbox.x0 = rast->bbox.y0 = BBOX_MAX;
	rast->bbox.x1 = rast->bbox.y1 = BBOX_MIN;
	if (rast->fns.reset)
		return rast->fns.reset(ctx, rast);
	return 0;
}

void *fz_new_rasterizer_of_size(fz_context *ctx, int size, const fz_rasterizer_fns *fns)
{
	fz_rasterizer *rast = fz_calloc(ctx, 1, size);

	rast->fns = *fns;
	rast->clip.x0 = rast->clip.y0 = BBOX_MIN;
	rast->clip.x1 = rast->clip.y1 = BBOX_MAX;

	rast->bbox.x0 = rast->bbox.y0 = BBOX_MAX;
	rast->bbox.x1 = rast->bbox.y1 = BBOX_MIN;

	return rast;
}

fz_rasterizer *fz_new_rasterizer(fz_context *ctx, const fz_aa_context *aa)
{
	fz_rasterizer *r;
	int bits;

#ifdef AA_BITS
	bits = AA_BITS;
#else
	if (aa == NULL)
		aa = &ctx->aa;
	bits = aa->bits;
#endif
	if (bits == 10)
		r = fz_new_edgebuffer(ctx, FZ_EDGEBUFFER_ANY_PART_OF_PIXEL);
	else if (bits == 9)
		r = fz_new_edgebuffer(ctx, FZ_EDGEBUFFER_CENTER_OF_PIXEL);
	else
		r = fz_new_gel(ctx);
#ifndef AA_BITS
	r->aa = *aa;
#endif

	return r;
}

void fz_convert_rasterizer(fz_context *ctx, fz_rasterizer *r, int eofill, fz_pixmap *pix, unsigned char *colorbv, fz_overprint *eop)
{
	fz_irect clip = fz_bound_rasterizer(ctx, r);
	clip = fz_intersect_irect(clip, fz_pixmap_bbox_no_ctx(pix));
	clip = fz_intersect_irect(clip, fz_clip_rasterizer(ctx, r));
	if (!fz_is_empty_irect(clip))
		r->fns.convert(ctx, r, eofill, &clip, pix, colorbv, eop);
}
