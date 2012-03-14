#ifndef MUXPS_H
#define MUXPS_H

#include "fitz.h"

typedef struct xps_document_s xps_document;
typedef struct xps_page_s xps_page;

/*
 * XML document model
 */

typedef struct element xml_element;

xml_element *xml_parse_document(fz_context *doc, unsigned char *buf, int len);
xml_element *xml_next(xml_element *item);
xml_element *xml_down(xml_element *item);
char *xml_tag(xml_element *item);
char *xml_att(xml_element *item, const char *att);
void xml_free_element(fz_context *doc, xml_element *item);
void xml_print_element(xml_element *item, int level);
void xml_detach(xml_element *node);

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
xps_document *xps_open_document(fz_context *ctx, char *filename);

/*
	xps_open_document_with_stream: Opens a document.

	Same as xps_open_document, but takes a stream instead of a
	filename to locate the document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
xps_document *xps_open_document_with_stream(fz_stream *file);

/*
	xps_close_document: Closes and frees an opened document.

	The resource store in the context associated with xps_document
	is emptied.

	Does not throw exceptions.
*/
void xps_close_document(xps_document *doc);

int xps_count_pages(xps_document *doc);
xps_page *xps_load_page(xps_document *doc, int number);
fz_rect xps_bound_page(xps_document *doc, xps_page *page);
void xps_run_page(xps_document *doc, xps_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
fz_link *xps_load_links(xps_document *doc, xps_page *page);
void xps_free_page(xps_document *doc, xps_page *page);

fz_outline *xps_load_outline(xps_document *doc);

#endif
