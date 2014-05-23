#include "pdf-interpret-imp.h"

#define TILE

/*
 * Emit graphics calls to device.
 */

typedef struct pdf_material_s pdf_material;

enum
{
	PDF_FILL,
	PDF_STROKE,
};

enum
{
	PDF_MAT_NONE,
	PDF_MAT_COLOR,
	PDF_MAT_PATTERN,
	PDF_MAT_SHADE,
};

struct pdf_material_s
{
	int kind;
	fz_colorspace *colorspace;
	pdf_pattern *pattern;
	fz_shade *shade;
	int gstate_num;
	float alpha;
	float v[FZ_MAX_COLORS];
};

struct pdf_gstate_s
{
	fz_matrix ctm;
	int clip_depth;

	/* path stroking */
	fz_stroke_state *stroke_state;

	/* materials */
	pdf_material stroke;
	pdf_material fill;

	/* text state */
	float char_space;
	float word_space;
	float scale;
	float leading;
	pdf_font_desc *font;
	float size;
	int render;
	float rise;

	/* transparency */
	int blendmode;
	pdf_xobject *softmask;
	fz_matrix softmask_ctm;
	float softmask_bc[FZ_MAX_COLORS];
	int luminosity;

	/* SumatraPDF: support transfer functions */
	fz_transfer_function *tr;
	fz_transfer_function *softmask_tr;
};

typedef struct pdf_run_state_s
{
	fz_context *ctx;
	fz_device *dev;
	pdf_csi *csi;

	int nested_depth;
	int in_hidden_ocg;

	/* path object state */
	fz_path *path;
	int clip;
	int clip_even_odd;
	const char *event;

	/* text object state */
	fz_text *text;
	fz_rect text_bbox;
	fz_matrix tlm;
	fz_matrix tm;
	int text_mode;
	int accumulate;

	/* graphics state */
	pdf_gstate *gstate;
	int gcap;
	int gtop;
	int gbot;
	int gparent;
}
pdf_run_state;

typedef struct softmask_save_s softmask_save;

struct softmask_save_s
{
	pdf_xobject *softmask;
	fz_matrix ctm;
};

static void
run_xobject(pdf_csi *csi, void *state, pdf_obj *resources, pdf_xobject *xobj, const fz_matrix *transform);

static int
ocg_intents_include(pdf_ocg_descriptor *desc, char *name)
{
	int i, len;

	if (strcmp(name, "All") == 0)
		return 1;

	/* In the absence of a specified intent, it's 'View' */
	if (!desc->intent)
		return (strcmp(name, "View") == 0);

	if (pdf_is_name(desc->intent))
	{
		char *intent = pdf_to_name(desc->intent);
		if (strcmp(intent, "All") == 0)
			return 1;
		return (strcmp(intent, name) == 0);
	}
	if (!pdf_is_array(desc->intent))
		return 0;

	len = pdf_array_len(desc->intent);
	for (i=0; i < len; i++)
	{
		char *intent = pdf_to_name(pdf_array_get(desc->intent, i));
		if (strcmp(intent, "All") == 0)
			return 1;
		if (strcmp(intent, name) == 0)
			return 1;
	}
	return 0;
}

int
pdf_is_hidden_ocg(pdf_obj *ocg, pdf_csi *csi, pdf_run_state *pr, pdf_obj *rdb)
{
	char event_state[16];
	pdf_obj *obj, *obj2;
	char *type;
	pdf_ocg_descriptor *desc = csi->doc->ocg;
	fz_context *ctx = csi->doc->ctx;

	/* Avoid infinite recursions */
	if (pdf_obj_marked(ocg))
		return 0;

	/* If no ocg descriptor, everything is visible */
	if (!desc)
		return 0;

	/* If we've been handed a name, look it up in the properties. */
	if (pdf_is_name(ocg))
	{
		ocg = pdf_dict_gets(pdf_dict_gets(rdb, "Properties"), pdf_to_name(ocg));
	}
	/* If we haven't been given an ocg at all, then we're visible */
	if (!ocg)
		return 0;

	fz_strlcpy(event_state, pr->event, sizeof event_state);
	fz_strlcat(event_state, "State", sizeof event_state);

	type = pdf_to_name(pdf_dict_gets(ocg, "Type"));

	if (strcmp(type, "OCG") == 0)
	{
		/* An Optional Content Group */
		int default_value = 0;
		int num = pdf_to_num(ocg);
		int gen = pdf_to_gen(ocg);
		int len = desc->len;
		int i;

		/* by default an OCG is visible, unless it's explicitly hidden */
		for (i = 0; i < len; i++)
		{
			if (desc->ocgs[i].num == num && desc->ocgs[i].gen == gen)
			{
				default_value = desc->ocgs[i].state == 0;
				break;
			}
		}

		/* Check Intents; if our intent is not part of the set given
		 * by the current config, we should ignore it. */
		obj = pdf_dict_gets(ocg, "Intent");
		if (pdf_is_name(obj))
		{
			/* If it doesn't match, it's hidden */
			if (ocg_intents_include(desc, pdf_to_name(obj)) == 0)
				return 1;
		}
		else if (pdf_is_array(obj))
		{
			int match = 0;
			len = pdf_array_len(obj);
			for (i=0; i<len; i++) {
				match |= ocg_intents_include(desc, pdf_to_name(pdf_array_get(obj, i)));
				if (match)
					break;
			}
			/* If we don't match any, it's hidden */
			if (match == 0)
				return 1;
		}
		else
		{
			/* If it doesn't match, it's hidden */
			if (ocg_intents_include(desc, "View") == 0)
				return 1;
		}

		/* FIXME: Currently we do a very simple check whereby we look
		 * at the Usage object (an Optional Content Usage Dictionary)
		 * and check to see if the corresponding 'event' key is on
		 * or off.
		 *
		 * Really we should only look at Usage dictionaries that
		 * correspond to entries in the AS list in the OCG config.
		 * Given that we don't handle Zoom or User, or Language
		 * dicts, this is not really a problem. */
		obj = pdf_dict_gets(ocg, "Usage");
		if (!pdf_is_dict(obj))
			return default_value;
		/* FIXME: Should look at Zoom (and return hidden if out of
		 * max/min range) */
		/* FIXME: Could provide hooks to the caller to check if
		 * User is appropriate - if not return hidden. */
		obj2 = pdf_dict_gets(obj, pr->event);
		if (strcmp(pdf_to_name(pdf_dict_gets(obj2, event_state)), "OFF") == 0)
		{
			return 1;
		}
		if (strcmp(pdf_to_name(pdf_dict_gets(obj2, event_state)), "ON") == 0)
		{
			return 0;
		}
		return default_value;
	}
	else if (strcmp(type, "OCMD") == 0)
	{
		/* An Optional Content Membership Dictionary */
		char *name;
		int combine, on;

		obj = pdf_dict_gets(ocg, "VE");
		if (pdf_is_array(obj)) {
			/* FIXME: Calculate visibility from array */
			return 0;
		}
		name = pdf_to_name(pdf_dict_gets(ocg, "P"));
		/* Set combine; Bit 0 set => AND, Bit 1 set => true means
		 * Off, otherwise true means On */
		if (strcmp(name, "AllOn") == 0)
		{
			combine = 1;
		}
		else if (strcmp(name, "AnyOff") == 0)
		{
			combine = 2;
		}
		else if (strcmp(name, "AllOff") == 0)
		{
			combine = 3;
		}
		else /* Assume it's the default (AnyOn) */
		{
			combine = 0;
		}

		if (pdf_mark_obj(ocg))
			return 0; /* Should never happen */
		fz_try(ctx)
		{
			obj = pdf_dict_gets(ocg, "OCGs");
			on = combine & 1;
			if (pdf_is_array(obj)) {
				int i, len;
				len = pdf_array_len(obj);
				for (i = 0; i < len; i++)
				{
					int hidden;
					hidden = pdf_is_hidden_ocg(pdf_array_get(obj, i), csi, pr, rdb);
					if ((combine & 1) == 0)
						hidden = !hidden;
					if (combine & 2)
						on &= hidden;
					else
						on |= hidden;
				}
			}
			else
			{
				on = pdf_is_hidden_ocg(obj, csi, pr, rdb);
				if ((combine & 1) == 0)
					on = !on;
			}
		}
		fz_always(ctx)
		{
			pdf_unmark_obj(ocg);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
		return !on;
	}
	/* No idea what sort of object this is - be visible */
	return 0;
}

/* SumatraPDF: support transfer functions */
static fz_transfer_function *
pdf_load_transfer_function(pdf_document *doc, pdf_obj *obj, int is_tr2)
{
	fz_context *ctx = doc->ctx;
	fz_transfer_function *tr;

	if (pdf_is_name(obj))
	{
		if (strcmp(pdf_to_name(obj), "Identity") && (!is_tr2 || strcmp(pdf_to_name(obj), "Default")))
			fz_throw(ctx, FZ_ERROR_GENERIC, "unknown transfer function %s", pdf_to_name(obj));
		return NULL;
	}

	tr = fz_malloc_struct(ctx, fz_transfer_function);
	FZ_INIT_STORABLE(tr, 1, fz_free);

	fz_try(ctx)
	{
		fz_function *func;
		float in, out;
		int i, n;

		if (pdf_is_array(obj))
		{
			for (n = 0; n < 4; n++)
			{
				pdf_obj *part = pdf_array_get(obj, n);
				func = pdf_load_function(doc, part, 1, 1);
				for (i = 0; i < 256; i++)
				{
					in = i / 255.0f;
					fz_eval_function(ctx, func, &in, 1, &out, 1);
					tr->function[n][i] = (int)(out * 255 + 0.5f);
				}
				fz_drop_function(ctx, func);
			}
		}
		else
		{
			func = pdf_load_function(doc, obj, 1, 1);
			for (i = 0; i < 256; i++)
			{
				in = i / 255.0f;
				fz_eval_function(ctx, func, &in, 1, &out, 1);
				for (n = 0; n < 4; n++)
					tr->function[n][i] = (int)(out * 255 + 0.5f);
			}
			fz_drop_function(ctx, func);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_transfer_function(ctx, tr);
		fz_rethrow(ctx);
	}

	return tr;
}

static pdf_gstate *
begin_softmask(pdf_csi *csi, pdf_run_state *pr, softmask_save *save)
{
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	pdf_xobject *softmask = gstate->softmask;
	fz_rect mask_bbox;
	fz_context *ctx;
	fz_matrix save_tm, save_tlm, save_ctm;
	/* SumatraPDF: support transfer functions */
	fz_transfer_function *tr = gstate->softmask_tr;

	save->softmask = softmask;
	if (softmask == NULL)
		return gstate;
	save->ctm = gstate->softmask_ctm;
	save_ctm = gstate->ctm;

	mask_bbox = softmask->bbox;
	ctx = pr->ctx;
	save_tm = pr->tm;
	save_tlm = pr->tlm;

	if (gstate->luminosity)
		mask_bbox = fz_infinite_rect;
	else
	{
		fz_transform_rect(&mask_bbox, &softmask->matrix);
		fz_transform_rect(&mask_bbox, &gstate->softmask_ctm);
	}
	gstate->softmask = NULL;
	gstate->ctm = gstate->softmask_ctm;
	/* SumatraPDF: support transfer functions */
	gstate->softmask_tr = NULL;

	fz_begin_mask(pr->dev, &mask_bbox, gstate->luminosity,
			softmask->colorspace, gstate->softmask_bc);
	fz_try(ctx)
	{
		run_xobject(csi, pr, csi->rdb, softmask, &fz_identity);
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		/* FIXME: Ignore error - nasty, but if we throw from
		 * here the clip stack would be messed up. */
		if (csi->cookie)
			csi->cookie->errors++;
	}

	/* SumatraPDF: support transfer functions */
	if (tr)
	{
		fz_apply_transfer_function(pr->dev, tr, 1);
		gstate = pr->gstate + pr->gtop;
		gstate->softmask_tr = tr;
	}

	fz_end_mask(pr->dev);

	pr->tm = save_tm;
	pr->tlm = save_tlm;

	gstate = pr->gstate + pr->gtop;
	gstate->ctm = save_ctm;

	return gstate;
}

static void
end_softmask(pdf_csi *csi, pdf_run_state *pr, softmask_save *save)
{
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	if (save->softmask == NULL)
		return;

	gstate->softmask = save->softmask;
	gstate->softmask_ctm = save->ctm;
	fz_pop_clip(pr->dev);
}

static pdf_gstate *
pdf_begin_group(pdf_csi *csi, pdf_run_state *pr, const fz_rect *bbox, softmask_save *softmask)
{
	pdf_gstate *gstate = begin_softmask(csi, pr, softmask);

	/* SumatraPDF: support transfer functions */
	if (gstate->blendmode || gstate->tr)
		fz_begin_group(pr->dev, bbox, 1, 0, gstate->blendmode, 1);

	return pr->gstate + pr->gtop;
}

static void
pdf_end_group(pdf_csi *csi, pdf_run_state *pr, softmask_save *softmask)
{
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	/* SumatraPDF: support transfer functions */
	if (gstate->tr)
		fz_apply_transfer_function(pr->dev, gstate->tr, 0);
	if (gstate->blendmode || gstate->tr)
		fz_end_group(pr->dev);

	end_softmask(csi, pr, softmask);
}

static void
pdf_show_shade(pdf_csi *csi, pdf_run_state *pr, fz_shade *shd)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	fz_rect bbox;
	softmask_save softmask = { NULL };

	if (pr->in_hidden_ocg > 0)
		return;

	fz_bound_shade(ctx, shd, &gstate->ctm, &bbox);

	gstate = pdf_begin_group(csi, pr, &bbox, &softmask);

	/* FIXME: The gstate->ctm in the next line may be wrong; maybe
	 * it should be the parent gstates ctm? */
	fz_fill_shade(pr->dev, shd, &gstate->ctm, gstate->fill.alpha);

	pdf_end_group(csi, pr, &softmask);
}

static pdf_material *
pdf_keep_material(fz_context *ctx, pdf_material *mat)
{
	if (mat->colorspace)
		fz_keep_colorspace(ctx, mat->colorspace);
	if (mat->pattern)
		pdf_keep_pattern(ctx, mat->pattern);
	if (mat->shade)
		fz_keep_shade(ctx, mat->shade);
	return mat;
}

static pdf_material *
pdf_drop_material(fz_context *ctx, pdf_material *mat)
{
	if (mat->colorspace)
		fz_drop_colorspace(ctx, mat->colorspace);
	if (mat->pattern)
		pdf_drop_pattern(ctx, mat->pattern);
	if (mat->shade)
		fz_drop_shade(ctx, mat->shade);
	return mat;
}

static void
pdf_copy_pattern_gstate(fz_context *ctx, pdf_gstate *gs, const pdf_gstate *old)
{
	gs->ctm = old->ctm;

	pdf_drop_font(ctx, gs->font);
	gs->font = pdf_keep_font(ctx, old->font);

	pdf_drop_xobject(ctx, gs->softmask);
	gs->softmask = pdf_keep_xobject(ctx, old->softmask);

	fz_drop_stroke_state(ctx, gs->stroke_state);
	gs->stroke_state = fz_keep_stroke_state(ctx, old->stroke_state);

	/* SumatraPDF: support transfer functions */
	fz_drop_transfer_function(ctx, gs->tr);
	gs->tr = fz_keep_transfer_function(ctx, old->tr);
	fz_drop_transfer_function(ctx, gs->softmask_tr);
	gs->softmask_tr = fz_keep_transfer_function(ctx, old->softmask_tr);
}

static void
pdf_unset_pattern(pdf_run_state *pr, int what)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gs = pr->gstate + pr->gtop;
	pdf_material *mat;
	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;
	if (mat->kind == PDF_MAT_PATTERN)
	{
		if (mat->pattern)
			pdf_drop_pattern(ctx, mat->pattern);
		mat->pattern = NULL;
		mat->kind = PDF_MAT_COLOR;
	}
}

