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
getrgba(byte *s, int w, int h, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s + ((w * v + u) << 2);
}

static inline byte *
getga(byte *s, int w, int h, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s + ((w * v + u) << 1);
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
lerpga(byte *dst, byte *a, byte *b, int t)
{
	dst[0] = lerp(a[0], b[0], t);
	dst[1] = lerp(a[1], b[1], t);
}

static inline void
lerprgba(byte *dst, byte *a, byte *b, int t)
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
samplega(byte *s, int w, int h, int u, int v, byte *out)
{
	byte ab[4];
	byte cd[4];
	int ui = u >> 16;
	int vi = v >> 16;
	int ud = u & 0xFFFF;
	int vd = v & 0xFFFF;
	byte *a = getga(s, w, h, ui, vi);
	byte *b = getga(s, w, h, ui+1, vi);
	byte *c = getga(s, w, h, ui, vi+1);
	byte *d = getga(s, w, h, ui+1, vi+1);
	lerpga(ab, a, b, ud);
	lerpga(cd, c, d, ud);
	lerpga(out, ab, cd, vd);
}

static inline void
samplergba(byte *s, int w, int h, int u, int v, byte *out)
{
	byte ab[4];
	byte cd[4];
	int ui = u >> 16;
	int vi = v >> 16;
	int ud = u & 0xFFFF;
	int vd = v & 0xFFFF;
	byte *a = getrgba(s, w, h, ui, vi);
	byte *b = getrgba(s, w, h, ui+1, vi);
	byte *c = getrgba(s, w, h, ui, vi+1);
	byte *d = getrgba(s, w, h, ui+1, vi+1);
	lerprgba(ab, a, b, ud);
	lerprgba(cd, c, d, ud);
	lerprgba(out, ab, cd, vd);
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

	while (len--)
	{
		int sa;
		cov += *src; *src = 0; src++;
		if (cov != 0)
		{
			sa = samplemask(samples, w, h, u, v);
			sa = FZ_COMBINE(FZ_EXPAND(sa), FZ_EXPAND(cov));
			if (sa != 0)
			{
				dst[0] = FZ_BLEND(255, dst[0], sa);
			}
		}
		dst++;
		u += fa;
		v += fb;
	}
}

static void
img_2o2(byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h;
	byte ga[2];

	while (len--)
	{
		int sa;
		cov += *src; *src = 0; src++;
		if (cov != 0)
		{
			samplega(samples, w, h, u, v, ga);
			sa = FZ_COMBINE(FZ_EXPAND(ga[1]), FZ_EXPAND(cov));
			if (sa != 0)
			{
				dst[0] = FZ_BLEND(ga[0], dst[0], sa);
				dst[1] = FZ_BLEND(255, dst[1], sa);
			}
		}
		dst += 2;
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
	byte rgba[4];

	while (len--)
	{
		int sa;
		cov += *src; *src = 0; src++;
		if (cov != 0)
		{
			samplergba(samples, w, h, u, v, rgba);
			sa = FZ_COMBINE(FZ_EXPAND(rgba[3]), FZ_EXPAND(cov));
			if (sa != 0)
			{
				dst[0] = FZ_BLEND(rgba[0], dst[0], sa);
				dst[1] = FZ_BLEND(rgba[1], dst[1], sa);
				dst[2] = FZ_BLEND(rgba[2], dst[2], sa);
				dst[3] = FZ_BLEND(255, dst[3], sa);
			}
		}
		dst += 4;
		u += fa;
		v += fb;
	}
}

static void
img_w2i1o2(byte *ga, byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h;
	byte g = ga[0];
	byte a = ga[1];

	if (a == 0)
		return;
	if (a != 255)
	{
		while (len--)
		{
			int ca;
			cov += *src; *src = 0; src++;
			if (cov != 0)
			{
				ca = samplemask(samples, w, h, u, v);
				ca = FZ_COMBINE(FZ_EXPAND(cov), FZ_EXPAND(ca));
				ca = FZ_COMBINE(ca, FZ_EXPAND(a));
				if (ca != 0)
				{
					dst[0] = FZ_BLEND(g, dst[0], ca);
					dst[1] = FZ_BLEND(255, dst[1], ca);
				}
			}
			dst += 2;
			u += fa;
			v += fb;
		}
	}
	else
	{
		while (len--)
		{
			int ca;
			cov += *src; *src = 0; src++;
			if (cov != 0)
			{
				ca = samplemask(samples, w, h, u, v);
				ca = FZ_COMBINE(FZ_EXPAND(cov), FZ_EXPAND(ca));
				if (ca != 0)
				{
					dst[0] = FZ_BLEND(g, dst[0], ca);
					dst[1] = FZ_BLEND(255, dst[1], ca);
				}
			}
			dst += 2;
			u += fa;
			v += fb;
		}
	}
}

static void
img_w4i1o4(byte *rgba, byte * restrict src, byte cov, int len, byte * restrict dst,
	fz_pixmap *image, int u, int v, int fa, int fb)
{
	byte *samples = image->samples;
	int w = image->w;
	int h = image->h;
	byte r = rgba[0];
	byte g = rgba[1];
	byte b = rgba[2];
	byte a = rgba[3];

	if (a == 0)
		return;
	if (a != 255)
	{
		while (len--)
		{
			int ca;
			cov += *src; *src = 0; src++;
			if (cov != 0)
			{
				ca = samplemask(samples, w, h, u, v);
				ca = FZ_COMBINE(FZ_EXPAND(cov), FZ_EXPAND(ca));
				ca = FZ_COMBINE(ca, FZ_EXPAND(a));
				if (ca != 0)
				{
					dst[0] = FZ_BLEND(r, dst[0], ca);
					dst[1] = FZ_BLEND(g, dst[1], ca);
					dst[2] = FZ_BLEND(b, dst[2], ca);
					dst[3] = FZ_BLEND(255, dst[3], ca);
				}
			}
			dst += 4;
			u += fa;
			v += fb;
		}
	}
	else
	{
		while (len--)
		{
			int ca;
			cov += *src; *src = 0; src++;
			if (cov != 0)
			{
				ca = samplemask(samples, w, h, u, v);
				ca = FZ_COMBINE(FZ_EXPAND(cov), FZ_EXPAND(ca));
				if (ca != 0)
				{
					dst[0] = FZ_BLEND(r, dst[0], ca);
					dst[1] = FZ_BLEND(g, dst[1], ca);
					dst[2] = FZ_BLEND(b, dst[2], ca);
					dst[3] = FZ_BLEND(255, dst[3], ca);
				}
			}
			dst += 4;
			u += fa;
			v += fb;
		}
	}
}

void (*fz_img_1o1)(byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_1o1;
void (*fz_img_2o2)(byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_2o2;
void (*fz_img_4o4)(byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_4o4;
void (*fz_img_w2i1o2)(byte*, byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_w2i1o2;
void (*fz_img_w4i1o4)(byte*, byte*, byte, int, byte*, fz_pixmap *image, int u, int v, int fa, int fb) = img_w4i1o4;
