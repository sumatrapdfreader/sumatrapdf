#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

static char *
xps_parse_float_array(fz_context *ctx, xps_document *doc, char *s, int num, float *x)
{
	int k = 0;

	if (s == NULL || *s == 0)
		return NULL;

	while (*s)
	{
		while (*s == 0x0d || *s == '\t' || *s == ' ' || *s == 0x0a)
			s++;
		x[k] = fz_strtof(s, &s);
		while (*s == 0x0d || *s == '\t' || *s == ' ' || *s == 0x0a)
			s++;
		if (*s == ',')
			s++;
		if (++k == num)
			break;
	}
	return s;
}

char *
xps_parse_point(fz_context *ctx, xps_document *doc, char *s_in, float *x, float *y)
{
	char *s_out = s_in;
	float xy[2];

	s_out = xps_parse_float_array(ctx, doc, s_out, 2, &xy[0]);
	*x = xy[0];
	*y = xy[1];
	return s_out;
}

/* Draw an arc segment transformed by the matrix, we approximate with straight
 * line segments. We cannot use the fz_arc function because they only draw
 * circular arcs, we need to transform the line to make them elliptical but
 * without transforming the line width.
 *
 * We are guaranteed that on entry the point is at the point that would be
 * calculated by th0, and on exit, a point is generated for us at th0.
 */
static void
xps_draw_arc_segment(fz_context *ctx, xps_document *doc, fz_path *path, fz_matrix mtx, float th0, float th1, int iscw)
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

/* Given two vectors find the angle between them. */
static float
angle_between(fz_point u, fz_point v)
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

/*
	Some explanation of the parameters here is warranted. See:

	http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes

	Add an arc segment to path, that describes a section of an elliptical
	arc from the current point of path to (point_x,point_y), such that:

	The arc segment is taken from an elliptical arc of semi major radius
	size_x, semi minor radius size_y, where the semi major axis of the
	ellipse is rotated by rotation_angle.

	If is_large_arc, then the arc segment is selected to be > 180 degrees.

	If is_clockwise, then the arc sweeps clockwise.
*/
static void
xps_draw_arc(fz_context *ctx, xps_document *doc, fz_path *path,
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
	xps_draw_arc_segment(ctx, doc, path, mtx, th1, th1 + dth, is_clockwise);

	fz_lineto(ctx, path, point_x, point_y);
}

/*
 * Parse an abbreviated geometry string, and call
 * ghostscript moveto/lineto/curveto functions to
 * build up a path.
 */

