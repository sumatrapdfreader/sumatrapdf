#include "fitz-internal.h"
#include "ucdn.h"

/* Extract text into an unsorted span soup. */

#define LINE_DIST 0.9f
#define SPACE_DIST 0.2f
#define SPACE_MAX_DIST 0.8f
#define PARAGRAPH_DIST 0.5f

#undef DEBUG_SPANS
#undef DEBUG_INTERNALS
#undef DEBUG_LINE_HEIGHTS
#undef DEBUG_MASKS
#undef DEBUG_ALIGN
#undef DEBUG_INDENTS

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

typedef struct fz_text_device_s fz_text_device;

typedef struct span_soup_s span_soup;

struct fz_text_device_s
{
	fz_text_sheet *sheet;
	fz_text_page *page;
	span_soup *spans;
	fz_text_span *cur_span;
	int lastchar;
};

static fz_rect *
add_point_to_rect(fz_rect *a, const fz_point *p)
{
	if (p->x < a->x0)
		a->x0 = p->x;
	if (p->x > a->x1)
		a->x1 = p->x;
	if (p->y < a->y0)
		a->y0 = p->y;
	if (p->y > a->y1)
		a->y1 = p->y;
	return a;
}

fz_rect *
fz_text_char_bbox(fz_rect *bbox, fz_text_span *span, int i)
{
	fz_point a, d;
	const fz_point *max;
	fz_text_char *ch;

	if (!span || i >= span->len)
	{
		*bbox = fz_empty_rect;
	}
	ch = &span->text[i];
	if (i == span->len-1)
		max = &span->max;
	else
		max = &span->text[i+1].p;
	a.x = 0;
	a.y = span->ascender_max;
	fz_transform_vector(&a, &span->transform);
	d.x = 0;
	d.y = span->descender_min;
	fz_transform_vector(&d, &span->transform);
	bbox->x0 = bbox->x1 = ch->p.x + a.x;
	bbox->y0 = bbox->y1 = ch->p.y + a.y;
	a.x += max->x;
	a.y += max->y;
	add_point_to_rect(bbox, &a);
	a.x = ch->p.x + d.x;
	a.y = ch->p.y + d.y;
	add_point_to_rect(bbox, &a);
	a.x = max->x + d.x;
	a.y = max->y + d.y;
	add_point_to_rect(bbox, &a);
	return bbox;
}

static void
add_bbox_to_span(fz_text_span *span)
{
	fz_point a, d;
	fz_rect *bbox = &span->bbox;

	if (!span)
		return;
	a.x = 0;
	a.y = span->ascender_max;
	fz_transform_vector(&a, &span->transform);
	d.x = 0;
	d.y = span->descender_min;
	fz_transform_vector(&d, &span->transform);
	bbox->x0 = bbox->x1 = span->min.x + a.x;
	bbox->y0 = bbox->y1 = span->min.y + a.y;
	a.x += span->max.x;
	a.y += span->max.y;
	add_point_to_rect(bbox, &a);
	a.x = span->min.x + d.x;
	a.y = span->min.y + d.y;
	add_point_to_rect(bbox, &a);
	a.x = span->max.x + d.x;
	a.y = span->max.y + d.y;
	add_point_to_rect(bbox, &a);
}

struct span_soup_s
{
	fz_context *ctx;
	int len, cap;
	fz_text_span **spans;
};

static span_soup *
new_span_soup(fz_context *ctx)
{
	span_soup *soup = fz_malloc_struct(ctx, span_soup);
	soup->ctx = ctx;
	soup->len = 0;
	soup->cap = 0;
	soup->spans = NULL;
	return soup;
}

static void
free_span_soup(span_soup *soup)
{
	int i;

	if (soup == NULL)
		return;
	for (i = 0; i < soup->len; i++)
	{
		fz_free(soup->ctx, soup->spans[i]);
	}
	fz_free(soup->ctx, soup->spans);
	fz_free(soup->ctx, soup);
}

static void
add_span_to_soup(span_soup *soup, fz_text_span *span)
{
	if (span == NULL)
		return;
	if (soup->len == soup->cap)
	{
		int newcap = (soup->cap ? soup->cap * 2 : 16);
		soup->spans = fz_resize_array(soup->ctx, soup->spans, newcap, sizeof(*soup->spans));
		soup->cap = newcap;
	}
	add_bbox_to_span(span);
	soup->spans[soup->len++] = span;
}

