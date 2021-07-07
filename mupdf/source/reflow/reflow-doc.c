#include "mupdf/fitz.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

#define DEF_WIDTH 612
#define DEF_HEIGHT 792
#define DEF_FONTSIZE 12

typedef struct
{
	fz_document base;

	fz_document *underdoc;
	fz_stext_options opts;
	float w;
	float h;
	float em;
} reflow_document;

typedef struct {
	fz_page base;

	fz_document *html_doc;
	fz_page *html_page;
} reflow_page;

static void
reflow_drop_document_imp(fz_context *ctx, reflow_document *doc)
{
	fz_defer_reap_start(ctx);

	fz_drop_document(ctx, doc->underdoc);
	fz_defer_reap_end(ctx);
}

static fz_colorspace *
reflow_document_output_intent(fz_context *ctx, reflow_document *doc)
{
	return fz_document_output_intent(ctx, doc->underdoc);
}

static int
reflow_needs_password(fz_context *ctx, reflow_document *doc)
{
	return fz_needs_password(ctx, doc->underdoc);
}

static int
reflow_authenticate_password(fz_context *ctx, reflow_document *doc, const char *password)
{
	return fz_authenticate_password(ctx, doc->underdoc, password);
}

static int
reflow_has_permission(fz_context *ctx, reflow_document *doc, fz_permission permission)
{
	return fz_has_permission(ctx, doc->underdoc, permission);
}

static fz_outline *
reflow_load_outline(fz_context *ctx, reflow_document *doc)
{
	return fz_load_outline(ctx, doc->underdoc);
}

static fz_location
reflow_resolve_link(fz_context *ctx, reflow_document *doc, const char *uri, float *xp, float *yp)
{
	return fz_resolve_link(ctx, doc->underdoc, uri, xp, yp);
}

static int
reflow_count_pages(fz_context *ctx, reflow_document *doc, int chapter)
{
	return fz_count_chapter_pages(ctx, doc->underdoc, chapter);
}

static fz_rect
reflow_bound_page(fz_context *ctx, fz_page *page_)
{
	reflow_page *page = (reflow_page *)page_;

	return fz_bound_page(ctx, page->html_page);
}

static void
reflow_drop_page(fz_context *ctx, fz_page *page_)
{
	reflow_page *page = (reflow_page *)page_;

	fz_drop_page(ctx, page->html_page);
	fz_drop_document(ctx, page->html_doc);
}

static void
reflow_run_page_contents(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	reflow_page *page = (reflow_page *)page_;

	fz_run_page_contents(ctx, page->html_page, dev, transform, cookie);
}

static void
reflow_run_page_annots(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	reflow_page *page = (reflow_page *)page_;

	fz_run_page_annots(ctx, page->html_page, dev, transform, cookie);
}

static void
reflow_run_page_widgets(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	reflow_page *page = (reflow_page *)page_;

	fz_run_page_widgets(ctx, page->html_page, dev, transform, cookie);
}

static fz_page *
reflow_load_page(fz_context *ctx, reflow_document *doc, int chapter, int pagenum)
{
	fz_buffer *buf = NULL;
	fz_stext_page *text = NULL;
	fz_stext_options default_opts = { FZ_STEXT_PRESERVE_IMAGES | FZ_STEXT_DEHYPHENATE };
	reflow_page *page = NULL;
	fz_stream *stm = NULL;
	fz_output *out = NULL;

	page = fz_new_derived_page(ctx, reflow_page, (fz_document *)doc);
	page->base.bound_page = reflow_bound_page;
	page->base.drop_page = reflow_drop_page;
	page->base.run_page_contents = reflow_run_page_contents;
	page->base.run_page_annots = reflow_run_page_annots;
	page->base.run_page_widgets = reflow_run_page_widgets;

	fz_var(buf);
	fz_var(out);
	fz_var(text);
	fz_var(stm);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 8192);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_header_as_xhtml(ctx, out);

		text = fz_new_stext_page_from_chapter_page_number(ctx, doc->underdoc, chapter, pagenum, &default_opts);
		fz_print_stext_page_as_xhtml(ctx, out, text, pagenum+1); /* pagenum is not right w.r.t chapter. */
		fz_drop_stext_page(ctx, text);
		text = NULL;

		fz_print_stext_trailer_as_xhtml(ctx, out);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);

		stm = fz_open_buffer(ctx, buf);
		page->html_doc = fz_open_document_with_stream(ctx, "application/xhtml+xml", stm);
		fz_layout_document(ctx, page->html_doc, doc->w, 0, doc->em);
		page->html_page = fz_load_chapter_page(ctx, page->html_doc, 0, 0);
	}
	fz_always(ctx)
	{
		fz_drop_stext_page(ctx, text);
		fz_drop_output(ctx, out);
		fz_drop_stream(ctx, stm);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_drop_page(ctx, &page->base);
		fz_rethrow(ctx);
	}

	return &page->base;
}

