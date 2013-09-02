#include "mupdf/fitz.h"
#include "draw-imp.h"

/*

The functions in this file implement various flavours of Porter-Duff blending.

We take the following as definitions:

	Cx = Color (from plane x)
	ax = Alpha (from plane x)
	cx = Cx.ax = Premultiplied color (from plane x)

The general PorterDuff blending equation is:

	Blend Z = X op Y	cz = Fx.cx + Fy. cy	where Fx and Fy depend on op

The two operations we use in this file are: '(X in Y) over Z' and
'S over Z'. The definitions of the 'over' and 'in' operations are as
follows:

	For S over Z,	Fs = 1, Fz = 1-as
	For X in Y,	Fx = ay, Fy = 0

We have 2 choices; we can either work with premultiplied data, or non
premultiplied data. Our

First the premultiplied case:

	Let S = (X in Y)
	Let R = (X in Y) over Z = S over Z

	cs	= cx.Fx + cy.Fy	(where Fx = ay, Fy = 0)
		= cx.ay
	as	= ax.Fx + ay.Fy
		= ax.ay

	cr	= cs.Fs + cz.Fz	(where Fs = 1, Fz = 1-as)
		= cs + cz.(1-as)
		= cx.ay + cz.(1-ax.ay)
	ar	= as.Fs + az.Fz
		= as + az.(1-as)
		= ax.ay + az.(1-ax.ay)

This has various nice properties, like not needing any divisions, and
being symmetric in color and alpha, so this is what we use. Because we
went through the pain of deriving the non premultiplied forms, we list
them here too, though they are not used.

Non Pre-multiplied case:

	Cs.as	= Fx.Cx.ax + Fy.Cy.ay	(where Fx = ay, Fy = 0)
		= Cx.ay.ax
	Cs	= (Cx.ay.ax)/(ay.ax)
		= Cx
	Cr.ar	= Fs.Cs.as + Fz.Cz.az	(where Fs = 1, Fz = 1-as)
		= Cs.as	+ (1-as).Cz.az
		= Cx.ax.ay + Cz.az.(1-ax.ay)
	Cr	= (Cx.ax.ay + Cz.az.(1-ax.ay))/(ax.ay + az.(1-ax-ay))

Much more complex, it seems. However, if we could restrict ourselves to
the case where we were always plotting onto an opaque background (i.e.
az = 1), then:

	Cr	= Cx.(ax.ay) + Cz.(1-ax.ay)
		= (Cx-Cz)*(1-ax.ay) + Cz	(a single MLA operation)
	ar	= 1

Sadly, this is not true in the general case, so we abandon this effort
and stick to using the premultiplied form.

*/

typedef unsigned char byte;

/* These are used by the non-aa scan converter */

void
fz_paint_solid_alpha(byte * restrict dp, int w, int alpha)
{
	int t = FZ_EXPAND(255 - alpha);
	while (w--)
	{
		*dp = alpha + FZ_COMBINE(*dp, t);
		dp ++;
	}
}

static inline void
fz_paint_solid_color_2(byte * restrict dp, int w, byte *color)
{
	int sa = FZ_EXPAND(color[1]);
	if (sa == 0)
		return;
	if (sa == 256)
	{
		while (w--)
		{
			dp[0] = color[0];
			dp[1] = 255;
			dp += 2;
		}
	}
	else
	{
		while (w--)
		{
			dp[0] = FZ_BLEND(color[0], dp[0], sa);
			dp[1] = FZ_BLEND(255, dp[1], sa);
			dp += 2;
		}
	}
}

static inline int isbigendian(void)
{
	union { int i; char c[sizeof(int)]; } u = {1};
	return u.c[0] != 1;
}

