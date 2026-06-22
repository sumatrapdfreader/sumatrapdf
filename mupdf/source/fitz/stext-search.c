// Copyright (C) 2004-2026 Artifex Software, Inc.
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

/* SumatraPDF: different location */
#include "../../../ext/mujs/regexp.h"

#include "mupdf/ucdn.h"

#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>

const char *fz_search_options_usage =
	"Search options:\n"
	"\texact: match exact, case sensitive pattern\n"
	"\tignore-case: case insensitive search\n"
	"\tignore-diacritics: ignore character diacritics\n"
	"\tregexp: interpret search pattern as regular expression\n"
	"\tkeep-lines: preserve line breaks so pattern can match them\n"
	"\tkeep-paragraphs: preserve paragraph breaks so pattern can match them\n"
	"\tkeep-hyphens: preserve hyphens, avoiding joining lines\n"
	"\n";

void fz_init_search_options(fz_context *ctx, fz_search_options *opts)
{
	*opts = 0;
}

fz_search_options *fz_parse_search_options(fz_context *ctx, fz_search_options *opts, const char *args)
{
	fz_options *options = fz_new_options(ctx, args);
	fz_try(ctx)
	{
		fz_init_search_options(ctx, opts);
		fz_apply_search_options(ctx, opts, options);
		fz_throw_on_unused_options(ctx, options, "search");
	}
	fz_always(ctx)
		fz_drop_options(ctx, options);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return opts;
}

void fz_apply_search_options(fz_context *ctx, fz_search_options *opts, fz_options *args)
{
	fz_search_options mask = *opts;

	// TODO: stricter parsing of options bitmask string
	if (fz_lookup_option_yes(ctx, args, "exact")) mask |= FZ_SEARCH_EXACT;
	if (fz_lookup_option_yes(ctx, args, "ignore-case")) mask |= FZ_SEARCH_IGNORE_CASE;
	if (fz_lookup_option_yes(ctx, args, "ignore-diacritics")) mask |= FZ_SEARCH_IGNORE_DIACRITICS;
	if (fz_lookup_option_yes(ctx, args, "regexp")) mask |= FZ_SEARCH_REGEXP;
	if (fz_lookup_option_yes(ctx, args, "keep-lines")) mask |= FZ_SEARCH_KEEP_LINES;
	if (fz_lookup_option_yes(ctx, args, "keep-paragraphs")) mask |= FZ_SEARCH_KEEP_PARAGRAPHS;
	if (fz_lookup_option_yes(ctx, args, "keep-hyphens")) mask |= FZ_SEARCH_KEEP_HYPHENS;

	fz_validate_options(ctx, args, "search");

	*opts = mask;
}

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

static int same_point(fz_point a, fz_point b)
{
	int dx = fz_abs(a.x - b.x);
	int dy = fz_abs(a.y - b.y);
	return (dx < 0.1 && dy < 0.1);
}

static int is_near(float hfuzz, float vfuzz, fz_point hdir, fz_point end, fz_point p1, fz_point p2)
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

static int my_isspace(int ch)
{
	/* \t, \n, \v, \f, \r, space */
	return (ch == 9 || ch == 10 || ch == 11 || ch == 12 || ch == 13 || ch == 32);
}

int fz_is_unicode_whitespace(int c)
{
	if (fz_is_unicode_space_equivalent(c))
		return 1;
	if (c == 0x2028)
		return 1; /* Line separator */
	if (c == 0x2029)
		return 1; /* Paragraph separator */
	if (c > 0 && c < 256 && my_isspace(c))
		return 1;
	return 0;
}

/*
 * NEW ADVANCED SEARCH API
 */

int
fz_is_unicode_space_equivalent(int c)
{
	switch (c)
	{
	case 0xa0:	/* NO-BREAK-SPACE */
	case 0x1680:	/* OGHAM SPACE MARK */
	case 0x2000:	/* EN QUAD */
	case 0x2001:	/* EM QUAD */
	case 0x2002:	/* EN SPACE */
	case 0x2003:	/* EM SPACE */
	case 0x2004:	/* THREE-PER-EM-SPACE */
	case 0x2005:	/* FOUR-PER-EM-SPACE */
	case 0x2006:	/* SIX-PER-EM-SPACE */
	case 0x2007:	/* FIGURE-SPACE */
	case 0x2008:	/* PUNCTUATION-SPACE */
	case 0x2009:	/* THIN-SPACE */
	case 0x200A:	/* HAIR-SPACE */
	case 0x202F:	/* NARROW-NO-BREAK-SPACE */
	case 0x205F:	/* MEDIUM-MATHEMATICAL-SPACE */
	case 0x3000:	/* IDEOGRAPHIC-SPACE */
		return 1;
	}

	return 0;
}

static const char *advance_one_utf8(const char *s, const char *p)
{
	if (p >= s && *p == 0x00)
		return NULL;
	p++;
	while (((*p & 0x80) != 0) && ((*p & 0xc0) != 0xc0) && *p)
		p++;
	return p;
}

static const char *retreat_one_utf8(const char *s, const char *p)
{
	if (p <= s)
		return NULL;
	do
		--p;
	while (((*p & 0x80) != 0) && ((*p & 0xc0) != 0xc0) && p > s);
	return p;
}

static inline int canon(int c)
{
	if (c == '\r' || c == '\n' || c == '\t' || fz_is_unicode_space_equivalent(c))
		return ' ';
	return c;
}

static inline int is_marking_nonspacing(const char *h)
{
	/* match must not end between base and combining character */
	int hc;
	if (*h < 0)
	{
		(void)fz_chartorune(&hc, h);
		if (ucdn_get_general_category(hc) == UCDN_GENERAL_CATEGORY_MN)
			return 1;
	}
	return 0;
}

static const char *match_exact(const char *h, const char *n)
{
	int hc, nc;
	do {
		n += fz_chartorune(&nc, n);
		if (nc == 0)
		{
			if (is_marking_nonspacing(h))
				return NULL;
			return h;
		}
		h += fz_chartorune(&hc, h);
	} while (hc == nc);
	return NULL;
}

static const char *find_exact(fz_context *ctx, void *dummy, const char *s, const char *p, const char *needle, const char **endp)
{
	const char *end;
	while (p)
	{
		end = match_exact(p, needle);
		if (end)
			return *endp = end, p;
		p = advance_one_utf8(s, p);
	}
	return *endp = NULL, NULL;
}

