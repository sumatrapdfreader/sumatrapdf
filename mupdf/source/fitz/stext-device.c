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
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include "glyphbox.h"

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
#define FAKE_BOLD_MAX_DIST 0.1f

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
	fz_point from;
	fz_point to;
	float thickness;
} rect_details;

typedef struct
{
	fz_device super;
	fz_stext_page *page;
	int id;
	fz_point pen, start;
	fz_point lag_pen;
	fz_matrix trm;
	int new_obj;
	int lastchar;
	fz_stext_line *lastline;
	int lastbidi;
	int flags;
	int color;
	int last_was_fake_bold;
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
		int flags;
	} last;

	/* The list of 'rects' seen during processing (if we're collecting styles). */
	int rect_max;
	int rect_len;
	rect_details *rects;
} fz_stext_device;

const char *fz_stext_options_usage =
	"Structured text options:\n"
	"\tpreserve-images: keep images in output\n"
	"\tpreserve-ligatures: do not expand ligatures into constituent characters\n"
	"\tpreserve-spans: do not merge spans on the same line\n"
	"\tpreserve-whitespace: do not convert all whitespace into space characters\n"
	"\tinhibit-spaces: don't add spaces between gaps in the text\n"
	"\tparagraph-break: break blocks at paragraph boundaries\n"
	"\tdehyphenate: attempt to join up hyphenated words\n"
	"\tignore-actualtext: do not apply ActualText replacements\n"
	"\tuse-cid-for-unknown-unicode: use character code if unicode mapping fails\n"
	"\tuse-gid-for-unknown-unicode: use glyph index if unicode mapping fails\n"
	"\taccurate-bboxes: calculate char bboxes from the outlines\n"
	"\taccurate-ascenders: calculate ascender/descender from font glyphs\n"
	"\taccurate-side-bearings: expand char bboxes to completely include width of glyphs\n"
	"\tcollect-styles: attempt to detect text features (fake bold, strikeout, underlined etc)\n"
	"\tclip: do not include text that is completely clipped\n"
	"\tclip-rect=x0:y0:x1:y1 specify clipping rectangle within which to collect content\n"
	"\tstructured: collect structure markup\n"
	"\tvectors: include vector bboxes in output\n"
	"\tsegment: attempt to segment the page\n"
	"\ttable-hunt: hunt for tables within a (segmented) page\n"
	"\tresolution: resolution to render at\n"
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
		page->refs = 1;
		page->pool = pool;
		page->mediabox = mediabox;
		page->first_block = NULL;
		page->last_block = NULL;
		page->id_list = fz_new_pool_array(ctx, pool, fz_stext_page_details, 4);
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
		fz_rethrow(ctx);
	}
	return page;
}

static void
drop_run(fz_context *ctx, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;
	while (block)
	{
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_IMAGE:
			fz_drop_image(ctx, block->u.i.image);
			break;
		case FZ_STEXT_BLOCK_TEXT:
			for (line = block->u.t.first_line; line; line = line->next)
				for (ch = line->first_char; ch; ch = ch->next)
					fz_drop_font(ctx, ch->font);
			break;
		case FZ_STEXT_BLOCK_STRUCT:
			drop_run(ctx, block->u.s.down->first_block);
			break;
		default:
			break;
		}
		block = block->next;
	}
}

fz_stext_page_details *fz_stext_page_details_for_block(fz_context *ctx, fz_stext_page *page, fz_stext_block *block)
{
	if (block == NULL || page == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "page details require a page and a block");

	return (fz_stext_page_details *)fz_pool_array_lookup(ctx, page->id_list, block->id);
}

fz_stext_page *
fz_keep_stext_page(fz_context *ctx, fz_stext_page *page)
{
	return fz_keep_imp(ctx, page, &page->refs);
}

void
fz_drop_stext_page(fz_context *ctx, fz_stext_page *page)
{
	if (page == NULL)
		return;

	if (fz_drop_imp(ctx, page, &page->refs))
	{
		drop_run(ctx, page->first_block);
		fz_drop_pool(ctx, page->pool);
	}
}

/*
 * This adds a new block at the end of the page. This should not be used
 * to add 'struct' blocks to the page as those have to be added internally,
 * with more complicated pointer setup.
 */
static fz_stext_block *
add_block_to_page(fz_context *ctx, fz_stext_page *page, int type, int id)
{
	fz_stext_block *block = fz_pool_alloc(ctx, page->pool, sizeof *page->first_block);
	block->bbox = fz_empty_rect; /* Fixes bug 703267. */
	block->prev = page->last_block;
	block->type = type;
	block->id = id;
	if (page->last_struct)
	{
		if (page->last_struct->last_block)
		{
			block->prev = page->last_struct->last_block;
			block->prev->next = block;
			page->last_struct->last_block = block;
		}
		else
			page->last_struct->last_block = page->last_struct->first_block = block;
	}
	else if (!page->last_block)
	{
		page->last_block = block;
		if (!page->first_block)
			page->first_block = block;
	}
	else
	{
		page->last_block->next = block;
		page->last_block = block;
	}
	return block;
}

static fz_stext_block *
add_text_block_to_page(fz_context *ctx, fz_stext_page *page, int id)
{
	return add_block_to_page(ctx, page, FZ_STEXT_BLOCK_TEXT, id);
}

static fz_stext_block *
add_image_block_to_page(fz_context *ctx, fz_stext_page *page, fz_matrix ctm, fz_image *image, int id)
{
	fz_stext_block *block = add_block_to_page(ctx, page, FZ_STEXT_BLOCK_IMAGE, id);
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

#define NON_ACCURATE_GLYPH_ADDED_SPACE (-2)
#define NON_ACCURATE_GLYPH (-1)

static fz_stext_char *
add_char_to_line(fz_context *ctx, fz_stext_page *page, fz_stext_line *line, fz_matrix trm, fz_font *font, float size, int c, int glyph, fz_point *p, fz_point *q, int bidi, int color, int synthetic, int flags, int dev_flags)
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
	ch->argb = color;
	ch->bidi = bidi;
	ch->origin = *p;
	ch->size = size;
	ch->font = fz_keep_font(ctx, font);
	ch->flags = flags | (synthetic ? FZ_STEXT_SYNTHETIC : 0) | (synthetic > 1 ? FZ_STEXT_SYNTHETIC_LARGE : 0);
	if (font->flags.is_bold)
		ch->flags |= FZ_STEXT_BOLD;

	if (line->wmode == 0)
	{
		fz_rect bounds;
		int bounded = 0;
		a.x = 0;
		d.x = 0;
		if (glyph == NON_ACCURATE_GLYPH_ADDED_SPACE)
		{
			/* Added space, in accurate mode. */
			a.y = d.y = 0;
		}
		else if (glyph == NON_ACCURATE_GLYPH)
		{
			/* Non accurate mode. */
			a.y = fz_font_ascender(ctx, font);
			d.y = fz_font_descender(ctx, font);
		}
		else
		{
			/* Any glyph in accurate mode */
			bounds = fz_bound_glyph(ctx, font, glyph, fz_identity);
			bounded = 1;
			a.y = bounds.y1;
			d.y = bounds.y0;
		}
		if (dev_flags & FZ_STEXT_ACCURATE_SIDE_BEARINGS)
		{
			if (!bounded)
				bounds = fz_bound_glyph(ctx, font, glyph, fz_identity);
			if (a.x > bounds.x0)
				a.x = bounds.x0;
			if (d.y < bounds.x1)
				d.y = bounds.x1;
		}
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

int fz_is_unicode_hyphen(int c)
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

#define FAKEBOLD_THRESHOLD_RECIP (1.0f / FAKE_BOLD_MAX_DIST)

static int
is_within_fake_bold_distance(float a, float b, float size)
{
	a -= b;
	if (a < 0)
		a = -a;

	return FAKEBOLD_THRESHOLD_RECIP * a < size;
}

static int
font_equiv(fz_context *ctx, fz_font *f, fz_font *g)
{
	unsigned char fdigest[16];
	unsigned char gdigest[16];

	if (f == g)
		return 1;

	if (strcmp(f->name, g->name) != 0)
		return 0;

	if (f->buffer == NULL || g->buffer == NULL)
		return 0;

	fz_font_digest(ctx, f, fdigest);
	fz_font_digest(ctx, g, gdigest);

	return (memcmp(fdigest, gdigest, 16) == 0);
}

static int
check_for_fake_bold(fz_context *ctx, fz_stext_block *block, fz_font *font, int c, fz_point p, float size, int flags)
{
	fz_stext_line *line;
	fz_stext_char *ch;

	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down != NULL && check_for_fake_bold(ctx, block->u.s.down->first_block, font, c, p, size, flags))
				return 1;
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (line = block->u.t.first_line; line != NULL; line = line->next)
			{
				fz_stext_char *pr = NULL;
				for (ch = line->first_char; ch != NULL; ch = ch->next)
				{
					/* Not perfect, but it'll do! */
					if (ch->c == c && is_within_fake_bold_distance(ch->origin.x, p.x, size) && is_within_fake_bold_distance(ch->origin.y, p.y, size) && font_equiv(ctx, ch->font, font))
					{
						/* If we were filled before, and we are stroking now... */
						if ((ch->flags & (FZ_STEXT_FILLED | FZ_STEXT_STROKED)) == FZ_STEXT_FILLED &&
							(flags & (FZ_STEXT_FILLED | FZ_STEXT_STROKED)) == FZ_STEXT_STROKED)
						{
							/* Update this to be filled + stroked, but don't specifically mark it as fake bold. */
							ch->flags |= flags;
							return 1;
						}
						/* Overlaying spaces is tricksy. How can that count as boldening when it doesn't mark? We only accept these
						 * as boldening if either the char before, or the char after were also boldened. */
						ch->flags |= flags;

						if (c == ' ')
						{
							if ((pr && (pr->flags & FZ_STEXT_BOLD) != 0) ||
								(ch->next && (ch->next->flags & FZ_STEXT_BOLD) != 0))
							{
								/* OK, we can be bold. */
								ch->flags |= FZ_STEXT_BOLD;
								return 1;
							}
							/* Ignore this and keep going */
						}
						else
						{
							ch->flags |= FZ_STEXT_BOLD;
							return 1;
						}
					}
					pr = ch;
				}
			}
		}
	}

	return 0;
}

