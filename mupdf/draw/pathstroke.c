#include "fitz.h"

enum { BUTT = 0, ROUND = 1, SQUARE = 2, MITER = 0, BEVEL = 2 };

struct sctx
{
	fz_gel *gel;
	fz_matrix *ctm;
	float flatness;

	int linecap;
	int linejoin;
	float linewidth;
	float miterlimit;
	fz_point beg[2];
	fz_point seg[2];
	int sn, bn;
	int dot;

	float *dashlist;
	float dashphase;
	int dashlen;
	int toggle;
	int offset;
	float phase;
	fz_point cur;
};

static void
line(struct sctx *s, float x0, float y0, float x1, float y1)
{
	float tx0 = s->ctm->a * x0 + s->ctm->c * y0 + s->ctm->e;
	float ty0 = s->ctm->b * x0 + s->ctm->d * y0 + s->ctm->f;
	float tx1 = s->ctm->a * x1 + s->ctm->c * y1 + s->ctm->e;
	float ty1 = s->ctm->b * x1 + s->ctm->d * y1 + s->ctm->f;
	fz_insertgel(s->gel, tx0, ty0, tx1, ty1);
}

static void
arc(struct sctx *s,
	float xc, float yc,
	float x0, float y0,
	float x1, float y1)
{
	float th0, th1, r;
	float theta;
	float ox, oy, nx, ny;
	int n, i;

	r = fabs(s->linewidth);
	theta = 2 * M_SQRT2 * sqrt(s->flatness / r);
	th0 = atan2(y0, x0);
	th1 = atan2(y1, x1);

	if (r > 0)
	{
		if (th0 < th1)
			th0 += M_PI * 2;
		n = ceil((th0 - th1) / theta);
	}
	else
	{
		if (th1 < th0)
			th1 += M_PI * 2;
		n = ceil((th1 - th0) / theta);
	}

	ox = x0;
	oy = y0;
	for (i = 1; i < n; i++)
	{
		theta = th0 + (th1 - th0) * i / n;
		nx = cos(theta) * r;
		ny = sin(theta) * r;
		line(s, xc + ox, yc + oy, xc + nx, yc + ny);
		ox = nx;
		oy = ny;
	}

	line(s, xc + ox, yc + oy, xc + x1, yc + y1);
}

static void
linestroke(struct sctx *s, fz_point a, fz_point b)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float scale = s->linewidth / sqrt(dx * dx + dy * dy);
	float dlx = dy * scale;
	float dly = -dx * scale;
	line(s, a.x - dlx, a.y - dly, b.x - dlx, b.y - dly);
	line(s, b.x + dlx, b.y + dly, a.x + dlx, a.y + dly);
}

static void
linejoin(struct sctx *s, fz_point a, fz_point b, fz_point c)
{
	float miterlimit = s->miterlimit;
	float linewidth = s->linewidth;
	int linejoin = s->linejoin;
	float dx0, dy0;
	float dx1, dy1;
	float dlx0, dly0;
	float dlx1, dly1;
	float dmx, dmy;
	float dmr2;
	float scale;
	float cross;

	dx0 = b.x - a.x;
	dy0 = b.y - a.y;

	dx1 = c.x - b.x;
	dy1 = c.y - b.y;

	if (dx0 * dx0 + dy0 * dy0 < FLT_EPSILON)
		linejoin = BEVEL;
	if (dx1 * dx1 + dy1 * dy1 < FLT_EPSILON)
		linejoin = BEVEL;

	scale = linewidth / sqrt(dx0 * dx0 + dy0 * dy0);
	dlx0 = dy0 * scale;
	dly0 = -dx0 * scale;

	scale = linewidth / sqrt(dx1 * dx1 + dy1 * dy1);
	dlx1 = dy1 * scale;
	dly1 = -dx1 * scale;

	cross = dx1 * dy0 - dx0 * dy1;

	dmx = (dlx0 + dlx1) * 0.5;
	dmy = (dly0 + dly1) * 0.5;
	dmr2 = dmx * dmx + dmy * dmy;

	if (cross * cross < FLT_EPSILON && dx0 * dx1 + dy0 * dy1 >= 0)
		linejoin = BEVEL;

	if (linejoin == MITER)
		if (dmr2 * miterlimit * miterlimit < linewidth * linewidth)
			linejoin = BEVEL;

	if (linejoin == BEVEL)
	{
		line(s, b.x - dlx0, b.y - dly0, b.x - dlx1, b.y - dly1);
		line(s, b.x + dlx1, b.y + dly1, b.x + dlx0, b.y + dly0);
	}

	if (linejoin == MITER)
	{
		scale = linewidth * linewidth / dmr2;
		dmx *= scale;
		dmy *= scale;

		if (cross < 0)
		{
			line(s, b.x - dlx0, b.y - dly0, b.x - dlx1, b.y - dly1);
			line(s, b.x + dlx1, b.y + dly1, b.x + dmx, b.y + dmy);
			line(s, b.x + dmx, b.y + dmy, b.x + dlx0, b.y + dly0);
		}
		else
		{
			line(s, b.x + dlx1, b.y + dly1, b.x + dlx0, b.y + dly0);
			line(s, b.x - dlx0, b.y - dly0, b.x - dmx, b.y - dmy);
			line(s, b.x - dmx, b.y - dmy, b.x - dlx1, b.y - dly1);
		}
	}

	if (linejoin == ROUND)
	{
		if (cross < 0)
		{
			line(s, b.x - dlx0, b.y - dly0, b.x - dlx1, b.y - dly1);
			arc(s, b.x, b.y, dlx1, dly1, dlx0, dly0);
		}
		else
		{
			line(s, b.x + dlx1, b.y + dly1, b.x + dlx0, b.y + dly0);
			arc(s, b.x, b.y, -dlx0, -dly0, -dlx1, -dly1);
		}
	}
}

