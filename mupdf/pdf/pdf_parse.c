#include "fitz.h"
#include "mupdf.h"

fz_rect
pdf_to_rect(fz_context *ctx, fz_obj *array)
{
	fz_rect r;
	float a = fz_to_real(fz_array_get(array, 0));
	float b = fz_to_real(fz_array_get(array, 1));
	float c = fz_to_real(fz_array_get(array, 2));
	float d = fz_to_real(fz_array_get(array, 3));
	r.x0 = MIN(a, c);
	r.y0 = MIN(b, d);
	r.x1 = MAX(a, c);
	r.y1 = MAX(b, d);
	return r;
}

fz_matrix
pdf_to_matrix(fz_context *ctx, fz_obj *array)
{
	fz_matrix m;
	m.a = fz_to_real(fz_array_get(array, 0));
	m.b = fz_to_real(fz_array_get(array, 1));
	m.c = fz_to_real(fz_array_get(array, 2));
	m.d = fz_to_real(fz_array_get(array, 3));
	m.e = fz_to_real(fz_array_get(array, 4));
	m.f = fz_to_real(fz_array_get(array, 5));
	return m;
}

/* Convert Unicode/PdfDocEncoding string into utf-8 */
char *
pdf_to_utf8(fz_context *ctx, fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_to_str_buf(src);
	char *dstptr, *dst;
	int srclen = fz_to_str_len(src);
	int dstlen = 0;
	int ucs;
	int i;

	if (srclen >= 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		for (i = 2; i + 1 < srclen; i += 2)
		{
			ucs = srcptr[i] << 8 | srcptr[i+1];
			dstlen += runelen(ucs);
		}

		dstptr = dst = fz_malloc(ctx, dstlen + 1);

		for (i = 2; i + 1 < srclen; i += 2)
		{
			ucs = srcptr[i] << 8 | srcptr[i+1];
			dstptr += runetochar(dstptr, &ucs);
		}
	}
	else if (srclen >= 2 && srcptr[0] == 255 && srcptr[1] == 254)
	{
		for (i = 2; i + 1 < srclen; i += 2)
		{
			ucs = srcptr[i] | srcptr[i+1] << 8;
			dstlen += runelen(ucs);
		}

		dstptr = dst = fz_malloc(ctx, dstlen + 1);

		for (i = 2; i + 1 < srclen; i += 2)
		{
			ucs = srcptr[i] | srcptr[i+1] << 8;
			dstptr += runetochar(dstptr, &ucs);
		}
	}
	else
	{
		for (i = 0; i < srclen; i++)
			dstlen += runelen(pdf_doc_encoding[srcptr[i]]);

		dstptr = dst = fz_malloc(ctx, dstlen + 1);

		for (i = 0; i < srclen; i++)
		{
			ucs = pdf_doc_encoding[srcptr[i]];
			dstptr += runetochar(dstptr, &ucs);
		}
	}

	*dstptr = '\0';
	return dst;
}

/* Convert Unicode/PdfDocEncoding string into ucs-2 */
unsigned short *
pdf_to_ucs2(fz_context *ctx, fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_to_str_buf(src);
	unsigned short *dstptr, *dst;
	int srclen = fz_to_str_len(src);
	int i;

	if (srclen >= 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		dstptr = dst = fz_malloc_array(ctx, (srclen - 2) / 2 + 1, sizeof(short));
		for (i = 2; i + 1 < srclen; i += 2)
			*dstptr++ = srcptr[i] << 8 | srcptr[i+1];
	}
	else if (srclen >= 2 && srcptr[0] == 255 && srcptr[1] == 254)
	{
		dstptr = dst = fz_malloc_array(ctx, (srclen - 2) / 2 + 1, sizeof(short));
		for (i = 2; i + 1 < srclen; i += 2)
			*dstptr++ = srcptr[i] | srcptr[i+1] << 8;
	}
	else
	{
		dstptr = dst = fz_malloc_array(ctx, srclen + 1, sizeof(short));
		for (i = 0; i < srclen; i++)
			*dstptr++ = pdf_doc_encoding[srcptr[i]];
	}

	*dstptr = '\0';
	return dst;
}

/* SumatraPDF: allow to convert to UCS-2 without the need for an fz_context */
/* (buffer must be at least (fz_to_str_len(src) + 1) * 2 bytes in size) */
void
pdf_to_ucs2_buf(unsigned short *buffer, fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_to_str_buf(src);
	unsigned short *dstptr = buffer;
	int srclen = fz_to_str_len(src);
	int i;

	if (srclen >= 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		for (i = 2; i + 1 < srclen; i += 2)
			*dstptr++ = srcptr[i] << 8 | srcptr[i+1];
	}
	else if (srclen >= 2 && srcptr[0] == 255 && srcptr[1] == 254)
	{
		for (i = 2; i + 1 < srclen; i += 2)
			*dstptr++ = srcptr[i] | srcptr[i+1] << 8;
	}
	else
	{
		for (i = 0; i < srclen; i++)
			*dstptr++ = pdf_doc_encoding[srcptr[i]];
	}

	*dstptr = '\0';
}

