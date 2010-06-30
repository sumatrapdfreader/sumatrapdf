#include "fitz.h"
#include "mupdf.h"

#define HUGENUM 32000 /* how far to extend axial/radial shadings */
#define FUNSEGS 32 /* size of sampled mesh for function-based shadings */
#define RADSEGS 36 /* how many segments to generate for radial meshes */
#define SUBDIV 3 /* how many levels to subdivide patches */

static void
pdf_growmesh(fz_shade *shade, int amount)
{
	if (shade->meshlen + amount < shade->meshcap)
		return;

	if (shade->meshcap == 0)
		shade->meshcap = 1024;

	while (shade->meshlen + amount > shade->meshcap)
		shade->meshcap = (shade->meshcap * 3) / 2;

	shade->mesh = fz_realloc(shade->mesh, sizeof(float) * shade->meshcap);
}

static void
pdf_addvertex(fz_shade *shade, float x, float y, float *color)
{
	int ncomp = shade->usefunction ? 1 : shade->cs->n;
	int i;
	pdf_growmesh(shade, 2 + ncomp);
	shade->mesh[shade->meshlen++] = x;
	shade->mesh[shade->meshlen++] = y;
	for (i = 0; i < ncomp; i++)
		shade->mesh[shade->meshlen++] = color[i];
}

static void
pdf_addtriangle(fz_shade *shade,
	float x0, float y0, float *color0,
	float x1, float y1, float *color1,
	float x2, float y2, float *color2)
{
	pdf_addvertex(shade, x0, y0, color0);
	pdf_addvertex(shade, x1, y1, color1);
	pdf_addvertex(shade, x2, y2, color2);
}

static void
pdf_addquad(fz_shade *shade,
	float x0, float y0, float *color0,
	float x1, float y1, float *color1,
	float x2, float y2, float *color2,
	float x3, float y3, float *color3)
{
	pdf_addtriangle(shade, x0, y0, color0, x1, y1, color1, x3, y3, color3);
	pdf_addtriangle(shade, x1, y1, color1, x3, y3, color3, x2, y2, color2);
}

/*
 * Subdivide and tesselate tensor-patches
 */

typedef struct pdf_tensorpatch_s pdf_tensorpatch;

struct pdf_tensorpatch_s
{
	fz_point pole[4][4];
	float color[4][FZ_MAXCOLORS];
};

static void
triangulatepatch(pdf_tensorpatch p, fz_shade *shade)
{
	pdf_addquad(shade,
		p.pole[0][0].x, p.pole[0][0].y, p.color[0],
		p.pole[0][3].x, p.pole[0][3].y, p.color[1],
		p.pole[3][3].x, p.pole[3][3].y, p.color[2],
		p.pole[3][0].x, p.pole[3][0].y, p.color[3]);
}

static inline void
copycolor(float *c, float *s)
{
	memcpy(c, s, FZ_MAXCOLORS * sizeof(float));
}

static inline void
midcolor(float *c, float *c1, float *c2)
{
	int i;
	for (i = 0; i < FZ_MAXCOLORS; i++)
		c[i] = (c1[i] + c2[i]) * 0.5f;
}

static inline void
splitcurve(fz_point *pole, fz_point *q0, fz_point *q1, int polestep)
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

static inline void
splitstripe(pdf_tensorpatch *p, pdf_tensorpatch *s0, pdf_tensorpatch *s1)
{
	/* split all horizontal bezier curves in patch,
	 * creating two new patches with half the width. */
	splitcurve(&p->pole[0][0], &s0->pole[0][0], &s1->pole[0][0], 4);
	splitcurve(&p->pole[0][1], &s0->pole[0][1], &s1->pole[0][1], 4);
	splitcurve(&p->pole[0][2], &s0->pole[0][2], &s1->pole[0][2], 4);
	splitcurve(&p->pole[0][3], &s0->pole[0][3], &s1->pole[0][3], 4);

	/* bilinear interpolation to find color values of corners of the two new patches. */
	copycolor(s0->color[0], p->color[0]);
	copycolor(s0->color[1], p->color[1]);
	midcolor(s0->color[2], p->color[1], p->color[2]);
	midcolor(s0->color[3], p->color[0], p->color[3]);

	copycolor(s1->color[0], s0->color[3]);
	copycolor(s1->color[1], s0->color[2]);
	copycolor(s1->color[2], p->color[2]);
	copycolor(s1->color[3], p->color[3]);
}

static void
drawstripe(pdf_tensorpatch *p, fz_shade *shade, int depth)
{
	pdf_tensorpatch s0, s1;

	/* split patch into two half-height patches */
	splitstripe(p, &s0, &s1);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		triangulatepatch(s0, shade);
		triangulatepatch(s1, shade);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		drawstripe(&s0, shade, depth);
		drawstripe(&s1, shade, depth);
	}
}

