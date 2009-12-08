#include "fitz_base.h"
#include "fitz_tree.h"
#include "fitz_draw.h"

typedef unsigned char byte;

/*
 * Apply decode parameters
 */

static void decodetile(fz_pixmap *pix, int skip, float *decode)
{
	int min[FZ_MAXCOLORS];
	int max[FZ_MAXCOLORS];
	int sub[FZ_MAXCOLORS];
	int needed = 0;
	int n = pix->n;
	byte *p = pix->samples;
	int wh = pix->w * pix->h;
	int i;
	int justinvert = 1;

	min[0] = 0;
	max[0] = 255;
	sub[0] = 255;

	for (i = skip; i < n; i++)
	{
		min[i] = decode[(i - skip) * 2] * 255;
		max[i] = decode[(i - skip) * 2 + 1] * 255;
		sub[i] = max[i] - min[i];
		needed |= (min[i] != 0) |  (max[i] != 255);
		justinvert &= min[i] == 255 && max[i] == 0 && sub[i] == -255;
	}

	if (!needed)
		return;

	switch (n) {
	case 1:
		while (wh--)
		{
			p[0] = min[0] + fz_mul255(sub[0], p[0]);
			p ++;
		}
		break;
	case 2:
		if (justinvert) {
			unsigned *wp = (unsigned *)p;

			if ((((char *)wp - (char *)0) & 3) == 0) {
				int hwh = wh / 2;
				wh = wh - 2 * hwh;
				while(hwh--) {
					unsigned in = *wp;
#if BYTE_ORDER == LITTLE_ENDIAN
					unsigned out = in ^ 0xff00ff00;
#else
					unsigned out = in ^ 0x00ff00ff;
#endif
					*wp++ = out;
				}
				p = (byte *)wp;
			}
			if (wh--) {
				p[0] = p[0];
				p[1] = 255 - p[1];
				p += 2;
			}
		}
		else
			while (wh--)
		{
			p[0] = min[0] + fz_mul255(sub[0], p[0]);
			p[1] = min[1] + fz_mul255(sub[1], p[1]);
			p += 2;
		}
		break;
	default:
		while (wh--)
		{
			for (i = 0; i < n; i++)
				p[i] = min[i] + fz_mul255(sub[i], p[i]);
			p += n;
		}
	}
}

/*
 * Unpack image samples and optionally pad pixels with opaque alpha
 */

#define tbit(buf,x) ((buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1 ) * 255
#define ttwo(buf,x) ((buf[x >> 2] >> ( ( 3 - (x & 3) ) << 1 ) ) & 3 ) * 85
#define tnib(buf,x) ((buf[x >> 1] >> ( ( 1 - (x & 1) ) << 2 ) ) & 15 ) * 17
#define toct(buf,x) (buf[x])
#define thex(buf,x) (buf[x << 1])

static byte t1pad0[256][8];
static byte t1pad1[256][16];

static void init1(void)
{
	static int inited = 0;
	byte bits[1];
	int i, k, x;

	if (inited)
		return;

	for (i = 0; i < 256; i++)
	{
		bits[0] = i;
		for (k = 0; k < 8; k++)
		{
			x = tbit(bits, k);
			t1pad0[i][k] = x;
			t1pad1[i][k * 2 + 0] = 255;
			t1pad1[i][k * 2 + 1] = x;
		}
	}

	inited = 1;
}

static void loadtile1(byte * restrict src, int sw, byte * restrict dst, int dw, int w, int h, int pad)
{
	byte *sp;
	byte *dp;
	int x;

	init1();

	if (pad == 0)
	{
		int w3 = w >> 3;
		while (h--)
		{
			sp = src;
			dp = dst;
			for (x = 0; x < w3; x++)
			{
				memcpy(dp, t1pad0[*sp++], 8);
				dp += 8;
			}
			x = x << 3;
			if (x < w)
				memcpy(dp, t1pad0[*sp], w - x);
			src += sw;
			dst += dw;
		}
	}

	else if (pad == 1)
	{
		int w3 = w >> 3;
		while (h--)
		{
			sp = src;
			dp = dst;
			for (x = 0; x < w3; x++)
			{
				memcpy(dp, t1pad1[*sp++], 16);
				dp += 16;
			}
			x = x << 3;
			if (x < w)
				memcpy(dp, t1pad1[*sp], (w - x) << 1);
			src += sw;
			dst += dw;
		}
	}

	else
	{
		while (h--)
		{
			dp = dst;
			for (x = 0; x < w; x++)
			{
				if ((x % pad) == 0)
					*dp++ = 255;
				*dp++ = tbit(src, x);
			}
			src += sw;
			dst += dw;
		}
	}
}

#define TILE(getf) \
{ \
	int x; \
	if (!pad) \
		while (h--) \
		{ \
			for (x = 0; x < w; x++) \
				dst[x] = getf(src, x); \
			src += sw; \
			dst += dw; \
		} \
	else { \
		int tpad; \
		while (h--) \
		{ \
			byte *dp = dst; \
			tpad = 0; \
			for (x = 0; x < w; x++) \
			{ \
				if (!tpad--) { \
					tpad = pad-1; \
					*dp++ = 255; \
				} \
				*dp++ = getf(src, x); \
			} \
			src += sw; \
			dst += dw; \
		} \
	} \
}

static void loadtile2(byte * restrict src, int sw, byte * restrict dst, int dw, int w, int h, int pad)
TILE(ttwo)
static void loadtile4(byte * restrict src, int sw, byte * restrict dst, int dw, int w, int h, int pad)
TILE(tnib)
static void loadtile8(byte * restrict src, int sw, byte * restrict dst, int dw, int w, int h, int pad)
TILE(toct)
static void loadtile16(byte * restrict src, int sw, byte * restrict dst, int dw, int w, int h, int pad)
TILE(thex)

void (*fz_decodetile)(fz_pixmap *pix, int skip, float *decode) = decodetile;
void (*fz_loadtile1)(byte*, int sw, byte*, int dw, int w, int h, int pad) = loadtile1;
void (*fz_loadtile2)(byte*, int sw, byte*, int dw, int w, int h, int pad) = loadtile2;
void (*fz_loadtile4)(byte*, int sw, byte*, int dw, int w, int h, int pad) = loadtile4;
void (*fz_loadtile8)(byte*, int sw, byte*, int dw, int w, int h, int pad) = loadtile8;
void (*fz_loadtile16)(byte*, int sw, byte*, int dw, int w, int h, int pad) = loadtile16;

