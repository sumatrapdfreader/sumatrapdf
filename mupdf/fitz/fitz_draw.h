/*
 * Resources and other graphics related objects.
 */

typedef struct fz_pixmap_s fz_pixmap;
typedef struct fz_colorspace_s fz_colorspace;
typedef struct fz_path_s fz_path;
typedef struct fz_text_s fz_text;
typedef struct fz_font_s fz_font;
typedef struct fz_shade_s fz_shade;
typedef struct fz_glyphcache_s fz_glyphcache;

typedef struct fz_strokestate_s fz_strokestate;

enum { FZ_MAXCOLORS = 32 };

typedef enum fz_blendkind_e
{
	/* PDF 1.4 -- standard separable */
	FZ_BNORMAL,
	FZ_BMULTIPLY,
	FZ_BSCREEN,
	FZ_BOVERLAY,
	FZ_BDARKEN,
	FZ_BLIGHTEN,
	FZ_BCOLORDODGE,
	FZ_BCOLORBURN,
	FZ_BHARDLIGHT,
	FZ_BSOFTLIGHT,
	FZ_BDIFFERENCE,
	FZ_BEXCLUSION,

	/* PDF 1.4 -- standard non-separable */
	FZ_BHUE,
	FZ_BSATURATION,
	FZ_BCOLOR,
	FZ_BLUMINOSITY
} fz_blendkind;

/*
pixmaps have n components per pixel. the last is always alpha.
premultiplied alpha when rendering, but non-premultiplied for colorspace
conversions and rescaling.
*/

extern fz_colorspace *pdf_devicegray;
extern fz_colorspace *pdf_devicergb;
extern fz_colorspace *pdf_devicebgr;
extern fz_colorspace *pdf_devicecmyk;
extern fz_colorspace *pdf_devicelab;
extern fz_colorspace *pdf_devicepattern;

struct fz_pixmap_s
{
	int refs;
	int x, y, w, h, n;
	fz_colorspace *colorspace;
	unsigned char *samples;
};

fz_pixmap * fz_newpixmapwithrect(fz_colorspace *, fz_bbox bbox);
fz_pixmap * fz_newpixmap(fz_colorspace *, int x, int y, int w, int h);
fz_pixmap *fz_keeppixmap(fz_pixmap *map);
void fz_droppixmap(fz_pixmap *map);

void fz_debugpixmap(fz_pixmap *map, char *prefix);
void fz_clearpixmap(fz_pixmap *map, unsigned char value);

fz_pixmap * fz_scalepixmap(fz_pixmap *src, int xdenom, int ydenom);

/*
 * The device interface.
 */

enum
{
	FZ_IGNOREIMAGE = 1,
	FZ_IGNORESHADE = 2,
};

typedef struct fz_device_s fz_device;

struct fz_device_s
{
	int hints;

	void *user;
	void (*freeuser)(void *);

	void (*fillpath)(void *, fz_path *, int evenodd, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*strokepath)(void *, fz_path *, fz_strokestate *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clippath)(void *, fz_path *, int evenodd, fz_matrix);
	void (*clipstrokepath)(void *, fz_path *, fz_strokestate *, fz_matrix);

	void (*filltext)(void *, fz_text *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroketext)(void *, fz_text *, fz_strokestate *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*cliptext)(void *, fz_text *, fz_matrix, int accumulate);
	void (*clipstroketext)(void *, fz_text *, fz_strokestate *, fz_matrix);
	void (*ignoretext)(void *, fz_text *, fz_matrix);

	void (*fillshade)(void *, fz_shade *shd, fz_matrix ctm);
	void (*fillimage)(void *, fz_pixmap *img, fz_matrix ctm);
	void (*fillimagemask)(void *, fz_pixmap *img, fz_matrix ctm, fz_colorspace *, float *color, float alpha);
	void (*clipimagemask)(void *, fz_pixmap *img, fz_matrix ctm);

