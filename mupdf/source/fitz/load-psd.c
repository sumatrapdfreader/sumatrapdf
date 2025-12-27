// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include "pixmap-imp.h"

#include <limits.h>
#include <string.h>

struct info
{
	unsigned int width, height, n;
	int xres, yres;
	fz_colorspace *cs;
};

typedef struct
{
	fz_context *ctx;
	const unsigned char *p;
	size_t total;
	int packbits;
	int packbits_n;
	int packbits_rep;
} source_t;

static int
get8(source_t *source)
{
	if (source->total < 1)
		fz_throw(source->ctx, FZ_ERROR_FORMAT, "Truncated PSD");
	source->total--;

	return *source->p++;
}

static int
get16be(source_t *source)
{
	int v;

	if (source->total < 2)
	{
		source->total = 0;
		fz_throw(source->ctx, FZ_ERROR_FORMAT, "Truncated PSD");
	}

	source->total -= 2;

	v = *source->p++;
	v = (v<<8) | *source->p++;

	return v;
}

static int
get32be(source_t *source)
{
	int v;

	if (source->total < 4)
	{
		source->total = 0;
		fz_throw(source->ctx, FZ_ERROR_FORMAT, "Truncated PSD");
	}

	source->total -= 4;

	v = *source->p++;
	v = (v<<8) | *source->p++;
	v = (v<<8) | *source->p++;
	v = (v<<8) | *source->p++;

	return v;
}

static uint32_t
getu32be(source_t *source)
{
	return (uint32_t)get32be(source);
}

static int
unpack8(source_t *source)
{
	int i;

	if (source->packbits == 0)
		return get8(source);

	i = source->packbits_n;
	if (i == 128)
	{
		do
		{
			i = source->packbits_n = get8(source);
		}
		while (i == 128);
		if (i > 128)
			source->packbits_rep = get8(source);
	}
	if (i < 128)
	{
		/* Literal n+1 */
		i--;
		if (i < 0)
			i = 128;
		source->packbits_n = i;
		return get8(source);
	}
	else
	{
		i++;
		if (i == 257)
			i = 128;
		source->packbits_n = i;
		return source->packbits_rep;
	}
}

static char *getString(source_t *source)
{
	size_t len = get8(source);
	size_t odd = !(len & 1);
	char *s;

	if (source->total < len + odd)
	{
		source->total = 0;
		fz_throw(source->ctx, FZ_ERROR_FORMAT, "Truncated string in PSD");
	}

	s = fz_malloc(source->ctx, len+1);
	memcpy(s, source->p, len);
	s[len] = 0;

	source->p += len + odd;
	source->total -= len + odd;

	return s;
}

