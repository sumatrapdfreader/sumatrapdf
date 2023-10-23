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

#include "z-imp.h"

#include <limits.h>

void fz_deflate(fz_context *ctx, unsigned char *dest, size_t *destLen, const unsigned char *source, size_t sourceLen, fz_deflate_level level)
{
	z_stream stream;
	int err;
	size_t left;

	left = *destLen;
	*destLen = 0;

	stream.zalloc = fz_zlib_alloc;
	stream.zfree = fz_zlib_free;
	stream.opaque = ctx;

	err = deflateInit(&stream, (int)level);
	if (err != Z_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "zlib compression failed: %d", err);

	stream.next_out = dest;
	stream.avail_out = 0;
	stream.next_in = (z_const Bytef *)source;
	stream.avail_in = 0;

	do {
		if (stream.avail_out == 0) {
			stream.avail_out = left > UINT_MAX ? UINT_MAX : (uInt)left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0) {
			stream.avail_in = sourceLen > UINT_MAX ? UINT_MAX : (uInt)sourceLen;
			sourceLen -= stream.avail_in;
		}
		err = deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
	} while (err == Z_OK);

	/* We might have problems if the compressed length > uLong sized. Tough, for now. */
	*destLen = stream.total_out;
	deflateEnd(&stream);
	if (err != Z_STREAM_END)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Zlib failure: %d", err);
}

unsigned char *fz_new_deflated_data(fz_context *ctx, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_deflate_level level)
{
	size_t bound = fz_deflate_bound(ctx, source_length);
	unsigned char *cdata = Memento_label(fz_malloc(ctx, bound), "deflated_data");
	*compressed_length = 0;

	fz_try(ctx)
		fz_deflate(ctx, cdata, &bound, source, source_length, level);
	fz_catch(ctx)
	{
		fz_free(ctx, cdata);
		fz_rethrow(ctx);
	}

	*compressed_length = bound;
	return cdata;
}

unsigned char *fz_new_deflated_data_from_buffer(fz_context *ctx, size_t *compressed_length, fz_buffer *buffer, fz_deflate_level level)
{
	unsigned char *data;
	size_t size = fz_buffer_storage(ctx, buffer, &data);

	if (size == 0 || data == NULL)
	{
		*compressed_length = 0;
		return NULL;
	}

	return fz_new_deflated_data(ctx, compressed_length, data, size, level);
}

size_t fz_deflate_bound(fz_context *ctx, size_t size)
{
	/* Copied from zlib to account for size_t vs uLong */
	return size + (size >> 12) + (size >> 14) + (size >> 25) + 13;
}
