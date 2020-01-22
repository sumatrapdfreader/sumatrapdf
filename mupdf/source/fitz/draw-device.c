#include "fitz-imp.h"
#include "draw-imp.h"

#include <string.h>
#include <assert.h>
#include <math.h>
#include <float.h>

#define STACK_SIZE 96

/* Enable the following to attempt to support knockout and/or isolated
 * blending groups. */
#define ATTEMPT_KNOCKOUT_AND_ISOLATED

/* Enable the following to help debug group blending. */
#undef DUMP_GROUP_BLENDS

/* Enable the following to help debug graphics stack pushes/pops */
#undef DUMP_STACK_CHANGES

typedef struct fz_draw_device_s fz_draw_device;

enum {
	FZ_DRAWDEV_FLAGS_TYPE3 = 1,
};

typedef struct fz_draw_state_s fz_draw_state;

struct fz_draw_state_s {
	fz_irect scissor;
	fz_pixmap *dest;
	fz_pixmap *mask;
	fz_pixmap *shape;
	fz_pixmap *group_alpha;
	int blendmode;
	int id, encache;
	float alpha;
	fz_matrix ctm;
	float xstep, ystep;
	fz_irect area;
};

struct fz_draw_device_s
{
	fz_device super;
	fz_matrix transform;
	fz_rasterizer *rast;
	fz_default_colorspaces *default_cs;
	fz_colorspace *proof_cs;
	int flags;
	int resolve_spots;
	int top;
	fz_scale_cache *cache_x;
	fz_scale_cache *cache_y;
	fz_draw_state *stack;
	int stack_cap;
	fz_draw_state init_stack[STACK_SIZE];
};

#ifdef DUMP_GROUP_BLENDS

#include <stdio.h>

static int group_dump_count = 0;

static void fz_dump_blend(fz_context *ctx, const char *s, fz_pixmap *pix)
{
	char name[80];
	int psd = 0;

	if (!pix)
		return;

	if (pix->s || fz_colorspace_is_subtractive(ctx, pix->colorspace))
		psd = 1;

	fz_snprintf(name, sizeof(name), "dump%02d.%s", group_dump_count, psd ? "psd" : "png");
	printf("%s%02d%s(%p)", s ? s : "", group_dump_count++, psd ? "(PSD)" : "", pix);
	if (psd)
		fz_save_pixmap_as_psd(ctx, pix, name);
	else
		fz_save_pixmap_as_png(ctx, pix, name);
}

static void dump_spaces(int x, const char *s)
{
	int i;
	for (i = 0; i < x; i++)
		printf(" ");
	printf("%s", s);
}

#endif

#ifdef DUMP_STACK_CHANGES
#define STACK_PUSHED(A) stack_change(ctx, dev, '>', A)
#define STACK_POPPED(A) stack_change(ctx, dev, '<', A)
#define STACK_CONVERT(A) stack_change(ctx, dev, '=', A)
static void stack_change(fz_context *ctx, fz_draw_device *dev, int c, const char *s)
{
	int n, depth = dev->top;
	if (c != '<')
		depth--;
	n = depth;
	while (n-- > 0)
		fputc('\t', stderr);
	fprintf(stderr, "%c%s (%d)\n", c, s, depth);
}
#else
#define STACK_PUSHED(A) do {} while (0)
#define STACK_POPPED(A) do {} while (0)
#define STACK_CONVERT(A) do {} while (0)
#endif

/* Logic below assumes that default cs is set to color context cs if there
 * was not a default in the document for that particular cs
 */
static fz_colorspace *fz_default_colorspace(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs)
{
	if (cs == NULL)
		return NULL;
	if (default_cs == NULL)
		return cs;

	switch (fz_colorspace_type(ctx, cs))
	{
	case FZ_COLORSPACE_GRAY:
		if (cs == fz_device_gray(ctx))
			return fz_default_gray(ctx, default_cs);
		break;
	case FZ_COLORSPACE_RGB:
		if (cs == fz_device_rgb(ctx))
			return fz_default_rgb(ctx, default_cs);
		break;
	case FZ_COLORSPACE_CMYK:
		if (cs == fz_device_cmyk(ctx))
			return fz_default_cmyk(ctx, default_cs);
		break;
	default:
		break;
	}
	return cs;
}

static void grow_stack(fz_context *ctx, fz_draw_device *dev)
{
	int max = dev->stack_cap * 2;
	fz_draw_state *stack;
	if (dev->stack == &dev->init_stack[0])
	{
		stack = fz_malloc_array(ctx, max, fz_draw_state);
		memcpy(stack, dev->stack, sizeof(*stack) * dev->stack_cap);
	}
	else
	{
		stack = fz_realloc_array(ctx, dev->stack, max, fz_draw_state);
	}
	dev->stack = Memento_label(stack, "draw_stack");
	dev->stack_cap = max;
}

/* 'Push' the stack. Returns a pointer to the current state, with state[1]
 * already having been initialised to contain the same thing. Simply
 * change any contents of state[1] that you want to and continue. */
static fz_draw_state *push_stack(fz_context *ctx, fz_draw_device *dev, const char *message)
{
	fz_draw_state *state;
	if (dev->top == dev->stack_cap-1)
		grow_stack(ctx, dev);
	state = &dev->stack[dev->top];
	dev->top++;
	memcpy(&state[1], state, sizeof(*state));
	STACK_PUSHED(message);
	return state;
}

static fz_draw_state *pop_stack(fz_context *ctx, fz_draw_device *dev, const char *message)
{
	fz_draw_state *state = &dev->stack[--dev->top];
	STACK_POPPED(message);
	return state;
}

static fz_draw_state *convert_stack(fz_context *ctx, fz_draw_device *dev, const char *message)
{
	fz_draw_state *state = &dev->stack[dev->top-1];
	STACK_CONVERT(message);
	return state;
}

static fz_draw_state *
fz_knockout_begin(fz_context *ctx, fz_draw_device *dev)
{
	fz_irect bbox, ga_bbox;
	fz_draw_state *state = &dev->stack[dev->top];
	int isolated = state->blendmode & FZ_BLEND_ISOLATED;

	if ((state->blendmode & FZ_BLEND_KNOCKOUT) == 0)
		return state;

	state = push_stack(ctx, dev, "knockout");

	bbox = fz_pixmap_bbox(ctx, state->dest);
	bbox = fz_intersect_irect(bbox, state->scissor);
	state[1].dest = fz_new_pixmap_with_bbox(ctx, state->dest->colorspace, bbox, state->dest->seps, state->dest->alpha);
	if (state[0].group_alpha)
	{
		ga_bbox = fz_pixmap_bbox(ctx, state->group_alpha);
		ga_bbox = fz_intersect_irect(ga_bbox, state->scissor);
		state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, state->group_alpha->colorspace, ga_bbox, state->group_alpha->seps, state->group_alpha->alpha);
	}

	if (isolated)
	{
		fz_clear_pixmap(ctx, state[1].dest);
		if (state[1].group_alpha)
			fz_clear_pixmap(ctx, state[1].group_alpha);
	}
	else
	{
		/* Find the last but one destination to copy */
		int i = dev->top-1; /* i = the one on entry (i.e. the last one) */
		fz_draw_state *prev = state;
		while (i > 0)
		{
			prev = &dev->stack[--i];
			if (prev->dest != state->dest)
				break;
		}
		if (prev->dest)
		{
			fz_copy_pixmap_rect(ctx, state[1].dest, prev->dest, bbox, dev->default_cs);
			if (state[1].group_alpha)
			{
				if (prev->group_alpha)
					fz_copy_pixmap_rect(ctx, state[1].group_alpha, prev->group_alpha, ga_bbox, dev->default_cs);
				else
					fz_clear_pixmap(ctx, state[1].group_alpha);
			}
		}
		else
		{
			fz_clear_pixmap(ctx, state[1].dest);
			if (state[1].group_alpha)
				fz_clear_pixmap(ctx, state[1].group_alpha);
		}
	}

	/* Knockout groups (and only knockout groups) rely on shape */
	state[1].shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
	fz_clear_pixmap(ctx, state[1].shape);

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "");
	fz_dump_blend(ctx, "Knockout begin: background is ", state[1].dest);
	if (state[1].shape)
		fz_dump_blend(ctx, "/S=", state[1].shape);
	if (state[1].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
	printf("\n");
#endif

	state[1].scissor = bbox;
	state[1].blendmode &= ~(FZ_BLEND_MODEMASK | FZ_BLEND_ISOLATED);

	return &state[1];
}

