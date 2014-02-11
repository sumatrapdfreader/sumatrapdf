#include "mupdf/tiff.h"

static void tiff_init_document(tiff_document *doc);

#define DPI 72.0f

struct tiff_page_s
{
	fz_image *image;
};

struct tiff_document_s
{
	fz_document super;

	fz_context *ctx;
	fz_stream *file;
	fz_buffer *buffer;
	int page_count;
};

tiff_document *
tiff_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	tiff_document *doc;
	int len;
	unsigned char *buf;

	doc = fz_malloc_struct(ctx, tiff_document);
	tiff_init_document(doc);
	doc->ctx = ctx;
	doc->file = fz_keep_stream(file);
	doc->page_count = 0;

	fz_try(ctx)
	{
		doc->buffer = fz_read_all(doc->file, 1024);
		len = doc->buffer->len;
		buf = doc->buffer->data;

		doc->page_count = fz_load_tiff_subimage_count(ctx, buf, len);
	}
	fz_catch(ctx)
	{
		tiff_close_document(doc);
		fz_rethrow(ctx);
	}

	return doc;
}

tiff_document *
tiff_open_document(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	tiff_document *doc;

	file = fz_open_file(ctx, filename);
	if (!file)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));

	fz_try(ctx)
	{
		doc = tiff_open_document_with_stream(ctx, file);
	}
	fz_always(ctx)
	{
		fz_close(file);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return doc;
}

void
tiff_close_document(tiff_document *doc)
{
	fz_context *ctx = doc->ctx;
	fz_drop_buffer(ctx, doc->buffer);
	fz_close(doc->file);
	fz_free(ctx, doc);
}

int
tiff_count_pages(tiff_document *doc)
{
	return doc->page_count;
}

tiff_page *
tiff_load_page(tiff_document *doc, int number)
{
	fz_context *ctx = doc->ctx;
	fz_image *mask = NULL;
	fz_pixmap *pixmap = NULL;
	tiff_page *page = NULL;

	if (number < 0 || number >= doc->page_count)
		return NULL;

	fz_var(pixmap);
	fz_var(page);
	fz_try(ctx)
	{
		pixmap = fz_load_tiff_subimage(ctx, doc->buffer->data, doc->buffer->len, number);

		page = fz_malloc_struct(ctx, tiff_page);
		page->image = fz_new_image_from_pixmap(ctx, pixmap, mask);
	}
	fz_catch(ctx)
	{
		tiff_free_page(doc, page);
		fz_rethrow(ctx);
	}

	return page;
}

void
tiff_free_page(tiff_document *doc, tiff_page *page)
{
	if (!page)
		return;
	fz_drop_image(doc->ctx, page->image);
	fz_free(doc->ctx, page);
}

fz_rect *
tiff_bound_page(tiff_document *doc, tiff_page *page, fz_rect *bbox)
{
	fz_image *image = page->image;
	bbox->x0 = bbox->y0 = 0;
	bbox->x1 = image->w * DPI / image->xres;
	bbox->y1 = image->h * DPI / image->yres;
	return bbox;
}

void
tiff_run_page(tiff_document *doc, tiff_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	fz_matrix local_ctm = *ctm;
	fz_image *image = page->image;
	float w = image->w * DPI / image->xres;
	float h = image->h * DPI / image->yres;
	fz_pre_scale(&local_ctm, w, h);
	fz_fill_image(dev, image, &local_ctm, 1);
}

static int
tiff_meta(tiff_document *doc, int key, void *ptr, int size)
{
	switch (key)
	{
	case FZ_META_FORMAT_INFO:
		sprintf((char *)ptr, "TIFF");
		return FZ_META_OK;
	default:
		return FZ_META_UNKNOWN_KEY;
	}
}

static void
tiff_rebind(tiff_document *doc, fz_context *ctx)
{
	doc->ctx = ctx;
	fz_rebind_stream(doc->file, ctx);
}

static void
tiff_init_document(tiff_document *doc)
{
	doc->super.close = (fz_document_close_fn *)tiff_close_document;
	doc->super.count_pages = (fz_document_count_pages_fn *)tiff_count_pages;
	doc->super.load_page = (fz_document_load_page_fn *)tiff_load_page;
	doc->super.bound_page = (fz_document_bound_page_fn *)tiff_bound_page;
	doc->super.run_page_contents = (fz_document_run_page_contents_fn *)tiff_run_page;
	doc->super.free_page = (fz_document_free_page_fn *)tiff_free_page;
	doc->super.meta = (fz_document_meta_fn *)tiff_meta;
	doc->super.rebind = (fz_document_rebind_fn *)tiff_rebind;
}

static int
tiff_recognize(fz_context *doc, const char *magic)
{
	char *ext = strrchr(magic, '.');

	if (ext)
	{
		if (!fz_strcasecmp(ext, ".tiff") || !fz_strcasecmp(ext, ".tif"))
			return 100;
	}
	if (!strcmp(magic, "tif") || !strcmp(magic, "image/tiff") ||
		!strcmp(magic, "tiff") || !strcmp(magic, "image/x-tiff"))
		return 100;

	return 0;
}

fz_document_handler tiff_document_handler =
{
	(fz_document_recognize_fn *)&tiff_recognize,
	(fz_document_open_fn *)&tiff_open_document,
	(fz_document_open_with_stream_fn *)&tiff_open_document_with_stream
};
