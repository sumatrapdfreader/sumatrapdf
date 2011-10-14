#include "fitz.h"

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

#define STACK_SIZE 96

/* Enable the following to attempt to support knockout and/or isolated
 * blending groups. This code is known to give incorrect results currently
 * so disabled by default. See bug 692377. */
#undef ATTEMPT_KNOCKOUT_AND_ISOLATED

/* Enable the following to help debug group blending. */
#undef DUMP_GROUP_BLENDS

/* Note #1: At various points in this code (notably when clipping with non
 * rectangular masks), we create a new (empty) destination pixmap. We then
 * render this pixmap, then plot it back into the original destination
 * through a mask. This works well for normal blending, but falls down for
 * non-zero blending modes; effectively we are forcing ourselves to use an
 * isolated group.
 *
 * The fix for this would be to copy the contents from the underlying dest
 * into the newly created dest. This would enable us to use a non
 * FZ_BLEND_ISOLATED blendmode. Unfortunately, tt would break tiling, as
 * we could no longer render once and blend back multiple times.
 */


typedef struct fz_draw_device_s fz_draw_device;

enum {
	FZ_DRAWDEV_FLAGS_TYPE3 = 1,
};

struct fz_draw_device_s
{
	fz_glyph_cache *cache;
	fz_gel *gel;

	fz_pixmap *dest;
	fz_pixmap *shape;
	fz_bbox scissor;

	int flags;
	int top;
	int blendmode;
	struct {
		fz_bbox scissor;
		fz_pixmap *dest;
		fz_pixmap *mask;
		fz_pixmap *shape;
		int blendmode;
		int luminosity;
		float alpha;
		fz_matrix ctm;
		float xstep, ystep;
		fz_rect area;
	} stack[STACK_SIZE];
};

#ifdef DUMP_GROUP_BLENDS
static int group_dump_count = 0;

static void fz_dump_blend(fz_pixmap *pix, const char *s)
{
	char name[80];

	if (pix == NULL)
		return;

	sprintf(name, "dump%02d.png", group_dump_count);
	if (s)
		printf("%s%02d", s, group_dump_count);
	group_dump_count++;

	fz_write_png(pix, name, (pix->n > 1));
}

static void dump_spaces(int x, const char *s)
{
	int i;
	for (i = 0; i < x; i++)
		printf(" ");
	printf("%s", s);
}

#endif

static void fz_knockout_begin(void *user)
{
	fz_draw_device *dev = user;
	fz_bbox bbox;
	fz_pixmap *dest, *shape;
	int isolated = dev->blendmode & FZ_BLEND_ISOLATED;

	if ((dev->blendmode & FZ_BLEND_KNOCKOUT) == 0)
		return;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_bound_pixmap(dev->dest);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(dev->dest->colorspace, bbox);

	if (isolated)
	{
		fz_clear_pixmap(dest);
	}
	else
	{
		fz_pixmap *prev;
		int i  = dev->top;
		do
			prev = dev->stack[--i].dest;
		while (prev == NULL);
		fz_copy_pixmap_rect(dest, prev, bbox);
	}

	if (dev->blendmode == 0 && isolated || 1 /* SumatraPDF: disable crashy shape code */)
	{
		/* We can render direct to any existing shape plane. If there
		 * isn't one, we don't need to make one. */
		shape = dev->shape;
	}
	else
	{
		shape = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(shape);
	}
	dev->stack[dev->top].blendmode = dev->blendmode;
	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Knockout begin\n");
#endif
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
	dev->blendmode &= ~FZ_BLEND_MODEMASK;
}

static void fz_knockout_end(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *group = dev->dest;
	fz_pixmap *shape = dev->shape;
	int blendmode;
	int isolated;

	if ((dev->blendmode & FZ_BLEND_KNOCKOUT) == 0)
		return;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (dev->top > 0)
	{
		dev->top--;
		blendmode = dev->blendmode & FZ_BLEND_MODEMASK;
		isolated = dev->blendmode & FZ_BLEND_ISOLATED;
		dev->blendmode = dev->stack[dev->top].blendmode;
		dev->shape = dev->stack[dev->top].shape;
		dev->dest = dev->stack[dev->top].dest;
		dev->scissor = dev->stack[dev->top].scissor;

#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "");
		fz_dump_blend(group, "Blending ");
		if (shape)
			fz_dump_blend(shape, "/");
		fz_dump_blend(dev->dest, " onto ");
		if (dev->shape)
			fz_dump_blend(dev->shape, "/");
		if (blendmode != 0)
			printf(" (blend %d)", blendmode);
		if (isolated != 0)
			printf(" (isolated)");
		printf(" (knockout)");
#endif
		if ((blendmode == 0) && (shape == NULL))
			fz_paint_pixmap(dev->dest, group, 255);
		else
			fz_blend_pixmap(dev->dest, group, 255, blendmode, isolated, shape);

		fz_drop_pixmap(group);
		if (shape != dev->shape)
		{
			if (dev->shape)
			{
				fz_paint_pixmap(dev->shape, shape, 255);
			}
			fz_drop_pixmap(shape);
		}
#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(dev->dest, " to get ");
		if (dev->shape)
			fz_dump_blend(dev->shape, "/");
		printf("\n");
#endif
	}
}