static void
linecap(struct sctx *s, fz_point a, fz_point b)
{
	float flatness = s->flatness;
	float linewidth = s->linewidth;
	int linecap = s->linecap;

	float dx = b.x - a.x;
	float dy = b.y - a.y;

	float scale = linewidth / sqrt(dx * dx + dy * dy);
	float dlx = dy * scale;
	float dly = -dx * scale;

	if (linecap == BUTT)
		line(s, b.x - dlx, b.y - dly, b.x + dlx, b.y + dly);

	if (linecap == ROUND)
	{
		int i;
		int n = ceil(M_PI / (2.0 * M_SQRT2 * sqrt(flatness / linewidth)));
		float ox = b.x - dlx;
		float oy = b.y - dly;
		for (i = 1; i < n; i++)
		{
			float theta = M_PI * i / n;
			float cth = cos(theta);
			float sth = sin(theta);
			float nx = b.x - dlx * cth - dly * sth;
			float ny = b.y - dly * cth + dlx * sth;
			line(s, ox, oy, nx, ny);
			ox = nx;
			oy = ny;
		}
		line(s, ox, oy, b.x + dlx, b.y + dly);
	}

	if (linecap == SQUARE)
	{
		line(s, b.x - dlx, b.y - dly,
			b.x - dlx - dly,
			b.y - dly + dlx);
		line(s, b.x - dlx - dly,
			b.y - dly + dlx,
			b.x + dlx - dly,
			b.y + dly + dlx);
		line(s, b.x + dlx - dly,
			b.y + dly + dlx,
			b.x + dlx, b.y + dly);
	}
}

static void
linedot(struct sctx *s, fz_point a)
{
	float flatness = s->flatness;
	float linewidth = s->linewidth;
	int n = ceil(M_PI / (M_SQRT2 * sqrt(flatness / linewidth)));
	float ox = a.x - linewidth;
	float oy = a.y;
	int i;

	for (i = 1; i < n; i++)
	{
		float theta = M_PI * 2 * i / n;
		float cth = cos(theta);
		float sth = sin(theta);
		float nx = a.x - cth * linewidth;
		float ny = a.y + sth * linewidth;
		line(s, ox, oy, nx, ny);
		ox = nx;
		oy = ny;
	}

	line(s, ox, oy, a.x - linewidth, a.y);
}

static void
strokeflush(struct sctx *s)
{
	if (s->sn == 2)
	{
		linecap(s, s->beg[1], s->beg[0]);
		linecap(s, s->seg[0], s->seg[1]);
	}
	else if (s->dot)
	{
		linedot(s, s->beg[0]);
	}

	s->dot = 0;
}

static void
strokemoveto(struct sctx *s, fz_point cur)
{
	strokeflush(s);
	s->seg[0] = cur;
	s->beg[0] = cur;
	s->sn = 1;
	s->bn = 1;
}

static void
strokelineto(struct sctx *s, fz_point cur)
{
	float dx = cur.x - s->seg[s->sn-1].x;
	float dy = cur.y - s->seg[s->sn-1].y;

	if (dx * dx + dy * dy < FLT_EPSILON)
	{
		s->dot = 1;
		return;
	}

	linestroke(s, s->seg[s->sn-1], cur);

	if (s->sn == 2)
	{
		linejoin(s, s->seg[0], s->seg[1], cur);
		s->seg[0] = s->seg[1];
		s->seg[1] = cur;
	}

	if (s->sn == 1)
		s->seg[s->sn++] = cur;
	if (s->bn == 1)
		s->beg[s->bn++] = cur;
}

static void
strokeclosepath(struct sctx *s)
{
	if (s->sn == 2)
	{
		strokelineto(s, s->beg[0]);
		if (s->seg[1].x == s->beg[0].x && s->seg[1].y == s->beg[0].y)
			linejoin(s, s->seg[0], s->beg[0], s->beg[1]);
		else
			linejoin(s, s->seg[1], s->beg[0], s->beg[1]);
	}
	else if (s->dot)
	{
		linedot(s, s->beg[0]);
	}

	s->bn = 0;
	s->sn = 0;
	s->dot = 0;
}

static void
strokebezier(struct sctx *s,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc,
	float xd, float yd)
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
	if (dmax < s->flatness) {
		fz_point p;
		p.x = xd;
		p.y = yd;
		strokelineto(s, p);
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

	strokebezier(s, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd);
	strokebezier(s, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd);
}

