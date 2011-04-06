#include "fitz.h"

typedef unsigned char byte;

static inline float roundup(float x)
{
	return (x < 0) ? floorf(x) : ceilf(x);
}

static inline int lerp(int a, int b, int t)
{
	return a + (((b - a) * t) >> 16);
}

static inline int bilerp(int a, int b, int c, int d, int u, int v)
{
	return lerp(lerp(a, b, u), lerp(c, d, u), v);
}

static inline byte *sample_nearest(byte *s, int w, int h, int n, int u, int v)
{
	if (u < 0) u = 0;
	if (v < 0) v = 0;
	if (u >= w) u = w - 1;
	if (v >= h) v = h - 1;
	return s + (v * w + u) * n;
}

/* Blend premultiplied source image in constant alpha over destination */

static inline void
fz_paint_affine_alpha_N_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha)
{
	int k;

	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int uf = u & 0xffff;
			int vf = v & 0xffff;
			byte *a = sample_nearest(sp, sw, sh, n, ui, vi);
			byte *b = sample_nearest(sp, sw, sh, n, ui+1, vi);
			byte *c = sample_nearest(sp, sw, sh, n, ui, vi+1);
			byte *d = sample_nearest(sp, sw, sh, n, ui+1, vi+1);
			int x = bilerp(a[n-1], b[n-1], c[n-1], d[n-1], uf, vf);
			int t = 255 - fz_mul255(x, alpha);
			for (k = 0; k < n; k++)
			{
				x = bilerp(a[k], b[k], c[k], d[k], uf, vf);
				dp[k] = fz_mul255(x, alpha) + fz_mul255(dp[k], t);
			}
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

/* Special case code for gray -> rgb */
static inline void
fz_paint_affine_alpha_g2rgb_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int alpha)
{
	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int uf = u & 0xffff;
			int vf = v & 0xffff;
			byte *a = sample_nearest(sp, sw, sh, 2, ui, vi);
			byte *b = sample_nearest(sp, sw, sh, 2, ui+1, vi);
			byte *c = sample_nearest(sp, sw, sh, 2, ui, vi+1);
			byte *d = sample_nearest(sp, sw, sh, 2, ui+1, vi+1);
			int y = bilerp(a[1], b[1], c[1], d[1], uf, vf);
			int t = 255 - fz_mul255(y, alpha);
			int x = bilerp(a[0], b[0], c[0], d[0], uf, vf);
			x = fz_mul255(x, alpha);
			dp[0] = x + fz_mul255(dp[0], t);
			dp[1] = x + fz_mul255(dp[1], t);
			dp[2] = x + fz_mul255(dp[2], t);
			dp[3] = fz_mul255(y, alpha) + fz_mul255(dp[3], t);
		}
		dp += 4;
		u += fa;
		v += fb;
	}
}

static inline void
fz_paint_affine_alpha_N_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha)
{
	int k;

	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			byte *sample = sp + ((vi * sw + ui) * n);
			int t = 255 - fz_mul255(sample[n-1], alpha);
			for (k = 0; k < n; k++)
				dp[k] = fz_mul255(sample[k], alpha) + fz_mul255(dp[k], t);
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

static inline void
fz_paint_affine_alpha_g2rgb_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int alpha)
{
	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			byte *sample = sp + ((vi * sw + ui) * 2);
			int t = 255 - fz_mul255(sample[1], alpha);
			int x = fz_mul255(sample[0], alpha);
			dp[0] = x + fz_mul255(dp[0], t);
			dp[1] = x + fz_mul255(dp[1], t);
			dp[2] = x + fz_mul255(dp[2], t);
			dp[3] = fz_mul255(sample[1], alpha) + fz_mul255(dp[3], t);
		}
		dp += 4;
		u += fa;
		v += fb;
	}
}

/* Blend premultiplied source image over destination */

static inline void
fz_paint_affine_N_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n)
{
	int k;

	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int uf = u & 0xffff;
			int vf = v & 0xffff;
			byte *a = sample_nearest(sp, sw, sh, n, ui, vi);
			byte *b = sample_nearest(sp, sw, sh, n, ui+1, vi);
			byte *c = sample_nearest(sp, sw, sh, n, ui, vi+1);
			byte *d = sample_nearest(sp, sw, sh, n, ui+1, vi+1);
			int t = 255 - bilerp(a[n-1], b[n-1], c[n-1], d[n-1], uf, vf);
			for (k = 0; k < n; k++)
			{
				int x = bilerp(a[k], b[k], c[k], d[k], uf, vf);
				dp[k] = x + fz_mul255(dp[k], t);
			}
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

static inline void
fz_paint_affine_solid_g2rgb_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w)
{
	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int uf = u & 0xffff;
			int vf = v & 0xffff;
			byte *a = sample_nearest(sp, sw, sh, 2, ui, vi);
			byte *b = sample_nearest(sp, sw, sh, 2, ui+1, vi);
			byte *c = sample_nearest(sp, sw, sh, 2, ui, vi+1);
			byte *d = sample_nearest(sp, sw, sh, 2, ui+1, vi+1);
			int y = bilerp(a[1], b[1], c[1], d[1], uf, vf);
			int t = 255 - y;
			int x = bilerp(a[0], b[0], c[0], d[0], uf, vf);
			dp[0] = x + fz_mul255(dp[0], t);
			dp[1] = x + fz_mul255(dp[1], t);
			dp[2] = x + fz_mul255(dp[2], t);
			dp[3] = y + fz_mul255(dp[3], t);
		}
		dp += 4;
		u += fa;
		v += fb;
	}
}