static fz_text_line *
push_span(fz_context *ctx, fz_text_device *tdev, fz_text_span *span, int new_line, float distance)
{
	fz_text_line *line;
	fz_text_block *block;
	fz_text_page *page = tdev->page;
	int prev_not_text = 0;

	if (page->len == 0 || page->blocks[page->len-1].type != FZ_PAGE_BLOCK_TEXT)
		prev_not_text = 1;

	if (new_line || prev_not_text)
	{
		/* SumatraPDF: fixup_text_page doesn't handle multiple blocks yet * /
		float size = fz_matrix_expansion(&span->transform);
		/* So, a new line. Part of the same block or not? */
		if (/* distance == 0 || distance > size * 1.5 || distance < -size * PARAGRAPH_DIST || page->len == 0 || */ prev_not_text)
		{
			/* New block */
			if (page->len == page->cap)
			{
				int newcap = (page->cap ? page->cap*2 : 4);
				page->blocks = fz_resize_array(ctx, page->blocks, newcap, sizeof(*page->blocks));
				page->cap = newcap;
			}
			block = fz_malloc_struct(ctx, fz_text_block);
			page->blocks[page->len].type = FZ_PAGE_BLOCK_TEXT;
			page->blocks[page->len].u.text = block;
			block->cap = 0;
			block->len = 0;
			block->lines = 0;
			block->bbox = fz_empty_rect;
			page->len++;
			distance = 0;
		}

		/* New line */
		block = page->blocks[page->len-1].u.text;
		if (block->len == block->cap)
		{
			int newcap = (block->cap ? block->cap*2 : 4);
			block->lines = fz_resize_array(ctx, block->lines, newcap, sizeof(*block->lines));
			block->cap = newcap;
		}
		block->lines[block->len].first_span = NULL;
		block->lines[block->len].last_span = NULL;
		block->lines[block->len].distance = distance;
		block->lines[block->len].bbox = fz_empty_rect;
		block->len++;
	}

	/* Find last line and append to it */
	block = page->blocks[page->len-1].u.text;
	line = &block->lines[block->len-1];

	fz_union_rect(&block->lines[block->len-1].bbox, &span->bbox);
	fz_union_rect(&block->bbox, &span->bbox);
	span->base_offset = (new_line ? 0 : distance);

	if (!line->first_span)
	{
		line->first_span = line->last_span = span;
		span->next = NULL;
	}
	else
	{
		line->last_span->next = span;
		line->last_span = span;
	}

	return line;
}

#if defined(DEBUG_SPANS) || defined(DEBUG_ALIGN) || defined(DEBUG_INDENTS)
static void
dump_span(fz_text_span *s)
{
	int i;
	for (i=0; i < s->len; i++)
	{
		printf("%c", s->text[i].c);
	}
}
#endif

#ifdef DEBUG_ALIGN
static void
dump_line(fz_text_line *line)
{
	int i;
	for (i=0; i < line->len; i++)
	{
		fz_text_span *s = line->spans[i];
		if (s->spacing > 1)
			printf(" ");
		dump_span(s);
	}
	printf("\n");
}
#endif

