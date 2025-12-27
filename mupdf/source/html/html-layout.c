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
#include "mupdf/ucdn.h"
#include "html-imp.h"

#include "hb.h"
#include "hb-ft.h"
#include <ft2build.h>

#include <math.h>
#include <assert.h>

#undef DEBUG_HARFBUZZ

#undef DEBUG_DESPERATE_SPLITTING

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
	out each of (any) child elements, we ensure that the base of our box is always large enough to enclose the child's boxes with the appropriate
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

/* === LAYOUT INLINE TEXT === */

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
	int graphemes;
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

static void init_string_walker(fz_context *ctx, string_walker *walker, hb_buffer_t *hb_buf, int rtl, fz_font *font, int script, int language, int small_caps, const char *text, int cluster_as_graphemes)
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
	walker->graphemes = cluster_as_graphemes;
}

static void
destroy_hb_shaper_data(fz_context *ctx, void *handle)
{
	fz_hb_lock(ctx);
	hb_font_destroy(handle);
	fz_hb_unlock(ctx);
}

static const hb_feature_t small_caps_feature[1] = {
	{ HB_TAG('s','m','c','p'), 1, 0, (unsigned int)-1 }
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
			fz_throw(ctx, FZ_ERROR_LIBRARY, "freetype setting character size: %s", ft_error_string(fterr));

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
		if (walker->graphemes)
			hb_buffer_set_cluster_level(walker->hb_buf, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
		else
		hb_buffer_set_cluster_level(walker->hb_buf, HB_BUFFER_CLUSTER_LEVEL_CHARACTERS);

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

static void measure_string_w(fz_context *ctx, fz_html_flow *node, hb_buffer_t *hb_buf)
{
	float em = node->box->s.layout.em;
	string_walker walker;
	unsigned int i;
	const char *s;
	node->w = 0;
	s = get_node_text(ctx, node);
	init_string_walker(ctx, &walker, hb_buf, node->bidi_level & 1, node->box->style->font, node->script, node->markup_lang, node->box->style->small_caps, s, 0);
	while (walk_string(&walker))
	{
		int x = 0;
		for (i = 0; i < walker.glyph_count; i++)
			x += walker.glyph_pos[i].x_advance;
		node->w += x * em / walker.scale;
	}
}

static void measure_string_h(fz_context *ctx, fz_html_flow *node)
{
	float em = node->box->s.layout.em;
	if (fz_css_number_defined(node->box->style->leading))
		node->h = fz_from_css_number(node->box->style->leading, em, em, 0);
	else
		node->h = fz_from_css_number_scale(node->box->style->line_height, em);
}


static float measure_line(fz_html_flow *node, fz_html_flow *end, float *baseline)
{
	float max_a = 0, max_d = 0, h = node->h;
	while (node != end)
	{
		if (node->type == FLOW_IMAGE)
		{
			if (node->h > max_a)
				max_a = node->h;
		}
		else if (node->type != FLOW_SBREAK && node->type != FLOW_BREAK && node->type != FLOW_ANCHOR)
		{
			// Clamp ascender/descender to line-height size.
			// TODO: This is not entirely to spec, but close enough.
			float s = fz_min(node->box->s.layout.em, node->h);
			float a = s * 0.8f;
			float d = s * 0.2f;
			if (a > max_a) max_a = a;
			if (d > max_d) max_d = d;
		}
		if (node->h > h)
			h = node->h;
		node = node->next;
	}
	*baseline = max_a + (h - max_a - max_d) / 2;
	return h;
}

static void layout_line(fz_context *ctx, float indent, float page_w, float line_w, int align, fz_html_flow *start, fz_html_flow *end, fz_html_box *box, float baseline, float line_h)
{
	float x = box->s.layout.x + indent;
	float y = box->s.layout.b;
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
		case VA_SUB:
		case VA_SUPER:
		case VA_MIDDLE:
			va = node->box->s.layout.baseline;
			break;

		case VA_TOP:
		case VA_TEXT_TOP:
			va = -baseline + node->box->s.layout.em * 0.8f;
			break;
		case VA_BOTTOM:
		case VA_TEXT_BOTTOM:
			va = -baseline + line_h - node->box->s.layout.em * 0.2f;
			break;
		}

		if (node->type == FLOW_IMAGE)
			node->y = y + baseline - node->h;
		else
		{
			node->y = y + baseline + va;
			node->h = node->box->s.layout.em;
		}
	}

	fz_free(ctx, reorder);

	if (box->s.layout.x + box->s.layout.w < x)
		box->s.layout.w = x - box->s.layout.x;
}

typedef struct
{
	fz_pool *pool;

	float page[4];
	/* The ancestor trbl is the trbl for the last positioned
	 * node we have walked though. Boxes with "absolute" positioning
	 * will be positioned relative to this. Initially this will be the
	 * same as page. */
	float ancestor[4];
	/* bounds is the trbl that is the current bounds we're filling
	 * into. This will initially be page, but can be changed by
	 * positions/width/heights etc. Our next fill should be at
	 * bounds[L],[T]. */
	float bounds[4];
	/* used should be updated on exit from any layout stage to
	 * be the area used by this block. */
	float used[4];

	hb_buffer_t *hb_buf;
	fz_html_restarter *restart;
} layout_data;

/* "are we in a restarting context, and still skipping?" */
#define WE_ARE_SKIPPING(restart) ((restart) && (restart)->start != NULL)

/* "are we in a restarting context without having stopped for a restart yet?" */
#define SHOULD_STOP_FOR_RESTART(restart) ((restart) && (restart)->end == NULL)

/* "are we in a restarting context, and stopping already?" */
#define STOPPING_FOR_RESTART(restart) ((restart) && (restart)->end != NULL)

#define TRBLCPY(DST,SRC) \
do { DST[T] = SRC[T] ; DST[R] = SRC[R]; DST[B] = SRC[B]; DST[L] = SRC[L]; } while (0)

static int flush_line(fz_context *ctx, fz_html_box *box, layout_data *ld, float line_w, int align, float indent, fz_html_flow *a, fz_html_flow *b, fz_html_restarter *restart)
{
	float avail, line_h, baseline;
	float page_w = ld->bounds[R] - ld->bounds[L];
	float page_h = ld->page[B] - ld->page[T];
	float page_top = ld->page[T];
	line_h = measure_line(a, b, &baseline);
	if (page_h > 0)
	{
		avail = page_h - fmodf(box->s.layout.b - page_top, page_h);
		/* If the line is larger than the available space skip to the start
		 * of the next page. */
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
				restart->reason = FZ_HTML_RESTART_REASON_LINE_HEIGHT;
				return 1;
			}
			box->s.layout.b += avail;
		}
	}
	layout_line(ctx, indent, page_w, line_w, align, a, b, box, baseline, line_h);
	box->s.layout.b += line_h;
	if (box->s.layout.b > ld->used[B])
		ld->used[B] = box->s.layout.b;
	if (restart)
		restart->potential = NULL;

	return 0;
}

static void split_flow_at_byte_offset(fz_context *ctx, fz_pool *pool, fz_html_flow *flow, size_t offset)
{
	fz_html_flow *new_flow;
	char *text;
	size_t len;

	assert(flow->type == FLOW_WORD);

	assert(offset != 0);
	text = flow->content.text + offset;
	len = strlen(text);
	new_flow = fz_pool_alloc(ctx, pool, offsetof(fz_html_flow, content) + len+1);
	memcpy(new_flow, flow, offsetof(fz_html_flow, content));
	new_flow->next = flow->next;
	flow->next = new_flow;
	strcpy(new_flow->content.text, text);
	*text = 0;
}

/* node becomes last cluster, node->next becomes the rest */
static void split_flow_at_byte_offset_reverse(fz_context *ctx, fz_pool *pool, fz_html_flow *flow, size_t offset)
{
	fz_html_flow *new_flow;
	char *text;
	size_t len;

	assert(flow->type == FLOW_WORD);

	assert(offset != 0);
	text = flow->content.text + offset;
	len = strlen(text);
	new_flow = fz_pool_alloc(ctx, pool, offsetof(fz_html_flow, content) + offset+1);
	memcpy(new_flow, flow, offsetof(fz_html_flow, content));
	new_flow->next = flow->next;
	flow->next = new_flow;
	memcpy(new_flow->content.text, flow->content.text, offset);
	new_flow->content.text[offset] = 0;
	memmove(flow->content.text, text, len);
	flow->content.text[len] = 0;
}

