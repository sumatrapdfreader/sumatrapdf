#include "fitz.h"

typedef unsigned char byte;

/* These C implementations use SWAR (SIMD-within-a-register) techniques. */

#if 0 /* TODO: move into porterduff.c functions */

#define MASK 0xFF00FF00;

static void
path_w4i1o4_32bit(byte *rgba,
	byte * restrict src, byte cov, int len, byte * restrict dst)
{
	/* COLOR * coverage + DST * (256-coverage) = (COLOR - DST)*coverage + DST*256 */
	unsigned int *dst32 = (unsigned int *)(void *)dst;
	int alpha = rgba[3];
	unsigned int rb = rgba[0] | (rgba[2] << 16);
	unsigned int ga = rgba[1] | 0xFF0000;

	if (alpha == 0)
		return;

	if (alpha != 255)
	{
		alpha += alpha>>7; /* alpha is now in the 0...256 range */
		while (len--)
		{
			unsigned int ca, drb, dga, crb, cga;
			cov += *src; *src++ = 0;
			ca = cov + (cov>>7); /* ca is in 0...256 range */
			ca = (ca*alpha)>>8; /* ca is is in 0...256 range */
			drb = *dst32++;
			if (ca != 0)
			{
				dga = drb & MASK;
				drb = (drb<<8) & MASK;
				cga = ga - (dga>>8);
				crb = rb - (drb>>8);
				dga += cga * ca;
				drb += crb * ca;
				dga &= MASK;
				drb &= MASK;
				drb = dga | (drb>>8);
				dst32[-1] = drb;
			}
		}
	}
	else
	{
		while (len--)
		{
			unsigned int ca, drb, dga, crb, cga;
			cov += *src; *src++ = 0;
			ca = cov + (cov>>7); /* ca is in 0...256 range */
			drb = *dst32++;
			if (ca == 0)
				continue;
			if (ca == 255)
			{
				drb = (ga<<8) | rb;
			}
			else
			{
				dga = drb & MASK;
				drb = (drb<<8) & MASK;
				cga = ga - (dga>>8);
				crb = rb - (drb>>8);
				dga += cga * ca;
				drb += crb * ca;
				dga &= MASK;
				drb &= MASK;
				drb = dga |(drb>>8);
			}
			dst32[-1] = drb;
		}
	}
}

static void
text_w4i1o4_32bit(byte *rgba,
	byte * restrict src, int srcw,
	byte * restrict dst, int dstw, int w0, int h)
{
	unsigned int *dst32 = (unsigned int *)(void *)dst;
	unsigned int alpha = rgba[3];
	unsigned int rb = rgba[0] | (rgba[2] << 16);
	unsigned int ga = rgba[1] | 0xFF0000;

	if (alpha == 0)
		return;

	srcw -= w0;
	dstw = (dstw>>2)-w0;

	if (alpha != 255)
	{
		alpha += alpha>>7; /* alpha is now in the 0...256 range */
		while (h--)
		{
			int w = w0;
			while (w--)
			{
				unsigned int ca, drb, dga, crb, cga;
				ca = *src++;
				drb = *dst32++;
				ca += ca>>7;
				ca = (ca*alpha)>>8;
				if (ca == 0)
					continue;
				dga = drb & MASK;
				drb = (drb<<8) & MASK;
				cga = ga - (dga>>8);
				crb = rb - (drb>>8);
				dga += cga * ca;
				drb += crb * ca;
				dga &= MASK;
				drb &= MASK;
				drb = dga | (drb>>8);
				dst32[-1] = drb;
			}
			src += srcw;
			dst32 += dstw;
		}
	}
	else
	{
		while (h--)
		{
			int w = w0;
			while (w--)
			{
				unsigned int ca, drb, dga, crb, cga;
				ca = *src++;
				drb = *dst32++;
				ca += ca>>7;
				if (ca == 0)
					continue;
				dga = drb & MASK;
				drb = (drb<<8) & MASK;
				cga = ga - (dga>>8);
				crb = rb - (drb>>8);
				dga += cga * ca;
				drb += crb * ca;
				dga &= MASK;
				drb &= MASK;
				drb = dga | (drb>>8);
				dst32[-1] = drb;
			}
			src += srcw;
			dst32 += dstw;
		}
	}
}

