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

#include <string.h>
#include <limits.h>
#include <assert.h>

/* Enumerate marked selection */

static float hdist(fz_point *dir, fz_point *a, fz_point *b)
{
	float dx = b->x - a->x;
	float dy = b->y - a->y;
	return fz_abs(dx * dir->x - dy * dir->y);
}

static float vdist(fz_point *dir, fz_point *a, fz_point *b)
{
	float dx = b->x - a->x;
	float dy = b->y - a->y;
	return fz_abs(dx * dir->y - dy * dir->x);
}

static float vecdot(fz_point a, fz_point b)
{
	return a.x * b.x + a.y * b.y;
}

static float linedist(fz_point origin, fz_point dir, fz_point q)
{
	return vecdot(dir, fz_make_point(q.x - origin.x, q.y - origin.y));
}

static int line_length(fz_stext_line *line)
{
	fz_stext_char *ch;
	int n = 0;
	for (ch = line->first_char; ch; ch = ch->next)
		++n;
	return n;
}

static float largest_size_in_line(fz_stext_line *line)
{
	fz_stext_char *ch;
	float size = 0;
	for (ch = line->first_char; ch; ch = ch->next)
		if (ch->size > size)
			size = ch->size;
	return size;
}

static int find_closest_in_line(fz_stext_line *line, int idx, fz_point q)
{
	fz_stext_char *ch;
	float closest_dist = 1e30f;
	int closest_idx = idx;
	float d1, d2;

	float hsize = largest_size_in_line(line) / 2;
	fz_point vdir = fz_make_point(-line->dir.y, line->dir.x);
	fz_point hdir = line->dir;

	// Compute mid-line from quads!
	fz_point p1 = fz_make_point(
		(line->first_char->quad.ll.x + line->first_char->quad.ul.x) / 2,
		(line->first_char->quad.ll.y + line->first_char->quad.ul.y) / 2
	);

	// Signed distance perpendicular mid-line (positive is below)
	float vdist = linedist(p1, vdir, q);
	if (vdist < -hsize)
		return idx;
	if (vdist > hsize)
		return idx + line_length(line);

	for (ch = line->first_char; ch; ch = ch->next)
	{
		if (ch->bidi & 1)
		{
			d1 = fz_abs(linedist(ch->quad.lr, hdir, q));
			d2 = fz_abs(linedist(ch->quad.ll, hdir, q));
		}
		else
		{
			d1 = fz_abs(linedist(ch->quad.ll, hdir, q));
			d2 = fz_abs(linedist(ch->quad.lr, hdir, q));
		}

		if (d1 < closest_dist)
		{
			closest_dist = d1;
			closest_idx = idx;
		}

		if (d2 < closest_dist)
		{
			closest_dist = d2;
			closest_idx = idx + 1;
		}

		++idx;
	}

	return closest_idx;
}

static int find_closest_in_page(fz_stext_page *page, fz_point q)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_line *closest_line = NULL;
	int closest_idx = 0;
	float closest_vdist = 1e30f;
	float closest_hdist = 1e30f;
	int idx = 0;

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_TEXT)
			continue;
		for (line = block->u.t.first_line; line; line = line->next)
		{
			float hsize = largest_size_in_line(line) / 2;
			fz_point hdir = line->dir;
			fz_point vdir = fz_make_point(-line->dir.y, line->dir.x);

			// Compute mid-line from quads!
			fz_point p1 = fz_make_point(
				(line->first_char->quad.ll.x + line->first_char->quad.ul.x) / 2,
				(line->first_char->quad.ll.y + line->first_char->quad.ul.y) / 2
			);
			fz_point p2 = fz_make_point(
				(line->last_char->quad.lr.x + line->last_char->quad.ur.x) / 2,
				(line->last_char->quad.lr.y + line->last_char->quad.ur.y) / 2
			);

			// Signed distance perpendicular mid-line (positive is below)
			float vdist = linedist(p1, vdir, q);

			// Signed distance tangent to mid-line from end points (positive is to end)
			float hdist1 = linedist(p1, hdir, q);
			float hdist2 = linedist(p2, hdir, q);

			// Within the line itself!
			if (vdist >= -hsize && vdist <= hsize && (hdist1 > 0) != (hdist2 > 0))
			{
				closest_vdist = 0;
				closest_hdist = 0;
				closest_line = line;
				closest_idx = idx;
			}
			else
			{
				// Vertical distance from mid-line.
				float avdist = fz_abs(vdist);

				// Horizontal distance from closest end-point
				float ahdist = fz_min(fz_abs(hdist1), fz_abs(hdist2));

				if (avdist < hsize)
				{
					// Within extended line
					if (ahdist <= closest_hdist)
					{
						closest_vdist = 0;
						closest_hdist = ahdist;
						closest_line = line;
						closest_idx = idx;
					}
				}
				else
				{
					// Outside line
					// TODO: closest column?
					if (avdist <= closest_vdist)
					{
						closest_vdist = avdist;
						closest_line = line;
						closest_idx = idx;
					}
				}
			}

			idx += line_length(line);
		}
	}

	if (closest_line)
		return find_closest_in_line(closest_line, closest_idx, q);

	return 0;
}