/* Convert UCS-2 string into PdfDocEncoding for authentication */
char *
pdf_from_ucs2(fz_context *ctx, unsigned short *src)
{
	int i, j, len;
	char *docstr;

	len = 0;
	while (src[len])
		len++;

	docstr = fz_malloc(ctx, len + 1);

	for (i = 0; i < len; i++)
	{
		/* shortcut: check if the character has the same code point in both encodings */
		if (0 < src[i] && src[i] < 256 && pdf_doc_encoding[src[i]] == src[i]) {
			docstr[i] = src[i];
			continue;
		}

		/* search through pdf_docencoding for the character's code point */
		for (j = 0; j < 256; j++)
			if (pdf_doc_encoding[j] == src[i])
				break;
		docstr[i] = j;

		/* fail, if a character can't be encoded */
		if (!docstr[i])
		{
			fz_free(ctx, docstr);
			return NULL;
		}
	}
	docstr[len] = '\0';

	return docstr;
}

fz_obj *
pdf_to_utf8_name(fz_context *ctx, fz_obj *src)
{
	char *buf = pdf_to_utf8(ctx, src);
	fz_obj *dst = fz_new_name(ctx, buf);
	fz_free(ctx, buf);
	return dst;
}

fz_obj *
pdf_parse_array(pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_obj *ary = NULL;
	fz_obj *obj = NULL;
	int a = 0, b = 0, n = 0;
	int tok;
	int len;
	fz_context *ctx = file->ctx;
	fz_obj *op;

	fz_var(obj);

	ary = fz_new_array(ctx, 4);

	fz_try(ctx)
	{
		while (1)
		{
			tok = pdf_lex(file, buf, cap, &len);

			if (tok != PDF_TOK_INT && tok != PDF_TOK_R)
			{
				if (n > 0)
				{
					obj = fz_new_int(ctx, a);
					fz_array_push(ary, obj);
					fz_drop_obj(obj);
					obj = NULL;
				}
				if (n > 1)
				{
					obj = fz_new_int(ctx, b);
					fz_array_push(ary, obj);
					fz_drop_obj(obj);
					obj = NULL;
				}
				n = 0;
			}

			if (tok == PDF_TOK_INT && n == 2)
			{
				obj = fz_new_int(ctx, a);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				a = b;
				n --;
			}

			switch (tok)
			{
			case PDF_TOK_CLOSE_ARRAY:
				op = ary;
				goto end;

			case PDF_TOK_INT:
				if (n == 0)
					a = atoi(buf);
				if (n == 1)
					b = atoi(buf);
				n ++;
				break;

			case PDF_TOK_R:
				if (n != 2)
					fz_throw(ctx, "cannot parse indirect reference in array");
				obj = fz_new_indirect(ctx, a, b, xref);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				n = 0;
				break;

			case PDF_TOK_OPEN_ARRAY:
				obj = pdf_parse_array(xref, file, buf, cap);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;

			case PDF_TOK_OPEN_DICT:
				obj = pdf_parse_dict(xref, file, buf, cap);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;

			case PDF_TOK_NAME:
				obj = fz_new_name(ctx, buf);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;
			case PDF_TOK_REAL:
				obj = fz_new_real(ctx, fz_atof(buf));
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;
			case PDF_TOK_STRING:
				obj = fz_new_string(ctx, buf, len);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;
			case PDF_TOK_TRUE:
				obj = fz_new_bool(ctx, 1);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;
			case PDF_TOK_FALSE:
				obj = fz_new_bool(ctx, 0);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;
			case PDF_TOK_NULL:
				obj = fz_new_null(ctx);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
				obj = NULL;
				break;

			default:
				fz_throw(ctx, "cannot parse token in array");
			}
		}
end:
		{}
	}
	fz_catch(ctx)
	{
		fz_drop_obj(obj);
		fz_drop_obj(ary);
		fz_throw(ctx, "cannot parse array");
	}
	return op;
}