static void
strain_soup(fz_context *ctx, fz_text_device *tdev)
{
	span_soup *soup = tdev->spans;
	fz_text_line *last_line = NULL;
	fz_text_span *last_span = NULL;
	int span_num;

	/* Really dumb implementation to match what we had before */
	for (span_num=0; span_num < soup->len; span_num++)
	{
		fz_text_span *span = soup->spans[span_num];
		int new_line = 1;
		float distance = 0;
		float spacing = 0;
		soup->spans[span_num] = NULL;
		if (last_span)
		{
			/* If we have a last_span, we must have a last_line */
			/* Do span and last_line share the same baseline? */
			fz_point p, q, perp_r;
			float dot;
			float size = fz_matrix_expansion(&span->transform);

#ifdef DEBUG_SPANS
			{
				printf("Comparing: \"");
				dump_span(last_span);
				printf("\" and \"");
				dump_span(span);
				printf("\"\n");
			}
#endif

			p.x = last_line->first_span->max.x - last_line->first_span->min.x;
			p.y = last_line->first_span->max.y - last_line->first_span->min.y;
			fz_normalize_vector(&p);
			q.x = span->max.x - span->min.x;
			q.y = span->max.y - span->min.y;
			fz_normalize_vector(&q);
#ifdef DEBUG_SPANS
			printf("last_span=%g %g -> %g %g = %g %g\n", last_span->min.x, last_span->min.y, last_span->max.x, last_span->max.y, p.x, p.y);
			printf("span     =%g %g -> %g %g = %g %g\n", span->min.x, span->min.y, span->max.x, span->max.y, q.x, q.y);
#endif
			perp_r.y = last_line->first_span->min.x - span->min.x;
			perp_r.x = -(last_line->first_span->min.y - span->min.y);
			/* Check if p and q are parallel. If so, then this
			 * line is parallel with the last one. */
			dot = p.x * q.x + p.y * q.y;
			if (fabsf(dot) > 0.9995)
			{
				/* If we take the dot product of normalised(p) and
				 * perp(r), we get the perpendicular distance from
				 * one line to the next (assuming they are parallel). */
				distance = p.x * perp_r.x + p.y * perp_r.y;
				/* We allow 'small' distances of baseline changes
				 * to cope with super/subscript. FIXME: We should
				 * gather subscript/superscript information here. */
				new_line = (fabsf(distance) > size * LINE_DIST);
			}
			else
			{
				new_line = 1;
				distance = 0;
			}
			if (!new_line)
			{
				fz_point delta;

				delta.x = span->min.x - last_span->max.x;
				delta.y = span->min.y - last_span->max.y;

				spacing = (p.x * delta.x + p.y * delta.y);
				spacing = fabsf(spacing);
				/* Only allow changes in baseline (subscript/superscript etc)
				 * when the spacing is small. */
				if (spacing * fabsf(distance) > size * LINE_DIST && fabsf(distance) > size * 0.1f)
				{
					new_line = 1;
					distance = 0;
					spacing = 0;
				}
				else
				{
					spacing /= size * SPACE_DIST;
					/* Apply the same logic here as when we're adding chars to build spans. */
					if (spacing >= 1 && spacing < (SPACE_MAX_DIST/SPACE_DIST))
						spacing = 1;
				}
			}
#ifdef DEBUG_SPANS
			printf("dot=%g new_line=%d distance=%g size=%g spacing=%g\n", dot, new_line, distance, size, spacing);
#endif
		}
		span->spacing = spacing;
		last_line = push_span(ctx, tdev, span, new_line, distance);
		last_span = span;
	}
}

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
	fz_text_style *style;
	
	if (sheet == NULL)
		return;

	style = sheet->style;
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
fz_lookup_text_style(fz_context *ctx, fz_text_sheet *sheet, fz_text *text, const fz_matrix *ctm,
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
		fz_concat(&trm, &tm, ctm);
		size = fz_matrix_expansion(&trm);
	}
	return fz_lookup_text_style_imp(ctx, sheet, size, font, wmode, 0);
}

fz_text_page *
fz_new_text_page(fz_context *ctx)
{
	fz_text_page *page = fz_malloc(ctx, sizeof(*page));
	page->mediabox = fz_empty_rect;
	page->len = 0;
	page->cap = 0;
	page->blocks = NULL;
	page->next = NULL;
	return page;
}

static void
fz_free_text_line_contents(fz_context *ctx, fz_text_line *line)
{
	fz_text_span *span, *next;
	for (span = line->first_span; span; span=next)
	{
		next = span->next;
		fz_free(ctx, span->text);
		fz_free(ctx, span);
	}
}

static void
fz_free_text_block(fz_context *ctx, fz_text_block *block)
{
	fz_text_line *line;
	if (block == NULL)
		return;
	for (line = block->lines; line < block->lines + block->len; line++)
		fz_free_text_line_contents(ctx, line);
	fz_free(ctx, block->lines);
	fz_free(ctx, block);
}

static void
fz_free_image_block(fz_context *ctx, fz_image_block *block)
{
	if (block == NULL)
		return;
	fz_drop_image(ctx, block->image);
	fz_drop_colorspace(ctx, block->cspace);
	fz_free(ctx, block);
}

void
fz_free_text_page(fz_context *ctx, fz_text_page *page)
{
	fz_page_block *block;
	if (page == NULL)
		return;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		switch (block->type)
		{
		case FZ_PAGE_BLOCK_TEXT:
			fz_free_text_block(ctx, block->u.text);
			break;
		case FZ_PAGE_BLOCK_IMAGE:
			fz_free_image_block(ctx, block->u.image);
			break;
		}
	}
	fz_free(ctx, page->blocks);
	fz_free(ctx, page);
}