static inline void
fz_paint_solid_color_4(byte * restrict dp, int w, byte *color)
{
	unsigned int rgba = *(int *)color;
	int sa = FZ_EXPAND(color[3]);
	if (sa == 0)
		return;
	if (isbigendian())
		rgba |= 0x000000FF;
	else
		rgba |= 0xFF000000;
	if (sa == 256)
	{
		while (w--)
		{
			*(unsigned int *)dp = rgba;
			dp += 4;
		}
	}
	else
	{
		unsigned int mask = 0xFF00FF00;
		unsigned int rb = rgba & (mask>>8);
		unsigned int ga = (rgba & mask)>>8;
		while (w--)
		{
			unsigned int RGBA = *(unsigned int *)dp;
			unsigned int RB = (RGBA<<8) & mask;
			unsigned int GA = RGBA & mask;
			RB += (rb-(RB>>8))*sa;
			GA += (ga-(GA>>8))*sa;
			RB &= mask;
			GA &= mask;
			*(unsigned int *)dp = (RB>>8) | GA;
			dp += 4;
		}
	}
}

static inline void
fz_paint_solid_color_N(byte * restrict dp, int n, int w, byte *color)
{
	int k;
	int n1 = n - 1;
	int sa = FZ_EXPAND(color[n1]);
	if (sa == 0)
		return;
	if (sa == 256)
	{
		while (w--)
		{
			for (k = 0; k < n1; k++)
				dp[k] = color[k];
			dp[k] = 255;
			dp += n;
		}
	}
	else
	{
		while (w--)
		{
			for (k = 0; k < n1; k++)
				dp[k] = FZ_BLEND(color[k], dp[k], sa);
			dp[k] = FZ_BLEND(255, dp[k], sa);
			dp += n;
		}
	}
}

void
fz_paint_solid_color(byte * restrict dp, int n, int w, byte *color)
{
	switch (n)
	{
	case 2: fz_paint_solid_color_2(dp, w, color); break;
	case 4: fz_paint_solid_color_4(dp, w, color); break;
	default: fz_paint_solid_color_N(dp, n, w, color); break;
	}
}

/* Blend a non-premultiplied color in mask over destination */

static inline void
fz_paint_span_with_color_2(byte * restrict dp, byte * restrict mp, int w, byte *color)
{
	int sa = FZ_EXPAND(color[1]);
	int g = color[0];
	if (sa == 256)
	{
		while (w--)
		{
			int ma = *mp++;
			ma = FZ_EXPAND(ma);
			if (ma == 0)
			{
			}
			else if (ma == 256)
			{
				dp[0] = g;
				dp[1] = 255;
			}
			else
			{
				dp[0] = FZ_BLEND(g, dp[0], ma);
				dp[1] = FZ_BLEND(255, dp[1], ma);
			}
			dp += 2;
		}
	}
	else
	{
		while (w--)
		{
			int ma = *mp++;
			ma = FZ_EXPAND(ma);
			if (ma == 0)
			{
			}
			else
			{
				ma = FZ_COMBINE(ma, sa);
				dp[0] = FZ_BLEND(g, dp[0], ma);
				dp[1] = FZ_BLEND(255, dp[1], ma);
			}
			dp += 2;
		}
	}
}

static inline void
fz_paint_span_with_color_4(byte * restrict dp, byte * restrict mp, int w, byte *color)
{
	unsigned int rgba = *((unsigned int *)color);
	unsigned int mask, rb, ga;
	int sa = FZ_EXPAND(color[3]);
	if (sa == 0)
		return;
	if (isbigendian())
		rgba |= 0x000000FF;
	else
		rgba |= 0xFF000000;
	mask = 0xFF00FF00;
	rb = rgba & (mask>>8);
	ga = (rgba & mask)>>8;
	if (sa == 256)
	{
		while (w--)
		{
			unsigned int ma = *mp++;
			dp += 4;
			ma = FZ_EXPAND(ma);
			if (ma == 0)
			{
			}
			else if (ma == 256)
			{
				((unsigned int *)dp)[-1] = rgba;
			}
			else
			{
				unsigned int RGBA = ((unsigned int *)dp)[-1];
				unsigned int RB = (RGBA<<8) & mask;
				unsigned int GA = RGBA & mask;
				RB += (rb-(RB>>8))*ma;
				GA += (ga-(GA>>8))*ma;
				RB &= mask;
				GA &= mask;
				((unsigned int *)dp)[-1] = (RB>>8) | GA;
			}
		}
	}
	else
	{
		while (w--)
		{
			unsigned int ma = *mp++;
			ma = FZ_COMBINE(FZ_EXPAND(ma), sa);
			dp += 4;
			if (ma != 0)
 			{
				unsigned int RGBA = ((unsigned int*)dp)[-1];
				unsigned int RB = (RGBA<<8) & mask;
				unsigned int GA = RGBA & mask;
				RB += (rb-(RB>>8))*ma;
				GA += (ga-(GA>>8))*ma;
				RB &= mask;
				GA &= mask;
				((unsigned int *)dp)[-1] = (RB>>8) | GA;
			}
		}
	}
}

