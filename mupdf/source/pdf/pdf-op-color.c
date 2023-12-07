// Copyright (C) 2004-2022 Artifex Software, Inc.
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

typedef struct resources_stack
{
	struct resources_stack *next;
	pdf_obj *old_rdb;
	pdf_obj *new_rdb;
} resources_stack;

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
	pdf_processor *chain;
	pdf_filter_options *global_options;
	pdf_color_filter_options *options;
	resources_stack *rstack;
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
	pdf_obj *res = pdf_dict_get(ctx, p->rstack->new_rdb, key);
	if (!res)
		res = pdf_dict_put_dict(ctx, p->rstack->new_rdb, key, 8);

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
		cs_obj = pdf_dict_get(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(ColorSpace)), cs_obj);
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
					p->chain->op_G(ctx, p->chain, color[0]);
				else
					p->chain->op_CS(ctx, p->chain, "DeviceGray", fz_device_gray(ctx));
			}
			else
			{
				if (n == 1)
					p->chain->op_g(ctx, p->chain, color[0]);
				else
					p->chain->op_cs(ctx, p->chain, "DeviceGray", fz_device_gray(ctx));
			}
			break;
		}

		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceRGB)))
		{
			if (stroking)
			{
				if (n == 3)
					p->chain->op_RG(ctx, p->chain, color[0], color[1], color[2]);
				else
					p->chain->op_CS(ctx, p->chain, "DeviceRGB", fz_device_rgb(ctx));
			}
			else
			{
				if (n == 3)
					p->chain->op_rg(ctx, p->chain, color[0], color[1], color[2]);
				else
					p->chain->op_cs(ctx, p->chain, "DeviceRGB", fz_device_rgb(ctx));
			}
			break;
		}

		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(DeviceCMYK)))
		{
			if (stroking)
			{
				if (n == 4)
					p->chain->op_K(ctx, p->chain, color[0], color[1], color[2], color[3]);
				else
					p->chain->op_CS(ctx, p->chain, "DeviceCMYK", fz_device_cmyk(ctx));
			}
			else
			{
				if (n == 4)
					p->chain->op_k(ctx, p->chain, color[0], color[1], color[2], color[3]);
				else
					p->chain->op_cs(ctx, p->chain, "DeviceCMYK", fz_device_cmyk(ctx));
			}
			break;
		}

		/* Accept both /Pattern and [ /Pattern ] */
		if (pdf_name_eq(ctx, cs_obj, PDF_NAME(Pattern)) ||
			(pdf_array_len(ctx, cs_obj) == 1 && pdf_name_eq(ctx, pdf_array_get(ctx, cs_obj, 0), PDF_NAME(Pattern))))
		{
			assert(n == 0);
			if (stroking)
				p->chain->op_CS(ctx, p->chain, "Pattern", NULL);
			else
				p->chain->op_cs(ctx, p->chain, "Pattern", NULL);
			break;
		}

		/* Has it been rewritten to be an array? */
		if (pdf_is_array(ctx, cs_obj))
		{
			/* Make a new entry (or find an existing one), and send that. */
			make_resource_instance(ctx, p, PDF_NAME(ColorSpace), "CS", new_name, sizeof(new_name), cs_obj);

			cs = pdf_load_colorspace(ctx, cs_obj);
			if (stroking)
				p->chain->op_CS(ctx, p->chain, new_name, cs);
			else
				p->chain->op_cs(ctx, p->chain, new_name, cs);

			if (n > 0)
			{
				if (stroking)
					p->chain->op_SC_color(ctx, p->chain, n, color);
				else
					p->chain->op_sc_color(ctx, p->chain, n, color);
			}
			break;
		}

		/* Has it been rewritten to be a pattern? */
		type = pdf_dict_get_int(ctx, cs_obj, PDF_NAME(PatternType));
		if (type < 1 || type > 2)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Bad PatternType");

		/* Make a new entry (or find an existing one), and send that. */
		make_resource_instance(ctx, p, PDF_NAME(Pattern), "Pa", new_name, sizeof(new_name), cs_obj);

		if (type == 1)
		{
			pat = pdf_load_pattern(ctx, p->doc, cs_obj);
			if (stroking)
				p->chain->op_SC_pattern(ctx, p->chain, new_name, pat, n, color);
			else
				p->chain->op_sc_pattern(ctx, p->chain, new_name, pat, n, color);
			break;
		}
		else if (type == 2)
		{
			shade = pdf_load_shading(ctx, p->doc, cs_obj);
			if (stroking)
				p->chain->op_SC_shade(ctx, p->chain, new_name, shade);
			else
				p->chain->op_sc_shade(ctx, p->chain, new_name, shade);
			break;
		}

		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal rewritten colorspace");
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

