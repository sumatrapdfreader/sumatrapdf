typedef struct fz_renderer_s fz_renderer;

#define FZ_BYTE unsigned char

#define FZ_PSRC \
	unsigned char *src, int srcw, int srch
#define FZ_PDST \
	unsigned char *dst0, int dstw
#define FZ_PCTM \
	int u0, int v0, int fa, int fb, int fc, int fd, int w0, int h

typedef struct fz_glyph_s fz_glyph;
typedef struct fz_glyphcache_s fz_glyphcache;

fz_error fz_renderftglyph(fz_glyph *glyph, fz_font *font, int cid, fz_matrix trm);
fz_error fz_rendert3glyph(fz_glyph *glyph, fz_font *font, int cid, fz_matrix trm);
fz_error fz_newglyphcache(fz_glyphcache **arenap, int slots, int size);
fz_error fz_renderglyph(fz_glyphcache*, fz_glyph*, fz_font*, int, fz_matrix);
void fz_debugglyphcache(fz_glyphcache *);
void fz_dropglyphcache(fz_glyphcache *);

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
	fz_irect clip;
	fz_irect bbox;
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

fz_error fz_newgel(fz_gel **gelp);
fz_error fz_insertgel(fz_gel *gel, float x0, float y0, float x1, float y1);
fz_irect fz_boundgel(fz_gel *gel);
void fz_resetgel(fz_gel *gel, fz_irect clip);
void fz_sortgel(fz_gel *gel);
void fz_dropgel(fz_gel *gel);

fz_error fz_newael(fz_ael **aelp);
void fz_dropael(fz_ael *ael);

fz_error fz_scanconvert(fz_gel *gel, fz_ael *ael, int eofill,
	fz_irect clip, fz_pixmap *pix, unsigned char *argb, int over);

fz_error fz_fillpath(fz_gel *gel, fz_pathnode *path, fz_matrix ctm, float flatness);
fz_error fz_strokepath(fz_gel *gel, fz_pathnode *path, fz_matrix ctm, float flatness, float linewidth);
fz_error fz_dashpath(fz_gel *gel, fz_pathnode *path, fz_matrix ctm, float flatness, float linewidth);


/*
 * Function pointers -- they can be replaced by cpu-optimized versions
 */

extern void (*fz_duff_non)(FZ_BYTE*,int,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_nimcn)(FZ_BYTE*,int,int,FZ_BYTE*,int,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_nimon)(FZ_BYTE*,int,int,FZ_BYTE*,int,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_1o1)(FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_4o4)(FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_1i1c1)(FZ_BYTE*,int,FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_4i1c4)(FZ_BYTE*,int,FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_1i1o1)(FZ_BYTE*,int,FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_duff_4i1o4)(FZ_BYTE*,int,FZ_BYTE*,int,FZ_BYTE*,int,int,int);

extern void (*fz_path_1c1)(FZ_BYTE*,unsigned char,int,FZ_BYTE*);
extern void (*fz_path_1o1)(FZ_BYTE*,unsigned char,int,FZ_BYTE*);
extern void (*fz_path_w4i1o4)(FZ_BYTE*,FZ_BYTE*,unsigned char,int,FZ_BYTE*);

extern void (*fz_text_1c1)(FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_text_1o1)(FZ_BYTE*,int,FZ_BYTE*,int,int,int);
extern void (*fz_text_w4i1o4)(FZ_BYTE*,FZ_BYTE*,int,FZ_BYTE*,int,int,int);

extern void (*fz_img_ncn)(FZ_PSRC, int sn, FZ_PDST, FZ_PCTM);
extern void (*fz_img_1c1)(FZ_PSRC, FZ_PDST, FZ_PCTM);
extern void (*fz_img_4c4)(FZ_PSRC, FZ_PDST, FZ_PCTM);
extern void (*fz_img_1o1)(FZ_PSRC, FZ_PDST, FZ_PCTM);
extern void (*fz_img_4o4)(FZ_PSRC, FZ_PDST, FZ_PCTM);
extern void (*fz_img_w4i1o4)(FZ_BYTE*,FZ_PSRC,FZ_PDST,FZ_PCTM);

extern void (*fz_decodetile)(fz_pixmap *pix, int skip, float *decode);
extern void (*fz_loadtile1)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile2)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile4)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile8)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile16)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);

extern void (*fz_srown)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom, int n);
extern void (*fz_srow1)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);
extern void (*fz_srow2)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);
extern void (*fz_srow4)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);
extern void (*fz_srow5)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);

extern void (*fz_scoln)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom, int n);
extern void (*fz_scol1)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);
extern void (*fz_scol2)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);
extern void (*fz_scol4)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);
extern void (*fz_scol5)(FZ_BYTE *src, FZ_BYTE *dst, int w, int denom);

#undef FZ_BYTE

struct fz_renderer_s
{
	int maskonly;
	fz_colorspace *model;
	fz_glyphcache *cache;
	fz_gel *gel;
	fz_ael *ael;

	fz_irect clip;
	fz_pixmap *dest;
	fz_pixmap *over;
	unsigned char argb[7]; /* alpha, a*r, a*g, a*b, r, g, b */
	int flag;
};

extern void fz_accelerate(void);

fz_error fz_newrenderer(fz_renderer **gcp, fz_colorspace *pcm, int maskonly, int gcmem);
void fz_droprenderer(fz_renderer *gc);
fz_error fz_rendertree(fz_pixmap **out, fz_renderer *gc, fz_tree *tree, fz_matrix ctm, fz_irect bbox, int white);
fz_error fz_rendertreeover(fz_renderer *gc, fz_pixmap *dest, fz_tree *tree, fz_matrix ctm);


