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

#ifndef MUPDF_FITZ_FONT_H
#define MUPDF_FITZ_FONT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/color.h"

/* forward declaration for circular dependency */
struct fz_device;

/* Various font encoding tables and lookup functions */

FZ_DATA extern const char *fz_glyph_name_from_adobe_standard[256];
FZ_DATA extern const char *fz_glyph_name_from_iso8859_7[256];
FZ_DATA extern const char *fz_glyph_name_from_koi8u[256];
FZ_DATA extern const char *fz_glyph_name_from_mac_expert[256];
FZ_DATA extern const char *fz_glyph_name_from_mac_roman[256];
FZ_DATA extern const char *fz_glyph_name_from_win_ansi[256];
FZ_DATA extern const char *fz_glyph_name_from_windows_1252[256];

FZ_DATA extern const unsigned short fz_unicode_from_iso8859_1[256];
FZ_DATA extern const unsigned short fz_unicode_from_iso8859_7[256];
FZ_DATA extern const unsigned short fz_unicode_from_koi8u[256];
FZ_DATA extern const unsigned short fz_unicode_from_pdf_doc_encoding[256];
FZ_DATA extern const unsigned short fz_unicode_from_windows_1250[256];
FZ_DATA extern const unsigned short fz_unicode_from_windows_1251[256];
FZ_DATA extern const unsigned short fz_unicode_from_windows_1252[256];

int fz_iso8859_1_from_unicode(int u);
int fz_iso8859_7_from_unicode(int u);
int fz_koi8u_from_unicode(int u);
int fz_windows_1250_from_unicode(int u);
int fz_windows_1251_from_unicode(int u);
int fz_windows_1252_from_unicode(int u);

int fz_unicode_from_glyph_name(const char *name);
int fz_unicode_from_glyph_name_strict(const char *name);
const char **fz_duplicate_glyph_names_from_unicode(int unicode);
const char *fz_glyph_name_from_unicode_sc(int unicode);

/**
	An abstract font handle.
*/
typedef struct fz_font fz_font;

/**
	Fonts come in two variants:
	Regular fonts are handled by FreeType.
	Type 3 fonts have callbacks to the interpreter.
*/

/**
	Retrieve the FT_Face handle
	for the font.

	font: The font to query

	Returns the FT_Face handle for the font, or NULL
	if not a freetype handled font. (Cast to void *
	to avoid nasty header exposure).
*/
void *fz_font_ft_face(fz_context *ctx, fz_font *font);

/**
	Retrieve the Type3 procs
	for a font.

	font: The font to query

	Returns the t3_procs pointer. Will be NULL for a
	non type-3 font.
*/
fz_buffer **fz_font_t3_procs(fz_context *ctx, fz_font *font);

/* common CJK font collections */
enum { FZ_ADOBE_CNS, FZ_ADOBE_GB, FZ_ADOBE_JAPAN, FZ_ADOBE_KOREA };

/**
	Every fz_font carries a set of flags
	within it, in a fz_font_flags_t structure.
*/
typedef struct
{
	unsigned int is_mono : 1;
	unsigned int is_serif : 1;
	unsigned int is_bold : 1;
	unsigned int is_italic : 1;
	unsigned int ft_substitute : 1; /* use substitute metrics */
	unsigned int ft_stretch : 1; /* stretch to match PDF metrics */

	unsigned int fake_bold : 1; /* synthesize bold */
	unsigned int fake_italic : 1; /* synthesize italic */
	unsigned int has_opentype : 1; /* has opentype shaping tables */
	unsigned int invalid_bbox : 1;

	unsigned int cjk : 1;
	unsigned int cjk_lang : 2; /* CNS, GB, JAPAN, or KOREA */

	unsigned int embed : 1;
	unsigned int never_embed : 1;
} fz_font_flags_t;

/**
	Retrieve a pointer to the font flags
	for a given font. These can then be updated as required.

	font: The font to query

	Returns a pointer to the flags structure (or NULL, if
	the font is NULL).
*/
fz_font_flags_t *fz_font_flags(fz_font *font);

