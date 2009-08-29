#ifndef _MUPDF_H_
#define _MUPDF_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before mupdf.h"
#endif

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
fz_error pdf_parseindobj(fz_obj **op, pdf_xref *xref, fz_stream *f, char *buf, int cap, int *oid, int *gen, int *stmofs);

fz_rect pdf_torect(fz_obj *array);
fz_matrix pdf_tomatrix(fz_obj *array);
fz_error pdf_toutf8(char **dstp, fz_obj *src);
fz_error pdf_toucs2(unsigned short **dstp, fz_obj *src);

/*
 * Encryption
 */

/* Permission flag bits */
#define PDF_PERM_PRINT          (1<<2)
#define PDF_PERM_CHANGE         (1<<3)
#define PDF_PERM_COPY           (1<<4)
#define PDF_PERM_NOTES          (1<<5)
#define PDF_PERM_FILL_FORM      (1<<8)
#define PDF_PERM_ACCESSIBILITY  (1<<9)
#define PDF_PERM_ASSEMBLE       (1<<10)
#define PDF_PERM_HIGH_RES_PRINT (1<<11)
#define PDF_DEFAULT_PERM_FLAGS  0xfffc

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
fz_error pdf_cryptstream(fz_filter **fp, pdf_crypt *crypt, pdf_cryptfilter *cf, int num, int gen);
void pdf_cryptobj(pdf_crypt *crypt, fz_obj *obj, int num, int gen);

int pdf_needspassword(pdf_xref *xref);
int pdf_authenticatepassword(pdf_xref *xref, char *pw);

/*
 * xref and object / stream api
 */

typedef struct pdf_xrefentry_s pdf_xrefentry;
/* typedef struct pdf_xref_s pdf_xref; -- already defined in fitz_stream.h */

struct pdf_xref_s
{
	fz_stream *file;
	int version;
	int startxref;
	pdf_crypt *crypt;

	fz_obj *trailer;		/* TODO split this into root/info/encrypt/id */
	fz_obj *root;			/* resolved catalog dict */
	fz_obj *info;			/* resolved info dict */

	int len;
	int cap;
	pdf_xrefentry *table;

	struct pdf_store_s *store;
	struct pdf_outline_s *outlines;
};

struct pdf_xrefentry_s
{
	int ofs;	/* file offset / objstm object number */
	int gen;	/* generation / objstm index */
	int stmofs;	/* on-disk stream */
	fz_obj *obj;	/* stored/cached object */
	int type;	/* 0=unset (f)ree i(n)use (o)bjstm */
};

fz_error pdf_newxref(pdf_xref **);
fz_error pdf_repairxref(pdf_xref *, char *filename);
fz_error pdf_loadxref(pdf_xref *, char *filename);
#ifdef WIN32
#include <wchar.h>
fz_error pdf_loadxrefw(pdf_xref *xref, wchar_t *filename);
fz_error pdf_repairxrefw(pdf_xref *xref, wchar_t *filename);
#ifdef _UNICODE
#define pdf_loadxreft pdf_loadxrefw
#define pdf_repairxreft pdf_repairxrefw
#else
#define pdf_loadxreft pdf_loadxref
#define pdf_repairxreft pdf_repairxref
#endif
#endif
fz_error pdf_initxref(pdf_xref *);
fz_error pdf_decryptxref(pdf_xref *);

void pdf_debugxref(pdf_xref *);
void pdf_flushxref(pdf_xref *, int force);
void pdf_closexref(pdf_xref *);

fz_error pdf_cacheobject(pdf_xref *, int oid, int gen);
fz_error pdf_loadobject(fz_obj **objp, pdf_xref *, int oid, int gen);

