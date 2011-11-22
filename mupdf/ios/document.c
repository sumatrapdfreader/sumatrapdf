#include "fitz/fitz.h"
#include "pdf/mupdf.h"
#include "xps/muxps.h"
#include "document.h"

#include <ctype.h> // for tolower()

struct document *
open_document(char *filename)
{
	fz_error error;

	if (strstr(filename, ".pdf") || strstr(filename, ".PDF")) {
		struct document *doc = fz_malloc(sizeof *doc);
		memset(doc, 0, sizeof *doc);
		doc->number = -1;
		error = pdf_open_xref(&doc->pdf, filename, NULL);
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

int
needs_password(struct document *doc)
{
	if (doc->pdf) {
		return pdf_needs_password(doc->pdf);
	}
	return 0;
}

int
authenticate_password(struct document *doc, char *password)
{
	if (doc->pdf) {
		return pdf_authenticate_password(doc->pdf, password);
	}
	return 1;
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
		error = pdf_load_page(&doc->pdf_page, doc->pdf, number);
		if (error)
			fz_catch(error, "cannot load page %d", number);
	}
	if (doc->xps) {
		if (doc->xps_page)
			xps_free_page(doc->xps, doc->xps_page);
		doc->xps_page = NULL;
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

static int
charat(fz_text_span *span, int idx)
{
	int ofs = 0;
	while (span) {
		if (idx < ofs + span->len)
			return span->text[idx - ofs].c;
		if (span->eol) {
			if (idx == ofs + span->len)
				return ' ';
			ofs ++;
		}
		ofs += span->len;
		span = span->next;
	}
	return 0;
}

static fz_bbox
bboxat(fz_text_span *span, int idx)
{
	int ofs = 0;
	while (span) {
		if (idx < ofs + span->len)
			return span->text[idx - ofs].bbox;
		if (span->eol) {
			if (idx == ofs + span->len)
				return fz_empty_bbox;
			ofs ++;
		}
		ofs += span->len;
		span = span->next;
	}
	return fz_empty_bbox;
}

static int
textlen(fz_text_span *span)
{
	int len = 0;
	while (span) {
		len += span->len;
		if (span->eol)
			len ++;
		span = span->next;
	}
	return len;
}

static int
match(fz_text_span *span, char *s, int n)
{
	int start = n, c;
	while (*s) {
		s += chartorune(&c, s);
		if (c == ' ' && charat(span, n) == ' ') {
			while (charat(span, n) == ' ')
				n++;
		} else {
			if (tolower(c) != tolower(charat(span, n)))
				return 0;
			n++;
		}
	}
	return n - start;
}

int
search_page(struct document *doc, int number, char *needle)
{
	int pos, len, i, n;

	if (strlen(needle) == 0)
		return 0;

	fz_text_span *text = fz_new_text_span();
	fz_device *dev = fz_new_text_device(text);
	draw_page(doc, number, dev, fz_identity);
	fz_free_device(dev);

	doc->hit_count = 0;

	len = textlen(text);
	for (pos = 0; pos < len; pos++) {
		n = match(text, needle, pos);
		if (n) {
			for (i = 0; i < n; i++) {
				fz_bbox r = bboxat(text, pos + i);
				if (!fz_is_empty_bbox(r) && doc->hit_count < nelem(doc->hit_bbox))
					doc->hit_bbox[doc->hit_count++] = r;
			}
		}
	}

	fz_free_text_span(text);
	return doc->hit_count;
}

fz_bbox
search_result_bbox(struct document *doc, int i)
{
	return doc->hit_bbox[i];
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
