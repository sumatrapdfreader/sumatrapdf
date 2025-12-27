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

/* Define the following for some debugging output. */
#undef DEBUG_SUBSETTING

typedef struct gstate
{
	struct gstate *next;
	int current_font;
	pdf_font_desc *font;
} gstate;

typedef struct
{
	/* We have one of these records for each fontfile. */
	int num;
	int gen;
	int is_ttf;
	int is_cidfont;
	pdf_obj *fontfile;
	unsigned char digest[16];

	fz_int_heap gids;
	fz_int_heap cids;

	/* Pointers back to the top level fonts that refer to this. */
	int max;
	int len;
	pdf_obj **font;
} font_usage_t;

typedef struct
{
	int max;
	int len;
	font_usage_t *font;
} fonts_usage_t;

typedef struct
{
	pdf_processor super;
	fonts_usage_t *usage;
	gstate *gs;
} pdf_font_analysis_processor;

static void
pop_gstate(fz_context *ctx, pdf_font_analysis_processor *p)
{
	gstate *gs = p->gs;
	gstate *old;

	if (gs == NULL)
		return;

	old = gs->next;
	pdf_drop_font(ctx, gs->font);
	fz_free(ctx, gs);
	p->gs = old;
}

static void
drop_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;

	while (p->gs)
		pop_gstate(ctx, p);
}

static void
font_analysis_Q(fz_context *ctx, pdf_processor *proc)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;

	pop_gstate(ctx, p);
}

static void
font_analysis_q(fz_context *ctx, pdf_processor *proc)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;
	gstate *gs = p->gs;
	gstate *new_gs = fz_malloc_struct(ctx, gstate);
	p->gs = new_gs;

	if (gs)
	{
		*new_gs = *gs;
		new_gs->next = gs;
	}

	pdf_keep_font(ctx, new_gs->font);

}