fz_path *
xps_parse_abbreviated_geometry(fz_context *ctx, xps_document *doc, char *geom, int *fill_rule)
{
	fz_path *path;
	char **args = NULL;
	char **pargs;
	char *s = geom;
	fz_point pt;
	int i, n;
	int cmd, old;
	float x1, y1, x2, y2, x3, y3;
	float smooth_x, smooth_y; /* saved cubic bezier control point for smooth curves */
	int reset_smooth;

	fz_var(args);

	path = fz_new_path(ctx);

	fz_try(ctx)
	{
		args = fz_malloc_array(ctx, strlen(geom) + 1, char*);
		pargs = args;

		while (*s)
		{
			if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z'))
			{
				*pargs++ = s++;
			}
			else if ((*s >= '0' && *s <= '9') || *s == '.' || *s == '+' || *s == '-' || *s == 'e' || *s == 'E')
			{
				*pargs++ = s;
				while ((*s >= '0' && *s <= '9') || *s == '.' || *s == '+' || *s == '-' || *s == 'e' || *s == 'E')
					s ++;
			}
			else
			{
				s++;
			}
		}

		*pargs = s;

		n = pargs - args;
		i = 0;

		old = 0;

		reset_smooth = 1;
		smooth_x = 0;
		smooth_y = 0;

		while (i < n)
		{
			cmd = args[i][0];
			if (cmd == '+' || cmd == '.' || cmd == '-' || (cmd >= '0' && cmd <= '9'))
				cmd = old; /* it's a number, repeat old command */
			else
				i ++;

			if (reset_smooth)
			{
				smooth_x = 0;
				smooth_y = 0;
			}

			reset_smooth = 1;

			switch (cmd)
			{
			case 'F':
				if (i >= n) break;
				*fill_rule = atoi(args[i]);
				i ++;
				break;

			case 'M':
				if (i + 1 >= n) break;
				fz_moveto(ctx, path, fz_atof(args[i]), fz_atof(args[i+1]));
				i += 2;
				break;
			case 'm':
				if (i + 1 >= n) break;
				pt = fz_currentpoint(ctx, path);
				fz_moveto(ctx, path, pt.x + fz_atof(args[i]), pt.y + fz_atof(args[i+1]));
				i += 2;
				break;

			case 'L':
				if (i + 1 >= n) break;
				fz_lineto(ctx, path, fz_atof(args[i]), fz_atof(args[i+1]));
				i += 2;
				break;
			case 'l':
				if (i + 1 >= n) break;
				pt = fz_currentpoint(ctx, path);
				fz_lineto(ctx, path, pt.x + fz_atof(args[i]), pt.y + fz_atof(args[i+1]));
				i += 2;
				break;

			case 'H':
				if (i >= n) break;
				pt = fz_currentpoint(ctx, path);
				fz_lineto(ctx, path, fz_atof(args[i]), pt.y);
				i += 1;
				break;
			case 'h':
				if (i >= n) break;
				pt = fz_currentpoint(ctx, path);
				fz_lineto(ctx, path, pt.x + fz_atof(args[i]), pt.y);
				i += 1;
				break;

			case 'V':
				if (i >= n) break;
				pt = fz_currentpoint(ctx, path);
				fz_lineto(ctx, path, pt.x, fz_atof(args[i]));
				i += 1;
				break;
			case 'v':
				if (i >= n) break;
				pt = fz_currentpoint(ctx, path);
				fz_lineto(ctx, path, pt.x, pt.y + fz_atof(args[i]));
				i += 1;
				break;

			case 'C':
				if (i + 5 >= n) break;
				x1 = fz_atof(args[i+0]);
				y1 = fz_atof(args[i+1]);
				x2 = fz_atof(args[i+2]);
				y2 = fz_atof(args[i+3]);
				x3 = fz_atof(args[i+4]);
				y3 = fz_atof(args[i+5]);
				fz_curveto(ctx, path, x1, y1, x2, y2, x3, y3);
				i += 6;
				reset_smooth = 0;
				smooth_x = x3 - x2;
				smooth_y = y3 - y2;
				break;

			case 'c':
				if (i + 5 >= n) break;
				pt = fz_currentpoint(ctx, path);
				x1 = fz_atof(args[i+0]) + pt.x;
				y1 = fz_atof(args[i+1]) + pt.y;
				x2 = fz_atof(args[i+2]) + pt.x;
				y2 = fz_atof(args[i+3]) + pt.y;
				x3 = fz_atof(args[i+4]) + pt.x;
				y3 = fz_atof(args[i+5]) + pt.y;
				fz_curveto(ctx, path, x1, y1, x2, y2, x3, y3);
				i += 6;
				reset_smooth = 0;
				smooth_x = x3 - x2;
				smooth_y = y3 - y2;
				break;

			case 'S':
				if (i + 3 >= n) break;
				pt = fz_currentpoint(ctx, path);
				x1 = fz_atof(args[i+0]);
				y1 = fz_atof(args[i+1]);
				x2 = fz_atof(args[i+2]);
				y2 = fz_atof(args[i+3]);
				fz_curveto(ctx, path, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
				i += 4;
				reset_smooth = 0;
				smooth_x = x2 - x1;
				smooth_y = y2 - y1;
				break;

			case 's':
				if (i + 3 >= n) break;
				pt = fz_currentpoint(ctx, path);
				x1 = fz_atof(args[i+0]) + pt.x;
				y1 = fz_atof(args[i+1]) + pt.y;
				x2 = fz_atof(args[i+2]) + pt.x;
				y2 = fz_atof(args[i+3]) + pt.y;
				fz_curveto(ctx, path, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
				i += 4;
				reset_smooth = 0;
				smooth_x = x2 - x1;
				smooth_y = y2 - y1;
				break;

			case 'Q':
				if (i + 3 >= n) break;
				x1 = fz_atof(args[i+0]);
				y1 = fz_atof(args[i+1]);
				x2 = fz_atof(args[i+2]);
				y2 = fz_atof(args[i+3]);
				fz_quadto(ctx, path, x1, y1, x2, y2);
				i += 4;
				break;
			case 'q':
				if (i + 3 >= n) break;
				pt = fz_currentpoint(ctx, path);
				x1 = fz_atof(args[i+0]) + pt.x;
				y1 = fz_atof(args[i+1]) + pt.y;
				x2 = fz_atof(args[i+2]) + pt.x;
				y2 = fz_atof(args[i+3]) + pt.y;
				fz_quadto(ctx, path, x1, y1, x2, y2);
				i += 4;
				break;

			case 'A':
				if (i + 6 >= n) break;
				xps_draw_arc(ctx, doc, path,
					fz_atof(args[i+0]), fz_atof(args[i+1]), fz_atof(args[i+2]),
					atoi(args[i+3]), atoi(args[i+4]),
					fz_atof(args[i+5]), fz_atof(args[i+6]));
				i += 7;
				break;
			case 'a':
				if (i + 6 >= n) break;
				pt = fz_currentpoint(ctx, path);
				xps_draw_arc(ctx, doc, path,
					fz_atof(args[i+0]), fz_atof(args[i+1]), fz_atof(args[i+2]),
					atoi(args[i+3]), atoi(args[i+4]),
					fz_atof(args[i+5]) + pt.x, fz_atof(args[i+6]) + pt.y);
				i += 7;
				break;

			case 'Z':
			case 'z':
				fz_closepath(ctx, path);
				break;

			default:
				fz_warn(ctx, "ignoring invalid command '%c'", cmd);
				if (old == cmd) /* avoid infinite loop */
					i++;
				break;
			}

			old = cmd;
		}
	}
	fz_always(ctx)
		fz_free(ctx, args);
	fz_catch(ctx)
	{
		fz_drop_path(ctx, path);
		fz_rethrow(ctx);
	}

	return path;
}

static void
xps_parse_arc_segment(fz_context *ctx, xps_document *doc, fz_path *path, fz_xml *root, int stroking, int *skipped_stroke)
{
	/* ArcSegment pretty much follows the SVG algorithm for converting an
	 * arc in endpoint representation to an arc in centerpoint
	 * representation. Once in centerpoint it can be given to the
	 * graphics library in the form of a postscript arc. */

	float rotation_angle;
	int is_large_arc, is_clockwise;
	float point_x, point_y;
	float size_x, size_y;
	int is_stroked;

	char *point_att = fz_xml_att(root, "Point");
	char *size_att = fz_xml_att(root, "Size");
	char *rotation_angle_att = fz_xml_att(root, "RotationAngle");
	char *is_large_arc_att = fz_xml_att(root, "IsLargeArc");
	char *sweep_direction_att = fz_xml_att(root, "SweepDirection");
	char *is_stroked_att = fz_xml_att(root, "IsStroked");

	if (!point_att || !size_att || !rotation_angle_att || !is_large_arc_att || !sweep_direction_att)
	{
		fz_warn(ctx, "ArcSegment element is missing attributes");
		return;
	}

	is_stroked = 1;
	if (is_stroked_att && !strcmp(is_stroked_att, "false"))
			is_stroked = 0;
	if (!is_stroked)
		*skipped_stroke = 1;

	point_x = point_y = 0;
	size_x = size_y = 0;

	xps_parse_point(ctx, doc, point_att, &point_x, &point_y);
	xps_parse_point(ctx, doc, size_att, &size_x, &size_y);
	rotation_angle = fz_atof(rotation_angle_att);
	is_large_arc = !strcmp(is_large_arc_att, "true");
	is_clockwise = !strcmp(sweep_direction_att, "Clockwise");

	if (stroking && !is_stroked)
	{
		fz_moveto(ctx, path, point_x, point_y);
		return;
	}

	xps_draw_arc(ctx, doc, path, size_x, size_y, rotation_angle, is_large_arc, is_clockwise, point_x, point_y);
}

static void
xps_parse_poly_quadratic_bezier_segment(fz_context *ctx, xps_document *doc, fz_path *path, fz_xml *root, int stroking, int *skipped_stroke)
{
	char *points_att = fz_xml_att(root, "Points");
	char *is_stroked_att = fz_xml_att(root, "IsStroked");
	float x[2], y[2];
	int is_stroked;
	fz_point pt;
	char *s;
	int n;

	if (!points_att)
	{
		fz_warn(ctx, "PolyQuadraticBezierSegment element has no points");
		return;
	}

	is_stroked = 1;
	if (is_stroked_att && !strcmp(is_stroked_att, "false"))
			is_stroked = 0;
	if (!is_stroked)
		*skipped_stroke = 1;

	s = points_att;
	n = 0;
	while (*s != 0)
	{
		while (*s == ' ') s++;
		s = xps_parse_point(ctx, doc, s, &x[n], &y[n]);
		n ++;
		if (n == 2)
		{
			if (stroking && !is_stroked)
			{
				fz_moveto(ctx, path, x[1], y[1]);
			}
			else
			{
				pt = fz_currentpoint(ctx, path);
				fz_curveto(ctx, path,
						(pt.x + 2 * x[0]) / 3, (pt.y + 2 * y[0]) / 3,
						(x[1] + 2 * x[0]) / 3, (y[1] + 2 * y[0]) / 3,
						x[1], y[1]);
			}
			n = 0;
		}
	}
}

static void
xps_parse_poly_bezier_segment(fz_context *ctx, xps_document *doc, fz_path *path, fz_xml *root, int stroking, int *skipped_stroke)
{
	char *points_att = fz_xml_att(root, "Points");
	char *is_stroked_att = fz_xml_att(root, "IsStroked");
	float x[3], y[3];
	int is_stroked;
	char *s;
	int n;

	if (!points_att)
	{
		fz_warn(ctx, "PolyBezierSegment element has no points");
		return;
	}

	is_stroked = 1;
	if (is_stroked_att && !strcmp(is_stroked_att, "false"))
			is_stroked = 0;
	if (!is_stroked)
		*skipped_stroke = 1;

	s = points_att;
	n = 0;
	while (*s != 0)
	{
		while (*s == ' ') s++;
		s = xps_parse_point(ctx, doc, s, &x[n], &y[n]);
		n ++;
		if (n == 3)
		{
			if (stroking && !is_stroked)
				fz_moveto(ctx, path, x[2], y[2]);
			else
				fz_curveto(ctx, path, x[0], y[0], x[1], y[1], x[2], y[2]);
			n = 0;
		}
	}
}

static void
xps_parse_poly_line_segment(fz_context *ctx, xps_document *doc, fz_path *path, fz_xml *root, int stroking, int *skipped_stroke)
{
	char *points_att = fz_xml_att(root, "Points");
	char *is_stroked_att = fz_xml_att(root, "IsStroked");
	int is_stroked;
	float x, y;
	char *s;

	if (!points_att)
	{
		fz_warn(ctx, "PolyLineSegment element has no points");
		return;
	}

	is_stroked = 1;
	if (is_stroked_att && !strcmp(is_stroked_att, "false"))
			is_stroked = 0;
	if (!is_stroked)
		*skipped_stroke = 1;

	s = points_att;
	while (*s != 0)
	{
		while (*s == ' ') s++;
		s = xps_parse_point(ctx, doc, s, &x, &y);
		if (stroking && !is_stroked)
			fz_moveto(ctx, path, x, y);
		else
			fz_lineto(ctx, path, x, y);
	}
}

static void
xps_parse_path_figure(fz_context *ctx, xps_document *doc, fz_path *path, fz_xml *root, int stroking)
{
	fz_xml *node;

	char *is_closed_att;
	char *start_point_att;
	char *is_filled_att;

	int is_closed = 0;
	int is_filled = 1;
	float start_x = 0;
	float start_y = 0;

	int skipped_stroke = 0;

	is_closed_att = fz_xml_att(root, "IsClosed");
	start_point_att = fz_xml_att(root, "StartPoint");
	is_filled_att = fz_xml_att(root, "IsFilled");

	if (is_closed_att)
		is_closed = !strcmp(is_closed_att, "true");
	if (is_filled_att)
		is_filled = !strcmp(is_filled_att, "true");
	if (start_point_att)
		xps_parse_point(ctx, doc, start_point_att, &start_x, &start_y);

	if (!stroking && !is_filled) /* not filled, when filling */
		return;

	fz_moveto(ctx, path, start_x, start_y);

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "ArcSegment"))
			xps_parse_arc_segment(ctx, doc, path, node, stroking, &skipped_stroke);
		if (fz_xml_is_tag(node, "PolyBezierSegment"))
			xps_parse_poly_bezier_segment(ctx, doc, path, node, stroking, &skipped_stroke);
		if (fz_xml_is_tag(node, "PolyLineSegment"))
			xps_parse_poly_line_segment(ctx, doc, path, node, stroking, &skipped_stroke);
		if (fz_xml_is_tag(node, "PolyQuadraticBezierSegment"))
			xps_parse_poly_quadratic_bezier_segment(ctx, doc, path, node, stroking, &skipped_stroke);
	}

	if (is_closed)
	{
		if (stroking && skipped_stroke)
			fz_lineto(ctx, path, start_x, start_y); /* we've skipped using fz_moveto... */
		else
			fz_closepath(ctx, path); /* no skipped segments, safe to closepath properly */
	}
}

