// Copyright (C) 2025 Artifex Software, Inc.
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

#include "pdf-imp.h"

#include <string.h>

typedef struct vectorize_gstate
{
	struct vectorize_gstate *next;
	pdf_text_state text;
} vectorize_gstate;

typedef struct
{
	pdf_processor super;
	pdf_document *doc;
	vectorize_gstate *gstate;
	pdf_text_object_state tos;
	int bidi;
	pdf_vectorize_filter_options *options;
	pdf_filter_options *global_options;
	pdf_resource_stack *new_rstack;
} pdf_vectorize_processor;

static void
send_moveto(fz_context *ctx, void *arg, float x, float y)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_m)
		p->super.chain->op_m(ctx, p->super.chain, x, y);
}

static void
send_lineto(fz_context *ctx, void *arg, float x, float y)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_l)
		p->super.chain->op_l(ctx, p->super.chain, x, y);
}

static void
send_curveto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_c)
		p->super.chain->op_c(ctx, p->super.chain, x1, y1, x2, y2, x3, y3);
}

static void
send_closepath(fz_context *ctx, void *arg)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_h)
		p->super.chain->op_h(ctx, p->super.chain);
}

static void
send_curvetov(fz_context *ctx, void *arg, float x2, float y2, float x3, float y3)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_v)
		p->super.chain->op_v(ctx, p->super.chain, x2, y2, x3, y3);
}

static void
send_curvetoy(fz_context *ctx, void *arg, float x1, float y1, float x3, float y3)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_y)
		p->super.chain->op_y(ctx, p->super.chain, x1, y1, x3, y3);
}

static void
send_rectto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)arg;

	if (p->super.chain->op_re)
		p->super.chain->op_re(ctx, p->super.chain, x1, y1, x2, y2);
}

static void
send_path(fz_context *ctx, pdf_vectorize_processor *pr, fz_path *path)
{
	static const fz_path_walker sender =
	{
		/* Compulsory ones */
		send_moveto,
		send_lineto,
		send_curveto,
		send_closepath,
		NULL,
		send_curvetov,
		send_curvetoy,
		send_rectto
	};

	fz_walk_path(ctx, path, &sender, pr);
}

static void
vectorize_push(fz_context *ctx, pdf_vectorize_processor *p)
{
	vectorize_gstate *gnew = fz_malloc_struct(ctx, vectorize_gstate);

	gnew->next = p->gstate;
	gnew->text = p->gstate->text;
	p->gstate = gnew;
	if (gnew->text.font)
		pdf_keep_font(ctx, gnew->text.font);
	if (gnew->text.fontname)
		fz_keep_string(ctx, gnew->text.fontname);

	if (p->super.chain->op_q)
		p->super.chain->op_q(ctx, p->super.chain);
}

static int
vectorize_pop(fz_context *ctx, pdf_vectorize_processor *p)
{
	vectorize_gstate *gstate = p->gstate;
	vectorize_gstate *old = gstate->next;

	/* We are at the top, so nothing to pop! */
	if (old == NULL)
		return 1;

	if (p->super.chain->op_Q)
		p->super.chain->op_Q(ctx, p->super.chain);

	pdf_drop_font(ctx, gstate->text.font);
	fz_drop_string(ctx, gstate->text.fontname);
	fz_free(ctx, gstate);
	p->gstate = old;
	return 0;
}

