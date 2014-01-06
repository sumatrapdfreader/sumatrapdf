#ifndef MUPDF_FITZ_DOCUMENT_H
#define MUPDF_FITZ_DOCUMENT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/device.h"
#include "mupdf/fitz/transition.h"
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/outline.h"

/*
	Document interface
*/
typedef struct fz_document_s fz_document;
typedef struct fz_page_s fz_page;
typedef struct fz_annot_s fz_annot;

// TODO: move out of this interface (it's pdf specific)
typedef struct fz_write_options_s fz_write_options;

typedef void (fz_document_close_fn)(fz_document *doc);
typedef int (fz_document_needs_password_fn)(fz_document *doc);
typedef int (fz_document_authenticate_password_fn)(fz_document *doc, const char *password);
typedef fz_outline *(fz_document_load_outline_fn)(fz_document *doc);
typedef int (fz_document_count_pages_fn)(fz_document *doc);
typedef fz_page *(fz_document_load_page_fn)(fz_document *doc, int number);
typedef fz_link *(fz_document_load_links_fn)(fz_document *doc, fz_page *page);
typedef fz_rect *(fz_document_bound_page_fn)(fz_document *doc, fz_page *page, fz_rect *);
typedef void (fz_document_run_page_contents_fn)(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);
typedef void (fz_document_run_annot_fn)(fz_document *doc, fz_page *page, fz_annot *annot, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);
typedef void (fz_document_free_page_fn)(fz_document *doc, fz_page *page);
typedef int (fz_document_meta_fn)(fz_document *doc, int key, void *ptr, int size);
typedef fz_transition *(fz_document_page_presentation_fn)(fz_document *doc, fz_page *page, float *duration);
typedef fz_annot *(fz_document_first_annot_fn)(fz_document *doc, fz_page *page);
typedef fz_annot *(fz_document_next_annot_fn)(fz_document *doc, fz_annot *annot);
typedef fz_rect *(fz_document_bound_annot_fn)(fz_document *doc, fz_annot *annot, fz_rect *rect);
typedef void (fz_document_write_fn)(fz_document *doc, char *filename, fz_write_options *opts);
typedef void (fz_document_rebind_fn)(fz_document *doc, fz_context *ctx);

struct fz_document_s
{
	fz_document_close_fn *close;
	fz_document_needs_password_fn *needs_password;
	fz_document_authenticate_password_fn *authenticate_password;
	fz_document_load_outline_fn *load_outline;
	fz_document_count_pages_fn *count_pages;
	fz_document_load_page_fn *load_page;
	fz_document_load_links_fn *load_links;
	fz_document_bound_page_fn *bound_page;
	fz_document_run_page_contents_fn *run_page_contents;
	fz_document_run_annot_fn *run_annot;
	fz_document_free_page_fn *free_page;
	fz_document_meta_fn *meta;
	fz_document_page_presentation_fn *page_presentation;
	fz_document_first_annot_fn *first_annot;
	fz_document_next_annot_fn *next_annot;
	fz_document_bound_annot_fn *bound_annot;
	fz_document_write_fn *write;
	fz_document_rebind_fn *rebind;
};

/*
	fz_open_document: Open a PDF, XPS or CBZ document.

	Open a document file and read its basic structure so pages and
	objects can be located. MuPDF will try to repair broken
	documents (without actually changing the file contents).

	The returned fz_document is used when calling most other
	document related functions. Note that it wraps the context, so
	those functions implicitly can access the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
fz_document *fz_open_document(fz_context *ctx, const char *filename);

/*
	fz_open_document_with_stream: Open a PDF, XPS or CBZ document.

	Open a document using the specified stream object rather than
	opening a file on disk.

	magic: a string used to detect document type; either a file name or mime-type.
*/
fz_document *fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream);

/*
	fz_close_document: Close and free an open document.

	The resource store in the context associated with fz_document
	is emptied, and any allocations for the document are freed.

	Does not throw exceptions.
*/
void fz_close_document(fz_document *doc);

