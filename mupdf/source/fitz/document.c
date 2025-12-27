// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#include "context-imp.h"

#include <string.h>
#ifndef _WIN32
#include <unistd.h> /* For unlink */
#endif
#include <errno.h>

static void fz_reap_dead_pages(fz_context *ctx, fz_document *doc);

enum
{
	FZ_DOCUMENT_HANDLER_MAX = 32
};

#define DEFW (450)
#define DEFH (600)
#define DEFEM (12)

static fz_output *
fz_new_output_to_tempfile(fz_context *ctx, char **namep)
{
	fz_output *out = NULL;
#ifdef _WIN32
	char namebuf[L_tmpnam];
	int attempts = 0;
#else
	char namebuf[] = "/tmp/fztmpXXXXXX";
#endif


	fz_var(out);

#ifdef _WIN32
	/* Windows has no mkstemp command, so we have to use the old-style
	 * tmpnam based system, and retry in the case of races. */
	do
	{
		if (tmpnam(namebuf) == NULL)
			fz_throw(ctx, FZ_ERROR_SYSTEM, "tmpnam failed");
		fz_try(ctx)
			out = fz_new_output_with_path(ctx, namebuf, 0);
		fz_catch(ctx)
		{
			/* We might hit a race condition and not be able to
			 * open the file because someone beats us to it. We'd
			 * be unbearably unlucky to hit this 10 times in a row. */
			attempts++;
			if (attempts >= 10)
				fz_rethrow(ctx);
			else
				fz_ignore_error(ctx);
		}
	}
	while (out == NULL);
#else
	{
		FILE *file;
		int fd = mkstemp(namebuf);

		if (fd == -1)
			fz_throw(ctx, FZ_ERROR_SYSTEM, "Cannot mkstemp: %s", strerror(errno));
		file = fdopen(fd, "w");
		if (file == NULL)
			fz_throw(ctx, FZ_ERROR_SYSTEM, "Failed to open temporary file");
		out = fz_new_output_with_file_ptr(ctx, file);
	}
#endif

	if (namep)
	{
		fz_try(ctx)
			*namep = fz_strdup(ctx, namebuf);
		fz_catch(ctx)
		{
			fz_drop_output(ctx, out);
			unlink(namebuf);
			fz_rethrow(ctx);
		}
	}

	return out;
}

static char *
fz_new_tmpfile_from_stream(fz_context *ctx, fz_stream *stm)
{
	char *name;
	fz_output *out = fz_new_output_to_tempfile(ctx, &name);

	fz_try(ctx)
	{
		fz_write_stream(ctx, out, stm);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
	{
		fz_free(ctx, name);
		fz_rethrow(ctx);
	}

	return name;
}

static fz_stream *
fz_file_backed_stream(fz_context *ctx, fz_stream *stream)
{
	const char *oname = fz_stream_filename(ctx, stream);
	char *name;

	/* If the file has a name, it's already a file-backed stream.*/
	if (oname)
		return stream;

	/* Otherwise we need to make it one. */
	name = fz_new_tmpfile_from_stream(ctx, stream);
	fz_try(ctx)
		stream = fz_open_file_autodelete(ctx, name);
	fz_always(ctx)
		fz_free(ctx, name);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return stream;
}

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
	int i;

	if (!ctx || !ctx->handler)
		return;

	for (i = 0; i < ctx->handler->count; i++)
	{
		if (ctx->handler->handler[i]->fin)
		{
			fz_try(ctx)
				ctx->handler->handler[i]->fin(ctx, ctx->handler->handler[i]);
			fz_catch(ctx)
				fz_ignore_error(ctx);
		}
	}

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
	return fz_recognize_document_stream_and_dir_content(ctx, stream, NULL, magic);
}

