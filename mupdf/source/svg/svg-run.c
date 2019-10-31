#include "mupdf/fitz.h"
#include "svg-imp.h"

#include <string.h>
#include <math.h>

/* default page size */
#define DEF_WIDTH 612
#define DEF_HEIGHT 792
#define DEF_FONTSIZE 12

#define MAX_USE_DEPTH 100

typedef struct svg_state_s svg_state;

struct svg_state_s
{
	fz_matrix transform;
	fz_stroke_state stroke;
	int use_depth;

	float viewport_w, viewport_h;
	float viewbox_w, viewbox_h, viewbox_size;
	float fontsize;

	float opacity;

	int fill_rule;
	int fill_is_set;
	float fill_color[3];
	float fill_opacity;

	int stroke_is_set;
	float stroke_color[3];
	float stroke_opacity;
};

static void svg_parse_common(fz_context *ctx, svg_document *doc, fz_xml *node, svg_state *state);
static void svg_run_element(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *root, const svg_state *state);

static void svg_fill(fz_context *ctx, fz_device *dev, svg_document *doc, fz_path *path, svg_state *state)
{
	float opacity = state->opacity * state->fill_opacity;
	if (path)
		fz_fill_path(ctx, dev, path, state->fill_rule, state->transform, fz_device_rgb(ctx), state->fill_color, opacity, fz_default_color_params);
}

static void svg_stroke(fz_context *ctx, fz_device *dev, svg_document *doc, fz_path *path, svg_state *state)
{
	float opacity = state->opacity * state->stroke_opacity;
	if (path)
		fz_stroke_path(ctx, dev, path, &state->stroke, state->transform, fz_device_rgb(ctx), state->stroke_color, opacity, fz_default_color_params);
}

static void svg_draw_path(fz_context *ctx, fz_device *dev, svg_document *doc, fz_path *path, svg_state *state)
{
	if (state->fill_is_set)
		svg_fill(ctx, dev, doc, path, state);
	if (state->stroke_is_set)
		svg_stroke(ctx, dev, doc, path, state);
}

/*
	We use the MAGIC number 0.551915 as a bezier subdivision to approximate
	a quarter circle arc. The reasons for this can be found here:
	http://mechanicalexpressions.com/explore/geometric-modeling/circle-spline-approximation.pdf
*/
static const float MAGIC_CIRCLE = 0.551915f;

static void approx_circle(fz_context *ctx, fz_path *path, float cx, float cy, float rx, float ry)
{
	float mx = rx * MAGIC_CIRCLE;
	float my = ry * MAGIC_CIRCLE;
	fz_moveto(ctx, path, cx, cy+ry);
	fz_curveto(ctx, path, cx + mx, cy + ry, cx + rx, cy + my, cx + rx, cy);
	fz_curveto(ctx, path, cx + rx, cy - my, cx + mx, cy - ry, cx, cy - ry);
	fz_curveto(ctx, path, cx - mx, cy - ry, cx - rx, cy - my, cx - rx, cy);
	fz_curveto(ctx, path, cx - rx, cy + my, cx - mx, cy + ry, cx, cy + ry);
	fz_closepath(ctx, path);
}

static void
svg_run_rect(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	char *x_att = fz_xml_att(node, "x");
	char *y_att = fz_xml_att(node, "y");
	char *w_att = fz_xml_att(node, "width");
	char *h_att = fz_xml_att(node, "height");
	char *rx_att = fz_xml_att(node, "rx");
	char *ry_att = fz_xml_att(node, "ry");

	float x = 0;
	float y = 0;
	float w = 0;
	float h = 0;
	float rx = 0;
	float ry = 0;

	fz_path *path;

	svg_parse_common(ctx, doc, node, &local_state);

	if (x_att) x = svg_parse_length(x_att, local_state.viewbox_w, local_state.fontsize);
	if (y_att) y = svg_parse_length(y_att, local_state.viewbox_h, local_state.fontsize);
	if (w_att) w = svg_parse_length(w_att, local_state.viewbox_w, local_state.fontsize);
	if (h_att) h = svg_parse_length(h_att, local_state.viewbox_h, local_state.fontsize);
	if (rx_att) rx = svg_parse_length(rx_att, local_state.viewbox_w, local_state.fontsize);
	if (ry_att) ry = svg_parse_length(ry_att, local_state.viewbox_h, local_state.fontsize);

	if (rx_att && !ry_att)
		ry = rx;
	if (ry_att && !rx_att)
		rx = ry;
	if (rx > w * 0.5f)
		rx = w * 0.5f;
	if (ry > h * 0.5f)
		ry = h * 0.5f;

	if (w <= 0 || h <= 0)
		return;

	path = fz_new_path(ctx);
	fz_try(ctx)
	{
		if (rx == 0 || ry == 0)
		{
			fz_moveto(ctx, path, x, y);
			fz_lineto(ctx, path, x + w, y);
			fz_lineto(ctx, path, x + w, y + h);
			fz_lineto(ctx, path, x, y + h);
		}
		else
		{
			float rxs = rx * MAGIC_CIRCLE;
			float rys = rx * MAGIC_CIRCLE;
			fz_moveto(ctx, path, x + w - rx, y);
			fz_curveto(ctx, path, x + w - rxs, y, x + w, y + rys, x + w, y + ry);
			fz_lineto(ctx, path, x + w, y + h - ry);
			fz_curveto(ctx, path, x + w, y + h - rys, x + w - rxs, y + h, x + w - rx, y + h);
			fz_lineto(ctx, path, x + rx, y + h);
			fz_curveto(ctx, path, x + rxs, y + h, x, y + h - rys, x, y + h - rx);
			fz_lineto(ctx, path, x, y + rx);
			fz_curveto(ctx, path, x, y + rxs, x + rxs, y, x + rx, y);
		}
		fz_closepath(ctx, path);

		svg_draw_path(ctx, dev, doc, path, &local_state);
	}
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);

}

