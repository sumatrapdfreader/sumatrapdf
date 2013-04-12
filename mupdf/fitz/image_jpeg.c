#include "fitz-internal.h"

#include <jpeglib.h>

static void error_exit(j_common_ptr cinfo)
{
	char msg[JMSG_LENGTH_MAX];
	fz_context *ctx = (fz_context *)cinfo->client_data;

	cinfo->err->format_message(cinfo, msg);
	fz_throw(ctx, "jpeg error: %s", msg);
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

static int extract_exif_resolution(unsigned char *rbuf, int rlen, int *xres, int *yres)
{
	int is_big_endian;
	int offset, ifd_len, res_type = 0;
	float x_res = 0, y_res = 0;

	if (rlen >= 10 &&
		read_value(rbuf + 2, 2, 1) == 0xFFE0 /* APP0 */ &&
		read_value(rbuf + 6, 4, 1) == 0x4A464946 /* JFIF */ &&
		read_value(rbuf + 4, 2, 1) <= rlen - 2)
	{
		int jfif_len = read_value(rbuf + 4, 2, 1) + 2;
		rbuf += jfif_len;
		rlen -= jfif_len;
	}

	if (rlen < 20 ||
		read_value(rbuf + 2, 2, 1) != 0xFFE1 /* APP1 */ ||
		read_value(rbuf + 6, 4, 1) != 0x45786966 /* Exif */ ||
		read_value(rbuf + 10, 2, 1) != 0x0000)
	{
		return 0;
	}
	if (read_value(rbuf + 12, 4, 1) == 0x49492A00)
		is_big_endian = 0;
	else if (read_value(rbuf + 12, 4, 1) == 0x4D4D002A)
		is_big_endian = 1;
	else
		return 0;

	offset = read_value(rbuf + 16, 4, is_big_endian) + 12;
	if (offset < 20 || offset + 2 > rlen)
		return 0;
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
		return 0;
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
	return 1;
}

static int extract_app13_resolution(unsigned char *rbuf, int rlen, int *xres, int *yres)
{
	unsigned char *seg_end;

	if (rlen < 48 ||
		read_value(rbuf + 2, 2, 1) != 0xFFED /* APP13 */ ||
		rbuf[19] != 0 || strcmp((const char *)rbuf + 6, "Photoshop 3.0") != 0 ||
		read_value(rbuf + 4, 2, 1) > rlen - 4)
	{
		return 0;
	}

	seg_end = rbuf + 4 + read_value(rbuf + 4, 2, 1);
	for (rbuf += 20; rbuf + 12 < seg_end; ) {
		int data_size = -1;
		int tag = read_value(rbuf + 4, 2, 1);
		int value_off = 11 + read_value(rbuf + 6, 2, 1);
		if (value_off % 2 == 1)
			value_off++;
		if (read_value(rbuf, 4, 1) == 0x3842494D /* 8BIM */ && rbuf + value_off <= seg_end)
			data_size = read_value(rbuf + value_off - 4, 4, 1);
		if (data_size < 0 || rbuf + value_off + data_size > seg_end)
			return 0;
		if (tag == 0x3ED && data_size == 16)
		{
			*xres = read_value(rbuf + value_off, 2, 1);
			*yres = read_value(rbuf + value_off + 8, 2, 1);
			return 1;
		}
		rbuf += value_off + data_size;
	}

	return 0;
}

void
fz_load_jpeg_info(fz_context *ctx, unsigned char *rbuf, int rlen, int *xp, int *yp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr err;
	struct jpeg_source_mgr src;

	fz_try(ctx)
	{
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

		if (cinfo.num_components == 1)
			*cspacep = fz_device_gray;
		else if (cinfo.num_components == 3)
			*cspacep = fz_device_rgb;
		else if (cinfo.num_components == 4)
			*cspacep = fz_device_cmyk;
		else
			fz_throw(ctx, "bad number of components in jpeg: %d", cinfo.num_components);

		*xp = cinfo.image_width;
		*yp = cinfo.image_height;

		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1963 */
		if (cinfo.density_unit == 0)
		{
			/* cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2252 */
			if (!extract_exif_resolution(rbuf, rlen, xresp, yresp))
				extract_app13_resolution(rbuf, rlen, xresp, yresp);
		}
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2249 */
		else if (extract_exif_resolution(rbuf, rlen, xresp, yresp))
			/* XPS seems to prefer EXIF resolution to JFIF density */;
		else
		if (cinfo.density_unit == 1)
		{
			*xresp = cinfo.X_density;
			*yresp = cinfo.Y_density;
		}
		else if (cinfo.density_unit == 2)
		{
			*xresp = cinfo.X_density * 254 / 100;
			*yresp = cinfo.Y_density * 254 / 100;
		}
		else
		{
			*xresp = 0;
			*yresp = 0;
		}

		if (*xresp <= 0) *xresp = 72;
		if (*yresp <= 0) *yresp = 72;
	}
	fz_always(ctx)
	{
		jpeg_destroy_decompress(&cinfo);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
