#include "fitz.h"

/*
 * polygon clipping
 */

enum { IN, OUT, ENTER, LEAVE };
enum { MAXV = 3 + 4 };
enum { MAXN = 2 + FZ_MAX_COLORS };

static int clipx(float val, int ismax, float *v1, float *v2, int n)
{
	float t;
	int i;
	int v1o = ismax ? v1[0] > val : v1[0] < val;
	int v2o = ismax ? v2[0] > val : v2[0] < val;
	if (v1o + v2o == 0)
		return IN;
	if (v1o + v2o == 2)
		return OUT;
	if (v2o)
	{
		t = (val - v1[0]) / (v2[0] - v1[0]);
		v2[0] = val;
		v2[1] = v1[1] + t * (v2[1] - v1[1]);
		for (i = 2; i < n; i++)
			v2[i] = v1[i] + t * (v2[i] - v1[i]);
		return LEAVE;
	}
	else
	{
		t = (val - v2[0]) / (v1[0] - v2[0]);
		v1[0] = val;
		v1[1] = v2[1] + t * (v1[1] - v2[1]);
		for (i = 2; i < n; i++)
			v1[i] = v2[i] + t * (v1[i] - v2[i]);
		return ENTER;
	}
}

static int clipy(float val, int ismax, float *v1, float *v2, int n)
{
	float t;
	int i;
	int v1o = ismax ? v1[1] > val : v1[1] < val;
	int v2o = ismax ? v2[1] > val : v2[1] < val;
	if (v1o + v2o == 0)
		return IN;
	if (v1o + v2o == 2)
		return OUT;
	if (v2o)
	{
		t = (val - v1[1]) / (v2[1] - v1[1]);
		v2[0] = v1[0] + t * (v2[0] - v1[0]);
		v2[1] = val;
		for (i = 2; i < n; i++)
			v2[i] = v1[i] + t * (v2[i] - v1[i]);
		return LEAVE;
	}
	else
	{
		t = (val - v2[1]) / (v1[1] - v2[1]);
		v1[0] = v2[0] + t * (v1[0] - v2[0]);
		v1[1] = val;
		for (i = 2; i < n; i++)
			v1[i] = v2[i] + t * (v1[i] - v2[i]);
		return ENTER;
	}
}

static inline void copy_vert(float *dst, float *src, int n)
{
	while (n--)
		*dst++ = *src++;
}

static int clip_poly(float src[MAXV][MAXN],
	float dst[MAXV][MAXN], int len, int n,
	float val, int isy, int ismax)
{
	float cv1[MAXN];
	float cv2[MAXN];
	int v1, v2, cp;
	int r;

	v1 = len - 1;
	cp = 0;

	for (v2 = 0; v2 < len; v2++)
	{
		copy_vert(cv1, src[v1], n);
		copy_vert(cv2, src[v2], n);

		if (isy)
			r = clipy(val, ismax, cv1, cv2, n);
		else
			r = clipx(val, ismax, cv1, cv2, n);

		switch (r)
		{
		case IN:
			copy_vert(dst[cp++], cv2, n);
			break;
		case OUT:
			break;
		case LEAVE:
			copy_vert(dst[cp++], cv2, n);
			break;
		case ENTER:
			copy_vert(dst[cp++], cv1, n);
			copy_vert(dst[cp++], cv2, n);
			break;
		}
		v1 = v2;
	}

	return cp;
}

/*
 * gouraud shaded polygon scan conversion
 */

static void paint_scan(fz_pixmap *pix, int y, int x1, int x2, int *v1, int *v2, int n)
{
	unsigned char *p = pix->samples + ((y - pix->y) * pix->w + (x1 - pix->x)) * pix->n;
	int v[FZ_MAX_COLORS];
	int dv[FZ_MAX_COLORS];
	int w = x2 - x1;
	int k;

	assert(w >= 0);
	assert(y >= pix->y);
	assert(y < pix->y + pix->h);
	assert(x1 >= pix->x);
	assert(x2 <= pix->x + pix->w);

	if (w == 0)
		return;

	for (k = 0; k < n; k++)
	{
		v[k] = v1[k];
		dv[k] = (v2[k] - v1[k]) / w;
	}

	while (w--)
	{
		for (k = 0; k < n; k++)
		{
			*p++ = v[k] >> 16;
			v[k] += dv[k];
		}
		*p++ = 255;
	}
}

