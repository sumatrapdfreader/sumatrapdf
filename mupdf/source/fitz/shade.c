#include "mupdf/fitz.h"

#define SWAP(a,b) {fz_vertex *t = (a); (a) = (b); (b) = t;}

static void
paint_tri(fz_mesh_processor *painter, fz_vertex *v0, fz_vertex *v1, fz_vertex *v2)
{
	painter->process(painter->process_arg, v0, v1, v2);
}

static void
paint_quad(fz_mesh_processor *painter, fz_vertex *v0, fz_vertex *v1, fz_vertex *v2, fz_vertex *v3)
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
	painter->process(painter->process_arg, v0, v1, v3);
	painter->process(painter->process_arg, v3, v2, v1);
}

static void
fz_process_mesh_type1(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
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
	int n = shade->colorspace->n;
	fz_matrix local_ctm;

	fz_concat(&local_ctm, &shade->u.f.matrix, ctm);

	y = y0;
	for (yy = 0; yy < ydivs; yy++)
	{
		yn = y0 + (y1 - y0) * (yy + 1) / ydivs;

		x = x0;
		v[0].p.x = x; v[0].p.y = y;
		fz_transform_point(&v[0].p, &local_ctm);
		memcpy(v[0].c, p, n*sizeof(float));
		p += n;
		v[1].p.x = x; v[1].p.y = yn;
		fz_transform_point(&v[1].p, &local_ctm);
		memcpy(v[1].c, p + xdivs*n, n*sizeof(float));
		for (xx = 0; xx < xdivs; xx++)
		{
			x = x0 + (x1 - x0) * (xx + 1) / xdivs;

			vn[0].p.x = x; vn[0].p.y = y;
			fz_transform_point(&vn[0].p, &local_ctm);
			memcpy(vn[0].c, p, n*sizeof(float));
			p += n;
			vn[1].p.x = x; vn[1].p.y = yn;
			fz_transform_point(&vn[1].p, &local_ctm);
			memcpy(vn[1].c, p + xdivs*n, n*sizeof(float));

			paint_quad(painter, &v[0], &vn[0], &vn[1], &v[1]);
			SWAP(v,vn);
		}
		y = yn;
	}
}

/* FIXME: Nasty */
#define HUGENUM 32000 /* how far to extend linear/radial shadings */

static fz_point
fz_point_on_circle(fz_point p, float r, float theta)
{
	p.x = p.x + cosf(theta) * r;
	p.y = p.y + sinf(theta) * r;

	return p;
}

