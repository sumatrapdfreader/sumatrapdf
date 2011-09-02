#include "fitz.h"

#define MAX_DEPTH 8

enum { BUTT = 0, ROUND = 1, SQUARE = 2, TRIANGLE = 3, MITER = 0, BEVEL = 2 };

static void
line(fz_gel *gel, fz_matrix *ctm, float x0, float y0, float x1, float y1)
{
	float tx0 = ctm->a * x0 + ctm->c * y0 + ctm->e;
	float ty0 = ctm->b * x0 + ctm->d * y0 + ctm->f;
	float tx1 = ctm->a * x1 + ctm->c * y1 + ctm->e;
	float ty1 = ctm->b * x1 + ctm->d * y1 + ctm->f;
	fz_insert_gel(gel, tx0, ty0, tx1, ty1);
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
	if (dmax < flatness || depth >= MAX_DEPTH)
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
fz_flatten_fill_path(fz_gel *gel, fz_path *path, fz_matrix ctm, float flatness)
{
	float x1, y1, x2, y2, x3, y3;
	float cx = 0;
	float cy = 0;
	float bx = 0;
	float by = 0;
	int i = 0;

	while (i < path->len)
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			/* implicit closepath before moveto */
			if (i && (cx != bx || cy != by))
				line(gel, &ctm, cx, cy, bx, by);
			x1 = path->items[i++].v;
			y1 = path->items[i++].v;
			cx = bx = x1;
			cy = by = y1;
			break;

		case FZ_LINETO:
			x1 = path->items[i++].v;
			y1 = path->items[i++].v;
			line(gel, &ctm, cx, cy, x1, y1);
			cx = x1;
			cy = y1;
			break;

		case FZ_CURVETO:
			x1 = path->items[i++].v;
			y1 = path->items[i++].v;
			x2 = path->items[i++].v;
			y2 = path->items[i++].v;
			x3 = path->items[i++].v;
			y3 = path->items[i++].v;
			bezier(gel, &ctm, flatness, cx, cy, x1, y1, x2, y2, x3, y3, 0);
			cx = x3;
			cy = y3;
			break;

		case FZ_CLOSE_PATH:
			line(gel, &ctm, cx, cy, bx, by);
			cx = bx;
			cy = by;
			break;
		}
	}

	if (i && (cx != bx || cy != by))
		line(gel, &ctm, cx, cy, bx, by);
}

struct sctx
{
	fz_gel *gel;
	fz_matrix *ctm;
	float flatness;

	int linejoin;
	float linewidth;
	float miterlimit;
	fz_point beg[2];
	fz_point seg[2];
	int sn, bn;
	int dot;

	float *dash_list;
	float dash_phase;
	int dash_len;
	int toggle, cap;
	int offset;
	float phase;
	fz_point cur;
};

static void
fz_add_line(struct sctx *s, float x0, float y0, float x1, float y1)
{
	float tx0 = s->ctm->a * x0 + s->ctm->c * y0 + s->ctm->e;
	float ty0 = s->ctm->b * x0 + s->ctm->d * y0 + s->ctm->f;
	float tx1 = s->ctm->a * x1 + s->ctm->c * y1 + s->ctm->e;
	float ty1 = s->ctm->b * x1 + s->ctm->d * y1 + s->ctm->f;
	fz_insert_gel(s->gel, tx0, ty0, tx1, ty1);
}

