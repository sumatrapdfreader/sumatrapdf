#include "fitz.h"
#include "mupdf.h"

fz_rect
pdf_to_rect(fz_obj *array)
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
pdf_to_matrix(fz_obj *array)
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
pdf_to_utf8(fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_to_str_buf(src);
	char *dstptr, *dst;
	int srclen = fz_to_str_len(src);
	int dstlen = 0;
	int ucs;
	int i;

	if (srclen > 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		for (i = 2; i < srclen; i += 2)
		{
			ucs = (srcptr[i] << 8) | srcptr[i+1];
			dstlen += runelen(ucs);
		}

		dstptr = dst = fz_malloc(dstlen + 1);

		for (i = 2; i < srclen; i += 2)
		{
			ucs = (srcptr[i] << 8) | srcptr[i+1];
			dstptr += runetochar(dstptr, &ucs);
		}
	}

	else
	{
		for (i = 0; i < srclen; i++)
			dstlen += runelen(pdf_doc_encoding[srcptr[i]]);

		dstptr = dst = fz_malloc(dstlen + 1);

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
pdf_to_ucs2(fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_to_str_buf(src);
	unsigned short *dstptr, *dst;
	int srclen = fz_to_str_len(src);
	int i;

	if (srclen > 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		dstptr = dst = fz_calloc((srclen - 2) / 2 + 1, sizeof(short));
		for (i = 2; i < srclen; i += 2)
			*dstptr++ = (srcptr[i] << 8) | srcptr[i+1];
	}

	else
	{
		dstptr = dst = fz_calloc(srclen + 1, sizeof(short));
		for (i = 0; i < srclen; i++)
			*dstptr++ = pdf_doc_encoding[srcptr[i]];
	}

	*dstptr = '\0';
	return dst;
}

/* Convert UCS-2 string into PdfDocEncoding for authentication */
char *
pdf_from_ucs2(unsigned short *src)
{
	int i, j, len;
	char *docstr;

	len = 0;
	while (src[len])
		len++;

	docstr = fz_malloc(len + 1);

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
			fz_free(docstr);
			return NULL;
		}
	}
	docstr[len] = '\0';

	return docstr;
}

fz_obj *
pdf_to_utf8_name(fz_obj *src)
{
	char *buf = pdf_to_utf8(src);
	fz_obj *dst = fz_new_name(buf);
	fz_free(buf);
	return dst;
}

fz_error
pdf_parse_array(fz_obj **op, pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_error error = fz_okay;
	fz_obj *ary = NULL;
	fz_obj *obj = NULL;
	int a = 0, b = 0, n = 0;
	int tok;
	int len;

	ary = fz_new_array(4);

	while (1)
	{
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
		{
			fz_drop_obj(ary);
			return fz_rethrow(error, "cannot parse array");
		}

		if (tok != PDF_TOK_INT && tok != PDF_TOK_R)
		{
			if (n > 0)
			{
				obj = fz_new_int(a);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
			}
			if (n > 1)
			{
				obj = fz_new_int(b);
				fz_array_push(ary, obj);
				fz_drop_obj(obj);
			}
			n = 0;
		}

		if (tok == PDF_TOK_INT && n == 2)
		{
			obj = fz_new_int(a);
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			a = b;
			n --;
		}

		switch (tok)
		{
		case PDF_TOK_CLOSE_ARRAY:
			*op = ary;
			return fz_okay;

		case PDF_TOK_INT:
			if (n == 0)
				a = atoi(buf);
			if (n == 1)
				b = atoi(buf);
			n ++;
			break;

		case PDF_TOK_R:
			if (n != 2)
			{
				fz_drop_obj(ary);
				return fz_throw("cannot parse indirect reference in array");
			}
			obj = fz_new_indirect(a, b, xref);
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			n = 0;
			break;

		case PDF_TOK_OPEN_ARRAY:
			error = pdf_parse_array(&obj, xref, file, buf, cap);
			if (error)
			{
				fz_drop_obj(ary);
				return fz_rethrow(error, "cannot parse array");
			}
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;

		case PDF_TOK_OPEN_DICT:
			error = pdf_parse_dict(&obj, xref, file, buf, cap);
			if (error)
			{
				fz_drop_obj(ary);
				return fz_rethrow(error, "cannot parse array");
			}
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;

		case PDF_TOK_NAME:
			obj = fz_new_name(buf);
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;
		case PDF_TOK_REAL:
			obj = fz_new_real(fz_atof(buf));
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;
		case PDF_TOK_STRING:
			obj = fz_new_string(buf, len);
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;
		case PDF_TOK_TRUE:
			obj = fz_new_bool(1);
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;
		case PDF_TOK_FALSE:
			obj = fz_new_bool(0);
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;
		case PDF_TOK_NULL:
			obj = fz_new_null();
			fz_array_push(ary, obj);
			fz_drop_obj(obj);
			break;

		default:
			fz_drop_obj(ary);
			return fz_throw("cannot parse token in array");
		}
	}
}

