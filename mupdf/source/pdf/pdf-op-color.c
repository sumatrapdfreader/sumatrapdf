// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

enum {
	UNMARKED_STROKE = 1,
	UNMARKED_FILL = 2
};

typedef struct gstate_stack
{
	struct gstate_stack *next;
	pdf_obj *cs_stroke;
	pdf_obj *cs_fill;
	int unmarked;
	fz_matrix ctm;
} gstate_stack;

typedef struct
{
	pdf_obj *cs;
	float color[FZ_MAX_COLORS];
} cs_color;

#define MAX_REWRITTEN_NAME 32

typedef struct
{
	pdf_obj *im_obj;
	fz_image *after;
	char name[MAX_REWRITTEN_NAME];
} rewritten_image;

typedef struct
{
	int max;
	int len;
	rewritten_image *res;
} rewritten_images;

typedef struct
{
	pdf_obj *before;
	fz_shade *after;
	char name[MAX_REWRITTEN_NAME];
} rewritten_shade;

typedef struct
{
	int max;
	int len;
	rewritten_shade *res;
} rewritten_shades;

typedef struct
{
	pdf_processor super;
	pdf_document *doc;
	int structparents;
	pdf_filter_options *global_options;
	pdf_color_filter_options *options;
	pdf_resource_stack *new_rstack;
	gstate_stack *gstate;
	cs_color *stroke;
	cs_color *fill;
	rewritten_images images;
	rewritten_shades shades;
} pdf_color_processor;

static void
push_rewritten_image(fz_context *ctx, pdf_color_processor *p, pdf_obj *im_obj, fz_image *after, char *name)
{
	rewritten_images *list = &p->images;

	if (list->max == list->len)
	{
		int new_max = list->max * 2;
		if (new_max == 0)
			new_max = 32;
		list->res = fz_realloc(ctx, list->res, sizeof(*list->res) * new_max);
		list->max = new_max;
	}
	list->res[list->len].im_obj = pdf_keep_obj(ctx, im_obj);
	list->res[list->len].after = fz_keep_image(ctx, after);
	memcpy(list->res[list->len].name, name, MAX_REWRITTEN_NAME);
	list->len++;
}

static fz_image *
find_rewritten_image(fz_context *ctx, pdf_color_processor *p, pdf_obj *im_obj, char *name)
{
	rewritten_images *list = &p->images;
	int i;

	for (i = 0; i < list->len; i++)
		if (list->res[i].im_obj == im_obj)
		{
			memcpy(name, list->res[i].name, MAX_REWRITTEN_NAME);
			return list->res[i].after;
		}

	return NULL;
}

static void
drop_rewritten_images(fz_context *ctx, pdf_color_processor *p)
{
	rewritten_images *list = &p->images;
	int i;

	for (i = 0; i < list->len; i++)
	{
		pdf_drop_obj(ctx, list->res[i].im_obj);
		fz_drop_image(ctx, list->res[i].after);
	}
	fz_free(ctx, list->res);
	list->res = NULL;
	list->len = 0;
	list->max = 0;
}

static void
push_rewritten_shade(fz_context *ctx, pdf_color_processor *p, pdf_obj *before, fz_shade *after, char *name)
{
	rewritten_shades *list = &p->shades;

	if (list->max == list->len)
	{
		int new_max = list->max * 2;
		if (new_max == 0)
			new_max = 32;
		list->res = fz_realloc(ctx, list->res, sizeof(*list->res) * new_max);
		list->max = new_max;
	}
	list->res[list->len].before = pdf_keep_obj(ctx, before);
	list->res[list->len].after = fz_keep_shade(ctx, after);
	memcpy(list->res[list->len].name, name, MAX_REWRITTEN_NAME);
	list->len++;
}

static fz_shade *
find_rewritten_shade(fz_context *ctx, pdf_color_processor *p, pdf_obj *before, char *name)
{
	rewritten_shades *list = &p->shades;
	int i;

	for (i = 0; i < list->len; i++)
		if (list->res[i].before == before)
		{
			memcpy(name, list->res[i].name, MAX_REWRITTEN_NAME);
			return list->res[i].after;
		}

	return NULL;
}

static void
drop_rewritten_shades(fz_context *ctx, pdf_color_processor *p)
{
	rewritten_shades *list = &p->shades;
	int i;

	for (i = 0; i < list->len; i++)
	{
		pdf_drop_obj(ctx, list->res[i].before);
		fz_drop_shade(ctx, list->res[i].after);
	}
	fz_free(ctx, list->res);
	list->res = NULL;
	list->len = 0;
	list->max = 0;
}

