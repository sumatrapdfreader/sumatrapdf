#include "mupdf/fitz.h"
#include "draw-imp.h"

enum { MAXN = 2 + FZ_MAX_COLORS };

static void paint_scan(fz_pixmap *restrict pix, int y, int fx0, int fx1, int cx0, int cx1, const int *restrict v0, const int *restrict v1, int n)
{
	unsigned char *p;
	int c[MAXN], dc[MAXN];
	int k, w;
	float div, mul;
	int x0, x1;

	/* Ensure that fx0 is left edge, and fx1 is right */
	if (fx0 > fx1)
	{
		const int *v;
		int t = fx0; fx0 = fx1; fx1 = t;
		v = v0; v0 = v1; v1 = v;
	}
	else if (fx0 == fx1)
		return;

	/* Clip fx0, fx1 to range */
	if (fx0 >= cx1)
		return;
	if (fx1 <= cx0)
		return;
	x0 = (fx0 > cx0 ? fx0 : cx0);
	x1 = (fx1 < cx1 ? fx1 : cx1);

	w = x1 - x0;
	if (w == 0)
		return;

	div = 1.0f / (fx1 - fx0);
	mul = (x0 - fx0);
	for (k = 0; k < n; k++)
	{
		dc[k] = (v1[k] - v0[k]) * div;
		c[k] = v0[k] + dc[k] * mul;
	}

	p = pix->samples + ((x0 - pix->x) + (y - pix->y) * pix->w) * pix->n;
	while (w--)
	{
		for (k = 0; k < n; k++)
		{
			*p++ = c[k]>>16;
			c[k] += dc[k];
		}
		*p++ = 255;
	}
}

typedef struct edge_data_s edge_data;

struct edge_data_s
{
	float x;
	float dx;
	int v[2*MAXN];
};

static inline void prepare_edge(const float *restrict vtop, const float *restrict vbot, edge_data *restrict edge, float y, int n)
{
	float r = 1.0f / (vbot[1] - vtop[1]);
	float t = (y - vtop[1]) * r;
	float diff = vbot[0] - vtop[0];
	int i;

	edge->x = vtop[0] + diff * t;
	edge->dx = diff * r;

	for (i = 0; i < n; i++)
	{
		diff = vbot[i+2] - vtop[i+2];
		edge->v[i] = (int)(65536.0f * (vtop[i+2] + diff * t));
		edge->v[i+MAXN] = (int)(65536.0f * diff * r);
	}
}

static inline void step_edge(edge_data *edge, int n)
{
	int i;

	edge->x += edge->dx;

	for (i = 0; i < n; i++)
	{
		edge->v[i] += edge->v[i + MAXN];
	}
}

static void
fz_paint_triangle(fz_pixmap *pix, float v[3][MAXN], int n, const fz_irect *bbox)
{
	edge_data e0, e1;
	int top, mid, bot;
	float y, y1;
	int minx, maxx;

	top = bot = 0;
	if (v[1][1] < v[0][1]) top = 1; else bot = 1;
	if (v[2][1] < v[top][1]) top = 2;
	else if (v[2][1] > v[bot][1]) bot = 2;
	if (v[top][1] == v[bot][1]) return;

	/* Test if the triangle is completely outside the scissor rect */
	if (v[bot][1] < bbox->y0) return;
	if (v[top][1] > bbox->y1) return;

	/* Magic! Ensure that mid/top/bot are all different */
	mid = 3^top^bot;

	assert(top != bot && top != mid && mid != bot);

	minx = fz_maxi(bbox->x0, pix->x);
	maxx = fz_mini(bbox->x1, pix->x + pix->w);

	y = ceilf(fz_max(bbox->y0, v[top][1]));
	y1 = ceilf(fz_min(bbox->y1, v[mid][1]));

	n -= 2;
	prepare_edge(v[top], v[bot], &e0, y, n);
	if (y < y1)
	{
		prepare_edge(v[top], v[mid], &e1, y, n);

		do
		{
			paint_scan(pix, y, (int)e0.x, (int)e1.x, minx, maxx, &e0.v[0], &e1.v[0], n);
			step_edge(&e0, n);
			step_edge(&e1, n);
			y ++;
		}
		while (y < y1);
	}

	y1 = ceilf(fz_min(bbox->y1, v[bot][1]));
	if (y < y1)
	{
		prepare_edge(v[mid], v[bot], &e1, y, n);

		do
		{
			paint_scan(pix, y, (int)e0.x, (int)e1.x, minx, maxx, &e0.v[0], &e1.v[0], n);
			y ++;
			if (y >= y1)
				break;
			step_edge(&e0, n);
			step_edge(&e1, n);
		}
		while (1);
	}
}

