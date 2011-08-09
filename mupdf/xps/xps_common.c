#include "fitz.h"
#include "muxps.h"

static inline int unhex(int a)
{
	if (a >= 'A' && a <= 'F') return a - 'A' + 0xA;
	if (a >= 'a' && a <= 'f') return a - 'a' + 0xA;
	if (a >= '0' && a <= '9') return a - '0';
	return 0;
}

void
xps_parse_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node)
{
	/* SolidColorBrushes are handled in a special case and will never show up here */
	if (!strcmp(xml_tag(node), "ImageBrush"))
		xps_parse_image_brush(ctx, ctm, area, base_uri, dict, node);
	else if (!strcmp(xml_tag(node), "VisualBrush"))
		xps_parse_visual_brush(ctx, ctm, area, base_uri, dict, node);
	else if (!strcmp(xml_tag(node), "LinearGradientBrush"))
		xps_parse_linear_gradient_brush(ctx, ctm, area, base_uri, dict, node);
	else if (!strcmp(xml_tag(node), "RadialGradientBrush"))
		xps_parse_radial_gradient_brush(ctx, ctm, area, base_uri, dict, node);
	else
		fz_warn("unknown brush tag: %s", xml_tag(node));
}

void
xps_parse_element(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node)
{
	if (!strcmp(xml_tag(node), "Path"))
		xps_parse_path(ctx, ctm, base_uri, dict, node);
	if (!strcmp(xml_tag(node), "Glyphs"))
		xps_parse_glyphs(ctx, ctm, base_uri, dict, node);
	if (!strcmp(xml_tag(node), "Canvas"))
		xps_parse_canvas(ctx, ctm, area, base_uri, dict, node);
	/* skip unknown tags (like Foo.Resources and similar) */
}

void
xps_begin_opacity(xps_context *ctx, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict,
	char *opacity_att, xml_element *opacity_mask_tag)
{
	float opacity;

	if (!opacity_att && !opacity_mask_tag)
		return;

	opacity = 1;
	if (opacity_att)
		opacity = fz_atof(opacity_att);

	if (opacity_mask_tag && !strcmp(xml_tag(opacity_mask_tag), "SolidColorBrush"))
	{
		char *scb_opacity_att = xml_att(opacity_mask_tag, "Opacity");
		char *scb_color_att = xml_att(opacity_mask_tag, "Color");
		if (scb_opacity_att)
			opacity = opacity * fz_atof(scb_opacity_att);
		if (scb_color_att)
		{
			fz_colorspace *colorspace;
			float samples[32];
			xps_parse_color(ctx, base_uri, scb_color_att, &colorspace, samples);
			opacity = opacity * samples[0];
		}
		opacity_mask_tag = NULL;
	}

	if (ctx->opacity_top + 1 < nelem(ctx->opacity))
	{
		ctx->opacity[ctx->opacity_top + 1] = ctx->opacity[ctx->opacity_top] * opacity;
		ctx->opacity_top++;
	}

	if (opacity_mask_tag)
	{
		fz_begin_mask(ctx->dev, area, 0, NULL, NULL);
		xps_parse_brush(ctx, ctm, area, base_uri, dict, opacity_mask_tag);
		fz_end_mask(ctx->dev);
	}
}

void
xps_end_opacity(xps_context *ctx, char *base_uri, xps_resource *dict,
	char *opacity_att, xml_element *opacity_mask_tag)
{
	if (!opacity_att && !opacity_mask_tag)
		return;

	if (ctx->opacity_top > 0)
		ctx->opacity_top--;

	if (opacity_mask_tag)
	{
		if (strcmp(xml_tag(opacity_mask_tag), "SolidColorBrush"))
			fz_pop_clip(ctx->dev);
	}
}

void
xps_parse_render_transform(xps_context *ctx, char *transform, fz_matrix *matrix)
{
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

	matrix->a = args[0]; matrix->b = args[1];
	matrix->c = args[2]; matrix->d = args[3];
	matrix->e = args[4]; matrix->f = args[5];
}

void
xps_parse_matrix_transform(xps_context *ctx, xml_element *root, fz_matrix *matrix)
{
	char *transform;

	*matrix = fz_identity;

	if (!strcmp(xml_tag(root), "MatrixTransform"))
	{
		transform = xml_att(root, "Matrix");
		if (transform)
			xps_parse_render_transform(ctx, transform, matrix);
	}
}

void
xps_parse_rectangle(xps_context *ctx, char *text, fz_rect *rect)
{
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

	rect->x0 = args[0];
	rect->y0 = args[1];
	rect->x1 = args[0] + args[2];
	rect->y1 = args[1] + args[3];
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

void
xps_parse_color(xps_context *ctx, char *base_uri, char *string,
		fz_colorspace **csp, float *samples)
{
	char *p;
	int i, n;
	char buf[1024];
	char *profile;

	*csp = fz_device_rgb;

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
	}

	else if (strstr(string, "ContextColor ") == string)
	{
		/* Crack the string for profile name and sample values */
		fz_strlcpy(buf, string, sizeof buf);

		profile = strchr(buf, ' ');
		if (!profile)
		{
			fz_warn("cannot find icc profile uri in '%s'", string);
			return;
		}

		*profile++ = 0;
		p = strchr(profile, ' ');
		if (!p)
		{
			fz_warn("cannot find component values in '%s'", profile);
			return;
		}

		*p++ = 0;
		n = count_commas(p) + 1;
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
		case 2: *csp = fz_device_gray; break;
		case 4: *csp = fz_device_rgb; break;
		case 5: *csp = fz_device_cmyk; break;
		default: *csp = fz_device_gray; break;
		}
	}
}

void
xps_set_color(xps_context *ctx, fz_colorspace *colorspace, float *samples)
{
	int i;
	ctx->colorspace = colorspace;
	for (i = 0; i < colorspace->n; i++)
		ctx->color[i] = samples[i + 1];
	ctx->alpha = samples[0] * ctx->opacity[ctx->opacity_top];
}