/* general graphics state */

static void
pdf_color_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_w)
		p->chain->op_w(ctx, p->chain, linewidth);
}

static void
pdf_color_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_j)
		p->chain->op_j(ctx, p->chain, linejoin);
}

static void
pdf_color_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_J)
		p->chain->op_J(ctx, p->chain, linecap);
}

static void
pdf_color_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_M)
		p->chain->op_M(ctx, p->chain, miterlimit);
}

static void
pdf_color_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_d)
		p->chain->op_d(ctx, p->chain, array, phase);
}

static void
pdf_color_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_ri)
		p->chain->op_ri(ctx, p->chain, intent);
}

static void
pdf_color_gs_OP(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_OP)
		p->chain->op_gs_OP(ctx, p->chain, b);
}

static void
pdf_color_gs_op(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_op)
		p->chain->op_gs_op(ctx, p->chain, b);
}

static void
pdf_color_gs_OPM(fz_context *ctx, pdf_processor *proc, int i)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_OPM)
		p->chain->op_gs_OPM(ctx, p->chain, i);
}

static void
pdf_color_gs_UseBlackPtComp(fz_context *ctx, pdf_processor *proc, pdf_obj *name)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_UseBlackPtComp)
		p->chain->op_gs_UseBlackPtComp(ctx, p->chain, name);
}

static void
pdf_color_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_i)
		p->chain->op_i(ctx, p->chain, flatness);
}

static void
pdf_color_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_begin)
		p->chain->op_gs_begin(ctx, p->chain, name, extgstate);
}

static void
pdf_color_gs_BM(fz_context *ctx, pdf_processor *proc, const char *blendmode)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_BM)
		p->chain->op_gs_BM(ctx, p->chain, blendmode);
}

static void
pdf_color_gs_CA(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_CA)
		p->chain->op_gs_CA(ctx, p->chain, alpha);
}

static void
pdf_color_gs_ca(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_ca)
		p->chain->op_gs_ca(ctx, p->chain, alpha);
}

static void
pdf_color_gs_SMask(fz_context *ctx, pdf_processor *proc, pdf_obj *smask, float *bc, int luminosity)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_SMask)
		p->chain->op_gs_SMask(ctx, p->chain, smask, bc, luminosity);
}

static void
pdf_color_gs_end(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_gs_end)
		p->chain->op_gs_end(ctx, p->chain);
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

	if (p->chain->op_q)
		p->chain->op_q(ctx, p->chain);
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

	if (p->chain->op_cm)
		p->chain->op_cm(ctx, p->chain, a, b, c, d, e, f);
}

/* path construction */

static void
pdf_color_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_m)
		p->chain->op_m(ctx, p->chain, x, y);
}

static void
pdf_color_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_l)
		p->chain->op_l(ctx, p->chain, x, y);
}

static void
pdf_color_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_c)
		p->chain->op_c(ctx, p->chain, x1, y1, x2, y2, x3, y3);
}

static void
pdf_color_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_v)
		p->chain->op_v(ctx, p->chain, x2, y2, x3, y3);
}

static void
pdf_color_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_y)
		p->chain->op_y(ctx, p->chain, x1, y1, x3, y3);
}

static void
pdf_color_h(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_h)
		p->chain->op_h(ctx, p->chain);
}

static void
pdf_color_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_re)
		p->chain->op_re(ctx, p->chain, x, y, w, h);
}

/* path painting */

static void
pdf_color_S(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);

	if (p->chain->op_S)
		p->chain->op_S(ctx, p->chain);
}

static void
pdf_color_s(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_s)
		p->chain->op_s(ctx, p->chain);
}

static void
pdf_color_F(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_F)
		p->chain->op_F(ctx, p->chain);
}

static void
pdf_color_f(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_f)
		p->chain->op_f(ctx, p->chain);
}

static void
pdf_color_fstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_fstar)
		p->chain->op_fstar(ctx, p->chain);
}

static void
pdf_color_B(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_B)
		p->chain->op_B(ctx, p->chain);
}

static void
pdf_color_Bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_Bstar)
		p->chain->op_Bstar(ctx, p->chain);
}

static void
pdf_color_b(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_b)
		p->chain->op_b(ctx, p->chain);
}

static void
pdf_color_bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->gstate->unmarked & UNMARKED_STROKE)
		mark_stroke(ctx, p);
	if (p->gstate->unmarked & UNMARKED_FILL)
		mark_fill(ctx, p);

	if (p->chain->op_bstar)
		p->chain->op_bstar(ctx, p->chain);
}

