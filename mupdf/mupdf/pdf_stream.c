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
static fz_error
buildonefilter(fz_filter **fp, pdf_xref *xref, fz_obj *f, fz_obj *p, int num, int gen)
{
	fz_filter *decompress;
	fz_filter *predict;
	fz_error error;
	char *s;

	s = fz_toname(f);

	if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
		error = fz_newahxd(fp, p);

	else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
		error = fz_newa85d(fp, p);

	else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
		error = fz_newfaxd(fp, p);

	else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
		error = fz_newdctd(fp, p);

	else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
		error = fz_newrld(fp, p);

	else if (!strcmp(s, "FlateDecode") || !strcmp(s, "Fl"))
	{
		if (fz_isdict(p))
		{
			fz_obj *obj = fz_dictgets(p, "Predictor");
			if (obj)
			{
				error = fz_newflated(&decompress, p);
				if (error)
					return fz_rethrow(error, "cannot create deflate filter");

				error = fz_newpredictd(&predict, p);
				if (error)
				{
					fz_dropfilter(decompress);
					return fz_rethrow(error, "cannot create predictor filter");
				}

				error = fz_newpipeline(fp, decompress, predict);
				fz_dropfilter(decompress);
				fz_dropfilter(predict);
				if (error)
					return fz_rethrow(error, "cannot create pipeline filter");
				return fz_okay;
			}
		}
		error = fz_newflated(fp, p);
	}

	else if (!strcmp(s, "LZWDecode") || !strcmp(s, "LZW"))
	{
		if (fz_isdict(p))
		{
			fz_obj *obj = fz_dictgets(p, "Predictor");
			if (obj)
			{
				error = fz_newlzwd(&decompress, p);
				if (error)
					return fz_rethrow(error, "cannot create lzwd filter");

				error = fz_newpredictd(&predict, p);
				if (error)
				{
					fz_dropfilter(decompress);
					return fz_rethrow(error, "cannot create predictor filter");
				}

				error = fz_newpipeline(fp, decompress, predict);
				fz_dropfilter(decompress);
				fz_dropfilter(predict);
				if (error)
					return fz_rethrow(error, "cannot create pipeline filter");
				return fz_okay;
			}
		}
		error = fz_newlzwd(fp, p);
	}

#ifdef HAVE_JBIG2DEC
	else if (!strcmp(s, "JBIG2Decode"))
	{
		if (fz_isdict(p))
		{
			fz_obj *obj = fz_dictgets(p, "JBIG2Globals");
			if (obj)
			{
				fz_buffer *globals;

				error = fz_newjbig2d(fp, p);
				if (error)
					return fz_rethrow(error, "cannot create jbig2 filter");

				error = pdf_loadstream(&globals, xref, fz_tonum(obj), fz_togen(obj));
				if (error)
					return fz_rethrow(error, "cannot load jbig2 global segments");

				error = fz_setjbig2dglobalstream(*fp, globals->rp, globals->wp - globals->rp);
				if (error)
					return fz_rethrow(error, "cannot apply jbig2 global segments");

				fz_dropbuffer(globals);

				return fz_okay;
			}
		}

		error = fz_newjbig2d(fp, p);
	}
#endif

#ifdef HAVE_JASPER
	else if (!strcmp(s, "JPXDecode"))
		error = fz_newjpxd(fp, p);
#endif
#ifdef HAVE_OPENJPEG
	else if (!strcmp(s, "JPXDecode"))
		error = fz_newjpxd(fp, p);
#endif

	else if (!strcmp(s, "Crypt"))
	{
		pdf_cryptfilter cf;
		fz_obj *name;

		if (!xref->crypt)
			return fz_throw("crypt filter in unencrypted document");

		name = fz_dictgets(p, "Name");
		if (fz_isname(name) && strcmp(fz_toname(name), "Identity") != 0)
		{
			fz_obj *obj = fz_dictget(xref->crypt->cf, name);
			if (fz_isdict(obj))
			{
				error = pdf_parsecryptfilter(&cf, obj, xref->crypt->length);
				if (error)
					return fz_rethrow(error, "cannot parse crypt filter");

				error = pdf_cryptstream(fp, xref->crypt, &cf, num, gen);
				if (error)
					return fz_rethrow(error, "cannot create crypt filter");
				return fz_okay;
			}
		}

		error = fz_newcopyfilter(fp);
		if (error)
		    return fz_rethrow(error, "cannot create identity crypt filter");
		return fz_okay;
	}

	else
	{
		return fz_throw("unknown filter name (%s)", s);
	}

	if (error)
		return fz_rethrow(error, "cannot create filter");
	return fz_okay;
}

/*
 * Build a chain of filters given filter names and param dicts.
 * If head is given, start filter chain with it.
 * Assume ownership of head.
 */
static fz_error
buildfilterchain(fz_filter **filterp, pdf_xref *xref, fz_filter *head,
	fz_obj *fs, fz_obj *ps, int num, int gen)
{
	fz_error error;
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

		error = buildonefilter(&tail, xref, f, p, num, gen);
		if (error)
			return fz_rethrow(error, "cannot create filter");

		if (head)
		{
			error = fz_newpipeline(&newhead, head, tail);
			fz_dropfilter(head);
			fz_dropfilter(tail);
			if (error)
			{
				fz_dropfilter(newhead);
				return fz_rethrow(error, "cannot create pipeline filter");
			}
			head = newhead;
		}
		else
			head = tail;
	}

	*filterp = head;
	return fz_okay;
}

