#include "fitz.h"
#include "mupdf.h"

static pdf_csi *
pdf_newcsi(pdf_xref *xref, fz_device *dev, fz_matrix ctm, char *target)
{
	pdf_csi *csi;

	csi = fz_malloc(sizeof(pdf_csi));
	csi->xref = xref;
	csi->dev = dev;
	csi->target = target;

	csi->top = 0;
	csi->obj = nil;
	csi->name[0] = 0;
	csi->stringlen = 0;
	memset(csi->stack, 0, sizeof csi->stack);

	csi->xbalance = 0;
	csi->intext = 0;
	csi->inarray = 0;

	csi->path = fz_newpath();
	csi->clip = 0;
	csi->clipevenodd = 0;

	csi->text = nil;
	csi->tlm = fz_identity;
	csi->tm = fz_identity;
	csi->textmode = 0;
	csi->accumulate = 1;

	csi->topctm = ctm;
	pdf_initgstate(&csi->gstate[0], ctm);
	csi->gtop = 0;

	return csi;
}

static void
pdf_clearstack(pdf_csi *csi)
{
	int i;

	if (csi->obj)
		fz_dropobj(csi->obj);
	csi->obj = nil;

	csi->name[0] = 0;
	csi->stringlen = 0;
	for (i = 0; i < csi->top; i++)
		csi->stack[i] = 0;

	csi->top = 0;
}

pdf_material *
pdf_keepmaterial(pdf_material *mat)
{
	if (mat->colorspace)
		fz_keepcolorspace(mat->colorspace);
	if (mat->pattern)
		pdf_keeppattern(mat->pattern);
	if (mat->shade)
		fz_keepshade(mat->shade);
	return mat;
}

pdf_material *
pdf_dropmaterial(pdf_material *mat)
{
	if (mat->colorspace)
		fz_dropcolorspace(mat->colorspace);
	if (mat->pattern)
		pdf_droppattern(mat->pattern);
	if (mat->shade)
		fz_dropshade(mat->shade);
	return mat;
}

void
pdf_gsave(pdf_csi *csi)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;

	if (csi->gtop == nelem(csi->gstate) - 1)
	{
		fz_warn("gstate overflow in content stream");
		return;
	}

	memcpy(&csi->gstate[csi->gtop + 1], &csi->gstate[csi->gtop], sizeof(pdf_gstate));

	csi->gtop ++;

	pdf_keepmaterial(&gs->stroke);
	pdf_keepmaterial(&gs->fill);
	if (gs->font)
		pdf_keepfont(gs->font);
	if (gs->softmask)
		pdf_keepxobject(gs->softmask);
}

void
pdf_grestore(pdf_csi *csi)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	int clipdepth = gs->clipdepth;

	if (csi->gtop == 0)
	{
		fz_warn("gstate underflow in content stream");
		return;
	}

	pdf_dropmaterial(&gs->stroke);
	pdf_dropmaterial(&gs->fill);
	if (gs->font)
		pdf_dropfont(gs->font);
	if (gs->softmask)
		pdf_dropxobject(gs->softmask);

	csi->gtop --;

	gs = csi->gstate + csi->gtop;
	while (clipdepth > gs->clipdepth)
	{
		csi->dev->popclip(csi->dev->user);
		clipdepth--;
	}
}

static void
pdf_freecsi(pdf_csi *csi)
{
	while (csi->gtop)
		pdf_grestore(csi);

	pdf_dropmaterial(&csi->gstate[0].fill);
	pdf_dropmaterial(&csi->gstate[0].stroke);
	if (csi->gstate[0].font)
		pdf_dropfont(csi->gstate[0].font);
	if (csi->gstate[0].softmask)
		pdf_dropxobject(csi->gstate[0].softmask);

	while (csi->gstate[0].clipdepth--)
		csi->dev->popclip(csi->dev->user);

	if (csi->path) fz_freepath(csi->path);
	if (csi->text) fz_freetext(csi->text);

	pdf_clearstack(csi);

	fz_free(csi);
}

static int
pdf_ishiddenocg(pdf_csi *csi, fz_obj *xobj)
{
	char target_state[16];
	fz_obj *obj;

	fz_strlcpy(target_state, csi->target, sizeof target_state);
	fz_strlcat(target_state, "State", sizeof target_state);

	obj = fz_dictgets(xobj, "OC");
	obj = fz_dictgets(obj, "OCGs");
	if (fz_isarray(obj))
		obj = fz_arrayget(obj, 0);
	obj = fz_dictgets(obj, "Usage");
	obj = fz_dictgets(obj, csi->target);
	obj = fz_dictgets(obj, target_state);
	return !strcmp(fz_toname(obj), "OFF");
}

