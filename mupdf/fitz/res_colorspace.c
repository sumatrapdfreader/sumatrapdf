#include "fitz.h"

#define SLOWCMYK

fz_colorspace *
fz_newcolorspace(char *name, int n)
{
	fz_colorspace *cs = fz_malloc(sizeof(fz_colorspace));
	cs->refs = 1;
	fz_strlcpy(cs->name, name, sizeof cs->name);
	cs->n = n;
	cs->toxyz = nil;
	cs->fromxyz = nil;
	cs->freedata = nil;
	cs->data = nil;
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

/* Device colorspace definitions */

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
#ifdef SLOWCMYK /* from poppler */
	float c = cmyk[0], m = cmyk[1], y = cmyk[2], k = cmyk[3];
	float c1 = 1 - c, m1 = 1 - m, y1 = 1 - y, k1 = 1 - k;
	float r, g, b, x;

	/* this is a matrix multiplication, unrolled for performance */
	x = c1 * m1 * y1 * k1;	/* 0 0 0 0 */
	r = g = b = x;
	x = c1 * m1 * y1 * k;	/* 0 0 0 1 */
	r += 0.1373 * x;
	g += 0.1216 * x;
	b += 0.1255 * x;
	x = c1 * m1 * y * k1;	/* 0 0 1 0 */
	r += x;
	g += 0.9490 * x;
	x = c1 * m1 * y * k;	/* 0 0 1 1 */
	r += 0.1098 * x;
	g += 0.1020 * x;
	x = c1 * m * y1 * k1;	/* 0 1 0 0 */
	r += 0.9255 * x;
	b += 0.5490 * x;
	x = c1 * m * y1 * k;	/* 0 1 0 1 */
	r += 0.1412 * x;
	x = c1 * m * y * k1;	/* 0 1 1 0 */
	r += 0.9294 * x;
	g += 0.1098 * x;
	b += 0.1412 * x;
	x = c1 * m * y * k;	/* 0 1 1 1 */
	r += 0.1333 * x;
	x = c * m1 * y1 * k1;	/* 1 0 0 0 */
	g += 0.6784 * x;
	b += 0.9373 * x;
	x = c * m1 * y1 * k;	/* 1 0 0 1 */
	g += 0.0588 * x;
	b += 0.1412 * x;
	x = c * m1 * y * k1;	/* 1 0 1 0 */
	g += 0.6510 * x;
	b += 0.3137 * x;
	x = c * m1 * y * k;	/* 1 0 1 1 */
	g += 0.0745 * x;
	x = c * m * y1 * k1;	/* 1 1 0 0 */
	r += 0.1804 * x;
	g += 0.1922 * x;
	b += 0.5725 * x;
	x = c * m * y1 * k;	/* 1 1 0 1 */
	b += 0.0078 * x;
	x = c * m * y * k1;	/* 1 1 1 0 */
	r += 0.2118 * x;
	g += 0.2119 * x;
	b += 0.2235 * x;

	xyz[0] = CLAMP(r, 0, 1);
	xyz[1] = CLAMP(g, 0, 1);
	xyz[2] = CLAMP(b, 0, 1);
#else
	xyz[0] = 1 - MIN(1, cmyk[0] + cmyk[3]);
	xyz[1] = 1 - MIN(1, cmyk[1] + cmyk[3]);
	xyz[2] = 1 - MIN(1, cmyk[2] + cmyk[3]);
#endif
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

fz_colorspace *fz_devicegray = &kdevicegray;
fz_colorspace *fz_devicergb = &kdevicergb;
fz_colorspace *fz_devicebgr = &kdevicebgr;
fz_colorspace *fz_devicecmyk = &kdevicecmyk;

/* Fast pixmap color conversions */

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
#ifdef SLOWCMYK
		float cmyk[4], rgb[3];
		cmyk[0] = s[0] / 255.0f;
		cmyk[1] = s[1] / 255.0f;
		cmyk[2] = s[2] / 255.0f;
		cmyk[3] = s[3] / 255.0f;
		cmyktoxyz(nil, cmyk, rgb);
		d[0] = rgb[0] * 255;
		d[1] = rgb[1] * 255;
		d[2] = rgb[2] * 255;
#else
		d[0] = 255 - MIN(s[0] + s[3], 255);
		d[1] = 255 - MIN(s[1] + s[3], 255);
		d[2] = 255 - MIN(s[2] + s[3], 255);
#endif
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
#ifdef SLOWCMYK
		float cmyk[4], rgb[3];
		cmyk[0] = s[0] / 255.0f;
		cmyk[1] = s[1] / 255.0f;
		cmyk[2] = s[2] / 255.0f;
		cmyk[3] = s[3] / 255.0f;
		cmyktoxyz(nil, cmyk, rgb);
		d[0] = rgb[2] * 255;
		d[1] = rgb[1] * 255;
		d[2] = rgb[0] * 255;
#else
		d[0] = 255 - MIN(s[2] + s[3], 255);
		d[1] = 255 - MIN(s[1] + s[3], 255);
		d[2] = 255 - MIN(s[0] + s[3], 255);
#endif
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
	int srcn, dstn;
	int y, x, k, i;

	fz_colorspace *ss = src->colorspace;
	fz_colorspace *ds = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	assert(src->w == dst->w && src->h == dst->h);
	assert(src->n == ss->n + 1);
	assert(dst->n == ds->n + 1);

	srcn = ss->n;
	dstn = ds->n;

	/* Special case for Lab colorspace (scaling of components to float) */
	if (!strcmp(ss->name, "Lab") && srcn == 3)
	{
		for (y = 0; y < src->h; y++)
		{
			for (x = 0; x < src->w; x++)
			{
				srcv[0] = *s++ / 255.0f * 100;
				srcv[1] = *s++ - 128;
				srcv[2] = *s++ - 128;

				fz_convertcolor(ss, srcv, ds, dstv);

				for (k = 0; k < dstn; k++)
					*d++ = dstv[k] * 255;

				*d++ = *s++;
			}
		}
	}

	/* Brute-force for small images */
	else if (src->w * src->h < 256)
	{
		for (y = 0; y < src->h; y++)
		{
			for (x = 0; x < src->w; x++)
			{
				for (k = 0; k < srcn; k++)
					srcv[k] = *s++ / 255.0f;

				fz_convertcolor(ss, srcv, ds, dstv);

				for (k = 0; k < dstn; k++)
					*d++ = dstv[k] * 255;

				*d++ = *s++;
			}
		}
	}

	/* 1-d lookup table for separation and similar colorspaces */
	else if (srcn == 1)
	{
		unsigned char lookup[FZ_MAXCOLORS * 256];

		for (i = 0; i < 256; i++)
		{
			srcv[0] = i / 255.0f;
			fz_convertcolor(ss, srcv, ds, dstv);
			for (k = 0; k < dstn; k++)
				lookup[i * dstn + k] = dstv[k] * 255;
		}

		for (y = 0; y < src->h; y++)
		{
			for (x = 0; x < src->w; x++)
			{
				i = *s++;
				for (k = 0; k < dstn; k++)
					*d++ = lookup[i * dstn + k];
				*d++ = *s++;
			}
		}
	}

	/* Memoize colors using a hash table for the general case */
	else
	{
		fz_hashtable *lookup;
		unsigned char *color;

		lookup = fz_newhash(509, srcn);

		for (y = 0; y < src->h; y++)
		{
			for (x = 0; x < src->w; x++)
			{
				color = fz_hashfind(lookup, s);
				if (color)
				{
					memcpy(d, color, dstn);
					s += srcn;
					d += dstn;
					*d++ = *s++;
				}
				else
				{
					for (k = 0; k < srcn; k++)
						srcv[k] = *s++ / 255.0f;
					fz_convertcolor(ss, srcv, ds, dstv);
					for (k = 0; k < dstn; k++)
						*d++ = dstv[k] * 255;

					fz_hashinsert(lookup, s - srcn, d - dstn);

					*d++ = *s++;
				}
			}
		}

		fz_freehash(lookup);
	}
}

