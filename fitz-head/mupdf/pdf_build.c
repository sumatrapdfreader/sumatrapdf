#include "fitz.h"
#include "mupdf.h"

void
pdf_initgstate(pdf_gstate *gs)
{
    gs->linewidth = 1.0;
    gs->linecap = 0;
    gs->linejoin = 0;
    gs->miterlimit = 10;
    gs->dashphase = 0;
    gs->dashlen = 0;
    memset(gs->dashlist, 0, sizeof(gs->dashlist));

    gs->stroke.kind = PDF_MCOLOR;
    gs->stroke.cs = fz_keepcolorspace(pdf_devicegray);
    gs->stroke.v[0] = 0;
    gs->stroke.indexed = nil;
    gs->stroke.pattern = nil;
    gs->stroke.shade = nil;

    gs->fill.kind = PDF_MCOLOR;
    gs->fill.cs = fz_keepcolorspace(pdf_devicegray);
    gs->fill.v[0] = 0;
    gs->fill.indexed = nil;
    gs->fill.pattern = nil;
    gs->fill.shade = nil;

    gs->charspace = 0;
    gs->wordspace = 0;
    gs->scale = 1;
    gs->leading = 0;
    gs->font = nil;
    gs->size = -1;
    gs->render = 0;
    gs->rise = 0;

    gs->head = nil;
}

fz_error *
pdf_setcolorspace(pdf_csi *csi, int what, fz_colorspace *cs)
{
    pdf_gstate *gs = csi->gstate + csi->gtop;
    fz_error *error;
    pdf_material *mat;

    error = pdf_flushtext(csi);
    if (error)
        return fz_rethrow(error, "cannot finish text node (state change)");

    mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

    fz_dropcolorspace(mat->cs);

    mat->kind = PDF_MCOLOR;
    mat->cs = fz_keepcolorspace(cs);

    mat->v[0] = 0;	/* FIXME: default color */
    mat->v[1] = 0;	/* FIXME: default color */
    mat->v[2] = 0;	/* FIXME: default color */
    mat->v[3] = 1;	/* FIXME: default color */

    if (!strcmp(cs->name, "Indexed"))
    {
        mat->kind = PDF_MINDEXED;
        mat->indexed = (pdf_indexed*)cs;
        mat->cs = mat->indexed->base;
    }

    if (!strcmp(cs->name, "Lab"))
        mat->kind = PDF_MLAB;

    return fz_okay;
}

fz_error *
pdf_setcolor(pdf_csi *csi, int what, float *v)
{
    pdf_gstate *gs = csi->gstate + csi->gtop;
    fz_error *error;
    pdf_indexed *ind;
    pdf_material *mat;
    int i, k;

    error = pdf_flushtext(csi);
    if (error)
        return fz_rethrow(error, "cannot finish text node (state change)");

    mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

    switch (mat->kind)
    {
    case PDF_MPATTERN:
        if (!strcmp(mat->cs->name, "Lab"))
            goto Llab;
        if (!strcmp(mat->cs->name, "Indexed"))
            goto Lindexed;
        /* fall through */

    case PDF_MCOLOR:
        for (i = 0; i < mat->cs->n; i++)
            mat->v[i] = v[i];
        break;

    case PDF_MLAB:
Llab:
        mat->v[0] = v[0] / 100.0;
        mat->v[1] = (v[1] + 100) / 200.0;
        mat->v[2] = (v[2] + 100) / 200.0;
        break;

    case PDF_MINDEXED:
Lindexed:
        ind = mat->indexed;
        i = CLAMP(v[0], 0, ind->high);
        for (k = 0; k < ind->base->n; k++)
            mat->v[k] = ind->lookup[ind->base->n * i + k] / 255.0;
        break;

    default:
        return fz_throw("color incompatible with material");
    }

    return fz_okay;
}