static void
make_resource_instance(fz_context *ctx, pdf_color_processor *p, pdf_obj *key, const char *prefix, char *buf, int len, pdf_obj *target)
{
	int i;

	/* key gives us our category. Make sure we have such a category. */
	pdf_obj *res = pdf_dict_get(ctx, p->new_rstack->resources, key);
	if (!res)
		res = pdf_dict_put_dict(ctx, p->new_rstack->resources, key, 8);

	/* Now check through the category for each possible prefixed name
	 * in turn. */
	for (i = 1; i < 65536; ++i)
	{
		pdf_obj *obj;
		fz_snprintf(buf, len, "%s%d", prefix, i);

		obj = pdf_dict_gets(ctx, res, buf);
		if (!obj)
		{
			/* We've run out of names. At least that means we haven't
			 * previously added one ourselves. So add it now. */
			pdf_dict_puts(ctx, res, buf, target);
			return;
		}
		if (pdf_objcmp_resolve(ctx, obj, target) == 0)
		{
			/* We've found this one before! */
			return;
		}
	}
	fz_throw(ctx, FZ_ERROR_LIMIT, "Cannot create unique resource name");
}

static void
rewrite_cs(fz_context *ctx, pdf_color_processor *p, pdf_obj *cs_obj, int n, float *color, int stroking)
{
	char new_name[MAX_REWRITTEN_NAME];
	fz_colorspace *cs = NULL;
	pdf_pattern *pat = NULL;
	fz_shade *shade = NULL;
	int type;

	if (stroking)
		p->gstate->unmarked &= ~UNMARKED_STROKE;
	else
		p->gstate->unmarked &= ~UNMARKED_FILL;

	/* Otherwise, if it's a name, look it up as a colorspace. */
	if (pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceGray)) ||
		pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceCMYK)) ||
		pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceRGB)) ||
		pdf_name_eq(ctx, cs_obj, PDF_NAME(Pattern)))
	{
		/* These names should not be looked up. */
	}
	else if (pdf_is_name(ctx, cs_obj))
	{
		/* Any other names should be looked up in the resource dict,
		 * because our rewrite function doesn't have access to that. */
		cs_obj = pdf_lookup_resource(ctx, p->super.rstack, PDF_NAME(ColorSpace), pdf_to_name(ctx, cs_obj));
	}

	/* Until now, cs_obj has been a borrowed reference. Make it a real one. */
	pdf_keep_obj(ctx, cs_obj);

	/* Whatever happens, from here on in, we must drop cs_obj. */
	fz_var(cs);
	fz_var(pat);
	fz_var(shade);

	fz_try(ctx)
	{
		/* Our gstate always has to contain colorspaces BEFORE rewriting.
		 * Consider the case where we are given a separation space, and
		 * we rewrite it to be RGB. Then we change the 'amount' of that
		 * separation; we can't do that with the RGB value. */
		if (stroking)
		{
			pdf_drop_obj(ctx, p->gstate->cs_stroke);
			p->gstate->cs_stroke = pdf_keep_obj(ctx, cs_obj);
		}
		else
		{
			pdf_drop_obj(ctx, p->gstate->cs_fill);
			p->gstate->cs_fill = pdf_keep_obj(ctx, cs_obj);
		}
		/* Now, do any rewriting. This might drop the reference to cs_obj and
		 * return with a different one. */
		if (p->options->color_rewrite)
			p->options->color_rewrite(ctx, p->options->opaque, &cs_obj, &n, color);

		/* If we've rewritten it to be a simple name, great! */
		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceGray)))
		{
			if (stroking)
			{
				if (n == 1)
					p->super.chain->op_G(ctx, p->super.chain, color[0]);
				else
					p->super.chain->op_CS(ctx, p->super.chain, "DeviceGray", fz_device_gray(ctx));
			}
			else
			{
				if (n == 1)
					p->super.chain->op_g(ctx, p->super.chain, color[0]);
				else
					p->super.chain->op_cs(ctx, p->super.chain, "DeviceGray", fz_device_gray(ctx));
			}
			break;
		}

		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceRGB)))
		{
			if (stroking)
			{
				if (n == 3)
					p->super.chain->op_RG(ctx, p->super.chain, color[0], color[1], color[2]);
				else
					p->super.chain->op_CS(ctx, p->super.chain, "DeviceRGB", fz_device_rgb(ctx));
			}
			else
			{
				if (n == 3)
					p->super.chain->op_rg(ctx, p->super.chain, color[0], color[1], color[2]);
				else
					p->super.chain->op_cs(ctx, p->super.chain, "DeviceRGB", fz_device_rgb(ctx));
			}
			break;
		}

		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceCMYK)))
		{
			if (stroking)
			{
				if (n == 4)
					p->super.chain->op_K(ctx, p->super.chain, color[0], color[1], color[2], color[3]);
				else
					p->super.chain->op_CS(ctx, p->super.chain, "DeviceCMYK", fz_device_cmyk(ctx));
			}
			else
			{
				if (n == 4)
					p->super.chain->op_k(ctx, p->super.chain, color[0], color[1], color[2], color[3]);
				else
					p->super.chain->op_cs(ctx, p->super.chain, "DeviceCMYK", fz_device_cmyk(ctx));
			}
			break;
		}

		/* Accept both /Pattern and [ /Pattern ] */
		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(Pattern)) ||
			(pdf_array_len(ctx, cs_obj) == 1 && pdf_name_eq(ctx, pdf_array_get(ctx, cs_obj, 0), PDF_NAME(Pattern))))
		{
			assert(n == 0);
			if (stroking)
				p->super.chain->op_CS(ctx, p->super.chain, "Pattern", NULL);
			else
				p->super.chain->op_cs(ctx, p->super.chain, "Pattern", NULL);
			break;
		}

		/* Has it been rewritten to be an array? */
		if (pdf_is_array(ctx, cs_obj))
		{
			/* Make a new entry (or find an existing one), and send that. */
			make_resource_instance(ctx, p, PDF_NAME(ColorSpace), "CS", new_name, sizeof(new_name), cs_obj);

			cs = pdf_load_colorspace(ctx, cs_obj);
			if (stroking)
				p->super.chain->op_CS(ctx, p->super.chain, new_name, cs);
			else
				p->super.chain->op_cs(ctx, p->super.chain, new_name, cs);

			if (n > 0)
			{
				if (stroking)
					p->super.chain->op_SC_color(ctx, p->super.chain, n, color);
				else
					p->super.chain->op_sc_color(ctx, p->super.chain, n, color);
			}
			break;
		}

		/* Has it been rewritten to be a pattern? */
		type = pdf_dict_get_int(ctx, cs_obj, PDF_NAME(PatternType));
		if (type < 1 || type > 2)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Bad PatternType");

		/* Make a new entry (or find an existing one), and send that. */
		make_resource_instance(ctx, p, PDF_NAME(Pattern), "Pa", new_name, sizeof(new_name), cs_obj);

		if (type == 1)
		{
			pat = pdf_load_pattern(ctx, p->doc, cs_obj);
			if (stroking)
				p->super.chain->op_SC_pattern(ctx, p->super.chain, new_name, pat, n, color);
			else
				p->super.chain->op_sc_pattern(ctx, p->super.chain, new_name, pat, n, color);
			break;
		}
		else if (type == 2)
		{
			shade = pdf_load_shading(ctx, p->doc, cs_obj);
			if (stroking)
				p->super.chain->op_SC_shade(ctx, p->super.chain, new_name, shade);
			else
				p->super.chain->op_sc_shade(ctx, p->super.chain, new_name, shade);
			break;
		}

		fz_throw(ctx, FZ_ERROR_FORMAT, "Illegal rewritten colorspace");
	}
	fz_always(ctx)
	{
		fz_drop_shade(ctx, shade);
		fz_drop_colorspace(ctx, cs);
		pdf_drop_pattern(ctx, pat);
		pdf_drop_obj(ctx, cs_obj);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
mark_stroke(fz_context *ctx, pdf_color_processor *p)
{
	float zero[FZ_MAX_COLORS] = { 0 };

	rewrite_cs(ctx, p, PDF_NAME(DeviceGray), 1, zero, 1);

	p->gstate->unmarked &= ~UNMARKED_STROKE;
}

static void
mark_fill(fz_context *ctx, pdf_color_processor *p)
{
	float zero[FZ_MAX_COLORS] = { 0 };

	rewrite_cs(ctx, p, PDF_NAME(DeviceGray), 1, zero, 0);

	p->gstate->unmarked &= ~UNMARKED_FILL;
}

/* special graphics state */

static void
pdf_color_q(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	gstate_stack *gs = fz_malloc_struct(ctx, gstate_stack);

	gs->next = p->gstate;
	gs->cs_fill = pdf_keep_obj(ctx, p->gstate->cs_fill);
	gs->cs_stroke = pdf_keep_obj(ctx, p->gstate->cs_stroke);
	gs->unmarked = p->gstate->unmarked;
	gs->ctm = p->gstate->ctm;
	p->gstate = gs;

	if (p->super.chain->op_q)
		p->super.chain->op_q(ctx, p->super.chain);
}

static void
pdf_color_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	fz_matrix m;

	m.a = a;
	m.b = b;
	m.c = c;
	m.d = d;
	m.e = e;
	m.f = f;

	p->gstate->ctm = fz_concat(m, p->gstate->ctm);

	if (p->super.chain->op_cm)
		p->super.chain->op_cm(ctx, p->super.chain, a, b, c, d, e, f);
}

