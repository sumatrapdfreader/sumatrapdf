#include "fitz.h"

/* SumatraPDF: use between 512 MB and up to 60% of physical RAM for images */
#ifndef _WIN32
static int fz_memory_limit = 256 << 20;
static int fz_memory_used = 0;

static int fz_get_memory_limit() { return fz_memory_limit; }
#else
#include <windows.h>
static DWORDLONG fz_memory_used = 0;

static DWORDLONG fz_get_memory_limit()
{
	static DWORDLONG memory_limit = 0;

	if (memory_limit == 0)
	{
		MEMORYSTATUSEX ms;
		ms.dwLength = sizeof(ms);
		if (!getenv("MUPDF_MEMORY_LIMIT") ||
			!(ms.ullTotalPhys = (DWORDLONG)atoi(getenv("MUPDF_MEMORY_LIMIT")) << 20))
		{
			GlobalMemoryStatusEx(&ms);
			ms.ullTotalPhys = ms.ullTotalPhys / 5 * 3;
		}
		memory_limit = max(ms.ullTotalPhys, 512 << 20);
	}

	return memory_limit;
}
#endif

fz_pixmap *
fz_new_pixmap_with_data(fz_colorspace *colorspace, int w, int h, unsigned char *samples)
{
	fz_pixmap *pix;

	pix = fz_malloc(sizeof(fz_pixmap));
	pix->refs = 1;
	pix->x = 0;
	pix->y = 0;
	pix->w = w;
	pix->h = h;
	pix->mask = NULL;
	pix->interpolate = 1;
	pix->xres = 96;
	pix->yres = 96;
	pix->colorspace = NULL;
	pix->n = 1;
	pix->has_alpha = 1; /* SumatraPDF: allow optimizing non-alpha pixmaps */

	if (colorspace)
	{
		pix->colorspace = fz_keep_colorspace(colorspace);
		pix->n = 1 + colorspace->n;
	}

	if (samples)
	{
		pix->samples = samples;
		pix->free_samples = 0;
	}
	else
	{
		/* SumatraPDF: abort on integer overflow */
		if (pix->w > INT_MAX / pix->n) fz_calloc(-1, -1);
		fz_synchronize_begin();
		fz_memory_used += pix->w * pix->h * pix->n;
		fz_synchronize_end();
		pix->samples = fz_calloc(pix->h, pix->w * pix->n);
		pix->free_samples = 1;
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_with_limit(fz_colorspace *colorspace, int w, int h)
{
	int n = colorspace ? colorspace->n + 1 : 1;
	int size = w * h * n;
	if (fz_memory_used + size > fz_get_memory_limit() || h != 0 && INT_MAX / h / n < w)
	{
		fz_warn("pixmap memory exceeds soft limit %dM + %dM > %dM",
			(int)(fz_memory_used/(1<<20)), (int)(size/(1<<20)), (int)(fz_get_memory_limit()/(1<<20)));
		return NULL;
	}
	return fz_new_pixmap_with_data(colorspace, w, h, NULL);
}

fz_pixmap *
fz_new_pixmap(fz_colorspace *colorspace, int w, int h)
{
	return fz_new_pixmap_with_data(colorspace, w, h, NULL);
}

fz_pixmap *
fz_new_pixmap_with_rect(fz_colorspace *colorspace, fz_bbox r)
{
	fz_pixmap *pixmap;
	pixmap = fz_new_pixmap(colorspace, r.x1 - r.x0, r.y1 - r.y0);
	pixmap->x = r.x0;
	pixmap->y = r.y0;
	return pixmap;
}

fz_pixmap *
fz_new_pixmap_with_rect_and_data(fz_colorspace *colorspace, fz_bbox r, unsigned char *samples)
{
	fz_pixmap *pixmap;
	pixmap = fz_new_pixmap_with_data(colorspace, r.x1 - r.x0, r.y1 - r.y0, samples);
	pixmap->x = r.x0;
	pixmap->y = r.y0;
	return pixmap;
}

fz_pixmap *
fz_keep_pixmap(fz_pixmap *pix)
{
	pix->refs++;
	return pix;
}

void
fz_drop_pixmap(fz_pixmap *pix)
{
	if (pix && --pix->refs == 0)
	{
		fz_synchronize_begin();
		fz_memory_used -= pix->w * pix->h * pix->n;
		fz_synchronize_end();
		if (pix->mask)
			fz_drop_pixmap(pix->mask);
		if (pix->colorspace)
			fz_drop_colorspace(pix->colorspace);
		if (pix->free_samples)
			fz_free(pix->samples);
		fz_free(pix);
	}
}

fz_bbox
fz_bound_pixmap(fz_pixmap *pix)
{
	fz_bbox bbox;
	bbox.x0 = pix->x;
	bbox.y0 = pix->y;
	bbox.x1 = pix->x + pix->w;
	bbox.y1 = pix->y + pix->h;
	return bbox;
}

void
fz_clear_pixmap(fz_pixmap *pix)
{
	memset(pix->samples, 0, pix->w * pix->h * pix->n);
}

void
fz_clear_pixmap_with_color(fz_pixmap *pix, int value)
{
	if (value == 255)
		memset(pix->samples, 255, pix->w * pix->h * pix->n);
	else
	{
		int k, x, y;
		unsigned char *s = pix->samples;
		for (y = 0; y < pix->h; y++)
		{
			for (x = 0; x < pix->w; x++)
			{
				for (k = 0; k < pix->n - 1; k++)
					*s++ = value;
				*s++ = 255;
			}
		}
	}
}

void
fz_copy_pixmap_rect(fz_pixmap *dest, fz_pixmap *src, fz_bbox r)
{
	const unsigned char *srcp;
	unsigned char *destp;
	int y, w, destspan, srcspan;

	r = fz_intersect_bbox(r, fz_bound_pixmap(dest));
	r = fz_intersect_bbox(r, fz_bound_pixmap(src));
	w = r.x1 - r.x0;
	y = r.y1 - r.y0;
	if (w <= 0 || y <= 0)
		return;

	w *= src->n;
	srcspan = src->w * src->n;
	srcp = src->samples + srcspan * (r.y0 - src->y) + src->n * (r.x0 - src->x);
	destspan = dest->w * dest->n;
	destp = dest->samples + destspan * (r.y0 - dest->y) + dest->n * (r.x0 - dest->x);
	do
	{
		memcpy(destp, srcp, w);
		srcp += srcspan;
		destp += destspan;
	}
	while (--y);
}

void
fz_clear_pixmap_rect_with_color(fz_pixmap *dest, int value, fz_bbox r)
{
	unsigned char *destp;
	int x, y, w, k, destspan;

	r = fz_intersect_bbox(r, fz_bound_pixmap(dest));
	w = r.x1 - r.x0;
	y = r.y1 - r.y0;
	if (w <= 0 || y <= 0)
		return;

	destspan = dest->w * dest->n;
	destp = dest->samples + destspan * (r.y0 - dest->y) + dest->n * (r.x0 - dest->x);
	if (value == 255)
		do
		{
			memset(destp, 255, w * dest->n);
			destp += destspan;
		}
		while (--y);
	else
		do
		{
			unsigned char *s = destp;
			for (x = 0; x < w; x++)
			{
				for (k = 0; k < dest->n - 1; k++)
					*s++ = value;
				*s++ = 255;
			}
			destp += destspan;
		}
		while (--y);
}

void
fz_premultiply_pixmap(fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	unsigned char a;
	int k, x, y;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			a = s[pix->n - 1];
			for (k = 0; k < pix->n - 1; k++)
				s[k] = fz_mul255(s[k], a);
			s += pix->n;
		}
	}
}