static fz_text_span *
fz_new_text_span(fz_context *ctx, const fz_point *p, int wmode, const fz_matrix *trm)
{
	fz_text_span *span = fz_malloc_struct(ctx, fz_text_span);
	span->ascender_max = 0;
	span->descender_min = 0;
	span->cap = 0;
	span->len = 0;
	span->min = *p;
	span->max = *p;
	span->wmode = wmode;
	span->transform.a = trm->a;
	span->transform.b = trm->b;
	span->transform.c = trm->c;
	span->transform.d = trm->d;
	span->transform.e = 0;
	span->transform.f = 0;
	span->text = NULL;
	span->next = NULL;
	return span;
}

static void
add_char_to_span(fz_context *ctx, fz_text_span *span, int c, fz_point *p, fz_point *max, fz_text_style *style)
{
	if (span->len == span->cap)
	{
		int newcap = (span->cap ? span->cap * 2 : 16);
		span->text = fz_resize_array(ctx, span->text, newcap, sizeof(fz_text_char));
		span->cap = newcap;
		span->bbox = fz_empty_rect;
	}
	span->max = *max;
	if (style->ascender > span->ascender_max)
		span->ascender_max = style->ascender;
	if (style->descender < span->descender_min)
		span->descender_min = style->descender;
	span->text[span->len].c = c;
	span->text[span->len].p = *p;
	span->text[span->len].style = style;
	span->len++;
}

static void
fz_add_text_char_imp(fz_context *ctx, fz_text_device *dev, fz_text_style *style, int c, fz_matrix *trm, float adv, int wmode)
{
	int can_append = 1;
	int add_space = 0;
	fz_point dir, ndir, p, q;
	float size;
	fz_point delta;
	float spacing = 0;
	float base_offset = 0;

	/* SumatraPDF: TODO: make this depend on the per-glyph displacement vector */
	if (wmode == 0)
	{
		dir.x = 1;
		dir.y = 0;
	}
	else
	{
		dir.x = 0;
		dir.y = 1;
	}
	fz_transform_vector(&dir, trm);
	ndir = dir;
	fz_normalize_vector(&ndir);
	/* dir = direction vector for motion. ndir = normalised(dir) */

	size = fz_matrix_expansion(trm);

	if (dev->cur_span == NULL ||
		trm->a != dev->cur_span->transform.a || trm->b != dev->cur_span->transform.b ||
		trm->c != dev->cur_span->transform.c || trm->d != dev->cur_span->transform.d)
	{
		/* If the matrix has changed (or if we don't have a span at
		 * all), then we can't append. */
#ifdef DEBUG_SPANS
		printf("Transform changed\n");
#endif
		can_append = 0;
	}
	else
	{
		/* Calculate how far we've moved since the end of the current
		 * span. */
		delta.x = trm->e - dev->cur_span->max.x;
		delta.y = trm->f - dev->cur_span->max.y;

		/* The transform has not changed, so we know we're in the same
		 * direction. Calculate 2 distances; how far off the previous
		 * baseline we are, together with how far along the baseline
		 * we are from the expected position. */
		spacing = ndir.x * delta.x + ndir.y * delta.y;
		base_offset = -ndir.y * delta.x + ndir.x * delta.y;

		spacing /= size * SPACE_DIST;
		spacing = fabsf(spacing);
		if (fabsf(base_offset) < size * 0.1)
		{
			/* Only a small amount off the baseline - we'll take this */
			if (spacing < 1.0)
			{
				/* Motion is in line, and small. */
			}
			else if (spacing >= 1 && spacing < (SPACE_MAX_DIST/SPACE_DIST))
			{
				/* Motion is in line, but large enough
				 * to warrant us adding a space */
				if (dev->lastchar != ' ' && wmode == 0)
					add_space = 1;
			}
			else
			{
				/* Motion is in line, but too large - split to a new span */
				can_append = 0;
			}
		}
		else
		{
			can_append = 0;
			spacing = 0;
		}
	}

#ifdef DEBUG_SPANS
	printf("%c%c append=%d space=%d size=%g spacing=%g base_offset=%g\n", dev->lastchar, c, can_append, add_space, size, spacing, base_offset);
#endif

	p.x = trm->e;
	p.y = trm->f;
	if (can_append == 0)
	{
		/* Start a new span */
		add_span_to_soup(dev->spans, dev->cur_span);
		dev->cur_span = NULL;
		dev->cur_span = fz_new_text_span(ctx, &p, wmode, trm);
		dev->cur_span->spacing = 0;
	}
	/* SumatraPDF: don't add spaces before spaces */
	if (add_space && c != 32)
	{
		q.x = - 0.2f;
		q.y = 0;
		fz_transform_point(&q, trm);
		add_char_to_span(ctx, dev->cur_span, ' ', &p, &q, style);
	}
	/* Advance the matrix */
	q.x = trm->e += adv * dir.x;
	q.y = trm->f += adv * dir.y;
	add_char_to_span(ctx, dev->cur_span, c, &p, &q, style);
}

