// Copyright (C) 2004-2021 Artifex Software, Inc.
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

typedef enum
{
	FLUSH_CTM = 1,
	FLUSH_COLOR_F = 2,
	FLUSH_COLOR_S = 4,
	FLUSH_TEXT = 8,

	FLUSH_ALL = 15,
	FLUSH_STROKE = 1+4,
	FLUSH_FILL = 1+2
} gstate_flush_flags;

typedef struct pdf_filter_gstate_sc
{
	char name[256];
	pdf_pattern *pat;
	fz_shade *shd;
	int n;
	float c[FZ_MAX_COLORS];
} pdf_filter_gstate_sc;

typedef struct pdf_filter_gstate
{
	fz_matrix ctm;
	struct
	{
		char name[256];
		fz_colorspace *cs;
	} cs, CS;
	pdf_filter_gstate_sc sc, SC;
	struct
	{
		fz_linecap linecap;
		fz_linejoin linejoin;
		float linewidth;
		float miterlimit;
	} stroke;
	pdf_text_state text;
} pdf_filter_gstate;

typedef struct filter_gstate
{
	struct filter_gstate *next;
	int pushed;
	int empty_clip_region;
	pdf_filter_gstate pending;
	pdf_filter_gstate sent;
} filter_gstate;

typedef struct
{
	char *utf8;
	int edited;
	int pos;
} editable_str;

typedef struct tag_record
{
	int bdc;
	char *tag;
	pdf_obj *raw;
	pdf_obj *cooked;

	int mcid_num;
	pdf_obj *mcid_obj;
	editable_str alt;
	editable_str actualtext;

	struct tag_record *prev;
} tag_record;

typedef struct resources_stack
{
	struct resources_stack *next;
	pdf_obj *old_rdb;
	pdf_obj *new_rdb;
} resources_stack;

typedef struct
{
	pdf_processor super;
	pdf_document *doc;
	int structparents;
	pdf_obj *structarray;
	pdf_processor *chain;
	filter_gstate *gstate;
	pdf_text_object_state tos;
	/* If Td_pending, then any Tm_pending can be ignored and we can just
	 * send a Td with Td_value rather than the Tm. */
	int Td_pending;
	fz_point Td_value;
	int Tm_pending;
	int BT_pending;
	int in_BT;
	float Tm_adjust;
	void *font_name;
	tag_record *current_tags;
	tag_record *pending_tags;
	resources_stack *rstack;
	pdf_sanitize_filter_options *options;
	fz_matrix transform;
	/* Has any marking text been sent so far this text object? */
	int text_sent;
	/* Has any marking text been removed so far this text object? */
	int text_removed;
	pdf_filter_options *global_options;
	/* path is only constructed if we are using culling. */
	fz_path *path;
} pdf_sanitize_processor;

static void
copy_resource(fz_context *ctx, pdf_sanitize_processor *p, pdf_obj *key, const char *name)
{
	pdf_obj *res, *obj;

	if (!name || name[0] == 0)
		return;

	res = pdf_dict_get(ctx, p->rstack->old_rdb, key);
	obj = pdf_dict_gets(ctx, res, name);
	if (obj)
	{
		res = pdf_dict_get(ctx, p->rstack->new_rdb, key);
		if (!res)
		{
			res = pdf_new_dict(ctx, pdf_get_bound_document(ctx, p->rstack->new_rdb), 1);
			pdf_dict_put_drop(ctx, p->rstack->new_rdb, key, res);
		}
		pdf_dict_putp(ctx, res, name, obj);
	}
}

static void
add_resource(fz_context *ctx, pdf_sanitize_processor *p, pdf_obj *key, const char *name, pdf_obj *val)
{
	pdf_obj *res = pdf_dict_get(ctx, p->rstack->new_rdb, key);
	if (!res)
		res = pdf_dict_put_dict(ctx, p->rstack->new_rdb, key, 8);
	pdf_dict_puts(ctx, res, name, val);
}