static inline void
splitpatch(pdf_tensorpatch *p, pdf_tensorpatch *s0, pdf_tensorpatch *s1)
{
	/* split all vertical bezier curves in patch,
	 * creating two new patches with half the height. */
	splitcurve(p->pole[0], s0->pole[0], s1->pole[0], 1);
	splitcurve(p->pole[1], s0->pole[1], s1->pole[1], 1);
	splitcurve(p->pole[2], s0->pole[2], s1->pole[2], 1);
	splitcurve(p->pole[3], s0->pole[3], s1->pole[3], 1);

	/* bilinear interpolation to find color values of corners of the two new patches. */
	copycolor(s0->color[0], p->color[0]);
	midcolor(s0->color[1], p->color[0], p->color[1]);
	midcolor(s0->color[2], p->color[2], p->color[3]);
	copycolor(s0->color[3], p->color[3]);

	copycolor(s1->color[0], s0->color[1]);
	copycolor(s1->color[1], p->color[1]);
	copycolor(s1->color[2], p->color[2]);
	copycolor(s1->color[3], s0->color[2]);
}

static void
drawpatch(fz_shade *shade, pdf_tensorpatch *p, int depth, int origdepth)
{
	pdf_tensorpatch s0, s1;

	/* split patch into two half-width patches */
	splitpatch(p, &s0, &s1);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		drawstripe(&s0, shade, origdepth);
		drawstripe(&s1, shade, origdepth);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		drawpatch(shade, &s0, depth, origdepth);
		drawpatch(shade, &s1, depth, origdepth);
	}
}