	void (*popclip)(void *);
};

fz_device *fz_newdevice(void *user);
void fz_freedevice(fz_device *dev);

fz_device *fz_newtracedevice(void);

fz_device *fz_newbboxdevice(fz_bbox *bboxp);

fz_device *fz_newdrawdevice(fz_glyphcache *cache, fz_pixmap *dest);

/* Text extraction device */

typedef struct fz_textspan_s fz_textspan;
typedef struct fz_textchar_s fz_textchar;

struct fz_textchar_s
{
	int c;
	fz_bbox bbox;
};

struct fz_textspan_s
{
	fz_font *font;
	float size;
	int wmode;
	int len, cap;
	fz_textchar *text;
	fz_textspan *next;
	int eol;
};

fz_textspan * fz_newtextspan(void);
void fz_freetextspan(fz_textspan *line);
void fz_debugtextspan(fz_textspan *line);
void fz_debugtextspanxml(fz_textspan *span);

fz_device *fz_newtextdevice(fz_textspan *text);

/* Display list device -- record and play back device commands. */

typedef struct fz_displaylist_s fz_displaylist;
typedef struct fz_displaynode_s fz_displaynode;

typedef enum fz_displaycommand_e
{
	FZ_CMDFILLPATH,
	FZ_CMDSTROKEPATH,
	FZ_CMDCLIPPATH,
	FZ_CMDCLIPSTROKEPATH,
	FZ_CMDFILLTEXT,
	FZ_CMDSTROKETEXT,
	FZ_CMDCLIPTEXT,
	FZ_CMDCLIPSTROKETEXT,
	FZ_CMDIGNORETEXT,
	FZ_CMDFILLSHADE,
	FZ_CMDFILLIMAGE,
	FZ_CMDFILLIMAGEMASK,
	FZ_CMDCLIPIMAGEMASK,
	FZ_CMDPOPCLIP,
} fz_displaycommand;

struct fz_displaylist_s
{
	fz_displaynode *first;
	fz_displaynode *last;
};

struct fz_displaynode_s
{
	fz_displaycommand cmd;
	fz_displaynode *next;
	union {
		fz_path *path;
		fz_text *text;
		fz_shade *shade;
		fz_pixmap *image;
	} item;
	fz_strokestate *stroke;
	int flag; /* evenodd, accumulate, ... */
	fz_matrix ctm;
	fz_colorspace *colorspace;
	float alpha;
	float color[FZ_MAXCOLORS];
};

fz_displaylist *fz_newdisplaylist(void);
void fz_freedisplaylist(fz_displaylist *list);
fz_device *fz_newlistdevice(fz_displaylist *list);
void fz_executedisplaylist(fz_displaylist *list, fz_device *dev, fz_matrix ctm);

/*
 * Vector path buffer.
 * It can be stroked and dashed, or be filled.
 * It has a fill rule (nonzero or evenodd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 */

typedef union fz_pathel_s fz_pathel;

typedef enum fz_pathelkind_e
{
	FZ_MOVETO,
	FZ_LINETO,
	FZ_CURVETO,
	FZ_CLOSEPATH
} fz_pathelkind;

union fz_pathel_s
{
	fz_pathelkind k;
	float v;
};

struct fz_strokestate_s
{
	int linecap;
	int linejoin;
	float linewidth;
	float miterlimit;
	float dashphase;
	int dashlen;
	float dashlist[32];
};

struct fz_path_s
{
	int len, cap;
	fz_pathel *els;
};

fz_path *fz_newpath(void);
void fz_moveto(fz_path*, float x, float y);
void fz_lineto(fz_path*, float x, float y);
void fz_curveto(fz_path*, float, float, float, float, float, float);
void fz_curvetov(fz_path*, float, float, float, float);
void fz_curvetoy(fz_path*, float, float, float, float);
void fz_closepath(fz_path*);
void fz_freepath(fz_path *path);

