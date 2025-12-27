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
#include "pdf-annot-imp.h"

#include <string.h>
#include <math.h>

/* Maximum number of errors before aborting */
#define MAX_SYNTAX_ERRORS 100

pdf_obj *
pdf_lookup_resource(fz_context *ctx, pdf_resource_stack *stack, pdf_obj *type, const char *name)
{
	pdf_obj *sub, *obj;
	while (stack)
	{
		sub = pdf_dict_get(ctx, stack->resources, type);
		if (sub)
		{
			obj = pdf_dict_gets(ctx, sub, name);
			if (obj)
				return obj;
		}
		stack = stack->next;
	}
	return NULL;
}

#define PASSON(A) if (proc && proc->chain && proc->chain->A) proc->chain->A
static void
pdf_default_close_processor(fz_context *ctx, pdf_processor *proc)
{
	if (proc && proc->chain)
		pdf_close_processor(ctx, proc->chain);
}

static void
pdf_default_drop_processor(fz_context *ctx, pdf_processor *proc)
{
	PASSON(drop_processor)(ctx, proc->chain);
}

static void
pdf_default_reset_processor(fz_context *ctx, pdf_processor *proc)
{
	if (proc && proc->chain)
		pdf_reset_processor(ctx, proc->chain);
}

static void
pdf_default_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	if (proc && proc->chain)
		pdf_processor_push_resources(ctx, proc->chain, res);
}

static pdf_obj *
pdf_default_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	if (proc && proc->chain)
		return pdf_processor_pop_resources(ctx, proc->chain);
	return NULL;
}

static void
pdf_default_op_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	PASSON(op_w)(ctx, proc->chain, linewidth);
}

static void
pdf_default_op_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	PASSON(op_j)(ctx, proc->chain, linejoin);
}

static void
pdf_default_op_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	PASSON(op_J)(ctx, proc->chain, linecap);
}

static void
pdf_default_op_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	PASSON(op_M)(ctx, proc->chain, miterlimit);
}

static void
pdf_default_op_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	PASSON(op_d)(ctx, proc->chain, array, phase);
}

static void
pdf_default_op_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	PASSON(op_ri)(ctx, proc->chain, intent);
}

static void
pdf_default_op_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	PASSON(op_i)(ctx, proc->chain, flatness);
}

static void
pdf_default_op_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	PASSON(op_gs_begin)(ctx, proc->chain, name, extgstate);
}

static void
pdf_default_op_gs_BM(fz_context *ctx, pdf_processor *proc, const char *blendmode)
{
	PASSON(op_gs_BM)(ctx, proc->chain, blendmode);
}

static void
pdf_default_op_gs_ca(fz_context *ctx, pdf_processor *proc, float alpha)
{
	PASSON(op_gs_ca)(ctx, proc->chain, alpha);
}

static void
pdf_default_op_gs_CA(fz_context *ctx, pdf_processor *proc, float alpha)
{
	PASSON(op_gs_CA)(ctx, proc->chain, alpha);
}

static void
pdf_default_op_gs_SMask(fz_context *ctx, pdf_processor *proc, pdf_obj *smask, fz_colorspace *smask_cs, float *bc, int luminosity, pdf_obj *tr)
{
	PASSON(op_gs_SMask)(ctx, proc->chain, smask, smask_cs, bc, luminosity, tr);
}

static void
pdf_default_op_gs_end(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_gs_end)(ctx, proc->chain);
}

static void
pdf_default_op_q(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_q)(ctx, proc->chain);
}

static void
pdf_default_op_Q(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_Q)(ctx, proc->chain);
}

static void
pdf_default_op_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	PASSON(op_cm)(ctx, proc->chain, a, b, c, d, e, f);
}

static void
pdf_default_op_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	PASSON(op_m)(ctx, proc->chain, x, y);
}

static void
pdf_default_op_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	PASSON(op_l)(ctx, proc->chain, x, y);
}

static void
pdf_default_op_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	PASSON(op_c)(ctx, proc->chain, x1, y1, x2, y2, x3, y3);
}

static void
pdf_default_op_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	PASSON(op_v)(ctx, proc->chain, x2, y2, x3, y3);
}

static void
pdf_default_op_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	PASSON(op_y)(ctx, proc->chain, x1, y1, x3, y3);
}

static void
pdf_default_op_h(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_h)(ctx, proc->chain);
}

static void
pdf_default_op_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	PASSON(op_re)(ctx, proc->chain, x, y, w, h);
}

static void
pdf_default_op_S(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_S)(ctx, proc->chain);
}

static void
pdf_default_op_s(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_s)(ctx, proc->chain);
}

static void
pdf_default_op_F(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_F)(ctx, proc->chain);
}

static void
pdf_default_op_f(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_f)(ctx, proc->chain);
}

static void
pdf_default_op_fstar(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_fstar)(ctx, proc->chain);
}

static void
pdf_default_op_B(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_B)(ctx, proc->chain);
}

static void
pdf_default_op_Bstar(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_Bstar)(ctx, proc->chain);
}

static void
pdf_default_op_b(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_b)(ctx, proc->chain);
}

static void
pdf_default_op_bstar(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_bstar)(ctx, proc->chain);
}

static void
pdf_default_op_n(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_n)(ctx, proc->chain);
}

static void
pdf_default_op_W(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_W)(ctx, proc->chain);
}

static void
pdf_default_op_Wstar(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_Wstar)(ctx, proc->chain);
}

static void
pdf_default_op_BT(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_BT)(ctx, proc->chain);
}

static void
pdf_default_op_ET(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_ET)(ctx, proc->chain);
}

static void
pdf_default_op_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	PASSON(op_Tc)(ctx, proc->chain, charspace);
}

static void
pdf_default_op_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	PASSON(op_Tw)(ctx, proc->chain, wordspace);
}

static void
pdf_default_op_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	PASSON(op_Tz)(ctx, proc->chain, scale);
}

static void
pdf_default_op_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	PASSON(op_TL)(ctx, proc->chain, leading);
}

static void
pdf_default_op_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	PASSON(op_Tf)(ctx, proc->chain, name, font, size);
}

static void
pdf_default_op_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	PASSON(op_Tr)(ctx, proc->chain, render);
}

static void
pdf_default_op_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	PASSON(op_Ts)(ctx, proc->chain, rise);
}

static void
pdf_default_op_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	PASSON(op_Td)(ctx, proc->chain, tx, ty);
}

static void
pdf_default_op_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	PASSON(op_TD)(ctx, proc->chain, tx, ty);
}

static void
pdf_default_op_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	PASSON(op_Tm)(ctx, proc->chain, a, b, c, d, e, f);
}

static void
pdf_default_op_Tstar(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_Tstar)(ctx, proc->chain);
}

static void
pdf_default_op_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	PASSON(op_TJ)(ctx, proc->chain, array);
}

static void
pdf_default_op_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	PASSON(op_Tj)(ctx, proc->chain, str, len);
}

static void
pdf_default_op_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	PASSON(op_squote)(ctx, proc->chain, str, len);
}

static void
pdf_default_op_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	PASSON(op_dquote)(ctx, proc->chain, aw, ac, str, len);
}

static void
pdf_default_op_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	PASSON(op_d0)(ctx, proc->chain, wx, wy);
}

static void
pdf_default_op_d1(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury)
{
	PASSON(op_d1)(ctx, proc->chain, wx, wy, llx, lly, urx, ury);
}

static void
pdf_default_op_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	PASSON(op_CS)(ctx, proc->chain, name, cs);
}

static void
pdf_default_op_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	PASSON(op_cs)(ctx, proc->chain, name, cs);
}

