#ifndef MUPDF_FITZ_FONT_H
#define MUPDF_FITZ_FONT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/buffer.h"

/* forward declaration for circular dependency */
struct fz_device_s;

/* Various font encoding tables and lookup functions */

extern const char *fz_glyph_name_from_adobe_standard[256];
extern const char *fz_glyph_name_from_iso8859_1[256];
extern const char *fz_glyph_name_from_iso8859_7[256];
extern const char *fz_glyph_name_from_koi8u[256];
extern const char *fz_glyph_name_from_mac_expert[256];
extern const char *fz_glyph_name_from_mac_roman[256];
extern const char *fz_glyph_name_from_win_ansi[256];
extern const char *fz_glyph_name_from_windows_1250[256];
extern const char *fz_glyph_name_from_windows_1251[256];
extern const char *fz_glyph_name_from_windows_1252[256];

extern const unsigned short fz_unicode_from_iso8859_1[256];
extern const unsigned short fz_unicode_from_iso8859_7[256];
extern const unsigned short fz_unicode_from_koi8u[256];
extern const unsigned short fz_unicode_from_pdf_doc_encoding[256];
extern const unsigned short fz_unicode_from_windows_1250[256];
extern const unsigned short fz_unicode_from_windows_1251[256];
extern const unsigned short fz_unicode_from_windows_1252[256];

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

/*
	An abstract font handle.
*/
typedef struct fz_font_s fz_font;

/*
 * Fonts come in two variants:
 *	Regular fonts are handled by FreeType.
 *	Type 3 fonts have callbacks to the interpreter.
 */

void *fz_font_ft_face(fz_context *ctx, fz_font *font);

fz_buffer **fz_font_t3_procs(fz_context *ctx, fz_font *font);

const char *ft_error_string(int err);
int ft_char_index(void *face, int cid);
int ft_name_index(void *face, const char *name);

/* common CJK font collections */
enum { FZ_ADOBE_CNS, FZ_ADOBE_GB, FZ_ADOBE_JAPAN, FZ_ADOBE_KOREA };

/*
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
} fz_font_flags_t;

fz_font_flags_t *fz_font_flags(fz_font *font);

/*
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

fz_shaper_data_t *fz_font_shaper_data(fz_context *ctx, fz_font *font);

struct fz_font_s
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
	struct fz_display_list_s **t3lists; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	unsigned short *t3flags; /* has 256 entries if used */
	void *t3doc; /* a pdf_document for the callback */
	void (*t3run)(fz_context *ctx, void *doc, void *resources, fz_buffer *contents, struct fz_device_s *dev, fz_matrix ctm, void *gstate, fz_default_colorspaces *default_cs);
	void (*t3freeres)(fz_context *ctx, void *doc, void *resources);

	fz_rect bbox;	/* font bbox is used only for t3 fonts */

	int glyph_count;

	/* per glyph bounding box cache */
	fz_rect *bbox_table;

	/* substitute metrics */
	int width_count;
	short width_default; /* in 1000 units */
	short *width_table; /* in 1000 units */

	/* cached glyph metrics */
	float *advance_cache;

	/* cached encoding lookup */
	uint16_t *encoding_cache[256];

	/* cached md5sum for caching */
	int has_digest;
	unsigned char digest[16];
};


const char *fz_font_name(fz_context *ctx, fz_font *font);

int fz_font_is_bold(fz_context *ctx, fz_font *font);
int fz_font_is_italic(fz_context *ctx, fz_font *font);
int fz_font_is_serif(fz_context *ctx, fz_font *font);
int fz_font_is_monospaced(fz_context *ctx, fz_font *font);

fz_rect fz_font_bbox(fz_context *ctx, fz_font *font);

/*
	Type for user supplied system font loading hook.

	name: The name of the font to load.
	bold: 1 if a bold font desired, 0 otherwise.
	italic: 1 if an italic font desired, 0 otherwise.
	needs_exact_metrics: 1 if an exact metric match is required for the font requested.

	Returns a new font handle, or NULL if no font found (or on error).
*/
typedef fz_font *(fz_load_system_font_fn)(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics);