static void
fz_add_arc(struct sctx *s,
	float xc, float yc,
	float x0, float y0,
	float x1, float y1)
{
	float th0, th1, r;
	float theta;
	float ox, oy, nx, ny;
	int n, i;

	r = fabsf(s->linewidth);
	theta = 2 * (float)M_SQRT2 * sqrtf(s->flatness / r);
	th0 = atan2f(y0, x0);
	th1 = atan2f(y1, x1);

	if (r > 0)
	{
		if (th0 < th1)
			th0 += (float)M_PI * 2;
		n = ceilf((th0 - th1) / theta);
	}
	else
	{
		if (th1 < th0)
			th1 += (float)M_PI * 2;
		n = ceilf((th1 - th0) / theta);
	}

	ox = x0;
	oy = y0;
	for (i = 1; i < n; i++)
	{
		theta = th0 + (th1 - th0) * i / n;
		nx = cosf(theta) * r;
		ny = sinf(theta) * r;
		fz_add_line(s, xc + ox, yc + oy, xc + nx, yc + ny);
		ox = nx;
		oy = ny;
	}

	fz_add_line(s, xc + ox, yc + oy, xc + x1, yc + y1);
}

static void
fz_add_line_stroke(struct sctx *s, fz_point a, fz_point b)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	float scale = s->linewidth / sqrtf(dx * dx + dy * dy);
	float dlx = dy * scale;
	float dly = -dx * scale;
	fz_add_line(s, a.x - dlx, a.y - dly, b.x - dlx, b.y - dly);
	fz_add_line(s, b.x + dlx, b.y + dly, a.x + dlx, a.y + dly);
}

static void
fz_add_line_join(struct sctx *s, fz_point a, fz_point b, fz_point c)
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

	scale = linewidth / sqrtf(dx0 * dx0 + dy0 * dy0);
	dlx0 = dy0 * scale;
	dly0 = -dx0 * scale;

	scale = linewidth / sqrtf(dx1 * dx1 + dy1 * dy1);
	dlx1 = dy1 * scale;
	dly1 = -dx1 * scale;

	cross = dx1 * dy0 - dx0 * dy1;

	dmx = (dlx0 + dlx1) * 0.5f;
	dmy = (dly0 + dly1) * 0.5f;
	dmr2 = dmx * dmx + dmy * dmy;

	if (cross * cross < FLT_EPSILON && dx0 * dx1 + dy0 * dy1 >= 0)
		linejoin = BEVEL;

	if (linejoin == MITER)
		if (dmr2 * miterlimit * miterlimit < linewidth * linewidth)
			linejoin = BEVEL;

	if (linejoin == BEVEL)
	{
		fz_add_line(s, b.x - dlx0, b.y - dly0, b.x - dlx1, b.y - dly1);
		fz_add_line(s, b.x + dlx1, b.y + dly1, b.x + dlx0, b.y + dly0);
	}

	if (linejoin == MITER)
	{
		scale = linewidth * linewidth / dmr2;
		dmx *= scale;
		dmy *= scale;

		if (cross < 0)
		{
			fz_add_line(s, b.x - dlx0, b.y - dly0, b.x - dlx1, b.y - dly1);
			fz_add_line(s, b.x + dlx1, b.y + dly1, b.x + dmx, b.y + dmy);
			fz_add_line(s, b.x + dmx, b.y + dmy, b.x + dlx0, b.y + dly0);
		}
		else
		{
			fz_add_line(s, b.x + dlx1, b.y + dly1, b.x + dlx0, b.y + dly0);
			fz_add_line(s, b.x - dlx0, b.y - dly0, b.x - dmx, b.y - dmy);
			fz_add_line(s, b.x - dmx, b.y - dmy, b.x - dlx1, b.y - dly1);
		}
	}

	if (linejoin == ROUND)
	{
		if (cross < 0)
		{
			fz_add_line(s, b.x - dlx0, b.y - dly0, b.x - dlx1, b.y - dly1);
			fz_add_arc(s, b.x, b.y, dlx1, dly1, dlx0, dly0);
		}
		else
		{
			fz_add_line(s, b.x + dlx1, b.y + dly1, b.x + dlx0, b.y + dly0);
			fz_add_arc(s, b.x, b.y, -dlx0, -dly0, -dlx1, -dly1);
		}
	}
}

