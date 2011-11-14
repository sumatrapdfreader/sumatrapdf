#ifndef _DOCUMENT_H_
#define _DOCUMENT_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before document.h"
#endif

#ifndef _MUPDF_H_
#error "mupdf.h must be included before document.h"
#endif

#ifndef _MUXPS_H_
#error "muxps.h must be included before document.h"
#endif

struct document
{
	pdf_xref *pdf;
	xps_context *xps;
	int number;
	pdf_page *pdf_page;
	xps_page *xps_page;
};

struct document *open_document(char *filename);
fz_outline *load_outline(struct document *doc);
int count_pages(struct document *doc);
void measure_page(struct document *doc, int number, float *w, float *h);
void draw_page(struct document *doc, int number, fz_device *dev, fz_matrix ctm);
void close_document(struct document *doc);

#endif
