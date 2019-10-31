#include "mupdf/fitz.h"

#include <string.h>
#include <math.h>

typedef struct fz_mesh_processor_s fz_mesh_processor;

struct fz_mesh_processor_s {
	fz_shade *shade;
	fz_shade_prepare_fn *prepare;
	fz_shade_process_fn *process;
	void *process_arg;
	int ncomp;
};

#define SWAP(a,b) {fz_vertex *t = (a); (a) = (b); (b) = t;}

static inline void
paint_tri(fz_context *ctx, fz_mesh_processor *painter, fz_vertex *v0, fz_vertex *v1, fz_vertex *v2)
{
	if (painter->process)
	{
		painter->process(ctx, painter->process_arg, v0, v1, v2);
	}
}

static inline void
paint_quad(fz_context *ctx, fz_mesh_processor *painter, fz_vertex *v0, fz_vertex *v1, fz_vertex *v2, fz_vertex *v3)
{
	/* For a quad with corners (in clockwise or anticlockwise order) are
	 * v0, v1, v2, v3. We can choose to split in in various different ways.
	 * Arbitrarily we can pick v0, v1, v3 for the first triangle. We then
	 * have to choose between v1, v2, v3 or v3, v2, v1 (or their equivalent
	 * rotations) for the second triangle.
	 *
	 * v1, v2, v3 has the property that both triangles share the same
	 * winding (useful if we were ever doing simple back face culling).
	 *
	 * v3, v2, v1 has the property that all the 'shared' edges (both
	 * within this quad, and with adjacent quads) are walked in the same
	 * direction every time. This can be useful in that depending on the
	 * implementation/rounding etc walking from A -> B can hit different
	 * pixels than walking from B->A.
	 *
	 * In the event neither of these things matter at the moment, as all
	 * the process functions where it matters order the edges from top to
	 * bottom before walking them.
	 */
	if (painter->process)
	{
		painter->process(ctx, painter->process_arg, v0, v1, v3);
		painter->process(ctx, painter->process_arg, v3, v2, v1);
	}
}

static inline void
fz_prepare_color(fz_context *ctx, fz_mesh_processor *painter, fz_vertex *v, float *c)
{
	if (painter->prepare)
	{
		painter->prepare(ctx, painter->process_arg, v, c);
	}
}

static inline void
fz_prepare_vertex(fz_context *ctx, fz_mesh_processor *painter, fz_vertex *v, fz_matrix ctm, float x, float y, float *c)
{
	v->p = fz_transform_point_xy(x, y, ctm);
	if (painter->prepare)
	{
		painter->prepare(ctx, painter->process_arg, v, c);
	}
}

static void
fz_process_shade_type1(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter)
{
	float *p = shade->u.f.fn_vals;
	int xdivs = shade->u.f.xdivs;
	int ydivs = shade->u.f.ydivs;
	float x0 = shade->u.f.domain[0][0];
	float y0 = shade->u.f.domain[0][1];
	float x1 = shade->u.f.domain[1][0];
	float y1 = shade->u.f.domain[1][1];
	int xx, yy;
	float y, yn, x;
	fz_vertex vs[2][2];
	fz_vertex *v = vs[0];
	fz_vertex *vn = vs[1];
	int n = fz_colorspace_n(ctx, shade->colorspace);

	ctm = fz_concat(shade->u.f.matrix, ctm);

	y = y0;
	for (yy = 0; yy < ydivs; yy++)
	{
		yn = y0 + (y1 - y0) * (yy + 1) / ydivs;

		x = x0;

		fz_prepare_vertex(ctx, painter, &v[0], ctm, x, y, p);
		p += n;
		fz_prepare_vertex(ctx, painter, &v[1], ctm, x, yn, p + xdivs * n);

		for (xx = 0; xx < xdivs; xx++)
		{
			x = x0 + (x1 - x0) * (xx + 1) / xdivs;

			fz_prepare_vertex(ctx, painter, &vn[0], ctm, x, y, p);
			p += n;
			fz_prepare_vertex(ctx, painter, &vn[1], ctm, x, yn, p + xdivs * n);

			paint_quad(ctx, painter, &v[0], &vn[0], &vn[1], &v[1]);
			SWAP(v,vn);
		}
		y = yn;
	}
}