static void
fz_draw_fill_path(void *user, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	float colorfv[FZ_MAX_COLORS];
	fz_bbox bbox;
	int i;

	fz_reset_gel(dev->gel, dev->scissor);
	fz_flatten_fill_path(dev->gel, path, ctm, flatness);
	fz_sort_gel(dev->gel);

	bbox = fz_bound_gel(dev->gel);
	bbox = fz_intersect_bbox(bbox, dev->scissor);

	if (fz_is_empty_rect(bbox))
		return;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_scan_convert(dev->gel, even_odd, bbox, dev->dest, colorbv);
	if (dev->shape)
	{
		fz_reset_gel(dev->gel, dev->scissor);
		fz_flatten_fill_path(dev->gel, path, ctm, flatness);
		fz_sort_gel(dev->gel);

		colorbv[0] = alpha * 255;
		fz_scan_convert(dev->gel, even_odd, bbox, dev->shape, colorbv);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	float linewidth = stroke->linewidth;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	float colorfv[FZ_MAX_COLORS];
	fz_bbox bbox;
	int i;

	if (linewidth * expansion < 0.1f)
		linewidth = 1 / expansion;

	fz_reset_gel(dev->gel, dev->scissor);
	if (stroke->dash_len > 0)
		fz_flatten_dash_path(dev->gel, path, stroke, ctm, flatness, linewidth);
	else
		fz_flatten_stroke_path(dev->gel, path, stroke, ctm, flatness, linewidth);
	fz_sort_gel(dev->gel);

	bbox = fz_bound_gel(dev->gel);
	bbox = fz_intersect_bbox(bbox, dev->scissor);

	if (fz_is_empty_rect(bbox))
		return;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_scan_convert(dev->gel, 0, bbox, dev->dest, colorbv);
	if (dev->shape)
	{
		fz_reset_gel(dev->gel, dev->scissor);
		if (stroke->dash_len > 0)
			fz_flatten_dash_path(dev->gel, path, stroke, ctm, flatness, linewidth);
		else
			fz_flatten_stroke_path(dev->gel, path, stroke, ctm, flatness, linewidth);
		fz_sort_gel(dev->gel);

		colorbv[0] = 255;
		fz_scan_convert(dev->gel, 0, bbox, dev->shape, colorbv);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_clip_path(void *user, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	fz_pixmap *mask, *dest, *shape;
	fz_bbox bbox;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	fz_reset_gel(dev->gel, dev->scissor);
	fz_flatten_fill_path(dev->gel, path, ctm, flatness);
	fz_sort_gel(dev->gel);

	bbox = fz_bound_gel(dev->gel);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	if (rect)
		bbox = fz_intersect_bbox(bbox, fz_round_rect(*rect));

	if (fz_is_empty_rect(bbox) || fz_is_rect_gel(dev->gel))
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = NULL;
		dev->stack[dev->top].dest = NULL;
		dev->stack[dev->top].shape = dev->shape;
		dev->stack[dev->top].blendmode = dev->blendmode;
		dev->scissor = bbox;
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "Clip (rectangular) begin\n");
#endif
		dev->top++;
		return;
	}

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(shape);
	}
	else
		shape = NULL;

	fz_scan_convert(dev->gel, even_odd, bbox, mask, NULL);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
	/* FIXME: See note #1 */
	dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Clip (non-rectangular) begin\n");
#endif
	dev->top++;
}

