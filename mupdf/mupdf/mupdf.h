#ifndef _MUPDF_H_
#define _MUPDF_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before mupdf.h"
#endif

typedef struct pdf_xref_s pdf_xref;

void pdf_logxref(char *fmt, ...);
void pdf_logrsrc(char *fmt, ...);
void pdf_logfont(char *fmt, ...);
void pdf_logimage(char *fmt, ...);
void pdf_logshade(char *fmt, ...);
void pdf_logpage(char *fmt, ...);

/*
 * tokenizer and low-level object parser
 */

typedef enum pdf_token_e
{
	PDF_TERROR, PDF_TEOF,
	PDF_TOARRAY, PDF_TCARRAY,
	PDF_TODICT, PDF_TCDICT,
	PDF_TOBRACE, PDF_TCBRACE,
	PDF_TNAME, PDF_TINT, PDF_TREAL, PDF_TSTRING, PDF_TKEYWORD,
	PDF_TR, PDF_TTRUE, PDF_TFALSE, PDF_TNULL,
	PDF_TOBJ, PDF_TENDOBJ,
	PDF_TSTREAM, PDF_TENDSTREAM,
	PDF_TXREF, PDF_TTRAILER, PDF_TSTARTXREF,
	PDF_NTOKENS
} pdf_token_e;

/* lex.c */
fz_error pdf_lex(pdf_token_e *tok, fz_stream *f, char *buf, int n, int *len);

/* parse.c */
fz_error pdf_parsearray(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap);
fz_error pdf_parsedict(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap);
fz_error pdf_parsestmobj(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap);
fz_error pdf_parseindobj(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap, int *num, int *gen, int *stmofs);

fz_rect pdf_torect(fz_obj *array);
fz_matrix pdf_tomatrix(fz_obj *array);
char * pdf_toutf8(fz_obj *src);
unsigned short * pdf_toucs2(fz_obj *src);

/*
 * Encryption
 */

/* Permission flag bits */
#define PDF_PERM_PRINT (1<<2)
#define PDF_PERM_CHANGE (1<<3)
#define PDF_PERM_COPY (1<<4)
#define PDF_PERM_NOTES (1<<5)
#define PDF_PERM_FILL_FORM (1<<8)
#define PDF_PERM_ACCESSIBILITY (1<<9)
#define PDF_PERM_ASSEMBLE (1<<10)
#define PDF_PERM_HIGH_RES_PRINT (1<<11)
#define PDF_DEFAULT_PERM_FLAGS 0xfffc

typedef struct pdf_crypt_s pdf_crypt;
typedef struct pdf_cryptfilter_s pdf_cryptfilter;
typedef enum pdf_cryptmethod_e pdf_cryptmethod;

enum pdf_cryptmethod_e
{
	PDF_CRYPT_NONE,
	PDF_CRYPT_RC4,
	PDF_CRYPT_AESV2,
	PDF_CRYPT_UNKNOWN,
};

struct pdf_cryptfilter_s
{
	pdf_cryptmethod method;
	int length;
	unsigned char key[16];
};

struct pdf_crypt_s
{
	unsigned char idstring[32];
	int idlength;

	int v;
	int length;
	fz_obj *cf;
	pdf_cryptfilter stmf;
	pdf_cryptfilter strf;

	int r;
	unsigned char o[32];
	unsigned char u[32];
	int p;
	int encryptmetadata;

	unsigned char key[32]; /* decryption key generated from password */
};

/* crypt.c */
fz_error pdf_newcrypt(pdf_crypt **cp, fz_obj *enc, fz_obj *id);
void pdf_freecrypt(pdf_crypt *crypt);

fz_error pdf_parsecryptfilter(pdf_cryptfilter *cf, fz_obj *dict, int defaultlength);
fz_filter * pdf_cryptstream(pdf_crypt *crypt, pdf_cryptfilter *cf, int num, int gen);
void pdf_cryptobj(pdf_crypt *crypt, fz_obj *obj, int num, int gen);

int pdf_needspassword(pdf_xref *xref);
int pdf_authenticatepassword(pdf_xref *xref, char *pw);

/*
 * xref and object / stream api
 */

typedef struct pdf_xrefentry_s pdf_xrefentry;