fz_path *
xps_parse_path_geometry(fz_context *ctx, xps_document *doc, xps_resource *dict, fz_xml *root, int stroking, int *fill_rule)
{
	fz_xml *node;

	char *figures_att;
	char *fill_rule_att;
	char *transform_att;

	fz_xml *transform_tag = NULL;
	fz_xml *figures_tag = NULL; /* only used by resource */

	fz_matrix transform;
	fz_path *path;

	figures_att = fz_xml_att(root, "Figures");
	fill_rule_att = fz_xml_att(root, "FillRule");
	transform_att = fz_xml_att(root, "Transform");

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "PathGeometry.Transform"))
			transform_tag = fz_xml_down(node);
	}

	xps_resolve_resource_reference(ctx, doc, dict, &transform_att, &transform_tag, NULL);
	xps_resolve_resource_reference(ctx, doc, dict, &figures_att, &figures_tag, NULL);

	if (fill_rule_att)
	{
		if (!strcmp(fill_rule_att, "NonZero"))
			*fill_rule = 1;
		if (!strcmp(fill_rule_att, "EvenOdd"))
			*fill_rule = 0;
	}

	transform = xps_parse_transform(ctx, doc, transform_att, transform_tag, fz_identity);

	if (figures_att)
		path = xps_parse_abbreviated_geometry(ctx, doc, figures_att, fill_rule);
	else
		path = fz_new_path(ctx);

	fz_try(ctx)
	{
		if (figures_tag)
			xps_parse_path_figure(ctx, doc, path, figures_tag, stroking);

		for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		{
			if (fz_xml_is_tag(node, "PathFigure"))
				xps_parse_path_figure(ctx, doc, path, node, stroking);
		}

		if (transform_att || transform_tag)
			fz_transform_path(ctx, path, transform);
	}
	fz_catch(ctx)
	{
		fz_drop_path(ctx, path);
		fz_rethrow(ctx);
	}

	return path;
}

