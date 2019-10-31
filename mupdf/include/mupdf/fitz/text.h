#ifndef MUPDF_FITZ_TEXT_H
#define MUPDF_FITZ_TEXT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/path.h"
#include "mupdf/fitz/bidi.h"

/*
	Text buffer.

	The trm field contains the a, b, c and d coefficients.
	The e and f coefficients come from the individual elements,
	together they form the transform matrix for the glyph.

	Glyphs are referenced by glyph ID.
	The Unicode text equivalent is kept in a separate array
	with indexes into the glyph array.
*/

typedef struct fz_text_s fz_text;
typedef struct fz_text_span_s fz_text_span;
typedef struct fz_text_item_s fz_text_item;

struct fz_text_item_s
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings */
	int ucs; /* -1 for one ucs to many gid mappings */
};

#define FZ_LANG_TAG2(c1,c2) ((c1-'a'+1) + ((c2-'a'+1)*27))
#define FZ_LANG_TAG3(c1,c2,c3) ((c1-'a'+1) + ((c2-'a'+1)*27) + ((c3-'a'+1)*27*27))

typedef enum fz_text_language_e
{
	FZ_LANG_UNSET = 0,
	FZ_LANG_ur = FZ_LANG_TAG2('u','r'),
	FZ_LANG_urd = FZ_LANG_TAG3('u','r','d'),
	FZ_LANG_ko = FZ_LANG_TAG2('k','o'),
	FZ_LANG_ja = FZ_LANG_TAG2('j','a'),
	FZ_LANG_zh = FZ_LANG_TAG2('z','h'),
	FZ_LANG_zh_Hans = FZ_LANG_TAG3('z','h','s'),
	FZ_LANG_zh_Hant = FZ_LANG_TAG3('z','h','t'),
} fz_text_language;

struct fz_text_span_s
{
	fz_font *font;
	fz_matrix trm;
	unsigned wmode : 1;		/* 0 horizontal, 1 vertical */
	unsigned bidi_level : 7;	/* The bidirectional level of text */
	unsigned markup_dir : 2;	/* The direction of text as marked in the original document */
	unsigned language : 15;		/* The language as marked in the original document */
	int len, cap;
	fz_text_item *items;
	fz_text_span *next;
};

struct fz_text_s
{
	int refs;
	fz_text_span *head, *tail;
};

fz_text *fz_new_text(fz_context *ctx);

fz_text *fz_keep_text(fz_context *ctx, const fz_text *text);
void fz_drop_text(fz_context *ctx, const fz_text *text);

void fz_show_glyph(fz_context *ctx, fz_text *text, fz_font *font, fz_matrix trm, int glyph, int unicode, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language);

fz_matrix fz_show_string(fz_context *ctx, fz_text *text, fz_font *font, fz_matrix trm, const char *s, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language);

fz_rect fz_bound_text(fz_context *ctx, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm);

fz_text_language fz_text_language_from_string(const char *str);

char *fz_string_from_text_language(char str[8], fz_text_language lang);

#endif
