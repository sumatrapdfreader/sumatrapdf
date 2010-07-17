#include "fitz.h"

/*
 * Unpack image samples and optionally pad pixels with opaque alpha
 */

#define get1(buf,x) ((buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1 )
#define get2(buf,x) ((buf[x >> 2] >> ( ( 3 - (x & 3) ) << 1 ) ) & 3 )
#define get4(buf,x) ((buf[x >> 1] >> ( ( 1 - (x & 1) ) << 2 ) ) & 15 )
#define get8(buf,x) (buf[x])
#define get16(buf,x) (buf[x << 1])

static unsigned char get1tab1[256][8];
static unsigned char get1tab1p[256][16];
static unsigned char get1tab255[256][8];
static unsigned char get1tab255p[256][16];

static void
initget1tables(void)
{
	static int once = 0;
	unsigned char bits[1];
	int i, k, x;

	/* TODO: mutex lock here */

	if (once)
		return;

	for (i = 0; i < 256; i++)
	{
		bits[0] = i;
		for (k = 0; k < 8; k++)
		{
			x = get1(bits, k);

			get1tab1[i][k] = x;
			get1tab1p[i][k * 2] = x;
			get1tab1p[i][k * 2 + 1] = 255;

			get1tab255[i][k] = x * 255;
			get1tab255p[i][k * 2] = x * 255;
			get1tab255p[i][k * 2 + 1] = 255;
		}
	}

	once = 1;
}

void
fz_unpacktile(fz_pixmap *dst, unsigned char * restrict src, int n, int depth, int stride, int scale)
{
	int pad, x, y, k;
	int w = dst->w;

	pad = 0;
	if (dst->n > n)
		pad = 255;

	initget1tables();

	for (y = 0; y < dst->h; y++)
	{
		unsigned char *sp = src + y * stride;
		unsigned char *dp = dst->samples + y * (dst->w * dst->n);

		/* Specialized loops */

		if (n == 1 && depth == 1 && scale == 1 && !pad)
		{
			int w3 = w >> 3;
			for (x = 0; x < w3; x++)
			{
				memcpy(dp, get1tab1[*sp++], 8);
				dp += 8;
			}
			x = x << 3;
			if (x < w)
				memcpy(dp, get1tab1[*sp], w - x);
		}

		else if (n == 1 && depth == 1 && scale == 255 && !pad)
		{
			int w3 = w >> 3;
			for (x = 0; x < w3; x++)
			{
				memcpy(dp, get1tab255[*sp++], 8);
				dp += 8;
			}
			x = x << 3;
			if (x < w)
				memcpy(dp, get1tab255[*sp], w - x);
		}

		else if (n == 1 && depth == 1 && scale == 1 && pad)
		{
			int w3 = w >> 3;
			for (x = 0; x < w3; x++)
			{
				memcpy(dp, get1tab1p[*sp++], 16);
				dp += 16;
			}
			x = x << 3;
			if (x < w)
				memcpy(dp, get1tab1p[*sp], (w - x) << 1);
		}

		else if (n == 1 && depth == 1 && scale == 255 && pad)
		{
			int w3 = w >> 3;
			for (x = 0; x < w3; x++)
			{
				memcpy(dp, get1tab255p[*sp++], 16);
				dp += 16;
			}
			x = x << 3;
			if (x < w)
				memcpy(dp, get1tab255p[*sp], (w - x) << 1);
		}

		else if (depth == 8 && !pad)
		{
			int len = w * n;
			while (len--)
				*dp++ = *sp++;
		}

		else if (depth == 8 && pad)
		{
			for (x = 0; x < w; x++)
			{
				for (k = 0; k < n; k++)
					*dp++ = *sp++;
				*dp++ = 255;
			}
		}

		else
		{
			int b = 0;
			for (x = 0; x < w; x++)
			{
				for (k = 0; k < n; k++)
				{
					switch (depth)
					{
					case 1: *dp++ = get1(sp, b) * scale; break;
					case 2: *dp++ = get2(sp, b) * scale; break;
					case 4: *dp++ = get4(sp, b) * scale; break;
					case 8: *dp++ = get8(sp, b); break;
					case 16: *dp++ = get16(sp, b); break;
					}
					b++;
				}
				if (pad)
					*dp++ = 255;
			}
		}
	}
}

/*
 * Apply decode parameters and scale integers
 */

void
fz_decodetile(fz_pixmap *pix, float *decode)
{
	int min[FZ_MAXCOLORS + 2];
	int max[FZ_MAXCOLORS + 2];
	int sub[FZ_MAXCOLORS + 2];
	unsigned char *p = pix->samples;
	int len = pix->w * pix->h;
	int n = pix->n;
	int needed;
	int k;

	needed = 0;
	for (k = 0; k < n; k++)
	{
		min[k] = decode[k * 2] * 255;
		max[k] = decode[k * 2 + 1] * 255;
		sub[k] = max[k] - min[k];
		needed |= min[k] != 0 || max[k] != 255;
	}

	if (!needed)
		return;

	while (len--)
	{
		for (k = 0; k < n; k++)
			p[k] = min[k] + fz_mul255(sub[k], p[k]);
		p += n;
	}
}