static inline fz_point
pdf_computetensorinterior(
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

static inline void
pdf_maketensorpatch(pdf_tensorpatch *p, int type, fz_point *pt)
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

		p->pole[1][1] = pdf_computetensorinterior(
			p->pole[0][0], p->pole[0][1], p->pole[1][0], p->pole[0][3],
			p->pole[3][0], p->pole[3][1], p->pole[1][3], p->pole[3][3]);

		p->pole[1][2] = pdf_computetensorinterior(
			p->pole[0][3], p->pole[0][2], p->pole[1][3], p->pole[0][0],
			p->pole[3][3], p->pole[3][2], p->pole[1][0], p->pole[3][0]);

		p->pole[2][1] = pdf_computetensorinterior(
			p->pole[3][0], p->pole[3][1], p->pole[2][0], p->pole[3][3],
			p->pole[0][0], p->pole[0][1], p->pole[2][3], p->pole[0][3]);

		p->pole[2][2] = pdf_computetensorinterior(
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

/*
 * Sample various functions into lookup tables
 */

static fz_error
pdf_samplecompositeshadefunction(fz_shade *shade,
	pdf_function *func, float t0, float t1)
{
	fz_error error;
	int i;

	for (i = 0; i < 256; i++)
	{
		float t = t0 + (i / 255.0f) * (t1 - t0);

		error = pdf_evalfunction(func, &t, 1, shade->function[i], shade->cs->n);
		if (error)
			return fz_rethrow(error, "unable to evaluate shading function at %g", t);
	}

	return fz_okay;
}

static fz_error
pdf_samplecomponentshadefunction(fz_shade *shade,
	int funcs, pdf_function **func, float t0, float t1)
{
	fz_error error;
	int i, k;

	for (i = 0; i < 256; i++)
	{
		float t = t0 + (i / 255.0f) * (t1 - t0);

		for (k = 0; k < funcs; k++)
		{
			error = pdf_evalfunction(func[k], &t, 1, &shade->function[i][k], 1);
			if (error)
				return fz_rethrow(error, "unable to evaluate shading function at %g", t);
		}
	}

	return fz_okay;
}

static fz_error
pdf_sampleshadefunction(fz_shade *shade, int funcs, pdf_function **func, float t0, float t1)
{
	fz_error error;

	shade->usefunction = 1;

	if (funcs == 1)
		error = pdf_samplecompositeshadefunction(shade, func[0], t0, t1);
	else
		error = pdf_samplecomponentshadefunction(shade, funcs, func, t0, t1);

	if (error)
		return fz_rethrow(error, "cannot sample shading function");

	return fz_okay;
}

/*
 * Type 1 -- Function-based shading
 */

static fz_error
pdf_loadfunctionbasedshading(fz_shade *shade, pdf_xref *xref,
	float *domain, fz_matrix matrix, pdf_function *func)
{
	fz_error error;
	int xx, yy;
	float x, y;
	float xn, yn;
	float x0, y0, x1, y1;

	pdf_logshade("load type1 (function-based) shading\n");

	x0 = domain[0];
	x1 = domain[1];
	y0 = domain[2];
	y1 = domain[3];

	for (yy = 0; yy < FUNSEGS; yy++)
	{
		y = y0 + (y1 - y0) * yy / FUNSEGS;
		yn = y0 + (y1 - y0) * (yy + 1) / FUNSEGS;

		for (xx = 0; xx < FUNSEGS; xx++)
		{
			float vcolor[4][FZ_MAXCOLORS];
			fz_point vcoord[4];
			int i;

			x = x0 + (x1 - x0) * xx / FUNSEGS;
			xn = x0 + (x1 - x0) * (xx + 1) / FUNSEGS;

			vcoord[0].x = x; vcoord[0].y = y;
			vcoord[1].x = xn; vcoord[1].y = y;
			vcoord[2].x = xn; vcoord[2].y = yn;
			vcoord[3].x = x; vcoord[3].y = yn;

			for (i = 0; i < 4; i++)
			{
				float point[2];

				point[0] = vcoord[i].x;
				point[1] = vcoord[i].y;

				error = pdf_evalfunction(func, point, 2, vcolor[i], shade->cs->n);
				if (error)
					return fz_rethrow(error, "unable to evaluate shading function");
			}

			for (i = 0; i < 4; i++)
				vcoord[i] = fz_transformpoint(matrix, vcoord[i]);

			pdf_addquad(shade,
				vcoord[0].x, vcoord[0].y, vcolor[0],
				vcoord[1].x, vcoord[1].y, vcolor[1],
				vcoord[2].x, vcoord[2].y, vcolor[2],
				vcoord[3].x, vcoord[3].y, vcolor[3]);
		}
	}

	return fz_okay;
}

/*
 * Type 2 -- Axial shading
 */

static fz_error
pdf_loadaxialshading(fz_shade *shade, pdf_xref *xref,
	float *coords, float *domain, int funcs, pdf_function **func, int *extend)
{
	float tmin[1] = { 0 };
	float tmax[1] = { 1 };
	fz_point p1, p2, p3, p4;
	fz_point ep1, ep2, ep3, ep4;
	float x0, y0, x1, y1;
	float theta;
	float dist;
	fz_error error;

	pdf_logshade("load type2 (axial) shading\n");

	x0 = coords[0];
	y0 = coords[1];
	x1 = coords[2];
	y1 = coords[3];

	error = pdf_sampleshadefunction(shade, funcs, func, domain[0], domain[1]);
	if (error)
		return fz_rethrow(error, "unable to sample shading function");

	theta = atan2f(y1 - y0, x1 - x0);
	theta += (float)M_PI * 0.5f;

	dist = hypotf(x1 - x0, y1 - y0);
	if (dist < FLT_EPSILON)
		return fz_throw("zero-sized axial shading");

	p1.x = x0 + HUGENUM * cosf(theta);
	p1.y = y0 + HUGENUM * sinf(theta);
	p2.x = x1 + HUGENUM * cosf(theta);
	p2.y = y1 + HUGENUM * sinf(theta);
	p3.x = x0 - HUGENUM * cosf(theta);
	p3.y = y0 - HUGENUM * sinf(theta);
	p4.x = x1 - HUGENUM * cosf(theta);
	p4.y = y1 - HUGENUM * sinf(theta);

	pdf_addquad(shade,
		p1.x, p1.y, tmin,
		p2.x, p2.y, tmax,
		p4.x, p4.y, tmax,
		p3.x, p3.y, tmin);

	if (extend[0])
	{
		ep1.x = p1.x - (x1 - x0) / dist * HUGENUM;
		ep1.y = p1.y - (y1 - y0) / dist * HUGENUM;
		ep3.x = p3.x - (x1 - x0) / dist * HUGENUM;
		ep3.y = p3.y - (y1 - y0) / dist * HUGENUM;
		pdf_addquad(shade,
			ep1.x, ep1.y, tmin,
			p1.x, p1.y, tmin,
			p3.x, p3.y, tmin,
			ep3.x, ep3.y, tmin);
	}

	if (extend[1])
	{
		ep2.x = p2.x + (x1 - x0) / dist * HUGENUM;
		ep2.y = p2.y + (y1 - y0) / dist * HUGENUM;
		ep4.x = p4.x + (x1 - x0) / dist * HUGENUM;
		ep4.y = p4.y + (y1 - y0) / dist * HUGENUM;
		pdf_addquad(shade,
			p2.x, p2.y, tmax,
			ep2.x, ep2.y, tmax,
			ep4.x, ep4.y, tmax,
			p4.x, p4.y, tmax);
	}

	return fz_okay;
}

/*
 * Type 3 -- Radial shading
 */

static void
pdf_buildannulusmesh(fz_shade *shade,
	float x0, float y0, float r0, float c0,
	float x1, float y1, float r1, float c1)
{
	float dist = hypotf(x1 - x0, y1 - y0);
	float step;
	float theta;
	int i;

	if (dist != 0)
		theta = asinf((r1 - r0) / dist) + (float)M_PI * 0.5f + atan2f(y1 - y0, x1 - x0);
	else
		theta = 0;

	if (!(theta >= 0 && theta <= (float)M_PI))
		theta = 0;

	step = (float)M_PI * 2 / RADSEGS;

	for (i = 0; i < RADSEGS; theta -= step, i++)
	{
		fz_point pt1, pt2, pt3, pt4;

		pt1.x = cosf(theta) * r1 + x1;
		pt1.y = sinf(theta) * r1 + y1;
		pt2.x = cosf(theta) * r0 + x0;
		pt2.y = sinf(theta) * r0 + y0;
		pt3.x = cosf(theta + step) * r1 + x1;
		pt3.y = sinf(theta + step) * r1 + y1;
		pt4.x = cosf(theta + step) * r0 + x0;
		pt4.y = sinf(theta + step) * r0 + y0;

		if (r0 > 0)
			pdf_addtriangle(shade, pt1.x, pt1.y, &c1, pt2.x, pt2.y, &c0, pt4.x, pt4.y, &c0);
		if (r1 > 0)
			pdf_addtriangle(shade, pt1.x, pt1.y, &c1, pt3.x, pt3.y, &c1, pt4.x, pt4.y, &c0);
	}
}

static fz_error
pdf_loadradialshading(fz_shade *shade, pdf_xref *xref,
	float *coords, float *domain, int funcs, pdf_function **func, int *extend)
{
	float x0, y0, r0, x1, y1, r1;
	float ex0, ey0, er0;
	float ex1, ey1, er1;
	float rs;
	fz_error error;

	pdf_logshade("load type3 (radial) shading\n");

	x0 = coords[0];
	y0 = coords[1];
	r0 = coords[2];
	x1 = coords[3];
	y1 = coords[4];
	r1 = coords[5];

	error = pdf_sampleshadefunction(shade, funcs, func, domain[0], domain[1]);
	if (error)
		return fz_rethrow(error, "unable to sample shading function");

	if (r0 < r1)
		rs = r0 / (r0 - r1);
	else
		rs = -HUGENUM;

	ex0 = x0 + (x1 - x0) * rs;
	ey0 = y0 + (y1 - y0) * rs;
	er0 = r0 + (r1 - r0) * rs;

	if (r0 > r1)
		rs = r1 / (r1 - r0);
	else
		rs = -HUGENUM;

	ex1 = x1 + (x0 - x1) * rs;
	ey1 = y1 + (y0 - y1) * rs;
	er1 = r1 + (r0 - r1) * rs;

	if (extend[0])
		pdf_buildannulusmesh(shade, ex0, ey0, er0, 0, x0, y0, r0, 0);
	pdf_buildannulusmesh(shade, x0, y0, r0, 0, x1, y1, r1, 1);
	if (extend[1])
		pdf_buildannulusmesh(shade, x1, y1, r1, 1, ex1, ey1, er1, 1);

	return fz_okay;
}

/*
 * Type 4 & 5 -- Triangle mesh shadings
 */

struct vertex
{
	int flag;
	float x, y;
	float c[FZ_MAXCOLORS];
};

static unsigned
readbits(fz_stream *stream, int bits)
{
	unsigned int v = 0;
	int c, n;

	for (n = 0; n < bits; n += 8)
	{
		c = fz_readbyte(stream);
		if (c == EOF)
			break;
		v = (v << 8) | c;
	}

	return v;
}

static float
readsample(fz_stream *stream, int bits, float min, float max)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = 1.0f / (powf(2, bits) - 1);
	return min + readbits(stream, bits) * (max - min) * bitscale;
}

static fz_error
pdf_loadtype4shade(fz_shade *shade, pdf_xref *xref,
	int bpcoord, int bpcomp, int bpflag, float *decode,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	float x0, x1, y0, y1;
	float c0[FZ_MAXCOLORS];
	float c1[FZ_MAXCOLORS];
	struct vertex va, vb, vc, vd;
	int ncomp;
	int i;

	pdf_logshade("load type4 (free-form triangle mesh) shading\n");

	x0 = decode[0];
	x1 = decode[1];
	y0 = decode[2];
	y1 = decode[3];
	for (i = 0; i < shade->cs->n; i++)
	{
		c0[i] = decode[4 + i * 2 + 0];
		c1[i] = decode[4 + i * 2 + 1];
	}

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, c0[0], c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}
	else
		ncomp = shade->cs->n;

	while (fz_peekbyte(stream) != EOF)
	{
		vd.flag = readbits(stream, bpflag);
		vd.x = readsample(stream, bpcoord, x0, x1);
		vd.y = readsample(stream, bpcoord, y0, y1);
		for (i = 0; i < ncomp; i++)
			vd.c[i] = readsample(stream, bpcomp, c0[i], c1[i]);

		switch (vd.flag)
		{
		case 0: /* start new triangle */
			va = vd;

			vb.flag = readbits(stream, bpflag);
			vb.x = readsample(stream, bpcoord, x0, x1);
			vb.y = readsample(stream, bpcoord, y0, y1);
			for (i = 0; i < ncomp; i++)
				vb.c[i] = readsample(stream, bpcomp, c0[i], c1[i]);

			vc.flag = readbits(stream, bpflag);
			vc.x = readsample(stream, bpcoord, x0, x1);
			vc.y = readsample(stream, bpcoord, y0, y1);
			for (i = 0; i < ncomp; i++)
				vc.c[i] = readsample(stream, bpcomp, c0[i], c1[i]);

			pdf_addtriangle(shade, va.x, va.y, va.c, vb.x, vb.y, vb.c, vc.x, vc.y, vc.c);
			break;

		case 1: /* Vb, Vc, Vd */
			va = vb;
			vb = vc;
			vc = vd;
			pdf_addtriangle(shade, va.x, va.y, va.c, vb.x, vb.y, vb.c, vc.x, vc.y, vc.c);
			break;

		case 2: /* Va, Vc, Vd */
			vb = vc;
			vc = vd;
			pdf_addtriangle(shade, va.x, va.y, va.c, vb.x, vb.y, vb.c, vc.x, vc.y, vc.c);
			break;
		}
	}

	return fz_okay;
}

