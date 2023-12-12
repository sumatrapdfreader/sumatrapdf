// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include <string.h>

enum
{
	FZ_DOCUMENT_HANDLER_MAX = 32
};

#define DEFW (450)
#define DEFH (600)
#define DEFEM (12)

struct fz_document_handler_context
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
	return fz_keep_imp(ctx, ctx->handler, &ctx->handler->refs);
}

void fz_drop_document_handler_context(fz_context *ctx)
{
	if (!ctx)
		return;

	if (fz_drop_imp(ctx, ctx->handler, &ctx->handler->refs))
	{
		fz_free(ctx, ctx->handler);
		ctx->handler = NULL;
	}
}

void fz_register_document_handler(fz_context *ctx, const fz_document_handler *handler)
{
	fz_document_handler_context *dc;
	int i;

	if (!handler)
		return;

	dc = ctx->handler;
	if (dc == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Document handler list not found");

	for (i = 0; i < dc->count; i++)
		if (dc->handler[i] == handler)
			return;

	if (dc->count >= FZ_DOCUMENT_HANDLER_MAX)
		fz_throw(ctx, FZ_ERROR_LIMIT, "Too many document handlers");

	dc->handler[dc->count++] = handler;
}

const fz_document_handler *
fz_recognize_document_stream_content(fz_context *ctx, fz_stream *stream, const char *magic)
{
	fz_document_handler_context *dc;
	int i, best_score, best_i;
	const char *ext;

	dc = ctx->handler;
	if (dc->count == 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "No document handlers registered");

	ext = strrchr(magic, '.');
	if (ext)
		ext = ext + 1;
	else
		ext = magic;

	best_score = 0;
	best_i = -1;

	if (stream && stream->seek != NULL)
	{
		for (i = 0; i < dc->count; i++)
		{
			int score = 0;

			if (dc->handler[i]->recognize_content)
			{
				fz_seek(ctx, stream, 0, SEEK_SET);
				fz_try(ctx)
				{
					score = dc->handler[i]->recognize_content(ctx, stream);
				}
				fz_catch(ctx)
				{
					/* in case of zip errors when recognizing EPUB/XPS/DOCX files */
					fz_rethrow_unless(ctx, FZ_ERROR_FORMAT);
					(void)fz_convert_error(ctx, NULL); /* ugly hack to silence the error message */
					score = 0;
				}
			}
			if (best_score < score)
			{
				best_score = score;
				best_i = i;
			}
		}
		fz_seek(ctx, stream, 0, SEEK_SET);
	}

	if (best_score < 100)
	{
		for (i = 0; i < dc->count; i++)
		{
			int score = 0;
			const char **entry;

			if (dc->handler[i]->recognize)
				score = dc->handler[i]->recognize(ctx, magic);

			for (entry = &dc->handler[i]->mimetypes[0]; *entry; entry++)
				if (!fz_strcasecmp(magic, *entry) && score < 100)
				{
					score = 100;
					break;
				}

			if (ext)
			{
				for (entry = &dc->handler[i]->extensions[0]; *entry; entry++)
					if (!fz_strcasecmp(ext, *entry) && score < 100)
					{
						score = 100;
						break;
					}
			}

			if (best_score < score)
			{
				best_score = score;
				best_i = i;
			}
		}
	}

	if (best_i < 0)
		return NULL;

	return dc->handler[best_i];
}

const fz_document_handler *fz_recognize_document_content(fz_context *ctx, const char *filename)
{
	fz_stream *stream  = fz_open_file(ctx, filename);
	const fz_document_handler *handler = NULL;

	fz_try(ctx)
		handler = fz_recognize_document_stream_content(ctx, stream, filename);
	fz_always(ctx)
		fz_drop_stream(ctx, stream);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return handler;
}

const fz_document_handler *
fz_recognize_document(fz_context *ctx, const char *magic)
{
	return fz_recognize_document_stream_content(ctx, NULL, magic);
}

#if FZ_ENABLE_PDF
extern fz_document_handler pdf_document_handler;
#endif

fz_document *
fz_open_accelerated_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream, fz_stream *accel)
{
	const fz_document_handler *handler;

	if (stream == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no document to open");
	if (magic == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "missing file type");

	handler = fz_recognize_document_stream_content(ctx, stream, magic);
	if (!handler)
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "cannot find document handler for file type: '%s'", magic);
	if (handler->open_accel_with_stream)
		if (accel || handler->open_with_stream == NULL)
			return handler->open_accel_with_stream(ctx, stream, accel);
	if (accel)
	{
		/* We've had an accelerator passed to a format that doesn't
		 * handle it. This should never happen, as how did the
		 * accelerator get created? */
		fz_drop_stream(ctx, accel);
	}
	return handler->open_with_stream(ctx, stream);
}

