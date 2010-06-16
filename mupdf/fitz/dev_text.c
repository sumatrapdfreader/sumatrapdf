#include "fitz.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#if ((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 1)) || \
	((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 2)) || \
	((FREETYPE_MAJOR == 2) && (FREETYPE_MINOR == 3) && (FREETYPE_PATCH < 8))

int
FT_Get_Advance(FT_Face face, int gid, int masks, FT_Fixed *out)
{
	int err;
	err = FT_Load_Glyph(face, gid, masks | FT_LOAD_IGNORE_TRANSFORM);
	if (err)
		return err;
	if (masks & FT_LOAD_VERTICAL_LAYOUT)
		*out = face->glyph->advance.y * 1024;
	else
		*out = face->glyph->advance.x * 1024;
	return 0;
}

#else
#include FT_ADVANCES_H
#endif

typedef struct fz_textdevice_s fz_textdevice;

struct fz_textdevice_s
{
	fz_point point;
	fz_textspan *head;
	fz_textspan *span;
};

fz_textspan *
fz_newtextspan(void)
{
	fz_textspan *span;
	span = fz_malloc(sizeof(fz_textspan));
	span->font = nil;
	span->wmode = 0;
	span->size = 0.0;
	span->len = 0;
	span->cap = 0;
	span->text = nil;
	span->next = nil;
	span->eol = 0;
	return span;
}

void
fz_freetextspan(fz_textspan *span)
{
	if (span->font)
		fz_dropfont(span->font);
	if (span->next)
		fz_freetextspan(span->next);
	fz_free(span->text);
	fz_free(span);
}

static void
fz_addtextcharimp(fz_textspan *span, int c, fz_bbox bbox)
{
	if (span->len + 1 >= span->cap)
	{
		span->cap = span->cap ? (span->cap * 3) / 2 : 80;
		span->text = fz_realloc(span->text, sizeof(fz_textchar) * span->cap);
	}
	span->text[span->len].c = c;
	span->text[span->len].bbox = bbox;
	span->len ++;
}

static fz_bbox
fz_splitbbox(fz_bbox bbox, int i, int n)
{
	int w = bbox.x1 - bbox.x0;
	bbox.x0 = bbox.x0 + w * i / n;
	bbox.x1 = bbox.x0 + w * (i + 1) / n;
	return bbox;
}

static void
fz_addtextchar(fz_textspan **last, fz_font *font, float size, int wmode, int c, fz_bbox bbox)
{
	fz_textspan *span = *last;

	if (!span->font)
	{
		span->font = fz_keepfont(font);
		span->size = size;
	}

	if (span->font != font || span->size != size || span->wmode != wmode)
	{
		span = fz_newtextspan();
		span->font = fz_keepfont(font);
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
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 0, 2));
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 1, 2));
		break;
	case 0xFB01: /* fi */
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 0, 2));
		fz_addtextcharimp(span, 'i', fz_splitbbox(bbox, 1, 2));
		break;
	case 0xFB02: /* fl */
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 0, 2));
		fz_addtextcharimp(span, 'l', fz_splitbbox(bbox, 1, 2));
		break;
	case 0xFB03: /* ffi */
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 0, 3));
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 1, 3));
		fz_addtextcharimp(span, 'i', fz_splitbbox(bbox, 2, 3));
		break;
	case 0xFB04: /* ffl */
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 0, 3));
		fz_addtextcharimp(span, 'f', fz_splitbbox(bbox, 1, 3));
		fz_addtextcharimp(span, 'l', fz_splitbbox(bbox, 2, 3));
		break;
	case 0xFB05: /* long st */
	case 0xFB06: /* st */
		fz_addtextcharimp(span, 's', fz_splitbbox(bbox, 0, 2));
		fz_addtextcharimp(span, 't', fz_splitbbox(bbox, 1, 2));
		break;
	default:
		fz_addtextcharimp(span, c, bbox);
		break;
	}
}

static void
fz_addtextnewline(fz_textspan **last, fz_font *font, float size, int wmode)
{
	fz_textspan *span;
	span = fz_newtextspan();
	span->font = fz_keepfont(font);
	span->size = size;
	span->wmode = wmode;
	(*last)->eol = 1;
	(*last)->next = span;
	*last = span;
}

void
fz_debugtextspanxml(fz_textspan *span)
{
	char buf[10];
	int c, n, k, i;

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
		printf("\" bbox=\"[%d %d %d %d]\">\n",
			span->text[i].bbox.x0,
			span->text[i].bbox.y0,
			span->text[i].bbox.x1,
			span->text[i].bbox.y1);
	}

	printf("</span>\n");

	if (span->next)
		fz_debugtextspanxml(span->next);
}

