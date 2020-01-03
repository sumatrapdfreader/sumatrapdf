#ifndef MUPDF_DRAW_IMP_H
#define MUPDF_DRAW_IMP_H

#define BBOX_MIN -(1<<20)
#define BBOX_MAX (1<<20)

/* divide and floor towards -inf */
static inline int fz_idiv(int a, int b)
{
	return a < 0 ? (a - b + 1) / b : a / b;
}

/* divide and ceil towards inf */
static inline int fz_idiv_up(int a, int b)
{
	return a < 0 ? a / b : (a + b - 1) / b;
}

#ifdef AA_BITS

#define fz_aa_scale 0
#define fz_rasterizer_aa_scale(ras) 0

#if AA_BITS > 6
#define AA_SCALE(s, x) (x)
#define fz_aa_hscale 17
#define fz_aa_vscale 15
#define fz_aa_bits 8
#define fz_aa_text_bits 8
#define fz_rasterizer_aa_hscale(ras) 17
#define fz_rasterizer_aa_vscale(ras) 15
#define fz_rasterizer_aa_bits(ras) 8
#define fz_rasterizer_aa_text_bits(ras) 8

#elif AA_BITS > 4
#define AA_SCALE(s, x) ((x * 255) >> 6)
#define fz_aa_hscale 8
#define fz_aa_vscale 8
#define fz_aa_bits 6
#define fz_aa_text_bits 6
#define fz_rasterizer_aa_hscale(ras) 8
#define fz_rasterizer_aa_vscale(ras) 8
#define fz_rasterizer_aa_bits(ras) 6
#define fz_rasterizer_aa_text_bits(ras) 6

#elif AA_BITS > 2
#define AA_SCALE(s, x) (x * 17)
#define fz_aa_hscale 5
#define fz_aa_vscale 3
#define fz_aa_bits 4
#define fz_aa_text_bits 4
#define fz_rasterizer_aa_hscale(ras) 5
#define fz_rasterizer_aa_vscale(ras) 3
#define fz_rasterizer_aa_bits(ras) 4
#define fz_rasterizer_aa_text_bits(ras) 4

#elif AA_BITS > 0
#define AA_SCALE(s, x) ((x * 255) >> 2)
#define fz_aa_hscale 2
#define fz_aa_vscale 2
#define fz_aa_bits 2
#define fz_aa_text_bits 2
#define fz_rasterizer_aa_hscale(ras) 2
#define fz_rasterizer_aa_vscale(ras) 2
#define fz_rasterizer_aa_bits(ras) 2
#define fz_rasterizer_aa_text_bits(ras) 2

#else
#define AA_SCALE(s, x) (x * 255)
#define fz_aa_hscale 1
#define fz_aa_vscale 1
#define fz_aa_bits 0
#define fz_aa_text_bits 0
#define fz_rasterizer_aa_hscale(ras) 1
#define fz_rasterizer_aa_vscale(ras) 1
#define fz_rasterizer_aa_bits(ras) 0
#define fz_rasterizer_aa_text_bits(ras) 0

#endif
#else

#define AA_SCALE(scale, x) ((x * scale) >> 8)
#define fz_aa_hscale (ctx->aa.hscale)
#define fz_aa_vscale (ctx->aa.vscale)
#define fz_aa_scale (ctx->aa.scale)
#define fz_aa_bits (ctx->aa.bits)
#define fz_aa_text_bits (ctx->aa.text_bits)
#define fz_rasterizer_aa_hscale(ras) ((ras)->aa.hscale)
#define fz_rasterizer_aa_vscale(ras) ((ras)->aa.vscale)
#define fz_rasterizer_aa_scale(ras) ((ras)->aa.scale)
#define fz_rasterizer_aa_bits(ras) ((ras)->aa.bits)
#define fz_rasterizer_aa_text_bits(ras) ((ras)->aa.text_bits)

#endif

