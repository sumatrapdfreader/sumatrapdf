#include "fitz.h"
#include "mupdf.h"

#define HUGENUM 32000 /* how far to extend axial/radial shadings */
#define FUNSEGS 32 /* size of sampled mesh for function-based shadings */
#define RADSEGS 32 /* how many segments to generate for radial meshes */
#define SUBDIV 3 /* how many levels to subdivide patches */

struct vertex
{
	float x, y;
	float c[FZ_MAXCOLORS];
};

static void
pdf_growmesh(fz_shade *shade, int amount)
{
	if (shade->meshlen + amount < shade->meshcap)
		return;

	if (shade->meshcap == 0)
		shade->meshcap = 1024;

	while (shade->meshlen + amount > shade->meshcap)
		shade->meshcap = (shade->meshcap * 3) / 2;

	shade->mesh = fz_realloc(shade->mesh, shade->meshcap, sizeof(float));
}

static void
pdf_addvertex(fz_shade *shade, struct vertex *v)
{
	int ncomp = shade->usefunction ? 1 : shade->cs->n;
	int i;
	pdf_growmesh(shade, 2 + ncomp);
	shade->mesh[shade->meshlen++] = v->x;
	shade->mesh[shade->meshlen++] = v->y;
	for (i = 0; i < ncomp; i++)
		shade->mesh[shade->meshlen++] = v->c[i];
}

static void
pdf_addtriangle(fz_shade *shade,
	struct vertex *v0,
	struct vertex *v1,
	struct vertex *v2)
{
	pdf_addvertex(shade, v0);
	pdf_addvertex(shade, v1);
	pdf_addvertex(shade, v2);
}

static void
pdf_addquad(fz_shade *shade,
	struct vertex *v0,
	struct vertex *v1,
	struct vertex *v2,
	struct vertex *v3)
{
	pdf_addtriangle(shade, v0, v1, v3);
	pdf_addtriangle(shade, v1, v3, v2);
}

/* Subdivide and tesselate tensor-patches */

typedef struct pdf_tensorpatch_s pdf_tensorpatch;

struct pdf_tensorpatch_s
{
	fz_point pole[4][4];
	float color[4][FZ_MAXCOLORS];
};

static void
triangulatepatch(pdf_tensorpatch p, fz_shade *shade)
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

	pdf_addquad(shade, &v0, &v1, &v2, &v3);
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
	/*
	split all horizontal bezier curves in patch,
	creating two new patches with half the width.
	*/
	splitcurve(&p->pole[0][0], &s0->pole[0][0], &s1->pole[0][0], 4);
	splitcurve(&p->pole[0][1], &s0->pole[0][1], &s1->pole[0][1], 4);
	splitcurve(&p->pole[0][2], &s0->pole[0][2], &s1->pole[0][2], 4);
	splitcurve(&p->pole[0][3], &s0->pole[0][3], &s1->pole[0][3], 4);

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
	/*
	split all vertical bezier curves in patch,
	creating two new patches with half the height.
	*/
	splitcurve(p->pole[0], s0->pole[0], s1->pole[0], 1);
	splitcurve(p->pole[1], s0->pole[1], s1->pole[1], 1);
	splitcurve(p->pole[2], s0->pole[2], s1->pole[2], 1);
	splitcurve(p->pole[3], s0->pole[3], s1->pole[3], 1);

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

/* Sample various functions into lookup tables */

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

/* Type 1-3 -- Function-based, axial and radial shadings */