static const char *find_rev_exact(fz_context *ctx, void *dummy, const char *s, const char *p, const char *needle, const char **endp)
{
	const char *end;
	p = retreat_one_utf8(s, p);
	while (p)
	{
		end = match_exact(p, needle);
		if (end)
			return *endp = end, p;

		p = retreat_one_utf8(s, p);
	}
	return *endp = NULL, NULL;
}

static const char *find_regexp(fz_context *ctx, void *arg, const char *s, const char *p, const char *needle, const char **endp)
{
	Reprog **prog = (Reprog **)arg;
	Resub m;

	while (p)
	{
		int bol = (p == s) || (p[-1] == '\n');
		int result = js_regexec(*prog, p, &m, bol ? 0 : REG_NOTBOL);
		if (result < 0)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "regexec failure");
		if (result == 1)
			return *endp = NULL, NULL;
		if (m.sub[0].ep - m.sub[0].sp > 0 && !is_marking_nonspacing(m.sub[0].ep))
			return *endp = m.sub[0].ep, m.sub[0].sp;
		p = advance_one_utf8(s, p);
	}

	return *endp = NULL, NULL;
}

static const char *find_rev_regexp(fz_context *ctx, void *arg, const char *s, const char *p, const char *needle, const char **endp)
{
	/* This is pretty horrible.
	 * We search from the start for a match; if we fail to find it, no match.
	 * If we match, but the match starts after our offset, no match.
	 * Otherwise, we keep looking for matches further on in the buffer.
	 */
	const char *start, *later_end, *later_start;

	start = find_regexp(ctx, arg, s, s, needle, endp);
	if (start == NULL || (start - s) >= (p - s))
	{
		/* No match at all. */
		*endp = NULL;
		return NULL;
	}

	/* Now look for a later one */
	while (1)
	{
		later_start = find_regexp(ctx, arg, s, *endp, needle, &later_end);
		if (later_start == NULL || (later_end - s) > (p - s))
			return start;
		start = later_start;
		*endp = later_end;
	}
}

/**
  A set of functions to implement a 'finder' for the match
  functions.
 */
typedef struct
{
	/* init: May be NULL. If not, it will be called with the needle
	 * that is to be used. This allows the needle to be 'compiled'.
	 * This should throw if the needle fails to compile.
	 */
	void (*init)(fz_context *ctx, void *find_arg, const char *needle);

	/* find: Search for the needle in the haystack.
	 *
	 * Return value is NULL (and *endp = NULL) if no match found.
	 * Otherwise, return value is the start of the match to the
	 * needle in the haystack (and *endp = one byte beyond
	 * the last character included in the match).
	 */
	const char *(*find)(fz_context *ctx, void *find_arg, const char *haystack, const char *current, const char *needle, const char **endp);

	/* find_rev: Search for the needle backwards in the haystack.
	 *
	 * Return value is NULL (and *endp = NULL) if no match found.
	 * Otherwise, return value is the start of the match to the
	 * needle in the haystack (and *endp = one byte beyond
	 * the last character included in the match).
	 */
	const char *(*find_rev)(fz_context *ctx, void *find_arg, const char *haystack, const char *current, const char *needle, const char **endp);

	/* fin: Maybe NULL. If not, it will be called whenever the
	 * search process has finished (after a successful init).
	 * This allows the 'compiled' needle to be freed. Must
	 * never throw.
	 */
	void (*fin)(fz_context *ctx, void *find_arg);
} fz_match_finder;

static const fz_match_finder simple_finder =
{
	NULL,
	find_exact,
	find_rev_exact,
	NULL
};

static void
init_regexp(fz_context *ctx, void *find_arg, const char *needle)
{
	Reprog **progp = (void *)find_arg;

	*progp = js_regcomp(needle, REG_NEWLINE, NULL);
	if (*progp == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "regcomp failure");
}

static void
init_regexp_insensitive(fz_context *ctx, void *find_arg, const char *needle)
{
	Reprog **progp = (void *)find_arg;

	*progp = js_regcomp(needle, REG_NEWLINE | REG_ICASE, NULL);
	if (*progp == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "regcomp failure");
}

static void
fin_regexp(fz_context *ctx, void *find_arg)
{
	Reprog **progp = (void *)find_arg;
	js_regfree(*progp);
}

static const fz_match_finder regexp_finder =
{
	init_regexp,
	find_regexp,
	find_rev_regexp,
	fin_regexp
};

static const fz_match_finder regexp_finder_insensitive =
{
	init_regexp_insensitive,
	find_regexp,
	find_rev_regexp,
	fin_regexp
};

/**
  Bitfield of transforms to apply to search text before passing to
  the finder functions.

  All search text is transformed so that newline, carriage return,
  tab, line separator, paragraph separator and non-breaking space
  are are converted to space.

  Runs of multiple "spaces" (including the above chars) are
  transformed to single spaces.

  Logical OR these together as appropriate to make the required
  transformations.

  In general these will be applied from larger numbers to smaller
  numbers.
 */