static void
create_resource_name(fz_context *ctx, pdf_sanitize_processor *p, pdf_obj *key, const char *prefix, char *buf, int len)
{
	int i;
	pdf_obj *res = pdf_dict_get(ctx, p->rstack->new_rdb, key);
	if (!res)
		res = pdf_dict_put_dict(ctx, p->rstack->new_rdb, key, 8);
	for (i = 1; i < 65536; ++i)
	{
		fz_snprintf(buf, len, "%s%d", prefix, i);
		if (!pdf_dict_gets(ctx, res, buf))
			return;
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot create unique resource name");
}

static void
filter_push(fz_context *ctx, pdf_sanitize_processor *p)
{
	filter_gstate *gstate = p->gstate;
	filter_gstate *new_gstate = fz_malloc_struct(ctx, filter_gstate);
	*new_gstate = *gstate;
	new_gstate->pushed = 0;
	new_gstate->next = gstate;
	p->gstate = new_gstate;

	pdf_keep_font(ctx, new_gstate->pending.text.font);
	pdf_keep_font(ctx, new_gstate->sent.text.font);
}

static int
filter_pop(fz_context *ctx, pdf_sanitize_processor *p)
{
	filter_gstate *gstate = p->gstate;
	filter_gstate *old = gstate->next;

	/* We are at the top, so nothing to pop! */
	if (old == NULL)
		return 1;

	if (gstate->pushed)
		if (p->chain->op_Q)
			p->chain->op_Q(ctx, p->chain);

	pdf_drop_font(ctx, gstate->pending.text.font);
	pdf_drop_font(ctx, gstate->sent.text.font);
	fz_free(ctx, gstate);
	p->gstate = old;
	return 0;
}

/* We never allow the topmost gstate to be changed. This allows us
 * to pop back to the zeroth level and be sure that our gstate is
 * sane. This is important for being able to add new operators at
 * the end of pages in a sane way. */
static filter_gstate *
gstate_to_update(fz_context *ctx, pdf_sanitize_processor *p)
{
	filter_gstate *gstate = p->gstate;

	/* If we're not the top, that's fine */
	if (gstate->next != NULL)
		return gstate;

	/* We are the top. Push a group, so we're not */
	filter_push(ctx, p);
	gstate = p->gstate;
	gstate->pushed = 1;
	if (p->chain->op_q)
		p->chain->op_q(ctx, p->chain);

	return p->gstate;
}

static void flush_tags(fz_context *ctx, pdf_sanitize_processor *p, tag_record **tags)
{
	tag_record *tag = *tags;

	if (tag == NULL)
		return;
	if (tag->prev)
		flush_tags(ctx, p, &tag->prev);
	if (tag->bdc)
	{
		if (p->chain->op_BDC)
			p->chain->op_BDC(ctx, p->chain, tag->tag, tag->raw, tag->cooked);
	}
	else if (p->chain->op_BMC)
		p->chain->op_BMC(ctx, p->chain, tag->tag);
	tag->prev = p->current_tags;
	p->current_tags = tag;
	*tags = NULL;
}

static void filter_flush(fz_context *ctx, pdf_sanitize_processor *p, int flush)
{
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;

	/* No point in sending anything if we're clipping it away! */
	if (p->gstate->empty_clip_region)
		return;

	if (gstate->pushed == 0)
	{
		gstate->pushed = 1;
		if (p->chain->op_q)
			p->chain->op_q(ctx, p->chain);
	}

	if (flush)
		flush_tags(ctx, p, &p->pending_tags);

	if (flush & FLUSH_CTM)
	{
		if (gstate->pending.ctm.a != 1 || gstate->pending.ctm.b != 0 ||
			gstate->pending.ctm.c != 0 || gstate->pending.ctm.d != 1 ||
			gstate->pending.ctm.e != 0 || gstate->pending.ctm.f != 0)
		{
			fz_matrix current = gstate->sent.ctm;

			if (p->chain->op_cm)
				p->chain->op_cm(ctx, p->chain,
					gstate->pending.ctm.a,
					gstate->pending.ctm.b,
					gstate->pending.ctm.c,
					gstate->pending.ctm.d,
					gstate->pending.ctm.e,
					gstate->pending.ctm.f);

			gstate->sent.ctm = fz_concat(gstate->pending.ctm, current);
			gstate->pending.ctm.a = 1;
			gstate->pending.ctm.b = 0;
			gstate->pending.ctm.c = 0;
			gstate->pending.ctm.d = 1;
			gstate->pending.ctm.e = 0;
			gstate->pending.ctm.f = 0;
		}
	}

	if (flush & FLUSH_COLOR_F)
	{
		if (gstate->pending.cs.cs == fz_device_gray(ctx) && !gstate->pending.sc.pat && !gstate->pending.sc.shd && gstate->pending.sc.n == 1 &&
			(gstate->sent.cs.cs != fz_device_gray(ctx) || gstate->sent.sc.pat || gstate->sent.sc.shd || gstate->sent.sc.n != 1 || gstate->pending.sc.c[0] != gstate->sent.sc.c[0]))
		{
			if (p->chain->op_g)
				p->chain->op_g(ctx, p->chain, gstate->pending.sc.c[0]);
			goto done_sc;
		}
		if (gstate->pending.cs.cs == fz_device_rgb(ctx) && !gstate->pending.sc.pat && !gstate->pending.sc.shd && gstate->pending.sc.n == 3 &&
			(gstate->sent.cs.cs != fz_device_rgb(ctx) || gstate->sent.sc.pat || gstate->sent.sc.shd || gstate->sent.sc.n != 3 || gstate->pending.sc.c[0] != gstate->sent.sc.c[0] ||
				gstate->pending.sc.c[1] != gstate->sent.sc.c[1] || gstate->pending.sc.c[1] != gstate->sent.sc.c[1]))
		{
			if (p->chain->op_rg)
				p->chain->op_rg(ctx, p->chain, gstate->pending.sc.c[0], gstate->pending.sc.c[1], gstate->pending.sc.c[2]);
			goto done_sc;
		}
		if (gstate->pending.cs.cs == fz_device_cmyk(ctx) && !gstate->pending.sc.pat && !gstate->pending.sc.shd && gstate->pending.sc.n == 4 &&
			(gstate->sent.cs.cs != fz_device_cmyk(ctx) || gstate->sent.sc.pat || gstate->sent.sc.shd || gstate->pending.sc.n != 4 || gstate->pending.sc.c[0] != gstate->sent.sc.c[0] ||
				gstate->pending.sc.c[1] != gstate->sent.sc.c[1] || gstate->pending.sc.c[2] != gstate->sent.sc.c[2] || gstate->pending.sc.c[3] != gstate->sent.sc.c[3]))
		{
			if (p->chain->op_k)
				p->chain->op_k(ctx, p->chain, gstate->pending.sc.c[0], gstate->pending.sc.c[1], gstate->pending.sc.c[2], gstate->pending.sc.c[3]);
			goto done_sc;
		}

		if (strcmp(gstate->pending.cs.name, gstate->sent.cs.name))
		{
			if (p->chain->op_cs)
				p->chain->op_cs(ctx, p->chain, gstate->pending.cs.name, gstate->pending.cs.cs);
		}

		/* pattern or shading */
		if (gstate->pending.sc.name[0])
		{
			int emit = 0;
			if (strcmp(gstate->pending.sc.name, gstate->sent.sc.name))
				emit = 1;
			if (gstate->pending.sc.n != gstate->sent.sc.n)
				emit = 1;
			else
				for (i = 0; i < gstate->pending.sc.n; ++i)
					if (gstate->pending.sc.c[i] != gstate->sent.sc.c[i])
						emit = 1;
			if (emit)
			{
				if (gstate->pending.sc.pat)
					if (p->chain->op_sc_pattern)
						p->chain->op_sc_pattern(ctx, p->chain, gstate->pending.sc.name, gstate->pending.sc.pat, gstate->pending.sc.n, gstate->pending.sc.c);
				if (gstate->pending.sc.shd)
					if (p->chain->op_sc_shade)
						p->chain->op_sc_shade(ctx, p->chain, gstate->pending.sc.name, gstate->pending.sc.shd);
			}
		}

		/* plain color */
		else
		{
			int emit = 0;
			if (gstate->pending.sc.n != gstate->sent.sc.n)
				emit = 1;
			else
				for (i = 0; i < gstate->pending.sc.n; ++i)
					if (gstate->pending.sc.c[i] != gstate->sent.sc.c[i])
						emit = 1;
			if (emit)
			{
				if (p->chain->op_sc_color)
					p->chain->op_sc_color(ctx, p->chain, gstate->pending.sc.n, gstate->pending.sc.c);
			}
		}

done_sc:
		gstate->sent.cs = gstate->pending.cs;
		gstate->sent.sc = gstate->pending.sc;
	}

	if (flush & FLUSH_COLOR_S)
	{
		if (gstate->pending.CS.cs == fz_device_gray(ctx) && !gstate->pending.SC.pat && !gstate->pending.SC.shd && gstate->pending.SC.n == 1 &&
			(gstate->sent.CS.cs != fz_device_gray(ctx) || gstate->sent.SC.pat || gstate->sent.SC.shd || gstate->sent.SC.n != 0 || gstate->pending.SC.c[0] != gstate->sent.SC.c[0]))
		{
			if (p->chain->op_G)
				p->chain->op_G(ctx, p->chain, gstate->pending.SC.c[0]);
			goto done_SC;
		}
		if (gstate->pending.CS.cs == fz_device_rgb(ctx) && !gstate->pending.SC.pat && !gstate->pending.SC.shd && gstate->pending.SC.n == 3 &&
			(gstate->sent.CS.cs != fz_device_rgb(ctx) || gstate->sent.SC.pat || gstate->sent.SC.shd || gstate->sent.SC.n != 3 || gstate->pending.SC.c[0] != gstate->sent.SC.c[0] ||
				gstate->pending.SC.c[1] != gstate->sent.SC.c[1] || gstate->pending.SC.c[1] != gstate->sent.SC.c[1]))
		{
			if (p->chain->op_RG)
				p->chain->op_RG(ctx, p->chain, gstate->pending.SC.c[0], gstate->pending.SC.c[1], gstate->pending.SC.c[2]);
			goto done_SC;
		}
		if (gstate->pending.CS.cs == fz_device_cmyk(ctx) && !gstate->pending.SC.pat && !gstate->pending.SC.shd && gstate->pending.SC.n == 4 &&
			(gstate->sent.CS.cs != fz_device_cmyk(ctx) || gstate->sent.SC.pat || gstate->sent.SC.shd || gstate->pending.SC.n != 4 || gstate->pending.SC.c[0] != gstate->sent.SC.c[0] ||
				gstate->pending.SC.c[1] != gstate->sent.SC.c[1] || gstate->pending.SC.c[2] != gstate->sent.SC.c[2] || gstate->pending.SC.c[3] != gstate->sent.SC.c[3]))
		{
			if (p->chain->op_K)
				p->chain->op_K(ctx, p->chain, gstate->pending.SC.c[0], gstate->pending.SC.c[1], gstate->pending.SC.c[2], gstate->pending.SC.c[3]);
			goto done_SC;
		}

		if (strcmp(gstate->pending.CS.name, gstate->sent.CS.name))
		{
			if (p->chain->op_CS)
				p->chain->op_CS(ctx, p->chain, gstate->pending.CS.name, gstate->pending.CS.cs);
		}

		/* pattern or shading */
		if (gstate->pending.SC.name[0])
		{
			int emit = 0;
			if (strcmp(gstate->pending.SC.name, gstate->sent.SC.name))
				emit = 1;
			if (gstate->pending.SC.n != gstate->sent.SC.n)
				emit = 1;
			else
				for (i = 0; i < gstate->pending.SC.n; ++i)
					if (gstate->pending.SC.c[i] != gstate->sent.SC.c[i])
						emit = 1;
			if (emit)
			{
				if (gstate->pending.SC.pat)
					if (p->chain->op_SC_pattern)
						p->chain->op_SC_pattern(ctx, p->chain, gstate->pending.SC.name, gstate->pending.SC.pat, gstate->pending.SC.n, gstate->pending.SC.c);
				if (gstate->pending.SC.shd)
					if (p->chain->op_SC_shade)
						p->chain->op_SC_shade(ctx, p->chain, gstate->pending.SC.name, gstate->pending.SC.shd);
			}
		}

		/* plain color */
		else
		{
			int emit = 0;
			if (gstate->pending.SC.n != gstate->sent.SC.n)
				emit = 1;
			else
				for (i = 0; i < gstate->pending.SC.n; ++i)
					if (gstate->pending.SC.c[i] != gstate->sent.SC.c[i])
						emit = 1;
			if (emit)
			{
				if (p->chain->op_SC_color)
					p->chain->op_SC_color(ctx, p->chain, gstate->pending.SC.n, gstate->pending.SC.c);
			}
		}

done_SC:
		gstate->sent.CS = gstate->pending.CS;
		gstate->sent.SC = gstate->pending.SC;
	}

	if (flush & FLUSH_STROKE)
	{
		if (gstate->pending.stroke.linecap != gstate->sent.stroke.linecap)
		{
			if (p->chain->op_J)
				p->chain->op_J(ctx, p->chain, gstate->pending.stroke.linecap);
		}
		if (gstate->pending.stroke.linejoin != gstate->sent.stroke.linejoin)
		{
			if (p->chain->op_j)
				p->chain->op_j(ctx, p->chain, gstate->pending.stroke.linejoin);
		}
		if (gstate->pending.stroke.linewidth != gstate->sent.stroke.linewidth)
		{
			if (p->chain->op_w)
				p->chain->op_w(ctx, p->chain, gstate->pending.stroke.linewidth);
		}
		if (gstate->pending.stroke.miterlimit != gstate->sent.stroke.miterlimit)
		{
			if (p->chain->op_M)
				p->chain->op_M(ctx, p->chain, gstate->pending.stroke.miterlimit);
		}
		gstate->sent.stroke = gstate->pending.stroke;
	}

	if (flush & FLUSH_TEXT)
	{
		if (p->BT_pending)
		{
			if (p->chain->op_BT)
				p->chain->op_BT(ctx, p->chain);
			p->BT_pending = 0;
			p->in_BT = 1;
			p->text_sent = 0;
			p->text_removed = 0;
		}
		if (p->in_BT)
		{
			if (gstate->pending.text.char_space != gstate->sent.text.char_space)
			{
				if (p->chain->op_Tc)
					p->chain->op_Tc(ctx, p->chain, gstate->pending.text.char_space);
			}
			if (gstate->pending.text.word_space != gstate->sent.text.word_space)
			{
				if (p->chain->op_Tw)
					p->chain->op_Tw(ctx, p->chain, gstate->pending.text.word_space);
			}
			if (gstate->pending.text.scale != gstate->sent.text.scale)
			{
				/* The value of scale in the gstate is divided by 100 from what is written in the file */
				if (p->chain->op_Tz)
					p->chain->op_Tz(ctx, p->chain, gstate->pending.text.scale*100);
			}
			if (gstate->pending.text.leading != gstate->sent.text.leading)
			{
				if (p->chain->op_TL)
					p->chain->op_TL(ctx, p->chain, gstate->pending.text.leading);
			}
			if (gstate->pending.text.font != gstate->sent.text.font ||
				gstate->pending.text.size != gstate->sent.text.size)
			{
				if (p->chain->op_Tf)
					p->chain->op_Tf(ctx, p->chain, p->font_name, gstate->pending.text.font, gstate->pending.text.size);
			}
			if (gstate->pending.text.render != gstate->sent.text.render)
			{
				if (p->chain->op_Tr)
					p->chain->op_Tr(ctx, p->chain, gstate->pending.text.render);
			}
			if (gstate->pending.text.rise != gstate->sent.text.rise)
			{
				if (p->chain->op_Ts)
					p->chain->op_Ts(ctx, p->chain, gstate->pending.text.rise);
			}
			pdf_drop_font(ctx, gstate->sent.text.font);
			gstate->sent.text = gstate->pending.text;
			gstate->sent.text.font = pdf_keep_font(ctx, gstate->pending.text.font);

			if (p->Td_pending != 0)
			{
				if (p->chain->op_Td)
					p->chain->op_Td(ctx, p->chain, p->Td_value.x, p->Td_value.y);
				p->Tm_pending = 0;
				p->Td_pending = 0;
			}
			else if (p->Tm_pending != 0)
			{
				if (p->chain->op_Tm)
					p->chain->op_Tm(ctx, p->chain, p->tos.tlm.a, p->tos.tlm.b, p->tos.tlm.c, p->tos.tlm.d, p->tos.tlm.e, p->tos.tlm.f);
				p->Tm_pending = 0;
			}
		}
	}
}

static int
filter_show_char(fz_context *ctx, pdf_sanitize_processor *p, int cid, int *unicode)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;
	fz_matrix trm;
	int ucsbuf[PDF_MRANGE_CAP];
	int ucslen;
	int remove = 0;

	(void)pdf_tos_make_trm(ctx, &p->tos, &gstate->pending.text, fontdesc, cid, &trm);

	ucslen = 0;
	if (fontdesc->to_unicode)
		ucslen = pdf_lookup_cmap_full(fontdesc->to_unicode, cid, ucsbuf);
	if (ucslen == 0 && (size_t)cid < fontdesc->cid_to_ucs_len)
	{
		ucsbuf[0] = fontdesc->cid_to_ucs[cid];
		ucslen = 1;
	}
	if (ucslen == 0 || (ucslen == 1 && ucsbuf[0] == 0))
	{
		ucsbuf[0] = FZ_REPLACEMENT_CHARACTER;
		ucslen = 1;
	}
	*unicode = ucsbuf[0];

	if (p->options->text_filter || p->options->culler)
	{
		fz_matrix ctm;
		fz_rect bbox;

		ctm = fz_concat(gstate->pending.ctm, gstate->sent.ctm);
		ctm = fz_concat(ctm, p->transform);

		if (fontdesc->wmode == 0)
		{
			bbox.x0 = 0;
			bbox.y0 = fz_font_descender(ctx, fontdesc->font);
			bbox.x1 = fz_advance_glyph(ctx, fontdesc->font, p->tos.gid, 0);
			bbox.y1 = fz_font_ascender(ctx, fontdesc->font);
		}
		else
		{
			fz_rect font_bbox = fz_font_bbox(ctx, fontdesc->font);
			bbox.x0 = font_bbox.x0;
			bbox.x1 = font_bbox.x1;
			bbox.y0 = 0;
			bbox.y1 = fz_advance_glyph(ctx, fontdesc->font, p->tos.gid, 1);
		}

		if (p->options->text_filter)
			remove = p->options->text_filter(ctx, p->options->opaque, ucsbuf, ucslen, trm, ctm, bbox);
		if (p->options->culler && !remove)
		{
			ctm = fz_concat(trm, ctm);
			bbox = fz_transform_rect(bbox, ctm);
			remove = p->options->culler(ctx, p->options->opaque, bbox, FZ_CULL_GLYPH);
		}
	}

	pdf_tos_move_after_char(ctx, &p->tos);

	return remove;
}

