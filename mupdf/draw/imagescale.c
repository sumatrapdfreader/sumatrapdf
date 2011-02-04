#include "fitz.h"

typedef unsigned char byte;

static inline void srown(byte * restrict src, byte * restrict dst, int w, int denom, int n)
{
	int invdenom = (1<<16) / denom;
	int x, left, k;
	unsigned sum[FZ_MAXCOLORS];

	left = 0;
	for (k = 0; k < n; k++)
		sum[k] = 0;

	for (x = 0; x < w; x++)
	{
		for (k = 0; k < n; k++)
			sum[k] += src[x * n + k];
		if (++left == denom)
		{
			left = 0;
			for (k = 0; k < n; k++)
			{
				dst[k] = (sum[k] * invdenom + (1<<15)) >> 16;
				sum[k] = 0;
			}
			dst += n;
		}
	}

	/* left overs */
	if (left)
		for (k = 0; k < n; k++)
			dst[k] = sum[k] / left;
}

/* special-case common 1-5 components - the compiler optimizes this */
static inline void srowc(byte * restrict src, byte * restrict dst, int w, int denom, int n)
{
	int invdenom = (1<<16) / denom;
	int x, left;
	unsigned sum1 = 0;
	unsigned sum2 = 0;
	unsigned sum3 = 0;
	unsigned sum4 = 0;
	unsigned sum5 = 0;

	assert(n <= 5);

	left = denom;

	for (x = w; x > 0; x--)
	{
		sum1 += *src++;
		/* the compiler eliminates these if-tests */
		if (n >= 2)
			sum2 += *src++;
		if (n >= 3)
			sum3 += *src++;
		if (n >= 4)
			sum4 += *src++;
		if (n >= 5)
			sum5 += *src++;

		if (--left == 0)
		{
			left = denom;

			*dst++ = (sum1 * invdenom + (1<<15)) >> 16;
			sum1 = 0;
			if (n >= 2) {
				*dst++ = (sum2 * invdenom + (1<<15)) >> 16;
				sum2 = 0;
			}
			if (n >= 3) {
				*dst++ = (sum3 * invdenom + (1<<15)) >> 16;
				sum3 = 0;
			}
			if (n >= 4) {
				*dst++ = (sum4 * invdenom + (1<<15)) >> 16;
				sum4 = 0;
			}
			if (n >= 5) {
				*dst++ = (sum5 * invdenom + (1<<15)) >> 16;
				sum5 = 0;
			}
		}
	}

	/* left overs */
	left = denom - left;
	if (left) {
		*dst++ = sum1 / left;
		if (n >= 2)
			*dst++ = sum2 / left;
		if (n >= 3)
			*dst++ = sum3 / left;
		if (n >= 4)
			*dst++ = sum4 / left;
		if (n >= 5)
			*dst++ = sum5 / left;
	}
}

static void srow1(byte * restrict src, byte * restrict dst, int w, int denom)
{
	srowc(src, dst, w, denom, 1);
}

static void srow2(byte * restrict src, byte * restrict dst, int w, int denom)
{
	srowc(src, dst, w, denom, 2);
}

static void srow4(byte * restrict src, byte * restrict dst, int w, int denom)
{
	srowc(src, dst, w, denom, 4);
}

static void srow5(byte * restrict src, byte * restrict dst, int w, int denom)
{
	srowc(src, dst, w, denom, 5);
}

static inline void scoln(byte * restrict src, byte * restrict dst, int w, int denom, int n)
{
	int invdenom = (1<<16) / denom;
	int x, y, k;
	byte *s;
	int sum[FZ_MAXCOLORS];

	for (x = 0; x < w; x++)
	{
		s = src + (x * n);
		for (k = 0; k < n; k++)
			sum[k] = 0;
		for (y = 0; y < denom; y++)
			for (k = 0; k < n; k++)
				sum[k] += s[y * w * n + k];
		for (k = 0; k < n; k++)
			dst[k] = (sum[k] * invdenom + (1<<15)) >> 16;
		dst += n;
	}
}

static inline void scolc(byte * restrict src, byte * restrict dst, int w, int denom, int n)
{
	int invdenom = (1<<16) / denom;
	int x, y;
	int sum0;
	int sum1;
	int sum2;
	int sum3;
	int sum4;

	assert(n <= 5);

	x = w;
	w *= n;
	for (; x > 0; x--)
	{
		sum0 = 0;
		sum1 = 0;
		sum2 = 0;
		sum3 = 0;
		sum4 = 0;
		for (y = denom; y > 0; y--)
		{
			sum0 += src[0];
			if (n >= 2)
				sum1 += src[1];
			if (n >= 3)
				sum2 += src[2];
			if (n >= 4)
				sum3 += src[3];
			if (n >= 5)
				sum4 += src[4];
			src += w;
		}
		src += n - denom * w;
		*dst++ = (sum0 * invdenom + (1<<15)) >> 16;
		if (n >= 2)
			*dst++ = (sum1 * invdenom + (1<<15)) >> 16;
		if (n >= 3)
			*dst++ = (sum2 * invdenom + (1<<15)) >> 16;
		if (n >= 4)
			*dst++ = (sum3 * invdenom + (1<<15)) >> 16;
		if (n >= 5)
			*dst++ = (sum4 * invdenom + (1<<15)) >> 16;
	}
}

