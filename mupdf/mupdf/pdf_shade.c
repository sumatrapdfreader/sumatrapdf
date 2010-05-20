#include "fitz.h"
#include "mupdf.h"

/* most of this mess is jeong's */

#define HUGENUM 32000
#define BIGNUM 1024

#define NSEGS 32
#define MAX_RAD_SEGS 36

#define SEGMENTATION_DEPTH 3

typedef struct pdf_tensorpatch_s pdf_tensorpatch;

struct pdf_tensorpatch_s
{
	fz_point pole[4][4];
	float color[4][FZ_MAXCOLORS];
};

static void
growshademesh(fz_shade *shade, int amount)
{
	if (shade->meshlen + amount < shade->meshcap)
		return;

	if (shade->meshcap == 0)
		shade->meshcap = 1024;

	while (shade->meshlen + amount > shade->meshcap)
		shade->meshcap = (shade->meshcap * 3) / 2;

	shade->mesh = fz_realloc(shade->mesh, sizeof(float) * shade->meshcap);
}

/* adds triangle (x0, y0) -> (x1, y1) -> (x2, y2) to mesh */
static void
pdf_addtriangle(fz_shade *shade,
	float x0, float y0, float *color0,
	float x1, float y1, float *color1,
	float x2, float y2, float *color2)
{
	int triangleentries;
	int vertexentries;
	int ncomp;
	int i;

	if (shade->usefunction)
		ncomp = 3;
	else
		ncomp = 2 + shade->cs->n;

	vertexentries = 2 + ncomp;
	triangleentries = 3 * vertexentries;

	growshademesh(shade, triangleentries);

	shade->mesh[shade->meshlen++] = x0;
	shade->mesh[shade->meshlen++] = y0;
	for (i = 2; i < ncomp; i++)
		shade->mesh[shade->meshlen++] = color0[i - 2];

	shade->mesh[shade->meshlen++] = x1;
	shade->mesh[shade->meshlen++] = y1;
	for (i = 2; i < ncomp; i++)
		shade->mesh[shade->meshlen++] = color1[i - 2];

	shade->mesh[shade->meshlen++] = x2;
	shade->mesh[shade->meshlen++] = y2;
	for (i = 2; i < ncomp; i++)
		shade->mesh[shade->meshlen++] = color2[i - 2];
}

/* adds quad triangles (x0, y0) -> (x1, y1) -> (x3, y3) and
   (x1, y1) -> (x3, y3) -> (x2, y2) to mesh */
static void
pdf_addquad(fz_shade *shade,
	float x0, float y0, float *color0,
	float x1, float y1, float *color1,
	float x2, float y2, float *color2,
	float x3, float y3, float *color3)
{
	pdf_addtriangle(shade,
			x0, y0, color0,
			x1, y1, color1,
			x3, y3, color3);

	pdf_addtriangle(shade,
			x1, y1, color1,
			x3, y3, color3,
			x2, y2, color2);
}

static void
triangulatepatch(pdf_tensorpatch p, fz_shade *shade, int ncomp)
{
	pdf_addquad(shade,
		p.pole[0][0].x, p.pole[0][0].y, p.color[0],
		p.pole[0][3].x, p.pole[0][3].y, p.color[1],
		p.pole[3][3].x, p.pole[3][3].y, p.color[2],
		p.pole[3][0].x, p.pole[3][0].y, p.color[3]);
}

static inline void
copyvert(float *dst, float *src, int n)
{
	memcpy(dst, src, n * sizeof(float));
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
		c[i] = (float) ((c1[i] + c2[i]) / 2.0);
}

static inline float
midpoint(float a, float b)
{
	return a / 2.0f + b / 2.0f;
}