static void
font_analysis_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;
	pdf_obj *dict = pdf_lookup_resource(ctx, proc->rstack, PDF_NAME(Font), name);
	pdf_obj *subtype, *fontdesc;
	pdf_obj *fontfile = NULL;
	pdf_obj *key;
	int num, gen, i;
	int is_cidfont = 0;
	int is_ttf = 0;
	unsigned char digest[16];

	p->gs->current_font = -1; /* unknown font! */

	if (dict == NULL)
		return;

	/* We can have multiple fonts that rely on the same underlying fontfile
	 * object. Therefore, resolve down to that. */
	subtype = pdf_dict_get(ctx, dict, PDF_NAME(Subtype));

	if (subtype == PDF_NAME(Type1) || subtype == PDF_NAME(MMType1))
	{
		// fontfile subtype should be Type1C for us to be able to subset it
		key = PDF_NAME(FontFile);
		fontdesc = pdf_dict_get(ctx, dict, PDF_NAME(FontDescriptor));
		fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile));
		is_cidfont = 0;
		is_ttf = 0;
	}
	else if (subtype == PDF_NAME(TrueType))
	{
		key = PDF_NAME(FontFile2);
		fontdesc = pdf_dict_get(ctx, dict, PDF_NAME(FontDescriptor));
		fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile2));
		is_cidfont = 0;
		is_ttf = 1;
	}
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type0)))
	{
		/* In this case, we keep dict pointing to the parent font, not the descendant font. */
		pdf_obj *ddict = pdf_array_get(ctx, pdf_dict_get(ctx, dict, PDF_NAME(DescendantFonts)), 0);
		subtype = pdf_dict_get(ctx, ddict, PDF_NAME(Subtype));
		fontdesc = pdf_dict_get(ctx, ddict, PDF_NAME(FontDescriptor));
		if (subtype == PDF_NAME(CIDFontType0))
		{
			// fontfile subtype is either CIDFontType0C or OpenType
			key = PDF_NAME(FontFile3);
			fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile3));
			subtype = pdf_dict_get(ctx, fontfile, PDF_NAME(Subtype));
			if (subtype == PDF_NAME(CIDFontType0C))
			{
				is_cidfont = 1;
				is_ttf = 0;
			}
			else if (subtype == PDF_NAME(OpenType))
			{
				is_cidfont = 1;
				is_ttf = 1;
			}
			else
			{
				fontfile = NULL;
			}
		}
		else if (subtype == PDF_NAME(CIDFontType2))
		{
			key = PDF_NAME(FontFile2);
			fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile2));
			is_cidfont = 1;
			is_ttf = 1;
		}
	}

	if (!fontfile)
	{
#ifdef DEBUG_SUBSETTING
		fz_write_printf(ctx, fz_stddbg(ctx), "No embedded file found for font of subtype %s\n", pdf_to_name(ctx, subtype));
#endif
		return;
	}

	num = pdf_to_num(ctx, fontfile);
	gen = pdf_to_gen(ctx, fontfile);

	for (i = 0; i < p->usage->len; i++)
	{
		if (p->usage->font[i].num == num &&
			p->usage->font[i].gen == gen)
			break;
	}

	fz_font_digest(ctx, font->font, digest);

	/* Check for duplicate fonts. (Fonts in the document that have
	 * the font stream included multiple times as different objects).
	 * This can happen with naive insertion routines. */
	if (i == p->usage->len)
	{
		for (i = 0; i < p->usage->len; i++)
		{
			if (memcmp(digest, p->usage->font[i].digest, 16) == 0)
			{
				pdf_dict_put(ctx, fontdesc, key, p->usage->font[i].fontfile);
				break;
			}
		}
	}

	pdf_drop_font(ctx, p->gs->font);
	p->gs->font = pdf_keep_font(ctx, font);
	p->gs->current_font = i;
	if (i < p->usage->len)
	{
		int j;

		for (j = 0; j < p->usage->font[i].len; j++)
		{
			if (pdf_objcmp(ctx, p->usage->font[i].font[j], dict) == 0)
				return;
		}

		if (p->usage->font[i].len == p->usage->font[i].max)
		{
			int newmax = p->usage->font[i].max * 2;
			p->usage->font[i].font = fz_realloc(ctx, p->usage->font[i].font, sizeof(*p->usage->font[i].font) * newmax);
			p->usage->font[i].max = newmax;
		}
		p->usage->font[i].font[j] = pdf_keep_obj(ctx, dict);
		p->usage->font[i].len++;

		return;
	}

	if (p->usage->max == p->usage->len)
	{
		int n = p->usage->max * 2;

		if (n == 0)
			n = 32;
		p->usage->font = (font_usage_t *)fz_realloc(ctx, p->usage->font, sizeof(*p->usage->font) * n);
		p->usage->max = n;
	}

	p->usage->font[i].is_ttf = is_ttf;
	p->usage->font[i].is_cidfont = is_cidfont;
	p->usage->font[i].fontfile = pdf_keep_obj(ctx, fontfile);
	p->usage->font[i].num = num;
	p->usage->font[i].gen = gen;
	p->usage->font[i].cids.len = 0;
	p->usage->font[i].cids.max = 0;
	p->usage->font[i].cids.heap = NULL;
	p->usage->font[i].gids.len = 0;
	p->usage->font[i].gids.max = 0;
	p->usage->font[i].gids.heap = NULL;
	p->usage->font[i].len = 0;
	p->usage->font[i].max = 0;
	p->usage->font[i].font = NULL;
	memcpy(p->usage->font[i].digest, digest, 16);
	p->usage->len++;

	p->usage->font[i].font = fz_malloc(ctx, sizeof(*p->usage->font[i].font) * 4);
	p->usage->font[i].len = 1;
	p->usage->font[i].max = 4;
	p->usage->font[i].font[0] = pdf_keep_obj(ctx, dict);
}

static void
show_char(fz_context *ctx, font_usage_t *font, int cid, int gid)
{
	fz_int_heap_insert(ctx, &font->cids, cid);
	fz_int_heap_insert(ctx, &font->gids, gid);
}

