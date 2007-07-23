/*
 * Misc drawing stuff
 */

typedef struct fz_graphics_s fz_graphics;
typedef struct fz_gstate_s fz_gstate;
typedef struct fz_pixfuns_s fz_pixfuns;

struct fz_pixfuns_s
{
	void (*ainboverc)(fz_pixmap *color, fz_pixmap *shape, fz_pixmap *dest);
	void (*solidover)(unsigned char*, fz_pixmap*);
	void (*maskover)(fz_scanargs*, unsigned char*, int, int, int, int, int);
	void (*maskoverwithsolid)(fz_scanargs*, unsigned char*, int, int, int, int, int);
	void (*imageover)(fz_scanargs*, unsigned char*, int, int, int, int, int);
	void (*imageoverwithsolid)(fz_scanargs*, unsigned char*, int, int, int, int, int);
	void (*imageadd)(fz_scanargs*, unsigned char*, int, int, int, int, int);
	void (*shadeover)(fz_scanargs*, unsigned char*, int, int, int, int, int);
};

typedef unsigned char FZ_BYTE;

extern void (*fz_decodetile)(fz_pixmap *pix, int skip, float *decode);
extern void (*fz_loadtile1)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile2)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile4)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);
extern void (*fz_loadtile8)(FZ_BYTE*, int sw, FZ_BYTE*, int dw, int w, int h, int pad);

struct fz_gstate_s
{
	fz_matrix ctm;
	fz_colorspace *pcm;
	int blend;
	fz_pixmap *dest;
};

struct fz_graphics_s
{
	/* cache & edgelists */
	fz_glyphcache *cache;
	fz_gel *gel;
	fz_ael *ael;

	/* inherited graphics state */
	fz_gstate state;

	/* temporaries */
	unsigned char solid[FZ_MAXCOLORS];

	/* blit functions */
	fz_pixfuns funs;
};

fz_error *fz_drawnode(fz_graphics *gc, fz_node *node);
fz_error *fz_drawtransform(fz_graphics *gc, fz_transformnode *node);
fz_error *fz_drawblend(fz_graphics *gc, fz_blendnode *node);
fz_error *fz_drawmask(fz_graphics *gc, fz_masknode *node);
fz_error *fz_drawover(fz_graphics *gc, fz_overnode *node);
fz_error *fz_drawsolid(fz_graphics *gc, fz_solidnode *node);
fz_error *fz_drawpath(fz_graphics *gc, fz_pathnode *node);
fz_error *fz_drawtext(fz_graphics *gc, fz_textnode *node);
fz_error *fz_drawimage(fz_graphics *gc, fz_imagenode *node);
fz_error *fz_drawshade(fz_graphics *gc, fz_shadenode *node);

fz_error *fz_newgraphics(fz_graphics **gcp, int gcmem);
void fz_dropgraphics(fz_graphics *gc);

fz_error *fz_drawtree(fz_pixmap **out, fz_graphics *gc, fz_tree *tree, fz_matrix ctm, fz_colorspace *pcm, fz_irect bbox, int white);
fz_error *fz_drawtreeover(fz_pixmap *out, fz_graphics *gc, fz_tree *tree, fz_matrix ctm, fz_colorspace *pcm);