static void
pdf_keep_gstate(fz_context *ctx, pdf_gstate *gs)
{
	pdf_keep_material(ctx, &gs->stroke);
	pdf_keep_material(ctx, &gs->fill);
	if (gs->font)
		pdf_keep_font(ctx, gs->font);
	if (gs->softmask)
		pdf_keep_xobject(ctx, gs->softmask);
	fz_keep_stroke_state(ctx, gs->stroke_state);

	/* SumatraPDF: support transfer functions */
	fz_keep_transfer_function(ctx, gs->tr);
	fz_keep_transfer_function(ctx, gs->softmask_tr);
}

static void
pdf_drop_gstate(fz_context *ctx, pdf_gstate *gs)
{
	pdf_drop_material(ctx, &gs->stroke);
	pdf_drop_material(ctx, &gs->fill);
	if (gs->font)
		pdf_drop_font(ctx, gs->font);
	if (gs->softmask)
		pdf_drop_xobject(ctx, gs->softmask);
	fz_drop_stroke_state(ctx, gs->stroke_state);

	/* SumatraPDF: support transfer functions */
	fz_drop_transfer_function(ctx, gs->tr);
	fz_drop_transfer_function(ctx, gs->softmask_tr);
}

static void
pdf_gsave(pdf_run_state *pr)
{
	fz_context *ctx = pr->ctx;

	if (pr->gtop == pr->gcap-1)
	{
		pr->gstate = fz_resize_array(ctx, pr->gstate, pr->gcap*2, sizeof(pdf_gstate));
		pr->gcap *= 2;
	}

	memcpy(&pr->gstate[pr->gtop + 1], &pr->gstate[pr->gtop], sizeof(pdf_gstate));

	pr->gtop++;
	pdf_keep_gstate(ctx, &pr->gstate[pr->gtop]);
}

static void
pdf_grestore(pdf_run_state *pr)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gs = pr->gstate + pr->gtop;
	int clip_depth = gs->clip_depth;

	if (pr->gtop <= pr->gbot)
	{
		fz_warn(ctx, "gstate underflow in content stream");
		return;
	}

	pdf_drop_gstate(ctx, gs);
	pr->gtop --;

	gs = pr->gstate + pr->gtop;
	while (clip_depth > gs->clip_depth)
	{
		fz_try(ctx)
		{
			fz_pop_clip(pr->dev);
		}
		fz_catch(ctx)
		{
			/* Silently swallow the problem - restores must
			 * never throw! */
		}
		clip_depth--;
	}
}

static void
pdf_show_pattern(pdf_csi *csi, pdf_run_state *pr, pdf_pattern *pat, pdf_gstate *pat_gstate, const fz_rect *area, int what)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate;
	int gparent_save;
	fz_matrix ptm, invptm, gparent_save_ctm;
	int x0, y0, x1, y1;
	float fx0, fy0, fx1, fy1;
	int oldtop;
	fz_rect local_area;

	pdf_gsave(pr);
	gstate = pr->gstate + pr->gtop;
	/* Patterns are run with the gstate of the parent */
	pdf_copy_pattern_gstate(ctx, gstate, pat_gstate);

	if (pat->ismask)
	{
		pdf_unset_pattern(pr, PDF_FILL);
		pdf_unset_pattern(pr, PDF_STROKE);
		if (what == PDF_FILL)
		{
			pdf_drop_material(ctx, &gstate->stroke);
			pdf_keep_material(ctx, &gstate->fill);
			gstate->stroke = gstate->fill;
		}
		if (what == PDF_STROKE)
		{
			pdf_drop_material(ctx, &gstate->fill);
			pdf_keep_material(ctx, &gstate->stroke);
			gstate->fill = gstate->stroke;
		}
	}
	else
	{
		// TODO: unset only the current fill/stroke or both?
		pdf_unset_pattern(pr, what);
	}

	/* don't apply soft masks to objects in the pattern as well */
	if (gstate->softmask)
	{
		pdf_drop_xobject(ctx, gstate->softmask);
		gstate->softmask = NULL;
	}

	fz_concat(&ptm, &pat->matrix, &pat_gstate->ctm);
	fz_invert_matrix(&invptm, &ptm);

	/* The parent_ctm is amended with our pattern matrix */
	gparent_save = pr->gparent;
	pr->gparent = pr->gtop-1;
	gparent_save_ctm = pr->gstate[pr->gparent].ctm;
	pr->gstate[pr->gparent].ctm = ptm;

	fz_try(ctx)
	{
		/* patterns are painted using the parent_ctm. area = bbox of
		 * shape to be filled in device space. Map it back to pattern
		 * space. */
		local_area = *area;
		fz_transform_rect(&local_area, &invptm);

		fx0 = (local_area.x0 - pat->bbox.x0) / pat->xstep;
		fy0 = (local_area.y0 - pat->bbox.y0) / pat->ystep;
		fx1 = (local_area.x1 - pat->bbox.x0) / pat->xstep;
		fy1 = (local_area.y1 - pat->bbox.y0) / pat->ystep;
		if (fx0 > fx1)
		{
			float t = fx0; fx0 = fx1; fx1 = t;
		}
		if (fy0 > fy1)
		{
			float t = fy0; fy0 = fy1; fy1 = t;
		}

		oldtop = pr->gtop;

#ifdef TILE
		/* We have tried various formulations in the past, but this one is
		 * best we've found; only use it as a tile if a whole repeat is
		 * required in at least one direction. Note, that this allows for
		 * 'sections' of 4 tiles to be show, but all non-overlapping. */
		if (fx1-fx0 > 1 || fy1-fy0 > 1)
#else
		if (0)
#endif
		{
			fz_begin_tile(pr->dev, &local_area, &pat->bbox, pat->xstep, pat->ystep, &ptm);
			gstate->ctm = ptm;
			pdf_gsave(pr);
			pdf_process_contents_object(csi, pat->resources, pat->contents);
			pdf_grestore(pr);
			while (oldtop < pr->gtop)
				pdf_grestore(pr);
			fz_end_tile(pr->dev);
		}
		else
		{
			int x, y;

			/* When calculating the number of tiles required, we adjust by
			 * a small amount to allow for rounding errors. By choosing
			 * this amount to be smaller than 1/256, we guarantee we won't
			 * cause problems that will be visible even under our most
			 * extreme antialiasing. */
			x0 = floorf(fx0 + 0.001);
			y0 = floorf(fy0 + 0.001);
			x1 = ceilf(fx1 - 0.001);
			y1 = ceilf(fy1 - 0.001);

			for (y = y0; y < y1; y++)
			{
				for (x = x0; x < x1; x++)
				{
					gstate->ctm = ptm;
					fz_pre_translate(&gstate->ctm, x * pat->xstep, y * pat->ystep);
					pdf_gsave(pr);
					fz_try(ctx)
					{
						pdf_process_contents_object(csi, pat->resources, pat->contents);
					}
					fz_always(ctx)
					{
						pdf_grestore(pr);
						while (oldtop < pr->gtop)
							pdf_grestore(pr);
					}
					fz_catch(ctx)
					{
						fz_rethrow_message(ctx, "cannot render pattern tile");
					}
				}
			}
		}
	}
	fz_always(ctx)
	{
		pr->gstate[pr->gparent].ctm = gparent_save_ctm;
		pr->gparent = gparent_save;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	pdf_grestore(pr);
}

