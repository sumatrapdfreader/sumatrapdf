#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <string.h>

/*
	Create a new empty fz_text object.

	Throws exception on failure to allocate.
*/
fz_text *
fz_new_text(fz_context *ctx)
{
	fz_text *text = fz_malloc_struct(ctx, fz_text);
	text->refs = 1;
	return text;
}

fz_text *
fz_keep_text(fz_context *ctx, const fz_text *textc)
{
	fz_text *text = (fz_text *)textc; /* Explicit cast away of const */

	return fz_keep_imp(ctx, text, &text->refs);
}

void
fz_drop_text(fz_context *ctx, const fz_text *textc)
{
	fz_text *text = (fz_text *)textc; /* Explicit cast away of const */

	if (fz_drop_imp(ctx, text, &text->refs))
	{
		fz_text_span *span = text->head;
		while (span)
		{
			fz_text_span *next = span->next;
			fz_drop_font(ctx, span->font);
			fz_free(ctx, span->items);
			fz_free(ctx, span);
			span = next;
		}
		fz_free(ctx, text);
	}
}

static fz_text_span *
fz_new_text_span(fz_context *ctx, fz_font *font, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language, fz_matrix trm)
{
	fz_text_span *span = fz_malloc_struct(ctx, fz_text_span);
	span->font = fz_keep_font(ctx, font);
	span->wmode = wmode;
	span->bidi_level = bidi_level;
	span->markup_dir = markup_dir;
	span->language = language;
	span->trm = trm;
	span->trm.e = 0;
	span->trm.f = 0;
	return span;
}

static fz_text_span *
fz_add_text_span(fz_context *ctx, fz_text *text, fz_font *font, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language, fz_matrix trm)
{
	if (!text->tail)
	{
		text->head = text->tail = fz_new_text_span(ctx, font, wmode, bidi_level, markup_dir, language, trm);
	}
	else if (text->tail->font != font ||
		text->tail->wmode != wmode ||
		text->tail->bidi_level != bidi_level ||
		text->tail->markup_dir != markup_dir ||
		text->tail->language != language ||
		text->tail->trm.a != trm.a ||
		text->tail->trm.b != trm.b ||
		text->tail->trm.c != trm.c ||
		text->tail->trm.d != trm.d)
	{
		text->tail = text->tail->next = fz_new_text_span(ctx, font, wmode, bidi_level, markup_dir, language, trm);
	}
	return text->tail;
}

static void
fz_grow_text_span(fz_context *ctx, fz_text_span *span, int n)
{
	int new_cap = span->cap;
	if (span->len + n < new_cap)
		return;
	while (span->len + n > new_cap)
		new_cap = new_cap + 36;
	span->items = fz_realloc_array(ctx, span->items, new_cap, fz_text_item);
	span->cap = new_cap;
}

