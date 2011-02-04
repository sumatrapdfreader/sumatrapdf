#include "fitz.h"

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

#define STACKSIZE 96

#define SMOOTHSCALE

typedef struct fz_drawdevice_s fz_drawdevice;

struct fz_drawdevice_s
{
	fz_glyphcache *cache;
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
	} stack[STACKSIZE];
};

static void
fz_drawfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3f / expansion;
	unsigned char colorbv[FZ_MAXCOLORS + 1];
	float colorfv[FZ_MAXCOLORS];
	fz_bbox bbox;
	int i;

	fz_resetgel(dev->gel, dev->scissor);
	fz_fillpath(dev->gel, path, ctm, flatness);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	if (fz_isemptyrect(bbox))
		return;

	fz_convertcolor(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, dev->dest, colorbv);
}

static void
fz_drawstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3f / expansion;
	float linewidth = stroke->linewidth;
	unsigned char colorbv[FZ_MAXCOLORS + 1];
	float colorfv[FZ_MAXCOLORS];
	fz_bbox bbox;
	int i;

	if (linewidth * expansion < 0.1f)
		linewidth = 1 / expansion;

	fz_resetgel(dev->gel, dev->scissor);
	if (stroke->dashlen > 0)
		fz_dashpath(dev->gel, path, stroke, ctm, flatness, linewidth);
	else
		fz_strokepath(dev->gel, path, stroke, ctm, flatness, linewidth);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	if (fz_isemptyrect(bbox))
		return;

	fz_convertcolor(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, colorbv);
}

static void
fz_drawclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3f / expansion;
	fz_pixmap *mask, *dest;
	fz_bbox bbox;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	fz_resetgel(dev->gel, dev->scissor);
	fz_fillpath(dev->gel, path, ctm, flatness);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	if (fz_isemptyrect(bbox) || fz_isrectgel(dev->gel))
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = nil;
		dev->stack[dev->top].dest = nil;
		dev->scissor = bbox;
		dev->top++;
		return;
	}

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(model, bbox);

	fz_clearpixmap(mask);
	fz_clearpixmap(dest);

	fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, mask, nil);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;
}

static void
fz_drawclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3f / expansion;
	float linewidth = stroke->linewidth;
	fz_pixmap *mask, *dest;
	fz_bbox bbox;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (linewidth * expansion < 0.1f)
		linewidth = 1 / expansion;

	fz_resetgel(dev->gel, dev->scissor);
	if (stroke->dashlen > 0)
		fz_dashpath(dev->gel, path, stroke, ctm, flatness, linewidth);
	else
		fz_strokepath(dev->gel, path, stroke, ctm, flatness, linewidth);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(model, bbox);

	fz_clearpixmap(mask);
	fz_clearpixmap(dest);

	if (!fz_isemptyrect(bbox))
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, mask, nil);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;
}

static void
drawglyph(unsigned char *colorbv, fz_pixmap *dst, fz_pixmap *msk,
	int xorig, int yorig, fz_bbox scissor)
{
	unsigned char *dp, *mp;
	fz_bbox bbox;
	int x, y, w, h;

	bbox = fz_boundpixmap(msk);
	bbox.x0 += xorig;
	bbox.y0 += yorig;
	bbox.x1 += xorig;
	bbox.y1 += yorig;

	bbox = fz_intersectbbox(bbox, scissor); /* scissor < dst */
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
			fz_paintspancolor(dp, mp, dst->n, w, colorbv);
		else
			fz_paintspan(dp, mp, 1, w, 255);
		dp += dst->w * dst->n;
		mp += msk->w;
	}
}

static void
fz_drawfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAXCOLORS + 1];
	float colorfv[FZ_MAXCOLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	fz_convertcolor(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	tm = text->trm;

	for (i = 0; i < text->len; i++)
	{
		gid = text->els[i].gid;
		if (gid < 0)
			continue;

		tm.e = text->els[i].x;
		tm.f = text->els[i].y;
		trm = fz_concat(tm, ctm);
		x = floorf(trm.e);
		y = floorf(trm.f);
		trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
		trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

		glyph = fz_renderglyph(dev->cache, text->font, gid, trm);
		if (glyph)
		{
			drawglyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			fz_droppixmap(glyph);
		}
	}
}

static void
fz_drawstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAXCOLORS + 1];
	float colorfv[FZ_MAXCOLORS];
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	fz_convertcolor(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	tm = text->trm;

	for (i = 0; i < text->len; i++)
	{
		gid = text->els[i].gid;
		if (gid < 0)
			continue;

		tm.e = text->els[i].x;
		tm.f = text->els[i].y;
		trm = fz_concat(tm, ctm);
		x = floorf(trm.e);
		y = floorf(trm.f);
		trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
		trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

		glyph = fz_renderstrokedglyph(dev->cache, text->font, gid, trm, ctm, stroke);
		if (glyph)
		{
			drawglyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			fz_droppixmap(glyph);
		}
	}
}

