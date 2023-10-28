// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include "color-imp.h"
#include "pixmap-imp.h"

#include <assert.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <float.h>

fz_pixmap *
fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	return fz_keep_storable(ctx, &pix->storable);
}

void
fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	fz_drop_storable(ctx, &pix->storable);
}

void
fz_drop_pixmap_imp(fz_context *ctx, fz_storable *pix_)
{
	fz_pixmap *pix = (fz_pixmap *)pix_;

	fz_drop_colorspace(ctx, pix->colorspace);
	fz_drop_separations(ctx, pix->seps);
	if (pix->flags & FZ_PIXMAP_FLAG_FREE_SAMPLES)
		fz_free(ctx, pix->samples);
	fz_drop_pixmap(ctx, pix->underlying);
	fz_free(ctx, pix);
}

fz_pixmap *
fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *colorspace, int w, int h, fz_separations *seps, int alpha, int stride, unsigned char *samples)
{
	fz_pixmap *pix;
	int s = fz_count_active_separations(ctx, seps);
	int n;

	if (w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal dimensions for pixmap %d %d", w, h);

	n = alpha + s + fz_colorspace_n(ctx, colorspace);
	if (stride < n*w && stride > -n*w)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal stride for pixmap (n=%d w=%d, stride=%d)", n, w, stride);
	if (samples == NULL && stride < n*w)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal -ve stride for pixmap without data");
	if (n > FZ_MAX_COLORS)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal number of colorants");

	pix = fz_malloc_struct(ctx, fz_pixmap);
	FZ_INIT_STORABLE(pix, 1, fz_drop_pixmap_imp);
	pix->x = 0;
	pix->y = 0;
	pix->w = w;
	pix->h = h;
	pix->alpha = alpha = !!alpha;
	pix->flags = FZ_PIXMAP_FLAG_INTERPOLATE;
	pix->xres = 96;
	pix->yres = 96;
	pix->colorspace = NULL;
	pix->n = n;
	pix->s = s;
	pix->seps = fz_keep_separations(ctx, seps);
	pix->stride = stride;

	if (colorspace)
	{
		pix->colorspace = fz_keep_colorspace(ctx, colorspace);
	}
	else
	{
		assert(alpha || s);
	}

	pix->samples = samples;
	if (!samples && pix->h > 0 && pix->w > 0)
	{
		fz_try(ctx)
		{
			if ((size_t)pix->stride > SIZE_MAX / (size_t)pix->h)
				fz_throw(ctx, FZ_ERROR_GENERIC, "Overly large image");
			pix->samples = Memento_label(fz_malloc(ctx, pix->h * pix->stride), "pixmap_data");
		}
		fz_catch(ctx)
		{
			fz_drop_separations(ctx, pix->seps);
			fz_drop_colorspace(ctx, pix->colorspace);
			fz_free(ctx, pix);
			fz_rethrow(ctx);
		}
		pix->flags |= FZ_PIXMAP_FLAG_FREE_SAMPLES;
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap(fz_context *ctx, fz_colorspace *colorspace, int w, int h, fz_separations *seps, int alpha)
{
	int stride;
	int s = fz_count_active_separations(ctx, seps);
	int n;
	if (!colorspace && s == 0) alpha = 1;
	n = fz_colorspace_n(ctx, colorspace) + s + alpha;
	if (w > INT_MAX / n)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Overly wide image");
	stride = n * w;
	return fz_new_pixmap_with_data(ctx, colorspace, w, h, seps, alpha, stride, NULL);
}

fz_pixmap *
fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *colorspace, fz_irect bbox, fz_separations *seps, int alpha)
{
	fz_pixmap *pixmap;
	pixmap = fz_new_pixmap(ctx, colorspace, fz_irect_width(bbox), fz_irect_height(bbox), seps, alpha);
	pixmap->x = bbox.x0;
	pixmap->y = bbox.y0;
	return pixmap;
}

fz_pixmap *
fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *colorspace, fz_irect bbox, fz_separations *seps, int alpha, unsigned char *samples)
{
	int w = fz_irect_width(bbox);
	int stride;
	int s = fz_count_active_separations(ctx, seps);
	fz_pixmap *pixmap;
	if (!colorspace && s == 0) alpha = 1;
	stride = (fz_colorspace_n(ctx, colorspace) + s + alpha) * w;
	pixmap = fz_new_pixmap_with_data(ctx, colorspace, w, fz_irect_height(bbox), seps, alpha, stride, samples);
	pixmap->x = bbox.x0;
	pixmap->y = bbox.y0;
	return pixmap;
}

fz_pixmap *fz_new_pixmap_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, const fz_irect *rect)
{
	fz_irect local_rect;
	fz_pixmap *subpix;

	if (!pixmap)
		return NULL;

	if (rect == NULL)
	{
		rect = &local_rect;
		local_rect.x0 = pixmap->x;
		local_rect.y0 = pixmap->y;
		local_rect.x1 = pixmap->x + pixmap->w;
		local_rect.y1 = pixmap->y + pixmap->h;
	}
	else if (rect->x0 < pixmap->x || rect->y0 < pixmap->y || rect->x1 > pixmap->x + pixmap->w || rect->y1 > pixmap->y + pixmap->h)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Pixmap region is not a subarea");

	subpix = fz_malloc_struct(ctx, fz_pixmap);
	*subpix = *pixmap;
	subpix->storable.refs = 1;
	subpix->x = rect->x0;
	subpix->y = rect->y0;
	subpix->w = fz_irect_width(*rect);
	subpix->h = fz_irect_height(*rect);
	subpix->samples += (rect->x0 - pixmap->x) + (rect->y0 - pixmap->y) * pixmap->stride;
	subpix->underlying = fz_keep_pixmap(ctx, pixmap);
	subpix->colorspace = fz_keep_colorspace(ctx, pixmap->colorspace);
	subpix->seps = fz_keep_separations(ctx, pixmap->seps);
	subpix->flags &= ~FZ_PIXMAP_FLAG_FREE_SAMPLES;

	return subpix;
}

fz_pixmap *fz_clone_pixmap(fz_context *ctx, const fz_pixmap *old)
{
	fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, old->colorspace, fz_make_irect(old->x, old->y, old->w, old->h), old->seps, old->alpha);
	memcpy(pix->samples, old->samples, pix->stride * pix->h);
	return pix;
}

fz_irect
fz_pixmap_bbox(fz_context *ctx, const fz_pixmap *pix)
{
	fz_irect bbox;
	bbox.x0 = pix->x;
	bbox.y0 = pix->y;
	bbox.x1 = pix->x + pix->w;
	bbox.y1 = pix->y + pix->h;
	return bbox;
}

fz_irect
fz_pixmap_bbox_no_ctx(const fz_pixmap *pix)
{
	fz_irect bbox;
	bbox.x0 = pix->x;
	bbox.y0 = pix->y;
	bbox.x1 = pix->x + pix->w;
	bbox.y1 = pix->y + pix->h;
	return bbox;
}