/**
	In order to shape a given font, we need to
	declare it to a shaper library (harfbuzz, by default, but others
	are possible). To avoid redeclaring it every time we need to
	shape, we hold a shaper handle and the destructor for it within
	the font itself. The handle is initialised by the caller when
	first required and the destructor is called when the fz_font is
	destroyed.
*/
typedef struct
{
	void *shaper_handle;
	void (*destroy)(fz_context *ctx, void *); /* Destructor for shape_handle */
} fz_shaper_data_t;

/**
	Retrieve a pointer to the shaper data
	structure for the given font.

	font: The font to query.

	Returns a pointer to the shaper data structure (or NULL if
	font is NULL).
*/
fz_shaper_data_t *fz_font_shaper_data(fz_context *ctx, fz_font *font);

/**
	Retrieve a pointer to the name of the font.

	font: The font to query.

	Returns a pointer to an internal copy of the font name.
	Will never be NULL, but may be the empty string.
*/
const char *fz_font_name(fz_context *ctx, fz_font *font);

/**
	Query whether the font flags say that this font is bold.
*/
int fz_font_is_bold(fz_context *ctx, fz_font *font);

/**
	Query whether the font flags say that this font is italic.
*/
int fz_font_is_italic(fz_context *ctx, fz_font *font);

/**
	Query whether the font flags say that this font is serif.
*/
int fz_font_is_serif(fz_context *ctx, fz_font *font);

/**
	Query whether the font flags say that this font is monospaced.
*/
int fz_font_is_monospaced(fz_context *ctx, fz_font *font);

/**
	Retrieve the font bbox.

	font: The font to query.

	Returns the font bbox by value; it is valid only if
	fz_font_flags(font)->invalid_bbox is zero.
*/
fz_rect fz_font_bbox(fz_context *ctx, fz_font *font);

/**
	Type for user supplied system font loading hook.

	name: The name of the font to load.

	bold: 1 if a bold font desired, 0 otherwise.

	italic: 1 if an italic font desired, 0 otherwise.
	needs_exact_metrics: 1 if an exact metric match is required for
	the font requested.

	Returns a new font handle, or NULL if no font found (or on error).
*/
typedef fz_font *(fz_load_system_font_fn)(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics);

/**
	Type for user supplied cjk font loading hook.

	name: The name of the font to load.

	ordering: The ordering for which to load the font (e.g.
	FZ_ADOBE_KOREA)

	serif: 1 if a serif font is desired, 0 otherwise.

	Returns a new font handle, or NULL if no font found (or on error).
*/
typedef fz_font *(fz_load_system_cjk_font_fn)(fz_context *ctx, const char *name, int ordering, int serif);

/**
	Type for user supplied fallback font loading hook.

	name: The name of the font to load.

	script: UCDN script enum.

	language: FZ_LANG enum.

	serif, bold, italic: boolean style flags.

	Returns a new font handle, or NULL if no font found (or on error).
*/
typedef fz_font *(fz_load_system_fallback_font_fn)(fz_context *ctx, int script, int language, int serif, int bold, int italic);

/**
	Install functions to allow MuPDF to request fonts from the
	system.

	Only one set of hooks can be in use at a time.
*/
void fz_install_load_system_font_funcs(fz_context *ctx,
	fz_load_system_font_fn *f,
	fz_load_system_cjk_font_fn *f_cjk,
	fz_load_system_fallback_font_fn *f_fallback);

/**
	Attempt to load a given font from the system.

	name: The name of the desired font.

	bold: 1 if bold desired, 0 otherwise.

	italic: 1 if italic desired, 0 otherwise.

	needs_exact_metrics: 1 if an exact metrical match is required,
	0 otherwise.

	Returns a new font handle, or NULL if no matching font was found
	(or on error).
*/
fz_font *fz_load_system_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics);

/**
	Attempt to load a given font from
	the system.

	name: The name of the desired font.

	ordering: The ordering to load the font from (e.g. FZ_ADOBE_KOREA)

	serif: 1 if serif desired, 0 otherwise.

	Returns a new font handle, or NULL if no matching font was found
	(or on error).
*/
fz_font *fz_load_system_cjk_font(fz_context *ctx, const char *name, int ordering, int serif);