static void
fz_add_line_cap(struct sctx *s, fz_point a, fz_point b, int linecap)
{
	float flatness = s->flatness;
	float linewidth = s->linewidth;

	float dx = b.x - a.x;
	float dy = b.y - a.y;

	float scale = linewidth / sqrtf(dx * dx + dy * dy);
	float dlx = dy * scale;
	float dly = -dx * scale;

	if (linecap == BUTT)
		fz_add_line(s, b.x - dlx, b.y - dly, b.x + dlx, b.y + dly);

	if (linecap == ROUND)
	{
		int i;
		int n = ceilf((float)M_PI / (2.0f * (float)M_SQRT2 * sqrtf(flatness / linewidth)));
		float ox = b.x - dlx;
		float oy = b.y - dly;
		for (i = 1; i < n; i++)
		{
			float theta = (float)M_PI * i / n;
			float cth = cosf(theta);
			float sth = sinf(theta);
			float nx = b.x - dlx * cth - dly * sth;
			float ny = b.y - dly * cth + dlx * sth;
			fz_add_line(s, ox, oy, nx, ny);
			ox = nx;
			oy = ny;
		}
		fz_add_line(s, ox, oy, b.x + dlx, b.y + dly);
	}

	if (linecap == SQUARE)
	{
		fz_add_line(s, b.x - dlx, b.y - dly,
			b.x - dlx - dly, b.y - dly + dlx);
		fz_add_line(s, b.x - dlx - dly, b.y - dly + dlx,
			b.x + dlx - dly, b.y + dly + dlx);
		fz_add_line(s, b.x + dlx - dly, b.y + dly + dlx,
			b.x + dlx, b.y + dly);
	}

	if (linecap == TRIANGLE)
	{
		float mx = -dly;
		float my = dlx;
		fz_add_line(s, b.x - dlx, b.y - dly, b.x + mx, b.y + my);
		fz_add_line(s, b.x + mx, b.y + my, b.x + dlx, b.y + dly);
	}
}

static void
fz_add_line_dot(struct sctx *s, fz_point a)
{
	float flatness = s->flatness;
	float linewidth = s->linewidth;
	int n = ceilf((float)M_PI / ((float)M_SQRT2 * sqrtf(flatness / linewidth)));
	float ox = a.x - linewidth;
	float oy = a.y;
	int i;

	for (i = 1; i < n; i++)
	{
		float theta = (float)M_PI * 2 * i / n;
		float cth = cosf(theta);
		float sth = sinf(theta);
		float nx = a.x - cth * linewidth;
		float ny = a.y + sth * linewidth;
		fz_add_line(s, ox, oy, nx, ny);
		ox = nx;
		oy = ny;
	}

	fz_add_line(s, ox, oy, a.x - linewidth, a.y);
}

static void
fz_stroke_flush(struct sctx *s, int start_cap, int end_cap)
{
	if (s->sn == 2)
	{
		fz_add_line_cap(s, s->beg[1], s->beg[0], start_cap);
		fz_add_line_cap(s, s->seg[0], s->seg[1], end_cap);
	}
	else if (s->dot)
	{
		fz_add_line_dot(s, s->beg[0]);
	}
}

static void
fz_stroke_moveto(struct sctx *s, fz_point cur)
{
	s->seg[0] = cur;
	s->beg[0] = cur;
	s->sn = 1;
	s->bn = 1;
	s->dot = 0;
}

static void
fz_stroke_lineto(struct sctx *s, fz_point cur)
{
	float dx = cur.x - s->seg[s->sn-1].x;
	float dy = cur.y - s->seg[s->sn-1].y;

	if (dx * dx + dy * dy < FLT_EPSILON)
	{
		if (s->cap == ROUND || s->dash_list)
			s->dot = 1;
		return;
	}

	fz_add_line_stroke(s, s->seg[s->sn-1], cur);

	if (s->sn == 2)
	{
		fz_add_line_join(s, s->seg[0], s->seg[1], cur);
		s->seg[0] = s->seg[1];
		s->seg[1] = cur;
	}

	if (s->sn == 1)
		s->seg[s->sn++] = cur;
	if (s->bn == 1)
		s->beg[s->bn++] = cur;
}

