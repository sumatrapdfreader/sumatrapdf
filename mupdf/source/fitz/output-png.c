#include "mupdf/fitz.h"

#include <string.h>

#include <zlib.h>

static inline void big32(unsigned char *buf, unsigned int v)
{
	buf[0] = (v >> 24) & 0xff;
	buf[1] = (v >> 16) & 0xff;
	buf[2] = (v >> 8) & 0xff;
	buf[3] = (v) & 0xff;
}

static void putchunk(fz_context *ctx, fz_output *out, char *tag, unsigned char *data, size_t size)
{
	unsigned int sum;

	if ((uint32_t)size != size)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PNG chunk too large");

	fz_write_int32_be(ctx, out, (int)size);
	fz_write_data(ctx, out, tag, 4);
	fz_write_data(ctx, out, data, size);
	sum = crc32(0, NULL, 0);
	sum = crc32(sum, (unsigned char*)tag, 4);
	sum = crc32(sum, data, (unsigned int)size);
	fz_write_int32_be(ctx, out, sum);
}

void
fz_save_pixmap_as_png(fz_context *ctx, fz_pixmap *pixmap, const char *filename)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, 0);
	fz_band_writer *writer = NULL;

	fz_var(writer);

	fz_try(ctx)
	{
		writer = fz_new_png_band_writer(ctx, out);
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, pixmap->xres, pixmap->yres, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_band_writer(ctx, writer);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_write_pixmap_as_png(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap)
{
	fz_band_writer *writer;

	if (!out)
		return;

	writer = fz_new_png_band_writer(ctx, out);

	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, pixmap->xres, pixmap->yres, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
	}
	fz_always(ctx)
	{
		fz_drop_band_writer(ctx, writer);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

typedef struct png_band_writer_s
{
	fz_band_writer super;
	unsigned char *udata;
	unsigned char *cdata;
	uLong usize, csize;
	z_stream stream;
	int stream_ended;
} png_band_writer;

static void
png_write_icc(fz_context *ctx, png_band_writer *writer, fz_colorspace *cs)
{
#if FZ_ENABLE_ICC
	if (cs && !(cs->flags & FZ_COLORSPACE_IS_DEVICE) && (cs->flags & FZ_COLORSPACE_IS_ICC) && cs->u.icc.buffer)
	{
		fz_output *out = writer->super.out;
		size_t size, csize;
		fz_buffer *buffer = cs->u.icc.buffer;
		unsigned char *pos, *cdata, *chunk = NULL;
		const char *name;

		/* Deflate the profile */
		cdata = fz_new_deflated_data_from_buffer(ctx, &csize, buffer, FZ_DEFLATE_DEFAULT);

		if (!cdata)
			return;

		name = cs->name;
		size = csize + strlen(name) + 2;

		fz_try(ctx)
		{
			chunk = fz_calloc(ctx, size, 1);
			pos = chunk;
			memcpy(chunk, name, strlen(name));
			pos += strlen(name) + 2;
			memcpy(pos, cdata, csize);
			putchunk(ctx, out, "iCCP", chunk, size);
		}
		fz_always(ctx)
		{
			fz_free(ctx, cdata);
			fz_free(ctx, chunk);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}
#endif
}

static void
png_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	png_band_writer *writer = (png_band_writer *)(void *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int alpha = writer->super.alpha;
	static const unsigned char pngsig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	unsigned char head[13];
	int color;

	if (writer->super.s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PNGs cannot contain spot colors");

	/* Treat alpha only as greyscale */
	if (n == 1 && alpha)
		alpha = 0;

	switch (n - alpha)
	{
	case 1: color = (alpha ? 4 : 0); break; /* 0 = Greyscale, 4 = Greyscale + Alpha */
	case 3: color = (alpha ? 6 : 2); break; /* 2 = RGB, 6 = RGBA */
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "pixmap must be grayscale or rgb to write as png");
	}

	big32(head+0, w);
	big32(head+4, h);
	head[8] = 8; /* depth */
	head[9] = color;
	head[10] = 0; /* compression */
	head[11] = 0; /* filter */
	head[12] = 0; /* interlace */

	fz_write_data(ctx, out, pngsig, 8);
	putchunk(ctx, out, "IHDR", head, 13);

	png_write_icc(ctx, writer, cs);
}

static void
png_write_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *sp)
{
	png_band_writer *writer = (png_band_writer *)(void *)writer_;
	fz_output *out = writer->super.out;
	unsigned char *dp;
	int y, x, k, err, finalband;
	int w, h, n;

	if (!out)
		return;

	w = writer->super.w;
	h = writer->super.h;
	n = writer->super.n;

	finalband = (band_start+band_height >= h);
	if (finalband)
		band_height = h - band_start;

	if (writer->udata == NULL)
	{
		writer->usize = (w * n + 1) * band_height;
		/* Sadly the bound returned by compressBound is just for a
		 * single usize chunk; if you compress a sequence of them
		 * the buffering can result in you suddenly getting a block
		 * larger than compressBound outputted in one go, even if you
		 * take all the data out each time. */
		writer->csize = compressBound(writer->usize);
		writer->udata = Memento_label(fz_malloc(ctx, writer->usize), "png_write_udata");
		writer->cdata = Memento_label(fz_malloc(ctx, writer->csize), "png_write_cdata");
		writer->stream.opaque = ctx;
		writer->stream.zalloc = fz_zlib_alloc;
		writer->stream.zfree = fz_zlib_free;
		err = deflateInit(&writer->stream, Z_DEFAULT_COMPRESSION);
		if (err != Z_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
	}

	dp = writer->udata;
	stride -= w*n;
	if (writer->super.alpha)
	{
		/* Unpremultiply data */
		for (y = 0; y < band_height; y++)
		{
			int prev[FZ_MAX_COLORS];
			*dp++ = 1; /* sub prediction filter */
			for (x = 0; x < w; x++)
			{
				int a = sp[n-1];
				int inva = a ? 256*255/a : 0;
				int p;
				for (k = 0; k < n-1; k++)
				{
					int v = (sp[k] * inva + 128)>>8;
					p = x ? prev[k] : 0;
					prev[k] = v;
					v -= p;
					dp[k] = v;
				}
				p = x ? prev[k] : 0;
				prev[k] = a;
				a -= p;
				dp[k] = a;
				sp += n;
				dp += n;
			}
			sp += stride;
		}
	}
	else
	{
		for (y = 0; y < band_height; y++)
		{
			*dp++ = 1; /* sub prediction filter */
			for (x = 0; x < w; x++)
			{
				for (k = 0; k < n; k++)
				{
					if (x == 0)
						dp[k] = sp[k];
					else
						dp[k] = sp[k] - sp[k-n];
				}
				sp += n;
				dp += n;
			}
			sp += stride;
		}
	}

	writer->stream.next_in = (Bytef*)writer->udata;
	writer->stream.avail_in = (uInt)(dp - writer->udata);
	do
	{
		writer->stream.next_out = writer->cdata;
		writer->stream.avail_out = (uInt)writer->csize;

		if (!finalband)
		{
			err = deflate(&writer->stream, Z_NO_FLUSH);
			if (err != Z_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
		}
		else
		{
			err = deflate(&writer->stream, Z_FINISH);
			if (err != Z_STREAM_END)
				fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
		}

		if (writer->stream.next_out != writer->cdata)
			putchunk(ctx, out, "IDAT", writer->cdata, writer->stream.next_out - writer->cdata);
	}
	while (writer->stream.avail_out == 0);
}

static void
png_write_trailer(fz_context *ctx, fz_band_writer *writer_)
{
	png_band_writer *writer = (png_band_writer *)(void *)writer_;
	fz_output *out = writer->super.out;
	unsigned char block[1];
	int err;

	writer->stream_ended = 1;
	err = deflateEnd(&writer->stream);
	if (err != Z_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);

	putchunk(ctx, out, "IEND", block, 0);
}

static void
png_drop_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	png_band_writer *writer = (png_band_writer *)(void *)writer_;

	if (!writer->stream_ended)
	{
		int err = deflateEnd(&writer->stream);
		if (err != Z_OK)
			fz_warn(ctx, "ignoring compression error %d", err);
	}

	fz_free(ctx, writer->cdata);
	fz_free(ctx, writer->udata);
}

fz_band_writer *fz_new_png_band_writer(fz_context *ctx, fz_output *out)
{
	png_band_writer *writer = fz_new_band_writer(ctx, png_band_writer, out);

	writer->super.header = png_write_header;
	writer->super.band = png_write_band;
	writer->super.trailer = png_write_trailer;
	writer->super.drop = png_drop_band_writer;

	return &writer->super;
}

/* We use an auxiliary function to do pixmap_as_png, as it can enable us to
 * drop pix early in the case where we have to convert, potentially saving
 * us having to have 2 copies of the pixmap and a buffer open at once. */
static fz_buffer *
png_from_pixmap(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params, int drop)
{
	fz_buffer *buf = NULL;
	fz_output *out = NULL;
	fz_pixmap *pix2 = NULL;

	fz_var(buf);
	fz_var(out);
	fz_var(pix2);

	if (pix->w == 0 || pix->h == 0)
	{
		if (drop)
			fz_drop_pixmap(ctx, pix);
		return NULL;
	}

	fz_try(ctx)
	{
		if (pix->colorspace && pix->colorspace != fz_device_gray(ctx) && pix->colorspace != fz_device_rgb(ctx))
		{
			pix2 = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), NULL, NULL, color_params, 1);
			if (drop)
				fz_drop_pixmap(ctx, pix);
			pix = pix2;
		}
		buf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_write_pixmap_as_png(ctx, out, pix);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, drop ? pix : pix2);
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
fz_new_buffer_from_image_as_png(fz_context *ctx, fz_image *image, fz_color_params color_params)
{
	fz_pixmap *pix = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
	return png_from_pixmap(ctx, pix, color_params, 1);
}

fz_buffer *
fz_new_buffer_from_pixmap_as_png(fz_context *ctx, fz_pixmap *pix, fz_color_params color_params)
{
	return png_from_pixmap(ctx, pix, color_params, 0);
}