static inline void
fz_paint_affine_N_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n)
{
	int k;

	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			byte *sample = sp + ((vi * sw + ui) * n);
			int t = 255 - sample[n-1];
			for (k = 0; k < n; k++)
				dp[k] = sample[k] + fz_mul255(dp[k], t);
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

static inline void
fz_paint_affine_solid_g2rgb_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w)
{
	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			byte *sample = sp + ((vi * sw + ui) * 2);
			int t = 255 - sample[1];
			dp[0] = sample[0] + fz_mul255(dp[0], t);
			dp[1] = sample[0] + fz_mul255(dp[1], t);
			dp[2] = sample[0] + fz_mul255(dp[2], t);
			dp[3] = sample[1] + fz_mul255(dp[3], t);
		}
		dp += 4;
		u += fa;
		v += fb;
	}
}

/* Blend non-premultiplied color in source image mask over destination */

static inline void
fz_paint_affine_color_N_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, byte *color)
{
	int sa = color[n-1];
	int k;

	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int uf = u & 0xffff;
			int vf = v & 0xffff;
			byte *a = sample_nearest(sp, sw, sh, 1, ui, vi);
			byte *b = sample_nearest(sp, sw, sh, 1, ui+1, vi);
			byte *c = sample_nearest(sp, sw, sh, 1, ui, vi+1);
			byte *d = sample_nearest(sp, sw, sh, 1, ui+1, vi+1);
			int ma = bilerp(a[0], b[0], c[0], d[0], uf, vf);
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			for (k = 0; k < n - 1; k++)
				dp[k] = FZ_BLEND(color[k], dp[k], masa);
			dp[k] = FZ_BLEND(255, dp[k], masa);
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

static inline void
fz_paint_affine_color_N_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, byte *color)
{
	int sa = color[n-1];
	int k;

	while (w--)
	{
		int ui = u >> 16;
		int vi = v >> 16;
		if (ui >= 0 && ui < sw && vi >= 0 && vi < sh)
		{
			int ma = sp[vi * sw + ui];
			int masa = FZ_COMBINE(FZ_EXPAND(ma), sa);
			for (k = 0; k < n - 1; k++)
				dp[k] = FZ_BLEND(color[k], dp[k], masa);
			dp[k] = FZ_BLEND(255, dp[k], masa);
		}
		dp += n;
		u += fa;
		v += fb;
	}
}

static void
fz_paint_affine_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha, byte *color/*unused*/)
{
	if (alpha == 255)
	{
		switch (n)
		{
		case 1: fz_paint_affine_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 1); break;
		case 2: fz_paint_affine_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 2); break;
		case 4: fz_paint_affine_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 4); break;
		default: fz_paint_affine_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, n); break;
		}
	}
	else if (alpha > 0)
	{
		switch (n)
		{
		case 1: fz_paint_affine_alpha_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 1, alpha); break;
		case 2: fz_paint_affine_alpha_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 2, alpha); break;
		case 4: fz_paint_affine_alpha_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 4, alpha); break;
		default: fz_paint_affine_alpha_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, n, alpha); break;
		}
	}
}

static void
fz_paint_affine_g2rgb_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha, byte *color/*unused*/)
{
	if (alpha == 255)
	{
		fz_paint_affine_solid_g2rgb_lerp(dp, sp, sw, sh, u, v, fa, fb, w);
	}
	else if (alpha > 0)
	{
		fz_paint_affine_alpha_g2rgb_lerp(dp, sp, sw, sh, u, v, fa, fb, w, alpha);
	}
}

static void
fz_paint_affine_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha, byte *color/*unused */)
{
	if (alpha == 255)
	{
		switch (n)
		{
		case 1: fz_paint_affine_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 1); break;
		case 2: fz_paint_affine_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 2); break;
		case 4: fz_paint_affine_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 4); break;
		default: fz_paint_affine_N_near(dp, sp, sw, sh, u, v, fa, fb, w, n); break;
		}
	}
	else if (alpha > 0)
	{
		switch (n)
		{
		case 1: fz_paint_affine_alpha_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 1, alpha); break;
		case 2: fz_paint_affine_alpha_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 2, alpha); break;
		case 4: fz_paint_affine_alpha_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 4, alpha); break;
		default: fz_paint_affine_alpha_N_near(dp, sp, sw, sh, u, v, fa, fb, w, n, alpha); break;
		}
	}
}