static int
xps_parse_line_cap(char *attr)
{
	if (attr)
	{
		if (!strcmp(attr, "Flat")) return 0;
		if (!strcmp(attr, "Round")) return 1;
		if (!strcmp(attr, "Square")) return 2;
		if (!strcmp(attr, "Triangle")) return 3;
	}
	return 0;
}

void
xps_clip(fz_context *ctx, xps_document *doc, fz_matrix ctm, xps_resource *dict, char *clip_att, fz_xml *clip_tag)
{
	fz_device *dev = doc->dev;
	fz_path *path;
	int fill_rule = 0;

	if (clip_att)
		path = xps_parse_abbreviated_geometry(ctx, doc, clip_att, &fill_rule);
	else if (clip_tag)
		path = xps_parse_path_geometry(ctx, doc, dict, clip_tag, 0, &fill_rule);
	else
		path = fz_new_path(ctx);
	fz_clip_path(ctx, dev, path, fill_rule == 0, ctm, fz_infinite_rect);
	fz_drop_path(ctx, path);
}

/*
 * Parse an XPS <Path> element, and call relevant ghostscript
 * functions for drawing and/or clipping the child elements.
 */

void
xps_parse_path(fz_context *ctx, xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, fz_xml *root)
{
	fz_device *dev = doc->dev;

	fz_xml *node;

	char *fill_uri;
	char *stroke_uri;
	char *opacity_mask_uri;

	char *transform_att;
	char *clip_att;
	char *data_att;
	char *fill_att;
	char *stroke_att;
	char *opacity_att;
	char *opacity_mask_att;

	fz_xml *transform_tag = NULL;
	fz_xml *clip_tag = NULL;
	fz_xml *data_tag = NULL;
	fz_xml *fill_tag = NULL;
	fz_xml *stroke_tag = NULL;
	fz_xml *opacity_mask_tag = NULL;

	char *fill_opacity_att = NULL;
	char *stroke_opacity_att = NULL;

	char *stroke_dash_array_att;
	char *stroke_dash_cap_att;
	char *stroke_dash_offset_att;
	char *stroke_end_line_cap_att;
	char *stroke_start_line_cap_att;
	char *stroke_line_join_att;
	char *stroke_miter_limit_att;
	char *stroke_thickness_att;

	fz_stroke_state *stroke = NULL;
	float samples[FZ_MAX_COLORS];
	fz_colorspace *colorspace;
	fz_path *path = NULL;
	fz_path *stroke_path = NULL;
	fz_rect area;
	int fill_rule;
	int dash_len = 0;

	/*
	 * Extract attributes and extended attributes.
	 */

	transform_att = fz_xml_att(root, "RenderTransform");
	clip_att = fz_xml_att(root, "Clip");
	data_att = fz_xml_att(root, "Data");
	fill_att = fz_xml_att(root, "Fill");
	stroke_att = fz_xml_att(root, "Stroke");
	opacity_att = fz_xml_att(root, "Opacity");
	opacity_mask_att = fz_xml_att(root, "OpacityMask");

	stroke_dash_array_att = fz_xml_att(root, "StrokeDashArray");
	stroke_dash_cap_att = fz_xml_att(root, "StrokeDashCap");
	stroke_dash_offset_att = fz_xml_att(root, "StrokeDashOffset");
	stroke_end_line_cap_att = fz_xml_att(root, "StrokeEndLineCap");
	stroke_start_line_cap_att = fz_xml_att(root, "StrokeStartLineCap");
	stroke_line_join_att = fz_xml_att(root, "StrokeLineJoin");
	stroke_miter_limit_att = fz_xml_att(root, "StrokeMiterLimit");
	stroke_thickness_att = fz_xml_att(root, "StrokeThickness");

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "Path.RenderTransform"))
			transform_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Path.OpacityMask"))
			opacity_mask_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Path.Clip"))
			clip_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Path.Fill"))
			fill_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Path.Stroke"))
			stroke_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Path.Data"))
			data_tag = fz_xml_down(node);
	}

	fill_uri = base_uri;
	stroke_uri = base_uri;
	opacity_mask_uri = base_uri;

	xps_resolve_resource_reference(ctx, doc, dict, &data_att, &data_tag, NULL);
	xps_resolve_resource_reference(ctx, doc, dict, &clip_att, &clip_tag, NULL);
	xps_resolve_resource_reference(ctx, doc, dict, &transform_att, &transform_tag, NULL);
	xps_resolve_resource_reference(ctx, doc, dict, &fill_att, &fill_tag, &fill_uri);
	xps_resolve_resource_reference(ctx, doc, dict, &stroke_att, &stroke_tag, &stroke_uri);
	xps_resolve_resource_reference(ctx, doc, dict, &opacity_mask_att, &opacity_mask_tag, &opacity_mask_uri);

	/*
	 * Act on the information we have gathered:
	 */

	if (!data_att && !data_tag)
		return;

	if (fz_xml_is_tag(fill_tag, "SolidColorBrush"))
	{
		fill_opacity_att = fz_xml_att(fill_tag, "Opacity");
		fill_att = fz_xml_att(fill_tag, "Color");
		fill_tag = NULL;
	}

	if (fz_xml_is_tag(stroke_tag, "SolidColorBrush"))
	{
		stroke_opacity_att = fz_xml_att(stroke_tag, "Opacity");
		stroke_att = fz_xml_att(stroke_tag, "Color");
		stroke_tag = NULL;
	}

	if (stroke_att || stroke_tag)
	{
		if (stroke_dash_array_att)
		{
			char *s = stroke_dash_array_att;

			while (*s)
			{
				while (*s == ' ')
					s++;
				if (*s) /* needed in case of a space before the last quote */
					dash_len++;

				while (*s && *s != ' ')
					s++;
			}
		}
		stroke = fz_new_stroke_state_with_dash_len(ctx, dash_len);
		stroke->start_cap = xps_parse_line_cap(stroke_start_line_cap_att);
		stroke->dash_cap = xps_parse_line_cap(stroke_dash_cap_att);
		stroke->end_cap = xps_parse_line_cap(stroke_end_line_cap_att);

		stroke->linejoin = FZ_LINEJOIN_MITER_XPS;
		if (stroke_line_join_att)
		{
			if (!strcmp(stroke_line_join_att, "Miter")) stroke->linejoin = FZ_LINEJOIN_MITER_XPS;
			if (!strcmp(stroke_line_join_att, "Round")) stroke->linejoin = FZ_LINEJOIN_ROUND;
			if (!strcmp(stroke_line_join_att, "Bevel")) stroke->linejoin = FZ_LINEJOIN_BEVEL;
		}

		stroke->miterlimit = 10;
		if (stroke_miter_limit_att)
			stroke->miterlimit = fz_atof(stroke_miter_limit_att);

		stroke->linewidth = 1;
		if (stroke_thickness_att)
			stroke->linewidth = fz_atof(stroke_thickness_att);

		stroke->dash_phase = 0;
		stroke->dash_len = 0;
		if (stroke_dash_array_att)
		{
			char *s = stroke_dash_array_att;

			if (stroke_dash_offset_att)
				stroke->dash_phase = fz_atof(stroke_dash_offset_att) * stroke->linewidth;

			while (*s)
			{
				while (*s == ' ')
					s++;
				if (*s) /* needed in case of a space before the last quote */
					stroke->dash_list[stroke->dash_len++] = fz_atof(s) * stroke->linewidth;
				while (*s && *s != ' ')
					s++;
			}
			if (dash_len > 0)
			{
				/* fz_stroke_path doesn't draw non-empty paths with phase length zero */
				float phase_len = 0;
				int i;
				for (i = 0; i < dash_len; i++)
					phase_len += stroke->dash_list[i];
				if (phase_len == 0)
					dash_len = 0;
			}
			stroke->dash_len = dash_len;
		}
	}

	ctm = xps_parse_transform(ctx, doc, transform_att, transform_tag, ctm);

	if (clip_att || clip_tag)
		xps_clip(ctx, doc, ctm, dict, clip_att, clip_tag);

	fz_try(ctx)
	{
		fill_rule = 0;
		if (data_att)
			path = xps_parse_abbreviated_geometry(ctx, doc, data_att, &fill_rule);
		else if (data_tag)
		{
			path = xps_parse_path_geometry(ctx, doc, dict, data_tag, 0, &fill_rule);
			// /home/sebras/src/jxr/fts_06xx.xps
			if (stroke_att || stroke_tag)
				stroke_path = xps_parse_path_geometry(ctx, doc, dict, data_tag, 1, &fill_rule);
		}
		if (!stroke_path)
			stroke_path = path;

		if (stroke_att || stroke_tag)
		{
			area = fz_bound_path(ctx, stroke_path, stroke, ctm);
			if (stroke_path != path && (fill_att || fill_tag)) {
				fz_rect bounds = fz_bound_path(ctx, path, NULL, ctm);
				area = fz_union_rect(area, bounds);
			}
		}
		else
			area = fz_bound_path(ctx, path, NULL, ctm);

		xps_begin_opacity(ctx, doc, ctm, area, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

		if (fill_att)
		{
			xps_parse_color(ctx, doc, base_uri, fill_att, &colorspace, samples);
			if (fill_opacity_att)
				samples[0] *= fz_atof(fill_opacity_att);
			xps_set_color(ctx, doc, colorspace, samples);
			fz_fill_path(ctx, dev, path, fill_rule == 0, ctm,
				doc->colorspace, doc->color, doc->alpha, fz_default_color_params);
		}

		if (fill_tag)
		{
			fz_clip_path(ctx, dev, path, fill_rule == 0, ctm, area);
			xps_parse_brush(ctx, doc, ctm, area, fill_uri, dict, fill_tag);
			fz_pop_clip(ctx, dev);
		}

		if (stroke_att)
		{
			xps_parse_color(ctx, doc, base_uri, stroke_att, &colorspace, samples);
			if (stroke_opacity_att)
				samples[0] *= fz_atof(stroke_opacity_att);
			xps_set_color(ctx, doc, colorspace, samples);
			fz_stroke_path(ctx, dev, stroke_path, stroke, ctm,
				doc->colorspace, doc->color, doc->alpha, fz_default_color_params);
		}

		if (stroke_tag)
		{
			fz_clip_stroke_path(ctx, dev, stroke_path, stroke, ctm, area);
			xps_parse_brush(ctx, doc, ctm, area, stroke_uri, dict, stroke_tag);
			fz_pop_clip(ctx, dev);
		}

		xps_end_opacity(ctx, doc, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);
	}
	fz_always(ctx)
	{
		if (stroke_path != path)
			fz_drop_path(ctx, stroke_path);
		fz_drop_path(ctx, path);
		fz_drop_stroke_state(ctx, stroke);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	if (clip_att || clip_tag)
		fz_pop_clip(ctx, dev);
}