static void
pdf_color_n(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_n)
		p->chain->op_n(ctx, p->chain);
}

/* clipping paths */

static void
pdf_color_W(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_W)
		p->chain->op_W(ctx, p->chain);
}

static void
pdf_color_Wstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Wstar)
		p->chain->op_Wstar(ctx, p->chain);
}

/* text objects */

static void
pdf_color_BT(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_BT)
		p->chain->op_BT(ctx, p->chain);
}

static void
pdf_color_ET(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_ET)
		p->chain->op_ET(ctx, p->chain);
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
		if (p->chain->op_Q)
			p->chain->op_Q(ctx, p->chain);
	fz_always(ctx)
		fz_free(ctx, gs);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* text state */

static void
pdf_color_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tc)
		p->chain->op_Tc(ctx, p->chain, charspace);
}

static void
pdf_color_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tw)
		p->chain->op_Tw(ctx, p->chain, wordspace);
}

static void
pdf_color_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tz)
		p->chain->op_Tz(ctx, p->chain, scale);
}

static void
pdf_color_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_TL)
		p->chain->op_TL(ctx, p->chain, leading);
}

static void
pdf_color_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tf)
		p->chain->op_Tf(ctx, p->chain, name, font, size);
}

static void
pdf_color_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tr)
		p->chain->op_Tr(ctx, p->chain, render);
}

static void
pdf_color_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Ts)
		p->chain->op_Ts(ctx, p->chain, rise);
}

/* text positioning */

static void
pdf_color_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Td)
		p->chain->op_Td(ctx, p->chain, tx, ty);
}

static void
pdf_color_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_TD)
		p->chain->op_TD(ctx, p->chain, tx, ty);
}

static void
pdf_color_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tm)
		p->chain->op_Tm(ctx, p->chain, a, b, c, d, e, f);
}

static void
pdf_color_Tstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_Tstar)
		p->chain->op_Tstar(ctx, p->chain);
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

	if (p->chain->op_TJ)
		p->chain->op_TJ(ctx, p->chain, array);
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

	if (p->chain->op_Tj)
		p->chain->op_Tj(ctx, p->chain, str, len);
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

	if (p->chain->op_squote)
		p->chain->op_squote(ctx, p->chain, str, len);
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

	if (p->chain->op_dquote)
		p->chain->op_dquote(ctx, p->chain, aw, ac, str, len);
}

/* type 3 fonts */

static void
pdf_color_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_d0)
		p->chain->op_d0(ctx, p->chain, wx, wy);
}

static void
pdf_color_d1(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_d1)
		p->chain->op_d1(ctx, p->chain, wx, wy, llx, lly, urx, ury);
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
	pdf_obj *cs_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Pattern)), name);

	memcpy(local_color, color, sizeof(float) * n);
	rewrite_cs(ctx, p, cs_obj, n, local_color, 1);
}

