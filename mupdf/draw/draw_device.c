#include "fitz.h"

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

#define STACK_SIZE 96

typedef struct fz_draw_device_s fz_draw_device;

struct fz_draw_device_s
{
	fz_glyph_cache *cache;
	fz_gel *gel;
	fz_ael *ael;

	fz_pixmap *dest;
	fz_bbox scissor;

	int top;
	struct {
		fz_bbox scissor;
		fz_pixmap *dest;
		fz_pixmap *mask;
		fz_blendmode blendmode;
		int luminosity;
		float alpha;
		fz_matrix ctm;
		float xstep, ystep;
		fz_rect area;
	} stack[STACK_SIZE];
};

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

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_scan_convert(dev->gel, dev->ael, even_odd, bbox, dev->dest, colorbv);
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

	fz_convert_color(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_scan_convert(dev->gel, dev->ael, 0, bbox, dev->dest, colorbv);
}

static void
fz_draw_clip_path(void *user, fz_path *path, int even_odd, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	fz_pixmap *mask, *dest;
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

	if (fz_is_empty_rect(bbox) || fz_is_rect_gel(dev->gel))
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = NULL;
		dev->stack[dev->top].dest = NULL;
		dev->scissor = bbox;
		dev->top++;
		return;
	}

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	dest = fz_new_pixmap_with_rect(model, bbox);

	fz_clear_pixmap(mask);
	fz_clear_pixmap(dest);

	fz_scan_convert(dev->gel, dev->ael, even_odd, bbox, mask, NULL);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;
}

static void
fz_draw_clip_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrix_expansion(ctm);
	float flatness = 0.3f / expansion;
	float linewidth = stroke->linewidth;
	fz_pixmap *mask, *dest;
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

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	dest = fz_new_pixmap_with_rect(model, bbox);

	fz_clear_pixmap(mask);
	fz_clear_pixmap(dest);

	if (!fz_is_empty_rect(bbox))
		fz_scan_convert(dev->gel, dev->ael, 0, bbox, mask, NULL);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
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
	float colorfv[FZ_MAX_COLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

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

		glyph = fz_render_glyph(dev->cache, text->font, gid, trm);
		if (glyph)
		{
			draw_glyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			fz_drop_pixmap(glyph);
		}
	}
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
			fz_drop_pixmap(glyph);
		}
	}
}

static void
fz_draw_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
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
		dest = fz_new_pixmap_with_rect(model, bbox);

		fz_clear_pixmap(mask);
		fz_clear_pixmap(dest);

		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = mask;
		dev->stack[dev->top].dest = dev->dest;
		dev->scissor = bbox;
		dev->dest = dest;
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

			glyph = fz_render_glyph(dev->cache, text->font, gid, trm);
			if (glyph)
			{
				draw_glyph(NULL, mask, glyph, x, y, bbox);
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
	fz_pixmap *mask, *dest;
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
	dest = fz_new_pixmap_with_rect(model, bbox);

	fz_clear_pixmap(mask);
	fz_clear_pixmap(dest);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
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
	}

	fz_paint_shade(shade, ctm, dest, bbox);

	if (alpha < 1)
	{
		fz_paint_pixmap(dev->dest, dest, alpha * 255);
		fz_drop_pixmap(dest);
	}
}

static fz_pixmap *
fz_transform_pixmap(fz_pixmap *image, fz_matrix *ctm, int x, int y, int dx, int dy)
{
	fz_pixmap *scaled;

	if ((ctm->a != 0) && (ctm->b == 0) && (ctm->c == 0) && (ctm->d != 0))
	{
		/* Unrotated or X flip or Yflip or XYflip */
		scaled = fz_scale_pixmap(image, ctm->e, ctm->f, ctm->a, ctm->d);
		if (scaled == NULL)
			return NULL;
		ctm->a = scaled->w;
		ctm->d = scaled->h;
		ctm->e = scaled->x;
		ctm->f = scaled->y;
		return scaled;
	}
	if ((ctm->a == 0) && (ctm->b != 0) && (ctm->c != 0) && (ctm->d == 0))
	{
		/* Other orthogonal flip/rotation cases */
		scaled = fz_scale_pixmap(image, ctm->f, ctm->e, ctm->b, ctm->c);
		if (scaled == NULL)
			return NULL;
		ctm->b = scaled->w;
		ctm->c = scaled->h;
		ctm->f = scaled->x;
		ctm->e = scaled->y;
		return scaled;
	}
	/* Downscale, non rectilinear case */
	if ((dx > 0) && (dy > 0))
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

	after = 0;
	if (image->colorspace == fz_device_gray)
		after = 1;

	if (image->colorspace != model && !after)
	{
		converted = fz_new_pixmap(model, image->x, image->y, image->w, image->h);
		fz_convert_pixmap(image, converted);
		image = converted;
	}

	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		scaled = fz_transform_pixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy);
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
		if (image->colorspace == fz_device_gray && model == fz_device_rgb)
		{
			/* We have special case rendering code for gray -> rgb */
		}
		else
		{
			converted = fz_new_pixmap(model, image->x, image->y, image->w, image->h);
			fz_convert_pixmap(image, converted);
			image = converted;
		}
	}

	fz_paint_image(dev->dest, dev->scissor, image, ctm, alpha * 255);

	if (scaled)
		fz_drop_pixmap(scaled);
	if (converted)
		fz_drop_pixmap(converted);
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

	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		scaled = fz_transform_pixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy);
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

	fz_paint_image_with_color(dev->dest, dev->scissor, image, ctm, colorbv);

	if (scaled)
		fz_drop_pixmap(scaled);
}

