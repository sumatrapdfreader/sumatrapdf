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

#include <assert.h>

/* #define DEBUG_SPLITS */

/* #define DEBUG_PARA_SPLITS */

static void
recalc_bbox(fz_stext_block *block)
{
	fz_rect bbox = fz_empty_rect;
	fz_stext_line *line;

	for (line = block->u.t.first_line; line != NULL; line = line->next)
		bbox = fz_union_rect(bbox, line->bbox);

	block->bbox = bbox;
}

typedef enum
{
	UNDERLINE_UNKNOWN,
	UNDERLINE_YES,
	UNDERLINE_NO,
	UNDERLINE_MIXED
} underline_state;

/* Some crap heuristics to spot a bold font. */
static int
font_is_bold(fz_font *font)
{
	const char *c;

	if (font == NULL)
		return 0;
	if (font->flags.is_bold)
		return 1;

	if (fz_strstrcase(font->name, "Bold") != NULL)
		return 1;
	if (fz_strstrcase(font->name, "Black") != NULL)
		return 1;
	if (fz_strstrcase(font->name, "Medium") != NULL)
		return 0;
	if (fz_strstrcase(font->name, "Light") != NULL)
		return 0;

	c = fz_strstr(font->name, " B");
	if (c && (c[2] == ' ' || c[2] == 0))
		return 1;

	return 0;
}

/* Check to see if lines move left to right and downwards. */
/* FIXME: Maybe allow right to left? checking unicode values? */
static int
lines_move_plausibly_like_paragraph(fz_stext_block *block)
{
	fz_stext_line *line;
	int firstline = 1;
	float line_height, line_x, line_y;

	/* Do the lines that make up this block move in an appropriate way? */
	for (line = block->u.t.first_line; line != NULL; line = line->next)
	{
		float x = (line->bbox.x0 + line->bbox.x1)/2;
		float y = (line->bbox.y0 + line->bbox.y1)/2;
		float height = line->bbox.y1 - line->bbox.y0;
		fz_stext_char *ch;

		/* Ignore any completely empty lines */
		for (ch = line->first_char; ch != NULL; ch = ch->next)
			if (ch->c != ' ')
				break;
		if (ch == NULL)
			continue;

		if (firstline)
		{
			line_height = height;
			line_x = x;
			line_y = y;
			firstline = 0;
		}
		else if (line_y - line_height/2 < y && line_y + line_height/2 > y)
		{
			/* We are plausibly the same line. Only accept if we move right. */
			if (x < line_x)
				return 0;
			else
				line_x = x;
		}
		else if (line_y < y)
		{
			/* Moving downwards. Plausible. */
			line_y = y;
			line_height = height;
			line_x = x;
		}
		else
		{
			/* Nothing else is plausible. */
			return 0;
		}
	}
	return 1;
}

#ifdef DEBUG_SPLITS
static void dump_line(fz_context *ctx, const char *str, fz_stext_line *line)
{
	fz_stext_char *ch;

	if (str)
		fz_write_printf(ctx, fz_stddbg(ctx), "%s\n", str);

	if (line == NULL)
		return;

	for (ch = line->first_char; ch != NULL; ch = ch->next)
		fz_write_printf(ctx, fz_stddbg(ctx), "%c", (char)ch->c);
	fz_write_printf(ctx, fz_stddbg(ctx), "\n");
}

static void dump_block(fz_context *ctx, const char *fmt, fz_stext_block *block)
{
	fz_stext_line *line;

	fz_write_printf(ctx, fz_stddbg(ctx), "%s\n", fmt);
	if (block == NULL || block->type != FZ_STEXT_BLOCK_TEXT)
		return;

	for (line = block->u.t.first_line; line != NULL; line = line->next)
		dump_line(ctx, NULL, line);
}
#endif

typedef struct
{
	fz_pool *pool;
	fz_stext_struct *parent;
	int idx;
	fz_stext_block **pfirst;
	fz_stext_block **plast;
} stext_pos;

static fz_stext_block *split_block_at_line(fz_context *ctx, stext_pos *pos, fz_stext_block *block, fz_stext_line *line)
{
	fz_stext_block *newblock = fz_pool_alloc(ctx, pos->pool, sizeof *newblock);

#ifdef DEBUG_SPLITS
	dump_block(ctx, "Splitting:", block);
	dump_line(ctx, "At line:", line);
#endif

	newblock->bbox = fz_empty_rect;
	newblock->prev = block;
	newblock->next = block->next;
	if (block->next)
		block->next->prev = newblock;
	else
	{
		assert(*pos->plast == block);
		*pos->plast = newblock;
	}
	block->next = newblock;
	newblock->type = FZ_STEXT_BLOCK_TEXT;
	newblock->id = block->id;
	newblock->u.t.flags = block->u.t.flags;
	newblock->u.t.first_line = line;
	newblock->u.t.last_line = block->u.t.last_line;
	block->u.t.last_line = line->prev;
	line->prev->next = NULL;
	line->prev = NULL;
	recalc_bbox(block);
	recalc_bbox(newblock);

#ifdef DEBUG_SPLITS
	dump_block(ctx, "Giving:", block);
	dump_block(ctx, "and:", newblock);
#endif

	return newblock;
}

/* Convert a block to being a struct that contains just that block. */
static void block_to_struct(fz_context *ctx, stext_pos *pos, fz_stext_block *block, int structtype)
{
	fz_stext_struct *str = fz_pool_alloc_flexible(ctx, pos->pool, fz_stext_struct, raw, 1);
	fz_stext_block *new_block = fz_pool_alloc(ctx, pos->pool, sizeof(*new_block));

	str->up = block;
	str->parent = pos->parent;
	str->first_block = new_block;
	str->last_block = new_block;
	str->standard = structtype;
	str->raw[0] = 0;

	new_block->type = block->type;
	new_block->bbox = block->bbox;
	new_block->u = block->u;

	block->type = FZ_STEXT_BLOCK_STRUCT;
	block->u.s.down = str;
	block->u.s.index = pos->idx++;
}