static void
show_string(fz_context *ctx, pdf_font_analysis_processor *p, unsigned char *buf, size_t len)
{
	gstate *gs = p->gs;
	pdf_font_desc *fontdesc = gs->font;
	size_t pos = 0;
	font_usage_t *font;

	// Not an embedded font!
	if (gs->current_font < 0 || fontdesc == NULL)
		return;

	font = &p->usage->font[gs->current_font];

	while (pos < len)
	{
		unsigned int cpt;
		int inc = pdf_decode_cmap(fontdesc->encoding, &buf[pos], &buf[len], &cpt);

		int cid = pdf_lookup_cmap(fontdesc->encoding, cpt);
		if (cid >= 0)
		{
			int gid = pdf_font_cid_to_gid(ctx, fontdesc, cid);
			show_char(ctx, font, cid, gid);
		}

		pos += inc;
	}
}

static void
show_text(fz_context *ctx, pdf_font_analysis_processor *p, pdf_obj *text)
{
	gstate *gs = p->gs;
	pdf_font_desc *fontdesc;
	int i, n;

	if (!gs)
		return;
	fontdesc = gs->font;
	if (!fontdesc)
		return;

	if (pdf_is_string(ctx, text))
	{
		show_string(ctx, p, (unsigned char *)pdf_to_str_buf(ctx, text), pdf_to_str_len(ctx, text));
	}
	else if (pdf_is_array(ctx, text))
	{
		n = pdf_array_len(ctx, text);
		for (i = 0; i < n; i++)
		{
			pdf_obj *item = pdf_array_get(ctx, text, i);
			if (pdf_is_string(ctx, item))
			{
				show_string(ctx, p, (unsigned char *)pdf_to_str_buf(ctx, item), pdf_to_str_len(ctx, item));
			}
		}
	}
}

static void
font_analysis_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;

	show_text(ctx, p, array);
}

static void
font_analysis_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;

	show_string(ctx, p, (unsigned char *)str, len);
}

static void
font_analysis_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	/* Note, we convert all T' operators to (maybe) a T* and a Tj */
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;

	show_string(ctx, p, (unsigned char *)str, len);
}

static void
font_analysis_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	/* Note, we convert all T" operators to (maybe) a T*,
	 * (maybe) Tc, (maybe) Tw and a Tj. */
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;

	show_string(ctx, p, (unsigned char*)str, len);
}

static void
font_analysis_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj)
{
	pdf_font_analysis_processor *pr = (pdf_font_analysis_processor *)proc;
	pdf_document *doc = pdf_get_bound_document(ctx, xobj);
	pdf_obj *resources = pdf_xobject_resources(ctx, xobj);
	pdf_process_contents(ctx, (pdf_processor*)pr, doc, resources, xobj, NULL, NULL);
}

static pdf_processor *
pdf_new_font_analysis_processor(fz_context *ctx, fonts_usage_t *usage)
{
	pdf_font_analysis_processor *proc = (pdf_font_analysis_processor *)pdf_new_processor(ctx, sizeof *proc);

	proc->super.drop_processor = drop_processor;

	proc->super.op_Do_form = font_analysis_Do_form;

	proc->super.op_Tf = font_analysis_Tf;
	proc->super.op_Tj = font_analysis_Tj;
	proc->super.op_TJ = font_analysis_TJ;
	proc->super.op_squote = font_analysis_squote;
	proc->super.op_dquote = font_analysis_dquote;

	proc->super.op_q = font_analysis_q;
	proc->super.op_Q = font_analysis_Q;

	fz_try(ctx)
		proc->gs = fz_malloc_struct(ctx, gstate);
	fz_catch(ctx)
	{
		fz_free(ctx, proc);
		fz_rethrow(ctx);
	}

	proc->gs->current_font = -1; // no font set yet

	proc->usage = usage;

	return &proc->super;
}