/* path construction */

/* path painting */

static void
pdf_color_S(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);

	if (p->super.chain->op_S)
		p->super.chain->op_S(ctx, p->super.chain);
}

static void
pdf_color_s(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_s)
		p->super.chain->op_s(ctx, p->super.chain);
}

static void
pdf_color_F(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_F)
		p->super.chain->op_F(ctx, p->super.chain);
}

static void
pdf_color_f(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_f)
		p->super.chain->op_f(ctx, p->super.chain);
}

static void
pdf_color_fstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_fstar)
		p->super.chain->op_fstar(ctx, p->super.chain);
}

static void
pdf_color_B(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_B)
		p->super.chain->op_B(ctx, p->super.chain);
}

static void
pdf_color_Bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_Bstar)
		p->super.chain->op_Bstar(ctx, p->super.chain);
}

static void
pdf_color_b(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_b)
		p->super.chain->op_b(ctx, p->super.chain);
}

static void
pdf_color_bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_bstar)
		p->super.chain->op_bstar(ctx, p->super.chain);
}

static void
pdf_color_Q(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	gstate_stack *gs = p->gstate;

	p->gstate = gs->next;
	pdf_drop_obj(ctx, gs->cs_fill);
	pdf_drop_obj(ctx, gs->cs_stroke);

	fz_try(ctx)
		if (p->super.chain->op_Q)
			p->super.chain->op_Q(ctx, p->super.chain);
	fz_always(ctx)
		fz_free(ctx, gs);
	fz_catch(ctx)
		fz_rethrow(ctx);

	if (!p->gstate)
{
		p->gstate = fz_malloc_struct(ctx, gstate_stack);
		p->gstate->ctm = fz_identity;
}
}

