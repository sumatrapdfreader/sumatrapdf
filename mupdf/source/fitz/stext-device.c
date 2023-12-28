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

#include "glyphbox.h"

#include <math.h>
#include <float.h>
#include <string.h>

/* Simple layout structure */

fz_layout_block *fz_new_layout(fz_context *ctx)
{
	fz_pool *pool = fz_new_pool(ctx);
	fz_layout_block *block;
	fz_try(ctx)
	{
		block = fz_pool_alloc(ctx, pool, sizeof (fz_layout_block));
		block->pool = pool;
		block->head = NULL;
		block->tailp = &block->head;
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
		fz_rethrow(ctx);
	}
	return block;
}

void fz_drop_layout(fz_context *ctx, fz_layout_block *block)
{
	if (block)
		fz_drop_pool(ctx, block->pool);
}

void fz_add_layout_line(fz_context *ctx, fz_layout_block *block, float x, float y, float font_size, const char *p)
{
	fz_layout_line *line = fz_pool_alloc(ctx, block->pool, sizeof (fz_layout_line));
	line->x = x;
	line->y = y;
	line->font_size = font_size;
	line->p = p;
	line->text = NULL;
	line->next = NULL;
	*block->tailp = line;
	block->tailp = &line->next;
	block->text_tailp = &line->text;
}

void fz_add_layout_char(fz_context *ctx, fz_layout_block *block, float x, float advance, const char *p)
{
	fz_layout_char *ch = fz_pool_alloc(ctx, block->pool, sizeof (fz_layout_char));
	ch->x = x;
	ch->advance = advance;
	ch->p = p;
	ch->next = NULL;
	*block->text_tailp = ch;
	block->text_tailp = &ch->next;
}

/* Extract text into blocks and lines. */

#define PARAGRAPH_DIST 1.5f
#define SPACE_DIST 0.15f
#define SPACE_MAX_DIST 0.8f
#define BASE_MAX_DIST 0.8f

/* We keep a stack of the different metatexts that apply at any
 * given point (normally none!). Whenever we get some content
 * with a metatext in force, we really want to update the bounds
 * for that metatext. But running along the whole list each time
 * would be painful. So we just update the bounds for dev->metatext
 * and rely on metatext_bounds() propagating it upwards 'just in
 * time' for us to use metatexts other than the latest one. This
 * also means we need to propagate bounds upwards when we pop
 * a metatext.
 *
 * Why do we need bounds at all? Well, suppose we get:
 *    /Span <</ActualText (c) >> BDC /Im0 Do EMC
 * Then where on the page do we put 'c' ? By collecting the
 * bounds, we can place 'c' wherever the image was.
 */
typedef struct metatext_t
{
	fz_metatext type;
	char *text;
	fz_rect bounds;
	struct metatext_t *prev;
} metatext_t;

typedef struct
{
	fz_device super;
	fz_stext_page *page;
	fz_point pen, start;
	fz_point lag_pen;
	fz_matrix trm;
	int new_obj;
	int lastchar;
	int lastbidi;
	int flags;
	int color;
	const fz_text *lasttext;
	fz_stext_options opts;

	metatext_t *metatext;

	/* Store the last values we saw. We need this for flushing the actualtext. */
	struct
	{
		int valid;
		int clipped;
		fz_matrix trm;
		int wmode;
		int bidi_level;
		fz_font *font;
	} last;
} fz_stext_device;

const char *fz_stext_options_usage =
	"Text output options:\n"
	"\tinhibit-spaces: don't add spaces between gaps in the text\n"
	"\tpreserve-images: keep images in output\n"
	"\tpreserve-ligatures: do not expand ligatures into constituent characters\n"
	"\tpreserve-whitespace: do not convert all whitespace into space characters\n"
	"\tpreserve-spans: do not merge spans on the same line\n"
	"\tdehyphenate: attempt to join up hyphenated words\n"
	"\tmediabox-clip=no: include characters outside mediabox\n"
	"\n";

/* Find the current actualtext, if any. Will abort if dev == NULL. */
static metatext_t *
find_actualtext(fz_stext_device *dev)
{
	metatext_t *mt = dev->metatext;

	while (mt && mt->type != FZ_METATEXT_ACTUALTEXT)
		mt = mt->prev;

	return mt;
}

/* Find the bounds of the given metatext. Will abort if mt or
 * dev are NULL. */
static fz_rect *
metatext_bounds(metatext_t *mt, fz_stext_device *dev)
{
	metatext_t *mt2 = dev->metatext;

	while (mt2 != mt)
	{
		mt2->prev->bounds = fz_union_rect(mt2->prev->bounds, mt2->bounds);
		mt2 = mt2->prev;
	}

	return &mt->bounds;
}

/* Find the bounds of the current actualtext, or NULL if there
 * isn't one. Will abort if dev is NULL. */
static fz_rect *
actualtext_bounds(fz_stext_device *dev)
{
	metatext_t *mt = find_actualtext(dev);

	if (mt == NULL)
		return NULL;

	return metatext_bounds(mt, dev);
}

