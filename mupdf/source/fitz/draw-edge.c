#include "mupdf/fitz.h"
#include "draw-imp.h"

#define BBOX_MIN -(1<<20)
#define BBOX_MAX (1<<20)

/* divide and floor towards -inf */
static inline int fz_idiv(int a, int b)
{
	return a < 0 ? (a - b + 1) / b : a / b;
}

/* If AA_BITS is defined, then we assume constant N bits of antialiasing. We
 * will attempt to provide at least that number of bits of accuracy in the
 * antialiasing (to a maximum of 8). If it is defined to be 0 then no
 * antialiasing is done. If it is undefined to we will leave the antialiasing
 * accuracy as a run time choice.
 */
struct fz_aa_context_s
{
	int hscale;
	int vscale;
	int scale;
	int bits;
};

void fz_new_aa_context(fz_context *ctx)
{
#ifndef AA_BITS
	ctx->aa = fz_malloc_struct(ctx, fz_aa_context);
	ctx->aa->hscale = 17;
	ctx->aa->vscale = 15;
	ctx->aa->scale = 256;
	ctx->aa->bits = 8;

#define fz_aa_hscale ((ctxaa)->hscale)
#define fz_aa_vscale ((ctxaa)->vscale)
#define fz_aa_scale ((ctxaa)->scale)
#define fz_aa_bits ((ctxaa)->bits)
#define AA_SCALE(x) ((x * fz_aa_scale) >> 8)

#endif
}

void fz_copy_aa_context(fz_context *dst, fz_context *src)
{
	if (dst && dst->aa && src && src->aa)
		memcpy(dst->aa, src->aa, sizeof(*src->aa));
}

void fz_free_aa_context(fz_context *ctx)
{
#ifndef AA_BITS
	fz_free(ctx, ctx->aa);
	ctx->aa = NULL;
#endif
}

#ifdef AA_BITS

#if AA_BITS > 6
#define AA_SCALE(x) (x)
#define fz_aa_hscale 17
#define fz_aa_vscale 15
#define fz_aa_bits 8

#elif AA_BITS > 4
#define AA_SCALE(x) ((x * 255) >> 6)
#define fz_aa_hscale 8
#define fz_aa_vscale 8
#define fz_aa_bits 6

#elif AA_BITS > 2
#define AA_SCALE(x) (x * 17)
#define fz_aa_hscale 5
#define fz_aa_vscale 3
#define fz_aa_bits 4

#elif AA_BITS > 0
#define AA_SCALE(x) ((x * 255) >> 2)
#define fz_aa_hscale 2
#define fz_aa_vscale 2
#define fz_aa_bits 2

#else
#define AA_SCALE(x) (x * 255)
#define fz_aa_hscale 1
#define fz_aa_vscale 1
#define fz_aa_bits 0

#endif
#endif

int
fz_aa_level(fz_context *ctx)
{
	fz_aa_context *ctxaa = ctx->aa;
	return fz_aa_bits;
}

void
fz_set_aa_level(fz_context *ctx, int level)
{
	fz_aa_context *ctxaa = ctx->aa;
#ifdef AA_BITS
	fz_warn(ctx, "anti-aliasing was compiled with a fixed precision of %d bits", fz_aa_bits);
#else
	if (level > 6)
	{
		fz_aa_hscale = 17;
		fz_aa_vscale = 15;
		fz_aa_bits = 8;
	}
	else if (level > 4)
	{
		fz_aa_hscale = 8;
		fz_aa_vscale = 8;
		fz_aa_bits = 6;
	}
	else if (level > 2)
	{
		fz_aa_hscale = 5;
		fz_aa_vscale = 3;
		fz_aa_bits = 4;
	}
	else if (level > 0)
	{
		fz_aa_hscale = 2;
		fz_aa_vscale = 2;
		fz_aa_bits = 2;
	}
	else
	{
		fz_aa_hscale = 1;
		fz_aa_vscale = 1;
		fz_aa_bits = 0;
	}
	fz_aa_scale = 0xFF00 / (fz_aa_hscale * fz_aa_vscale);
#endif
}