void
fz_strokepath(fz_gel *gel, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, float flatness, float linewidth)
{
	struct sctx s;
	fz_point p0, p1, p2, p3;
	int i;

	s.gel = gel;
	s.ctm = &ctm;
	s.flatness = flatness;

	s.linecap = stroke->linecap;
	s.linejoin = stroke->linejoin;
	s.linewidth = linewidth * 0.5; /* hairlines use a different value from the path value */
	s.miterlimit = stroke->miterlimit;
	s.sn = 0;
	s.bn = 0;
	s.dot = 0;

	i = 0;

	if (path->len > 0 && path->els[0].k != FZ_MOVETO)
		fz_warn("assert: path must begin with moveto");

	p0.x = p0.y = 0;

	while (i < path->len)
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			p1.x = path->els[i++].v;
			p1.y = path->els[i++].v;
			strokemoveto(&s, p1);
			p0 = p1;
			break;

		case FZ_LINETO:
			p1.x = path->els[i++].v;
			p1.y = path->els[i++].v;
			strokelineto(&s, p1);
			p0 = p1;
			break;

		case FZ_CURVETO:
			p1.x = path->els[i++].v;
			p1.y = path->els[i++].v;
			p2.x = path->els[i++].v;
			p2.y = path->els[i++].v;
			p3.x = path->els[i++].v;
			p3.y = path->els[i++].v;
			strokebezier(&s, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
			p0 = p3;
			break;

		case FZ_CLOSEPATH:
			strokeclosepath(&s);
			break;
		}
	}

	strokeflush(&s);
}

static void
dashmoveto(struct sctx *s, fz_point a)
{
	s->toggle = 1;
	s->offset = 0;
	s->phase = s->dashphase;

	while (s->phase >= s->dashlist[s->offset])
	{
		s->toggle = !s->toggle;
		s->phase -= s->dashlist[s->offset];
		s->offset ++;
		if (s->offset == s->dashlen)
			s->offset = 0;
	}

	s->cur = a;

	if (s->toggle)
		strokemoveto(s, a);
}

static void
dashlineto(struct sctx *s, fz_point b)
{
	float dx, dy;
	float total, used, ratio;
	fz_point a;
	fz_point m;

	a = s->cur;
	dx = b.x - a.x;
	dy = b.y - a.y;
	total = sqrt(dx * dx + dy * dy);
	used = 0;

	while (total - used > s->dashlist[s->offset] - s->phase)
	{
		used += s->dashlist[s->offset] - s->phase;
		ratio = used / total;
		m.x = a.x + ratio * dx;
		m.y = a.y + ratio * dy;

		if (s->toggle)
			strokelineto(s, m);
		else
			strokemoveto(s, m);

		s->toggle = !s->toggle;
		s->phase = 0;
		s->offset ++;
		if (s->offset == s->dashlen)
			s->offset = 0;
	}

	s->phase += total - used;

	s->cur = b;

	if (s->toggle)
		strokelineto(s, b);
}

static void
dashbezier(struct sctx *s,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc,
	float xd, float yd)
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
	if (dmax < s->flatness) {
		fz_point p;
		p.x = xd;
		p.y = yd;
		dashlineto(s, p);
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

	dashbezier(s, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd);
	dashbezier(s, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd);
}

void
fz_dashpath(fz_gel *gel, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, float flatness, float linewidth)
{
	struct sctx s;
	fz_point p0, p1, p2, p3, beg;
	int i;

	s.gel = gel;
	s.ctm = &ctm;
	s.flatness = flatness;

	s.linecap = stroke->linecap;
	s.linejoin = stroke->linejoin;
	s.linewidth = linewidth * 0.5;
	s.miterlimit = stroke->miterlimit;
	s.sn = 0;
	s.bn = 0;
	s.dot = 0;

	s.dashlist = stroke->dashlist;
	s.dashphase = stroke->dashphase;
	s.dashlen = stroke->dashlen;
	s.toggle = 0;
	s.offset = 0;
	s.phase = 0;

	i = 0;

	if (path->len > 0 && path->els[0].k != FZ_MOVETO)
		fz_warn("assert: path must begin with moveto");

	p0.x = p0.y = 0;

	while (i < path->len)
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			p1.x = path->els[i++].v;
			p1.y = path->els[i++].v;
			dashmoveto(&s, p1);
			beg = p0 = p1;
			break;

		case FZ_LINETO:
			p1.x = path->els[i++].v;
			p1.y = path->els[i++].v;
			dashlineto(&s, p1);
			p0 = p1;
			break;

		case FZ_CURVETO:
			p1.x = path->els[i++].v;
			p1.y = path->els[i++].v;
			p2.x = path->els[i++].v;
			p2.y = path->els[i++].v;
			p3.x = path->els[i++].v;
			p3.y = path->els[i++].v;
			dashbezier(&s, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
			p0 = p3;
			break;

		case FZ_CLOSEPATH:
			dashlineto(&s, beg);
			break;
		}
	}

	strokeflush(&s);
}