static inline void
fz_paint_span_with_color_N(byte * restrict dp, byte * restrict mp, int n, int w, byte *color)
{
	int k;
	int n1 = n - 1;
	int sa = FZ_EXPAND(color[n1]);
	if (sa == 0)
		return;
	if (sa == 256)
	{
		while (w--)
		{
			int ma = *mp++;
			ma = FZ_EXPAND(ma);
			if (ma == 0)
			{
			}
			else if (ma == 256)
			{
				for (k = 0; k < n1; k++)
					dp[k] = color[k];
				dp[k] = 255;
			}
			else
			{
				for (k = 0; k < n1; k++)
					dp[k] = FZ_BLEND(color[k], dp[k], ma);
				dp[k] = FZ_BLEND(255, dp[k], ma);
			}
			dp += n;
		}
	}
	else
	{
		while (w--)
		{
			int ma = *mp++;
			ma = FZ_COMBINE(FZ_EXPAND(ma), sa);
			for (k = 0; k < n1; k++)
				dp[k] = FZ_BLEND(color[k], dp[k], ma);
			dp[k] = FZ_BLEND(255, dp[k], ma);
			dp += n;
		}
	}
}

void
fz_paint_span_with_color(byte * restrict dp, byte * restrict mp, int n, int w, byte *color)
{
	switch (n)
	{
	case 2: fz_paint_span_with_color_2(dp, mp, w, color); break;
	case 4: fz_paint_span_with_color_4(dp, mp, w, color); break;
	default: fz_paint_span_with_color_N(dp, mp, n, w, color); break;
	}
}

/* Blend source in mask over destination */

/* FIXME: There is potential for SWAR optimisation here */
static inline void
fz_paint_span_with_mask_2(byte * restrict dp, byte * restrict sp, byte * restrict mp, int w)
{
	while (w--)
	{
		int masa;
		int ma = *mp++;
		ma = FZ_EXPAND(ma);
		if (ma == 0)
		{
			dp += 2;
			sp += 2;
		}
		else if (ma == 256)
		{
			masa = 255 - sp[1];
			if (masa == 0)
			{
				*dp++ = *sp++;
				*dp++ = *sp++;
			}
			else
			{
				masa = FZ_EXPAND(masa);
				*dp = *sp + FZ_COMBINE(*dp, masa);
				sp++; dp++;
				*dp = *sp + FZ_COMBINE(*dp, masa);
				sp++; dp++;
			}
		}
		else
		{
			masa = FZ_COMBINE(sp[1], ma);
			masa = 255 - masa;
			masa = FZ_EXPAND(masa);
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
		}
	}
}

/* FIXME: There is potential for SWAR optimisation here */
static inline void
fz_paint_span_with_mask_4(byte * restrict dp, byte * restrict sp, byte * restrict mp, int w)
{
	while (w--)
	{
		int masa;
		int ma = *mp++;
		ma = FZ_EXPAND(ma);
		if (ma == 0)
		{
			dp += 4;
			sp += 4;
		}
		else if (ma == 256)
		{
			masa = 255 - sp[3];
			if (masa == 0)
			{
				*(int*)dp = *(int *)sp;
				sp += 4; dp += 4;
			}
			else
			{
				masa = FZ_EXPAND(masa);
				*dp = *sp + FZ_COMBINE(*dp, masa);
				sp++; dp++;
				*dp = *sp + FZ_COMBINE(*dp, masa);
				sp++; dp++;
				*dp = *sp + FZ_COMBINE(*dp, masa);
				sp++; dp++;
				*dp = *sp + FZ_COMBINE(*dp, masa);
				sp++; dp++;
			}
		}
		else
		{
			/* FIXME: There is potential for SWAR optimisation here */
			masa = FZ_COMBINE(sp[3], ma);
			masa = 255 - masa;
			masa = FZ_EXPAND(masa);
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
			*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
			sp++; dp++;
		}
	}
}

