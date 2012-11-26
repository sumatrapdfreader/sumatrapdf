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
	memset(pix->samples, 0, (unsigned int)(pix->w * pix->h * pix->n));
}

void
fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value)
{
	if (value == 255)
	{
		memset(pix->samples, 255, (unsigned int)(pix->w * pix->h * pix->n));
	}
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
	int x, y, w, destspan, srcspan;

	r = fz_intersect_bbox(r, fz_pixmap_bbox(ctx, dest));
	r = fz_intersect_bbox(r, fz_pixmap_bbox(ctx, src));
	w = r.x1 - r.x0;
	y = r.y1 - r.y0;
	if (w <= 0 || y <= 0)
		return;

	srcspan = src->w * src->n;
	srcp = src->samples + (unsigned int)(srcspan * (r.y0 - src->y) + src->n * (r.x0 - src->x));
	destspan = dest->w * dest->n;
	destp = dest->samples + (unsigned int)(destspan * (r.y0 - dest->y) + dest->n * (r.x0 - dest->x));

	if (src->n == dest->n)
	{
		w *= src->n;
		do
		{
			memcpy(destp, srcp, w);
			srcp += srcspan;
			destp += destspan;
		}
		while (--y);
	}
	else if (src->n == 2 && dest->n == 4)
	{
		/* Copy, and convert from grey+alpha to rgb+alpha */
		srcspan -= w*2;
		destspan -= w*4;
		do
		{
			for (x = w; x > 0; x--)
			{
				unsigned char v = *srcp++;
				unsigned char a = *srcp++;
				*destp++ = v;
				*destp++ = v;
				*destp++ = v;
				*destp++ = a;
			}
			srcp += srcspan;
			destp += destspan;
		}
		while (--y);
	}
	else if (src->n == 4 && dest->n == 2)
	{
		/* Copy, and convert from rgb+alpha to grey+alpha */
		srcspan -= w*4;
		destspan -= w*2;
		do
		{
			for (x = w; x > 0; x--)
			{
				int v;
				v = *srcp++;
				v += *srcp++;
				v += *srcp++;
				*destp++ = (unsigned char)((v+1)/3);
				*destp++ = *srcp++;
			}
			srcp += srcspan;
			destp += destspan;
		}
		while (--y);
	}
	else
	{
		/* FIXME: Crap conversion */
		int z;
		int sn = src->n-1;
		int dn = dest->n-1;

		srcspan -= w*src->n;
		destspan -= w*dest->n;
		do
		{
			for (x = w; x > 0; x--)
			{
				int v = 0;
				for (z = sn; z > 0; z--)
					v += *srcp++;
				v = (v * dn + (sn>>1)) / sn;
				for (z = dn; z > 0; z--)
					*destp++ = (unsigned char)v;
				*destp++ = *srcp++;
			}
			srcp += srcspan;
			destp += destspan;
		}
		while (--y);
	}
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
	destp = dest->samples + (unsigned int)(destspan * (r.y0 - dest->y) + dest->n * (r.x0 - dest->x));
	if (value == 255)
		do
		{
			memset(destp, 255, (unsigned int)(w * dest->n));
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

	int x0 = fz_clampi(rect.x0 - image->x, 0, image->w - 1);
	int x1 = fz_clampi(rect.x1 - image->x, 0, image->w - 1);
	int y0 = fz_clampi(rect.y0 - image->y, 0, image->h - 1);
	int y1 = fz_clampi(rect.y1 - image->y, 0, image->h - 1);

	for (y = y0; y < y1; y++)
	{
		p = image->samples + (unsigned int)((y * image->w + x0) * image->n);
		for (x = x0; x < x1; x++)
		{
			for (n = image->n; n > 1; n--, p++)
				*p = 255 - *p;
			p++;
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
	if (n >= 3)
	{
		char buf[4];
		buf[0] = data[2];
		buf[1] = data[1];
		buf[2] = data[0];
		if (n == 4)
			buf[3] = data[3];
		data = buf;
	}
	else if (n == 2)
	{
		char buf[2];
		buf[0] = buf[1] = data[0];
		fwrite(buf, 1, 2, fp);
	}
	fwrite(data, 1, n, fp);
}

static inline int memeq(char *pix1, char *pix2, int n)
{
	int k;
	for (k = 0; k < n; k++)
	{
		if (pix1[k] != pix2[k])
			return 0;
	}
	return 1;
}

void
fz_write_tga(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha)
{
	FILE *fp;
	unsigned char head[18];
	int n = pixmap->n;
	int d = savealpha || n == 1 ? n : n - 1;
	int k;

	if (n != 1 && n != 2 && n != 4)
		fz_throw(ctx, "pixmap must be grayscale or rgb to write as tga");

	fp = fopen(filename, "wb");
	if (!fp)
		fz_throw(ctx, "cannot open file '%s': %s", filename, strerror(errno));

	memset(head, 0, sizeof(head));
	head[2] = n == 4 ? 10 : 11;
	head[12] = pixmap->w & 0xFF; head[13] = (pixmap->w >> 8) & 0xFF;
	head[14] = pixmap->h & 0xFF; head[15] = (pixmap->h >> 8) & 0xFF;
	head[16] = d * 8;
	head[17] = savealpha && n > 1 ? 8 : 0;
	if (savealpha && d == 2)
		head[16] = 32;

	fwrite(head, sizeof(head), 1, fp);
	for (k = 1; k <= pixmap->h; k++)
	{
		int i, j;
		char *line = pixmap->samples + pixmap->w * n * (pixmap->h - k);
		for (i = 0, j = 1; i < pixmap->w; i += j, j = 1)
		{
			for (; i + j < pixmap->w && j < 128 && memeq(line + i * n, line + (i + j) * n, d); j++);
			if (j > 1)
			{
				putc(j - 1 + 128, fp);
				tga_put_pixel(line + i * n, d, fp);
			}
			else
			{
				for (; i + j < pixmap->w && j < 128 && !memeq(line + (i + j - 1) * n, line + (i + j) * n, d) != 0; j++);
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

#ifdef ARCH_ARM
static void
fz_subsample_pixmap_ARM(unsigned char *ptr, int w, int h, int f, int factor,
			int n, int fwd, int back, int back2, int fwd2,
			int divX, int back4, int fwd4, int fwd3,
			int divY, int back5, int divXY)
__attribute__((naked));

static void
fz_subsample_pixmap_ARM(unsigned char *ptr, int w, int h, int f, int factor,
			int n, int fwd, int back, int back2, int fwd2,
			int divX, int back4, int fwd4, int fwd3,
			int divY, int back5, int divXY)
{
	asm volatile(
	ENTER_ARM
	"stmfd	r13!,{r1,r4-r11,r14}					\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"@ r0 = src = ptr						\n"
	"@ r1 = w							\n"
	"@ r2 = h							\n"
	"@ r3 = f							\n"
	"mov	r9, r0			@ r9 = dst = ptr		\n"
	"ldr	r6, [r13,#4*12]		@ r6 = fwd			\n"
	"ldr	r7, [r13,#4*13]		@ r7 = back			\n"
	"subs	r2, r2, r3		@ r2 = h -= f			\n"
	"blt	11f			@ Skip if less than a full row	\n"
	"1:				@ for (y = h; y > 0; y--) {	\n"
	"ldr	r1, [r13]		@ r1 = w			\n"
	"subs	r1, r1, r3		@ r1 = w -= f			\n"
	"blt	6f			@ Skip if less than a full col	\n"
	"ldr	r4, [r13,#4*10]		@ r4 = factor			\n"
	"ldr	r8, [r13,#4*14]		@ r8 = back2			\n"
	"ldr	r12,[r13,#4*15]		@ r12= fwd2			\n"
	"2:				@ for (x = w; x > 0; x--) {	\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"3:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r3, LSL #8	@ for (xx = f; xx > 0; x--) {	\n"
	"4:				@				\n"
	"add	r5, r5, r3, LSL #16	@ for (yy = f; yy > 0; y--) {	\n"
	"5:				@				\n"
	"ldrb	r11,[r0], r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	5b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	4b			@ }				\n"
	"mov	r14,r14,LSR r4		@ r14 = v >>= factor		\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back2			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	3b			@ }				\n"
	"add	r0, r0, r12		@ s += fwd2			\n"
	"subs	r1, r1, r3		@ x -= f			\n"
	"bge	2b			@ }				\n"
	"6:				@ Less than a full column left	\n"
	"adds	r1, r1, r3		@ x += f			\n"
	"beq	11f			@ if (x == 0) next row		\n"
	"@ r0 = src							\n"
	"@ r1 = x							\n"
	"@ r2 = y							\n"
	"@ r3 = f							\n"
	"@ r4 = factor							\n"
	"@ r6 = fwd							\n"
	"@ r7 = back							\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"ldr	r4, [r13,#4*16]		@ r4 = divX			\n"
	"ldr	r8, [r13,#4*17]		@ r8 = back4			\n"
	"ldr	r12,[r13,#4*18]		@ r12= fwd4			\n"
	"8:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r1, LSL #8	@ for (xx = x; xx > 0; x--) {	\n"
	"9:				@				\n"
	"add	r5, r5, r3, LSL #16	@ for (yy = f; yy > 0; y--) {	\n"
	"10:				@				\n"
	"ldrb	r11,[r0], r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	10b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	9b			@ }				\n"
	"mul	r14,r4, r14		@ r14= v *= divX		\n"
	"mov	r14,r14,LSR #16		@ r14= v >>= 16			\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back4			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	8b			@ }				\n"
	"add	r0, r0, r12		@ s += fwd4			\n"
	"11:				@				\n"
	"ldr	r14,[r13,#4*19]		@ r14 = fwd3			\n"
	"subs	r2, r2, r3		@ h -= f			\n"
	"add	r0, r0, r14		@ s += fwd3			\n"
	"bge	1b			@ }				\n"
	"adds	r2, r2, r3		@ h += f			\n"
	"beq	21f			@ if no stray row, end		\n"
	"@ So doing one last (partial) row				\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"@ r0 = src = ptr						\n"
	"@ r1 = w							\n"
	"@ r2 = h							\n"
	"@ r3 = f							\n"
	"@ r4 = factor							\n"
	"@ r5 = n							\n"
	"@ r6 = fwd							\n"
	"12:				@ for (y = h; y > 0; y--) {	\n"
	"ldr	r1, [r13]		@ r1 = w			\n"
	"ldr	r7, [r13,#4*21]		@ r7 = back5			\n"
	"ldr	r8, [r13,#4*14]		@ r8 = back2			\n"
	"subs	r1, r1, r3		@ r1 = w -= f			\n"
	"blt	17f			@ Skip if less than a full col	\n"
	"ldr	r4, [r13,#4*20]		@ r4 = divY			\n"
	"ldr	r12,[r13,#4*15]		@ r12= fwd2			\n"
	"13:				@ for (x = w; x > 0; x--) {	\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"14:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r3, LSL #8	@ for (xx = f; xx > 0; x--) {	\n"
	"15:				@				\n"
	"add	r5, r5, r2, LSL #16	@ for (yy = y; yy > 0; y--) {	\n"
	"16:				@				\n"
	"ldrb	r11,[r0], r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	16b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back5			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	15b			@ }				\n"
	"mul	r14,r4, r14		@ r14 = x *= divY		\n"
	"mov	r14,r14,LSR #16		@ r14 = v >>= 16		\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back2			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	14b			@ }				\n"
	"add	r0, r0, r12		@ s += fwd2			\n"
	"subs	r1, r1, r3		@ x -= f			\n"
	"bge	13b			@ }				\n"
	"17:				@ Less than a full column left	\n"
	"adds	r1, r1, r3		@ x += f			\n"
	"beq	21f			@ if (x == 0) end		\n"
	"@ r0 = src							\n"
	"@ r1 = x							\n"
	"@ r2 = y							\n"
	"@ r3 = f							\n"
	"@ r4 = factor							\n"
	"@ r6 = fwd							\n"
	"@ r7 = back5							\n"
	"@ r8 = back2							\n"
	"@STACK:r1,<9>,factor,n,fwd,back,back2,fwd2,divX,back4,fwd4,fwd3,divY,back5,divXY\n"
	"ldr	r4, [r13,#4*22]		@ r4 = divXY			\n"
	"ldr	r5, [r13,#4*11]		@ for (nn = n; nn > 0; n--) {	\n"
	"18:				@				\n"
	"mov	r14,#0			@ r14= v = 0			\n"
	"sub	r5, r5, r1, LSL #8	@ for (xx = x; xx > 0; x--) {	\n"
	"19:				@				\n"
	"add	r5, r5, r2, LSL #16	@ for (yy = y; yy > 0; y--) {	\n"
	"20:				@				\n"
	"ldrb	r11,[r0],r6		@ r11= *src	src += fwd	\n"
	"subs	r5, r5, #1<<16		@ xx--				\n"
	"add	r14,r14,r11		@ v += r11			\n"
	"bgt	20b			@ }				\n"
	"sub	r0, r0, r7		@ src -= back5			\n"
	"adds	r5, r5, #1<<8		@ yy--				\n"
	"blt	19b			@ }				\n"
	"mul	r14,r4, r14		@ r14= v *= divX		\n"
	"mov	r14,r14,LSR #16		@ r14= v >>= 16			\n"
	"strb	r14,[r9], #1		@ *d++ = r14			\n"
	"sub	r0, r0, r8		@ s -= back2			\n"
	"subs	r5, r5, #1		@ n--				\n"
	"bgt	18b			@ }				\n"
	"21:				@				\n"
	"ldmfd	r13!,{r1,r4-r11,PC}	@ pop, return to thumb		\n"
	ENTER_THUMB
	);
}

#endif

void
fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor)
{
	int dst_w, dst_h, w, h, fwd, fwd2, fwd3, back, back2, x, y, n, xx, yy, nn, f;
	unsigned char *s, *d;

	if (!tile)
		return;
	s = d = tile->samples;
	f = 1<<factor;
	w = tile->w;
	h = tile->h;
	n = tile->n;
	dst_w = (w + f-1)>>factor;
	dst_h = (h + f-1)>>factor;
	fwd = w*n;
	back = f*fwd-n;
	back2 = f*n-1;
	fwd2 = (f-1)*n;
	fwd3 = (f-1)*fwd;
	factor *= 2;
#ifdef ARCH_ARM
	{
		int strayX = w%f;
		int divX = (strayX ? 65536/(strayX*f) : 0);
		int fwd4 = (strayX-1) * n;
		int back4 = strayX*n-1;
		int strayY = h%f;
		int divY = (strayY ? 65536/(strayY*f) : 0);
		int back5 = fwd * strayY - n;
		int divXY = (strayY*strayX ? 65536/(strayX*strayY) : 0);
		fz_subsample_pixmap_ARM(s, w, h, f, factor, n, fwd, back,
					back2, fwd2, divX, back4, fwd4, fwd3,
					divY, back5, divXY);
	}
#else
	for (y = h - f; y >= 0; y -= f)
	{
		for (x = w - f; x >= 0; x -= f)
		{
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = f; xx > 0; xx--)
				{
					for (yy = f; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back;
				}
				*d++ = v >> factor;
				s -= back2;
			}
			s += fwd2;
		}
		/* Do any strays */
		x += f;
		if (x > 0)
		{
			int div = x * f;
			int fwd4 = (x-1) * n;
			int back4 = x*n-1;
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = x; xx > 0; xx--)
				{
					for (yy = f; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back;
				}
				*d++ = v / div;
				s -= back4;
			}
			s += fwd4;
		}
		s += fwd3;
	}
	/* Do any stray line */
	y += f;
	if (y > 0)
	{
		int div = y * f;
		int back5 = fwd * y - n;
		for (x = w - f; x >= 0; x -= f)
		{
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = f; xx > 0; xx--)
				{
					for (yy = y; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back5;
				}
				*d++ = v / div;
				s -= back2;
			}
			s += fwd2;
		}
		/* Do any stray at the end of the stray line */
		x += f;
		if (x > 0)
		{
			div = x * y;
			for (nn = n; nn > 0; nn--)
			{
				int v = 0;
				for (xx = x; xx > 0; xx--)
				{
					for (yy = y; yy > 0; yy--)
					{
						v += *s;
						s += fwd;
					}
					s -= back5;
				}
				*d++ = v / div;
				s -= back2;
			}
		}
	}
#endif
	tile->w = dst_w;
	tile->h = dst_h;
	tile->samples = fz_resize_array(ctx, tile->samples, dst_w * n, dst_h);
}