fz_path *fz_clonepath(fz_path *old);

fz_rect fz_boundpath(fz_path *path, fz_strokestate *stroke, fz_matrix ctm);
void fz_debugpath(fz_path *, int indent);

/*
 * Text buffer.
 *
 * The trm field contains the a, b, c and d coefficients.
 * The e and f coefficients come from the individual elements,
 * together they form the transform matrix for the glyph.
 *
 * Glyphs are referenced by glyph ID.
 * The Unicode text equivalent is kept in a separate array
 * with indexes into the glyph array.
 */

typedef struct fz_textel_s fz_textel;

struct fz_textel_s
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings */
	int ucs; /* -1 for one ucs to many gid mappings */
};

struct fz_text_s
{
	fz_font *font;
	fz_matrix trm;
	int wmode;
	int len, cap;
	fz_textel *els;
};

fz_text * fz_newtext(fz_font *face, fz_matrix trm, int wmode);
void fz_addtext(fz_text *text, int gid, int ucs, float x, float y);
void fz_endtext(fz_text *text);
void fz_freetext(fz_text *text);
void fz_debugtext(fz_text*, int indent);
fz_rect fz_boundtext(fz_text *text, fz_matrix ctm);
fz_text *fz_clonetext(fz_text *old);

/*
 * Colorspace resources.
 *
 * TODO: use lcms
 */

struct fz_colorspace_s
{
	int refs;
	char name[16];
	int n;
	void (*convpixmap)(fz_colorspace *ss, fz_pixmap *sp, fz_colorspace *ds, fz_pixmap *dp);
	void (*convcolor)(fz_colorspace *ss, float *sv, fz_colorspace *ds, float *dv);
	void (*toxyz)(fz_colorspace *, float *src, float *xyz);
	void (*fromxyz)(fz_colorspace *, float *xyz, float *dst);
	void (*freefunc)(fz_colorspace *);
};

fz_colorspace *fz_keepcolorspace(fz_colorspace *cs);
void fz_dropcolorspace(fz_colorspace *cs);

void fz_convertcolor(fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv);
void fz_convertpixmap(fz_colorspace *srcs, fz_pixmap *srcv, fz_colorspace *dsts, fz_pixmap *dstv);

void fz_stdconvcolor(fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv);
void fz_stdconvpixmap(fz_colorspace *srcs, fz_pixmap *srcv, fz_colorspace *dsts, fz_pixmap *dstv);

/*
 * Fonts.
 *
 * Fonts come in three variants:
 *	Regular fonts are handled by FreeType.
 *	Type 3 fonts have callbacks to the interpreter.
 *	Substitute fonts are a thin wrapper over a regular font that adjusts metrics.
 */

char *ft_errorstring(int err);

struct fz_font_s
{
	int refs;
	char name[32];

	void *ftface; /* has an FT_Face if used */
	int ftsubstitute; /* ... substitute metrics */
	int fthint; /* ... force hinting for DynaLab fonts */

	fz_matrix t3matrix;
	fz_obj *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	void *t3xref; /* a pdf_xref for the callback */
	fz_error (*t3runcontentstream)(fz_device *dev, fz_matrix ctm,
		struct pdf_xref_s *xref, fz_obj *resources, fz_buffer *contents);

	fz_rect bbox;

	/* substitute metrics */
	int widthcount;
	int *widthtable;
};

fz_error fz_newfreetypefont(fz_font **fontp, char *name, int substitute);
fz_error fz_loadfreetypefontfile(fz_font *font, char *path, int index);
fz_error fz_loadfreetypefontbuffer(fz_font *font, unsigned char *data, int len, int index);
fz_font * fz_newtype3font(char *name, fz_matrix matrix);

fz_error fz_newfontfrombuffer(fz_font **fontp, unsigned char *data, int len, int index);
fz_error fz_newfontfromfile(fz_font **fontp, char *path, int index);