/**
	Search the builtin fonts for a match.
	Whether a given font is present or not will depend on the
	configuration in which MuPDF is built.

	name: The name of the font desired.

	bold: 1 if bold desired, 0 otherwise.

	italic: 1 if italic desired, 0 otherwise.

	len: Pointer to a place to receive the length of the discovered
	font buffer.

	Returns a pointer to the font file data, or NULL if not present.
*/
const unsigned char *fz_lookup_builtin_font(fz_context *ctx, const char *name, int bold, int italic, int *len);

/**
	Search the builtin base14 fonts for a match.
	Whether a given font is present or not will depend on the
	configuration in which MuPDF is built.

	name: The name of the font desired.

	len: Pointer to a place to receive the length of the discovered
	font buffer.

	Returns a pointer to the font file data, or NULL if not present.
*/
const unsigned char *fz_lookup_base14_font(fz_context *ctx, const char *name, int *len);

/**
	Search the builtin cjk fonts for a match.
	Whether a font is present or not will depend on the
	configuration in which MuPDF is built.

	ordering: The desired ordering of the font (e.g. FZ_ADOBE_KOREA).

	len: Pointer to a place to receive the length of the discovered
	font buffer.

	Returns a pointer to the font file data, or NULL if not present.
*/
const unsigned char *fz_lookup_cjk_font(fz_context *ctx, int ordering, int *len, int *index);

/**
	Search the builtin cjk fonts for a match for a given language.
	Whether a font is present or not will depend on the
	configuration in which MuPDF is built.

	lang: Pointer to a (case sensitive) language string (e.g.
	"ja", "ko", "zh-Hant" etc).

	len: Pointer to a place to receive the length of the discovered
	font buffer.

	subfont: Pointer to a place to store the subfont index of the
	discovered font.

	Returns a pointer to the font file data, or NULL if not present.
*/
const unsigned char *fz_lookup_cjk_font_by_language(fz_context *ctx, const char *lang, int *len, int *subfont);

/**
	Return the matching FZ_ADOBE_* ordering
	for the given language tag, such as "zh-Hant", "zh-Hans", "ja", or "ko".
*/
int fz_lookup_cjk_ordering_by_language(const char *name);

/**
	Search the builtin noto fonts for a match.
	Whether a font is present or not will depend on the
	configuration in which MuPDF is built.

	script: The script desired (e.g. UCDN_SCRIPT_KATAKANA).

	lang: The language desired (e.g. FZ_LANG_ja).

	len: Pointer to a place to receive the length of the discovered
	font buffer.

	Returns a pointer to the font file data, or NULL if not present.
*/
const unsigned char *fz_lookup_noto_font(fz_context *ctx, int script, int lang, int *len, int *subfont);

/**
	Search the builtin noto fonts specific symbol fonts.
	Whether a font is present or not will depend on the
	configuration in which MuPDF is built.
*/
const unsigned char *fz_lookup_noto_math_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_music_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_symbol1_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_symbol2_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_emoji_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_boxes_font(fz_context *ctx, int *len);

/**
	Try to load a fallback font for the
	given combination of font attributes. Whether a font is
	present or not will depend on the configuration in which
	MuPDF is built.

	script: The script desired (e.g. UCDN_SCRIPT_KATAKANA).

	language: The language desired (e.g. FZ_LANG_ja).

	serif: 1 if serif desired, 0 otherwise.

	bold: 1 if bold desired, 0 otherwise.

	italic: 1 if italic desired, 0 otherwise.

	Returns a new font handle, or NULL if not available.
*/
fz_font *fz_load_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic);

/**
	Create a new (empty) type3 font.

	name: Name of font (or NULL).

	matrix: Font matrix.

	Returns a new font handle, or throws exception on
	allocation failure.
*/
fz_font *fz_new_type3_font(fz_context *ctx, const char *name, fz_matrix matrix);

/**
	Create a new font from a font file in memory.

	Fonts created in this way, will be eligible for embedding by default.

	name: Name of font (leave NULL to use name from font).

	data: Pointer to the font file data.

	len: Length of the font file data.

	index: Which font from the file to load (0 for default).

	use_glyph_box: 1 if we should use the glyph bbox, 0 otherwise.

	Returns new font handle, or throws exception on error.
*/
fz_font *fz_new_font_from_memory(fz_context *ctx, const char *name, const unsigned char *data, int len, int index, int use_glyph_bbox);

