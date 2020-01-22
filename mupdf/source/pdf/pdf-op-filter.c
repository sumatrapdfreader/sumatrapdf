#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

typedef struct filter_gstate_s filter_gstate;

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

typedef struct pdf_filter_gstate_s pdf_filter_gstate;

struct pdf_filter_gstate_s
{
	fz_matrix ctm;
	struct
	{
		char name[256];
		fz_colorspace *cs;
	} cs, CS;
	struct
	{
		char name[256];
		pdf_pattern *pat;
		fz_shade *shd;
		int n;
		float c[FZ_MAX_COLORS];
	} sc, SC;
	struct
	{
		fz_linecap linecap;
		fz_linejoin linejoin;
		float linewidth;
		float miterlimit;
	} stroke;
	pdf_text_state text;
};

struct filter_gstate_s
{
	filter_gstate *next;
	int pushed;
	pdf_filter_gstate pending;
	pdf_filter_gstate sent;
};

typedef struct editable_str_s
{
	char *utf8;
	int edited;
	int pos;
} editable_str;

typedef struct tag_record_s
{
	int bdc;
	char *tag;
	pdf_obj *raw;
	pdf_obj *cooked;

	int mcid_num;
	pdf_obj *mcid_obj;
	editable_str alt;
	editable_str actualtext;

	struct tag_record_s *prev;
} tag_record;

typedef struct pdf_filter_processor_s
{
	pdf_processor super;
	pdf_document *doc;
	int structparents;
	pdf_obj *structarray;
	pdf_processor *chain;
	filter_gstate *gstate;
	pdf_text_object_state tos;
	int Tm_pending;
	int BT_pending;
	int in_BT;
	float Tm_adjust;
	void *font_name;
	tag_record *current_tags;
	tag_record *pending_tags;
	pdf_obj *old_rdb, *new_rdb;
	pdf_filter_options *filter;
	fz_matrix transform;
	int form_id;
	int image_id;
} pdf_filter_processor;

static void
copy_resource(fz_context *ctx, pdf_filter_processor *p, pdf_obj *key, const char *name)
{
	pdf_obj *res, *obj;

	if (!name || name[0] == 0)
		return;

	res = pdf_dict_get(ctx, p->old_rdb, key);
	obj = pdf_dict_gets(ctx, res, name);
	if (obj)
	{
		res = pdf_dict_get(ctx, p->new_rdb, key);
		if (!res)
		{
			res = pdf_new_dict(ctx, pdf_get_bound_document(ctx, p->new_rdb), 1);
			pdf_dict_put_drop(ctx, p->new_rdb, key, res);
		}
		pdf_dict_putp(ctx, res, name, obj);
	}
}

static void
add_resource(fz_context *ctx, pdf_filter_processor *p, pdf_obj *key, const char *name, pdf_obj *val)
{
	pdf_obj *res = pdf_dict_get(ctx, p->new_rdb, key);
	if (!res)
		res = pdf_dict_put_dict(ctx, p->new_rdb, key, 8);
	pdf_dict_puts(ctx, res, name, val);
}

static void
filter_push(fz_context *ctx, pdf_filter_processor *p)
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
filter_pop(fz_context *ctx, pdf_filter_processor *p)
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
gstate_to_update(fz_context *ctx, pdf_filter_processor *p)
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

static void flush_tags(fz_context *ctx, pdf_filter_processor *p, tag_record **tags)
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

static void filter_flush(fz_context *ctx, pdf_filter_processor *p, int flush)
{
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;

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
		}
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
		if (p->Tm_pending != 0)
		{
			if (p->chain->op_Tm)
				p->chain->op_Tm(ctx, p->chain, p->tos.tlm.a, p->tos.tlm.b, p->tos.tlm.c, p->tos.tlm.d, p->tos.tlm.e, p->tos.tlm.f);
			p->Tm_pending = 0;
		}
	}
}

static int
filter_show_char(fz_context *ctx, pdf_filter_processor *p, int cid, int *unicode)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;
	fz_matrix trm;
	int ucsbuf[8];
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

	if (p->filter->text_filter)
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

		remove = p->filter->text_filter(ctx, p->filter->opaque, ucsbuf, ucslen, trm, ctm, bbox);
	}

	pdf_tos_move_after_char(ctx, &p->tos);

	return remove;
}