static void
show_char(fz_context *ctx, pdf_vectorize_processor *pr, int cid)
{
	vectorize_gstate *gstate = pr->gstate;
	pdf_font_desc *fontdesc = gstate->text.font;
	fz_matrix trm;
	float adv;
	int gid;
	fz_path *path;

	if (gstate->text.render == 3)
		return;

	gid = pdf_tos_make_trm(ctx, &pr->tos, &gstate->text, fontdesc, cid, &trm, &adv);

	if (gid < 0 || gid >= fontdesc->font->glyph_count)
		return;

	if (fontdesc->font->t3procs != NULL)
	{
		/* Type 3 font */
		/* PDF spec: ISO 32000-2 latest version at the time of writing:
		 * Section 9.3.6:
		 * Where text is drawn using a Type 3 font:
		 *  + if text rendering mode is set to a value of 3 or 7, the text shall not be rendered.
		 *  + if text rendering mode is set to a value other than 3 or 7, the text shall be rendered using the glyph descriptions in the Type 3 font.
		 *  + If text rendering mode is set to a value of 4, 5, 6 or 7, nothing shall be added to the clipping path.
		 */
		if ((gstate->text.render & 3) != 3)
		{
			fz_matrix tfm = fz_concat(fontdesc->font->t3matrix, trm);
			vectorize_push(ctx, pr);
			if (pr->super.chain->op_cm)
				pr->super.chain->op_cm(ctx, pr->super.chain, tfm.a, tfm.b, tfm.c, tfm.d, tfm.e, tfm.f);
			pdf_process_glyph(ctx, &pr->super, fontdesc->font->t3doc, pr->super.rstack->resources, fontdesc->font->t3procs[gid]);
			vectorize_pop(ctx, pr);
		}
	}
	else
	{
		path = fz_outline_glyph(ctx, fontdesc->font, gid, trm);
		if (fz_path_is_empty(ctx, path))
		{
			if (!path)
				fz_warn(ctx, "cannot render glyph");
		}
		else
		{
			if (gstate->text.render != 7)
			{
				if (pr->super.chain->op_q)
					pr->super.chain->op_q(ctx, pr->super.chain);
				send_path(ctx, pr, path);
				if ((gstate->text.render & 1) == 0)
					if (pr->super.chain->op_f)
						pr->super.chain->op_f(ctx, pr->super.chain);
				if (((gstate->text.render-1) & 2) == 0)
					if (pr->super.chain->op_S)
						pr->super.chain->op_S(ctx, pr->super.chain);
				if (pr->super.chain->op_Q)
					pr->super.chain->op_Q(ctx, pr->super.chain);
			}
			if (gstate->text.render > 3)
			{
				send_path(ctx, pr, path);
				if (pr->super.chain->op_W)
					pr->super.chain->op_W(ctx, pr->super.chain);
				if (pr->super.chain->op_n)
					pr->super.chain->op_n(ctx, pr->super.chain);
			}
		}
	}

	pdf_tos_move_after_char(ctx, &pr->tos);
}

static void
show_space(fz_context *ctx, pdf_vectorize_processor *pr, float tadj)
{
	vectorize_gstate *gstate = pr->gstate;
	pdf_font_desc *fontdesc = gstate->text.font;

	if (fontdesc->wmode == 0)
		pr->tos.tm = fz_pre_translate(pr->tos.tm, tadj * gstate->text.scale, 0);
	else
		pr->tos.tm = fz_pre_translate(pr->tos.tm, 0, tadj);
}

static void
do_show_string(fz_context *ctx, pdf_vectorize_processor *pr, unsigned char *buf, size_t len)
{
	vectorize_gstate *gstate = pr->gstate;
	pdf_font_desc *fontdesc = gstate->text.font;
	unsigned char *end = buf + len;
	unsigned int cpt;
	int cid;

	while (buf < end)
	{
		int w = pdf_decode_cmap(fontdesc->encoding, buf, end, &cpt);
		buf += w;

		cid = pdf_lookup_cmap(fontdesc->encoding, cpt);
		if (cid >= 0)
			show_char(ctx, pr, cid);
		else
			fz_warn(ctx, "cannot encode character");
		if (cpt == 32 && w == 1)
		{
			/* Bug 703151: pdf_show_char can realloc gstate. */
			gstate = pr->gstate;
			show_space(ctx, pr, gstate->text.word_space);
		}
	}
}