struct pdf_xref_s
{
	fz_stream *file;
	int version;
	int startxref;
	pdf_crypt *crypt;
	fz_obj *trailer;

	int len;
	int cap;
	pdf_xrefentry *table;

	int pagelen;
	int pagecap;
	fz_obj **pageobjs;
	fz_obj **pagerefs;

	struct pdf_store_s *store;
	struct pdf_outline_s *outlines;

	char scratch[65536];
};

struct pdf_xrefentry_s
{
	int ofs;	/* file offset / objstm object number */
	int gen;	/* generation / objstm index */
	int stmofs;	/* on-disk stream */
	fz_obj *obj;	/* stored/cached object */
	int type;	/* 0=unset (f)ree i(n)use (o)bjstm */
};

pdf_xref * pdf_openxref(fz_stream *file);
void pdf_closexref(pdf_xref *);
void pdf_debugxref(pdf_xref *);
void pdf_flushxref(pdf_xref *, int force);

fz_error pdf_cacheobject(pdf_xref *, int num, int gen);
fz_error pdf_loadobject(fz_obj **objp, pdf_xref *, int num, int gen);

int pdf_isstream(pdf_xref *xref, int num, int gen);
fz_filter * pdf_buildinlinefilter(pdf_xref *xref, fz_obj *stmobj);
fz_error pdf_loadrawstream(fz_buffer **bufp, pdf_xref *xref, int num, int gen);
fz_error pdf_loadstream(fz_buffer **bufp, pdf_xref *xref, int num, int gen);
fz_error pdf_openrawstream(fz_stream **stmp, pdf_xref *, int num, int gen);
fz_error pdf_openstream(fz_stream **stmp, pdf_xref *, int num, int gen);
fz_error pdf_openstreamat(fz_stream **stmp, pdf_xref *xref, int num, int gen, fz_obj *dict, int stmofs);

/* private */
extern fz_error pdf_repairxref(pdf_xref *xref, char *buf, int bufsize);

/*
 * Resource store
 */

typedef struct pdf_store_s pdf_store;

typedef enum pdf_itemkind_e
{
	PDF_KCOLORSPACE,
	PDF_KFUNCTION,
	PDF_KXOBJECT,
	PDF_KIMAGE,
	PDF_KPATTERN,
	PDF_KSHADE,
	PDF_KCMAP,
	PDF_KFONT
} pdf_itemkind;

pdf_store * pdf_newstore(void);
void pdf_emptystore(pdf_store *store);
void pdf_freestore(pdf_store *store);
void pdf_debugstore(pdf_store *store);

void pdf_agestoreditems(pdf_store *store);
void pdf_evictageditems(pdf_store *store);

void pdf_storeitem(pdf_store *store, pdf_itemkind tag, fz_obj *key, void *val);
void *pdf_finditem(pdf_store *store, pdf_itemkind tag, fz_obj *key);
void pdf_removeitem(pdf_store *store, pdf_itemkind tag, fz_obj *key);

/*
 * Functions
 */

typedef struct pdf_function_s pdf_function;

fz_error pdf_loadfunction(pdf_function **func, pdf_xref *xref, fz_obj *ref);
fz_error pdf_evalfunction(pdf_function *func, float *in, int inlen, float *out, int outlen);
pdf_function *pdf_keepfunction(pdf_function *func);
void pdf_dropfunction(pdf_function *func);

/*
 * ColorSpace
 */

typedef struct pdf_indexed_s pdf_indexed;

struct pdf_indexed_s
{
	fz_colorspace super;	/* hmmm... */
	fz_colorspace *base;
	int high;
	unsigned char *lookup;
};

void pdf_convcolor(fz_colorspace *ss, float *sv, fz_colorspace *ds, float *dv);
void pdf_convpixmap(fz_colorspace *ss, fz_pixmap *sp, fz_colorspace *ds, fz_pixmap *dp);

fz_error pdf_loadcolorspace(fz_colorspace **csp, pdf_xref *xref, fz_obj *obj);

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

fz_error pdf_loadpattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *obj);
pdf_pattern *pdf_keeppattern(pdf_pattern *pat);
void pdf_droppattern(pdf_pattern *pat);

/*
 * Shading
 */