static void
fz_process_mesh_type2(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
{
	fz_point p0, p1, dir;
	fz_vertex v0, v1, v2, v3;
	fz_vertex e0, e1;
	float theta;

	p0.x = shade->u.l_or_r.coords[0][0];
	p0.y = shade->u.l_or_r.coords[0][1];
	p1.x = shade->u.l_or_r.coords[1][0];
	p1.y = shade->u.l_or_r.coords[1][1];
	dir.x = p0.y - p1.y;
	dir.y = p1.x - p0.x;
	fz_transform_point(&p0, ctm);
	fz_transform_point(&p1, ctm);
	fz_transform_vector(&dir, ctm);
	theta = atan2f(dir.y, dir.x);

	v0.p = fz_point_on_circle(p0, HUGENUM, theta);
	v1.p = fz_point_on_circle(p1, HUGENUM, theta);
	v2.p = fz_point_on_circle(p0, -HUGENUM, theta);
	v3.p = fz_point_on_circle(p1, -HUGENUM, theta);

	v0.c[0] = 0;
	v1.c[0] = 1;
	v2.c[0] = 0;
	v3.c[0] = 1;

	paint_quad(painter, &v0, &v2, &v3, &v1);

	if (shade->u.l_or_r.extend[0])
	{
		e0.p.x = v0.p.x - (p1.x - p0.x) * HUGENUM;
		e0.p.y = v0.p.y - (p1.y - p0.y) * HUGENUM;

		e1.p.x = v2.p.x - (p1.x - p0.x) * HUGENUM;
		e1.p.y = v2.p.y - (p1.y - p0.y) * HUGENUM;

		e0.c[0] = 0;
		e1.c[0] = 0;
		v0.c[0] = 0;
		v2.c[0] = 0;

		paint_quad(painter, &e0, &v0, &v2, &e1);
	}

	if (shade->u.l_or_r.extend[1])
	{
		e0.p.x = v1.p.x + (p1.x - p0.x) * HUGENUM;
		e0.p.y = v1.p.y + (p1.y - p0.y) * HUGENUM;

		e1.p.x = v3.p.x + (p1.x - p0.x) * HUGENUM;
		e1.p.y = v3.p.y + (p1.y - p0.y) * HUGENUM;

		e0.c[0] = 1;
		e1.c[0] = 1;
		v1.c[0] = 1;
		v3.c[0] = 1;

		paint_quad(painter, &e0, &v1, &v3, &e1);
	}
}

/* FIXME: Nasty */
#define RADSEGS 32 /* how many segments to generate for radial meshes */

static void
fz_paint_annulus(const fz_matrix *ctm,
		fz_point p0, float r0, float c0,
		fz_point p1, float r1, float c1,
		fz_mesh_processor *painter)
{
	fz_vertex t0, t1, t2, t3, b0, b1, b2, b3;
	float theta, step;
	int i;

	theta = atan2f(p1.y - p0.y, p1.x - p0.x);
	step = (float)M_PI * 2 / RADSEGS;

	for (i = 0; i < RADSEGS / 2; i++)
	{
		t0.p = fz_point_on_circle(p0, r0, theta + i * step);
		t1.p = fz_point_on_circle(p0, r0, theta + i * step + step);
		t2.p = fz_point_on_circle(p1, r1, theta + i * step);
		t3.p = fz_point_on_circle(p1, r1, theta + i * step + step);
		b0.p = fz_point_on_circle(p0, r0, theta - i * step);
		b1.p = fz_point_on_circle(p0, r0, theta - i * step - step);
		b2.p = fz_point_on_circle(p1, r1, theta - i * step);
		b3.p = fz_point_on_circle(p1, r1, theta - i * step - step);

		fz_transform_point(&t0.p, ctm);
		fz_transform_point(&t1.p, ctm);
		fz_transform_point(&t2.p, ctm);
		fz_transform_point(&t3.p, ctm);
		fz_transform_point(&b0.p, ctm);
		fz_transform_point(&b1.p, ctm);
		fz_transform_point(&b2.p, ctm);
		fz_transform_point(&b3.p, ctm);

		t0.c[0] = c0;
		t1.c[0] = c0;
		t2.c[0] = c1;
		t3.c[0] = c1;
		b0.c[0] = c0;
		b1.c[0] = c0;
		b2.c[0] = c1;
		b3.c[0] = c1;

		paint_quad(painter, &t0, &t2, &t3, &t1);
		paint_quad(painter, &b0, &b2, &b3, &b1);
	}
}

static void
fz_process_mesh_type3(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
{
	fz_point p0, p1;
	float r0, r1;
	fz_point e;
	float er, rs;

	p0.x = shade->u.l_or_r.coords[0][0];
	p0.y = shade->u.l_or_r.coords[0][1];
	r0 = shade->u.l_or_r.coords[0][2];

	p1.x = shade->u.l_or_r.coords[1][0];
	p1.y = shade->u.l_or_r.coords[1][1];
	r1 = shade->u.l_or_r.coords[1][2];

	if (shade->u.l_or_r.extend[0])
	{
		if (r0 < r1)
			rs = r0 / (r0 - r1);
		else
			rs = -HUGENUM;

		e.x = p0.x + (p1.x - p0.x) * rs;
		e.y = p0.y + (p1.y - p0.y) * rs;
		er = r0 + (r1 - r0) * rs;

		fz_paint_annulus(ctm, e, er, 0, p0, r0, 0, painter);
	}

	fz_paint_annulus(ctm, p0, r0, 0, p1, r1, 1, painter);

	if (shade->u.l_or_r.extend[1])
	{
		if (r0 > r1)
			rs = r1 / (r1 - r0);
		else
			rs = -HUGENUM;

		e.x = p1.x + (p0.x - p1.x) * rs;
		e.y = p1.y + (p0.y - p1.y) * rs;
		er = r1 + (r0 - r1) * rs;

		fz_paint_annulus(ctm, p1, r1, 1, e, er, 1, painter);
	}
}

static inline float read_sample(fz_stream *stream, int bits, float min, float max)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = 1 / (powf(2, bits) - 1);
	return min + fz_read_bits(stream, bits) * (max - min) * bitscale;
}

static void
fz_process_mesh_type4(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
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
	float *c0 = shade->u.m.c0;
	float *c1 = shade->u.m.c1;

	fz_try(ctx)
	{
		while (!fz_is_eof_bits(stream))
		{
			flag = fz_read_bits(stream, bpflag);
			vd->p.x = read_sample(stream, bpcoord, x0, x1);
			vd->p.y = read_sample(stream, bpcoord, y0, y1);
			fz_transform_point(&vd->p, ctm);
			for (i = 0; i < ncomp; i++)
				vd->c[i] = read_sample(stream, bpcomp, c0[i], c1[i]);

			switch (flag)
			{
			case 0: /* start new triangle */
				SWAP(va, vd);

				fz_read_bits(stream, bpflag);
				vb->p.x = read_sample(stream, bpcoord, x0, x1);
				vb->p.y = read_sample(stream, bpcoord, y0, y1);
				fz_transform_point(&vb->p, ctm);
				for (i = 0; i < ncomp; i++)
					vb->c[i] = read_sample(stream, bpcomp, c0[i], c1[i]);

				fz_read_bits(stream, bpflag);
				vc->p.x = read_sample(stream, bpcoord, x0, x1);
				vc->p.y = read_sample(stream, bpcoord, y0, y1);
				fz_transform_point(&vc->p, ctm);
				for (i = 0; i < ncomp; i++)
					vc->c[i] = read_sample(stream, bpcomp, c0[i], c1[i]);

				paint_tri(painter, va, vb, vc);
				break;

			case 1: /* Vb, Vc, Vd */
				SWAP(va, vb);
				SWAP(vb, vc);
				SWAP(vc, vd);
				paint_tri(painter, va, vb, vc);
				break;

			case 2: /* Va, Vc, Vd */
				SWAP(vb, vc);
				SWAP(vc, vd);
				paint_tri(painter, va, vb, vc);
				break;
			}
		}
	}
	fz_always(ctx)
	{
		fz_close(stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_process_mesh_type5(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
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
	float *c0 = shade->u.m.c0;
	float *c1 = shade->u.m.c1;

	fz_var(buf);
	fz_var(ref);

	fz_try(ctx)
	{
		ref = fz_malloc_array(ctx, vprow, sizeof(fz_vertex));
		buf = fz_malloc_array(ctx, vprow, sizeof(fz_vertex));
		first = 1;

		while (!fz_is_eof_bits(stream))
		{
			for (i = 0; i < vprow; i++)
			{
				buf[i].p.x = read_sample(stream, bpcoord, x0, x1);
				buf[i].p.y = read_sample(stream, bpcoord, y0, y1);
				fz_transform_point(&buf[i].p, ctm);
				for (k = 0; k < ncomp; k++)
					buf[i].c[k] = read_sample(stream, bpcomp, c0[k], c1[k]);
			}

			if (!first)
				for (i = 0; i < vprow - 1; i++)
					paint_quad(painter, &ref[i], &ref[i+1], &buf[i+1], &buf[i]);

			SWAP(ref,buf);
			first = 0;
		}
	}
	fz_always(ctx)
	{
		fz_free(ctx, ref);
		fz_free(ctx, buf);
		fz_close(stream);
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
triangulate_patch(fz_mesh_processor *painter, tensor_patch p)
{
	fz_vertex v0, v1, v2, v3;
	int col_len = painter->ncomp * sizeof(v0.c[0]);

	v0.p = p.pole[0][0];
	memcpy(v0.c, p.color[0], col_len);

	v1.p = p.pole[0][3];
	memcpy(v1.c, p.color[1], col_len);

	v2.p = p.pole[3][3];
	memcpy(v2.c, p.color[2], col_len);

	v3.p = p.pole[3][0];
	memcpy(v3.c, p.color[3], col_len);

	paint_quad(painter, &v0, &v1, &v2, &v3);
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
draw_stripe(fz_mesh_processor *painter, tensor_patch *p, int depth)
{
	tensor_patch s0, s1;

	/* split patch into two half-height patches */
	split_stripe(p, &s0, &s1, painter->ncomp);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		triangulate_patch(painter, s1);
		triangulate_patch(painter, s0);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		draw_stripe(painter, &s1, depth);
		draw_stripe(painter, &s0, depth);
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
draw_patch(fz_mesh_processor *painter, tensor_patch *p, int depth, int origdepth)
{
	tensor_patch s0, s1;

	/* split patch into two half-width patches */
	split_patch(p, &s0, &s1, painter->ncomp);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		draw_stripe(painter, &s0, origdepth);
		draw_stripe(painter, &s1, origdepth);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		draw_patch(painter, &s0, depth, origdepth);
		draw_patch(painter, &s1, depth, origdepth);
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
fz_process_mesh_type6(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
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
	float *c0 = shade->u.m.c0;
	float *c1 = shade->u.m.c1;

	fz_try(ctx)
	{
		float (*prevc)[FZ_MAX_COLORS] = NULL;
		fz_point *prevp = NULL;
		while (!fz_is_eof_bits(stream))
		{
			float (*c)[FZ_MAX_COLORS] = color_storage[store];
			fz_point *v = point_storage[store];
			int startcolor;
			int startpt;
			int flag;
			tensor_patch patch;

			flag = fz_read_bits(stream, bpflag);

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
				v[i].x = read_sample(stream, bpcoord, x0, x1);
				v[i].y = read_sample(stream, bpcoord, y0, y1);
				fz_transform_point(&v[i], ctm);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < ncomp; k++)
					c[i][k] = read_sample(stream, bpcomp, c0[k], c1[k]);
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

			draw_patch(painter, &patch, SUBDIV, SUBDIV);

			prevp = v;
			prevc = c;
			store ^= 1;
		}
	}
	fz_always(ctx)
	{
		fz_close(stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
fz_process_mesh_type7(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_mesh_processor *painter)
{
	fz_stream *stream = fz_open_compressed_buffer(ctx, shade->buffer);
	int bpflag = shade->u.m.bpflag;
	int bpcoord = shade->u.m.bpcoord;
	int bpcomp = shade->u.m.bpcomp;
	float x0 = shade->u.m.x0;
	float x1 = shade->u.m.x1;
	float y0 = shade->u.m.y0;
	float y1 = shade->u.m.y1;
	float *c0 = shade->u.m.c0;
	float *c1 = shade->u.m.c1;
	float color_storage[2][4][FZ_MAX_COLORS];
	fz_point point_storage[2][16];
	int store = 0;
	int ncomp = painter->ncomp;
	int i, k;
	float (*prevc)[FZ_MAX_COLORS] = NULL;
	fz_point (*prevp) = NULL;

	fz_try(ctx)
	{
		while (!fz_is_eof_bits(stream))
		{
			float (*c)[FZ_MAX_COLORS] = color_storage[store];
			fz_point *v = point_storage[store];
			int startcolor;
			int startpt;
			int flag;
			tensor_patch patch;

			flag = fz_read_bits(stream, bpflag);

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
				v[i].x = read_sample(stream, bpcoord, x0, x1);
				v[i].y = read_sample(stream, bpcoord, y0, y1);
				fz_transform_point(&v[i], ctm);
			}

			for (i = startcolor; i < 4; i++)
			{
				for (k = 0; k < ncomp; k++)
					c[i][k] = read_sample(stream, bpcomp, c0[k], c1[k]);
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

			draw_patch(painter, &patch, SUBDIV, SUBDIV);

			prevp = v;
			prevc = c;
			store ^= 1;
		}
	}
	fz_always(ctx)
	{
		fz_close(stream);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_process_mesh(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm,
		fz_mesh_process_fn *process, void *process_arg)
{
	fz_mesh_processor painter;

	painter.ctx = ctx;
	painter.shade = shade;
	painter.process = process;
	painter.process_arg = process_arg;
	painter.ncomp = (shade->use_function > 0 ? 1 : shade->colorspace->n);

	if (shade->type == FZ_FUNCTION_BASED)
		fz_process_mesh_type1(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_LINEAR)
		fz_process_mesh_type2(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_RADIAL)
		fz_process_mesh_type3(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE4)
		fz_process_mesh_type4(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE5)
		fz_process_mesh_type5(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE6)
		fz_process_mesh_type6(ctx, shade, ctm, &painter);
	else if (shade->type == FZ_MESH_TYPE7)
		fz_process_mesh_type7(ctx, shade, ctm, &painter);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected mesh type %d\n", shade->type);
}

static fz_rect *
fz_bound_mesh_type1(fz_context *ctx, fz_shade *shade, fz_rect *bbox)
{
	bbox->x0 = shade->u.f.domain[0][0];
	bbox->y0 = shade->u.f.domain[0][1];
	bbox->x1 = shade->u.f.domain[1][0];
	bbox->y1 = shade->u.f.domain[1][1];
	return fz_transform_rect(bbox, &shade->u.f.matrix);
}

static fz_rect *
fz_bound_mesh_type2(fz_context *ctx, fz_shade *shade, fz_rect *bbox)
{
	/* FIXME: If axis aligned and not extended, the bbox may only be
	 * infinite in one direction */
	*bbox = fz_infinite_rect;
	return bbox;
}

static fz_rect *
fz_bound_mesh_type3(fz_context *ctx, fz_shade *shade, fz_rect *bbox)
{
	fz_point p0, p1;
	float r0, r1;

	r0 = shade->u.l_or_r.coords[0][2];
	r1 = shade->u.l_or_r.coords[1][2];

	if (shade->u.l_or_r.extend[0])
	{
		if (r0 >= r1)
		{
			*bbox = fz_infinite_rect;
			return bbox;
		}
	}

	if (shade->u.l_or_r.extend[1])
	{
		if (r0 <= r1)
		{
			*bbox = fz_infinite_rect;
			return bbox;
		}
	}

	p0.x = shade->u.l_or_r.coords[0][0];
	p0.y = shade->u.l_or_r.coords[0][1];
	p1.x = shade->u.l_or_r.coords[1][0];
	p1.y = shade->u.l_or_r.coords[1][1];

	bbox->x0 = p0.x - r0; bbox->y0 = p0.y - r0;
	bbox->x1 = p0.x + r0; bbox->y1 = p0.x + r0;
	if (bbox->x0 > p1.x - r1)
		bbox->x0 = p1.x - r1;
	if (bbox->x1 < p1.x + r1)
		bbox->x1 = p1.x + r1;
	if (bbox->y0 > p1.y - r1)
		bbox->y0 = p1.y - r1;
	if (bbox->y1 < p1.y + r1)
		bbox->y1 = p1.y + r1;
	return bbox;
}

static fz_rect *
fz_bound_mesh_type4567(fz_context *ctx, fz_shade *shade, fz_rect *bbox)
{
	bbox->x0 = shade->u.m.x0;
	bbox->y0 = shade->u.m.y0;
	bbox->x1 = shade->u.m.x1;
	bbox->y1 = shade->u.m.y1;
	return bbox;
}

static fz_rect *
fz_bound_mesh(fz_context *ctx, fz_shade *shade, fz_rect *bbox)
{
	if (shade->type == FZ_FUNCTION_BASED)
		fz_bound_mesh_type1(ctx, shade, bbox);
	else if (shade->type == FZ_LINEAR)
		fz_bound_mesh_type2(ctx, shade, bbox);
	else if (shade->type == FZ_RADIAL)
		fz_bound_mesh_type3(ctx, shade, bbox);
	else if (shade->type == FZ_MESH_TYPE4 ||
		shade->type == FZ_MESH_TYPE5 ||
		shade->type == FZ_MESH_TYPE6 ||
		shade->type == FZ_MESH_TYPE7)
		fz_bound_mesh_type4567(ctx, shade, bbox);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unexpected mesh type %d\n", shade->type);

	return bbox;
}

fz_shade *
fz_keep_shade(fz_context *ctx, fz_shade *shade)
{
	return (fz_shade *)fz_keep_storable(ctx, &shade->storable);
}

void
fz_free_shade_imp(fz_context *ctx, fz_storable *shade_)
{
	fz_shade *shade = (fz_shade *)shade_;

	if (shade->colorspace)
		fz_drop_colorspace(ctx, shade->colorspace);
	if (shade->type == FZ_FUNCTION_BASED)
		fz_free(ctx, shade->u.f.fn_vals);
	fz_free_compressed_buffer(ctx, shade->buffer);
	fz_free(ctx, shade);
}

void
fz_drop_shade(fz_context *ctx, fz_shade *shade)
{
	fz_drop_storable(ctx, &shade->storable);
}

fz_rect *
fz_bound_shade(fz_context *ctx, fz_shade *shade, const fz_matrix *ctm, fz_rect *s)
{
	fz_matrix local_ctm;
	fz_rect rect;

	fz_concat(&local_ctm, &shade->matrix, ctm);
	*s = shade->bbox;
	if (shade->type != FZ_LINEAR && shade->type != FZ_RADIAL)
	{
		fz_bound_mesh(ctx, shade, &rect);
		fz_intersect_rect(s, &rect);
	}
	return fz_transform_rect(s, &local_ctm);
}

#ifndef NDEBUG
void
fz_print_shade(fz_context *ctx, FILE *out, fz_shade *shade)
{
	int i;

	fprintf(out, "shading {\n");

	switch (shade->type)
	{
	case FZ_FUNCTION_BASED: fprintf(out, "\ttype function_based\n"); break;
	case FZ_LINEAR: fprintf(out, "\ttype linear\n"); break;
	case FZ_RADIAL: fprintf(out, "\ttype radial\n"); break;
	default: /* MESH */ fprintf(out, "\ttype mesh\n"); break;
	}

	fprintf(out, "\tbbox [%g %g %g %g]\n",
		shade->bbox.x0, shade->bbox.y0,
		shade->bbox.x1, shade->bbox.y1);

	fprintf(out, "\tcolorspace %s\n", shade->colorspace->name);

	fprintf(out, "\tmatrix [%g %g %g %g %g %g]\n",
			shade->matrix.a, shade->matrix.b, shade->matrix.c,
			shade->matrix.d, shade->matrix.e, shade->matrix.f);

	if (shade->use_background)
	{
		fprintf(out, "\tbackground [");
		for (i = 0; i < shade->colorspace->n; i++)
			fprintf(out, "%s%g", i == 0 ? "" : " ", shade->background[i]);
		fprintf(out, "]\n");
	}

	if (shade->use_function)
	{
		fprintf(out, "\tfunction\n");
	}

	fprintf(out, "}\n");
}
#endif