/* If AA_BITS is defined, then we assume constant N bits of antialiasing. We
 * will attempt to provide at least that number of bits of accuracy in the
 * antialiasing (to a maximum of 8). If it is defined to be 0 then no
 * antialiasing is done. If it is undefined to we will leave the antialiasing
 * accuracy as a run time choice.
 */

struct fz_overprint_s
{
	/* Bit i set -> never alter this color */
	uint32_t mask[(FZ_MAX_COLORS+31)/32];
};

static void inline fz_set_overprint(fz_overprint *op, int i)
{
	op->mask[i>>5] |= 1<<(i&31);
}

static int inline fz_overprint_component(const fz_overprint *op, int i)
{
	return ((op->mask[i>>5]>>(i & 31)) & 1) == 0;
}

static int inline fz_overprint_required(const fz_overprint *op)
{
	int i;

	if (op == NULL)
		return 0;

	for (i = 0; i < (FZ_MAX_COLORS+31)/32; i++)
		if (op->mask[i] != 0)
			return 1;

	return 0;
}

typedef struct fz_rasterizer_s fz_rasterizer;

typedef void (fz_rasterizer_drop_fn)(fz_context *ctx, fz_rasterizer *r);
typedef int (fz_rasterizer_reset_fn)(fz_context *ctx, fz_rasterizer *r);
typedef void (fz_rasterizer_postindex_fn)(fz_context *ctx, fz_rasterizer *r);
typedef void (fz_rasterizer_insert_fn)(fz_context *ctx, fz_rasterizer *r, float x0, float y0, float x1, float y1, int rev);
typedef void (fz_rasterizer_insert_rect_fn)(fz_context *ctx, fz_rasterizer *r, float fx0, float fy0, float fx1, float fy1);
typedef void (fz_rasterizer_gap_fn)(fz_context *ctx, fz_rasterizer *r);
typedef fz_irect *(fz_rasterizer_bound_fn)(fz_context *ctx, const fz_rasterizer *r, fz_irect *bbox);
typedef void (fz_rasterizer_fn)(fz_context *ctx, fz_rasterizer *r, int eofill, const fz_irect *clip, fz_pixmap *pix, unsigned char *colorbv, fz_overprint *eop);
typedef int (fz_rasterizer_is_rect_fn)(fz_context *ctx, fz_rasterizer *r);

typedef struct
{
	fz_rasterizer_drop_fn *drop;
	fz_rasterizer_reset_fn *reset;
	fz_rasterizer_postindex_fn *postindex;
	fz_rasterizer_insert_fn *insert;
	fz_rasterizer_insert_rect_fn *rect;
	fz_rasterizer_gap_fn *gap;
	fz_rasterizer_fn *convert;
	fz_rasterizer_is_rect_fn *is_rect;
	int reusable;
} fz_rasterizer_fns;

struct fz_rasterizer_s
{
	fz_rasterizer_fns fns;
	fz_aa_context aa;
	fz_irect clip; /* Specified clip rectangle */
	fz_irect bbox; /* Measured bbox of path while stroking/filling */
};

/*
	When rasterizing a shape, we first create a rasterizer then
	run through the edges of the shape, feeding them in.

	For a fill, this is easy as we just run along the path, feeding
	edges as we go.

	For a stroke, this is trickier, as we feed in edges from
	alternate sides of the stroke as we proceed along it. It is only
	when we reach the end of a subpath that we know whether we need
	an initial cap, or whether the list of edges match up.

	To identify whether a given edge fed in is forward or reverse,
	we tag it with a 'rev' value.

	Consider the following simplified example:

	Consider a simple path A, B, C, D, close.

	+------->-------+	The outside edge of this shape is the
	| A           B |	forward edge. This is fed into the rasterizer
	|   +---<---+   |	in order, with rev=0.
	|   |       |   |
	^   v       ^   v	The inside edge of this shape is the reverse
	|   |       |   |	edge. These edges are generated as we step
	|   +--->---+   |	through the path in clockwise order, but
	| D           C |	conceptually the path runs the other way.
	+-------<-------+	These are fed into the rasterizer in clockwise
				order, with rev=1.

	Consider another path, this time an open one: A,B,C,D

	+--->-------+	The outside edge of this shape is again the
	* A       B |	forward edge. This is fed into the rasterizer
	+---<---+   |	in order, with rev=0.
		|   |
		^   v	The inside edge of this shape is the reverse
		|   |	edge. These edges are generated as we step
	+--->---+   |	through the path in clockwise order, but
	^ D       C |	conceptually the path runs the other way.
	+---<-------+	These are fed into the rasterizer in clockwise
			order, with rev=1.

	At the end of the path, we realise that this is an open path, and we
	therefore have to put caps on. The cap at 'D' is easy, because it's
	a simple continuation of the rev=0 edge list that joins to the end
	of the rev=1 edge list.

	The cap at 'A' is trickier; it either needs to be (an) edge(s) prepended
	to the rev=0 list or the rev=1 list. We signal this special case by
	sending them with the special value rev=2.

	The "edge" rasterizer ignores these values. The "edgebuffer" rasterizer
	needs to use them to ensure that edges are correctly joined together
	to allow for any part of a pixel operation.
*/

