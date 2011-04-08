#include "fitz.h"
#include "mupdf.h"

#define HUGENUM 32000 /* how far to extend axial/radial shadings */
#define FUNSEGS 32 /* size of sampled mesh for function-based shadings */
#define RADSEGS 32 /* how many segments to generate for radial meshes */
#define SUBDIV 3 /* how many levels to subdivide patches */

struct vertex
{
	float x, y;
	float c[FZ_MAX_COLORS];
};

static void
pdf_grow_mesh(fz_shade *shade, int amount)
{
	if (shade->mesh_len + amount < shade->mesh_cap)
		return;

	if (shade->mesh_cap == 0)
		shade->mesh_cap = 1024;

	while (shade->mesh_len + amount > shade->mesh_cap)
		shade->mesh_cap = (shade->mesh_cap * 3) / 2;

	shade->mesh = fz_realloc(shade->mesh, shade->mesh_cap, sizeof(float));
}

static void
pdf_add_vertex(fz_shade *shade, struct vertex *v)
{
	int ncomp = shade->use_function ? 1 : shade->colorspace->n;
	int i;
	pdf_grow_mesh(shade, 2 + ncomp);
	shade->mesh[shade->mesh_len++] = v->x;
	shade->mesh[shade->mesh_len++] = v->y;
	for (i = 0; i < ncomp; i++)
		shade->mesh[shade->mesh_len++] = v->c[i];
}

static void
pdf_add_triangle(fz_shade *shade,
	struct vertex *v0,
	struct vertex *v1,
	struct vertex *v2)
{
	pdf_add_vertex(shade, v0);
	pdf_add_vertex(shade, v1);
	pdf_add_vertex(shade, v2);
}

static void
pdf_add_quad(fz_shade *shade,
	struct vertex *v0,
	struct vertex *v1,
	struct vertex *v2,
	struct vertex *v3)
{
	pdf_add_triangle(shade, v0, v1, v3);
	pdf_add_triangle(shade, v1, v3, v2);
}

/* Subdivide and tesselate tensor-patches */

typedef struct pdf_tensor_patch_s pdf_tensor_patch;

struct pdf_tensor_patch_s
{
	fz_point pole[4][4];
	float color[4][FZ_MAX_COLORS];
};

static void
triangulate_patch(pdf_tensor_patch p, fz_shade *shade)
{
	struct vertex v0, v1, v2, v3;

	v0.x = p.pole[0][0].x;
	v0.y = p.pole[0][0].y;
	memcpy(v0.c, p.color[0], sizeof(v0.c));

	v1.x = p.pole[0][3].x;
	v1.y = p.pole[0][3].y;
	memcpy(v1.c, p.color[1], sizeof(v1.c));

	v2.x = p.pole[3][3].x;
	v2.y = p.pole[3][3].y;
	memcpy(v2.c, p.color[2], sizeof(v2.c));

	v3.x = p.pole[3][0].x;
	v3.y = p.pole[3][0].y;
	memcpy(v3.c, p.color[3], sizeof(v3.c));

	pdf_add_quad(shade, &v0, &v1, &v2, &v3);
}

static inline void midcolor(float *c, float *c1, float *c2)
{
	int i;
	for (i = 0; i < FZ_MAX_COLORS; i++)
		c[i] = (c1[i] + c2[i]) * 0.5f;
}

static void
split_curve(fz_point *pole, fz_point *q0, fz_point *q1, int polestep)
{
	/*
	split bezier curve given by control points pole[0]..pole[3]
	using de casteljau algo at midpoint and build two new
	bezier curves q0[0]..q0[3] and q1[0]..q1[3]. all indices
	should be multiplies by polestep == 1 for vertical bezier
	curves in patch and == 4 for horizontal bezier curves due
	to C's multi-dimensional matrix memory layout.
	*/

	float x12 = (pole[1 * polestep].x + pole[2 * polestep].x) * 0.5f;
	float y12 = (pole[1 * polestep].y + pole[2 * polestep].y) * 0.5f;

	q0[1 * polestep].x = (pole[0 * polestep].x + pole[1 * polestep].x) * 0.5f;
	q0[1 * polestep].y = (pole[0 * polestep].y + pole[1 * polestep].y) * 0.5f;
	q1[2 * polestep].x = (pole[2 * polestep].x + pole[3 * polestep].x) * 0.5f;
	q1[2 * polestep].y = (pole[2 * polestep].y + pole[3 * polestep].y) * 0.5f;

	q0[2 * polestep].x = (q0[1 * polestep].x + x12) * 0.5f;
	q0[2 * polestep].y = (q0[1 * polestep].y + y12) * 0.5f;
	q1[1 * polestep].x = (x12 + q1[2 * polestep].x) * 0.5f;
	q1[1 * polestep].y = (y12 + q1[2 * polestep].y) * 0.5f;

	q0[3 * polestep].x = (q0[2 * polestep].x + q1[1 * polestep].x) * 0.5f;
	q0[3 * polestep].y = (q0[2 * polestep].y + q1[1 * polestep].y) * 0.5f;
	q1[0 * polestep].x = (q0[2 * polestep].x + q1[1 * polestep].x) * 0.5f;
	q1[0 * polestep].y = (q0[2 * polestep].y + q1[1 * polestep].y) * 0.5f;

	q0[0 * polestep].x = pole[0 * polestep].x;
	q0[0 * polestep].y = pole[0 * polestep].y;
	q1[3 * polestep].x = pole[3 * polestep].x;
	q1[3 * polestep].y = pole[3 * polestep].y;
}