typedef enum
{
	FZ_TEXT_TRANSFORM_NONE = 0,

	/* Perform unicode composition on the strings. */
	FZ_TEXT_TRANSFORM_COMPOSE = 1,

	/* Convert all characters to upper case. */
	FZ_TEXT_TRANSFORM_UPPERCASE = 2,

	/* Convert full width ASCII variants to standard ASCII variants */
	FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII = 4,

	/* Strip "marking, non-spacing" characters out of the string. */
	FZ_TEXT_TRANSFORM_STRIP_MARKING_NONSPACING = 8,

	/* Perform unicode decomposition on the string. (i.e. NFD,
	 * according to Unicode tr15)
	 */
	FZ_TEXT_TRANSFORM_DECOMPOSE = 16,

	/* Perform unicode compatibility decomposition on the string.
	 * (i.e. NFKD according to Unicode tr15)
	 */
	FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE = 32,

	/* Normalize all hyphen-equivalents (fz_is_unicode_hyphen) to '-'.
	 */
	FZ_TEXT_TRANSFORM_NORMALIZE_HYPHENS = 64,

	/* Preserve line breaks (mainly for use with regexps) -
	 * line breaks are preserved as single \n.
	 */
	FZ_TEXT_TRANSFORM_KEEP_LINES = 256,

	/* Preserve paragraph breaks (mainly for use with regexps) -
	 * paragraph breaks are preserved as double \n.
	 */
	FZ_TEXT_TRANSFORM_KEEP_PARAGRAPHS = 512,

	/* Preserve hyphens. Without this, they are removed and
	 * lines joined.
	 */
	FZ_TEXT_TRANSFORM_KEEP_HYPHENS = 1024,


	/* And some useful combinations of these flags */

	/* NORMAL: case sensitive text matching, allowing for different
	 * encodings of the same diacritics.
	 */
	FZ_TEXT_TRANSFORM__NORMAL =
		FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
		FZ_TEXT_TRANSFORM_NORMALIZE_HYPHENS |
		FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
		FZ_TEXT_TRANSFORM_COMPOSE,

	/* IGNORE_CASE: flags necessary for case insensitive matching */
	FZ_TEXT_TRANSFORM__IGNORE_CASE =
		FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
		FZ_TEXT_TRANSFORM_NORMALIZE_HYPHENS |
		FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
		FZ_TEXT_TRANSFORM_UPPERCASE |
		FZ_TEXT_TRANSFORM_COMPOSE,

	/* IGNORE_DIACRITICS: flags necessary to ignore marking non-spacing characters (i.e. diacritics) */
	FZ_TEXT_TRANSFORM__IGNORE_DIACRITICS =
		FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
		FZ_TEXT_TRANSFORM_NORMALIZE_HYPHENS |
		FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
		FZ_TEXT_TRANSFORM_STRIP_MARKING_NONSPACING |
		FZ_TEXT_TRANSFORM_COMPOSE,

	/* IGNORE_CASE_DIACRITICS: flags necessary to ignore both case and marking non-spacing characters (i.e. diacritics) */
	FZ_TEXT_TRANSFORM__IGNORE_CASE_DIACRITICS =
		FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
		FZ_TEXT_TRANSFORM_NORMALIZE_HYPHENS |
		FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
		FZ_TEXT_TRANSFORM_STRIP_MARKING_NONSPACING |
		FZ_TEXT_TRANSFORM_UPPERCASE |
		FZ_TEXT_TRANSFORM_COMPOSE,
} fz_text_transform;

/* Given that the maximum we can decompose from a single char is
 * 18, it seems reasonable to only be able compose the same number?
 * Anyway, 32 should be WAY more than is ever needed in the real
 * world.
 */
typedef struct
{
	int len;
	int cache[32];
	size_t pos[32];
} transform_cache;

static size_t
flush_transform_cache(fz_context *ctx, char *output, size_t *index, fz_text_transform transform, transform_cache *cache)
{
	int i, j, k, n = cache->len;
	char dummy[4];
	size_t len;

	if (n == 0)
		return 0;

	/* First off, do we need to bubblesort?*/
	if (transform & (FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE | FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE | FZ_TEXT_TRANSFORM_COMPOSE))
	{
		/* We need to reorder. Simple bubblesort. */
		for (i = 0; i < n-1; i++)
		{
			int i_ccc = ucdn_get_combining_class(cache->cache[i]);
			for (j = i+1; j < n; j++)
	{
				int j_ccc = ucdn_get_combining_class(cache->cache[j]);
				if (i_ccc > j_ccc)
		{
					int t = cache->cache[j];
					cache->cache[j] = cache->cache[i];
					cache->cache[i] = t;

					// NOTE: We do NOT swap the positions here.
					// The index must be in increasing order for lookup_stext_position to work, so we have no choice here.
					// This means the index will be jumbled for the individual marks in combining characters.
					// That shouldn't matter because we should not split text in the middle of a character and its combining marks.

					i_ccc = j_ccc;
				}
			}
		}
	}

	if (transform & FZ_TEXT_TRANSFORM_COMPOSE)
	{
		for (i = 0, j = 1; j < n; j++)
		{
			uint32_t a;
			if (ucdn_compose(&a, cache->cache[i], cache->cache[j]))
			{
				/* We composed. */
				cache->cache[i] = (int)a;
			}
			else
			{
				/* Composition failed. */
				cache->cache[++i] = cache->cache[j];
				cache->pos[i] = cache->pos[j];
			}
		}
		n = i+1;
}

	/* Now actually do the output */
	len = 0;
	for (i = 0; i < n; i++)
	{
		if (output)
		{
			j = fz_runetochar(output, cache->cache[i]);
			output += j;
		}
		else
		{
			j = fz_runetochar(dummy, cache->cache[i]);
		}
		if (index)
		{
			for (k = 0; k < j; k++, index++)
				*index = cache->pos[i];
		}
		len += j;
	}
	return len;
}

static size_t
transform_char(fz_context *ctx, char *output, size_t *index, int c, size_t pos, fz_text_transform transform, transform_cache *cache)
{
	size_t len;
	int ccc;

	if (transform & FZ_TEXT_TRANSFORM_DECOMPOSE)
	{
		uint32_t a, b;
		if (ucdn_decompose((uint32_t)c, &a, &b))
		{
			size_t len2 = transform_char(ctx, output, index, a, pos, transform, cache);
			if (index)
				index += len2;
			if (output)
				output += len2;
			len = transform_char(ctx, output, index, b, pos, transform, cache);
			if (index)
				index += len;
			if (output)
				output += len;
			return len2 + len;
		}
		transform &= ~FZ_TEXT_TRANSFORM_DECOMPOSE;
		/* Otherwise, it doesn't decompose to anything, so just fall
		 * through and process it as is.
		 */
	}

	if (transform & FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE)
	{
		uint32_t decomp[18]; /* 1 char decomps to a maximum of 18. */
		int i, n = ucdn_compat_decompose((uint32_t)c, decomp);
		if (n > 0)
		{
			len = 0;
			for (i = 0; i < n; i++)
			{
				size_t char_len = transform_char(ctx, output, index, decomp[i], pos, transform, cache);
				if (index)
					index += char_len;
				if (output)
					output += char_len;
				len += char_len;
			}
			return len;
		}
		transform &= ~FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE;
		/* Otherwise, it doesn't decompose to anything, so just fall
		 * through and process it as is.
		 */
	}

	if (transform & FZ_TEXT_TRANSFORM_STRIP_MARKING_NONSPACING)
	{
		if (ucdn_get_general_category(c) == UCDN_GENERAL_CATEGORY_MN)
			return 0;
	}

	if (transform & FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII)
	{
		// Map full-width ASCII forms to ASCII:
		// U+FF01 .. U+FF5E => U+0021 .. U+007E
		if (c >= 0xFF01 && c <= 0xFF5E)
			c = c - 0xFF01 + 0x21;
	}

	if (transform & FZ_TEXT_TRANSFORM_NORMALIZE_HYPHENS)
		if (fz_is_unicode_hyphen(c))
			c = '-';

	if (transform & FZ_TEXT_TRANSFORM_UPPERCASE)
		c = fz_toupper(c);

	if (c == '\n' && (transform & (FZ_TEXT_TRANSFORM_KEEP_LINES | FZ_TEXT_TRANSFORM_KEEP_PARAGRAPHS)))
		{
		/* Don't strip \n if we might need it. */
		}
		else
		c = canon(c);

	/* Squash runs of spaces. */
	if (c == ' ' && cache->len > 0 && cache->cache[cache->len-1] == ' ')
		return 0;

	/* We keep a cache to allow for composition, and to remove runs of
	 * spaces.
	 */
	ccc = 0;
	if (transform & (FZ_TEXT_TRANSFORM_DECOMPOSE | FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE | FZ_TEXT_TRANSFORM_COMPOSE))
		ccc = ucdn_get_combining_class(c);
	/* If the cache is empty, we always store the character. */
	if (cache->len == 0)
	{
		cache->cache[cache->len] = c;
		cache->pos[cache->len] = pos;
		cache->len++;
		return 0;
		}
	/* Alternatively, if we've got a combining character, then we want to store that in the cache. */
	if (ccc > 0)
	{
		if (cache->len == nelem(cache->cache))
		{
			/* Cache is full. Just drop this character. This should never happen. */
			fz_warn(ctx, "Overfull unicode composition cache - character dropped");
			return 0;
		}
		cache->cache[cache->len] = c;
		cache->pos[cache->len] = pos;
		cache->len++;
		return 0;
	}
	/* So we've got a new base character. */

	/* Do we have a cache to flush? */
	len = flush_transform_cache(ctx, output, index, transform, cache);
	cache->len = 1;
	cache->cache[0] = c;
	cache->pos[0] = pos;
	return len;
}

