#include "fitz.h"

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

#define STACK_SIZE 96

/* Enable the following to attempt to support knockout and/or isolated
 * blending groups. */
#define ATTEMPT_KNOCKOUT_AND_ISOLATED

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

typedef struct fz_draw_stack_s fz_draw_stack;

struct fz_draw_stack_s {
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
};

struct fz_draw_device_s
{
	fz_glyph_cache *cache;
	fz_gel *gel;
	fz_context *ctx;

	fz_pixmap *dest;
	fz_pixmap *shape;
	fz_bbox scissor;

	int flags;
	int top;
	int blendmode;
	fz_draw_stack *stack;
	int stack_max;
	fz_draw_stack init_stack[STACK_SIZE];
};

#ifdef DUMP_GROUP_BLENDS
static int group_dump_count = 0;

static void fz_dump_blend(fz_context *ctx, fz_pixmap *pix, const char *s)
{
	char name[80];

	if (!pix)
		return;

	sprintf(name, "dump%02d.png", group_dump_count);
	if (s)
		printf("%s%02d", s, group_dump_count);
	group_dump_count++;

	fz_write_png(ctx, pix, name, (pix->n > 1));
}

static void dump_spaces(int x, const char *s)
{
	int i;
	for (i = 0; i < x; i++)
		printf(" ");
	printf("%s", s);
}

#endif

static void fz_grow_stack(fz_draw_device *dev)
{
	int max = dev->stack_max * 2;
	fz_draw_stack *stack;

	if (dev->stack == &dev->init_stack[0])
	{
		stack = fz_malloc(dev->ctx, sizeof(*stack) * max);
		memcpy(stack, dev->stack, sizeof(*stack) * dev->stack_max);
	}
	else
	{
		stack = fz_resize_array(dev->ctx, dev->stack, max, sizeof(*stack));
	}
	dev->stack = stack;
	dev->stack_max = max;
}