/*
	We are going to repeatedly walk the lines that make up a block.
	To reduce the boilerplate here, we'll use a line_walker function.
	This will call a bunch of callbacks as it goes.

	newline_fn	Called whenever we move to a new horizontal line (i.e.
			as if we've got a newline). This is not the same as being
			called every fz_stext_line, as we frequently get multiple
			fz_stext_line's on a single horizontal line. If this returns
			0, execution continues. Return 1 to stop the walking.
	line_fn		Called for every fz_stext_line (typically used to process
			characters).
	end_fn		Called at the end of the block (with line being the final
			line of the block.
	arg		An opaque pointer passed to all the callbacks.
*/
typedef int (line_walker_newline_fn)(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height);
typedef int (line_walker_fn)(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg);
typedef void (line_walker_end_fn)(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg);

static void
line_walker(fz_context *ctx, fz_stext_block *block, line_walker_newline_fn *newline_fn, line_walker_fn *line_fn, line_walker_end_fn *end_fn, void *arg)
{
	int firstline = 1;
	fz_stext_line *line;
	float line_height, line_y;

	if (block->u.t.first_line == NULL)
		return;

	for (line = block->u.t.first_line; line != NULL; line = line->next)
	{
		float y = (line->bbox.y0 + line->bbox.y1)/2;
		float height = line->bbox.y1 - line->bbox.y0;

		if (line->first_char == NULL)
			continue; /* Should never happen, but makes life easier to assume this later. */

		if (firstline)
		{
			line_height = height;
			firstline = 0;
			line_y = y;
		}
		else if (line_y - line_height/2 < y && line_y + line_height/2 > y)
		{
			/* We are plausibly the same horizontal line. */
		}
		else if (line_y < y)
		{
			/* Moving downwards. */
			line_height = height;
			line_y = y;
			if (newline_fn && newline_fn(ctx, block, line, arg, line_height))
				return;
		}
		if (line_fn && line_fn(ctx, block, line, arg))
			return;
	}
	if (end_fn)
		end_fn(ctx, block, block->u.t.last_line, arg);
}

/* We scan through the block, collecting lines up that look
 * "title-ish" (by which here, we mean "are completely
 * underlined"). As soon as we finish such a region, we split
 * the block (either before or after it as appropriate), and
 * mark it as a title.
 *
 * e.g.
 *
 * _THIS_IS_LIKELY_A
 * _TITLE_			___ < BREAK HERE
 * Lorem ipsum dolor sit
 * amet, consectetur
 * adipiscing elit.		___ < BREAK HERE
 * _LIKELY_ANOTHER_TITLE_	____< BREAK HERE
 * Sed do eiusmod tempor
 * incididunt ut labore
 * et dolore magna aliqua.
 */
typedef struct
{
	stext_pos *pos;
	fz_stext_line *title_start;
	fz_stext_line *title_end;
	underline_state underlined;
	int changed;
} underlined_data;

static int
underlined_break(fz_context *ctx, fz_stext_block *block, underlined_data *data)
{
	fz_stext_line *line;

	/* We have a block that looks like a title. */
	if (data->title_start != block->u.t.first_line)
	{
		/* We need to split the block before title_start */
		line = data->title_start;
	}
	else if (data->title_end != block->u.t.last_line)
	{
		/* We need to split the block after title_end */
		line = data->title_end->next;
	}
	else
	{
		/* This block is already entirely title. */
		line = NULL;
	}
	if (line)
	{
		(void)split_block_at_line(ctx, data->pos, block, line);
		data->changed = 1;
		if (line == data->title_start)
		{
			/* Don't label the latter part as a title yet, we'll do it when
			 * we step back in, but we don't know how much of the latter
			 * block is title yet. */
		}
		else
		{
			block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_H);
		}
	}
	else
	{
		block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_H);
	}
	return 1;
}

static int
underlined_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	underlined_data *data = (underlined_data *)arg;

	if (data->underlined == UNDERLINE_YES)
	{
		/* Add the line we've just finished to the start/stop region */
		if (data->title_start == NULL)
			data->title_start = line->prev;
		data->title_end = line->prev;
	}
	else if (data->title_start != NULL)
	{
		/* We've reached the end of a title region. */
		return underlined_break(ctx, block, data);
	}
	data->underlined = UNDERLINE_UNKNOWN;

	return 0;
}

static int
underlined_line(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	underlined_data *data = (underlined_data *)arg;
	fz_stext_char *ch;

	/* If we already know that this line is mixed underlined, then no point in
	 * wasting time. */
	if (data->underlined == UNDERLINE_MIXED)
		return 0;

	/* If we haven't started looking yet, prime the value. */
	if (data->underlined == UNDERLINE_UNKNOWN)
		data->underlined = (line->first_char->flags & FZ_STEXT_UNDERLINE) ? UNDERLINE_YES : UNDERLINE_NO;

	/* Check that all the rest of the the chars match our expected value. */
	for (ch = line->first_char; ch != NULL; ch = ch->next)
		if ((!!(ch->flags & FZ_STEXT_UNDERLINE)) ^ (data->underlined == UNDERLINE_YES))
		{
			/* Differs! So, Mixed. */
			data->underlined = UNDERLINE_MIXED;
			break;
		}

	return 0;
}

static void
underlined_end(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	underlined_data *data = (underlined_data *)arg;

	if (data->underlined == UNDERLINE_YES)
	{
		/* Add the line we've just finished to the start/stop region */
		if (data->title_start == NULL)
			data->title_start = block->u.t.last_line;
		data->title_end = block->u.t.last_line;
	}

	/* If we didn't find a region, bale. */
	if (data->title_start)
		underlined_break(ctx, block, data);
}

static int
detect_underlined_titles(fz_context *ctx, stext_pos *pos, fz_stext_block *block)
{
	/* Let's do the title scanning, where our criteria is
	 * "the entire line is underlined". */
	underlined_data data[1];

	data->pos = pos;
	data->title_start = NULL;
	data->title_end = NULL;
	data->underlined = UNDERLINE_UNKNOWN;
	data->changed = 0;

	line_walker(ctx, block, underlined_newline, underlined_line, underlined_end, data);

	return data->changed;
}


