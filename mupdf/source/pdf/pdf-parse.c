#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

fz_rect
pdf_to_rect(fz_context *ctx, pdf_obj *array)
{
	if (!pdf_is_array(ctx, array))
		return fz_empty_rect;
	else
	{
		float a = pdf_array_get_real(ctx, array, 0);
		float b = pdf_array_get_real(ctx, array, 1);
		float c = pdf_array_get_real(ctx, array, 2);
		float d = pdf_array_get_real(ctx, array, 3);
		fz_rect r;
		r.x0 = fz_min(a, c);
		r.y0 = fz_min(b, d);
		r.x1 = fz_max(a, c);
		r.y1 = fz_max(b, d);
		return r;
	}
}

fz_quad
pdf_to_quad(fz_context *ctx, pdf_obj *array, int offset)
{
	fz_quad q;
	q.ul.x = pdf_array_get_real(ctx, array, offset+0);
	q.ul.y = pdf_array_get_real(ctx, array, offset+1);
	q.ur.x = pdf_array_get_real(ctx, array, offset+2);
	q.ur.y = pdf_array_get_real(ctx, array, offset+3);
	q.ll.x = pdf_array_get_real(ctx, array, offset+4);
	q.ll.y = pdf_array_get_real(ctx, array, offset+5);
	q.lr.x = pdf_array_get_real(ctx, array, offset+6);
	q.lr.y = pdf_array_get_real(ctx, array, offset+7);
	return q;
}

fz_matrix
pdf_to_matrix(fz_context *ctx, pdf_obj *array)
{
	if (!pdf_is_array(ctx, array))
		return fz_identity;
	else
	{
		fz_matrix m;
		m.a = pdf_array_get_real(ctx, array, 0);
		m.b = pdf_array_get_real(ctx, array, 1);
		m.c = pdf_array_get_real(ctx, array, 2);
		m.d = pdf_array_get_real(ctx, array, 3);
		m.e = pdf_array_get_real(ctx, array, 4);
		m.f = pdf_array_get_real(ctx, array, 5);
		return m;
	}
}

static int
rune_from_utf16be(int *out, const unsigned char *s, const unsigned char *end)
{
	if (s + 2 <= end)
	{
		int a = s[0] << 8 | s[1];
		if (a >= 0xD800 && a <= 0xDFFF && s + 4 <= end)
		{
			int b = s[2] << 8 | s[3];
			*out = ((a - 0xD800) << 10) + (b - 0xDC00) + 0x10000;
			return 4;
		}
		*out = a;
		return 2;
	}
	*out = FZ_REPLACEMENT_CHARACTER;
	return 1;
}

static int
rune_from_utf16le(int *out, const unsigned char *s, const unsigned char *end)
{
	if (s + 2 <= end)
	{
		int a = s[1] << 8 | s[0];
		if (a >= 0xD800 && a <= 0xDFFF && s + 4 <= end)
		{
			int b = s[3] << 8 | s[2];
			*out = ((a - 0xD800) << 10) + (b - 0xDC00) + 0x10000;
			return 4;
		}
		*out = a;
		return 2;
	}
	*out = FZ_REPLACEMENT_CHARACTER;
	return 1;
}

static size_t
skip_language_code_utf16le(const unsigned char *s, size_t n, size_t i)
{
	/* skip language escape codes */
	if (i + 6 <= n && s[i+1] == 0 && s[i+0] == 27 && s[i+5] == 0 && s[i+4] == 27)
		return 6;
	else if (i + 8 <= n && s[i+1] == 0 && s[i+0] == 27 && s[i+7] == 0 && s[i+6] == 27)
		return 8;
	return 0;
}

static size_t
skip_language_code_utf16be(const unsigned char *s, size_t n, size_t i)
{
	/* skip language escape codes */
	if (i + 6 <= n && s[i+0] == 0 && s[i+1] == 27 && s[i+4] == 0 && s[i+5] == 27)
		return 6;
	else if (i + 8 <= n && s[i+0] == 0 && s[i+1] == 27 && s[i+6] == 0 && s[i+7] == 27)
		return 8;
	return 0;
}

static size_t
skip_language_code_utf8(const unsigned char *s, size_t n, size_t i)
{
	/* skip language escape codes */
	if (i + 3 <= n && s[i] == 27 && s[i+3])
		return 3;
	else if (i + 5 <= n && s[i] == 27 && s[i+5] == 27)
		return 5;
	return 0;
}