fz_error *
pdf_setpattern(pdf_csi *csi, int what, pdf_pattern *pat, float *v)
{
    pdf_gstate *gs = csi->gstate + csi->gtop;
    fz_error *error;
    pdf_material *mat;

    error = pdf_flushtext(csi);
    if (error)
        return fz_rethrow(error, "cannot finish text node (state change)");

    mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

    if (mat->pattern)
        pdf_droppattern(mat->pattern);

    mat->kind = PDF_MPATTERN;
    if (pat)
        mat->pattern = pdf_keeppattern(pat);
    else
        mat->pattern = nil;

    if (v)
    {
        error = pdf_setcolor(csi, what, v);
        if (error)
            return fz_rethrow(error, "cannot set color");
    }

    return fz_okay;
}

fz_error *
pdf_setshade(pdf_csi *csi, int what, fz_shade *shade)
{
    pdf_gstate *gs = csi->gstate + csi->gtop;
    fz_error *error;
    pdf_material *mat;

    error = pdf_flushtext(csi);
    if (error)
        return fz_rethrow(error, "cannot finish text node (state change)");

    mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

    if (mat->shade)
        fz_dropshade(mat->shade);

    mat->kind = PDF_MSHADE;
    mat->shade = fz_keepshade(shade);

    return fz_okay;
}

fz_error *
pdf_buildstrokepath(pdf_gstate *gs, fz_pathnode *path)
{
    fz_error *error;
    fz_stroke stroke;
    fz_dash *dash;

    stroke.linecap = gs->linecap;
    stroke.linejoin = gs->linejoin;
    stroke.linewidth = gs->linewidth;
    stroke.miterlimit = gs->miterlimit;

    if (gs->dashlen)
    {
        error = fz_newdash(&dash, gs->dashphase, gs->dashlen, gs->dashlist);
        if (error)
            return fz_rethrow(error, "cannot create dash pattern");
    }
    else
        dash = nil;

    error = fz_endpath(path, FZ_STROKE, &stroke, dash);
    if (error)
    {
        fz_dropdash(dash);
        return fz_rethrow(error, "cannot finish path node");
    }

    return fz_okay;
}

fz_error *
pdf_buildfillpath(pdf_gstate *gs, fz_pathnode *path, int eofill)
{
    fz_error *error;
    error = fz_endpath(path, eofill ? FZ_EOFILL : FZ_FILL, nil, nil);
    if (error)
        return fz_rethrow(error, "cannot finish path node");
    return fz_okay;
}

static fz_error *
addcolorshape(pdf_gstate *gs, fz_node *shape, fz_colorspace *cs, float *v)
{
    fz_error *error;
    fz_node *mask;
    fz_node *solid;

    error = fz_newmasknode(&mask);
    if (error)
        return fz_rethrow(error, "cannot create mask node");

    error = fz_newsolidnode(&solid, cs, cs->n, 0, v);
    if (error)
    {
        fz_dropnode(mask);
        return fz_rethrow(error, "cannot create color node");
    }

    fz_insertnodelast(mask, shape);
    fz_insertnodelast(mask, solid);
    fz_insertnodelast(gs->head, mask);

    return fz_okay;
}

static fz_error *
addinvisibleshape(pdf_gstate *gs, fz_node *shape)
{
    fz_error *error;
    fz_node *mask;
    fz_pathnode *path;

    error = fz_newmasknode(&mask);
    if (error)
        return fz_rethrow(error, "cannot create mask node");

    error = fz_newpathnode(&path);
    if (error)
    {
        fz_dropnode(mask);
        return fz_rethrow(error, "cannot create path node");
    }

    error = fz_endpath(path, FZ_FILL, nil, nil);
    if (error)
    {
        fz_dropnode(mask);
        fz_dropnode((fz_node*)path);
        return fz_rethrow(error, "cannot finish path node");
    }

    fz_insertnodelast(mask, (fz_node*)path);
    fz_insertnodelast(mask, shape);
    fz_insertnodelast(gs->head, mask);

    return fz_okay;
}