static size_t
do_transform(fz_context *ctx, fz_text_transform transform, char *output, size_t *index, const char *input)
{
	size_t byte_len = 1; /* Allow for terminator */
	transform_cache cache;
	size_t pos = 0;
	size_t len;
	int c;

	cache.len = 0;
	while (1)
	{
		int inlen = fz_chartorune(&c, &input[pos]);

		if (c == 0)
			break;

		len = transform_char(ctx, output, index, c, pos, transform, &cache);
		if (index)
			index += len;
		if (output)
			output += len;
		byte_len += len;
		pos += inlen;
	}

	/* Flush the cache */
	len = flush_transform_cache(ctx, output, index, transform, &cache);
	if (index)
	{
		index += len;
		*index = pos;
	}
	if (output)
			{
		output += len;
		*output = 0;
	}
	byte_len += len;

	return byte_len;
}

static char *
transform_text_with_index(fz_context *ctx, fz_text_transform transform, const char *input, size_t **indexp)
				{
	char *output;
	size_t len = do_transform(ctx, transform, NULL, NULL, input);

	output = fz_malloc(ctx, len);
	fz_try(ctx)
					{
		*indexp = fz_malloc_array(ctx, len+1, size_t);
	}
	fz_catch(ctx)
						{
		fz_free(ctx, output);
		fz_rethrow(ctx);
						}

	(void)do_transform(ctx, transform, output, *indexp, input);

	return output;
					}

static char *
transform_text_without_index(fz_context *ctx, fz_text_transform transform, const char *input)
{
	char *output;
	size_t len = do_transform(ctx, transform, NULL, NULL, input);
	output = fz_malloc(ctx, len);
	(void)do_transform(ctx, transform, output, NULL, input);
	return output;
}

/*
 * Iterative search.
 */

typedef struct
{
	fz_stext_page *page;
	fz_stext_position *map;

	char *haystack;
	size_t length;
	size_t utf_length;

	char *spun_haystack;
	size_t spun_length;

	size_t *index;

	int seq;
	int end_of_doc;
} search_page;

struct fz_search
{
	fz_search_options options;

	/* transform and finder are derived from options */
	fz_text_transform transform;
	const fz_match_finder *finder;

	float hfuzz, vfuzz;

	/* match.quads is an array match_quads_max in size, with match.num_quads used. */
	int match_quads_max;
	fz_search_match match;

	char *needle;
	char *spun_needle;
	void *compiled_needle; /* for regexp */

	char *combined_spun_haystack;
	size_t combined_spun_split_1;
	size_t combined_spun_split_2;
	size_t combined_spun_length;

	/* The current offset within the combined spun haystack. */
	int is_current_spun_pos_valid;
	size_t current_spun_pos;

	/* < 0 if the last search was backwards, > 0 if forwards. 0 is the initial value before any searches. */
	int last_direction;
	int discard_next_match;

	int req_seq;
	search_page *req_target;

	search_page page[3];
};

#define PREVIOUS_PAGE 0
#define CURRENT_PAGE 1
#define NEXT_PAGE 2

static fz_text_transform
init_transform_and_finder(fz_search_options options, const fz_match_finder **finder)
{
	fz_text_transform trans = FZ_TEXT_TRANSFORM_NONE;

	if (options & FZ_SEARCH_KEEP_LINES)
		trans |= FZ_TEXT_TRANSFORM_KEEP_LINES;
	if (options & FZ_SEARCH_KEEP_PARAGRAPHS)
		trans |= FZ_TEXT_TRANSFORM_KEEP_PARAGRAPHS;
	if (options & FZ_SEARCH_KEEP_HYPHENS)
		trans |= FZ_TEXT_TRANSFORM_KEEP_HYPHENS;

	if (options & FZ_SEARCH_REGEXP)
	{
		if (options & FZ_SEARCH_IGNORE_CASE)
		{
			*finder = &regexp_finder_insensitive;
			options &= ~FZ_SEARCH_IGNORE_CASE;
		}
		else
			*finder = &regexp_finder;
	}
	else
	{
		*finder = &simple_finder;
	}

	switch (options & (FZ_SEARCH_IGNORE_CASE | FZ_SEARCH_IGNORE_DIACRITICS))
	{
	default:
	case FZ_SEARCH_EXACT:
		trans |= FZ_TEXT_TRANSFORM__NORMAL;
		break;
	case FZ_SEARCH_IGNORE_CASE:
		trans |= FZ_TEXT_TRANSFORM__IGNORE_CASE;
		break;
	case FZ_SEARCH_IGNORE_DIACRITICS:
		trans |= FZ_TEXT_TRANSFORM__IGNORE_DIACRITICS;
		break;
	case FZ_SEARCH_IGNORE_CASE | FZ_SEARCH_IGNORE_DIACRITICS:
		trans |= FZ_TEXT_TRANSFORM__IGNORE_CASE_DIACRITICS;
		break;
	}

	return trans;
}

