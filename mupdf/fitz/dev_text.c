#include "fitz-internal.h"

#define LINE_DIST 0.9f
#define SPACE_DIST 0.2f
#define PARAGRAPH_DIST 0.5f

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

typedef struct fz_text_device_s fz_text_device;

struct fz_text_device_s
{
	fz_text_sheet *sheet;
	fz_text_page *page;
	fz_text_line cur_line;
	fz_text_span cur_span;
	fz_point point;
	int lastchar;
};

fz_text_sheet *
fz_new_text_sheet(fz_context *ctx)
{
	fz_text_sheet *sheet = fz_malloc(ctx, sizeof *sheet);
	sheet->maxid = 0;
	sheet->style = NULL;
	return sheet;
}

void
fz_free_text_sheet(fz_context *ctx, fz_text_sheet *sheet)
{
	fz_text_style *style = sheet->style;
	while (style)
	{
		fz_text_style *next = style->next;
		fz_drop_font(ctx, style->font);
		fz_free(ctx, style);
		style = next;
	}
	fz_free(ctx, sheet);
}

static fz_text_style *
fz_lookup_text_style_imp(fz_context *ctx, fz_text_sheet *sheet,
	float size, fz_font *font, int wmode, int script)
{
	fz_text_style *style;

	for (style = sheet->style; style; style = style->next)
	{
		if (style->font == font &&
			style->size == size &&
			style->wmode == wmode &&
			style->script == script) /* FIXME: others */
		{
			return style;
		}
	}

	/* Better make a new one and add it to our list */
	style = fz_malloc(ctx, sizeof *style);
	style->id = sheet->maxid++;
	style->font = fz_keep_font(ctx, font);
	style->size = size;
	style->wmode = wmode;
	style->script = script;
	style->next = sheet->style;
	sheet->style = style;
	return style;
}

static fz_text_style *
fz_lookup_text_style(fz_context *ctx, fz_text_sheet *sheet, fz_text *text, fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha, fz_stroke_state *stroke)
{
	float size = 1.0f;
	fz_font *font = text ? text->font : NULL;
	int wmode = text ? text->wmode : 0;
	if (ctm && text)
	{
		fz_matrix tm = text->trm;
		fz_matrix trm;
		tm.e = 0;
		tm.f = 0;
		trm = fz_concat(tm, *ctm);
		size = fz_matrix_expansion(trm);
	}
	return fz_lookup_text_style_imp(ctx, sheet, size, font, wmode, 0);
}

fz_text_page *
fz_new_text_page(fz_context *ctx, fz_rect mediabox)
{
	fz_text_page *page = fz_malloc(ctx, sizeof(*page));
	page->mediabox = mediabox;
	page->len = 0;
	page->cap = 0;
	page->blocks = NULL;
	return page;
}

void
fz_free_text_page(fz_context *ctx, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				fz_free(ctx, span->text);
			}
			fz_free(ctx, line->spans);
		}
		fz_free(ctx, block->lines);
	}
	fz_free(ctx, page->blocks);
	fz_free(ctx, page);
}

static void
append_char(fz_context *ctx, fz_text_span *span, int c, fz_rect bbox)
{
	if (span->len == span->cap)
	{
		span->cap = MAX(64, span->cap * 2);
		span->text = fz_resize_array(ctx, span->text, span->cap, sizeof(*span->text));
	}
	span->bbox = fz_union_rect(span->bbox, bbox);
	span->text[span->len].c = c;
	span->text[span->len].bbox = bbox;
	span->len++;
}

static void
init_span(fz_context *ctx, fz_text_span *span, fz_text_style *style)
{
	span->style = style;
	span->bbox = fz_empty_rect;
	span->len = span->cap = 0;
	span->text = NULL;
}

static void
append_span(fz_context *ctx, fz_text_line *line, fz_text_span *span)
{
	if (line->len == line->cap)
	{
		line->cap = MAX(8, line->cap * 2);
		line->spans = fz_resize_array(ctx, line->spans, line->cap, sizeof(*line->spans));
	}
	line->bbox = fz_union_rect(line->bbox, span->bbox);
	line->spans[line->len++] = *span;
}

