#include "fitz.h"
#include "mupdf.h"

/*
 * CMap parser
 */

enum
{
	TUSECMAP = PDF_NTOKENS,
	TBEGINCODESPACERANGE,
	TENDCODESPACERANGE,
	TBEGINBFCHAR,
	TENDBFCHAR,
	TBEGINBFRANGE,
	TENDBFRANGE,
	TBEGINCIDCHAR,
	TENDCIDCHAR,
	TBEGINCIDRANGE,
	TENDCIDRANGE,
	TENDCMAP
};

static int
pdf_cmaptokenfromkeyword(char *key)
{
	if (!strcmp(key, "usecmap")) return TUSECMAP;
	if (!strcmp(key, "begincodespacerange")) return TBEGINCODESPACERANGE;
	if (!strcmp(key, "endcodespacerange")) return TENDCODESPACERANGE;
	if (!strcmp(key, "beginbfchar")) return TBEGINBFCHAR;
	if (!strcmp(key, "endbfchar")) return TENDBFCHAR;
	if (!strcmp(key, "beginbfrange")) return TBEGINBFRANGE;
	if (!strcmp(key, "endbfrange")) return TENDBFRANGE;
	if (!strcmp(key, "begincidchar")) return TBEGINCIDCHAR;
	if (!strcmp(key, "endcidchar")) return TENDCIDCHAR;
	if (!strcmp(key, "begincidrange")) return TBEGINCIDRANGE;
	if (!strcmp(key, "endcidrange")) return TENDCIDRANGE;
	if (!strcmp(key, "endcmap")) return TENDCMAP;
	return PDF_TKEYWORD;
}

static int
pdf_codefromstring(char *buf, int len)
{
	int a = 0;
	while (len--)
		a = (a << 8) | *(unsigned char *)buf++;
	return a;
}

static fz_error
pdf_lexcmap(int *tok, fz_stream *file, char *buf, int n, int *sl)
{
	fz_error error;

	error = pdf_lex(tok, file, buf, n, sl);
	if (error)
		return fz_rethrow(error, "cannot parse cmap token");

	if (*tok == PDF_TKEYWORD)
		*tok = pdf_cmaptokenfromkeyword(buf);

	return fz_okay;
}

static fz_error
pdf_parsecmapname(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;

	error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
	if (error)
		return fz_rethrow(error, "syntaxerror in cmap");

	if (tok == PDF_TNAME)
		fz_strlcpy(cmap->cmapname, buf, sizeof(cmap->cmapname));
	else
		fz_warn("expected name after CMapName in cmap");

	return fz_okay;
}

static fz_error
pdf_parsewmode(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;

	error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
	if (error)
		return fz_rethrow(error, "syntaxerror in cmap");

	if (tok == PDF_TINT)
		pdf_setwmode(cmap, atoi(buf));
	else
		fz_warn("expected integer after WMode in cmap");

	return fz_okay;
}

static fz_error
pdf_parsecodespacerange(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int lo, hi;

	while (1)
	{
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TENDCODESPACERANGE)
			return fz_okay;

		else if (tok == PDF_TSTRING)
		{
			lo = pdf_codefromstring(buf, len);
			error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
			if (error)
				return fz_rethrow(error, "syntaxerror in cmap");
			if (tok == PDF_TSTRING)
			{
				hi = pdf_codefromstring(buf, len);
				pdf_addcodespace(cmap, lo, hi, len);
			}
			else break;
		}

		else break;
	}

	return fz_throw("expected string or endcodespacerange");
}

static fz_error
pdf_parsecidrange(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int lo, hi, dst;

	while (1)
	{
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TENDCIDRANGE)
			return fz_okay;

		else if (tok != PDF_TSTRING)
			return fz_throw("expected string or endcidrange");

		lo = pdf_codefromstring(buf, len);

		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TSTRING)
			return fz_throw("expected string");

		hi = pdf_codefromstring(buf, len);

		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TINT)
			return fz_throw("expected integer");

		dst = atoi(buf);

		pdf_maprangetorange(cmap, lo, hi, dst);
	}
}

static fz_error
pdf_parsecidchar(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int src, dst;

	while (1)
	{
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TENDCIDCHAR)
			return fz_okay;

		else if (tok != PDF_TSTRING)
			return fz_throw("expected string or endcidchar");

		src = pdf_codefromstring(buf, len);

		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TINT)
			return fz_throw("expected integer");

		dst = atoi(buf);

		pdf_maprangetorange(cmap, src, src, dst);
	}
}