static void
fz_draw_clip_stroke_path(void *user, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	float linewidth = stroke->linewidth;
	fz_pixmap *mask, *dest, *shape;
	fz_bbox bbox;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (linewidth * expansion < 0.1f)
		linewidth = 1 / expansion;

	fz_reset_gel(dev->gel, dev->scissor);
	if (stroke->dash_len > 0)
		fz_flatten_dash_path(dev->gel, path, stroke, ctm, flatness, linewidth);
	else
		fz_flatten_stroke_path(dev->gel, path, stroke, ctm, flatness, linewidth);
	fz_sort_gel(dev->gel);

	bbox = fz_bound_gel(dev->gel);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	if (rect)
		bbox = fz_intersect_bbox(bbox, fz_round_rect(*rect));

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(shape);
	}
	else
		shape = NULL;

	if (!fz_is_empty_rect(bbox))
		fz_scan_convert(dev->gel, 0, bbox, mask, NULL);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
	/* FIXME: See note #1 */
	dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Clip (stroke) begin\n");
#endif
	dev->top++;
}

static void
draw_glyph(unsigned char *colorbv, fz_pixmap *dst, fz_pixmap *msk,
	int xorig, int yorig, fz_bbox scissor)
{
	unsigned char *dp, *mp;
	fz_bbox bbox;
	int x, y, w, h;

	bbox = fz_bound_pixmap(msk);
	bbox.x0 += xorig;
	bbox.y0 += yorig;
	bbox.x1 += xorig;
	bbox.y1 += yorig;

	bbox = fz_intersect_bbox(bbox, scissor); /* scissor < dst */
	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	mp = msk->samples + ((y - msk->y - yorig) * msk->w + (x - msk->x - xorig));
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	assert(msk->n == 1);

	while (h--)
	{
		if (dst->colorspace)
			fz_paint_span_with_color(dp, mp, dst->n, w, colorbv);
		else
			fz_paint_span(dp, mp, 1, w, 255);
		dp += dst->w * dst->n;
		mp += msk->w;
	}
}

static void
fz_draw_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	unsigned char shapebv;
	float colorfv[FZ_MAX_COLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;
	shapebv = 255;

	tm = text->trm;

	for (i = 0; i < text->len; i++)
	{
		gid = text->items[i].gid;
		if (gid < 0)
			continue;

		tm.e = text->items[i].x;
		tm.f = text->items[i].y;
		trm = fz_concat(tm, ctm);
		x = floorf(trm.e);
		y = floorf(trm.f);
		trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
		trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

		glyph = fz_render_glyph(dev->cache, text->font, gid, trm, model);
		if (glyph)
		{
			if (glyph->n == 1)
			{
				draw_glyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
				if (dev->shape)
					draw_glyph(&shapebv, dev->shape, glyph, x, y, dev->scissor);
			}
			else
			{
				fz_matrix ctm = {glyph->w, 0.0, 0.0, -glyph->h, x + glyph->x, y + glyph->y + glyph->h};
				fz_paint_image(dev->dest, dev->scissor, dev->shape, glyph, ctm, alpha * 255);
			}
			fz_drop_pixmap(glyph);
		}
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	float colorfv[FZ_MAX_COLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	tm = text->trm;

	for (i = 0; i < text->len; i++)
	{
		gid = text->items[i].gid;
		if (gid < 0)
			continue;

		tm.e = text->items[i].x;
		tm.f = text->items[i].y;
		trm = fz_concat(tm, ctm);
		x = floorf(trm.e);
		y = floorf(trm.f);
		trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
		trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

		glyph = fz_render_stroked_glyph(dev->cache, text->font, gid, trm, ctm, stroke);
		if (glyph)
		{
			draw_glyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			if (dev->shape)
				draw_glyph(colorbv, dev->shape, glyph, x, y, dev->scissor);
			fz_drop_pixmap(glyph);
		}
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest, *shape;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	/* If accumulate == 0 then this text object is guaranteed complete */
	/* If accumulate == 1 then this text object is the first (or only) in a sequence */
	/* If accumulate == 2 then this text object is a continuation */

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (accumulate == 0)
	{
		/* make the mask the exact size needed */
		bbox = fz_round_rect(fz_bound_text(text, ctm));
		bbox = fz_intersect_bbox(bbox, dev->scissor);
	}
	else
	{
		/* be conservative about the size of the mask needed */
		bbox = dev->scissor;
	}

	if (accumulate == 0 || accumulate == 1)
	{
		mask = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(mask);
		dest = fz_new_pixmap_with_rect(model, bbox);
		/* FIXME: See note #1 */
		fz_clear_pixmap(dest);
		if (dev->shape)
		{
			shape = fz_new_pixmap_with_rect(NULL, bbox);
			fz_clear_pixmap(shape);
		}
		else
			shape = NULL;

		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = mask;
		dev->stack[dev->top].dest = dev->dest;
		dev->stack[dev->top].shape = dev->shape;
		/* FIXME: See note #1 */
		dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
		dev->scissor = bbox;
		dev->dest = dest;
		dev->shape = shape;
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "Clip (text) begin\n");
#endif
		dev->top++;
	}
	else
	{
		mask = dev->stack[dev->top-1].mask;
	}

	if (!fz_is_empty_rect(bbox))
	{
		tm = text->trm;

		for (i = 0; i < text->len; i++)
		{
			gid = text->items[i].gid;
			if (gid < 0)
				continue;

			tm.e = text->items[i].x;
			tm.f = text->items[i].y;
			trm = fz_concat(tm, ctm);
			x = floorf(trm.e);
			y = floorf(trm.f);
			trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
			trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

			glyph = fz_render_glyph(dev->cache, text->font, gid, trm, model);
			if (glyph)
			{
				draw_glyph(NULL, mask, glyph, x, y, bbox);
				if (dev->shape)
					draw_glyph(NULL, dev->shape, glyph, x, y, bbox);
				fz_drop_pixmap(glyph);
			}
		}
	}
}

static void
fz_draw_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest, *shape;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	/* make the mask the exact size needed */
	bbox = fz_round_rect(fz_bound_text(text, ctm));
	bbox = fz_intersect_bbox(bbox, dev->scissor);

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(shape);
	}
	else
		shape = dev->shape;

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
	/* FIXME: See note #1 */
	dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Clip (stroke text) begin\n");
#endif
	dev->top++;

	if (!fz_is_empty_rect(bbox))
	{
		tm = text->trm;

		for (i = 0; i < text->len; i++)
		{
			gid = text->items[i].gid;
			if (gid < 0)
				continue;

			tm.e = text->items[i].x;
			tm.f = text->items[i].y;
			trm = fz_concat(tm, ctm);
			x = floorf(trm.e);
			y = floorf(trm.f);
			trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
			trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

			glyph = fz_render_stroked_glyph(dev->cache, text->font, gid, trm, ctm, stroke);
			if (glyph)
			{
				draw_glyph(NULL, mask, glyph, x, y, bbox);
				if (dev->shape)
					draw_glyph(NULL, dev->shape, glyph, x, y, bbox);
				fz_drop_pixmap(glyph);
			}
		}
	}
}

