#ifndef MUPDF_CBZ_H
#define MUPDF_CBZ_H

#include "mupdf/fitz.h"

typedef struct cbz_document_s cbz_document;
typedef struct cbz_page_s cbz_page;

/*
	cbz_open_document: Open a document.

	Open a document for reading so the library is able to locate
	objects and pages inside the file.

	The returned cbz_document should be used when calling most
	other functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
cbz_document *cbz_open_document(fz_context *ctx, const char *filename);

/*
	cbz_open_document_with_stream: Opens a document.

	Same as cbz_open_document, but takes a stream instead of a
	filename to locate the document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
cbz_document *cbz_open_document_with_stream(fz_context *ctx, fz_stream *file);

/*
	cbz_close_document: Closes and frees an opened document.

	The resource store in the context associated with cbz_document
	is emptied.

	Does not throw exceptions.
*/
void cbz_close_document(cbz_document *doc);

int cbz_count_pages(cbz_document *doc);
cbz_page *cbz_load_page(cbz_document *doc, int number);
fz_rect *cbz_bound_page(cbz_document *doc, cbz_page *page, fz_rect *rect);
void cbz_free_page(cbz_document *doc, cbz_page *page);
void cbz_run_page(cbz_document *doc, cbz_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

#endif