fz_colorspace *
fz_pixmap_colorspace(fz_context *ctx, const fz_pixmap *pix)
{
	if (!pix)
		return NULL;
	return pix->colorspace;
}

int
fz_pixmap_x(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->x;
}

int
fz_pixmap_y(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->y;
}

int
fz_pixmap_width(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->w;
}

int
fz_pixmap_height(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->h;
}

int
fz_pixmap_components(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->n;
}

int
fz_pixmap_colorants(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->n - pix->alpha - pix->s;
}

int
fz_pixmap_spots(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->s;
}

int
fz_pixmap_alpha(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->alpha;
}

int
fz_pixmap_stride(fz_context *ctx, const fz_pixmap *pix)
{
	return pix->stride;
}

unsigned char *
fz_pixmap_samples(fz_context *ctx, const fz_pixmap *pix)
{
	if (!pix)
		return NULL;
	return pix->samples;
}

/*
	The slowest routine in most CMYK rendering profiles.
	We therefore spend some effort to improve it. Rather than
	writing bytes, we write uint32_t's.
*/
#ifdef ARCH_ARM
static void
clear_cmyka_bitmap_ARM(uint32_t *samples, int c, int value)
__attribute__((naked));

static void
clear_cmyka_bitmap_ARM(uint32_t *samples, int c, int value)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r4-r6,r14}					\n"
	"@ r0 = samples							\n"
	"@ r1 = c							\n"
	"@ r2 = value							\n"
	"mov	r3, #255						\n"
	"mov	r12,#0			@ r12= 0			\n"
	"subs	r1, r1, #3						\n"
	"ble	2f							\n"
	"str	r12,[r13,#-20]!						\n"
	"str	r12,[r13,#4]						\n"
	"str	r12,[r13,#8]						\n"
	"str	r12,[r13,#12]						\n"
	"str	r12,[r13,#16]						\n"
	"strb	r2, [r13,#3]						\n"
	"strb	r3, [r13,#4]						\n"
	"strb	r2, [r13,#8]						\n"
	"strb	r3, [r13,#9]						\n"
	"strb	r2, [r13,#13]						\n"
	"strb	r3, [r13,#14]						\n"
	"strb	r2, [r13,#18]						\n"
	"strb	r3, [r13,#19]						\n"
	"ldmfd	r13!,{r4,r5,r6,r12,r14}					\n"
	"1:								\n"
	"stmia	r0!,{r4,r5,r6,r12,r14}					\n"
	"subs	r1, r1, #4						\n"
	"bgt	1b							\n"
	"2:								\n"
	"adds	r1, r1, #3						\n"
	"ble	4f							\n"
	"3:								\n"
	"strb	r12,[r0], #1						\n"
	"strb	r12,[r0], #1						\n"
	"strb	r12,[r0], #1						\n"
	"strb	r2, [r0], #1						\n"
	"strb	r3, [r0], #1						\n"
	"subs	r1, r1, #1						\n"
	"bgt	3b							\n"
	"4:								\n"
	"ldmfd	r13!,{r4-r6,PC}						\n"
	ENTER_THUMB
	);
}
#endif

static void
clear_cmyk_bitmap(unsigned char *samples, int w, int h, int spots, int stride, int value, int alpha)
{
	uint32_t *s = (uint32_t *)(void *)samples;
	uint8_t *t;

	if (w < 0 || h < 0)
		return;

	if (spots)
	{
		int x, i;
		spots += 4;
		stride -= w * (spots + alpha);
		for (; h > 0; h--)
		{
			for (x = w; x > 0; x--)
			{
				for (i = spots; i > 0; i--)
					*samples++ = value;
				if (alpha)
					*samples++ = 255;
			}
			samples += stride;
		}
		return;
	}

	if (alpha)
	{
		int c = w;
		stride -= w*5;
		if (stride == 0)
		{
#ifdef ARCH_ARM
			clear_cmyka_bitmap_ARM(s, c, alpha);
			return;
#else
			/* We can do it all fast (except for maybe a few stragglers) */
			union
			{
				uint8_t bytes[20];
				uint32_t words[5];
			} d;

			c *= h;
			h = 1;

			d.words[0] = 0;
			d.words[1] = 0;
			d.words[2] = 0;
			d.words[3] = 0;
			d.words[4] = 0;
			d.bytes[3] = value;
			d.bytes[4] = 255;
			d.bytes[8] = value;
			d.bytes[9] = 255;
			d.bytes[13] = value;
			d.bytes[14] = 255;
			d.bytes[18] = value;
			d.bytes[19] = 255;

			c -= 3;
			{
				const uint32_t a0 = d.words[0];
				const uint32_t a1 = d.words[1];
				const uint32_t a2 = d.words[2];
				const uint32_t a3 = d.words[3];
				const uint32_t a4 = d.words[4];
				while (c > 0)
				{
					*s++ = a0;
					*s++ = a1;
					*s++ = a2;
					*s++ = a3;
					*s++ = a4;
					c -= 4;
				}
			}
			c += 3;
#endif
		}
		t = (unsigned char *)s;
		w = c;
		while (h--)
		{
			c = w;
			while (c > 0)
			{
				*t++ = 0;
				*t++ = 0;
				*t++ = 0;
				*t++ = value;
				*t++ = 255;
				c--;
			}
			t += stride;
		}
	}
	else
	{
		stride -= w*4;
		if ((stride & 3) == 0)
		{
			size_t W = w;
			if (stride == 0)
			{
				W *= h;
				h = 1;
			}
			W *= 4;
			if (value == 0)
			{
				while (h--)
				{
					memset(s, 0, W);
					s += (stride>>2);
				}
			}
			else
			{
				/* We can do it all fast */
				union
				{
					uint8_t bytes[4];
					uint32_t word;
				} d;

				d.word = 0;
				d.bytes[3] = value;
				{
					const uint32_t a0 = d.word;
					while (h--)
					{
						size_t WW = W >> 2;
						while (WW--)
						{
							*s++ = a0;
						}
						s += (stride>>2);
					}
				}
			}
		}
		else
		{
			t = (unsigned char *)s;
			while (h--)
			{
				int c = w;
				while (c > 0)
				{
					*t++ = 0;
					*t++ = 0;
					*t++ = 0;
					*t++ = value;
					c--;
				}
				t += stride;
			}
		}
	}
}

void
fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	ptrdiff_t stride = pix->w * (ptrdiff_t)pix->n;
	int h = pix->h;
	unsigned char *s = pix->samples;
	if (stride == pix->stride)
	{
		stride *= h;
		h = 1;
	}
	if (pix->alpha || fz_colorspace_is_subtractive(ctx, pix->colorspace))
	{
		while (h--)
		{
			memset(s, 0, stride);
			s += pix->stride;
		}
	}
	else if (pix->s == 0)
	{
		while (h--)
		{
			memset(s, 0xff, stride);
			s += pix->stride;
		}
	}
	else
	{
		/* Horrible, slow case: additive with spots */
		size_t w = stride/pix->n;
		int spots = pix->s;
		int colorants = pix->n - spots; /* We know there is no alpha */
		while (h--)
		{
			size_t w2 = w;
			while (w2--)
			{
				int i = colorants;
				do
				{
					*s++ = 0xff;
					i--;
				}
				while (i != 0);

				i = spots;
				do
				{
					*s++ = 0;
					i--;
				}
				while (i != 0);
			}
		}
	}
}