fz_error pdf_loadshade(fz_shade **shadep, pdf_xref *xref, fz_obj *obj);

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
	fz_obj *resources;
	fz_buffer *contents;
};

fz_error pdf_loadxobject(pdf_xobject **xobjp, pdf_xref *xref, fz_obj *obj);
pdf_xobject *pdf_keepxobject(pdf_xobject *xobj);
void pdf_dropxobject(pdf_xobject *xobj);

/*
 * Image
 */

typedef struct pdf_image_s pdf_image;

struct pdf_image_s
{
	int refs;
	int w, h, n, a;
	fz_colorspace *cs;
	pdf_image *mask;			/* explicit mask with subimage */
	int usecolorkey;		/* explicit color-keyed masking */
	int colorkey[FZ_MAXCOLORS * 2];
	pdf_indexed *indexed;
	float decode[32];
	int bpc;
	int stride;
	fz_buffer *samples;
};

fz_error pdf_loadinlineimage(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *file);
fz_error pdf_loadimage(pdf_image **imgp, pdf_xref *xref, fz_obj *obj);
fz_error pdf_loadtile(pdf_image *image, fz_pixmap *tile);
pdf_image *pdf_keepimage(pdf_image *img);
void pdf_dropimage(pdf_image *img);

/*
 * CMap
 */

typedef struct pdf_cmap_s pdf_cmap;
typedef struct pdf_range_s pdf_range;

enum { PDF_CMAP_SINGLE, PDF_CMAP_RANGE, PDF_CMAP_TABLE, PDF_CMAP_MULTI };

struct pdf_range_s
{
	unsigned short low;
	unsigned short high;
	unsigned short flag;	/* single, range, table, multi */
	unsigned short offset;	/* range-delta or table-index */
};

struct pdf_cmap_s
{
	int refs;
	char cmapname[32];

	char usecmapname[32];
	pdf_cmap *usecmap;

	int wmode;

	int ncspace;
	struct
	{
		unsigned short n;
		unsigned short low;
		unsigned short high;
	} cspace[40];

	int rlen, rcap;
	pdf_range *ranges;

	int tlen, tcap;
	unsigned short *table;
};

extern pdf_cmap *pdf_cmaptable[]; /* list of builtin system cmaps */

pdf_cmap *pdf_newcmap(void);
pdf_cmap *pdf_keepcmap(pdf_cmap *cmap);
void pdf_dropcmap(pdf_cmap *cmap);

void pdf_debugcmap(pdf_cmap *cmap);
int pdf_getwmode(pdf_cmap *cmap);
pdf_cmap *pdf_getusecmap(pdf_cmap *cmap);
void pdf_setwmode(pdf_cmap *cmap, int wmode);
void pdf_setusecmap(pdf_cmap *cmap, pdf_cmap *usecmap);

void pdf_addcodespace(pdf_cmap *cmap, int low, int high, int n);
void pdf_maprangetotable(pdf_cmap *cmap, int low, int *map, int len);
void pdf_maprangetorange(pdf_cmap *cmap, int srclo, int srchi, int dstlo);
void pdf_maponetomany(pdf_cmap *cmap, int one, int *many, int len);
void pdf_sortcmap(pdf_cmap *cmap);

int pdf_lookupcmap(pdf_cmap *cmap, int cpt);
int pdf_lookupcmapfull(pdf_cmap *cmap, int cpt, int *out);
unsigned char *pdf_decodecmap(pdf_cmap *cmap, unsigned char *s, int *cpt);

pdf_cmap * pdf_newidentitycmap(int wmode, int bytes);
fz_error pdf_parsecmap(pdf_cmap **cmapp, fz_stream *file);
fz_error pdf_loadembeddedcmap(pdf_cmap **cmapp, pdf_xref *xref, fz_obj *ref);
fz_error pdf_loadsystemcmap(pdf_cmap **cmapp, char *name);

/*
 * Font
 */

void pdf_loadencoding(char **estrings, char *encoding);
int pdf_lookupagl(char *name);
char **pdf_lookupaglnames(int ucs);