static void
fz_draw_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
}

static void
fz_draw_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *dest = dev->dest;
	fz_rect bounds;
	fz_bbox bbox, scissor;
	float colorfv[FZ_MAX_COLORS];
	unsigned char colorbv[FZ_MAX_COLORS + 1];

	bounds = fz_bound_shade(shade, ctm);
	bbox = fz_intersect_bbox(fz_round_rect(bounds), dev->scissor);
	scissor = dev->scissor;

	// TODO: proper clip by shade->bbox

	if (fz_is_empty_rect(bbox))
		return;

	if (!model)
	{
		fz_warn("cannot render shading directly to an alpha mask");
		return;
	}

	if (alpha < 1)
	{
		dest = fz_new_pixmap_with_rect(dev->dest->colorspace, bbox);
		fz_clear_pixmap(dest);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	if (shade->use_background)
	{
		unsigned char *s;
		int x, y, n, i;
		fz_convert_color(shade->colorspace, shade->background, model, colorfv);
		for (i = 0; i < model->n; i++)
			colorbv[i] = colorfv[i] * 255;
		colorbv[i] = 255;

		n = dest->n;
		for (y = scissor.y0; y < scissor.y1; y++)
		{
			s = dest->samples + ((scissor.x0 - dest->x) + (y - dest->y) * dest->w) * dest->n;
			for (x = scissor.x0; x < scissor.x1; x++)
			{
				for (i = 0; i < n; i++)
					*s++ = colorbv[i];
			}
		}
		if (dev->shape)
		{
			for (y = scissor.y0; y < scissor.y1; y++)
			{
				s = dev->shape->samples + (scissor.x0 - dev->shape->x) + (y - dev->shape->y) * dev->shape->w;
				for (x = scissor.x0; x < scissor.x1; x++)
				{
					*s++ = 255;
				}
			}
		}
	}

	fz_paint_shade(shade, ctm, dest, bbox);
	if (dev->shape)
		fz_clear_pixmap_rect_with_color(dev->shape, 255, bbox);

	if (alpha < 1)
	{
		fz_paint_pixmap(dev->dest, dest, alpha * 255);
		fz_drop_pixmap(dest);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static fz_pixmap *
fz_transform_pixmap(fz_pixmap *image, fz_matrix *ctm, int x, int y, int dx, int dy, int gridfit)
{
	fz_pixmap *scaled;

	if (ctm->a != 0 && ctm->b == 0 && ctm->c == 0 && ctm->d != 0)
	{
		/* Unrotated or X-flip or Y-flip or XY-flip */
		scaled = fz_scale_pixmap_gridfit(image, ctm->e, ctm->f, ctm->a, ctm->d, gridfit);
		if (scaled == NULL)
			return NULL;
		ctm->a = scaled->w;
		ctm->d = scaled->h;
		ctm->e = scaled->x;
		ctm->f = scaled->y;
		return scaled;
	}

	if (ctm->a == 0 && ctm->b != 0 && ctm->c != 0 && ctm->d == 0)
	{
		/* Other orthogonal flip/rotation cases */
		scaled = fz_scale_pixmap_gridfit(image, ctm->f, ctm->e, ctm->b, ctm->c, gridfit);
		if (scaled == NULL)
			return NULL;
		ctm->b = scaled->w;
		ctm->c = scaled->h;
		ctm->f = scaled->x;
		ctm->e = scaled->y;
		return scaled;
	}

	/* Downscale, non rectilinear case */
	if (dx > 0 && dy > 0)
	{
		scaled = fz_scale_pixmap(image, 0, 0, (float)dx, (float)dy);
		return scaled;
	}

	return NULL;
}

static void
fz_draw_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *converted = NULL;
	fz_pixmap *scaled = NULL;
	int after;
	int dx, dy;

	if (!model)
	{
		fz_warn("cannot render image directly to an alpha mask");
		return;
	}

	if (image->w == 0 || image->h == 0)
		return;

	/* convert images with more components (cmyk->rgb) before scaling */
	/* convert images with fewer components (gray->rgb after scaling */
	/* convert images with expensive colorspace transforms after scaling */

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	after = 0;
	if (image->colorspace == fz_device_gray)
		after = 1;

	if (image->colorspace != model && !after)
	{
		converted = fz_new_pixmap_with_rect(model, fz_bound_pixmap(image));
		fz_convert_pixmap(image, converted);
		image = converted;
	}

	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		int gridfit = alpha == 1.0f && !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
		scaled = fz_transform_pixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy, gridfit);
		if (scaled == NULL)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_scale_pixmap(image, image->x, image->y, dx, dy);
		}
		if (scaled != NULL)
			image = scaled;
	}

	if (image->colorspace != model)
	{
		if ((image->colorspace == fz_device_gray && model == fz_device_rgb) ||
			(image->colorspace == fz_device_gray && model == fz_device_bgr))
		{
			/* We have special case rendering code for gray -> rgb/bgr */
		}
		else
		{
			converted = fz_new_pixmap_with_rect(model, fz_bound_pixmap(image));
			fz_convert_pixmap(image, converted);
			image = converted;
		}
	}

	fz_paint_image(dev->dest, dev->scissor, dev->shape, image, ctm, alpha * 255);

	if (scaled)
		fz_drop_pixmap(scaled);
	if (converted)
		fz_drop_pixmap(converted);

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	float colorfv[FZ_MAX_COLORS];
	fz_pixmap *scaled = NULL;
	int dx, dy;
	int i;

	if (image->w == 0 || image->h == 0)
		return;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		int gridfit = alpha == 1.0f && !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
		scaled = fz_transform_pixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy, gridfit);
		if (scaled == NULL)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_scale_pixmap(image, image->x, image->y, dx, dy);
		}
		if (scaled != NULL)
			image = scaled;
	}

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_paint_image_with_color(dev->dest, dev->scissor, dev->shape, image, ctm, colorbv);

	if (scaled)
		fz_drop_pixmap(scaled);

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);
}