/*
	fz_new_rasterizer: Create a new rasterizer instance.
	This encapsulates a scan converter.

	A single rasterizer instance can be used to scan convert many
	things.

	aa: The antialiasing settings to use (or NULL).
*/
fz_rasterizer *fz_new_rasterizer(fz_context *ctx, const fz_aa_context *aa);

/*
	fz_drop_rasterizer: Dispose of a rasterizer once
	finished with.
*/
static inline void fz_drop_rasterizer(fz_context *ctx, fz_rasterizer *r)
{
	if (r)
		r->fns.drop(ctx, r);
}

/*
	fz_reset_rasterizer: Reset a rasterizer, ready to scan convert
	a new shape.

	clip: A pointer to a (device space) clipping rectangle.

	Returns 1 if a indexing pass is required, or 0 if not.

	After this, the edges should be 'inserted' into the rasterizer.
*/
int fz_reset_rasterizer(fz_context *ctx, fz_rasterizer *r, fz_irect clip);

/*
	fz_insert_rasterizer: Insert an edge into a rasterizer.

	x0, y0: Initial point

	x1, y1: Final point

	rev: 'reverse' value, 0, 1 or 2. See above.
*/
static inline void fz_insert_rasterizer(fz_context *ctx, fz_rasterizer *r, float x0, float y0, float x1, float y1, int rev)
{
	r->fns.insert(ctx, r, x0, y0, x1, y1, rev);
}

/*
	fz_insert_rasterizer: Insert a rectangle into a rasterizer.

	x0, y0: One corner of the rectangle.

	x1, y1: The opposite corner of the rectangle.

	The rectangle inserted is conceptually:
		(x0,y0)->(x1,y0)->(x1,y1)->(x0,y1)->(x0,y0).

	This method is only used for axis aligned rectangles,
	and enables rasterizers to perform special 'anti-dropout'
	processing to ensure that horizontal artifacts aren't
	lost.
*/
static inline void fz_insert_rasterizer_rect(fz_context *ctx, fz_rasterizer *r, float x0, float y0, float x1, float y1)
{
	r->fns.rect(ctx, r, x0, y0, x1, y1);
}

/*
	fz_gap_rasterizer: Called to indicate that there is a gap
	in the lists of edges fed into the rasterizer (i.e. when
	a path hits a move).
*/
static inline void fz_gap_rasterizer(fz_context *ctx, fz_rasterizer *r)
{
	if (r->fns.gap)
		r->fns.gap(ctx, r);
}

/*
	fz_antidropout_rasterizer: Detect whether antidropout
	behaviour is required with this rasterizer.

	Returns 1 if required, 0 otherwise.
*/
static inline int fz_antidropout_rasterizer(fz_context *ctx, fz_rasterizer *r)
{
	return r->fns.rect != NULL;
}

/*
	fz_postindex_rasterizer: Called to signify the end of the
	indexing phase.

	After this has been called, the edges should be inserted
	again.
*/
static inline void fz_postindex_rasterizer(fz_context *ctx, fz_rasterizer *r)
{
	if (r->fns.postindex)
		r->fns.postindex(ctx, r);
}