#define HUGENUM 32000 /* how far to extend linear/radial shadings */

static fz_point
fz_point_on_circle(fz_point p, float r, float theta)
{
	p.x = p.x + cosf(theta) * r;
	p.y = p.y + sinf(theta) * r;
	return p;
}

static void
fz_process_shade_type2(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter, fz_rect scissor)
{
	fz_point p0, p1, dir;
	fz_vertex v0, v1, v2, v3;
	fz_vertex e0, e1;
	float theta;
	float zero = 0;
	float one = 1;
	float r;

	p0.x = shade->u.l_or_r.coords[0][0];
	p0.y = shade->u.l_or_r.coords[0][1];
	p1.x = shade->u.l_or_r.coords[1][0];
	p1.y = shade->u.l_or_r.coords[1][1];
	dir.x = p0.y - p1.y;
	dir.y = p1.x - p0.x;
	p0 = fz_transform_point(p0, ctm);
	p1 = fz_transform_point(p1, ctm);
	dir = fz_transform_vector(dir, ctm);
	theta = atan2f(dir.y, dir.x);

	if (fz_is_infinite_rect(scissor)) {
		r = HUGENUM; /* Not ideal, but it'll do for now */
	} else {
		float x = p0.x - scissor.x0;
		float y = p0.y - scissor.y0;
		if (x < scissor.x1 - p0.x)
			x = scissor.x1 - p0.x;
		if (x < p0.x - scissor.x1)
			x = p0.x - scissor.x1;
		if (x < scissor.x1 - p1.x)
			x = scissor.x1 - p1.x;
		if (y < scissor.y1 - p0.y)
			y = scissor.y1 - p0.y;
		if (y < p0.y - scissor.y1)
			y = p0.y - scissor.y1;
		if (y < scissor.y1 - p1.y)
			y = scissor.y1 - p1.y;
		r = x+y;
	}
	v0.p = fz_point_on_circle(p0, r, theta);
	v1.p = fz_point_on_circle(p1, r, theta);
	v2.p.x = 2*p0.x - v0.p.x;
	v2.p.y = 2*p0.y - v0.p.y;
	v3.p.x = 2*p1.x - v1.p.x;
	v3.p.y = 2*p1.y - v1.p.y;

	fz_prepare_color(ctx, painter, &v0, &zero);
	fz_prepare_color(ctx, painter, &v1, &one);
	fz_prepare_color(ctx, painter, &v2, &zero);
	fz_prepare_color(ctx, painter, &v3, &one);

	paint_quad(ctx, painter, &v0, &v2, &v3, &v1);

	if (shade->u.l_or_r.extend[0] || shade->u.l_or_r.extend[1]) {
		float d = fabsf(p1.x - p0.x);
		float e = fabsf(p1.y - p0.y);
		if (d < e)
			d = e;
		if (d != 0)
			r /= d;
	}
	if (shade->u.l_or_r.extend[0])
	{
		e0.p.x = v0.p.x - (p1.x - p0.x) * r;
		e0.p.y = v0.p.y - (p1.y - p0.y) * r;
		fz_prepare_color(ctx, painter, &e0, &zero);

		e1.p.x = v2.p.x - (p1.x - p0.x) * r;
		e1.p.y = v2.p.y - (p1.y - p0.y) * r;
		fz_prepare_color(ctx, painter, &e1, &zero);

		paint_quad(ctx, painter, &e0, &v0, &v2, &e1);
	}

	if (shade->u.l_or_r.extend[1])
	{
		e0.p.x = v1.p.x + (p1.x - p0.x) * r;
		e0.p.y = v1.p.y + (p1.y - p0.y) * r;
		fz_prepare_color(ctx, painter, &e0, &one);

		e1.p.x = v3.p.x + (p1.x - p0.x) * r;
		e1.p.y = v3.p.y + (p1.y - p0.y) * r;
		fz_prepare_color(ctx, painter, &e1, &one);

		paint_quad(ctx, painter, &e0, &v1, &v3, &e1);
	}
}