static void
init_line(fz_context *ctx, fz_text_line *line)
{
	line->bbox = fz_empty_rect;
	line->len = line->cap = 0;
	line->spans = NULL;
}

static void
append_line(fz_context *ctx, fz_text_block *block, fz_text_line *line)
{
	if (block->len == block->cap)
	{
		block->cap = MAX(16, block->cap * 2);
		block->lines = fz_resize_array(ctx, block->lines, block->cap, sizeof *block->lines);
	}
	block->bbox = fz_union_rect(block->bbox, line->bbox);
	block->lines[block->len++] = *line;
}

static fz_text_block *
lookup_block_for_line(fz_context *ctx, fz_text_page *page, fz_text_line *line)
{
	float size = line->len > 0 && line->spans[0].len > 0 ? line->spans[0].style->size : 1;
	int i;

	/* SumatraPDF: TODO: tweak heuristic below */
	if (page->len > 0)
		return page->blocks;

	for (i = 0; i < page->len; i++)
	{
		fz_text_block *block = page->blocks + i;
		int w = block->bbox.x1 - block->bbox.x0;
		if (block->bbox.y0 - line->bbox.y1 < size * PARAGRAPH_DIST)
			if (line->bbox.x0 < block->bbox.x1 && line->bbox.x1 > block->bbox.x0)
				if (ABS(line->bbox.x0 - block->bbox.x0) < w / 4)
					return block;
	}

	if (page->len == page->cap)
	{
		page->cap = MAX(16, page->cap * 2);
		page->blocks = fz_resize_array(ctx, page->blocks, page->cap, sizeof(*page->blocks));
	}

	page->blocks[page->len].bbox = fz_empty_rect;
	page->blocks[page->len].len = 0;
	page->blocks[page->len].cap = 0;
	page->blocks[page->len].lines = NULL;

	return &page->blocks[page->len++];
}

static void
insert_line(fz_context *ctx, fz_text_page *page, fz_text_line *line)
{
	append_line(ctx, lookup_block_for_line(ctx, page, line), line);
}

static fz_rect
fz_split_bbox(fz_rect bbox, int i, int n)
{
	float w = (bbox.x1 - bbox.x0) / n;
	float x0 = bbox.x0;
	bbox.x0 = x0 + i * w;
	bbox.x1 = x0 + (i + 1) * w;
	return bbox;
}

static void
fz_flush_text_line(fz_context *ctx, fz_text_device *dev, fz_text_style *style)
{
	append_span(ctx, &dev->cur_line, &dev->cur_span);
	insert_line(ctx, dev->page, &dev->cur_line);
	init_span(ctx, &dev->cur_span, style);
	init_line(ctx, &dev->cur_line);
}

static void
fz_add_text_char_imp(fz_context *ctx, fz_text_device *dev, fz_text_style *style, int c, fz_rect bbox)
{
	if (!dev->cur_span.style)
		dev->cur_span.style = style;
	if (style != dev->cur_span.style)
	{
		append_span(ctx, &dev->cur_line, &dev->cur_span);
		init_span(ctx, &dev->cur_span, style);
	}
	append_char(ctx, &dev->cur_span, c, bbox);
}

static void
fz_add_text_char(fz_context *ctx, fz_text_device *dev, fz_text_style *style, int c, fz_rect bbox)
{
	switch (c)
	{
	case -1: /* ignore when one unicode character maps to multiple glyphs */
		break;
	case 0xFB00: /* ff */
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 1, 2));
		break;
	case 0xFB01: /* fi */
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(ctx, dev, style, 'i', fz_split_bbox(bbox, 1, 2));
		break;
	case 0xFB02: /* fl */
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(ctx, dev, style, 'l', fz_split_bbox(bbox, 1, 2));
		break;
	case 0xFB03: /* ffi */
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 0, 3));
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 1, 3));
		fz_add_text_char_imp(ctx, dev, style, 'i', fz_split_bbox(bbox, 2, 3));
		break;
	case 0xFB04: /* ffl */
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 0, 3));
		fz_add_text_char_imp(ctx, dev, style, 'f', fz_split_bbox(bbox, 1, 3));
		fz_add_text_char_imp(ctx, dev, style, 'l', fz_split_bbox(bbox, 2, 3));
		break;
	case 0xFB05: /* long st */
	case 0xFB06: /* st */
		fz_add_text_char_imp(ctx, dev, style, 's', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(ctx, dev, style, 't', fz_split_bbox(bbox, 1, 2));
		break;
	default:
		fz_add_text_char_imp(ctx, dev, style, c, bbox);
		break;
	}
}

