#include "fitz.h"
#include "mupdf.h"

/*
 * Check if an object is a stream or not.
 */
int
pdf_isstream(pdf_xref *xref, int oid, int gen)
{
    fz_error *error;

    if (oid < 0 || oid >= xref->len)
        return 0;

    error = pdf_cacheobject(xref, oid, gen);
    if (error)
    {
        fz_warn("%s", error->msg);
        fz_droperror(error);
        return 0;
    }

    return xref->table[oid].stmbuf || xref->table[oid].stmofs;
}

/*
 * Create a filter given a name and param dictionary.
 */
static fz_error *
buildonefilter(fz_filter **fp, fz_obj *f, fz_obj *p)
{
    fz_filter *decompress;
    fz_filter *predict;
    fz_error *error;
    char *s;

    s = fz_toname(f);

    if (!strcmp(s, "ASCIIHexDecode") || !strcmp(s, "AHx"))
        error =  fz_newahxd(fp, p);

    else if (!strcmp(s, "ASCII85Decode") || !strcmp(s, "A85"))
        error =  fz_newa85d(fp, p);

    else if (!strcmp(s, "CCITTFaxDecode") || !strcmp(s, "CCF"))
        error =  fz_newfaxd(fp, p);

    else if (!strcmp(s, "DCTDecode") || !strcmp(s, "DCT"))
        error =  fz_newdctd(fp, p);

    else if (!strcmp(s, "RunLengthDecode") || !strcmp(s, "RL"))
        error =  fz_newrld(fp, p);

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
        /* TODO: extract and feed JBIG2Global */
        error = fz_newjbig2d(fp, p);
    }
#endif

#ifdef HAVE_JASPER
    else if (!strcmp(s, "JPXDecode"))
        error = fz_newjpxd(fp, p);