/* text showing */

static void
pdf_color_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	/* FIXME: We could optimise this if we knew the Tr, maybe. */
	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_TJ)
		p->super.chain->op_TJ(ctx, p->super.chain, array);
}

static void
pdf_color_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	/* FIXME: We could optimise this if we knew the Tr, maybe. */
	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_Tj)
		p->super.chain->op_Tj(ctx, p->super.chain, str, len);
}

static void
pdf_color_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	/* FIXME: We could optimise this if we knew the Tr, maybe. */
	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_squote)
		p->super.chain->op_squote(ctx, p->super.chain, str, len);
}

static void
pdf_color_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	/* FIXME: We could optimise this if we knew the Tr, maybe. */
	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->super.chain->op_dquote)
		p->super.chain->op_dquote(ctx, p->super.chain, aw, ac, str, len);
}

/* color */

static void
pdf_color_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_color_processor *p = (pdf_color_processor *)proc;
	pdf_obj *cs_obj = pdf_new_name(ctx, name);
	float color[FZ_MAX_COLORS] = { 1 };

	fz_try(ctx)
		rewrite_cs(ctx, p, cs_obj, 0, color, 1);
	fz_always(ctx)
		pdf_drop_obj(ctx, cs_obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_color_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	pdf_obj *cs_obj = pdf_new_name(ctx, name);
	float color[FZ_MAX_COLORS] = { 1 };

	fz_try(ctx)
		rewrite_cs(ctx, p, cs_obj, 0, color, 0);
	fz_always(ctx)
		pdf_drop_obj(ctx, cs_obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_color_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { 0 };
	pdf_obj *cs_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Pattern), name);

	memcpy(local_color, color, sizeof(float) * n);
	rewrite_cs(ctx, p, cs_obj, n, local_color, 1);
}

static void
pdf_color_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { 0 };
	pdf_obj *cs_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Pattern), name);

	memcpy(local_color, color, sizeof(float) * n);
	rewrite_cs(ctx, p, cs_obj, n, local_color, 0);
}