static void
split_stripe(pdf_tensor_patch *p, pdf_tensor_patch *s0, pdf_tensor_patch *s1)
{
	/*
	split all horizontal bezier curves in patch,
	creating two new patches with half the width.
	*/
	split_curve(&p->pole[0][0], &s0->pole[0][0], &s1->pole[0][0], 4);
	split_curve(&p->pole[0][1], &s0->pole[0][1], &s1->pole[0][1], 4);
	split_curve(&p->pole[0][2], &s0->pole[0][2], &s1->pole[0][2], 4);
	split_curve(&p->pole[0][3], &s0->pole[0][3], &s1->pole[0][3], 4);

	/* interpolate the colors for the two new patches. */
	memcpy(s0->color[0], p->color[0], sizeof(s0->color[0]));
	memcpy(s0->color[1], p->color[1], sizeof(s0->color[1]));
	midcolor(s0->color[2], p->color[1], p->color[2]);
	midcolor(s0->color[3], p->color[0], p->color[3]);

	memcpy(s1->color[0], s0->color[3], sizeof(s1->color[0]));
	memcpy(s1->color[1], s0->color[2], sizeof(s1->color[1]));
	memcpy(s1->color[2], p->color[2], sizeof(s1->color[2]));
	memcpy(s1->color[3], p->color[3], sizeof(s1->color[3]));
}

static void
draw_stripe(pdf_tensor_patch *p, fz_shade *shade, int depth)
{
	pdf_tensor_patch s0, s1;

	/* split patch into two half-height patches */
	split_stripe(p, &s0, &s1);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		triangulate_patch(s0, shade);
		triangulate_patch(s1, shade);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		draw_stripe(&s0, shade, depth);
		draw_stripe(&s1, shade, depth);
	}
}

static void
split_patch(pdf_tensor_patch *p, pdf_tensor_patch *s0, pdf_tensor_patch *s1)
{
	/*
	split all vertical bezier curves in patch,
	creating two new patches with half the height.
	*/
	split_curve(p->pole[0], s0->pole[0], s1->pole[0], 1);
	split_curve(p->pole[1], s0->pole[1], s1->pole[1], 1);
	split_curve(p->pole[2], s0->pole[2], s1->pole[2], 1);
	split_curve(p->pole[3], s0->pole[3], s1->pole[3], 1);

	/* interpolate the colors for the two new patches. */
	memcpy(s0->color[0], p->color[0], sizeof(s0->color[0]));
	midcolor(s0->color[1], p->color[0], p->color[1]);
	midcolor(s0->color[2], p->color[2], p->color[3]);
	memcpy(s0->color[3], p->color[3], sizeof(s0->color[3]));

	memcpy(s1->color[0], s0->color[1], sizeof(s1->color[0]));
	memcpy(s1->color[1], p->color[1], sizeof(s1->color[1]));
	memcpy(s1->color[2], p->color[2], sizeof(s1->color[2]));
	memcpy(s1->color[3], s0->color[2], sizeof(s1->color[3]));
}

static void
draw_patch(fz_shade *shade, pdf_tensor_patch *p, int depth, int origdepth)
{
	pdf_tensor_patch s0, s1;

	/* split patch into two half-width patches */
	split_patch(p, &s0, &s1);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		draw_stripe(&s0, shade, origdepth);
		draw_stripe(&s1, shade, origdepth);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		draw_patch(shade, &s0, depth, origdepth);
		draw_patch(shade, &s1, depth, origdepth);
	}
}