fz_font * fz_keepfont(fz_font *font);
void fz_dropfont(fz_font *font);

void fz_debugfont(fz_font *font);
void fz_setfontbbox(fz_font *font, float xmin, float ymin, float xmax, float ymax);

/*
 * The shading code is in rough shape but the general architecture is sound.
 */

struct fz_shade_s
{
	int refs;

	fz_rect bbox;		/* can be fz_infiniterect */
	fz_colorspace *cs;

	fz_matrix matrix;	/* matrix from pattern dict */
	int usebackground;	/* background color for fills but not 'sh' */
	float background[FZ_MAXCOLORS];

	int usefunction;
	float function[256][FZ_MAXCOLORS];

	int meshlen;
	int meshcap;
	float *mesh; /* [x y t] or [x y c1 ... cn] */
};

fz_shade *fz_keepshade(fz_shade *shade);
void fz_dropshade(fz_shade *shade);
void fz_debugshade(fz_shade *shade);

fz_rect fz_boundshade(fz_shade *shade, fz_matrix ctm);
void fz_rendershade(fz_shade *shade, fz_matrix ctm, fz_pixmap *dst, fz_bbox bbox);

/*
 * Glyph cache
 */

fz_glyphcache * fz_newglyphcache(void);
fz_pixmap * fz_renderftglyph(fz_font *font, int cid, fz_matrix trm);
fz_pixmap * fz_rendert3glyph(fz_font *font, int cid, fz_matrix trm);
fz_pixmap * fz_renderglyph(fz_glyphcache*, fz_font*, int, fz_matrix);
void fz_evictglyphcache(fz_glyphcache *);
void fz_freeglyphcache(fz_glyphcache *);

/*
 * Scan converter
 */

typedef struct fz_edge_s fz_edge;
typedef struct fz_gel_s fz_gel;
typedef struct fz_ael_s fz_ael;

struct fz_edge_s
{
	int x, e, h, y;
	int adjup, adjdown;
	int xmove;
	int xdir, ydir;     /* -1 or +1 */
};

struct fz_gel_s
{
	fz_bbox clip;
	fz_bbox bbox;
	int cap;
	int len;
	fz_edge *edges;
};

struct fz_ael_s
{
	int cap;
	int len;
	fz_edge **edges;
};

fz_gel * fz_newgel(void);
void fz_insertgel(fz_gel *gel, float x0, float y0, float x1, float y1);
fz_bbox fz_boundgel(fz_gel *gel);
void fz_resetgel(fz_gel *gel, fz_bbox clip);
void fz_sortgel(fz_gel *gel);
void fz_freegel(fz_gel *gel);
int fz_isrectgel(fz_gel *gel);

fz_ael * fz_newael(void);
void fz_freeael(fz_ael *ael);

fz_error fz_scanconvert(fz_gel *gel, fz_ael *ael, int eofill,
	fz_bbox clip, fz_pixmap *pix, unsigned char *colorbv, fz_pixmap *image, fz_matrix *invmat);

void fz_fillpath(fz_gel *gel, fz_path *path, fz_matrix ctm, float flatness);
void fz_strokepath(fz_gel *gel, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, float flatness, float linewidth);
void fz_dashpath(fz_gel *gel, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, float flatness, float linewidth);

/*
 * Macros used to do blending
 */

/* Expand a value A from the 0...255 range to the 0..256 range */
#define FZ_EXPAND(A) ((A)+((A)>>7))

/* Combine values A (in any range) and B (in the 0..256 range),
 * to give a single value in the same range as A was. */
#define FZ_COMBINE(A,B) (((A)*(B))>>8)

/* Blend SRC and DST (in the same range) together according to
 * AMOUNT (in the 0...256 range). */
#define FZ_BLEND(SRC, DST, AMOUNT) ((((SRC)-(DST))*(AMOUNT) + ((DST)<<8))>>8)

/*
 * Function pointers -- they can be replaced by cpu-optimized versions
 */