static void
filter_show_space(fz_context *ctx, pdf_filter_processor *p, float tadj)
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
			str->pos += n;
		}
		else if (uni == 32) {
			/* We don't care if we're given whitespace
			 * and it doesn't match the string. Don't
			 * skip forward. Nothing to remove. */
			break;
		}
		else if (rune == 32) {
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
mcid_char_imp(fz_context *ctx, pdf_filter_processor *p, tag_record *tr, int uni, int remove)
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
mcid_char(fz_context *ctx, pdf_filter_processor *p, int uni, int remove)
{
	tag_record *tr  = p->pending_tags;

	for (tr = p->pending_tags; tr != NULL; tr = tr->prev)
		mcid_char_imp(ctx, p, tr, uni, remove);
	for (tr = p->current_tags; tr != NULL; tr = tr->prev)
		mcid_char_imp(ctx, p, tr, uni, remove);
}

static void
update_mcid(fz_context *ctx, pdf_filter_processor *p)
{
	tag_record *tag = p->current_tags;

	if (tag == NULL)
		return;
	if (tag->alt.edited)
		pdf_dict_put_text_string(ctx, tag->mcid_obj, PDF_NAME(Alt), tag->alt.utf8);
	if (tag->actualtext.edited)
		pdf_dict_put_text_string(ctx, tag->mcid_obj, PDF_NAME(Alt), tag->actualtext.utf8);
}

/* Process a string (from buf, of length len), from position *pos onwards.
 * Stop when we hit the end, or when we find a character to remove. The
 * caller will restart us again later. On exit, *pos = the point we got to,
 * *inc = The number of bytes to skip to step over the next character (unless
 * we hit the end).
 */
static void
filter_string_to_segment(fz_context *ctx, pdf_filter_processor *p, unsigned char *buf, size_t len, size_t *pos, int *inc, int *removed_space)
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
		}
		else
			remove = filter_show_char(ctx, p, cid, &uni);
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
adjust_text(fz_context *ctx, pdf_filter_processor *p, float x, float y)
{
	float skip_dist = p->tos.fontdesc->wmode == 1 ? -y : -x;
	skip_dist = skip_dist / p->gstate->pending.text.size;
	p->Tm_adjust += skip_dist;
}

static void
adjust_for_removed_space(fz_context *ctx, pdf_filter_processor *p)
{
	filter_gstate *gstate = p->gstate;
	float adj = gstate->pending.text.word_space;
	adjust_text(ctx, p, adj * gstate->pending.text.scale, adj);
}

static void
flush_adjustment(fz_context *ctx, pdf_filter_processor *p)
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
push_adjustment_to_array(fz_context *ctx, pdf_filter_processor *p, pdf_obj *arr)
{
	if (p->Tm_adjust == 0)
		return;
	pdf_array_push_real(ctx, arr, p->Tm_adjust * 1000);
	p->Tm_adjust = 0;
}

static void
filter_show_string(fz_context *ctx, pdf_filter_processor *p, unsigned char *buf, size_t len)
{
	filter_gstate *gstate = p->gstate;
	pdf_font_desc *fontdesc = gstate->pending.text.font;
	int inc, removed_space;
	size_t i;

	if (!fontdesc)
		return;

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
filter_show_text(fz_context *ctx, pdf_filter_processor *p, pdf_obj *text)
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	gstate->pending.stroke.linewidth = linewidth;
}

static void
pdf_filter_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	gstate->pending.stroke.linejoin = linejoin;
}

static void
pdf_filter_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	gstate->pending.stroke.linecap = linecap;
}

static void
pdf_filter_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	gstate->pending.stroke.miterlimit = miterlimit;
}

static void
pdf_filter_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_d)
		p->chain->op_d(ctx, p->chain, array, phase);
}

static void
pdf_filter_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_ri)
		p->chain->op_ri(ctx, p->chain, intent);
}

static void
pdf_filter_gs_OP(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_OP)
		p->chain->op_gs_OP(ctx, p->chain, b);
}