static int find_next(int gel[MAXV][MAXN], int len, int a, int *s, int *e, int d)
{
	int b;

	while (1)
	{
		b = a + d;
		if (b == len)
			b = 0;
		if (b == -1)
			b = len - 1;

		if (gel[b][1] == gel[a][1])
		{
			a = b;
			continue;
		}

		if (gel[b][1] > gel[a][1])
		{
			*s = a;
			*e = b;
			return 0;
		}

		return 1;
	}
}

static void load_edge(int gel[MAXV][MAXN], int s, int e, int *ael, int *del, int n)
{
	int swp, k, dy;

	if (gel[s][1] > gel[e][1])
	{
		swp = s; s = e; e = swp;
	}

	dy = gel[e][1] - gel[s][1];

	ael[0] = gel[s][0];
	del[0] = (gel[e][0] - gel[s][0]) / dy;
	for (k = 2; k < n; k++)
	{
		ael[k] = gel[s][k];
		del[k] = (gel[e][k] - gel[s][k]) / dy;
	}
}

static inline void step_edge(int *ael, int *del, int n)
{
	int k;
	ael[0] += del[0];
	for (k = 2; k < n; k++)
		ael[k] += del[k];
}

static void
fz_paint_triangle(fz_pixmap *pix, float *av, float *bv, float *cv, int n, fz_bbox bbox)
{
	float poly[MAXV][MAXN];
	float temp[MAXV][MAXN];
	float cx0 = bbox.x0;
	float cy0 = bbox.y0;
	float cx1 = bbox.x1;
	float cy1 = bbox.y1;

	int gel[MAXV][MAXN];
	int ael[2][MAXN];
	int del[2][MAXN];
	int y, s0, s1, e0, e1;
	int top, bot, len;

	int i, k;

	copy_vert(poly[0], av, n);
	copy_vert(poly[1], bv, n);
	copy_vert(poly[2], cv, n);

	len = clip_poly(poly, temp, 3, n, cx0, 0, 0);
	len = clip_poly(temp, poly, len, n, cx1, 0, 1);
	len = clip_poly(poly, temp, len, n, cy0, 1, 0);
	len = clip_poly(temp, poly, len, n, cy1, 1, 1);

	if (len < 3)
		return;

	for (i = 0; i < len; i++)
	{
		gel[i][0] = floorf(poly[i][0] + 0.5f) * 65536; /* trunc and fix */
		gel[i][1] = floorf(poly[i][1] + 0.5f);	/* y is not fixpoint */
		for (k = 2; k < n; k++)
			gel[i][k] = poly[i][k] * 65536;	/* fix with precision */
	}

	top = bot = 0;
	for (i = 0; i < len; i++)
	{
		if (gel[i][1] < gel[top][1])
			top = i;
		if (gel[i][1] > gel[bot][1])
			bot = i;
	}

	if (gel[bot][1] - gel[top][1] == 0)
		return;

	y = gel[top][1];

	if (find_next(gel, len, top, &s0, &e0, 1))
		return;
	if (find_next(gel, len, top, &s1, &e1, -1))
		return;

	load_edge(gel, s0, e0, ael[0], del[0], n);
	load_edge(gel, s1, e1, ael[1], del[1], n);

	while (1)
	{
		int x0 = ael[0][0] >> 16;
		int x1 = ael[1][0] >> 16;

		if (ael[0][0] < ael[1][0])
			paint_scan(pix, y, x0, x1, ael[0]+2, ael[1]+2, n-2);
		else
			paint_scan(pix, y, x1, x0, ael[1]+2, ael[0]+2, n-2);

		step_edge(ael[0], del[0], n);
		step_edge(ael[1], del[1], n);
		y ++;

		if (y >= gel[e0][1])
		{
			if (find_next(gel, len, e0, &s0, &e0, 1))
				return;
			load_edge(gel, s0, e0, ael[0], del[0], n);
		}

		if (y >= gel[e1][1])
		{
			if (find_next(gel, len, e1, &s1, &e1, -1))
				return;
			load_edge(gel, s1, e1, ael[1], del[1], n);
		}
	}
}