fz_obj *
pdf_parse_dict(pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_obj *dict = NULL;
	fz_obj *key = NULL;
	fz_obj *val = NULL;
	int tok;
	int len;
	int a, b;
	fz_context *ctx = file->ctx;

	fz_var(dict);
	fz_var(key);
	fz_var(val);

	dict = fz_new_dict(ctx, 8);

	fz_try(ctx)
	{
		while (1)
		{
			tok = pdf_lex(file, buf, cap, &len);
	skip:
			if (tok == PDF_TOK_CLOSE_DICT)
				break;

			/* for BI .. ID .. EI in content streams */
			if (tok == PDF_TOK_KEYWORD && !strcmp(buf, "ID"))
				break;

			if (tok != PDF_TOK_NAME)
				fz_throw(ctx, "invalid key in dict");

			key = fz_new_name(ctx, buf);

			tok = pdf_lex(file, buf, cap, &len);

			switch (tok)
			{
			case PDF_TOK_OPEN_ARRAY:
				/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1643 */
				fz_try(ctx)
				{
					val = pdf_parse_array(xref, file, buf, cap);
				}
				fz_catch(ctx)
				{
					fz_warn(ctx, "ignoring broken array for '%s'", fz_to_name(key));
					fz_drop_obj(key);
					val = key = NULL;
					do
						tok = pdf_lex(file, buf, cap, &len);
					while (tok != PDF_TOK_CLOSE_DICT && tok != PDF_TOK_CLOSE_ARRAY &&
						tok != PDF_TOK_EOF && tok != PDF_TOK_OPEN_ARRAY && tok != PDF_TOK_OPEN_DICT);
					if (tok == PDF_TOK_CLOSE_DICT)
						goto skip;
					if (tok == PDF_TOK_CLOSE_ARRAY)
						continue;
					fz_throw(ctx, "cannot make sense of broken array after all");
				}
				break;

			case PDF_TOK_OPEN_DICT:
				val = pdf_parse_dict(xref, file, buf, cap);
				break;

			case PDF_TOK_NAME: val = fz_new_name(ctx, buf); break;
			case PDF_TOK_REAL: val = fz_new_real(ctx, fz_atof(buf)); break;
			case PDF_TOK_STRING: val = fz_new_string(ctx, buf, len); break;
			case PDF_TOK_TRUE: val = fz_new_bool(ctx, 1); break;
			case PDF_TOK_FALSE: val = fz_new_bool(ctx, 0); break;
			case PDF_TOK_NULL: val = fz_new_null(ctx); break;

			case PDF_TOK_INT:
				/* 64-bit to allow for numbers > INT_MAX and overflow */
				a = (int) strtoll(buf, 0, 10);
				tok = pdf_lex(file, buf, cap, &len);
				if (tok == PDF_TOK_CLOSE_DICT || tok == PDF_TOK_NAME ||
					(tok == PDF_TOK_KEYWORD && !strcmp(buf, "ID")))
				{
					val = fz_new_int(ctx, a);
					fz_dict_put(dict, key, val);
					fz_drop_obj(val);
					val = NULL;
					fz_drop_obj(key);
					key = NULL;
					goto skip;
				}
				if (tok == PDF_TOK_INT)
				{
					b = atoi(buf);
					tok = pdf_lex(file, buf, cap, &len);
					if (tok == PDF_TOK_R)
					{
						val = fz_new_indirect(ctx, a, b, xref);
						break;
					}
				}
				fz_throw(ctx, "invalid indirect reference in dict");

			default:
				fz_throw(ctx, "unknown token in dict");
			}

			fz_dict_put(dict, key, val);
			fz_drop_obj(val);
			val = NULL;
			fz_drop_obj(key);
			key = NULL;
		}
	}
	fz_catch(ctx)
	{
		fz_drop_obj(dict);
		fz_drop_obj(key);
		fz_drop_obj(val);
		fz_throw(ctx, "cannot parse dict");
	}
	return dict;
}

fz_obj *
pdf_parse_stm_obj(pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	int tok;
	int len;
	fz_context *ctx = file->ctx;

	tok = pdf_lex(file, buf, cap, &len);
	/* RJW: "cannot parse token in object stream") */

	switch (tok)
	{
	case PDF_TOK_OPEN_ARRAY:
		return pdf_parse_array(xref, file, buf, cap);
		/* RJW: "cannot parse object stream" */
	case PDF_TOK_OPEN_DICT:
		return pdf_parse_dict(xref, file, buf, cap);
		/* RJW: "cannot parse object stream" */
	case PDF_TOK_NAME: return fz_new_name(ctx, buf); break;
	case PDF_TOK_REAL: return fz_new_real(ctx, fz_atof(buf)); break;
	case PDF_TOK_STRING: return fz_new_string(ctx, buf, len); break;
	case PDF_TOK_TRUE: return fz_new_bool(ctx, 1); break;
	case PDF_TOK_FALSE: return fz_new_bool(ctx, 0); break;
	case PDF_TOK_NULL: return fz_new_null(ctx); break;
	case PDF_TOK_INT: return fz_new_int(ctx, atoi(buf)); break;
	default: fz_throw(ctx, "unknown token in object stream");
	}
	return NULL; /* Stupid MSVC */
}

