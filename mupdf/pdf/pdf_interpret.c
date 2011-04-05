#include "fitz.h"
#include "mupdf.h"

static pdf_csi *
pdf_new_csi(pdf_xref *xref, fz_device *dev, fz_matrix ctm, char *target)
{
	pdf_csi *csi;

	csi = fz_malloc(sizeof(pdf_csi));
	csi->xref = xref;
	csi->dev = dev;
	csi->target = target;

	csi->top = 0;
	csi->obj = NULL;
	csi->name[0] = 0;
	csi->string_len = 0;
	memset(csi->stack, 0, sizeof csi->stack);

	csi->xbalance = 0;
	csi->in_text = 0;
	csi->in_array = 0;

	csi->path = fz_new_path();
	csi->clip = 0;
	csi->clip_even_odd = 0;

	csi->text = NULL;
	csi->tlm = fz_identity;
	csi->tm = fz_identity;
	csi->text_mode = 0;
	csi->accumulate = 1;

	csi->top_ctm = ctm;
	pdf_init_gstate(&csi->gstate[0], ctm);
	csi->gtop = 0;

	return csi;
}

static void
pdf_clear_stack(pdf_csi *csi)
{
	int i;

	if (csi->obj)
		fz_drop_obj(csi->obj);
	csi->obj = NULL;

	csi->name[0] = 0;
	csi->string_len = 0;
	for (i = 0; i < csi->top; i++)
		csi->stack[i] = 0;

	csi->top = 0;
}

pdf_material *
pdf_keep_material(pdf_material *mat)
{
	if (mat->colorspace)
		fz_keep_colorspace(mat->colorspace);
	if (mat->pattern)
		pdf_keep_pattern(mat->pattern);
	if (mat->shade)
		fz_keep_shade(mat->shade);
	return mat;
}

pdf_material *
pdf_drop_material(pdf_material *mat)
{
	if (mat->colorspace)
		fz_drop_colorspace(mat->colorspace);
	if (mat->pattern)
		pdf_drop_pattern(mat->pattern);
	if (mat->shade)
		fz_drop_shade(mat->shade);
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

	pdf_keep_material(&gs->stroke);
	pdf_keep_material(&gs->fill);
	if (gs->font)
		pdf_keep_font(gs->font);
	if (gs->softmask)
		pdf_keep_xobject(gs->softmask);
}

void
pdf_grestore(pdf_csi *csi)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	int clip_depth = gs->clip_depth;

	if (csi->gtop == 0)
	{
		fz_warn("gstate underflow in content stream");
		return;
	}

	pdf_drop_material(&gs->stroke);
	pdf_drop_material(&gs->fill);
	if (gs->font)
		pdf_drop_font(gs->font);
	if (gs->softmask)
		pdf_drop_xobject(gs->softmask);

	csi->gtop --;

	gs = csi->gstate + csi->gtop;
	while (clip_depth > gs->clip_depth)
	{
		fz_pop_clip(csi->dev);
		clip_depth--;
	}
}

static void
pdf_free_csi(pdf_csi *csi)
{
	while (csi->gtop)
		pdf_grestore(csi);

	pdf_drop_material(&csi->gstate[0].fill);
	pdf_drop_material(&csi->gstate[0].stroke);
	if (csi->gstate[0].font)
		pdf_drop_font(csi->gstate[0].font);
	if (csi->gstate[0].softmask)
		pdf_drop_xobject(csi->gstate[0].softmask);

	while (csi->gstate[0].clip_depth--)
		fz_pop_clip(csi->dev);

	if (csi->path) fz_free_path(csi->path);
	if (csi->text) fz_free_text(csi->text);

	pdf_clear_stack(csi);

	fz_free(csi);
}

static int
pdf_is_hidden_ocg(fz_obj *xobj, char *target)
{
	char target_state[16];
	fz_obj *obj;

	fz_strlcpy(target_state, target, sizeof target_state);
	fz_strlcat(target_state, "State", sizeof target_state);

	obj = fz_dict_gets(xobj, "OC");
	obj = fz_dict_gets(obj, "OCGs");
	if (fz_is_array(obj))
		obj = fz_array_get(obj, 0);
	obj = fz_dict_gets(obj, "Usage");
	obj = fz_dict_gets(obj, target);
	obj = fz_dict_gets(obj, target_state);
	return !strcmp(fz_to_name(obj), "OFF");
}

