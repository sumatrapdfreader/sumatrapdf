#include "mupdf/fitz.h"

#include <string.h>
#include <limits.h>

const char *fz_pclm_write_options_usage =
	"PCLm output options:\n"
	"\tcompression=none: No compression (default)\n"
	"\tcompression=flate: Flate compression\n"
	"\tstrip-height=N: Strip height (default 16)\n"
	"\n";

/*
	Parse PCLm options.

	Currently defined options and values are as follows:

		compression=none: No compression
		compression=flate: Flate compression
		strip-height=n: Strip height (default 16)
*/
fz_pclm_options *
fz_parse_pclm_options(fz_context *ctx, fz_pclm_options *opts, const char *args)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	if (fz_has_option(ctx, args, "compression", &val))
	{
		if (fz_option_eq(val, "none"))
			opts->compress = 0;
		else if (fz_option_eq(val, "flate"))
			opts->compress = 1;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported PCLm compression %s (none, or flate only)", val);
	}
	if (fz_has_option(ctx, args, "strip-height", &val))
	{
		int i = fz_atoi(val);
		if (i <= 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported PCLm strip height %d (suggest 16)", i);
		opts->strip_height = i;
	}

	return opts;
}

void
fz_write_pixmap_as_pclm(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pclm_options *pclm)
{
	fz_band_writer *writer;

	if (!pixmap || !out)
		return;

	writer = fz_new_pclm_band_writer(ctx, out, pclm);
	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, pixmap->xres, pixmap->yres, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

typedef struct pclm_band_writer_s
{
	fz_band_writer super;
	fz_pclm_options options;

	int obj_num;
	int xref_max;
	int64_t *xref;
	int pages;
	int page_max;
	int *page_obj;
	unsigned char *stripbuf;
	unsigned char *compbuf;
	size_t complen;
} pclm_band_writer;

static int
new_obj(fz_context *ctx, pclm_band_writer *writer)
{
	int64_t pos = fz_tell_output(ctx, writer->super.out);

	if (writer->obj_num >= writer->xref_max)
	{
		int new_max = writer->xref_max * 2;
		if (new_max < writer->obj_num + 8)
			new_max = writer->obj_num + 8;
		writer->xref = fz_realloc_array(ctx, writer->xref, new_max, int64_t);
		writer->xref_max = new_max;
	}

	writer->xref[writer->obj_num] = pos;

	return writer->obj_num++;
}

static void
pclm_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	pclm_band_writer *writer = (pclm_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int s = writer->super.s;
	int a = writer->super.alpha;
	int xres = writer->super.xres;
	int yres = writer->super.yres;
	int sh = writer->options.strip_height;
	int strips = (h + sh-1)/sh;
	int i;
	size_t len;
	unsigned char *data;
	fz_buffer *buf = NULL;

	if (a != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PCLm cannot write alpha channel");
	if (s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PCLm cannot write spot colors");
	if (n != 3 && n != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PCLm expected to be Grayscale or RGB");

	fz_free(ctx, writer->stripbuf);
	fz_free(ctx, writer->compbuf);
	writer->stripbuf = Memento_label(fz_malloc(ctx, w * sh * n), "pclm_stripbuf");
	writer->complen = fz_deflate_bound(ctx, w * sh * n);
	writer->compbuf = Memento_label(fz_malloc(ctx, writer->complen), "pclm_compbuf");

	/* Send the file header on the first page */
	if (writer->pages == 0)
		fz_write_string(ctx, out, "%PDF-1.4\n%PCLm-1.0\n");

	if (writer->page_max <= writer->pages)
	{
		int new_max = writer->page_max * 2;
		if (new_max == 0)
			new_max = writer->pages + 8;
		writer->page_obj = fz_realloc_array(ctx, writer->page_obj, new_max, int);
		writer->page_max = new_max;
	}
	writer->page_obj[writer->pages] = writer->obj_num;
	writer->pages++;

	/* Send the Page Object */
	fz_write_printf(ctx, out, "%d 0 obj\n<<\n/Type /Page\n/Parent 2 0 R\n/Resources <<\n/XObject <<\n", new_obj(ctx, writer));
	for (i = 0; i < strips; i++)
		fz_write_printf(ctx, out, "/Image%d %d 0 R\n", i, writer->obj_num + 1 + i);
	fz_write_printf(ctx, out, ">>\n>>\n/MediaBox[ 0 0 %g %g ]\n/Contents [ %d 0 R ]\n>>\nendobj\n",
		w * 72.0f / xres, h * 72.0f / yres, writer->obj_num);

	/* And the Page contents */
	/* We need the length to this, so write to a buffer first */
	fz_var(buf);
	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 0);
		fz_append_printf(ctx, buf, "%g 0 0 %g 0 0 cm\n", 72.0f/xres, 72.0f/yres);
		for (i = 0; i < strips; i++)
		{
			int at = h - (i+1)*sh;
			int this_sh = sh;
			if (at < 0)
			{
				this_sh += at;
				at = 0;
			}
			fz_append_printf(ctx, buf, "/P <</MCID 0>> BDC q\n%d 0 0 %d 0 %d cm\n/Image%d Do Q\n",
				w, this_sh, at, i);
		}
		len = fz_buffer_storage(ctx, buf, &data);
		fz_write_printf(ctx, out, "%d 0 obj\n<<\n/Length %zd\n>>\nstream\n", new_obj(ctx, writer), len);
		fz_write_data(ctx, out, data, len);
		fz_drop_buffer(ctx, buf);
		buf = NULL;
		fz_write_string(ctx, out, "\nendstream\nendobj\n");
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
}

static void
flush_strip(fz_context *ctx, pclm_band_writer *writer, int fill)
{
	unsigned char *data = writer->stripbuf;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int n = writer->super.n;
	size_t len = w*n*fill;

	/* Buffer is full, compress it and write it. */
	if (writer->options.compress)
	{
		size_t destLen = writer->complen;
		fz_deflate(ctx, writer->compbuf, &destLen, data, len, FZ_DEFLATE_DEFAULT);
		len = destLen;
		data = writer->compbuf;
	}
	fz_write_printf(ctx, out, "%d 0 obj\n<<\n/Width %d\n/ColorSpace /Device%s\n/Height %d\n%s/Subtype /Image\n",
		new_obj(ctx, writer), w, n == 1 ? "Gray" : "RGB", fill, writer->options.compress ? "/Filter /FlateDecode\n" : "");
	fz_write_printf(ctx, out, "/Length %zd\n/Type /XObject\n/BitsPerComponent 8\n>>\nstream\n", len);
	fz_write_data(ctx, out, data, len);
	fz_write_string(ctx, out, "\nendstream\nendobj\n");
}

static void
pclm_write_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *sp)
{
	pclm_band_writer *writer = (pclm_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int sh = writer->options.strip_height;
	int line;

	if (!out)
		return;

	for (line = 0; line < band_height; line++)
	{
		int dstline = (band_start+line) % sh;
		memcpy(writer->stripbuf + w*n*dstline, sp + line * w*n, w*n);
		if (dstline+1 == sh)
			flush_strip(ctx, writer, dstline+1);
	}

	if (band_start + band_height == h && h % sh != 0)
		flush_strip(ctx, writer, h % sh);
}

static void
pclm_write_trailer(fz_context *ctx, fz_band_writer *writer_)
{
}

static void
pclm_drop_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	pclm_band_writer *writer = (pclm_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int i;

	/* We actually do the trailer writing in the drop */
	if (writer->xref_max > 2)
	{
		int64_t t_pos;

		/* Catalog */
		writer->xref[1] = fz_tell_output(ctx, out);
		fz_write_printf(ctx, out, "1 0 obj\n<<\n/Type /Catalog\n/Pages 2 0 R\n>>\nendobj\n");

		/* Page table */
		writer->xref[2] = fz_tell_output(ctx, out);
		fz_write_printf(ctx, out, "2 0 obj\n<<\n/Count %d\n/Kids [ ", writer->pages);

		for (i = 0; i < writer->pages; i++)
			fz_write_printf(ctx, out, "%d 0 R ", writer->page_obj[i]);
		fz_write_string(ctx, out, "]\n/Type /Pages\n>>\nendobj\n");

		/* Xref */
		t_pos = fz_tell_output(ctx, out);
		fz_write_printf(ctx, out, "xref\n0 %d\n0000000000 65535 f \n", writer->obj_num);
		for (i = 1; i < writer->obj_num; i++)
			fz_write_printf(ctx, out, "%010zd 00000 n \n", writer->xref[i]);
		fz_write_printf(ctx, out, "trailer\n<<\n/Size %d\n/Root 1 0 R\n>>\nstartxref\n%ld\n%%%%EOF\n", writer->obj_num, t_pos);
	}

	fz_free(ctx, writer->stripbuf);
	fz_free(ctx, writer->compbuf);
	fz_free(ctx, writer->page_obj);
	fz_free(ctx, writer->xref);
}

fz_band_writer *fz_new_pclm_band_writer(fz_context *ctx, fz_output *out, const fz_pclm_options *options)
{
	pclm_band_writer *writer = fz_new_band_writer(ctx, pclm_band_writer, out);

	writer->super.header = pclm_write_header;
	writer->super.band = pclm_write_band;
	writer->super.trailer = pclm_write_trailer;
	writer->super.drop = pclm_drop_band_writer;

	if (options)
		writer->options = *options;
	else
		memset(&writer->options, 0, sizeof(writer->options));

	if (writer->options.strip_height == 0)
		writer->options.strip_height = 16;
	writer->obj_num = 3; /* 1 reserved for catalog, 2 for pages tree. */

	return &writer->super;
}

void
fz_save_pixmap_as_pclm(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pclm_options *pclm)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		fz_write_pixmap_as_pclm(ctx, out, pixmap, pclm);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* High-level document writer interface */

typedef struct fz_pclm_writer_s fz_pclm_writer;

struct fz_pclm_writer_s
{
	fz_document_writer super;
	fz_draw_options draw;
	fz_pclm_options pclm;
	fz_pixmap *pixmap;
	fz_band_writer *bander;
	fz_output *out;
	int pagenum;
};

static fz_device *
pclm_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_pclm_writer *wri = (fz_pclm_writer*)wri_;
	return fz_new_draw_device_with_options(ctx, &wri->draw, mediabox, &wri->pixmap);
}

static void
pclm_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_pclm_writer *wri = (fz_pclm_writer*)wri_;
	fz_pixmap *pix = wri->pixmap;

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		fz_write_header(ctx, wri->bander, pix->w, pix->h, pix->n, pix->alpha, pix->xres, pix->yres, wri->pagenum++, pix->colorspace, pix->seps);
		fz_write_band(ctx, wri->bander, pix->stride, pix->h, pix->samples);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_pixmap(ctx, pix);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pclm_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pclm_writer *wri = (fz_pclm_writer*)wri_;

	fz_drop_band_writer(ctx, wri->bander);
	wri->bander = NULL;

	fz_close_output(ctx, wri->out);
}

static void
pclm_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pclm_writer *wri = (fz_pclm_writer*)wri_;

	fz_drop_pixmap(ctx, wri->pixmap);
	fz_drop_output(ctx, wri->out);
	fz_drop_band_writer(ctx, wri->bander);
}

fz_document_writer *
fz_new_pclm_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_pclm_writer *wri = fz_new_derived_document_writer(ctx, fz_pclm_writer, pclm_begin_page, pclm_end_page, pclm_close_writer, pclm_drop_writer);

	fz_try(ctx)
	{
		fz_parse_draw_options(ctx, &wri->draw, options);
		fz_parse_pclm_options(ctx, &wri->pclm, options);
		wri->out = out;
		wri->bander = fz_new_pclm_band_writer(ctx, wri->out, &wri->pclm);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_pclm_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.pclm", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_pclm_writer_with_output(ctx, out, options);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return wri;
}
