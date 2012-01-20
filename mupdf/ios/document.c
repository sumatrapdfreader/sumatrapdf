#include "fitz/fitz.h"
#include "pdf/mupdf.h"
#include "xps/muxps.h"
#include "document.h"

#include <ctype.h> /* for tolower() */

struct document *
open_document(fz_context *ctx, char *filename)
{
	struct document *doc;

	doc = fz_malloc_struct(ctx, struct document);
	memset(doc, 0, sizeof *doc);
	doc->ctx = ctx;
	doc->number = -1;

	fz_try(ctx)
	{
		if (strstr(filename, ".pdf") || strstr(filename, ".PDF"))
		{
			doc->pdf = pdf_open_xref(ctx, filename);
			pdf_load_page_tree(doc->pdf);
		}
		else if (strstr(filename, ".xps") || strstr(filename, ".XPS"))
		{
			doc->xps = xps_open_file(ctx, filename);
		}
		else
		{
			fz_throw(ctx, "unknown document format");
		}
	}
	fz_catch(ctx)
	{
		close_document(doc);
		return NULL;
	}

	return doc;
}

int
needs_password(struct document *doc)
{
	if (doc->pdf)
		return pdf_needs_password(doc->pdf);
	return 0;
}

int
authenticate_password(struct document *doc, char *password)
{
	if (doc->pdf)
		return pdf_authenticate_password(doc->pdf, password);
	return 1;
}

fz_outline *
load_outline(struct document *doc)
{
	fz_outline *outline;
	fz_var(outline);
	fz_try (doc->ctx)
	{
		if (doc->pdf)
			outline = pdf_load_outline(doc->pdf);
		else if (doc->xps)
			outline = xps_load_outline(doc->xps);
		else
			outline = NULL;
	}
	fz_catch (doc->ctx)
	{
		outline = NULL;
	}
	return outline;
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
	if (doc->number == number)
		return;
	fz_try (doc->ctx)
	{
		doc->number = number;
		if (doc->pdf) {
			if (doc->pdf_page)
				pdf_free_page(doc->ctx, doc->pdf_page);
			doc->pdf_page = NULL;
			doc->pdf_page = pdf_load_page(doc->pdf, number);
		}
		if (doc->xps) {
			if (doc->xps_page)
				xps_free_page(doc->xps, doc->xps_page);
			doc->xps_page = NULL;
			doc->xps_page = xps_load_page(doc->xps, number);
		}
	}
	fz_catch (doc->ctx)
	{
		fprintf(stderr, "cannot load page %d", number);
	}
}

void
measure_page(struct document *doc, int number, float *w, float *h)
{
	load_page(doc, number);
	if (doc->pdf_page) {
		fz_rect bounds = pdf_bound_page(doc->pdf, doc->pdf_page);
		*w = bounds.x1 - bounds.x0;
		*h = bounds.y1 - bounds.y0;
	}
	else if (doc->xps_page) {
		fz_rect bounds = xps_bound_page(doc->xps, doc->xps_page);
		*w = bounds.x1 - bounds.x0;
		*h = bounds.y1 - bounds.y0;
	}
	else {
		*w = *h = 72;
	}
	fz_flush_warnings(doc->ctx);
}

void
draw_page(struct document *doc, int number, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	load_page(doc, number);
	fz_try (doc->ctx)
	{
		if (doc->pdf_page) {
			pdf_run_page(doc->pdf, doc->pdf_page, dev, ctm, cookie);
		} else if (doc->xps_page) {
			xps_run_page(doc->xps, doc->xps_page, dev, ctm, cookie);
		}
	}
	fz_catch (doc->ctx)
	{
		fprintf(stderr, "cannot draw page %d", number);
	}
	fz_flush_warnings(doc->ctx);
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
search_page(struct document *doc, int number, char *needle, fz_cookie *cookie)
{
	int pos, len, i, n;

	if (strlen(needle) == 0)
		return 0;

	fz_text_span *text = fz_new_text_span(doc->ctx);
	fz_device *dev = fz_new_text_device(doc->ctx, text);
	draw_page(doc, number, dev, fz_identity, cookie);
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

	fz_free_text_span(doc->ctx, text);
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
			pdf_free_page(doc->ctx, doc->pdf_page);
		pdf_free_xref(doc->pdf);
	}
	if (doc->xps) {
		if (doc->xps_page)
			xps_free_page(doc->xps, doc->xps_page);
		xps_free_context(doc->xps);
	}
	fz_flush_warnings(doc->ctx);
	fz_free(doc->ctx, doc);
}