static void
fz_drawcliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	/* If accumulate == 0 then this text object is guaranteed complete */
	/* If accumulate == 1 then this text object is the first (or only) in a sequence */
	/* If accumulate == 2 then this text object is a continuation */

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (accumulate == 0)
	{
		/* make the mask the exact size needed */
		bbox = fz_roundrect(fz_boundtext(text, ctm));
		bbox = fz_intersectbbox(bbox, dev->scissor);
	}
	else
	{
		/* be conservative about the size of the mask needed */
		bbox = dev->scissor;
	}

	if (accumulate == 0 || accumulate == 1)
	{
		mask = fz_newpixmapwithrect(nil, bbox);
		dest = fz_newpixmapwithrect(model, bbox);

		fz_clearpixmap(mask);
		fz_clearpixmap(dest);

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

	if (!fz_isemptyrect(bbox))
	{
		tm = text->trm;

		for (i = 0; i < text->len; i++)
		{
			gid = text->els[i].gid;
			if (gid < 0)
				continue;

			tm.e = text->els[i].x;
			tm.f = text->els[i].y;
			trm = fz_concat(tm, ctm);
			x = floorf(trm.e);
			y = floorf(trm.f);
			trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
			trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

			glyph = fz_renderglyph(dev->cache, text->font, gid, trm);
			if (glyph)
			{
				drawglyph(nil, mask, glyph, x, y, bbox);
				fz_droppixmap(glyph);
			}
		}
	}
}

static void
fz_drawclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	/* make the mask the exact size needed */
	bbox = fz_roundrect(fz_boundtext(text, ctm));
	bbox = fz_intersectbbox(bbox, dev->scissor);

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(model, bbox);

	fz_clearpixmap(mask);
	fz_clearpixmap(dest);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;

	if (!fz_isemptyrect(bbox))
	{
		tm = text->trm;

		for (i = 0; i < text->len; i++)
		{
			gid = text->els[i].gid;
			if (gid < 0)
				continue;

			tm.e = text->els[i].x;
			tm.f = text->els[i].y;
			trm = fz_concat(tm, ctm);
			x = floorf(trm.e);
			y = floorf(trm.f);
			trm.e = QUANT(trm.e - floorf(trm.e), HSUBPIX);
			trm.f = QUANT(trm.f - floorf(trm.f), VSUBPIX);

			glyph = fz_renderstrokedglyph(dev->cache, text->font, gid, trm, ctm, stroke);
			if (glyph)
			{
				drawglyph(nil, mask, glyph, x, y, bbox);
				fz_droppixmap(glyph);
			}
		}
	}
}

static void
fz_drawignoretext(void *user, fz_text *text, fz_matrix ctm)
{
}

static void
fz_drawfillshade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *dest = dev->dest;
	fz_rect bounds;
	fz_bbox bbox, scissor;
	float colorfv[FZ_MAXCOLORS];
	unsigned char colorbv[FZ_MAXCOLORS + 1];

	bounds = fz_boundshade(shade, ctm);
	bbox = fz_intersectbbox(fz_roundrect(bounds), dev->scissor);
	scissor = dev->scissor;

	// TODO: proper clip by shade->bbox

	if (fz_isemptyrect(bbox))
		return;

	if (!model)
	{
		fz_warn("cannot render shading directly to an alpha mask");
		return;
	}

	if (alpha < 1)
	{
		dest = fz_newpixmapwithrect(dev->dest->colorspace, bbox);
		fz_clearpixmap(dest);
	}

	if (shade->usebackground)
	{
		unsigned char *s;
		int x, y, n, i;
		fz_convertcolor(shade->cs, shade->background, model, colorfv);
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

	fz_paintshade(shade, ctm, dest, bbox);

	if (alpha < 1)
	{
		fz_paintpixmap(dev->dest, dest, alpha * 255);
		fz_droppixmap(dest);
	}
}

static int
fz_calcimagescale(fz_pixmap *image, fz_matrix ctm, int *dx, int *dy)
{
	float sx = image->w / sqrtf(ctm.a * ctm.a + ctm.b * ctm.b) * 0.66f;
	float sy = image->h / sqrtf(ctm.c * ctm.c + ctm.d * ctm.d) * 0.66f;
	*dx = sx > 1 ? sx : 1;
	*dy = sy > 1 ? sy : 1;
	return *dx > 1 || *dy > 1;
}