static void
pdf_show_image(pdf_csi *csi, pdf_run_state *pr, fz_image *image)
{
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	fz_matrix image_ctm;
	fz_rect bbox;
	softmask_save softmask = { NULL };

	if (pr->in_hidden_ocg > 0)
		return;

	/* PDF has images bottom-up, so flip them right side up here */
	image_ctm = gstate->ctm;
	fz_pre_scale(fz_pre_translate(&image_ctm, 0, 1), 1, -1);

	bbox = fz_unit_rect;
	fz_transform_rect(&bbox, &image_ctm);

	if (image->mask)
	{
		/* apply blend group even though we skip the soft mask */
		if (gstate->blendmode)
			fz_begin_group(pr->dev, &bbox, 0, 0, gstate->blendmode, 1);
		fz_clip_image_mask(pr->dev, image->mask, &bbox, &image_ctm);
	}
	else
		gstate = pdf_begin_group(csi, pr, &bbox, &softmask);

	if (!image->colorspace)
	{

		switch (gstate->fill.kind)
		{
		case PDF_MAT_NONE:
			break;
		case PDF_MAT_COLOR:
			fz_fill_image_mask(pr->dev, image, &image_ctm,
				gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
			break;
		case PDF_MAT_PATTERN:
			if (gstate->fill.pattern)
			{
				fz_clip_image_mask(pr->dev, image, &bbox, &image_ctm);
				pdf_show_pattern(csi, pr, gstate->fill.pattern, &pr->gstate[gstate->fill.gstate_num], &bbox, PDF_FILL);
				fz_pop_clip(pr->dev);
			}
			break;
		case PDF_MAT_SHADE:
			if (gstate->fill.shade)
			{
				fz_clip_image_mask(pr->dev, image, &bbox, &image_ctm);
				fz_fill_shade(pr->dev, gstate->fill.shade, &pr->gstate[gstate->fill.gstate_num].ctm, gstate->fill.alpha);
				fz_pop_clip(pr->dev);
			}
			break;
		}
	}
	else
	{
		fz_fill_image(pr->dev, image, &image_ctm, gstate->fill.alpha);
	}

	if (image->mask)
	{
		fz_pop_clip(pr->dev);
		if (gstate->blendmode)
			fz_end_group(pr->dev);
	}
	else
		pdf_end_group(csi, pr, &softmask);
}

static void
pdf_show_path(pdf_csi *csi, pdf_run_state *pr, int doclose, int dofill, int dostroke, int even_odd)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	fz_path *path;
	fz_rect bbox;
	softmask_save softmask = { NULL };
	int knockout_group = 0;

	if (dostroke) {
		if (pr->dev->flags & (FZ_DEVFLAG_STROKECOLOR_UNDEFINED | FZ_DEVFLAG_LINEJOIN_UNDEFINED | FZ_DEVFLAG_LINEWIDTH_UNDEFINED))
			pr->dev->flags |= FZ_DEVFLAG_UNCACHEABLE;
		else if (gstate->stroke_state->dash_len != 0 && pr->dev->flags & (FZ_DEVFLAG_STARTCAP_UNDEFINED | FZ_DEVFLAG_DASHCAP_UNDEFINED | FZ_DEVFLAG_ENDCAP_UNDEFINED))
			pr->dev->flags |= FZ_DEVFLAG_UNCACHEABLE;
		else if (gstate->stroke_state->linejoin == FZ_LINEJOIN_MITER && (pr->dev->flags & FZ_DEVFLAG_MITERLIMIT_UNDEFINED))
			pr->dev->flags |= FZ_DEVFLAG_UNCACHEABLE;
	}
	if (dofill) {
		if (pr->dev->flags & FZ_DEVFLAG_FILLCOLOR_UNDEFINED)
			pr->dev->flags |= FZ_DEVFLAG_UNCACHEABLE;
	}

	path = pr->path;
	pr->path = fz_new_path(ctx);

	fz_try(ctx)
	{
		if (doclose)
			fz_closepath(ctx, path);

		fz_bound_path(ctx, path, (dostroke ? gstate->stroke_state : NULL), &gstate->ctm, &bbox);

		if (pr->clip)
		{
			gstate->clip_depth++;
			fz_clip_path(pr->dev, path, &bbox, pr->clip_even_odd, &gstate->ctm);
			pr->clip = 0;
		}

		if (pr->in_hidden_ocg > 0)
			dostroke = dofill = 0;

		if (dofill || dostroke)
			gstate = pdf_begin_group(csi, pr, &bbox, &softmask);

		/* SumatraPDF: prevent regression (e.g. in blend mode 10.pdf and annotations galore.pdf) */
		if (dofill && dostroke && 0)
		{
			/* We may need to push a knockout group */
			if (gstate->stroke.alpha == 0)
			{
				/* No need for group, as stroke won't do anything */
			}
			else if (gstate->stroke.alpha == 1.0f && gstate->blendmode == FZ_BLEND_NORMAL)
			{
				/* No need for group, as stroke won't show up */
			}
			else
			{
				knockout_group = 1;
				fz_begin_group(pr->dev, &bbox, 0, 1, FZ_BLEND_NORMAL, 1);
			}
		}

		if (dofill)
		{
			switch (gstate->fill.kind)
			{
			case PDF_MAT_NONE:
				break;
			case PDF_MAT_COLOR:
				/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=966 */
				if (path->coord_len == 4 && path->cmds[0] == FZ_MOVETO && path->cmds[1] == FZ_LINETO &&
					(path->coords[0] != path->coords[2] || path->coords[1] != path->coords[3]))
				{
					fz_stroke_state *stroke = fz_new_stroke_state(ctx);
					stroke->linewidth = 0.1f / fz_matrix_expansion(&gstate->ctm);
					fz_stroke_path(pr->dev, path, stroke, &gstate->ctm,
						gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
					fz_drop_stroke_state(ctx, stroke);
					break;
				}
				fz_fill_path(pr->dev, path, even_odd, &gstate->ctm,
					gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
				break;
			case PDF_MAT_PATTERN:
				if (gstate->fill.pattern)
				{
					fz_clip_path(pr->dev, path, &bbox, even_odd, &gstate->ctm);
					pdf_show_pattern(csi, pr, gstate->fill.pattern, &pr->gstate[gstate->fill.gstate_num], &bbox, PDF_FILL);
					fz_pop_clip(pr->dev);
				}
				break;
			case PDF_MAT_SHADE:
				if (gstate->fill.shade)
				{
					fz_clip_path(pr->dev, path, &bbox, even_odd, &gstate->ctm);
					/* The cluster and page 2 of patterns.pdf shows that fz_fill_shade should NOT be called with gstate->ctm. */
					fz_fill_shade(pr->dev, gstate->fill.shade, &pr->gstate[gstate->fill.gstate_num].ctm, gstate->fill.alpha);
					fz_pop_clip(pr->dev);
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
				fz_stroke_path(pr->dev, path, gstate->stroke_state, &gstate->ctm,
					gstate->stroke.colorspace, gstate->stroke.v, gstate->stroke.alpha);
				break;
			case PDF_MAT_PATTERN:
				if (gstate->stroke.pattern)
				{
					fz_clip_stroke_path(pr->dev, path, &bbox, gstate->stroke_state, &gstate->ctm);
					pdf_show_pattern(csi, pr, gstate->stroke.pattern, &pr->gstate[gstate->stroke.gstate_num], &bbox, PDF_STROKE);
					fz_pop_clip(pr->dev);
				}
				break;
			case PDF_MAT_SHADE:
				if (gstate->stroke.shade)
				{
					fz_clip_stroke_path(pr->dev, path, &bbox, gstate->stroke_state, &gstate->ctm);
					fz_fill_shade(pr->dev, gstate->stroke.shade, &pr->gstate[gstate->stroke.gstate_num].ctm, gstate->stroke.alpha);
					fz_pop_clip(pr->dev);
				}
				break;
			}
		}

		if (knockout_group)
			fz_end_group(pr->dev);

		if (dofill || dostroke)
			pdf_end_group(csi, pr, &softmask);
	}
	fz_always(ctx)
	{
		fz_free_path(ctx, path);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
 * Assemble and emit text
 */

static pdf_gstate *
pdf_flush_text(pdf_csi *csi, pdf_run_state *pr)
{
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	fz_text *text;
	int dofill;
	int dostroke;
	int doclip;
	int doinvisible;
	fz_context *ctx = pr->ctx;
	softmask_save softmask = { NULL };

	if (!pr->text)
		return gstate;
	text = pr->text;
	pr->text = NULL;

	dofill = dostroke = doclip = doinvisible = 0;
	switch (pr->text_mode)
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

	if (pr->in_hidden_ocg > 0)
		dostroke = dofill = 0;

	fz_try(ctx)
	{
		fz_rect tb = pr->text_bbox;

		fz_transform_rect(&tb, &gstate->ctm);

		/* Don't bother sending a text group with nothing in it */
		if (text->len == 0)
			break;

		gstate = pdf_begin_group(csi, pr, &tb, &softmask);

		if (doinvisible)
			fz_ignore_text(pr->dev, text, &gstate->ctm);

		if (dofill)
		{
			switch (gstate->fill.kind)
			{
			case PDF_MAT_NONE:
				break;
			case PDF_MAT_COLOR:
				fz_fill_text(pr->dev, text, &gstate->ctm,
					gstate->fill.colorspace, gstate->fill.v, gstate->fill.alpha);
				break;
			case PDF_MAT_PATTERN:
				if (gstate->fill.pattern)
				{
					fz_clip_text(pr->dev, text, &gstate->ctm, 0);
					pdf_show_pattern(csi, pr, gstate->fill.pattern, &pr->gstate[gstate->fill.gstate_num], &tb, PDF_FILL);
					fz_pop_clip(pr->dev);
				}
				break;
			case PDF_MAT_SHADE:
				if (gstate->fill.shade)
				{
					fz_clip_text(pr->dev, text, &gstate->ctm, 0);
					/* Page 2 of patterns.pdf shows that fz_fill_shade should NOT be called with gstate->ctm */
					fz_fill_shade(pr->dev, gstate->fill.shade, &pr->gstate[gstate->fill.gstate_num].ctm, gstate->fill.alpha);
					fz_pop_clip(pr->dev);
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
				fz_stroke_text(pr->dev, text, gstate->stroke_state, &gstate->ctm,
					gstate->stroke.colorspace, gstate->stroke.v, gstate->stroke.alpha);
				break;
			case PDF_MAT_PATTERN:
				if (gstate->stroke.pattern)
				{
					fz_clip_stroke_text(pr->dev, text, gstate->stroke_state, &gstate->ctm);
					pdf_show_pattern(csi, pr, gstate->stroke.pattern, &pr->gstate[gstate->stroke.gstate_num], &tb, PDF_STROKE);
					fz_pop_clip(pr->dev);
				}
				break;
			case PDF_MAT_SHADE:
				if (gstate->stroke.shade)
				{
					fz_clip_stroke_text(pr->dev, text, gstate->stroke_state, &gstate->ctm);
					fz_fill_shade(pr->dev, gstate->stroke.shade, &pr->gstate[gstate->stroke.gstate_num].ctm, gstate->stroke.alpha);
					fz_pop_clip(pr->dev);
				}
				break;
			}
		}

		if (doclip)
		{
			if (pr->accumulate < 2)
				gstate->clip_depth++;
			fz_clip_text(pr->dev, text, &gstate->ctm, pr->accumulate);
			pr->accumulate = 2;
		}

		pdf_end_group(csi, pr, &softmask);
	}
	fz_always(ctx)
	{
		fz_free_text(ctx, text);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return pr->gstate + pr->gtop;
}

static void
pdf_show_char(pdf_csi *csi, pdf_run_state *pr, int cid)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	pdf_font_desc *fontdesc = gstate->font;
	fz_matrix tsm, trm;
	float w0, w1, tx, ty;
	pdf_hmtx h;
	pdf_vmtx v;
	int gid;
	int ucsbuf[8];
	int ucslen;
	int i;
	fz_rect bbox;
	int render_direct;

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

	gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1149 */
	if (fontdesc->wmode == 1 && fontdesc->font->ft_face)
		gid = pdf_ft_lookup_vgid(ctx, fontdesc, gid);

	if (fontdesc->wmode == 1)
	{
		v = pdf_lookup_vmtx(ctx, fontdesc, cid);
		tsm.e -= v.x * fabsf(gstate->size) * 0.001f;
		tsm.f -= v.y * gstate->size * 0.001f;
	}

	fz_concat(&trm, &tsm, &pr->tm);

	fz_bound_glyph(ctx, fontdesc->font, gid, &trm, &bbox);
	/* Compensate for the glyph cache limited positioning precision */
	bbox.x0 -= 1;
	bbox.y0 -= 1;
	bbox.x1 += 1;
	bbox.y1 += 1;

	/* If we are a type3 font within a type 3 font, or are otherwise
	 * uncachable, then render direct. */
	render_direct = (!fontdesc->font->ft_face && pr->nested_depth > 0) || !fz_glyph_cacheable(ctx, fontdesc->font, gid);

	/* flush buffered text if face or matrix or rendermode has changed */
	if (!pr->text ||
		fontdesc->font != pr->text->font ||
		fontdesc->wmode != pr->text->wmode ||
		fabsf(trm.a - pr->text->trm.a) > FLT_EPSILON ||
		fabsf(trm.b - pr->text->trm.b) > FLT_EPSILON ||
		fabsf(trm.c - pr->text->trm.c) > FLT_EPSILON ||
		fabsf(trm.d - pr->text->trm.d) > FLT_EPSILON ||
		gstate->render != pr->text_mode ||
		render_direct)
	{
		gstate = pdf_flush_text(csi, pr);

		pr->text = fz_new_text(ctx, fontdesc->font, &trm, fontdesc->wmode);
		pr->text->trm.e = 0;
		pr->text->trm.f = 0;
		pr->text_mode = gstate->render;
		pr->text_bbox = fz_empty_rect;
	}

	if (render_direct)
	{
		/* Render the glyph stream direct here (only happens for
		 * type3 glyphs that seem to inherit current graphics
		 * attributes, or type 3 glyphs within type3 glyphs). */
		fz_matrix composed;
		fz_concat(&composed, &trm, &gstate->ctm);
		fz_render_t3_glyph_direct(ctx, pr->dev, fontdesc->font, gid, &composed, gstate, pr->nested_depth);
	}
	/* SumatraPDF: still allow text extraction */
	if (render_direct)
		pr->text_mode = 3 /* invisible */;
	{
		fz_union_rect(&pr->text_bbox, &bbox);

		/* add glyph to textobject */
		fz_add_text(ctx, pr->text, gid, ucsbuf[0], trm.e, trm.f);

		/* add filler glyphs for one-to-many unicode mapping */
		for (i = 1; i < ucslen; i++)
			fz_add_text(ctx, pr->text, -1, ucsbuf[i], trm.e, trm.f);
	}

	if (fontdesc->wmode == 0)
	{
		h = pdf_lookup_hmtx(ctx, fontdesc, cid);
		w0 = h.w * 0.001f;
		tx = (w0 * gstate->size + gstate->char_space) * gstate->scale;
		fz_pre_translate(&pr->tm, tx, 0);
	}

	if (fontdesc->wmode == 1)
	{
		w1 = v.w * 0.001f;
		ty = w1 * gstate->size + gstate->char_space;
		fz_pre_translate(&pr->tm, 0, ty);
	}
}

static void
pdf_show_space(pdf_run_state *pr, float tadj)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	pdf_font_desc *fontdesc = gstate->font;

	if (!fontdesc)
	{
		fz_warn(ctx, "cannot draw text since font and size not set");
		return;
	}

	if (fontdesc->wmode == 0)
		fz_pre_translate(&pr->tm, tadj * gstate->scale, 0);
	else
		fz_pre_translate(&pr->tm, 0, tadj);
}

static void
pdf_show_string(pdf_csi *csi, pdf_run_state *pr, unsigned char *buf, int len)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	pdf_font_desc *fontdesc = gstate->font;
	unsigned char *end = buf + len;
	unsigned int cpt;
	int cid;

	if (!fontdesc)
	{
		fz_warn(ctx, "cannot draw text since font and size not set");
		return;
	}

	while (buf < end)
	{
		int w = pdf_decode_cmap(fontdesc->encoding, buf, end, &cpt);
		buf += w;

		cid = pdf_lookup_cmap(fontdesc->encoding, cpt);
		/* cf. https://code.google.com/p/sumatrapdf/issues/detail?id=2286 */
		if (w == 1 && (cpt == 10 || cpt == 13) && !pdf_font_cid_to_gid(ctx, fontdesc, cid))
			fz_warn(ctx, "ignoring line break in string");
		else
		if (cid >= 0)
			pdf_show_char(csi, pr, cid);
		else
			fz_warn(ctx, "cannot encode character");
		if (cpt == 32 && w == 1)
			pdf_show_space(pr, gstate->word_space);
	}
}

static void
pdf_show_text(pdf_csi *csi, pdf_run_state *pr, pdf_obj *text)
{
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	int i;

	if (pdf_is_array(text))
	{
		int n = pdf_array_len(text);
		for (i = 0; i < n; i++)
		{
			pdf_obj *item = pdf_array_get(text, i);
			if (pdf_is_string(item))
				pdf_show_string(csi, pr, (unsigned char *)pdf_to_str_buf(item), pdf_to_str_len(item));
			else
				pdf_show_space(pr, - pdf_to_real(item) * gstate->size * 0.001f);
		}
	}
	else if (pdf_is_string(text))
	{
		pdf_show_string(csi, pr, (unsigned char *)pdf_to_str_buf(text), pdf_to_str_len(text));
	}
}

/*
 * Interpreter and graphics state stack.
 */

static void
pdf_init_gstate(fz_context *ctx, pdf_gstate *gs, const fz_matrix *ctm)
{
	gs->ctm = *ctm;
	gs->clip_depth = 0;

	gs->stroke_state = fz_new_stroke_state(ctx);

	gs->stroke.kind = PDF_MAT_COLOR;
	gs->stroke.colorspace = fz_device_gray(ctx); /* No fz_keep_colorspace as static */
	gs->stroke.v[0] = 0;
	gs->stroke.pattern = NULL;
	gs->stroke.shade = NULL;
	gs->stroke.alpha = 1;
	gs->stroke.gstate_num = -1;

	gs->fill.kind = PDF_MAT_COLOR;
	gs->fill.colorspace = fz_device_gray(ctx); /* No fz_keep_colorspace as static */
	gs->fill.v[0] = 0;
	gs->fill.pattern = NULL;
	gs->fill.shade = NULL;
	gs->fill.alpha = 1;
	gs->fill.gstate_num = -1;

	gs->char_space = 0;
	gs->word_space = 0;
	gs->scale = 1;
	gs->leading = 0;
	gs->font = NULL;
	gs->size = -1;
	gs->render = 0;
	gs->rise = 0;

	gs->blendmode = 0;
	gs->softmask = NULL;
	gs->softmask_ctm = fz_identity;
	gs->luminosity = 0;

	/* SumatraPDF: support transfer functions */
	gs->tr = NULL;
	gs->softmask_tr = NULL;
}

static void
pdf_copy_gstate(fz_context *ctx, pdf_gstate *gs, pdf_gstate *old)
{
	pdf_drop_gstate(ctx, gs);
	*gs = *old;
	pdf_keep_gstate(ctx, gs);
}

/*
 * Material state
 */

static void
pdf_set_colorspace(pdf_csi *csi, pdf_run_state *pr, int what, fz_colorspace *colorspace)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gs;
	pdf_material *mat;

	gs = pdf_flush_text(csi, pr);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	fz_drop_colorspace(ctx, mat->colorspace);

	mat->kind = PDF_MAT_COLOR;
	mat->colorspace = fz_keep_colorspace(ctx, colorspace);

	mat->v[0] = 0;
	mat->v[1] = 0;
	mat->v[2] = 0;
	mat->v[3] = 1;

	if (pdf_is_tint_colorspace(colorspace))
	{
		int i;
		for (i = 0; i < colorspace->n; i++)
			mat->v[i] = 1.0f;
	}
}

static void
pdf_set_color(pdf_csi *csi, pdf_run_state *pr, int what, float *v)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gs;
	pdf_material *mat;
	int i;

	gs = pdf_flush_text(csi, pr);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	switch (mat->kind)
	{
	case PDF_MAT_PATTERN:
	case PDF_MAT_COLOR:
		if (fz_colorspace_is_indexed(mat->colorspace))
		{
			mat->v[0] = v[0] / 255;
			break;
		}
		for (i = 0; i < mat->colorspace->n; i++)
			mat->v[i] = v[i];
		break;
	default:
		fz_warn(ctx, "color incompatible with material");
	}
}

static void
pdf_set_shade(pdf_csi *csi, pdf_run_state *pr, int what, fz_shade *shade)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gs;
	pdf_material *mat;

	gs = pdf_flush_text(csi, pr);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	if (mat->shade)
		fz_drop_shade(ctx, mat->shade);

	mat->kind = PDF_MAT_SHADE;
	mat->shade = fz_keep_shade(ctx, shade);
}

static void
pdf_set_pattern(pdf_csi *csi, pdf_run_state *pr, int what, pdf_pattern *pat, float *v)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gs;
	pdf_material *mat;

	gs = pdf_flush_text(csi, pr);

	mat = what == PDF_FILL ? &gs->fill : &gs->stroke;

	if (mat->pattern)
		pdf_drop_pattern(ctx, mat->pattern);

	mat->kind = PDF_MAT_PATTERN;
	if (pat)
		mat->pattern = pdf_keep_pattern(ctx, pat);
	else
		mat->pattern = NULL;
	mat->gstate_num = pr->gparent;

	if (v)
		pdf_set_color(csi, pr, what, v);
}