static fz_error
pdf_parsebfrangearray(pdf_cmap *cmap, fz_stream *file, int lo, int hi)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int dst[256];
	int i;

	while (1)
	{
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == PDF_TCARRAY)
			return fz_okay;

		/* Note: does not handle [ /Name /Name ... ] */
		else if (tok != PDF_TSTRING)
			return fz_throw("expected string or ]");

		if (len / 2)
		{
			for (i = 0; i < len / 2; i++)
				dst[i] = pdf_codefromstring(buf + i * 2, 2);

			pdf_maponetomany(cmap, lo, dst, len / 2);
		}

		lo ++;
	}
}

static fz_error
pdf_parsebfrange(pdf_cmap *cmap, fz_stream *file)
{
	fz_error error;
	char buf[256];
	int tok;
	int len;
	int lo, hi, dst;

	while (1)
	{
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TENDBFRANGE)
			return fz_okay;

		else if (tok != PDF_TSTRING)
			return fz_throw("expected string or endbfrange");

		lo = pdf_codefromstring(buf, len);

		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		if (tok != PDF_TSTRING)
			return fz_throw("expected string");

		hi = pdf_codefromstring(buf, len);

		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == PDF_TSTRING)
		{
			if (len == 2)
			{
				dst = pdf_codefromstring(buf, len);
				pdf_maprangetorange(cmap, lo, hi, dst);
			}
			else
			{
				int dststr[256];
				int i;

				if (len / 2)
				{
					for (i = 0; i < len / 2; i++)
						dststr[i] = pdf_codefromstring(buf + i * 2, 2);

					while (lo <= hi)
					{
						dststr[i-1] ++;
						pdf_maponetomany(cmap, lo, dststr, i);
						lo ++;
					}
				}
			}
		}

		else if (tok == PDF_TOARRAY)
		{
			error = pdf_parsebfrangearray(cmap, file, lo, hi);
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
pdf_parsebfchar(pdf_cmap *cmap, fz_stream *file)
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
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");

		if (tok == TENDBFCHAR)
			return fz_okay;

		else if (tok != PDF_TSTRING)
			return fz_throw("expected string or endbfchar");

		src = pdf_codefromstring(buf, len);

		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
			return fz_rethrow(error, "syntaxerror in cmap");
		/* Note: does not handle /dstName */
		if (tok != PDF_TSTRING)
			return fz_throw("expected string");

		if (len / 2)
		{
			for (i = 0; i < len / 2; i++)
				dst[i] = pdf_codefromstring(buf + i * 2, 2);
			pdf_maponetomany(cmap, src, dst, i);
		}
	}
}

fz_error
pdf_parsecmap(pdf_cmap **cmapp, fz_stream *file)
{
	fz_error error;
	pdf_cmap *cmap;
	char key[64];
	char buf[256];
	int tok;
	int len;

	cmap = pdf_newcmap();

	strcpy(key, ".notdef");

	while (1)
	{
		error = pdf_lexcmap(&tok, file, buf, sizeof buf, &len);
		if (error)
		{
			error = fz_rethrow(error, "syntaxerror in cmap");
			goto cleanup;
		}

		if (tok == PDF_TEOF || tok == TENDCMAP)
			break;

		else if (tok == PDF_TNAME)
		{
			if (!strcmp(buf, "CMapName"))
			{
				error = pdf_parsecmapname(cmap, file);
				if (error)
				{
					error = fz_rethrow(error, "syntaxerror in cmap after CMapName");
					goto cleanup;
				}
			}
			else if (!strcmp(buf, "WMode"))
			{
				error = pdf_parsewmode(cmap, file);
				if (error)
				{
					error = fz_rethrow(error, "syntaxerror in cmap after WMode");
					goto cleanup;
				}
			}
			else
				fz_strlcpy(key, buf, sizeof key);
		}

		else if (tok == TUSECMAP)
		{
			fz_strlcpy(cmap->usecmapname, key, sizeof(cmap->usecmapname));
		}

		else if (tok == TBEGINCODESPACERANGE)
		{
			error = pdf_parsecodespacerange(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap codespacerange");
				goto cleanup;
			}
		}

		else if (tok == TBEGINBFCHAR)
		{
			error = pdf_parsebfchar(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap bfchar");
				goto cleanup;
			}
		}

		else if (tok == TBEGINCIDCHAR)
		{
			error = pdf_parsecidchar(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap cidchar");
				goto cleanup;
			}
		}

		else if (tok == TBEGINBFRANGE)
		{
			error = pdf_parsebfrange(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap bfrange");
				goto cleanup;
			}
		}

		else if (tok == TBEGINCIDRANGE)
		{
			error = pdf_parsecidrange(cmap, file);
			if (error)
			{
				error = fz_rethrow(error, "syntaxerror in cmap cidrange");
				goto cleanup;
			}
		}

		/* ignore everything else */
	}

	pdf_sortcmap(cmap);

	*cmapp = cmap;
	return fz_okay;

cleanup:
	pdf_dropcmap(cmap);
	return error; /* already rethrown */
}
