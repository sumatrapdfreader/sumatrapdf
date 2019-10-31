#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>

#define DPI 72.0f

typedef struct cbz_document_s cbz_document;
typedef struct cbz_page_s cbz_page;

static const char *cbz_ext_list[] = {
	".bmp",
	".gif",
	".hdp",
	".j2k",
	".jp2",
	".jpeg",
	".jpg",
	".jpx",
	".jxr",
	".pam",
	".pbm",
	".pgm",
	".pkm",
	".png",
	".pnm",
	".ppm",
	".tif",
	".tiff",
	".wdp",
	NULL
};

struct cbz_page_s
{
	fz_page super;
	fz_image *image;
};

struct cbz_document_s
{
	fz_document super;
	fz_archive *arch;
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
cbz_create_page_list(fz_context *ctx, cbz_document *doc)
{
	fz_archive *arch = doc->arch;
	int i, k, count;

	count = fz_count_archive_entries(ctx, arch);

	doc->page_count = 0;
	doc->page = fz_malloc_array(ctx, count, const char *);

	for (i = 0; i < count; i++)
	{
		const char *name = fz_list_archive_entry(ctx, arch, i);
		const char *ext = name ? strrchr(name, '.') : NULL;
		for (k = 0; cbz_ext_list[k]; k++)
		{
			if (ext && !fz_strcasecmp(ext, cbz_ext_list[k]))
			{
				doc->page[doc->page_count++] = name;
				break;
			}
		}
	}

	qsort((char **)doc->page, doc->page_count, sizeof *doc->page, cbz_compare_page_names);
}

static void
cbz_drop_document(fz_context *ctx, fz_document *doc_)
{
	cbz_document *doc = (cbz_document*)doc_;
	fz_drop_archive(ctx, doc->arch);
	fz_free(ctx, (char **)doc->page);
}

static int
cbz_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	cbz_document *doc = (cbz_document*)doc_;
	return doc->page_count;
}

static fz_rect
cbz_bound_page(fz_context *ctx, fz_page *page_)
{
	cbz_page *page = (cbz_page*)page_;
	fz_image *image = page->image;
	int xres, yres;
	fz_rect bbox;

	fz_image_resolution(image, &xres, &yres);
	bbox.x0 = bbox.y0 = 0;
	bbox.x1 = image->w * DPI / xres;
	bbox.y1 = image->h * DPI / yres;
	return bbox;
}

static void
cbz_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	cbz_page *page = (cbz_page*)page_;
	fz_matrix local_ctm;
	fz_image *image = page->image;
	int xres, yres;
	float w, h;

	fz_image_resolution(image, &xres, &yres);
	w = image->w * DPI / xres;
	h = image->h * DPI / yres;
	local_ctm = fz_pre_scale(ctm, w, h);
	fz_fill_image(ctx, dev, image, local_ctm, 1, fz_default_color_params);
}

static void
cbz_drop_page(fz_context *ctx, fz_page *page_)
{
	cbz_page *page = (cbz_page*)page_;
	fz_drop_image(ctx, page->image);
}

static fz_page *
cbz_load_page(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	cbz_document *doc = (cbz_document*)doc_;
	cbz_page *page = NULL;
	fz_buffer *buf = NULL;

	if (number < 0 || number >= doc->page_count)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot load page %d", number);

	fz_var(page);

	if (doc->arch)
		buf = fz_read_archive_entry(ctx, doc->arch, doc->page[number]);
	if (!buf)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot load cbz page");

	fz_try(ctx)
	{
		page = fz_new_derived_page(ctx, cbz_page);
		page->super.bound_page = cbz_bound_page;
		page->super.run_page_contents = cbz_run_page;
		page->super.drop_page = cbz_drop_page;
		page->image = fz_new_image_from_buffer(ctx, buf);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_drop_page(ctx, (fz_page*)page);
		fz_rethrow(ctx);
	}

	return (fz_page*)page;
}

static int
cbz_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, int size)
{
	cbz_document *doc = (cbz_document*)doc_;
	if (!strcmp(key, "format"))
		return (int) fz_strlcpy(buf, fz_archive_format(ctx, doc->arch), size);
	return -1;
}

static fz_document *
cbz_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	cbz_document *doc;

	doc = fz_new_derived_document(ctx, cbz_document);

	doc->super.drop_document = cbz_drop_document;
	doc->super.count_pages = cbz_count_pages;
	doc->super.load_page = cbz_load_page;
	doc->super.lookup_metadata = cbz_lookup_metadata;

	fz_try(ctx)
	{
		doc->arch = fz_open_archive_with_stream(ctx, file);
		cbz_create_page_list(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, (fz_document*)doc);
		fz_rethrow(ctx);
	}
	return (fz_document*)doc;
}

static const char *cbz_extensions[] =
{
	"cbt",
	"cbz",
	"tar",
	"zip",
	NULL
};

static const char *cbz_mimetypes[] =
{
	"application/vnd.comicbook+zip",
	"application/x-cbt",
	"application/x-cbz",
	"application/x-tar",
	"application/zip",
	NULL
};

fz_document_handler cbz_document_handler =
{
	NULL,
	NULL,
	cbz_open_document_with_stream,
	cbz_extensions,
	cbz_mimetypes,
	NULL,
	NULL
};