/***** SumatraPDF: various string fixups *****/

static void
delete_character(fz_text_span *span, int i)
{
	memmove(&span->text[i], &span->text[i + 1], (span->len - (i + 1)) * sizeof(fz_text_char));
	span->len--;
}

static void
reverse_characters(fz_text_span *span, int i, int j)
{
	while (i < j)
	{
		fz_text_char tc = span->text[i];
		span->text[i] = span->text[j];
		span->text[j] = tc;
		i++; j--;
	}
}

static int
ornate_character(int ornate, int character)
{
	static wchar_t *ornates[] = {
		L" \xA8\xB4\x60\x5E\u02DA",
		L"a\xE4\xE1\xE0\xE2\xE5", L"A\xC4\xC1\xC0\xC2\0",
		L"e\xEB\xE9\xE8\xEA\0", L"E\xCB\xC9\xC8\xCA\0",
		L"i\xEF\xED\xEC\xEE\0", L"I\xCF\xCD\xCC\xCE\0",
		L"o\xF6\xF3\xF2\xF4\0", L"O\xD6\xD3\xD2\xD4\0",
		L"u\xFC\xFA\xF9\xFB\0", L"U\xDC\xDA\xD9\xDB\0",
		NULL
	};
	int i = 1, j = 1;
	while (ornates[0][i] && ornates[0][i] != (wchar_t)ornate)
		i++;
	while (ornates[j] && ornates[j][0] != (wchar_t)character)
		j++;
	if (!ornates[0][i] || !ornates[j])
		return 0;
	return ornates[j][i];
}

/* TODO: Complete these lists... */
#define ISLEFTTORIGHTCHAR(c) ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c) ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

static void
fixup_text_span(fz_text_span *span)
{
	int i;
	for (i = 0; i < span->len; i++)
	{
		switch (span->text[i].c)
		{
		/* recombine characters and their accents */
		case 0x00A8: /* diaeresis/umlaut */
		case 0x00B4: /* accute accent */
		case 0x0060: /* grave accent */
		case 0x005E: /* circumflex accent */
		case 0x02DA: /* ring above */
			if (i + 1 < span->len)
			{
				int newC = 0;
				if (span->text[i + 1].c != 32 || i + 2 == span->len)
					newC = ornate_character(span->text[i].c, span->text[i + 1].c);
				else if ((newC = ornate_character(span->text[i].c, span->text[i + 2].c)))
					delete_character(span, i + 1);
				if (newC)
				{
					delete_character(span, i);
					span->text[i].c = newC;
				}
			}
			break;
		default:
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=733 */
			/* reverse words written in RTL languages */
			if (ISRIGHTTOLEFTCHAR(span->text[i].c))
			{
				int j = i + 1;
				while (j < span->len && span->text[j - 1].bbox.x0 <= span->text[j].bbox.x0 && !ISLEFTTORIGHTCHAR(span->text[i].c))
					j++;
				reverse_characters(span, i, j - 1);
				i = j;
			}
		}
	}
}

static float
calc_bbox_overlap(fz_rect bbox1, fz_rect bbox2)
{
	fz_rect intersect = fz_intersect_rect(bbox1, bbox2);
	float area1, area2, area3;

	if (fz_is_empty_rect(intersect))
		return 0;

	area1 = (bbox1.x1 - bbox1.x0) * (bbox1.y1 - bbox1.y0);
	area2 = (bbox2.x1 - bbox2.x0) * (bbox2.y1 - bbox2.y0);
	area3 = (intersect.x1 - intersect.x0) * (intersect.y1 - intersect.y0);

	return area3 / MAX(area1, area2);
}

static int
is_same_c(fz_text_span *span, int i, fz_text_span *span2, int j)
{
	return i < span->len && j < span2->len && span->text[i].c == span2->text[j].c;
}