static void
fz_add_text_char(fz_context *ctx, fz_text_device *dev, fz_text_style *style, int c, fz_matrix *trm, float adv, int wmode)
{
	switch (c)
	{
	case -1: /* ignore when one unicode character maps to multiple glyphs */
		break;
	case 0xFB00: /* ff */
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/2, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/2, wmode);
		break;
	case 0xFB01: /* fi */
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/2, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'i', trm, adv/2, wmode);
		break;
	case 0xFB02: /* fl */
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/2, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'l', trm, adv/2, wmode);
		break;
	case 0xFB03: /* ffi */
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/3, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/3, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'i', trm, adv/3, wmode);
		break;
	case 0xFB04: /* ffl */
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/3, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'f', trm, adv/3, wmode);
		fz_add_text_char_imp(ctx, dev, style, 'l', trm, adv/3, wmode);
		break;
	case 0xFB05: /* long st */
	case 0xFB06: /* st */
		fz_add_text_char_imp(ctx, dev, style, 's', trm, adv/2, wmode);
		fz_add_text_char_imp(ctx, dev, style, 't', trm, adv/2, wmode);
		break;
	default:
		fz_add_text_char_imp(ctx, dev, style, c, trm, adv, wmode);
		break;
	}
}

static void
fz_text_extract(fz_context *ctx, fz_text_device *dev, fz_text *text, const fz_matrix *ctm, fz_text_style *style)
{
	fz_font *font = text->font;
	FT_Face face = font->ft_face;
	fz_matrix tm = text->trm;
	fz_matrix trm;
	float adv = -1;
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
	else if (font->t3procs && !fz_is_empty_rect(&font->bbox))
	{
		ascender = font->bbox.y1;
		descender = font->bbox.y0;
	}
	style->ascender = ascender;
	style->descender = descender;

	tm.e = 0;
	tm.f = 0;
	fz_concat(&trm, &tm, ctm);

	for (i = 0; i < text->len; i++)
	{
		/* Calculate new pen location and delta */
		tm.e = text->items[i].x;
		tm.f = text->items[i].y;
		fz_concat(&trm, &tm, ctm);

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
		}
		/* SumatraPDF: TODO: this check might no longer be needed */
		else if (text->items[i].gid < 256)
		{
			adv = font->t3widths[text->items[i].gid];
		}

		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1839 */
		if (font->ft_face)
		{
			fz_rect bbox;
			fz_bound_glyph(ctx, font, text->items[i].gid, &fz_identity, &bbox);
			style->ascender = fz_max(style->ascender, bbox.y1);
			style->descender = fz_min(style->descender, bbox.y0);
		}

		/* Check for one glyph to many char mapping */
		for (j = i + 1; j < text->len; j++)
			if (text->items[j].gid >= 0)
				break;
		multi = j - i;

		if (multi == 1)
		{
			fz_add_text_char(ctx, dev, style, text->items[i].ucs, &trm, adv, text->wmode);
		}
		else
		{
			for (j = 0; j < multi; j++)
			{
				fz_add_text_char(ctx, dev, style, text->items[i + j].ucs, &trm, adv/multi, text->wmode);
			}
			i += j - 1;
		}

		dev->lastchar = text->items[i].ucs;
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
insert_character(fz_context *ctx, fz_text_span *span, fz_text_char *c, int i)
{
	add_char_to_span(ctx, span, span->text[span->len - 1].c, &span->text[span->len - 1].p, &span->max, span->text[span->len - 1].style);
	memmove(&span->text[i + 1], &span->text[i], (span->len - (i - 1)) * sizeof(fz_text_char));
	span->text[i] = *c;
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
is_character_ornate(int c)
{
	switch (c)
	{
	case 0x00A8: /* diaeresis/umlaut */
	case 0x00B4: /* accute accent */
	case 0x0060: /* grave accent */
	case 0x005E: case 0x02C6: /* circumflex accent */
	case 0x02DA: /* ring above */
		return 1;
	default:
		return 0;
	}
}

static int
ornate_character(fz_text_span *span, int ornate, int character)
{
	static wchar_t *ornates[] = {
		L" \xA8\xB4\x60\x5E\u02C6\u02DA",
		L"a\xE4\xE1\xE0\xE2\xE2\xE5", L"A\xC4\xC1\xC0\xC2\xC2\0",
		L"e\xEB\xE9\xE8\xEA\xEA\0", L"E\xCB\xC9\xC8\xCA\xCA\0",
		L"i\xEF\xED\xEC\xEE\xEE\0", L"I\xCF\xCD\xCC\xCE\xCE\0",
		L"\u0131\xEF\xED\xEC\xEE\xEE\0", L"\u0130\xCF\xCD\xCC\xCE\xCE\0",
		L"o\xF6\xF3\xF2\xF4\xF4\0", L"O\xD6\xD3\xD2\xD4\xD4\0",
		L"u\xFC\xFA\xF9\xFB\xFB\0", L"U\xDC\xDA\xD9\xDB\xDB\0",
		NULL
	};
	int i = 1, j = 1;
	fz_rect bbox1, bbox2;
	fz_text_char_bbox(&bbox1, span, ornate);
	if ((bbox1.x0 + bbox1.x1) / 2 < fz_text_char_bbox(&bbox2, span, character)->x0)
		return 0;
	while (ornates[0][i] && ornates[0][i] != (wchar_t)span->text[ornate].c)
		i++;
	while (ornates[j] && ornates[j][0] != (wchar_t)span->text[character].c)
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
		// TODO: some ornates now are on their own line
		if (is_character_ornate(span->text[i].c) && i + 1 < span->len)
		{
			/* recombine characters and their accents */
			int newC = 0;
			if (span->text[i + 1].c != 32 || i + 2 == span->len)
				newC = ornate_character(span, i, i + 1);
			else if ((newC = ornate_character(span, i, i + 2)) != 0)
				delete_character(span, i + 1);
			if (newC)
			{
				delete_character(span, i);
				span->text[i].c = newC;
			}
		}
		else if (ISRIGHTTOLEFTCHAR(span->text[i].c))
		{
			/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=733 */
			/* reverse words written in RTL languages */
			fz_rect a, b;
			int j = i + 1;
			while (j < span->len && fz_text_char_bbox(&a, span, j - 1)->x0 <= fz_text_char_bbox(&b, span, j)->x0 && !ISLEFTTORIGHTCHAR(span->text[i].c))
				j++;
			reverse_characters(span, i, j - 1);
			i = j;
		}
	}
}

static float
calc_bbox_overlap(fz_text_span *span, int i, fz_text_span *span2, int j)
{
	float area1, area2, area3;
	fz_rect bbox1, bbox2, intersect;

	intersect = *fz_text_char_bbox(&bbox1, span, i);
	fz_intersect_rect(&intersect, fz_text_char_bbox(&bbox2, span2, j));
	if (fz_is_empty_rect(&intersect))
		return 0;

	area1 = (bbox1.x1 - bbox1.x0) * (bbox1.y1 - bbox1.y0);
	area2 = (bbox2.x1 - bbox2.x0) * (bbox2.y1 - bbox2.y0);
	area3 = (intersect.x1 - intersect.x0) * (intersect.y1 - intersect.y0);

	return area3 / fz_max(area1, area2);
}

static inline int
is_same_c(fz_text_span *span, int i, fz_text_span *span2, int j)
{
	return i < span->len && j < span2->len && span->text[i].c == span2->text[j].c;
}

static int
do_glyphs_overlap(fz_text_span *span, int i, fz_text_span *span2, int j, int start)
{
	return
		is_same_c(span, i, span2, j) &&
		// calc_bbox_overlap is too imprecise for small glyphs
		(!start || span->text[i].style->size >= 5 && span2->text[j].style->size >= 5) &&
		(calc_bbox_overlap(span, i, span2, j) >
		 // if only a single glyph overlaps, require slightly more overlapping
		 (start && !is_same_c(span, i + 1, span2, j + 1) ? 0.8f : 0.7f) ||
		 // bboxes of slim glyphs sometimes don't overlap enough, so
		 // check if the overlapping continues with the following two glyphs
		 is_same_c(span, i + 1, span2, j + 1) &&
		 (calc_bbox_overlap(span, i + 1, span2, j + 1) > 0.7f ||
		  is_same_c(span, i + 2, span2, j + 2) &&
		  calc_bbox_overlap(span, i + 2, span2, j + 2) > 0.7f));
}

static void
merge_lines(fz_context *ctx, fz_text_block *block, fz_text_line *line)
{
	if (line == block->lines + block->len - 1)
		return;
	while ((line + 1)->first_span)
	{
		fz_union_rect(&line->bbox, &(line + 1)->first_span->bbox);
		line->last_span = line->last_span->next = (line + 1)->first_span;
		(line + 1)->first_span = (line + 1)->first_span->next;
	}
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
		for (span = line->first_span; span; span = span->next)
		{
			for (i = 0; i < span->len && i < 512; i++)
			{
				fz_text_line *line2 = line;
				fz_text_span *span2 = span;
				int j = i + 1;
				for (;;)
				{
					if (!span2)
					{
						if (line2 + 1 == block->lines + block->len || line2 != line || !(line2 + 1)->first_span)
							break;
						line2++;
						span2 = line2->first_span;
					}
					for (; j < span2->len && j < 512; j++)
					{
						int c = span->text[i].c;
						if (c != 32 && c == span2->text[j].c && do_glyphs_overlap(span, i, span2, j, 1))
							goto fixup_delete_duplicates;
					}
					span2 = span2->next;
					j = 0;
				}
				continue;

fixup_delete_duplicates:
				do
				{
					delete_character(span, i);
					if (span != span2)
						j++;
				} while (do_glyphs_overlap(span, i, span2, j, 0));

				if (i < span->len && span->text[i].c == 32)
					delete_character(span, i);
				else if (i == span->len && !span->next)
					merge_lines(ctx, block, line);
			}
		}
	}
}