static void fz_knockout_end(fz_context *ctx, fz_draw_device *dev)
{
	fz_draw_state *state;

	if (dev->top == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected knockout end");

	state = pop_stack(ctx, dev, "knockout");
	if ((state[0].blendmode & FZ_BLEND_KNOCKOUT) == 0)
		return;

	assert((state[1].blendmode & FZ_BLEND_ISOLATED) == 0);
	assert((state[1].blendmode & FZ_BLEND_MODEMASK) == 0);
	assert(state[1].shape);

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "");
	fz_dump_blend(ctx, "Knockout end: blending ", state[1].dest);
	if (state[1].shape)
		fz_dump_blend(ctx, "/S=", state[1].shape);
	if (state[1].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
	fz_dump_blend(ctx, " onto ", state[0].dest);
	if (state[0].shape)
		fz_dump_blend(ctx, "/S=", state[0].shape);
	if (state[0].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
	if ((state->blendmode & FZ_BLEND_MODEMASK) != 0)
		printf(" (blend %d)", state->blendmode & FZ_BLEND_MODEMASK);
	if ((state->blendmode & FZ_BLEND_ISOLATED) != 0)
		printf(" (isolated)");
	printf(" (knockout)");
#endif

	fz_blend_pixmap_knockout(ctx, state[0].dest, state[1].dest, state[1].shape);
	fz_drop_pixmap(ctx, state[1].dest);
	state[1].dest = NULL;

	if (state[1].group_alpha && state[0].group_alpha != state[1].group_alpha)
	{
		if (state[0].group_alpha)
			fz_blend_pixmap_knockout(ctx, state[0].group_alpha, state[1].group_alpha, state[1].shape);
		fz_drop_pixmap(ctx, state[1].group_alpha);
		state[1].group_alpha = NULL;
	}

	if (state[0].shape != state[1].shape)
	{
		if (state[0].shape)
			fz_paint_pixmap(state[0].shape, state[1].shape, 255);
		fz_drop_pixmap(ctx, state[1].shape);
		state[1].shape = NULL;
	}

#ifdef DUMP_GROUP_BLENDS
	fz_dump_blend(ctx, " to get ", state[0].dest);
	if (state[0].shape)
		fz_dump_blend(ctx, "/S=", state[0].shape);
	if (state[0].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
	printf("\n");
#endif
}

static int
colors_supported(fz_context *ctx, fz_colorspace *cs, fz_pixmap *dest)
{
	/* Even if we support separations in the destination, if the color space has CMY or K as one of
	 * its colorants and we are in RGB or Gray we will want to do the tint transform */
	if (!fz_colorspace_is_subtractive(ctx, dest->colorspace) && fz_colorspace_device_n_has_cmyk(ctx, cs))
		return 0;

	/* If we have separations then we should support it */
	if (dest->seps)
		return 1;

	/* If our destination is CMYK and the source color space is only C, M, Y or K we support it
	 * even if we have no seps */
	if (fz_colorspace_is_subtractive(ctx, dest->colorspace))
	{
		int i, n;
		if (fz_colorspace_device_n_has_only_cmyk(ctx, cs))
			return 1;

		n = fz_colorspace_n(ctx, cs);
		for (i = 0; i < n; i++)
		{
			const char *name = fz_colorspace_colorant(ctx, cs, i);

			if (!name)
				return 0;
			if (!strcmp(name, "All"))
				continue;
			if (!strcmp(name, "Cyan"))
				continue;
			if (!strcmp(name, "Magenta"))
				continue;
			if (!strcmp(name, "Yellow"))
				continue;
			if (!strcmp(name, "Black"))
				continue;
			if (!strcmp(name, "None"))
				continue;
			return 0;
		}
		return 1;
	}

	return 0;
}

static fz_overprint *
set_op_from_spaces(fz_context *ctx, fz_overprint *op, const fz_pixmap *dest, fz_colorspace *src, int opm)
{
	int dn, sn, i, j, dc;

	if (!op)
		return NULL;

	if (!fz_colorspace_is_subtractive(ctx, src) || !fz_colorspace_is_subtractive(ctx, dest->colorspace))
		return NULL;

	sn = fz_colorspace_n(ctx, src);
	dn = dest->n - dest->alpha;
	dc = dn - dest->s;

	/* If a source colorant is not mentioned in the destination
	 * colorants (either process or spots), then it will be mapped
	 * to process colorants. In this case, the process colorants
	 * can never be protected.
	 */
	for (j = 0; j < sn; j++)
	{
		/* Run through the colorants looking for one that isn't mentioned.
		 * i.e. continue if we we find one, break if not. */
		const char *sname = fz_colorspace_colorant(ctx, src, j);
		if (!sname)
			break;
		if (!strcmp(sname, "All") || !strcmp(sname, "None"))
			continue;
		for (i = 0; i < dc; i++)
		{
			const char *name = fz_colorspace_colorant(ctx, dest->colorspace, i);
			if (!name)
				continue;
			if (!strcmp(name, sname))
				break;
		}
		if (i != dc)
			continue;
		for (; i < dn; i++)
		{
			const char *name = fz_separation_name(ctx, dest->seps, i - dc);
			if (!name)
				continue;
			if (!strcmp(name, sname))
				break;
		}
		if (i == dn)
		{
			/* This source colorant wasn't mentioned */
			break;
		}
	}
	if (j == sn)
	{
		/* We did not find any source colorants that weren't mentioned, so
		 * process colorants might not be touched... */
		for (i = 0; i < dc; i++)
		{
			const char *name = fz_colorspace_colorant(ctx, dest->colorspace, i);

			for (j = 0; j < sn; j++)
			{
				const char *sname = fz_colorspace_colorant(ctx, src, j);
				if (!name || !sname)
					continue;
				if (!strcmp(name, sname))
					break;
				if (!strcmp(sname, "All"))
					break;
			}
			if (j == sn)
				fz_set_overprint(op, i);
		}
	}
	for (i = dc; i < dn; i++)
	{
		const char *name = fz_separation_name(ctx, dest->seps, i - dc);

		for (j = 0; j < sn; j++)
		{
			const char *sname = fz_colorspace_colorant(ctx, src, j);
			if (!name || !sname)
				continue;
			if (!strcmp(name, sname))
				break;
			if (!strcmp(sname, "All"))
				break;
		}
		if (j == sn)
			fz_set_overprint(op, i);
	}

	return op;
}

static fz_overprint *
resolve_color(fz_context *ctx,
	fz_overprint *op,
	const float *color,
	fz_colorspace *colorspace,
	float alpha,
	fz_color_params color_params,
	unsigned char *colorbv,
	fz_pixmap *dest)
{
	float colorfv[FZ_MAX_COLORS];
	int i;
	int n = dest->n - dest->alpha;
	fz_colorspace *model = dest->colorspace;
	int devn, devgray;
	int effective_opm;

	if (colorspace == NULL && model != NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color destination requires source color");

	effective_opm = color_params.opm;
	devn = fz_colorspace_is_device_n(ctx, colorspace);
	devgray = fz_colorspace_is_device_gray(ctx, colorspace);

	/* We can only overprint when enabled, and when we are in a subtractive colorspace */
	if (color_params.op == 0 || !fz_colorspace_is_subtractive(ctx, dest->colorspace))
		op = NULL;

	/* Device Gray is additive, but seems to still be counted for overprint
	 * (see Ghent_V3.0/030_Gray_K_black_OP_x1a.pdf 030.pdf). */
	else if (devgray)
	{
	}

	/* If we are in a CMYK space (i.e. not a devn one, given we know we are subtractive at this point),
	 * then we only adhere to overprint mode if it's the same space as the destination. */
	/* FIXME: Possibly we need a better equivalency test here. */
	else if (!devn && colorspace != dest->colorspace)
	{
		effective_opm = 0;
	}

	if (n == 0)
		i = 0;
	else if (devn && colors_supported(ctx, colorspace, dest))
	{
		fz_convert_separation_colors(ctx, colorspace, color, dest->seps, dest->colorspace, colorfv, color_params);
		for (i = 0; i < n; i++)
			colorbv[i] = colorfv[i] * 255;
		op = set_op_from_spaces(ctx, op, dest, colorspace, effective_opm);
	}
	else
	{
		int c = n - dest->s;
		fz_convert_color(ctx, colorspace, color, dest->colorspace, colorfv, NULL, color_params);
		for (i = 0; i < c; i++)
			colorbv[i] = colorfv[i] * 255;
		for (; i < n; i++)
		{
			colorfv[i] = 0;
			colorbv[i] = 0;
		}
	}
	colorbv[i] = alpha * 255;

	/* op && !devn => overprinting in cmyk or devicegray. */
	if (op && !devn)
	{
		/* We are overprinting, so protect all spots. */
		for (i = 4; i < n; i++)
			fz_set_overprint(op, i);
		/* If OPM, then protect all components for which the color values are zero.
		 * (but only if we're in devicecmyk). */
		if (effective_opm == 1 && colorspace != fz_device_gray(ctx))
			for (i = 0; i < n; i++)
				if (colorfv[i] == 0)
					fz_set_overprint(op, i);
	}

	return op;
}

static fz_draw_state *
push_group_for_separations(fz_context *ctx, fz_draw_device *dev, fz_color_params color_params, fz_default_colorspaces *default_cs)
{
	fz_separations *clone = fz_clone_separations_for_overprint(ctx, dev->stack[0].dest->seps);
	fz_colorspace *oi = fz_default_output_intent(ctx, default_cs);
	fz_colorspace *dcs = fz_device_cmyk(ctx);

	/* Pick sep target CMYK based upon proof and output intent settings.  Priority
	* is oi, proof, devicecmyk. */
	if (dev->proof_cs)
	{
		dcs = dev->proof_cs;
	}

	if (oi)
	{
		dcs = oi;
	}

	/* Not needed if dest has the seps, and we are not using a proof or the target is the same as the proof and we don't have an oi or the target is the same as the oi */
	if ((clone == dev->stack[0].dest->seps) && (dev->proof_cs == NULL || dev->proof_cs == dev->stack[0].dest->colorspace) && (oi == NULL || oi == dev->stack[0].dest->colorspace))
	{
		fz_drop_separations(ctx, clone);
		dev->resolve_spots = 0;
		return &dev->stack[0];
	}

	/* Make a new pixmap to render to. */
	fz_try(ctx)
	{
		push_stack(ctx, dev, "separations");
		dev->stack[1].dest = fz_clone_pixmap_area_with_different_seps(ctx, dev->stack[0].dest, &dev->stack[0].scissor, dcs, clone, color_params, default_cs);
	}
	fz_always(ctx)
		fz_drop_separations(ctx, clone);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return &dev->stack[1];
}

static void
fz_draw_fill_path(fz_context *ctx, fz_device *devp, const fz_path *path, int even_odd, fz_matrix in_ctm,
	fz_colorspace *colorspace_in, const float *color, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_rasterizer *rast = dev->rast;
	fz_colorspace *colorspace = fz_default_colorspace(ctx, dev->default_cs, colorspace_in);
	float expansion = fz_matrix_expansion(ctm);
	float flatness;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	fz_irect bbox;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_overprint op = { { 0 } };
	fz_overprint *eop;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	if (expansion < FLT_EPSILON)
		expansion = 1;
	flatness = 0.3f / expansion;
	if (flatness < 0.001f)
		flatness = 0.001f;

	bbox = fz_intersect_irect(fz_pixmap_bbox(ctx, state->dest), state->scissor);
	if (fz_flatten_fill_path(ctx, rast, path, ctm, flatness, &bbox, &bbox))
		return;

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		state = fz_knockout_begin(ctx, dev);

	eop = resolve_color(ctx, &op, color, colorspace, alpha, color_params, colorbv, state->dest);

	fz_convert_rasterizer(ctx, rast, even_odd, state->dest, colorbv, eop);
	if (state->shape)
	{
		if (!rast->fns.reusable)
			fz_flatten_fill_path(ctx, rast, path, ctm, flatness, &bbox, NULL);

		colorbv[0] = 255;
		fz_convert_rasterizer(ctx, rast, even_odd, state->shape, colorbv, 0);
	}
	if (state->group_alpha)
	{
		if (!rast->fns.reusable)
			fz_flatten_fill_path(ctx, rast, path, ctm, flatness, &bbox, NULL);

		colorbv[0] = alpha * 255;
		fz_convert_rasterizer(ctx, rast, even_odd, state->group_alpha, colorbv, 0);
	}

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(ctx, dev);
}

static void
fz_draw_stroke_path(fz_context *ctx, fz_device *devp, const fz_path *path, const fz_stroke_state *stroke, fz_matrix in_ctm,
	fz_colorspace *colorspace_in, const float *color, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_rasterizer *rast = dev->rast;
	fz_colorspace *colorspace = fz_default_colorspace(ctx, dev->default_cs, colorspace_in);
	float expansion = fz_matrix_expansion(ctm);
	float flatness;
	float linewidth = stroke->linewidth;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	fz_irect bbox;
	float aa_level = 2.0f/(fz_rasterizer_graphics_aa_level(rast)+2);
	fz_draw_state *state = &dev->stack[dev->top];
	float mlw = fz_rasterizer_graphics_min_line_width(rast);
	fz_overprint op = { { 0 } };
	fz_overprint *eop;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	if (mlw > aa_level)
		aa_level = mlw;
	if (expansion < FLT_EPSILON)
		expansion = 1;
	if (linewidth * expansion < aa_level)
		linewidth = aa_level / expansion;
	flatness = 0.3f / expansion;
	if (flatness < 0.001f)
		flatness = 0.001f;

	bbox = fz_intersect_irect(fz_pixmap_bbox_no_ctx(state->dest), state->scissor);
	if (fz_flatten_stroke_path(ctx, rast, path, stroke, ctm, flatness, linewidth, &bbox, &bbox))
		return;

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		state = fz_knockout_begin(ctx, dev);

	eop = resolve_color(ctx, &op, color, colorspace, alpha, color_params, colorbv, state->dest);

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "");
	fz_dump_blend(ctx, "Before stroke ", state->dest);
	if (state->shape)
		fz_dump_blend(ctx, "/S=", state->shape);
	if (state->group_alpha)
		fz_dump_blend(ctx, "/GA=", state->group_alpha);
	printf("\n");
#endif
	fz_convert_rasterizer(ctx, rast, 0, state->dest, colorbv, eop);
	if (state->shape)
	{
		if (!rast->fns.reusable)
			(void)fz_flatten_stroke_path(ctx, rast, path, stroke, ctm, flatness, linewidth, &bbox, NULL);

		colorbv[0] = 255;
		fz_convert_rasterizer(ctx, rast, 0, state->shape, colorbv, 0);
	}
	if (state->group_alpha)
	{
		if (!rast->fns.reusable)
			(void)fz_flatten_stroke_path(ctx, rast, path, stroke, ctm, flatness, linewidth, &bbox, NULL);

		colorbv[0] = 255 * alpha;
		fz_convert_rasterizer(ctx, rast, 0, state->group_alpha, colorbv, 0);
	}

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "");
	fz_dump_blend(ctx, "After stroke ", state->dest);
	if (state->shape)
		fz_dump_blend(ctx, "/S=", state->shape);
	if (state->group_alpha)
		fz_dump_blend(ctx, "/GA=", state->group_alpha);
	printf("\n");
#endif

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(ctx, dev);
}

static void
fz_draw_clip_path(fz_context *ctx, fz_device *devp, const fz_path *path, int even_odd, fz_matrix in_ctm, fz_rect scissor)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_rasterizer *rast = dev->rast;

	float expansion = fz_matrix_expansion(ctm);
	float flatness;
	fz_irect bbox;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_colorspace *model;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	if (expansion < FLT_EPSILON)
		expansion = 1;
	flatness = 0.3f / expansion;
	if (flatness < 0.001f)
		flatness = 0.001f;

	state = push_stack(ctx, dev, "clip path");

	model = state->dest->colorspace;

	if (!fz_is_infinite_rect(scissor))
	{
		bbox = fz_irect_from_rect(fz_transform_rect(scissor, dev->transform));
		bbox = fz_intersect_irect(bbox, fz_pixmap_bbox(ctx, state->dest));
		bbox = fz_intersect_irect(bbox, state->scissor);
	}
	else
	{
		bbox = fz_intersect_irect(fz_pixmap_bbox(ctx, state->dest), state->scissor);
	}

	if (fz_flatten_fill_path(ctx, rast, path, ctm, flatness, &bbox, &bbox) || fz_is_rect_rasterizer(ctx, rast))
	{
		state[1].scissor = bbox;
		state[1].mask = NULL;
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top-1, "Clip (rectangular) begin\n");
#endif
		return;
	}

	state[1].mask = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
	fz_clear_pixmap(ctx, state[1].mask);
	state[1].dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, state[0].dest->alpha);
	fz_copy_pixmap_rect(ctx, state[1].dest, state[0].dest, bbox, dev->default_cs);
	if (state[1].shape)
	{
		state[1].shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].shape);
	}
	if (state[1].group_alpha)
	{
		state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].group_alpha);
	}

	fz_convert_rasterizer(ctx, rast, even_odd, state[1].mask, NULL, 0);

	state[1].scissor = bbox;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Clip (non-rectangular) begin\n");
#endif
}

static void
fz_draw_clip_stroke_path(fz_context *ctx, fz_device *devp, const fz_path *path, const fz_stroke_state *stroke, fz_matrix in_ctm, fz_rect scissor)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_rasterizer *rast = dev->rast;

	float expansion = fz_matrix_expansion(ctm);
	float flatness;
	float linewidth = stroke->linewidth;
	fz_irect bbox;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_colorspace *model;
	float aa_level = 2.0f/(fz_rasterizer_graphics_aa_level(rast)+2);
	float mlw = fz_rasterizer_graphics_min_line_width(rast);

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	if (mlw > aa_level)
		aa_level = mlw;
	if (expansion < FLT_EPSILON)
		expansion = 1;
	if (linewidth * expansion < aa_level)
		linewidth = aa_level / expansion;
	flatness = 0.3f / expansion;
	if (flatness < 0.001f)
		flatness = 0.001f;

	state = push_stack(ctx, dev, "clip stroke");

	model = state->dest->colorspace;

	if (!fz_is_infinite_rect(scissor))
	{
		bbox = fz_irect_from_rect(fz_transform_rect(scissor, dev->transform));
		bbox = fz_intersect_irect(bbox, fz_pixmap_bbox(ctx, state->dest));
		bbox = fz_intersect_irect(bbox, state->scissor);
	}
	else
	{
		bbox = fz_intersect_irect(fz_pixmap_bbox(ctx, state->dest), state->scissor);
	}

	if (fz_flatten_stroke_path(ctx, rast, path, stroke, ctm, flatness, linewidth, &bbox, &bbox))
	{
		state[1].scissor = bbox;
		state[1].mask = NULL;
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top-1, "Clip (stroke, empty) begin\n");
#endif
		return;
	}

	state[1].mask = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
	fz_clear_pixmap(ctx, state[1].mask);
	/* When there is no alpha in the current destination (state[0].dest->alpha == 0)
	 * we have a choice. We can either create the new destination WITH alpha, or
	 * we can copy the old pixmap contents in. We opt for the latter here, but
	 * may want to revisit this decision in the future. */
	state[1].dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, state[0].dest->alpha);
	if (state[0].dest->alpha)
		fz_clear_pixmap(ctx, state[1].dest);
	else
		fz_copy_pixmap_rect(ctx, state[1].dest, state[0].dest, bbox, dev->default_cs);
	if (state->shape)
	{
		state[1].shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].shape);
	}
	if (state->group_alpha)
	{
		state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].group_alpha);
	}

	fz_convert_rasterizer(ctx, rast, 0, state[1].mask, NULL, 0);

	state[1].blendmode |= FZ_BLEND_ISOLATED;
	state[1].scissor = bbox;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Clip (stroke) begin\n");