fz_stext_page *
fz_new_stext_page(fz_context *ctx, fz_rect mediabox)
{
	fz_pool *pool = fz_new_pool(ctx);
	fz_stext_page *page = NULL;
	fz_try(ctx)
	{
		page = fz_pool_alloc(ctx, pool, sizeof(*page));
		page->pool = pool;
		page->mediabox = mediabox;
		page->first_block = NULL;
		page->last_block = NULL;
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
		fz_rethrow(ctx);
	}
	return page;
}

void
fz_drop_stext_page(fz_context *ctx, fz_stext_page *page)
{
	if (page)
	{
		fz_stext_block *block;
		fz_stext_line *line;
		fz_stext_char *ch;
		for (block = page->first_block; block; block = block->next)
		{
			if (block->type == FZ_STEXT_BLOCK_IMAGE)
				fz_drop_image(ctx, block->u.i.image);
			else
				for (line = block->u.t.first_line; line; line = line->next)
					for (ch = line->first_char; ch; ch = ch->next)
						fz_drop_font(ctx, ch->font);
		}
		fz_drop_pool(ctx, page->pool);
	}
}

static fz_stext_block *
add_block_to_page(fz_context *ctx, fz_stext_page *page)
{
	fz_stext_block *block = fz_pool_alloc(ctx, page->pool, sizeof *page->first_block);
	block->bbox = fz_empty_rect; /* Fixes bug 703267. */
	block->prev = page->last_block;
	if (!page->first_block)
		page->first_block = page->last_block = block;
	else
	{
		page->last_block->next = block;
		page->last_block = block;
	}
	return block;
}

static fz_stext_block *
add_text_block_to_page(fz_context *ctx, fz_stext_page *page)
{
	fz_stext_block *block = add_block_to_page(ctx, page);
	block->type = FZ_STEXT_BLOCK_TEXT;
	return block;
}

static fz_stext_block *
add_image_block_to_page(fz_context *ctx, fz_stext_page *page, fz_matrix ctm, fz_image *image)
{
	fz_stext_block *block = add_block_to_page(ctx, page);
	block->type = FZ_STEXT_BLOCK_IMAGE;
	block->u.i.transform = ctm;
	block->u.i.image = fz_keep_image(ctx, image);
	block->bbox = fz_transform_rect(fz_unit_rect, ctm);
	return block;
}

static fz_stext_line *
add_line_to_block(fz_context *ctx, fz_stext_page *page, fz_stext_block *block, const fz_point *dir, int wmode, int bidi)
{
	fz_stext_line *line = fz_pool_alloc(ctx, page->pool, sizeof *block->u.t.first_line);
	line->prev = block->u.t.last_line;
	if (!block->u.t.first_line)
		block->u.t.first_line = block->u.t.last_line = line;
	else
	{
		block->u.t.last_line->next = line;
		block->u.t.last_line = line;
	}

	line->dir = *dir;
	line->wmode = wmode;

	return line;
}

static fz_stext_char *
add_char_to_line(fz_context *ctx, fz_stext_page *page, fz_stext_line *line, fz_matrix trm, fz_font *font, float size, int c, fz_point *p, fz_point *q, int bidi, int color)
{
	fz_stext_char *ch = fz_pool_alloc(ctx, page->pool, sizeof *line->first_char);
	fz_point a, d;

	if (!line->first_char)
		line->first_char = line->last_char = ch;
	else
	{
		line->last_char->next = ch;
		line->last_char = ch;
	}

	ch->c = c;
	ch->color = color;
	ch->bidi = bidi;
	ch->origin = *p;
	ch->size = size;
	ch->font = fz_keep_font(ctx, font);

	if (line->wmode == 0)
	{
		a.x = 0;
		d.x = 0;
		a.y = fz_font_ascender(ctx, font);
		d.y = fz_font_descender(ctx, font);
	}
	else
	{
		a.x = 1;
		d.x = 0;
		a.y = 0;
		d.y = 0;
	}
	a = fz_transform_vector(a, trm);
	d = fz_transform_vector(d, trm);

	ch->quad.ll = fz_make_point(p->x + d.x, p->y + d.y);
	ch->quad.ul = fz_make_point(p->x + a.x, p->y + a.y);
	ch->quad.lr = fz_make_point(q->x + d.x, q->y + d.y);
	ch->quad.ur = fz_make_point(q->x + a.x, q->y + a.y);

	return ch;
}

static void
remove_last_char(fz_context *ctx, fz_stext_line *line)
{
	if (line && line->first_char)
	{
		fz_stext_char *prev = NULL;
		fz_stext_char *ch = line->first_char;
		while (ch->next)
		{
			prev = ch;
			ch = ch->next;
		}
		if (prev)
		{
			/* The characters are pool allocated, so we don't actually leak the removed node. */
			/* We do need to drop the char's font reference though. */
			fz_drop_font(ctx, prev->next->font);
			line->last_char = prev;
			line->last_char->next = NULL;
		}
	}
}