static fz_error
pdf_loadtype5shade(fz_shade *shade, pdf_xref *xref,
	int bpcoord, int bpcomp, int vprow, float *decode,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	int ncomp;
	float x0, x1, y0, y1;
	float c0[FZ_MAXCOLORS];
	float c1[FZ_MAXCOLORS];
	fz_point *pbuf;
	float *cbuf;
	int rows, col;
	int i;

	pdf_logshade("load type5 (lattice-form triangle mesh) shading\n");

	x0 = decode[0];
	x1 = decode[1];
	y0 = decode[2];
	y1 = decode[3];
	for (i = 0; i < shade->cs->n; i++)
	{
		c0[i] = decode[4 + i * 2 + 0];
		c1[i] = decode[4 + i * 2 + 1];
	}

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, c0[0], c1[0]);
		if (error)
			return fz_rethrow(error, "cannot sample shading function");
	}
	else
		ncomp = shade->cs->n;

	pbuf = fz_malloc(2 * vprow * sizeof(fz_point));
	cbuf = fz_malloc(2 * vprow * FZ_MAXCOLORS * sizeof(float));

	rows = 0;

	do
	{
		fz_point *p = &pbuf[rows * vprow];
		float *c = &cbuf[rows * vprow * FZ_MAXCOLORS];

		while (rows < 2)
		{
			for (col = 0; col < vprow; col++)
			{
				p->x = readsample(stream, bpcoord, x0, x1);
				p->y = readsample(stream, bpcoord, y0, y1);
				for (i = 0; i < ncomp; i++)
					c[i] = readsample(stream, bpcomp, c0[i], c1[i]);
				p++;
				c += FZ_MAXCOLORS;
			}

			rows++;
		}


		for (i = 0; i < vprow - 1; i++)
		{
			int va = i;
			int vb = i + 1;
			int vc = i + 1 + vprow;
			int vd = i + vprow;

			pdf_addquad(shade,
				pbuf[va].x, pbuf[va].y, &cbuf[va * FZ_MAXCOLORS],
				pbuf[vb].x, pbuf[vb].y, &cbuf[vb * FZ_MAXCOLORS],
				pbuf[vc].x, pbuf[vc].y, &cbuf[vc * FZ_MAXCOLORS],
				pbuf[vd].x, pbuf[vd].y, &cbuf[vd * FZ_MAXCOLORS]);
		}

		memcpy(pbuf, &pbuf[vprow], vprow * sizeof(fz_point));
		memcpy(cbuf, &cbuf[vprow * FZ_MAXCOLORS], vprow * FZ_MAXCOLORS * sizeof(float));
		rows--;

	} while (fz_peekbyte(stream) != EOF);

	free(pbuf);
	free(cbuf);

	return fz_okay;
}