fz_pixmap *
fz_alpha_from_gray(fz_pixmap *gray, int luminosity)
{
	fz_pixmap *alpha;
	unsigned char *sp, *dp;
	int len;

	assert(gray->n == 2);

	alpha = fz_new_pixmap_with_rect(NULL, fz_bound_pixmap(gray));
	dp = alpha->samples;
	sp = gray->samples;
	if (!luminosity)
		sp ++;

	len = gray->w * gray->h;
	while (len--)
	{
		*dp++ = sp[0];
		sp += 2;
	}

	return alpha;
}

void
fz_invert_pixmap(fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	int k, x, y;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			for (k = 0; k < pix->n - 1; k++)
				s[k] = 255 - s[k];
			s += pix->n;
		}
	}
}

void
fz_gamma_pixmap(fz_pixmap *pix, float gamma)
{
	unsigned char gamma_map[256];
	unsigned char *s = pix->samples;
	int k, x, y;

	for (k = 0; k < 256; k++)
		gamma_map[k] = pow(k / 255.0f, gamma) * 255;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			for (k = 0; k < pix->n - 1; k++)
				s[k] = gamma_map[s[k]];
			s += pix->n;
		}
	}
}

/*
 * Write pixmap to PNM file (without alpha channel)
 */

fz_error
fz_write_pnm(fz_pixmap *pixmap, char *filename)
{
	FILE *fp;
	unsigned char *p;
	int len;

	if (pixmap->n != 1 && pixmap->n != 2 && pixmap->n != 4)
		return fz_throw("pixmap must be grayscale or rgb to write as pnm");

	fp = fopen(filename, "wb");
	if (!fp)
		return fz_throw("cannot open file '%s': %s", filename, strerror(errno));

	if (pixmap->n == 1 || pixmap->n == 2)
		fprintf(fp, "P5\n");
	if (pixmap->n == 4)
		fprintf(fp, "P6\n");
	fprintf(fp, "%d %d\n", pixmap->w, pixmap->h);
	fprintf(fp, "255\n");

	len = pixmap->w * pixmap->h;
	p = pixmap->samples;

	switch (pixmap->n)
	{
	case 1:
		fwrite(p, 1, len, fp);
		break;
	case 2:
		while (len--)
		{
			putc(p[0], fp);
			p += 2;
		}
		break;
	case 4:
		while (len--)
		{
			putc(p[0], fp);
			putc(p[1], fp);
			putc(p[2], fp);
			p += 4;
		}
	}

	fclose(fp);
	return fz_okay;
}