static void
examine_page(fz_context *ctx, pdf_document *doc, pdf_page *page, fonts_usage_t *usage)
{
	pdf_processor *proc = pdf_new_font_analysis_processor(ctx, usage);
	pdf_obj *contents = pdf_page_contents(ctx, page);
	pdf_obj *resources = pdf_page_resources(ctx, page);
	pdf_annot *annot, *widget;

	fz_try(ctx)
	{
		pdf_process_contents(ctx, proc, doc, resources, contents, NULL, NULL);

		pdf_processor_push_resources(ctx, proc, resources);
		for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
			pdf_process_annot(ctx, proc, annot, NULL);
		for (widget = pdf_first_widget(ctx, page); widget; widget = pdf_next_widget(ctx, widget))
			pdf_process_annot(ctx, proc, widget, NULL);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
	{
		pdf_drop_processor(ctx, proc);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
subset_ttf(fz_context *ctx, pdf_document *doc, font_usage_t *font, pdf_obj *fontfile, int symbolic, int cidfont)
{
	fz_buffer *buf = pdf_load_stream(ctx, fontfile);
	fz_buffer *newbuf = NULL;

	if (buf->len == 0)
	{
		fz_drop_buffer(ctx, buf);
		return;
	}

	fz_var(newbuf);

	fz_try(ctx)
	{
		newbuf = fz_subset_ttf_for_gids(ctx, buf, font->gids.heap, font->gids.len, symbolic, cidfont);

		pdf_update_stream(ctx, doc, fontfile, newbuf, 0);
		pdf_dict_put_int(ctx, fontfile, PDF_NAME(Length1), newbuf->len);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, newbuf);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
subset_cff(fz_context *ctx, pdf_document *doc, font_usage_t *font, pdf_obj *fontfile, int symbolic, int cidfont)
{
	fz_buffer *buf = pdf_load_stream(ctx, fontfile);
	fz_buffer *newbuf = NULL;

	if (buf->len == 0)
	{
		fz_drop_buffer(ctx, buf);
		return;
	}

	fz_var(newbuf);

	fz_try(ctx)
	{
		newbuf = fz_subset_cff_for_gids(ctx, buf, font->gids.heap, font->gids.len, symbolic, cidfont);

		pdf_update_stream(ctx, doc, fontfile, newbuf, 0);
		pdf_dict_put_int(ctx, fontfile, PDF_NAME(Length1), newbuf->len);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, newbuf);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
do_adjust_simple_font(fz_context *ctx, pdf_document *doc, font_usage_t *font, int n)
{
	pdf_obj *obj = font->font[n];
	int old_firstchar = pdf_dict_get_int(ctx, obj, PDF_NAME(FirstChar));
	pdf_obj *old_widths = pdf_dict_get(ctx, obj, PDF_NAME(Widths));
	int new_firstchar = font->cids.heap[0];
	int new_lastchar = font->cids.heap[font->cids.len-1];
	pdf_obj *widths;
	int i;

	pdf_dict_put_int(ctx, obj, PDF_NAME(FirstChar), new_firstchar);
	pdf_dict_put_int(ctx, obj, PDF_NAME(LastChar), new_lastchar);
	if (old_widths)
	{
		int j = 0;
		widths = pdf_new_array(ctx, doc, new_lastchar - new_firstchar + 1);
		for (i = new_firstchar; i <= new_lastchar; i++)
		{
			if (font->cids.heap[j] == i)
			{
				pdf_array_push_int(ctx, widths, pdf_array_get_int(ctx, old_widths, i - old_firstchar));
				j++;
			}
			else
				pdf_array_push_int(ctx, widths, 0);
		}
		pdf_dict_put_drop(ctx, obj, PDF_NAME(Widths), widths);
	}
}

static void
adjust_simple_font(fz_context *ctx, pdf_document *doc, font_usage_t *font)
{
	int i;

	for (i = 0; i < font->len; i++)
		do_adjust_simple_font(ctx, doc, font, i);
}


static pdf_obj *
get_fontdesc(fz_context *ctx, pdf_obj *font)
{
	pdf_obj *fontdesc = pdf_dict_get(ctx, font, PDF_NAME(FontDescriptor));

	if (fontdesc)
		return fontdesc;

	return pdf_dict_get(ctx, pdf_array_get(ctx, pdf_dict_get(ctx, font, PDF_NAME(DescendantFonts)), 0), PDF_NAME(FontDescriptor));
}

static void
prefix_font_name(fz_context *ctx, pdf_document *doc, pdf_obj *font, pdf_obj *file)
{
	fz_buffer *buf;
	uint32_t digest[4], v;
	pdf_obj *descendant;
	pdf_obj *fontdesc;
	const char *fontdesc_name;
	char new_name[256];
	size_t len;

	descendant = pdf_array_get(ctx, pdf_dict_get(ctx, font, PDF_NAME(DescendantFonts)), 0);
	if (descendant)
		fontdesc = get_fontdesc(ctx, descendant);
	else
		fontdesc = get_fontdesc(ctx, font);
	fontdesc_name = pdf_dict_get_name(ctx, fontdesc, PDF_NAME(FontName));

	buf = pdf_load_stream(ctx, file);
	fz_md5_buffer(ctx, buf, (uint8_t *)digest);
	fz_drop_buffer(ctx, buf);

	len = fontdesc_name == NULL ? 0 : strlen(fontdesc_name);
	if (len >= 6 && fontdesc_name[6] == '+')
	{
		/* Already prefixed. */
		memcpy(new_name, fontdesc_name, len > sizeof(new_name)-1 ? sizeof(new_name)-1 : len+1);
	}
	else
	{
		/* Invent a prefix */
	v = digest[0] ^ digest[1] ^ digest[2] ^ digest[3];
	new_name[0] = 'A' + (v % 26);
	v /= 26;
	new_name[1] = 'A' + (v % 26);
	v /= 26;
	new_name[2] = 'A' + (v % 26);
	v /= 26;
	new_name[3] = 'A' + (v % 26);
	v /= 26;
	new_name[4] = 'A' + (v % 26);
	v /= 26;
	new_name[5] = 'A' + (v % 26);
	new_name[6] = '+';

		if (fontdesc_name)
			memcpy(new_name+7, fontdesc_name, len > sizeof(new_name)-8 ? sizeof(new_name)-8 : len+1);
		else
			memcpy(new_name+7, "Anonymous", 10);
	new_name[sizeof(new_name)-1] = 0;

	pdf_dict_put_name(ctx, fontdesc, PDF_NAME(FontName), new_name);
	}

	/* Always set font_name to the same as fontdesc. */
	pdf_dict_put_name(ctx, font, PDF_NAME(BaseFont), new_name);

	if (descendant)
		pdf_dict_put_name(ctx, descendant, PDF_NAME(BaseFont), new_name);
}

static int
get_symbolic(fz_context *ctx, font_usage_t *font)
{
	int i, flags, symbolic, symbolic2;
	pdf_obj *fontdesc;

	if (!font || font->len == 0)
		return 0;

	fontdesc = pdf_dict_get(ctx, font->font[0], PDF_NAME(FontDescriptor));
	flags = pdf_dict_get_int(ctx, fontdesc, PDF_NAME(Flags));
	symbolic = (!!(flags & 4)) | ((flags & 32) == 0);

	for (i = 1; i < font->len; i++)
	{
		fontdesc = pdf_dict_get(ctx, font->font[i], PDF_NAME(FontDescriptor));
		flags = pdf_dict_get_int(ctx, fontdesc, PDF_NAME(Flags));
		symbolic2 = (!!(flags & 4)) | ((flags & 32) == 0);

		if (symbolic != symbolic2)
		{
			fz_warn(ctx, "Font cannot be both symbolic and non-symbolic. Skipping subsetting.");
			return -1;
		}
	}

	return symbolic;
}

static pdf_obj *get_subtype(fz_context *ctx, font_usage_t *font)
{
	/* If we can get the subtype from the fontfile, great. Use that. */
	pdf_obj *subtype = pdf_dict_get(ctx, font->fontfile, PDF_NAME(Subtype));
	int i;

	if (subtype != NULL)
		return subtype;

	/* Otherwise we'll have to get it from the font objects, and they'd
	 * all better agree. */
	if (font->len == 0)
		return NULL;

	subtype = pdf_dict_get(ctx, font->font[0], PDF_NAME(Subtype));

	for (i = 1; i < font->len; i++)
	{
		pdf_obj *subtype2 = pdf_dict_get(ctx, font->font[i], PDF_NAME(Subtype));

		if (pdf_objcmp(ctx, subtype, subtype2))
			return NULL;
	}
	return subtype;
}

void
pdf_subset_fonts(fz_context *ctx, pdf_document *doc, int len, const int *pages)
{
	int i, j;
	pdf_page *page = NULL;
	fonts_usage_t usage = { 0 };

	fz_var(page);

	fz_try(ctx)
	{
		if (len == 0)
		{
			/* Process every page. */
			len = pdf_count_pages(ctx, doc);
			for (i = 0; i < len; i++)
			{
				page = pdf_load_page(ctx, doc, i);

				examine_page(ctx, doc, page, &usage);

				fz_drop_page(ctx, (fz_page *)page);
				page = NULL;
			}
		}
		else
		{
			/* Process just the pages we are given. */
			for (i = 0; i < len; i++)
			{
				page = pdf_load_page(ctx, doc, pages[i]);

				examine_page(ctx, doc, page, &usage);

				fz_drop_page(ctx, (fz_page *)page);
				page = NULL;
			}
		}

		/* All our font usage data is in heaps. Sort the heaps. */
		for (i = 0; i < usage.len; i++)
		{
			font_usage_t *font = &usage.font[i];

			fz_int_heap_sort(ctx, &font->cids);
			fz_int_heap_uniq(ctx, &font->cids);
			fz_int_heap_sort(ctx, &font->gids);
			fz_int_heap_uniq(ctx, &font->gids);
		}

		/* Now, actually subset the fonts. */
		for (i = 0; i < usage.len; i++)
		{
			font_usage_t *font = &usage.font[i];
			pdf_obj *subtype = get_subtype(ctx, font);
			int symbolic = get_symbolic(ctx, font);
			if (symbolic < 0)
				continue;

			/* Not sure this can ever happen, and if it does this is not a great
			 * way to handle it, but it'll do for now. */
			if (font->gids.len == 0 || font->cids.len == 0 || subtype == NULL)
				continue;

#ifdef DEBUG_SUBSETTING
			fz_write_printf(ctx, fz_stddbg(ctx), "font->obj=%d  subtype=", pdf_to_num(ctx, font->fontfile));
			pdf_debug_obj(ctx, subtype);
			fz_write_printf(ctx, fz_stddbg(ctx), "\n");
			pdf_debug_obj(ctx, pdf_dict_get(ctx, font->font[0], PDF_NAME(FontDescriptor)));
#endif

			/* If we hit a (non-SYSTEM) problem subsetting a font, give up for this font alone.
			 * This will leave this font alone. */
			fz_try(ctx)
			{
				if (font->is_ttf)
					subset_ttf(ctx, doc, font, font->fontfile, symbolic, font->is_cidfont);
				else if (font->is_cidfont)
					subset_cff(ctx, doc, font, font->fontfile, symbolic, font->is_cidfont);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
				fz_report_error(ctx);
				continue;
			}

			/* Any problems changing these parts of the fonts are really fatal though. */
			if (pdf_name_eq(ctx, subtype, PDF_NAME(TrueType)) ||
				pdf_name_eq(ctx, subtype, PDF_NAME(Type1)))
			{
				adjust_simple_font(ctx, doc, font);
			}

			/* And prefix the name */
			for (j = 0; j < font->len; j++)
				prefix_font_name(ctx, doc, font->font[j], font->fontfile);
		}
	}
	fz_always(ctx)
	{
			fz_drop_page(ctx, (fz_page *)page);

			for (i = 0; i < usage.len; i++)
			{
				pdf_drop_obj(ctx, usage.font[i].fontfile);
				fz_free(ctx, usage.font[i].cids.heap);
				fz_free(ctx, usage.font[i].gids.heap);
				for (j = 0; j < usage.font[i].len; j++)
					pdf_drop_obj(ctx, usage.font[i].font[j]);
				fz_free(ctx, usage.font[i].font);
			}
			fz_free(ctx, usage.font);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}