/*
 * Type 6 & 7 -- Patch mesh shadings
 */

static fz_error
pdf_loadtype6shade(fz_shade *shade, pdf_xref *xref,
	int bpcoord, int bpcomp, int bpflag, float *decode,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	int ncomp;
	fz_point p0, p1;
	float c0[FZ_MAXCOLORS];
	float c1[FZ_MAXCOLORS];
	int i, k;
	int haspatch, hasprevpatch;
	float prevc[4][FZ_MAXCOLORS];
	fz_point prevp[12];

	pdf_logshade("load type6 (coons patch mesh) shading\n");

	ncomp = shade->cs->n;

	p0.x = decode[0];
	p1.x = decode[1];
	p0.y = decode[2];
	p1.y = decode[3];
	for (i = 0; i < ncomp; i++)
	{
		c0[i] = decode[4 + i * 2 + 0];
		c1[i] = decode[4 + i * 2 + 1];
	}

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, c0[0], c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}

	shade->meshcap = 0;
	shade->mesh = nil;

	hasprevpatch = 0;

	while (fz_peekbyte(stream) != EOF)
	{
		float c[4][FZ_MAXCOLORS];
		fz_point p[12];
		int startcolor;
		int startpt;
		int flag;

		flag = readbits(stream, bpflag);

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
			p[i].x = readsample(stream, bpcoord, p0.x, p1.x);
			p[i].y = readsample(stream, bpcoord, p0.y, p1.y);
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
				c[i][k] = readsample(stream, bpcomp, c0[k], c1[k]);
		}

		haspatch = 0;

		if (flag == 0)
		{
			haspatch = 1;
		}
		else if (flag == 1 && hasprevpatch)
		{
			p[0] = prevp[3];
			p[1] = prevp[4];
			p[2] = prevp[5];
			p[3] = prevp[6];
			memcpy(c[0], prevc[1], FZ_MAXCOLORS * sizeof(float));
			memcpy(c[1], prevc[2], FZ_MAXCOLORS * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 2 && hasprevpatch)
		{
			p[0] = prevp[6];
			p[1] = prevp[7];
			p[2] = prevp[8];
			p[3] = prevp[9];
			memcpy(c[0], prevc[2], FZ_MAXCOLORS * sizeof(float));
			memcpy(c[1], prevc[3], FZ_MAXCOLORS * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 3 && hasprevpatch)
		{
			p[0] = prevp[ 9];
			p[1] = prevp[10];
			p[2] = prevp[11];
			p[3] = prevp[ 0];
			memcpy(c[0], prevc[3], FZ_MAXCOLORS * sizeof(float));
			memcpy(c[1], prevc[0], FZ_MAXCOLORS * sizeof(float));

			haspatch = 1;
		}

		if (haspatch)
		{
			pdf_tensorpatch patch;

			pdf_maketensorpatch(&patch, 6, p);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], FZ_MAXCOLORS * sizeof(float));

			drawpatch(shade, &patch, SUBDIV, SUBDIV);

			for (i = 0; i < 12; i++)
				prevp[i] = p[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], FZ_MAXCOLORS * sizeof(float));

			hasprevpatch = 1;
		}
	}

	return fz_okay;
}