struct callbacks
{
	void (*on_char)(fz_context *ctx, void *arg, fz_stext_line *ln, fz_stext_char *ch);
	void (*on_line)(fz_context *ctx, void *arg, fz_stext_line *ln);
	void *arg;
};

static void
fz_enumerate_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, struct callbacks *cb)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	int idx, start, end;
	int inside;

	start = find_closest_in_page(page, a);
	end = find_closest_in_page(page, b);

	if (start > end)
		idx = start, start = end, end = idx;

	if (start == end)
		return;

	inside = 0;
	idx = 0;
	for (block = page->first_block; block; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_TEXT)
			continue;
		for (line = block->u.t.first_line; line; line = line->next)
		{
			for (ch = line->first_char; ch; ch = ch->next)
			{
				if (!inside)
					if (idx == start)
						inside = 1;
				if (inside)
					cb->on_char(ctx, cb->arg, line, ch);
				if (++idx == end)
					return;
			}
			if (inside)
				cb->on_line(ctx, cb->arg, line);
		}
	}
}

fz_quad
fz_snap_selection(fz_context *ctx, fz_stext_page *page, fz_point *a, fz_point *b, int mode)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	fz_quad handles;
	int idx, start, end;
	int pc;

	start = find_closest_in_page(page, *a);
	end = find_closest_in_page(page, *b);

	if (start > end)
		idx = start, start = end, end = idx;

	handles.ll = handles.ul = *a;
	handles.lr = handles.ur = *b;

	idx = 0;
	for (block = page->first_block; block; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_TEXT)
			continue;
		for (line = block->u.t.first_line; line; line = line->next)
		{
			pc = '\n';
			for (ch = line->first_char; ch; ch = ch->next)
			{
				if (idx <= start)
				{
					if (mode == FZ_SELECT_CHARS
						|| (mode == FZ_SELECT_WORDS && (pc == ' ' || pc == '\n'))
						|| (mode == FZ_SELECT_LINES && (pc == '\n')))
					{
						handles.ll = ch->quad.ll;
						handles.ul = ch->quad.ul;
						*a = ch->origin;
					}
				}
				if (idx >= end)
				{
					if (mode == FZ_SELECT_CHARS
						|| (mode == FZ_SELECT_WORDS && (ch->c == ' ')))
					{
						handles.lr = ch->quad.ll;
						handles.ur = ch->quad.ul;
						*b = ch->origin;
						return handles;
					}
					if (!ch->next)
					{
						handles.lr = ch->quad.lr;
						handles.ur = ch->quad.ur;
						*b = ch->quad.lr;
						return handles;
					}
				}
				pc = ch->c;
				++idx;
			}
		}
	}

	return handles;
}

/* Highlight selection */

struct highlight
{
	int len, cap;
	fz_quad *box;
	float hfuzz, vfuzz;
};

int same_point(fz_point a, fz_point b)
{
	int dx = fz_abs(a.x - b.x);
	int dy = fz_abs(a.y - b.y);
	return (dx < 0.1 && dy < 0.1);
}

int is_near(float hfuzz, float vfuzz, fz_point hdir, fz_point end, fz_point p1, fz_point p2)
{
	float v = fz_abs(linedist(end, fz_make_point(-hdir.y, hdir.x), p1));
	float d1 = fz_abs(linedist(end, hdir, p1));
	float d2 = fz_abs(linedist(end, hdir, p2));
	return (v < vfuzz && d1 < hfuzz && d1 < d2);
}