int pdf_isstream(pdf_xref *xref, int oid, int gen);
fz_error pdf_buildinlinefilter(fz_filter **filterp, pdf_xref *xref, fz_obj *stmobj);
fz_error pdf_loadrawstream(fz_buffer **bufp, pdf_xref *xref, int oid, int gen);
fz_error pdf_loadstream(fz_buffer **bufp, pdf_xref *xref, int oid, int gen);
fz_error pdf_openrawstream(fz_stream **stmp, pdf_xref *, int oid, int gen);
fz_error pdf_openstream(fz_stream **stmp, pdf_xref *, int oid, int gen);

fz_error pdf_garbagecollect(pdf_xref *xref);
fz_error pdf_transplant(pdf_xref *dst, pdf_xref *src, fz_obj **newp, fz_obj *old);

/* private */
fz_error pdf_loadobjstm(pdf_xref *xref, int oid, int gen, char *buf, int cap);

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

fz_error pdf_newstore(pdf_store **storep);
void pdf_emptystore(pdf_store *store);
void pdf_dropstore(pdf_store *store);

void pdf_agestoreditems(pdf_store *store);
fz_error pdf_evictageditems(pdf_store *store);

fz_error pdf_storeitem(pdf_store *store, pdf_itemkind tag, fz_obj *key, void *val);
void *pdf_finditem(pdf_store *store, pdf_itemkind tag, fz_obj *key);
fz_error pdf_removeitem(pdf_store *store, pdf_itemkind tag, fz_obj *key);

fz_error pdf_loadresources(fz_obj **rdb, pdf_xref *xref, fz_obj *orig);

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

extern fz_colorspace *pdf_devicegray;
extern fz_colorspace *pdf_devicergb;
extern fz_colorspace *pdf_devicecmyk;
extern fz_colorspace *pdf_devicelab;
extern fz_colorspace *pdf_devicepattern;

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
	fz_tree *tree;
};

fz_error pdf_loadpattern(pdf_pattern **patp, pdf_xref *xref, fz_obj *obj);
pdf_pattern *pdf_keeppattern(pdf_pattern *pat);
void pdf_droppattern(pdf_pattern *pat);

/*
 * Shading
 */

void pdf_setmeshvalue(float *mesh, int i, float x, float y, float t);
fz_error pdf_loadshadefunction(fz_shade *shade, pdf_xref *xref, fz_obj *dict, float t0, float t1);
fz_error pdf_loadtype1shade(fz_shade *, pdf_xref *, fz_obj *dict);
fz_error pdf_loadtype2shade(fz_shade *, pdf_xref *, fz_obj *dict);
fz_error pdf_loadtype3shade(fz_shade *, pdf_xref *, fz_obj *dict);
fz_error pdf_loadtype4shade(fz_shade *, pdf_xref *, fz_obj *dict);
fz_error pdf_loadtype5shade(fz_shade *, pdf_xref *, fz_obj *dict);
fz_error pdf_loadtype6shade(fz_shade *, pdf_xref *, fz_obj *dict);
fz_error pdf_loadtype7shade(fz_shade *, pdf_xref *, fz_obj *dict);
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
	fz_image super;
	fz_image *mask;			/* explicit mask with subimage */
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
fz_error pdf_loadtile(fz_image *image, fz_pixmap *tile);
void pdf_dropimage(fz_image *img);

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

fz_error pdf_newcmap(pdf_cmap **cmapp);
pdf_cmap *pdf_keepcmap(pdf_cmap *cmap);
void pdf_dropcmap(pdf_cmap *cmap);

void pdf_debugcmap(pdf_cmap *cmap);
int pdf_getwmode(pdf_cmap *cmap);
pdf_cmap *pdf_getusecmap(pdf_cmap *cmap);
void pdf_setwmode(pdf_cmap *cmap, int wmode);
void pdf_setusecmap(pdf_cmap *cmap, pdf_cmap *usecmap);

fz_error pdf_addcodespace(pdf_cmap *cmap, int low, int high, int n);
fz_error pdf_maprangetotable(pdf_cmap *cmap, int low, int *map, int len);
fz_error pdf_maprangetorange(pdf_cmap *cmap, int srclo, int srchi, int dstlo);
fz_error pdf_maponetomany(pdf_cmap *cmap, int one, int *many, int len);
fz_error pdf_sortcmap(pdf_cmap *cmap);