static void
fz_add_stext_char_imp(fz_context *ctx, fz_stext_device *dev, fz_font *font, int c, int glyph, fz_matrix trm, float adv, int wmode, int bidi, int force_new_line, int flags)
{
	fz_stext_page *page = dev->page;
	fz_stext_block *cur_block;
	fz_stext_line *cur_line = NULL;

	int new_para = 0;
	int new_line = 1;
	int add_space = 0;
	fz_point dir, ndir, p, q;
	float size;
	fz_point delta;
	float spacing = 0;
	float base_offset = 0;
	float dist;

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

	if ((dev->opts.flags & FZ_STEXT_COLLECT_STYLES) != 0)
	{
		if (glyph == -1)
		{
			if (dev->last_was_fake_bold)
				goto move_pen_and_exit;
		}
		else if (check_for_fake_bold(ctx, page->first_block, font, c, p, size, flags))
		{
			dev->last_was_fake_bold = 1;
			goto move_pen_and_exit;
		}
		dev->last_was_fake_bold = 0;
	}

	/* Find current position to enter new text. */
	cur_block = page->last_struct ? page->last_struct->last_block : page->last_block;
	if (cur_block && cur_block->type != FZ_STEXT_BLOCK_TEXT)
		cur_block = NULL;
	cur_line = cur_block ? cur_block->u.t.last_line : NULL;

	if (cur_line && glyph < 0)
	{
		/* Don't advance pen or break lines for no-glyph characters in a cluster */
		add_char_to_line(ctx, page, cur_line, trm, font, size, c, (dev->flags & FZ_STEXT_ACCURATE_BBOXES) ? glyph : NON_ACCURATE_GLYPH, &dev->pen, &dev->pen, bidi, dev->color, 0, flags, dev->flags);
		dev->lastbidi = bidi;
		dev->lastchar = c;
		dev->lastline = cur_line;
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
		/* Largely supplanted by the check_for_fake_bold mechanism above,
		 * but we leave this in for backward compatibility as it's cheap,
		 * and works even when FZ_STEXT_COLLECT_STYLES is not set. */
		dist = hypotf(q.x - dev->pen.x, q.y - dev->pen.y) / size;
		if (dist < FAKE_BOLD_MAX_DIST && c == dev->lastchar)
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
				else if (spacing < 0 && spacing > -SPACE_MAX_DIST)
				{
					/* Motion is in line, but negative. We've probably got overlapping
					 * chars here. Live with it. */
					new_line = 0;
				}
				else if (spacing > 0 && spacing < SPACE_MAX_DIST)
				{
					bidi = 3; /* mark line as visual */
					if (wmode == 0 && may_add_space(dev->lastchar))
						add_space = 1 + (spacing > SPACE_DIST*2);
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
				else if (spacing < 0 && spacing > -SPACE_MAX_DIST)
				{
					/* Motion is in line, but negative. We've probably got overlapping
					 * chars here. Live with it. */
					new_line = 0;
				}
				else if (spacing > 0 && spacing < SPACE_MAX_DIST)
				{
					/* Motion is forward in line and large enough to warrant us adding a space. */
					if (wmode == 0 && may_add_space(dev->lastchar))
						add_space = 1 + (spacing > SPACE_DIST*2);
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
				if ((p.x - dev->start.x) > 0.5f)
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
		cur_block = add_text_block_to_page(ctx, page, dev->id);
		cur_line = cur_block->u.t.last_line;
	}

	if (new_line && (dev->flags & FZ_STEXT_DEHYPHENATE) && fz_is_unicode_hyphen(dev->lastchar) && dev->lastline != NULL)
		dev->lastline->flags |= FZ_STEXT_LINE_FLAGS_JOINED;

	/* Start a new line */
	if (new_line || !cur_line || force_new_line)
	{
		cur_line = add_line_to_block(ctx, page, cur_block, &ndir, wmode, bidi);
		dev->start = p;
	}

	/* Add synthetic space */
	if (add_space && !(dev->flags & FZ_STEXT_INHIBIT_SPACES))
		add_char_to_line(ctx, page, cur_line, trm, font, size, ' ', (dev->flags & FZ_STEXT_ACCURATE_BBOXES) ? NON_ACCURATE_GLYPH_ADDED_SPACE : NON_ACCURATE_GLYPH, &dev->pen, &p, bidi, dev->color, add_space, flags, dev->flags);

	add_char_to_line(ctx, page, cur_line, trm, font, size, c, (dev->flags & FZ_STEXT_ACCURATE_BBOXES) ? glyph : NON_ACCURATE_GLYPH, &p, &q, bidi, dev->color, 0, flags, dev->flags);

move_pen_and_exit:
	dev->lastchar = c;
	dev->lastbidi = bidi;
	dev->lastline = cur_line;
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
	int force_new_line,
	int flags)
{
	/* ignore when one unicode character maps to multiple glyphs */
	if (c == -1)
		return;

	if (dev->flags & FZ_STEXT_ACCURATE_ASCENDERS)
		fz_calculate_font_ascender_descender(ctx, font);

	if (!(dev->flags & FZ_STEXT_PRESERVE_LIGATURES))
	{
		switch (c)
		{
		case 0xFB00: /* ff */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'f', -1, trm, 0, wmode, bidi, 0, flags);
			return;
		case 0xFB01: /* fi */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'i', -1, trm, 0, wmode, bidi, 0, flags);
			return;
		case 0xFB02: /* fl */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'l', -1, trm, 0, wmode, bidi, 0, flags);
			return;
		case 0xFB03: /* ffi */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'f', -1, trm, 0, wmode, bidi, 0, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'i', -1, trm, 0, wmode, bidi, 0, flags);
			return;
		case 0xFB04: /* ffl */
			fz_add_stext_char_imp(ctx, dev, font, 'f', glyph, trm, adv, wmode, bidi, force_new_line, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'f', -1, trm, 0, wmode, bidi, 0, flags);
			fz_add_stext_char_imp(ctx, dev, font, 'l', -1, trm, 0, wmode, bidi, 0, flags);
			return;
		case 0xFB05: /* long st */
		case 0xFB06: /* st */
			fz_add_stext_char_imp(ctx, dev, font, 's', glyph, trm, adv, wmode, bidi, force_new_line, flags);
			fz_add_stext_char_imp(ctx, dev, font, 't', -1, trm, 0, wmode, bidi, 0, flags);
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

	fz_add_stext_char_imp(ctx, dev, font, c, glyph, trm, adv, wmode, bidi, force_new_line, flags);
}