static void on_highlight_char(fz_context *ctx, void *arg, fz_stext_line *line, fz_stext_char *ch)
{
	struct highlight *hits = arg;
	float vfuzz = hits->vfuzz * ch->size;
	float hfuzz = hits->hfuzz * ch->size;
	fz_point dir = line->dir;

	// Skip zero-extent quads
	if (same_point(ch->quad.ll, ch->quad.lr))
		return;

	if (hits->len > 0)
	{
		fz_quad *end = &hits->box[hits->len-1];

		if (is_near(hfuzz, vfuzz, dir, end->lr, ch->quad.ll, ch->quad.lr) &&
			is_near(hfuzz, vfuzz, dir, end->ur, ch->quad.ul, ch->quad.ur))
		{
			end->ur = ch->quad.ur;
			end->lr = ch->quad.lr;
			return;
		}

		if (is_near(hfuzz, vfuzz, dir, end->ll, ch->quad.lr, ch->quad.ll) &&
			is_near(hfuzz, vfuzz, dir, end->ul, ch->quad.ur, ch->quad.ul))
		{
			end->ul = ch->quad.ul;
			end->ll = ch->quad.ll;
			return;
		}
	}

	if (hits->len < hits->cap)
		hits->box[hits->len++] = ch->quad;
}

static void on_highlight_line(fz_context *ctx, void *arg, fz_stext_line *line)
{
}

int
fz_highlight_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, fz_quad *quads, int max_quads)
{
	struct callbacks cb;
	struct highlight hits;

	hits.len = 0;
	hits.cap = max_quads;
	hits.box = quads;
	hits.hfuzz = 0.5f; /* merge large gaps */
	hits.vfuzz = 0.1f;

	cb.on_char = on_highlight_char;
	cb.on_line = on_highlight_line;
	cb.arg = &hits;

	fz_enumerate_selection(ctx, page, a, b, &cb);

	return hits.len;
}

/* Copy selection */

static void on_copy_char(fz_context *ctx, void *arg, fz_stext_line *line, fz_stext_char *ch)
{
	fz_buffer *buffer = arg;
	int c = ch->c;
	if (c < 32)
		c = FZ_REPLACEMENT_CHARACTER;
	fz_append_rune(ctx, buffer, c);
}

static void on_copy_line_crlf(fz_context *ctx, void *arg, fz_stext_line *line)
{
	fz_buffer *buffer = arg;
	fz_append_byte(ctx, buffer, '\r');
	fz_append_byte(ctx, buffer, '\n');
}

static void on_copy_line_lf(fz_context *ctx, void *arg, fz_stext_line *line)
{
	fz_buffer *buffer = arg;
	fz_append_byte(ctx, buffer, '\n');
}

char *
fz_copy_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, int crlf)
{
	struct callbacks cb;
	fz_buffer *buffer;
	unsigned char *s;

	buffer = fz_new_buffer(ctx, 1024);
	fz_try(ctx)
	{
		cb.on_char = on_copy_char;
		cb.on_line = crlf ? on_copy_line_crlf : on_copy_line_lf;
		cb.arg = buffer;

		fz_enumerate_selection(ctx, page, a, b, &cb);
		fz_terminate_buffer(ctx, buffer);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		fz_rethrow(ctx);
	}
	fz_buffer_extract(ctx, buffer, &s); /* take over the data */
	fz_drop_buffer(ctx, buffer);
	return (char*)s;
}

char *
fz_copy_rectangle(fz_context *ctx, fz_stext_page *page, fz_rect area, int crlf)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	fz_buffer *buffer;
	unsigned char *s;

	int need_new_line = 0;

	buffer = fz_new_buffer(ctx, 1024);
	fz_try(ctx)
	{
		for (block = page->first_block; block; block = block->next)
		{
			if (block->type != FZ_STEXT_BLOCK_TEXT)
				continue;
			for (line = block->u.t.first_line; line; line = line->next)
			{
				int line_had_text = 0;
				for (ch = line->first_char; ch; ch = ch->next)
				{
					fz_rect r = fz_rect_from_quad(ch->quad);
					if (!fz_is_empty_rect(fz_intersect_rect(r, area)))
					{
						line_had_text = 1;
						if (need_new_line)
						{
							fz_append_string(ctx, buffer, crlf ? "\r\n" : "\n");
							need_new_line = 0;
						}
						fz_append_rune(ctx, buffer, ch->c < 32 ? FZ_REPLACEMENT_CHARACTER : ch->c);
					}
				}
				if (line_had_text)
					need_new_line = 1;
			}
		}
		fz_terminate_buffer(ctx, buffer);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		fz_rethrow(ctx);
	}

	fz_buffer_extract(ctx, buffer, &s); /* take over the data */
	fz_drop_buffer(ctx, buffer);
	return (char*)s;
}