static void
fz_draw_clip_image_mask(void *user, fz_pixmap *image, fz_rect *rect, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest, *shape;
	fz_pixmap *scaled = NULL;
	int dx, dy;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Clip (image mask) begin\n");
#endif

	if (image->w == 0 || image->h == 0)
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = NULL;
		dev->stack[dev->top].dest = NULL;
		dev->stack[dev->top].blendmode = dev->blendmode;
		dev->scissor = fz_empty_bbox;
		dev->top++;
		return;
	}

	bbox = fz_round_rect(fz_transform_rect(ctm, fz_unit_rect));
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	if (rect)
		bbox = fz_intersect_bbox(bbox, fz_round_rect(*rect));

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(shape);
	}
	else
		shape = NULL;

	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		int gridfit = !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
		scaled = fz_transform_pixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy, gridfit);
		if (scaled == NULL)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_scale_pixmap(image, image->x, image->y, dx, dy);
		}
		if (scaled != NULL)
			image = scaled;
	}

	fz_paint_image(mask, bbox, dev->shape, image, ctm, 255);

	if (scaled)
		fz_drop_pixmap(scaled);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
	/* FIXME: See note #1 */
	dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
	dev->top++;
}

static void
fz_draw_pop_clip(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *mask, *dest, *shape;
	if (dev->top > 0)
	{
		dev->top--;
		dev->scissor = dev->stack[dev->top].scissor;
		mask = dev->stack[dev->top].mask;
		dest = dev->stack[dev->top].dest;
		shape = dev->stack[dev->top].shape;
		dev->blendmode = dev->stack[dev->top].blendmode;

		/* We can get here with mask == NULL if the clipping actually
		 * resolved to a rectangle earlier. In this case, we will
		 * have a dest, and the shape will be unchanged.
		 */
		if (mask)
		{
			assert(dest);

#ifdef DUMP_GROUP_BLENDS
			dump_spaces(dev->top, "");
			fz_dump_blend(dev->dest, "Clipping ");
			if (dev->shape)
				fz_dump_blend(dev->shape, "/");
			fz_dump_blend(dest, " onto ");
			if (shape)
				fz_dump_blend(shape, "/");
			fz_dump_blend(mask, " with ");
#endif
			fz_paint_pixmap_with_mask(dest, dev->dest, mask);
			if (shape != NULL)
			{
				assert(shape != dev->shape);
				fz_paint_pixmap_with_mask(shape, dev->shape, mask);
				fz_drop_pixmap(dev->shape);
				dev->shape = shape;
			}
			fz_drop_pixmap(mask);
			fz_drop_pixmap(dev->dest);
			dev->dest = dest;
#ifdef DUMP_GROUP_BLENDS
			fz_dump_blend(dev->dest, " to get ");
			if (dev->shape)
				fz_dump_blend(dev->shape, "/");
			printf("\n");
#endif
		}
		else
		{
#ifdef DUMP_GROUP_BLENDS
			dump_spaces(dev->top, "Clip End\n");
#endif
			assert(dest == NULL);
			assert(shape == dev->shape);
		}
	}
}