void
fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value)
{
	unsigned char *s;
	int w, h, n;
	ptrdiff_t stride, len;
	int alpha = pix->alpha;

	w = pix->w;
	h = pix->h;
	if (w < 0 || h < 0)
		return;

	/* CMYK needs special handling (and potentially any other subtractive colorspaces) */
	if (fz_colorspace_n(ctx, pix->colorspace) == 4)
	{
		clear_cmyk_bitmap(pix->samples, w, h, pix->s, pix->stride, 255-value, pix->alpha);
		return;
	}

	n = pix->n;
	stride = pix->stride;
	len = (ptrdiff_t)w * n;

	s = pix->samples;
	if (value == 255 || !alpha)
	{
		if (stride == len)
		{
			len *= h;
			h = 1;
		}
		while (h--)
		{
			memset(s, value, len);
			s += stride;
		}
	}
	else
	{
		int k, x, y;
		stride -= len;
		for (y = 0; y < pix->h; y++)
		{
			for (x = 0; x < pix->w; x++)
			{
				for (k = 0; k < pix->n - 1; k++)
					*s++ = value;
				if (alpha)
					*s++ = 255;
			}
			s += stride;
		}
	}
}

void
fz_fill_pixmap_with_color(fz_context *ctx, fz_pixmap *pix, fz_colorspace *colorspace, float *color, fz_color_params color_params)
{
	float colorfv[FZ_MAX_COLORS];
	unsigned char colorbv[FZ_MAX_COLORS];
	int i, n, a, s, x, y, w, h;

	n = fz_colorspace_n(ctx, pix->colorspace);
	a = pix->alpha;
	s = pix->s;
	fz_convert_color(ctx, colorspace, color, pix->colorspace, colorfv, NULL, color_params);
	for (i = 0; i < n; ++i)
		colorbv[i] = colorfv[i] * 255;

	w = pix->w;
	h = pix->h;
	for (y = 0; y < h; ++y)
	{
		unsigned char *p = pix->samples + y * pix->stride;
		for (x = 0; x < w; ++x)
		{
			for (i = 0; i < n; ++i)
				*p++ = colorbv[i];
			for (i = 0; i < s; ++i)
				*p++ = 0;
			if (a)
				*p++ = 255;
		}
	}
}

void
fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_irect b, const fz_default_colorspaces *default_cs)
{
	unsigned char *srcp;
	unsigned char *destp;
	unsigned int y, w;
	size_t destspan, srcspan;

	b = fz_intersect_irect(b, fz_pixmap_bbox(ctx, dest));
	b = fz_intersect_irect(b, fz_pixmap_bbox(ctx, src));
	if (fz_is_empty_irect(b))
		return;
	w = (unsigned int)(b.x1 - b.x0);
	y = (unsigned int)(b.y1 - b.y0);

	srcspan = src->stride;
	srcp = src->samples + srcspan * (b.y0 - src->y) + (b.x0 - src->x) * (size_t)src->n;
	destspan = dest->stride;
	destp = dest->samples + destspan * (b.y0 - dest->y) + (b.x0 - dest->x) * (size_t)dest->n;

	if (src->n == dest->n)
	{
		w *= src->n;
		do
		{
			memcpy(destp, srcp, w);
			srcp += srcspan;
			destp += destspan;
		}
		while (--y);
	}
	else
	{
		fz_pixmap fake_src = *src;
		fake_src.x = b.x0;
		fake_src.y = b.y0;
		fake_src.w = w;
		fake_src.h = y;
		fake_src.samples = srcp;
		fz_convert_pixmap_samples(ctx, &fake_src, dest, NULL, default_cs, fz_default_color_params, 0);
	}
}

void
fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *dest, int value, fz_irect b)
{
	unsigned char *destp;
	int x, y, w, k;
	size_t destspan;

	b = fz_intersect_irect(b, fz_pixmap_bbox(ctx, dest));
	w = b.x1 - b.x0;
	y = b.y1 - b.y0;
	if (w <= 0 || y <= 0)
		return;

	destspan = dest->stride;
	destp = dest->samples + destspan * (b.y0 - dest->y) + (b.x0 - dest->x) * (size_t)dest->n;

	/* CMYK needs special handling (and potentially any other subtractive colorspaces) */
	if (fz_colorspace_n(ctx, dest->colorspace) == 4)
	{
		value = 255 - value;
		do
		{
			unsigned char *s = destp;
			for (x = 0; x < w; x++)
			{
				*s++ = 0;
				*s++ = 0;
				*s++ = 0;
				*s++ = value;
				*s++ = 255;
			}
			destp += destspan;
		}
		while (--y);
		return;
	}

	if (value == 255)
	{
		do
		{
			memset(destp, 255, w * (size_t)dest->n);
			destp += destspan;
		}
		while (--y);
	}
	else
	{
		do
		{
			unsigned char *s = destp;
			for (x = 0; x < w; x++)
			{
				for (k = 0; k < dest->n - 1; k++)
					*s++ = value;
				*s++ = 255;
			}
			destp += destspan;
		}
		while (--y);
	}
}

void
fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	unsigned char a;
	int k, x, y;
	size_t stride = pix->stride - pix->w * (size_t)pix->n;

	if (!pix->alpha)
		return;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			a = s[pix->n - 1];
			for (k = 0; k < pix->n - 1; k++)
				s[k] = fz_mul255(s[k], a);
			s += pix->n;
		}
		s += stride;
	}
}

fz_pixmap *
fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray)
{
	fz_pixmap *alpha;
	unsigned char *sp, *dp;
	int w, h, sstride, dstride;

	assert(gray->n == 1);

	alpha = fz_new_pixmap_with_bbox(ctx, NULL, fz_pixmap_bbox(ctx, gray), 0, 1);
	dp = alpha->samples;
	dstride = alpha->stride;
	sp = gray->samples;
	sstride = gray->stride;

	h = gray->h;
	w = gray->w;
	while (h--)
	{
		memcpy(dp, sp, w);
		sp += sstride;
		dp += dstride;
	}

	return alpha;
}

