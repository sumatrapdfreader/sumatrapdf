#ifndef MUPDF_PDF_PAGE_H
#define MUPDF_PDF_PAGE_H

int pdf_lookup_page_number(pdf_document *doc, pdf_obj *pageobj);
int pdf_count_pages(pdf_document *doc);
pdf_obj *pdf_lookup_page_obj(pdf_document *doc, int needle);
/* SumatraPDF: make pdf_lookup_inherited_page_item externally available */
pdf_obj *pdf_lookup_inherited_page_item(pdf_document *doc, pdf_obj *node, const char *key);

/*
	pdf_load_page: Load a page and its resources.

	Locates the page in the PDF document and loads the page and its
	resources. After pdf_load_page is it possible to retrieve the size
	of the page using pdf_bound_page, or to render the page using
	pdf_run_page_*.

	number: page number, where 0 is the first page of the document.
*/
pdf_page *pdf_load_page(pdf_document *doc, int number);

/* SumatraPDF: allow working around broken pdf_lookup_page_obj */
pdf_page *pdf_load_page_by_obj(pdf_document *doc, int number, pdf_obj *page_obj);

fz_link *pdf_load_links(pdf_document *doc, pdf_page *page);

/*
	pdf_bound_page: Determine the size of a page.

	Determine the page size in user space units, taking page rotation
	into account. The page size is taken to be the crop box if it
	exists (visible area after cropping), otherwise the media box will
	be used (possibly including printing marks).

	Does not throw exceptions.
*/
fz_rect *pdf_bound_page(pdf_document *doc, pdf_page *page, fz_rect *);

/*
	pdf_free_page: Frees a page and its resources.

	Does not throw exceptions.
*/
void pdf_free_page(pdf_document *doc, pdf_page *page);

/*
	pdf_run_page: Interpret a loaded page and render it on a device.

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_page(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

void pdf_run_page_with_usage(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, char *event, fz_cookie *cookie);

/*
	pdf_run_page_contents: Interpret a loaded page and render it on a device.
	Just the main page contents without the annotations

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_page_contents(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

/*
	Presentation interface.
*/
fz_transition *pdf_page_presentation(pdf_document *doc, pdf_page *page, float *duration);

/*
 * Page tree, pages and related objects
 */

struct pdf_page_s
{
	fz_matrix ctm; /* calculated from mediabox and rotate */
	fz_rect mediabox;
	int rotate;
	int transparency;
	pdf_obj *resources;
	pdf_obj *contents;
	fz_link *links;
	pdf_annot *annots;
	pdf_annot **annot_tailp;
	pdf_annot *changed_annots;
	pdf_annot *deleted_annots;
	pdf_annot *tmp_annots;
	pdf_obj *me;
	float duration;
	int transition_present;
	fz_transition transition;
	int incomplete;
};

enum
{
	PDF_PAGE_INCOMPLETE_CONTENTS = 1,
	PDF_PAGE_INCOMPLETE_ANNOTS = 2
};

#endif