fz_error
pdf_runxobject(pdf_csi *csi, fz_obj *resources, pdf_xobject *xobj, fz_matrix transform)
{
	fz_error error;
	pdf_gstate *gstate;
	fz_matrix oldtopctm;
	int oldtop;
	int popmask;

	pdf_gsave(csi);

	gstate = csi->gstate + csi->gtop;
	oldtop = csi->gtop;
	popmask = 0;

	/* apply xobject's transform matrix */
	transform = fz_concat(transform, xobj->matrix);
	gstate->ctm = fz_concat(transform, gstate->ctm);

	/* apply soft mask, create transparency group and reset state */
	if (xobj->transparency)
	{
		if (gstate->softmask)
		{
			pdf_xobject *softmask = gstate->softmask;
			fz_rect bbox = fz_transformrect(gstate->ctm, xobj->bbox);

			gstate->softmask = nil;
			popmask = 1;

			csi->dev->beginmask(csi->dev->user, bbox, gstate->luminosity,
				softmask->colorspace, gstate->softmaskbc);
			error = pdf_runxobject(csi, resources, softmask, fz_identity);
			if (error)
				return fz_rethrow(error, "cannot run softmask");
			csi->dev->endmask(csi->dev->user);

			pdf_dropxobject(softmask);
		}

		csi->dev->begingroup(csi->dev->user,
			fz_transformrect(gstate->ctm, xobj->bbox),
			xobj->isolated, xobj->knockout, gstate->blendmode, gstate->fill.alpha);

		gstate->blendmode = FZ_BNORMAL;
		gstate->stroke.alpha = 1;
		gstate->fill.alpha = 1;
	}

	/* clip to the bounds */

	fz_moveto(csi->path, xobj->bbox.x0, xobj->bbox.y0);
	fz_lineto(csi->path, xobj->bbox.x1, xobj->bbox.y0);
	fz_lineto(csi->path, xobj->bbox.x1, xobj->bbox.y1);
	fz_lineto(csi->path, xobj->bbox.x0, xobj->bbox.y1);
	fz_closepath(csi->path);
	csi->clip = 1;
	pdf_showpath(csi, 0, 0, 0, 0);

	/* run contents */

	oldtopctm = csi->topctm;
	csi->topctm = gstate->ctm;

	if (xobj->resources)
		resources = xobj->resources;

	error = pdf_runcsibuffer(csi, resources, xobj->contents);
	if (error)
		return fz_rethrow(error, "cannot interpret XObject stream");

	csi->topctm = oldtopctm;

	while (oldtop < csi->gtop)
		pdf_grestore(csi);

	pdf_grestore(csi);

	/* wrap up transparency stacks */

	if (xobj->transparency)
	{
		csi->dev->endgroup(csi->dev->user);
		if (popmask)
			csi->dev->popclip(csi->dev->user);
	}

	return fz_okay;
}

static fz_error
pdf_runinlineimage(pdf_csi *csi, fz_obj *rdb, fz_stream *file, fz_obj *dict)
{
	fz_error error;
	fz_pixmap *img;
	int ch;

	error = pdf_loadinlineimage(&img, csi->xref, rdb, dict, file);
	if (error)
		return fz_rethrow(error, "cannot load inline image");

	/* find EI */
	ch = fz_readbyte(file);
	while (ch != 'E' && ch != EOF)
		ch = fz_readbyte(file);
	ch = fz_readbyte(file);
	if (ch != 'I')
	{
		fz_droppixmap(img);
		return fz_rethrow(error, "syntax error after inline image");
	}

	pdf_showimage(csi, img);

	fz_droppixmap(img);
	return fz_okay;
}