static void
pdf_filter_gs_op(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_op)
		p->chain->op_gs_op(ctx, p->chain, b);
}

static void
pdf_filter_gs_OPM(fz_context *ctx, pdf_processor *proc, int i)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_OPM)
		p->chain->op_gs_OPM(ctx, p->chain, i);
}

static void
pdf_filter_gs_UseBlackPtComp(fz_context *ctx, pdf_processor *proc, pdf_obj *name)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_gs_UseBlackPtComp)
		p->chain->op_gs_UseBlackPtComp(ctx, p->chain, name);
}

static void
pdf_filter_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_i)
		p->chain->op_i(ctx, p->chain, flatness);
}

static void
pdf_filter_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_gs_begin)
		p->chain->op_gs_begin(ctx, p->chain, name, extgstate);
	copy_resource(ctx, p, PDF_NAME(ExtGState), name);
}

static void
pdf_filter_gs_BM(fz_context *ctx, pdf_processor *proc, const char *blendmode)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	if (p->chain->op_gs_BM)
		p->chain->op_gs_BM(ctx, p->chain, blendmode);
}

static void
pdf_filter_gs_CA(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	if (p->chain->op_gs_CA)
		p->chain->op_gs_CA(ctx, p->chain, alpha);
}

static void
pdf_filter_gs_ca(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	if (p->chain->op_gs_ca)
		p->chain->op_gs_ca(ctx, p->chain, alpha);
}

static void
pdf_filter_gs_SMask(fz_context *ctx, pdf_processor *proc, pdf_obj *smask, pdf_obj *page_resources, float *bc, int luminosity)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	if (p->chain->op_gs_SMask)
		p->chain->op_gs_SMask(ctx, p->chain, smask, page_resources, bc, luminosity);
}

static void
pdf_filter_gs_end(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	if (p->chain->op_gs_end)
		p->chain->op_gs_end(ctx, p->chain);
}

/* special graphics state */

static void
pdf_filter_q(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_push(ctx, p);
}

static void
pdf_filter_Q(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_TEXT);
	if (p->in_BT)
	{
		if (p->chain->op_ET)
			p->chain->op_ET(ctx, p->chain);
		p->in_BT = 0;
		p->BT_pending = 1;
	}
	filter_pop(ctx, p);
}

static void
pdf_filter_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	fz_matrix ctm;

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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_m)
		p->chain->op_m(ctx, p->chain, x, y);
}

static void
pdf_filter_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_l)
		p->chain->op_l(ctx, p->chain, x, y);
}

static void
pdf_filter_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_c)
		p->chain->op_c(ctx, p->chain, x1, y1, x2, y2, x3, y3);
}

static void
pdf_filter_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_v)
		p->chain->op_v(ctx, p->chain, x2, y2, x3, y3);
}

static void
pdf_filter_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_y)
		p->chain->op_y(ctx, p->chain, x1, y1, x3, y3);
}

static void
pdf_filter_h(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_h)
		p->chain->op_h(ctx, p->chain);
}

static void
pdf_filter_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_re)
		p->chain->op_re(ctx, p->chain, x, y, w, h);
}

/* path painting */

static void
pdf_filter_S(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_STROKE);
	if (p->chain->op_S)
		p->chain->op_S(ctx, p->chain);
}

static void
pdf_filter_s(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_STROKE);
	if (p->chain->op_s)
		p->chain->op_s(ctx, p->chain);
}

static void
pdf_filter_F(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_FILL);
	if (p->chain->op_F)
		p->chain->op_F(ctx, p->chain);
}

static void
pdf_filter_f(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_FILL);
	if (p->chain->op_f)
		p->chain->op_f(ctx, p->chain);
}

static void
pdf_filter_fstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_FILL);
	if (p->chain->op_fstar)
		p->chain->op_fstar(ctx, p->chain);
}

static void
pdf_filter_B(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_B)
		p->chain->op_B(ctx, p->chain);
}

static void
pdf_filter_Bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_Bstar)
		p->chain->op_Bstar(ctx, p->chain);
}

static void
pdf_filter_b(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_b)
		p->chain->op_b(ctx, p->chain);
}

