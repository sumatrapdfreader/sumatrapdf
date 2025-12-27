// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_WRITER_H
#define MUPDF_FITZ_WRITER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/document.h"
#include "mupdf/fitz/device.h"

typedef struct fz_document_writer fz_document_writer;

/**
	Function type to start
	the process of writing a page to a document.

	mediabox: page size rectangle in points.

	Returns a fz_device to write page contents to.
*/
typedef fz_device *(fz_document_writer_begin_page_fn)(fz_context *ctx, fz_document_writer *wri, fz_rect mediabox);

/**
	Function type to end the
	process of writing a page to a document.

	dev: The device created by the begin_page function.
*/
typedef void (fz_document_writer_end_page_fn)(fz_context *ctx, fz_document_writer *wri, fz_device *dev);

/**
	Function type to end
	the process of writing pages to a document.

	This writes any file level trailers required. After this
	completes successfully the file is up to date and complete.
*/
typedef void (fz_document_writer_close_writer_fn)(fz_context *ctx, fz_document_writer *wri);

/**
	Function type to discard
	an fz_document_writer. This may be called at any time during
	the process to release all the resources owned by the writer.

	Calling drop without having previously called close may leave
	the file in an inconsistent state and the user of the
	fz_document_writer would need to do any cleanup required.
*/
typedef void (fz_document_writer_drop_writer_fn)(fz_context *ctx, fz_document_writer *wri);