static fz_error
pdf_loadtype7shade(fz_shade *shade, pdf_xref *xref,
	int bpcoord, int bpcomp, int bpflag, float *decode,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	int ncomp;
	fz_point p0, p1;
	float c0[FZ_MAXCOLORS];
	float c1[FZ_MAXCOLORS];
	int i, k;
	int haspatch, hasprevpatch;
	float prevc[4][FZ_MAXCOLORS];
	fz_point prevp[16];

	pdf_logshade("load type7 (tensor-product patch mesh) shading\n");

	ncomp = shade->cs->n;

	p0.x = decode[0];
	p1.x = decode[1];
	p0.y = decode[2];
	p1.y = decode[3];
	for (i = 0; i < ncomp; i++)
	{
		c0[i] = decode[4 + i * 2 + 0];
		c1[i] = decode[4 + i * 2 + 1];
	}

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, c0[0], c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}

	shade->meshcap = 0;
	shade->mesh = nil;

	hasprevpatch = 0;

	while (fz_peekbyte(stream) != EOF)
	{
		float c[4][FZ_MAXCOLORS];
		fz_point p[16];
		int startcolor;
		int startpt;
		int flag;

		flag = readbits(stream, bpflag);

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
			p[i].x = readsample(stream, bpcoord, p0.x, p1.x);
			p[i].y = readsample(stream, bpcoord, p0.y, p1.y);
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
				c[i][k] = readsample(stream, bpcomp, c0[k], c1[k]);
		}

		haspatch = 0;

		if (flag == 0)
		{
			haspatch = 1;
		}
		else if (flag == 1 && hasprevpatch)
		{
			p[0] = prevp[3];
			p[1] = prevp[4];
			p[2] = prevp[5];
			p[3] = prevp[6];
			memcpy(c[0], prevc[1], FZ_MAXCOLORS * sizeof(float));
			memcpy(c[1], prevc[2], FZ_MAXCOLORS * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 2 && hasprevpatch)
		{
			p[0] = prevp[6];
			p[1] = prevp[7];
			p[2] = prevp[8];
			p[3] = prevp[9];
			memcpy(c[0], prevc[2], FZ_MAXCOLORS * sizeof(float));
			memcpy(c[1], prevc[3], FZ_MAXCOLORS * sizeof(float));

			haspatch = 1;
		}
		else if (flag == 3 && hasprevpatch)
		{
			p[0] = prevp[ 9];
			p[1] = prevp[10];
			p[2] = prevp[11];
			p[3] = prevp[ 0];
			memcpy(c[0], prevc[3], FZ_MAXCOLORS * sizeof(float));
			memcpy(c[1], prevc[0], FZ_MAXCOLORS * sizeof(float));

			haspatch = 1;
		}

		if (haspatch)
		{
			pdf_tensorpatch patch;

			pdf_maketensorpatch(&patch, 7, p);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], FZ_MAXCOLORS * sizeof(float));

			drawpatch(shade, &patch, SUBDIV, SUBDIV);

			for (i = 0; i < 16; i++)
				prevp[i] = p[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], FZ_MAXCOLORS * sizeof(float));

			hasprevpatch = 1;
		}
	}

	return fz_okay;
}

/*
 * Load all of the common shading dictionary parameters, then switch on the shading type.
 */
