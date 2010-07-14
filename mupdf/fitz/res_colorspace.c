#include "fitz.h"

fz_colorspace *
fz_newcolorspace(char *name, int n)
{
	fz_colorspace *cs = fz_malloc(sizeof(fz_colorspace));
	cs->refs = 1;
	fz_strlcpy(cs->name, name, sizeof cs->name);
	cs->n = n;
	cs->toxyz = NULL;
	cs->fromxyz = NULL;
	cs->freedata = NULL;
	cs->data = NULL;
	return cs;
}

fz_colorspace *
fz_keepcolorspace(fz_colorspace *cs)
{
	if (cs->refs < 0)
		return cs;
	cs->refs ++;
	return cs;
}

void
fz_dropcolorspace(fz_colorspace *cs)
{
	if (cs && cs->refs < 0)
		return;
	if (cs && --cs->refs == 0)
	{
		if (cs->freedata && cs->data)
			cs->freedata(cs);
		fz_free(cs);
	}
}

/*
 * Fast Device color spaces
 */

static void graytoxyz(fz_colorspace *cs, float *gray, float *xyz)
{
	xyz[0] = gray[0];
	xyz[1] = gray[0];
	xyz[2] = gray[0];
}

static void xyztogray(fz_colorspace *cs, float *xyz, float *gray)
{
	float r = xyz[0];
	float g = xyz[1];
	float b = xyz[2];
	gray[0] = r * 0.3f + g * 0.59f + b * 0.11f;
}

static void rgbtoxyz(fz_colorspace *cs, float *rgb, float *xyz)
{
	xyz[0] = rgb[0];
	xyz[1] = rgb[1];
	xyz[2] = rgb[2];
}

static void xyztorgb(fz_colorspace *cs, float *xyz, float *rgb)
{
	rgb[0] = xyz[0];
	rgb[1] = xyz[1];
	rgb[2] = xyz[2];
}

static void bgrtoxyz(fz_colorspace *cs, float *bgr, float *xyz)
{
	xyz[0] = bgr[2];
	xyz[1] = bgr[1];
	xyz[2] = bgr[0];
}

static void xyztobgr(fz_colorspace *cs, float *xyz, float *bgr)
{
	bgr[0] = xyz[2];
	bgr[1] = xyz[1];
	bgr[2] = xyz[0];
}

static void cmyktoxyz(fz_colorspace *cs, float *cmyk, float *xyz)
{
	xyz[0] = 1 - MIN(1, cmyk[0] + cmyk[3]);
	xyz[1] = 1 - MIN(1, cmyk[1] + cmyk[3]);
	xyz[2] = 1 - MIN(1, cmyk[2] + cmyk[3]);
}

static void xyztocmyk(fz_colorspace *cs, float *xyz, float *cmyk)
{
	float c, m, y, k;
	c = 1 - xyz[0];
	m = 1 - xyz[1];
	y = 1 - xyz[2];
	k = MIN(c, MIN(m, y));
	cmyk[0] = c - k;
	cmyk[1] = m - k;
	cmyk[2] = y - k;
	cmyk[3] = k;
}

static fz_colorspace kdevicegray = { -1, "DeviceGray", 1, graytoxyz, xyztogray };
static fz_colorspace kdevicergb = { -1, "DeviceRGB", 3, rgbtoxyz, xyztorgb };
static fz_colorspace kdevicebgr = { -1, "DeviceRGB", 3, bgrtoxyz, xyztobgr };
static fz_colorspace kdevicecmyk = { -1, "DeviceCMYK", 4, cmyktoxyz, xyztocmyk };

fz_colorspace *pdf_devicegray = &kdevicegray;
fz_colorspace *pdf_devicergb = &kdevicergb;
fz_colorspace *pdf_devicebgr = &kdevicebgr;
fz_colorspace *pdf_devicecmyk = &kdevicecmyk;

/*
 * Pixmap color conversions
 */

static void fastgraytorgb(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[0];
		d[1] = s[0];
		d[2] = s[0];
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
		d[0] = 0;
		d[1] = 0;
		d[2] = 0;
		d[3] = s[0];
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
		d[0] = ((s[0]+1) * 77 + (s[1]+1) * 150 + (s[2]+1) * 28) >> 8;
		d[1] = s[3];
		s += 4;
		d += 2;
	}
}

static void fastbgrtogray(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = ((s[0]+1) * 28 + (s[1]+1) * 150 + (s[2]+1) * 77) >> 8;
		d[1] = s[3];
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
		unsigned char c = 255 - s[0];
		unsigned char m = 255 - s[1];
		unsigned char y = 255 - s[2];
		unsigned char k = MIN(c, MIN(m, y));
		d[0] = c - k;
		d[1] = m - k;
		d[2] = y - k;
		d[3] = k;
		d[4] = s[3];
		s += 4;
		d += 5;
	}
}

static void fastbgrtocmyk(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		unsigned char c = 255 - s[2];
		unsigned char m = 255 - s[1];
		unsigned char y = 255 - s[0];
		unsigned char k = MIN(c, MIN(m, y));
		d[0] = c - k;
		d[1] = m - k;
		d[2] = y - k;
		d[3] = k;
		d[4] = s[3];
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
		unsigned char c = fz_mul255(s[0], 77);
		unsigned char m = fz_mul255(s[1], 150);
		unsigned char y = fz_mul255(s[2], 28);
		d[0] = 255 - MIN(c + m + y + s[3], 255);
		d[1] = s[4];
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
		d[0] = 255 - MIN(s[0] + s[3], 255);
		d[1] = 255 - MIN(s[1] + s[3], 255);
		d[2] = 255 - MIN(s[2] + s[3], 255);
		d[3] = s[4];
		s += 5;
		d += 4;
	}
}

