#include "mupdf/fitz.h"

#include <zlib.h>

typedef struct ps_band_writer_s
{
	fz_band_writer super;
	z_stream stream;
	int stream_ended;
	int input_size;
	unsigned char *input;
	int output_size;
	unsigned char *output;
} ps_band_writer;

void
fz_write_ps_file_header(fz_context *ctx, fz_output *out)
{
	fz_write_printf(ctx, out,
		"%%!PS-Adobe-3.0\n"
		//"%%%%BoundingBox: 0 0 612 792\n"
		//"%%%%HiResBoundingBox: 0 0 612 792\n"
		"%%%%Creator: MuPDF\n"
		"%%%%LanguageLevel: 2\n"
		"%%%%CreationDate: D:20160318101706Z00'00'\n"
		"%%%%DocumentData: Binary\n"
		"%%%%Pages: (atend)\n"
		"%%%%EndComments\n"
		"\n"
		"%%%%BeginProlog\n"
		"%%%%EndProlog\n"
		"\n"
		"%%%%BeginSetup\n"
		"%%%%EndSetup\n"
		"\n"
		);
}

void
fz_write_ps_file_trailer(fz_context *ctx, fz_output *out, int pages)
{
	fz_write_printf(ctx, out, "%%%%Trailer\n%%%%Pages: %d\n%%%%EOF\n", pages);
}