/*
	fz_bound_rasterizer: Once a set of edges has been fed into a
	rasterizer, the (device space) bounding box can be retrieved.
*/
fz_irect fz_bound_rasterizer(fz_context *ctx, const fz_rasterizer *rast);

/*
	fz_scissor_rasterizer: Retrieve the clipping box with which the
	rasterizer was reset.
*/
fz_rect fz_scissor_rasterizer(fz_context *ctx, const fz_rasterizer *rast);

/*
	fz_convert_rasterizer: Convert the set of edges that have
	been fed in, into pixels within the pixmap.

	eofill: Fill rule; True for even odd, false for non zero.

	pix: The pixmap to fill into.

	colorbv: The color components corresponding to the pixmap.

	eop: effective overprint.
*/
void fz_convert_rasterizer(fz_context *ctx, fz_rasterizer *r, int eofill, fz_pixmap *pix, unsigned char *colorbv, fz_overprint *eop);

/*
	fz_is_rect_rasterizer: Detect if the edges fed into a
	rasterizer make up a simple rectangle.
*/
static inline int fz_is_rect_rasterizer(fz_context *ctx, fz_rasterizer *r)
{
	return r->fns.is_rect(ctx, r);
}

void *fz_new_rasterizer_of_size(fz_context *ctx, int size, const fz_rasterizer_fns *fns);

