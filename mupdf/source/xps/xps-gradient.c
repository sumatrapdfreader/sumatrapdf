#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <string.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>

#define MAX_STOPS 256

enum { SPREAD_PAD, SPREAD_REPEAT, SPREAD_REFLECT };

/*
 * Parse a list of GradientStop elements.
 * Fill the offset and color arrays, and
 * return the number of stops parsed.
 */

struct stop
{
	float offset;
	float r, g, b, a;
	int index;
};

static int cmp_stop(const void *a, const void *b)
{
	const struct stop *astop = a;
	const struct stop *bstop = b;
	float diff = astop->offset - bstop->offset;
	if (diff < 0)
		return -1;
	if (diff > 0)
		return 1;
	return astop->index - bstop->index;
}

static inline float lerp(float a, float b, float x)
{
	return a + (b - a) * x;
}

static int
xps_parse_gradient_stops(fz_context *ctx, xps_document *doc, char *base_uri, fz_xml *node,
	struct stop *stops, int maxcount)
{
	fz_colorspace *colorspace;
	float sample[FZ_MAX_COLORS];
	float rgb[3];
	int before, after;
	int count;
	int i;

	/* We may have to insert 2 extra stops when postprocessing */
	maxcount -= 2;

	count = 0;
	while (node && count < maxcount)
	{
		if (fz_xml_is_tag(node, "GradientStop"))
		{
			char *offset = fz_xml_att(node, "Offset");
			char *color = fz_xml_att(node, "Color");
			if (offset && color)
			{
				stops[count].offset = fz_atof(offset);
				stops[count].index = count;

				xps_parse_color(ctx, doc, base_uri, color, &colorspace, sample);

				fz_convert_color(ctx, colorspace, sample+1, fz_device_rgb(ctx), rgb, NULL, fz_default_color_params);

				stops[count].r = rgb[0];
				stops[count].g = rgb[1];
				stops[count].b = rgb[2];
				stops[count].a = sample[0];

				count ++;
			}
		}
		node = fz_xml_next(node);
	}

	if (count == 0)
	{
		fz_warn(ctx, "gradient brush has no gradient stops");
		stops[0].offset = 0;
		stops[0].r = 0;
		stops[0].g = 0;
		stops[0].b = 0;
		stops[0].a = 1;
		stops[1].offset = 1;
		stops[1].r = 1;
		stops[1].g = 1;
		stops[1].b = 1;
		stops[1].a = 1;
		return 2;
	}

	if (count == maxcount)
		fz_warn(ctx, "gradient brush exceeded maximum number of gradient stops");

	/* Postprocess to make sure the range of offsets is 0.0 to 1.0 */

	qsort(stops, count, sizeof(struct stop), cmp_stop);

	before = -1;
	after = -1;

	for (i = 0; i < count; i++)
	{
		if (stops[i].offset < 0)
			before = i;
		if (stops[i].offset > 1)
		{
			after = i;
			break;
		}
	}

	/* Remove all stops < 0 except the largest one */
	if (before > 0)
	{
		memmove(stops, stops + before, (count - before) * sizeof(struct stop));
		count -= before;
	}

	/* Remove all stops > 1 except the smallest one */
	if (after >= 0)
		count = after + 1;

	/* Expand single stop to 0 .. 1 */
	if (count == 1)
	{
		stops[1] = stops[0];
		stops[0].offset = 0;
		stops[1].offset = 1;
		return 2;
	}

	/* First stop < 0 -- interpolate value to 0 */
	if (stops[0].offset < 0)
	{
		float d = -stops[0].offset / (stops[1].offset - stops[0].offset);
		stops[0].offset = 0;
		stops[0].r = lerp(stops[0].r, stops[1].r, d);
		stops[0].g = lerp(stops[0].g, stops[1].g, d);
		stops[0].b = lerp(stops[0].b, stops[1].b, d);
		stops[0].a = lerp(stops[0].a, stops[1].a, d);
	}

	/* Last stop > 1 -- interpolate value to 1 */
	if (stops[count-1].offset > 1)
	{
		float d = (1 - stops[count-2].offset) / (stops[count-1].offset - stops[count-2].offset);
		stops[count-1].offset = 1;
		stops[count-1].r = lerp(stops[count-2].r, stops[count-1].r, d);
		stops[count-1].g = lerp(stops[count-2].g, stops[count-1].g, d);
		stops[count-1].b = lerp(stops[count-2].b, stops[count-1].b, d);
		stops[count-1].a = lerp(stops[count-2].a, stops[count-1].a, d);
	}

