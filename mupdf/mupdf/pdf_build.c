#include "fitz.h"
#include "mupdf.h"
#include <ft2build.h>
#include FT_FREETYPE_H

void
pdf_initgstate(pdf_gstate *gs, fz_matrix ctm)
{
	gs->ctm = ctm;
	gs->clipdepth = 0;

	gs->strokestate.linewidth = 1;
	gs->strokestate.linecap = 0;
	gs->strokestate.linejoin = 0;
	gs->strokestate.miterlimit = 10;
	gs->strokestate.dashphase = 0;
	gs->strokestate.dashlen = 0;
	memset(gs->strokestate.dashlist, 0, sizeof(gs->strokestate.dashlist));

	gs->stroke.kind = PDF_MCOLOR;
	gs->stroke.cs = fz_keepcolorspace(fz_devicegray);
	gs->stroke.v[0] = 0;
	gs->stroke.pattern = nil;
	gs->stroke.shade = nil;
	gs->stroke.alpha = 1;

	gs->fill.kind = PDF_MCOLOR;
	gs->fill.cs = fz_keepcolorspace(fz_devicegray);
	gs->fill.v[0] = 0;
	gs->fill.pattern = nil;
	gs->fill.shade = nil;
	gs->fill.alpha = 1;

	gs->charspace = 0;
	gs->wordspace = 0;
	gs->scale = 1;
	gs->leading = 0;
	gs->font = nil;
	gs->size = -1;
	gs->render = 0;
	gs->rise = 0;

	gs->blendmode = FZ_BNORMAL;
	gs->softmask = nil;
	gs->softmaskctm = fz_identity;
	gs->luminosity = 0;
}

void
pdf_setcolorspace(pdf_csi *csi, int what, fz_colorspace *cs)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;

	pdf_flushtext(csi);

	mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

	fz_dropcolorspace(mat->cs);

	mat->kind = PDF_MCOLOR;
	mat->cs = fz_keepcolorspace(cs);

	mat->v[0] = 0;
	mat->v[1] = 0;
	mat->v[2] = 0;
	mat->v[3] = 1;
}

void
pdf_setcolor(pdf_csi *csi, int what, float *v)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;
	int i;

	pdf_flushtext(csi);

	mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

	switch (mat->kind)
	{
	case PDF_MPATTERN:
	case PDF_MCOLOR:
		if (!strcmp(mat->cs->name, "Lab"))
		{
			mat->v[0] = v[0] / 100;
			mat->v[1] = (v[1] + 100) / 200;
			mat->v[2] = (v[2] + 100) / 200;
		}
		for (i = 0; i < mat->cs->n; i++)
			mat->v[i] = v[i];
		break;
	default:
		fz_warn("color incompatible with material");
	}
}

static void
pdf_unsetpattern(pdf_csi *csi, int what)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;
	mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;
	if (mat->kind == PDF_MPATTERN)
	{
		if (mat->pattern)
			pdf_droppattern(mat->pattern);
		mat->pattern = nil;
		mat->kind = PDF_MCOLOR;
	}
}

void
pdf_setpattern(pdf_csi *csi, int what, pdf_pattern *pat, float *v)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;

	pdf_flushtext(csi);

	mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

	if (mat->pattern)
		pdf_droppattern(mat->pattern);

	mat->kind = PDF_MPATTERN;
	if (pat)
		mat->pattern = pdf_keeppattern(pat);
	else
		mat->pattern = nil;

	if (v)
		pdf_setcolor(csi, what, v);
}

void
pdf_setshade(pdf_csi *csi, int what, fz_shade *shade)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;

	pdf_flushtext(csi);

	mat = what == PDF_MFILL ? &gs->fill : &gs->stroke;

	if (mat->shade)
		fz_dropshade(mat->shade);

	mat->kind = PDF_MSHADE;
	mat->shade = fz_keepshade(shade);
}

