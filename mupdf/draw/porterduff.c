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

/*
 * Blend pixmap regions
 */

/* dst = src in msk over dst */
static void
duff_ni1on(byte * restrict sp, int sw, int sn, byte * restrict mp, int mw, byte * restrict dp, int dw, int w0, int h)
{
	int k;

	sw -= w0*sn;
	mw -= w0;
	dw -= w0*sn;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int ma = mp[0];
			int ssa = 255 - fz_mul255(sp[sn-1], ma);
			for (k = 0; k < sn; k++)
			{
				dp[k] = fz_mul255(sp[k], ma) + fz_mul255(dp[k], ssa);
			}
			sp += sn;
			mp ++;
			dp += sn;
		}
		sp += sw;
		mp += mw;
		dp += dw;
	}
}

static void
duff_1i1o1(byte * restrict sp, int sw, byte * restrict mp, int mw, byte * restrict dp, int dw, int w0, int h)
{
	/* duff_nimon(sp0, sw, 1, mp0, mw, 1, dp0, dw, w0, h); */

	sw -= w0;
	dw -= w0;
	mw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			byte ma = mp[0];
			byte sa = fz_mul255(sp[0], ma);
			byte ssa = 255 - sa;
			dp[0] = sa + fz_mul255(dp[0], ssa);
			sp ++;
			mp ++;
			dp ++;
		}
		sp += sw;
		mp += mw;
		dp += dw;
	}
}

static void
duff_2i1o2(byte * restrict sp, int sw, byte * restrict mp, int mw, byte * restrict dp, int dw, int w0, int h)
{

	/* duff_nimon(sp, sw, 2, mp, mw, 1, dp, dw, w0, h); */
	sw -= w0<<1;
	dw -= w0<<1;
	mw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			byte ma = mp[0];
			byte ssa = 255 - fz_mul255(sp[1], ma);
			dp[0] = fz_mul255(sp[0], ma) + fz_mul255(dp[0], ssa);
			dp[1] = fz_mul255(sp[1], ma) + fz_mul255(dp[1], ssa);
			sp += 2;
			mp += 1;
			dp += 2;
		}
		sp += sw;
		mp += mw;
		dp += dw;
	}
}

static void
duff_4i1o4(byte * restrict sp, int sw, byte * restrict mp, int mw, byte * restrict dp, int dw, int w0, int h)
{
	/* duff_nimon(sp, sw, 4, mp, mw, 1, dp, dw, w0, h); */

	sw -= w0<<2;
	dw -= w0<<2;
	mw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			byte ma = mp[0];
			byte ssa = 255 - fz_mul255(sp[3], ma);
			dp[0] = fz_mul255(sp[0], ma) + fz_mul255(dp[0], ssa);
			dp[1] = fz_mul255(sp[1], ma) + fz_mul255(dp[1], ssa);
			dp[2] = fz_mul255(sp[2], ma) + fz_mul255(dp[2], ssa);
			dp[3] = fz_mul255(sp[3], ma) + fz_mul255(dp[3], ssa);
			sp += 4;
			mp += 1;
			dp += 4;
		}
		sp += sw;
		mp += mw;
		dp += dw;
	}
}

/*
 * Path masks
 */

static void
path_1o1(byte * restrict src, byte cov, int len, byte * restrict dst)
{
	while (len--)
	{
		int c;
		cov += *src; *src = 0; src++;
		c = FZ_EXPAND(cov);
		dst[0] = FZ_BLEND(255, dst[0], c);
		dst++;
	}
}

static void
path_w2i1o2(byte * restrict ga, byte * restrict src, byte cov, int len, byte * restrict dst)
{
	byte g = ga[0];
	int a = FZ_EXPAND(ga[1]);

	while (len--)
	{
		int ca;
		cov += *src; *src = 0; src++;
		ca = FZ_COMBINE(FZ_EXPAND(cov), a);
		dst[0] = FZ_BLEND(g, dst[0], ca);
		dst[1] = FZ_BLEND(255, dst[1], ca);
		dst += 2;
	}
}

static void
path_w4i1o4(byte * restrict rgba, byte * restrict src, byte cov, int len, byte * restrict dst)
{
	byte r = rgba[0];
	byte g = rgba[1];
	byte b = rgba[2];
	int a = FZ_EXPAND(rgba[3]);
	while (len--)
	{
		int ca;
		cov += *src; *src = 0; src++;
		ca = FZ_COMBINE(FZ_EXPAND(cov), a);
		dst[0] = FZ_BLEND(r, dst[0], ca);
		dst[1] = FZ_BLEND(g, dst[1], ca);
		dst[2] = FZ_BLEND(b, dst[2], ca);
		dst[3] = FZ_BLEND(255, dst[3], ca);
		dst += 4;
	}
}

/*
 * Text masks
 */

static void
text_1o1(byte * restrict src, int srcw, byte * restrict dst, int dstw, int w0, int h)
{

	srcw -= w0;
	dstw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			dst[0] = src[0] + fz_mul255(dst[0], 255 - src[0]);
			src++;
			dst++;
		}
		src += srcw;
		dst += dstw;
	}
}

static void
text_w2i1o2(byte * restrict ga, byte * restrict src, int srcw, byte * restrict dst, int dstw, int w0, int h)
{
	byte g = ga[0];
	int a = FZ_EXPAND(ga[1]);

	srcw -= w0;
	dstw -= w0<<1;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int c = FZ_COMBINE(FZ_EXPAND(src[0]), a);
			dst[0] = FZ_BLEND(g, dst[0], c);
			dst[1] = FZ_BLEND(255, dst[1], c);
			src ++;
			dst += 2;
		}
		src += srcw;
		dst += dstw;
	}
}

static void
text_w4i1o4(byte * restrict rgba, byte * restrict src, int srcw, byte * restrict dst, int dstw, int w0, int h)
{
	byte r = rgba[0];
	byte g = rgba[1];
	byte b = rgba[2];
	int a = FZ_EXPAND(rgba[3]);

	srcw -= w0;
	dstw -= w0<<2;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int c = FZ_COMBINE(FZ_EXPAND(src[0]), a);
			dst[0] = FZ_BLEND(r, dst[0], c);
			dst[1] = FZ_BLEND(g, dst[1], c);
			dst[2] = FZ_BLEND(b, dst[2], c);
			dst[3] = FZ_BLEND(255, dst[3], c);
			src ++;
			dst += 4;
		}
		src += srcw;
		dst += dstw;
	}
}

/*
 * ... and the function pointers
 */

void (*fz_duff_ni1on)(byte*,int,int,byte*,int,byte*,int,int,int) = duff_ni1on;
void (*fz_duff_1i1o1)(byte*,int,byte*,int,byte*,int,int,int) = duff_1i1o1;
void (*fz_duff_2i1o2)(byte*,int,byte*,int,byte*,int,int,int) = duff_2i1o2;
void (*fz_duff_4i1o4)(byte*,int,byte*,int,byte*,int,int,int) = duff_4i1o4;

void (*fz_path_1o1)(byte*,byte,int,byte*) = path_1o1;
void (*fz_path_w2i1o2)(byte*,byte*,byte,int,byte*) = path_w2i1o2;
void (*fz_path_w4i1o4)(byte*,byte*,byte,int,byte*) = path_w4i1o4;

void (*fz_text_1o1)(byte*,int,byte*,int,int,int) = text_1o1;
void (*fz_text_w2i1o2)(byte*,byte*,int,byte*,int,int,int) = text_w2i1o2;
void (*fz_text_w4i1o4)(byte*,byte*,int,byte*,int,int,int) = text_w4i1o4;