fz_error
pdf_parse_dict(fz_obj **op, pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_error error = fz_okay;
	fz_obj *dict = NULL;
	fz_obj *key = NULL;
	fz_obj *val = NULL;
	int tok;
	int len;
	int a, b;

	dict = fz_new_dict(8);

	while (1)
	{
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
		{
			fz_drop_obj(dict);
			return fz_rethrow(error, "cannot parse dict");
		}

skip:
		if (tok == PDF_TOK_CLOSE_DICT)
		{
			*op = dict;
			return fz_okay;
		}

		/* for BI .. ID .. EI in content streams */
		if (tok == PDF_TOK_KEYWORD && !strcmp(buf, "ID"))
		{
			*op = dict;
			return fz_okay;
		}

		if (tok != PDF_TOK_NAME)
		{
			fz_drop_obj(dict);
			return fz_throw("invalid key in dict");
		}

		key = fz_new_name(buf);

		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
		{
			fz_drop_obj(key);
			fz_drop_obj(dict);
			return fz_rethrow(error, "cannot parse dict");
		}

		switch (tok)
		{
		case PDF_TOK_OPEN_ARRAY:
			error = pdf_parse_array(&val, xref, file, buf, cap);
			if (error)
			{
				fz_drop_obj(key);
				fz_drop_obj(dict);
				return fz_rethrow(error, "cannot parse dict");
			}
			break;

		case PDF_TOK_OPEN_DICT:
			error = pdf_parse_dict(&val, xref, file, buf, cap);
			if (error)
			{
				fz_drop_obj(key);
				fz_drop_obj(dict);
				return fz_rethrow(error, "cannot parse dict");
			}
			break;

		case PDF_TOK_NAME: val = fz_new_name(buf); break;
		case PDF_TOK_REAL: val = fz_new_real(fz_atof(buf)); break;
		case PDF_TOK_STRING: val = fz_new_string(buf, len); break;
		case PDF_TOK_TRUE: val = fz_new_bool(1); break;
		case PDF_TOK_FALSE: val = fz_new_bool(0); break;
		case PDF_TOK_NULL: val = fz_new_null(); break;

		case PDF_TOK_INT:
			/* 64-bit to allow for numbers > INT_MAX and overflow */
			a = (int) strtoll(buf, 0, 10);
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
			{
				fz_drop_obj(key);
				fz_drop_obj(dict);
				return fz_rethrow(error, "cannot parse dict");
			}
			if (tok == PDF_TOK_CLOSE_DICT || tok == PDF_TOK_NAME ||
				(tok == PDF_TOK_KEYWORD && !strcmp(buf, "ID")))
			{
				val = fz_new_int(a);
				fz_dict_put(dict, key, val);
				fz_drop_obj(val);
				fz_drop_obj(key);
				goto skip;
			}
			if (tok == PDF_TOK_INT)
			{
				b = atoi(buf);
				error = pdf_lex(&tok, file, buf, cap, &len);
				if (error)
				{
					fz_drop_obj(key);
					fz_drop_obj(dict);
					return fz_rethrow(error, "cannot parse dict");
				}
				if (tok == PDF_TOK_R)
				{
					val = fz_new_indirect(a, b, xref);
					break;
				}
			}
			fz_drop_obj(key);
			fz_drop_obj(dict);
			return fz_throw("invalid indirect reference in dict");

		default:
			fz_drop_obj(key);
			fz_drop_obj(dict);
			return fz_throw("unknown token in dict");
		}

		fz_dict_put(dict, key, val);
		fz_drop_obj(val);
		fz_drop_obj(key);
	}
}

