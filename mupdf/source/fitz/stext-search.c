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

/* SumatraPDF: different location */
#include "../../../ext/mujs/regexp.h"

#include "mupdf/ucdn.h"

#include <string.h>
#include <limits.h>
#include <assert.h>

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

fz_search_options fz_parse_search_options(const char *args)
{
	fz_search_options mask = 0;
	// TODO: stricter parsing of options bitmask string
	if (strstr(args, "exact")) mask |= FZ_SEARCH_EXACT;
	if (strstr(args, "ignore-case")) mask |= FZ_SEARCH_IGNORE_CASE;
	if (strstr(args, "ignore-diacritics")) mask |= FZ_SEARCH_IGNORE_DIACRITICS;
	if (strstr(args, "regexp")) mask |= FZ_SEARCH_REGEXP;
	if (strstr(args, "keep-lines")) mask |= FZ_SEARCH_KEEP_LINES;
	if (strstr(args, "keep-paragraphs")) mask |= FZ_SEARCH_KEEP_PARAGRAPHS;
	if (strstr(args, "keep-hyphens")) mask |= FZ_SEARCH_KEEP_HYPHENS;
	return mask;
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

static inline int canon(int c)
{
	if (c == '\r' || c == '\n' || c == '\t' || fz_is_unicode_space_equivalent(c))
		return ' ';
	return c;
}

static const char *match_exact(const char *h, const char *n)
{
	int hc, nc;
	h += fz_chartorune(&hc, h);
	n += fz_chartorune(&nc, n);
	while (hc == nc)
	{
		n += fz_chartorune(&nc, n);
		if (nc == 0)
			return h;
		h += fz_chartorune(&hc, h);
	}
	return NULL;
}

static const char *find_exact(fz_context *ctx, void *dummy, const char *s, const char *needle, const char **endp, int bol)
{
	const char *end;
	int c;
	while (*s)
	{
		end = match_exact(s, needle);
		if (end)
			return *endp = end, s;
		s += fz_chartorune(&c, s);
	}
	return *endp = NULL, NULL;
}

static const char *find_rev_exact(fz_context *ctx, void *dummy, const char *start, size_t offset, const char *needle, const char **endp, int bol)
{
	const char *end;
	int idx = fz_runeidx(start, &start[offset]);

	while (1)
	{
		end = match_exact(&start[offset], needle);
		if (end)
			return *endp = end, &start[offset];
		if (idx == 0)
			break;
		idx--;
		offset = fz_runeptr(start, idx) - start;
	}
	return *endp = NULL, NULL;
}

static const char *find_regexp(fz_context *ctx, void *arg, const char *s, const char *needle, const char **endp, int bol)
{
	Reprog **prog = (Reprog **)arg;
	Resub m;
	int result = js_regexec(*prog, s, &m, bol ? 0 : REG_NOTBOL);
	if (result < 0)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "regexec failure");
	if (result == 0)
		return *endp = m.sub[0].ep, m.sub[0].sp;
	return *endp = NULL, NULL;
}

static const char *find_rev_regexp(fz_context *ctx, void *arg, const char *s, size_t offset, const char *needle, const char **endp, int bol)
{
	/* This is pretty horrible. We search from the start for a match; if we fail to find it, no match.
	 * If we match, but the match starts after our offset, no match.
	 * Otherwise, we keep looking for matches further on in the buffer.
	*/
	const char *start, *later_end, *later_start;

	start = find_regexp(ctx, arg, s, needle, endp, bol);
	if (start == NULL || (size_t)(*endp - s) >= offset)
{
		/* No match at all. */
		*endp = NULL;
		return NULL;
	}

	/* Now look for a later one */
	while (1)
	{
		later_start = find_regexp(ctx, arg, *endp, needle, &later_end, bol);
		if (later_start == NULL || (size_t)(later_end - s) >= offset)
			return start;
		start = later_start;
		*endp = later_end;
	}
}

typedef const char *(fz_match_finder_fn)(void *find_arg, const char *haystack, const char *needle, const char **endp, int bol);