/* Now we scan again, where the 'title' criteria is based upon
 * the titles being entirely in a different font. */
typedef struct
{
	stext_pos *pos;
	fz_stext_line *title_start;
	fz_stext_line *title_end;
	fz_font *font;
	int changed;
} font_data;

#define MIXED_FONT ((fz_font *)1)

static int
font_break(fz_context *ctx, fz_stext_block *block, font_data *data)
{
	fz_stext_line *line;

	/* We have a block that looks like a title. */
	if (data->title_start != block->u.t.first_line)
	{
		/* We need to split the block before title_start */
		line = data->title_start;
	}
	else if (data->title_end != block->u.t.last_line)
	{
		/* We need to split the block after title_end */
		line = data->title_end->next;
	}
	else
	{
		/* This block is already entirely title. */
		line = NULL;
	}
	if (line)
	{
		(void)split_block_at_line(ctx, data->pos, block, line);
		data->changed = 1;
		if (line == data->title_start)
		{
			/* Don't label the latter part as a title yet, we'll do it when
			 * we step back in, but we don't know how much of the latter
			 * block is title yet. */
		}
		else
		{
			block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_H);
		}
	}
	else
	{
		block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_H);
	}

	return 1;
}

static int
font_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	font_data *data = (font_data *)arg;

	if (data->font != NULL && data->font != MIXED_FONT && font_is_bold(data->font))
	{
		/* Add the line we've just finished to the start/stop region */
		if (data->title_start == NULL)
			data->title_start = line->prev;
		data->title_end = line->prev;
	}
	else if (data->title_start != NULL)
	{
		/* We've reached the end of a title region. */
		return font_break(ctx, block, data);
	}
	data->font = NULL;

	return 0;
}

static int
font_line(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	font_data *data = (font_data *)arg;
	fz_stext_char *ch;

	/* If we already know that this line is mixed fonts, then no point in
	 * wasting time. */
	if (data->font == MIXED_FONT)
		return 0;

	/* If we are just starting, prime it. */
	if (data->font == NULL)
		data->font = line->first_char->font;

	for (ch = line->first_char; ch != NULL; ch = ch->next)
		if (ch->font != data->font)
		{
			data->font = MIXED_FONT;
			break;
		}

	return 0;
}

static void
font_end(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	font_data *data = (font_data *)arg;

	if (data->font != NULL && data->font != MIXED_FONT && font_is_bold(data->font))
	{
		/* Add the line we've just finished to the start/stop region */
		if (data->title_start == NULL)
			data->title_start = block->u.t.last_line;
		data->title_end = block->u.t.last_line;
	}

	if (data->title_start)
		font_break(ctx, block, data);
}

static int
detect_titles_by_font_usage(fz_context *ctx, stext_pos *pos, fz_stext_block *block)
{
	font_data data[1];

	data->pos = pos;
	data->title_start = NULL;
	data->title_end = NULL;
	data->font = NULL;
	data->changed = 0;

	line_walker(ctx, block, font_newline, font_line, font_end, data);

	return data->changed;
}

typedef struct
{
	fz_rect bbox;
	stext_pos *pos;
	int changed;
} indent_data;

static int
indent_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	indent_data *data = (indent_data *)arg;
	float indent = line->bbox.x0 - data->bbox.x0;

	if (indent > line_height)
	{
		/* Break the block here! */
		(void)split_block_at_line(ctx, data->pos, block, line);
		data->changed = 1;
		return 1;
	}

	return 0;
}

static int
break_paragraphs_by_indent(fz_context *ctx, stext_pos *pos, fz_stext_block *block, fz_rect bbox)
{
	indent_data data[1];

	data->pos = pos;
	data->bbox = bbox;
	data->changed = 0;

	line_walker(ctx, block, indent_newline, NULL, NULL, data);

	return data->changed;
}

typedef struct
{
	fz_rect bbox;
	stext_pos *pos;
	float line_gap;
	float prev_line_gap;
	int looking_for_space;
	float space_size;
	int maybe_ends_paragraph;
	int changed;
} trailing_data;

static int
trailing_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	trailing_data *data = (trailing_data *)arg;

	data->prev_line_gap = data->line_gap;

	if (data->looking_for_space)
	{
		/* We've moved downwards onto a line, and failed to find
		 * a space on that line. Presumably that means that whole
		 * line is a single word. */
		float line_len = line->bbox.x1 - line->bbox.x0;

		if (line_len + data->space_size < data->prev_line_gap)
		{
			/* We could have fitted this word into the previous line. */
			/* So presumably that was a paragraph break. Split here. */
			(void)split_block_at_line(ctx, data->pos, block, line);
			data->changed = 1;
			return 1;
		}
		data->looking_for_space = 0;
	}

	/* If we the last line we looked at ended plausibly for a paragraph,
	 * then look for a space in this line... */
	data->looking_for_space = data->maybe_ends_paragraph;

	return 0;
}

static int
trailing_line(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	trailing_data *data = (trailing_data *)arg;
	fz_stext_char *ch;

	data->line_gap = data->bbox.x1 - line->bbox.x1;
	if (line->last_char && (
		(line->last_char->c >= 'A' && line->last_char->c <= 'Z') ||
		(line->last_char->c >= 'a' && line->last_char->c <= 'z') ||
		(line->last_char->c >= '0' && line->last_char->c <= '9')))
	{
		/* In Latin text, paragraphs should always end up some form
		 * of punctuation. I suspect that's less true of some other
		 * languages (particularly far-eastern ones). Let's just say
		 * that if we end in A-Za-z0-9 we can't possibly be the last
		 * line of a paragraph. */
		data->maybe_ends_paragraph = 0;
	}
	else
	{
		/* Plausibly the next line might be the first line of a new paragraph */
		data->maybe_ends_paragraph = 1;
	}
	for (ch = line->first_char; ch != NULL; ch = ch->next)
	{
		fz_rect r;
		float w, line_len;

		if (ch->c != ' ')
			continue;

		r = fz_rect_from_quad(ch->quad);
		w = r.x1 - r.x0;

		if (w < data->space_size)
			data->space_size = w;

		/* If we aren't looking_for_space, then no point in checking for
		 * whether the prefix will fit. But keep looping as we want to
		 * continue to refine our idea of how big a space is. */
		if (!data->looking_for_space)
			continue;

		line_len = r.x0 - line->bbox.x0;
		if (line_len + data->space_size < data->prev_line_gap)
		{
			/* We could have fitted this word into the previous line. */
			/* So presumably that was a paragraph break. Split here. */
			(void)split_block_at_line(ctx, data->pos, block, line);
			data->changed = 1;
			return 1;
		}
		data->looking_for_space = 0;
	}

	return 0;
}

