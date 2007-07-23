#include "fitz.h"
#include "mupdf.h"

static inline int iswhite(int ch)
{
    return
        ch == '\000' || ch == '\011' || ch == '\012' ||
        ch == '\014' || ch == '\015' || ch == '\040';
}

/*
 * magic version tag and startxref
 */

static fz_error *
loadversion(pdf_xref *xref)
{
    fz_error *error;
    char buf[20];

    error = fz_seek(xref->file, 0, 0);
    if (error)
        return fz_rethrow(error, "cannot seek to beginning of file");

    error = fz_readline(xref->file, buf, sizeof buf);
    if (error)
        return fz_rethrow(error, "cannot read version marker");
    if (memcmp(buf, "%PDF-", 5) != 0)
        return fz_throw("cannot recognize version marker");

    xref->version = atof(buf + 5) * 10.0;

    pdf_logxref("version %d.%d\n", xref->version / 10, xref->version % 10);

    return fz_okay;
}

static fz_error *
readstartxref(pdf_xref *xref)
{
    fz_error *error;
    char buf[1024];
    int t, n;
    int i;

    error = fz_seek(xref->file, 0, 2);
    if (error)
        return fz_rethrow(error, "cannot seek to end of file");

    t = MAX(0, fz_tell(xref->file) - ((int)sizeof buf));
    error = fz_seek(xref->file, t, 0);
    if (error)
        return fz_rethrow(error, "cannot seek to offset %d", t);

    error = fz_read(&n, xref->file, buf, sizeof buf);
    if (error)
        return fz_rethrow(error, "cannot read from file");

    for (i = n - 9; i >= 0; i--)
    {
        if (memcmp(buf + i, "startxref", 9) == 0)
        {
            i += 9;
            while (iswhite(buf[i]) && i < n)
                i ++;
            xref->startxref = atoi(buf + i);
            return fz_okay;
        }
    }

    return fz_throw("cannot find startxref");
}

/*
 * trailer dictionary
 */