static void
fixup_text_page(fz_context *ctx, fz_text_page *page)
{
	fz_page_block *block;
	fz_text_line *line;
	fz_text_span *span;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		if (block->type != FZ_PAGE_BLOCK_TEXT)
			continue;
		for (line = block->u.text->lines; line < block->u.text->lines + block->u.text->len; line++)
		{
			for (span = line->first_span; span; span = span->next)
				fixup_text_span(span);
		}
		fixup_text_block(ctx, block->u.text);
	}
}

/***** various string fixups *****/

static void
fz_text_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, ctm, colorspace, color, alpha, NULL);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, ctm, colorspace, color, alpha, stroke);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_clip_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, int accumulate)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, ctm, NULL, NULL, 0, NULL);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, ctm, NULL, NULL, 0, stroke);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_ignore_text(fz_device *dev, fz_text *text, const fz_matrix *ctm)
{
	fz_text_device *tdev = dev->user;
	fz_text_style *style;
	style = fz_lookup_text_style(dev->ctx, tdev->sheet, text, ctm, NULL, NULL, 0, NULL);
	fz_text_extract(dev->ctx, tdev, text, ctm, style);
}

static void
fz_text_fill_image_mask(fz_device *dev, fz_image *img, const fz_matrix *ctm,
		fz_colorspace *cspace, float *color, float alpha)
{
	fz_text_device *tdev = dev->user;
	fz_text_page *page = tdev->page;
	fz_image_block *block;
	fz_context *ctx = dev->ctx;

	/* SumatraPDF: fixup_text_page doesn't handle multiple blocks yet */
	return;

	/* If the alpha is less than 50% then it's probably a watermark or
	 * effect or something. Skip it */
	if (alpha < 0.5)
		return;

	/* New block */
	if (page->len == page->cap)
	{
		int newcap = (page->cap ? page->cap*2 : 4);
		page->blocks = fz_resize_array(ctx, page->blocks, newcap, sizeof(*page->blocks));
		page->cap = newcap;
	}
	block = fz_malloc_struct(ctx, fz_image_block);
	page->blocks[page->len].type = FZ_PAGE_BLOCK_IMAGE;
	page->blocks[page->len].u.image = block;
	block->image = fz_keep_image(ctx, img);
	block->cspace = fz_keep_colorspace(ctx, cspace);
	if (cspace)
		memcpy(block->colors, color, sizeof(block->colors[0])*cspace->n);
	page->len++;
}

