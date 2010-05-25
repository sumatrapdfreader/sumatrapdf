#include "fitz.h"

#define QUANT(x,a) (((int)((x) * (a))) / (a))
#define HSUBPIX 5.0
#define VSUBPIX 5.0

#define MAXCLIP 64

typedef struct fz_drawdevice_s fz_drawdevice;

struct fz_drawdevice_s
{
	fz_colorspace *model;
	fz_glyphcache *cache;
	fz_gel *gel;
	fz_ael *ael;
	fz_pixmap *dest;
	fz_bbox scissor;
	struct {
		fz_pixmap *dest;
		fz_pixmap *mask;
		fz_bbox scissor;
	} clipstack[MAXCLIP];
	int cliptop;
};

static void
blendover(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *sp, *dp;
	fz_bbox sr, dr;
	int x, y, w, h;

	sr.x0 = src->x;
	sr.y0 = src->y;
	sr.x1 = src->x + src->w;
	sr.y1 = src->y + src->h;

	dr.x0 = dst->x;
	dr.y0 = dst->y;
	dr.x1 = dst->x + dst->w;
	dr.y1 = dst->y + dst->h;

	dr = fz_intersectbbox(sr, dr);
	x = dr.x0;
	y = dr.y0;
	w = dr.x1 - dr.x0;
	h = dr.y1 - dr.y0;

	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	if (src->n == 1 && dst->n == 1)
		fz_duff_1o1(sp, src->w, dp, dst->w, w, h);
	else if (src->n == 4 && dst->n == 4)
		fz_duff_4o4(sp, src->w * 4, dp, dst->w * 4, w, h);
	else if (src->n == dst->n)
		fz_duff_non(sp, src->w * src->n, src->n, dp, dst->w * dst->n, w, h);
	else
		assert(!"blendover src and dst mismatch");
}

static void
blendmaskover(fz_pixmap *src, fz_pixmap *msk, fz_pixmap *dst)
{
	unsigned char *sp, *dp, *mp;
	fz_bbox sr, dr, mr;
	int x, y, w, h;

	sr.x0 = src->x;
	sr.y0 = src->y;
	sr.x1 = src->x + src->w;
	sr.y1 = src->y + src->h;

	dr.x0 = dst->x;
	dr.y0 = dst->y;
	dr.x1 = dst->x + dst->w;
	dr.y1 = dst->y + dst->h;

	mr.x0 = msk->x;
	mr.y0 = msk->y;
	mr.x1 = msk->x + msk->w;
	mr.y1 = msk->y + msk->h;

	dr = fz_intersectbbox(sr, dr);
	dr = fz_intersectbbox(dr, mr);
	x = dr.x0;
	y = dr.y0;
	w = dr.x1 - dr.x0;
	h = dr.y1 - dr.y0;

	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	mp = msk->samples + ((y - msk->y) * msk->w + (x - msk->x)) * msk->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	if (src->n == 1 && msk->n == 1 && dst->n == 1)
		fz_duff_1i1o1(sp, src->w, mp, msk->w, dp, dst->w, w, h);
	else if (src->n == 4 && msk->n == 1 && dst->n == 4)
		fz_duff_4i1o4(sp, src->w * 4, mp, msk->w, dp, dst->w * 4, w, h);
	else if (src->n == dst->n)
		fz_duff_nimon(sp, src->w * src->n, src->n,
			mp, msk->w * msk->n, msk->n,
			dp, dst->w * dst->n, w, h);
	else
		assert(!"blendmaskover src and msk and dst mismatch");
}