static inline void
split_curve(fz_point *pole, fz_point *q0, fz_point *q1, int pole_step)
{
	/* split bezier curve given by control points pole[0]..pole[3]
	   using de casteljau algo at midpoint and build two new
	   bezier curves q0[0]..q0[3] and q1[0]..q1[3]. all indices
	   should be multiplies by pole_step == 1 for vertical bezier
	   curves in patch and == 4 for horizontal bezier curves due
	   to C's multi-dimensional matrix memory layout. */

	float x12 = midpoint(pole[1 * pole_step].x, pole[2 * pole_step].x);
	float y12 = midpoint(pole[1 * pole_step].y, pole[2 * pole_step].y);

	q0[1 * pole_step].x = midpoint(pole[0 * pole_step].x, pole[1 * pole_step].x);
	q0[1 * pole_step].y = midpoint(pole[0 * pole_step].y, pole[1 * pole_step].y);
	q1[2 * pole_step].x = midpoint(pole[2 * pole_step].x, pole[3 * pole_step].x);
	q1[2 * pole_step].y = midpoint(pole[2 * pole_step].y, pole[3 * pole_step].y);

	q0[2 * pole_step].x = midpoint(q0[1 * pole_step].x, x12);
	q0[2 * pole_step].y = midpoint(q0[1 * pole_step].y, y12);
	q1[1 * pole_step].x = midpoint(x12, q1[2 * pole_step].x);
	q1[1 * pole_step].y = midpoint(y12, q1[2 * pole_step].y);

	q0[3 * pole_step].x = midpoint(q0[2 * pole_step].x, q1[1 * pole_step].x);
	q0[3 * pole_step].y = midpoint(q0[2 * pole_step].y, q1[1 * pole_step].y);
	q1[0 * pole_step].x = midpoint(q0[2 * pole_step].x, q1[1 * pole_step].x);
	q1[0 * pole_step].y = midpoint(q0[2 * pole_step].y, q1[1 * pole_step].y);

	q0[0 * pole_step].x = pole[0 * pole_step].x;
	q0[0 * pole_step].y = pole[0 * pole_step].y;
	q1[3 * pole_step].x = pole[3 * pole_step].x;
	q1[3 * pole_step].y = pole[3 * pole_step].y;
}

static inline void
split_stripe(pdf_tensorpatch *p, pdf_tensorpatch *s0, pdf_tensorpatch *s1)
{
	/* split all vertical bezier curves in patch two, creating
	   two new patches with half the height. */
	split_curve(p->pole[0], s0->pole[0], s1->pole[0], 1);
	split_curve(p->pole[1], s0->pole[1], s1->pole[1], 1);
	split_curve(p->pole[2], s0->pole[2], s1->pole[2], 1);
	split_curve(p->pole[3], s0->pole[3], s1->pole[3], 1);

	/* linear interpolation to find color values of corners of
	   the two new patches. TODO: shouldn't this be bicubic
	   interpolation since the color values are not linearly
	   drawn over the patch? */
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
drawstripe(pdf_tensorpatch *p, fz_shade *shade, int ncomp, int depth)
{
	pdf_tensorpatch s0, s1;

	/* split patch into two half-height patches */
	split_stripe(p, &s0, &s1);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		triangulatepatch(s0, shade, ncomp);
		triangulatepatch(s1, shade, ncomp);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		drawstripe(&s0, shade, ncomp, depth);
		drawstripe(&s1, shade, ncomp, depth);
	}
}