static fz_rect
current_clip(fz_context *ctx, fz_stext_device *dev)
{
	fz_rect r = fz_infinite_rect;

	if (dev->flags & FZ_STEXT_CLIP)
	{
		r = fz_device_current_scissor(ctx, &dev->super);
		r = fz_intersect_rect(r, dev->page->mediabox);
	}
	if (dev->flags & FZ_STEXT_CLIP_RECT)
		r = fz_intersect_rect(r, dev->opts.clip);

	return r;
}

static void
do_extract(fz_context *ctx, fz_stext_device *dev, fz_text_span *span, fz_matrix ctm, int start, int end, int flags)
{
	fz_font *font = span->font;
	fz_matrix tm = span->trm;
	float adv;
	int unicode;
	int i;

	for (i = start; i < end; i++)
	{
		if (dev->flags & (FZ_STEXT_CLIP | FZ_STEXT_CLIP_RECT))
		{
			fz_rect r = current_clip(ctx, dev);
			if (fz_glyph_entirely_outside_box(ctx, &ctm, span, &span->items[i], &r))
			{
				dev->last.clipped = 1;
				continue;
			}
		}
		dev->last.clipped = 0;

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
		dev->last.flags = flags;

		/* Calculate bounding box and new pen position based on font metrics */
		if (span->items[i].gid >= 0)
			adv = span->items[i].adv;
		else
			adv = 0;

		unicode = span->items[i].ucs;
		if (unicode == FZ_REPLACEMENT_CHARACTER)
		{
			if (dev->flags & FZ_STEXT_USE_CID_FOR_UNKNOWN_UNICODE)
			{
				unicode = span->items[i].cid;
				flags |= FZ_STEXT_UNICODE_IS_CID;
			}
			else if (dev->flags & FZ_STEXT_USE_GID_FOR_UNKNOWN_UNICODE)
			{
				unicode = span->items[i].gid;
				flags |= FZ_STEXT_UNICODE_IS_GID;
			}
		}

		/* Send the chars we have through. */
		fz_add_stext_char(ctx, dev, font,
			unicode,
			span->items[i].gid,
			dev->last.trm,
			adv,
			dev->last.wmode,
			dev->last.bidi_level,
			(i == 0) && (dev->flags & FZ_STEXT_PRESERVE_SPANS),
			flags);
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
flush_actualtext(fz_context *ctx, fz_stext_device *dev, const char *actualtext, int i, int end)
{
	if (*actualtext == 0)
		return;

	/* SumatraPFD: https://github.com/sumatrapdfreader/sumatrapdf/issues/5280 */
	if (!dev->last.valid)
		return;

	if (dev->flags & (FZ_STEXT_CLIP | FZ_STEXT_CLIP_RECT))
		if (dev->last.clipped)
			return;

	while (end < 0 || (end >= 0 && i < end))
	{
		int rune;
		actualtext += fz_chartorune(&rune, actualtext);

		if (rune == 0)
			break;

		fz_add_stext_char(ctx, dev, dev->last.font,
			rune,
			-1,
			dev->last.trm,
			0,
			dev->last.wmode,
			dev->last.bidi_level,
			(i == 0) && (dev->flags & FZ_STEXT_PRESERVE_SPANS),
			dev->last.flags);
		i++;
	}
}

static void
do_extract_within_actualtext(fz_context *ctx, fz_stext_device *dev, fz_text_span *span, fz_matrix ctm, metatext_t *mt, int flags)
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
		if (span->items[start].ucs != rune || rune == 0)
			break;
		actualtext += len; z--;
	}
	if (start != 0)
		do_extract(ctx, dev, span, ctm, 0, start, flags);

	if (start == span->len)
	{
		/* The prefix has consumed all this object. Just shorten the actualtext and we'll
		 * catch the rest next time. */
		z = strlen(actualtext)+1;
		memmove(mt->text, actualtext, z);
		return;
	}

	/* We haven't consumed the whole string, so there must be runes left.
	 * Shut coverity up. */
	assert(z != 0);

	/* Spot a matching postfix. Can't send it til the end. */
	for (end = span->len; end > start; end--)
	{
		/* Nasty n^2 algo here, cos backtracking through utf8 is not trivial. It'll do. */
		int rune = rune_index(actualtext, z-1);
		if (span->items[end-1].ucs != rune)
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

		if (dev->flags & (FZ_STEXT_CLIP | FZ_STEXT_CLIP_RECT))
		{
			fz_rect r = current_clip(ctx, dev);
			if (fz_glyph_entirely_outside_box(ctx, &ctm, span, &span->items[i], &r))
			{
				dev->last.clipped = 1;
				continue;
			}
		}
		dev->last.clipped = 0;

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
		dev->last.flags = flags;

		/* Calculate bounding box and new pen position based on font metrics */
		if (item->gid >= 0)
			adv = item->adv;
		else
			adv = 0;

		fz_add_stext_char(ctx, dev, font,
			rune,
			span->items[i].gid,
			dev->last.trm,
			adv,
			dev->last.wmode,
			dev->last.bidi_level,
			(i == 0) && (dev->flags & FZ_STEXT_PRESERVE_SPANS),
			flags);
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
	flush_actualtext(ctx, dev, actualtext, i, i + strlen(actualtext) - (span->len - end));

	/* Send the postfix */
	if (end != span->len)
		do_extract(ctx, dev, span, ctm, end, span->len, flags);

	mt->text[0] = 0;
}

static void
fz_stext_extract(fz_context *ctx, fz_stext_device *dev, fz_text_span *span, fz_matrix ctm, int flags)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	metatext_t *mt = NULL;

	if (span->len == 0)
		return;

	/* Are we in an actualtext? */
	if (!(tdev->opts.flags & FZ_STEXT_IGNORE_ACTUALTEXT))
		mt = find_actualtext(dev);

	if (mt)
		do_extract_within_actualtext(ctx, dev, span, ctm, mt, flags);
	else
		do_extract(ctx, dev, span, ctm, 0, span->len, flags);
}

static uint32_t hexrgba_from_color(fz_context *ctx, fz_colorspace *colorspace, const float *color, float alpha)
{
	float rgb[3];
	fz_convert_color(ctx, colorspace, color, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params);
	return
		(((uint32_t) fz_clampi(alpha * 255 + 0.5f, 0, 255)) << 24) |
		(((uint32_t) fz_clampi(rgb[0] * 255 + 0.5f, 0, 255)) << 16) |
		(((uint32_t) fz_clampi(rgb[1] * 255 + 0.5f, 0, 255)) << 8) |
		(((uint32_t) fz_clampi(rgb[2] * 255 + 0.5f, 0, 255)));
}

