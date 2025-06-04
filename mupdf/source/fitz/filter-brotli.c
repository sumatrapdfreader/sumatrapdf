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

#if FZ_ENABLE_BROTLI

#include "brotli/decode.h"

#include <string.h>

typedef struct
{
	fz_stream *chain;
	BrotliDecoderState *dec_state;
	unsigned char buffer[4096];
} fz_brotli_state;

void *my_brotlid_alloc(void *ctx, size_t size)
{
	return Memento_label(fz_malloc_no_throw(ctx, size), "brotlid_alloc");
}

void my_brotlid_free(void *ctx, void *ptr)
{
	fz_free(ctx, ptr);
}

static int
next_brotlid(fz_context *ctx, fz_stream *stm, size_t required)
{
	fz_brotli_state *state = stm->state;
	fz_stream *chain = state->chain;
	unsigned char *outbuf = state->buffer;
	size_t outlen = sizeof(state->buffer);
	BrotliDecoderResult res;

	if (stm->eof)
		return EOF;

	while (outlen > 0)
	{
		size_t avail_in = fz_available(ctx, chain, 1);

		res = BrotliDecoderDecompressStream(state->dec_state,
							&avail_in,
							(const uint8_t **)&chain->rp,
							&outlen,
							&outbuf,
							NULL);

		chain->rp = chain->wp - avail_in;

		if (res == BROTLI_DECODER_RESULT_SUCCESS)
		{
			break;
		}
		else if (res == BROTLI_DECODER_RESULT_ERROR)
		{
			fz_throw(ctx, FZ_ERROR_LIBRARY, "brotli decompression error");
		}
	}

	stm->rp = state->buffer;
	stm->wp = state->buffer + sizeof(state->buffer) - outlen;
	stm->pos += sizeof(state->buffer) - outlen;
	if (stm->rp == stm->wp)
	{
		stm->eof = 1;
		return EOF;
	}
	return *stm->rp++;
}

static void
close_brotlid(fz_context *ctx, void *state_)
{
	fz_brotli_state *state = (fz_brotli_state *)state_;

	BrotliDecoderDestroyInstance(state->dec_state);

	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream *
fz_open_brotlid(fz_context *ctx, fz_stream *chain)
{
	fz_brotli_state *state;

	state = fz_malloc_struct(ctx, fz_brotli_state);
	state->dec_state = BrotliDecoderCreateInstance(
					my_brotlid_alloc,
					my_brotlid_free,
					ctx);

	if (state->dec_state == NULL)
	{
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "brotli error: decoder creation failed");
	}

	state->chain = fz_keep_stream(ctx, chain);

	return fz_new_stream(ctx, state, next_brotlid, close_brotlid);
}

#else

fz_stream *
fz_open_brotlid(fz_context *ctx, fz_stream *chain)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "brotli compression not enabled");
	return NULL;
}

#endif