static void
pdf_color_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	pdf_obj *orig;
	pdf_obj *dict = NULL;
	pdf_obj *dict2 = NULL;
	char new_name[MAX_REWRITTEN_NAME];
	pdf_obj *rewritten;
	fz_shade *new_shade = NULL;

	if (p->options->shade_rewrite == NULL)
	{
		/* Must copy shading over to new resources dict. */
		pdf_obj *old_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Shading), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->new_rstack->resources, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->new_rstack->resources, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->super.chain->op_SC_shade)
			p->super.chain->op_SC_shade(ctx, p->super.chain, name, shade);
		return;
	}

	orig = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Pattern), name);
	orig = pdf_dict_get(ctx, orig, PDF_NAME(Shading));

	new_shade = find_rewritten_shade(ctx, p, orig, new_name);
	if (new_shade)
	{
		/* Must copy shading over to new resources dict. */
		pdf_obj *old_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Shading), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->new_rstack->resources, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->new_rstack->resources, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->super.chain->op_SC_shade)
			p->super.chain->op_SC_shade(ctx, p->super.chain, new_name, new_shade);
		return;
	}

	rewritten = pdf_recolor_shade(ctx, orig, p->options->shade_rewrite, p->options->opaque);

	fz_var(new_shade);
	fz_var(dict);
	fz_var(dict2);

	fz_try(ctx)
	{
		dict = pdf_new_dict(ctx, p->doc, 1);
		pdf_dict_put_int(ctx, dict, PDF_NAME(PatternType), 2);
		pdf_dict_put(ctx, dict, PDF_NAME(Shading), rewritten);
		dict2 = pdf_add_object(ctx, p->doc, dict);
		make_resource_instance(ctx, p, PDF_NAME(Pattern), "Pa", new_name, sizeof(new_name), dict2);

		new_shade = pdf_load_shading(ctx, p->doc, rewritten);

		/* Remember that we've done this one before. */
		push_rewritten_shade(ctx, p, orig, new_shade, new_name);

		if (p->super.chain->op_sh)
			p->super.chain->op_SC_shade(ctx, p->super.chain, new_name, new_shade);
	}
	fz_always(ctx)
	{
		fz_drop_shade(ctx, new_shade);
		pdf_drop_obj(ctx, rewritten);
		pdf_drop_obj(ctx, dict);
		pdf_drop_obj(ctx, dict2);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_color_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	pdf_obj *orig;
	pdf_obj *dict = NULL;
	pdf_obj *dict2 = NULL;
	char new_name[MAX_REWRITTEN_NAME];
	pdf_obj *rewritten;
	fz_shade *new_shade = NULL;

	if (p->options->shade_rewrite == NULL)
	{
		/* Must copy shading over to new resources dict. */
		pdf_obj *old_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Shading), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->new_rstack->resources, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->new_rstack->resources, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->super.chain->op_sc_shade)
			p->super.chain->op_sc_shade(ctx, p->super.chain, name, shade);
		return;
	}

	orig = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Pattern), name);
	orig = pdf_dict_get(ctx, orig, PDF_NAME(Shading));

	new_shade = find_rewritten_shade(ctx, p, orig, new_name);
	if (new_shade)
	{
		if (p->super.chain->op_sc_shade)
			p->super.chain->op_sc_shade(ctx, p->super.chain, new_name, new_shade);
		return;
	}

	rewritten = pdf_recolor_shade(ctx, orig, p->options->shade_rewrite, p->options->opaque);

	fz_var(new_shade);
	fz_var(dict);
	fz_var(dict2);

	fz_try(ctx)
	{
		dict = pdf_new_dict(ctx, p->doc, 1);
		pdf_dict_put_int(ctx, dict, PDF_NAME(PatternType), 2);
		pdf_dict_put(ctx, dict, PDF_NAME(Shading), rewritten);
		dict2 = pdf_add_object(ctx, p->doc, dict);
		make_resource_instance(ctx, p, PDF_NAME(Pattern), "Pa", new_name, sizeof(new_name), dict2);

		new_shade = pdf_load_shading(ctx, p->doc, rewritten);

		/* Remember that we've done this one before. */
		push_rewritten_shade(ctx, p, orig, new_shade, new_name);

		if (p->super.chain->op_sh)
			p->super.chain->op_sc_shade(ctx, p->super.chain, new_name, new_shade);
	}
	fz_always(ctx)
	{
		fz_drop_shade(ctx, new_shade);
		pdf_drop_obj(ctx, rewritten);
		pdf_drop_obj(ctx, dict);
		pdf_drop_obj(ctx, dict2);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_color_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { 0 };
	pdf_obj *cs_obj = p->gstate->cs_stroke;

	memcpy(local_color, color, sizeof(float) * n);
	rewrite_cs(ctx, p, cs_obj, n, local_color, 1);
}