fz_search *fz_new_search(fz_context *ctx, const char *needle, fz_search_options options)
{
	fz_search *search = fz_malloc_struct(ctx, fz_search);

	search->hfuzz = 0.5f; /* merge large gaps */
	search->vfuzz = 0.1f;

	/* for the initial page feed before we start requesting pages */
	search->req_seq = 0;
	search->req_target = NULL;

	if (needle == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search for a non-existent needle");

	search->transform = init_transform_and_finder(options, &search->finder);

	/* Make sure we have a copy of the needle. */
	search->needle = fz_strdup(ctx, needle);

	/* Spin that needle into gold (to match the haystack) */
	if (search->transform == FZ_TEXT_TRANSFORM_NONE)
		search->spun_needle = search->needle;
	else
		search->spun_needle = transform_text_without_index(ctx, search->transform, search->needle);

	/* Compile the needle if required. */
	if (search->finder->init)
		search->finder->init(ctx, &search->compiled_needle, search->spun_needle);

	return search;
}

static void
drop_needle(fz_context *ctx, fz_search *search)
{
	if (search->compiled_needle && search->finder->fin)
		search->finder->fin(ctx, &search->compiled_needle);
	search->compiled_needle = NULL;
	if (search->spun_needle != search->needle)
		fz_free(ctx, search->spun_needle);
	search->spun_needle = NULL;
	fz_free(ctx, search->needle);
	search->needle = NULL;
}

static void
drop_haystack(fz_context *ctx, search_page *page)
{
	fz_free(ctx, page->map);
	fz_free(ctx, page->haystack);
	if (page->haystack != page->spun_haystack)
		fz_free(ctx, page->spun_haystack);
	fz_free(ctx, page->index);

	page->map = NULL;
	page->haystack = NULL;
	page->spun_haystack = NULL;
	page->index = NULL;

	page->length = 0;
	page->utf_length = 0;
	page->spun_length = 0;
}

static void
drop_combined(fz_context *ctx, fz_search *search)
{
	fz_free(ctx, search->combined_spun_haystack);
	search->combined_spun_haystack = NULL;
	search->combined_spun_split_1 = 0;
	search->combined_spun_split_2 = 0;
	search->combined_spun_length = 0;
}

static void
zero_page(fz_context *ctx, search_page *page)
{
	page->page = NULL;
	page->map = NULL;
	page->haystack = NULL;
	page->length = 0;
	page->utf_length = 0;
	page->spun_haystack = NULL;
	page->spun_length = 0;
	page->index = NULL;
	page->seq = 0;
	page->end_of_doc = 0;
}

static void
drop_page(fz_context *ctx, search_page *page)
{
	fz_drop_stext_page(ctx, page->page);
	drop_haystack(ctx, page);
	zero_page(ctx, page);
}

static void
move_page(fz_context *ctx, fz_search *search, search_page *to, search_page *from)
{
	drop_page(ctx, to);
	memcpy(to, from, sizeof *to);
	zero_page(ctx, from);
}

static void
advance_page(fz_context *ctx, fz_search *search)
{
	drop_combined(ctx, search);
	move_page(ctx, search, &search->page[0], &search->page[1]);
	move_page(ctx, search, &search->page[1], &search->page[2]);
}

static void
retreat_page(fz_context *ctx, fz_search *search)
{
	drop_combined(ctx, search);
	move_page(ctx, search, &search->page[2], &search->page[1]);
	move_page(ctx, search, &search->page[1], &search->page[0]);
}

static void
get_haystack(fz_context *ctx, search_page *ss, fz_stext_page *page, fz_text_transform transform)
{
	fz_buffer *buffer;
	fz_text_flatten flatten = FZ_TEXT_FLATTEN_ALL;

	if (transform & FZ_TEXT_TRANSFORM_KEEP_LINES)
		flatten |= FZ_TEXT_FLATTEN_KEEP_LINES;
	if (transform & FZ_TEXT_TRANSFORM_KEEP_PARAGRAPHS)
		flatten |= FZ_TEXT_FLATTEN_KEEP_PARAGRAPHS;
	if (transform & FZ_TEXT_TRANSFORM_KEEP_HYPHENS)
		flatten |= FZ_TEXT_FLATTEN_KEEP_HYPHENS;

	buffer = fz_new_buffer_from_flattened_stext_page(ctx, page, flatten, &ss->map);
	fz_try(ctx)
	{
		fz_terminate_buffer(ctx, buffer);
		(void)fz_buffer_extract(ctx, buffer, (unsigned char **)&ss->haystack);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, ss->map);
		ss->map = NULL;
		fz_rethrow(ctx);
	}
}

static int
ensure_haystack_for_page(fz_context *ctx, fz_search *search, search_page *page)
{
	char *s;
	int c;

	if (page->end_of_doc)
	{
		return 0;
	}

	if (page->haystack == NULL)
	{
		get_haystack(ctx, page, page->page, search->transform);
		page->length = strlen(page->haystack);
		page->utf_length = fz_utflen(page->haystack);
	}

	/* skip initial whitespace looking for the first non-whitespace character */
	s = page->haystack;
	while (*s)
	{
		s += fz_chartorune(&c, s);
		if (!fz_is_unicode_whitespace(c))
			return 0;
	}

	/* no non-whitespace character found! */
	drop_haystack(ctx, page);
	return 1;
}

static void
ensure_spun_haystack_for_page(fz_context *ctx, fz_search *search, search_page *page)
{
	if (page->spun_haystack != NULL)
		return;

	assert(page->index == NULL);

	if (page->haystack == NULL)
	{
		page->spun_haystack = NULL;
		page->spun_length = 0;
		page->index = NULL;
	}
	else
	{
		if (search->transform == FZ_TEXT_TRANSFORM_NONE)
		{
			page->spun_haystack = page->haystack;
			page->spun_length = page->length;
			page->index = NULL;
		}
		else
		{
			page->spun_haystack = transform_text_with_index(ctx, search->transform, page->haystack, &page->index);
			page->spun_length = strlen(page->spun_haystack);
		}
	}

}

