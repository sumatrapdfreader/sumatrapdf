#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <string.h>
#include <stdio.h> /* for sscanf */
#include <math.h> /* for pow */

static inline int unhex(int a)
{
	if (a >= 'A' && a <= 'F') return a - 'A' + 0xA;
	if (a >= 'a' && a <= 'f') return a - 'a' + 0xA;
	if (a >= '0' && a <= '9') return a - '0';
	return 0;
}

fz_xml *
xps_lookup_alternate_content(fz_context *ctx, xps_document *doc, fz_xml *node)
{
	for (node = fz_xml_down(node); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "Choice") && fz_xml_att(node, "Requires"))
		{
			char list[64];
			char *next = list, *item;
			fz_strlcpy(list, fz_xml_att(node, "Requires"), sizeof(list));
			while ((item = fz_strsep(&next, " \t\r\n")) != NULL && (!*item || !strcmp(item, "xps")));
			if (!item)
				return fz_xml_down(node);
		}
		else if (fz_xml_is_tag(node, "Fallback"))
			return fz_xml_down(node);
	}
	return NULL;
}

void
xps_parse_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node)
{
	if (doc->cookie && doc->cookie->abort)
		return;
	/* SolidColorBrushes are handled in a special case and will never show up here */
	if (fz_xml_is_tag(node, "ImageBrush"))
		xps_parse_image_brush(ctx, doc, ctm, area, base_uri, dict, node);
	else if (fz_xml_is_tag(node, "VisualBrush"))
		xps_parse_visual_brush(ctx, doc, ctm, area, base_uri, dict, node);
	else if (fz_xml_is_tag(node, "LinearGradientBrush"))
		xps_parse_linear_gradient_brush(ctx, doc, ctm, area, base_uri, dict, node);
	else if (fz_xml_is_tag(node, "RadialGradientBrush"))
		xps_parse_radial_gradient_brush(ctx, doc, ctm, area, base_uri, dict, node);
	else
		fz_warn(ctx, "unknown brush tag");
}

void
xps_parse_element(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node)
{
	if (doc->cookie && doc->cookie->abort)
		return;
	if (fz_xml_is_tag(node, "Path"))
		xps_parse_path(ctx, doc, ctm, base_uri, dict, node);
	if (fz_xml_is_tag(node, "Glyphs"))
		xps_parse_glyphs(ctx, doc, ctm, base_uri, dict, node);
	if (fz_xml_is_tag(node, "Canvas"))
		xps_parse_canvas(ctx, doc, ctm, area, base_uri, dict, node);
	if (fz_xml_is_tag(node, "AlternateContent"))
	{
		node = xps_lookup_alternate_content(ctx, doc, node);
		if (node)
			xps_parse_element(ctx, doc, ctm, area, base_uri, dict, node);
	}
	/* skip unknown tags (like Foo.Resources and similar) */
}

void
xps_begin_opacity(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict,
	char *opacity_att, fz_xml *opacity_mask_tag)
{
	fz_device *dev = doc->dev;
	float opacity;

	if (!opacity_att && !opacity_mask_tag)
		return;

	opacity = 1;
	if (opacity_att)
		opacity = fz_atof(opacity_att);

	if (fz_xml_is_tag(opacity_mask_tag, "SolidColorBrush"))
	{
		char *scb_opacity_att = fz_xml_att(opacity_mask_tag, "Opacity");
		char *scb_color_att = fz_xml_att(opacity_mask_tag, "Color");
		if (scb_opacity_att)
			opacity = opacity * fz_atof(scb_opacity_att);
		if (scb_color_att)
		{
			fz_colorspace *colorspace;
			float samples[FZ_MAX_COLORS];
			xps_parse_color(ctx, doc, base_uri, scb_color_att, &colorspace, samples);
			opacity = opacity * samples[0];
		}
		opacity_mask_tag = NULL;
	}

	if (doc->opacity_top + 1 < (int)nelem(doc->opacity))
	{
		doc->opacity[doc->opacity_top + 1] = doc->opacity[doc->opacity_top] * opacity;
		doc->opacity_top++;
	}

	if (opacity_mask_tag)
	{
		fz_begin_mask(ctx, dev, area, 0, NULL, NULL, fz_default_color_params);
		xps_parse_brush(ctx, doc, ctm, area, base_uri, dict, opacity_mask_tag);
		fz_end_mask(ctx, dev);
	}
}

void
xps_end_opacity(fz_context *ctx, xps_document *doc, char *base_uri, xps_resource *dict,
	char *opacity_att, fz_xml *opacity_mask_tag)
{
	fz_device *dev = doc->dev;

	if (!opacity_att && !opacity_mask_tag)
		return;

	if (doc->opacity_top > 0)
		doc->opacity_top--;

	if (opacity_mask_tag)
	{
		if (!fz_xml_is_tag(opacity_mask_tag, "SolidColorBrush"))
			fz_pop_clip(ctx, dev);
	}
}

static fz_matrix
xps_parse_render_transform(fz_context *ctx, xps_document *doc, char *transform)
{
	fz_matrix matrix;
	float args[6];
	char *s = transform;
	int i;

	args[0] = 1; args[1] = 0;
	args[2] = 0; args[3] = 1;
	args[4] = 0; args[5] = 0;

	for (i = 0; i < 6 && *s; i++)
	{
		args[i] = fz_atof(s);
		while (*s && *s != ',')
			s++;
		if (*s == ',')
			s++;
	}

	matrix.a = args[0]; matrix.b = args[1];
	matrix.c = args[2]; matrix.d = args[3];
	matrix.e = args[4]; matrix.f = args[5];
	return matrix;
}