fz_obj *
pdf_parse_ind_obj(pdf_xref *xref,
	fz_stream *file, char *buf, int cap,
	int *onum, int *ogen, int *ostmofs)
{
	fz_obj *obj = NULL;
	int num = 0, gen = 0, stm_ofs;
	int tok;
	int len;
	int a, b;
	fz_context *ctx = file->ctx;

	fz_var(obj);

	tok = pdf_lex(file, buf, cap, &len);
	/* RJW: cannot parse indirect object (%d %d R)", num, gen */
	if (tok != PDF_TOK_INT)
		fz_throw(ctx, "expected object number (%d %d R)", num, gen);
	num = atoi(buf);

	tok = pdf_lex(file, buf, cap, &len);
	/* RJW: "cannot parse indirect object (%d %d R)", num, gen */
	if (tok != PDF_TOK_INT)
		fz_throw(ctx, "expected generation number (%d %d R)", num, gen);
	gen = atoi(buf);

	tok = pdf_lex(file, buf, cap, &len);
	/* RJW: "cannot parse indirect object (%d %d R)", num, gen */
	if (tok != PDF_TOK_OBJ)
		fz_throw(ctx, "expected 'obj' keyword (%d %d R)", num, gen);

	tok = pdf_lex(file, buf, cap, &len);
	/* RJW: "cannot parse indirect object (%d %d R)", num, gen */

	switch (tok)
	{
	case PDF_TOK_OPEN_ARRAY:
		obj = pdf_parse_array(xref, file, buf, cap);
		/* RJW: "cannot parse indirect object (%d %d R)", num, gen */
		break;

	case PDF_TOK_OPEN_DICT:
		obj = pdf_parse_dict(xref, file, buf, cap);
		/* RJW: "cannot parse indirect object (%d %d R)", num, gen */
		break;

	case PDF_TOK_NAME: obj = fz_new_name(ctx, buf); break;
	case PDF_TOK_REAL: obj = fz_new_real(ctx, fz_atof(buf)); break;
	case PDF_TOK_STRING: obj = fz_new_string(ctx, buf, len); break;
	case PDF_TOK_TRUE: obj = fz_new_bool(ctx, 1); break;
	case PDF_TOK_FALSE: obj = fz_new_bool(ctx, 0); break;
	case PDF_TOK_NULL: obj = fz_new_null(ctx); break;

	case PDF_TOK_INT:
		a = atoi(buf);
		tok = pdf_lex(file, buf, cap, &len);
		/* "cannot parse indirect object (%d %d R)", num, gen */
		if (tok == PDF_TOK_STREAM || tok == PDF_TOK_ENDOBJ)
		{
			obj = fz_new_int(ctx, a);
			goto skip;
		}
		if (tok == PDF_TOK_INT)
		{
			b = atoi(buf);
			tok = pdf_lex(file, buf, cap, &len);
			/* RJW: "cannot parse indirect object (%d %d R)", num, gen); */
			if (tok == PDF_TOK_R)
			{
				obj = fz_new_indirect(ctx, a, b, xref);
				break;
			}
		}
		fz_throw(ctx, "expected 'R' keyword (%d %d R)", num, gen);

	case PDF_TOK_ENDOBJ:
		obj = fz_new_null(ctx);
		goto skip;

	default:
		fz_throw(ctx, "syntax error in object (%d %d R)", num, gen);
	}

	fz_try(ctx)
	{
		tok = pdf_lex(file, buf, cap, &len);
	}
	fz_catch(ctx)
	{
		fz_drop_obj(obj);
		fz_throw(ctx, "cannot parse indirect object (%d %d R)", num, gen);
	}

skip:
	if (tok == PDF_TOK_STREAM)
	{
		int c = fz_read_byte(file);
		while (c == ' ')
			c = fz_read_byte(file);
		if (c == '\r')
		{
			c = fz_peek_byte(file);
			if (c != '\n')
				fz_warn(ctx, "line feed missing after stream begin marker (%d %d R)", num, gen);
			else
				fz_read_byte(file);
		}
		stm_ofs = fz_tell(file);
	}
	else if (tok == PDF_TOK_ENDOBJ)
	{
		stm_ofs = 0;
	}
	else
	{
		fz_warn(ctx, "expected 'endobj' or 'stream' keyword (%d %d R)", num, gen);
		stm_ofs = 0;
	}

	if (onum) *onum = num;
	if (ogen) *ogen = gen;
	if (ostmofs) *ostmofs = stm_ofs;
	return obj;
}
