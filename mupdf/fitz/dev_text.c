#include "fitz-internal.h"

#define LINE_DIST 0.9f
#define SPACE_DIST 0.2f
#define SPACE_MAX_DIST 0.8f
#define PARAGRAPH_DIST 0.5f
#define MY_EPSILON 0.001f
#define SUBSCRIPT_OFFSET 0.2f
#define SUPERSCRIPT_OFFSET -0.2f

#undef DEBUG_SPANS
#undef DEBUG_INTERNALS
#undef DEBUG_LINE_HEIGHTS
#undef DEBUG_MASKS
#undef DEBUG_ALIGN
#undef DEBUG_INDENTS

#undef SPOT_LINE_NUMBERS

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

	if (new_line)
	{
		/* SumatraPDF: fixup_text_page doesn't handle multiple blocks yet * /
		float size = fz_matrix_expansion(&span->transform);
		/* So, a new line. Part of the same block or not? */
		if (/* distance == 0 || distance > size * 1.5 || distance < -size * PARAGRAPH_DIST || */ page->len == 0)
		{
			/* New block */
			if (page->len == page->cap)
			{
				int newcap = (page->cap ? page->cap*2 : 4);
				page->blocks = fz_resize_array(ctx, page->blocks, newcap, sizeof(*page->blocks));
				page->cap = newcap;
			}
			page->blocks[page->len].cap = 0;
			page->blocks[page->len].len = 0;
			page->blocks[page->len].lines = 0;
			page->blocks[page->len].bbox = fz_empty_rect;
			page->len++;
			distance = 0;
		}

		/* New line */
		block = &page->blocks[page->len-1];
		if (block->len == block->cap)
		{
			int newcap = (block->cap ? block->cap*2 : 4);
			block->lines = fz_resize_array(ctx, block->lines, newcap, sizeof(*block->lines));
			block->cap = newcap;
		}
		block->lines[block->len].cap = 0;
		block->lines[block->len].len = 0;
		block->lines[block->len].spans = NULL;
		block->lines[block->len].distance = distance;
		block->lines[block->len].bbox = fz_empty_rect;
		block->len++;
	}

	/* Find last line and append to it */
	block = &page->blocks[page->len-1];
	line = &block->lines[block->len-1];

	if (line->len == line->cap)
	{
		int newcap = (line->cap ? line->cap*2 : 4);
		line->spans = fz_resize_array(ctx, line->spans, newcap, sizeof(*line->spans));
		line->cap = newcap;
	}
	fz_union_rect(&block->lines[block->len-1].bbox, &span->bbox);
	fz_union_rect(&block->bbox, &span->bbox);
	span->base_offset = (new_line ? 0 : distance);
	line->spans[line->len++] = span;
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

static inline void
normalise(fz_point *p)
{
	float len = p->x * p->x + p->y * p->y;
	if (len != 0)
	{
		len = sqrtf(len);
		p->x /= len;
		p->y /= len;
	}
}

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

			p.x = last_line->spans[0]->max.x - last_line->spans[0]->min.x;
			p.y = last_line->spans[0]->max.y - last_line->spans[0]->min.y;
			normalise(&p);
			q.x = span->max.x - span->min.x;
			q.y = span->max.y - span->min.y;
			normalise(&q);
#ifdef DEBUG_SPANS
			printf("last_span=%g %g -> %g %g = %g %g\n", last_span->min.x, last_span->min.y, last_span->max.x, last_span->max.y, p.x, p.y);
			printf("span     =%g %g -> %g %g = %g %g\n", span->min.x, span->min.y, span->max.x, span->max.y, q.x, q.y);
#endif
			perp_r.y = last_line->spans[0]->min.x - span->min.x;
			perp_r.x = -(last_line->spans[0]->min.y - span->min.y);
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
fz_new_text_page(fz_context *ctx, const fz_rect *mediabox)
{
	fz_text_page *page = fz_malloc(ctx, sizeof(*page));
	page->mediabox = *mediabox;
	page->len = 0;
	page->cap = 0;
	page->blocks = NULL;
	return page;
}

static void
fz_free_text_line_contents(fz_context *ctx, fz_text_line *line)
{
	int span_num;
	for (span_num = 0; span_num < line->len; span_num++)
	{
		fz_text_span *span = line->spans[span_num];
		fz_free(ctx, span->text);
		fz_free(ctx, span);
	}
	fz_free(ctx, line->spans);
}

void
fz_free_text_page(fz_context *ctx, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
			fz_free_text_line_contents(ctx, line);
		fz_free(ctx, block->lines);
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
	normalise(&ndir);
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
	/* don't add spaces before spaces */
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
	int i;
	if (line == block->lines + block->len - 1)
		return;
	for (i = 0; i < (line + 1)->len; i++)
	{
		if (line->len == line->cap)
		{
			int newcap = (line->cap ? line->cap * 2 : 4);
			line->spans = fz_resize_array(ctx, line->spans, newcap, sizeof(*line->spans));
			line->cap = newcap;
		}
		fz_union_rect(&line->bbox, &(line + 1)->spans[i]->bbox);
		line->spans[line->len++] = (line + 1)->spans[i];
	}
	fz_free(ctx, (line + 1)->spans);
	memmove(line + 1, line + 2, (block->lines + block->len - (line + 2)) * sizeof(fz_text_line));
	block->len--;
}