static fz_error
pdf_runextgstate(pdf_csi *csi, fz_obj *rdb, fz_obj *extgstate)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_colorspace *colorspace;
	int i, k;

	pdf_flushtext(csi);

	for (i = 0; i < fz_dictlen(extgstate); i++)
	{
		fz_obj *key = fz_dictgetkey(extgstate, i);
		fz_obj *val = fz_dictgetval(extgstate, i);
		char *s = fz_toname(key);

		if (!strcmp(s, "Font"))
		{
			if (fz_isarray(val) && fz_arraylen(val) == 2)
			{
				fz_error error;
				fz_obj *font = fz_arrayget(val, 0);

				if (gstate->font)
				{
					pdf_dropfont(gstate->font);
					gstate->font = nil;
				}

				error = pdf_loadfont(&gstate->font, csi->xref, rdb, font);
				if (error)
					return fz_rethrow(error, "cannot load font (%d %d R)", fz_tonum(font), fz_togen(font));
				if (!gstate->font)
					return fz_throw("cannot find font in store");
				gstate->size = fz_toreal(fz_arrayget(val, 1));
			}
			else
				return fz_throw("malformed /Font dictionary");
		}

		else if (!strcmp(s, "LW"))
			gstate->strokestate.linewidth = fz_toreal(val);
		else if (!strcmp(s, "LC"))
			gstate->strokestate.linecap = fz_toint(val);
		else if (!strcmp(s, "LJ"))
			gstate->strokestate.linejoin = fz_toint(val);
		else if (!strcmp(s, "ML"))
			gstate->strokestate.miterlimit = fz_toreal(val);

		else if (!strcmp(s, "D"))
		{
			if (fz_isarray(val) && fz_arraylen(val) == 2)
			{
				fz_obj *dashes = fz_arrayget(val, 0);
				gstate->strokestate.dashlen = MAX(fz_arraylen(dashes), 32);
				for (k = 0; k < gstate->strokestate.dashlen; k++)
					gstate->strokestate.dashlist[k] = fz_toreal(fz_arrayget(dashes, k));
				gstate->strokestate.dashphase = fz_toreal(fz_arrayget(val, 1));
			}
			else
				return fz_throw("malformed /D");
		}

		else if (!strcmp(s, "CA"))
			gstate->stroke.alpha = fz_toreal(val);

		else if (!strcmp(s, "ca"))
			gstate->fill.alpha = fz_toreal(val);

		else if (!strcmp(s, "BM"))
		{
			if (fz_isarray(val))
				val = fz_arrayget(val, 0);

			gstate->blendmode = FZ_BNORMAL;
			for (k = 0; fz_blendnames[k]; k++)
				if (!strcmp(fz_blendnames[k], fz_toname(val)))
					gstate->blendmode = k;
		}

		else if (!strcmp(s, "SMask"))
		{
			if (fz_isdict(val))
			{
				fz_error error;
				pdf_xobject *xobj;
				fz_obj *group, *luminosity, *bc;

				if (gstate->softmask)
				{
					pdf_dropxobject(gstate->softmask);
					gstate->softmask = nil;
				}

				group = fz_dictgets(val, "G");
				if (!group)
					return fz_throw("cannot load softmask xobject (%d %d R)", fz_tonum(val), fz_togen(val));
				error = pdf_loadxobject(&xobj, csi->xref, group);
				if (error)
					return fz_rethrow(error, "cannot load xobject (%d %d R)", fz_tonum(val), fz_togen(val));

				colorspace = xobj->colorspace;
				if (!colorspace)
					colorspace = fz_devicegray;

				gstate->softmaskctm = fz_concat(xobj->matrix, gstate->ctm);
				gstate->softmask = xobj;
				for (k = 0; k < colorspace->n; k++)
					gstate->softmaskbc[k] = 0;

				bc = fz_dictgets(val, "BC");
				if (fz_isarray(bc))
				{
					for (k = 0; k < colorspace->n; k++)
						gstate->softmaskbc[k] = fz_toreal(fz_arrayget(bc, k));
				}

				luminosity = fz_dictgets(val, "S");
				if (fz_isname(luminosity) && !strcmp(fz_toname(luminosity), "Luminosity"))
					gstate->luminosity = 1;
				else
					gstate->luminosity = 0;
			}
			else if (fz_isname(val) && !strcmp(fz_toname(val), "None"))
			{
				if (gstate->softmask)
				{
					pdf_dropxobject(gstate->softmask);
					gstate->softmask = nil;
				}
			}
		}

		else if (!strcmp(s, "TR"))
		{
			if (fz_isname(val) && strcmp(fz_toname(val), "Identity"))
				fz_warn("ignoring transfer function");
		}
	}

	return fz_okay;
}

static void pdf_run_BDC(pdf_csi *csi)
{
}

static fz_error pdf_run_BI(pdf_csi *csi, fz_obj *rdb, fz_stream *file)
{
	int ch;
	fz_error error;
	char *buf = csi->xref->scratch;
	int buflen = sizeof(csi->xref->scratch);
	fz_obj *obj;

	error = pdf_parsedict(&obj, csi->xref, file, buf, buflen);
	if (error)
		return fz_rethrow(error, "cannot parse inline image dictionary");

	/* read whitespace after ID keyword */
	ch = fz_readbyte(file);
	if (ch == '\r')
		if (fz_peekbyte(file) == '\n')
			fz_readbyte(file);

	error = pdf_runinlineimage(csi, rdb, file, obj);
	fz_dropobj(obj);
	if (error)
		return fz_rethrow(error, "cannot parse inline image");

	return fz_okay;
}

static void pdf_run_B(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 1, 1, 0);
}

static void pdf_run_BMC(pdf_csi *csi)
{
}

static void pdf_run_BT(pdf_csi *csi)
{
	csi->intext = 1;
	csi->tm = fz_identity;
	csi->tlm = fz_identity;
}

static void pdf_run_BX(pdf_csi *csi)
{
	csi->xbalance ++;
}

static void pdf_run_Bstar(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 1, 1, 1);
}

