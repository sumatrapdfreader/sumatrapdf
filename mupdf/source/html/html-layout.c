#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "html-imp.h"

#include "hb.h"
#include "hb-ft.h"
#include <ft2build.h>

#include <math.h>
#include <assert.h>

#undef DEBUG_HARFBUZZ

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
		justify = slop / i;
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
			int start = 0;
			int end;
			do
			{
				/* Skip until we find a level that's >= max_level */
				while (start < n && reorder[start]->bidi_level < max_level)
					start++;
				/* If start >= n-1 then no more runs. */
				if (start >= n-1)
					break;
				/* Find the end of the match */
				i = start+1;
				while (i < n && reorder[i]->bidi_level >= max_level)
					i++;
				/* Reverse from start to i-1 */
				end = i-1;
				while (start < end)
				{
					fz_html_flow *t = reorder[start];
					reorder[start++] = reorder[end];
					reorder[end--] = t;
				}
				start = i+1;
			}
			while (start < n);
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

static void flush_line(fz_context *ctx, fz_html_box *box, float page_h, float page_w, float line_w, int align, float indent, fz_html_flow *a, fz_html_flow *b)
{
	float avail, line_h, baseline;
	line_h = measure_line(a, b, &baseline);
	if (page_h > 0)
	{
		avail = page_h - fmodf(box->b, page_h);
		if (line_h > avail)
			box->b += avail;
	}
	layout_line(ctx, indent, page_w, line_w, align, a, b, box, baseline, line_h);
	box->b += line_h;
}

static void layout_flow_inline(fz_context *ctx, fz_html_box *box, fz_html_box *top)
{
	while (box)
	{
		box->y = top->y;
		box->em = fz_from_css_number(box->style->font_size, top->em, top->em, top->em);
		if (box->down)
			layout_flow_inline(ctx, box->down, box);
		box = box->next;
	}
}