static void
pdf_default_op_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	PASSON(op_SC_pattern)(ctx, proc->chain, name, pat, n, color);
}

static void
pdf_default_op_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	PASSON(op_sc_pattern)(ctx, proc->chain, name, pat, n, color);
}

static void
pdf_default_op_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PASSON(op_SC_shade)(ctx, proc->chain, name, shade);
}

static void
pdf_default_op_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PASSON(op_sc_shade)(ctx, proc->chain, name, shade);
}

static void
pdf_default_op_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	PASSON(op_SC_color)(ctx, proc->chain, n, color);
}

static void
pdf_default_op_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	PASSON(op_sc_color)(ctx, proc->chain, n, color);
}

static void
pdf_default_op_G(fz_context *ctx, pdf_processor *proc, float g)
{
	PASSON(op_G)(ctx, proc->chain, g);
}

static void
pdf_default_op_g(fz_context *ctx, pdf_processor *proc, float g)
{
	PASSON(op_g)(ctx, proc->chain, g);
}

static void
pdf_default_op_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	PASSON(op_RG)(ctx, proc->chain, r, g, b);
}

static void
pdf_default_op_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	PASSON(op_rg)(ctx, proc->chain, r, g, b);
}

static void
pdf_default_op_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	PASSON(op_K)(ctx, proc->chain, c, m, y, k);
}

static void
pdf_default_op_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	PASSON(op_k)(ctx, proc->chain, c, m, y, k);
}

static void
pdf_default_op_BI(fz_context *ctx, pdf_processor *proc, fz_image *image, const char *colorspace_name)
{
	PASSON(op_BI)(ctx, proc->chain, image, colorspace_name);
}

static void
pdf_default_op_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	PASSON(op_sh)(ctx, proc->chain, name, shade);
}

static void
pdf_default_op_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	PASSON(op_Do_image)(ctx, proc->chain, name, image);
}

static void
pdf_default_op_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *form)
{
	PASSON(op_Do_form)(ctx, proc->chain, name, form);
}

static void
pdf_default_op_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	PASSON(op_MP)(ctx, proc->chain, tag);
}

static void
pdf_default_op_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	PASSON(op_DP)(ctx, proc->chain, tag, raw, cooked);
}

static void
pdf_default_op_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	PASSON(op_BMC)(ctx, proc->chain, tag);
}

static void
pdf_default_op_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	PASSON(op_BDC)(ctx, proc->chain, tag, raw, cooked);
}

static void
pdf_default_op_EMC(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_EMC)(ctx, proc->chain);
}

static void
pdf_default_op_BX(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_BX)(ctx, proc->chain);
}

static void
pdf_default_op_EX(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_EX)(ctx, proc->chain);
}

static void
pdf_default_op_gs_OP(fz_context *ctx, pdf_processor *proc, int b)
{
	PASSON(op_gs_OP)(ctx, proc->chain, b);
}

static void
pdf_default_op_gs_op(fz_context *ctx, pdf_processor *proc, int b)
{
	PASSON(op_gs_op)(ctx, proc->chain, b);
}

static void
pdf_default_op_gs_OPM(fz_context *ctx, pdf_processor *proc, int i)
{
	PASSON(op_gs_OPM)(ctx, proc->chain, i);
}

static void
pdf_default_op_gs_UseBlackPtComp(fz_context *ctx, pdf_processor *proc, pdf_obj *name)
{
	PASSON(op_gs_UseBlackPtComp)(ctx, proc->chain, name);
}

static void
pdf_default_op_EOD(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_EOD)(ctx, proc->chain);
}

static void
pdf_default_op_END(fz_context *ctx, pdf_processor *proc)
{
	PASSON(op_END)(ctx, proc->chain);
}

#undef PASSON

void *
pdf_new_processor(fz_context *ctx, int size)
{
	pdf_processor *ret = Memento_label(fz_calloc(ctx, 1, size), "pdf_processor");
	ret->refs = 1;

	ret->close_processor = pdf_default_close_processor;
	ret->drop_processor = pdf_default_drop_processor;
	ret->reset_processor = pdf_default_reset_processor;
	ret->push_resources = pdf_default_push_resources;
	ret->pop_resources = pdf_default_pop_resources;
	ret->op_w = pdf_default_op_w;
	ret->op_j = pdf_default_op_j;
	ret->op_J = pdf_default_op_J;
	ret->op_M = pdf_default_op_M;
	ret->op_d = pdf_default_op_d;
	ret->op_ri = pdf_default_op_ri;
	ret->op_i = pdf_default_op_i;
	ret->op_gs_begin = pdf_default_op_gs_begin;
	ret->op_gs_BM = pdf_default_op_gs_BM;
	ret->op_gs_ca = pdf_default_op_gs_ca;
	ret->op_gs_CA = pdf_default_op_gs_CA;
	ret->op_gs_SMask = pdf_default_op_gs_SMask;
	ret->op_gs_end = pdf_default_op_gs_end;
	ret->op_q = pdf_default_op_q;
	ret->op_Q = pdf_default_op_Q;
	ret->op_cm = pdf_default_op_cm;
	ret->op_m = pdf_default_op_m;
	ret->op_l = pdf_default_op_l;
	ret->op_c = pdf_default_op_c;
	ret->op_v = pdf_default_op_v;
	ret->op_y = pdf_default_op_y;
	ret->op_h = pdf_default_op_h;
	ret->op_re = pdf_default_op_re;
	ret->op_S = pdf_default_op_S;
	ret->op_B = pdf_default_op_B;
	ret->op_s = pdf_default_op_s;
	ret->op_F = pdf_default_op_F;
	ret->op_f = pdf_default_op_f;
	ret->op_fstar = pdf_default_op_fstar;
	ret->op_B = pdf_default_op_B;
	ret->op_Bstar = pdf_default_op_Bstar;
	ret->op_b = pdf_default_op_b;
	ret->op_bstar = pdf_default_op_bstar;
	ret->op_n = pdf_default_op_n;
	ret->op_W = pdf_default_op_W;
	ret->op_Wstar = pdf_default_op_Wstar;
	ret->op_BT = pdf_default_op_BT;
	ret->op_ET = pdf_default_op_ET;
	ret->op_Tc = pdf_default_op_Tc;
	ret->op_Tw = pdf_default_op_Tw;
	ret->op_Tz = pdf_default_op_Tz;
	ret->op_TL = pdf_default_op_TL;
	ret->op_Tf = pdf_default_op_Tf;
	ret->op_Tr = pdf_default_op_Tr;
	ret->op_Ts = pdf_default_op_Ts;
	ret->op_Td = pdf_default_op_Td;
	ret->op_TD = pdf_default_op_TD;
	ret->op_Tm = pdf_default_op_Tm;
	ret->op_Tstar = pdf_default_op_Tstar;
	ret->op_TJ = pdf_default_op_TJ;
	ret->op_Tj = pdf_default_op_Tj;
	ret->op_squote = pdf_default_op_squote;
	ret->op_dquote = pdf_default_op_dquote;
	ret->op_d0 = pdf_default_op_d0;
	ret->op_d1 = pdf_default_op_d1;
	ret->op_CS = pdf_default_op_CS;
	ret->op_cs = pdf_default_op_cs;
	ret->op_SC_pattern = pdf_default_op_SC_pattern;
	ret->op_sc_pattern = pdf_default_op_sc_pattern;
	ret->op_SC_shade = pdf_default_op_SC_shade;
	ret->op_sc_shade = pdf_default_op_sc_shade;
	ret->op_SC_color = pdf_default_op_SC_color;
	ret->op_sc_color = pdf_default_op_sc_color;
	ret->op_G = pdf_default_op_G;
	ret->op_g = pdf_default_op_g;
	ret->op_RG = pdf_default_op_RG;
	ret->op_rg = pdf_default_op_rg;
	ret->op_K = pdf_default_op_K;
	ret->op_k = pdf_default_op_k;
	ret->op_BI = pdf_default_op_BI;
	ret->op_sh = pdf_default_op_sh;
	ret->op_Do_image = pdf_default_op_Do_image;
	ret->op_Do_form = pdf_default_op_Do_form;
	ret->op_MP = pdf_default_op_MP;
	ret->op_DP = pdf_default_op_DP;
	ret->op_BMC = pdf_default_op_BMC;
	ret->op_BDC = pdf_default_op_BDC;
	ret->op_EMC = pdf_default_op_EMC;
	ret->op_BX = pdf_default_op_BX;
	ret->op_EX = pdf_default_op_EX;
	ret->op_gs_OP = pdf_default_op_gs_OP;
	ret->op_gs_op = pdf_default_op_gs_op;
	ret->op_gs_OPM = pdf_default_op_gs_OPM;
	ret->op_gs_UseBlackPtComp = pdf_default_op_gs_UseBlackPtComp;
	ret->op_EOD = pdf_default_op_EOD;
	ret->op_END = pdf_default_op_END;

	return ret;
}