static void
svg_run_circle(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	char *cx_att = fz_xml_att(node, "cx");
	char *cy_att = fz_xml_att(node, "cy");
	char *r_att = fz_xml_att(node, "r");

	float cx = 0;
	float cy = 0;
	float r = 0;
	fz_path *path;

	svg_parse_common(ctx, doc, node, &local_state);

	if (cx_att) cx = svg_parse_length(cx_att, local_state.viewbox_w, local_state.fontsize);
	if (cy_att) cy = svg_parse_length(cy_att, local_state.viewbox_h, local_state.fontsize);
	if (r_att) r = svg_parse_length(r_att, local_state.viewbox_size, 12);

	if (r <= 0)
		return;

	path = fz_new_path(ctx);
	fz_try(ctx)
	{
		approx_circle(ctx, path, cx, cy, r, r);
		svg_draw_path(ctx, dev, doc, path, &local_state);
	}
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
svg_run_ellipse(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	char *cx_att = fz_xml_att(node, "cx");
	char *cy_att = fz_xml_att(node, "cy");
	char *rx_att = fz_xml_att(node, "rx");
	char *ry_att = fz_xml_att(node, "ry");

	float cx = 0;
	float cy = 0;
	float rx = 0;
	float ry = 0;

	fz_path *path;

	svg_parse_common(ctx, doc, node, &local_state);

	if (cx_att) cx = svg_parse_length(cx_att, local_state.viewbox_w, local_state.fontsize);
	if (cy_att) cy = svg_parse_length(cy_att, local_state.viewbox_h, local_state.fontsize);
	if (rx_att) rx = svg_parse_length(rx_att, local_state.viewbox_w, local_state.fontsize);
	if (ry_att) ry = svg_parse_length(ry_att, local_state.viewbox_h, local_state.fontsize);

	if (rx <= 0 || ry <= 0)
		return;

	path = fz_new_path(ctx);
	fz_try(ctx)
	{
		approx_circle(ctx, path, cx, cy, rx, ry);
		svg_draw_path(ctx, dev, doc, path, &local_state);
	}
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
svg_run_line(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	char *x1_att = fz_xml_att(node, "x1");
	char *y1_att = fz_xml_att(node, "y1");
	char *x2_att = fz_xml_att(node, "x2");
	char *y2_att = fz_xml_att(node, "y2");

	float x1 = 0;
	float y1 = 0;
	float x2 = 0;
	float y2 = 0;

	svg_parse_common(ctx, doc, node, &local_state);

	if (x1_att) x1 = svg_parse_length(x1_att, local_state.viewbox_w, local_state.fontsize);
	if (y1_att) y1 = svg_parse_length(y1_att, local_state.viewbox_h, local_state.fontsize);
	if (x2_att) x2 = svg_parse_length(x2_att, local_state.viewbox_w, local_state.fontsize);
	if (y2_att) y2 = svg_parse_length(y2_att, local_state.viewbox_h, local_state.fontsize);

	if (local_state.stroke_is_set)
	{
		fz_path *path = fz_new_path(ctx);
		fz_try(ctx)
		{
			fz_moveto(ctx, path, x1, y1);
			fz_lineto(ctx, path, x2, y2);
			svg_stroke(ctx, dev, doc, path, &local_state);
		}
		fz_always(ctx)
			fz_drop_path(ctx, path);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static fz_path *
svg_parse_polygon_imp(fz_context *ctx, svg_document *doc, fz_xml *node, int doclose)
{
	fz_path *path;

	const char *str = fz_xml_att(node, "points");
	float number;
	float args[2];
	int nargs;
	int isfirst;

	if (!str)
		return NULL;

	isfirst = 1;
	nargs = 0;

	path = fz_new_path(ctx);
	fz_try(ctx)
	{
		while (*str)
		{
			while (svg_is_whitespace_or_comma(*str))
				str ++;

			if (svg_is_digit(*str))
			{
				str = svg_lex_number(&number, str);
				args[nargs++] = number;
			}

			if (nargs == 2)
			{
				if (isfirst)
				{
					fz_moveto(ctx, path, args[0], args[1]);
					isfirst = 0;
				}
				else
				{
					fz_lineto(ctx, path, args[0], args[1]);
				}
				nargs = 0;
			}
		}
	}
	fz_catch(ctx)
	{
		fz_drop_path(ctx, path);
		fz_rethrow(ctx);
	}

	return path;
}

static void
svg_run_polyline(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	svg_parse_common(ctx, doc, node, &local_state);

	if (local_state.stroke_is_set)
	{
		fz_path *path = svg_parse_polygon_imp(ctx, doc, node, 0);
		fz_try(ctx)
			svg_stroke(ctx, dev, doc, path, &local_state);
		fz_always(ctx)
			fz_drop_path(ctx, path);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static void
svg_run_polygon(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;
	fz_path *path;

	svg_parse_common(ctx, doc, node, &local_state);

	path = svg_parse_polygon_imp(ctx, doc, node, 1);
	fz_try(ctx)
		svg_draw_path(ctx, dev, doc, path, &local_state);
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
svg_add_arc_segment(fz_context *ctx, fz_path *path, fz_matrix mtx, float th0, float th1, int iscw)
{
	float t, d;
	fz_point p;

	while (th1 < th0)
		th1 += FZ_PI * 2;

	d = FZ_PI / 180; /* 1-degree precision */

	if (iscw)
	{
		for (t = th0 + d; t < th1 - d/2; t += d)
		{
			p = fz_transform_point_xy(cosf(t), sinf(t), mtx);
			fz_lineto(ctx, path, p.x, p.y);
		}
	}
	else
	{
		th0 += FZ_PI * 2;
		for (t = th0 - d; t > th1 + d/2; t -= d)
		{
			p = fz_transform_point_xy(cosf(t), sinf(t), mtx);
			fz_lineto(ctx, path, p.x, p.y);
		}
	}
}

static float
angle_between(const fz_point u, const fz_point v)
{
	float det = u.x * v.y - u.y * v.x;
	float sign = (det < 0 ? -1 : 1);
	float magu = u.x * u.x + u.y * u.y;
	float magv = v.x * v.x + v.y * v.y;
	float udotv = u.x * v.x + u.y * v.y;
	float t = udotv / (magu * magv);
	/* guard against rounding errors when near |1| (where acos will return NaN) */
	if (t < -1) t = -1;
	if (t > 1) t = 1;
	return sign * acosf(t);
}

static void
svg_add_arc(fz_context *ctx, fz_path *path,
	float size_x, float size_y, float rotation_angle,
	int is_large_arc, int is_clockwise,
	float point_x, float point_y)
{
	fz_matrix rotmat, revmat;
	fz_matrix mtx;
	fz_point pt;
	float rx, ry;
	float x1, y1, x2, y2;
	float x1t, y1t;
	float cxt, cyt, cx, cy;
	float t1, t2, t3;
	float sign;
	float th1, dth;

	pt = fz_currentpoint(ctx, path);
	x1 = pt.x;
	y1 = pt.y;
	x2 = point_x;
	y2 = point_y;
	rx = size_x;
	ry = size_y;

	if (is_clockwise != is_large_arc)
		sign = 1;
	else
		sign = -1;

	rotmat = fz_rotate(rotation_angle);
	revmat = fz_rotate(-rotation_angle);

	/* http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes */
	/* Conversion from endpoint to center parameterization */

	/* F.6.6.1 -- ensure radii are positive and non-zero */
	rx = fabsf(rx);
	ry = fabsf(ry);
	if (rx < 0.001f || ry < 0.001f || (x1 == x2 && y1 == y2))
	{
		fz_lineto(ctx, path, x2, y2);
		return;
	}

	/* F.6.5.1 */
	pt.x = (x1 - x2) / 2;
	pt.y = (y1 - y2) / 2;
	pt = fz_transform_vector(pt, revmat);
	x1t = pt.x;
	y1t = pt.y;

	/* F.6.6.2 -- ensure radii are large enough */
	t1 = (x1t * x1t) / (rx * rx) + (y1t * y1t) / (ry * ry);
	if (t1 > 1)
	{
		rx = rx * sqrtf(t1);
		ry = ry * sqrtf(t1);
	}

	/* F.6.5.2 */
	t1 = (rx * rx * ry * ry) - (rx * rx * y1t * y1t) - (ry * ry * x1t * x1t);
	t2 = (rx * rx * y1t * y1t) + (ry * ry * x1t * x1t);
	t3 = t1 / t2;
	/* guard against rounding errors; sqrt of negative numbers is bad for your health */
	if (t3 < 0) t3 = 0;
	t3 = sqrtf(t3);

	cxt = sign * t3 * (rx * y1t) / ry;
	cyt = sign * t3 * -(ry * x1t) / rx;

	/* F.6.5.3 */
	pt.x = cxt;
	pt.y = cyt;
	pt = fz_transform_vector(pt, rotmat);
	cx = pt.x + (x1 + x2) / 2;
	cy = pt.y + (y1 + y2) / 2;

	/* F.6.5.4 */
	{
		fz_point coord1, coord2, coord3, coord4;
		coord1.x = 1;
		coord1.y = 0;
		coord2.x = (x1t - cxt) / rx;
		coord2.y = (y1t - cyt) / ry;
		coord3.x = (x1t - cxt) / rx;
		coord3.y = (y1t - cyt) / ry;
		coord4.x = (-x1t - cxt) / rx;
		coord4.y = (-y1t - cyt) / ry;
		th1 = angle_between(coord1, coord2);
		dth = angle_between(coord3, coord4);
		if (dth < 0 && !is_clockwise)
			dth += ((FZ_PI / 180) * 360);
		if (dth > 0 && is_clockwise)
			dth -= ((FZ_PI / 180) * 360);
	}

	mtx = fz_pre_scale(fz_pre_rotate(fz_translate(cx, cy), rotation_angle), rx, ry);
	svg_add_arc_segment(ctx, path, mtx, th1, th1 + dth, is_clockwise);

	fz_lineto(ctx, path, point_x, point_y);
}

static fz_path *
svg_parse_path_data(fz_context *ctx, svg_document *doc, const char *str)
{
	fz_path *path;

	fz_point p;
	float x1, y1, x2, y2;

	int cmd;
	float number;
	float args[7];
	int nargs;

	/* saved control point for smooth curves */
	int reset_smooth = 1;
	float smooth_x = 0.0f;
	float smooth_y = 0.0f;

	cmd = 0;
	nargs = 0;

	path = fz_new_path(ctx);
	fz_try(ctx)
	{
		fz_moveto(ctx, path, 0.0f, 0.0f); /* for the case of opening 'm' */

		while (*str)
		{
			while (svg_is_whitespace_or_comma(*str))
				str ++;

			if (svg_is_digit(*str))
			{
				str = svg_lex_number(&number, str);
				if (nargs == nelem(args))
					fz_throw(ctx, FZ_ERROR_GENERIC, "stack overflow in path data");
				args[nargs++] = number;
			}
			else if (svg_is_alpha(*str))
			{
				if (nargs != 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "syntax error in path data (wrong number of parameters to '%c')", cmd);
				cmd = *str++;
			}
			else if (*str == 0)
			{
				break;
			}
			else
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "syntax error in path data: '%c'", *str);
			}

			if (reset_smooth)
			{
				smooth_x = 0.0f;
				smooth_y = 0.0f;
			}

			reset_smooth = 1;

			switch (cmd)
			{
			case 'M':
				if (nargs == 2)
				{
					fz_moveto(ctx, path, args[0], args[1]);
					nargs = 0;
					cmd = 'L'; /* implicit lineto after */
				}
				break;

			case 'm':
				if (nargs == 2)
				{
					p = fz_currentpoint(ctx, path);
					fz_moveto(ctx, path, p.x + args[0], p.y + args[1]);
					nargs = 0;
					cmd = 'l'; /* implicit lineto after */
				}
				break;

			case 'Z':
			case 'z':
				if (nargs == 0)
				{
					fz_closepath(ctx, path);
				}
				break;

			case 'L':
				if (nargs == 2)
				{
					fz_lineto(ctx, path, args[0], args[1]);
					nargs = 0;
				}
				break;

			case 'l':
				if (nargs == 2)
				{
					p = fz_currentpoint(ctx, path);
					fz_lineto(ctx, path, p.x + args[0], p.y + args[1]);
					nargs = 0;
				}
				break;

			case 'H':
				if (nargs == 1)
				{
					p = fz_currentpoint(ctx, path);
					fz_lineto(ctx, path, args[0], p.y);
					nargs = 0;
				}
				break;

			case 'h':
				if (nargs == 1)
				{
					p = fz_currentpoint(ctx, path);
					fz_lineto(ctx, path, p.x + args[0], p.y);
					nargs = 0;
				}
				break;

			case 'V':
				if (nargs == 1)
				{
					p = fz_currentpoint(ctx, path);
					fz_lineto(ctx, path, p.x, args[0]);
					nargs = 0;
				}
				break;

			case 'v':
				if (nargs == 1)
				{
					p = fz_currentpoint(ctx, path);
					fz_lineto(ctx, path, p.x, p.y + args[0]);
					nargs = 0;
				}
				break;

			case 'C':
				reset_smooth = 0;
				if (nargs == 6)
				{
					fz_curveto(ctx, path, args[0], args[1], args[2], args[3], args[4], args[5]);
					smooth_x = args[4] - args[2];
					smooth_y = args[5] - args[3];
					nargs = 0;
				}
				break;

			case 'c':
				reset_smooth = 0;
				if (nargs == 6)
				{
					p = fz_currentpoint(ctx, path);
					fz_curveto(ctx, path,
						p.x + args[0], p.y + args[1],
						p.x + args[2], p.y + args[3],
						p.x + args[4], p.y + args[5]);
					smooth_x = args[4] - args[2];
					smooth_y = args[5] - args[3];
					nargs = 0;
				}
				break;

			case 'S':
				reset_smooth = 0;
				if (nargs == 4)
				{
					p = fz_currentpoint(ctx, path);
					fz_curveto(ctx, path,
							p.x + smooth_x, p.y + smooth_y,
							args[0], args[1],
							args[2], args[3]);
					smooth_x = args[2] - args[0];
					smooth_y = args[3] - args[1];
					nargs = 0;
				}
				break;

			case 's':
				reset_smooth = 0;
				if (nargs == 4)
				{
					p = fz_currentpoint(ctx, path);
					fz_curveto(ctx, path,
							p.x + smooth_x, p.y + smooth_y,
							p.x + args[0], p.y + args[1],
							p.x + args[2], p.y + args[3]);
					smooth_x = args[2] - args[0];
					smooth_y = args[3] - args[1];
					nargs = 0;
				}
				break;

			case 'Q':
				reset_smooth = 0;
				if (nargs == 4)
				{
					p = fz_currentpoint(ctx, path);
					x1 = args[0];
					y1 = args[1];
					x2 = args[2];
					y2 = args[3];
					fz_curveto(ctx, path,
							(p.x + 2 * x1) / 3, (p.y + 2 * y1) / 3,
							(x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
							x2, y2);
					smooth_x = x2 - x1;
					smooth_y = y2 - y1;
					nargs = 0;
				}
				break;

			case 'q':
				reset_smooth = 0;
				if (nargs == 4)
				{
					p = fz_currentpoint(ctx, path);
					x1 = args[0] + p.x;
					y1 = args[1] + p.y;
					x2 = args[2] + p.x;
					y2 = args[3] + p.y;
					fz_curveto(ctx, path,
							(p.x + 2 * x1) / 3, (p.y + 2 * y1) / 3,
							(x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
							x2, y2);
					smooth_x = x2 - x1;
					smooth_y = y2 - y1;
					nargs = 0;
				}
				break;

			case 'T':
				reset_smooth = 0;
				if (nargs == 4)
				{
					p = fz_currentpoint(ctx, path);
					x1 = p.x + smooth_x;
					y1 = p.y + smooth_y;
					x2 = args[0];
					y2 = args[1];
					fz_curveto(ctx, path,
							(p.x + 2 * x1) / 3, (p.y + 2 * y1) / 3,
							(x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
							x2, y2);
					smooth_x = x2 - x1;
					smooth_y = y2 - y1;
					nargs = 0;
				}
				break;

			case 't':
				reset_smooth = 0;
				if (nargs == 4)
				{
					p = fz_currentpoint(ctx, path);
					x1 = p.x + smooth_x;
					y1 = p.y + smooth_y;
					x2 = args[0] + p.x;
					y2 = args[1] + p.y;
					fz_curveto(ctx, path,
							(p.x + 2 * x1) / 3, (p.y + 2 * y1) / 3,
							(x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
							x2, y2);
					smooth_x = x2 - x1;
					smooth_y = y2 - y1;
					nargs = 0;
				}
				break;

			case 'A':
				if (nargs == 7)
				{
					svg_add_arc(ctx, path, args[0], args[1], args[2], args[3], args[4], args[5], args[6]);
					nargs = 0;
				}
				break;
			case 'a':
				if (nargs == 7)
				{
					p = fz_currentpoint(ctx, path);
					svg_add_arc(ctx, path, args[0], args[1], args[2], args[3], args[4], args[5] + p.x, args[6] + p.y);
					nargs = 0;
				}
				break;

			case 0:
				if (nargs != 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "path data must begin with a command");
				break;

			default:
				fz_throw(ctx, FZ_ERROR_GENERIC, "unrecognized command in path data: '%c'", cmd);
			}
		}
	}
	fz_catch(ctx)
	{
		fz_drop_path(ctx, path);
		fz_rethrow(ctx);
	}

	return path;
}

static void
svg_run_path(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *node, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	const char *d_att = fz_xml_att(node, "d");
	/* unused: char *path_length_att = fz_xml_att(node, "pathLength"); */

	svg_parse_common(ctx, doc, node, &local_state);

	if (d_att)
	{
		fz_path *path = svg_parse_path_data(ctx, doc, d_att);
		fz_try(ctx)
			svg_draw_path(ctx, dev, doc, path, &local_state);
		fz_always(ctx)
			fz_drop_path(ctx, path);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

/* svg, symbol, image, foreignObject establish new viewports */
void
svg_parse_viewport(fz_context *ctx, svg_document *doc, fz_xml *node, svg_state *state)
{
	char *w_att = fz_xml_att(node, "width");
	char *h_att = fz_xml_att(node, "height");

	if (w_att)
		state->viewport_w = svg_parse_length(w_att, state->viewbox_w, state->fontsize);
	if (h_att)
		state->viewport_h = svg_parse_length(h_att, state->viewbox_h, state->fontsize);

}

static void
svg_lex_viewbox(const char *s, float *x, float *y, float *w, float *h)
{
	while (svg_is_whitespace_or_comma(*s)) ++s;
	if (svg_is_digit(*s)) s = svg_lex_number(x, s);
	while (svg_is_whitespace_or_comma(*s)) ++s;
	if (svg_is_digit(*s)) s = svg_lex_number(y, s);
	while (svg_is_whitespace_or_comma(*s)) ++s;
	if (svg_is_digit(*s)) s = svg_lex_number(w, s);
	while (svg_is_whitespace_or_comma(*s)) ++s;
	if (svg_is_digit(*s)) s = svg_lex_number(h, s);
}

static int
svg_parse_preserve_aspect_ratio(const char *att, int *x, int *y)
{
	*x = *y = 1;
	if (strstr(att, "none")) return 0;
	if (strstr(att, "xMin")) *x = 0;
	if (strstr(att, "xMid")) *x = 1;
	if (strstr(att, "xMax")) *x = 2;
	if (strstr(att, "YMin")) *y = 0;
	if (strstr(att, "YMid")) *y = 1;
	if (strstr(att, "YMax")) *y = 2;
	return 1;
}

/* svg, symbol, image, foreignObject plus marker, pattern, view can use viewBox to set the transform */
void
svg_parse_viewbox(fz_context *ctx, svg_document *doc, fz_xml *node, svg_state *state)
{
	char *viewbox_att = fz_xml_att(node, "viewBox");
	char *preserve_att = fz_xml_att(node, "preserveAspectRatio");
	if (viewbox_att)
	{
		/* scale and translate to fit [minx miny minx+w miny+h] to [0 0 viewport.w viewport.h] */
		float min_x, min_y, box_w, box_h, sx, sy;
		int align_x=1, align_y=1, preserve=1;
		float pad_x=0, pad_y=0;

		svg_lex_viewbox(viewbox_att, &min_x, &min_y, &box_w, &box_h);
		sx = state->viewport_w / box_w;
		sy = state->viewport_h / box_h;

		if (preserve_att)
			preserve = svg_parse_preserve_aspect_ratio(preserve_att, &align_x, &align_y);
		if (preserve)
		{
			sx = sy = fz_min(sx, sy);
			if (align_x == 1) pad_x = (box_w * sx - state->viewport_w) / 2;
			if (align_x == 2) pad_x = (box_w * sx - state->viewport_w);
			if (align_y == 1) pad_y = (box_h * sy - state->viewport_h) / 2;
			if (align_y == 2) pad_y = (box_h * sy - state->viewport_h);
			state->transform = fz_concat(fz_translate(-pad_x, -pad_y), state->transform);
		}
		state->transform = fz_concat(fz_scale(sx, sy), state->transform);
		state->transform = fz_concat(fz_translate(-min_x, -min_y), state->transform);
		state->viewbox_w = box_w;
		state->viewbox_h = box_h;
		state->viewbox_size = sqrtf(box_w*box_w + box_h*box_h) / sqrtf(2);
	}
}

/* parse transform and presentation attributes */
void
svg_parse_common(fz_context *ctx, svg_document *doc, fz_xml *node, svg_state *state)
{
	fz_stroke_state *stroke = &state->stroke;

	char *transform_att = fz_xml_att(node, "transform");

	char *font_size_att = fz_xml_att(node, "font-size");
	// TODO: all font stuff

	char *style_att = fz_xml_att(node, "style");

	// TODO: clip, clip-path, clip-rule

	char *opacity_att = fz_xml_att(node, "opacity");

	char *fill_att = fz_xml_att(node, "fill");
	char *fill_rule_att = fz_xml_att(node, "fill-rule");
	char *fill_opacity_att = fz_xml_att(node, "fill-opacity");

	char *stroke_att = fz_xml_att(node, "stroke");
	char *stroke_opacity_att = fz_xml_att(node, "stroke-opacity");
	char *stroke_width_att = fz_xml_att(node, "stroke-width");
	char *stroke_linecap_att = fz_xml_att(node, "stroke-linecap");
	char *stroke_linejoin_att = fz_xml_att(node, "stroke-linejoin");
	char *stroke_miterlimit_att = fz_xml_att(node, "stroke-miterlimit");
	// TODO: stroke-dasharray, stroke-dashoffset

	// TODO: marker, marker-start, marker-mid, marker-end

	// TODO: overflow
	// TODO: mask

	/* Dirty hack scans of CSS style */
	if (style_att)
	{
		svg_parse_color_from_style(ctx, doc, style_att,
			&state->fill_is_set, state->fill_color,
			&state->stroke_is_set, state->stroke_color);
	}

	if (transform_att)
	{
		state->transform = svg_parse_transform(ctx, doc, transform_att, state->transform);
	}

	if (font_size_att)
	{
		state->fontsize = svg_parse_length(font_size_att, state->fontsize, state->fontsize);
	}

	if (opacity_att)
	{
		state->opacity = svg_parse_number(opacity_att, 0, 1, state->opacity);
	}

	if (fill_att)
	{
		if (!strcmp(fill_att, "none"))
		{
			state->fill_is_set = 0;
		}
		else
		{
			state->fill_is_set = 1;
			svg_parse_color(ctx, doc, fill_att, state->fill_color);
		}
	}

	if (fill_opacity_att)
		state->fill_opacity = svg_parse_number(fill_opacity_att, 0, 1, state->fill_opacity);

	if (fill_rule_att)
	{
		if (!strcmp(fill_rule_att, "nonzero"))
			state->fill_rule = 1;
		if (!strcmp(fill_rule_att, "evenodd"))
			state->fill_rule = 0;
	}

	if (stroke_att)
	{
		if (!strcmp(stroke_att, "none"))
		{
			state->stroke_is_set = 0;
		}
		else
		{
			state->stroke_is_set = 1;
			svg_parse_color(ctx, doc, stroke_att, state->stroke_color);
		}
	}

	if (stroke_opacity_att)
		state->stroke_opacity = svg_parse_number(stroke_opacity_att, 0, 1, state->stroke_opacity);

	if (stroke_width_att)
	{
		if (!strcmp(stroke_width_att, "inherit"))
			;
		else
			stroke->linewidth = svg_parse_length(stroke_width_att, state->viewbox_size, state->fontsize);
	}
	else
	{
		stroke->linewidth = 1;
	}

	if (stroke_linecap_att)
	{
		if (!strcmp(stroke_linecap_att, "butt"))
			stroke->start_cap = FZ_LINECAP_BUTT;
		if (!strcmp(stroke_linecap_att, "round"))
			stroke->start_cap = FZ_LINECAP_ROUND;
		if (!strcmp(stroke_linecap_att, "square"))
			stroke->start_cap = FZ_LINECAP_SQUARE;
	}
	else
	{
		stroke->start_cap = FZ_LINECAP_BUTT;
	}

	stroke->dash_cap = stroke->start_cap;
	stroke->end_cap = stroke->start_cap;

	if (stroke_linejoin_att)
	{
		if (!strcmp(stroke_linejoin_att, "miter"))
			stroke->linejoin = FZ_LINEJOIN_MITER;
		if (!strcmp(stroke_linejoin_att, "round"))
			stroke->linejoin = FZ_LINEJOIN_ROUND;
		if (!strcmp(stroke_linejoin_att, "bevel"))
			stroke->linejoin = FZ_LINEJOIN_BEVEL;
	}
	else
	{
		stroke->linejoin = FZ_LINEJOIN_MITER;
	}

	if (stroke_miterlimit_att)
	{
		if (!strcmp(stroke_miterlimit_att, "inherit"))
			;
		else
			stroke->miterlimit = svg_parse_length(stroke_miterlimit_att, state->viewbox_size, state->fontsize);
	}
	else
	{
		stroke->miterlimit = 4.0f;
	}
}

static void
svg_run_svg(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *root, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;
	fz_xml *node;

	char *w_att = fz_xml_att(root, "width");
	char *h_att = fz_xml_att(root, "height");
	char *viewbox_att = fz_xml_att(root, "viewBox");

	/* get default viewport from viewBox if width and/or height is missing */
	if (viewbox_att && (!w_att || !h_att))
	{
		float x, y;
		svg_lex_viewbox(viewbox_att, &x, &y, &local_state.viewbox_w, &local_state.viewbox_h);
		if (!w_att) local_state.viewport_w = local_state.viewbox_w;
		if (!h_att) local_state.viewport_h = local_state.viewbox_h;
	}

	svg_parse_viewport(ctx, doc, root, &local_state);
	svg_parse_viewbox(ctx, doc, root, &local_state);
	svg_parse_common(ctx, doc, root, &local_state);

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		svg_run_element(ctx, dev, doc, node, &local_state);
}

static void
svg_run_g(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *root, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;
	fz_xml *node;

	svg_parse_common(ctx, doc, root, &local_state);

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		svg_run_element(ctx, dev, doc, node, &local_state);
}

static void
svg_run_use_symbol(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *use, fz_xml *symbol, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;
	fz_xml *node;

	svg_parse_viewport(ctx, doc, use, &local_state);
	svg_parse_viewbox(ctx, doc, use, &local_state);
	svg_parse_common(ctx, doc, use, &local_state);

	for (node = fz_xml_down(symbol); node; node = fz_xml_next(node))
		svg_run_element(ctx, dev, doc, node, &local_state);
}

static void
svg_run_use(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *root, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;

	char *xlink_href_att = fz_xml_att(root, "xlink:href");
	char *x_att = fz_xml_att(root, "x");
	char *y_att = fz_xml_att(root, "y");

	float x = 0;
	float y = 0;

	if (++local_state.use_depth > MAX_USE_DEPTH)
	{
		fz_warn(ctx, "svg: too much recursion");
		return;
	}

	svg_parse_common(ctx, doc, root, &local_state);
	if (x_att) x = svg_parse_length(x_att, local_state.viewbox_w, local_state.fontsize);
	if (y_att) y = svg_parse_length(y_att, local_state.viewbox_h, local_state.fontsize);

	local_state.transform = fz_concat(fz_translate(x, y), local_state.transform);

	if (xlink_href_att && xlink_href_att[0] == '#')
	{
		fz_xml *linked = fz_tree_lookup(ctx, doc->idmap, xlink_href_att + 1);
		if (linked)
		{
			if (fz_xml_is_tag(linked, "symbol"))
				svg_run_use_symbol(ctx, dev, doc, root, linked, &local_state);
			else
				svg_run_element(ctx, dev, doc, linked, &local_state);
			return;
		}
	}

	fz_warn(ctx, "svg: cannot find linked symbol");
}

static void
svg_run_image(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *root, const svg_state *inherit_state)
{
	svg_state local_state = *inherit_state;
	float x=0, y=0, w=0, h=0;
	const char *data;

	static const char *jpeg_uri = "data:image/jpeg;base64,";
	static const char *png_uri = "data:image/png;base64,";

	char *href_att = fz_xml_att(root, "xlink:href");
	char *x_att = fz_xml_att(root, "x");
	char *y_att = fz_xml_att(root, "y");
	char *w_att = fz_xml_att(root, "width");
	char *h_att = fz_xml_att(root, "height");

	svg_parse_common(ctx, doc, root, &local_state);
	if (x_att) x = svg_parse_length(x_att, local_state.viewbox_w, local_state.fontsize);
	if (y_att) y = svg_parse_length(y_att, local_state.viewbox_h, local_state.fontsize);
	if (w_att) w = svg_parse_length(w_att, local_state.viewbox_w, local_state.fontsize);
	if (h_att) h = svg_parse_length(h_att, local_state.viewbox_h, local_state.fontsize);

	if (w <= 0 || h <= 0)
		return;

	if (!href_att)
		return;

	local_state.transform = fz_concat(fz_translate(x, y), local_state.transform);
	local_state.transform = fz_concat(fz_scale(w, h), local_state.transform);

	if (!strncmp(href_att, jpeg_uri, strlen(jpeg_uri)))
		data = href_att + strlen(jpeg_uri);
	else if (!strncmp(href_att, png_uri, strlen(png_uri)))
		data = href_att + strlen(png_uri);
	else
		data = NULL;
	if (data)
	{
		fz_image *img = NULL;
		fz_buffer *buf;

		fz_var(img);

		buf = fz_new_buffer_from_base64(ctx, data, 0);
		fz_try(ctx)
		{
			img = fz_new_image_from_buffer(ctx, buf);
			fz_fill_image(ctx, dev, img, local_state.transform, 1, fz_default_color_params);
		}
		fz_always(ctx)
		{
			fz_drop_buffer(ctx, buf);
			fz_drop_image(ctx, img);
		}
		fz_catch(ctx)
			fz_warn(ctx, "svg: ignoring embedded image '%s'", href_att);
	}
	else if (doc->zip)
	{
		char path[2048];
		fz_buffer *buf = NULL;
		fz_image *img = NULL;

		fz_var(buf);
		fz_var(img);

		fz_strlcpy(path, doc->base_uri, sizeof path);
		fz_strlcat(path, "/", sizeof path);
		fz_strlcat(path, href_att, sizeof path);
		fz_urldecode(path);
		fz_cleanname(path);

		fz_try(ctx)
		{
			buf = fz_read_archive_entry(ctx, doc->zip, path);
			img = fz_new_image_from_buffer(ctx, buf);
			fz_fill_image(ctx, dev, img, local_state.transform, 1, fz_default_color_params);
		}
		fz_always(ctx)
		{
			fz_drop_buffer(ctx, buf);
			fz_drop_image(ctx, img);
		}
		fz_catch(ctx)
			fz_warn(ctx, "svg: ignoring external image '%s'", href_att);
	}
	else
	{
		fz_warn(ctx, "svg: ignoring external image '%s'", href_att);
	}
}

static void
svg_run_element(fz_context *ctx, fz_device *dev, svg_document *doc, fz_xml *root, const svg_state *state)
{
	if (fz_xml_is_tag(root, "svg"))
		svg_run_svg(ctx, dev, doc, root, state);

	else if (fz_xml_is_tag(root, "g"))
		svg_run_g(ctx, dev, doc, root, state);

	else if (fz_xml_is_tag(root, "title"))
		;
	else if (fz_xml_is_tag(root, "desc"))
		;

	else if (fz_xml_is_tag(root, "defs"))
		;
	else if (fz_xml_is_tag(root, "symbol"))
		;

	else if (fz_xml_is_tag(root, "use"))
		svg_run_use(ctx, dev, doc, root, state);

	else if (fz_xml_is_tag(root, "path"))
		svg_run_path(ctx, dev, doc, root, state);
	else if (fz_xml_is_tag(root, "rect"))
		svg_run_rect(ctx, dev, doc, root, state);
	else if (fz_xml_is_tag(root, "circle"))
		svg_run_circle(ctx, dev, doc, root, state);
	else if (fz_xml_is_tag(root, "ellipse"))
		svg_run_ellipse(ctx, dev, doc, root, state);
	else if (fz_xml_is_tag(root, "line"))
		svg_run_line(ctx, dev, doc, root, state);
	else if (fz_xml_is_tag(root, "polyline"))
		svg_run_polyline(ctx, dev, doc, root, state);
	else if (fz_xml_is_tag(root, "polygon"))
		svg_run_polygon(ctx, dev, doc, root, state);

	else if (fz_xml_is_tag(root, "image"))
		svg_run_image(ctx, dev, doc, root, state);

#if 0
	else if (fz_xml_is_tag(root, "text"))
		svg_run_text(ctx, dev, doc, root);
	else if (fz_xml_is_tag(root, "tspan"))
		svg_run_text_span(ctx, dev, doc, root);
	else if (fz_xml_is_tag(root, "tref"))
		svg_run_text_ref(ctx, dev, doc, root);
	else if (fz_xml_is_tag(root, "textPath"))
		svg_run_text_path(ctx, dev, doc, root);
#endif

	else
	{
		/* ignore unrecognized tags */
	}
}

void
svg_parse_document_bounds(fz_context *ctx, svg_document *doc, fz_xml *root)
{
	char *version_att;
	char *w_att;
	char *h_att;
	char *viewbox_att;
	int version;

	if (!fz_xml_is_tag(root, "svg"))
		fz_throw(ctx, FZ_ERROR_GENERIC, "expected svg element (found %s)", fz_xml_tag(root));

	version_att = fz_xml_att(root, "version");
	w_att = fz_xml_att(root, "width");
	h_att = fz_xml_att(root, "height");
	viewbox_att = fz_xml_att(root, "viewBox");

	version = 10;
	if (version_att)
		version = fz_atof(version_att) * 10;

	if (version > 12)
		fz_warn(ctx, "svg document version is newer than we support");

	/* If no width or height attributes, then guess from the viewbox */
	if (w_att == NULL && h_att == NULL && viewbox_att != NULL)
	{
		float min_x, min_y, box_w, box_h;
		svg_lex_viewbox(viewbox_att, &min_x, &min_y, &box_w, &box_h);
		doc->width = box_w;
		doc->height = box_h;
	}
	else
	{
		doc->width = DEF_WIDTH;
		if (w_att)
			doc->width = svg_parse_length(w_att, doc->width, DEF_FONTSIZE);

		doc->height = DEF_HEIGHT;
		if (h_att)
			doc->height = svg_parse_length(h_att, doc->height, DEF_FONTSIZE);
	}
}

void
svg_run_document(fz_context *ctx, svg_document *doc, fz_xml *root, fz_device *dev, fz_matrix ctm)
{
	svg_state state;

	svg_parse_document_bounds(ctx, doc, root);

	/* Initial graphics state */
	state.transform = ctm;
	state.stroke = fz_default_stroke_state;
	state.use_depth = 0;

	state.viewport_w = DEF_WIDTH;
	state.viewport_h = DEF_HEIGHT;

	state.viewbox_w = DEF_WIDTH;
	state.viewbox_h = DEF_HEIGHT;
	state.viewbox_size = sqrtf(DEF_WIDTH*DEF_WIDTH + DEF_HEIGHT*DEF_HEIGHT) / sqrtf(2);

	state.fontsize = 12;

	state.opacity = 1;

	state.fill_rule = 0;

	state.fill_is_set = 1;
	state.fill_color[0] = 0;
	state.fill_color[1] = 0;
	state.fill_color[2] = 0;
	state.fill_opacity = 1;

	state.stroke_is_set = 0;
	state.stroke_color[0] = 0;
	state.stroke_color[1] = 0;
	state.stroke_color[2] = 0;
	state.stroke_opacity = 1;

	svg_run_svg(ctx, dev, doc, root, &state);
}
