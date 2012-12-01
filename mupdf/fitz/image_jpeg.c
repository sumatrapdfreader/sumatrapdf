#include "fitz-internal.h"

#include <jpeglib.h>

/* SumatraPDF: prevent inconsistent ctx state */
static void error_exit(j_common_ptr cinfo)
{
	char msg[JMSG_LENGTH_MAX];
	cinfo->err->format_message(cinfo, msg);
	fz_throw((fz_context *)cinfo->client_data, "jpeg error: %s", msg);
}

static void init_source(j_decompress_ptr cinfo)
{
	/* nothing to do */
}

static void term_source(j_decompress_ptr cinfo)
{
	/* nothing to do */
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
	static unsigned char eoi[2] = { 0xFF, JPEG_EOI };
	struct jpeg_source_mgr *src = cinfo->src;
	src->next_input_byte = eoi;
	src->bytes_in_buffer = 2;
	return 1;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	struct jpeg_source_mgr *src = cinfo->src;
	if (num_bytes > 0)
	{
		size_t skip = (size_t)num_bytes; /* size_t may be 64bit */
		if (skip > src->bytes_in_buffer)
			skip = (size_t)src->bytes_in_buffer;
		src->next_input_byte += skip;
		src->bytes_in_buffer -= skip;
	}
}

/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1963 */
static inline int read_value(unsigned char *data, int bytes, int is_big_endian)
{
	int value = 0;
	if (!is_big_endian)
		data += bytes;
	for (; bytes > 0; bytes--)
		value = (value << 8) | (is_big_endian ? *data++ : *--data);
	return value;
}

static void extract_exif_resolution(unsigned char *rbuf, int rlen, int *xres, int *yres)
{
	int is_big_endian;
	int offset, ifd_len, res_type = 0;
	float x_res = 0, y_res = 0;

	if (rlen >= 10 &&
		read_value(rbuf + 2, 2, 1) == 0xFFE0 &&
		read_value(rbuf + 6, 4, 1) == 0x4A464946 /* JFIF */ &&
		read_value(rbuf + 4, 2, 1) <= rlen - 2)
	{
		int jfif_len = read_value(rbuf + 4, 2, 1) + 2;
		rbuf += jfif_len;
		rlen -= jfif_len;
	}

	if (rlen < 20 ||
		read_value(rbuf + 2, 2, 1) != 0xFFE1 ||
		read_value(rbuf + 6, 4, 1) != 0x45786966 /* Exif */ ||
		read_value(rbuf + 10, 2, 1) != 0x0000)
	{
		return;
	}
	if (read_value(rbuf + 12, 4, 1) == 0x49492A00)
		is_big_endian = 0;
	else if (read_value(rbuf + 12, 4, 1) == 0x4D4D002A)
		is_big_endian = 1;
	else
		return;

	offset = read_value(rbuf + 16, 4, is_big_endian) + 12;
	if (offset < 20 || offset + 2 >= rlen)
		return;
	ifd_len = read_value(rbuf + offset, 2, is_big_endian);
	for (offset += 2; ifd_len > 0 && offset + 12 < rlen; ifd_len--, offset += 12)
	{
		int tag = read_value(rbuf + offset, 2, is_big_endian);
		int type = read_value(rbuf + offset + 2, 2, is_big_endian);
		int count = read_value(rbuf + offset + 4, 4, is_big_endian);
		int value_off = read_value(rbuf + offset + 8, 4, is_big_endian) + 12;
		switch (tag)
		{
		case 0x11A:
			if (type == 5 && value_off > offset && value_off + 8 <= rlen)
				x_res = 1.0f * read_value(rbuf + value_off, 4, is_big_endian) / read_value(rbuf + value_off + 4, 4, is_big_endian);
			break;
		case 0x11B:
			if (type == 5 && value_off > offset && value_off + 8 <= rlen)
				y_res = 1.0f * read_value(rbuf + value_off, 4, is_big_endian) / read_value(rbuf + value_off + 4, 4, is_big_endian);
			break;
		case 0x128:
			if (type == 3 && count == 1)
				res_type = read_value(rbuf + offset + 8, 2, is_big_endian);
			break;
		}
	}

	if (x_res <= 0 || x_res > INT_MAX || y_res <= 0 || y_res > INT_MAX)
		return;
	if (res_type == 2)
	{
		*xres = (int)x_res;
		*yres = (int)y_res;
	}
	else if (res_type == 3)
	{
		*xres = (int)(x_res * 254 / 100);
		*yres = (int)(y_res * 254 / 100);
	}
}

fz_pixmap *
fz_load_jpeg(fz_context *ctx, unsigned char *rbuf, int rlen)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr err;
	struct jpeg_source_mgr src;
	unsigned char *row[1], *sp, *dp;
	fz_colorspace *colorspace;
	unsigned int x;
	int k;
	fz_pixmap *image = NULL;

	fz_var(image);
	fz_var(row);

	row[0] = NULL;

	fz_try(ctx)
	{
		/* SumatraPDF: prevent inconsistent ctx state */
		cinfo.client_data = ctx;
		cinfo.err = jpeg_std_error(&err);
		err.error_exit = error_exit;

		jpeg_create_decompress(&cinfo);

		cinfo.src = &src;
		src.init_source = init_source;
		src.fill_input_buffer = fill_input_buffer;
		src.skip_input_data = skip_input_data;
		src.resync_to_restart = jpeg_resync_to_restart;
		src.term_source = term_source;
		src.next_input_byte = rbuf;
		src.bytes_in_buffer = rlen;

		jpeg_read_header(&cinfo, 1);

		jpeg_start_decompress(&cinfo);

		if (cinfo.output_components == 1)
			colorspace = fz_device_gray;
		else if (cinfo.output_components == 3)
			colorspace = fz_device_rgb;
		else if (cinfo.output_components == 4)
			colorspace = fz_device_cmyk;
		else
			fz_throw(ctx, "bad number of components in jpeg: %d", cinfo.output_components);

		image = fz_new_pixmap(ctx, colorspace, cinfo.output_width, cinfo.output_height);

		if (cinfo.density_unit == 1)
		{
			image->xres = cinfo.X_density;
			image->yres = cinfo.Y_density;
		}
		else if (cinfo.density_unit == 2)
		{
			image->xres = cinfo.X_density * 254 / 100;
			image->yres = cinfo.Y_density * 254 / 100;
		}
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1963 */
		else if (cinfo.density_unit == 0)
		{
			extract_exif_resolution(rbuf, rlen, &image->xres, &image->yres);
		}

		if (image->xres <= 0) image->xres = 72;
		if (image->yres <= 0) image->yres = 72;

		fz_clear_pixmap(ctx, image);

		row[0] = fz_malloc(ctx, cinfo.output_components * cinfo.output_width);
		dp = image->samples;
		while (cinfo.output_scanline < cinfo.output_height)
		{
			jpeg_read_scanlines(&cinfo, row, 1);
			sp = row[0];
			for (x = 0; x < cinfo.output_width; x++)
			{
				for (k = 0; k < cinfo.output_components; k++)
					*dp++ = *sp++;
				*dp++ = 255;
			}
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, row[0]);
		row[0] = NULL;
		/* SumatraPDF: prevent inconsistent ctx state */
		fz_try(ctx)
		{
			jpeg_finish_decompress(&cinfo);
		}
		fz_catch(ctx) { }
		jpeg_destroy_decompress(&cinfo);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}
