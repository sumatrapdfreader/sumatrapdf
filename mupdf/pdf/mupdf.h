#ifndef _MUPDF_H_
#define _MUPDF_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before mupdf.h"
#endif

typedef struct pdf_xref_s pdf_xref;

/*
 * tokenizer and low-level object parser
 */

enum
{
	PDF_TOK_ERROR, PDF_TOK_EOF,
	PDF_TOK_OPEN_ARRAY, PDF_TOK_CLOSE_ARRAY,
	PDF_TOK_OPEN_DICT, PDF_TOK_CLOSE_DICT,
	PDF_TOK_OPEN_BRACE, PDF_TOK_CLOSE_BRACE,
	PDF_TOK_NAME, PDF_TOK_INT, PDF_TOK_REAL, PDF_TOK_STRING, PDF_TOK_KEYWORD,
	PDF_TOK_R, PDF_TOK_TRUE, PDF_TOK_FALSE, PDF_TOK_NULL,
	PDF_TOK_OBJ, PDF_TOK_ENDOBJ,
	PDF_TOK_STREAM, PDF_TOK_ENDSTREAM,
	PDF_TOK_XREF, PDF_TOK_TRAILER, PDF_TOK_STARTXREF,
	PDF_NUM_TOKENS
};

fz_error pdf_lex(int *tok, fz_stream *f, char *buf, int n, int *len);

fz_error pdf_parse_array(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap);
fz_error pdf_parse_dict(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap);
fz_error pdf_parse_stm_obj(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap);
fz_error pdf_parse_ind_obj(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap, int *num, int *gen, int *stm_ofs);

fz_rect pdf_to_rect(fz_obj *array);
fz_matrix pdf_to_matrix(fz_obj *array);
char *pdf_to_utf8(fz_obj *src);
unsigned short *pdf_to_ucs2(fz_obj *src);
fz_obj *pdf_to_utf8_name(fz_obj *src);
char *pdf_from_ucs2(unsigned short *str);

/*
 * xref and object / stream api
 */

typedef struct pdf_xref_entry_s pdf_xref_entry;
typedef struct pdf_crypt_s pdf_crypt;

struct pdf_xref_entry_s
{
	int ofs;	/* file offset / objstm object number */
	int gen;	/* generation / objstm index */
	int stm_ofs;	/* on-disk stream */
	fz_obj *obj;	/* stored/cached object */
	int type;	/* 0=unset (f)ree i(n)use (o)bjstm */
};

struct pdf_xref_s
{
	fz_stream *file;
	int version;
	int startxref;
	int file_size;
	pdf_crypt *crypt;
	fz_obj *trailer;

	int len;
	pdf_xref_entry *table;

	int page_len;
	int page_cap;
	fz_obj **page_objs;
	fz_obj **page_refs;

	struct pdf_store_s *store;

	char scratch[65536];
};

fz_obj *pdf_resolve_indirect(fz_obj *ref);
fz_error pdf_cache_object(pdf_xref *, int num, int gen);
fz_error pdf_load_object(fz_obj **objp, pdf_xref *, int num, int gen);
void pdf_update_object( pdf_xref *xref, int num, int gen, fz_obj *newobj);

int pdf_is_stream(pdf_xref *xref, int num, int gen);
fz_stream *pdf_open_inline_stream(fz_stream *chain, pdf_xref *xref, fz_obj *stmobj, int length);
fz_error pdf_load_raw_stream(fz_buffer **bufp, pdf_xref *xref, int num, int gen);
fz_error pdf_load_stream(fz_buffer **bufp, pdf_xref *xref, int num, int gen);
fz_error pdf_open_raw_stream(fz_stream **stmp, pdf_xref *, int num, int gen);
fz_error pdf_open_stream(fz_stream **stmp, pdf_xref *, int num, int gen);
fz_error pdf_open_stream_at(fz_stream **stmp, pdf_xref *xref, int num, int gen, fz_obj *dict, int stm_ofs);

fz_error pdf_open_xref_with_stream(pdf_xref **xrefp, fz_stream *file, char *password);
fz_error pdf_open_xref(pdf_xref **xrefp, const char *filename, char *password);
void pdf_free_xref(pdf_xref *);

/* private */
fz_error pdf_repair_xref(pdf_xref *xref, char *buf, int bufsize);
fz_error pdf_repair_obj_stms(pdf_xref *xref);
void pdf_debug_xref(pdf_xref *);
void pdf_resize_xref(pdf_xref *xref, int newcap);

/*
 * Encryption
 */

