#include "mupdf/fitz.h"

#include <math.h>

/* Fast color transforms */

static void gray_to_gray(fz_context *ctx, fz_color_converter *cc, const float *gray, float *xyz)
{
	xyz[0] = gray[0];
}

static void gray_to_rgb(fz_context *ctx, fz_color_converter *cc, const float *gray, float *rgb)
{
	rgb[0] = gray[0];
	rgb[1] = gray[0];
	rgb[2] = gray[0];
}

static void rgb_to_gray(fz_context *ctx, fz_color_converter *cc, const float *rgb, float *gray)
{
	gray[0] = rgb[0] * 0.3f + rgb[1] * 0.59f + rgb[2] * 0.11f;
}

static void bgr_to_gray(fz_context *ctx, fz_color_converter *cc, const float *bgr, float *gray)
{
	gray[0] = bgr[0] * 0.11f + bgr[1] * 0.59f + bgr[2] * 0.3f;
}

static void rgb_to_rgb(fz_context *ctx, fz_color_converter *cc, const float *rgb, float *xyz)
{
	xyz[0] = rgb[0];
	xyz[1] = rgb[1];
	xyz[2] = rgb[2];
}

static void rgb_to_bgr(fz_context *ctx, fz_color_converter *cc, const float *rgb, float *bgr)
{
	bgr[0] = rgb[2];
	bgr[1] = rgb[1];
	bgr[2] = rgb[0];
}

static void cmyk_to_cmyk(fz_context *ctx, fz_color_converter *cc, const float *cmyk, float *xyz)
{
	xyz[0] = cmyk[0];
	xyz[1] = cmyk[1];
	xyz[2] = cmyk[2];
	xyz[3] = cmyk[3];
}

static void gray_to_cmyk(fz_context *ctx, fz_color_converter *cc, const float *gray, float *cmyk)
{
	cmyk[0] = 0;
	cmyk[1] = 0;
	cmyk[2] = 0;
	cmyk[3] = 1 - gray[0];
}

static void cmyk_to_gray(fz_context *ctx, fz_color_converter *cc, const float *cmyk, float *gray)
{
	float c = cmyk[0] * 0.3f;
	float m = cmyk[1] * 0.59f;
	float y = cmyk[2] * 0.11f;
	gray[0] = 1 - fz_min(c + m + y + cmyk[3], 1);
}

static void rgb_to_cmyk(fz_context *ctx, fz_color_converter *cc, const float *rgb, float *cmyk)
{
	float c, m, y, k;
	c = 1 - rgb[0];
	m = 1 - rgb[1];
	y = 1 - rgb[2];
	k = fz_min(c, fz_min(m, y));
	cmyk[0] = c - k;
	cmyk[1] = m - k;
	cmyk[2] = y - k;
	cmyk[3] = k;
}

static void bgr_to_cmyk(fz_context *ctx, fz_color_converter *cc, const float *bgr, float *cmyk)
{
	float c, m, y, k;
	c = 1 - bgr[2];
	m = 1 - bgr[1];
	y = 1 - bgr[0];
	k = fz_min(c, fz_min(m, y));
	cmyk[0] = c - k;
	cmyk[1] = m - k;
	cmyk[2] = y - k;
	cmyk[3] = k;
}

static void cmyk_to_rgb(fz_context *ctx, fz_color_converter *cc, const float *cmyk, float *rgb)
{
	rgb[0] = 1 - fz_min(1, cmyk[0] + cmyk[3]);
	rgb[1] = 1 - fz_min(1, cmyk[1] + cmyk[3]);
	rgb[2] = 1 - fz_min(1, cmyk[2] + cmyk[3]);
}

static void cmyk_to_bgr(fz_context *ctx, fz_color_converter *cc, const float *cmyk, float *bgr)
{
	bgr[0] = 1 - fz_min(cmyk[2] + cmyk[3], 1);
	bgr[1] = 1 - fz_min(cmyk[1] + cmyk[3], 1);
	bgr[2] = 1 - fz_min(cmyk[0] + cmyk[3], 1);
}

static inline float fung(float x)
{
	if (x >= 6.0f / 29.0f)
		return x * x * x;
	return (108.0f / 841.0f) * (x - (4.0f / 29.0f));
}