/* Convert Unicode/PdfDocEncoding string into utf-8 */
char *
pdf_new_utf8_from_pdf_string(fz_context *ctx, const char *ssrcptr, size_t srclen)
{
	const unsigned char *srcptr = (const unsigned char*)ssrcptr;
	char *dstptr, *dst;
	size_t dstlen = 0;
	int ucs;
	size_t i, n;

	/* UTF-16BE */
	if (srclen >= 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		i = 2;
		while (i + 2 <= srclen)
		{
			n = skip_language_code_utf16be(srcptr, srclen, i);
			if (n)
				i += n;
			else
			{
				i += rune_from_utf16be(&ucs, srcptr + i, srcptr + srclen);
				dstlen += fz_runelen(ucs);
			}
		}

		dstptr = dst = Memento_label(fz_malloc(ctx, dstlen + 1), "utf8_from_utf16be");

		i = 2;
		while (i + 2 <= srclen)
		{
			n = skip_language_code_utf16be(srcptr, srclen, i);
			if (n)
				i += n;
			else
			{
				i += rune_from_utf16be(&ucs, srcptr + i, srcptr + srclen);
				dstptr += fz_runetochar(dstptr, ucs);
			}
		}
	}

	/* UTF-16LE */
	else if (srclen >= 2 && srcptr[0] == 255 && srcptr[1] == 254)
	{
		i = 2;
		while (i + 2 <= srclen)
		{
			n = skip_language_code_utf16le(srcptr, srclen, i);
			if (n)
				i += n;
			else
			{
				i += rune_from_utf16le(&ucs, srcptr + i, srcptr + srclen);
				dstlen += fz_runelen(ucs);
			}
		}

		dstptr = dst = Memento_label(fz_malloc(ctx, dstlen + 1), "utf8_from_utf16le");

		i = 2;
		while (i + 2 <= srclen)
		{
			n = skip_language_code_utf16le(srcptr, srclen, i);
			if (n)
				i += n;
			else
			{
				i += rune_from_utf16le(&ucs, srcptr + i, srcptr + srclen);
				dstptr += fz_runetochar(dstptr, ucs);
			}
		}
	}

	/* UTF-8 */
	else if (srclen >= 3 && srcptr[0] == 239 && srcptr[1] == 187 && srcptr[2] == 191)
	{
		i = 3;
		while (i < srclen)
		{
			n = skip_language_code_utf8(srcptr, srclen, i);
			if (n)
				i += n;
			else
			{
				i += 1;
				dstlen += 1;
			}
		}

		dstptr = dst = Memento_label(fz_malloc(ctx, dstlen + 1), "utf8_from_utf8");

		i = 3;
		while (i < srclen)
		{
			n = skip_language_code_utf8(srcptr, srclen, i);
			if (n)
				i += n;
			else
				*dstptr++ = srcptr[i++];
		}
	}

	/* PDFDocEncoding */
	else
	{
		for (i = 0; i < srclen; i++)
			dstlen += fz_runelen(fz_unicode_from_pdf_doc_encoding[srcptr[i]]);

		dstptr = dst = Memento_label(fz_malloc(ctx, dstlen + 1), "utf8_from_pdfdocenc");

		for (i = 0; i < srclen; i++)
		{
			ucs = fz_unicode_from_pdf_doc_encoding[srcptr[i]];
			dstptr += fz_runetochar(dstptr, ucs);
		}
	}

	*dstptr = 0;
	return dst;
}

/* Convert text string object to UTF-8 */
char *
pdf_new_utf8_from_pdf_string_obj(fz_context *ctx, pdf_obj *src)
{
	const char *srcptr;
	size_t srclen;
	srcptr = pdf_to_string(ctx, src, &srclen);
	return pdf_new_utf8_from_pdf_string(ctx, srcptr, srclen);
}

/* Load text stream and convert to UTF-8 */
char *
pdf_new_utf8_from_pdf_stream_obj(fz_context *ctx, pdf_obj *src)
{
	fz_buffer *stmbuf;
	char *srcptr;
	size_t srclen;
	char *dst = NULL;

	stmbuf = pdf_load_stream(ctx, src);
	srclen = fz_buffer_storage(ctx, stmbuf, (unsigned char **)&srcptr);
	fz_try(ctx)
		dst = pdf_new_utf8_from_pdf_string(ctx, srcptr, srclen);
	fz_always(ctx)
		fz_drop_buffer(ctx, stmbuf);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return dst;
}

