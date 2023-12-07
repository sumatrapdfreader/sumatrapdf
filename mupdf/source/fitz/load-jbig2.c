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

#include "pixmap-imp.h"

#include <jbig2.h>

struct info
{
	int width, height;
	int xres, yres;
	int pages;
	fz_colorspace *cspace;
};

struct fz_jbig2_allocator
{
	Jbig2Allocator super;
	fz_context *ctx;
};

static void
error_callback(void *data, const char *msg, Jbig2Severity severity, uint32_t seg_idx)
{
	fz_context *ctx = data;
	if (severity == JBIG2_SEVERITY_FATAL)
		fz_warn(ctx, "jbig2dec error: %s (segment %u)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_WARNING)
		fz_warn(ctx, "jbig2dec warning: %s (segment %u)", msg, seg_idx);
#ifdef JBIG2_DEBUG
	else if (severity == JBIG2_SEVERITY_INFO)
		fz_warn(ctx, "jbig2dec info: %s (segment %u)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_DEBUG)
		fz_warn(ctx, "jbig2dec debug: %s (segment %u)", msg, seg_idx);
#endif
}

static void *fz_jbig2_alloc(Jbig2Allocator *allocator, size_t size)
{
	fz_context *ctx = ((struct fz_jbig2_allocator *) allocator)->ctx;
	return fz_malloc_no_throw(ctx, size);
}

static void fz_jbig2_free(Jbig2Allocator *allocator, void *p)
{
	fz_context *ctx = ((struct fz_jbig2_allocator *) allocator)->ctx;
	fz_free(ctx, p);
}

static void *fz_jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
	fz_context *ctx = ((struct fz_jbig2_allocator *) allocator)->ctx;
	if (size == 0)
	{
		fz_free(ctx, p);
		return NULL;
	}
	if (p == NULL)
		return Memento_label(fz_malloc(ctx, size), "jbig2_realloc");
	return Memento_label(fz_realloc_no_throw(ctx, p, size), "jbig2_realloc");
}

static fz_pixmap *
jbig2_read_image(fz_context *ctx, struct info *jbig2, const unsigned char *buf, size_t len, int only_metadata, int subimage)
{
	Jbig2Ctx *jctx = NULL;
	Jbig2Image *page = NULL;
	struct fz_jbig2_allocator allocator;
	fz_pixmap *pix = NULL;

	allocator.super.alloc = fz_jbig2_alloc;
	allocator.super.free = fz_jbig2_free;
	allocator.super.realloc = fz_jbig2_realloc;
	allocator.ctx = ctx;

	fz_var(jctx);
	fz_var(page);
	fz_var(pix);

	fz_try(ctx)
	{
		jctx = jbig2_ctx_new((Jbig2Allocator *) &allocator, 0, NULL, error_callback, ctx);
		if (jctx == NULL)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot create jbig2 context");
		if (jbig2_data_in(jctx, buf, len) < 0)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot decode jbig2 image");
		if (jbig2_complete_page(jctx) < 0)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot complete jbig2 image");

		if (only_metadata && subimage < 0)
		{
			while ((page = jbig2_page_out(jctx)) != NULL)
			{
				jbig2_release_page(jctx, page);
				jbig2->pages++;
			}
		}
		else if (only_metadata && subimage >= 0)
		{
			while ((page = jbig2_page_out(jctx)) != NULL && subimage > 0)
			{
				jbig2_release_page(jctx, page);
				subimage--;
			}

			if (page == NULL)
				fz_throw(ctx, FZ_ERROR_LIBRARY, "no jbig2 image decoded");

			jbig2->cspace = fz_device_gray(ctx);
			jbig2->width = page->width;
			jbig2->height = page->height;
			jbig2->xres = 72;
			jbig2->yres = 72;
		}
		else if (subimage >= 0)
		{
			while ((page = jbig2_page_out(jctx)) != NULL && subimage > 0)
			{
				jbig2_release_page(jctx, page);
				subimage--;
			}

			if (page == NULL)
				fz_throw(ctx, FZ_ERROR_LIBRARY, "no jbig2 image decoded");

			jbig2->cspace = fz_device_gray(ctx);
			jbig2->width = page->width;
			jbig2->height = page->height;
			jbig2->xres = 72;
			jbig2->yres = 72;

			pix = fz_new_pixmap(ctx, jbig2->cspace, jbig2->width, jbig2->height, NULL, 0);
			fz_unpack_tile(ctx, pix, page->data, 1, 1, page->stride, 0);
			fz_invert_pixmap(ctx, pix);
		}
	}
	fz_always(ctx)
	{
		jbig2_release_page(jctx, page);
		jbig2_ctx_free(jctx);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

int
fz_load_jbig2_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len)
{
	struct info jbig2 = { 0 };
	int subimage_count = 0;

	fz_try(ctx)
	{
		jbig2_read_image(ctx, &jbig2, buf, len, 1, -1);
		subimage_count = jbig2.pages;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return subimage_count;
}

void
fz_load_jbig2_info_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep, int subimage)
{
	struct info jbig2 = { 0 };

	jbig2_read_image(ctx, &jbig2, buf, len, 1, subimage);
	*cspacep = fz_keep_colorspace(ctx, jbig2.cspace);
	*wp = jbig2.width;
	*hp = jbig2.height;
	*xresp = jbig2.xres;
	*yresp = jbig2.yres;
}

fz_pixmap *
fz_load_jbig2_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage)
{
	struct info jbig2 = { 0 };
	return jbig2_read_image(ctx, &jbig2, buf, len, 0, subimage);
}

fz_pixmap *
fz_load_jbig2(fz_context *ctx, const unsigned char *buf, size_t len)
{
	return fz_load_jbig2_subimage(ctx, buf, len, 0);
}

void
fz_load_jbig2_info(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_load_jbig2_info_subimage(ctx, buf, len, wp, hp, xresp, yresp, cspacep, 0);
}