static void
filter_show_space(fz_context *ctx, pdf_sanitize_processor *p, float tadj)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;

	if (fontdesc->wmode == 0)
		p->tos.tm = fz_pre_translate(p->tos.tm, tadj * gstate->pending.text.scale, 0);
	else
		p->tos.tm = fz_pre_translate(p->tos.tm, 0, tadj);
}

static void
walk_string(fz_context *ctx, int uni, int remove, editable_str *str)
{
	int rune;

	if (str->utf8 == NULL || str->pos == -1)
		return;

	do
	{
		char *s = &str->utf8[str->pos];
		size_t len;
		int n = fz_chartorune(&rune, s);
		if (rune == uni)
		{
			/* Match. Skip over that one. */
		}
		else if (uni == 32)
		{
			/* We don't care if we're given whitespace
			 * and it doesn't match the string. Don't
			 * skip forward. Nothing to remove. */
			break;
		}
		else if (rune == 32)
		{
			/* The string has a whitespace, and we
			 * don't match it; that's forgivable as
			 * PDF often misses out spaces. Remove this
			 * if we are removing stuff. */
		}
		else
		{
			/* Mismatch. No point in tracking through any more. */
			str->pos = -1;
			break;
		}
		if (remove)
		{
			len = strlen(s+n);
			memmove(s, s+n, len+1);
			str->edited = 1;
		}
		else
		{
			str->pos += n;
		}
	}
	while (rune != uni);
}

/* For a given character we've processed (removed or not)
 * consider it in the tag_record. Try and step over it in
 * the Alt or ActualText strings, removing if possible.
 * If we can't marry up the Alt/ActualText strings with
 * what we're meeting, just take the easy route and delete
 * the whole lot. */
static void
mcid_char_imp(fz_context *ctx, pdf_sanitize_processor *p, tag_record *tr, int uni, int remove)
{
	if (tr->mcid_obj == NULL)
		/* No object, or already deleted */
		return;

	if (remove)
	{
		/* Remove the expanded abbreviation, if there is one. */
		pdf_dict_del(ctx, tr->mcid_obj, PDF_NAME(E));
		/* Remove the structure title, if there is one. */
		pdf_dict_del(ctx, tr->mcid_obj, PDF_NAME(T));
	}

	/* Edit the Alt string */
	walk_string(ctx, uni, remove, &tr->alt);

	/* Edit the ActualText string */
	walk_string(ctx, uni, remove, &tr->actualtext);

	/* If we're removing a character, and either of the strings
	 * haven't matched up to what we were expecting, then just
	 * delete the whole string. */
	if (remove)
		remove = (tr->alt.pos == -1 || tr->actualtext.pos == -1);
	else if (tr->alt.pos >= 0 || tr->actualtext.pos >= 0)
	{
		/* The strings are making sense so far */
		remove = 0;
	}

	if (remove)
	{
		/* Anything else we have to err on the side of caution and
		 * delete everything that might leak info. */
		if (tr->actualtext.pos == -1)
			pdf_dict_del(ctx, tr->mcid_obj, PDF_NAME(ActualText));
		if (tr->alt.pos == -1)
			pdf_dict_del(ctx, tr->mcid_obj, PDF_NAME(Alt));
		pdf_drop_obj(ctx, tr->mcid_obj);
		tr->mcid_obj = NULL;
		fz_free(ctx, tr->alt.utf8);
		tr->alt.utf8 = NULL;
		fz_free(ctx, tr->actualtext.utf8);
		tr->actualtext.utf8 = NULL;
	}
}

/* For every character that is processed, consider that character in
 * every pending/current MCID. */
static void
mcid_char(fz_context *ctx, pdf_sanitize_processor *p, int uni, int remove)
{
	tag_record *tr  = p->pending_tags;

	for (tr = p->pending_tags; tr != NULL; tr = tr->prev)
		mcid_char_imp(ctx, p, tr, uni, remove);
	for (tr = p->current_tags; tr != NULL; tr = tr->prev)
		mcid_char_imp(ctx, p, tr, uni, remove);
}

static void
update_mcid(fz_context *ctx, pdf_sanitize_processor *p)
{
	tag_record *tag = p->current_tags;

	if (tag == NULL)
		return;
	if (tag->mcid_obj == NULL)
		return;
	if (tag->alt.edited)
		pdf_dict_put_text_string(ctx, tag->mcid_obj, PDF_NAME(Alt), tag->alt.utf8 ? tag->alt.utf8 : "");
	if (tag->actualtext.edited)
		pdf_dict_put_text_string(ctx, tag->mcid_obj, PDF_NAME(Alt), tag->actualtext.utf8 ? tag->actualtext.utf8 : "");
}

/* Process a string (from buf, of length len), from position *pos onwards.
 * Stop when we hit the end, or when we find a character to remove. The
 * caller will restart us again later. On exit, *pos = the point we got to,
 * *inc = The number of bytes to skip to step over the next character (unless
 * we hit the end).
 */
static void
filter_string_to_segment(fz_context *ctx, pdf_sanitize_processor *p, unsigned char *buf, size_t len, size_t *pos, int *inc, int *removed_space)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;
	unsigned char *end = buf + len;
	unsigned int cpt;
	int cid;
	int remove;

	buf += *pos;

	*removed_space = 0;

	while (buf < end)
	{
		int uni;
		*inc = pdf_decode_cmap(fontdesc->encoding, buf, end, &cpt);
		buf += *inc;

		cid = pdf_lookup_cmap(fontdesc->encoding, cpt);
		if (cid < 0)
		{
			uni = FZ_REPLACEMENT_CHARACTER;
			fz_warn(ctx, "cannot encode character");
			remove = 0;
		}
		else
			remove = filter_show_char(ctx, p, cid, &uni);

		/* FIXME: Should check for marking/non-marking! For
		 * now assume space is the only non-marking. */
		if (cpt != 32)
		{
			if (remove)
				p->text_removed = 1;
			else
				p->text_sent = 1;
		}

		if (cpt == 32 && *inc == 1)
			filter_show_space(ctx, p, gstate->pending.text.word_space);

		/* For every character we process (whether we remove it
		 * or not), we consider any MCIDs that are in effect. */
		mcid_char(ctx, p, uni, remove);
		if (remove)
		{
			*removed_space = (cpt == 32 && *inc == 1);
			return;
		}
		*pos += *inc;
	}
}

