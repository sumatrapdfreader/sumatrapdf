#include "fitz.h"
#include "muxps.h"

static fz_point
fz_currentpoint(fz_path *path)
{
	fz_point c, m;
	int i;

	c.x = c.y = m.x = m.y = 0;
	i = 0;

	while (i < path->len)
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			m.x = c.x = path->items[i++].v;
			m.y = c.y = path->items[i++].v;
			break;
		case FZ_LINETO:
			c.x = path->items[i++].v;
			c.y = path->items[i++].v;
			break;
		case FZ_CURVETO:
			i += 4;
			c.x = path->items[i++].v;
			c.y = path->items[i++].v;
			break;
		case FZ_CLOSE_PATH:
			c = m;
		}
	}

	return c;
}

/* Draw an arc segment transformed by the matrix, we approximate with straight
 * line segments. We cannot use the fz_arc function because they only draw
 * circular arcs, we need to transform the line to make them elliptical but
 * without transforming the line width.
 */
static void
xps_draw_arc_segment(fz_path *path, fz_matrix mtx, float th0, float th1, int iscw)
{
	float t, d;
	fz_point p;

	while (th1 < th0)
		th1 += (float)M_PI * 2;

	d = (float)M_PI / 180; /* 1-degree precision */

	if (iscw)
	{
		p.x = cosf(th0);
		p.y = sinf(th0);
		p = fz_transform_point(mtx, p);
		fz_lineto(path, p.x, p.y);
		for (t = th0; t < th1; t += d)
		{
			p.x = cosf(t);
			p.y = sinf(t);
			p = fz_transform_point(mtx, p);
			fz_lineto(path, p.x, p.y);
		}
		p.x = cosf(th1);
		p.y = sinf(th1);
		p = fz_transform_point(mtx, p);
		fz_lineto(path, p.x, p.y);
	}
	else
	{
		th0 += (float)M_PI * 2;
		p.x = cosf(th0);
		p.y = sinf(th0);
		p = fz_transform_point(mtx, p);
		fz_lineto(path, p.x, p.y);
		for (t = th0; t > th1; t -= d)
		{
			p.x = cosf(t);
			p.y = sinf(t);
			p = fz_transform_point(mtx, p);
			fz_lineto(path, p.x, p.y);
		}
		p.x = cosf(th1);
		p.y = sinf(th1);
		p = fz_transform_point(mtx, p);
		fz_lineto(path, p.x, p.y);
	}
}

/* Given two vectors find the angle between them. */
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
xps_draw_arc(fz_path *path,
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

	pt = fz_currentpoint(path);
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
	if (rx < 0.001f || ry < 0.001f)
	{
		fz_lineto(path, x2, y2);
		return;
	}

	/* F.6.5.1 */
	pt.x = (x1 - x2) / 2;
	pt.y = (y1 - y2) / 2;
	pt = fz_transform_vector(revmat, pt);
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
	pt = fz_transform_vector(rotmat, pt);
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
			dth += (((float)M_PI / 180) * 360);
		if (dth > 0 && is_clockwise)
			dth -= (((float)M_PI / 180) * 360);
	}

	mtx = fz_identity;
	mtx = fz_concat(fz_translate(cx, cy), mtx);
	mtx = fz_concat(fz_rotate(rotation_angle), mtx);
	mtx = fz_concat(fz_scale(rx, ry), mtx);
	xps_draw_arc_segment(path, mtx, th1, th1 + dth, is_clockwise);

	fz_lineto(path, point_x, point_y);
}

/*
 * Parse an abbreviated geometry string, and call
 * ghostscript moveto/lineto/curveto functions to
 * build up a path.
 */