static pdf_font_desc *
load_font_or_hail_mary(pdf_csi *csi, pdf_obj *rdb, pdf_obj *font, int depth)
{
	pdf_document *doc = csi->doc;
	fz_context *ctx = doc->ctx;
	pdf_font_desc *desc;

	fz_try(ctx)
	{
		desc = pdf_load_font(doc, rdb, font, depth);
	}
	fz_catch(ctx)
	{
		if (fz_caught(ctx) != FZ_ERROR_TRYLATER)
			fz_rethrow(ctx);
		if (!csi->cookie || !csi->cookie->incomplete_ok)
			fz_rethrow(ctx);
		desc = NULL;
		csi->cookie->incomplete++;
	}
	if (desc == NULL)
		desc = pdf_load_hail_mary_font(doc);
	return desc;
}

static void
pdf_run_extgstate(pdf_csi *csi, pdf_run_state *pr, pdf_obj *rdb, pdf_obj *extgstate)
{
	fz_context *ctx = pr->ctx;
	pdf_gstate *gstate;
	fz_colorspace *colorspace;
	int i, k, n;

	gstate = pdf_flush_text(csi, pr);

	n = pdf_dict_len(extgstate);
	for (i = 0; i < n; i++)
	{
		pdf_obj *key = pdf_dict_get_key(extgstate, i);
		pdf_obj *val = pdf_dict_get_val(extgstate, i);
		char *s = pdf_to_name(key);

		if (!strcmp(s, "Font"))
		{
			if (pdf_is_array(val) && pdf_array_len(val) == 2)
			{
				pdf_obj *font = pdf_array_get(val, 0);

				if (gstate->font)
				{
					pdf_drop_font(ctx, gstate->font);
					gstate->font = NULL;
				}

				gstate->font = load_font_or_hail_mary(csi, rdb, font, pr->nested_depth);
				if (!gstate->font)
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find font in store");
				gstate->size = pdf_to_real(pdf_array_get(val, 1));
			}
			else
				fz_throw(ctx, FZ_ERROR_GENERIC, "malformed /Font dictionary");
		}

		else if (!strcmp(s, "LC"))
		{
			pr->dev->flags &= ~(FZ_DEVFLAG_STARTCAP_UNDEFINED | FZ_DEVFLAG_DASHCAP_UNDEFINED | FZ_DEVFLAG_ENDCAP_UNDEFINED);
			gstate->stroke_state = fz_unshare_stroke_state(ctx, gstate->stroke_state);
			gstate->stroke_state->start_cap = pdf_to_int(val);
			gstate->stroke_state->dash_cap = pdf_to_int(val);
			gstate->stroke_state->end_cap = pdf_to_int(val);
		}
		else if (!strcmp(s, "LW"))
		{
			pr->dev->flags &= ~FZ_DEVFLAG_LINEWIDTH_UNDEFINED;
			gstate->stroke_state = fz_unshare_stroke_state(ctx, gstate->stroke_state);
			gstate->stroke_state->linewidth = pdf_to_real(val);
		}
		else if (!strcmp(s, "LJ"))
		{
			pr->dev->flags &= ~FZ_DEVFLAG_LINEJOIN_UNDEFINED;
			gstate->stroke_state = fz_unshare_stroke_state(ctx, gstate->stroke_state);
			gstate->stroke_state->linejoin = pdf_to_int(val);
		}
		else if (!strcmp(s, "ML"))
		{
			pr->dev->flags &= ~FZ_DEVFLAG_MITERLIMIT_UNDEFINED;
			gstate->stroke_state = fz_unshare_stroke_state(ctx, gstate->stroke_state);
			gstate->stroke_state->miterlimit = pdf_to_real(val);
		}

		else if (!strcmp(s, "D"))
		{
			if (pdf_is_array(val) && pdf_array_len(val) == 2)
			{
				pdf_obj *dashes = pdf_array_get(val, 0);
				int len = pdf_array_len(dashes);
				gstate->stroke_state = fz_unshare_stroke_state_with_dash_len(ctx, gstate->stroke_state, len);
				gstate->stroke_state->dash_len = len;
				for (k = 0; k < len; k++)
					gstate->stroke_state->dash_list[k] = pdf_to_real(pdf_array_get(dashes, k));
				gstate->stroke_state->dash_phase = pdf_to_real(pdf_array_get(val, 1));
			}
			else
				fz_throw(ctx, FZ_ERROR_GENERIC, "malformed /D");
		}

		else if (!strcmp(s, "CA"))
			gstate->stroke.alpha = fz_clamp(pdf_to_real(val), 0, 1);

		else if (!strcmp(s, "ca"))
			gstate->fill.alpha = fz_clamp(pdf_to_real(val), 0, 1);

		else if (!strcmp(s, "BM"))
		{
			if (pdf_is_array(val))
			{
				/* SumatraPDF: properly support /BM arrays */
				for (k = 0; k < pdf_array_len(val); k++)
				{
					char *bm = pdf_to_name(pdf_array_get(val, k));
					if (!strcmp(bm, "Normal") || fz_lookup_blendmode(bm) > 0)
						break;
				}
				val = pdf_array_get(val, k);
			}
			gstate->blendmode = fz_lookup_blendmode(pdf_to_name(val));
		}

		else if (!strcmp(s, "SMask"))
		{
			if (pdf_is_dict(val))
			{
				pdf_xobject *xobj;
				pdf_obj *group, *luminosity, *bc, *tr;

				if (gstate->softmask)
				{
					pdf_drop_xobject(ctx, gstate->softmask);
					gstate->softmask = NULL;
				}
				/* SumatraPDF: support transfer functions */
				if (gstate->softmask_tr)
				{
					fz_drop_transfer_function(ctx, gstate->softmask_tr);
					gstate->softmask_tr = NULL;
				}

				group = pdf_dict_gets(val, "G");
				if (!group)
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot load softmask xobject (%d %d R)", pdf_to_num(val), pdf_to_gen(val));
				xobj = pdf_load_xobject(csi->doc, group);

				colorspace = xobj->colorspace;
				if (!colorspace)
					colorspace = fz_device_gray(ctx);

				/* The softmask_ctm no longer has the softmask matrix rolled into it, as this
				 * causes the softmask matrix to be applied twice. */
				gstate->softmask_ctm = gstate->ctm;
				gstate->softmask = xobj;
				for (k = 0; k < colorspace->n; k++)
					gstate->softmask_bc[k] = 0;

				bc = pdf_dict_gets(val, "BC");
				if (pdf_is_array(bc))
				{
					for (k = 0; k < colorspace->n; k++)
						gstate->softmask_bc[k] = pdf_to_real(pdf_array_get(bc, k));
				}

				luminosity = pdf_dict_gets(val, "S");
				if (pdf_is_name(luminosity) && !strcmp(pdf_to_name(luminosity), "Luminosity"))
					gstate->luminosity = 1;
				else
					gstate->luminosity = 0;

				tr = pdf_dict_gets(val, "TR");
				/* SumatraPDF: support transfer functions */
				if (tr)
					gstate->softmask_tr = pdf_load_transfer_function(csi->doc, tr, 0);
			}
			else if (pdf_is_name(val) && !strcmp(pdf_to_name(val), "None"))
			{
				if (gstate->softmask)
				{
					pdf_drop_xobject(ctx, gstate->softmask);
					gstate->softmask = NULL;
				}
			}
		}

		/* SumatraPDF: support transfer functions */
		else if ((!strcmp(s, "TR") && !pdf_dict_gets(extgstate, "TR2")) || !strcmp(s, "TR2"))
		{
			fz_drop_transfer_function(ctx, gstate->tr);
			gstate->tr = NULL;
			gstate->tr = pdf_load_transfer_function(csi->doc, val, !strcmp(s, "TR2"));
		}
	}
}

