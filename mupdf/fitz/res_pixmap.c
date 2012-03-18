#include "fitz-internal.h"

fz_pixmap *
fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	return (fz_pixmap *)fz_keep_storable(ctx, &pix->storable);
}

void
fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	fz_drop_storable(ctx, &pix->storable);
}

void
fz_free_pixmap_imp(fz_context *ctx, fz_storable *pix_)
{
	fz_pixmap *pix = (fz_pixmap *)pix_;

	if (pix->colorspace)
		fz_drop_colorspace(ctx, pix->colorspace);
	if (pix->free_samples)
		fz_free(ctx, pix->samples);
	fz_free(ctx, pix);
}

fz_pixmap *
fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *colorspace, int w, int h, unsigned char *samples)
{
	fz_pixmap *pix;

	pix = fz_malloc_struct(ctx, fz_pixmap);
	FZ_INIT_STORABLE(pix, 1, fz_free_pixmap_imp);
	pix->x = 0;
	pix->y = 0;
	pix->w = w;
	pix->h = h;
	pix->interpolate = 1;
	pix->xres = 96;
	pix->yres = 96;
	pix->colorspace = NULL;
	pix->n = 1;
	pix->has_alpha = 1; /* SumatraPDF: allow optimizing non-alpha pixmaps */
	pix->single_bit = 0; /* SumatraPDF: allow optimizing 1-bit pixmaps */

	if (colorspace)
	{
		pix->colorspace = fz_keep_colorspace(ctx, colorspace);
		pix->n = 1 + colorspace->n;
	}

	pix->samples = samples;
	if (samples)
	{
		pix->free_samples = 0;
	}
	else
	{
		fz_try(ctx)
		{
			if (pix->w + pix->n - 1 > INT_MAX / pix->n)
				fz_throw(ctx, "overly wide image");
			pix->samples = fz_malloc_array(ctx, pix->h, pix->w * pix->n);
		}
		fz_catch(ctx)
		{
			if (colorspace)
				fz_drop_colorspace(ctx, colorspace);
			fz_free(ctx, pix);
			fz_rethrow(ctx);
		}
		pix->free_samples = 1;
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap(fz_context *ctx, fz_colorspace *colorspace, int w, int h)
{
	return fz_new_pixmap_with_data(ctx, colorspace, w, h, NULL);
}

fz_pixmap *
fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *colorspace, fz_bbox r)
{
	fz_pixmap *pixmap;
	pixmap = fz_new_pixmap(ctx, colorspace, r.x1 - r.x0, r.y1 - r.y0);
	pixmap->x = r.x0;
	pixmap->y = r.y0;
	return pixmap;
}

fz_pixmap *
fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *colorspace, fz_bbox r, unsigned char *samples)
{
	fz_pixmap *pixmap;
	pixmap = fz_new_pixmap_with_data(ctx, colorspace, r.x1 - r.x0, r.y1 - r.y0, samples);
	pixmap->x = r.x0;
	pixmap->y = r.y0;
	return pixmap;
}

fz_bbox
fz_pixmap_bbox(fz_context *ctx, fz_pixmap *pix)
{
	fz_bbox bbox;
	bbox.x0 = pix->x;
	bbox.y0 = pix->y;
	bbox.x1 = pix->x + pix->w;
	bbox.y1 = pix->y + pix->h;
	return bbox;
}

fz_bbox
fz_pixmap_bbox_no_ctx(fz_pixmap *pix)
{
	fz_bbox bbox;
	bbox.x0 = pix->x;
	bbox.y0 = pix->y;
	bbox.x1 = pix->x + pix->w;
	bbox.y1 = pix->y + pix->h;
	return bbox;
}

int
fz_pixmap_width(fz_context *ctx, fz_pixmap *pix)
{
	return pix->w;
}

int
fz_pixmap_height(fz_context *ctx, fz_pixmap *pix)
{
	return pix->h;
}

void
fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	memset(pix->samples, 0, pix->w * pix->h * pix->n);
}

void
fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value)
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
fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_bbox r)
{
	const unsigned char *srcp;
	unsigned char *destp;
	int y, w, destspan, srcspan;

	r = fz_intersect_bbox(r, fz_pixmap_bbox(ctx, dest));
	r = fz_intersect_bbox(r, fz_pixmap_bbox(ctx, src));
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
fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *dest, int value, fz_bbox r)
{
	unsigned char *destp;
	int x, y, w, k, destspan;

	r = fz_intersect_bbox(r, fz_pixmap_bbox(ctx, dest));
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
fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix)
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

void
fz_unmultiply_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	unsigned char *s = pix->samples;
	int a, inva;
	int k, x, y;

	for (y = 0; y < pix->h; y++)
	{
		for (x = 0; x < pix->w; x++)
		{
			a = s[pix->n - 1];
			inva = a ? 255 * 256 / a : 0;
			for (k = 0; k < pix->n - 1; k++)
				s[k] = (s[k] * inva) >> 8;
			s += pix->n;
		}
	}
}

fz_pixmap *
fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray, int luminosity)
{
	fz_pixmap *alpha;
	unsigned char *sp, *dp;
	int len;

	assert(gray->n == 2);

	alpha = fz_new_pixmap_with_bbox(ctx, NULL, fz_pixmap_bbox(ctx, gray));
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
fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix)
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

