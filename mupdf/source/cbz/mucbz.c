// Copyright (C) 2004-2024 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>

#define DPI 72.0f

static const char *cbz_ext_list[] = {
	".bmp",
	".gif",
	".hdp",
	".j2k",
	".jb2",
	".jbig2",
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

typedef struct
{
	fz_page super;
	fz_image *image;
} cbz_page;

typedef struct
{
	fz_document super;
	fz_archive *arch;
	int page_count;
	const char **page;
} cbz_document;

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
cbz_bound_page(fz_context *ctx, fz_page *page_, fz_box_type box)
{
	cbz_page *page = (cbz_page*)page_;
	fz_image *image = page->image;
	int xres, yres;
	fz_rect bbox = fz_empty_rect;
	uint8_t orientation;

	if (image)
	{
		fz_image_resolution(image, &xres, &yres);
		bbox.x0 = bbox.y0 = 0;
		orientation = fz_image_orientation(ctx, image);
		if (orientation == 0 || (orientation & 1) == 1)
		{
			bbox.x1 = image->w * DPI / xres;
			bbox.y1 = image->h * DPI / yres;
		}
		else
		{
			bbox.y1 = image->w * DPI / xres;
			bbox.x1 = image->h * DPI / yres;
		}
	}
	return bbox;
}

static void
cbz_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	cbz_page *page = (cbz_page*)page_;
	fz_image *image = page->image;
	int xres, yres;
	float w, h;
	uint8_t orientation;
	fz_matrix immat;

	if (image)
	{
		fz_try(ctx)
		{
			fz_image_resolution(image, &xres, &yres);
			orientation = fz_image_orientation(ctx, image);
			if (orientation == 0 || (orientation & 1) == 1)
			{
				w = image->w * DPI / xres;
				h = image->h * DPI / yres;
			}
			else
			{
				h = image->w * DPI / xres;
				w = image->h * DPI / yres;
			}
			immat = fz_image_orientation_matrix(ctx, image);
			immat = fz_post_scale(immat, w, h);
			ctm = fz_concat(immat, ctm);
			fz_fill_image(ctx, dev, image, ctm, 1, fz_default_color_params);
		}
		fz_catch(ctx)
		{
			fz_report_error(ctx);
			fz_warn(ctx, "cannot render image on page");
		}
	}
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
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid page number %d", number);

	fz_var(page);

	page = fz_new_derived_page(ctx, cbz_page, doc_);
	page->super.bound_page = cbz_bound_page;
	page->super.run_page_contents = cbz_run_page;
	page->super.drop_page = cbz_drop_page;

	fz_try(ctx)
	{
		buf = fz_read_archive_entry(ctx, doc->arch, doc->page[number]);
		page->image = fz_new_image_from_buffer(ctx, buf);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fz_warn(ctx, "cannot decode image on page, leaving it blank");
	}

	return (fz_page*)page;
}

static int
cbz_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, size_t size)
{
	cbz_document *doc = (cbz_document*)doc_;
	if (!strcmp(key, FZ_META_FORMAT))
		return 1 + (int) fz_strlcpy(buf, fz_archive_format(ctx, doc->arch), size);
	return -1;
}

static fz_document *
cbz_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *dir, void *state)
{
	cbz_document *doc = fz_new_derived_document(ctx, cbz_document);

	doc->super.drop_document = cbz_drop_document;
	doc->super.count_pages = cbz_count_pages;
	doc->super.load_page = cbz_load_page;
	doc->super.lookup_metadata = cbz_lookup_metadata;

	fz_try(ctx)
	{
		if (file)
			doc->arch = fz_open_archive_with_stream(ctx, file);
		else
			doc->arch = fz_keep_archive(ctx, dir);
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
#ifdef HAVE_LIBARCHIVE
	"cbr",
#endif
	"cbt",
	"cbz",
	"tar",
	"zip",
	NULL
};

static const char *cbz_mimetypes[] =
{
#ifdef HAVE_LIBARCHIVE
	"application/vnd.comicbook-rar",
#endif
	"application/vnd.comicbook+zip",
#ifdef HAVE_LIBARCHIVE
	"application/x-cbr",
#endif
	"application/x-cbt",
	"application/x-cbz",
	"application/x-tar",
	"application/zip",
	NULL
};

static int
cbz_recognize_doc_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **state, fz_document_recognize_state_free_fn **freestate)
{
	fz_archive *arch = NULL;
	int ret = 0;
	int i, k, count;

	fz_var(arch);
	fz_var(ret);

	fz_try(ctx)
	{
		if (stream == NULL)
			arch = fz_keep_archive(ctx, dir);
		else
		{
			arch = fz_try_open_archive_with_stream(ctx, stream);
			if (arch == NULL)
				break;
		}

		/* If it's an archive, and we can find at least one plausible page
		 * then we can open it as a cbz. */
		count = fz_count_archive_entries(ctx, arch);
		for (i = 0; i < count && ret == 0; i++)
		{
			const char *name = fz_list_archive_entry(ctx, arch, i);
			const char *ext;
			if (name == NULL)
				continue;
			ext = strrchr(name, '.');
			if (ext)
			{
				for (k = 0; cbz_ext_list[k]; k++)
				{
					if (!fz_strcasecmp(ext, cbz_ext_list[k]))
					{
						ret = 25;
						break;
					}
				}
			}
		}
	}
	fz_always(ctx)
		fz_drop_archive(ctx, arch);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_document_handler cbz_document_handler =
{
	NULL,
	cbz_open_document,
	cbz_extensions,
	cbz_mimetypes,
	cbz_recognize_doc_content
};