/*
	Add a glyph/unicode value to a text object.

	text: Text object to add to.

	font: The font the glyph should be added in.

	trm: The transform to use for the glyph.

	glyph: The glyph id to add.

	unicode: The unicode character for the glyph.

	wmode: 1 for vertical mode, 0 for horizontal.

	bidi_level: The bidirectional level for this glyph.

	markup_dir: The direction of the text as specified in the
	markup.

	language: The language in use (if known, 0 otherwise)
	(e.g. FZ_LANG_zh_Hans).

	Throws exception on failure to allocate.
*/
void
fz_show_glyph(fz_context *ctx, fz_text *text, fz_font *font, fz_matrix trm, int gid, int ucs, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language lang)
{
	fz_text_span *span;

	if (text->refs != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot modify shared text objects");

	span = fz_add_text_span(ctx, text, font, wmode, bidi_level, markup_dir, lang, trm);

	fz_grow_text_span(ctx, span, 1);

	span->items[span->len].ucs = ucs;
	span->items[span->len].gid = gid;
	span->items[span->len].x = trm.e;
	span->items[span->len].y = trm.f;
	span->len++;
}

/*
	Add a UTF8 string to a text object.

	text: Text object to add to.

	font: The font the string should be added in.

	trm: The transform to use.

	s: The utf-8 string to add.

	wmode: 1 for vertical mode, 0 for horizontal.

	bidi_level: The bidirectional level for this glyph.

	markup_dir: The direction of the text as specified in the
	markup.

	language: The language in use (if known, 0 otherwise)
	(e.g. FZ_LANG_zh_Hans).

	Returns the transform updated with the advance width of the string.
*/
fz_matrix
fz_show_string(fz_context *ctx, fz_text *text, fz_font *user_font, fz_matrix trm, const char *s, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language)
{
	fz_font *font;
	int gid, ucs;
	float adv;

	while (*s)
	{
		s += fz_chartorune(&ucs, s);
		gid = fz_encode_character_with_fallback(ctx, user_font, ucs, 0, language, &font);
		fz_show_glyph(ctx, text, font, trm, gid, ucs, wmode, bidi_level, markup_dir, language);
		adv = fz_advance_glyph(ctx, font, gid, wmode);
		if (wmode == 0)
			trm = fz_pre_translate(trm, adv, 0);
		else
			trm = fz_pre_translate(trm, 0, -adv);
	}

	return trm;
}

/*
	Find the bounds of a given text object.

	text: The text object to find the bounds of.

	stroke: Pointer to the stroke attributes (for stroked
	text), or NULL (for filled text).

	ctm: The matrix in use.

	r: pointer to storage for the bounds.

	Returns a pointer to r, which is updated to contain the
	bounding box for the text object.
*/
fz_rect
fz_bound_text(fz_context *ctx, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_text_span *span;
	fz_matrix tm, trm;
	fz_rect gbox;
	fz_rect bbox;
	int i;

	bbox = fz_empty_rect;

	for (span = text->head; span; span = span->next)
	{
		if (span->len > 0)
		{
			tm = span->trm;
			for (i = 0; i < span->len; i++)
			{
				if (span->items[i].gid >= 0)
				{
					tm.e = span->items[i].x;
					tm.f = span->items[i].y;
					trm = fz_concat(tm, ctm);
					gbox = fz_bound_glyph(ctx, span->font, span->items[i].gid, trm);
					bbox = fz_union_rect(bbox, gbox);
				}
			}
		}
	}

	if (!fz_is_empty_rect(bbox))
	{
		if (stroke)
			bbox = fz_adjust_rect_for_stroke(ctx, bbox, stroke, ctm);

		/* Compensate for the glyph cache limited positioning precision */
		bbox.x0 -= 1;
		bbox.y0 -= 1;
		bbox.x1 += 1;
		bbox.y1 += 1;
	}

	return bbox;
}

/*
	Convert ISO 639 (639-{1,2,3,5}) language specification
	strings losslessly to a 15 bit fz_text_language code.

	No validation is carried out. Obviously invalid (out
	of spec) codes will be mapped to FZ_LANG_UNSET, but
	well-formed (but undefined) codes will be blithely
	accepted.
*/
fz_text_language fz_text_language_from_string(const char *str)
{
	fz_text_language lang;

	if (str == NULL)
		return FZ_LANG_UNSET;

	if (!strcmp(str, "zh-Hant") ||
			!strcmp(str, "zh-HK") ||
			!strcmp(str, "zh-MO") ||
			!strcmp(str, "zh-SG") ||
			!strcmp(str, "zh-TW"))
		return FZ_LANG_zh_Hant;
	if (!strcmp(str, "zh-Hans") ||
			!strcmp(str, "zh-CN"))
		return FZ_LANG_zh_Hans;

	/* 1st char */
	if (str[0] >= 'a' && str[0] <= 'z')
		lang = str[0] - 'a' + 1;
	else if (str[0] >= 'A' && str[0] <= 'Z')
		lang = str[0] - 'A' + 1;
	else
		return 0;

	/* 2nd char */
	if (str[1] >= 'a' && str[1] <= 'z')
		lang += 27*(str[1] - 'a' + 1);
	else if (str[1] >= 'A' && str[1] <= 'Z')
		lang += 27*(str[1] - 'A' + 1);
	else
		return 0; /* There are no valid 1 char language codes */

	/* 3nd char */
	if (str[2] >= 'a' && str[2] <= 'z')
		lang += 27*27*(str[2] - 'a' + 1);
	else if (str[2] >= 'A' && str[2] <= 'Z')
		lang += 27*27*(str[2] - 'A' + 1);

	/* We don't support iso 639-6 4 char codes, cos the standard
	 * has been withdrawn, and no one uses them. */
	return lang;
}

/*
	Recover ISO 639 (639-{1,2,3,5}) language specification
	strings losslessly from a 15 bit fz_text_language code.

	No validation is carried out. See note above.
*/
char *fz_string_from_text_language(char str[8], fz_text_language lang)
{
	int c;

	/* str is supposed to be at least 8 chars in size */
	if (str == NULL)
		return NULL;

	if (lang == FZ_LANG_zh_Hant)
		fz_strlcpy(str, "zh-Hant", 8);
	else if (lang == FZ_LANG_zh_Hans)
		fz_strlcpy(str, "zh-Hans", 8);
	else
	{
		c = lang % 27;
		lang = lang / 27;
		str[0] = c == 0 ? 0 : c - 1 + 'a';
		c = lang % 27;
		lang = lang / 27;
		str[1] = c == 0 ? 0 : c - 1 + 'a';
		c = lang % 27;
		str[2] = c == 0 ? 0 : c - 1 + 'a';
		str[3] = 0;
	}

	return str;
}
