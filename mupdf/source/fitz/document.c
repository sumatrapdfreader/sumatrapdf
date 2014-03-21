#include "mupdf/fitz.h"

extern int pdf_js_supported(void);

enum
{
	FZ_DOCUMENT_HANDLER_MAX = 10
};

struct fz_document_handler_context_s
{
	int refs;
	int count;
	const fz_document_handler *handler[FZ_DOCUMENT_HANDLER_MAX];
};

void fz_new_document_handler_context(fz_context *ctx)
{
	ctx->handler = fz_malloc_struct(ctx, fz_document_handler_context);
	ctx->handler->refs = 1;
}

fz_document_handler_context *fz_keep_document_handler_context(fz_context *ctx)
{
	if (!ctx || !ctx->handler)
		return NULL;
	ctx->handler->refs++;
	return ctx->handler;
}

void fz_drop_document_handler_context(fz_context *ctx)
{
	if (!ctx || !ctx->handler)
		return;

	if (--ctx->handler->refs != 0)
		return;

	fz_free(ctx, ctx->handler);
	ctx->handler = NULL;
}

void fz_register_document_handler(fz_context *ctx, const fz_document_handler *handler)
{
	fz_document_handler_context *dc;
	int i;

	if (!ctx || !handler)
		return;

	dc = ctx->handler;
	if (dc == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Document handler list not found");

	for (i = 0; i < dc->count; i++)
		if (dc->handler[i] == handler)
			return;

	if (dc->count >= FZ_DOCUMENT_HANDLER_MAX)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Too many document handlers");

	dc->handler[dc->count++] = handler;
}

static inline int fz_tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	return c;
}

int fz_strcasecmp(const char *a, const char *b)
{
	while (fz_tolower(*a) == fz_tolower(*b))
	{
		if (*a++ == 0)
			return 0;
		b++;
	}
	return fz_tolower(*a) - fz_tolower(*b);
}

fz_document *
fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream)
{
	int i, score;
	int best_i, best_score;
	fz_document_handler_context *dc;

	if (ctx == NULL || magic == NULL || stream == NULL)
		return NULL;

	dc = ctx->handler;
	if (dc->count == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "No document handlers registered");

	best_i = -1;
	best_score = 0;
	for (i = 0; i < dc->count; i++)
	{
		score = dc->handler[i]->recognize(ctx, magic);
		if (best_score < score)
		{
			best_score = score;
			best_i = i;
		}
	}

	if (best_i >= 0)
		return dc->handler[best_i]->open_with_stream(ctx, stream);

	return NULL;
}

fz_document *
fz_open_document(fz_context *ctx, const char *filename)
{
	int i, score;
	int best_i, best_score;
	fz_document_handler_context *dc;

	if (ctx == NULL || filename == NULL)
		return NULL;

	dc = ctx->handler;
	if (dc->count == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "No document handlers registered");

	best_i = -1;
	best_score = 0;
	for (i = 0; i < dc->count; i++)
	{
		score = dc->handler[i]->recognize(ctx, filename);
		if (best_score < score)
		{
			best_score = score;
			best_i = i;
		}
	}

	if (best_i >= 0)
		return dc->handler[best_i]->open(ctx, filename);

	return NULL;
}

void
fz_close_document(fz_document *doc)
{
	if (doc && doc->close)
		doc->close(doc);
}

void
fz_rebind_document(fz_document *doc, fz_context *ctx)
{
	if (doc != NULL && doc->rebind != NULL)
		doc->rebind(doc, ctx);
}

int
fz_needs_password(fz_document *doc)
{
	if (doc && doc->needs_password)
		return doc->needs_password(doc);
	return 0;
}

int
fz_authenticate_password(fz_document *doc, const char *password)
{
	if (doc && doc->authenticate_password)
		return doc->authenticate_password(doc, password);
	return 1;
}

fz_outline *
fz_load_outline(fz_document *doc)
{
	if (doc && doc->load_outline)
		return doc->load_outline(doc);
	return NULL;
}

int
fz_count_pages(fz_document *doc)
{
	if (doc && doc->count_pages)
		return doc->count_pages(doc);
	return 0;
}

fz_page *
fz_load_page(fz_document *doc, int number)
{
	if (doc && doc->load_page)
		return doc->load_page(doc, number);
	return NULL;
}

fz_link *
fz_load_links(fz_document *doc, fz_page *page)
{
	if (doc && doc->load_links && page)
		return doc->load_links(doc, page);
	return NULL;
}

fz_rect *
fz_bound_page(fz_document *doc, fz_page *page, fz_rect *r)
{
	if (doc && doc->bound_page && page && r)
		return doc->bound_page(doc, page, r);
	if (r)
		*r = fz_empty_rect;
	return r;
}

fz_annot *
fz_first_annot(fz_document *doc, fz_page *page)
{
	if (doc && doc->first_annot && page)
		return doc->first_annot(doc, page);
	return NULL;
}

fz_annot *
fz_next_annot(fz_document *doc, fz_annot *annot)
{
	if (doc && doc->next_annot && annot)
		return doc->next_annot(doc, annot);
	return NULL;
}

fz_rect *
fz_bound_annot(fz_document *doc, fz_annot *annot, fz_rect *rect)
{
	if (doc && doc->bound_annot && annot && rect)
		return doc->bound_annot(doc, annot, rect);
	if (rect)
		*rect = fz_empty_rect;
	return rect;
}

void
fz_run_page_contents(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	if (doc && doc->run_page_contents && page)
		doc->run_page_contents(doc, page, dev, transform, cookie);
}

void
fz_run_annot(fz_document *doc, fz_page *page, fz_annot *annot, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	if (doc && doc->run_annot && page && annot)
		doc->run_annot(doc, page, annot, dev, transform, cookie);
}

void
fz_run_page(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	fz_annot *annot;
	fz_rect mediabox;

	fz_bound_page(doc, page, &mediabox);
	fz_begin_page(dev, &mediabox, transform);

	fz_run_page_contents(doc, page, dev, transform, cookie);

	if (cookie && cookie->progress_max != -1)
	{
		int count = 1;
		for (annot = fz_first_annot(doc, page); annot; annot = fz_next_annot(doc, annot))
			count++;
		cookie->progress_max += count;
	}

	for (annot = fz_first_annot(doc, page); annot; annot = fz_next_annot(doc, annot))
	{
		/* Check the cookie for aborting */
		if (cookie)
		{
			if (cookie->abort)
				break;
			cookie->progress++;
		}

		fz_run_annot(doc, page, annot, dev, transform, cookie);
	}

	fz_end_page(dev);
}

void
fz_free_page(fz_document *doc, fz_page *page)
{
	if (doc && doc->free_page && page)
		doc->free_page(doc, page);
}

int
fz_meta(fz_document *doc, int key, void *ptr, int size)
{
	if (doc && doc->meta)
		return doc->meta(doc, key, ptr, size);
	return FZ_META_UNKNOWN_KEY;
}

fz_transition *
fz_page_presentation(fz_document *doc, fz_page *page, float *duration)
{
	float dummy;
	if (duration)
		*duration = 0;
	else
		duration = &dummy;
	if (doc && doc->page_presentation && page)
		return doc->page_presentation(doc, page, duration);
	return NULL;
}

void
fz_write_document(fz_document *doc, char *filename, fz_write_options *opts)
{
	if (doc && doc->write)
		doc->write(doc, filename, opts);
}