const fz_document_handler *
do_recognize_document_stream_and_dir_content(fz_context *ctx, fz_stream **streamp, fz_archive *dir, const char *magic, void **handler_state, fz_document_recognize_state_free_fn **handler_free_state)
{
	fz_document_handler_context *dc;
	int i, best_score, best_i;
	void *best_state = NULL;
	fz_document_recognize_state_free_fn *best_free_state = NULL;
	const char *ext;
	int drop_stream = 0;
	fz_stream *stream = *streamp;

	if (handler_state)
		*handler_state = NULL;
	if (handler_free_state)
		*handler_free_state = NULL;

	dc = ctx->handler;
	if (dc->count == 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "No document handlers registered");

	if (magic == NULL)
		magic = "";
	ext = strrchr(magic, '.');
	if (ext)
		ext = ext + 1;
	else
		ext = magic;

	best_score = 0;
	best_i = -1;

	/* If we're handed a stream, check to see if any of our document handlers
	 * need a file. If so, change the stream to be a file-backed one. */
	if (stream)
	{
		int wants_file = 0;
		for (i = 0; i < dc->count; i++)
			wants_file |= dc->handler[i]->wants_file;

		/* Convert the stream into a file_backed stream. */
		if (wants_file)
		{
			fz_stream *stream2 = fz_file_backed_stream(ctx, stream);
			if (stream2 != stream)
			{
				/* Either we need to pass this back to our caller, or we
				 * need to drop it. */
				drop_stream = 1;
				stream = stream2;
			}
		}
	}

	fz_try(ctx)
	{
		int can_recognize_stream = ((stream && stream->seek != NULL) || (stream == NULL && dir != NULL));

		for (i = 0; i < dc->count; i++)
		{
			void *state = NULL;
			fz_document_recognize_state_free_fn *free_state = NULL;
			int score = 0;
			int magic_score = 0;
			const char **entry;

			/* Get a score from recognizing the stream */
			if (dc->handler[i]->recognize_content && can_recognize_stream)
			{
				if (stream)
					fz_seek(ctx, stream, 0, SEEK_SET);
				fz_try(ctx)
				{
					score = dc->handler[i]->recognize_content(ctx, dc->handler[i], stream, dir, &state, &free_state);
				}
				fz_catch(ctx)
				{
					/* in case of zip errors when recognizing EPUB/XPS/DOCX files */
					fz_rethrow_unless(ctx, FZ_ERROR_FORMAT);
					(void)fz_convert_error(ctx, NULL); /* ugly hack to silence the error message */
					score = 0;
				}
			}

			/* Now get a score from recognizing the magic */
			if (dc->handler[i]->recognize)
				magic_score = dc->handler[i]->recognize(ctx, dc->handler[i], magic);

			for (entry = &dc->handler[i]->mimetypes[0]; *entry; entry++)
				if (!fz_strcasecmp(magic, *entry))
				{
					magic_score = 100;
					break;
				}

			if (ext)
			{
				for (entry = &dc->handler[i]->extensions[0]; *entry; entry++)
					if (!fz_strcasecmp(ext, *entry))
					{
						magic_score = 100;
						break;
					}
			}

			/* If we recognized the format (at least partially), and the magic_score matches, then that's
			 * definitely the one we want to use. Use 100 + score here, to allow for having multiple
			 * handlers that support a given magic, where one agent is better than the other. */
			if (score > 0 && magic_score > 0)
				score = 100 + score;
			/* Otherwise, if we didn't recognize the format, we'll weakly believe in the magic, but
			 * we won't let it override anything that actually will cope. */
			else if (magic_score > 0)
				score = 1;
			if (best_score < score)
			{
				best_score = score;
				best_i = i;
				if (best_free_state)
					best_free_state(ctx, best_state);
				best_free_state = free_state;
				best_state = state;
			}
			else if (free_state)
				free_state(ctx, state);
		}
		if (stream)
			fz_seek(ctx, stream, 0, SEEK_SET);
	}
	fz_catch(ctx)
	{
		if (best_free_state)
			best_free_state(ctx, best_state);
		if (drop_stream)
			fz_drop_stream(ctx, stream);
		fz_rethrow(ctx);
	}

	if (best_i < 0)
	{
		if (drop_stream)
			fz_drop_stream(ctx, stream);
		return NULL;
	}

	/* Only if we found a handler, do we make our modified stream available to the
	 * caller. */
	*streamp = stream;

	if (handler_state && handler_free_state)
	{
		*handler_state = best_state;
		*handler_free_state = best_free_state;
	}
	else if (best_free_state)
		best_free_state(ctx, best_state);

	return dc->handler[best_i];
}