void
fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int black, int white)
{
	unsigned char *s = pix->samples;
	int n = pix->n;
	int x, y, save;
	int rb = (black>>16)&255;
	int gb = (black>>8)&255;
	int bb = (black)&255;
	int rw = (white>>16)&255;
	int gw = (white>>8)&255;
	int bw = (white)&255;
	int rm = (rw - rb);
	int gm = (gw - gb);
	int bm = (bw - bb);

	switch (fz_colorspace_type(ctx, pix->colorspace))
	{
	case FZ_COLORSPACE_GRAY:
		gw = (rw + gw + bw) / 3;
		gb = (rb + gb + bb) / 3;
		gm = gw - gb;
		for (y = 0; y < pix->h; y++)
		{
			for (x = 0; x < pix->w; x++)
			{
				*s = gb + fz_mul255(*s, gm);
				s += n;
			}
			s += pix->stride - pix->w * n;
		}
		break;

	case FZ_COLORSPACE_BGR:
		save = rm; rm = bm; bm = save;
		save = rb; rb = bb; bb = save;
		/* fall through */
	case FZ_COLORSPACE_RGB:
		for (y = 0; y < pix->h; y++)
		{
			for (x = 0; x < pix->w; x++)
			{
				s[0] = rb + fz_mul255(s[0], rm);
				s[1] = gb + fz_mul255(s[1], gm);
				s[2] = bb + fz_mul255(s[2], bm);
				s += n;
			}
			s += pix->stride - pix->w * n;
		}
		break;

	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "can only tint RGB, BGR and Gray pixmaps");
		break;
	}
}

/* Invert luminance in RGB/BGR pixmap, but keep the colors as is. */
static inline void invert_luminance(int type, unsigned char *s)
{
	int r, g, b, y;

	/* Convert to YUV */
	if (type == FZ_COLORSPACE_RGB)
	{
		r = s[0];
		g = s[1];
		b = s[2];
	}
	else
	{
		r = s[2];
		g = s[1];
		b = s[0];
	}

	y = (39336 * r + 76884 * g + 14900 * b + 32768)>>16;
	y = 259-y;
	r += y;
	g += y;
	b += y;

	if (type == FZ_COLORSPACE_RGB)
	{
		s[0] = r > 255 ? 255 : r < 0 ? 0 : r;
		s[1] = g > 255 ? 255 : g < 0 ? 0 : g;
		s[2] = b > 255 ? 255 : b < 0 ? 0 : b;
	}
	else
	{
		s[2] = r > 255 ? 255 : r < 0 ? 0 : r;
		s[1] = g > 255 ? 255 : g < 0 ? 0 : g;
		s[0] = b > 255 ? 255 : b < 0 ? 0 : b;
	}
}

void
fz_invert_pixmap_luminance(fz_context *ctx, fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	int x, y, n = pix->n;
	int type = pix->colorspace ? pix->colorspace->type : FZ_COLORSPACE_NONE;

	if (type == FZ_COLORSPACE_GRAY)
	{
		fz_invert_pixmap(ctx, pix);
	}
	else if (type == FZ_COLORSPACE_RGB || type == FZ_COLORSPACE_BGR)
	{
		for (y = 0; y < pix->h; y++)
		{
			for (x = 0; x < pix->w; x++)
			{
				invert_luminance(type, s);
				s += n;
			}
			s += pix->stride - pix->w * n;
		}
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "can only invert luminance of Gray and RGB pixmaps");
	}
}

void
fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	fz_irect rect = { pix->x, pix->y, pix->x + pix->w, pix->y + pix->h };
	fz_invert_pixmap_rect(ctx, pix, rect);
}

void
fz_invert_pixmap_alpha(fz_context *ctx, fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	int x, y;
	int n1 = pix->n - pix->alpha;
	int n = pix->n;

	if (!pix->alpha)
		return;

	for (y = 0; y < pix->h; y++)
	{
		s += n1;
		for (x = 0; x < pix->w; x++)
		{
			*s = 255 - *s;
			s += n;
		}
		s += pix->stride - pix->w * n;
	}
}

void fz_invert_pixmap_rect(fz_context *ctx, fz_pixmap *pix, fz_irect rect)
{
	int x0 = fz_clampi(rect.x0 - pix->x, 0, pix->w);
	int x1 = fz_clampi(rect.x1 - pix->x, 0, pix->w);
	int y0 = fz_clampi(rect.y0 - pix->y, 0, pix->h);
	int y1 = fz_clampi(rect.y1 - pix->y, 0, pix->h);

	int k, x, y;
	int n = pix->n;
	int s = pix->s;
	int cmyk = (pix->colorspace && pix->colorspace->type == FZ_COLORSPACE_CMYK);

	if (cmyk)
	{
		/* For cmyk, we're storing: (a.c, a.m, a.y, a.k, a)
		 * So, a.r = a - a.c - a.k
		 *     a.g = a - a.m - a.k
		 *     a.b = a - a.y - a.k
		 * Invert that:
		 *     a.R = a.c + a.k
		 *     a.G = a.m + a.k
		 *     a.B = a.y + a.k
		 * Convert that back to cmy
		 *     a.C = a - a.c - a.k;
		 *     a.M = a - a.m - a.k;
		 *     a.Y = a - a.y - a.k;
		 * Extract K:
		 *     a.K' = min(a.C, a.M, a.Y)
		 *          = a - a.k - max(a.c, a.m, a.y)
		 *     a.C' = a.C - a.K' = a - a.c - a.k - (a - a.k - max(a.c, a.m, a.y)) = max(a.c, a.m, a.y) - a.c
		 *     a.M' = a.M - a.K' = a - a.m - a.k - (a - a.k - max(a.c, a.m, a.y)) = max(a.c, a.m, a.y) - a.m
		 *     a.Y' = a.Y - a.K' = a - a.y - a.k - (a - a.k - max(a.c, a.m, a.y)) = max(a.c, a.m, a.y) - a.y
		 * */
		if (pix->alpha)
		{
			int n1 = pix->n - pix->alpha - s;
			for (y = y0; y < y1; y++)
			{
				unsigned char *d = pix->samples + ((y * (size_t)pix->stride) + (x0 * (size_t)pix->n));
				for (x = x0; x < x1; x++)
				{
					int ac = d[0];
					int am = d[1];
					int ay = d[2];
					int ak = d[3];
					int a = d[n1];
					int mx = fz_maxi(fz_maxi(ac, am), ay);
					d[0] = mx-ac;
					d[1] = mx-am;
					d[2] = mx-ay;
					ak = a - ak - mx;
					if (ak < 0)
						ak = 0;
					d[3] = ak;
					d += n;
				}
			}
		}
		else
		{
			for (y = y0; y < y1; y++)
			{
				unsigned char *d = pix->samples + ((y * (size_t)pix->stride) + (x0 * (size_t)pix->n));
				for (x = x0; x < x1; x++)
				{
					int c = d[0];
					int m = d[1];
					int y = d[2];
					int k = d[3];
					int mx = fz_maxi(fz_maxi(c, m), y);
					d[0] = mx-c;
					d[1] = mx-m;
					d[2] = mx-y;
					k = 255 - k - mx;
					if (k < 0)
						k = 0;
					d[3] = k;
					d += n;
				}
			}
		}
	}
	else if (pix->alpha)
	{
		int n1 = pix->n - pix->alpha - s;
		for (y = y0; y < y1; y++)
		{
			unsigned char *d = pix->samples + ((y * (size_t)pix->stride) + (x0 * (size_t)pix->n));
			for (x = x0; x < x1; x++)
			{
				int a = d[n1];
				for (k = 0; k < n1; k++)
					d[k] = a - d[k];
				d += n;
			}
		}
	}
	else if (s)
	{
		int n1 = pix->n - s;
		for (y = y0; y < y1; y++)
		{
			unsigned char *d = pix->samples + ((y * (size_t)pix->stride) + (x0 * (size_t)pix->n));
			for (x = x0; x < x1; x++)
			{
				for (k = 0; k < n1; k++)
					d[k] = 255 - d[k];
				d += n;
			}
		}
	}
	else
	{
		for (y = y0; y < y1; y++)
		{
			unsigned char *d = pix->samples + ((y * (size_t)pix->stride) + (x0 * (size_t)pix->n));
			for (x = x0; x < x1; x++)
			{
				for (k = 0; k < n; k++)
					d[k] = 255 - d[k];
				d += n;
			}
		}
	}
}