static void
fixup_text_block(fz_context *ctx, fz_text_block *block)
{
	fz_text_line *line;
	fz_text_span *span;
	int i, span_num;

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=734 */
	/* remove duplicate character sequences in (almost) the same spot */
	for (line = block->lines; line < block->lines + block->len; line++)
	{
		for (span_num = 0; span_num < line->len; span_num++)
		{
			span = line->spans[span_num];
			for (i = 0; i < span->len; i++)
			{
				fz_text_span *span2;
				fz_text_line *line2 = line;
				int j = i + 1, span_num2 = span_num;
				for (span_num2 = span_num; span_num2 <= line2->len; span_num2++, j = 0)
				{
					if (span_num2 == line2->len)
					{
						if (line2 + 1 == block->lines + block->len || line2 != line || (line2 + 1)->len == 0)
							break;
						line2++;
						span_num2 = 0;
					}
					span2 = line2->spans[span_num2];
					for (; j < span2->len; j++)
					{
						int c = span->text[i].c;
						if (c != 32 && c == span2->text[j].c && do_glyphs_overlap(span, i, span2, j, 1))
							goto fixup_delete_duplicates;
					}
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
				else if (i == span->len && span_num == line->len - 1)
				{
					int len = line->len;
					merge_lines(ctx, block, line);
					span = line->spans[len - 1];
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
	int span_num;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span_num = 0; span_num < line->len; span_num++)
				fixup_text_span(line->spans[span_num]);
		}
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
fz_text_free_user(fz_device *dev)
{
	fz_context *ctx = dev->ctx;
	fz_text_device *tdev = dev->user;

	add_span_to_soup(tdev->spans, tdev->cur_span);
	tdev->cur_span = NULL;

	strain_soup(ctx, tdev);
	free_span_soup(tdev->spans);

	/* TODO: smart sorting of blocks in reading order */
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
	tdev->spans = new_span_soup(ctx);
	tdev->cur_span = NULL;
	tdev->lastchar = ' ';

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
fz_print_style_begin(fz_output *out, fz_text_style *style)
{
	int script = style->script;
	fz_printf(out, "<span class=\"s%d\">", style->id);
	while (script-- > 0)
		fz_printf(out, "<sup>");
	while (++script < 0)
		fz_printf(out, "<sub>");
}

static void
fz_print_style_end(fz_output *out, fz_text_style *style)
{
	int script = style->script;
	while (script-- > 0)
		fz_printf(out, "</sup>");
	while (++script < 0)
		fz_printf(out, "</sub>");
	fz_printf(out, "</span>");
}

static void
fz_print_style(fz_output *out, fz_text_style *style)
{
	char *s = strchr(style->font->name, '+');
	s = s ? s + 1 : style->font->name;
	fz_printf(out, "span.s%d{font-family:\"%s\";font-size:%gpt;",
		style->id, s, style->size);
	if (font_is_italic(style->font))
		fz_printf(out, "font-style:italic;");
	if (font_is_bold(style->font))
		fz_printf(out, "font-weight:bold;");
	fz_printf(out, "}\n");
}

void
fz_print_text_sheet(fz_context *ctx, fz_output *out, fz_text_sheet *sheet)
{
	fz_text_style *style;
	for (style = sheet->style; style; style = style->next)
		fz_print_style(out, style);
}

void
fz_print_text_page_html(fz_context *ctx, fz_output *out, fz_text_page *page)
{
	int block_n, line_n, span_n, ch_n;
	fz_text_style *style = NULL;
	fz_text_block *block;
	fz_text_line *line;
	void *last_region = NULL;

	fz_printf(out, "<div class=\"page\">\n");

	for (block_n = 0; block_n < page->len; block_n++)
	{
		block = &page->blocks[block_n];
		fz_printf(out, "<div class=\"block\"><p>\n");
		for (line_n = 0; line_n < block->len; line_n++)
		{
			int lastcol=-1;
			line = &block->lines[line_n];
			style = NULL;

			if (line->region != last_region)
			{
				if (last_region)
					fz_printf(out, "</div>");
				fz_printf(out, "<div class=\"metaline\">");
				last_region = line->region;
			}
			fz_printf(out, "<div class=\"line\"");
#ifdef DEBUG_INTERNALS
			if (line->region)
				fz_printf(out, " region=\"%x\"", line->region);
#endif
			fz_printf(out, ">");
			for (span_n = 0; span_n < line->len; span_n++)
			{
				fz_text_span *span = line->spans[span_n];
				float size = fz_matrix_expansion(&span->transform);
				float base_offset = span->base_offset / size;

				if (lastcol != span->column)
				{
					if (lastcol >= 0)
					{
						fz_printf(out, "</div>");
					}
					/* If we skipped any columns then output some spacer spans */
					while (lastcol < span->column-1)
					{
						fz_printf(out, "<div class=\"cell\"></div>");
						lastcol++;
					}
					lastcol++;
					/* Now output the span to contain this entire column */
					fz_printf(out, "<div class=\"cell\" style=\"");
					{
						int sn;
						for (sn = span_n+1; sn < line->len; sn++)
						{
							if (line->spans[sn]->column != lastcol)
								break;
						}
						fz_printf(out, "width:%g%%;align:%s", span->column_width, (span->align == 0 ? "left" : (span->align == 1 ? "center" : "right")));
					}
					if (span->indent > 1)
						fz_printf(out, ";padding-left:1em;text-indent:-1em");
					if (span->indent < -1)
						fz_printf(out, ";text-indent:1em");
					fz_printf(out, "\">");
				}
#ifdef DEBUG_INTERNALS
				fz_printf(out, "<span class=\"internal_span\"");
				if (span->column)
					fz_printf(out, " col=\"%x\"", span->column);
				fz_printf(out, ">");
#endif
				if (span->spacing >= 1)
					fz_printf(out, " ");
				if (base_offset > SUBSCRIPT_OFFSET)
					fz_printf(out, "<sub>");
				else if (base_offset < SUPERSCRIPT_OFFSET)
					fz_printf(out, "<sup>");
				for (ch_n = 0; ch_n < span->len; ch_n++)
				{
					fz_text_char *ch = &span->text[ch_n];
					if (style != ch->style)
					{
						if (style)
							fz_print_style_end(out, style);
						fz_print_style_begin(out, ch->style);
						style = ch->style;
					}

					if (ch->c == '<')
						fz_printf(out, "&lt;");
					else if (ch->c == '>')
						fz_printf(out, "&gt;");
					else if (ch->c == '&')
						fz_printf(out, "&amp;");
					else if (ch->c >= 32 && ch->c <= 127)
						fz_printf(out, "%c", ch->c);
					else
						fz_printf(out, "&#x%x;", ch->c);
				}
				if (style)
				{
					fz_print_style_end(out, style);
					style = NULL;
				}
				if (base_offset > SUBSCRIPT_OFFSET)
					fz_printf(out, "</sub>");
				else if (base_offset < SUPERSCRIPT_OFFSET)
					fz_printf(out, "</sup>");
#ifdef DEBUG_INTERNALS
				fz_printf(out, "</span>");
#endif
			}
			/* Close our floating span */
			fz_printf(out, "</div>");
#ifdef DEBUG_INTERNALS
#endif
			/* Close the line */
			fz_printf(out, "</div>");
			fz_printf(out, "\n");
		}
		/* Close the metaline */
		fz_printf(out, "</div>");
		last_region = NULL;
		fz_printf(out, "</p></div>\n");
	}

	fz_printf(out, "</div>\n");
}

void
fz_print_text_page_xml(fz_context *ctx, fz_output *out, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	char *s;

	fz_printf(out, "<page>\n");
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		fz_printf(out, "<block bbox=\"%g %g %g %g\">\n",
			block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			int span_num;
			fz_printf(out, "<line bbox=\"%g %g %g %g\">\n",
				line->bbox.x0, line->bbox.y0, line->bbox.x1, line->bbox.y1);
			for (span_num = 0; span_num < line->len; span_num++)
			{
				fz_text_span *span = line->spans[span_num];
				fz_text_style *style = NULL;
				int char_num;
				for (char_num = 0; char_num < span->len; char_num++)
				{
					fz_text_char *ch = &span->text[char_num];
					if (ch->style != style)
					{
						if (style)
						{
							fz_printf(out, "</span>\n");
						}
						style = ch->style;
						s = strchr(style->font->name, '+');
						s = s ? s + 1 : style->font->name;
						fz_printf(out, "<span bbox=\"%g %g %g %g\" font=\"%s\" size=\"%g\">\n",
							span->bbox.x0, span->bbox.y0, span->bbox.x1, span->bbox.y1,
							s, style->size);
					}
					{
						fz_rect rect;
						fz_text_char_bbox(&rect, span, char_num);
						fz_printf(out, "<char bbox=\"%g %g %g %g\" x=\"%g\" y=\"%g\" c=\"",
							rect.x0, rect.y0, rect.x1, rect.y1, ch->p.x, ch->p.y);
					}
					switch (ch->c)
					{
					case '<': fz_printf(out, "&lt;"); break;
					case '>': fz_printf(out, "&gt;"); break;
					case '&': fz_printf(out, "&amp;"); break;
					case '"': fz_printf(out, "&quot;"); break;
					case '\'': fz_printf(out, "&apos;"); break;
					default:
						if (ch->c >= 32 && ch->c <= 127)
							fz_printf(out, "%c", ch->c);
						else
							fz_printf(out, "&#x%x;", ch->c);
						break;
					}
					fz_printf(out, "\"/>\n");
				}
				if (style)
					fz_printf(out, "</span>\n");
			}
			fz_printf(out, "</line>\n");
		}
		fz_printf(out, "</block>\n");
	}
	fz_printf(out, "</page>\n");
}

void
fz_print_text_page(fz_context *ctx, fz_output *out, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_char *ch;
	char utf[10];
	int i, n;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			int span_num;
			for (span_num = 0; span_num < line->len; span_num++)
			{
				fz_text_span *span = line->spans[span_num];
				for (ch = span->text; ch < span->text + span->len; ch++)
				{
					n = fz_runetochar(utf, ch->c);
					for (i = 0; i < n; i++)
						fz_printf(out, "%c", utf[i]);
				}
				/* SumatraPDF: separate spans with spaces */
				if (span_num < line->len - 1 && span->len > 0 && span->text[span->len - 1].c != ' ')
					fz_printf(out, " ");
			}
			fz_printf(out, "\n");
		}
		fz_printf(out, "\n");
	}
}

typedef struct line_height_s
{
	float height;
	int count;
	fz_text_style *style;
} line_height;

typedef struct line_heights_s
{
	fz_context *ctx;
	int cap;
	int len;
	line_height *lh;
} line_heights;

static line_heights *
new_line_heights(fz_context *ctx)
{
	line_heights *lh = fz_malloc_struct(ctx, line_heights);
	lh->ctx = ctx;
	return lh;
}

static void
free_line_heights(line_heights *lh)
{
	if (!lh)
		return;
	fz_free(lh->ctx, lh->lh);
	fz_free(lh->ctx, lh);
}

static void
insert_line_height(line_heights *lh, fz_text_style *style, float height)
{
	int i;

#ifdef DEBUG_LINE_HEIGHTS
	printf("style=%x height=%g\n", style, height);
#endif

	/* If we have one already, add it in */
	for (i=0; i < lh->cap; i++)
	{
		/* Match if we are within 5% */
		if (lh->lh[i].style == style && lh->lh[i].height * 0.95 <= height && lh->lh[i].height * 1.05 >= height)
		{
			/* Ensure that the average height is correct */
			lh->lh[i].height = (lh->lh[i].height * lh->lh[i].count + height) / (lh->lh[i].count+1);
			lh->lh[i].count++;
			return;
		}
	}

	/* Otherwise extend (if required) and add it */
	if (lh->cap == lh->len)
	{
		int newcap = (lh->cap ? lh->cap * 2 : 4);
		lh->lh = fz_resize_array(lh->ctx, lh->lh, newcap, sizeof(line_height));
		lh->cap = newcap;
	}

	lh->lh[lh->len].count = 1;
	lh->lh[lh->len].height = height;
	lh->lh[lh->len].style = style;
	lh->len++;
}

static void
cull_line_heights(line_heights *lh)
{
	int i, j, k;

#ifdef DEBUG_LINE_HEIGHTS
	printf("Before culling:\n");
	for (i = 0; i < lh->len; i++)
	{
		fz_text_style *style = lh->lh[i].style;
		printf("style=%x height=%g count=%d\n", style, lh->lh[i].height, lh->lh[i].count);
	}
#endif
	for (i = 0; i < lh->len; i++)
	{
		fz_text_style *style = lh->lh[i].style;
		int count = lh->lh[i].count;
		int max = i;

		/* Find the max for this style */
		for (j = i+1; j < lh->len; j++)
		{
			if (lh->lh[j].style == style && lh->lh[j].count > count)
			{
				max = j;
				count = lh->lh[j].count;
			}
		}

		/* Destroy all the ones other than the max */
		if (max != i)
		{
			lh->lh[i].count = count;
			lh->lh[i].height = lh->lh[max].height;
			lh->lh[max].count = 0;
		}
		j = i+1;
		for (k = j; k < lh->len; k++)
		{
			if (lh->lh[k].style != style)
				lh->lh[j++] = lh->lh[k];
		}
		lh->len = j;
	}
#ifdef DEBUG_LINE_HEIGHTS
	printf("After culling:\n");
	for (i = 0; i < lh->len; i++)
	{
		fz_text_style *style = lh->lh[i].style;
		printf("style=%x height=%g count=%d\n", style, lh->lh[i].height, lh->lh[i].count);
	}
#endif
}

static float
line_height_for_style(line_heights *lh, fz_text_style *style)
{
	int i;

	for (i=0; i < lh->len; i++)
	{
		if (lh->lh[i].style == style)
			return lh->lh[i].height;
	}
	return 0.0; /* Never reached */
}

static void
split_block(fz_context *ctx, fz_text_page *page, int block_num, int linenum)
{
	int split_len;

	if (page->len == page->cap)
	{
		int new_cap = fz_maxi(16, page->cap * 2);
		page->blocks = fz_resize_array(ctx, page->blocks, new_cap, sizeof(*page->blocks));
		page->cap = new_cap;
	}

	memmove(page->blocks+block_num+1, page->blocks+block_num, (page->len - block_num)*sizeof(*page->blocks));
	page->len++;

	split_len = page->blocks[block_num].len - linenum;
	page->blocks[block_num+1].bbox = page->blocks[block_num].bbox; /* FIXME! */
	page->blocks[block_num+1].cap = 0;
	page->blocks[block_num+1].len = 0;
	page->blocks[block_num+1].lines = NULL;
	page->blocks[block_num+1].lines = fz_malloc_array(ctx, split_len, sizeof(fz_text_line));
	page->blocks[block_num+1].cap = page->blocks[block_num+1].len;
	page->blocks[block_num+1].len = split_len;
	page->blocks[block_num].len = linenum;
	memcpy(page->blocks[block_num+1].lines, page->blocks[block_num].lines + linenum, split_len * sizeof(fz_text_line));
	page->blocks[block_num+1].lines[0].distance = 0;
}

static inline int
is_unicode_wspace(int c)
{
	return (c == 9 || /* TAB */
		c == 0x0a || /* HT */
		c == 0x0b || /* LF */
		c == 0x0c || /* VT */
		c == 0x0d || /* FF */
		c == 0x20 || /* CR */
		c == 0x85 || /* NEL */
		c == 0xA0 || /* No break space */
		c == 0x1680 || /* Ogham space mark */
		c == 0x180E || /* Mongolian Vowel Separator */
		c == 0x2000 || /* En quad */
		c == 0x2001 || /* Em quad */
		c == 0x2002 || /* En space */
		c == 0x2003 || /* Em space */
		c == 0x2004 || /* Three-per-Em space */
		c == 0x2005 || /* Four-per-Em space */
		c == 0x2006 || /* Five-per-Em space */
		c == 0x2007 || /* Figure space */
		c == 0x2008 || /* Punctuation space */
		c == 0x2009 || /* Thin space */
		c == 0x200A || /* Hair space */
		c == 0x2028 || /* Line separator */
		c == 0x2029 || /* Paragraph separator */
		c == 0x202F || /* Narrow no-break space */
		c == 0x205F || /* Medium mathematical space */
		c == 0x3000); /* Ideographic space */
}

static inline int
is_unicode_bullet(int c)
{
	/* The last 2 aren't strictly bullets, but will do for our usage here */
	return (c == 0x2022 || /* Bullet */
		c == 0x2023 || /* Triangular bullet */
		c == 0x25e6 || /* White bullet */
		c == 0x2043 || /* Hyphen bullet */
		c == 0x2219 || /* Bullet operator */
		c == 149 || /* Ascii bullet */
		c == '*');
}

static inline int
is_number(int c)
{
	return ((c >= '0' && c <= '9') ||
		(c == '.'));
}

static inline int
is_latin_char(int c)
{
	return ((c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z'));
}

static inline int
is_roman(int c)
{
	return (c == 'i' || c == 'I' ||
		c == 'v' || c == 'V' ||
		c == 'x' || c == 'X' ||
		c == 'l' || c == 'L' ||
		c == 'c' || c == 'C' ||
		c == 'm' || c == 'M');
}

static int
is_list_entry(fz_text_span *span, int *char_num_ptr, int span_num)
{
	int char_num;
	fz_text_char *chr;

	/* First, skip over any whitespace */
	for (char_num = 0; char_num < span->len; char_num++)
	{
		chr = &span->text[char_num];
		if (!is_unicode_wspace(chr->c))
			break;
	}
	*char_num_ptr = char_num;

	if (span_num != 0 || char_num >= span->len)
		return 0;

	/* Now we check for various special cases, which we consider to mean
	 * that this is probably a list entry and therefore should always count
	 * as a separate paragraph (and hence not be entered in the line height
	 * table). */
	chr = &span->text[char_num];

	/* Is the first char on the line, a bullet point? */
	if (is_unicode_bullet(chr->c))
		return 1;

#ifdef SPOT_LINE_NUMBERS
	/* Is the entire first span a number? Or does it start with a number
	 * followed by ) or : ? Allow to involve single latin chars too. */
	if (is_number(chr->c) || is_latin_char(chr->c))
	{
		int cn = char_num;
		int met_char = is_latin_char(chr->c);
		for (cn = char_num+1; cn < span->len; cn++)
		{
			fz_text_char *chr2 = &span->text[cn];

			if (is_latin_char(chr2->c) && !met_char)
			{
				met_char = 1;
				continue;
			}
			met_char = 0;
			if (!is_number(chr2->c) && !is_unicode_wspace(chr2->c))
				break;
			else if (chr2->c == ')' || chr2->c == ':')
			{
				cn = span->len;
				break;
			}
		}
		if (cn == span->len)
			return 1;
	}

	/* Is the entire first span a roman numeral? Or does it start with
	 * a roman numeral followed by ) or : ? */
	if (is_roman(chr->c))
	{
		int cn = char_num;
		for (cn = char_num+1; cn < span->len; cn++)
		{
			fz_text_char *chr2 = &span->text[cn];

			if (!is_roman(chr2->c) && !is_unicode_wspace(chr2->c))
				break;
			else if (chr2->c == ')' || chr2->c == ':')
			{
				cn = span->len;
				break;
			}
		}
		if (cn == span->len)
			return 1;
	}
#endif
	return 0;
}

typedef struct region_masks_s region_masks;

typedef struct region_mask_s region_mask;

typedef struct region_s region;

struct region_s
{
	float start;
	float stop;
	float ave_start;
	float ave_stop;
	int align;
	float colw;
};

struct region_mask_s
{
	fz_context *ctx;
	int freq;
	fz_point blv;
	int cap;
	int len;
	float size;
	region *mask;
};

struct region_masks_s
{
	fz_context *ctx;
	int cap;
	int len;
	region_mask **mask;
};

static region_masks *
new_region_masks(fz_context *ctx)
{
	region_masks *rms = fz_malloc_struct(ctx, region_masks);
	rms->ctx = ctx;
	rms->cap = 0;
	rms->len = 0;
	rms->mask = NULL;
	return rms;
}

static void
free_region_mask(region_mask *rm)
{
	if (!rm)
		return;
	fz_free(rm->ctx, rm->mask);
	fz_free(rm->ctx, rm);
}

static void
free_region_masks(region_masks *rms)
{
	int i;

	if (!rms)
		return;
	for (i=0; i < rms->len; i++)
	{
		free_region_mask(rms->mask[i]);
	}
	fz_free(rms->ctx, rms->mask);
	fz_free(rms->ctx, rms);
}

static int region_masks_mergeable(const region_mask *rm1, const region_mask *rm2, float *score)
{
	int i1, i2;
	int count = 0;

	*score = 0;
	if (fabsf(rm1->blv.x-rm2->blv.x) >= MY_EPSILON || fabsf(rm1->blv.y-rm2->blv.y) >= MY_EPSILON)
		return 0;

	for (i1 = 0, i2 = 0; i1 < rm1->len && i2 < rm2->len; )
	{
		if (rm1->mask[i1].stop < rm2->mask[i2].start)
		{
			/* rm1's region is entirely before rm2's */
			*score += rm1->mask[i1].stop - rm1->mask[i1].start;
			i1++;
		}
		else if (rm1->mask[i1].start > rm2->mask[i2].stop)
		{
			/* rm2's region is entirely before rm1's */
			*score += rm2->mask[i2].stop - rm2->mask[i2].start;
			i2++;
		}
		else
		{
			float lscore, rscore;
			if (rm1->mask[i1].start < rm2->mask[i2].start)
			{
				if (i2 > 0 && rm2->mask[i2-1].stop >= rm1->mask[i1].start)
					return 0; /* Not compatible */
				lscore = rm2->mask[i2].start - rm1->mask[i1].start;
			}
			else
			{
				if (i1 > 0 && rm1->mask[i1-1].stop >= rm2->mask[i2].start)
					return 0; /* Not compatible */
				lscore = rm1->mask[i1].start - rm2->mask[i2].start;
			}
			if (rm1->mask[i1].stop > rm2->mask[i2].stop)
			{
				if (i2+1 < rm2->len && rm2->mask[i2+1].start <= rm1->mask[i1].stop)
					return 0; /* Not compatible */
				rscore = rm1->mask[i1].stop - rm2->mask[i2].stop;
			}
			else
			{
				if (i1+1 < rm1->len && rm1->mask[i1+1].start <= rm2->mask[i2].stop)
					return 0; /* Not compatible */
				rscore = rm2->mask[i2].stop - rm1->mask[i1].stop;
			}
			/* In order to allow a region to merge, either the
			 * left, the right, or the centre must agree */
			if (lscore < 1)
			{
				if (rscore < 1)
				{
					rscore = 0;
				}
				lscore = 0;
			}
			else if (rscore < 1)
			{
				rscore = 0;
			}
			else
			{
				/* Neither Left or right agree. Does the centre? */
				float ave1 = rm1->mask[i1].start + rm1->mask[i1].stop;
				float ave2 = rm2->mask[i2].start + rm2->mask[i2].stop;
				if (fabsf(ave1-ave2) > 1)
				{
					/* Nothing agrees, so don't merge */
					return 0;
				}
				lscore = 0;
				rscore = 0;
			}
			*score += lscore + rscore;
			/* These two regions could be merged */
			i1++;
			i2++;
		}
		count++;
	}
	count += rm1->len-i1 + rm2->len-i2;
	return count;
}

static int region_mask_matches(const region_mask *rm1, const region_mask *rm2, float *score)
{
	int i1, i2;
	int close = 1;

	*score = 0;
	if (fabsf(rm1->blv.x-rm2->blv.x) >= MY_EPSILON || fabsf(rm1->blv.y-rm2->blv.y) >= MY_EPSILON)
		return 0;

	for (i1 = 0, i2 = 0; i1 < rm1->len && i2 < rm2->len; )
	{
		if (rm1->mask[i1].stop < rm2->mask[i2].start)
		{
			/* rm1's region is entirely before rm2's */
			*score += rm1->mask[i1].stop - rm1->mask[i1].start;
			i1++;
		}
		else if (rm1->mask[i1].start > rm2->mask[i2].stop)
		{
			/* Not compatible */
			return 0;
		}
		else
		{
			float lscore, rscore;
			if (rm1->mask[i1].start > rm2->mask[i2].start)
			{
				/* Not compatible */
				return 0;
			}
			if (rm1->mask[i1].stop < rm2->mask[i2].stop)
			{
				/* Not compatible */
				return 0;
			}
			lscore = rm2->mask[i2].start - rm1->mask[i1].start;
			rscore = rm1->mask[i1].stop - rm2->mask[i2].stop;
			if (lscore < 1)
			{
				if (rscore < 1)
					close++;
				close++;
			}
			else if (rscore < 1)
				close++;
			else if (fabsf(lscore - rscore) < 1)
			{
				lscore = fabsf(lscore-rscore);
				rscore = 0;
				close++;
			}
			*score += lscore + rscore;
			i1++;
			i2++;
		}
	}
	if (i1 < rm1->len)
	{
		/* Still more to go in rm1 */
		if (rm1->mask[i1].start < rm2->mask[rm2->len-1].stop)
			return 0;
	}
	else if (i2 < rm2->len)
	{
		/* Still more to go in rm2 */
		if (rm2->mask[i2].start < rm1->mask[rm1->len-1].stop)
			return 0;
	}

	return close;
}

static void region_mask_merge(region_mask *rm1, const region_mask *rm2, int newlen)
{
	int o, i1, i2;

	/* First, ensure that rm1 is long enough */
	if (rm1->cap < newlen)
	{
		int newcap = rm1->cap ? rm1->cap : 2;
		do
		{
			newcap *= 2;
		}
		while (newcap < newlen);
		rm1->mask = fz_resize_array(rm1->ctx, rm1->mask, newcap, sizeof(*rm1->mask));
		rm1->cap = newcap;
	}

	/* Now run backwards along rm1, filling it out with the merged regions */
	for (o = newlen-1, i1 = rm1->len-1, i2 = rm2->len-1; o >= 0; o--)
	{
		/* So we read from i1 and i2 and store in o */
		if (i1 < 0)
		{
			/* Just copy i2 */
			rm1->mask[o] = rm2->mask[i2];
			i2--;
		}
		else if (i2 < 0)
		{
			/* Just copy i1 */
			rm1->mask[o] = rm1->mask[i1];
			i1--;
		}
		else if (rm1->mask[i1].stop < rm2->mask[i2].start)
		{
			/* rm1's region is entirely before rm2's - copy rm2's */
			rm1->mask[o] = rm2->mask[i2];
			i2--;
		}
		else if (rm2->mask[i2].stop < rm1->mask[i1].start)
		{
			/* rm2's region is entirely before rm1's - copy rm1's */
			rm1->mask[o] = rm1->mask[i1];
			i1--;
		}
		else
		{
			/* We must be merging */
			rm1->mask[o].ave_start = (rm1->mask[i1].start * rm1->freq + rm2->mask[i2].start * rm2->freq)/(rm1->freq + rm2->freq);
			rm1->mask[o].ave_stop = (rm1->mask[i1].stop * rm1->freq + rm2->mask[i2].stop * rm2->freq)/(rm1->freq + rm2->freq);
			rm1->mask[o].start = fz_min(rm1->mask[i1].start, rm2->mask[i2].start);
			rm1->mask[o].stop = fz_max(rm1->mask[i1].stop, rm2->mask[i2].stop);
			i1--;
			i2--;
		}
	}
	rm1->freq += rm2->freq;
	rm1->len = newlen;
}

static region_mask *region_masks_match(const region_masks *rms, const region_mask *rm, fz_text_line *line, region_mask *prev_match)
{
	int i;
	float best_score = 9999999;
	float score;
	int best = -1;
	int best_count = 0;

	/* If the 'previous match' matches, use it regardless. */
	if (prev_match && region_mask_matches(prev_match, rm, &score))
	{
		return prev_match;
	}

	/* Run through and find the 'most compatible' region mask. We are
	 * guaranteed that there will always be at least one compatible one!
	 */
	for (i=0; i < rms->len; i++)
	{
		int count = region_mask_matches(rms->mask[i], rm, &score);
		if (count > best_count || (count == best_count && (score < best_score || best == -1)))
		{
			best = i;
			best_score = score;
			best_count = count;
		}
	}
	assert(best >= 0 && best < rms->len);

	/* So we have the matching mask. */
	return rms->mask[best];
}

#ifdef DEBUG_MASKS
static void
dump_region_mask(const region_mask *rm)
{
	int j;
	for (j = 0; j < rm->len; j++)
	{
		printf("%g->%g ", rm->mask[j].start, rm->mask[j].stop);
	}
	printf("* %d\n", rm->freq);
}

static void
dump_region_masks(const region_masks *rms)
{
	int i;

	for (i = 0; i < rms->len; i++)
	{
		region_mask *rm = rms->mask[i];
		dump_region_mask(rm);
	}
}
#endif

static void region_masks_add(region_masks *rms, region_mask *rm)
{
	/* Add rm to rms */
	if (rms->len == rms->cap)
	{
		int newcap = (rms->cap ? rms->cap * 2 : 4);
		rms->mask = fz_resize_array(rms->ctx, rms->mask, newcap, sizeof(*rms->mask));
		rms->cap = newcap;
	}
	rms->mask[rms->len] = rm;
	rms->len++;
}

static void region_masks_sort(region_masks *rms)
{
	int i, j;

	/* First calculate sizes */
	for (i=0; i < rms->len; i++)
	{
		region_mask *rm = rms->mask[i];
		float size = 0;
		for (j=0; j < rm->len; j++)
		{
			size += rm->mask[j].stop - rm->mask[j].start;
		}
		rm->size = size;
	}

	/* Now, sort on size */
	/* FIXME: bubble sort - use heapsort for efficiency */
	for (i=0; i < rms->len-1; i++)
	{
		for (j=i+1; j < rms->len; j++)
		{
			if (rms->mask[i]->size < rms->mask[j]->size)
			{
				region_mask *tmp = rms->mask[i];
				rms->mask[i] = rms->mask[j];
				rms->mask[j] = tmp;
			}
		}
	}
}

static void region_masks_merge(region_masks *rms, region_mask *rm)
{
	int i;
	float best_score = 9999999;
	float score;
	int best = -1;
	int best_count = 0;

#ifdef DEBUG_MASKS
	printf("\nAdding:\n");
	dump_region_mask(rm);
	printf("To:\n");
	dump_region_masks(rms);
#endif
	for (i=0; i < rms->len; i++)
	{
		int count = region_masks_mergeable(rms->mask[i], rm, &score);
		if (count && (score < best_score || best == -1))
		{
			best = i;
			best_count = count;
			best_score = score;
		}
	}
	if (best != -1)
	{
		region_mask_merge(rms->mask[best], rm, best_count);
#ifdef DEBUG_MASKS
		printf("Merges to give:\n");
		dump_region_masks(rms);
#endif
		free_region_mask(rm);
		return;
	}
	region_masks_add(rms, rm);
#ifdef DEBUG_MASKS
	printf("Adding new one to give:\n");
	dump_region_masks(rms);
#endif
}

static region_mask *
new_region_mask(fz_context *ctx, const fz_point *blv)
{
	region_mask *rm = fz_malloc_struct(ctx, region_mask);
	rm->ctx = ctx;
	rm->freq = 1;
	rm->blv = *blv;
	rm->cap = 0;
	rm->len = 0;
	rm->mask = NULL;
	return rm;
}

static void
region_mask_project(const region_mask *rm, const fz_point *min, const fz_point *max, float *start, float *end)
{
	/* We project min and max down onto the blv */
	float s = min->x * rm->blv.x + min->y * rm->blv.y;
	float e = max->x * rm->blv.x + max->y * rm->blv.y;
	if (s > e)
	{
		*start = e;
		*end = s;
	}
	else
	{
		*start = s;
		*end = e;
	}
}

static void
region_mask_add(region_mask *rm, const fz_point *min, const fz_point *max)
{
	float start, end;
	int i, j;

	region_mask_project(rm, min, max, &start, &end);

	/* Now add start/end into our region list. Typically we will be adding
	 * to the end of the region list, so search from there backwards. */
	for (i = rm->len; i > 0;)
	{
		if (start > rm->mask[i-1].stop)
			break;
		i--;
	}
	/* So we know that our interval can only affect list items >= i.
	 * We know that start is after our previous end. */
	if (i == rm->len || end < rm->mask[i].start)
	{
		/* Insert new one. No overlap. No merging */
		if (rm->len == rm->cap)
		{
			int newcap = (rm->cap ? rm->cap * 2 : 4);
			rm->mask = fz_resize_array(rm->ctx, rm->mask, newcap, sizeof(*rm->mask));
			rm->cap = newcap;
		}
		if (rm->len > i)
			memmove(&rm->mask[i+1], &rm->mask[i], (rm->len - i) * sizeof(*rm->mask));
		rm->mask[i].ave_start = start;
		rm->mask[i].ave_stop = end;
		rm->mask[i].start = start;
		rm->mask[i].stop = end;
		rm->len++;
	}
	else
	{
		/* Extend current one down. */
		rm->mask[i].ave_start = start;
		rm->mask[i].start = start;
		if (rm->mask[i].stop < end)
		{
			rm->mask[i].stop = end;
			rm->mask[i].ave_stop = end;
			/* Our region may now extend upwards too far */
			i++;
			j = i;
			while (j < rm->len && rm->mask[j].start <= end)
			{
				rm->mask[i-1].stop = end = rm->mask[j].stop;
				j++;
			}
			if (i != j)
			{
				/* Move everything from j down to i */
				while (j < rm->len)
				{
					rm->mask[i++] = rm->mask[j++];
				}
			}
			rm->len -= j-i;
		}
	}
}

static int
region_mask_column(region_mask *rm, const fz_point *min, const fz_point *max, int *align, float *colw, float *left_)
{
	float start, end, left, right;
	int i;

	region_mask_project(rm, min, max, &start, &end);

	for (i = 0; i < rm->len; i++)
	{
		/* The use of MY_EPSILON here is because we might be matching
		 * start/end values calculated with slightly different blv's */
		if (rm->mask[i].start - MY_EPSILON <= start && rm->mask[i].stop + MY_EPSILON >= end)
			break;
	}
	if (i >= rm->len)
	{
		*align = 0;
		*colw = 0;
		return 0;
	}
	left = start - rm->mask[i].start;
	right = rm->mask[i].stop - end;
	if (left < 1 && right < 1)
		*align = rm->mask[i].align;
	else if (left*2 <= right)
		*align = 0; /* Left */
	else if (right * 2 < left)
		*align = 2; /* Right */
	else
		*align = 1;
	*left_ = left;
	*colw = rm->mask[i].colw;
	return i;
}

static void
region_mask_alignment(region_mask *rm)
{
	int i;
	float width = 0;

	for (i = 0; i < rm->len; i++)
	{
		width += rm->mask[i].stop - rm->mask[i].start;
	}
	for (i = 0; i < rm->len; i++)
	{
		region *r = &rm->mask[i];
		float left = r->ave_start - r->start;
		float right = r->stop - r->ave_stop;
		if (left*2 <= right)
			r->align = 0; /* Left */
		else if (right * 2 < left)
			r->align = 2; /* Right */
		else
			r->align = 1;
		r->colw = 100 * (rm->mask[i].stop - rm->mask[i].start) / width;
	}
}

static void
region_masks_alignment(region_masks *rms)
{
	int i;

	for (i = 0; i < rms->len; i++)
	{
		region_mask_alignment(rms->mask[i]);
	}
}

static int
is_unicode_hyphen(int c)
{
	/* We omit 0x2011 (Non breaking hyphen) and 0x2043 (Hyphen Bullet)
	 * from this list. */
	return (c == '-' ||
		c == 0x2010 || /* Hyphen */
		c == 0x002d || /* Hyphen-Minus */
		c == 0x00ad || /* Soft hyphen */
		c == 0x058a || /* Armenian Hyphen */
		c == 0x1400 || /* Canadian Syllabive Hyphen */
		c == 0x1806); /* Mongolian Todo soft hyphen */
}

static int
is_unicode_hyphenatable(int c)
{
	/* This is a pretty ad-hoc collection. It may need tuning. */
	return ((c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') ||
		(c >= 0x00c0 && c <= 0x00d6) ||
		(c >= 0x00d8 && c <= 0x00f6) ||
		(c >= 0x00f8 && c <= 0x02af) ||
		(c >= 0x1d00 && c <= 0x1dbf) ||
		(c >= 0x1e00 && c <= 0x1eff) ||
		(c >= 0x2c60 && c <= 0x2c7f) ||
		(c >= 0xa722 && c <= 0xa78e) ||
		(c >= 0xa790 && c <= 0xa793) ||
		(c >= 0xa7a8 && c <= 0xa7af) ||
		(c >= 0xfb00 && c <= 0xfb07) ||
		(c >= 0xff21 && c <= 0xff3a) ||
		(c >= 0xff41 && c <= 0xff5a));
}

static void
dehyphenate(fz_text_span *s1, fz_text_span *s2)
{
	int i;

	for (i = s1->len-1; i > 0; i--)
		if (!is_unicode_wspace(s1->text[i].c))
			break;
	/* Can't leave an empty span. */
	if (i == 0)
		return;

	if (!is_unicode_hyphen(s1->text[i].c))
		return;
	if (!is_unicode_hyphenatable(s1->text[i-1].c))
		return;
	if (!is_unicode_hyphenatable(s2->text[0].c))
		return;
	s1->len = i;
	s2->spacing = 0;
}

void
fz_text_analysis(fz_context *ctx, fz_text_sheet *sheet, fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	line_heights *lh;
	region_masks *rms;
	int block_num;

	/* Simple paragraph analysis; look for the most common 'inter line'
	 * spacing. This will be assumed to be our line spacing. Anything
	 * more than 25% wider than this will be assumed to be a paragraph
	 * space. */

	/* Step 1: Gather the line height information */
	lh = new_line_heights(ctx);
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			/* For every style in the line, add lineheight to the
			 * record for that style. FIXME: This is a nasty n^2
			 * algorithm at the moment. */
			int span_num;
			fz_text_style *style = NULL;

			if (line->distance == 0)
				continue;

			for (span_num = 0; span_num < line->len; span_num++)
			{
				fz_text_span *span = line->spans[span_num];
				int char_num;

				if (is_list_entry(span, &char_num, span_num))
					goto list_entry;

				for (; char_num < span->len; char_num++)
				{
					fz_text_char *chr = &span->text[char_num];

					/* Ignore any whitespace chars */
					if (is_unicode_wspace(chr->c))
						continue;

					if (chr->style != style)
					{
						/* Have we had this style before? */
						int match = 0;
						int span_num2;
						for (span_num2 = 0; span_num2 < span_num; span_num2++)
						{
							fz_text_span *span2 = line->spans[span_num2];
							int char_num2;
							for (char_num2 = 0; char_num2 < span2->len; char_num2++)
							{
								fz_text_char *chr2 = &span2->text[char_num2];
								if (chr2->style == chr->style)
								{
									match = 1;
									break;
								}
							}
						}
						if (char_num > 0 && match == 0)
						{
							fz_text_span *span2 = line->spans[span_num];
							int char_num2;
							for (char_num2 = 0; char_num2 < char_num; char_num2++)
							{
								fz_text_char *chr2 = &span2->text[char_num2];
								if (chr2->style == chr->style)
								{
									match = 1;
									break;
								}
							}
						}
						if (match == 0)
							insert_line_height(lh, chr->style, line->distance);
						style = chr->style;
					}
				}
list_entry:
				{}
			}
		}
	}

	/* Step 2: Find the most popular line height for each style */
	cull_line_heights(lh);

	/* Step 3: Run through the blocks, breaking each block into two if
	 * the line height isn't right. */
	for (block_num = 0; block_num < page->len; block_num++)
	{
		int line_num;
		block = &page->blocks[block_num];
		for (line_num = 0; line_num < block->len; line_num++)
		{
			/* For every style in the line, check to see if lineheight
			 * is correct for that style. FIXME: We check each style
			 * more than once, currently. */
			int span_num;
			int ok = 0; /* -1 = early exit, split now. 0 = split. 1 = don't split. */
			fz_text_style *style = NULL;
			line = &block->lines[line_num];

			if (line->distance == 0)
				continue;

#ifdef DEBUG_LINE_HEIGHTS
			printf("line height=%g nspans=%d\n", line->distance, line->len);
#endif
			for (span_num = 0; span_num < line->len; span_num++)
			{
				fz_text_span *span = line->spans[span_num];
				int char_num;

				if (is_list_entry(span, &char_num, span_num))
					goto force_paragraph;

				/* Now we do the rest of the line */
				for (; char_num < span->len; char_num++)
				{
					fz_text_char *chr = &span->text[char_num];

					/* Ignore any whitespace chars */
					if (is_unicode_wspace(chr->c))
						continue;

					if (chr->style != style)
					{
						float proper_step = line_height_for_style(lh, chr->style);
						if (proper_step * 0.95 <= line->distance && line->distance <= proper_step * 1.05)
						{
							ok = 1;
							break;
						}
						style = chr->style;
					}
				}
				if (ok)
					break;
			}
			if (!ok)
			{
force_paragraph:
				split_block(ctx, page, block_num, line_num);
				break;
			}
		}
	}
	free_line_heights(lh);

	/* Simple line region analysis:
	 * For each line:
	 *    form a list of 'start/stop' points (henceforth a 'region mask')
	 *    find the normalised baseline vector for the line.
	 *    Store the region mask and baseline vector.
	 * Collate lines that have compatible region masks and identical
	 * baseline vectors.
	 * If the collated masks are column-like, then split into columns.
	 * Otherwise split into tables.
	 */
	rms = new_region_masks(ctx);
	/* Step 1: Form the region masks and store them into a list with the
	 * normalised baseline vectors. */
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			fz_point blv;
			region_mask *rm;
			int span_num;

#ifdef DEBUG_MASKS
			printf("Line: ");
			dump_line(line);
#endif
			blv = line->spans[0]->max;
			blv.x -= line->spans[0]->min.x;
			blv.y -= line->spans[0]->min.y;
			normalise(&blv);

			rm = new_region_mask(ctx, &blv);
			for (span_num = 0; span_num < line->len; span_num++)
			{
				fz_text_span *span = line->spans[span_num];
				fz_point *region_min = &span->min;
				fz_point *region_max = &span->max;

				/* Treat adjacent spans as one big region */
				while (span_num+1 < line->len && line->spans[span_num+1]->spacing < 1.5)
				{
					span_num++;
					span = line->spans[span_num];
					region_max = &span->max;
				}

				region_mask_add(rm, region_min, region_max);
			}
#ifdef DEBUG_MASKS
			dump_region_mask(rm);
#endif
			region_masks_add(rms, rm);
		}
	}

	/* Step 2: Sort the region_masks by size of masked region */
	region_masks_sort(rms);

#ifdef DEBUG_MASKS
	printf("Sorted list of regions:\n");
	dump_region_masks(rms);
#endif
	/* Step 3: Merge the region masks where possible (large ones first) */
	{
		int i;
		region_masks *rms2;
		rms2 = new_region_masks(ctx);
		for (i=0; i < rms->len; i++)
		{
			region_mask *rm = rms->mask[i];
			rms->mask[i] = NULL;
			region_masks_merge(rms2, rm);
		}
		free_region_masks(rms);
		rms = rms2;
	}

#ifdef DEBUG_MASKS
	printf("Merged list of regions:\n");
	dump_region_masks(rms);
#endif

	/* Step 4: Figure out alignment */
	region_masks_alignment(rms);

	/* Step 5: At this point, we should probably look at the region masks
	 * to try to guess which ones represent columns on the page. With our
	 * current code, we could only get blocks of lines that span 2 or more
	 * columns if the PDF producer wrote text out horizontally across 2
	 * or more columns, and we've never seen that (yet!). So we skip this
	 * step for now. */

	/* Step 6: Run through the lines again, deciding which ones fit into
	 * which region mask. */
	{
	region_mask *prev_match = NULL;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			fz_point blv;
			region_mask *rm;
			int span_num;
			region_mask *match;

			blv = line->spans[0]->max;
			blv.x -= line->spans[0]->min.x;
			blv.y -= line->spans[0]->min.y;
			normalise(&blv);

#ifdef DEBUG_MASKS
			dump_line(line);
#endif
			rm = new_region_mask(ctx, &blv);
			for (span_num = 0; span_num < line->len; span_num++)
			{
				fz_text_span *span = line->spans[span_num];
				fz_point *region_min = &span->min;
				fz_point *region_max = &span->max;

				/* Treat adjacent spans as one big region */
				while (span_num+1 < line->len && line->spans[span_num+1]->spacing < 1.5)
				{
					span_num++;
					span = line->spans[span_num];
					region_max = &span->max;
				}

				region_mask_add(rm, region_min, region_max);
			}
#ifdef DEBUG_MASKS
			printf("Mask: ");
			dump_region_mask(rm);
#endif
			match = region_masks_match(rms, rm, line, prev_match);
			prev_match = match;
#ifdef DEBUG_MASKS
			printf("Matches: ");
			dump_region_mask(match);
#endif
			free_region_mask(rm);
			for (span_num = 0; span_num < line->len; )
			{
				fz_text_span *span = line->spans[span_num];
				fz_point *region_min = &span->min;
				fz_point *region_max = &span->max;
				int sn;
				int col, align;
				float colw, left;

				/* Treat adjacent spans as one big region */
#ifdef DEBUG_ALIGN
				dump_span(line->spans[span_num]);
#endif
				for (sn = span_num+1; sn < line->len && line->spans[sn]->spacing < 1.5; sn++)
				{
					region_max = &line->spans[sn]->max;
#ifdef DEBUG_ALIGN
					dump_span(line->spans[sn]);
#endif
				}
				col = region_mask_column(match, region_min, region_max, &align, &colw, &left);
#ifdef DEBUG_ALIGN
				printf(" = col%d colw=%g align=%d\n", col, colw, align);
#endif
				do
				{
					line->spans[span_num]->column = col;
					line->spans[span_num]->align = align;
					line->spans[span_num]->indent = left;
					line->spans[span_num]->column_width = colw;
					span_num++;
				}
				while (span_num < sn);
			}
			line->region = match;
		}
	}
	free_region_masks(rms);
	}

	/* Step 7: Collate lines within a block that share the same region
	 * mask. */
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		int line_num;
		int prev_line_num;
		int last_from = -1;

		/* First merge lines. This may leave empty lines behind. */
		for (prev_line_num = 0, line_num = 1; line_num < block->len; line_num++)
		{
			fz_text_line *prev_line;
			line = &block->lines[line_num];
			if (line->len == 0)
				continue;
			prev_line = &block->lines[prev_line_num];
			if (prev_line->region == line->region)
			{
				int in1, in2, newlen, i, col;
				float indent;

				/* We only merge lines if the second line
				 * only uses 1 of the columns. */
				col = line->spans[0]->column;
				/* Copy the left value for the first span
				 * in the first column in this line forward
				 * for all the rest of the spans in the same
				 * column. */
				indent = line->spans[0]->indent;
				for (i = 1; i < line->len; i++)
				{
					if (col != line->spans[i]->column)
						break;
					line->spans[i]->indent = indent;
				}
				if (i != line->len)
				{
					prev_line_num = line_num;
					continue;
				}

				/* Merge line into prev_line */
				newlen = prev_line->len + line->len;
				if (newlen > prev_line->cap)
				{
					int newcap = prev_line->cap ? prev_line->cap : 2;
					do
					{
						newcap *= 2;
					}
					while (newcap < newlen);

					prev_line->spans = fz_resize_array(ctx, prev_line->spans, newcap, sizeof(*prev_line->spans));
					prev_line->cap = newcap;
				}

				in1 = prev_line->len-1;
				in2 = line->len-1;
				prev_line->len = newlen;
				for (; in1 >= 0 || in2 >= 0; )
				{
					newlen--;
					if (in1 < 0 || (in2 >= 0 && line->spans[in2]->column >= prev_line->spans[in1]->column))
					{
						prev_line->spans[newlen] = line->spans[in2];
						in2--;
						last_from = 1;
					}
					else
					{
						prev_line->spans[newlen] = prev_line->spans[in1];
						in1--;
						if (last_from == 1)
						{
							prev_line->spans[newlen+1]->spacing = 1;
							dehyphenate(prev_line->spans[newlen], prev_line->spans[newlen+1]);
							last_from = 0;
						}
					}
				}

				/* Leave line empty */
				line->len = 0;
			}
			else
				prev_line_num = line_num;
		}
		/* Now get rid of the empty lines */
		for (prev_line_num = 0, line_num = 0; line_num < block->len; line_num++)
		{
			line = &block->lines[line_num];
			if (line->len == 0)
				fz_free(ctx, line->spans);
			else
				block->lines[prev_line_num++] = *line;
		}
		block->len = prev_line_num;
		/* Now try to spot indents */
		for (line_num = 0; line_num < block->len; line_num++)
		{
			int span_num, sn, col;
			line = &block->lines[line_num];
			/* Run through the spans... */
			span_num = 0;
			{
				float indent = 0;
				/* For each set of spans that share the same
				 * column... */
				col = line->spans[span_num]->column;
#ifdef DEBUG_INDENTS
				printf("Indent %g: ", line->spans[span_num]->indent);
				dump_span(line->spans[span_num]);
				printf("\n");
#endif
				/* find the average indent of all but the first.. */
				for (sn = span_num+1; sn < line->len && line->spans[sn]->column == col; sn++)
				{
#ifdef DEBUG_INDENTS
					printf("Indent %g: ", line->spans[sn]->indent);
					dump_span(line->spans[sn]);
				printf("\n");
#endif
					indent += line->spans[sn]->indent;
					line->spans[sn]->indent = 0;
				}
				if (sn > span_num+1)
					indent /= sn-(span_num+1);
				/* And compare this indent with the first one... */
#ifdef DEBUG_INDENTS
				printf("Average indent %g ", indent);
#endif
				indent -= line->spans[span_num]->indent;
#ifdef DEBUG_INDENTS
				printf("delta %g ", indent);
#endif
				if (fabsf(indent) < 1)
				{
					/* No indent worth speaking of */
					indent = 0;
				}
#ifdef DEBUG_INDENTS
				printf("recorded %g\n", indent);
#endif
				line->spans[span_num]->indent = indent;
				span_num = sn;
			}
			for (; span_num < line->len; span_num++)
			{
				line->spans[span_num]->indent = 0;
			}
		}
	}
}
