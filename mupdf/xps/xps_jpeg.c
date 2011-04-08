#include "fitz.h"
#include "muxps.h"

#include <jpeglib.h>
#include <setjmp.h>

struct jpeg_error_mgr_jmp
{
	struct jpeg_error_mgr super;
	jmp_buf env;
	char msg[JMSG_LENGTH_MAX];
};

static void error_exit(j_common_ptr cinfo)
{
	struct jpeg_error_mgr_jmp *err = (struct jpeg_error_mgr_jmp *)cinfo->err;
	cinfo->err->format_message(cinfo, err->msg);
	longjmp(err->env, 1);
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
		src->next_input_byte += num_bytes;
		src->bytes_in_buffer -= num_bytes;
	}
}

int
xps_decode_jpeg(fz_pixmap **imagep, byte *rbuf, int rlen)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_error_mgr_jmp err;
	struct jpeg_source_mgr src;
	unsigned char *row[1], *sp, *dp;
	fz_colorspace *colorspace;
	unsigned int x;
	int k;

	fz_pixmap *image = NULL;

	if (setjmp(err.env))
	{
		if (image)
			fz_drop_pixmap(image);
		return fz_throw("jpeg error: %s", err.msg);
	}

	cinfo.err = jpeg_std_error(&err.super);
	err.super.error_exit = error_exit;

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
		return fz_throw("bad number of components in jpeg: %d", cinfo.output_components);

	image = fz_new_pixmap_with_limit(colorspace, cinfo.output_width, cinfo.output_height);
	if (!image)
	{
		jpeg_finish_decompress(&cinfo);
		jpeg_destroy_decompress(&cinfo);
		return fz_throw("out of memory");
	}

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

	fz_clear_pixmap(image);

	row[0] = fz_malloc(cinfo.output_components * cinfo.output_width);
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
	fz_free(row[0]);

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);

	*imagep = image;
	return fz_okay;
}
