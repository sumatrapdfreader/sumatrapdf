#include "mupdf/fitz.h"

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

	if (w < 0 || h < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal dimensions for pixmap %d %d", w, h);

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
				fz_throw(ctx, FZ_ERROR_GENERIC, "overly wide image");
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
fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *colorspace, const fz_irect *r)
{
	fz_pixmap *pixmap;
	pixmap = fz_new_pixmap(ctx, colorspace, r->x1 - r->x0, r->y1 - r->y0);
	pixmap->x = r->x0;
	pixmap->y = r->y0;
	return pixmap;
}

fz_pixmap *
fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *colorspace, const fz_irect *r, unsigned char *samples)
{
	fz_pixmap *pixmap = fz_new_pixmap_with_data(ctx, colorspace, r->x1 - r->x0, r->y1 - r->y0, samples);
	pixmap->x = r->x0;
	pixmap->y = r->y0;
	return pixmap;
}

fz_irect *
fz_pixmap_bbox(fz_context *ctx, fz_pixmap *pix, fz_irect *bbox)
{
	bbox->x0 = pix->x;
	bbox->y0 = pix->y;
	bbox->x1 = pix->x + pix->w;
	bbox->y1 = pix->y + pix->h;
	return bbox;
}

fz_irect *
fz_pixmap_bbox_no_ctx(fz_pixmap *pix, fz_irect *bbox)
{
	bbox->x0 = pix->x;
	bbox->y0 = pix->y;
	bbox->x1 = pix->x + pix->w;
	bbox->y1 = pix->y + pix->h;
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
	/* CMYK needs special handling (and potentially any other subtractive colorspaces) */
	if (pix->colorspace && pix->colorspace->n == 4)
	{
		int x, y;
		unsigned char *s = pix->samples;

		value = 255 - value;
		for (y = 0; y < pix->h; y++)
		{
			for (x = 0; x < pix->w; x++)
			{
				*s++ = 0;
				*s++ = 0;
				*s++ = 0;
				*s++ = value;
				*s++ = 255;
			}
		}
		return;
	}

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
fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, const fz_irect *b)
{
	const unsigned char *srcp;
	unsigned char *destp;
	int x, y, w, destspan, srcspan;
	fz_irect local_b, bb;

	local_b = *b;
	fz_intersect_irect(&local_b, fz_pixmap_bbox(ctx, dest, &bb));
	fz_intersect_irect(&local_b, fz_pixmap_bbox(ctx, src, &bb));
	w = local_b.x1 - local_b.x0;
	y = local_b.y1 - local_b.y0;
	if (w <= 0 || y <= 0)
		return;

	srcspan = src->w * src->n;
	srcp = src->samples + (unsigned int)(srcspan * (local_b.y0 - src->y) + src->n * (local_b.x0 - src->x));
	destspan = dest->w * dest->n;
	destp = dest->samples + (unsigned int)(destspan * (local_b.y0 - dest->y) + dest->n * (local_b.x0 - dest->x));

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
fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *dest, int value, const fz_irect *b)
{
	unsigned char *destp;
	int x, y, w, k, destspan;
	fz_irect bb;
	fz_irect local_b = *b;

	fz_intersect_irect(&local_b, fz_pixmap_bbox(ctx, dest, &bb));
	w = local_b.x1 - local_b.x0;
	y = local_b.y1 - local_b.y0;
	if (w <= 0 || y <= 0)
		return;

	destspan = dest->w * dest->n;
	destp = dest->samples + (unsigned int)(destspan * (local_b.y0 - dest->y) + dest->n * (local_b.x0 - dest->x));

	/* CMYK needs special handling (and potentially any other subtractive colorspaces) */
	if (dest->colorspace && dest->colorspace->n == 4)
	{
		value = 255 - value;
		do
		{
			unsigned char *s = destp;
			for (x = 0; x < w; x++)
			{
				*s++ = 0;
				*s++ = 0;
				*s++ = 0;
				*s++ = value;
				*s++ = 255;
			}
			destp += destspan;
		}
		while (--y);
		return;
	}

	if (value == 255)
	{
		do
		{
			memset(destp, 255, (unsigned int)(w * dest->n));
			destp += destspan;
		}
		while (--y);
	}
	else
	{
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
	fz_irect bbox;

	assert(gray->n == 2);

	alpha = fz_new_pixmap_with_bbox(ctx, NULL, fz_pixmap_bbox(ctx, gray, &bbox));
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
fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int r, int g, int b)
{
	unsigned char *s = pix->samples;
	int x, y;

	if (pix->colorspace == fz_device_bgr(ctx))
	{
		int save = r;
		r = b;
		b = save;
	}
	else if (pix->colorspace == fz_device_gray(ctx))
	{
		g = (r + g + b) / 3;
	}
	else if (pix->colorspace != fz_device_rgb(ctx))
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "can only tint RGB, BGR and Gray pixmaps");
	}

	if (pix->n == 4)
	{
		for (x = 0; x < pix->w; x++)
		{
			for (y = 0; y < pix->h; y++)
			{
				s[0] = fz_mul255(s[0], r);
				s[1] = fz_mul255(s[1], g);
				s[2] = fz_mul255(s[2], b);
				s += 4;
			}
		}
	}
	else if (pix->n == 2)
	{
		for (x = 0; x < pix->w; x++)
		{
			for (y = 0; y < pix->h; y++)
			{
				*s = fz_mul255(*s, g);
				s += 2;
			}
		}
	}
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