int pdf_lookupcmap(pdf_cmap *cmap, int cpt);
unsigned char *pdf_decodecmap(pdf_cmap *cmap, unsigned char *s, int *cpt);

fz_error pdf_parsecmap(pdf_cmap **cmapp, fz_stream *file);
fz_error pdf_loadembeddedcmap(pdf_cmap **cmapp, pdf_xref *xref, fz_obj *ref);
fz_error pdf_loadsystemcmap(pdf_cmap **cmapp, char *name);
fz_error pdf_newidentitycmap(pdf_cmap **cmapp, int wmode, int bytes);

/*
 * Font
 */

void pdf_loadencoding(char **estrings, char *encoding);
int pdf_lookupagl(char *name, int *ucsbuf, int ucscap);

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
fz_error pdf_addhmtx(pdf_fontdesc *font, int lo, int hi, int w);
fz_error pdf_addvmtx(pdf_fontdesc *font, int lo, int hi, int x, int y, int w);
fz_error pdf_endhmtx(pdf_fontdesc *font);
fz_error pdf_endvmtx(pdf_fontdesc *font);
pdf_hmtx pdf_gethmtx(pdf_fontdesc *font, int cid);
pdf_vmtx pdf_getvmtx(pdf_fontdesc *font, int cid);

/* unicode.c */
fz_error pdf_loadtounicode(pdf_fontdesc *font, pdf_xref *xref, char **strings, char *collection, fz_obj *cmapstm);

/* fontfile.c */
fz_error pdf_loadbuiltinfont(pdf_fontdesc *font, char *basefont);
fz_error pdf_loadembeddedfont(pdf_fontdesc *font, pdf_xref *xref, fz_obj *stmref);
fz_error pdf_loadsystemfont(pdf_fontdesc *font, char *basefont, char *collection);
fz_error pdf_loadsubstitutefont(pdf_fontdesc *font, int fdflags, char *collection);

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
typedef struct pdf_comment_s pdf_comment;
typedef struct pdf_widget_s pdf_widget;
typedef struct pdf_outline_s pdf_outline;

typedef enum pdf_linkkind_e
{
	PDF_LGOTO = 0,
	PDF_LURI,
} pdf_linkkind;

struct pdf_link_s
{
	pdf_linkkind kind;
	fz_rect rect;
	fz_obj *dest;
	pdf_link *next;
};

typedef enum pdf_commentkind_e
{
	PDF_CTEXT,
	PDF_CFREETEXT,
	PDF_CLINE,
	PDF_CSQUARE,
	PDF_CCIRCLE,
	PDF_CPOLYGON,
	PDF_CPOLYLINE,
	PDF_CMARKUP,
	PDF_CCARET,
	PDF_CSTAMP,
	PDF_CINK
} pdf_commentkind;

struct pdf_comment_s
{
	pdf_commentkind kind;
	fz_rect rect;
	fz_rect popup;
	fz_obj *contents;
	pdf_comment *next;
};

struct pdf_outline_s
{
	char *title;
	pdf_link *link;
	pdf_outline *child;
	pdf_outline *next;
	int count;
};

fz_error pdf_loadnametree(fz_obj **dictp, pdf_xref *xref, fz_obj *root);
fz_obj *pdf_lookupdest(pdf_xref *xref, fz_obj *nameddest);

fz_error pdf_newlink(pdf_link**, pdf_linkkind kind, fz_rect rect, fz_obj *dest);
fz_error pdf_loadlink(pdf_link **linkp, pdf_xref *xref, fz_obj *dict);
void pdf_droplink(pdf_link *link);

fz_error pdf_loadoutline(pdf_outline **outlinep, pdf_xref *xref);
void pdf_debugoutline(pdf_outline *outline, int level);
void pdf_dropoutline(pdf_outline *outline);