static fz_error
pdf_loadshadingdict(fz_shade **shadep, pdf_xref *xref, fz_obj *dict, fz_matrix transform)
{
	fz_error error;
	fz_shade *shade;
	float decode[4 + 2 * FZ_MAXCOLORS] = {0}; // [x0 x1 y0 y1 c1[0] c1[1] ... cn[0] cn[1]]
	float coords[6] = {0}; // [x0 y0 x1 y1] or [x0 y0 r0 x1 y1 r1]
	float domain[6] = {0}; // [x0 x1 y0 y1] or [t0 t1]
	int extend[2] = {0}; // [e0 e1]
	pdf_function *func[FZ_MAXCOLORS] = { nil };
	fz_stream *stream = nil;
	fz_matrix matrix;
	int bpcoord = 0;
	int bpcomp = 0;
	int bpflag = 0;
	int vprow = 0;
	fz_obj *obj;
	int funcs;
	int type;
	int i;

	pdf_logshade("load shading dict (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

	shade = fz_malloc(sizeof(fz_shade));
	shade->refs = 1;
	shade->usebackground = 0;
	shade->usefunction = 0;
	shade->matrix = transform;
	shade->bbox = fz_infiniterect;

	shade->meshlen = 0;
	shade->meshcap = 0;
	shade->mesh = nil;

	shade->cs = nil;

	domain[0] = 0;
	domain[1] = 1;
	domain[2] = 0;
	domain[3] = 1;

	matrix = fz_identity();

	funcs = 0;

	obj = fz_dictgets(dict, "ShadingType");
	type = fz_toint(obj);

	obj = fz_dictgets(dict, "ColorSpace");
	if (!obj)
	{
		fz_dropshade(shade);
		return fz_throw("shading colorspace is missing");
	}
	error = pdf_loadcolorspace(&shade->cs, xref, obj);
	if (error)
	{
		fz_dropshade(shade);
		return fz_rethrow(error, "cannot load colorspace (%d %d R)", fz_tonum(obj), fz_togen(obj));
	}
	pdf_logshade("colorspace %s\n", shade->cs->name);

	obj = fz_dictgets(dict, "Background");
	if (obj)
	{
		pdf_logshade("background\n");
		shade->usebackground = 1;
		for (i = 0; i < shade->cs->n; i++)
			shade->background[i] = fz_toreal(fz_arrayget(obj, i));
	}

	obj = fz_dictgets(dict, "BBox");
	if (fz_isarray(obj))
	{
		shade->bbox = pdf_torect(obj);
	}

	obj = fz_dictgets(dict, "Function");
	if (fz_isdict(obj))
	{
		funcs = 1;

		error = pdf_loadfunction(&func[0], xref, obj);
		if (error)
		{
			error = fz_rethrow(error, "cannot load shading function (%d %d R)", fz_tonum(obj), fz_togen(obj));
			goto cleanup;
		}
	}
	else if (fz_isarray(obj))
	{
		funcs = fz_arraylen(obj);
		if (funcs != 1 && funcs != shade->cs->n)
		{
			error = fz_throw("incorrect number of shading functions");
			goto cleanup;
		}

		for (i = 0; i < funcs; i++)
		{
			error = pdf_loadfunction(&func[i], xref, fz_arrayget(obj, i));
			if (error)
			{
				error = fz_rethrow(error, "cannot load shading function (%d %d R)", fz_tonum(obj), fz_togen(obj));
				goto cleanup;
			}
		}
	}

	if (type >= 1 && type <= 3)
	{
		obj = fz_dictgets(dict, "Domain");
		if (fz_isarray(obj))
		{
			for (i = 0; i < MIN(nelem(domain), fz_arraylen(obj)); i++)
				domain[i] = fz_toreal(fz_arrayget(obj, i));
		}

		obj = fz_dictgets(dict, "Matrix");
		if (fz_isarray(obj))
			matrix = pdf_tomatrix(obj);

		obj = fz_dictgets(dict, "Coords");
		if (fz_isarray(obj))
		{
			for (i = 0; i < MIN(nelem(domain), fz_arraylen(obj)); i++)
				coords[i] = fz_toreal(fz_arrayget(obj, i));
		}

		obj = fz_dictgets(dict, "Extend");
		if (fz_isarray(obj))
		{
			extend[0] = fz_tobool(fz_arrayget(obj, 0));
			extend[1] = fz_tobool(fz_arrayget(obj, 1));
		}
	}

	if (type >= 4 && type <= 7)
	{
		if (type == 4 || type == 6 || type == 7)
		{
			bpflag = fz_toint(fz_dictgets(dict, "BitsPerFlag"));
			if (bpflag != 2 && bpflag != 4 && bpflag != 8)
				fz_warn("invalid number of bits per vertex flag in shading, continuing...");
		}

		if (type == 5)
		{
			vprow = fz_toint(fz_dictgets(dict, "VerticesPerRow"));
			if (vprow < 2)
			{
				vprow = 2;
				fz_warn("invalid number of vertices per row in shading, continuing...");
			}
		}

		bpcoord = fz_toint(fz_dictgets(dict, "BitsPerCoordinate"));
		if (bpcoord != 1 && bpcoord != 2 && bpcoord != 4 && bpcoord != 4 &&
			bpcoord != 8 && bpcoord != 12 && bpcoord != 16 &&
			bpcoord != 24 && bpcoord != 32)
			fz_warn("invalid number of bits per vertex coordinate in shading, continuing...");

		bpcomp = fz_toint(fz_dictgets(dict, "BitsPerComponent"));
		if (bpcomp != 1 && bpcomp != 2 && bpcomp != 4 && bpcomp != 4 &&
			bpcomp != 8 && bpcomp != 12 && bpcomp != 16)
			fz_warn("invalid number of bits per vertex color component in shading, continuing...");

		obj = fz_dictgets(dict, "Decode");
		if (fz_isarray(obj))
		{
			if (fz_arraylen(obj) != 4 + shade->cs->n * 2)
				fz_warn("shading decode array is the wrong length");
			for (i = 0; i < 4 + shade->cs->n * 2; i++)
				decode[i] = fz_toreal(fz_arrayget(obj, i));
		}
		else
			fz_warn("shading decode array is missing");

		error = pdf_openstream(&stream, xref, fz_tonum(dict), fz_togen(dict));
		if (error)
			return fz_rethrow(error, "cannot open shading stream (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	pdf_logshade("type %d\n", type);
	if (type <= 3)
	{
		// coords
		// domain
		pdf_logshade("extend = [%d %d]\n", extend[0], extend[1]);
	}
	if (type >= 4)
	{
		pdf_logshade("bpflag = %d\n", bpflag);
		pdf_logshade("bpcoord = %d\n", bpcoord);
		pdf_logshade("bpcomp = %d\n", bpcomp);
		pdf_logshade("vprow = %d\n", vprow);
		pdf_logshade("decode = [%g %g %g %g ...]\n", decode[0], decode[1], decode[2], decode[3]);
	}

	error = 0;
	switch (type)
	{
	case 1:
		error = pdf_loadfunctionbasedshading(shade, xref, domain, matrix, func[0]);
		break;
	case 2:
		error = pdf_loadaxialshading(shade, xref, coords, domain, funcs, func, extend);
		break;
	case 3:
		error = pdf_loadradialshading(shade, xref, coords, domain, funcs, func, extend);
		break;
	case 4:
		error = pdf_loadtype4shade(shade, xref, bpcoord, bpcomp, bpflag, decode, funcs, func, stream);
		break;
	case 5:
		error = pdf_loadtype5shade(shade, xref, bpcoord, bpcomp, vprow, decode, funcs, func, stream);
		break;
	case 6:
		error = pdf_loadtype6shade(shade, xref, bpcoord, bpcomp, bpflag, decode, funcs, func, stream);
		break;
	case 7:
		error = pdf_loadtype7shade(shade, xref, bpcoord, bpcomp, bpflag, decode, funcs, func, stream);
		break;
	default:
		error = fz_throw("unknown shading type: %d", type);
		break;
	}
	if (error)
		goto cleanup;

	if (stream)
		fz_dropstream(stream);
	for (i = 0; i < funcs; i++)
		if (func[i])
			pdf_dropfunction(func[i]);

	pdf_logshade("}\n");

	*shadep = shade;
	return fz_okay;

cleanup:
	if (stream)
		fz_dropstream(stream);
	for (i = 0; i < funcs; i++)
		if (func[i])
			pdf_dropfunction(func[i]);
	fz_dropshade(shade);

	return fz_rethrow(error, "cannot load shading type %d (%d %d R)", type, fz_tonum(dict), fz_togen(dict));
}

fz_error
pdf_loadshading(fz_shade **shadep, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_matrix mat;
	fz_obj *obj;

	if ((*shadep = pdf_finditem(xref->store, PDF_KSHADING, dict)))
	{
		fz_keepshade(*shadep);
		return fz_okay;
	}

	/*
	 * Type 2 pattern dictionary
	 */
	if (fz_dictgets(dict, "PatternType"))
	{
		pdf_logshade("load shading pattern (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

		obj = fz_dictgets(dict, "Matrix");
		if (obj)
		{
			mat = pdf_tomatrix(obj);
			pdf_logshade("matrix [%g %g %g %g %g %g]\n",
				mat.a, mat.b, mat.c, mat.d, mat.e, mat.f);
		}
		else
		{
			mat = fz_identity();
		}

		obj = fz_dictgets(dict, "ExtGState");
		if (obj)
		{
			if (fz_dictgets(obj, "CA") || fz_dictgets(obj, "ca"))
			{
				fz_warn("shading with alpha not supported");
			}
		}

		obj = fz_dictgets(dict, "Shading");
		if (!obj)
			return fz_throw("syntaxerror: missing shading dictionary");

		error = pdf_loadshadingdict(shadep, xref, obj, mat);
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary (%d %d R)", fz_tonum(obj), fz_togen(obj));

		pdf_logshade("}\n");
	}

	/*
	 * Naked shading dictionary
	 */
	else
	{
		error = pdf_loadshadingdict(shadep, xref, dict, fz_identity());
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	pdf_storeitem(xref->store, PDF_KSHADING, dict, *shadep);

	return fz_okay;
}