static fz_error
pdf_loadfunctionbasedshading(fz_shade *shade, pdf_xref *xref, fz_obj *dict, pdf_function *func)
{
	fz_error error;
	fz_obj *obj;
	float x0, y0, x1, y1;
	fz_matrix matrix;
	struct vertex v[4];
	int xx, yy;
	float x, y;
	float xn, yn;
	int i;

	pdf_logshade("load type1 (function-based) shading\n");

	x0 = y0 = 0;
	x1 = y1 = 1;
	obj = fz_dictgets(dict, "Domain");
	if (fz_arraylen(obj) == 4)
	{
		x0 = fz_toreal(fz_arrayget(obj, 0));
		x1 = fz_toreal(fz_arrayget(obj, 1));
		y0 = fz_toreal(fz_arrayget(obj, 2));
		y1 = fz_toreal(fz_arrayget(obj, 3));
	}

	matrix = fz_identity;
	obj = fz_dictgets(dict, "Matrix");
	if (fz_arraylen(obj) == 6)
		matrix = pdf_tomatrix(obj);

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
				error = pdf_evalfunction(func, fv, 2, v[i].c, shade->cs->n);
				if (error)
					return fz_rethrow(error, "unable to evaluate shading function");

				pt.x = v[i].x;
				pt.y = v[i].y;
				pt = fz_transformpoint(matrix, pt);
				v[i].x = pt.x;
				v[i].y = pt.y;
			}

			pdf_addquad(shade, &v[0], &v[1], &v[2], &v[3]);
		}
	}

	return fz_okay;
}

static fz_error
pdf_loadaxialshading(fz_shade *shade, pdf_xref *xref, fz_obj *dict, int funcs, pdf_function **func)
{
	fz_error error;
	fz_obj *obj;
	float d0, d1;
	int e0, e1;
	float x0, y0, x1, y1;
	struct vertex p1, p2;

	pdf_logshade("load type2 (axial) shading\n");

	obj = fz_dictgets(dict, "Coords");
	if (fz_arraylen(obj) != 4)
		return fz_throw("invalid coordinates in axial shading");
	x0 = fz_toreal(fz_arrayget(obj, 0));
	y0 = fz_toreal(fz_arrayget(obj, 1));
	x1 = fz_toreal(fz_arrayget(obj, 2));
	y1 = fz_toreal(fz_arrayget(obj, 3));

	d0 = 0;
	d1 = 1;
	obj = fz_dictgets(dict, "Domain");
	if (fz_arraylen(obj) == 2)
	{
		d0 = fz_toreal(fz_arrayget(obj, 0));
		d1 = fz_toreal(fz_arrayget(obj, 1));
	}

	e0 = e1 = 0;
	obj = fz_dictgets(dict, "Extend");
	if (fz_arraylen(obj) == 2)
	{
		e0 = fz_tobool(fz_arrayget(obj, 0));
		e1 = fz_tobool(fz_arrayget(obj, 1));
	}

	error = pdf_sampleshadefunction(shade, funcs, func, d0, d1);
	if (error)
		return fz_rethrow(error, "unable to sample shading function");

	shade->type = FZ_LINEAR;

	shade->extend[0] = e0;
	shade->extend[1] = e1;

	p1.x = x0;
	p1.y = y0;
	p1.c[0] = 0;
	pdf_addvertex(shade, &p1);

	p2.x = x1;
	p2.y = y1;
	p2.c[0] = 0;
	pdf_addvertex(shade, &p2);

	return fz_okay;
}

static fz_error
pdf_loadradialshading(fz_shade *shade, pdf_xref *xref, fz_obj *dict, int funcs, pdf_function **func)
{
	fz_error error;
	fz_obj *obj;
	float d0, d1;
	int e0, e1;
	float x0, y0, r0, x1, y1, r1;
	struct vertex p1, p2;

	pdf_logshade("load type3 (radial) shading\n");

	obj = fz_dictgets(dict, "Coords");
	if (fz_arraylen(obj) != 6)
		return fz_throw("invalid coordinates in radial shading");
	x0 = fz_toreal(fz_arrayget(obj, 0));
	y0 = fz_toreal(fz_arrayget(obj, 1));
	r0 = fz_toreal(fz_arrayget(obj, 2));
	x1 = fz_toreal(fz_arrayget(obj, 3));
	y1 = fz_toreal(fz_arrayget(obj, 4));
	r1 = fz_toreal(fz_arrayget(obj, 5));

	d0 = 0;
	d1 = 1;
	obj = fz_dictgets(dict, "Domain");
	if (fz_arraylen(obj) == 2)
	{
		d0 = fz_toreal(fz_arrayget(obj, 0));
		d1 = fz_toreal(fz_arrayget(obj, 1));
	}

	e0 = e1 = 0;
	obj = fz_dictgets(dict, "Extend");
	if (fz_arraylen(obj) == 2)
	{
		e0 = fz_tobool(fz_arrayget(obj, 0));
		e1 = fz_tobool(fz_arrayget(obj, 1));
	}

	error = pdf_sampleshadefunction(shade, funcs, func, d0, d1);
	if (error)
		return fz_rethrow(error, "unable to sample shading function");

	shade->type = FZ_RADIAL;

	shade->extend[0] = e0;
	shade->extend[1] = e1;

	p1.x = x0;
	p1.y = y0;
	p1.c[0] = r0;
	pdf_addvertex(shade, &p1);

	p2.x = x1;
	p2.y = y1;
	p2.c[0] = r1;
	pdf_addvertex(shade, &p2);
	return fz_okay;
}

