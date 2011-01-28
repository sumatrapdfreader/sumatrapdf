#include "fitz.h"

fz_text *
fz_newtext(fz_font *font, fz_matrix trm, int wmode)
{
	fz_text *text;

	text = fz_malloc(sizeof(fz_text));
	text->font = fz_keepfont(font);
	text->trm = trm;
	text->wmode = wmode;
	text->len = 0;
	text->cap = 0;
	text->els = nil;

	return text;
}

void
fz_freetext(fz_text *text)
{
	fz_dropfont(text->font);
	fz_free(text->els);
	fz_free(text);
}

fz_text *
fz_clonetext(fz_text *old)
{
	fz_text *text;

	text = fz_malloc(sizeof(fz_text));
	text->font = fz_keepfont(old->font);
	text->trm = old->trm;
	text->wmode = old->wmode;
	text->len = old->len;
	text->cap = text->len;
	text->els = fz_calloc(text->len, sizeof(fz_textel));
	memcpy(text->els, old->els, text->len * sizeof(fz_textel));

	return text;
}

fz_rect
fz_boundtext(fz_text *text, fz_matrix ctm)
{
	fz_matrix trm;
	fz_rect bbox;
	fz_rect fbox;
	int i;

	if (text->len == 0)
		return fz_emptyrect;

	/* find bbox of glyph origins in ctm space */

	bbox.x0 = bbox.x1 = text->els[0].x;
	bbox.y0 = bbox.y1 = text->els[0].y;

	for (i = 1; i < text->len; i++)
	{
		bbox.x0 = MIN(bbox.x0, text->els[i].x);
		bbox.y0 = MIN(bbox.y0, text->els[i].y);
		bbox.x1 = MAX(bbox.x1, text->els[i].x);
		bbox.y1 = MAX(bbox.y1, text->els[i].y);
	}

	bbox = fz_transformrect(ctm, bbox);

	/* find bbox of font in trm * ctm space */

	trm = fz_concat(text->trm, ctm);
	trm.e = 0;
	trm.f = 0;

	fbox.x0 = text->font->bbox.x0 * 0.001f;
	fbox.y0 = text->font->bbox.y0 * 0.001f;
	fbox.x1 = text->font->bbox.x1 * 0.001f;
	fbox.y1 = text->font->bbox.y1 * 0.001f;

	fbox = fz_transformrect(trm, fbox);

	/* expand glyph origin bbox by font bbox */

	bbox.x0 += fbox.x0;
	bbox.y0 += fbox.y0;
	bbox.x1 += fbox.x1;
	bbox.y1 += fbox.y1;

	return bbox;
}

static void
fz_growtext(fz_text *text, int n)
{
	if (text->len + n < text->cap)
		return;
	while (text->len + n > text->cap)
		text->cap = text->cap + 36;
	text->els = fz_realloc(text->els, text->cap, sizeof(fz_textel));
}

void
fz_addtext(fz_text *text, int gid, int ucs, float x, float y)
{
	fz_growtext(text, 1);
	text->els[text->len].ucs = ucs;
	text->els[text->len].gid = gid;
	text->els[text->len].x = x;
	text->els[text->len].y = y;
	text->len++;
}

static int isxmlmeta(int c)
{
	return c < 32 || c >= 128 || c == '&' || c == '<' || c == '>' || c == '\'' || c == '"';
}

void fz_debugtext(fz_text *text, int indent)
{
	int i, n;
	for (i = 0; i < text->len; i++)
	{
		for (n = 0; n < indent; n++)
			putchar(' ');
		if (!isxmlmeta(text->els[i].ucs))
			printf("<g ucs=\"%c\" gid=\"%d\" x=\"%g\" y=\"%g\" />\n",
				text->els[i].ucs, text->els[i].gid, text->els[i].x, text->els[i].y);
		else
			printf("<g ucs=\"U+%04X\" gid=\"%d\" x=\"%g\" y=\"%g\" />\n",
				text->els[i].ucs, text->els[i].gid, text->els[i].x, text->els[i].y);
	}
}