static void fastcmyktobgr(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = 255 - MIN(s[2] + s[3], 255);
		d[1] = 255 - MIN(s[1] + s[3], 255);
		d[2] = 255 - MIN(s[0] + s[3], 255);
		d[3] = s[4];
		s += 5;
		d += 4;
	}
}

static void fastrgbtobgr(fz_pixmap *src, fz_pixmap *dst)
{
	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;
	int n = src->w * src->h;
	while (n--)
	{
		d[0] = s[2];
		d[1] = s[1];
		d[2] = s[0];
		d[3] = s[3];
		s += 4;
		d += 4;
	}
}

static void
fz_stdconvpixmap(fz_pixmap *src, fz_pixmap *dst)
{
	float srcv[FZ_MAXCOLORS];
	float dstv[FZ_MAXCOLORS];
	int y, x, k;

	fz_colorspace *ss = src->colorspace;
	fz_colorspace *ds = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	assert(src->w == dst->w && src->h == dst->h);
	assert(src->n == ss->n + 1);
	assert(dst->n == ds->n + 1);

	for (y = 0; y < src->h; y++)
	{
		for (x = 0; x < src->w; x++)
		{
			for (k = 0; k < src->n - 1; k++)
				srcv[k] = *s++ / 255.0f;

			fz_convertcolor(ss, srcv, ds, dstv);

			for (k = 0; k < dst->n - 1; k++)
				*d++ = dstv[k] * 255;

			*d++ = *s++;
		}
	}
}

void
fz_convertpixmap(fz_pixmap *sp, fz_pixmap *dp)
{
	fz_colorspace *ss = sp->colorspace;
	fz_colorspace *ds = dp->colorspace;

	assert(ss && ds);

	if (ss == pdf_devicegray)
	{
		if (ds == pdf_devicergb) fastgraytorgb(sp, dp);
		else if (ds == pdf_devicebgr) fastgraytorgb(sp, dp); /* bgr == rgb here */
		else if (ds == pdf_devicecmyk) fastgraytocmyk(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else if (ss == pdf_devicergb)
	{
		if (ds == pdf_devicegray) fastrgbtogray(sp, dp);
		else if (ds == pdf_devicebgr) fastrgbtobgr(sp, dp);
		else if (ds == pdf_devicecmyk) fastrgbtocmyk(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else if (ss == pdf_devicebgr)
	{
		if (ds == pdf_devicegray) fastbgrtogray(sp, dp);
		else if (ds == pdf_devicergb) fastrgbtobgr(sp, dp); /* bgr = rgb here */
		else if (ds == pdf_devicecmyk) fastbgrtocmyk(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else if (ss == pdf_devicecmyk)
	{
		if (ds == pdf_devicegray) fastcmyktogray(sp, dp);
		else if (ds == pdf_devicebgr) fastcmyktobgr(sp, dp);
		else if (ds == pdf_devicergb) fastcmyktorgb(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else fz_stdconvpixmap(sp, dp);
}

/*
 * Convert a single color
 */

static void
fz_stdconvcolor(fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv)
{
	float xyz[3];
	int i;

	if (srcs != dsts)
	{
		assert(srcs->toxyz && dsts->fromxyz);
		srcs->toxyz(srcs, srcv, xyz);
		dsts->fromxyz(dsts, xyz, dstv);
		for (i = 0; i < dsts->n; i++)
			dstv[i] = CLAMP(dstv[i], 0, 1);
	}
	else
	{
		for (i = 0; i < srcs->n; i++)
			dstv[i] = srcv[i];
	}
}

void
fz_convertcolor(fz_colorspace *ss, float *sv, fz_colorspace *ds, float *dv)
{
	if (ss == pdf_devicegray)
	{
		if ((ds == pdf_devicergb) || (ds == pdf_devicebgr))
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
			dv[0] = sv[0] * 0.3f + sv[1] * 0.59f + sv[2] * 0.11f;
		}
		else if (ds == pdf_devicebgr)
		{
			dv[0] = sv[2];
			dv[1] = sv[1];
			dv[2] = sv[0];
		}
		else if (ds == pdf_devicecmyk)
		{
			float c = 1 - sv[0];
			float m = 1 - sv[1];
			float y = 1 - sv[2];
			float k = MIN(c, MIN(m, y));
			dv[0] = c - k;
			dv[1] = m - k;
			dv[2] = y - k;
			dv[3] = k;
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else if (ss == pdf_devicebgr)
	{
		if (ds == pdf_devicegray)
		{
			dv[0] = sv[0] * 0.11f + sv[1] * 0.59f + sv[2] * 0.3f;
		}
		else if (ds == pdf_devicebgr)
		{
			dv[0] = sv[2];
			dv[1] = sv[1];
			dv[2] = sv[0];
		}
		else if (ds == pdf_devicecmyk)
		{
			float c = 1 - sv[2];
			float m = 1 - sv[1];
			float y = 1 - sv[0];
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
			float c = sv[1] * 0.3f;
			float m = sv[2] * 0.59f;
			float y = sv[2] * 0.11f;
			dv[0] = 1 - MIN(c + m + y + sv[3], 1);
		}
		else if (ds == pdf_devicergb)
		{
			dv[0] = 1 - MIN(sv[0] + sv[3], 1);
			dv[1] = 1 - MIN(sv[1] + sv[3], 1);
			dv[2] = 1 - MIN(sv[2] + sv[3], 1);
		}
		else if (ds == pdf_devicebgr)
		{
			dv[0] = 1 - MIN(sv[2] + sv[3], 1);
			dv[1] = 1 - MIN(sv[1] + sv[3], 1);
			dv[2] = 1 - MIN(sv[0] + sv[3], 1);
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else
		fz_stdconvcolor(ss, sv, ds, dv);
}

