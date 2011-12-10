#include "fitz.h"

#define LINE_DIST 0.9f
#define SPACE_DIST 0.2f

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

typedef struct fz_text_device_s fz_text_device;

struct fz_text_device_s
{
	fz_point point;
	fz_text_span *head;
	fz_text_span *span;
};

fz_text_span *
fz_new_text_span(void)
{
	fz_text_span *span;
	span = fz_malloc(sizeof(fz_text_span));
	span->font = NULL;
	span->wmode = 0;
	span->size = 0;
	span->len = 0;
	span->cap = 0;
	span->text = NULL;
	span->next = NULL;
	span->eol = 0;
	return span;
}

void
fz_free_text_span(fz_text_span *span)
{
	fz_text_span *next;

	while (span)
	{
		if (span->font)
			fz_drop_font(span->font);
		next = span->next;
		fz_free(span->text);
		fz_free(span);
		span = next;
	}
}

static void
fz_add_text_char_imp(fz_text_span *span, int c, fz_bbox bbox)
{
	if (span->len + 1 >= span->cap)
	{
		span->cap = span->cap > 1 ? (span->cap * 3) / 2 : 80;
		span->text = fz_realloc(span->text, span->cap, sizeof(fz_text_char));
	}
	span->text[span->len].c = c;
	span->text[span->len].bbox = bbox;
	span->len ++;
}

static fz_bbox
fz_split_bbox(fz_bbox bbox, int i, int n)
{
	float w = (float)(bbox.x1 - bbox.x0) / n;
	float x0 = bbox.x0;
	bbox.x0 = x0 + i * w;
	bbox.x1 = x0 + (i + 1) * w;
	return bbox;
}

static void
fz_add_text_char(fz_text_span **last, fz_font *font, float size, int wmode, int c, fz_bbox bbox)
{
	fz_text_span *span = *last;

	if (!span->font)
	{
		span->font = fz_keep_font(font);
		span->size = size;
	}

	if ((span->font != font || span->size != size || span->wmode != wmode) && c != 32)
	{
		span = fz_new_text_span();
		span->font = fz_keep_font(font);
		span->size = size;
		span->wmode = wmode;
		(*last)->next = span;
		*last = span;
	}

	switch (c)
	{
	case -1: /* ignore when one unicode character maps to multiple glyphs */
		break;
	case 0xFB00: /* ff */
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 1, 2));
		break;
	case 0xFB01: /* fi */
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(span, 'i', fz_split_bbox(bbox, 1, 2));
		break;
	case 0xFB02: /* fl */
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(span, 'l', fz_split_bbox(bbox, 1, 2));
		break;
	case 0xFB03: /* ffi */
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 0, 3));
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 1, 3));
		fz_add_text_char_imp(span, 'i', fz_split_bbox(bbox, 2, 3));
		break;
	case 0xFB04: /* ffl */
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 0, 3));
		fz_add_text_char_imp(span, 'f', fz_split_bbox(bbox, 1, 3));
		fz_add_text_char_imp(span, 'l', fz_split_bbox(bbox, 2, 3));
		break;
	case 0xFB05: /* long st */
	case 0xFB06: /* st */
		fz_add_text_char_imp(span, 's', fz_split_bbox(bbox, 0, 2));
		fz_add_text_char_imp(span, 't', fz_split_bbox(bbox, 1, 2));
		break;
	default:
		fz_add_text_char_imp(span, c, bbox);
		break;
	}
}

static void
fz_divide_text_chars(fz_text_span **last, int n, fz_bbox bbox)
{
	fz_text_span *span = *last;
	int i, x;
	x = span->len - n;
	if (x >= 0)
		for (i = 0; i < n; i++)
			span->text[x + i].bbox = fz_split_bbox(bbox, i, n);
}

static void
fz_add_text_newline(fz_text_span **last, fz_font *font, float size, int wmode)
{
	fz_text_span *span;
	span = fz_new_text_span();
	span->font = fz_keep_font(font);
	span->size = size;
	span->wmode = wmode;
	(*last)->eol = 1;
	(*last)->next = span;
	*last = span;
}

void
fz_debug_text_span_xml(fz_text_span *span)
{
	char buf[10];
	int c, n, k, i;

	while (span)
	{
		printf("<span font=\"%s\" size=\"%g\" wmode=\"%d\" eol=\"%d\">\n",
			span->font ? span->font->name : "NULL", span->size, span->wmode, span->eol);

		for (i = 0; i < span->len; i++)
		{
			printf("\t<char ucs=\"");
			c = span->text[i].c;
			if (c < 128)
				putchar(c);
			else
			{
				n = runetochar(buf, &c);
				for (k = 0; k < n; k++)
					putchar(buf[k]);
			}
			printf("\" bbox=\"%d %d %d %d\" />\n",
				span->text[i].bbox.x0,
				span->text[i].bbox.y0,
				span->text[i].bbox.x1,
				span->text[i].bbox.y1);
		}

		printf("</span>\n");

		span = span->next;
	}
}