static fz_point
pdf_compute_tensor_interior(
	fz_point a, fz_point b, fz_point c, fz_point d,
	fz_point e, fz_point f, fz_point g, fz_point h)
{
	fz_point pt;

	/* see equations at page 330 in pdf 1.7 */

	pt.x = -4 * a.x;
	pt.x += 6 * (b.x + c.x);
	pt.x += -2 * (d.x + e.x);
	pt.x += 3 * (f.x + g.x);
	pt.x += -1 * h.x;
	pt.x /= 9;

	pt.y = -4 * a.y;
	pt.y += 6 * (b.y + c.y);
	pt.y += -2 * (d.y + e.y);
	pt.y += 3 * (f.y + g.y);
	pt.y += -1 * h.y;
	pt.y /= 9;

	return pt;
}

static void
pdf_make_tensor_patch(pdf_tensor_patch *p, int type, fz_point *pt)
{
	if (type == 6)
	{
		/* see control point stream order at page 325 in pdf 1.7 */

		p->pole[0][0] = pt[0];
		p->pole[0][1] = pt[1];
		p->pole[0][2] = pt[2];
		p->pole[0][3] = pt[3];
		p->pole[1][3] = pt[4];
		p->pole[2][3] = pt[5];
		p->pole[3][3] = pt[6];
		p->pole[3][2] = pt[7];
		p->pole[3][1] = pt[8];
		p->pole[3][0] = pt[9];
		p->pole[2][0] = pt[10];
		p->pole[1][0] = pt[11];

		/* see equations at page 330 in pdf 1.7 */

		p->pole[1][1] = pdf_compute_tensor_interior(
			p->pole[0][0], p->pole[0][1], p->pole[1][0], p->pole[0][3],
			p->pole[3][0], p->pole[3][1], p->pole[1][3], p->pole[3][3]);

		p->pole[1][2] = pdf_compute_tensor_interior(
			p->pole[0][3], p->pole[0][2], p->pole[1][3], p->pole[0][0],
			p->pole[3][3], p->pole[3][2], p->pole[1][0], p->pole[3][0]);

		p->pole[2][1] = pdf_compute_tensor_interior(
			p->pole[3][0], p->pole[3][1], p->pole[2][0], p->pole[3][3],
			p->pole[0][0], p->pole[0][1], p->pole[2][3], p->pole[0][3]);

		p->pole[2][2] = pdf_compute_tensor_interior(
			p->pole[3][3], p->pole[3][2], p->pole[2][3], p->pole[3][0],
			p->pole[0][3], p->pole[0][2], p->pole[2][0], p->pole[0][0]);
	}
	else if (type == 7)
	{
		/* see control point stream order at page 330 in pdf 1.7 */

		p->pole[0][0] = pt[0];
		p->pole[0][1] = pt[1];
		p->pole[0][2] = pt[2];
		p->pole[0][3] = pt[3];
		p->pole[1][3] = pt[4];
		p->pole[2][3] = pt[5];
		p->pole[3][3] = pt[6];
		p->pole[3][2] = pt[7];
		p->pole[3][1] = pt[8];
		p->pole[3][0] = pt[9];
		p->pole[2][0] = pt[10];
		p->pole[1][0] = pt[11];
		p->pole[1][1] = pt[12];
		p->pole[1][2] = pt[13];
		p->pole[2][2] = pt[14];
		p->pole[2][1] = pt[15];
	}
}

/* Sample various functions into lookup tables */

static void
pdf_sample_composite_shade_function(fz_shade *shade, pdf_function *func, float t0, float t1)
{
	int i;
	float t;

	for (i = 0; i < 256; i++)
	{
		t = t0 + (i / 255.0f) * (t1 - t0);
		pdf_eval_function(func, &t, 1, shade->function[i], shade->colorspace->n);
		shade->function[i][shade->colorspace->n] = 1;
	}
}

static void
pdf_sample_component_shade_function(fz_shade *shade, int funcs, pdf_function **func, float t0, float t1)
{
	int i, k;
	float t;

	for (i = 0; i < 256; i++)
	{
		t = t0 + (i / 255.0f) * (t1 - t0);
		for (k = 0; k < funcs; k++)
			pdf_eval_function(func[k], &t, 1, &shade->function[i][k], 1);
		shade->function[i][k] = 1;
	}
}

static void
pdf_sample_shade_function(fz_shade *shade, int funcs, pdf_function **func, float t0, float t1)
{
	shade->use_function = 1;
	if (funcs == 1)
		pdf_sample_composite_shade_function(shade, func[0], t0, t1);
	else
		pdf_sample_component_shade_function(shade, funcs, func, t0, t1);
}