fz_document *
fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream)
{
	return fz_open_accelerated_document_with_stream(ctx, magic, stream, NULL);
}

fz_document *
fz_open_document_with_buffer(fz_context *ctx, const char *magic, fz_buffer *buffer)
{
	fz_document *doc;
	fz_stream *stream = fz_open_buffer(ctx, buffer);
	fz_try(ctx)
		doc = fz_open_document_with_stream(ctx, magic, stream);
	fz_always(ctx)
		fz_drop_stream(ctx, stream);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return doc;
}

fz_document *
fz_open_accelerated_document(fz_context *ctx, const char *filename, const char *accel)
{
	const fz_document_handler *handler;
	fz_stream *file;
	fz_stream *afile = NULL;
	fz_document *doc = NULL;

	fz_var(afile);

	if (filename == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no document to open");

	handler = fz_recognize_document_content(ctx, filename);
	if (!handler)
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "cannot find document handler for file: %s", filename);

	if (accel) {
		if (handler->open_accel)
			return handler->open_accel(ctx, filename, accel);
		if (handler->open_accel_with_stream == NULL)
		{
			/* We're not going to be able to use the accelerator - this
			 * should never happen, as how can one have been created? */
			accel = NULL;
		}
	}
	if (!accel && handler->open)
		return handler->open(ctx, filename);

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
	{
		if (accel || handler->open_with_stream == NULL)
		{
			if (accel)
				afile = fz_open_file(ctx, accel);
			doc = handler->open_accel_with_stream(ctx, file, afile);
		}
		else
			doc = handler->open_with_stream(ctx, file);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, afile);
		fz_drop_stream(ctx, file);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return doc;
}

fz_document *
fz_open_document(fz_context *ctx, const char *filename)
{
	return fz_open_accelerated_document(ctx, filename, NULL);
}

void fz_save_accelerator(fz_context *ctx, fz_document *doc, const char *accel)
{
	if (doc == NULL)
		return;
	if (doc->output_accelerator == NULL)
		return;

	fz_output_accelerator(ctx, doc, fz_new_output_with_path(ctx, accel, 0));
}

void fz_output_accelerator(fz_context *ctx, fz_document *doc, fz_output *accel)
{
	if (doc == NULL || accel == NULL)
		return;
	if (doc->output_accelerator == NULL)
	{
		fz_drop_output(ctx, accel);
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Document does not support writing an accelerator");
	}

	doc->output_accelerator(ctx, doc, accel);
}

int fz_document_supports_accelerator(fz_context *ctx, fz_document *doc)
{
	if (doc == NULL)
		return 0;
	return (doc->output_accelerator) != NULL;
}

void *
fz_new_document_of_size(fz_context *ctx, int size)
{
	fz_document *doc = fz_calloc(ctx, 1, size);
	doc->refs = 1;
	return doc;
}

fz_document *
fz_keep_document(fz_context *ctx, fz_document *doc)
{
	return fz_keep_imp(ctx, doc, &doc->refs);
}

void
fz_drop_document(fz_context *ctx, fz_document *doc)
{
	if (fz_drop_imp(ctx, doc, &doc->refs))
	{
		if (doc->open)
			fz_warn(ctx, "There are still open pages in the document!");
		if (doc->drop_document)
			doc->drop_document(ctx, doc);
		fz_free(ctx, doc);
	}
}

static void
fz_ensure_layout(fz_context *ctx, fz_document *doc)
{
	if (doc && doc->layout && !doc->did_layout)
	{
		doc->layout(ctx, doc, DEFW, DEFH, DEFEM);
		doc->did_layout = 1;
	}
}