static int
break_paragraphs_by_analysing_trailing_gaps(fz_context *ctx, stext_pos *pos, fz_stext_block *block, fz_rect bbox)
{
	trailing_data data[1];

	data->bbox = bbox;
	data->pos = pos;
	data->line_gap = 0;
	data->prev_line_gap = 0;
	data->looking_for_space = 0;
	data->space_size = 99999;
	data->maybe_ends_paragraph = 0;
	data->changed = 0;

	line_walker(ctx, block, trailing_newline, trailing_line, NULL, data);

	return data->changed;
}

typedef struct
{
	fz_rect bbox;
	stext_pos *pos;
	int count_lines;
	int count_justified;
	int non_digits_exist_in_this_line;
	fz_rect fragment_box;
	fz_rect line_box;
	int gap_count_this_line;
	float gap_size_this_line;
	int bad_gap;
	float xmin, xmax;
	float last_min_space;
	int changed;
} justify_data;

#define JUSTIFY_THRESHOLD 1

static int
justify_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	justify_data *data = (justify_data *)arg;

	if (line->prev)
		line = line->prev;

	data->line_box = fz_union_rect(data->line_box, data->fragment_box);
	if (data->line_box.x0 < data->bbox.x0 + JUSTIFY_THRESHOLD && data->line_box.x1 > data->bbox.x1 - JUSTIFY_THRESHOLD && data->gap_count_this_line && data->non_digits_exist_in_this_line)
		data->count_justified++;
	data->non_digits_exist_in_this_line = 0;
	data->count_lines++;
	data->gap_size_this_line = 0;
	data->gap_count_this_line = 0;
	data->fragment_box = fz_empty_rect;
	data->line_box = fz_empty_rect;

	data->xmin = INFINITY;
	data->xmax = -INFINITY;

	return 0;
}

static void
fragment_end(justify_data *data)
{
	float gap;

	if (fz_is_empty_rect(data->fragment_box))
	{
		/* No fragment. Nothing to do. */
		return;
	}
	if (fz_is_empty_rect(data->line_box))
	{
		/* First fragment of the line; no gap yet. */
		gap = 0;
	}
	else if (data->fragment_box.x0 > data->line_box.x1)
	{
		/* This whole fragment is to the right of the line so far. */
		gap = data->fragment_box.x0 - data->line_box.x1;
	}
	else if (data->fragment_box.x1 < data->line_box.x0)
	{
		/* This whole fragment is the left of the line so far. */
		gap = data->line_box.x1 - data->fragment_box.x0;
	}
	else
	{
		/* Abutting or overlapping fragment. Ignore it. */
		gap = 0;
	}
	data->line_box = fz_union_rect(data->line_box, data->fragment_box);
	data->fragment_box = fz_empty_rect;
	if (gap < data->last_min_space)
		return;
	/* So we have a gap to consider */
	if (data->gap_count_this_line > 0)
	{
		/* Allow for double spaces, cos some layouts put
		 * double spaces before full stops. */
		if (fabs(gap - data->gap_size_this_line) > 1 &&
			fabs(gap/2.0 - data->gap_size_this_line) < 1)
			gap /= 2;
		if (fabs(gap - data->gap_size_this_line) > 1)
			data->bad_gap = 1;
	}
	data->gap_size_this_line = (data->gap_size_this_line * data->gap_count_this_line + gap) / (data->gap_count_this_line + 1);
	data->gap_count_this_line++;
}

/* This is trickier than you'd imagine. We want to walk the line, looking
 * for how large the spaces are. In a justified line, all the spaces should
 * be pretty much the same size. (Except maybe before periods). But we want
 * to cope with bidirectional text which can send glyphs in unexpected orders.
 * e.g.   abc fed ghi
 * So we have to walk over "fragments" at a time.
 */
static int
justify_line(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	justify_data *data = (justify_data *)arg;
	fz_stext_char *ch;

	for (ch = line->first_char; ch != NULL; ch = ch->next)
	{
		fz_rect r = fz_rect_from_quad(ch->quad);
		float min_space = ch->size * 0.15f; /* Matches SPACE_DIST from stext-device. */

		if (ch->c == ' ')
		{
			/* This ends a fragment, but we don't treat it as such.
			 * Just continue, because we'll end the fragment next time
			 * around the loop (this copes with trailing spaces, and
			 * multiple spaces, and gaps between 'lines' that are on
			 * the same line. */
			data->last_min_space = min_space;
			continue;
		}
		if ((ch->c <= '0' || ch->c >= '9') && ch->c != '.')
			data->non_digits_exist_in_this_line = 1;
		if (!fz_is_empty_rect(data->fragment_box))
		{
			if (r.x0 > data->fragment_box.x1 + data->last_min_space)
			{
				/* Fragment ends due to gap on right. */
				fragment_end(data);
			}
			else if (r.x1 < data->fragment_box.x0 - data->last_min_space)
			{
				/* Fragment ends due to gap on left. */
				fragment_end(data);
			}
		}
		/* Extend the fragment */
		data->fragment_box = fz_union_rect(data->fragment_box, r);
		data->last_min_space = min_space;
	}

	return 0;
}