static void
fz_stext_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext && (tdev->opts.flags & FZ_STEXT_COLLECT_STYLES) == 0)
		return;
	tdev->color = hexrgba_from_color(ctx, colorspace, color, alpha);
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm, FZ_STEXT_FILLED);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext && (tdev->opts.flags & FZ_STEXT_COLLECT_STYLES) == 0)
		return;
	tdev->color = hexrgba_from_color(ctx, colorspace, color, alpha);
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm, FZ_STEXT_STROKED);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext && (tdev->opts.flags & FZ_STEXT_COLLECT_STYLES) == 0)
		return;
	tdev->color = 0;
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm, FZ_STEXT_FILLED | FZ_STEXT_CLIPPED);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext && (tdev->opts.flags & FZ_STEXT_COLLECT_STYLES) == 0)
		return;
	tdev->color = 0;
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm, FZ_STEXT_STROKED | FZ_STEXT_CLIPPED);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_text_span *span;
	if (text == tdev->lasttext && (tdev->opts.flags & FZ_STEXT_COLLECT_STYLES) == 0)
		return;
	tdev->color = 0;
	tdev->new_obj = 1;
	for (span = text->head; span; span = span->next)
		fz_stext_extract(ctx, tdev, span, ctm, 0);
	fz_drop_text(ctx, tdev->lasttext);
	tdev->lasttext = fz_keep_text(ctx, text);
}