/* Type 1-3 -- Function-based, axial and radial shadings */

static void
pdf_load_function_based_shading(fz_shade *shade, pdf_xref *xref, fz_obj *dict, pdf_function *func)
{
	fz_obj *obj;
	float x0, y0, x1, y1;
	fz_matrix matrix;
	struct vertex v[4];
	int xx, yy;
	float x, y;
	float xn, yn;
	int i;

	x0 = y0 = 0;
	x1 = y1 = 1;
	obj = fz_dict_gets(dict, "Domain");
	if (fz_array_len(obj) == 4)
	{
		x0 = fz_to_real(fz_array_get(obj, 0));
		x1 = fz_to_real(fz_array_get(obj, 1));
		y0 = fz_to_real(fz_array_get(obj, 2));
		y1 = fz_to_real(fz_array_get(obj, 3));
	}

	matrix = fz_identity;
	obj = fz_dict_gets(dict, "Matrix");
	if (fz_array_len(obj) == 6)
		matrix = pdf_to_matrix(obj);

	for (yy = 0; yy < FUNSEGS; yy++)
	{
		y = y0 + (y1 - y0) * yy / FUNSEGS;
		yn = y0 + (y1 - y0) * (yy + 1) / FUNSEGS;

		for (xx = 0; xx < FUNSEGS; xx++)
		{
			x = x0 + (x1 - x0) * xx / FUNSEGS;
			xn = x0 + (x1 - x0) * (xx + 1) / FUNSEGS;

			v[0].x = x; v[0].y = y;
			v[1].x = xn; v[1].y = y;
			v[2].x = xn; v[2].y = yn;
			v[3].x = x; v[3].y = yn;

			for (i = 0; i < 4; i++)
			{
				fz_point pt;
				float fv[2];

				fv[0] = v[i].x;
				fv[1] = v[i].y;
				pdf_eval_function(func, fv, 2, v[i].c, shade->colorspace->n);

				pt.x = v[i].x;
				pt.y = v[i].y;
				pt = fz_transform_point(matrix, pt);
				v[i].x = pt.x;
				v[i].y = pt.y;
			}

			pdf_add_quad(shade, &v[0], &v[1], &v[2], &v[3]);
		}
	}
}

static void
pdf_load_axial_shading(fz_shade *shade, pdf_xref *xref, fz_obj *dict, int funcs, pdf_function **func)
{
	fz_obj *obj;
	float d0, d1;
	int e0, e1;
	float x0, y0, x1, y1;
	struct vertex p1, p2;

	obj = fz_dict_gets(dict, "Coords");
	x0 = fz_to_real(fz_array_get(obj, 0));
	y0 = fz_to_real(fz_array_get(obj, 1));
	x1 = fz_to_real(fz_array_get(obj, 2));
	y1 = fz_to_real(fz_array_get(obj, 3));

	d0 = 0;
	d1 = 1;
	obj = fz_dict_gets(dict, "Domain");
	if (fz_array_len(obj) == 2)
	{
		d0 = fz_to_real(fz_array_get(obj, 0));
		d1 = fz_to_real(fz_array_get(obj, 1));
	}

	e0 = e1 = 0;
	obj = fz_dict_gets(dict, "Extend");
	if (fz_array_len(obj) == 2)
	{
		e0 = fz_to_bool(fz_array_get(obj, 0));
		e1 = fz_to_bool(fz_array_get(obj, 1));
	}

	pdf_sample_shade_function(shade, funcs, func, d0, d1);

	shade->type = FZ_LINEAR;

	shade->extend[0] = e0;
	shade->extend[1] = e1;

	p1.x = x0;
	p1.y = y0;
	p1.c[0] = 0;
	pdf_add_vertex(shade, &p1);

	p2.x = x1;
	p2.y = y1;
	p2.c[0] = 0;
	pdf_add_vertex(shade, &p2);
}

