#ifndef MUPDF_PDF_PAGE_H
#define MUPDF_PDF_PAGE_H

#include "mupdf/pdf/interpret.h"

int pdf_lookup_page_number(fz_context *ctx, pdf_document *doc, pdf_obj *pageobj);
int pdf_count_pages(fz_context *ctx, pdf_document *doc);
int pdf_count_pages_imp(fz_context *ctx, fz_document *doc, int chapter);
pdf_obj *pdf_lookup_page_obj(fz_context *ctx, pdf_document *doc, int needle);
void pdf_load_page_tree(fz_context *ctx, pdf_document *doc);
void pdf_drop_page_tree(fz_context *ctx, pdf_document *doc);

int pdf_lookup_anchor(fz_context *ctx, pdf_document *doc, const char *name, float *xp, float *yp);

void pdf_flatten_inheritable_page_items(fz_context *ctx, pdf_obj *page);

pdf_page *pdf_load_page(fz_context *ctx, pdf_document *doc, int number);
fz_page *pdf_load_page_imp(fz_context *ctx, fz_document *doc, int chapter, int number);

void pdf_page_obj_transform(fz_context *ctx, pdf_obj *pageobj, fz_rect *page_mediabox, fz_matrix *page_ctm);
void pdf_page_transform(fz_context *ctx, pdf_page *page, fz_rect *mediabox, fz_matrix *ctm);
pdf_obj *pdf_page_resources(fz_context *ctx, pdf_page *page);
pdf_obj *pdf_page_contents(fz_context *ctx, pdf_page *page);
pdf_obj *pdf_page_group(fz_context *ctx, pdf_page *page);

fz_separations *pdf_page_separations(fz_context *ctx, pdf_page *page);

void pdf_read_ocg(fz_context *ctx, pdf_document *doc);
void pdf_drop_ocg(fz_context *ctx, pdf_document *doc);
int pdf_is_hidden_ocg(fz_context *ctx, pdf_ocg_descriptor *desc, pdf_obj *rdb, const char *usage, pdf_obj *ocg);

fz_link *pdf_load_links(fz_context *ctx, pdf_page *page);

fz_rect pdf_bound_page(fz_context *ctx, pdf_page *page);

void pdf_run_page(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

void pdf_run_page_with_usage(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie);

void pdf_run_page_contents(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
void pdf_run_page_annots(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
void pdf_run_page_widgets(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

void pdf_filter_page_contents(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_filter_options *filter);
void pdf_filter_annot_contents(fz_context *ctx, pdf_document *doc, pdf_annot *annot, pdf_filter_options *filter);

typedef struct pdf_redact_options_s pdf_redact_options;

struct pdf_redact_options_s
{
	int no_black_boxes;
	int keep_images;
};

int pdf_redact_page(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_redact_options *opts);

fz_transition *pdf_page_presentation(fz_context *ctx, pdf_page *page, fz_transition *transition, float *duration);

fz_default_colorspaces *pdf_load_default_colorspaces(fz_context *ctx, pdf_document *doc, pdf_page *page);

fz_default_colorspaces *pdf_update_default_colorspaces(fz_context *ctx, fz_default_colorspaces *old_cs, pdf_obj *res);

/*
 * Page tree, pages and related objects
 */

struct pdf_page_s
{
	fz_page super;
	pdf_document *doc;
	pdf_obj *obj;

	int transparency;
	int overprint;

	fz_link *links;
	pdf_annot *annots, **annot_tailp;
	pdf_widget *widgets, **widget_tailp;
};

#endif