static void
fz_draw_begin_mask(void *user, fz_rect rect, int luminosity, fz_colorspace *colorspace, float *colorfv)
{
	fz_draw_device *dev = user;
	fz_pixmap *dest;
	fz_pixmap *shape = dev->shape;
	fz_bbox bbox;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_round_rect(rect);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(fz_device_gray, bbox);
	if (dev->shape)
	{
		/* FIXME: If we ever want to support AIS true, then we
		 * probably want to create a shape pixmap here, using:
		 *     shape = fz_new_pixmap_with_rect(NULL, bbox);
		 * then, in the end_mask code, we create the mask from this
		 * rather than dest.
		 */
		shape = NULL;
	}

	if (luminosity)
	{
		float bc;
		if (!colorspace)
			colorspace = fz_device_gray;
		fz_convert_color(colorspace, colorfv, fz_device_gray, &bc);
		fz_clear_pixmap_with_color(dest, bc * 255);
		if (shape)
			fz_clear_pixmap_with_color(shape, 255);
	}
	else
	{
		fz_clear_pixmap(dest);
		if (shape)
			fz_clear_pixmap(shape);
	}

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].luminosity = luminosity;
	dev->stack[dev->top].shape = dev->shape;
	dev->stack[dev->top].blendmode = dev->blendmode;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Mask begin\n");
#endif
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
}