/**
	A set of functions to implement a 'finder' for the match
	functions.
*/
typedef struct
{
	/* init: May be NULL. If not, it will be called with the needle
	 * that is to be used. This allows the needle to be 'compiled'.
	 * This should throw if the needle fails to compile. */
	void (*init)(fz_context *ctx, void *find_arg, const char *needle);

	/* find: Search for the needle in the haystack.
	 *
	 * bol = 1 if at beginning of line, 0 otherwise.
	 *
	 * Return value is NULL (and *endp = NULL) if no match found.
	 * Otherwise, return value is the start of the match to the
	 * needle in the haystack (and *endp = the end of the match).
	 */
	const char *(*find)(fz_context *ctx, void *find_arg, const char *haystack, const char *needle, const char **endp, int bol);

	/* find_rev: Search for the needle backwards in the haystack.
	 *
	 * bol = 1 if at beginning of line, 0 otherwise.
	 *
	 * Return value is NULL (and *endp = NULL) if no match found.
	 * Otherwise, return value is the start of the match to the
	 * needle in the haystack (and *endp = the end of the match).
	 */
	const char *(*find_rev)(fz_context *ctx, void *find_arg, const char *haystack, size_t offset, const char *needle, const char **endp, int bol);

	/* fin: Maybe NULL. If not, it will be called whenever the
	 * search process has finished (after a successful init).
	 * This allows the 'compiled' needle to be freed. Must
	 * never throw. */
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
	 * according to Unicode tr15) */
	FZ_TEXT_TRANSFORM_DECOMPOSE = 16,

	/* Perform unicode compatibility decomposition on the string.
	 * (i.e. NFKD according to Unicode tr15) */
	FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE = 32,

	/* Preserve line breaks (mainly for use with regexps) -
	 * line breaks are preserved as single \n. */
	FZ_TEXT_TRANSFORM_KEEP_LINES = 256,

	/* Preserve paragraph breaks (mainly for use with regexps) -
	 * paragraph breaks are preserved as double \n. */
	FZ_TEXT_TRANSFORM_KEEP_PARAGRAPHS = 512,

	/* Preserve hyphens. Without this, they are removed and
	 * lines joined. */
	FZ_TEXT_TRANSFORM_KEEP_HYPHENS = 1024,


	/* And some useful combinations of these flags */

	/* NORMAL: case sensitive text matching, allowing for different
	 * encodings of the same diacritics. */
	FZ_TEXT_TRANSFORM__NORMAL = FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
					FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
					FZ_TEXT_TRANSFORM_COMPOSE,

	/* IGNORE_CASE: flags necessary for case insensitive matching */
	FZ_TEXT_TRANSFORM__IGNORE_CASE = FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
					FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
					FZ_TEXT_TRANSFORM_UPPERCASE |
					FZ_TEXT_TRANSFORM_COMPOSE,

	/* IGNORE_DIACRITICS: flags necessary to ignore marking non-spacing characters (i.e. diacritics) */
	FZ_TEXT_TRANSFORM__IGNORE_DIACRITICS = FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
					FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
					FZ_TEXT_TRANSFORM_STRIP_MARKING_NONSPACING |
					FZ_TEXT_TRANSFORM_COMPOSE,

	/* IGNORE_CASE_DIACRITICS: flags necessary to ignore both case and marking non-spacing characters (i.e. diacritics) */
	FZ_TEXT_TRANSFORM__IGNORE_CASE_DIACRITICS = FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE |
					FZ_TEXT_TRANSFORM_FULLWIDTH_ASCII |
					FZ_TEXT_TRANSFORM_STRIP_MARKING_NONSPACING |
					FZ_TEXT_TRANSFORM_UPPERCASE |
					FZ_TEXT_TRANSFORM_COMPOSE,
} fz_text_transform;

/* Given that the maximum we can decompose from a single char is
 * 18, it seems reasonable to only be able compose the same number?
 * Anyway, 32 should be WAY more than is ever needed in the real
 * world. */
typedef struct
{
	int len;
	int cache[32];
} transform_cache;

static size_t
flush_transform_cache(fz_context *ctx, char *output, fz_text_transform transform, transform_cache *cache)
{
	int i, j, n = cache->len;
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
			}
		}
		n = i+1;
}

	/* Now actually do the output */
	len = 0;
	if (output)
	{
		for (i = 0; i < n; i++)
		{
			j = fz_runetochar(output, cache->cache[i]);
			output += j;
			len += j;
		}
	}
	else
	{
		for (i = 0; i < n; i++)
			len += fz_runelen(cache->cache[i]);
	}

	return len;
}

static size_t
transform_char(fz_context *ctx, char *output, int c, fz_text_transform transform, transform_cache *cache)
{
	size_t len;
	int ccc;

	if (transform & FZ_TEXT_TRANSFORM_DECOMPOSE)
	{
		uint32_t a, b;
		if (ucdn_decompose((uint32_t)c, &a, &b))
		{
			size_t len2 = transform_char(ctx, output, a, transform, cache);
			if (output)
				output += len2;
			len = transform_char(ctx, output, b, transform, cache);
			if (output)
				output += len;
			return len2 + len;
		}
		transform &= ~FZ_TEXT_TRANSFORM_DECOMPOSE;
		/* Otherwise, it doesn't decompose to anything, so just fall
		 * through and process it as is. */
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
				size_t char_len = transform_char(ctx, output, decomp[i], transform, cache);
				if (output)
					output += char_len;
				len += char_len;
			}
			return len;
		}
		transform &= ~FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE;
		/* Otherwise, it doesn't decompose to anything, so just fall
		 * through and process it as is. */
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
	 * spaces. */
	ccc = 0;
	if (transform & (FZ_TEXT_TRANSFORM_DECOMPOSE | FZ_TEXT_TRANSFORM_COMPATIBILITY_DECOMPOSE | FZ_TEXT_TRANSFORM_COMPOSE))
		ccc = ucdn_get_combining_class(c);
	/* If the cache is empty, we always store the character. */
	if (cache->len == 0)
		{
		cache->cache[cache->len++] = c;
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
		cache->cache[cache->len++] = c;
		return 0;
	}
	/* So we've got a new base character. */

	/* Do we have a cache to flush? */
	len = flush_transform_cache(ctx, output, transform, cache);
	cache->len = 1;
	cache->cache[0] = c;
	return len;
}