static void
pdf_filter_bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_bstar)
		p->chain->op_bstar(ctx, p->chain);
}

static void
pdf_filter_n(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_n)
		p->chain->op_n(ctx, p->chain);
}

/* clipping paths */

static void
pdf_filter_W(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_W)
		p->chain->op_W(ctx, p->chain);
}

static void
pdf_filter_Wstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_CTM);
	if (p->chain->op_Wstar)
		p->chain->op_Wstar(ctx, p->chain);
}

/* text objects */

static void
pdf_filter_BT(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->tos.tm = fz_identity;
	p->tos.tlm = fz_identity;
	p->BT_pending = 1;
}

static void
pdf_filter_ET(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;

	if (!p->BT_pending)
	{
		filter_flush(ctx, p, 0);
		if (p->chain->op_ET)
			p->chain->op_ET(ctx, p->chain);
		p->in_BT = 0;
	}
	p->BT_pending = 0;
	if (p->filter->after_text_object)
	{
		fz_matrix ctm;
		ctm = fz_concat(p->gstate->pending.ctm, p->gstate->sent.ctm);
		ctm = fz_concat(ctm, p->transform);
		if (p->chain->op_q)
			p->chain->op_q(ctx, p->chain);
		p->filter->after_text_object(ctx, p->filter->opaque, p->doc, p->chain, ctm);
		if (p->chain->op_Q)
			p->chain->op_Q(ctx, p->chain);
	}
}

/* text state */

static void
pdf_filter_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->gstate->pending.text.char_space = charspace;
}

static void
pdf_filter_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->gstate->pending.text.word_space = wordspace;
}

static void
pdf_filter_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	/* scale is as written in the file. It is 100 times smaller
	 * in the gstate. */
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->gstate->pending.text.scale = scale / 100;
}

static void
pdf_filter_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->gstate->pending.text.leading = leading;
}

static void
pdf_filter_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->gstate->pending.text.render = render;
}

static void
pdf_filter_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	p->gstate->pending.text.rise = rise;
}

/* text positioning */

static void
pdf_filter_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	p->Tm_adjust = 0;
	pdf_tos_translate(&p->tos, tx, ty);
	p->Tm_pending = 1;
}

static void
pdf_filter_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	p->Tm_adjust = 0;
	pdf_tos_translate(&p->tos, tx, ty);
	p->gstate->pending.text.leading = -ty;
	p->Tm_pending = 1;
}

static void
pdf_filter_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	pdf_tos_set_matrix(&p->tos, a, b, c, d, e, f);
	p->Tm_pending = 1;
	p->Tm_adjust = 0;
}

static void
pdf_filter_Tstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_show_text(ctx, p, array);
}

static void
pdf_filter_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_show_string(ctx, p, (unsigned char *)str, len);
}

static void
pdf_filter_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	/* Note, we convert all T' operators to (maybe) a T* and a Tj */
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_d0)
		p->chain->op_d0(ctx, p->chain, wx, wy);
}

static void
pdf_filter_d1(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_d1)
		p->chain->op_d1(ctx, p->chain, wx, wy, llx, lly, urx, ury);
}

/* color */

static void
pdf_filter_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	fz_strlcpy(gstate->pending.CS.name, name, sizeof gstate->pending.CS.name);
	gstate->pending.CS.cs = cs;
	copy_resource(ctx, p, PDF_NAME(ColorSpace), name);
}

static void
pdf_filter_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	fz_strlcpy(gstate->pending.cs.name, name, sizeof gstate->pending.cs.name);
	gstate->pending.cs.cs = cs;
	copy_resource(ctx, p, PDF_NAME(ColorSpace), name);
}

static void
pdf_filter_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	fz_strlcpy(gstate->pending.SC.name, name, sizeof gstate->pending.SC.name);
	gstate->pending.SC.pat = NULL;
	gstate->pending.SC.shd = shade;
	gstate->pending.SC.n = 0;
	copy_resource(ctx, p, PDF_NAME(Pattern), name);
}

