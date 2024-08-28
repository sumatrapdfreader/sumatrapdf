// Copyright (C) 2023-2024 Artifex Software, Inc.
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

#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

static fz_document *
gz_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *ostm, fz_stream *accel, fz_archive *dir, void *state)
{
	fz_stream *stm = fz_open_flated(ctx, ostm, 16 + MAX_WBITS);
	fz_buffer *buf = NULL;
	fz_document *doc = NULL;

	fz_var(buf);
	fz_var(doc);

	fz_try(ctx)
	{
		buf = fz_read_all(ctx, stm, 1024);
		/* No way to pass the magic onwards :( */
		doc = fz_open_document_with_buffer(ctx, "application/octet-stream", buf);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return doc;
}

static const char *gz_extensions[] =
{
	"gz",
	NULL
};

static const char *gz_mimetypes[] =
{
	"application/x-gzip-compressed",
	NULL
};

static int
gz_recognize_doc_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *dir, void **state, fz_document_recognize_state_free_fn **free_state)
{
	int ret = 0;
	uint8_t data[10];

	if (state)
		*state = NULL;
	if (free_state)
		*free_state = NULL;

	if (stream == NULL)
		return 0;

	fz_try(ctx)
	{
		fz_seek(ctx, stream, 0, SEEK_SET);
		/* 10 byte header */
		if (fz_read(ctx, stream, data, 10) != 10)
			break;
		/* We only actually check the first 3 bytes though. */
		if (data[0] == 0x1f && data[1] == 0x8b && data[2] == 0x08)
			ret = 100;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_document_handler gz_document_handler =
{
	NULL,
	gz_open_document,
	gz_extensions,
	gz_mimetypes,
	gz_recognize_doc_content
};