int
fz_is_document_reflowable(fz_context *ctx, fz_document *doc)
{
	return doc ? doc->is_reflowable : 0;
}

fz_bookmark fz_make_bookmark(fz_context *ctx, fz_document *doc, fz_location loc)
{
	if (doc && doc->make_bookmark)
		return doc->make_bookmark(ctx, doc, loc);
	return (loc.chapter<<16) + loc.page;
}

fz_location fz_lookup_bookmark(fz_context *ctx, fz_document *doc, fz_bookmark mark)
{
	if (doc && doc->lookup_bookmark)
		return doc->lookup_bookmark(ctx, doc, mark);
	return fz_make_location((mark>>16) & 0xffff, mark & 0xffff);
}

int
fz_needs_password(fz_context *ctx, fz_document *doc)
{
	if (doc && doc->needs_password)
		return doc->needs_password(ctx, doc);
	return 0;
}

int
fz_authenticate_password(fz_context *ctx, fz_document *doc, const char *password)
{
	if (doc && doc->authenticate_password)
		return doc->authenticate_password(ctx, doc, password);
	return 1;
}

int
fz_has_permission(fz_context *ctx, fz_document *doc, fz_permission p)
{
	if (doc && doc->has_permission)
		return doc->has_permission(ctx, doc, p);
	return 1;
}

fz_outline *
fz_load_outline(fz_context *ctx, fz_document *doc)
{
	if (doc == NULL)
		return NULL;
	fz_ensure_layout(ctx, doc);
	if (doc->load_outline)
		return doc->load_outline(ctx, doc);
	if (doc->outline_iterator == NULL)
		return NULL;
	return fz_load_outline_from_iterator(ctx, doc->outline_iterator(ctx, doc));
}

fz_outline_iterator *
fz_new_outline_iterator(fz_context *ctx, fz_document *doc)
{
	if (doc == NULL)
		return NULL;
	if (doc->outline_iterator)
		return doc->outline_iterator(ctx, doc);
	if (doc->load_outline == NULL)
		return NULL;
	return fz_outline_iterator_from_outline(ctx, fz_load_outline(ctx, doc));
}

fz_link_dest
fz_resolve_link_dest(fz_context *ctx, fz_document *doc, const char *uri)
{
	fz_ensure_layout(ctx, doc);
	if (doc && doc->resolve_link_dest)
		return doc->resolve_link_dest(ctx, doc, uri);
	return fz_make_link_dest_none();
}

char *
fz_format_link_uri(fz_context *ctx, fz_document *doc, fz_link_dest dest)
{
	if (doc && doc->format_link_uri)
		return doc->format_link_uri(ctx, doc, dest);
	fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot create internal links for this document type");
}

fz_location
fz_resolve_link(fz_context *ctx, fz_document *doc, const char *uri, float *xp, float *yp)
{
	fz_link_dest dest = fz_resolve_link_dest(ctx, doc, uri);
	if (xp) *xp = dest.x;
	if (yp) *yp = dest.y;
	return dest.loc;
}

void
fz_layout_document(fz_context *ctx, fz_document *doc, float w, float h, float em)
{
	if (doc && doc->layout)
	{
		doc->layout(ctx, doc, w, h, em);
		doc->did_layout = 1;
	}
}

int
fz_count_chapters(fz_context *ctx, fz_document *doc)
{
	fz_ensure_layout(ctx, doc);
	if (doc && doc->count_chapters)
		return doc->count_chapters(ctx, doc);
	return 1;
}

int
fz_count_chapter_pages(fz_context *ctx, fz_document *doc, int chapter)
{
	fz_ensure_layout(ctx, doc);
	if (doc && doc->count_pages)
		return doc->count_pages(ctx, doc, chapter);
	return 0;
}

int
fz_count_pages(fz_context *ctx, fz_document *doc)
{
	int i, c, n = 0;
	c = fz_count_chapters(ctx, doc);
	for (i = 0; i < c; ++i)
		n += fz_count_chapter_pages(ctx, doc, i);
	return n;
}

