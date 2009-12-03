#include "fitz_base.h"
#include "fitz_tree.h"
#include "fitz_draw.h"

/* divide and floor towards -inf */
static inline int fz_idiv(int a, int b)
{
	return a < 0 ? (a - b + 1) / b : a / b;
}

enum { HSCALE = 17, VSCALE = 15, SF = 1 };

/*
 * Global Edge List -- list of straight path segments for scan conversion
 *
 * Stepping along the edges is with bresenham's line algorithm.
 *
 * See Mike Abrash -- Graphics Programming Black Book (notably chapter 40)
 */

fz_error
fz_newgel(fz_gel **gelp)
{
	fz_gel *gel;

	gel = *gelp = fz_malloc(sizeof(fz_gel));
	if (!gel)
		return fz_rethrow(-1, "out of memory");

	gel->edges = nil;

	gel->cap = 512;
	gel->len = 0;
	gel->edges = fz_malloc(sizeof(fz_edge) * gel->cap);
	if (!gel->edges) {
		fz_free(gel);
		return fz_rethrow(-1, "out of memory");
	}

	gel->clip.x0 = gel->clip.y0 = INT_MAX;
	gel->clip.x1 = gel->clip.y1 = INT_MIN;

	gel->bbox.x0 = gel->bbox.y0 = INT_MAX;
	gel->bbox.x1 = gel->bbox.y1 = INT_MIN;

	return fz_okay;
}

void
fz_resetgel(fz_gel *gel, fz_irect clip)
{
	if (fz_isinfiniterect(clip))
	{
		gel->clip.x0 = gel->clip.y0 = INT_MAX;
		gel->clip.x1 = gel->clip.y1 = INT_MIN;
	}
	else {
		gel->clip.x0 = clip.x0 * HSCALE;
		gel->clip.x1 = clip.x1 * HSCALE;
		gel->clip.y0 = clip.y0 * VSCALE;
		gel->clip.y1 = clip.y1 * VSCALE;
	}

	gel->bbox.x0 = gel->bbox.y0 = INT_MAX;
	gel->bbox.x1 = gel->bbox.y1 = INT_MIN;

	gel->len = 0;
}

void
fz_dropgel(fz_gel *gel)
{
	fz_free(gel->edges);
	fz_free(gel);
}

fz_irect
fz_boundgel(fz_gel *gel)
{
	fz_irect bbox;
	bbox.x0 = fz_idiv(gel->bbox.x0, HSCALE);
	bbox.y0 = fz_idiv(gel->bbox.y0, VSCALE);
	bbox.x1 = fz_idiv(gel->bbox.x1, HSCALE) + 1;
	bbox.y1 = fz_idiv(gel->bbox.y1, VSCALE) + 1;
	return bbox;
}

enum { INSIDE, OUTSIDE, LEAVE, ENTER };

#define cliplerpy(v,m,x0,y0,x1,y1,t) cliplerpx(v,m,y0,x0,y1,x1,t)

static int
cliplerpx(int val, int m, int x0, int y0, int x1, int y1, int *out)
{
	int v0out = m ? x0 > val : x0 < val;
	int v1out = m ? x1 > val : x1 < val;

	if (v0out + v1out == 0)
		return INSIDE;

	if (v0out + v1out == 2)
		return OUTSIDE;

	if (v1out)
	{
		*out = y0 + (y1 - y0) * (val - x0) / (x1 - x0);
		return LEAVE;
	}

	else
	{
		*out = y1 + (y0 - y1) * (val - x1) / (x0 - x1);
		return ENTER;
	}
}

