// Copyright (C) 2004-2022 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_UTIL_H
#define MUPDF_FITZ_UTIL_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/document.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/structured-text.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/xml.h"
#include "mupdf/fitz/archive.h"
#include "mupdf/fitz/display-list.h"

/**
	Create a display list.

	Ownership of the display list is returned to the caller.
*/
fz_display_list *fz_new_display_list_from_page(fz_context *ctx, fz_page *page);
fz_display_list *fz_new_display_list_from_page_number(fz_context *ctx, fz_document *doc, int number);

/**
	Create a display list from page contents (no annotations).

	Ownership of the display list is returned to the caller.
*/
fz_display_list *fz_new_display_list_from_page_contents(fz_context *ctx, fz_page *page);

/**
	Render the page to a pixmap using the transform and colorspace.

	Ownership of the pixmap is returned to the caller.
*/
fz_pixmap *fz_new_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha);
fz_pixmap *fz_new_pixmap_from_page(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha);
fz_pixmap *fz_new_pixmap_from_page_number(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha);

/**
	Render the page contents without annotations.

	Ownership of the pixmap is returned to the caller.
*/
fz_pixmap *fz_new_pixmap_from_page_contents(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha);

/**
	Render the page contents with control over spot colors.

	Ownership of the pixmap is returned to the caller.
*/
fz_pixmap *fz_new_pixmap_from_display_list_with_separations(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_pixmap *fz_new_pixmap_from_page_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_pixmap *fz_new_pixmap_from_page_number_with_separations(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_pixmap *fz_new_pixmap_from_page_contents_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);

fz_pixmap *fz_fill_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_pixmap *pix);

/**
	Extract text from page.

	Ownership of the fz_stext_page is returned to the caller.
*/
fz_stext_page *fz_new_stext_page_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options);
fz_stext_page *fz_new_stext_page_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options);
fz_stext_page *fz_new_stext_page_from_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int number, const fz_stext_options *options);
fz_stext_page *fz_new_stext_page_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options);

/**
	Convert structured text into plain text.
*/
fz_buffer *fz_new_buffer_from_stext_page(fz_context *ctx, fz_stext_page *text);
fz_buffer *fz_new_buffer_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options);
fz_buffer *fz_new_buffer_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options);
fz_buffer *fz_new_buffer_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options);

/**
	Search for the 'needle' text on the page.
	Record the hits in the hit_bbox array and return the number of
	hits. Will stop looking once it has filled hit_max rectangles.
*/
int fz_search_page(fz_context *ctx, fz_page *page, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max);
int fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max);
int fz_search_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int page, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max);
int fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max);

/**
	Parse an SVG document into a display-list.
*/
fz_display_list *fz_new_display_list_from_svg(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip, float *w, float *h);

/**
	Create a scalable image from an SVG document.
*/
fz_image *fz_new_image_from_svg(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip);

/**
	Parse an SVG document into a display-list.
*/
fz_display_list *fz_new_display_list_from_svg_xml(fz_context *ctx, fz_xml_doc *xmldoc, fz_xml *xml, const char *base_uri, fz_archive *zip, float *w, float *h);

/**
	Create a scalable image from an SVG document.
*/
fz_image *fz_new_image_from_svg_xml(fz_context *ctx, fz_xml_doc *xmldoc, fz_xml *xml, const char *base_uri, fz_archive *zip);

/**
	Write image as a data URI (for HTML and SVG output).
*/
void fz_write_image_as_data_uri(fz_context *ctx, fz_output *out, fz_image *image);
void fz_write_pixmap_as_data_uri(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);
void fz_append_image_as_data_uri(fz_context *ctx, fz_buffer *out, fz_image *image);
void fz_append_pixmap_as_data_uri(fz_context *ctx, fz_buffer *out, fz_pixmap *pixmap);

/**
	Use text extraction to convert the input document into XHTML,
	then open the result as a new document that can be reflowed.
*/
fz_document *fz_new_xhtml_document_from_document(fz_context *ctx, fz_document *old_doc, const fz_stext_options *opts);

/**
	Returns an fz_buffer containing a page after conversion to specified format.

	page: The page to convert.
	format, options: Passed to fz_new_document_writer_with_output() internally.
	transform, cookie: Passed to fz_run_page() internally.
*/
fz_buffer *fz_new_buffer_from_page_with_format(fz_context *ctx, fz_page *page, const char *format, const char *options, fz_matrix transform, fz_cookie *cookie);

#endif