/* Load text stream or text string and convert to UTF-8 */
char *
pdf_load_stream_or_string_as_utf8(fz_context *ctx, pdf_obj *src)
{
	if (pdf_is_stream(ctx, src))
		return pdf_new_utf8_from_pdf_stream_obj(ctx, src);
	return pdf_new_utf8_from_pdf_string_obj(ctx, src);
}

static pdf_obj *
pdf_new_text_string_utf16be(fz_context *ctx, const char *s)
{
	const char *ss;
	int c, i, n, a, b;
	unsigned char *p;
	pdf_obj *obj;

	ss = s;
	n = 0;
	while (*ss)
	{
		ss += fz_chartorune(&c, ss);
		n += (c >= 0x10000) ? 2 : 1;
	}

	p = fz_malloc(ctx, n * 2 + 2);
	i = 0;
	p[i++] = 254;
	p[i++] = 255;
	while (*s)
	{
		s += fz_chartorune(&c, s);
		if (c >= 0x10000)
		{
			a = (((c - 0x10000) >> 10) & 0x3ff) + 0xD800;
			p[i++] = (a>>8) & 0xff;
			p[i++] = (a) & 0xff;
			b = (((c - 0x10000)) & 0x3ff) + 0xDC00;
			p[i++] = (b>>8) & 0xff;
			p[i++] = (b) & 0xff;
		}
		else
		{
			p[i++] = (c>>8) & 0xff;
			p[i++] = (c) & 0xff;
		}
	}

	fz_try(ctx)
		obj = pdf_new_string(ctx, (char*)p, i);
	fz_always(ctx)
		fz_free(ctx, p);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return obj;
}

/*
 * Create a PDF 'text string' by encoding input string as either ASCII or UTF-16BE.
 * In theory, we could also use PDFDocEncoding.
 */
pdf_obj *
pdf_new_text_string(fz_context *ctx, const char *s)
{
	int i = 0;
	while (s[i] != 0)
	{
		if (((unsigned char)s[i]) >= 128)
			return pdf_new_text_string_utf16be(ctx, s);
		++i;
	}
	return pdf_new_string(ctx, s, i);
}

pdf_obj *
pdf_parse_array(fz_context *ctx, pdf_document *doc, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_obj *ary = NULL;
	pdf_obj *obj = NULL;
	int64_t a = 0, b = 0, n = 0;
	pdf_token tok;
	pdf_obj *op = NULL;

	fz_var(obj);

	ary = pdf_new_array(ctx, doc, 4);

	fz_try(ctx)
	{
		while (1)
		{
			tok = pdf_lex(ctx, file, buf);

			if (tok != PDF_TOK_INT && tok != PDF_TOK_R)
			{
				if (n > 0)
					pdf_array_push_int(ctx, ary, a);
				if (n > 1)
					pdf_array_push_int(ctx, ary, b);
				n = 0;
			}

			if (tok == PDF_TOK_INT && n == 2)
			{
				pdf_array_push_int(ctx, ary, a);
				a = b;
				n --;
			}

			switch (tok)
			{
			case PDF_TOK_EOF:
				fz_throw(ctx, FZ_ERROR_SYNTAX, "array not closed before end of file");

			case PDF_TOK_CLOSE_ARRAY:
				op = ary;
				goto end;

			case PDF_TOK_INT:
				if (n == 0)
					a = buf->i;
				if (n == 1)
					b = buf->i;
				n ++;
				break;

			case PDF_TOK_R:
				if (n != 2)
					fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot parse indirect reference in array");
				pdf_array_push_drop(ctx, ary, pdf_new_indirect(ctx, doc, a, b));
				n = 0;
				break;

			case PDF_TOK_OPEN_ARRAY:
				obj = pdf_parse_array(ctx, doc, file, buf);
				pdf_array_push_drop(ctx, ary, obj);
				break;

			case PDF_TOK_OPEN_DICT:
				obj = pdf_parse_dict(ctx, doc, file, buf);
				pdf_array_push_drop(ctx, ary, obj);
				break;

			case PDF_TOK_NAME:
				pdf_array_push_name(ctx, ary, buf->scratch);
				break;
			case PDF_TOK_REAL:
				pdf_array_push_real(ctx, ary, buf->f);
				break;
			case PDF_TOK_STRING:
				pdf_array_push_string(ctx, ary, buf->scratch, buf->len);
				break;
			case PDF_TOK_TRUE:
				pdf_array_push_bool(ctx, ary, 1);
				break;
			case PDF_TOK_FALSE:
				pdf_array_push_bool(ctx, ary, 0);
				break;
			case PDF_TOK_NULL:
				pdf_array_push(ctx, ary, PDF_NULL);
				break;

			default:
				pdf_array_push(ctx, ary, PDF_NULL);
				break;
			}
		}
end:
		{}
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, ary);
		fz_rethrow(ctx);
	}
	return op;
}