void
fz_convertpixmap(fz_pixmap *sp, fz_pixmap *dp)
{
	fz_colorspace *ss = sp->colorspace;
	fz_colorspace *ds = dp->colorspace;

	assert(ss && ds);

	if (sp->mask)
		dp->mask = fz_keeppixmap(sp->mask);
	dp->interpolate = sp->interpolate;

	if (ss == fz_devicegray)
	{
		if (ds == fz_devicergb) fastgraytorgb(sp, dp);
		else if (ds == fz_devicebgr) fastgraytorgb(sp, dp); /* bgr == rgb here */
		else if (ds == fz_devicecmyk) fastgraytocmyk(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else if (ss == fz_devicergb)
	{
		if (ds == fz_devicegray) fastrgbtogray(sp, dp);
		else if (ds == fz_devicebgr) fastrgbtobgr(sp, dp);
		else if (ds == fz_devicecmyk) fastrgbtocmyk(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else if (ss == fz_devicebgr)
	{
		if (ds == fz_devicegray) fastbgrtogray(sp, dp);
		else if (ds == fz_devicergb) fastrgbtobgr(sp, dp); /* bgr = rgb here */
		else if (ds == fz_devicecmyk) fastbgrtocmyk(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else if (ss == fz_devicecmyk)
	{
		if (ds == fz_devicegray) fastcmyktogray(sp, dp);
		else if (ds == fz_devicebgr) fastcmyktobgr(sp, dp);
		else if (ds == fz_devicergb) fastcmyktorgb(sp, dp);
		else fz_stdconvpixmap(sp, dp);
	}

	else fz_stdconvpixmap(sp, dp);
}

/* Convert a single color */

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
	if (ss == fz_devicegray)
	{
		if ((ds == fz_devicergb) || (ds == fz_devicebgr))
		{
			dv[0] = sv[0];
			dv[1] = sv[0];
			dv[2] = sv[0];
		}
		else if (ds == fz_devicecmyk)
		{
			dv[0] = 0;
			dv[1] = 0;
			dv[2] = 0;
			dv[3] = sv[0];
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else if (ss == fz_devicergb)
	{
		if (ds == fz_devicegray)
		{
			dv[0] = sv[0] * 0.3f + sv[1] * 0.59f + sv[2] * 0.11f;
		}
		else if (ds == fz_devicebgr)
		{
			dv[0] = sv[2];
			dv[1] = sv[1];
			dv[2] = sv[0];
		}
		else if (ds == fz_devicecmyk)
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

	else if (ss == fz_devicebgr)
	{
		if (ds == fz_devicegray)
		{
			dv[0] = sv[0] * 0.11f + sv[1] * 0.59f + sv[2] * 0.3f;
		}
		else if (ds == fz_devicebgr)
		{
			dv[0] = sv[2];
			dv[1] = sv[1];
			dv[2] = sv[0];
		}
		else if (ds == fz_devicecmyk)
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

	else if (ss == fz_devicecmyk)
	{
		if (ds == fz_devicegray)
		{
			float c = sv[0] * 0.3f;
			float m = sv[1] * 0.59f;
			float y = sv[2] * 0.11f;
			dv[0] = 1 - MIN(c + m + y + sv[3], 1);
		}
		else if (ds == fz_devicergb)
		{
#ifdef SLOWCMYK
			cmyktoxyz(nil, sv, dv);
#else
			dv[0] = 1 - MIN(sv[0] + sv[3], 1);
			dv[1] = 1 - MIN(sv[1] + sv[3], 1);
			dv[2] = 1 - MIN(sv[2] + sv[3], 1);
#endif
		}
		else if (ds == fz_devicebgr)
		{
#ifdef SLOWCMYK
			float rgb[3];
			cmyktoxyz(nil, sv, rgb);
			dv[0] = rgb[2];
			dv[1] = rgb[1];
			dv[2] = rgb[0];
#else
			dv[0] = 1 - MIN(sv[2] + sv[3], 1);
			dv[1] = 1 - MIN(sv[1] + sv[3], 1);
			dv[2] = 1 - MIN(sv[0] + sv[3], 1);
#endif
		}
		else
			fz_stdconvcolor(ss, sv, ds, dv);
	}

	else
		fz_stdconvcolor(ss, sv, ds, dv);
}