fz_page *
fz_load_page(fz_context *ctx, fz_document *doc, int number)
{
	int i, n = fz_count_chapters(ctx, doc);
	int start = 0;
	for (i = 0; i < n; ++i)
	{
		int m = fz_count_chapter_pages(ctx, doc, i);
		if (number < start + m)
			return fz_load_chapter_page(ctx, doc, i, number - start);
		start += m;
	}
	fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid page number: %d", number+1);
}

fz_location fz_last_page(fz_context *ctx, fz_document *doc)
{
	int nc = fz_count_chapters(ctx, doc);
	int np = fz_count_chapter_pages(ctx, doc, nc-1);
	return fz_make_location(nc-1, np-1);
}

fz_location fz_next_page(fz_context *ctx, fz_document *doc, fz_location loc)
{
	int nc = fz_count_chapters(ctx, doc);
	int np = fz_count_chapter_pages(ctx, doc, loc.chapter);
	if (loc.page + 1 == np)
	{
		if (loc.chapter + 1 < nc)
		{
			return fz_make_location(loc.chapter + 1, 0);
		}
	}
	else
	{
		return fz_make_location(loc.chapter, loc.page + 1);
	}
	return loc;
}

fz_location fz_previous_page(fz_context *ctx, fz_document *doc, fz_location loc)
{
	if (loc.page == 0)
	{
		if (loc.chapter > 0)
		{
			int np = fz_count_chapter_pages(ctx, doc, loc.chapter - 1);
			return fz_make_location(loc.chapter - 1, np - 1);
		}
	}
	else
	{
		return fz_make_location(loc.chapter, loc.page - 1);
	}
	return loc;
}

fz_location fz_clamp_location(fz_context *ctx, fz_document *doc, fz_location loc)
{
	int nc = fz_count_chapters(ctx, doc);
	int np;
	if (loc.chapter < 0) loc.chapter = 0;
	if (loc.chapter >= nc) loc.chapter = nc - 1;
	np = fz_count_chapter_pages(ctx, doc, loc.chapter);
	if (loc.page < 0) loc.page = 0;
	if (loc.page >= np) loc.page = np - 1;
	return loc;
}

fz_location fz_location_from_page_number(fz_context *ctx, fz_document *doc, int number)
{
	int i, m = 0, n = fz_count_chapters(ctx, doc);
	int start = 0;
	if (number < 0)
		number = 0;
	for (i = 0; i < n; ++i)
	{
		m = fz_count_chapter_pages(ctx, doc, i);
		if (number < start + m)
			return fz_make_location(i, number - start);
		start += m;
	}
	return fz_make_location(i-1, m-1);
}

int fz_page_number_from_location(fz_context *ctx, fz_document *doc, fz_location loc)
{
	int i, n, start = 0;
	n = fz_count_chapters(ctx, doc);
	for (i = 0; i < n; ++i)
	{
		if (i == loc.chapter)
			return start + loc.page;
		start += fz_count_chapter_pages(ctx, doc, i);
	}
	return -1;
}

int
fz_lookup_metadata(fz_context *ctx, fz_document *doc, const char *key, char *buf, int size)
{
	if (buf && size > 0)
		buf[0] = 0;
	if (doc && doc->lookup_metadata)
		return doc->lookup_metadata(ctx, doc, key, buf, size);
	return -1;
}

void
fz_set_metadata(fz_context *ctx, fz_document *doc, const char *key, const char *value)
{
	if (doc && doc->set_metadata)
		doc->set_metadata(ctx, doc, key, value);
}

fz_colorspace *
fz_document_output_intent(fz_context *ctx, fz_document *doc)
{
	if (doc && doc->get_output_intent)
		return doc->get_output_intent(ctx, doc);
	return NULL;
}

fz_page *
fz_load_chapter_page(fz_context *ctx, fz_document *doc, int chapter, int number)
{
	fz_page *page;

	if (doc == NULL)
		return NULL;

	fz_ensure_layout(ctx, doc);

	/* Protect modifications to the page list to cope with
	 * destruction of pages on other threads. */
	fz_lock(ctx, FZ_LOCK_ALLOC);
	for (page = doc->open; page; page = page->next)
		if (page->chapter == chapter && page->number == number)
		{
			fz_keep_page_locked(ctx, page);
			fz_unlock(ctx, FZ_LOCK_ALLOC);
			return page;
		}
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	if (doc->load_page)
	{
		page = doc->load_page(ctx, doc, chapter, number);
		page->chapter = chapter;
		page->number = number;

		/* Insert new page at the head of the list of open pages. */
		if (!page->incomplete)
		{
			fz_lock(ctx, FZ_LOCK_ALLOC);
			if ((page->next = doc->open) != NULL)
				doc->open->prev = &page->next;
			doc->open = page;
			page->prev = &doc->open;
			fz_unlock(ctx, FZ_LOCK_ALLOC);
		}
		return page;
	}

	return NULL;
}

