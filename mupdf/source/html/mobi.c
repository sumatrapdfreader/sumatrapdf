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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

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
skip_bytes(fz_context *ctx, fz_stream *stm, size_t len)
{
	size_t skipped = fz_skip(ctx, stm, len);
	if (skipped < len)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in data");
}

static void
mobi_read_text_none(fz_context *ctx, fz_buffer *out, fz_stream *stm, uint32_t size)
{
	unsigned char buf[4096];
	size_t n;
	if (size > 4096)
		fz_throw(ctx, FZ_ERROR_GENERIC, "text block too large");
	n = fz_read(ctx, stm, buf, size);
	if (n < size)
		fz_warn(ctx, "premature end in mobi uncompressed text data");
	fz_append_data(ctx, out, buf, n);
}

static void
mobi_read_text_palmdoc(fz_context *ctx, fz_buffer *out, fz_stream *stm, uint32_t size)
{
	// https://wiki.mobileread.com/wiki/PalmDOC
	size_t end = out->len + size;
	while (out->len < end)
	{
		int c = fz_read_byte(ctx, stm);
		if (c == EOF)
			break;
		if (c >= 0x01 && c <= 0x08)
		{
			unsigned char buf[8];
			size_t n = fz_read(ctx, stm, buf, c);
			fz_append_data(ctx, out, buf, n);
			if (n < (size_t) c)
				break;
		}
		else if (c <= 0x7f)
		{
			fz_append_byte(ctx, out, c);
		}
		else if (c >= 0x80 && c <= 0xbf)
		{
			int cc, x, distance, length;
			cc = fz_read_byte(ctx, stm);
			if (cc == EOF)
				break;
			x = (c << 8) | cc;
			distance = (x >> 3) & 0x7ff;
			length = (x & 7) + 3;
			if (distance > 0 && (size_t)distance <= out->len)
			{
				int i;
				int p = (int)(out->len - distance);
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

	if (out->len < end)
		fz_warn(ctx, "premature end in mobi palmdoc data");
}

static uint32_t
mobi_read_data(fz_context *ctx, fz_buffer *out, fz_stream *stm, uint32_t *offset, uint32_t total_count, int format)
{
	// https://wiki.mobileread.com/wiki/MOBI
	uint32_t compression, text_length, record_count, text_encoding, i;
	unsigned char buf[4];
	fz_range range = { 0 };
	fz_stream *rec = NULL;
	size_t n;

	fz_var(rec);

	fz_try(ctx)
	{
		range.offset = offset[0];
		range.length = offset[1] - offset[0];
		rec = fz_open_range_filter(ctx, stm, &range, 1);

		// PalmDOC header
		compression = fz_read_uint16(ctx, rec);
		skip_bytes(ctx, rec, 2);
		text_length = fz_read_uint32(ctx, rec);
		record_count = fz_read_uint16(ctx, rec);
		skip_bytes(ctx, rec, 2);
		skip_bytes(ctx, rec, 2); // encryption
		skip_bytes(ctx, rec, 2);

		// Optional MOBI header
		text_encoding = TEXT_ENCODING_LATIN_1;
		n = fz_read(ctx, rec, buf, 4);
		if (n == 4 && !memcmp(buf, "MOBI", 4))
		{
			skip_bytes(ctx, rec, 4);
			skip_bytes(ctx, rec, 4);
			text_encoding = fz_read_uint32(ctx, rec);
		}
	}
	fz_always(ctx)
		fz_drop_stream(ctx, rec);
	fz_catch(ctx)
		fz_rethrow(ctx);

	if (compression != COMPRESSION_NONE && compression != COMPRESSION_PALMDOC)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown compression method");
	if (text_encoding != TEXT_ENCODING_LATIN_1 &&
		text_encoding != TEXT_ENCODING_1252 &&
		text_encoding != TEXT_ENCODING_UTF8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown text encoding");

	for (i = 1; i <= record_count && i < total_count; ++i)
	{
		uint32_t remain = text_length - (uint32_t)out->len;
		uint32_t size = remain < 4096 ? remain : 4096;

		fz_try(ctx)
		{
			range.offset = offset[i];
			range.length = offset[i + 1] - offset[i];
			rec = fz_open_range_filter(ctx, stm, &range, 1);

			if (compression == COMPRESSION_NONE)
				mobi_read_text_none(ctx, out, rec, size);
			else
				mobi_read_text_palmdoc(ctx, out, rec, size);
		}
		fz_always(ctx)
			fz_drop_stream(ctx, rec);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	if (format == FORMAT_TEXT && out->len > 6)
	{
		if (!memcmp(out->data, "<html>", 6) || !memcmp(out->data, "<HTML>", 6))
			format = FORMAT_HTML;
	}

	if (text_encoding != TEXT_ENCODING_UTF8 || format == FORMAT_TEXT)
	{
		unsigned char *p;
		size_t i, n = fz_buffer_extract(ctx, out, &p);
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
	uint32_t *offsets = NULL;
	char buf[32];
	uint32_t i, k, extra;
	uint32_t recindex;
	uint32_t minoffset, maxoffset;
	int format = FORMAT_TEXT;
	size_t n;

	// https://wiki.mobileread.com/wiki/PalmDOC

	fz_var(stm);
	fz_var(buffer);
	fz_var(offsets);
	fz_var(tree);

	fz_try(ctx)
	{
		stm = fz_open_buffer(ctx, mobi);

		skip_bytes(ctx, stm, 32); // database name
		skip_bytes(ctx, stm, 28); // database attributes, version, dates, etc

		n = fz_read(ctx, stm, (unsigned char *)buf, 8); // database type and creator
		buf[8] = 0;

		if (n == 8 && !memcmp(buf, "BOOKMOBI", 8))
			format = FORMAT_HTML;
		else if (n == 8 && !memcmp(buf, "TEXtREAd", 8))
			format = FORMAT_TEXT;
		else if (n != 8)
			fz_warn(ctx, "premature end in data");
		else
			fz_warn(ctx, "Unknown MOBI/PRC format: %s.", buf);

		skip_bytes(ctx, stm, 8); // database internal fields

		// record info list count
		n = fz_read_uint16(ctx, stm);

		minoffset = (uint32_t)fz_tell(ctx, stm) + n * 2 * sizeof (uint32_t) - 1;
		maxoffset = (uint32_t)mobi->len;

		// record info list
		offsets = fz_malloc_array(ctx, n + 1, uint32_t);
		for (i = 0, k = 0; i < n; ++i)
		{
			uint32_t offset = fz_read_uint32(ctx, stm);
			if (offset <= minoffset)
				continue;
			if (offset >= maxoffset)
				continue;
			offsets[k++] = offset;
			skip_bytes(ctx, stm, 4);
			minoffset = fz_mini(minoffset, offsets[i]);
		}
		offsets[k] = (uint32_t)mobi->len;

		// adjust n in case some out of bound offsets were skipped
		n = k;
		if (n == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "no mobi records to read");

		// decompress text data
		buffer = fz_new_buffer(ctx, 128 << 10);
		extra = mobi_read_data(ctx, buffer, stm, offsets, n, format);
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
			uint32_t size = offsets[i+1] - offsets[i];
			if (size > 8)
			{
				unsigned char *data = mobi->data + offsets[i];
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
		fz_free(ctx, offsets);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		fz_drop_tree(ctx, tree, drop_tree_entry);
		fz_rethrow(ctx);
	}

	return fz_new_tree_archive(ctx, tree);
}
