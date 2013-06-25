#include "mupdf/pdf.h"

/*
 * CMap parser
 */

static int
pdf_code_from_string(char *buf, int len)
{
	int a = 0;
	while (len--)
		a = (a << 8) | *(unsigned char *)buf++;
	return a;
}

static void
pdf_parse_cmap_name(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;

	tok = pdf_lex(file, buf);

	if (tok == PDF_TOK_NAME)
		fz_strlcpy(cmap->cmap_name, buf->scratch, sizeof(cmap->cmap_name));
	else
		fz_warn(ctx, "expected name after CMapName in cmap");
}

static void
pdf_parse_wmode(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;

	tok = pdf_lex(file, buf);

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
		tok = pdf_lex(file, buf);

		if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "endcodespacerange"))
			return;

		else if (tok == PDF_TOK_STRING)
		{
			lo = pdf_code_from_string(buf->scratch, buf->len);
			tok = pdf_lex(file, buf);
			if (tok == PDF_TOK_STRING)
			{
				hi = pdf_code_from_string(buf->scratch, buf->len);
				pdf_add_codespace(ctx, cmap, lo, hi, buf->len);
			}
			else break;
		}

		else break;
	}

	fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or endcodespacerange");
}

static void
pdf_parse_cid_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int lo, hi, dst;

	while (1)
	{
		tok = pdf_lex(file, buf);

		if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "endcidrange"))
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or endcidrange");

		lo = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(file, buf);
		if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string");

		hi = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(file, buf);
		if (tok != PDF_TOK_INT)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected integer");

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
		tok = pdf_lex(file, buf);

		if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "endcidchar"))
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or endcidchar");

		src = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(file, buf);
		if (tok != PDF_TOK_INT)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected integer");

		dst = buf->i;

		pdf_map_range_to_range(ctx, cmap, src, src, dst);
	}
}

static void
pdf_parse_bf_range_array(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf, int lo, int hi)
{
	pdf_token tok;
	int dst[256];
	int i;

	while (1)
	{
		tok = pdf_lex(file, buf);

		if (tok == PDF_TOK_CLOSE_ARRAY)
			return;

		/* Note: does not handle [ /Name /Name ... ] */
		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or ]");

		if (buf->len / 2)
		{
			int len = fz_mini(buf->len / 2, nelem(dst));
			for (i = 0; i < len; i++)
				dst[i] = pdf_code_from_string(&buf->scratch[i * 2], 2);

			pdf_map_one_to_many(ctx, cmap, lo, dst, buf->len / 2);
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
		tok = pdf_lex(file, buf);

		if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "endbfrange"))
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or endbfrange");

		lo = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(file, buf);
		if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string");

		hi = pdf_code_from_string(buf->scratch, buf->len);
		if (lo < 0 || lo > 65535 || hi < 0 || hi > 65535 || lo > hi)
		{
			fz_warn(ctx, "bf_range limits out of range in cmap %s", cmap->cmap_name);
			return;
		}

		tok = pdf_lex(file, buf);

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
				int i;

				if (buf->len / 2)
				{
					int len = fz_mini(buf->len / 2, nelem(dststr));
					for (i = 0; i < len; i++)
						dststr[i] = pdf_code_from_string(&buf->scratch[i * 2], 2);

					while (lo <= hi)
					{
						dststr[i-1] ++;
						pdf_map_one_to_many(ctx, cmap, lo, dststr, i);
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
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or array or endbfrange");
		}
	}
}

static void
pdf_parse_bf_char(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;
	int dst[256];
	int src;
	int i;

	while (1)
	{
		tok = pdf_lex(file, buf);

		if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "endbfchar"))
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string or endbfchar");

		src = pdf_code_from_string(buf->scratch, buf->len);

		tok = pdf_lex(file, buf);
		/* Note: does not handle /dstName */
		if (tok != PDF_TOK_STRING)
			fz_throw(ctx, FZ_ERROR_GENERIC, "expected string");

		if (buf->len / 2)
		{
			int len = fz_mini(buf->len / 2, nelem(dst));
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
			tok = pdf_lex(file, &buf);

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
				if (!strcmp(buf.scratch, "endcmap"))
					break;

				else if (!strcmp(buf.scratch, "usecmap"))
					fz_strlcpy(cmap->usecmap_name, key, sizeof(cmap->usecmap_name));

				else if (!strcmp(buf.scratch, "begincodespacerange"))
					pdf_parse_codespace_range(ctx, cmap, file, &buf);

				else if (!strcmp(buf.scratch, "beginbfchar"))
					pdf_parse_bf_char(ctx, cmap, file, &buf);

				else if (!strcmp(buf.scratch, "begincidchar"))
					pdf_parse_cid_char(ctx, cmap, file, &buf);

				else if (!strcmp(buf.scratch, "beginbfrange"))
					pdf_parse_bf_range(ctx, cmap, file, &buf);

				else if (!strcmp(buf.scratch, "begincidrange"))
					pdf_parse_cid_range(ctx, cmap, file, &buf);
			}

			/* ignore everything else */
		}

		pdf_sort_cmap(ctx, cmap);
	}
	fz_always(ctx)
	{
		pdf_lexbuf_fin(&buf);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, cmap);
		fz_rethrow_message(ctx, "syntaxerror in cmap");
	}

	return cmap;
}
