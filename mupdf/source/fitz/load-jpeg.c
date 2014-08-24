#include "mupdf/fitz.h"

#include <jpeglib.h>

#ifdef SHARE_JPEG

#define JZ_CTX_FROM_CINFO(c) (fz_context *)(c->client_data)

#define fz_jpg_mem_init(ctx, cinfo)
#define fz_jpg_mem_term(cinfo)

#else /* SHARE_JPEG */

typedef void * backing_store_ptr;
#include "jmemcust.h"

#define JZ_CTX_FROM_CINFO(c) (fz_context *)(GET_CUST_MEM_DATA(c)->priv)

static void *
fz_jpg_mem_alloc(j_common_ptr cinfo, size_t size)
{
	fz_context *ctx = JZ_CTX_FROM_CINFO(cinfo);
	return fz_malloc(ctx, size);
}

static void
fz_jpg_mem_free(j_common_ptr cinfo, void *object, size_t size)
{
	fz_context *ctx = JZ_CTX_FROM_CINFO(cinfo);
	UNUSED(size);
	fz_free(ctx, object);
}

static void
fz_jpg_mem_init(fz_context *ctx, struct jpeg_decompress_struct *cinfo)
{
	jpeg_cust_mem_data *custmptr;

	custmptr = fz_malloc_struct(ctx, jpeg_cust_mem_data);

	if (!jpeg_cust_mem_init(custmptr, (void *) ctx, NULL, NULL, NULL,
				fz_jpg_mem_alloc, fz_jpg_mem_free,
				fz_jpg_mem_alloc, fz_jpg_mem_free, NULL))
	{
		fz_free(ctx, custmptr);
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot initialize custom JPEG memory handler");
	}

	cinfo->client_data = custmptr;
}

static void
fz_jpg_mem_term(struct jpeg_decompress_struct *cinfo)
{
	if(cinfo->client_data)
	{
		fz_context *ctx = JZ_CTX_FROM_CINFO(cinfo);
		fz_free(ctx, cinfo->client_data);
		cinfo->client_data = NULL;
	}
}

#endif /* SHARE_JPEG */