/*
 * Global Edge List -- list of straight path segments for scan conversion
 *
 * Stepping along the edges is with Bresenham's line algorithm.
 *
 * See Mike Abrash -- Graphics Programming Black Book (notably chapter 40)
 */

typedef struct fz_edge_s fz_edge;

struct fz_edge_s
{
	int x, e, h, y;
	int adj_up, adj_down;
	int xmove;
	int xdir, ydir; /* -1 or +1 */
};

struct fz_gel_s
{
	fz_rect clip;
	fz_rect bbox;
	int cap, len;
	fz_edge *edges;
	int acap, alen;
	fz_edge **active;
	fz_context *ctx;
};

fz_gel *
fz_new_gel(fz_context *ctx)
{
	fz_gel *gel;

	gel = fz_malloc_struct(ctx, fz_gel);
	fz_try(ctx)
	{
		gel->edges = NULL;
		gel->ctx = ctx;
		gel->cap = 512;
		gel->len = 0;
		gel->edges = fz_malloc_array(ctx, gel->cap, sizeof(fz_edge));

		gel->clip.x0 = gel->clip.y0 = BBOX_MIN;
		gel->clip.x1 = gel->clip.y1 = BBOX_MAX;

		gel->bbox.x0 = gel->bbox.y0 = BBOX_MAX;
		gel->bbox.x1 = gel->bbox.y1 = BBOX_MIN;

		gel->acap = 64;
		gel->alen = 0;
		gel->active = fz_malloc_array(ctx, gel->acap, sizeof(fz_edge*));
	}
	fz_catch(ctx)
	{
		if (gel)
			fz_free(ctx, gel->edges);
		fz_free(ctx, gel);
		fz_rethrow(ctx);
	}

	return gel;
}

void
fz_reset_gel(fz_gel *gel, const fz_irect *clip)
{
	fz_aa_context *ctxaa = gel->ctx->aa;

	if (fz_is_infinite_irect(clip))
	{
		gel->clip.x0 = gel->clip.y0 = BBOX_MIN;
		gel->clip.x1 = gel->clip.y1 = BBOX_MAX;
	}
	else {
		gel->clip.x0 = clip->x0 * fz_aa_hscale;
		gel->clip.x1 = clip->x1 * fz_aa_hscale;
		gel->clip.y0 = clip->y0 * fz_aa_vscale;
		gel->clip.y1 = clip->y1 * fz_aa_vscale;
	}

	gel->bbox.x0 = gel->bbox.y0 = BBOX_MAX;
	gel->bbox.x1 = gel->bbox.y1 = BBOX_MIN;

	gel->len = 0;
}

void
fz_free_gel(fz_gel *gel)
{
	if (gel == NULL)
		return;
	fz_free(gel->ctx, gel->active);
	fz_free(gel->ctx, gel->edges);
	fz_free(gel->ctx, gel);
}

fz_irect *
fz_bound_gel(const fz_gel *gel, fz_irect *bbox)
{
	fz_aa_context *ctxaa = gel->ctx->aa;
	if (gel->len == 0)
	{
		*bbox = fz_empty_irect;
	}
	else
	{
		bbox->x0 = fz_idiv(gel->bbox.x0, fz_aa_hscale);
		bbox->y0 = fz_idiv(gel->bbox.y0, fz_aa_vscale);
		bbox->x1 = fz_idiv(gel->bbox.x1, fz_aa_hscale) + 1;
		bbox->y1 = fz_idiv(gel->bbox.y1, fz_aa_vscale) + 1;
	}
	return bbox;
}

enum { INSIDE, OUTSIDE, LEAVE, ENTER };

#define clip_lerp_y(v,m,x0,y0,x1,y1,t) clip_lerp_x(v,m,y0,x0,y1,x1,t)

static int
clip_lerp_x(int val, int m, int x0, int y0, int x1, int y1, int *out)
{
	int v0out = m ? x0 > val : x0 < val;
	int v1out = m ? x1 > val : x1 < val;

	if (v0out + v1out == 0)
		return INSIDE;

	if (v0out + v1out == 2)
		return OUTSIDE;

	if (v1out)
	{
		*out = y0 + (int)(((float)(y1 - y0)) * (val - x0) / (x1 - x0));
		return LEAVE;
	}

	else
	{
		*out = y1 + (int)(((float)(y0 - y1)) * (val - x1) / (x0 - x1));
		return ENTER;
	}
}