static fz_error pdf_run_cs_imp(pdf_csi *csi, fz_obj *rdb, int what)
{
	fz_colorspace *colorspace;
	fz_obj *obj, *dict;
	fz_error error;

	if (!strcmp(csi->name, "Pattern"))
	{
		pdf_setpattern(csi, what, nil, nil);
	}
	else
	{
		if (!strcmp(csi->name, "DeviceGray"))
			colorspace = fz_keepcolorspace(fz_devicegray);
		else if (!strcmp(csi->name, "DeviceRGB"))
			colorspace = fz_keepcolorspace(fz_devicergb);
		else if (!strcmp(csi->name, "DeviceCMYK"))
			colorspace = fz_keepcolorspace(fz_devicecmyk);
		else
		{
			dict = fz_dictgets(rdb, "ColorSpace");
			if (!dict)
				return fz_throw("cannot find ColorSpace dictionary");
			obj = fz_dictgets(dict, csi->name);
			if (!obj)
				return fz_throw("cannot find colorspace resource '%s'", csi->name);
			error = pdf_loadcolorspace(&colorspace, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load colorspace (%d 0 R)", fz_tonum(obj));
		}

		pdf_setcolorspace(csi, what, colorspace);

		fz_dropcolorspace(colorspace);
	}
	return fz_okay;
}

static void pdf_run_CS(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	error = pdf_run_cs_imp(csi, rdb, PDF_MSTROKE);
	if (error)
		fz_catch(error, "cannot set colorspace");
}

static void pdf_run_cs(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	error = pdf_run_cs_imp(csi, rdb, PDF_MFILL);
	if (error)
		fz_catch(error, "cannot set colorspace");
}

static void pdf_run_DP(pdf_csi *csi)
{
}

static fz_error pdf_run_Do(pdf_csi *csi, fz_obj *rdb)
{
	fz_obj *dict;
	fz_obj *obj;
	fz_obj *subtype;
	fz_error error;

	dict = fz_dictgets(rdb, "XObject");
	if (!dict)
		return fz_throw("cannot find XObject dictionary when looking for: '%s'", csi->name);

	obj = fz_dictgets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find xobject resource: '%s'", csi->name);

	subtype = fz_dictgets(obj, "Subtype");
	if (!fz_isname(subtype))
		return fz_throw("no XObject subtype specified");

	if (pdf_ishiddenocg(csi, obj))
		return fz_okay;

	if (!strcmp(fz_toname(subtype), "Form") && fz_dictgets(obj, "Subtype2"))
		subtype = fz_dictgets(obj, "Subtype2");

	if (!strcmp(fz_toname(subtype), "Form"))
	{
		pdf_xobject *xobj;

		error = pdf_loadxobject(&xobj, csi->xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load xobject (%d %d R)", fz_tonum(obj), fz_togen(obj));

		/* Inherit parent resources, in case this one was empty XXX check where it's loaded */
		if (!xobj->resources)
			xobj->resources = fz_keepobj(rdb);

		error = pdf_runxobject(csi, xobj->resources, xobj, fz_identity);
		if (error)
			return fz_rethrow(error, "cannot draw xobject (%d %d R)", fz_tonum(obj), fz_togen(obj));

		pdf_dropxobject(xobj);
	}

	else if (!strcmp(fz_toname(subtype), "Image"))
	{
		if ((csi->dev->hints & FZ_IGNOREIMAGE) == 0)
		{
			fz_pixmap *img;
			error = pdf_loadimage(&img, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load image (%d %d R)", fz_tonum(obj), fz_togen(obj));
			pdf_showimage(csi, img);
			fz_droppixmap(img);
		}
	}

	else if (!strcmp(fz_toname(subtype), "PS"))
	{
		fz_warn("ignoring XObject with subtype PS");
	}

	else
	{
		return fz_throw("unknown XObject subtype: '%s'", fz_toname(subtype));
	}

	return fz_okay;
}

static void pdf_run_EMC(pdf_csi *csi)
{
}

static void pdf_run_ET(pdf_csi *csi)
{
	pdf_flushtext(csi);
	csi->accumulate = 1;
	csi->intext = 0;
}

static void pdf_run_EX(pdf_csi *csi)
{
	csi->xbalance --;
}

static void pdf_run_F(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 1, 0, 0);
}

static void pdf_run_G(pdf_csi *csi)
{
	pdf_setcolorspace(csi, PDF_MSTROKE, fz_devicegray);
	pdf_setcolor(csi, PDF_MSTROKE, csi->stack);
}

static void pdf_run_J(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->strokestate.linecap = csi->stack[0];
}

static void pdf_run_K(pdf_csi *csi)
{
	pdf_setcolorspace(csi, PDF_MSTROKE, fz_devicecmyk);
	pdf_setcolor(csi, PDF_MSTROKE, csi->stack);
}

static void pdf_run_M(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->strokestate.miterlimit = csi->stack[0];
}

static void pdf_run_MP(pdf_csi *csi)
{
}

static void pdf_run_Q(pdf_csi *csi)
{
	pdf_grestore(csi);
}

static void pdf_run_RG(pdf_csi *csi)
{
	pdf_setcolorspace(csi, PDF_MSTROKE, fz_devicergb);
	pdf_setcolor(csi, PDF_MSTROKE, csi->stack);
}

static void pdf_run_S(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 0, 1, 0);
}

static fz_error pdf_run_SC_imp(pdf_csi *csi, fz_obj *rdb, int what, pdf_material *mat)
{
	fz_error error;
	fz_obj *patterntype;
	fz_obj *dict;
	fz_obj *obj;
	int kind;

	kind = mat->kind;
	if (csi->name[0])
		kind = PDF_MPATTERN;

	switch (kind)
	{
	case PDF_MNONE:
		return fz_throw("cannot set color in mask objects");

	case PDF_MCOLOR:
		pdf_setcolor(csi, what, csi->stack);
		break;

	case PDF_MPATTERN:
		dict = fz_dictgets(rdb, "Pattern");
		if (!dict)
			return fz_throw("cannot find Pattern dictionary");

		obj = fz_dictgets(dict, csi->name);
		if (!obj)
			return fz_throw("cannot find pattern resource '%s'", csi->name);

		patterntype = fz_dictgets(obj, "PatternType");

		if (fz_toint(patterntype) == 1)
		{
			pdf_pattern *pat;
			error = pdf_loadpattern(&pat, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load pattern (%d 0 R)", fz_tonum(obj));
			pdf_setpattern(csi, what, pat, csi->top > 0 ? csi->stack : nil);
			pdf_droppattern(pat);
		}
		else if (fz_toint(patterntype) == 2)
		{
			fz_shade *shd;
			error = pdf_loadshading(&shd, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load shading (%d 0 R)", fz_tonum(obj));
			pdf_setshade(csi, what, shd);
			fz_dropshade(shd);
		}
		else
		{
			return fz_throw("unknown pattern type: %d", fz_toint(patterntype));
		}
		break;

	case PDF_MSHADE:
		return fz_throw("cannot set color in shade objects");
	}

	return fz_okay;
}

static void pdf_run_SC(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	error = pdf_run_SC_imp(csi, rdb, PDF_MSTROKE, &gstate->stroke);
	if (error)
		fz_catch(error, "cannot set color and colorspace");
}

static void pdf_run_sc(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	error = pdf_run_SC_imp(csi, rdb, PDF_MFILL, &gstate->fill);
	if (error)
		fz_catch(error, "cannot set color and colorspace");
}

static void pdf_run_Tc(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->charspace = csi->stack[0];
}

static void pdf_run_Tw(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->wordspace = csi->stack[0];
}

static void pdf_run_Tz(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	float a = csi->stack[0] / 100;
	pdf_flushtext(csi);
	gstate->scale = a;
}

static void pdf_run_TL(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->leading = csi->stack[0];
}

static fz_error pdf_run_Tf(pdf_csi *csi, fz_obj *rdb)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_error error;
	fz_obj *dict;
	fz_obj *obj;

	gstate->size = csi->stack[0];
	if (gstate->font)
		pdf_dropfont(gstate->font);
	gstate->font = nil;

	dict = fz_dictgets(rdb, "Font");
	if (!dict)
		return fz_throw("cannot find Font dictionary");

	obj = fz_dictgets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find font resource: '%s'", csi->name);

	error = pdf_loadfont(&gstate->font, csi->xref, rdb, obj);
	if (error)
		return fz_rethrow(error, "cannot load font (%d 0 R)", fz_tonum(obj));

	return fz_okay;
}

static void pdf_run_Tr(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->render = csi->stack[0];
}

static void pdf_run_Ts(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->rise = csi->stack[0];
}

static void pdf_run_Td(pdf_csi *csi)
{
	fz_matrix m = fz_translate(csi->stack[0], csi->stack[1]);
	csi->tlm = fz_concat(m, csi->tlm);
	csi->tm = csi->tlm;
}

static void pdf_run_TD(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_matrix m;

	gstate->leading = -csi->stack[1];
	m = fz_translate(csi->stack[0], csi->stack[1]);
	csi->tlm = fz_concat(m, csi->tlm);
	csi->tm = csi->tlm;
}

static void pdf_run_Tm(pdf_csi *csi)
{
	pdf_flushtext(csi);
	csi->tm.a = csi->stack[0];
	csi->tm.b = csi->stack[1];
	csi->tm.c = csi->stack[2];
	csi->tm.d = csi->stack[3];
	csi->tm.e = csi->stack[4];
	csi->tm.f = csi->stack[5];
	csi->tlm = csi->tm;
}

static void pdf_run_Tstar(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_matrix m = fz_translate(0, -gstate->leading);
	csi->tlm = fz_concat(m, csi->tlm);
	csi->tm = csi->tlm;
}

static void pdf_run_Tj(pdf_csi *csi)
{
	if (csi->stringlen)
		pdf_showstring(csi, csi->string, csi->stringlen);
	else
		pdf_showtext(csi, csi->obj);
}

static void pdf_run_TJ(pdf_csi *csi)
{
	if (csi->stringlen)
		pdf_showstring(csi, csi->string, csi->stringlen);
	else
		pdf_showtext(csi, csi->obj);
}

static void pdf_run_W(pdf_csi *csi)
{
	csi->clip = 1;
	csi->clipevenodd = 0;
}

static void pdf_run_Wstar(pdf_csi *csi)
{
	csi->clip = 1;
	csi->clipevenodd = 1;
}

static void pdf_run_b(pdf_csi *csi)
{
	pdf_showpath(csi, 1, 1, 1, 0);
}

static void pdf_run_bstar(pdf_csi *csi)
{
	pdf_showpath(csi, 1, 1, 1, 1);
}

static void pdf_run_c(pdf_csi *csi)
{
	float a, b, c, d, e, f;
	a = csi->stack[0];
	b = csi->stack[1];
	c = csi->stack[2];
	d = csi->stack[3];
	e = csi->stack[4];
	f = csi->stack[5];
	fz_curveto(csi->path, a, b, c, d, e, f);
}

static void pdf_run_cm(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_matrix m;

	m.a = csi->stack[0];
	m.b = csi->stack[1];
	m.c = csi->stack[2];
	m.d = csi->stack[3];
	m.e = csi->stack[4];
	m.f = csi->stack[5];

	gstate->ctm = fz_concat(m, gstate->ctm);
}

static void pdf_run_d(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_obj *array;
	int i;

	array = csi->obj;
	gstate->strokestate.dashlen = MIN(fz_arraylen(array), nelem(gstate->strokestate.dashlist));
	for (i = 0; i < gstate->strokestate.dashlen; i++)
		gstate->strokestate.dashlist[i] = fz_toreal(fz_arrayget(array, i));
	gstate->strokestate.dashphase = csi->stack[0];
}

static void pdf_run_d0(pdf_csi *csi)
{
}

static void pdf_run_d1(pdf_csi *csi)
{
}

static void pdf_run_f(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 1, 0, 0);
}

static void pdf_run_fstar(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 1, 0, 1);
}

