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

static fz_error
pdf_lex_cmap(int *tok, fz_stream *file, char *buf, int n, int *sl)
{
	fz_error error;

	error = pdf_lex(tok, file, buf, n, sl);
	if (error)
		return fz_rethrow(error, "cannot parse cmap token");

	if (*tok == PDF_TOK_KEYWORD)
		*tok = pdf_cmap_token_from_keyword(buf);

	return fz_okay;
}

static fz_error
pdf_parse_cmap_name(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;

	error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
	if (error)
		return fz_rethrow(error, "syntaxerror in cmap");

	if (tok == PDF_TOK_NAME)
		fz_strlcpy(cmap->cmap_name, buf, sizeof(cmap->cmap_name));
	else
		fz_warn("expected name after CMapName in cmap");

	return fz_okay;
}

static fz_error
pdf_parse_wmode(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;

	error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
	if (error)
		return fz_rethrow(error, "syntaxerror in cmap");

	if (tok == PDF_TOK_INT)
		pdf_set_wmode(cmap, atoi(buf));
	else
		fz_warn("expected integer after WMode in cmap");

	return fz_okay;
}

static fz_error
pdf_parse_codespace_range(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int lo, hi;

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TOK_END_CODESPACE_RANGE)
			return fz_okay;

		else if (tok == PDF_TOK_STRING)
		{
			lo = pdf_code_from_string(buf, len);
			error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
			if (error)
				return fz_rethrow(error, "syntaxerror in cmap");
			if (tok == PDF_TOK_STRING)
			{
				hi = pdf_code_from_string(buf, len);
				pdf_add_codespace(cmap, lo, hi, len);
			}
			else break;
		}

		else break;
	}

	return fz_throw("expected string or endcodespacerange");
}

static fz_error
pdf_parse_cid_range(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int lo, hi, dst;

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TOK_END_CID_RANGE)
			return fz_okay;

		else if (tok != PDF_TOK_STRING)
			return fz_throw("expected string or endcidrange");

		lo = pdf_code_from_string(buf, len);

		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TOK_STRING)
			return fz_throw("expected string");

		hi = pdf_code_from_string(buf, len);

		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TOK_INT)
			return fz_throw("expected integer");

		dst = atoi(buf);

		pdf_map_range_to_range(cmap, lo, hi, dst);
	}
}

static fz_error
pdf_parse_cid_char(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int src, dst;

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TOK_END_CID_CHAR)
			return fz_okay;

		else if (tok != PDF_TOK_STRING)
			return fz_throw("expected string or endcidchar");

		src = pdf_code_from_string(buf, len);

		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TOK_INT)
			return fz_throw("expected integer");

		dst = atoi(buf);

		pdf_map_range_to_range(cmap, src, src, dst);
	}
}

static fz_error
pdf_parse_bf_range_array(pdf_cmap *cmap, fz_stream *file, int lo, int hi)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int dst[256];
	int i;

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == PDF_TOK_CLOSE_ARRAY)
			return fz_okay;

		/* Note: does not handle [ /Name /Name ... ] */
		else if (tok != PDF_TOK_STRING)
			return fz_throw("expected string or ]");

		if (len / 2)
		{
			for (i = 0; i < len / 2; i++)
				dst[i] = pdf_code_from_string(buf + i * 2, 2);

			pdf_map_one_to_many(cmap, lo, dst, len / 2);
		}

		lo ++;
	}
}

static fz_error
pdf_parse_bf_range(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int lo, hi, dst;

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TOK_END_BF_RANGE)
			return fz_okay;

		else if (tok != PDF_TOK_STRING)
			return fz_throw("expected string or endbfrange");

		lo = pdf_code_from_string(buf, len);

		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TOK_STRING)
			return fz_throw("expected string");

		hi = pdf_code_from_string(buf, len);

		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == PDF_TOK_STRING)
		{
			if (len == 2)
			{
				dst = pdf_code_from_string(buf, len);
				pdf_map_range_to_range(cmap, lo, hi, dst);
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
						pdf_map_one_to_many(cmap, lo, dststr, i);
						lo ++;
					}
				}
			}
		}

		else if (tok == PDF_TOK_OPEN_ARRAY)
		{
			error = pdf_parse_bf_range_array(cmap, file, lo, hi);
			if (error)
				return fz_rethrow(error, "cannot map bfrange");
		}

		else
		{
			return fz_throw("expected string or array or endbfrange");
		}
	}
}

static fz_error
pdf_parse_bf_char(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int dst[256];
	int src;
	int i;

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TOK_END_BF_CHAR)
			return fz_okay;

		else if (tok != PDF_TOK_STRING)
			return fz_throw("expected string or endbfchar");

		src = pdf_code_from_string(buf, len);

		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		/* Note: does not handle /dstName */
		if (tok != PDF_TOK_STRING)
			return fz_throw("expected string");

		if (len / 2)
		{
			for (i = 0; i < len / 2; i++)
				dst[i] = pdf_code_from_string(buf + i * 2, 2);
			pdf_map_one_to_many(cmap, src, dst, i);
		}
	}
}

fz_error
pdf_parse_cmap(pdf_cmap **cmapp, fz_stream *file)
{
	fz_error error;
	pdf_cmap *cmap;
	char key[64];
	char buf[256];
	int tok;
	int len;

	cmap = pdf_new_cmap();

	strcpy(key, ".notdef");

	while (1)
	{
		error = pdf_lex_cmap(&tok, file, buf, sizeof buf, &len);
		if (error)
		{
			error = fz_rethrow(error, "syntaxerror in cmap");
			goto cleanup;
		}

		if (tok == PDF_TOK_EOF || tok == TOK_END_CMAP)
			break;

		else if (tok == PDF_TOK_NAME)
		{
			if (!strcmp(buf, "CMapName"))
			{
				error = pdf_parse_cmap_name(cmap, file);
				if (error)
				{
					error = fz_rethrow(error, "syntaxerror in cmap after CMapName");
					goto cleanup;
				}
			}
			else if (!strcmp(buf, "WMode"))
			{
				error = pdf_parse_wmode(cmap, file);
				if (error)
				{
					error = fz_rethrow(error, "syntaxerror in cmap after WMode");
					goto cleanup;
				}
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
			error = pdf_parse_codespace_range(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap codespacerange");
				goto cleanup;
			}
		}

		else if (tok == TOK_BEGIN_BF_CHAR)
		{
			error = pdf_parse_bf_char(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap bfchar");
				goto cleanup;
			}
		}

		else if (tok == TOK_BEGIN_CID_CHAR)
		{
			error = pdf_parse_cid_char(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap cidchar");
				goto cleanup;
			}
		}

		else if (tok == TOK_BEGIN_BF_RANGE)
		{
			error = pdf_parse_bf_range(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap bfrange");
				goto cleanup;
			}
		}

		else if (tok == TOK_BEGIN_CID_RANGE)
		{
			error = pdf_parse_cid_range(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap cidrange");
				goto cleanup;
			}
		}

		/* ignore everything else */
	}

	pdf_sort_cmap(cmap);

	*cmapp = cmap;
	return fz_okay;

cleanup:
	pdf_drop_cmap(cmap);
	return error; /* already rethrown */
}