static void
fz_drawfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3 / expansion;
	fz_bbox bbox;

	fz_resetgel(dev->gel, dev->scissor);
	fz_fillpath(dev->gel, path, ctm, flatness);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	if (fz_isemptyrect(bbox))
		return;

	if (dev->model)
	{
		unsigned char argb[4];
		float rgb[3];
		fz_convertcolor(colorspace, color, dev->model, rgb);
		argb[0] = alpha * 255;
		argb[1] = rgb[0] * 255;
		argb[2] = rgb[1] * 255;
		argb[3] = rgb[2] * 255;
		fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, dev->dest, argb, nil, nil);
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
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3 / expansion;
	float linewidth = stroke->linewidth;
	fz_bbox bbox;

	if (linewidth * expansion < 0.1)
		linewidth = 1.0 / expansion;

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

	if (dev->model)
	{
		unsigned char argb[4];
		float rgb[3];
		fz_convertcolor(colorspace, color, dev->model, rgb);
		argb[0] = alpha * 255;
		argb[1] = rgb[0] * 255;
		argb[2] = rgb[1] * 255;
		argb[3] = rgb[2] * 255;
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, argb, nil, nil);
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
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3 / expansion;
	fz_pixmap *mask, *dest;
	fz_bbox bbox;

	if (dev->cliptop == MAXCLIP)
	{
		fz_warn("assert: too many clip masks on stack");
		return;
	}

	fz_resetgel(dev->gel, dev->scissor);
	fz_fillpath(dev->gel, path, ctm, flatness);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	if (fz_isemptyrect(bbox) || fz_isrectgel(dev->gel))
	{
		dev->clipstack[dev->cliptop].scissor = dev->scissor;
		dev->clipstack[dev->cliptop].mask = nil;
		dev->clipstack[dev->cliptop].dest = nil;
		dev->scissor = bbox;
		dev->cliptop++;
		return;
	}

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(dev->model, bbox);

	memset(mask->samples, 0, mask->w * mask->h * mask->n);
	memset(dest->samples, 0, dest->w * dest->h * dest->n);

	fz_scanconvert(dev->gel, dev->ael, evenodd, bbox, mask, nil, nil, nil);

	dev->clipstack[dev->cliptop].scissor = dev->scissor;
	dev->clipstack[dev->cliptop].mask = mask;
	dev->clipstack[dev->cliptop].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->cliptop++;
}

static void
fz_drawclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	float expansion = fz_matrixexpansion(ctm);
	float flatness = 0.3 / expansion;
	float linewidth = stroke->linewidth;
	fz_pixmap *mask, *dest;
	fz_bbox bbox;

	if (dev->cliptop == MAXCLIP)
	{
		fz_warn("assert: too many clip masks on stack");
		return;
	}

	if (linewidth * expansion < 0.1)
		linewidth = 1.0 / expansion;

	fz_resetgel(dev->gel, dev->scissor);
	if (stroke->dashlen > 0)
		fz_dashpath(dev->gel, path, stroke, ctm, flatness, linewidth);
	else
		fz_strokepath(dev->gel, path, stroke, ctm, flatness, linewidth);
	fz_sortgel(dev->gel);

	bbox = fz_boundgel(dev->gel);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(dev->model, bbox);

	memset(mask->samples, 0, mask->w * mask->h * mask->n);
	memset(dest->samples, 0, dest->w * dest->h * dest->n);

	if (!fz_isemptyrect(bbox))
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, mask, nil, nil, nil);

	dev->clipstack[dev->cliptop].scissor = dev->scissor;
	dev->clipstack[dev->cliptop].mask = mask;
	dev->clipstack[dev->cliptop].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->cliptop++;
}

static void
drawglyph(unsigned char *argb, fz_pixmap *dst, fz_pixmap *src, int xorig, int yorig, fz_bbox scissor)
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
	if (x1 > dx1) { sx1 += dx1 - x1; x1 = dx1; }
	if (y1 > dy1) { sy1 += dy1 - y1; y1 = dy1; }

	sp = src->samples + (sy0 * src->w + sx0);
	dp = dst->samples + ((y0 - dst->y) * dst->w + (x0 - dst->x)) * dst->n;

	w = sx1 - sx0;
	h = sy1 - sy0;

	if (dst->colorspace)
		fz_text_w4i1o4(argb, sp, src->w, dp, dst->w * 4, w, h);
	else
		fz_text_1o1(sp, src->w, dp, dst->w, w, h);
}

static void
fz_drawfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_drawdevice *dev = user;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;
	unsigned char tmp[4];
	unsigned char *argb;

	if (dev->model)
	{
		float rgb[3];
		fz_convertcolor(colorspace, color, dev->model, rgb);
		tmp[0] = alpha * 255;
		tmp[1] = rgb[0] * 255;
		tmp[2] = rgb[1] * 255;
		tmp[3] = rgb[2] * 255;
		argb = tmp;
	}
	else
	{
		argb = nil;
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
		x = floor(trm.e);
		y = floor(trm.f);
		trm.e = QUANT(trm.e - floor(trm.e), HSUBPIX);
		trm.f = QUANT(trm.f - floor(trm.f), VSUBPIX);

		glyph = fz_renderglyph(dev->cache, text->font, gid, trm);
		if (glyph)
		{
			drawglyph(argb, dev->dest, glyph, x, y, dev->scissor);
			fz_droppixmap(glyph);
		}
	}
}