static void
pdf_color_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	float local_color[FZ_MAX_COLORS] = { 0 };
	pdf_obj *cs_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Pattern)), name);

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
		pdf_obj *old_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Shading)), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->rstack->new_rdb, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->rstack->new_rdb, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->chain->op_SC_shade)
			p->chain->op_SC_shade(ctx, p->chain, name, shade);
		return;
	}

	orig = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Pattern)), name);
	orig = pdf_dict_get(ctx, orig, PDF_NAME(Shading));

	new_shade = find_rewritten_shade(ctx, p, orig, new_name);
	if (new_shade)
	{
		/* Must copy shading over to new resources dict. */
		pdf_obj *old_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Shading)), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->rstack->new_rdb, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->rstack->new_rdb, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->chain->op_SC_shade)
			p->chain->op_SC_shade(ctx, p->chain, new_name, new_shade);
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

		if (p->chain->op_sh)
			p->chain->op_SC_shade(ctx, p->chain, new_name, new_shade);
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
		pdf_obj *old_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Shading)), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->rstack->new_rdb, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->rstack->new_rdb, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->chain->op_sc_shade)
			p->chain->op_sc_shade(ctx, p->chain, name, shade);
		return;
	}

	orig = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Pattern)), name);
	orig = pdf_dict_get(ctx, orig, PDF_NAME(Shading));

	new_shade = find_rewritten_shade(ctx, p, orig, new_name);
	if (new_shade)
	{
		if (p->chain->op_sc_shade)
			p->chain->op_sc_shade(ctx, p->chain, new_name, new_shade);
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

		if (p->chain->op_sh)
			p->chain->op_sc_shade(ctx, p->chain, new_name, new_shade);
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

	if (p->chain->op_BI)
		p->chain->op_BI(ctx, p->chain, image, colorspace);
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
		pdf_obj *old_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Shading)), name);
		pdf_obj *new_shading_dict = pdf_dict_get(ctx, p->rstack->new_rdb, PDF_NAME(Shading));
		if (new_shading_dict == NULL)
			pdf_dict_put_drop(ctx, p->rstack->new_rdb, PDF_NAME(Shading), new_shading_dict = pdf_new_dict(ctx, p->doc, 4));
		pdf_dict_puts(ctx, new_shading_dict, name, old_obj);

		if (p->chain->op_sh)
			p->chain->op_sh(ctx, p->chain, name, shade);
		return;
	}

	orig = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(Shading)), name);

	new_shade = find_rewritten_shade(ctx, p, orig, new_name);
	if (new_shade)
	{
		if (p->chain->op_sh)
			p->chain->op_sh(ctx, p->chain, new_name, new_shade);
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

		if (p->chain->op_sh)
			p->chain->op_sh(ctx, p->chain, new_name, new_shade);
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
	pdf_obj *im_obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(XObject)), name);

	/* Have we done this one before? */
	if (!p->options->repeated_image_rewrite)
	{
		image = find_rewritten_image(ctx, p, im_obj, new_name);
		if (image)
		{
			if (p->chain->op_Do_image)
				p->chain->op_Do_image(ctx, p->chain, new_name, image);
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

		if (p->chain->op_Do_image)
			p->chain->op_Do_image(ctx, p->chain, new_name, image);
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

		if (p->chain->op_Do_form)
			p->chain->op_Do_form(ctx, p->chain, new_name, xobj);
	}
	else
	{
		/* In this case, we just copy the XObject across (renaming it). Our caller will arrange to
		 * filter it. */
		make_resource_instance(ctx, p, PDF_NAME(XObject), "Xo", new_name, sizeof(new_name), xobj);
		if (p->chain->op_Do_form)
			p->chain->op_Do_form(ctx, p->chain, new_name, xobj);
	}
}

/* marked content */

static void
pdf_color_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_MP)
		p->chain->op_MP(ctx, p->chain, tag);
}

static void
pdf_color_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_DP)
		p->chain->op_DP(ctx, p->chain, tag, raw, cooked);
}

static void
pdf_color_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_BMC)
		p->chain->op_BMC(ctx, p->chain, tag);
}

static void
pdf_color_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_BDC)
		p->chain->op_BDC(ctx, p->chain, tag, raw, cooked);
}

static void
pdf_color_EMC(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_EMC)
		p->chain->op_EMC(ctx, p->chain);
}

/* compatibility */

static void
pdf_color_BX(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_BX)
		p->chain->op_BX(ctx, p->chain);
}

static void
pdf_color_EX(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_EX)
		p->chain->op_EX(ctx, p->chain);
}

static void
pdf_color_END(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	if (p->chain->op_END)
		p->chain->op_END(ctx, p->chain);
}

static void
pdf_close_color_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;

	pdf_close_processor(ctx, p->chain);
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
	drop_rewritten_images(ctx, p);
	drop_rewritten_shades(ctx, p);

	pdf_drop_document(ctx, p->doc);
}

static void
pdf_color_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	resources_stack *stk = fz_malloc_struct(ctx, resources_stack);
	pdf_obj *obj;

	p->gstate->unmarked = UNMARKED_STROKE | UNMARKED_FILL;

	stk->next = p->rstack;
	p->rstack = stk;
	fz_try(ctx)
	{
		stk->old_rdb = pdf_keep_obj(ctx, res);
		/* At the moment we know that we'll always be flattening XObjects.
		 * So only the top level 'push' makes a new resource dict. Any
		 * subsequent one will share the previous levels one. */
		if (stk->next)
			stk->new_rdb = pdf_keep_obj(ctx, stk->next->new_rdb);
		else
			stk->new_rdb = pdf_new_dict(ctx, p->doc, 1);

		obj = pdf_dict_get(ctx, res, PDF_NAME(Properties));
		if (obj)
			pdf_dict_put(ctx, stk->new_rdb, PDF_NAME(Properties), obj);
		obj = pdf_dict_get(ctx, res, PDF_NAME(ExtGState));
		if (obj)
			pdf_dict_put(ctx, stk->new_rdb, PDF_NAME(ExtGState), obj);
		obj = pdf_dict_get(ctx, res, PDF_NAME(Font));
		if (obj)
			pdf_dict_put(ctx, stk->new_rdb, PDF_NAME(Font), obj);

		pdf_processor_push_resources(ctx, p->chain, stk->new_rdb);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, stk->old_rdb);
		pdf_drop_obj(ctx, stk->new_rdb);
		fz_free(ctx, stk);
		p->rstack = stk->next;
		fz_rethrow(ctx);
	}
}