static void
pdf_color_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { 0 };
	pdf_obj *cs_obj = p->gstate->cs_fill;

	memcpy(local_color, color, sizeof(float) * n);
	rewrite_cs(ctx, p, cs_obj, n, local_color, 0);
}

static void
pdf_color_G(fz_context *ctx, pdf_processor *proc, float g)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { g };

	rewrite_cs(ctx, p, PDF_NAME(DeviceGray), 1, local_color, 1);
}

static void
pdf_color_g(fz_context *ctx, pdf_processor *proc, float g)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { g };

	rewrite_cs(ctx, p, PDF_NAME(DeviceGray), 1, local_color, 0);
}

static void
pdf_color_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { r, g, b };

	rewrite_cs(ctx, p, PDF_NAME(DeviceRGB), 3, local_color, 1);
}

static void
pdf_color_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { r, g, b };

	rewrite_cs(ctx, p, PDF_NAME(DeviceRGB), 3, local_color, 0);
}

static void
pdf_color_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { c, m, y, k };

	rewrite_cs(ctx, p, PDF_NAME(DeviceCMYK), 4, local_color, 1);
}

static void
pdf_color_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	float local_color[FZ_MAX_COLORS] = { c, m, y, k };

	rewrite_cs(ctx, p, PDF_NAME(DeviceCMYK), 4, local_color, 0);
}

/* shadings, images, xobjects */

static void
pdf_color_BI(fz_context *ctx, pdf_processor *proc, fz_image *image, const char *colorspace)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (image->imagemask)
	{
		/* Imagemasks require the color to have been set. */
		if (p->gstate->unmarked & UNMARKED_FILL)
			mark_fill(ctx, p);
	}

	fz_keep_image(ctx, image);
	if (p->options->image_rewrite)
		p->options->image_rewrite(ctx, p->options->opaque, &image, p->gstate->ctm, NULL);

	if (p->super.chain->op_BI)
		p->super.chain->op_BI(ctx, p->super.chain, image, colorspace);
	fz_drop_image(ctx, image);
}

