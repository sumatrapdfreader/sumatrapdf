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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"

#include <zlib.h>

#include <string.h>

typedef struct
{
	fz_stream *chain;
	fz_buffer *buffer;
} fz_leech;

static int
next_leech(fz_context *ctx, fz_stream *stm, size_t max)
{
	fz_leech *state = stm->state;
	fz_buffer *buffer = state->buffer;
	size_t n = fz_available(ctx, state->chain, max);

	if (n > max)
		n = max;

	while (buffer->cap < buffer->len + n)
	{
		fz_grow_buffer(ctx, state->buffer);
	}
	memcpy(buffer->data + buffer->len, state->chain->rp, n);
	stm->rp = buffer->data + buffer->len;
	stm->wp = buffer->data + buffer->len + n;
	state->chain->rp += n;
	buffer->len += n;

	if (n == 0)
		return EOF;
	return *stm->rp++;
}

static void
close_leech(fz_context *ctx, void *state_)
{
	fz_leech *state = (fz_leech *)state_;
	fz_drop_stream(ctx, state->chain);
	fz_drop_buffer(ctx, state->buffer);
	fz_free(ctx, state);
}

fz_stream *
fz_open_leecher(fz_context *ctx, fz_stream *chain, fz_buffer *buffer)
{
	fz_leech *state = fz_malloc_struct(ctx, fz_leech);
	state->chain = fz_keep_stream(ctx, chain);
	state->buffer = fz_keep_buffer(ctx, buffer);
	return fz_new_stream(ctx, state, next_leech, close_leech);
}