static void
img_4o4_32bit(byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	unsigned int *dst32 = (unsigned int *)(void *)dst;
	unsigned int *samples = (unsigned int *)(void *)image->samples;
	int w = image->w;
	int h = image->h-1;

	while (len--)
	{
		unsigned int a, a1, d, d1;
		int sa;
		cov += *src; *src = 0; src++;
		/* (a,a1) = sampleargb(samples, w, h, u, v, argb); */
		{
			int ui, ui1, vi, vi1, ud, vd;
			unsigned int b, b1, c, c1;
			ui1 = 1;
			ui = u >> 16;
			if (ui < 0)
			{
				ui = 0;
				ui1 = 0;
			}
			else if (ui >= w-1)
			{
				ui = w-1;
				ui1 = 0;
			}
			vi1 = w;
			vi = v >> 16;
			if (vi < 0)
			{
				vi = 0;
				vi1 = 0;
			}
			else if (vi >= h)
			{
				vi = h;
				vi1 = 0;
			}
			ui += vi*w;
			a = samples[ui];
			b = samples[ui + ui1];
			c = samples[ui + vi1];
			d = samples[ui + ui1 + vi1];
			ud = (u>>8) & 0xFF;
			vd = (v>>8) & 0xFF;
			ud = FZ_EXPAND(ud);
			vd = FZ_EXPAND(vd);
			/* (a,a1) = blend(a,b,ud) */
			a1 = a & MASK;
			a = (a<<8) & MASK;
			b1 = (b>>8) & ~MASK;
			b = b & ~MASK;
			a = ((b -(a >>8)) * ud + a ) & MASK;
			a1 = ((b1-(a1>>8)) * ud + a1) & MASK;
			/* (c,c1) = blend(c,d,ud) */
			c1 = c & MASK;
			c = (c<<8) & MASK;
			d1 = (d>>8) & ~MASK;
			d = d & ~MASK;
			c = ((d -(c >>8)) * ud + c ) & MASK;
			c1 = ((d1-(c1>>8)) * ud + c1) & MASK;
			/* (a,a1) = blend((a,a1),(c,c1),vd) */
			a = (((c >>8)-(a >>8)) * vd + a ) & MASK;
			a1 = (((c1>>8)-(a1>>8)) * vd + a1) & MASK;
		}
		sa = (a1>>24);
		sa = FZ_COMBINE(FZ_EXPAND(sa), FZ_EXPAND(cov));
		a1 |= 0xFF000000;
		d = *dst32++;
		d1 = d & MASK;
		d = (d<<8) & MASK;
		a = (((a >>8)-(d >>8)) * sa + d ) & MASK;
		a1 = (((a1>>8)-(d1>>8)) * sa + d1) & MASK;
		dst32[-1] = (a>>8) | a1;
		u += fa;
		v += fb;
	}
}

