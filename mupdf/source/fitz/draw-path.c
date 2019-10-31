#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <math.h>
#include <float.h>
#include <assert.h>

#define MAX_DEPTH 8

/*
	When stroking/filling, we now label the edges as we emit them.

	For filling, we walk the outline of the shape in order, so everything
	is labelled as '0'.

	For stroking, we walk up both sides of the stroke at once; the forward
	side (0), and the reverse side (1). When we get to the top, either
	both sides join back to where they started, or we cap them.

	The start cap is labelled 2, the end cap is labelled 0.

	These labels are ignored for edge based rasterization, but are required
	for edgebuffer based rasterization.

	Consider the following simplified ascii art diagram of a stroke from
	left to right with 3 sections.

	|            0           0           0
	|      +----->-----+----->-----+----->-----+
	|      |                                   |
	|      ^ 2   A           B           C     v 0
	|      |                                   |
	|      +-----<-----+-----<-----+-----<-----+
	|            1           1           1

	Edge 0 is sent in order (the top edge of A then B then C, left to right
	in the above diagram). Edge 1 is sent in reverse order (the bottom edge
	of A then B then C, still left to right in the above diagram, even though
	the sense of the line is right to left).

	Finally any caps required are sent, 0 and 2.

	It would be nicer if we could roll edge 2 into edge 1, but to do that
	we'd need to know in advance if a stroke was closed or not, so we have
	special case code in the edgebuffer based rasterizer to cope with this.
*/

static void
line(fz_context *ctx, fz_rasterizer *rast, fz_matrix ctm, float x0, float y0, float x1, float y1)
{
	float tx0 = ctm.a * x0 + ctm.c * y0 + ctm.e;
	float ty0 = ctm.b * x0 + ctm.d * y0 + ctm.f;
	float tx1 = ctm.a * x1 + ctm.c * y1 + ctm.e;
	float ty1 = ctm.b * x1 + ctm.d * y1 + ctm.f;
	fz_insert_rasterizer(ctx, rast, tx0, ty0, tx1, ty1, 0);
}

