#ifndef MUPDF_FITZ_UTIL_H
#define MUPDF_FITZ_UTIL_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/document.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/structured-text.h"
#include "mupdf/fitz/buffer.h"

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

/**
	Extract text from page.

	Ownership of the fz_stext_page is returned to the caller.
*/
fz_stext_page *fz_new_stext_page_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options);
fz_stext_page *fz_new_stext_page_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options);
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
int fz_search_page(fz_context *ctx, fz_page *page, const char *needle, fz_quad *hit_bbox, int hit_max);
int fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, fz_quad *hit_bbox, int hit_max);
int fz_search_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int page, const char *needle, fz_quad *hit_bbox, int hit_max);
int fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, fz_quad *hit_bbox, int hit_max);

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
fz_display_list *fz_new_display_list_from_svg_xml(fz_context *ctx, fz_xml *xml, const char *base_uri, fz_archive *zip, float *w, float *h);

/**
	Create a scalable image from an SVG document.
*/
fz_image *fz_new_image_from_svg_xml(fz_context *ctx, fz_xml *xml, const char *base_uri, fz_archive *zip);

/**
	Write image as a data URI (for HTML and SVG output).
*/
void fz_write_image_as_data_uri(fz_context *ctx, fz_output *out, fz_image *image);
void fz_write_pixmap_as_data_uri(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

/**
	Use text extraction to convert the input document into XHTML,
	then open the result as a new document that can be reflowed.
*/
fz_document *fz_new_xhtml_document_from_document(fz_context *ctx, fz_document *old_doc);

#endif