#endif
}

static void
draw_glyph(unsigned char *colorbv, fz_pixmap *dst, fz_glyph *glyph,
	int xorig, int yorig, const fz_irect *scissor, fz_overprint *eop)
{
	unsigned char *dp;
	fz_irect bbox;
	int x, y, w, h;
	int skip_x, skip_y;
	fz_pixmap *msk;

	bbox = fz_glyph_bbox_no_ctx(glyph);
	bbox = fz_translate_irect(bbox, xorig, yorig);
	bbox = fz_intersect_irect(bbox, *scissor); /* scissor < dst */
	bbox = fz_intersect_irect(bbox, fz_pixmap_bbox_no_ctx(dst));

	if (fz_is_empty_irect(bbox))
		return;

	x = bbox.x0;
	y = bbox.y0;
	w = bbox.x1 - bbox.x0;
	h = bbox.y1 - bbox.y0;

	skip_x = x - glyph->x - xorig;
	skip_y = y - glyph->y - yorig;

	msk = glyph->pixmap;
	dp = dst->samples + (unsigned int)((y - dst->y) * dst->stride + (x - dst->x) * dst->n);
	if (msk == NULL)
	{
		fz_paint_glyph(colorbv, dst, dp, glyph, w, h, skip_x, skip_y, eop);
	}
	else
	{
		unsigned char *mp = msk->samples + skip_y * msk->stride + skip_x;
		int da = dst->alpha;

		if (dst->colorspace)
		{
			fz_span_color_painter_t *fn;

			fn = fz_get_span_color_painter(dst->n, da, colorbv, eop);
			assert(fn);
			if (fn == NULL)
				return;
			while (h--)
			{
				(*fn)(dp, mp, dst->n, w, colorbv, da, eop);
				dp += dst->stride;
				mp += msk->stride;
			}
		}
		else
		{
			fz_span_painter_t *fn;

			fn = fz_get_span_painter(da, 1, 0, 255, eop);
			assert(fn);
			if (fn == NULL)
				return;
			while (h--)
			{
				(*fn)(dp, da, mp, 1, 0, w, 255, eop);
				dp += dst->stride;
				mp += msk->stride;
			}
		}
	}
}

static void
fz_draw_fill_text(fz_context *ctx, fz_device *devp, const fz_text *text, fz_matrix in_ctm,
	fz_colorspace *colorspace_in, const float *color, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_draw_state *state = &dev->stack[dev->top];
	fz_colorspace *model = state->dest->colorspace;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	unsigned char shapebv, shapebva;
	fz_text_span *span;
	int i;
	fz_colorspace *colorspace = NULL;
	fz_rasterizer *rast = dev->rast;
	fz_overprint op = { { 0 } };
	fz_overprint *eop;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	if (colorspace_in)
		colorspace = fz_default_colorspace(ctx, dev->default_cs, colorspace_in);

	if (colorspace == NULL && model != NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color destination requires source color");

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		state = fz_knockout_begin(ctx, dev);

	eop = resolve_color(ctx, &op, color, colorspace, alpha, color_params, colorbv, state->dest);
	shapebv = 255;
	shapebva = 255 * alpha;

	for (span = text->head; span; span = span->next)
	{
		fz_matrix tm, trm;
		fz_glyph *glyph;
		int gid;

		tm = span->trm;

		for (i = 0; i < span->len; i++)
		{
			gid = span->items[i].gid;
			if (gid < 0)
				continue;

			tm.e = span->items[i].x;
			tm.f = span->items[i].y;
			trm = fz_concat(tm, ctm);

			glyph = fz_render_glyph(ctx, span->font, gid, &trm, model, &state->scissor, state->dest->alpha, fz_rasterizer_text_aa_level(rast));
			if (glyph)
			{
				fz_pixmap *pixmap = glyph->pixmap;
				int x = floorf(trm.e);
				int y = floorf(trm.f);
				if (pixmap == NULL || pixmap->n == 1)
				{
					draw_glyph(colorbv, state->dest, glyph, x, y, &state->scissor, eop);
					if (state->shape)
						draw_glyph(&shapebv, state->shape, glyph, x, y, &state->scissor, 0);
					if (state->group_alpha)
						draw_glyph(&shapebva, state->group_alpha, glyph, x, y, &state->scissor, 0);
				}
				else
				{
					fz_matrix mat;
					mat.a = pixmap->w; mat.b = mat.c = 0; mat.d = pixmap->h;
					mat.e = x + pixmap->x; mat.f = y + pixmap->y;
					fz_paint_image(ctx, state->dest, &state->scissor, state->shape, state->group_alpha, pixmap, mat, alpha * 255, !(devp->hints & FZ_DONT_INTERPOLATE_IMAGES), devp->flags & FZ_DEVFLAG_GRIDFIT_AS_TILED, eop);
				}
				fz_drop_glyph(ctx, glyph);
			}
			else
			{
				fz_path *path = fz_outline_glyph(ctx, span->font, gid, tm);
				if (path)
				{
					fz_draw_fill_path(ctx, devp, path, 0, in_ctm, colorspace, color, alpha, color_params);
					fz_drop_path(ctx, path);
				}
				else
				{
					fz_warn(ctx, "cannot render glyph");
				}
			}
		}
	}

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(ctx, dev);
}

static void
fz_draw_stroke_text(fz_context *ctx, fz_device *devp, const fz_text *text, const fz_stroke_state *stroke,
	fz_matrix in_ctm, fz_colorspace *colorspace_in, const float *color, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_draw_state *state = &dev->stack[dev->top];
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	unsigned char solid = 255;
	unsigned char alpha_byte = alpha * 255;
	fz_text_span *span;
	int i;
	fz_colorspace *colorspace = NULL;
	int aa = fz_rasterizer_text_aa_level(dev->rast);
	fz_overprint op = { { 0 } };
	fz_overprint *eop;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	if (colorspace_in)
		colorspace = fz_default_colorspace(ctx, dev->default_cs, colorspace_in);

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		state = fz_knockout_begin(ctx, dev);

	eop = resolve_color(ctx, &op, color, colorspace, alpha, color_params, colorbv, state->dest);

	for (span = text->head; span; span = span->next)
	{
		fz_matrix tm, trm;
		fz_glyph *glyph;
		int gid;

		tm = span->trm;

		for (i = 0; i < span->len; i++)
		{
			gid = span->items[i].gid;
			if (gid < 0)
				continue;

			tm.e = span->items[i].x;
			tm.f = span->items[i].y;
			trm = fz_concat(tm, ctm);

			glyph = fz_render_stroked_glyph(ctx, span->font, gid, &trm, ctm, stroke, &state->scissor, aa);
			if (glyph)
			{
				int x = (int)trm.e;
				int y = (int)trm.f;
				draw_glyph(colorbv, state->dest, glyph, x, y, &state->scissor, eop);
				if (state->shape)
					draw_glyph(&solid, state->shape, glyph, x, y, &state->scissor, 0);
				if (state->group_alpha)
					draw_glyph(&alpha_byte, state->group_alpha, glyph, x, y, &state->scissor, 0);
				fz_drop_glyph(ctx, glyph);
			}
			else
			{
				fz_path *path = fz_outline_glyph(ctx, span->font, gid, tm);
				if (path)
				{
					fz_draw_stroke_path(ctx, devp, path, stroke, in_ctm, colorspace, color, alpha, color_params);
					fz_drop_path(ctx, path);
				}
				else
				{
					fz_warn(ctx, "cannot render glyph");
				}
			}
		}
	}

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(ctx, dev);
}

static void
fz_draw_clip_text(fz_context *ctx, fz_device *devp, const fz_text *text, fz_matrix in_ctm, fz_rect scissor)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_irect bbox;
	fz_matrix tm, trm;
	fz_glyph *glyph;
	int i, gid;
	fz_draw_state *state;
	fz_colorspace *model;
	fz_text_span *span;
	fz_rasterizer *rast = dev->rast;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	state = push_stack(ctx, dev, "clip text");

	model = state->dest->colorspace;

	/* make the mask the exact size needed */
	bbox = fz_irect_from_rect(fz_bound_text(ctx, text, NULL, ctm));
	bbox = fz_intersect_irect(bbox, state->scissor);
	if (!fz_is_infinite_rect(scissor))
	{
		fz_rect tscissor = fz_transform_rect(scissor, dev->transform);
		bbox = fz_intersect_irect(bbox, fz_irect_from_rect(tscissor));
	}

	state[1].mask = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
	fz_clear_pixmap(ctx, state[1].mask);
	/* When there is no alpha in the current destination (state[0].dest->alpha == 0)
	 * we have a choice. We can either create the new destination WITH alpha, or
	 * we can copy the old pixmap contents in. We opt for the latter here, but
	 * may want to revisit this decision in the future. */
	state[1].dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, state[0].dest->alpha);
	if (state[0].dest->alpha)
		fz_clear_pixmap(ctx, state[1].dest);
	else
		fz_copy_pixmap_rect(ctx, state[1].dest, state[0].dest, bbox, dev->default_cs);
	if (state->shape)
	{
		state[1].shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].shape);
	}
	else
		state[1].shape = NULL;
	if (state->group_alpha)
	{
		state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].group_alpha);
	}
	else
		state[1].group_alpha = NULL;

	state[1].blendmode |= FZ_BLEND_ISOLATED;
	state[1].scissor = bbox;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Clip (text) begin\n");
#endif

	if (!fz_is_empty_irect(bbox) && state[1].mask)
	{
		for (span = text->head; span; span = span->next)
		{
			tm = span->trm;

			for (i = 0; i < span->len; i++)
			{
				gid = span->items[i].gid;
				if (gid < 0)
					continue;

				tm.e = span->items[i].x;
				tm.f = span->items[i].y;
				trm = fz_concat(tm, ctm);

				glyph = fz_render_glyph(ctx, span->font, gid, &trm, model, &state->scissor, state[1].dest->alpha, fz_rasterizer_text_aa_level(rast));
				if (glyph)
				{
					int x = (int)trm.e;
					int y = (int)trm.f;
					draw_glyph(NULL, state[1].mask, glyph, x, y, &bbox, 0);
					if (state[1].shape)
						draw_glyph(NULL, state[1].shape, glyph, x, y, &bbox, 0);
					if (state[1].group_alpha)
						draw_glyph(NULL, state[1].group_alpha, glyph, x, y, &bbox, 0);
					fz_drop_glyph(ctx, glyph);
				}
				else
				{
					fz_path *path = fz_outline_glyph(ctx, span->font, gid, tm);
					if (path)
					{
						fz_pixmap *old_dest;
						float white = 1;

						old_dest = state[1].dest;
						state[1].dest = state[1].mask;
						state[1].mask = NULL;
						fz_try(ctx)
						{
							fz_draw_fill_path(ctx, devp, path, 0, in_ctm, fz_device_gray(ctx), &white, 1, fz_default_color_params);
						}
						fz_always(ctx)
						{
							state[1].mask = state[1].dest;
							state[1].dest = old_dest;
							fz_drop_path(ctx, path);
						}
						fz_catch(ctx)
						{
							fz_rethrow(ctx);
						}
					}
					else
					{
						fz_warn(ctx, "cannot render glyph for clipping");
					}
				}
			}
		}
	}
}

static void
fz_draw_clip_stroke_text(fz_context *ctx, fz_device *devp, const fz_text *text, const fz_stroke_state *stroke, fz_matrix in_ctm, fz_rect scissor)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_irect bbox;
	fz_pixmap *mask, *dest, *shape, *group_alpha;
	fz_matrix tm, trm;
	fz_glyph *glyph;
	int i, gid;
	fz_draw_state *state = push_stack(ctx, dev, "clip stroke text");
	fz_colorspace *model = state->dest->colorspace;
	fz_text_span *span;
	int aa = fz_rasterizer_text_aa_level(dev->rast);

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	/* make the mask the exact size needed */
	bbox = fz_irect_from_rect(fz_bound_text(ctx, text, stroke, ctm));
	bbox = fz_intersect_irect(bbox, state->scissor);
	if (!fz_is_infinite_rect(scissor))
	{
		fz_rect tscissor = fz_transform_rect(scissor, dev->transform);
		bbox = fz_intersect_irect(bbox, fz_irect_from_rect(tscissor));
	}

	state[1].mask = mask = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
	fz_clear_pixmap(ctx, mask);
	/* When there is no alpha in the current destination (state[0].dest->alpha == 0)
	 * we have a choice. We can either create the new destination WITH alpha, or
	 * we can copy the old pixmap contents in. We opt for the latter here, but
	 * may want to revisit this decision in the future. */
	state[1].dest = dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, state[0].dest->alpha);
	if (state[0].dest->alpha)
		fz_clear_pixmap(ctx, state[1].dest);
	else
		fz_copy_pixmap_rect(ctx, state[1].dest, state[0].dest, bbox, dev->default_cs);
	if (state->shape)
	{
		state[1].shape = shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, shape);
	}
	else
		shape = state->shape;
	if (state->group_alpha)
	{
		state[1].group_alpha = group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, group_alpha);
	}
	else
		group_alpha = NULL;

	state[1].blendmode |= FZ_BLEND_ISOLATED;
	state[1].scissor = bbox;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Clip (stroke text) begin\n");