static void
show_string(fz_context *ctx, pdf_vectorize_processor *pr, unsigned char *buf, size_t len)
{
	vectorize_gstate *gstate = pr->gstate;
	pdf_font_desc *fontdesc = gstate->text.font;

	if (!fontdesc)
	{
		fz_warn(ctx, "cannot draw text since font and size not set");
		return;
	}

	do_show_string(ctx, pr, buf, len);
}

static void
show_text(fz_context *ctx, pdf_vectorize_processor *pr, pdf_obj *text)
{
	vectorize_gstate *gstate = pr->gstate;
	pdf_font_desc *fontdesc = gstate->text.font;
	int i;

	if (!fontdesc)
	{
		fz_warn(ctx, "cannot draw text since font and size not set");
		return;
	}

	if (pdf_is_array(ctx, text))
	{
		int n = pdf_array_len(ctx, text);
		for (i = 0; i < n; i++)
		{
			pdf_obj *item = pdf_array_get(ctx, text, i);
			if (pdf_is_string(ctx, item))
				do_show_string(ctx, pr, (unsigned char *)pdf_to_str_buf(ctx, item), pdf_to_str_len(ctx, item));
			else
			{
				gstate = pr->gstate;
				show_space(ctx, pr, - pdf_to_real(ctx, item) * gstate->text.size * 0.001f);
			}
		}
	}
	else if (pdf_is_string(ctx, text))
	{
		do_show_string(ctx, pr, (unsigned char *)pdf_to_str_buf(ctx, text), pdf_to_str_len(ctx, text));
	}
}

static void
pdf_vectorize_BT(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	p->tos.tm = fz_identity;
	p->tos.tlm = fz_identity;
	p->bidi = 0;
}

static void pdf_vectorize_q(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *pr = (pdf_vectorize_processor *)proc;

	vectorize_push(ctx, pr);
}


static void
pdf_vectorize_Q(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	vectorize_pop(ctx, p);
}

/* text state */

static void
pdf_vectorize_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.char_space = charspace;
}

static void
pdf_vectorize_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.word_space = wordspace;
}

static void
pdf_vectorize_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.scale = scale / 100;
}

static void
pdf_vectorize_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.leading = leading;
}

static void
pdf_vectorize_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	pdf_drop_font(ctx, gstate->text.font);
	gstate->text.font = pdf_keep_font(ctx, font);
	gstate->text.size = size;
}

static void
pdf_vectorize_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.render = render;
}

static void
pdf_vectorize_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.rise = rise;
}

/* text positioning */

static void
pdf_vectorize_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	pdf_tos_translate(&p->tos, tx, ty);
}

static void
pdf_vectorize_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	gstate->text.leading = -ty;
	pdf_tos_translate(&p->tos, tx, ty);
}

static void
pdf_vectorize_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	pdf_tos_set_matrix(&p->tos, a, b, c, d, e, f);
}

static void
pdf_vectorize_Tstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gstate = p->gstate;

	pdf_tos_newline(&p->tos, gstate->text.leading);
}

/* text showing */

static void
pdf_vectorize_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *obj)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	show_text(ctx, p, obj);
}

static void
pdf_vectorize_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	show_string(ctx, p, (unsigned char *)str, len);
}

static void
pdf_vectorize_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	show_string(ctx, p, (unsigned char*)str, len);
}

static void
pdf_vectorize_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;

	show_string(ctx, p, (unsigned char*)str, len);
}

static void
pdf_close_vectorize_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	while (!vectorize_pop(ctx, p))
	{
		/* Nothing to do in the loop, all work done above */
	}
	pdf_close_processor(ctx, p->super.chain);
}

static void
pdf_drop_vectorize_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor*)proc;
	vectorize_gstate *gs = p->gstate;
	while (gs)
	{
		vectorize_gstate *next = gs->next;
		pdf_drop_font(ctx, gs->text.font);
		fz_drop_string(ctx, gs->text.fontname);
		fz_free(ctx, gs);
		gs = next;
	}
	fz_drop_text(ctx, p->tos.text);
	pdf_drop_document(ctx, p->doc);
}