static void
fz_stroke_closepath(struct sctx *s)
{
	if (s->sn == 2)
	{
		fz_stroke_lineto(s, s->beg[0]);
		if (s->seg[1].x == s->beg[0].x && s->seg[1].y == s->beg[0].y)
			fz_add_line_join(s, s->seg[0], s->beg[0], s->beg[1]);
		else
			fz_add_line_join(s, s->seg[1], s->beg[0], s->beg[1]);
	}
	else if (s->dot)
	{
		fz_add_line_dot(s, s->beg[0]);
	}

	s->seg[0] = s->beg[0];
	s->bn = 1;
	s->sn = 1;
	s->dot = 0;
}

static void
fz_stroke_bezier(struct sctx *s,
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
	if (dmax < s->flatness || depth >= MAX_DEPTH)
	{
		fz_point p;
		p.x = xd;
		p.y = yd;
		fz_stroke_lineto(s, p);
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

	fz_stroke_bezier(s, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd, depth + 1);
	fz_stroke_bezier(s, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd, depth + 1);
}

void
fz_flatten_stroke_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth)
{
	struct sctx s;
	fz_point p0, p1, p2, p3;
	int i;

	s.gel = gel;
	s.ctm = &ctm;
	s.flatness = flatness;

	s.linejoin = stroke->linejoin;
	s.linewidth = linewidth * 0.5f; /* hairlines use a different value from the path value */
	s.miterlimit = stroke->miterlimit;
	s.sn = 0;
	s.bn = 0;
	s.dot = 0;

	s.dash_list = NULL;
	s.dash_phase = 0;
	s.dash_len = 0;
	s.toggle = 0;
	s.offset = 0;
	s.phase = 0;

	s.cap = stroke->start_cap;

	i = 0;

	if (path->len > 0 && path->items[0].k != FZ_MOVETO)
	{
		fz_warn("assert: path must begin with moveto");
		return;
	}

	p0.x = p0.y = 0;

	while (i < path->len)
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			p1.x = path->items[i++].v;
			p1.y = path->items[i++].v;
			fz_stroke_flush(&s, stroke->start_cap, stroke->end_cap);
			fz_stroke_moveto(&s, p1);
			p0 = p1;
			break;

		case FZ_LINETO:
			p1.x = path->items[i++].v;
			p1.y = path->items[i++].v;
			fz_stroke_lineto(&s, p1);
			p0 = p1;
			break;

		case FZ_CURVETO:
			p1.x = path->items[i++].v;
			p1.y = path->items[i++].v;
			p2.x = path->items[i++].v;
			p2.y = path->items[i++].v;
			p3.x = path->items[i++].v;
			p3.y = path->items[i++].v;
			fz_stroke_bezier(&s, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, 0);
			p0 = p3;
			break;

		case FZ_CLOSE_PATH:
			fz_stroke_closepath(&s);
			break;
		}
	}

	fz_stroke_flush(&s, stroke->start_cap, stroke->end_cap);
}

static void
fz_dash_moveto(struct sctx *s, fz_point a, int start_cap, int end_cap)
{
	s->toggle = 1;
	s->offset = 0;
	s->phase = s->dash_phase;

	while (s->phase >= s->dash_list[s->offset])
	{
		s->toggle = !s->toggle;
		s->phase -= s->dash_list[s->offset];
		s->offset ++;
		if (s->offset == s->dash_len)
			s->offset = 0;
	}

	s->cur = a;

	if (s->toggle)
	{
		fz_stroke_flush(s, s->cap, end_cap);
		s->cap = start_cap;
		fz_stroke_moveto(s, a);
	}
}