static fz_stext_char *reverse_bidi_span(fz_stext_char *curr, fz_stext_char *tail)
{
	fz_stext_char *prev, *next;
	prev = tail;
	while (curr != tail)
	{
		next = curr->next;
		curr->next = prev;
		prev = curr;
		curr = next;
	}
	return prev;
}

static void reverse_bidi_line(fz_stext_line *line)
{
	fz_stext_char *a, *b, **prev;
	prev = &line->first_char;
	for (a = line->first_char; a; a = a->next)
	{
		if (a->bidi)
		{
			b = a;
			while (b->next && b->next->bidi)
				b = b->next;
			if (a != b)
				*prev = reverse_bidi_span(a, b->next);
		}
		prev = &a->next;
		line->last_char = a;
	}
}

static int is_hyphen(int c)
{
	/* check for: hyphen-minus, soft hyphen, hyphen, and non-breaking hyphen */
	return (c == '-' || c == 0xAD || c == 0x2010 || c == 0x2011);
}

static float
vec_dot(const fz_point *a, const fz_point *b)
{
	return a->x * b->x + a->y * b->y;
}

static int may_add_space(int lastchar)
{
	/* Basic latin, greek, cyrillic, hebrew, arabic,
	 * general punctuation,
	 * superscripts and subscripts,
	 * and currency symbols.
	 */
	return (lastchar != ' ' && (lastchar < 0x700 || (lastchar >= 0x2000 && lastchar <= 0x20CF)));
}

