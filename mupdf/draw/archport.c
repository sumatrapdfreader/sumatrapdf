#include "fitz.h"

typedef unsigned char byte;

/* These C implementations use SWAR (SIMD-within-a-register) techniques. */

#define MASK 0xFF00FF00;

static void
path_w4i1o4_32bit(byte * restrict argb,
	byte * restrict src, byte cov, int len,
	byte * restrict dst)
{
	/* COLOR * coverage + DST * (256-coverage) = (COLOR - DST)*coverage + DST*256 */
	unsigned int *dst32 = (unsigned int *)(void *)dst;
	int alpha = argb[0];
	unsigned int rb = argb[1] | (argb[3] << 16);
	unsigned int ag = 255 | (argb[2] << 16);

	if (alpha == 0)
		return;

	if (alpha != 255)
	{
		alpha += alpha>>7; /* alpha is now in the 0...256 range */
		while (len--)
		{
			unsigned int ca, drb, dag, crb, cag;
			cov += *src; *src++ = 0;
			ca = cov + (cov>>7); /* ca is in 0...256 range */
			ca = (ca*alpha)>>8; /* ca is is in 0...256 range */
			dag = *dst32++;
			if (ca != 0)
			{
				drb = dag & MASK;
				dag = (dag<<8) & MASK;
				crb = rb - (drb>>8);
				cag = ag - (dag>>8);
				drb += crb * ca;
				dag += cag * ca;
				drb &= MASK;
				dag &= MASK;
				dag = drb | (dag>>8);
				dst32[-1] = dag;
			}
		}
	}
	else
	{
		while (len--)
		{
			unsigned int ca, drb, dag, crb, cag;
			cov += *src; *src++ = 0;
			ca = cov + (cov>>7); /* ca is in 0...256 range */
			dag = *dst32++;
			if (ca == 0)
				continue;
			if (ca == 255)
			{
				dag = (rb<<8) | ag;
			}
			else
			{
				drb = dag & MASK;
				dag = (dag<<8) & MASK;
				crb = rb - (drb>>8);
				cag = ag - (dag>>8);
				drb += crb * ca;
				dag += cag * ca;
				drb &= MASK;
				dag &= MASK;
				dag = drb | (dag>>8);
			}
			dst32[-1] = dag;
		}
	}
}

static void
duff_4o4_32bit(byte * restrict sp, int sw, byte * restrict dp, int dw, int w0, int h)
{
	unsigned int *sp32 = (unsigned int *)(void *)sp;
	unsigned int *dp32 = (unsigned int *)(void *)dp;

	/* duff_non(sp0, sw, 4, dp0, dw, w0, h); */

	sw = (sw>>2)-w0;
	dw = (dw>>2)-w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			unsigned int sag = *sp32++;
			unsigned int dag = *dp32++;
			unsigned int srb, drb;
			int alpha = sag & 255;
			if (alpha == 0)
				continue;
			alpha += alpha>>7;
			sag |= 0xFF;
			drb = dag & MASK;
			dag = (dag<<8) & MASK;
			srb = (sag>>8) & ~MASK;
			sag = sag & ~MASK;
			srb -= (drb>>8);
			sag -= (dag>>8);
			drb += srb * alpha;
			dag += sag * alpha;
			drb &= MASK;
			dag &= MASK;
			dag = drb | (dag>>8);
			dp32[-1] = dag;
		}
		sp32 += sw;
		dp32 += dw;
	}
}

static void
duff_4i1o4_32bit(byte * restrict sp, int sw,
	byte * restrict mp, int mw,
	byte * restrict dp, int dw, int w0, int h)
{
	unsigned int *sp32 = (unsigned int *)(void *)sp;
	unsigned int *dp32 = (unsigned int *)(void *)dp;

	/* duff_nimon(sp, sw, 4, mp, mw, 1, dp, dw, w0, h); */

	sw = (sw>>2)-w0;
	dw = (dw>>2)-w0;
	mw -= w0;
	while (h--)
	{
		int w = w0;
		while (w--)
		{
			unsigned int sag = *sp32++;
			unsigned int dag = *dp32++;
			unsigned int srb, drb, alpha, ma;
			alpha = sag & 255;
			ma = *mp++;
			if (alpha == 0)
				continue;
			ma += ma>>7;
			if (ma == 0)
				continue;
			alpha += alpha>>7;
			alpha = (alpha*ma)>>8;
			sag |= 0xFF;
			drb = dag & MASK;
			dag = (dag<<8) & MASK;
			srb = (sag>>8) & ~MASK;
			sag = sag & ~MASK;
			srb -= (drb>>8);
			sag -= (dag>>8);
			drb += srb * alpha;
			dag += sag * alpha;
			drb &= MASK;
			dag &= MASK;
			dag = drb | (dag>>8);
			dp32[-1] = dag;
		}
		sp32 += sw;
		mp += mw;
		dp32 += dw;
	}
}

static void
text_w4i1o4_32bit(byte * restrict argb,
	byte * restrict src, int srcw,
	byte * restrict dst, int dstw, int w0, int h)
{
	unsigned int *dst32 = (unsigned int *)(void *)dst;
	unsigned int alpha = argb[0];
	unsigned int rb = argb[1] | (argb[3] << 16);
	unsigned int ag = 255 | (argb[2] << 16);

	if (alpha == 0)
		return;

	srcw -= w0;
	dstw = (dstw>>2)-w0;

	if (alpha != 255)
	{
		alpha += alpha>>7;
		while (h--)
		{
			int w = w0;
			while (w--)
			{
				unsigned int ca, drb, dag, crb, cag;
				ca = *src++;
				dag = *dst32++;
				ca += ca>>7;
				ca = (ca*alpha)>>8;
				if (ca == 0)
				 continue;
				drb = dag & MASK;
				dag = (dag<<8) & MASK;
				crb = rb - (drb>>8);
				cag = ag - (dag>>8);
				drb += crb * ca;
				dag += cag * ca;
				drb &= MASK;
				dag &= MASK;
				dag = drb | (dag>>8);
				dst32[-1] = dag;
			}
			src += srcw;
			dst32 += dstw;
		}
	}
	else
	{
		alpha += alpha>>7;
		while (h--)
		{
			int w = w0;
			while (w--)
			{
				unsigned int ca, drb, dag, crb, cag;
				ca = *src++;
				dag = *dst32++;
				ca += ca>>7;
				if (ca == 0)
				 continue;
				drb = dag & MASK;
				dag = (dag<<8) & MASK;
				crb = rb - (drb>>8);
				cag = ag - (dag>>8);
				drb += crb * ca;
				dag += cag * ca;
				drb &= MASK;
				dag &= MASK;
				dag = drb | (dag>>8);
				dst32[-1] = dag;
			}
			src += srcw;
			dst32 += dstw;
		}
	}
}

void fz_accelerate(void)
{
	if (sizeof(int) == 4 && sizeof(unsigned int) == 4)
	{
		fz_duff_4o4 = duff_4o4_32bit;
		fz_duff_4i1o4 = duff_4i1o4_32bit;
		fz_path_w4i1o4 = path_w4i1o4_32bit;
		fz_text_w4i1o4 = text_w4i1o4_32bit;
	}

	if (sizeof(int) == 8)
	{
	}

#ifdef HAVE_CPUDEP
	fz_acceleratearch();
#endif
}
