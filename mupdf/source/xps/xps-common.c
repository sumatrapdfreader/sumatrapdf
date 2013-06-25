#include "mupdf/xps.h"

static inline int unhex(int a)
{
	if (a >= 'A' && a <= 'F') return a - 'A' + 0xA;
	if (a >= 'a' && a <= 'f') return a - 'a' + 0xA;
	if (a >= '0' && a <= '9') return a - '0';
	return 0;
}

fz_xml *
xps_lookup_alternate_content(fz_xml *node)
{
	for (node = fz_xml_down(node); node; node = fz_xml_next(node))
	{
		if (!strcmp(fz_xml_tag(node), "mc:Choice") && fz_xml_att(node, "Requires"))
		{
			char list[64];
			char *next = list, *item;
			fz_strlcpy(list, fz_xml_att(node, "Requires"), sizeof(list));
			while ((item = fz_strsep(&next, " \t\r\n")) && (!*item || !strcmp(item, "xps")));
			if (!item)
				return fz_xml_down(node);
		}
		else if (!strcmp(fz_xml_tag(node), "mc:Fallback"))
			return fz_xml_down(node);
	}
	return NULL;
}

void
xps_parse_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node)
{
	if (doc->cookie && doc->cookie->abort)
		return;
	/* SolidColorBrushes are handled in a special case and will never show up here */
	if (!strcmp(fz_xml_tag(node), "ImageBrush"))
		xps_parse_image_brush(doc, ctm, area, base_uri, dict, node);
	else if (!strcmp(fz_xml_tag(node), "VisualBrush"))
		xps_parse_visual_brush(doc, ctm, area, base_uri, dict, node);
	else if (!strcmp(fz_xml_tag(node), "LinearGradientBrush"))
		xps_parse_linear_gradient_brush(doc, ctm, area, base_uri, dict, node);
	else if (!strcmp(fz_xml_tag(node), "RadialGradientBrush"))
		xps_parse_radial_gradient_brush(doc, ctm, area, base_uri, dict, node);
	else
		fz_warn(doc->ctx, "unknown brush tag: %s", fz_xml_tag(node));
}

void
xps_parse_element(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node)
{
	if (doc->cookie && doc->cookie->abort)
		return;
	if (!strcmp(fz_xml_tag(node), "Path"))
		xps_parse_path(doc, ctm, base_uri, dict, node);
	if (!strcmp(fz_xml_tag(node), "Glyphs"))
		xps_parse_glyphs(doc, ctm, base_uri, dict, node);
	if (!strcmp(fz_xml_tag(node), "Canvas"))
		xps_parse_canvas(doc, ctm, area, base_uri, dict, node);
	if (!strcmp(fz_xml_tag(node), "mc:AlternateContent"))
	{
		node = xps_lookup_alternate_content(node);
		if (node)
			xps_parse_element(doc, ctm, area, base_uri, dict, node);
	}
	/* skip unknown tags (like Foo.Resources and similar) */
}

void
xps_begin_opacity(xps_document *doc, const fz_matrix *ctm, const fz_rect *area,
	char *base_uri, xps_resource *dict,
	char *opacity_att, fz_xml *opacity_mask_tag)
{
	float opacity;

	if (!opacity_att && !opacity_mask_tag)
		return;

	opacity = 1;
	if (opacity_att)
		opacity = fz_atof(opacity_att);

	if (opacity_mask_tag && !strcmp(fz_xml_tag(opacity_mask_tag), "SolidColorBrush"))
	{
		char *scb_opacity_att = fz_xml_att(opacity_mask_tag, "Opacity");
		char *scb_color_att = fz_xml_att(opacity_mask_tag, "Color");
		if (scb_opacity_att)
			opacity = opacity * fz_atof(scb_opacity_att);
		if (scb_color_att)
		{
			fz_colorspace *colorspace;
			float samples[32];
			xps_parse_color(doc, base_uri, scb_color_att, &colorspace, samples);
			opacity = opacity * samples[0];
		}
		opacity_mask_tag = NULL;
	}

	if (doc->opacity_top + 1 < nelem(doc->opacity))
	{
		doc->opacity[doc->opacity_top + 1] = doc->opacity[doc->opacity_top] * opacity;
		doc->opacity_top++;
	}

	if (opacity_mask_tag)
	{
		fz_begin_mask(doc->dev, area, 0, NULL, NULL);
		xps_parse_brush(doc, ctm, area, base_uri, dict, opacity_mask_tag);
		fz_end_mask(doc->dev);
	}
}

void
xps_end_opacity(xps_document *doc, char *base_uri, xps_resource *dict,
	char *opacity_att, fz_xml *opacity_mask_tag)
{
	if (!opacity_att && !opacity_mask_tag)
		return;

	if (doc->opacity_top > 0)
		doc->opacity_top--;

	if (opacity_mask_tag)
	{
		if (strcmp(fz_xml_tag(opacity_mask_tag), "SolidColorBrush"))
			fz_pop_clip(doc->dev);
	}
}

void
xps_parse_render_transform(xps_document *doc, char *transform, fz_matrix *matrix)
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
xps_parse_matrix_transform(xps_document *doc, fz_xml *root, fz_matrix *matrix)
{
	char *transform;

	*matrix = fz_identity;

	if (!strcmp(fz_xml_tag(root), "MatrixTransform"))
	{
		transform = fz_xml_att(root, "Matrix");
		if (transform)
			xps_parse_render_transform(doc, transform, matrix);
	}
}

void
xps_parse_rectangle(xps_document *doc, char *text, fz_rect *rect)
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
xps_parse_color(xps_document *doc, char *base_uri, char *string,
		fz_colorspace **csp, float *samples)
{
	char *p;
	int i, n;
	char buf[1024];
	char *profile;

	*csp = fz_device_rgb(doc->ctx);

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
			fz_warn(doc->ctx, "cannot find icc profile uri in '%s'", string);
			return;
		}

		*profile++ = 0;
		p = strchr(profile, ' ');
		if (!p)
		{
			fz_warn(doc->ctx, "cannot find component values in '%s'", profile);
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
		case 2: *csp = fz_device_gray(doc->ctx); break;
		case 4: *csp = fz_device_rgb(doc->ctx); break;
		case 5: *csp = fz_device_cmyk(doc->ctx); break;
		default: *csp = fz_device_gray(doc->ctx); break;
		}
	}
}

void
xps_set_color(xps_document *doc, fz_colorspace *colorspace, float *samples)
{
	int i;
	doc->colorspace = colorspace;
	for (i = 0; i < colorspace->n; i++)
		doc->color[i] = samples[i + 1];
	doc->alpha = samples[0] * doc->opacity[doc->opacity_top];
}