static pdf_obj *
pdf_color_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_color_processor *p = (pdf_color_processor*)proc;
	resources_stack *stk = p->rstack;

	p->rstack = stk->next;
	pdf_drop_obj(ctx, stk->old_rdb);
	pdf_drop_obj(ctx, stk->new_rdb);
	fz_free(ctx, stk);

	return pdf_processor_pop_resources(ctx, p->chain);
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

	/* general graphics state */
	proc->super.op_w = pdf_color_w;
	proc->super.op_j = pdf_color_j;
	proc->super.op_J = pdf_color_J;
	proc->super.op_M = pdf_color_M;
	proc->super.op_d = pdf_color_d;
	proc->super.op_ri = pdf_color_ri;
	proc->super.op_i = pdf_color_i;
	proc->super.op_gs_begin = pdf_color_gs_begin;
	proc->super.op_gs_end = pdf_color_gs_end;

	/* transparency graphics state */
	proc->super.op_gs_BM = pdf_color_gs_BM;
	proc->super.op_gs_CA = pdf_color_gs_CA;
	proc->super.op_gs_ca = pdf_color_gs_ca;
	proc->super.op_gs_SMask = pdf_color_gs_SMask;

	/* special graphics state */
	proc->super.op_q = pdf_color_q;
	proc->super.op_Q = pdf_color_Q;
	proc->super.op_cm = pdf_color_cm;

	/* path construction */
	proc->super.op_m = pdf_color_m;
	proc->super.op_l = pdf_color_l;
	proc->super.op_c = pdf_color_c;
	proc->super.op_v = pdf_color_v;
	proc->super.op_y = pdf_color_y;
	proc->super.op_h = pdf_color_h;
	proc->super.op_re = pdf_color_re;

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
	proc->super.op_n = pdf_color_n;

	/* clipping paths */
	proc->super.op_W = pdf_color_W;
	proc->super.op_Wstar = pdf_color_Wstar;

	/* text objects */
	proc->super.op_BT = pdf_color_BT;
	proc->super.op_ET = pdf_color_ET;

	/* text state */
	proc->super.op_Tc = pdf_color_Tc;
	proc->super.op_Tw = pdf_color_Tw;
	proc->super.op_Tz = pdf_color_Tz;
	proc->super.op_TL = pdf_color_TL;
	proc->super.op_Tf = pdf_color_Tf;
	proc->super.op_Tr = pdf_color_Tr;
	proc->super.op_Ts = pdf_color_Ts;

	/* text positioning */
	proc->super.op_Td = pdf_color_Td;
	proc->super.op_TD = pdf_color_TD;
	proc->super.op_Tm = pdf_color_Tm;
	proc->super.op_Tstar = pdf_color_Tstar;

	/* text showing */
	proc->super.op_TJ = pdf_color_TJ;
	proc->super.op_Tj = pdf_color_Tj;
	proc->super.op_squote = pdf_color_squote;
	proc->super.op_dquote = pdf_color_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = pdf_color_d0;
	proc->super.op_d1 = pdf_color_d1;

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

	/* marked content */
	proc->super.op_MP = pdf_color_MP;
	proc->super.op_DP = pdf_color_DP;
	proc->super.op_BMC = pdf_color_BMC;
	proc->super.op_BDC = pdf_color_BDC;
	proc->super.op_EMC = pdf_color_EMC;

	/* compatibility */
	proc->super.op_BX = pdf_color_BX;
	proc->super.op_EX = pdf_color_EX;

	/* extgstate */
	proc->super.op_gs_OP = pdf_color_gs_OP;
	proc->super.op_gs_op = pdf_color_gs_op;
	proc->super.op_gs_OPM = pdf_color_gs_OPM;
	proc->super.op_gs_UseBlackPtComp = pdf_color_gs_UseBlackPtComp;

	proc->super.op_END = pdf_color_END;

	fz_try(ctx)
		proc->gstate = fz_malloc_struct(ctx, gstate_stack);
	fz_catch(ctx)
	{
		fz_free(ctx, proc);
		fz_rethrow(ctx);
	}
	proc->gstate->ctm = fz_identity;

	proc->doc = pdf_keep_document(ctx, doc);
	proc->chain = chain;
	proc->global_options = global_options;
	proc->options = options;

	return (pdf_processor*)proc;
}