#endif

	if (!fz_is_empty_irect(bbox))
	{
		for (span = text->head; span; span = span->next)
		{
			tm = span->trm;

			for (i = 0; i < span->len; i++)
			{
				gid = span->items[i].gid;
				if (gid < 0)
					continue;

				tm.e = span->items[i].x;
				tm.f = span->items[i].y;
				trm = fz_concat(tm, ctm);

				glyph = fz_render_stroked_glyph(ctx, span->font, gid, &trm, ctm, stroke, &state->scissor, aa);
				if (glyph)
				{
					int x = (int)trm.e;
					int y = (int)trm.f;
					draw_glyph(NULL, mask, glyph, x, y, &bbox, 0);
					if (shape)
						draw_glyph(NULL, shape, glyph, x, y, &bbox, 0);
					if (group_alpha)
						draw_glyph(NULL, group_alpha, glyph, x, y, &bbox, 0);
					fz_drop_glyph(ctx, glyph);
				}
				else
				{
					fz_path *path = fz_outline_glyph(ctx, span->font, gid, tm);
					if (path)
					{
						fz_pixmap *old_dest;
						float white = 1;

						state = &dev->stack[dev->top];
						old_dest = state[0].dest;
						state[0].dest = state[0].mask;
						state[0].mask = NULL;
						fz_try(ctx)
						{
							fz_draw_stroke_path(ctx, devp, path, stroke, in_ctm, fz_device_gray(ctx), &white, 1, fz_default_color_params);
						}
						fz_always(ctx)
						{
							state[0].mask = state[0].dest;
							state[0].dest = old_dest;
							fz_drop_path(ctx, path);
						}
						fz_catch(ctx)
						{
							fz_rethrow(ctx);
						}
					}
					else
					{
						fz_warn(ctx, "cannot render glyph for stroked clipping");
					}
				}
			}
		}
	}
}

static void
fz_draw_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
}

static void
fz_draw_fill_shade(fz_context *ctx, fz_device *devp, fz_shade *shade, fz_matrix in_ctm, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_irect bbox, scissor;
	fz_pixmap *dest, *shape, *group_alpha;
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	unsigned char alpha_byte = 255 * alpha;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_overprint op = { { 0 } };
	fz_overprint *eop;
	fz_colorspace *colorspace = fz_default_colorspace(ctx, dev->default_cs, shade->colorspace);

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	scissor = state->scissor;
	bbox = fz_irect_from_rect(fz_bound_shade(ctx, shade, ctm));
	bbox = fz_intersect_irect(bbox, scissor);

	if (fz_is_empty_irect(bbox))
		return;

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		state = fz_knockout_begin(ctx, dev);

	fz_var(dest);
	fz_var(shape);
	fz_var(group_alpha);

	dest = state->dest;
	shape = state->shape;
	group_alpha = state->group_alpha;

	fz_try(ctx)
	{
		if (alpha < 1)
		{
			dest = fz_new_pixmap_with_bbox(ctx, state->dest->colorspace, bbox, state->dest->seps, state->dest->alpha);
			if (state->dest->alpha)
				fz_clear_pixmap(ctx, dest);
			else
				fz_copy_pixmap_rect(ctx, dest, state[0].dest, bbox, dev->default_cs);
			if (shape)
			{
				shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
				fz_clear_pixmap(ctx, shape);
			}
			if (group_alpha)
			{
				group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
				fz_clear_pixmap(ctx, group_alpha);
			}
		}

		if (shade->use_background)
		{
			unsigned char *s;
			int x, y, n, i;

			/* Disable OPM */
			color_params.opm = 0;

			eop = resolve_color(ctx, &op, shade->background, colorspace, alpha, color_params, colorbv, state->dest);

			n = dest->n;
			if (fz_overprint_required(eop))
			{
				for (y = scissor.y0; y < scissor.y1; y++)
				{
					s = dest->samples + (unsigned int)((y - dest->y) * dest->stride + (scissor.x0 - dest->x) * n);
					for (x = scissor.x0; x < scissor.x1; x++)
					{
						for (i = 0; i < n; i++)
							if (fz_overprint_component(eop, i))
								*s++ = colorbv[i];
					}
				}
			}
			else
			{
				for (y = scissor.y0; y < scissor.y1; y++)
				{
					s = dest->samples + (unsigned int)((y - dest->y) * dest->stride + (scissor.x0 - dest->x) * n);
					for (x = scissor.x0; x < scissor.x1; x++)
					{
						for (i = 0; i < n; i++)
							*s++ = colorbv[i];
					}
				}
			}
			if (shape)
			{
				for (y = scissor.y0; y < scissor.y1; y++)
				{
					s = shape->samples + (unsigned int)((y - shape->y) * shape->stride + (scissor.x0 - shape->x));
					for (x = scissor.x0; x < scissor.x1; x++)
					{
						*s++ = 255;
					}
				}
			}
			if (group_alpha)
			{
				for (y = scissor.y0; y < scissor.y1; y++)
				{
					s = group_alpha->samples + (unsigned int)((y - group_alpha->y) * group_alpha->stride + (scissor.x0 - group_alpha->x));
					for (x = scissor.x0; x < scissor.x1; x++)
					{
						*s++ = alpha_byte;
					}
				}
			}
		}

		if (color_params.op)
			eop = set_op_from_spaces(ctx, &op, dest, colorspace, 0);
		else
			eop = NULL;

		fz_paint_shade(ctx, shade, colorspace, ctm, dest, color_params, bbox, eop);
		if (shape)
			fz_clear_pixmap_rect_with_value(ctx, shape, 255, bbox);
		if (group_alpha)
			fz_clear_pixmap_rect_with_value(ctx, group_alpha, 255, bbox);

#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "");
		fz_dump_blend(ctx, "Shade ", dest);
		if (shape)
			fz_dump_blend(ctx, "/S=", shape);
		if (group_alpha)
			fz_dump_blend(ctx, "/GA=", group_alpha);
		printf("\n");
#endif

		if (alpha < 1)
		{
			/* FIXME: eop */
			fz_paint_pixmap(state->dest, dest, alpha * 255);
			fz_drop_pixmap(ctx, dest);
			dest = NULL;

			if (shape)
			{
				fz_paint_pixmap(state->shape, shape, 255);
				fz_drop_pixmap(ctx, shape);
				shape = NULL;
			}

			if (group_alpha)
			{
				fz_paint_pixmap(state->group_alpha, group_alpha, alpha * 255);
				fz_drop_pixmap(ctx, group_alpha);
				group_alpha = NULL;
			}
		}

		if (state->blendmode & FZ_BLEND_KNOCKOUT)
			fz_knockout_end(ctx, dev);
	}
	fz_catch(ctx)
	{
		if (dest != state[0].dest) fz_drop_pixmap(ctx, dest);
		if (shape != state[0].shape) fz_drop_pixmap(ctx, shape);
		if (group_alpha != state[0].group_alpha) fz_drop_pixmap(ctx, group_alpha);
		fz_rethrow(ctx);
	}
}

static fz_pixmap *
fz_transform_pixmap(fz_context *ctx, fz_draw_device *dev, const fz_pixmap *image, fz_matrix *ctm, int x, int y, int dx, int dy, int gridfit, const fz_irect *clip)
{
	fz_pixmap *scaled;

	if (ctm->a != 0 && ctm->b == 0 && ctm->c == 0 && ctm->d != 0)
	{
		/* Unrotated or X-flip or Y-flip or XY-flip */
		fz_matrix m = *ctm;
		if (gridfit)
		{
			m = fz_gridfit_matrix(dev->flags & FZ_DEVFLAG_GRIDFIT_AS_TILED, m);
		}
		scaled = fz_scale_pixmap_cached(ctx, image, m.e, m.f, m.a, m.d, clip, dev->cache_x, dev->cache_y);
		if (!scaled)
			return NULL;
		ctm->a = scaled->w;
		ctm->d = scaled->h;
		ctm->e = scaled->x;
		ctm->f = scaled->y;
		return scaled;
	}

	if (ctm->a == 0 && ctm->b != 0 && ctm->c != 0 && ctm->d == 0)
	{
		/* Other orthogonal flip/rotation cases */
		fz_matrix m = *ctm;
		fz_irect rclip;
		if (gridfit)
			m = fz_gridfit_matrix(dev->flags & FZ_DEVFLAG_GRIDFIT_AS_TILED, m);
		if (clip)
		{
			rclip.x0 = clip->y0;
			rclip.y0 = clip->x0;
			rclip.x1 = clip->y1;
			rclip.y1 = clip->x1;
		}
		scaled = fz_scale_pixmap_cached(ctx, image, m.f, m.e, m.b, m.c, (clip ? &rclip : NULL), dev->cache_x, dev->cache_y);
		if (!scaled)
			return NULL;
		ctm->b = scaled->w;
		ctm->c = scaled->h;
		ctm->f = scaled->x;
		ctm->e = scaled->y;
		return scaled;
	}

	/* Downscale, non rectilinear case */
	if (dx > 0 && dy > 0)
	{
		scaled = fz_scale_pixmap_cached(ctx, image, 0, 0, dx, dy, NULL, dev->cache_x, dev->cache_y);
		return scaled;
	}

	return NULL;
}

int
fz_default_image_scale(void *arg, int dst_w, int dst_h, int src_w, int src_h)
{
	(void)arg;
	return dst_w < src_w && dst_h < src_h;
}

static fz_pixmap *
convert_pixmap_for_painting(fz_context *ctx, fz_pixmap *pixmap, fz_colorspace *model, fz_colorspace *src_cs, fz_pixmap *dest, fz_color_params color_params, fz_draw_device *dev, fz_overprint **eop)
{
	fz_pixmap *converted;

	if (fz_colorspace_is_device_n(ctx, src_cs) && dest->seps)
	{
		converted = fz_clone_pixmap_area_with_different_seps(ctx, pixmap, NULL, model, dest->seps, color_params, dev->default_cs);
		*eop = set_op_from_spaces(ctx, *eop, dest, src_cs, 0);
	}
	else
	{
		converted = fz_convert_pixmap(ctx, pixmap, model, NULL, dev->default_cs, color_params, 1);
		if (*eop)
		{
			if (fz_colorspace_type(ctx, model) != FZ_COLORSPACE_CMYK)
			{
				/* Can only overprint to CMYK based spaces */
				*eop = NULL;
			}
			else if (!fz_colorspace_is_device_n(ctx, pixmap->colorspace))
			{
				int i;
				int n = dest->n - dest->alpha;
				for (i = 4; i < n; i++)
					fz_set_overprint(*eop, i);
			}
			else
			{
				*eop = set_op_from_spaces(ctx, *eop, dest, src_cs, 0);
			}
		}
	}
	fz_drop_pixmap(ctx, pixmap);

	return converted;
}