void
fz_debugtextspan(fz_textspan *span)
{
	char buf[10];
	int c, n, k, i;

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

	if (span->next)
		fz_debugtextspan(span->next);
}

/***** various string fixups *****/
static void
ensurespanlength(fz_textspan *span, int mincap)
{
	if (span->cap < mincap)
	{
		span->cap = mincap * 3 / 2;
		span->text = fz_realloc(span->text, span->cap * sizeof(fz_textchar));
	}
}

static void
mergetwospans(fz_textspan *span)
{
	ensurespanlength(span, span->len + span->next->len);
	memcpy(&span->text[span->len], &span->next->text[0], span->next->len * sizeof(fz_textchar));
	span->len += span->next->len;
	span->next->len = 0;

	if (span->next->next)
	{
		fz_textspan *newNext = span->next->next;
		span->next->next = nil;
		fz_freetextspan(span->next);
		span->next = newNext;
	}
}

static void
deletecharacter(fz_textspan *span, int i)
{
	memmove(&span->text[i], &span->text[i + 1], (span->len - (i + 1)) * sizeof(fz_textchar));
	span->len--;
}

static void
reversecharacters(fz_textspan *span, int i, int j)
{
	while (i < j)
	{
		fz_textchar tc = span->text[i];
		span->text[i] = span->text[j];
		span->text[j] = tc;
		i++; j--;
	}
}

static int
ornatecharacter(int ornate, int character)
{
	static wchar_t *ornates[] = {
		L" \xA8\xB4`^",
		L"a\xE4\xE1\xE0\xE2", L"A\xC4\xC1\xC0\xC2",
		L"e\xEB\xE9\xE8\xEA", L"E\xCB\xC9\xC8\xCA",
		L"i\xEF\xED\xEC\xEE", L"I\xCF\xCD\xCC\xCE",
		L"o\xF6\xF3\xF2\xF4", L"O\xD6\xD3\xD2\xD4",
		L"u\xFC\xFA\xF9\xFB", L"U\xDC\xDA\xD9\xDB",
		nil
	};
	int i, j;

	for (i = 1; ornates[0][i] && ornates[0][i] != (wchar_t)ornate; i++);
	for (j = 1; ornates[j] && ornates[j][0] != (wchar_t)character; j++);
	return ornates[0][i] && ornates[j] ? ornates[j][i] : 0;
}

/* TODO: Complete these lists... */
#define ISLEFTTORIGHTCHAR(c) ((0x0041 <= (c) && (c) <= 0x005A) || (0x0061 <= (c) && (c) <= 0x007A) || (0xFB00 <= (c) && (c) <= 0xFB06))
#define ISRIGHTTOLEFTCHAR(c) ((0x0590 <= (c) && (c) <= 0x05FF) || (0x0600 <= (c) && (c) <= 0x06FF) || (0x0750 <= (c) && (c) <= 0x077F) || (0xFB50 <= (c) && (c) <= 0xFDFF) || (0xFE70 <= (c) && (c) <= 0xFEFF))