static void
fz_draw_end_mask(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *mask = dev->dest;
	fz_pixmap *maskshape = dev->shape;
	fz_pixmap *temp, *dest;
	fz_bbox bbox;
	int luminosity;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (dev->top > 0)
	{
		/* pop soft mask buffer */
		dev->top--;
		luminosity = dev->stack[dev->top].luminosity;
		dev->scissor = dev->stack[dev->top].scissor;
		dev->dest = dev->stack[dev->top].dest;
		dev->shape = dev->stack[dev->top].shape;

		/* convert to alpha mask */
		temp = fz_alpha_from_gray(mask, luminosity);
		fz_drop_pixmap(mask);
		fz_drop_pixmap(maskshape);

		/* create new dest scratch buffer */
		bbox = fz_bound_pixmap(temp);
		dest = fz_new_pixmap_with_rect(dev->dest->colorspace, bbox);
		/* FIXME: See note #1 */
		fz_clear_pixmap(dest);

		/* push soft mask as clip mask */
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = temp;
		dev->stack[dev->top].dest = dev->dest;
		/* FIXME: See note #1 */
		dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
		/* If we have a shape, then it'll need to be masked with the
		 * clip mask when we pop. So create a new shape now. */
		if (dev->shape)
		{
			dev->stack[dev->top].shape = dev->shape;
			dev->shape = fz_new_pixmap_with_rect(NULL, bbox);
			fz_clear_pixmap(dev->shape);
		}
		dev->scissor = bbox;
		dev->dest = dest;
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "Mask -> Clip\n");
#endif
		dev->top++;
	}
}

static void
fz_draw_begin_group(void *user, fz_rect rect, int isolated, int knockout, int blendmode, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *dest, *shape;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	bbox = fz_round_rect(rect);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(model, bbox);

#ifndef ATTEMPT_KNOCKOUT_AND_ISOLATED
	knockout = 0;
	isolated = 1;
#endif

	if (isolated)
	{
		fz_clear_pixmap(dest);
	}
	else
	{
		fz_copy_pixmap_rect(dest, dev->dest, bbox);
	}

	if (blendmode == 0 && alpha == 1.0 && isolated || 1 /* SumatraPDF: disable crashy shape code */)
	{
		/* We can render direct to any existing shape plane. If there
		 * isn't one, we don't need to make one. */
		shape = dev->shape;
	}
	else
	{
		shape = fz_new_pixmap_with_rect(NULL, bbox);
		fz_clear_pixmap(shape);
	}

	dev->stack[dev->top].alpha = alpha;
	dev->stack[dev->top].blendmode = dev->blendmode;
	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Group Begin\n");
#endif
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
	dev->shape = shape;
	dev->blendmode = blendmode | (isolated ? FZ_BLEND_ISOLATED : 0) | (knockout ? FZ_BLEND_KNOCKOUT : 0);
}

static void
fz_draw_end_group(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *group = dev->dest;
	fz_pixmap *shape = dev->shape;
	int blendmode;
	int isolated;
	float alpha;

	if (dev->top > 0)
	{
		dev->top--;
		alpha = dev->stack[dev->top].alpha;
		blendmode = dev->blendmode & FZ_BLEND_MODEMASK;
		isolated = dev->blendmode & FZ_BLEND_ISOLATED;
		dev->blendmode = dev->stack[dev->top].blendmode;
		dev->shape = dev->stack[dev->top].shape;
		dev->dest = dev->stack[dev->top].dest;
		dev->scissor = dev->stack[dev->top].scissor;

#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "");
		fz_dump_blend(group, "Blending ");
		if (shape)
			fz_dump_blend(shape, "/");
		fz_dump_blend(dev->dest, " onto ");
		if (dev->shape)
			fz_dump_blend(dev->shape, "/");
		if (alpha != 1.0f)
			printf(" (alpha %g)", alpha);
		if (blendmode != 0)
			printf(" (blend %d)", blendmode);
		if (isolated != 0)
			printf(" (isolated)");
		if (blendmode & FZ_BLEND_KNOCKOUT)
			printf(" (knockout)");
#endif
		if ((blendmode == 0) && (shape == NULL))
			fz_paint_pixmap(dev->dest, group, alpha * 255);
		else
			fz_blend_pixmap(dev->dest, group, alpha * 255, blendmode, isolated, shape);

		fz_drop_pixmap(group);
		if (shape != dev->shape)
		{
			if (dev->shape)
			{
				fz_paint_pixmap(dev->shape, shape, alpha * 255);
			}
			fz_drop_pixmap(shape);
		}
#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(dev->dest, " to get ");
		if (dev->shape)
			fz_dump_blend(dev->shape, "/");
		printf("\n");
#endif
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_begin_tile(void *user, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *dest;
	fz_bbox bbox;

	/* area, view, xstep, ystep are in pattern space */
	/* ctm maps from pattern space to device space */

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	bbox = fz_round_rect(fz_transform_rect(ctm, view));
	/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=692418 */
	dest = fz_new_pixmap_with_limit(model, bbox.x1 - bbox.x0, bbox.y1 - bbox.y0);
	if (dest)
	{
		dest->x = bbox.x0;
		dest->y = bbox.y0;
	}
	else
	{
		bbox.x1 = bbox.x0;
		bbox.y1 = bbox.y0;
		dest = fz_new_pixmap_with_rect(model, bbox);
	}
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].shape = dev->shape;
	/* FIXME: See note #1 */
	dev->stack[dev->top].blendmode = dev->blendmode | FZ_BLEND_ISOLATED;
	dev->stack[dev->top].xstep = xstep;
	dev->stack[dev->top].ystep = ystep;
	dev->stack[dev->top].area = area;
	dev->stack[dev->top].ctm = ctm;
#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "Tile begin\n");
#endif
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
}

static void
fz_draw_end_tile(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *tile = dev->dest;
	float xstep, ystep;
	fz_matrix ctm, ttm;
	fz_rect area;
	int x0, y0, x1, y1, x, y;

	if (dev->top > 0)
	{
		dev->top--;
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "Tile end\n");
#endif
		xstep = dev->stack[dev->top].xstep;
		ystep = dev->stack[dev->top].ystep;
		area = dev->stack[dev->top].area;
		ctm = dev->stack[dev->top].ctm;
		dev->scissor = dev->stack[dev->top].scissor;
		dev->dest = dev->stack[dev->top].dest;
		dev->blendmode = dev->stack[dev->top].blendmode;


		x0 = floorf(area.x0 / xstep);
		y0 = floorf(area.y0 / ystep);
		x1 = ceilf(area.x1 / xstep);
		y1 = ceilf(area.y1 / ystep);

		ctm.e = tile->x;
		ctm.f = tile->y;

		for (y = y0; y < y1; y++)
		{
			for (x = x0; x < x1; x++)
			{
				ttm = fz_concat(fz_translate(x * xstep, y * ystep), ctm);
				tile->x = ttm.e;
				tile->y = ttm.f;
				fz_paint_pixmap_with_rect(dev->dest, tile, 255, dev->scissor);
			}
		}

		fz_drop_pixmap(tile);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);
}