static inline void
split_patch(pdf_tensorpatch *p, pdf_tensorpatch *s0, pdf_tensorpatch *s1)
{
	/* split all horizontal bezier curves in patch two, creating
	   two new patches with half the width. */
	split_curve(&p->pole[0][0], &s0->pole[0][0], &s1->pole[0][0], 4);
	split_curve(&p->pole[0][1], &s0->pole[0][1], &s1->pole[0][1], 4);
	split_curve(&p->pole[0][2], &s0->pole[0][2], &s1->pole[0][2], 4);
	split_curve(&p->pole[0][3], &s0->pole[0][3], &s1->pole[0][3], 4);

	/* linear interpolation to find color values of corners of
	   the two new patches. TODO: shouldn't this be bicubic
	   interpolation since the color values are not linearly
	   drawn over the patch? */
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
drawpatch(fz_shade *shade, pdf_tensorpatch *p, int ncomp, int depth, int origdepth)
{
	pdf_tensorpatch s0, s1;

	/* split patch into two half-width patches */
	split_patch(p, &s0, &s1);

	depth--;
	if (depth == 0)
	{
		/* if no more subdividing, draw two new patches... */
		drawstripe(&s0, shade, ncomp, origdepth);
		drawstripe(&s1, shade, ncomp, origdepth);
	}
	else
	{
		/* ...otherwise, continue subdividing. */
		drawpatch(shade, &s0, ncomp, depth, origdepth);
		drawpatch(shade, &s1, ncomp, depth, origdepth);
	}
}

static void
pdf_addpatch(fz_shade *shade, pdf_tensorpatch *patch, int ncomp)
{
	drawpatch(shade, patch, ncomp, SEGMENTATION_DEPTH, SEGMENTATION_DEPTH);
}

static inline fz_point
pdf_computetensorinterior(
	fz_point a, fz_point b, fz_point c, fz_point d,
	fz_point e, fz_point f, fz_point g, fz_point h)
{
	fz_point pt;

	/* see equations at page 330 in pdf 1.7 */

	pt.x  = -4 * a.x;
	pt.x +=  6 * (b.x + c.x);
	pt.x += -2 * (d.x + e.x);
	pt.x +=  3 * (f.x + g.x);
	pt.x += -1 * h.x;
	pt.x /= 9;

	pt.y  = -4 * a.y;
	pt.y +=  6 * (b.y + c.y);
	pt.y += -2 * (d.y + e.y);
	pt.y +=  3 * (f.y + g.y);
	pt.y += -1 * h.y;
	pt.y /= 9;

	return pt;
}

static void
pdf_tensorinteriorfromcoons(pdf_tensorpatch *p)
{
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

static inline void
pdf_maketensorpatch(pdf_tensorpatch *p, int type, fz_point *pt)
{
	if (type == 7)
	{
		/* see control point stream order at page 330 in pdf 1.7 */

		p->pole[0][0] = pt[ 0];
		p->pole[0][1] = pt[ 1];
		p->pole[0][2] = pt[ 2];
		p->pole[0][3] = pt[ 3];
		p->pole[1][3] = pt[ 4];
		p->pole[2][3] = pt[ 5];
		p->pole[3][3] = pt[ 6];
		p->pole[3][2] = pt[ 7];
		p->pole[3][1] = pt[ 8];
		p->pole[3][0] = pt[ 9];
		p->pole[2][0] = pt[10];
		p->pole[1][0] = pt[11];
		p->pole[1][1] = pt[12];
		p->pole[1][2] = pt[13];
		p->pole[2][2] = pt[14];
		p->pole[2][1] = pt[15];
	}
	else if (type == 6)
	{
		/* see control point stream order at page 325 in pdf 1.7 */

		p->pole[0][0] = pt[ 0];
		p->pole[0][1] = pt[ 1];
		p->pole[0][2] = pt[ 2];
		p->pole[0][3] = pt[ 3];
		p->pole[1][3] = pt[ 4];
		p->pole[2][3] = pt[ 5];
		p->pole[3][3] = pt[ 6];
		p->pole[3][2] = pt[ 7];
		p->pole[3][1] = pt[ 8];
		p->pole[3][0] = pt[ 9];
		p->pole[2][0] = pt[10];
		p->pole[1][0] = pt[11];

		pdf_tensorinteriorfromcoons(p);
	}
}

static fz_error
pdf_samplecompositeshadefunction(fz_shade *shade,
	pdf_function *func, float t0, float t1)
{
	fz_error error;
	int i;

	for (i = 0; i < 256; i++)
	{
		float t = t0 + (i / 256.0) * (t1 - t0);

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
		float t = t0 + (i / 256.0) * (t1 - t0);

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

static fz_error
pdf_loadtype1shade(fz_shade *shade, pdf_xref *xref,
	float *domain, fz_matrix matrix, pdf_function *func)
{
	fz_error error;
	int xx, yy;
	float x, y;
	float xn, yn;
	float x0, y0, x1, y1;

	pdf_logshade("load type1 shade {\n");

	x0 = domain[0];
	x1 = domain[1];
	y0 = domain[2];
	y1 = domain[3];

	pdf_logshade("domain %g %g %g %g\n", x0, x1, y0, y1);
	pdf_logshade("matrix [%g %g %g %g %g %g]\n",
			matrix.a, matrix.b, matrix.c,
			matrix.d, matrix.e, matrix.f);

	for (yy = 0; yy < NSEGS; yy++)
	{
		y = y0 + (y1 - y0) * yy / (float) NSEGS;
		yn = y0 + (y1 - y0) * (yy + 1) / (float) NSEGS;

		for (xx = 0; xx < NSEGS; xx++)
		{
			float vcolor[4][FZ_MAXCOLORS];
			fz_point vcoord[4];
			int i;

			x = x0 + (x1 - x0) * (xx / (float) NSEGS);
			xn = x0 + (x1 - x0) * (xx + 1) / (float) NSEGS;

			vcoord[0].x =  x; vcoord[0].y =  y;
			vcoord[1].x = xn; vcoord[1].y =  y;
			vcoord[2].x = xn; vcoord[2].y = yn;
			vcoord[3].x =  x; vcoord[3].y = yn;

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

	shade->meshlen = shade->meshlen / (2 + shade->cs->n) / 3;

	pdf_logshade("}\n");

	return fz_okay;
}

static fz_error
pdf_loadtype2shade(fz_shade *shade, pdf_xref *xref,
	float *coords, float *domain, int funcs, pdf_function **func, int *extend)
{
	fz_point p1, p2, p3, p4;
	fz_point ep1, ep2, ep3, ep4;
	float x0, y0, x1, y1;
	float t0, t1;
	int e0, e1;
	float theta;
	float dist;
	int n;
	fz_error error;
	float tmin, tmax;

	pdf_logshade("load type2 shade {\n");

	x0 = coords[0];
	y0 = coords[1];
	x1 = coords[2];
	y1 = coords[3];

	t0 = domain[0];
	t1 = domain[1];

	e0 = extend[0];
	e1 = extend[1];

	pdf_logshade("coords %g %g %g %g\n", x0, y0, x1, y1);
	pdf_logshade("domain %g %g\n", t0, t1);
	pdf_logshade("extend %d %d\n", e0, e1);

	error = pdf_sampleshadefunction(shade, funcs, func, t0, t1);
	if (error)
		return fz_rethrow(error, "unable to sample shading function");

	theta = atan2(y1 - y0, x1 - x0);
	theta += M_PI / 2.0;

	pdf_logshade("theta=%g\n", theta);

	dist = hypot(x1 - x0, y1 - y0);

	/* if the axis has virtually length 0 (a point),
	do not extend as there is nothing to extend beyond */
	if (dist < FLT_EPSILON)
	{
		e0 = 0;
		e1 = 0;
	}

	p1.x = x0 + HUGENUM * cos(theta);
	p1.y = y0 + HUGENUM * sin(theta);
	p2.x = x1 + HUGENUM * cos(theta);
	p2.y = y1 + HUGENUM * sin(theta);
	p3.x = x0 - HUGENUM * cos(theta);
	p3.y = y0 - HUGENUM * sin(theta);
	p4.x = x1 - HUGENUM * cos(theta);
	p4.y = y1 - HUGENUM * sin(theta);

	pdf_logshade("p1 %g %g\n", p1.x, p1.y);
	pdf_logshade("p2 %g %g\n", p2.x, p2.y);
	pdf_logshade("p3 %g %g\n", p3.x, p3.y);
	pdf_logshade("p4 %g %g\n", p4.x, p4.y);

	n = 0;
	tmin = 0.0f;
	tmax = 1.0f;

	/* if the axis has virtually length 0 (a point), use the same axis
	position t = 0 for all triangle vertices */
	if (dist < FLT_EPSILON)
	{
		pdf_addquad(shade,
			p1.x, p1.y, &tmin,
			p2.x, p2.y, &tmin,
			p4.x, p4.y, &tmin,
			p3.x, p3.y, &tmin);
	}
	else
	{
		pdf_addquad(shade,
			p1.x, p1.y, &tmin,
			p2.x, p2.y, &tmax,
			p4.x, p4.y, &tmax,
			p3.x, p3.y, &tmin);
	}

	if (e0)
	{
		ep1.x = p1.x - (x1 - x0) / dist * HUGENUM;
		ep1.y = p1.y - (y1 - y0) / dist * HUGENUM;
		ep3.x = p3.x - (x1 - x0) / dist * HUGENUM;
		ep3.y = p3.y - (y1 - y0) / dist * HUGENUM;

		pdf_addquad(shade,
			ep1.x, ep1.y, &tmin,
			 p1.x,  p1.y, &tmin,
			 p3.x,  p3.y, &tmin,
			ep3.x, ep3.y, &tmin);
	}

	if (e1)
	{
		ep2.x = p2.x + (x1 - x0) / dist * HUGENUM;
		ep2.y = p2.y + (y1 - y0) / dist * HUGENUM;
		ep4.x = p4.x + (x1 - x0) / dist * HUGENUM;
		ep4.y = p4.y + (y1 - y0) / dist * HUGENUM;

		pdf_addquad(shade,
			 p2.x,  p2.y, &tmax,
			ep2.x, ep2.y, &tmax,
			ep4.x, ep4.y, &tmax,
			 p4.x,  p4.y, &tmax);
	}

	shade->meshlen = shade->meshlen / 3 / 3;

	pdf_logshade("}\n");

	return fz_okay;
}

static int
buildannulusmesh(fz_shade *shade, int pos,
	float x0, float y0, float r0,
	float x1, float y1, float r1,
	float c0, float c1, int nomesh)
{
	float dist = hypot(x1 - x0, y1 - y0);
	float step;
	float theta;
	int i;

	if (dist != 0)
		theta = asin((r1 - r0) / dist) + M_PI / 2.0 + atan2(y1 - y0, x1 - x0);
	else
		theta = 0;

	if (!(theta >= 0 && theta <= M_PI))
		theta = 0;

	step = M_PI * 2.0 / (float) MAX_RAD_SEGS;

	for (i = 0; i < MAX_RAD_SEGS; theta -= step, i++)
	{
		fz_point pt1, pt2, pt3, pt4;

		pt1.x = cos (theta) * r1 + x1;
		pt1.y = sin (theta) * r1 + y1;
		pt2.x = cos (theta) * r0 + x0;
		pt2.y = sin (theta) * r0 + y0;
		pt3.x = cos (theta+step) * r1 + x1;
		pt3.y = sin (theta+step) * r1 + y1;
		pt4.x = cos (theta+step) * r0 + x0;
		pt4.y = sin (theta+step) * r0 + y0;

		if (r0 > 0)
		{
			if (!nomesh)
			{
				pdf_addtriangle(shade,
					pt1.x, pt1.y, &c1,
					pt2.x, pt2.y, &c0,
					pt4.x, pt4.y, &c0);
			}
			pos++;
		}

		if (r1 > 0)
		{
			if (!nomesh)
			{
				pdf_addtriangle(shade,
					pt1.x, pt1.y, &c1,
					pt3.x, pt3.y, &c1,
					pt4.x, pt4.y, &c0);
			}
			pos++;
		}
	}

	return pos;
}

static fz_error
pdf_loadtype3shade(fz_shade *shade, pdf_xref *xref,
	float *coords, float *domain, int funcs, pdf_function **func, int *extend)
{
	float x0, y0, r0, x1, y1, r1;
	float t0, t1;
	int e0, e1;
	float ex0, ey0, er0;
	float ex1, ey1, er1;
	float rs;
	int i;
	fz_error error;

	pdf_logshade("load type3 shade {\n");

	x0 = coords[0];
	y0 = coords[1];
	r0 = coords[2];
	x1 = coords[3];
	y1 = coords[4];
	r1 = coords[5];

	pdf_logshade("coords %g %g %g  %g %g %g\n", x0, y0, r0, x1, y1, r1);

	t0 = domain[0];
	t1 = domain[1];

	e0 = extend[0];
	e1 = extend[1];

	pdf_logshade("domain %g %g\n", t0, t1);
	pdf_logshade("extend %d %d\n", e0, e1);

	error = pdf_sampleshadefunction(shade, funcs, func, t0, t1);
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

	for (i = 0; i < 2; i++)
	{
		int pos = 0;
		if (e0)
			pos = buildannulusmesh(shade, pos,
				ex0, ey0, er0,
				x0, y0, r0, 0,
				0, 1 - i);
		pos = buildannulusmesh(shade, pos,
			x0, y0, r0,
			x1, y1, r1,
			0, 1.0f, 1 - i);
		if (e1)
			pos = buildannulusmesh(shade, pos,
				x1, y1, r1,
				ex1, ey1, er1,
				1.0f, 1.0f, 1 - i);
	}

	shade->meshlen = shade->meshlen / 3 / 3;

	pdf_logshade("}\n");

	return fz_okay;
}

static int
getdata(fz_stream *stream, int bps)
{
	unsigned int bitmask = (1 << bps) - 1;
	unsigned int buf = 0;
	int bits = 0;
	int s;

	while (bits < bps)
	{
		buf = (buf << 8) | (fz_readbyte(stream) & 0xff); // TODO: EOF? No? Oh, ok...
		bits += 8;
	}
	s = buf >> (bits - bps);
	if (bps < 32)
		s = s & bitmask;
	bits -= bps;

	return s;
}

static fz_error
pdf_loadtype4shade(fz_shade *shade, pdf_xref *xref,
	int bpcoord, int bpcomp, int bpflag, float *decode,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	int ncomp;
	float x0, x1, y0, y1;
	float c0[FZ_MAXCOLORS];
	float c1[FZ_MAXCOLORS];
	float cval[4][FZ_MAXCOLORS];
	int idx, intriangle, badtriangle;
	int i;

	int flag[4];
	float x[4], y[4];

	pdf_logshade("load type4 shade {\n");

	x0 = decode[0];
	x1 = decode[1];
	y0 = decode[2];
	y1 = decode[3];
	for (i = 0; i < shade->cs->n; i++)
	{
		c0[i] = decode[4 + i * 2 + 0];
		c1[i] = decode[4 + i * 2 + 1];
	}

	pdf_logshade("decode [%g %g %g %g", x0, x1, y0, y1);
	for (i = 0; i < shade->cs->n; i++)
		pdf_logshade(" %g %g", c0[i], c1[i]);
	pdf_logshade("]\n");

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, c0[0], c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}
	else
		ncomp = shade->cs->n;

	idx = 0;
	intriangle = 0;
	badtriangle = 0;

	while (fz_peekbyte(stream) != EOF)
	{
		unsigned int t;
		int a, b, c;

		flag[idx] = getdata(stream, bpflag);

		t = getdata(stream, bpcoord);
		x[idx] = x0 + (t * (x1 - x0) / (pow(2, 24) - 1));

		t = getdata(stream, bpcoord);
		y[idx] = y0 + (t * (y1 - y0) / (pow(2, 24) - 1));

		for (i = 0; i < ncomp; i++)
		{
			t = getdata(stream, bpcomp);
			cval[idx][i] = t / (double)(pow(2, 16) - 1);
		}

		if (!intriangle && flag[idx] == 0)
		{
			/* two more vertices necessary */
			intriangle = 1;
		}
		else if (intriangle && idx >= 2)
		{
			/* collected three vertices */
			a = 0; b = 1; c = 2;
			intriangle = 0;
		}
		else if (!intriangle && flag[idx] == 1)
		{
			/* re-use previous b c vertices */
			a = 1; b = 2; c = 3;
			intriangle = 0;

			if (idx < 3)
				badtriangle = 1;
		}
		else if (!intriangle && flag[idx] == 2)
		{
			/* re-use previous a c vertices */
			a = 0; b = 2; c = 3;
			intriangle = 0;

			if (idx < 3)
				badtriangle = 1;
		}

		if (intriangle || badtriangle)
		{
			idx++;
			badtriangle = 0;
		}
		else
		{
			pdf_addtriangle(shade,
					x[a], y[a], cval[a],
					x[b], y[b], cval[b],
					x[c], y[c], cval[c]);

			flag[0] = flag[a];
			flag[1] = flag[b];
			flag[2] = flag[c];

			x[0] = x[a];
			x[1] = x[b];
			x[2] = x[c];

			y[0] = y[a];
			y[1] = y[b];
			y[2] = y[c];

			memmove(cval[0], cval[a], sizeof cval[0]);
			memmove(cval[1], cval[b], sizeof cval[1]);
			memmove(cval[2], cval[c], sizeof cval[2]);

			idx = 3;
		}

	}

	shade->meshlen = shade->meshlen / (2 + ncomp) / 3;

	pdf_logshade("}\n");

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
	int i;
	unsigned int t;
	fz_point *pbuf;
	float *cbuf;
	int rows, col;

	pdf_logshade("load type5 shade {\n");

	x0 = decode[0];
	x1 = decode[1];
	y0 = decode[2];
	y1 = decode[3];
	for (i = 0; i < shade->cs->n; i++)
	{
		c0[i] = decode[4 + i * 2 + 0];
		c1[i] = decode[4 + i * 2 + 1];
	}

	pdf_logshade("decode [%g %g %g %g", x0, x1, y0, y1);
	for (i = 0; i < shade->cs->n; i++)
		pdf_logshade(" %g %g", c0[i], c1[i]);
	pdf_logshade("]\n");

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
				t = getdata(stream, bpcoord);
				p->x = x0 + (t * (x1 - x0) / (float) (pow(2, bpcoord) - 1));
				t = getdata(stream, bpcoord);
				p->y = y0 + (t * (y1 - y0) / (float) (pow(2, bpcoord) - 1));

				for (i = 0; i < ncomp; i++)
				{
					t = getdata(stream, bpcomp);
					c[i] = c0[i] + (t * (c1[i] - c0[i]) / (float) (pow(2, bpcomp) - 1));
				}

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

	shade->meshlen = shade->meshlen / (2 + ncomp) / 3 ;

	free(pbuf);
	free(cbuf);

	pdf_logshade("}\n");

	return fz_okay;
}

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

	pdf_logshade("load type6 shade {\n");

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

	pdf_logshade("decode [%g %g %g %g", p0.x, p1.x, p0.y, p1.y);
	for (i = 0; i < ncomp; i++)
		pdf_logshade(" %g %g", c0[i], c1[i]);
	pdf_logshade("]\n");

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
		unsigned int t;
		int startcolor;
		int startpt;
		int flag;

		flag = getdata(stream, bpflag);

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
			t = getdata(stream, bpcoord);
			p[i].x = (float) (p0.x + (t * (p1.x - p0.x) / (pow(2, bpcoord) - 1.)));
			t = getdata(stream, bpcoord);
			p[i].y = (float) (p0.y + (t * (p1.y - p0.y) / (pow(2, bpcoord) - 1.)));
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
			{
				t = getdata(stream, bpcomp);
				c[i][k] = c0[k] + (t * (c1[k] - c0[k]) / (pow(2, bpcomp) - 1.0f));
			}
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
			p[0] = prevp[10];
			p[1] = prevp[11];
			p[2] = prevp[12];
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

			pdf_addpatch(shade, &patch, ncomp);

			for (i = 0; i < 12; i++)
				prevp[i] = p[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], FZ_MAXCOLORS * sizeof(float));

			hasprevpatch = 1;
		}
	}

	shade->meshlen = shade->meshlen / (2 + ncomp) / 3;

	pdf_logshade("}\n");

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

	pdf_logshade("load type7 shade {\n");

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

	pdf_logshade("decode [%g %g %g %g", p0.x, p1.x, p0.y, p1.y);
	for (i = 0; i < ncomp; i++)
		pdf_logshade(" %g %g", c0[i], c1[i]);
	pdf_logshade("]\n");

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
		unsigned int t;
		int startcolor;
		int startpt;
		int flag;

		flag = getdata(stream, bpflag);

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
			t = getdata(stream, bpcoord);
			p[i].x = (float) (p0.x + (t * (p1.x - p0.x) / (pow(2, bpcoord) - 1.)));
			t = getdata(stream, bpcoord);
			p[i].y = (float) (p0.y + (t * (p1.y - p0.y) / (pow(2, bpcoord) - 1.)));
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
			{
				t = getdata(stream, bpcomp);
				c[i][k] = c0[k] + (t * (c1[k] - c0[k]) / (pow(2, bpcomp) - 1.0f));
			}
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

			pdf_addpatch(shade, &patch, ncomp);

			for (i = 0; i < 16; i++)
				prevp[i] = p[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], FZ_MAXCOLORS * sizeof(float));

			hasprevpatch = 1;
		}
	}

	shade->meshlen = shade->meshlen / (2 + ncomp) / 3;

	pdf_logshade("}\n");

	return fz_okay;
}