void fz_invert_pixmap_rect(fz_pixmap *image, const fz_irect *rect)
{
	unsigned char *p;
	int x, y, n;

	int x0 = fz_clampi(rect->x0 - image->x, 0, image->w - 1);
	int x1 = fz_clampi(rect->x1 - image->x, 0, image->w - 1);
	int y0 = fz_clampi(rect->y0 - image->y, 0, image->h - 1);
	int y1 = fz_clampi(rect->y1 - image->y, 0, image->h - 1);

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
fz_output_pnm_header(fz_output *out, int w, int h, int n)
{
	fz_context *ctx = out->ctx;

	if (n != 1 && n != 2 && n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as pnm");

	if (n == 1 || n == 2)
		fz_printf(out, "P5\n");
	if (n == 4)
		fz_printf(out, "P6\n");
	fz_printf(out, "%d %d\n", w, h);
	fz_printf(out, "255\n");
}

void
fz_output_pnm_band(fz_output *out, int w, int h, int n, int band, int bandheight, unsigned char *p)
{
	int len;
	int start = band * bandheight;
	int end = start + bandheight;

	if (end > h)
		end = h;
	end -= start;

	len = w * end;

	switch (n)
	{
	case 1:
		fz_write(out, p, len);
		break;
	case 2:
		while (len--)
		{
			fz_putc(out, p[0]);
			p += 2;
		}
		break;
	case 4:
		while (len--)
		{
			fz_putc(out, p[0]);
			fz_putc(out, p[1]);
			fz_putc(out, p[2]);
			p += 4;
		}
	}
}

void
fz_write_pnm(fz_context *ctx, fz_pixmap *pixmap, char *filename)
{
	fz_output *out = fz_new_output_to_filename(ctx, filename);
	fz_output_pnm_header(out, pixmap->w, pixmap->h, pixmap->n);
	fz_output_pnm_band(out, pixmap->w, pixmap->h, pixmap->n, 0, pixmap->h, pixmap->samples);
	fz_close_output(out);
}

/*
 * Write pixmap to PAM file (with or without alpha channel)
 */

void
fz_output_pam_header(fz_output *out, int w, int h, int n, int savealpha)
{
	int sn = n;
	int dn = n;
	if (!savealpha && dn > 1)
		dn--;

	fz_printf(out, "P7\n");
	fz_printf(out, "WIDTH %d\n", w);
	fz_printf(out, "HEIGHT %d\n", h);
	fz_printf(out, "DEPTH %d\n", dn);
	fz_printf(out, "MAXVAL 255\n");
	if (dn == 1) fz_printf(out, "TUPLTYPE GRAYSCALE\n");
	else if (dn == 2 && sn == 2) fz_printf(out, "TUPLTYPE GRAYSCALE_ALPHA\n");
	else if (dn == 3 && sn == 4) fz_printf(out, "TUPLTYPE RGB\n");
	else if (dn == 4 && sn == 4) fz_printf(out, "TUPLTYPE RGB_ALPHA\n");
	else if (dn == 4 && sn == 5) fz_printf(out, "TUPLTYPE CMYK\n");
	else if (dn == 5 && sn == 5) fz_printf(out, "TUPLTYPE CMYK_ALPHA\n");
	fz_printf(out, "ENDHDR\n");
}

void
fz_output_pam_band(fz_output *out, int w, int h, int n, int band, int bandheight, unsigned char *sp, int savealpha)
{
	int y, x, k;
	int start = band * bandheight;
	int end = start + bandheight;
	int sn = n;
	int dn = n;
	if (!savealpha && dn > 1)
		dn--;

	if (end > h)
		end = h;
	end -= start;

	for (y = 0; y < end; y++)
	{
		x = w;
		while (x--)
		{
			for (k = 0; k < dn; k++)
				fz_putc(out, sp[k]);
			sp += sn;
		}
	}
}

void
fz_write_pam(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha)
{
	fz_output *out = fz_new_output_to_filename(ctx, filename);
	fz_output_pam_header(out, pixmap->w, pixmap->h, pixmap->n, savealpha);
	fz_output_pam_band(out, pixmap->w, pixmap->h, pixmap->n, 0, pixmap->h, pixmap->samples, savealpha);
	fz_close_output(out);
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

static void putchunk(char *tag, unsigned char *data, int size, fz_output *out)
{
	unsigned int sum;
	fz_write_int32be(out, size);
	fz_write(out, tag, 4);
	fz_write(out, data, size);
	sum = crc32(0, NULL, 0);
	sum = crc32(sum, (unsigned char*)tag, 4);
	sum = crc32(sum, data, size);
	fz_write_int32be(out, sum);
}

void
fz_write_png(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha)
{
	fz_output *out = fz_new_output_to_filename(ctx, filename);
	fz_png_output_context *poc = NULL;

	fz_var(poc);

	fz_try(ctx)
	{
		poc = fz_output_png_header(out, pixmap->w, pixmap->h, pixmap->n, savealpha);
		fz_output_png_band(out, pixmap->w, pixmap->h, pixmap->n, 0, pixmap->h, pixmap->samples, savealpha, poc);
	}
	fz_always(ctx)
	{
		fz_output_png_trailer(out, poc);
		fz_close_output(out);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_output_png(fz_output *out, const fz_pixmap *pixmap, int savealpha)
{
	fz_png_output_context *poc;
	fz_context *ctx;

	if (!out)
		return;

	ctx = out->ctx;
	poc = fz_output_png_header(out, pixmap->w, pixmap->h, pixmap->n, savealpha);

	fz_try(ctx)
	{
		fz_output_png_band(out, pixmap->w, pixmap->h, pixmap->n, 0, pixmap->h, pixmap->samples, savealpha, poc);
	}
	fz_always(ctx)
	{
		fz_output_png_trailer(out, poc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

struct fz_png_output_context_s
{
	unsigned char *udata;
	unsigned char *cdata;
	uLong usize, csize;
	z_stream stream;
};

fz_png_output_context *
fz_output_png_header(fz_output *out, int w, int h, int n, int savealpha)
{
	static const unsigned char pngsig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	unsigned char head[13];
	fz_context *ctx;
	int color;
	fz_png_output_context *poc;

	if (!out)
		return NULL;

	ctx = out->ctx;

	if (n != 1 && n != 2 && n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as png");

	poc = fz_malloc_struct(ctx, fz_png_output_context);

	if (!savealpha && n > 1)
		n--;

	switch (n)
	{
	default:
	case 1: color = 0; break;
	case 2: color = 4; break;
	case 3: color = 2; break;
	case 4: color = 6; break;
	}

	big32(head+0, w);
	big32(head+4, h);
	head[8] = 8; /* depth */
	head[9] = color;
	head[10] = 0; /* compression */
	head[11] = 0; /* filter */
	head[12] = 0; /* interlace */

	fz_write(out, pngsig, 8);
	putchunk("IHDR", head, 13, out);

	return poc;
}

void
fz_output_png_band(fz_output *out, int w, int h, int n, int band, int bandheight, unsigned char *sp, int savealpha, fz_png_output_context *poc)
{
	unsigned char *dp;
	int y, x, k, sn, dn, err, finalband;
	fz_context *ctx;

	if (!out || !sp || !poc)
		return;

	ctx = out->ctx;

	if (n != 1 && n != 2 && n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as png");

	band *= bandheight;
	finalband = (band+bandheight >= h);
	if (finalband)
		bandheight = h - band;

	sn = n;
	dn = n;
	if (!savealpha && dn > 1)
		dn--;

	if (poc->udata == NULL)
	{
		poc->usize = (w * dn + 1) * bandheight;
		/* Sadly the bound returned by compressBound is just for a
		 * single usize chunk; if you compress a sequence of them
		 * the buffering can result in you suddenly getting a block
		 * larger than compressBound outputted in one go, even if you
		 * take all the data out each time. */
		poc->csize = compressBound(poc->usize);
		fz_try(ctx)
		{
			poc->udata = fz_malloc(ctx, poc->usize);
			poc->cdata = fz_malloc(ctx, poc->csize);
		}
		fz_catch(ctx)
		{
			fz_free(ctx, poc->udata);
			poc->udata = NULL;
			poc->cdata = NULL;
			fz_rethrow(ctx);
		}
		err = deflateInit(&poc->stream, Z_DEFAULT_COMPRESSION);
		if (err != Z_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
	}

	dp = poc->udata;
	for (y = 0; y < bandheight; y++)
	{
		*dp++ = 1; /* sub prediction filter */
		for (x = 0; x < w; x++)
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

	poc->stream.next_in = (Bytef*)poc->udata;
	poc->stream.avail_in = (uInt)(dp - poc->udata);
	do
	{
		poc->stream.next_out = poc->cdata;
		poc->stream.avail_out = (uInt)poc->csize;

		if (!finalband)
		{
			err = deflate(&poc->stream, Z_NO_FLUSH);
			if (err != Z_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
		}
		else
		{
			err = deflate(&poc->stream, Z_FINISH);
			if (err != Z_STREAM_END)
				fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
		}

		if (poc->stream.next_out != poc->cdata)
			putchunk("IDAT", poc->cdata, poc->stream.next_out - poc->cdata, out);
	}
	while (poc->stream.avail_out == 0);
}

void
fz_output_png_trailer(fz_output *out, fz_png_output_context *poc)
{
	unsigned char block[1];
	int err;
	fz_context *ctx;

	if (!out || !poc)
		return;

	ctx = out->ctx;

	err = deflateEnd(&poc->stream);
	if (err != Z_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);

	fz_free(ctx, poc->cdata);
	fz_free(ctx, poc->udata);
	fz_free(ctx, poc);

	putchunk("IEND", block, 0, out);
}

/* We use an auxiliary function to do pixmap_as_png, as it can enable us to
 * drop pix early in the case where we have to convert, potentially saving
 * us having to have 2 copies of the pixmap and a buffer open at once. */
static fz_buffer *
png_from_pixmap(fz_context *ctx, fz_pixmap *pix, int drop)
{
	fz_buffer *buf = NULL;
	fz_output *out;
	fz_pixmap *pix2 = NULL;

	fz_var(buf);
	fz_var(out);
	fz_var(pix2);

	if (pix->w == 0 || pix->h == 0)
		return NULL;

	fz_try(ctx)
	{
		if (pix->colorspace && pix->colorspace != fz_device_gray(ctx) && pix->colorspace != fz_device_rgb(ctx))
		{
			pix2 = fz_new_pixmap(ctx, fz_device_rgb(ctx), pix->w, pix->h);
			fz_convert_pixmap(ctx, pix2, pix);
			if (drop)
				fz_drop_pixmap(ctx, pix);
			pix = pix2;
		}
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_output_png(out, pix, 1);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, drop ? pix : pix2);
		fz_close_output(out);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
	return buf;
}

fz_buffer *
fz_new_png_from_image(fz_context *ctx, fz_image *image, int w, int h)
{
	fz_pixmap *pix = fz_image_get_pixmap(ctx, image, image->w, image->h);

	return png_from_pixmap(ctx, pix, 1);
}

fz_buffer *
fz_new_png_from_pixmap(fz_context *ctx, fz_pixmap *pix)
{
	return png_from_pixmap(ctx, pix, 0);
}

/*
 * Write pixmap to TGA file (with or without alpha channel)
 */

static inline void tga_put_pixel(unsigned char *data, int n, int is_bgr, fz_output *out)
{
	if (n >= 3 && !is_bgr)
	{
		fz_putc(out, data[2]);
		fz_putc(out, data[1]);
		fz_putc(out, data[0]);
		if (n == 4)
			fz_putc(out, data[3]);
		return;
	}
	if (n == 2)
	{
		fz_putc(out, data[0]);
		fz_putc(out, data[0]);
	}
	fz_write(out, data, n);
}

void
fz_write_tga(fz_context *ctx, fz_pixmap *pixmap, const char *filename, int savealpha)
{
	fz_output *out;
	unsigned char head[18];
	int n = pixmap->n;
	int d = savealpha || n == 1 ? n : n - 1;
	int is_bgr = pixmap->colorspace == fz_device_bgr(ctx);
	int k;

	if (pixmap->colorspace && pixmap->colorspace != fz_device_gray(ctx) &&
		pixmap->colorspace != fz_device_rgb(ctx) && pixmap->colorspace != fz_device_bgr(ctx))
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as tga");
	}

	out = fz_new_output_to_filename(ctx, filename);

	memset(head, 0, sizeof(head));
	head[2] = n == 4 ? 10 : 11;
	head[12] = pixmap->w & 0xFF; head[13] = (pixmap->w >> 8) & 0xFF;
	head[14] = pixmap->h & 0xFF; head[15] = (pixmap->h >> 8) & 0xFF;
	head[16] = d * 8;
	head[17] = savealpha && n > 1 ? 8 : 0;
	if (savealpha && d == 2)
		head[16] = 32;

	fz_write(out, head, sizeof(head));
	for (k = 1; k <= pixmap->h; k++)
	{
		int i, j;
		unsigned char *line = pixmap->samples + pixmap->w * n * (pixmap->h - k);
		for (i = 0, j = 1; i < pixmap->w; i += j, j = 1)
		{
			for (; i + j < pixmap->w && j < 128 && !memcmp(line + i * n, line + (i + j) * n, d); j++);
			if (j > 1)
			{
				fz_putc(out, j - 1 + 128);
				tga_put_pixel(line + i * n, d, is_bgr, out);
			}
			else
			{
				for (; i + j < pixmap->w && j <= 128 && memcmp(line + (i + j - 1) * n, line + (i + j) * n, d) != 0; j++);
				if (i + j < pixmap->w || j > 128)
					j--;
				fz_putc(out, j - 1);
				for (; j > 0; j--, i++)
					tga_put_pixel(line + i * n, d, is_bgr, out);
			}
		}
	}
	fz_write(out, "\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0", 26);

	fz_close_output(out);
}

unsigned int
fz_pixmap_size(fz_context *ctx, fz_pixmap * pix)
{
	if (pix == NULL)
		return 0;
	return sizeof(*pix) + pix->n * pix->w * pix->h;
}

fz_pixmap *
fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span)
{
	fz_pixmap *pixmap = fz_new_pixmap(ctx, NULL, w, h);
	pixmap->x = x;
	pixmap->y = y;

	for (y = 0; y < h; y++)
		memcpy(pixmap->samples + y * w, sp + y * span, w);

	return pixmap;
}

fz_pixmap *
fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span)
{
	fz_pixmap *pixmap = fz_new_pixmap(ctx, NULL, w, h);
	pixmap->x = x;
	pixmap->y = y;

	for (y = 0; y < h; y++)
	{
		unsigned char *out = pixmap->samples + y * w;
		unsigned char *in = sp + y * span;
		unsigned char bit = 0x80;
		int ww = w;
		while (ww--)
		{
			*out++ = (*in & bit) ? 255 : 0;
			bit >>= 1;
			if (bit == 0)
				bit = 0x80, in++;
		}
	}

	return pixmap;
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

void
fz_pixmap_set_resolution(fz_pixmap *pix, int res)
{
	pix->xres = res;
	pix->yres = res;
}

void
fz_md5_pixmap(fz_pixmap *pix, unsigned char digest[16])
{
	fz_md5 md5;

	fz_md5_init(&md5);
	if (pix)
		fz_md5_update(&md5, pix->samples, pix->w * pix->h * pix->n);
	fz_md5_final(&md5, digest);
}