static size_t
do_transform(fz_context *ctx, fz_text_transform transform, char *output, size_t *index, const char *input)
{
	size_t byte_len = 1; /* Allow for terminator */
	transform_cache cache;
	size_t pos = 0, last_pos = 0;
	size_t len;

	cache.len = 0;
	while (1)
	{
		int c;
		int inlen = fz_chartorune(&c, &input[pos]);

		if (c == 0)
			break;

		len = transform_char(ctx, output, c, transform, &cache);
		if (index)
	{
			size_t i;
			for (i = 0; i < len; i++)
				*index++ = last_pos;
		}
		if (output)
			output += len;
		byte_len += len;
		last_pos = pos;
		pos += inlen;
	}

	/* Flush the cache */
	len = flush_transform_cache(ctx, output, transform, &cache);
	if (index)
		{
		size_t i;
		for (i = 0; i < len; i++)
			*index++ = last_pos++;
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

int
fz_match_stext_page_cb(fz_context *ctx, fz_stext_page *page, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options)
{
	fz_search *search = fz_new_search(ctx);
	fz_search_result res;
	fz_quad *quads = NULL;
	int quad_max = 0;
	int hits = 0;

	fz_var(search);
	fz_var(quads);

	fz_try(ctx)
					{
		fz_search_set_options(ctx, search, options, needle);

		fz_feed_search(ctx, search, fz_keep_stext_page(ctx, page), 0);

		do
		{
			res = fz_search_forwards(ctx, search);
			if (res.reason == FZ_SEARCH_MORE_INPUT)
						{
				fz_feed_search(ctx, search, NULL, res.u.more_input.seq_needed);
						}
			else if (res.reason == FZ_SEARCH_MATCH)
					{
				fz_search_result_details *details = res.u.match.result;
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
					cb(ctx, opaque, details->num_quads, quads, details->quads[i].chapter_num, details->quads[i].page_num);
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
	 * want the old API to get the correct total number of quads. */
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

/* New iterative API */

typedef struct
{
	fz_stext_page *page;
	fz_stext_position *map;
	char *haystack;
	size_t length;
	size_t utf_length;
	int seq;
	int end_of_doc;
} search_page;

struct fz_search
{
	fz_search_options options;

	/* transform and finder are derived from options */
	fz_text_transform transform;
	const fz_match_finder *finder;

	int count_quads; /* The number of unmerged quads that got merged into the quads array. */
	float hfuzz, vfuzz;

	/* details.quads is an array quads_max in size, with details.num_quads used. */
	int quads_max;
	fz_search_result_details details;

	char *needle;
	char *spun_needle;
	size_t *spun_needle_index;
	void *compiled_needle;

	char *combined_haystack;
	size_t combined_split;
	size_t combined_utf_split;
	size_t combined_length;
	size_t *combined_index;
	char *combined_spun_haystack;
	size_t combined_spun_split;
	size_t combined_spun_length;

	int first;

	/* 0 if the last search was forwards, 1 if backwards. */
	int backwards;

	/* The current offset within the combined spun haystack. */
	size_t current_spun_pos;
	/* The current offset within the combined haystack. */
	size_t current_pos;
	int bol;

	search_page current_page;
	search_page previous_page;
	search_page next_page;
};

fz_search *fz_new_search(fz_context *ctx)
{
	fz_search *search = fz_malloc_struct(ctx, fz_search);

	search->first = 1;
	search->hfuzz = 0.5f; /* merge large gaps */
	search->vfuzz = 0.1f;

	return search;
}

static void
drop_needle(fz_context *ctx, fz_search *search)
{
	if (search->compiled_needle && search->finder->fin)
		search->finder->fin(ctx, &search->compiled_needle);
	search->compiled_needle = NULL;
	fz_free(ctx, search->spun_needle_index);
	search->spun_needle_index = NULL;
	fz_free(ctx, search->spun_needle);
	search->spun_needle = NULL;
	fz_free(ctx, search->needle);
	search->needle = NULL;
}

static void
drop_haystack(fz_context *ctx, search_page *page)
{
	fz_free(ctx, page->map);
	page->map = NULL;
	fz_free(ctx, page->haystack);
	page->haystack = NULL;
}

static void
drop_current_pos(fz_search *search)
{
	search->first = 1;
	search->current_spun_pos = 0;
	search->current_pos = 0;
}

static void
drop_page(fz_context *ctx, search_page *page)
{
	if (page->page)
		fz_drop_stext_page(ctx, page->page);
	drop_haystack(ctx, page);
	page->page = NULL;
}

static void
drop_combined(fz_context *ctx, fz_search *search)
{
	fz_free(ctx, search->combined_index);
	search->combined_index = NULL;
	fz_free(ctx, search->combined_haystack);
	search->combined_haystack = NULL;
	fz_free(ctx, search->combined_spun_haystack);
	search->combined_spun_haystack = NULL;
	search->combined_split = 0;
	search->combined_utf_split = 0;
	search->combined_spun_split = 0;
	search->combined_length = 0;
	search->combined_spun_length = 0;
}

static fz_text_transform
split_options(fz_search_options options, const fz_match_finder **finder)
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

void fz_search_set_options(fz_context *ctx, fz_search *search, fz_search_options options, const char *needle)
{
	fz_text_transform transform = FZ_TEXT_TRANSFORM_NONE;
	const fz_match_finder *finder;

	if (search == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't set options in a non-existent search");
	if (needle == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search for a non-existent needle");

	transform = split_options(options, &finder);

	if (transform != search->transform)
	{
		/* We need to ditch the needle and the stored haystacks. */
		drop_needle(ctx, search);
		drop_haystack(ctx, &search->current_page);
		drop_haystack(ctx, &search->previous_page);
		drop_haystack(ctx, &search->next_page);
		drop_combined(ctx, search);
		search->transform = transform;
	}
	/* A change in the finder might invalidate the needle. */
	if (search->finder != finder && search->finder && search->finder->fin)
	{
		drop_needle(ctx, search);
		search->finder = NULL;
	}

	if (search->needle != NULL && strcmp(search->needle, needle) != 0)
	{
		/* Search needle has changed. */
		drop_needle(ctx, search);
	}

	/* Make sure we have a copy of the needle. */
	if (search->needle == NULL)
		search->needle = fz_strdup(ctx, needle);

	/* Spin that needle into gold (to match the haystack), and compile it if required. */
	if (search->spun_needle == NULL)
	{
		search->spun_needle = transform_text_with_index(ctx, transform, needle, &search->spun_needle_index);
	}
	if (search->finder == NULL)
	{
		search->finder = finder;
		if (search->finder->init)
			search->finder->init(ctx, &search->compiled_needle, search->spun_needle);
	}
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
		assert(&search->current_page != page);
		return 0;
	}

	if (page->haystack == NULL)
		get_haystack(ctx, page, page->page, search->transform);
	page->length = strlen(page->haystack);
	page->utf_length = fz_utflen(page->haystack);

	/* skip initial whitespace looking for the first non-whitespace character */
	s = page->haystack;
	while (*s)
	{
		s += fz_chartorune(&c, s);
		if (!fz_is_unicode_whitespace(c))
			return 0;
	}

	/* no non-whitespace character found! */
	return 1;
}

static void add_quad(fz_context *ctx, fz_search *search, fz_stext_position *pos, int seq)
{
	fz_stext_page *page = pos->page;
	fz_stext_block *block = pos->block;
	fz_stext_line *line = pos->line;
	fz_stext_char *ch = pos->ch;
	float vfuzz = ch->size * search->vfuzz;
	float hfuzz = ch->size * search->hfuzz;
	int chapter_num = 0;
	int page_num = 0;
	fz_stext_page_details *deets = fz_stext_page_details_for_block(ctx, page, block);
	if (deets != NULL)
	{
		chapter_num = deets->chapter;
		page_num = deets->page;
	}

	/* Can we merge this quad into the last one we already have? */
	if (search->details.num_quads > 0)
	{
		fz_match_quad *end = &search->details.quads[search->details.num_quads-1];
		if (end->chapter_num == chapter_num
			&& end->page_num == page_num
			&& end->seq == seq
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

	if (search->details.num_quads == search->quads_max)
	{
		int newmax = search->quads_max * 2;
		if (newmax == 0)
			newmax = 32;
		search->details.quads = fz_realloc(ctx, search->details.quads, sizeof(search->details.quads[0]) * newmax);
		search->quads_max = newmax;
	}
	search->details.quads[search->details.num_quads].chapter_num = chapter_num;
	search->details.quads[search->details.num_quads].page_num = page_num;
	search->details.quads[search->details.num_quads].seq = seq;
	search->details.quads[search->details.num_quads++].quad = ch->quad;
	search->count_quads++;
}

static void
ensure_combined_spun_haystack(fz_context *ctx, fz_search *search)
{
	if (search->combined_spun_haystack != NULL)
		return;

	assert(search->combined_index == NULL);
	if (search->transform == FZ_TEXT_TRANSFORM_NONE)
	{
		search->combined_spun_haystack = search->combined_haystack;
		search->combined_spun_split = search->combined_split;
		search->combined_spun_length = search->combined_length;
	}
	else
	{
		size_t i;
		search->combined_spun_haystack = transform_text_with_index(ctx, search->transform, search->combined_haystack, &search->combined_index);
		search->combined_spun_length = strlen(search->combined_spun_haystack);
		/* combined_spun_split needs to be the index into the combined_spun_haystack that corresponds to the split point
		 * between the first and second pages of the haystack. Currently we just search for this. We could do this better
		 * with a binary search if we were so minded. */
		for (i = 0; i < search->combined_spun_length; i++)
			if (search->combined_index[i] >= search->combined_split)
				break;
		search->combined_spun_split = i;
	}
}

static void
ensure_combined_haystack(fz_context *ctx, fz_search *search, search_page *page1, search_page *page2)
{
	char *s1, *s2;

	if (search->combined_haystack != NULL)
		return;

	if (page1->end_of_doc)
	{
		search->combined_split = 0;
		search->combined_utf_split = 0;
		if (page2->end_of_doc)
		{
			/* We started searching on a page with no text? */
			search->combined_haystack = fz_strdup(ctx, "");
			search->combined_length = 0;
			return;
		}
		s2 = page2->haystack;
		search->combined_haystack = fz_strdup(ctx, s2);
		search->combined_length = page2->length;
		return;
	}

	search->combined_split = page1->length;
	search->combined_utf_split = page1->utf_length;

	if (page2->end_of_doc)
	{
		s1 = page1->haystack;
		search->combined_haystack = fz_strdup(ctx, s1);
		search->combined_length = page1->length;
		return;
	}

	s1 = page1->haystack;
	s2 = page2->haystack;
	search->combined_haystack = fz_malloc(ctx, 1 + page1->length + page2->length);
	memcpy(search->combined_haystack, s1, page1->length);
	memcpy(search->combined_haystack + page1->length, s2, page2->length);
	search->combined_haystack[page1->length + page2->length] = 0;
	search->combined_length = page1->length + page2->length;
}

static void
advance_page(fz_context *ctx, fz_search *search, search_page *to, search_page *from)
{
	drop_page(ctx, to);
	*to = *from;
	from->page = NULL;
	from->end_of_doc = 0;
	from->map = NULL;
	from->haystack = NULL;
	from->length = 0;
	from->utf_length = 0;
	drop_combined(ctx, search);
}

fz_stext_position *lookup_search_ix(fz_search *search, size_t ix)
{
	if (ix < search->combined_utf_split)
		return &search->current_page.map[ix];
	ix -= search->combined_utf_split;
	if (ix < search->next_page.utf_length)
		return &search->next_page.map[ix];
	return NULL;
}

fz_stext_position *lookup_backward_search_ix(fz_search *search, size_t ix)
{
	if (ix < search->combined_utf_split)
		return &search->previous_page.map[ix];
	ix -= search->combined_utf_split;
	if (ix < search->current_page.utf_length)
		return &search->current_page.map[ix];
	return NULL;
}

static const char *step_back_utf8(const char *p)
{
	do
		--p;
	while (((*p & 0x80) != 0) && ((*p & 0xc0) != 0xc0));
	return p;
}

fz_search_result fz_search_forwards(fz_context *ctx, fz_search *search)
{
	fz_search_result result = { 0 };
	const char *begin, *end, *spun_begin, *spun_end;
	size_t ix, begin_ix, end_ix;
	char *needle;

	if (search == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search forwards in a non-existent search");

	if (search->needle == NULL || *search->needle == 0)
	{
		result.reason = FZ_SEARCH_COMPLETE;
		return result;
	}

	/* On entry, we assume that current_pos, current_spun_pos, and current_stext
	 * all point to the same position within the document. This might be 0/0/NULL,
	 * meaning the start of the pages we have, but they should always be consistent.
	 * We may have ditched both the combined haystack and the spun version, but the
	 * offsets within those buffers, when they are reformed, should be valid.
	 */
	assert(search->combined_spun_haystack == NULL || search->current_spun_pos <= search->combined_spun_length);
	assert(search->combined_haystack == NULL || search->current_pos <= search->combined_length);

	/* Was the last search a backwards search? If so, we need to adjust our buffers. */
	if (search->backwards == 1)
	{
		/* First remember our offset. */
		/* We were searching backwards so the combined haystack is a
		 * combination of previous_page, and current_page. */
		size_t spun_offset = search->current_spun_pos;
		size_t offset = search->current_pos;

		if (offset >= search->combined_split)
		{
			/* Adjust offset to just be in current_page. */
			offset -= search->combined_split;
			spun_offset -= search->combined_spun_split;
		}
		else
		{
			/* Offset is in prev_page. */
			/* Move current to next, and previous to current. */
			advance_page(ctx, search, &search->next_page, &search->current_page);
			advance_page(ctx, search, &search->current_page, &search->previous_page);
		}
		/* Offsets are now within current_page. */

		/* Ditch the combined haystack (and spun one). */
		drop_combined(ctx, search);

		/* There is no longer any combined_haystack, but if there were, both current_pos
		 * and current_spun_pos would be the offsets we'd like to be at within it, and
		 * should correspond to where current_stext points. */
		search->current_spun_pos = spun_offset;
		search->current_pos = offset;
	}
	search->backwards = 0;

	/* We need to have a current page. */
	if (search->current_page.page == NULL)
	{
		if (search->current_page.end_of_doc)
		{
			result.reason = FZ_SEARCH_COMPLETE;
			return result;
		}

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = 0; /* Any number will do! */
		return result;
	}

	/* And we need a haystack from that page. */
	if (ensure_haystack_for_page(ctx, search, &search->current_page))
	{
		/* current_page is empty. */
		if (search->current_page.end_of_doc || search->next_page.end_of_doc)
		{
			result.reason = FZ_SEARCH_COMPLETE;
			return result;
		}

		/* Ditch current, and move next_page down. Then ask for a new next page. */
		advance_page(ctx, search, &search->previous_page, &search->current_page);
		advance_page(ctx, search, &search->current_page, &search->next_page);

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = search->current_page.seq+1;
		return result;
	}

	/* We need to either have the next page, or to know there is no next page. */
	if (search->next_page.page == NULL && !search->next_page.end_of_doc)
	{
		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = search->current_page.seq+1;
		return result;
	}

	/* And we need a haystack from that page (unless it's the end). */
	if (ensure_haystack_for_page(ctx, search, &search->next_page))
	{
		/* We can only get here if next_page is empty. */
		int seq = search->next_page.seq+1;

		drop_page(ctx, &search->next_page);

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = seq;
		return result;
	}

	/* Then we need to combine the haystacks from those 2 pages. */
	ensure_combined_haystack(ctx, search, &search->current_page, &search->next_page);

	/* Then spin that hay into gold. */
	ensure_combined_spun_haystack(ctx, search);

	/* Check out our offsets make sense. */
	assert(search->current_spun_pos <= search->combined_spun_split && search->combined_spun_split <= search->combined_spun_length);
	assert(search->current_pos <= search->combined_split && search->combined_split <= search->combined_length);

	needle = search->spun_needle;
	if (needle == NULL)
		needle = search->needle;

	/* Where are we searching from? */
	if (search->first)
	{
		/* Search from the first char on the page. */
		search->first = 0;
	}
	else
	{
		/* Search from the char after the end of the previous match. */
	}

	/* Search within the combined_spun_haystack onwards from current_spun_pos. */
	spun_begin = search->finder->find(ctx,
		&search->compiled_needle,
		search->combined_spun_haystack + search->current_spun_pos,
		needle,
		&spun_end,
		search->bol
	);
	if (spun_begin)
	{
		/* We found a match! */

		/* step back to the last character (inclusive) */
		spun_end = step_back_utf8(spun_end);

		/* Which page did the match start on? */
		if ((size_t)(spun_begin - search->combined_spun_haystack) >= search->combined_spun_split)
		{
			/* Match starts on next_page. */

			/* For simplicity, we advance pages here. We'll refind the match next time. */
			advance_page(ctx, search, &search->previous_page, &search->current_page);
			advance_page(ctx, search, &search->current_page, &search->next_page);

			/* We'll start searching from the start of the following page. */
			drop_current_pos(search);

			result.reason = FZ_SEARCH_MORE_INPUT;
			result.u.more_input.seq_needed = search->current_page.seq+1;
			return result;
		}

		/* Match starts on current page. */

		/* Convert spun_begin/spun_end back to their equivalents in the unspun combined haystack */
		if (search->combined_spun_haystack == search->combined_haystack)
		{
			begin = spun_begin;
			end = spun_end;
		}
		else
		{
			begin = &search->combined_haystack[search->combined_index[spun_begin - search->combined_spun_haystack]];
			end = &search->combined_haystack[search->combined_index[spun_end - search->combined_spun_haystack]];
		}

		/* Find the match in the stext char list */
		begin_ix = fz_runeidx(search->combined_haystack, begin);
		end_ix = fz_runeidx(search->combined_haystack, end);

		/* Skip over positions that have no matching stext character (inserted line ends, etc) */
		while (lookup_search_ix(search, begin_ix)->ch == NULL && begin_ix < end_ix) ++begin_ix;
		assert(lookup_search_ix(search, begin_ix)->ch != NULL);
		while (lookup_search_ix(search, end_ix)->ch == NULL && end_ix > begin_ix) --end_ix;
		assert(lookup_search_ix(search, end_ix)->ch != NULL);

		/* Remember the location of the match. */
		search->details.begin = *lookup_search_ix(search, begin_ix);
		search->details.end = *lookup_search_ix(search, end_ix);

		/* Gather the quads for the result hit. */
		search->details.num_quads = 0;
		for (ix = begin_ix; ix <= end_ix; ++ix)
		{
			fz_stext_position *pos = lookup_search_ix(search, ix);
			int seq = ix < search->combined_utf_split
				? search->current_page.seq
				: search->next_page.seq;
			if (pos->ch)
			{
				add_quad(ctx, search, pos, seq);
			}
		}

		search->current_spun_pos = spun_end - search->combined_spun_haystack;
		search->current_pos = end - search->combined_haystack;

		search->current_spun_pos = spun_begin - search->combined_spun_haystack + 1;
		search->current_pos = begin - search->combined_haystack + 1;

		/* So we've found a match. */
		result.reason = FZ_SEARCH_MATCH;
		result.u.match.result = &search->details;
		return result;
	}
	else
	{
		/* We failed to match. */

		/* End of doc? We're done! */
		if (search->next_page.end_of_doc)
		{
			result.reason = FZ_SEARCH_COMPLETE;
			return result;
		}

		/* Move the next_page down, and ask for another one
		 * to keep searching. */
		advance_page(ctx, search, &search->previous_page, &search->current_page);
		advance_page(ctx, search, &search->current_page, &search->next_page);
		/* We'll restart searching from the start of the new current_page in
		 * case there is a match that spans from current -> next. */
		drop_current_pos(search);

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = search->current_page.seq+1;
		return result;
	}
}

/**
	Continue searching for the next match.

	Will return asking for more stext, or having matched.
	If there is no more stext to supply, the search is complete.
*/
fz_search_result fz_search_backwards(fz_context *ctx, fz_search *search)
{
	fz_search_result result;
	const char *begin, *end, *spun_begin, *spun_end;
	size_t ix, begin_ix, end_ix;
	char *needle;
	size_t from;

	if (search == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search backwards in a non-existent search");
	if (search->finder->find_rev == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't search backwards with this finder");

	if (search->needle == NULL || *search->needle == 0)
	{
		result.reason = FZ_SEARCH_COMPLETE;
		return result;
	}

	/* On entry, we assume that current_pos, current_spun_pos, and current_stext
	 * all point to the same position within the document. This might be 0/0/NULL,
	 * meaning the start of the pages we have, but they should always be consistent.
	 * We may have ditched both the combined haystack and the spun version, but the
	 * offsets within those buffers, when they are reformed, should be valid.
	 */
	assert(search->combined_spun_haystack == NULL || search->current_spun_pos <= search->combined_spun_length);
	assert(search->combined_haystack == NULL || search->current_pos <= search->combined_length);

	/* Was the last search a forwards search? If so, we need to adjust our buffers. */
	if (search->backwards == 0)
	{
		/* First remember our offset. */
		/* We were searching forwards so we the combined haystack is a
		 * combination of current_page, and next page. */
		size_t spun_offset = search->current_spun_pos;
		size_t offset = search->current_pos;

		if (offset < search->combined_split || offset == 0)
		{
			/* Offset is just in current_page. */
		}
		else
		{
			/* Offset is in next_page. */
			offset -= search->combined_split;
			spun_offset -= search->combined_spun_split;
			/* Move current to previous, and next to current. */
			advance_page(ctx, search, &search->previous_page, &search->current_page);
			advance_page(ctx, search, &search->current_page, &search->next_page);
		}
		/* Offset is now within current_page. */

		/* Ditch the combined haystack (and spun one). */
		drop_combined(ctx, search);

		/* There is no longer a combined_haystack, but if there was, both current_pos
		 * and current_spun_pos would be the offset we'd like it be within it, and
		 * should correspond to where current_stext points. */
		search->current_spun_pos = spun_offset;
		search->current_pos = offset;
	}
	search->backwards = 1;

	/* We need to have a current page. */
	if (search->current_page.page == NULL)
	{
		if (search->current_page.end_of_doc)
		{
			result.reason = FZ_SEARCH_COMPLETE;
			return result;
		}

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = 0; /* Any number will do! */
		return result;
	}

	/* And we need a haystack from that page. */
	if (ensure_haystack_for_page(ctx, search, &search->current_page))
	{
		/* current_page is empty. */
		if (search->current_page.end_of_doc || search->next_page.end_of_doc)
		{
			result.reason = FZ_SEARCH_COMPLETE;
			return result;
		}

		/* Ditch current, and move previous_page up. Then ask for a new previous_page. */
		advance_page(ctx, search, &search->next_page, &search->current_page);
		advance_page(ctx, search, &search->current_page, &search->previous_page);

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = search->current_page.seq-1;

		return result;
	}

	/* We need to either have the previous page, or to know there is no previous page. */
	if (search->previous_page.page == NULL && !search->previous_page.end_of_doc)
	{
		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = search->current_page.seq-1;

		return result;
	}

	/* And we need a haystack from that page (unless it's the beginning). */
	if (ensure_haystack_for_page(ctx, search, &search->previous_page))
	{
		/* We can only get here if previous_page is empty. */
		int seq = search->previous_page.seq-1;

		drop_page(ctx, &search->previous_page);

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = seq;

		return result;
	}

	/* Then we need to combine the haystacks from those 2 pages. */
	ensure_combined_haystack(ctx, search, &search->previous_page, &search->current_page);

	/* Then spin that hay into gold. */
	ensure_combined_spun_haystack(ctx, search);

	/* Check out our offsets make sense. */
	assert(search->current_spun_pos <= search->combined_spun_length && search->combined_spun_split <= search->combined_spun_length);
	assert(search->current_pos <= search->combined_length && search->combined_split <= search->combined_length);

	needle = search->spun_needle;
	if (needle == NULL)
		needle = search->needle;

	/* We are searching from the last char on the page or from the char before the previous match. */
	if (search->first)
	{
		/* Search from the last char on the page. */
		search->first = 0;
		if (search->combined_spun_length == 0)
			goto failed_to_match;
		from = search->combined_spun_length;
		assert(from >= 0 && from <= search->combined_spun_length);
	}
	else
	{
		/* Search from the char before where we matched before. */
		if (search->current_spun_pos == 0)
			goto failed_to_match;
		from = search->current_spun_pos - 1;
		assert(from >= 0 && from <= search->combined_spun_length);
	}

	/* Search within the combined_spun_haystack onwards from current_spun_pos. */
	spun_begin = search->finder->find_rev(ctx,
		&search->compiled_needle,
		search->combined_spun_haystack,
		from,
		needle,
		&spun_end,
		search->bol
	);
	if (spun_end)
	{
		/* We found a match! */

		/* Which page did the match end on? */
		if ((size_t)(spun_end - search->combined_spun_haystack) <= search->combined_spun_split)
		{
			/* Match ends on previous_page. */

			/* For simplicity, we advance pages here. We'll refind the match next time. */
			advance_page(ctx, search, &search->next_page, &search->current_page);
			advance_page(ctx, search, &search->current_page, &search->previous_page);

			/* We'll start searching from the end of the current page. */
			drop_current_pos(search);

			result.reason = FZ_SEARCH_MORE_INPUT;
			result.u.more_input.seq_needed = search->current_page.seq-1;

			return result;
		}

		/* Match ends on current page. But where does it begin? */
		if ((size_t)(spun_begin - search->combined_spun_haystack) <= search->combined_split)
		{
			/* Begins on previous_page */

			/* Convert spun_begin/spun_end back to their equivalents in the original haystack */
			if (search->combined_spun_haystack == search->combined_haystack)
			{
				begin = spun_begin;
				end = spun_end;
			}
			else
			{
				begin = &search->combined_haystack[search->combined_index[spun_begin - search->combined_spun_haystack]];
				end = &search->combined_haystack[search->combined_index[spun_end - search->combined_spun_haystack]];
			}
		}
		else
		{
			/* Begins on current_page */

			/* Convert spun_begin/spun_end back to their equivalents in the combined haystack */
			if (search->combined_spun_haystack == search->combined_haystack)
			{
				begin = spun_begin + (search->combined_haystack - search->combined_spun_haystack);
				end = spun_end + (search->combined_haystack - search->combined_spun_haystack);
			}
			else
			{
				begin = &search->combined_haystack[search->combined_index[spun_begin - search->combined_spun_haystack]];
				end = &search->combined_haystack[search->combined_index[spun_end - search->combined_spun_haystack]];
			}
		}

		/* Find the match in the stext char list */
		begin_ix = fz_runeidx(search->combined_haystack, begin);
		end_ix = fz_runeidx(search->combined_haystack, end);

		/* Skip over positions that have no matching stext character (inserted line ends, etc) */
		while (lookup_backward_search_ix(search, begin_ix)->ch == NULL && begin_ix < end_ix) ++begin_ix;
		assert(lookup_backward_search_ix(search, begin_ix)->ch != NULL);
		while (lookup_backward_search_ix(search, end_ix)->ch == NULL && end_ix > begin_ix) --end_ix;
		assert(lookup_backward_search_ix(search, end_ix)->ch != NULL);

		/* Remember the location of the match. */
		search->details.begin = *lookup_backward_search_ix(search, begin_ix);
		search->details.end = *lookup_backward_search_ix(search, end_ix);

		/* Gather the quads for the result hit. */
		search->details.num_quads = 0;
		for (ix = begin_ix; ix <= end_ix; ++ix)
		{
			fz_stext_position *pos = lookup_backward_search_ix(search, ix);
			int seq = ix < search->combined_utf_split
				? search->previous_page.seq
				: search->current_page.seq;
			if (pos->ch)
			{
				add_quad(ctx, search, pos, seq);
			}
		}

		/* start next match at the char before the current match */
		search->current_spun_pos = spun_begin - search->combined_spun_haystack;
		search->current_pos = begin - search->combined_haystack;

		/* So we've found a match. */
		result.reason = FZ_SEARCH_MATCH;
		result.u.match.result = &search->details;
		return result;
	}
	else
	{
		/* We failed to match. */

failed_to_match:

		/* End of doc? We're done! */
		if (search->previous_page.end_of_doc)
		{
			result.reason = FZ_SEARCH_COMPLETE;
			return result;
		}

		/* Move the previous_page up, and ask for another one
		 * to keep searching. */
		advance_page(ctx, search, &search->next_page, &search->current_page);
		advance_page(ctx, search, &search->current_page, &search->previous_page);
		/* We'll restart searching from the end of the new current_page in
		 * case there is a match that spans from current -> next. */
		drop_current_pos(search);

		result.reason = FZ_SEARCH_MORE_INPUT;
		result.u.more_input.seq_needed = search->current_page.seq-1;

		return result;
	}

	return result;
}

void fz_feed_search(fz_context *ctx, fz_search *search, fz_stext_page *page, int seq)
{
	search_page *pg;

	if (search == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Can't feed a non-existent search");

	pg = search->backwards ? &search->previous_page : &search->next_page;
	if (search->current_page.page == NULL)
		pg = &search->current_page;

	drop_page(ctx, pg);

	pg->page = page;
	pg->seq = seq;
	pg->end_of_doc = (page == NULL);
}

void fz_drop_search(fz_context *ctx, fz_search *search)
{
	if (search == NULL)
		return;

	drop_page(ctx, &search->current_page);
	drop_page(ctx, &search->previous_page);
	drop_page(ctx, &search->next_page);
	drop_combined(ctx, search);
	drop_needle(ctx, search);
	fz_free(ctx, search->details.quads);

	fz_free(ctx, search);
}