static void
fz_add_stext_char_imp(fz_context *ctx, fz_stext_device *dev, fz_font *font, int c, int glyph, fz_matrix trm, float adv, int wmode, int bidi, int force_new_line)
{
	fz_stext_page *page = dev->page;
	fz_stext_block *cur_block;
	fz_stext_line *cur_line;

	int new_para = 0;
	int new_line = 1;
	int add_space = 0;
	fz_point dir, ndir, p, q;
	float size;
	fz_point delta;
	float spacing = 0;
	float base_offset = 0;

	/* Preserve RTL-ness only (and ignore level) so we can use bit 2 as "visual" tag for reordering pass. */
	bidi = bidi & 1;

	/* dir = direction vector for motion. ndir = normalised(dir) */
	if (wmode == 0)
	{
		dir.x = 1;
		dir.y = 0;
	}
	else
	{
		dir.x = 0;
		dir.y = -1;
	}
	dir = fz_transform_vector(dir, trm);
	ndir = fz_normalize_vector(dir);

	size = fz_matrix_expansion(trm);

	/* We need to identify where glyphs 'start' (p) and 'stop' (q).
	 * Each glyph holds its 'start' position, and the next glyph in the
	 * span (or span->max if there is no next glyph) holds its 'end'
	 * position.
	 *
	 * For both horizontal and vertical motion, trm->{e,f} gives the
	 * origin (usually the bottom left) of the glyph.
	 *
	 * In horizontal mode:
	 *   + p is bottom left.
	 *   + q is the bottom right
	 * In vertical mode:
	 *   + p is top left (where it advanced from)
	 *   + q is bottom left
	 */
	if (wmode == 0)
	{
		p.x = trm.e;
		p.y = trm.f;
		q.x = trm.e + adv * dir.x;
		q.y = trm.f + adv * dir.y;
	}
	else
	{
		p.x = trm.e - adv * dir.x;
		p.y = trm.f - adv * dir.y;
		q.x = trm.e;
		q.y = trm.f;
	}

	/* Find current position to enter new text. */
	cur_block = page->last_block;
	if (cur_block && cur_block->type != FZ_STEXT_BLOCK_TEXT)
		cur_block = NULL;
	cur_line = cur_block ? cur_block->u.t.last_line : NULL;

	if (cur_line && glyph < 0)
	{
		/* Don't advance pen or break lines for no-glyph characters in a cluster */
		add_char_to_line(ctx, page, cur_line, trm, font, size, c, &dev->pen, &dev->pen, bidi, dev->color);
		dev->lastbidi = bidi;
		dev->lastchar = c;
		return;
	}

	if (cur_line == NULL || cur_line->wmode != wmode || vec_dot(&ndir, &cur_line->dir) < 0.999f)
	{
		/* If the matrix has changed rotation, or the wmode is different (or if we don't have a line at all),
		 * then we can't append to the current block/line. */
		new_para = 1;
		new_line = 1;
	}
	else
	{
		/* Detect fake bold where text is printed twice in the same place. */
		delta.x = fabsf(q.x - dev->pen.x);
		delta.y = fabsf(q.y - dev->pen.y);
		if (delta.x < FLT_EPSILON && delta.y < FLT_EPSILON && c == dev->lastchar)
			return;

		/* Calculate how far we've moved since the last character. */
		delta.x = p.x - dev->pen.x;
		delta.y = p.y - dev->pen.y;

		/* The transform has not changed, so we know we're in the same
		 * direction. Calculate 2 distances; how far off the previous
		 * baseline we are, together with how far along the baseline
		 * we are from the expected position. */
		spacing = (ndir.x * delta.x + ndir.y * delta.y) / size;
		base_offset = (-ndir.y * delta.x + ndir.x * delta.y) / size;

		/* Only a small amount off the baseline - we'll take this */
		if (fabsf(base_offset) < BASE_MAX_DIST)
		{
			/* If mixed LTR and RTL content */
			if ((bidi & 1) != (dev->lastbidi & 1))
			{
				/* Ignore jumps within line when switching between LTR and RTL text. */
				new_line = 0;
			}

			/* RTL */
			else if (bidi & 1)
			{
				fz_point logical_delta = fz_make_point(p.x - dev->lag_pen.x, p.y - dev->lag_pen.y);
				float logical_spacing = (ndir.x * logical_delta.x + ndir.y * logical_delta.y) / size + adv;

				/* If the pen is where we would have been if we
				 * had advanced backwards from the previous
				 * character by this character's advance, we
				 * are probably seeing characters emitted in
				 * logical order.
				 */
				if (fabsf(logical_spacing) < SPACE_DIST)
				{
					new_line = 0;
				}

				/* However, if the pen has advanced to where we would expect it
				 * in an LTR context, we're seeing them emitted in visual order
				 * and should flag them for reordering!
				 */
				else if (fabsf(spacing) < SPACE_DIST)
				{
					bidi = 3; /* mark line as visual */
					new_line = 0;
				}

				/* And any other small jump could be a missing space. */
				else if (logical_spacing < 0 && logical_spacing > -SPACE_MAX_DIST)
				{
					if (wmode == 0 && may_add_space(dev->lastchar))
						add_space = 1;
					new_line = 0;
				}

				else if (spacing > 0 && spacing < SPACE_MAX_DIST)
				{
					bidi = 3; /* mark line as visual */
					if (wmode == 0 && may_add_space(dev->lastchar))
						add_space = 1;
					new_line = 0;
				}

				else
				{
					/* Motion is large and unexpected (probably a new table column). */
					new_line = 1;
				}
			}

			/* LTR or neutral character */
			else
			{
				if (fabsf(spacing) < SPACE_DIST)
				{
					/* Motion is in line and small enough to ignore. */
					new_line = 0;
				}
				else if (spacing > 0 && spacing < SPACE_MAX_DIST)
				{
					/* Motion is forward in line and large enough to warrant us adding a space. */
					if (wmode == 0 && may_add_space(dev->lastchar))
						add_space = 1;
					new_line = 0;
				}
				else
				{
					/* Motion is large and unexpected (probably a new table column). */
					new_line = 1;
				}
			}
		}

		/* Enough for a new line, but not enough for a new paragraph */
		else if (fabsf(base_offset) <= PARAGRAPH_DIST)
		{
			/* Check indent to spot text-indent style paragraphs */
			if (wmode == 0 && cur_line && dev->new_obj)
				if (fabsf(p.x - dev->start.x) > 0.5f)
					new_para = 1;
			new_line = 1;
		}

		/* Way off the baseline - open a new paragraph */
		else
		{
			new_para = 1;
			new_line = 1;
		}
	}

	/* Start a new block (but only at the beginning of a text object) */
	if (new_para || !cur_block)
	{
		cur_block = add_text_block_to_page(ctx, page);
		cur_line = cur_block->u.t.last_line;
	}

	if (new_line && (dev->flags & FZ_STEXT_DEHYPHENATE) && is_hyphen(dev->lastchar))
	{
		remove_last_char(ctx, cur_line);
		new_line = 0;
	}

	/* Start a new line */
	if (new_line || !cur_line || force_new_line)
	{
		cur_line = add_line_to_block(ctx, page, cur_block, &ndir, wmode, bidi);
		dev->start = p;
	}

	/* Add synthetic space */
	if (add_space && !(dev->flags & FZ_STEXT_INHIBIT_SPACES))
		add_char_to_line(ctx, page, cur_line, trm, font, size, ' ', &dev->pen, &p, bidi, dev->color);

	add_char_to_line(ctx, page, cur_line, trm, font, size, c, &p, &q, bidi, dev->color);
	dev->lastchar = c;
	dev->lastbidi = bidi;
	dev->lag_pen = p;
	dev->pen = q;

	dev->new_obj = 0;
	dev->trm = trm;
}