static void
pdf_load_radial_shading(fz_shade *shade, pdf_xref *xref, fz_obj *dict, int funcs, pdf_function **func)
{
	fz_obj *obj;
	float d0, d1;
	int e0, e1;
	float x0, y0, r0, x1, y1, r1;
	struct vertex p1, p2;

	obj = fz_dict_gets(dict, "Coords");
	x0 = fz_to_real(fz_array_get(obj, 0));
	y0 = fz_to_real(fz_array_get(obj, 1));
	r0 = fz_to_real(fz_array_get(obj, 2));
	x1 = fz_to_real(fz_array_get(obj, 3));
	y1 = fz_to_real(fz_array_get(obj, 4));
	r1 = fz_to_real(fz_array_get(obj, 5));

	d0 = 0;
	d1 = 1;
	obj = fz_dict_gets(dict, "Domain");
	if (fz_array_len(obj) == 2)
	{
		d0 = fz_to_real(fz_array_get(obj, 0));
		d1 = fz_to_real(fz_array_get(obj, 1));
	}

	e0 = e1 = 0;
	obj = fz_dict_gets(dict, "Extend");
	if (fz_array_len(obj) == 2)
	{
		e0 = fz_to_bool(fz_array_get(obj, 0));
		e1 = fz_to_bool(fz_array_get(obj, 1));
	}

	pdf_sample_shade_function(shade, funcs, func, d0, d1);

	shade->type = FZ_RADIAL;

	shade->extend[0] = e0;
	shade->extend[1] = e1;

	p1.x = x0;
	p1.y = y0;
	p1.c[0] = r0;
	pdf_add_vertex(shade, &p1);

	p2.x = x1;
	p2.y = y1;
	p2.c[0] = r1;
	pdf_add_vertex(shade, &p2);
}

/* Type 4-7 -- Triangle and patch mesh shadings */

static inline float read_sample(fz_stream *stream, int bits, float min, float max)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = 1 / (powf(2, bits) - 1);
	return min + fz_read_bits(stream, bits) * (max - min) * bitscale;
}

struct mesh_params
{
	int vprow;
	int bpflag;
	int bpcoord;
	int bpcomp;
	float x0, x1;
	float y0, y1;
	float c0[FZ_MAX_COLORS];
	float c1[FZ_MAX_COLORS];
};

static void
pdf_load_mesh_params(pdf_xref *xref, fz_obj *dict, struct mesh_params *p)
{
	fz_obj *obj;
	int i, n;

	p->x0 = p->y0 = 0;
	p->x1 = p->y1 = 1;
	for (i = 0; i < FZ_MAX_COLORS; i++)
	{
		p->c0[i] = 0;
		p->c1[i] = 1;
	}

	p->vprow = fz_to_int(fz_dict_gets(dict, "VerticesPerRow"));
	p->bpflag = fz_to_int(fz_dict_gets(dict, "BitsPerFlag"));
	p->bpcoord = fz_to_int(fz_dict_gets(dict, "BitsPerCoordinate"));
	p->bpcomp = fz_to_int(fz_dict_gets(dict, "BitsPerComponent"));

	obj = fz_dict_gets(dict, "Decode");
	if (fz_array_len(obj) >= 6)
	{
		n = (fz_array_len(obj) - 4) / 2;
		p->x0 = fz_to_real(fz_array_get(obj, 0));
		p->x1 = fz_to_real(fz_array_get(obj, 1));
		p->y0 = fz_to_real(fz_array_get(obj, 2));
		p->y1 = fz_to_real(fz_array_get(obj, 3));
		for (i = 0; i < n; i++)
		{
			p->c0[i] = fz_to_real(fz_array_get(obj, 4 + i * 2));
			p->c1[i] = fz_to_real(fz_array_get(obj, 5 + i * 2));
		}
	}

	if (p->vprow < 2)
		p->vprow = 2;

	if (p->bpflag != 2 && p->bpflag != 4 && p->bpflag != 8)
		p->bpflag = 8;

	if (p->bpcoord != 1 && p->bpcoord != 2 && p->bpcoord != 4 &&
		p->bpcoord != 8 && p->bpcoord != 12 && p->bpcoord != 16 &&
		p->bpcoord != 24 && p->bpcoord != 32)
		p->bpcoord = 8;

	if (p->bpcomp != 1 && p->bpcomp != 2 && p->bpcomp != 4 &&
		p->bpcomp != 8 && p->bpcomp != 12 && p->bpcomp != 16)
		p->bpcomp = 8;
}