static void
adjust_text(fz_context *ctx, pdf_sanitize_processor *p, float x, float y)
{
	float skip_dist = p->tos.fontdesc->wmode == 1 ? -y : -x;
	skip_dist = skip_dist / p->gstate->pending.text.size;
	p->Tm_adjust += skip_dist;
}

static void
adjust_for_removed_space(fz_context *ctx, pdf_sanitize_processor *p)
{
	filter_gstate *gstate = p->gstate;
	float adj = gstate->pending.text.word_space;
	adjust_text(ctx, p, adj * gstate->pending.text.scale, adj);
}

static void
flush_adjustment(fz_context *ctx, pdf_sanitize_processor *p)
{
	pdf_obj *arr;

	if (p->Tm_adjust == 0)
		return;

	filter_flush(ctx, p, FLUSH_ALL);
	arr = pdf_new_array(ctx, p->doc, 1);
	fz_try(ctx)
	{
		pdf_array_push_real(ctx, arr, p->Tm_adjust * 1000);
		if (p->chain->op_TJ)
			p->chain->op_TJ(ctx, p->chain, arr);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, arr);
	fz_catch(ctx)
		fz_rethrow(ctx);

	p->Tm_adjust = 0;
}

static void
push_adjustment_to_array(fz_context *ctx, pdf_sanitize_processor *p, pdf_obj *arr)
{
	if (p->Tm_adjust == 0)
		return;
	pdf_array_push_real(ctx, arr, p->Tm_adjust * 1000);
	p->Tm_adjust = 0;
}

static void
filter_show_string(fz_context *ctx, pdf_sanitize_processor *p, unsigned char *buf, size_t len)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;
	int inc, removed_space;
	size_t i;

	if (!fontdesc)
		return;

	p->tos.fontdesc = fontdesc;
	i = 0;
	while (i < len)
	{
		size_t start = i;
		filter_string_to_segment(ctx, p, buf, len, &i, &inc, &removed_space);
		if (start != i)
		{
			/* We have *some* chars to send at least */
			filter_flush(ctx, p, FLUSH_ALL);
			flush_adjustment(ctx, p);
			if (p->chain->op_Tj)
				p->chain->op_Tj(ctx, p->chain, (char *)buf+start, i-start);
		}
		if (i != len)
		{
			adjust_text(ctx, p, p->tos.char_tx, p->tos.char_ty);
			i += inc;
		}
		if (removed_space)
			adjust_for_removed_space(ctx, p);
	}
}

static void
filter_show_text(fz_context *ctx, pdf_sanitize_processor *p, pdf_obj *text)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;
	int i, n;
	pdf_obj *new_arr;
	pdf_document *doc;

	if (!fontdesc)
		return;

	if (pdf_is_string(ctx, text))
	{
		filter_show_string(ctx, p, (unsigned char *)pdf_to_str_buf(ctx, text), pdf_to_str_len(ctx, text));
		return;
	}
	if (!pdf_is_array(ctx, text))
		return;

	p->tos.fontdesc = fontdesc;
	n = pdf_array_len(ctx, text);
	doc = pdf_get_bound_document(ctx, text);
	new_arr = pdf_new_array(ctx, doc, 4);
	fz_try(ctx)
	{
		for (i = 0; i < n; i++)
		{
			pdf_obj *item = pdf_array_get(ctx, text, i);
			if (pdf_is_string(ctx, item))
			{
				unsigned char *buf = (unsigned char *)pdf_to_str_buf(ctx, item);
				size_t len = pdf_to_str_len(ctx, item);
				size_t j = 0;
				int removed_space;
				while (j < len)
				{
					int inc;
					size_t start = j;
					filter_string_to_segment(ctx, p, buf, len, &j, &inc, &removed_space);
					if (start != j)
					{
						/* We have *some* chars to send at least */
						filter_flush(ctx, p, FLUSH_ALL);
						push_adjustment_to_array(ctx, p, new_arr);
						pdf_array_push_string(ctx, new_arr, (char *)buf+start, j-start);
					}
					if (j != len)
					{
						adjust_text(ctx, p, p->tos.char_tx, p->tos.char_ty);
						j += inc;
					}
					if (removed_space)
						adjust_for_removed_space(ctx, p);
				}
			}
			else
			{
				float tadj = - pdf_to_real(ctx, item) * gstate->pending.text.size * 0.001f;
				if (fontdesc->wmode == 0)
				{
					adjust_text(ctx, p, tadj, 0);
					p->tos.tm = fz_pre_translate(p->tos.tm, tadj * p->gstate->pending.text.scale, 0);
				}
				else
				{
					adjust_text(ctx, p, 0, tadj);
					p->tos.tm = fz_pre_translate(p->tos.tm, 0, tadj);
				}
			}
		}
		if (p->chain->op_TJ && pdf_array_len(ctx, new_arr))
			p->chain->op_TJ(ctx, p->chain, new_arr);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, new_arr);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* general graphics state */

static void
pdf_filter_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	gstate->pending.stroke.linewidth = linewidth;
}

static void
pdf_filter_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	gstate->pending.stroke.linejoin = linejoin;
}

static void
pdf_filter_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	gstate->pending.stroke.linecap = linecap;
}

static void
pdf_filter_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	gstate->pending.stroke.miterlimit = miterlimit;
}

static void
pdf_filter_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_d)
		p->chain->op_d(ctx, p->chain, array, phase);
}

static void
pdf_filter_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_ri)
		p->chain->op_ri(ctx, p->chain, intent);
}

static void
pdf_filter_gs_OP(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_OP)
		p->chain->op_gs_OP(ctx, p->chain, b);
}

static void
pdf_filter_gs_op(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_op)
		p->chain->op_gs_op(ctx, p->chain, b);
}

static void
pdf_filter_gs_OPM(fz_context *ctx, pdf_processor *proc, int i)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_OPM)
		p->chain->op_gs_OPM(ctx, p->chain, i);
}

static void
pdf_filter_gs_UseBlackPtComp(fz_context *ctx, pdf_processor *proc, pdf_obj *name)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_UseBlackPtComp)
		p->chain->op_gs_UseBlackPtComp(ctx, p->chain, name);
}

static void
pdf_filter_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_i)
		p->chain->op_i(ctx, p->chain, flatness);
}

static void
pdf_filter_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_gs_begin)
		p->chain->op_gs_begin(ctx, p->chain, name, extgstate);
	copy_resource(ctx, p, PDF_NAME(ExtGState), name);
}

static void
pdf_filter_gs_BM(fz_context *ctx, pdf_processor *proc, const char *blendmode)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->chain->op_gs_BM)
		p->chain->op_gs_BM(ctx, p->chain, blendmode);
}

static void
pdf_filter_gs_CA(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->chain->op_gs_CA)
		p->chain->op_gs_CA(ctx, p->chain, alpha);
}

static void
pdf_filter_gs_ca(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->chain->op_gs_ca)
		p->chain->op_gs_ca(ctx, p->chain, alpha);
}

static void
pdf_filter_gs_SMask(fz_context *ctx, pdf_processor *proc, pdf_obj *smask, float *bc, int luminosity)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->chain->op_gs_SMask)
		p->chain->op_gs_SMask(ctx, p->chain, smask, bc, luminosity);
}

static void
pdf_filter_gs_end(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->chain->op_gs_end)
		p->chain->op_gs_end(ctx, p->chain);
}

/* special graphics state */

static void
pdf_filter_q(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_push(ctx, p);
}

static void
pdf_filter_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	fz_matrix ctm;

	if (p->gstate->empty_clip_region)
		return;

	/* If we're being given an identity matrix, don't bother sending it */
	if (a == 1 && b == 0 && c == 0 && d == 1 && e == 0 && f == 0)
		return;

	ctm.a = a;
	ctm.b = b;
	ctm.c = c;
	ctm.d = d;
	ctm.e = e;
	ctm.f = f;

	gstate->pending.ctm = fz_concat(ctm, gstate->pending.ctm);
}

/* path construction */

static void
pdf_filter_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_moveto(ctx, p->path, x, y);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_m)
		p->chain->op_m(ctx, p->chain, x, y);
}

static void
pdf_filter_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_lineto(ctx, p->path, x, y);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_l)
		p->chain->op_l(ctx, p->chain, x, y);
}

static void
pdf_filter_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_curveto(ctx, p->path, x1, y1, x2, y2, x3, y3);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_c)
		p->chain->op_c(ctx, p->chain, x1, y1, x2, y2, x3, y3);
}

static void
pdf_filter_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_curvetov(ctx, p->path, x2, y2, x3, y3);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_v)
		p->chain->op_v(ctx, p->chain, x2, y2, x3, y3);
}

static void
pdf_filter_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_curvetoy(ctx, p->path, x1, y1, x3, y3);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_y)
		p->chain->op_y(ctx, p->chain, x1, y1, x3, y3);
}

static void
pdf_filter_h(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_closepath(ctx, p->path);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_h)
		p->chain->op_h(ctx, p->chain);
}

static void
pdf_filter_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_rectto(ctx, p->path, x, y, x+w, y+h);
		return;
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_re)
		p->chain->op_re(ctx, p->chain, x, y, w, h);
}

/* path painting */

static void
cull_replay_moveto(fz_context *ctx, void *arg, float x, float y)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_m)
		p->chain->op_m(ctx, p->chain, x, y);
}

static void cull_replay_lineto(fz_context *ctx, void *arg, float x, float y)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_l)
		p->chain->op_l(ctx, p->chain, x, y);
}