static int
do_glyphs_overlap(fz_text_span *span, int i, fz_text_span *span2, int j, int start)
{
	// if only a single glyph overlaps, require slightly more overlapping
	int single_glyph = start && !is_same_c(span, i + 1, span2, j + 1);
	return
		is_same_c(span, i, span2, j) &&
		(calc_bbox_overlap(span->text[i].bbox, span2->text[j].bbox) > (single_glyph ? 0.8f : 0.7f) ||
		 // bboxes of slim glyphs sometimes don't overlap enough, so
		 // check if the overlapping continues with the following two glyphs
		 is_same_c(span, i + 1, span2, j + 1) &&
		 (calc_bbox_overlap(span->text[i + 1].bbox, span2->text[j + 1].bbox) > 0.7f ||
		  is_same_c(span, i + 2, span2, j + 2) &&
		  calc_bbox_overlap(span->text[i + 2].bbox, span2->text[j + 2].bbox) > 0.7f));
}

static void
merge_lines(fz_context *ctx, fz_text_block *block, fz_text_line *line)
{
	int i;
	if (line == block->lines + block->len - 1)
		return;
	for (i = 0; i < (line + 1)->len; i++)
		append_span(ctx, line, &(line + 1)->spans[i]);
	fz_free(ctx, (line + 1)->spans);
	memmove(line + 1, line + 2, (block->lines + block->len - (line + 2)) * sizeof(fz_text_line));
	block->len--;
}

static void
fixup_text_block(fz_context *ctx, fz_text_block *block)
{
	fz_text_line *line;
	fz_text_span *span;
	int i;

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=734 */
	/* remove duplicate character sequences in (almost) the same spot */
	for (line = block->lines; line < block->lines + block->len; line++)
	{
		for (span = line->spans; span < line->spans + line->len; span++)
		{
			if (span->style && span->style->size < 5) /* do_glyphs_overlap fails too often for small fonts */
				continue;
			for (i = 0; i < span->len; i++)
			{
				fz_text_span *span2 = span;
				fz_text_line *line2 = line;
				int j = i + 1;
				for (; span2 <= line2->spans + line2->len; span2++, j = 0)
				{
					if (span2 == line2->spans + line2->len)
					{
						span2 = (++line2)->spans;
						if (line2 == block->lines + block->len || line2 - line == 2)
							break;
					}
					for (; j < span2->len; j++)
						if (span->text[i].c != 32 && do_glyphs_overlap(span, i, span2, j, 1))
							goto fixup_delete_duplicates;
				}
				continue;

fixup_delete_duplicates:
				do
					delete_character(span, i);
				while (do_glyphs_overlap(span, i, span2, ++j, 0));

				if (i < span->len && span->text[i].c == 32)
					delete_character(span, i);
				else if (i == span->len && span == line->spans + line->len - 1)
				{
					int len = line->len;
					merge_lines(ctx, block, line);
					span = line->spans + len - 1;
				}
			}
		}
	}
}

static void
fixup_text_page(fz_context *ctx, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
			for (span = line->spans; span < line->spans + line->len; span++)
				fixup_text_span(span);
		fixup_text_block(ctx, block);
	}
}

static int
fz_maps_into_rect(fz_matrix ctm, fz_rect rect)
{
	return rect.x0 <= ctm.e && ctm.e <= rect.x1 && rect.y0 <= ctm.f && ctm.f <= rect.y1;
}

/***** various string fixups *****/