fz_error
pdf_run_xobject(pdf_csi *csi, fz_obj *resources, pdf_xobject *xobj, fz_matrix transform)
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
			fz_rect bbox = fz_transform_rect(gstate->ctm, xobj->bbox);

			gstate->softmask = NULL;
			popmask = 1;

			fz_begin_mask(csi->dev, bbox, gstate->luminosity,
				softmask->colorspace, gstate->softmask_bc);
			error = pdf_run_xobject(csi, resources, softmask, fz_identity);
			if (error)
				return fz_rethrow(error, "cannot run softmask");
			fz_end_mask(csi->dev);

			pdf_drop_xobject(softmask);
		}

		fz_begin_group(csi->dev,
			fz_transform_rect(gstate->ctm, xobj->bbox),
			xobj->isolated, xobj->knockout, gstate->blendmode, gstate->fill.alpha);

		gstate->blendmode = FZ_BLEND_NORMAL;
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
	pdf_show_path(csi, 0, 0, 0, 0);

	/* run contents */

	oldtopctm = csi->top_ctm;
	csi->top_ctm = gstate->ctm;

	if (xobj->resources)
		resources = xobj->resources;

	error = pdf_run_csi_buffer(csi, resources, xobj->contents);
	if (error)
		return fz_rethrow(error, "cannot interpret XObject stream");

	csi->top_ctm = oldtopctm;

	while (oldtop < csi->gtop)
		pdf_grestore(csi);

	pdf_grestore(csi);

	/* wrap up transparency stacks */

	if (xobj->transparency)
	{
		fz_end_group(csi->dev);
		if (popmask)
			fz_pop_clip(csi->dev);
	}

	return fz_okay;
}

static fz_error
pdf_run_inline_image(pdf_csi *csi, fz_obj *rdb, fz_stream *file, fz_obj *dict)
{
	fz_error error;
	fz_pixmap *img;
	int ch;

	error = pdf_load_inline_image(&img, csi->xref, rdb, dict, file);
	if (error)
		return fz_rethrow(error, "cannot load inline image");

	/* find EI */
	ch = fz_read_byte(file);
	while (ch != 'E' && ch != EOF)
		ch = fz_read_byte(file);
	ch = fz_read_byte(file);
	if (ch != 'I')
	{
		fz_drop_pixmap(img);
		return fz_rethrow(error, "syntax error after inline image");
	}

	pdf_show_image(csi, img);

	fz_drop_pixmap(img);
	return fz_okay;
}