fz_error
pdf_parse_stm_obj(fz_obj **op, pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_error error;
	int tok;
	int len;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse token in object stream");

	switch (tok)
	{
	case PDF_TOK_OPEN_ARRAY:
		error = pdf_parse_array(op, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse object stream");
		break;
	case PDF_TOK_OPEN_DICT:
		error = pdf_parse_dict(op, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse object stream");
		break;
	case PDF_TOK_NAME: *op = fz_new_name(buf); break;
	case PDF_TOK_REAL: *op = fz_new_real(fz_atof(buf)); break;
	case PDF_TOK_STRING: *op = fz_new_string(buf, len); break;
	case PDF_TOK_TRUE: *op = fz_new_bool(1); break;
	case PDF_TOK_FALSE: *op = fz_new_bool(0); break;
	case PDF_TOK_NULL: *op = fz_new_null(); break;
	case PDF_TOK_INT: *op = fz_new_int(atoi(buf)); break;
	default: return fz_throw("unknown token in object stream");
	}

	return fz_okay;
}

fz_error
pdf_parse_ind_obj(fz_obj **op, pdf_xref *xref,
	fz_stream *file, char *buf, int cap,
	int *onum, int *ogen, int *ostmofs)
{
	fz_error error = fz_okay;
	fz_obj *obj = NULL;
	int num = 0, gen = 0, stm_ofs;
	int tok;
	int len;
	int a, b;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	if (tok != PDF_TOK_INT)
		return fz_throw("expected object number (%d %d R)", num, gen);
	num = atoi(buf);

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	if (tok != PDF_TOK_INT)
		return fz_throw("expected generation number (%d %d R)", num, gen);
	gen = atoi(buf);

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	if (tok != PDF_TOK_OBJ)
		return fz_throw("expected 'obj' keyword (%d %d R)", num, gen);

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);

	switch (tok)
	{
	case PDF_TOK_OPEN_ARRAY:
		error = pdf_parse_array(&obj, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		break;

	case PDF_TOK_OPEN_DICT:
		error = pdf_parse_dict(&obj, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		break;

	case PDF_TOK_NAME: obj = fz_new_name(buf); break;
	case PDF_TOK_REAL: obj = fz_new_real(fz_atof(buf)); break;
	case PDF_TOK_STRING: obj = fz_new_string(buf, len); break;
	case PDF_TOK_TRUE: obj = fz_new_bool(1); break;
	case PDF_TOK_FALSE: obj = fz_new_bool(0); break;
	case PDF_TOK_NULL: obj = fz_new_null(); break;

	case PDF_TOK_INT:
		a = atoi(buf);
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		if (tok == PDF_TOK_STREAM || tok == PDF_TOK_ENDOBJ)
		{
			obj = fz_new_int(a);
			goto skip;
		}
		if (tok == PDF_TOK_INT)
		{
			b = atoi(buf);
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
				return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
			if (tok == PDF_TOK_R)
			{
				obj = fz_new_indirect(a, b, xref);
				break;
			}
		}
		return fz_throw("expected 'R' keyword (%d %d R)", num, gen);

	case PDF_TOK_ENDOBJ:
		obj = fz_new_null();
		goto skip;

	default:
		return fz_throw("syntax error in object (%d %d R)", num, gen);
	}

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
	{
		fz_drop_obj(obj);
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
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
				fz_warn("line feed missing after stream begin marker (%d %d R)", num, gen);
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
		fz_warn("expected 'endobj' or 'stream' keyword (%d %d R)", num, gen);
		stm_ofs = 0;
	}

	if (onum) *onum = num;
	if (ogen) *ogen = gen;
	if (ostmofs) *ostmofs = stm_ofs;
	*op = obj;
	return fz_okay;
}
