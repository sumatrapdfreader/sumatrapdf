// Copyright (C) 2004-2024 Artifex Software, Inc.
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

pdf_page *pdf_keep_page(fz_context *ctx, pdf_page *page);
void pdf_drop_page(fz_context *ctx, pdf_page *page);

int pdf_lookup_page_number(fz_context *ctx, pdf_document *doc, pdf_obj *pageobj);
int pdf_count_pages(fz_context *ctx, pdf_document *doc);
int pdf_count_pages_imp(fz_context *ctx, fz_document *doc, int chapter);
pdf_obj *pdf_lookup_page_obj(fz_context *ctx, pdf_document *doc, int needle);
pdf_obj *pdf_lookup_page_loc(fz_context *ctx, pdf_document *doc, int needle, pdf_obj **parentp, int *indexp);

/*
	Enable or disable the page tree cache that is used to speed up page object lookups.
	The page tree cache is used unless explicitly disabled with this function.
*/
void pdf_set_page_tree_cache(fz_context *ctx, pdf_document *doc, int enabled);

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

/*
	Internal function used to drop the page tree.

	Library users should not call this directly.
*/
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

/*
	Internal function to perform pdf_load_page.

	Do not call this directly.
*/
fz_page *pdf_load_page_imp(fz_context *ctx, fz_document *doc, int chapter, int number);

/*
	Enquire as to whether a given page uses transparency or not.
*/
int pdf_page_has_transparency(fz_context *ctx, pdf_page *page);

/*
	Fetch the given box for a page, together with a transform that converts
	from fitz coords to PDF coords.

	pageobj: The object that represents the page.

	outbox: If non-NULL, this will be filled in with the requested box
	in fitz coordinates.

	outctm: A transform to map from fitz page space to PDF page space.

	box: Which box to return.
*/
void pdf_page_obj_transform_box(fz_context *ctx, pdf_obj *pageobj, fz_rect *outbox, fz_matrix *out, fz_box_type box);

/*
	As for pdf_page_obj_transform_box, always requesting the
	cropbox.
*/
void pdf_page_obj_transform(fz_context *ctx, pdf_obj *pageobj, fz_rect *outbox, fz_matrix *outctm);

/*
	As for pdf_page_obj_transform_box, but working from a pdf_page
	object rather than the pdf_obj representing the page.
*/
void pdf_page_transform_box(fz_context *ctx, pdf_page *page, fz_rect *mediabox, fz_matrix *ctm, fz_box_type box);

/*
	As for pdf_page_transform_box, always requesting the
	cropbox.
*/
void pdf_page_transform(fz_context *ctx, pdf_page *page, fz_rect *mediabox, fz_matrix *ctm);

/*
	Find the pdf object that represents the resources dictionary
	for a page.

	This is a borrowed pointer that the caller should pdf_keep_obj
	if. This may be NULL.
*/
pdf_obj *pdf_page_resources(fz_context *ctx, pdf_page *page);

/*
	Find the pdf object that represents the page contents
	for a page.

	This is a borrowed pointer that the caller should pdf_keep_obj
	if. This may be NULL.
*/
pdf_obj *pdf_page_contents(fz_context *ctx, pdf_page *page);

/*
	Find the pdf object that represents the transparency group
	for a page.

	This is a borrowed pointer that the caller should pdf_keep_obj
	if. This may be NULL.
*/
pdf_obj *pdf_page_group(fz_context *ctx, pdf_page *page);

/*
	Modify the page boxes (using fitz space coordinates).

	Note that changing the CropBox will change the fitz coordinate space mapping,
	invalidating all bounding boxes previously acquired.
*/
void pdf_set_page_box(fz_context *ctx, pdf_page *page, fz_box_type box, fz_rect rect);

/*
	Get the separation details for a page.
*/
fz_separations *pdf_page_separations(fz_context *ctx, pdf_page *page);

pdf_ocg_descriptor *pdf_read_ocg(fz_context *ctx, pdf_document *doc);
void pdf_drop_ocg(fz_context *ctx, pdf_document *doc);
int pdf_is_ocg_hidden(fz_context *ctx, pdf_document *doc, pdf_resource_stack *rdb, const char *usage, pdf_obj *ocg);

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
	/* Do not change images at all */
	PDF_REDACT_IMAGE_NONE,

	/* If the image intrudes across the redaction region (even if clipped),
	 * remove it. */
	PDF_REDACT_IMAGE_REMOVE,

	/* If the image intrudes across the redaction region (even if clipped),
	 * replace the bit that intrudes with black pixels. */
	PDF_REDACT_IMAGE_PIXELS,

	/* If the image, when clipped, intrudes across the redaction
	 * region, remove it completely. Note: clipped is a rough estimate
	 * based on the bbox of clipping paths.
	 *
	 * Essentially this says "remove any image that has visible parts
	 * that extend into the redaction region".
	 *
	 * This method can effectively 'leak' invisible information during
	 * the redaction phase, so should be used with caution.
	 */
	PDF_REDACT_IMAGE_REMOVE_UNLESS_INVISIBLE
};

enum {
	PDF_REDACT_LINE_ART_NONE,
	PDF_REDACT_LINE_ART_REMOVE_IF_COVERED,
	PDF_REDACT_LINE_ART_REMOVE_IF_TOUCHED
};

enum {
	/* Remove any text that overlaps with the redaction region,
	 * however slightly. This is the default option, and is the
	 * correct option for secure behaviour. */
	PDF_REDACT_TEXT_REMOVE,
	/* Do not remove any text at all as part of this redaction
	 * operation. Using this option is INSECURE! Use at your own
	 * risk. */
	PDF_REDACT_TEXT_NONE,
	/* Remove any invisible text that overlaps with the redaction
	 * region however slightly. This is intended to allow the
	 * removal of invisible text layers added by OCR passes.
	 * This will remove text that is made invisible by rendering
	 * mode, but will NOT remove other cases (like white-on-white
	 * text, etc). */
	PDF_REDACT_TEXT_REMOVE_INVISIBLE,
};

typedef struct
{
	int black_boxes;
	int image_method;
	int line_art;
	int text;
} pdf_redact_options;

int pdf_redact_page(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_redact_options *opts);

fz_transition *pdf_page_presentation(fz_context *ctx, pdf_page *page, fz_transition *transition, float *duration);

fz_default_colorspaces *pdf_load_default_colorspaces(fz_context *ctx, pdf_document *doc, pdf_page *page);

void pdf_clip_page(fz_context *ctx, pdf_page *page, fz_rect *clip);

void pdf_vectorize_page(fz_context *ctx, pdf_page *page);

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

/* Keep pdf_page, pdf_annot, and pdf_link structs in sync with underlying pdf objects. */
void pdf_sync_open_pages(fz_context *ctx, pdf_document *doc);
void pdf_sync_page(fz_context *ctx, pdf_page *page);
void pdf_sync_links(fz_context *ctx, pdf_page *page);
void pdf_sync_annots(fz_context *ctx, pdf_page *page);
void pdf_nuke_page(fz_context *ctx, pdf_page *page);
void pdf_nuke_links(fz_context *ctx, pdf_page *page);
void pdf_nuke_annots(fz_context *ctx, pdf_page *page);

#endif