#define fz_new_derived_document_writer(CTX,TYPE,BEGIN_PAGE,END_PAGE,CLOSE,DROP) \
	((TYPE *)Memento_label(fz_new_document_writer_of_size(CTX,sizeof(TYPE),BEGIN_PAGE,END_PAGE,CLOSE,DROP),#TYPE))

/**
	Look for a given option (key) in the opts string. Return 1 if
	it has it, and update *val to point to the value within opts.
*/
int fz_has_option(fz_context *ctx, const char *opts, const char *key, const char **val);

/**
	Check to see if an option, a, from a string matches a reference
	option, b.

	(i.e. a could be 'foo' or 'foo,bar...' etc, but b can only be
	'foo'.)
*/
int fz_option_eq(const char *a, const char *b);

/**
	Copy an option (val) into a destination buffer (dest), of maxlen
	bytes.

	Returns the number of bytes (including terminator) that did not
	fit. If val is maxlen or greater bytes in size, it will be left
	unterminated.
*/
size_t fz_copy_option(fz_context *ctx, const char *val, char *dest, size_t maxlen);

/**
	Create a new fz_document_writer, for a
	file of the given type.

	path: The document name to write (or NULL for default)

	format: Which format to write (currently cbz, html, pdf, pam,
	pbm, pgm, pkm, png, ppm, pnm, svg, text, xhtml, docx, odt)

	options: NULL, or pointer to comma separated string to control
	file generation.
*/
fz_document_writer *fz_new_document_writer(fz_context *ctx, const char *path, const char *format, const char *options);

/**
	Like fz_new_document_writer but takes a fz_output for writing
	the result. Only works for multi-page formats.
*/
fz_document_writer *
fz_new_document_writer_with_output(fz_context *ctx, fz_output *out, const char *format, const char *options);

fz_document_writer *
fz_new_document_writer_with_buffer(fz_context *ctx, fz_buffer *buf, const char *format, const char *options);

/**
	Document writers for various possible output formats.

	All of the "_with_output" variants pass the ownership of out in
	immediately upon calling. The writers are responsible for
	dropping the fz_output when they are finished with it (even
	if they throw an exception during creation).
*/
fz_document_writer *fz_new_pdf_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pdf_writer_with_output(fz_context *ctx, fz_output *out, const char *options);
fz_document_writer *fz_new_svg_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_svg_writer_with_output(fz_context *ctx, fz_output *out, const char *options);

fz_document_writer *fz_new_text_writer(fz_context *ctx, const char *format, const char *path, const char *options);
fz_document_writer *fz_new_text_writer_with_output(fz_context *ctx, const char *format, fz_output *out, const char *options);

fz_document_writer *fz_new_odt_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_odt_writer_with_output(fz_context *ctx, fz_output *out, const char *options);
fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_docx_writer_with_output(fz_context *ctx, fz_output *out, const char *options);

fz_document_writer *fz_new_ps_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_ps_writer_with_output(fz_context *ctx, fz_output *out, const char *options);
fz_document_writer *fz_new_pcl_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pcl_writer_with_output(fz_context *ctx, fz_output *out, const char *options);
fz_document_writer *fz_new_pclm_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pclm_writer_with_output(fz_context *ctx, fz_output *out, const char *options);
fz_document_writer *fz_new_pwg_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pwg_writer_with_output(fz_context *ctx, fz_output *out, const char *options);

fz_document_writer *fz_new_cbz_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_cbz_writer_with_output(fz_context *ctx, fz_output *out, const char *options);

fz_document_writer *fz_new_csv_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_csv_writer_with_output(fz_context *ctx, fz_output *out, const char *options);

/**
	Used to report progress of the OCR operation.

	page: Current page being processed.

	percent: Progress of the OCR operation for the
	current page in percent. Whether it reaches 100
	once a page is finished, depends on the OCR engine.

	Return 0 to continue progress, return 1 to cancel the
	operation.
*/
typedef int (fz_pdfocr_progress_fn)(fz_context *ctx, void *progress_arg, int page, int percent);

fz_document_writer *fz_new_pdfocr_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pdfocr_writer_with_output(fz_context *ctx, fz_output *out, const char *options);
void fz_pdfocr_writer_set_progress(fz_context *ctx, fz_document_writer *writer, fz_pdfocr_progress_fn *progress, void *);

fz_document_writer *fz_new_jpeg_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_png_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pam_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pnm_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pgm_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_ppm_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pbm_pixmap_writer(fz_context *ctx, const char *path, const char *options);
fz_document_writer *fz_new_pkm_pixmap_writer(fz_context *ctx, const char *path, const char *options);

/**
	Called to start the process of writing a page to
	a document.

	mediabox: page size rectangle in points.

	Returns a borrowed fz_device to write page contents to. This
	should be kept if required, and only dropped if it was kept.
*/
fz_device *fz_begin_page(fz_context *ctx, fz_document_writer *wri, fz_rect mediabox);

/**
	Called to end the process of writing a page to a
	document.
*/
void fz_end_page(fz_context *ctx, fz_document_writer *wri);

/**
	Convenience function to feed all the pages of a document to
	fz_begin_page/fz_run_page/fz_end_page.
*/
void fz_write_document(fz_context *ctx, fz_document_writer *wri, fz_document *doc);

/**
	Called to end the process of writing
	pages to a document.

	This writes any file level trailers required. After this
	completes successfully the file is up to date and complete.
*/
void fz_close_document_writer(fz_context *ctx, fz_document_writer *wri);

/**
	Called to discard a fz_document_writer.
	This may be called at any time during the process to release all
	the resources owned by the writer.

	Calling drop without having previously called close may leave
	the file in an inconsistent state.
*/
void fz_drop_document_writer(fz_context *ctx, fz_document_writer *wri);

fz_document_writer *fz_new_pixmap_writer(fz_context *ctx, const char *path, const char *options, const char *default_path, int n,
	void (*save)(fz_context *ctx, fz_pixmap *pix, const char *filename));

fz_document_writer *fz_new_pixmap_writer_with_output(fz_context *ctx, fz_output *out, const char *options, int n,
	void (*write)(fz_context *ctx, fz_output *out, fz_pixmap *pix));

FZ_DATA extern const char *fz_pdf_write_options_usage;
FZ_DATA extern const char *fz_svg_write_options_usage;

FZ_DATA extern const char *fz_pcl_write_options_usage;
FZ_DATA extern const char *fz_pclm_write_options_usage;
FZ_DATA extern const char *fz_pwg_write_options_usage;
FZ_DATA extern const char *fz_pdfocr_write_options_usage;

/* Implementation details: subject to change. */

/**
	Structure is public to allow other structures to
	be derived from it. Do not access members directly.
*/
struct fz_document_writer
{
	fz_document_writer_begin_page_fn *begin_page;
	fz_document_writer_end_page_fn *end_page;
	fz_document_writer_close_writer_fn *close_writer;
	fz_document_writer_drop_writer_fn *drop_writer;
	fz_device *dev;
};

/**
	Internal function to allocate a
	block for a derived document_writer structure, with the base
	structure's function pointers populated correctly, and the extra
	space zero initialised.
*/
fz_document_writer *fz_new_document_writer_of_size(fz_context *ctx, size_t size,
		fz_document_writer_begin_page_fn *begin_page,
		fz_document_writer_end_page_fn *end_page,
		fz_document_writer_close_writer_fn *close,
		fz_document_writer_drop_writer_fn *drop);



#endif
