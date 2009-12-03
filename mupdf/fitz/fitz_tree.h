/*
 * The display tree is at the center of attention in Fitz.
 * The tree and most of its minor nodes.
 * Paths and text nodes are found elsewhere.
 */

typedef struct fz_tree_s fz_tree;
typedef struct fz_node_s fz_node;

typedef struct fz_colorspace_s fz_colorspace;
typedef struct fz_image_s fz_image;
typedef struct fz_shade_s fz_shade;
typedef struct fz_font_s fz_font;

struct fz_tree_s
{
	int refs;
	fz_node *root;
	fz_node *head;
};

/* tree operations */
fz_error fz_newtree(fz_tree **treep);
fz_tree *fz_keeptree(fz_tree *tree);
void fz_droptree(fz_tree *tree);

fz_rect fz_boundtree(fz_tree *tree, fz_matrix ctm);
void fz_debugtree(fz_tree *tree);
void fz_insertnodefirst(fz_node *parent, fz_node *child);
void fz_insertnodelast(fz_node *parent, fz_node *child);
void fz_insertnodeafter(fz_node *prev, fz_node *child);
void fz_removenode(fz_node *child);

/* node types */

typedef struct fz_transformnode_s fz_transformnode;
typedef struct fz_overnode_s fz_overnode;
typedef struct fz_masknode_s fz_masknode;
typedef struct fz_blendnode_s fz_blendnode;
typedef struct fz_pathnode_s fz_pathnode;
typedef struct fz_textnode_s fz_textnode;
typedef struct fz_solidnode_s fz_solidnode;
typedef struct fz_imagenode_s fz_imagenode;
typedef struct fz_shadenode_s fz_shadenode;
typedef struct fz_linknode_s fz_linknode;

typedef enum fz_nodekind_e
{
	FZ_NTRANSFORM,
	FZ_NOVER,
	FZ_NMASK,
	FZ_NBLEND,
	FZ_NPATH,
	FZ_NTEXT,
	FZ_NCOLOR,
	FZ_NIMAGE,
	FZ_NSHADE,
	FZ_NLINK,
} fz_nodekind;

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
	FZ_BLUMINOSITY,

	FZ_BOVERPRINT,
	FZ_BRASTEROP
} fz_blendkind;

struct fz_node_s
{
	fz_nodekind kind;
	fz_node *parent;
	fz_node *first;
	fz_node *last;
	fz_node *next;
};

struct fz_transformnode_s
{
	fz_node super;
	fz_matrix m;
};

struct fz_overnode_s
{
	fz_node super;
};

struct fz_masknode_s
{
	fz_node super;
};

struct fz_blendnode_s
{
	fz_node super;
	fz_colorspace *cs;
	fz_blendkind mode;
	int isolated;
	int knockout;
};

struct fz_solidnode_s
{
	fz_node super;
	fz_colorspace *cs;
	int n;
	float a;
	float samples[FZ_FLEX];
};

struct fz_linknode_s
{
	fz_node super;
	fz_tree *tree;
};

struct fz_imagenode_s
{
	fz_node super;
	fz_image *image;
};

struct fz_shadenode_s
{
	fz_node super;
	fz_shade *shade;
};

/* common to all nodes */
void fz_initnode(fz_node *node, fz_nodekind kind);
fz_rect fz_boundnode(fz_node *node, fz_matrix ctm);
void fz_dropnode(fz_node *node);

/* branch nodes */
fz_error fz_newovernode(fz_node **nodep);
fz_error fz_newmasknode(fz_node **nodep);
fz_error fz_newblendnode(fz_node **nodep, fz_blendkind b, int i, int k);
fz_error fz_newtransformnode(fz_node **nodep, fz_matrix m);

int fz_istransformnode(fz_node *node);
int fz_isovernode(fz_node *node);
int fz_ismasknode(fz_node *node);
int fz_isblendnode(fz_node *node);

/* leaf nodes */
fz_error fz_newlinknode(fz_node **nodep, fz_tree *subtree);
fz_error fz_newsolidnode(fz_node **nodep, float a, fz_colorspace *cs, int n, float *v);
fz_error fz_newimagenode(fz_node **nodep, fz_image *image);
fz_error fz_newshadenode(fz_node **nodep, fz_shade *shade);

int fz_islinknode(fz_node *node);
int fz_issolidnode(fz_node *node);
int fz_ispathnode(fz_node *node);
int fz_istextnode(fz_node *node);
int fz_isimagenode(fz_node *node);
int fz_isshadenode(fz_node *node);

/*
 * Vector path nodes in the display tree.
 * They can be stroked and dashed, or be filled.
 * They have a fill rule (nonzero or evenodd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 *
 * TODO flatten, stroke and dash into another path
 * TODO set operations on flat paths (union, intersect, difference)
 * TODO decide whether dashing should be part of the tree and renderer,
 *      or if it is something the client has to do (with a util function).
 */

typedef struct fz_stroke_s fz_stroke;
typedef struct fz_dash_s fz_dash;
typedef union fz_pathel_s fz_pathel;

typedef enum fz_pathkind_e
{
	FZ_STROKE,
	FZ_FILL,
	FZ_EOFILL
} fz_pathkind;

typedef enum fz_pathelkind_e
{
	FZ_MOVETO,
	FZ_LINETO,
	FZ_CURVETO,
	FZ_CLOSEPATH
} fz_pathelkind;

struct fz_stroke_s
{
	int linecap;
	int linejoin;
	float linewidth;
	float miterlimit;
};

struct fz_dash_s
{
	int len;
	float phase;
	float array[FZ_FLEX];
};

