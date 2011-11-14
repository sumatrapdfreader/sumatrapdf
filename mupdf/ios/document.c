#include "fitz/fitz.h"
#include "pdf/mupdf.h"
#include "xps/muxps.h"
#include "document.h"

struct document *
open_document(char *filename)
{
	fz_error error;

	if (strstr(filename, ".pdf") || strstr(filename, ".PDF")) {
		struct document *doc = fz_malloc(sizeof *doc);
		memset(doc, 0, sizeof *doc);
		doc->number = -1;
		error = pdf_open_xref(&doc->pdf, filename, "");
		if (error) {
			fz_free(doc);
			fz_rethrow(error, "cannot open pdf document");
			return NULL;
		}
		error = pdf_load_page_tree(doc->pdf);
		if (error) {
			pdf_free_xref(doc->pdf);
			fz_free(doc);
			fz_rethrow(error, "cannot open pdf document");
			return NULL;
		}
		return doc;
	} else if (strstr(filename, ".xps") || strstr(filename, ".XPS")) {
		struct document *doc = fz_malloc(sizeof *doc);
		memset(doc, 0, sizeof *doc);
		doc->number = -1;
		error = xps_open_file(&doc->xps, filename);
		if (error) {
			fz_free(doc);
			fz_rethrow(error, "cannot open xps document");
			return NULL;
		}
		return doc;
	} else {
		fz_throw("unknown document format");
		return NULL;
	}
}

fz_outline *
load_outline(struct document *doc)
{
	if (doc->pdf)
		return pdf_load_outline(doc->pdf);
	else if (doc->xps)
		return xps_load_outline(doc->xps);
	else
		return NULL;
}

int
count_pages(struct document *doc)
{
	if (doc->pdf)
		return pdf_count_pages(doc->pdf);
	else if (doc->xps)
		return xps_count_pages(doc->xps);
	else
		return 1;
}

static void
load_page(struct document *doc, int number)
{
	fz_error error;
	if (doc->number == number)
		return;
	doc->number = number;
	if (doc->pdf) {
		if (doc->pdf_page) {
			pdf_age_store(doc->pdf->store, 1);
			pdf_free_page(doc->pdf_page);
		}
		doc->pdf_page = NULL;
printf("load pdf page %d\n", number);
		error = pdf_load_page(&doc->pdf_page, doc->pdf, number);
		if (error)
			fz_catch(error, "cannot load page %d", number);
	}
	if (doc->xps) {
		if (doc->xps_page)
			xps_free_page(doc->xps, doc->xps_page);
		doc->xps_page = NULL;
printf("load xps page %d\n", number);
		error = xps_load_page(&doc->xps_page, doc->xps, number);
		if (error)
			fz_catch(error, "cannot load page %d", number);
	}
}

void
measure_page(struct document *doc, int number, float *w, float *h)
{
	load_page(doc, number);
	if (doc->pdf_page) {
		pdf_page *page = doc->pdf_page;
		fz_rect mediabox = fz_transform_rect(fz_rotate(page->rotate), page->mediabox);
		*w = mediabox.x1 - mediabox.x0;
		*h = mediabox.y1 - mediabox.y0;
	}
	else if (doc->xps_page) {
		xps_page *page = doc->xps_page;
		*w = page->width * 72.0f / 96.0f;
		*h = page->height * 72.0f / 96.0f;
	}
	else {
		*w = *h = 72;
	}
	fz_flush_warnings();
}

void
draw_page(struct document *doc, int number, fz_device *dev, fz_matrix ctm)
{
	load_page(doc, number);
	if (doc->pdf_page) {
		pdf_page *page = doc->pdf_page;
		fz_matrix page_ctm = fz_concat(fz_rotate(-page->rotate), fz_scale(1, -1));
		fz_rect mediabox = fz_transform_rect(page_ctm, page->mediabox);
		page_ctm = fz_concat(page_ctm, fz_translate(-mediabox.x0, -mediabox.y0));
		ctm = fz_concat(page_ctm, ctm);
		pdf_run_page(doc->pdf, page, dev, ctm);
	} else if (doc->xps_page) {
		xps_page *page = doc->xps_page;
		fz_matrix page_ctm = fz_scale(72.0f / 96.0f, 72.0f / 96.0f);
		ctm = fz_concat(page_ctm, ctm);
		doc->xps->dev = dev;
		xps_parse_fixed_page(doc->xps, ctm, page);
		doc->xps->dev = NULL;
	}
	fz_flush_warnings();
}

void
close_document(struct document *doc)
{
	if (doc->pdf) {
		if (doc->pdf_page)
			pdf_free_page(doc->pdf_page);
		if (doc->pdf->store)
			pdf_free_store(doc->pdf->store);
		doc->pdf->store = NULL;
		pdf_free_xref(doc->pdf);
	}
	if (doc->xps) {
		if (doc->xps_page)
			xps_free_page(doc->xps, doc->xps_page);
		xps_free_context(doc->xps);
	}
	fz_flush_warnings();
}