static void
fz_stext_begin_metatext(fz_context *ctx, fz_device *dev, fz_metatext meta, const char *text)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	metatext_t *mt = find_actualtext(tdev);

	if (mt != NULL && meta == FZ_METATEXT_ACTUALTEXT)
		flush_actualtext(ctx, tdev, mt->text, 0, -1);

	if (meta == FZ_METATEXT_ACTUALTEXT)
		tdev->last.valid = 0;

	mt = fz_malloc_struct(ctx, metatext_t);

	mt->prev = tdev->metatext;
	tdev->metatext = mt;
	mt->type = meta;
	mt->text = text ? fz_strdup(ctx, text) : NULL;
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

	if (tdev->metatext->type != FZ_METATEXT_ACTUALTEXT || (tdev->opts.flags & FZ_STEXT_IGNORE_ACTUALTEXT) != 0)
	{
		/* We only deal with ActualText here. Just pop anything else off,
		 * and we're done. */
		pop_metatext(ctx, tdev);
		return;
	}

	/* If we have a 'last' text position, send the content after that. */
	if (tdev->last.valid)
	{
		flush_actualtext(ctx, tdev, tdev->metatext->text, 0, -1);
		pop_metatext(ctx, tdev);
		tdev->last.valid = 0;
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
	{
		if ((dev->flags & (FZ_STEXT_CLIP | FZ_STEXT_CLIP_RECT)) == 0)
		fz_warn(ctx, "Actualtext with no position. Text may be lost or mispositioned.");
		pop_metatext(ctx, tdev);
		return;
	}

	fz_var(myfont);

	fz_try(ctx)
	{
		if (tdev->last.font == NULL)
		{
			myfont = fz_new_base14_font(ctx, "Helvetica");
			tdev->last.font = myfont;
		}
		flush_actualtext(ctx, tdev, tdev->metatext->text, 0, -1);
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

	/* If there is an actualtext in force, update its bounds. */
	if (bounds)
	{
		static const fz_rect unit = { 0, 0, 1, 1 };
		*bounds = fz_union_rect(*bounds, fz_transform_rect(unit, ctm));
	}

	/* Unless we are being told to preserve images, nothing to do here. */
	if ((tdev->opts.flags & FZ_STEXT_PRESERVE_IMAGES) == 0)
		return;

	/* If the alpha is less than 50% then it's probably a watermark or effect or something. Skip it. */
	if (alpha >= 0.5f)
		add_image_block_to_page(ctx, tdev->page, ctm, img, tdev->id);

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
	fz_matrix local_ctm;
	fz_rect scissor;
	fz_image *image;

	/* If we aren't preserving images, don't waste time making the shade. */
	if ((tdev->opts.flags & FZ_STEXT_PRESERVE_IMAGES) == 0)
	{
		/* But we do still need to handle actualtext bounds. */
		fz_rect *bounds = actualtext_bounds(tdev);
		if (bounds)
		*bounds = fz_union_rect(*bounds, fz_bound_shade(ctx, shade, ctm));
		return;
	}

	local_ctm = ctm;
	scissor = fz_device_current_scissor(ctx, dev);
	if (dev->flags & FZ_STEXT_CLIP_RECT)
		scissor = fz_intersect_rect(scissor, tdev->opts.clip);
	scissor = fz_intersect_rect(scissor, tdev->page->mediabox);
	image = fz_new_image_from_shade(ctx, shade, &local_ctm, color_params, scissor);
	fz_try(ctx)
		fz_stext_fill_image(ctx, dev, image, local_ctm, alpha, color_params);
	fz_always(ctx)
		fz_drop_image(ctx, image);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fixup_bboxes_and_bidi(fz_context *ctx, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;

	for ( ; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
			if (block->u.s.down)
				fixup_bboxes_and_bidi(ctx, block->u.s.down->first_block);
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
}

static void
advance_to_x(fz_point *a, fz_point b, float x)
{
	a->y += (b.y - a->y) * (x - a->x) / (b.x - a->x);
	a->x = x;
}

static void
advance_to_y(fz_point *a, fz_point b, float y)
{
	a->x += (b.x - a->x) * (y - a->y) / (b.y - a->y);
	a->y = y;
}

static int
line_crosses_rect(fz_point a, fz_point b, fz_rect r)
{
	/* Cope with trivial exclusions */
	if (a.x < r.x0 && b.x < r.x0)
		return 0;
	if (a.x > r.x1 && b.x > r.x1)
		return 0;
	if (a.y < r.y0 && b.y < r.y0)
		return 0;
	if (a.y > r.y1 && b.y > r.y1)
		return 0;

	if (a.x < r.x0)
		advance_to_x(&a, b, r.x0);
	if (a.x > r.x1)
		advance_to_x(&a, b, r.x1);
	if (a.y < r.y0)
		advance_to_y(&a, b, r.y0);
	if (a.y > r.y1)
		advance_to_y(&a, b, r.y1);

	return fz_is_point_inside_rect(a, r);
}

static float
calculate_ascent(fz_point p, fz_point origin, fz_point dir)
{
	return fabsf((origin.x-p.x)*dir.y - (origin.y-p.y)*dir.x);
}

/* Create us a rect from the given quad, but extend it downwards
 * to allow for underlines that pass under the glyphs. */
static fz_rect expanded_rect_from_quad(fz_quad quad, fz_point dir, fz_point origin, float size)
{
	/* Consider the two rects from A and g respectively.
	 *
	 * ul +------+ ur   or
	 *    |  /\  |         ul +------+ ur
	 *    | /__\ |            | /''\ |
	 *    |/    \|            |(    ||
	 * ll +------+ lr         | ''''||
	 *                        |  ''' | <-expected underline level
	 *                     ll +------+ lr
	 *
	 * So an underline won't cross A's rect, but will cross g's.
	 * We want to make a rect that includes a suitable amount of
	 * space underneath. The information we have available to us
	 * is summed up here:
	 *
	 *  ul +---------+ ur
	 *     |         |
	 *     | origin  |
	 *     |+----------> dir
	 *     |         |
	 *  ll +---------+ lr
	 *
	 * Consider the distance from ul to the line that passes through
	 * the origin with direction dir. Similarly, consider the distance
	 * from ur to the same line. This can be thought of as the 'ascent'
	 * of this character.
	 *
	 * We'd like the distance from ul to ll to be greater than this, so
	 * as to ensure we cover the possible location where an underline
	 * might reasonably go.
	 *
	 * If we have a line (l) through point A with direction vector u,
	 * the distance between point P and line(l) is:
	 *
	 * d(P,l) = || AP x u || / || u ||
	 *
	 * where x is the cross product.
	 *
	 * For us, because || dir || = 1:
	 *
	 * d(ul, origin) = || (origin-ul) x dir ||
	 *
	 * The cross product is only defined in 3 (or 7!) dimensions, so
	 * extend both vectors into 3d by defining a 0 z component.
	 *
	 * (origin-ul) x dir = [ (origin.y - ul.y) . 0     - 0                 . dir.y ]
	 *                     [ 0                 . dir.x - (origin.x - ul.y) . 0     ]
	 *                     [ (origin.x - ul.x) . dir.y - (origin.y - ul.y) . dir.x ]
	 *
	 * So d(ul, origin) = abs(D) where D = (origin.x-ul.x).dir.y - (origin.y-ul.y).dir.x
	 */
	float ascent = (calculate_ascent(quad.ul, origin, dir) + calculate_ascent(quad.ur, origin, dir)) / 2;
	fz_point left = { quad.ll.x - quad.ul.x, quad.ll.y - quad.ul.y };
	fz_point right = { quad.lr.x - quad.ur.x, quad.lr.y - quad.ur.y };
	float height = (hypotf(left.x, left.y) + hypotf(right.x, right.y))/2;
	int neg = 0;
	float extra_rise = 0;

	/* Spaces will have 0 ascent. underscores will have small ascent.
	 * We want a sane ascent to be able to spot strikeouts, but not
	 * so big that it incorporates lines above the text, like borders. */
	if (ascent < 0.75*size)
		extra_rise = 0.75*size - ascent;

	/* We'd like height to be at least ascent + 1/4 size */
	if (height < 0)
		neg = 1, height = -height;
	if (height < ascent + size * 0.25f)
		height = ascent + size * 0.25f;

	height -= ascent;
	if (neg)
		height = -height;
	quad.ll.x += - height * dir.y;
	quad.ll.y +=   height * dir.x;
	quad.lr.x += - height * dir.y;
	quad.lr.y +=   height * dir.x;
	quad.ul.x -= - extra_rise * dir.y;
	quad.ul.y -=   extra_rise * dir.x;
	quad.ur.x -= - extra_rise * dir.y;
	quad.ur.y -=   extra_rise * dir.x;

	return fz_rect_from_quad(quad);
}

static int feq(float a,float b)
{
#define EPSILON 0.00001
	a -= b;
	if (a < 0)
		a = -a;
	return a < EPSILON;
}

static void
check_strikeout(fz_context *ctx, fz_stext_block *block, fz_point from, fz_point to, fz_point dir, float thickness)
{
	for ( ; block; block = block->next)
	{
		fz_stext_line *line;

		if (block->type != FZ_STEXT_BLOCK_TEXT)
			continue;

		for (line = block->u.t.first_line; line != NULL; line = line->next)
		{
			fz_stext_char *ch;

			if ((!feq(line->dir.x, dir.x) || !feq(line->dir.y, dir.y)) &&
				(!feq(line->dir.x, -dir.x) || !feq(line->dir.y, -dir.y)))
				continue;

			/* Matching directions... */

			/* Unfortunately, we don't have a valid line->bbox at this point, so we need to check
			 * chars. - FIXME: Now we do! */
			for (ch = line->first_char; ch; ch = ch->next)
			{
				fz_point up;
				float dx, dy, dot;
				fz_rect ch_box;

				/* If the thickness is more than a 1/4 of the size, it's a highlight, not a
				 * line! */
				if (ch->size < thickness*4)
					continue;

				ch_box = expanded_rect_from_quad(ch->quad, line->dir, ch->origin, ch->size);

				if (!line_crosses_rect(from, to, ch_box))
					continue;

				/* Is this a strikeout or an underline? */

				/* The baseline moves from ch->origin in the direction line->dir */
				up.x = line->dir.y;
				up.y = -line->dir.x;

				/* How far is our line displaced from the line through the origin? */
				dx = from.x - ch->origin.x;
				dy = from.y - ch->origin.y;
				/* Dot product with up. up is normalised */
				dot = dx * up.x + dy * up.y;

				if (dot > 0 && dot <= 0.8f * ch->font->ascender * ch->size)
					ch->flags |= FZ_STEXT_STRIKEOUT;
				else
					ch->flags |= FZ_STEXT_UNDERLINE;
			}
		}
	}
}

static void
check_rects_for_strikeout(fz_context *ctx, fz_stext_device *tdev, fz_stext_page *page)
{
	int i, n = tdev->rect_len;

	for (i = 0; i < n; i++)
	{
		fz_point from = tdev->rects[i].from;
		fz_point to = tdev->rects[i].to;
		float thickness = tdev->rects[i].thickness;
		fz_point dir;
		dir.x = to.x - from.x;
		dir.y = to.y - from.y;
		dir = fz_normalize_vector(dir);

		check_strikeout(ctx, page->first_block, from, to, dir, thickness);
	}
}

static void
fz_stext_close_device(fz_context *ctx, fz_device *dev)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_stext_page *page = tdev->page;

	if ((tdev->flags & FZ_STEXT_DEHYPHENATE) && fz_is_unicode_hyphen(tdev->lastchar) && tdev->lastline != NULL)
		tdev->lastline->flags |= FZ_STEXT_LINE_FLAGS_JOINED;

	fixup_bboxes_and_bidi(ctx, page->first_block);

	if (tdev->opts.flags & FZ_STEXT_COLLECT_STYLES)
		check_rects_for_strikeout(ctx, tdev, page);

	/* TODO: smart sorting of blocks and lines in reading order */
	/* TODO: unicode NFC normalization */

	if (tdev->opts.flags & FZ_STEXT_SEGMENT)
		fz_segment_stext_page(ctx, page);

	if (tdev->opts.flags & FZ_STEXT_PARAGRAPH_BREAK)
		fz_paragraph_break(ctx, page);

	if (tdev->opts.flags & FZ_STEXT_TABLE_HUNT)
		fz_table_hunt(ctx, page);
}

static void
fz_stext_drop_device(fz_context *ctx, fz_device *dev)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_drop_text(ctx, tdev->lasttext);
	fz_drop_font(ctx, tdev->last.font);
	while (tdev->metatext)
		pop_metatext(ctx, tdev);

	fz_free(ctx, tdev->rects);
}

static int
val_is_rect(const char *val, fz_rect *rp)
{
	fz_rect r;
	const char *s;

	s = strchr(val, ':');
	if (s == NULL || s == val)
		return 0;
	r.x0 = fz_atof(val);
	val = s+1;
	s = strchr(val, ':');
	if (s == NULL || s == val)
		return 0;
	r.y0 = fz_atof(val);
	val = s+1;
	s = strchr(val, ':');
	if (s == NULL || s == val)
		return 0;
	r.x1 = fz_atof(val);
	val = s+1;
	r.y1 = fz_atof(val);

	*rp = r;

	return 1;
}

fz_stext_options *
fz_parse_stext_options(fz_context *ctx, fz_stext_options *opts, const char *string)
{
	const char *val;

	memset(opts, 0, sizeof *opts);

	/* when adding options, remember to update fz_stext_options_usage above */

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
	if (fz_has_option(ctx, string, "structured", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_COLLECT_STRUCTURE;
	if (fz_has_option(ctx, string, "use-cid-for-unknown-unicode", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_USE_CID_FOR_UNKNOWN_UNICODE;
	if (fz_has_option(ctx, string, "use-gid-for-unknown-unicode", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_USE_GID_FOR_UNKNOWN_UNICODE;
	if (fz_has_option(ctx, string, "accurate-bboxes", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_ACCURATE_BBOXES;
	if (fz_has_option(ctx, string, "vectors", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_COLLECT_VECTORS;
	if (fz_has_option(ctx, string, "ignore-actualtext", & val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_IGNORE_ACTUALTEXT;
	if (fz_has_option(ctx, string, "segment", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_SEGMENT;
	if (fz_has_option(ctx, string, "paragraph-break", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_PARAGRAPH_BREAK;
	if (fz_has_option(ctx, string, "table-hunt", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_TABLE_HUNT;
	if (fz_has_option(ctx, string, "collect-styles", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_COLLECT_STYLES;
	if (fz_has_option(ctx, string, "accurate-ascenders", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_ACCURATE_ASCENDERS;
	if (fz_has_option(ctx, string, "accurate-side-bearings", &val) && fz_option_eq(val, "yes"))
		opts->flags |= FZ_STEXT_ACCURATE_SIDE_BEARINGS;

	opts->flags |= FZ_STEXT_CLIP;
	if (fz_has_option(ctx, string, "mediabox-clip", &val))
	{
		fz_warn(ctx, "The 'mediabox-clip' option has been deprecated. Use 'clip' instead.");
		if (fz_option_eq(val, "no"))
			opts->flags ^= FZ_STEXT_CLIP;
	}
	if (fz_has_option(ctx, string, "clip", &val) && fz_option_eq(val, "no"))
		opts->flags ^= FZ_STEXT_CLIP;
	if (fz_has_option(ctx, string, "clip-rect", &val) && val_is_rect(val, &opts->clip))
		opts->flags |= FZ_STEXT_CLIP_RECT;

	opts->scale = 1;
	if (fz_has_option(ctx, string, "resolution", &val))
		opts->scale = fz_atof(val) / 96.0f; /* HTML base resolution is 96ppi */

	return opts;
}

typedef struct
{
	int fail;
	int count;
	fz_point corners[4];
} is_rect_data;

static void
stash_point(is_rect_data *rd, float x, float y)
{
	if (rd->count > 3)
	{
		rd->fail = 1;
		return;
	}

	rd->corners[rd->count].x = x;
	rd->corners[rd->count].y = y;
	rd->count++;
}

static void
is_rect_moveto(fz_context *ctx, void *arg, float x, float y)
{
	is_rect_data *rd = arg;
	if (rd->fail)
		return;

	if (rd->count != 0)
	{
		rd->fail = 1;
		return;
	}
	stash_point(rd, x, y);
}

static void
is_rect_lineto(fz_context *ctx, void *arg, float x, float y)
{
	is_rect_data *rd = arg;
	if (rd->fail)
		return;

	if (rd->count == 4 && rd->corners[0].x == x && rd->corners[1].y == y)
		return;

	stash_point(rd, x, y);
}

static void
is_rect_curveto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	is_rect_data *rd = arg;
	rd->fail = 1;
}

static void
is_rect_closepath(fz_context *ctx, void *arg)
{
	is_rect_data *rd = arg;
	if (rd->fail)
		return;
	if (rd->count == 3)
		stash_point(rd, rd->corners[0].x, rd->corners[0].y);
	if (rd->count != 4)
		rd->fail = 1;
}

static int
is_path_rect(fz_context *ctx, const fz_path *path, fz_point *from, fz_point *to, float *thickness, fz_matrix ctm)
{
	float d01, d01x, d01y, d03, d03x, d03y, d32x, d32y;
	is_rect_data rd = { 0 };
	static const fz_path_walker walker =
	{
		is_rect_moveto, is_rect_lineto, is_rect_curveto, is_rect_closepath
	};
	int i;

	fz_walk_path(ctx, path, &walker, &rd);

	if (rd.fail)
		return 0;

	if (rd.count == 2)
	{
		stash_point(&rd, rd.corners[1].x, rd.corners[1].y);
		stash_point(&rd, rd.corners[0].x, rd.corners[0].y);
	}

	for (i = 0 ; i < 4; i++)
	{
		fz_point p = fz_transform_point(rd.corners[i], ctm);

		rd.corners[i].x = p.x;
		rd.corners[i].y = p.y;
	}

	/* So we have a 4 cornered path. Hopefully something like:
	 * 0---------1
	 * |         |
	 * 3---------2
	 * but it might be:
	 * 0---------3
	 * |         |
	 * 1---------2
	*/
	while (1)
	{
		d01x = rd.corners[1].x - rd.corners[0].x;
		d01y = rd.corners[1].y - rd.corners[0].y;
		d01 = d01x * d01x + d01y * d01y;
		d03x = rd.corners[3].x - rd.corners[0].x;
		d03y = rd.corners[3].y - rd.corners[0].y;
		d03 = d03x * d03x + d03y * d03y;
		if(d01 < d03)
		{
			/* We are the latter case. Transpose it. */
			fz_point p = rd.corners[1];
			rd.corners[1] = rd.corners[3];
			rd.corners[3] = p;
		}
		else
			break;
	}
	d32x = rd.corners[2].x - rd.corners[3].x;
	d32y = rd.corners[2].y - rd.corners[3].y;

	/* So d32x and d01x need to be the same for this to be a strikeout. */
	if (!feq(d32x, d01x) || !feq(d32y, d01y))
		return 0;

	/* We are plausibly a rectangle. */
	*thickness = sqrtf(d03x * d03x + d03y * d03y);

	from->x = (rd.corners[0].x + rd.corners[3].x)/2;
	from->y = (rd.corners[0].y + rd.corners[3].y)/2;
	to->x = (rd.corners[1].x + rd.corners[2].x)/2;
	to->y = (rd.corners[1].y + rd.corners[2].y)/2;

	return 1;
}

static void
check_for_strikeout(fz_context *ctx, fz_stext_device *tdev, fz_stext_page *page, const fz_path *path, fz_matrix ctm)
{
	float thickness;
	fz_point from, to;

	/* Is this path a thin rectangle (possibly rotated)? If so, then we need to
	 * consider it as being a strikeout or underline. */
	if (!is_path_rect(ctx, path, &from, &to, &thickness, ctm))
		return;

	/* Add to the list of rects in the device. */
	if (tdev->rect_len == tdev->rect_max)
	{
		int newmax = tdev->rect_max * 2;
		if (newmax == 0)
			newmax = 32;

		tdev->rects = fz_realloc(ctx, tdev->rects, sizeof(*tdev->rects) * newmax);
		tdev->rect_max = newmax;
	}
	tdev->rects[tdev->rect_len].from = from;
	tdev->rects[tdev->rect_len].to = to;
	tdev->rects[tdev->rect_len].thickness = thickness;
	tdev->rect_len++;
}

static void
add_vector(fz_context *ctx, fz_stext_page *page, fz_stext_device *tdev, fz_rect bbox, uint32_t flags, uint32_t argb, int id, float exp)
{
	fz_stext_block *b;

	if (exp != 0)
	{
		bbox.x0 -= exp;
		bbox.y0 -= exp;
		bbox.x1 += exp;
		bbox.y1 += exp;
	}

	if (tdev->flags & (FZ_STEXT_CLIP_RECT | FZ_STEXT_CLIP))
	{
		fz_rect r = current_clip(ctx, tdev);
		bbox = fz_intersect_rect(bbox, r);
		if (!fz_is_valid_rect(bbox))
			return;
	}

	b = add_block_to_page(ctx, page, FZ_STEXT_BLOCK_VECTOR, id);

	b->bbox = bbox;
	b->u.v.flags = flags;
	b->u.v.argb = argb;
}

typedef struct
{
	fz_stext_device *dev;
	fz_matrix ctm;
	uint32_t argb;
	uint32_t flags;
	fz_stext_page *page;
	fz_rect seg_bounds;
	fz_rect leftovers;
	fz_rect pending;
	int count;
	fz_point p[5];
	int id;
	float exp;
} split_path_data;

static void
maybe_rect(fz_context *ctx, split_path_data *sp)
{
	int rect = 0;
	int i;
	fz_rect leftovers;

	if (sp->count >= 0)
	{
		if (sp->count == 3)
		{
			/* Allow for "moveto A, lineto B, lineto A, close" */
			if (feq(sp->p[0].x, sp->p[2].x) || feq(sp->p[0].y, sp->p[2].y))
				sp->count = 2;
		}
		if (sp->count == 2)
		{
			if (feq(sp->p[0].x, sp->p[1].x) || feq(sp->p[0].y, sp->p[1].y))
				rect = 1; /* Count that as a rect */
		}
		else if (sp->count == 4 || sp->count == 5)
		{
			if (feq(sp->p[0].x, sp->p[1].x) && feq(sp->p[2].x, sp->p[3].x) && feq(sp->p[0].y, sp->p[3].y) && feq(sp->p[1].y, sp->p[2].y))
				rect = 1;
			else if (feq(sp->p[0].x, sp->p[3].x) && feq(sp->p[1].x, sp->p[2].x) && feq(sp->p[0].y, sp->p[1].y) && feq(sp->p[2].y, sp->p[3].y))
				rect = 1;
		}
		if (rect)
		{
			fz_rect bounds;

			bounds.x0 = bounds.x1 = sp->p[0].x;
			bounds.y0 = bounds.y1 = sp->p[0].y;
			for (i = 1; i < sp->count; i++)
				bounds = fz_include_point_in_rect(bounds, sp->p[i]);
			if (fz_is_valid_rect(sp->pending))
				add_vector(ctx, sp->page, sp->dev, sp->pending, sp->flags | FZ_STEXT_VECTOR_IS_RECTANGLE | FZ_STEXT_VECTOR_CONTINUES, sp->argb, sp->id, sp->exp);
			sp->pending = bounds;
			return;
		}
	}

	/* We aren't a rectangle! */
	leftovers = sp->seg_bounds;

	if (sp->dev->flags & (FZ_STEXT_CLIP_RECT | FZ_STEXT_CLIP))
		leftovers = fz_intersect_rect(leftovers, current_clip(ctx, sp->dev));

	if (fz_is_valid_rect(leftovers))
		sp->leftovers = fz_union_rect(sp->leftovers, leftovers);

	/* Remember we're not a rect. */
	sp->count = -1;
}

static void
split_move(fz_context *ctx, void *arg, float x, float y)
{
	split_path_data *sp = (split_path_data *)arg;
	fz_point p = fz_transform_point_xy(x, y, sp->ctm);

	maybe_rect(ctx, sp);
	sp->p[0] = p;
	sp->count = 1;
	sp->seg_bounds.x0 = sp->seg_bounds.x1 = p.x;
	sp->seg_bounds.y0 = sp->seg_bounds.y1 = p.y;
}

static void
split_line(fz_context *ctx, void *arg, float x, float y)
{
	split_path_data *sp = (split_path_data *)arg;
	fz_point p = fz_transform_point_xy(x, y, sp->ctm);

	sp->seg_bounds = fz_include_point_in_rect(sp->seg_bounds, p);

	if (sp->count >= 0)
	{
		/* Check for lines to the same point. */
		if (feq(sp->p[sp->count-1].x, p.x) && feq(sp->p[sp->count-1].y, p.y))
			return;
		/* If we're still maybe a rect, just record the point. */
		if (sp->count < 4)
		{
			sp->p[sp->count++] = p;
			return;
		}
		/* Check for close line? */
		if (sp->count == 4)
		{
			if (feq(sp->p[0].x, p.x) && feq(sp->p[0].y, p.y))
			{
				/* We've just drawn a line back to the start point. */
				/* Needless saving of point, but it makes the logic
				 * easier elsewhere. */
				sp->p[sp->count++] = p;
				return;
			}
		}
		/* We can no longer be a rect. */
		sp->count = -1;
	}
}

static void
split_curve(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	split_path_data *sp = (split_path_data *)arg;

	sp->seg_bounds = fz_include_point_in_rect(sp->seg_bounds, fz_transform_point_xy(x1, y1, sp->ctm));
	sp->seg_bounds = fz_include_point_in_rect(sp->seg_bounds, fz_transform_point_xy(x2, y2, sp->ctm));
	sp->seg_bounds = fz_include_point_in_rect(sp->seg_bounds, fz_transform_point_xy(x3, y3, sp->ctm));

	/* We can no longer be a rect. */
		sp->count = -1;
	}

static void
split_close(fz_context *ctx, void *arg)
{
	split_path_data *sp = (split_path_data *)arg;

	maybe_rect(ctx, sp);
	sp->count = 0;
}


static const
fz_path_walker split_path_rects =
{
	split_move,
	split_line,
	split_curve,
	split_close
};

static void
add_vectors_from_path(fz_context *ctx, fz_stext_page *page, fz_stext_device *tdev, const fz_path *path, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp, int stroke, float exp)
{
	int have_leftovers;
	split_path_data sp;
	int id = tdev->id;

	sp.dev = tdev;
	sp.ctm = ctm;
	sp.argb = hexrgba_from_color(ctx, cs, color, alpha);
	sp.flags = stroke ? FZ_STEXT_VECTOR_IS_STROKED : 0;
	sp.page = page;
	sp.count = 0;
	sp.leftovers = fz_empty_rect;
	sp.seg_bounds = fz_empty_rect;
	sp.pending = fz_empty_rect;
	sp.id = id;
	sp.exp = exp;
	fz_walk_path(ctx, path, &split_path_rects, &sp);

	have_leftovers = fz_is_valid_rect(sp.leftovers);

	maybe_rect(ctx, &sp);

	if (fz_is_valid_rect(sp.pending))
		add_vector(ctx, page, sp.dev, sp.pending, sp.flags | FZ_STEXT_VECTOR_IS_RECTANGLE | (have_leftovers ? FZ_STEXT_VECTOR_CONTINUES : 0), sp.argb, id, exp);
	if (have_leftovers)
		add_vector(ctx, page, sp.dev, sp.leftovers, sp.flags, sp.argb, id, exp);
}

static void
fz_stext_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_stext_page *page = tdev->page;
	fz_rect path_bounds = fz_bound_path(ctx, path, NULL, ctm);
	fz_rect *bounds = actualtext_bounds(tdev);

	/* If we're in an actualtext, then update the bounds to include this content. */
	if (bounds != NULL)
		*bounds = fz_union_rect(*bounds, path_bounds);

	if (tdev->flags & FZ_STEXT_COLLECT_STYLES)
		check_for_strikeout(ctx, tdev, page, path, ctm);

	if (tdev->flags & FZ_STEXT_COLLECT_VECTORS)
		add_vectors_from_path(ctx, page, tdev, path, ctm, cs, color, alpha, cp, 0, 0);
}

static void
fz_stext_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *ss, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_stext_page *page = tdev->page;
	fz_rect path_bounds = fz_bound_path(ctx, path, ss, ctm);
	fz_rect *bounds = actualtext_bounds((fz_stext_device *)dev);
	float exp = ss->linewidth / 2;

	/* If we're in an actualtext, then update the bounds to include this content. */
	if (bounds != NULL)
		*bounds = fz_union_rect(*bounds, path_bounds);

	if (tdev->flags & FZ_STEXT_COLLECT_STYLES)
		check_for_strikeout(ctx, tdev, page, path, ctm);

	if (tdev->flags & FZ_STEXT_COLLECT_VECTORS)
		add_vectors_from_path(ctx, page, tdev, path, ctm, cs, color, alpha, cp, 1, exp);
}

static void
new_stext_struct(fz_context *ctx, fz_stext_page *page, fz_stext_block *block, fz_structure standard, const char *raw)
{
	fz_stext_struct *str;
	size_t z;

	if (raw == NULL)
		raw = "";
	z = strlen(raw);

	str = fz_pool_alloc(ctx, page->pool, offsetof(fz_stext_struct, raw) + z + 1);
	str->first_block = NULL;
	str->last_block = NULL;
	str->standard = standard;
	str->parent = page->last_struct;
	str->up = block;
	memcpy(str->raw, raw, z+1);

	block->u.s.down = str;
}

fz_stext_block *
fz_new_stext_struct(fz_context *ctx, fz_stext_page *page, fz_structure standard, const char *raw, int idx)
{
	fz_stext_block *block;

	block = fz_pool_alloc(ctx, page->pool, sizeof *page->first_block);
	block->bbox = fz_empty_rect;
	block->prev = NULL;
	block->next = NULL;
	block->type = FZ_STEXT_BLOCK_STRUCT;
	block->u.s.index = idx;
	block->u.s.down = NULL;
	/* If this throws, we leak newblock but it's within the pool, so it doesn't matter. */
	new_stext_struct(ctx, page, block, standard, raw);

	return block;
}


static void
fz_stext_begin_structure(fz_context *ctx, fz_device *dev, fz_structure standard, const char *raw, int idx)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_stext_page *page = tdev->page;
	fz_stext_block *block, *le, *gt, *newblock;

	if (raw == NULL)
		raw = "";

	/* Find a pointer to the last block. */
	if (page->last_block)
	{
		block = page->last_block;
	}
	else if (page->last_struct)
	{
		block = page->last_struct->last_block;
	}
	else
	{
		block = page->first_block;
	}

	/* So block is somewhere in the content chain. Let's try and find:
	 *   le = the struct node <= idx before block in the content chain.
	 *   ge = the struct node >= idx after block in the content chain.
	 * Search backwards to start with.
	 */
	gt = NULL;
	le = block;
	while (le)
	{
		if (le->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (le->u.s.index > idx)
				gt = le;
			if (le->u.s.index <= idx)
				break;
		}
		le = le->prev;
	}
	/* The following loop copes with finding gt (the smallest block with an index higher
	 * than we want) if we haven't found it already. The while loop in here was designed
	 * to cope with 'block' being in the middle of a list. In fact, the way the code is
	 * currently, block will always be at the end of a list, so the while won't do anything.
	 * But I'm loathe to remove it in case we ever change this code to start from wherever
	 * we did the last insertion. */
	if (gt == NULL)
	{
		gt = block;
		while (gt)
		{
			if (gt->type == FZ_STEXT_BLOCK_STRUCT)
			{
				if (gt->u.s.index <= idx)
					le = gt;
				if (gt->u.s.index >= idx)
					break;
			}
			block = gt;
			gt = gt->next;
		}
	}

	if (le && le->u.s.index == idx)
	{
		/* We want to move down into the le block. Does it have a struct
		 * attached yet? */
		if (le->u.s.down == NULL)
		{
			/* No. We need to create a new struct node. */
			new_stext_struct(ctx, page, le, standard, raw);
		}
		else if (le->u.s.down->standard != standard || strcmp(raw, le->u.s.down->raw) != 0)
		{
			/* Yes, but it doesn't match the one we expect! */
			fz_warn(ctx, "Mismatched structure type!");
		}
		page->last_struct = le->u.s.down;
		page->last_block = le->u.s.down->last_block;

		return;
	}

	/* We are going to need to create a new block. Create a complete unlinked one here. */
	newblock = fz_new_stext_struct(ctx, page, standard, raw, idx);

	/* So now we just need to link it in somewhere. */
	if (gt)
	{
		/* Link it in before gt. */
		newblock->prev = gt->prev;
		if (gt->prev)
			gt->prev->next = newblock;
		else if (page->last_struct)
		{
			/* We're linking it in at the start under another struct! */
			assert(page->last_struct->first_block == gt);
			assert(page->last_struct->last_block != NULL);
			page->last_struct->first_block = newblock;
		}
		else
		{
			/* We're linking it in at the start of the page! */
			assert(page->first_block == gt);
			page->first_block = newblock;
		}
		gt->prev = newblock;
		newblock->next = gt;
		newblock->id = gt->id;
	}
	else if (block)
	{
		/* Link it in at the end of the list (i.e. after 'block') */
		newblock->prev = block;
		block->next = newblock;
		if (page->last_struct)
		{
			assert(page->last_struct->last_block == block);
			page->last_struct->last_block = newblock;
		}
		else
		{
			assert(page->last_block == block);
			page->last_block = newblock;
		}
		newblock->id = block->id;
	}
	else if (page->last_struct)
	{
		/* We have no blocks at all at this level. */
		page->last_struct->first_block = newblock;
		page->last_struct->last_block = newblock;
		newblock->id = page->last_struct->up->id;
	}
	else
	{
		/* We have no blocks at ANY level. */
		page->first_block = newblock;
		/* newblock will have an id of 0. Best we can do. */
	}
	/* Wherever we linked it in, that's where we want to continue adding content. */
	page->last_struct = newblock->u.s.down;
	page->last_block = NULL;
}

static void
fz_stext_end_structure(fz_context *ctx, fz_device *dev)
{
	fz_stext_device *tdev = (fz_stext_device*)dev;
	fz_stext_page *page = tdev->page;
	fz_stext_struct *str = page->last_struct;

	if (str == NULL)
	{
		fz_warn(ctx, "Structure out of sync");
		return;
	}

	page->last_struct = str->parent;
	if (page->last_struct == NULL)
	{
		page->last_block = page->first_block;
		/* Yuck */
		while (page->last_block->next)
			page->last_block = page->last_block->next;
	}
	else
	{
		page->last_block = page->last_struct->last_block;
	}
}

fz_device *
fz_new_stext_device(fz_context *ctx, fz_stext_page *page, const fz_stext_options *opts)
{
	return fz_new_stext_device_for_page(ctx, page, opts, 0, 0, fz_empty_rect);
}

fz_device *
fz_new_stext_device_for_page(fz_context *ctx, fz_stext_page *page, const fz_stext_options *opts, int chapter_num, int page_num, fz_rect mediabox)
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

	dev->super.fill_shade = fz_stext_fill_shade;
	dev->super.fill_image = fz_stext_fill_image;
	dev->super.fill_image_mask = fz_stext_fill_image_mask;

	if (opts)
	{
		dev->flags = opts->flags;
		if (opts->flags & FZ_STEXT_COLLECT_STRUCTURE)
		{
			dev->super.begin_structure = fz_stext_begin_structure;
			dev->super.end_structure = fz_stext_end_structure;
		}
		if (opts->flags & (FZ_STEXT_COLLECT_VECTORS | FZ_STEXT_COLLECT_STYLES))
		{
			dev->super.fill_path = fz_stext_fill_path;
			dev->super.stroke_path = fz_stext_stroke_path;
		}
	}
	dev->page = page;
	dev->pen.x = 0;
	dev->pen.y = 0;
	dev->trm = fz_identity;
	dev->lastchar = ' ';
	dev->lastline = NULL;
	dev->lasttext = NULL;
	dev->lastbidi = 0;
	dev->last_was_fake_bold = 1;
	if (opts)
		dev->opts = *opts;

	/* If we are ignoring images, then it'd be nice to skip the decode costs. BUT we still need them to tell
	 * us the bounds for ActualText, so we can only actually skip them if we are ignoring actualtext too. */
	if ((dev->flags & FZ_STEXT_PRESERVE_IMAGES) == 0 && (dev->opts.flags & FZ_STEXT_IGNORE_ACTUALTEXT) != 0)
		dev->super.hints |= FZ_DONT_DECODE_IMAGES;

	dev->rect_max = 0;
	dev->rect_len = 0;
	dev->rects = NULL;

	/* Push a new id */
	fz_try(ctx)
	{
		fz_stext_page_details *deets;
		size_t id;
		deets = fz_pool_array_append(ctx, page->id_list, &id);
		dev->id = (int)id;
		deets->mediabox = mediabox;
		deets->chapter = chapter_num;
		deets->page = page_num;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, dev);
		fz_rethrow(ctx);
	}

	page->mediabox = fz_union_rect(page->mediabox, mediabox);

	return (fz_device*)dev;
}