static void
fz_insert_gel_raw(fz_gel *gel, int x0, int y0, int x1, int y1)
{
	fz_edge *edge;
	int dx, dy;
	int winding;
	int width;
	int tmp;

	if (y0 == y1)
		return;

	if (y0 > y1) {
		winding = -1;
		tmp = x0; x0 = x1; x1 = tmp;
		tmp = y0; y0 = y1; y1 = tmp;
	}
	else
		winding = 1;

	if (x0 < gel->bbox.x0) gel->bbox.x0 = x0;
	if (x0 > gel->bbox.x1) gel->bbox.x1 = x0;
	if (x1 < gel->bbox.x0) gel->bbox.x0 = x1;
	if (x1 > gel->bbox.x1) gel->bbox.x1 = x1;

	if (y0 < gel->bbox.y0) gel->bbox.y0 = y0;
	if (y1 > gel->bbox.y1) gel->bbox.y1 = y1;

	if (gel->len + 1 == gel->cap) {
		int new_cap = gel->cap + 512;
		gel->edges = fz_resize_array(gel->ctx, gel->edges, new_cap, sizeof(fz_edge));
		gel->cap = new_cap;
	}

	edge = &gel->edges[gel->len++];

	dy = y1 - y0;
	dx = x1 - x0;
	width = fz_absi(dx);

	edge->xdir = dx > 0 ? 1 : -1;
	edge->ydir = winding;
	edge->x = x0;
	edge->y = y0;
	edge->h = dy;
	edge->adj_down = dy;

	/* initial error term going l->r and r->l */
	if (dx >= 0)
		edge->e = 0;
	else
		edge->e = -dy + 1;

	/* y-major edge */
	if (dy >= width) {
		edge->xmove = 0;
		edge->adj_up = width;
	}

	/* x-major edge */
	else {
		edge->xmove = (width / dy) * edge->xdir;
		edge->adj_up = width % dy;
	}
}

void
fz_insert_gel(fz_gel *gel, float fx0, float fy0, float fx1, float fy1)
{
	int x0, y0, x1, y1;
	int d, v;
	fz_aa_context *ctxaa = gel->ctx->aa;

	fx0 = floorf(fx0 * fz_aa_hscale);
	fx1 = floorf(fx1 * fz_aa_hscale);
	fy0 = floorf(fy0 * fz_aa_vscale);
	fy1 = floorf(fy1 * fz_aa_vscale);

	/* Call fz_clamp so that clamping is done in the float domain, THEN
	 * cast down to an int. Calling fz_clampi causes problems due to the
	 * implicit cast down from float to int of the first argument
	 * over/underflowing and flipping sign at extreme values. */
	x0 = (int)fz_clamp(fx0, BBOX_MIN * fz_aa_hscale, BBOX_MAX * fz_aa_hscale);
	y0 = (int)fz_clamp(fy0, BBOX_MIN * fz_aa_vscale, BBOX_MAX * fz_aa_vscale);
	x1 = (int)fz_clamp(fx1, BBOX_MIN * fz_aa_hscale, BBOX_MAX * fz_aa_hscale);
	y1 = (int)fz_clamp(fy1, BBOX_MIN * fz_aa_vscale, BBOX_MAX * fz_aa_vscale);

	d = clip_lerp_y(gel->clip.y0, 0, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) return;
	if (d == LEAVE) { y1 = gel->clip.y0; x1 = v; }
	if (d == ENTER) { y0 = gel->clip.y0; x0 = v; }

	d = clip_lerp_y(gel->clip.y1, 1, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) return;
	if (d == LEAVE) { y1 = gel->clip.y1; x1 = v; }
	if (d == ENTER) { y0 = gel->clip.y1; x0 = v; }

	d = clip_lerp_x(gel->clip.x0, 0, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) {
		x0 = x1 = gel->clip.x0;
	}
	if (d == LEAVE) {
		fz_insert_gel_raw(gel, gel->clip.x0, v, gel->clip.x0, y1);
		x1 = gel->clip.x0;
		y1 = v;
	}
	if (d == ENTER) {
		fz_insert_gel_raw(gel, gel->clip.x0, y0, gel->clip.x0, v);
		x0 = gel->clip.x0;
		y0 = v;
	}

	d = clip_lerp_x(gel->clip.x1, 1, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) {
		x0 = x1 = gel->clip.x1;
	}
	if (d == LEAVE) {
		fz_insert_gel_raw(gel, gel->clip.x1, v, gel->clip.x1, y1);
		x1 = gel->clip.x1;
		y1 = v;
	}
	if (d == ENTER) {
		fz_insert_gel_raw(gel, gel->clip.x1, y0, gel->clip.x1, v);
		x0 = gel->clip.x1;
		y0 = v;
	}

	fz_insert_gel_raw(gel, x0, y0, x1, y1);
}

