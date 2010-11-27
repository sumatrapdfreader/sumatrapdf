#include "fitz.h"
#include "mupdf.h"

/*
 * Check if an object is a stream or not.
 */
int
pdf_isstream(pdf_xref *xref, int num, int gen)
{
	fz_error error;

	if (num < 0 || num >= xref->len)
		return 0;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
	{
		fz_catch(error, "cannot load object, ignoring error");
		return 0;
	}

	return xref->table[num].stmofs > 0;
}

/*
 * Scan stream dictionary for an explicit /Crypt filter
 */
static int
pdf_streamhascrypt(fz_obj *stm)
{
	fz_obj *filters;
	fz_obj *obj;
	int i;

	filters = fz_dictgetsa(stm, "Filter", "F");
	if (filters)
	{
		if (!strcmp(fz_toname(filters), "Crypt"))
			return 1;
		if (fz_isarray(filters))
		{
			for (i = 0; i < fz_arraylen(filters); i++)
			{
				obj = fz_arrayget(filters, i);
				if (!strcmp(fz_toname(obj), "Crypt"))
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
buildfilter(fz_stream *chain, pdf_xref * xref, fz_obj * f, fz_obj * p, int num, int gen)
{
	fz_error error;
	char *s;

	s = fz_toname(f);

	if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
		return fz_openahxd(chain);

	else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
		return fz_opena85d(chain);

	else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
		return fz_openfaxd(chain, p);

	else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
		return fz_opendctd(chain, p);

	else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
		return fz_openrld(chain);

	else if (!strcmp(s, "FlateDecode") || !strcmp(s, "Fl"))
	{
		fz_obj *obj = fz_dictgets(p, "Predictor");
		if (fz_toint(obj) > 1)
			return fz_openpredict(fz_openflated(chain), p);
		return fz_openflated(chain);
	}

	else if (!strcmp(s, "LZWDecode") || !strcmp(s, "LZW"))
	{
		fz_obj *obj = fz_dictgets(p, "Predictor");
		if (fz_toint(obj) > 1)
			return fz_openpredict(fz_openlzwd(chain, p), p);
		return fz_openlzwd(chain, p);
	}

	else if (!strcmp(s, "JBIG2Decode"))
	{
		fz_obj *obj = fz_dictgets(p, "JBIG2Globals");
		if (obj)
		{
			fz_buffer *globals;
			error = pdf_loadstream(&globals, xref, fz_tonum(obj), fz_togen(obj));
			if (error)
				fz_catch(error, "cannot load jbig2 global segments");
			chain = fz_openjbig2d(chain, globals);
			fz_dropbuffer(globals);
			return chain;
		}
		return fz_openjbig2d(chain, nil);
	}

	else if (!strcmp(s, "JPXDecode"))
		return chain; /* JPX decoding is special cased in the image loading code */

	else if (!strcmp(s, "Crypt"))
	{
		pdf_cryptfilter cf;
		fz_obj *name;

		if (!xref->crypt)
		{
			fz_warn("crypt filter in unencrypted document");
			return chain;
		}

		name = fz_dictgets(p, "Name");
		if (fz_isname(name) && strcmp(fz_toname(name), "Identity") != 0)
		{
			fz_obj *obj = fz_dictget(xref->crypt->cf, name);
			if (fz_isdict(obj))
			{
				error = pdf_parsecryptfilter(&cf, obj, xref->crypt->length);
				if (error)
					fz_catch(error, "cannot parse crypt filter (%d %d R)", fz_tonum(obj), fz_togen(obj));
				else
					return pdf_opencrypt(chain, xref->crypt, &cf, num, gen);
			}
		}

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
buildfilterchain(fz_stream *chain, pdf_xref *xref, fz_obj *fs, fz_obj *ps, int num, int gen)
{
	fz_obj *f;
	fz_obj *p;
	int i;

	for (i = 0; i < fz_arraylen(fs); i++)
	{
		f = fz_arrayget(fs, i);
		p = fz_arrayget(ps, i);
		chain = buildfilter(chain, xref, f, p, num, gen);
	}

	return chain;
}

/*
 * Build a filter for reading raw stream data.
 * This is a null filter to constrain reading to the
 * stream length, followed by a decryption filter.
 */
static fz_stream *
pdf_openrawfilter(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	int hascrypt;
	int len;

	/* don't close chain when we close this filter */
	fz_keepstream(chain);

	len = fz_toint(fz_dictgets(stmobj, "Length"));
	chain = fz_opennull(chain, len);

	hascrypt = pdf_streamhascrypt(stmobj);
	if (xref->crypt && !hascrypt)
		chain = pdf_opencrypt(chain, xref->crypt, &xref->crypt->stmf, num, gen);

	return chain;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_stream *
pdf_openfilter(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dictgetsa(stmobj, "Filter", "F");
	params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

	chain = pdf_openrawfilter(chain, xref, stmobj, num, gen);

	if (fz_isname(filters))
		return buildfilter(chain, xref, filters, params, num, gen);
	if (fz_arraylen(filters) > 0)
		return buildfilterchain(chain, xref, filters, params, num, gen);

	return chain;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_stream *
pdf_openinlinestream(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int length)
{
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dictgetsa(stmobj, "Filter", "F");
	params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

	/* don't close chain when we close this filter */
	fz_keepstream(chain);

	if (fz_isname(filters))
		return buildfilter(chain, xref, filters, params, 0, 0);
	if (fz_arraylen(filters) > 0)
		return buildfilterchain(chain, xref, filters, params, 0, 0);

	return fz_opennull(chain, length);
}

/*
 * Open a stream for reading the raw (compressed but decrypted) data.
 * Using xref->file while this is open is a bad idea.
 */
fz_error
pdf_openrawstream(fz_stream **stmp, pdf_xref *xref, int num, int gen)
{
	pdf_xrefentry *x;
	fz_error error;

	if (num < 0 || num >= xref->len)
		return fz_throw("object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream object (%d %d R)", num, gen);

	if (x->stmofs)
	{
		*stmp = pdf_openrawfilter(xref->file, xref, x->obj, num, gen);
		fz_seek(xref->file, x->stmofs, 0);
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
pdf_openstream(fz_stream **stmp, pdf_xref *xref, int num, int gen)
{
	pdf_xrefentry *x;
	fz_error error;

	if (num < 0 || num >= xref->len)
		return fz_throw("object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream object (%d %d R)", num, gen);

	if (x->stmofs)
	{
		*stmp = pdf_openfilter(xref->file, xref, x->obj, num, gen);
		fz_seek(xref->file, x->stmofs, 0);
		return fz_okay;
	}

	return fz_throw("object is not a stream");
}

fz_error
pdf_openstreamat(fz_stream **stmp, pdf_xref *xref, int num, int gen, fz_obj *dict, int stmofs)
{
	if (stmofs)
	{
		*stmp = pdf_openfilter(xref->file, xref, dict, num, gen);
		fz_seek(xref->file, stmofs, 0);
		return fz_okay;
	}
	return fz_throw("object is not a stream");
}

/*
 * Load raw (compressed but decrypted) contents of a stream into buf.
 */
fz_error
pdf_loadrawstream(fz_buffer **bufp, pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *dict;
	int len;

	error = pdf_loadobject(&dict, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream dictionary (%d %d R)", num, gen);

	len = fz_toint(fz_dictgets(dict, "Length"));

	fz_dropobj(dict);

	error = pdf_openrawstream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open raw stream (%d %d R)", num, gen);

	error = fz_readall(bufp, stm, len);
	if (error)
	{
		fz_close(stm);
		return fz_rethrow(error, "cannot read raw stream (%d %d R)", num, gen);
	}

	fz_close(stm);
	return fz_okay;
}

static int
pdf_guessfilterlength(int len, char *filter)
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
pdf_loadstream(fz_buffer **bufp, pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_stream *stm;
	fz_obj *dict, *obj;
	int i, len;

	error = pdf_openstream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open stream (%d %d R)", num, gen);

	error = pdf_loadobject(&dict, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream dictionary (%d %d R)", num, gen);

	len = fz_toint(fz_dictgets(dict, "Length"));
	obj = fz_dictgets(dict, "Filter");
	len = pdf_guessfilterlength(len, fz_toname(obj));
	for (i = 0; i < fz_arraylen(obj); i++)
		len = pdf_guessfilterlength(len, fz_toname(fz_arrayget(obj, i)));

	fz_dropobj(dict);

	error = fz_readall(bufp, stm, len);
	if (error)
	{
		fz_close(stm);
		return fz_rethrow(error, "cannot read raw stream (%d %d R)", num, gen);
	}

	fz_close(stm);
	return fz_okay;
}