static void
run_xobject(pdf_csi *csi, void *state, pdf_obj *resources, pdf_xobject *xobj, const fz_matrix *transform)
{
	fz_context *ctx = csi->doc->ctx;
	pdf_gstate *gstate = NULL;
	int oldtop = 0;
	fz_matrix local_transform = *transform;
	softmask_save softmask = { NULL };
	int gparent_save;
	fz_matrix gparent_save_ctm;
	pdf_run_state *pr = (pdf_run_state *)state;

	/* Avoid infinite recursion */
	if (xobj == NULL || pdf_mark_obj(xobj->me))
		return;

	fz_var(gstate);
	fz_var(oldtop);

	gparent_save = pr->gparent;
	pr->gparent = pr->gtop;

	fz_try(ctx)
	{
		pdf_gsave(pr);

		gstate = pr->gstate + pr->gtop;
		oldtop = pr->gtop;

		/* apply xobject's transform matrix */
		fz_concat(&local_transform, &xobj->matrix, &local_transform);
		fz_concat(&gstate->ctm, &local_transform, &gstate->ctm);

		/* The gparent is updated with the modified ctm */
		gparent_save_ctm = pr->gstate[pr->gparent].ctm;
		pr->gstate[pr->gparent].ctm = gstate->ctm;

		/* apply soft mask, create transparency group and reset state */
		if (xobj->transparency)
		{
			fz_rect bbox = xobj->bbox;
			fz_transform_rect(&bbox, &gstate->ctm);
			gstate = begin_softmask(csi, pr, &softmask);

			fz_begin_group(pr->dev, &bbox,
				xobj->isolated, xobj->knockout, gstate->blendmode, gstate->fill.alpha);

			gstate->blendmode = 0;
			gstate->stroke.alpha = 1;
			gstate->fill.alpha = 1;
		}

		pdf_gsave(pr); /* Save here so the clippath doesn't persist */

		/* clip to the bounds */
		fz_moveto(ctx, pr->path, xobj->bbox.x0, xobj->bbox.y0);
		fz_lineto(ctx, pr->path, xobj->bbox.x1, xobj->bbox.y0);
		fz_lineto(ctx, pr->path, xobj->bbox.x1, xobj->bbox.y1);
		fz_lineto(ctx, pr->path, xobj->bbox.x0, xobj->bbox.y1);
		fz_closepath(ctx, pr->path);
		pr->clip = 1;
		pdf_show_path(csi, pr, 0, 0, 0, 0);

		/* run contents */

		if (xobj->resources)
			resources = xobj->resources;

		pdf_process_contents_object(csi, resources, xobj->contents);
	}
	fz_always(ctx)
	{
		pdf_grestore(pr); /* Remove the clippath */

		/* wrap up transparency stacks */
		if (xobj->transparency)
		{
			fz_end_group(pr->dev);
			end_softmask(csi, pr, &softmask);
		}

		pr->gstate[pr->gparent].ctm = gparent_save_ctm;
		pr->gparent = gparent_save;

		if (gstate)
		{
			while (oldtop < pr->gtop)
				pdf_grestore(pr);

			pdf_grestore(pr);
		}

		pdf_unmark_obj(xobj->me);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void pdf_run_BDC(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_obj *ocg;
	pdf_obj *rdb = csi->rdb;

	/* We only understand OC groups so far */
	if (strcmp(csi->name, "OC") != 0)
		return;

	/* If we are already in a hidden OCG, then we'll still be hidden -
	 * just increment the depth so we pop back to visibility when we've
	 * seen enough EDCs. */
	if (pr->in_hidden_ocg > 0)
	{
		pr->in_hidden_ocg++;
		return;
	}

	if (pdf_is_name(csi->obj))
	{
		ocg = pdf_dict_gets(pdf_dict_gets(rdb, "Properties"), pdf_to_name(csi->obj));
	}
	else
		ocg = csi->obj;

	if (!ocg)
	{
		/* No Properties array, or name not found in the properties
		 * means visible. */
		return;
	}
	if (strcmp(pdf_to_name(pdf_dict_gets(ocg, "Type")), "OCG") != 0)
	{
		/* Wrong type of property */
		return;
	}
	if (pdf_is_hidden_ocg(ocg, csi, pr, rdb))
		pr->in_hidden_ocg++;
}

static void pdf_run_BI(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_image(csi, pr, csi->img);
}

static void pdf_run_B(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 1, 1, 0);
}

static void pdf_run_BMC(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	/* If we are already in a hidden OCG, then we'll still be hidden -
	 * just increment the depth so we pop back to visibility when we've
	 * seen enough EDCs. */
	if (pr->in_hidden_ocg > 0)
	{
		pr->in_hidden_ocg++;
	}
}

static void pdf_run_BT(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->tm = fz_identity;
	pr->tlm = fz_identity;
}

static void pdf_run_BX(pdf_csi *csi, void *state)
{
}

static void pdf_run_Bstar(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 1, 1, 1);
}