static void
ps_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	ps_band_writer *writer = (ps_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int alpha = writer->super.alpha;
	int xres = writer->super.xres;
	int yres = writer->super.yres;
	int pagenum = writer->super.pagenum;
	int w_points = (w * 72 + (xres>>1)) / xres;
	int h_points = (h * 72 + (yres>>1)) / yres;
	float sx = (float) w / w_points;
	float sy = (float) h / h_points;
	int err;

	if (writer->super.s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Postscript writer cannot cope with spot colors");

	if (alpha != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Postscript output cannot have alpha");

	writer->super.w = w;
	writer->super.h = h;
	writer->super.n = n;

	writer->stream.zalloc = fz_zlib_alloc;
	writer->stream.zfree = fz_zlib_free;
	writer->stream.opaque = ctx;

	err = deflateInit(&writer->stream, Z_DEFAULT_COMPRESSION);
	if (err != Z_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);

	fz_write_printf(ctx, out, "%%%%Page: %d %d\n", pagenum, pagenum);
	fz_write_printf(ctx, out, "%%%%PageBoundingBox: 0 0 %d %d\n", w_points, h_points);
	fz_write_printf(ctx, out, "%%%%BeginPageSetup\n");
	fz_write_printf(ctx, out, "<</PageSize [%d %d]>> setpagedevice\n", w_points, h_points);
	fz_write_printf(ctx, out, "%%%%EndPageSetup\n\n");
	fz_write_printf(ctx, out, "/DataFile currentfile /FlateDecode filter def\n\n");
	switch(n)
	{
	case 1:
		fz_write_string(ctx, out, "/DeviceGray setcolorspace\n");
		break;
	case 3:
		fz_write_string(ctx, out, "/DeviceRGB setcolorspace\n");
		break;
	case 4:
		fz_write_string(ctx, out, "/DeviceCMYK setcolorspace\n");
		break;
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected colorspace for ps output");
	}
	fz_write_printf(ctx, out,
		"<<\n"
		"/ImageType 1\n"
		"/Width %d\n"
		"/Height %d\n"
		"/ImageMatrix [ %g 0 0 -%g 0 %d ]\n"
		"/MultipleDataSources false\n"
		"/DataSource DataFile\n"
		"/BitsPerComponent 8\n"
		//"/Decode [0 1]\n"
		"/Interpolate false\n"
		">>\n"
		"image\n"
		, w, h, sx, sy, h);
}

static void
ps_write_trailer(fz_context *ctx, fz_band_writer *writer_)
{
	ps_band_writer *writer = (ps_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int err;

	writer->stream.next_in = NULL;
	writer->stream.avail_in = 0;
	writer->stream.next_out = (Bytef*)writer->output;
	writer->stream.avail_out = (uInt)writer->output_size;

	err = deflate(&writer->stream, Z_FINISH);
	if (err != Z_STREAM_END)
		fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);
	writer->stream_ended = 1;
	err = deflateEnd(&writer->stream);
	if (err != Z_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);

	fz_write_data(ctx, out, writer->output, writer->output_size - writer->stream.avail_out);
	fz_write_string(ctx, out, "\nshowpage\n%%%%PageTrailer\n%%%%EndPageTrailer\n\n");
}

static void
ps_drop_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	ps_band_writer *writer = (ps_band_writer *)writer_;

	if (!writer->stream_ended)
	{
		int err = deflateEnd(&writer->stream);
		if (err != Z_OK)
			fz_warn(ctx, "ignoring compression error %d", err);
	}

	fz_free(ctx, writer->input);
	fz_free(ctx, writer->output);
}

void fz_write_pixmap_as_ps(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap)
{
	fz_band_writer *writer;

	fz_write_ps_file_header(ctx, out);

	writer = fz_new_ps_band_writer(ctx, out);

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

	fz_write_ps_file_trailer(ctx, out, 1);
}

void fz_save_pixmap_as_ps(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		fz_write_pixmap_as_ps(ctx, out, pixmap);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
ps_write_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *samples)
{
	ps_band_writer *writer = (ps_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int x, y, i, err;
	int required_input;
	int required_output;
	unsigned char *o;

	if (!out)
		return;

	if (band_start+band_height >= h)
		band_height = h - band_start;

	required_input = w*n*band_height;
	required_output = (int)deflateBound(&writer->stream, required_input);

	if (writer->input == NULL || writer->input_size < required_input)
	{
		fz_free(ctx, writer->input);
		writer->input = NULL;
		writer->input = Memento_label(fz_malloc(ctx, required_input), "pswriter_input");
		writer->input_size = required_input;
	}

	if (writer->output == NULL || writer->output_size < required_output)
	{
		fz_free(ctx, writer->output);
		writer->output = NULL;
		writer->output = Memento_label(fz_malloc(ctx, required_output), "pswriter_output");
		writer->output_size = required_output;
	}

	o = writer->input;
	for (y = 0; y < band_height; y++)
	{
		for (x = 0; x < w; x++)
		{
			for (i = n; i > 0; i--)
				*o++ = *samples++;
		}
		samples += stride - w*n;
	}

	writer->stream.next_in = (Bytef*)writer->input;
	writer->stream.avail_in = required_input;
	writer->stream.next_out = (Bytef*)writer->output;
	writer->stream.avail_out = (uInt)writer->output_size;

	err = deflate(&writer->stream, Z_NO_FLUSH);
	if (err != Z_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "compression error %d", err);

	fz_write_data(ctx, out, writer->output, writer->output_size - writer->stream.avail_out);
}

fz_band_writer *fz_new_ps_band_writer(fz_context *ctx, fz_output *out)
{
	ps_band_writer *writer = fz_new_band_writer(ctx, ps_band_writer, out);

	writer->super.header = ps_write_header;
	writer->super.band = ps_write_band;
	writer->super.trailer = ps_write_trailer;
	writer->super.drop = ps_drop_band_writer;

	return &writer->super;
}

/* High-level document writer interface */

typedef struct fz_ps_writer_s fz_ps_writer;

struct fz_ps_writer_s
{
	fz_document_writer super;
	fz_draw_options draw;
	fz_pixmap *pixmap;
	fz_output *out;
	int count;
};

static fz_device *
ps_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_ps_writer *wri = (fz_ps_writer*)wri_;
	wri->count++;
	return fz_new_draw_device_with_options(ctx, &wri->draw, mediabox, &wri->pixmap);
}

static void
ps_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_ps_writer *wri = (fz_ps_writer*)wri_;
	fz_pixmap *pix = wri->pixmap;
	fz_band_writer *bw;

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		bw = fz_new_ps_band_writer(ctx, wri->out);
		fz_write_header(ctx, bw, pix->w, pix->h, pix->n, pix->alpha, pix->xres, pix->yres, 0, pix->colorspace, pix->seps);
		fz_write_band(ctx, bw, pix->stride, pix->h, pix->samples);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_band_writer(ctx, bw);
		fz_drop_pixmap(ctx, wri->pixmap);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
ps_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_ps_writer *wri = (fz_ps_writer*)wri_;
	fz_write_ps_file_trailer(ctx, wri->out, wri->count);
	fz_close_output(ctx, wri->out);
}

static void
ps_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_ps_writer *wri = (fz_ps_writer*)wri_;
	fz_drop_pixmap(ctx, wri->pixmap);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_ps_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_ps_writer *wri = fz_new_derived_document_writer(ctx, fz_ps_writer, ps_begin_page, ps_end_page, ps_close_writer, ps_drop_writer);

	fz_try(ctx)
	{
		fz_parse_draw_options(ctx, &wri->draw, options);
		wri->out = out;
		fz_write_ps_file_header(ctx, wri->out);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_ps_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.ps", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_ps_writer_with_output(ctx, out, options);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return wri;
}
