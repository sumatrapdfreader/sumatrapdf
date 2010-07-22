#include "fitz.h"

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

/* Blend source alpha over destination alpha */

void
fz_blendmasks(byte * restrict dp, byte * restrict sp, int w)
{
	while (w--)
	{
		dp[0] = sp[0] + fz_mul255(dp[0], 255 - sp[0]);
		sp++;
		dp++;
	}
}

/* Blend a non-premultiplied color in mask over destination */

void
fz_blendwithcolormask(byte * restrict dp, byte * restrict sp, byte * restrict mp, int n, int w)
{
	int sa, r, g, b, k;

	switch (n)
	{
	case 2:
		sa = FZ_EXPAND(sp[1]);
		g = sp[0];
		while (w--)
		{
			int ma = *mp++;
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			dp[0] = FZ_BLEND(g, dp[0], masa);
			dp[1] = FZ_BLEND(255, dp[1], masa);
			dp += 2;
		}
		break;
	case 4:
		sa = FZ_EXPAND(sp[3]);
		r = sp[0];
		g = sp[1];
		b = sp[2];
		while (w--)
		{
			int ma = *mp++;
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			dp[0] = FZ_BLEND(r, dp[0], masa);
			dp[1] = FZ_BLEND(g, dp[1], masa);
			dp[2] = FZ_BLEND(b, dp[2], masa);
			dp[3] = FZ_BLEND(255, dp[3], masa);
			dp += 4;
		}
		break;
	default:
		sa = FZ_EXPAND(sp[n-1]);
		while (w--)
		{
			int ma = *mp++;
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			for (k = 0; k < n - 1; k++)
				dp[k] = FZ_BLEND(sp[k], dp[k], masa);
			dp[k] = FZ_BLEND(255, dp[k], masa);
			dp += n;
		}
	}
}

/* Blend source in mask over destination */

void
fz_blendwithmask(byte * restrict dp, byte * restrict sp, byte * restrict mp, int n, int w)
{
	int k;

	switch (n)
	{
	case 2:
		while (w--)
		{
			int ma = *mp++;
			int masa = fz_mul255(sp[1], ma);
			int t = 255 - masa;
			dp[0] = fz_mul255(sp[0], ma) + fz_mul255(dp[0], t);
			sp += 2;
			dp += 2;
		}
		break;
	case 4:
		while (w--)
		{
			int ma = *mp++;
			int masa = fz_mul255(sp[3], ma);
			int t = 255 - masa;
			dp[0] = fz_mul255(sp[0], ma) + fz_mul255(dp[0], t);
			dp[1] = fz_mul255(sp[1], ma) + fz_mul255(dp[1], t);
			dp[2] = fz_mul255(sp[2], ma) + fz_mul255(dp[2], t);
			dp[3] = fz_mul255(sp[3], ma) + fz_mul255(dp[3], t);
			sp += 4;
			dp += 4;
		}
		break;
	default:
		while (w--)
		{
			int ma = *mp++;
			int masa = fz_mul255(sp[n-1], ma);
			int t = 255 - masa;
			for (k = 0; k < n; k++)
				dp[k] = fz_mul255(sp[k], ma) + fz_mul255(dp[k], t);
			sp += n;
			dp += n;
		}
	}
}

/* Blend source in (constant) alpha over destination */

void
fz_blendwithalpha(byte * restrict dp, byte * restrict sp, int ma, int n, int w)
{
	int k;

	while (w--)
	{
		int masa = fz_mul255(sp[n-1], ma);
		int t = 255 - masa;
		for (k = 0; k < n; k++)
			dp[k] = fz_mul255(sp[k], ma) + fz_mul255(dp[k], t);
		sp += n;
		dp += n;
	}
}

/* Blend source over destination */

void
fz_blendnormal(byte * restrict dp, byte * restrict sp, int n, int w)
{
	int k;

	while (w--)
	{
		int t = 255 - sp[n-1];
		for (k = 0; k < n; k++)
			dp[k] = sp[k] + fz_mul255(dp[k], t);
		sp += n;
		dp += n;
	}
}

/*
 * Pixmap blending functions
 */

void
fz_blendpixmapswithmask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk)
{
	unsigned char *sp, *dp, *mp;
	fz_bbox bbox;
	int x, y, w, h, n;

	assert(dst->n == src->n);
	assert(msk->n == 1);

	bbox = fz_boundpixmap(dst);
	bbox = fz_intersectbbox(bbox, fz_boundpixmap(src));
	bbox = fz_intersectbbox(bbox, fz_boundpixmap(msk));

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	n = src->n;
	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	mp = msk->samples + ((y - msk->y) * msk->w + (x - msk->x)) * msk->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	while (h--)
	{
		fz_blendwithmask(dp, sp, mp, n, w);
		sp += src->w * n;
		dp += dst->w * n;
		mp += msk->w;
	}
}

void
fz_blendpixmapswithalpha(fz_pixmap *dst, fz_pixmap *src, float alpha)
{
	unsigned char *sp, *dp;
	fz_bbox bbox;
	int x, y, w, h, n, a;

	assert(dst->n == src->n);

	bbox = fz_boundpixmap(dst);
	bbox = fz_intersectbbox(bbox, fz_boundpixmap(src));

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	a = alpha * 255;
	n = src->n;
	sp = src->samples + ((y - src->y) * src->w + (x - src->x)) * src->n;
	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;

	while (h--)
	{
		if (a == 255)
			fz_blendnormal(dp, sp, n, w);
		else
			fz_blendwithalpha(dp, sp, a, n, w);
		sp += src->w * n;
		dp += dst->w * n;
	}
}

void
fz_blendpixmaps(fz_pixmap *dst, fz_pixmap *src)
{
	fz_blendpixmapswithalpha(dst, src, 1);
}
