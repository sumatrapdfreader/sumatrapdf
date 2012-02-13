#include "fitz.h"
#include "mupdf.h"

/*
 * Check if an object is a stream or not.
 */
int
pdf_is_stream(pdf_document *xref, int num, int gen)
{
	if (num < 0 || num >= xref->len)
		return 0;

	pdf_cache_object(xref, num, gen);
	/* RJW: "cannot load object, ignoring error" */

	return xref->table[num].stm_ofs > 0;
}

/*
 * Scan stream dictionary for an explicit /Crypt filter
 */
static int
pdf_stream_has_crypt(fz_context *ctx, fz_obj *stm)
{
	fz_obj *filters;
	fz_obj *obj;
	int i;

	filters = fz_dict_getsa(stm, "Filter", "F");
	if (filters)
	{
		if (!strcmp(fz_to_name(filters), "Crypt"))
			return 1;
		if (fz_is_array(filters))
		{
			int n = fz_array_len(filters);
			for (i = 0; i < n; i++)
			{
				obj = fz_array_get(filters, i);
				if (!strcmp(fz_to_name(obj), "Crypt"))
					return 1;
			}
		}
	}
	return 0;
}

/*
 * Create a filter given a name and param dictionary.
 */
static fz_stream *
build_filter(fz_stream *chain, pdf_document * xref, fz_obj * f, fz_obj * p, int num, int gen)
{
	fz_context *ctx = chain->ctx;
	char *s = fz_to_name(f);

	int predictor = fz_to_int(fz_dict_gets(p, "Predictor"));
	int columns = fz_to_int(fz_dict_gets(p, "Columns"));
	int colors = fz_to_int(fz_dict_gets(p, "Colors"));
	int bpc = fz_to_int(fz_dict_gets(p, "BitsPerComponent"));

	if (predictor == 0) predictor = 1;
	if (columns == 0) columns = 1;
	if (colors == 0) colors = 1;
	if (bpc == 0) bpc = 8;

	if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
		return fz_open_ahxd(chain);

	else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
		return fz_open_a85d(chain);

	else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
	{
		fz_obj *k = fz_dict_gets(p, "K");
		fz_obj *eol = fz_dict_gets(p, "EndOfLine");
		fz_obj *eba = fz_dict_gets(p, "EncodedByteAlign");
		fz_obj *columns = fz_dict_gets(p, "Columns");
		fz_obj *rows = fz_dict_gets(p, "Rows");
		fz_obj *eob = fz_dict_gets(p, "EndOfBlock");
		fz_obj *bi1 = fz_dict_gets(p, "BlackIs1");
		return fz_open_faxd(chain,
				k ? fz_to_int(k) : 0,
				eol ? fz_to_bool(eol) : 0,
				eba ? fz_to_bool(eba) : 0,
				columns ? fz_to_int(columns) : 1728,
				rows ? fz_to_int(rows) : 0,
				eob ? fz_to_bool(eob) : 1,
				bi1 ? fz_to_bool(bi1) : 0);
	}

	else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
	{
		fz_obj *ct = fz_dict_gets(p, "ColorTransform");
		return fz_open_dctd(chain, ct ? fz_to_int(ct) : -1);
	}

	else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
		return fz_open_rld(chain);

	else if (!strcmp(s, "FlateDecode") || !strcmp(s, "Fl"))
	{
		chain = fz_open_flated(chain);
		if (predictor > 1)
			chain = fz_open_predict(chain, predictor, columns, colors, bpc);
		return chain;
	}

	else if (!strcmp(s, "LZWDecode") || !strcmp(s, "LZW"))
	{
		fz_obj *ec = fz_dict_gets(p, "EarlyChange");
		chain = fz_open_lzwd(chain, ec ? fz_to_int(ec) : 1);
		if (predictor > 1)
			chain = fz_open_predict(chain, predictor, columns, colors, bpc);
		return chain;
	}

	else if (!strcmp(s, "JBIG2Decode"))
	{
		fz_buffer *globals = NULL;
		fz_obj *obj = fz_dict_gets(p, "JBIG2Globals");
		if (obj)
			globals = pdf_load_stream(xref, fz_to_num(obj), fz_to_gen(obj));
		/* fz_open_jbig2d takes possession of globals */
		return fz_open_jbig2d(chain, globals);
	}

	else if (!strcmp(s, "JPXDecode"))
		return chain; /* JPX decoding is special cased in the image loading code */

	else if (!strcmp(s, "Crypt"))
	{
		fz_obj *name;

		if (!xref->crypt)
		{
			fz_warn(ctx, "crypt filter in unencrypted document");
			return chain;
		}

		name = fz_dict_gets(p, "Name");
		if (fz_is_name(name))
			return pdf_open_crypt_with_filter(chain, xref->crypt, fz_to_name(name), num, gen);

		return chain;
	}

	fz_warn(ctx, "unknown filter name (%s)", s);
	return chain;
}

