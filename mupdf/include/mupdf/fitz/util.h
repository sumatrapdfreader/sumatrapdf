#ifndef MUPDF_FITZ_UTIL_H
#define MUPDF_FITZ_UTIL_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/document.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/structured-text.h"
#include "mupdf/fitz/buffer.h"

fz_display_list *fz_new_display_list_from_page(fz_context *ctx, fz_page *page);
fz_display_list *fz_new_display_list_from_page_number(fz_context *ctx, fz_document *doc, int number);
fz_display_list *fz_new_display_list_from_page_contents(fz_context *ctx, fz_page *page);

fz_pixmap *fz_new_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha);
fz_pixmap *fz_new_pixmap_from_page(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha);
fz_pixmap *fz_new_pixmap_from_page_number(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha);
fz_pixmap *fz_new_pixmap_from_page_contents(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha);

fz_pixmap *fz_new_pixmap_from_display_list_with_separations(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_pixmap *fz_new_pixmap_from_page_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_pixmap *fz_new_pixmap_from_page_number_with_separations(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);
fz_pixmap *fz_new_pixmap_from_page_contents_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha);

fz_stext_page *fz_new_stext_page_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options);
fz_stext_page *fz_new_stext_page_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options);
fz_stext_page *fz_new_stext_page_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options);

fz_buffer *fz_new_buffer_from_stext_page(fz_context *ctx, fz_stext_page *text);
fz_buffer *fz_new_buffer_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options);
fz_buffer *fz_new_buffer_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options);
fz_buffer *fz_new_buffer_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options);

int fz_search_page(fz_context *ctx, fz_page *page, const char *needle, fz_quad *hit_bbox, int hit_max);
int fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, fz_quad *hit_bbox, int hit_max);
int fz_search_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int page, const char *needle, fz_quad *hit_bbox, int hit_max);
int fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, fz_quad *hit_bbox, int hit_max);

fz_display_list *fz_new_display_list_from_svg(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip, float *w, float *h);
fz_image *fz_new_image_from_svg(fz_context *ctx, fz_buffer *buf, const char *base_uri, fz_archive *zip);
fz_display_list *fz_new_display_list_from_svg_xml(fz_context *ctx, fz_xml *xml, const char *base_uri, fz_archive *zip, float *w, float *h);
fz_image *fz_new_image_from_svg_xml(fz_context *ctx, fz_xml *xml, const char *base_uri, fz_archive *zip);

void fz_write_image_as_data_uri(fz_context *ctx, fz_output *out, fz_image *image);
void fz_write_pixmap_as_data_uri(fz_context *ctx, fz_output *out, fz_pixmap *pixmap);

fz_document *fz_new_xhtml_document_from_document(fz_context *ctx, fz_document *old_doc);

#endif