static void
ensure_combined_spun_haystack(fz_context *ctx, fz_search *search)
{
	search_page *p1 = &search->page[0];
	search_page *p2 = &search->page[1];
	search_page *p3 = &search->page[2];

	if (search->combined_spun_haystack != NULL)
		return;

	search->combined_spun_split_1 = p1->spun_length;
	search->combined_spun_split_2 = p1->spun_length + p2->spun_length;
	search->combined_spun_length = p1->spun_length + p2->spun_length + p3->spun_length;

	search->combined_spun_haystack = fz_malloc(ctx, search->combined_spun_length + 1);
	if (p1->spun_length)
		memcpy(search->combined_spun_haystack, p1->spun_haystack, p1->spun_length);
	if (p2->spun_length)
		memcpy(search->combined_spun_haystack + p1->spun_length, p2->spun_haystack, p2->spun_length);
	if (p3->spun_length)
		memcpy(search->combined_spun_haystack + p1->spun_length + p2->spun_length, p3->spun_haystack, p3->spun_length);
	search->combined_spun_haystack[search->combined_spun_length] = 0;
}

static fz_stext_position
lookup_stext_position(fz_context *ctx, fz_search *search, const char *spun)
{
	search_page *page1 = &search->page[0];
	search_page *page2 = &search->page[1];
	search_page *page3 = &search->page[2];
	search_page *page;
	size_t spun_ix;
	size_t page_spun_ix;
	size_t page_unspun_ix;
	size_t utf_ix;

	assert(spun >= search->combined_spun_haystack && spun < search->combined_spun_haystack + search->combined_spun_length);

	spun_ix = spun - search->combined_spun_haystack;
	assert(spun_ix >= 0 && spun_ix < search->combined_spun_length);

	if (spun_ix < search->combined_spun_split_1)
	{
		page = page1;
		page_spun_ix = spun_ix;
	}
	else if (spun_ix < search->combined_spun_split_2)
	{
		page = page2;
		page_spun_ix = spun_ix - search->combined_spun_split_1;
	}
	else
	{
		page = page3;
		page_spun_ix = spun_ix - search->combined_spun_split_2;
	}
	assert(page_spun_ix >= 0 && page_spun_ix < page->spun_length);

	page_unspun_ix = page->index ? page->index[page_spun_ix] : page_spun_ix;
	assert(page_unspun_ix >= 0 && page_unspun_ix < page->length);

	utf_ix = fz_runeidx(page->haystack, page->haystack + page_unspun_ix);

	return page->map[utf_ix];
}

static fz_stext_position
lookup_stext_position_end(fz_context *ctx, fz_search *search, const char *spun)
{
	fz_stext_position pos = lookup_stext_position(ctx, search, spun);
	while (pos.ch && pos.ch->next && ucdn_get_general_category(pos.ch->next->c) == UCDN_GENERAL_CATEGORY_MN)
		pos.ch = pos.ch->next;
	return pos;
}

static int
lookup_stext_seq(fz_search *search, fz_stext_position *pos)
{
	if (pos->page == search->page[0].page) return search->page[0].seq;
	if (pos->page == search->page[1].page) return search->page[1].seq;
	if (pos->page == search->page[2].page) return search->page[2].seq;
	return -1;
}

static void add_quad(fz_context *ctx, fz_search *search, fz_stext_position *pos, int seq)
{
	fz_stext_line *line = pos->line;
	fz_stext_char *ch = pos->ch;
	float vfuzz = ch->size * search->vfuzz;
	float hfuzz = ch->size * search->hfuzz;

	/* Can we merge this quad into the last one we already have? */
	if (search->match.num_quads > 0)
	{
		fz_search_quad *end = &search->match.quads[search->match.num_quads-1];
		if (end->seq == seq
			&& hdist(&line->dir, &end->quad.lr, &ch->quad.ll) < hfuzz
			&& vdist(&line->dir, &end->quad.lr, &ch->quad.ll) < vfuzz
			&& hdist(&line->dir, &end->quad.ur, &ch->quad.ul) < hfuzz
			&& vdist(&line->dir, &end->quad.ur, &ch->quad.ul) < vfuzz)
		{
			/* Yes */
			end->quad.ur = ch->quad.ur;
			end->quad.lr = ch->quad.lr;
			return;
		}
	}

	if (search->match.num_quads == search->match_quads_max)
	{
		int newmax = search->match_quads_max * 2;
		if (newmax == 0)
			newmax = 32;
		search->match.quads = fz_realloc(ctx, search->match.quads, sizeof(search->match.quads[0]) * newmax);
		search->match_quads_max = newmax;
	}
	search->match.quads[search->match.num_quads].seq = seq;
	search->match.quads[search->match.num_quads++].quad = ch->quad;
}

static fz_search_result result_complete = { FZ_SEARCH_COMPLETE };

static fz_search_result
request_page(fz_context *ctx, fz_search *search, int seq, search_page *page)
{
	fz_search_result result;
	search->req_seq = seq;
	search->req_target = page;
	result.reason = FZ_SEARCH_MORE_INPUT;
	result.u.seq_needed = seq;
	return result;
}

