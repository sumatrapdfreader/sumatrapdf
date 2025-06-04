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

#include "brotli/encode.h"

#include <limits.h>

static void *
my_brotli_alloc(void *opaque, size_t size)
{
	return fz_malloc_no_throw((fz_context *)opaque, size);
}

static void
my_brotli_free(void *opaque, void *ptr)
{
	fz_free((fz_context *)opaque, ptr);
}

void fz_compress_brotli(fz_context *ctx, unsigned char *dest, size_t *dest_len, const unsigned char *source, size_t source_len, fz_brotli_level level)
{
	int ok;
	BrotliEncoderState *enc_state;
	unsigned char *outp = dest;
	size_t avail_out = *dest_len;

	enc_state = BrotliEncoderCreateInstance(my_brotli_alloc, my_brotli_free, ctx);

	if (!BrotliEncoderSetParameter(enc_state, BROTLI_PARAM_QUALITY, level))
	{
		BrotliEncoderDestroyInstance(enc_state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Brotli compression failed");
	}

	do {
		ok = BrotliEncoderCompressStream(enc_state,
						(source_len > 0 ? BROTLI_OPERATION_PROCESS : BROTLI_OPERATION_FINISH),
						&source_len,
						&source,
						&avail_out,
						&outp,
						NULL);
	} while (ok && !BrotliEncoderIsFinished(enc_state));

	if (!ok)
	{
		BrotliEncoderDestroyInstance(enc_state);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Brotli compression failed");
	}
	*dest_len = outp - dest;

	BrotliEncoderDestroyInstance(enc_state);
}

unsigned char *fz_new_brotli_data(fz_context *ctx, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_brotli_level level)
{
	size_t bound = fz_brotli_bound(ctx, source_length);
	unsigned char *cdata = Memento_label(fz_malloc(ctx, bound), "brotli_data");
	*compressed_length = 0;

	fz_try(ctx)
		fz_compress_brotli(ctx, cdata, &bound, source, source_length, level);
	fz_catch(ctx)
	{
		fz_free(ctx, cdata);
		fz_rethrow(ctx);
	}

	*compressed_length = bound;
	return cdata;
}

unsigned char *fz_new_brotli_data_from_buffer(fz_context *ctx, size_t *compressed_length, fz_buffer *buffer, fz_brotli_level level)
{
	unsigned char *data;
	size_t size = fz_buffer_storage(ctx, buffer, &data);

	if (size == 0 || data == NULL)
	{
		*compressed_length = 0;
		return NULL;
	}

	return fz_new_brotli_data(ctx, compressed_length, data, size, level);
}

size_t fz_brotli_bound(fz_context *ctx, size_t size)
{
	return BrotliEncoderMaxCompressedSize(size);
}

#else

size_t fz_brotli_bound(fz_context *ctx, size_t size)
{
	return size;
}

void fz_compress_brotli(fz_context *ctx, unsigned char *dest, size_t *dest_len, const unsigned char *source, size_t source_len, fz_brotli_level level)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "brotli compression not enabled");
}

unsigned char *fz_new_brotli_data_from_buffer(fz_context *ctx, size_t *compressed_length, fz_buffer *buffer, fz_brotli_level level)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "brotli compression not enabled");
	return NULL;
}

unsigned char *fz_new_brotli_data(fz_context *ctx, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_brotli_level level)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "brotli compression not enabled");
	return NULL;
}

#endif