	/* First stop > 0 -- insert a duplicate at 0 */
	if (stops[0].offset > 0)
	{
		memmove(stops + 1, stops, count * sizeof(struct stop));
		stops[0] = stops[1];
		stops[0].offset = 0;
		count++;
	}

	/* Last stop < 1 -- insert a duplicate at 1 */
	if (stops[count-1].offset < 1)
	{
		stops[count] = stops[count-1];
		stops[count].offset = 1;
		count++;
	}

	return count;
}

static void
xps_sample_gradient_stops(fz_context *ctx, xps_document *doc, fz_shade *shade, struct stop *stops, int count)
{
	float offset, d;
	int i, k;

	k = 0;
	for (i = 0; i < 256; i++)
	{
		offset = i / 255.0f;
		while (k + 1 < count && offset > stops[k+1].offset)
			k++;

		d = (offset - stops[k].offset) / (stops[k+1].offset - stops[k].offset);

		shade->function[i][0] = lerp(stops[k].r, stops[k+1].r, d);
		shade->function[i][1] = lerp(stops[k].g, stops[k+1].g, d);
		shade->function[i][2] = lerp(stops[k].b, stops[k+1].b, d);
		shade->function[i][3] = lerp(stops[k].a, stops[k+1].a, d);
	}
}

/*
 * Radial gradients map more or less to Radial shadings.
 * The inner circle is always a point.
 * The outer circle is actually an ellipse,
 * mess with the transform to squash the circle into the right aspect.
 */

