#include "fitz.h"
#include "mupdf.h"

fz_rect pdf_torect(fz_obj *array)
{
	fz_rect r;
	float a = fz_toreal(fz_arrayget(array, 0));
	float b = fz_toreal(fz_arrayget(array, 1));
	float c = fz_toreal(fz_arrayget(array, 2));
	float d = fz_toreal(fz_arrayget(array, 3));
	r.x0 = MIN(a, c);
	r.y0 = MIN(b, d);
	r.x1 = MAX(a, c);
	r.y1 = MAX(b, d);
	return r;
}

fz_matrix pdf_tomatrix(fz_obj *array)
{
	fz_matrix m;
	m.a = fz_toreal(fz_arrayget(array, 0));
	m.b = fz_toreal(fz_arrayget(array, 1));
	m.c = fz_toreal(fz_arrayget(array, 2));
	m.d = fz_toreal(fz_arrayget(array, 3));
	m.e = fz_toreal(fz_arrayget(array, 4));
	m.f = fz_toreal(fz_arrayget(array, 5));
	return m;
}

char *
pdf_toutf8(fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_tostrbuf(src);
	char *dstptr, *dst;
	int srclen = fz_tostrlen(src);
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
			dstlen += runelen(pdf_docencoding[srcptr[i]]);

		dstptr = dst = fz_malloc(dstlen + 1);

		for (i = 0; i < srclen; i++)
		{
			ucs = pdf_docencoding[srcptr[i]];
			dstptr += runetochar(dstptr, &ucs);
		}
	}

	*dstptr = '\0';
	return dst;
}

unsigned short *
pdf_toucs2(fz_obj *src)
{
	unsigned char *srcptr = (unsigned char *) fz_tostrbuf(src);
	unsigned short *dstptr, *dst;
	int srclen = fz_tostrlen(src);
	int i;

	if (srclen > 2 && srcptr[0] == 254 && srcptr[1] == 255)
	{
		dstptr = dst = fz_malloc(((srclen - 2) / 2 + 1) * sizeof(short));
		for (i = 2; i < srclen; i += 2)
			*dstptr++ = (srcptr[i] << 8) | srcptr[i+1];
	}

	else
	{
		dstptr = dst = fz_malloc((srclen + 1) * sizeof(short));
		for (i = 0; i < srclen; i++)
			*dstptr++ = pdf_docencoding[srcptr[i]];
	}

	*dstptr = '\0';
	return dst;
}

fz_error
pdf_parsearray(fz_obj **op, pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_error error = fz_okay;
	fz_obj *ary = nil;
	fz_obj *obj = nil;
	int a = 0, b = 0, n = 0;
	pdf_token_e tok;
	int len;

	ary = fz_newarray(4);

	while (1)
	{
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
		{
			fz_dropobj(ary);
			return fz_rethrow(error, "cannot parse array");
		}

		if (tok != PDF_TINT && tok != PDF_TR)
		{
			if (n > 0)
			{
				obj = fz_newint(a);
				fz_arraypush(ary, obj);
				fz_dropobj(obj);
			}
			if (n > 1)
			{
				obj = fz_newint(b);
				fz_arraypush(ary, obj);
				fz_dropobj(obj);
			}
			n = 0;
		}

		if (tok == PDF_TINT && n == 2)
		{
			obj = fz_newint(a);
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			a = b;
			n --;
		}

		switch (tok)
		{
		case PDF_TCARRAY:
			*op = ary;
			return fz_okay;

		case PDF_TINT:
			if (n == 0)
				a = atoi(buf);
			if (n == 1)
				b = atoi(buf);
			n ++;
			break;

		case PDF_TR:
			if (n != 2)
			{
				fz_dropobj(ary);
				return fz_throw("cannot parse indirect reference in array");
			}
			obj = fz_newindirect(a, b, xref);
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			n = 0;
			break;

		case PDF_TOARRAY:
			error = pdf_parsearray(&obj, xref, file, buf, cap);
			if (error)
			{
				fz_dropobj(ary);
				return fz_rethrow(error, "cannot parse array");
			}
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;

		case PDF_TODICT:
			error = pdf_parsedict(&obj, xref, file, buf, cap);
			if (error)
			{
				fz_dropobj(ary);
				return fz_rethrow(error, "cannot parse array");
			}
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;

		case PDF_TNAME:
			obj = fz_newname(buf);
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;
		case PDF_TREAL:
			obj = fz_newreal(atof(buf));
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;
		case PDF_TSTRING:
			obj = fz_newstring(buf, len);
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;
		case PDF_TTRUE:
			obj = fz_newbool(1);
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;
		case PDF_TFALSE:
			obj = fz_newbool(0);
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;
		case PDF_TNULL:
			obj = fz_newnull();
			fz_arraypush(ary, obj);
			fz_dropobj(obj);
			break;

		default:
			fz_dropobj(ary);
			return fz_rethrow(error, "cannot parse token in array");
		}
	}
}