fz_link *
fz_load_links(fz_context *ctx, fz_page *page)
{
	if (page && page->load_links)
		return page->load_links(ctx, page);
	return NULL;
}

fz_rect
fz_bound_page(fz_context *ctx, fz_page *page)
{
	if (page && page->bound_page)
		return page->bound_page(ctx, page, FZ_CROP_BOX);
	return fz_empty_rect;
}

fz_rect
fz_bound_page_box(fz_context *ctx, fz_page *page, fz_box_type box)
{
	if (page && page->bound_page)
		return page->bound_page(ctx, page, box);
	return fz_empty_rect;
}

void
fz_run_page_contents(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (page && page->run_page_contents)
	{
		fz_try(ctx)
		{
			page->run_page_contents(ctx, page, dev, transform, cookie);
		}
		fz_catch(ctx)
		{
			dev->close_device = NULL; /* aborted run, don't warn about unclosed device */
			fz_rethrow_unless(ctx, FZ_ERROR_ABORT);
			fz_ignore_error(ctx);
		}
	}
}

void
fz_run_page_annots(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (page && page->run_page_annots)
	{
		fz_try(ctx)
		{
			page->run_page_annots(ctx, page, dev, transform, cookie);
		}
		fz_catch(ctx)
		{
			dev->close_device = NULL; /* aborted run, don't warn about unclosed device */
			fz_rethrow_unless(ctx, FZ_ERROR_ABORT);
			fz_ignore_error(ctx);
		}
	}
}

void
fz_run_page_widgets(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (page && page->run_page_widgets)
	{
		fz_try(ctx)
		{
			page->run_page_widgets(ctx, page, dev, transform, cookie);
		}
		fz_catch(ctx)
		{
			dev->close_device = NULL; /* aborted run, don't warn about unclosed device */
			fz_rethrow_unless(ctx, FZ_ERROR_ABORT);
			fz_ignore_error(ctx);
		}
	}
}

void
fz_run_page(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	fz_run_page_contents(ctx, page, dev, transform, cookie);
	fz_run_page_annots(ctx, page, dev, transform, cookie);
	fz_run_page_widgets(ctx, page, dev, transform, cookie);
}

fz_page *
fz_new_page_of_size(fz_context *ctx, int size, fz_document *doc)
{
	fz_page *page = Memento_label(fz_calloc(ctx, 1, size), "fz_page");
	page->refs = 1;
	page->doc = fz_keep_document(ctx, doc);
	return page;
}

fz_page *
fz_keep_page(fz_context *ctx, fz_page *page)
{
	return fz_keep_imp(ctx, page, &page->refs);
}

fz_page *
fz_keep_page_locked(fz_context *ctx, fz_page *page)
{
	return fz_keep_imp_locked(ctx, page, &page->refs);
}

void
fz_drop_page(fz_context *ctx, fz_page *page)
{
	if (fz_drop_imp(ctx, page, &page->refs))
	{
		/* Remove page from the list of open pages */
		fz_lock(ctx, FZ_LOCK_ALLOC);
		if (page->next != NULL)
			page->next->prev = page->prev;
		if (page->prev != NULL)
			*page->prev = page->next;
		fz_unlock(ctx, FZ_LOCK_ALLOC);

		if (page->drop_page)
			page->drop_page(ctx, page);

		fz_drop_document(ctx, page->doc);

		fz_free(ctx, page);
	}
}

fz_transition *
fz_page_presentation(fz_context *ctx, fz_page *page, fz_transition *transition, float *duration)
{
	float dummy;
	if (duration)
		*duration = 0;
	else
		duration = &dummy;
	if (page && page->page_presentation && page)
		return page->page_presentation(ctx, page, transition, duration);
	return NULL;
}