static void
fz_draw_fill_image(fz_context *ctx, fz_device *devp, fz_image *image, fz_matrix in_ctm, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix local_ctm = fz_concat(in_ctm, dev->transform);
	fz_pixmap *pixmap;
	int after;
	int dx, dy;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_colorspace *model;
	fz_irect clip;
	fz_matrix inverse;
	fz_irect src_area;
	fz_colorspace *src_cs;
	fz_overprint op = { { 0 } };
	fz_overprint *eop = &op;

	if (alpha == 0)
		return;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);
	model = state->dest->colorspace;

	clip = fz_intersect_irect(fz_pixmap_bbox(ctx, state->dest), state->scissor);

	if (image->w == 0 || image->h == 0)
		return;

	if (color_params.op == 0)
		eop = NULL;

	/* ctm maps the image (expressed as the unit square) onto the
	 * destination device. Reverse that to get a mapping from
	 * the destination device to the source pixels. */
	if (fz_try_invert_matrix(&inverse, local_ctm))
	{
		/* Not invertible. Could just bail? Use the whole image
		 * for now. */
		src_area.x0 = 0;
		src_area.x1 = image->w;
		src_area.y0 = 0;
		src_area.y1 = image->h;
	}
	else
	{
		float exp;
		fz_rect rect;
		fz_irect sane;
		/* We want to scale from image coords, not from unit square */
		inverse = fz_post_scale(inverse, image->w, image->h);
		/* Are we scaling up or down? exp < 1 means scaling down. */
		exp = fz_matrix_max_expansion(inverse);
		rect = fz_rect_from_irect(clip);
		rect = fz_transform_rect(rect, inverse);
		/* Allow for support requirements for scalers. */
		rect = fz_expand_rect(rect, fz_max(exp, 1) * 4);
		src_area = fz_irect_from_rect(rect);
		sane.x0 = 0;
		sane.y0 = 0;
		sane.x1 = image->w;
		sane.y1 = image->h;
		src_area = fz_intersect_irect(src_area, sane);
		if (fz_is_empty_irect(src_area))
			return;
	}

	pixmap = fz_get_pixmap_from_image(ctx, image, &src_area, &local_ctm, &dx, &dy);
	src_cs = fz_default_colorspace(ctx, dev->default_cs, pixmap->colorspace);

	/* convert images with more components (cmyk->rgb) before scaling */
	/* convert images with fewer components (gray->rgb) after scaling */
	/* convert images with expensive colorspace transforms after scaling */

	fz_var(pixmap);

	fz_try(ctx)
	{
		int conversion_required = (src_cs != model || state->dest->seps);

		if (state->blendmode & FZ_BLEND_KNOCKOUT)
			state = fz_knockout_begin(ctx, dev);

		switch (fz_colorspace_type(ctx, src_cs))
		{
		case FZ_COLORSPACE_GRAY:
			after = 1;
			break;
		case FZ_COLORSPACE_INDEXED:
			after = 0;
			break;
		default:
			if (fz_colorspace_n(ctx, src_cs) <= fz_colorspace_n(ctx, model))
				after = 1;
			else
				after = 0;
			break;
		}

		if (conversion_required && !after)
			pixmap = convert_pixmap_for_painting(ctx, pixmap, model, src_cs, state->dest, color_params, dev, &eop);

		if (!(devp->hints & FZ_DONT_INTERPOLATE_IMAGES) && ctx->tuning->image_scale(ctx->tuning->image_scale_arg, dx, dy, pixmap->w, pixmap->h))
		{
			int gridfit = alpha == 1.0f && !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
			fz_pixmap *scaled = fz_transform_pixmap(ctx, dev, pixmap, &local_ctm, state->dest->x, state->dest->y, dx, dy, gridfit, &clip);
			if (!scaled)
			{
				if (dx < 1)
					dx = 1;
				if (dy < 1)
					dy = 1;
				scaled = fz_scale_pixmap_cached(ctx, pixmap, pixmap->x, pixmap->y, dx, dy, NULL, dev->cache_x, dev->cache_y);
			}
			if (scaled)
			{
				fz_drop_pixmap(ctx, pixmap);
				pixmap = scaled;
			}
		}

		if (conversion_required && after)
		{
#if FZ_PLOTTERS_RGB
			if (state->dest->seps == NULL &&
				((src_cs == fz_device_gray(ctx) && model == fz_device_rgb(ctx)) ||
				(src_cs == fz_device_gray(ctx) && model == fz_device_bgr(ctx))))
			{
				/* We have special case rendering code for gray -> rgb/bgr */
			}
			else
#endif
				pixmap = convert_pixmap_for_painting(ctx, pixmap, model, src_cs, state->dest, color_params, dev, &eop);
		}

		fz_paint_image(ctx, state->dest, &state->scissor, state->shape, state->group_alpha, pixmap, local_ctm, alpha * 255, !(devp->hints & FZ_DONT_INTERPOLATE_IMAGES), devp->flags & FZ_DEVFLAG_GRIDFIT_AS_TILED, eop);

		if (state->blendmode & FZ_BLEND_KNOCKOUT)
			fz_knockout_end(ctx, dev);
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pixmap);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fz_draw_fill_image_mask(fz_context *ctx, fz_device *devp, fz_image *image, fz_matrix in_ctm,
	fz_colorspace *colorspace_in, const float *color, float alpha, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix local_ctm = fz_concat(in_ctm, dev->transform);
	unsigned char colorbv[FZ_MAX_COLORS + 1];
	fz_pixmap *scaled = NULL;
	fz_pixmap *pixmap;
	int dx, dy;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_irect clip;
	fz_matrix inverse;
	fz_irect src_area;
	fz_colorspace *colorspace = NULL;
	fz_overprint op = { { 0 } };
	fz_overprint *eop;

	if (alpha == 0)
		return;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	if (colorspace_in)
		colorspace = fz_default_colorspace(ctx, dev->default_cs, colorspace_in);

	clip = fz_pixmap_bbox(ctx, state->dest);
	clip = fz_intersect_irect(clip, state->scissor);

	if (image->w == 0 || image->h == 0)
		return;

	/* ctm maps the image (expressed as the unit square) onto the
	 * destination device. Reverse that to get a mapping from
	 * the destination device to the source pixels. */
	if (fz_try_invert_matrix(&inverse, local_ctm))
	{
		/* Not invertible. Could just bail? Use the whole image
		 * for now. */
		src_area.x0 = 0;
		src_area.x1 = image->w;
		src_area.y0 = 0;
		src_area.y1 = image->h;
	}
	else
	{
		float exp;
		fz_rect rect;
		fz_irect sane;
		/* We want to scale from image coords, not from unit square */
		inverse = fz_post_scale(inverse, image->w, image->h);
		/* Are we scaling up or down? exp < 1 means scaling down. */
		exp = fz_matrix_max_expansion(inverse);
		rect = fz_rect_from_irect(clip);
		rect = fz_transform_rect(rect, inverse);
		/* Allow for support requirements for scalers. */
		rect = fz_expand_rect(rect, fz_max(exp, 1) * 4);
		src_area = fz_irect_from_rect(rect);
		sane.x0 = 0;
		sane.y0 = 0;
		sane.x1 = image->w;
		sane.y1 = image->h;
		src_area = fz_intersect_irect(src_area, sane);
		if (fz_is_empty_irect(src_area))
			return;
	}

	pixmap = fz_get_pixmap_from_image(ctx, image, &src_area, &local_ctm, &dx, &dy);

	fz_var(pixmap);

	fz_try(ctx)
	{
		if (state->blendmode & FZ_BLEND_KNOCKOUT)
			state = fz_knockout_begin(ctx, dev);

		if (!(devp->hints & FZ_DONT_INTERPOLATE_IMAGES) && ctx->tuning->image_scale(ctx->tuning->image_scale_arg, dx, dy, pixmap->w, pixmap->h))
		{
			int gridfit = alpha == 1.0f && !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
			scaled = fz_transform_pixmap(ctx, dev, pixmap, &local_ctm, state->dest->x, state->dest->y, dx, dy, gridfit, &clip);
			if (!scaled)
			{
				if (dx < 1)
					dx = 1;
				if (dy < 1)
					dy = 1;
				scaled = fz_scale_pixmap_cached(ctx, pixmap, pixmap->x, pixmap->y, dx, dy, NULL, dev->cache_x, dev->cache_y);
			}
			if (scaled)
			{
				fz_drop_pixmap(ctx, pixmap);
				pixmap = scaled;
			}
		}

		eop = resolve_color(ctx, &op, color, colorspace, alpha, color_params, colorbv, state->dest);

		fz_paint_image_with_color(ctx, state->dest, &state->scissor, state->shape, state->group_alpha, pixmap, local_ctm, colorbv, !(devp->hints & FZ_DONT_INTERPOLATE_IMAGES), devp->flags & FZ_DEVFLAG_GRIDFIT_AS_TILED, eop);

		if (state->blendmode & FZ_BLEND_KNOCKOUT)
			fz_knockout_end(ctx, dev);
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pixmap);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fz_draw_clip_image_mask(fz_context *ctx, fz_device *devp, fz_image *image, fz_matrix in_ctm, fz_rect scissor)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix local_ctm = fz_concat(in_ctm, dev->transform);
	fz_irect bbox;
	fz_pixmap *scaled = NULL;
	fz_pixmap *pixmap = NULL;
	int dx, dy;
	fz_draw_state *state = push_stack(ctx, dev, "clip image mask");
	fz_colorspace *model = state->dest->colorspace;
	fz_irect clip;

	fz_var(pixmap);

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	clip = fz_pixmap_bbox(ctx, state->dest);
	clip = fz_intersect_irect(clip, state->scissor);

	if (image->w == 0 || image->h == 0)
	{
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top-1, "Clip (image mask) (empty) begin\n");
#endif
		state[1].scissor = fz_empty_irect;
		state[1].mask = NULL;
		return;
	}

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Clip (image mask) begin\n");
#endif

	bbox = fz_irect_from_rect(fz_transform_rect(fz_unit_rect, local_ctm));
	bbox = fz_intersect_irect(bbox, state->scissor);
	if (!fz_is_infinite_rect(scissor))
	{
		fz_rect tscissor = fz_transform_rect(scissor, dev->transform);
		bbox = fz_intersect_irect(bbox, fz_irect_from_rect(tscissor));
	}

	fz_try(ctx)
	{
		pixmap = fz_get_pixmap_from_image(ctx, image, NULL, &local_ctm, &dx, &dy);

		state[1].mask = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].mask);

		state[1].dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, state[0].dest->alpha);
		fz_copy_pixmap_rect(ctx, state[1].dest, state[0].dest, bbox, dev->default_cs);
		if (state[0].shape)
		{
			state[1].shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
			fz_clear_pixmap(ctx, state[1].shape);
		}
		if (state[0].group_alpha)
		{
			state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
			fz_clear_pixmap(ctx, state[1].group_alpha);
		}

		state[1].blendmode |= FZ_BLEND_ISOLATED;
		state[1].scissor = bbox;

		if (!(devp->hints & FZ_DONT_INTERPOLATE_IMAGES) && ctx->tuning->image_scale(ctx->tuning->image_scale_arg, dx, dy, pixmap->w, pixmap->h))
		{
			int gridfit = !(dev->flags & FZ_DRAWDEV_FLAGS_TYPE3);
			scaled = fz_transform_pixmap(ctx, dev, pixmap, &local_ctm, state->dest->x, state->dest->y, dx, dy, gridfit, &clip);
			if (!scaled)
			{
				if (dx < 1)
					dx = 1;
				if (dy < 1)
					dy = 1;
				scaled = fz_scale_pixmap_cached(ctx, pixmap, pixmap->x, pixmap->y, dx, dy, NULL, dev->cache_x, dev->cache_y);
			}
			if (scaled)
			{
				fz_drop_pixmap(ctx, pixmap);
				pixmap = scaled;
			}
		}

#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "");
		fz_dump_blend(ctx, "Creating imagemask: plotting ", pixmap);
		fz_dump_blend(ctx, " onto ", state[1].mask);
		if (state[1].shape)
			fz_dump_blend(ctx, "/S=", state[1].shape);
		if (state[1].group_alpha)
			fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
#endif

		fz_paint_image(ctx, state[1].mask, &bbox, state[1].shape, state[1].group_alpha, pixmap, local_ctm, 255, !(devp->hints & FZ_DONT_INTERPOLATE_IMAGES), devp->flags & FZ_DEVFLAG_GRIDFIT_AS_TILED, 0);

#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(ctx, " to get ", state[1].mask);
		if (state[1].shape)
			fz_dump_blend(ctx, "/S=", state[1].shape);
		if (state[1].group_alpha)
			fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
		printf("\n");
#endif
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pixmap);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fz_draw_pop_clip(fz_context *ctx, fz_device *devp)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_draw_state *state;

	if (dev->top == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected pop clip");

	state = pop_stack(ctx, dev, "clip");

	/* We can get here with state[1].mask == NULL if the clipping actually
	 * resolved to a rectangle earlier.
	 */
	if (state[1].mask)
	{
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "");
		fz_dump_blend(ctx, "Clipping ", state[1].dest);
		if (state[1].shape)
			fz_dump_blend(ctx, "/S=", state[1].shape);
		if (state[1].group_alpha)
			fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
		fz_dump_blend(ctx, " onto ", state[0].dest);
		if (state[0].shape)
			fz_dump_blend(ctx, "/S=", state[0].shape);
		if (state[0].group_alpha)
			fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
		fz_dump_blend(ctx, " with ", state[1].mask);
#endif

		fz_paint_pixmap_with_mask(state[0].dest, state[1].dest, state[1].mask);
		if (state[0].shape != state[1].shape)
		{
			fz_paint_pixmap_with_mask(state[0].shape, state[1].shape, state[1].mask);
			fz_drop_pixmap(ctx, state[1].shape);
			state[1].shape = NULL;
		}
		if (state[0].group_alpha != state[1].group_alpha)
		{
			fz_paint_pixmap_with_mask(state[0].group_alpha, state[1].group_alpha, state[1].mask);
			fz_drop_pixmap(ctx, state[1].group_alpha);
			state[1].group_alpha = NULL;
		}
		fz_drop_pixmap(ctx, state[1].mask);
		state[1].mask = NULL;
		fz_drop_pixmap(ctx, state[1].dest);
		state[1].dest = NULL;

#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(ctx, " to get ", state[0].dest);
		if (state[0].shape)
			fz_dump_blend(ctx, "/S=", state[0].shape);
		if (state[0].group_alpha)
			fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
		printf("\n");
#endif
	}
	else
	{
#ifdef DUMP_GROUP_BLENDS
		dump_spaces(dev->top, "Clip end\n");
#endif
	}
}

