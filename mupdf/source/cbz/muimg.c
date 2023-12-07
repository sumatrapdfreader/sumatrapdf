// Copyright (C) 2004-2021 Artifex Software, Inc.
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

#define DPI 72.0f

typedef struct
{
	fz_page super;
	fz_image *image;
} img_page;

typedef struct
{
	fz_document super;
	fz_buffer *buffer;
	const char *format;
	int page_count;
	fz_pixmap *(*load_subimage)(fz_context *ctx, const unsigned char *p, size_t total, int subimage);
} img_document;

static void
img_drop_document(fz_context *ctx, fz_document *doc_)
{
	img_document *doc = (img_document*)doc_;
	fz_drop_buffer(ctx, doc->buffer);
}

static int
img_count_pages(fz_context *ctx, fz_document *doc_, int chapter)
{
	img_document *doc = (img_document*)doc_;
	return doc->page_count;
}

static fz_rect
img_bound_page(fz_context *ctx, fz_page *page_, fz_box_type box)
{
	img_page *page = (img_page*)page_;
	fz_image *image = page->image;
	int xres, yres;
	fz_rect bbox;
	uint8_t orientation = fz_image_orientation(ctx, page->image);

	fz_image_resolution(image, &xres, &yres);
	bbox.x0 = bbox.y0 = 0;
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
	return bbox;
}

static void
img_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	img_page *page = (img_page*)page_;
	fz_image *image = page->image;
	int xres, yres;
	float w, h;
	uint8_t orientation = fz_image_orientation(ctx, page->image);
	fz_matrix immat = fz_image_orientation_matrix(ctx, page->image);

	fz_image_resolution(image, &xres, &yres);
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
	immat = fz_post_scale(immat, w, h);
	ctm = fz_concat(immat, ctm);
	fz_fill_image(ctx, dev, image, ctm, 1, fz_default_color_params);
}

static void
img_drop_page(fz_context *ctx, fz_page *page_)
{
	img_page *page = (img_page*)page_;
	fz_drop_image(ctx, page->image);
}

static fz_page *
img_load_page(fz_context *ctx, fz_document *doc_, int chapter, int number)
{
	img_document *doc = (img_document*)doc_;
	fz_pixmap *pixmap = NULL;
	fz_image *image = NULL;
	img_page *page = NULL;

	if (number < 0 || number >= doc->page_count)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid page number %d", number);

	fz_var(pixmap);
	fz_var(image);
	fz_var(page);

	fz_try(ctx)
	{
		if (doc->load_subimage)
		{
			size_t len;
			unsigned char *data;
			len = fz_buffer_storage(ctx, doc->buffer, &data);
			pixmap = doc->load_subimage(ctx, data, len, number);
			image = fz_new_image_from_pixmap(ctx, pixmap, NULL);
		}
		else
		{
			image = fz_new_image_from_buffer(ctx, doc->buffer);
		}

		page = fz_new_derived_page(ctx, img_page, doc_);
		page->super.bound_page = img_bound_page;
		page->super.run_page_contents = img_run_page;
		page->super.drop_page = img_drop_page;
		page->image = fz_keep_image(ctx, image);
	}
	fz_always(ctx)
	{
		fz_drop_image(ctx, image);
		fz_drop_pixmap(ctx, pixmap);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, page);
		fz_rethrow(ctx);
	}

	return (fz_page*)page;
}

static int
img_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, size_t size)
{
	img_document *doc = (img_document*)doc_;
	if (!strcmp(key, FZ_META_FORMAT))
		return 1 + (int)fz_strlcpy(buf, doc->format, size);
	return -1;
}

static fz_document *
img_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	img_document *doc = NULL;

	doc = fz_new_derived_document(ctx, img_document);

	doc->super.drop_document = img_drop_document;
	doc->super.count_pages = img_count_pages;
	doc->super.load_page = img_load_page;
	doc->super.lookup_metadata = img_lookup_metadata;

	fz_try(ctx)
	{
		int fmt;
		size_t len;
		unsigned char *data;

		doc->buffer = fz_read_all(ctx, file, 0);
		len = fz_buffer_storage(ctx, doc->buffer, &data);

		fmt = FZ_IMAGE_UNKNOWN;
		if (len >= 8)
			fmt = fz_recognize_image_format(ctx, data);
		if (fmt == FZ_IMAGE_TIFF)
		{
			doc->page_count = fz_load_tiff_subimage_count(ctx, data, len);
			doc->load_subimage = fz_load_tiff_subimage;
			doc->format = "TIFF";
		}
		else if (fmt == FZ_IMAGE_PNM)
		{
			doc->page_count = fz_load_pnm_subimage_count(ctx, data, len);
			doc->load_subimage = fz_load_pnm_subimage;
			doc->format = "PNM";
		}
		else if (fmt == FZ_IMAGE_JBIG2)
		{
			doc->page_count = fz_load_jbig2_subimage_count(ctx, data, len);
			if (doc->page_count > 1)
				doc->load_subimage = fz_load_jbig2_subimage;
			doc->format = "JBIG2";
		}
		else if (fmt == FZ_IMAGE_BMP)
		{
			doc->page_count = fz_load_bmp_subimage_count(ctx, data, len);
			doc->load_subimage = fz_load_bmp_subimage;
			doc->format = "BMP";
		}
		else
		{
			doc->page_count = 1;
			doc->format = "Image";
		}
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, (fz_document*)doc);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

static int
img_recognize_content(fz_context *ctx, fz_stream *stream)
{
	unsigned char data[8];
	size_t n = fz_read(ctx, stream, data, 8);
	int fmt;

	if (n != 8)
		return 0;

	fmt = fz_recognize_image_format(ctx, data);
	if (fmt != FZ_IMAGE_UNKNOWN)
		return 100;

	return 0;
}

static const char *img_extensions[] =
{
	"bmp",
	"gif",
	"hdp",
	"j2k",
	"jb2",
	"jbig2",
	"jfif",
	"jfif-tbnl",
	"jp2",
	"jpe",
	"jpeg",
	"jpg",
	"jpx",
	"jxr",
	"pam",
	"pbm",
	"pfm",
	"pgm",
	"pkm",
	"png",
	"pnm",
	"ppm",
	"psd",
	"tif",
	"tiff",
	"wdp",
	NULL
};

static const char *img_mimetypes[] =
{
	"image/bmp",
	"image/gif",
	"image/jp2",
	"image/jpeg",
	"image/jpx",
	"image/jxr",
	"image/pjpeg",
	"image/png",
	"image/tiff",
	"image/vnd.ms-photo",
	"image/vnd.adobe.photoshop",
	"image/x-jb2",
	"image/x-jbig2",
	"image/x-portable-anymap",
	"image/x-portable-arbitrarymap",
	"image/x-portable-bitmap",
	"image/x-portable-greymap",
	"image/x-portable-pixmap",
	"image/x-portable-floatmap",
	"image/x-tiff",
	NULL
};

fz_document_handler img_document_handler =
{
	NULL,
	NULL,
	img_open_document_with_stream,
	img_extensions,
	img_mimetypes,
	NULL,
	NULL,
	img_recognize_content
};