fz_separations *
fz_page_separations(fz_context *ctx, fz_page *page)
{
	if (page && page->separations)
		return page->separations(ctx, page);
	return NULL;
}

int fz_page_uses_overprint(fz_context *ctx, fz_page *page)
{
	if (page && page->overprint)
		return page->overprint(ctx, page);
	return 0;
}

fz_link *fz_create_link(fz_context *ctx, fz_page *page, fz_rect bbox, const char *uri)
{
	if (page == NULL || uri == NULL)
		return NULL;
	if (page->create_link == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "This format of document does not support creating links");
	return page->create_link(ctx, page, bbox, uri);
}

void fz_delete_link(fz_context *ctx, fz_page *page, fz_link *link)
{
	if (page == NULL || link == NULL)
		return;
	if (page->delete_link == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "This format of document does not support deleting links");
	page->delete_link(ctx, page, link);
}

void fz_set_link_rect(fz_context *ctx, fz_link *link, fz_rect rect)
{
	if (link == NULL)
		return;
	if (link->set_rect_fn == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "This format of document does not support updating link bounds");
	link->set_rect_fn(ctx, link, rect);
}

void fz_set_link_uri(fz_context *ctx, fz_link *link, const char *uri)
{
	if (link == NULL)
		return;
	if (link->set_uri_fn == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "This format of document does not support updating link uri");
	link->set_uri_fn(ctx, link, uri);
}

void *
fz_process_opened_pages(fz_context *ctx, fz_document *doc, fz_process_opened_page_fn *process_opened_page, void *state)
{
	fz_page *page;
	fz_page *kept = NULL;
	fz_page *dropme = NULL;
	void *ret = NULL;

	fz_var(kept);
	fz_var(dropme);
	fz_var(page);
	fz_try(ctx)
	{
		/* We can only walk the page list while the alloc lock is taken, so gymnastics are required. */
		/* Loop invariant: at any point where we might throw, kept != NULL iff we are unlocked. */
		fz_lock(ctx, FZ_LOCK_ALLOC);
		for (page = doc->open; ret == NULL && page != NULL; page = page->next)
		{
			/* Keep an extra reference to the page so that no other thread can remove it. */
			kept = fz_keep_page_locked(ctx, page);
			fz_unlock(ctx, FZ_LOCK_ALLOC);
			/* Drop any extra reference we might still have to a previous page. */
			fz_drop_page(ctx, dropme);
			dropme = NULL;

			ret = process_opened_page(ctx, page, state);

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

	return ret;
}

const char *
fz_page_label(fz_context *ctx, fz_page *page, char *buf, int size)
{
	fz_document *doc = page->doc;
	if (doc->page_label)
		doc->page_label(ctx, page->doc, page->chapter, page->number, buf, size);
	else if (fz_count_chapters(ctx, page->doc) > 1)
		fz_snprintf(buf, size, "%d/%d", page->chapter + 1, page->number + 1);
	else
		fz_snprintf(buf, size, "%d", page->number + 1);
	return buf;
}


fz_box_type fz_box_type_from_string(const char *name)
{
	if (!fz_strcasecmp(name, "MediaBox"))
		return FZ_MEDIA_BOX;
	if (!fz_strcasecmp(name, "CropBox"))
		return FZ_CROP_BOX;
	if (!fz_strcasecmp(name, "BleedBox"))
		return FZ_BLEED_BOX;
	if (!fz_strcasecmp(name, "TrimBox"))
		return FZ_TRIM_BOX;
	if (!fz_strcasecmp(name, "ArtBox"))
		return FZ_ART_BOX;
	return FZ_UNKNOWN_BOX;
}

const char *fz_string_from_box_type(fz_box_type box)
{
	switch (box)
	{
	case FZ_MEDIA_BOX: return "MediaBox";
	case FZ_CROP_BOX: return "CropBox";
	case FZ_BLEED_BOX: return "BleedBox";
	case FZ_TRIM_BOX: return "TrimBox";
	case FZ_ART_BOX: return "ArtBox";
	default: return "UnknownBox";
	}
}
