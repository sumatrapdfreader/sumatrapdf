// Copyright (C) 2004-2023 Artifex Software, Inc.
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

/*
 * Write pixmap to PNM file (without alpha channel)
 */
static void
pnm_write_header(fz_context *ctx, fz_band_writer *writer, fz_colorspace *cs)
{
	fz_output *out = writer->out;
	int w = writer->w;
	int h = writer->h;
	int n = writer->n;
	int alpha = writer->alpha;

	if (writer->s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PNM writer cannot cope with spot colors");
	if (cs && !fz_colorspace_is_gray(ctx, cs) && !fz_colorspace_is_rgb(ctx, cs))
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as pnm");

	/* Treat alpha only as greyscale */
	if (n == 1 && alpha)
		alpha = 0;
	n -= alpha;

	if (alpha)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PNM writer cannot cope with alpha");

	if (n == 1)
		fz_write_printf(ctx, out, "P5\n");
	if (n == 3)
		fz_write_printf(ctx, out, "P6\n");
	fz_write_printf(ctx, out, "%d %d\n", w, h);
	fz_write_printf(ctx, out, "255\n");
}

static void
pnm_write_band(fz_context *ctx, fz_band_writer *writer, int stride, int band_start, int band_height, const unsigned char *p)
{
	fz_output *out = writer->out;
	int w = writer->w;
	int h = writer->h;
	int n = writer->n;
	int len;
	int end = band_start + band_height;

	if (n != 1 && n != 3)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as pnm");

	if (!out)
		return;

	if (end > h)
		end = h;
	end -= band_start;

	/* Tests show that writing single bytes out at a time
	 * is appallingly slow. We get a huge improvement
	 * by collating stuff into buffers first. */

	while (end--)
	{
		len = w;
		while (len)
		{
			int num_written = len;

			switch (n)
			{
			case 1:
				/* No collation required */
				fz_write_data(ctx, out, p, num_written);
				p += num_written;
				break;
			case 3:
				fz_write_data(ctx, out, p, num_written*3);
				p += num_written*3;
				break;
			}
			len -= num_written;
		}
		p += stride - w*n;
	}
}

fz_band_writer *fz_new_pnm_band_writer(fz_context *ctx, fz_output *out)
{
	fz_band_writer *writer = fz_new_band_writer(ctx, fz_band_writer, out);

	writer->header = pnm_write_header;
	writer->band = pnm_write_band;

	return writer;
}

void
fz_write_pixmap_as_pnm(fz_context *ctx, fz_output *out, fz_pixmap *pixmap)
{
	fz_band_writer *writer = fz_new_pnm_band_writer(ctx, out);
	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, 0, 0, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
		fz_close_band_writer(ctx, writer);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_save_pixmap_as_pnm(fz_context *ctx, fz_pixmap *pixmap, const char *filename)
{
	fz_band_writer *writer = NULL;
	fz_output *out = fz_new_output_with_path(ctx, filename, 0);

	fz_var(writer);

	fz_try(ctx)
	{
		writer = fz_new_pnm_band_writer(ctx, out);
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, 0, 0, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
		fz_close_band_writer(ctx, writer);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_band_writer(ctx, writer);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
 * Write pixmap to PAM file (with or without alpha channel)
 */

static void
pam_write_header(fz_context *ctx, fz_band_writer *writer, fz_colorspace *cs)
{
	fz_output *out = writer->out;
	int w = writer->w;
	int h = writer->h;
	int n = writer->n;
	int alpha = writer->alpha;

	if (writer->s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PAM writer cannot cope with spot colors");

	fz_write_printf(ctx, out, "P7\n");
	fz_write_printf(ctx, out, "WIDTH %d\n", w);
	fz_write_printf(ctx, out, "HEIGHT %d\n", h);
	fz_write_printf(ctx, out, "DEPTH %d\n", n);
	fz_write_printf(ctx, out, "MAXVAL 255\n");

	n -= alpha;

	if (n == 0 && alpha) fz_write_printf(ctx, out, "TUPLTYPE GRAYSCALE\n");
	else if (n == 1 && !alpha && fz_colorspace_is_gray(ctx, cs)) fz_write_printf(ctx, out, "TUPLTYPE GRAYSCALE\n");
	else if (n == 1 && alpha && fz_colorspace_is_gray(ctx, cs)) fz_write_printf(ctx, out, "TUPLTYPE GRAYSCALE_ALPHA\n");
	else if (n == 3 && !alpha && fz_colorspace_is_rgb(ctx, cs)) fz_write_printf(ctx, out, "TUPLTYPE RGB\n");
	else if (n == 3 && alpha && fz_colorspace_is_rgb(ctx, cs)) fz_write_printf(ctx, out, "TUPLTYPE RGB_ALPHA\n");
	else if (n == 4 && !alpha && fz_colorspace_is_cmyk(ctx, cs)) fz_write_printf(ctx, out, "TUPLTYPE CMYK\n");
	else if (n == 4 && alpha && fz_colorspace_is_cmyk(ctx, cs)) fz_write_printf(ctx, out, "TUPLTYPE CMYK_ALPHA\n");
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be alpha only, gray, rgb, or cmyk");
	fz_write_printf(ctx, out, "ENDHDR\n");
}

static void
pam_write_band(fz_context *ctx, fz_band_writer *writer, int stride, int band_start, int band_height, const unsigned char *sp)
{
	fz_output *out = writer->out;
	int w = writer->w;
	int h = writer->h;
	int n = writer->n;
	int alpha = writer->alpha;
	int x, y;
	int end = band_start + band_height;

	if (!out)
		return;

	if (end > h)
		end = h;
	end -= band_start;

	if (alpha)
	{
		/* Buffer must be a multiple of 2, 3 and 5 at least. */
		/* Also, for the generic case, it must be bigger than FZ_MAX_COLORS */
		char buffer[2*3*4*5*6];
		char *b = buffer;
		stride -= n * w;
		switch (n)
		{
		case 2:
			for (y = 0; y < end; y++)
			{
				for (x = 0; x < w; x++)
				{
					int a = sp[1];
					*b++ = a ? (sp[0] * 255 + (a>>1))/a : 0;
					*b++ = a;
					sp += 2;
					if (b == &buffer[sizeof(buffer)])
					{
						fz_write_data(ctx, out, buffer, sizeof(buffer));
						b = buffer;
					}
				}
				sp += stride;
			}
			if (b != buffer)
				fz_write_data(ctx, out, buffer, b - buffer);
			break;
		case 4:
			for (y = 0; y < end; y++)
			{
				for (x = 0; x < w; x++)
				{
					int a = sp[3];
					int inva = a ? 256 * 255 / a : 0;
					*b++ = (sp[0] * inva + 128)>>8;
					*b++ = (sp[1] * inva + 128)>>8;
					*b++ = (sp[2] * inva + 128)>>8;
					*b++ = a;
					sp += 4;
					if (b == &buffer[sizeof(buffer)])
					{
						fz_write_data(ctx, out, buffer, sizeof(buffer));
						b = buffer;
					}
				}
				sp += stride;
			}
			if (b != buffer)
				fz_write_data(ctx, out, buffer, b - buffer);
			break;
		case 5:
			for (y = 0; y < end; y++)
			{
				for (x = 0; x < w; x++)
				{
					int a = sp[4];
					int inva = a ? 256 * 255 / a : 0;
					*b++ = (sp[0] * inva + 128)>>8;
					*b++ = (sp[1] * inva + 128)>>8;
					*b++ = (sp[2] * inva + 128)>>8;
					*b++ = (sp[3] * inva + 128)>>8;
					*b++ = a;
					sp += 5;
					if (b == &buffer[sizeof(buffer)])
					{
						fz_write_data(ctx, out, buffer, sizeof(buffer));
						b = buffer;
					}
				}
				sp += stride;
			}
			if (b != buffer)
				fz_write_data(ctx, out, buffer, b - buffer);
			break;
		default:
			for (y = 0; y < end; y++)
			{
				for (x = 0; x < w; x++)
				{
					int a = sp[n-1];
					int inva = a ? 256 * 255 / a : 0;
					int k;
					for (k = 0; k < n-1; k++)
						*b++ = (*sp++ * inva + 128)>>8;
					*b++ = a;
					sp++;
					if (b >= &buffer[sizeof(buffer)] - n)
					{
						fz_write_data(ctx, out, buffer, b - buffer);
						b = buffer;
					}
				}
				sp += stride;
			}
			if (b != buffer)
				fz_write_data(ctx, out, buffer, b - buffer);
			break;
		}
	}
	else
		for (y = 0; y < end; y++)
		{
			fz_write_data(ctx, out, sp, (size_t)w * n);
			sp += stride;
		}
}

fz_band_writer *fz_new_pam_band_writer(fz_context *ctx, fz_output *out)
{
	fz_band_writer *writer = fz_new_band_writer(ctx, fz_band_writer, out);

	writer->header = pam_write_header;
	writer->band = pam_write_band;

	return writer;
}

void
fz_write_pixmap_as_pam(fz_context *ctx, fz_output *out, fz_pixmap *pixmap)
{
	fz_band_writer *writer = fz_new_pam_band_writer(ctx, out);
	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, 0, 0, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
		fz_close_band_writer(ctx, writer);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_save_pixmap_as_pam(fz_context *ctx, fz_pixmap *pixmap, const char *filename)
{
	fz_band_writer *writer = NULL;
	fz_output *out = fz_new_output_with_path(ctx, filename, 0);

	fz_var(writer);

	fz_try(ctx)
	{
		writer = fz_new_pam_band_writer(ctx, out);
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, 0, 0, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
		fz_close_band_writer(ctx, writer);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_band_writer(ctx, writer);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static fz_buffer *
buffer_from_pixmap(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params, int drop,
	void (*do_write)(fz_context *ctx, fz_output *out, fz_pixmap *pix))
{
	fz_buffer *buf = NULL;
	fz_output *out = NULL;

	fz_var(buf);
	fz_var(out);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		do_write(ctx, out, pix);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		if (drop)
			fz_drop_pixmap(ctx, pix);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
	return buf;
}

fz_buffer *
fz_new_buffer_from_image_as_pnm(fz_context *ctx, fz_image *image, fz_color_params color_params)
{
	fz_pixmap *pix = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
	return buffer_from_pixmap(ctx, pix, color_params, 1, fz_write_pixmap_as_pnm);
}

fz_buffer *
fz_new_buffer_from_pixmap_as_pnm(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params)
{
	return buffer_from_pixmap(ctx, pix, color_params, 0, fz_write_pixmap_as_pnm);
}

fz_buffer *
fz_new_buffer_from_image_as_pam(fz_context *ctx, fz_image *image, fz_color_params color_params)
{
	fz_pixmap *pix = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
	return buffer_from_pixmap(ctx, pix, color_params, 1, fz_write_pixmap_as_pam);
}

fz_buffer *
fz_new_buffer_from_pixmap_as_pam(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params)
{
	return buffer_from_pixmap(ctx, pix, color_params, 0, fz_write_pixmap_as_pam);
}