pdf_obj *
pdf_parse_dict(fz_context *ctx, pdf_document *doc, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_obj *dict;
	pdf_obj *key = NULL;
	pdf_obj *val = NULL;
	pdf_token tok;
	int64_t a, b;

	dict = pdf_new_dict(ctx, doc, 8);

	fz_var(key);
	fz_var(val);

	fz_try(ctx)
	{
		while (1)
		{
			tok = pdf_lex(ctx, file, buf);
	skip:
			if (tok == PDF_TOK_CLOSE_DICT)
				break;

			/* for BI .. ID .. EI in content streams */
			if (tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "ID"))
				break;

			if (tok != PDF_TOK_NAME)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "invalid key in dict");

			key = pdf_new_name(ctx, buf->scratch);

			tok = pdf_lex(ctx, file, buf);

			switch (tok)
			{
			case PDF_TOK_OPEN_ARRAY:
				val = pdf_parse_array(ctx, doc, file, buf);
				break;

			case PDF_TOK_OPEN_DICT:
				val = pdf_parse_dict(ctx, doc, file, buf);
				break;

			case PDF_TOK_NAME: val = pdf_new_name(ctx, buf->scratch); break;
			case PDF_TOK_REAL: val = pdf_new_real(ctx, buf->f); break;
			case PDF_TOK_STRING: val = pdf_new_string(ctx, buf->scratch, buf->len); break;
			case PDF_TOK_TRUE: val = PDF_TRUE; break;
			case PDF_TOK_FALSE: val = PDF_FALSE; break;
			case PDF_TOK_NULL: val = PDF_NULL; break;

			case PDF_TOK_INT:
				/* 64-bit to allow for numbers > INT_MAX and overflow */
				a = buf->i;
				tok = pdf_lex(ctx, file, buf);
				if (tok == PDF_TOK_CLOSE_DICT || tok == PDF_TOK_NAME ||
					(tok == PDF_TOK_KEYWORD && !strcmp(buf->scratch, "ID")))
				{
					val = pdf_new_int(ctx, a);
					pdf_dict_put(ctx, dict, key, val);
					pdf_drop_obj(ctx, val);
					val = NULL;
					pdf_drop_obj(ctx, key);
					key = NULL;
					goto skip;
				}
				if (tok == PDF_TOK_INT)
				{
					b = buf->i;
					tok = pdf_lex(ctx, file, buf);
					if (tok == PDF_TOK_R)
					{
						val = pdf_new_indirect(ctx, doc, a, b);
						break;
					}
				}
				fz_warn(ctx, "invalid indirect reference in dict");
				val = PDF_NULL;
				break;

			default:
				val = PDF_NULL;
				break;
			}

			pdf_dict_put(ctx, dict, key, val);
			pdf_drop_obj(ctx, val);
			val = NULL;
			pdf_drop_obj(ctx, key);
			key = NULL;
		}
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, dict);
		pdf_drop_obj(ctx, key);
		pdf_drop_obj(ctx, val);
		fz_rethrow(ctx);
	}
	return dict;
}

pdf_obj *
pdf_parse_stm_obj(fz_context *ctx, pdf_document *doc, fz_stream *file, pdf_lexbuf *buf)
{
	pdf_token tok;

	tok = pdf_lex(ctx, file, buf);

	switch (tok)
	{
	case PDF_TOK_OPEN_ARRAY:
		return pdf_parse_array(ctx, doc, file, buf);
	case PDF_TOK_OPEN_DICT:
		return pdf_parse_dict(ctx, doc, file, buf);
	case PDF_TOK_NAME: return pdf_new_name(ctx, buf->scratch);
	case PDF_TOK_REAL: return pdf_new_real(ctx, buf->f);
	case PDF_TOK_STRING: return pdf_new_string(ctx, buf->scratch, buf->len);
	case PDF_TOK_TRUE: return PDF_TRUE;
	case PDF_TOK_FALSE: return PDF_FALSE;
	case PDF_TOK_NULL: return PDF_NULL;
	case PDF_TOK_INT: return pdf_new_int(ctx, buf->i);
	default: fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown token in object stream");
	}
}

