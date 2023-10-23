// Copyright (C) 2004-2023 Artifex Software, Inc.
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

/* Return non-null terminated pointers to key/value entries in comma separated
 * option string. A plain key has the default value 'yes'. Use strncmp to compare
 * key/value strings. */
static const char *
fz_get_option(fz_context *ctx, const char **key, const char **val, const char *opts)
{
	if (!opts || *opts == 0)
		return NULL;

	if (*opts == ',')
		++opts;

	*key = opts;
	while (*opts != 0 && *opts != ',' && *opts != '=')
		++opts;

	if (*opts == '=')
	{
		*val = ++opts;
		while (*opts != 0 && *opts != ',')
			++opts;
	}
	else
	{
		*val = "yes";
	}

	return opts;
}

int
fz_has_option(fz_context *ctx, const char *opts, const char *key, const char **val)
{
	const char *straw;
	size_t n = strlen(key);
	while ((opts = fz_get_option(ctx, &straw, val, opts)))
		if (!strncmp(straw, key, n) && (straw[n] == '=' || straw[n] == ',' || straw[n] == 0))
			return 1;
	return 0;
}

int
fz_option_eq(const char *a, const char *b)
{
	size_t n = strlen(b);
	return !strncmp(a, b, n) && (a[n] == ',' || a[n] == 0);
}

size_t
fz_copy_option(fz_context *ctx, const char *val, char *dest, size_t maxlen)
{
	const char *e = val;
	size_t len, len2;

	if (val == NULL) {
		if (maxlen)
			*dest = 0;
		return 0;
	}

	while (*e != ',' && *e != 0)
		e++;

	len = e-val;
	len2 = len+1; /* Allow for terminator */
	if (len > maxlen)
		len = maxlen;
	memcpy(dest, val, len);
	if (len < maxlen)
		memset(dest+len, 0, maxlen-len);

	return len2 >= maxlen ? len2 - maxlen : 0;
}

fz_document_writer *fz_new_document_writer_of_size(fz_context *ctx, size_t size, fz_document_writer_begin_page_fn *begin_page,
	fz_document_writer_end_page_fn *end_page, fz_document_writer_close_writer_fn *close, fz_document_writer_drop_writer_fn *drop)
{
	fz_document_writer *wri = Memento_label(fz_calloc(ctx, 1, size), "fz_document_writer");

	wri->begin_page = begin_page;
	wri->end_page = end_page;
	wri->close_writer = close;
	wri->drop_writer = drop;

	return wri;
}

static void fz_save_pixmap_as_jpeg_default(fz_context *ctx, fz_pixmap *pixmap, const char *filename)
{
	fz_save_pixmap_as_jpeg(ctx, pixmap, filename, 90);
}

fz_document_writer *fz_new_jpeg_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.jpeg", 0, fz_save_pixmap_as_jpeg_default);
}

fz_document_writer *fz_new_png_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.png", 0, fz_save_pixmap_as_png);
}

fz_document_writer *fz_new_pam_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pam", 0, fz_save_pixmap_as_pam);
}

fz_document_writer *fz_new_pnm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pnm", 0, fz_save_pixmap_as_pnm);
}

fz_document_writer *fz_new_pgm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pgm", 1, fz_save_pixmap_as_pnm);
}

fz_document_writer *fz_new_ppm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.ppm", 3, fz_save_pixmap_as_pnm);
}

fz_document_writer *fz_new_pbm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pbm", 1, fz_save_pixmap_as_pbm);
}

fz_document_writer *fz_new_pkm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pkm", 4, fz_save_pixmap_as_pkm);
}

static int is_extension(const char *a, const char *ext)
{
	if (a[0] == '.')
		++a;
	return !fz_strcasecmp(a, ext);
}

static const char *prev_period(const char *start, const char *p)
{
	while (--p > start)
		if (*p == '.')
			return p;
	return NULL;
}

