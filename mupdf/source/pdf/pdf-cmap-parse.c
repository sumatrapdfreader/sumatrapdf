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
#include "mupdf/pdf.h"

#include <string.h>

/*
 * CMap parser
 */

static int
is_keyword(pdf_token tok, pdf_lexbuf *buf, const char *word)
{
	/* Ignore trailing garbage when matching keywords */
	return (tok == PDF_TOK_KEYWORD && !strncmp(buf->scratch, word, strlen(word)));
}

static void
skip_to_keyword(fz_context *ctx, fz_stream *file, pdf_lexbuf *buf, const char *end, const char *warn)
{
	fz_warn(ctx, "%s", warn);
	for (;;)
	{
		pdf_token tok = pdf_lex(ctx, file, buf);
		if (is_keyword(tok, buf, end))
			return;
		if (tok == PDF_TOK_ERROR)
			return;
		if (tok == PDF_TOK_EOF)
			return;
	}
}

static void
skip_to_token(fz_context *ctx, fz_stream *file, pdf_lexbuf *buf, pdf_token end, const char *warn)
{
	fz_warn(ctx, "%s", warn);
	for (;;)
	{
		pdf_token tok = pdf_lex(ctx, file, buf);
		if (tok == end)
			return;
		if (tok == PDF_TOK_ERROR)
			return;
		if (tok == PDF_TOK_EOF)
			return;
	}
}

static int
pdf_code_from_string(char *buf, size_t len)
{
	unsigned int a = 0;
	while (len--)
		a = (a << 8) | *(unsigned char *)buf++;
	return a;
}

static void
pdf_parse_cmap_name(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;

	tok = pdf_lex(ctx, file, buf);

	if (tok == PDF_TOK_NAME)
		fz_strlcpy(cmap->cmap_name, buf->scratch, sizeof(cmap->cmap_name));
	else
		fz_warn(ctx, "expected name after CMapName in cmap");
}

static void
pdf_parse_wmode(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;

	tok = pdf_lex(ctx, file, buf);

	if (tok == PDF_TOK_INT)
		pdf_set_cmap_wmode(ctx, cmap, buf->i);
	else
		fz_warn(ctx, "expected integer after WMode in cmap");
}

static void
pdf_parse_codespace_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int lo, hi;

	while (1)
	{
		tok = pdf_lex(ctx, file, buf);

		if (is_keyword(tok, buf, "endcodespacerange"))
			return;

		else if (tok == PDF_TOK_STRING)
		{
			lo = pdf_code_from_string(buf->scratch, buf->len);
			tok = pdf_lex(ctx, file, buf);
			if (tok == PDF_TOK_STRING)
			{
				hi = pdf_code_from_string(buf->scratch, buf->len);
				pdf_add_codespace(ctx, cmap, lo, hi, buf->len);
			}
			else
			{
				skip_to_keyword(ctx, file, buf, "endcodespacerange", "expected string or endcodespacerange");
				return;
			}
		}
		else
		{
			skip_to_keyword(ctx, file, buf, "endcodespacerange", "expected string or endcodespacerange");
			return;
		}
	}
}

static void
pdf_parse_cid_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int lo, hi, dst;

	while (1)
	{
		tok = pdf_lex(ctx, file, buf);

		if (is_keyword(tok, buf, "endcidrange"))
			return;

		else if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endcidrange", "expected string or endcidrange");
			return;
		}

		lo = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(ctx, file, buf);
		if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endcidrange", "expected string");
			return;
		}

		hi = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(ctx, file, buf);
		if (tok != PDF_TOK_INT)
		{
			skip_to_keyword(ctx, file, buf, "endcidrange", "expected integer");
			return;
		}

		dst = buf->i;

		pdf_map_range_to_range(ctx, cmap, lo, hi, dst);
	}
}

static void
pdf_parse_cid_char(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int src, dst;

	while (1)
	{
		tok = pdf_lex(ctx, file, buf);

		if (is_keyword(tok, buf, "endcidchar"))
			return;

		else if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endcidchar", "expected string or endcidchar");
			return;
		}

		src = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(ctx, file, buf);
		if (tok != PDF_TOK_INT)
		{
			skip_to_keyword(ctx, file, buf, "endcidchar", "expected integer");
			return;
		}

		dst = buf->i;

		pdf_map_range_to_range(ctx, cmap, src, src, dst);
	}
}

static void
pdf_parse_bf_range_array(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf, int lo, int hi)
{
	pdf_token tok;
	int dst[256];

	while (1)
	{
		tok = pdf_lex(ctx, file, buf);

		if (tok == PDF_TOK_CLOSE_ARRAY)
			return;

		/* Note: does not handle [ /Name /Name ... ] */
		else if (tok != PDF_TOK_STRING)
		{
			skip_to_token(ctx, file, buf, PDF_TOK_CLOSE_ARRAY, "expected string or ]");
			return;
		}

		if (buf->len / 2)
		{
			size_t i;
			size_t len = fz_minz(buf->len / 2, nelem(dst));
			for (i = 0; i < len; i++)
				dst[i] = pdf_code_from_string(&buf->scratch[i * 2], 2);

			pdf_map_one_to_many(ctx, cmap, lo, dst, i);
		}

		lo ++;
	}
}