static void
fz_dash_lineto(struct sctx *s, fz_point b, int dash_cap)
{
	float dx, dy;
	float total, used, ratio;
	fz_point a;
	fz_point m;

	a = s->cur;
	dx = b.x - a.x;
	dy = b.y - a.y;
	total = sqrtf(dx * dx + dy * dy);
	used = 0;

	while (total - used > s->dash_list[s->offset] - s->phase)
	{
		used += s->dash_list[s->offset] - s->phase;
		ratio = used / total;
		m.x = a.x + ratio * dx;
		m.y = a.y + ratio * dy;

		if (s->toggle)
		{
			fz_stroke_lineto(s, m);
		}
		else
		{
			fz_stroke_flush(s, s->cap, dash_cap);
			s->cap = dash_cap;
			fz_stroke_moveto(s, m);
		}

		s->toggle = !s->toggle;
		s->phase = 0;
		s->offset ++;
		if (s->offset == s->dash_len)
			s->offset = 0;
	}

	s->phase += total - used;

	s->cur = b;

	if (s->toggle)
	{
		fz_stroke_lineto(s, b);
	}
}

static void
fz_dash_bezier(struct sctx *s,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc,
	float xd, float yd, int depth,
	int dash_cap)
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
	if (dmax < s->flatness || depth >= MAX_DEPTH)
	{
		fz_point p;
		p.x = xd;
		p.y = yd;
		fz_dash_lineto(s, p, dash_cap);
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

	fz_dash_bezier(s, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd, depth + 1, dash_cap);
	fz_dash_bezier(s, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd, depth + 1, dash_cap);
}

void
fz_flatten_dash_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth)
{
	struct sctx s;
	fz_point p0, p1, p2, p3, beg;
	float phase_len;
	int i;

	s.gel = gel;
	s.ctm = &ctm;
	s.flatness = flatness;

	s.linejoin = stroke->linejoin;
	s.linewidth = linewidth * 0.5f;
	s.miterlimit = stroke->miterlimit;
	s.sn = 0;
	s.bn = 0;
	s.dot = 0;

	s.dash_list = stroke->dash_list;
	s.dash_phase = stroke->dash_phase;
	s.dash_len = stroke->dash_len;
	s.toggle = 0;
	s.offset = 0;
	s.phase = 0;

	s.cap = stroke->start_cap;

	if (path->len > 0 && path->items[0].k != FZ_MOVETO)
	{
		fz_warn("assert: path must begin with moveto");
		return;
	}

	phase_len = 0;
	for (i = 0; i < stroke->dash_len; i++)
		phase_len += stroke->dash_list[i];
	if (phase_len < 0.01f || phase_len < stroke->linewidth * 0.5f)
	{
		fz_flatten_stroke_path(gel, path, stroke, ctm, flatness, linewidth);
		return;
	}

	p0.x = p0.y = 0;
	i = 0;

	while (i < path->len)
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			p1.x = path->items[i++].v;
			p1.y = path->items[i++].v;
			fz_dash_moveto(&s, p1, stroke->start_cap, stroke->end_cap);
			beg = p0 = p1;
			break;

		case FZ_LINETO:
			p1.x = path->items[i++].v;
			p1.y = path->items[i++].v;
			fz_dash_lineto(&s, p1, stroke->dash_cap);
			p0 = p1;
			break;

		case FZ_CURVETO:
			p1.x = path->items[i++].v;
			p1.y = path->items[i++].v;
			p2.x = path->items[i++].v;
			p2.y = path->items[i++].v;
			p3.x = path->items[i++].v;
			p3.y = path->items[i++].v;
			fz_dash_bezier(&s, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, 0, stroke->dash_cap);
			p0 = p3;
			break;

		case FZ_CLOSE_PATH:
			fz_dash_lineto(&s, beg, stroke->dash_cap);
			p0 = p1 = beg;
			break;
		}
	}

	fz_stroke_flush(&s, s.cap, stroke->end_cap);
}