static fz_path *
xps_parse_abbreviated_geometry(xps_context *ctx, char *geom, int *fill_rule)
{
	fz_path *path;
	char **args;
	char **pargs;
	char *s = geom;
	fz_point pt;
	int i, n;
	int cmd, old;
	float x1, y1, x2, y2, x3, y3;
	float smooth_x, smooth_y; /* saved cubic bezier control point for smooth curves */
	int reset_smooth;

	path = fz_new_path();

	args = fz_calloc(strlen(geom) + 1, sizeof(char*));
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

	pargs[0] = s;
	pargs[1] = 0;

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
			*fill_rule = atoi(args[i]);
			i ++;
			break;

		case 'M':
			fz_moveto(path, fz_atof(args[i]), fz_atof(args[i+1]));
			i += 2;
			break;
		case 'm':
			pt = fz_currentpoint(path);
			fz_moveto(path, pt.x + fz_atof(args[i]), pt.y + fz_atof(args[i+1]));
			i += 2;
			break;

		case 'L':
			fz_lineto(path, fz_atof(args[i]), fz_atof(args[i+1]));
			i += 2;
			break;
		case 'l':
			pt = fz_currentpoint(path);
			fz_lineto(path, pt.x + fz_atof(args[i]), pt.y + fz_atof(args[i+1]));
			i += 2;
			break;

		case 'H':
			pt = fz_currentpoint(path);
			fz_lineto(path, fz_atof(args[i]), pt.y);
			i += 1;
			break;
		case 'h':
			pt = fz_currentpoint(path);
			fz_lineto(path, pt.x + fz_atof(args[i]), pt.y);
			i += 1;
			break;

		case 'V':
			pt = fz_currentpoint(path);
			fz_lineto(path, pt.x, fz_atof(args[i]));
			i += 1;
			break;
		case 'v':
			pt = fz_currentpoint(path);
			fz_lineto(path, pt.x, pt.y + fz_atof(args[i]));
			i += 1;
			break;

		case 'C':
			x1 = fz_atof(args[i+0]);
			y1 = fz_atof(args[i+1]);
			x2 = fz_atof(args[i+2]);
			y2 = fz_atof(args[i+3]);
			x3 = fz_atof(args[i+4]);
			y3 = fz_atof(args[i+5]);
			fz_curveto(path, x1, y1, x2, y2, x3, y3);
			i += 6;
			reset_smooth = 0;
			smooth_x = x3 - x2;
			smooth_y = y3 - y2;
			break;

		case 'c':
			pt = fz_currentpoint(path);
			x1 = fz_atof(args[i+0]) + pt.x;
			y1 = fz_atof(args[i+1]) + pt.y;
			x2 = fz_atof(args[i+2]) + pt.x;
			y2 = fz_atof(args[i+3]) + pt.y;
			x3 = fz_atof(args[i+4]) + pt.x;
			y3 = fz_atof(args[i+5]) + pt.y;
			fz_curveto(path, x1, y1, x2, y2, x3, y3);
			i += 6;
			reset_smooth = 0;
			smooth_x = x3 - x2;
			smooth_y = y3 - y2;
			break;

		case 'S':
			pt = fz_currentpoint(path);
			x1 = fz_atof(args[i+0]);
			y1 = fz_atof(args[i+1]);
			x2 = fz_atof(args[i+2]);
			y2 = fz_atof(args[i+3]);
			fz_curveto(path, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
			i += 4;
			reset_smooth = 0;
			smooth_x = x2 - x1;
			smooth_y = y2 - y1;
			break;

		case 's':
			pt = fz_currentpoint(path);
			x1 = fz_atof(args[i+0]) + pt.x;
			y1 = fz_atof(args[i+1]) + pt.y;
			x2 = fz_atof(args[i+2]) + pt.x;
			y2 = fz_atof(args[i+3]) + pt.y;
			fz_curveto(path, pt.x + smooth_x, pt.y + smooth_y, x1, y1, x2, y2);
			i += 4;
			reset_smooth = 0;
			smooth_x = x2 - x1;
			smooth_y = y2 - y1;
			break;

		case 'Q':
			pt = fz_currentpoint(path);
			x1 = fz_atof(args[i+0]);
			y1 = fz_atof(args[i+1]);
			x2 = fz_atof(args[i+2]);
			y2 = fz_atof(args[i+3]);
			fz_curveto(path,
				(pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
				(x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
				x2, y2);
			i += 4;
			break;
		case 'q':
			pt = fz_currentpoint(path);
			x1 = fz_atof(args[i+0]) + pt.x;
			y1 = fz_atof(args[i+1]) + pt.y;
			x2 = fz_atof(args[i+2]) + pt.x;
			y2 = fz_atof(args[i+3]) + pt.y;
			fz_curveto(path,
				(pt.x + 2 * x1) / 3, (pt.y + 2 * y1) / 3,
				(x2 + 2 * x1) / 3, (y2 + 2 * y1) / 3,
				x2, y2);
			i += 4;
			break;

		case 'A':
			xps_draw_arc(path,
				fz_atof(args[i+0]), fz_atof(args[i+1]), fz_atof(args[i+2]),
				atoi(args[i+3]), atoi(args[i+4]),
				fz_atof(args[i+5]), fz_atof(args[i+6]));
			i += 7;
			break;
		case 'a':
			pt = fz_currentpoint(path);
			xps_draw_arc(path,
				fz_atof(args[i+0]), fz_atof(args[i+1]), fz_atof(args[i+2]),
				atoi(args[i+3]), atoi(args[i+4]),
				fz_atof(args[i+5]) + pt.x, fz_atof(args[i+6]) + pt.y);
			i += 7;
			break;

		case 'Z':
		case 'z':
			fz_closepath(path);
			break;

		default:
			/* eek */
			break;
		}

		old = cmd;
	}

	fz_free(args);
	return path;
}

static void
xps_parse_arc_segment(fz_path *path, xml_element *root, int stroking, int *skipped_stroke)
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

	char *point_att = xml_att(root, "Point");
	char *size_att = xml_att(root, "Size");
	char *rotation_angle_att = xml_att(root, "RotationAngle");
	char *is_large_arc_att = xml_att(root, "IsLargeArc");
	char *sweep_direction_att = xml_att(root, "SweepDirection");
	char *is_stroked_att = xml_att(root, "IsStroked");

	if (!point_att || !size_att || !rotation_angle_att || !is_large_arc_att || !sweep_direction_att)
	{
		fz_warn("ArcSegment element is missing attributes");
		return;
	}

	is_stroked = 1;
	if (is_stroked_att && !strcmp(is_stroked_att, "false"))
			is_stroked = 0;
	if (!is_stroked)
		*skipped_stroke = 1;

	sscanf(point_att, "%g,%g", &point_x, &point_y);
	sscanf(size_att, "%g,%g", &size_x, &size_y);
	rotation_angle = fz_atof(rotation_angle_att);
	is_large_arc = !strcmp(is_large_arc_att, "true");
	is_clockwise = !strcmp(sweep_direction_att, "Clockwise");

	if (stroking && !is_stroked)
	{
		fz_moveto(path, point_x, point_y);
		return;
	}

	xps_draw_arc(path, size_x, size_y, rotation_angle, is_large_arc, is_clockwise, point_x, point_y);
}

