#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Global Edge List -- list of straight path segments for scan conversion
 *
 * Stepping along the edges is with Bresenham's line algorithm.
 *
 * See Mike Abrash -- Graphics Programming Black Book (notably chapter 40)
 */

typedef struct fz_edge_s
{
	int x, e, h, y;
	int adj_up, adj_down;
	int xmove;
	int xdir, ydir; /* -1 or +1 */
} fz_edge;

typedef struct fz_gel_s
{
	fz_rasterizer super;
	int cap, len;
	fz_edge *edges;
	int acap, alen;
	fz_edge **active;
	int bcap;
	unsigned char *alphas;
	int *deltas;
} fz_gel;

static int
fz_reset_gel(fz_context *ctx, fz_rasterizer *rast)
{
	fz_gel *gel = (fz_gel *)rast;

	gel->len = 0;
	gel->alen = 0;

	return 0;
}

static void
fz_drop_gel(fz_context *ctx, fz_rasterizer *rast)
{
	fz_gel *gel = (fz_gel *)rast;
	if (gel == NULL)
		return;
	fz_free(ctx, gel->active);
	fz_free(ctx, gel->edges);
	fz_free(ctx, gel->alphas);
	fz_free(ctx, gel->deltas);
	fz_free(ctx, gel);
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
fz_insert_gel_raw(fz_context *ctx, fz_rasterizer *ras, int x0, int y0, int x1, int y1)
{
	fz_gel *gel = (fz_gel *)ras;
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

	if (x0 < gel->super.bbox.x0) gel->super.bbox.x0 = x0;
	if (x0 > gel->super.bbox.x1) gel->super.bbox.x1 = x0;
	if (x1 < gel->super.bbox.x0) gel->super.bbox.x0 = x1;
	if (x1 > gel->super.bbox.x1) gel->super.bbox.x1 = x1;

	if (y0 < gel->super.bbox.y0) gel->super.bbox.y0 = y0;
	if (y1 > gel->super.bbox.y1) gel->super.bbox.y1 = y1;

	if (gel->len + 1 == gel->cap) {
		int new_cap = gel->cap * 2;
		gel->edges = fz_realloc_array(ctx, gel->edges, new_cap, fz_edge);
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

static void
fz_insert_gel(fz_context *ctx, fz_rasterizer *ras, float fx0, float fy0, float fx1, float fy1, int rev)
{
	int x0, y0, x1, y1;
	int d, v;
	const int hscale = fz_rasterizer_aa_hscale(ras);
	const int vscale = fz_rasterizer_aa_vscale(ras);

	fx0 = floorf(fx0 * hscale);
	fx1 = floorf(fx1 * hscale);
	fy0 = floorf(fy0 * vscale);
	fy1 = floorf(fy1 * vscale);

	/* Call fz_clamp so that clamping is done in the float domain, THEN
	 * cast down to an int. Calling fz_clampi causes problems due to the
	 * implicit cast down from float to int of the first argument
	 * over/underflowing and flipping sign at extreme values. */
	x0 = (int)fz_clamp(fx0, BBOX_MIN * hscale, BBOX_MAX * hscale);
	y0 = (int)fz_clamp(fy0, BBOX_MIN * vscale, BBOX_MAX * vscale);
	x1 = (int)fz_clamp(fx1, BBOX_MIN * hscale, BBOX_MAX * hscale);
	y1 = (int)fz_clamp(fy1, BBOX_MIN * vscale, BBOX_MAX * vscale);

	d = clip_lerp_y(ras->clip.y0, 0, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) return;
	if (d == LEAVE) { y1 = ras->clip.y0; x1 = v; }
	if (d == ENTER) { y0 = ras->clip.y0; x0 = v; }

	d = clip_lerp_y(ras->clip.y1, 1, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) return;
	if (d == LEAVE) { y1 = ras->clip.y1; x1 = v; }
	if (d == ENTER) { y0 = ras->clip.y1; x0 = v; }

	d = clip_lerp_x(ras->clip.x0, 0, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) {
		x0 = x1 = ras->clip.x0;
	}
	if (d == LEAVE) {
		fz_insert_gel_raw(ctx, ras, ras->clip.x0, v, ras->clip.x0, y1);
		x1 = ras->clip.x0;
		y1 = v;
	}
	if (d == ENTER) {
		fz_insert_gel_raw(ctx, ras, ras->clip.x0, y0, ras->clip.x0, v);
		x0 = ras->clip.x0;
		y0 = v;
	}

	d = clip_lerp_x(ras->clip.x1, 1, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) {
		x0 = x1 = ras->clip.x1;
	}
	if (d == LEAVE) {
		fz_insert_gel_raw(ctx, ras, ras->clip.x1, v, ras->clip.x1, y1);
		x1 = ras->clip.x1;
		y1 = v;
	}
	if (d == ENTER) {
		fz_insert_gel_raw(ctx, ras, ras->clip.x1, y0, ras->clip.x1, v);
		x0 = ras->clip.x1;
		y0 = v;
	}

	fz_insert_gel_raw(ctx, ras, x0, y0, x1, y1);
}

static void
fz_insert_gel_rect(fz_context *ctx, fz_rasterizer *ras, float fx0, float fy0, float fx1, float fy1)
{
	int x0, y0, x1, y1;
	const int hscale = fz_rasterizer_aa_hscale(ras);
	const int vscale = fz_rasterizer_aa_vscale(ras);

	if (fx0 <= fx1)
	{
		fx0 = floorf(fx0 * hscale);
		fx1 = ceilf(fx1 * hscale);
	}
	else
	{
		fx0 = ceilf(fx0 * hscale);
		fx1 = floorf(fx1 * hscale);
	}
	if (fy0 <= fy1)
	{
		fy0 = floorf(fy0 * vscale);
		fy1 = ceilf(fy1 * vscale);
	}
	else
	{
		fy0 = ceilf(fy0 * vscale);
		fy1 = floorf(fy1 * vscale);
	}

	fx0 = fz_clamp(fx0, ras->clip.x0, ras->clip.x1);
	fx1 = fz_clamp(fx1, ras->clip.x0, ras->clip.x1);
	fy0 = fz_clamp(fy0, ras->clip.y0, ras->clip.y1);
	fy1 = fz_clamp(fy1, ras->clip.y0, ras->clip.y1);

	/* Call fz_clamp so that clamping is done in the float domain, THEN
	 * cast down to an int. Calling fz_clampi causes problems due to the
	 * implicit cast down from float to int of the first argument
	 * over/underflowing and flipping sign at extreme values. */
	x0 = (int)fz_clamp(fx0, BBOX_MIN * hscale, BBOX_MAX * hscale);
	y0 = (int)fz_clamp(fy0, BBOX_MIN * vscale, BBOX_MAX * vscale);
	x1 = (int)fz_clamp(fx1, BBOX_MIN * hscale, BBOX_MAX * hscale);
	y1 = (int)fz_clamp(fy1, BBOX_MIN * vscale, BBOX_MAX * vscale);

	fz_insert_gel_raw(ctx, ras, x1, y0, x1, y1);
	fz_insert_gel_raw(ctx, ras, x0, y1, x0, y0);
}

static int
cmpedge(const void *va, const void *vb)
{
	const fz_edge *a = va;
	const fz_edge *b = vb;
	return a->y - b->y;
}

static void
sort_gel(fz_context *ctx, fz_gel *gel)
{
	fz_edge *a = gel->edges;
	int n = gel->len;
	int h, i, k;
	fz_edge t;

	/* quick sort for long lists */
	if (n > 10000)
	{
		qsort(a, n, sizeof *a, cmpedge);
		return;
	}

	/* shell sort for short lists */
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

static int
fz_is_rect_gel(fz_context *ctx, fz_rasterizer *ras)
{
	fz_gel *gel = (fz_gel *)ras;
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
insert_active(fz_context *ctx, fz_gel *gel, int y, int *e_)
{
	int h_min = INT_MAX;
	int e = *e_;

	/* insert edges that start here */
	if (e < gel->len && gel->edges[e].y == y)
	{
		do {
			if (gel->alen + 1 == gel->acap) {
				int newcap = gel->acap + 64;
				fz_edge **newactive = fz_realloc_array(ctx, gel->active, newcap, fz_edge*);
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
advance_active(fz_context *ctx, fz_gel *gel, int inc)
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

static inline void
add_span_aa(fz_context *ctx, fz_gel *gel, int *list, int x0, int x1, int xofs, int h)
{
	int x0pix, x0sub;
	int x1pix, x1sub;
	const int hscale = fz_rasterizer_aa_hscale(&gel->super);

	if (x0 == x1)
		return;

	/* x between 0 and width of bbox */
	x0 -= xofs;
	x1 -= xofs;

	/* The cast to unsigned below helps the compiler produce faster
	 * code on ARMs as the multiply by reciprocal trick it uses does not
	 * need to correct for signedness. */
	x0pix = ((unsigned int)x0) / hscale;
	x0sub = ((unsigned int)x0) % hscale;
	x1pix = ((unsigned int)x1) / hscale;
	x1sub = ((unsigned int)x1) % hscale;

	if (x0pix == x1pix)
	{
		list[x0pix] += h*(x1sub - x0sub);
		list[x0pix+1] += h*(x0sub - x1sub);
	}

	else
	{
		list[x0pix] += h*(hscale - x0sub);
		list[x0pix+1] += h*x0sub;
		list[x1pix] += h*(x1sub - hscale);
		list[x1pix+1] += h*-x1sub;
	}
}

static inline void
non_zero_winding_aa(fz_context *ctx, fz_gel *gel, int *list, int xofs, int h)
{
	int winding = 0;
	int x = 0;
	int i;

	for (i = 0; i < gel->alen; i++)
	{
		if (!winding && (winding + gel->active[i]->ydir))
			x = gel->active[i]->x;
		if (winding && !(winding + gel->active[i]->ydir))
			add_span_aa(ctx, gel, list, x, gel->active[i]->x, xofs, h);
		winding += gel->active[i]->ydir;
	}
}

static inline void
even_odd_aa(fz_context *ctx, fz_gel *gel, int *list, int xofs, int h)
{
	int even = 0;
	int x = 0;
	int i;

	for (i = 0; i < gel->alen; i++)
	{
		if (!even)
			x = gel->active[i]->x;
		else
			add_span_aa(ctx, gel, list, x, gel->active[i]->x, xofs, h);
		even = !even;
	}
}

static inline void
undelta_aa(fz_context *ctx, unsigned char * FZ_RESTRICT out, int * FZ_RESTRICT in, int n, int scale)
{
	int d = 0;
	(void)scale; /* Avoid warnings in some builds */

	while (n--)
	{
		d += *in++;
		*out++ = AA_SCALE(scale, d);
	}
}

static inline void
blit_aa(fz_pixmap *dst, int x, int y, unsigned char *mp, int w, unsigned char *color, void *fn, fz_overprint *eop)
{
	unsigned char *dp;
	dp = dst->samples + (unsigned int)((y - dst->y) * dst->stride + (x - dst->x) * dst->n);
	if (color)
		(*(fz_span_color_painter_t *)fn)(dp, mp, dst->n, w, color, dst->alpha, eop);
	else
		(*(fz_span_painter_t *)fn)(dp, dst->alpha, mp, 1, 0, w, 255, eop);
}

static void
fz_scan_convert_aa(fz_context *ctx, fz_gel *gel, int eofill, const fz_irect *clip, fz_pixmap *dst, unsigned char *color, void *painter, fz_overprint *eop)
{
	unsigned char *alphas;
	int *deltas;
	int y, e;
	int yd, yc;
	int height, h0, rh;
	int bcap;
	const int hscale = fz_rasterizer_aa_hscale(&gel->super);
	const int vscale = fz_rasterizer_aa_vscale(&gel->super);
	const int scale = fz_rasterizer_aa_scale(&gel->super);

	int xmin = fz_idiv(gel->super.bbox.x0, hscale);
	int xmax = fz_idiv_up(gel->super.bbox.x1, hscale);

	int xofs = xmin * hscale;

	int skipx = clip->x0 - xmin;
	int clipn = clip->x1 - clip->x0;

	if (gel->len == 0)
		return;

	assert(xmin < xmax);
	assert(clip->x0 >= xmin);
	assert(clip->x1 <= xmax);

	bcap = xmax - xmin + 2; /* big enough for both alphas and deltas */
	if (bcap > gel->bcap)
	{
		gel->bcap = bcap;
		fz_free(ctx, gel->alphas);
		fz_free(ctx, gel->deltas);
		gel->alphas = NULL;
		gel->deltas = NULL;
		alphas = gel->alphas = Memento_label(fz_malloc_array(ctx, bcap, unsigned char), "gel_alphas");
		deltas = gel->deltas = Memento_label(fz_malloc_array(ctx, bcap, int), "gel_deltas");
	}
	alphas = gel->alphas;
	deltas = gel->deltas;

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
	yd = fz_idiv(y, vscale);

	/* Quickly skip to the start of the clip region */
	while (yd < clip->y0 && (gel->alen > 0 || e < gel->len))
	{
		/* rh = remaining height = number of subscanlines left to be
		 * inserted into the current scanline, which will be plotted
		 * at yd. */
		rh = (yd+1)*vscale - y;

		/* height = The number of subscanlines with identical edge
		 * positions (i.e. 1 if we have any non vertical edges). */
		height = insert_active(ctx, gel, y, &e);
		h0 = height;
		if (h0 >= rh)
		{
			/* We have enough subscanlines to skip to the next
			 * scanline. */
			h0 -= rh;
			yd++;
		}
		/* Skip any whole scanlines we can */
		while (yd < clip->y0 && h0 >= vscale)
		{
			h0 -= vscale;
			yd++;
		}
		/* If we haven't hit the start of the clip region, then we
		 * have less than a scanline left. */
		if (yd < clip->y0)
		{
			h0 = 0;
		}
		height -= h0;
		advance_active(ctx, gel, height);

		y += height;
	}

	/* Now do the active lines */
	while (gel->alen > 0 || e < gel->len)
	{
		yc = fz_idiv(y, vscale);	/* yc = current scanline */
		/* rh = remaining height = number of subscanlines left to be
		 * inserted into the current scanline, which will be plotted
		 * at yd. */
		rh = (yc+1)*vscale - y;
		if (yc != yd)
		{
			undelta_aa(ctx, alphas, deltas, skipx + clipn, scale);
			blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color, painter, eop);
			memset(deltas, 0, (skipx + clipn) * sizeof(int));
		}
		yd = yc;
		if (yd >= clip->y1)
			break;

		/* height = The number of subscanlines with identical edge
		 * positions (i.e. 1 if we have any non vertical edges). */
		height = insert_active(ctx, gel, y, &e);
		h0 = height;
		if (h0 > rh)
		{
			if (rh < vscale)
			{
				/* We have to finish a scanline off, and we
				 * have more sub scanlines than will fit into
				 * it. */
				if (eofill)
					even_odd_aa(ctx, gel, deltas, xofs, rh);
				else
					non_zero_winding_aa(ctx, gel, deltas, xofs, rh);
				undelta_aa(ctx, alphas, deltas, skipx + clipn, scale);
				blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color, painter, eop);
				memset(deltas, 0, (skipx + clipn) * sizeof(int));
				yd++;
				if (yd >= clip->y1)
					break;
				h0 -= rh;
			}
			if (h0 > vscale)
			{
				/* Calculate the deltas for any completely full
				 * scanlines. */
				h0 -= vscale;
				if (eofill)
					even_odd_aa(ctx, gel, deltas, xofs, vscale);
				else
					non_zero_winding_aa(ctx, gel, deltas, xofs, vscale);
				undelta_aa(ctx, alphas, deltas, skipx + clipn, scale);
				do
				{
					/* Do any successive whole scanlines - no need
					 * to recalculate deltas here. */
					blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color, painter, eop);
					yd++;
					if (yd >= clip->y1)
						goto clip_ended;
					h0 -= vscale;
				}
				while (h0 > 0);
				/* If we have exactly one full scanline left
				 * to go, then the deltas/alphas are set up
				 * already. */
				if (h0 == 0)
					goto advance;
				memset(deltas, 0, (skipx + clipn) * sizeof(int));
				h0 += vscale;
			}
		}
		if (eofill)
			even_odd_aa(ctx, gel, deltas, xofs, h0);
		else
			non_zero_winding_aa(ctx, gel, deltas, xofs, h0);
advance:
		advance_active(ctx, gel, height);

		y += height;
	}

	if (yd < clip->y1)
	{
		undelta_aa(ctx, alphas, deltas, skipx + clipn, scale);
		blit_aa(dst, xmin + skipx, yd, alphas + skipx, clipn, color, painter, eop);
	}
clip_ended:
	;
}

/*
 * Sharp (not anti-aliased) scan conversion
 */

static inline void
blit_sharp(int x0, int x1, int y, const fz_irect *clip, fz_pixmap *dst, unsigned char *color, fz_solid_color_painter_t *fn, fz_overprint *eop)
{
	unsigned char *dp;
	int da = dst->alpha;
	x0 = fz_clampi(x0, dst->x, dst->x + dst->w);
	x1 = fz_clampi(x1, dst->x, dst->x + dst->w);
	if (x0 < x1)
	{
		dp = dst->samples + (unsigned int)((y - dst->y) * dst->stride + (x0 - dst->x) * dst->n);
		if (color)
			(*fn)(dp, dst->n, x1 - x0, color, da, eop);
		else
			memset(dp, 255, x1-x0);
	}
}

static inline void
non_zero_winding_sharp(fz_context *ctx, fz_gel *gel, int y, const fz_irect *clip, fz_pixmap *dst, unsigned char *color, fz_solid_color_painter_t *fn, fz_overprint *eop)
{
	int winding = 0;
	int x = 0;
	int i;
	for (i = 0; i < gel->alen; i++)
	{
		if (!winding && (winding + gel->active[i]->ydir))
			x = gel->active[i]->x;
		if (winding && !(winding + gel->active[i]->ydir))
			blit_sharp(x, gel->active[i]->x, y, clip, dst, color, fn, eop);
		winding += gel->active[i]->ydir;
	}
}

static inline void
even_odd_sharp(fz_context *ctx, fz_gel *gel, int y, const fz_irect *clip, fz_pixmap *dst, unsigned char *color, fz_solid_color_painter_t *fn, fz_overprint *eop)
{
	int even = 0;
	int x = 0;
	int i;
	for (i = 0; i < gel->alen; i++)
	{
		if (!even)
			x = gel->active[i]->x;
		else
			blit_sharp(x, gel->active[i]->x, y, clip, dst, color, fn, eop);
		even = !even;
	}
}

static void
fz_scan_convert_sharp(fz_context *ctx,
	fz_gel *gel, int eofill, const fz_irect *clip,
	fz_pixmap *dst, unsigned char *color, fz_solid_color_painter_t *fn, fz_overprint *eop)
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
			height = insert_active(ctx, gel, y, &e);
			y += height;
			if (y >= clip->y0)
			{
				y = clip->y0;
				break;
			}
		}
	}

	/* Now process as lines within the clip region */
	while (gel->alen > 0 || e < gel->len)
	{
		height = insert_active(ctx, gel, y, &e);

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
					even_odd_sharp(ctx, gel, y, clip, dst, color, fn, eop);
				else
					non_zero_winding_sharp(ctx, gel, y, clip, dst, color, fn, eop);
				y++;
			}
		}
		if (y >= clip->y1)
			break;

		advance_active(ctx, gel, height);
	}
}

static void
fz_convert_gel(fz_context *ctx, fz_rasterizer *rast, int eofill, const fz_irect *clip, fz_pixmap *dst, unsigned char *color, fz_overprint *eop)
{
	fz_gel *gel = (fz_gel *)rast;

	sort_gel(ctx, gel);

	if (fz_aa_bits > 0)
	{
		void *fn;
		if (color)
			fn = (void *)fz_get_span_color_painter(dst->n, dst->alpha, color, eop);
		else
			fn = (void *)fz_get_span_painter(dst->alpha, 1, 0, 255, eop);
		assert(fn);
		if (fn == NULL)
			return;
		fz_scan_convert_aa(ctx, gel, eofill, clip, dst, color, fn, eop);
	}
	else
	{
		fz_solid_color_painter_t *fn = fz_get_solid_color_painter(dst->n, color, dst->alpha, eop);
		assert(fn);
		if (fn == NULL)
			return;
		fz_scan_convert_sharp(ctx, gel, eofill, clip, dst, color, (fz_solid_color_painter_t *)fn, eop);
	}
}

static const fz_rasterizer_fns gel_rasterizer =
{
	fz_drop_gel,
	fz_reset_gel,
	NULL, /* postindex */
	fz_insert_gel,
	fz_insert_gel_rect,
	NULL, /* gap */
	fz_convert_gel,
	fz_is_rect_gel,
	0 /* Not reusable */
};

fz_rasterizer *
fz_new_gel(fz_context *ctx)
{
	fz_gel *gel;

	gel = fz_new_derived_rasterizer(ctx, fz_gel, &gel_rasterizer);
	fz_try(ctx)
	{
		gel->edges = NULL;
		gel->cap = 512;
		gel->len = 0;
		gel->edges = Memento_label(fz_malloc_array(ctx, gel->cap, fz_edge), "gel_edges");

		gel->acap = 64;
		gel->alen = 0;
		gel->active = Memento_label(fz_malloc_array(ctx, gel->acap, fz_edge*), "gel_active");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, gel->edges);
		fz_free(ctx, gel);
		fz_rethrow(ctx);
	}

	return &gel->super;
}
