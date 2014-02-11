#ifndef MUPDF_TIFF_H
#define MUPDF_TIFF_H

#include "mupdf/fitz.h"

typedef struct tiff_document_s tiff_document;
typedef struct tiff_page_s tiff_page;

/*
	tiff_open_document: Open a document.

	Open a document for reading so the library is able to locate
	objects and pages inside the file.

	The returned tiff_document should be used when calling most
	other functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
tiff_document *tiff_open_document(fz_context *ctx, const char *filename);

/*
	tiff_open_document_with_stream: Opens a document.

	Same as tiff_open_document, but takes a stream instead of a
	filename to locate the document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
tiff_document *tiff_open_document_with_stream(fz_context *ctx, fz_stream *file);

/*
	tiff_close_document: Closes and frees an opened document.

	The resource store in the context associated with tiff_document
	is emptied.

	Does not throw exceptions.
*/
void tiff_close_document(tiff_document *doc);

int tiff_count_pages(tiff_document *doc);
tiff_page *tiff_load_page(tiff_document *doc, int number);
fz_rect *tiff_bound_page(tiff_document *doc, tiff_page *page, fz_rect *rect);
void tiff_free_page(tiff_document *doc, tiff_page *page);
void tiff_run_page(tiff_document *doc, tiff_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

#endif