void
fz_sort_gel(fz_gel *gel)
{
	fz_edge *a = gel->edges;
	int n = gel->len;

	int h, i, k;
	fz_edge t;

	h = 1;
	if (n < 14) {
		h = 1;
	}
	else {
		while (h < n)
			h = 3 * h + 1;
		h /= 3;
		h /= 3;
	}

	while (h > 0)
	{
		for (i = 0; i < n; i++) {
			t = a[i];
			k = i - h;
			/* TODO: sort on y major, x minor */
			while (k >= 0 && a[k].y > t.y) {
				a[k + h] = a[k];
				k -= h;
			}
			a[k + h] = t;
		}

		h /= 3;
	}
}

int
fz_is_rect_gel(fz_gel *gel)
{
	/* a rectangular path is converted into two vertical edges of identical height */
	if (gel->len == 2)
	{
		fz_edge *a = gel->edges + 0;
		fz_edge *b = gel->edges + 1;
		return a->y == b->y && a->h == b->h &&
			a->xmove == 0 && a->adj_up == 0 &&
			b->xmove == 0 && b->adj_up == 0;
	}
	return 0;
}

/*
 * Active Edge List -- keep track of active edges while sweeping
 */

static void
sort_active(fz_edge **a, int n)
{
	int h, i, k;
	fz_edge *t;

	h = 1;
	if (n < 14) {
		h = 1;
	}
	else {
		while (h < n)
			h = 3 * h + 1;
		h /= 3;
		h /= 3;
	}

	while (h > 0)
	{
		for (i = 0; i < n; i++) {
			t = a[i];
			k = i - h;
			while (k >= 0 && a[k]->x > t->x) {
				a[k + h] = a[k];
				k -= h;
			}
			a[k + h] = t;
		}

		h /= 3;
	}
}

static int
insert_active(fz_gel *gel, int y, int *e_)
{
	int h_min = INT_MAX;
	int e = *e_;

	/* insert edges that start here */
	if (e < gel->len && gel->edges[e].y == y)
	{
		do {
			if (gel->alen + 1 == gel->acap) {
				int newcap = gel->acap + 64;
				fz_edge **newactive = fz_resize_array(gel->ctx, gel->active, newcap, sizeof(fz_edge*));
				gel->active = newactive;
				gel->acap = newcap;
			}
			gel->active[gel->alen++] = &gel->edges[e++];
		} while (e < gel->len && gel->edges[e].y == y);
		*e_ = e;
	}

	if (e < gel->len)
		h_min = gel->edges[e].y - y;

	for (e=0; e < gel->alen; e++)
	{
		if (gel->active[e]->xmove != 0 || gel->active[e]->adj_up != 0)
		{
			h_min = 1;
			break;
		}
		if (gel->active[e]->h < h_min)
		{
			h_min = gel->active[e]->h;
			if (h_min == 1)
				break;
		}
	}

	/* shell-sort the edges by increasing x */
	sort_active(gel->active, gel->alen);

	return h_min;
}

static void
advance_active(fz_gel *gel, int inc)
{
	fz_edge *edge;
	int i = 0;

	while (i < gel->alen)
	{
		edge = gel->active[i];

		edge->h -= inc;

		/* terminator! */
		if (edge->h == 0) {
			gel->active[i] = gel->active[--gel->alen];
		}

		else {
			edge->x += edge->xmove;
			edge->e += edge->adj_up;
			if (edge->e > 0) {
				edge->x += edge->xdir;
				edge->e -= edge->adj_down;
			}
			i ++;
		}
	}
}