static void
fz_paint_annulus(fz_context *ctx, fz_matrix ctm,
		fz_point p0, float r0, float c0,
		fz_point p1, float r1, float c1,
		int count,
		fz_mesh_processor *painter)
{
	fz_vertex t0, t1, t2, t3, b0, b1, b2, b3;
	float theta, step, a, b;
	int i;

	theta = atan2f(p1.y - p0.y, p1.x - p0.x);
	step = FZ_PI / count;

	a = 0;
	for (i = 1; i <= count; i++)
	{
		b = i * step;

		t0.p = fz_transform_point(fz_point_on_circle(p0, r0, theta + a), ctm);
		t1.p = fz_transform_point(fz_point_on_circle(p0, r0, theta + b), ctm);
		t2.p = fz_transform_point(fz_point_on_circle(p1, r1, theta + a), ctm);
		t3.p = fz_transform_point(fz_point_on_circle(p1, r1, theta + b), ctm);
		b0.p = fz_transform_point(fz_point_on_circle(p0, r0, theta - a), ctm);
		b1.p = fz_transform_point(fz_point_on_circle(p0, r0, theta - b), ctm);
		b2.p = fz_transform_point(fz_point_on_circle(p1, r1, theta - a), ctm);
		b3.p = fz_transform_point(fz_point_on_circle(p1, r1, theta - b), ctm);

		fz_prepare_color(ctx, painter, &t0, &c0);
		fz_prepare_color(ctx, painter, &t1, &c0);
		fz_prepare_color(ctx, painter, &t2, &c1);
		fz_prepare_color(ctx, painter, &t3, &c1);
		fz_prepare_color(ctx, painter, &b0, &c0);
		fz_prepare_color(ctx, painter, &b1, &c0);
		fz_prepare_color(ctx, painter, &b2, &c1);
		fz_prepare_color(ctx, painter, &b3, &c1);

		paint_quad(ctx, painter, &t0, &t2, &t3, &t1);
		paint_quad(ctx, painter, &b0, &b2, &b3, &b1);

		a = b;
	}
}

static void
fz_process_shade_type3(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter)
{
	fz_point p0, p1;
	float r0, r1;
	fz_point e;
	float er, rs;
	int count;

	p0.x = shade->u.l_or_r.coords[0][0];
	p0.y = shade->u.l_or_r.coords[0][1];
	r0 = shade->u.l_or_r.coords[0][2];

	p1.x = shade->u.l_or_r.coords[1][0];
	p1.y = shade->u.l_or_r.coords[1][1];
	r1 = shade->u.l_or_r.coords[1][2];

	/* number of segments for a half-circle */
	count = 4 * sqrtf(fz_matrix_expansion(ctm) * fz_max(r0, r1));
	if (count < 3)
		count = 3;
	if (count > 1024)
		count = 1024;

	if (shade->u.l_or_r.extend[0])
	{
		if (r0 < r1)
			rs = r0 / (r0 - r1);
		else
			rs = -HUGENUM;

		e.x = p0.x + (p1.x - p0.x) * rs;
		e.y = p0.y + (p1.y - p0.y) * rs;
		er = r0 + (r1 - r0) * rs;

		fz_paint_annulus(ctx, ctm, e, er, 0, p0, r0, 0, count, painter);
	}

	fz_paint_annulus(ctx, ctm, p0, r0, 0, p1, r1, 1, count, painter);

	if (shade->u.l_or_r.extend[1])
	{
		if (r0 > r1)
			rs = r1 / (r1 - r0);
		else
			rs = -HUGENUM;

		e.x = p1.x + (p0.x - p1.x) * rs;
		e.y = p1.y + (p0.y - p1.y) * rs;
		er = r1 + (r0 - r1) * rs;

		fz_paint_annulus(ctx, ctm, p1, r1, 1, e, er, 1, count, painter);
	}
}

static inline float read_sample(fz_context *ctx, fz_stream *stream, int bits, float min, float max)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = 1 / (powf(2, bits) - 1);
	return min + fz_read_bits(ctx, stream, bits) * (max - min) * bitscale;
}

