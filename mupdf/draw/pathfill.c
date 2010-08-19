#include "fitz.h"

#define MAXDEPTH 8

static void
line(fz_gel *gel, fz_matrix *ctm, float x0, float y0, float x1, float y1)
{
	float tx0 = ctm->a * x0 + ctm->c * y0 + ctm->e;
	float ty0 = ctm->b * x0 + ctm->d * y0 + ctm->f;
	float tx1 = ctm->a * x1 + ctm->c * y1 + ctm->e;
	float ty1 = ctm->b * x1 + ctm->d * y1 + ctm->f;
	fz_insertgel(gel, tx0, ty0, tx1, ty1);
}

static void
bezier(fz_gel *gel, fz_matrix *ctm, float flatness,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc,
	float xd, float yd, int depth)
{
	float dmax;
	float xab, yab;
	float xbc, ybc;
	float xcd, ycd;
	float xabc, yabc;
	float xbcd, ybcd;
	float xabcd, yabcd;

	/* termination check */
	dmax = ABS(xa - xb);
	dmax = MAX(dmax, ABS(ya - yb));
	dmax = MAX(dmax, ABS(xd - xc));
	dmax = MAX(dmax, ABS(yd - yc));
	if (dmax < flatness || depth >= MAXDEPTH)
	{
		line(gel, ctm, xa, ya, xd, yd);
		return;
	}

	xab = xa + xb;
	yab = ya + yb;
	xbc = xb + xc;
	ybc = yb + yc;
	xcd = xc + xd;
	ycd = yc + yd;

	xabc = xab + xbc;
	yabc = yab + ybc;
	xbcd = xbc + xcd;
	ybcd = ybc + ycd;

	xabcd = xabc + xbcd;
	yabcd = yabc + ybcd;

	xab *= 0.5f; yab *= 0.5f;
	xbc *= 0.5f; ybc *= 0.5f;
	xcd *= 0.5f; ycd *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;
	xbcd *= 0.25f; ybcd *= 0.25f;

	xabcd *= 0.125f; yabcd *= 0.125f;

	bezier(gel, ctm, flatness, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd, depth + 1);
	bezier(gel, ctm, flatness, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd, depth + 1);
}

void
fz_fillpath(fz_gel *gel, fz_path *path, fz_matrix ctm, float flatness)
{
	float x1, y1, x2, y2, x3, y3;
	float cx = 0;
	float cy = 0;
	float bx = 0;
	float by = 0;
	int i = 0;

	while (i < path->len)
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			/* implicit closepath before moveto */
			if (i && (cx != bx || cy != by))
				line(gel, &ctm, cx, cy, bx, by);
			x1 = path->els[i++].v;
			y1 = path->els[i++].v;
			cx = bx = x1;
			cy = by = y1;
			break;

		case FZ_LINETO:
			x1 = path->els[i++].v;
			y1 = path->els[i++].v;
			line(gel, &ctm, cx, cy, x1, y1);
			cx = x1;
			cy = y1;
			break;

		case FZ_CURVETO:
			x1 = path->els[i++].v;
			y1 = path->els[i++].v;
			x2 = path->els[i++].v;
			y2 = path->els[i++].v;
			x3 = path->els[i++].v;
			y3 = path->els[i++].v;
			bezier(gel, &ctm, flatness, cx, cy, x1, y1, x2, y2, x3, y3, 0);
			cx = x3;
			cy = y3;
			break;

		case FZ_CLOSEPATH:
			line(gel, &ctm, cx, cy, bx, by);
			cx = bx;
			cy = by;
			break;
		}
	}

	if (i && (cx != bx || cy != by))
		line(gel, &ctm, cx, cy, bx, by);
}