static void
xps_parse_poly_quadratic_bezier_segment(fz_path *path, xml_element *root, int stroking, int *skipped_stroke)
{
	char *points_att = xml_att(root, "Points");
	char *is_stroked_att = xml_att(root, "IsStroked");
	float x[2], y[2];
	int is_stroked;
	fz_point pt;
	char *s;
	int n;

	if (!points_att)
	{
		fz_warn("PolyQuadraticBezierSegment element has no points");
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
		sscanf(s, "%g,%g", &x[n], &y[n]);
		while (*s != ' ' && *s != 0) s++;
		n ++;
		if (n == 2)
		{
			if (stroking && !is_stroked)
			{
				fz_moveto(path, x[1], y[1]);
			}
			else
			{
				pt = fz_currentpoint(path);
				fz_curveto(path,
						(pt.x + 2 * x[0]) / 3, (pt.y + 2 * y[0]) / 3,
						(x[1] + 2 * x[0]) / 3, (y[1] + 2 * y[0]) / 3,
						x[1], y[1]);
			}
			n = 0;
		}
	}
}

static void
xps_parse_poly_bezier_segment(fz_path *path, xml_element *root, int stroking, int *skipped_stroke)
{
	char *points_att = xml_att(root, "Points");
	char *is_stroked_att = xml_att(root, "IsStroked");
	float x[3], y[3];
	int is_stroked;
	char *s;
	int n;

	if (!points_att)
	{
		fz_warn("PolyBezierSegment element has no points");
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
		sscanf(s, "%g,%g", &x[n], &y[n]);
		while (*s != ' ' && *s != 0) s++;
		n ++;
		if (n == 3)
		{
			if (stroking && !is_stroked)
				fz_moveto(path, x[2], y[2]);
			else
				fz_curveto(path, x[0], y[0], x[1], y[1], x[2], y[2]);
			n = 0;
		}
	}
}