/*
 * Anti-aliased scan conversion.
 */

static inline void add_span_aa(fz_aa_context *ctxaa, int *list, int x0, int x1, int xofs, int h)
{
	int x0pix, x0sub;
	int x1pix, x1sub;

	if (x0 == x1)
		return;

	/* x between 0 and width of bbox */
	x0 -= xofs;
	x1 -= xofs;

	/* The cast to unsigned below helps the compiler produce faster
	 * code on ARMs as the multiply by reciprocal trick it uses does not
	 * need to correct for signedness. */
	x0pix = ((unsigned int)x0) / fz_aa_hscale;
	x0sub = ((unsigned int)x0) % fz_aa_hscale;
	x1pix = ((unsigned int)x1) / fz_aa_hscale;
	x1sub = ((unsigned int)x1) % fz_aa_hscale;

	if (x0pix == x1pix)
	{
		list[x0pix] += h*(x1sub - x0sub);
		list[x0pix+1] += h*(x0sub - x1sub);
	}

	else
	{
		list[x0pix] += h*(fz_aa_hscale - x0sub);
		list[x0pix+1] += h*x0sub;
		list[x1pix] += h*(x1sub - fz_aa_hscale);
		list[x1pix+1] += h*-x1sub;
	}
}

static inline void non_zero_winding_aa(fz_gel *gel, int *list, int xofs, int h)
{
	int winding = 0;
	int x = 0;
	int i;
	fz_aa_context *ctxaa = gel->ctx->aa;

	for (i = 0; i < gel->alen; i++)
	{
		if (!winding && (winding + gel->active[i]->ydir))
			x = gel->active[i]->x;
		if (winding && !(winding + gel->active[i]->ydir))
			add_span_aa(ctxaa, list, x, gel->active[i]->x, xofs, h);
		winding += gel->active[i]->ydir;
	}
}

static inline void even_odd_aa(fz_gel *gel, int *list, int xofs, int h)
{
	int even = 0;
	int x = 0;
	int i;
	fz_aa_context *ctxaa = gel->ctx->aa;

	for (i = 0; i < gel->alen; i++)
	{
		if (!even)
			x = gel->active[i]->x;
		else
			add_span_aa(ctxaa, list, x, gel->active[i]->x, xofs, h);
		even = !even;
	}
}

static inline void undelta_aa(fz_aa_context *ctxaa, unsigned char * restrict out, int * restrict in, int n)
{
	int d = 0;
	while (n--)
	{
		d += *in++;
		*out++ = AA_SCALE(d);
	}
}

static inline void blit_aa(fz_pixmap *dst, int x, int y,
	unsigned char *mp, int w, unsigned char *color)
{
	unsigned char *dp;
	dp = dst->samples + (unsigned int)(( (y - dst->y) * dst->w + (x - dst->x) ) * dst->n);
	if (color)
		fz_paint_span_with_color(dp, mp, dst->n, w, color);
	else
		fz_paint_span(dp, mp, 1, w, 255);
}