static void
fixuptextspan(fz_textspan *span)
{
	for (; span; span = span->next)
	{
		int i;
		for (i = 0; i < span->len; i++)
		{
			switch (span->text[i].c)
			{
			/* recombine characters and their accents */
			case 0x00A8: /* ¨ */
			case 0x00B4: /* ´ */
			case 0x0060: /* ` */
			case 0x005E: /* ^ */
				if (i + 1 == span->len && span->next && span->next->len > 0)
				{
					mergetwospans(span);
				}
				if (i + 1 < span->len)
				{
					int newC = ornatecharacter(span->text[i].c, span->text[i + 1].c);
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
}
/***** various string fixups *****/

static void
fz_textextractspan(fz_textspan **last, fz_text *text, fz_matrix ctm, fz_point *pen)
{
	fz_font *font = text->font;
	fz_matrix tm = text->trm;
	fz_matrix trm;
	float size;
	float dx, dy;
	float adv;
	fz_rect rect;
	fz_point dir;
	int i, err;
	float cross, dist2;
	fz_textspan *firstSpan = *last;

 	if (text->len == 0)
		return;

	if (font->ftface)
	{
		FT_Set_Transform(font->ftface, NULL, NULL);
		err = FT_Set_Char_Size(font->ftface, 64, 64, 72, 72);
		if (err)
			fz_warn("freetype set character size: %s", ft_errorstring(err));
	}

	rect = fz_emptyrect;

	if (text->wmode == 0)
	{
		dir.x = 1.0;
		dir.y = 0.0;
	}
	else
	{
		dir.x = 0.0;
		dir.y = 1.0;
	}

	tm.e = 0;
	tm.f = 0;
	trm = fz_concat(tm, ctm);
 	dir = fz_transformvector(trm, dir);
	size = fz_matrixexpansion(trm);

	for (i = 0; i < text->len; i++)
	{
		if (text->els[i].gid < 0)
		{
			/* TODO: split rect for one-to-many mapped chars */
			fz_addtextchar(last, font, size, text->wmode, text->els[i].ucs, fz_roundrect(rect));
			continue;
		}

		/* Calculate new pen location and delta */
		tm.e = text->els[i].x;
		tm.f = text->els[i].y;
		trm = fz_concat(tm, ctm);

		dx = pen->x - trm.e;
		dy = pen->y - trm.f;
		if (pen->x == -1 && pen->y == -1)
			dx = dy = 0;
		cross = dx * dir.y - dy * dir.x;
		dist2 = dx * dx + dy * dy;

		/* Add space and newlines based on pen movement */
		if (fabs(dist2) > size * size * 0.04)
		{
			if (fabs(cross) > 0.1)
			{
				fz_addtextnewline(last, font, size, text->wmode);
			}
			else if ((*last)->len == 0 || (*last)->text[(*last)->len - 1].c == ' ') ; /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=687 */
			else if (fabs(cross) < 0.1 && dist2 > size * size * 0.04)
			{
				fz_rect spacerect;
				spacerect.x0 = -fabs(dx);
				spacerect.y0 = 0.0;
				spacerect.x1 = 0.0;
				spacerect.y1 = 1.0;
				spacerect = fz_transformrect(trm, spacerect);
				fz_addtextchar(last, font, size, text->wmode, ' ', fz_roundrect(spacerect));
			}
		}

		/* Calculate bounding box and new pen position based on font metrics */
		if (font->ftface)
		{
			FT_Fixed ftadv = 0;
			int mask = FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING;
			if (text->wmode)
				mask |= FT_LOAD_VERTICAL_LAYOUT;
			err = FT_Get_Advance(font->ftface, text->els[i].gid, mask, &ftadv);
			if (err)
				fz_warn("freetype get advance (gid %d): %s", text->els[i].gid, ft_errorstring(err));
			adv = ftadv / 65536.0;
			if (text->wmode)
			{
				adv = -1.0; /* TODO: freetype returns broken vertical metrics */
				rect.x0 = 0.0; rect.y0 = 0.0;
				rect.x1 = 1.0; rect.y1 = adv;
			}
			else
			{
				rect.x0 = 0.0; rect.y0 = 0.0;
				rect.x1 = adv; rect.y1 = 1.0;
			}
		}
		else
		{
			adv = font->t3widths[text->els[i].gid];
			rect.x0 = 0.0; rect.y0 = 0.0;
			rect.x1 = adv; rect.y1 = 1.0;
		}

		rect = fz_transformrect(trm, rect);
		pen->x = trm.e + dir.x * adv;
		pen->y = trm.f + dir.y * adv;

		fz_addtextchar(last, font, size, text->wmode, text->els[i].ucs, fz_roundrect(rect));
	}

	fixuptextspan(firstSpan);
}

static void
fz_textfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_textdevice *tdev = user;
	fz_textextractspan(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_textstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_textdevice *tdev = user;
	fz_textextractspan(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_textcliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_textdevice *tdev = user;
	fz_textextractspan(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_textclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_textdevice *tdev = user;
	fz_textextractspan(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_textignoretext(void *user, fz_text *text, fz_matrix ctm)
{
	fz_textdevice *tdev = user;
	fz_textextractspan(&tdev->span, text, ctm, &tdev->point);
}

static void
fz_textfreeuser(void *user)
{
	fz_textdevice *tdev = user;

	tdev->span->eol = 1;

	/* TODO: unicode NFC normalization */
	/* TODO: bidi logical reordering */

	fz_free(tdev);
}

fz_device *
fz_newtextdevice(fz_textspan *root)
{
	fz_device *dev;
	fz_textdevice *tdev = fz_malloc(sizeof(fz_textdevice));
	tdev->head = root;
	tdev->span = root;
	tdev->point.x = -1;
	tdev->point.y = -1;

	dev = fz_newdevice(tdev);
	dev->freeuser = fz_textfreeuser;
	dev->filltext = fz_textfilltext;
	dev->stroketext = fz_textstroketext;
	dev->cliptext = fz_textcliptext;
	dev->clipstroketext = fz_textclipstroketext;
	dev->ignoretext = fz_textignoretext;
	return dev;
}