/* Type 4-7 -- Triangle and patch mesh shadings */

static inline unsigned int
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

static inline float
readsample(fz_stream *stream, int bits, float min, float max)
{
	/* we use pow(2,x) because (1<<x) would overflow the math on 32-bit samples */
	float bitscale = 1 / (powf(2, bits) - 1);
	return min + readbits(stream, bits) * (max - min) * bitscale;
}

struct meshparams
{
	int vprow;
	int bpflag;
	int bpcoord;
	int bpcomp;
	float x0, x1;
	float y0, y1;
	float c0[FZ_MAXCOLORS];
	float c1[FZ_MAXCOLORS];
};

static void
pdf_loadmeshparams(pdf_xref *xref, fz_obj *dict, struct meshparams *p)
{
	fz_obj *obj;
	int i, n;

	p->x0 = p->y0 = 0;
	p->x1 = p->y1 = 1;
	for (i = 0; i < FZ_MAXCOLORS; i++)
	{
		p->c0[i] = 0;
		p->c1[i] = 1;
	}

	p->vprow = fz_toint(fz_dictgets(dict, "VerticesPerRow"));
	p->bpflag = fz_toint(fz_dictgets(dict, "BitsPerFlag"));
	p->bpcoord = fz_toint(fz_dictgets(dict, "BitsPerCoordinate"));
	p->bpcomp = fz_toint(fz_dictgets(dict, "BitsPerComponent"));

	obj = fz_dictgets(dict, "Decode");
	if (fz_arraylen(obj) >= 6)
	{
		n = (fz_arraylen(obj) - 4) / 2;
		p->x0 = fz_toreal(fz_arrayget(obj, 0));
		p->x1 = fz_toreal(fz_arrayget(obj, 1));
		p->y0 = fz_toreal(fz_arrayget(obj, 2));
		p->y1 = fz_toreal(fz_arrayget(obj, 3));
		for (i = 0; i < n; i++)
		{
			p->c0[i] = fz_toreal(fz_arrayget(obj, 4 + i * 2));
			p->c1[i] = fz_toreal(fz_arrayget(obj, 5 + i * 2));
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

static fz_error
pdf_loadtype4shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	struct meshparams p;
	struct vertex va, vb, vc, vd;
	int ncomp;
	int flag;
	int i;

	pdf_logshade("load type4 (free-form triangle mesh) shading\n");

	pdf_loadmeshparams(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, p.c0[0], p.c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}
	else
		ncomp = shade->cs->n;

	while (fz_peekbyte(stream) != EOF)
	{
		flag = readbits(stream, p.bpflag);
		vd.x = readsample(stream, p.bpcoord, p.x0, p.x1);
		vd.y = readsample(stream, p.bpcoord, p.y0, p.y1);
		for (i = 0; i < ncomp; i++)
			vd.c[i] = readsample(stream, p.bpcomp, p.c0[i], p.c1[i]);

		switch (flag)
		{
		case 0: /* start new triangle */
			va = vd;

			readbits(stream, p.bpflag);
			vb.x = readsample(stream, p.bpcoord, p.x0, p.x1);
			vb.y = readsample(stream, p.bpcoord, p.y0, p.y1);
			for (i = 0; i < ncomp; i++)
				vb.c[i] = readsample(stream, p.bpcomp, p.c0[i], p.c1[i]);

			readbits(stream, p.bpflag);
			vc.x = readsample(stream, p.bpcoord, p.x0, p.x1);
			vc.y = readsample(stream, p.bpcoord, p.y0, p.y1);
			for (i = 0; i < ncomp; i++)
				vc.c[i] = readsample(stream, p.bpcomp, p.c0[i], p.c1[i]);

			pdf_addtriangle(shade, &va, &vb, &vc);
			break;

		case 1: /* Vb, Vc, Vd */
			va = vb;
			vb = vc;
			vc = vd;
			pdf_addtriangle(shade, &va, &vb, &vc);
			break;

		case 2: /* Va, Vc, Vd */
			vb = vc;
			vc = vd;
			pdf_addtriangle(shade, &va, &vb, &vc);
			break;
		}
	}

	return fz_okay;
}

static fz_error
pdf_loadtype5shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	struct meshparams p;
	struct vertex *buf, *ref;
	fz_error error;
	int first;
	int ncomp;
	int i, k;

	pdf_logshade("load type5 (lattice-form triangle mesh) shading\n");

	pdf_loadmeshparams(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, p.c0[0], p.c1[0]);
		if (error)
			return fz_rethrow(error, "cannot sample shading function");
	}
	else
		ncomp = shade->cs->n;

	ref = fz_calloc(p.vprow, sizeof(struct vertex));
	buf = fz_calloc(p.vprow, sizeof(struct vertex));
	first = 1;

	while (fz_peekbyte(stream) != EOF)
	{
		for (i = 0; i < p.vprow; i++)
		{
			buf[i].x = readsample(stream, p.bpcoord, p.x0, p.x1);
			buf[i].y = readsample(stream, p.bpcoord, p.y0, p.y1);
			for (k = 0; k < ncomp; k++)
				buf[i].c[k] = readsample(stream, p.bpcomp, p.c0[k], p.c1[k]);
		}

		if (!first)
			for (i = 0; i < p.vprow - 1; i++)
				pdf_addquad(shade,
					&ref[i], &ref[i+1], &buf[i+1], &buf[i]);

		memcpy(ref, buf, p.vprow * sizeof(struct vertex));
		first = 0;
	}

	free(ref);
	free(buf);

	return fz_okay;
}