pdf_obj *
pdf_parse_ind_obj(fz_context *ctx, pdf_document *doc,
	fz_stream *file, pdf_lexbuf *buf,
	int *onum, int *ogen, int64_t *ostmofs, int *try_repair)
{
	pdf_obj *obj = NULL;
	int num = 0, gen = 0;
	int64_t stm_ofs;
	pdf_token tok;
	int64_t a, b;
	int read_next_token = 1;

	fz_var(obj);

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_INT)
	{
		if (try_repair)
			*try_repair = 1;
		fz_throw(ctx, FZ_ERROR_SYNTAX, "expected object number");
	}
	num = buf->i;
	if (num < 0 || num > PDF_MAX_OBJECT_NUMBER)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "object number out of range");

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_INT)
	{
		if (try_repair)
			*try_repair = 1;
		fz_throw(ctx, FZ_ERROR_SYNTAX, "expected generation number (%d ? obj)", num);
	}
	gen = buf->i;

	tok = pdf_lex(ctx, file, buf);
	if (tok != PDF_TOK_OBJ)
	{
		if (try_repair)
			*try_repair = 1;
		fz_throw(ctx, FZ_ERROR_SYNTAX, "expected 'obj' keyword (%d %d ?)", num, gen);
	}

	tok = pdf_lex(ctx, file, buf);

	switch (tok)
	{
	case PDF_TOK_OPEN_ARRAY:
		obj = pdf_parse_array(ctx, doc, file, buf);
		break;

	case PDF_TOK_OPEN_DICT:
		obj = pdf_parse_dict(ctx, doc, file, buf);
		break;

	case PDF_TOK_NAME: obj = pdf_new_name(ctx, buf->scratch); break;
	case PDF_TOK_REAL: obj = pdf_new_real(ctx, buf->f); break;
	case PDF_TOK_STRING: obj = pdf_new_string(ctx, buf->scratch, buf->len); break;
	case PDF_TOK_TRUE: obj = PDF_TRUE; break;
	case PDF_TOK_FALSE: obj = PDF_FALSE; break;
	case PDF_TOK_NULL: obj = PDF_NULL; break;

	case PDF_TOK_INT:
		a = buf->i;
		tok = pdf_lex(ctx, file, buf);

		if (tok == PDF_TOK_STREAM || tok == PDF_TOK_ENDOBJ)
		{
			obj = pdf_new_int(ctx, a);
			read_next_token = 0;
			break;
		}
		else if (tok == PDF_TOK_INT)
		{
			b = buf->i;
			tok = pdf_lex(ctx, file, buf);
			if (tok == PDF_TOK_R)
			{
				obj = pdf_new_indirect(ctx, doc, a, b);
				break;
			}
		}
		fz_throw(ctx, FZ_ERROR_SYNTAX, "expected 'R' keyword (%d %d R)", num, gen);

	case PDF_TOK_ENDOBJ:
		obj = PDF_NULL;
		read_next_token = 0;
		break;

	default:
		fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error in object (%d %d R)", num, gen);
	}

	fz_try(ctx)
	{
		if (read_next_token)
			tok = pdf_lex(ctx, file, buf);

		if (tok == PDF_TOK_STREAM)
		{
			int c = fz_read_byte(ctx, file);
			while (c == ' ')
				c = fz_read_byte(ctx, file);
			if (c == '\r')
			{
				c = fz_peek_byte(ctx, file);
				if (c != '\n')
					fz_warn(ctx, "line feed missing after stream begin marker (%d %d R)", num, gen);
				else
					fz_read_byte(ctx, file);
			}
			stm_ofs = fz_tell(ctx, file);
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
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, obj);
		fz_rethrow(ctx);
	}

	if (onum) *onum = num;
	if (ogen) *ogen = gen;
	if (ostmofs) *ostmofs = stm_ofs;

	return obj;
}
