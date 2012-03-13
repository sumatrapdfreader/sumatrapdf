#include "fitz-internal.h"
#include "mupdf-internal.h"

/*
 * CMap parser
 */

enum
{
	TOK_USECMAP = PDF_NUM_TOKENS,
	TOK_BEGIN_CODESPACE_RANGE,
	TOK_END_CODESPACE_RANGE,
	TOK_BEGIN_BF_CHAR,
	TOK_END_BF_CHAR,
	TOK_BEGIN_BF_RANGE,
	TOK_END_BF_RANGE,
	TOK_BEGIN_CID_CHAR,
	TOK_END_CID_CHAR,
	TOK_BEGIN_CID_RANGE,
	TOK_END_CID_RANGE,
	TOK_END_CMAP
};

static int
pdf_cmap_token_from_keyword(char *key)
{
	if (!strcmp(key, "usecmap")) return TOK_USECMAP;
	if (!strcmp(key, "begincodespacerange")) return TOK_BEGIN_CODESPACE_RANGE;
	if (!strcmp(key, "endcodespacerange")) return TOK_END_CODESPACE_RANGE;
	if (!strcmp(key, "beginbfchar")) return TOK_BEGIN_BF_CHAR;
	if (!strcmp(key, "endbfchar")) return TOK_END_BF_CHAR;
	if (!strcmp(key, "beginbfrange")) return TOK_BEGIN_BF_RANGE;
	if (!strcmp(key, "endbfrange")) return TOK_END_BF_RANGE;
	if (!strcmp(key, "begincidchar")) return TOK_BEGIN_CID_CHAR;
	if (!strcmp(key, "endcidchar")) return TOK_END_CID_CHAR;
	if (!strcmp(key, "begincidrange")) return TOK_BEGIN_CID_RANGE;
	if (!strcmp(key, "endcidrange")) return TOK_END_CID_RANGE;
	if (!strcmp(key, "endcmap")) return TOK_END_CMAP;
	return PDF_TOK_KEYWORD;
}

static int
pdf_code_from_string(char *buf, int len)
{
	int a = 0;
	while (len--)
		a = (a << 8) | *(unsigned char *)buf++;
	return a;
}

static int
pdf_lex_cmap(fz_stream *file, pdf_lexbuf *buf)
{
	int tok = pdf_lex(file, buf);

	/* RJW: Lost debugging here: "cannot parse cmap token" */

	if (tok == PDF_TOK_KEYWORD)
		tok = pdf_cmap_token_from_keyword(buf->scratch);

	return tok;
}

static void
pdf_parse_cmap_name(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;

	buf.size = PDF_LEXBUF_SMALL;
	tok = pdf_lex_cmap(file, &buf);
	/* RJW: Lost debugging: "syntaxerror in cmap" */

	if (tok == PDF_TOK_NAME)
		fz_strlcpy(cmap->cmap_name, buf.scratch, sizeof(cmap->cmap_name));
	else
		fz_warn(ctx, "expected name after CMapName in cmap");
}

static void
pdf_parse_wmode(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;

	buf.size = PDF_LEXBUF_SMALL;
	tok = pdf_lex_cmap(file, &buf);
	/* RJW: Lost debugging: "syntaxerror in cmap" */

	if (tok == PDF_TOK_INT)
		pdf_set_cmap_wmode(ctx, cmap, buf.i);
	else
		fz_warn(ctx, "expected integer after WMode in cmap");
}

static void
pdf_parse_codespace_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;
	int lo, hi;

	buf.size = PDF_LEXBUF_SMALL;
	while (1)
	{
		tok = pdf_lex_cmap(file, &buf);
		/* RJW: Lost debugging: "syntaxerror in cmap" */

		if (tok == TOK_END_CODESPACE_RANGE)
			return;

		else if (tok == PDF_TOK_STRING)
		{
			lo = pdf_code_from_string(buf.scratch, buf.len);
			tok = pdf_lex_cmap(file, &buf);
			/* RJW: Lost debugging: "syntaxerror in cmap" */
			if (tok == PDF_TOK_STRING)
			{
				hi = pdf_code_from_string(buf.scratch, buf.len);
				pdf_add_codespace(ctx, cmap, lo, hi, buf.len);
			}
			else break;
		}

		else break;
	}

	fz_throw(ctx, "expected string or endcodespacerange");
}

static void
pdf_parse_cid_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;
	int lo, hi, dst;

	buf.size = PDF_LEXBUF_SMALL;
	while (1)
	{
		tok = pdf_lex_cmap(file, &buf);
		/* RJW: Lost debugging: "syntaxerror in cmap" */

		if (tok == TOK_END_CID_RANGE)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string or endcidrange");

		lo = pdf_code_from_string(buf.scratch, buf.len);

		tok = pdf_lex_cmap(file, &buf);
		/* RJW: Lost debugging: "syntaxerror in cmap" */
		if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string");

		hi = pdf_code_from_string(buf.scratch, buf.len);

		tok = pdf_lex_cmap(file, &buf);
		/* RJW: Lost debugging: "syntaxerror in cmap" */
		if (tok != PDF_TOK_INT)
			fz_throw(ctx, "expected integer");

		dst = buf.i;

		pdf_map_range_to_range(ctx, cmap, lo, hi, dst);
	}
}