static inline void
fz_paint_span_with_mask_N(byte * restrict dp, byte * restrict sp, byte * restrict mp, int n, int w)
{
	while (w--)
	{
		int ma = *mp++;
		ma = FZ_EXPAND(ma);
		if (ma == 0)
		{
			dp += n;
			sp += n;
		}
		else if (ma == 256)
		{
			int k = n;
			int masa = 255 - sp[n-1];
			if (masa == 0)
			{
				while (k--)
				{
					*dp++ = *sp++;
				}
			}
			else
			{
				masa = FZ_EXPAND(masa);
				while (k--)
				{
					*dp = *sp + FZ_COMBINE(*dp, masa);
					sp++; dp++;
				}
			}
		}
		else
		{
			int k = n;
			int masa = FZ_COMBINE(sp[n-1], ma);
			masa = 255-masa;
			masa = FZ_EXPAND(masa);
			while (k--)
			{
				*dp = FZ_COMBINE2(*sp, ma, *dp, masa);
				sp++; dp++;
			}
		}
	}
}

static void
fz_paint_span_with_mask(byte * restrict dp, byte * restrict sp, byte * restrict mp, int n, int w)
{
	switch (n)
	{
	case 2: fz_paint_span_with_mask_2(dp, sp, mp, w); break;
	case 4: fz_paint_span_with_mask_4(dp, sp, mp, w); break;
	default: fz_paint_span_with_mask_N(dp, sp, mp, n, w); break;
	}
}

/* Blend source in constant alpha over destination */

static inline void
fz_paint_span_2_with_alpha(byte * restrict dp, byte * restrict sp, int w, int alpha)
{
	alpha = FZ_EXPAND(alpha);
	while (w--)
	{
		int masa = FZ_COMBINE(sp[1], alpha);
		*dp = FZ_BLEND(*sp, *dp, masa);
		dp++; sp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		dp++; sp++;
	}
}

static inline void
fz_paint_span_4_with_alpha(byte * restrict dp, byte * restrict sp, int w, int alpha)
{
	alpha = FZ_EXPAND(alpha);
	while (w--)
	{
		int masa = FZ_COMBINE(sp[3], alpha);
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
		*dp = FZ_BLEND(*sp, *dp, masa);
		sp++; dp++;
	}
}

static inline void
fz_paint_span_N_with_alpha(byte * restrict dp, byte * restrict sp, int n, int w, int alpha)
{
	alpha = FZ_EXPAND(alpha);
	while (w--)
	{
		int masa = FZ_COMBINE(sp[n-1], alpha);
		int k = n;
		while (k--)
		{
			*dp = FZ_BLEND(*sp++, *dp, masa);
			dp++;
		}
	}
}

/* Blend source over destination */

static inline void
fz_paint_span_1(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(255 - sp[0]);
		*dp = *sp++ + FZ_COMBINE(*dp, t);
		dp ++;
	}
}

static inline void
fz_paint_span_2(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(sp[1]);
		if (t == 0)
		{
			dp += 2; sp += 2;
		}
		else
		{
			t = 256 - t;
			if (t == 0)
			{
				*dp++ = *sp++;
				*dp++ = *sp++;
			}
			else
			{
				*dp = *sp++ + FZ_COMBINE(*dp, t);
				dp++;
				*dp = *sp++ + FZ_COMBINE(*dp, t);
				dp++;
			}
		}
	}
}

static inline void
fz_paint_span_4(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(sp[3]);
		if (t == 0)
		{
			dp += 4; sp += 4;
		}
		else
		{
			t = 256 - t;
			if (t == 0)
			{
				*(int *)dp = *(int *)sp;
				dp += 4; sp += 4;
			}
			else
			{
				*dp = *sp++ + FZ_COMBINE(*dp, t);
				dp++;
				*dp = *sp++ + FZ_COMBINE(*dp, t);
				dp++;
				*dp = *sp++ + FZ_COMBINE(*dp, t);
				dp++;
				*dp = *sp++ + FZ_COMBINE(*dp, t);
				dp++;
			}
		}
	}
}