struct paint_tri_data
{
	fz_context *ctx;
	fz_shade *shade;
	fz_pixmap *dest;
	const fz_irect *bbox;
};

static void
do_paint_tri(void *arg, fz_vertex *av, fz_vertex *bv, fz_vertex *cv)
{
	struct paint_tri_data *ptd = (struct paint_tri_data *)arg;
	int i, k;
	fz_vertex *vertices[3];
	fz_vertex *v;
	float *ltri;
	fz_context *ctx;
	fz_shade *shade;
	fz_pixmap *dest;
	float local[3][MAXN];

	vertices[0] = av;
	vertices[1] = bv;
	vertices[2] = cv;

	dest = ptd->dest;
	ctx = ptd->ctx;
	shade = ptd->shade;
	for (k = 0; k < 3; k++)
	{
		v = vertices[k];
		ltri = &local[k][0];
		ltri[0] = v->p.x;
		ltri[1] = v->p.y;
		if (shade->use_function)
			ltri[2] = v->c[0] * 255;
		else
		{
			fz_convert_color(ctx, dest->colorspace, &ltri[2], shade->colorspace, v->c);
			for (i = 0; i < dest->colorspace->n; i++)
				ltri[i + 2] *= 255;
		}
	}
	fz_paint_triangle(dest, local, 2 + dest->colorspace->n, ptd->bbox);
}

void
fz_paint_shade(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_pixmap *dest, const fz_irect *bbox)
{
	unsigned char clut[256][FZ_MAX_COLORS];
	fz_pixmap *temp = NULL;
	fz_pixmap *conv = NULL;
	float color[FZ_MAX_COLORS];
	struct paint_tri_data ptd;
	int i, k;
	fz_matrix local_ctm;

	fz_var(temp);
	fz_var(conv);

	fz_try(ctx)
	{
		fz_concat(&local_ctm, &shade->matrix, ctm);

		if (shade->use_function)
		{
			fz_color_converter cc;
			fz_lookup_color_converter(&cc, ctx, dest->colorspace, shade->colorspace);
			for (i = 0; i < 256; i++)
			{
				cc.convert(&cc, color, shade->function[i]);
				for (k = 0; k < dest->colorspace->n; k++)
					clut[i][k] = color[k] * 255;
				clut[i][k] = shade->function[i][shade->colorspace->n] * 255;
			}
			conv = fz_new_pixmap_with_bbox(ctx, dest->colorspace, bbox);
			temp = fz_new_pixmap_with_bbox(ctx, fz_device_gray(ctx), bbox);
			fz_clear_pixmap(ctx, temp);
		}
		else
		{
			temp = dest;
		}

		ptd.ctx = ctx;
		ptd.dest = temp;
		ptd.shade = shade;
		ptd.bbox = bbox;

		fz_process_mesh(ctx, shade, &local_ctm, &do_paint_tri, &ptd);

		if (shade->use_function)
		{
			unsigned char *s = temp->samples;
			unsigned char *d = conv->samples;
			int len = temp->w * temp->h;
			while (len--)
			{
				int v = *s++;
				int a = fz_mul255(*s++, clut[v][conv->n - 1]);
				for (k = 0; k < conv->n - 1; k++)
					*d++ = fz_mul255(clut[v][k], a);
				*d++ = a;
			}
			fz_paint_pixmap(dest, conv, 255);
			fz_drop_pixmap(ctx, conv);
			fz_drop_pixmap(ctx, temp);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, conv);
		fz_drop_pixmap(ctx, temp);
		fz_rethrow(ctx);
	}
}
