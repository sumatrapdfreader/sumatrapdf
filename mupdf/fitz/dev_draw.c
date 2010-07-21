#include "fitz.h"

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

#define STACKSIZE 96

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

	if (model)
	{
		fz_convertcolor(colorspace, color, model, colorfv);
		for (i = 0; i < model->n; i++)
			colorbv[i] = colorfv[i] * 255;
		colorbv[i] = alpha * 255;
		fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, dev->dest, colorbv, nil, nil);
	}
	else
	{
		fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, dev->dest, nil, nil, nil);
	}
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

	if (model)
	{
		fz_convertcolor(colorspace, color, model, colorfv);
		for (i = 0; i < model->n; i++)
			colorbv[i] = colorfv[i] * 255;
		colorbv[i] = alpha * 255;
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, colorbv, nil, nil);
	}
	else
	{
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, nil, nil, nil);
	}
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

	fz_clearpixmap(mask, 0);
	fz_clearpixmap(dest, 0);

	fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, mask, nil, nil, nil);

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

	fz_clearpixmap(mask, 0);
	fz_clearpixmap(dest, 0);

	if (!fz_isemptyrect(bbox))
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, mask, nil, nil, nil);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;
}

static void
drawglyph(unsigned char *colorbv, fz_pixmap *dst, fz_pixmap *src,
	int xorig, int yorig, fz_bbox scissor)
{
	unsigned char *dp, *sp;
	int w, h;

	int dx0 = scissor.x0;
	int dy0 = scissor.y0;
	int dx1 = scissor.x1;
	int dy1 = scissor.y1;

	int x0 = xorig + src->x;
	int y0 = yorig + src->y;
	int x1 = x0 + src->w;
	int y1 = y0 + src->h;

	int sx0 = 0;
	int sy0 = 0;
	int sx1 = src->w;
	int sy1 = src->h;

	if (x1 <= dx0 || x0 >= dx1) return;
	if (y1 <= dy0 || y0 >= dy1) return;
	if (x0 < dx0) { sx0 += dx0 - x0; x0 = dx0; }
	if (y0 < dy0) { sy0 += dy0 - y0; y0 = dy0; }
	if (x1 > dx1) { sx1 += dx1 - x1; }
	if (y1 > dy1) { sy1 += dy1 - y1; }

	sp = src->samples + (sy0 * src->w + sx0);
	dp = dst->samples + ((y0 - dst->y) * dst->w + (x0 - dst->x)) * dst->n;

	w = sx1 - sx0;
	h = sy1 - sy0;

	if (dst->colorspace)
	{
		switch (dst->n)
		{
		case 2:
			fz_text_w2i1o2(colorbv, sp, src->w, dp, dst->w * 2, w, h);
			break;
		case 4:
			fz_text_w4i1o4(colorbv, sp, src->w, dp, dst->w * 4, w, h);
			break;
		default:
			assert("Write fz_text_wni1on" != NULL);
			break;
		}
	}
	else
		fz_text_1o1(sp, src->w, dp, dst->w, w, h);
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

	if (model)
	{
		fz_convertcolor(colorspace, color, model, colorfv);
		for (i = 0; i < model->n; i++)
			colorbv[i] = colorfv[i] * 255;
		colorbv[i] = alpha * 255;
	}

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
			if (model)
				drawglyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			else
				drawglyph(nil, dev->dest, glyph, x, y, dev->scissor);
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

	if (model)
	{
		fz_convertcolor(colorspace, color, model, colorfv);
		for (i = 0; i < model->n; i++)
			colorbv[i] = colorfv[i] * 255;
		colorbv[i] = alpha * 255;
	}

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
			if (model)
				drawglyph(colorbv, dev->dest, glyph, x, y, dev->scissor);
			else
				drawglyph(nil, dev->dest, glyph, x, y, dev->scissor);
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

		fz_clearpixmap(mask, 0);
		fz_clearpixmap(dest, 0);

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
				drawglyph(NULL, mask, glyph, x, y, bbox);
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

	fz_clearpixmap(mask, 0);
	fz_clearpixmap(dest, 0);

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
				drawglyph(NULL, mask, glyph, x, y, bbox);
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
fz_drawfillshade(void *user, fz_shade *shade, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_pixmap *dest = dev->dest;
	fz_rect bounds;
	fz_bbox bbox;
	float colorfv[FZ_MAXCOLORS];
	unsigned char colorbv[FZ_MAXCOLORS + 1];

	bounds = fz_boundshade(shade, ctm);
	bbox = fz_intersectbbox(fz_roundrect(bounds), dev->scissor);

	// TODO: proper clip by shade->bbox
	if (!fz_isemptyrect(shade->bbox))
	{
		bounds = fz_transformrect(fz_concat(shade->matrix, ctm), shade->bbox);
		bbox = fz_intersectbbox(fz_roundrect(bounds), bbox);
	}

	if (fz_isemptyrect(bbox))
		return;

	if (!model)
	{
		fz_warn("cannot render shading directly to an alpha mask");
		return;
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
		for (y = bbox.y0; y < bbox.y1; y++)
		{
			s = dest->samples + ((bbox.x0 - dest->x) + (y - dest->y) * dest->w) * dest->n;
			for (x = bbox.x0; x < bbox.x1; x++)
			{
				for (i = 0; i < n; i++)
					*s++ = colorbv[i];
			}
		}
	}

	fz_rendershade(shade, ctm, dev->dest, bbox);
}

static inline void
calcimagestate(fz_drawdevice *dev, fz_pixmap *image, fz_matrix ctm,
	fz_bbox *bbox, fz_matrix *invmat, int *dx, int *dy)
{
	float sx, sy;
	fz_path *path;
	fz_matrix mat;
	int w, h;

	sx = image->w / sqrtf(ctm.a * ctm.a + ctm.b * ctm.b);
	sy = image->h / sqrtf(ctm.c * ctm.c + ctm.d * ctm.d);

	if (sx < 1)
		*dx = 1;
	else
		*dx = sx;

	if (sy < 1)
		*dy = 1;
	else
		*dy = sy;

	w = (image->w + *dx - 1) / *dx;
	h = (image->h + *dy - 1) / *dy;

	path = fz_newpath();
	fz_moveto(path, 0, 0);
	fz_lineto(path, 1, 0);
	fz_lineto(path, 1, 1);
	fz_lineto(path, 0, 1);
	fz_closepath(path);

	fz_resetgel(dev->gel, dev->scissor);
	fz_fillpath(dev->gel, path, ctm, 1);
	fz_sortgel(dev->gel);

	fz_freepath(path);

	*bbox = fz_boundgel(dev->gel);
	*bbox = fz_intersectbbox(*bbox, dev->scissor);

	mat.a = 1.0f / w;
	mat.b = 0;
	mat.c = 0;
	mat.d = -1.0f / h;
	mat.e = 0;
	mat.f = 1;
	*invmat = fz_invertmatrix(fz_concat(mat, ctm));
	invmat->e -= 0.5f;
	invmat->f -= 0.5f;
}

static void
fz_drawfillimage(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_colorspace *model = dev->dest->colorspace;
	fz_bbox bbox;
	int dx, dy;
	fz_pixmap *scaled = nil;
	fz_pixmap *converted = nil;
	fz_matrix invmat;

	if (!model)
	{
		fz_warn("cannot render image directly to an alpha mask");
		return;
	}

	calcimagestate(dev, image, ctm, &bbox, &invmat, &dx, &dy);

	if (fz_isemptyrect(bbox) || image->w == 0 || image->h == 0)
		return;

	if (dx != 1 || dy != 1)
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}

	if (image->colorspace != model)
	{
		converted = fz_newpixmap(model, image->x, image->y, image->w, image->h);
		fz_convertpixmap(image, converted);
		image = converted;
	}

	fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, nil, image, &invmat);

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
	fz_bbox bbox;
	int dx, dy;
	fz_pixmap *scaled = nil;
	fz_matrix invmat;
	int i;

	calcimagestate(dev, image, ctm, &bbox, &invmat, &dx, &dy);

	if (fz_isemptyrect(bbox) || image->w == 0 || image->h == 0)
		return;

	if (dx != 1 || dy != 1)
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}

	if (dev->dest->colorspace)
	{
		fz_convertcolor(colorspace, color, model, colorfv);
		for (i = 0; i < model->n; i++)
			colorbv[i] = colorfv[i] * 255;
		colorbv[i] = alpha * 255;
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, colorbv, image, &invmat);
	}
	else
	{
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, nil, image, &invmat);
	}

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
	int dx, dy;
	fz_pixmap *scaled = nil;
	fz_matrix invmat;

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	calcimagestate(dev, image, ctm, &bbox, &invmat, &dx, &dy);

	if (fz_isemptyrect(bbox) || image->w == 0 || image->h == 0)
	{
		dev->stack[dev->top].scissor = dev->scissor;
		dev->stack[dev->top].mask = nil;
		dev->stack[dev->top].dest = nil;
		dev->scissor = bbox;
		dev->top++;
		return;
	}

	if (dx != 1 || dy != 1)
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(model, bbox);

	fz_clearpixmap(mask, 0);
	fz_clearpixmap(dest, 0);

	fz_scanconvert(dev->gel, dev->ael, 0, bbox, mask, nil, image, &invmat);

	dev->stack[dev->top].scissor = dev->scissor;
	dev->stack[dev->top].mask = mask;
	dev->stack[dev->top].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->top++;

	if (scaled)
		fz_droppixmap(scaled);
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
			fz_blendpixmapswithmask(dest, scratch, mask);
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

	fz_warn("fz_drawbeginmask");

	if (dev->top == STACKSIZE)
	{
		fz_warn("assert: too many buffers on stack");
		return;
	}

	bbox = fz_roundrect(rect);
	bbox = fz_intersectbbox(bbox, dev->scissor);
	dest = fz_newpixmapwithrect(fz_devicegray, bbox);

	if (luminosity)
		fz_clearpixmap(dest, 255);
	else
		fz_clearpixmap(dest, 0);

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

	fz_warn("fz_drawendmask");

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
		fz_clearpixmap(dest, 0);

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
fz_drawbegingroup(void *user, fz_rect rect, int isolated, int knockout, fz_blendmode blendmode)
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

	fz_clearpixmap(dest, 0);

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

	if (dev->top > 0)
	{
		dev->top--;
		blendmode = dev->stack[dev->top].blendmode;
		dev->dest = dev->stack[dev->top].dest;
		dev->scissor = dev->stack[dev->top].scissor;
		fz_blendpixmapswithmode(dev->dest, group, blendmode);
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