void
pdf_showpattern(pdf_csi *csi, pdf_pattern *pat, fz_rect bbox, int what)
{
	pdf_gstate *gstate;
	fz_matrix ptm, invptm;
	fz_matrix oldtopctm;
	fz_error error;
	int x, y, x0, y0, x1, y1;
	int oldtop;

	pdf_gsave(csi);
	gstate = csi->gstate + csi->gtop;

	if (pat->ismask)
	{
		pdf_unsetpattern(csi, PDF_MFILL);
		pdf_unsetpattern(csi, PDF_MSTROKE);
		if (what == PDF_MFILL)
		{
			pdf_dropmaterial(&gstate->stroke);
			pdf_keepmaterial(&gstate->fill);
			gstate->stroke = gstate->fill;
		}
		if (what == PDF_MSTROKE)
		{
			pdf_dropmaterial(&gstate->fill);
			pdf_keepmaterial(&gstate->stroke);
			gstate->fill = gstate->stroke;
		}
	}
	else
	{
		// TODO: unset only the current fill/stroke or both?
		pdf_unsetpattern(csi, what);
	}

	ptm = fz_concat(pat->matrix, csi->topctm);
	invptm = fz_invertmatrix(ptm);

	/* patterns are painted using the ctm in effect at the beginning of the content stream */
	/* get bbox of shape in pattern space for stamping */
	bbox = fz_transformrect(invptm, bbox);
	x0 = floorf(bbox.x0 / pat->xstep);
	y0 = floorf(bbox.y0 / pat->ystep);
	x1 = ceilf(bbox.x1 / pat->xstep);
	y1 = ceilf(bbox.y1 / pat->ystep);

	oldtopctm = csi->topctm;
	oldtop = csi->gtop;

	for (y = y0; y < y1; y++)
	{
		for (x = x0; x < x1; x++)
		{
			gstate->ctm = fz_concat(fz_translate(x * pat->xstep, y * pat->ystep), ptm);
			csi->topctm = gstate->ctm;
			error = pdf_runcsibuffer(csi, pat->resources, pat->contents);
			while (oldtop < csi->gtop)
				pdf_grestore(csi);
			if (error)
			{
				fz_catch(error, "cannot render pattern tile");
				goto cleanup;
			}
		}
	}

cleanup:
	csi->topctm = oldtopctm;

	pdf_grestore(csi);
}

void
pdf_showshade(pdf_csi *csi, fz_shade *shd)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_rect bbox;

	bbox = fz_boundshade(shd, gstate->ctm);

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->begingroup(csi->dev->user, bbox, 0, 0, gstate->blendmode, 1);

	csi->dev->fillshade(csi->dev->user, shd, gstate->ctm, gstate->fill.alpha);

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->endgroup(csi->dev->user);
}