static void
fz_text_extract(fz_context *ctx, fz_text_device *dev, fz_text *text, fz_matrix ctm, fz_text_style *style)
{
	fz_point *pen = &dev->point;
	fz_font *font = text->font;
	FT_Face face = font->ft_face;
	fz_matrix tm = text->trm;
	fz_matrix trm;
	float size;
	float adv;
	fz_rect rect;
	fz_point dir, ndir;
	fz_point delta, ndelta;
	float dist, dot;
	float ascender = 1;
	float descender = 0;
	int multi;
	int i, j, err;

	if (text->len == 0)
		return;

	if (font->ft_face)
	{
		fz_lock(ctx, FZ_LOCK_FREETYPE);
		err = FT_Set_Char_Size(font->ft_face, 64, 64, 72, 72);
		if (err)
			fz_warn(ctx, "freetype set character size: %s", ft_error_string(err));
		ascender = (float)face->ascender / face->units_per_EM;
		descender = (float)face->descender / face->units_per_EM;
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	else if (font->t3procs && !fz_is_empty_rect(font->bbox))
	{
		ascender = font->bbox.y1;
		descender = font->bbox.y0;
	}

	rect = fz_empty_rect;

	/* SumatraPDF: TODO: make this depend on the per-glyph displacement vector */
	if (text->wmode == 0)
	{
		dir.x = 1;
		dir.y = 0;
	}
	else
	{
		dir.x = 0;
		dir.y = 1;
	}

	tm.e = 0;
	tm.f = 0;
	trm = fz_concat(tm, ctm);

	dir = fz_transform_vector(trm, dir);
	dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
	ndir.x = dir.x / dist;
	ndir.y = dir.y / dist;

	size = fz_matrix_expansion(trm);

	for (i = 0; i < text->len; i++)
	{
		/* Calculate new pen location and delta */
		tm.e = text->items[i].x;
		tm.f = text->items[i].y;
		trm = fz_concat(tm, ctm);

		delta.x = pen->x - trm.e;
		delta.y = pen->y - trm.f;
		if (pen->x == -1 && pen->y == -1)
			delta.x = delta.y = 0;

		dist = sqrtf(delta.x * delta.x + delta.y * delta.y);

		/* Add space and newlines based on pen movement */
		if (dist > 0)
		{
			/* SumatraPDF: don't add spaces for large overlapping glyphs */
			fz_text_span *last = &dev->cur_span;
			if (last->len == 0 && dev->cur_line.len > 0)
				last = &dev->cur_line.spans[dev->cur_line.len - 1];

			ndelta.x = delta.x / dist;
			ndelta.y = delta.y / dist;
			dot = ndelta.x * ndir.x + ndelta.y * ndir.y;

			if (dist > size * LINE_DIST)
			{
				fz_flush_text_line(ctx, dev, style);
				dev->lastchar = ' ';
			}
			else if (fabsf(dot) > 0.95f && dist > size * SPACE_DIST && dev->lastchar != ' ' &&
			/* SumatraPDF: don't add spaces before spaces or for large overlapping glyphs */
				text->items[i].ucs != ' ' && !fz_maps_into_rect(trm, last->text[last->len - 1].bbox))
			{
				fz_rect spacerect;
				spacerect.x0 = -0.2f;
				spacerect.y0 = descender;
				spacerect.x1 = 0;
				spacerect.y1 = ascender;
				spacerect = fz_transform_rect(trm, spacerect);
				fz_add_text_char(ctx, dev, style, ' ', spacerect);
				dev->lastchar = ' ';
			}
		}

		/* Calculate bounding box and new pen position based on font metrics */
		if (font->ft_face)
		{
			FT_Fixed ftadv = 0;
			int mask = FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM;

			/* TODO: freetype returns broken vertical metrics */
			/* if (text->wmode) mask |= FT_LOAD_VERTICAL_LAYOUT; */

			fz_lock(ctx, FZ_LOCK_FREETYPE);
			err = FT_Set_Char_Size(font->ft_face, 64, 64, 72, 72);
			if (err)
				fz_warn(ctx, "freetype set character size: %s", ft_error_string(err));
			FT_Get_Advance(font->ft_face, text->items[i].gid, mask, &ftadv);
			adv = ftadv / 65536.0f;
			fz_unlock(ctx, FZ_LOCK_FREETYPE);

			rect.x0 = 0;
			rect.y0 = descender;
			rect.x1 = adv;
			rect.y1 = ascender;
		}
		else
		{
			adv = font->t3widths[text->items[i].gid];
			rect.x0 = 0;
			rect.y0 = descender;
			rect.x1 = adv;
			rect.y1 = ascender;
		}

		rect = fz_transform_rect(trm, rect);
		pen->x = trm.e + dir.x * adv;
		pen->y = trm.f + dir.y * adv;

		/* Check for one glyph to many char mapping */
		for (j = i + 1; j < text->len; j++)
			if (text->items[j].gid >= 0)
				break;
		multi = j - i;

		if (multi == 1)
		{
			fz_add_text_char(ctx, dev, style, text->items[i].ucs, rect);
		}
		else
		{
			for (j = 0; j < multi; j++)
			{
				fz_rect part = fz_split_bbox(rect, j, multi);
				fz_add_text_char(ctx, dev, style, text->items[i + j].ucs, part);
			}
			i += j - 1;
		}

		dev->lastchar = text->items[i].ucs;
	}
}

static void
fz_text_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, &ctm, colorspace, color, alpha, NULL);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, &ctm, colorspace, color, alpha, stroke);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, &ctm, NULL, NULL, 0, NULL);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, &ctm, NULL, NULL, 0, stroke);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, &ctm, NULL, NULL, 0, NULL);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static int cmp_block(const void *av, const void *bv)
{
	const fz_text_block *a = av;
	const fz_text_block *b = bv;
	int x = a->bbox.x0 - b->bbox.x0;
	if (x) return x;
	return -(a->bbox.y0 - b->bbox.y0);
}