static void error_exit(j_common_ptr cinfo)
{
	char msg[JMSG_LENGTH_MAX];
	fz_context *ctx = (fz_context *)cinfo->client_data;

	cinfo->err->format_message(cinfo, msg);
	fz_throw(ctx, FZ_ERROR_GENERIC, "jpeg error: %s", msg);
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

static inline int read_value(const unsigned char *data, int bytes, int is_big_endian)
{
	int value = 0;
	if (!is_big_endian)
		data += bytes;
	for (; bytes > 0; bytes--)
		value = (value << 8) | (is_big_endian ? *data++ : *--data);
	return value;
}

static int extract_exif_resolution(jpeg_saved_marker_ptr marker, int *xres, int *yres)
{
	int is_big_endian;
	const unsigned char *data;
	unsigned int offset, ifd_len, res_type = 0;
	float x_res = 0, y_res = 0;

	if (!marker || marker->marker != JPEG_APP0 + 1 || marker->data_length < 14)
		return 0;
	data = (const unsigned char *)marker->data;
	if (read_value(data, 4, 1) != 0x45786966 /* Exif */ || read_value(data + 4, 2, 1) != 0x0000)
		return 0;
	if (read_value(data + 6, 4, 1) == 0x49492A00)
		is_big_endian = 0;
	else if (read_value(data + 6, 4, 1) == 0x4D4D002A)
		is_big_endian = 1;
	else
		return 0;

	offset = read_value(data + 10, 4, is_big_endian) + 6;
	if (offset < 14 || offset > marker->data_length - 2)
		return 0;
	ifd_len = read_value(data + offset, 2, is_big_endian);
	for (offset += 2; ifd_len > 0 && offset + 12 < marker->data_length; ifd_len--, offset += 12)
	{
		int tag = read_value(data + offset, 2, is_big_endian);
		int type = read_value(data + offset + 2, 2, is_big_endian);
		int count = read_value(data + offset + 4, 4, is_big_endian);
		unsigned int value_off = read_value(data + offset + 8, 4, is_big_endian) + 6;
		switch (tag)
		{
		case 0x11A:
			if (type == 5 && value_off > offset && value_off <= marker->data_length - 8)
				x_res = 1.0f * read_value(data + value_off, 4, is_big_endian) / read_value(data + value_off + 4, 4, is_big_endian);
			break;
		case 0x11B:
			if (type == 5 && value_off > offset && value_off <= marker->data_length - 8)
				y_res = 1.0f * read_value(data + value_off, 4, is_big_endian) / read_value(data + value_off + 4, 4, is_big_endian);
			break;
		case 0x128:
			if (type == 3 && count == 1)
				res_type = read_value(data + offset + 8, 2, is_big_endian);
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
	else
	{
		*xres = 0;
		*yres = 0;
	}
	return 1;
}

static int extract_app13_resolution(jpeg_saved_marker_ptr marker, int *xres, int *yres)
{
	const unsigned char *data, *data_end;

	if (!marker || marker->marker != JPEG_APP0 + 13 || marker->data_length < 42 ||
		strcmp((const char *)marker->data, "Photoshop 3.0") != 0)
	{
		return 0;
	}

	data = (const unsigned char *)marker->data;
	data_end = data + marker->data_length;
	for (data += 14; data + 12 < data_end; ) {
		int data_size = -1;
		int tag = read_value(data + 4, 2, 1);
		int value_off = 11 + read_value(data + 6, 2, 1);
		if (value_off % 2 == 1)
			value_off++;
		if (read_value(data, 4, 1) == 0x3842494D /* 8BIM */ && value_off <= data_end - data)
			data_size = read_value(data + value_off - 4, 4, 1);
		if (data_size < 0 || data_size > data_end - data - value_off)
			return 0;
		if (tag == 0x3ED && data_size == 16)
		{
			*xres = read_value(data + value_off, 2, 1);
			*yres = read_value(data + value_off + 8, 2, 1);
			return 1;
		}
		if (data_size % 2 == 1)
			data_size++;
		data += value_off + data_size;
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

		fz_jpg_mem_init(ctx, &cinfo);

		jpeg_create_decompress(&cinfo);

		cinfo.src = &src;
		src.init_source = init_source;
		src.fill_input_buffer = fill_input_buffer;
		src.skip_input_data = skip_input_data;
		src.resync_to_restart = jpeg_resync_to_restart;
		src.term_source = term_source;
		src.next_input_byte = rbuf;
		src.bytes_in_buffer = rlen;

		jpeg_save_markers(&cinfo, JPEG_APP0+1, 0xffff);
		jpeg_save_markers(&cinfo, JPEG_APP0+13, 0xffff);

		jpeg_read_header(&cinfo, 1);

		if (cinfo.num_components == 1)
			*cspacep = fz_device_gray(ctx);
		else if (cinfo.num_components == 3)
			*cspacep = fz_device_rgb(ctx);
		else if (cinfo.num_components == 4)
			*cspacep = fz_device_cmyk(ctx);
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "bad number of components in jpeg: %d", cinfo.num_components);

		*xp = cinfo.image_width;
		*yp = cinfo.image_height;

		if (extract_exif_resolution(cinfo.marker_list, xresp, yresp))
			/* XPS prefers EXIF resolution to JFIF density */;
		else if (extract_app13_resolution(cinfo.marker_list, xresp, yresp))
			/* XPS prefers APP13 resolution to JFIF density */;
		else if (cinfo.density_unit == 1)
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

		if (*xresp <= 0) *xresp = 96;
		if (*yresp <= 0) *yresp = 96;
	}
	fz_always(ctx)
	{
		jpeg_destroy_decompress(&cinfo);
		fz_jpg_mem_term(&cinfo);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