static fz_matrix getmatrix(fz_node *node)
{
    if (node->parent)
    {
        fz_matrix ptm = getmatrix(node->parent);
        if (fz_istransformnode(node))
            return fz_concat(((fz_transformnode*)node)->m, ptm);
        return ptm;
    }
    if (fz_istransformnode(node))
        return ((fz_transformnode*)node)->m;
    return fz_identity();
}

static fz_error *
addpatternshape(pdf_gstate *gs, fz_node *shape,
	pdf_pattern *pat, fz_colorspace *cs, float *v)
{
    fz_error *error;
    fz_node *xform;
    fz_node *over;
    fz_node *mask;
    fz_node *link;
    fz_matrix ctm;
    fz_matrix inv;
    fz_matrix ptm;
    fz_rect bbox;
    int x, y, x0, y0, x1, y1;

    /* patterns are painted in user space */
    ctm = getmatrix(gs->head);
    inv = fz_invertmatrix(ctm);

    error = fz_newmasknode(&mask);
    if (error)
        return fz_rethrow(error, "cannot create mask node");

    ptm = fz_concat(pat->matrix, fz_invertmatrix(ctm));
    error = fz_newtransformnode(&xform, ptm);
    if (error)
    {
        fz_dropnode(mask);
        return fz_rethrow(error, "cannot create transform node");
    }

    error = fz_newovernode(&over);
    if (error)
    {
        fz_dropnode(xform);
        fz_dropnode(mask);
        return fz_rethrow(error, "cannot create over node");
    }

    fz_insertnodelast(mask, shape);
    fz_insertnodelast(mask, xform);
    fz_insertnodelast(xform, over);
    xform = nil;

    /* over, xform, mask are now owned by the tree */

    /* get bbox of shape in pattern space for stamping */
    ptm = fz_concat(ctm, fz_invertmatrix(pat->matrix));
    bbox = fz_boundnode(shape, ptm);

    /* expand bbox by pattern bbox */
    bbox.x0 += pat->bbox.x0;
    bbox.y0 += pat->bbox.y0;
    bbox.x1 += pat->bbox.x1;
    bbox.y1 += pat->bbox.y1;

    x0 = fz_floor(bbox.x0 / pat->xstep);
    y0 = fz_floor(bbox.y0 / pat->ystep);
    x1 = fz_ceil(bbox.x1 / pat->xstep);
    y1 = fz_ceil(bbox.y1 / pat->ystep);

    for (y = y0; y <= y1; y++)
    {
        for (x = x0; x <= x1; x++)
        {
            ptm = fz_translate(x * pat->xstep, y * pat->ystep);
            error = fz_newtransformnode(&xform, ptm);
            if (error)
                return fz_rethrow(error, "cannot create transform node for stamp");
            error = fz_newlinknode(&link, pat->tree);
            if (error)
            {
                fz_dropnode(xform);
                return fz_rethrow(error, "cannot create link node for stamp");
            }
            fz_insertnodelast(xform, link);
            fz_insertnodelast(over, xform);
        }
    }

    if (pat->ismask)
    {
        error = addcolorshape(gs, mask, cs, v);
        if (error)
            return fz_rethrow(error, "cannot add colored shape");
        return fz_okay;
    }

    fz_insertnodelast(gs->head, mask);
    return fz_okay;
}

fz_error *
pdf_addshade(pdf_gstate *gs, fz_shade *shade)
{
    fz_error *error;
    fz_node *node;

    error = fz_newshadenode(&node, shade);
    if (error)
        return fz_rethrow(error, "cannot create shade node");

    fz_insertnodelast(gs->head, node);

    return fz_okay;
}

