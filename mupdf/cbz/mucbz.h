#ifndef _MUCBZ_H_
#define _MUCBZ_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before mucbz.h"
#endif

typedef struct cbz_document_s cbz_document;
typedef struct cbz_page_s cbz_page;

cbz_document *cbz_open_document(fz_context *ctx, char *filename);
cbz_document *cbz_open_document_with_stream(fz_stream *file);
void cbz_close_document(cbz_document *doc);

int cbz_count_pages(cbz_document *doc);
cbz_page *cbz_load_page(cbz_document *doc, int number);
fz_rect cbz_bound_page(cbz_document *doc, cbz_page *page);
void cbz_free_page(cbz_document *doc, cbz_page *page);
void cbz_run_page(cbz_document *doc, cbz_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

#endif
