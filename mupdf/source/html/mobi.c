// Copyright (C) 2004-2022 Artifex Software, Inc.
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
#include "html-imp.h"

#include <string.h>

#define FORMAT_HTML 1
#define FORMAT_TEXT 2

#define COMPRESSION_NONE 1
#define COMPRESSION_PALMDOC 2
#define COMPRESSION_HUFF_CDIC 17480

#define TEXT_ENCODING_LATIN_1 0
#define TEXT_ENCODING_1252 1252
#define TEXT_ENCODING_UTF8 65001

static void
mobi_read_text_none(fz_context *ctx, fz_buffer *out, fz_stream *stm, uint32_t size)
{
	unsigned char buf[4096];
	if (size > 4096)
		fz_throw(ctx, FZ_ERROR_GENERIC, "text block too large");
	fz_read(ctx, stm, buf, size);
	fz_append_data(ctx, out, buf, size);
}

static void
mobi_read_text_palmdoc(fz_context *ctx, fz_buffer *out, fz_stream *stm, uint32_t size)
{
	// https://wiki.mobileread.com/wiki/PalmDOC
	uint32_t end = out->len + size;
	while (out->len < end)
	{
		int c = fz_read_byte(ctx, stm);
		if (c == EOF)
			break;
		if (c >= 0x01 && c <= 0x08)
		{
			unsigned char buf[8];
			fz_read(ctx, stm, buf, c);
			fz_append_data(ctx, out, buf, c);
		}
		else if (c <= 0x7f)
		{
			fz_append_byte(ctx, out, c);
		}
		else if (c >= 0x80 && c <= 0xbf)
		{
			int x = (c << 8) | fz_read_byte(ctx, stm);
			int distance = (x >> 3) & 0x7ff;
			int length = (x & 7) + 3;
			int p = out->len - distance;
			if (p >= 0 && p < (int)out->len)
			{
				int i;
				for (i = 0; i < length; ++i)
					fz_append_byte(ctx, out, out->data[p + i]);
			}
		}
		else if (c >= 0xc0 && c <= 0xff)
		{
			fz_append_byte(ctx, out, ' ');
			fz_append_byte(ctx, out, c ^ 0x80);
		}
	}
}