/*
 * Write pixmap to PAM file (with or without alpha channel)
 */

fz_error
fz_write_pam(fz_pixmap *pixmap, char *filename, int savealpha)
{
	unsigned char *sp;
	int y, w, k;
	FILE *fp;

	int sn = pixmap->n;
	int dn = pixmap->n;
	if (!savealpha && dn > 1)
		dn--;

	fp = fopen(filename, "wb");
	if (!fp)
		return fz_throw("cannot open file '%s': %s", filename, strerror(errno));

	fprintf(fp, "P7\n");
	fprintf(fp, "WIDTH %d\n", pixmap->w);
	fprintf(fp, "HEIGHT %d\n", pixmap->h);
	fprintf(fp, "DEPTH %d\n", dn);
	fprintf(fp, "MAXVAL 255\n");
	if (pixmap->colorspace)
		fprintf(fp, "# COLORSPACE %s\n", pixmap->colorspace->name);
	switch (dn)
	{
	case 1: fprintf(fp, "TUPLTYPE GRAYSCALE\n"); break;
	case 2: if (sn == 2) fprintf(fp, "TUPLTYPE GRAYSCALE_ALPHA\n"); break;
	case 3: if (sn == 4) fprintf(fp, "TUPLTYPE RGB\n"); break;
	case 4: if (sn == 4) fprintf(fp, "TUPLTYPE RGB_ALPHA\n"); break;
	}
	fprintf(fp, "ENDHDR\n");

	sp = pixmap->samples;
	for (y = 0; y < pixmap->h; y++)
	{
		w = pixmap->w;
		while (w--)
		{
			for (k = 0; k < dn; k++)
				putc(sp[k], fp);
			sp += sn;
		}
	}

	fclose(fp);

	return fz_okay;
}

/*
 * Write pixmap to PNG file (with or without alpha channel)
 */

#include <zlib.h>

static inline void big32(unsigned char *buf, unsigned int v)
{
	buf[0] = (v >> 24) & 0xff;
	buf[1] = (v >> 16) & 0xff;
	buf[2] = (v >> 8) & 0xff;
	buf[3] = (v) & 0xff;
}

static inline void put32(unsigned int v, FILE *fp)
{
	putc(v >> 24, fp);
	putc(v >> 16, fp);
	putc(v >> 8, fp);
	putc(v, fp);
}

static void putchunk(char *tag, unsigned char *data, int size, FILE *fp)
{
	unsigned int sum;
	put32(size, fp);
	fwrite(tag, 1, 4, fp);
	fwrite(data, 1, size, fp);
	sum = crc32(0, NULL, 0);
	sum = crc32(sum, (unsigned char*)tag, 4);
	sum = crc32(sum, data, size);
	put32(sum, fp);
}

fz_error
fz_write_png(fz_pixmap *pixmap, char *filename, int savealpha)
{
	static const unsigned char pngsig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	FILE *fp;
	unsigned char head[13];
	unsigned char *udata, *cdata, *sp, *dp;
	uLong usize, csize;
	int y, x, k, sn, dn;
	int color;
	int err;

	if (pixmap->n != 1 && pixmap->n != 2 && pixmap->n != 4)
		return fz_throw("pixmap must be grayscale or rgb to write as png");

	sn = pixmap->n;
	dn = pixmap->n;
	if (!savealpha && dn > 1)
		dn--;

	switch (dn)
	{
	default:
	case 1: color = 0; break;
	case 2: color = 4; break;
	case 3: color = 2; break;
	case 4: color = 6; break;
	}

	usize = (pixmap->w * dn + 1) * pixmap->h;
	csize = compressBound(usize);
	udata = fz_malloc(usize);
	cdata = fz_malloc(csize);

	sp = pixmap->samples;
	dp = udata;
	for (y = 0; y < pixmap->h; y++)
	{
		*dp++ = 1; /* sub prediction filter */
		for (x = 0; x < pixmap->w; x++)
		{
			for (k = 0; k < dn; k++)
			{
				if (x == 0)
					dp[k] = sp[k];
				else
					dp[k] = sp[k] - sp[k-sn];
			}
			sp += sn;
			dp += dn;
		}
	}

	err = compress(cdata, &csize, udata, usize);
	if (err != Z_OK)
	{
		fz_free(udata);
		fz_free(cdata);
		return fz_throw("cannot compress image data");
	}

	fp = fopen(filename, "wb");
	if (!fp)
	{
		fz_free(udata);
		fz_free(cdata);
		return fz_throw("cannot open file '%s': %s", filename, strerror(errno));
	}

	big32(head+0, pixmap->w);
	big32(head+4, pixmap->h);
	head[8] = 8; /* depth */
	head[9] = color;
	head[10] = 0; /* compression */
	head[11] = 0; /* filter */
	head[12] = 0; /* interlace */

	fwrite(pngsig, 1, 8, fp);
	putchunk("IHDR", head, 13, fp);
	putchunk("IDAT", cdata, csize, fp);
	putchunk("IEND", head, 0, fp);
	fclose(fp);

	fz_free(udata);
	fz_free(cdata);
	return fz_okay;
}
