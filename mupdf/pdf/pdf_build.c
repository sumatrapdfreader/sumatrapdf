#include "fitz.h"
#include "mupdf.h"
#include <ft2build.h>
#include FT_FREETYPE_H

#define TILE

void
pdf_init_gstate(pdf_gstate *gs, fz_matrix ctm)
{
	gs->ctm = ctm;
	gs->clip_depth = 0;

	gs->stroke_state.linewidth = 1;
	gs->stroke_state.linecap = 0;
	gs->stroke_state.linejoin = 0;
	gs->stroke_state.miterlimit = 10;
	gs->stroke_state.dash_phase = 0;
	gs->stroke_state.dash_len = 0;
	memset(gs->stroke_state.dash_list, 0, sizeof(gs->stroke_state.dash_list));

	gs->stroke.kind = PDF_MAT_COLOR;
	gs->stroke.colorspace = fz_keep_colorspace(fz_device_gray);
	gs->stroke.v[0] = 0;
	gs->stroke.pattern = NULL;
	gs->stroke.shade = NULL;
	gs->stroke.alpha = 1;

	gs->fill.kind = PDF_MAT_COLOR;
	gs->fill.colorspace = fz_keep_colorspace(fz_device_gray);
	gs->fill.v[0] = 0;
	gs->fill.pattern = NULL;
	gs->fill.shade = NULL;
	gs->fill.alpha = 1;

	gs->char_space = 0;
	gs->word_space = 0;
	gs->scale = 1;
	gs->leading = 0;
	gs->font = NULL;
	gs->size = -1;
	gs->render = 0;
	gs->rise = 0;

	gs->blendmode = FZ_BLEND_NORMAL;
	gs->softmask = NULL;
	gs->softmask_ctm = fz_identity;
	gs->luminosity = 0;
}

void
pdf_set_colorspace(pdf_csi *csi, int what, fz_colorspace *colorspace)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;

	pdf_flush_text(csi);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	fz_drop_colorspace(mat->colorspace);

	mat->kind = PDF_MAT_COLOR;
	mat->colorspace = fz_keep_colorspace(colorspace);

	mat->v[0] = 0;
	mat->v[1] = 0;
	mat->v[2] = 0;
	mat->v[3] = 1;
}

void
pdf_set_color(pdf_csi *csi, int what, float *v)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;
	int i;

	pdf_flush_text(csi);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	switch (mat->kind)
	{
	case PDF_MAT_PATTERN:
	case PDF_MAT_COLOR:
		if (!strcmp(mat->colorspace->name, "Lab"))
		{
			mat->v[0] = v[0] / 100;
			mat->v[1] = (v[1] + 100) / 200;
			mat->v[2] = (v[2] + 100) / 200;
		}
		for (i = 0; i < mat->colorspace->n; i++)
			mat->v[i] = v[i];
		break;
	default:
		fz_warn("color incompatible with material");
	}
}

static void
pdf_unset_pattern(pdf_csi *csi, int what)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;
	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;
	if (mat->kind == PDF_MAT_PATTERN)
	{
		if (mat->pattern)
			pdf_drop_pattern(mat->pattern);
		mat->pattern = NULL;
		mat->kind = PDF_MAT_COLOR;
	}
}

void
pdf_set_pattern(pdf_csi *csi, int what, pdf_pattern *pat, float *v)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;

	pdf_flush_text(csi);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	if (mat->pattern)
		pdf_drop_pattern(mat->pattern);

	mat->kind = PDF_MAT_PATTERN;
	if (pat)
		mat->pattern = pdf_keep_pattern(pat);
	else
		mat->pattern = NULL;

	if (v)
		pdf_set_color(csi, what, v);
}

void
pdf_set_shade(pdf_csi *csi, int what, fz_shade *shade)
{
	pdf_gstate *gs = csi->gstate + csi->gtop;
	pdf_material *mat;

	pdf_flush_text(csi);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	if (mat->shade)
		fz_drop_shade(mat->shade);

	mat->kind = PDF_MAT_SHADE;
	mat->shade = fz_keep_shade(shade);
}

