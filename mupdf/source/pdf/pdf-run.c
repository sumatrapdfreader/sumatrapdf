#include "pdf-interpret-imp.h"

static void
pdf_run_annot_with_usage(pdf_document *doc, pdf_page *page, pdf_annot *annot, fz_device *dev, const fz_matrix *ctm, char *event, fz_cookie *cookie)
{
	fz_matrix local_ctm;
	pdf_process process;

	fz_concat(&local_ctm, &page->ctm, ctm);

	pdf_process_run(&process, dev, &local_ctm, event, NULL, 0);

	pdf_process_annot(doc, page, annot, &process, cookie);
}

static void pdf_run_page_contents_with_usage(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, char *event, fz_cookie *cookie)
{
	fz_matrix local_ctm;
	pdf_process process;

	fz_concat(&local_ctm, &page->ctm, ctm);

	if (page->transparency)
	{
		fz_rect mediabox = page->mediabox;
		fz_begin_group(dev, fz_transform_rect(&mediabox, &local_ctm), 1, 0, 0, 1);
	}

	pdf_process_run(&process, dev, &local_ctm, event, NULL, 0);

	pdf_process_stream_object(doc, page->contents, &process, page->resources, cookie);

	if (page->transparency)
		fz_end_group(dev);
}

void pdf_run_page_contents(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	int nocache = !!(dev->hints & FZ_NO_CACHE);

	if (nocache)
		pdf_mark_xref(doc);
	fz_try(ctx)
	{
		pdf_run_page_contents_with_usage(doc, page, dev, ctm, "View", cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	if (page->incomplete & PDF_PAGE_INCOMPLETE_CONTENTS)
		fz_throw(doc->ctx, FZ_ERROR_TRYLATER, "incomplete rendering");
}


void pdf_run_annot(pdf_document *doc, pdf_page *page, pdf_annot *annot, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	int nocache = !!(dev->hints & FZ_NO_CACHE);

	if (nocache)
		pdf_mark_xref(doc);
	fz_try(ctx)
	{
		pdf_run_annot_with_usage(doc, page, annot, dev, ctm, "View", cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	if (page->incomplete & PDF_PAGE_INCOMPLETE_ANNOTS)
		fz_throw(doc->ctx, FZ_ERROR_TRYLATER, "incomplete rendering");
}

static void pdf_run_page_annots_with_usage(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, char *event, fz_cookie *cookie)
{
	pdf_annot *annot;

	if (cookie && cookie->progress_max != -1)
	{
		int count = 1;
		for (annot = page->annots; annot; annot = annot->next)
			count++;
		cookie->progress_max += count;
	}

	for (annot = page->annots; annot; annot = annot->next)
	{
		/* Check the cookie for aborting */
		if (cookie)
		{
			if (cookie->abort)
				break;
			cookie->progress++;
		}

		pdf_run_annot_with_usage(doc, page, annot, dev, ctm, event, cookie);
	}
}

void
pdf_run_page_with_usage(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, char *event, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	int nocache = !!(dev->hints & FZ_NO_CACHE);

	if (nocache)
		pdf_mark_xref(doc);
	fz_try(ctx)
	{
		pdf_run_page_contents_with_usage(doc, page, dev, ctm, event, cookie);
		pdf_run_page_annots_with_usage(doc, page, dev, ctm, event, cookie);
	}
	fz_always(ctx)
	{
		if (nocache)
			pdf_clear_xref_to_mark(doc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	if (page->incomplete)
		fz_throw(doc->ctx, FZ_ERROR_TRYLATER, "incomplete rendering");
}

void
pdf_run_page(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie)
{
	pdf_run_page_with_usage(doc, page, dev, ctm, "View", cookie);
}

void
pdf_run_glyph(pdf_document *doc, pdf_obj *resources, fz_buffer *contents, fz_device *dev, const fz_matrix *ctm, void *gstate, int nested_depth)
{
	fz_context *ctx = doc->ctx;
	pdf_process process;

	if (nested_depth > 10)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Too many nestings of Type3 glyphs");

	pdf_process_run(&process, dev, ctm, "View", gstate, nested_depth+1);

	pdf_process_glyph(doc, resources, contents, &process);
}