void
fz_debug_text_span(fz_text_span *span)
{
	char buf[10];
	int c, n, k, i;

	while (span)
	{
		for (i = 0; i < span->len; i++)
		{
			c = span->text[i].c;
			if (c < 128)
				putchar(c);
			else
			{
				n = runetochar(buf, &c);
				for (k = 0; k < n; k++)
					putchar(buf[k]);
			}
		}

		if (span->eol)
			putchar('\n');

		span = span->next;
	}
}

/***** SumatraPDF: various string fixups *****/
static void
ensurespanlength(fz_text_span *span, int mincap)
{
	if (span->cap < mincap)
	{
		span->cap = mincap * 3 / 2;
		span->text = fz_realloc(span->text, span->cap, sizeof(fz_text_char));
	}
}

static void
mergetwospans(fz_text_span *span)
{
	if (!span->next || span->font != span->next->font || span->size != span->next->size || span->wmode != span->next->wmode)
		return;

	ensurespanlength(span, span->len + span->next->len);
	memcpy(&span->text[span->len], &span->next->text[0], span->next->len * sizeof(fz_text_char));
	span->len += span->next->len;
	span->next->len = 0;

	if (span->next->next)
	{
		fz_text_span *newNext = span->next->next;
		span->eol = span->next->eol;
		span->next->next = NULL;
		fz_free_text_span(span->next);
		span->next = newNext;
	}
}

static void
deletecharacter(fz_text_span *span, int i)
{
	memmove(&span->text[i], &span->text[i + 1], (span->len - (i + 1)) * sizeof(fz_text_char));
	span->len--;
}

static void
reversecharacters(fz_text_span *span, int i, int j)
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
ornatecharacter(int ornate, int character)
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
	int i, j;

	for (i = 1; ornates[0][i] && ornates[0][i] != (wchar_t)ornate; i++);
	for (j = 1; ornates[j] && ornates[j][0] != (wchar_t)character; j++);
	return ornates[0][i] && ornates[j] ? ornates[j][i] : 0;
}

static float
calcbboxoverlap(fz_bbox bbox1, fz_bbox bbox2)
{
	fz_bbox intersect = fz_intersect_bbox(bbox1, bbox2);
	int area1, area2, area3;

	if (fz_is_empty_rect(intersect))
		return 0;

	area1 = (bbox1.x1 - bbox1.x0) * (bbox1.y1 - bbox1.y0);
	area2 = (bbox2.x1 - bbox2.x0) * (bbox2.y1 - bbox2.y0);
	area3 = (intersect.x1 - intersect.x0) * (intersect.y1 - intersect.y0);

	return 1.0 * area3 / MAX(area1, area2);
}

static int
doglyphsoverlap(fz_text_span *span, int i, fz_text_span *span2, int j)
{
	return
		i < span->len && j < span2->len && span->text[i].c == span2->text[j].c &&
		(calcbboxoverlap(span->text[i].bbox, span2->text[j].bbox) > 0.7f ||
		 // bboxes of slim glyphs sometimes don't overlap enough, so
		 // check if the overlapping continues with the following glyph
		 i + 1 < span->len && j + 1 < span2->len && span->text[i + 1].c == span2->text[j + 1].c &&
		 calcbboxoverlap(span->text[i + 1].bbox, span2->text[j + 1].bbox) > 0.7f);
}

/* TODO: Complete these lists... */
#define ISLEFTTORIGHTCHAR(c) ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c) ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

static void
fixuptextspan(fz_text_span *head)
{
	fz_text_span *span;
	int i;

	for (span = head; span; span = span->next)
	{
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
				if (span->next && span->next->len > 0 && (i + 1 == span->len || i + 2 == span->len && span->text[i + 1].c == 32))
				{
					mergetwospans(span);
				}
				if (i + 1 < span->len)
				{
					int newC = 0;
					if (span->text[i + 1].c != 32 || i + 2 >= span->len)
						newC = ornatecharacter(span->text[i].c, span->text[i + 1].c);
					else if ((newC = ornatecharacter(span->text[i].c, span->text[i + 2].c)))
						deletecharacter(span, i + 1);
					if (newC)
					{
						deletecharacter(span, i);
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
					reversecharacters(span, i, j - 1);
					i = j;
				}
			}
		}
	}

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=734 */
	/* remove duplicate character sequences in (almost) the same spot */
	for (span = head; span; span = span->next)
	{
		if (span->size < 5) /* doglyphsoverlap fails too often for small fonts */
			continue;
		for (i = 0; i < span->len; i++)
		{
			fz_text_span *span2;
			int newlines, j;
			for (span2 = span, j = i + 1, newlines = 0; span2 && newlines < 2; newlines += span2->eol, span2 = span2->next, j = 0)
				for (; j < span2->len; j++)
					if (span->text[i].c != 32 && doglyphsoverlap(span, i, span2, j))
						goto fixup_delete_duplicates;
			continue;

fixup_delete_duplicates:
			do
				deletecharacter(span, i);
			while (doglyphsoverlap(span, i, span2, ++j));

			if (i < span->len && span->text[i].c == 32)
				deletecharacter(span, i);
			else if (i == span->len && span->eol)
				span->eol = 0;
			i--;
		}
	}
}
/***** various string fixups *****/