static fz_error
pdf_run_extgstate(pdf_csi *csi, fz_obj *rdb, fz_obj *extgstate)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_colorspace *colorspace;
	int i, k;

	pdf_flush_text(csi);

	for (i = 0; i < fz_dict_len(extgstate); i++)
	{
		fz_obj *key = fz_dict_get_key(extgstate, i);
		fz_obj *val = fz_dict_get_val(extgstate, i);
		char *s = fz_to_name(key);

		if (!strcmp(s, "Font"))
		{
			if (fz_is_array(val) && fz_array_len(val) == 2)
			{
				fz_error error;
				fz_obj *font = fz_array_get(val, 0);

				if (gstate->font)
				{
					pdf_drop_font(gstate->font);
					gstate->font = NULL;
				}

				error = pdf_load_font(&gstate->font, csi->xref, rdb, font);
				if (error)
					return fz_rethrow(error, "cannot load font (%d %d R)", fz_to_num(font), fz_to_gen(font));
				if (!gstate->font)
					return fz_throw("cannot find font in store");
				gstate->size = fz_to_real(fz_array_get(val, 1));
			}
			else
				return fz_throw("malformed /Font dictionary");
		}

		else if (!strcmp(s, "LW"))
			gstate->stroke_state.linewidth = fz_to_real(val);
		else if (!strcmp(s, "LC"))
			gstate->stroke_state.linecap = fz_to_int(val);
		else if (!strcmp(s, "LJ"))
			gstate->stroke_state.linejoin = fz_to_int(val);
		else if (!strcmp(s, "ML"))
			gstate->stroke_state.miterlimit = fz_to_real(val);

		else if (!strcmp(s, "D"))
		{
			if (fz_is_array(val) && fz_array_len(val) == 2)
			{
				fz_obj *dashes = fz_array_get(val, 0);
				gstate->stroke_state.dash_len = MAX(fz_array_len(dashes), 32);
				for (k = 0; k < gstate->stroke_state.dash_len; k++)
					gstate->stroke_state.dash_list[k] = fz_to_real(fz_array_get(dashes, k));
				gstate->stroke_state.dash_phase = fz_to_real(fz_array_get(val, 1));
			}
			else
				return fz_throw("malformed /D");
		}

		else if (!strcmp(s, "CA"))
			gstate->stroke.alpha = fz_to_real(val);

		else if (!strcmp(s, "ca"))
			gstate->fill.alpha = fz_to_real(val);

		else if (!strcmp(s, "BM"))
		{
			if (fz_is_array(val))
				val = fz_array_get(val, 0);

			gstate->blendmode = FZ_BLEND_NORMAL;
			for (k = 0; fz_blendmode_names[k]; k++)
				if (!strcmp(fz_blendmode_names[k], fz_to_name(val)))
					gstate->blendmode = k;
		}

		else if (!strcmp(s, "SMask"))
		{
			if (fz_is_dict(val))
			{
				fz_error error;
				pdf_xobject *xobj;
				fz_obj *group, *luminosity, *bc;

				if (gstate->softmask)
				{
					pdf_drop_xobject(gstate->softmask);
					gstate->softmask = NULL;
				}

				group = fz_dict_gets(val, "G");
				if (!group)
					return fz_throw("cannot load softmask xobject (%d %d R)", fz_to_num(val), fz_to_gen(val));
				error = pdf_load_xobject(&xobj, csi->xref, group);
				if (error)
					return fz_rethrow(error, "cannot load xobject (%d %d R)", fz_to_num(val), fz_to_gen(val));

				colorspace = xobj->colorspace;
				if (!colorspace)
					colorspace = fz_device_gray;

				gstate->softmask_ctm = fz_concat(xobj->matrix, gstate->ctm);
				gstate->softmask = xobj;
				for (k = 0; k < colorspace->n; k++)
					gstate->softmask_bc[k] = 0;

				bc = fz_dict_gets(val, "BC");
				if (fz_is_array(bc))
				{
					for (k = 0; k < colorspace->n; k++)
						gstate->softmask_bc[k] = fz_to_real(fz_array_get(bc, k));
				}

				luminosity = fz_dict_gets(val, "S");
				if (fz_is_name(luminosity) && !strcmp(fz_to_name(luminosity), "Luminosity"))
					gstate->luminosity = 1;
				else
					gstate->luminosity = 0;
			}
			else if (fz_is_name(val) && !strcmp(fz_to_name(val), "None"))
			{
				if (gstate->softmask)
				{
					pdf_drop_xobject(gstate->softmask);
					gstate->softmask = NULL;
				}
			}
		}

		else if (!strcmp(s, "TR"))
		{
			if (fz_is_name(val) && strcmp(fz_to_name(val), "Identity"))
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

	error = pdf_parse_dict(&obj, csi->xref, file, buf, buflen);
	if (error)
		return fz_rethrow(error, "cannot parse inline image dictionary");

	/* read whitespace after ID keyword */
	ch = fz_read_byte(file);
	if (ch == '\r')
		if (fz_peek_byte(file) == '\n')
			fz_read_byte(file);

	error = pdf_run_inline_image(csi, rdb, file, obj);
	fz_drop_obj(obj);
	if (error)
		return fz_rethrow(error, "cannot parse inline image");

	return fz_okay;
}

static void pdf_run_B(pdf_csi *csi)
{
	pdf_show_path(csi, 0, 1, 1, 0);
}

static void pdf_run_BMC(pdf_csi *csi)
{
}

static void pdf_run_BT(pdf_csi *csi)
{
	csi->in_text = 1;
	csi->tm = fz_identity;
	csi->tlm = fz_identity;
}

static void pdf_run_BX(pdf_csi *csi)
{
	csi->xbalance ++;
}

static void pdf_run_Bstar(pdf_csi *csi)
{
	pdf_show_path(csi, 0, 1, 1, 1);
}

static fz_error pdf_run_cs_imp(pdf_csi *csi, fz_obj *rdb, int what)
{
	fz_colorspace *colorspace;
	fz_obj *obj, *dict;
	fz_error error;

	if (!strcmp(csi->name, "Pattern"))
	{
		pdf_set_pattern(csi, what, NULL, NULL);
	}
	else
	{
		if (!strcmp(csi->name, "DeviceGray"))
			colorspace = fz_keep_colorspace(fz_device_gray);
		else if (!strcmp(csi->name, "DeviceRGB"))
			colorspace = fz_keep_colorspace(fz_device_rgb);
		else if (!strcmp(csi->name, "DeviceCMYK"))
			colorspace = fz_keep_colorspace(fz_device_cmyk);
		else
		{
			dict = fz_dict_gets(rdb, "ColorSpace");
			if (!dict)
				return fz_throw("cannot find ColorSpace dictionary");
			obj = fz_dict_gets(dict, csi->name);
			if (!obj)
				return fz_throw("cannot find colorspace resource '%s'", csi->name);
			error = pdf_load_colorspace(&colorspace, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load colorspace (%d 0 R)", fz_to_num(obj));
		}

		pdf_set_colorspace(csi, what, colorspace);

		fz_drop_colorspace(colorspace);
	}
	return fz_okay;
}

static void pdf_run_CS(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	error = pdf_run_cs_imp(csi, rdb, PDF_STROKE);
	if (error)
		fz_catch(error, "cannot set colorspace");
}

static void pdf_run_cs(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	error = pdf_run_cs_imp(csi, rdb, PDF_FILL);
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

	dict = fz_dict_gets(rdb, "XObject");
	if (!dict)
		return fz_throw("cannot find XObject dictionary when looking for: '%s'", csi->name);

	obj = fz_dict_gets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find xobject resource: '%s'", csi->name);

	subtype = fz_dict_gets(obj, "Subtype");
	if (!fz_is_name(subtype))
		return fz_throw("no XObject subtype specified");

	if (pdf_is_hidden_ocg(obj, csi->target))
		return fz_okay;

	if (!strcmp(fz_to_name(subtype), "Form") && fz_dict_gets(obj, "Subtype2"))
		subtype = fz_dict_gets(obj, "Subtype2");

	if (!strcmp(fz_to_name(subtype), "Form"))
	{
		pdf_xobject *xobj;

		error = pdf_load_xobject(&xobj, csi->xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load xobject (%d %d R)", fz_to_num(obj), fz_to_gen(obj));

		/* Inherit parent resources, in case this one was empty XXX check where it's loaded */
		if (!xobj->resources)
			xobj->resources = fz_keep_obj(rdb);

		error = pdf_run_xobject(csi, xobj->resources, xobj, fz_identity);
		if (error)
			return fz_rethrow(error, "cannot draw xobject (%d %d R)", fz_to_num(obj), fz_to_gen(obj));

		pdf_drop_xobject(xobj);
	}

	else if (!strcmp(fz_to_name(subtype), "Image"))
	{
		if ((csi->dev->hints & FZ_IGNORE_IMAGE) == 0)
		{
			fz_pixmap *img;
			error = pdf_load_image(&img, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load image (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
			pdf_show_image(csi, img);
			fz_drop_pixmap(img);
		}
	}

	else if (!strcmp(fz_to_name(subtype), "PS"))
	{
		fz_warn("ignoring XObject with subtype PS");
	}

	else
	{
		return fz_throw("unknown XObject subtype: '%s'", fz_to_name(subtype));
	}

	return fz_okay;
}

static void pdf_run_EMC(pdf_csi *csi)
{
}

static void pdf_run_ET(pdf_csi *csi)
{
	pdf_flush_text(csi);
	csi->accumulate = 1;
	csi->in_text = 0;
}

static void pdf_run_EX(pdf_csi *csi)
{
	csi->xbalance --;
}

static void pdf_run_F(pdf_csi *csi)
{
	pdf_show_path(csi, 0, 1, 0, 0);
}

static void pdf_run_G(pdf_csi *csi)
{
	pdf_set_colorspace(csi, PDF_STROKE, fz_device_gray);
	pdf_set_color(csi, PDF_STROKE, csi->stack);
}

static void pdf_run_J(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->stroke_state.linecap = csi->stack[0];
}

static void pdf_run_K(pdf_csi *csi)
{
	pdf_set_colorspace(csi, PDF_STROKE, fz_device_cmyk);
	pdf_set_color(csi, PDF_STROKE, csi->stack);
}

static void pdf_run_M(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->stroke_state.miterlimit = csi->stack[0];
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
	pdf_set_colorspace(csi, PDF_STROKE, fz_device_rgb);
	pdf_set_color(csi, PDF_STROKE, csi->stack);
}

static void pdf_run_S(pdf_csi *csi)
{
	pdf_show_path(csi, 0, 0, 1, 0);
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
		kind = PDF_MAT_PATTERN;

	switch (kind)
	{
	case PDF_MAT_NONE:
		return fz_throw("cannot set color in mask objects");

	case PDF_MAT_COLOR:
		pdf_set_color(csi, what, csi->stack);
		break;

	case PDF_MAT_PATTERN:
		dict = fz_dict_gets(rdb, "Pattern");
		if (!dict)
			return fz_throw("cannot find Pattern dictionary");

		obj = fz_dict_gets(dict, csi->name);
		if (!obj)
			return fz_throw("cannot find pattern resource '%s'", csi->name);

		patterntype = fz_dict_gets(obj, "PatternType");

		if (fz_to_int(patterntype) == 1)
		{
			pdf_pattern *pat;
			error = pdf_load_pattern(&pat, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load pattern (%d 0 R)", fz_to_num(obj));
			pdf_set_pattern(csi, what, pat, csi->top > 0 ? csi->stack : NULL);
			pdf_drop_pattern(pat);
		}
		else if (fz_to_int(patterntype) == 2)
		{
			fz_shade *shd;
			error = pdf_load_shading(&shd, csi->xref, obj);
			if (error)
				return fz_rethrow(error, "cannot load shading (%d 0 R)", fz_to_num(obj));
			pdf_set_shade(csi, what, shd);
			fz_drop_shade(shd);
		}
		else
		{
			return fz_throw("unknown pattern type: %d", fz_to_int(patterntype));
		}
		break;

	case PDF_MAT_SHADE:
		return fz_throw("cannot set color in shade objects");
	}

	return fz_okay;
}

static void pdf_run_SC(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	error = pdf_run_SC_imp(csi, rdb, PDF_STROKE, &gstate->stroke);
	if (error)
		fz_catch(error, "cannot set color and colorspace");
}

static void pdf_run_sc(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	error = pdf_run_SC_imp(csi, rdb, PDF_FILL, &gstate->fill);
	if (error)
		fz_catch(error, "cannot set color and colorspace");
}

static void pdf_run_Tc(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->char_space = csi->stack[0];
}

static void pdf_run_Tw(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	gstate->word_space = csi->stack[0];
}

static void pdf_run_Tz(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	float a = csi->stack[0] / 100;
	pdf_flush_text(csi);
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
		pdf_drop_font(gstate->font);
	gstate->font = NULL;

	dict = fz_dict_gets(rdb, "Font");
	if (!dict)
		return fz_throw("cannot find Font dictionary");

	obj = fz_dict_gets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find font resource: '%s'", csi->name);

	error = pdf_load_font(&gstate->font, csi->xref, rdb, obj);
	if (error)
		return fz_rethrow(error, "cannot load font (%d 0 R)", fz_to_num(obj));

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
	pdf_flush_text(csi);
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
	if (csi->string_len)
		pdf_show_string(csi, csi->string, csi->string_len);
	else
		pdf_show_text(csi, csi->obj);
}

static void pdf_run_TJ(pdf_csi *csi)
{
	if (csi->string_len)
		pdf_show_string(csi, csi->string, csi->string_len);
	else
		pdf_show_text(csi, csi->obj);
}

static void pdf_run_W(pdf_csi *csi)
{
	csi->clip = 1;
	csi->clip_even_odd = 0;
}

static void pdf_run_Wstar(pdf_csi *csi)
{
	csi->clip = 1;
	csi->clip_even_odd = 1;
}

static void pdf_run_b(pdf_csi *csi)
{
	pdf_show_path(csi, 1, 1, 1, 0);
}

static void pdf_run_bstar(pdf_csi *csi)
{
	pdf_show_path(csi, 1, 1, 1, 1);
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
	gstate->stroke_state.dash_len = MIN(fz_array_len(array), nelem(gstate->stroke_state.dash_list));
	for (i = 0; i < gstate->stroke_state.dash_len; i++)
		gstate->stroke_state.dash_list[i] = fz_to_real(fz_array_get(array, i));
	gstate->stroke_state.dash_phase = csi->stack[0];
}

static void pdf_run_d0(pdf_csi *csi)
{
}

static void pdf_run_d1(pdf_csi *csi)
{
}

static void pdf_run_f(pdf_csi *csi)
{
	pdf_show_path(csi, 0, 1, 0, 0);
}

static void pdf_run_fstar(pdf_csi *csi)
{
	pdf_show_path(csi, 0, 1, 0, 1);
}

static void pdf_run_g(pdf_csi *csi)
{
	pdf_set_colorspace(csi, PDF_FILL, fz_device_gray);
	pdf_set_color(csi, PDF_FILL, csi->stack);
}

static fz_error pdf_run_gs(pdf_csi *csi, fz_obj *rdb)
{
	fz_error error;
	fz_obj *dict;
	fz_obj *obj;

	dict = fz_dict_gets(rdb, "ExtGState");
	if (!dict)
		return fz_throw("cannot find ExtGState dictionary");

	obj = fz_dict_gets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find extgstate resource '%s'", csi->name);

	error = pdf_run_extgstate(csi, rdb, obj);
	if (error)
		return fz_rethrow(error, "cannot set ExtGState (%d 0 R)", fz_to_num(obj));
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
	gstate->stroke_state.linejoin = csi->stack[0];
}

static void pdf_run_k(pdf_csi *csi)
{
	pdf_set_colorspace(csi, PDF_FILL, fz_device_cmyk);
	pdf_set_color(csi, PDF_FILL, csi->stack);
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
	pdf_show_path(csi, 0, 0, 0, csi->clip_even_odd);
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
	pdf_set_colorspace(csi, PDF_FILL, fz_device_rgb);
	pdf_set_color(csi, PDF_FILL, csi->stack);
}

static void pdf_run_ri(pdf_csi *csi)
{
}

static void pdf_run(pdf_csi *csi)
{
	pdf_show_path(csi, 1, 0, 1, 0);
}

static fz_error pdf_run_sh(pdf_csi *csi, fz_obj *rdb)
{
	fz_obj *dict;
	fz_obj *obj;
	fz_shade *shd;
	fz_error error;

	dict = fz_dict_gets(rdb, "Shading");
	if (!dict)
		return fz_throw("cannot find shading dictionary");

	obj = fz_dict_gets(dict, csi->name);
	if (!obj)
		return fz_throw("cannot find shading resource: '%s'", csi->name);

	if ((csi->dev->hints & FZ_IGNORE_SHADE) == 0)
	{
		error = pdf_load_shading(&shd, csi->xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load shading (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
		pdf_show_shade(csi, shd);
		fz_drop_shade(shd);
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
	gstate->stroke_state.linewidth = csi->stack[0];
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

	if (csi->string_len)
		pdf_show_string(csi, csi->string, csi->string_len);
	else
		pdf_show_text(csi, csi->obj);
}

static void pdf_run_dquote(pdf_csi *csi)
{
	fz_matrix m;
	pdf_gstate *gstate = csi->gstate + csi->gtop;

	gstate->word_space = csi->stack[0];
	gstate->char_space = csi->stack[1];

	m = fz_translate(0, -gstate->leading);
	csi->tlm = fz_concat(m, csi->tlm);
	csi->tm = csi->tlm;

	if (csi->string_len)
		pdf_show_string(csi, csi->string, csi->string_len);
	else
		pdf_show_text(csi, csi->obj);
}

#define A(a) (a)
#define B(a,b) (a | b << 8)
#define C(a,b,c) (a | b << 8 | c << 16)

static fz_error
pdf_run_keyword(pdf_csi *csi, fz_obj *rdb, fz_stream *file, char *buf)
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
	case A('s'): pdf_run(csi); break;
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
pdf_run_csi_file(pdf_csi *csi, fz_obj *rdb, fz_stream *file, char *buf, int buflen)
{
	fz_error error;
	int tok;
	int len;

	pdf_clear_stack(csi);

	while (1)
	{
		if (csi->top == nelem(csi->stack) - 1)
			return fz_throw("stack overflow");

		error = pdf_lex(&tok, file, buf, buflen, &len);
		if (error)
			return fz_rethrow(error, "lexical error in content stream");

		if (csi->in_array)
		{
			if (tok == PDF_TOK_CLOSE_ARRAY)
			{
				csi->in_array = 0;
			}
			else if (tok == PDF_TOK_INT || tok == PDF_TOK_REAL)
			{
				pdf_gstate *gstate = csi->gstate + csi->gtop;
				pdf_show_space(csi, -atof(buf) * gstate->size * 0.001f);
			}
			else if (tok == PDF_TOK_STRING)
			{
				pdf_show_string(csi, (unsigned char *)buf, len);
			}
			else if (tok == PDF_TOK_KEYWORD)
			{
				if (!strcmp(buf, "Tw") || !strcmp(buf, "Tc"))
					fz_warn("ignoring keyword '%s' inside array", buf);
				else
					return fz_throw("syntax error in array");
			}
			else if (tok == PDF_TOK_EOF)
				return fz_okay;
			else
				return fz_throw("syntax error in array");
		}

		else switch (tok)
		{
		case PDF_TOK_ENDSTREAM:
		case PDF_TOK_EOF:
			return fz_okay;

		case PDF_TOK_OPEN_ARRAY:
			if (!csi->in_text)
			{
				error = pdf_parse_array(&csi->obj, csi->xref, file, buf, buflen);
				if (error)
					return fz_rethrow(error, "cannot parse array");
			}
			else
			{
				csi->in_array = 1;
			}
			break;

		case PDF_TOK_OPEN_DICT:
			error = pdf_parse_dict(&csi->obj, csi->xref, file, buf, buflen);
			if (error)
				return fz_rethrow(error, "cannot parse dictionary");
			break;

		case PDF_TOK_NAME:
			fz_strlcpy(csi->name, buf, sizeof(csi->name));
			break;

		case PDF_TOK_INT:
			csi->stack[csi->top] = atoi(buf);
			csi->top ++;
			break;

		case PDF_TOK_REAL:
			csi->stack[csi->top] = atof(buf);
			csi->top ++;
			break;

		case PDF_TOK_STRING:
			if (len <= sizeof(csi->string))
			{
				memcpy(csi->string, buf, len);
				csi->string_len = len;
			}
			else
			{
				csi->obj = fz_new_string(buf, len);
			}
			break;

		case PDF_TOK_KEYWORD:
			error = pdf_run_keyword(csi, rdb, file, buf);
			if (error)
				return fz_rethrow(error, "cannot run keyword");
			pdf_clear_stack(csi);
			break;

		default:
			return fz_throw("syntax error in content stream");
		}
	}
}

fz_error
pdf_run_csi_buffer(pdf_csi *csi, fz_obj *rdb, fz_buffer *contents)
{
	fz_stream *file;
	fz_error error;
	file = fz_open_buffer(contents);
	error = pdf_run_csi_file(csi, rdb, file, csi->xref->scratch, sizeof csi->xref->scratch);
	fz_close(file);
	if (error)
		return fz_rethrow(error, "cannot parse content stream");
	return fz_okay;
}

fz_error
pdf_run_page_with_usage(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm, char *target)
{
	pdf_csi *csi;
	fz_error error;
	pdf_annot *annot;
	int flags;

	if (page->transparency)
		dev->begin_group(dev->user,
			fz_transform_rect(ctm, page->mediabox),
			0, 0, FZ_BLEND_NORMAL, 1);

	csi = pdf_new_csi(xref, dev, ctm, target);
	error = pdf_run_csi_buffer(csi, page->resources, page->contents);
	pdf_free_csi(csi);
	if (error)
		return fz_rethrow(error, "cannot parse page content stream");

	for (annot = page->annots; annot; annot = annot->next)
	{
		flags = fz_to_int(fz_dict_gets(annot->obj, "F"));

		/* TODO: NoZoom and NoRotate */
		if (flags & (1 << 0)) /* Invisible */
			continue;
		if (flags & (1 << 1)) /* Hidden */
			continue;
		if (flags & (1 << 5)) /* NoView */
			continue;

		if (pdf_is_hidden_ocg(annot->obj, target))
			continue;

		csi = pdf_new_csi(xref, dev, ctm, target);
		error = pdf_run_xobject(csi, page->resources, annot->ap, annot->matrix);
		pdf_free_csi(csi);
		if (error)
			return fz_rethrow(error, "cannot parse annotation appearance stream");
	}

	if (page->transparency)
		dev->end_group(dev->user);

	return fz_okay;
}

fz_error
pdf_run_page(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm)
{
	return pdf_run_page_with_usage(xref, page, dev, ctm, "View");
}

fz_error
pdf_run_glyph(pdf_xref *xref, fz_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm)
{
	pdf_csi *csi = pdf_new_csi(xref, dev, ctm, "View");
	fz_error error = pdf_run_csi_buffer(csi, resources, contents);
	pdf_free_csi(csi);
	if (error)
		return fz_rethrow(error, "cannot parse glyph content stream");
	return fz_okay;
}
