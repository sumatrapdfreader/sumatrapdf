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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "html-imp.h"

#include "hb.h"
#include "hb-ft.h"
#include <ft2build.h>

#include <math.h>
#include <assert.h>

#undef DEBUG_HARFBUZZ

/*
	Some notes on the layout code below and the concepts used.

	THE MODEL:

	The standard CSS box model is used here. Each box has a margin, a border, padding, and content, any of which can be zero in size.

	+-------------------------------------------------------------------+
	| margin                                                            |
	|        +-------------------------------------------------+        |
	|        | border                                          |        |
	|        |        +-------------------------------+        |        |
	|        |        | padding                       |        |        |
	|        |        |           +---------+         |        |        |
	|        |        |           | content |         |        |        |
	|        |        |           +---------+         |        |        |
	|        |        |                               |        |        |
	|        |        +-------------------------------+        |        |
	|        |                                                 |        |
	|        +-------------------------------------------------+        |
	|                                                                   |
	+-------------------------------------------------------------------+

	In our structures, the box->x,y,w,b refer to the bounding box of the content section alone. The margins/borders/paddings are applied
	to the outside of this automatically.

	HOW WE LAYOUT:

	Our implementation is a 'simple' recursive descent of the structure. We know the width of the area we are laying out into at each point,
	and we know the top/left coords of the enclosing block. So we start with a well defined left/top point, offset that for margins/borders/padding
	etc, calculate the width from the enclosing width (via the CSS). We set the height of the content to be zero to start with, and after laying
	out each of (any) child elements, we ensure that the base of our box is always large enough to enclose the childs boxes with the appropriate
	padding.

	VERTICAL MARGIN COLLAPSE:

	The big complexity in this code is the need to do 'vertical margin collapse'. Basically, if we have 2 blocks, laid out vertically from one
	another, the margin between them is collapsed to be the larger of margins of the two blocks, rather than the sum of the margins of the
	two blocks.

	So block A with a margin of 30, and block B with a margin of 20 will be separated by 30, not 50.

	To make matters more complicated, margin collapse can happen 'through' nestings of blocks, and even through empty blocks. It cannot
	happen through blocks with borders, or padding.

	Accordingly, as we recursively pass through our structure laying out, we keep a 'vertical' parameter, representing the amount of vertical
	space to be considered for collapse. This is set to zero if we hit borders or padding or non-zero height content boxes, and our margins are
	adjusted by it.

	While the margins of a block may be adjusted by a parent 'inheriting' those margins, the bbox of the content of a block will never be
	changed.

	NON-RESTARTING OPERATION:

	The standard layout operation as used for HTML documents and ebooks has us layout onto an infinitely long page, with the vertical
	offsets of boxes adjusted so that a given line of text (or image) never spans a page end (defined to be a multiple of page_h). This
	layout can proceed from start to finish with no need to stop. This is achieved by calling the code with restart == NULL.

	RESTARTING:

	In order to cope with laying out a text story into multiple (potentially differently sized) areas, we need to be able to stop the layout
	when the available area is full, and restart again later. We do this by having a restart record. The tree structure means we can't
	simply return a pointer to the node to continue processing at - we need to recursively redescend the tree until we reach the same
	position, whereupon we will continue. Also, because of the fact that the area being laid out into is potentially a completely different
	size, we need to recalculate widths etc, so at least part of the tree needs to be reprocessed when we restart.

	Accordingly, two ways to do this immediately suggest themselves. The first is to store the path down the tree so that we can quickly
	and efficiently skip whole areas of the tree that do not need reprocessing and redescend just the sections we need to. The second is to
	accept that we will reprocess the entire prefix of the tree each time, and just not actually do any work until we reach the exact
	box where we need to restart.

	We opt for the second one here, on the grounds that 1) it's easier to code, 2) it requires a constant amount of storage, and 3) that
	the trees we will be reprocessing will be relatively small. Should we ever need to revisit this decision, we can do so.
*/

enum { T, R, B, L };

typedef struct string_walker
{
	fz_context *ctx;
	hb_buffer_t *hb_buf;
	int rtl;
	const char *start;
	const char *end;
	const char *s;
	fz_font *base_font;
	int script;
	int language;
	int small_caps;
	fz_font *font;
	fz_font *next_font;
	hb_glyph_position_t *glyph_pos;
	hb_glyph_info_t *glyph_info;
	unsigned int glyph_count;
	int scale;
} string_walker;

static int quick_ligature_mov(fz_context *ctx, string_walker *walker, unsigned int i, unsigned int n, int unicode)
{
	unsigned int k;
	for (k = i + n + 1; k < walker->glyph_count; ++k)
	{
		walker->glyph_info[k-n] = walker->glyph_info[k];
		walker->glyph_pos[k-n] = walker->glyph_pos[k];
	}
	walker->glyph_count -= n;
	return unicode;
}

static int quick_ligature(fz_context *ctx, string_walker *walker, unsigned int i)
{
	if (walker->glyph_info[i].codepoint == 'f' && i + 1 < walker->glyph_count && !fz_font_flags(walker->font)->is_mono)
	{
		if (walker->glyph_info[i+1].codepoint == 'f')
		{
			if (i + 2 < walker->glyph_count && walker->glyph_info[i+2].codepoint == 'i')
			{
				if (fz_encode_character(ctx, walker->font, 0xFB03))
					return quick_ligature_mov(ctx, walker, i, 2, 0xFB03);
			}
			if (i + 2 < walker->glyph_count && walker->glyph_info[i+2].codepoint == 'l')
			{
				if (fz_encode_character(ctx, walker->font, 0xFB04))
					return quick_ligature_mov(ctx, walker, i, 2, 0xFB04);
			}
			if (fz_encode_character(ctx, walker->font, 0xFB00))
				return quick_ligature_mov(ctx, walker, i, 1, 0xFB00);
		}
		if (walker->glyph_info[i+1].codepoint == 'i')
		{
			if (fz_encode_character(ctx, walker->font, 0xFB01))
				return quick_ligature_mov(ctx, walker, i, 1, 0xFB01);
		}
		if (walker->glyph_info[i+1].codepoint == 'l')
		{
			if (fz_encode_character(ctx, walker->font, 0xFB02))
				return quick_ligature_mov(ctx, walker, i, 1, 0xFB02);
		}
	}
	return walker->glyph_info[i].codepoint;
}

static void init_string_walker(fz_context *ctx, string_walker *walker, hb_buffer_t *hb_buf, int rtl, fz_font *font, int script, int language, int small_caps, const char *text)
{
	walker->ctx = ctx;
	walker->hb_buf = hb_buf;
	walker->rtl = rtl;
	walker->start = text;
	walker->end = text;
	walker->s = text;
	walker->base_font = font;
	walker->script = script;
	walker->language = language;
	walker->font = NULL;
	walker->next_font = NULL;
	walker->small_caps = small_caps;
}

static void
destroy_hb_shaper_data(fz_context *ctx, void *handle)
{
	fz_hb_lock(ctx);
	hb_font_destroy(handle);
	fz_hb_unlock(ctx);
}

static const hb_feature_t small_caps_feature[1] = {
	{ HB_TAG('s','m','c','p'), 1, 0, -1 }
};