static void pdf_run_g(pdf_csi *csi)
{
	pdf_setcolorspace(csi, PDF_MFILL, fz_devicegray);
	pdf_setcolor(csi, PDF_MFILL, csi->stack);
}

static fz_error pdf_run_gs(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	fz_obj *dict;
	fz_obj *obj;

	dict = fz_dictgets(rdb, "ExtGState");
	if (!dict)
		return fz_throw("cannot find ExtGState dictionary");

	obj = fz_dictgets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find extgstate resource '%s'", csi->name);

	error = pdf_runextgstate(csi, rdb, obj);
	if (error)
		return fz_rethrow(error, "cannot set ExtGState (%d 0 R)", fz_tonum(obj));
	return fz_okay;
}

static void pdf_run_h(pdf_csi *csi)
{
	fz_closepath(csi->path);
}

static void pdf_run_i(pdf_csi *csi)
{
}

static void pdf_run_j(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->strokestate.linejoin = csi->stack[0];
}

static void pdf_run_k(pdf_csi *csi)
{
	pdf_setcolorspace(csi, PDF_MFILL, fz_devicecmyk);
	pdf_setcolor(csi, PDF_MFILL, csi->stack);
}

static void pdf_run_l(pdf_csi *csi)
{
	float a, b;
	a = csi->stack[0];
	b = csi->stack[1];
	fz_lineto(csi->path, a, b);
}