/**
	Create a new font from a font file in a fz_buffer.

	Fonts created in this way, will be eligible for embedding by default.

	name: Name of font (leave NULL to use name from font).

	buffer: Buffer to load from.

	index: Which font from the file to load (0 for default).

	use_glyph_box: 1 if we should use the glyph bbox, 0 otherwise.

	Returns new font handle, or throws exception on error.
*/
fz_font *fz_new_font_from_buffer(fz_context *ctx, const char *name, fz_buffer *buffer, int index, int use_glyph_bbox);

/**
	Create a new font from a font file.

	Fonts created in this way, will be eligible for embedding by default.

	name: Name of font (leave NULL to use name from font).

	path: File path to load from.

	index: Which font from the file to load (0 for default).

	use_glyph_box: 1 if we should use the glyph bbox, 0 otherwise.

	Returns new font handle, or throws exception on error.
*/
fz_font *fz_new_font_from_file(fz_context *ctx, const char *name, const char *path, int index, int use_glyph_bbox);

/**
	Create a new font from one of the built-in fonts.
*/
fz_font *fz_new_base14_font(fz_context *ctx, const char *name);
fz_font *fz_new_cjk_font(fz_context *ctx, int ordering);
fz_font *fz_new_builtin_font(fz_context *ctx, const char *name, int is_bold, int is_italic);

/**
	Control whether a given font should be embedded or not when writing.
*/
void fz_set_font_embedding(fz_context *ctx, fz_font *font, int embed);

/**
	Add a reference to an existing fz_font.

	font: The font to add a reference to.

	Returns the same font.
*/
fz_font *fz_keep_font(fz_context *ctx, fz_font *font);

/**
	Drop a reference to a fz_font, destroying the
	font when the last reference is dropped.

	font: The font to drop a reference to.
*/
void fz_drop_font(fz_context *ctx, fz_font *font);

/**
	Set the font bbox.

	font: The font to set the bbox for.

	xmin, ymin, xmax, ymax: The bounding box.
*/
void fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax);

/**
	Return a bbox for a given glyph in a font.

	font: The font to look for the glyph in.

	gid: The glyph to bound.

	trm: The matrix to apply to the glyph before bounding.

	Returns rectangle by value containing the bounds of the given
	glyph.
*/
fz_rect fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm);

/**
	Determine if a given glyph in a font
	is cacheable. Certain glyphs in a type 3 font cannot safely
	be cached, as their appearance depends on the enclosing
	graphic state.

	font: The font to look for the glyph in.

	gif: The glyph to query.

	Returns non-zero if cacheable, 0 if not.
*/
int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid);

/**
	Run a glyph from a Type3 font to
	a given device.

	font: The font to find the glyph in.

	gid: The glyph to run.

	trm: The transform to apply.

	dev: The device to render onto.
*/
void fz_run_t3_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, struct fz_device *dev);

/**
	Return the advance for a given glyph.

	font: The font to look for the glyph in.

	glyph: The glyph to find the advance for.

	wmode: 1 for vertical mode, 0 for horizontal.

	Returns the advance for the glyph.
*/
float fz_advance_glyph(fz_context *ctx, fz_font *font, int glyph, int wmode);

/**
	Find the glyph id for a given unicode
	character within a font.

	font: The font to look for the unicode character in.

	unicode: The unicode character to encode.

	Returns the glyph id for the given unicode value, or 0 if
	unknown.
*/
int fz_encode_character(fz_context *ctx, fz_font *font, int unicode);

/**
	Encode character, preferring small-caps variant if available.

	font: The font to look for the unicode character in.

	unicode: The unicode character to encode.

	Returns the glyph id for the given unicode value, or 0 if
	unknown.
*/
int fz_encode_character_sc(fz_context *ctx, fz_font *font, int unicode);

/**
	Encode character.

	Either by direct lookup of glyphname within a font, or, failing
	that, by mapping glyphname to unicode and thence to the glyph
	index within the given font.

	Returns zero for type3 fonts.
*/
int fz_encode_character_by_glyph_name(fz_context *ctx, fz_font *font, const char *glyphname);