static void
pdf_load_type4_shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	struct mesh_params p;
	struct vertex va, vb, vc, vd;
	int ncomp;
	int flag;
	int i;

	pdf_load_mesh_params(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		pdf_sample_shade_function(shade, funcs, func, p.c0[0], p.c1[0]);
	}
	else
		ncomp = shade->colorspace->n;

	while (!fz_is_eof_bits(stream))
	{
		flag = fz_read_bits(stream, p.bpflag);
		vd.x = read_sample(stream, p.bpcoord, p.x0, p.x1);
		vd.y = read_sample(stream, p.bpcoord, p.y0, p.y1);
		for (i = 0; i < ncomp; i++)
			vd.c[i] = read_sample(stream, p.bpcomp, p.c0[i], p.c1[i]);

		switch (flag)
		{
		case 0: /* start new triangle */
			va = vd;

			fz_read_bits(stream, p.bpflag);
			vb.x = read_sample(stream, p.bpcoord, p.x0, p.x1);
			vb.y = read_sample(stream, p.bpcoord, p.y0, p.y1);
			for (i = 0; i < ncomp; i++)
				vb.c[i] = read_sample(stream, p.bpcomp, p.c0[i], p.c1[i]);

			fz_read_bits(stream, p.bpflag);
			vc.x = read_sample(stream, p.bpcoord, p.x0, p.x1);
			vc.y = read_sample(stream, p.bpcoord, p.y0, p.y1);
			for (i = 0; i < ncomp; i++)
				vc.c[i] = read_sample(stream, p.bpcomp, p.c0[i], p.c1[i]);

			pdf_add_triangle(shade, &va, &vb, &vc);
			break;

		case 1: /* Vb, Vc, Vd */
			va = vb;
			vb = vc;
			vc = vd;
			pdf_add_triangle(shade, &va, &vb, &vc);
			break;

		case 2: /* Va, Vc, Vd */
			vb = vc;
			vc = vd;
			pdf_add_triangle(shade, &va, &vb, &vc);
			break;
		}
	}
}

static void
pdf_load_type5_shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	struct mesh_params p;
	struct vertex *buf, *ref;
	int first;
	int ncomp;
	int i, k;

	pdf_load_mesh_params(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		pdf_sample_shade_function(shade, funcs, func, p.c0[0], p.c1[0]);
	}
	else
		ncomp = shade->colorspace->n;

	ref = fz_calloc(p.vprow, sizeof(struct vertex));
	buf = fz_calloc(p.vprow, sizeof(struct vertex));
	first = 1;

	while (!fz_is_eof_bits(stream))
	{
		for (i = 0; i < p.vprow; i++)
		{
			buf[i].x = read_sample(stream, p.bpcoord, p.x0, p.x1);
			buf[i].y = read_sample(stream, p.bpcoord, p.y0, p.y1);
			for (k = 0; k < ncomp; k++)
				buf[i].c[k] = read_sample(stream, p.bpcomp, p.c0[k], p.c1[k]);
		}

		if (!first)
			for (i = 0; i < p.vprow - 1; i++)
				pdf_add_quad(shade,
					&ref[i], &ref[i+1], &buf[i+1], &buf[i]);

		memcpy(ref, buf, p.vprow * sizeof(struct vertex));
		first = 0;
	}

	free(ref);
	free(buf);
}

/* Type 6 & 7 -- Patch mesh shadings */

static void
pdf_load_type6_shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	struct mesh_params p;
	int haspatch, hasprevpatch;
	float prevc[4][FZ_MAX_COLORS];
	fz_point prevp[12];
	int ncomp;
	int i, k;

	pdf_load_mesh_params(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		pdf_sample_shade_function(shade, funcs, func, p.c0[0], p.c1[0]);
	}
	else
		ncomp = shade->colorspace->n;

	hasprevpatch = 0;

	while (!fz_is_eof_bits(stream))
	{
		float c[4][FZ_MAX_COLORS];
		fz_point v[12];
		int startcolor;
		int startpt;
		int flag;

		flag = fz_read_bits(stream, p.bpflag);

		if (flag == 0)
		{
			startpt = 0;
			startcolor = 0;
		}
		else
		{
			startpt = 4;
			startcolor = 2;
		}

		for (i = startpt; i < 12; i++)
		{
			v[i].x = read_sample(stream, p.bpcoord, p.x0, p.x1);
			v[i].y = read_sample(stream, p.bpcoord, p.y0, p.y1);
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
				c[i][k] = read_sample(stream, p.bpcomp, p.c0[k], p.c1[k]);
		}

		haspatch = 0;

		if (flag == 0)
		{
			haspatch = 1;
		}
		else if (flag == 1 && hasprevpatch)
		{
			v[0] = prevp[3];
			v[1] = prevp[4];
			v[2] = prevp[5];
			v[3] = prevp[6];
			memcpy(c[0], prevc[1], ncomp * sizeof(float));
			memcpy(c[1], prevc[2], ncomp * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 2 && hasprevpatch)
		{
			v[0] = prevp[6];
			v[1] = prevp[7];
			v[2] = prevp[8];
			v[3] = prevp[9];
			memcpy(c[0], prevc[2], ncomp * sizeof(float));
			memcpy(c[1], prevc[3], ncomp * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 3 && hasprevpatch)
		{
			v[0] = prevp[ 9];
			v[1] = prevp[10];
			v[2] = prevp[11];
			v[3] = prevp[ 0];
			memcpy(c[0], prevc[3], ncomp * sizeof(float));
			memcpy(c[1], prevc[0], ncomp * sizeof(float));

			haspatch = 1;
		}

		if (haspatch)
		{
			pdf_tensor_patch patch;

			pdf_make_tensor_patch(&patch, 6, v);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], ncomp * sizeof(float));

			draw_patch(shade, &patch, SUBDIV, SUBDIV);

			for (i = 0; i < 12; i++)
				prevp[i] = v[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], ncomp * sizeof(float));

			hasprevpatch = 1;
		}
	}
}