static void scol1(byte * restrict src, byte * restrict dst, int w, int denom)
{
	scolc(src, dst, w, denom, 1);
}

static void scol2(byte * restrict src, byte * restrict dst, int w, int denom)
{
	scolc(src, dst, w, denom, 2);
}

static void scol4(byte * restrict src, byte * restrict dst, int w, int denom)
{
	scolc(src, dst, w, denom, 4);
}

static void scol5(byte * restrict src, byte * restrict dst, int w, int denom)
{
	scolc(src, dst, w, denom, 5);
}

void (*fz_srown)(byte *restrict, byte *restrict, int w, int denom, int n) = srown;
void (*fz_srow1)(byte *restrict, byte *restrict, int w, int denom) = srow1;
void (*fz_srow2)(byte *restrict, byte *restrict, int w, int denom) = srow2;
void (*fz_srow4)(byte *restrict, byte *restrict, int w, int denom) = srow4;
void (*fz_srow5)(byte *restrict, byte *restrict, int w, int denom) = srow5;

void (*fz_scoln)(byte *restrict, byte *restrict, int w, int denom, int n) = scoln;
void (*fz_scol1)(byte *restrict, byte *restrict, int w, int denom) = scol1;
void (*fz_scol2)(byte *restrict, byte *restrict, int w, int denom) = scol2;
void (*fz_scol4)(byte *restrict, byte *restrict, int w, int denom) = scol4;
void (*fz_scol5)(byte *restrict, byte *restrict, int w, int denom) = scol5;

fz_pixmap *
fz_scalepixmap(fz_pixmap *src, int xdenom, int ydenom)
{
	fz_pixmap *dst;
	unsigned char *buf;
	int y, iy, oy;
	int ow, oh, n;
	int remaining;

	void (*srowx)(byte * restrict src, byte * restrict dst, int w, int denom) = nil;
	void (*scolx)(byte * restrict src, byte * restrict dst, int w, int denom) = nil;

	ow = (src->w + xdenom - 1) / xdenom;
	oh = (src->h + ydenom - 1) / ydenom;
	n = src->n;

	buf = fz_calloc(ydenom, ow * n);

	dst = fz_newpixmap(src->colorspace, 0, 0, ow, oh);

	switch (n)
	{
	case 1: srowx = fz_srow1; scolx = fz_scol1; break;
	case 2: srowx = fz_srow2; scolx = fz_scol2; break;
	case 4: srowx = fz_srow4; scolx = fz_scol4; break;
	case 5: srowx = fz_srow5; scolx = fz_scol5; break;
	}

	if (srowx && scolx)
	{
		for (y = 0, oy = 0; y < (src->h / ydenom) * ydenom; y += ydenom, oy++)
		{
			for (iy = 0; iy < ydenom; iy++)
			{
				srowx(src->samples + (y + iy) * src->w * n,
					buf + iy * ow * n,
					src->w, xdenom);
			}
			scolx(buf, dst->samples + oy * dst->w * n, dst->w, ydenom);
		}

		remaining = src->h - y;
		if (remaining)
		{
			for (iy = 0; iy < remaining; iy++)
			{
				srowx(src->samples + (y + iy) * src->w * n,
					buf + iy * ow * n,
					src->w, xdenom);
			}
			scolx(buf, dst->samples + oy * dst->w * n, dst->w, remaining);
		}
	}

	else
	{
		for (y = 0, oy = 0; y < (src->h / ydenom) * ydenom; y += ydenom, oy++)
		{
			for (iy = 0; iy < ydenom; iy++)
			{
				fz_srown(src->samples + (y + iy) * src->w * n,
					buf + iy * ow * n,
					src->w, xdenom, n);
			}
			fz_scoln(buf, dst->samples + oy * dst->w * n, dst->w, ydenom, n);
		}

		remaining = src->h - y;
		if (remaining)
		{
			for (iy = 0; iy < remaining; iy++)
			{
				fz_srown(src->samples + (y + iy) * src->w * n,
					buf + iy * ow * n,
					src->w, xdenom, n);
			}
			fz_scoln(buf, dst->samples + oy * dst->w * n, dst->w, remaining, n);
		}
	}

	fz_free(buf);
	return dst;
}