void
fz_invert_pixmap_raw(fz_context *ctx, fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	int k, x, y;
	int n1 = pix->n - pix->alpha;
	int n = pix->n;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			for (k = 0; k < n1; k++)
				s[k] = 255 - s[k];
			s += n;
		}
		s += pix->stride - pix->w * n;
	}
}

void
fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma)
{
	unsigned char gamma_map[256];
	unsigned char *s = pix->samples;
	int n1 = pix->n - pix->alpha;
	int n = pix->n;
	int k, x, y;

	for (k = 0; k < 256; k++)
		gamma_map[k] = powf(k / 255.0f, gamma) * 255;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			for (k = 0; k < n1; k++)
				s[k] = gamma_map[s[k]];
			s += n;
		}
		s += pix->stride - pix->w * n;
	}
}

size_t
fz_pixmap_size(fz_context *ctx, fz_pixmap * pix)
{
	if (pix == NULL)
		return 0;
	return sizeof(*pix) + (size_t)pix->n * pix->w * pix->h;
}

fz_pixmap *
fz_convert_pixmap(fz_context *ctx, const fz_pixmap *pix, fz_colorspace *ds, fz_colorspace *prf, fz_default_colorspaces *default_cs, fz_color_params color_params, int keep_alpha)
{
	fz_pixmap *cvt;

	if (!ds && !keep_alpha)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot both throw away and keep alpha");

	cvt = fz_new_pixmap(ctx, ds, pix->w, pix->h, pix->seps, keep_alpha && pix->alpha);

	cvt->xres = pix->xres;
	cvt->yres = pix->yres;
	cvt->x = pix->x;
	cvt->y = pix->y;
	if (pix->flags & FZ_PIXMAP_FLAG_INTERPOLATE)
		cvt->flags |= FZ_PIXMAP_FLAG_INTERPOLATE;
	else
		cvt->flags &= ~FZ_PIXMAP_FLAG_INTERPOLATE;

	fz_try(ctx)
	{
		fz_convert_pixmap_samples(ctx, pix, cvt, prf, default_cs, color_params, 1);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, cvt);
		fz_rethrow(ctx);
	}

	return cvt;
}

fz_pixmap *
fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span)
{
	fz_pixmap *pixmap = fz_new_pixmap(ctx, NULL, w, h, NULL, 1);
	int stride = pixmap->stride;
	unsigned char *s = pixmap->samples;
	pixmap->x = x;
	pixmap->y = y;

	for (y = 0; y < h; y++)
	{
		memcpy(s, sp + y * span, w);
		s += stride;
	}

	return pixmap;
}

fz_pixmap *
fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span)
{
	fz_pixmap *pixmap = fz_new_pixmap(ctx, NULL, w, h, NULL, 1);
	int stride = pixmap->stride - pixmap->w;
	pixmap->x = x;
	pixmap->y = y;

	for (y = 0; y < h; y++)
	{
		unsigned char *out = pixmap->samples + y * w;
		unsigned char *in = sp + y * span;
		unsigned char bit = 0x80;
		int ww = w;
		while (ww--)
		{
			*out++ = (*in & bit) ? 255 : 0;
			bit >>= 1;
			if (bit == 0)
				bit = 0x80, in++;
		}
		out += stride;
	}

	return pixmap;
}

static float
calc_percentile(int *hist, float thr, float scale, float minval, float maxval)
{
	float prct;
	int k = 0, count = 0;

	while (count < thr)
		count += hist[k++];

	if (k <= 0)
		prct = k;
	else
	{
		float c0 = count - thr;
		float c1 = thr - (count - hist[k - 1]);
		prct = (c1 * k + c0 * (k - 1)) / (c0 + c1);
	}

	prct /= scale;
	prct += minval;
	return fz_clamp(prct, minval, maxval);
}

static void
calc_percentiles(fz_context *ctx, int nsamples, float *samples, float *minprct, float *maxprct)
{
	float minval, maxval, scale;
	int *hist, size, k;

	minval = maxval = samples[0];
	for (k = 1; k < nsamples; k++)
	{
		minval = fz_min(minval, samples[k]);
		maxval = fz_max(maxval, samples[k]);
	}

	if (minval - maxval == 0)
	{
		*minprct = *maxprct = minval;
		return;
	}

	size = fz_mini(65535, nsamples);
	scale = (size - 1) / (maxval - minval);

	hist = fz_calloc(ctx, size, sizeof(int));

	fz_try(ctx)
	{
		for (k = 0; k < nsamples; k++)
			hist[(uint16_t) (scale * (samples[k] - minval))]++;

		*minprct = calc_percentile(hist, 0.01f * nsamples, scale, minval, maxval);
		*maxprct = calc_percentile(hist, 0.99f * nsamples, scale, minval, maxval);
	}
	fz_always(ctx)
		fz_free(ctx, hist);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* Tone mapping according to "Consistent Tone Reproduction" by Min H. Kim and Jan Kautz. */
fz_pixmap *
fz_new_pixmap_from_float_data(fz_context *ctx, fz_colorspace *cs, int w, int h, float *samples)
{
	fz_pixmap *pixmap = NULL;
	unsigned char *dp;
	float *sample;
	float *lsamples = NULL;
	float minsample, maxsample, mu;
	float k1, d0, sigma, sigmasq2;
	float minprct, maxprct, range;
	int y, k, n = fz_colorspace_n(ctx, cs);
	int nsamples = w * h * n;
#define KIMKAUTZC1 (3.0f)
#define KIMKAUTZC2 (0.5f)
#define MAXLD (logf(300.0f))
#define MINLD (logf(0.3f))

	fz_var(pixmap);
	fz_var(lsamples);

	fz_try(ctx)
	{
		lsamples = fz_malloc(ctx, nsamples * sizeof(float));

		mu = 0;
		minsample = FLT_MAX;
		maxsample = -FLT_MAX;

		for (k = 0; k < nsamples; k++)
		{
			lsamples[k] = logf(samples[k] == 0 ? FLT_MIN : samples[k]);
			mu += lsamples[k];
			minsample = fz_min(minsample, lsamples[k]);
			maxsample = fz_max(maxsample, lsamples[k]);
		}

		mu /= nsamples;
		d0 = maxsample - minsample;
		k1 = (MAXLD - MINLD) / d0;
		sigma = d0 / KIMKAUTZC1;
		sigmasq2 = sigma * sigma * 2;

		for (k = 0; k < nsamples; k++)
		{
			float samplemu = samples[k] - mu;
			float samplemu2 = samplemu * samplemu;
			float fw = expf(-samplemu2 / sigmasq2);
			float k2 = (1 - k1) * fw + k1;
			samples[k] = expf(KIMKAUTZC2 * k2 * (lsamples[k] - mu) + mu);
		}

		calc_percentiles(ctx, nsamples, samples, &minprct, &maxprct);
		range = maxprct - minprct;

		pixmap = fz_new_pixmap(ctx, cs, w, h, NULL, 0);

		dp = pixmap->samples + pixmap->stride * (h - 1);
		sample = samples;

		for (y = 0; y < h; y++)
		{
			unsigned char *dpp = dp;

			for (k = 0; k < w * n; k++)
				*dpp++ = 255.0f * (fz_clamp(*sample++, minprct, maxprct) - minprct) / range;

			dp -= pixmap->stride;
		}
	}
	fz_always(ctx)
		fz_free(ctx, lsamples);
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pixmap);
		fz_rethrow(ctx);
	}

	return pixmap;
}