fz_error pdf_loadannots(pdf_comment **, pdf_link **, pdf_xref *, fz_obj *annots);

/*
 * Page tree, pages and related objects
 */

typedef struct pdf_page_s pdf_page;
typedef struct pdf_textline_s pdf_textline;
typedef struct pdf_textchar_s pdf_textchar;

struct pdf_page_s
{
	fz_rect mediabox;
	int rotate;
	fz_obj *resources;
	fz_tree *tree;
	pdf_comment *comments;
	pdf_link *links;
};

struct pdf_textchar_s
{
	fz_irect bbox;
	int c;
};

struct pdf_textline_s
{
	int len, cap;
	pdf_textchar *text;
	pdf_textline *next;
};

/* pagetree.c */
fz_error pdf_getpagecount(pdf_xref *xref, int *pagesp);
fz_error pdf_getpageobject(pdf_xref *xref, int p, fz_obj **pagep);
fz_error pdf_findpageobject(pdf_xref *xref, fz_obj *pageobj, int *page);

/* page.c */
fz_error pdf_loadpage(pdf_page **pagep, pdf_xref *xref, fz_obj *ref);
void pdf_droppage(pdf_page *page);

/* unicode.c */
fz_error pdf_loadtextfromtree(pdf_textline **linep, fz_tree *tree, fz_matrix ctm);
void pdf_debugtextline(pdf_textline *line);
fz_error pdf_newtextline(pdf_textline **linep);
void pdf_droptextline(pdf_textline *line);

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
	/* path stroking */
	float linewidth;
	int linecap;
	int linejoin;
	float miterlimit;
	float dashphase;
	int dashlen;
	float dashlist[32];

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

	/* tree construction state */
	fz_node *head;
};

struct pdf_csi_s
{
	pdf_gstate gstate[32];
	int gtop;
	fz_obj *stack[32];
	int top;
	int xbalance;
	fz_obj *array;

	/* path object state */
	fz_pathnode *path;
	int clip;
	int clipevenodd;

	/* text object state */
	fz_node *textclip;
	fz_textnode *text;
	fz_matrix tlm;
	fz_matrix tm;
	int textmode;

	fz_tree *tree;
};

/* build.c */
void pdf_initgstate(pdf_gstate *gs);
fz_error pdf_setcolorspace(pdf_csi *csi, int what, fz_colorspace *cs);
fz_error pdf_setcolor(pdf_csi *csi, int what, float *v);
fz_error pdf_setpattern(pdf_csi *csi, int what, pdf_pattern *pat, float *v);
fz_error pdf_setshade(pdf_csi *csi, int what, fz_shade *shade);

fz_error pdf_buildstrokepath(pdf_gstate *gs, fz_pathnode *path);
fz_error pdf_buildfillpath(pdf_gstate *gs, fz_pathnode *path, int evenodd);
fz_error pdf_addfillshape(pdf_gstate *gs, fz_node *shape);
fz_error pdf_addstrokeshape(pdf_gstate *gs, fz_node *shape);
fz_error pdf_addclipmask(pdf_gstate *gs, fz_node *shape);
fz_error pdf_addtransform(pdf_gstate *gs, fz_node *transform);
fz_error pdf_addshade(pdf_gstate *gs, fz_shade *shade);
fz_error pdf_showpath(pdf_csi*, int close, int fill, int stroke, int evenodd);
fz_error pdf_showtext(pdf_csi*, fz_obj *text);
fz_error pdf_flushtext(pdf_csi*);
fz_error pdf_showimage(pdf_csi*, pdf_image *img);

/* interpret.c */
fz_error pdf_newcsi(pdf_csi **csip, int maskonly);
fz_error pdf_runcsi(pdf_csi *, pdf_xref *xref, fz_obj *rdb, fz_stream *);
void pdf_dropcsi(pdf_csi *csi);

#endif