pdf_processor *
pdf_keep_processor(fz_context *ctx, pdf_processor *proc)
{
	return fz_keep_imp(ctx, proc, &proc->refs);
}

void
pdf_close_processor(fz_context *ctx, pdf_processor *proc)
{
	void (*close_processor)(fz_context *ctx, pdf_processor *proc);

	if (!proc || proc->closed)
		return;

	proc->closed = 1;
	close_processor = proc->close_processor;
	if (!close_processor)
		return;

	close_processor(ctx, proc); /* Tail recursion */
}

void
pdf_drop_processor(fz_context *ctx, pdf_processor *proc)
{
	if (fz_drop_imp(ctx, proc, &proc->refs))
	{
		if (!proc->closed)
			fz_warn(ctx, "dropping unclosed PDF processor");
		if (proc->drop_processor)
			proc->drop_processor(ctx, proc);
		while (proc->rstack)
		{
			pdf_resource_stack *stk = proc->rstack;
			proc->rstack = stk->next;
			pdf_drop_obj(ctx, stk->resources);
			fz_free(ctx, stk);
		}
		fz_free(ctx, proc);
	}
}

void pdf_reset_processor(fz_context *ctx, pdf_processor *proc)
{
	if (proc == NULL)
		return;

	proc->closed = 0;

	if (proc->reset_processor == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot reset PDF processor");

	proc->reset_processor(ctx, proc);
}

static void
pdf_init_csi(fz_context *ctx, pdf_csi *csi, pdf_document *doc, pdf_lexbuf *buf, fz_cookie *cookie)
{
	memset(csi, 0, sizeof *csi);
	csi->doc = doc;
	csi->buf = buf;
	csi->cookie = cookie;
}

static void
pdf_clear_stack(fz_context *ctx, pdf_csi *csi)
{
	int i;

	pdf_drop_obj(ctx, csi->obj);
	csi->obj = NULL;

	csi->name[0] = 0;
	csi->string_len = 0;
	for (i = 0; i < csi->top; i++)
		csi->stack[i] = 0;

	csi->top = 0;
}

static pdf_font_desc *
pdf_try_load_font(fz_context *ctx, pdf_document *doc, pdf_resource_stack *rdb, pdf_obj *font, fz_cookie *cookie)
{
	pdf_font_desc *desc = NULL;
	fz_try(ctx)
		desc = pdf_load_font(ctx, doc, rdb, font);
	fz_catch(ctx)
	{
		if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		{
			fz_ignore_error(ctx);
			if (cookie)
				cookie->incomplete++;
		}
		else
		{
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
		}
	}
	if (desc == NULL)
		desc = pdf_load_hail_mary_font(ctx, doc);
	return desc;
}

static fz_image *
parse_inline_image(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, fz_stream *stm, char *csname, int cslen)
{
	pdf_document *doc = csi->doc;
	pdf_obj *obj = NULL;
	pdf_obj *cs;
	fz_image *img = NULL;
	int ch, found;

	fz_var(obj);
	fz_var(img);

	fz_try(ctx)
	{
		obj = pdf_parse_dict(ctx, doc, stm, &doc->lexbuf.base);

		if (csname)
		{
			cs = pdf_dict_get(ctx, obj, PDF_NAME(CS));
			if (!pdf_is_indirect(ctx, cs) && pdf_is_name(ctx, cs))
				fz_strlcpy(csname, pdf_to_name(ctx, cs), cslen);
			else
				csname[0] = 0;
		}

		/* read whitespace after ID keyword */
		ch = fz_read_byte(ctx, stm);
		if (ch == '\r')
			if (fz_peek_byte(ctx, stm) == '\n')
				fz_read_byte(ctx, stm);

		img = pdf_load_inline_image(ctx, doc, proc->rstack, obj, stm);

		/* find EI */
		found = 0;
		ch = fz_read_byte(ctx, stm);
		do
		{
			while (ch != 'E' && ch != EOF)
				ch = fz_read_byte(ctx, stm);
			if (ch == 'E')
			{
				ch = fz_read_byte(ctx, stm);
				if (ch == 'I')
				{
					ch = fz_peek_byte(ctx, stm);
					if (ch == ' ' || ch <= 32 || ch == '<' || ch == '/')
					{
						found = 1;
						break;
					}
				}
			}
		} while (ch != EOF);
		if (!found)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error after inline image");
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_drop_image(ctx, img);
		fz_rethrow(ctx);
	}

	return img;
}

static void
pdf_process_extgstate(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, pdf_obj *dict)
{
	pdf_obj *obj;

	obj = pdf_dict_get(ctx, dict, PDF_NAME(LW));
	if (pdf_is_number(ctx, obj) && proc->op_w)
		proc->op_w(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(LC));
	if (pdf_is_int(ctx, obj) && proc->op_J)
		proc->op_J(ctx, proc, fz_clampi(pdf_to_int(ctx, obj), 0, 2));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(LJ));
	if (pdf_is_int(ctx, obj) && proc->op_j)
		proc->op_j(ctx, proc, fz_clampi(pdf_to_int(ctx, obj), 0, 2));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(ML));
	if (pdf_is_number(ctx, obj) && proc->op_M)
		proc->op_M(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(D));
	if (pdf_is_array(ctx, obj) && proc->op_d)
	{
		pdf_obj *dash_array = pdf_array_get(ctx, obj, 0);
		pdf_obj *dash_phase = pdf_array_get(ctx, obj, 1);
		proc->op_d(ctx, proc, dash_array, pdf_to_real(ctx, dash_phase));
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(RI));
	if (pdf_is_name(ctx, obj) && proc->op_ri)
		proc->op_ri(ctx, proc, pdf_to_name(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(FL));
	if (pdf_is_number(ctx, obj) && proc->op_i)
		proc->op_i(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Font));
	if (pdf_is_array(ctx, obj) && proc->op_Tf)
	{
		pdf_obj *font_ref = pdf_array_get(ctx, obj, 0);
		pdf_obj *font_size = pdf_array_get(ctx, obj, 1);
		pdf_font_desc *font;
		if (pdf_is_dict(ctx, font_ref))
			font = pdf_try_load_font(ctx, csi->doc, proc->rstack, font_ref, csi->cookie);
		else
			font = pdf_load_hail_mary_font(ctx, csi->doc);
		fz_try(ctx)
			proc->op_Tf(ctx, proc, "ExtGState", font, pdf_to_real(ctx, font_size));
		fz_always(ctx)
			pdf_drop_font(ctx, font);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	/* overprint and color management */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(OP));
	if (pdf_is_bool(ctx, obj) && proc->op_gs_OP)
		proc->op_gs_OP(ctx, proc, pdf_to_bool(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(op));
	if (pdf_is_bool(ctx, obj) && proc->op_gs_op)
		proc->op_gs_op(ctx, proc, pdf_to_bool(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(OPM));
	if (pdf_is_int(ctx, obj) && proc->op_gs_OPM)
		proc->op_gs_OPM(ctx, proc, pdf_to_int(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(UseBlackPtComp));
	if (pdf_is_name(ctx, obj) && proc->op_gs_UseBlackPtComp)
		proc->op_gs_UseBlackPtComp(ctx, proc, obj);

	/* transfer functions */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(TR2));
	if (pdf_is_name(ctx, obj))
		if (!pdf_name_eq(ctx, obj, PDF_NAME(Identity)) && !pdf_name_eq(ctx, obj, PDF_NAME(Default)))
			fz_warn(ctx, "ignoring transfer function");
	if (!obj) /* TR is ignored in the presence of TR2 */
	{
		pdf_obj *tr = pdf_dict_get(ctx, dict, PDF_NAME(TR));
		if (pdf_is_name(ctx, tr))
			if (!pdf_name_eq(ctx, tr, PDF_NAME(Identity)))
				fz_warn(ctx, "ignoring transfer function");
	}

	/* transparency state */

	obj = pdf_dict_get(ctx, dict, PDF_NAME(CA));
	if (pdf_is_number(ctx, obj) && proc->op_gs_CA)
		proc->op_gs_CA(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(ca));
	if (pdf_is_number(ctx, obj) && proc->op_gs_ca)
		proc->op_gs_ca(ctx, proc, pdf_to_real(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(BM));
	if (pdf_is_array(ctx, obj))
		obj = pdf_array_get(ctx, obj, 0);
	if (pdf_is_name(ctx, obj) && proc->op_gs_BM)
		proc->op_gs_BM(ctx, proc, pdf_to_name(ctx, obj));

	obj = pdf_dict_get(ctx, dict, PDF_NAME(SMask));
	if (proc->op_gs_SMask)
	{
		if (pdf_is_dict(ctx, obj))
		{
			pdf_obj *xobj, *s, *bc, *tr;
			float softmask_bc[FZ_MAX_COLORS];
			fz_colorspace *softmask_cs;
			int colorspace_n = 1;
			int k, luminosity;

			xobj = pdf_dict_get(ctx, obj, PDF_NAME(G));

			softmask_cs = pdf_xobject_colorspace(ctx, xobj);
			fz_try(ctx)
			{
				if (softmask_cs)
					colorspace_n = fz_colorspace_n(ctx, softmask_cs);

				/* Default background color is black. */
				for (k = 0; k < colorspace_n; k++)
					softmask_bc[k] = 0;
				/* Which in CMYK means not all zeros! This should really be
				 * a test for subtractive color spaces, but this will have
				 * to do for now. */
				if (fz_colorspace_is_cmyk(ctx, softmask_cs))
				{
					/* Default background color is black. */
					for (k = 0; k < colorspace_n; k++)
						softmask_bc[k] = 0;
					/* Which in CMYK means not all zeros! This should really be
					 * a test for subtractive color spaces, but this will have
					 * to do for now. */
					if (fz_colorspace_is_cmyk(ctx, softmask_cs))
						softmask_bc[3] = 1.0f;
				}

				bc = pdf_dict_get(ctx, obj, PDF_NAME(BC));
				if (pdf_is_array(ctx, bc))
				{
					for (k = 0; k < colorspace_n; k++)
						softmask_bc[k] = pdf_array_get_real(ctx, bc, k);
				}

				s = pdf_dict_get(ctx, obj, PDF_NAME(S));
				if (pdf_name_eq(ctx, s, PDF_NAME(Luminosity)))
					luminosity = 1;
				else
					luminosity = 0;

				tr = pdf_dict_get(ctx, obj, PDF_NAME(TR));
				if (tr && pdf_name_eq(ctx, tr, PDF_NAME(Identity)))
					tr = NULL;

				proc->op_gs_SMask(ctx, proc, xobj, softmask_cs, softmask_bc, luminosity, tr);
			}
			fz_always(ctx)
				fz_drop_colorspace(ctx, softmask_cs);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		else if (pdf_is_name(ctx, obj) && pdf_name_eq(ctx, obj, PDF_NAME(None)))
		{
			proc->op_gs_SMask(ctx, proc, NULL, NULL, NULL, 0, NULL);
		}
	}
}

static void
pdf_process_Do(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	pdf_obj *xobj, *subtype;

	xobj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(XObject), csi->name);
	if (!xobj)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find XObject resource '%s'", csi->name);
	subtype = pdf_dict_get(ctx, xobj, PDF_NAME(Subtype));
	if (pdf_name_eq(ctx, subtype, PDF_NAME(Form)))
	{
		pdf_obj *st = pdf_dict_get(ctx, xobj, PDF_NAME(Subtype2));
		if (st)
			subtype = st;
	}
	if (!pdf_is_name(ctx, subtype))
		fz_throw(ctx, FZ_ERROR_SYNTAX, "no XObject subtype specified");

	if (pdf_is_ocg_hidden(ctx, csi->doc, proc->rstack, proc->usage, pdf_dict_get(ctx, xobj, PDF_NAME(OC))))
		return;

	if (pdf_name_eq(ctx, subtype, PDF_NAME(Form)))
	{
		if (proc->op_Do_form)
			proc->op_Do_form(ctx, proc, csi->name, xobj);
	}

	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Image)))
	{
		if (proc->op_Do_image)
		{
			fz_image *image = NULL;

			if (proc->requirements && PDF_PROCESSOR_REQUIRES_DECODED_IMAGES)
				image = pdf_load_image(ctx, csi->doc, xobj);
			fz_try(ctx)
				proc->op_Do_image(ctx, proc, csi->name, image);
			fz_always(ctx)
				fz_drop_image(ctx, image);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
	}

	else if (!strcmp(pdf_to_name(ctx, subtype), "PS"))
		fz_warn(ctx, "ignoring XObject with subtype PS");
	else
		fz_warn(ctx, "ignoring XObject with unknown subtype: '%s'", pdf_to_name(ctx, subtype));
}