static void lab_to_rgb(fz_context *ctx, fz_color_converter *cc, const float *lab, float *rgb)
{
	/* input is in range (0..100, -128..127, -128..127) not (0..1, 0..1, 0..1) */
	float lstar, astar, bstar, l, m, n, x, y, z, r, g, b;
	lstar = lab[0];
	astar = lab[1];
	bstar = lab[2];
	m = (lstar + 16) / 116;
	l = m + astar / 500;
	n = m - bstar / 200;
	x = fung(l);
	y = fung(m);
	z = fung(n);
	r = (3.240449f * x + -1.537136f * y + -0.498531f * z) * 0.830026f;
	g = (-0.969265f * x + 1.876011f * y + 0.041556f * z) * 1.05452f;
	b = (0.055643f * x + -0.204026f * y + 1.057229f * z) * 1.1003f;
	rgb[0] = sqrtf(fz_clamp(r, 0, 1));
	rgb[1] = sqrtf(fz_clamp(g, 0, 1));
	rgb[2] = sqrtf(fz_clamp(b, 0, 1));
}

static void lab_to_gray(fz_context *ctx, fz_color_converter *cc, const float *lab, float *gray)
{
	gray[0] = lab[0] / 100;
}

static void lab_to_bgr(fz_context *ctx, fz_color_converter *cc, const float *lab, float *bgr)
{
	float rgb[3];
	lab_to_rgb(ctx, cc, lab, rgb);
	rgb_to_bgr(ctx, cc, rgb, bgr);
}

static void lab_to_cmyk(fz_context *ctx, fz_color_converter *cc, const float *lab, float *cmyk)
{
	float rgb[3];
	lab_to_rgb(ctx, cc, lab, rgb);
	rgb_to_cmyk(ctx, cc, rgb, cmyk);
}

fz_color_convert_fn *
fz_lookup_fast_color_converter(fz_context *ctx, fz_colorspace *ss, fz_colorspace *ds)
{
	int stype = ss->type;
	int dtype = ds->type;

	if (stype == FZ_COLORSPACE_GRAY)
	{
		if (dtype == FZ_COLORSPACE_GRAY) return gray_to_gray;
		if (dtype == FZ_COLORSPACE_RGB) return gray_to_rgb;
		if (dtype == FZ_COLORSPACE_BGR) return gray_to_rgb;
		if (dtype == FZ_COLORSPACE_CMYK) return gray_to_cmyk;
	}

	else if (stype == FZ_COLORSPACE_RGB)
	{
		if (dtype == FZ_COLORSPACE_GRAY) return rgb_to_gray;
		if (dtype == FZ_COLORSPACE_RGB) return rgb_to_rgb;
		if (dtype == FZ_COLORSPACE_BGR) return rgb_to_bgr;
		if (dtype == FZ_COLORSPACE_CMYK) return rgb_to_cmyk;
	}

	else if (stype == FZ_COLORSPACE_BGR)
	{
		if (dtype == FZ_COLORSPACE_GRAY) return bgr_to_gray;
		if (dtype == FZ_COLORSPACE_RGB) return rgb_to_bgr;
		if (dtype == FZ_COLORSPACE_BGR) return rgb_to_rgb;
		if (dtype == FZ_COLORSPACE_CMYK) return bgr_to_cmyk;
	}

	else if (stype == FZ_COLORSPACE_CMYK)
	{
		if (dtype == FZ_COLORSPACE_GRAY) return cmyk_to_gray;
		if (dtype == FZ_COLORSPACE_RGB) return cmyk_to_rgb;
		if (dtype == FZ_COLORSPACE_BGR) return cmyk_to_bgr;
		if (dtype == FZ_COLORSPACE_CMYK) return cmyk_to_cmyk;
	}

	else if (stype == FZ_COLORSPACE_LAB)
	{
		if (dtype == FZ_COLORSPACE_GRAY) return lab_to_gray;
		if (dtype == FZ_COLORSPACE_RGB) return lab_to_rgb;
		if (dtype == FZ_COLORSPACE_BGR) return lab_to_bgr;
		if (dtype == FZ_COLORSPACE_CMYK) return lab_to_cmyk;
	}

	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find color converter");
}

/* Fast pixmap color conversions */

static void fast_gray_to_rgb(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[0];
						d[2] = s[0];
						d[3] = s[1];
						s += 2;
						d += 4;
					}
					d += d_line_inc;
					s += s_line_inc;
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[0];
						d[2] = s[0];
						d[3] = 255;
						s++;
						d += 4;
					}
					d += d_line_inc;
					s += s_line_inc;
				}
			}
		}
		else
		{
			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = s[0];
					d[1] = s[0];
					d[2] = s[0];
					s++;
					d += 3;
				}
				d += d_line_inc;
				s += s_line_inc;
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		int i;
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				d[1] = s[0];
				d[2] = s[0];
				s += 1;
				d += 3;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				d[1] = s[0];
				d[2] = s[0];
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