/**
	Find the glyph id for
	a given unicode character within a font, falling back to
	an alternative if not found.

	font: The font to look for the unicode character in.

	unicode: The unicode character to encode.

	script: The script in use.

	language: The language in use.

	out_font: The font handle in which the given glyph represents
	the requested unicode character. The caller does not own the
	reference it is passed, so should call fz_keep_font if it is
	not simply to be used immediately.

	Returns the glyph id for the given unicode value in the supplied
	font (and sets *out_font to font) if it is present. Otherwise
	an alternative fallback font (based on script/language) is
	searched for. If the glyph is found therein, *out_font is set
	to this reference, and the glyph reference is returned. If it
	cannot be found anywhere, the function returns 0.
*/
int fz_encode_character_with_fallback(fz_context *ctx, fz_font *font, int unicode, int script, int language, fz_font **out_font);

/**
	Find the name of a glyph

	font: The font to look for the glyph in.

	glyph: The glyph id to look for.

	buf: Pointer to a buffer for the name to be inserted into.

	size: The size of the buffer.

	If a font contains a name table, then the name of the glyph
	will be returned in the supplied buffer. Otherwise a name
	is synthesised. The name will be truncated to fit in
	the buffer.
*/
void fz_get_glyph_name(fz_context *ctx, fz_font *font, int glyph, char *buf, int size);

/**
	Retrieve font ascender in ems.
*/
float fz_font_ascender(fz_context *ctx, fz_font *font);

/**
	Retrieve font descender in ems.
*/
float fz_font_descender(fz_context *ctx, fz_font *font);

/**
	Retrieve the MD5 digest for the font's data.
*/
void fz_font_digest(fz_context *ctx, fz_font *font, unsigned char digest[16]);

/* Implementation details: subject to change. */

void fz_decouple_type3_font(fz_context *ctx, fz_font *font, void *t3doc);

/**
	map an FT error number to a
	static string.

	err: The error number to lookup.

	Returns a pointer to a static textual representation
	of a freetype error.
*/
const char *ft_error_string(int err);
int ft_char_index(void *face, int cid);
int ft_name_index(void *face, const char *name);

/**
	Internal functions for our Harfbuzz integration
	to work around the lack of thread safety.
*/

/**
	Lock against Harfbuzz being called
	simultaneously in several threads. This reuses
	FZ_LOCK_FREETYPE.
*/
void fz_hb_lock(fz_context *ctx);

/**
	Unlock after a Harfbuzz call. This reuses
	FZ_LOCK_FREETYPE.
*/
void fz_hb_unlock(fz_context *ctx);

struct fz_font
{
	int refs;
	char name[32];
	fz_buffer *buffer;

	fz_font_flags_t flags;

	void *ft_face; /* has an FT_Face if used */
	fz_shaper_data_t shaper_data;

	fz_matrix t3matrix;
	void *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used */
	struct fz_display_list **t3lists; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	unsigned short *t3flags; /* has 256 entries if used */
	void *t3doc; /* a pdf_document for the callback */
	void (*t3run)(fz_context *ctx, void *doc, void *resources, fz_buffer *contents, struct fz_device *dev, fz_matrix ctm, void *gstate, fz_default_colorspaces *default_cs);
	void (*t3freeres)(fz_context *ctx, void *doc, void *resources);

	fz_rect bbox;	/* font bbox is used only for t3 fonts */

	int glyph_count;

	/* per glyph bounding box cache. */
	fz_rect **bbox_table;
	int use_glyph_bbox;

	/* substitute metrics */
	int width_count;
	short width_default; /* in 1000 units */
	short *width_table; /* in 1000 units */

	/* cached glyph metrics */
	float **advance_cache;

	/* cached encoding lookup */
	uint16_t *encoding_cache[256];

	/* cached md5sum for caching */
	int has_digest;
	unsigned char digest[16];

	/* Which font to use in a collection. */
	int subfont;
};

void fz_ft_lock(fz_context *ctx);

void fz_ft_unlock(fz_context *ctx);

/* Internal function. Must be called with FT_ALLOC_LOCK
 * held. Returns 1 if this thread (context!) already holds
 * the freeetype lock. */
int fz_ft_lock_held(fz_context *ctx);

#endif