static fz_error *
readoldtrailer(pdf_xref *xref, char *buf, int cap)
{
    fz_error *error;
    int ofs, len;
    char *s;
    int n = 0;
    int t;
    int c;

    pdf_logxref("load old xref format trailer\n");

    error = fz_readline(xref->file, buf, cap);
    if (error)
        return fz_rethrow(error, "cannot read xref marker");
    if (strcmp(buf, "xref") != 0)
        return fz_throw("cannot find xref marker");

    while (1)
    {
        c = fz_peekbyte(xref->file);
        if (!(c >= '0' && c <= '9'))
            break;

        error = fz_readline(xref->file, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot read xref count");

        s = buf;
        ofs = atoi(strsep(&s, " "));
        len = atoi(strsep(&s, " "));

        /* broken pdfs where the section is not on a separate line */
        if (s && *s != '\0')
        {
            error = fz_seek(xref->file, -(n + buf - s + 2), 1);
            if (error)
                return fz_rethrow(error, "cannot seek in file");
        }

        t = fz_tell(xref->file);
        if (t < 0)
            return fz_throw("cannot tell in file");

        error = fz_seek(xref->file, t + 20 * len, 0);
        if (error)
            return fz_rethrow(error, "cannot seek in file");
    }

    error = fz_readerror(xref->file);
    if (error)
        return fz_rethrow(error, "cannot read from file");

    error = pdf_lex(&t, xref->file, buf, cap, &n);
    if (error)
        return fz_rethrow(error, "cannot parse trailer");
    if (t != PDF_TTRAILER)
        return fz_throw("expected trailer marker");

    error = pdf_lex(&t, xref->file, buf, cap, &n);
    if (error)
        return fz_rethrow(error, "cannot parse trailer");
    if (t != PDF_TODICT)
        return fz_throw("expected trailer dictionary");

    error = pdf_parsedict(&xref->trailer, xref->file, buf, cap);
    if (error)
        return fz_rethrow(error, "cannot parse trailer");
    return fz_okay;
}

static fz_error *
readnewtrailer(pdf_xref *xref, char *buf, int cap)
{
    fz_error *error;

    pdf_logxref("load new xref format trailer\n");

    error = pdf_parseindobj(&xref->trailer, xref->file, buf, cap, nil, nil, nil);
    if (error)
        return fz_rethrow(error, "cannot parse trailer (compressed)");
    return fz_okay;
}

static fz_error *
readtrailer(pdf_xref *xref, char *buf, int cap)
{
    fz_error *error;
    int c;

    error = fz_seek(xref->file, xref->startxref, 0);
    if (error)
        return fz_rethrow(error, "cannot seek to startxref");

    c = fz_peekbyte(xref->file);
    error = fz_readerror(xref->file);
    if (error)
        return fz_rethrow(error, "cannot read trailer");

    if (c == 'x')
    {
        error = readoldtrailer(xref, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot read trailer");
    }
    else if (c >= '0' && c <= '9')
    {
        error = readnewtrailer(xref, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot read trailer");
    }
    else
    {
        return fz_throw("cannot recognize xref format");
    }

    return fz_okay;
}

/*
 * xref tables
 */

static fz_error *
readoldxref(fz_obj **trailerp, pdf_xref *xref, char *buf, int cap)
{
    fz_error *error;
    int ofs, len;
    char *s;
    int n;
    int t;
    int i;
    int c;

    pdf_logxref("load old xref format\n");

    error = fz_readline(xref->file, buf, cap);
    if (error)
        return fz_rethrow(error, "cannot read xref marker");
    if (strcmp(buf, "xref") != 0)
        return fz_throw("cannot find xref marker");

    while (1)
    {
        c = fz_peekbyte(xref->file);
        if (!(c >= '0' && c <= '9'))
            break;

        error = fz_readline(xref->file, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot read xref count");

        s = buf;
        ofs = atoi(strsep(&s, " "));
        len = atoi(strsep(&s, " "));

        /* broken pdfs where the section is not on a separate line */
        if (s && *s != '\0')
        {
            fz_warn("broken xref section. proceeding anyway.");
            error = fz_seek(xref->file, -(n + buf - s + 2), 1);
            if (error)
                return fz_rethrow(error, "cannot seek to xref");
        }

        for (i = 0; i < len; i++)
        {
            error = fz_read(&n, xref->file, buf, 20);
            if (error)
                return fz_rethrow(error, "cannot read xref table");
            if (!xref->table[ofs + i].type)
            {
                s = buf;
                xref->table[ofs + i].ofs = atoi(s);
                xref->table[ofs + i].gen = atoi(s + 11);
                xref->table[ofs + i].type = s[17];
            }
        }
    }

    error = pdf_lex(&t, xref->file, buf, cap, &n);
    if (error)
        return fz_rethrow(error, "cannot parse trailer");
    if (t != PDF_TTRAILER)
        return fz_throw("expected trailer marker");

    error = pdf_lex(&t, xref->file, buf, cap, &n);
    if (error)
        return fz_rethrow(error, "cannot parse trailer");
    if (t != PDF_TODICT)
        return fz_throw("expected trailer dictionary");

    error = pdf_parsedict(trailerp, xref->file, buf, cap);
    if (error)
        return fz_rethrow(error, "cannot parse trailer");
    return fz_okay;
}

static fz_error *
readnewxref(fz_obj **trailerp, pdf_xref *xref, char *buf, int cap)
{
    fz_error *error;
    fz_stream *stm;
    fz_obj *trailer;
    fz_obj *obj;
    int oid, gen, stmofs;
    int size, w0, w1, w2, i0, i1;
    int i, n;

    pdf_logxref("load new xref format\n");

    error = pdf_parseindobj(&trailer, xref->file, buf, cap, &oid, &gen, &stmofs);
    if (error)
        return fz_rethrow(error, "cannot parse compressed xref stream object");

    if (oid < 0 || oid >= xref->len)
    {
        error = fz_throw("object id out of range");
        goto cleanup;
    }

    xref->table[oid].type = 'n';
    xref->table[oid].gen = gen;
    xref->table[oid].obj = fz_keepobj(trailer);
    xref->table[oid].stmofs = stmofs;

    obj = fz_dictgets(trailer, "Size");
    if (!obj)
    {
        error = fz_throw("xref stream missing Size entry");
        goto cleanup;
    }
    size = fz_toint(obj);

    obj = fz_dictgets(trailer, "W");
    if (!obj) {
        error = fz_throw("xref stream missing W entry");
        goto cleanup;
    }
    w0 = fz_toint(fz_arrayget(obj, 0));
    w1 = fz_toint(fz_arrayget(obj, 1));
    w2 = fz_toint(fz_arrayget(obj, 2));

    obj = fz_dictgets(trailer, "Index");
    if (obj) {
        i0 = fz_toint(fz_arrayget(obj, 0));
        i1 = fz_toint(fz_arrayget(obj, 1));
    }
    else {
        i0 = 0;
        i1 = size;
    }

    if (i0 < 0 || i1 > xref->len) {
        error = fz_throw("xref stream has too many entries");
        goto cleanup;
    }

    error = pdf_openstream(&stm, xref, oid, gen);
    if (error)
    {
        error = fz_rethrow(error, "cannot open compressed xref stream");
        goto cleanup;
    }

    for (i = i0; i < i0 + i1; i++)
    {
        int a = 0;
        int b = 0;
        int c = 0;

        if (fz_peekbyte(stm) == EOF)
        {
            error = fz_readerror(stm);
            if (error)
                error = fz_rethrow(error, "truncated xref stream");
            else
                error = fz_throw("truncated xref stream");
            fz_dropstream(stm);
            goto cleanup;
        }

        for (n = 0; n < w0; n++)
            a = (a << 8) + fz_readbyte(stm);
        for (n = 0; n < w1; n++)
            b = (b << 8) + fz_readbyte(stm);
        for (n = 0; n < w2; n++)
            c = (c << 8) + fz_readbyte(stm);

        error = fz_readerror(stm);
        if (error)
        {
            error = fz_rethrow(error, "truncated xref stream");
            fz_dropstream(stm);
            goto cleanup;
        }

        if (!xref->table[i].type)
        {
            int t = w0 ? a : 1;
            xref->table[i].type = t == 0 ? 'f' : t == 1 ? 'n' : t == 2 ? 'o' : 0;
            xref->table[i].ofs = w2 ? b : 0;
            xref->table[i].gen = w1 ? c : 0;
        }
    }

    fz_dropstream(stm);

    *trailerp = trailer;

    return nil;

cleanup:
    fz_dropobj(trailer);
    return error;
}

static fz_error *
readxref(fz_obj **trailerp, pdf_xref *xref, int ofs, char *buf, int cap)
{
    fz_error *error;
    int c;

    error = fz_seek(xref->file, ofs, 0);
    if (error)
        return fz_rethrow(error, "cannot seek to xref");

    c = fz_peekbyte(xref->file);
    error = fz_readerror(xref->file);
    if (error)
        return fz_rethrow(error, "cannot read trailer");

    if (c == 'x')
    {
        error = readoldxref(trailerp, xref, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot read xref (ofs=%d)", ofs);
    }
    else if (c >= '0' && c <= '9')
    {
        error = readnewxref(trailerp, xref, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot read xref (ofs=%d)", ofs);
    }
    else
    {
        return fz_throw("cannot recognize xref format");
    }

    return fz_okay;
}

static fz_error *
readxrefsections(pdf_xref *xref, int ofs, char *buf, int cap)
{
    fz_error *error;
    fz_obj *trailer;
    fz_obj *prev;
    fz_obj *xrefstm;

    error = readxref(&trailer, xref, ofs, buf, cap);
    if (error)
        return fz_rethrow(error, "cannot read xref section");

    /* FIXME: do we overwrite free entries properly? */
    xrefstm = fz_dictgets(trailer, "XrefStm");
    if (xrefstm)
    {
        pdf_logxref("load xrefstm\n");
        error = readxrefsections(xref, fz_toint(xrefstm), buf, cap);
        if (error)
        {
            error = fz_rethrow(error, "cannot read /XrefStm xref section");
            goto cleanup;
        }
    }

    prev = fz_dictgets(trailer, "Prev");
    if (prev)
    {
        pdf_logxref("load prev\n");
        error = readxrefsections(xref, fz_toint(prev), buf, cap);
        if (error)
        {
            error = fz_rethrow(error, "cannot read /Prev xref section");
            goto cleanup;
        }
    }

    fz_dropobj(trailer);
    return nil;

cleanup:
    fz_dropobj(trailer);
    return error;
}

/*
 * compressed object streams
 */

fz_error *
pdf_loadobjstm(pdf_xref *xref, int oid, int gen, char *buf, int cap)
{
    fz_error *error;
    fz_stream *stm;
    fz_obj *objstm;
    int *oidbuf;
    int *ofsbuf;

    fz_obj *obj;
    int first;
    int count;
    int i, n, t;

    pdf_logxref("loadobjstm %d %d\n", oid, gen);

    error = pdf_loadobject(&objstm, xref, oid, gen);
    if (error)
        return fz_rethrow(error, "cannot load object stream object");

    count = fz_toint(fz_dictgets(objstm, "N"));
    first = fz_toint(fz_dictgets(objstm, "First"));

    pdf_logxref("  count %d\n", count);

    oidbuf = fz_malloc(count * sizeof(int));
    if (!oidbuf)
    {
        error = fz_throw("outofmem: object id buffer");
        goto cleanupobj;
    }

    ofsbuf = fz_malloc(count * sizeof(int));
    if (!ofsbuf)
    {
        error = fz_throw("outofmem: offset buffer");
        goto cleanupoid;
    }

    error = pdf_openstream(&stm, xref, oid, gen);
    if (error)
    {
        error = fz_rethrow(error, "cannot open object stream");
        goto cleanupofs;
    }

    for (i = 0; i < count; i++)
    {
        error = pdf_lex(&t, stm, buf, cap, &n);
        if (error || t != PDF_TINT)
        {
            error = fz_rethrow(error, "corrupt object stream");
            goto cleanupstm;
        }
        oidbuf[i] = atoi(buf);

        error = pdf_lex(&t, stm, buf, cap, &n);
        if (error || t != PDF_TINT)
        {
            error = fz_rethrow(error, "corrupt object stream");
            goto cleanupstm;
        }
        ofsbuf[i] = atoi(buf);
    }

    error = fz_seek(stm, first, 0);
    if (error)
    {
        error = fz_rethrow(error, "cannot seek in object stream");
        goto cleanupstm;
    }

    for (i = 0; i < count; i++)
    {
        /* FIXME: seek to first + ofsbuf[i] */

        error = pdf_parsestmobj(&obj, stm, buf, cap);
        if (error)
        {
            error = fz_rethrow(error, "cannot parse object %d in stream", i);
            goto cleanupstm;
        }

        if (oidbuf[i] < 1 || oidbuf[i] >= xref->len)
        {
            error = fz_throw("object id out of range (%d)", oidbuf[i]);
            goto cleanupstm;
        }

        if (xref->table[oidbuf[i]].obj)
            fz_dropobj(xref->table[oidbuf[i]].obj);
        xref->table[oidbuf[i]].obj = obj;
    }

    fz_dropstream(stm);
    fz_free(ofsbuf);
    fz_free(oidbuf);
    fz_dropobj(objstm);
    return nil;

cleanupstm:
    fz_dropstream(stm);
cleanupofs:
    fz_free(ofsbuf);
cleanupoid:
    fz_free(oidbuf);
cleanupobj:
    fz_dropobj(objstm);
    return error;
}

/*
 * open and load xref tables from pdf
 */

fz_error *
pdf_loadxref(pdf_xref *xref, char *filename)
{
    fz_error *error;
    fz_obj *size;
    int i;

    char buf[65536];	/* yeowch! */

    pdf_logxref("loadxref '%s' %p\n", filename, xref);

    error = fz_openrfile(&xref->file, filename);
    if (error)
        return fz_rethrow(error, "cannot open file: '%s'", filename);

    error = loadversion(xref);
    if (error)
        return fz_rethrow(error, "cannot read version marker");

    error = readstartxref(xref);
    if (error)
        return fz_rethrow(error, "cannot read startxref");

    error = readtrailer(xref, buf, sizeof buf);
    if (error)
        return fz_rethrow(error, "cannot read trailer");

    size = fz_dictgets(xref->trailer, "Size");
    if (!size)
        return fz_throw("trailer missing Size entry");

    pdf_logxref("  size %d\n", fz_toint(size));

    assert(xref->table == nil);

    xref->cap = fz_toint(size);
    xref->len = fz_toint(size);
    xref->table = fz_malloc(xref->cap * sizeof(pdf_xrefentry));
    if (!xref->table)
        return fz_throw("outofmem: xref table");

    for (i = 0; i < xref->len; i++)
    {
        xref->table[i].ofs = 0;
        xref->table[i].gen = 0;
        xref->table[i].type = 0;
        xref->table[i].mark = 0;
        xref->table[i].stmbuf = nil;
        xref->table[i].stmofs = 0;
        xref->table[i].obj = nil;
    }

    error = readxrefsections(xref, xref->startxref, buf, sizeof buf);
    if (error)
        return fz_rethrow(error, "cannot read xref");

    return fz_okay;
}