static void
fz_drawstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_warn("stroked text not implemented; filling instead");
	fz_drawfilltext(user, text, ctm, colorspace, color, alpha);
}

static void
fz_drawcliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_drawdevice *dev = user;
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
	fz_matrix tm, trm;
	fz_pixmap *glyph;
	int i, x, y, gid;

	/* If accumulate == 0 then this text object is guaranteed complete */
	/* If accumulate == 1 then this text object is the first (or only) in a sequence */
	/* If accumulate == 2 then this text object is a continuation */

	if (dev->cliptop == MAXCLIP)
	{
		fz_warn("assert: too many clip masks on stack");
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
		dest = fz_newpixmapwithrect(dev->model, bbox);

		memset(mask->samples, 0, mask->w * mask->h * mask->n);
		memset(dest->samples, 0, dest->w * dest->h * dest->n);

		dev->clipstack[dev->cliptop].scissor = dev->scissor;
		dev->clipstack[dev->cliptop].mask = mask;
		dev->clipstack[dev->cliptop].dest = dev->dest;
		dev->scissor = bbox;
		dev->dest = dest;
		dev->cliptop++;
	}
	else
	{
		mask = dev->clipstack[dev->cliptop-1].mask;
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
			x = floor(trm.e);
			y = floor(trm.f);
			trm.e = QUANT(trm.e - floor(trm.e), HSUBPIX);
			trm.f = QUANT(trm.f - floor(trm.f), VSUBPIX);

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
	fz_drawcliptext(user, text, ctm, 0);
}

static void
fz_drawignoretext(void *user, fz_text *text, fz_matrix ctm)
{
}

static void
fz_drawfillshade(void *user, fz_shade *shade, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_rect bounds;
	fz_bbox bbox;
	fz_pixmap *temp;
	float rgb[3];
	unsigned char argb[4];
	unsigned char *s;
	int n;

	bounds = fz_transformrect(fz_concat(shade->matrix, ctm), shade->bbox);
	bbox = fz_roundrect(bounds);
	bbox = fz_intersectbbox(bbox, dev->scissor);

	if (fz_isemptyrect(bbox))
		return;

	if (!dev->model)
	{
		fz_warn("cannot render shading directly to an alpha mask");
		return;
	}

	temp = fz_newpixmapwithrect(dev->model, bbox);

	if (shade->usebackground)
	{
		fz_convertcolor(shade->cs, shade->background, dev->model, rgb);
		argb[0] = 255;
		argb[1] = rgb[0] * 255;
		argb[2] = rgb[1] * 255;
		argb[3] = rgb[2] * 255;
		s = temp->samples;
		n = temp->w * temp->h;
		while (n--)
		{
			*s++ = argb[0];
			*s++ = argb[1];
			*s++ = argb[2];
			*s++ = argb[3];
		}
		blendover(temp, dev->dest);
	}

	fz_rendershade(shade, ctm, temp);
	blendover(temp, dev->dest);

	fz_droppixmap(temp);
}

static inline void
calcimagestate(fz_drawdevice *dev, fz_pixmap *image, fz_matrix ctm,
	fz_bbox *bbox, fz_matrix *invmat, int *dx, int *dy)
{
	float sx, sy;
	fz_path *path;
	fz_matrix mat;
	int w, h;

	sx = image->w / sqrt(ctm.a * ctm.a + ctm.b * ctm.b);
	sy = image->h / sqrt(ctm.c * ctm.c + ctm.d * ctm.d);

	if (sx < 1.0)
		*dx = 1;
	else
		*dx = sx;

	if (sy < 1.0)
		*dy = 1;
	else
		*dy = sy;

	w = image->w / *dx;
	h = image->h / *dy;

	path = fz_newpath();
	fz_moveto(path, 0.0, 0.0);
	fz_lineto(path, 1.0, 0.0);
	fz_lineto(path, 1.0, 1.0);
	fz_lineto(path, 0.0, 1.0);
	fz_closepath(path);

	fz_resetgel(dev->gel, dev->scissor);
	fz_fillpath(dev->gel, path, ctm, 1.0);
	fz_sortgel(dev->gel);

	fz_freepath(path);

	*bbox = fz_boundgel(dev->gel);
	*bbox = fz_intersectbbox(*bbox, dev->scissor);
	
	mat.a =  1.0 / w;
	mat.b = 0.0;
	mat.c = 0.0;
	mat.d =  -1.0 / h;
	mat.e = 0.0;
	mat.f = 1.0;
	*invmat = fz_invertmatrix(fz_concat(mat, ctm));
	invmat->e -= 0.5;
	invmat->f -= 0.5;
}