static void
fz_draw_begin_mask(fz_context *ctx, fz_device *devp, fz_rect area, int luminosity, fz_colorspace *colorspace_in, const float *colorfv, fz_color_params color_params)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_pixmap *dest;
	fz_irect bbox;
	fz_draw_state *state = push_stack(ctx, dev, "mask");
	fz_pixmap *shape = state->shape;
	fz_pixmap *group_alpha = state->group_alpha;
	fz_rect trect;
	fz_colorspace *colorspace = NULL;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, color_params, dev->default_cs);

	if (colorspace_in)
		colorspace = fz_default_colorspace(ctx, dev->default_cs, colorspace_in);

	trect = fz_transform_rect(area, dev->transform);
	bbox = fz_intersect_irect(fz_irect_from_rect(trect), state->scissor);

	/* Reset the blendmode for the mask rendering. In particular,
	 * don't carry forward knockout or isolated. */
	state[1].blendmode = 0;

	/* If luminosity, then we generate a mask from the greyscale value of the shapes.
	 * If !luminosity, then we generate a mask from the alpha value of the shapes.
	 */
	if (luminosity)
		state[1].dest = dest = fz_new_pixmap_with_bbox(ctx, fz_device_gray(ctx), bbox, NULL, 0);
	else
		state[1].dest = dest = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
	if (state->shape)
	{
		/* FIXME: If we ever want to support AIS true, then
		 * we probably want to create a shape pixmap here,
		 * using: shape = fz_new_pixmap_with_bbox(NULL, bbox);
		 * then, in the end_mask code, we create the mask
		 * from this rather than dest.
		 */
		state[1].shape = shape = NULL;
	}
	if (state->group_alpha)
	{
		state[1].group_alpha = group_alpha = NULL;
	}

	if (luminosity)
	{
		float bc;
		if (!colorspace)
			colorspace = fz_device_gray(ctx);
		fz_convert_color(ctx, colorspace, colorfv, fz_device_gray(ctx), &bc, NULL, color_params);
		fz_clear_pixmap_with_value(ctx, dest, bc * 255);
		if (shape)
			fz_clear_pixmap_with_value(ctx, shape, 255);
		if (group_alpha)
			fz_clear_pixmap_with_value(ctx, group_alpha, 255);
	}
	else
	{
		fz_clear_pixmap(ctx, dest);
		if (shape)
			fz_clear_pixmap(ctx, shape);
		if (group_alpha)
			fz_clear_pixmap(ctx, group_alpha);
	}

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Mask begin\n");
#endif
	state[1].scissor = bbox;
}

static void
fz_draw_end_mask(fz_context *ctx, fz_device *devp)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_pixmap *temp, *dest;
	fz_irect bbox;
	fz_draw_state *state;

	if (dev->top == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected end mask");

	state = convert_stack(ctx, dev, "mask");

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Mask -> Clip: ");
	fz_dump_blend(ctx, "Mask ", state[1].dest);
	if (state[1].shape)
		fz_dump_blend(ctx, "/S=", state[1].shape);
	if (state[1].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
#endif
	{
		/* convert to alpha mask */
		temp = fz_alpha_from_gray(ctx, state[1].dest);
		if (state[1].mask != state[0].mask)
			fz_drop_pixmap(ctx, state[1].mask);
		state[1].mask = temp;
		if (state[1].dest != state[0].dest)
			fz_drop_pixmap(ctx, state[1].dest);
		state[1].dest = NULL;
		if (state[1].shape != state[0].shape)
			fz_drop_pixmap(ctx, state[1].shape);
		state[1].shape = NULL;
		if (state[1].group_alpha != state[0].group_alpha)
			fz_drop_pixmap(ctx, state[1].group_alpha);
		state[1].group_alpha = NULL;

#ifdef DUMP_GROUP_BLENDS
		fz_dump_blend(ctx, "-> Clip ", temp);
		printf("\n");
#endif

		/* create new dest scratch buffer */
		bbox = fz_pixmap_bbox(ctx, temp);
		dest = fz_new_pixmap_with_bbox(ctx, state->dest->colorspace, bbox, state->dest->seps, state->dest->alpha);
		fz_copy_pixmap_rect(ctx, dest, state->dest, bbox, dev->default_cs);

		/* push soft mask as clip mask */
		state[1].dest = dest;
		state[1].blendmode |= FZ_BLEND_ISOLATED;
		/* If we have a shape, then it'll need to be masked with the
		 * clip mask when we pop. So create a new shape now. */
		if (state[0].shape)
		{
			state[1].shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
			fz_clear_pixmap(ctx, state[1].shape);
		}
		if (state[0].group_alpha)
		{
			state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
			fz_clear_pixmap(ctx, state[1].group_alpha);
		}
		state[1].scissor = bbox;
	}
}

static void
fz_draw_begin_group(fz_context *ctx, fz_device *devp, fz_rect area, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_irect bbox;
	fz_pixmap *dest;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_colorspace *model = state->dest->colorspace;
	fz_rect trect;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	if (cs != NULL)
		model = fz_default_colorspace(ctx, dev->default_cs, cs);

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(ctx, dev);

	state = push_stack(ctx, dev, "group");

	trect = fz_transform_rect(area, dev->transform);
	bbox = fz_intersect_irect(fz_irect_from_rect(trect), state->scissor);

#ifndef ATTEMPT_KNOCKOUT_AND_ISOLATED
	knockout = 0;
	isolated = 1;
#endif

	state[1].dest = dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, state[0].dest->alpha || isolated);

	if (isolated)
	{
		fz_clear_pixmap(ctx, dest);
		state[1].group_alpha = NULL;
	}
	else
	{
		fz_copy_pixmap_rect(ctx, dest, state[0].dest, bbox, dev->default_cs);
		state[1].group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, state[1].group_alpha);
	}

	/* shape is inherited from the previous group */
	state[1].alpha = alpha;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "");
	{
		char text[240];
		char atext[80];
		char btext[80];
		if (alpha != 1)
			sprintf(atext, " (alpha %g)", alpha);
		else
			atext[0] = 0;
		if (blendmode != 0)
			sprintf(btext, " (blend %d)", blendmode);
		else
			btext[0] = 0;
		sprintf(text, "Group begin%s%s%s%s: background is ", isolated ? " (isolated)" : "", knockout ? " (knockout)" : "", atext, btext);
		fz_dump_blend(ctx, text, state[1].dest);
	}
	if (state[1].shape)
		fz_dump_blend(ctx, "/S=", state[1].shape);
	if (state[1].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
	printf("\n");
#endif

	state[1].scissor = bbox;
	state[1].blendmode = blendmode | (isolated ? FZ_BLEND_ISOLATED : 0) | (knockout ? FZ_BLEND_KNOCKOUT : 0);
}

static void
fz_draw_end_group(fz_context *ctx, fz_device *devp)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	int blendmode;
	int isolated;
	float alpha;
	fz_draw_state *state;

	if (dev->top == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected end group");

	state = pop_stack(ctx, dev, "group");

	alpha = state[1].alpha;
	blendmode = state[1].blendmode & FZ_BLEND_MODEMASK;
	isolated = state[1].blendmode & FZ_BLEND_ISOLATED;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "");
	fz_dump_blend(ctx, "Group end: blending ", state[1].dest);
	if (state[1].shape)
		fz_dump_blend(ctx, "/S=", state[1].shape);
	if (state[1].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
	fz_dump_blend(ctx, " onto ", state[0].dest);
	if (state[0].shape)
		fz_dump_blend(ctx, "/S=", state[0].shape);
	if (state[0].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
	if (alpha != 1.0f)
		printf(" (alpha %g)", alpha);
	if (blendmode != 0)
		printf(" (blend %d)", blendmode);
	if (isolated != 0)
		printf(" (isolated)");
	if (state[1].blendmode & FZ_BLEND_KNOCKOUT)
		printf(" (knockout)");
#endif

	if (state[0].dest->colorspace != state[1].dest->colorspace)
	{
		fz_pixmap *converted = fz_convert_pixmap(ctx, state[1].dest, state[0].dest->colorspace, NULL, dev->default_cs, fz_default_color_params, 1);
		fz_drop_pixmap(ctx, state[1].dest);
		state[1].dest = converted;
	}

	if ((blendmode == 0) && (state[0].shape == state[1].shape) && (state[0].group_alpha == state[1].group_alpha))
		fz_paint_pixmap(state[0].dest, state[1].dest, alpha * 255);
	else
		fz_blend_pixmap(ctx, state[0].dest, state[1].dest, alpha * 255, blendmode, isolated, state[1].group_alpha);

	if (state[0].shape != state[1].shape)
	{
		/* The 'D' on page 7 of Altona_Technical_v20_x4.pdf goes wrong if this
		 * isn't alpha * 255, as the blend back fails to take account of alpha. */
		if (state[0].shape)
		{
			if (state[1].shape)
				fz_paint_pixmap(state[0].shape, state[1].shape, alpha * 255);
			else
				fz_paint_pixmap_alpha(state[0].shape, state[1].dest, alpha * 255);
		}
	}
	assert(state[0].group_alpha == NULL || state[0].group_alpha != state[1].group_alpha);
	if (state[0].group_alpha && state[0].group_alpha != state[1].group_alpha)
	{
		/* The 'D' on page 7 of Altona_Technical_v20_x4.pdf uses an isolated group,
		 * and goes wrong if this is 255 * alpha, as an alpha effectively gets
		 * applied twice. CATX5233 page 7 uses a non-isolated group, and goes wrong
		 * if alpha isn't applied here. */
		if (state[1].group_alpha)
			fz_paint_pixmap(state[0].group_alpha, state[1].group_alpha, isolated ? 255 : alpha * 255);
		else
			fz_paint_pixmap_alpha(state[0].group_alpha, state[1].dest, isolated ? 255 : alpha * 255);
	}

	assert(state[0].dest != state[1].dest);

#ifdef DUMP_GROUP_BLENDS
	fz_dump_blend(ctx, " to get ", state[0].dest);
	if (state[0].shape)
		fz_dump_blend(ctx, "/S=", state[0].shape);
	if (state[0].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
	printf("\n");
#endif

	if (state[0].shape != state[1].shape)
	{
		fz_drop_pixmap(ctx, state[1].shape);
		state[1].shape = NULL;
	}
	fz_drop_pixmap(ctx, state[1].group_alpha);
	state[1].group_alpha = NULL;
	fz_drop_pixmap(ctx, state[1].dest);
	state[1].dest = NULL;

	if (state[0].blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(ctx, dev);
}

typedef struct
{
	int refs;
	float ctm[4];
	int id;
	char has_shape;
	char has_group_alpha;
	fz_colorspace *cs;
} tile_key;

typedef struct
{
	fz_storable storable;
	fz_pixmap *dest;
	fz_pixmap *shape;
	fz_pixmap *group_alpha;
} tile_record;

static int
fz_make_hash_tile_key(fz_context *ctx, fz_store_hash *hash, void *key_)
{
	tile_key *key = key_;

	hash->u.im.id = key->id;
	hash->u.im.has_shape = key->has_shape;
	hash->u.im.has_group_alpha = key->has_group_alpha;
	hash->u.im.m[0] = key->ctm[0];
	hash->u.im.m[1] = key->ctm[1];
	hash->u.im.m[2] = key->ctm[2];
	hash->u.im.m[3] = key->ctm[3];
	hash->u.im.ptr = key->cs;
	return 1;
}

static void *
fz_keep_tile_key(fz_context *ctx, void *key_)
{
	tile_key *key = key_;
	return fz_keep_imp(ctx, key, &key->refs);
}

static void
fz_drop_tile_key(fz_context *ctx, void *key_)
{
	tile_key *key = key_;
	if (fz_drop_imp(ctx, key, &key->refs))
	{
		fz_drop_colorspace_store_key(ctx, key->cs);
		fz_free(ctx, key);
	}
}

static int
fz_cmp_tile_key(fz_context *ctx, void *k0_, void *k1_)
{
	tile_key *k0 = k0_;
	tile_key *k1 = k1_;
	return k0->id == k1->id &&
		k0->has_shape == k1->has_shape &&
		k0->has_group_alpha == k1->has_group_alpha &&
		k0->ctm[0] == k1->ctm[0] &&
		k0->ctm[1] == k1->ctm[1] &&
		k0->ctm[2] == k1->ctm[2] &&
		k0->ctm[3] == k1->ctm[3] &&
		k0->cs == k1->cs;
}

static void
fz_format_tile_key(fz_context *ctx, char *s, size_t n, void *key_)
{
	tile_key *key = (tile_key *)key_;
	fz_snprintf(s, n, "(tile id=%x, ctm=%g %g %g %g, cs=%x, shape=%d, ga=%d)",
			key->id, key->ctm[0], key->ctm[1], key->ctm[2], key->ctm[3], key->cs,
			key->has_shape, key->has_group_alpha);
}

static const fz_store_type fz_tile_store_type =
{
	fz_make_hash_tile_key,
	fz_keep_tile_key,
	fz_drop_tile_key,
	fz_cmp_tile_key,
	fz_format_tile_key,
	NULL
};

static void
fz_drop_tile_record_imp(fz_context *ctx, fz_storable *storable)
{
	tile_record *tr = (tile_record *)storable;
	fz_drop_pixmap(ctx, tr->dest);
	fz_drop_pixmap(ctx, tr->shape);
	fz_drop_pixmap(ctx, tr->group_alpha);
	fz_free(ctx, tr);
}

static void
fz_drop_tile_record(fz_context *ctx, tile_record *tile)
{
	fz_drop_storable(ctx, &tile->storable);
}

static tile_record *
fz_new_tile_record(fz_context *ctx, fz_pixmap *dest, fz_pixmap *shape, fz_pixmap *group_alpha)
{
	tile_record *tile = fz_malloc_struct(ctx, tile_record);
	FZ_INIT_STORABLE(tile, 1, fz_drop_tile_record_imp);
	tile->dest = fz_keep_pixmap(ctx, dest);
	tile->shape = fz_keep_pixmap(ctx, shape);
	tile->group_alpha = fz_keep_pixmap(ctx, group_alpha);
	return tile;
}

size_t
fz_tile_size(fz_context *ctx, tile_record *tile)
{
	if (!tile)
		return 0;
	return sizeof(*tile) + fz_pixmap_size(ctx, tile->dest) + fz_pixmap_size(ctx, tile->shape) + fz_pixmap_size(ctx, tile->group_alpha);
}

static int
fz_draw_begin_tile(fz_context *ctx, fz_device *devp, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix in_ctm, int id)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_matrix ctm = fz_concat(in_ctm, dev->transform);
	fz_pixmap *dest = NULL;
	fz_pixmap *shape, *group_alpha;
	fz_irect bbox;
	fz_draw_state *state = &dev->stack[dev->top];
	fz_colorspace *model = state->dest->colorspace;
	fz_rect local_view;

	if (dev->top == 0 && dev->resolve_spots)
		state = push_group_for_separations(ctx, dev, fz_default_color_params /* FIXME */, dev->default_cs);

	/* area, view, xstep, ystep are in pattern space */
	/* ctm maps from pattern space to device space */

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_begin(ctx, dev);

	state = push_stack(ctx, dev, "tile");

	local_view = fz_transform_rect(view, ctm);
	bbox = fz_irect_from_rect(local_view);
	/* We should never have a bbox that entirely covers our destination.
	 * If we do, then the check for only 1 tile being visible above has
	 * failed. Actually, this *can* fail due to the round_rect, at extreme
	 * resolutions, so disable this assert.
	 * assert(bbox.x0 > state->dest->x || bbox.x1 < state->dest->x + state->dest->w ||
	 *	bbox.y0 > state->dest->y || bbox.y1 < state->dest->y + state->dest->h);
	 */

	/* Check to see if we have one cached */
	if (id)
	{
		tile_key tk;
		tile_record *tile;
		tk.ctm[0] = ctm.a;
		tk.ctm[1] = ctm.b;
		tk.ctm[2] = ctm.c;
		tk.ctm[3] = ctm.d;
		tk.id = id;
		tk.cs = state[1].dest->colorspace;
		tk.has_shape = (state[1].shape != NULL);
		tk.has_group_alpha = (state[1].group_alpha != NULL);

		tile = fz_find_item(ctx, fz_drop_tile_record_imp, &tk, &fz_tile_store_type);
		if (tile)
		{
			state[1].dest = fz_keep_pixmap(ctx, tile->dest);
			state[1].shape = fz_keep_pixmap(ctx, tile->shape);
			state[1].group_alpha = fz_keep_pixmap(ctx, tile->group_alpha);
			state[1].blendmode |= FZ_BLEND_ISOLATED;
			state[1].xstep = xstep;
			state[1].ystep = ystep;
			state[1].id = id;
			state[1].encache = 0;
			state[1].area = fz_irect_from_rect(area);
			state[1].ctm = ctm;
			state[1].scissor = bbox;

#ifdef DUMP_GROUP_BLENDS
			dump_spaces(dev->top-1, "Tile begin (cached)\n");
#endif

			fz_drop_tile_record(ctx, tile);
			return 1;
		}
	}

	/* Patterns can be transparent, so we need to have an alpha here. */
	state[1].dest = dest = fz_new_pixmap_with_bbox(ctx, model, bbox, state[0].dest->seps, 1);
	fz_clear_pixmap(ctx, dest);
	shape = state[0].shape;
	if (shape)
	{
		state[1].shape = shape = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, shape);
	}
	group_alpha = state[0].group_alpha;
	if (group_alpha)
	{
		state[1].group_alpha = group_alpha = fz_new_pixmap_with_bbox(ctx, NULL, bbox, NULL, 1);
		fz_clear_pixmap(ctx, group_alpha);
	}

	state[1].blendmode |= FZ_BLEND_ISOLATED;
	state[1].xstep = xstep;
	state[1].ystep = ystep;
	state[1].id = id;
	state[1].encache = 1;
	state[1].area = fz_irect_from_rect(area);
	state[1].ctm = ctm;
	state[1].scissor = bbox;

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top-1, "Tile begin\n");
#endif

	return 0;
}

