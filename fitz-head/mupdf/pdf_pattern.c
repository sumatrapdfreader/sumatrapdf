#include "fitz.h"
#include "mupdf.h"

pdf_pattern *
pdf_keeppattern(pdf_pattern *pat)
{
    pat->refs ++;
    return pat;
}

void
pdf_droppattern(pdf_pattern *pat)
{
    if (--pat->refs == 0)
    {
        if (pat->tree)
            fz_droptree(pat->tree);
        fz_free(pat);
    }
}

fz_error *
pdf_loadpattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *dict, fz_obj *stmref)
{
    fz_error *error;
    pdf_pattern *pat;
    fz_stream *stm;
    fz_obj *resources;
    fz_obj *obj;
    pdf_csi *csi;

    if ((*patp = pdf_finditem(xref->store, PDF_KPATTERN, stmref)))
    {
        pdf_keeppattern(*patp);
        return fz_okay;
    }

    pdf_logrsrc("load pattern %d %d {\n", fz_tonum(stmref), fz_togen(stmref));

    pat = fz_malloc(sizeof(pdf_pattern));
    if (!pat)
        return fz_throw("outofmem: pattern struct");

    pat->tree = nil;
    pat->ismask = fz_toint(fz_dictgets(dict, "PaintType")) == 2;
    pat->xstep = fz_toreal(fz_dictgets(dict, "XStep"));
    pat->ystep = fz_toreal(fz_dictgets(dict, "YStep"));

    pdf_logrsrc("mask %d\n", pat->ismask);
    pdf_logrsrc("xstep %g\n", pat->xstep);
    pdf_logrsrc("ystep %g\n", pat->ystep);

    obj = fz_dictgets(dict, "BBox");
    pat->bbox = pdf_torect(obj);

    pdf_logrsrc("bbox [%g %g %g %g]\n",
            pat->bbox.x0, pat->bbox.y0,
            pat->bbox.x1, pat->bbox.y1);

    obj = fz_dictgets(dict, "Matrix");
    if (obj)
        pat->matrix = pdf_tomatrix(obj);
    else
        pat->matrix = fz_identity();

    pdf_logrsrc("matrix [%g %g %g %g %g %g]\n",
            pat->matrix.a, pat->matrix.b,
            pat->matrix.c, pat->matrix.d,
            pat->matrix.e, pat->matrix.f);

    /*
     * Resources
     */

    obj = fz_dictgets(dict, "Resources");
    if (!obj) {
        error = fz_throw("cannot find Resources dictionary");
        goto cleanup;
    }

    error = pdf_resolve(&obj, xref);
    if (error)
    {
        error = fz_rethrow(error, "cannot resolve resource dictionary");
        goto cleanup;
    }

    error = pdf_loadresources(&resources, xref, obj);

    fz_dropobj(obj);

    if (error)
    {
        error = fz_rethrow(error, "cannot load resources");
        goto cleanup;
    }

    /*
     * Content stream
     */

    pdf_logrsrc("content stream\n");

    error = pdf_newcsi(&csi, pat->ismask);
    if (error)
    {
        error = fz_rethrow(error, "cannot create interpreter");
        goto cleanup;
    }

    error = pdf_openstream(&stm, xref, fz_tonum(stmref), fz_togen(stmref));
    if (error)
    {
        error = fz_rethrow(error, "cannot open pattern stream %d", fz_tonum(stmref));
        goto cleanupcsi;
    }

    error = pdf_runcsi(csi, xref, resources, stm);

    fz_dropstream(stm);

    if (error)
    {
        error = fz_rethrow(error, "cannot interpret pattern stream %d", fz_tonum(stmref));
        goto cleanupcsi;
    }

    pat->tree = csi->tree;
    csi->tree = nil;

    pdf_dropcsi(csi);

    fz_dropobj(resources);

    pdf_logrsrc("optimize tree\n");
    error = fz_optimizetree(pat->tree);
    if (error)
    {
        error = fz_rethrow(error, "cannot optimize pattern tree");
        goto cleanup;
    }

    pdf_logrsrc("}\n");

    error = pdf_storeitem(xref->store, PDF_KPATTERN, stmref, pat);
    if (error)
    {
        error = fz_rethrow(error, "cannot store pattern resource");
        goto cleanup;
    }

    *patp = pat;
    return fz_okay;

cleanupcsi:
    pdf_dropcsi(csi);
cleanup:
    pdf_droppattern(pat);
    return error;
}

