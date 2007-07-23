#include "fitz.h"
#include "mupdf.h"

/*
 * open pdf and scan objects to reconstruct xref table
 */

struct entry
{
    int oid;
    int gen;
    int ofs;
    int stmofs;
    int stmlen;
};

static fz_error *
parseobj(fz_stream *file, char *buf, int cap, int *stmofs, int *stmlen,
	int *isroot, int *isinfo)
{
    fz_error *error;
    fz_obj *dict = nil;
    fz_obj *length;
    fz_obj *filter;
    fz_obj *type;
    int tok, len;
    int n;

    *stmlen = -1;
    *isroot = 0;
    *isinfo = 0;

    error = pdf_lex(&tok, file, buf, cap, &len);
    if (tok == PDF_TODICT)
    {
        error = pdf_parsedict(&dict, file, buf, cap);
        if (error)
            return fz_rethrow(error, "cannot parse object");

        type = fz_dictgets(dict, "Type");
        if (fz_isname(type) && !strcmp(fz_toname(type), "Catalog"))
            *isroot = 1;

        filter = fz_dictgets(dict, "Filter");
        if (fz_isname(filter) && !strcmp(fz_toname(filter), "Standard"))
        {
            fz_dropobj(dict);
            return fz_throw("cannot repair encrypted files");
        }

        if (fz_dictgets(dict, "Producer"))
            if (fz_dictgets(dict, "Creator"))
                if (fz_dictgets(dict, "Title"))
                    *isinfo = 1;

        fz_dropobj(dict);
    }

    while ( tok != PDF_TSTREAM &&
            tok != PDF_TENDOBJ &&
            tok != PDF_TERROR &&
            tok != PDF_TEOF )
    {
        error = pdf_lex(&tok, file, buf, cap, &len);
        if (error)
            return fz_rethrow(error, "cannot scan for endobj or stream token");
    }

    if (tok == PDF_TSTREAM)
    {
        int c = fz_readbyte(file);
        if (c == '\r') {
            c = fz_peekbyte(file);
            if (c == '\n')
                fz_readbyte(file);
        }

        error = fz_readerror(file);
        if (error)
            return fz_rethrow(error, "cannot read from file");

        *stmofs = fz_tell(file);
        if (*stmofs < 0)
            return fz_throw("cannot seek in file");

        length = fz_dictgets(dict, "Length");
        if (fz_isint(length))
        {
            error = fz_seek(file, *stmofs + fz_toint(length), 0);
            if (error)
                return fz_rethrow(error, "cannot seek in file");
            error = pdf_lex(&tok, file, buf, cap, &len);
            if (error)
                return fz_rethrow(error, "cannot scan for endstream token");
            if (tok == PDF_TENDSTREAM)
                goto atobjend;
            error = fz_seek(file, *stmofs, 0);
            if (error)
                return fz_rethrow(error, "cannot seek in file");
        }

        error = fz_read(&n, file, buf, 9);
        if (error)
            return fz_rethrow(error, "cannot read from file");

        while (memcmp(buf, "endstream", 9) != 0)
        {
            c = fz_readbyte(file);
            if (c == EOF)
                break;
            memmove(buf, buf + 1, 8);
            buf[8] = c;
        }

        error = fz_readerror(file);
        if (error)
            return fz_rethrow(error, "cannot read from file");

        *stmlen = fz_tell(file) - *stmofs - 9;

atobjend:
        error = pdf_lex(&tok, file, buf, cap, &len);
        if (error)
            return fz_rethrow(error, "cannot scan for endobj token");
        if (tok == PDF_TENDOBJ)
            ;
    }

    if (dict)
        fz_dropobj(dict);

    return fz_okay;
}

