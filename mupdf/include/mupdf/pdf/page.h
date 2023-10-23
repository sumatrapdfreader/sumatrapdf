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

#ifndef MUPDF_PDF_PAGE_H
#define MUPDF_PDF_PAGE_H

#include "mupdf/pdf/interpret.h"

int pdf_lookup_page_number(fz_context *ctx, pdf_document *doc, pdf_obj *pageobj);
int pdf_count_pages(fz_context *ctx, pdf_document *doc);
int pdf_count_pages_imp(fz_context *ctx, fz_document *doc, int chapter);
pdf_obj *pdf_lookup_page_obj(fz_context *ctx, pdf_document *doc, int needle);

/*
	Cache the page tree for fast forward/reverse page lookups.

	No longer required. This is a No Op, now as page tree
	maps are loaded automatically 'just in time'.
*/
void pdf_load_page_tree(fz_context *ctx, pdf_document *doc);

/*
	Discard the page tree maps.

	No longer required. This is a No Op, now as page tree
	maps are discarded automatically 'just in time'.
*/
void pdf_drop_page_tree(fz_context *ctx, pdf_document *doc);

void pdf_drop_page_tree_internal(fz_context *ctx, pdf_document *doc);


/*
	Make page self sufficient.

	Copy any inheritable page keys into the actual page object, removing
	any dependencies on the page tree parents.
*/
void pdf_flatten_inheritable_page_items(fz_context *ctx, pdf_obj *page);

/*
	Load a page and its resources.

	Locates the page in the PDF document and loads the page and its
	resources. After pdf_load_page is it possible to retrieve the size
	of the page using pdf_bound_page, or to render the page using
	pdf_run_page_*.

	number: page number, where 0 is the first page of the document.
*/
pdf_page *pdf_load_page(fz_context *ctx, pdf_document *doc, int number);
fz_page *pdf_load_page_imp(fz_context *ctx, fz_document *doc, int chapter, int number);
int pdf_page_has_transparency(fz_context *ctx, pdf_page *page);

void pdf_page_obj_transform(fz_context *ctx, pdf_obj *pageobj, fz_rect *page_mediabox, fz_matrix *page_ctm);
void pdf_page_transform(fz_context *ctx, pdf_page *page, fz_rect *mediabox, fz_matrix *ctm);
void pdf_page_obj_transform_box(fz_context *ctx, pdf_obj *pageobj, fz_rect *page_mediabox, fz_matrix *page_ctm, fz_box_type box);
void pdf_page_transform_box(fz_context *ctx, pdf_page *page, fz_rect *mediabox, fz_matrix *ctm, fz_box_type box);
pdf_obj *pdf_page_resources(fz_context *ctx, pdf_page *page);
pdf_obj *pdf_page_contents(fz_context *ctx, pdf_page *page);
pdf_obj *pdf_page_group(fz_context *ctx, pdf_page *page);

/*
	Get the separation details for a page.
*/
fz_separations *pdf_page_separations(fz_context *ctx, pdf_page *page);

pdf_ocg_descriptor *pdf_read_ocg(fz_context *ctx, pdf_document *doc);
void pdf_drop_ocg(fz_context *ctx, pdf_document *doc);
int pdf_is_ocg_hidden(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, const char *usage, pdf_obj *ocg);

fz_link *pdf_load_links(fz_context *ctx, pdf_page *page);

/*
	Determine the size of a page.

	Determine the page size in points, taking page rotation
	into account. The page size is taken to be the crop box if it
	exists (visible area after cropping), otherwise the media box will
	be used (possibly including printing marks).
*/
fz_rect pdf_bound_page(fz_context *ctx, pdf_page *page, fz_box_type box);

/*
	Interpret a loaded page and render it on a device.

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_page(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

/*
	Interpret a loaded page and render it on a device.

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.

	usage: The 'usage' for displaying the file (typically
	'View', 'Print' or 'Export'). NULL means 'View'.

	cookie: A pointer to an optional fz_cookie structure that can be used
	to track progress, collect errors etc.
*/
void pdf_run_page_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie);

/*
	Interpret a loaded page and render it on a device.
	Just the main page contents without the annotations

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_page_contents(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
void pdf_run_page_annots(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
void pdf_run_page_widgets(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
void pdf_run_page_contents_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie);
void pdf_run_page_annots_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie);
void pdf_run_page_widgets_with_usage(fz_context *ctx, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie);

void pdf_filter_page_contents(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_filter_options *options);
void pdf_filter_annot_contents(fz_context *ctx, pdf_document *doc, pdf_annot *annot, pdf_filter_options *options);

fz_pixmap *pdf_new_pixmap_from_page_contents_with_usage(fz_context *ctx, pdf_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha, const char *usage, fz_box_type box);
fz_pixmap *pdf_new_pixmap_from_page_with_usage(fz_context *ctx, pdf_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha, const char *usage, fz_box_type box);
fz_pixmap *pdf_new_pixmap_from_page_contents_with_separations_and_usage(fz_context *ctx, pdf_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha, const char *usage, fz_box_type box);
fz_pixmap *pdf_new_pixmap_from_page_with_separations_and_usage(fz_context *ctx, pdf_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha, const char *usage, fz_box_type box);

enum {
	PDF_REDACT_IMAGE_NONE,
	PDF_REDACT_IMAGE_REMOVE,
	PDF_REDACT_IMAGE_PIXELS,
};

typedef struct
{
	int black_boxes;
	int image_method;
} pdf_redact_options;

int pdf_redact_page(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_redact_options *opts);

fz_transition *pdf_page_presentation(fz_context *ctx, pdf_page *page, fz_transition *transition, float *duration);

fz_default_colorspaces *pdf_load_default_colorspaces(fz_context *ctx, pdf_document *doc, pdf_page *page);

/*
	Update default colorspaces for an xobject.
*/
fz_default_colorspaces *pdf_update_default_colorspaces(fz_context *ctx, fz_default_colorspaces *old_cs, pdf_obj *res);

/*
 * Page tree, pages and related objects
 */

struct pdf_page
{
	fz_page super;
	pdf_document *doc; /* type alias for super.doc */
	pdf_obj *obj;

	int transparency;
	int overprint;

	fz_link *links;
	pdf_annot *annots, **annot_tailp;
	pdf_annot *widgets, **widget_tailp;
};

#endif