static void
pdf_color_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	pdf_obj *orig;
	char new_name[MAX_REWRITTEN_NAME];
	pdf_obj *rewritten;
	fz_shade *new_shade = NULL;

	if (p->options->shade_rewrite == NULL)
	{
		/* Must copy shading over to new resources dict. */
		pdf_obj *old_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Shading), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->new_rstack->resources, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->new_rstack->resources, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->super.chain->op_sh)
			p->super.chain->op_sh(ctx, p->super.chain, name, shade);
		return;
	}

	orig = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Shading), name);

	new_shade = find_rewritten_shade(ctx, p, orig, new_name);
	if (new_shade)
	{
		if (p->super.chain->op_sh)
			p->super.chain->op_sh(ctx, p->super.chain, new_name, new_shade);
		return;
	}

	rewritten = pdf_recolor_shade(ctx, orig, p->options->shade_rewrite, p->options->opaque);

	fz_var(new_shade);

	fz_try(ctx)
	{
		make_resource_instance(ctx, p, PDF_NAME(Shading), "Sh", new_name, sizeof(new_name), rewritten);

		new_shade = pdf_load_shading(ctx, p->doc, rewritten);

		/* Remember that we've done this one before. */
		push_rewritten_shade(ctx, p, orig, new_shade, new_name);

		if (p->super.chain->op_sh)
			p->super.chain->op_sh(ctx, p->super.chain, new_name, new_shade);
	}
	fz_always(ctx)
	{
		fz_drop_shade(ctx, new_shade);
		pdf_drop_obj(ctx, rewritten);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_color_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	fz_image *orig = image;
	char new_name[MAX_REWRITTEN_NAME];
	pdf_obj *im_obj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(XObject), name);

	/* Have we done this one before? */
	if (!p->options->repeated_image_rewrite)
	{
		image = find_rewritten_image(ctx, p, im_obj, new_name);
		if (image)
		{
			if (p->super.chain->op_Do_image)
				p->super.chain->op_Do_image(ctx, p->super.chain, new_name, image);
			return;
		}
	}

	image = orig;
	fz_keep_image(ctx, image);
	pdf_keep_obj(ctx, im_obj);

	fz_var(im_obj);
	fz_try(ctx)
	{

		if (image->imagemask)
		{
			/* Imagemasks require the color to have been set. */
			if (p->gstate->unmarked & UNMARKED_FILL)
				mark_fill(ctx, p);
		}
		else
		{
			if (p->options->image_rewrite)
				p->options->image_rewrite(ctx, p->options->opaque, &image, p->gstate->ctm, im_obj);
		}

		/* If it's been rewritten add the new one, otherwise copy the old one across. */
		if (image != orig)
		{
			pdf_drop_obj(ctx, im_obj);
			im_obj = NULL;
			im_obj = pdf_add_image(ctx, p->doc, image);
		}

		make_resource_instance(ctx, p, PDF_NAME(XObject), "Im", new_name, sizeof(new_name), im_obj);

		if (!p->options->repeated_image_rewrite)
		{
			/* Remember that we've done this one before. */
			push_rewritten_image(ctx, p, im_obj, image, new_name);
		}

		if (p->super.chain->op_Do_image)
			p->super.chain->op_Do_image(ctx, p->super.chain, new_name, image);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, im_obj);
		fz_drop_image(ctx, image);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_color_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	char new_name[MAX_REWRITTEN_NAME];
	pdf_obj *xres;
	pdf_obj *new_xres = NULL;

	fz_var(new_xres);

	/* FIXME: Ideally we'd look at p->global_options->instance_forms here
	 * to avoid flattening all the XObjects. This is non-trivial, and
	 * currently incomplete, hence disabled. We have to find some way to
	 * tell the underlying filters (in particular the output filter) that
	 * we are in a new XObject (so the output filter can start a new buffer).
	 * Maybe it could look at p->global_options->instance_forms too on a
	 * push/pop of the resources? For now, let's just leave it disabled. */
	if (0 && p->global_options->instance_forms)
	{
		make_resource_instance(ctx, p, PDF_NAME(XObject), "Xo", new_name, sizeof(new_name), xobj);

		xres = pdf_xobject_resources(ctx, xobj);
		fz_try(ctx)
			pdf_process_contents(ctx, (pdf_processor *)p, p->doc, xres, xobj, NULL, &new_xres);
		fz_catch(ctx)
		{
			pdf_drop_obj(ctx, new_xres);
			fz_rethrow(ctx);
		}

		pdf_dict_put_drop(ctx, xobj, PDF_NAME(Resources), new_xres);

		if (p->super.chain->op_Do_form)
			p->super.chain->op_Do_form(ctx, p->super.chain, new_name, xobj);
	}
	else
	{
		/* In this case, we just copy the XObject across (renaming it). Our caller will arrange to
		 * filter it. */
		make_resource_instance(ctx, p, PDF_NAME(XObject), "Xo", new_name, sizeof(new_name), xobj);
		if (p->super.chain->op_Do_form)
			p->super.chain->op_Do_form(ctx, p->super.chain, new_name, xobj);
}
}

static void
pdf_close_color_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	pdf_close_processor(ctx, p->super.chain);
}

static void
pdf_drop_color_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	gstate_stack *gs = p->gstate;

	while (gs)
	{
		gstate_stack *gs_next = gs->next;

		pdf_drop_obj(ctx, gs->cs_fill);
		pdf_drop_obj(ctx, gs->cs_stroke);
		fz_free(ctx, gs);
		gs = gs_next;
	}

	while (p->new_rstack)
	{
		pdf_resource_stack *stk = p->new_rstack;
		p->new_rstack = stk->next;
		pdf_drop_obj(ctx, stk->resources);
		fz_free(ctx, stk);
	}

	drop_rewritten_images(ctx, p);
	drop_rewritten_shades(ctx, p);

	pdf_drop_document(ctx, p->doc);
}