const fz_document_handler *
fz_recognize_document_stream_and_dir_content(fz_context *ctx, fz_stream *stream, fz_archive *dir, const char *magic)
{
	fz_stream *stm = stream;
	const fz_document_handler *res;

	res = do_recognize_document_stream_and_dir_content(ctx, &stm, dir, magic, NULL, NULL);

	if (stm != stream)
		fz_drop_stream(ctx, stm);

	return res;
}

static const fz_document_handler *do_recognize_document_content(fz_context *ctx, const char *filename, void **handler_state, fz_document_recognize_state_free_fn **handler_free_state)
{
	fz_stream *stream = NULL;
	const fz_document_handler *handler = NULL;
	fz_archive *zip = NULL;
	fz_stream *stm;

	if (fz_is_directory(ctx, filename))
		zip = fz_open_directory(ctx, filename);
	else
		stream  = fz_open_file(ctx, filename);

	stm = stream;
	fz_try(ctx)
		handler = do_recognize_document_stream_and_dir_content(ctx, &stm, zip, filename, handler_state, handler_free_state);
	fz_always(ctx)
	{
		if (stm != stream)
			fz_drop_stream(ctx, stm);
		fz_drop_stream(ctx, stream);
		fz_drop_archive(ctx, zip);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return handler;
}

const fz_document_handler *fz_recognize_document_content(fz_context* ctx, const char* filename)
{
	return do_recognize_document_content(ctx, filename, NULL, NULL);
}

const fz_document_handler *
fz_recognize_document(fz_context *ctx, const char *magic)
{
	return fz_recognize_document_stream_and_dir_content(ctx, NULL, NULL, magic);
}

#if FZ_ENABLE_PDF
extern fz_document_handler pdf_document_handler;
#endif

fz_document *
fz_open_accelerated_document_with_stream_and_dir(fz_context *ctx, const char *magic, fz_stream *stream, fz_stream *accel, fz_archive *dir)
{
	const fz_document_handler *handler;
	fz_stream *wrapped_stream = stream;
	fz_document *ret;
	void *state = NULL;
	fz_document_recognize_state_free_fn *free_state = NULL;

	if (stream == NULL && dir == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no document to open");
	if (magic == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "missing file type");

	/* If this finds a handler, then this might wrap stream. If it does, we reuse the wrapped one in
	 * the open call (hence avoiding us having to 'file-back' a stream twice), but we must free it. */
	handler = do_recognize_document_stream_and_dir_content(ctx, &wrapped_stream, dir, magic, &state, &free_state);
	if (!handler)
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "cannot find document handler for file type: '%s'", magic);
	fz_try(ctx)
		ret = handler->open(ctx, handler, wrapped_stream, accel, dir, state);
	fz_always(ctx)
	{
		if (wrapped_stream != stream)
			fz_drop_stream(ctx, wrapped_stream);
		if (free_state && state)
			free_state(ctx, state);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_document *
fz_open_accelerated_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream, fz_stream *accel)
{
	return fz_open_accelerated_document_with_stream_and_dir(ctx, magic, stream, accel, NULL);
}

fz_document *
fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream)
{
	return fz_open_accelerated_document_with_stream(ctx, magic, stream, NULL);
}

fz_document *
fz_open_document_with_stream_and_dir(fz_context *ctx, const char *magic, fz_stream *stream, fz_archive *dir)
{
	return fz_open_accelerated_document_with_stream_and_dir(ctx, magic, stream, NULL, dir);
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
	fz_stream *file = NULL;
	fz_stream *afile = NULL;
	fz_document *doc = NULL;
	fz_archive *dir = NULL;
	char dirname[PATH_MAX];
	void *state = NULL;
	fz_document_recognize_state_free_fn *free_state = NULL;

	if (filename == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no document to open");

	if (fz_is_directory(ctx, filename))
	{
		/* Cannot accelerate directories, currently. */
		dir = fz_open_directory(ctx, filename);

		fz_try(ctx)
			doc = fz_open_accelerated_document_with_stream_and_dir(ctx, filename, NULL, NULL, dir);
		fz_always(ctx)
			fz_drop_archive(ctx, dir);
		fz_catch(ctx)
			fz_rethrow(ctx);

		return doc;
	}

	handler = do_recognize_document_content(ctx, filename, &state, &free_state);
	if (!handler)
		fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "cannot find document handler for file: %s", filename);

	fz_var(afile);
	fz_var(file);

	fz_try(ctx)
	{
		file = fz_open_file(ctx, filename);

		if (accel)
			afile = fz_open_file(ctx, accel);
		if (handler->wants_dir)
		{
			fz_dirname(dirname, filename, sizeof dirname);
			dir = fz_open_directory(ctx, dirname);
		}
		doc = handler->open(ctx, handler, file, afile, dir, state);
	}
	fz_always(ctx)
	{
		if (free_state)
			free_state(ctx, state);
		fz_drop_archive(ctx, dir);
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
	doc->id = fz_new_document_id(ctx);

	fz_log_activity(ctx, FZ_ACTIVITY_NEW_DOC, NULL);

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
		fz_reap_dead_pages(ctx, doc);
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
fz_lookup_metadata(fz_context *ctx, fz_document *doc, const char *key, char *buf, size_t size)
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

static void
fz_reap_dead_pages(fz_context *ctx, fz_document *doc)
{
	fz_page *page;
	fz_page *next_page;

	for (page = doc->open; page; page = next_page)
	{
		next_page = page->next;
		if (!page->doc)
		{
			if (page->next != NULL)
				page->next->prev = page->prev;
			if (page->prev != NULL)
				*page->prev = page->next;
			fz_free(ctx, page);
			if (page == doc->open)
				doc->open = next_page;
		}
	}
}

fz_page *
fz_load_chapter_page(fz_context *ctx, fz_document *doc, int chapter, int number)
{
	fz_page *page;

	if (doc == NULL)
		return NULL;

	fz_ensure_layout(ctx, doc);

	// Trigger reaping dead pages when we load a new page.
	fz_reap_dead_pages(ctx, doc);

	/* Protect modifications to the page list to cope with
	 * destruction of pages on other threads. */
	for (page = doc->open; page; page = page->next)
	{
		if (page->chapter == chapter && page->number == number)
		{
			fz_keep_page(ctx, page);
			return page;
		}
	}

	if (doc->load_page)
	{
		page = doc->load_page(ctx, doc, chapter, number);
		page->chapter = chapter;
		page->number = number;

		/* Insert new page at the head of the list of open pages. */
		if (!page->incomplete)
		{
			if ((page->next = doc->open) != NULL)
				doc->open->prev = &page->next;
			doc->open = page;
			page->prev = &doc->open;
			page->in_doc = 1;
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
fz_run_document_structure(fz_context *ctx, fz_document *doc, fz_device *dev, fz_cookie *cookie)
{
	if (doc && doc->run_structure)
	{
		fz_try(ctx)
		{
			doc->run_structure(ctx, doc, dev, cookie);
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

void
fz_drop_page(fz_context *ctx, fz_page *page)
{
	if (fz_drop_imp(ctx, page, &page->refs))
	{
		fz_document *doc = page->doc;

		if (page->drop_page)
			page->drop_page(ctx, page);

		// Mark the page as dead so we can reap the struct allocation later.
		page->doc = NULL;
		page->chapter = -1;
		page->number = -1;

		// If page has never been added to the list of open pages in a document,
		// it will not get be reaped upon document freeing; instead free the page
		// immediately.
		if (!page->in_doc)
			fz_free(ctx, page);

		fz_drop_document(ctx, doc);
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
	void *ret;

	for (page = doc->open; page != NULL; page = page->next)
	{
		// Skip dead pages.
		if (page->doc == NULL)
			continue;

		ret = process_opened_page(ctx, page, state);
		if (ret)
			return ret;
	}

	return NULL;
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
