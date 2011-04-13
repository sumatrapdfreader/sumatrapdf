#include "fitz.h"
#include "mupdf.h"

/*
 * Check if an object is a stream or not.
 */
int
pdf_is_stream(pdf_xref *xref, int num, int gen)
{
	fz_error error;

	if (num < 0 || num >= xref->len)
		return 0;

	error = pdf_cache_object(xref, num, gen);
	if (error)
	{
		fz_catch(error, "cannot load object, ignoring error");
		return 0;
	}

	return xref->table[num].stm_ofs > 0;
}

/*
 * Scan stream dictionary for an explicit /Crypt filter
 */
static int
pdf_stream_has_crypt(fz_obj *stm)
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
			for (i = 0; i < fz_array_len(filters); i++)
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
build_filter(fz_stream *chain, pdf_xref * xref, fz_obj * f, fz_obj * p, int num, int gen)
{
	fz_error error;
	char *s;

	s = fz_to_name(f);

	if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
		return fz_open_ahxd(chain);

	else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
		return fz_open_a85d(chain);

	else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
		return fz_open_faxd(chain, p);

	else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
		return fz_open_dctd(chain, p);

	else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
		return fz_open_rld(chain);

	else if (!strcmp(s, "FlateDecode") || !strcmp(s, "Fl"))
	{
		fz_obj *obj = fz_dict_gets(p, "Predictor");
		if (fz_to_int(obj) > 1)
			return fz_open_predict(fz_open_flated(chain), p);
		return fz_open_flated(chain);
	}

	else if (!strcmp(s, "LZWDecode") || !strcmp(s, "LZW"))
	{
		fz_obj *obj = fz_dict_gets(p, "Predictor");
		if (fz_to_int(obj) > 1)
			return fz_open_predict(fz_open_lzwd(chain, p), p);
		return fz_open_lzwd(chain, p);
	}

	else if (!strcmp(s, "JBIG2Decode"))
	{
		fz_obj *obj = fz_dict_gets(p, "JBIG2Globals");
		if (obj)
		{
			fz_buffer *globals;
			error = pdf_load_stream(&globals, xref, fz_to_num(obj), fz_to_gen(obj));
			if (error)
				fz_catch(error, "cannot load jbig2 global segments");
			chain = fz_open_jbig2d(chain, globals);
			fz_drop_buffer(globals);
			return chain;
		}
		return fz_open_jbig2d(chain, NULL);
	}

	else if (!strcmp(s, "JPXDecode"))
		return chain; /* JPX decoding is special cased in the image loading code */

	else if (!strcmp(s, "Crypt"))
	{
		fz_obj *name;

		if (!xref->crypt)
		{
			fz_warn("crypt filter in unencrypted document");
			return chain;
		}

		name = fz_dict_gets(p, "Name");
		if (fz_is_name(name))
			return pdf_open_crypt_with_filter(chain, xref->crypt, fz_to_name(name), num, gen);

		return chain;
	}

	fz_warn("unknown filter name (%s)", s);
	return chain;
}

/*
 * Build a chain of filters given filter names and param dicts.
 * If head is given, start filter chain with it.
 * Assume ownership of head.
 */