static void
justify_end(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	justify_data *data = (justify_data *)arg;

	fragment_end(data);
	data->line_box = fz_union_rect(data->line_box, data->fragment_box);
	if (data->line_box.x0 < data->bbox.x0 + JUSTIFY_THRESHOLD && data->line_box.x1 > data->bbox.x1 - JUSTIFY_THRESHOLD && data->gap_count_this_line && data->non_digits_exist_in_this_line)
		data->count_justified++;
	data->count_lines++;
}

static int
justify2_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	justify_data *data = (justify_data *)arg;

	if (data->line_box.x0 < data->bbox.x0 + JUSTIFY_THRESHOLD && data->line_box.x1 > data->bbox.x1 - JUSTIFY_THRESHOLD)
	{
		/* Justified */
	}
	else
	{
		/* Break after line */
		(void)split_block_at_line(ctx, data->pos, block, line);
		data->changed = 1;
		return 1;
	}

	data->line_box = fz_empty_rect;

	return 0;
}

static int
justify2_line(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	justify_data *data = (justify_data *)arg;
	fz_stext_char *ch;

	for (ch = line->first_char; ch != NULL; ch = ch->next)
	{
		if (ch->c == ' ')
			continue;

		data->line_box = fz_union_rect(data->line_box, fz_rect_from_quad(ch->quad));
	}

	return 0;
}

static fz_rect
text_block_marked_bbox(fz_context *ctx, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;
	fz_rect r = fz_empty_rect;

	for (line = block->u.t.first_line; line != NULL; line = line->next)
	{
		for (ch = line->first_char; ch != NULL; ch = ch->next)
		{
			if (ch->c == ' ')
				continue;
			r = fz_union_rect(r, fz_rect_from_quad(ch->quad));
		}
	}

	return r;
}

static int
break_paragraphs_within_justified_text(fz_context *ctx, stext_pos *pos, fz_stext_block *block, fz_rect bbox)
{
	justify_data data[1];

	if (block->u.t.flags != FZ_STEXT_TEXT_JUSTIFY_UNKNOWN)
		return 0;

	data->bbox = bbox;

	data->pos = pos;
	data->count_lines = 0;
	data->count_justified = 0;
	data->non_digits_exist_in_this_line = 0;
	data->bad_gap = 0;
	data->gap_size_this_line = 0;
	data->gap_count_this_line = 0;
	data->fragment_box = fz_empty_rect;
	data->line_box = fz_empty_rect;
	data->xmin = INFINITY;
	data->xmax = -INFINITY;
	data->changed = 0;

	line_walker(ctx, block, justify_newline, justify_line, justify_end, data);

	/* We can't really derive anything about single lines! */
	if (data->count_lines < 2)
		return 0;
	/* If at least half of the lines don't appear to be justified, then
	 * don't trust 'em. */
	if (data->count_justified * 2 < data->count_lines)
		return 0;
	/* If the "badness" we've seen to do with big gaps (i.e. how much
	 * bigger the gaps are than we'd reasonably expect) is too large
	 * then we can't be a justified block. We are prepared to forgive
	 * larger sizes in larger paragraphs. */
	if (data->bad_gap)
		return 0;
	block->u.t.flags = FZ_STEXT_TEXT_JUSTIFY_FULL;

	line_walker(ctx, block, justify2_newline, justify2_line, NULL, data);

	return data->changed;
}

typedef enum
{
	LOOKING_FOR_BULLET = 0,
	LOOKING_FOR_POST_BULLET = 1,
	LOOKING_FOR_POST_NUMERICAL_BULLET = 2,
	FOUND_BULLET = 3,
	CONTINUATION_LINE = 4,
	NO_BULLET = 5
} list_state;

typedef struct
{
	stext_pos *pos;
	list_state state;
	int buffer[10];
	int buffer_fill;
	float bullet_r;
	float post_bullet_indent;
	float l;
	fz_stext_line *bullet_line_start;
	fz_stext_line *this_line_start;
	int changed;
} list_data;

static int
list_newline(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg, float line_height)
{
	list_data *data = (list_data *)arg;

	if (data->state == FOUND_BULLET)
	{
		if (block->u.t.first_line != data->bullet_line_start && data->state == FOUND_BULLET)
		{
			/* We need to split the block before the bullet started. */
			(void)split_block_at_line(ctx, data->pos, block, data->bullet_line_start);
			data->changed = 1;
			return 1;
		}
		if (data->bullet_line_start != data->this_line_start)
		{
			/* We've found a second bullet. Break before the previous line. */
			(void)split_block_at_line(ctx, data->pos, block, data->this_line_start);
			block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_LISTITEM);
			data->changed = 1;
			return 1;
		}
	}
	else if (data->state == NO_BULLET && data->bullet_line_start)
	{
		/* We've found a bullet before, and the line we've just completed
		 * is neither a new bullet line, or a continuation so, we need to
		 * break that into a new block. */
		(void)split_block_at_line(ctx, data->pos, block, data->this_line_start);
		block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_LISTITEM);
		data->changed = 1;
		return 1;
	}

	data->this_line_start = line;
	data->state = LOOKING_FOR_BULLET;
	data->buffer_fill = 0;
	data->l = block->bbox.x1;
	data->bullet_r = block->bbox.x0;

	return 0;
}

static int
approx_eq(float a, float b, float c)
{
	return fabs(a - b) <= c;
}

static int
is_roman(int c)
{
	switch (c)
	{
	case 'm': case 'M':
	case 'c': case 'C':
	case 'l': case 'L':
	case 'x': case 'X':
	case 'v': case 'V':
	case 'i': case 'I':
		return 1;
	}
	return 0;
}

typedef enum {
	NOT_A_BULLET,
	BULLET,
	NUMERICAL_BULLET
} bullet_t;