static void
pdf_parse_cid_char(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;
	int src, dst;

	buf.size = PDF_LEXBUF_SMALL;
	while (1)
	{
		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */

		if (tok == TOK_END_CID_CHAR)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string or endcidchar");

		src = pdf_code_from_string(buf.scratch, buf.len);

		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */

		if (tok != PDF_TOK_INT)
			fz_throw(ctx, "expected integer");

		dst = buf.i;

		pdf_map_range_to_range(ctx, cmap, src, src, dst);
	}
}

static void
pdf_parse_bf_range_array(fz_context *ctx, pdf_cmap *cmap, fz_stream *file, int lo, int hi)
{
	pdf_lexbuf buf;
	int tok;
	int dst[256];
	int i;

	buf.size = PDF_LEXBUF_SMALL;
	while (1)
	{
		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */

		if (tok == PDF_TOK_CLOSE_ARRAY)
			return;

		/* Note: does not handle [ /Name /Name ... ] */
		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string or ]");

		if (buf.len / 2)
		{
			for (i = 0; i < buf.len / 2; i++)
				dst[i] = pdf_code_from_string(&buf.scratch[i * 2], 2);

			pdf_map_one_to_many(ctx, cmap, lo, dst, buf.len / 2);
		}

		lo ++;
	}
}

static void
pdf_parse_bf_range(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;
	int lo, hi, dst;

	buf.size = PDF_LEXBUF_SMALL;
	while (1)
	{
		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */

		if (tok == TOK_END_BF_RANGE)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string or endbfrange");

		lo = pdf_code_from_string(buf.scratch, buf.len);

		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */
		if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string");

		hi = pdf_code_from_string(buf.scratch, buf.len);

		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */

		if (tok == PDF_TOK_STRING)
		{
			if (buf.len == 2)
			{
				dst = pdf_code_from_string(buf.scratch, buf.len);
				pdf_map_range_to_range(ctx, cmap, lo, hi, dst);
			}
			else
			{
				int dststr[256];
				int i;

				if (buf.len / 2)
				{
					for (i = 0; i < buf.len / 2; i++)
						dststr[i] = pdf_code_from_string(&buf.scratch[i * 2], 2);

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
			pdf_parse_bf_range_array(ctx, cmap, file, lo, hi);
			/* RJW: "cannot map bfrange" */
		}

		else
		{
			fz_throw(ctx, "expected string or array or endbfrange");
		}
	}
}

static void
pdf_parse_bf_char(fz_context *ctx, pdf_cmap *cmap, fz_stream *file)
{
	pdf_lexbuf buf;
	int tok;
	int dst[256];
	int src;
	int i;

	buf.size = PDF_LEXBUF_SMALL;
	while (1)
	{
		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */

		if (tok == TOK_END_BF_CHAR)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string or endbfchar");

		src = pdf_code_from_string(buf.scratch, buf.len);

		tok = pdf_lex_cmap(file, &buf);
		/* RJW: "syntaxerror in cmap" */
		/* Note: does not handle /dstName */
		if (tok != PDF_TOK_STRING)
			fz_throw(ctx, "expected string");

		if (buf.len / 2)
		{
			for (i = 0; i < buf.len / 2; i++)
				dst[i] = pdf_code_from_string(&buf.scratch[i * 2], 2);
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
	int tok;
	const char *where;

	buf.size = PDF_LEXBUF_SMALL;
	cmap = pdf_new_cmap(ctx);

	strcpy(key, ".notdef");

	fz_var(where);

	fz_try(ctx)
	{
		while (1)
		{
			where = "";
			tok = pdf_lex_cmap(file, &buf);

			if (tok == PDF_TOK_EOF || tok == TOK_END_CMAP)
				break;

			else if (tok == PDF_TOK_NAME)
			{
				if (!strcmp(buf.scratch, "CMapName"))
				{
					where = " after CMapName";
					pdf_parse_cmap_name(ctx, cmap, file);
				}
				else if (!strcmp(buf.scratch, "WMode"))
				{
					where = " after WMode";
					pdf_parse_wmode(ctx, cmap, file);
				}
				else
					fz_strlcpy(key, buf.scratch, sizeof key);
			}

			else if (tok == TOK_USECMAP)
			{
				fz_strlcpy(cmap->usecmap_name, key, sizeof(cmap->usecmap_name));
			}

			else if (tok == TOK_BEGIN_CODESPACE_RANGE)
			{
				where = " codespacerange";
				pdf_parse_codespace_range(ctx, cmap, file);
			}

			else if (tok == TOK_BEGIN_BF_CHAR)
			{
				where = " bfchar";
				pdf_parse_bf_char(ctx, cmap, file);
			}

			else if (tok == TOK_BEGIN_CID_CHAR)
			{
				where = " cidchar";
				pdf_parse_cid_char(ctx, cmap, file);
			}

			else if (tok == TOK_BEGIN_BF_RANGE)
			{
				where = " bfrange";
				pdf_parse_bf_range(ctx, cmap, file);
			}

			else if (tok == TOK_BEGIN_CID_RANGE)
			{
				where = "cidrange";
				pdf_parse_cid_range(ctx, cmap, file);
			}

			/* ignore everything else */
		}

		pdf_sort_cmap(ctx, cmap);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(ctx, cmap);
		fz_throw(ctx, "syntaxerror in cmap%s", where);
	}

	return cmap;
}