static void pdf_run_m(pdf_csi *csi)
{
	float a, b;
	a = csi->stack[0];
	b = csi->stack[1];
	fz_moveto(csi->path, a, b);
}

static void pdf_run_n(pdf_csi *csi)
{
	pdf_showpath(csi, 0, 0, 0, csi->clipevenodd);
}

static void pdf_run_q(pdf_csi *csi)
{
	pdf_gsave(csi);
}

static void pdf_run_re(pdf_csi *csi)
{
	float x, y, w, h;

	x = csi->stack[0];
	y = csi->stack[1];
	w = csi->stack[2];
	h = csi->stack[3];

	fz_moveto(csi->path, x, y);
	fz_lineto(csi->path, x + w, y);
	fz_lineto(csi->path, x + w, y + h);
	fz_lineto(csi->path, x, y + h);
	fz_closepath(csi->path);
}

static void pdf_run_rg(pdf_csi *csi)
{
	pdf_setcolorspace(csi, PDF_MFILL, fz_devicergb);
	pdf_setcolor(csi, PDF_MFILL, csi->stack);
}

static void pdf_run_ri(pdf_csi *csi)
{
}

static void pdf_run_s(pdf_csi *csi)
{
	pdf_showpath(csi, 1, 0, 1, 0);
}

static fz_error pdf_run_sh(pdf_csi *csi, fz_obj *rdb)
{
	fz_obj *dict;
	fz_obj *obj;
	fz_shade *shd;
	fz_error error;

	dict = fz_dictgets(rdb, "Shading");
	if (!dict)
		return fz_throw("cannot find shading dictionary");

	obj = fz_dictgets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find shading resource: '%s'", csi->name);

	if ((csi->dev->hints & FZ_IGNORESHADE) == 0)
	{
		error = pdf_loadshading(&shd, csi->xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load shading (%d %d R)", fz_tonum(obj), fz_togen(obj));
		pdf_showshade(csi, shd);
		fz_dropshade(shd);
	}
	return fz_okay;
}

static void pdf_run_v(pdf_csi *csi)
{
	float a, b, c, d;
	a = csi->stack[0];
	b = csi->stack[1];
	c = csi->stack[2];
	d = csi->stack[3];
	fz_curvetov(csi->path, a, b, c, d);
}

static void pdf_run_w(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->strokestate.linewidth = csi->stack[0];
}

static void pdf_run_y(pdf_csi *csi)
{
	float a, b, c, d;
	a = csi->stack[0];
	b = csi->stack[1];
	c = csi->stack[2];
	d = csi->stack[3];
	fz_curvetoy(csi->path, a, b, c, d);
}

static void pdf_run_squote(pdf_csi *csi)
{
	fz_matrix m;
	pdf_gstate *gstate = csi->gstate + csi->gtop;

	m = fz_translate(0, -gstate->leading);
	csi->tlm = fz_concat(m, csi->tlm);
	csi->tm = csi->tlm;

	if (csi->stringlen)
		pdf_showstring(csi, csi->string, csi->stringlen);
	else
		pdf_showtext(csi, csi->obj);
}

static void pdf_run_dquote(pdf_csi *csi)
{
	fz_matrix m;
	pdf_gstate *gstate = csi->gstate + csi->gtop;

	gstate->wordspace = csi->stack[0];
	gstate->charspace = csi->stack[1];

	m = fz_translate(0, -gstate->leading);
	csi->tlm = fz_concat(m, csi->tlm);
	csi->tm = csi->tlm;

	if (csi->stringlen)
		pdf_showstring(csi, csi->string, csi->stringlen);
	else
		pdf_showtext(csi, csi->obj);
}

#define A(a) (a)
#define B(a,b) (a | b << 8)
#define C(a,b,c) (a | b << 8 | c << 16)

static fz_error
pdf_runkeyword(pdf_csi *csi, fz_obj *rdb, fz_stream *file, char *buf)
{
	fz_error error;
	int key;

	key = buf[0];
	if (buf[1])
	{
		key |= buf[1] << 8;
		if (buf[2])
		{
			key |= buf[2] << 16;
			if (buf[3])
				key = 0;
		}
	}

	switch (key)
	{
	case A('"'): pdf_run_dquote(csi); break;
	case A('\''): pdf_run_squote(csi); break;
	case A('B'): pdf_run_B(csi); break;
	case B('B','*'): pdf_run_Bstar(csi); break;
	case C('B','D','C'): pdf_run_BDC(csi); break;
	case B('B','I'):
		error = pdf_run_BI(csi, rdb, file);
		if (error)
			return fz_rethrow(error, "cannot draw inline image");
		break;
	case C('B','M','C'): pdf_run_BMC(csi); break;
	case B('B','T'): pdf_run_BT(csi); break;
	case B('B','X'): pdf_run_BX(csi); break;
	case B('C','S'): pdf_run_CS(csi, rdb); break;
	case B('D','P'): pdf_run_DP(csi); break;
	case B('D','o'):
		error = pdf_run_Do(csi, rdb);
		if (error)
			fz_catch(error, "cannot draw xobject/image");
		break;
	case C('E','M','C'): pdf_run_EMC(csi); break;
	case B('E','T'): pdf_run_ET(csi); break;
	case B('E','X'): pdf_run_EX(csi); break;
	case A('F'): pdf_run_F(csi); break;
	case A('G'): pdf_run_G(csi); break;
	case A('J'): pdf_run_J(csi); break;
	case A('K'): pdf_run_K(csi); break;
	case A('M'): pdf_run_M(csi); break;
	case B('M','P'): pdf_run_MP(csi); break;
	case A('Q'): pdf_run_Q(csi); break;
	case B('R','G'): pdf_run_RG(csi); break;
	case A('S'): pdf_run_S(csi); break;
	case B('S','C'): pdf_run_SC(csi, rdb); break;
	case C('S','C','N'): pdf_run_SC(csi, rdb); break;
	case B('T','*'): pdf_run_Tstar(csi); break;
	case B('T','D'): pdf_run_TD(csi); break;
	case B('T','J'): pdf_run_TJ(csi); break;
	case B('T','L'): pdf_run_TL(csi); break;
	case B('T','c'): pdf_run_Tc(csi); break;
	case B('T','d'): pdf_run_Td(csi); break;
	case B('T','f'):
		error = pdf_run_Tf(csi, rdb);
		if (error)
			fz_catch(error, "cannot set font");
		break;
	case B('T','j'): pdf_run_Tj(csi); break;
	case B('T','m'): pdf_run_Tm(csi); break;
	case B('T','r'): pdf_run_Tr(csi); break;
	case B('T','s'): pdf_run_Ts(csi); break;
	case B('T','w'): pdf_run_Tw(csi); break;
	case B('T','z'): pdf_run_Tz(csi); break;
	case A('W'): pdf_run_W(csi); break;
	case B('W','*'): pdf_run_Wstar(csi); break;
	case A('b'): pdf_run_b(csi); break;
	case B('b','*'): pdf_run_bstar(csi); break;
	case A('c'): pdf_run_c(csi); break;
	case B('c','m'): pdf_run_cm(csi); break;
	case B('c','s'): pdf_run_cs(csi, rdb); break;
	case A('d'): pdf_run_d(csi); break;
	case B('d','0'): pdf_run_d0(csi); break;
	case B('d','1'): pdf_run_d1(csi); break;
	case A('f'): pdf_run_f(csi); break;
	case B('f','*'): pdf_run_fstar(csi); break;
	case A('g'): pdf_run_g(csi); break;
	case B('g','s'):
		error = pdf_run_gs(csi, rdb);
		if (error)
			fz_catch(error, "cannot set graphics state");
		break;
	case A('h'): pdf_run_h(csi); break;
	case A('i'): pdf_run_i(csi); break;
	case A('j'): pdf_run_j(csi); break;
	case A('k'): pdf_run_k(csi); break;
	case A('l'): pdf_run_l(csi); break;
	case A('m'): pdf_run_m(csi); break;
	case A('n'): pdf_run_n(csi); break;
	case A('q'): pdf_run_q(csi); break;
	case B('r','e'): pdf_run_re(csi); break;
	case B('r','g'): pdf_run_rg(csi); break;
	case B('r','i'): pdf_run_ri(csi); break;
	case A('s'): pdf_run_s(csi); break;
	case B('s','c'): pdf_run_sc(csi, rdb); break;
	case C('s','c','n'): pdf_run_sc(csi, rdb); break;
	case B('s','h'):
		error = pdf_run_sh(csi, rdb);
		if (error)
			fz_catch(error, "cannot draw shading");
		break;
	case A('v'): pdf_run_v(csi); break;
	case A('w'): pdf_run_w(csi); break;
	case A('y'): pdf_run_y(csi); break;
	default:
		if (!csi->xbalance)
			fz_warn("unknown keyword: '%s'", buf);
		break;
	}

	return fz_okay;
}

static fz_error
pdf_runcsifile(pdf_csi *csi, fz_obj *rdb, fz_stream *file, char *buf, int buflen)
{
	fz_error error;
	int tok;
	int len;

	pdf_clearstack(csi);

	while (1)
	{
		if (csi->top == nelem(csi->stack) - 1)
			return fz_throw("stack overflow");

		error = pdf_lex(&tok, file, buf, buflen, &len);
		if (error)
			return fz_rethrow(error, "lexical error in content stream");

		if (csi->inarray)
		{
			if (tok == PDF_TCARRAY)
			{
				csi->inarray = 0;
			}
			else if (tok == PDF_TINT || tok == PDF_TREAL)
			{
				pdf_gstate *gstate = csi->gstate + csi->gtop;
				pdf_showspace(csi, -atof(buf) * gstate->size * 0.001f);
			}
			else if (tok == PDF_TSTRING)
			{
				pdf_showstring(csi, (unsigned char *)buf, len);
			}
			else if (tok == PDF_TKEYWORD)
			{
				if (!strcmp(buf, "Tw") || !strcmp(buf, "Tc"))
					fz_warn("ignoring keyword '%s' inside array", buf);
				else
					return fz_throw("syntax error in array");
			}
			else if (tok == PDF_TEOF)
				return fz_okay;
			else
				return fz_throw("syntax error in array");
		}

		else switch (tok)
		{
		case PDF_TENDSTREAM:
		case PDF_TEOF:
			return fz_okay;

		case PDF_TOARRAY:
			if (!csi->intext)
			{
				error = pdf_parsearray(&csi->obj, csi->xref, file, buf, buflen);
				if (error)
					return fz_rethrow(error, "cannot parse array");
			}
			else
			{
				csi->inarray = 1;
			}
			break;

		case PDF_TODICT:
			error = pdf_parsedict(&csi->obj, csi->xref, file, buf, buflen);
			if (error)
				return fz_rethrow(error, "cannot parse dictionary");
			break;

		case PDF_TNAME:
			fz_strlcpy(csi->name, buf, sizeof(csi->name));
			break;

		case PDF_TINT:
			csi->stack[csi->top] = atoi(buf);
			csi->top ++;
			break;

		case PDF_TREAL:
			csi->stack[csi->top] = atof(buf);
			csi->top ++;
			break;

		case PDF_TSTRING:
			if (len <= sizeof(csi->string))
			{
				memcpy(csi->string, buf, len);
				csi->stringlen = len;
			}
			else
			{
				csi->obj = fz_newstring(buf, len);
			}
			break;

		case PDF_TKEYWORD:
			error = pdf_runkeyword(csi, rdb, file, buf);
			if (error)
				return fz_rethrow(error, "cannot run keyword");
			pdf_clearstack(csi);
			break;

		default:
			return fz_throw("syntax error in content stream");
		}
	}
}

fz_error
pdf_runcsibuffer(pdf_csi *csi, fz_obj *rdb, fz_buffer *contents)
{
	fz_stream *file;
	fz_error error;
	file = fz_openbuffer(contents);
	error = pdf_runcsifile(csi, rdb, file, csi->xref->scratch, sizeof csi->xref->scratch);
	fz_close(file);
	if (error)
		return fz_rethrow(error, "cannot parse content stream");
	return fz_okay;
}

fz_error
pdf_runpagewithtarget(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm, char *target)
{
	pdf_csi *csi;
	fz_error error;
	pdf_annot *annot;
	int flags;

	if (page->transparency)
		dev->begingroup(dev->user,
			fz_transformrect(ctm, page->mediabox),
			0, 0, FZ_BNORMAL, 1);

	csi = pdf_newcsi(xref, dev, ctm, target);
	error = pdf_runcsibuffer(csi, page->resources, page->contents);
	pdf_freecsi(csi);
	if (error)
		return fz_rethrow(error, "cannot parse page content stream");

	for (annot = page->annots; annot; annot = annot->next)
	{
		flags = fz_toint(fz_dictgets(annot->obj, "F"));

		/* TODO: NoZoom and NoRotate */
		if (flags & (1 << 0)) /* Invisible */
			continue;
		if (flags & (1 << 1)) /* Hidden */
			continue;
		if (flags & (1 << 5)) /* NoView */
			continue;

		if (pdf_ishiddenocg(csi, annot->obj))
			continue;

		csi = pdf_newcsi(xref, dev, ctm, target);
		error = pdf_runxobject(csi, page->resources, annot->ap, annot->matrix);
		pdf_freecsi(csi);
		if (error)
			return fz_rethrow(error, "cannot parse annotation appearance stream");
	}

	if (page->transparency)
		dev->endgroup(dev->user);

	return fz_okay;
}

fz_error
pdf_runpage(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm)
{
	return pdf_runpagewithtarget(xref, page, dev, ctm, "View");
}

fz_error
pdf_runglyph(pdf_xref *xref, fz_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm)
{
	pdf_csi *csi = pdf_newcsi(xref, dev, ctm, "View");
	fz_error error = pdf_runcsibuffer(csi, resources, contents);
	pdf_freecsi(csi);
	if (error)
		return fz_rethrow(error, "cannot parse glyph content stream");
	return fz_okay;
}
