#include "fitz.h"
#include "mupdf.h"

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
pdf_lex_cmap(fz_stream *file, char *buf, int n, int *sl)
{
	int tok = pdf_lex(file, buf, n, sl);

	/* RJW: Lost debugging here: "cannot parse cmap token" */

	if (tok == PDF_TOK_KEYWORD)
		tok = pdf_cmap_token_from_keyword(buf);

	return tok;
}

static void
pdf_parse_cmap_name(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;

	tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
	/* RJW: Lost debugging: "syntaxerror in cmap" */

	if (tok == PDF_TOK_NAME)
		fz_strlcpy(cmap->cmap_name, buf, sizeof(cmap->cmap_name));
	else
		fz_warn(file->ctx, "expected name after CMapName in cmap");
}

static void
pdf_parse_wmode(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;

	tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
	/* RJW: Lost debugging: "syntaxerror in cmap" */

	if (tok == PDF_TOK_INT)
		pdf_set_wmode(cmap, atoi(buf));
	else
		fz_warn(file->ctx, "expected integer after WMode in cmap");
}

static void
pdf_parse_codespace_range(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;
	int lo, hi;

	while (1)
	{
		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: Lost debugging: "syntaxerror in cmap" */

		if (tok == TOK_END_CODESPACE_RANGE)
			return;

		else if (tok == PDF_TOK_STRING)
		{
			lo = pdf_code_from_string(buf, len);
			tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
			/* RJW: Lost debugging: "syntaxerror in cmap" */
			if (tok == PDF_TOK_STRING)
			{
				hi = pdf_code_from_string(buf, len);
				pdf_add_codespace(file->ctx, cmap, lo, hi, len);
			}
			else break;
		}

		else break;
	}

	fz_throw(file->ctx, "expected string or endcodespacerange");
}

static void
pdf_parse_cid_range(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;
	int lo, hi, dst;

	while (1)
	{
		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: Lost debugging: "syntaxerror in cmap" */

		if (tok == TOK_END_CID_RANGE)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string or endcidrange");

		lo = pdf_code_from_string(buf, len);

		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: Lost debugging: "syntaxerror in cmap" */
		if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string");

		hi = pdf_code_from_string(buf, len);

		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: Lost debugging: "syntaxerror in cmap" */
		if (tok != PDF_TOK_INT)
			fz_throw(file->ctx, "expected integer");

		dst = atoi(buf);

		pdf_map_range_to_range(file->ctx, cmap, lo, hi, dst);
	}
}

static void
pdf_parse_cid_char(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;
	int src, dst;

	while (1)
	{
		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */

		if (tok == TOK_END_CID_CHAR)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string or endcidchar");

		src = pdf_code_from_string(buf, len);

		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */

		if (tok != PDF_TOK_INT)
			fz_throw(file->ctx, "expected integer");

		dst = atoi(buf);

		pdf_map_range_to_range(file->ctx, cmap, src, src, dst);
	}
}

static void
pdf_parse_bf_range_array(pdf_cmap *cmap, fz_stream *file, int lo, int hi)
{
	char buf[256];
	int tok;
	int len;
	int dst[256];
	int i;

	while (1)
	{
		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */

		if (tok == PDF_TOK_CLOSE_ARRAY)
			return;

		/* Note: does not handle [ /Name /Name ... ] */
		else if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string or ]");

		if (len / 2)
		{
			for (i = 0; i < len / 2; i++)
				dst[i] = pdf_code_from_string(buf + i * 2, 2);

			pdf_map_one_to_many(file->ctx, cmap, lo, dst, len / 2);
		}

		lo ++;
	}
}