fz_error
fz_insertgel(fz_gel *gel, float fx0, float fy0, float fx1, float fy1)
{
	fz_edge *edge;
	int dx, dy;
	int winding;
	int width;
	int tmp;
	int v;
	int d;

	int x0 = fz_floor(fx0 * HSCALE);
	int y0 = fz_floor(fy0 * VSCALE);
	int x1 = fz_floor(fx1 * HSCALE);
	int y1 = fz_floor(fy1 * VSCALE);

	d = cliplerpy(gel->clip.y0, 0, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) return fz_okay;
	if (d == LEAVE) { y1 = gel->clip.y0; x1 = v; }
	if (d == ENTER) { y0 = gel->clip.y0; x0 = v; }

	d = cliplerpy(gel->clip.y1, 1, x0, y0, x1, y1, &v);
	if (d == OUTSIDE) return fz_okay;
	if (d == LEAVE) { y1 = gel->clip.y1; x1 = v; }
	if (d == ENTER) { y0 = gel->clip.y1; x0 = v; }

	if (y0 == y1)
		return fz_okay;

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
		int newcap = gel->cap + 512;
		fz_edge *newedges = fz_realloc(gel->edges, sizeof(fz_edge) * newcap);
		if (!newedges)
			return fz_rethrow(-1, "out of memory");
		gel->cap = newcap;
		gel->edges = newedges;
	}

	edge = &gel->edges[gel->len++];

	dy = y1 - y0;
	dx = x1 - x0;
	width = dx < 0 ? -dx : dx;

	edge->xdir = dx > 0 ? 1 : -1;
	edge->ydir = winding;
	edge->x = x0;
	edge->y = y0;
	edge->h = dy;
	edge->adjdown = dy;

	/* initial error term going l->r and r->l */
	if (dx >= 0)
		edge->e = 0;
	else
		edge->e = -dy + 1;

	/* y-major edge */
	if (dy >= width) {
		edge->xmove = 0;
		edge->adjup = width;
	}

	/* x-major edge */
	else {
		edge->xmove = (width / dy) * edge->xdir;
		edge->adjup = width % dy;
	}

	return fz_okay;
}

void
fz_sortgel(fz_gel *gel)
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

/*
 * Active Edge List -- keep track of active edges while sweeping
 */

fz_error
fz_newael(fz_ael **aelp)
{
	fz_ael *ael;

	ael = *aelp = fz_malloc(sizeof(fz_ael));
	if (!ael)
		return fz_rethrow(-1, "out of memory");

	ael->cap = 64;
	ael->len = 0;
	ael->edges = fz_malloc(sizeof(fz_edge*) * ael->cap);
	if (!ael->edges) {
		fz_free(ael);
		return fz_rethrow(-1, "out of memory");
	}

	return fz_okay;
}

void
fz_dropael(fz_ael *ael)
{
	fz_free(ael->edges);
	fz_free(ael);
}

static inline void
sortael(fz_edge **a, int n)
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

static fz_error
insertael(fz_ael *ael, fz_gel *gel, int y, int *e)
{
	/* insert edges that start here */
	while (*e < gel->len && gel->edges[*e].y == y) {
		if (ael->len + 1 == ael->cap) {
			int newcap = ael->cap + 64;
			fz_edge **newedges = fz_realloc(ael->edges, sizeof(fz_edge*) * newcap);
			if (!newedges)
				return fz_rethrow(-1, "out of memory");
			ael->edges = newedges;
			ael->cap = newcap;
		}
		ael->edges[ael->len++] = &gel->edges[(*e)++];
	}

	/* shell-sort the edges by increasing x */
	sortael(ael->edges, ael->len);

	return fz_okay;
}

static void
advanceael(fz_ael *ael)
{
	fz_edge *edge;
	int i = 0;

	while (i < ael->len)
	{
		edge = ael->edges[i];

		edge->h --;

		/* terminator! */
		if (edge->h == 0) {
			ael->edges[i] = ael->edges[--ael->len];
		}

		else {
			edge->x += edge->xmove;
			edge->e += edge->adjup;
			if (edge->e > 0) {
				edge->x += edge->xdir;
				edge->e -= edge->adjdown;
			}
			i ++;
		}
	}
}

/*
 * Scan convert
 */