enum
{
	PDF_PERM_PRINT = 1 << 2,
	PDF_PERM_CHANGE = 1 << 3,
	PDF_PERM_COPY = 1 << 4,
	PDF_PERM_NOTES = 1 << 5,
	PDF_PERM_FILL_FORM = 1 << 8,
	PDF_PERM_ACCESSIBILITY = 1 << 9,
	PDF_PERM_ASSEMBLE = 1 << 10,
	PDF_PERM_HIGH_RES_PRINT = 1 << 11,
	PDF_DEFAULT_PERM_FLAGS = 0xfffc
};

fz_error pdf_new_crypt(pdf_crypt **cp, fz_obj *enc, fz_obj *id);
void pdf_free_crypt(pdf_crypt *crypt);

void pdf_crypt_obj(pdf_crypt *crypt, fz_obj *obj, int num, int gen);
fz_stream *pdf_open_crypt(fz_stream *chain, pdf_crypt *crypt, int num, int gen);
fz_stream *pdf_open_crypt_with_filter(fz_stream *chain, pdf_crypt *crypt, char *name, int num, int gen);

int pdf_needs_password(pdf_xref *xref);
int pdf_authenticate_password(pdf_xref *xref, char *pw);
int pdf_has_permission(pdf_xref *xref, int p);

int pdf_get_crypt_revision(pdf_xref *xref);
char *pdf_get_crypt_method(pdf_xref *xref);
int pdf_get_crypt_length(pdf_xref *xref);
unsigned char *pdf_get_crypt_key(pdf_xref *xref);

void pdf_debug_crypt(pdf_crypt *crypt);

/*
 * Resource store
 */

typedef struct pdf_store_s pdf_store;

pdf_store *pdf_new_store(void);
void pdf_free_store(pdf_store *store);
void pdf_debug_store(pdf_store *store);

void pdf_store_item(pdf_store *store, void *keepfn, void *dropfn, fz_obj *key, void *val);
void *pdf_find_item(pdf_store *store, void *dropfn, fz_obj *key);
void pdf_remove_item(pdf_store *store, void *dropfn, fz_obj *key);
void pdf_age_store(pdf_store *store, int maxage);

/*
 * Functions, Colorspaces, Shadings and Images
 */

typedef struct pdf_function_s pdf_function;

fz_error pdf_load_function(pdf_function **func, pdf_xref *xref, fz_obj *ref);
void pdf_eval_function(pdf_function *func, float *in, int inlen, float *out, int outlen);
pdf_function *pdf_keep_function(pdf_function *func);
void pdf_drop_function(pdf_function *func);

fz_error pdf_load_colorspace(fz_colorspace **csp, pdf_xref *xref, fz_obj *obj);
fz_pixmap *pdf_expand_indexed_pixmap(fz_pixmap *src);

fz_error pdf_load_shading(fz_shade **shadep, pdf_xref *xref, fz_obj *obj);

fz_error pdf_load_inline_image(fz_pixmap **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *file);
fz_error pdf_load_image(fz_pixmap **imgp, pdf_xref *xref, fz_obj *obj);
int pdf_is_jpx_image(fz_obj *dict);

/*
 * Pattern
 */

typedef struct pdf_pattern_s pdf_pattern;

struct pdf_pattern_s
{
	int refs;
	int ismask;
	float xstep;
	float ystep;
	fz_matrix matrix;
	fz_rect bbox;
	fz_obj *resources;
	fz_buffer *contents;
};

fz_error pdf_load_pattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *obj);
pdf_pattern *pdf_keep_pattern(pdf_pattern *pat);
void pdf_drop_pattern(pdf_pattern *pat);

/*
 * XObject
 */

typedef struct pdf_xobject_s pdf_xobject;

struct pdf_xobject_s
{
	int refs;
	fz_matrix matrix;
	fz_rect bbox;
	int isolated;
	int knockout;
	int transparency;
	fz_colorspace *colorspace;
	fz_obj *resources;
	fz_buffer *contents;
};

fz_error pdf_load_xobject(pdf_xobject **xobjp, pdf_xref *xref, fz_obj *obj);
pdf_xobject *pdf_keep_xobject(pdf_xobject *xobj);
void pdf_drop_xobject(pdf_xobject *xobj);

/*
 * CMap
 */

typedef struct pdf_cmap_s pdf_cmap;
typedef struct pdf_range_s pdf_range;

enum { PDF_CMAP_SINGLE, PDF_CMAP_RANGE, PDF_CMAP_TABLE, PDF_CMAP_MULTI };

struct pdf_range_s
{
	unsigned short low;
	/* Next, we pack 2 fields into the same unsigned short. Top 14 bits
	 * are the extent, bottom 2 bits are flags: single, range, table,
	 * multi */
	unsigned short extent_flags;
	unsigned short offset;	/* range-delta or table-index */
};

struct pdf_cmap_s
{
	int refs;
	char cmap_name[32];

	char usecmap_name[32];
	pdf_cmap *usecmap;