static int walk_string(string_walker *walker)
{
	fz_context *ctx = walker->ctx;
	FT_Face face;
	int fterr;
	int quickshape;
	char lang[8];

	walker->start = walker->end;
	walker->end = walker->s;
	walker->font = walker->next_font;

	if (*walker->start == 0)
		return 0;

	/* Run through the string, encoding chars until we find one
	 * that requires a different fallback font. */
	while (*walker->s)
	{
		int c;

		walker->s += fz_chartorune(&c, walker->s);
		(void)fz_encode_character_with_fallback(ctx, walker->base_font, c, walker->script, walker->language, &walker->next_font);
		if (walker->next_font != walker->font)
		{
			if (walker->font != NULL)
				break;
			walker->font = walker->next_font;
		}
		walker->end = walker->s;
	}

	/* Disable harfbuzz shaping if script is common or LGC and there are no opentype tables. */
	quickshape = 0;
	if (walker->script <= 3 && !walker->rtl && !fz_font_flags(walker->font)->has_opentype)
		quickshape = 1;

	fz_hb_lock(ctx);
	fz_try(ctx)
	{
		face = fz_font_ft_face(ctx, walker->font);
		walker->scale = face->units_per_EM;
		fterr = FT_Set_Char_Size(face, walker->scale, walker->scale, 72, 72);
		if (fterr)
			fz_throw(ctx, FZ_ERROR_GENERIC, "freetype setting character size: %s", ft_error_string(fterr));

		hb_buffer_clear_contents(walker->hb_buf);
		hb_buffer_set_direction(walker->hb_buf, walker->rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
		/* hb_buffer_set_script(walker->hb_buf, hb_ucdn_script_translate(walker->script)); */
		if (walker->language)
		{
			fz_string_from_text_language(lang, walker->language);
			Memento_startLeaking(); /* HarfBuzz leaks harmlessly */
			hb_buffer_set_language(walker->hb_buf, hb_language_from_string(lang, (int)strlen(lang)));
			Memento_stopLeaking(); /* HarfBuzz leaks harmlessly */
		}
		/* hb_buffer_set_cluster_level(hb_buf, HB_BUFFER_CLUSTER_LEVEL_CHARACTERS); */

		hb_buffer_add_utf8(walker->hb_buf, walker->start, walker->end - walker->start, 0, -1);

		if (!quickshape)
		{
			fz_shaper_data_t *hb = fz_font_shaper_data(ctx, walker->font);
			Memento_startLeaking(); /* HarfBuzz leaks harmlessly */
			if (hb->shaper_handle == NULL)
			{
				hb->destroy = destroy_hb_shaper_data;
				hb->shaper_handle = hb_ft_font_create(face, NULL);
			}

			hb_buffer_guess_segment_properties(walker->hb_buf);

			if (walker->small_caps)
				hb_shape(hb->shaper_handle, walker->hb_buf, small_caps_feature, nelem(small_caps_feature));
			else
				hb_shape(hb->shaper_handle, walker->hb_buf, NULL, 0);
			Memento_stopLeaking();
		}

		walker->glyph_pos = hb_buffer_get_glyph_positions(walker->hb_buf, &walker->glyph_count);
		walker->glyph_info = hb_buffer_get_glyph_infos(walker->hb_buf, NULL);
	}
	fz_always(ctx)
	{
		fz_hb_unlock(ctx);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	if (quickshape)
	{
		unsigned int i;
		for (i = 0; i < walker->glyph_count; ++i)
		{
			int glyph, unicode;
			unicode = quick_ligature(ctx, walker, i);
			if (walker->small_caps)
				glyph = fz_encode_character_sc(ctx, walker->font, unicode);
			else
				glyph = fz_encode_character(ctx, walker->font, unicode);
			walker->glyph_info[i].codepoint = glyph;
			walker->glyph_pos[i].x_offset = 0;
			walker->glyph_pos[i].y_offset = 0;
			walker->glyph_pos[i].x_advance = fz_advance_glyph(ctx, walker->font, glyph, 0) * face->units_per_EM;
			walker->glyph_pos[i].y_advance = 0;
		}
	}

	return 1;
}

static const char *get_node_text(fz_context *ctx, fz_html_flow *node)
{
	if (node->type == FLOW_WORD)
		return node->content.text;
	else if (node->type == FLOW_SPACE)
		return " ";
	else if (node->type == FLOW_SHYPHEN)
		return "-";
	else
		return "";
}

static void measure_string(fz_context *ctx, fz_html_flow *node, hb_buffer_t *hb_buf)
{
	string_walker walker;
	unsigned int i;
	const char *s;
	float em;

	em = node->box->em;
	node->x = 0;
	node->y = 0;
	node->w = 0;
	if (fz_css_number_defined(node->box->style->leading))
		node->h = fz_from_css_number(node->box->style->leading, em, em, 0);
	else
		node->h = fz_from_css_number_scale(node->box->style->line_height, em);

	s = get_node_text(ctx, node);
	init_string_walker(ctx, &walker, hb_buf, node->bidi_level & 1, node->box->style->font, node->script, node->markup_lang, node->box->style->small_caps, s);
	while (walk_string(&walker))
	{
		int x = 0;
		for (i = 0; i < walker.glyph_count; i++)
			x += walker.glyph_pos[i].x_advance;
		node->w += x * em / walker.scale;
	}
}

static float measure_line(fz_html_flow *node, fz_html_flow *end, float *baseline, float *vert_adv)
{
	float max_a = 0, max_d = 0, h = node->h;
	*vert_adv = node->h;
	while (node != end)
	{
		if (node->h > *vert_adv)
			*vert_adv = node->h;
		if (node->type == FLOW_IMAGE)
		{
			if (node->h > max_a)
				max_a = node->h;
		}
		else
		{
			float a = node->box->em * 0.8f;
			float d = node->box->em * 0.2f;
			if (a > max_a) max_a = a;
			if (d > max_d) max_d = d;
		}
		if (node->h > h) h = node->h;
		if (max_a + max_d > h) h = max_a + max_d;
		node = node->next;
	}
	*baseline = max_a + (h - max_a - max_d) / 2;
	return h;
}

static void layout_line(fz_context *ctx, float indent, float page_w, float line_w, int align, fz_html_flow *start, fz_html_flow *end, fz_html_box *box, float baseline, float line_h)
{
	float x = box->x + indent;
	float y = box->b;
	float slop = page_w - line_w;
	float justify = 0;
	float va;
	int n, i;
	fz_html_flow *node;
	fz_html_flow **reorder;
	unsigned int min_level, max_level;

	/* Count the number of nodes on the line */
	for(i = 0, n = 0, node = start; node != end; node = node->next)
	{
		n++;
		if (node->type == FLOW_SPACE && node->expand && !node->breaks_line)
			i++;
	}

	if (align == TA_JUSTIFY)
	{
		justify = slop / (i ? i : 1);
	}
	else if (align == TA_RIGHT)
		x += slop;
	else if (align == TA_CENTER)
		x += slop / 2;

	/* We need a block to hold the node pointers while we reorder */
	reorder = fz_malloc_array(ctx, n, fz_html_flow*);
	min_level = start->bidi_level;
	max_level = start->bidi_level;
	for(i = 0, node = start; node != end; i++, node = node->next)
	{
		reorder[i] = node;
		if (node->bidi_level < min_level)
			min_level = node->bidi_level;
		if (node->bidi_level > max_level)
			max_level = node->bidi_level;
	}

	/* Do we need to do any reordering? */
	if (min_level != max_level || (min_level & 1))
	{
		/* The lowest level we swap is always a rtl one */
		min_level |= 1;
		/* Each time around the loop we swap runs of fragments that have
		 * levels >= max_level (and decrement max_level). */
		do
		{
			int start_idx = 0;
			int end_idx;
			do
			{
				/* Skip until we find a level that's >= max_level */
				while (start_idx < n && reorder[start_idx]->bidi_level < max_level)
					start_idx++;
				/* If start >= n-1 then no more runs. */
				if (start_idx >= n-1)
					break;
				/* Find the end of the match */
				i = start_idx+1;
				while (i < n && reorder[i]->bidi_level >= max_level)
					i++;
				/* Reverse from start to i-1 */
				end_idx = i-1;
				while (start_idx < end_idx)
				{
					fz_html_flow *t = reorder[start_idx];
					reorder[start_idx++] = reorder[end_idx];
					reorder[end_idx--] = t;
				}
				start_idx = i+1;
			}
			while (start_idx < n);
			max_level--;
		}
		while (max_level >= min_level);
	}

	for (i = 0; i < n; i++)
	{
		float w;

		node = reorder[i];
		w = node->w;

		if (node->type == FLOW_SPACE && node->breaks_line)
			w = 0;
		else if (node->type == FLOW_SPACE && !node->breaks_line)
			w += node->expand ? justify : 0;
		else if (node->type == FLOW_SHYPHEN && !node->breaks_line)
			w = 0;
		else if (node->type == FLOW_SHYPHEN && node->breaks_line)
			w = node->w;

		node->x = x;
		x += w;

		switch (node->box->style->vertical_align)
		{
		default:
		case VA_BASELINE:
			va = 0;
			break;
		case VA_SUB:
			va = node->box->em * 0.2f;
			break;
		case VA_SUPER:
			va = node->box->em * -0.3f;
			break;
		case VA_TOP:
		case VA_TEXT_TOP:
			va = -baseline + node->box->em * 0.8f;
			break;
		case VA_BOTTOM:
		case VA_TEXT_BOTTOM:
			va = -baseline + line_h - node->box->em * 0.2f;
			break;
		}

		if (node->type == FLOW_IMAGE)
			node->y = y + baseline - node->h;
		else
		{
			node->y = y + baseline + va;
			node->h = node->box->em;
		}
	}

	fz_free(ctx, reorder);
}

static void find_accumulated_margins(fz_context *ctx, fz_html_box *box, float *w, float *h)
{
	while (box)
	{
		if (fz_html_box_has_boxes(box)) {
			/* TODO: take into account collapsed margins */
			*h += box->margin[T] + box->padding[T] + box->border[T];
			*h += box->margin[B] + box->padding[B] + box->border[B];
			*w += box->margin[L] + box->padding[L] + box->border[L];
			*w += box->margin[R] + box->padding[R] + box->border[R];
		}
		box = box->up;
	}
}

static int flush_line(fz_context *ctx, fz_html_box *box, float page_h, float page_w, float line_w, int align, float indent, fz_html_flow *a, fz_html_flow *b, fz_html_restarter *restart)
{
	float avail, line_h, baseline, vadv;
	line_h = measure_line(a, b, &baseline, &vadv);
	if (page_h > 0)
	{
		avail = page_h - fmodf(box->b, page_h);
		if (line_h > avail)
		{
			if (restart)
			{
				assert(restart->start == NULL);

				if (restart->potential)
					restart->end = restart->potential;
				else
				{
					restart->end = box;
					restart->end_flow = a;
				}
				return 1;
			}
			box->b += avail;
		}
	}
	layout_line(ctx, indent, page_w, line_w, align, a, b, box, baseline, line_h);
	box->b += vadv;
	if (restart)
		restart->potential = NULL;

	return 0;
}

static void layout_flow_inline(fz_context *ctx, fz_html_box *box, fz_html_box *top, fz_html_restarter *restart)
{
	while (box)
	{
		box->y = top->y;
		box->em = fz_from_css_number(box->style->font_size, top->em, top->em, top->em);
		if (box->down)
			layout_flow_inline(ctx, box->down, box, restart);
		box = box->next;
	}
}

/*
	Layout a BOX_FLOW.

	ctx: The context in use.
	box: The BOX_FLOW to layout.
	top: The enclosing box.
	page_h: The height at which to break the page.
	hb_buf: The Harfbuzz buffer.
	restart: NULL, or a restart record.
*/
static void layout_flow(fz_context *ctx, fz_html_box *box, fz_html_box *top, float page_h, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_flow *node, *line, *candidate;
	fz_html_flow *start_flow = NULL;
	float line_w, candidate_w, indent, break_w, nonbreak_w;
	int line_align, align;

	float em = box->em = fz_from_css_number(box->style->font_size, top->em, top->em, top->em);
	indent = box->is_first_flow ? fz_from_css_number(top->style->text_indent, em, top->w, 0) : 0;
	align = top->style->text_align;

	if (box->markup_dir == FZ_BIDI_RTL)
	{
		if (align == TA_LEFT)
			align = TA_RIGHT;
		else if (align == TA_RIGHT)
			align = TA_LEFT;
	}

	/* Position the box, initially zero height. */
	box->x = top->x;
	box->y = top->b;
	box->w = top->w;
	box->b = box->y;

	if (restart && restart->start)
	{
		/* If we are still skipping, and don't match, nothing to do. */
		if (restart->start != box)
			return;
		/* We match! Remember where we should start. */
		restart->start = NULL;
		start_flow = restart->start_flow;
	}

	/* If we have nothing to flow, nothing to do. */
	if (!box->flow_head)
		return;

	/* Run through the child nodes setting y and em. */
	if (box->down)
		layout_flow_inline(ctx, box->down, box, restart);

	for (node = box->flow_head; node; node = node->next)
	{
		if (restart && restart->start_flow)
		{
			if (restart->start_flow != node)
				continue;
			restart->start_flow = NULL;
		}
		node->breaks_line = 0; /* reset line breaks from previous layout */
		if (node->type == FLOW_IMAGE)
		{
			float margin_w = 0, margin_h = 0;
			float max_w, max_h;
			float xs = 1, ys = 1, s;
			float aspect = 1;

			find_accumulated_margins(ctx, box, &margin_w, &margin_h);
			max_w = top->w - margin_w;
			max_h = page_h - margin_h;

			/* NOTE: We ignore the image DPI here, since most images in EPUB files have bogus values. */
			node->w = node->content.image->w * 72 / 96;
			node->h = node->content.image->h * 72 / 96;
			aspect = node->w / node->h;

			if (node->box->style->width.unit != N_AUTO)
				node->w = fz_from_css_number(node->box->style->width, top->em, top->w - margin_w, node->w);
			if (node->box->style->height.unit != N_AUTO)
				node->h = fz_from_css_number(node->box->style->height, top->em, page_h - margin_h, node->h);
			if (node->box->style->width.unit == N_AUTO && node->box->style->height.unit != N_AUTO)
				node->w = node->h * aspect;
			if (node->box->style->width.unit != N_AUTO && node->box->style->height.unit == N_AUTO)
				node->h = node->w / aspect;

			/* Shrink image to fit on one page if needed */
			if (max_w > 0 && node->w > max_w)
				xs = max_w / node->w;
			if (max_h > 0 && node->h > max_h)
				ys = max_h / node->h;
			s = fz_min(xs, ys);
			node->w = node->w * s;
			node->h = node->h * s;

		}
		else
		{
			measure_string(ctx, node, hb_buf);
		}
	}

	node = box->flow_head;

	candidate = NULL;
	candidate_w = 0;
	if (start_flow)
	{
		line = start_flow;
		line_w = start_flow == node ? indent : 0;
	}
	else
	{
		line = node;
		line_w = indent;
	}

	while (node)
	{
		if (start_flow)
		{
			if (start_flow != node)
			{
				node = node->next;
				continue;
			}
			start_flow = NULL;
		}
		switch (node->type)
		{
		default:
		case FLOW_WORD:
		case FLOW_IMAGE:
			nonbreak_w = break_w = node->w;
			break;

		case FLOW_SHYPHEN:
		case FLOW_SBREAK:
		case FLOW_SPACE:
			nonbreak_w = break_w = 0;

			/* Determine broken and unbroken widths of this node. */
			if (node->type == FLOW_SPACE)
				nonbreak_w = node->w;
			else if (node->type == FLOW_SHYPHEN)
				break_w = node->w;

			/* If the broken node fits, remember it. */
			/* Also remember it if we have no other candidate and need to break in desperation. */
			if (line_w + break_w <= box->w || !candidate)
			{
				candidate = node;
				candidate_w = line_w + break_w;
			}
			break;

		case FLOW_BREAK:
			nonbreak_w = break_w = 0;
			candidate = node;
			candidate_w = line_w;
			break;
		}

		/* The current node either does not fit or we saw a hard break. */
		/* Break the line if we have a candidate break point. */
		if (node->type == FLOW_BREAK || (line_w + nonbreak_w > box->w && candidate))
		{
			candidate->breaks_line = 1;
			if (candidate->type == FLOW_BREAK)
				line_align = (align == TA_JUSTIFY) ? TA_LEFT : align;
			else
				line_align = align;
			if (flush_line(ctx, box, page_h, box->w, candidate_w, line_align, indent, line, candidate->next, restart))
				return;

			line = candidate->next;
			node = candidate->next;
			candidate = NULL;
			candidate_w = 0;
			indent = 0;
			line_w = 0;
		}
		else
		{
			line_w += nonbreak_w;
			node = node->next;
		}
	}

	if (line)
	{
		line_align = (align == TA_JUSTIFY) ? TA_LEFT : align;
		flush_line(ctx, box, page_h, box->w, line_w, line_align, indent, line, NULL, restart);
	}
}

static int layout_block_page_break(fz_context *ctx, float *yp, float page_h, float vertical, int page_break)
{
	if (page_h <= 0)
		return 0;
	if (page_break == PB_ALWAYS || page_break == PB_LEFT || page_break == PB_RIGHT)
	{
		float avail = page_h - fmodf(*yp - vertical, page_h);
		int number = (*yp + (page_h * 0.1f)) / page_h;
		if (avail > 0 && avail < page_h)
		{
			*yp += avail - vertical;
			if (page_break == PB_LEFT && (number & 1) == 0) /* right side pages are even */
				*yp += page_h;
			if (page_break == PB_RIGHT && (number & 1) == 1) /* left side pages are odd */
				*yp += page_h;
			return 1;
		}
	}
	return 0;
}

static float layout_block(fz_context *ctx, fz_html_box *box, float em, float top_x, float *top_b, float top_w,
		float page_h, float vertical, hb_buffer_t *hb_buf, fz_html_restarter *restart);

/*
	Layout a table.

	ctx: The context in use.
	box: The box to layout.
	top: The box that encloses the table.
	page_h: The height at which to break the page.
	hb_buf: Harfbuzz buffer.
	restart: NULL, or a restart record.
*/
static void layout_table(fz_context *ctx, fz_html_box *box, fz_html_box *top, float page_h, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_box *row, *cell, *child;
	int col, ncol = 0;

	if (restart && restart->start == box)
	{
		/* We have reached the restart point */
		restart->start = NULL;
	}

	/* Position the box, zero height for now. */
	box->em = fz_from_css_number(box->style->font_size, top->em, top->em, top->em);
	box->x = top->x;
	box->w = fz_from_css_number(box->style->width, box->em, top->w, top->w);
	box->y = box->b = top->b;

	/* Find the maximum number of columns. (Count 'col' for each row, biggest one
	 * gives ncol). */
	for (row = box->down; row; row = row->next)
	{
		col = 0;
		for (cell = row->down; cell; cell = cell->next)
			++col;
		if (col > ncol)
			ncol = col;
	}

	/* Layout each row in turn. */
	for (row = box->down; row; row = row->next)
	{
		col = 0;

		/* Position the row, zero height for now. */
		row->em = fz_from_css_number(row->style->font_size, box->em, box->em, box->em);
		row->x = box->x;
		row->w = box->w;
		row->y = row->b = box->b;

		/* FIXME: If we stop laying out mid-row, then we really ought to cancel the whole
		 * row, and then restart the whole row next time. But this leads to us needing
		 * to be careful in case we have a row that will never fit. For now, just stop
		 * on the first failure. */

		/* For each cell in the row */
		for (cell = row->down; cell; cell = cell->next)
		{
			float colw = row->w / ncol; // TODO: proper calculation

			/* Position the cell, zero height for now. */
			cell->em = fz_from_css_number(cell->style->font_size, row->em, row->em, row->em);
			cell->y = cell->b = row->y;
			cell->x = row->x + col * colw;
			cell->w = colw;

			/* Layout cell contents into the cell. */
			for (child = cell->down; child; child = child->next)
			{
				if (child->type == BOX_BLOCK)
					layout_block(ctx, child, cell->em, cell->x, &cell->b, cell->w, page_h, 0, hb_buf, restart);
				else if (child->type == BOX_FLOW)
					layout_flow(ctx, child, cell, page_h, hb_buf, restart);
				cell->b = child->b;

				/* If we've reached an endpoint, stop looping. */
				if (restart && restart->end)
					break;
			}

			if (cell->b > row->b)
				row->b = cell->b;

			++col;

			/* If we've reached an endpoint, stop looping. */
			if (restart && restart->end)
				break;
		}

		box->b = row->b;
	}
}

static float
advance_for_spacing(float start_b, float spacing, float page_h, int *eop)
{
	float avail = page_h - fmodf(start_b, page_h);

	if (spacing > avail)
	{
		*eop = 1;
		spacing = avail;
	}
	return start_b + spacing;
}

/*
	Layout a BOX_BLOCK.

	ctx: The ctx in use.
	box: The BOX_BLOCK to layout.
	em: The base 'em' size in points.
	top_x: The x position for left of the topmost box.
	top_b: Pointer to the y position for the top of the topmost box on entry, updated to the y position for the bottom of the topmost box on exit.
	top_w: The width available for the topmost box.
	page_h: The height at which we break for the next page.
	vertical: The vertical margin between the previous boxes position and the start of this box.
	hb_buf: Harfbuzz buffer.
	restart: NULL, or a restart record.
*/
static float layout_block(fz_context *ctx, fz_html_box *box, float em, float top_x, float *top_b, float top_w,
		float page_h, float vertical, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_box *child;
	float auto_width;
	int first;

	const fz_css_style *style = box->style;
	float *margin = box->margin;
	float *border = box->border;
	float *padding = box->padding;
	int eop = 0;

	assert(fz_html_box_has_boxes(box));
	em = box->em = fz_from_css_number(style->font_size, em, em, em);

	margin[T] = fz_from_css_number(style->margin[T], em, top_w, 0);
	margin[R] = fz_from_css_number(style->margin[R], em, top_w, 0);
	margin[B] = fz_from_css_number(style->margin[B], em, top_w, 0);
	margin[L] = fz_from_css_number(style->margin[L], em, top_w, 0);

	padding[T] = fz_from_css_number(style->padding[T], em, top_w, 0);
	padding[R] = fz_from_css_number(style->padding[R], em, top_w, 0);
	padding[B] = fz_from_css_number(style->padding[B], em, top_w, 0);
	padding[L] = fz_from_css_number(style->padding[L], em, top_w, 0);

	border[T] = style->border_style_0 ? fz_from_css_number(style->border_width[T], em, top_w, 0) : 0;
	border[R] = style->border_style_1 ? fz_from_css_number(style->border_width[R], em, top_w, 0) : 0;
	border[B] = style->border_style_2 ? fz_from_css_number(style->border_width[B], em, top_w, 0) : 0;
	border[L] = style->border_style_3 ? fz_from_css_number(style->border_width[L], em, top_w, 0) : 0;

	/* If restart is non-NULL, we are running in restartable mode. */
	if (restart)
	{
		/* If restart->start == box, then we've been skipping, and this
		 * is the point at which we should restart. */
		if (restart->start == box)
		{
			/* Set restart->start to be NULL to indicate that we aren't skipping
			 * any more. */
			restart->start = NULL;
			/* By setting vertical = margin[T], we force the top margin to collapse,
			 * leaving us with zero space at the top on a restart. */
			vertical = margin[T];
		}

		/* If we have a border, then in the event that no content fits, we want to
		 * restart with us, not with the content that doesn't fit. */
		if (restart->potential == NULL &&
			(border[T] != 0 || border[R] != 0 || border[B] != 0 || border[L] != 0))
			restart->potential = box;
	}

	/* TODO: remove 'vertical' margin adjustments across automatic page breaks */

	if (layout_block_page_break(ctx, top_b, page_h, vertical, style->page_break_before))
		vertical = 0;

	/* Position the left of this box relative to the supplied 'top' positions. */
	box->x = top_x + margin[L] + border[L] + padding[L];
	auto_width = top_w - (margin[L] + margin[R] + border[L] + border[R] + padding[L] + padding[R]);
	box->w = fz_from_css_number(style->width, em, auto_width, auto_width);

	/* Vertical margin collapse; the space between a box and the next box is the maximum of the
	 * bottom margin of the previous box (aka 'vertical'), and the top margin of this box. */
	if (margin[T] > vertical)
		margin[T] -= vertical;
	else
		margin[T] = 0;
	/* Now margin[T] + vertical = desired vertical margin. We have already had y moved on by
	 * vertical, so adding margin[T] will leave us in the correct place. */

	/* So how large will the vertical blank space above this box be after we've added in
	 * the top margin? We need this to pass into our children for them to do vertical
	 * margin collapse in turn. */
	vertical += margin[T];

	/* If we are using a border or padding, then border collapse can't apply. */
	if (border[T] != 0 || padding[T] != 0)
		vertical = 0;

	/* Important to remember that box->{x,y,w,b} are the coordinates of the content. The
	 * margin/border/paddings are all outside this. */
	box->y = *top_b;
	if (restart && restart->start != NULL) {
		/* We're still skipping, so any child should inherit 0 vertical margin from
		 * us. */
		vertical = 0;
	}
	else
	{
		/* We're not skipping, so add in the spacings to the top edge of our box. */
		box->y += margin[T] + border[T] + padding[T];
	}
	/* Start with our content being zero height. */
	box->b = box->y;

	first = 1;
	for (child = box->down; child; child = child->next)
	{
		if (restart && restart->end == NULL)
		{
			/* If advancing over the bottom margin/border/padding of a child has previously
			 * brought us to the end of a page, this is where we end. */
			if (eop)
			{
				if (restart->potential)
					restart->end = restart->potential;
				else
					restart->end = child;
				break;
			}
		}

		if (child->type == BOX_BLOCK)
		{
			assert(fz_html_box_has_boxes(child));
			vertical = layout_block(ctx, child, em, box->x, &box->b, box->w, page_h, vertical, hb_buf, restart);
			if (first)
			{
				/* If we're skipping, then we take no notice of the child's margins. */
				if (!restart || restart->start == NULL)
				{
					/* If we have a border or padding, then leave everything alone. */
					if (border[T] == 0 && padding[T] == 0)
					{
						/* move collapsed parent/child top margins to parent */
						margin[T] += child->margin[T];
						box->y += child->margin[T];
						child->margin[T] = 0;
					}
				}
				first = 0;
			}
			/* Unless we're still skipping, the base of our box must now be at least as
			 * far down as the child, plus the childs spacing. */
			if (!restart || restart->start == NULL)
				box->b = advance_for_spacing(child->b, child->padding[B] + child->border[B] + child->margin[B], page_h, &eop);
		}
		else if (child->type == BOX_TABLE)
		{
			assert(fz_html_box_has_boxes(child));
			layout_table(ctx, child, box, page_h, hb_buf, restart);
			first = 0;
			/* If we're skipping, then take no notice of the child's margins. */
			if (!restart || restart->start == NULL)
				box->b = advance_for_spacing(child->b, child->padding[B] + child->border[B] + child->margin[B], page_h, &eop);
		}
		else if (child->type == BOX_FLOW)
		{
			layout_flow(ctx, child, box, page_h, hb_buf, restart);
			if (child->b > child->y)
			{
				if (!restart || restart->start == NULL)
				{
					box->b = child->b;
				}
				vertical = 0;
				first = 0;
			}
		}

		/* Stop if we've reached the endpoint. */
		if (restart && restart->end != NULL)
			break;
		if (restart && box->b != box->y)
			restart->potential = NULL;
	}

	/* If we're still skipping, exit with vertical=0. */
	/* If we've reached the endpoint, exit. */
	if (restart && (restart->start != NULL || restart->end != NULL))
		return 0;

	/* We can't collapse space past border or padding. */
	if (border[B] != 0 || padding[B] != 0)
		vertical = 0;

	/* reserve space for the list mark */
	if (box->list_item && box->y == box->b)
	{
		box->b += fz_from_css_number_scale(style->line_height, em);
		vertical = 0;
	}

	if (layout_block_page_break(ctx, &box->b, page_h, 0, style->page_break_after))
	{
		vertical = 0;
		margin[B] = 0;
	}

	/* If there is no border, padding, inline content etc we can collapse the
	 * bottom margin into the vertical space from above. We already set
	 * vertical to zero if there was a border above, so this resolves to just
	 * being a test for zero height.*/
	if (box->y == box->b)
	{
		if (margin[B] > vertical)
			margin[B] -= vertical;
		else
			margin[B] = 0;
	}
	else
	{
		/* If there is content, then box->b will have been updated to include all the childs
		 * margin etc. BUT some of this may still be being offered to us in vertical for
		 * margin collapse. Shrink that, and collapse that into our margin. */
		box->b -= vertical;
		vertical = fz_max(margin[B], vertical);
		margin[B] = vertical;
	}

	return vertical;
}

void
fz_restartable_layout_html(fz_context *ctx, fz_html_box *box, float w, float h, float page_w, float page_h, float em, fz_html_restarter *restart)
{
	hb_buffer_t *hb_buf = NULL;
	int unlocked = 0;

	fz_var(hb_buf);
	fz_var(unlocked);

	fz_hb_lock(ctx);

	fz_try(ctx)
	{
		Memento_startLeaking(); /* HarfBuzz leaks harmlessly */
		hb_buf = hb_buffer_create();
		Memento_stopLeaking(); /* HarfBuzz leaks harmlessly */
		unlocked = 1;
		fz_hb_unlock(ctx);

		box->em = em;
		box->w = page_w;
		box->b = box->y;

		if (restart)
			restart->potential = NULL;

		if (box->down)
		{
			switch (box->down->type)
			{
			case BOX_BLOCK:
				layout_block(ctx, box->down, box->em, box->x, &box->b, box->w, page_h, 0, hb_buf, restart);
				break;
			case BOX_FLOW:
				layout_flow(ctx, box->down, box, page_h, hb_buf, restart);
				break;
			}
			box->b = box->down->b;
		}
	}
	fz_always(ctx)
	{
		if (unlocked)
			fz_hb_lock(ctx);
		hb_buffer_destroy(hb_buf);
		fz_hb_unlock(ctx);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_layout_html(fz_context *ctx, fz_html *html, float w, float h, float em)
{
	/* If we're already laid out to the specifications we need,
	 * nothing to do. */
	if (html->layout_w == w && html->layout_h == h && html->layout_em == em)
		return;

	html->page_margin[T] = fz_from_css_number(html->tree.root->style->margin[T], em, em, 0);
	html->page_margin[B] = fz_from_css_number(html->tree.root->style->margin[B], em, em, 0);
	html->page_margin[L] = fz_from_css_number(html->tree.root->style->margin[L], em, em, 0);
	html->page_margin[R] = fz_from_css_number(html->tree.root->style->margin[R], em, em, 0);

	html->page_w = w - html->page_margin[L] - html->page_margin[R];
	if (html->page_w <= 72)
		html->page_w = 72; /* enforce a minimum page size! */
	if (h > 0)
	{
		html->page_h = h - html->page_margin[T] - html->page_margin[B];
		if (html->page_h <= 72)
			html->page_h = 72; /* enforce a minimum page size! */
	}
	else
	{
		/* h 0 means no pagination */
		html->page_h = 0;
	}

	fz_restartable_layout_html(ctx, html->tree.root, w, h, html->page_w, html->page_h, em, NULL);

	if (h == 0)
		html->page_h = html->tree.root->b;

	/* Remember how we're laid out so we can avoid needless
	 * relayouts in future. */
	html->layout_w = w;
	html->layout_h = h;
	html->layout_em = em;

#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_HTML")))
		fz_debug_html(ctx, html->tree.root);
#endif
}

static int draw_flow_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_flow *node;
	fz_text *text = NULL;
	fz_matrix trm;
	float color[3];
	float prev_color[3] = { 0, 0, 0 };
	int restartable_ended = 0;

	fz_var(text);

	/* FIXME: HB_DIRECTION_TTB? */

	if (restart && restart->start != NULL && restart->start != box)
		return 0;

	fz_try(ctx)
	{
		for (node = box->flow_head; node; node = node->next)
		{
			const fz_css_style *style = node->box->style;

			if (restart)
			{
				if (restart->start_flow != NULL)
				{
					if (restart->start_flow != node)
						continue;
					restart->start = NULL;
					restart->start_flow = NULL;
				}

				if (restart->end == box && restart->end_flow == node)
				{
					restartable_ended = 1;
					break;
				}
			}

			if (node->type == FLOW_IMAGE)
			{
				if (node->y >= page_bot || node->y + node->h <= page_top)
					continue;
			}
			else
			{
				if (node->y > page_bot || node->y < page_top)
					continue;
			}

			if (node->type == FLOW_WORD || node->type == FLOW_SPACE || node->type == FLOW_SHYPHEN)
			{
				string_walker walker;
				const char *s;
				float x, y;

				if (node->type == FLOW_SPACE && node->breaks_line)
					continue;
				if (node->type == FLOW_SHYPHEN && !node->breaks_line)
					continue;
				if (style->visibility != V_VISIBLE)
					continue;

				color[0] = style->color.r / 255.0f;
				color[1] = style->color.g / 255.0f;
				color[2] = style->color.b / 255.0f;

				if (color[0] != prev_color[0] || color[1] != prev_color[1] || color[2] != prev_color[2])
				{
					if (text)
					{
						fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), prev_color, 1, fz_default_color_params);
						fz_drop_text(ctx, text);
						text = NULL;
					}
					prev_color[0] = color[0];
					prev_color[1] = color[1];
					prev_color[2] = color[2];
				}

				if (!text)
					text = fz_new_text(ctx);

				if (node->bidi_level & 1)
					x = node->x + node->w;
				else
					x = node->x;
				y = node->y;

				trm.a = node->box->em;
				trm.b = 0;
				trm.c = 0;
				trm.d = -node->box->em;
				trm.e = x;
				trm.f = y - page_top;

				s = get_node_text(ctx, node);
				init_string_walker(ctx, &walker, hb_buf, node->bidi_level & 1, style->font, node->script, node->markup_lang, style->small_caps, s);
				while (walk_string(&walker))
				{
					float node_scale = node->box->em / walker.scale;
					unsigned int i;
					uint32_t k;
					int c, n;

					/* Flatten advance and offset into offset array. */
					int x_advance = 0;
					int y_advance = 0;
					for (i = 0; i < walker.glyph_count; ++i)
					{
						walker.glyph_pos[i].x_offset += x_advance;
						walker.glyph_pos[i].y_offset += y_advance;
						x_advance += walker.glyph_pos[i].x_advance;
						y_advance += walker.glyph_pos[i].y_advance;
					}

					if (node->bidi_level & 1)
						x -= x_advance * node_scale;

					/* Walk characters to find glyph clusters */
					k = 0;
					while (walker.start + k < walker.end)
					{
						n = fz_chartorune(&c, walker.start + k);

						for (i = 0; i < walker.glyph_count; ++i)
						{
							if (walker.glyph_info[i].cluster == k)
							{
								trm.e = x + walker.glyph_pos[i].x_offset * node_scale;
								trm.f = y - walker.glyph_pos[i].y_offset * node_scale - page_top;
								fz_show_glyph(ctx, text, walker.font, trm,
										walker.glyph_info[i].codepoint, c,
										0, node->bidi_level, box->markup_dir, node->markup_lang);
								c = -1; /* for subsequent glyphs in x-to-many mappings */
							}
						}

						/* no glyph found (many-to-many or many-to-one mapping) */
						if (c != -1)
						{
							fz_show_glyph(ctx, text, walker.font, trm,
									-1, c,
									0, node->bidi_level, box->markup_dir, node->markup_lang);
						}

						k += n;
					}

					if ((node->bidi_level & 1) == 0)
						x += x_advance * node_scale;

					y += y_advance * node_scale;
				}
			}
			else if (node->type == FLOW_IMAGE)
			{
				if (text)
				{
					fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color, 1, fz_default_color_params);
					fz_drop_text(ctx, text);
					text = NULL;
				}
				if (style->visibility == V_VISIBLE)
				{
					fz_matrix itm = fz_pre_translate(ctm, node->x, node->y - page_top);
					itm = fz_pre_scale(itm, node->w, node->h);
					fz_fill_image(ctx, dev, node->content.image, itm, 1, fz_default_color_params);
				}
			}
		}

		if (text)
		{
			fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color, 1, fz_default_color_params);
			fz_drop_text(ctx, text);
			text = NULL;
		}
	}
	fz_always(ctx)
		fz_drop_text(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return restartable_ended;
;
}