static void
fz_paint_quad(fz_pixmap *pix,
		fz_point p0, fz_point p1, fz_point p2, fz_point p3,
		float c0, float c1, float c2, float c3,
		int n, fz_bbox bbox)
{
	float v[4][3];

	v[0][0] = p0.x;
	v[0][1] = p0.y;
	v[0][2] = c0;

	v[1][0] = p1.x;
	v[1][1] = p1.y;
	v[1][2] = c1;

	v[2][0] = p2.x;
	v[2][1] = p2.y;
	v[2][2] = c2;

	v[3][0] = p3.x;
	v[3][1] = p3.y;
	v[3][2] = c3;

	fz_paint_triangle(pix, v[0], v[2], v[3], n, bbox);
	fz_paint_triangle(pix, v[0], v[3], v[1], n, bbox);
}

/*
 * linear, radial and mesh painting
 */

#define HUGENUM 32000 /* how far to extend axial/radial shadings */
#define RADSEGS 32 /* how many segments to generate for radial meshes */

static fz_point
fz_point_on_circle(fz_point p, float r, float theta)
{
	p.x = p.x + cosf(theta) * r;
	p.y = p.y + sinf(theta) * r;

	return p;
}

static void
fz_paint_linear(fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	fz_point p0, p1;
	fz_point v0, v1, v2, v3;
	fz_point e0, e1;
	float theta;

	p0.x = shade->mesh[0];
	p0.y = shade->mesh[1];
	p0 = fz_transform_point(ctm, p0);

	p1.x = shade->mesh[3];
	p1.y = shade->mesh[4];
	p1 = fz_transform_point(ctm, p1);

	theta = atan2f(p1.y - p0.y, p1.x - p0.x);
	theta += (float)M_PI * 0.5f;

	v0 = fz_point_on_circle(p0, HUGENUM, theta);
	v1 = fz_point_on_circle(p1, HUGENUM, theta);
	v2 = fz_point_on_circle(p0, -HUGENUM, theta);
	v3 = fz_point_on_circle(p1, -HUGENUM, theta);

	fz_paint_quad(dest, v0, v1, v2, v3, 0, 255, 0, 255, 3, bbox);

	if (shade->extend[0])
	{
		e0.x = v0.x - (p1.x - p0.x) * HUGENUM;
		e0.y = v0.y - (p1.y - p0.y) * HUGENUM;

		e1.x = v2.x - (p1.x - p0.x) * HUGENUM;
		e1.y = v2.y - (p1.y - p0.y) * HUGENUM;

		fz_paint_quad(dest, e0, e1, v0, v2, 0, 0, 0, 0, 3, bbox);
	}

	if (shade->extend[1])
	{
		e0.x = v1.x + (p1.x - p0.x) * HUGENUM;
		e0.y = v1.y + (p1.y - p0.y) * HUGENUM;

		e1.x = v3.x + (p1.x - p0.x) * HUGENUM;
		e1.y = v3.y + (p1.y - p0.y) * HUGENUM;

		fz_paint_quad(dest, e0, e1, v1, v3, 255, 255, 255, 255, 3, bbox);
	}
}

static void
fz_paint_annulus(fz_matrix ctm,
		fz_point p0, float r0, float c0,
		fz_point p1, float r1, float c1,
		fz_pixmap *dest, fz_bbox bbox)
{
	fz_point t0, t1, t2, t3, b0, b1, b2, b3;
	float theta, step;
	int i;

	theta = atan2f(p1.y - p0.y, p1.x - p0.x);
	step = (float)M_PI * 2 / RADSEGS;

	for (i = 0; i < RADSEGS / 2; i++)
	{
		t0 = fz_point_on_circle(p0, r0, theta + i * step);
		t1 = fz_point_on_circle(p0, r0, theta + i * step + step);
		t2 = fz_point_on_circle(p1, r1, theta + i * step);
		t3 = fz_point_on_circle(p1, r1, theta + i * step + step);
		b0 = fz_point_on_circle(p0, r0, theta - i * step);
		b1 = fz_point_on_circle(p0, r0, theta - i * step - step);
		b2 = fz_point_on_circle(p1, r1, theta - i * step);
		b3 = fz_point_on_circle(p1, r1, theta - i * step - step);

		t0 = fz_transform_point(ctm, t0);
		t1 = fz_transform_point(ctm, t1);
		t2 = fz_transform_point(ctm, t2);
		t3 = fz_transform_point(ctm, t3);
		b0 = fz_transform_point(ctm, b0);
		b1 = fz_transform_point(ctm, b1);
		b2 = fz_transform_point(ctm, b2);
		b3 = fz_transform_point(ctm, b3);

		fz_paint_quad(dest, t0, t1, t2, t3, c0, c0, c1, c1, 3, bbox);
		fz_paint_quad(dest, b0, b1, b2, b3, c0, c0, c1, c1, 3, bbox);
	}
}