static void
pdf_parse_bf_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int lo, hi, dst;

	while (1)
	{
		tok = pdf_lex(ctx, file, buf);

		if (is_keyword(tok, buf, "endbfrange"))
			return;

		else if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endbfrange", "expected string or endbfrange");
			return;
		}

		lo = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(ctx, file, buf);
		if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endbfrange", "expected string");
			return;
		}

		hi = pdf_code_from_string(buf->scratch, buf->len);
		if (lo < 0 || lo > 65535 || hi < 0 || hi > 65535 || lo > hi)
		{
			skip_to_keyword(ctx, file, buf, "endbfrange", "bfrange limits out of range");
			return;
		}

		tok = pdf_lex(ctx, file, buf);

		if (tok == PDF_TOK_STRING)
		{
			if (buf->len == 2)
			{
				dst = pdf_code_from_string(buf->scratch, buf->len);
				pdf_map_range_to_range(ctx, cmap, lo, hi, dst);
			}
			else
			{
				int dststr[256];
				size_t i;

				if (buf->len / 2)
				{
					size_t len = fz_minz(buf->len / 2, nelem(dststr));
					for (i = 0; i < len; i++)
						dststr[i] = pdf_code_from_string(&buf->scratch[i * 2], 2);

					while (lo <= hi)
					{
						pdf_map_one_to_many(ctx, cmap, lo, dststr, i);
						dststr[i-1] ++;
						lo ++;
					}
				}
			}
		}

		else if (tok == PDF_TOK_OPEN_ARRAY)
		{
			pdf_parse_bf_range_array(ctx, cmap, file, buf, lo, hi);
		}

		else
		{
			skip_to_keyword(ctx, file, buf, "endbfrange", "expected string or array or endbfrange");
			return;
		}
	}
}

static void
pdf_parse_bf_char(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int dst[256];
	int src;

	while (1)
	{
		tok = pdf_lex(ctx, file, buf);

		if (is_keyword(tok, buf, "endbfchar"))
			return;

		else if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endbfchar", "expected string or endbfchar");
			return;
		}

		src = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(ctx, file, buf);
		/* Note: does not handle /dstName */
		if (tok != PDF_TOK_STRING)
		{
			skip_to_keyword(ctx, file, buf, "endbfchar", "expected string");
			return;
		}

		if (buf->len / 2)
		{
			size_t i;
			size_t len = fz_minz(buf->len / 2, nelem(dst));
			for (i = 0; i < len; i++)
				dst[i] = pdf_code_from_string(&buf->scratch[i * 2], 2);
			pdf_map_one_to_many(ctx, cmap, src, dst, i);
		}
	}
}

pdf_cmap *
pdf_load_cmap(fz_context *ctx, fz_stream *file)
{
	pdf_cmap *cmap;
	char key[64];
	pdf_lexbuf buf;
	pdf_token tok;

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);
	cmap = pdf_new_cmap(ctx);

	strcpy(key, ".notdef");

	fz_try(ctx)
	{
		while (1)
		{
			tok = pdf_lex(ctx, file, &buf);

			if (tok == PDF_TOK_EOF)
				break;

			else if (tok == PDF_TOK_NAME)
			{
				if (!strcmp(buf.scratch, "CMapName"))
					pdf_parse_cmap_name(ctx, cmap, file, &buf);
				else if (!strcmp(buf.scratch, "WMode"))
					pdf_parse_wmode(ctx, cmap, file, &buf);
				else
					fz_strlcpy(key, buf.scratch, sizeof key);
			}

			else if (tok == PDF_TOK_KEYWORD)
			{
				if (is_keyword(tok, &buf, "endcmap"))
					break;

				else if (is_keyword(tok, &buf, "usecmap"))
					fz_strlcpy(cmap->usecmap_name, key, sizeof(cmap->usecmap_name));

				else if (is_keyword(tok, &buf, "begincodespacerange"))
					pdf_parse_codespace_range(ctx, cmap, file, &buf);

				else if (is_keyword(tok, &buf, "beginbfchar"))
					pdf_parse_bf_char(ctx, cmap, file, &buf);

				else if (is_keyword(tok, &buf, "begincidchar"))
					pdf_parse_cid_char(ctx, cmap, file, &buf);

				else if (is_keyword(tok, &buf, "beginbfrange"))
					pdf_parse_bf_range(ctx, cmap, file, &buf);

				else if (is_keyword(tok, &buf, "begincidrange"))
					pdf_parse_cid_range(ctx, cmap, file, &buf);
			}

			/* ignore everything else */
		}

		pdf_sort_cmap(ctx, cmap);
	}
	fz_always(ctx)
	{
		pdf_lexbuf_fin(ctx, &buf);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, cmap);
		fz_rethrow(ctx);
	}

	return cmap;
}
