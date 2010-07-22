#include "fitz.h"

#define noNEAREST

typedef unsigned char byte;

/* Sample image and clamp to edge */

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
getga(byte *s, int w, int h, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s + ((w * v + u) << 1);
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

static inline int
getcolor(byte *s, int w, int h, int n, int u, int v, int k)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s[w * v * n + u + k];
}

/* Bi-linear interpolation of sample */

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
#ifdef NEAREST
	return getmask(s, w, h, u >> 16, v >> 16);
#else
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
#endif
}

static inline void
samplega(byte *s, int w, int h, int u, int v, int *gout, int *aout)
{
#ifdef NEAREST
	byte *ga = getga(s, w, h, u >> 16, v >> 16);
	*gout = ga[0];
	*aout = ga[1];
#else
	byte ab[2];
	byte cd[2];
	byte abcd[2];
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
	lerpga(abcd, ab, cd, vd);
	*gout = abcd[0];
	*aout = abcd[0];
#endif
}

static inline void
samplergba(byte *s, int w, int h, int u, int v, int *rout, int *gout, int *bout, int *aout)
{
#ifdef NEAREST
	byte *rgba = getrgba(s, w, h, u >> 16, v >> 16);
	*rout = rgba[0];
	*gout = rgba[1];
	*bout = rgba[2];
	*aout = rgba[3];
#else
	byte ab[4];
	byte cd[4];
	byte abcd[4];
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
	lerprgba(abcd, ab, cd, vd);
	*rout = abcd[0];
	*gout = abcd[1];
	*bout = abcd[2];
	*aout = abcd[3];
#endif
}

static inline void
samplecolor(byte *s, int w, int h, int n, int u, int v, byte *out)
{
#ifdef NEAREST
	int k;
	for (k = 0; k < n; k++)
		out[k] = getcolor(s, w, h, n, u >> 16, v >> 16, k);
#else
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
#endif
}

/* Blend source image scanline over destination */

#define INSIDEU u >= 0 && u <= (sw << 16)
#define INSIDEV v >= 0 && v <= (sh << 16)
#define INSIDE INSIDEU && INSIDEV

static inline void
fz_blendscan1(unsigned char *dp, unsigned char *sp, int sw, int sh,
	int u, int v, int fa, int fb, int w)
{
	while (w--)
	{
		if (INSIDE)
		{
			int a = samplemask(sp, sw, sh, u, v);
			dp[0] = a + fz_mul255(dp[0], 255 - a);
		}
		dp ++;
		u += fa;
		v += fb;
	}
}

static inline void
fz_blendscan2(unsigned char *dp, unsigned char *sp, int sw, int sh,
	int u, int v, int fa, int fb, int w)
{
	while (w--)
	{
		if (INSIDE)
		{
			int g, a, t;
			samplega(sp, sw, sh, u, v, &g, &a);
			t = 255 - a;
			dp[0] = g + fz_mul255(dp[0], t);
			dp[1] = a + fz_mul255(dp[1], t);
		}
		dp += 2;
		u += fa;
		v += fb;
	}
}

static inline void
fz_blendscan4(unsigned char *dp, unsigned char *sp, int sw, int sh,
	int u, int v, int fa, int fb, int w)
{
	while (w--)
	{
		if (INSIDE)
		{
			int r, g, b, a, t;
			samplergba(sp, sw, sh, u, v, &r, &g, &b, &a);
			t = 255 - a;
			dp[0] = r + fz_mul255(dp[0], t);
			dp[1] = g + fz_mul255(dp[1], t);
			dp[2] = b + fz_mul255(dp[2], t);
			dp[3] = a + fz_mul255(dp[3], t);
		}
		dp += 4;
		u += fa;
		v += fb;
	}
}

static inline void
fz_blendscan(unsigned char *dp, unsigned char *sp, int sw, int sh,
	int u, int v, int fa, int fb, int w, int n)
{
	while (w--)
	{
		if (INSIDE)
		{
			unsigned char color[FZ_MAXCOLORS+1];
			int k, t;
			samplecolor(sp, sw, sh, n, u, v, color);
			t = 255 - color[n-1];
			for (k = 0; k < n; k++)
				dp[k] = color[k] + fz_mul255(dp[k], t);
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

/* Blend non-premultiplied color in image mask over destination */

static inline void
fz_blendscanwithcolor(unsigned char *dp, unsigned char *sp, int sw, int sh,
	int u, int v, int fa, int fb, int w, int n, unsigned char *color)
{
	int sa = color[n-1];
	while (w--)
	{
		if (INSIDE)
		{
			int ma = samplemask(sp, sw, sh, u, v);
			int masa = fz_mul255(sa, ma);
			int t = 255 - masa;
			int k;
			for (k = 0; k < n; k++)
				dp[k] = fz_mul255(color[k], ma) + fz_mul255(dp[k], t);
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

/* Draw an image with an affine transform on destination */

static inline float roundup(float x)
{
	return (x < 0) ? floorf(x) : ceilf(x);
}

static void
fz_blendimageimp(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm,
	unsigned char *color)
{
	unsigned char *dp, *sp;
	int u, v, fa, fb, fc, fd;
	int x, y, w, h;
	int sw, sh, n;
	fz_matrix inv;
	fz_bbox bbox;

	/* grid fit the image */
	if (fz_isrectilinear(ctm))
	{
		ctm.a = roundup(ctm.a);
		ctm.b = roundup(ctm.b);
		ctm.c = roundup(ctm.c);
		ctm.d = roundup(ctm.d);
		ctm.e = floorf(ctm.e) + 0.5f;
		ctm.f = floorf(ctm.f) + 0.5f;
	}

	bbox = fz_roundrect(fz_transformrect(ctm, fz_unitrect));
	bbox = fz_intersectbbox(bbox, scissor);
	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	/* map from screen space (x,y) to image space (u,v) */
	inv = fz_scale(1.0f / img->w, -1.0f / img->h);
	inv = fz_concat(inv, fz_translate(0, 1));
	inv = fz_concat(inv, ctm);
	inv = fz_invertmatrix(inv);

	fa = inv.a * 65536;
	fb = inv.b * 65536;
	fc = inv.c * 65536;
	fd = inv.d * 65536;
	u = (fa * x) + (fc * y) + inv.e * 65536;
	v = (fb * x) + (fd * y) + inv.f * 65536;

	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;
	n = dst->n;
	sp = img->samples;
	sw = img->w;
	sh = img->h;

	while (h--)
	{
		if (color)
		{
			fz_blendscanwithcolor(dp, sp, sw, sh, u, v, fa, fb, w, n, color);
		}
		else
		{
			switch (n)
			{
			case 1: fz_blendscan1(dp, sp, sw, sh, u, v, fa, fb, w); break;
			case 2: fz_blendscan2(dp, sp, sw, sh, u, v, fa, fb, w); break;
			case 4: fz_blendscan4(dp, sp, sw, sh, u, v, fa, fb, w); break;
			default: fz_blendscan(dp, sp, sw, sh, u, v, fa, fb, w, n); break;
			}
		}
		dp += dst->w * n;
		u += fc;
		v += fd;
	}
}

void
fz_blendimagewithcolor(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm,
	unsigned char *color)
{
	assert(img->n == 1);
	fz_blendimageimp(dst, scissor, img, ctm, color);
}

void
fz_blendimage(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm)
{
	assert(dst->n == img->n);
	fz_blendimageimp(dst, scissor, img, ctm, nil);
}