static void break_word_for_overflow_wrap(fz_context *ctx, fz_html_flow *node, layout_data *ld, float max_w)
{
	hb_buffer_t *hb_buf = ld->hb_buf;
	const char *text = node->content.text;
	string_walker walker;
	float w = 0;
	unsigned int at = (unsigned int)-1;
	float em = node->box->s.layout.em;

	assert(node->type == FLOW_WORD);
	assert(node->atomic == 0);

	/* The entire flow doesn't fit on a line, so we need to break it in the middle.
	 * We need to be careful not to break it in the middle of a cluster, as this
	 * would really mess shaping up.
	 *
	 * For left 2 right text, this means split the first cluster into its own 'atomic'
	 * node, and shorten the remainder.
	 *
	 * For right 2 left text, we split the last cluster into its own 'atomic' node
	 * and shorten the remainder. This is fine, cos although it appears to change the
	 * logical ordering for the text, we never extract from HTML, and the correct
	 * appearance is preserved.
	 *
	 * Desperately breaking in the middle of a word like this should should
	 * rarely (if ever) come up.
	 *
	 * TODO: Split at all the clusters in the word at once.
	 */

	/* Walk string and split at the first cluster. */
	if ((node->bidi_level & 1) == 0)
	{
		/* Left 2 Right */
		init_string_walker(ctx, &walker, hb_buf, 0 /* L2R */, node->box->style->font, node->script, node->markup_lang, node->box->style->small_caps, text, 1);
	while (walk_string(&walker))
	{
			unsigned int i;
#ifdef DEBUG_DESPERATE_SPLITTING
			for (i = 0; i < walker.glyph_count; ++i)
			{
				uint32_t can_break_here = (hb_glyph_info_get_glyph_flags(&walker.glyph_info[i]) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0;
				printf("%s(%x, %d)",
					can_break_here ? "|" : " ",
					walker.glyph_info[i].codepoint, walker.glyph_info[i].cluster);
			}
			printf("\n");
#endif
		for (i = 0; i < walker.glyph_count; ++i)
		{
				uint32_t can_break_here = (hb_glyph_info_get_glyph_flags(&walker.glyph_info[i]) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0;

				if (can_break_here)
				{
					/* If this fragment would take us beyond the end, then give up. */
					if (w > max_w)
						break;

					at = walker.start + walker.glyph_info[i].cluster - text;
				}

				w += walker.glyph_pos[i].x_advance * em / walker.scale;

				/* Make sure we have the whole cluster */
				while (i+1 < walker.glyph_count && walker.glyph_info[i].cluster == walker.glyph_info[i+1].cluster)
			{
					i++;
					w += walker.glyph_pos[i].x_advance * em / walker.scale;
				}
			}
		}
		if (at != (unsigned int)-1 && at != 0 && at != strlen(text))
		{
			/* node becomes first cluster, node->next becomes the rest */
			split_flow_at_byte_offset(ctx, ld->pool, node, at);
				node->next->overflow_wrap = 1;
				measure_string_w(ctx, node, ld->hb_buf);
				measure_string_w(ctx, node->next, ld->hb_buf);
				return;
			}
		}
	else
	{
		/* Right 2 Left */
		init_string_walker(ctx, &walker, hb_buf, 1 /* R2L */, node->box->style->font, node->script, node->markup_lang, node->box->style->small_caps, text, 1);
		while (walk_string(&walker))
		{
			unsigned int i;
#ifdef DEBUG_DESPERATE_SPLITTING
			for (i = 0; i < walker.glyph_count; ++i)
			{
				uint32_t can_break_here = (hb_glyph_info_get_glyph_flags(&walker.glyph_info[i]) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0;
				printf("%s(%x, %d)",
					can_break_here ? "|" : " ",
					walker.glyph_info[i].codepoint, walker.glyph_info[i].cluster);
	}
			printf("\n");
#endif
			/* Find the first cluster we can break before. */

			/* We can always break at the start of a fragment returned by walk_string
			 * (unless it's the very first one!) */
			if (w > max_w)
				break;
			if (walker.start != text)
				at = walker.start - text;

			for(i = 0; i < walker.glyph_count; i++)
			{
				uint32_t can_break_here = (hb_glyph_info_get_glyph_flags(&walker.glyph_info[i]) & HB_GLYPH_FLAG_UNSAFE_TO_BREAK) == 0;

				w += walker.glyph_pos[i].x_advance * em / walker.scale;

				if (can_break_here)
				{
					/* If this fragment would take us beyond the end, then give up. */
					if (w > max_w)
						break;

					if (i == walker.glyph_count)
						at = walker.start - text;
					else
						at = walker.start + walker.glyph_info[i].cluster - text;
				}
			}
		}
		if (at != (unsigned int)-1 && at != 0 && at != strlen(text))
		{
			/* Split at the last point found */
			/* node becomes last cluster, node->next becomes the rest */
			split_flow_at_byte_offset_reverse(ctx, ld->pool, node, at);
			node->next->overflow_wrap = 1;
			measure_string_w(ctx, node, ld->hb_buf);
			measure_string_w(ctx, node->next, ld->hb_buf);
			return;
		}
	}

	/* Unless we've overflowed word is already only one cluster. Don't try breaking here again! */
	if (w <= max_w)
	node->atomic = 1;
}

/*
	Layout a BOX_FLOW.

	Flow box is in a BOX_BLOCK or BOX_TABLE_CELL context, and has no margin/padding/border.

	box: The BOX_FLOW to layout.
	top: The enclosing box.
*/
static void layout_flow(fz_context *ctx, layout_data *ld, fz_html_box *box, fz_html_box *top)
{
	fz_html_flow *node, *line, *candidate, *desperate;
	fz_html_flow *start_flow = NULL;
	float line_w, candidate_w, desperate_w, indent, bounds_w;
	int justify_align;
	int align;
	fz_html_restarter *restart = ld->restart;

	float em = box->s.layout.em;
	indent = box->is_first_flow ? fz_from_css_number(top->style->text_indent, em, ld->bounds[R] - ld->bounds[L], 0) : 0;
	align = top->style->text_align;
	justify_align = TA_LEFT;

	if (box->markup_dir == FZ_BIDI_RTL)
	{
		justify_align = TA_RIGHT;
		if (align == TA_LEFT)
			align = TA_RIGHT;
		else if (align == TA_RIGHT)
			align = TA_LEFT;
	}

	/* Position the box, initially zero height. */
	box->s.layout.x = ld->bounds[L];
	box->s.layout.y = ld->bounds[T];
	box->s.layout.w = ld->bounds[R] - ld->bounds[L];
	box->s.layout.b = ld->bounds[T];

	if (restart && restart->start)
	{
		/* If we are still skipping, and don't match, nothing to do. */
		if (restart->start != box)
			return;
		/* We match! Remember where we should start. */
		restart->start = NULL;

#ifdef DEBUG_LAYOUT_RESTARTING
		/* Output us some crufty debug */
		if (restart->start_flow == NULL)
			printf("<restart>");
		for (node = box->u.flow.head; node; node = node->next)
		{
			if (restart->start_flow == node)
				printf("<restart>");
			switch (node->type)
			{
			case FLOW_WORD:
				printf("[%s]", node->content.text);
				break;
			case FLOW_BREAK:
			case FLOW_SBREAK:
				printf("\n");
				break;
			case FLOW_SPACE:
				printf(" ");
				break;
			case FLOW_SHYPHEN:
				printf("-");
				break;
			default:
				printf("?");
				break;
			}
		}
		printf("\n");
#endif
	}

	/* If we have nothing to flow, nothing to do. */
	if (!box->u.flow.head)
		return;

	/* Measure the size of all the words and images in the flow. */
	node = box->u.flow.head;

	/* First, if we are restarting, skip over ones we've done already. */
	if (restart && restart->start_flow)
	{
		while(node && node != restart->start_flow)
		{
			indent = 0;
			node = node->next;
		}
		start_flow = node;
		restart->start_flow = NULL;
	}

	/* Now measure the size of the remaining nodes. */
	for (; node; node = node->next)
	{
		node->breaks_line = 0; /* reset line breaks from previous layout */

		if (node->type == FLOW_IMAGE)
		{
			float max_w, max_h;
			float xs = 1, ys = 1, s;
			float aspect = 1;
			float page_h = ld->bounds[B] - ld->bounds[T];

			max_w = ld->bounds[R] - ld->bounds[L];
			max_h = page_h;

			/* NOTE: We ignore the image DPI here, since most images in EPUB files have bogus values. */
			node->w = node->content.image->w * 72.0f / 96.0f;
			node->h = node->content.image->h * 72.0f / 96.0f;
			aspect = node->h ? node->w / node->h : 0;

			if (node->box->style->width.unit != N_AUTO)
				node->w = fz_from_css_number(node->box->style->width, top->s.layout.em, ld->bounds[R] - ld->bounds[L], node->w);
			if (node->box->style->height.unit != N_AUTO)
				node->h = fz_from_css_number(node->box->style->height, top->s.layout.em, page_h, node->h);
			if (node->box->style->width.unit == N_AUTO && node->box->style->height.unit != N_AUTO)
				node->w = node->h * aspect;
			if (node->box->style->width.unit != N_AUTO && node->box->style->height.unit == N_AUTO)
				node->h = (aspect == 0) ? 0 : (node->w / aspect);

			/* Shrink image to fit on one page if needed */
			if (max_w > 0 && node->w > max_w)
				xs = max_w / node->w;
			if (max_h > 0 && node->h > max_h)
				ys = max_h / node->h;
			s = fz_min(xs, ys);
			node->w = node->w * s;
			node->h = node->h * s;

		}
		else if (node->type == FLOW_ANCHOR)
		{
			node->h = 0;
		}
		else
		{
			/* Note: already measured width in layout_update_widths */
			measure_string_h(ctx, node);
		}
	}

	node = box->u.flow.head;
	if (start_flow)
	{
		line_w = start_flow == node ? indent : 0;
		node = start_flow;
	}
	else
	{
		line_w = indent;
	}
	line = node;

	candidate = NULL;
	candidate_w = 0;
	desperate = NULL;
	desperate_w = 0;
	bounds_w = ld->bounds[R] - ld->bounds[L];

	/* Now collate the measured nodes into a line. */
	while (node)
	{

		switch (node->type)
		{
		default:
			// always same width
			line_w += node->w;
			break;

		case FLOW_WORD:
			/* Allow desperate breaking in middle of word if overflow-wrap: break-word. */
			if (line_w + node->w > bounds_w && !candidate)
			{
				if (!node->atomic && node->box->style->overflow_wrap == OVERFLOW_WRAP_BREAK_WORD)
				{
					break_word_for_overflow_wrap(ctx, node, ld, bounds_w - line_w);
				}
			}
			/* Remember overflow-wrap word fragments, unless at the beginning of a line. */
			if (node->overflow_wrap && node != line)
			{
				desperate = node;
				desperate_w = line_w;
			}
			line_w += node->w;
			break;

		case FLOW_BREAK:
		case FLOW_SBREAK:
			// always zero width
			candidate = node;
			candidate_w = line_w;
			break;

		case FLOW_SPACE:
			// zero width if broken, default width if unbroken
			candidate = node;
			candidate_w = line_w;
			line_w += node->w;
			break;

		case FLOW_SHYPHEN:
			// default width if broken, zero width if unbroken
			candidate = node;
			candidate_w = line_w + node->w;
			break;
		}

		/* If we see a hard break...
		 * or the line doesn't fit, and we have a candidate breakpoint...
		 * then we can flush the line so far. */
		if (node->type == FLOW_BREAK || (line_w > bounds_w && (candidate || desperate)))
		{
			int line_align = align;
			fz_html_flow *break_at;
			float break_w;

			if (candidate)
			{
				if (candidate->type == FLOW_BREAK)
					line_align = (align == TA_JUSTIFY) ? justify_align : align;
				candidate->breaks_line = 1;
				break_at = candidate->next;
				break_w = candidate_w;
			}
			else
			{
				break_at = desperate;
				break_w = desperate_w;
			}

			if (flush_line(ctx, box, ld, break_w, line_align, indent, line, break_at, restart))
				return;

			line = break_at;
			node = break_at;
			candidate = NULL;
			candidate_w = 0;
			desperate = NULL;
			desperate_w = 0;
			indent = 0;
			line_w = 0;
		}
		else if (restart && (restart->flags & FZ_HTML_RESTARTER_FLAGS_NO_OVERFLOW) && line_w > box->s.layout.w)
		{
			/* We are in 'no-overflow' mode, and the line doesn't fit.
			 * This means our first box is too wide to ever fit in a box of this width. */
			assert(restart->start == NULL);

			restart->end = box;
			restart->end_flow = start_flow;
			restart->reason = FZ_HTML_RESTART_REASON_LINE_WIDTH;
			return;
		}
		else
		{
			node = node->next;
		}
	}

	if (line)
	{
		int line_align = (align == TA_JUSTIFY) ? justify_align : align;
		flush_line(ctx, box, ld, line_w, line_align, indent, line, NULL, restart);
	}

	ld->used[T] = box->s.layout.y;
	ld->used[R] = box->s.layout.x + box->s.layout.w;
	ld->used[B] = box->s.layout.b;
	ld->used[L] = box->s.layout.x;
}

static float advance_for_spacing(fz_context *ctx, layout_data *ld, float start_b, float spacing, int *eop)
{
	float page_h = ld->page[B] - ld->page[T];
	float page_top = ld->page[T];
	float avail = page_h - fmodf(start_b - page_top, page_h);
	if (spacing > avail)
	{
		*eop = 1;
		spacing = avail;
	}
	return start_b + spacing;
}

static int layout_block_page_break(fz_context *ctx, layout_data *ld, float *yp, int page_break)
{
	fz_html_restarter *restart = ld->restart;
	float page_h = ld->page[B] - ld->page[T];
	float page_top = ld->page[T];
	float avail;
	int side; /* 0 = right, 1 = left */

	/* If we are skipping, nothing to do! */
	if (WE_ARE_SKIPPING(restart))
		return 0;

	/* If the page height is screwy (or the rectangle height), then bale. */
	if (page_h <= 0)
		return 0;

	/* If we aren't doing some kind of page break, bale. */
	if (page_break != PB_ALWAYS && page_break != PB_LEFT && page_break != PB_RIGHT)
		return 0;

	/* How much space left on the page? */
	avail = page_h - fmodf(*yp - page_top, page_h);

	/* Which side of the page are we on? In restarting (i.e. story) contexts,
	 * this is passed in explicitly. In traditional contexts, we calculate it
	 * according to which page we're on. */
	if (restart != NULL)
		side = restart->left_page;
	else
		side = 1 & (int)((*yp - page_top) / page_h);

	/* If we haven't used ANY of the page, then we might not need to do anything. */
	if (avail == page_h)
	{
		if (page_break == PB_ALWAYS)
			return 0;
		if (page_break == PB_LEFT && side == 1)
			return 0;
		if (page_break == PB_RIGHT && side == 0)
			return 0;
	}

	/* Move us to the next page. */
	if (avail > 0)
		*yp += avail, side ^= 1;

	/* Do we need to move further? */
	if (page_break == PB_LEFT)
	{
		/* If we're restarting, make sure we only restart on a left page. */
		if (SHOULD_STOP_FOR_RESTART(restart))
			restart->end_flags = FZ_HTML_RESTARTER_START_END_FLAGS_SPECIFIC_SIDE | FZ_HTML_RESTARTER_START_END_FLAGS_LEFT_SIDE;
		if (side == 0) /* right pages, side == 0 */
			*yp += page_h;
	}
	if (page_break == PB_RIGHT)
		{
		/* If we're restarting, make sure we only restart on a right page. */
		if (SHOULD_STOP_FOR_RESTART(restart))
			restart->end_flags = FZ_HTML_RESTARTER_START_END_FLAGS_SPECIFIC_SIDE;
		if (side == 1) /* left pages, side == 1 */
				*yp += page_h;
	}

			return 1;
		}

/* === POSITION LOGIC === */

/* We support the following 4 types of "position:" style.
 * POS_STATIC: Nothing to do.
 * POS_RELATIVE: Offset everything inside by a constant amount.
 * POS_FIXED: Rewrite the block according to the top level block.
 * POS_ABSOLUTE: Positioned relative to the nearest positioned ancestor.
 */
static void
shift_box_contents(fz_html_box *box, float x, float y)
{
	fz_html_box *child;

	box->s.layout.x += x;
	box->s.layout.y += y;
	box->s.layout.b += y;
	if (box->type == BOX_FLOW)
	{
		fz_html_flow *flow;
		for (flow = box->u.flow.head; flow != NULL; flow = flow->next)
			flow->x += x, flow->y += y;
	}
	for (child = box->down; child; child = child->next)
	{
		if (child->style->position == POS_FIXED || child->style->position == POS_ABSOLUTE)
			return;
		shift_box_contents(child, x, y);
	}
}

typedef struct
{
	float ancestor[4];
	float bounds[4];
	float inset[4];

	int has_width;
	float width;
	int has_height;
	float height;
} position_data;

/*	pre_position: called when we are about to layout a block.
 *	This expects that all the ld trbls are valid. It stores as
 *	many as need to be stored in position_data, recalculates
 *	bounds and fill to allow for margins/borders/padding,
 *	and sets the initial box position appropriately.
 *
 *	For positioned blocks, it calculates inset, and rewrites
 *	ld->bounds and ld->fill as appropriate.
 *
 *	If this returns 1, then we are moving onto a new page.
 */
static int
pre_position(fz_context *ctx, position_data *pd, layout_data *ld, fz_html_box *box, int ignore_width)
{
	float em = box->s.layout.em;
	const float *margin = box->u.block.margin;
	const float *border = box->u.block.border;
	const float *padding = box->u.block.padding;
	float auto_w, auto_h;
	fz_html_restarter *restart = ld->restart;
	int eop = 0;

	/* On entry, ld->bounds is the area we expect to fill this block into.
	 * ld->fill is the area of this that has been used to date.
	 */

	/* Store the bounds. */
	TRBLCPY(pd->bounds, ld->bounds);

	/* We want to adjust bounds before we lay the children out so that
	 * any margin/borders/padding are all outside them. */

	if (WE_ARE_SKIPPING(restart))
	{
		/* We're still skipping, so any child should inherit 0 vertical margin from us. */
	}
	else
	{
		/* We're not skipping, so add in the spacings to the top edge of our box. */
		ld->bounds[T] = advance_for_spacing(ctx, ld, ld->bounds[T], margin[T] + border[T] + padding[T], &eop);
		if (eop && SHOULD_STOP_FOR_RESTART(restart))
			return 1;
	}

	/* Now calculate the bounds for any sub blocks. */
	pd->has_width = fz_css_number_defined_not_auto(box->style->width);
	pd->width = fz_from_css_number(box->style->width, em, ld->bounds[R] - ld->bounds[L], 0);
	pd->has_height = fz_css_number_defined_not_auto(box->style->height);
	pd->height = fz_from_css_number(box->style->height, em, ld->bounds[B] - ld->bounds[T], 0);

	/* Adjust horizontally for Width and spacings */
	auto_w = ld->bounds[R] - ld->bounds[L] - (margin[L] + margin[R] + border[L] + border[R] + padding[L] + padding[R]);
	ld->bounds[L] = ld->bounds[L] + margin[L] + border[L] + padding[L];
	if (ignore_width)
		ld->bounds[R] = ld->bounds[L] + auto_w;
	else
		ld->bounds[R] = ld->bounds[L] + fz_from_css_number(box->style->width, em, auto_w, auto_w);

	/* Adjust vertically for Height and spacings (noting that we have already done the top ones above!) */
	auto_h = ld->bounds[B] - ld->bounds[T] - (margin[B] + border[B] + padding[B]);
	ld->bounds[B] = ld->bounds[T] + fz_from_css_number(box->style->height, em, auto_h, auto_h);

	/* So bounds and fixed are calculated as they should be for positioned: static */
	ld->used[T] = ld->bounds[T];
	ld->used[R] = ld->bounds[L];
	ld->used[B] = ld->bounds[T];
	ld->used[L] = ld->bounds[L];

	/* So we know the position of the box in the positioned: static case. */
	box->s.layout.y = ld->bounds[T];
	box->s.layout.x = ld->bounds[L];
	box->s.layout.b = ld->bounds[T];
	box->s.layout.w = ld->bounds[R]-ld->bounds[L];

	if (!WE_ARE_SKIPPING(restart))
	{
		if (box->style->position == POS_STATIC || box->style->position == POS_RELATIVE)
		{
			if (pd->has_width && box->s.layout.w < pd->width)
				box->s.layout.w = pd->width;
			if (pd->has_height && box->s.layout.b - box->s.layout.y < pd->height)
				box->s.layout.b = pd->height + box->s.layout.y;
		}
	}

	/* So in the common case, we're done! */
	if (box->style->position == POS_STATIC)
		return eop;

	/* For non-static positionings, this becomes our ancestor */
	TRBLCPY(pd->ancestor, ld->ancestor);
	TRBLCPY(ld->ancestor, ld->bounds);

	/* Calculate insets. */
	pd->inset[T] = fz_from_css_number(box->style->inset[T], em, em, 0);
	pd->inset[R] = fz_from_css_number(box->style->inset[R], em, em, 0);
	pd->inset[B] = fz_from_css_number(box->style->inset[B], em, em, 0);
	pd->inset[L] = fz_from_css_number(box->style->inset[L], em, em, 0);

	switch (box->style->position)
	{
	case POS_RELATIVE:
		/* Nothing to do here. We'll adjust it later. */
		break;
	case POS_FIXED:
	{
		float page[4];

		page[T] = ld->page[T] + margin[T] + border[T] + padding[T];
		page[R] = ld->page[R] - margin[R] - border[R] - padding[R];
		page[B] = ld->page[B] - margin[B] - border[B] - padding[B];
		page[L] = ld->page[L] + margin[L] + border[L] + padding[L];

		if (fz_css_number_defined(box->style->inset[L]))
		{
			ld->bounds[L] = page[L] + pd->inset[L];
			if (pd->has_width)
			{
				/* Left and Width determine the box. */
				ld->bounds[R] = ld->bounds[L] + pd->width;
			}
			else
			{
				/* Left and bounds right determine the box. */
				ld->bounds[R] = page[R] - pd->inset[L];
			}
		}
		else
		{
			if (pd->has_width)
			{
				/* Right and Width determine the box. */
				ld->bounds[R] = page[R] - pd->inset[R];
				ld->bounds[L] = ld->bounds[R] - pd->width;
			}
			else
			{
				/* Right and page define the box. */
				ld->bounds[L] = page[L];
				ld->bounds[R] = page[R] - pd->inset[R];
			}
		}
		if (fz_css_number_defined(box->style->inset[T]))
		{
			ld->bounds[T] = page[T] + pd->inset[T];
			if (pd->has_height)
			{
				/* Top and Height determine the box. */
				ld->bounds[B] = ld->bounds[T] + pd->height;
			}
			else
			{
				/* Top and Bottom determine the box. */
				ld->bounds[B] = page[B] - pd->inset[T];
			}
		}
		else
		{
			if (pd->has_height)
			{
				/* Bottom and Height determine the box. */
				ld->bounds[B] = page[B] - pd->inset[B];
				ld->bounds[T] = ld->bounds[B] - pd->height;
			}
			else
			{
				/* Bottom and page define the box. */
				ld->bounds[T] = page[T];
				ld->bounds[B] = page[B] - pd->inset[B];
			}
		}
		break;
	}
	case POS_ABSOLUTE:
	{
		if (fz_css_number_defined(box->style->inset[L]))
		{
			ld->bounds[L] = pd->ancestor[L] + pd->inset[L];
			if (pd->has_width)
			{
				/* Left and Width determine the box. */
				ld->bounds[R] = ld->bounds[L] + pd->width;
			}
			else
			{
				/* Left and right determine the box. */
				ld->bounds[R] = pd->ancestor[R] - pd->inset[L];
			}
		}
		else
		{
			if (pd->has_width)
			{
				/* Right and Width determine the box. */
				ld->bounds[R] = pd->ancestor[R] - pd->inset[R];
				ld->bounds[L] = ld->bounds[R] - pd->width;
			}
			else
			{
				/* Right and ancestor define the box. */
				ld->bounds[L] = pd->ancestor[L];
				ld->bounds[R] = pd->ancestor[R] - pd->inset[R];
			}
		}
		if (fz_css_number_defined(box->style->inset[T]))
		{
			ld->bounds[T] = pd->ancestor[T] + pd->inset[T];
			if (pd->has_height)
			{
				/* Top and Height determine the box. */
				ld->bounds[B] = ld->bounds[T] + pd->height;
			}
			else
			{
				/* Top and Bottom determine the box. */
				ld->bounds[B] = pd->ancestor[B] - pd->inset[T];
			}
		}
		else
		{
			if (pd->has_height)
			{
				/* Bottom and Height determine the box. */
				ld->bounds[B] = pd->ancestor[B] - pd->inset[B];
				ld->bounds[T] = ld->bounds[B] - pd->height;
			}
			else
			{
				/* Bottom and ancestor define the box. */
				ld->bounds[T] = pd->ancestor[T];
				ld->bounds[B] = pd->ancestor[B] - pd->inset[B];
			}
		}
		break;
	}
	}

	box->s.layout.x = ld->bounds[L];
	box->s.layout.w = 0;
	box->s.layout.y = ld->bounds[T];
	box->s.layout.b = ld->bounds[T];

	ld->used[B] = ld->bounds[T];
	ld->used[L] = ld->bounds[L]; /* Deliberate! */

	return eop;
}

/*	post_position: called after we have laid out a block.
 *	Restore anything that was stashed.
 *	Rewrites ld->used as appropriate.
 *
 *	Returns eop
 */
static int
post_position(fz_context *ctx, position_data *pd, layout_data *ld, fz_html_box *box, int widths_set)
{
	int eop = 0; /* dummy */
	fz_html_restarter *restart = ld->restart;
	float em = box->s.layout.em;
	const float *margin = box->u.block.margin;
	const float *border = box->u.block.border;
	const float *padding = box->u.block.padding;

	/* Restore the bounds */
	TRBLCPY(ld->bounds, pd->bounds);

	if (ld->used[B] > box->s.layout.b)
		box->s.layout.b = ld->used[B];

	/* Unless we're still skipping, add the padding/border/margin on the bottom. */
	if (!WE_ARE_SKIPPING(restart))
	{
		if (box->style->position == POS_STATIC || box->style->position == POS_RELATIVE)
		{
			if (pd->has_width && box->s.layout.w < pd->width && !widths_set)
				box->s.layout.w = pd->width;
			if (pd->has_height && box->s.layout.b - box->s.layout.y < pd->height)
				box->s.layout.b = pd->height + box->s.layout.y;
		}

		ld->used[B] = advance_for_spacing(ctx, ld,
			ld->used[B],
			box->u.block.padding[B] + box->u.block.border[B] + box->u.block.margin[B],
			&eop);
	}

	/* In the common case, used is just whatever we laid out to.*/
	ld->used[T] = box->s.layout.y;
	ld->used[R] = box->s.layout.x + box->s.layout.w + padding[R] + border[R] + margin[R];
	ld->used[L] = box->s.layout.x;
	if (ld->used[B] < box->s.layout.b)
		ld->used[B] = box->s.layout.b;

	/* So in the common case, we're done! */
	if (box->style->position == POS_STATIC)
		return eop;

	/* Retore the stashed ancestor */
	TRBLCPY(ld->ancestor, pd->ancestor);

	switch (box->style->position)
	{
	case POS_RELATIVE:
	{
		float x = fz_css_number_defined(box->style->inset[L]) ? pd->inset[L] : -pd->inset[R];
		float y = fz_css_number_defined(box->style->inset[T]) ? pd->inset[T] : -pd->inset[B];

		/* Space is taken out of the flow as if this stuff had been laid out without
		 * relativeness. So we just need to move the content. */
		shift_box_contents(box, x, y);

		break;
	}
	case POS_FIXED:
	{
		/* Some adjustments are required:
		 * At this point, width will be correct, height will not.
		 * Contents will be positioned as if from top/left.
		 */
		float page[4];

		page[T] = ld->page[T] + margin[T] + border[T] + padding[T];
		page[R] = ld->page[R] - margin[R] - border[R] - padding[R];
		page[B] = ld->page[B] - margin[B] - border[B] - padding[B];
		page[L] = ld->page[L] + margin[L] + border[L] + padding[L];

		if (fz_css_number_defined(box->style->inset[L]))
		{
			if (pd->has_width)
			{
				/* Already fine */
				box->s.layout.w = pd->width;
			}
			else
			{
				/* Already fine */
				(void) box;
			}
		}
		else
		{
			if (pd->has_width)
			{
				/* Right and Width determine the box. */
				/* Already fine */
				box->s.layout.w = pd->width;
			}
			else
			{
				/* Right and page define the box. */
				/* We laid this out on the left, cos we didn't know the width. Now we
				 * know the width, shift it right. */
				shift_box_contents(box, page[R] - page[L] - box->s.layout.w - pd->inset[R], 0);
			}
		}
		if (fz_css_number_defined(box->style->inset[T]))
		{
			if (fz_css_number_defined_not_auto(box->style->height))
			{
				float h = fz_from_css_number(box->style->height, em, em, 0);
				/* Top and Height determine the box. */
				box->s.layout.b = box->s.layout.y + h;
			}
			else
			{
				/* Top and page bottom determine the box. */
				/* Leave the height as is. */
				(void) box;
			}
		}
		else
		{
			if (pd->has_height)
			{
				/* Bottom and Height determine the box. */
				box->s.layout.b = box->s.layout.y + pd->height;
			}
			else
			{
				/* Bottom and page define the box. */
				shift_box_contents(box, 0, page[B] - box->s.layout.b - pd->inset[B]);
			}
		}
		/* As far as the caller is concerned, no space used. */
		ld->used[T] = ld->used[B];
		ld->used[L] = ld->used[R];
		break;
	}
	case POS_ABSOLUTE:
	{
		/* Some adjustments are required:
		 * At this point, width will be correct, height will not.
		 * Contents will be positioned as if from top/left.
		 */
		if (fz_css_number_defined(box->style->inset[L]))
		{
			if (pd->has_width)
			{
				/* Already fine */
				box->s.layout.w = pd->width;
			}
			else
			{
				/* Already fine */
				(void) box;
	}
		}
		else
		{
			if (pd->has_width)
			{
				/* Right and Width determine the box. */
				/* Already fine */
				box->s.layout.w = pd->width;
			}
			else
			{
				/* Right and ancestor define the box. */
				/* We laid this out on the left, cos we didn't know the width. Now we
				 * know the width, shift it right. */
				shift_box_contents(box, ld->ancestor[R] - ld->ancestor[L] - box->s.layout.w - pd->inset[R], 0);
			}
		}
		if (fz_css_number_defined(box->style->inset[T]))
		{
			if (pd->has_height)
			{
				/* Top and Height determine the box. */
				box->s.layout.b = box->s.layout.y + pd->height;
			}
			else
			{
				/* Top and ancestor bottom determine the box. */
				/* Leave the height as is. */
				(void) box;
			}
		}
		else
		{
			if (pd->has_height)
			{
				/* Bottom and Height determine the box. */
				box->s.layout.b = box->s.layout.y + pd->height;
			}
			else
			{
				/* Bottom and ancestor bottom define the box. */
				shift_box_contents(box, 0, ld->ancestor[B] - box->s.layout.b - pd->inset[B]);
			}
		}
		/* As far as the caller is concerned, no space used. */
		ld->used[T] = ld->used[B];
		ld->used[L] = ld->used[R];
		break;
	}
	}

	return eop;
}


static int layout_block(fz_context *ctx, layout_data *ld, fz_html_box *box);
static int layout_table(fz_context *ctx, layout_data *ld, fz_html_box *box);

/* === LAYOUT TABLE === */

/* The column widths here should include any border widths and padding.
 * This is consistent with how the 'width' figures are understood in HTML.
 *
 * For the border-collapse cases, this is modified slightly.
 * Each column only contains half of each border as they are shared with
 * their neighbours. The exception to this is for the edges of the table,
 * when they contain the full widths.
 */
typedef struct {
	int fixed;
	float min, max, actual;
} column_width;

typedef struct
{
	/* If set, this bit means this cell is either spanned, or omitted from the definition. */
	unsigned int spanned : 1;
	/* If set, this bit means that the cell is defined with a "fixed" width. */
	unsigned int fixed : 1;
	float minw;
	float maxw;
	uint16_t rowspan;
	uint16_t colspan;
	fz_html_box *box;
} table_cell;

typedef struct
{
	/* How large the grid is allocated as */
	int max_w;
	int max_h;
	/* How large the grid is populated */
	int w;
	int h;
	/* Where are we in filling the grid */
	int x;
	int y;

	float spacing;

	table_cell *cells;

	float *row_b;
	float *row_max_bottom_border;
} table_grid;

static table_grid *
new_table_grid(fz_context *ctx, float spacing)
{
	table_grid *grid = fz_malloc_struct(ctx, table_grid);

	grid->spacing = spacing;

	return grid;
}

static void drop_table_grid(fz_context *ctx, table_grid *grid)
{
	if (grid)
	{
		fz_free(ctx, grid->cells);
		fz_free(ctx, grid->row_b);
		fz_free(ctx, grid->row_max_bottom_border);
		fz_free(ctx, grid);
	}
}

static void resize_grid(fz_context *ctx, table_grid *grid, int w, int h)
{
	int y;

	assert(w >= grid->max_w && h >= grid->max_h);

	grid->cells = fz_realloc(ctx, grid->cells, sizeof(grid->cells[0]) * w * h);

	if (grid->max_w != w)
	{
		for (y = grid->max_h-1; y >= 0; y--)
		{
			memmove(&grid->cells[y * w], &grid->cells[y * grid->max_w], sizeof(grid->cells[0]) * grid->max_w);
			memset(&grid->cells[y * w + grid->max_w], 0, sizeof(grid->cells[0]) * (w - grid->max_w));
		}
	}
	if (grid->max_h != h)
		memset(&grid->cells[w * grid->max_h], 0, sizeof(grid->cells[0]) * w * (h - grid->max_h));
	grid->max_w = w;
	grid->max_h = h;
}

static table_cell *cell_at(fz_context *ctx, table_grid *grid, int x, int y)
{
	if (x >= grid->max_w || y >= grid->max_h)
	{
		int new_w = grid->max_w;
		int new_h = grid->max_h;
		if (new_w == 0)
			new_w = 8;
		if (new_h == 0)
			new_h = 8;
		while (x >= new_w)
			new_w *= 2;
		while (y >= new_h)
			new_h *= 2;
		resize_grid(ctx, grid, new_w, new_h);
	}

	if (y >= grid->h)
		grid->h = y+1;
	if (x >= grid->w)
	{
		/* If we're widening the active area, make sure the unused cells above
		 * are marked as spanning. */
		int x0, y0;
		for (y0 = 0; y0 < grid->y-1; y0++)
			for (x0 = grid->w; x0 <= x; x0++)
				grid->cells[grid->max_w * y0 + x0].spanned = 1;
		grid->w = x+1;
	}


	return &grid->cells[grid->max_w * y + x];
}

static table_cell *next_table_cell(fz_context *ctx, table_grid *grid, fz_html_box *box, int colspan, int rowspan)
{
	table_cell *cell;
	int x, y;

	while (1)
	{
		cell = cell_at(ctx, grid, grid->x, grid->y);
		if (!cell->spanned)
			break;
		grid->x++;
	}

	if (colspan < 1)
		colspan = 1;
	if (rowspan < 1)
		rowspan = 1;

	/* Mark all as being spanned. */
	for (y = 0; y < rowspan; y++)
		for (x = 0; x < colspan; x++)
		{
			table_cell *cell2 = cell_at(ctx, grid, grid->x + x, grid->y + y);
			cell2->spanned = 1;
		}

	/* Reload the original cell (as it may have moved due to resizing) */
	cell = cell_at(ctx, grid, grid->x, grid->y);
	cell->spanned = 0;
	cell->colspan = colspan;
	cell->rowspan = rowspan;
	cell->box = box;

	grid->x += colspan;

	return cell;
}

static void next_table_row(fz_context *ctx, table_grid *grid)
{
	while (grid->x < grid->w)
	{
		table_cell *cell = cell_at(ctx, grid, grid->x, grid->y);
		cell->spanned = 1;
		grid->x++;
	}
	grid->x = 0;
	grid->y++;
}

static column_width *
table_grid_complete(fz_context *ctx, table_grid *grid, float top)
{
	int w = grid->w;
	int h = grid->h;
	int x, y, x2;
	column_width *colw = Memento_label(fz_calloc(ctx, grid->w, sizeof(column_width)), "column_width");

	/* Abuse fixed to serve double duty as meaning "no clue" */
	for (x = 0; x < w; x++)
		colw[x].fixed = -1;

	/* Look for widths, without colspan confusing them. */
	for (x = 0; x < w; x++)
	{
		float minw = 0, maxw = 0;
		int fixed = 0;
		int found = 0;
		for (y = 0; y < h; y++)
		{
			/* Can never throw, as x/y smaller than grid->w/h */
			table_cell *cell = cell_at(ctx, grid, x, y);
			if (cell->spanned || cell->colspan > 1)
				continue;
			if (cell->fixed)
			{
				fixed = 1;
				minw = cell->minw;
				maxw = cell->maxw;
				found = 1;
			}
			else if (fixed)
			{
				/* We should ignore a fixed size if another cell's minimum width is larger. */
				if (cell->minw > minw)
				{
					minw = cell->minw;
					if (minw > maxw)
						maxw = minw;
				}
			}
			else
			{
				/* Logic looks wrong, but isn't! */
				if (!found || cell->minw > minw)
					minw = cell->minw;
				if (cell->maxw > maxw)
					maxw = cell->maxw;
				found = 1;
			}
		}
		if (found)
		{
			colw[x].min = minw;
			colw[x].max = maxw;
			colw[x].fixed = fixed;
		}
	}

	/* Now we look at colspanning cells. */
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			float minw;
			int unknown, nonfixed;
			table_cell *cell = cell_at(ctx, grid, x, y);
			if (cell->spanned || cell->colspan == 1)
				continue;

			/* So, we have a spanning cell starting at (x, y) */
			minw = 0;
			unknown = 0;
			nonfixed = 0;
			for (x2 = x; x2 < x + cell->colspan; x2++)
			{
				if (colw[x2].fixed == -1)
				{
					unknown++;
					continue;
				}
				if (!colw[x2].fixed)
					nonfixed++;
				minw += colw[x2].min;
			}

			if (minw < cell->minw)
			{
				/* We need to boost the minimum widths. */
				float boost = (cell->minw - minw);
				if (unknown > 0)
				{
					/* Divide the required space between 'unknown' columns
					 * that currently have no width. */
					boost /= unknown;
					for (x2 = x; x2 < x + cell->colspan; x2++)
					{
						if (colw[x2].fixed == -1)
						{
							colw[x2].fixed = cell->fixed;
							colw[x2].min = boost;
							colw[x2].max = colw[x2].min;
						}
					}
				}
				else if (nonfixed > 0)
				{
					/* Divide the required space between 'nonfixed' columns
					 * that don't have fixed widths. */
					boost /= nonfixed;
					for (x2 = x; x2 < x + cell->colspan; x2++)
					{
						if (colw[x2].fixed)
							continue;
						colw[x2].fixed = cell->fixed;
						colw[x2].min += boost;
						if (colw[x2].max < colw[x2].min)
							colw[x2].max = colw[x2].min;
					}
				}
				else
				{
					/* We need more space, but everything is fixed! There seems to be
					 * a contradiction here. Just live with it. */
				}
			}
		}
	}

	/* Everything should now be set.
	 * But it might not be.
	 * <table><tbody><tr><th colspan="3">A</th></tr>
	 *        <tr><td>B</td><td>C</td>
	 * </tbody></table>
	 * will give us a 3 column table, with no clue what the size should be for column 3.
	*/
	for (x = 0; x < w; x++)
	{
		if (colw[x].fixed == -1)
			colw[x].fixed = 0;
	}

	fz_try(ctx)
	{
		grid->row_b = fz_malloc(ctx, grid->h * sizeof(float));
		grid->row_max_bottom_border = fz_calloc(ctx, grid->h, sizeof(float));
	}
	fz_catch(ctx)
	{
		fz_free(ctx, colw);
		fz_rethrow(ctx);
	}

	/* Don't add any spacing in here, because we may be skipping!
	 * We just want a good enough value to use for 'min'. */
	for (y = 0; y < h; y++)
		grid->row_b[y] = top;

	return colw;
}

static float table_cell_padding(fz_context *ctx, fz_html_box *box)
{
	return (
		box->u.block.padding[L] + box->u.block.border[L] +
		box->u.block.padding[R] + box->u.block.border[R]
	);
}

typedef struct
{
	float max_left_border;
	float max_right_border;
} border_collapse_info;

static float table_cell_padding_collapsed(fz_context *ctx, fz_html_box *box, border_collapse_info *bci, int leftmost, int rightmost)
{
	float left = leftmost ? bci->max_left_border : box->u.block.border[L]/2;
	float right = rightmost ? bci->max_right_border : box->u.block.border[R]/2;
	return (
		box->u.block.padding[L] + left +
		box->u.block.padding[R] + right
	);
}

static float block_padding(fz_context *ctx, fz_html_box *box)
{
	return (
		box->u.block.padding[L] + box->u.block.border[L] + box->u.block.margin[L] +
		box->u.block.padding[R] + box->u.block.border[R] + box->u.block.margin[R]
	);
}

static float largest_min_width(fz_context *ctx, fz_html_box *box)
{
	/* The "minimum" width is the largest word in a paragraph. */
	float r_min = 0;
	if (box->type == BOX_BLOCK)
	{
		fz_html_box *child = box->down;
		while (child)
		{
			float min = largest_min_width(ctx, child);
			if (min > r_min)
				r_min = min;
			child = child->next;
		}
		r_min += block_padding(ctx, box);
	}
	else if (box->type == BOX_FLOW)
	{
		fz_html_flow *flow;
		for (flow = box->u.flow.head; flow; flow = flow->next)
			if (flow->w > r_min)
				r_min = flow->w;
	}
	else
	{
		// TODO: nested TABLE
	}
	return r_min;
}

static float largest_max_width(fz_context *ctx, fz_html_box *box)
{
	/* The "maximum" width is the length of the longest paragraph laid out on one line. */
	float r_max = 0;
	if (box->type == BOX_BLOCK)
	{
		fz_html_box *child = box->down;
		while (child)
		{
			float max = largest_max_width(ctx, child);
			if (max > r_max)
				r_max = max;
			child = child->next;
		}
		r_max += block_padding(ctx, box);
	}
	else if (box->type == BOX_FLOW)
	{
		fz_html_flow *flow;
		float max = 0;
		for (flow = box->u.flow.head; flow; flow = flow->next)
		{
			max += flow->w;
			if (flow->type == FLOW_BREAK)
			{
				if (max > r_max)
					r_max = max;
				max = 0;
			}
		}
		if (max > r_max)
			r_max = max;
	}
	else
	{
		// TODO: nested TABLE
	}
	return r_max;
}

static void squish_block(fz_context *ctx, fz_html_box *box)
{
	fz_html_box *child;

	box->s.layout.b = box->s.layout.y;
	if (box->type == BOX_FLOW)
		return;
	for (child = box->down; child; child = child->next)
		squish_block(ctx, child);
}

static void layout_table_row(fz_context *ctx, layout_data *ld, table_grid *grid, int celly, fz_html_box *row, int ncol, column_width *colw, float spacing, border_collapse_info *border_collapse)
{
	fz_html_box *cell, *child;
	int col = 0;
	float x = row->s.layout.x;
	float y;
	float saved_bounds[4];
	float row_height;
	float em = row->s.layout.em;
	int fixed_height = 0;
	table_cell *tcell;
	float max_cell_border_above, max_cell_border_below;

	/* Always layout the full row since we can't restart in the middle of a cell.
	 * If the row doesn't fit fully, we'll postpone it to the next page.
	 * FIXME: If the row doesn't fit then either, we should split it with the offset.
	 */
	fz_html_restarter *save_restart = ld->restart;
	ld->restart = NULL;

	/* Note: margin is ignored for table cells and rows */

	TRBLCPY(saved_bounds, ld->bounds);

	if (fz_css_number_defined_not_auto(row->style->height))
	{
		row_height = fz_from_css_number(row->style->height, em, em, 0);
		fixed_height = 1;
	}

	/* Find the maximum cell border in this row. */
	max_cell_border_above = 0;
	max_cell_border_below = 0;
	for (cell = row->down; cell; cell = cell->next)
	{
		if (max_cell_border_above < cell->u.block.border[T])
			max_cell_border_above = cell->u.block.border[T];
		if (max_cell_border_below < cell->u.block.border[B])
			max_cell_border_below = cell->u.block.border[B];
	}

	/* For each cell in the row */
	for (cell = row->down; cell; cell = cell->next)
	{
		float cell_pad = table_cell_padding(ctx, cell);
		int colspan, rowspan;
		float cellw;
		int i;

		/* Find the cell record in the grid. */
		while (1)
		{
			assert(col < grid->w && celly <= grid->h);
			tcell = cell_at(ctx, grid, col, celly);
			if (tcell->box == cell)
				break;
			x += colw[col++].actual + spacing;
		}
		colspan = tcell->colspan;
		assert(colspan >= 1);
		rowspan = tcell->rowspan;
		assert(rowspan >= 1);

		/* Find the actual width of this cell */
		cellw = 0;
		for (i = 0; i < colspan; i++)
			cellw += colw[col+i].actual + (i != 0 ? spacing : 0);

		x += spacing;

		/* Position the cell */
		ld->bounds[T] = row->s.layout.y + max_cell_border_above + cell->u.block.padding[T];
		ld->bounds[L] = x + cell->u.block.padding[L] + cell->u.block.border[L];
		ld->bounds[R] = ld->bounds[L] + cellw - cell_pad;
		ld->bounds[B] = cell->s.layout.y;
		if (border_collapse)
		{
			if (col == 0)
			{
				/* Adjust the left hand edge so that all the borders line up */
				ld->bounds[L] += border_collapse->max_left_border/2;
			}
			/* We share half the border with our predecessor, so move back a bit. */
			ld->bounds[L] -= cell->u.block.border[L]/2;
			if (col + colspan == grid->w)
			{
				/* Adjust the right hand edge so that all the borders line up */
				ld->bounds[R] -= border_collapse->max_right_border/2;
			}
			/* We share half the border with our successor, so move on a bit. */
			ld->bounds[R] += cell->u.block.border[R]/2;
			/* Reuse T and B margins to allow for the required inner cell alignment spacing */
			cell->u.block.margin[T] = (max_cell_border_above - cell->u.block.border[T])/2;
			cell->u.block.margin[B] = (max_cell_border_below - cell->u.block.border[B])/2;
		}
		else
		{
			/* Reuse T and B margins to allow for the required inner cell alignment spacing */
			cell->u.block.margin[T] = max_cell_border_above - cell->u.block.border[T];
			cell->u.block.margin[B] = max_cell_border_below - cell->u.block.border[B];
		}

		cell->s.layout.x = ld->bounds[L];
		cell->s.layout.w = ld->bounds[R] - ld->bounds[L];
		cell->s.layout.y = ld->bounds[T];

		/* Layout cell contents into the cell. */
		for (child = cell->down; child; child = child->next)
		{
			if (child->type == BOX_BLOCK)
			{
				(void)layout_block(ctx, ld, child);
			}
			else if (child->type == BOX_TABLE)
			{
				(void)layout_table(ctx, ld, child);
			}
			else if (child->type == BOX_FLOW)
			{
				layout_flow(ctx, ld, child, cell);
			}
			ld->bounds[T] = ld->used[B];
		}
		cell->s.layout.b = ld->used[B];
		if (fz_css_number_defined_not_auto(cell->style->height))
		{
			float cellheight = fz_from_css_number(cell->style->height, em, em, 0);
			if (cell->s.layout.b < cell->s.layout.y + cellheight)
			cell->s.layout.b = cell->s.layout.y + cellheight;
		}
		else if (fixed_height && cell->s.layout.b < cell->s.layout.y + row_height)
			cell->s.layout.b = cell->s.layout.y + row_height;

		/* Advance to next column */
		x += colw[col].actual;

		/* Update row_b (most importantly for this row, but ensure the others
		 * are plausibly increasing too). */
		y = cell->s.layout.b + cell->u.block.padding[B] + cell->u.block.border[B] + cell->u.block.margin[B];
		if (border_collapse)
		{
			/* Half our border is shared with the next cell. */
			y -= cell->u.block.border[B]/2;
		}
		for (i = celly + rowspan - 1; i < grid->h; i++)
		{
			if (grid->row_b[i] < y)
				grid->row_b[i] = y;
			y += spacing;
		}

		if (border_collapse)
		{
			if (grid->row_max_bottom_border[celly + rowspan - 1] < cell->u.block.border[B])
				grid->row_max_bottom_border[celly + rowspan - 1] = cell->u.block.border[B];
		}

		++col;
	}

	row->s.layout.b = grid->row_b[celly];

	ld->restart = save_restart;

	TRBLCPY(ld->bounds, saved_bounds);
}

static inline int
cell_rowspan(fz_html_box *cell)
{
	int r = cell->style->rowspan;
	if (r < 1)
		return 1;
	return r;
}

static inline int
cell_colspan(fz_html_box *cell)
{
	int c = cell->style->colspan;
	if (c < 1)
		return 1;
	return c;
}

static void
fixup_collapsed_cell_bottoms_for_row(fz_context *ctx, table_grid *grid, int row, fz_html_box *box)
{
	int row0;
	fz_html_box *rowbox, *cell;

	for (row0 = 0, rowbox = box->down; row0 <= row && rowbox; row0++, rowbox = rowbox->next)
	{
		for (cell = rowbox->down; cell; cell = cell->next)
		{
			float y;
			int rowspan = cell_rowspan(cell);
			if (row0 + rowspan - 1 != row)
				continue;

			y = grid->row_b[row0 + rowspan - 1] - (cell->u.block.padding[B] + cell->u.block.border[B]/2 + cell->u.block.margin[B]);
			if (cell->style->vertical_align == VA_MIDDLE || cell->style->vertical_align == VA_BOTTOM)
			{
				float offset = (y - cell->s.layout.b);
				if (cell->style->vertical_align == VA_MIDDLE)
					offset /= 2;
				shift_box_contents(cell, 0, offset);
				cell->s.layout.y -= offset;
			}
			cell->s.layout.b = y;
		}
	}
}

static void
fixup_cell_bottoms(fz_context *ctx, table_grid *grid, fz_html_box *box)
{
	fz_html_box *rowbox, *cell;
	int row;

	/* Second pass, set the bottoms of all cells in the tables. */
	for (row = 0, rowbox = box; row <= grid->h && rowbox; row++, rowbox = rowbox->next)
	{
		for (cell = rowbox->down; cell; cell = cell->next)
		{
			float y;
			int rowspan = cell_rowspan(cell);

			y = grid->row_b[row + rowspan - 1] - (cell->u.block.padding[B] + cell->u.block.border[B] + cell->u.block.margin[B]);
			if (cell->style->vertical_align == VA_MIDDLE || cell->style->vertical_align == VA_BOTTOM)
			{
				float offset = (y - cell->s.layout.b);
				if (cell->style->vertical_align == VA_MIDDLE)
					offset /= 2;
				shift_box_contents(cell, 0, offset);
				cell->s.layout.y -= offset;
			}
			cell->s.layout.b = y;
		}
	}
}

static void collapse_table_borders(fz_context *ctx, table_grid *grid, border_collapse_info *bci)
{
	int w = grid->w;
	int h = grid->h;
	int x, y, x1, y1;

	/* Collapse columns */
	bci->max_right_border = 0;
	for (x = 0; x < w-1; x++)
	{
		for (y = 0; y < h; y++)
		{
			table_cell *cell0 = cell_at(ctx, grid, x, y);
			table_cell *cell1;
			float b0, b1;

			if (cell0->spanned == 1 || cell0->box == NULL)
				continue;

			cell0->box->collapsed_cell = 1;

			x1 = x + cell0->colspan;
			if (x1 == w)
			{
				if (bci->max_right_border < cell0->box->u.block.border[R])
					bci->max_right_border = cell0->box->u.block.border[R];
				continue;
			}

			cell1 = cell_at(ctx, grid, x1, y);
			if (cell1->box == NULL)
				continue;
			b0 = cell0->box->u.block.border[R];
			b1 = cell1->box->u.block.border[L];

			if (b0 < b1 && b1 >= 0)
			{
				cell0->box->u.block.border[R] = b1;
				cell0->box->suppress_border |= 1<<R;
			}
			else
			{
				cell1->box->u.block.border[L] = b0;
				cell1->box->suppress_border |= 1<<L;
			}
		}
	}
	for (y = 0; y < h; y++)
	{
		table_cell *cell0 = cell_at(ctx, grid, w-1, y);

		if (cell0->spanned == 1 || cell0->box == NULL)
			continue;

		x1 = x + cell0->colspan;
		if (x1 == w)
		{
			if (bci->max_right_border < cell0->box->u.block.border[R])
				bci->max_right_border = cell0->box->u.block.border[R];
			continue;
		}
	}
	bci->max_left_border = 0;
	for (y = 0; y < h; y++)
	{
		table_cell *cell0 = cell_at(ctx, grid, 0, y);

		if (cell0->spanned == 1 || cell0->box == NULL)
			continue;
		if (bci->max_left_border < cell0->box->u.block.border[L])
			bci->max_left_border = cell0->box->u.block.border[L];
	}

	/* Collapse rows */
	for (y = 0; y < h-1; y++)
	{
		for (x = 0; x < w; x++)
		{
			table_cell *cell0 = cell_at(ctx, grid, x, y);
			table_cell *cell1;
			float b0, b1;

			if (cell0->spanned == 1 || cell0->box == NULL)
				continue;

			y1 = y + cell0->rowspan;
			if (y1 >= h)
				continue;

			cell1 = cell_at(ctx, grid, x, y1);
			if (cell1->box == NULL)
				continue;
			b0 = cell0->box->u.block.border[B];
			b1 = cell1->box->u.block.border[T];

			if (b0 < b1 && b1 >= 0)
			{
				cell0->box->u.block.border[B] = b1;
				cell0->box->suppress_border |= 1<<B;
			}
			else
			{
				cell1->box->u.block.border[T] = b0;
				cell1->box->suppress_border |= 1<<T;
			}
		}
	}
}

static int layout_table(fz_context *ctx, layout_data *ld, fz_html_box *box)
{
	fz_html_box *row;
	int col, ncol;
	float min_tabw, max_tabw;
	column_width *colw = NULL;
	fz_html_restarter *restart = ld->restart;
	float spacing;
	int eop = 0;
	float avail_w;
	table_grid *table;
	int x, y;
	int table_has_width;
	int border_collapse = box->style->border_collapse;
	border_collapse_info bci;

	position_data position;

	spacing = fz_from_css_number(box->style->border_spacing, box->s.layout.em, box->s.layout.w, 0);
	if (border_collapse)
		spacing = 0;

	if (restart)
	{
		/* We have reached the restart point */
		if (restart->start == box)
			restart->start = NULL;
	}

	/* We honour positions on tables. We don't honour positions on elements of tables (like rows/cells etc). */
	eop = pre_position(ctx, &position, ld, box, 1);

	if (eop)
	{
		if (restart && restart->end == NULL)
		{
			box->s.layout.b = box->s.layout.y;
			if (restart->potential)
				restart->end = restart->potential;
			else
				restart->end = box;
			return 0;
		}
	}

	/* Add initial border-spacing */
	ld->used[B] += spacing;
	box->s.layout.b += spacing;

	table = new_table_grid(ctx, spacing);

	fz_var(colw);

	fz_try(ctx)
	{
		/* Table Autolayout algorithm from HTML */
		/* https://www.w3.org/TR/REC-html40/appendix/notes.html#h-B.5.2 */

		/* Make the grid. */
		for (row = box->down; row; row = row->next)
		{
			fz_html_box *cell;
			for (cell = row->down; cell; cell = cell->next)
				next_table_cell(ctx, table, cell, cell->style->colspan, cell->style->rowspan);
			next_table_row(ctx, table);
		}

		/* Collapse the cell borders if required. */
		if (border_collapse)
			collapse_table_borders(ctx, table, &bci);

		/* Now walk the cells, populating minw/maxw/fixed. */
		for (y = 0; y < table->h; y++)
		{
			for (x = 0; x < table->w; x++)
		{
			fz_html_box *cell, *child;
				float cell_pad;
				table_cell *tc = cell_at(ctx, table, x, y);

				if (tc->spanned)
					continue;

				cell = tc->box;
				if (cell == NULL)
					continue;

				if (border_collapse)
					cell_pad = table_cell_padding_collapsed(ctx, cell, &bci, x == 0, x + tc->colspan == table->w);
				else
					cell_pad = table_cell_padding(ctx, cell);
				if (fz_css_number_defined_not_auto(cell->style->width))
				{
					float em = box->s.layout.em;
					float w = fz_from_css_number(cell->style->width, em, em, 0) + cell_pad;
					tc->fixed = 1;
					tc->minw = w;
					tc->maxw = w;
				}
				else
			{
					float cellminw = 0;
					float cellmaxw = 0;
				for (child = cell->down; child; child = child->next)
				{
					float min = largest_min_width(ctx, child) + cell_pad;
					float max = largest_max_width(ctx, child) + cell_pad;
						/* The logic here looks screwy, but isn't! */
						if (min > cellminw)
							cellminw = min;
						if (max > cellmaxw)
							cellmaxw = max;
				}
					tc->fixed = 0;
					tc->minw = cellminw;
					tc->maxw = cellmaxw;
			}
		}
		}

		/* Now we can calculate the column widths. */
		colw = table_grid_complete(ctx, table, box->s.layout.b);
		ncol = table->w;

		min_tabw = max_tabw = 0;
		for (col = 0; col < ncol; ++col)
		{
			min_tabw += colw[col].min;
			max_tabw += colw[col].max;
		}
		min_tabw += spacing * (ncol + 1);
		max_tabw += spacing * (ncol + 1);

		avail_w = fz_from_css_number(box->style->width, box->s.layout.em, ld->bounds[R] - ld->bounds[L], ld->bounds[R] - ld->bounds[L]);
		table_has_width = fz_css_number_defined_not_auto(box->style->width);

		if (min_tabw >= avail_w)
		{
		/* The minimum table width is equal to or wider than the available space.
			 * Assign the minimum widths and let the lines overflow...
		 */
			box->s.layout.w = min_tabw;
			for (col = 0; col < ncol; ++col)
				colw[col].actual = colw[col].min;
		}
		else if (max_tabw <= avail_w && !table_has_width)
		{
			/* The maximum table width fits within the available space. */
			/* Set the columns to their maximum widths. */
			box->s.layout.w = max_tabw;
			for (col = 0; col < ncol; ++col)
				colw[col].actual = colw[col].max;
		}
		else
		{
		/* The maximum width of the table is greater than the available space, but
		 * the minimum table width is smaller. In this case, find the difference
		 * between the available space and the minimum table width, lets call it
		 * W. Lets also call D the difference between maximum and minimum width of
		 * the table.
		 *
		 * For each column, let d be the difference between maximum and minimum
		 * width of that column. Now set the column's width to the minimum width
		 * plus d times W over D. This makes columns with large differences
		 * between minimum and maximum widths wider than columns with smaller
		 * differences.
		 */
			float W = (avail_w - min_tabw);
			float D = (max_tabw - min_tabw);
			box->s.layout.w = avail_w;
			if (D == 0)
			{
				for (col = 0; col < ncol; ++col)
					colw[col].actual = colw[col].min + W / ncol;
			}
		else
		{
			for (col = 0; col < ncol; ++col)
				colw[col].actual = colw[col].min + (colw[col].max - colw[col].min) * W / D;
		}
		}

		/* Layout each row in turn. */
		for (row = box->down, y = 0; row && y < table->h; row = row->next, y++)
		{
			/* Position the row, zero height for now. */
			if (box->s.layout.b < table->row_b[y])
				box->s.layout.b = table->row_b[y];
			row->s.layout.x = box->s.layout.x;
			row->s.layout.w = avail_w;
			row->s.layout.y = row->s.layout.b = box->s.layout.b;

			if (restart && restart->start != NULL)
			{
				if (restart->start == row)
					restart->start = NULL;
				else
				{
					squish_block(ctx, row);
					continue; /* still skipping */
				}
			}

			if (border_collapse && y > 0)
			{
				/* When border collapsing, every row's borders overlap the ones from
				 * the one above. */
				row->s.layout.y -= table->row_max_bottom_border[y-1]/2;
			}
			layout_table_row(ctx, ld, table, y, row, ncol, colw, spacing, border_collapse ? &bci : NULL);
			if (border_collapse)
				fixup_collapsed_cell_bottoms_for_row(ctx, table, y, box);

			/* If the row doesn't fit on the current page, break here and put the row on the next page.
			 * Unless the row was at the very start of the page, in which case it'll overflow instead.
			 * FIXME: Don't overflow, draw twice with offset to break it abruptly at the page border!
			 */
			if (ld->page[B] != ld->page[T])
			{
				float page_h = ld->page[B] - ld->page[T];
				float avail = page_h - fmodf(row->s.layout.y - ld->page[T], page_h);
				float used = row->s.layout.b - row->s.layout.y;
				if (used > avail && avail < page_h)
				{
					if (restart)
					{
						restart->end = row;
						goto exit;
					}
					else
					{
						row->s.layout.y += avail;
						layout_table_row(ctx, ld, table, y, row, ncol, colw, spacing, border_collapse ? &bci : NULL);
					}
				}
			}

			box->s.layout.b = row->s.layout.b + spacing;
			ld->used[B] = box->s.layout.b;
		}

		/* And align the cell bottoms. */
		if (!border_collapse)
			fixup_cell_bottoms(ctx, table, box->down);

		exit:;
	}
	fz_always(ctx)
	{
		drop_table_grid(ctx, table);
		fz_free(ctx, colw);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	if (post_position(ctx, &position, ld, box, 1))
	{
		if (SHOULD_STOP_FOR_RESTART(restart))
		{
			if (restart->potential)
				restart->end = restart->potential;
			else
			{
				/* NEVER stop to restart on this object, or we will get into
				 * an infinite loop. We'd rather just risk losing the padding/margin/
				 * border at the bottom. Return 1, so we restart on the next box. */
				/* DO NOT DO: restart->end = box; */
				return 1;
			}
			return 0;
		}
		}

	if (layout_block_page_break(ctx, ld, &ld->used[B], box->style->page_break_after))
	{
		if (SHOULD_STOP_FOR_RESTART(restart))
			return 1;
	}

	return 0;
}

/* === LAYOUT BLOCKS === */

/*
	Layout a BOX_BLOCK.

	ctx: The ctx in use.
	ld: The layout data.
		We should fill into ld->bounds, and update ld->fill to be what we used before we return.
		A lot of the time, ld->fill will be the same as box->u.layout on exit, but (because of
		positioning) not always.
	box: The BOX_BLOCK to layout.

	Return 1 if we should restart at the next block, 0 otherwise.
*/
static int layout_block(fz_context *ctx, layout_data *ld, fz_html_box *box)
{
	fz_html_box *child;
	fz_html_restarter *restart = ld->restart;

	const fz_css_style *style = box->style;
	float em = box->s.layout.em;
	int eop = 0;
	position_data position;

	assert(fz_html_box_has_boxes(box));

	/* If restart is non-NULL, we are running in restartable mode. */
	if (restart)
	{
		/* If restart->start == box, then we've been skipping, and this
		 * is the point at which we should restart. */
		if (restart->start == box)
		{
			if (restart->start_flags & FZ_HTML_RESTARTER_START_END_FLAGS_SPECIFIC_SIDE)
			{
				/* If the side matches, then great. If not, we need to end here! */
				if (!!(restart->start_flags & FZ_HTML_RESTARTER_START_END_FLAGS_LEFT_SIDE) != restart->left_page)
				{
					/* Skip straight to stopping! */
					restart->end = box;
					restart->end_flags = restart->start_flags;
					return 1;
				}
			}
			/* Set restart->start to be NULL to indicate that we aren't skipping
			 * any more. */
			restart->start = NULL;
		}

		/* In the event that no content fits, we want to restart with us, not with the
		 * content that doesn't fit. */
		if (restart->potential == NULL)
			restart->potential = box;
	}

	/* TODO: remove 'vertical' margin adjustments across automatic page breaks */
	eop = layout_block_page_break(ctx, ld, &ld->used[B], style->page_break_before);


	/* Cope with positioned blocks. */
	eop |= pre_position(ctx, &position, ld, box, 0);

	if (eop)
	{
		if (SHOULD_STOP_FOR_RESTART(restart))
		{
			box->s.layout.b = box->s.layout.y = ld->used[B];
			if (restart->potential)
				restart->end = restart->potential;
			else
				restart->end = box;
			return 0;
		}
	}

	/* At this point, fill is already setup. */

	for (child = box->down; child; child = child->next)
	{
		if (SHOULD_STOP_FOR_RESTART(restart))
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
		ld->used[L] = ld->bounds[L];

			if (child->type == BOX_BLOCK)
			eop = layout_block(ctx, ld, child);

		else if (child->type == BOX_TABLE)
			eop = layout_table(ctx, ld, child);

		else if (child->type == BOX_FLOW)
		{
			layout_flow(ctx, ld, child, box);
			if (!WE_ARE_SKIPPING(restart))
			{
			if (child->s.layout.b > child->s.layout.y)
					box->s.layout.b = ld->used[B];
				}
			}

		if (!WE_ARE_SKIPPING(restart))
		{
			if (ld->used[B] > box->s.layout.b)
				box->s.layout.b = ld->used[B];
			if (ld->used[R] > box->s.layout.x + box->s.layout.w)
				box->s.layout.w = ld->used[R] - box->s.layout.x;
			if (ld->used[B] > box->s.layout.b)
				box->s.layout.b = ld->used[B];
		}

		/* Stop if we've reached the endpoint. */
		if (STOPPING_FOR_RESTART(restart))
			break;
		if (restart && box->s.layout.b != box->s.layout.y)
			restart->potential = NULL;

		ld->bounds[T] = ld->used[B];
	}

	if (post_position(ctx, &position, ld, box, 0))
	{
		if (SHOULD_STOP_FOR_RESTART(restart))
		{
			if (restart->potential)
				restart->end = restart->potential;
			else
			{
				/* NEVER stop to restart on this object, or we will get into
				 * an infinite loop. We'd rather just risk losing the padding/margin/
				 * border at the bottom. Return 1, so we restart on the next box. */
				/* DO NOT DO: restart->end = box; */
				return 1;
			}
			return 0;
		}
	}

	/* If we're still skipping, exit. */
	if (WE_ARE_SKIPPING(restart))
		return 0;
	/* If we've reached the endpoint, exit. */
	if (STOPPING_FOR_RESTART(restart))
		return 0;

	/* reserve space for the list mark */
	if (box->list_item && box->s.layout.y == box->s.layout.b)
	{
		float delta = fz_from_css_number_scale(style->line_height, em);
		box->s.layout.b += delta;
		ld->used[B] += delta;
	}

	if (layout_block_page_break(ctx, ld, &ld->used[B], style->page_break_after))
	{
		/* Tricky. We need to restart, but our restart marker should be on
		 * the NEXT one. */
		if (SHOULD_STOP_FOR_RESTART(restart))
			return 1;
	}

	return 0;
}

/* === LAYOUT === */

// Compute new em, padding, border, margin.
// Also compute layout x and w.
static void layout_update_styles(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	float top_em = top->s.layout.em;
	float top_baseline = top->s.layout.baseline;
	float top_w = top->s.layout.w;
	while (box)
	{
		const fz_css_style *style = box->style;
		float em = box->s.layout.em = fz_from_css_number(style->font_size, top_em, top_em, top_em);

		if (style->vertical_align == VA_SUPER)
			box->s.layout.baseline = top_baseline - top_em / 3;
		else if (style->vertical_align == VA_SUB)
			box->s.layout.baseline = top_baseline + top_em / 5;
		else
			box->s.layout.baseline = top_baseline;

		if (box->type != BOX_INLINE && box->type != BOX_FLOW)
		{
			float *margin = box->u.block.margin;
			float *border = box->u.block.border;
			float *padding = box->u.block.padding;

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

			// TODO: BLOCK nested inside TABLE!
			if (box->type == BOX_BLOCK || box->type == BOX_TABLE)
			{
				/* Compute preliminary width (will be adjusted if block is nested in table cell later) */
				float auto_w = top_w - (margin[L] + margin[R] + border[L] + border[R] + padding[L] + padding[R]);
				box->s.layout.w = fz_from_css_number(box->style->width, em, auto_w, auto_w);
			}
		}

		else if (box->type == BOX_FLOW)
		{
			box->s.layout.x = top->s.layout.x;
			box->s.layout.w = top->s.layout.w;
		}

		if (box->down)
			layout_update_styles(ctx, box->down, box);

		box = box->next;
	}
}

static void layout_update_widths(fz_context *ctx, fz_html_box *box, fz_html_box *top, hb_buffer_t *hb_buf)
{
	while (box)
	{
		if (box->type == BOX_FLOW)
		{
			fz_html_flow *node;

			for (node = box->u.flow.head; node; node = node->next)
			{
				if (node->type == FLOW_IMAGE)
				{
					/* start with "native" size (only used for table width calculations) */
					node->w = node->content.image->w * 72.0f / 96.0f;
					node->w = fz_from_css_number(node->box->style->width, top->s.layout.em, node->w, node->w);
				}
				else if (node->type == FLOW_WORD || node->type == FLOW_SPACE || node->type == FLOW_SHYPHEN)
					measure_string_w(ctx, node, hb_buf);
			}
		}

		if (box->down)
			layout_update_widths(ctx, box->down, box, hb_buf);

		box = box->next;
	}
}

static int is_layout_box(fz_html_box *box)
{
	return box->type == BOX_BLOCK || box->type == BOX_TABLE;
}

static int is_empty_block_box(fz_html_box *box)
{
	fz_html_box *child;
	if (box->type == BOX_BLOCK)
	{
		if (box->u.block.padding[T] != 0 || box->u.block.padding[B] != 0)
			return 0;
		if (box->u.block.border[T] != 0 || box->u.block.border[B] != 0)
			return 0;
		for (child = box->down; child; child = child->next)
		{
			if (child->type != BOX_BLOCK)
				return 0;
			if (!is_empty_block_box(child))
				return 0;
			if (child->u.block.margin[T] != 0 || child->u.block.margin[B] != 0)
				return 0;
		}
		return 1;
	}
	return 0;
}

static void layout_collapse_margin_with_children(fz_context *ctx, fz_html_box *here)
{
	fz_html_box *child, *first, *last = NULL;

	first = NULL;
	for (child = here->down; child; child = child->next)
	{
		/* Ignore absolute or fixed position blocks. */
		if (child->style->position != POS_FIXED && child->style->position != POS_ABSOLUTE)
		{
			if (first == NULL)
				first = child;
			last = child;
		}
		layout_collapse_margin_with_children(ctx, child);
	}

	if (here->style->position == POS_FIXED || here->style->position == POS_ABSOLUTE)
		return;

	if (is_layout_box(here))
	{
		if (first && is_layout_box(first))
		{
			if (first->u.block.border[T] == 0 && first->u.block.padding[T] == 0)
			{
				float m = fz_max(first->u.block.margin[T], here->u.block.margin[T]);
				here->u.block.margin[T] = m;
				first->u.block.margin[T] = 0;
			}
		}

		if (last && is_layout_box(last))
		{
			if (last->u.block.border[T] == 0 && last->u.block.padding[T] == 0)
			{
				float m = fz_max(last->u.block.margin[B], here->u.block.margin[B]);
				here->u.block.margin[B] = m;
				last->u.block.margin[B] = 0;
			}
		}
	}
}

static void layout_collapse_margin_with_siblings(fz_context *ctx, fz_html_box *here)
{
	fz_html_box *prev = NULL;

	while (here)
	{
		if (here->down)
			layout_collapse_margin_with_siblings(ctx, here->down);

		if (here->style->position != POS_ABSOLUTE && here->style->position != POS_FIXED)
		{
			if (prev && is_layout_box(prev) && is_layout_box(here))
			{
				float m = fz_max(prev->u.block.margin[B], here->u.block.margin[T]);
				prev->u.block.margin[B] = m;
				here->u.block.margin[T] = 0;
			}
			prev = here;
		}
		here = here->next;
	}
}

static void layout_collapse_margin_with_self(fz_context *ctx, fz_html_box *here)
{
	while (here)
	{
		if (here->down)
			layout_collapse_margin_with_self(ctx, here->down);

		if (is_layout_box(here) && is_empty_block_box(here))
		{
			float m = fz_max(here->u.block.margin[T], here->u.block.margin[B]);
			here->u.block.margin[T] = 0;
			here->u.block.margin[B] = m;
		}

		here = here->next;
	}
}

static void layout_collapse_margins(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	// Vertical margins are collapsed in these cases:
	// 1) Adjacent siblings
	// 2) No content separating parent and descendants. The margin ends up outside the parent.
	// 3) Empty blocks

	layout_collapse_margin_with_self(ctx, box);
	layout_collapse_margin_with_children(ctx, box);
	layout_collapse_margin_with_siblings(ctx, box);
}

void
fz_restartable_layout_html(fz_context *ctx, fz_html_tree *tree, float start_x, float start_y, float page_w, float page_h, float em, fz_html_restarter *restart)
{
	int unlocked = 0;
	layout_data ld = { 0 };
	fz_html_box *box = tree->root;

	fz_var(ld.hb_buf);
	fz_var(unlocked);

	// nothing to layout
	if (!box->down)
	{
		fz_warn(ctx, "html: nothing to layout");
		box->s.layout.em = em;
		box->s.layout.baseline = 0;
		box->s.layout.x = start_x;
		box->s.layout.w = page_w;
		box->s.layout.y = start_y;
		box->s.layout.b = start_y;
		return;
	}

	fz_hb_lock(ctx);

	fz_try(ctx)
	{
		Memento_startLeaking(); /* HarfBuzz leaks harmlessly */
		ld.hb_buf = hb_buffer_create();
		Memento_stopLeaking(); /* HarfBuzz leaks harmlessly */
		unlocked = 1;
		fz_hb_unlock(ctx);

		ld.restart = restart;
		ld.page[T] = start_y;
		ld.page[R] = start_x + page_w;
		ld.page[B] = start_y + page_h;
		ld.page[L] = start_x;
		TRBLCPY(ld.ancestor, ld.page);
		TRBLCPY(ld.bounds, ld.page);
		ld.pool = tree->pool;
		if (restart)
			restart->potential = NULL;

		// Update em/margin/padding/border if necessary.
		if (box->s.layout.em != em || box->s.layout.x != start_x || box->s.layout.w != page_w)
		{
			box->s.layout.em = em;
			box->s.layout.baseline = 0;
			box->s.layout.x = start_x;
			box->s.layout.w = page_w;
			layout_update_styles(ctx, box->down, box);
			layout_update_widths(ctx, box->down, box, ld.hb_buf);
			layout_collapse_margins(ctx, box->down, box);
		}

		assert(box->type == BOX_BLOCK);
		layout_block(ctx, &ld, box);
	}
	fz_always(ctx)
	{
		if (unlocked)
			fz_hb_lock(ctx);
		hb_buffer_destroy(ld.hb_buf);
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

	fz_restartable_layout_html(ctx, &html->tree, 0, 0, html->page_w, html->page_h, em, NULL);

	if (h == 0)
		html->page_h = html->tree.root->s.layout.b;

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

/* === DRAW === */

typedef struct
{
	float rgb[3];
	float a;
} unpacked_color;

static inline unpacked_color
unpack_color(const fz_css_color src)
{
	unpacked_color dst;
	dst.rgb[0] = src.r / 255.0f;
	dst.rgb[1] = src.g / 255.0f;
	dst.rgb[2] = src.b / 255.0f;
	dst.a = src.a / 255.0f;
	return dst;
}

static inline int
color_eq(fz_css_color a, fz_css_color b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int draw_flow_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_flow *node;
	fz_text *text = NULL;
	fz_path *line = NULL;
	fz_matrix trm;
	fz_css_color prev_color = { 0, 0, 0, 0 };
	fz_css_color prev_fill_color = { 0, 0, 0, 0 };
	fz_css_color prev_stroke_color = { 0, 0, 0, 0 };
	float line_width, prev_line_width = 0;
	int filling, prev_filling = 0;
	int stroking, prev_stroking = 0;
	int restartable_ended = 0;
	fz_stroke_state *ss = NULL;
	fz_stroke_state *line_ss = NULL;

	fz_var(text);
	fz_var(line);
	fz_var(ss);
	fz_var(line_ss);

	/* FIXME: HB_DIRECTION_TTB? */

	if (restart && restart->start != NULL && restart->start != box)
		return 0;

	fz_try(ctx)
	{
		for (node = box->u.flow.head; node; node = node->next)
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
				float em;

				if (node->type == FLOW_SPACE && node->breaks_line)
					continue;
				if (node->type == FLOW_SHYPHEN && !node->breaks_line)
					continue;
				if (style->visibility != V_VISIBLE)
					continue;

				em = node->box->s.layout.em;

				line_width = fz_from_css_number(style->text_stroke_width, em, em, 0);
				filling = style->text_fill_color.a != 0;
				stroking = style->text_stroke_color.a != 0;
				if (stroking)
				{
					if (ss == NULL)
						ss = fz_new_stroke_state(ctx);
				}
				if (line)
				{
					if (line_ss == NULL)
						line_ss = fz_new_stroke_state(ctx);
				}

				if (	/* If we've changed whether we're filling... */
					filling != prev_filling ||
					/* Or we're filling and the color has changed... */
					(prev_filling && !color_eq(style->text_fill_color, prev_fill_color)) ||
					/* Or we've changed whether we're stroking... */
					stroking != prev_stroking ||
					/* Or we're stroking, and the color or linewidth has changed... */
					(prev_stroking && (!color_eq(style->text_stroke_color, prev_stroke_color) || line_width != prev_line_width)))
				{
					if (text)
					{
						if (prev_filling)
						{
							unpacked_color color = unpack_color(prev_fill_color);
							fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
						}
						if (prev_stroking)
						{
							unpacked_color color = unpack_color(prev_stroke_color);
							ss->linewidth = prev_line_width;
							fz_stroke_text(ctx, dev, text, ss, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
						}
						fz_drop_text(ctx, text);
						text = NULL;
					}
					prev_filling = filling;
					prev_stroking = stroking;
				}
				prev_fill_color = style->text_fill_color;
				prev_stroke_color = style->text_stroke_color;
				prev_line_width = line_width;

				if (!color_eq(style->color, prev_color))
				{
					if (line)
					{
						unpacked_color color = unpack_color(prev_color);
						fz_stroke_path(ctx, dev, line, line_ss, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
						fz_drop_path(ctx, line);
						line = NULL;
					}
					prev_color = style->color;
				}

				if (style->text_decoration > 0)
				{
					if (!line)
					{
						line = fz_new_path(ctx);
						if (line_ss == NULL)
							line_ss = fz_new_stroke_state(ctx);
					}
					if (style->text_decoration & TD_UNDERLINE)
					{
						fz_moveto(ctx, line, node->x, node->y + 1.5f - page_top);
						fz_lineto(ctx, line, node->x + node->w, node->y + 1.5f - page_top);
					}
					if (style->text_decoration & TD_LINE_THROUGH)
					{
						fz_moveto(ctx, line, node->x, node->y - em * 0.3f - page_top);
						fz_lineto(ctx, line, node->x + node->w, node->y - em * 0.3f - page_top);
					}
				}

				if (!text)
					text = fz_new_text(ctx);

				if (node->bidi_level & 1)
					x = node->x + node->w;
				else
					x = node->x;
				y = node->y;

				trm.a = em;
				trm.b = 0;
				trm.c = 0;
				trm.d = -em;
				trm.e = x;
				trm.f = y - page_top;

				s = get_node_text(ctx, node);
				init_string_walker(ctx, &walker, hb_buf, node->bidi_level & 1, style->font, node->script, node->markup_lang, style->small_caps, s, 0);
				while (walk_string(&walker))
				{
					float node_scale = node->box->s.layout.em / walker.scale;
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
					if (filling)
					{
						unpacked_color color = unpack_color(prev_fill_color);
						fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
					}
					if (stroking)
					{
						unpacked_color color = unpack_color(prev_stroke_color);
						ss->linewidth = line_width;
						fz_stroke_text(ctx, dev, text, ss, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
					}
					fz_drop_text(ctx, text);
					text = NULL;
				}
				if (style->visibility == V_VISIBLE)
				{
					float alpha = style->color.a / 255.0f;
					fz_matrix itm = fz_pre_translate(ctm, node->x, node->y - page_top);
					itm = fz_pre_scale(itm, node->w, node->h);
					fz_fill_image(ctx, dev, node->content.image, itm, alpha, fz_default_color_params);
				}
			}
		}

		if (text)
		{
			if (filling)
			{
				unpacked_color color = unpack_color(prev_fill_color);
				fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
			}
			if (stroking)
			{
				unpacked_color color = unpack_color(prev_stroke_color);
				ss->linewidth = prev_line_width;
				fz_stroke_text(ctx, dev, text, ss, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
			}
			fz_drop_text(ctx, text);
			text = NULL;
		}

		if (line)
		{
			unpacked_color color = unpack_color(prev_color);
			fz_stroke_path(ctx, dev, line, line_ss, ctm, fz_device_rgb(ctx), color.rgb, color.a, fz_default_color_params);
			fz_drop_path(ctx, line);
			line = NULL;
		}
	}
	fz_always(ctx)
	{
		fz_drop_text(ctx, text);
		fz_drop_path(ctx, line);
		fz_drop_stroke_state(ctx, ss);
		fz_drop_stroke_state(ctx, line_ss);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return restartable_ended;
}

static void draw_rect(fz_context *ctx, fz_device *dev, fz_matrix ctm, float page_top, fz_css_color color, float x0, float y0, float x1, float y1)
{
		float rgb[3];
	fz_path *path;

	if (color.a <= 0)
		return;

	path = fz_new_path(ctx);

	fz_try(ctx)
	{
		fz_moveto(ctx, path, x0, y0 - page_top);
		fz_lineto(ctx, path, x1, y0 - page_top);
		fz_lineto(ctx, path, x1, y1 - page_top);
		fz_lineto(ctx, path, x0, y1 - page_top);
		fz_closepath(ctx, path);

		rgb[0] = color.r / 255.0f;
		rgb[1] = color.g / 255.0f;
		rgb[2] = color.b / 255.0f;

		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, color.a / 255.0f, fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void draw_quad(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_css_color color,
	float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3)
{
	float rgb[3];
	fz_path *path;

	if (color.a <= 0)
		return;

	path = fz_new_path(ctx);

	fz_try(ctx)
	{
		fz_moveto(ctx, path, x0, y0);
		fz_lineto(ctx, path, x1, y1);
		fz_lineto(ctx, path, x2, y2);
		fz_lineto(ctx, path, x3, y3);
		fz_closepath(ctx, path);

		rgb[0] = color.r / 255.0f;
		rgb[1] = color.g / 255.0f;
		rgb[2] = color.b / 255.0f;

		fz_fill_path(ctx, dev, path, 0, ctm, fz_device_rgb(ctx), rgb, color.a / 255.0f, fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fudge_len(float *wp, float len, int count, float extra)
{
	/* We have a line of length len, in which we have to fit n*count+extra sections
	 * for some integer n, n > 1. We'd like to find n s.t. each of those sections is
	 * approximately w.
	 *
	 * So, ideally len = (n*count+extra)*w
	 * (len/w - extra)/count = n
	 * ((len - w*extra)/count/w = n
	 */
	float w = *wp;
	int n = 0;

	assert(w > 0 && count > 0);

	if (len > w*extra)
		n = ((len - w*extra) + (count*w)/2) / (count*w);
	if (n < 1)
		n = 1;

	*wp = len / (n * count + extra);
}

static void draw_dotted_line(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_css_color color, int dashed, float w,
	float x0, float y0, float x1, float y1)
{
	float rgb[3];
	fz_path *path;
	fz_stroke_state *stroke;
	float len;

	if (color.a <= 0 || w <= 0)
		return;

	/* Rely on the fact that we only draw othogonal lines here! */
	len = x1 - x0 + y1 - y0;
	if (len < 0)
		len = -len;

	path = fz_new_path(ctx);
	stroke = NULL;

	fz_var(stroke);

	fz_try(ctx)
	{
		fz_moveto(ctx, path, x0, y0);
		fz_lineto(ctx, path, x1, y1);

		rgb[0] = color.r / 255.0f;
		rgb[1] = color.g / 255.0f;
		rgb[2] = color.b / 255.0f;

		stroke = fz_new_stroke_state_with_dash_len(ctx, 2);
		stroke->linewidth = w;
		stroke->dash_len = 2;
		if (dashed)
		{
			fudge_len(&w, len, 4, 1);
			stroke->start_cap = FZ_LINECAP_SQUARE;
			stroke->dash_cap = FZ_LINECAP_SQUARE;
			stroke->end_cap = FZ_LINECAP_SQUARE;
			stroke->dash_list[0] = w;
			stroke->dash_list[1] = w*3;
		}
		else
		{
			fudge_len(&w, len, 2, 0.01f);
			stroke->start_cap = FZ_LINECAP_ROUND;
			stroke->dash_cap = FZ_LINECAP_ROUND;
			stroke->end_cap = FZ_LINECAP_ROUND;
			stroke->dash_list[0] = 0;
			stroke->dash_list[1] = w*2;
		}

		fz_stroke_path(ctx, dev, path, stroke, ctm, fz_device_rgb(ctx), rgb, color.a / 255.0f, fz_default_color_params);
	}
	fz_always(ctx)
	{
		fz_drop_stroke_state(ctx, stroke);
		fz_drop_path(ctx, path);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
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
			return box->u.flow.head;
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
	float color[4];
	const char *s;
	char buf[40];
	int c, g;

	trm = fz_scale(box->s.layout.em, -box->s.layout.em);

	line = find_list_mark_anchor(ctx, box);
	if (line)
	{
		y = line->y;
	}
	else
	{
		float h = fz_from_css_number_scale(box->style->line_height, box->s.layout.em);
		float a = box->s.layout.em * 0.8f;
		float d = box->s.layout.em * 0.2f;
		if (a + d > h)
			h = a + d;
		y = box->s.layout.y + a + (h - a - d) / 2;
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
		w += fz_advance_glyph(ctx, font, g, 0) * box->s.layout.em;
	}

	text = fz_new_text(ctx);

	fz_try(ctx)
	{
		s = buf;
		trm.e = box->s.layout.x - w;
		trm.f = y - page_top;
		while (*s)
		{
			s += fz_chartorune(&c, s);
			g = fz_encode_character_with_fallback(ctx, box->style->font, c, UCDN_SCRIPT_LATIN, FZ_LANG_UNSET, &font);
			fz_show_glyph(ctx, text, font, trm, g, c, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
			trm.e += fz_advance_glyph(ctx, font, g, 0) * box->s.layout.em;
		}

		color[0] = box->style->color.r / 255.0f;
		color[1] = box->style->color.g / 255.0f;
		color[2] = box->style->color.b / 255.0f;
		color[3] = box->style->color.a / 255.0f;

		fz_fill_text(ctx, dev, text, ctm, fz_device_rgb(ctx), color, color[3], fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_text(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int draw_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart);
static int draw_table_row(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart);

static int draw_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	int ret = 0;
	int str = fz_html_tag_to_structure(box->tag);

	if (str != FZ_STRUCTURE_INVALID)
		fz_begin_structure(ctx, dev, str, box->tag, 0);

	switch (box->type)
	{
	case BOX_TABLE_ROW:
		if (restart && restart->end == box)
			ret = 1;
		else if (draw_table_row(ctx, box, page_top, page_bot, dev, ctm, hb_buf, restart))
			ret = 1;
		break;
	case BOX_TABLE:
	case BOX_TABLE_CELL:
	case BOX_BLOCK:
		if (restart && restart->end == box)
			ret = 1;
		else if (draw_block_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf, restart))
			ret = 1;
		break;
	case BOX_FLOW:
		if (draw_flow_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf, restart))
			ret = 1;
		break;
	}

	if (str != FZ_STRUCTURE_INVALID)
		fz_end_structure(ctx, dev);

	return ret;
}

static fz_css_color
brighten(fz_css_color c)
{
	int delta;
	float S;
	float C;
	float H;

	if (c.r > c.g)
	{
		if (c.b > c.r)
		{
			/* b  > r > g */
			/* All 6 cases are very similar, we'll just comment the first one. */
			delta = c.b - c.g; /* Never zero */
			S = delta/(float)c.b;
			/* A proper conversion from RGB -> HSV would map H into a sextant;
			 * given we're going to convert back, we don't bother. */
			H = (c.r - c.g)/(float)delta; /* Proper H would be: 4 <= H < 5 */
			c.b = (c.b>>1) + 128;
			/* v = cmax = c.b */
			C = c.b * S;
			/* m = cmax - C */
			c.g = c.b - C;
			c.r = c.g + C * H;
			c.b = c.g + C;
		}
		else if (c.b > c.g)
		{
			/* r >= b > g */
			delta = c.r - c.g; /* Never zero */
			S = delta/(float)c.r;
			H = (c.b - c.g)/(float)delta; /* Proper H would be: 5 <= H < 6 */
			c.r = (c.r>>1) + 128;
			C = c.r * S;
			c.g = c.r - C;
			c.b = c.g + C * H;
			c.r = c.g + C;
		}
		else
		{
			/* r > g >= b */
			delta = c.r - c.b; /* Never zero */
			S = delta/(float)c.r;
			H = (c.g - c.b)/(float)delta; /* Proper H would be: 0 <= H < 1 */
			c.r = (c.r>>1) + 128;
			C = c.r * S;
			c.b = c.r - C;
			c.g = c.b + C * H;
			c.r = c.b + C;
		}
	}
	else
	{
		if (c.b > c.g)
		{
			/* b > g >= r */
			delta = c.b - c.r; /* Never zero */
			S = delta/(float)c.b;
			H = (c.g - c.r)/(float)delta; /* Proper H would be: 3 <= H < 4 */
			c.b = (c.b>>1) + 128;
			C = c.b * S;
			c.r = c.b - C;
			c.g = c.r + C * H;
			c.b = c.r + C;
		}
		else if (c.r > c.b)
		{
			/* g >= r > b */
			delta = c.g - c.b; /* Never zero */
			S = delta/(float)c.g;
			H = (c.r - c.b)/(float)delta; /* Proper H would be: 1 <= H < 2 */
			c.g = (c.g>>1) + 128;
			C = c.g * S;
			c.b = c.g - C;
			c.r = c.b + C * H;
			c.g = c.b + C;
		}
		else if (c.g == c.b && c.g == c.r)
		{
			/* r = g = b */
			H = 0;
			c.g = (c.g>>1) + 128;
			c.r = c.g;
			c.b = c.g;
		}
		else
		{
			/* g >= b >= r */
			delta = c.g - c.r; /* Never zero */
			S = delta/(float)c.g;
			H = (c.b - c.r)/(float)delta; /* Proper H would be: 2 <= H < 3 */
			c.g = (c.g>>1) + 128;
			C = c.g * S;
			c.r = c.g - C;
			c.b = c.r + C * H;
			c.g = c.r + C;
		}
	}

	return c;
}

static fz_css_color
darken(fz_css_color c)
{
	int delta;
	float S;
	float C;
	float H;

	if (c.r > c.g)
	{
		if (c.b > c.r)
		{
			/* b  > r > g */
			/* All 6 cases are very similar, we'll just comment the first one. */
			delta = c.b - c.g; /* Never zero */
			S = delta/(float)c.b;
			/* A proper conversion from RGB -> HSV would map H into a sextant;
			 * given we're going to convert back, we don't bother. */
			H = (c.r - c.g)/(float)delta; /* Proper H would be: 4 <= H < 5 */
			c.b >>= 1;
			/* v = cmax = c.b */
			C = c.b * S;
			/* m = cmax - C */
			c.g = c.b - C;
			c.r = c.g + C * H;
			c.b = c.g + C;
		}
		else if (c.b > c.g)
		{
			/* r >= b > g */
			delta = c.r - c.g; /* Never zero */
			S = delta/(float)c.r;
			H = (c.b - c.g)/(float)delta; /* Proper H would be: 5 <= H < 6 */
			c.r >>= 1;
			C = c.r * S;
			c.g = c.r - C;
			c.b = c.g + C * H;
			c.r = c.g + C;
		}
		else
		{
			/* r > g >= b */
			delta = c.r - c.b; /* Never zero */
			S = delta/(float)c.r;
			H = (c.g - c.b)/(float)delta; /* Proper H would be: 0 <= H < 1 */
			c.r >>= 1;
			C = c.r * S;
			c.b = c.r - C;
			c.g = c.b + C * H;
			c.r = c.b + C;
		}
	}
	else
	{
		if (c.b > c.g)
		{
			/* b > g >= r */
			delta = c.b - c.r; /* Never zero */
			S = delta/(float)c.b;
			H = (c.g - c.r)/(float)delta; /* Proper H would be: 3 <= H < 4 */
			c.b >>= 1;
			C = c.b * S;
			c.r = c.b - C;
			c.g = c.r + C * H;
			c.b = c.r + C;
		}
		else if (c.r > c.b)
		{
			/* g >= r > b */
			delta = c.g - c.b; /* Never zero */
			S = delta/(float)c.g;
			H = (c.r - c.b)/(float)delta; /* Proper H would be: 1 <= H < 2 */
			c.g >>= 1;
			C = c.g * S;
			c.b = c.g - C;
			c.r = c.b + C * H;
			c.g = c.b + C;
		}
		else if (c.g == c.b && c.g == c.r)
		{
			/* r = g = b */
			H = 0;
			c.g >>= 1;
			c.r = c.g;
			c.b = c.g;
		}
		else
		{
			/* g >= b >= r */
			delta = c.g - c.r; /* Never zero */
			S = delta/(float)c.g;
			H = (c.b - c.r)/(float)delta; /* Proper H would be: 2 <= H < 3 */
			c.g >>= 1;
			C = c.g * S;
			c.r = c.g - C;
			c.b = c.r + C * H;
			c.g = c.r + C;
		}
	}

	return c;
}

static int collapse_border_style(int style, int collapsed)
{
	if (collapsed)
	{
		if (style == BS_INSET)
			style = BS_RIDGE;
		if (style == BS_OUTSET)
			style = BS_GROOVE;
	}
	return style;
}

static void
draw_border(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_html_box *box, int edge, float x0, float y0, float x1, float y1)
{
	float *border = box->u.block.border;
	const fz_css_style *style = box->style;
	int collapsed = box->collapsed_cell;

	switch (edge)
	{
	case T:
		switch (collapse_border_style(style->border_style_0, collapsed))
		{
		case BS_NONE:
			return; /* Should never happen */
		case BS_OUTSET:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[T]), x0 - border[L], y0 - border[T], x0, y0, x1, y0, x1 + border[R], y0 - border[T]);
			break;
		case BS_INSET:
			draw_quad(ctx, dev, ctm, darken(style->border_color[T]), x0 - border[L], y0 - border[T], x0, y0, x1, y0, x1 + border[R], y0 - border[T]);
			break;
		case BS_RIDGE:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[T]), x0 - border[L], y0 - border[T], x0 - border[L]/2, y0 - border[T]/2, x1 + border[R]/2, y0 - border[T]/2, x1 + border[R], y0 - border[T]);
			draw_quad(ctx, dev, ctm, darken(style->border_color[T]), x0 - border[L]/2, y0 - border[T]/2, x0, y0, x1, y0, x1 + border[R]/2, y0 - border[T]/2);
			break;
		case BS_GROOVE:
			draw_quad(ctx, dev, ctm, darken(style->border_color[T]), x0 - border[L], y0 - border[T], x0 - border[L]/2, y0 - border[T]/2, x1 + border[R]/2, y0 - border[T]/2, x1 + border[R], y0 - border[T]);
			draw_quad(ctx, dev, ctm, brighten(style->border_color[T]), x0 - border[L]/2, y0 - border[T]/2, x0, y0, x1, y0, x1 + border[R]/2, y0 - border[T]/2);
			break;
		case BS_DOUBLE:
			draw_quad(ctx, dev, ctm, style->border_color[T], x0 - border[L], y0 - border[T], x0 - 2*border[L]/3, y0 - 2*border[T]/3, x1 + 2*border[R]/3, y0 - 2*border[T]/3, x1 + border[R], y0 - border[T]);
			draw_quad(ctx, dev, ctm, style->border_color[T], x0 - border[L]/3, y0 - border[T]/3, x0, y0, x1, y0, x1 + border[R]/3, y0 - border[T]/3);
			break;
		case BS_DOTTED:
		{
			float left = border[L] - border[T]/2;
			float right = border[R] - border[T]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[T], 0, border[T], x0 - left, y0 - border[T]/2, x1 + right, y0 - border[T]/2);
			break;
		}
		case BS_DASHED:
		{
			float left = border[L] - border[T]/2;
			float right = border[R] - border[T]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[T], 1, border[T], x0 - left, y0 - border[T]/2, x1 + right, y0 - border[T]/2);
			break;
		}
		case BS_SOLID:
			draw_rect(ctx, dev, ctm, 0, style->border_color[T], x0 - border[L], y0 - border[T], x1 + border[R], y0);
			break;
		}
		break;
	case R:
		switch (collapse_border_style(style->border_style_1, collapsed))
		{
		case BS_NONE:
			return; /* Should never happen */
		case BS_OUTSET:
			draw_quad(ctx, dev, ctm, darken(style->border_color[R]), x1 + border[R], y0 - border[T], x1, y0, x1, y1, x1 + border[R], y1 + border[B]);
			break;
		case BS_INSET:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[R]), x1 + border[R], y0 - border[T], x1, y0, x1, y1, x1 + border[R], y1 + border[B]);
			break;
		case BS_RIDGE:
			draw_quad(ctx, dev, ctm, darken(style->border_color[R]), x1 + border[R], y0 - border[T], x1 + border[R]/2, y0 - border[T]/2, x1 + border[R]/2, y1 + border[B]/2, x1 + border[R], y1 + border[B]);
			draw_quad(ctx, dev, ctm, brighten(style->border_color[R]), x1 + border[R]/2, y0 - border[T]/2, x1, y0, x1, y1, x1 + border[R]/2, y1 + border[B]/2);
			break;
		case BS_GROOVE:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[R]), x1 + border[R], y0 - border[T], x1 + border[R]/2, y0 - border[T]/2, x1 + border[R]/2, y1 + border[B]/2, x1 + border[R], y1 + border[B]);
			draw_quad(ctx, dev, ctm, darken(style->border_color[R]), x1 + border[R]/2, y0 - border[T]/2, x1, y0, x1, y1, x1 + border[R]/2, y1 + border[B]/2);
			break;
		case BS_DOUBLE:
			draw_quad(ctx, dev, ctm, style->border_color[R], x1 + border[R], y0 - border[T], x1 + 2*border[R]/3, y0 - 2*border[T]/3, x1 + 2*border[R]/3, y1 + 2*border[B]/3, x1 + border[R], y1 + border[B]);
			draw_quad(ctx, dev, ctm, style->border_color[R], x1 + border[R]/3, y0 - border[T]/3, x1, y0, x1, y1, x1 + border[R]/3, y1 + border[B]/3);
			break;
		case BS_DOTTED:
		{
			float top = border[T] - border[R]/2;
			float bottom = border[B] - border[R]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[R], 0, border[R], x1 + border[R]/2, y0 - top, x1 + border[R]/2, y1 + bottom);
			break;
		}
		case BS_DASHED:
		{
			float top = border[T] - border[R]/2;
			float bottom = border[B] - border[R]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[R], 1, border[R], x1 + border[R]/2, y0 - top, x1 + border[R]/2, y1 + bottom);
			break;
		}
		case BS_SOLID:
			draw_rect(ctx, dev, ctm, 0, style->border_color[R], x1, y0 - border[T], x1 + border[R], y1 + border[B]);
			break;
		}
		break;
	case B:
		switch (collapse_border_style(style->border_style_2, collapsed))
		{
		case BS_NONE:
			return; /* Should never happen */
		case BS_OUTSET:
			draw_quad(ctx, dev, ctm, darken(style->border_color[B]), x0 - border[L], y1 + border[B], x0, y1, x1, y1, x1 + border[R], y1 + border[B]);
			break;
		case BS_INSET:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[B]), x0 - border[L], y1 + border[B], x0, y1, x1, y1, x1 + border[R], y1 + border[B]);
			break;
		case BS_RIDGE:
			draw_quad(ctx, dev, ctm, darken(style->border_color[B]), x0 - border[L], y1 + border[B], x0 - border[L]/2, y1 + border[B]/2, x1 + border[R]/2, y1 + border[B]/2, x1 + border[R], y1 + border[B]);
			draw_quad(ctx, dev, ctm, brighten(style->border_color[B]), x0 - border[L]/2, y1 + border[B]/2, x0, y1, x1, y1, x1 + border[R]/2, y1 + border[B]/2);
			break;
		case BS_GROOVE:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[B]), x0 - border[L], y1 + border[B], x0 - border[L]/2, y1 + border[B]/2, x1 + border[R]/2, y1 + border[B]/2, x1 + border[R], y1 + border[B]);
			draw_quad(ctx, dev, ctm, darken(style->border_color[B]), x0 - border[L]/2, y1 + border[B]/2, x0, y1, x1, y1, x1 + border[R]/2, y1 + border[B]/2);
			break;
		case BS_DOUBLE:
			draw_quad(ctx, dev, ctm, style->border_color[B], x0 - border[L], y1 + border[B], x0 - 2*border[L]/3, y1 + 2*border[B]/3, x1 + 2*border[R]/3, y1 + 2*border[B]/3, x1 + border[R], y1 + border[B]);
			draw_quad(ctx, dev, ctm, style->border_color[B], x0 - border[L]/3, y1 + border[B]/3, x0, y1, x1, y1, x1 + border[R]/3, y1 + border[B]/3);
			break;
		case BS_DOTTED:
		{
			float left = border[L] - border[B]/2;
			float right = border[R] - border[B]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[B], 0, border[B], x1 + right, y1 + border[B]/2, x0 - left, y1 + border[B]/2);
			break;
		}
		case BS_DASHED:
		{
			float left = border[L] - border[B]/2;
			float right = border[R] - border[B]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[B], 1, border[B], x1 + right, y1 + border[B]/2, x0 - left, y1 + border[B]/2);
			break;
		}
		case BS_SOLID:
			draw_rect(ctx, dev, ctm, 0, style->border_color[B], x0 - border[L], y1, x1 + border[R], y1 + border[B]);
			break;
		}
		break;
	case L:
		switch (collapse_border_style(style->border_style_3, collapsed))
		{
		case BS_NONE:
			return; /* Should never happen */
		case BS_OUTSET:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[L]), x0 - border[L], y0 - border[T], x0, y0, x0, y1, x0 - border[L], y1 + border[B]);
			break;
		case BS_INSET:
			draw_quad(ctx, dev, ctm, darken(style->border_color[L]), x0 - border[L], y0 - border[T], x0, y0, x0, y1, x0 - border[L], y1 + border[B]);
			break;
		case BS_RIDGE:
			draw_quad(ctx, dev, ctm, brighten(style->border_color[L]), x0 - border[L], y0 - border[T], x0 - border[L]/2, y0 - border[T]/2, x0 - border[L]/2, y1 + border[B]/2, x0 - border[L], y1 + border[B]);
			draw_quad(ctx, dev, ctm, darken(style->border_color[L]), x0 - border[L]/2, y0 - border[T]/2, x0, y0, x0, y1, x0 - border[L]/2, y1 + border[B]/2);
			break;
		case BS_GROOVE:
			draw_quad(ctx, dev, ctm, darken(style->border_color[L]), x0 - border[L], y0 - border[T], x0 - border[L]/2, y0 - border[T]/2, x0 - border[L]/2, y1 + border[B]/2, x0 - border[L], y1 + border[B]);
			draw_quad(ctx, dev, ctm, brighten(style->border_color[L]), x0 - border[L]/2, y0 - border[T]/2, x0, y0, x0, y1, x0 - border[L]/2, y1 + border[B]/2);
			break;
		case BS_DOUBLE:
			draw_quad(ctx, dev, ctm, style->border_color[L], x0 - border[L], y0 - border[T], x0 - 2*border[L]/3, y0 - 2*border[T]/3, x0 - 2*border[L]/3, y1 + 2*border[B]/3, x0 - border[L], y1 + border[B]);
			draw_quad(ctx, dev, ctm, style->border_color[L], x0 - border[L]/3, y0 - border[T]/3, x0, y0, x0, y1, x0 - border[L]/3, y1 + border[B]/3);
			break;
		case BS_DOTTED:
		{
			float top = border[T] - border[L]/2;
			float bottom = border[B] - border[L]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[L], 0, border[L], x0 - border[L]/2, y1 + bottom, x0 - border[L]/2, y0 - top);
			break;
		}
		case BS_DASHED:
		{
			float top = border[T] - border[L]/2;
			float bottom = border[B] - border[L]/2;
			draw_dotted_line(ctx, dev, ctm, style->border_color[L], 1, border[L], x0 - border[L]/2, y1 + bottom, x0 - border[L]/2, y0 - top);
			break;
		}
		case BS_SOLID:
			draw_rect(ctx, dev, ctm, 0, style->border_color[L], x0 - border[L], y0 - border[T], x0, y1 + border[B]);
			break;
		}
		break;
	}
}