static void fz_knockout_begin(fz_draw_device *dev)
{
	fz_bbox bbox;
	fz_pixmap *dest, *shape;
	int isolated = dev->blendmode & FZ_BLEND_ISOLATED;

	if ((dev->blendmode & FZ_BLEND_KNOCKOUT) == 0)
		return;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

	bbox = fz_bound_pixmap(dev->dest);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(dev->ctx, dev->dest->colorspace, bbox);

	if (isolated)
	{
		fz_clear_pixmap(dest);
	}
	else
	{
		fz_pixmap *prev;
		int i = dev->top;
		do
			prev = dev->stack[--i].dest;
		while (!prev);
		fz_copy_pixmap_rect(dest, prev, bbox);
	}

	if (dev->blendmode == 0 && isolated)
	{
		/* We can render direct to any existing shape plane. If there
		 * isn't one, we don't need to make one. */
		shape = dev->shape;
	}
	else
	{
		shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
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

static void fz_knockout_end(fz_draw_device *dev)
{
	fz_pixmap *group = dev->dest;
	fz_pixmap *shape = dev->shape;
	int blendmode;
	int isolated;

	if ((dev->blendmode & FZ_BLEND_KNOCKOUT) == 0)
		return;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

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
		fz_dump_blend(dev->ctx, group, "Blending ");
		if (shape)
			fz_dump_blend(dev->ctx, shape, "/");
		fz_dump_blend(dev->ctx, dev->dest, " onto ");
		if (dev->shape)
			fz_dump_blend(dev->ctx, dev->shape, "/");
		if (blendmode != 0)
			printf(" (blend %d)", blendmode);
		if (isolated != 0)
			printf(" (isolated)");
		printf(" (knockout)");
#endif
		if ((blendmode == 0) && (!shape))
			fz_paint_pixmap(dev->dest, group, 255);
		else
			fz_blend_pixmap(dev->dest, group, 255, blendmode, isolated, shape);

		fz_drop_pixmap(dev->ctx, group);
		if (shape != dev->shape)
		{
			if (dev->shape)
			{
				fz_paint_pixmap(dev->shape, shape, 255);
			}
			fz_drop_pixmap(dev->ctx, shape);
		}
#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(dev->ctx, dev->dest, " to get ");
		if (dev->shape)
			fz_dump_blend(dev->ctx, dev->shape, "/");
		printf("\n");
#endif
	}
}

static void
fz_draw_fill_path(fz_device *devp, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = devp->user;
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

	fz_convert_color(dev->ctx, colorspace, color, model, colorfv);
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
fz_draw_stroke_path(fz_device *devp, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = devp->user;
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

	fz_convert_color(dev->ctx, colorspace, color, model, colorfv);
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
fz_draw_clip_path(fz_device *devp, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	fz_pixmap *mask, *dest, *shape;
	fz_bbox bbox;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

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

	mask = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(dev->ctx, model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
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
fz_draw_clip_stroke_path(fz_device *devp, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	float linewidth = stroke->linewidth;
	fz_pixmap *mask, *dest, *shape;
	fz_bbox bbox;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

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

	mask = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(dev->ctx, model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
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
fz_draw_fill_text(fz_device *devp, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	unsigned char shapebv;
	float colorfv[FZ_MAX_COLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	fz_convert_color(dev->ctx, colorspace, color, model, colorfv);
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

		glyph = fz_render_glyph(dev->ctx, dev->cache, text->font, gid, trm, model);
		if (glyph)
		{
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1746 */
			if (glyph->n > 1 && text->font->t3procs)
			{
				float light;
				fz_convert_color(dev->ctx, colorspace, color, fz_device_gray, &light);
				if (light != 0)
				{
					fz_pixmap *gray = fz_new_pixmap_with_rect(dev->ctx, fz_device_gray, fz_bound_pixmap(glyph));
					fz_convert_pixmap(dev->ctx, glyph, gray);
					fz_drop_pixmap(dev->ctx, glyph);
					glyph = fz_alpha_from_gray(dev->ctx, gray, 0);
					fz_drop_pixmap(dev->ctx, gray);
				}
			}
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
			fz_drop_pixmap(dev->ctx, glyph);
		}
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_stroke_text(fz_device *devp, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	float colorfv[FZ_MAX_COLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	fz_convert_color(dev->ctx, colorspace, color, model, colorfv);
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

		glyph = fz_render_stroked_glyph(dev->ctx, dev->cache, text->font, gid, trm, ctm, stroke);
		if (glyph)
		{
			draw_glyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			if (dev->shape)
				draw_glyph(colorbv, dev->shape, glyph, x, y, dev->scissor);
			fz_drop_pixmap(dev->ctx, glyph);
		}
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_clip_text(fz_device *devp, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest, *shape;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	/* If accumulate == 0 then this text object is guaranteed complete */
	/* If accumulate == 1 then this text object is the first (or only) in a sequence */
	/* If accumulate == 2 then this text object is a continuation */

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

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
		mask = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
		fz_clear_pixmap(mask);
		dest = fz_new_pixmap_with_rect(dev->ctx, model, bbox);
		/* FIXME: See note #1 */
		fz_clear_pixmap(dest);
		if (dev->shape)
		{
			shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
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

			glyph = fz_render_glyph(dev->ctx, dev->cache, text->font, gid, trm, model);
			if (glyph)
			{
				draw_glyph(NULL, mask, glyph, x, y, bbox);
				if (dev->shape)
					draw_glyph(NULL, dev->shape, glyph, x, y, bbox);
				fz_drop_pixmap(dev->ctx, glyph);
			}
		}
	}
}

static void
fz_draw_clip_stroke_text(fz_device *devp, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest, *shape;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

	/* make the mask the exact size needed */
	bbox = fz_round_rect(fz_bound_text(text, ctm));
	bbox = fz_intersect_bbox(bbox, dev->scissor);

	mask = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
	fz_clear_pixmap(mask);
	dest = fz_new_pixmap_with_rect(dev->ctx, model, bbox);
	/* FIXME: See note #1 */
	fz_clear_pixmap(dest);
	if (dev->shape)
	{
		shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
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

			glyph = fz_render_stroked_glyph(dev->ctx, dev->cache, text->font, gid, trm, ctm, stroke);
			if (glyph)
			{
				draw_glyph(NULL, mask, glyph, x, y, bbox);
				if (dev->shape)
					draw_glyph(NULL, dev->shape, glyph, x, y, bbox);
				fz_drop_pixmap(dev->ctx, glyph);
			}
		}
	}
}

static void
fz_draw_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm)
{
}

static void
fz_draw_fill_shade(fz_device *devp, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_draw_device *dev = devp->user;
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
		fz_warn(dev->ctx, "cannot render shading directly to an alpha mask");
		return;
	}

	if (alpha < 1)
	{
		dest = fz_new_pixmap_with_rect(dev->ctx, dev->dest->colorspace, bbox);
		fz_clear_pixmap(dest);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	if (shade->use_background)
	{
		unsigned char *s;
		int x, y, n, i;
		fz_convert_color(dev->ctx, shade->colorspace, shade->background, model, colorfv);
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

	fz_paint_shade(dev->ctx, shade, ctm, dest, bbox);
	if (dev->shape)
		fz_clear_pixmap_rect_with_color(dev->shape, 255, bbox);

	if (alpha < 1)
	{
		fz_paint_pixmap(dev->dest, dest, alpha * 255);
		fz_drop_pixmap(dev->ctx, dest);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static fz_pixmap *
fz_transform_pixmap(fz_context *ctx, fz_pixmap *image, fz_matrix *ctm, int x, int y, int dx, int dy, int gridfit)
{
	fz_pixmap *scaled;

	if (ctm->a != 0 && ctm->b == 0 && ctm->c == 0 && ctm->d != 0)
	{
		/* Unrotated or X-flip or Y-flip or XY-flip */
		fz_matrix m = *ctm;
		if (gridfit)
			fz_gridfit_matrix(&m);
		scaled = fz_scale_pixmap(ctx, image, m.e, m.f, m.a, m.d);
		if (!scaled)
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
		fz_matrix m = *ctm;
		if (gridfit)
			fz_gridfit_matrix(&m);
		scaled = fz_scale_pixmap(ctx, image, m.f, m.e, m.b, m.c);
		if (!scaled)
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
		scaled = fz_scale_pixmap(ctx, image, 0, 0, (float)dx, (float)dy);
		return scaled;
	}

	return NULL;
}

static void
fz_draw_fill_image(fz_device *devp, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *converted = NULL;
	fz_pixmap *scaled = NULL;
	int after;
	int dx, dy;
	fz_context *ctx = dev->ctx;

	fz_var(scaled);

	if (!model)
	{
		fz_warn(dev->ctx, "cannot render image directly to an alpha mask");
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
		converted = fz_new_pixmap_with_rect(ctx, model, fz_bound_pixmap(image));
		fz_convert_pixmap(ctx, image, converted);
		image = converted;
	}

	fz_try(ctx)
	{
		dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
		dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
		if (dx < image->w && dy < image->h)
		{
			int gridfit = alpha == 1.0f && !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
			scaled = fz_transform_pixmap(ctx, image, &ctm, dev->dest->x, dev->dest->y, dx, dy, gridfit);
			if (!scaled)
			{
				if (dx < 1)
					dx = 1;
				if (dy < 1)
					dy = 1;
				scaled = fz_scale_pixmap(ctx, image, image->x, image->y, dx, dy);
			}
			if (scaled)
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
				converted = fz_new_pixmap_with_rect(ctx, model, fz_bound_pixmap(image));
				fz_convert_pixmap(ctx, image, converted);
				image = converted;
			}
		}

		fz_paint_image(dev->dest, dev->scissor, dev->shape, image, ctm, alpha * 255);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, scaled);
		fz_drop_pixmap(ctx, converted);
		fz_rethrow(ctx);
	}

	if (scaled)
		fz_drop_pixmap(ctx, scaled);
	if (converted)
		fz_drop_pixmap(ctx, converted);

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_fill_image_mask(fz_device *devp, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_draw_device *dev = devp->user;
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
		scaled = fz_transform_pixmap(dev->ctx, image, &ctm, dev->dest->x, dev->dest->y, dx, dy, gridfit);
		if (!scaled)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_scale_pixmap(dev->ctx, image, image->x, image->y, dx, dy);
		}
		if (scaled)
			image = scaled;
	}

	fz_convert_color(dev->ctx, colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_paint_image_with_color(dev->dest, dev->scissor, dev->shape, image, ctm, colorbv);

	if (scaled)
		fz_drop_pixmap(dev->ctx, scaled);

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);
}

static void
fz_draw_clip_image_mask(fz_device *devp, fz_pixmap *image, fz_rect *rect, fz_matrix ctm)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_context *ctx = dev->ctx;
	fz_bbox bbox;
	fz_pixmap *mask = NULL;
	fz_pixmap *dest = NULL;
	fz_pixmap *shape = NULL;
	fz_pixmap *scaled = NULL;
	int dx, dy;

	fz_var(mask);
	fz_var(dest);
	fz_var(shape);

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

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

	mask = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
	fz_clear_pixmap(mask);
	fz_try(ctx)
	{
		dest = fz_new_pixmap_with_rect(dev->ctx, model, bbox);
		/* FIXME: See note #1 */
		fz_clear_pixmap(dest);
		if (dev->shape)
		{
			shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
			fz_clear_pixmap(shape);
		}
		else
			shape = NULL;

		dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
		dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
		if (dx < image->w && dy < image->h)
		{
			int gridfit = !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
			scaled = fz_transform_pixmap(dev->ctx, image, &ctm, dev->dest->x, dev->dest->y, dx, dy, gridfit);
			if (!scaled)
			{
				if (dx < 1)
					dx = 1;
				if (dy < 1)
					dy = 1;
				scaled = fz_scale_pixmap(dev->ctx, image, image->x, image->y, dx, dy);
			}
			if (scaled)
				image = scaled;
		}
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, shape);
		fz_drop_pixmap(ctx, dest);
		fz_drop_pixmap(ctx, mask);
		fz_rethrow(ctx);
	}

	fz_paint_image(mask, bbox, dev->shape, image, ctm, 255);

	if (scaled)
		fz_drop_pixmap(dev->ctx, scaled);

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
fz_draw_pop_clip(fz_device *devp)
{
	fz_draw_device *dev = devp->user;
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
			fz_dump_blend(dev->ctx, dev->dest, "Clipping ");
			if (dev->shape)
				fz_dump_blend(dev->ctx, dev->shape, "/");
			fz_dump_blend(dev->ctx, dest, " onto ");
			if (shape)
				fz_dump_blend(dev->ctx, shape, "/");
			fz_dump_blend(dev->ctx, mask, " with ");
#endif
			fz_paint_pixmap_with_mask(dest, dev->dest, mask);
			if (shape)
			{
				assert(shape != dev->shape);
				fz_paint_pixmap_with_mask(shape, dev->shape, mask);
				fz_drop_pixmap(dev->ctx, dev->shape);
				dev->shape = shape;
			}
			fz_drop_pixmap(dev->ctx, mask);
			fz_drop_pixmap(dev->ctx, dev->dest);
			dev->dest = dest;
#ifdef DUMP_GROUP_BLENDS
			fz_dump_blend(dev->ctx, dev->dest, " to get ");
			if (dev->shape)
				fz_dump_blend(dev->ctx, dev->shape, "/");
			printf("\n");
#endif
		}
		else
		{
#ifdef DUMP_GROUP_BLENDS
			dump_spaces(dev->top, "Clip End\n");
#endif
			assert(!dest);
			assert(shape == dev->shape);
		}
	}
}

static void
fz_draw_begin_mask(fz_device *devp, fz_rect rect, int luminosity, fz_colorspace *colorspace, float *colorfv)
{
	fz_draw_device *dev = devp->user;
	fz_pixmap *dest;
	fz_pixmap *shape = dev->shape;
	fz_bbox bbox;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

	bbox = fz_round_rect(rect);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(dev->ctx, fz_device_gray, bbox);
	if (dev->shape)
	{
		/* FIXME: If we ever want to support AIS true, then we
		 * probably want to create a shape pixmap here, using:
		 * shape = fz_new_pixmap_with_rect(NULL, bbox);
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
		fz_convert_color(dev->ctx, colorspace, colorfv, fz_device_gray, &bc);
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
fz_draw_end_mask(fz_device *devp)
{
	fz_draw_device *dev = devp->user;
	fz_pixmap *mask = dev->dest;
	fz_pixmap *maskshape = dev->shape;
	fz_pixmap *temp, *dest;
	fz_bbox bbox;
	int luminosity;

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

	if (dev->top > 0)
	{
		/* pop soft mask buffer */
		dev->top--;
		luminosity = dev->stack[dev->top].luminosity;
		dev->scissor = dev->stack[dev->top].scissor;
		dev->dest = dev->stack[dev->top].dest;
		dev->shape = dev->stack[dev->top].shape;

		/* convert to alpha mask */
		temp = fz_alpha_from_gray(dev->ctx, mask, luminosity);
		fz_drop_pixmap(dev->ctx, mask);
		fz_drop_pixmap(dev->ctx, maskshape);

		/* create new dest scratch buffer */
		bbox = fz_bound_pixmap(temp);
		dest = fz_new_pixmap_with_rect(dev->ctx, dev->dest->colorspace, bbox);
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
			dev->shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
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
fz_draw_begin_group(fz_device *devp, fz_rect rect, int isolated, int knockout, int blendmode, float alpha)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *dest, *shape;
	fz_context *ctx = dev->ctx;

	if (dev->top == dev->stack_max)
	{
		fz_warn(ctx, "assert: too many buffers on stack");
		return;
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	bbox = fz_round_rect(rect);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(ctx, model, bbox);

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

	if (blendmode == 0 && alpha == 1.0 && isolated)
	{
		/* We can render direct to any existing shape plane. If there
		 * isn't one, we don't need to make one. */
		shape = dev->shape;
	}
	else
	{
		fz_try(ctx)
		{
			shape = fz_new_pixmap_with_rect(ctx, NULL, bbox);
			fz_clear_pixmap(shape);
		}
		fz_catch(ctx)
		{
			fz_drop_pixmap(ctx, dest);
			fz_rethrow(ctx);
		}
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
fz_draw_end_group(fz_device *devp)
{
	fz_draw_device *dev = devp->user;
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
		fz_dump_blend(dev->ctx, group, "Blending ");
		if (shape)
			fz_dump_blend(dev->ctx, shape, "/");
		fz_dump_blend(dev->ctx, dev->dest, " onto ");
		if (dev->shape)
			fz_dump_blend(dev->ctx, dev->shape, "/");
		if (alpha != 1.0f)
			printf(" (alpha %g)", alpha);
		if (blendmode != 0)
			printf(" (blend %d)", blendmode);
		if (isolated != 0)
			printf(" (isolated)");
		if (blendmode & FZ_BLEND_KNOCKOUT)
			printf(" (knockout)");
#endif
		if ((blendmode == 0) && (!shape))
			fz_paint_pixmap(dev->dest, group, alpha * 255);
		else
			fz_blend_pixmap(dev->dest, group, alpha * 255, blendmode, isolated, shape);

		fz_drop_pixmap(dev->ctx, group);
		if (shape != dev->shape)
		{
			if (dev->shape)
			{
				fz_paint_pixmap(dev->shape, shape, alpha * 255);
			}
			fz_drop_pixmap(dev->ctx, shape);
		}
#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(dev->ctx, dev->dest, " to get ");
		if (dev->shape)
			fz_dump_blend(dev->ctx, dev->shape, "/");
		printf("\n");
#endif
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(dev);
}

static void
fz_draw_begin_tile(fz_device *devp, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	fz_draw_device *dev = devp->user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *dest;
	fz_bbox bbox;

	/* area, view, xstep, ystep are in pattern space */
	/* ctm maps from pattern space to device space */

	if (dev->top == dev->stack_max)
		fz_grow_stack(dev);

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);

	bbox = fz_round_rect(fz_transform_rect(ctm, view));
	/* We should never have a bbox that entirely covers our destination.
	 * If we do, then the check for only 1 tile being visible above has
	 * failed. */
	/* SumatraPDF: this assertion doesn't always hold
	   (fails e.g. for "infinite pattern recursion.pdf")
	assert(bbox.x0 > dev->dest->x || bbox.x1 < dev->dest->x + dev->dest->w ||
		bbox.y0 > dev->dest->y || bbox.y1 < dev->dest->y + dev->dest->h);
	*/
	dest = fz_new_pixmap_with_rect(dev->ctx, model, bbox);
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

	/* SumatraPDF: fix shaping crash */
	if (dev->shape)
	{
		dev->shape = fz_new_pixmap_with_rect(dev->ctx, NULL, bbox);
		fz_clear_pixmap(dev->shape);
	}

	dev->scissor = bbox;
	dev->dest = dest;
}

static void
fz_draw_end_tile(fz_device *devp)
{
	fz_draw_device *dev = devp->user;
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

		/* SumatraPDF: fix shaping crash */
		if (dev->shape)
		{
			fz_pixmap *shape = dev->shape;
			dev->shape = dev->stack[dev->top].shape;
			for (y = y0; y < y1; y++)
			{
				for (x = x0; x < x1; x++)
				{
					ttm = fz_concat(fz_translate(x * xstep, y * ystep), ctm);
					shape->x = ttm.e;
					shape->y = ttm.f;
					fz_paint_pixmap(dev->shape, shape, 255);
				}
			}
			fz_drop_pixmap(dev->ctx, shape);
		}

		fz_drop_pixmap(dev->ctx, tile);
	}

	if (dev->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(dev);
}

static void
fz_draw_free_user(fz_device *devp)
{
	fz_draw_device *dev = devp->user;
	fz_context *ctx = dev->ctx;
	/* TODO: pop and free the stacks */
	if (dev->top > 0)
	{
		fz_warn(ctx, "items left on stack in draw device: %d", dev->top);
		dev->top--;
		if (dev->dest != dev->stack[dev->top].dest)
			fz_drop_pixmap(ctx, dev->dest);
		if (dev->shape != dev->stack[dev->top].shape)
			fz_drop_pixmap(ctx, dev->shape);
		while(dev->top > 0)
		{
			fz_pixmap *mask = dev->stack[dev->top].mask;
			fz_pixmap *dest = dev->stack[dev->top].dest;
			fz_pixmap *shape = dev->stack[dev->top].shape;

			dev->top--;
			if (mask != dev->stack[dev->top].mask)
				fz_drop_pixmap(ctx, mask);
			if (dest != dev->stack[dev->top].dest)
				fz_drop_pixmap(ctx, dest);
			if (shape != dev->stack[dev->top].shape)
				fz_drop_pixmap(ctx, shape);
		}
		fz_drop_pixmap(ctx, dev->stack[0].mask);
		fz_drop_pixmap(ctx, dev->stack[0].dest);
		fz_drop_pixmap(ctx, dev->stack[0].shape);
	}
	if (dev->stack != &dev->init_stack[0])
		fz_free(ctx, dev->stack);
	fz_free_gel(dev->gel);
	fz_free(ctx, dev);
}

fz_device *
fz_new_draw_device(fz_context *ctx, fz_glyph_cache *cache, fz_pixmap *dest)
{
	fz_device *dev = NULL;
	fz_draw_device *ddev = fz_malloc_struct(ctx, fz_draw_device);
	ddev->gel = NULL;

	fz_var(dev);
	fz_try(ctx)
	{
		ddev->cache = cache;
		ddev->gel = fz_new_gel(ctx);
		ddev->dest = dest;
		ddev->shape = NULL;
		ddev->top = 0;
		ddev->blendmode = 0;
		ddev->flags = 0;
		ddev->ctx = ctx;
		ddev->stack = &ddev->init_stack[0];
		ddev->stack_max = STACK_SIZE;

		ddev->scissor.x0 = dest->x;
		ddev->scissor.y0 = dest->y;
		ddev->scissor.x1 = dest->x + dest->w;
		ddev->scissor.y1 = dest->y + dest->h;

		dev = fz_new_device(ctx, ddev);
	}
	fz_catch(ctx)
	{
		fz_free_gel(ddev->gel);
		fz_free(ctx, ddev);
		fz_rethrow(ctx);
	}
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
fz_new_draw_device_type3(fz_context *ctx, fz_glyph_cache *cache, fz_pixmap *dest)
{
	fz_device *dev = fz_new_draw_device(ctx, cache, dest);
	fz_draw_device *ddev = dev->user;
	ddev->flags |= FZ_DRAWDEV_FLAGS_TYPE3;
	return dev;
}