static void layout_flow(fz_context *ctx, fz_html_box *box, fz_html_box *top, float page_h, hb_buffer_t *hb_buf)
{
	fz_html_flow *node, *line, *candidate;
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

	box->x = top->x;
	box->y = top->b;
	box->w = top->w;
	box->b = box->y;

	if (!box->flow_head)
		return;

	if (box->down)
		layout_flow_inline(ctx, box->down, box);

	for (node = box->flow_head; node; node = node->next)
	{
		node->breaks_line = 0; /* reset line breaks from previous layout */
		if (node->type == FLOW_IMAGE)
		{
			float margin_w = 0, margin_h = 0;
			float max_w, max_h;
			float xs = 1, ys = 1, s;

			find_accumulated_margins(ctx, box, &margin_w, &margin_h);
			max_w = top->w - margin_w;
			max_h = page_h - margin_h;

			/* NOTE: We ignore the image DPI here, since most images in EPUB files have bogus values. */
			node->w = node->content.image->w * 72 / 96;
			node->h = node->content.image->h * 72 / 96;

			node->w = fz_from_css_number(node->box->style->width, top->em, top->w - margin_w, node->w);
			node->h = fz_from_css_number(node->box->style->height, top->em, page_h - margin_h, node->h);

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
	line = node;
	line_w = indent;

	while (node)
	{
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
			flush_line(ctx, box, page_h, box->w, candidate_w, line_align, indent, line, candidate->next);

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
		flush_line(ctx, box, page_h, box->w, line_w, line_align, indent, line, NULL);
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
		float page_h, float vertical, hb_buffer_t *hb_buf);

static void layout_table(fz_context *ctx, fz_html_box *box, fz_html_box *top, float page_h, hb_buffer_t *hb_buf)
{
	fz_html_box *row, *cell, *child;
	int col, ncol = 0;

	box->em = fz_from_css_number(box->style->font_size, top->em, top->em, top->em);
	box->x = top->x;
	box->w = fz_from_css_number(box->style->width, box->em, top->w, top->w);
	box->y = box->b = top->b;

	for (row = box->down; row; row = row->next)
	{
		col = 0;
		for (cell = row->down; cell; cell = cell->next)
			++col;
		if (col > ncol)
			ncol = col;
	}

	for (row = box->down; row; row = row->next)
	{
		col = 0;

		row->em = fz_from_css_number(row->style->font_size, box->em, box->em, box->em);
		row->x = box->x;
		row->w = box->w;
		row->y = row->b = box->b;

		for (cell = row->down; cell; cell = cell->next)
		{
			float colw = row->w / ncol; // TODO: proper calculation

			cell->em = fz_from_css_number(cell->style->font_size, row->em, row->em, row->em);
			cell->y = cell->b = row->y;
			cell->x = row->x + col * colw;
			cell->w = colw;

			for (child = cell->down; child; child = child->next)
			{
				if (child->type == BOX_BLOCK)
					layout_block(ctx, child, cell->em, cell->x, &cell->b, cell->w, page_h, 0, hb_buf);
				else if (child->type == BOX_FLOW)
					layout_flow(ctx, child, cell, page_h, hb_buf);
				cell->b = child->b;
			}

			if (cell->b > row->b)
				row->b = cell->b;

			++col;
		}

		box->b = row->b;
	}
}

static float layout_block(fz_context *ctx, fz_html_box *box, float em, float top_x, float *top_b, float top_w,
		float page_h, float vertical, hb_buffer_t *hb_buf)
{
	fz_html_box *child;
	float auto_width;
	int first;

	const fz_css_style *style = box->style;
	float *margin = box->margin;
	float *border = box->border;
	float *padding = box->padding;

	assert(fz_html_box_has_boxes(box));
	em = box->em = fz_from_css_number(style->font_size, em, em, em);

	margin[0] = fz_from_css_number(style->margin[0], em, top_w, 0);
	margin[1] = fz_from_css_number(style->margin[1], em, top_w, 0);
	margin[2] = fz_from_css_number(style->margin[2], em, top_w, 0);
	margin[3] = fz_from_css_number(style->margin[3], em, top_w, 0);

	padding[0] = fz_from_css_number(style->padding[0], em, top_w, 0);
	padding[1] = fz_from_css_number(style->padding[1], em, top_w, 0);
	padding[2] = fz_from_css_number(style->padding[2], em, top_w, 0);
	padding[3] = fz_from_css_number(style->padding[3], em, top_w, 0);

	border[0] = style->border_style_0 ? fz_from_css_number(style->border_width[0], em, top_w, 0) : 0;
	border[1] = style->border_style_1 ? fz_from_css_number(style->border_width[1], em, top_w, 0) : 0;
	border[2] = style->border_style_2 ? fz_from_css_number(style->border_width[2], em, top_w, 0) : 0;
	border[3] = style->border_style_3 ? fz_from_css_number(style->border_width[3], em, top_w, 0) : 0;

	/* TODO: remove 'vertical' margin adjustments across automatic page breaks */

	if (layout_block_page_break(ctx, top_b, page_h, vertical, style->page_break_before))
		vertical = 0;

	box->x = top_x + margin[L] + border[L] + padding[L];
	auto_width = top_w - (margin[L] + margin[R] + border[L] + border[R] + padding[L] + padding[R]);
	box->w = fz_from_css_number(style->width, em, auto_width, auto_width);

	if (margin[T] > vertical)
		margin[T] -= vertical;
	else
		margin[T] = 0;

	if (padding[T] == 0 && border[T] == 0)
		vertical += margin[T];
	else
		vertical = 0;

	box->y = box->b = *top_b + margin[T] + border[T] + padding[T];

	first = 1;
	for (child = box->down; child; child = child->next)
	{
		if (child->type == BOX_BLOCK)
		{
			assert(fz_html_box_has_boxes(child));
			vertical = layout_block(ctx, child, em, box->x, &box->b, box->w, page_h, vertical, hb_buf);
			if (first)
			{
				/* move collapsed parent/child top margins to parent */
				margin[T] += child->margin[T];
				box->y += child->margin[T];
				child->margin[T] = 0;
				first = 0;
			}
			box->b = child->b + child->padding[B] + child->border[B] + child->margin[B];
		}
		else if (child->type == BOX_TABLE)
		{
			assert(fz_html_box_has_boxes(child));
			layout_table(ctx, child, box, page_h, hb_buf);
			first = 0;
			box->b = child->b + child->padding[B] + child->border[B] + child->margin[B];
		}
		else if (child->type == BOX_BREAK)
		{
			box->b += fz_from_css_number_scale(style->line_height, em);
			vertical = 0;
			first = 0;
		}
		else if (child->type == BOX_FLOW)
		{
			layout_flow(ctx, child, box, page_h, hb_buf);
			if (child->b > child->y)
			{
				box->b = child->b;
				vertical = 0;
				first = 0;
			}
		}
	}

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

	if (box->y == box->b)
	{
		if (margin[B] > vertical)
			margin[B] -= vertical;
		else
			margin[B] = 0;
	}
	else
	{
		box->b -= vertical;
		vertical = fz_max(margin[B], vertical);
		margin[B] = vertical;
	}

	return vertical;
}

void
fz_layout_html(fz_context *ctx, fz_html *html, float w, float h, float em)
{
	fz_html_box *box = html->root;
	hb_buffer_t *hb_buf = NULL;
	int unlocked = 0;

	fz_var(hb_buf);
	fz_var(unlocked);

	/* If we're already laid out to the specifications we need,
	 * nothing to do. */
	if (html->layout_w == w && html->layout_h == h && html->layout_em == em)
		return;

	html->page_margin[T] = fz_from_css_number(html->root->style->margin[T], em, em, 0);
	html->page_margin[B] = fz_from_css_number(html->root->style->margin[B], em, em, 0);
	html->page_margin[L] = fz_from_css_number(html->root->style->margin[L], em, em, 0);
	html->page_margin[R] = fz_from_css_number(html->root->style->margin[R], em, em, 0);

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

	fz_hb_lock(ctx);

	fz_try(ctx)
	{
		Memento_startLeaking(); /* HarfBuzz leaks harmlessly */
		hb_buf = hb_buffer_create();
		Memento_stopLeaking(); /* HarfBuzz leaks harmlessly */
		unlocked = 1;
		fz_hb_unlock(ctx);

		box->em = em;
		box->w = html->page_w;
		box->b = box->y;

		if (box->down)
		{
			switch (box->down->type)
			{
			case BOX_BLOCK:
				layout_block(ctx, box->down, box->em, box->x, &box->b, box->w, html->page_h, 0, hb_buf);
				break;
			case BOX_FLOW:
				layout_flow(ctx, box->down, box, html->page_h, hb_buf);
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

	if (h == 0)
		html->page_h = box->b;

	/* Remember how we're laid out so we can avoid needless
	 * relayouts in future. */
	html->layout_w = w;
	html->layout_h = h;
	html->layout_em = em;

#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_HTML")))
		fz_debug_html(ctx, html->root);
#endif
}

static void draw_flow_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf)
{
	fz_html_flow *node;
	fz_text *text;
	fz_matrix trm;
	float color[3];
	float prev_color[3];

	/* FIXME: HB_DIRECTION_TTB? */

	text = NULL;
	prev_color[0] = 0;
	prev_color[1] = 0;
	prev_color[2] = 0;

	for (node = box->flow_head; node; node = node->next)
	{
		const fz_css_style *style = node->box->style;

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

static void draw_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf);

static void draw_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf)
{
	switch (box->type)
	{
	case BOX_TABLE:
	case BOX_TABLE_ROW:
	case BOX_TABLE_CELL:
	case BOX_BLOCK:
		draw_block_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf);
		break;
	case BOX_FLOW:
		draw_flow_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf);
		break;
	}
}

static void draw_block_box(fz_context *ctx, fz_html_box *box, float page_top, float page_bot, fz_device *dev, fz_matrix ctm, hb_buffer_t *hb_buf)
{
	float x0, y0, x1, y1;

	float *border = box->border;
	float *padding = box->padding;

	assert(fz_html_box_has_boxes(box));
	x0 = box->x - padding[L];
	y0 = box->y - padding[T];
	x1 = box->x + box->w + padding[R];
	y1 = box->b + padding[B];

	if (y0 > page_bot || y1 < page_top)
		return;

	if (box->style->visibility == V_VISIBLE)
	{
		draw_rect(ctx, dev, ctm, page_top, box->style->background_color, x0, y0, x1, y1);

		if (border[T] > 0)
			draw_rect(ctx, dev, ctm, page_top, box->style->border_color[T], x0 - border[L], y0 - border[T], x1 + border[R], y0);
		if (border[B] > 0)
			draw_rect(ctx, dev, ctm, page_top, box->style->border_color[B], x0 - border[L], y1, x1 + border[R], y1 + border[B]);
		if (border[L] > 0)
			draw_rect(ctx, dev, ctm, page_top, box->style->border_color[L], x0 - border[L], y0 - border[T], x0, y1 + border[B]);
		if (border[R] > 0)
			draw_rect(ctx, dev, ctm, page_top, box->style->border_color[R], x1, y0 - border[T], x1 + border[R], y1 + border[B]);

		if (box->list_item)
			draw_list_mark(ctx, box, page_top, page_bot, dev, ctm, box->list_item);
	}

	for (box = box->down; box; box = box->next)
		draw_box(ctx, box, page_top, page_bot, dev, ctm, hb_buf);
}

void
fz_draw_html(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_html *html, int page)
{
	hb_buffer_t *hb_buf = NULL;
	fz_html_box *box;
	int unlocked = 0;
	float page_top = page * html->page_h;
	float page_bot = (page + 1) * html->page_h;

	fz_var(hb_buf);
	fz_var(unlocked);

	draw_rect(ctx, dev, ctm, 0, html->root->style->background_color,
			0, 0,
			html->page_w + html->page_margin[L] + html->page_margin[R],
			html->page_h + html->page_margin[T] + html->page_margin[B]);

	ctm = fz_pre_translate(ctm, html->page_margin[L], html->page_margin[T]);

	fz_hb_lock(ctx);
	fz_try(ctx)
	{
		hb_buf = hb_buffer_create();
		fz_hb_unlock(ctx);
		unlocked = 1;

		for (box = html->root->down; box; box = box->next)
			draw_box(ctx, html->root, page_top, page_bot, dev, ctm, hb_buf);
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