static void
fz_text_free_user(fz_device *dev)
{
	fz_context *ctx = dev->ctx;
	fz_text_device *tdev = dev->user;

	append_span(ctx, &tdev->cur_line, &tdev->cur_span);
	insert_line(ctx, tdev->page, &tdev->cur_line);

	qsort(tdev->page->blocks, tdev->page->len, sizeof *tdev->page->blocks, cmp_block);

	/* TODO: unicode NFC normalization */
	/* TODO: bidi logical reordering */
	fixup_text_page(dev->ctx, tdev->page);

	fz_free(dev->ctx, tdev);
}

fz_device *
fz_new_text_device(fz_context *ctx, fz_text_sheet *sheet, fz_text_page *page)
{
	fz_device *dev;

	fz_text_device *tdev = fz_malloc_struct(ctx, fz_text_device);
	tdev->sheet = sheet;
	tdev->page = page;
	tdev->point.x = -1;
	tdev->point.y = -1;
	tdev->lastchar = ' ';

	init_line(ctx, &tdev->cur_line);
	init_span(ctx, &tdev->cur_span, NULL);

	dev = fz_new_device(ctx, tdev);
	dev->hints = FZ_IGNORE_IMAGE | FZ_IGNORE_SHADE;
	dev->free_user = fz_text_free_user;
	dev->fill_text = fz_text_fill_text;
	dev->stroke_text = fz_text_stroke_text;
	dev->clip_text = fz_text_clip_text;
	dev->clip_stroke_text = fz_text_clip_stroke_text;
	dev->ignore_text = fz_text_ignore_text;
	return dev;
}

/* XML, HTML and plain-text output */

static int font_is_bold(fz_font *font)
{
	FT_Face face = font->ft_face;
	if (face && (face->style_flags & FT_STYLE_FLAG_BOLD))
		return 1;
	if (strstr(font->name, "Bold"))
		return 1;
	return 0;
}

static int font_is_italic(fz_font *font)
{
	FT_Face face = font->ft_face;
	if (face && (face->style_flags & FT_STYLE_FLAG_ITALIC))
		return 1;
	if (strstr(font->name, "Italic") || strstr(font->name, "Oblique"))
		return 1;
	return 0;
}

static void
fz_print_style_begin(FILE *out, fz_text_style *style)
{
	int script = style->script;
	fprintf(out, "<span class=\"s%d\">", style->id);
	while (script-- > 0)
		fprintf(out, "<sup>");
	while (++script < 0)
		fprintf(out, "<sub>");
}

static void
fz_print_style_end(FILE *out, fz_text_style *style)
{
	int script = style->script;
	while (script-- > 0)
		fprintf(out, "</sup>");
	while (++script < 0)
		fprintf(out, "</sub>");
	fprintf(out, "</span>");
}

static void
fz_print_style(FILE *out, fz_text_style *style)
{
	char *s = strchr(style->font->name, '+');
	s = s ? s + 1 : style->font->name;
	fprintf(out, "span.s%d{font-family:\"%s\";font-size:%gpt;",
		style->id, s, style->size);
	if (font_is_italic(style->font))
		fprintf(out, "font-style:italic;");
	if (font_is_bold(style->font))
		fprintf(out, "font-weight:bold;");
	fprintf(out, "}\n");
}

