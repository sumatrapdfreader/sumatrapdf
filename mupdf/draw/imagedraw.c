#include "fitz.h"

typedef unsigned char byte;

static inline byte
getmask(byte *s, int w, int h, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s[w * v + u];
}

static inline byte *
getargb(byte *s, int w, int h, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s + ((w * v + u) << 2);
}

static inline int
getcolor(byte *s, int w, int h, int n, int u, int v, int k)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s[w * v * n + u + k];
}

static inline int
lerp(int a, int b, int t)
{
	return a + (((b - a) * t) >> 16);
}

static inline void
lerpargb(byte *dst, byte *a, byte *b, int t)
{
	dst[0] = lerp(a[0], b[0], t);
	dst[1] = lerp(a[1], b[1], t);
	dst[2] = lerp(a[2], b[2], t);
	dst[3] = lerp(a[3], b[3], t);
}

static inline int
samplemask(byte *s, int w, int h, int u, int v)
{
	int ui = u >> 16;
	int vi = v >> 16;
	int ud = u & 0xFFFF;
	int vd = v & 0xFFFF;
	int a = getmask(s, w, h, ui, vi);
	int b = getmask(s, w, h, ui+1, vi);
	int c = getmask(s, w, h, ui, vi+1);
	int d = getmask(s, w, h, ui+1, vi+1);
	int ab = lerp(a, b, ud);
	int cd = lerp(c, d, ud);
	return lerp(ab, cd, vd);
}

static inline void
sampleargb(byte *s, int w, int h, int u, int v, byte *out)
{
	byte ab[4];
	byte cd[4];
	int ui = u >> 16;
	int vi = v >> 16;
	int ud = u & 0xFFFF;
	int vd = v & 0xFFFF;
	byte *a = getargb(s, w, h, ui, vi);
	byte *b = getargb(s, w, h, ui+1, vi);
	byte *c = getargb(s, w, h, ui, vi+1);
	byte *d = getargb(s, w, h, ui+1, vi+1);
	lerpargb(ab, a, b, ud);
	lerpargb(cd, c, d, ud);
	lerpargb(out, ab, cd, vd);
}

static inline void
samplecolor(byte *s, int w, int h, int n, int u, int v, byte *out)
{
	int ui = u >> 16;
	int vi = v >> 16;
	int ud = u & 0xFFFF;
	int vd = v & 0xFFFF;
	int k;
	for (k = 0; k < n; k++)
	{
		int a = getcolor(s, w, h, n, ui, vi, k);
		int b = getcolor(s, w, h, n, ui+1, vi, k);
		int c = getcolor(s, w, h, n, ui, vi+1, k);
		int d = getcolor(s, w, h, n, ui+1, vi+1, k);
		int ab = lerp(a, b, ud);
		int cd = lerp(c, d, ud);
		out[k] = lerp(ab, cd, vd);
	}
}

static void
img_1o1(byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h;
	byte sa;
	while (len--)
	{
		cov += *src; *src = 0; src++;
		sa = fz_mul255(cov, samplemask(samples, w, h, u, v));
		dst[0] = sa + fz_mul255(dst[0], 255 - sa);
		dst++;
		u += fa;
		v += fb;
	}
}

static void
img_4o4(byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h;
	byte argb[4];
	byte sa, ssa;
	while (len--)
	{
		cov += *src; *src = 0; src++;
		sampleargb(samples, w, h, u, v, argb);
		sa = fz_mul255(argb[0], cov);
		ssa = 255 - sa;
		dst[0] = sa + fz_mul255(dst[0], ssa);
		dst[1] = fz_mul255(argb[1], sa) + fz_mul255(dst[1], ssa);
		dst[2] = fz_mul255(argb[2], sa) + fz_mul255(dst[2], ssa);
		dst[3] = fz_mul255(argb[3], sa) + fz_mul255(dst[3], ssa);
		dst += 4;
		u += fa;
		v += fb;
	}
}

static void
img_w4i1o4(byte *argb, byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h;
	byte alpha = argb[0];
	byte r = argb[1];
	byte g = argb[2];
	byte b = argb[3];
	byte ca, cca;
	while (len--)
	{
		cov += *src; *src = 0; src++;
		ca = fz_mul255(cov, samplemask(samples, w, h, u, v));
		ca = fz_mul255(ca, alpha);
		cca = 255 - ca;
		dst[0] = ca + fz_mul255(dst[0], cca);
		dst[1] = fz_mul255(r, ca) + fz_mul255(dst[1], cca);
		dst[2] = fz_mul255(g, ca) + fz_mul255(dst[2], cca);
		dst[3] = fz_mul255(b, ca) + fz_mul255(dst[3], cca);
		dst += 4;
		u += fa;
		v += fb;
	}
}

void (*fz_img_1o1)(byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_1o1;
void (*fz_img_4o4)(byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_4o4;
void (*fz_img_w4i1o4)(byte*, byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_w4i1o4;
