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

typedef struct
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings */
	int ucs; /* -1 for one ucs to many gid mappings */
} fz_text_item;

#define FZ_LANG_TAG2(c1,c2) ((c1-'a'+1) + ((c2-'a'+1)*27))
#define FZ_LANG_TAG3(c1,c2,c3) ((c1-'a'+1) + ((c2-'a'+1)*27) + ((c3-'a'+1)*27*27))

typedef enum
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

typedef struct fz_text_span
{
	fz_font *font;
	fz_matrix trm;
	unsigned wmode : 1;		/* 0 horizontal, 1 vertical */
	unsigned bidi_level : 7;	/* The bidirectional level of text */
	unsigned markup_dir : 2;	/* The direction of text as marked in the original document */
	unsigned language : 15;		/* The language as marked in the original document */
	int len, cap;
	fz_text_item *items;
	struct fz_text_span *next;
} fz_text_span;

typedef struct
{
	int refs;
	fz_text_span *head, *tail;
} fz_text;

/*
	Create a new empty fz_text object.

	Throws exception on failure to allocate.
*/
fz_text *fz_new_text(fz_context *ctx);

/*
	Increment the reference count for the text object. The same
	pointer is returned.

	Never throws exceptions.
*/
fz_text *fz_keep_text(fz_context *ctx, const fz_text *text);

/*
	Decrement the reference count for the text object. When the
	reference count hits zero, the text object is freed.

	Never throws exceptions.
*/
void fz_drop_text(fz_context *ctx, const fz_text *text);

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
void fz_show_glyph(fz_context *ctx, fz_text *text, fz_font *font, fz_matrix trm, int glyph, int unicode, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language);

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

	Returns the transform updated with the advance width of the
	string.
*/
fz_matrix fz_show_string(fz_context *ctx, fz_text *text, fz_font *font, fz_matrix trm, const char *s, int wmode, int bidi_level, fz_bidi_direction markup_dir, fz_text_language language);

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
fz_rect fz_bound_text(fz_context *ctx, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm);

/*
	Convert ISO 639 (639-{1,2,3,5}) language specification
	strings losslessly to a 15 bit fz_text_language code.

	No validation is carried out. Obviously invalid (out
	of spec) codes will be mapped to FZ_LANG_UNSET, but
	well-formed (but undefined) codes will be blithely
	accepted.
*/
fz_text_language fz_text_language_from_string(const char *str);

/*
	Recover ISO 639 (639-{1,2,3,5}) language specification
	strings losslessly from a 15 bit fz_text_language code.

	No validation is carried out. See note above.
*/
char *fz_string_from_text_language(char str[8], fz_text_language lang);

#endif
