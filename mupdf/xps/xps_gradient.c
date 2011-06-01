#include "fitz.h"
#include "muxps.h"

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
	return 0;
}

static inline float lerp(float a, float b, float x)
{
	return a + (b - a) * x;
}

static int
xps_parse_gradient_stops(xps_context *ctx, char *base_uri, xml_element *node,
	struct stop *stops, int maxcount)
{
	fz_colorspace *colorspace;
	float sample[8];
	float rgb[3];
	int before, after;
	int count;
	int i;

	/* We may have to insert 2 extra stops when postprocessing */
	maxcount -= 2;

	count = 0;
	while (node && count < maxcount)
	{
		if (!strcmp(xml_tag(node), "GradientStop"))
		{
			char *offset = xml_att(node, "Offset");
			char *color = xml_att(node, "Color");
			if (offset && color)
			{
				stops[count].offset = fz_atof(offset);

				xps_parse_color(ctx, base_uri, color, &colorspace, sample);

				fz_convert_color(colorspace, sample + 1, fz_device_rgb, rgb);

				stops[count].r = rgb[0];
				stops[count].g = rgb[1];
				stops[count].b = rgb[2];
				stops[count].a = sample[0];

				count ++;
			}
		}
		node = xml_next(node);
	}

	if (count == 0)
	{
		fz_warn("gradient brush has no gradient stops");
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
		fz_warn("gradient brush exceeded maximum number of gradient stops");

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
		stops[0].r = lerp(stops[count-2].r, stops[count-1].r, d);
		stops[0].g = lerp(stops[count-2].g, stops[count-1].g, d);
		stops[0].b = lerp(stops[count-2].b, stops[count-1].b, d);
		stops[0].a = lerp(stops[count-2].a, stops[count-1].a, d);
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
xps_sample_gradient_stops(fz_shade *shade, struct stop *stops, int count)
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
xps_draw_one_radial_gradient(xps_context *ctx, fz_matrix ctm,
	struct stop *stops, int count,
	int extend,
	float x0, float y0, float r0,
	float x1, float y1, float r1)
{
	fz_shade *shade;

	/* TODO: this (and the stuff in pdf_shade) should move to res_shade.c */
	shade = fz_malloc(sizeof(fz_shade));
	shade->refs = 1;
	shade->colorspace = fz_device_rgb;
	shade->bbox = fz_infinite_rect;
	shade->matrix = fz_identity;
	shade->use_background = 0;
	shade->use_function = 1;
	shade->type = FZ_RADIAL;
	shade->extend[0] = extend;
	shade->extend[1] = extend;

	xps_sample_gradient_stops(shade, stops, count);

	shade->mesh_len = 6;
	shade->mesh_cap = 6;
	shade->mesh = fz_calloc(shade->mesh_cap, sizeof(float));
	shade->mesh[0] = x0;
	shade->mesh[1] = y0;
	shade->mesh[2] = r0;
	shade->mesh[3] = x1;
	shade->mesh[4] = y1;
	shade->mesh[5] = r1;

	fz_fill_shade(ctx->dev, shade, ctm, 1);

	fz_drop_shade(shade);
}

/*
 * Linear gradients map to Axial shadings.
 */

static void
xps_draw_one_linear_gradient(xps_context *ctx, fz_matrix ctm,
	struct stop *stops, int count,
	int extend,
	float x0, float y0, float x1, float y1)
{
	fz_shade *shade;

	/* TODO: this (and the stuff in pdf_shade) should move to res_shade.c */
	shade = fz_malloc(sizeof(fz_shade));
	shade->refs = 1;
	shade->colorspace = fz_device_rgb;
	shade->bbox = fz_infinite_rect;
	shade->matrix = fz_identity;
	shade->use_background = 0;
	shade->use_function = 1;
	shade->type = FZ_LINEAR;
	shade->extend[0] = extend;
	shade->extend[1] = extend;

	xps_sample_gradient_stops(shade, stops, count);

	shade->mesh_len = 6;
	shade->mesh_cap = 6;
	shade->mesh = fz_calloc(shade->mesh_cap, sizeof(float));
	shade->mesh[0] = x0;
	shade->mesh[1] = y0;
	shade->mesh[2] = 0;
	shade->mesh[3] = x1;
	shade->mesh[4] = y1;
	shade->mesh[5] = 0;

	fz_fill_shade(ctx->dev, shade, ctm, 1);

	fz_drop_shade(shade);
}

/*
 * We need to loop and create many shading objects to account
 * for the Repeat and Reflect SpreadMethods.
 * I'm not smart enough to calculate this analytically
 * so we iterate and check each object until we
 * reach a reasonable limit for infinite cases.
 */

static inline float point_inside_circle(float px, float py, float x, float y, float r)
{
	float dx = px - x;
	float dy = py - y;
	return dx * dx + dy * dy <= r * r;
}

static void
xps_draw_radial_gradient(xps_context *ctx, fz_matrix ctm,
	struct stop *stops, int count,
	xml_element *root, int spread)
{
	float x0, y0, r0;
	float x1, y1, r1;
	float xrad = 1;
	float yrad = 1;
	float invscale;

	char *center_att = xml_att(root, "Center");
	char *origin_att = xml_att(root, "GradientOrigin");
	char *radius_x_att = xml_att(root, "RadiusX");
	char *radius_y_att = xml_att(root, "RadiusY");

	if (origin_att)
		sscanf(origin_att, "%g,%g", &x0, &y0);
	if (center_att)
		sscanf(center_att, "%g,%g", &x1, &y1);
	if (radius_x_att)
		xrad = fz_atof(radius_x_att);
	if (radius_y_att)
		yrad = fz_atof(radius_y_att);

	/* scale the ctm to make ellipses */
	ctm = fz_concat(fz_scale(1, yrad / xrad), ctm);

	invscale = xrad / yrad;
	y0 = y0 * invscale;
	y1 = y1 * invscale;

	r0 = 0;
	r1 = xrad;

	xps_draw_one_radial_gradient(ctx, ctm, stops, count, 1, x0, y0, r0, x1, y1, r1);
}

/*
 * Calculate how many iterations are needed to cover
 * the bounding box.
 */

static void
xps_draw_linear_gradient(xps_context *ctx, fz_matrix ctm,
	struct stop *stops, int count,
	xml_element *root, int spread)
{
	float x0, y0, x1, y1;

	char *start_point_att = xml_att(root, "StartPoint");
	char *end_point_att = xml_att(root, "EndPoint");

	x0 = y0 = 0;
	x1 = y1 = 1;

	if (start_point_att)
		sscanf(start_point_att, "%g,%g", &x0, &y0);
	if (end_point_att)
		sscanf(end_point_att, "%g,%g", &x1, &y1);

	xps_draw_one_linear_gradient(ctx, ctm, stops, count, 1, x0, y0, x1, y1);
}

/*
 * Parse XML tag and attributes for a gradient brush, create color/opacity
 * function objects and call gradient drawing primitives.
 */

static void
xps_parse_gradient_brush(xps_context *ctx, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root,
	void (*draw)(xps_context *, fz_matrix, struct stop *, int, xml_element *, int))
{
	xml_element *node;

	char *opacity_att;
	char *interpolation_att;
	char *spread_att;
	char *mapping_att;
	char *transform_att;

	xml_element *transform_tag = NULL;
	xml_element *stop_tag = NULL;

	struct stop stop_list[MAX_STOPS];
	int stop_count;
	fz_matrix transform;
	int spread_method;

	opacity_att = xml_att(root, "Opacity");
	interpolation_att = xml_att(root, "ColorInterpolationMode");
	spread_att = xml_att(root, "SpreadMethod");
	mapping_att = xml_att(root, "MappingMode");
	transform_att = xml_att(root, "Transform");

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "LinearGradientBrush.Transform"))
			transform_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "RadialGradientBrush.Transform"))
			transform_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "LinearGradientBrush.GradientStops"))
			stop_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "RadialGradientBrush.GradientStops"))
			stop_tag = xml_down(node);
	}

	xps_resolve_resource_reference(ctx, dict, &transform_att, &transform_tag, NULL);

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

	transform = fz_identity;
	if (transform_att)
		xps_parse_render_transform(ctx, transform_att, &transform);
	if (transform_tag)
		xps_parse_matrix_transform(ctx, transform_tag, &transform);
	ctm = fz_concat(transform, ctm);

	if (!stop_tag) {
		fz_warn("missing gradient stops tag");
		return;
	}

	stop_count = xps_parse_gradient_stops(ctx, base_uri, stop_tag, stop_list, MAX_STOPS);
	if (stop_count == 0)
	{
		fz_warn("no gradient stops found");
		return;
	}

	xps_begin_opacity(ctx, ctm, area, base_uri, dict, opacity_att, NULL);

	draw(ctx, ctm, stop_list, stop_count, root, spread_method);

	xps_end_opacity(ctx, base_uri, dict, opacity_att, NULL);
}

void
xps_parse_linear_gradient_brush(xps_context *ctx, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root)
{
	xps_parse_gradient_brush(ctx, ctm, area, base_uri, dict, root, xps_draw_linear_gradient);
}

void
xps_parse_radial_gradient_brush(xps_context *ctx, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root)
{
	xps_parse_gradient_brush(ctx, ctm, area, base_uri, dict, root, xps_draw_radial_gradient);
}