static void
fz_draw_end_tile(fz_context *ctx, fz_device *devp)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	float xstep, ystep;
	fz_matrix ttm, ctm, shapectm, gactm;
	fz_irect area, scissor, tile_bbox;
	fz_rect scissor_tmp, tile_tmp;
	int x0, y0, x1, y1, x, y, extra_x, extra_y;
	fz_draw_state *state;
	fz_pixmap *dest = NULL;
	fz_pixmap *shape = NULL;
	fz_pixmap *group_alpha = NULL;

	if (dev->top == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unexpected end tile");

	state = pop_stack(ctx, dev, "tile");

	xstep = state[1].xstep;
	ystep = state[1].ystep;
	area = state[1].area;
	ctm = state[1].ctm;

	/* Fudge the scissor bbox a little to allow for inaccuracies in the
	 * matrix inversion. */
	ttm = fz_invert_matrix(ctm);
	scissor_tmp = fz_rect_from_irect(state[0].scissor);
	scissor_tmp = fz_expand_rect(scissor_tmp, 1);
	scissor_tmp = fz_transform_rect(scissor_tmp, ttm);
	scissor = fz_irect_from_rect(scissor_tmp);
	area = fz_intersect_irect(area, scissor);

	tile_bbox.x0 = state[1].dest->x;
	tile_bbox.y0 = state[1].dest->y;
	tile_bbox.x1 = state[1].dest->w + tile_bbox.x0;
	tile_bbox.y1 = state[1].dest->h + tile_bbox.y0;
	tile_tmp = fz_rect_from_irect(tile_bbox);
	tile_tmp = fz_expand_rect(tile_tmp, 1);
	tile_tmp = fz_transform_rect(tile_tmp, ttm);

	/* FIXME: area is a bbox, so FP not appropriate here */
	/* In PDF files xstep/ystep can be smaller than view (the area of a
	 * single tile) (see fts_15_1506.pdf for an example). This means that
	 * we have to bias the left hand/bottom edge calculations by the
	 * difference between the step and the width/height of the tile. */
	/* scissor, xstep and area are all in pattern space. */
	extra_x = tile_tmp.x1 - tile_tmp.x0 - xstep;
	if (extra_x < 0)
		extra_x = 0;
	extra_y = tile_tmp.y1 - tile_tmp.y0 - ystep;
	if (extra_y < 0)
		extra_y = 0;
	x0 = floorf((area.x0 - tile_tmp.x0 - extra_x) / xstep);
	y0 = floorf((area.y0 - tile_tmp.y0 - extra_y) / ystep);
	x1 = ceilf((area.x1 - tile_tmp.x0 + extra_x) / xstep);
	y1 = ceilf((area.y1 - tile_tmp.y0 + extra_y) / ystep);

	ctm.e = state[1].dest->x;
	ctm.f = state[1].dest->y;
	if (state[1].shape)
	{
		shapectm = ctm;
		shapectm.e = state[1].shape->x;
		shapectm.f = state[1].shape->y;
	}
	if (state[1].group_alpha)
	{
		gactm = ctm;
		gactm.e = state[1].group_alpha->x;
		gactm.f = state[1].group_alpha->y;
	}

#ifdef DUMP_GROUP_BLENDS
	dump_spaces(dev->top, "");
	fz_dump_blend(ctx, "Tiling ", state[1].dest);
	if (state[1].shape)
		fz_dump_blend(ctx, "/S=", state[1].shape);
	if (state[1].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[1].group_alpha);
	fz_dump_blend(ctx, " onto ", state[0].dest);
	if (state[0].shape)
		fz_dump_blend(ctx, "/S=", state[0].shape);
	if (state[0].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
#endif
	dest = fz_new_pixmap_from_pixmap(ctx, state[1].dest, NULL);

	fz_var(shape);
	fz_var(group_alpha);

	fz_try(ctx)
	{
		shape = fz_new_pixmap_from_pixmap(ctx, state[1].shape, NULL);
		group_alpha = fz_new_pixmap_from_pixmap(ctx, state[1].group_alpha, NULL);

		for (y = y0; y < y1; y++)
		{
			for (x = x0; x < x1; x++)
			{
				ttm = fz_pre_translate(ctm, x * xstep, y * ystep);
				dest->x = ttm.e;
				dest->y = ttm.f;
				/* Check for overflow due to float -> int conversions */
				if (dest->x > 0 && dest->x + dest->w < 0)
					continue;
				if (dest->y > 0 && dest->y + dest->h < 0)
					continue;
				fz_paint_pixmap_with_bbox(state[0].dest, dest, 255, state[0].scissor);
				if (shape)
				{
					ttm = fz_pre_translate(shapectm, x * xstep, y * ystep);
					shape->x = ttm.e;
					shape->y = ttm.f;
					fz_paint_pixmap_with_bbox(state[0].shape, shape, 255, state[0].scissor);
				}
				if (group_alpha)
				{
					ttm = fz_pre_translate(gactm, x * xstep, y * ystep);
					group_alpha->x = ttm.e;
					group_alpha->y = ttm.f;
					fz_paint_pixmap_with_bbox(state[0].group_alpha, group_alpha, 255, state[0].scissor);
				}
			}
		}
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, dest);
		fz_drop_pixmap(ctx, shape);
		fz_drop_pixmap(ctx, group_alpha);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	/* Now we try to cache the tiles. Any failure here will just result in us not caching. */
	if (state[1].encache && state[1].id != 0)
	{
		tile_record *tile = NULL;
		tile_key *key = NULL;
		fz_var(tile);
		fz_var(key);
		fz_try(ctx)
		{
			tile_record *existing_tile;

			tile = fz_new_tile_record(ctx, state[1].dest, state[1].shape, state[1].group_alpha);

			key = fz_malloc_struct(ctx, tile_key);
			key->refs = 1;
			key->id = state[1].id;
			key->ctm[0] = ctm.a;
			key->ctm[1] = ctm.b;
			key->ctm[2] = ctm.c;
			key->ctm[3] = ctm.d;
			key->cs = fz_keep_colorspace_store_key(ctx, state[1].dest->colorspace);
			key->has_shape = (state[1].shape != NULL);
			key->has_group_alpha = (state[1].group_alpha != NULL);
			existing_tile = fz_store_item(ctx, key, tile, fz_tile_size(ctx, tile), &fz_tile_store_type);
			if (existing_tile)
			{
				/* We already have a tile. This will either have been
				 * produced by a racing thread, or there is already
				 * an entry for this one in the store. */
				fz_drop_tile_record(ctx, tile);
				tile = existing_tile;
			}
		}
		fz_always(ctx)
		{
			fz_drop_tile_key(ctx, key);
			fz_drop_tile_record(ctx, tile);
		}
		fz_catch(ctx)
		{
			/* Do nothing */
		}
	}

	fz_drop_pixmap(ctx, state[1].dest);
	state[1].dest = NULL;
	fz_drop_pixmap(ctx, state[1].shape);
	state[1].shape = NULL;
	fz_drop_pixmap(ctx, state[1].group_alpha);
	state[1].group_alpha = NULL;

#ifdef DUMP_GROUP_BLENDS
	fz_dump_blend(ctx, " to get ", state[0].dest);
	if (state[0].shape)
		fz_dump_blend(ctx, "/S=", state[0].shape);
	if (state[0].group_alpha)
		fz_dump_blend(ctx, "/GA=", state[0].group_alpha);
	printf("\n");
#endif

	if (state->blendmode & FZ_BLEND_KNOCKOUT)
		fz_knockout_end(ctx, dev);
}

static void
fz_draw_render_flags(fz_context *ctx, fz_device *devp, int set, int clear)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	dev->flags = (dev->flags | set ) & ~clear;
}

static void
fz_draw_set_default_colorspaces(fz_context *ctx, fz_device *devp, fz_default_colorspaces *default_cs)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_drop_default_colorspaces(ctx, dev->default_cs);
	dev->default_cs = fz_keep_default_colorspaces(ctx, default_cs);
}

