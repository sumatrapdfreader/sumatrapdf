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
		fz_catch(error, "could not load object, ignoring error");
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
		if (fz_isname(filters))
			if (!strcmp(fz_toname(filters), "Crypt"))
				return 1;
		if (fz_isarray(filters))
		{
			for (i = 0; i < fz_arraylen(filters); i++)
			{
				obj = fz_arrayget(filters, i);
				if (fz_isname(obj))
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
static fz_filter *
buildonefilter(pdf_xref * xref, fz_obj * f, fz_obj * p, int num, int gen)
{
	fz_filter *decompress;
	fz_filter *predict;
	fz_filter *pipe;
	fz_error error;
	char *s;

	s = fz_toname(f);

	if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
		return fz_newahxd(p);

	else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
		return fz_newa85d(p);

	else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
		return fz_newfaxd(p);

	else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
		return fz_newdctd(p);

	else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
		return fz_newrld(p);

	else if (!strcmp(s, "FlateDecode") || !strcmp(s, "Fl"))
	{
		fz_obj *obj = fz_dictgets(p, "Predictor");
		if (obj)
		{
			decompress = fz_newflated(p);
			predict = fz_newpredictd(p);
			pipe = fz_newpipeline(decompress, predict);
			fz_dropfilter(decompress);
			fz_dropfilter(predict);
			return pipe;
		}
		return fz_newflated(p);
	}

	else if (!strcmp(s, "LZWDecode") || !strcmp(s, "LZW"))
	{
		fz_obj *obj = fz_dictgets(p, "Predictor");
		if (obj)
		{
			decompress = fz_newlzwd(p);
			predict = fz_newpredictd(p);
			pipe = fz_newpipeline(decompress, predict);
			fz_dropfilter(decompress);
			fz_dropfilter(predict);
			return pipe;
		}
		return fz_newlzwd(p);
	}

#ifdef HAVE_JBIG2DEC
	else if (!strcmp(s, "JBIG2Decode"))
	{
		fz_obj *obj = fz_dictgets(p, "JBIG2Globals");
		if (obj)
		{
			fz_buffer *globals;
			fz_filter *dec;

			dec = fz_newjbig2d(p);

			error = pdf_loadstream(&globals, xref, fz_tonum(obj), fz_togen(obj));
			if (error)
				fz_catch(error, "cannot load jbig2 global segments");
			else
			{
				error = fz_setjbig2dglobalstream(dec, globals->rp, globals->wp - globals->rp);
				if (error)
					fz_catch(error, "cannot apply jbig2 global segments");
			}

			fz_dropbuffer(globals);

			return dec;
		}
		return fz_newjbig2d(p);
	}
#endif

#ifdef HAVE_OPENJPEG
	else if (!strcmp(s, "JPXDecode"))
		return fz_newjpxd(p);
#endif

	else if (!strcmp(s, "Crypt"))
	{
		pdf_cryptfilter cf;
		fz_obj *name;

		if (!xref->crypt)
		{
			fz_warn("crypt filter in unencrypted document");
			return fz_newcopyfilter();
		}

		name = fz_dictgets(p, "Name");
		if (fz_isname(name) && strcmp(fz_toname(name), "Identity") != 0)
		{
			fz_obj *obj = fz_dictget(xref->crypt->cf, name);
			if (fz_isdict(obj))
			{
				error = pdf_parsecryptfilter(&cf, obj, xref->crypt->length);
				if (error)
					fz_catch(error, "cannot parse crypt filter");
				else
					return pdf_cryptstream(xref->crypt, &cf, num, gen);
			}
		}
		return fz_newcopyfilter();
	}

	fz_warn("unknown filter name (%s)", s);
	return fz_newcopyfilter();
}

/*
 * Build a chain of filters given filter names and param dicts.
 * If head is given, start filter chain with it.
 * Assume ownership of head.
 */
static fz_filter *
buildfilterchain(pdf_xref *xref, fz_filter *head,
	fz_obj *fs, fz_obj *ps, int num, int gen)
{
	fz_filter *newhead;
	fz_filter *tail;
	fz_obj *f;
	fz_obj *p;
	int i;

	for (i = 0; i < fz_arraylen(fs); i++)
	{
		f = fz_arrayget(fs, i);
		if (fz_isarray(ps))
			p = fz_arrayget(ps, i);
		else
			p = nil;

		tail = buildonefilter(xref, f, p, num, gen);
		if (head)
		{
			newhead = fz_newpipeline(head, tail);
			fz_dropfilter(head);
			fz_dropfilter(tail);
			head = newhead;
		}
		else
			head = tail;
	}

	return head;
}

/*
 * Build a filter for reading raw stream data.
 * This is a null filter to constrain reading to the
 * stream length, followed by a decryption filter.
 */
static fz_filter *
buildrawfilter(pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	fz_filter *base;
	fz_obj *stmlen;
	int len;
	int hascrypt;

	hascrypt = pdf_streamhascrypt(stmobj);

	stmlen = fz_dictgets(stmobj, "Length");
	len = fz_toint(stmlen);

	base = fz_newnullfilter(len);

	if (xref->crypt && !hascrypt)
	{
		fz_filter *crypt;
		fz_filter *pipe;
		crypt = pdf_cryptstream(xref->crypt, &xref->crypt->stmf, num, gen);
		pipe = fz_newpipeline(base, crypt);
		fz_dropfilter(base);
		fz_dropfilter(crypt);
		return pipe;
	}

	return base;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_filter *
pdf_buildinlinefilter(pdf_xref *xref, fz_obj *stmobj)
{
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dictgetsa(stmobj, "Filter", "F");
	params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

	if (filters)
	{
		if (fz_isname(filters))
			return buildonefilter(xref, filters, params, 0, 0);
		return buildfilterchain(xref, nil, filters, params, 0, 0);
	}

	return fz_newcopyfilter();
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_filter *
pdf_buildfilter(pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	fz_filter *base, *pipe, *tmp;
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dictgetsa(stmobj, "Filter", "F");
	params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

	base = buildrawfilter(xref, stmobj, num, gen);

	if (filters)
	{
		if (fz_isname(filters))
		{
			tmp = buildonefilter(xref, filters, params, num, gen);
			pipe = fz_newpipeline(base, tmp);
			fz_dropfilter(base);
			fz_dropfilter(tmp);
			return pipe;
		}
		else
		{
			/* The pipeline chain takes ownership of base */
			return buildfilterchain(xref, base, filters, params, num, gen);
		}
	}

	return base;
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
	fz_filter *filter;

	if (num < 0 || num >= xref->len)
		return fz_throw("object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream object (%d %d R)", num, gen);

	if (x->stmofs)
	{
		filter = buildrawfilter(xref, x->obj, num, gen);

		error = fz_seek(xref->file, x->stmofs, 0);
		if (error)
			return fz_rethrow(error, "cannot seek to stream");

		*stmp = fz_openrfilter(filter, xref->file);
		fz_dropfilter(filter);
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
	fz_filter *filter;

	if (num < 0 || num >= xref->len)
		return fz_throw("object id out of range (%d %d R)", num, gen);

	x = xref->table + num;

	error = pdf_cacheobject(xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot load stream object (%d %d R)", num, gen);

	if (x->stmofs)
	{
		filter = pdf_buildfilter(xref, x->obj, num, gen);

		error = fz_seek(xref->file, x->stmofs, 0);
		if (error)
			return fz_rethrow(error, "cannot seek to stream");

		*stmp = fz_openrfilter(filter, xref->file);
		fz_dropfilter(filter);
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

	error = pdf_openrawstream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open raw stream (%d %d R)", num, gen);

	*bufp = fz_readall(stm, 0); // TODO extract io errors
	fz_dropstream(stm);
	return fz_okay;
}

/*
 * Load uncompressed contents of a stream into buf.
 */
fz_error
pdf_loadstream(fz_buffer **bufp, pdf_xref *xref, int num, int gen)
{
	fz_error error;
	fz_stream *stm;

	error = pdf_openstream(&stm, xref, num, gen);
	if (error)
		return fz_rethrow(error, "cannot open stream (%d %d R)", num, gen);

	*bufp = fz_readall(stm, 0); // TODO extract io errors
	fz_dropstream(stm);
	return fz_okay;
}