static void pdf_run_cs_imp(pdf_csi *csi, pdf_run_state *pr, int what)
{
	fz_context *ctx = pr->ctx;
	fz_colorspace *colorspace;
	pdf_obj *obj, *dict;
	pdf_obj *rdb = csi->rdb;

	if (!strcmp(csi->name, "Pattern"))
	{
		pdf_set_pattern(csi, pr, what, NULL, NULL);
	}
	else
	{
		if (!strcmp(csi->name, "DeviceGray"))
			colorspace = fz_device_gray(ctx); /* No fz_keep_colorspace as static */
		else if (!strcmp(csi->name, "DeviceRGB"))
			colorspace = fz_device_rgb(ctx); /* No fz_keep_colorspace as static */
		else if (!strcmp(csi->name, "DeviceCMYK"))
			colorspace = fz_device_cmyk(ctx); /* No fz_keep_colorspace as static */
		else
		{
			dict = pdf_dict_gets(rdb, "ColorSpace");
			if (!dict)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find ColorSpace dictionary");
			obj = pdf_dict_gets(dict, csi->name);
			if (!obj)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find colorspace resource '%s'", csi->name);
			colorspace = pdf_load_colorspace(csi->doc, obj);
		}

		pdf_set_colorspace(csi, pr, what, colorspace);

		fz_drop_colorspace(ctx, colorspace);
	}
}

static void pdf_run_CS(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_STROKECOLOR_UNDEFINED;

	pdf_run_cs_imp(csi, pr, PDF_STROKE);
}

static void pdf_run_cs(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_FILLCOLOR_UNDEFINED;

	pdf_run_cs_imp(csi, pr, PDF_FILL);
}

static void pdf_run_DP(pdf_csi *csi, void *state)
{
}

static void pdf_run_Do(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = csi->doc->ctx;
	pdf_obj *dict;
	pdf_obj *obj;
	pdf_obj *subtype;
	pdf_obj *rdb = csi->rdb;

	dict = pdf_dict_gets(rdb, "XObject");
	if (!dict)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find XObject dictionary when looking for: '%s'", csi->name);

	obj = pdf_dict_gets(dict, csi->name);
	if (!obj)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find xobject resource: '%s'", csi->name);

	subtype = pdf_dict_gets(obj, "Subtype");
	if (!pdf_is_name(subtype))
		fz_throw(ctx, FZ_ERROR_GENERIC, "no XObject subtype specified");

	if (pdf_is_hidden_ocg(pdf_dict_gets(obj, "OC"), csi, pr, rdb))
		return;

	if (!strcmp(pdf_to_name(subtype), "Form") && pdf_dict_gets(obj, "Subtype2"))
		subtype = pdf_dict_gets(obj, "Subtype2");

	if (!strcmp(pdf_to_name(subtype), "Form"))
	{
		pdf_xobject *xobj;

		xobj = pdf_load_xobject(csi->doc, obj);

		/* Inherit parent resources, in case this one was empty XXX check where it's loaded */
		if (!xobj->resources)
			xobj->resources = pdf_keep_obj(rdb);

		fz_try(ctx)
		{
			run_xobject(csi, state, xobj->resources, xobj, &fz_identity);
		}
		fz_always(ctx)
		{
			pdf_drop_xobject(ctx, xobj);
		}
		fz_catch(ctx)
		{
			fz_rethrow_message(ctx, "cannot draw xobject (%d %d R)", pdf_to_num(obj), pdf_to_gen(obj));
		}
	}

	else if (!strcmp(pdf_to_name(subtype), "Image"))
	{
		if ((pr->dev->hints & FZ_IGNORE_IMAGE) == 0)
		{
			fz_image *img = pdf_load_image(csi->doc, obj);

			fz_try(ctx)
			{
				pdf_show_image(csi, pr, img);
			}
			fz_always(ctx)
			{
				fz_drop_image(ctx, img);
			}
			fz_catch(ctx)
			{
				fz_rethrow(ctx);
			}
		}
	}

	else if (!strcmp(pdf_to_name(subtype), "PS"))
	{
		fz_warn(ctx, "ignoring XObject with subtype PS");
	}

	else
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown XObject subtype: '%s'", pdf_to_name(subtype));
	}
}

static void pdf_run_EMC(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	if (pr->in_hidden_ocg > 0)
		pr->in_hidden_ocg--;
}

static void pdf_run_ET(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_flush_text(csi, pr);
	pr->accumulate = 1;
}

static void pdf_run_EX(pdf_csi *csi, void *state)
{
}

static void pdf_run_F(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 1, 0, 0);
}

static void pdf_run_G(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_STROKECOLOR_UNDEFINED;
	pdf_set_colorspace(csi, pr, PDF_STROKE, fz_device_gray(csi->doc->ctx));
	pdf_set_color(csi, pr, PDF_STROKE, csi->stack);
}