static void
pdf_show_pattern(pdf_csi *csi, pdf_pattern *pat, fz_rect area, int what)
{
	pdf_gstate *gstate;
	fz_matrix ptm, invptm;
	fz_matrix oldtopctm;
	fz_error error;
	int x0, y0, x1, y1;
	int oldtop;

	pdf_gsave(csi);
	gstate = csi->gstate + csi->gtop;

	if (pat->ismask)
	{
		pdf_unset_pattern(csi, PDF_FILL);
		pdf_unset_pattern(csi, PDF_STROKE);
		if (what == PDF_FILL)
		{
			pdf_drop_material(&gstate->stroke);
			pdf_keep_material(&gstate->fill);
			gstate->stroke = gstate->fill;
		}
		if (what == PDF_STROKE)
		{
			pdf_drop_material(&gstate->fill);
			pdf_keep_material(&gstate->stroke);
			gstate->fill = gstate->stroke;
		}
	}
	else
	{
		// TODO: unset only the current fill/stroke or both?
		pdf_unset_pattern(csi, what);
	}

	/* don't apply softmasks to objects in the pattern as well */
	if (gstate->softmask)
	{
		pdf_drop_xobject(gstate->softmask);
		gstate->softmask = NULL;
	}

	ptm = fz_concat(pat->matrix, csi->top_ctm);
	invptm = fz_invert_matrix(ptm);

	/* patterns are painted using the ctm in effect at the beginning of the content stream */
	/* get bbox of shape in pattern space for stamping */
	area = fz_transform_rect(invptm, area);
	x0 = floorf(area.x0 / pat->xstep);
	y0 = floorf(area.y0 / pat->ystep);
	x1 = ceilf(area.x1 / pat->xstep);
	y1 = ceilf(area.y1 / pat->ystep);

	oldtopctm = csi->top_ctm;
	oldtop = csi->gtop;

#ifdef TILE
	if ((x1 - x0) * (y1 - y0) > 0)
	{
		fz_begin_tile(csi->dev, area, pat->bbox, pat->xstep, pat->ystep, ptm);
		gstate->ctm = ptm;
		csi->top_ctm = gstate->ctm;
		pdf_gsave(csi);
		error = pdf_run_csi_buffer(csi, pat->resources, pat->contents);
		if (error)
			fz_catch(error, "cannot render pattern tile");
		pdf_grestore(csi);
		while (oldtop < csi->gtop)
			pdf_grestore(csi);
		fz_end_tile(csi->dev);
	}
#else
	{
		int x, y;
		for (y = y0; y < y1; y++)
		{
			for (x = x0; x < x1; x++)
			{
				gstate->ctm = fz_concat(fz_translate(x * pat->xstep, y * pat->ystep), ptm);
				csi->top_ctm = gstate->ctm;
				error = pdf_run_csi_buffer(csi, pat->resources, pat->contents);
				while (oldtop < csi->gtop)
					pdf_grestore(csi);
				if (error)
				{
					fz_catch(error, "cannot render pattern tile");
					goto cleanup;
				}
			}
		}
	}
cleanup:
#endif

	csi->top_ctm = oldtopctm;

	pdf_grestore(csi);
}

static void
pdf_begin_group(pdf_csi *csi, fz_rect bbox)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_error error;

	if (gstate->softmask)
	{
		pdf_xobject *softmask = gstate->softmask;
		fz_rect bbox = fz_transform_rect(gstate->ctm, softmask->bbox);

		gstate->softmask = NULL;

		fz_begin_mask(csi->dev, bbox, gstate->luminosity,
			softmask->colorspace, gstate->softmask_bc);
		error = pdf_run_xobject(csi, NULL, softmask, fz_identity);
		if (error)
			fz_catch(error, "cannot run softmask");
		fz_end_mask(csi->dev);

		gstate->softmask = softmask;
	}

	if (gstate->blendmode != FZ_BLEND_NORMAL)
		fz_begin_group(csi->dev, bbox, 0, 0, gstate->blendmode, 1);
}