static void
pdf_filter_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	fz_strlcpy(gstate->pending.sc.name, name, sizeof gstate->pending.sc.name);
	gstate->pending.sc.pat = NULL;
	gstate->pending.sc.shd = shade;
	gstate->pending.sc.n = 0;
	copy_resource(ctx, p, PDF_NAME(Pattern), name);
}

static void
pdf_filter_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_gstate *gstate = gstate_to_update(ctx, p);
	int i;
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
	float color[1] = { g };
	pdf_filter_CS(ctx, proc, "DeviceGray", fz_device_gray(ctx));
	pdf_filter_SC_color(ctx, proc, 1, color);
}

static void
pdf_filter_g(fz_context *ctx, pdf_processor *proc, float g)
{
	float color[1] = { g };
	pdf_filter_cs(ctx, proc, "DeviceGray", fz_device_gray(ctx));
	pdf_filter_sc_color(ctx, proc, 1, color);
}

static void
pdf_filter_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	float color[3] = { r, g, b };
	pdf_filter_CS(ctx, proc, "DeviceRGB", fz_device_rgb(ctx));
	pdf_filter_SC_color(ctx, proc, 3, color);
}

static void
pdf_filter_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	float color[3] = { r, g, b };
	pdf_filter_cs(ctx, proc, "DeviceRGB", fz_device_rgb(ctx));
	pdf_filter_sc_color(ctx, proc, 3, color);
}

static void
pdf_filter_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	float color[4] = { c, m, y, k };
	pdf_filter_CS(ctx, proc, "DeviceCMYK", fz_device_cmyk(ctx));
	pdf_filter_SC_color(ctx, proc, 4, color);
}

static void
pdf_filter_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	float color[4] = { c, m, y, k };
	pdf_filter_cs(ctx, proc, "DeviceCMYK", fz_device_cmyk(ctx));
	pdf_filter_sc_color(ctx, proc, 4, color);
}

/* shadings, images, xobjects */

static void
pdf_filter_BI(fz_context *ctx, pdf_processor *proc, fz_image *image, const char *colorspace)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	fz_matrix ctm;
	filter_flush(ctx, p, FLUSH_ALL);
	ctm = fz_concat(p->gstate->sent.ctm, p->transform);
	if (p->filter->image_filter && p->filter->image_filter(ctx, p->filter->opaque, ctm, "<inline>", image))
		return;
	if (p->chain->op_BI)
		p->chain->op_BI(ctx, p->chain, image, colorspace);
}

static void
pdf_filter_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_ALL);
	if (p->chain->op_sh)
		p->chain->op_sh(ctx, p->chain, name, shade);
	copy_resource(ctx, p, PDF_NAME(Shading), name);
}

static void
pdf_filter_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	fz_matrix ctm;
	filter_flush(ctx, p, FLUSH_ALL);
	ctm = fz_concat(p->gstate->sent.ctm, p->transform);
	if (p->filter->image_filter && p->filter->image_filter(ctx, p->filter->opaque, ctm, name, image))
		return;
	if (p->filter->instance_forms)
	{
		/* Make up a unique name when instancing forms so we don't accidentally clash. */
		char buf[40];
		pdf_obj *obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->old_rdb, PDF_NAME(XObject)), name);
		fz_snprintf(buf, sizeof buf, "Im%d", p->image_id++);
		add_resource(ctx, p, PDF_NAME(XObject), buf, obj);
		if (p->chain->op_Do_image)
			p->chain->op_Do_image(ctx, p->chain, buf, image);
	}
	else
	{
		copy_resource(ctx, p, PDF_NAME(XObject), name);
	if (p->chain->op_Do_image)
		p->chain->op_Do_image(ctx, p->chain, name, image);
	}
}

static void
pdf_filter_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj, pdf_obj *page_resources)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	fz_matrix transform;
	filter_flush(ctx, p, FLUSH_ALL);

	if (p->filter->instance_forms)
	{
		/* Copy an instance of the form with a new unique name. */
		pdf_obj *new_xobj;
		char buf[40];
		fz_snprintf(buf, sizeof buf, "Fm%d", p->form_id++);
		transform = fz_concat(p->gstate->sent.ctm, p->transform);
		new_xobj = pdf_filter_xobject_instance(ctx, xobj, page_resources, transform, p->filter);
		fz_try(ctx)
		{
			add_resource(ctx, p, PDF_NAME(XObject), buf, new_xobj);
			if (p->chain->op_Do_form)
				p->chain->op_Do_form(ctx, p->chain, buf, new_xobj, page_resources);
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
		p->chain->op_Do_form(ctx, p->chain, name, xobj, page_resources);
	}
}