static bullet_t
is_bullet_aux(int *buffer, int len, int contained)
{
	int i, decimal_pos, decimals_found;

	if (len == 1 && (
		buffer[0] == '*' ||
		buffer[0] == 0x00B7 || /* Middle Dot */
		buffer[0] == 0x2022 || /* Bullet */
		buffer[0] == 0x2023 || /* Triangular Bullet */
		buffer[0] == 0x2043 || /* Hyphen Bullet */
		buffer[0] == 0x204C || /* Back leftwards bullet */
		buffer[0] == 0x204D || /* Back rightwards bullet */
		buffer[0] == 0x2219 || /* Bullet operator */
		buffer[0] == 0x25C9 || /* Fisheye */
		buffer[0] == 0x25CB || /* White circle */
		buffer[0] == 0x25CF || /* Black circle */
		buffer[0] == 0x25D8 || /* Inverse Bullet */
		buffer[0] == 0x25E6 || /* White Bullet */
		buffer[0] == 0x2619 || /* Reversed Rotated Floral Heart Bullet / Fleuron */
		buffer[0] == 0x261a || /* Black left pointing index */
		buffer[0] == 0x261b || /* Black right pointing index */
		buffer[0] == 0x261c || /* White left pointing index */
		buffer[0] == 0x261d || /* White up pointing index */
		buffer[0] == 0x261e || /* White right pointing index */
		buffer[0] == 0x261f || /* White down pointing index */
		buffer[0] == 0x2765 || /* Rotated Heavy Heart Black Heart Bullet */
		buffer[0] == 0x2767 || /* Rotated Floral Heart Bullet / Fleuron */
		buffer[0] == 0x29BE || /* Circled White Bullet */
		buffer[0] == 0x29BF || /* Circled Bullet */
		buffer[0] == 0x2660 || /* Black Spade suit */
		buffer[0] == 0x2661 || /* White Heart suit */
		buffer[0] == 0x2662 || /* White Diamond suit */
		buffer[0] == 0x2663 || /* Black Club suit */
		buffer[0] == 0x2664 || /* White Spade suit */
		buffer[0] == 0x2665 || /* Black Heart suit */
		buffer[0] == 0x2666 || /* Black Diamond suit */
		buffer[0] == 0x2667 || /* White Clud suit */
		buffer[0] == 0x1F446 || /* WHITE UP POINTING BACKHAND INDEX */
		buffer[0] == 0x1F447 || /* WHITE DOWN POINTING BACKHAND INDEX */
		buffer[0] == 0x1F448 || /* WHITE LEFT POINTING BACKHAND INDEX */
		buffer[0] == 0x1F449 || /* WHITE RIGHT POINTING BACKHAND INDEX */
		buffer[0] == 0x1f597 || /* White down pointing left hand index */
		buffer[0] == 0x1F598 || /* SIDEWAYS WHITE LEFT POINTING INDEX */
		buffer[0] == 0x1F599 || /* SIDEWAYS WHITE RIGHT POINTING INDEX */
		buffer[0] == 0x1F59A || /* SIDEWAYS BLACK LEFT POINTING INDEX */
		buffer[0] == 0x1F59B || /* SIDEWAYS BLACK RIGHT POINTING INDEX */
		buffer[0] == 0x1F59C || /* BLACK LEFT POINTING BACKHAND INDEX */
		buffer[0] == 0x1F59D || /* BLACK RIGHT POINTING BACKHAND INDEX */
		buffer[0] == 0x1F59E || /* SIDEWAYS WHITE UP POINTING INDEX */
		buffer[0] == 0x1F59F || /* SIDEWAYS WHITE DOWN POINTING INDEX */
		buffer[0] == 0x1F5A0 || /* SIDEWAYS BLACK UP POINTING INDEX */
		buffer[0] == 0x1F5A1 || /* SIDEWAYS BLACK DOWN POINTING INDEX */
		buffer[0] == 0x1F5A2 || /* BLACK UP POINTING BACKHAND INDEX */
		buffer[0] == 0x1F5A3 || /* BLACK DOWN POINTING BACKHAND INDEX */
		buffer[0] == 0x1FBC1 || /* LEFT THIRD WHITE RIGHT POINTING INDEX */
		buffer[0] == 0x1FBC2 || /* MIDDLE THIRD WHITE RIGHT POINTING INDEX */
		buffer[0] == 0x1FBC3 || /* RIGHT THIRD WHITE RIGHT POINTING INDEX */
		buffer[0] == 0xFFFD || /* UNICODE_REPLACEMENT_CHARACTER */
		0))
		return BULLET;

	if (!contained)
	{
		if (len > 2 && buffer[0] == '(' && buffer[len-1] == ')')
			return is_bullet_aux(buffer+1, len-2, 1) ? BULLET : NOT_A_BULLET;
		if (len > 2 && buffer[0] == '<' && buffer[len-1] == '>')
			return is_bullet_aux(buffer+1, len-2, 1) ? BULLET : NOT_A_BULLET;
		if (len > 2 && buffer[0] == '[' && buffer[len-1] == ']')
			return is_bullet_aux(buffer+1, len-2, 1) ? BULLET : NOT_A_BULLET;
		if (len > 2 && buffer[0] == '{' && buffer[len-1] == '}')
			return is_bullet_aux(buffer+1, len-2, 1) ? BULLET : NOT_A_BULLET;

		if (len > 1 && buffer[len-1] == ':')
			return is_bullet_aux(buffer, len-1, 1) ? BULLET : NOT_A_BULLET;
		if (len > 1 && buffer[len-1] == ')')
			return is_bullet_aux(buffer, len-1, 1) ? BULLET : NOT_A_BULLET;
	}

	/* Look for numbers */
	/* Be careful not to interpret rows of numbers, like:
	 *    10.02 12.03
	 * as bullets.
	 */
	decimal_pos = 0;
	decimals_found = 0;
	for (i = 0; i < len; i++)
	{
		if (buffer[i] >= '0' && buffer[i] <= '9')
		{
		}
		else if (buffer[i] == '.')
		{
			decimal_pos = i;
			decimals_found++;
		}
		else
			break;
	}
	if (i == len && decimals_found <= 1)
		return NUMERICAL_BULLET;
	/* or number.something */
	if (decimals_found && i == decimal_pos+1 && i < len)
		return is_bullet_aux(buffer+i, len-i, 0) ? BULLET : NOT_A_BULLET;;

	/* Look for roman */
	for (i = 0; i < len; i++)
		if (!is_roman(buffer[i]))
			break;
	if (i == len)
		return 1;
	/* or roman.something */
	if (buffer[i] == '.' && i < len-1)
		return is_bullet_aux(buffer+i+1, len-i-1, 0) ? BULLET : NOT_A_BULLET;

	/* FIXME: Others. */
	return NOT_A_BULLET;
}