static void
fz_process_shade_type4(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter)
{
	fz_stream *stream = fz_open_compressed_buffer(ctx, shade->buffer);
	fz_vertex v[4];
	fz_vertex *va = &v[0];
	fz_vertex *vb = &v[1];
	fz_vertex *vc = &v[2];
	fz_vertex *vd = &v[3];
	int flag, i, ncomp = painter->ncomp;
	int bpflag = shade->u.m.bpflag;
	int bpcoord = shade->u.m.bpcoord;
	int bpcomp = shade->u.m.bpcomp;
	float x0 = shade->u.m.x0;
	float x1 = shade->u.m.x1;
	float y0 = shade->u.m.y0;
	float y1 = shade->u.m.y1;
	const float *c0 = shade->u.m.c0;
	const float *c1 = shade->u.m.c1;
	float x, y, c[FZ_MAX_COLORS];
	int first_triangle = 1;

	fz_try(ctx)
	{
		while (!fz_is_eof_bits(ctx, stream))
		{
			flag = fz_read_bits(ctx, stream, bpflag);
			x = read_sample(ctx, stream, bpcoord, x0, x1);
			y = read_sample(ctx, stream, bpcoord, y0, y1);
			for (i = 0; i < ncomp; i++)
				c[i] = read_sample(ctx, stream, bpcomp, c0[i], c1[i]);
			fz_prepare_vertex(ctx, painter, vd, ctm, x, y, c);

			if (first_triangle)
			{
				if (flag != 0)
				{
					fz_warn(ctx, "ignoring non-zero edge flags for first vertex in mesh");
					flag = 0;
				}
				first_triangle = 0;
			}

			switch (flag)
			{
			default:
				fz_warn(ctx, "ignoring out of range edge flag in mesh");
				/* fallthrough */

			case 0: /* start new triangle */
				SWAP(va, vd);

				fz_read_bits(ctx, stream, bpflag);
				x = read_sample(ctx, stream, bpcoord, x0, x1);
				y = read_sample(ctx, stream, bpcoord, y0, y1);
				for (i = 0; i < ncomp; i++)
					c[i] = read_sample(ctx, stream, bpcomp, c0[i], c1[i]);
				fz_prepare_vertex(ctx, painter, vb, ctm, x, y, c);

				fz_read_bits(ctx, stream, bpflag);
				x = read_sample(ctx, stream, bpcoord, x0, x1);
				y = read_sample(ctx, stream, bpcoord, y0, y1);
				for (i = 0; i < ncomp; i++)
					c[i] = read_sample(ctx, stream, bpcomp, c0[i], c1[i]);
				fz_prepare_vertex(ctx, painter, vc, ctm, x, y, c);

				paint_tri(ctx, painter, va, vb, vc);
				break;

			case 1: /* Vb, Vc, Vd */
				SWAP(va, vb);
				SWAP(vb, vc);
				SWAP(vc, vd);
				paint_tri(ctx, painter, va, vb, vc);
				break;

			case 2: /* Va, Vc, Vd */
				SWAP(vb, vc);
				SWAP(vc, vd);
				paint_tri(ctx, painter, va, vb, vc);
				break;
			}
		}
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_process_shade_type5(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter)
{
	fz_stream *stream = fz_open_compressed_buffer(ctx, shade->buffer);
	fz_vertex *buf = NULL;
	fz_vertex *ref = NULL;
	int first;
	int ncomp = painter->ncomp;
	int i, k;
	int vprow = shade->u.m.vprow;
	int bpcoord = shade->u.m.bpcoord;
	int bpcomp = shade->u.m.bpcomp;
	float x0 = shade->u.m.x0;
	float x1 = shade->u.m.x1;
	float y0 = shade->u.m.y0;
	float y1 = shade->u.m.y1;
	const float *c0 = shade->u.m.c0;
	const float *c1 = shade->u.m.c1;
	float x, y, c[FZ_MAX_COLORS];

	fz_var(buf);
	fz_var(ref);

	fz_try(ctx)
	{
		ref = fz_malloc_array(ctx, vprow, fz_vertex);
		buf = fz_malloc_array(ctx, vprow, fz_vertex);
		first = 1;

		while (!fz_is_eof_bits(ctx, stream))
		{
			for (i = 0; i < vprow; i++)
			{
				x = read_sample(ctx, stream, bpcoord, x0, x1);
				y = read_sample(ctx, stream, bpcoord, y0, y1);
				for (k = 0; k < ncomp; k++)
					c[k] = read_sample(ctx, stream, bpcomp, c0[k], c1[k]);
				fz_prepare_vertex(ctx, painter, &buf[i], ctm, x, y, c);
			}

			if (!first)
				for (i = 0; i < vprow - 1; i++)
					paint_quad(ctx, painter, &ref[i], &ref[i+1], &buf[i+1], &buf[i]);

			SWAP(ref,buf);
			first = 0;
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, ref);
		fz_free(ctx, buf);
		fz_drop_stream(ctx, stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/* Subdivide and tessellate tensor-patches */

typedef struct tensor_patch_s tensor_patch;

struct tensor_patch_s
{
	fz_point pole[4][4];
	float color[4][FZ_MAX_COLORS];
};

static void
triangulate_patch(fz_context *ctx, fz_mesh_processor *painter, tensor_patch *p)
{
	fz_vertex v0, v1, v2, v3;

	v0.p = p->pole[0][0];
	v1.p = p->pole[0][3];
	v2.p = p->pole[3][3];
	v3.p = p->pole[3][0];

	fz_prepare_color(ctx, painter, &v0, p->color[0]);
	fz_prepare_color(ctx, painter, &v1, p->color[1]);
	fz_prepare_color(ctx, painter, &v2, p->color[2]);
	fz_prepare_color(ctx, painter, &v3, p->color[3]);

	paint_quad(ctx, painter, &v0, &v1, &v2, &v3);
}

static inline void midcolor(float *c, float *c1, float *c2, int n)
{
	int i;
	for (i = 0; i < n; i++)
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
split_stripe(tensor_patch *p, tensor_patch *s0, tensor_patch *s1, int n)
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
	memcpy(s0->color[0], p->color[0], n * sizeof(s0->color[0][0]));
	memcpy(s0->color[1], p->color[1], n * sizeof(s0->color[1][0]));
	midcolor(s0->color[2], p->color[1], p->color[2], n);
	midcolor(s0->color[3], p->color[0], p->color[3], n);

	memcpy(s1->color[0], s0->color[3], n * sizeof(s1->color[0][0]));
	memcpy(s1->color[1], s0->color[2], n * sizeof(s1->color[1][0]));
	memcpy(s1->color[2], p->color[2], n * sizeof(s1->color[2][0]));
	memcpy(s1->color[3], p->color[3], n * sizeof(s1->color[3][0]));
}

static void
draw_stripe(fz_context *ctx, fz_mesh_processor *painter, tensor_patch *p, int depth)
{
	tensor_patch s0, s1;

	/* split patch into two half-height patches */
	split_stripe(p, &s0, &s1, painter->ncomp);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		triangulate_patch(ctx, painter, &s1);
		triangulate_patch(ctx, painter, &s0);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		draw_stripe(ctx, painter, &s1, depth);
		draw_stripe(ctx, painter, &s0, depth);
	}
}

static void
split_patch(tensor_patch *p, tensor_patch *s0, tensor_patch *s1, int n)
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
	memcpy(s0->color[0], p->color[0], n * sizeof(s0->color[0][0]));
	midcolor(s0->color[1], p->color[0], p->color[1], n);
	midcolor(s0->color[2], p->color[2], p->color[3], n);
	memcpy(s0->color[3], p->color[3], n * sizeof(s0->color[3][0]));

	memcpy(s1->color[0], s0->color[1], n * sizeof(s1->color[0][0]));
	memcpy(s1->color[1], p->color[1], n * sizeof(s1->color[1][0]));
	memcpy(s1->color[2], p->color[2], n * sizeof(s1->color[2][0]));
	memcpy(s1->color[3], s0->color[2], n * sizeof(s1->color[3][0]));
}

static void
draw_patch(fz_context *ctx, fz_mesh_processor *painter, tensor_patch *p, int depth, int origdepth)
{
	tensor_patch s0, s1;

	/* split patch into two half-width patches */
	split_patch(p, &s0, &s1, painter->ncomp);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		draw_stripe(ctx, painter, &s0, origdepth);
		draw_stripe(ctx, painter, &s1, origdepth);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		draw_patch(ctx, painter, &s0, depth, origdepth);
		draw_patch(ctx, painter, &s1, depth, origdepth);
	}
}

static fz_point
compute_tensor_interior(
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
make_tensor_patch(tensor_patch *p, int type, fz_point *pt)
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

		p->pole[1][1] = compute_tensor_interior(
			p->pole[0][0], p->pole[0][1], p->pole[1][0], p->pole[0][3],
			p->pole[3][0], p->pole[3][1], p->pole[1][3], p->pole[3][3]);

		p->pole[1][2] = compute_tensor_interior(
			p->pole[0][3], p->pole[0][2], p->pole[1][3], p->pole[0][0],
			p->pole[3][3], p->pole[3][2], p->pole[1][0], p->pole[3][0]);

		p->pole[2][1] = compute_tensor_interior(
			p->pole[3][0], p->pole[3][1], p->pole[2][0], p->pole[3][3],
			p->pole[0][0], p->pole[0][1], p->pole[2][3], p->pole[0][3]);

		p->pole[2][2] = compute_tensor_interior(
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

/* FIXME: Nasty */
#define SUBDIV 3 /* how many levels to subdivide patches */

static void
fz_process_shade_type6(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter)
{
	fz_stream *stream = fz_open_compressed_buffer(ctx, shade->buffer);
	float color_storage[2][4][FZ_MAX_COLORS];
	fz_point point_storage[2][12];
	int store = 0;
	int ncomp = painter->ncomp;
	int i, k;
	int bpflag = shade->u.m.bpflag;
	int bpcoord = shade->u.m.bpcoord;
	int bpcomp = shade->u.m.bpcomp;
	float x0 = shade->u.m.x0;
	float x1 = shade->u.m.x1;
	float y0 = shade->u.m.y0;
	float y1 = shade->u.m.y1;
	const float *c0 = shade->u.m.c0;
	const float *c1 = shade->u.m.c1;

	fz_try(ctx)
	{
		float (*prevc)[FZ_MAX_COLORS] = NULL;
		fz_point *prevp = NULL;
		while (!fz_is_eof_bits(ctx, stream))
		{
			float (*c)[FZ_MAX_COLORS] = color_storage[store];
			fz_point *v = point_storage[store];
			int startcolor;
			int startpt;
			int flag;
			tensor_patch patch;

			flag = fz_read_bits(ctx, stream, bpflag);

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
				v[i].x = read_sample(ctx, stream, bpcoord, x0, x1);
				v[i].y = read_sample(ctx, stream, bpcoord, y0, y1);
				v[i] = fz_transform_point(v[i], ctm);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < ncomp; k++)
					c[i][k] = read_sample(ctx, stream, bpcomp, c0[k], c1[k]);
			}

			if (flag == 0)
			{
			}
			else if (flag == 1 && prevc)
			{
				v[0] = prevp[3];
				v[1] = prevp[4];
				v[2] = prevp[5];
				v[3] = prevp[6];
				memcpy(c[0], prevc[1], ncomp * sizeof(float));
				memcpy(c[1], prevc[2], ncomp * sizeof(float));
			}
			else if (flag == 2 && prevc)
			{
				v[0] = prevp[6];
				v[1] = prevp[7];
				v[2] = prevp[8];
				v[3] = prevp[9];
				memcpy(c[0], prevc[2], ncomp * sizeof(float));
				memcpy(c[1], prevc[3], ncomp * sizeof(float));
			}
			else if (flag == 3 && prevc)
			{
				v[0] = prevp[ 9];
				v[1] = prevp[10];
				v[2] = prevp[11];
				v[3] = prevp[ 0];
				memcpy(c[0], prevc[3], ncomp * sizeof(float));
				memcpy(c[1], prevc[0], ncomp * sizeof(float));
			}
			else
				continue;

			make_tensor_patch(&patch, 6, v);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], ncomp * sizeof(float));

			draw_patch(ctx, painter, &patch, SUBDIV, SUBDIV);

			prevp = v;
			prevc = c;
			store ^= 1;
		}
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_process_shade_type7(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_mesh_processor *painter)
{
	fz_stream *stream = fz_open_compressed_buffer(ctx, shade->buffer);
	int bpflag = shade->u.m.bpflag;
	int bpcoord = shade->u.m.bpcoord;
	int bpcomp = shade->u.m.bpcomp;
	float x0 = shade->u.m.x0;
	float x1 = shade->u.m.x1;
	float y0 = shade->u.m.y0;
	float y1 = shade->u.m.y1;
	const float *c0 = shade->u.m.c0;
	const float *c1 = shade->u.m.c1;
	float color_storage[2][4][FZ_MAX_COLORS];
	fz_point point_storage[2][16];
	int store = 0;
	int ncomp = painter->ncomp;
	int i, k;
	float (*prevc)[FZ_MAX_COLORS] = NULL;
	fz_point (*prevp) = NULL;

	fz_try(ctx)
	{
		while (!fz_is_eof_bits(ctx, stream))
		{
			float (*c)[FZ_MAX_COLORS] = color_storage[store];
			fz_point *v = point_storage[store];
			int startcolor;
			int startpt;
			int flag;
			tensor_patch patch;

			flag = fz_read_bits(ctx, stream, bpflag);

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
				v[i].x = read_sample(ctx, stream, bpcoord, x0, x1);
				v[i].y = read_sample(ctx, stream, bpcoord, y0, y1);
				v[i] = fz_transform_point(v[i], ctm);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < ncomp; k++)
					c[i][k] = read_sample(ctx, stream, bpcomp, c0[k], c1[k]);
			}

			if (flag == 0)
			{
			}
			else if (flag == 1 && prevc)
			{
				v[0] = prevp[3];
				v[1] = prevp[4];
				v[2] = prevp[5];
				v[3] = prevp[6];
				memcpy(c[0], prevc[1], ncomp * sizeof(float));
				memcpy(c[1], prevc[2], ncomp * sizeof(float));
			}
			else if (flag == 2 && prevc)
			{
				v[0] = prevp[6];
				v[1] = prevp[7];
				v[2] = prevp[8];
				v[3] = prevp[9];
				memcpy(c[0], prevc[2], ncomp * sizeof(float));
				memcpy(c[1], prevc[3], ncomp * sizeof(float));
			}
			else if (flag == 3 && prevc)
			{
				v[0] = prevp[ 9];
				v[1] = prevp[10];
				v[2] = prevp[11];
				v[3] = prevp[ 0];
				memcpy(c[0], prevc[3], ncomp * sizeof(float));
				memcpy(c[1], prevc[0], ncomp * sizeof(float));
			}
			else
				continue; /* We have no patch! */

			make_tensor_patch(&patch, 7, v);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], ncomp * sizeof(float));

			draw_patch(ctx, painter, &patch, SUBDIV, SUBDIV);

			prevp = v;
			prevc = c;
			store ^= 1;
		}
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
	Process a shade, using supplied callback
	functions. This decomposes the shading to a mesh (even ones
	that are not natively meshes, such as linear or radial
	shadings), and processes triangles from those meshes.

	shade: The shade to process.

	ctm: The transform to use

	prepare: Callback function to 'prepare' each vertex.
	This function is passed an array of floats, and populates
	a fz_vertex structure.

	process: This function is passed 3 pointers to vertex
	structures, and actually performs the processing (typically
	filling the area between the vertexes).

	process_arg: An opaque argument passed through from caller
	to callback functions.