static void
fz_paint_affine_g2rgb_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha, byte *color/*unused*/)
{
	if (alpha == 255)
	{
		fz_paint_affine_solid_g2rgb_near(dp, sp, sw, sh, u, v, fa, fb, w);
	}
	else if (alpha > 0)
	{
		fz_paint_affine_alpha_g2rgb_near(dp, sp, sw, sh, u, v, fa, fb, w, alpha);
	}
}

static void
fz_paint_affine_color_lerp(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha/*unused*/, byte *color)
{
	switch (n)
	{
	case 2: fz_paint_affine_color_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 2, color); break;
	case 4: fz_paint_affine_color_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, 4, color); break;
	default: fz_paint_affine_color_N_lerp(dp, sp, sw, sh, u, v, fa, fb, w, n, color); break;
	}
}

static void
fz_paint_affine_color_near(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha/*unused*/, byte *color)
{
	switch (n)
	{
	case 2: fz_paint_affine_color_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 2, color); break;
	case 4: fz_paint_affine_color_N_near(dp, sp, sw, sh, u, v, fa, fb, w, 4, color); break;
	default: fz_paint_affine_color_N_near(dp, sp, sw, sh, u, v, fa, fb, w, n, color); break;
	}
}

/* Draw an image with an affine transform on destination */

static void
fz_paint_image_imp(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm, byte *color, int alpha)
{
	byte *dp, *sp;
	int u, v, fa, fb, fc, fd;
	int x, y, w, h;
	int sw, sh, n;
	fz_matrix inv;
	fz_bbox bbox;
	int dolerp;
	void (*paintfn)(byte *dp, byte *sp, int sw, int sh, int u, int v, int fa, int fb, int w, int n, int alpha, byte *color);

	/* grid fit the image */
	if (fz_is_rectilinear(ctm))
	{
		ctm.a = roundup(ctm.a);
		ctm.b = roundup(ctm.b);
		ctm.c = roundup(ctm.c);
		ctm.d = roundup(ctm.d);
		ctm.e = floorf(ctm.e);
		ctm.f = floorf(ctm.f);
	}

	/* turn on interpolation for upscaled and non-rectilinear transforms */
	dolerp = 0;
	if (!fz_is_rectilinear(ctm))
		dolerp = 1;
	if (sqrtf(ctm.a * ctm.a + ctm.b * ctm.b) > img->w)
		dolerp = 1;
	if (sqrtf(ctm.c * ctm.c + ctm.d * ctm.d) > img->h)
		dolerp = 1;

	/* except when we shouldn't, at large magnifications */
	if (!img->interpolate)
	{
		if (sqrtf(ctm.a * ctm.a + ctm.b * ctm.b) > img->w * 2)
			dolerp = 0;
		if (sqrtf(ctm.c * ctm.c + ctm.d * ctm.d) > img->h * 2)
			dolerp = 0;
	}

	bbox = fz_round_rect(fz_transform_rect(ctm, fz_unit_rect));
	bbox = fz_intersect_bbox(bbox, scissor);
	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	/* map from screen space (x,y) to image space (u,v) */
	inv = fz_scale(1.0f / img->w, -1.0f / img->h);
	inv = fz_concat(inv, fz_translate(0, 1));
	inv = fz_concat(inv, ctm);
	inv = fz_invert_matrix(inv);

	fa = inv.a * 65536;
	fb = inv.b * 65536;
	fc = inv.c * 65536;
	fd = inv.d * 65536;

	/* Calculate initial texture positions. Do a half step to start. */
	u = (fa * x) + (fc * y) + inv.e * 65536 + ((fa+fc)>>1);
	v = (fb * x) + (fd * y) + inv.f * 65536 + ((fb+fd)>>1);

	dp = dst->samples + ((y - dst->y) * dst->w + (x - dst->x)) * dst->n;
	n = dst->n;
	sp = img->samples;
	sw = img->w;
	sh = img->h;

	/* TODO: if (fb == 0 && fa == 1) call fz_paint_span */

	if (dst->n == 4 && img->n == 2)
	{
		assert(color == NULL);
		if (dolerp)
			paintfn = fz_paint_affine_g2rgb_lerp;
		else
			paintfn = fz_paint_affine_g2rgb_near;
	}
	else
	{
		if (dolerp)
		{
			if (color)
				paintfn = fz_paint_affine_color_lerp;
			else
				paintfn = fz_paint_affine_lerp;
		}
		else
		{
			if (color)
				paintfn = fz_paint_affine_color_near;
			else
				paintfn = fz_paint_affine_near;
		}
	}

	while (h--)
	{
		paintfn(dp, sp, sw, sh, u, v, fa, fb, w, n, alpha, color);
		dp += dst->w * n;
		u += fc;
		v += fd;
	}
}

void
fz_paint_image_with_color(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm, byte *color)
{
	assert(img->n == 1);
	fz_paint_image_imp(dst, scissor, img, ctm, color, 255);
}

void
fz_paint_image(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *img, fz_matrix ctm, int alpha)
{
	assert(dst->n == img->n || (dst->n == 4 && img->n == 2));
	fz_paint_image_imp(dst, scissor, img, ctm, NULL, alpha);
}