static fz_error *
addshadeshape(pdf_gstate *gs, fz_node *shape, fz_shade *shade)
{
    fz_error *error;
    fz_node *mask;
    fz_node *color;
    fz_node *xform;
    fz_node *over;
    fz_node *bgnd;
    fz_matrix ctm;
    fz_matrix inv;

    ctm = getmatrix(gs->head);
    inv = fz_invertmatrix(ctm);

    error = fz_newtransformnode(&xform, inv);
    if (error)
        return fz_rethrow(error, "cannot create transform node");

    error = fz_newmasknode(&mask);
    if (error)
    {
        fz_dropnode(xform);
        return fz_rethrow(error, "cannot create mask node");
    }

    error = fz_newshadenode(&color, shade);
    if (error)
    {
        fz_dropnode(mask);
        fz_dropnode(xform);
        return fz_rethrow(error, "cannot create shade node");
    }

    if (shade->usebackground)
    {
        error = fz_newovernode(&over);
        if (error)
        {
            fz_dropnode(color);
            fz_dropnode(mask);
            fz_dropnode(xform);
            return fz_rethrow(error, "cannot create over node for background color");
        }

        error = fz_newsolidnode(&bgnd, shade->cs, shade->cs->n, 0, shade->background);
        if (error)
        {
            fz_dropnode(over);
            fz_dropnode(color);
            fz_dropnode(mask);
            fz_dropnode(xform);
            return fz_rethrow(error, "cannot create solid node for background color");;
        }

        fz_insertnodelast(mask, shape);
        fz_insertnodelast(over, bgnd);
        fz_insertnodelast(over, color);
        fz_insertnodelast(xform, over);
        fz_insertnodelast(mask, xform);
        fz_insertnodelast(gs->head, mask);
    }
    else
    {
        fz_insertnodelast(mask, shape);
        fz_insertnodelast(xform, color);
        fz_insertnodelast(mask, xform);
        fz_insertnodelast(gs->head, mask);
    }

    return fz_okay;
}

fz_error *
pdf_addfillshape(pdf_gstate *gs, fz_node *shape)
{
    fz_error *error;

    switch (gs->fill.kind)
    {
    case PDF_MNONE:
        fz_insertnodelast(gs->head, shape);
        break;

    case PDF_MCOLOR:
    case PDF_MLAB:
    case PDF_MINDEXED:
        error = addcolorshape(gs, shape, gs->fill.cs, gs->fill.v);
        if (error)
            return fz_rethrow(error, "cannot add colored shape");
        break;

    case PDF_MPATTERN:
        error = addpatternshape(gs, shape, gs->fill.pattern, gs->fill.cs, gs->fill.v);
        if (error)
            return fz_rethrow(error, "cannot add pattern shape");
        break;

    case PDF_MSHADE:
        error = addshadeshape(gs, shape, gs->fill.shade);
        if (error)
            return fz_rethrow(error, "cannot add shade shape");
        break;

    default:
        return fz_throw("assert: unknown material");
    }

    return fz_okay;
}

fz_error *
pdf_addstrokeshape(pdf_gstate *gs, fz_node *shape)
{
    fz_error *error;

    switch (gs->stroke.kind)
    {
    case PDF_MNONE:
        fz_insertnodelast(gs->head, shape);
        break;

    case PDF_MCOLOR:
    case PDF_MLAB:
    case PDF_MINDEXED:
        error = addcolorshape(gs, shape, gs->stroke.cs, gs->stroke.v);
        if (error)
            return fz_rethrow(error, "cannot add colored shape");
        break;

    case PDF_MPATTERN:
        error = addpatternshape(gs, shape, gs->stroke.pattern, gs->stroke.cs, gs->stroke.v);
        if (error)
            return fz_rethrow(error, "cannot add pattern shape");
        break;

    case PDF_MSHADE:
        error = addshadeshape(gs, shape, gs->stroke.shade);
        if (error)
            return fz_rethrow(error, "cannot add shade shape");
        break;

    default:
        return fz_throw("assert: unknown material");
    }

    return fz_okay;
}