static void
pdf_load_type7_shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	struct mesh_params p;
	int haspatch, hasprevpatch;
	float prevc[4][FZ_MAX_COLORS];
	fz_point prevp[16];
	int ncomp;
	int i, k;

	pdf_load_mesh_params(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		pdf_sample_shade_function(shade, funcs, func, p.c0[0], p.c1[0]);
	}
	else
		ncomp = shade->colorspace->n;

	hasprevpatch = 0;

	while (!fz_is_eof_bits(stream))
	{
		float c[4][FZ_MAX_COLORS];
		fz_point v[16];
		int startcolor;
		int startpt;
		int flag;

		flag = fz_read_bits(stream, p.bpflag);

		if (flag == 0)
		{
			startpt = 0;
			startcolor = 0;
		}
		else
		{
			startpt = 4;
			startcolor = 2;
		}

		for (i = startpt; i < 16; i++)
		{
			v[i].x = read_sample(stream, p.bpcoord, p.x0, p.x1);
			v[i].y = read_sample(stream, p.bpcoord, p.y0, p.y1);
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
				c[i][k] = read_sample(stream, p.bpcomp, p.c0[k], p.c1[k]);
		}

		haspatch = 0;

		if (flag == 0)
		{
			haspatch = 1;
		}
		else if (flag == 1 && hasprevpatch)
		{
			v[0] = prevp[3];
			v[1] = prevp[4];
			v[2] = prevp[5];
			v[3] = prevp[6];
			memcpy(c[0], prevc[1], ncomp * sizeof(float));
			memcpy(c[1], prevc[2], ncomp * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 2 && hasprevpatch)
		{
			v[0] = prevp[6];
			v[1] = prevp[7];
			v[2] = prevp[8];
			v[3] = prevp[9];
			memcpy(c[0], prevc[2], ncomp * sizeof(float));
			memcpy(c[1], prevc[3], ncomp * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 3 && hasprevpatch)
		{
			v[0] = prevp[ 9];
			v[1] = prevp[10];
			v[2] = prevp[11];
			v[3] = prevp[ 0];
			memcpy(c[0], prevc[3], ncomp * sizeof(float));
			memcpy(c[1], prevc[0], ncomp * sizeof(float));

			haspatch = 1;
		}

		if (haspatch)
		{
			pdf_tensor_patch patch;

			pdf_make_tensor_patch(&patch, 7, v);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], ncomp * sizeof(float));

			draw_patch(shade, &patch, SUBDIV, SUBDIV);

			for (i = 0; i < 16; i++)
				prevp[i] = v[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], FZ_MAX_COLORS * sizeof(float));

			hasprevpatch = 1;
		}
	}
}

/* Load all of the shading dictionary parameters, then switch on the shading type. */