static void
fz_text_extract_span(fz_text_span **last, fz_text *text, fz_matrix ctm, fz_point *pen)
{
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
	int i, err;

	if (text->len == 0)
		return;

	if (font->ft_face)
	{
		err = FT_Set_Char_Size(font->ft_face, 64, 64, 72, 72);
		if (err)
			fz_warn("freetype set character size: %s", ft_error_string(err));
		ascender = (float)face->ascender / face->units_per_EM;
		descender = (float)face->descender / face->units_per_EM;
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

	multi = 1;

	for (i = 0; i < text->len; i++)
	{
		if (text->items[i].gid < 0)
		{
			fz_add_text_char(last, font, size, text->wmode, text->items[i].ucs, fz_round_rect(rect));
			multi ++;
			fz_divide_text_chars(last, multi, fz_round_rect(rect));
			continue;
		}
		multi = 1;

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
			ndelta.x = delta.x / dist;
			ndelta.y = delta.y / dist;
			dot = ndelta.x * ndir.x + ndelta.y * ndir.y;

			if (dist > size * LINE_DIST)
			{
				fz_add_text_newline(last, font, size, text->wmode);
			}
			else if (fabsf(dot) > 0.95f && dist > size * SPACE_DIST)
			{
				if ((*last)->len > 0 && (*last)->text[(*last)->len - 1].c != ' ')
				{
					fz_rect spacerect;
					spacerect.x0 = -0.2f;
					spacerect.y0 = 0;
					spacerect.x1 = 0;
					spacerect.y1 = 1;
					spacerect = fz_transform_rect(trm, spacerect);
					fz_add_text_char(last, font, size, text->wmode, ' ', fz_round_rect(spacerect));
				}
			}
		}

		/* Calculate bounding box and new pen position based on font metrics */
		if (font->ft_face)
		{
			FT_Fixed ftadv = 0;
			int mask = FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING | FT_LOAD_IGNORE_TRANSFORM;

			/* TODO: freetype returns broken vertical metrics */
			/* if (text->wmode) mask |= FT_LOAD_VERTICAL_LAYOUT; */

			FT_Get_Advance(font->ft_face, text->items[i].gid, mask, &ftadv);
			adv = ftadv / 65536.0f;

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

		fz_add_text_char(last, font, size, text->wmode, text->items[i].ucs, fz_round_rect(rect));
	}
}

static void
fz_text_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_text_device *tdev = user;
	fz_text_extract_span(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_text_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_text_device *tdev = user;
	fz_text_extract_span(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_text_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_text_device *tdev = user;
	fz_text_extract_span(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_text_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_text_device *tdev = user;
	fz_text_extract_span(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_text_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
	fz_text_device *tdev = user;
	fz_text_extract_span(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_text_free_user(void *user)
{
	fz_text_device *tdev = user;

	tdev->span->eol = 1;

	/* TODO: unicode NFC normalization */
	/* TODO: bidi logical reordering */
	fixuptextspan(tdev->head);

	fz_free(tdev);
}

fz_device *
fz_new_text_device(fz_text_span *root)
{
	fz_device *dev;
	fz_text_device *tdev = fz_malloc(sizeof(fz_text_device));
	tdev->head = root;
	tdev->span = root;
	tdev->point.x = -1;
	tdev->point.y = -1;

	dev = fz_new_device(tdev);
	dev->hints = FZ_IGNORE_IMAGE | FZ_IGNORE_SHADE;
	dev->free_user = fz_text_free_user;
	dev->fill_text = fz_text_fill_text;
	dev->stroke_text = fz_text_stroke_text;
	dev->clip_text = fz_text_clip_text;
	dev->clip_stroke_text = fz_text_clip_stroke_text;
	dev->ignore_text = fz_text_ignore_text;
	return dev;
}