void
fz_print_text_sheet(fz_context *ctx, FILE *out, fz_text_sheet *sheet)
{
	fz_text_style *style;
	for (style = sheet->style; style; style = style->next)
		fz_print_style(out, style);
}

void
fz_print_text_page_html(fz_context *ctx, FILE *out, fz_text_page *page)
{
	int block_n, line_n, span_n, ch_n;
	fz_text_style *style = NULL;
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;

	fprintf(out, "<div class=\"page\">\n");

	for (block_n = 0; block_n < page->len; block_n++)
	{
		block = &page->blocks[block_n];
		fprintf(out, "<div class=\"block\">\n");
		for (line_n = 0; line_n < block->len; line_n++)
		{
			line = &block->lines[line_n];
			fprintf(out, "<p>");
			style = NULL;

			for (span_n = 0; span_n < line->len; span_n++)
			{
				span = &line->spans[span_n];
				if (style != span->style)
				{
					if (style != NULL)
						fz_print_style_end(out, style);
					fz_print_style_begin(out, span->style);
					style = span->style;
				}

				for (ch_n = 0; ch_n < span->len; ch_n++)
				{
					fz_text_char *ch = &span->text[ch_n];
					if (ch->c == '<')
						fprintf(out, "&lt;");
					else if (ch->c == '>')
						fprintf(out, "&gt;");
					else if (ch->c == '&')
						fprintf(out, "&amp;");
					else if (ch->c >= 32 && ch->c <= 127)
						fprintf(out, "%c", ch->c);
					else
						fprintf(out, "&#x%x;", ch->c);
				}
			}
			if (style != NULL)
				fz_print_style_end(out, style);
			fprintf(out, "</p>\n");
		}
		fprintf(out, "</div>\n");
	}

	fprintf(out, "</div>\n");
}

void
fz_print_text_page_xml(fz_context *ctx, FILE *out, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	fz_text_char *ch;
	char *s;

	fprintf(out, "<page>\n");
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		fprintf(out, "<block bbox=\"%g %g %g %g\">\n",
			block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			fprintf(out, "<line bbox=\"%g %g %g %g\">\n",
				line->bbox.x0, line->bbox.y0, line->bbox.x1, line->bbox.y1);
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				fz_text_style *style = span->style;
				s = strchr(style->font->name, '+');
				s = s ? s + 1 : style->font->name;
				fprintf(out, "<span bbox=\"%g %g %g %g\" font=\"%s\" size=\"%g\">\n",
					span->bbox.x0, span->bbox.y0, span->bbox.x1, span->bbox.y1,
					s, style->size);
				for (ch = span->text; ch < span->text + span->len; ch++)
				{
					fprintf(out, "<char bbox=\"%g %g %g %g\" c=\"",
						ch->bbox.x0, ch->bbox.y0, ch->bbox.x1, ch->bbox.y1);
					switch (ch->c)
					{
					case '<': fprintf(out, "&lt;"); break;
					case '>': fprintf(out, "&gt;"); break;
					case '&': fprintf(out, "&amp;"); break;
					case '"': fprintf(out, "&quot;"); break;
					case '\'': fprintf(out, "&apos;"); break;
					default:
						if (ch->c >= 32 && ch->c <= 127)
							fprintf(out, "%c", ch->c);
						else
							fprintf(out, "&#x%x;", ch->c);
						break;
					}
					fprintf(out, "\"/>\n");
			}
				fprintf(out, "</span>\n");
			}
			fprintf(out, "</line>\n");
		}
		fprintf(out, "</block>\n");
	}
	fprintf(out, "</page>\n");
}

void
fz_print_text_page(fz_context *ctx, FILE *out, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	fz_text_char *ch;
	char utf[10];
	int i, n;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				for (ch = span->text; ch < span->text + span->len; ch++)
				{
					n = fz_runetochar(utf, ch->c);
					for (i = 0; i < n; i++)
						putc(utf[i], out);
				}
			}
			fprintf(out, "\n");
		}
		fprintf(out, "\n");
	}
}