#define fz_new_derived_rasterizer(C,M,F) \
	((M*)Memento_label(fz_new_rasterizer_of_size(C, sizeof(M), F), #M))

/*
	fz_rasterizer_text_aa_level: Get the number of bits of
	antialiasing we are using for text in a given rasterizer.
	Between 0 and 8.
*/
int fz_rasterizer_text_aa_level(fz_rasterizer *ras);

/*
	fz_set_rasterizer_text_aa_level: Set the number of bits of
	antialiasing we should use for text in a given configuration.

	bits: The number of bits of antialiasing to use (values are clamped
	to within the 0 to 8 range).
*/
void fz_set_rasterizer_text_aa_level(fz_context *ctx, fz_aa_context *aa, int bits);

/*
	fz_rasterizer_graphics_aa_level: Get the number of bits of
	antialiasing we are using for graphics in a given rasterizer.

	Between 0 and 8.
*/
int fz_rasterizer_graphics_aa_level(fz_rasterizer *ras);

/*
	fz_set_rasterizer_graphics_aa_level: Set the number of bits of
	antialiasing we should use for graphics in a given rasterizer.

	bits: The number of bits of antialiasing to use (values are clamped
	to within the 0 to 8 range).
*/
void fz_set_rasterizer_graphics_aa_level(fz_context *ctx, fz_aa_context *aa, int bits);

/*
	fz_rasterizer_graphics_min_line_width: Get the minimum line
	width to be used for stroked lines in a given rasterizer.

	min_line_width: The minimum line width to use (in pixels).
*/
float fz_rasterizer_graphics_min_line_width(fz_rasterizer *ras);

/*
	fz_set_rasterizer_graphics_min_line_width: Set the minimum line
	width to be used for stroked lines in a given configuration.

	min_line_width: The minimum line width to use (in pixels).
*/
void fz_set_rasterizer_graphics_min_line_width(fz_context *ctx, fz_aa_context *aa, float min_line_width);

fz_rasterizer *fz_new_gel(fz_context *ctx);

typedef enum
{
	FZ_EDGEBUFFER_ANY_PART_OF_PIXEL,
	FZ_EDGEBUFFER_CENTER_OF_PIXEL
} fz_edgebuffer_rule;

fz_rasterizer *fz_new_edgebuffer(fz_context *ctx, fz_edgebuffer_rule rule);

int fz_flatten_fill_path(fz_context *ctx, fz_rasterizer *rast, const fz_path *path, fz_matrix ctm, float flatness, const fz_irect *irect, fz_irect *bounds);
int fz_flatten_stroke_path(fz_context *ctx, fz_rasterizer *rast, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth, const fz_irect *irect, fz_irect *bounds);

fz_irect *fz_bound_path_accurate(fz_context *ctx, fz_irect *bbox, const fz_irect *scissor, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth);

typedef void (fz_solid_color_painter_t)(unsigned char * FZ_RESTRICT dp, int n, int w, const unsigned char * FZ_RESTRICT color, int da, const fz_overprint * FZ_RESTRICT eop);

typedef void (fz_span_painter_t)(unsigned char * FZ_RESTRICT dp, int da, const unsigned char * FZ_RESTRICT sp, int sa, int n, int w, int alpha, const fz_overprint * FZ_RESTRICT eop);
typedef void (fz_span_color_painter_t)(unsigned char * FZ_RESTRICT dp, const unsigned char * FZ_RESTRICT mp, int n, int w, const unsigned char * FZ_RESTRICT color, int da, const fz_overprint * FZ_RESTRICT eop);

fz_solid_color_painter_t *fz_get_solid_color_painter(int n, const unsigned char * FZ_RESTRICT color, int da, const fz_overprint * FZ_RESTRICT eop);
fz_span_painter_t *fz_get_span_painter(int da, int sa, int n, int alpha, const fz_overprint * FZ_RESTRICT eop);
fz_span_color_painter_t *fz_get_span_color_painter(int n, int da, const unsigned char * FZ_RESTRICT color, const fz_overprint * FZ_RESTRICT eop);

void fz_paint_image(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, const fz_irect * FZ_RESTRICT scissor, fz_pixmap * FZ_RESTRICT shape, fz_pixmap * FZ_RESTRICT group_alpha, fz_pixmap * FZ_RESTRICT img, fz_matrix ctm, int alpha, int lerp_allowed, int gridfit_as_tiled, const fz_overprint * FZ_RESTRICT eop);
void fz_paint_image_with_color(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, const fz_irect * FZ_RESTRICT scissor, fz_pixmap * FZ_RESTRICT shape, fz_pixmap * FZ_RESTRICT group_alpha, fz_pixmap * FZ_RESTRICT img, fz_matrix ctm, const unsigned char * FZ_RESTRICT colorbv, int lerp_allowed, int gridfit_as_tiled, const fz_overprint * FZ_RESTRICT eop);

void fz_paint_pixmap(fz_pixmap * FZ_RESTRICT dst, const fz_pixmap * FZ_RESTRICT src, int alpha);
void fz_paint_pixmap_alpha(fz_pixmap * FZ_RESTRICT dst, const fz_pixmap * FZ_RESTRICT src, int alpha);
void fz_paint_pixmap_with_mask(fz_pixmap * FZ_RESTRICT dst, const fz_pixmap * FZ_RESTRICT src, const fz_pixmap * FZ_RESTRICT msk);
void fz_paint_pixmap_with_bbox(fz_pixmap * FZ_RESTRICT dst, const fz_pixmap * FZ_RESTRICT src, int alpha, fz_irect bbox);
void fz_paint_pixmap_with_overprint(fz_pixmap * FZ_RESTRICT dst, const fz_pixmap * FZ_RESTRICT src, const fz_overprint * FZ_RESTRICT eop);

void fz_blend_pixmap(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, fz_pixmap * FZ_RESTRICT src, int alpha, int blendmode, int isolated, const fz_pixmap * FZ_RESTRICT shape);
void fz_blend_pixmap_knockout(fz_context *ctx, fz_pixmap * FZ_RESTRICT dst, fz_pixmap * FZ_RESTRICT src, const fz_pixmap * FZ_RESTRICT shape);

void fz_paint_glyph(const unsigned char * FZ_RESTRICT colorbv, fz_pixmap * FZ_RESTRICT dst, unsigned char * FZ_RESTRICT dp, const fz_glyph * FZ_RESTRICT glyph, int w, int h, int skip_x, int skip_y, const fz_overprint * FZ_RESTRICT eop);

#endif