static void draw_rect(fz_context *ctx, fz_device *dev, fz_matrix ctm, float page_top, fz_css_color color, float x0, float y0, float x1, float y1)
{
	if (color.a > 0)
	{
		float rgb[3];

		fz_path *path = fz_new_path(ctx);

		fz_moveto(ctx, path, x0, y0 - page_top);
		fz_lineto(ctx, path, x1, y0 - page_top);
		fz_lineto(ctx, path, x1, y1 - page_top);
		fz_lineto(ctx, path, x0, y1 - page_top);
		fz_closepath(ctx, path);

		rgb[0] = color.r / 255.0f;
		rgb[1] = color.g / 255.0f;
		rgb[2] = color.b / 255.0f;

		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, color.a / 255.0f, fz_default_color_params);

		fz_drop_path(ctx, path);
	}
}

static const char *roman_uc[3][10] = {
	{ "", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX" },
	{ "", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC" },
	{ "", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM" },
};

static const char *roman_lc[3][10] = {
	{ "", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix" },
	{ "", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc" },
	{ "", "c", "cc", "ccc", "cd", "d", "dc", "dcc", "dccc", "cm" },
};

static void format_roman_number(fz_context *ctx, char *buf, int size, int n, const char *sym[3][10], const char *sym_m)
{
	int I = n % 10;
	int X = (n / 10) % 10;
	int C = (n / 100) % 10;
	int M = (n / 1000);

	fz_strlcpy(buf, "", size);
	while (M--)
		fz_strlcat(buf, sym_m, size);
	fz_strlcat(buf, sym[2][C], size);
	fz_strlcat(buf, sym[1][X], size);
	fz_strlcat(buf, sym[0][I], size);
	fz_strlcat(buf, ". ", size);
}

static void format_alpha_number(fz_context *ctx, char *buf, int size, int n, int alpha, int omega)
{
	int base = omega - alpha + 1;
	int tmp[40];
	int i, c;

	if (alpha > 256) /* to skip final-s for greek */
		--base;

	/* Bijective base-26 (base-24 for greek) numeration */
	i = 0;
	while (n > 0)
	{
		--n;
		c = n % base + alpha;
		if (alpha > 256 && c > alpha + 16) /* skip final-s for greek */
			++c;
		tmp[i++] = c;
		n /= base;
	}

	while (i > 0)
		buf += fz_runetochar(buf, tmp[--i]);
	*buf++ = '.';
	*buf++ = ' ';
	*buf = 0;
}

static void format_list_number(fz_context *ctx, int type, int x, char *buf, int size)
{
	switch (type)
	{
	case LST_NONE: fz_strlcpy(buf, "", size); break;
	case LST_DISC: fz_snprintf(buf, size, "%C  ", 0x2022); break; /* U+2022 BULLET */
	case LST_CIRCLE: fz_snprintf(buf, size, "%C  ", 0x25CB); break; /* U+25CB WHITE CIRCLE */
	case LST_SQUARE: fz_snprintf(buf, size, "%C  ", 0x25A0); break; /* U+25A0 BLACK SQUARE */
	default:
	case LST_DECIMAL: fz_snprintf(buf, size, "%d. ", x); break;
	case LST_DECIMAL_ZERO: fz_snprintf(buf, size, "%02d. ", x); break;
	case LST_LC_ROMAN: format_roman_number(ctx, buf, size, x, roman_lc, "m"); break;
	case LST_UC_ROMAN: format_roman_number(ctx, buf, size, x, roman_uc, "M"); break;
	case LST_LC_ALPHA: format_alpha_number(ctx, buf, size, x, 'a', 'z'); break;
	case LST_UC_ALPHA: format_alpha_number(ctx, buf, size, x, 'A', 'Z'); break;
	case LST_LC_LATIN: format_alpha_number(ctx, buf, size, x, 'a', 'z'); break;
	case LST_UC_LATIN: format_alpha_number(ctx, buf, size, x, 'A', 'Z'); break;
	case LST_LC_GREEK: format_alpha_number(ctx, buf, size, x, 0x03B1, 0x03C9); break;
	case LST_UC_GREEK: format_alpha_number(ctx, buf, size, x, 0x0391, 0x03A9); break;
	}
}

static fz_html_flow *find_list_mark_anchor(fz_context *ctx, fz_html_box *box)
{
	/* find first flow node in <li> tag */
	while (box)
	{
		if (box->type == BOX_FLOW)
			return box->flow_head;
		box = box->down;
	}
	return NULL;
}

static void draw_list_mark(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, int n)
{
	fz_font *font;
	fz_text *text;
	fz_matrix trm;
	fz_html_flow *line;
	float y, w;
	float color[3];
	const char *s;
	char buf[40];
	int c, g;

	trm = fz_scale(box->em, -box->em);

	line = find_list_mark_anchor(ctx, box);
	if (line)
	{
		y = line->y;
	}
	else
	{
		float h = fz_from_css_number_scale(box->style->line_height, box->em);
		float a = box->em * 0.8f;
		float d = box->em * 0.2f;
		if (a + d > h)
			h = a + d;
		y = box->y + a + (h - a - d) / 2;
	}

	if (y > page_bot || y < page_top)
		return;

	format_list_number(ctx, box->style->list_style_type, n, buf, sizeof buf);

	s = buf;
	w = 0;
	while (*s)
	{
		s += fz_chartorune(&c, s);
		g = fz_encode_character_with_fallback(ctx, box->style->font, c, UCDN_SCRIPT_LATIN, FZ_LANG_UNSET, &font);
		w += fz_advance_glyph(ctx, font, g, 0) * box->em;
	}

	text = fz_new_text(ctx);

	fz_try(ctx)
	{
		s = buf;
		trm.e = box->x - w;
		trm.f = y - page_top;
		while (*s)
		{
			s += fz_chartorune(&c, s);
			g = fz_encode_character_with_fallback(ctx, box->style->font, c, UCDN_SCRIPT_LATIN, FZ_LANG_UNSET, &font);
			fz_show_glyph(ctx, text, font, trm, g, c, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
			trm.e += fz_advance_glyph(ctx, font, g, 0) * box->em;
		}

		color[0] = box->style->color.r / 255.0f;
		color[1] = box->style->color.g / 255.0f;
		color[2] = box->style->color.b / 255.0f;

		fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color, 1, fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_text(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int draw_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart);

static int draw_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	switch (box->type)
	{
	case BOX_TABLE:
	case BOX_TABLE_ROW:
	case BOX_TABLE_CELL:
	case BOX_BLOCK:
		if (restart && restart->end == box)
			return 1;
		if (draw_block_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf, restart))
			return 1;
		break;
	case BOX_FLOW:
		if (draw_flow_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf, restart))
			return 1;
		break;
	}

	return 0;
}

static void
do_borders(fz_context *ctx, fz_device *dev, fz_matrix ctm, float page_top, fz_html_box *box, int suppress)
{
	float *border = box->border;
	float *padding = box->padding;
	float x0 = box->x - padding[L];
	float y0 = box->y - padding[T];
	float x1 = box->x + box->w + padding[R];
	float y1 = box->b + padding[B];

	if (border[T] > 0 && !(suppress & (1<<T)))
		draw_rect(ctx, dev, ctm, page_top, box->style->border_color[T], x0 - border[L], y0 - border[T], x1 + border[R], y0);
	if (border[R] > 0 && !(suppress & (1<<R)))
		draw_rect(ctx, dev, ctm, page_top, box->style->border_color[R], x1, y0 - border[T], x1 + border[R], y1 + border[B]);
	if (border[B] > 0 && !(suppress & (1<<B)))
		draw_rect(ctx, dev, ctm, page_top, box->style->border_color[B], x0 - border[L], y1, x1 + border[R], y1 + border[B]);
	if (border[L] > 0 && !(suppress & (1<<L)))
		draw_rect(ctx, dev, ctm, page_top, box->style->border_color[L], x0 - border[L], y0 - border[T], x0, y1 + border[B]);
}

static int draw_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_box *child;
	float x0, y0, x1, y1;

	float *padding = box->padding;
	int stopped = 0;
	int skipping;

	assert(fz_html_box_has_boxes(box));
	x0 = box->x - padding[L];
	y0 = box->y - padding[T];
	x1 = box->x + box->w + padding[R];
	y1 = box->b + padding[B];

	if (y0 > page_bot || y1 < page_top)
		return 0;

	/* If we're skipping, is this the place we should restart? */
	if (restart)
	{
		if (restart->start == box)
			restart->start = NULL;
		if (restart->end == box)
			return 1;
	}

	if (restart && restart->end == box)
		return 1;

	/* Are we skipping? */
	skipping = (restart && restart->start != NULL);

	/* Only draw the content if it's visible (never visible if skipping) */
	if (box->style->visibility == V_VISIBLE && !skipping)
	{
		int suppress;
		draw_rect(ctx, dev, ctm, page_top, box->style->background_color, x0, y0, x1, y1);

		/* Draw a selection of borders. */
		/* If we are restarting, don't do the bottom one yet. */
		suppress = restart ? (1<<B) : 0;
		do_borders(ctx, dev, ctm, page_top, box, suppress);

		if (box->list_item)
			draw_list_mark(ctx, box, page_top, page_bot, dev, ctm, box->list_item);
	}

	for (child = box->down; child; child = child->next)
	{
		if (draw_box(ctx, child, page_top, page_bot, dev, ctm, hb_buf, restart))
		{
			stopped = 1;
			break;
		}
	}

	if (box->style->visibility == V_VISIBLE && restart && restart->start == NULL)
	{
		/* We didn't draw (at least some of) the borders on the way down,
		 * because we were in restart mode. */

		/* We never want to draw the top one. Either it will have been drawn
		 * before, or we were skipping at that point. */
		int suppress = (1<<T);

		/* If we were skipping, and we're not any more, better draw in the
		 * left and right ones we missed. */
		suppress += (skipping && restart->start == NULL) ? 0 : ((1<<L) + (1<<R));

		/* If we've stopped now, don't do the bottom one either. */
		suppress += stopped ? (1<<B) : 0;

		/* FIXME: background color? list mark is probably OK as we only want
		 * it once. */

		do_borders(ctx, dev, ctm, page_top, box, suppress);
	}

	return stopped;
}

void
fz_draw_restarted_html(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_html_box *top, float page_top, float page_bot, fz_html_restarter *restart)
{
	fz_html_box *box;
	hb_buffer_t *hb_buf = NULL;
	int unlocked = 0;

	fz_var(hb_buf);
	fz_var(unlocked);

	fz_hb_lock(ctx);
	fz_try(ctx)
	{
		hb_buf = hb_buffer_create();
		fz_hb_unlock(ctx);
		unlocked = 1;

		for (box = top->down; box; box = box->next)
			if (draw_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf, restart))
				break;
	}
	fz_always(ctx)
	{
		if (unlocked)
			fz_hb_lock(ctx);
		hb_buffer_destroy(hb_buf);
		fz_hb_unlock(ctx);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_draw_html(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_html *html, int page)
{
	float page_top = page * html->page_h;
	float page_bot = (page + 1) * html->page_h;

	draw_rect(ctx, dev, ctm, 0, html->tree.root->style->background_color,
			0, 0,
			html->page_w + html->page_margin[L] + html->page_margin[R],
			html->page_h + html->page_margin[T] + html->page_margin[B]);

	ctm = fz_pre_translate(ctm, html->page_margin[L], html->page_margin[T]);

	fz_draw_restarted_html(ctx, dev, ctm, html->tree.root->down, page_top, page_bot, NULL);
}

void fz_draw_story(fz_context *ctx, fz_html_story *story, fz_device *dev, fz_matrix ctm)
{
	float page_top, page_bot;
	fz_html_box *b;
	fz_path *clip;
	fz_rect bbox;

	if (story == NULL || story->complete)
		return;

	bbox = story->bbox;
	b = story->tree.root;
	page_top = b->y - b->margin[T] - b->border[T] - b->padding[T];
	page_bot = b->b + b->margin[B] + b->border[B] + b->padding[B];

	clip = fz_new_path(ctx);
	fz_moveto(ctx, clip, bbox.x0, bbox.y0);
	fz_lineto(ctx, clip, bbox.x1, bbox.y0);
	fz_lineto(ctx, clip, bbox.x1, bbox.y1);
	fz_lineto(ctx, clip, bbox.x0, bbox.y1);
	fz_closepath(ctx, clip);
	fz_clip_path(ctx, dev, clip, 0, ctm, bbox);

	story->restart_place = story->restart_draw;
	fz_draw_restarted_html(ctx, dev, ctm, story->tree.root->down, 0, page_bot+page_top, &story->restart_place);
	story->restart_place.start = story->restart_draw.end;
	story->restart_place.start_flow = story->restart_draw.end_flow;
	story->restart_place.end = NULL;
	story->restart_place.end_flow = NULL;

	if (story->restart_place.start == NULL)
		story->complete = 1;

	fz_pop_clip(ctx, dev);
}