static void
xps_draw_one_radial_gradient(fz_context *ctx, xps_document *doc, fz_matrix ctm,
	struct stop *stops, int count,
	int extend,
	float x0, float y0, float r0,
	float x1, float y1, float r1)
{
	fz_device *dev = doc->dev;
	fz_shade *shade;

	shade = fz_malloc_struct(ctx, fz_shade);
	FZ_INIT_STORABLE(shade, 1, fz_drop_shade_imp);
	shade->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	shade->bbox = fz_infinite_rect;
	shade->matrix = fz_identity;
	shade->use_background = 0;
	shade->use_function = 1;
	shade->type = FZ_RADIAL;
	shade->u.l_or_r.extend[0] = extend;
	shade->u.l_or_r.extend[1] = extend;

	shade->u.l_or_r.coords[0][0] = x0;
	shade->u.l_or_r.coords[0][1] = y0;
	shade->u.l_or_r.coords[0][2] = r0;
	shade->u.l_or_r.coords[1][0] = x1;
	shade->u.l_or_r.coords[1][1] = y1;
	shade->u.l_or_r.coords[1][2] = r1;

	fz_try(ctx)
	{
		xps_sample_gradient_stops(ctx, doc, shade, stops, count);
		fz_fill_shade(ctx, dev, shade, ctm, 1, fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_shade(ctx, shade);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
 * Linear gradients.
 */

static void
xps_draw_one_linear_gradient(fz_context *ctx, xps_document *doc, fz_matrix ctm,
	struct stop *stops, int count,
	int extend,
	float x0, float y0, float x1, float y1)
{
	fz_device *dev = doc->dev;
	fz_shade *shade;

	shade = fz_malloc_struct(ctx, fz_shade);
	FZ_INIT_STORABLE(shade, 1, fz_drop_shade_imp);
	shade->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	shade->bbox = fz_infinite_rect;
	shade->matrix = fz_identity;
	shade->use_background = 0;
	shade->use_function = 1;
	shade->type = FZ_LINEAR;
	shade->u.l_or_r.extend[0] = extend;
	shade->u.l_or_r.extend[1] = extend;

	shade->u.l_or_r.coords[0][0] = x0;
	shade->u.l_or_r.coords[0][1] = y0;
	shade->u.l_or_r.coords[0][2] = 0;
	shade->u.l_or_r.coords[1][0] = x1;
	shade->u.l_or_r.coords[1][1] = y1;
	shade->u.l_or_r.coords[1][2] = 0;

	fz_try(ctx)
	{
		xps_sample_gradient_stops(ctx, doc, shade, stops, count);
		fz_fill_shade(ctx, dev, shade, ctm, doc->opacity[doc->opacity_top], fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_shade(ctx, shade);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/*
 * We need to loop and create many shading objects to account
 * for the Repeat and Reflect SpreadMethods.
 * I'm not smart enough to calculate this analytically
 * so we iterate and check each object until we
 * reach a reasonable limit for infinite cases.
 */

static void
xps_draw_radial_gradient(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	struct stop *stops, int count,
	fz_xml *root, int spread)
{
	float x0, y0, r0;
	float x1, y1, r1;
	float xrad = 1;
	float yrad = 1;
	float invscale;
	int i, ma = 1;
	fz_matrix inv;

	char *center_att = fz_xml_att(root, "Center");
	char *origin_att = fz_xml_att(root, "GradientOrigin");
	char *radius_x_att = fz_xml_att(root, "RadiusX");
	char *radius_y_att = fz_xml_att(root, "RadiusY");

	x0 = y0 = 0.0f;
	x1 = y1 = 1.0f;
	xrad = 1.0f;
	yrad = 1.0f;

	if (origin_att)
		xps_parse_point(ctx, doc, origin_att, &x0, &y0);
	if (center_att)
		xps_parse_point(ctx, doc, center_att, &x1, &y1);
	if (radius_x_att)
		xrad = fz_atof(radius_x_att);
	if (radius_y_att)
		yrad = fz_atof(radius_y_att);

	xrad = fz_max(0.01f, xrad);
	yrad = fz_max(0.01f, yrad);

	/* scale the ctm to make ellipses */
	if (fz_abs(xrad) > FLT_EPSILON)
	{
		ctm = fz_pre_scale(ctm, 1, yrad/xrad);
	}

	if (yrad != 0.0f)
	{
		invscale = xrad / yrad;
		y0 = y0 * invscale;
		y1 = y1 * invscale;
	}

	r0 = 0;
	r1 = xrad;

	inv = fz_invert_matrix(ctm);
	area = fz_transform_rect(area, inv);
	ma = fz_maxi(ma, ceilf(hypotf(area.x0 - x0, area.y0 - y0) / xrad));
	ma = fz_maxi(ma, ceilf(hypotf(area.x1 - x0, area.y0 - y0) / xrad));
	ma = fz_maxi(ma, ceilf(hypotf(area.x0 - x0, area.y1 - y0) / xrad));
	ma = fz_maxi(ma, ceilf(hypotf(area.x1 - x0, area.y1 - y0) / xrad));

	if (spread == SPREAD_REPEAT)
	{
		for (i = ma - 1; i >= 0; i--)
			xps_draw_one_radial_gradient(ctx, doc, ctm, stops, count, 0, x0, y0, r0 + i * xrad, x1, y1, r1 + i * xrad);
	}
	else if (spread == SPREAD_REFLECT)
	{
		if ((ma % 2) != 0)
			ma++;
		for (i = ma - 2; i >= 0; i -= 2)
		{
			xps_draw_one_radial_gradient(ctx, doc, ctm, stops, count, 0, x0, y0, r0 + i * xrad, x1, y1, r1 + i * xrad);
			xps_draw_one_radial_gradient(ctx, doc, ctm, stops, count, 0, x0, y0, r0 + (i + 2) * xrad, x1, y1, r1 + i * xrad);
		}
	}
	else
	{
		xps_draw_one_radial_gradient(ctx, doc, ctm, stops, count, 1, x0, y0, r0, x1, y1, r1);
	}
}

/*
 * Calculate how many iterations are needed to cover
 * the bounding box.
 */

static void
xps_draw_linear_gradient(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	struct stop *stops, int count,
	fz_xml *root, int spread)
{
	float x0, y0, x1, y1;
	int i, mi, ma;
	float dx, dy, x, y, k;
	fz_point p1, p2;
	fz_matrix inv;

	char *start_point_att = fz_xml_att(root, "StartPoint");
	char *end_point_att = fz_xml_att(root, "EndPoint");

	x0 = y0 = 0;
	x1 = y1 = 1;

	if (start_point_att)
		xps_parse_point(ctx, doc, start_point_att, &x0, &y0);
	if (end_point_att)
		xps_parse_point(ctx, doc, end_point_att, &x1, &y1);

	p1.x = x0; p1.y = y0; p2.x = x1; p2.y = y1;
	inv = fz_invert_matrix(ctm);
	area = fz_transform_rect(area, inv);
	x = p2.x - p1.x; y = p2.y - p1.y;
	k = ((area.x0 - p1.x) * x + (area.y0 - p1.y) * y) / (x * x + y * y);
	mi = floorf(k); ma = ceilf(k);
	k = ((area.x1 - p1.x) * x + (area.y0 - p1.y) * y) / (x * x + y * y);
	mi = fz_mini(mi, floorf(k)); ma = fz_maxi(ma, ceilf(k));
	k = ((area.x0 - p1.x) * x + (area.y1 - p1.y) * y) / (x * x + y * y);
	mi = fz_mini(mi, floorf(k)); ma = fz_maxi(ma, ceilf(k));
	k = ((area.x1 - p1.x) * x + (area.y1 - p1.y) * y) / (x * x + y * y);
	mi = fz_mini(mi, floorf(k)); ma = fz_maxi(ma, ceilf(k));
	dx = x1 - x0; dy = y1 - y0;

	if (spread == SPREAD_REPEAT)
	{
		for (i = mi; i < ma; i++)
			xps_draw_one_linear_gradient(ctx, doc, ctm, stops, count, 0, x0 + i * dx, y0 + i * dy, x1 + i * dx, y1 + i * dy);
	}
	else if (spread == SPREAD_REFLECT)
	{
		if ((mi % 2) != 0)
			mi--;
		for (i = mi; i < ma; i += 2)
		{
			xps_draw_one_linear_gradient(ctx, doc, ctm, stops, count, 0, x0 + i * dx, y0 + i * dy, x1 + i * dx, y1 + i * dy);
			xps_draw_one_linear_gradient(ctx, doc, ctm, stops, count, 0, x0 + (i + 2) * dx, y0 + (i + 2) * dy, x1 + i * dx, y1 + i * dy);
		}
	}
	else
	{
		xps_draw_one_linear_gradient(ctx, doc, ctm, stops, count, 1, x0, y0, x1, y1);
	}
}

/*
 * Parse XML tag and attributes for a gradient brush, create color/opacity
 * function objects and call gradient drawing primitives.
 */

static void
xps_parse_gradient_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, fz_xml *root,
	void (*draw)(fz_context *ctx, xps_document *, fz_matrix, fz_rect, struct stop *, int, fz_xml *, int))
{
	fz_xml *node;

	char *opacity_att;
	char *spread_att;
	char *transform_att;

	fz_xml *transform_tag = NULL;
	fz_xml *stop_tag = NULL;

	struct stop stop_list[MAX_STOPS];
	int stop_count;
	int spread_method;

	opacity_att = fz_xml_att(root, "Opacity");
	spread_att = fz_xml_att(root, "SpreadMethod");
	transform_att = fz_xml_att(root, "Transform");

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "LinearGradientBrush.Transform"))
			transform_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "RadialGradientBrush.Transform"))
			transform_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "LinearGradientBrush.GradientStops"))
			stop_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "RadialGradientBrush.GradientStops"))
			stop_tag = fz_xml_down(node);
	}

	xps_resolve_resource_reference(ctx, doc, dict, &transform_att, &transform_tag, NULL);

	spread_method = SPREAD_PAD;
	if (spread_att)
	{
		if (!strcmp(spread_att, "Pad"))
			spread_method = SPREAD_PAD;
		if (!strcmp(spread_att, "Reflect"))
			spread_method = SPREAD_REFLECT;
		if (!strcmp(spread_att, "Repeat"))
			spread_method = SPREAD_REPEAT;
	}

	ctm = xps_parse_transform(ctx, doc, transform_att, transform_tag, ctm);

	if (!stop_tag) {
		fz_warn(ctx, "missing gradient stops tag");
		return;
	}

	stop_count = xps_parse_gradient_stops(ctx, doc, base_uri, stop_tag, stop_list, MAX_STOPS);
	if (stop_count == 0)
	{
		fz_warn(ctx, "no gradient stops found");
		return;
	}

	xps_begin_opacity(ctx, doc, ctm, area, base_uri, dict, opacity_att, NULL);

	draw(ctx, doc, ctm, area, stop_list, stop_count, root, spread_method);

	xps_end_opacity(ctx, doc, base_uri, dict, opacity_att, NULL);
}

void
xps_parse_linear_gradient_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, fz_xml *root)
{
	xps_parse_gradient_brush(ctx, doc, ctm, area, base_uri, dict, root, xps_draw_linear_gradient);
}

void
xps_parse_radial_gradient_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, fz_xml *root)
{
	xps_parse_gradient_brush(ctx, doc, ctm, area, base_uri, dict, root, xps_draw_radial_gradient);
}