fz_error
pdf_parsedict(fz_obj **op, pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_error error = fz_okay;
	fz_obj *dict = nil;
	fz_obj *key = nil;
	fz_obj *val = nil;
	pdf_token_e tok;
	int len;
	int a, b;

	dict = fz_newdict(8);

	while (1)
	{
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
		{
			fz_dropobj(dict);
			return fz_rethrow(error, "cannot parse dict");
		}

skip:
		if (tok == PDF_TCDICT)
		{
			*op = dict;
			return fz_okay;
		}

		/* for BI .. ID .. EI in content streams */
		if (tok == PDF_TKEYWORD && !strcmp(buf, "ID"))
		{
			*op = dict;
			return fz_okay;
		}

		if (tok != PDF_TNAME)
		{
			fz_dropobj(dict);
			return fz_throw("invalid key in dict");;
		}

		key = fz_newname(buf);

		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
		{
			fz_dropobj(dict);
			return fz_rethrow(error, "cannot parse dict");
		}

		switch (tok)
		{
		case PDF_TOARRAY:
			error = pdf_parsearray(&val, xref, file, buf, cap);
			if (error)
			{
				fz_dropobj(key);
				fz_dropobj(dict);
				return fz_rethrow(error, "cannot parse dict");
			}
			break;

		case PDF_TODICT:
			error = pdf_parsedict(&val, xref, file, buf, cap);
			if (error)
			{
				fz_dropobj(key);
				fz_dropobj(dict);
				return fz_rethrow(error, "cannot parse dict");
			}
			break;

		case PDF_TNAME: val = fz_newname(buf); break;
		case PDF_TREAL: val = fz_newreal(atof(buf)); break;
		case PDF_TSTRING: val = fz_newstring(buf, len); break;
		case PDF_TTRUE: val = fz_newbool(1); break;
		case PDF_TFALSE: val = fz_newbool(0); break;
		case PDF_TNULL: val = fz_newnull(); break;

		case PDF_TINT:
			a = atoi(buf);
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
			{
				fz_dropobj(key);
				fz_dropobj(dict);
				return fz_rethrow(error, "cannot parse dict");
			}
			if (tok == PDF_TCDICT || tok == PDF_TNAME ||
				(tok == PDF_TKEYWORD && !strcmp(buf, "ID")))
			{
				val = fz_newint(a);
				fz_dictput(dict, key, val);
				fz_dropobj(val);
				fz_dropobj(key);
				goto skip;
			}
			if (tok == PDF_TINT)
			{
				b = atoi(buf);
				error = pdf_lex(&tok, file, buf, cap, &len);
				if (error)
				{
					fz_dropobj(key);
					fz_dropobj(dict);
					return fz_rethrow(error, "cannot parse dict");
				}
				if (tok == PDF_TR)
				{
					val = fz_newindirect(a, b, xref);
					break;
				}
			}
			fz_dropobj(key);
			fz_dropobj(dict);
			return fz_throw("invalid indirect reference in dict");

		default:
			return fz_throw("unknown token in dict");
		}

		fz_dictput(dict, key, val);
		fz_dropobj(val);
		fz_dropobj(key);
	}
}