static fz_error
pdf_load_shading_dict(fz_shade **shadep, pdf_xref *xref, fz_obj *dict, fz_matrix transform)
{
	fz_error error;
	fz_shade *shade;
	pdf_function *func[FZ_MAX_COLORS] = { NULL };
	fz_stream *stream = NULL;
	fz_obj *obj;
	int funcs;
	int type;
	int i;

	shade = fz_malloc(sizeof(fz_shade));
	shade->refs = 1;
	shade->type = FZ_MESH;
	shade->use_background = 0;
	shade->use_function = 0;
	shade->matrix = transform;
	shade->bbox = fz_infinite_rect;
	shade->extend[0] = 0;
	shade->extend[1] = 0;

	shade->mesh_len = 0;
	shade->mesh_cap = 0;
	shade->mesh = NULL;

	shade->colorspace = NULL;

	funcs = 0;

	obj = fz_dict_gets(dict, "ShadingType");
	type = fz_to_int(obj);

	obj = fz_dict_gets(dict, "ColorSpace");
	if (!obj)
	{
		fz_drop_shade(shade);
		return fz_throw("shading colorspace is missing");
	}
	error = pdf_load_colorspace(&shade->colorspace, xref, obj);
	if (error)
	{
		fz_drop_shade(shade);
		return fz_rethrow(error, "cannot load colorspace (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
	}

	obj = fz_dict_gets(dict, "Background");
	if (obj)
	{
		shade->use_background = 1;
		for (i = 0; i < shade->colorspace->n; i++)
			shade->background[i] = fz_to_real(fz_array_get(obj, i));
	}

	obj = fz_dict_gets(dict, "BBox");
	if (fz_is_array(obj))
	{
		shade->bbox = pdf_to_rect(obj);
	}

	obj = fz_dict_gets(dict, "Function");
	if (fz_is_dict(obj))
	{
		funcs = 1;

		error = pdf_load_function(&func[0], xref, obj);
		if (error)
		{
			error = fz_rethrow(error, "cannot load shading function (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
			goto cleanup;
		}
	}
	else if (fz_is_array(obj))
	{
		funcs = fz_array_len(obj);
		if (funcs != 1 && funcs != shade->colorspace->n)
		{
			error = fz_throw("incorrect number of shading functions");
			goto cleanup;
		}

		for (i = 0; i < funcs; i++)
		{
			error = pdf_load_function(&func[i], xref, fz_array_get(obj, i));
			if (error)
			{
				error = fz_rethrow(error, "cannot load shading function (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
				goto cleanup;
			}
		}
	}

	if (type >= 4 && type <= 7)
	{
		error = pdf_open_stream(&stream, xref, fz_to_num(dict), fz_to_gen(dict));
		if (error)
		{
			error = fz_rethrow(error, "cannot open shading stream (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
			goto cleanup;
		}
	}

	switch (type)
	{
	case 1: pdf_load_function_based_shading(shade, xref, dict, func[0]); break;
	case 2: pdf_load_axial_shading(shade, xref, dict, funcs, func); break;
	case 3: pdf_load_radial_shading(shade, xref, dict, funcs, func); break;
	case 4: pdf_load_type4_shade(shade, xref, dict, funcs, func, stream); break;
	case 5: pdf_load_type5_shade(shade, xref, dict, funcs, func, stream); break;
	case 6: pdf_load_type6_shade(shade, xref, dict, funcs, func, stream); break;
	case 7: pdf_load_type7_shade(shade, xref, dict, funcs, func, stream); break;
	default:
		error = fz_throw("unknown shading type: %d", type);
		goto cleanup;
	}

	if (stream)
		fz_close(stream);
	for (i = 0; i < funcs; i++)
		if (func[i])
			pdf_drop_function(func[i]);

	*shadep = shade;
	return fz_okay;

cleanup:
	if (stream)
		fz_close(stream);
	for (i = 0; i < funcs; i++)
		if (func[i])
			pdf_drop_function(func[i]);
	fz_drop_shade(shade);

	return fz_rethrow(error, "cannot load shading type %d (%d %d R)", type, fz_to_num(dict), fz_to_gen(dict));
}

fz_error
pdf_load_shading(fz_shade **shadep, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_matrix mat;
	fz_obj *obj;

	if ((*shadep = pdf_find_item(xref->store, fz_drop_shade, dict)))
	{
		fz_keep_shade(*shadep);
		return fz_okay;
	}

	/* Type 2 pattern dictionary */
	if (fz_dict_gets(dict, "PatternType"))
	{
		obj = fz_dict_gets(dict, "Matrix");
		if (obj)
			mat = pdf_to_matrix(obj);
		else
			mat = fz_identity;

		obj = fz_dict_gets(dict, "ExtGState");
		if (obj)
		{
			if (fz_dict_gets(obj, "CA") || fz_dict_gets(obj, "ca"))
			{
				fz_warn("shading with alpha not supported");
			}
		}

		obj = fz_dict_gets(dict, "Shading");
		if (!obj)
			return fz_throw("syntaxerror: missing shading dictionary");

		error = pdf_load_shading_dict(shadep, xref, obj, mat);
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary (%d %d R)", fz_to_num(obj), fz_to_gen(obj));
	}

	/* Naked shading dictionary */
	else
	{
		error = pdf_load_shading_dict(shadep, xref, dict, fz_identity);
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary (%d %d R)", fz_to_num(dict), fz_to_gen(dict));
	}

	pdf_store_item(xref->store, fz_keep_shade, fz_drop_shade, dict, *shadep);

	return fz_okay;
}