fz_error *
pdf_addclipmask(pdf_gstate *gs, fz_node *shape)
{
    fz_error *error;
    fz_node *mask;
    fz_node *over;

    error = fz_newmasknode(&mask);
    if (error)
        return fz_rethrow(error, "cannot create mask node");

    error = fz_newovernode(&over);
    if (error)
    {
        fz_dropnode(mask);
        return fz_rethrow(error, "cannot create over node");
    }

    fz_insertnodelast(mask, shape);
    fz_insertnodelast(mask, over);
    fz_insertnodelast(gs->head, mask);
    gs->head = over;

    return fz_okay;
}

fz_error *
pdf_addtransform(pdf_gstate *gs, fz_node *transform)
{
    fz_error *error;
    fz_node *over;

    error = fz_newovernode(&over);
    if (error)
        return fz_rethrow(error, "cannot create over node");

    fz_insertnodelast(gs->head, transform);
    fz_insertnodelast(transform, over);
    gs->head = over;

    return fz_okay;
}

fz_error *
pdf_showimage(pdf_csi *csi, pdf_image *img)
{
    fz_error *error;
    fz_node *mask;
    fz_node *color;
    fz_node *shape;

    error = fz_newimagenode(&color, (fz_image*)img);
    if (error)
        return fz_rethrow(error, "cannot create image node");

    if (img->super.n == 0 && img->super.a == 1)
    {
        error = pdf_addfillshape(csi->gstate + csi->gtop, color);
        if (error)
        {
            fz_dropnode(color);
            return fz_rethrow(error, "cannot add filled image mask");
        }
    }

    else
    {
        if (img->mask)
        {
            error = fz_newimagenode(&shape, (fz_image*)img->mask);
            if (error)
            {
                fz_dropnode(color);
                return fz_rethrow(error, "cannot create image node for image mask");
            }
            error = fz_newmasknode(&mask);
            if (error)
            {
                fz_dropnode(shape);
                fz_dropnode(color);
                return fz_rethrow(error, "cannot create mask node for image mask");
            }
            fz_insertnodelast(mask, shape);
            fz_insertnodelast(mask, color);
            fz_insertnodelast(csi->gstate[csi->gtop].head, mask);
        }
        else
        {
            fz_insertnodelast(csi->gstate[csi->gtop].head, color);
        }
    }

    return fz_okay;
}