static void fast_gray_to_cmyk(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;
	int k, g;
	int a = 255;
	int i;

	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");

	if ((int)w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "integer overflow");

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			g = s[0];

			if (sa)
			{
				a = s[1+ss];
				g = fz_div255(g, a);
			}

			k = 255 - g;

			if (da)
			{
				*d++ = 0;
				*d++ = 0;
				*d++ = 0;
				*d++ = fz_mul255(k, a);
			}
			else
			{
				*d++ = 0;
				*d++ = 0;
				*d++ = 0;
				*d++ = k;
			}

			if (copy_spots)
			{
				s += 1;
				for (i=ss; i > 0; --i)
					*d++ = *s++;
				s += sa;
			}
			else
			{
				s += 1 + ss + sa;
				d += ds;
			}

			if (da)
			{
				*d++ = a;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void fast_rgb_to_gray(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
						d[1] = s[3];
						s += 4;
						d += 2;
					}
					d += d_line_inc;
					s += s_line_inc;
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
						d[1] = 255;
						s += 3;
						d += 2;
					}
					d += d_line_inc;
					s += s_line_inc;
				}
			}
		}
		else
		{
			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
					s += 3;
					d++;
				}
				d += d_line_inc;
				s += s_line_inc;
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		int i;
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
				s += 3;
				d++;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