static void
cull_replay_curveto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_c)
		p->chain->op_c(ctx, p->chain, x1, y1, x2, y2, x3, y3);
}

static void
cull_replay_closepath(fz_context *ctx, void *arg)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_h)
		p->chain->op_h(ctx, p->chain);
}

static void
cull_replay_curvetov(fz_context *ctx, void *arg, float x2, float y2, float x3, float y3)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_v)
		p->chain->op_v(ctx, p->chain, x2, y2, x3, y3);
}

static void
cull_replay_curvetoy(fz_context *ctx, void *arg, float x1, float y1, float x3, float y3)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_y)
		p->chain->op_y(ctx, p->chain, x1, y1, x3, y3);
}

static void
cull_replay_rectto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)arg;

	if (p->chain->op_re)
		p->chain->op_re(ctx, p->chain, x1, y1, x2, y2);
}

typedef struct
{
	pdf_sanitize_processor *p;
	fz_stroke_state sstate;
	fz_path *segment;
	fz_matrix ctm;
	int any_sent;
	int type;
} segmenter_data_t;

static void
end_segment(fz_context *ctx, segmenter_data_t *sd)
{
	static const fz_path_walker walker = {
		cull_replay_moveto,
		cull_replay_lineto,
		cull_replay_curveto,
		cull_replay_closepath,
		NULL /* quad */,
		cull_replay_curvetov,
		cull_replay_curvetoy,
		cull_replay_rectto
	};
	fz_rect r;

	if (sd->segment == NULL)
		return;

	r = fz_bound_path(ctx, sd->segment, (sd->type == FZ_CULL_PATH_STROKE || sd->type == FZ_CULL_PATH_FILL_STROKE) ? &sd->sstate : NULL, sd->ctm);

	if (sd->p->options->culler(ctx, sd->p->options->opaque, r, sd->type))
	{
		/* This segment can be skipped */
	}
	else
	{
		/* Better send this segment then! */
		fz_walk_path(ctx, sd->segment, &walker, sd->p);
		sd->any_sent = 1;
	}
	fz_drop_path(ctx, sd->segment);
	sd->segment = NULL;
}

static void
cull_segmenter_moveto(fz_context *ctx, void *arg, float x, float y)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	end_segment(ctx, sd);
	sd->segment = fz_new_path(ctx);
	fz_moveto(ctx, sd->segment, x, y);
}

static void
cull_segmenter_lineto(fz_context *ctx, void *arg, float x, float y)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	fz_lineto(ctx, sd->segment, x, y);
}

static void
cull_segmenter_curveto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	fz_curveto(ctx, sd->segment, x1, y1, x2, y2, x3, y3);
}

static void
cull_segmenter_closepath(fz_context *ctx, void *arg)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	fz_closepath(ctx, sd->segment);
}

static void
cull_segmenter_curvetov(fz_context *ctx, void *arg, float x2, float y2, float x3, float y3)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	fz_curvetov(ctx, sd->segment, x2, y2, x3, y3);
}

static void
cull_segmenter_curvetoy(fz_context *ctx, void *arg, float x1, float y1, float x3, float y3)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	fz_curvetoy(ctx, sd->segment, x1, y1, x3, y3);
}

static void
cull_segmenter_rectto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2)
{
	segmenter_data_t *sd = (segmenter_data_t *)arg;

	end_segment(ctx, sd);
	sd->segment = fz_new_path(ctx);
	fz_rectto(ctx, sd->segment, x1, y1, x2, y2);
}


static int
cull_path(fz_context *ctx, pdf_sanitize_processor *p, int type)
{
	segmenter_data_t sd = { 0 };
	static const fz_path_walker segmenter = {
		cull_segmenter_moveto,
		cull_segmenter_lineto,
		cull_segmenter_curveto,
		cull_segmenter_closepath,
		NULL /* quad */,
		cull_segmenter_curvetov,
		cull_segmenter_curvetoy,
		cull_segmenter_rectto
	};

	if (p->options->culler == NULL)
		return 0;

	sd.ctm = fz_concat(p->gstate->pending.ctm, p->gstate->sent.ctm);
	sd.ctm = fz_concat(sd.ctm, p->transform);

	if (type == FZ_CULL_PATH_STROKE || type == FZ_CULL_PATH_FILL_STROKE)
	{
		sd.sstate.refs = -1;
		sd.sstate.start_cap = p->gstate->pending.stroke.linecap;
		sd.sstate.dash_cap = p->gstate->pending.stroke.linecap;
		sd.sstate.end_cap = p->gstate->pending.stroke.linecap;
		sd.sstate.linejoin = p->gstate->pending.stroke.linejoin;
		sd.sstate.linewidth = p->gstate->pending.stroke.linewidth;
		sd.sstate.miterlimit = p->gstate->pending.stroke.miterlimit;
		/* Ignore dash for now. */
		sd.sstate.dash_phase = 0;
		sd.sstate.dash_len = 0;
	}

	sd.p = p;
	sd.segment = NULL;
	sd.type = type;
	fz_try(ctx)
	{
		fz_walk_path(ctx, p->path, &segmenter, &sd);
		end_segment(ctx, &sd);
	}
	fz_always(ctx)
		fz_drop_path(ctx, sd.segment);
	fz_catch(ctx)
		fz_rethrow(ctx);

	fz_drop_path(ctx, p->path);
	p->path = NULL;
	p->path = fz_new_path(ctx);

	/* If none were sent, we can skip this path. */
	return (sd.any_sent == 0);
}

static void
pdf_filter_S(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_STROKE))
		return;

	filter_flush(ctx, p, FLUSH_STROKE);
	if (p->chain->op_S)
		p->chain->op_S(ctx, p->chain);
}

static void
pdf_filter_s(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_STROKE))
		return;

	filter_flush(ctx, p, FLUSH_STROKE);
	if (p->chain->op_s)
		p->chain->op_s(ctx, p->chain);
}

static void
pdf_filter_F(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL))
		return;

	filter_flush(ctx, p, FLUSH_FILL);
	if (p->gstate->empty_clip_region)
		return;
	if (p->chain->op_F)
		p->chain->op_F(ctx, p->chain);
}

static void
pdf_filter_f(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL))
		return;

	filter_flush(ctx, p, FLUSH_FILL);
	if (p->chain->op_f)
		p->chain->op_f(ctx, p->chain);
}

static void
pdf_filter_fstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL))
		return;

	filter_flush(ctx, p, FLUSH_FILL);
	if (p->chain->op_fstar)
		p->chain->op_fstar(ctx, p->chain);
}

static void
pdf_filter_B(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL_STROKE))
		return;

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_B)
		p->chain->op_B(ctx, p->chain);
}

static void
pdf_filter_Bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL_STROKE))
		return;

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_Bstar)
		p->chain->op_Bstar(ctx, p->chain);
}

static void
pdf_filter_b(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL_STROKE))
		return;

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_b)
		p->chain->op_b(ctx, p->chain);
}

static void
pdf_filter_bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_PATH_FILL_STROKE))
		return;

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_bstar)
		p->chain->op_bstar(ctx, p->chain);
}

static void
pdf_filter_n(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_drop_path(ctx, p->path);
		p->path = NULL;
		p->path = fz_new_path(ctx);
	}

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_n)
		p->chain->op_n(ctx, p->chain);
}

/* clipping paths */

static void
pdf_filter_W(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_CLIP_PATH))
		return;

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_W)
		p->chain->op_W(ctx, p->chain);
}

static void
pdf_filter_Wstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (cull_path(ctx, p, FZ_CULL_CLIP_PATH))
		return;

	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_Wstar)
		p->chain->op_Wstar(ctx, p->chain);
}

/* text objects */

static void
pdf_filter_BT(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->tos.tm = fz_identity;
	p->tos.tlm = fz_identity;
	p->BT_pending = 1;
	p->text_sent = 0;
	p->text_removed = 0;
	p->Td_pending = 0;
	p->Td_value.x = p->Td_value.y = 0;
}

static void
pdf_filter_ET(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (!p->BT_pending)
	{
		filter_flush(ctx, p, 0);
		if (p->chain->op_ET)
			p->chain->op_ET(ctx, p->chain);
		p->in_BT = 0;
	}
	if ((p->gstate->pending.text.render & 4) && p->text_removed && !p->text_sent)
	{
		/* We've just had an empty (or non-marking) BT/ET, where clipping is
		 * enabled, and previously it was non-empty. This means we've changed
		 * it from a finite clip path, to something that doesn't change the
		 * clip path at all. We actually want it to be an empty clip path,
		 * but we can't easily achieve that. So we have to do the clipping
		 * ourselves. */
		p->gstate->empty_clip_region = 1;
	}
	p->BT_pending = 0;
	if (p->options->after_text_object)
	{
		fz_matrix ctm;
		ctm = fz_concat(p->gstate->pending.ctm, p->gstate->sent.ctm);
		ctm = fz_concat(ctm, p->transform);
		if (p->chain->op_q)
			p->chain->op_q(ctx, p->chain);
		p->options->after_text_object(ctx, p->options->opaque, p->doc, p->chain, ctm);
		if (p->chain->op_Q)
			p->chain->op_Q(ctx, p->chain);
	}
}

static void
pdf_filter_Q(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	filter_flush(ctx, p, FLUSH_TEXT);
	if (p->in_BT)
		pdf_filter_ET(ctx, proc);
	filter_pop(ctx, p);
}

/* text state */

static void
pdf_filter_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->gstate->pending.text.char_space = charspace;
}

static void
pdf_filter_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->gstate->pending.text.word_space = wordspace;
}

static void
pdf_filter_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	/* scale is as written in the file. It is 100 times smaller
	 * in the gstate. */
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->gstate->pending.text.scale = scale / 100;
}