/* Type 6 & 7 -- Patch mesh shadings */

static fz_error
pdf_loadtype6shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	struct meshparams p;
	int haspatch, hasprevpatch;
	float prevc[4][FZ_MAXCOLORS];
	fz_point prevp[12];
	int ncomp;
	int i, k;

	pdf_logshade("load type6 (coons patch mesh) shading\n");

	pdf_loadmeshparams(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, p.c0[0], p.c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}
	else
		ncomp = shade->cs->n;

	hasprevpatch = 0;

	while (fz_peekbyte(stream) != EOF)
	{
		float c[4][FZ_MAXCOLORS];
		fz_point v[12];
		int startcolor;
		int startpt;
		int flag;

		flag = readbits(stream, p.bpflag);

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
			v[i].x = readsample(stream, p.bpcoord, p.x0, p.x1);
			v[i].y = readsample(stream, p.bpcoord, p.y0, p.y1);
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
				c[i][k] = readsample(stream, p.bpcomp, p.c0[k], p.c1[k]);
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
			pdf_tensorpatch patch;

			pdf_maketensorpatch(&patch, 6, v);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], ncomp * sizeof(float));

			drawpatch(shade, &patch, SUBDIV, SUBDIV);

			for (i = 0; i < 12; i++)
				prevp[i] = v[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], ncomp * sizeof(float));

			hasprevpatch = 1;
		}
	}

	return fz_okay;
}