/*
 * Build a filter for reading raw stream data.
 * This is a null filter to constrain reading to the
 * stream length, followed by a decryption filter.
 */
static fz_error
buildrawfilter(fz_filter **filterp, pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	fz_error error;
	fz_filter *base;
	fz_obj *stmlen;
	int len;
	int hascrypt;

	hascrypt = pdf_streamhascrypt(stmobj);

	stmlen = fz_dictgets(stmobj, "Length");
	if (!fz_isint(stmlen))
		return fz_throw("corrupt stream length");
	len = fz_toint(stmlen);

	error = fz_newnullfilter(&base, len);
	if (error)
		return fz_rethrow(error, "cannot create null filter");

	if (xref->crypt && !hascrypt)
	{
		fz_filter *crypt;
		fz_filter *pipe;

		error = pdf_cryptstream(&crypt, xref->crypt, &xref->crypt->stmf, num, gen);
		if (error)
		{
			fz_dropfilter(base);
			return fz_rethrow(error, "cannot create decryption filter");
		}

		error = fz_newpipeline(&pipe, base, crypt);
		fz_dropfilter(base);
		fz_dropfilter(crypt);
		if (error)
			return fz_rethrow(error, "cannot create pipeline filter");

		*filterp = pipe;
	}
	else
	{
		*filterp = base;
	}

	return fz_okay;
}

/*
 * Construct a filter to decode a stream, without
 * constraining to stream length, and without decryption.
 */
fz_error
pdf_buildinlinefilter(fz_filter **filterp, pdf_xref *xref, fz_obj *stmobj)
{
	fz_error error;
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dictgetsa(stmobj, "Filter", "F");
	params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

	if (filters)
	{
		if (fz_isname(filters))
			error = buildonefilter(filterp, xref, filters, params, 0, 0);
		else
			error = buildfilterchain(filterp, xref, nil, filters, params, 0, 0);
	}
	else
		error = fz_newnullfilter(filterp, -1);

	if (error)
		return fz_rethrow(error, "cannot create inline filter chain");
	return fz_okay;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_error
pdf_buildfilter(fz_filter **filterp, pdf_xref *xref, fz_obj *stmobj, int num, int gen)
{
	fz_error error;
	fz_filter *base, *pipe, *tmp;
	fz_obj *filters;
	fz_obj *params;

	filters = fz_dictgetsa(stmobj, "Filter", "F");
	params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

	error = buildrawfilter(&base, xref, stmobj, num, gen);
	if (error)
		return fz_rethrow(error, "cannot create raw filter chain");

	if (filters)
	{
		if (fz_isname(filters))
		{
			error = buildonefilter(&tmp, xref, filters, params, num, gen);
			if (error)
			{
				fz_dropfilter(base);
				return fz_rethrow(error, "cannot create filter");
			}

			error = fz_newpipeline(&pipe, base, tmp);
			if (error)
			{
				fz_dropfilter(base);
				fz_dropfilter(tmp);
				return fz_rethrow(error, "cannot create filter pipeline");
			}

			fz_dropfilter(base);
			fz_dropfilter(tmp);
		}
		else
		{
			/* The pipeline chain takes ownership of base */
			error = buildfilterchain(&pipe, xref, base, filters, params, num, gen);
			if (error)
				return fz_rethrow(error, "cannot create filter chain");
		}

		*filterp = pipe;
	}
	else
	{
		*filterp = base;
	}

	return fz_okay;
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
		error = buildrawfilter(&filter, xref, x->obj, num, gen);
		if (error)
			return fz_rethrow(error, "cannot create raw filter");

		error = fz_seek(xref->file, x->stmofs, 0);
		if (error)
		{
			fz_dropfilter(filter);
			return fz_rethrow(error, "cannot seek to stream");
		}

		error = fz_openrfilter(stmp, filter, xref->file);

		fz_dropfilter(filter);

		if (error)
			return fz_rethrow(error, "cannot open filter stream");

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
		error = pdf_buildfilter(&filter, xref, x->obj, num, gen);
		if (error)
			return fz_rethrow(error, "cannot create filter");

		error = fz_seek(xref->file, x->stmofs, 0);
		if (error)
		{
			fz_dropfilter(filter);
			return fz_rethrow(error, "cannot seek to stream");
		}

		error = fz_openrfilter(stmp, filter, xref->file);
		fz_dropfilter(filter);
		if (error)
			return fz_rethrow(error, "cannot open filter stream");

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

	error = fz_readall(bufp, stm, 0);
	fz_dropstream(stm);
	if (error)
		return fz_rethrow(error, "cannot load stream into buffer (%d %d R)", num, gen);
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

	error = fz_readall(bufp, stm, 0);
	fz_dropstream(stm);
	if (error)
		return fz_rethrow(error, "cannot load stream into buffer (%d %d R)", num, gen);
	return fz_okay;
}