static void
pdf_filter_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->gstate->pending.text.leading = leading;
}

static void
pdf_filter_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	fz_free(ctx, p->font_name);
	p->font_name = NULL;
	p->font_name = name ? fz_strdup(ctx, name) : NULL;
	pdf_drop_font(ctx, p->gstate->pending.text.font);
	p->gstate->pending.text.font = pdf_keep_font(ctx, font);
	p->gstate->pending.text.size = size;
	copy_resource(ctx, p, PDF_NAME(Font), name);
}

static void
pdf_filter_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->gstate->pending.text.render = render;
}

static void
pdf_filter_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	p->gstate->pending.text.rise = rise;
}

/* text positioning */

static void
pdf_filter_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	p->Tm_adjust = 0;
	pdf_tos_translate(&p->tos, tx, ty);
	if (p->Tm_pending)
		return; /* Exit, just with Tm_pending */
	if (p->Td_pending)
		tx += p->Td_value.x, ty += p->Td_value.y;
	p->Td_value.x = tx;
	p->Td_value.y = ty;
	p->Td_pending = 1;
}

static void
pdf_filter_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	p->gstate->pending.text.leading = -ty;
	pdf_filter_Td(ctx, proc, tx, ty);
}

static void
pdf_filter_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	pdf_tos_set_matrix(&p->tos, a, b, c, d, e, f);
	p->Tm_pending = 1;
	p->Td_pending = 0;
	p->Tm_adjust = 0;
}

static void
pdf_filter_Tstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	p->Tm_adjust = 0;
	filter_flush(ctx, p, FLUSH_ALL);
	pdf_tos_newline(&p->tos, p->gstate->pending.text.leading);
	/* If Tm_pending, then just adjusting the matrix (as
	 * pdf_tos_newline has done) is enough. Otherwise we
	 * need to actually call the operator. */
	if (!p->Tm_pending && p->chain->op_Tstar)
		p->chain->op_Tstar(ctx, p->chain);
}

/* text showing */

static void
pdf_filter_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_show_text(ctx, p, array);
}

static void
pdf_filter_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_show_string(ctx, p, (unsigned char *)str, len);
}

static void
pdf_filter_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	/* Note, we convert all T' operators to (maybe) a T* and a Tj */
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	/* need to flush text state and clear adjustment before we emit a newline */
	p->Tm_adjust = 0;
	filter_flush(ctx, p, FLUSH_ALL);

	pdf_tos_newline(&p->tos, p->gstate->pending.text.leading);
	/* If Tm_pending, then just adjusting the matrix (as
	 * pdf_tos_newline has done) is enough. Otherwise we
	 * need to do it manually. */
	if (!p->Tm_pending && p->chain->op_Tstar)
		p->chain->op_Tstar(ctx, p->chain);
	filter_show_string(ctx, p, (unsigned char *)str, len);
}

static void
pdf_filter_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	/* Note, we convert all T" operators to (maybe) a T*,
	 * (maybe) Tc, (maybe) Tw and a Tj. */
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	/* need to flush text state and clear adjustment before we emit a newline */
	p->Tm_adjust = 0;
	filter_flush(ctx, p, FLUSH_ALL);

	p->gstate->pending.text.word_space = aw;
	p->gstate->pending.text.char_space = ac;
	pdf_tos_newline(&p->tos, p->gstate->pending.text.leading);
	/* If Tm_pending, then just adjusting the matrix (as
	 * pdf_tos_newline has done) is enough. Otherwise we
	 * need to do it manually. */
	if (!p->Tm_pending && p->chain->op_Tstar)
		p->chain->op_Tstar(ctx, p->chain);
	filter_show_string(ctx, p, (unsigned char*)str, len);
}

/* type 3 fonts */

static void
pdf_filter_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_d0)
		p->chain->op_d0(ctx, p->chain, wx, wy);
}

static void
pdf_filter_d1(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_d1)
		p->chain->op_d1(ctx, p->chain, wx, wy, llx, lly, urx, ury);
}

/* color */

static void
set_default_cs_values(pdf_filter_gstate_sc *sc, const char *name, fz_colorspace *cs)
{
	int n = cs ? cs->n : 0;
	int i;
	if (strcmp(name, "Separation") == 0 || strcmp(name, "DeviceN") == 0) {
		for (i = 0; i < n; ++i)
			sc->c[i] = 1;
	}
	else if (strcmp(name, "DeviceGray") == 0 ||
		strcmp(name, "DeviceRGB") == 0 ||
		strcmp(name, "CalGray") == 0 ||
		strcmp(name, "CalRGB") == 0 ||
		strcmp(name, "Indexed") == 0)
	{
		for (i = 0; i < n; ++i)
			sc->c[i] = 0;
	}
	else if (strcmp(name, "DeviceCMYK") == 0)
	{
		sc->c[0] = 0;
		sc->c[1] = 0;
		sc->c[2] = 0;
		sc->c[3] = 1;
	}
	else if (strcmp(name, "Lab") == 0 ||
		strcmp(name, "ICCBased") == 0)
	{
		/* Really we should clamp c[i] to the appropriate range. */
		for (i = 0; i < n; ++i)
			sc->c[i] = 0;
	}
	else
		return;
	sc->pat = NULL;
	sc->shd = NULL;
	sc->name[0] = 0;
	sc->n = n;
}

static void
pdf_filter_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	fz_strlcpy(gstate->pending.CS.name, name, sizeof gstate->pending.CS.name);
	gstate->pending.CS.cs = cs;
	copy_resource(ctx, p, PDF_NAME(ColorSpace), name);
	set_default_cs_values(&gstate->pending.SC, name, cs);
}

static void
pdf_filter_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	fz_strlcpy(gstate->pending.cs.name, name, sizeof gstate->pending.cs.name);
	gstate->pending.cs.cs = cs;
	copy_resource(ctx, p, PDF_NAME(ColorSpace), name);
	set_default_cs_values(&gstate->pending.sc, name, cs);
}

static void
pdf_filter_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;

	if (p->gstate->empty_clip_region)
		return;

	fz_strlcpy(gstate->pending.SC.name, name, sizeof gstate->pending.SC.name);
	gstate->pending.SC.pat = pat;
	gstate->pending.SC.shd = NULL;
	gstate->pending.SC.n = n;
	for (i = 0; i < n; ++i)
		gstate->pending.SC.c[i] = color[i];
	copy_resource(ctx, p, PDF_NAME(Pattern), name);
}

static void
pdf_filter_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;

	if (p->gstate->empty_clip_region)
		return;

	fz_strlcpy(gstate->pending.sc.name, name, sizeof gstate->pending.sc.name);
	gstate->pending.sc.pat = pat;
	gstate->pending.sc.shd = NULL;
	gstate->pending.sc.n = n;
	for (i = 0; i < n; ++i)
		gstate->pending.sc.c[i] = color[i];
	copy_resource(ctx, p, PDF_NAME(Pattern), name);
}

static void
pdf_filter_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	fz_strlcpy(gstate->pending.SC.name, name, sizeof gstate->pending.SC.name);
	gstate->pending.SC.pat = NULL;
	gstate->pending.SC.shd = shade;
	gstate->pending.SC.n = 0;
	copy_resource(ctx, p, PDF_NAME(Pattern), name);
}

static void
pdf_filter_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);

	if (p->gstate->empty_clip_region)
		return;

	fz_strlcpy(gstate->pending.sc.name, name, sizeof gstate->pending.sc.name);
	gstate->pending.sc.pat = NULL;
	gstate->pending.sc.shd = shade;
	gstate->pending.sc.n = 0;
	copy_resource(ctx, p, PDF_NAME(Pattern), name);
}

static void
pdf_filter_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;

	if (p->gstate->empty_clip_region)
		return;

	gstate->pending.SC.name[0] = 0;
	gstate->pending.SC.pat = NULL;
	gstate->pending.SC.shd = NULL;
	gstate->pending.SC.n = n;
	for (i = 0; i < n; ++i)
		gstate->pending.SC.c[i] = color[i];
}

static void
pdf_filter_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;

	if (p->gstate->empty_clip_region)
		return;

	gstate->pending.sc.name[0] = 0;
	gstate->pending.sc.pat = NULL;
	gstate->pending.sc.shd = NULL;
	gstate->pending.sc.n = n;
	for (i = 0; i < n; ++i)
		gstate->pending.sc.c[i] = color[i];
}

static void
pdf_filter_G(fz_context *ctx, pdf_processor *proc, float g)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	float color[1] = { g };

	if (p->gstate->empty_clip_region)
		return;

	pdf_filter_CS(ctx, proc, "DeviceGray", fz_device_gray(ctx));
	pdf_filter_SC_color(ctx, proc, 1, color);
}

static void
pdf_filter_g(fz_context *ctx, pdf_processor *proc, float g)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	float color[1] = { g };

	if (p->gstate->empty_clip_region)
		return;

	pdf_filter_cs(ctx, proc, "DeviceGray", fz_device_gray(ctx));
	pdf_filter_sc_color(ctx, proc, 1, color);
}

static void
pdf_filter_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	float color[3] = { r, g, b };

	if (p->gstate->empty_clip_region)
		return;

	pdf_filter_CS(ctx, proc, "DeviceRGB", fz_device_rgb(ctx));
	pdf_filter_SC_color(ctx, proc, 3, color);
}

static void
pdf_filter_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	float color[3] = { r, g, b };

	if (p->gstate->empty_clip_region)
		return;

	pdf_filter_cs(ctx, proc, "DeviceRGB", fz_device_rgb(ctx));
	pdf_filter_sc_color(ctx, proc, 3, color);
}