extern void fz_accelerate(void);
extern void fz_acceleratearch(void);

extern void (*fz_duff_ni1on)(unsigned char*,int,int,unsigned char*,int,unsigned char*,int,int,int);
extern void (*fz_duff_1i1o1)(unsigned char*,int,unsigned char*,int,unsigned char*,int,int,int);
extern void (*fz_duff_2i1o2)(unsigned char*,int,unsigned char*,int,unsigned char*,int,int,int);
extern void (*fz_duff_4i1o4)(unsigned char*,int,unsigned char*,int,unsigned char*,int,int,int);

extern void (*fz_path_1o1)(unsigned char*,unsigned char,int,unsigned char*);
extern void (*fz_path_w2i1o2)(unsigned char*,unsigned char*,unsigned char,int,unsigned char*);
extern void (*fz_path_w4i1o4)(unsigned char*,unsigned char*,unsigned char,int,unsigned char*);

extern void (*fz_text_1o1)(unsigned char*,int,unsigned char*,int,int,int);
extern void (*fz_text_w2i1o2)(unsigned char*,unsigned char*,int,unsigned char*,int,int,int);
extern void (*fz_text_w4i1o4)(unsigned char*,unsigned char*,int,unsigned char*,int,int,int);

extern void (*fz_img_non)(unsigned char*,unsigned char,int,unsigned char*,fz_pixmap*,fz_matrix*);
extern void (*fz_img_1o1)(unsigned char*,unsigned char,int,unsigned char*,fz_pixmap*,int u, int v, int fa, int fb);
extern void (*fz_img_4o4)(unsigned char*,unsigned char,int,unsigned char*,fz_pixmap*,int u, int v, int fa, int fb);
extern void (*fz_img_2o2)(unsigned char*,unsigned char,int,unsigned char*,fz_pixmap*,int u, int v, int fa, int fb);
extern void (*fz_img_w2i1o2)(unsigned char*,unsigned char*,unsigned char,int,unsigned char*,fz_pixmap*,int u, int v, int fa, int fb);
extern void (*fz_img_w4i1o4)(unsigned char*,unsigned char*,unsigned char,int,unsigned char*,fz_pixmap*,int u, int v, int fa, int fb);

extern void (*fz_decodetile)(fz_pixmap *pix, int skip, float *decode);
extern void (*fz_loadtile1)(unsigned char*, int sw, unsigned char*, int dw, int w, int h, int pad);
extern void (*fz_loadtile2)(unsigned char*, int sw, unsigned char*, int dw, int w, int h, int pad);
extern void (*fz_loadtile4)(unsigned char*, int sw, unsigned char*, int dw, int w, int h, int pad);
extern void (*fz_loadtile8)(unsigned char*, int sw, unsigned char*, int dw, int w, int h, int pad);
extern void (*fz_loadtile16)(unsigned char*, int sw, unsigned char*, int dw, int w, int h, int pad);

extern void (*fz_srown)(unsigned char *src, unsigned char *dst, int w, int denom, int n);
extern void (*fz_srow1)(unsigned char *src, unsigned char *dst, int w, int denom);
extern void (*fz_srow2)(unsigned char *src, unsigned char *dst, int w, int denom);
extern void (*fz_srow4)(unsigned char *src, unsigned char *dst, int w, int denom);
extern void (*fz_srow5)(unsigned char *src, unsigned char *dst, int w, int denom);

extern void (*fz_scoln)(unsigned char *src, unsigned char *dst, int w, int denom, int n);
extern void (*fz_scol1)(unsigned char *src, unsigned char *dst, int w, int denom);
extern void (*fz_scol2)(unsigned char *src, unsigned char *dst, int w, int denom);
extern void (*fz_scol4)(unsigned char *src, unsigned char *dst, int w, int denom);
extern void (*fz_scol5)(unsigned char *src, unsigned char *dst, int w, int denom);
