#include "fitz.h"
#include "mupdf.h"

static fz_error *
runone(pdf_csi *csi, pdf_xref *xref, fz_obj *rdb, fz_obj *stmref)
{
    fz_error *error;
    fz_stream *stm;

    pdf_logpage("simple content stream\n");

    error = pdf_openstream(&stm, xref, fz_tonum(stmref), fz_togen(stmref));
    if (error)
        return fz_rethrow(error, "cannot open content stream %d", fz_tonum(stmref));

    error = pdf_runcsi(csi, xref, rdb, stm);

    fz_dropstream(stm);

    if (error)
        return fz_rethrow(error, "cannot interpret content stream %d", fz_tonum(stmref));

    return fz_okay;
}

/* we need to combine all sub-streams into one for pdf_runcsi
 * to deal with split dictionaries etc.
 */
static fz_error *
runmany(pdf_csi *csi, pdf_xref *xref, fz_obj *rdb, fz_obj *list)
{
    fz_error *error;
    fz_stream *file;
    fz_buffer *big;
    fz_buffer *one;
    fz_obj *stm;
    int i;

    pdf_logpage("multiple content streams: %d\n", fz_arraylen(list));

    error = fz_newbuffer(&big, 32 * 1024);
    if (error)
        return fz_rethrow(error, "cannot create content buffer");

    error = fz_openwbuffer(&file, big);
    if (error)
    {
        error = fz_rethrow(error, "cannot open content buffer (write)");
        goto cleanupbuf;
    }

    for (i = 0; i < fz_arraylen(list); i++)
    {
        /* TODO dont use loadstream here */

        stm = fz_arrayget(list, i);
        error = pdf_loadstream(&one, xref, fz_tonum(stm), fz_togen(stm));
        if (error)
        {
            error = fz_rethrow(error, "cannot load content stream");
            goto cleanupstm;
        }

        error = fz_write(file, one->rp, one->wp - one->rp);
        if (error)
        {
            fz_dropbuffer(one);
            error = fz_rethrow(error, "cannot write to content buffer");
            goto cleanupstm;
        }

        fz_dropbuffer(one);

        fz_printstr(file, " ");
        if (error)
        {
            error = fz_rethrow(error, "cannot write to content buffer");
            goto cleanupstm;
        }
    }

    fz_dropstream(file);

    error = fz_openrbuffer(&file, big);
    if (error)
    {
        error = fz_rethrow(error, "cannot open content buffer (read)");
        goto cleanupbuf;
    }

    error = pdf_runcsi(csi, xref, rdb, file);
    if (error)
    {
        error = fz_rethrow(error, "cannot interpret content buffer");
        goto cleanupstm;
    }

    fz_dropstream(file);
    fz_dropbuffer(big);
    return fz_okay;

cleanupstm:
    fz_dropstream(file);
cleanupbuf:
    fz_dropbuffer(big);
    return error;
}

static fz_error *
loadpagecontents(fz_tree **treep, pdf_xref *xref, fz_obj *rdb, fz_obj *ref)
{
    fz_error *error;
    fz_obj *obj;
    pdf_csi *csi;

    error = pdf_newcsi(&csi, 0);
    if (error)
        return fz_rethrow(error, "cannot create interpreter");

    if (fz_isindirect(ref))
    {
        error = pdf_loadindirect(&obj, xref, ref);
        if (error)
            return fz_rethrow(error, "cannot load page contents (%d)", fz_tonum(ref));

        if (fz_isarray(obj))
        {
            if (fz_arraylen(obj) == 1)
                error = runone(csi, xref, rdb, fz_arrayget(obj, 0));
            else
                error = runmany(csi, xref, rdb, obj);
        }
        else
            error = runone(csi, xref, rdb, ref);

        fz_dropobj(obj);

        if (error)
        {
            pdf_dropcsi(csi);
            return fz_rethrow(error, "cannot interpret page contents (%d)", fz_tonum(ref));
        }
    }

    else if (fz_isarray(ref))
    {
        if (fz_arraylen(ref) == 1)
            error = runone(csi, xref, rdb, fz_arrayget(ref, 0));
        else
            error = runmany(csi, xref, rdb, ref);

        if (error)
        {
            pdf_dropcsi(csi);
            return fz_rethrow(error, "cannot interpret page contents (%d)", fz_tonum(ref));
        }
    }

    *treep = csi->tree;
    csi->tree = nil;

    pdf_dropcsi(csi);

    return fz_okay;
}