static void
pdf_color_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	pdf_resource_stack *stk = fz_malloc_struct(ctx, pdf_resource_stack);
	pdf_obj *obj;

	p->gstate->unmarked = UNMARKED_STROKE | UNMARKED_FILL;

	stk->next = p->new_rstack;
	p->new_rstack = stk;
	fz_try(ctx)
	{
		/* At the moment we know that we'll always be flattening XObjects.
		 * So only the top level 'push' makes a new resource dict. Any
		 * subsequent one will share the previous levels one. */
		if (stk->next)
			stk->resources = pdf_keep_obj(ctx, stk->next->resources);
		else
			stk->resources = pdf_new_dict(ctx, p->doc, 1);

		obj = pdf_dict_get(ctx, res, PDF_NAME(Properties));
		if (obj)
			pdf_dict_put(ctx, stk->resources, PDF_NAME(Properties), obj);
		obj = pdf_dict_get(ctx, res, PDF_NAME(ExtGState));
		if (obj)
			pdf_dict_put(ctx, stk->resources, PDF_NAME(ExtGState), obj);
		obj = pdf_dict_get(ctx, res, PDF_NAME(Font));
		if (obj)
			pdf_dict_put(ctx, stk->resources, PDF_NAME(Font), obj);

		pdf_processor_push_resources(ctx, p->super.chain, stk->resources);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, stk->resources);
		p->new_rstack = stk->next;
		fz_free(ctx, stk);
		fz_rethrow(ctx);
	}
}

static pdf_obj *
pdf_color_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	pdf_resource_stack *stk = p->new_rstack;

	p->new_rstack = stk->next;
	pdf_drop_obj(ctx, stk->resources);
	fz_free(ctx, stk);

	return pdf_processor_pop_resources(ctx, p->super.chain);
}

pdf_processor *
pdf_new_color_filter(
	fz_context *ctx,
	pdf_document *doc,
	pdf_processor *chain,
	int struct_parents,
	fz_matrix transform,
	pdf_filter_options *global_options,
	void *options_)
{
	pdf_color_processor *proc = pdf_new_processor(ctx, sizeof * proc);
	pdf_color_filter_options *options = (pdf_color_filter_options *)options_;

	proc->super.close_processor = pdf_close_color_processor;
	proc->super.drop_processor = pdf_drop_color_processor;

	proc->super.push_resources = pdf_color_push_resources;
	proc->super.pop_resources = pdf_color_pop_resources;

	/* special graphics state */
	proc->super.op_q = pdf_color_q;
	proc->super.op_Q = pdf_color_Q;
	proc->super.op_cm = pdf_color_cm;

	/* path painting */
	proc->super.op_S = pdf_color_S;
	proc->super.op_s = pdf_color_s;
	proc->super.op_F = pdf_color_F;
	proc->super.op_f = pdf_color_f;
	proc->super.op_fstar = pdf_color_fstar;
	proc->super.op_B = pdf_color_B;
	proc->super.op_Bstar = pdf_color_Bstar;
	proc->super.op_b = pdf_color_b;
	proc->super.op_bstar = pdf_color_bstar;

	/* text showing */
	proc->super.op_TJ = pdf_color_TJ;
	proc->super.op_Tj = pdf_color_Tj;
	proc->super.op_squote = pdf_color_squote;
	proc->super.op_dquote = pdf_color_dquote;

	/* color */
	proc->super.op_CS = pdf_color_CS;
	proc->super.op_cs = pdf_color_cs;
	proc->super.op_SC_color = pdf_color_SC_color;
	proc->super.op_sc_color = pdf_color_sc_color;
	proc->super.op_SC_pattern = pdf_color_SC_pattern;
	proc->super.op_sc_pattern = pdf_color_sc_pattern;
	proc->super.op_SC_shade = pdf_color_SC_shade;
	proc->super.op_sc_shade = pdf_color_sc_shade;

	proc->super.op_G = pdf_color_G;
	proc->super.op_g = pdf_color_g;
	proc->super.op_RG = pdf_color_RG;
	proc->super.op_rg = pdf_color_rg;
	proc->super.op_K = pdf_color_K;
	proc->super.op_k = pdf_color_k;

	/* shadings, images, xobjects */
	proc->super.op_BI = pdf_color_BI;
	proc->super.op_sh = pdf_color_sh;
	proc->super.op_Do_image = pdf_color_Do_image;
	proc->super.op_Do_form = pdf_color_Do_form;

	fz_try(ctx)
		proc->gstate = fz_malloc_struct(ctx, gstate_stack);
	fz_catch(ctx)
	{
		fz_free(ctx, proc);
		fz_rethrow(ctx);
	}
	proc->gstate->ctm = fz_identity;

	proc->doc = pdf_keep_document(ctx, doc);
	proc->super.chain = chain;
	proc->global_options = global_options;
	proc->options = options;

	proc->super.requirements = PDF_PROCESSOR_REQUIRES_DECODED_IMAGES | proc->super.chain->requirements;

	return (pdf_processor*)proc;
}