static void
xps_parse_poly_line_segment(fz_path *path, xml_element *root, int stroking, int *skipped_stroke)
{
	char *points_att = xml_att(root, "Points");
	char *is_stroked_att = xml_att(root, "IsStroked");
	int is_stroked;
	float x, y;
	char *s;

	if (!points_att)
	{
		fz_warn("PolyLineSegment element has no points");
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
		sscanf(s, "%g,%g", &x, &y);
		if (stroking && !is_stroked)
			fz_moveto(path, x, y);
		else
			fz_lineto(path, x, y);
		while (*s != ' ' && *s != 0) s++;
	}
}

static void
xps_parse_path_figure(fz_path *path, xml_element *root, int stroking)
{
	xml_element *node;

	char *is_closed_att;
	char *start_point_att;
	char *is_filled_att;

	int is_closed = 0;
	int is_filled = 1;
	float start_x = 0;
	float start_y = 0;

	int skipped_stroke = 0;

	is_closed_att = xml_att(root, "IsClosed");
	start_point_att = xml_att(root, "StartPoint");
	is_filled_att = xml_att(root, "IsFilled");

	if (is_closed_att)
		is_closed = !strcmp(is_closed_att, "true");
	if (is_filled_att)
		is_filled = !strcmp(is_filled_att, "true");
	if (start_point_att)
		sscanf(start_point_att, "%g,%g", &start_x, &start_y);

	if (!stroking && !is_filled) /* not filled, when filling */
		return;

	fz_moveto(path, start_x, start_y);

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "ArcSegment"))
			xps_parse_arc_segment(path, node, stroking, &skipped_stroke);
		if (!strcmp(xml_tag(node), "PolyBezierSegment"))
			xps_parse_poly_bezier_segment(path, node, stroking, &skipped_stroke);
		if (!strcmp(xml_tag(node), "PolyLineSegment"))
			xps_parse_poly_line_segment(path, node, stroking, &skipped_stroke);
		if (!strcmp(xml_tag(node), "PolyQuadraticBezierSegment"))
			xps_parse_poly_quadratic_bezier_segment(path, node, stroking, &skipped_stroke);
	}

	if (is_closed)
	{
		if (stroking && skipped_stroke)
			fz_lineto(path, start_x, start_y); /* we've skipped using fz_moveto... */
		else
			fz_closepath(path); /* no skipped segments, safe to closepath properly */
	}
}

fz_path *
xps_parse_path_geometry(xps_context *ctx, xps_resource *dict, xml_element *root, int stroking, int *fill_rule)
{
	xml_element *node;

	char *figures_att;
	char *fill_rule_att;
	char *transform_att;

	xml_element *transform_tag = NULL;
	xml_element *figures_tag = NULL; /* only used by resource */

	fz_matrix transform;
	fz_path *path;

	figures_att = xml_att(root, "Figures");
	fill_rule_att = xml_att(root, "FillRule");
	transform_att = xml_att(root, "Transform");

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "PathGeometry.Transform"))
			transform_tag = xml_down(node);
	}

	xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag, NULL);
	xps_resolve_resource_reference(ctx, dict, &figures_att, &figures_tag, NULL);

	if (fill_rule_att)
	{
		if (!strcmp(fill_rule_att, "NonZero"))
			*fill_rule = 1;
		if (!strcmp(fill_rule_att, "EvenOdd"))
			*fill_rule = 0;
	}

	transform = fz_identity;
	if (transform_att)
		xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
		xps_parse_matrix_transform(ctx, transform_tag, &transform);

	if (figures_att)
		path = xps_parse_abbreviated_geometry(ctx, figures_att, fill_rule);
	else
		path = fz_new_path();

	if (figures_tag)
		xps_parse_path_figure(path, figures_tag, stroking);

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "PathFigure"))
			xps_parse_path_figure(path, node, stroking);
	}

	if (transform_att || transform_tag)
		fz_transform_path(path, transform);

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
xps_clip(xps_context *ctx, fz_matrix ctm, xps_resource *dict, char *clip_att, xml_element *clip_tag)
{
	fz_path *path;
	int fill_rule = 0;

	if (clip_att)
		path = xps_parse_abbreviated_geometry(ctx, clip_att, &fill_rule);
	else if (clip_tag)
		path = xps_parse_path_geometry(ctx, dict, clip_tag, 0, &fill_rule);
	else
		path = fz_new_path();
	fz_clip_path(ctx->dev, path, NULL, fill_rule == 0, ctm);
	fz_free_path(path);
}