void
pdf_showimage(pdf_csi *csi, fz_pixmap *image)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_rect bbox;

	bbox = fz_transformrect(gstate->ctm, fz_unitrect);

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->begingroup(csi->dev->user, bbox, 0, 0, gstate->blendmode, 1);

	if (image->mask)
		csi->dev->clipimagemask(csi->dev->user, image->mask, gstate->ctm);

	if (!image->colorspace)
	{

		switch (gstate->fill.kind)
		{
		case PDF_MNONE:
			break;
		case PDF_MCOLOR:
			csi->dev->fillimagemask(csi->dev->user, image, gstate->ctm,
				gstate->fill.cs, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MPATTERN:
			if (gstate->fill.pattern)
			{
				csi->dev->clipimagemask(csi->dev->user, image, gstate->ctm);
				pdf_showpattern(csi, gstate->fill.pattern, bbox, PDF_MFILL);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		case PDF_MSHADE:
			if (gstate->fill.shade)
			{
				csi->dev->clipimagemask(csi->dev->user, image, gstate->ctm);
				csi->dev->fillshade(csi->dev->user, gstate->fill.shade, gstate->ctm, gstate->fill.alpha);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		}
	}
	else
	{
		csi->dev->fillimage(csi->dev->user, image, gstate->ctm, gstate->fill.alpha);
	}

	if (image->mask)
		csi->dev->popclip(csi->dev->user);

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->endgroup(csi->dev->user);
}

void
pdf_showpath(pdf_csi *csi, int doclose, int dofill, int dostroke, int evenodd)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_path *path;
	fz_rect bbox;

	path = csi->path;
	csi->path = fz_newpath();

	if (doclose)
		fz_closepath(path);

	if (csi->clip)
	{
		gstate->clipdepth++;
		csi->dev->clippath(csi->dev->user, path, evenodd, gstate->ctm);
		csi->clip = 0;
	}

	if (dostroke)
		bbox = fz_boundpath(path, &gstate->strokestate, gstate->ctm);
	else
		bbox = fz_boundpath(path, nil, gstate->ctm);

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->begingroup(csi->dev->user, bbox, 0, 0, gstate->blendmode, 1);

	if (dofill)
	{
		switch (gstate->fill.kind)
		{
		case PDF_MNONE:
			break;
		case PDF_MCOLOR:
			csi->dev->fillpath(csi->dev->user, path, evenodd, gstate->ctm,
				gstate->fill.cs, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MPATTERN:
			if (gstate->fill.pattern)
			{
				csi->dev->clippath(csi->dev->user, path, evenodd, gstate->ctm);
				pdf_showpattern(csi, gstate->fill.pattern, bbox, PDF_MFILL);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		case PDF_MSHADE:
			if (gstate->fill.shade)
			{
				csi->dev->clippath(csi->dev->user, path, evenodd, gstate->ctm);
				csi->dev->fillshade(csi->dev->user, gstate->fill.shade, csi->topctm, gstate->fill.alpha);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		}
	}

	if (dostroke)
	{
		switch (gstate->stroke.kind)
		{
		case PDF_MNONE:
			break;
		case PDF_MCOLOR:
			csi->dev->strokepath(csi->dev->user, path, &gstate->strokestate, gstate->ctm,
				gstate->stroke.cs, gstate->stroke.v, gstate->stroke.alpha);
			break;
		case PDF_MPATTERN:
			if (gstate->stroke.pattern)
			{
				csi->dev->clipstrokepath(csi->dev->user, path, &gstate->strokestate, gstate->ctm);
				pdf_showpattern(csi, gstate->stroke.pattern, bbox, PDF_MFILL);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		case PDF_MSHADE:
			if (gstate->stroke.shade)
			{
				csi->dev->clipstrokepath(csi->dev->user, path, &gstate->strokestate, gstate->ctm);
				csi->dev->fillshade(csi->dev->user, gstate->stroke.shade, csi->topctm, gstate->stroke.alpha);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		}
	}

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->endgroup(csi->dev->user);

	fz_freepath(path);
}

void
pdf_flushtext(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_text *text;
	int dofill = 0;
	int dostroke = 0;
	int doclip = 0;
	int doinvisible = 0;
	fz_rect bbox;

	if (!csi->text)
		return;
	text = csi->text;
	csi->text = nil;

	dofill = dostroke = doclip = doinvisible = 0;
	switch (csi->textmode)
	{
	case 0: dofill = 1; break;
	case 1: dostroke = 1; break;
	case 2: dofill = dostroke = 1; break;
	case 3: doinvisible = 1; break;
	case 4: dofill = doclip = 1; break;
	case 5: dostroke = doclip = 1; break;
	case 6: dofill = dostroke = doclip = 1; break;
	case 7: doclip = 1; break;
	}

	bbox = fz_boundtext(text, gstate->ctm);

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->begingroup(csi->dev->user, bbox, 0, 0, gstate->blendmode, 1);

	if (doinvisible)
		csi->dev->ignoretext(csi->dev->user, text, gstate->ctm);

	if (doclip)
	{
		gstate->clipdepth++;
		csi->dev->cliptext(csi->dev->user, text, gstate->ctm, csi->accumulate);
		csi->accumulate = 2;
	}

	if (dofill)
	{
		switch (gstate->fill.kind)
		{
		case PDF_MNONE:
			break;
		case PDF_MCOLOR:
			csi->dev->filltext(csi->dev->user, text, gstate->ctm,
				gstate->fill.cs, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MPATTERN:
			if (gstate->fill.pattern)
			{
				csi->dev->cliptext(csi->dev->user, text, gstate->ctm, 0);
				pdf_showpattern(csi, gstate->fill.pattern, bbox, PDF_MFILL);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		case PDF_MSHADE:
			if (gstate->fill.shade)
			{
				csi->dev->cliptext(csi->dev->user, text, gstate->ctm, 0);
				csi->dev->fillshade(csi->dev->user, gstate->fill.shade, csi->topctm, gstate->fill.alpha);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		}
	}

	if (dostroke)
	{
		switch (gstate->stroke.kind)
		{
		case PDF_MNONE:
			break;
		case PDF_MCOLOR:
			csi->dev->stroketext(csi->dev->user, text, &gstate->strokestate, gstate->ctm,
				gstate->stroke.cs, gstate->stroke.v, gstate->stroke.alpha);
			break;
		case PDF_MPATTERN:
			if (gstate->stroke.pattern)
			{
				csi->dev->clipstroketext(csi->dev->user, text, &gstate->strokestate, gstate->ctm);
				pdf_showpattern(csi, gstate->stroke.pattern, bbox, PDF_MFILL);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		case PDF_MSHADE:
			if (gstate->stroke.shade)
			{
				csi->dev->clipstroketext(csi->dev->user, text, &gstate->strokestate, gstate->ctm);
				csi->dev->fillshade(csi->dev->user, gstate->stroke.shade, csi->topctm, gstate->stroke.alpha);
				csi->dev->popclip(csi->dev->user);
			}
			break;
		}
	}

	if (gstate->blendmode != FZ_BNORMAL)
		csi->dev->endgroup(csi->dev->user);

	fz_freetext(text);
}

static void
pdf_showglyph(pdf_csi *csi, int cid)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	pdf_fontdesc *fontdesc = gstate->font;
	fz_matrix tsm, trm;
	float w0, w1, tx, ty;
	pdf_hmtx h;
	pdf_vmtx v;
	int gid;
	int ucsbuf[8];
	int ucslen;
	int i;

	tsm.a = gstate->size * gstate->scale;
	tsm.b = 0;
	tsm.c = 0;
	tsm.d = gstate->size;
	tsm.e = 0;
	tsm.f = gstate->rise;

	ucslen = 0;
	if (fontdesc->tounicode)
		ucslen = pdf_lookupcmapfull(fontdesc->tounicode, cid, ucsbuf);
	if (ucslen == 0 && cid < fontdesc->ncidtoucs)
	{
		ucsbuf[0] = fontdesc->cidtoucs[cid];
		ucslen = 1;
	}
	if (ucslen == 0 || (ucslen == 1 && ucsbuf[0] == 0))
	{
		ucsbuf[0] = '?';
		ucslen = 1;
	}

	gid = pdf_fontcidtogid(fontdesc, cid);
	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=855 */
	/* some chinese fonts only ship the similarly looking 0x2026 */
	if (gid == 0 && ucsbuf[0] == 0x22ef && fontdesc->font->ftface)
	{
		gid = FT_Get_Char_Index(fontdesc->font->ftface, 0x2026);
	}

	if (fontdesc->wmode == 1)
	{
		v = pdf_getvmtx(fontdesc, cid);
		tsm.e -= v.x * gstate->size * 0.001f;
		tsm.f -= v.y * gstate->size * 0.001f;
	}

	trm = fz_concat(tsm, csi->tm);

	/* flush buffered text if face or matrix or rendermode has changed */
	if (!csi->text ||
		fontdesc->font != csi->text->font ||
		fontdesc->wmode != csi->text->wmode ||
		fabsf(trm.a - csi->text->trm.a) > FLT_EPSILON ||
		fabsf(trm.b - csi->text->trm.b) > FLT_EPSILON ||
		fabsf(trm.c - csi->text->trm.c) > FLT_EPSILON ||
		fabsf(trm.d - csi->text->trm.d) > FLT_EPSILON ||
		gstate->render != csi->textmode)
	{
		pdf_flushtext(csi);

		csi->text = fz_newtext(fontdesc->font, trm, fontdesc->wmode);
		csi->text->trm.e = 0;
		csi->text->trm.f = 0;
		csi->textmode = gstate->render;
	}

	/* add glyph to textobject */
	fz_addtext(csi->text, gid, ucsbuf[0], trm.e, trm.f);

	/* add filler glyphs for one-to-many unicode mapping */
	for (i = 1; i < ucslen; i++)
		fz_addtext(csi->text, -1, ucsbuf[i], trm.e, trm.f);

	if (fontdesc->wmode == 0)
	{
		h = pdf_gethmtx(fontdesc, cid);
		w0 = h.w * 0.001f;
		tx = (w0 * gstate->size + gstate->charspace) * gstate->scale;
		csi->tm = fz_concat(fz_translate(tx, 0), csi->tm);
	}

	if (fontdesc->wmode == 1)
	{
		w1 = v.w * 0.001f;
		ty = w1 * gstate->size + gstate->charspace;
		csi->tm = fz_concat(fz_translate(0, ty), csi->tm);
	}
}

static void
pdf_showspace(pdf_csi *csi, float tadj)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	pdf_fontdesc *fontdesc = gstate->font;
	if (fontdesc->wmode == 0)
		csi->tm = fz_concat(fz_translate(tadj * gstate->scale, 0), csi->tm);
	else
		csi->tm = fz_concat(fz_translate(0, tadj), csi->tm);
}

void
pdf_showtext(pdf_csi *csi, fz_obj *text)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	pdf_fontdesc *fontdesc = gstate->font;
	unsigned char *buf;
	unsigned char *end;
	int i, len;
	int cpt, cid;

	if (!fontdesc)
	{
		fz_warn("cannot draw text since font and size not set");
		return;
	}

	if (fz_isarray(text))
	{
		for (i = 0; i < fz_arraylen(text); i++)
		{
			fz_obj *item = fz_arrayget(text, i);
			if (fz_isstring(item))
				pdf_showtext(csi, item);
			else
				pdf_showspace(csi, - fz_toreal(item) * gstate->size * 0.001f);
		}
	}

	if (fz_isstring(text))
	{
		buf = (unsigned char *)fz_tostrbuf(text);
		len = fz_tostrlen(text);
		end = buf + len;

		while (buf < end)
		{
			buf = pdf_decodecmap(fontdesc->encoding, buf, &cpt);
			cid = pdf_lookupcmap(fontdesc->encoding, cpt);
			if (cid >= 0)
				pdf_showglyph(csi, cid);
			else
				fz_warn("cannot encode character with code point %#x", cpt);
			if (cpt == 32)
				pdf_showspace(csi, gstate->wordspace);
		}
	}
}