static fz_error
pdf_loadtype7shade(fz_shade *shade, pdf_xref *xref, fz_obj *dict,
	int funcs, pdf_function **func, fz_stream *stream)
{
	fz_error error;
	struct meshparams p;
	int haspatch, hasprevpatch;
	float prevc[4][FZ_MAXCOLORS];
	fz_point prevp[16];
	int ncomp;
	int i, k;

	pdf_logshade("load type7 (tensor-product patch mesh) shading\n");

	pdf_loadmeshparams(xref, dict, &p);

	if (funcs > 0)
	{
		ncomp = 1;
		error = pdf_sampleshadefunction(shade, funcs, func, p.c0[0], p.c1[0]);
		if (error)
			return fz_rethrow(error, "cannot load shading function");
	}
	else
		ncomp = shade->cs->n;

	hasprevpatch = 0;

	while (fz_peekbyte(stream) != EOF)
	{
		float c[4][FZ_MAXCOLORS];
		fz_point v[16];
		int startcolor;
		int startpt;
		int flag;

		flag = readbits(stream, p.bpflag);

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
			v[i].x = readsample(stream, p.bpcoord, p.x0, p.x1);
			v[i].y = readsample(stream, p.bpcoord, p.y0, p.y1);
		}

		for (i = startcolor; i < 4; i++)
		{
			for (k = 0; k < ncomp; k++)
				c[i][k] = readsample(stream, p.bpcomp, p.c0[k], p.c1[k]);
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
			pdf_tensorpatch patch;

			pdf_maketensorpatch(&patch, 7, v);

			for (i = 0; i < 4; i++)
				memcpy(patch.color[i], c[i], ncomp * sizeof(float));

			drawpatch(shade, &patch, SUBDIV, SUBDIV);

			for (i = 0; i < 16; i++)
				prevp[i] = v[i];

			for (i = 0; i < 4; i++)
				memcpy(prevc[i], c[i], FZ_MAXCOLORS * sizeof(float));

			hasprevpatch = 1;
		}
	}

	return fz_okay;
}

/* Load all of the shading dictionary parameters, then switch on the shading type. */

static fz_error
pdf_loadshadingdict(fz_shade **shadep, pdf_xref *xref, fz_obj *dict, fz_matrix transform)
{
	fz_error error;
	fz_shade *shade;
	pdf_function *func[FZ_MAXCOLORS] = { nil };
	fz_stream *stream = nil;
	fz_obj *obj;
	int funcs;
	int type;
	int i;

	pdf_logshade("load shading dict (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

	shade = fz_malloc(sizeof(fz_shade));
	shade->refs = 1;
	shade->type = FZ_MESH;
	shade->usebackground = 0;
	shade->usefunction = 0;
	shade->matrix = transform;
	shade->bbox = fz_infiniterect;
	shade->extend[0] = 0;
	shade->extend[1] = 0;

	shade->meshlen = 0;
	shade->meshcap = 0;
	shade->mesh = nil;

	shade->cs = nil;

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

	if (type >= 4 && type <= 7)
	{
		error = pdf_openstream(&stream, xref, fz_tonum(dict), fz_togen(dict));
		if (error)
			return fz_rethrow(error, "cannot open shading stream (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	switch (type)
	{
	case 1:
		error = pdf_loadfunctionbasedshading(shade, xref, dict, func[0]);
		break;
	case 2:
		error = pdf_loadaxialshading(shade, xref, dict, funcs, func);
		break;
	case 3:
		error = pdf_loadradialshading(shade, xref, dict, funcs, func);
		break;
	case 4:
		error = pdf_loadtype4shade(shade, xref, dict, funcs, func, stream);
		break;
	case 5:
		error = pdf_loadtype5shade(shade, xref, dict, funcs, func, stream);
		break;
	case 6:
		error = pdf_loadtype6shade(shade, xref, dict, funcs, func, stream);
		break;
	case 7:
		error = pdf_loadtype7shade(shade, xref, dict, funcs, func, stream);
		break;
	default:
		error = fz_throw("unknown shading type: %d", type);
		break;
	}
	if (error)
		goto cleanup;

	if (stream)
		fz_close(stream);
	for (i = 0; i < funcs; i++)
		if (func[i])
			pdf_dropfunction(func[i]);

	pdf_logshade("}\n");

	*shadep = shade;
	return fz_okay;

cleanup:
	if (stream)
		fz_close(stream);
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

	if ((*shadep = pdf_finditem(xref->store, fz_dropshade, dict)))
	{
		fz_keepshade(*shadep);
		return fz_okay;
	}

	/* Type 2 pattern dictionary */
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
			mat = fz_identity;
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

	/* Naked shading dictionary */
	else
	{
		error = pdf_loadshadingdict(shadep, xref, dict, fz_identity);
		if (error)
			return fz_rethrow(error, "cannot load shading dictionary (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	pdf_storeitem(xref->store, fz_keepshade, fz_dropshade, dict, *shadep);

	return fz_okay;
}