/*
	fz_needs_password: Check if a document is encrypted with a
	non-blank password.

	Does not throw exceptions.
*/
int fz_needs_password(fz_document *doc);

/*
	fz_authenticate_password: Test if the given password can
	decrypt the document.

	password: The password string to be checked. Some document
	specifications do not specify any particular text encoding, so
	neither do we.

	Does not throw exceptions.
*/
int fz_authenticate_password(fz_document *doc, const char *password);

/*
	fz_load_outline: Load the hierarchical document outline.

	Should be freed by fz_free_outline.
*/
fz_outline *fz_load_outline(fz_document *doc);

/*
	fz_count_pages: Return the number of pages in document

	May return 0 for documents with no pages.
*/
int fz_count_pages(fz_document *doc);

/*
	fz_load_page: Load a page.

	After fz_load_page is it possible to retrieve the size of the
	page using fz_bound_page, or to render the page using
	fz_run_page_*. Free the page by calling fz_free_page.

	number: page number, 0 is the first page of the document.
*/
/* SumatraPDF: caution: fz_load_page uses cached page objects for XPS documents! */
fz_page *fz_load_page(fz_document *doc, int number);

/*
	fz_load_links: Load the list of links for a page.

	Returns a linked list of all the links on the page, each with
	its clickable region and link destination. Each link is
	reference counted so drop and free the list of links by
	calling fz_drop_link on the pointer return from fz_load_links.

	page: Page obtained from fz_load_page.
*/
fz_link *fz_load_links(fz_document *doc, fz_page *page);

/*
	fz_bound_page: Determine the size of a page at 72 dpi.

	Does not throw exceptions.
*/
fz_rect *fz_bound_page(fz_document *doc, fz_page *page, fz_rect *rect);

/*
	fz_run_page: Run a page through a device.

	page: Page obtained from fz_load_page.

	dev: Device obtained from fz_new_*_device.

	transform: Transform to apply to page. May include for example
	scaling and rotation, see fz_scale, fz_rotate and fz_concat.
	Set to fz_identity if no transformation is desired.

	cookie: Communication mechanism between caller and library
	rendering the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing rendering of a page. Cookie also
	communicates progress information back to the caller. The
	fields inside cookie are continually updated while the page is
	rendering.
*/
void fz_run_page(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);

/*
	fz_run_page_contents: Run a page through a device. Just the main
	page content, without the annotations, if any.

	page: Page obtained from fz_load_page.

	dev: Device obtained from fz_new_*_device.

	transform: Transform to apply to page. May include for example
	scaling and rotation, see fz_scale, fz_rotate and fz_concat.
	Set to fz_identity if no transformation is desired.

	cookie: Communication mechanism between caller and library
	rendering the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing rendering of a page. Cookie also
	communicates progress information back to the caller. The
	fields inside cookie are continually updated while the page is
	rendering.
*/
void fz_run_page_contents(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);

/*
	fz_run_annot: Run an annotation through a device.

	page: Page obtained from fz_load_page.

	annot: an annotation.

	dev: Device obtained from fz_new_*_device.

	transform: Transform to apply to page. May include for example
	scaling and rotation, see fz_scale, fz_rotate and fz_concat.
	Set to fz_identity if no transformation is desired.

	cookie: Communication mechanism between caller and library
	rendering the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing rendering of a page. Cookie also
	communicates progress information back to the caller. The
	fields inside cookie are continually updated while the page is
	rendering.
*/
void fz_run_annot(fz_document *doc, fz_page *page, fz_annot *annot, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie);

/*
	fz_free_page: Free a loaded page.

	Does not throw exceptions.
*/
void fz_free_page(fz_document *doc, fz_page *page);

/*
	fz_page_presentation: Get the presentation details for a given page.

	duration: NULL, or a pointer to a place to set the page duration in
	seconds. (Will be set to 0 if unspecified).

	Returns: a pointer to a transition structure, or NULL if there isn't
	one.

	Does not throw exceptions.
*/
fz_transition *fz_page_presentation(fz_document *doc, fz_page *page, float *duration);

void fz_rebind_document(fz_document *doc, fz_context *ctx);

#endif