static void
fz_paint_radial(fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	fz_point p0, p1;
	float r0, r1;
	fz_point e;
	float er, rs;

	p0.x = shade->mesh[0];
	p0.y = shade->mesh[1];
	r0 = shade->mesh[2];

	p1.x = shade->mesh[3];
	p1.y = shade->mesh[4];
	r1 = shade->mesh[5];

	if (shade->extend[0])
	{
		if (r0 < r1)
			rs = r0 / (r0 - r1);
		else
			rs = -HUGENUM;

		e.x = p0.x + (p1.x - p0.x) * rs;
		e.y = p0.y + (p1.y - p0.y) * rs;
		er = r0 + (r1 - r0) * rs;

		fz_paint_annulus(ctm, e, er, 0, p0, r0, 0, dest, bbox);
	}

	fz_paint_annulus(ctm, p0, r0, 0, p1, r1, 255, dest, bbox);

	if (shade->extend[1])
	{
		if (r0 > r1)
			rs = r1 / (r1 - r0);
		else
			rs = -HUGENUM;

		e.x = p1.x + (p0.x - p1.x) * rs;
		e.y = p1.y + (p0.y - p1.y) * rs;
		er = r1 + (r0 - r1) * rs;

		fz_paint_annulus(ctm, p1, r1, 255, e, er, 255, dest, bbox);
	}
}

static void
fz_paint_mesh(fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	float tri[3][MAXN];
	fz_point p;
	float *mesh;
	int ntris;
	int i, k;

	mesh = shade->mesh;

	if (shade->use_function)
		ntris = shade->mesh_len / 9;
	else
		ntris = shade->mesh_len / ((2 + shade->colorspace->n) * 3);

	while (ntris--)
	{
		for (k = 0; k < 3; k++)
		{
			p.x = *mesh++;
			p.y = *mesh++;
			p = fz_transform_point(ctm, p);
			tri[k][0] = p.x;
			tri[k][1] = p.y;
			if (shade->use_function)
				tri[k][2] = *mesh++ * 255;
			else
			{
				fz_convert_color(shade->colorspace, mesh, dest->colorspace, tri[k] + 2);
				for (i = 0; i < dest->colorspace->n; i++)
					tri[k][i + 2] *= 255;
				mesh += shade->colorspace->n;
			}
		}
		fz_paint_triangle(dest, tri[0], tri[1], tri[2], 2 + dest->colorspace->n, bbox);
	}
}

void
fz_paint_shade(fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox)
{
	unsigned char clut[256][FZ_MAX_COLORS];
	fz_pixmap *temp, *conv;
	float color[FZ_MAX_COLORS];
	int i, k;

	ctm = fz_concat(shade->matrix, ctm);

	if (shade->use_function)
	{
		for (i = 0; i < 256; i++)
		{
			fz_convert_color(shade->colorspace, shade->function[i], dest->colorspace, color);
			for (k = 0; k < dest->colorspace->n; k++)
				clut[i][k] = color[k] * 255;
			clut[i][k] = shade->function[i][shade->colorspace->n] * 255;
		}
		conv = fz_new_pixmap_with_rect(dest->colorspace, bbox);
		temp = fz_new_pixmap_with_rect(fz_device_gray, bbox);
		fz_clear_pixmap(temp);
	}
	else
	{
		temp = dest;
	}

	switch (shade->type)
	{
	case FZ_LINEAR: fz_paint_linear(shade, ctm, temp, bbox); break;
	case FZ_RADIAL: fz_paint_radial(shade, ctm, temp, bbox); break;
	case FZ_MESH: fz_paint_mesh(shade, ctm, temp, bbox); break;
	}

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
		fz_drop_pixmap(conv);
		fz_drop_pixmap(temp);
	}
}
