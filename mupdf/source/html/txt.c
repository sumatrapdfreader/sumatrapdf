// Copyright (C) 2023 Artifex Software, Inc.
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
#include "mupdf/html.h"

enum { ENCODING_ASCII, ENCODING_UTF8, ENCODING_UTF8_BOM, ENCODING_UTF16_LE, ENCODING_UTF16_BE };

static int
detect_txt_encoding(fz_context *ctx, fz_buffer *buf)
{
	const uint8_t *d = buf->data;
	size_t len = buf->len;
	const uint8_t *end = buf->data + len;
	int count_tabs = 0;
	int count_hi = 0;
	int count_controls = 0;
	int plausibly_utf8 = 1;

	/* If we find a BOM, believe it. */
	if (len >= 3 && d[0] == 0xef && d[1] == 0xbb && d[2] == 0xBF)
		return ENCODING_UTF8_BOM;
	else if (len >= 2 && d[0] == 0xff && d[1] == 0xfe)
		return ENCODING_UTF16_LE;
	else if (len >= 2 && d[0] == 0xfe && d[1] == 0xff)
		return ENCODING_UTF16_BE;

	while (d < end)
	{
		uint8_t c = *d++;
		if (c == 9)
			count_tabs++;
		else if (c == 12)
		{
			/* Form feed. Ignore that. */
		}
		else if (c == 10)
		{
			if (d < end && d[0] == 13)
				d++;
		}
		else if (c == 13)
		{
			if (d < end && d[0] == 10)
				d++;
		}
		else if (c < 32 || c == 0x7f)
			count_controls++;
		else if (c < 0x7f)
		{
			/* Reasonable ASCII value */
		}
		else
		{
			count_hi++;
			if ((c & 0xf8) == 0xF0)
			{
				/* Could be UTF8 with 3 following bytes */
				if (d+2 >= end ||
					(d[0] & 0xC0) != 0x80 ||
					(d[1] & 0xC0) != 0x80 ||
					(d[2] & 0xC0) != 0x80)
					plausibly_utf8 = 0;
				else
					d += 3;
			}
			else if ((c & 0xf0) == 0xE0)
			{
				/* Could be UTF8 with 2 following bytes */
				if (d+1 >= end ||
					(d[0] & 0xC0) != 0x80 ||
					(d[1] & 0xC0) != 0x80)
					plausibly_utf8 = 0;
				else
					d += 2;
			}
			else if ((c & 0xE0) == 0xC0)
			{
				/* Could be UTF8 with 1 following bytes */
				if (d+1 >= end ||
					(d[0] & 0xC0) != 0x80)
					plausibly_utf8 = 0;
				else
					d++;
			}
			else
				plausibly_utf8 = 0;
		}
	}

	if (plausibly_utf8)
		return ENCODING_UTF8;
	return ENCODING_ASCII;
}

fz_buffer *
fz_txt_buffer_to_html(fz_context *ctx, fz_buffer *in)
{
	int encoding = detect_txt_encoding(ctx, in);
	fz_stream *stream = fz_open_buffer(ctx, in);
	fz_buffer *outbuf = NULL;
	fz_output *out = NULL;
	int col = 0;

	fz_var(outbuf);
	fz_var(out);

	fz_try(ctx)
	{
		outbuf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, outbuf);

		fz_write_string(ctx, out, "<!doctype html><style>body{margin:0}pre{page-break-before:always;margin:0;white-space:pre-wrap;}</style><pre>");

		if (encoding == ENCODING_UTF16_LE || encoding == ENCODING_UTF16_BE)
		{
			fz_read_byte(ctx, stream);
			fz_read_byte(ctx, stream);
		}
		else if (encoding == ENCODING_UTF8_BOM)
		{
			fz_read_byte(ctx, stream);
			fz_read_byte(ctx, stream);
			fz_read_byte(ctx, stream);
		}

		while (!fz_is_eof(ctx, stream))
		{
			int c;
			switch (encoding)
			{
			default:
			case ENCODING_ASCII:
				c = fz_read_byte(ctx, stream);
				break;
			case ENCODING_UTF8:
			case ENCODING_UTF8_BOM:
				c = fz_read_rune(ctx, stream);
				break;
			case ENCODING_UTF16_LE:
				c = fz_read_utf16_le(ctx, stream);
				break;
			case ENCODING_UTF16_BE:
				c = fz_read_utf16_be(ctx, stream);
			}

			if (c == 10 || c == 13)
			{
				col = -1;
				fz_write_byte(ctx, out, c);
			}
			else if (c == 9)
			{
				int n = (8 - col) & 7;
				if (n == 0)
					n = 8;
				col += n-1;
				while (n--)
					fz_write_byte(ctx, out, ' ');
			}
			else if (c == 12)
			{
				col = -1;
				fz_write_string(ctx, out, "</pre><pre>\n");
			}
			else if (c == '<')
				fz_write_string(ctx, out, "&lt;");
			else if (c == '>')
				fz_write_string(ctx, out, "&gt;");
			else if (c == '"')
				fz_write_string(ctx, out, "&quot;");
			else
				fz_write_rune(ctx, out, c);

			++col;
		}

		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stream);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, outbuf);
		fz_rethrow(ctx);
	}

	return outbuf;
}

static fz_buffer *
txt_to_html(fz_context *ctx, fz_html_font_set *set, fz_buffer *buf, const char *user_css)
{
	return fz_txt_buffer_to_html(ctx, buf);
}

static const fz_htdoc_format_t fz_htdoc_txt =
{
	"Text",
	txt_to_html,
	0, 1, 0
};

static fz_document *
txt_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	return fz_htdoc_open_document_with_stream(ctx, file, &fz_htdoc_txt);
}

static fz_document *
txt_open_document(fz_context *ctx, const char *filename)
{
	return fz_htdoc_open_document_with_file(ctx, filename, &fz_htdoc_txt);
}

static const char *txt_extensions[] =
{
	"txt",
	"text",
	NULL
};

static const char *txt_mimetypes[] =
{
	"text.plain",
	NULL
};

fz_document_handler txt_document_handler =
{
	NULL,
	txt_open_document,
	txt_open_document_with_stream,
	txt_extensions,
	txt_mimetypes
};