fz_pixmap *
fz_new_pixmap_from_alpha_channel(fz_context *ctx, fz_pixmap *src)
{
	fz_pixmap *dst;
	int w, h, n, x;
	unsigned char *sp, *dp;

	if (!src->alpha)
		return NULL;

	dst = fz_new_pixmap_with_bbox(ctx, NULL, fz_pixmap_bbox(ctx, src), NULL, 1);
	w = src->w;
	h = src->h;
	n = src->n;
	sp = src->samples + n - 1;
	dp = dst->samples;

	while (h--)
	{
		unsigned char *s = sp;
		unsigned char *d = dp;
		for (x = 0; x < w; ++x)
		{
			*d++ = *s;
			s += n;
		}
		sp += src->stride;
		dp += dst->stride;
	}

	return dst;
}

fz_pixmap *
fz_new_pixmap_from_color_and_mask(fz_context *ctx, fz_pixmap *color, fz_pixmap *mask)
{
	fz_pixmap *dst;
	int w = color->w;
	int h = color->h;
	int n = color->n;
	int x, y, k;

	if (color->alpha)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color pixmap must not have an alpha channel");
	if (mask->n != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "mask pixmap must have exactly one channel");
	if (mask->w != color->w || mask->h != color->h)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color and mask pixmaps must be the same size");

	dst = fz_new_pixmap_with_bbox(ctx, color->colorspace, fz_pixmap_bbox(ctx, color), NULL, 1);

	for (y = 0; y < h; ++y)
	{
		unsigned char *cs = &color->samples[y * color->stride];
		unsigned char *ms = &mask->samples[y * mask->stride];
		unsigned char *ds = &dst->samples[y * dst->stride];
		for (x = 0; x < w; ++x)
		{
			unsigned char a = *ms++;
			for (k = 0; k < n; ++k)
				*ds++ = fz_mul255(*cs++, a);
			*ds++ = a;
		}
	}

	return dst;
}

int
fz_is_pixmap_monochrome(fz_context *ctx, fz_pixmap *pixmap)
{
	int n = pixmap->n;
	int w = pixmap->w;
	int h = pixmap->h;
	unsigned char *s = pixmap->samples;
	int x;

	if (n != 1)
		return 0;

	while (h--)
	{
		for (x = 0; x < w; ++x)
		{
			unsigned char v = s[x];
			if (v != 0 && v != 255)
				return 0;
		}
		s += pixmap->stride;
	}

	return 1;
}

#ifdef ARCH_ARM
static void
fz_subsample_pixmap_ARM(unsigned char *ptr, int w, int h, int f, int factor,
			int n, int fwd, int back, int back2, int fwd2,
			int divX, int back4, int fwd4, int fwd3,
			int divY, int back5, int divXY)
__attribute__((naked));