static fz_pixmap *
fz_smoothtransformpixmap(fz_pixmap *image, fz_matrix *ctm, int x, int y, int dx, int dy)
{
	fz_pixmap *scaled;

	if ((ctm->a != 0) && (ctm->b == 0) && (ctm->c == 0) && (ctm->d != 0))
	{
		/* Unrotated or X flip or Yflip or XYflip */
		scaled = fz_smoothscalepixmap(image, ctm->e, ctm->f, ctm->a, ctm->d);
		if (scaled == nil)
			return nil;
		ctm->a = scaled->w;
		ctm->d = scaled->h;
		ctm->e = scaled->x;
		ctm->f = scaled->y;
		return scaled;
	}
	if ((ctm->a == 0) && (ctm->b != 0) && (ctm->c != 0) && (ctm->d == 0))
	{
		/* Other orthogonal flip/rotation cases */
		scaled = fz_smoothscalepixmap(image, ctm->f, ctm->e, ctm->b, ctm->c);
		if (scaled == nil)
			return nil;
		ctm->b = scaled->w;
		ctm->c = scaled->h;
		ctm->f = scaled->x;
		ctm->e = scaled->y;
		return scaled;
	}
	/* Downscale, non rectilinear case */
	if ((dx > 0) && (dy > 0))
	{
		scaled = fz_smoothscalepixmap(image, 0, 0, (float)dx, (float)dy);
		return scaled;
	}
	return nil;
}

static void
fz_drawfillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *converted = nil;
	fz_pixmap *scaled = nil;
	int dx, dy;

	if (!model)
	{
		fz_warn("cannot render image directly to an alpha mask");
		return;
	}

	if (image->w == 0 || image->h == 0)
		return;

	if (image->colorspace != model)
	{
		converted = fz_newpixmap(model, image->x, image->y, image->w, image->h);
		fz_convertpixmap(image, converted);
		image = converted;
	}

#ifdef SMOOTHSCALE
	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		scaled = fz_smoothtransformpixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy);
		if (scaled == nil)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_smoothscalepixmap(image, image->x, image->y, dx, dy);
		}
		if (scaled != nil)
			image = scaled;
	}
#else
	if (fz_calcimagescale(image, ctm, &dx, &dy))
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}
#endif

	fz_paintimage(dev->dest, dev->scissor, image, ctm, alpha * 255);

	if (scaled)
		fz_droppixmap(scaled);
	if (converted)
		fz_droppixmap(converted);
}

static void
fz_drawfillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	unsigned char colorbv[FZ_MAXCOLORS + 1];
	float colorfv[FZ_MAXCOLORS];
	fz_pixmap *scaled = nil;
	int dx, dy;
	int i;

	if (image->w == 0 || image->h == 0)
		return;

#ifdef SMOOTHSCALE
	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		scaled = fz_smoothtransformpixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy);
		if (scaled == nil)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_smoothscalepixmap(image, image->x, image->y, dx, dy);
		}
		if (scaled != nil)
			image = scaled;
	}
#else
	if (fz_calcimagescale(image, ctm, &dx, &dy))
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}
#endif

	fz_convertcolor(colorspace, color, model, colorfv);
	for (i = 0; i < model->n; i++)
		colorbv[i] = colorfv[i] * 255;
	colorbv[i] = alpha * 255;

	fz_paintimagecolor(dev->dest, dev->scissor, image, ctm, colorbv);

	if (scaled)
		fz_droppixmap(scaled);
}

static void
fz_drawclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
	fz_pixmap *scaled = nil;
	int dx, dy;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	if (image->w == 0 || image->h == 0)
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = nil;
		dev->stack[dev->top].dest = nil;
		dev->scissor = fz_emptybbox;
		dev->top++;
		return;
	}

	bbox = fz_roundrect(fz_transformrect(ctm, fz_unitrect));
	bbox = fz_intersectbbox(bbox, dev->scissor);

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(model, bbox);

	fz_clearpixmap(mask);
	fz_clearpixmap(dest);

#ifdef SMOOTHSCALE
	dx = sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	dy = sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);
	if (dx < image->w && dy < image->h)
	{
		scaled = fz_smoothtransformpixmap(image, &ctm, dev->dest->x, dev->dest->y, dx, dy);
		if (scaled == nil)
		{
			if (dx < 1)
				dx = 1;
			if (dy < 1)
				dy = 1;
			scaled = fz_smoothscalepixmap(image, image->x, image->y, dx, dy);
		}
		if (scaled != nil)
			image = scaled;
	}