/*
 * Parse an XPS <Path> element, and call relevant ghostscript
 * functions for drawing and/or clipping the child elements.
 */

void
xps_parse_path(xps_context *ctx, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *root)
{
	xml_element *node;

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

	xml_element *transform_tag = NULL;
	xml_element *clip_tag = NULL;
	xml_element *data_tag = NULL;
	xml_element *fill_tag = NULL;
	xml_element *stroke_tag = NULL;
	xml_element *opacity_mask_tag = NULL;

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

	fz_stroke_state stroke;
	fz_matrix transform;
	float samples[32];
	fz_colorspace *colorspace;
	fz_path *path;
	fz_rect area;
	int fill_rule;

	/*
	 * Extract attributes and extended attributes.
	 */

	transform_att = xml_att(root, "RenderTransform");
	clip_att = xml_att(root, "Clip");
	data_att = xml_att(root, "Data");
	fill_att = xml_att(root, "Fill");
	stroke_att = xml_att(root, "Stroke");
	opacity_att = xml_att(root, "Opacity");
	opacity_mask_att = xml_att(root, "OpacityMask");

	stroke_dash_array_att = xml_att(root, "StrokeDashArray");
	stroke_dash_cap_att = xml_att(root, "StrokeDashCap");
	stroke_dash_offset_att = xml_att(root, "StrokeDashOffset");
	stroke_end_line_cap_att = xml_att(root, "StrokeEndLineCap");
	stroke_start_line_cap_att = xml_att(root, "StrokeStartLineCap");
	stroke_line_join_att = xml_att(root, "StrokeLineJoin");
	stroke_miter_limit_att = xml_att(root, "StrokeMiterLimit");
	stroke_thickness_att = xml_att(root, "StrokeThickness");

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "Path.RenderTransform"))
			transform_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Path.OpacityMask"))
			opacity_mask_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Path.Clip"))
			clip_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Path.Fill"))
			fill_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Path.Stroke"))
			stroke_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Path.Data"))
			data_tag = xml_down(node);
	}

	fill_uri = base_uri;
	stroke_uri = base_uri;
	opacity_mask_uri = base_uri;

	xps_resolve_resource_reference(ctx, dict, &data_att, &data_tag, NULL);
	xps_resolve_resource_reference(ctx, dict, &clip_att, &clip_tag, NULL);
	xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag, NULL);
	xps_resolve_resource_reference(ctx, dict, &fill_att, &fill_tag, &fill_uri);
	xps_resolve_resource_reference(ctx, dict, &stroke_att, &stroke_tag, &stroke_uri);
	xps_resolve_resource_reference(ctx, dict, &opacity_mask_att, &opacity_mask_tag, &opacity_mask_uri);

	/*
	 * Act on the information we have gathered:
	 */

	if (!data_att && !data_tag)
		return;

	if (fill_tag && !strcmp(xml_tag(fill_tag), "SolidColorBrush"))
	{
		fill_opacity_att = xml_att(fill_tag, "Opacity");
		fill_att = xml_att(fill_tag, "Color");
		fill_tag = NULL;
	}

	if (stroke_tag && !strcmp(xml_tag(stroke_tag), "SolidColorBrush"))
	{
		stroke_opacity_att = xml_att(stroke_tag, "Opacity");
		stroke_att = xml_att(stroke_tag, "Color");
		stroke_tag = NULL;
	}

	stroke.start_cap = xps_parse_line_cap(stroke_start_line_cap_att);
	stroke.dash_cap = xps_parse_line_cap(stroke_dash_cap_att);
	stroke.end_cap = xps_parse_line_cap(stroke_end_line_cap_att);

	stroke.linejoin = 0;
	if (stroke_line_join_att)
	{
		if (!strcmp(stroke_line_join_att, "Miter")) stroke.linejoin = 0;
		if (!strcmp(stroke_line_join_att, "Round")) stroke.linejoin = 1;
		if (!strcmp(stroke_line_join_att, "Bevel")) stroke.linejoin = 2;
	}

	stroke.miterlimit = 10;
	if (stroke_miter_limit_att)
		stroke.miterlimit = fz_atof(stroke_miter_limit_att);

	stroke.linewidth = 1;
	if (stroke_thickness_att)
		stroke.linewidth = fz_atof(stroke_thickness_att);

	stroke.dash_phase = 0;
	stroke.dash_len = 0;
	if (stroke_dash_array_att)
	{
		char *s = stroke_dash_array_att;

		if (stroke_dash_offset_att)
			stroke.dash_phase = fz_atof(stroke_dash_offset_att) * stroke.linewidth;

		while (*s && stroke.dash_len < nelem(stroke.dash_list))
		{
			while (*s == ' ')
				s++;
			stroke.dash_list[stroke.dash_len++] = fz_atof(s) * stroke.linewidth;
			while (*s && *s != ' ')
				s++;
		}
	}

	transform = fz_identity;
	if (transform_att)
		xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
		xps_parse_matrix_transform(ctx, transform_tag, &transform);
	ctm = fz_concat(transform, ctm);

	if (clip_att || clip_tag)
		xps_clip(ctx, ctm, dict, clip_att, clip_tag);

	fill_rule = 0;
	if (data_att)
		path = xps_parse_abbreviated_geometry(ctx, data_att, &fill_rule);
	else if (data_tag)
		path = xps_parse_path_geometry(ctx, dict, data_tag, 0, &fill_rule);

	if (stroke_att || stroke_tag)
		area = fz_bound_path(path, &stroke, ctm);
	else
		area = fz_bound_path(path, NULL, ctm);

	/* SumatraPDF: support links and outlines */
	xps_extract_link_info(ctx, root, area, base_uri);

	xps_begin_opacity(ctx, ctm, area, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

	if (fill_att)
	{
		xps_parse_color(ctx, base_uri, fill_att, &colorspace, samples);
		if (fill_opacity_att)
			samples[0] = fz_atof(fill_opacity_att);
		xps_set_color(ctx, colorspace, samples);

		fz_fill_path(ctx->dev, path, fill_rule == 0, ctm,
			ctx->colorspace, ctx->color, ctx->alpha);
	}

	if (fill_tag)
	{
		area = fz_bound_path(path, NULL, ctm);

		fz_clip_path(ctx->dev, path, NULL, fill_rule == 0, ctm);
		xps_parse_brush(ctx, ctm, area, fill_uri, dict, fill_tag);
		fz_pop_clip(ctx->dev);
	}

	if (stroke_att)
	{
		xps_parse_color(ctx, base_uri, stroke_att, &colorspace, samples);
		if (stroke_opacity_att)
			samples[0] = fz_atof(stroke_opacity_att);
		xps_set_color(ctx, colorspace, samples);

		fz_stroke_path(ctx->dev, path, &stroke, ctm,
			ctx->colorspace, ctx->color, ctx->alpha);
	}

	if (stroke_tag)
	{
		fz_clip_stroke_path(ctx->dev, path, NULL, &stroke, ctm);
		xps_parse_brush(ctx, ctm, area, stroke_uri, dict, stroke_tag);
		fz_pop_clip(ctx->dev);
	}

	xps_end_opacity(ctx, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

	fz_free_path(path);
	path = NULL;

	if (clip_att || clip_tag)
		fz_pop_clip(ctx->dev);
}