static void
pdf_filter_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	float color[4] = { c, m, y, k };

	if (p->gstate->empty_clip_region)
		return;

	pdf_filter_CS(ctx, proc, "DeviceCMYK", fz_device_cmyk(ctx));
	pdf_filter_SC_color(ctx, proc, 4, color);
}

static void
pdf_filter_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	float color[4] = { c, m, y, k };

	if (p->gstate->empty_clip_region)
		return;

	pdf_filter_cs(ctx, proc, "DeviceCMYK", fz_device_cmyk(ctx));
	pdf_filter_sc_color(ctx, proc, 4, color);
}

/* shadings, images, xobjects */

static void
pdf_filter_BI(fz_context *ctx, pdf_processor *proc, fz_image *image, const char *colorspace)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_matrix ctm = fz_concat(p->gstate->pending.ctm, p->gstate->sent.ctm);
		fz_rect r = { 0, 0, 1, 1 };
		ctm = fz_concat(ctm, p->transform);
		r = fz_transform_rect(r, ctm);

		if (p->options->culler(ctx, p->options->opaque, r, FZ_CULL_IMAGE))
			return;
	}

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_BI)
	{
		if (p->options->image_filter)
		{
			fz_matrix ctm = fz_concat(p->gstate->sent.ctm, p->transform);
			image = p->options->image_filter(ctx, p->options->opaque, ctm, "<inline>", image);
			if (image)
			{
				fz_try(ctx)
				{
					copy_resource(ctx, p, PDF_NAME(ColorSpace), colorspace);
					p->chain->op_BI(ctx, p->chain, image, colorspace);
				}
				fz_always(ctx)
					fz_drop_image(ctx, image);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
		}
		else
		{
			copy_resource(ctx, p, PDF_NAME(ColorSpace), colorspace);
			p->chain->op_BI(ctx, p->chain, image, colorspace);
		}
	}
}

static void
pdf_filter_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_matrix ctm = fz_concat(p->gstate->pending.ctm, p->gstate->sent.ctm);
		fz_rect r = { 0, 0, 1, 1 };
		ctm = fz_concat(ctm, p->transform);
		fz_bound_shade(ctx, shade, ctm);
		r = fz_transform_rect(r, ctm);

		if (p->options->culler(ctx, p->options->opaque, r, FZ_CULL_SHADING))
			return;
	}

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_sh)
		p->chain->op_sh(ctx, p->chain, name, shade);
	copy_resource(ctx, p, PDF_NAME(Shading), name);
}

static void
pdf_filter_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	fz_image *new_image;

	if (p->gstate->empty_clip_region)
		return;

	if (p->options->culler)
	{
		fz_matrix ctm = fz_concat(p->gstate->pending.ctm, p->gstate->sent.ctm);
		fz_rect r = { 0, 0, 1, 1 };
		ctm = fz_concat(ctm, p->transform);
		r = fz_transform_rect(r, ctm);

		if (p->options->culler(ctx, p->options->opaque, r, FZ_CULL_IMAGE))
			return;
	}

	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_Do_image)
	{
		if (p->options->image_filter)
		{
			fz_matrix ctm = fz_concat(p->gstate->sent.ctm, p->transform);
			new_image = p->options->image_filter(ctx, p->options->opaque, ctm, name, image);
		}
		else
		{
			new_image = image;
		}

		if (new_image == image)
		{
			if (p->global_options->instance_forms)
			{
				/* Make up a unique name when instancing forms so we don't accidentally clash. */
				char buf[40];
				pdf_obj *obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->old_rdb, PDF_NAME(XObject)), name);
				create_resource_name(ctx, p, PDF_NAME(XObject), "Im", buf, sizeof buf);
				add_resource(ctx, p, PDF_NAME(XObject), buf, obj);
				p->chain->op_Do_image(ctx, p->chain, buf, image);
			}
			else
			{
				copy_resource(ctx, p, PDF_NAME(XObject), name);
				p->chain->op_Do_image(ctx, p->chain, name, image);
			}
		}
		else if (new_image != NULL)
		{
			pdf_obj *obj = NULL;
			fz_var(obj);
			fz_try(ctx)
			{
				char buf[40];
				create_resource_name(ctx, p, PDF_NAME(XObject), "Im", buf, sizeof buf);
				obj = pdf_add_image(ctx, p->doc, new_image);
				add_resource(ctx, p, PDF_NAME(XObject), buf, obj);
				p->chain->op_Do_image(ctx, p->chain, buf, new_image);
			}
			fz_always(ctx)
			{
				pdf_drop_obj(ctx, obj);
				fz_drop_image(ctx, new_image);
			}
			fz_catch(ctx)
				fz_rethrow(ctx);
		}
	}
}

static void
pdf_filter_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	fz_matrix transform;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, FLUSH_ALL);

	if (p->global_options->instance_forms)
	{
		/* Copy an instance of the form with a new unique name. */
		pdf_obj *new_xobj;
		char buf[40];
		create_resource_name(ctx, p, PDF_NAME(XObject), "Fm", buf, sizeof buf);
		transform = fz_concat(p->gstate->sent.ctm, p->transform);
		new_xobj = pdf_filter_xobject_instance(ctx, xobj, p->rstack->new_rdb, transform, p->global_options, NULL);
		fz_try(ctx)
		{
			add_resource(ctx, p, PDF_NAME(XObject), buf, new_xobj);
			if (p->chain->op_Do_form)
				p->chain->op_Do_form(ctx, p->chain, buf, new_xobj);
		}
		fz_always(ctx)
			pdf_drop_obj(ctx, new_xobj);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else
	{
		copy_resource(ctx, p, PDF_NAME(XObject), name);
		if (p->chain->op_Do_form)
			p->chain->op_Do_form(ctx, p->chain, name, xobj);
	}
}

/* marked content */

static void
pdf_filter_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_MP)
		p->chain->op_MP(ctx, p->chain, tag);
}

static void
pdf_filter_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	if (p->gstate->empty_clip_region)
		return;

	filter_flush(ctx, p, 0);
	if (p->chain->op_DP)
		p->chain->op_DP(ctx, p->chain, tag, raw, cooked);
}

static void
pdf_filter_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	/* Create a tag, and push it onto pending_tags. If it gets
	 * flushed to the stream, it'll be moved from there onto
	 * current_tags. */
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	tag_record *bmc = fz_malloc_struct(ctx, tag_record);

	fz_try(ctx)
		bmc->tag = fz_strdup(ctx, tag);
	fz_catch(ctx)
	{
		fz_free(ctx, bmc);
		fz_rethrow(ctx);
	}
	bmc->prev = p->pending_tags;
	p->pending_tags = bmc;
}

static void
pdf_filter_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	/* Create a tag, and push it onto pending_tags. If it gets
	 * flushed to the stream, it'll be moved from there onto
	 * current_tags. */
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	tag_record *bdc = fz_malloc_struct(ctx, tag_record);
	pdf_obj *mcid;
	pdf_obj *str;

	fz_try(ctx)
	{
		bdc->bdc = 1;
		bdc->tag = fz_strdup(ctx, tag);
		bdc->raw = pdf_keep_obj(ctx, raw);
		bdc->cooked = pdf_keep_obj(ctx, raw);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, bdc->tag);
		pdf_drop_obj(ctx, bdc->raw);
		pdf_drop_obj(ctx, bdc->cooked);
		fz_free(ctx, bdc);
		fz_rethrow(ctx);
	}
	bdc->prev = p->pending_tags;
	p->pending_tags = bdc;

	/* Look to see if this has an mcid object */
	mcid = pdf_dict_get(ctx, cooked, PDF_NAME(MCID));
	if (!pdf_is_number(ctx, mcid))
		return;
	bdc->mcid_num = pdf_to_int(ctx, mcid);
	bdc->mcid_obj = pdf_keep_obj(ctx, pdf_array_get(ctx, p->structarray, bdc->mcid_num));
	str = pdf_dict_get(ctx, bdc->mcid_obj, PDF_NAME(Alt));
	if (str)
		bdc->alt.utf8 = pdf_new_utf8_from_pdf_string_obj(ctx, str);
	str = pdf_dict_get(ctx, bdc->mcid_obj, PDF_NAME(ActualText));
	if (str)
		bdc->actualtext.utf8 = pdf_new_utf8_from_pdf_string_obj(ctx, str);
}

/* Bin the topmost (most recent) tag from a tag list. */
static void
pop_tag(fz_context *ctx, pdf_sanitize_processor *p, tag_record **tags)
{
	tag_record *tag = *tags;

	if (tag == NULL)
		return;
	*tags = tag->prev;
	fz_free(ctx, tag->tag);
	if (tag->bdc)
	{
		pdf_drop_obj(ctx, tag->raw);
		pdf_drop_obj(ctx, tag->cooked);
	}
	fz_free(ctx, tag->alt.utf8);
	fz_free(ctx, tag->actualtext.utf8);
	pdf_drop_obj(ctx, tag->mcid_obj);
	fz_free(ctx, tag);
}

static void
pdf_filter_EMC(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	/* If we have any pending tags, pop one of those. If not,
	 * pop one of the current ones, and pass the EMC on. */
	if (p->pending_tags != NULL)
		pop_tag(ctx, p, &p->pending_tags);
	else if (p->current_tags)
	{
		update_mcid(ctx, p);
		copy_resource(ctx, p, PDF_NAME(Properties), pdf_to_name(ctx, p->current_tags->raw));
		pop_tag(ctx, p, &p->current_tags);
		if (p->chain->op_EMC)
			p->chain->op_EMC(ctx, p->chain);
	}
}

/* compatibility */

static void
pdf_filter_BX(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	filter_flush(ctx, p, 0);
	if (p->chain->op_BX)
		p->chain->op_BX(ctx, p->chain);
}