#else
	if (fz_calcimagescale(image, ctm, &dx, &dy))
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}
#endif

	fz_paintimage(mask, bbox, image, ctm, 255);

	if (scaled)
		fz_droppixmap(scaled);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;
}

static void
fz_drawpopclip(void *user)
{
	fz_drawdevice *dev = user;
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
			fz_paintpixmapmask(dest, scratch, mask);
			fz_droppixmap(mask);
			fz_droppixmap(scratch);
			dev->dest = dest;
		}
	}
}

static void
fz_drawbeginmask(void *user, fz_rect rect, int luminosity, fz_colorspace *colorspace, float *colorfv)
{
	fz_drawdevice *dev = user;
	fz_pixmap *dest;
	fz_bbox bbox;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_roundrect(rect);
	bbox = fz_intersectbbox(bbox, dev->scissor);
	dest = fz_newpixmapwithrect(fz_devicegray, bbox);

	if (luminosity)
	{
		float bc;
		if (!colorspace)
			colorspace = fz_devicegray;
		fz_convertcolor(colorspace, colorfv, fz_devicegray, &bc);
		fz_clearpixmapwithcolor(dest, bc * 255);
	}
	else
		fz_clearpixmap(dest);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->stack[dev->top].luminosity = luminosity;
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
}

static void
fz_drawendmask(void *user)
{
	fz_drawdevice *dev = user;
	fz_pixmap *mask = dev->dest;
	fz_pixmap *temp, *dest;
	fz_bbox bbox;
	int luminosity;

	if (dev->top == STACKSIZE)
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
		temp = fz_alphafromgray(mask, luminosity);
		fz_droppixmap(mask);

		/* create new dest scratch buffer */
		bbox = fz_boundpixmap(temp);
		dest = fz_newpixmapwithrect(dev->dest->colorspace, bbox);
		fz_clearpixmap(dest);

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
fz_drawbegingroup(void *user, fz_rect rect, int isolated, int knockout, fz_blendmode blendmode, float alpha)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	fz_pixmap *dest;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_roundrect(rect);
	bbox = fz_intersectbbox(bbox, dev->scissor);
	dest = fz_newpixmapwithrect(model, bbox);

	fz_clearpixmap(dest);

	dev->stack[dev->top].alpha = alpha;
	dev->stack[dev->top].blendmode = blendmode;
	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].dest = dev->dest;
	dev->top++;

	dev->scissor = bbox;
	dev->dest = dest;
}

static void
fz_drawendgroup(void *user)
{
	fz_drawdevice *dev = user;
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

		if (blendmode == FZ_BNORMAL)
			fz_paintpixmap(dev->dest, group, alpha * 255);
		else
			fz_blendpixmap(dev->dest, group, alpha * 255, blendmode);

		fz_droppixmap(group);
	}
}

static void
fz_drawfreeuser(void *user)
{
	fz_drawdevice *dev = user;
	/* TODO: pop and free the stacks */
	fz_freegel(dev->gel);
	fz_freeael(dev->ael);
	fz_free(dev);
}

fz_device *
fz_newdrawdevice(fz_glyphcache *cache, fz_pixmap *dest)
{
	fz_device *dev;
	fz_drawdevice *ddev = fz_malloc(sizeof(fz_drawdevice));
	ddev->cache = cache;
	ddev->gel = fz_newgel();
	ddev->ael = fz_newael();
	ddev->dest = dest;
	ddev->top = 0;

	ddev->scissor.x0 = dest->x;
	ddev->scissor.y0 = dest->y;
	ddev->scissor.x1 = dest->x + dest->w;
	ddev->scissor.y1 = dest->y + dest->h;

	dev = fz_newdevice(ddev);
	dev->freeuser = fz_drawfreeuser;

	dev->fillpath = fz_drawfillpath;
	dev->strokepath = fz_drawstrokepath;
	dev->clippath = fz_drawclippath;
	dev->clipstrokepath = fz_drawclipstrokepath;

	dev->filltext = fz_drawfilltext;
	dev->stroketext = fz_drawstroketext;
	dev->cliptext = fz_drawcliptext;
	dev->clipstroketext = fz_drawclipstroketext;
	dev->ignoretext = fz_drawignoretext;

	dev->fillimagemask = fz_drawfillimagemask;
	dev->clipimagemask = fz_drawclipimagemask;
	dev->fillimage = fz_drawfillimage;
	dev->fillshade = fz_drawfillshade;

	dev->popclip = fz_drawpopclip;

	dev->beginmask = fz_drawbeginmask;
	dev->endmask = fz_drawendmask;
	dev->begingroup = fz_drawbegingroup;
	dev->endgroup = fz_drawendgroup;

	return dev;
}