/* String search */

static inline int canon(int c)
{
	// Map full-width ASCII forms to ASCII:
	// U+FF01 .. U+FF5E => U+0021 .. U+007E
	if (c >= 0xFF01 && c <= 0xFF5E)
		c = c - 0xFF01 + 0x21;

	if (c == 0xA0 || c == 0x2028 || c == 0x2029)
		return ' ';
	if (c == '\r' || c == '\n' || c == '\t')
		return ' ';

	return fz_toupper(c);
}

static inline int chartocanon(int *c, const char *s)
{
	int n = fz_chartorune(c, s);
	*c = canon(*c);
	return n;
}

static const char *match_string(const char *h, const char *n)
{
	int hc, nc;
	const char *e = h;
	h += chartocanon(&hc, h);
	n += chartocanon(&nc, n);
	while (hc == nc)
	{
		e = h;
		if (hc == ' ')
			do
				h += chartocanon(&hc, h);
			while (hc == ' ');
		else
			h += chartocanon(&hc, h);
		if (nc == ' ')
			do
				n += chartocanon(&nc, n);
			while (nc == ' ');
		else
			n += chartocanon(&nc, n);
	}
	return nc == 0 ? e : NULL;
}

static const char *find_string(const char *s, const char *needle, const char **endp)
{
	const char *end;
	while (*s)
	{
		end = match_string(s, needle);
		if (end)
			return *endp = end, s;
		++s;
	}
	return *endp = NULL, NULL;
}

static void add_hit_char(fz_context *ctx, struct highlight *hits, int *hit_mark, fz_stext_line *line, fz_stext_char *ch, int is_at_start)
{
	float vfuzz = ch->size * hits->vfuzz;
	float hfuzz = ch->size * hits->hfuzz;

	if (hits->len > 0 && !is_at_start)
	{
		fz_quad *end = &hits->box[hits->len-1];
		if (hdist(&line->dir, &end->lr, &ch->quad.ll) < hfuzz
			&& vdist(&line->dir, &end->lr, &ch->quad.ll) < vfuzz
			&& hdist(&line->dir, &end->ur, &ch->quad.ul) < hfuzz
			&& vdist(&line->dir, &end->ur, &ch->quad.ul) < vfuzz)
		{
			end->ur = ch->quad.ur;
			end->lr = ch->quad.lr;
			return;
		}
	}

	if (hits->len < hits->cap)
	{
		if (hit_mark)
			hit_mark[hits->len] = is_at_start;
		hits->box[hits->len] = ch->quad;
		hits->len++;
	}
}

int
fz_search_stext_page(fz_context *ctx, fz_stext_page *page, const char *needle, int *hit_mark, fz_quad *quads, int max_quads)
{
	struct highlight hits;
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	fz_buffer *buffer;
	const char *haystack, *begin, *end;
	int c, inside;

	if (strlen(needle) == 0)
		return 0;

	hits.len = 0;
	hits.cap = max_quads;
	hits.box = quads;
	hits.hfuzz = 0.2f; /* merge kerns but not large gaps */
	hits.vfuzz = 0.1f;

	buffer = fz_new_buffer_from_stext_page(ctx, page);
	fz_try(ctx)
	{
		haystack = fz_string_from_buffer(ctx, buffer);
		begin = find_string(haystack, needle, &end);
		if (!begin)
			goto no_more_matches;

		inside = 0;
		for (block = page->first_block; block; block = block->next)
		{
			if (block->type != FZ_STEXT_BLOCK_TEXT)
				continue;
			for (line = block->u.t.first_line; line; line = line->next)
			{
				for (ch = line->first_char; ch; ch = ch->next)
				{
try_new_match:
					if (!inside)
					{
						if (haystack >= begin)
							inside = 1;
					}
					if (inside)
					{
						if (haystack < end)
						{
							add_hit_char(ctx, &hits, hit_mark, line, ch, haystack == begin);
						}
						else
						{
							inside = 0;
							begin = find_string(haystack, needle, &end);
							if (!begin)
								goto no_more_matches;
							else
								goto try_new_match;
						}
					}
					haystack += fz_chartorune(&c, haystack);
				}
				assert(*haystack == '\n');
				++haystack;
			}
			assert(*haystack == '\n');
			++haystack;
		}
no_more_matches:;
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buffer);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return hits.len;
}