union fz_pathel_s
{
	fz_pathelkind k;
	float v;
};

struct fz_pathnode_s
{
	fz_node super;
	fz_pathkind paint;
	fz_dash *dash;
	int linecap;
	int linejoin;
	float linewidth;
	float miterlimit;
	int len, cap;
	fz_pathel *els;
};

fz_error fz_newpathnode(fz_pathnode **pathp);
fz_error fz_clonepathnode(fz_pathnode **pathp, fz_pathnode *oldpath);
fz_error fz_moveto(fz_pathnode*, float x, float y);
fz_error fz_lineto(fz_pathnode*, float x, float y);
fz_error fz_curveto(fz_pathnode*, float, float, float, float, float, float);
fz_error fz_curvetov(fz_pathnode*, float, float, float, float);
fz_error fz_curvetoy(fz_pathnode*, float, float, float, float);
fz_error fz_closepath(fz_pathnode*);
fz_error fz_endpath(fz_pathnode*, fz_pathkind paint, fz_stroke *stroke, fz_dash *dash);

fz_rect fz_boundpathnode(fz_pathnode *node, fz_matrix ctm);
void fz_debugpathnode(fz_pathnode *node, int indent);
void fz_printpathnode(fz_pathnode *node, int indent);

fz_error fz_newdash(fz_dash **dashp, float phase, int len, float *array);
void fz_dropdash(fz_dash *dash);

/*
 * Fitz display tree text node.
 *
 * The text node is an optimization to reference glyphs in a font resource
 * and specifying an individual transform matrix for each one.
 *
 * The trm field contains the a, b, c and d coefficients.
 * The e and f coefficients come from the individual elements,
 * together they form the transform matrix for the glyph.
 *
 * Glyphs are referenced by glyph ID.
 * The Unicode text equivalent is kept in a separate array
 * with indexes into the glyph array.
 *

TODO the unicode textels

struct fz_textgid_s { float e, f; int gid; };
struct fz_textucs_s { int idx; int ucs; };

*/

typedef struct fz_textel_s fz_textel;

struct fz_textel_s
{
	float x, y;
	int gid;
	int ucs;
};

struct fz_textnode_s
{
	fz_node super;
	fz_font *font;
	fz_matrix trm;
	int len, cap;
	fz_textel *els;
};

fz_error fz_newtextnode(fz_textnode **textp, fz_font *face);
fz_error fz_clonetextnode(fz_textnode **textp, fz_textnode *oldtext);
fz_error fz_addtext(fz_textnode *text, int gid, int ucs, float x, float y);
fz_error fz_endtext(fz_textnode *text);

typedef struct fz_colorcube_s fz_colorcube;
typedef struct fz_colorcube1_s fz_colorcube1;
typedef struct fz_colorcube3_s fz_colorcube3;
typedef struct fz_colorcube4_s fz_colorcube4;

enum { FZ_MAXCOLORS = 32 };

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

struct fz_colorcube1_s { unsigned char v[17]; };
struct fz_colorcube3_s { unsigned char v[17][17][17]; };
struct fz_colorcube4_s { unsigned char v[17][17][17][17]; };

struct fz_colorcube_s
{
	fz_colorspace *src;
	fz_colorspace *dst;
	void **subcube;			/* dst->n * colorcube(src->n) */
};

fz_colorspace *fz_keepcolorspace(fz_colorspace *cs);
void fz_dropcolorspace(fz_colorspace *cs);

void fz_convertcolor(fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv);
void fz_convertpixmap(fz_colorspace *srcs, fz_pixmap *srcv, fz_colorspace *dsts, fz_pixmap *dstv);

void fz_stdconvcolor(fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv);
void fz_stdconvpixmap(fz_colorspace *srcs, fz_pixmap *srcv, fz_colorspace *dsts, fz_pixmap *dstv);

char *ft_errorstring(int err);

struct fz_font_s
{
	int refs;
	char name[32];

	void *ftface; /* has an FT_Face if used */
	int ftsubstitute; /* ... substitute metrics */
	int fthint; /* ... force hinting for DynaLab fonts */

	fz_matrix t3matrix;
	struct fz_tree_s **t3procs; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */

	fz_irect bbox;
};

struct fz_glyph_s
{
	int x, y, w, h;
	unsigned char *samples;
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
void fz_setfontbbox(fz_font *font, int xmin, int ymin, int xmax, int ymax);

/* loadtile will fill a pixmap with the pixel samples. non-premultiplied alpha. */

struct fz_image_s
{
	int refs;
	fz_error (*loadtile)(fz_image*,fz_pixmap*);
	void (*freefunc)(fz_image*);
	fz_colorspace *cs;
	int w, h, n, a;
};

fz_image *fz_keepimage(fz_image *img);
void fz_dropimage(fz_image *img);

struct fz_shade_s
{
	int refs;

	fz_rect bbox;		/* can be fz_infiniterect */
	fz_colorspace *cs;

	/* used by build.c -- not used in drawshade.c */
	fz_matrix matrix;	/* matrix from pattern dict */
	int usebackground;	/* background color for fills but not 'sh' */
	float background[FZ_MAXCOLORS];

	int usefunction;
	float function[256][FZ_MAXCOLORS];

	int meshlen;
	int meshcap;
	float *mesh; /* [x y t] or [x y c1 ... cn] * 3 * meshlen */
};


fz_shade *fz_keepshade(fz_shade *shade);
void fz_dropshade(fz_shade *shade);

fz_rect fz_boundshade(fz_shade *shade, fz_matrix ctm);
fz_error fz_rendershade(fz_shade *shade, fz_matrix ctm, fz_colorspace *dsts, fz_pixmap *dstp);