static inline void
fz_paint_span_N(byte * restrict dp, byte * restrict sp, int n, int w)
{
	while (w--)
	{
		int t = FZ_EXPAND(sp[n-1]);
		if (t == 0)
		{
			dp += n; sp += n;
		}
		else
		{
			t = 256 - t;
			if (t == 0)
			{
				int k = n;
				while (k--)
				{
					*dp++ = *sp++;
				}
			}
			else
			{
				int k = n;
				while (k--)
				{
					*dp = *sp++ + FZ_COMBINE(*dp, t);
					dp++;
				}
			}
		}
	}
}

void
fz_paint_span(byte * restrict dp, byte * restrict sp, int n, int w, int alpha)
{
	if (alpha == 255)
	{
		switch (n)
		{
		case 1: fz_paint_span_1(dp, sp, w); break;
		case 2: fz_paint_span_2(dp, sp, w); break;
		case 4: fz_paint_span_4(dp, sp, w); break;
		default: fz_paint_span_N(dp, sp, n, w); break;
		}
	}
	else if (alpha > 0)
	{
		switch (n)
		{
		case 2: fz_paint_span_2_with_alpha(dp, sp, w, alpha); break;
		case 4: fz_paint_span_4_with_alpha(dp, sp, w, alpha); break;
		default: fz_paint_span_N_with_alpha(dp, sp, n, w, alpha); break;
		}
	}
}

/*
 * Pixmap blending functions
 */

void
fz_paint_pixmap_with_bbox(fz_pixmap *dst, fz_pixmap *src, int alpha, fz_irect bbox)
{
	unsigned char *sp, *dp;
	int x, y, w, h, n;
	fz_irect bbox2;

	assert(dst->n == src->n);

	fz_pixmap_bbox_no_ctx(dst, &bbox2);
	fz_intersect_irect(&bbox, &bbox2);
	fz_pixmap_bbox_no_ctx(src, &bbox2);
	fz_intersect_irect(&bbox, &bbox2);

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;
	if ((w | h) == 0)
		return;

	n = src->n;
	sp = src->samples + (unsigned int)(((y - src->y) * src->w + (x - src->x)) * src->n);
	dp = dst->samples + (unsigned int)(((y - dst->y) * dst->w + (x - dst->x)) * dst->n);

	while (h--)
	{
		fz_paint_span(dp, sp, n, w, alpha);
		sp += src->w * n;
		dp += dst->w * n;
	}
}

void
fz_paint_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha)
{
	unsigned char *sp, *dp;
	fz_irect bbox;
	fz_irect bbox2;
	int x, y, w, h, n;

	assert(dst->n == src->n);

	fz_pixmap_bbox_no_ctx(dst, &bbox);
	fz_pixmap_bbox_no_ctx(src, &bbox2);
	fz_intersect_irect(&bbox, &bbox2);

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;
	if ((w | h) == 0)
		return;

	n = src->n;
	sp = src->samples + (unsigned int)(((y - src->y) * src->w + (x - src->x)) * src->n);
	dp = dst->samples + (unsigned int)(((y - dst->y) * dst->w + (x - dst->x)) * dst->n);

	while (h--)
	{
		fz_paint_span(dp, sp, n, w, alpha);
		sp += src->w * n;
		dp += dst->w * n;
	}
}

void
fz_paint_pixmap_with_mask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk)
{
	unsigned char *sp, *dp, *mp;
	fz_irect bbox, bbox2;
	int x, y, w, h, n;

	assert(dst->n == src->n);
	assert(msk->n == 1);

	fz_pixmap_bbox_no_ctx(dst, &bbox);
	fz_pixmap_bbox_no_ctx(src, &bbox2);
	fz_intersect_irect(&bbox, &bbox2);
	fz_pixmap_bbox_no_ctx(msk, &bbox2);
	fz_intersect_irect(&bbox, &bbox2);

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;
	if ((w | h) == 0)
		return;

	n = src->n;
	sp = src->samples + (unsigned int)(((y - src->y) * src->w + (x - src->x)) * src->n);
	mp = msk->samples + (unsigned int)(((y - msk->y) * msk->w + (x - msk->x)) * msk->n);
	dp = dst->samples + (unsigned int)(((y - dst->y) * dst->w + (x - dst->x)) * dst->n);

	while (h--)
	{
		fz_paint_span_with_mask(dp, sp, mp, n, w);
		sp += src->w * n;
		dp += dst->w * n;
		mp += msk->w;
	}
}

