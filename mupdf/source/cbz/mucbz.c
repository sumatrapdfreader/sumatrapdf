#include "mupdf/cbz.h"

#define DPI 72.0f

static void cbz_init_document(cbz_document *doc);

static const char *cbz_ext_list[] = {
	".jpg", ".jpeg", ".png",
	".JPG", ".JPEG", ".PNG",
	NULL
};

struct cbz_page_s
{
	fz_image *image;
};

struct cbz_document_s
{
	fz_document super;
	fz_context *ctx;
	fz_archive *zip;
	int page_count;
	const char **page;
};

static inline int cbz_isdigit(int c)
{
	return c >= '0' && c <= '9';
}

static inline int cbz_toupper(int c)
{
	if (c >= 'a' && c <= 'z')
		return c - 'a' + 'A';
	return c;
}

static inline int
cbz_strnatcmp(const char *a, const char *b)
{
	int x, y;

	while (*a || *b)
	{
		if (cbz_isdigit(*a) && cbz_isdigit(*b))
		{
			x = *a++ - '0';
			while (cbz_isdigit(*a))
				x = x * 10 + *a++ - '0';
			y = *b++ - '0';
			while (cbz_isdigit(*b))
				y = y * 10 + *b++ - '0';
		}
		else
		{
			x = cbz_toupper(*a++);
			y = cbz_toupper(*b++);
		}
		if (x < y)
			return -1;
		if (x > y)
			return 1;
	}

	return 0;
}

static int
cbz_compare_page_names(const void *a, const void *b)
{
	return cbz_strnatcmp(*(const char **)a, *(const char **)b);
}

static void
cbz_create_page_list(cbz_document *doc)
{
	fz_context *ctx = doc->ctx;
	fz_archive *zip = doc->zip;
	int i, k, count;

	count = fz_count_archive_entries(ctx, zip);

	doc->page_count = 0;
	doc->page = fz_malloc_array(ctx, count, sizeof *doc->page);

	for (i = 0; i < count; i++)
	{
		for (k = 0; cbz_ext_list[k]; k++)
		{
			const char *name = fz_list_archive_entry(ctx, zip, i);
			if (strstr(name, cbz_ext_list[k]))
			{
				doc->page[doc->page_count++] = name;
printf("found page %d = '%s'\n", i, name);
				break;
			}
		}
	}

	qsort((char **)doc->page, doc->page_count, sizeof *doc->page, cbz_compare_page_names);

	for (i = 0; i < doc->page_count; ++i)
		printf("  %d = %s\n", i, doc->page[i]);
}

cbz_document *
cbz_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	cbz_document *doc;

	doc = fz_malloc_struct(ctx, cbz_document);
	cbz_init_document(doc);
	doc->ctx = ctx;
	doc->page_count = 0;
	doc->page = NULL;

	fz_try(ctx)
	{
		doc->zip = fz_open_archive_with_stream(ctx, file);
		cbz_create_page_list(doc);
	}
	fz_catch(ctx)
	{
		cbz_close_document(doc);
		fz_rethrow(ctx);
	}

	return doc;
}

cbz_document *
cbz_open_document(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	cbz_document *doc;

	file = fz_open_file(ctx, filename);
	if (!file)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));

	fz_try(ctx)
	{
		doc = cbz_open_document_with_stream(ctx, file);
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
cbz_close_document(cbz_document *doc)
{
	fz_close_archive(doc->ctx, doc->zip);
	fz_free(doc->ctx, (char **)doc->page);
	fz_free(doc->ctx, doc);
}

int
cbz_count_pages(cbz_document *doc)
{
	return doc->page_count;
}

cbz_page *
cbz_load_page(cbz_document *doc, int number)
{
	fz_context *ctx = doc->ctx;
	unsigned char *data = NULL;
	cbz_page *page = NULL;
	fz_buffer *buf;

	if (number < 0 || number >= doc->page_count)
		return NULL;

	fz_var(data);
	fz_var(page);

	buf = fz_read_archive_entry(doc->ctx, doc->zip, doc->page[number]);
	fz_try(ctx)
	{
		page = fz_malloc_struct(ctx, cbz_page);
		page->image = fz_new_image_from_buffer(ctx, buf);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(doc->ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, data);
		cbz_free_page(doc, page);
		fz_rethrow(ctx);
	}

	return page;
}

void
cbz_free_page(cbz_document *doc, cbz_page *page)
{
	if (!page)
		return;
	fz_drop_image(doc->ctx, page->image);
	fz_free(doc->ctx, page);
}

fz_rect *
cbz_bound_page(cbz_document *doc, cbz_page *page, fz_rect *bbox)
{
	fz_image *image = page->image;
	bbox->x0 = bbox->y0 = 0;
	bbox->x1 = image->w * DPI / image->xres;
	bbox->y1 = image->h * DPI / image->yres;
	return bbox;
}

void
cbz_run_page(cbz_document *doc, cbz_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	fz_matrix local_ctm = *ctm;
	fz_image *image = page->image;
	float w = image->w * DPI / image->xres;
	float h = image->h * DPI / image->yres;
	fz_pre_scale(&local_ctm, w, h);
	fz_fill_image(dev, image, &local_ctm, 1);
}

static int
cbz_meta(cbz_document *doc, int key, void *ptr, int size)
{
	switch (key)
	{
	case FZ_META_FORMAT_INFO:
		sprintf((char *)ptr, "CBZ");
		return FZ_META_OK;
	default:
		return FZ_META_UNKNOWN_KEY;
	}
}

static void
cbz_rebind(cbz_document *doc, fz_context *ctx)
{
	doc->ctx = ctx;
	fz_rebind_archive(doc->zip, ctx);
}

static void
cbz_init_document(cbz_document *doc)
{
	doc->super.close = (fz_document_close_fn *)cbz_close_document;
	doc->super.count_pages = (fz_document_count_pages_fn *)cbz_count_pages;
	doc->super.load_page = (fz_document_load_page_fn *)cbz_load_page;
	doc->super.bound_page = (fz_document_bound_page_fn *)cbz_bound_page;
	doc->super.run_page_contents = (fz_document_run_page_contents_fn *)cbz_run_page;
	doc->super.free_page = (fz_document_free_page_fn *)cbz_free_page;
	doc->super.meta = (fz_document_meta_fn *)cbz_meta;
	doc->super.rebind = (fz_document_rebind_fn *)cbz_rebind;
}

static int
cbz_recognize(fz_context *doc, const char *magic)
{
	char *ext = strrchr(magic, '.');

	if (ext)
	{
		if (!fz_strcasecmp(ext, ".cbz") || !fz_strcasecmp(ext, ".zip"))
			return 100;
	}
	if (!strcmp(magic, "cbz") || !strcmp(magic, "application/x-cbz"))
		return 100;

	return 0;
}

fz_document_handler cbz_document_handler =
{
	(fz_document_recognize_fn *)&cbz_recognize,
	(fz_document_open_fn *)&cbz_open_document,
	(fz_document_open_with_stream_fn *)&cbz_open_document_with_stream
};