*/
void
fz_process_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_rect scissor,
		fz_shade_prepare_fn *prepare, fz_shade_process_fn *process, void *process_arg)
{
	fz_mesh_processor painter;

	painter.shade = shade;
	painter.prepare = prepare;
	painter.process = process;
	painter.process_arg = process_arg;
	painter.ncomp = (shade->use_function > 0 ? 1 : fz_colorspace_n(ctx, shade->colorspace));

	if (shade->type == FZ_FUNCTION_BASED)
		fz_process_shade_type1(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_LINEAR)
		fz_process_shade_type2(ctx, shade, ctm, &painter, scissor);
	else if (shade->type == FZ_RADIAL)
		fz_process_shade_type3(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE4)
		fz_process_shade_type4(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE5)
		fz_process_shade_type5(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE6)
		fz_process_shade_type6(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE7)
		fz_process_shade_type7(ctx, shade, ctm, &painter);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected mesh type %d\n", shade->type);
}

static fz_rect
fz_bound_mesh_type1(fz_context *ctx, fz_shade *shade)
{
	fz_rect bbox;
	bbox.x0 = shade->u.f.domain[0][0];
	bbox.y0 = shade->u.f.domain[0][1];
	bbox.x1 = shade->u.f.domain[1][0];
	bbox.y1 = shade->u.f.domain[1][1];
	return fz_transform_rect(bbox, shade->u.f.matrix);
}

