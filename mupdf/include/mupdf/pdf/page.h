#ifndef MUPDF_PDF_PAGE_H
#define MUPDF_PDF_PAGE_H

#include "mupdf/pdf/interpret.h"

int pdf_lookup_page_number(fz_context *ctx, pdf_document *doc, pdf_obj *pageobj);
int pdf_count_pages(fz_context *ctx, pdf_document *doc);
int pdf_count_pages_imp(fz_context *ctx, fz_document *doc, int chapter);
pdf_obj *pdf_lookup_page_obj(fz_context *ctx, pdf_document *doc, int needle);
void pdf_load_page_tree(fz_context *ctx, pdf_document *doc);
void pdf_drop_page_tree(fz_context *ctx, pdf_document *doc);

/*
	Find the page number of a named destination.

	For use with looking up the destination page of a fragment
	identifier in hyperlinks: foo.pdf#bar or foo.pdf#page=5.
*/
int pdf_lookup_anchor(fz_context *ctx, pdf_document *doc, const char *name, float *xp, float *yp);

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

void pdf_page_obj_transform(fz_context *ctx, pdf_obj *pageobj, fz_rect *page_mediabox, fz_matrix *page_ctm);
void pdf_page_transform(fz_context *ctx, pdf_page *page, fz_rect *mediabox, fz_matrix *ctm);
pdf_obj *pdf_page_resources(fz_context *ctx, pdf_page *page);
pdf_obj *pdf_page_contents(fz_context *ctx, pdf_page *page);
pdf_obj *pdf_page_group(fz_context *ctx, pdf_page *page);

/*
	Get the separation details for a page.
*/
fz_separations *pdf_page_separations(fz_context *ctx, pdf_page *page);

void pdf_read_ocg(fz_context *ctx, pdf_document *doc);
void pdf_drop_ocg(fz_context *ctx, pdf_document *doc);
int pdf_is_hidden_ocg(fz_context *ctx, pdf_ocg_descriptor *desc, pdf_obj *rdb, const char *usage, pdf_obj *ocg);

fz_link *pdf_load_links(fz_context *ctx, pdf_page *page);

/*
	Determine the size of a page.

	Determine the page size in user space units, taking page rotation
	into account. The page size is taken to be the crop box if it
	exists (visible area after cropping), otherwise the media box will
	be used (possibly including printing marks).
*/
fz_rect pdf_bound_page(fz_context *ctx, pdf_page *page);

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
void pdf_run_page_with_usage(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_device *dev, fz_matrix ctm, const char *usage, fz_cookie *cookie);

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

void pdf_filter_page_contents(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_filter_options *filter);
void pdf_filter_annot_contents(fz_context *ctx, pdf_document *doc, pdf_annot *annot, pdf_filter_options *filter);

typedef struct
{
	int no_black_boxes;
	int keep_images;
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
	pdf_document *doc;
	pdf_obj *obj;

	int transparency;
	int overprint;

	fz_link *links;
	pdf_annot *annots, **annot_tailp;
	pdf_widget *widgets, **widget_tailp;
};

#endif