static void
pdf_process_CS(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, int stroke)
{
	fz_colorspace *cs;

	if (!proc->op_CS || !proc->op_cs)
		return;

	if (!strcmp(csi->name, "Pattern"))
	{
		if (stroke)
			proc->op_CS(ctx, proc, "Pattern", NULL);
		else
			proc->op_cs(ctx, proc, "Pattern", NULL);
		return;
	}

	if (!strcmp(csi->name, "DeviceGray"))
		cs = fz_keep_colorspace(ctx, fz_device_gray(ctx));
	else if (!strcmp(csi->name, "DeviceRGB"))
		cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	else if (!strcmp(csi->name, "DeviceCMYK"))
		cs = fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
	else
	{
		pdf_obj *csobj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(ColorSpace), csi->name);
		if (!csobj)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find ColorSpace resource '%s'", csi->name);
		if (pdf_is_array(ctx, csobj) && pdf_array_len(ctx, csobj) == 1 && pdf_name_eq(ctx, pdf_array_get(ctx, csobj, 0), PDF_NAME(Pattern)))
		{
			if (stroke)
				proc->op_CS(ctx, proc, "Pattern", NULL);
			else
				proc->op_cs(ctx, proc, "Pattern", NULL);
			return;
		}
		cs = pdf_load_colorspace(ctx, csobj);
	}

	fz_try(ctx)
	{
		if (stroke)
			proc->op_CS(ctx, proc, csi->name, cs);
		else
			proc->op_cs(ctx, proc, csi->name, cs);
	}
	fz_always(ctx)
		fz_drop_colorspace(ctx, cs);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_process_SC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, int stroke)
{
	if (csi->name[0])
	{
		pdf_obj *patobj;
		int type;

		patobj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Pattern), csi->name);
		if (!patobj)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find Pattern resource '%s'", csi->name);

		type = pdf_dict_get_int(ctx, patobj, PDF_NAME(PatternType));

		if (type == 1)
		{
			if (proc->op_SC_pattern && proc->op_sc_pattern)
			{
				pdf_pattern *pat = pdf_load_pattern(ctx, csi->doc, patobj);
				fz_try(ctx)
				{
					if (stroke)
						proc->op_SC_pattern(ctx, proc, csi->name, pat, csi->top, csi->stack);
					else
						proc->op_sc_pattern(ctx, proc, csi->name, pat, csi->top, csi->stack);
				}
				fz_always(ctx)
					pdf_drop_pattern(ctx, pat);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
		}

		else if (type == 2)
		{
			if (proc->op_SC_shade && proc->op_sc_shade)
			{
				fz_shade *shade = pdf_load_shading(ctx, csi->doc, patobj);
				fz_try(ctx)
				{
					if (stroke)
						proc->op_SC_shade(ctx, proc, csi->name, shade);
					else
						proc->op_sc_shade(ctx, proc, csi->name, shade);
				}
				fz_always(ctx)
					fz_drop_shade(ctx, shade);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
		}

		else
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown pattern type: %d", type);
		}
	}

	else
	{
		if (proc->op_SC_color && proc->op_sc_color)
		{
			if (stroke)
				proc->op_SC_color(ctx, proc, csi->top, csi->stack);
			else
				proc->op_sc_color(ctx, proc, csi->top, csi->stack);
		}
	}
}

