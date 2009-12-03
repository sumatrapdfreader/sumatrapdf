#include "fitz_base.h"
#include "fitz_tree.h"
#include "fitz_draw.h"

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

	left = 0;

	for (x = 0; x < w; x++)
	{
		sum1 += src[x * n + 0];
		/* the compiler eliminates these if-tests */
		if (n >= 2)
			sum2 += src[x * n + 1];
		if (n >= 3)
			sum3 += src[x * n + 2];
		if (n >= 4)
			sum4 += src[x * n + 3];
		if (n >= 5)
			sum5 += src[x * n + 4];

		if (++left == denom)
		{
			left = 0;

			dst[0] = (sum1 * invdenom + (1<<15)) >> 16;
			sum1 = 0;
			if (n >= 2) {
				dst[1] = (sum2 * invdenom + (1<<15)) >> 16;
				sum2 = 0;
			}
			if (n >= 3) {
				dst[2] = (sum3 * invdenom + (1<<15)) >> 16;
				sum3 = 0;
			}
			if (n >= 4) {
				dst[3] = (sum4 * invdenom + (1<<15)) >> 16;
				sum4 = 0;
			}
			if (n >= 5) {
				dst[4] = (sum5 * invdenom + (1<<15)) >> 16;
				sum5 = 0;
			}


			dst += n;
		}
	}

	/* left overs */
	if (left) {
		dst[0] = sum1 / left;
		if (n >=2)
			dst[1] = sum2 / left;
		if (n >=3)
			dst[2] = sum3 / left;
		if (n >=4)
			dst[3] = sum4 / left;
		if (n >=5)
			dst[4] = sum5 / left;
	}
}

static void srow1(byte *src, byte *dst, int w, int denom)
{
	srowc(src, dst, w, denom, 1);
}

static void srow2(byte *src, byte *dst, int w, int denom)
{
	srowc(src, dst, w, denom, 2);
}

static void srow4(byte *src, byte *dst, int w, int denom)
{
	srowc(src, dst, w, denom, 4);
}

static void srow5(byte *src, byte *dst, int w, int denom)
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

static void scol1(byte *src, byte *dst, int w, int denom)
{
	scoln(src, dst, w, denom, 1);
}

static void scol2(byte *src, byte *dst, int w, int denom)
{
	scoln(src, dst, w, denom, 2);
}

static void scol4(byte *src, byte *dst, int w, int denom)
{
	scoln(src, dst, w, denom, 4);
}

static void scol5(byte *src, byte *dst, int w, int denom)
{
	scoln(src, dst, w, denom, 5);
}

void (*fz_srown)(byte *src, byte *dst, int w, int denom, int n) = srown;
void (*fz_srow1)(byte *src, byte *dst, int w, int denom) = srow1;
void (*fz_srow2)(byte *src, byte *dst, int w, int denom) = srow2;
void (*fz_srow4)(byte *src, byte *dst, int w, int denom) = srow4;
void (*fz_srow5)(byte *src, byte *dst, int w, int denom) = srow5;

void (*fz_scoln)(byte *src, byte *dst, int w, int denom, int n) = scoln;
void (*fz_scol1)(byte *src, byte *dst, int w, int denom) = scol1;
void (*fz_scol2)(byte *src, byte *dst, int w, int denom) = scol2;
void (*fz_scol4)(byte *src, byte *dst, int w, int denom) = scol4;
void (*fz_scol5)(byte *src, byte *dst, int w, int denom) = scol5;

fz_error
fz_newscaledpixmap(fz_pixmap **dstp, int w, int h, int n, int xdenom, int ydenom)
{
	int ow = (w + xdenom - 1) / xdenom;
	int oh = (h + ydenom - 1) / ydenom;
	return fz_newpixmap(dstp, 0, 0, ow, oh, n);
}

/* TODO: refactor */
fz_error
fz_scalepixmaptile(fz_pixmap *dst, int xoffs, int yoffs, fz_pixmap *src, int xdenom, int ydenom)
{
	unsigned char *buf;
	unsigned char *dstsamples;
	int y, iy, oy;
	int ow, oh, n;
	int remaining;

	void (*srowx)(byte *src, byte *dst, int w, int denom) = nil;
	void (*scolx)(byte *src, byte *dst, int w, int denom) = nil;

	ow = (src->w + xdenom - 1) / xdenom;
	oh = (src->h + ydenom - 1) / ydenom;
	xoffs /= xdenom;
	yoffs /= ydenom;
	n = src->n;

	assert(xoffs == 0); /* don't handle stride properly yet */
	assert(dst->n == n);
	assert(dst->w >= xoffs + ow && dst->h >= yoffs + oh);

	buf = fz_malloc(ow * n * ydenom);
	if (!buf)
		return fz_rethrow(-1, "out of memory");

	switch (n)
	{
	case 1: srowx = fz_srow1; scolx = fz_scol1; break;
	case 2: srowx = fz_srow2; scolx = fz_scol2; break;
	case 4: srowx = fz_srow4; scolx = fz_scol4; break;
	case 5: srowx = fz_srow5; scolx = fz_scol5; break;
	}

	dstsamples = dst->samples + (yoffs * dst->w + xoffs)*dst->n;
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
			scolx(buf, dstsamples + oy * dst->w * n, ow, ydenom);
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
			scolx(buf, dstsamples + oy * dst->w * n, ow, remaining);
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
			fz_scoln(buf, dstsamples + oy * dst->w * n, ow, ydenom, n);
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
			fz_scoln(buf, dstsamples + oy * dst->w * n, ow, remaining, n);
		}
	}

	fz_free(buf);
	return fz_okay;
}

fz_error
fz_scalepixmap(fz_pixmap **dstp, fz_pixmap *src, int xdenom, int ydenom)
{
	fz_error error;
	fz_pixmap *dst;
	unsigned char *buf;
	int y, iy, oy;
	int ow, oh, n;
	int remaining;

	void (*srowx)(byte *src, byte *dst, int w, int denom) = nil;
	void (*scolx)(byte *src, byte *dst, int w, int denom) = nil;

	ow = (src->w + xdenom - 1) / xdenom;
	oh = (src->h + ydenom - 1) / ydenom;
	n = src->n;

	buf = fz_malloc(ow * n * ydenom);
	if (!buf)
		return fz_rethrow(-1, "out of memory");

	error = fz_newpixmap(&dst, 0, 0, ow, oh, src->n);
	if (error)
	{
		fz_free(buf);
		return error;
	}

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
	*dstp = dst;
	return fz_okay;
}