fz_error *
pdf_repairxref(pdf_xref *xref, char *filename)
{
    fz_error *error;
    fz_stream *file;

    struct entry *list = nil;
    int listlen;
    int listcap;
    int maxoid = 0;

    char buf[65536];

    int oid = 0;
    int gen = 0;
    int tmpofs, oidofs = 0, genofs = 0;
    int isroot, rootoid = 0, rootgen = 0;
    int isinfo, infooid = 0, infogen = 0;
    int stmofs, stmlen;
    int tok, len;
    int next;
    int i;

    error = fz_openrfile(&file, filename);
    if (error)
        return fz_rethrow(error, "cannot open file '%s'", filename);

    pdf_logxref("repairxref '%s' %p\n", filename, xref);

    xref->file = file;

    /* TODO: extract version */

    listlen = 0;
    listcap = 1024;
    list = fz_malloc(listcap * sizeof(struct entry));
    if (!list)
    {
        error = fz_throw("outofmem: reparation object list");
        goto cleanup;
    }

    while (1)
    {
        tmpofs = fz_tell(file);
        if (tmpofs < 0)
        {
            error = fz_throw("cannot tell in file");
            goto cleanup;
        }

        error = pdf_lex(&tok, file, buf, sizeof buf, &len);
        if (error)
        {
            error = fz_rethrow(error, "cannot scan for objects");
            goto cleanup;
        }

        if (tok == PDF_TINT)
        {
            oidofs = genofs;
            oid = gen;
            genofs = tmpofs;
            gen = atoi(buf);
        }

        if (tok == PDF_TOBJ)
        {
            error = parseobj(file, buf, sizeof buf, &stmofs, &stmlen, &isroot, &isinfo);
            if (error)
            {
                error = fz_rethrow(error, "cannot parse object");
                goto cleanup;
            }

            if (isroot) {
                pdf_logxref("found catalog: %d %d\n", oid, gen);
                rootoid = oid;
                rootgen = gen;
            }

            if (isinfo) {
                pdf_logxref("found info: %d %d\n", oid, gen);
                infooid = oid;
                infogen = gen;
            }

            if (listlen + 1 == listcap)
            {
                struct entry *newlist;
                listcap = listcap * 2;
                newlist = fz_realloc(list, listcap * sizeof(struct entry));
                if (!newlist) {
                    error = fz_throw("outofmem: resize reparation object list");
                    goto cleanup;
                }
                list = newlist;
            }

            list[listlen].oid = oid;
            list[listlen].gen = gen;
            list[listlen].ofs = oidofs;
            list[listlen].stmofs = stmofs;
            list[listlen].stmlen = stmlen;
            listlen ++;

            if (oid > maxoid)
                maxoid = oid;
        }

        if (tok == PDF_TEOF)
            break;
    }

    if (rootoid == 0)
    {
        error = fz_throw("cannot find catalog object");
        goto cleanup;
    }

    error = fz_packobj(&xref->trailer,
            "<< /Size %i /Root %r >>",
            maxoid + 1, rootoid, rootgen);
    if (error)
    {
        error = fz_rethrow(error, "cannot create new trailer");
        goto cleanup;
    }

    xref->len = maxoid + 1;
    xref->cap = xref->len;
    xref->table = fz_malloc(xref->cap * sizeof(pdf_xrefentry));
    if (!xref->table)
    {
        error = fz_throw("outofmem: xref table");
        goto cleanup;
    }

    xref->table[0].type = 'f';
    xref->table[0].mark = 0;
    xref->table[0].ofs = 0;
    xref->table[0].gen = 65535;
    xref->table[0].stmbuf = nil;
    xref->table[0].stmofs = 0;
    xref->table[0].obj = nil;

    for (i = 1; i < xref->len; i++)
    {
        xref->table[i].type = 'f';
        xref->table[i].mark = 0;
        xref->table[i].ofs = 0;
        xref->table[i].gen = 0;
        xref->table[i].stmbuf = nil;
        xref->table[i].stmofs = 0;
        xref->table[i].obj = nil;
    }

    for (i = 0; i < listlen; i++)
    {
        xref->table[list[i].oid].type = 'n';
        xref->table[list[i].oid].ofs = list[i].ofs;
        xref->table[list[i].oid].gen = list[i].gen;
        xref->table[list[i].oid].mark = 0;

        xref->table[list[i].oid].stmofs = list[i].stmofs;

        /* corrected stream length */
        if (list[i].stmlen >= 0)
        {
            fz_obj *dict, *length;

            pdf_logxref("correct stream length %d %d = %d\n",
                    list[i].oid, list[i].gen, list[i].stmlen);

            error = pdf_loadobject(&dict, xref, list[i].oid, list[i].gen);
            if (error)
            {
                error = fz_rethrow(error, "cannot load stream object");
                goto cleanup;
            }

            error = fz_newint(&length, list[i].stmlen);
            if (error)
            {
                fz_dropobj(dict);
                error = fz_rethrow(error, "cannot create integer object");
                goto cleanup;
            }

            error = fz_dictputs(dict, "Length", length);
            if (error)
            {
                fz_dropobj(length);
                fz_dropobj(dict);
                error = fz_rethrow(error, "cannot update stream length");
                goto cleanup;
            }

            error = pdf_updateobject(xref, list[i].oid, list[i].gen, dict);
            if (error)
            {
                fz_dropobj(length);
                fz_dropobj(dict);
                error = fz_rethrow(error, "cannot update stream object");
                goto cleanup;
            }

            fz_dropobj(length);
            fz_dropobj(dict);
        }
    }

    next = 0;
    for (i = xref->len - 1; i >= 0; i--)
    {
        if (xref->table[i].type == 'f')
        {
            xref->table[i].ofs = next;
            if (xref->table[i].gen < 65535)
                xref->table[i].gen ++;
            next = i;
        }
    }

    fz_free(list);
    return fz_okay;

cleanup:
    fz_dropstream(file);
    fz_free(list);
    return error;
}