static void pdf_run_J(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	pr->dev->flags &= ~(FZ_DEVFLAG_STARTCAP_UNDEFINED | FZ_DEVFLAG_DASHCAP_UNDEFINED | FZ_DEVFLAG_ENDCAP_UNDEFINED);
	gstate->stroke_state = fz_unshare_stroke_state(csi->doc->ctx, gstate->stroke_state);
	gstate->stroke_state->start_cap = csi->stack[0];
	gstate->stroke_state->dash_cap = csi->stack[0];
	gstate->stroke_state->end_cap = csi->stack[0];
}

static void pdf_run_K(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_STROKECOLOR_UNDEFINED;
	pdf_set_colorspace(csi, pr, PDF_STROKE, fz_device_cmyk(csi->doc->ctx));
	pdf_set_color(csi, pr, PDF_STROKE, csi->stack);
}

static void pdf_run_M(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	pr->dev->flags &= ~FZ_DEVFLAG_MITERLIMIT_UNDEFINED;
	gstate->stroke_state = fz_unshare_stroke_state(csi->doc->ctx, gstate->stroke_state);
	gstate->stroke_state->miterlimit = csi->stack[0];
}

static void pdf_run_MP(pdf_csi *csi, void *state)
{
}

static void pdf_run_Q(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_grestore(pr);
}

static void pdf_run_RG(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_STROKECOLOR_UNDEFINED;
	pdf_set_colorspace(csi, pr, PDF_STROKE, fz_device_rgb(csi->doc->ctx));
	pdf_set_color(csi, pr, PDF_STROKE, csi->stack);
}

static void pdf_run_S(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 0, 1, 0);
}

static void pdf_run_SC_imp(pdf_csi *csi, pdf_run_state *pr, int what, pdf_material *mat)
{
	fz_context *ctx = pr->ctx;
	pdf_obj *patterntype;
	pdf_obj *dict;
	pdf_obj *obj;
	int kind;
	pdf_obj *rdb = csi->rdb;

	kind = mat->kind;
	if (csi->name[0])
		kind = PDF_MAT_PATTERN;

	switch (kind)
	{
	case PDF_MAT_NONE:
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot set color in mask objects");

	case PDF_MAT_COLOR:
		pdf_set_color(csi, pr, what, csi->stack);
		break;

	case PDF_MAT_PATTERN:
		dict = pdf_dict_gets(rdb, "Pattern");
		if (!dict)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find Pattern dictionary");

		obj = pdf_dict_gets(dict, csi->name);
		if (!obj)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find pattern resource '%s'", csi->name);

		patterntype = pdf_dict_gets(obj, "PatternType");

		if (pdf_to_int(patterntype) == 1)
		{
			pdf_pattern *pat;
			pat = pdf_load_pattern(csi->doc, obj);
			pdf_set_pattern(csi, pr, what, pat, csi->top > 0 ? csi->stack : NULL);
			pdf_drop_pattern(ctx, pat);
		}
		else if (pdf_to_int(patterntype) == 2)
		{
			fz_shade *shd;
			shd = pdf_load_shading(csi->doc, obj);
			pdf_set_shade(csi, pr, what, shd);
			fz_drop_shade(ctx, shd);
		}
		else
		{
			fz_throw(ctx, FZ_ERROR_GENERIC, "unknown pattern type: %d", pdf_to_int(patterntype));
		}
		break;

	case PDF_MAT_SHADE:
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot set color in shade objects");
	}
	mat->gstate_num = pr->gparent;
}

static void pdf_run_SC(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	pr->dev->flags &= ~FZ_DEVFLAG_STROKECOLOR_UNDEFINED;
	pdf_run_SC_imp(csi, pr, PDF_STROKE, &gstate->stroke);
}

static void pdf_run_sc(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	pr->dev->flags &= ~FZ_DEVFLAG_FILLCOLOR_UNDEFINED;
	pdf_run_SC_imp(csi, pr, PDF_FILL, &gstate->fill);
}

static void pdf_run_Tc(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->char_space = csi->stack[0];
}

static void pdf_run_Tw(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->word_space = csi->stack[0];
}

static void pdf_run_Tz(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate;
	float a = csi->stack[0] / 100;

	gstate = pdf_flush_text(csi, pr);
	gstate->scale = a;
}

static void pdf_run_TL(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->leading = csi->stack[0];
}

static void pdf_run_Tf(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = csi->doc->ctx;
	pdf_obj *rdb = csi->rdb;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	pdf_obj *dict;
	pdf_obj *obj;

	gstate->size = csi->stack[0];
	if (gstate->font)
		pdf_drop_font(ctx, gstate->font);
	gstate->font = NULL;

	dict = pdf_dict_gets(rdb, "Font");
	if (!dict)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find Font dictionary");

	obj = pdf_dict_gets(dict, csi->name);
	if (!obj)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find font resource: '%s'", csi->name);

	gstate->font = load_font_or_hail_mary(csi, rdb, obj, pr->nested_depth);
}

static void pdf_run_Tr(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->render = csi->stack[0];
}

static void pdf_run_Ts(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->rise = csi->stack[0];
}

static void pdf_run_Td(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	fz_pre_translate(&pr->tlm, csi->stack[0], csi->stack[1]);
	pr->tm = pr->tlm;
}

static void pdf_run_TD(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->leading = -csi->stack[1];
	fz_pre_translate(&pr->tlm, csi->stack[0], csi->stack[1]);
	pr->tm = pr->tlm;
}

static void pdf_run_Tm(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->tm.a = csi->stack[0];
	pr->tm.b = csi->stack[1];
	pr->tm.c = csi->stack[2];
	pr->tm.d = csi->stack[3];
	pr->tm.e = csi->stack[4];
	pr->tm.f = csi->stack[5];
	pr->tlm = pr->tm;
}

static void pdf_run_Tstar(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	fz_pre_translate(&pr->tlm, 0, -gstate->leading);
	pr->tm = pr->tlm;
}

static void pdf_run_Tj(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	if (csi->string_len)
		pdf_show_string(csi, pr, csi->string, csi->string_len);
	else
		pdf_show_text(csi, pr, csi->obj);
}

static void pdf_run_TJ(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	if (csi->string_len)
		pdf_show_string(csi, pr, csi->string, csi->string_len);
	else
		pdf_show_text(csi, pr, csi->obj);
}

static void pdf_run_W(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->clip = 1;
	pr->clip_even_odd = 0;
}

static void pdf_run_Wstar(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->clip = 1;
	pr->clip_even_odd = 1;
}

static void pdf_run_b(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 1, 1, 1, 0);
}

static void pdf_run_bstar(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 1, 1, 1, 1);
}

static void pdf_run_c(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	float a, b, c, d, e, f;

	a = csi->stack[0];
	b = csi->stack[1];
	c = csi->stack[2];
	d = csi->stack[3];
	e = csi->stack[4];
	f = csi->stack[5];
	fz_curveto(csi->doc->ctx, pr->path, a, b, c, d, e, f);
}

static void pdf_run_cm(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate;
	fz_matrix m;

	gstate = pdf_flush_text(csi, pr);
	m.a = csi->stack[0];
	m.b = csi->stack[1];
	m.c = csi->stack[2];
	m.d = csi->stack[3];
	m.e = csi->stack[4];
	m.f = csi->stack[5];

	fz_concat(&gstate->ctm, &m, &gstate->ctm);
}

static void pdf_run_d(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;
	pdf_obj *array;
	int i;
	int len;

	array = csi->obj;
	len = pdf_array_len(array);
	gstate->stroke_state = fz_unshare_stroke_state_with_dash_len(csi->doc->ctx, gstate->stroke_state, len);
	gstate->stroke_state->dash_len = len;
	for (i = 0; i < len; i++)
		gstate->stroke_state->dash_list[i] = pdf_to_real(pdf_array_get(array, i));
	gstate->stroke_state->dash_phase = csi->stack[0];
}

static void pdf_run_d0(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	if (pr->nested_depth > 1)
		return;
	pr->dev->flags |= FZ_DEVFLAG_COLOR;
}

static void pdf_run_d1(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	if (pr->nested_depth > 1)
		return;
	pr->dev->flags |= FZ_DEVFLAG_MASK;
	pr->dev->flags &= ~(FZ_DEVFLAG_FILLCOLOR_UNDEFINED |
				FZ_DEVFLAG_STROKECOLOR_UNDEFINED |
				FZ_DEVFLAG_STARTCAP_UNDEFINED |
				FZ_DEVFLAG_DASHCAP_UNDEFINED |
				FZ_DEVFLAG_ENDCAP_UNDEFINED |
				FZ_DEVFLAG_LINEJOIN_UNDEFINED |
				FZ_DEVFLAG_MITERLIMIT_UNDEFINED |
				FZ_DEVFLAG_LINEWIDTH_UNDEFINED);
}

static void pdf_run_f(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 1, 0, 0);
}

static void pdf_run_fstar(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 1, 0, 1);
}

static void pdf_run_g(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_FILLCOLOR_UNDEFINED;
	pdf_set_colorspace(csi, pr, PDF_FILL, fz_device_gray(csi->doc->ctx));
	pdf_set_color(csi, pr, PDF_FILL, csi->stack);
}

static void pdf_run_gs(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_obj *dict;
	pdf_obj *obj;
	fz_context *ctx = csi->doc->ctx;
	pdf_obj *rdb = csi->rdb;

	dict = pdf_dict_gets(rdb, "ExtGState");
	if (!dict)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find ExtGState dictionary");

	obj = pdf_dict_gets(dict, csi->name);
	if (!obj)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find extgstate resource '%s'", csi->name);

	pdf_run_extgstate(csi, pr, rdb, obj);
}

static void pdf_run_h(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	fz_closepath(csi->doc->ctx, pr->path);
}

static void pdf_run_i(pdf_csi *csi, void *state)
{
}

static void pdf_run_j(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	pr->dev->flags &= ~FZ_DEVFLAG_LINEJOIN_UNDEFINED;
	gstate->stroke_state = fz_unshare_stroke_state(csi->doc->ctx, gstate->stroke_state);
	gstate->stroke_state->linejoin = csi->stack[0];
}

static void pdf_run_k(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_FILLCOLOR_UNDEFINED;
	pdf_set_colorspace(csi, pr, PDF_FILL, fz_device_cmyk(csi->doc->ctx));
	pdf_set_color(csi, pr, PDF_FILL, csi->stack);
}

static void pdf_run_l(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	float a, b;

	a = csi->stack[0];
	b = csi->stack[1];
	fz_lineto(csi->doc->ctx, pr->path, a, b);
}