static inline void
fz_paint_glyph_mask(int span, unsigned char *dp, fz_glyph *glyph, int w, int h, int skip_x, int skip_y)
{
	while (h--)
	{
		int skip_xx, ww, len, extend;
		unsigned char *runp;
		unsigned char *ddp = dp;
		int offset = ((int *)(glyph->data))[skip_y++];
		if (offset >= 0)
		{
			int eol = 0;
			runp = &glyph->data[offset];
			extend = 0;
			ww = w;
			skip_xx = skip_x;
			while (skip_xx)
			{
				int v = *runp++;
				switch (v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					len = 0;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto transparent_run;
					}
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto solid_run;
					}
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						runp += skip_xx;
						len -= skip_xx;
						goto intermediate_run;
					}
					runp += len;
					break;
				}
				if (eol)
				{
					ww = 0;
					break;
				}
				skip_xx -= len;
			}
			while (ww > 0)
			{
				int v = *runp++;
				switch(v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
transparent_run:
					if (len > ww)
						len = ww;
					ww -= len;
					ddp += len;
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
solid_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						*ddp++ = 0xFF;
					}
					while (--len);
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
intermediate_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						int v = *ddp;
						int a = *runp++;
						if (v == 0)
						{
							*ddp++ = a;
						}
						else
						{
							a = FZ_EXPAND(a);
							*ddp = FZ_BLEND(0xFF, v, a);
							ddp++;
						}
					}
					while (--len);
					break;
				}
				if (eol)
					break;
			}
		}
		dp += span;
	}
}

static inline void
fz_paint_glyph_alpha_N(unsigned char *colorbv, int n, int span, unsigned char *dp, fz_glyph *glyph, int w, int h, int skip_x, int skip_y)
{
	int sa = FZ_EXPAND(colorbv[n-1]);
	while (h--)
	{
		int skip_xx, ww, len, extend;
		unsigned char *runp;
		unsigned char *ddp = dp;
		int offset = ((int *)(glyph->data))[skip_y++];
		if (offset >= 0)
		{
			int eol = 0;
			runp = &glyph->data[offset];
			extend = 0;
			ww = w;
			skip_xx = skip_x;
			while (skip_xx)
			{
				int v = *runp++;
				switch (v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					len = 0;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto transparent_run;
					}
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto solid_run;
					}
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						runp += skip_xx;
						len -= skip_xx;
						goto intermediate_run;
					}
					runp += len;
					break;
				}
				if (eol)
				{
					ww = 0;
					break;
				}
				skip_xx -= len;
			}
			while (ww > 0)
			{
				int v = *runp++;
				switch(v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
transparent_run:
					if (len > ww)
						len = ww;
					ww -= len;
					ddp += len * n;
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
solid_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						int k = 0;
						do
						{
							*ddp = FZ_BLEND(colorbv[k++], *ddp, sa);
							ddp++;
						}
						while (k != n-1);
						*ddp = FZ_BLEND(0xFF, *ddp, sa);
						ddp++;
					}
					while (--len);
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
intermediate_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						int k = 0;
						int a = *runp++;
						a = FZ_COMBINE(sa, FZ_EXPAND(a));
						do
						{
							*ddp = FZ_BLEND(colorbv[k++], *ddp, a);
							ddp++;
						}
						while (k != n-1);
						*ddp = FZ_BLEND(0xFF, *ddp, a);
						ddp++;
					}
					while (--len);
					break;
				}
				if (eol)
					break;
			}
		}
		dp += span;
	}
}