static void
pdf_end_group(pdf_csi *csi)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;

	if (gstate->blendmode != FZ_BLEND_NORMAL)
		fz_end_group(csi->dev);

	if (gstate->softmask)
		fz_pop_clip(csi->dev);
}

void
pdf_show_shade(pdf_csi *csi, fz_shade *shd)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_rect bbox;

	bbox = fz_bound_shade(shd, gstate->ctm);

	pdf_begin_group(csi, bbox);

	fz_fill_shade(csi->dev, shd, gstate->ctm, gstate->fill.alpha);

	pdf_end_group(csi);
}

void
pdf_show_image(pdf_csi *csi, fz_pixmap *image)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_rect bbox;

	bbox = fz_transform_rect(gstate->ctm, fz_unit_rect);

	if (image->mask)
	{
		/* apply blend group even though we skip the softmask */
		if (gstate->blendmode != FZ_BLEND_NORMAL)
			fz_begin_group(csi->dev, bbox, 0, 0, gstate->blendmode, 1);
		fz_clip_image_mask(csi->dev, image->mask, gstate->ctm);
	}
	else
		pdf_begin_group(csi, bbox);

	if (!image->colorspace)
	{

		switch (gstate->fill.kind)
		{
		case PDF_MAT_NONE:
			break;
		case PDF_MAT_COLOR:
			fz_fill_image_mask(csi->dev, image, gstate->ctm,
				gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MAT_PATTERN:
			if (gstate->fill.pattern)
			{
				fz_clip_image_mask(csi->dev, image, gstate->ctm);
				pdf_show_pattern(csi, gstate->fill.pattern, bbox, PDF_FILL);
				fz_pop_clip(csi->dev);
			}
			break;
		case PDF_MAT_SHADE:
			if (gstate->fill.shade)
			{
				fz_clip_image_mask(csi->dev, image, gstate->ctm);
				fz_fill_shade(csi->dev, gstate->fill.shade, gstate->ctm, gstate->fill.alpha);
				fz_pop_clip(csi->dev);
			}
			break;
		}
	}
	else
	{
		fz_fill_image(csi->dev, image, gstate->ctm, gstate->fill.alpha);
	}

	if (image->mask)
	{
		fz_pop_clip(csi->dev);
		if (gstate->blendmode != FZ_BLEND_NORMAL)
			fz_end_group(csi->dev);
	}
	else
		pdf_end_group(csi);
}

void
pdf_show_path(pdf_csi *csi, int doclose, int dofill, int dostroke, int even_odd)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	fz_path *path;
	fz_rect bbox;

	path = csi->path;
	csi->path = fz_new_path();

	if (doclose)
		fz_closepath(path);

	if (dostroke)
		bbox = fz_bound_path(path, &gstate->stroke_state, gstate->ctm);
	else
		bbox = fz_bound_path(path, NULL, gstate->ctm);

	if (csi->clip)
	{
		gstate->clip_depth++;
		fz_clip_path(csi->dev, path, even_odd, gstate->ctm);
		csi->clip = 0;
	}

	pdf_begin_group(csi, bbox);

	if (dofill)
	{
		switch (gstate->fill.kind)
		{
		case PDF_MAT_NONE:
			break;
		case PDF_MAT_COLOR:
			fz_fill_path(csi->dev, path, even_odd, gstate->ctm,
				gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MAT_PATTERN:
			if (gstate->fill.pattern)
			{
				fz_clip_path(csi->dev, path, even_odd, gstate->ctm);
				pdf_show_pattern(csi, gstate->fill.pattern, bbox, PDF_FILL);
				fz_pop_clip(csi->dev);
			}
			break;
		case PDF_MAT_SHADE:
			if (gstate->fill.shade)
			{
				fz_clip_path(csi->dev, path, even_odd, gstate->ctm);
				fz_fill_shade(csi->dev, gstate->fill.shade, csi->top_ctm, gstate->fill.alpha);
				fz_pop_clip(csi->dev);
			}
			break;
		}
	}

	if (dostroke)
	{
		switch (gstate->stroke.kind)
		{
		case PDF_MAT_NONE:
			break;
		case PDF_MAT_COLOR:
			fz_stroke_path(csi->dev, path, &gstate->stroke_state, gstate->ctm,
				gstate->stroke.colorspace, gstate->stroke.v, gstate->stroke.alpha);
			break;
		case PDF_MAT_PATTERN:
			if (gstate->stroke.pattern)
			{
				fz_clip_stroke_path(csi->dev, path, &gstate->stroke_state, gstate->ctm);
				pdf_show_pattern(csi, gstate->stroke.pattern, bbox, PDF_FILL);
				fz_pop_clip(csi->dev);
			}
			break;
		case PDF_MAT_SHADE:
			if (gstate->stroke.shade)
			{
				fz_clip_stroke_path(csi->dev, path, &gstate->stroke_state, gstate->ctm);
				fz_fill_shade(csi->dev, gstate->stroke.shade, csi->top_ctm, gstate->stroke.alpha);
				fz_pop_clip(csi->dev);
			}
			break;
		}
	}

	pdf_end_group(csi);

	fz_free_path(path);
}