static fz_rect
fz_bound_mesh_type2(fz_context *ctx, fz_shade *shade)
{
	/* FIXME: If axis aligned and not extended, the bbox may only be
	 * infinite in one direction */
	return fz_infinite_rect;
}

static fz_rect
fz_bound_mesh_type3(fz_context *ctx, fz_shade *shade)
{
	fz_rect bbox;
	fz_point p0, p1;
	float r0, r1;

	r0 = shade->u.l_or_r.coords[0][2];
	r1 = shade->u.l_or_r.coords[1][2];

	if (shade->u.l_or_r.extend[0])
	{
		if (r0 >= r1)
			return fz_infinite_rect;
	}

	if (shade->u.l_or_r.extend[1])
	{
		if (r0 <= r1)
			return fz_infinite_rect;
	}

	p0.x = shade->u.l_or_r.coords[0][0];
	p0.y = shade->u.l_or_r.coords[0][1];
	p1.x = shade->u.l_or_r.coords[1][0];
	p1.y = shade->u.l_or_r.coords[1][1];

	bbox.x0 = p0.x - r0; bbox.y0 = p0.y - r0;
	bbox.x1 = p0.x + r0; bbox.y1 = p0.x + r0;
	if (bbox.x0 > p1.x - r1)
		bbox.x0 = p1.x - r1;
	if (bbox.x1 < p1.x + r1)
		bbox.x1 = p1.x + r1;
	if (bbox.y0 > p1.y - r1)
		bbox.y0 = p1.y - r1;
	if (bbox.y1 < p1.y + r1)
		bbox.y1 = p1.y + r1;
	return bbox;
}