static void
do_borders(fz_context *ctx, fz_device *dev, fz_matrix ctm, float page_top, fz_html_box *box, int suppress)
{
	float cell_adjust_top = box->type == BOX_TABLE_CELL ? box->u.block.margin[T] : 0;
	float cell_adjust_right = box->type == BOX_TABLE_CELL ? box->u.block.margin[R] : 0;
	float cell_adjust_bot = box->type == BOX_TABLE_CELL ? box->u.block.margin[B] : 0;
	float cell_adjust_left = box->type == BOX_TABLE_CELL ? box->u.block.margin[L] : 0;
	float *border = box->u.block.border;
	float *padding = box->u.block.padding;
	float x0 = box->s.layout.x - padding[L] - cell_adjust_left;
	float y0 = box->s.layout.y - padding[T] - page_top - cell_adjust_top;
	float x1 = box->s.layout.x + box->s.layout.w + padding[R] + cell_adjust_right;
	float y1 = box->s.layout.b + padding[B] - page_top + cell_adjust_bot;

	suppress |= box->suppress_border;

	if (border[T] > 0 && !(suppress & (1<<T)))
		draw_border(ctx, dev, ctm, box, T, x0, y0, x1, y1);
	if (border[R] > 0 && !(suppress & (1<<R)))
		draw_border(ctx, dev, ctm, box, R, x0, y0, x1, y1);
	if (border[B] > 0 && !(suppress & (1<<B)))
		draw_border(ctx, dev, ctm, box, B, x0, y0, x1, y1);
	if (border[L] > 0 && !(suppress & (1<<L)))
		draw_border(ctx, dev, ctm, box, L, x0, y0, x1, y1);
}