static void
fz_text_fill_image(fz_device *dev, fz_image *img, const fz_matrix *ctm, float alpha)
{
	fz_text_fill_image_mask(dev, img, ctm, NULL, NULL, alpha);
}

static int
fz_bidi_direction(int bidiclass, int curdir)
{
	switch (bidiclass)
	{
	/* strong */
	case UCDN_BIDI_CLASS_L: return 1;
	case UCDN_BIDI_CLASS_R: return -1;
	case UCDN_BIDI_CLASS_AL: return -1;

	/* weak */
	case UCDN_BIDI_CLASS_EN:
	case UCDN_BIDI_CLASS_ES:
	case UCDN_BIDI_CLASS_ET:
	case UCDN_BIDI_CLASS_AN:
	case UCDN_BIDI_CLASS_CS:
	case UCDN_BIDI_CLASS_NSM:
	case UCDN_BIDI_CLASS_BN:
		return curdir;

	/* neutral */
	case UCDN_BIDI_CLASS_B:
	case UCDN_BIDI_CLASS_S:
	case UCDN_BIDI_CLASS_WS:
	case UCDN_BIDI_CLASS_ON:
		return curdir;

	/* embedding, override, pop ... we don't support them */
	default:
		return 0;
	}
}

static void
fz_bidi_reorder_run(fz_text_span *span, int a, int b, int dir)
{
	if (a < b && dir == -1)
	{
		fz_text_char c;
		int m = a + (b - a) / 2;
		while (a < m)
		{
			b--;
			c = span->text[a];
			span->text[a] = span->text[b];
			span->text[b] = c;
			a++;
		}
	}
}