static void
fz_add_stext_char(fz_context *ctx,
	fz_stext_device *dev,
	fz_font *font,
	int c,
	int glyph,
	fz_matrix trm,
	float adv,
	int wmode,
	int bidi,
	int force_new_line)
{
	/* ignore when one unicode character maps to multiple glyphs */
	if (c == -1)
		return;

	if (!(dev->flags & FZ_STEXT_PRESERVE_LIGATURES))
	{
		switch (c)
		{
		case 0xFB00: /* ff */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line);
			fz_add_stext_char_imp(ctx, dev, font, 'f', -1, trm, 0, wmode, bidi, 0);
			return;
		case 0xFB01: /* fi */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line);
			fz_add_stext_char_imp(ctx, dev, font, 'i', -1, trm, 0, wmode, bidi, 0);
			return;
		case 0xFB02: /* fl */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line);
			fz_add_stext_char_imp(ctx, dev, font, 'l', -1, trm, 0, wmode, bidi, 0);
			return;
		case 0xFB03: /* ffi */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line);
			fz_add_stext_char_imp(ctx, dev, font, 'f', -1, trm, 0, wmode, bidi, 0);
			fz_add_stext_char_imp(ctx, dev, font, 'i', -1, trm, 0, wmode, bidi, 0);
			return;
		case 0xFB04: /* ffl */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line);
			fz_add_stext_char_imp(ctx, dev, font, 'f', -1, trm, 0, wmode, bidi, 0);
			fz_add_stext_char_imp(ctx, dev, font, 'l', -1, trm, 0, wmode, bidi, 0);
			return;
		case 0xFB05: /* long st */
		case 0xFB06: /* st */
			fz_add_stext_char_imp(ctx, dev, font, 's', glyph, trm, adv, wmode, bidi, force_new_line);
			fz_add_stext_char_imp(ctx, dev, font, 't', -1, trm, 0, wmode, bidi, 0);
			return;
		}
	}

	if (!(dev->flags & FZ_STEXT_PRESERVE_WHITESPACE))
	{
		switch (c)
		{
		case 0x0009: /* tab */
		case 0x0020: /* space */
		case 0x00A0: /* no-break space */
		case 0x1680: /* ogham space mark */
		case 0x180E: /* mongolian vowel separator */
		case 0x2000: /* en quad */
		case 0x2001: /* em quad */
		case 0x2002: /* en space */
		case 0x2003: /* em space */
		case 0x2004: /* three-per-em space */
		case 0x2005: /* four-per-em space */
		case 0x2006: /* six-per-em space */
		case 0x2007: /* figure space */
		case 0x2008: /* punctuation space */
		case 0x2009: /* thin space */
		case 0x200A: /* hair space */
		case 0x202F: /* narrow no-break space */
		case 0x205F: /* medium mathematical space */
		case 0x3000: /* ideographic space */
			c = ' ';
		}
	}

	fz_add_stext_char_imp(ctx, dev, font, c, glyph, trm, adv, wmode, bidi, force_new_line);
}

static void
do_extract(fz_context *ctx, fz_stext_device *dev, fz_text_span *span, fz_matrix ctm, int start, int end)
{
	fz_font *font = span->font;
	fz_matrix tm = span->trm;
	float adv;
	int i;

	for (i = start; i < end; i++)
	{
		/* Calculate new pen location and delta */
		tm.e = span->items[i].x;
		tm.f = span->items[i].y;
		dev->last.trm = fz_concat(tm, ctm);
		dev->last.bidi_level = span->bidi_level;
		dev->last.wmode = span->wmode;
		if (font != dev->last.font)
		{
			fz_drop_font(ctx, dev->last.font);
			dev->last.font = fz_keep_font(ctx, font);
		}
		dev->last.valid = 1;

		if (dev->flags & FZ_STEXT_MEDIABOX_CLIP)
			if (fz_glyph_entirely_outside_box(ctx, &ctm, span, &span->items[i], &dev->page->mediabox))
			{
				dev->last.clipped = 1;
				continue;
			}
		dev->last.clipped = 0;

		/* Calculate bounding box and new pen position based on font metrics */
		if (span->items[i].gid >= 0)
			adv = fz_advance_glyph(ctx, font, span->items[i].gid, span->wmode);
		else
			adv = 0;

		/* Send the chars we have through. */
		fz_add_stext_char(ctx, dev, font,
			span->items[i].ucs,
			span->items[i].gid,
			dev->last.trm,
			adv,
			dev->last.wmode,
			dev->last.bidi_level,
			(i == 0) && (dev->flags & FZ_STEXT_PRESERVE_SPANS));
	}
}

static int
rune_index(const char *utf8, size_t idx)
{
	int rune;

	do
	{
		int len = fz_chartorune(&rune, utf8);
		if (rune == 0)
			return -1;
		utf8 += len;
	}
	while (idx--);

	return rune;
}

static void
flush_actualtext(fz_context *ctx, fz_stext_device *dev, const char *actualtext, int i)
{
	if (*actualtext == 0)
		return;

	while (1)
	{
		int rune;
		actualtext += fz_chartorune(&rune, actualtext);

		if (rune == 0)
			break;

		if (dev->flags & FZ_STEXT_MEDIABOX_CLIP)
			if (dev->last.clipped)
				continue;

		fz_add_stext_char(ctx, dev, dev->last.font,
			rune,
			-1,
			dev->last.trm,
			0,
			dev->last.wmode,
			dev->last.bidi_level,
			(i == 0) && (dev->flags & FZ_STEXT_PRESERVE_SPANS));
		i++;
	}
}