static void
img_w4i1o4_32bit(byte *rgba, byte * restrict src, byte cov, int len,
	byte * restrict dst, fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h-1;
	int alpha = FZ_EXPAND(rgba[3]);
	unsigned int rb = rgba[0] | (rgba[2] << 16);
	unsigned int ga = rgba[1] | 0xFF0000;
	unsigned int *dst32 = (unsigned int *)(void *)dst;

	if (alpha == 0)
		return;
	if (alpha != 256)
	{
		while (len--)
		{
			unsigned int ca, drb, dga, crb, cga;
			unsigned int a, b;
			cov += *src; *src = 0; src++;
			drb = *dst32++;
			ca = FZ_COMBINE(FZ_EXPAND(cov), alpha);
			if (ca != 0)
			{
				int ui, ui1, vi, vi1, ud, vd;
				/* a = samplemask(samples, w, h, u, v); */
				ui1 = 1;
				ui = u >> 16;
				if (ui < 0)
				{
					ui = 0;
					ui1 = 0;
				}
				else if (ui >= w-1)
				{
					ui = w-1;
					ui1 = 0;
				}
				vi1 = w;
				vi = v >> 16;
				if (vi < 0)
				{
					vi = 0;
					vi1 = 0;
				}
				else if (vi >= h)
				{
					vi = h;
					vi1 = 0;
				}
				ui += vi*w;
				a = samples[ui];
				b = samples[ui + ui1];
				a |= samples[ui + vi1]<<16;
				b |= samples[ui + ui1 + vi1]<<16;
				ud = (u>>8) & 0xFF;
				vd = (v>>8) & 0xFF;
				ud = FZ_EXPAND(ud);
				vd = FZ_EXPAND(vd);
				/* a = blend(a,b,ud) */
				a = ((b-a) * ud + (a<<8)) & MASK;
				/* a = blend(a,a>>16,vd) */
				a = (((a>>24)-(a>>8)) * vd + a);
				a = (a>>8) & 0xFF;
				ca = FZ_COMBINE(ca, FZ_EXPAND(a));
			}
			if (ca != 0)
			{
				dga = drb & MASK;
				drb = (drb<<8) & MASK;
				cga = ga - (dga>>8);
				crb = rb - (drb>>8);
				dga += cga * ca;
				drb += crb * ca;
				dga &= MASK;
				drb &= MASK;
				drb = dga | (drb>>8);
				dst32[-1] = drb;
			}
			u += fa;
			v += fb;
		}
	}
	else
	{
		while (len--)
		{
			unsigned int ca, drb, dga, crb, cga;
			unsigned int a, b;
			cov += *src; *src = 0; src++;
			drb = *dst32++;
			if (cov != 0)
			{
				int ui, ui1, vi, vi1, ud, vd;
				/* a = samplemask(samples, w, h, u, v); */
				ui1 = 1;
				ui = u >> 16;
				if (ui < 0)
				{
					ui = 0;
					ui1 = 0;
				}
				else if (ui >= w-1)
				{
					ui = w-1;
					ui1 = 0;
				}
				vi1 = w;
				vi = v >> 16;
				if (vi < 0)
				{
					vi = 0;
					vi1 = 0;
				}
				else if (vi >= h)
				{
					vi = h;
					vi1 = 0;
				}
				ui += vi*w;
				a = samples[ui];
				b = samples[ui + ui1];
				a |= samples[ui + vi1]<<16;
				b |= samples[ui + ui1 + vi1]<<16;
				ud = (u>>8) & 0xFF;
				vd = (v>>8) & 0xFF;
				ud = FZ_EXPAND(ud);
				vd = FZ_EXPAND(vd);
				/* a = blend(a,b,ud) */
				a = ((b-a) * ud + (a<<8)) & MASK;
				/* a = blend(a,a>>16,vd) */
				a = (((a>>24)-(a>>8)) * vd + a);
				a = (a>>8) & 0xFF;
				ca = FZ_COMBINE(FZ_EXPAND(cov),FZ_EXPAND(a));
				if (ca != 0)
				{
					if (ca == 256)
					{
						drb = (ga<<8) | rb;
					}
					else
					{
						dga = drb & MASK;
						drb = (drb<<8) & MASK;
						cga = ga - (dga>>8);
						crb = rb - (drb>>8);
						dga += cga * ca;
						drb += crb * ca;
						dga &= MASK;
						drb &= MASK;
						drb = dga | (drb>>8);
					}
					dst32[-1] = drb;
				}
			}
			u += fa;
			v += fb;
		}
	}
}

static void
img_1o1_32bit(byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h-1;

	while (len--)
	{
		unsigned int a, b;
		cov += *src; *src = 0; src++;
		if (cov != 0)
		{
			int ui, ui1, vi, vi1, ud, vd;
			/* sa = samplemask(samples, w, h, u, v); */
			ui1 = 1;
			ui = u >> 16;
			if (ui < 0)
			{
				ui = 0;
				ui1 = 0;
			}
			else if (ui >= w-1)
			{
				ui = w-1;
				ui1 = 0;
			}
			vi1 = w;
			vi = v >> 16;
			if (vi < 0)
			{
				vi = 0;
				vi1 = 0;
			}
			else if (vi >= h)
			{
				vi = h;
				vi1 = 0;
			}
			ui += vi*w;
			a = samples[ui];
			b = samples[ui + ui1];
			a |= samples[ui + vi1]<<16;
			b |= samples[ui + ui1 + vi1]<<16;
			ud = (u>>8) & 0xFF;
			vd = (v>>8) & 0xFF;
			ud = FZ_EXPAND(ud);
			vd = FZ_EXPAND(vd);
			/* a = blend(a,b,ud) */
			a = ((b-a) * ud + (a<<8)) & MASK;
			/* a = blend(a,a>>16,vd) */
			a = (((a>>24)-(a>>8)) * vd + a);
			a = (a>>8) & 0xFF;
			a = FZ_COMBINE(FZ_EXPAND(a), FZ_EXPAND(cov));
			if (a != 0)
			{
				if (a == 256)
					dst[0] = 255;
				else
					dst[0] = FZ_BLEND(255, dst[0], a);
			}
		}
		dst++;
		u += fa;
		v += fb;
	}
}

#endif

void fz_accelerate(void)
{
	if (sizeof(int) == 4 && sizeof(unsigned int) == 4 && !fz_is_big_endian())
	{
//		fz_path_w4i1o4 = path_w4i1o4_32bit;
//		fz_text_w4i1o4 = text_w4i1o4_32bit;
//		fz_img_4o4 = img_4o4_32bit;
//		fz_img_w4i1o4 = img_w4i1o4_32bit;
//		fz_img_1o1 = img_1o1_32bit;
	}

#ifdef HAVE_CPUDEP
	fz_accelerate_arch();
#endif
}