fz_error *
pdf_showpath(pdf_csi *csi,
        int doclose, int dofill, int dostroke, int evenodd)
{
    pdf_gstate *gstate = csi->gstate + csi->gtop;
    fz_error *error;
    fz_pathnode *spath;
    fz_pathnode *fpath;
    fz_pathnode *clip;

    /* TODO this is too messy and hairy with memory cleanups */

    if (doclose)
    {
        error = fz_closepath(csi->path);
        if (error)
            return fz_rethrow(error, "cannot create path node");
    }

    /*
     * Prepare the various copies of the path node.
     */

    if (csi->clip)
    {
        error = fz_clonepathnode(&clip, csi->path);
        if (error)
            return error;
    }

    if (dofill && dostroke)
    {
        fpath = csi->path;
        error = fz_clonepathnode(&spath, fpath);
        if (error)
            return fz_rethrow(error, "cannot duplicate path node");
    }
    else if (dofill)
    {
        fpath = csi->path;
        spath = nil;
    }
    else if (dostroke)
    {
        fpath = nil;
        spath = csi->path;
    }
    else
    {
        fz_dropnode((fz_node*)csi->path);
        spath = nil;
        fpath = nil;
    }

    csi->path = nil;

    /*
     * Add nodes to the tree.
     */

    if (dofill)
    {
        error = pdf_buildfillpath(gstate, fpath, evenodd);
        if (error)
        {
            if (fpath) fz_dropnode((fz_node*)fpath);
            if (spath) fz_dropnode((fz_node*)spath);
            if (clip) fz_dropnode((fz_node*)clip);
            return fz_rethrow(error, "cannot finish fill path");
        }

        error = pdf_addfillshape(gstate, (fz_node*)fpath);
        if (error)
        {
            if (fpath) fz_dropnode((fz_node*)fpath);
            if (spath) fz_dropnode((fz_node*)spath);
            if (clip) fz_dropnode((fz_node*)clip);
            return fz_rethrow(error, "cannot add filled path");
        }
    }

    if (dostroke)
    {
        error = pdf_buildstrokepath(gstate, spath);
        if (error)
        {
            if (fpath) fz_dropnode((fz_node*)fpath);
            if (spath) fz_dropnode((fz_node*)spath);
            if (clip) fz_dropnode((fz_node*)clip);
            return fz_rethrow(error, "cannot finish stroke path");
        }

        error = pdf_addstrokeshape(gstate, (fz_node*)spath);
        if (error)
        {
            if (fpath) fz_dropnode((fz_node*)fpath);
            if (spath) fz_dropnode((fz_node*)spath);
            if (clip) fz_dropnode((fz_node*)clip);
            return fz_rethrow(error, "cannot add stroked path");
        }
    }

    if (csi->clip)
    {
        error = fz_endpath(clip, FZ_FILL, nil, nil);
        if (error)
        {
            fz_dropnode((fz_node*)clip);
            return fz_rethrow(error, "cannot finish clip path");
        }

        error = pdf_addclipmask(gstate, (fz_node*)clip);
        if (error)
        {
            fz_dropnode((fz_node*)clip);
            return fz_rethrow(error, "cannot add clip mask");
        }

        csi->clip = 0;
    }

    error = fz_newpathnode(&csi->path);
    if (error)
        return fz_rethrow(error, "cannot create path node");;

    return fz_okay;
}

/*
 * Text
 */

fz_error *
pdf_flushtext(pdf_csi *csi)
{
    pdf_gstate *gstate = csi->gstate + csi->gtop;
    fz_error *error;

    if (csi->text)
    {
        switch (csi->textmode)
        {
        case 0:	/* fill */
        case 1:	/* stroke */
        case 2:	/* stroke + fill */
            error = pdf_addfillshape(gstate, (fz_node*)csi->text);
            if (error)
                return fz_rethrow(error, "cannot add filled text");
            break;

        case 3:	/* invisible */
            error = addinvisibleshape(gstate, (fz_node*)csi->text);
            if (error)
                return fz_rethrow(error, "cannot add invisible text");
            break;

        case 4: /* fill + clip */
        case 5: /* stroke + clip */
        case 6: /* stroke + fill + clip */
            {
                fz_textnode *temp;

                error = fz_clonetextnode(&temp, csi->text);
                if (error)
                    return fz_rethrow(error, "cannot duplicate text");

                error = pdf_addfillshape(gstate, (fz_node*)temp);
                if (error)
                {
                    fz_dropnode((fz_node*)temp);
                    return fz_rethrow(error, "cannot add filled text");
                }

                /* FIXME stroked text */
            }
            /* fall through */

        case 7: /* invisible clip ( + fallthrough clips ) */
            if (!csi->textclip)
            {
                error = fz_newovernode(&csi->textclip);
                if (error)
                    return fz_rethrow(error, "cannot create over node");
            }
            fz_insertnodelast(csi->textclip, (fz_node*)csi->text);
            break;
        }

        csi->text = nil;
    }

    return fz_okay;
}