static void pdf_run_m(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	float a, b;

	a = csi->stack[0];
	b = csi->stack[1];
	fz_moveto(csi->doc->ctx, pr->path, a, b);
}

static void pdf_run_n(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 0, 0, 0, 0);
}

static void pdf_run_q(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_gsave(pr);
}

static void pdf_run_re(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = csi->doc->ctx;
	float x, y, w, h;

	x = csi->stack[0];
	y = csi->stack[1];
	w = csi->stack[2];
	h = csi->stack[3];

	fz_moveto(ctx, pr->path, x, y);
	fz_lineto(ctx, pr->path, x + w, y);
	fz_lineto(ctx, pr->path, x + w, y + h);
	fz_lineto(ctx, pr->path, x, y + h);
	fz_closepath(ctx, pr->path);
}

static void pdf_run_rg(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pr->dev->flags &= ~FZ_DEVFLAG_FILLCOLOR_UNDEFINED;
	pdf_set_colorspace(csi, pr, PDF_FILL, fz_device_rgb(csi->doc->ctx));
	pdf_set_color(csi, pr, PDF_FILL, csi->stack);
}

static void pdf_run_ri(pdf_csi *csi, void *state)
{
}

static void pdf_run_s(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;

	pdf_show_path(csi, pr, 1, 0, 1, 0);
}

static void pdf_run_sh(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = csi->doc->ctx;
	pdf_obj *rdb = csi->rdb;
	pdf_obj *dict;
	pdf_obj *obj;
	fz_shade *shd;

	dict = pdf_dict_gets(rdb, "Shading");
	if (!dict)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find shading dictionary");

	obj = pdf_dict_gets(dict, csi->name);
	if (!obj)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find shading resource: '%s'", csi->name);

	if ((pr->dev->hints & FZ_IGNORE_SHADE) == 0)
	{
		shd = pdf_load_shading(csi->doc, obj);

		fz_try(ctx)
		{
			pdf_show_shade(csi, pr, shd);
		}
		fz_always(ctx)
		{
			fz_drop_shade(ctx, shd);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}
}

static void pdf_run_v(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	float a, b, c, d;

	a = csi->stack[0];
	b = csi->stack[1];
	c = csi->stack[2];
	d = csi->stack[3];
	fz_curvetov(csi->doc->ctx, pr->path, a, b, c, d);
}

static void pdf_run_w(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate;

	gstate = pdf_flush_text(csi, pr); /* linewidth affects stroked text rendering mode */
	pr->dev->flags &= ~FZ_DEVFLAG_LINEWIDTH_UNDEFINED;
	gstate->stroke_state = fz_unshare_stroke_state(csi->doc->ctx, gstate->stroke_state);
	gstate->stroke_state->linewidth = csi->stack[0];
}

static void pdf_run_y(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	float a, b, c, d;

	a = csi->stack[0];
	b = csi->stack[1];
	c = csi->stack[2];
	d = csi->stack[3];
	fz_curvetoy(csi->doc->ctx, pr->path, a, b, c, d);
}

static void pdf_run_squote(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	fz_pre_translate(&pr->tlm, 0, -gstate->leading);
	pr->tm = pr->tlm;

	if (csi->string_len)
		pdf_show_string(csi, pr, csi->string, csi->string_len);
	else
		pdf_show_text(csi, pr, csi->obj);
}

static void pdf_run_dquote(pdf_csi *csi, void *state)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	pdf_gstate *gstate = pr->gstate + pr->gtop;

	gstate->word_space = csi->stack[0];
	gstate->char_space = csi->stack[1];

	fz_pre_translate(&pr->tlm, 0, -gstate->leading);
	pr->tm = pr->tlm;

	if (csi->string_len)
		pdf_show_string(csi, pr, csi->string, csi->string_len);
	else
		pdf_show_text(csi, pr, csi->obj);
}

static void free_processor_normal(pdf_csi *csi, void *state)
{
	fz_context *ctx = csi->doc->ctx;
	pdf_run_state *pr = (pdf_run_state *)state;

	while (pr->gtop)
		pdf_grestore(pr);

	pdf_drop_material(ctx, &pr->gstate[0].fill);
	pdf_drop_material(ctx, &pr->gstate[0].stroke);
	if (pr->gstate[0].font)
		pdf_drop_font(ctx, pr->gstate[0].font);
	if (pr->gstate[0].softmask)
		pdf_drop_xobject(ctx, pr->gstate[0].softmask);
	/* SumatraPDF: support transfer functions */
	fz_drop_transfer_function(ctx, pr->gstate[0].tr);
	fz_drop_transfer_function(ctx, pr->gstate[0].softmask_tr);
	fz_drop_stroke_state(ctx, pr->gstate[0].stroke_state);

	while (pr->gstate[0].clip_depth--)
		fz_pop_clip(pr->dev);

	if (pr->path) fz_free_path(ctx, pr->path);
	if (pr->text) fz_free_text(ctx, pr->text);

	fz_free(ctx, pr->gstate);
	fz_free(ctx, pr);
}

static void
process_annot(pdf_csi *csi, void *state, pdf_obj *resources, pdf_annot *annot)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = pr->ctx;
	int flags;

	if (pdf_is_hidden_ocg(pdf_dict_gets(annot->obj, "OC"), csi, pr, resources))
		return;

	flags = pdf_to_int(pdf_dict_gets(annot->obj, "F"));
	if (!strcmp(pr->event, "Print") && !(flags & (1 << 2))) /* Print */
		return;
	if (!strcmp(pr->event, "View") && (flags & (1 << 5))) /* NoView */
		return;

	fz_try(ctx)
	{
		/* We need to save an extra level here to allow for level 0
		 * to be the 'parent' gstate level. */
		pdf_gsave(pr);
		run_xobject(csi, state, resources, annot->ap, &annot->matrix);
	}
	fz_catch(ctx)
	{
		while (pr->gtop > 0)
			pdf_grestore(pr);
		fz_rethrow(ctx);
	}
}

static void
process_stream(pdf_csi *csi, void *state, pdf_lexbuf *buf)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = pr->ctx;
	int save_gbot;

	save_gbot = pr->gbot;
	pr->gbot = pr->gtop;
	fz_try(ctx)
	{
		pdf_process_stream(csi, buf);
	}
	fz_always(ctx)
	{
		while (pr->gtop > pr->gbot)
			pdf_grestore(pr);
		pr->gbot = save_gbot;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
process_contents(pdf_csi *csi, void *state, pdf_obj *resources, pdf_obj *contents)
{
	pdf_run_state *pr = (pdf_run_state *)state;
	fz_context *ctx = pr->ctx;

	fz_try(ctx)
	{
		/* We need to save an extra level here to allow for level 0
		 * to be the 'parent' gstate level. */
		pdf_gsave(pr);
		pdf_process_contents_object(csi, resources, contents);
	}
	fz_always(ctx)
	{
		while (pr->gtop > 0)
			pdf_grestore(pr);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

const pdf_processor pdf_processor_normal =
{
	{
	pdf_run_dquote,
	pdf_run_squote,
	pdf_run_B,
	pdf_run_Bstar,
	pdf_run_BDC,
	pdf_run_BI,
	pdf_run_BMC,
	pdf_run_BT,
	pdf_run_BX,
	pdf_run_CS,
	pdf_run_DP,
	pdf_run_EMC,
	pdf_run_ET,
	pdf_run_EX,
	pdf_run_F,
	pdf_run_G,
	pdf_run_J,
	pdf_run_K,
	pdf_run_M,
	pdf_run_MP,
	pdf_run_Q,
	pdf_run_RG,
	pdf_run_S,
	pdf_run_SC,
	pdf_run_SC, /* SCN */
	pdf_run_Tstar,
	pdf_run_TD,
	pdf_run_TJ,
	pdf_run_TL,
	pdf_run_Tc,
	pdf_run_Td,
	pdf_run_Tj,
	pdf_run_Tm,
	pdf_run_Tr,
	pdf_run_Ts,
	pdf_run_Tw,
	pdf_run_Tz,
	pdf_run_W,
	pdf_run_Wstar,
	pdf_run_b,
	pdf_run_bstar,
	pdf_run_c,
	pdf_run_cm,
	pdf_run_cs,
	pdf_run_d,
	pdf_run_d0,
	pdf_run_d1,
	pdf_run_f,
	pdf_run_fstar,
	pdf_run_g,
	pdf_run_h,
	pdf_run_i,
	pdf_run_j,
	pdf_run_k,
	pdf_run_l,
	pdf_run_m,
	pdf_run_n,
	pdf_run_q,
	pdf_run_re,
	pdf_run_rg,
	pdf_run_ri,
	pdf_run_s,
	pdf_run_sc,
	pdf_run_sc, /* scn */
	pdf_run_v,
	pdf_run_w,
	pdf_run_y,
	pdf_run_Do,
	pdf_run_Tf,
	pdf_run_gs,
	pdf_run_sh,
	free_processor_normal
	},
	process_annot,
	process_stream,
	process_contents
};

pdf_process *pdf_process_run(pdf_process *process, fz_device *dev, const fz_matrix *ctm, const char *event, pdf_gstate *gstate, int nested)
{
	fz_context *ctx = dev->ctx;
	pdf_run_state *pr;

	pr = fz_malloc_struct(ctx, pdf_run_state);
	fz_try(ctx)
	{
		pr->ctx = ctx;
		pr->dev = dev;
		pr->in_hidden_ocg = 0;
		pr->event = event;

		pr->path = fz_new_path(ctx);
		pr->clip = 0;
		pr->clip_even_odd = 0;

		pr->text = NULL;
		pr->tlm = fz_identity;
		pr->tm = fz_identity;
		pr->text_mode = 0;
		pr->accumulate = 1;

		pr->gcap = 64;
		pr->gstate = fz_malloc_array(ctx, pr->gcap, sizeof(pdf_gstate));

		pr->nested_depth = nested;
		pdf_init_gstate(ctx, &pr->gstate[0], ctm);
		if (gstate)
		{
			pdf_copy_gstate(ctx, &pr->gstate[0], gstate);
			pr->gstate[0].ctm = *ctm;
		}
		pr->gtop = 0;
		pr->gbot = 0;
		pr->gparent = 0;
	}
	fz_catch(ctx)
	{
		fz_free_path(ctx, pr->path);
		fz_free(ctx, pr);
		fz_rethrow(ctx);
	}

	process->state = pr;
	process->processor = &pdf_processor_normal;
	return process;
}