static void
fz_bidi_reorder_span(fz_text_span *span)
{
	int a, b, dir, curdir;

	a = 0;
	curdir = 1;
	for (b = 0; b < span->len; b++)
	{
		dir = fz_bidi_direction(ucdn_get_bidi_class(span->text[b].c), curdir);
		if (dir != curdir)
		{
			fz_bidi_reorder_run(span, a, b, curdir);
			curdir = dir;
			a = b;
		}
	}
	fz_bidi_reorder_run(span, a, b, curdir);
}

static void
fz_bidi_reorder_text_page(fz_context *ctx, fz_text_page *page)
{
	fz_page_block *pageblock;
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;

	for (pageblock = page->blocks; pageblock < page->blocks + page->len; pageblock++)
		if (pageblock->type == FZ_PAGE_BLOCK_TEXT)
			for (block = pageblock->u.text, line = block->lines; line < block->lines + block->len; line++)
				for (span = line->first_span; span; span = span->next)
					fz_bidi_reorder_span(span);
}

static void
fz_text_begin_page(fz_device *dev, const fz_rect *mediabox, const fz_matrix *ctm)
{
	fz_context *ctx = dev->ctx;
	fz_text_device *tdev = dev->user;

	if (tdev->page->len)
	{
		tdev->page->next = fz_new_text_page(ctx);
		tdev->page = tdev->page->next;
	}

	tdev->page->mediabox = *mediabox;
	fz_transform_rect(&tdev->page->mediabox, ctm);

	tdev->spans = new_span_soup(ctx);
}

static void
fz_text_end_page(fz_device *dev)
{
	fz_context *ctx = dev->ctx;
	fz_text_device *tdev = dev->user;

	add_span_to_soup(tdev->spans, tdev->cur_span);
	tdev->cur_span = NULL;

	strain_soup(ctx, tdev);
	free_span_soup(tdev->spans);
	tdev->spans = NULL;

	/* TODO: smart sorting of blocks in reading order */
	/* TODO: unicode NFC normalization */

	fz_bidi_reorder_text_page(ctx, tdev->page);

	/* SumatraPDF: various string fixups */
	fixup_text_page(dev->ctx, tdev->page);
}

static void
fz_text_free_user(fz_device *dev)
{
	fz_text_device *tdev = dev->user;
	free_span_soup(tdev->spans);
	fz_free(dev->ctx, tdev);
}

fz_device *
fz_new_text_device(fz_context *ctx, fz_text_sheet *sheet, fz_text_page *page)
{
	fz_device *dev;

	fz_text_device *tdev = fz_malloc_struct(ctx, fz_text_device);
	tdev->sheet = sheet;
	tdev->page = page;
	tdev->spans = NULL;
	tdev->cur_span = NULL;
	tdev->lastchar = ' ';

	dev = fz_new_device(ctx, tdev);
	dev->hints = FZ_IGNORE_IMAGE | FZ_IGNORE_SHADE;
	dev->begin_page = fz_text_begin_page;
	dev->end_page = fz_text_end_page;
	dev->free_user = fz_text_free_user;
	dev->fill_text = fz_text_fill_text;
	dev->stroke_text = fz_text_stroke_text;
	dev->clip_text = fz_text_clip_text;
	dev->clip_stroke_text = fz_text_clip_stroke_text;
	dev->ignore_text = fz_text_ignore_text;
	dev->fill_image = fz_text_fill_image;
	dev->fill_image_mask = fz_text_fill_image_mask;

	return dev;
}
