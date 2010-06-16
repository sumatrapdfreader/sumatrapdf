#include "fitz.h"

typedef unsigned char byte;

/*
 * Blend pixmap regions
 */

/* dst = src over dst */
static void
duff_non(byte * restrict sp, int sw, int sn, byte * restrict dp, int dw, int w0, int h)
{
	int k;
	sw -= w0*sn;
	dw -= w0*sn;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			/* RJW: Alpha handling suspicious here; sp[0] counts twice */
			int sa = FZ_EXPAND(sp[0]);
			for (k = 0; k < sn; k++)
			{
				dp[k] = FZ_BLEND(sp[k], dp[k], sa);
			}
			sp += sn;
			dp += sn;
		}
		sp += sw;
		dp += dw;
	}
}

/* dst = src in msk over dst */
static void
duff_nimon(byte * restrict sp, int sw, int sn, byte * restrict mp, int mw, int mn, byte * restrict dp, int dw, int w0, int h)
{
	int k;
	sw -= w0*sn;
	mw -= w0*mn;
	dw -= w0*sn;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			/* TODO: validate this */
			int ma = FZ_COMBINE(FZ_EXPAND(mp[0]), FZ_EXPAND(sp[0]));
			for (k = 0; k < sn; k++)
			{
				dp[k] = FZ_BLEND(sp[k], dp[k], ma);
			}
			sp += sn;
			mp += mn;
			dp += sn;
		}
		sp += sw;
		mp += mw;
		dp += dw;
	}
}

static void
duff_1o1(byte * restrict sp, int sw, byte * restrict dp, int dw, int w0, int h)
{
	/* duff_non(sp0, sw, 1, dp0, dw, w0, h); */
	sw -= w0;
	dw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			dp[0] = FZ_BLEND(255, dp[0], FZ_EXPAND(sp[0]));
			sp ++;
			dp ++;
		}
		sp += sw;
		dp += dw;
	}
}

static void
duff_4o4(byte *sp, int sw, byte *dp, int dw, int w0, int h)
{
	/* duff_non(sp0, sw, 4, dp0, dw, w0, h); */
	sw -= w0<<2;
	dw -= w0<<2;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int alpha = FZ_EXPAND(sp[0]);
			dp[0] = FZ_BLEND(sp[0], dp[0], alpha);
			dp[1] = FZ_BLEND(sp[1], dp[1], alpha);
			dp[2] = FZ_BLEND(sp[2], dp[2], alpha);
			dp[3] = FZ_BLEND(sp[3], dp[3], alpha);
			sp += 4;
			dp += 4;
		}
		sp += sw;
		dp += dw;
	}
}

static void
duff_1i1o1(byte * restrict sp, int sw, byte * restrict mp, int mw, byte * restrict dp, int dw, int w0, int h)
{
	/* duff_nimon(sp0, sw, 1, mp0, mw, 1, dp0, dw, w0, h); */
	sw -= w0;
	mw -= w0;
	dw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int ma = FZ_COMBINE(FZ_EXPAND(mp[0]), FZ_EXPAND(sp[0]));
			dp[0] = FZ_BLEND(255, dp[0], ma);
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
			int ma = FZ_COMBINE(FZ_EXPAND(mp[0]), FZ_EXPAND(sp[0]));
			dp[0] = FZ_BLEND(255, dp[0], ma);
			dp[1] = FZ_BLEND(sp[1], dp[1], ma);
			dp[2] = FZ_BLEND(sp[2], dp[2], ma);
			dp[3] = FZ_BLEND(sp[3], dp[3], ma);
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
path_w4i1o4(byte * restrict argb, byte * restrict src, byte cov, int len, byte * restrict dst)
{
	int alpha = FZ_EXPAND(argb[0]);
	byte r = argb[1];
	byte g = argb[2];
	byte b = argb[3];
	while (len--)
	{
		int ca;
		cov += *src; *src = 0; src++;
		ca = FZ_COMBINE(FZ_EXPAND(cov), alpha);
		dst[0] = FZ_BLEND(255, dst[0], ca);
		dst[1] = FZ_BLEND(r, dst[1], ca);
		dst[2] = FZ_BLEND(g, dst[2], ca);
		dst[3] = FZ_BLEND(b, dst[3], ca);
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
			int c = FZ_EXPAND(src[0]);
			dst[0] = FZ_BLEND(255, dst[0], c);
			src++;
			dst++;
		}
		src += srcw;
		dst += dstw;
	}
}

static void
text_w4i1o4(byte * restrict argb, byte * restrict src, int srcw, byte * restrict dst, int dstw, int w0, int h)
{
	int alpha = FZ_EXPAND(argb[0]);
	byte r = argb[1];
	byte g = argb[2];
	byte b = argb[3];
	srcw -= w0;
	dstw -= w0<<2;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			int c = FZ_COMBINE(FZ_EXPAND(src[0]), alpha);
			dst[0] = FZ_BLEND(255, dst[0], c);
			dst[1] = FZ_BLEND(r, dst[1], c);
			dst[2] = FZ_BLEND(g, dst[2], c);
			dst[3] = FZ_BLEND(b, dst[3], c);
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

void (*fz_duff_non)(byte*,int,int,byte*,int,int,int) = duff_non;
void (*fz_duff_nimon)(byte*,int,int,byte*,int,int,byte*,int,int,int) = duff_nimon;
void (*fz_duff_1o1)(byte*,int,byte*,int,int,int) = duff_1o1;
void (*fz_duff_4o4)(byte*,int,byte*,int,int,int) = duff_4o4;
void (*fz_duff_1i1o1)(byte*,int,byte*,int,byte*,int,int,int) = duff_1i1o1;
void (*fz_duff_4i1o4)(byte*,int,byte*,int,byte*,int,int,int) = duff_4i1o4;

void (*fz_path_1o1)(byte*,byte,int,byte*) = path_1o1;
void (*fz_path_w4i1o4)(byte*,byte*,byte,int,byte*) = path_w4i1o4;

void (*fz_text_1o1)(byte*,int,byte*,int,int,int) = text_1o1;
void (*fz_text_w4i1o4)(byte*,byte*,int,byte*,int,int,int) = text_w4i1o4;
