#include "mupdf/img.h"

#include <ctype.h> /* for tolower */

#define DPI 72.0f

static void image_init_document(image_document *doc);

struct image_document_s
{
	fz_document super;

	fz_context *ctx;
	fz_stream *file;
	fz_image *image;
};

image_document *
image_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	image_document *doc;
	fz_buffer *buffer = NULL;

	doc = fz_malloc_struct(ctx, image_document);
	image_init_document(doc);
	doc->ctx = ctx;
	doc->file = fz_keep_stream(file);

	fz_var(buffer);

	fz_try(ctx)
	{
		buffer = fz_read_all(doc->file, 1024);
		doc->image = fz_new_image_from_buffer(ctx, buffer);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
	}
	fz_catch(ctx)
	{
		image_close_document(doc);
		fz_rethrow(ctx);
	}

	return doc;
}

image_document *
image_open_document(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	image_document *doc;

	file = fz_open_file(ctx, filename);
	if (!file)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));

	fz_try(ctx)
	{
		doc = image_open_document_with_stream(ctx, file);
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
image_close_document(image_document *doc)
{
	fz_context *ctx = doc->ctx;
	fz_drop_image(ctx, doc->image);
	fz_close(doc->file);
	fz_free(ctx, doc);
}

int
image_count_pages(image_document *doc)
{
	return 1;
}

image_page *
image_load_page(image_document *doc, int number)
{
	if (number != 0)
		return NULL;

	return (image_page *)doc->image;
}

void
image_free_page(image_document *doc, image_page *page)
{
}

fz_rect *
image_bound_page(image_document *doc, image_page *page, fz_rect *bbox)
{
	fz_image *image = (fz_image *)page;
	bbox->x0 = bbox->y0 = 0;
	bbox->x1 = image->w * DPI / image->xres;
	bbox->y1 = image->h * DPI / image->yres;
	return bbox;
}

void
image_run_page(image_document *doc, image_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	fz_matrix local_ctm = *ctm;
	fz_image *image = (fz_image *)page;
	float w = image->w * DPI / image->xres;
	float h = image->h * DPI / image->yres;
	fz_pre_scale(&local_ctm, w, h);
	fz_fill_image(dev, image, &local_ctm, 1);
}

static int
image_meta(image_document *doc, int key, void *ptr, int size)
{
	switch(key)
	{
	case FZ_META_FORMAT_INFO:
		sprintf((char *)ptr, "IMAGE");
		return FZ_META_OK;
	default:
		return FZ_META_UNKNOWN_KEY;
	}
}

static void
image_init_document(image_document *doc)
{
	doc->super.close = (void*)image_close_document;
	doc->super.count_pages = (void*)image_count_pages;
	doc->super.load_page = (void*)image_load_page;
	doc->super.bound_page = (void*)image_bound_page;
	doc->super.run_page_contents = (void*)image_run_page;
	doc->super.free_page = (void*)image_free_page;
	doc->super.meta = (void*)image_meta;
}