static void
do_extract_within_actualtext(fz_context *ctx, fz_stext_device *dev, fz_text_span *span, fz_matrix ctm, metatext_t *mt)
{
	/* We are within an actualtext block. This means we can't just add the chars
	 * as they are. We need to add the chars as they are meant to be. Sadly the
	 * actualtext mechanism doesn't help us at all with positioning. */
	fz_font *font = span->font;
	fz_matrix tm = span->trm;
	float adv;
	int start, i, end;
	char *actualtext = mt->text;
	size_t z = fz_utflen(actualtext);

	/* If actualtext is empty, nothing to do! */
	if (z == 0)
		return;

	/* Now, we HOPE that the creator of a PDF will minimise the actual text
	 * differences, so that we'll get:
	 *   "Politicians <Actualtext="lie">fib</ActualText>, always."
	 * rather than:
	 *   "<Actualtext="Politicians lie, always">Politicians fib, always.</ActualText>
	 * but experience with PDF files tells us that this won't always be the case.
	 *
	 * We try to minimise the actualtext section here, just in case.
	 */

	/* Spot a matching prefix and send it. */
	for (start = 0; start < span->len; start++)
	{
		int rune;
		int len = fz_chartorune(&rune, actualtext);
		if (span->items[start].gid != rune || rune == 0)
			break;
		actualtext += len; z--;
	}
	if (start != 0)
		do_extract(ctx, dev, span, ctm, 0, start);

	if (start == span->len)
	{
		/* The prefix has consumed all this object. Just shorten the actualtext and we'll
		 * catch the rest next time. */
		z = strlen(actualtext)+1;
		memmove(mt->text, actualtext, z);
		return;
	}

	/* Spot a matching postfix. Can't send it til the end. */
	for (end = span->len; end > start; end--)
	{
		/* Nasty n^2 algo here, cos backtracking through utf8 is not trivial. It'll do. */
		int rune = rune_index(actualtext, z-1);
		if (span->items[end-1].gid != rune)
			break;
		z--;
	}
	/* So we can send end -> span->len at the end. */

	/* So we have at least SOME chars that don't match. */
	/* Now, do the difficult bit in the middle.*/
	/* items[start..end] have to be sent with actualtext[start..z] */
	for (i = start; i < end; i++)
	{
		fz_text_item *item = &span->items[i];
		int rune = -1;

		if ((size_t)i < z)
			actualtext += fz_chartorune(&rune, actualtext);

		/* Calculate new pen location and delta */
		tm.e = item->x;
		tm.f = item->y;
		dev->last.trm = fz_concat(tm, ctm);
		dev->last.bidi_level = span->bidi_level;
		dev->last.wmode = span->wmode;
		if (font != dev->last.font)
		{
			fz_drop_font(ctx, dev->last.font);
			dev->last.font = fz_keep_font(ctx, font);
		}
		dev->last.valid = 1;

		if (dev->flags & FZ_STEXT_MEDIABOX_CLIP)
			if (fz_glyph_entirely_outside_box(ctx, &ctm, span, &span->items[i], &dev->page->mediabox))
			{
				dev->last.clipped = 1;
				continue;
			}
		dev->last.clipped = 0;

		/* Calculate bounding box and new pen position based on font metrics */
		if (item->gid >= 0)
			adv = fz_advance_glyph(ctx, font, item->gid, span->wmode);
		else
			adv = 0;

		fz_add_stext_char(ctx, dev, font,
			rune,
			span->items[i].gid,
			dev->last.trm,
			adv,
			dev->last.wmode,
			dev->last.bidi_level,
			(i == 0) && (dev->flags & FZ_STEXT_PRESERVE_SPANS));
	}

	/* If we haven't spotted a postfix by this point, then don't force ourselves to output
	 * any more of the actualtext at this point. We might get a new text object that matches
	 * more of it. */
	if (end == span->len)
	{
		/* Shorten actualtext and exit. */
		z = strlen(actualtext)+1;
		memmove(mt->text, actualtext, z);
		return;
	}

	/* We found a matching postfix. It seems likely that this is going to be the only
	 * text object we get, so send any remaining actualtext now. */
	flush_actualtext(ctx, dev, actualtext, i);

	/* Send the postfix */
	if (end != span->len)
		do_extract(ctx, dev, span, ctm, end, span->len);

	mt->text[0] = 0;
}

static void
fz_stext_extract(fz_context *ctx, fz_stext_device *dev, fz_text_span *span, fz_matrix ctm)
{
	metatext_t *mt;

	if (span->len == 0)
		return;

	/* Are we in an actualtext? */
	mt = find_actualtext(dev);

	if (mt)
		do_extract_within_actualtext(ctx, dev, span, ctm, mt);
	else
		do_extract(ctx, dev, span, ctm, 0, span->len);
}

static int hexrgb_from_color(fz_context *ctx, fz_colorspace *colorspace, const float *color)
{
	float rgb[3];
	fz_convert_color(ctx, colorspace, color, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params);
	return
		(fz_clampi(rgb[0] * 255, 0, 255) << 16) |
		(fz_clampi(rgb[1] * 255, 0, 255) << 8) |
		(fz_clampi(rgb[2] * 255, 0, 255));
}