static void
fz_draw_free_user(void *user)
{
	fz_draw_device *dev = user;
	/* TODO: pop and free the stacks */
	if (dev->top > 0)
		fz_warn("items left on stack in draw device: %d", dev->top);
	fz_free_gel(dev->gel);
	fz_free(dev);
}

fz_device *
fz_new_draw_device(fz_glyph_cache *cache, fz_pixmap *dest)
{
	fz_device *dev;
	fz_draw_device *ddev = fz_malloc(sizeof(fz_draw_device));
	ddev->cache = cache;
	ddev->gel = fz_new_gel();
	ddev->dest = dest;
	ddev->shape = NULL;
	ddev->top = 0;
	ddev->blendmode = 0;
	ddev->flags = 0;

	ddev->scissor.x0 = dest->x;
	ddev->scissor.y0 = dest->y;
	ddev->scissor.x1 = dest->x + dest->w;
	ddev->scissor.y1 = dest->y + dest->h;

	dev = fz_new_device(ddev);
	dev->free_user = fz_draw_free_user;

	dev->fill_path = fz_draw_fill_path;
	dev->stroke_path = fz_draw_stroke_path;
	dev->clip_path = fz_draw_clip_path;
	dev->clip_stroke_path = fz_draw_clip_stroke_path;

	dev->fill_text = fz_draw_fill_text;
	dev->stroke_text = fz_draw_stroke_text;
	dev->clip_text = fz_draw_clip_text;
	dev->clip_stroke_text = fz_draw_clip_stroke_text;
	dev->ignore_text = fz_draw_ignore_text;

	dev->fill_image_mask = fz_draw_fill_image_mask;
	dev->clip_image_mask = fz_draw_clip_image_mask;
	dev->fill_image = fz_draw_fill_image;
	dev->fill_shade = fz_draw_fill_shade;

	dev->pop_clip = fz_draw_pop_clip;

	dev->begin_mask = fz_draw_begin_mask;
	dev->end_mask = fz_draw_end_mask;
	dev->begin_group = fz_draw_begin_group;
	dev->end_group = fz_draw_end_group;

	dev->begin_tile = fz_draw_begin_tile;
	dev->end_tile = fz_draw_end_tile;

	return dev;
}

fz_device *
fz_new_draw_device_type3(fz_glyph_cache *cache, fz_pixmap *dest)
{
	fz_device *dev = fz_new_draw_device(cache, dest);
	fz_draw_device *ddev = dev->user;
	ddev->flags |= FZ_DRAWDEV_FLAGS_TYPE3;
	return dev;
}