static fz_rect
fz_bound_mesh_type4567(fz_context *ctx, fz_shade *shade)
{
	fz_rect bbox;
	bbox.x0 = shade->u.m.x0;
	bbox.y0 = shade->u.m.y0;
	bbox.x1 = shade->u.m.x1;
	bbox.y1 = shade->u.m.y1;
	return bbox;
}

static fz_rect
fz_bound_mesh(fz_context *ctx, fz_shade *shade)
{
	if (shade->type == FZ_FUNCTION_BASED)
		return fz_bound_mesh_type1(ctx, shade);
	else if (shade->type == FZ_LINEAR)
		return fz_bound_mesh_type2(ctx, shade);
	else if (shade->type == FZ_RADIAL)
		return fz_bound_mesh_type3(ctx, shade);
	else if (shade->type == FZ_MESH_TYPE4 ||
		shade->type == FZ_MESH_TYPE5 ||
		shade->type == FZ_MESH_TYPE6 ||
		shade->type == FZ_MESH_TYPE7)
		return fz_bound_mesh_type4567(ctx, shade);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected mesh type %d\n", shade->type);
}

fz_shade *
fz_keep_shade(fz_context *ctx, fz_shade *shade)
{
	return fz_keep_storable(ctx, &shade->storable);
}

/*
	Internal function to destroy a
	shade. Only exposed for use with the fz_store.

	shade: The reference to destroy.
*/
void
fz_drop_shade_imp(fz_context *ctx, fz_storable *shade_)
{
	fz_shade *shade = (fz_shade *)shade_;

	fz_drop_colorspace(ctx, shade->colorspace);
	if (shade->type == FZ_FUNCTION_BASED)
		fz_free(ctx, shade->u.f.fn_vals);
	fz_drop_compressed_buffer(ctx, shade->buffer);
	fz_free(ctx, shade);
}

void
fz_drop_shade(fz_context *ctx, fz_shade *shade)
{
	fz_drop_storable(ctx, &shade->storable);
}

/*
	Bound a given shading.

	shade: The shade to bound.

	ctm: The transform to apply to the shade before bounding.

	r: Pointer to storage to put the bounds in.

	Returns r, updated to contain the bounds for the shading.
*/
fz_rect
fz_bound_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm)
{
	ctm = fz_concat(shade->matrix, ctm);
	if (shade->type != FZ_LINEAR && shade->type != FZ_RADIAL)
	{
		fz_rect rect = fz_bound_mesh(ctx, shade);
		rect = fz_intersect_rect(rect, shade->bbox);
		return fz_transform_rect(rect, ctm);
	}
	return fz_transform_rect(shade->bbox, ctm);
}