static void
fz_subsample_pixmap_ARM(unsigned char *ptr, int w, int h, int f, int factor,
			int n, int fwd, int back, int back2, int fwd2,
			int divX, int back4, int fwd4, int fwd3,
			int divY, int back5, int divXY)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r1,r4-r11,r14}					\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"@ r0 = src = ptr						\n"
	"@ r1 = w							\n"
	"@ r2 = h							\n"
	"@ r3 = f							\n"
	"mov	r9, r0			@ r9 = dst = ptr		\n"
	"ldr	r6, [r13,#4*12]		@ r6 = fwd			\n"
	"ldr	r7, [r13,#4*13]		@ r7 = back			\n"
	"subs	r2, r2, r3		@ r2 = h -= f			\n"
	"blt	12f			@ Skip if less than a full row	\n"
	"1:				@ for (y = h; y > 0; y--) {	\n"
	"ldr	r1, [r13]		@ r1 = w			\n"
	"subs	r1, r1, r3		@ r1 = w -= f			\n"
	"blt	6f			@ Skip if less than a full col	\n"
	"ldr	r4, [r13,#4*10]		@ r4 = factor			\n"
	"ldr	r8, [r13,#4*14]		@ r8 = back2			\n"
	"ldr	r12,[r13,#4*15]		@ r12= fwd2			\n"
	"2:				@ for (x = w; x > 0; x--) {	\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"3:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r3, LSL #8	@ for (xx = f; xx > 0; x--) {	\n"
	"4:				@				\n"
	"add	r5, r5, r3, LSL #16	@ for (yy = f; yy > 0; y--) {	\n"
	"5:				@				\n"
	"ldrb	r11,[r0], r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	5b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	4b			@ }				\n"
	"mov	r14,r14,LSR r4		@ r14 = v >>= factor		\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back2			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	3b			@ }				\n"
	"add	r0, r0, r12		@ s += fwd2			\n"
	"subs	r1, r1, r3		@ x -= f			\n"
	"bge	2b			@ }				\n"
	"6:				@ Less than a full column left	\n"
	"adds	r1, r1, r3		@ x += f			\n"
	"beq	11f			@ if (x == 0) next row		\n"
	"@ r0 = src							\n"
	"@ r1 = x							\n"
	"@ r2 = y							\n"
	"@ r3 = f							\n"
	"@ r4 = factor							\n"
	"@ r6 = fwd							\n"
	"@ r7 = back							\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"ldr	r4, [r13,#4*16]		@ r4 = divX			\n"
	"ldr	r8, [r13,#4*17]		@ r8 = back4			\n"
	"ldr	r12,[r13,#4*18]		@ r12= fwd4			\n"
	"8:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r1, LSL #8	@ for (xx = x; xx > 0; x--) {	\n"
	"9:				@				\n"
	"add	r5, r5, r3, LSL #16	@ for (yy = f; yy > 0; y--) {	\n"
	"10:				@				\n"
	"ldrb	r11,[r0], r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	10b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	9b			@ }				\n"
	"mul	r14,r4, r14		@ r14= v *= divX		\n"
	"mov	r14,r14,LSR #16		@ r14= v >>= 16			\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back4			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	8b			@ }				\n"
	"add	r0, r0, r12		@ s += fwd4			\n"
	"11:				@				\n"
	"ldr	r14,[r13,#4*19]		@ r14 = fwd3			\n"
	"subs	r2, r2, r3		@ h -= f			\n"
	"add	r0, r0, r14		@ s += fwd3			\n"
	"bge	1b			@ }				\n"
	"12:								\n"
	"adds	r2, r2, r3		@ h += f			\n"
	"beq	21f			@ if no stray row, end		\n"
	"@ So doing one last (partial) row				\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"@ r0 = src = ptr						\n"
	"@ r1 = w							\n"
	"@ r2 = h							\n"
	"@ r3 = f							\n"
	"@ r4 = factor							\n"
	"@ r5 = n							\n"
	"@ r6 = fwd							\n"
	"				@ for (y = h; y > 0; y--) {	\n"
	"ldr	r1, [r13]		@ r1 = w			\n"
	"ldr	r7, [r13,#4*21]		@ r7 = back5			\n"
	"ldr	r8, [r13,#4*14]		@ r8 = back2			\n"
	"subs	r1, r1, r3		@ r1 = w -= f			\n"
	"blt	17f			@ Skip if less than a full col	\n"
	"ldr	r4, [r13,#4*20]		@ r4 = divY			\n"
	"ldr	r12,[r13,#4*15]		@ r12= fwd2			\n"
	"13:				@ for (x = w; x > 0; x--) {	\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"14:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r3, LSL #8	@ for (xx = f; xx > 0; x--) {	\n"
	"15:				@				\n"
	"add	r5, r5, r2, LSL #16	@ for (yy = y; yy > 0; y--) {	\n"
	"16:				@				\n"
	"ldrb	r11,[r0], r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	16b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back5			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	15b			@ }				\n"
	"mul	r14,r4, r14		@ r14 = x *= divY		\n"
	"mov	r14,r14,LSR #16		@ r14 = v >>= 16		\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back2			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	14b			@ }				\n"
	"add	r0, r0, r12		@ s += fwd2			\n"
	"subs	r1, r1, r3		@ x -= f			\n"
	"bge	13b			@ }				\n"
	"17:				@ Less than a full column left	\n"
	"adds	r1, r1, r3		@ x += f			\n"
	"beq	21f			@ if (x == 0) end		\n"
	"@ r0 = src							\n"
	"@ r1 = x							\n"
	"@ r2 = y							\n"
	"@ r3 = f							\n"
	"@ r4 = factor							\n"
	"@ r6 = fwd							\n"
	"@ r7 = back5							\n"
	"@ r8 = back2							\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"ldr	r4, [r13,#4*22]		@ r4 = divXY			\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"ldr	r8, [r13,#4*17]		@ r8 = back4			\n"
	"18:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r1, LSL #8	@ for (xx = x; xx > 0; x--) {	\n"
	"19:				@				\n"
	"add	r5, r5, r2, LSL #16	@ for (yy = y; yy > 0; y--) {	\n"
	"20:				@				\n"
	"ldrb	r11,[r0],r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	20b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back5			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	19b			@ }				\n"
	"mul	r14,r4, r14		@ r14= v *= divX		\n"
	"mov	r14,r14,LSR #16		@ r14= v >>= 16			\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back4			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	18b			@ }				\n"
	"21:				@				\n"
	"ldmfd	r13!,{r1,r4-r11,PC}	@ pop, return to thumb		\n"
	ENTER_THUMB
	);
}

#endif

void
fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor)
{
	int f;

	if (!tile)
		return;

	assert(tile->stride >= tile->w * tile->n);

	fz_subsample_pixblock(tile->samples, tile->w, tile->h, tile->n, factor, tile->stride);

	f = 1<<factor;
	tile->w = (tile->w + f-1)>>factor;
	tile->h = (tile->h + f-1)>>factor;
	tile->stride = tile->w * (size_t)tile->n;
	/* Redundant test? We only ever make pixmaps smaller! */
	if (tile->h > INT_MAX / (tile->w * tile->n))
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap too large");
	tile->samples = fz_realloc(ctx, tile->samples, (size_t)tile->h * tile->w * tile->n);
}

void
fz_subsample_pixblock(unsigned char *s, int w, int h, int n, int factor, ptrdiff_t stride)
{
	int fwd, fwd2, fwd3, back, back2, f;
	unsigned char *d;
#ifndef ARCH_ARM
	int x, y, xx, yy, nn;
#endif

	d = s;
	f = 1<<factor;
	fwd = stride;
	back = f*fwd-n;
	back2 = f*n-1;
	fwd2 = (f-1)*n;
	fwd3 = (f-1)*fwd + (int)stride - w * n;
	factor *= 2;
#ifdef ARCH_ARM
	{
		int strayX = w%f;
		int divX = (strayX ? 65536/(strayX*f) : 0);
		int fwd4 = (strayX-1) * n;
		int back4 = strayX*n-1;
		int strayY = h%f;
		int divY = (strayY ? 65536/(strayY*f) : 0);
		int back5 = fwd * strayY - n;
		int divXY = (strayY*strayX ? 65536/(strayX*strayY) : 0);
		fz_subsample_pixmap_ARM(s, w, h, f, factor, n, fwd, back,
					back2, fwd2, divX, back4, fwd4, fwd3,
					divY, back5, divXY);
	}
#else
	for (y = h - f; y >= 0; y -= f)
	{
		for (x = w - f; x >= 0; x -= f)
		{
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = f; xx > 0; xx--)
				{
					for (yy = f; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back;
				}
				*d++ = v >> factor;
				s -= back2;
			}
			s += fwd2;
		}
		/* Do any strays */
		x += f;
		if (x > 0)
		{
			int div = x * f;
			int fwd4 = (x-1) * n;
			int back4 = x*n-1;
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = x; xx > 0; xx--)
				{
					for (yy = f; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back;
				}
				*d++ = v / div;
				s -= back4;
			}
			s += fwd4;
		}
		s += fwd3;
	}
	/* Do any stray line */
	y += f;
	if (y > 0)
	{
		int div = y * f;
		int back5 = fwd * y - n;
		for (x = w - f; x >= 0; x -= f)
		{
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = f; xx > 0; xx--)
				{
					for (yy = y; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back5;
				}
				*d++ = v / div;
				s -= back2;
			}
			s += fwd2;
		}
		/* Do any stray at the end of the stray line */
		x += f;
		if (x > 0)
		{
			int back4 = x * n - 1;
			div = x * y;
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = x; xx > 0; xx--)
				{
					for (yy = y; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back5;
				}
				*d++ = v / div;
				s -= back4;
			}
		}
	}
#endif
}

void
fz_set_pixmap_resolution(fz_context *ctx, fz_pixmap *pix, int xres, int yres)
{
	pix->xres = xres;
	pix->yres = yres;
}