static void fast_bgr_to_gray(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
						d[1] = s[3];
						s += 4;
						d += 2;
					}
					d += d_line_inc;
					s += s_line_inc;
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
						d[1] = 255;
						s += 3;
						d += 2;
					}
					d += d_line_inc;
					s += s_line_inc;
				}
			}
		}
		else
		{
			int si = 3 + src->alpha;

			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
					s += si;
					d++;
				}
				d += d_line_inc;
				s += s_line_inc;
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		while (h--)
		{
			int i;
			size_t ww = w;
			while (ww--)
			{
				d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
				s += 3;
				d++;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		/* Slower, spots capable version */
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

static void fast_rgb_to_cmyk(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;
	int c, m, y, k, r, g, b;
	int a = 255;
	int i;

	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");

	if ((int)w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "integer overflow");

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			r = s[0];
			g = s[1];
			b = s[2];

			if (sa)
			{
				a = s[3+ss];
				r = fz_div255(r, a);
				g = fz_div255(g, a);
				b = fz_div255(b, a);
			}

			c = 255 - r;
			m = 255 - g;
			y = 255 - b;
			k = fz_mini(c, fz_mini(m, y));
			c = c - k;
			m = m - k;
			y = y - k;

			if (da)
			{
				*d++ = fz_mul255(c, a);
				*d++ = fz_mul255(m, a);
				*d++ = fz_mul255(y, a);
				*d++ = fz_mul255(k, a);
			}
			else
			{
				*d++ = c;
				*d++ = m;
				*d++ = y;
				*d++ = k;
			}

			if (copy_spots)
			{
				s += 3;
				for (i=ss; i > 0; --i)
					*d++ = *s++;
				s += sa;
			}
			else
			{
				s += 3 + ss + sa;
				d += ds;
			}

			if (da)
			{
				*d++ = a;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void fast_bgr_to_cmyk(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;
	int c, m, y, k, r, g, b;
	int a = 255;
	int i;

	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");

	if ((int)w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "integer overflow");

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			b = s[0];
			g = s[1];
			r = s[2];

			if (sa)
			{
				a = s[3+ss];
				r = fz_div255(r, a);
				g = fz_div255(g, a);
				b = fz_div255(b, a);
			}

			c = 255 - r;
			m = 255 - g;
			y = 255 - b;
			k = fz_mini(c, fz_mini(m, y));
			c = c - k;
			m = m - k;
			y = y - k;

			if (da)
			{
				*d++ = fz_mul255(c, a);
				*d++ = fz_mul255(m, a);
				*d++ = fz_mul255(y, a);
				*d++ = fz_mul255(k, a);
			}
			else
			{
				*d++ = c;
				*d++ = m;
				*d++ = y;
				*d++ = k;
			}

			if (copy_spots)
			{
				s += 3;
				for (i=ss; i > 0; --i)
					*d++ = *s++;
				s += sa;
			}
			else
			{
				s += 3 + ss + sa;
				d += ds;
			}

			if (da)
			{
				*d++ = a;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void fast_cmyk_to_gray(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;
	int c, m, y, k, g;
	int a = 255;
	int i;

	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");

	if ((int)w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "integer overflow");

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			c = s[0];
			m = s[1];
			y = s[2];
			k = s[3];

			if (sa)
			{
				a = s[4+ss];
				c = fz_div255(c, a);
				m = fz_div255(m, a);
				y = fz_div255(y, a);
				k = fz_div255(k, a);
			}

			g = 255 - fz_mini(c + m + y + k, 255);

			if (da)
			{
				*d++ = fz_mul255(g, a);
			}
			else
			{
				*d++ = g;
			}

			if (copy_spots)
			{
				s += 4;
				for (i=ss; i > 0; --i)
					*d++ = *s++;
				s += sa;
			}
			else
			{
				s += 4 + ss + sa;
				d += ds;
			}

			if (da)
			{
				*d++ = a;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void fast_cmyk_to_rgb(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;
	int c, m, y, k, r, g, b;
	int a = 255;
	int i;

	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");

	if ((int)w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "integer overflow");

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			c = s[0];
			m = s[1];
			y = s[2];
			k = s[3];

			if (sa)
			{
				a = s[4+ss];
				c = fz_div255(c, a);
				m = fz_div255(m, a);
				y = fz_div255(y, a);
				k = fz_div255(k, a);
			}

			r = 255 - fz_mini(c + k, 255);
			g = 255 - fz_mini(m + k, 255);
			b = 255 - fz_mini(y + k, 255);

			if (da)
			{
				*d++ = fz_mul255(r, a);
				*d++ = fz_mul255(g, a);
				*d++ = fz_mul255(b, a);
			}
			else
			{
				*d++ = r;
				*d++ = g;
				*d++ = b;
			}

			if (copy_spots)
			{
				s += 4;
				for (i=ss; i > 0; --i)
					*d++ = *s++;
				s += sa;
			}
			else
			{
				s += 4 + ss + sa;
				d += ds;
			}

			if (da)
			{
				*d++ = a;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void fast_cmyk_to_bgr(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;
	int c, m, y, k, r, g, b;
	int a = 255;
	int i;

	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");

	if ((int)w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "integer overflow");

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			c = s[0];
			m = s[1];
			y = s[2];
			k = s[3];

			if (sa)
			{
				a = s[4+ss];
				c = fz_div255(c, a);
				m = fz_div255(m, a);
				y = fz_div255(y, a);
				k = fz_div255(k, a);
			}

			r = 255 - fz_mini(c + k, 255);
			g = 255 - fz_mini(m + k, 255);
			b = 255 - fz_mini(y + k, 255);

			if (da)
			{
				*d++ = fz_mul255(b, a);
				*d++ = fz_mul255(g, a);
				*d++ = fz_mul255(r, a);
			}
			else
			{
				*d++ = b;
				*d++ = g;
				*d++ = r;
			}

			if (copy_spots)
			{
				s += 4;
				for (i=ss; i > 0; --i)
					*d++ = *s++;
				s += sa;
			}
			else
			{
				s += 4 + ss + sa;
				d += ds;
			}

			if (da)
			{
				*d++ = a;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void fast_rgb_to_bgr(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[2];
						d[1] = s[1];
						d[2] = s[0];
						d[3] = s[3];
						s += 4;
						d += 4;
					}
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[2];
						d[1] = s[1];
						d[2] = s[0];
						d[3] = 255;
						s += 3;
						d += 4;
					}
				}
			}
		}
		else
		{
			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = s[2];
					d[1] = s[1];
					d[2] = s[0];
					s += 3;
					d += 3;
				}
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		while (h--)
		{
			int i;
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[2];
				d[1] = s[1];
				d[2] = s[0];
				s += 3;
				d += 3;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[2];
				d[1] = s[1];
				d[2] = s[0];
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

static void fast_gray_to_gray(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[1];
						s += 2;
						d += 2;
					}
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = 255;
						s += 1;
						d += 2;
					}
				}
			}
		}
		else
		{
			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = s[0];
					s += 1;
					d += 1;
				}
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		while (h--)
		{
			int i;
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				s += 1;
				d += 1;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

static void fast_rgb_to_rgb(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = s[3];
						s += 4;
						d += 4;
					}
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = 255;
						s += 3;
						d += 4;
					}
				}
			}
		}
		else
		{
			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = s[0];
					d[1] = s[1];
					d[2] = s[2];
					s += 3;
					d += 3;
				}
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		while (h--)
		{
			int i;
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				s += 3;
				d += 3;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

static void fast_cmyk_to_cmyk(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	size_t w = src->w;
	int h = src->h;
	int sn = src->n;
	int ss = src->s;
	int sa = src->alpha;
	int dn = dst->n;
	int ds = dst->s;
	int da = dst->alpha;
	ptrdiff_t d_line_inc = dst->stride - w * dn;
	ptrdiff_t s_line_inc = src->stride - w * sn;

	/* If copying spots, they must match, and we can never drop alpha (but we can invent it) */
	if (copy_spots && ss != ds)
		fz_throw(ctx, FZ_ERROR_GENERIC, "incompatible number of spots when converting pixmap");
	if (!da && sa)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot drop alpha when converting pixmap");

	if ((int)w < 0 || h < 0)
		return;

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (ss == 0 && ds == 0)
	{
		/* Common, no spots case */
		if (da)
		{
			if (sa)
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = s[3];
						d[4] = s[4];
						s += 5;
						d += 5;
					}
				}
			}
			else
			{
				while (h--)
				{
					size_t ww = w;
					while (ww--)
					{
						d[0] = s[0];
						d[1] = s[1];
						d[2] = s[2];
						d[3] = s[3];
						d[4] = 255;
						s += 4;
						d += 5;
					}
				}
			}
		}
		else
		{
			while (h--)
			{
				size_t ww = w;
				while (ww--)
				{
					d[0] = s[0];
					d[1] = s[1];
					d[2] = s[2];
					d[3] = s[3];
					s += 4;
					d += 4;
				}
			}
		}
	}
	else if (copy_spots)
	{
		/* Slower, spots capable version */
		while (h--)
		{
			int i;
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				d[3] = s[3];
				s += 4;
				d += 4;
				for (i=ss; i > 0; i--)
					*d++ = *s++;
				if (da)
					*d++ = sa ? *s++ : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	else
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				d[0] = s[0];
				d[1] = s[1];
				d[2] = s[2];
				d[3] = s[3];
				s += sn;
				d += dn;
				if (da)
					d[-1] = sa ? s[-1] : 255;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

void
fz_fast_any_to_alpha(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	if (!src->alpha)
		fz_clear_pixmap_with_value(ctx, dst, 255);
	else
	{
		unsigned char *s = src->samples;
		unsigned char *d = dst->samples;
		size_t w = src->w;
		int h = src->h;
		int n = src->n;
		ptrdiff_t d_line_inc = dst->stride - w * dst->n;
		ptrdiff_t s_line_inc = src->stride - w * src->n;

		if ((int)w < 0 || h < 0)
			return;

		if (d_line_inc == 0 && s_line_inc == 0)
		{
			w *= h;
			h = 1;
		}

		s += n-1;
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				*d++ = *s;
				s += n;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
}

void
fz_convert_fast_pixmap_samples(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	fz_colorspace *ss = src->colorspace;
	fz_colorspace *ds = dst->colorspace;
	int dtype = ds ? ds->type : FZ_COLORSPACE_GRAY;
	int stype = ss ? ss->type : FZ_COLORSPACE_GRAY;

	if (!ds)
	{
		fz_fast_any_to_alpha(ctx, src, dst, copy_spots);
	}

	else if (stype == FZ_COLORSPACE_GRAY)
	{
		if (dtype == FZ_COLORSPACE_GRAY)
			fast_gray_to_gray(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_RGB)
			fast_gray_to_rgb(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_BGR)
			fast_gray_to_rgb(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_CMYK)
			fast_gray_to_cmyk(ctx, src, dst, copy_spots);
		else
			goto slow;
	}

	else if (stype == FZ_COLORSPACE_RGB)
	{
		if (dtype == FZ_COLORSPACE_GRAY)
			fast_rgb_to_gray(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_RGB)
			fast_rgb_to_rgb(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_BGR)
			fast_rgb_to_bgr(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_CMYK)
			fast_rgb_to_cmyk(ctx, src, dst, copy_spots);
		else
			goto slow;
	}

	else if (stype == FZ_COLORSPACE_BGR)
	{
		if (dtype == FZ_COLORSPACE_GRAY)
			fast_bgr_to_gray(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_RGB)
			fast_rgb_to_bgr(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_BGR)
			fast_rgb_to_rgb(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_CMYK)
			fast_bgr_to_cmyk(ctx, src, dst, copy_spots);
		else
			goto slow;
	}

	else if (stype == FZ_COLORSPACE_CMYK)
	{
		if (dtype == FZ_COLORSPACE_GRAY)
			fast_cmyk_to_gray(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_RGB)
			fast_cmyk_to_rgb(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_BGR)
			fast_cmyk_to_bgr(ctx, src, dst, copy_spots);
		else if (dtype == FZ_COLORSPACE_CMYK)
			fast_cmyk_to_cmyk(ctx, src, dst, copy_spots);
		else
			goto slow;
	}
	else
	{
		goto slow;
	}
	return;

slow:
	fz_convert_slow_pixmap_samples(ctx, src, dst, NULL, fz_default_color_params, copy_spots);
}