fz_error
pdf_parsestmobj(fz_obj **op, pdf_xref *xref, fz_stream *file, char *buf, int cap)
{
	fz_error error;
	pdf_token_e tok;
	int len;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse token in object stream");

	switch (tok)
	{
	case PDF_TOARRAY:
		error = pdf_parsearray(op, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse object stream");
		break;
	case PDF_TODICT:
		error = pdf_parsedict(op, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse object stream");
		break;
	case PDF_TNAME: *op = fz_newname(buf); break;
	case PDF_TREAL: *op = fz_newreal(atof(buf)); break;
	case PDF_TSTRING: *op = fz_newstring(buf, len); break;
	case PDF_TTRUE: *op = fz_newbool(1); break;
	case PDF_TFALSE: *op = fz_newbool(0); break;
	case PDF_TNULL: *op = fz_newnull(); break;
	case PDF_TINT: *op = fz_newint(atoi(buf)); break;
	default: return fz_throw("unknown token in object stream");
	}

	return fz_okay;
}

fz_error
pdf_parseindobj(fz_obj **op, pdf_xref *xref,
	fz_stream *file, char *buf, int cap,
	int *onum, int *ogen, int *ostmofs)
{
	fz_error error = fz_okay;
	fz_obj *obj = nil;
	int num = 0, gen = 0, stmofs;
	pdf_token_e tok;
	int len;
	int a, b;

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	if (tok != PDF_TINT)
		return fz_throw("cannot parse indirect object (%d %d R)", num, gen);
	num = atoi(buf);

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	if (tok != PDF_TINT)
		return fz_throw("cannot parse indirect object (%d %d R)", num, gen);
	gen = atoi(buf);

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	if (tok != PDF_TOBJ)
		return fz_throw("cannot parse indirect object (%d %d R)", num, gen);

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);

	switch (tok)
	{
	case PDF_TOARRAY:
		error = pdf_parsearray(&obj, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		break;

	case PDF_TODICT:
		error = pdf_parsedict(&obj, xref, file, buf, cap);
		if (error)
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		break;

	case PDF_TNAME: obj = fz_newname(buf); break;
	case PDF_TREAL: obj = fz_newreal(atof(buf)); break;
	case PDF_TSTRING: obj = fz_newstring(buf, len); break;
	case PDF_TTRUE: obj = fz_newbool(1); break;
	case PDF_TFALSE: obj = fz_newbool(0); break;
	case PDF_TNULL: obj = fz_newnull(); break;

	case PDF_TINT:
		a = atoi(buf);
		error = pdf_lex(&tok, file, buf, cap, &len);
		if (error)
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		if (tok == PDF_TSTREAM || tok == PDF_TENDOBJ)
		{
			obj = fz_newint(a);
			goto skip;
		}
		if (tok == PDF_TINT)
		{
			b = atoi(buf);
			error = pdf_lex(&tok, file, buf, cap, &len);
			if (error)
				return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
			if (tok == PDF_TR)
			{
				obj = fz_newindirect(a, b, xref);
				break;
			}
		}
		return fz_throw("cannot parse indirect object (%d %d R)", num, gen);

	case PDF_TENDOBJ:
		obj = fz_newnull();
		goto skip;

	default:
		return fz_throw("cannot parse indirect object (%d %d R)", num, gen);
	}

	error = pdf_lex(&tok, file, buf, cap, &len);
	if (error)
	{
		fz_dropobj(obj);
		return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
	}

skip:
	if (tok == PDF_TSTREAM)
	{
		int c = fz_readbyte(file);
		while (c == ' ')
			c = fz_readbyte(file);
		if (c == '\r')
		{
			c = fz_peekbyte(file);
			if (c != '\n')
				fz_warn("line feed missing after stream begin marker (%d %d R)", num, gen);
			else
				c = fz_readbyte(file);
		}
		error = fz_readerror(file);
		if (error)
		{
			fz_dropobj(obj);
			return fz_rethrow(error, "cannot parse indirect object (%d %d R)", num, gen);
		}
		stmofs = fz_tell(file);
	}
	else if (tok == PDF_TENDOBJ)
	{
		stmofs = 0;
	}
	else
	{
		fz_warn("expected endobj or stream keyword (%d %d R)", num, gen);
		stmofs = 0;
	}

	if (onum) *onum = num;
	if (ogen) *ogen = gen;
	if (ostmofs) *ostmofs = stmofs;
	*op = obj;
	return fz_okay;
}