static pdf_obj *
resolve_properties(fz_context *ctx, pdf_processor *proc, pdf_obj *obj)
{
	if (pdf_is_name(ctx, obj))
		return pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Properties), pdf_to_name(ctx, obj));
	else
		return obj;
}

static void
pdf_process_BDC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (proc->op_BDC)
		proc->op_BDC(ctx, proc, csi->name, csi->obj, resolve_properties(ctx, proc, csi->obj));

	/* Already hidden, no need to look further */
	if (proc->hidden > 0)
	{
		++proc->hidden;
		return;
	}

	/* We only look at OC groups here */
	if (strcmp(csi->name, "OC"))
		return;

	if (pdf_is_ocg_hidden(ctx, csi->doc, proc->rstack, proc->usage, csi->obj))
		++proc->hidden;
}

static void
pdf_process_BMC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, const char *name)
{
	if (proc->op_BMC)
		proc->op_BMC(ctx, proc, name);
	if (proc->hidden > 0)
		++proc->hidden;
}

static void
pdf_process_EMC(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (proc->op_EMC)
		proc->op_EMC(ctx, proc);
	if (proc->hidden > 0)
		--proc->hidden;
}

static void
pdf_process_gsave(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	++csi->gstate;
	if (proc->op_q)
		proc->op_q(ctx, proc);
}

static void
pdf_process_grestore(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	--csi->gstate;
	if (proc->op_Q)
		proc->op_Q(ctx, proc);
}

static void
pdf_process_end(fz_context *ctx, pdf_processor *proc, pdf_csi *csi)
{
	if (proc->op_EOD)
		proc->op_EOD(ctx, proc);
	while (csi->gstate > 0)
		pdf_process_grestore(ctx, proc, csi);
	if (proc->op_END)
		proc->op_END(ctx, proc);
}

static int is_known_bad_word(const char *word)
{
	switch (*word)
	{
	case 'I': return !strcmp(word, "Infinity");
	case 'N': return !strcmp(word, "NaN");
	case 'i': return !strcmp(word, "inf");
	case 'n': return !strcmp(word, "nan");
	}
	return 0;
}

#define A(a) (a)
#define B(a,b) (a | b << 8)
#define C(a,b,c) (a | b << 8 | c << 16)