/* marked content */

static void
pdf_filter_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_MP)
		p->chain->op_MP(ctx, p->chain, tag);
}

static void
pdf_filter_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
pop_tag(fz_context *ctx, pdf_filter_processor *p, tag_record **tags)
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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;

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
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_BX)
		p->chain->op_BX(ctx, p->chain);
}

static void
pdf_filter_EX(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, 0);
	if (p->chain->op_EX)
		p->chain->op_EX(ctx, p->chain);
}

static void
pdf_filter_END(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	filter_flush(ctx, p, FLUSH_TEXT);
	if (p->chain->op_END)
		p->chain->op_END(ctx, p->chain);
}

static void
pdf_close_filter_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
	while (!filter_pop(ctx, p))
	{
		/* Nothing to do in the loop, all work done above */
	}
}

static void
pdf_drop_filter_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_filter_processor *p = (pdf_filter_processor*)proc;
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
}

/*
	Create a filter processor. This
	filters the PDF operators it is fed, and passes them down
	(with some changes) to the child filter.

	The changes made by the filter are:

	* No operations are allowed to change the top level gstate.
	Additional q/Q operators are inserted to prevent this.

	* Repeated/unnecessary colour operators are removed (so,
	for example, "0 0 0 rg 0 1 rg 0.5 g" would be sanitised to
	"0.5 g")

	The intention of these changes is to provide a simpler,
	but equivalent stream, repairing problems with mismatched
	operators, maintaining structure (such as BMC, EMC calls)
	and leaving the graphics state in an known (default) state
	so that subsequent operations (such as synthesising new
	operators to be appended to the stream) are easier.

	The net graphical effect of the filtered operator stream
	should be identical to the incoming operator stream.

	chain: The child processor to which the filtered operators
	will be fed.

	old_res: The incoming resource dictionary.

	new_res: An (initially empty) resource dictionary that will
	be populated by copying entries from the old dictionary to
	the new one as they are used. At the end therefore, this
	contains exactly those resource objects actually required.

	The filter options struct allows you to filter objects using callbacks.
*/
pdf_processor *
pdf_new_filter_processor(
	fz_context *ctx,
	pdf_document *doc,
	pdf_processor *chain,
	pdf_obj *old_rdb,
	pdf_obj *new_rdb,
	int structparents,
	fz_matrix transform,
	pdf_filter_options *filter)
{
	pdf_filter_processor *proc = pdf_new_processor(ctx, sizeof *proc);
	{
		proc->super.close_processor = pdf_close_filter_processor;
		proc->super.drop_processor = pdf_drop_filter_processor;

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
	}

	proc->doc = pdf_keep_document(ctx, doc);
	proc->structparents = structparents;
	if (structparents != -1)
		proc->structarray = pdf_keep_obj(ctx, pdf_lookup_number(ctx, pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/StructTreeRoot/ParentTree"), structparents));
	proc->chain = chain;
	proc->old_rdb = old_rdb;
	proc->new_rdb = new_rdb;
	proc->filter = filter;
	proc->transform = transform;
	proc->form_id = 1;
	proc->image_id = 1;

	fz_try(ctx)
	{
		proc->gstate = fz_malloc_struct(ctx, filter_gstate);
		proc->gstate->pending.ctm = fz_identity;
		proc->gstate->sent.ctm = fz_identity;
		proc->gstate->pending.text.scale = 1;
		proc->gstate->pending.text.size = -1;
		proc->gstate->sent.text.scale = 1;
		proc->gstate->sent.text.size = -1;
	}
	fz_catch(ctx)
	{
		pdf_drop_processor(ctx, (pdf_processor *) proc);
		fz_rethrow(ctx);
	}

	return (pdf_processor*)proc;
}