static void
fz_draw_clip_image_mask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
	fz_pixmap *scaled = NULL;
	int dx, dy;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (image->w == 0 || image->h == 0)
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = NULL;
		dev->stack[dev->top].dest = NULL;
		dev->scissor = fz_empty_bbox;
		dev->top++;
		return;
	}

	bbox = fz_round_rect(fz_transform_rect(ctm, fz_unit_rect));
	bbox = fz_intersect_bbox(bbox, dev->scissor);

	mask = fz_new_pixmap_with_rect(NULL, bbox);
	dest = fz_new_pixmap_with_rect(model, bbox);

	fz_clear_pixmap(mask);
	fz_clear_pixmap(dest);

	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		scaled = fz_transform_pixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy);
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

	fz_paint_image(mask, bbox, image, ctm, 255);

	if (scaled)
		fz_drop_pixmap(scaled);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;
}

static void
fz_draw_pop_clip(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *mask, *dest;
	if (dev->top > 0)
	{
		dev->top--;
		dev->scissor = dev->stack[dev->top].scissor;
		mask = dev->stack[dev->top].mask;
		dest = dev->stack[dev->top].dest;
		if (mask && dest)
		{
			fz_pixmap *scratch = dev->dest;
			fz_paint_pixmap_with_mask(dest, scratch, mask);
			fz_drop_pixmap(mask);
			fz_drop_pixmap(scratch);
			dev->dest = dest;
		}
	}
}

static void
fz_draw_begin_mask(void *user, fz_rect rect, int luminosity, fz_colorspace *colorspace, float *colorfv)
{
	fz_draw_device *dev = user;
	fz_pixmap *dest;
	fz_bbox bbox;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_round_rect(rect);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(fz_device_gray, bbox);

	if (luminosity)
	{
		float bc;
		if (!colorspace)
			colorspace = fz_device_gray;
		fz_convert_color(colorspace, colorfv, fz_device_gray, &bc);
		fz_clear_pixmap_with_color(dest, bc * 255);
	}
	else
		fz_clear_pixmap(dest);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].luminosity = luminosity;
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
}

static void
fz_draw_end_mask(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *mask = dev->dest;
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

		/* convert to alpha mask */
		temp = fz_alpha_from_gray(mask, luminosity);
		fz_drop_pixmap(mask);

		/* create new dest scratch buffer */
		bbox = fz_bound_pixmap(temp);
		dest = fz_new_pixmap_with_rect(dev->dest->colorspace, bbox);
		fz_clear_pixmap(dest);

		/* push soft mask as clip mask */
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = temp;
		dev->stack[dev->top].dest = dev->dest;
		dev->scissor = bbox;
		dev->dest = dest;
		dev->top++;
	}
}

static void
fz_draw_begin_group(void *user, fz_rect rect, int isolated, int knockout, fz_blendmode blendmode, float alpha)
{
	fz_draw_device *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *dest;

	if (dev->top == STACK_SIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_round_rect(rect);
	bbox = fz_intersect_bbox(bbox, dev->scissor);
	dest = fz_new_pixmap_with_rect(model, bbox);

	fz_clear_pixmap(dest);

	dev->stack[dev->top].alpha = alpha;
	dev->stack[dev->top].blendmode = blendmode;
	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
}

static void
fz_draw_end_group(void *user)
{
	fz_draw_device *dev = user;
	fz_pixmap *group = dev->dest;
	fz_blendmode blendmode;
	float alpha;

	if (dev->top > 0)
	{
		dev->top--;
		alpha = dev->stack[dev->top].alpha;
		blendmode = dev->stack[dev->top].blendmode;
		dev->dest = dev->stack[dev->top].dest;
		dev->scissor = dev->stack[dev->top].scissor;

		if (blendmode == FZ_BLEND_NORMAL)
			fz_paint_pixmap(dev->dest, group, alpha * 255);
		else
			fz_blend_pixmap(dev->dest, group, alpha * 255, blendmode);

		fz_drop_pixmap(group);
	}
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

	bbox = fz_round_rect(fz_transform_rect(ctm, view));
	dest = fz_new_pixmap_with_rect(model, bbox);
	fz_clear_pixmap(dest);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].xstep = xstep;
	dev->stack[dev->top].ystep = ystep;
	dev->stack[dev->top].area = area;
	dev->stack[dev->top].ctm = ctm;
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
		xstep = dev->stack[dev->top].xstep;
		ystep = dev->stack[dev->top].ystep;
		area = dev->stack[dev->top].area;
		ctm = dev->stack[dev->top].ctm;
		dev->scissor = dev->stack[dev->top].scissor;
		dev->dest = dev->stack[dev->top].dest;

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
}

static void
fz_draw_free_user(void *user)
{
	fz_draw_device *dev = user;
	/* TODO: pop and free the stacks */
	fz_free_gel(dev->gel);
	fz_free_ael(dev->ael);
	fz_free(dev);
}

fz_device *
fz_new_draw_device(fz_glyph_cache *cache, fz_pixmap *dest)
{
	fz_device *dev;
	fz_draw_device *ddev = fz_malloc(sizeof(fz_draw_device));
	ddev->cache = cache;
	ddev->gel = fz_new_gel();
	ddev->ael = fz_new_ael();
	ddev->dest = dest;
	ddev->top = 0;

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