static void
pdf_parse_bf_range(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;
	int lo, hi, dst;

	while (1)
	{
		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */

		if (tok == TOK_END_BF_RANGE)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string or endbfrange");

		lo = pdf_code_from_string(buf, len);

		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */
		if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string");

		hi = pdf_code_from_string(buf, len);

		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */

		if (tok == PDF_TOK_STRING)
		{
			if (len == 2)
			{
				dst = pdf_code_from_string(buf, len);
				pdf_map_range_to_range(file->ctx, cmap, lo, hi, dst);
			}
			else
			{
				int dststr[256];
				int i;

				if (len / 2)
				{
					for (i = 0; i < len / 2; i++)
						dststr[i] = pdf_code_from_string(buf + i * 2, 2);

					while (lo <= hi)
					{
						dststr[i-1] ++;
						pdf_map_one_to_many(file->ctx, cmap, lo, dststr, i);
						lo ++;
					}
				}
			}
		}

		else if (tok == PDF_TOK_OPEN_ARRAY)
		{
			pdf_parse_bf_range_array(cmap, file, lo, hi);
			/* RJW: "cannot map bfrange" */
		}

		else
		{
			fz_throw(file->ctx, "expected string or array or endbfrange");
		}
	}
}

static void
pdf_parse_bf_char(pdf_cmap *cmap, fz_stream *file)
{
	char buf[256];
	int tok;
	int len;
	int dst[256];
	int src;
	int i;

	while (1)
	{
		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */

		if (tok == TOK_END_BF_CHAR)
			return;

		else if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string or endbfchar");

		src = pdf_code_from_string(buf, len);

		tok = pdf_lex_cmap(file, buf, sizeof buf, &len);
		/* RJW: "syntaxerror in cmap" */
		/* Note: does not handle /dstName */
		if (tok != PDF_TOK_STRING)
			fz_throw(file->ctx, "expected string");

		if (len / 2)
		{
			for (i = 0; i < len / 2; i++)
				dst[i] = pdf_code_from_string(buf + i * 2, 2);
			pdf_map_one_to_many(file->ctx, cmap, src, dst, i);
		}
	}
}

pdf_cmap *
pdf_parse_cmap(fz_stream *file)
{
	pdf_cmap *cmap;
	char key[64];
	char buf[256];
	int tok;
	int len;
	const char *where;
	fz_context *ctx = file->ctx;

	cmap = pdf_new_cmap(ctx);

	strcpy(key, ".notdef");

	fz_try(ctx)
	{
		while (1)
		{
			where = "";
			tok = pdf_lex_cmap(file, buf, sizeof buf, &len);

			if (tok == PDF_TOK_EOF || tok == TOK_END_CMAP)
				break;

			else if (tok == PDF_TOK_NAME)
			{
				if (!strcmp(buf, "CMapName"))
				{
					where = " after CMapName";
					pdf_parse_cmap_name(cmap, file);
				}
				else if (!strcmp(buf, "WMode"))
				{
					where = " after WMode";
					pdf_parse_wmode(cmap, file);
				}
				else
					fz_strlcpy(key, buf, sizeof key);
			}

			else if (tok == TOK_USECMAP)
			{
				fz_strlcpy(cmap->usecmap_name, key, sizeof(cmap->usecmap_name));
			}

			else if (tok == TOK_BEGIN_CODESPACE_RANGE)
			{
				where = " codespacerange";
				pdf_parse_codespace_range(cmap, file);
			}

			else if (tok == TOK_BEGIN_BF_CHAR)
			{
				where = " bfchar";
				pdf_parse_bf_char(cmap, file);
			}

			else if (tok == TOK_BEGIN_CID_CHAR)
			{
				where = " cidchar";
				pdf_parse_cid_char(cmap, file);
			}

			else if (tok == TOK_BEGIN_BF_RANGE)
			{
				where = " bfrange";
				pdf_parse_bf_range(cmap, file);
			}

			else if (tok == TOK_BEGIN_CID_RANGE)
			{
				where = "cidrange";
				pdf_parse_cid_range(cmap, file);
			}

			/* ignore everything else */
		}

		pdf_sort_cmap(file->ctx, cmap);
	}
	fz_catch(ctx)
	{
		pdf_drop_cmap(file->ctx, cmap);
		fz_throw(ctx, "syntaxerror in cmap%s", where);
	}

	return cmap;
}