void
pdf_flush_text(pdf_csi *csi)
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
	csi->text = NULL;

	dofill = dostroke = doclip = doinvisible = 0;
	switch (csi->text_mode)
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

	bbox = fz_bound_text(text, gstate->ctm);

	pdf_begin_group(csi, bbox);

	if (doinvisible)
		fz_ignore_text(csi->dev, text, gstate->ctm);

	if (doclip)
	{
		if (csi->accumulate < 2)
			gstate->clip_depth++;
		fz_clip_text(csi->dev, text, gstate->ctm, csi->accumulate);
		csi->accumulate = 2;
	}

	if (dofill)
	{
		switch (gstate->fill.kind)
		{
		case PDF_MAT_NONE:
			break;
		case PDF_MAT_COLOR:
			fz_fill_text(csi->dev, text, gstate->ctm,
				gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MAT_PATTERN:
			if (gstate->fill.pattern)
			{
				fz_clip_text(csi->dev, text, gstate->ctm, 0);
				pdf_show_pattern(csi, gstate->fill.pattern, bbox, PDF_FILL);
				fz_pop_clip(csi->dev);
			}
			break;
		case PDF_MAT_SHADE:
			if (gstate->fill.shade)
			{
				fz_clip_text(csi->dev, text, gstate->ctm, 0);
				fz_fill_shade(csi->dev, gstate->fill.shade, csi->top_ctm, gstate->fill.alpha);
				fz_pop_clip(csi->dev);
			}
			break;
		}
	}

	if (dostroke)
	{
		switch (gstate->stroke.kind)
		{
		case PDF_MAT_NONE:
			break;
		case PDF_MAT_COLOR:
			fz_stroke_text(csi->dev, text, &gstate->stroke_state, gstate->ctm,
				gstate->stroke.colorspace, gstate->stroke.v, gstate->stroke.alpha);
			break;
		case PDF_MAT_PATTERN:
			if (gstate->stroke.pattern)
			{
				fz_clip_stroke_text(csi->dev, text, &gstate->stroke_state, gstate->ctm);
				pdf_show_pattern(csi, gstate->stroke.pattern, bbox, PDF_FILL);
				fz_pop_clip(csi->dev);
			}
			break;
		case PDF_MAT_SHADE:
			if (gstate->stroke.shade)
			{
				fz_clip_stroke_text(csi->dev, text, &gstate->stroke_state, gstate->ctm);
				fz_fill_shade(csi->dev, gstate->stroke.shade, csi->top_ctm, gstate->stroke.alpha);
				fz_pop_clip(csi->dev);
			}
			break;
		}
	}

	pdf_end_group(csi);

	fz_free_text(text);
}