static void
fz_draw_close_device(fz_context *ctx, fz_device *devp)
{
	fz_draw_device *dev = (fz_draw_device*)devp;

	/* pop and free the stacks */
	if (dev->top > dev->resolve_spots)
		fz_throw(ctx, FZ_ERROR_GENERIC, "items left on stack in draw device: %d", dev->top);

	if (dev->resolve_spots && dev->top)
	{
		fz_draw_state *state = &dev->stack[--dev->top];
		fz_try(ctx)
		{
			fz_copy_pixmap_area_converting_seps(ctx, state[1].dest, state[0].dest, dev->proof_cs, fz_default_color_params, dev->default_cs);
			assert(state[1].mask == NULL);
			assert(state[1].shape == NULL);
			assert(state[1].group_alpha == NULL);
		}
		fz_always(ctx)
		{
			fz_drop_pixmap(ctx, state[1].dest);
			state[1].dest = NULL;
		}
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static void
fz_draw_drop_device(fz_context *ctx, fz_device *devp)
{
	fz_draw_device *dev = (fz_draw_device*)devp;
	fz_rasterizer *rast = dev->rast;

	fz_drop_default_colorspaces(ctx, dev->default_cs);
	fz_drop_colorspace(ctx, dev->proof_cs);

	/* pop and free the stacks */
	for (; dev->top > 0; dev->top--)
	{
		fz_draw_state *state = &dev->stack[dev->top - 1];
		if (state[1].mask != state[0].mask)
			fz_drop_pixmap(ctx, state[1].mask);
		if (state[1].dest != state[0].dest)
			fz_drop_pixmap(ctx, state[1].dest);
		if (state[1].shape != state[0].shape)
			fz_drop_pixmap(ctx, state[1].shape);
		if (state[1].group_alpha != state[0].group_alpha)
			fz_drop_pixmap(ctx, state[1].group_alpha);
	}

	/* We never free the dest/mask/shape at level 0, as:
	 * 1) dest is passed in and ownership remains with the caller.
	 * 2) shape and mask are NULL at level 0.
	 */

	if (dev->stack != &dev->init_stack[0])
		fz_free(ctx, dev->stack);
	fz_drop_scale_cache(ctx, dev->cache_x);
	fz_drop_scale_cache(ctx, dev->cache_y);
	fz_drop_rasterizer(ctx, rast);
}

fz_device *
new_draw_device(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, const fz_aa_context *aa, const fz_irect *clip, fz_colorspace *proof_cs)
{
	fz_draw_device *dev = fz_new_derived_device(ctx, fz_draw_device);

	dev->super.drop_device = fz_draw_drop_device;
	dev->super.close_device = fz_draw_close_device;

	dev->super.fill_path = fz_draw_fill_path;
	dev->super.stroke_path = fz_draw_stroke_path;
	dev->super.clip_path = fz_draw_clip_path;
	dev->super.clip_stroke_path = fz_draw_clip_stroke_path;

	dev->super.fill_text = fz_draw_fill_text;
	dev->super.stroke_text = fz_draw_stroke_text;
	dev->super.clip_text = fz_draw_clip_text;
	dev->super.clip_stroke_text = fz_draw_clip_stroke_text;
	dev->super.ignore_text = fz_draw_ignore_text;

	dev->super.fill_image_mask = fz_draw_fill_image_mask;
	dev->super.clip_image_mask = fz_draw_clip_image_mask;
	dev->super.fill_image = fz_draw_fill_image;
	dev->super.fill_shade = fz_draw_fill_shade;

	dev->super.pop_clip = fz_draw_pop_clip;

	dev->super.begin_mask = fz_draw_begin_mask;
	dev->super.end_mask = fz_draw_end_mask;
	dev->super.begin_group = fz_draw_begin_group;
	dev->super.end_group = fz_draw_end_group;

	dev->super.begin_tile = fz_draw_begin_tile;
	dev->super.end_tile = fz_draw_end_tile;

	dev->super.render_flags = fz_draw_render_flags;
	dev->super.set_default_colorspaces = fz_draw_set_default_colorspaces;

	dev->proof_cs = fz_keep_colorspace(ctx, proof_cs);
	dev->transform = transform;
	dev->flags = 0;
	dev->resolve_spots = 0;
	dev->top = 0;
	dev->stack = &dev->init_stack[0];
	dev->stack_cap = STACK_SIZE;
	dev->stack[0].dest = dest;
	dev->stack[0].shape = NULL;
	dev->stack[0].group_alpha = NULL;
	dev->stack[0].mask = NULL;
	dev->stack[0].blendmode = 0;
	dev->stack[0].scissor.x0 = dest->x;
	dev->stack[0].scissor.y0 = dest->y;
	dev->stack[0].scissor.x1 = dest->x + dest->w;
	dev->stack[0].scissor.y1 = dest->y + dest->h;

	if (clip)
	{
		if (clip->x0 > dev->stack[0].scissor.x0)
			dev->stack[0].scissor.x0 = clip->x0;
		if (clip->x1 < dev->stack[0].scissor.x1)
			dev->stack[0].scissor.x1 = clip->x1;
		if (clip->y0 > dev->stack[0].scissor.y0)
			dev->stack[0].scissor.y0 = clip->y0;
		if (clip->y1 < dev->stack[0].scissor.y1)
			dev->stack[0].scissor.y1 = clip->y1;
	}

	/* If we have no separations structure at all, then we want a
	 * simple composite rendering (with no overprint simulation).
	 * If we do have a separations structure, so: 1) Any
	 * 'disabled' separations are ignored. 2) Any 'composite'
	 * separations means we will need to do an overprint
	 * simulation.
	 *
	 * The supplied pixmaps 's' will match the number of
	 * 'spots' separations. If we have any 'composite'
	 * separations therefore, we'll need to make a new pixmap
	 * with a new (completely 'spots') separations structure,
	 * render to that, and then map down at the end.
	 *
	 * Unfortunately we can't produce this until we know what
	 * the default_colorspaces etc are, so set a flag for us
	 * to trigger on later.
	 */
	if (dest->seps || dev->proof_cs != NULL)
#if FZ_ENABLE_SPOT_RENDERING
		dev->resolve_spots = 1;
#else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Spot rendering (and overprint/overprint simulation) not available in this build");
#endif

	fz_try(ctx)
	{
		dev->rast = fz_new_rasterizer(ctx, aa);
		dev->cache_x = fz_new_scale_cache(ctx);
		dev->cache_y = fz_new_scale_cache(ctx);
	}
	fz_catch(ctx)
	{
		fz_drop_device(ctx, (fz_device*)dev);
		fz_rethrow(ctx);
	}

	return (fz_device*)dev;
}

/*
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space in pixels.
*/
fz_device *
fz_new_draw_device(fz_context *ctx, fz_matrix transform, fz_pixmap *dest)
{
	return new_draw_device(ctx, transform, dest, NULL, NULL, NULL);
}

/*
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space in pixels.

	clip: Bounding box to restrict any marking operations of the
	draw device.
*/
fz_device *
fz_new_draw_device_with_bbox(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, const fz_irect *clip)
{
	return new_draw_device(ctx, transform, dest, NULL, clip, NULL);
}

/*
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space in pixels.

	proof_cs: Intermediate color space to map though when mapping to
	color space defined by pixmap.
*/
fz_device *
fz_new_draw_device_with_proof(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, fz_colorspace *cs)
{
	return new_draw_device(ctx, transform, dest, NULL, NULL, cs);
}

/*
	Create a device to draw on a pixmap.

	dest: Target pixmap for the draw device. See fz_new_pixmap*
	for how to obtain a pixmap. The pixmap is not cleared by the
	draw device, see fz_clear_pixmap* for how to clear it prior to
	calling fz_new_draw_device. Free the device by calling
	fz_drop_device.

	transform: Transform from user space in points to device space in pixels.

	clip: Bounding box to restrict any marking operations of the
	draw device.

	proof_cs: Color space to render to prior to mapping to color space defined by pixmap.
*/
fz_device *
fz_new_draw_device_with_bbox_proof(fz_context *ctx, fz_matrix transform, fz_pixmap *dest, const fz_irect *clip, fz_colorspace *cs)
{
	return new_draw_device(ctx, transform, dest, NULL, clip, cs);
}

fz_device *
fz_new_draw_device_type3(fz_context *ctx, fz_matrix transform, fz_pixmap *dest)
{
	fz_draw_device *dev = (fz_draw_device*)fz_new_draw_device(ctx, transform, dest);
	dev->flags |= FZ_DRAWDEV_FLAGS_TYPE3;
	return (fz_device*)dev;
}

fz_irect *
fz_bound_path_accurate(fz_context *ctx, fz_irect *bbox, const fz_irect *scissor, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth)
{
	fz_rasterizer *rast = fz_new_rasterizer(ctx, NULL);

	fz_try(ctx)
	{
		if (stroke)
			(void)fz_flatten_stroke_path(ctx, rast, path, stroke, ctm, flatness, linewidth, scissor, bbox);
		else
			(void)fz_flatten_fill_path(ctx, rast, path, ctm, flatness, scissor, bbox);
	}
	fz_always(ctx)
		fz_drop_rasterizer(ctx, rast);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return bbox;
}

const char *fz_draw_options_usage =
	"Raster output options:\n"
	"\trotate=N: rotate rendered pages N degrees counterclockwise\n"
	"\tresolution=N: set both X and Y resolution in pixels per inch\n"
	"\tx-resolution=N: X resolution of rendered pages in pixels per inch\n"
	"\ty-resolution=N: Y resolution of rendered pages in pixels per inch\n"
	"\twidth=N: render pages to fit N pixels wide (ignore resolution option)\n"
	"\theight=N: render pages to fit N pixels tall (ignore resolution option)\n"
	"\tcolorspace=(gray|rgb|cmyk): render using specified colorspace\n"
	"\talpha: render pages with alpha channel and transparent background\n"
	"\tgraphics=(aaN|cop|app): set the rasterizer to use\n"
	"\ttext=(aaN|cop|app): set the rasterizer to use for text\n"
	"\t\taaN=antialias with N bits (0 to 8)\n"
	"\t\tcop=center of pixel\n"
	"\t\tapp=any part of pixel\n"
	"\n";

static int parse_aa_opts(const char *val)
{
	if (fz_option_eq(val, "cop"))
		return 9;
	if (fz_option_eq(val, "app"))
		return 10;
	if (val[0] == 'a' && val[1] == 'a' && val[2] >= '0' && val[2] <= '9')
		return  fz_clampi(fz_atoi(&val[2]), 0, 8);
	return 8;
}

/*
	Parse draw device options from a comma separated key-value string.
*/
fz_draw_options *
fz_parse_draw_options(fz_context *ctx, fz_draw_options *opts, const char *args)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	opts->x_resolution = 96;
	opts->y_resolution = 96;
	opts->rotate = 0;
	opts->width = 0;
	opts->height = 0;
	opts->colorspace = fz_device_rgb(ctx);
	opts->alpha = 0;
	opts->graphics = fz_aa_level(ctx);
	opts->text = fz_text_aa_level(ctx);

	if (fz_has_option(ctx, args, "rotate", &val))
		opts->rotate = fz_atoi(val);
	if (fz_has_option(ctx, args, "resolution", &val))
		opts->x_resolution = opts->y_resolution = fz_atoi(val);
	if (fz_has_option(ctx, args, "x-resolution", &val))
		opts->x_resolution = fz_atoi(val);
	if (fz_has_option(ctx, args, "y-resolution", &val))
		opts->y_resolution = fz_atoi(val);
	if (fz_has_option(ctx, args, "width", &val))
		opts->width = fz_atoi(val);
	if (fz_has_option(ctx, args, "height", &val))
		opts->height = fz_atoi(val);
	if (fz_has_option(ctx, args, "colorspace", &val))
	{
		if (fz_option_eq(val, "gray") || fz_option_eq(val, "grey") || fz_option_eq(val, "mono"))
			opts->colorspace = fz_device_gray(ctx);
		else if (fz_option_eq(val, "rgb"))
			opts->colorspace = fz_device_rgb(ctx);
		else if (fz_option_eq(val, "cmyk"))
			opts->colorspace = fz_device_cmyk(ctx);
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "unknown colorspace in options");
	}
	if (fz_has_option(ctx, args, "alpha", &val))
		opts->alpha = fz_option_eq(val, "yes");
	if (fz_has_option(ctx, args, "graphics", &val))
		opts->text = opts->graphics = parse_aa_opts(val);
	if (fz_has_option(ctx, args, "text", &val))
		opts->text = parse_aa_opts(val);

	/* Sanity check values */
	if (opts->x_resolution <= 0) opts->x_resolution = 96;
	if (opts->y_resolution <= 0) opts->y_resolution = 96;
	if (opts->width < 0) opts->width = 0;
	if (opts->height < 0) opts->height = 0;

	return opts;
}

/*

	Create a new pixmap and draw device, using the specified options.

	options: Options to configure the draw device, and choose the resolution and colorspace.
	mediabox: The bounds of the page in points.
	pixmap: An out parameter containing the newly created pixmap.
*/
fz_device *
fz_new_draw_device_with_options(fz_context *ctx, const fz_draw_options *opts, fz_rect mediabox, fz_pixmap **pixmap)
{
	fz_aa_context aa = ctx->aa;
	float x_zoom = opts->x_resolution / 72.0f;
	float y_zoom = opts->y_resolution / 72.0f;
	float page_w = mediabox.x1 - mediabox.x0;
	float page_h = mediabox.y1 - mediabox.y0;
	float w = opts->width;
	float h = opts->height;
	float x_scale, y_scale;
	fz_matrix transform;
	fz_irect bbox;
	fz_device *dev;

	fz_set_rasterizer_graphics_aa_level(ctx, &aa, opts->graphics);
	fz_set_rasterizer_text_aa_level(ctx, &aa, opts->text);

	if (w > 0)
	{
		x_scale = w / page_w;
		if (h > 0)
			y_scale = h / page_h;
		else
			y_scale = floorf(page_h * x_scale + 0.5f) / page_h;
	}
	else if (h > 0)
	{
		y_scale = h / page_h;
		x_scale = floorf(page_w * y_scale + 0.5f) / page_w;
	}
	else
	{
		x_scale = floorf(page_w * x_zoom + 0.5f) / page_w;
		y_scale = floorf(page_h * y_zoom + 0.5f) / page_h;
	}

	transform = fz_pre_rotate(fz_scale(x_scale, y_scale), opts->rotate);
	bbox = fz_irect_from_rect(fz_transform_rect(mediabox, transform));

	*pixmap = fz_new_pixmap_with_bbox(ctx, opts->colorspace, bbox, NULL, opts->alpha);
	fz_try(ctx)
	{
		fz_set_pixmap_resolution(ctx, *pixmap, opts->x_resolution, opts->y_resolution);
		if (opts->alpha)
			fz_clear_pixmap(ctx, *pixmap);
		else
			fz_clear_pixmap_with_value(ctx, *pixmap, 255);

		dev = new_draw_device(ctx, transform, *pixmap, &aa, NULL, NULL);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, *pixmap);
		*pixmap = NULL;
		fz_rethrow(ctx);
	}
	return dev;
}