static inline void
addspan(unsigned char *list, int x0, int x1, int xofs)
{
	int x0pix, x0sub;
	int x1pix, x1sub;

	if (x0 == x1)
		return;

	/* x between 0 and width of bbox */
	x0 -= xofs;
	x1 -= xofs;

	x0pix = x0 / HSCALE;
	x0sub = x0 % HSCALE;
	x1pix = x1 / HSCALE;
	x1sub = x1 % HSCALE;

	if (x0pix == x1pix)
	{
		list[x0pix] += x1sub - x0sub;
		list[x0pix+1] += x0sub - x1sub;
	}

	else
	{
		list[x0pix] += HSCALE - x0sub;
		list[x0pix+1] += x0sub;
		list[x1pix] += x1sub - HSCALE;
		list[x1pix+1] += -x1sub;
	}
}

static inline void
nonzerowinding(fz_ael *ael, unsigned char *list, int xofs)
{
	int winding = 0;
	int x = 0;
	int i;
	for (i = 0; i < ael->len; i++)
	{
		if (!winding && (winding + ael->edges[i]->ydir))
			x = ael->edges[i]->x;
		if (winding && !(winding + ael->edges[i]->ydir))
			addspan(list, x, ael->edges[i]->x, xofs);
		winding += ael->edges[i]->ydir;
	}
}

static inline void
evenodd(fz_ael *ael, unsigned char *list, int xofs)
{
	int even = 0;
	int x = 0;
	int i;
	for (i = 0; i < ael->len; i++)
	{
		if (!even)
			x = ael->edges[i]->x;
		else
			addspan(list, x, ael->edges[i]->x, xofs);
		even = !even;
	}
}

static inline void toalpha(unsigned char *list, int n)
{
	int d = 0;
	while (n--)
	{
		d += *list;
		*list++ = d;
	}
}

static inline void blit(fz_pixmap *pix, int x, int y,
	unsigned char *list, int skipx, int len,
	unsigned char *argb, int over)
{
	unsigned char *dst;
	unsigned char cov;

	dst = pix->samples + ( (y - pix->y) * pix->w + (x - pix->x) ) * pix->n;
	cov = 0;

	while (skipx--)
	{
		cov += *list;
		*list = 0;
		++list;
	}

	if (argb)
		fz_path_w4i1o4(argb, list, cov, len, dst);
	else if (over)
		fz_path_1o1(list, cov, len, dst);
	else
		fz_path_1c1(list, cov, len, dst);
}

fz_error
fz_scanconvert(fz_gel *gel, fz_ael *ael, int eofill, fz_irect clip,
	fz_pixmap *pix, unsigned char *argb, int over)
{
	fz_error error;
	unsigned char *deltas;
	int y, e;
	int yd, yc;

	int xmin = fz_idiv(gel->bbox.x0, HSCALE);
	int xmax = fz_idiv(gel->bbox.x1, HSCALE) + 1;

	int xofs = xmin * HSCALE;

	int skipx = clip.x0 - xmin;
	int clipn = clip.x1 - clip.x0;

	assert(clip.x0 >= xmin);
	assert(clip.x1 <= xmax);

	if (gel->len == 0)
		return fz_okay;

	deltas = fz_malloc(xmax - xmin + 1);
	if (!deltas)
		return fz_rethrow(-1, "out of memory");

	memset(deltas, 0, xmax - xmin + 1);

	e = 0;
	y = gel->edges[0].y;
	yc = fz_idiv(y, VSCALE);
	yd = yc;

	while (ael->len > 0 || e < gel->len)
	{
		yc = fz_idiv(y, VSCALE);
		if (yc != yd)
		{
			if (yd >= clip.y0 && yd < clip.y1)
			{
				blit(pix, xmin + skipx, yd, deltas, skipx, clipn, argb, over);
			}
		}
		yd = yc;

		error = insertael(ael, gel, y, &e);
		if (error) {
			fz_free(deltas);
			return error;
		}

		if (yd >= clip.y0 && yd < clip.y1)
		{
			if (eofill)
				evenodd(ael, deltas, xofs);
			else
				nonzerowinding(ael, deltas, xofs);
		}

		advanceael(ael);

		if (ael->len > 0)
			y ++;
		else if (e < gel->len)
			y = gel->edges[e].y;
	}

	if (yd >= clip.y0 && yd < clip.y1)
	{
		blit(pix, xmin + skipx, yd, deltas, skipx, clipn, argb, over);
	}

	fz_free(deltas);
	return fz_okay;
}

