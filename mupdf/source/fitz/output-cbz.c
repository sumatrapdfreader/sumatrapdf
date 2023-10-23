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

#include <zlib.h>

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct
{
	fz_document_writer super;
	fz_draw_options options;
	fz_pixmap *pixmap;
	int count;
	fz_zip_writer *zip;
} fz_cbz_writer;

static fz_device *
cbz_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	return fz_new_draw_device_with_options(ctx, &wri->options, mediabox, &wri->pixmap);
}

static void
cbz_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	fz_buffer *buffer = NULL;
	char name[40];

	fz_var(buffer);

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		wri->count += 1;
		fz_snprintf(name, sizeof name, "p%04d.png", wri->count);
		buffer = fz_new_buffer_from_pixmap_as_png(ctx, wri->pixmap, fz_default_color_params);
		fz_write_zip_entry(ctx, wri->zip, name, buffer, 0);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_buffer(ctx, buffer);
		fz_drop_pixmap(ctx, wri->pixmap);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
cbz_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	fz_close_zip_writer(ctx, wri->zip);
}

static void
cbz_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_cbz_writer *wri = (fz_cbz_writer*)wri_;
	fz_drop_zip_writer(ctx, wri->zip);
	fz_drop_pixmap(ctx, wri->pixmap);
}

fz_document_writer *
fz_new_cbz_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_cbz_writer *wri = NULL;

	fz_var(wri);
	fz_var(out);

	fz_try(ctx)
	{
		fz_output *out_temp = out;
		wri = fz_new_derived_document_writer(ctx, fz_cbz_writer, cbz_begin_page, cbz_end_page, cbz_close_writer, cbz_drop_writer);
		fz_parse_draw_options(ctx, &wri->options, options);
		out = NULL;
		wri->zip = fz_new_zip_writer_with_output(ctx, out_temp);
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}
	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_cbz_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.cbz", 0);
	fz_document_writer *wri = NULL;
	fz_try(ctx)
		wri = fz_new_cbz_writer_with_output(ctx, out, options);
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return wri;
}

/* generic image file output writer */

typedef struct
{
	fz_document_writer super;
	fz_draw_options options;
	fz_pixmap *pixmap;
	void (*save)(fz_context *ctx, fz_pixmap *pix, const char *filename);
	int count;
	char *path;
} fz_pixmap_writer;

static fz_device *
pixmap_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_pixmap_writer *wri = (fz_pixmap_writer*)wri_;
	return fz_new_draw_device_with_options(ctx, &wri->options, mediabox, &wri->pixmap);
}

static void
pixmap_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_pixmap_writer *wri = (fz_pixmap_writer*)wri_;
	char path[PATH_MAX];

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		wri->count += 1;
		fz_format_output_path(ctx, path, sizeof path, wri->path, wri->count);
		wri->save(ctx, wri->pixmap, path);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_pixmap(ctx, wri->pixmap);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pixmap_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pixmap_writer *wri = (fz_pixmap_writer*)wri_;
	fz_drop_pixmap(ctx, wri->pixmap);
	fz_free(ctx, wri->path);
}

fz_document_writer *
fz_new_pixmap_writer(fz_context *ctx, const char *path, const char *options,
	const char *default_path, int n,
	void (*save)(fz_context *ctx, fz_pixmap *pix, const char *filename))
{
	fz_pixmap_writer *wri = fz_new_derived_document_writer(ctx, fz_pixmap_writer, pixmap_begin_page, pixmap_end_page, NULL, pixmap_drop_writer);

	fz_try(ctx)
	{
		fz_parse_draw_options(ctx, &wri->options, options);
		wri->path = fz_strdup(ctx, path ? path : default_path);
		wri->save = save;
		switch (n)
		{
		case 1: wri->options.colorspace = fz_device_gray(ctx); break;
		case 3: wri->options.colorspace = fz_device_rgb(ctx); break;
		case 4: wri->options.colorspace = fz_device_cmyk(ctx); break;
		}
	}
	fz_catch(ctx)
	{
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}