static void
pdf_process_keyword(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, fz_stream *stm, char *word)
{
	float *s = csi->stack;
	char csname[40];
	int key;

	key = word[0];
	if (word[1])
	{
		key |= word[1] << 8;
		if (word[2])
		{
			key |= word[2] << 16;
			if (word[3])
				key = 0;
		}
	}

	switch (key)
	{
	default:
		if (!csi->xbalance)
		{
			if (is_known_bad_word(word))
				fz_warn(ctx, "unknown keyword: '%s'", word);
			else
				fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown keyword: '%s'", word);
		}
		break;

	/* general graphics state */
	case A('w'): if (proc->op_w) proc->op_w(ctx, proc, s[0]); break;
	case A('j'): if (proc->op_j) proc->op_j(ctx, proc, fz_clampi(s[0], 0, 2)); break;
	case A('J'): if (proc->op_J) proc->op_J(ctx, proc, fz_clampi(s[0], 0, 2)); break;
	case A('M'): if (proc->op_M) proc->op_M(ctx, proc, s[0]); break;
	case A('d'): if (proc->op_d) proc->op_d(ctx, proc, csi->obj, s[0]); break;
	case B('r','i'): if (proc->op_ri) proc->op_ri(ctx, proc, csi->name); break;
	case A('i'): if (proc->op_i) proc->op_i(ctx, proc, s[0]); break;

	case B('g','s'):
		{
			pdf_obj *gsobj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(ExtGState), csi->name);
			if (!gsobj)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find ExtGState resource '%s'", csi->name);
			if (proc->op_gs_begin)
				proc->op_gs_begin(ctx, proc, csi->name, gsobj);
			pdf_process_extgstate(ctx, proc, csi, gsobj);
			if (proc->op_gs_end)
				proc->op_gs_end(ctx, proc);
		}
		break;

	/* special graphics state */
	case A('q'): pdf_process_gsave(ctx, proc, csi); break;
	case A('Q'): pdf_process_grestore(ctx, proc, csi); break;
	case B('c','m'): if (proc->op_cm) proc->op_cm(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;

	/* path construction */
	case A('m'): if (proc->op_m) proc->op_m(ctx, proc, s[0], s[1]); break;
	case A('l'): if (proc->op_l) proc->op_l(ctx, proc, s[0], s[1]); break;
	case A('c'): if (proc->op_c) proc->op_c(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;
	case A('v'): if (proc->op_v) proc->op_v(ctx, proc, s[0], s[1], s[2], s[3]); break;
	case A('y'): if (proc->op_y) proc->op_y(ctx, proc, s[0], s[1], s[2], s[3]); break;
	case A('h'): if (proc->op_h) proc->op_h(ctx, proc); break;
	case B('r','e'): if (proc->op_re) proc->op_re(ctx, proc, s[0], s[1], s[2], s[3]); break;

	/* path painting */
	case A('S'): if (proc->op_S) proc->op_S(ctx, proc); break;
	case A('s'): if (proc->op_s) proc->op_s(ctx, proc); break;
	case A('F'): if (proc->op_F) proc->op_F(ctx, proc); break;
	case A('f'): if (proc->op_f) proc->op_f(ctx, proc); break;
	case B('f','*'): if (proc->op_fstar) proc->op_fstar(ctx, proc); break;
	case A('B'): if (proc->op_B) proc->op_B(ctx, proc); break;
	case B('B','*'): if (proc->op_Bstar) proc->op_Bstar(ctx, proc); break;
	case A('b'): if (proc->op_b) proc->op_b(ctx, proc); break;
	case B('b','*'): if (proc->op_bstar) proc->op_bstar(ctx, proc); break;
	case A('n'): if (proc->op_n) proc->op_n(ctx, proc); break;

	/* path clipping */
	case A('W'): if (proc->op_W) proc->op_W(ctx, proc); break;
	case B('W','*'): if (proc->op_Wstar) proc->op_Wstar(ctx, proc); break;

	/* text objects */
	case B('B','T'): csi->in_text = 1; if (proc->op_BT) proc->op_BT(ctx, proc); break;
	case B('E','T'): csi->in_text = 0; if (proc->op_ET) proc->op_ET(ctx, proc); break;

	/* text state */
	case B('T','c'): if (proc->op_Tc) proc->op_Tc(ctx, proc, s[0]); break;
	case B('T','w'): if (proc->op_Tw) proc->op_Tw(ctx, proc, s[0]); break;
	case B('T','z'): if (proc->op_Tz) proc->op_Tz(ctx, proc, s[0]); break;
	case B('T','L'): if (proc->op_TL) proc->op_TL(ctx, proc, s[0]); break;
	case B('T','r'): if (proc->op_Tr) proc->op_Tr(ctx, proc, s[0]); break;
	case B('T','s'): if (proc->op_Ts) proc->op_Ts(ctx, proc, s[0]); break;

	case B('T','f'):
		if (proc->op_Tf)
		{
			pdf_obj *fontobj;
			pdf_font_desc *font;
			fontobj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Font), csi->name);
			if (pdf_is_dict(ctx, fontobj))
				font = pdf_try_load_font(ctx, csi->doc, proc->rstack, fontobj, csi->cookie);
			else
				font = pdf_load_hail_mary_font(ctx, csi->doc);
			fz_try(ctx)
				proc->op_Tf(ctx, proc, csi->name, font, s[0]);
			fz_always(ctx)
				pdf_drop_font(ctx, font);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		break;

	/* text positioning */
	case B('T','d'): if (proc->op_Td) proc->op_Td(ctx, proc, s[0], s[1]); break;
	case B('T','D'): if (proc->op_TD) proc->op_TD(ctx, proc, s[0], s[1]); break;
	case B('T','m'): if (proc->op_Tm) proc->op_Tm(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;
	case B('T','*'): if (proc->op_Tstar) proc->op_Tstar(ctx, proc); break;

	/* text showing */
	case B('T','J'): if (proc->op_TJ) proc->op_TJ(ctx, proc, csi->obj); break;
	case B('T','j'):
		if (proc->op_Tj)
		{
			if (csi->string_len > 0)
				proc->op_Tj(ctx, proc, csi->string, csi->string_len);
			else
				proc->op_Tj(ctx, proc, pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
		}
		break;
	case A('\''):
		if (proc->op_squote)
		{
			if (csi->string_len > 0)
				proc->op_squote(ctx, proc, csi->string, csi->string_len);
			else
				proc->op_squote(ctx, proc, pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
		}
		break;
	case A('"'):
		if (proc->op_dquote)
		{
			if (csi->string_len > 0)
				proc->op_dquote(ctx, proc, s[0], s[1], csi->string, csi->string_len);
			else
				proc->op_dquote(ctx, proc, s[0], s[1], pdf_to_str_buf(ctx, csi->obj), pdf_to_str_len(ctx, csi->obj));
		}
		break;

	/* type 3 fonts */
	case B('d','0'): if (proc->op_d0) proc->op_d0(ctx, proc, s[0], s[1]); break;
	case B('d','1'): if (proc->op_d1) proc->op_d1(ctx, proc, s[0], s[1], s[2], s[3], s[4], s[5]); break;

	/* color */
	case B('C','S'): pdf_process_CS(ctx, proc, csi, 1); break;
	case B('c','s'): pdf_process_CS(ctx, proc, csi, 0); break;
	case B('S','C'): pdf_process_SC(ctx, proc, csi, 1); break;
	case B('s','c'): pdf_process_SC(ctx, proc, csi, 0); break;
	case C('S','C','N'): pdf_process_SC(ctx, proc, csi, 1); break;
	case C('s','c','n'): pdf_process_SC(ctx, proc, csi, 0); break;

	case A('G'): if (proc->op_G) proc->op_G(ctx, proc, s[0]); break;
	case A('g'): if (proc->op_g) proc->op_g(ctx, proc, s[0]); break;
	case B('R','G'): if (proc->op_RG) proc->op_RG(ctx, proc, s[0], s[1], s[2]); break;
	case B('r','g'): if (proc->op_rg) proc->op_rg(ctx, proc, s[0], s[1], s[2]); break;
	case A('K'): if (proc->op_K) proc->op_K(ctx, proc, s[0], s[1], s[2], s[3]); break;
	case A('k'): if (proc->op_k) proc->op_k(ctx, proc, s[0], s[1], s[2], s[3]); break;

	/* shadings, images, xobjects */
	case B('B','I'):
		{
			fz_image *img = parse_inline_image(ctx, proc, csi, stm, csname, sizeof csname);
			fz_try(ctx)
			{
				if (proc->op_BI)
					proc->op_BI(ctx, proc, img, csname[0] ? csname : NULL);
			}
			fz_always(ctx)
				fz_drop_image(ctx, img);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		break;

	case B('s','h'):
		if (proc->op_sh)
		{
			pdf_obj *shadeobj;
			fz_shade *shade;
			shadeobj = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Shading), csi->name);
			if (!shadeobj)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot find Shading resource '%s'", csi->name);
			shade = pdf_load_shading(ctx, csi->doc, shadeobj);
			fz_try(ctx)
				proc->op_sh(ctx, proc, csi->name, shade);
			fz_always(ctx)
				fz_drop_shade(ctx, shade);
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
		break;

	case B('D','o'): pdf_process_Do(ctx, proc, csi); break;

	/* marked content */
	case B('M','P'): if (proc->op_MP) proc->op_MP(ctx, proc, csi->name); break;
	case B('D','P'): if (proc->op_DP) proc->op_DP(ctx, proc, csi->name, csi->obj, resolve_properties(ctx, proc, csi->obj)); break;
	case C('B','M','C'): pdf_process_BMC(ctx, proc, csi, csi->name); break;
	case C('B','D','C'): pdf_process_BDC(ctx, proc, csi); break;
	case C('E','M','C'): pdf_process_EMC(ctx, proc, csi); break;

	/* compatibility */
	case B('B','X'): ++csi->xbalance; if (proc->op_BX) proc->op_BX(ctx, proc); break;
	case B('E','X'): --csi->xbalance; if (proc->op_EX) proc->op_EX(ctx, proc); break;
	}
}

static void
pdf_process_stream(fz_context *ctx, pdf_processor *proc, pdf_csi *csi, fz_stream *stm)
{
	pdf_document *doc = csi->doc;
	pdf_lexbuf *buf = csi->buf;
	fz_cookie *cookie = csi->cookie;

	pdf_token tok = PDF_TOK_ERROR;
	int in_text_array = 0;
	int syntax_errors = 0;

	/* make sure we have a clean slate if we come here from flush_text */
	pdf_clear_stack(ctx, csi);

	fz_var(in_text_array);
	fz_var(tok);

	if (cookie)
	{
		cookie->progress_max = (size_t)-1;
		cookie->progress = 0;
	}

	do
	{
		fz_try(ctx)
		{
			do
			{
				/* Check the cookie */
				if (cookie)
				{
					if (cookie->abort)
					{
						tok = PDF_TOK_EOF;
						break;
					}
					cookie->progress++;
				}

				tok = pdf_lex(ctx, stm, buf);

				if (in_text_array)
				{
					switch(tok)
					{
					case PDF_TOK_CLOSE_ARRAY:
						in_text_array = 0;
						break;
					case PDF_TOK_REAL:
						pdf_array_push_real(ctx, csi->obj, buf->f);
						break;
					case PDF_TOK_INT:
						pdf_array_push_int(ctx, csi->obj, buf->i);
						break;
					case PDF_TOK_STRING:
						pdf_array_push_string(ctx, csi->obj, buf->scratch, buf->len);
						break;
					case PDF_TOK_EOF:
						break;
					case PDF_TOK_KEYWORD:
						if (buf->scratch[0] == 'T' && (buf->scratch[1] == 'w' || buf->scratch[1] == 'c') && buf->scratch[2] == 0)
						{
							int n = pdf_array_len(ctx, csi->obj);
							if (n > 0)
							{
								pdf_obj *o = pdf_array_get(ctx, csi->obj, n-1);
								if (pdf_is_number(ctx, o))
								{
									csi->stack[0] = pdf_to_real(ctx, o);
									pdf_array_delete(ctx, csi->obj, n-1);
									pdf_process_keyword(ctx, proc, csi, stm, buf->scratch);
								}
							}
						}
						/* Deliberate Fallthrough! */
					default:
						fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error in array");
					}
				}
				else switch (tok)
				{
				case PDF_TOK_ENDSTREAM:
				case PDF_TOK_EOF:
					tok = PDF_TOK_EOF;
					break;

				case PDF_TOK_OPEN_ARRAY:
					if (csi->obj)
					{
						pdf_drop_obj(ctx, csi->obj);
						csi->obj = NULL;
					}
					if (csi->in_text)
					{
						in_text_array = 1;
						csi->obj = pdf_new_array(ctx, doc, 4);
					}
					else
					{
						csi->obj = pdf_parse_array(ctx, doc, stm, buf);
					}
					break;

				case PDF_TOK_OPEN_DICT:
					if (csi->obj)
					{
						pdf_drop_obj(ctx, csi->obj);
						csi->obj = NULL;
					}
					csi->obj = pdf_parse_dict(ctx, doc, stm, buf);
					break;

				case PDF_TOK_NAME:
					if (csi->name[0])
					{
						pdf_drop_obj(ctx, csi->obj);
						csi->obj = NULL;
						csi->obj = pdf_new_name(ctx, buf->scratch);
					}
					else
						fz_strlcpy(csi->name, buf->scratch, sizeof(csi->name));
					break;

				case PDF_TOK_INT:
					if (csi->top < (int)nelem(csi->stack)) {
						csi->stack[csi->top] = buf->i;
						csi->top ++;
					}
					else
						fz_throw(ctx, FZ_ERROR_SYNTAX, "stack overflow");
					break;

				case PDF_TOK_REAL:
					if (csi->top < (int)nelem(csi->stack)) {
						csi->stack[csi->top] = buf->f;
						csi->top ++;
					}
					else
						fz_throw(ctx, FZ_ERROR_SYNTAX, "stack overflow");
					break;

				case PDF_TOK_STRING:
					if (buf->len <= sizeof(csi->string))
					{
						memcpy(csi->string, buf->scratch, buf->len);
						csi->string_len = buf->len;
					}
					else
					{
						if (csi->obj)
						{
							pdf_drop_obj(ctx, csi->obj);
							csi->obj = NULL;
						}
						csi->obj = pdf_new_string(ctx, buf->scratch, buf->len);
					}
					break;

				case PDF_TOK_KEYWORD:
					pdf_process_keyword(ctx, proc, csi, stm, buf->scratch);
					pdf_clear_stack(ctx, csi);
					break;

				default:
					fz_throw(ctx, FZ_ERROR_SYNTAX, "syntax error in content stream");
				}
			}
			while (tok != PDF_TOK_EOF);
		}
		fz_always(ctx)
		{
			pdf_clear_stack(ctx, csi);
		}
		fz_catch(ctx)
		{
			int caught = fz_caught(ctx);
			if (cookie)
			{
				if (caught == FZ_ERROR_TRYLATER)
				{
					fz_ignore_error(ctx);
					cookie->incomplete++;
					tok = PDF_TOK_EOF;
				}
				else if (caught == FZ_ERROR_ABORT)
				{
					fz_rethrow(ctx);
				}
				else if (caught == FZ_ERROR_SYNTAX)
				{
					fz_report_error(ctx);
					cookie->errors++;
					if (++syntax_errors >= MAX_SYNTAX_ERRORS)
					{
						fz_warn(ctx, "too many syntax errors; ignoring rest of page");
						tok = PDF_TOK_EOF;
					}
				}
				else
				{
					fz_rethrow(ctx);
				}
			}
			else
			{
				if (caught == FZ_ERROR_TRYLATER)
				{
					fz_ignore_error(ctx);
					tok = PDF_TOK_EOF;
				}
				else if (caught == FZ_ERROR_ABORT)
				{
					fz_rethrow(ctx);
				}
				else if (caught == FZ_ERROR_SYNTAX)
				{
					fz_report_error(ctx);
					if (++syntax_errors >= MAX_SYNTAX_ERRORS)
					{
						fz_warn(ctx, "too many syntax errors; ignoring rest of page");
						tok = PDF_TOK_EOF;
					}
				}
				else
				{
					fz_rethrow(ctx);
				}
			}

			/* If we do catch an error, then reset ourselves to a base lexing state */
			in_text_array = 0;
		}
	}
	while (tok != PDF_TOK_EOF);

	if (syntax_errors > 0)
		fz_warn(ctx, "encountered syntax errors; page may not be correct");
}

void pdf_processor_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_resource_stack *stk = fz_malloc_struct(ctx, pdf_resource_stack);
	stk->next = proc->rstack;
	proc->rstack = stk;
	stk->resources = pdf_keep_obj(ctx, res);

	if (proc->push_resources)
	proc->push_resources(ctx, proc, res);
}

pdf_obj *pdf_processor_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_resource_stack *stk = proc->rstack;
	pdf_obj *res = NULL;
	pdf_obj *out_res = NULL;

	if (stk)
	{
		res = stk->resources;
		proc->rstack = stk->next;
		fz_free(ctx, stk);
	}

	if (proc->pop_resources == pdf_default_pop_resources && proc->chain == NULL)
	{
		/* It only makes sense to call the default pop resources if we
		 * have a chained processor. Otherwise just return what we have
		 * here. */
		return res;
	}
	if (proc->pop_resources)
	{
		out_res = proc->pop_resources(ctx, proc);
		pdf_drop_obj(ctx, res);
		return out_res;
	}

	return res;
}

void
pdf_process_raw_contents(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *stmobj, fz_cookie *cookie)
{
	pdf_csi csi;
	pdf_lexbuf buf;
	fz_stream *stm = NULL;

	if (!stmobj)
		return;

	fz_var(stm);

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);
	pdf_init_csi(ctx, &csi, doc, &buf, cookie);

	fz_try(ctx)
	{
		fz_defer_reap_start(ctx);
		stm = pdf_open_contents_stream(ctx, doc, stmobj);
		pdf_process_stream(ctx, proc, &csi, stm);
		pdf_process_end(ctx, proc, &csi);
	}
	fz_always(ctx)
	{
		fz_defer_reap_end(ctx);
		fz_drop_stream(ctx, stm);
		pdf_clear_stack(ctx, &csi);
		pdf_lexbuf_fin(ctx, &buf);
	}
	fz_catch(ctx)
	{
		proc->close_processor = NULL; /* aborted run, don't warn about unclosed processor */
		fz_rethrow(ctx);
	}
}