static void
fz_drawfillimage(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_drawdevice *dev = user;
	fz_bbox bbox;
	int dx, dy;
	fz_pixmap *scaled = nil;
	fz_pixmap *converted = nil;
	fz_matrix invmat;

	if (!dev->model)
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

	if (image->colorspace != dev->model)
	{
		converted = fz_newpixmap(dev->model, image->x, image->y, image->w, image->h);
		fz_convertpixmap(image->colorspace, image, dev->model, converted);
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
	fz_bbox bbox;
	int dx, dy;
	fz_pixmap *scaled = nil;
	fz_matrix invmat;

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
		unsigned char argb[4];
		float rgb[3];
		fz_convertcolor(colorspace, color, dev->model, rgb);
		argb[0] = alpha * 255;
		argb[1] = rgb[0] * 255;
		argb[2] = rgb[1] * 255;
		argb[3] = rgb[2] * 255;
		fz_scanconvert(dev->gel, dev->ael, 0, bbox, dev->dest, argb, image, &invmat);
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
	fz_bbox bbox;
	fz_pixmap *mask, *dest;
	int dx, dy;
	fz_pixmap *scaled = nil;
	fz_matrix invmat;

	if (dev->cliptop == MAXCLIP)
	{
		fz_warn("assert: too many clip masks on stack");
		return;
	}

	calcimagestate(dev, image, ctm, &bbox, &invmat, &dx, &dy);

	if (fz_isemptyrect(bbox) || image->w == 0 || image->h == 0)
	{
		dev->clipstack[dev->cliptop].scissor = dev->scissor;
		dev->clipstack[dev->cliptop].mask = nil;
		dev->clipstack[dev->cliptop].dest = nil;
		dev->scissor = bbox;
		dev->cliptop++;
		return;
	}

	if (dx != 1 || dy != 1)
	{
		scaled = fz_scalepixmap(image, dx, dy);
		image = scaled;
	}

	mask = fz_newpixmapwithrect(nil, bbox);
	dest = fz_newpixmapwithrect(dev->model, bbox);

	memset(mask->samples, 0, mask->w * mask->h * mask->n);
	memset(dest->samples, 0, dest->w * dest->h * dest->n);

	fz_scanconvert(dev->gel, dev->ael, 0, bbox, mask, nil, image, &invmat);

	dev->clipstack[dev->cliptop].scissor = dev->scissor;
	dev->clipstack[dev->cliptop].mask = mask;
	dev->clipstack[dev->cliptop].dest = dev->dest;
	dev->scissor = bbox;
	dev->dest = dest;
	dev->cliptop++;

	if (scaled)
		fz_droppixmap(scaled);
}

static void
fz_drawpopclip(void *user)
{
	fz_drawdevice *dev = user;
	fz_pixmap *mask, *dest;
	if (dev->cliptop > 0)
	{
		dev->cliptop--;
		dev->scissor = dev->clipstack[dev->cliptop].scissor;
		mask = dev->clipstack[dev->cliptop].mask;
		dest = dev->clipstack[dev->cliptop].dest;
		if (mask && dest)
		{
			fz_pixmap *scratch = dev->dest;
			blendmaskover(scratch, mask, dest);
			fz_droppixmap(mask);
			fz_droppixmap(scratch);
			dev->dest = dest;
		}
	}
}

static void
fz_drawfreeuser(void *user)
{
	fz_drawdevice *dev = user;
	if (dev->model)
		fz_dropcolorspace(dev->model);
	fz_freegel(dev->gel);
	fz_freeael(dev->ael);
	fz_free(dev);
}

fz_device *
fz_newdrawdevice(fz_glyphcache *cache, fz_pixmap *dest)
{
	fz_device *dev;
	fz_drawdevice *ddev = fz_malloc(sizeof(fz_drawdevice));
	if (dest->colorspace)
		ddev->model = fz_keepcolorspace(dest->colorspace);
	else
		ddev->model = nil;
	ddev->cache = cache;
	ddev->gel = fz_newgel();
	ddev->ael = fz_newael();
	ddev->dest = dest;
	ddev->cliptop = 0;

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

	return dev;
}