static void
fz_stext_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext)
		return;
	tdev->color = hexrgb_from_color(ctx, colorspace, color);
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext)
		return;
	tdev->color = hexrgb_from_color(ctx, colorspace, color);
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext)
		return;
	tdev->color = 0;
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext)
		return;
	tdev->color = 0;
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext)
		return;
	tdev->color = 0;
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_begin_metatext(fz_context *ctx, fz_device *dev, fz_metatext meta, const char *text)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	metatext_t *mt = fz_malloc_struct(ctx, metatext_t);

	mt->prev = tdev->metatext;
	tdev->metatext = mt;
	mt->type = meta;
	mt->text = fz_strdup(ctx, text);
	mt->bounds = fz_empty_rect;
}

static void
pop_metatext(fz_context *ctx, fz_stext_device *dev)
{
	metatext_t *prev;
	fz_rect bounds;

	if (!dev->metatext)
		return;

	prev = dev->metatext->prev;
	bounds = dev->metatext->bounds;
	fz_free(ctx, dev->metatext->text);
	fz_free(ctx, dev->metatext);
	dev->metatext = prev;
	if (prev)
		prev->bounds = fz_union_rect(prev->bounds, bounds);
}

static void
fz_stext_end_metatext(fz_context *ctx, fz_device *dev)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_font *myfont = NULL;

	if (!tdev->metatext)
		return; /* Mismatched pop. Live with it. */

	if (tdev->metatext->type != FZ_METATEXT_ACTUALTEXT)
	{
		/* We only deal with ActualText here. Just pop anything else off,
		 * and we're done. */
		pop_metatext(ctx, tdev);
		return;
	}

	/* If we have a 'last' text position, send the content after that. */
	if (tdev->last.valid)
	{
		flush_actualtext(ctx, tdev, tdev->metatext->text, 0);
		pop_metatext(ctx, tdev);
		return;
	}

	/* If we have collected a rectangle for content that encloses the actual text,
	 * send the content there. */
	if (!fz_is_empty_rect(tdev->metatext->bounds))
	{
		tdev->last.trm.a = tdev->metatext->bounds.x1 - tdev->metatext->bounds.x0;
		tdev->last.trm.b = 0;
		tdev->last.trm.c = 0;
		tdev->last.trm.d = tdev->metatext->bounds.y1 - tdev->metatext->bounds.y0;
		tdev->last.trm.e = tdev->metatext->bounds.x0;
		tdev->last.trm.f = tdev->metatext->bounds.y0;
	}
	else
		fz_warn(ctx, "Actualtext with no position. Text may be lost or mispositioned.");

	fz_var(myfont);

	fz_try(ctx)
	{
		if (tdev->last.font == NULL)
		{
			myfont = fz_new_base14_font(ctx, "Helvetica");
			tdev->last.font = myfont;
		}
		flush_actualtext(ctx, tdev, tdev->metatext->text, 0);
		pop_metatext(ctx, tdev);
	}
	fz_always(ctx)
	{
		if (myfont)
		{
			tdev->last.font = NULL;
			fz_drop_font(ctx, myfont);
		}
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}


/* Images and shadings */

static void
fz_stext_fill_image(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_rect *bounds = actualtext_bounds(tdev);

	/* Unless we are being told to preserve images OR we are in an actualtext, nothing to do here. */
	if ((tdev->opts.flags & FZ_STEXT_PRESERVE_IMAGES) == 0 && bounds == NULL)
		return;

	/* If the alpha is less than 50% then it's probably a watermark or effect or something. Skip it. */
	if (alpha >= 0.5f)
	add_image_block_to_page(ctx, tdev->page, ctm, img);

	/* If there is an actualtext in force, update its bounds. */
	if (bounds)
	{
		static const fz_rect unit = { 0, 0, 1, 1 };
		*bounds = fz_union_rect(*bounds, fz_transform_rect(unit, ctm));
	}
}

static void
fz_stext_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm,
		fz_colorspace *cspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_stext_fill_image(ctx, dev, img, ctm, alpha, color_params);
}

static fz_image *
fz_new_image_from_shade(fz_context *ctx, fz_shade *shade, fz_matrix *in_out_ctm, fz_color_params color_params, fz_rect scissor)
{
	fz_matrix ctm = *in_out_ctm;
	fz_pixmap *pix;
	fz_image *img = NULL;
	fz_rect bounds;
	fz_irect bbox;

	bounds = fz_bound_shade(ctx, shade, ctm);
	bounds = fz_intersect_rect(bounds, scissor);
	bbox = fz_irect_from_rect(bounds);

	pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), bbox, NULL, !shade->use_background);
	fz_try(ctx)
	{
		if (shade->use_background)
			fz_fill_pixmap_with_color(ctx, pix, shade->colorspace, shade->background, color_params);
		else
			fz_clear_pixmap(ctx, pix);
		fz_paint_shade(ctx, shade, NULL, ctm, pix, color_params, bbox, NULL, NULL);
		img = fz_new_image_from_pixmap(ctx, pix, NULL);
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pix);
	fz_catch(ctx)
		fz_rethrow(ctx);

	in_out_ctm->a = pix->w;
	in_out_ctm->b = 0;
	in_out_ctm->c = 0;
	in_out_ctm->d = pix->h;
	in_out_ctm->e = pix->x;
	in_out_ctm->f = pix->y;
	return img;
}