static void
fz_scan_convert_aa(fz_gel *gel, int eofill, const fz_irect *clip,
	fz_pixmap *dst, unsigned char *color)
{
	unsigned char *alphas;
	int *deltas;
	int y, e;
	int yd, yc;
	fz_context *ctx = gel->ctx;
	fz_aa_context *ctxaa = ctx->aa;
	int height, h0, rh;

	int xmin = fz_idiv(gel->bbox.x0, fz_aa_hscale);
	int xmax = fz_idiv(gel->bbox.x1, fz_aa_hscale) + 1;

	int xofs = xmin * fz_aa_hscale;

	int skipx = clip->x0 - xmin;
	int clipn = clip->x1 - clip->x0;

	if (gel->len == 0)
		return;

	assert(clip->x0 >= xmin);
	assert(clip->x1 <= xmax);

	alphas = fz_malloc_no_throw(ctx, xmax - xmin + 1);
	deltas = fz_malloc_no_throw(ctx, (xmax - xmin + 1) * sizeof(int));
	if (alphas == NULL || deltas == NULL)
	{
		fz_free(ctx, alphas);
		fz_free(ctx, deltas);
		fz_throw(ctx, FZ_ERROR_GENERIC, "scan conversion failed (malloc failure)");
	}
	memset(deltas, 0, (xmax - xmin + 1) * sizeof(int));
	gel->alen = 0;

	/* The theory here is that we have a list of the edges (gel) of length
	 * gel->len. We have an initially empty list of 'active' edges (of
	 * length gel->alen). As we increase y, we move any edge that is
	 * active at this point into the active list. We know that any edge
	 * before index 'e' is either active, or has been retired.
	 * Once the length of the active list is 0, and e has reached gel->len
	 * we know we are finished.
	 *
	 * As we move through the list, we group fz_aa_vscale 'sub scanlines'
	 * into single scanlines, and we blit them.
	 */

	e = 0;
	y = gel->edges[0].y;
	yd = fz_idiv(y, fz_aa_vscale);

	/* Quickly skip to the start of the clip region */
	while (yd < clip->y0 && (gel->alen > 0 || e < gel->len))
	{
		/* rh = remaining height = number of subscanlines left to be
		 * inserted into the current scanline, which will be plotted
		 * at yd. */
		rh = (yd+1)*fz_aa_vscale - y;

		/* height = The number of subscanlines with identical edge
		 * positions (i.e. 1 if we have any non vertical edges). */
		height = insert_active(gel, y, &e);
		h0 = height;
		if (h0 >= rh)
		{
			/* We have enough subscanlines to skip to the next
			 * scanline. */
			h0 -= rh;
			yd++;
		}
		/* Skip any whole scanlines we can */
		while (yd < clip->y0 && h0 >= fz_aa_vscale)
		{
			h0 -= fz_aa_vscale;
			yd++;
		}
		/* If we haven't hit the start of the clip region, then we
		 * have less than a scanline left. */
		if (yd < clip->y0)
		{
			h0 = 0;
		}
		height -= h0;
		advance_active(gel, height);

		y += height;
	}

	/* Now do the active lines */
	while (gel->alen > 0 || e < gel->len)
	{
		yc = fz_idiv(y, fz_aa_vscale);	/* yc = current scanline */
		/* rh = remaining height = number of subscanlines left to be
		 * inserted into the current scanline, which will be plotted
		 * at yd. */
		rh = (yc+1)*fz_aa_vscale - y;
		if (yc != yd)
		{
			undelta_aa(ctxaa, alphas, deltas, skipx + clipn);
			blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color);
			memset(deltas, 0, (skipx + clipn) * sizeof(int));
		}
		yd = yc;
		if (yd >= clip->y1)
			break;

		/* height = The number of subscanlines with identical edge
		 * positions (i.e. 1 if we have any non vertical edges). */
		height = insert_active(gel, y, &e);
		h0 = height;
		if (h0 > rh)
		{
			if (rh < fz_aa_vscale)
			{
				/* We have to finish a scanline off, and we
				 * have more sub scanlines than will fit into
				 * it. */
				if (eofill)
					even_odd_aa(gel, deltas, xofs, rh);
				else
					non_zero_winding_aa(gel, deltas, xofs, rh);
				undelta_aa(ctxaa, alphas, deltas, skipx + clipn);
				blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color);
				memset(deltas, 0, (skipx + clipn) * sizeof(int));
				yd++;
				if (yd >= clip->y1)
					break;
				h0 -= rh;
			}
			if (h0 > fz_aa_vscale)
			{
				/* Calculate the deltas for any completely full
				 * scanlines. */
				h0 -= fz_aa_vscale;
				if (eofill)
					even_odd_aa(gel, deltas, xofs, fz_aa_vscale);
				else
					non_zero_winding_aa(gel, deltas, xofs, fz_aa_vscale);
				undelta_aa(ctxaa, alphas, deltas, skipx + clipn);
				do
				{
					/* Do any successive whole scanlines - no need
					 * to recalculate deltas here. */
					blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color);
					yd++;
					if (yd >= clip->y1)
						goto clip_ended;
					h0 -= fz_aa_vscale;
				}
				while (h0 > 0);
				/* If we have exactly one full scanline left
				 * to go, then the deltas/alphas are set up
				 * already. */
				if (h0 == 0)
					goto advance;
				memset(deltas, 0, (skipx + clipn) * sizeof(int));
				h0 += fz_aa_vscale;
			}
		}
		if (eofill)
			even_odd_aa(gel, deltas, xofs, h0);
		else
			non_zero_winding_aa(gel, deltas, xofs, h0);