static void
pdf_show_glyph(pdf_csi *csi, int cid)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	pdf_font_desc *fontdesc = gstate->font;
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
	if (fontdesc->to_unicode)
		ucslen = pdf_lookup_cmap_full(fontdesc->to_unicode, cid, ucsbuf);
	if (ucslen == 0 && cid < fontdesc->cid_to_ucs_len)
	{
		ucsbuf[0] = fontdesc->cid_to_ucs[cid];
		ucslen = 1;
	}
	if (ucslen == 0 || (ucslen == 1 && ucsbuf[0] == 0))
	{
		ucsbuf[0] = '?';
		ucslen = 1;
	}

	gid = pdf_font_cid_to_gid(fontdesc, cid);

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1149 */
	if (fontdesc->wmode == 1 && fontdesc->font->ft_face)
		gid = pdf_ft_get_vgid(fontdesc, gid);

	if (fontdesc->wmode == 1)
	{
		v = pdf_get_vmtx(fontdesc, cid);
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
		gstate->render != csi->text_mode)
	{
		pdf_flush_text(csi);

		csi->text = fz_new_text(fontdesc->font, trm, fontdesc->wmode);
		csi->text->trm.e = 0;
		csi->text->trm.f = 0;
		csi->text_mode = gstate->render;
	}

	/* add glyph to textobject */
	fz_add_text(csi->text, gid, ucsbuf[0], trm.e, trm.f);

	/* add filler glyphs for one-to-many unicode mapping */
	for (i = 1; i < ucslen; i++)
		fz_add_text(csi->text, -1, ucsbuf[i], trm.e, trm.f);

	if (fontdesc->wmode == 0)
	{
		h = pdf_get_hmtx(fontdesc, cid);
		w0 = h.w * 0.001f;
		tx = (w0 * gstate->size + gstate->char_space) * gstate->scale;
		csi->tm = fz_concat(fz_translate(tx, 0), csi->tm);
	}

	if (fontdesc->wmode == 1)
	{
		w1 = v.w * 0.001f;
		ty = w1 * gstate->size + gstate->char_space;
		csi->tm = fz_concat(fz_translate(0, ty), csi->tm);
	}
}

void
pdf_show_space(pdf_csi *csi, float tadj)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	pdf_font_desc *fontdesc = gstate->font;

	if (!fontdesc)
	{
		fz_warn("cannot draw text since font and size not set");
		return;
	}

	if (fontdesc->wmode == 0)
		csi->tm = fz_concat(fz_translate(tadj * gstate->scale, 0), csi->tm);
	else
		csi->tm = fz_concat(fz_translate(0, tadj), csi->tm);
}

void
pdf_show_string(pdf_csi *csi, unsigned char *buf, int len)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	pdf_font_desc *fontdesc = gstate->font;
	unsigned char *end = buf + len;
	int cpt, cid;

	if (!fontdesc)
	{
		fz_warn("cannot draw text since font and size not set");
		return;
	}

	while (buf < end)
	{
		buf = pdf_decode_cmap(fontdesc->encoding, buf, &cpt);
		cid = pdf_lookup_cmap(fontdesc->encoding, cpt);
		if (cid >= 0)
			pdf_show_glyph(csi, cid);
		else
			fz_warn("cannot encode character with code point %#x", cpt);
		if (cpt == 32)
			pdf_show_space(csi, gstate->word_space);
	}
}

void
pdf_show_text(pdf_csi *csi, fz_obj *text)
{
	pdf_gstate *gstate = csi->gstate + csi->gtop;
	int i;

	if (fz_is_array(text))
	{
		for (i = 0; i < fz_array_len(text); i++)
		{
			fz_obj *item = fz_array_get(text, i);
			if (fz_is_string(item))
				pdf_show_string(csi, (unsigned char *)fz_to_str_buf(item), fz_to_str_len(item));
			else
				pdf_show_space(csi, - fz_to_real(item) * gstate->size * 0.001f);
		}
	}
	else if (fz_is_string(text))
	{
		pdf_show_string(csi, (unsigned char *)fz_to_str_buf(text), fz_to_str_len(text));
	}
}