	int wmode;

	int codespace_len;
	struct
	{
		unsigned short n;
		unsigned short low;
		unsigned short high;
	} codespace[40];

	int rlen, rcap;
	pdf_range *ranges;

	int tlen, tcap;
	unsigned short *table;
};

pdf_cmap *pdf_new_cmap(void);
pdf_cmap *pdf_keep_cmap(pdf_cmap *cmap);
void pdf_drop_cmap(pdf_cmap *cmap);

void pdf_debug_cmap(pdf_cmap *cmap);
int pdf_get_wmode(pdf_cmap *cmap);
void pdf_set_wmode(pdf_cmap *cmap, int wmode);
void pdf_set_usecmap(pdf_cmap *cmap, pdf_cmap *usecmap);

void pdf_add_codespace(pdf_cmap *cmap, int low, int high, int n);
void pdf_map_range_to_table(pdf_cmap *cmap, int low, int *map, int len);
void pdf_map_range_to_range(pdf_cmap *cmap, int srclo, int srchi, int dstlo);
void pdf_map_one_to_many(pdf_cmap *cmap, int one, int *many, int len);
void pdf_sort_cmap(pdf_cmap *cmap);

int pdf_lookup_cmap(pdf_cmap *cmap, int cpt);
int pdf_lookup_cmap_full(pdf_cmap *cmap, int cpt, int *out);
unsigned char *pdf_decode_cmap(pdf_cmap *cmap, unsigned char *s, int *cpt);

pdf_cmap *pdf_new_identity_cmap(int wmode, int bytes);
fz_error pdf_parse_cmap(pdf_cmap **cmapp, fz_stream *file);
fz_error pdf_load_embedded_cmap(pdf_cmap **cmapp, pdf_xref *xref, fz_obj *ref);
fz_error pdf_load_system_cmap(pdf_cmap **cmapp, char *name);
pdf_cmap *pdf_find_builtin_cmap(char *cmap_name);

/*
 * Font
 */

enum
{
	PDF_FD_FIXED_PITCH = 1 << 0,
	PDF_FD_SERIF = 1 << 1,
	PDF_FD_SYMBOLIC = 1 << 2,
	PDF_FD_SCRIPT = 1 << 3,
	PDF_FD_NONSYMBOLIC = 1 << 5,
	PDF_FD_ITALIC = 1 << 6,
	PDF_FD_ALL_CAP = 1 << 16,
	PDF_FD_SMALL_CAP = 1 << 17,
	PDF_FD_FORCE_BOLD = 1 << 18
};

enum { PDF_ROS_CNS, PDF_ROS_GB, PDF_ROS_JAPAN, PDF_ROS_KOREA };

void pdf_load_encoding(char **estrings, char *encoding);
int pdf_lookup_agl(char *name);
const char **pdf_lookup_agl_duplicates(int ucs);

extern const unsigned short pdf_doc_encoding[256];
extern const char * const pdf_mac_roman[256];
extern const char * const pdf_mac_expert[256];
extern const char * const pdf_win_ansi[256];
extern const char * const pdf_standard[256];

typedef struct pdf_font_desc_s pdf_font_desc;
typedef struct pdf_hmtx_s pdf_hmtx;
typedef struct pdf_vmtx_s pdf_vmtx;

struct pdf_hmtx_s
{
	unsigned short lo;
	unsigned short hi;
	int w;	/* type3 fonts can be big! */
};

struct pdf_vmtx_s
{
	unsigned short lo;
	unsigned short hi;
	short x;
	short y;
	short w;
};

struct pdf_font_desc_s
{
	int refs;

	fz_font *font;

	/* FontDescriptor */
	int flags;
	float italic_angle;
	float ascent;
	float descent;
	float cap_height;
	float x_height;
	float missing_width;

	/* Encoding (CMap) */
	pdf_cmap *encoding;
	pdf_cmap *to_ttf_cmap;
	int cid_to_gid_len;
	unsigned short *cid_to_gid;

	/* ToUnicode */
	pdf_cmap *to_unicode;
	int cid_to_ucs_len;
	unsigned short *cid_to_ucs;

	/* Metrics (given in the PDF file) */
	int wmode;

	int hmtx_len, hmtx_cap;
	pdf_hmtx dhmtx;
	pdf_hmtx *hmtx;

	int vmtx_len, vmtx_cap;
	pdf_vmtx dvmtx;
	pdf_vmtx *vmtx;

	int is_embedded;

	/* SumatraPDF: store vertical glyph substitution data for the font's lifetime */
	void *_vsubst;
};