static fz_pixmap *
psd_read_image(fz_context *ctx, struct info *info, const unsigned char *p, size_t total, int only_metadata)
{
	int v, bpc, c, n;
	source_t source;
	size_t ir_len, data_len;
	fz_separations *seps = NULL;
	fz_pixmap *image = NULL;
	size_t m;
	unsigned char *q;
	int alpha = 0;

	source.ctx = ctx;
	source.p = p;
	source.total = total;
	source.packbits = 0;

	memset(info, 0, sizeof(*info));

	fz_var(image);
	fz_var(seps);

	fz_try(ctx)
	{
		info->xres = 96;
		info->yres = 96;

		v = get32be(&source);
		/* Read signature */
		if (v != 0x38425053 /* 8BPS */)
			fz_throw(ctx, FZ_ERROR_FORMAT, "not a psd image (wrong signature)");

		/* Version */
		v = get16be(&source);
		if (v != 1)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad PSD version");

		(void)get16be(&source);
		(void)get32be(&source);

		info->n = n = get16be(&source);
		info->height = getu32be(&source);
		info->width = getu32be(&source);
		if (info->height == 0)
			fz_throw(ctx, FZ_ERROR_FORMAT, "image height must be > 0");
		if (info->width == 0)
			fz_throw(ctx, FZ_ERROR_FORMAT, "image width must be > 0");

		bpc = get16be(&source);
		if (bpc != 8 && bpc != 16)
			fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Only 8 or 16 bpc PSD files supported!");

		c = get16be(&source);
		if (c == 4) /* CMYK (+ Spots?) */
		{
			if (n != 4)
				fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "CMYK PSD with %d chans not supported!", n);
			info->cs = fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
		}
		else if (c == 3) /* RGB */
		{
			if (n == 4)
				alpha = 1;
			else if (n != 3)
				fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "RGB PSD with %d chans not supported!", n);
			info->cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		}
		else if (c == 1) /* Greyscale */
		{
			if (n != 1)
				fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Greyscale PSD with %d chans not supported!", n);
			info->cs = fz_keep_colorspace(ctx, fz_device_gray(ctx));
		}
		else
			fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Unsupported PSD colorspace (%d)!", c);

		v = get32be(&source);
		if (v != 0)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected color data in PSD!");

		/* Now read image resources... */
		ir_len = getu32be(&source);
		while (ir_len >= 12)
		{
			size_t start = source.p - p;

			v = get32be(&source);
			if (v != 0x3842494d) /* 8BIM */
				fz_throw(ctx, FZ_ERROR_FORMAT, "Failed to find expected 8BIM in PSD");
			v = get16be(&source);

			fz_free(ctx, getString(&source));

			data_len = getu32be(&source);
			ir_len -= (source.p - p) - start;
			switch (v)
			{
			case 0x3ef: /* Spot */
			{
				int spots = 0;
				int alpha_found = 0;

				while (data_len > 0)
				{
					int C, M, Y, K;
					char text[32];

					v = get16be(&source);
					if (v == 0 && alpha_found == 0)
						alpha_found = 1, alpha = 1;
					else if (v != 2)
						fz_throw(ctx, FZ_ERROR_FORMAT, "Non CMYK spot found in PSD");

					C = 0xff - (get16be(&source)>>8);
					M = 0xff - (get16be(&source)>>8);
					Y = 0xff - (get16be(&source)>>8);
					K = 0xff - (get16be(&source)>>8);
					(void)get16be(&source); /* opacity */
					(void)get8(&source); /* kind */
					(void)get8(&source); /* padding */
					if (v == 2)
					{
						uint32_t cmyk = C | (M<<8) | (Y<<16) | (K<<24);
						int R = fz_clampi(255-C-K, 0, 255);
						int G = fz_clampi(255-M-K, 0, 255);
						int B = fz_clampi(255-Y-K, 0, 255);
						uint32_t rgba = R | (G<<8) | (B<<16);
						if (seps == NULL)
							seps = fz_new_separations(ctx, 1);
						snprintf(text, sizeof(text), "s%d", spots);
						/* Use the old entry-point until we fix the new one */
						fz_add_separation_equivalents(ctx, seps, rgba, cmyk, text);
						spots++;
					}
					data_len -= 14;
					ir_len -= 14;
				}
			}
			}
			/* Skip any unread data */
			if (data_len & 1)
				data_len++;
			ir_len -= data_len;
			while (data_len--)
				get8(&source);
		}
		if (fz_count_separations(ctx, seps) + info->cs->n + 1 == n && alpha == 0)
			alpha = 1;
		if (fz_count_separations(ctx, seps) + info->cs->n + alpha != n)
			fz_throw(ctx, FZ_ERROR_FORMAT, "PSD contains mismatching spot/alpha data");

		/* Skip over the Layer data. */
		v = get32be(&source);
		if (v != 0)
		{
		  if (source.total < (size_t)v)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated PSD");
			source.total -= v;
			source.p += v;
		}
		if (source.total == 0)
			fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Unflattened PSD not supported");

		v = get16be(&source);
		switch (v)
		{
		case 0:
			/* No compression */
			break;
		case 1:
			/* Packbits */
			source.packbits = 1;
			source.packbits_n = 128;

			/* Skip over rows * channels * byte counts. */
			m = ((size_t)info->height) * info->n * 2;
			if (m > source.total)
				fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated RLE PSD");
			source.total -= m;
			source.p += m;
			break;
		case 2: /* Deflate */
		case 3: /* Deflate with prediction */
			fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Deflate PSD not supported");
		default:
			fz_throw(ctx, FZ_ERROR_FORMAT, "Unexpected compression (%d) found in PSD", v);
		}

		if (only_metadata)
			break;

		m = ((size_t)info->width) * info->height;
		image = fz_new_pixmap(ctx, info->cs, info->width, info->height, seps, alpha);
		q = image->samples;
		if (bpc == 8)
		{
			if (n == 1)
			{
				while (m--)
				{
					*q++ = 255 - unpack8(&source);
				}
			}
			else if (n - alpha == 3)
			{
				int N = n;

				while (N--)
				{
					size_t M = m;
					while (M--)
					{
						*q = unpack8(&source);
						q += n;
					}
					q -= m*n - 1;
				}
			}
			else
			{
				int N = n - alpha;

				/* CMYK is inverted */
				while (N--)
				{
					size_t M = m;
					while (M--)
					{
						*q = 255 - unpack8(&source);
						q += n;
					}
					q -= m*n - 1;
				}

				/* But alpha is not */
				if (alpha)
				{
					size_t M = m;
					while (M--)
					{
						*q = unpack8(&source);
						q += n;
					}
					q -= m*n - 1;
				}
			}
		}
		else
		{
			if (n == 1)
			{
				while (m--)
				{
					*q++ = 255 - unpack8(&source);
					(void)unpack8(&source);
				}
			}
			else if (n - alpha == 3)
			{
				int N = n;

				while (N--)
				{
					size_t M = m;

					while (M--)
					{
						*q = unpack8(&source);
						(void)unpack8(&source);
						q += n;
					}
					q -= m*n - 1;
				}
			}
			else
			{
				int N = n - alpha;

				/* CMYK is inverted */
				while (N--)
				{
					size_t M = m;

					while (M--)
					{
						*q = 255 - unpack8(&source);
						(void)unpack8(&source);
						q += n;
					}
					q -= m*n - 1;
				}

				/* But alpha is not */
				if (alpha)
				{
					size_t M = m;

					while (M--)
					{
						*q = unpack8(&source);
						(void)unpack8(&source);
						q += n;
					}
					q -= m*n - 1;
				}
			}
		}

		if (alpha)
			fz_premultiply_pixmap(ctx, image);
	}
	fz_always(ctx)
	{
		fz_drop_separations(ctx, seps);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_drop_colorspace(ctx, info->cs);
		fz_rethrow(ctx);
	}

	return image;
}

fz_pixmap *
fz_load_psd(fz_context *ctx, const unsigned char *p, size_t total)
{
	fz_pixmap *image = NULL;
	struct info psd;

	image = psd_read_image(ctx, &psd, p, total, 0);

	fz_drop_colorspace(ctx, psd.cs);

	return image;
}

void
fz_load_psd_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct info psd;

	psd_read_image(ctx, &psd, p, total, 1);

	*cspacep = psd.cs;
	*wp = psd.width;
	*hp = psd.height;
	*xresp = psd.xres;
	*yresp = psd.xres;
}
