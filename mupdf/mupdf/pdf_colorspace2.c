#include "fitz.h"
#include "mupdf.h"

/*
 * Optimized color conversions for Device colorspaces
 */

static void fastgraytorgb(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[0];
		d[1] = s[1];
		d[2] = s[1];
		d[3] = s[1];
		s += 2;
		d += 4;
	}
}

static void fastgraytocmyk(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[0];
		d[1] = 0;
		d[2] = 0;
		d[3] = 0;
		d[4] = s[1];
		s += 2;
		d += 5;
	}
}

static void fastrgbtogray(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[0];
		d[1] = ((s[1]+1) * 77 + (s[2]+1) * 150 + (s[3]+1) * 28) >> 8;
		s += 4;
		d += 2;
	}
}

static void fastrgbtocmyk(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		unsigned char c = 255 - s[1];
		unsigned char m = 255 - s[2];
		unsigned char y = 255 - s[3];
		unsigned char k = MIN(c, MIN(m, y));
		d[0] = s[0];
		d[1] = c - k;
		d[2] = m - k;
		d[3] = y - k;
		d[4] = k;
		s += 4;
		d += 5;
	}
}

static void fastcmyktogray(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		unsigned char c = fz_mul255(s[1], 77);
		unsigned char m = fz_mul255(s[2], 150);
		unsigned char y = fz_mul255(s[3], 28);
		d[0] = s[0];
		d[1] = 255 - MIN(c + m + y + s[4], 255);
		s += 5;
		d += 2;
	}
}

static void fastcmyktorgb(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[0];
		d[1] = 255 - MIN(s[1] + s[4], 255);
		d[2] = 255 - MIN(s[2] + s[4], 255);
		d[3] = 255 - MIN(s[3] + s[4], 255);
		s += 5;
		d += 4;
	}
}

void pdf_convpixmap(fz_colorspace *ss, fz_pixmap *sp, fz_colorspace *ds, fz_pixmap *dp)
{
	pdf_logimage("convert pixmap from %s to %s\n", ss->name, ds->name);

	if (ss == pdf_devicegray)
	{
		if (ds == pdf_devicergb) fastgraytorgb(sp, dp);
		else if (ds == pdf_devicecmyk) fastgraytocmyk(sp, dp);
		else fz_stdconvpixmap(ss, sp, ds, dp);
	}

	else if (ss == pdf_devicergb)
	{
		if (ds == pdf_devicegray) fastrgbtogray(sp, dp);
		else if (ds == pdf_devicecmyk) fastrgbtocmyk(sp, dp);
		else fz_stdconvpixmap(ss, sp, ds, dp);

	}

	else if (ss == pdf_devicecmyk)
	{
		if (ds == pdf_devicegray) fastcmyktogray(sp, dp);
		else if (ds == pdf_devicergb) fastcmyktorgb(sp, dp);
		else fz_stdconvpixmap(ss, sp, ds, dp);
	}

	else fz_stdconvpixmap(ss, sp, ds, dp);
}

/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=756 */
/* function adapted from Poppler's GfxState_helpers.h
   Poppler is licensed under GPL, see http://poppler.freedesktop.org/ */
static inline void cmykToRGBMatrixMultiplication(float *sv, float *dv)
{
	float c = sv[0], m = sv[1], y = sv[2], k = sv[3];
	float c1 = 1 - c, m1 = 1 - m, y1 = 1 - y, k1 = 1 - k;
	float r, g, b, x;

	// this is a matrix multiplication, unrolled for performance
	//                        C M Y K
	x = c1 * m1 * y1 * k1; // 0 0 0 0
	r = g = b = x;
	x = c1 * m1 * y1 * k;  // 0 0 0 1
	r += 0.1373 * x;
	g += 0.1216 * x;
	b += 0.1255 * x;
	x = c1 * m1 * y  * k1; // 0 0 1 0
	r += x;
	g += 0.9490 * x;
	x = c1 * m1 * y  * k;  // 0 0 1 1
	r += 0.1098 * x;
	g += 0.1020 * x;
	x = c1 * m  * y1 * k1; // 0 1 0 0
	r += 0.9255 * x;
	b += 0.5490 * x;
	x = c1 * m  * y1 * k;  // 0 1 0 1
	r += 0.1412 * x;
	x = c1 * m  * y  * k1; // 0 1 1 0
	r += 0.9294 * x;
	g += 0.1098 * x;
	b += 0.1412 * x;
	x = c1 * m  * y  * k;  // 0 1 1 1
	r += 0.1333 * x;
	x = c  * m1 * y1 * k1; // 1 0 0 0
	g += 0.6784 * x;
	b += 0.9373 * x;
	x = c  * m1 * y1 * k;  // 1 0 0 1
	g += 0.0588 * x;
	b += 0.1412 * x;
	x = c  * m1 * y  * k1; // 1 0 1 0
	g += 0.6510 * x;
	b += 0.3137 * x;
	x = c  * m1 * y  * k;  // 1 0 1 1
	g += 0.0745 * x;
	x = c  * m  * y1 * k1; // 1 1 0 0
	r += 0.1804 * x;
	g += 0.1922 * x;
	b += 0.5725 * x;
	x = c  * m  * y1 * k;  // 1 1 0 1
	b += 0.0078 * x;
	x = c  * m  * y  * k1; // 1 1 1 0
	r += 0.2118 * x;
	g += 0.2119 * x;
	b += 0.2235 * x;

	dv[0] = MIN(MAX(r, 0), 1);
	dv[1] = MIN(MAX(g, 0), 1);
	dv[2] = MIN(MAX(b, 0), 1);
}

void pdf_convcolor(fz_colorspace *ss, float *sv, fz_colorspace *ds, float *dv)
{

	if (ss == pdf_devicegray)
	{
		if (ds == pdf_devicergb)
		{
			dv[0] = sv[0];
			dv[1] = sv[0];
			dv[2] = sv[0];
		}
		else if (ds == pdf_devicecmyk)
		{
			dv[0] = 0;
			dv[1] = 0;
			dv[2] = 0;
			dv[3] = sv[0];
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else if (ss == pdf_devicergb)
	{
		if (ds == pdf_devicegray)
		{
			dv[0] = sv[0] * 0.3 + sv[1] * 0.59 + sv[2] * 0.11;
		}
		else if (ds == pdf_devicecmyk)
		{
			float c = 1.0 - sv[1];
			float m = 1.0 - sv[2];
			float y = 1.0 - sv[3];
			float k = MIN(c, MIN(m, y));
			dv[0] = c - k;
			dv[1] = m - k;
			dv[2] = y - k;
			dv[3] = k;
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else if (ss == pdf_devicecmyk)
	{
		if (ds == pdf_devicegray)
		{
			float c = sv[1] * 0.3;
			float m = sv[2] * 0.59;
			float y = sv[2] * 0.11;
			dv[0] = 1.0 - MIN(c + m + y + sv[3], 1.0);
		}
		else if (ds == pdf_devicergb)
		{
			cmykToRGBMatrixMultiplication(sv, dv); /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=756 */
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else
		fz_stdconvcolor(ss, sv, ds, dv);
}