fz_document_writer *
fz_new_document_writer(fz_context *ctx, const char *path, const char *explicit_format, const char *options)
{
	const char *format = explicit_format;
	if (!format)
		format = strrchr(path, '.');
	while (format)
	{
#if FZ_ENABLE_OCR_OUTPUT
		if (is_extension(format, "ocr"))
			return fz_new_pdfocr_writer(ctx, path, options);
#endif
#if FZ_ENABLE_PDF
		if (is_extension(format, "pdf"))
			return fz_new_pdf_writer(ctx, path, options);
#endif

		if (is_extension(format, "cbz"))
			return fz_new_cbz_writer(ctx, path, options);

		if (is_extension(format, "svg"))
			return fz_new_svg_writer(ctx, path, options);

		if (is_extension(format, "png"))
			return fz_new_png_pixmap_writer(ctx, path, options);
		if (is_extension(format, "pam"))
			return fz_new_pam_pixmap_writer(ctx, path, options);
		if (is_extension(format, "pnm"))
			return fz_new_pnm_pixmap_writer(ctx, path, options);
		if (is_extension(format, "pgm"))
			return fz_new_pgm_pixmap_writer(ctx, path, options);
		if (is_extension(format, "ppm"))
			return fz_new_ppm_pixmap_writer(ctx, path, options);
		if (is_extension(format, "pbm"))
			return fz_new_pbm_pixmap_writer(ctx, path, options);
		if (is_extension(format, "pkm"))
			return fz_new_pkm_pixmap_writer(ctx, path, options);
		if (is_extension(format, "jpeg") || is_extension(format, "jpg"))
			return fz_new_jpeg_pixmap_writer(ctx, path, options);

		if (is_extension(format, "pcl"))
			return fz_new_pcl_writer(ctx, path, options);
		if (is_extension(format, "pclm"))
			return fz_new_pclm_writer(ctx, path, options);
		if (is_extension(format, "ps"))
			return fz_new_ps_writer(ctx, path, options);
		if (is_extension(format, "pwg"))
			return fz_new_pwg_writer(ctx, path, options);

		if (is_extension(format, "txt") || is_extension(format, "text"))
			return fz_new_text_writer(ctx, "text", path, options);
		if (is_extension(format, "html"))
			return fz_new_text_writer(ctx, "html", path, options);
		if (is_extension(format, "xhtml"))
			return fz_new_text_writer(ctx, "xhtml", path, options);
		if (is_extension(format, "stext") || is_extension(format, "stext.xml"))
			return fz_new_text_writer(ctx, "stext.xml", path, options);
		if (is_extension(format, "stext.json"))
			return fz_new_text_writer(ctx, "stext.json", path, options);

#if FZ_ENABLE_ODT_OUTPUT
		if (is_extension(format, "odt"))
			return fz_new_odt_writer(ctx, path, options);
#endif
#if FZ_ENABLE_DOCX_OUTPUT
		if (is_extension(format, "docx"))
			return fz_new_docx_writer(ctx, path, options);
#endif
		if (format != explicit_format)
			format = prev_period(path, format);
		else
			format = NULL;
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot detect document format");
}

fz_document_writer *
fz_new_document_writer_with_output(fz_context *ctx, fz_output *out, const char *format, const char *options)
{
	if (is_extension(format, "cbz"))
		return fz_new_cbz_writer_with_output(ctx, out, options);
	if (is_extension(format, "ocr"))
		return fz_new_pdfocr_writer_with_output(ctx, out, options);
#if FZ_ENABLE_PDF
	if (is_extension(format, "pdf"))
		return fz_new_pdf_writer_with_output(ctx, out, options);
#endif

	if (is_extension(format, "pcl"))
		return fz_new_pcl_writer_with_output(ctx, out, options);
	if (is_extension(format, "pclm"))
		return fz_new_pclm_writer_with_output(ctx, out, options);
	if (is_extension(format, "ps"))
		return fz_new_ps_writer_with_output(ctx, out, options);
	if (is_extension(format, "pwg"))
		return fz_new_pwg_writer_with_output(ctx, out, options);

	if (is_extension(format, "txt") || is_extension(format, "text"))
		return fz_new_text_writer_with_output(ctx, "text", out, options);
	if (is_extension(format, "html"))
		return fz_new_text_writer_with_output(ctx, "html", out, options);
	if (is_extension(format, "xhtml"))
		return fz_new_text_writer_with_output(ctx, "xhtml", out, options);
	if (is_extension(format, "stext") || is_extension(format, "stext.xml"))
		return fz_new_text_writer_with_output(ctx, "stext.xml", out, options);
	if (is_extension(format, "stext.json"))
		return fz_new_text_writer_with_output(ctx, "stext.json", out, options);

#if FZ_ENABLE_ODT_OUTPUT
	if (is_extension(format, "odt"))
		return fz_new_odt_writer_with_output(ctx, out, options);
#endif
#if FZ_ENABLE_DOCX_OUTPUT
	if (is_extension(format, "docx"))
		return fz_new_docx_writer_with_output(ctx, out, options);
#endif

	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown output document format: %s", format);
}

fz_document_writer *
fz_new_document_writer_with_buffer(fz_context *ctx, fz_buffer *buffer, const char *format, const char *options)
{
	fz_document_writer *wri;
	fz_output *out = fz_new_output_with_buffer(ctx, buffer);
	fz_try(ctx)
		wri = fz_new_document_writer_with_output(ctx, out, format, options);
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return wri;
}

void
fz_close_document_writer(fz_context *ctx, fz_document_writer *wri)
{
	if (wri->close_writer)
		wri->close_writer(ctx, wri);
	wri->close_writer = NULL;
}

void
fz_drop_document_writer(fz_context *ctx, fz_document_writer *wri)
{
	if (!wri)
		return;

	if (wri->close_writer)
		fz_warn(ctx, "dropping unclosed document writer");
	if (wri->dev)
		fz_drop_device(ctx, wri->dev);
	if (wri->drop_writer)
		wri->drop_writer(ctx, wri);
	fz_free(ctx, wri);
}

fz_device *
fz_begin_page(fz_context *ctx, fz_document_writer *wri, fz_rect mediabox)
{
	if (!wri)
		return NULL;
	if (wri->dev)
		fz_throw(ctx, FZ_ERROR_GENERIC, "called begin page without ending the previous page");
	wri->dev = wri->begin_page(ctx, wri, mediabox);
	return wri->dev;
}

void
fz_end_page(fz_context *ctx, fz_document_writer *wri)
{
	fz_device *dev;

	if (!wri)
		return;
	dev = wri->dev;
	wri->dev = NULL;
	wri->end_page(ctx, wri, dev);
}

void
fz_write_document(fz_context *ctx, fz_document_writer *wri, fz_document *doc)
{
	int i, n;
	fz_page *page = NULL;
	fz_device *dev;

	fz_var(page);

	n = fz_count_pages(ctx, doc);
	fz_try(ctx)
	{
		for (i = 0; i < n; i++)
		{
			page = fz_load_page(ctx, doc, i);
			dev = fz_begin_page(ctx, wri, fz_bound_page(ctx, page));
			fz_run_page(ctx, page, dev, fz_identity, NULL);
			fz_drop_page(ctx, page);
			page = NULL;
			fz_end_page(ctx, wri);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_page(ctx, page);
		fz_rethrow(ctx);
	}
}