static bullet_t
is_bullet(int *buffer, int len)
{
	return is_bullet_aux(buffer, len, 0);
}

static int
eval_buffer_for_bullet(fz_context *ctx, list_data *data, float size)
{
	bullet_t bullet_type;

	bullet_type = is_bullet(data->buffer, data->buffer_fill);
	if (bullet_type == NUMERICAL_BULLET)
		data->state = LOOKING_FOR_POST_NUMERICAL_BULLET;
	else if (bullet_type)
		data->state = LOOKING_FOR_POST_BULLET;
	else
	{
		if (approx_eq(data->l, data->post_bullet_indent, size/2))
			data->state = CONTINUATION_LINE;
		else
			data->state = NO_BULLET;
		return 1;
	}
	return 0;
}

static int
list_line(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	list_data *data = (list_data *)arg;
	fz_stext_char *ch;

	for (ch = line->first_char; ch != NULL; ch = ch->next)
	{
		fz_rect r = fz_rect_from_quad(ch->quad);

		if (r.x0 < data->l)
			data->l = line->bbox.x0;

		switch (data->state)
		{
		case LOOKING_FOR_BULLET:
			if (ch->c == ' ')
			{
				/* We have a space */
				if (data->buffer_fill == 0)
					continue; /* Just skip leading spaces */
				if (eval_buffer_for_bullet(ctx, data, ch->size))
					return 0;
			}
			else if (data->buffer_fill > 0 && r.x0 - data->bullet_r > ch->size/2)
			{
				/* We have a gap large enough to be a space while we've
				 * got something in the buffer. */
				if (eval_buffer_for_bullet(ctx, data, ch->size))
					return 0;
			}
			else if (data->buffer_fill < (int)nelem(data->buffer))
			{
				/* Stick it in the buffer for evaluation later. */
				data->buffer[data->buffer_fill++] = ch->c;
			}
			else
			{
				/* Buffer overflowed. Can't be a bullet. */
				if (approx_eq(data->l, data->post_bullet_indent, ch->size))
					data->state = CONTINUATION_LINE;
				else
					data->state = NO_BULLET;
				return 0;
			}
			data->bullet_r = r.x1;
			break;
		case LOOKING_FOR_POST_BULLET:
			if (ch->c != ' ')
			{
				data->state = FOUND_BULLET;
				if (data->bullet_line_start == NULL)
					data->bullet_line_start = data->this_line_start;
				data->post_bullet_indent = r.x0;
			}
			break;
		case LOOKING_FOR_POST_NUMERICAL_BULLET:
			if (ch->c >= '0' && ch->c <= '9')
			{
				/* Numerical bullets can't be followed by numbers. */
				if (approx_eq(data->l, data->post_bullet_indent, ch->size))
					data->state = CONTINUATION_LINE;
				else
					data->state = NO_BULLET;
				return 0;
			}
			if (ch->c != ' ')
			{
				data->state = FOUND_BULLET;
				if (data->bullet_line_start == NULL)
					data->bullet_line_start = data->this_line_start;
				data->post_bullet_indent = r.x0;
			}
			break;
		default:
			break;
		}
	}

	return 0;
}

static void
list_end(fz_context *ctx, fz_stext_block *block, fz_stext_line *line, void *arg)
{
	list_data *data = (list_data *)arg;

	if (data->state == LOOKING_FOR_BULLET)
	{
		eval_buffer_for_bullet(ctx, data, 0);
		/* If we ended up thinking we'd found a bullet, subject to
		 * what follows not being of a specific form, then we're
		 * fine, because nothing follows us! */
		if (data->state == LOOKING_FOR_POST_NUMERICAL_BULLET ||
			data->state == LOOKING_FOR_POST_BULLET)
		{
			data->state = FOUND_BULLET;
			if (data->bullet_line_start == NULL)
				data->bullet_line_start = data->this_line_start;
		}
		/* FIXME: This block contains just a bullet - not the content
		 * for the bullet. We see this with page-12.pdf.
		 *    <>    Rising commitment to battery...
		 *          committed to in-house battery...
		 *          developing and manufacturing...
		 *
		 * The <> is in a whole different DIV to the following text.
		 * Really we want to look for if the "next" content (for some
		 * definition of next) is on the same line as the bullet. If
		 * it is, we want to merge the 2 divs.
		 *
		 * But that's a really tricky thing to do given the recursive
		 * block walk we are current doing. Think about this.
		 * For now, we just mark the <> as being a list item.
		 */
	}
	if (data->state == FOUND_BULLET)
	{
		if (block->u.t.first_line != data->bullet_line_start && data->state == FOUND_BULLET)
		{
			/* We need to split the block before the start of the bullet. */
			(void)split_block_at_line(ctx, data->pos, block, data->bullet_line_start);
			data->changed = 1;
			return;
		}
		if (data->bullet_line_start != data->this_line_start)
		{
			/* We've found a second bullet. Break before the line. */
			(void)split_block_at_line(ctx, data->pos, block, data->this_line_start);
			block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_LISTITEM);
			data->changed = 1;
			return;
		}
		block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_LISTITEM);
	}
	else if (data->state == NO_BULLET && data->bullet_line_start)
	{
		/* We've found a bullet before, and the line we've just completed
		 * is neither a new bullet line, or a continuation so, we need to
		 * break that into a new block. */
		(void)split_block_at_line(ctx, data->pos, block, data->this_line_start);
		block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_LISTITEM);
		data->changed = 1;
		return;
	}
	else if (data->bullet_line_start)
	{
		/* We've come to the end of the block still in the list item. */
		block_to_struct(ctx, data->pos, block, FZ_STRUCTURE_LISTITEM);
	}
}