static void
bezier(fz_context *ctx, fz_rasterizer *rast, fz_matrix ctm, float flatness,
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
	dmax = fz_abs(xa - xb);
	dmax = fz_max(dmax, fz_abs(ya - yb));
	dmax = fz_max(dmax, fz_abs(xd - xc));
	dmax = fz_max(dmax, fz_abs(yd - yc));
	if (dmax < flatness || depth >= MAX_DEPTH)
	{
		line(ctx, rast, ctm, xa, ya, xd, yd);
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
	/* xbc *= 0.5f; ybc *= 0.5f; */
	xcd *= 0.5f; ycd *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;
	xbcd *= 0.25f; ybcd *= 0.25f;

	xabcd *= 0.125f; yabcd *= 0.125f;

	bezier(ctx, rast, ctm, flatness, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd, depth + 1);
	bezier(ctx, rast, ctm, flatness, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd, depth + 1);
}

static void
quad(fz_context *ctx, fz_rasterizer *rast, fz_matrix ctm, float flatness,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc, int depth)
{
	float dmax;
	float xab, yab;
	float xbc, ybc;
	float xabc, yabc;

	/* termination check */
	dmax = fz_abs(xa - xb);
	dmax = fz_max(dmax, fz_abs(ya - yb));
	dmax = fz_max(dmax, fz_abs(xc - xb));
	dmax = fz_max(dmax, fz_abs(yc - yb));
	if (dmax < flatness || depth >= MAX_DEPTH)
	{
		line(ctx, rast, ctm, xa, ya, xc, yc);
		return;
	}

	xab = xa + xb;
	yab = ya + yb;
	xbc = xb + xc;
	ybc = yb + yc;

	xabc = xab + xbc;
	yabc = yab + ybc;

	xab *= 0.5f; yab *= 0.5f;
	xbc *= 0.5f; ybc *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;

	quad(ctx, rast, ctm, flatness, xa, ya, xab, yab, xabc, yabc, depth + 1);
	quad(ctx, rast, ctm, flatness, xabc, yabc, xbc, ybc, xc, yc, depth + 1);
}

typedef struct
{
	fz_rasterizer *rast;
	fz_matrix ctm;
	float flatness;
	fz_point b;
	fz_point c;
}
flatten_arg;

static void
flatten_moveto(fz_context *ctx, void *arg_, float x, float y)
{
	flatten_arg *arg = (flatten_arg *)arg_;

	/* implicit closepath before moveto */
	if (arg->c.x != arg->b.x || arg->c.y != arg->b.y)
		line(ctx, arg->rast, arg->ctm, arg->c.x, arg->c.y, arg->b.x, arg->b.y);
	arg->c.x = arg->b.x = x;
	arg->c.y = arg->b.y = y;

	fz_gap_rasterizer(ctx, arg->rast);
}

static void
flatten_lineto(fz_context *ctx, void *arg_, float x, float y)
{
	flatten_arg *arg = (flatten_arg *)arg_;

	line(ctx, arg->rast, arg->ctm, arg->c.x, arg->c.y, x, y);
	arg->c.x = x;
	arg->c.y = y;
}

static void
flatten_curveto(fz_context *ctx, void *arg_, float x1, float y1, float x2, float y2, float x3, float y3)
{
	flatten_arg *arg = (flatten_arg *)arg_;

	bezier(ctx, arg->rast, arg->ctm, arg->flatness, arg->c.x, arg->c.y, x1, y1, x2, y2, x3, y3, 0);
	arg->c.x = x3;
	arg->c.y = y3;
}

static void
flatten_quadto(fz_context *ctx, void *arg_, float x1, float y1, float x2, float y2)
{
	flatten_arg *arg = (flatten_arg *)arg_;

	quad(ctx, arg->rast, arg->ctm, arg->flatness, arg->c.x, arg->c.y, x1, y1, x2, y2, 0);
	arg->c.x = x2;
	arg->c.y = y2;
}

static void
flatten_close(fz_context *ctx, void *arg_)
{
	flatten_arg *arg = (flatten_arg *)arg_;

	line(ctx, arg->rast, arg->ctm, arg->c.x, arg->c.y, arg->b.x, arg->b.y);
	arg->c.x = arg->b.x;
	arg->c.y = arg->b.y;
}

static void
flatten_rectto(fz_context *ctx, void *arg_, float x0, float y0, float x1, float y1)
{
	flatten_arg *arg = (flatten_arg *)arg_;
	fz_matrix ctm = arg->ctm;

	flatten_moveto(ctx, arg_, x0, y0);

	if (fz_antidropout_rasterizer(ctx, arg->rast))
	{
		/* In the case where we have an axis aligned rectangle, do some
		 * horrid antidropout stuff. */
		if (ctm.b == 0 && ctm.c == 0)
		{
			float tx0 = ctm.a * x0 + ctm.e;
			float ty0 = ctm.d * y0 + ctm.f;
			float tx1 = ctm.a * x1 + ctm.e;
			float ty1 = ctm.d * y1 + ctm.f;
			fz_insert_rasterizer_rect(ctx, arg->rast, tx0, ty0, tx1, ty1);
			return;
		}
		else if (ctm.a == 0 && ctm.d == 0)
		{
			float tx0 = ctm.c * y0 + ctm.e;
			float ty0 = ctm.b * x0 + ctm.f;
			float tx1 = ctm.c * y1 + ctm.e;
			float ty1 = ctm.b * x1 + ctm.f;
			fz_insert_rasterizer_rect(ctx, arg->rast, tx0, ty1, tx1, ty0);
			return;
		}
	}

	flatten_lineto(ctx, arg_, x1, y0);
	flatten_lineto(ctx, arg_, x1, y1);
	flatten_lineto(ctx, arg_, x0, y1);
	flatten_close(ctx, arg_);
}

static const fz_path_walker flatten_proc =
{
	flatten_moveto,
	flatten_lineto,
	flatten_curveto,
	flatten_close,
	flatten_quadto,
	NULL,
	NULL,
	flatten_rectto
};

int
fz_flatten_fill_path(fz_context *ctx, fz_rasterizer *rast, const fz_path *path, fz_matrix ctm, float flatness, const fz_irect *scissor, fz_irect *bbox)
{
	flatten_arg arg;

	if (fz_reset_rasterizer(ctx, rast, *scissor))
	{
		arg.rast = rast;
		arg.ctm = ctm;
		arg.flatness = flatness;
		arg.b.x = arg.b.y = arg.c.x = arg.c.y = 0;

		fz_walk_path(ctx, path, &flatten_proc, &arg);
		if (arg.c.x != arg.b.x || arg.c.y != arg.b.y)
			line(ctx, rast, ctm, arg.c.x, arg.c.y, arg.b.x, arg.b.y);

		fz_gap_rasterizer(ctx, rast);

		fz_postindex_rasterizer(ctx, rast);
	}

	arg.rast = rast;
	arg.ctm = ctm;
	arg.flatness = flatness;
	arg.b.x = arg.b.y = arg.c.x = arg.c.y = 0;

	fz_walk_path(ctx, path, &flatten_proc, &arg);
	if (arg.c.x != arg.b.x || arg.c.y != arg.b.y)
		line(ctx, rast, ctm, arg.c.x, arg.c.y, arg.b.x, arg.b.y);

	fz_gap_rasterizer(ctx, rast);

	if (!bbox)
		return 0;

	*bbox = fz_bound_rasterizer(ctx, rast);
	return fz_is_empty_irect(fz_intersect_irect(*bbox, *scissor));
}

enum {
	ONLY_MOVES = 0,
	NON_NULL_LINE = 1,
	NULL_LINE
};

typedef struct sctx
{
	fz_rasterizer *rast;
	fz_matrix ctm;
	float flatness;
	const fz_stroke_state *stroke;

	int linejoin;
	float linewidth;
	float miterlimit;
	fz_point beg[2];
	fz_point seg[2];
	int sn;
	int dot;
	int from_bezier;
	fz_point cur;

	fz_rect rect;
	const float *dash_list;
	float dash_phase;
	int dash_len;
	float dash_total;
	int toggle, cap;
	int offset;
	float phase;
	fz_point dash_cur;
	fz_point dash_beg;
} sctx;

static void
fz_add_line(fz_context *ctx, sctx *s, float x0, float y0, float x1, float y1, int rev)
{
	float tx0 = s->ctm.a * x0 + s->ctm.c * y0 + s->ctm.e;
	float ty0 = s->ctm.b * x0 + s->ctm.d * y0 + s->ctm.f;
	float tx1 = s->ctm.a * x1 + s->ctm.c * y1 + s->ctm.e;
	float ty1 = s->ctm.b * x1 + s->ctm.d * y1 + s->ctm.f;

	fz_insert_rasterizer(ctx, s->rast, tx0, ty0, tx1, ty1, rev);
}

static void
fz_add_horiz_rect(fz_context *ctx, sctx *s, float x0, float y0, float x1, float y1)
{
	if (fz_antidropout_rasterizer(ctx, s->rast)) {
		if (s->ctm.b == 0 && s->ctm.c == 0)
		{
			float tx0 = s->ctm.a * x0 + s->ctm.e;
			float ty0 = s->ctm.d * y0 + s->ctm.f;
			float tx1 = s->ctm.a * x1 + s->ctm.e;
			float ty1 = s->ctm.d * y1 + s->ctm.f;
			fz_insert_rasterizer_rect(ctx, s->rast, tx1, ty1, tx0, ty0);
			return;
		}
		else if (s->ctm.a == 0 && s->ctm.d == 0)
		{
			float tx0 = s->ctm.c * y0 + s->ctm.e;
			float ty0 = s->ctm.b * x0 + s->ctm.f;
			float tx1 = s->ctm.c * y1 + s->ctm.e;
			float ty1 = s->ctm.b * x1 + s->ctm.f;
			fz_insert_rasterizer_rect(ctx, s->rast, tx1, ty0, tx0, ty1);
			return;
		}
	}

	fz_add_line(ctx, s, x0, y0, x1, y0, 0);
	fz_add_line(ctx, s, x1, y1, x0, y1, 1);
}

static void
fz_add_vert_rect(fz_context *ctx, sctx *s, float x0, float y0, float x1, float y1)
{
	if (fz_antidropout_rasterizer(ctx, s->rast))
	{
		if (s->ctm.b == 0 && s->ctm.c == 0)
		{
			float tx0 = s->ctm.a * x0 + s->ctm.e;
			float ty0 = s->ctm.d * y0 + s->ctm.f;
			float tx1 = s->ctm.a * x1 + s->ctm.e;
			float ty1 = s->ctm.d * y1 + s->ctm.f;
			fz_insert_rasterizer_rect(ctx, s->rast, tx0, ty1, tx1, ty0);
			return;
		}
		else if (s->ctm.a == 0 && s->ctm.d == 0)
		{
			float tx0 = s->ctm.c * y0 + s->ctm.e;
			float ty0 = s->ctm.b * x0 + s->ctm.f;
			float tx1 = s->ctm.c * y1 + s->ctm.e;
			float ty1 = s->ctm.b * x1 + s->ctm.f;
			fz_insert_rasterizer_rect(ctx, s->rast, tx0, ty0, tx1, ty1);
			return;
		}
	}

	fz_add_line(ctx, s, x1, y0, x0, y0, 0);
	fz_add_line(ctx, s, x0, y1, x1, y1, 1);
}

static void
fz_add_arc(fz_context *ctx, sctx *s,
	float xc, float yc,
	float x0, float y0,
	float x1, float y1,
	int rev)
{
	float th0, th1, r;
	float theta;
	float ox, oy, nx, ny;
	int n, i;

	r = fabsf(s->linewidth);
	theta = 2 * FZ_SQRT2 * sqrtf(s->flatness / r);
	th0 = atan2f(y0, x0);
	th1 = atan2f(y1, x1);

	if (r > 0)
	{
		if (th0 < th1)
			th0 += FZ_PI * 2;
		n = ceilf((th0 - th1) / theta);
	}
	else
	{
		if (th1 < th0)
			th1 += FZ_PI * 2;
		n = ceilf((th1 - th0) / theta);
	}

	if (rev)
	{
		ox = x1;
		oy = y1;
		for (i = n-1; i > 0; i--)
		{
			theta = th0 + (th1 - th0) * i / n;
			nx = cosf(theta) * r;
			ny = sinf(theta) * r;
			fz_add_line(ctx, s, xc + nx, yc + ny, xc + ox, yc + oy, rev);
			ox = nx;
			oy = ny;
		}

		fz_add_line(ctx, s, xc + x0, yc + y0, xc + ox, yc + oy, rev);
	}
	else
	{
		ox = x0;
		oy = y0;
		for (i = 1; i < n; i++)
		{
			theta = th0 + (th1 - th0) * i / n;
			nx = cosf(theta) * r;
			ny = sinf(theta) * r;
			fz_add_line(ctx, s, xc + ox, yc + oy, xc + nx, yc + ny, rev);
			ox = nx;
			oy = ny;
		}

		fz_add_line(ctx, s, xc + ox, yc + oy, xc + x1, yc + y1, rev);
	}
}

/* FLT_TINY * FLT_TINY is approximately FLT_EPSILON */
#define FLT_TINY 3.4e-4F
static int find_normal_vectors(float dx, float dy, float linewidth, float *dlx, float *dly)
{
	if (dx == 0)
	{
		if (dy < FLT_TINY && dy > - FLT_TINY)
			goto tiny;
		else if (dy > 0)
			*dlx = linewidth;
		else
			*dlx = -linewidth;
		*dly = 0;
	}
	else if (dy == 0)
	{
		if (dx < FLT_TINY && dx > - FLT_TINY)
			goto tiny;
		else if (dx > 0)
			*dly = -linewidth;
		else
			*dly = linewidth;
		*dlx = 0;
	}
	else
	{
		float sq = dx * dx + dy * dy;
		float scale;

		if (sq < FLT_EPSILON)
			goto tiny;
		scale = linewidth / sqrtf(sq);
		*dlx = dy * scale;
		*dly = -dx * scale;
	}
	return 0;
tiny:
	*dlx = 0;
	*dly = 0;
	return 1;
}

static void
fz_add_line_join(fz_context *ctx, sctx *s, float ax, float ay, float bx, float by, float cx, float cy, int join_under)
{
	float miterlimit = s->miterlimit;
	float linewidth = s->linewidth;
	fz_linejoin linejoin = s->linejoin;
	float dx0, dy0;
	float dx1, dy1;
	float dlx0, dly0;
	float dlx1, dly1;
	float dmx, dmy;
	float dmr2;
	float scale;
	float cross;
	int rev = 0;

	dx0 = bx - ax;
	dy0 = by - ay;

	dx1 = cx - bx;
	dy1 = cy - by;

	cross = dx1 * dy0 - dx0 * dy1;
	/* Ensure that cross >= 0 */
	if (cross < 0)
	{
		float tmp;
		tmp = dx1; dx1 = -dx0; dx0 = -tmp;
		tmp = dy1; dy1 = -dy0; dy0 = -tmp;
		cross = -cross;
		rev = !rev;
	}

	if (find_normal_vectors(dx0, dy0, linewidth, &dlx0, &dly0))
		linejoin = FZ_LINEJOIN_BEVEL;

	if (find_normal_vectors(dx1, dy1, linewidth, &dlx1, &dly1))
		linejoin = FZ_LINEJOIN_BEVEL;

	dmx = (dlx0 + dlx1) * 0.5f;
	dmy = (dly0 + dly1) * 0.5f;
	dmr2 = dmx * dmx + dmy * dmy;

	if (cross * cross < FLT_EPSILON && dx0 * dx1 + dy0 * dy1 >= 0)
		linejoin = FZ_LINEJOIN_BEVEL;

	/* XPS miter joins are clipped at miterlength, rather than simply
	 * being converted to bevelled joins. */
	if (linejoin == FZ_LINEJOIN_MITER_XPS)
	{
		if (cross == 0)
			linejoin = FZ_LINEJOIN_BEVEL;
		else if (dmr2 * miterlimit * miterlimit >= linewidth * linewidth)
			linejoin = FZ_LINEJOIN_MITER;
	}
	else if (linejoin == FZ_LINEJOIN_MITER)
		if (dmr2 * miterlimit * miterlimit < linewidth * linewidth)
			linejoin = FZ_LINEJOIN_BEVEL;

	if (join_under)
	{
		fz_add_line(ctx, s, bx + dlx1, by + dly1, bx + dlx0, by + dly0, !rev);
	}
	else if (rev)
	{
		fz_add_line(ctx, s, bx + dlx1, by + dly1, bx, by, 0);
		fz_add_line(ctx, s, bx, by, bx + dlx0, by + dly0, 0);
	}
	else
	{
		fz_add_line(ctx, s, bx, by, bx + dlx0, by + dly0, 1);
		fz_add_line(ctx, s, bx + dlx1, by + dly1, bx, by, 1);
	}

	switch (linejoin)
	{
	case FZ_LINEJOIN_MITER_XPS:
	{
		float k, t0x, t0y, t1x, t1y;

		scale = linewidth * linewidth / dmr2;
		dmx *= scale;
		dmy *= scale;
		k = (scale - linewidth * miterlimit / sqrtf(dmr2)) / (scale - 1);
		t0x = bx - dmx + k * (dmx - dlx0);
		t0y = by - dmy + k * (dmy - dly0);
		t1x = bx - dmx + k * (dmx - dlx1);
		t1y = by - dmy + k * (dmy - dly1);

		if (rev)
		{
			fz_add_line(ctx, s, t1x, t1y, bx - dlx1, by - dly1, 1);
			fz_add_line(ctx, s, t0x, t0y, t1x, t1y, 1);
			fz_add_line(ctx, s, bx - dlx0, by - dly0, t0x, t0y, 1);
		}
		else
		{
			fz_add_line(ctx, s, bx - dlx0, by - dly0, t0x, t0y, 0);
			fz_add_line(ctx, s, t0x, t0y, t1x, t1y, 0);
			fz_add_line(ctx, s, t1x, t1y, bx - dlx1, by - dly1, 0);
		}
		break;
	}
	case FZ_LINEJOIN_MITER:
		scale = linewidth * linewidth / dmr2;
		dmx *= scale;
		dmy *= scale;

		if (rev)
		{
			fz_add_line(ctx, s, bx - dmx, by - dmy, bx - dlx1, by - dly1, 1);
			fz_add_line(ctx, s, bx - dlx0, by - dly0, bx - dmx, by - dmy, 1);
		}
		else
		{
			fz_add_line(ctx, s, bx - dlx0, by - dly0, bx - dmx, by - dmy, 0);
			fz_add_line(ctx, s, bx - dmx, by - dmy, bx - dlx1, by - dly1, 0);
		}
		break;

	case FZ_LINEJOIN_BEVEL:
		fz_add_line(ctx, s, bx - dlx0, by - dly0, bx - dlx1, by - dly1, rev);
		break;

	case FZ_LINEJOIN_ROUND:
		fz_add_arc(ctx, s, bx, by, -dlx0, -dly0, -dlx1, -dly1, rev);
		break;

	default:
		assert("Invalid line join" == NULL);
	}
}

static void
fz_add_line_cap(fz_context *ctx, sctx *s, float ax, float ay, float bx, float by, fz_linecap linecap, int rev)
{
	float flatness = s->flatness;
	float linewidth = s->linewidth;

	float dx = bx - ax;
	float dy = by - ay;

	float scale = linewidth / sqrtf(dx * dx + dy * dy);
	float dlx = dy * scale;
	float dly = -dx * scale;

	switch (linecap)
	{
	case FZ_LINECAP_BUTT:
		fz_add_line(ctx, s, bx - dlx, by - dly, bx + dlx, by + dly, rev);
		break;

	case FZ_LINECAP_ROUND:
	{
		int i;
		int n = ceilf(FZ_PI / (2.0f * FZ_SQRT2 * sqrtf(flatness / linewidth)));
		float ox = bx - dlx;
		float oy = by - dly;
		for (i = 1; i < n; i++)
		{
			float theta = FZ_PI * i / n;
			float cth = cosf(theta);
			float sth = sinf(theta);
			float nx = bx - dlx * cth - dly * sth;
			float ny = by - dly * cth + dlx * sth;
			fz_add_line(ctx, s, ox, oy, nx, ny, rev);
			ox = nx;
			oy = ny;
		}
		fz_add_line(ctx, s, ox, oy, bx + dlx, by + dly, rev);
		break;
	}

	case FZ_LINECAP_SQUARE:
		fz_add_line(ctx, s, bx - dlx, by - dly,
			bx - dlx - dly, by - dly + dlx, rev);
		fz_add_line(ctx, s, bx - dlx - dly, by - dly + dlx,
			bx + dlx - dly, by + dly + dlx, rev);
		fz_add_line(ctx, s, bx + dlx - dly, by + dly + dlx,
			bx + dlx, by + dly, rev);
		break;

	case FZ_LINECAP_TRIANGLE:
	{
		float mx = -dly;
		float my = dlx;
		fz_add_line(ctx, s, bx - dlx, by - dly, bx + mx, by + my, rev);
		fz_add_line(ctx, s, bx + mx, by + my, bx + dlx, by + dly, rev);
		break;
	}

	default:
		assert("Invalid line cap" == NULL);
	}
}

static void
fz_add_line_dot(fz_context *ctx, sctx *s, float ax, float ay)
{
	float flatness = s->flatness;
	float linewidth = s->linewidth;
	int n = ceilf(FZ_PI / (FZ_SQRT2 * sqrtf(flatness / linewidth)));
	float ox = ax - linewidth;
	float oy = ay;
	int i;

	if (n < 3)
		n = 3;
	for (i = 1; i < n; i++)
	{
		float theta = FZ_PI * 2 * i / n;
		float cth = cosf(theta);
		float sth = sinf(theta);
		float nx = ax - cth * linewidth;
		float ny = ay + sth * linewidth;
		fz_add_line(ctx, s, ox, oy, nx, ny, 0);
		ox = nx;
		oy = ny;
	}

	fz_add_line(ctx, s, ox, oy, ax - linewidth, ay, 0);
}

static void
fz_stroke_flush(fz_context *ctx, sctx *s, fz_linecap start_cap, fz_linecap end_cap)
{
	if (s->sn == 2)
	{
		fz_add_line_cap(ctx, s, s->beg[1].x, s->beg[1].y, s->beg[0].x, s->beg[0].y, start_cap, 2);
		fz_add_line_cap(ctx, s, s->seg[0].x, s->seg[0].y, s->seg[1].x, s->seg[1].y, end_cap, 0);
	}
	else if (s->dot == NULL_LINE)
		fz_add_line_dot(ctx, s, s->beg[0].x, s->beg[0].y);
	fz_gap_rasterizer(ctx, s->rast);
}

static void
fz_stroke_moveto(fz_context *ctx, void *s_, float x, float y)
{
	struct sctx *s = (struct sctx *)s_;

	s->seg[0].x = s->beg[0].x = x;
	s->seg[0].y = s->beg[0].y = y;
	s->sn = 1;
	s->dot = ONLY_MOVES;
	s->from_bezier = 0;
}

static void
fz_stroke_lineto(fz_context *ctx, sctx *s, float x, float y, int from_bezier)
{
	float ox = s->seg[s->sn-1].x;
	float oy = s->seg[s->sn-1].y;
	float dx = x - ox;
	float dy = y - oy;
	float dlx, dly;

	if (find_normal_vectors(dx, dy, s->linewidth, &dlx, &dly))
	{
		if (s->dot == ONLY_MOVES && (s->cap == FZ_LINECAP_ROUND || s->dash_list))
			s->dot = NULL_LINE;
		return;
	}
	s->dot = NON_NULL_LINE;

	if (s->sn == 2)
		fz_add_line_join(ctx, s, s->seg[0].x, s->seg[0].y, ox, oy, x, y, s->from_bezier & from_bezier);

#if 1
	if (0 && dx == 0)
	{
		fz_add_vert_rect(ctx, s, ox - dlx, oy, x + dlx, y);
	}
	else if (dy == 0)
	{
		fz_add_horiz_rect(ctx, s, ox, oy - dly, x, y + dly);
	}
	else
#endif
	{

		fz_add_line(ctx, s, ox - dlx, oy - dly, x - dlx, y - dly, 0);
		fz_add_line(ctx, s, x + dlx, y + dly, ox + dlx, oy + dly, 1);
	}

	if (s->sn == 2)
	{
		s->seg[0] = s->seg[1];
		s->seg[1].x = x;
		s->seg[1].y = y;
	}
	else
	{
		s->seg[1].x = s->beg[1].x = x;
		s->seg[1].y = s->beg[1].y = y;
		s->sn = 2;
	}
	s->from_bezier = from_bezier;
}

static void
fz_stroke_closepath(fz_context *ctx, sctx *s)
{
	if (s->sn == 2)
	{
		fz_stroke_lineto(ctx, s, s->beg[0].x, s->beg[0].y, 0);
		/* fz_stroke_lineto will *normally* end up with s->seg[1] being the x,y coords passed in.
		 * As such, the following line should draw a linejoin between the closing segment of this
		 * subpath (seg[0]->seg[1]) == (seg[0]->beg[0]) and the first segment of this subpath
		 * (beg[0]->beg[1]).
		 * In cases where the line was already at an x,y infinitesimally close to s->beg[0],
		 * fz_stroke_lineto may exit without doing any processing. This leaves seg[0]->seg[1]
		 * pointing at the penultimate line segment. Thus this draws a linejoin between that
		 * penultimate segment and the end segment. This is what we want. */
		fz_add_line_join(ctx, s, s->seg[0].x, s->seg[0].y, s->beg[0].x, s->beg[0].y, s->beg[1].x, s->beg[1].y, 0);
	}
	else if (s->dot == NULL_LINE)
		fz_add_line_dot(ctx, s, s->beg[0].x, s->beg[0].y);

	s->seg[0] = s->beg[0];
	s->sn = 1;
	s->dot = ONLY_MOVES;
	s->from_bezier = 0;

	fz_gap_rasterizer(ctx, s->rast);
}

static void
fz_stroke_bezier(fz_context *ctx, struct sctx *s,
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
	dmax = fz_abs(xa - xb);
	dmax = fz_max(dmax, fz_abs(ya - yb));
	dmax = fz_max(dmax, fz_abs(xd - xc));
	dmax = fz_max(dmax, fz_abs(yd - yc));
	if (dmax < s->flatness || depth >= MAX_DEPTH)
	{
		fz_stroke_lineto(ctx, s, xd, yd, 1);
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
	/* xbc *= 0.5f; ybc *= 0.5f; */
	xcd *= 0.5f; ycd *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;
	xbcd *= 0.25f; ybcd *= 0.25f;

	xabcd *= 0.125f; yabcd *= 0.125f;

	fz_stroke_bezier(ctx, s, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd, depth + 1);
	fz_stroke_bezier(ctx, s, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd, depth + 1);
}

static void
fz_stroke_quad(fz_context *ctx, struct sctx *s,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc, int depth)
{
	float dmax;
	float xab, yab;
	float xbc, ybc;
	float xabc, yabc;

	/* termination check */
	dmax = fz_abs(xa - xb);
	dmax = fz_max(dmax, fz_abs(ya - yb));
	dmax = fz_max(dmax, fz_abs(xc - xb));
	dmax = fz_max(dmax, fz_abs(yc - yb));
	if (dmax < s->flatness || depth >= MAX_DEPTH)
	{
		fz_stroke_lineto(ctx, s, xc, yc, 1);
		return;
	}

	xab = xa + xb;
	yab = ya + yb;
	xbc = xb + xc;
	ybc = yb + yc;

	xabc = xab + xbc;
	yabc = yab + ybc;

	xab *= 0.5f; yab *= 0.5f;
	xbc *= 0.5f; ybc *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;

	fz_stroke_quad(ctx, s, xa, ya, xab, yab, xabc, yabc, depth + 1);
	fz_stroke_quad(ctx, s, xabc, yabc, xbc, ybc, xc, yc, depth + 1);
}

static void
stroke_moveto(fz_context *ctx, void *s_, float x, float y)
{
	sctx *s = (sctx *)s_;

	fz_stroke_flush(ctx, s, s->stroke->start_cap, s->stroke->end_cap);
	fz_stroke_moveto(ctx, s, x, y);
	s->cur.x = x;
	s->cur.y = y;
}

static void
stroke_lineto(fz_context *ctx, void *s_, float x, float y)
{
	sctx *s = (sctx *)s_;

	fz_stroke_lineto(ctx, s, x, y, 0);
	s->cur.x = x;
	s->cur.y = y;
}

static void
stroke_curveto(fz_context *ctx, void *s_, float x1, float y1, float x2, float y2, float x3, float y3)
{
	sctx *s = (sctx *)s_;

	fz_stroke_bezier(ctx, s, s->cur.x, s->cur.y, x1, y1, x2, y2, x3, y3, 0);
	s->cur.x = x3;
	s->cur.y = y3;
}

static void
stroke_quadto(fz_context *ctx, void *s_, float x1, float y1, float x2, float y2)
{
	sctx *s = (sctx *)s_;

	fz_stroke_quad(ctx, s, s->cur.x, s->cur.y, x1, y1, x2, y2, 0);
	s->cur.x = x2;
	s->cur.y = y2;
}

static void
stroke_close(fz_context *ctx, void *s_)
{
	sctx *s = (sctx *)s_;

	fz_stroke_closepath(ctx, s);
}

static const fz_path_walker stroke_proc =
{
	stroke_moveto,
	stroke_lineto,
	stroke_curveto,
	stroke_close,
	stroke_quadto
};

static void
fz_dash_moveto(fz_context *ctx, struct sctx *s, float x, float y)
{
	s->toggle = 1;
	s->offset = 0;
	s->phase = s->dash_phase;

	while (s->phase > 0 && s->phase >= s->dash_list[s->offset])
	{
		s->toggle = !s->toggle;
		s->phase -= s->dash_list[s->offset];
		s->offset ++;
		if (s->offset == s->dash_len)
			s->offset = 0;
	}

	s->dash_cur.x = x;
	s->dash_cur.y = y;

	if (s->toggle)
	{
		fz_stroke_flush(ctx, s, s->cap, s->stroke->end_cap);
		s->cap = s->stroke->start_cap;
		fz_stroke_moveto(ctx, s, x, y);
	}
}

static void
fz_dash_lineto(fz_context *ctx, struct sctx *s, float bx, float by, int from_bezier)
{
	float dx, dy, d;
	float total, used, ratio, tail;
	float ax, ay;
	float mx, my;
	float old_bx, old_by;
	int n;
	int dash_cap = s->stroke->dash_cap;

	ax = s->dash_cur.x;
	ay = s->dash_cur.y;
	dx = bx - ax;
	dy = by - ay;
	used = 0;
	tail = 0;
	total = sqrtf(dx * dx + dy * dy);

	/* If a is off screen, bring it onto the screen. First
	 * horizontally... */
	if ((d = s->rect.x0 - ax) > 0)
	{
		if (bx < s->rect.x0)
		{
			/* Entirely off screen */
			tail = total;
			old_bx = bx;
			old_by = by;
			goto adjust_for_tail;
		}
		ax = s->rect.x0;	/* d > 0, dx > 0 */
		goto a_moved_horizontally;
	}
	else if (d < 0 && (d = (s->rect.x1 - ax)) < 0)
	{
		if (bx > s->rect.x1)
		{
			/* Entirely off screen */
			tail = total;
			old_bx = bx;
			old_by = by;
			goto adjust_for_tail;
		}
		ax = s->rect.x1;	/* d < 0, dx < 0 */
a_moved_horizontally:	/* d and dx have the same sign */
		ay += dy * d/dx;
		used = total * d/dx;
		total -= used;
		dx = bx - ax;
		dy = by - ay;
	}
	/* Then vertically... */
	if ((d = s->rect.y0 - ay) > 0)
	{
		if (by < s->rect.y0)
		{
			/* Entirely off screen */
			tail = total;
			old_bx = bx;
			old_by = by;
			goto adjust_for_tail;
		}
		ay = s->rect.y0;	/* d > 0, dy > 0 */
		goto a_moved_vertically;
	}
	else if (d < 0 && (d = (s->rect.y1 - ay)) < 0)
	{
		if (by > s->rect.y1)
		{
			/* Entirely off screen */
			tail = total;
			old_bx = bx;
			old_by = by;
			goto adjust_for_tail;
		}
		ay = s->rect.y1;	/* d < 0, dy < 0 */
a_moved_vertically:	/* d and dy have the same sign */
		ax += dx * d/dy;
		d = total * d/dy;
		total -= d;
		used += d;
		dx = bx - ax;
		dy = by - ay;
	}
	if (used != 0.0f)
	{
		/* Update the position in the dash array */
		if (s->toggle)
		{
			fz_stroke_lineto(ctx, s, ax, ay, from_bezier);
		}
		else
		{
			fz_stroke_flush(ctx, s, s->cap, s->stroke->dash_cap);
			s->cap = s->stroke->dash_cap;
			fz_stroke_moveto(ctx, s, ax, ay);
		}
		used += s->phase;
		n = used/s->dash_total;
		used -= n*s->dash_total;
		if (n & s->dash_len & 1)
			s->toggle = !s->toggle;
		while (used >= s->dash_list[s->offset])
		{
			used -= s->dash_list[s->offset];
			s->offset++;
			if (s->offset == s->dash_len)
				s->offset = 0;
			s->toggle = !s->toggle;
		}
		if (s->toggle)
		{
			fz_stroke_lineto(ctx, s, ax, ay, from_bezier);
		}
		else
		{
			fz_stroke_flush(ctx, s, s->cap, s->stroke->dash_cap);
			s->cap = s->stroke->dash_cap;
			fz_stroke_moveto(ctx, s, ax, ay);
		}
		s->phase = used;
		used = 0;
	}

	/* Now if bx is off screen, bring it back */
	if ((d = bx - s->rect.x0) < 0)
	{
		old_bx = bx;
		old_by = by;
		bx = s->rect.x0;	/* d < 0, dx < 0 */
		goto b_moved_horizontally;
	}
	else if (d > 0 && (d = (bx - s->rect.x1)) > 0)
	{
		old_bx = bx;
		old_by = by;
		bx = s->rect.x1;	/* d > 0, dx > 0 */
b_moved_horizontally:	/* d and dx have the same sign */
		by -= dy * d/dx;
		tail = total * d/dx;
		total -= tail;
		dx = bx - ax;
		dy = by - ay;
	}
	/* Then vertically... */
	if ((d = by - s->rect.y0) < 0)
	{
		old_bx = bx;
		old_by = by;
		by = s->rect.y0;	/* d < 0, dy < 0 */
		goto b_moved_vertically;
	}
	else if (d > 0 && (d = (by - s->rect.y1)) > 0)
	{
		float t;
		old_bx = bx;
		old_by = by;
		by = s->rect.y1;	/* d > 0, dy > 0 */
b_moved_vertically:	/* d and dy have the same sign */
		bx -= dx * d/dy;
		t = total * d/dy;
		tail += t;
		total -= t;
		dx = bx - ax;
		dy = by - ay;
	}

	while (total - used > s->dash_list[s->offset] - s->phase)
	{
		used += s->dash_list[s->offset] - s->phase;
		ratio = used / total;
		mx = ax + ratio * dx;
		my = ay + ratio * dy;

		if (s->toggle)
		{
			fz_stroke_lineto(ctx, s, mx, my, from_bezier);
		}
		else
		{
			fz_stroke_flush(ctx, s, s->cap, dash_cap);
			s->cap = dash_cap;
			fz_stroke_moveto(ctx, s, mx, my);
		}

		s->toggle = !s->toggle;
		s->phase = 0;
		s->offset ++;
		if (s->offset == s->dash_len)
			s->offset = 0;
	}

	s->phase += total - used;

	if (tail == 0.0f)
	{
		s->dash_cur.x = bx;
		s->dash_cur.y = by;

		if (s->toggle)
		{
			fz_stroke_lineto(ctx, s, bx, by, from_bezier);
		}
	}
	else
	{
adjust_for_tail:
		s->dash_cur.x = old_bx;
		s->dash_cur.y = old_by;
		/* Update the position in the dash array */
		if (s->toggle)
		{
			fz_stroke_lineto(ctx, s, old_bx, old_by, from_bezier);
		}
		else
		{
			fz_stroke_flush(ctx, s, s->cap, dash_cap);
			s->cap = dash_cap;
			fz_stroke_moveto(ctx, s, old_bx, old_by);
		}
		tail += s->phase;
		n = tail/s->dash_total;
		tail -= n*s->dash_total;
		if (n & s->dash_len & 1)
			s->toggle = !s->toggle;
		while (tail > s->dash_list[s->offset])
		{
			tail -= s->dash_list[s->offset];
			s->offset++;
			if (s->offset == s->dash_len)
				s->offset = 0;
			s->toggle = !s->toggle;
		}
		if (s->toggle)
		{
			fz_stroke_lineto(ctx, s, old_bx, old_by, from_bezier);
		}
		else
		{
			fz_stroke_flush(ctx, s, s->cap, dash_cap);
			s->cap = dash_cap;
			fz_stroke_moveto(ctx, s, old_bx, old_by);
		}
		s->phase = tail;
	}
}

static void
fz_dash_bezier(fz_context *ctx, struct sctx *s,
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
	dmax = fz_abs(xa - xb);
	dmax = fz_max(dmax, fz_abs(ya - yb));
	dmax = fz_max(dmax, fz_abs(xd - xc));
	dmax = fz_max(dmax, fz_abs(yd - yc));
	if (dmax < s->flatness || depth >= MAX_DEPTH)
	{
		fz_dash_lineto(ctx, s, xd, yd, 1);
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
	/* xbc *= 0.5f; ybc *= 0.5f; */
	xcd *= 0.5f; ycd *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;
	xbcd *= 0.25f; ybcd *= 0.25f;

	xabcd *= 0.125f; yabcd *= 0.125f;

	fz_dash_bezier(ctx, s, xa, ya, xab, yab, xabc, yabc, xabcd, yabcd, depth + 1);
	fz_dash_bezier(ctx, s, xabcd, yabcd, xbcd, ybcd, xcd, ycd, xd, yd, depth + 1);
}

static void
fz_dash_quad(fz_context *ctx, struct sctx *s,
	float xa, float ya,
	float xb, float yb,
	float xc, float yc, int depth)
{
	float dmax;
	float xab, yab;
	float xbc, ybc;
	float xabc, yabc;

	/* termination check */
	dmax = fz_abs(xa - xb);
	dmax = fz_max(dmax, fz_abs(ya - yb));
	dmax = fz_max(dmax, fz_abs(xc - xb));
	dmax = fz_max(dmax, fz_abs(yc - yb));
	if (dmax < s->flatness || depth >= MAX_DEPTH)
	{
		fz_dash_lineto(ctx, s, xc, yc, 1);
		return;
	}

	xab = xa + xb;
	yab = ya + yb;
	xbc = xb + xc;
	ybc = yb + yc;

	xabc = xab + xbc;
	yabc = yab + ybc;

	xab *= 0.5f; yab *= 0.5f;
	xbc *= 0.5f; ybc *= 0.5f;

	xabc *= 0.25f; yabc *= 0.25f;

	fz_dash_quad(ctx, s, xa, ya, xab, yab, xabc, yabc, depth + 1);
	fz_dash_quad(ctx, s, xabc, yabc, xbc, ybc, xc, yc, depth + 1);
}

static void
dash_moveto(fz_context *ctx, void *s_, float x, float y)
{
	sctx *s = (sctx *)s_;

	fz_dash_moveto(ctx, s, x, y);
	s->dash_beg.x = s->cur.x = x;
	s->dash_beg.y = s->cur.y = y;
}

static void
dash_lineto(fz_context *ctx, void *s_, float x, float y)
{
	sctx *s = (sctx *)s_;

	fz_dash_lineto(ctx, s, x, y, 0);
	s->cur.x = x;
	s->cur.y = y;
}

static void
dash_curveto(fz_context *ctx, void *s_, float x1, float y1, float x2, float y2, float x3, float y3)
{
	sctx *s = (sctx *)s_;

	fz_dash_bezier(ctx, s, s->cur.x, s->cur.y, x1, y1, x2, y2, x3, y3, 0);
	s->cur.x = x3;
	s->cur.y = y3;
}

static void
dash_quadto(fz_context *ctx, void *s_, float x1, float y1, float x2, float y2)
{
	sctx *s = (sctx *)s_;

	fz_dash_quad(ctx, s, s->cur.x, s->cur.y, x1, y1, x2, y2, 0);
	s->cur.x = x2;
	s->cur.y = y2;
}

static void
dash_close(fz_context *ctx, void *s_)
{
	sctx *s = (sctx *)s_;

	fz_dash_lineto(ctx, s, s->dash_beg.x, s->dash_beg.y, 0);
	s->cur.x = s->dash_beg.x;
	s->cur.y = s->dash_beg.y;
}

static const fz_path_walker dash_proc =
{
	dash_moveto,
	dash_lineto,
	dash_curveto,
	dash_close,
	dash_quadto
};

static int
do_flatten_stroke(fz_context *ctx, fz_rasterizer *rast, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth, const fz_irect *scissor, fz_irect *bbox)
{
	struct sctx s;
	const fz_path_walker *proc = &stroke_proc;

	s.stroke = stroke;
	s.rast = rast;
	s.ctm = ctm;
	s.flatness = flatness;
	s.linejoin = stroke->linejoin;
	s.linewidth = linewidth * 0.5f; /* hairlines use a different value from the path value */
	s.miterlimit = stroke->miterlimit;
	s.sn = 0;
	s.dot = ONLY_MOVES;
	s.toggle = 0;
	s.offset = 0;
	s.phase = 0;

	s.cap = stroke->start_cap;

	s.dash_list = NULL;
	s.dash_len = stroke->dash_len;
	if (s.dash_len > 0)
	{
		int i;
		fz_matrix inv;
		float max_expand;
		const float *list = stroke->dash_list;

		s.dash_total = 0;
		for (i = 0; i < s.dash_len; i++)
			s.dash_total += list[i];
		if (s.dash_total == 0)
			return 1;

		s.rect = fz_scissor_rasterizer(ctx, rast);
		if (fz_try_invert_matrix(&inv, ctm))
			return 1;
		s.rect = fz_transform_rect(s.rect, inv);
		s.rect.x0 -= linewidth;
		s.rect.x1 += linewidth;
		s.rect.y0 -= linewidth;
		s.rect.y1 += linewidth;

		max_expand = fz_matrix_max_expansion(ctm);
		if (s.dash_total >= 0.01f && s.dash_total * max_expand >= 0.5f)
		{
			proc = &dash_proc;
			s.dash_phase = fmodf(stroke->dash_phase, s.dash_total);
			s.dash_list = list;
		}
	}

	s.cur.x = s.cur.y = 0;
	fz_walk_path(ctx, path, proc, &s);
	fz_stroke_flush(ctx, &s, s.cap, stroke->end_cap);

	if (!bbox)
		return 0;

	*bbox = fz_bound_rasterizer(ctx, rast);
	return fz_is_empty_irect(*bbox);
}

int
fz_flatten_stroke_path(fz_context *ctx, fz_rasterizer *rast, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth, const fz_irect *scissor, fz_irect *bbox)
{
	if (fz_reset_rasterizer(ctx, rast, *scissor))
	{
		if (do_flatten_stroke(ctx, rast, path, stroke, ctm, flatness, linewidth, scissor, bbox))
			return 1;
		fz_postindex_rasterizer(ctx, rast);
		bbox = NULL;
	}

	return do_flatten_stroke(ctx, rast, path, stroke, ctm, flatness, linewidth, scissor, bbox);
}