static uint32_t
mobi_read_data(fz_context *ctx, fz_buffer *out, fz_stream *stm, uint32_t *offset, uint32_t total_count, int format)
{
	// https://wiki.mobileread.com/wiki/MOBI
	uint32_t compression, text_length, record_count, text_encoding;
	uint32_t i;
	unsigned char buf[4];

	fz_seek(ctx, stm, offset[0], 0);

	// PalmDOC header
	compression = fz_read_uint16(ctx, stm);
	fz_skip(ctx, stm, 2);
	text_length = fz_read_uint32(ctx, stm);
	record_count = fz_read_uint16(ctx, stm);
	fz_skip(ctx, stm, 2);
	fz_skip(ctx, stm, 2); // encryption
	fz_skip(ctx, stm, 2);

	// Optional MOBI header
	fz_read(ctx, stm, buf, 4);
	if (!memcmp(buf, "MOBI", 4))
	{
		fz_skip(ctx, stm, 8);
		text_encoding = fz_read_uint32(ctx, stm);
	}
	else
	{
		text_encoding = TEXT_ENCODING_LATIN_1;
	}

	if (compression != COMPRESSION_NONE && compression != COMPRESSION_PALMDOC)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown compression method");
	if (text_encoding != TEXT_ENCODING_LATIN_1 &&
		text_encoding != TEXT_ENCODING_1252 &&
		text_encoding != TEXT_ENCODING_UTF8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown text encoding");

	for (i = 1; i <= record_count && i < total_count; ++i)
	{
		uint32_t remain = text_length - out->len;
		uint32_t size = remain < 4096 ? remain : 4096;
		fz_seek(ctx, stm, offset[i], 0);
		if (compression == COMPRESSION_NONE)
			mobi_read_text_none(ctx, out, stm, size);
		else
			mobi_read_text_palmdoc(ctx, out, stm, size);
	}

	if (format == FORMAT_TEXT && out->len > 6)
	{
		if (!memcmp(out->data, "<html>", 6) || !memcmp(out->data, "<HTML>", 6))
			format = FORMAT_HTML;
	}

	if (text_encoding != TEXT_ENCODING_UTF8 || format == FORMAT_TEXT)
	{
		unsigned char *p;
		int i, n = fz_buffer_extract(ctx, out, &p);
		fz_resize_buffer(ctx, out, 0);
		if (format == FORMAT_TEXT)
			fz_append_string(ctx, out, "<html><head><style>body{white-space:pre-wrap}</style></head><body>");
		for (i = 0; i < n; ++i)
		{
			int c = p[i];
			if (format == FORMAT_TEXT && (c == '<' || c == '>' || c == '&'))
			{
				if (c == '<')
					fz_append_string(ctx, out, "&lt;");
				else if (c == '>')
					fz_append_string(ctx, out, "&gt;");
				else if (c == '&')
					fz_append_string(ctx, out, "&amp;");
			}
			else
			{
				switch (text_encoding)
				{
				case TEXT_ENCODING_UTF8:
					fz_append_byte(ctx, out, c);
					break;
				case TEXT_ENCODING_LATIN_1:
					fz_append_rune(ctx, out, c);
					break;
				case TEXT_ENCODING_1252:
					fz_append_rune(ctx, out, fz_unicode_from_windows_1252[c]);
					break;
				}
			}
		}
		if (format == FORMAT_TEXT)
			fz_append_string(ctx, out, "</body></html>");
		fz_free(ctx, p);
	}

	return record_count;
}

static void drop_tree_entry(fz_context *ctx, void *ent)
{
	fz_drop_buffer(ctx, ent);
}

fz_archive *
fz_extract_html_from_mobi(fz_context *ctx, fz_buffer *mobi)
{
	fz_stream *stm = NULL;
	fz_buffer *buffer = NULL;
	fz_tree *tree = NULL;
	uint32_t *offset = NULL;
	char buf[32];
	uint32_t i, n, extra;
	uint32_t recindex;
	int format = FORMAT_TEXT;

	// https://wiki.mobileread.com/wiki/PalmDOC

	fz_var(stm);
	fz_var(buffer);
	fz_var(offset);
	fz_var(tree);

	fz_try(ctx)
	{
		stm = fz_open_buffer(ctx, mobi);

		fz_skip(ctx, stm, 32); // database name
		fz_skip(ctx, stm, 28); // database attributes, version, dates, etc

		fz_read(ctx, stm, (unsigned char *)buf, 8); // database type and creator
		buf[8] = 0;

		if (!memcmp(buf, "BOOKMOBI", 8))
			format = FORMAT_HTML;
		else if (!memcmp(buf, "TEXtREAd", 8))
			format = FORMAT_TEXT;
		else
			fz_warn(ctx, "Unknown MOBI/PRC format: %s.", buf);

		fz_skip(ctx, stm, 8); // database internal fields

		// record info list
		n = fz_read_uint16(ctx, stm);
		offset = fz_malloc_array(ctx, n + 1, uint32_t);
		for (i = 0; i < n; ++i)
		{
			offset[i] = fz_read_uint32(ctx, stm);
			fz_skip(ctx, stm, 4);
		}
		offset[n] = mobi->len;

		// decompress text data
		buffer = fz_new_buffer(ctx, 128 << 10);
		extra = mobi_read_data(ctx, buffer, stm, offset, n, format);
		fz_terminate_buffer(ctx, buffer);

#ifndef NDEBUG
		if (fz_atoi(getenv("FZ_DEBUG_MOBI")))
			fz_save_buffer(ctx, buffer, "mobi.xhtml");
#endif

		tree = fz_tree_insert(ctx, tree, "index.html", buffer);
		buffer = NULL;

		// copy image data records into tree
		recindex = 1;
		for (i = extra; i < n; ++i)
		{
			uint32_t size = offset[i+1] - offset[i];
			if (size > 8)
			{
				unsigned char *data = mobi->data + offset[i];
				if (fz_recognize_image_format(ctx, data))
				{
					buffer = fz_new_buffer_from_copied_data(ctx, data, size);
					fz_snprintf(buf, sizeof buf, "%05d", recindex);
					tree = fz_tree_insert(ctx, tree, buf, buffer);
					buffer = NULL;
					recindex++;
				}
			}
		}
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_free(ctx, offset);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		fz_drop_tree(ctx, tree, drop_tree_entry);
		fz_rethrow(ctx);
	}

	return fz_new_tree_archive(ctx, tree);
}