/*
	Type for user supplied cjk font loading hook.

	name: The name of the font to load.
	ordering: The ordering for which to load the font (e.g. FZ_ADOBE_KOREA)
	serif: 1 if a serif font is desired, 0 otherwise.

	Returns a new font handle, or NULL if no font found (or on error).
*/
typedef fz_font *(fz_load_system_cjk_font_fn)(fz_context *ctx, const char *name, int ordering, int serif);

/*
	Type for user supplied fallback font loading hook.

	name: The name of the font to load.
	script: UCDN script enum.
	language: FZ_LANG enum.
	serif, bold, italic: boolean style flags.

	Returns a new font handle, or NULL if no font found (or on error).
*/
typedef fz_font *(fz_load_system_fallback_font_fn)(fz_context *ctx, int script, int language, int serif, int bold, int italic);

void fz_install_load_system_font_funcs(fz_context *ctx,
	fz_load_system_font_fn *f,
	fz_load_system_cjk_font_fn *f_cjk,
	fz_load_system_fallback_font_fn *f_fallback);

fz_font *fz_load_system_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics);

fz_font *fz_load_system_cjk_font(fz_context *ctx, const char *name, int ordering, int serif);

const unsigned char *fz_lookup_builtin_font(fz_context *ctx, const char *name, int bold, int italic, int *len);

const unsigned char *fz_lookup_base14_font(fz_context *ctx, const char *name, int *len);

const unsigned char *fz_lookup_cjk_font(fz_context *ctx, int ordering, int *len, int *index);
const unsigned char *fz_lookup_cjk_font_by_language(fz_context *ctx, const char *lang, int *size, int *subfont);

int fz_lookup_cjk_ordering_by_language(const char *name);

const unsigned char *fz_lookup_noto_font(fz_context *ctx, int script, int lang, int *len, int *subfont);

const unsigned char *fz_lookup_noto_math_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_music_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_symbol1_font(fz_context *ctx, int *len);
const unsigned char *fz_lookup_noto_symbol2_font(fz_context *ctx, int *len);

const unsigned char *fz_lookup_noto_emoji_font(fz_context *ctx, int *len);

fz_font *fz_load_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic);

fz_font *fz_new_type3_font(fz_context *ctx, const char *name, fz_matrix matrix);

fz_font *fz_new_font_from_memory(fz_context *ctx, const char *name, const unsigned char *data, int len, int index, int use_glyph_bbox);

fz_font *fz_new_font_from_buffer(fz_context *ctx, const char *name, fz_buffer *buffer, int index, int use_glyph_bbox);

fz_font *fz_new_font_from_file(fz_context *ctx, const char *name, const char *path, int index, int use_glyph_bbox);

fz_font *fz_new_base14_font(fz_context *ctx, const char *name);
fz_font *fz_new_cjk_font(fz_context *ctx, int ordering);
fz_font *fz_new_builtin_font(fz_context *ctx, const char *name, int is_bold, int is_italic);

fz_font *fz_keep_font(fz_context *ctx, fz_font *font);

void fz_drop_font(fz_context *ctx, fz_font *font);

void fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax);

fz_rect fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm);

int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid);

void fz_run_t3_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, struct fz_device_s *dev);

void fz_decouple_type3_font(fz_context *ctx, fz_font *font, void *t3doc);

float fz_advance_glyph(fz_context *ctx, fz_font *font, int glyph, int wmode);

int fz_encode_character(fz_context *ctx, fz_font *font, int unicode);
int fz_encode_character_sc(fz_context *ctx, fz_font *font, int unicode);
int fz_encode_character_by_glyph_name(fz_context *ctx, fz_font *font, const char *glyphname);

int fz_encode_character_with_fallback(fz_context *ctx, fz_font *font, int unicode, int script, int language, fz_font **out_font);

void fz_get_glyph_name(fz_context *ctx, fz_font *font, int glyph, char *buf, int size);

float fz_font_ascender(fz_context *ctx, fz_font *font);
float fz_font_descender(fz_context *ctx, fz_font *font);

void fz_font_digest(fz_context *ctx, fz_font *font, unsigned char digest[16]);

/*
	Internal functions for our Harfbuzz integration
	to work around the lack of thread safety.
*/

void fz_hb_lock(fz_context *ctx);
void fz_hb_unlock(fz_context *ctx);

#endif