void
pdf_process_contents(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *in_res, pdf_obj *stmobj, fz_cookie *cookie, pdf_obj **out_res)
{
	if (in_res)
		pdf_processor_push_resources(ctx, proc, in_res);
	fz_try(ctx)
		pdf_process_raw_contents(ctx, proc, doc, stmobj, cookie);
	fz_always(ctx)
	{
		if (in_res)
		{
			pdf_obj *ret_res = pdf_processor_pop_resources(ctx, proc);
		if (out_res)
				*out_res = ret_res;
		else
				pdf_drop_obj(ctx, ret_res);
		}
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* Bug 702543: It looks like certain types of annotation are never
 * printed. */
static int
pdf_should_print_annot(fz_context *ctx, pdf_annot *annot)
{
	enum pdf_annot_type type = pdf_annot_type(ctx, annot);

	/* We may need to add more types here. */
	if (type == PDF_ANNOT_FILE_ATTACHMENT)
		return 0;

	return 1;
}

void
pdf_process_annot(fz_context *ctx, pdf_processor *proc, pdf_annot *annot, fz_cookie *cookie)
{
	int flags = pdf_annot_flags(ctx, annot);
	fz_matrix matrix;
	pdf_obj *ap;

	if (flags & (PDF_ANNOT_IS_INVISIBLE | PDF_ANNOT_IS_HIDDEN) || annot->hidden_editing)
		return;

	/* popup annotations should never be drawn */
	if (pdf_annot_type(ctx, annot) == PDF_ANNOT_POPUP)
		return;

	if (proc->usage)
	{
		if (!strcmp(proc->usage, "Print"))
		{
			if (!(flags & PDF_ANNOT_IS_PRINT))
				return;
			if (!pdf_should_print_annot(ctx, annot))
				return;
		}
		if (!strcmp(proc->usage, "View") && (flags & PDF_ANNOT_IS_NO_VIEW))
			return;
	}

	/* XXX what resources, if any, to use for this check? */
	if (pdf_is_ocg_hidden(ctx, annot->page->doc, NULL, proc->usage, pdf_dict_get(ctx, annot->obj, PDF_NAME(OC))))
		return;

	ap = pdf_annot_ap(ctx, annot);

	if (!ap)
		return;

	matrix = pdf_annot_transform(ctx, annot);
	if (proc->op_q)
		proc->op_q(ctx, proc);
	if (proc->op_cm)
		proc->op_cm(ctx, proc,
			matrix.a, matrix.b,
			matrix.c, matrix.d,
			matrix.e, matrix.f);
	if (proc->op_Do_form)
		proc->op_Do_form(ctx, proc, NULL, ap);
	if (proc->op_Q)
		proc->op_Q(ctx, proc);
}

void
pdf_process_glyph(fz_context *ctx, pdf_processor *proc, pdf_document *doc, pdf_obj *res, fz_buffer *contents)
{
	pdf_csi csi;
	pdf_lexbuf buf;
	fz_stream *stm = NULL;

	fz_var(stm);

	if (!contents)
		return;

	pdf_lexbuf_init(ctx, &buf, PDF_LEXBUF_SMALL);
	pdf_init_csi(ctx, &csi, doc, &buf, NULL);

	fz_try(ctx)
	{
		if (res)
			pdf_processor_push_resources(ctx, proc, res);
		stm = fz_open_buffer(ctx, contents);
		pdf_process_stream(ctx, proc, &csi, stm);
		pdf_process_end(ctx, proc, &csi);
	}
	fz_always(ctx)
	{
		if (res)
		pdf_drop_obj(ctx, pdf_processor_pop_resources(ctx, proc));
		fz_drop_stream(ctx, stm);
		pdf_clear_stack(ctx, &csi);
		pdf_lexbuf_fin(ctx, &buf);
	}
	fz_catch(ctx)
	{
		/* Note: Any SYNTAX errors should have been swallowed
		 * by pdf_process_stream, but in case any escape from other
		 * functions, recast the error type here to be safe. */
		fz_morph_error(ctx, FZ_ERROR_SYNTAX, FZ_ERROR_FORMAT);
		fz_rethrow(ctx);
	}
}

void
pdf_tos_save(fz_context *ctx, pdf_text_object_state *tos, fz_matrix save[2])
{
	save[0] = tos->tm;
	save[1] = tos->tlm;
}

void
pdf_tos_restore(fz_context *ctx, pdf_text_object_state *tos, fz_matrix save[2])
{
	tos->tm = save[0];
	tos->tlm = save[1];
}

fz_text *
pdf_tos_get_text(fz_context *ctx, pdf_text_object_state *tos)
{
	fz_text *text = tos->text;

	tos->text = NULL;

	return text;
}

void
pdf_tos_reset(fz_context *ctx, pdf_text_object_state *tos, int render)
{
	tos->text = fz_new_text(ctx);
	tos->text_mode = render;
	tos->text_bbox = fz_empty_rect;
}

int
pdf_tos_make_trm(fz_context *ctx, pdf_text_object_state *tos, pdf_text_state *text, pdf_font_desc *fontdesc, int cid, fz_matrix *trm, float *adv)
{
	fz_matrix tsm;

	tsm.a = text->size * text->scale;
	tsm.b = 0;
	tsm.c = 0;
	tsm.d = text->size;
	tsm.e = 0;
	tsm.f = text->rise;

	if (fontdesc->wmode == 0)
	{
		pdf_hmtx h = pdf_lookup_hmtx(ctx, fontdesc, cid);
		float w0 = *adv = h.w * 0.001f;
		tos->char_tx = (w0 * text->size + text->char_space) * text->scale;
		tos->char_ty = 0;
	}
	else
	{
		pdf_vmtx v = pdf_lookup_vmtx(ctx, fontdesc, cid);
		float w1 = *adv = v.w * 0.001f;
		tsm.e -= v.x * fabsf(text->size) * 0.001f;
		tsm.f -= v.y * text->size * 0.001f;
		tos->char_tx = 0;
		tos->char_ty = w1 * text->size + text->char_space;
	}

	*trm = fz_concat(tsm, tos->tm);

	tos->cid = cid;
	tos->gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);
	tos->fontdesc = fontdesc;

	/* Compensate for the glyph cache limited positioning precision */
	tos->char_bbox = fz_expand_rect(fz_bound_glyph(ctx, fontdesc->font, tos->gid, *trm), 1);

	return tos->gid;
}

void
pdf_tos_move_after_char(fz_context *ctx, pdf_text_object_state *tos)
{
	tos->text_bbox = fz_union_rect(tos->text_bbox, tos->char_bbox);
	tos->tm = fz_pre_translate(tos->tm, tos->char_tx, tos->char_ty);
}

void
pdf_tos_translate(pdf_text_object_state *tos, float tx, float ty)
{
	tos->tlm = fz_pre_translate(tos->tlm, tx, ty);
	tos->tm = tos->tlm;
}

void
pdf_tos_set_matrix(pdf_text_object_state *tos, float a, float b, float c, float d, float e, float f)
{
	tos->tm.a = a;
	tos->tm.b = b;
	tos->tm.c = c;
	tos->tm.d = d;
	tos->tm.e = e;
	tos->tm.f = f;
	tos->tlm = tos->tm;
}

void
pdf_tos_newline(pdf_text_object_state *tos, float leading)
{
	tos->tlm = fz_pre_translate(tos->tlm, 0, -leading);
	tos->tm = tos->tlm;
}