static fz_error
parsedecode(fz_obj *obj, int ncomp, float *decode)
{
	int i;

	decode[0] = fz_toreal(fz_arrayget(obj, 0));
	decode[1] = fz_toreal(fz_arrayget(obj, 1));
	decode[2] = fz_toreal(fz_arrayget(obj, 2));
	decode[3] = fz_toreal(fz_arrayget(obj, 3));

	for (i = 0; i < MIN(fz_arraylen(obj) / 2, ncomp); i++)
	{
		decode[4 + i * 2 + 0] = fz_toreal(fz_arrayget(obj, i * 2 + 4));
		decode[4 + i * 2 + 1] = fz_toreal(fz_arrayget(obj, i * 2 + 5));
	}

	return fz_okay;
}


static fz_error
pdf_loadshadedict(fz_shade **shadep, pdf_xref *xref, fz_obj *dict, fz_matrix transform)
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

	pdf_logshade("load shade dict (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

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

	obj = fz_dictgets(dict, "ShadingType");
	type = fz_toint(obj);
	pdf_logshade("type %d\n", type);

	obj = fz_dictgets(dict, "ColorSpace");
	error = pdf_loadcolorspace(&shade->cs, xref, obj);
	if (error)
	{
		fz_dropshade(shade);
		return fz_rethrow(error, "cannot load colorspace");
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
		pdf_logshade("bbox [%g %g %g %g]\n",
			shade->bbox.x0, shade->bbox.y0,
			shade->bbox.x1, shade->bbox.y1);
	}

	domain[0] = 0.0;
	domain[1] = 1.0;
	domain[2] = 0.0;
	domain[3] = 1.0;

	obj = fz_dictgets(dict, "Domain");
	if (fz_isarray(obj))
	{
		for (i = 0; i < MIN(nelem(domain), fz_arraylen(obj)); i++)
			domain[i] = fz_toreal(fz_arrayget(obj, i));
	}

	matrix = fz_identity();

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

	bpcoord = fz_toint(fz_dictgets(dict, "BitsPerCoordinate"));
	if (type >= 4 && type <= 7)
	{
		if (bpcoord != 1 && bpcoord != 2 && bpcoord != 4 && bpcoord != 4 &&
			bpcoord != 8 && bpcoord != 12 && bpcoord != 16 &&
			bpcoord != 24 && bpcoord != 32)
			fz_warn("invalid number of bits per vertex coordinate in shading, continuing...");
	}

	bpcomp = fz_toint(fz_dictgets(dict, "BitsPerComponent"));
	if (type >= 4 && type <= 7)
	{
		if (bpcomp != 1 && bpcomp != 2 && bpcomp != 4 && bpcomp != 4 &&
			bpcomp != 8 && bpcomp != 12 && bpcomp != 16)
			fz_warn("invalid number of bits per vertex color component in shading, continuing...");
	}

	bpflag = fz_toint(fz_dictgets(dict, "BitsPerFlag"));
	if (type == 4 || type == 6 || type == 7)
	{
		if (bpflag != 2 && bpflag != 4 && bpflag != 8)
			fz_warn("invalid number of bits per vertex flag in shading, continuing...");
	}

	vprow = fz_toint(fz_dictgets(dict, "VerticesPerRow"));
	if (type == 5)
	{
		if (vprow < 2)
		{
			vprow = 2;
			fz_warn("invalid number of vertices per row in shading, continuing...");
		}
	}

	obj = fz_dictgets(dict, "Decode");
	if (fz_isarray(obj))
	{
		error = parsedecode(obj, shade->cs->n, decode);
		if (error)
		{
			error = fz_rethrow(error, "cannot parse shading decode");
			goto cleanup;
		}
	}
	else if (type >= 4 && type <= 7)
		fz_warn("shading vertex color decoding invalid, continuing...");

	funcs = 0;

	obj = fz_dictgets(dict, "Function");
	if (fz_isdict(obj))
	{
		funcs = 1;

		error = pdf_loadfunction(&func[0], xref, obj);
		if (error)
		{
			error = fz_rethrow(error, "cannot load shading function");
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
				error = fz_rethrow(error, "cannot load shading function");
				goto cleanup;
			}
		}
	}

	if (type >= 4 && type <= 7)
	{
		error = pdf_openstream(&stream, xref, fz_tonum(dict), fz_togen(dict));
		if (error)
			return fz_rethrow(error, "cannot open shading stream");
	}

	switch (type)
	{
	case 1:
		error = pdf_loadtype1shade(shade, xref, domain, matrix, func[0]);
		if (error) goto cleanup;
		break;
	case 2:
		error = pdf_loadtype2shade(shade, xref, coords, domain, funcs, func, extend);
		if (error) goto cleanup;
		break;
	case 3:
		error = pdf_loadtype3shade(shade, xref, coords, domain, funcs, func, extend);
		if (error) goto cleanup;
		break;
	case 4:
		error = pdf_loadtype4shade(shade, xref, bpcoord, bpcomp, bpflag, decode, funcs, func, stream);
		if (error) goto cleanup;
		break;
	case 5:
		error = pdf_loadtype5shade(shade, xref, bpcoord, bpcomp, vprow, decode, funcs, func, stream);
		if (error) goto cleanup;
		break;
	case 6:
		error = pdf_loadtype6shade(shade, xref, bpcoord, bpcomp, bpflag, decode, funcs, func, stream);
		if (error) goto cleanup;
		break;
	case 7:
		error = pdf_loadtype7shade(shade, xref, bpcoord, bpcomp, bpflag, decode, funcs, func, stream);
		if (error) goto cleanup;
		break;
	default:
		fz_warn("syntaxerror: unknown shading type: %d", type);
		break;
	}

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

	return fz_rethrow(error, "cannot load shading");
}

fz_error
pdf_loadshade(fz_shade **shadep, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_matrix mat;
	fz_obj *obj;

	if ((*shadep = pdf_finditem(xref->store, PDF_KSHADE, dict)))
	{
		fz_keepshade(*shadep);
		return fz_okay;
	}

	/*
	 * Type 2 pattern dictionary
	 */
	if (fz_dictgets(dict, "PatternType"))
	{
		pdf_logshade("load shade pattern (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

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

		error = pdf_loadshadedict(shadep, xref, obj, mat);
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary");

		pdf_logshade("}\n");
	}

	/*
	 * Naked shading dictionary
	 */
	else
	{
		error = pdf_loadshadedict(shadep, xref, dict, fz_identity());
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary");
	}

	pdf_storeitem(xref->store, PDF_KSHADE, dict, *shadep);

	return fz_okay;
}

