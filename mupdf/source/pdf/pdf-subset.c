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

typedef struct gstate
{
	struct gstate *next;
	int current_font;
	pdf_font_desc *font;
} gstate;

typedef struct resources_stack
{
	struct resources_stack *next;
	pdf_obj *res;
} resources_stack;

typedef struct
{
	int num;
	int gen;
	pdf_obj *obj;

	fz_int_heap gids;
	fz_int_heap cids;
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
	resources_stack *rstack;
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

	while (p->rstack)
	{
		resources_stack *stk = p->rstack;
		p->rstack = stk->next;
		pdf_drop_obj(ctx, stk->res);
		fz_free(ctx, stk);
	}

	while (p->gs)
		pop_gstate(ctx, p);
}

static void
push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor *)proc;
	resources_stack *stk = fz_malloc_struct(ctx, resources_stack);

	stk->next = p->rstack;
	p->rstack = stk;
	fz_try(ctx)
	{
		stk->res = pdf_keep_obj(ctx, res);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, stk->res);
		p->rstack = stk->next;
		fz_free(ctx, stk);
		fz_rethrow(ctx);
	}
}

static pdf_obj *
pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor *)proc;
	resources_stack *stk = p->rstack;
	pdf_obj *res = p->rstack->res;

	p->rstack = stk->next;
	fz_free(ctx, stk);

	return res;
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
	*new_gs = *gs;
	new_gs->next = gs;
	p->gs = new_gs;

	pdf_keep_font(ctx, new_gs->font);
}

static void
font_analysis_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_font_analysis_processor *p = (pdf_font_analysis_processor*)proc;
	pdf_obj *obj = pdf_dict_gets(ctx, pdf_dict_get(ctx, p->rstack->res, PDF_NAME(Font)), name);
	pdf_obj *subtype, *fontdesc;
	pdf_obj *key = NULL;
	int num, gen, i;

	if (obj == NULL)
		return;

	/* We can have multiple fonts that rely on the same underlying fontfile
	 * object. Therefore, resolve down to that. */
	subtype = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));

	if (pdf_name_eq(ctx, subtype, PDF_NAME(TrueType)))
	{
		fontdesc = pdf_dict_get(ctx, obj, PDF_NAME(FontDescriptor));
		key = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile2));
	}
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type0)))
	{
		pdf_obj *cidfont = pdf_array_get(ctx, pdf_dict_get(ctx, obj, PDF_NAME(DescendantFonts)), 0);
		fontdesc = pdf_dict_get(ctx, cidfont, PDF_NAME(FontDescriptor));
		key = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile2));
		if (!key)
			key = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile3));
	}
	else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type1)))
	{
		fontdesc = pdf_dict_get(ctx, obj, PDF_NAME(FontDescriptor));
		key = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile3));
	}

	if (!key)
		return;

	num = pdf_to_num(ctx, key);
	gen = pdf_to_gen(ctx, key);

	for (i = 0; i < p->usage->len; i++)
	{
		if (p->usage->font[i].num == num &&
			p->usage->font[i].gen == gen)
			break;
	}

	pdf_drop_font(ctx, p->gs->font);
	p->gs->font = pdf_keep_font(ctx, font);
	p->gs->current_font = i;
	if (i < p->usage->len)
		return;

	if (p->usage->max == p->usage->len)
	{
		int n = p->usage->max * 2;

		if (n == 0)
			n = 32;
		p->usage->font = (font_usage_t *)fz_realloc(ctx, p->usage->font, sizeof(*p->usage->font) * n);
		p->usage->max = n;
	}

	p->usage->font[i].obj = pdf_keep_obj(ctx, key);
	p->usage->font[i].num = num;
	p->usage->font[i].gen = gen;
	p->usage->font[i].cids.len = 0;
	p->usage->font[i].cids.max = 0;
	p->usage->font[i].cids.heap = NULL;
	p->usage->font[i].gids.len = 0;
	p->usage->font[i].gids.max = 0;
	p->usage->font[i].gids.heap = NULL;
	p->usage->len++;
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
	font_usage_t *font = &p->usage->font[gs->current_font];

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

	if (!resources)
		resources = pr->rstack->res;

	pdf_process_contents(ctx, (pdf_processor*)pr, doc, resources, xobj, NULL, NULL);
}

static pdf_processor *
pdf_new_font_analysis_processor(fz_context *ctx, fonts_usage_t *usage)
{
	pdf_font_analysis_processor *proc = (pdf_font_analysis_processor *)pdf_new_processor(ctx, sizeof *proc);

	proc->super.drop_processor = drop_processor;
	proc->super.push_resources = push_resources;
	proc->super.pop_resources = pop_resources;

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

	proc->usage = usage;

	return &proc->super;
}