static fz_stream *
build_filter_chain(fz_stream *chain, pdf_xref *xref, fz_obj *fs, fz_obj *ps, int num, int gen)
{
	fz_obj *f;
	fz_obj *p;
	int i;

	for (i = 0; i < fz_array_len(fs); i++)
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
pdf_open_raw_filter(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	int hascrypt;
	int len;

	/* don't close chain when we close this filter */
	fz_keep_stream(chain);

	len = fz_to_int(fz_dict_gets(stmobj, "Length"));
	chain = fz_open_null(chain, len);

	hascrypt = pdf_stream_has_crypt(stmobj);
	if (xref->crypt && !hascrypt)
		chain = pdf_open_crypt(chain, xref->crypt, num, gen);

	return chain;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_stream *
pdf_open_filter(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dict_getsa(stmobj, "Filter", "F");
	params = fz_dict_getsa(stmobj, "DecodeParms", "DP");

	chain = pdf_open_raw_filter(chain, xref, stmobj, num, gen);

	if (fz_is_name(filters))
		return build_filter(chain, xref, filters, params, num, gen);
	if (fz_array_len(filters) > 0)
		return build_filter_chain(chain, xref, filters, params, num, gen);

	return chain;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_stream *
pdf_open_inline_stream(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int length)
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
fz_error
pdf_open_raw_stream(fz_stream **stmp, pdf_xref *xref, int num, int gen)
{
	pdf_xref_entry *x;
	fz_error error;

	if (num < 0 || num >= xref->len)
		return fz_throw("object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	error = pdf_cache_object(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream object (%d %d R)", num, gen);

	if (x->stm_ofs)
	{
		*stmp = pdf_open_raw_filter(xref->file, xref, x->obj, num, gen);
		fz_seek(xref->file, x->stm_ofs, 0);
		return fz_okay;
	}

	return fz_throw("object is not a stream");
}

/*
 * Open a stream for reading uncompressed data.
 * Put the opened file in xref->stream.
 * Using xref->file while a stream is open is a Bad idea.
 */
fz_error
pdf_open_stream(fz_stream **stmp, pdf_xref *xref, int num, int gen)
{
	pdf_xref_entry *x;
	fz_error error;

	if (num < 0 || num >= xref->len)
		return fz_throw("object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	error = pdf_cache_object(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream object (%d %d R)", num, gen);

	if (x->stm_ofs)
	{
		*stmp = pdf_open_filter(xref->file, xref, x->obj, num, gen);
		fz_seek(xref->file, x->stm_ofs, 0);
		return fz_okay;
	}

	return fz_throw("object is not a stream");
}

fz_error
pdf_open_stream_at(fz_stream **stmp, pdf_xref *xref, int num, int gen, fz_obj *dict, int stm_ofs)
{
	if (stm_ofs)
	{
		*stmp = pdf_open_filter(xref->file, xref, dict, num, gen);
		fz_seek(xref->file, stm_ofs, 0);
		return fz_okay;
	}
	return fz_throw("object is not a stream");
}

/*
 * Load raw (compressed but decrypted) contents of a stream into buf.
 */
fz_error
pdf_load_raw_stream(fz_buffer **bufp, pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *dict;
	int len;

	error = pdf_load_object(&dict, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream dictionary (%d %d R)", num, gen);

	len = fz_to_int(fz_dict_gets(dict, "Length"));

	fz_drop_obj(dict);

	error = pdf_open_raw_stream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open raw stream (%d %d R)", num, gen);

	error = fz_read_all(bufp, stm, len);
	if (error)
	{
		fz_close(stm);
		return fz_rethrow(error, "cannot read raw stream (%d %d R)", num, gen);
	}

	fz_close(stm);
	return fz_okay;
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
fz_error
pdf_load_stream(fz_buffer **bufp, pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *dict, *obj;
	int i, len;

	error = pdf_open_stream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open stream (%d %d R)", num, gen);

	error = pdf_load_object(&dict, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream dictionary (%d %d R)", num, gen);

	len = fz_to_int(fz_dict_gets(dict, "Length"));
	obj = fz_dict_gets(dict, "Filter");
	len = pdf_guess_filter_length(len, fz_to_name(obj));
	for (i = 0; i < fz_array_len(obj); i++)
		len = pdf_guess_filter_length(len, fz_to_name(fz_array_get(obj, i)));

	fz_drop_obj(dict);

	error = fz_read_all(bufp, stm, len);
	if (error)
	{
		fz_close(stm);
		return fz_rethrow(error, "cannot read raw stream (%d %d R)", num, gen);
	}

	fz_close(stm);
	return fz_okay;
}