void fz_invert_pixmap_rect(fz_pixmap *image, fz_bbox rect)
{
	unsigned char *p;
	int x, y, n;

	int x0 = CLAMP(rect.x0 - image->x, 0, image->w - 1);
	int x1 = CLAMP(rect.x1 - image->x, 0, image->w - 1);
	int y0 = CLAMP(rect.y0 - image->y, 0, image->h - 1);
	int y1 = CLAMP(rect.y1 - image->y, 0, image->h - 1);

	for (y = y0; y < y1; y++)
	{
		p = image->samples + (y * image->w + x0) * image->n;
		for (x = x0; x < x1; x++)
		{
			for (n = image->n; n > 0; n--, p++)
				*p = 255 - *p;
		}
	}
}

void
fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma)
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

void
fz_write_pnm(fz_context *ctx, fz_pixmap *pixmap, char *filename)
{
	FILE *fp;
	unsigned char *p;
	int len;

	if (pixmap->n != 1 && pixmap->n != 2 && pixmap->n != 4)
		fz_throw(ctx, "pixmap must be grayscale or rgb to write as pnm");

	fp = fopen(filename, "wb");
	if (!fp)
		fz_throw(ctx, "cannot open file '%s': %s", filename, strerror(errno));

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
}

/*
 * Write pixmap to PAM file (with or without alpha channel)
 */

void
fz_write_pam(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha)
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
		fz_throw(ctx, "cannot open file '%s': %s", filename, strerror(errno));

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

void
fz_write_png(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha)
{
	static const unsigned char pngsig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	FILE *fp;
	unsigned char head[13];
	unsigned char *udata = NULL;
	unsigned char *cdata = NULL;
	unsigned char *sp, *dp;
	uLong usize, csize;
	int y, x, k, sn, dn;
	int color;
	int err;

	fz_var(udata);
	fz_var(cdata);

	if (pixmap->n != 1 && pixmap->n != 2 && pixmap->n != 4)
		fz_throw(ctx, "pixmap must be grayscale or rgb to write as png");

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
	fz_try(ctx)
	{
		udata = fz_malloc(ctx, usize);
		cdata = fz_malloc(ctx, csize);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, udata);
		fz_rethrow(ctx);
	}

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
		fz_free(ctx, udata);
		fz_free(ctx, cdata);
		fz_throw(ctx, "cannot compress image data");
	}

	fp = fopen(filename, "wb");
	if (!fp)
	{
		fz_free(ctx, udata);
		fz_free(ctx, cdata);
		fz_throw(ctx, "cannot open file '%s': %s", filename, strerror(errno));
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

	fz_free(ctx, udata);
	fz_free(ctx, cdata);
}

/* SumatraPDF: Write pixmap to TGA file (with or without alpha channel) */
static inline void tga_put_pixel(char *data, int n, FILE *fp)
{
	if (n < 3)
		fwrite(data, 1, n, fp);
	else
	{
		putc(data[2], fp);
		putc(data[1], fp);
		putc(data[0], fp);
		if (n == 4)
			putc(data[3], fp);
	}
}

void
fz_write_tga(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha)
{
	FILE *fp;
	unsigned short head[9];
	int n = pixmap->n;
	int d = savealpha || n == 1 ? n : n - 1;
	int k;

	if (n != 1 && n != 2 && n != 4)
		fz_throw(ctx, "pixmap must be grayscale or rgb to write as tga");

	fp = fopen(filename, "wb");
	if (!fp)
		fz_throw(ctx, "cannot open file '%s': %s", filename, strerror(errno));

	memset(head, 0, sizeof(head));
	head[1] = n == 4 ? 10 : 11;
	head[6] = pixmap->w;
	head[7] = pixmap->h;
	head[8] = d * 8 + (savealpha ? 1 << 8 : 0);
	fwrite(head, sizeof(head), 1, fp);

	for (k = 1; k <= pixmap->h; k++)
	{
		int i, j;
		char *line = pixmap->samples + pixmap->w * n * (pixmap->h - k);
		for (i = 0, j = 1; i < pixmap->w; i += j, j = 1)
		{
			for (; i + j < pixmap->w && j < 128 && !memcmp(line + i * n, line + (i + j) * n, d); j++);
			if (j > 1)
			{
				putc(j - 1 + 128, fp);
				tga_put_pixel(line + i * n, d, fp);
			}
			else
			{
				for (; i + j < pixmap->w && j < 128 && memcmp(line + (i + j - 1) * n, line + (i + j) * n, d) != 0; j++);
				putc(j - 1, fp);
				for (; j > 0; j--, i++)
					tga_put_pixel(line + i * n, d, fp);
			}
		}
	}
	fwrite("\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0", 1, 26, fp);

	fclose(fp);
}

unsigned int
fz_pixmap_size(fz_context *ctx, fz_pixmap * pix)
{
	if (pix == NULL)
		return 0;
	return sizeof(*pix) + pix->n * pix->w * pix->h;
}

fz_pixmap *
fz_image_to_pixmap(fz_context *ctx, fz_image *image, int w, int h)
{
	if (image == NULL)
		return NULL;
	return image->get_pixmap(ctx, image, w, h);
}

fz_image *
fz_keep_image(fz_context *ctx, fz_image *image)
{
	return (fz_image *)fz_keep_storable(ctx, &image->storable);
}

void
fz_drop_image(fz_context *ctx, fz_image *image)
{
	fz_drop_storable(ctx, &image->storable);
}