fz_error *
pdf_getpageinfo(fz_obj *dict, fz_rect *bboxp, int *rotatep)
{
    fz_rect bbox;
    int rotate;
    fz_obj *obj;

    obj = fz_dictgets(dict, "CropBox");
    if (!obj)
        obj = fz_dictgets(dict, "MediaBox");
    if (!fz_isarray(obj))
        return fz_throw("cannot find page bounds");
    bbox = pdf_torect(obj);

    pdf_logpage("bbox [%g %g %g %g]\n",
            bbox.x0, bbox.y0, bbox.x1, bbox.y1);

    obj = fz_dictgets(dict, "Rotate");
    if (fz_isint(obj))
        rotate = fz_toint(obj);
    else
        rotate = 0;

    pdf_logpage("rotate %d\n", rotate);

    if (bboxp)
        *bboxp = bbox;
    if (rotatep)
        *rotatep = rotate;
    return nil;
}

fz_error *
pdf_loadpage(pdf_page **pagep, pdf_xref *xref, fz_obj *dict)
{
    fz_error *error;
    fz_obj *obj;
    pdf_page *page;
    fz_obj *rdb;
    pdf_comment *comments = nil;
    pdf_link *links = nil;
    fz_tree *tree;
    fz_rect bbox;
    int rotate;

    pdf_logpage("load page {\n");

    /*
     * Sort out page media
     */
    error = pdf_getpageinfo(dict, &bbox, &rotate);
    if (error)
        return error;

    /*
     * Load annotations
     */

    obj = fz_dictgets(dict, "Annots");
    if (obj)
    {
        error = pdf_resolve(&obj, xref);
        if (error)
            return fz_rethrow(error, "cannot resolve annotations");
        error = pdf_loadannots(&comments, &links, xref, obj);
        fz_dropobj(obj);
        if (error)
            return fz_rethrow(error, "cannot load annotations");
    }

    /*
     * Load resources
     */

    obj = fz_dictgets(dict, "Resources");
    if (!obj)
        return fz_throw("cannot find page resources");
    error = pdf_resolve(&obj, xref);
    if (error)
        return fz_rethrow(error, "cannot resolve page resources");
    error = pdf_loadresources(&rdb, xref, obj);
    fz_dropobj(obj);
    if (error)
        return fz_rethrow(error, "cannot load page resources");

    /*
     * Interpret content stream to build display tree
     */

    obj = fz_dictgets(dict, "Contents");

    error = loadpagecontents(&tree, xref, rdb, obj);
    if (error)
    {
        fz_dropobj(rdb);
        return fz_rethrow(error, "cannot load page contents");
    }

    pdf_logpage("optimize tree\n");
    error = fz_optimizetree(tree);
    if (error)
    {
        fz_dropobj(rdb);
        return fz_rethrow(error, "cannot optimize page display tree");
    }

    /*
     * Create page object
     */

    page = fz_malloc(sizeof(pdf_page));
    if (!page)
    {
        fz_droptree(tree);
        fz_dropobj(rdb);
        return fz_throw("outofmem: page struct");
    }

    page->mediabox.x0 = MIN(bbox.x0, bbox.x1);
    page->mediabox.y0 = MIN(bbox.y0, bbox.y1);
    page->mediabox.x1 = MAX(bbox.x0, bbox.x1);
    page->mediabox.y1 = MAX(bbox.y0, bbox.y1);
    page->rotate = rotate;
    page->resources = rdb;
    page->tree = tree;

    page->comments = comments;
    page->links = links;

    pdf_logpage("} %p\n", page);

    *pagep = page;
    return fz_okay;
}

void
pdf_droppage(pdf_page *page)
{
    pdf_logpage("drop page %p\n", page);
    /* if (page->comments) pdf_dropcomment(page->comments); */
    if (page->links)
        pdf_droplink(page->links);
    fz_dropobj(page->resources);
    fz_droptree(page->tree);
    fz_free(page);
}

