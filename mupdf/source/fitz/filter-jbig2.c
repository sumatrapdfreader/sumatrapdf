// Copyright (C) 2004-2021 Artifex Software, Inc.
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

#include <jbig2.h>

typedef struct
{
	Jbig2Allocator alloc;
	fz_context *ctx;
} fz_jbig2_allocators;

struct fz_jbig2_globals
{
	fz_storable storable;
	Jbig2GlobalCtx *gctx;
	fz_jbig2_allocators alloc;
	fz_buffer *data;
};

typedef struct
{
	fz_stream *chain;
	Jbig2Ctx *ctx;
	fz_jbig2_allocators alloc;
	fz_jbig2_globals *gctx;
	Jbig2Image *page;
	int idx;
	unsigned char buffer[4096];
} fz_jbig2d;

fz_jbig2_globals *
fz_keep_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	return fz_keep_storable(ctx, &globals->storable);
}

void
fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	fz_drop_storable(ctx, &globals->storable);
}

static void
close_jbig2d(fz_context *ctx, void *state_)
{
	fz_jbig2d *state = state_;
	if (state->page)
		jbig2_release_page(state->ctx, state->page);
	fz_drop_jbig2_globals(ctx, state->gctx);
	jbig2_ctx_free(state->ctx);
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

static int
next_jbig2d(fz_context *ctx, fz_stream *stm, size_t len)
{
	fz_jbig2d *state = stm->state;
	unsigned char tmp[4096];
	unsigned char *buf = state->buffer;
	unsigned char *p = buf;
	unsigned char *ep;
	unsigned char *s;
	int x, w;
	size_t n;

	if (len > sizeof(state->buffer))
		len = sizeof(state->buffer);
	ep = buf + len;

	if (!state->page)
	{
		while (1)
		{
			n = fz_read(ctx, state->chain, tmp, sizeof tmp);
			if (n == 0)
				break;

			if (jbig2_data_in(state->ctx, tmp, n) < 0)
				fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot decode jbig2 image");
		}

		if (jbig2_complete_page(state->ctx) < 0)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot complete jbig2 image");

		state->page = jbig2_page_out(state->ctx);
		if (!state->page)
			fz_throw(ctx, FZ_ERROR_LIBRARY, "no jbig2 image decoded");
	}

	s = state->page->data;
	w = state->page->height * state->page->stride;
	x = state->idx;
	while (p < ep && x < w)
		*p++ = s[x++] ^ 0xff;
	state->idx = x;

	stm->rp = buf;
	stm->wp = p;
	if (p == buf)
		return EOF;
	stm->pos += p - buf;
	return *stm->rp++;
}

static void
error_callback(void *data, const char *msg, Jbig2Severity severity, uint32_t seg_idx)
{
	fz_context *ctx = data;
	if (severity == JBIG2_SEVERITY_FATAL)
		fz_warn(ctx, "jbig2dec error: %s (segment %u)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_WARNING)
		fz_warn(ctx, "jbig2dec warning: %s (segment %u)", msg, seg_idx);
#ifdef JBIG2_DEBUG
	else if (severity == JBIG2_SEVERITY_INFO)
		fz_warn(ctx, "jbig2dec info: %s (segment %u)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_DEBUG)
		fz_warn(ctx, "jbig2dec debug: %s (segment %u)", msg, seg_idx);
#endif
}

static void *fz_jbig2_alloc(Jbig2Allocator *allocator, size_t size)
{
	fz_context *ctx = ((fz_jbig2_allocators *) allocator)->ctx;
	return Memento_label(fz_malloc_no_throw(ctx, size), "jbig2_alloc");
}

static void fz_jbig2_free(Jbig2Allocator *allocator, void *p)
{
	fz_context *ctx = ((fz_jbig2_allocators *) allocator)->ctx;
	fz_free(ctx, p);
}

static void *fz_jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
	fz_context *ctx = ((fz_jbig2_allocators *) allocator)->ctx;
	if (size == 0)
	{
		fz_free(ctx, p);
		return NULL;
	}
	if (p == NULL)
		return Memento_label(fz_malloc(ctx, size), "jbig2_realloc");
	return Memento_label(fz_realloc_no_throw(ctx, p, size), "jbig2_realloc");
}

fz_jbig2_globals *
fz_load_jbig2_globals(fz_context *ctx, fz_buffer *buf)
{
	fz_jbig2_globals *globals;
	Jbig2Ctx *jctx;

	if (buf == NULL || buf->data == NULL || buf->len == 0)
		return NULL;

	globals = fz_malloc_struct(ctx, fz_jbig2_globals);

	globals->alloc.ctx = ctx;
	globals->alloc.alloc.alloc = fz_jbig2_alloc;
	globals->alloc.alloc.free = fz_jbig2_free;
	globals->alloc.alloc.realloc = fz_jbig2_realloc;

	jctx = jbig2_ctx_new((Jbig2Allocator *) &globals->alloc, JBIG2_OPTIONS_EMBEDDED, NULL, error_callback, ctx);
	if (!jctx)
	{
		fz_free(ctx, globals);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot allocate jbig2 globals context");
	}

	if (jbig2_data_in(jctx, buf->data, buf->len) < 0)
	{
		jbig2_global_ctx_free(jbig2_make_global_ctx(jctx));
		fz_free(ctx, globals);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot decode jbig2 globals");
	}

	FZ_INIT_STORABLE(globals, 1, fz_drop_jbig2_globals_imp);
	globals->gctx = jbig2_make_global_ctx(jctx);

	globals->data = fz_keep_buffer(ctx, buf);

	return globals;
}

void
fz_drop_jbig2_globals_imp(fz_context *ctx, fz_storable *globals_)
{
	fz_jbig2_globals *globals = (fz_jbig2_globals *)globals_;
	globals->alloc.ctx = ctx;
	jbig2_global_ctx_free(globals->gctx);
	fz_drop_buffer(ctx, globals->data);
	fz_free(ctx, globals);
}

fz_stream *
fz_open_jbig2d(fz_context *ctx, fz_stream *chain, fz_jbig2_globals *globals, int embedded)
{
	fz_jbig2d *state = NULL;
	Jbig2Options options;

	fz_var(state);

	state = fz_malloc_struct(ctx, fz_jbig2d);
	state->gctx = fz_keep_jbig2_globals(ctx, globals);
	state->alloc.ctx = ctx;
	state->alloc.alloc.alloc = fz_jbig2_alloc;
	state->alloc.alloc.free = fz_jbig2_free;
	state->alloc.alloc.realloc = fz_jbig2_realloc;

	options = 0;
	if (embedded)
		options |= JBIG2_OPTIONS_EMBEDDED;

	state->ctx = jbig2_ctx_new((Jbig2Allocator *) &state->alloc, options, globals ? globals->gctx : NULL, error_callback, ctx);
	if (state->ctx == NULL)
	{
		fz_drop_jbig2_globals(ctx, state->gctx);
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "cannot allocate jbig2 context");
	}

	state->page = NULL;
	state->idx = 0;
	state->chain = fz_keep_stream(ctx, chain);

	return fz_new_stream(ctx, state, next_jbig2d, close_jbig2d);
}

fz_buffer *
fz_jbig2_globals_data(fz_context *ctx, fz_jbig2_globals *globals)
{
	return globals ? globals->data : NULL;
}