extern const unsigned short pdf_docencoding[256];
extern const char * const pdf_macroman[256];
extern const char * const pdf_macexpert[256];
extern const char * const pdf_winansi[256];
extern const char * const pdf_standard[256];
extern const char * const pdf_expert[256];
extern const char * const pdf_symbol[256];
extern const char * const pdf_zapfdingbats[256];

typedef struct pdf_hmtx_s pdf_hmtx;
typedef struct pdf_vmtx_s pdf_vmtx;
typedef struct pdf_fontdesc_s pdf_fontdesc;

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

struct pdf_fontdesc_s
{
	int refs;

	fz_font *font;
	unsigned char *buffer; /* contains allocated memory that should be freed */

	/* FontDescriptor */
	int flags;
	float italicangle;
	float ascent;
	float descent;
	float capheight;
	float xheight;
	float missingwidth;

	/* Encoding (CMap) */
	pdf_cmap *encoding;
	pdf_cmap *tottfcmap;
	int ncidtogid;
	unsigned short *cidtogid;

	/* ToUnicode */
	pdf_cmap *tounicode;
	int ncidtoucs;
	unsigned short *cidtoucs;

	/* Metrics (given in the PDF file) */
	int wmode;

	int nhmtx, hmtxcap;
	pdf_hmtx dhmtx;
	pdf_hmtx *hmtx;

	int nvmtx, vmtxcap;
	pdf_vmtx dvmtx;
	pdf_vmtx *vmtx;

	int isembedded;
};

/* fontmtx.c */
void pdf_setfontwmode(pdf_fontdesc *font, int wmode);
void pdf_setdefaulthmtx(pdf_fontdesc *font, int w);
void pdf_setdefaultvmtx(pdf_fontdesc *font, int y, int w);
void pdf_addhmtx(pdf_fontdesc *font, int lo, int hi, int w);
void pdf_addvmtx(pdf_fontdesc *font, int lo, int hi, int x, int y, int w);
void pdf_endhmtx(pdf_fontdesc *font);
void pdf_endvmtx(pdf_fontdesc *font);
pdf_hmtx pdf_gethmtx(pdf_fontdesc *font, int cid);
pdf_vmtx pdf_getvmtx(pdf_fontdesc *font, int cid);

/* unicode.c */
fz_error pdf_loadtounicode(pdf_fontdesc *font, pdf_xref *xref, char **strings, char *collection, fz_obj *cmapstm);

/* fontfile.c */
fz_error pdf_loadbuiltinfont(pdf_fontdesc *font, char *basefont);
fz_error pdf_loadembeddedfont(pdf_fontdesc *font, pdf_xref *xref, fz_obj *stmref);
fz_error pdf_loadsystemfont(pdf_fontdesc *font, char *basefont, char *collection);

/* type3.c */
fz_error pdf_loadtype3font(pdf_fontdesc **fontp, pdf_xref *xref, fz_obj *rdb, fz_obj *obj);

/* font.c */
int pdf_fontcidtogid(pdf_fontdesc *fontdesc, int cid);
fz_error pdf_loadfontdescriptor(pdf_fontdesc *font, pdf_xref *xref, fz_obj *desc, char *collection);
fz_error pdf_loadfont(pdf_fontdesc **fontp, pdf_xref *xref, fz_obj *rdb, fz_obj *obj);
pdf_fontdesc * pdf_newfontdesc(void);
pdf_fontdesc * pdf_keepfont(pdf_fontdesc *fontdesc);
void pdf_dropfont(pdf_fontdesc *font);
void pdf_debugfont(pdf_fontdesc *fontdesc);

/*
 * Interactive features
 */

typedef struct pdf_link_s pdf_link;
typedef struct pdf_outline_s pdf_outline;

typedef enum pdf_linkkind_e
{
	PDF_LGOTO = 0,
	PDF_LURI,
	PDF_LLAUNCH /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=726 */
} pdf_linkkind;

struct pdf_link_s
{
	pdf_linkkind kind;
	fz_rect rect;
	fz_obj *dest;
	pdf_link *next;
};

struct pdf_outline_s
{
	char *title;
	pdf_link *link;
	int count;
	pdf_outline *child;
	pdf_outline *next;
};

fz_obj *pdf_lookupdest(pdf_xref *xref, fz_obj *nameddest);