advance:
		advance_active(gel, height);

		y += height;
	}

	if (yd < clip->y1)
	{
		undelta_aa(ctxaa, alphas, deltas, skipx + clipn);
		blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color);
	}
clip_ended:
	fz_free(ctx, deltas);
	fz_free(ctx, alphas);
}

/*
 * Sharp (not anti-aliased) scan conversion
 */

static inline void blit_sharp(int x0, int x1, int y,
	const fz_irect *clip, fz_pixmap *dst, unsigned char *color)
{
	unsigned char *dp;
	x0 = fz_clampi(x0, dst->x, dst->x + dst->w);
	x1 = fz_clampi(x1, dst->x, dst->x + dst->w);
	if (x0 < x1)
	{
		dp = dst->samples + (unsigned int)(( (y - dst->y) * dst->w + (x0 - dst->x) ) * dst->n);
		if (color)
			fz_paint_solid_color(dp, dst->n, x1 - x0, color);
		else
			fz_paint_solid_alpha(dp, x1 - x0, 255);
	}
}

static inline void non_zero_winding_sharp(fz_gel *gel, int y,
	const fz_irect *clip, fz_pixmap *dst, unsigned char *color)
{
	int winding = 0;
	int x = 0;
	int i;
	for (i = 0; i < gel->alen; i++)
	{
		if (!winding && (winding + gel->active[i]->ydir))
			x = gel->active[i]->x;
		if (winding && !(winding + gel->active[i]->ydir))
			blit_sharp(x, gel->active[i]->x, y, clip, dst, color);
		winding += gel->active[i]->ydir;
	}
}

static inline void even_odd_sharp(fz_gel *gel, int y,
	const fz_irect *clip, fz_pixmap *dst, unsigned char *color)
{
	int even = 0;
	int x = 0;
	int i;
	for (i = 0; i < gel->alen; i++)
	{
		if (!even)
			x = gel->active[i]->x;
		else
			blit_sharp(x, gel->active[i]->x, y, clip, dst, color);
		even = !even;
	}
}

static void
fz_scan_convert_sharp(fz_gel *gel, int eofill, const fz_irect *clip,
	fz_pixmap *dst, unsigned char *color)
{
	int e = 0;
	int y = gel->edges[0].y;
	int height;

	gel->alen = 0;

	/* Skip any lines before the clip region */
	if (y < clip->y0)
	{
		while (gel->alen > 0 || e < gel->len)
		{
			height = insert_active(gel, y, &e);
			y += height;
			if (y >= clip->y0)
			{
				height -= y - clip->y0;
				y = clip->y0;
				break;
			}
		}
	}

	/* Now process as lines within the clip region */
	while (gel->alen > 0 || e < gel->len)
	{
		height = insert_active(gel, y, &e);

		if (gel->alen == 0)
			y += height;
		else
		{
			int h;
			if (height >= clip->y1 - y)
				height = clip->y1 - y;

			h = height;
			while (h--)
			{
				if (eofill)
					even_odd_sharp(gel, y, clip, dst, color);
				else
					non_zero_winding_sharp(gel, y, clip, dst, color);
				y++;
			}
		}
		if (y >= clip->y1)
			break;

		advance_active(gel, height);
	}
}

void
fz_scan_convert(fz_gel *gel, int eofill, const fz_irect *clip,
	fz_pixmap *dst, unsigned char *color)
{
	fz_aa_context *ctxaa = gel->ctx->aa;
	fz_irect local_clip;

	if (fz_is_empty_irect(fz_intersect_irect(fz_pixmap_bbox_no_ctx(dst, &local_clip), clip)))
		return;

	if (fz_aa_bits > 0)
		fz_scan_convert_aa(gel, eofill, &local_clip, dst, color);
	else
		fz_scan_convert_sharp(gel, eofill, &local_clip, dst, color);
}