static void
fz_stext_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_rect *bounds = actualtext_bounds(tdev);
	fz_matrix local_ctm;
	fz_rect scissor;
	fz_image *image;

	/* Unless we are preserving image, OR we are in an actualtext, nothing to do here. */
	if ((tdev->opts.flags & FZ_STEXT_PRESERVE_IMAGES) == 0 && bounds == NULL)
		return;

	local_ctm = ctm;
	scissor = fz_device_current_scissor(ctx, dev);
	image = fz_new_image_from_shade(ctx, shade, &local_ctm, color_params, scissor);
	fz_try(ctx)
		fz_stext_fill_image(ctx, dev, image, local_ctm, alpha, color_params);
	fz_always(ctx)
		fz_drop_image(ctx, image);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fz_stext_close_device(fz_context *ctx, fz_device *dev)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_stext_page *page = tdev->page;
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_TEXT)
			continue;
		for (line = block->u.t.first_line; line; line = line->next)
		{
			int reorder = 0;
			for (ch = line->first_char; ch; ch = ch->next)
			{
				fz_rect ch_box = fz_rect_from_quad(ch->quad);
				if (ch == line->first_char)
					line->bbox = ch_box;
				else
					line->bbox = fz_union_rect(line->bbox, ch_box);
				if (ch->bidi == 3)
					reorder = 1;
			}
			block->bbox = fz_union_rect(block->bbox, line->bbox);
			if (reorder)
				reverse_bidi_line(line);
		}
	}

	/* TODO: smart sorting of blocks and lines in reading order */
	/* TODO: unicode NFC normalization */
}

static void
fz_stext_drop_device(fz_context *ctx, fz_device *dev)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_drop_text(ctx, tdev->lasttext);
	fz_drop_font(ctx, tdev->last.font);
	while (tdev->metatext)
		pop_metatext(ctx, tdev);
}

fz_stext_options *
fz_parse_stext_options(fz_context *ctx, fz_stext_options *opts, const char *string)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	if (fz_has_option(ctx, string, "preserve-ligatures", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_PRESERVE_LIGATURES;
	if (fz_has_option(ctx, string, "preserve-whitespace", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_PRESERVE_WHITESPACE;
	if (fz_has_option(ctx, string, "preserve-images", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_PRESERVE_IMAGES;
	if (fz_has_option(ctx, string, "inhibit-spaces", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_INHIBIT_SPACES;
	if (fz_has_option(ctx, string, "dehyphenate", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_DEHYPHENATE;
	if (fz_has_option(ctx, string, "preserve-spans", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_PRESERVE_SPANS;

	opts->flags |= FZ_STEXT_MEDIABOX_CLIP;
	if (fz_has_option(ctx, string, "mediabox-clip", &val) && fz_option_eq(val, "no"))
		opts->flags ^= FZ_STEXT_MEDIABOX_CLIP;

	opts->scale = 1;
	if (fz_has_option(ctx, string, "resolution", &val))
		opts->scale = fz_atof(val) / 96.0f; /* HTML base resolution is 96ppi */

	return opts;
}

static void
fz_stext_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *ss, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp)
{
	fz_rect *bounds = actualtext_bounds((fz_stext_device *)dev);

	if (bounds == NULL)
		return;

	*bounds = fz_union_rect(*bounds, fz_bound_path(ctx, path, ss, ctm));
}

static void
fz_stext_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp)
{
	fz_stext_stroke_path(ctx, dev, path, NULL, ctm, cs, color, alpha, cp);
}

fz_device *
fz_new_stext_device(fz_context *ctx, fz_stext_page *page, const fz_stext_options *opts)
{
	fz_stext_device *dev = fz_new_derived_device(ctx, fz_stext_device);

	dev->super.close_device = fz_stext_close_device;
	dev->super.drop_device = fz_stext_drop_device;

	dev->super.fill_text = fz_stext_fill_text;
	dev->super.stroke_text = fz_stext_stroke_text;
	dev->super.clip_text = fz_stext_clip_text;
	dev->super.clip_stroke_text = fz_stext_clip_stroke_text;
	dev->super.ignore_text = fz_stext_ignore_text;
	dev->super.begin_metatext = fz_stext_begin_metatext;
	dev->super.end_metatext = fz_stext_end_metatext;

	dev->super.fill_path = fz_stext_fill_path;
	dev->super.stroke_path = fz_stext_stroke_path;

		dev->super.fill_shade = fz_stext_fill_shade;
		dev->super.fill_image = fz_stext_fill_image;
		dev->super.fill_image_mask = fz_stext_fill_image_mask;

	if (opts)
		dev->flags = opts->flags;
	dev->page = page;
	dev->pen.x = 0;
	dev->pen.y = 0;
	dev->trm = fz_identity;
	dev->lastchar = ' ';
	dev->lasttext = NULL;
	dev->lastbidi = 0;
	if (opts)
		dev->opts = *opts;

	return (fz_device*)dev;
}