static fz_search_result
fz_search_imp(fz_context *ctx, fz_search *search, int direction)
{
	fz_search_result result;
	const char *spun_begin, *spun_end, *p;
	int seq;

	if (search == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search in a non-existent search");
	if (direction != -1 && direction != 1)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid search direction (must be 1 or -1)");
	if (direction < 0 && search->finder->find_rev == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search backwards with this finder");

	if (search->needle == NULL || *search->needle == 0)
		return result_complete;

	/* On entry, we assume that current_spun_pos point a position within
	 * the combined spun haystack if it exists. We may have ditched the
	 * combined spun haystack, but the position within that buffer, when
	 * reformed, should be valid.
	 */
	assert(search->combined_spun_haystack == NULL || search->current_spun_pos <= search->combined_spun_length);

	/* Was the last search in a different direction? If so, we need to discard the duplicate result that we'll find. */
	if (search->last_direction && search->last_direction != direction)
		search->discard_next_match = 1;
	search->last_direction = direction;

restart:

	/* We need to have a current page. */
	if (search->page[CURRENT_PAGE].page == NULL)
	{
		if (search->page[CURRENT_PAGE].end_of_doc)
			return result_complete;
		return request_page(ctx, search, 0, &search->page[CURRENT_PAGE]); /* Any seq will do! */
	}

	/* If the current page is empty (and it's not the final page) */
	if (search->page[CURRENT_PAGE].haystack == NULL && !search->page[CURRENT_PAGE].end_of_doc)
	{
		/* Ditch current page, then ask for the next page to take its place. */
		seq = search->page[CURRENT_PAGE].seq + direction;
		drop_page(ctx, &search->page[CURRENT_PAGE]);
		return request_page(ctx, search, seq, &search->page[CURRENT_PAGE]);
	}

	/* We need to either have the next page, or to know there is no next page. */
	if (search->page[CURRENT_PAGE+direction].page == NULL && !search->page[CURRENT_PAGE+direction].end_of_doc)
	{
		return request_page(ctx, search, search->page[CURRENT_PAGE].seq + direction, &search->page[CURRENT_PAGE+direction]);
	}

	/* The next page was empty (and not the last page) */
	if (!search->page[CURRENT_PAGE+direction].end_of_doc && search->page[CURRENT_PAGE+direction].haystack == NULL)
	{
		/* Ditch the next page, then ask for the next-next page to take its place. */
		seq = search->page[CURRENT_PAGE+direction].seq + direction;
		drop_page(ctx, &search->page[CURRENT_PAGE+direction]);
		return request_page(ctx, search, seq, &search->page[CURRENT_PAGE+direction]);
	}

	/* Then spin all the hay (for all the loaded pages) into gold. */
	ensure_combined_spun_haystack(ctx, search);

	/* Where are we searching from? */
	if (!search->is_current_spun_pos_valid)
	{
		/* Search from the first or last char on the current page depending on direction. */
		search->is_current_spun_pos_valid = 1;
		search->current_spun_pos = (direction > 0) ? search->combined_spun_split_1 : search->combined_spun_split_2;
	}
	else
	{
		/* Forwards search from the char before where we matched before. */
		/* Backwards search from the char after the end of the previous match. */
	}

	/* If we're searching forwards and the current position is on the
	 * next page, we need to shift the window so that we can find
	 * matches that straddle the page boundary of the next and
	 * next-next page (current and next page after shifting).
	 */
	if (direction > 0 && search->current_spun_pos >= search->combined_spun_split_2)
	{
		/* Note: We adjust the current position by subtracting the previous page's length,
		 * so that it remains at the same relative position after shifting the window.
		 */
		search->current_spun_pos -= search->combined_spun_split_1;
		advance_page(ctx, search);
		goto restart;
	}

	/* If we're searching backwards and the current position is on the
	 * previous page, we need to shift the window so that we can find
	 * matches that straddle the page boundary of the previous-previous and
	 * previous page (previous and current page after shifting).
	 */
	if (direction < 0 && search->current_spun_pos < search->combined_spun_split_1)
	{
		/* Note: the current position will be updated when fz_feed_search receives the page. */
		retreat_page(ctx, search);
		goto restart;
	}

	/* Assert that our offsets make sense. */
	assert(search->combined_spun_split_1 <= search->combined_spun_length);
	assert(search->combined_spun_split_2 <= search->combined_spun_length);
	if (direction > 0) assert(search->current_spun_pos <= search->combined_spun_split_2);
	if (direction < 0) assert(search->current_spun_pos >= search->combined_spun_split_1);

	/* Search within the combined_spun_haystack onwards from current_spun_pos. */
	spun_begin = (direction > 0 ? search->finder->find : search->finder->find_rev)(ctx,
		&search->compiled_needle,
		search->combined_spun_haystack,
		search->combined_spun_haystack + search->current_spun_pos,
		search->spun_needle,
		&spun_end
	);

	if (spun_begin && spun_end)
	{
		/* We found a match! spun_begin points to first char of match, spun_end points to first char after match */

		if (direction > 0)
		{
			/* Next search should start after last character of this match. */
			search->current_spun_pos = spun_end - search->combined_spun_haystack;
		}
		else
		{
			/* Next search should start at first character of this match. */
			search->current_spun_pos = spun_begin - search->combined_spun_haystack;
		}

		/* We need to discard the match, because we changed direction. */
		if (search->discard_next_match)
		{
			search->discard_next_match = 0;
			goto restart;
		}

		/* spun_begin and spun_end must now point to first and last characters of match */

		spun_end = retreat_one_utf8(search->combined_spun_haystack, spun_end);

		/* Skip over spun characters that have no matching stext character (inserted line ends, removed spaces, etc) */
		while (spun_begin < spun_end && lookup_stext_position(ctx, search, spun_begin).ch == NULL)
			spun_begin = advance_one_utf8(search->combined_spun_haystack, spun_begin);
		while (spun_end > spun_begin && lookup_stext_position(ctx, search, spun_end).ch == NULL)
			spun_end = retreat_one_utf8(search->combined_spun_haystack, spun_end);

		/* Remember the location of the match. */
		search->match.begin = lookup_stext_position(ctx, search, spun_begin);
		search->match.end = lookup_stext_position_end(ctx, search, spun_end);

		/* Skip matches that consist of synthetic characters */
		// TODO: We could create a fake fz_stext_char to return the synthetic space here.
		// However, there's not much utility to be had from being able
		// to search for single space characters that stand in for line
		// and paragraph breaks.
		if (!search->match.begin.ch || !search->match.end.ch)
			goto restart;

		search->match.begin_seq = lookup_stext_seq(search, &search->match.begin);
		search->match.end_seq = lookup_stext_seq(search, &search->match.end);

		/* Gather the quads for the result hit. */
		search->match.num_quads = 0;
		for (p = spun_begin; p <= spun_end; p = advance_one_utf8(search->combined_spun_haystack, p))
		{
			fz_stext_position pos = lookup_stext_position(ctx, search, p);
			if (pos.ch != NULL)
			{
				size_t spun_ix = p - search->combined_spun_haystack;
				if (spun_ix < search->combined_spun_split_1)
					seq = search->page[PREVIOUS_PAGE].seq;
				else if (spun_ix < search->combined_spun_split_2)
					seq = search->page[CURRENT_PAGE].seq;
				else
					seq = search->page[NEXT_PAGE].seq;
				add_quad(ctx, search, &pos, seq);
			}
		}

		result.reason = FZ_SEARCH_MATCH;
		result.u.match = &search->match;
		return result;
	}

	/* We failed to match. */

	/* Start any future searches from the next page (no current position) */
	search->is_current_spun_pos_valid = 0;

	/* End of doc? We're done! */
	if (search->page[CURRENT_PAGE+direction].end_of_doc)
		return result_complete;

	/*
	 * There may still be a match straddling the next page and the page after that.
	 * We slide the window one page in the search direction, and start searching anew
	 * on the next page.
	 */
	if (direction > 0)
		advance_page(ctx, search);
	else
		retreat_page(ctx, search);

	return request_page(ctx, search, search->page[CURRENT_PAGE].seq + direction, &search->page[CURRENT_PAGE+direction]);
}

fz_search_result fz_search_forwards(fz_context *ctx, fz_search *search)
{
	return fz_search_imp(ctx, search, 1);
}

fz_search_result fz_search_backwards(fz_context *ctx, fz_search *search)
{
	return fz_search_imp(ctx, search, -1);
}

void fz_feed_search(fz_context *ctx, fz_search *search, fz_stext_page *page, int seq)
{
	search_page *pg;

	if (search == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't feed a non-existent search");

	if (search->req_target == NULL)
	{
		/* we are force-fed the first page to start searching on */
		search->req_target = &search->page[CURRENT_PAGE];
	}
	else
	{
		/* we have requested a page! make sure we get the one we asked for. */
		if (search->req_seq != seq)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Not the requested page (requested %d, got %d)", search->req_seq, seq);
	}

	pg = search->req_target;

	drop_page(ctx, pg);

	pg->page = page;
	pg->seq = seq;
	pg->end_of_doc = (page == NULL);

	/* Prepare a haystack for the page. */
	if (ensure_haystack_for_page(ctx, search, pg))
	{
		/* The page is empty! */
		return;
	}

	/* We also need the page to have a spun haystack. */
	ensure_spun_haystack_for_page(ctx, search, pg);

	/* Update current spun pos now that we know the length of the previous page. */
	if (search->is_current_spun_pos_valid && search->req_target == &search->page[PREVIOUS_PAGE])
	{
		search->current_spun_pos += search->page[PREVIOUS_PAGE].spun_length;
	}

	/* We have a new page, so the combined haystack content is invalid */
	drop_combined(ctx, search);
}

void fz_drop_search(fz_context *ctx, fz_search *search)
{
	if (search == NULL)
		return;

	drop_page(ctx, &search->page[CURRENT_PAGE]);
	drop_page(ctx, &search->page[PREVIOUS_PAGE]);
	drop_page(ctx, &search->page[NEXT_PAGE]);
	drop_combined(ctx, search);
	drop_needle(ctx, search);
	fz_free(ctx, search->match.quads);
	fz_free(ctx, search);
}

/*
 * Non-iterative search functions (using iterative API internally).
 */

int
fz_match_stext_page_cb(fz_context *ctx, fz_stext_page *page, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options)
{
	fz_search *search = fz_new_search(ctx, needle, options);
	fz_search_result res;
	fz_quad *quads = NULL;
	int quad_max = 0;
	int hits = 0;

	fz_var(search);
	fz_var(quads);

	fz_try(ctx)
	{
		fz_feed_search(ctx, search, fz_keep_stext_page(ctx, page), 0);

		do
		{
			res = fz_search_forwards(ctx, search);
			if (res.reason == FZ_SEARCH_MORE_INPUT)
			{
				fz_feed_search(ctx, search, NULL, res.u.seq_needed);
			}
			else if (res.reason == FZ_SEARCH_MATCH)
			{
				fz_search_match *details = res.u.match;
				if (cb != NULL && details->num_quads > 0)
				{
					int i;
					if (details->num_quads > quad_max)
					{
						quads = fz_realloc(ctx, quads, sizeof(*quads) * details->num_quads);
						quad_max = details->num_quads;
					}
					for (i = 0; i < details->num_quads; i++)
						quads[i] = details->quads[i].quad;
					cb(ctx, opaque, details->num_quads, quads, 0, 0);
				}
				hits++;
			}
		}
		while (res.reason != FZ_SEARCH_COMPLETE);
	}
	fz_always(ctx)
	{
		fz_free(ctx, quads);
		fz_drop_search(ctx, search);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return hits;
}

typedef struct
{
	fz_search_callback_fn *cb;
	void *opaque;
} match2search_data;

static int match2search(fz_context *ctx, void *opaque, int num_quads, fz_quad *hit_bbox, int chapter, int page)
{
	match2search_data *md = (match2search_data *)opaque;

	return md->cb(ctx, md->opaque, num_quads, hit_bbox);
}

int
fz_search_stext_page_cb(fz_context *ctx, fz_stext_page *page, const char *needle, fz_search_callback_fn *cb, void *opaque)
{
	match2search_data md = { cb, opaque };

	return fz_match_stext_page_cb(ctx, page, needle, match2search, &md, FZ_SEARCH_IGNORE_CASE);
}

typedef struct
{
	int *hit_mark;
	fz_quad *quads;
	int max_quads;
	int fill;
	int hit;
} oldsearch_data;

static int
oldsearch_cb(fz_context *ctx, void *opaque, int num_quads, fz_quad *quads)
{
	oldsearch_data *data = (oldsearch_data *)opaque;
	int i;
	int hit = data->hit++;

	for (i = 0; i < num_quads; i++)
	{
		if (data->fill == data->max_quads)
			break;
		if (data->hit_mark)
			data->hit_mark[data->fill] = hit;
		data->quads[data->fill] = quads[i];
		data->fill++;
	}

	/* We never return 1 here, even if we fill up the buffer, as we
	 * want the old API to get the correct total number of quads.
	 */
	return 0;
}

int
fz_search_stext_page(fz_context *ctx, fz_stext_page *page, const char *needle, int *hit_mark, fz_quad *quads, int max_quads)
{
	oldsearch_data data;
	match2search_data md = { oldsearch_cb, &data };

	data.hit_mark = hit_mark;
	data.quads = quads;
	data.max_quads = max_quads;
	data.fill = 0;
	data.hit = 0;
	(void)fz_match_stext_page_cb(ctx, page, needle, match2search, &md, FZ_SEARCH_IGNORE_CASE);

	return data.fill; /* Return the number of quads we have read */
}

int
fz_match_stext_page(fz_context *ctx, fz_stext_page *page, const char *needle, int *hit_mark, fz_quad *quads, int max_quads, fz_search_options options)
{
	oldsearch_data data;
	match2search_data md = { oldsearch_cb, &data };

	data.hit_mark = hit_mark;
	data.quads = quads;
	data.max_quads = max_quads;
	data.fill = 0;
	data.hit = 0;

	(void)fz_match_stext_page_cb(ctx, page, needle, match2search, &md, options);

	return data.fill; /* Return the number of quads we have read */
}