void pdf_set_font_wmode(pdf_font_desc *font, int wmode);
void pdf_set_default_hmtx(pdf_font_desc *font, int w);
void pdf_set_default_vmtx(pdf_font_desc *font, int y, int w);
void pdf_add_hmtx(pdf_font_desc *font, int lo, int hi, int w);
void pdf_add_vmtx(pdf_font_desc *font, int lo, int hi, int x, int y, int w);
void pdf_end_hmtx(pdf_font_desc *font);
void pdf_end_vmtx(pdf_font_desc *font);
pdf_hmtx pdf_get_hmtx(pdf_font_desc *font, int cid);
pdf_vmtx pdf_get_vmtx(pdf_font_desc *font, int cid);

fz_error pdf_load_to_unicode(pdf_font_desc *font, pdf_xref *xref, char **strings, char *collection, fz_obj *cmapstm);

int pdf_font_cid_to_gid(pdf_font_desc *fontdesc, int cid);

unsigned char *pdf_find_builtin_font(char *name, unsigned int *len);
unsigned char *pdf_find_substitute_font(int mono, int serif, int bold, int italic, unsigned int *len);
unsigned char *pdf_find_substitute_cjk_font(int ros, int serif, unsigned int *len);

/* SumatraPDF: use locally installed fonts */
fz_error pdf_load_windows_font(pdf_font_desc *font, char *fontname);
fz_error pdf_load_similar_cjk_font(pdf_font_desc *font, int ros, int serif);

fz_error pdf_load_type3_font(pdf_font_desc **fontp, pdf_xref *xref, fz_obj *rdb, fz_obj *obj);
fz_error pdf_load_font(pdf_font_desc **fontp, pdf_xref *xref, fz_obj *rdb, fz_obj *obj);

pdf_font_desc *pdf_new_font_desc(void);
pdf_font_desc *pdf_keep_font(pdf_font_desc *fontdesc);
void pdf_drop_font(pdf_font_desc *font);

void pdf_debug_font(pdf_font_desc *fontdesc);

/* SumatraPDF */
int pdf_ft_get_vgid(pdf_font_desc *fontdesc, int gid);
void pdf_ft_free_vsubst(pdf_font_desc *fontdesc);

/*
 * Interactive features
 */

typedef struct pdf_link_s pdf_link;
typedef struct pdf_annot_s pdf_annot;
typedef struct pdf_outline_s pdf_outline;

typedef enum pdf_link_kind_e
{
	PDF_LINK_GOTO = 0,
	PDF_LINK_URI,
	PDF_LINK_LAUNCH,
	PDF_LINK_NAMED,
	PDF_LINK_ACTION,
} pdf_link_kind;

struct pdf_link_s
{
	pdf_link_kind kind;
	fz_rect rect;
	fz_obj *dest;
	pdf_link *next;
};

struct pdf_annot_s
{
	fz_obj *obj;
	fz_rect rect;
	pdf_xobject *ap;
	fz_matrix matrix;
	pdf_annot *next;
};

struct pdf_outline_s
{
	char *title;
	pdf_link *link;
	int count;
	pdf_outline *child;
	pdf_outline *next;
};

fz_obj *pdf_lookup_dest(pdf_xref *xref, fz_obj *needle);
fz_obj *pdf_lookup_name(pdf_xref *xref, char *which, fz_obj *needle);
fz_obj *pdf_load_name_tree(pdf_xref *xref, char *which);

pdf_outline *pdf_load_outline(pdf_xref *xref);
void pdf_debug_outline(pdf_outline *outline, int level);
void pdf_free_outline(pdf_outline *outline);

pdf_link *pdf_load_link(pdf_xref *xref, fz_obj *dict);
void pdf_load_links(pdf_link **, pdf_xref *, fz_obj *annots);
void pdf_free_link(pdf_link *link);

void pdf_load_annots(pdf_annot **, pdf_xref *, fz_obj *annots);
void pdf_free_annot(pdf_annot *link);

/*
 * Page tree, pages and related objects
 */

typedef struct pdf_page_s pdf_page;

struct pdf_page_s
{
	fz_rect mediabox;
	int rotate;
	int transparency;
	fz_obj *resources;
	fz_buffer *contents;
	pdf_link *links;
	pdf_annot *annots;
};

fz_error pdf_load_page_tree(pdf_xref *xref);
int pdf_find_page_number(pdf_xref *xref, fz_obj *pageobj);
int pdf_count_pages(pdf_xref *xref);

fz_error pdf_load_page(pdf_page **pagep, pdf_xref *xref, int number);
void pdf_free_page(pdf_page *page);

/*
 * Content stream parsing
 */

fz_error pdf_run_page_with_usage(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm, char *target);
fz_error pdf_run_page(pdf_xref *xref, pdf_page *page, fz_device *dev, fz_matrix ctm);
fz_error pdf_run_glyph(pdf_xref *xref, fz_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm);

#endif