static fz_matrix
xps_parse_matrix_transform(fz_context *ctx, xps_document *doc, fz_xml *root)
{
	if (fz_xml_is_tag(root, "MatrixTransform"))
	{
		char *transform = fz_xml_att(root, "Matrix");
		if (transform)
			return xps_parse_render_transform(ctx, doc, transform);
	}
	return fz_identity;
}

fz_matrix
xps_parse_transform(fz_context *ctx, xps_document *doc, char *att, fz_xml *tag, fz_matrix ctm)
{
	if (att)
		return fz_concat(xps_parse_render_transform(ctx, doc, att), ctm);
	if (tag)
		return fz_concat(xps_parse_matrix_transform(ctx, doc, tag), ctm);
	return ctm;
}

fz_rect
xps_parse_rectangle(fz_context *ctx, xps_document *doc, char *text)
{
	fz_rect rect;
	float args[4];
	char *s = text;
	int i;

	args[0] = 0; args[1] = 0;
	args[2] = 1; args[3] = 1;

	for (i = 0; i < 4 && *s; i++)
	{
		args[i] = fz_atof(s);
		while (*s && *s != ',')
			s++;
		if (*s == ',')
			s++;
	}

	rect.x0 = args[0];
	rect.y0 = args[1];
	rect.x1 = args[0] + args[2];
	rect.y1 = args[1] + args[3];
	return rect;
}

static int count_commas(char *s)
{
	int n = 0;
	while (*s)
	{
		if (*s == ',')
			n ++;
		s ++;
	}
	return n;
}

static float sRGB_from_scRGB(float x)
{
	if (x < 0.0031308f)
		return 12.92f * x;
	return 1.055f * pow(x, 1/2.4f) - 0.055f;
}

void
xps_parse_color(fz_context *ctx, xps_document *doc, char *base_uri, char *string,
		fz_colorspace **csp, float *samples)
{
	char *p;
	int i, n;
	char buf[1024];
	char *profile;

	*csp = fz_device_rgb(ctx);

	samples[0] = 1;
	samples[1] = 0;
	samples[2] = 0;
	samples[3] = 0;

	if (string[0] == '#')
	{
		if (strlen(string) == 9)
		{
			samples[0] = unhex(string[1]) * 16 + unhex(string[2]);
			samples[1] = unhex(string[3]) * 16 + unhex(string[4]);
			samples[2] = unhex(string[5]) * 16 + unhex(string[6]);
			samples[3] = unhex(string[7]) * 16 + unhex(string[8]);
		}
		else
		{
			samples[0] = 255;
			samples[1] = unhex(string[1]) * 16 + unhex(string[2]);
			samples[2] = unhex(string[3]) * 16 + unhex(string[4]);
			samples[3] = unhex(string[5]) * 16 + unhex(string[6]);
		}

		samples[0] /= 255;
		samples[1] /= 255;
		samples[2] /= 255;
		samples[3] /= 255;
	}

	else if (string[0] == 's' && string[1] == 'c' && string[2] == '#')
	{
		if (count_commas(string) == 2)
			sscanf(string, "sc#%g,%g,%g", samples + 1, samples + 2, samples + 3);
		if (count_commas(string) == 3)
			sscanf(string, "sc#%g,%g,%g,%g", samples, samples + 1, samples + 2, samples + 3);

		/* Convert from scRGB gamma 1.0 to sRGB gamma */
		samples[1] = sRGB_from_scRGB(samples[1]);
		samples[2] = sRGB_from_scRGB(samples[2]);
		samples[3] = sRGB_from_scRGB(samples[3]);
	}

	else if (strstr(string, "ContextColor ") == string)
	{
		/* Crack the string for profile name and sample values */
		fz_strlcpy(buf, string, sizeof buf);

		profile = strchr(buf, ' ');
		if (!profile)
		{
			fz_warn(ctx, "cannot find icc profile uri in '%s'", string);
			return;
		}

		*profile++ = 0;
		p = strchr(profile, ' ');
		if (!p)
		{
			fz_warn(ctx, "cannot find component values in '%s'", profile);
			return;
		}

		*p++ = 0;
		n = count_commas(p) + 1;
		if (n > FZ_MAX_COLORS)
		{
			fz_warn(ctx, "ignoring %d color components (max %d allowed)", n - FZ_MAX_COLORS, FZ_MAX_COLORS);
			n = FZ_MAX_COLORS;
		}
		i = 0;
		while (i < n)
		{
			samples[i++] = fz_atof(p);
			p = strchr(p, ',');
			if (!p)
				break;
			p ++;
			if (*p == ' ')
				p ++;
		}
		while (i < n)
		{
			samples[i++] = 0;
		}

		/* TODO: load ICC profile */
		switch (n)
		{
		case 2: *csp = fz_device_gray(ctx); break;
		case 4: *csp = fz_device_rgb(ctx); break;
		case 5: *csp = fz_device_cmyk(ctx); break;
		default: *csp = fz_device_gray(ctx); break;
		}
	}
}

void
xps_set_color(fz_context *ctx, xps_document *doc, fz_colorspace *colorspace, float *samples)
{
	int i;
	int n = fz_colorspace_n(ctx, colorspace);
	doc->colorspace = colorspace;
	for (i = 0; i < n; i++)
		doc->color[i] = samples[i + 1];
	doc->alpha = samples[0] * doc->opacity[doc->opacity_top];
}