/*
 * Build a chain of filters given filter names and param dicts.
 * If head is given, start filter chain with it.
 * Assume ownership of head.
 */
static fz_stream *
build_filter_chain(fz_stream *chain, pdf_document *xref, fz_obj *fs, fz_obj *ps, int num, int gen)
{
	fz_obj *f;
	fz_obj *p;
	int i, n;

	n = fz_array_len(fs);
	for (i = 0; i < n; i++)
	{
		f = fz_array_get(fs, i);
		p = fz_array_get(ps, i);
		chain = build_filter(chain, xref, f, p, num, gen);
	}

	return chain;
}

/*
 * Build a filter for reading raw stream data.
 * This is a null filter to constrain reading to the
 * stream length, followed by a decryption filter.
 */
static fz_stream *
pdf_open_raw_filter(fz_stream *chain, pdf_document *xref, fz_obj *stmobj, int num, int gen)
{
	int hascrypt;
	int len;
	fz_context *ctx = chain->ctx;

	/* don't close chain when we close this filter */
	fz_keep_stream(chain);

	len = fz_to_int(fz_dict_gets(stmobj, "Length"));
	chain = fz_open_null(chain, len);

	fz_try(ctx)
	{
		hascrypt = pdf_stream_has_crypt(ctx, stmobj);
		if (xref->crypt && !hascrypt)
			chain = pdf_open_crypt(chain, xref->crypt, num, gen);
	}
	fz_catch(ctx)
	{
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return chain;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_stream *
pdf_open_filter(fz_stream *chain, pdf_document *xref, fz_obj *stmobj, int num, int gen)
{
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dict_getsa(stmobj, "Filter", "F");
	params = fz_dict_getsa(stmobj, "DecodeParms", "DP");

	chain = pdf_open_raw_filter(chain, xref, stmobj, num, gen);

	if (fz_is_name(filters))
		chain = build_filter(chain, xref, filters, params, num, gen);
	else if (fz_array_len(filters) > 0)
		chain = build_filter_chain(chain, xref, filters, params, num, gen);

	fz_lock_stream(chain);
	return chain;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_stream *
pdf_open_inline_stream(pdf_document *xref, fz_obj *stmobj, int length, fz_stream *chain)
{
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dict_getsa(stmobj, "Filter", "F");
	params = fz_dict_getsa(stmobj, "DecodeParms", "DP");

	/* don't close chain when we close this filter */
	fz_keep_stream(chain);

	if (fz_is_name(filters))
		return build_filter(chain, xref, filters, params, 0, 0);
	if (fz_array_len(filters) > 0)
		return build_filter_chain(chain, xref, filters, params, 0, 0);

	return fz_open_null(chain, length);
}

/*
 * Open a stream for reading the raw (compressed but decrypted) data.
 * Using xref->file while this is open is a bad idea.
 */
fz_stream *
pdf_open_raw_stream(pdf_document *xref, int num, int gen)
{
	pdf_xref_entry *x;
	fz_stream *stm;

	fz_var(x);

	if (num < 0 || num >= xref->len)
		fz_throw(xref->ctx, "object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	pdf_cache_object(xref, num, gen);
	/* RJW: "cannot load stream object (%d %d R)", num, gen */

	if (x->stm_ofs == 0)
		fz_throw(xref->ctx, "object is not a stream");

	stm = pdf_open_raw_filter(xref->file, xref, x->obj, num, gen);
	fz_lock_stream(stm);
	fz_seek(xref->file, x->stm_ofs, 0);
	return stm;
}

/*
 * Open a stream for reading uncompressed data.
 * Put the opened file in xref->stream.
 * Using xref->file while a stream is open is a Bad idea.
 */
fz_stream *
pdf_open_stream(pdf_document *xref, int num, int gen)
{
	pdf_xref_entry *x;
	fz_stream *stm;

	if (num < 0 || num >= xref->len)
		fz_throw(xref->ctx, "object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	pdf_cache_object(xref, num, gen);
	/* RJW: "cannot load stream object (%d %d R)", num, gen */

	if (x->stm_ofs == 0)
		fz_throw(xref->ctx, "object is not a stream");

	stm = pdf_open_filter(xref->file, xref, x->obj, num, gen);
	fz_seek(xref->file, x->stm_ofs, 0);
	return stm;
}

fz_stream *
pdf_open_stream_with_offset(pdf_document *xref, int num, int gen, fz_obj *dict, int stm_ofs)
{
	fz_stream *stm;

	if (stm_ofs == 0)
		fz_throw(xref->ctx, "object is not a stream");

	stm = pdf_open_filter(xref->file, xref, dict, num, gen);
	fz_seek(xref->file, stm_ofs, 0);
	return stm;
}

/*
 * Load raw (compressed but decrypted) contents of a stream into buf.
 */
fz_buffer *
pdf_load_raw_stream(pdf_document *xref, int num, int gen)
{
	fz_stream *stm;
	fz_obj *dict;
	int len;
	fz_buffer *buf;

	dict = pdf_load_object(xref, num, gen);
	/* RJW: "cannot load stream dictionary (%d %d R)", num, gen */

	len = fz_to_int(fz_dict_gets(dict, "Length"));

	fz_drop_obj(dict);

	stm = pdf_open_raw_stream(xref, num, gen);
	/* RJW: "cannot open raw stream (%d %d R)", num, gen */

	buf = fz_read_all(stm, len);
	/* RJW: "cannot read raw stream (%d %d R)", num, gen */

	fz_close(stm);
	return buf;
}

static int
pdf_guess_filter_length(int len, char *filter)
{
	if (!strcmp(filter, "ASCIIHexDecode"))
		return len / 2;
	if (!strcmp(filter, "ASCII85Decode"))
		return len * 4 / 5;
	if (!strcmp(filter, "FlateDecode"))
		return len * 3;
	if (!strcmp(filter, "RunLengthDecode"))
		return len * 3;
	if (!strcmp(filter, "LZWDecode"))
		return len * 2;
	return len;
}

/*
 * Load uncompressed contents of a stream into buf.
 */
fz_buffer *
pdf_load_stream(pdf_document *xref, int num, int gen)
{
	fz_context *ctx = xref->ctx;
	fz_stream *stm = NULL;
	fz_obj *dict, *obj;
	int i, len, n;
	fz_buffer *buf;

	fz_var(buf);

	dict = pdf_load_object(xref, num, gen);
	/* RJW: "cannot load stream dictionary (%d %d R)", num, gen */

	len = fz_to_int(fz_dict_gets(dict, "Length"));
	obj = fz_dict_gets(dict, "Filter");
	len = pdf_guess_filter_length(len, fz_to_name(obj));
	n = fz_array_len(obj);
	for (i = 0; i < n; i++)
		len = pdf_guess_filter_length(len, fz_to_name(fz_array_get(obj, i)));

	fz_drop_obj(dict);

	stm = pdf_open_stream(xref, num, gen);
	/* RJW: "cannot open stream (%d %d R)", num, gen */

	fz_try(ctx)
	{
		buf = fz_read_all(stm, len);
	}
	fz_always(ctx)
	{
		fz_close(stm);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "cannot read raw stream (%d %d R)", num, gen);
	}

	return buf;
}