/*
	Return the md5 digest for a pixmap
*/
void
fz_md5_pixmap(fz_context *ctx, fz_pixmap *pix, unsigned char digest[16])
{
	fz_md5 md5;

	fz_md5_init(&md5);
	if (pix)
	{
		unsigned char *s = pix->samples;
		int h = pix->h;
		int ss = pix->stride;
		int len = pix->w * pix->n;
		while (h--)
		{
			fz_md5_update(&md5, s, len);
			s += ss;
		}
	}
	fz_md5_final(&md5, digest);
}

#ifdef HAVE_VALGRIND
int fz_valgrind_pixmap(const fz_pixmap *pix)
{
	int w, h, n, total;
	int ww, hh, nn;
	int stride;
	const unsigned char *p = pix->samples;

	if (pix == NULL)
		return 0;

	total = 0;
	ww = pix->w;
	hh = pix->h;
	nn = pix->n;
	stride = pix->stride - ww*nn;
	for (h = 0; h < hh; h++)
	{
		for (w = 0; w < ww; w++)
			for (n = 0; n < nn; n++)
				if (*p++) total ++;
		p += stride;
	}
	return total;
}
#endif /* HAVE_VALGRIND */

fz_pixmap *
fz_convert_indexed_pixmap_to_base(fz_context *ctx, const fz_pixmap *src)
{
	fz_pixmap *dst;
	fz_colorspace *base;
	const unsigned char *s;
	unsigned char *d;
	int y, x, k, n, high;
	unsigned char *lookup;
	ptrdiff_t s_line_inc, d_line_inc;

	if (src->colorspace->type != FZ_COLORSPACE_INDEXED)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot convert non-indexed pixmap");
	if (src->n != 1 + src->alpha)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot convert indexed pixmap mis-matching components");

	base = src->colorspace->u.indexed.base;
	high = src->colorspace->u.indexed.high;
	lookup = src->colorspace->u.indexed.lookup;
	n = base->n;

	dst = fz_new_pixmap_with_bbox(ctx, base, fz_pixmap_bbox(ctx, src), src->seps, src->alpha);
	s = src->samples;
	d = dst->samples;
	s_line_inc = src->stride - src->w * (ptrdiff_t)src->n;
	d_line_inc = dst->stride - dst->w * (ptrdiff_t)dst->n;

	if (src->alpha)
	{
		for (y = 0; y < src->h; y++)
		{
			for (x = 0; x < src->w; x++)
			{
				int v = *s++;
				int a = *s++;
				int aa = a + (a>>7);
				v = fz_mini(v, high);
				for (k = 0; k < n; k++)
					*d++ = (aa * lookup[v * n + k] + 128)>>8;
				*d++ = a;
			}
			s += s_line_inc;
			d += d_line_inc;
		}
	}
	else
	{
		for (y = 0; y < src->h; y++)
		{
			for (x = 0; x < src->w; x++)
			{
				int v = *s++;
				v = fz_mini(v, high);
				for (k = 0; k < n; k++)
					*d++ = lookup[v * n + k];
			}
			s += s_line_inc;
			d += d_line_inc;
		}
	}

	if (src->flags & FZ_PIXMAP_FLAG_INTERPOLATE)
		dst->flags |= FZ_PIXMAP_FLAG_INTERPOLATE;
	else
		dst->flags &= ~FZ_PIXMAP_FLAG_INTERPOLATE;

	return dst;
}

fz_pixmap *
fz_convert_separation_pixmap_to_base(fz_context *ctx, const fz_pixmap *src)
{
	fz_pixmap *dst;
	fz_colorspace *ss, *base;
	const unsigned char *s;
	unsigned char *d;
	int y, x, k, sn, bn, a;
	float src_v[FZ_MAX_COLORS];
	float base_v[FZ_MAX_COLORS];
	ptrdiff_t s_line_inc, d_line_inc;

	ss = src->colorspace;

	if (ss->type != FZ_COLORSPACE_SEPARATION)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot expand non-separation pixmap");
	if (src->n != ss->n + src->alpha)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot expand separation pixmap mis-matching alpha channel");

	base = ss->u.separation.base;
	dst = fz_new_pixmap_with_bbox(ctx, base, fz_pixmap_bbox(ctx, src), src->seps, src->alpha);
	fz_clear_pixmap(ctx, dst);
	fz_try(ctx)
	{
		s = src->samples;
		d = dst->samples;
		s_line_inc = src->stride - src->w * (ptrdiff_t)src->n;
		d_line_inc = dst->stride - dst->w * (ptrdiff_t)dst->n;
		sn = ss->n;
		bn = base->n;

		if (base->type == FZ_COLORSPACE_LAB)
		{
			if (src->alpha)
			{
				for (y = 0; y < src->h; y++)
				{
					for (x = 0; x < src->w; x++)
					{
						for (k = 0; k < sn; ++k)
							src_v[k] = *s++ / 255.0f;
						a = *s++;
						ss->u.separation.eval(ctx, ss->u.separation.tint, src_v, sn, base_v, bn);
						*d++ = (base_v[0] / 100) * 255.0f;
						*d++ = base_v[1] + 128;
						*d++ = base_v[2] + 128;
						*d++ = a;
					}
					s += s_line_inc;
					d += d_line_inc;
				}
			}
			else
			{
				for (y = 0; y < src->h; y++)
				{
					for (x = 0; x < src->w; x++)
					{
						for (k = 0; k < sn; ++k)
							src_v[k] = *s++ / 255.0f;
						ss->u.separation.eval(ctx, ss->u.separation.tint, src_v, sn, base_v, bn);
						*d++ = (base_v[0] / 100) * 255.0f;
						*d++ = base_v[1] + 128;
						*d++ = base_v[2] + 128;
					}
					s += s_line_inc;
					d += d_line_inc;
				}
			}
		}
		else
		{
			if (src->alpha)
			{
				for (y = 0; y < src->h; y++)
				{
					for (x = 0; x < src->w; x++)
					{
						for (k = 0; k < sn; ++k)
							src_v[k] = *s++ / 255.0f;
						a = *s++;
						ss->u.separation.eval(ctx, ss->u.separation.tint, src_v, sn, base_v, bn);
						for (k = 0; k < bn; ++k)
							*d++ = base_v[k] * 255.0f;
						*d++ = a;
					}
					s += s_line_inc;
					d += d_line_inc;
				}
			}
			else
			{
				for (y = 0; y < src->h; y++)
				{
					for (x = 0; x < src->w; x++)
					{
						for (k = 0; k < sn; ++k)
							src_v[k] = *s++ / 255.0f;
						ss->u.separation.eval(ctx, ss->u.separation.tint, src_v, sn, base_v, bn);
						for (k = 0; k < bn; ++k)
							*d++ = base_v[k] * 255.0f;
					}
					s += s_line_inc;
					d += d_line_inc;
				}
			}
		}

		if (src->flags & FZ_PIXMAP_FLAG_INTERPOLATE)
			dst->flags |= FZ_PIXMAP_FLAG_INTERPOLATE;
		else
			dst->flags &= ~FZ_PIXMAP_FLAG_INTERPOLATE;
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, dst);
		fz_rethrow(ctx);
	}

	return dst;
}