static int
break_list_items(fz_context *ctx, stext_pos *pos, fz_stext_block *block)
{
	list_data data[1];

	if (block->u.t.flags != FZ_STEXT_TEXT_JUSTIFY_UNKNOWN)
		return 0;

	data->pos = pos;
	data->state = LOOKING_FOR_BULLET;
	data->buffer_fill = 0;
	data->l = block->bbox.x1;
	data->bullet_line_start = NULL;
	data->this_line_start = block->u.t.first_line;
	data->bullet_r = block->bbox.x0;
	data->changed = 0;

	line_walker(ctx, block, list_newline, list_line, list_end, data);

	return data->changed;
}

static int
is_header(fz_structure s)
{
	return (s == FZ_STRUCTURE_H ||
		s == FZ_STRUCTURE_H1 ||
		s == FZ_STRUCTURE_H2 ||
		s == FZ_STRUCTURE_H3 ||
		s == FZ_STRUCTURE_H4 ||
		s == FZ_STRUCTURE_H5 ||
		s == FZ_STRUCTURE_H6);
}

static void
do_para_break(fz_context *ctx, fz_stext_page *page, fz_stext_block **pfirst, fz_stext_block **plast, fz_stext_struct *parent, int in_header)
{
	fz_stext_block *block, *next_block;
	stext_pos pos;
	fz_rect bbox;

	pos.pool = page->pool;
	pos.idx = 0;
	pos.pfirst = pfirst;
	pos.plast = plast;
	pos.parent = parent;

	/* First off, in order for us to consider a block to be suitable for paragraph
	 * splitting, we want it to be a series of lines moving down the page, (or left
	 * to right within a line). */
	for (block = *pfirst; block != NULL; block = next_block)
	{
		next_block = block->next;

		switch (block->type)
		{
		case FZ_STEXT_BLOCK_STRUCT:
			if (block->u.s.index < pos.idx)
				block->u.s.index = pos.idx++;
			else
				pos.idx = block->u.s.index+1;
			if (block->u.s.down)
			{
				int header = in_header | is_header(block->u.s.down->standard);
				do_para_break(ctx, page, &block->u.s.down->first_block, &block->u.s.down->last_block, block->u.s.down, header);
			}
			break;
		case FZ_STEXT_BLOCK_TEXT:
			if (!lines_move_plausibly_like_paragraph(block))
				break;

#ifdef DEBUG_SPLITS
			dump_block(ctx, "Around the top level block loop:", block);
#endif

			/* Firstly, and somewhat annoyingly we need to find the bbox of the
			 * block that doesn't include for trailing spaces. If we just use
			 * the normal bbox, then lines that end in "foo " will end further
			 * to the right of lines that end in "ba-", and consequently we'll
			 * fail to detect blocks as being justified.
			 * See PMC2656817_00002.pdf as an example. */
			bbox = text_block_marked_bbox(ctx, block);

#ifdef DEBUG_PARA_SPLITS
			{
				fz_stext_line *line;

				for (line = block->u.t.first_line; line != NULL; line = line->next)
				{
					fz_stext_char *ch;

					for (ch = line->first_char; ch != NULL; ch = ch->next)
					{
						fz_write_printf(ctx, fz_stddbg(ctx), "%C", ch->c);
					}
				}
			}
#endif

			/* Think about breaking lines at Titles. */
			/* First, underlined ones. */
			if (!in_header)
			{
			if (detect_underlined_titles(ctx, &pos, block))
				next_block = block->next; /* We split the block! */
			if (block->type != FZ_STEXT_BLOCK_TEXT)
			{
				next_block = block;
				break;
				}
			}

#ifdef DEBUG_PARA_SPLITS
			fz_write_printf(ctx, fz_stddbg(ctx), "A");
#endif

			/* Next, ones that use bold fonts. */
			if (!in_header)
			{
				if (detect_titles_by_font_usage(ctx, &pos, block))
					next_block = block->next; /* We split the block! */
				if (block->type != FZ_STEXT_BLOCK_TEXT)
				{
					next_block = block;
					break;
				}
			}

#ifdef DEBUG_PARA_SPLITS
			fz_write_printf(ctx, fz_stddbg(ctx), "B");
#endif

			/* Now look at breaking based upon indents */
			if (break_paragraphs_by_indent(ctx, &pos, block, bbox))
				next_block = block->next; /* We split the block! */
			if (block->type != FZ_STEXT_BLOCK_TEXT)
			{
				next_block = block;
				break;
			}

#ifdef DEBUG_PARA_SPLITS
			fz_write_printf(ctx, fz_stddbg(ctx), "C");
#endif

			/* Now we're going to look for unindented paragraphs. We do this by
			 * considering if the first word on the next line would have fitted
			 * into the space left at the end of the previous line. */
			if (break_paragraphs_by_analysing_trailing_gaps(ctx, &pos, block, bbox))
				next_block = block->next; /* We split the block! */
			if (block->type != FZ_STEXT_BLOCK_TEXT)
			{
				next_block = block;
				break;
			}

#ifdef DEBUG_PARA_SPLITS
			fz_write_printf(ctx, fz_stddbg(ctx), "D");
#endif

			/* Now look to see if a block looks like fully justified text. If it
			 * does, then any line that doesn't reach the right hand side must be
			 * a paragraph break. */
			if (break_paragraphs_within_justified_text(ctx, &pos, block, bbox))
				next_block = block->next; /* We split the block! */
			if (block->type != FZ_STEXT_BLOCK_TEXT)
			{
				next_block = block;
				break;
			}

#ifdef DEBUG_PARA_SPLITS
			fz_write_printf(ctx, fz_stddbg(ctx), "E");
#endif

			/* Look for bulleted list items. */
			if (break_list_items(ctx, &pos, block))
				next_block = block->next; /* We split the block! */

			break;
		}
	}
}

void
fz_paragraph_break(fz_context *ctx, fz_stext_page *page)
{
	do_para_break(ctx, page, &page->first_block, &page->last_block, NULL, 0);
}
