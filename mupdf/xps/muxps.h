#ifndef MUXPS_H
#define MUXPS_H

#include "fitz.h"

typedef struct xps_document_s xps_document;
typedef struct xps_page_s xps_page;

/*
	xps_open_document: Open a document.

	Open a document for reading so the library is able to locate
	objects and pages inside the file.

	The returned xps_document should be used when calling most
	other functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
xps_document *xps_open_document(fz_context *ctx, const char *filename);

/*
	xps_open_document_with_stream: Opens a document.

	Same as xps_open_document, but takes a stream instead of a
	filename to locate the document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
xps_document *xps_open_document_with_stream(fz_context *ctx, fz_stream *file);

/*
	xps_close_document: Closes and frees an opened document.

	The resource store in the context associated with xps_document
	is emptied.

	Does not throw exceptions.
*/
void xps_close_document(xps_document *doc);

int xps_count_pages(xps_document *doc);
xps_page *xps_load_page(xps_document *doc, int number);
fz_rect *xps_bound_page(xps_document *doc, xps_page *page, fz_rect *rect);
void xps_run_page(xps_document *doc, xps_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);
fz_link *xps_load_links(xps_document *doc, xps_page *page);
void xps_free_page(xps_document *doc, xps_page *page);

fz_outline *xps_load_outline(xps_document *doc);

#endif