static int draw_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	fz_html_box *child;
	float x0, y0, x1, y1;

	float *padding = box->u.block.padding;
	int stopped = 0;
	int skipping;
	float cell_padding_top = box->type == BOX_TABLE_CELL ? box->u.block.margin[T] : 0;
	float cell_padding_bot = box->type == BOX_TABLE_CELL ? box->u.block.margin[B] : 0;

	assert(fz_html_box_has_boxes(box));
	x0 = box->s.layout.x - padding[L];
	y0 = box->s.layout.y - padding[T];
	x1 = box->s.layout.x + box->s.layout.w + padding[R];
	y1 = box->s.layout.b + padding[B];

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

	/* Only draw the content if it's visible */
	if (box->style->visibility == V_VISIBLE)
	{
		int suppress;

		/* We draw the background rectangle regardless if we are skipping or not, because
		 * we might find the end-of-skip point inside this box. If there is no content
		 * then the box height will be 0, so nothing will be drawn. */
		if (y1 > y0)
			draw_rect(ctx, dev, ctm, page_top, box->style->background_color, x0, y0 - cell_padding_top, x1, y1 + cell_padding_bot);

		if (!skipping)
		{
			/* Draw a selection of borders. */
			/* If we are restarting, don't do the bottom one yet. */
			suppress = restart ? (1<<B) : 0;
			do_borders(ctx, dev, ctm, page_top, box, suppress);

			if (box->list_item)
				draw_list_mark(ctx, box, page_top, page_bot, dev, ctm, box->list_item);
		}
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

static int draw_table_row(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf, fz_html_restarter *restart)
{
	/* Table rows don't draw background colors or borders */

	fz_html_box *child;

	float y0 = box->s.layout.y;
	float y1 = box->s.layout.b;
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

	for (child = box->down; child; child = child->next)
		if (draw_box(ctx, child, page_top, page_bot, dev, ctm, hb_buf, restart))
			return 1;

	return 0;
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

	fz_draw_restarted_html(ctx, dev, ctm, html->tree.root, page_top, page_bot, NULL);
}

void fz_draw_story(fz_context *ctx, fz_story *story, fz_device *dev, fz_matrix ctm)
{
	float page_top, page_bot;
	fz_path *clip;
	fz_rect bbox;

	if (story == NULL || story->complete)
		return;

	bbox = story->bbox;
	page_top = bbox.y0;
	page_bot = bbox.y1;

	if (dev)
	{
		clip = fz_new_path(ctx);
		fz_try(ctx)
		{
			fz_moveto(ctx, clip, bbox.x0, bbox.y0);
			fz_lineto(ctx, clip, bbox.x1, bbox.y0);
			fz_lineto(ctx, clip, bbox.x1, bbox.y1);
			fz_lineto(ctx, clip, bbox.x0, bbox.y1);
			fz_closepath(ctx, clip);
			fz_clip_path(ctx, dev, clip, 0, ctm, bbox);
		}
		fz_always(ctx)
			fz_drop_path(ctx, clip);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	story->restart_draw.left_page ^= 1;
	story->restart_place = story->restart_draw;
	if (dev)
		fz_draw_restarted_html(ctx, dev, ctm, story->tree.root->down, 0, page_bot+page_top, &story->restart_place);
	story->restart_place.start = story->restart_draw.end;
	story->restart_place.start_flow = story->restart_draw.end_flow;
	story->restart_place.start_flags = story->restart_draw.end_flags;
	story->restart_place.end = NULL;
	story->restart_place.end_flow = NULL;
	story->restart_place.end_flags = 0;
	story->rect_count++;

	if (story->restart_place.start == NULL)
		story->complete = 1;

	if (dev)
		fz_pop_clip(ctx, dev);
}

void fz_reset_story(fz_context *ctx, fz_story *story)
{
	if (story == NULL)
		return;

	story->restart_place.start = NULL;
	story->restart_place.start_flow = NULL;
	story->restart_place.start_flags = 0;
	story->restart_place.end = NULL;
	story->restart_place.end_flow = NULL;
	story->restart_place.end_flags = 0;
	story->restart_place.left_page = 0;
	story->restart_draw.start = NULL;
	story->restart_draw.start_flow = NULL;
	story->restart_draw.start_flags = 0;
	story->restart_draw.end = NULL;
	story->restart_draw.end_flow = NULL;
	story->restart_draw.end_flags = 0;
	story->restart_draw.left_page = 0;
	story->rect_count = 0;
	story->complete = 0;
}

static char *
gather_text(fz_context *ctx, fz_html_box *box)
{
	fz_html_flow *node;
	char *text = NULL;

	fz_var(text);

	fz_try(ctx)
	{
		for (node = box->u.flow.head; node; node = node->next)
		{
			const fz_css_style *style = node->box->style;

			if (node->type == FLOW_WORD || node->type == FLOW_SPACE || node->type == FLOW_SHYPHEN)
			{
				const char *s;

				if (node->type == FLOW_SPACE && node->breaks_line)
					continue;
				if (node->type == FLOW_SHYPHEN && !node->breaks_line)
					continue;
				if (style->visibility != V_VISIBLE)
					continue;

				s = get_node_text(ctx, node);

				if (text)
				{
					size_t newsize = strlen(text) + strlen(s) + 1;
					text = fz_realloc(ctx, text, newsize);
					strcat(text, s);
				}
				else
				{
					text = fz_strdup(ctx, s);
				}
			}
			else if (node->type == FLOW_IMAGE)
			{
			}
		}

	}
	fz_catch(ctx)
	{
		fz_free(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

static int enumerate_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_story_position_callback *cb, void *arg, int depth, int rect_num, fz_html_restarter *restart);

static int enumerate_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_story_position_callback *cb, void *arg, int depth, int rect_num, fz_html_restarter *restart)
{
	fz_html_box *child;
	float y0, y1;

	float *padding = box->u.block.padding;
	int stopped = 0;
	int skipping;
	fz_story_element_position pos;
	int heading;

	assert(fz_html_box_has_boxes(box));
	y0 = box->s.layout.y - padding[T];
	y1 = box->s.layout.b + padding[B];

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

	if (box->style->visibility == V_VISIBLE && !skipping)
	{
		heading = box->heading;
		if (heading || box->id != NULL || box->href)
		{
			/* We have a box worthy of a callback. */
			char *text = NULL;
			pos.text = NULL;
			if (heading && box->down)
				pos.text = text = gather_text(ctx, box->down);
			pos.depth = depth;
			pos.heading = heading;
			pos.open_close = 1;
			pos.id = box->id;
			pos.href = box->href;
			pos.rect.x0 = box->s.layout.x;
			pos.rect.y0 = box->s.layout.y;
			pos.rect.x1 = box->s.layout.x + box->s.layout.w;
			pos.rect.y1 = box->s.layout.b;
			pos.rectangle_num = rect_num;
			fz_try(ctx)
				cb(ctx, arg, &pos);
			fz_always(ctx)
				fz_free(ctx, text);
			fz_catch(ctx)
				fz_rethrow(ctx);
			pos.text = NULL;
		}
	}

	for (child = box->down; child; child = child->next)
	{
		if (enumerate_box(ctx, child, page_top, page_bot, cb, arg, depth+1, rect_num, restart))
		{
			stopped = 1;
			break;
		}
	}

	if (box->style->visibility == V_VISIBLE && !skipping)
	{
		if (heading || box->id != NULL || box->href)
		{
			/* We have a box worthy of a callback that needs closing. */
			pos.open_close = 2;
			pos.rectangle_num = rect_num;
			cb(ctx, arg, &pos);
		}
	}

	return stopped;
}

static int enumerate_flow_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_story_position_callback *cb, void *arg, int depth, int rect_num, fz_html_restarter *restart)
{
	fz_html_flow *node;
	int restartable_ended = 0;

	/* FIXME: HB_DIRECTION_TTB? */

	if (restart && restart->start != NULL && restart->start != box)
		return 0;

	for (node = box->u.flow.head; node; node = node->next)
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

		if (node->box->id || node->box->href)
		{
			/* We have a node to callback for. */
			fz_story_element_position pos;

			pos.text = NULL;
			pos.depth = depth;
			pos.heading = 0;
			pos.open_close = 1 | 2;
			pos.id = node->box->id;
			pos.href = node->box->href;
			/* We only have the baseline and the em, so the bbox is a bit of a fudge. */
			pos.rect.x0 = node->x;
			pos.rect.y0 = node->y - node->h * 0.8f;
			pos.rect.x1 = node->x + node->w;
			pos.rect.y1 = node->y + node->h * 0.2f;
			pos.rectangle_num = rect_num;
			cb(ctx, arg, &pos);
		}

		if (node->type == FLOW_WORD || node->type == FLOW_SPACE || node->type == FLOW_SHYPHEN)
		{
		}
		else if (node->type == FLOW_IMAGE)
		{
			if (style->visibility == V_VISIBLE)
			{
				/* FIXME: Maybe callback for images? */
			}
		}
	}

	return restartable_ended;
}

static int enumerate_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_story_position_callback *cb, void *arg, int depth, int rect_num, fz_html_restarter *restart)
{
	switch (box->type)
	{
	case BOX_TABLE:
	case BOX_TABLE_ROW:
	case BOX_TABLE_CELL:
	case BOX_BLOCK:
		if (restart && restart->end == box)
			return 1;
		if (enumerate_block_box(ctx, box, page_top, page_bot, cb, arg, depth, rect_num, restart))
			return 1;
		break;
	case BOX_FLOW:
		if (enumerate_flow_box(ctx, box, page_top, page_bot, cb, arg, depth, rect_num, restart))
			return 1;
		break;
	}

	return 0;
}

void fz_story_positions(fz_context *ctx, fz_story *story, fz_story_position_callback *cb, void *arg)
{
	float page_top, page_bot;
	fz_html_box *b;
	fz_html_restarter restart;
	fz_html_box *box;
	fz_html_box *top;

	if (story == NULL || story->complete)
		return;

	b = story->tree.root;
	page_top = b->s.layout.y - b->u.block.margin[T] - b->u.block.border[T] - b->u.block.padding[T];
	page_bot = b->s.layout.b + b->u.block.margin[B] + b->u.block.border[B] + b->u.block.padding[B];
	top = story->tree.root->down;

	restart = story->restart_draw;

	for (box = top->down; box; box = box->next)
		if (enumerate_box(ctx, box, page_top, page_bot, cb, arg, 0, story->rect_count+1, &restart))
			break;
}