pdf_link *pdf_newlink(pdf_linkkind kind, fz_rect rect, fz_obj *dest);
pdf_link *pdf_loadlink(pdf_xref *xref, fz_obj *dict);
void pdf_freelink(pdf_link *link);

pdf_outline *pdf_loadoutline(pdf_xref *xref);
void pdf_debugoutline(pdf_outline *outline, int level);
void pdf_freeoutline(pdf_outline *outline);

void pdf_loadannots(pdf_link **, pdf_xref *, fz_obj *annots);

/*
 * Page tree, pages and related objects
 */

typedef struct pdf_page_s pdf_page;

struct pdf_page_s
{
	fz_rect mediabox;
	int rotate;
	fz_obj *resources;
	fz_buffer *contents;
	fz_displaylist *list;
	fz_textspan *text;
	pdf_link *links;
};

/* pagetree.c */
fz_error pdf_loadpagetree(pdf_xref *xref);
int pdf_getpagecount(pdf_xref *xref);
fz_obj * pdf_getpageobject(pdf_xref *xref, int p);
int pdf_findpageobject(pdf_xref *xref, fz_obj *pageobj);

/* page.c */
fz_error pdf_loadpage(pdf_page **pagep, pdf_xref *xref, fz_obj *ref);
void pdf_freepage(pdf_page *page);

/*
 * content stream parsing
 */

typedef struct pdf_material_s pdf_material;
typedef struct pdf_gstate_s pdf_gstate;
typedef struct pdf_csi_s pdf_csi;

enum
{
	PDF_MFILL,
	PDF_MSTROKE
};

enum
{
	PDF_MNONE,
	PDF_MCOLOR,
	PDF_MLAB,
	PDF_MINDEXED,
	PDF_MPATTERN,
	PDF_MSHADE
};

struct pdf_material_s
{
	int kind;
	fz_colorspace *cs;
	pdf_indexed *indexed;
	pdf_pattern *pattern;
	fz_shade *shade;
	float parentalpha;
	float alpha;
	float v[32];
};

struct pdf_gstate_s
{
	fz_matrix ctm;
	int clipdepth;

	/* path stroking */
	fz_strokestate strokestate;

	/* materials */
	pdf_material stroke;
	pdf_material fill;
	fz_blendkind blendmode;

	/* text state */
	float charspace;
	float wordspace;
	float scale;
	float leading;
	pdf_fontdesc *font;
	float size;
	int render;
	float rise;
};

struct pdf_csi_s
{
	fz_device *dev;
	pdf_xref *xref;

	fz_obj *stack[32];
	int top;
	int xbalance;
	fz_obj *array;

	/* path object state */
	fz_path *path;
	int clip;
	int clipevenodd;

	/* text object state */
	fz_text *text;
	fz_matrix tlm;
	fz_matrix tm;
	int textmode;
	int accumulate;

	/* graphics state */
	fz_matrix topctm;
	pdf_gstate gstate[32];
	int gtop;
};

/* build.c */
void pdf_initgstate(pdf_gstate *gs, fz_matrix ctm);
void pdf_setcolorspace(pdf_csi *csi, int what, fz_colorspace *cs);
void pdf_setcolor(pdf_csi *csi, int what, float *v);
void pdf_setpattern(pdf_csi *csi, int what, pdf_pattern *pat, float *v);
void pdf_setshade(pdf_csi *csi, int what, fz_shade *shade);
void pdf_showpath(pdf_csi*, int close, int fill, int stroke, int evenodd);
void pdf_showtext(pdf_csi*, fz_obj *text);
void pdf_flushtext(pdf_csi*);
void pdf_showimage(pdf_csi*, pdf_image *img);
void pdf_showshade(pdf_csi*, fz_shade *shd);

/* interpret.c */
void pdf_gsave(pdf_csi *csi);
void pdf_grestore(pdf_csi *csi);
fz_error pdf_runcsibuffer(pdf_csi *csi, fz_obj *rdb, fz_buffer *contents);
fz_error pdf_runcontentstream(fz_device *dev, fz_matrix ctm, pdf_xref *xref, fz_obj *resources, fz_buffer *contents);

pdf_material * pdf_keepmaterial(pdf_material *mat);
pdf_material * pdf_dropmaterial(pdf_material *mat);

#endif