static int reflow_lookup_metadata(fz_context *ctx, reflow_document *doc, const char *key, char *buf, int size)
{
	return fz_lookup_metadata(ctx, doc->underdoc, key, buf, size);
}

static void reflow_layout(fz_context *ctx, reflow_document *doc, float w, float h, float em)
{
	reflow_page *page;
	fz_page *kept = NULL;
	fz_page *dropme = NULL;

	if (doc->w == w && doc->h == h && doc->em == em)
		return;
	doc->w = w;
	doc->h = h;
	doc->em = em;

	fz_var(kept);
	fz_var(dropme);
	fz_var(page);
	fz_try(ctx)
	{
		/* We can only walk the page list while the alloc lock is taken, so gymnastics are required. */
		/* Loop invariant: at any point where we might throw, kept != NULL iff we are unlocked. */
		fz_lock(ctx, FZ_LOCK_ALLOC);
		for (page = (reflow_page *)doc->base.open; page != NULL; page = (reflow_page *)page->base.next)
		{
			/* Keep an extra reference to the page so that no other thread can remove it. */
			kept = fz_keep_page_locked(ctx, (fz_page *)page);
			fz_unlock(ctx, FZ_LOCK_ALLOC);
			/* Drop any extra reference we might still have to a previous page. */
			fz_drop_page(ctx, dropme);
			dropme = NULL;
			/* Layout the page. */
			fz_layout_document(ctx, page->html_doc, doc->w, 0, doc->em);
			/* We can't drop kept here, because that would give us a race condition with
			 * us taking the lock and hoping that 'page' would still be valid. So remember it
			 * for dropping later. */
			dropme = kept;
			kept = NULL;
			fz_lock(ctx, FZ_LOCK_ALLOC);
		}
		/* unlock (and final drop of dropme) happens in the always. */
	}
	fz_always(ctx)
	{
		if (kept == NULL)
			fz_unlock(ctx, FZ_LOCK_ALLOC);
		fz_drop_page(ctx, kept);
		fz_drop_page(ctx, dropme);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

fz_document *
fz_open_reflowed_document(fz_context *ctx, fz_document *underdoc, const fz_stext_options *opts)
{
	reflow_document *doc = fz_new_derived_document(ctx, reflow_document);

	doc->base.drop_document = (fz_document_drop_fn*)reflow_drop_document_imp;
	doc->base.get_output_intent = (fz_document_output_intent_fn*)reflow_document_output_intent;
	doc->base.needs_password = (fz_document_needs_password_fn*)reflow_needs_password;
	doc->base.authenticate_password = (fz_document_authenticate_password_fn*)reflow_authenticate_password;
	doc->base.has_permission = (fz_document_has_permission_fn*)reflow_has_permission;
	doc->base.load_outline = (fz_document_load_outline_fn*)reflow_load_outline;
	doc->base.resolve_link = (fz_document_resolve_link_fn *)reflow_resolve_link;
	doc->base.count_pages = (fz_document_count_pages_fn *)reflow_count_pages;
	doc->base.load_page = (fz_document_load_page_fn *)reflow_load_page;
	doc->base.lookup_metadata = (fz_document_lookup_metadata_fn*)reflow_lookup_metadata;
	doc->base.layout = (fz_document_layout_fn *)reflow_layout;

	doc->underdoc = fz_keep_document(ctx, underdoc);
	doc->opts = *opts;

	doc->w = DEF_WIDTH;
	doc->h = DEF_HEIGHT;
	doc->em = DEF_FONTSIZE;

	return &doc->base;
}
