#ifndef MUIMAGE_H
#define MUIMAGE_H

#include "mupdf/fitz.h"

typedef struct image_document_s image_document;
typedef struct image_page_s image_page;

/*
	image_open_document: Open a document.

	Open a document for reading so the library is able to locate
	objects and pages inside the file.

	The returned image_document should be used when calling most
	other functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
image_document *image_open_document(fz_context *ctx, const char *filename);

/*
	image_open_document_with_stream: Opens a document.

	Same as image_open_document, but takes a stream instead of a
	filename to locate the document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
image_document *image_open_document_with_stream(fz_context *ctx, fz_stream *file);

/*
	image_close_document: Closes and frees an opened document.

	The resource store in the context associated with image_document
	is emptied.

	Does not throw exceptions.
*/
void image_close_document(image_document *doc);

int image_count_pages(image_document *doc);
image_page *image_load_page(image_document *doc, int number);
fz_rect *image_bound_page(image_document *doc, image_page *page, fz_rect *rect);
void image_free_page(image_document *doc, image_page *page);
void image_run_page(image_document *doc, image_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

#endif