static void
pdf_vectorize_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)proc;
	pdf_resource_stack *stk = fz_malloc_struct(ctx, pdf_resource_stack);
	int i, n;

	stk->next = p->new_rstack;
	p->new_rstack = stk;
	fz_try(ctx)
	{
		n = pdf_dict_len(ctx, res);
		stk->resources = pdf_new_dict(ctx, p->doc, n);
		for (i = 0; i < n; i++)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, res, i);
			if (pdf_name_eq(ctx, key, PDF_NAME(Font)))
				continue;
			pdf_dict_put(ctx, stk->resources, key, pdf_dict_get_val(ctx, res, i));
		}
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
pdf_vectorize_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_vectorize_processor *p = (pdf_vectorize_processor *)proc;
	pdf_resource_stack *stk = p->new_rstack;

	p->new_rstack = stk->next;
	pdf_drop_obj(ctx, stk->resources);
	fz_free(ctx, stk);

	return pdf_processor_pop_resources(ctx, p->super.chain);
}

pdf_processor *
pdf_new_vectorize_filter(
	fz_context *ctx,
	pdf_document *doc,
	pdf_processor *chain,
	int structparents,
	fz_matrix transform,
	pdf_filter_options *options,
	void *vopts_)
{
	pdf_vectorize_processor *proc = pdf_new_processor(ctx, sizeof *proc);
	pdf_vectorize_filter_options *vopts = vopts_;

	proc->super.close_processor = pdf_close_vectorize_processor;
	proc->super.drop_processor = pdf_drop_vectorize_processor;
	proc->super.push_resources = pdf_vectorize_push_resources;
	proc->super.pop_resources = pdf_vectorize_pop_resources;

	proc->super.op_q = pdf_vectorize_q;
	proc->super.op_Q = pdf_vectorize_Q;

	/* text objects */
	proc->super.op_BT = pdf_vectorize_BT;
	proc->super.op_ET = NULL;
	proc->super.op_d0 = NULL;
	proc->super.op_d1 = NULL;

	/* text state */
	proc->super.op_Tc = pdf_vectorize_Tc;
	proc->super.op_Tw = pdf_vectorize_Tw;
	proc->super.op_Tz = pdf_vectorize_Tz;
	proc->super.op_TL = pdf_vectorize_TL;
	proc->super.op_Tf = pdf_vectorize_Tf;
	proc->super.op_Tr = pdf_vectorize_Tr;
	proc->super.op_Ts = pdf_vectorize_Ts;

	/* text positioning */
	proc->super.op_Td = pdf_vectorize_Td;
	proc->super.op_TD = pdf_vectorize_TD;
	proc->super.op_Tm = pdf_vectorize_Tm;
	proc->super.op_Tstar = pdf_vectorize_Tstar;

	/* text showing */
	proc->super.op_TJ = pdf_vectorize_TJ;
	proc->super.op_Tj = pdf_vectorize_Tj;
	proc->super.op_squote = pdf_vectorize_squote;
	proc->super.op_dquote = pdf_vectorize_dquote;

	proc->doc = pdf_keep_document(ctx, doc);
	proc->super.chain = chain;

	proc->global_options = options;
	proc->options = vopts;

	fz_try(ctx)
	{
		proc->gstate = fz_malloc_struct(ctx, vectorize_gstate);
		proc->gstate->text.scale = 1;
		proc->gstate->text.size = -1;
	}
	fz_catch(ctx)
	{
		pdf_drop_processor(ctx, (pdf_processor *) proc);
		fz_rethrow(ctx);
	}

	proc->super.requirements = proc->super.chain->requirements;

	return (pdf_processor*)proc;
}