static inline void
fz_paint_glyph_solid_N(unsigned char *colorbv, int n, int span, unsigned char *dp, fz_glyph *glyph, int w, int h, int skip_x, int skip_y)
{
	while (h--)
	{
		int skip_xx, ww, len, extend;
		unsigned char *runp;
		unsigned char *ddp = dp;
		int offset = ((int *)(glyph->data))[skip_y++];
		if (offset >= 0)
		{
			int eol = 0;
			runp = &glyph->data[offset];
			extend = 0;
			ww = w;
			skip_xx = skip_x;
			while (skip_xx)
			{
				int v = *runp++;
				switch (v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					len = 0;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto transparent_run;
					}
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						len -= skip_xx;
						goto solid_run;
					}
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
					if (len > skip_xx)
					{
						runp += skip_xx;
						len -= skip_xx;
						goto intermediate_run;
					}
					runp += len;
					break;
				}
				if (eol)
				{
					ww = 0;
					break;
				}
				skip_xx -= len;
			}
			while (ww > 0)
			{
				int v = *runp++;
				switch(v & 3)
				{
				case 0: /* Extend */
					extend = v>>2;
					break;
				case 1: /* Transparent */
					len = (v>>2) + 1 + (extend<<6);
					extend = 0;
transparent_run:
					if (len > ww)
						len = ww;
					ww -= len;
					ddp += len * n;
					break;
				case 2: /* Solid */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
solid_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						int k = 0;
						do
						{
							*ddp++ = colorbv[k++];
						}
						while (k != n);
					}
					while (--len);
					break;
				default: /* Intermediate */
					eol = v & 4;
					len = (v>>3) + 1 + (extend<<5);
					extend = 0;
intermediate_run:
					if (len > ww)
						len = ww;
					ww -= len;
					do
					{
						int k = 0;
						int a = *runp++;
						a = FZ_EXPAND(a);
						do
						{
							*ddp = FZ_BLEND(colorbv[k++], *ddp, a);
							ddp++;
						}
						while (k != n-1);
						*ddp = FZ_BLEND(0xFF, *ddp, a);
						ddp++;
					}
					while (--len);
					break;
				}
				if (eol)
					break;
			}
		}
		dp += span;
	}
}

static inline void
fz_paint_glyph_alpha(unsigned char *colorbv, int n, int span, unsigned char *dp, fz_glyph *glyph, int w, int h, int skip_x, int skip_y)
{
	switch (n)
	{
	case 4:
		fz_paint_glyph_alpha_N(colorbv, 4, span, dp, glyph, w, h, skip_x, skip_y);
		break;
	case 2:
		fz_paint_glyph_alpha_N(colorbv, 2, span, dp, glyph, w, h, skip_x, skip_y);
		break;
	default:
		fz_paint_glyph_alpha_N(colorbv, n, span, dp, glyph, w, h, skip_x, skip_y);
		break;
	}
}

static inline void
fz_paint_glyph_solid(unsigned char *colorbv, int n, int span, unsigned char *dp, fz_glyph *glyph, int w, int h, int skip_x, int skip_y)
{
	switch (n)
	{
	case 4:
		fz_paint_glyph_solid_N(colorbv, 4, span, dp, glyph, w, h, skip_x, skip_y);
		break;
	case 2:
		fz_paint_glyph_solid_N(colorbv, 2, span, dp, glyph, w, h, skip_x, skip_y);
		break;
	default:
		fz_paint_glyph_solid_N(colorbv, n, span, dp, glyph, w, h, skip_x, skip_y);
		break;
	}
}

void
fz_paint_glyph(unsigned char *colorbv, fz_pixmap *dst, unsigned char *dp, fz_glyph *glyph, int w, int h, int skip_x, int skip_y)
{
	if (dst->colorspace)
	{
		if (colorbv[dst->n-1] == 255)
			fz_paint_glyph_solid(colorbv, dst->n, dst->w * dst->n, dp, glyph, w, h, skip_x, skip_y);
		else if (colorbv[dst->n-1] != 0)
			fz_paint_glyph_alpha(colorbv, dst->n, dst->w * dst->n, dp, glyph, w, h, skip_x, skip_y);
	}
	else
		fz_paint_glyph_mask(dst->w, dp, glyph, w, h, skip_x, skip_y);
}