static void
examine_page(fz_context *ctx, pdf_document *doc, pdf_page *page, fonts_usage_t *usage)
{
	pdf_processor *proc = pdf_new_font_analysis_processor(ctx, usage);
	pdf_obj *contents = pdf_page_contents(ctx, page);
	pdf_obj *resources = pdf_page_resources(ctx, page);
	pdf_annot *annot;

	fz_try(ctx)
	{
		pdf_process_contents(ctx, proc, doc, resources, contents, NULL, NULL);

		for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
			pdf_process_annot(ctx, proc, annot, NULL);
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
adjust_simple_font(fz_context *ctx, pdf_document *doc, font_usage_t *font)
{
	pdf_obj *obj = font->obj;
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
prefix_font_name(fz_context *ctx, pdf_document *doc, pdf_obj *fontdesc, pdf_obj *file)
{
	fz_buffer *buf;
	uint32_t digest[4], v;
	const char *name = pdf_dict_get_name(ctx, fontdesc, PDF_NAME(FontName));
	char new_name[256];
	size_t len;

	/* If there is no name, just exit. Possibly should throw here. */
	if (name == NULL)
		return;

	len = strlen(name);
	if (len > 6 && name[6] == '+')
		return; /* Already a subset name */

	buf = pdf_load_stream(ctx, file);
	fz_md5_buffer(ctx, buf, (uint8_t *)digest);
	fz_drop_buffer(ctx, buf);

	v = digest[0] ^ digest[1] ^ digest[2] ^ digest[3];

	v = digest[0];
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

	memcpy(new_name+7, name, len > sizeof(new_name)-8 ? sizeof(new_name)-8 : len+1);
	new_name[sizeof(new_name)-1] = 0;

	pdf_dict_put_name(ctx, fontdesc, PDF_NAME(FontName), new_name);
}

void
pdf_subset_fonts(fz_context *ctx, pdf_document *doc, int len, int *pages)
{
	int i;
	pdf_page *page = NULL;
	fonts_usage_t usage = { 0 };

	fz_var(page);

	fz_try(ctx)
	{
		/* Process every page. */
		for (i = 0; i < len; i++)
		{
			page = pdf_load_page(ctx, doc, pages[i]);

			examine_page(ctx, doc, page, &usage);

			fz_drop_page(ctx, (fz_page *)page);
			page = NULL;
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
			pdf_obj *subtype = pdf_dict_get(ctx, font->obj, PDF_NAME(Subtype));

			/* Not sure this can ever happen, and if it does this is not a great
			 * way to handle it, but it'll do for now. */
			if (font->gids.len == 0 || font->cids.len == 0)
				continue;

			if (pdf_name_eq(ctx, subtype, PDF_NAME(TrueType)))
			{
				pdf_obj *fontdesc = pdf_dict_get(ctx, font->obj, PDF_NAME(FontDescriptor));
				pdf_obj *fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile2));
				int flags = pdf_dict_get_int(ctx, fontdesc, PDF_NAME(Flags));
				int symbolic = (!!(flags & 4)) | ((flags & 32) == 0);
				if (fontfile)
				{
					subset_ttf(ctx, doc, font, fontfile, symbolic, 0);
					adjust_simple_font(ctx, doc, font);
					prefix_font_name(ctx, doc, fontdesc, fontfile);
					continue;
				}
			}
			else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type0)))
			{
				pdf_obj *cidfont = pdf_array_get(ctx, pdf_dict_get(ctx, font->obj, PDF_NAME(DescendantFonts)), 0);
				pdf_obj *fontdesc = pdf_dict_get(ctx, cidfont, PDF_NAME(FontDescriptor));
				pdf_obj *fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile2));
				pdf_obj *subtype = pdf_dict_get(ctx, cidfont, PDF_NAME(Subtype));
				int flags = pdf_dict_get_int(ctx, fontdesc, PDF_NAME(Flags));
				int symbolic = (!!(flags & 4)) | ((flags & 32) == 0);
				if (fontfile)
				{
					subset_ttf(ctx, doc, font, fontfile, symbolic, 1);
					prefix_font_name(ctx, doc, fontdesc, fontfile);
					continue;
				}
				fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile3));
				if (pdf_name_eq(ctx, subtype, PDF_NAME(CIDFontType0)) && fontfile)
				{
					subtype = pdf_dict_get(ctx, fontfile, PDF_NAME(Subtype));
					if (pdf_name_eq(ctx, subtype, PDF_NAME(CIDFontType0C)))
					{
						subset_cff(ctx, doc, font, fontfile, symbolic, 1);
						prefix_font_name(ctx, doc, fontdesc, fontfile);
						continue;
					}
				}
			}
			else if (pdf_name_eq(ctx, subtype, PDF_NAME(Type1)))
			{
				pdf_obj *fontdesc = pdf_dict_get(ctx, font->obj, PDF_NAME(FontDescriptor));
				pdf_obj *fontfile = pdf_dict_get(ctx, fontdesc, PDF_NAME(FontFile3));
				pdf_obj *st2 = pdf_dict_get(ctx, fontfile, PDF_NAME(Subtype));
				int flags = pdf_dict_get_int(ctx, fontdesc, PDF_NAME(Flags));
				int symbolic = (!!(flags & 4)) | ((flags & 32) == 0);
				if (pdf_name_eq(ctx, st2, PDF_NAME(Type1C)))
				{
					subset_cff(ctx, doc, font, fontfile, symbolic, 0);
					adjust_simple_font(ctx, doc, font);
					prefix_font_name(ctx, doc, fontdesc, fontfile);
					continue;
				}
			}
		}

	}
	fz_always(ctx)
	{
			fz_drop_page(ctx, (fz_page *)page);

			for (i = 0; i < usage.len; i++)
			{
				pdf_drop_obj(ctx, usage.font[i].obj);
				fz_free(ctx, usage.font[i].cids.heap);
				fz_free(ctx, usage.font[i].gids.heap);
			}
			fz_free(ctx, usage.font);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}