#endif

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
static fz_error *
buildfilterchain(fz_filter **filterp, fz_filter *head, fz_obj *fs, fz_obj *ps)
{
    fz_error *error;
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

        error = buildonefilter(&tail, f, p);
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
static fz_error *
buildrawfilter(fz_filter **filterp, pdf_xref *xref, fz_obj *stmobj, int oid, int gen)
{
    fz_error *error;
    fz_filter *base;
    fz_obj *stmlen;
    int len;

    stmlen = fz_dictgets(stmobj, "Length");
    error = pdf_resolve(&stmlen, xref);
    if (error)
        return fz_rethrow(error, "cannot resolve stream /Length");
    len = fz_toint(stmlen);
    fz_dropobj(stmlen);

    error = fz_newnullfilter(&base, len);
    if (error)
        return fz_rethrow(error, "cannot create null filter");

    if (xref->crypt)
    {
        fz_filter *crypt;
        fz_filter *pipe;

        error = pdf_cryptstream(&crypt, xref->crypt, oid, gen);
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
fz_error *
pdf_buildinlinefilter(fz_filter **filterp, fz_obj *stmobj)
{
//    fz_error *error;
    fz_obj *filters;
    fz_obj *params;

    filters = fz_dictgetsa(stmobj, "Filter", "F");
    params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

    if (filters)
    {
	if (fz_isname(filters))
	    return buildonefilter(filterp, filters, params);
	else if (fz_arraylen(filters) > 0)
	    return buildfilterchain(filterp, nil, filters, params);
    }

    /* uh oh, no filter */
    *filterp = nil;

//    if (error)
//        return fz_rethrow(error, "cannot create inline filter chain");
    return fz_okay;
}

/*
 * Construct a filter to decode a stream, constraining
 * to stream length and decrypting.
 */
static fz_error *
pdf_buildfilter(fz_filter **filterp, pdf_xref *xref, fz_obj *stmobj, int oid, int gen)
{
    fz_error *error;
    fz_filter *base, *pipe, *tmp;
    fz_obj *filters;
    fz_obj *params;

    error = buildrawfilter(&base, xref, stmobj, oid, gen);
    if (error)
        return fz_rethrow(error, "cannot create raw filter chain");

    filters = fz_dictgetsa(stmobj, "Filter", "F");
    params = fz_dictgetsa(stmobj, "DecodeParms", "DP");

    if (filters)
    {
        error = pdf_resolve(&filters, xref);
        if (error)
        {
            error = fz_rethrow(error, "cannot resolve stream /Filter");
            goto cleanup0;
        }

        if (params)
        {
            error = pdf_resolve(&params, xref);
            if (error)
            {
                error = fz_rethrow(error, "cannot resolve stream /DecodeParms");
                goto cleanup1;
            }
        }

        if (fz_isname(filters))
        {
            error = buildonefilter(&tmp, filters, params);
            if (error)
            {
                error = fz_rethrow(error, "cannot create filter");
                goto cleanup2;
            }

            error = fz_newpipeline(&pipe, base, tmp);
            fz_dropfilter(tmp);
            if (error)
            {
                error = fz_rethrow(error, "cannot create filter pipeline");
                goto cleanup2;
            }
        }
        else
        {
            error = buildfilterchain(&pipe, base, filters, params);
            if (error)
            {
                error = fz_rethrow(error, "cannot create filter chain");
                goto cleanup2;
            }
        }

        if (params)
            fz_dropobj(params);

        fz_dropobj(filters);

        *filterp = pipe;
    }
    else
    {
        *filterp = base;
    }

    return fz_okay;

cleanup2:
    if (params)
        fz_dropobj(params);
cleanup1:
    fz_dropobj(filters);
cleanup0:
    fz_dropfilter(base);
    return error;
}

/*
 * Open a stream for reading the raw (compressed but decrypted) data. 
 * Using xref->file while this is open is a bad idea.
 */
fz_error *
pdf_openrawstream(fz_stream **stmp, pdf_xref *xref, int oid, int gen)
{
    pdf_xrefentry *x;
    fz_error *error;
    fz_filter *filter;

    if (oid < 0 || oid >= xref->len)
        return fz_throw("object id out of range (%d)", oid);

    x = xref->table + oid;

    error = pdf_cacheobject(xref, oid, gen);
    if (error)
        return fz_rethrow(error, "cannot load stream object (%d)", oid);

    if (x->stmbuf)
    {
        error = fz_openrbuffer(stmp, x->stmbuf);
        if (error)
            return fz_rethrow(error, "cannot open stream from buffer");
        return fz_okay;
    }

    if (x->stmofs)
    {
        error = buildrawfilter(&filter, xref, x->obj, oid, gen);
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
fz_error *
pdf_openstream(fz_stream **stmp, pdf_xref *xref, int oid, int gen)
{
    pdf_xrefentry *x;
    fz_error *error;
    fz_stream *rawstm;
    fz_filter *filter;

    if (oid < 0 || oid >= xref->len)
        return fz_throw("object id out of range (%d)", oid);

    x = xref->table + oid;

    error = pdf_cacheobject(xref, oid, gen);
    if (error)
        return fz_rethrow(error, "cannot load stream object (%d)", oid);

    if (x->stmbuf)
    {
        error = pdf_buildfilter(&filter, xref, x->obj, oid, gen);
        if (error)
            return fz_rethrow(error, "cannot create filter");

        error = fz_openrbuffer(&rawstm, x->stmbuf);
        if (error)
        {
            fz_dropfilter(filter);
            return fz_rethrow(error, "cannot open stream from buffer");
        }

        error = fz_openrfilter(stmp, filter, rawstm);
        fz_dropfilter(filter);
        fz_dropstream(rawstm);
        if (error)
            return fz_rethrow(error, "cannot open filter stream");
        return fz_okay;
    }

    if (x->stmofs)
    {
        error = pdf_buildfilter(&filter, xref, x->obj, oid, gen);
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
fz_error *
pdf_loadrawstream(fz_buffer **bufp, pdf_xref *xref, int oid, int gen)
{
    fz_error *error;
    fz_stream *stm;

    error = pdf_openrawstream(&stm, xref, oid, gen);
    if (error)
        return fz_rethrow(error, "cannot open raw stream (%d)", oid);

    error = fz_readall(bufp, stm);
    fz_dropstream(stm);
    if (error)
        return fz_rethrow(error, "cannot load stream into buffer (%d)", oid);
    return fz_okay;
}

/*
 * Load uncompressed contents of a stream into buf.
 */
fz_error *
pdf_loadstream(fz_buffer **bufp, pdf_xref *xref, int oid, int gen)
{
    fz_error *error;
    fz_stream *stm;

    error = pdf_openstream(&stm, xref, oid, gen);
    if (error)
        return fz_rethrow(error, "cannot open stream (%d)", oid);

    error = fz_readall(bufp, stm);
    fz_dropstream(stm);
    if (error)
        return fz_rethrow(error, "cannot load stream into buffer (%d)", oid);
    return fz_okay;
}