static void
pdf_filter_EX(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;

	filter_flush(ctx, p, 0);
	if (p->chain->op_EX)
		p->chain->op_EX(ctx, p->chain);
}

static void
pdf_filter_END(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_flush(ctx, p, FLUSH_TEXT);
	if (p->chain->op_END)
		p->chain->op_END(ctx, p->chain);
}

static void
pdf_close_sanitize_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	while (!filter_pop(ctx, p))
	{
		/* Nothing to do in the loop, all work done above */
	}
	pdf_close_processor(ctx, p->chain);
}

static void
pdf_drop_sanitize_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor*)proc;
	filter_gstate *gs = p->gstate;
	while (gs)
	{
		filter_gstate *next = gs->next;
		pdf_drop_font(ctx, gs->pending.text.font);
		pdf_drop_font(ctx, gs->sent.text.font);
		fz_free(ctx, gs);
		gs = next;
	}
	while (p->pending_tags)
		pop_tag(ctx, p, &p->pending_tags);
	while (p->current_tags)
		pop_tag(ctx, p, &p->current_tags);
	pdf_drop_obj(ctx, p->structarray);
	pdf_drop_document(ctx, p->doc);
	fz_free(ctx, p->font_name);

	fz_drop_path(ctx, p->path);

	while (p->rstack)
	{
		resources_stack *stk = p->rstack;
		p->rstack = stk->next;
		pdf_drop_obj(ctx, stk->new_rdb);
		pdf_drop_obj(ctx, stk->old_rdb);
		fz_free(ctx, stk);
	}
}

static void
pdf_sanitize_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)proc;
	resources_stack *stk = fz_malloc_struct(ctx, resources_stack);

	stk->next = p->rstack;
	p->rstack = stk;
	fz_try(ctx)
	{
		stk->old_rdb = pdf_keep_obj(ctx, res);
		stk->new_rdb = pdf_new_dict(ctx, p->doc, 1);
		pdf_processor_push_resources(ctx, p->chain, stk->new_rdb);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, stk->old_rdb);
		pdf_drop_obj(ctx, stk->new_rdb);
		p->rstack = stk->next;
		fz_free(ctx, stk);
		fz_rethrow(ctx);
	}
}

static pdf_obj *
pdf_sanitize_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_sanitize_processor *p = (pdf_sanitize_processor *)proc;
	resources_stack *stk = p->rstack;

	p->rstack = stk->next;
	pdf_drop_obj(ctx, stk->old_rdb);
	pdf_drop_obj(ctx, stk->new_rdb);
	fz_free(ctx, stk);

	return pdf_processor_pop_resources(ctx, p->chain);
}

pdf_processor *
pdf_new_sanitize_filter(
	fz_context *ctx,
	pdf_document *doc,
	pdf_processor *chain,
	int structparents,
	fz_matrix transform,
	pdf_filter_options *options,
	void *sopts_)
{
	pdf_sanitize_processor *proc = pdf_new_processor(ctx, sizeof *proc);
	pdf_sanitize_filter_options *sopts = (pdf_sanitize_filter_options *)sopts_;

	proc->super.close_processor = pdf_close_sanitize_processor;
	proc->super.drop_processor = pdf_drop_sanitize_processor;

	proc->super.push_resources = pdf_sanitize_push_resources;
	proc->super.pop_resources = pdf_sanitize_pop_resources;

	/* general graphics state */
	proc->super.op_w = pdf_filter_w;
	proc->super.op_j = pdf_filter_j;
	proc->super.op_J = pdf_filter_J;
	proc->super.op_M = pdf_filter_M;
	proc->super.op_d = pdf_filter_d;
	proc->super.op_ri = pdf_filter_ri;
	proc->super.op_i = pdf_filter_i;
	proc->super.op_gs_begin = pdf_filter_gs_begin;
	proc->super.op_gs_end = pdf_filter_gs_end;

	/* transparency graphics state */
	proc->super.op_gs_BM = pdf_filter_gs_BM;
	proc->super.op_gs_CA = pdf_filter_gs_CA;
	proc->super.op_gs_ca = pdf_filter_gs_ca;
	proc->super.op_gs_SMask = pdf_filter_gs_SMask;

	/* special graphics state */
	proc->super.op_q = pdf_filter_q;
	proc->super.op_Q = pdf_filter_Q;
	proc->super.op_cm = pdf_filter_cm;

	/* path construction */
	proc->super.op_m = pdf_filter_m;
	proc->super.op_l = pdf_filter_l;
	proc->super.op_c = pdf_filter_c;
	proc->super.op_v = pdf_filter_v;
	proc->super.op_y = pdf_filter_y;
	proc->super.op_h = pdf_filter_h;
	proc->super.op_re = pdf_filter_re;

	/* path painting */
	proc->super.op_S = pdf_filter_S;
	proc->super.op_s = pdf_filter_s;
	proc->super.op_F = pdf_filter_F;
	proc->super.op_f = pdf_filter_f;
	proc->super.op_fstar = pdf_filter_fstar;
	proc->super.op_B = pdf_filter_B;
	proc->super.op_Bstar = pdf_filter_Bstar;
	proc->super.op_b = pdf_filter_b;
	proc->super.op_bstar = pdf_filter_bstar;
	proc->super.op_n = pdf_filter_n;

	/* clipping paths */
	proc->super.op_W = pdf_filter_W;
	proc->super.op_Wstar = pdf_filter_Wstar;

	/* text objects */
	proc->super.op_BT = pdf_filter_BT;
	proc->super.op_ET = pdf_filter_ET;

	/* text state */
	proc->super.op_Tc = pdf_filter_Tc;
	proc->super.op_Tw = pdf_filter_Tw;
	proc->super.op_Tz = pdf_filter_Tz;
	proc->super.op_TL = pdf_filter_TL;
	proc->super.op_Tf = pdf_filter_Tf;
	proc->super.op_Tr = pdf_filter_Tr;
	proc->super.op_Ts = pdf_filter_Ts;

	/* text positioning */
	proc->super.op_Td = pdf_filter_Td;
	proc->super.op_TD = pdf_filter_TD;
	proc->super.op_Tm = pdf_filter_Tm;
	proc->super.op_Tstar = pdf_filter_Tstar;

	/* text showing */
	proc->super.op_TJ = pdf_filter_TJ;
	proc->super.op_Tj = pdf_filter_Tj;
	proc->super.op_squote = pdf_filter_squote;
	proc->super.op_dquote = pdf_filter_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = pdf_filter_d0;
	proc->super.op_d1 = pdf_filter_d1;

	/* color */
	proc->super.op_CS = pdf_filter_CS;
	proc->super.op_cs = pdf_filter_cs;
	proc->super.op_SC_color = pdf_filter_SC_color;
	proc->super.op_sc_color = pdf_filter_sc_color;
	proc->super.op_SC_pattern = pdf_filter_SC_pattern;
	proc->super.op_sc_pattern = pdf_filter_sc_pattern;
	proc->super.op_SC_shade = pdf_filter_SC_shade;
	proc->super.op_sc_shade = pdf_filter_sc_shade;

	proc->super.op_G = pdf_filter_G;
	proc->super.op_g = pdf_filter_g;
	proc->super.op_RG = pdf_filter_RG;
	proc->super.op_rg = pdf_filter_rg;
	proc->super.op_K = pdf_filter_K;
	proc->super.op_k = pdf_filter_k;

	/* shadings, images, xobjects */
	proc->super.op_BI = pdf_filter_BI;
	proc->super.op_sh = pdf_filter_sh;
	proc->super.op_Do_image = pdf_filter_Do_image;
	proc->super.op_Do_form = pdf_filter_Do_form;

	/* marked content */
	proc->super.op_MP = pdf_filter_MP;
	proc->super.op_DP = pdf_filter_DP;
	proc->super.op_BMC = pdf_filter_BMC;
	proc->super.op_BDC = pdf_filter_BDC;
	proc->super.op_EMC = pdf_filter_EMC;

	/* compatibility */
	proc->super.op_BX = pdf_filter_BX;
	proc->super.op_EX = pdf_filter_EX;

	/* extgstate */
	proc->super.op_gs_OP = pdf_filter_gs_OP;
	proc->super.op_gs_op = pdf_filter_gs_op;
	proc->super.op_gs_OPM = pdf_filter_gs_OPM;
	proc->super.op_gs_UseBlackPtComp = pdf_filter_gs_UseBlackPtComp;

	proc->super.op_END = pdf_filter_END;

	proc->doc = pdf_keep_document(ctx, doc);
	proc->structparents = structparents;
	if (structparents != -1)
		proc->structarray = pdf_keep_obj(ctx, pdf_lookup_number(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/StructTreeRoot/ParentTree"), structparents));
	proc->chain = chain;
	proc->global_options = options;
	proc->options = sopts;
	proc->transform = transform;
	proc->path = NULL;

	fz_try(ctx)
	{
		if (proc->options->culler)
			proc->path = fz_new_path(ctx);
		proc->gstate = fz_malloc_struct(ctx, filter_gstate);
		proc->gstate->pending.ctm = fz_identity;
		proc->gstate->sent.ctm = fz_identity;
		proc->gstate->pending.text.scale = 1;
		proc->gstate->pending.text.size = -1;
		proc->gstate->pending.stroke.linewidth = 1;
		proc->gstate->pending.stroke.miterlimit = 10;
		proc->gstate->sent.text.scale = 1;
		proc->gstate->sent.text.size = -1;
		proc->gstate->sent.stroke.linewidth = 1;
		proc->gstate->sent.stroke.miterlimit = 10;
	}
	fz_catch(ctx)
	{
		pdf_drop_processor(ctx, (pdf_processor *) proc);
		fz_rethrow(ctx);
	}

	return (pdf_processor*)proc;
}