fz_error *
showglyph(pdf_csi *csi, int cid)
{
    pdf_gstate *gstate = csi->gstate + csi->gtop;
    pdf_font *font = gstate->font;
    fz_error *error;
    fz_matrix tsm, trm;
    float w0, w1, tx, ty;
    fz_hmtx h;
    fz_vmtx v;

    tsm.a = gstate->size * gstate->scale;
    tsm.b = 0;
    tsm.c = 0;
    tsm.d = gstate->size;
    tsm.e = 0;
    tsm.f = gstate->rise;

    if (font->super.wmode == 1)
    {
        v = fz_getvmtx((fz_font*)font, cid);
        tsm.e -= v.x * gstate->size / 1000.0;
        tsm.f -= v.y * gstate->size / 1000.0;
    }

    trm = fz_concat(tsm, csi->tm);

    /* flush buffered text if face or matrix or rendermode has changed */
    if (!csi->text ||
            ((fz_font*)font) != csi->text->font ||
            fabs(trm.a - csi->text->trm.a) > FLT_EPSILON ||
            fabs(trm.b - csi->text->trm.b) > FLT_EPSILON ||
            fabs(trm.c - csi->text->trm.c) > FLT_EPSILON ||
            fabs(trm.d - csi->text->trm.d) > FLT_EPSILON ||
            gstate->render != csi->textmode)
    {
        error = pdf_flushtext(csi);
        if (error)
            return fz_rethrow(error, "cannot finish text node (face/matrix change)");

        error = fz_newtextnode(&csi->text, (fz_font*)font);
        if (error)
            return fz_rethrow(error, "cannot create text node");

        csi->text->trm = trm;
        csi->text->trm.e = 0;
        csi->text->trm.f = 0;
        csi->textmode = gstate->render;
    }

    /* add glyph to textobject */
    error = fz_addtext(csi->text, cid, trm.e, trm.f);
    if (error)
        return fz_rethrow(error, "cannot add glyph to text node");

    if (font->super.wmode == 0)
    {
        h = fz_gethmtx((fz_font*)font, cid);
        w0 = h.w / 1000.0;
        tx = (w0 * gstate->size + gstate->charspace) * gstate->scale;
        csi->tm = fz_concat(fz_translate(tx, 0), csi->tm);
    }
    else
    {
        w1 = v.w / 1000.0;
        ty = w1 * gstate->size + gstate->charspace;
        csi->tm = fz_concat(fz_translate(0, ty), csi->tm);
    }

    return fz_okay;
}

void
showspace(pdf_csi *csi, float tadj)
{
    pdf_gstate *gstate = csi->gstate + csi->gtop;
    pdf_font *font = gstate->font;
    if (font->super.wmode == 0)
        csi->tm = fz_concat(fz_translate(tadj * gstate->scale, 0), csi->tm);
    else
        csi->tm = fz_concat(fz_translate(0, tadj), csi->tm);
}

fz_error *
pdf_showtext(pdf_csi *csi, fz_obj *text)
{
    pdf_gstate *gstate = csi->gstate + csi->gtop;
    pdf_font *font = gstate->font;
    fz_error *error;
    unsigned char *buf;
    unsigned char *end;
    int i, len;
    int cpt, cid;

    if (fz_isarray(text))
    {
        for (i = 0; i < fz_arraylen(text); i++)
        {
            fz_obj *item = fz_arrayget(text, i);
            if (fz_isstring(item))
            {
                error = pdf_showtext(csi, item);
                if (error)
                    return fz_rethrow(error, "cannot draw text item");
            }
            else
            {
                showspace(csi, - fz_toreal(item) * gstate->size / 1000.0);
            }
        }
        return nil;
    }

    buf = fz_tostrbuf(text);
    len = fz_tostrlen(text);
    end = buf + len;

    while (buf < end)
    {
        buf = pdf_decodecmap(font->encoding, buf, &cpt);
        cid = pdf_lookupcmap(font->encoding, cpt);
        if (cid == -1)
            cid = 0;

        error = showglyph(csi, cid);
        if (error)
            return fz_rethrow(error, "cannot draw glyph");

        if (cpt == 32)
            showspace(csi, gstate->wordspace);
    }

    return fz_okay;
}

