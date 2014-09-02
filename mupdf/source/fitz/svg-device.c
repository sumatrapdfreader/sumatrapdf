#include "mupdf/fitz.h"

typedef struct svg_device_s svg_device;

typedef struct tile_s tile;
typedef struct font_s font;
typedef struct glyph_s glyph;

struct tile_s
{
	int pattern;
	fz_matrix ctm;
	fz_rect view;
	fz_rect area;
	fz_point step;
};

struct glyph_s
{
	float x_off;
	float y_off;
};

struct font_s
{
	int id;
	fz_font *font;
	int max_sentlist;
	glyph *sentlist;
};

struct svg_device_s
{
	fz_context *ctx;
	fz_output *out;
	fz_output *out_store;
	fz_output *defs;
	fz_buffer *defs_buffer;
	int def_count;

	int id;

	int num_tiles;
	int max_tiles;
	tile *tiles;

	int num_fonts;
	int max_fonts;
	font *fonts;
};

/* SVG is awkward about letting us define things within symbol definitions
 * so we have to delay definitions until after the symbol definition ends. */

static fz_output *
start_def(svg_device *sdev)
{
	sdev->def_count++;
	if (sdev->def_count == 2)
	{
		if (sdev->defs == NULL)
		{
			if (sdev->defs_buffer == NULL)
				sdev->defs_buffer = fz_new_buffer(sdev->ctx, 1024);
			sdev->defs = fz_new_output_with_buffer(sdev->ctx, sdev->defs_buffer);
		}
		sdev->out = sdev->defs;
	}
	return sdev->out;
}

static fz_output *
end_def(svg_device *sdev)
{
	if (sdev->def_count > 0)
		sdev->def_count--;
	if (sdev->def_count == 1)
		sdev->out = sdev->out_store;
	if (sdev->def_count == 0 && sdev->defs_buffer != NULL)
	{
		fz_write(sdev->out, sdev->defs_buffer->data, sdev->defs_buffer->len);
		sdev->defs_buffer->len = 0;
	}
	return sdev->out;
}

/* Helper functions */

static void
svg_dev_path(svg_device *sdev, fz_path *path)
{
	fz_output *out = sdev->out;
	float x, y;
	int i, k;
	fz_printf(out, " d=\"");
	for (i = 0, k = 0; i < path->cmd_len; i++)
	{
		switch (path->cmds[i])
		{
		case FZ_MOVETO:
			x = path->coords[k++];
			y = path->coords[k++];
			fz_printf(out, "M %g %g ", x, y);
			break;
		case FZ_LINETO:
			x = path->coords[k++];
			y = path->coords[k++];
			fz_printf(out, "L %g %g ", x, y);
			break;
		case FZ_CURVETO:
			x = path->coords[k++];
			y = path->coords[k++];
			fz_printf(out, "C %g %g ", x, y);
			x = path->coords[k++];
			y = path->coords[k++];
			fz_printf(out, "%g %g ", x, y);
			x = path->coords[k++];
			y = path->coords[k++];
			fz_printf(out, "%g %g ", x, y);
			break;
		case FZ_CLOSE_PATH:
			fz_printf(out, "Z ");
			break;
		}
	}
	fz_printf(out, "\"");
}

static void
svg_dev_ctm(svg_device *sdev, const fz_matrix *ctm)
{
	fz_output *out = sdev->out;

	if (ctm->a != 1.0 || ctm->b != 0 || ctm->c != 0 || ctm->d != 1.0 || ctm->e != 0 || ctm->f != 0)
	{
		fz_printf(out, " transform=\"matrix(%g,%g,%g,%g,%g,%g)\"",
			ctm->a, ctm->b, ctm->c, ctm->d, ctm->e, ctm->f);
	}
}

static void
svg_dev_stroke_state(svg_device *sdev, fz_stroke_state *stroke_state, const fz_matrix *ctm)
{
	fz_output *out = sdev->out;
	float exp;

	exp = fz_matrix_expansion(ctm);
	if (exp == 0)
		exp = 1;

	fz_printf(out, " stroke-width=\"%g\"", stroke_state->linewidth/exp);
	fz_printf(out, " stroke-linecap=\"%s\"",
		(stroke_state->start_cap == FZ_LINECAP_SQUARE ? "square" :
			(stroke_state->start_cap == FZ_LINECAP_ROUND ? "round" : "butt")));
	if (stroke_state->dash_len != 0)
	{
		int i;
		fz_printf(out, " stroke-dasharray=");
		for (i = 0; i < stroke_state->dash_len; i++)
			fz_printf(out, "%c%g", (i == 0 ? '\"' : ','), stroke_state->dash_list[i]);
		fz_printf(out, "\"");
		if (stroke_state->dash_phase != 0)
			fz_printf(out, " stroke-dashoffset=\"%g\"", stroke_state->dash_phase);
	}
	if (stroke_state->linejoin == FZ_LINEJOIN_MITER || stroke_state->linejoin == FZ_LINEJOIN_MITER_XPS)
		fz_printf(out, " stroke-miterlimit=\"%g\"", stroke_state->miterlimit);
	fz_printf(out, " stroke-linejoin=\"%s\"",
		(stroke_state->linejoin == FZ_LINEJOIN_BEVEL ? "bevel" :
			(stroke_state->linejoin == FZ_LINEJOIN_ROUND ? "round" : "miter")));
}

static void
svg_dev_fill_color(svg_device *sdev, fz_colorspace *colorspace, float *color, float alpha)
{
	fz_context *ctx = sdev->ctx;
	fz_output *out = sdev->out;
	float rgb[FZ_MAX_COLORS];

	if (colorspace != fz_device_rgb(ctx))
	{
		/* If it's not rgb, make it rgb */
		colorspace->to_rgb(ctx, colorspace, color, rgb);
		color = rgb;
	}

	if (color[0] == 0 && color[1] == 0 && color[2] == 0)
	{
		/* don't send a fill, as it will be assumed to be black */
	}
	else
		fz_printf(out, " fill=\"rgb(%d,%d,%d)\"", (int)(255*color[0] + 0.5), (int)(255*color[1] + 0.5), (int)(255*color[2]+0.5));
	if (alpha != 1)
		fz_printf(out, " fill-opacity=\"%g\"", alpha);
}

static void
svg_dev_stroke_color(svg_device *sdev, fz_colorspace *colorspace, float *color, float alpha)
{
	fz_context *ctx = sdev->ctx;
	fz_output *out = sdev->out;
	float rgb[FZ_MAX_COLORS];

	if (colorspace != fz_device_rgb(ctx))
	{
		/* If it's not rgb, make it rgb */
		colorspace->to_rgb(ctx, colorspace, color, rgb);
		color = rgb;
	}

	fz_printf(out, " fill=\"none\" stroke=\"rgb(%d,%d,%d)\"", (int)(255*color[0] + 0.5), (int)(255*color[1] + 0.5), (int)(255*color[2]+0.5));
	if (alpha != 1)
		fz_printf(out, " stroke-opacity=\"%g\"", alpha);
}

static inline int
is_xml_wspace(int c)
{
	return (c == 9 || /* TAB */
		c == 0x0a || /* HT */
		c == 0x0b || /* LF */
		c == 0x20);
}

static void
svg_dev_text(svg_device *sdev, const fz_matrix *ctm, fz_text *text)
{
	fz_output *out = sdev->out;
	int i;
	fz_matrix inverse;
	fz_matrix local_trm;
	float size;
	int start, is_wspace, was_wspace;

	/* Rely on the fact that trm.{e,f} == 0 */
	size = fz_matrix_expansion(&text->trm);
	local_trm.a = text->trm.a / size;
	local_trm.b = text->trm.b / size;
	local_trm.c = -text->trm.c / size;
	local_trm.d = -text->trm.d / size;
	local_trm.e = 0;
	local_trm.f = 0;
	fz_invert_matrix(&inverse, &local_trm);
	fz_concat(&local_trm, &local_trm, ctm);

	fz_printf(out, " transform=\"matrix(%g,%g,%g,%g,%g,%g)\"",
		local_trm.a, local_trm.b, local_trm.c, local_trm.d, local_trm.e, local_trm.f);
	fz_printf(out, " font-size=\"%g\"", size);
	fz_printf(out, " font-family=\"%s\"", text->font->name);

	/* Leading (and repeated) whitespace presents a problem for SVG
	 * text, so elide it here. */
	for (start=0; start < text->len; start++)
	{
		fz_text_item *it = &text->items[start];
		if (!is_xml_wspace(it->ucs))
			break;
	}

	fz_printf(out, " x=");
	was_wspace = 0;
	for (i=start; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		fz_point p;
		is_wspace = is_xml_wspace(it->ucs);
		if (is_wspace && was_wspace)
			continue;
		was_wspace = is_wspace;
		p.x = it->x;
		p.y = it->y;
		fz_transform_point(&p, &inverse);
		fz_printf(out, "%c%g", i == start ? '\"' : ' ', p.x);
	}
	fz_printf(out, "\" y=");
	was_wspace = 0;
	for (i=start; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		fz_point p;
		is_wspace = is_xml_wspace(it->ucs);
		if (is_wspace && was_wspace)
			continue;
		was_wspace = is_wspace;
		p.x = it->x;
		p.y = it->y;
		fz_transform_point(&p, &inverse);
		fz_printf(out, "%c%g", i == start ? '\"' : ' ', p.y);
	}
	fz_printf(out, "\">\n");
	was_wspace = 0;
	for (i=start; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		int c = it->ucs;
		is_wspace = is_xml_wspace(c);
		if (is_wspace && was_wspace)
			continue;
		was_wspace = is_wspace;
		if (c >= 32 && c <= 127 && c != '<' && c != '&')
			fz_printf(out, "%c", c);
		else
			fz_printf(out, "&#x%04x;", c);
	}
	fz_printf(out, "\n</text>\n");
}

static font *
svg_dev_text_as_paths_defs(fz_device *dev, fz_text *text, const fz_matrix *ctm)
{
	svg_device *sdev = dev->user;
	fz_context *ctx = sdev->ctx;
	fz_output *out = sdev->out;
	int i, font_idx;
	font *fnt;
	fz_matrix shift = fz_identity;

	for (font_idx = 0; font_idx < sdev->num_fonts; font_idx++)
	{
		if (sdev->fonts[font_idx].font == text->font)
			break;
	}
	if (font_idx == sdev->num_fonts)
	{
		/* New font */
		if (font_idx == sdev->max_fonts)
		{
			int newmax = sdev->max_fonts * 2;
			if (newmax == 0)
				newmax = 4;
			sdev->fonts = fz_resize_array(ctx, sdev->fonts, newmax, sizeof(*sdev->fonts));
			memset(&sdev->fonts[font_idx], 0, (newmax - font_idx) * sizeof(sdev->fonts[0]));
			sdev->max_fonts = newmax;
		}
		sdev->fonts[font_idx].id = sdev->id++;
		sdev->fonts[font_idx].font = fz_keep_font(ctx, text->font);
		sdev->num_fonts++;
	}
	fnt = &sdev->fonts[font_idx];

	for (i=0; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		int gid = it->gid;

		if (gid < 0)
			continue;
		if (gid >= fnt->max_sentlist)
		{
			int j;
			fnt->sentlist = fz_resize_array(ctx, fnt->sentlist, gid+1, sizeof(fnt->sentlist[0]));
			for (j = fnt->max_sentlist; j <= gid; j++)
			{
				fnt->sentlist[j].x_off = FLT_MIN;
				fnt->sentlist[j].y_off = FLT_MIN;
			}
			fnt->max_sentlist = gid+1;
		}
		if (fnt->sentlist[gid].x_off == FLT_MIN)
		{
			/* Need to send this one */
			fz_rect rect;
			fz_path *path;
			path = fz_outline_glyph(sdev->ctx, text->font, gid, &fz_identity);
			if (path)
			{
				fz_bound_path(ctx, path, NULL, &fz_identity, &rect);
				shift.e = -rect.x0;
				shift.f = -rect.y0;
				fz_transform_path(ctx, path, &shift);
				out = start_def(sdev);
				fz_printf(out, "<symbol id=\"font_%x_%x\">", fnt->id, gid);
				fz_printf(out, "<path");
				svg_dev_path(sdev, path);
				fz_printf(out, "/>\n");
			}
			else
			{
				fz_bound_glyph(ctx, text->font, gid, &fz_identity, &rect);
				shift.e = -rect.x0;
				shift.f = -rect.y0;
				out = start_def(sdev);
				fz_printf(out, "<symbol id=\"font_%x_%x\">", fnt->id, gid);
				fz_run_t3_glyph(ctx, text->font, gid, &shift, dev);
			}
			fz_printf(out, "</symbol>");
			out = end_def(sdev);
			fnt->sentlist[gid].x_off = rect.x0;
			fnt->sentlist[gid].y_off = rect.y0;
		}
	}
	return fnt;
}

static void
svg_dev_text_as_paths_fill(fz_device *dev, fz_text *text, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha, font *fnt)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;
	fz_matrix local_trm, local_trm2;
	int i;
	fz_matrix shift = { 1, 0, 0, 1, 0, 0};

	/* Rely on the fact that trm.{e,f} == 0 */
	local_trm.a = text->trm.a;
	local_trm.b = text->trm.b;
	local_trm.c = text->trm.c;
	local_trm.d = text->trm.d;
	local_trm.e = 0;
	local_trm.f = 0;

	for (i=0; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		int gid = it->gid;

		if (gid < 0)
			continue;

		shift.e = fnt->sentlist[gid].x_off;
		shift.f = fnt->sentlist[gid].y_off;
		local_trm.e = it->x;
		local_trm.f = it->y;
		fz_concat(&local_trm2, &local_trm, ctm);
		fz_concat(&local_trm2, &shift, &local_trm2);
		fz_printf(out, "<use xlink:href=\"#font_%x_%x\"", fnt->id, gid);
		svg_dev_ctm(sdev, &local_trm2);
		svg_dev_fill_color(sdev, colorspace, color, alpha);
		fz_printf(out, "/>\n");
	}
}

static void
svg_dev_text_as_paths_stroke(fz_device *dev, fz_text *text,
	fz_stroke_state *stroke, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha, font *fnt)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;
	fz_matrix local_trm, local_trm2;
	int i;
	fz_matrix shift = { 1, 0, 0, 1, 0, 0};

	/* Rely on the fact that trm.{e,f} == 0 */
	local_trm.a = text->trm.a;
	local_trm.b = text->trm.b;
	local_trm.c = text->trm.c;
	local_trm.d = text->trm.d;
	local_trm.e = 0;
	local_trm.f = 0;

	for (i=0; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		int gid = it->gid;

		if (gid < 0)
			continue;

		shift.e = fnt->sentlist[gid].x_off;
		shift.f = fnt->sentlist[gid].y_off;
		local_trm.e = it->x;
		local_trm.f = it->y;
		fz_concat(&local_trm2, &local_trm, ctm);
		fz_concat(&local_trm2, &shift, &local_trm2);
		fz_printf(out, "<use xlink:href=\"#font_%x_%x\"", fnt->id, gid);
		svg_dev_stroke_state(sdev, stroke, &local_trm2);
		svg_dev_ctm(sdev, &local_trm2);
		svg_dev_stroke_color(sdev, colorspace, color, alpha);
		fz_printf(out, "/>\n");
	}
}

/* Entry points */

static void
svg_dev_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;

	fz_printf(out, "<path");
	svg_dev_ctm(sdev, ctm);
	svg_dev_path(sdev, path);
	svg_dev_fill_color(sdev, colorspace, color, alpha);
	if (even_odd)
		fz_printf(out, " fill-rule=\"evenodd\"");
	fz_printf(out, "/>\n");
}

static void
svg_dev_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;

	fz_printf(out, "<path");
	svg_dev_ctm(sdev, ctm);
	svg_dev_stroke_state(sdev, stroke, &fz_identity);
	svg_dev_stroke_color(sdev, colorspace, color, alpha);
	svg_dev_path(sdev, path);
	fz_printf(out, "/>\n");
}

static void
svg_dev_clip_path(fz_device *dev, fz_path *path, const fz_rect *rect, int even_odd, const fz_matrix *ctm)
{
	svg_device *sdev = dev->user;
	fz_output *out;
	int num = sdev->id++;

	out = start_def(sdev);
	fz_printf(out, "<clipPath id=\"cp%d\">\n", num);
	fz_printf(out, "<path");
	svg_dev_ctm(sdev, ctm);
	svg_dev_path(sdev, path);
	if (even_odd)
		fz_printf(out, " fill-rule=\"evenodd\"");
	fz_printf(out, "/>\n</clipPath>\n");
	out = end_def(sdev);
	fz_printf(out, "<g clip-path=\"url(#cp%d)\">\n", num);
}

static void
svg_dev_clip_stroke_path(fz_device *dev, fz_path *path, const fz_rect *rect, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	svg_device *sdev = dev->user;
	fz_output *out;
	fz_context *ctx = dev->ctx;
	fz_rect bounds;
	int num = sdev->id++;
	float white[3] = { 1, 1, 1 };

	fz_bound_path(ctx, path, stroke, ctm, &bounds);

	out = start_def(sdev);
	fz_printf(out, "<mask id=\"ma%d\" x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" maskUnits=\"userSpaceOnUse\" maskContentUnits=\"userSpaceOnUse\">\n",
		num, bounds.x0, bounds.y0, bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
	fz_printf(out, "<path");
	svg_dev_ctm(sdev, ctm);
	svg_dev_stroke_state(sdev, stroke, &fz_identity);
	svg_dev_stroke_color(sdev, fz_device_rgb(ctx), white, 1);
	svg_dev_path(sdev, path);
	fz_printf(out, "/>\n</mask>\n");
	out = end_def(sdev);
	fz_printf(out, "<g mask=\"url(#ma%d)\">\n", num);
}

static void
svg_dev_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;
	font *fnt;

	fz_printf(out, "<text");
	svg_dev_fill_color(sdev, colorspace, color, 0.0f);
	svg_dev_text(sdev, ctm, text);
	fnt = svg_dev_text_as_paths_defs(dev, text, ctm);
	svg_dev_text_as_paths_fill(dev, text, ctm, colorspace, color, alpha, fnt);
}

static void
svg_dev_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;
	font *fnt;

	fz_printf(out, "<text");
	svg_dev_fill_color(sdev, colorspace, color, 0.0f);
	svg_dev_text(sdev, ctm, text);
	fnt = svg_dev_text_as_paths_defs(dev, text, ctm);
	svg_dev_text_as_paths_stroke(dev, text, stroke, ctm, colorspace, color, alpha, fnt);
}

static void
svg_dev_clip_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, int accumulate)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;
	fz_context *ctx = dev->ctx;
	fz_rect bounds;
	int num = sdev->id++;
	float white[3] = { 1, 1, 1 };
	font *fnt;

	fz_bound_text(ctx, text, NULL, ctm, &bounds);

	out = start_def(sdev);
	fz_printf(out, "<mask id=\"ma%d\" x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" maskUnits=\"userSpaceOnUse\" maskContentUnits=\"userSpaceOnUse\">\n",
		num, bounds.x0, bounds.y0, bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
	fz_printf(out, "<text");
	svg_dev_fill_color(sdev, fz_device_rgb(ctx), white, 0.0f);
	svg_dev_text(sdev, ctm, text);
	fnt = svg_dev_text_as_paths_defs(dev, text, ctm);
	svg_dev_text_as_paths_fill(dev, text, ctm, fz_device_rgb(ctx), white, 1.0f, fnt);
	fz_printf(out, "</mask>\n");
	out = end_def(sdev);
	fz_printf(out, "<g mask=\"url(#ma%d)\">\n", num);
}

static void
svg_dev_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	svg_device *sdev = dev->user;
	fz_output *out;
	fz_context *ctx = dev->ctx;
	fz_rect bounds;
	int num = sdev->id++;
	float white[3] = { 255, 255, 255 };
	font *fnt;

	fz_bound_text(ctx, text, NULL, ctm, &bounds);

	out = start_def(sdev);
	fz_printf(out, "<mask id=\"ma%d\" x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\" maskUnits=\"userSpaceOnUse\" maskContentUnits=\"userSpaceOnUse\">\n",
		num, bounds.x0, bounds.y0, bounds.x1 - bounds.x0, bounds.y1 - bounds.y0);
	fz_printf(out, "<text");
	svg_dev_stroke_state(sdev, stroke, &fz_identity);
	svg_dev_stroke_color(sdev, fz_device_rgb(ctx), white, 0.0f);
	svg_dev_text(sdev, ctm, text);
	fnt = svg_dev_text_as_paths_defs(dev, text, ctm);
	svg_dev_text_as_paths_stroke(dev, text, stroke, ctm, fz_device_rgb(ctx), white, 1.0f, fnt);
	fz_printf(out, "</mask>\n");
	out = end_def(sdev);
	fz_printf(out, "<g mask=\"url(#ma%d)\">\n", num);
}

static void
svg_dev_ignore_text(fz_device *dev, fz_text *text, const fz_matrix *ctm)
{
	svg_device *sdev = dev->user;
	fz_output *out = sdev->out;
	float black[3] = { 0, 0, 0};

	fz_printf(out, "<text");
	svg_dev_fill_color(sdev, fz_device_rgb(sdev->ctx), black, 0.0f);
	svg_dev_text(sdev, ctm, text);
}

static void
send_data_base64(fz_output *out, fz_buffer *buffer)
{
	int i, len;
	static const char set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	len = buffer->len/3;
	for (i = 0; i < len; i++)
	{
		int c = buffer->data[3*i];
		int d = buffer->data[3*i+1];
		int e = buffer->data[3*i+2];
		if ((i & 15) == 0)
			fz_printf(out, "\n");
		fz_printf(out, "%c%c%c%c", set[c>>2], set[((c&3)<<4)|(d>>4)], set[((d&15)<<2)|(e>>6)], set[e & 63]);
	}
	i *= 3;
	switch (buffer->len-i)
	{
		case 2:
		{
			int c = buffer->data[i];
			int d = buffer->data[i+1];
			fz_printf(out, "%c%c%c=", set[c>>2], set[((c&3)<<4)|(d>>4)], set[((d&15)<<2)]);
			break;
		}
	case 1:
		{
			int c = buffer->data[i];
			fz_printf(out, "%c%c==", set[c>>2], set[(c&3)<<4]);
			break;
		}
	default:
	case 0:
		break;
	}
}

static void
svg_dev_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_context *ctx = dev->ctx;
	fz_output *out = sdev->out;
	fz_matrix local_ctm = *ctm;
	fz_matrix scale = { 0 };

	scale.a = 1.0f / image->w;
	scale.d = 1.0f / image->h;

	fz_concat(&local_ctm, &scale, ctm);
	if (alpha != 1.0f)
		fz_printf(out, "<g opacity=\"%g\">", alpha);
	fz_printf(out, "<image");
	svg_dev_ctm(sdev, &local_ctm);
	fz_printf(out, " width=\"%dpx\" height=\"%dpx\" xlink:href=\"data:", image->w, image->h);
	switch (image->buffer == NULL ? FZ_IMAGE_JPX : image->buffer->params.type)
	{
	case FZ_IMAGE_JPEG:
		fz_printf(out, "image/jpeg;base64,");
		send_data_base64(out, image->buffer->buffer);
		break;
	case FZ_IMAGE_PNG:
		fz_printf(out, "image/png;base64,");
		send_data_base64(out, image->buffer->buffer);
		break;
	default:
		{
			fz_buffer *buf = fz_new_png_from_image(ctx, image, image->w, image->h);
			fz_printf(out, "image/png;base64,");
			send_data_base64(out, buf);
			fz_drop_buffer(ctx, buf);
			break;
		}
	}
	fz_printf(out, "\"/>\n");
	if (alpha != 1.0f)
		fz_printf(out, "</g>");
}

static void
svg_dev_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_context *ctx = dev->ctx;
	fz_output *out = sdev->out;
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_buffer *buf = NULL;

	fz_var(buf);

	if (dev->container_len == 0)
		return;

	fz_round_rect(&bbox, fz_intersect_rect(fz_bound_shade(ctx, shade, ctm, &rect), &dev->container[dev->container_len-1].scissor));
	if (fz_is_empty_irect(&bbox))
		return;
	pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), &bbox);
	fz_clear_pixmap(ctx, pix);

	fz_try(ctx)
	{
		fz_paint_shade(ctx, shade, ctm, pix, &bbox);
		buf = fz_new_png_from_pixmap(ctx, pix);
		if (alpha != 1.0f)
			fz_printf(out, "<g opacity=\"%g\">", alpha);
		fz_printf(out, "<image x=\"%dpx\" y=\"%dpx\" width=\"%dpx\" height=\"%dpx\" xlink:href=\"data:image/png;base64,", pix->x, pix->y, pix->w, pix->h);
		send_data_base64(out, buf);
		fz_printf(out, "\"/>\n");
		if (alpha != 1.0f)
			fz_printf(out, "</g>");
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
svg_dev_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm,
fz_colorspace *colorspace, float *color, float alpha)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_context *ctx = dev->ctx;
	fz_output *out;
	fz_matrix local_ctm = *ctm;
	fz_matrix scale = { 0 };
	int mask = sdev->id++;

	scale.a = 1.0f / image->w;
	scale.d = 1.0f / image->h;

	fz_concat(&local_ctm, &scale, ctm);
	out = start_def(sdev);
	fz_printf(out, "<mask id=\"ma%d\"><image", mask);
	fz_printf(out, " width=\"%dpx\" height=\"%dpx\" xlink:href=\"data:", image->w, image->h);
	switch (image->buffer == NULL ? FZ_IMAGE_JPX : image->buffer->params.type)
	{
	case FZ_IMAGE_JPEG:
		fz_printf(out, "image/jpeg;base64,");
		send_data_base64(out, image->buffer->buffer);
		break;
	case FZ_IMAGE_PNG:
		fz_printf(out, "image/png;base64,");
		send_data_base64(out, image->buffer->buffer);
		break;
	default:
		{
			fz_buffer *buf = fz_new_png_from_image(ctx, image, image->w, image->h);
			fz_printf(out, "image/png;base64,");
			send_data_base64(out, buf);
			fz_drop_buffer(ctx, buf);
			break;
		}
	}
	fz_printf(out, "\"/></mask>\n");
	out = end_def(sdev);
	fz_printf(out, "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\"", image->w, image->h);
	svg_dev_fill_color(sdev, colorspace, color, alpha);
	svg_dev_ctm(sdev, &local_ctm);
	fz_printf(out, " mask=\"url(#ma%d)\"/>\n", mask);
}

static void
svg_dev_clip_image_mask(fz_device *dev, fz_image *image, const fz_rect *rect, const fz_matrix *ctm)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_context *ctx = dev->ctx;
	fz_output *out;
	fz_matrix local_ctm = *ctm;
	fz_matrix scale = { 0 };
	int mask = sdev->id++;

	scale.a = 1.0f / image->w;
	scale.d = 1.0f / image->h;

	fz_concat(&local_ctm, &scale, ctm);
	out = start_def(sdev);
	fz_printf(out, "<mask id=\"ma%d\"><image", mask);
	svg_dev_ctm(sdev, &local_ctm);
	fz_printf(out, " width=\"%dpx\" height=\"%dpx\" xlink:href=\"data:", image->w, image->h);
	switch (image->buffer == NULL ? FZ_IMAGE_JPX : image->buffer->params.type)
	{
	case FZ_IMAGE_JPEG:
		fz_printf(out, "image/jpeg;base64,");
		send_data_base64(out, image->buffer->buffer);
		break;
	case FZ_IMAGE_PNG:
		fz_printf(out, "image/png;base64,");
		send_data_base64(out, image->buffer->buffer);
		break;
	default:
		{
			fz_buffer *buf = fz_new_png_from_image(ctx, image, image->w, image->h);
			fz_printf(out, "image/png;base64,");
			send_data_base64(out, buf);
			fz_drop_buffer(ctx, buf);
			break;
		}
	}
	fz_printf(out, "\"/></mask>\n");
	out = end_def(sdev);
	fz_printf(out, "<g mask=\"url(#ma%d)\">\n", mask);
}

static void
svg_dev_pop_clip(fz_device *dev)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out = sdev->out;

	/* FIXME */
	fz_printf(out, "</g>\n");
}

static void
svg_dev_begin_mask(fz_device *dev, const fz_rect *bbox, int luminosity, fz_colorspace *colorspace, float *color)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out;
	int mask = sdev->id++;

	out = start_def(sdev);
	fz_printf(out, "<mask id=\"ma%d\">", mask);

	if (dev->container_len > 0)
		dev->container[dev->container_len-1].user = mask;
}

static void
svg_dev_end_mask(fz_device *dev)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out = sdev->out;
	int mask = 0;

	if (dev->container_len > 0)
		mask = (int)dev->container[dev->container_len-1].user;

	fz_printf(out, "\"/></mask>\n");
	out = end_def(sdev);
	fz_printf(out, "<g mask=\"url(#ma%d)\">\n", mask);

}

static void
svg_dev_begin_group(fz_device *dev, const fz_rect *bbox, int isolated, int knockout, int blendmode, float alpha)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out = sdev->out;

	/* SVG 1.1 doesn't support adequate blendmodes/knockout etc, so just ignore it for now */
	fz_printf(out, "<g>\n");
}

static void
svg_dev_end_group(fz_device *dev)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out = sdev->out;

	fz_printf(out, "</g>\n");
}

static int
svg_dev_begin_tile(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out;
	fz_context *ctx = dev->ctx;
	int num;
	tile *t;

	if (sdev->num_tiles == sdev->max_tiles)
	{
		int n = (sdev->num_tiles == 0 ? 4 : sdev->num_tiles * 2);

		sdev->tiles = fz_resize_array(ctx, sdev->tiles, n, sizeof(tile));
		sdev->max_tiles = n;
	}
	num = sdev->num_tiles++;
	t = &sdev->tiles[num];
	t->area = *area;
	t->view = *view;
	t->ctm = *ctm;
	t->pattern = sdev->id++;
	t->step.x = xstep;
	t->step.y = ystep;

	/* view = area of our reference tile in pattern space.
	 * area = area to tile into in pattern space.
	 * xstep/ystep = pattern repeat step in pattern space.
	 * All of these need to be transformed by ctm to get to device space.
	 * SVG only allows us to specify pattern tiles as axis aligned
	 * rectangles, so we send these through as is, and ensure that the
	 * correct matrix is used on the fill.
	 */

	/* The first thing we do is to capture the contents of the pattern
	 * as a symbol we can reuse. */
	out = start_def(sdev);
	fz_printf(out, "<symbol id=\"pac%d\">\n", t->pattern);

	return 0;
}

static void
svg_dev_end_tile(fz_device *dev)
{
	svg_device *sdev = (svg_device *)dev->user;
	fz_output *out = sdev->out;
	int num, cp = -1;
	tile *t;
	fz_matrix inverse;
	float x, y, w, h;

	if (sdev->num_tiles == 0)
		return;
	num = --sdev->num_tiles;
	t = &sdev->tiles[num];

	fz_printf(out, "</symbol>\n");

	/* In svg, the reference tile is taken from (x,y) to (x+width,y+height)
	 * and is repeated at (x+n*width,y+m*height) for all integer n and m.
	 * This means that width and height generally correspond to xstep and
	 * ystep. There are exceptional cases where we have to break this
	 * though; when xstep/ystep are smaller than the width/height of the
	 * pattern tile, we need to render the pattern contents several times
	 * to ensure that the pattern tile contains everything. */

	fz_printf(out, "<pattern id=\"pa%d\" patternUnits=\"userSpaceOnUse\" patternContentUnits=\"userSpaceOnUse\"",
		t->pattern);
	fz_printf(out, " x=\"0\" y=\"0\" width=\"%g\" height=\"%g\">\n",
		t->step.x, t->step.y);

	if (t->view.x0 > 0 || t->step.x < t->view.x1 || t->view.y0 > 0 || t->step.y < t->view.y1)
	{
		cp = sdev->id++;
		fz_printf(out, "<clipPath id=\"cp%d\">\n", cp);
		fz_printf(out, "<path d=\"M %g %g L %g %g L %g %g L %g %g Z\"/>",
			t->view.x0, t->view.y0,
			t->view.x1, t->view.y0,
			t->view.x1, t->view.y1,
			t->view.x0, t->view.y1);
		fz_printf(out, "</clipPath>\n");
		fz_printf(out, "<g clip-path=\"url(#cp%d)\">\n", cp);
	}

	/* All the pattern contents will have their own ctm applied. Let's
	 * undo the current one to allow for this */
	fz_invert_matrix(&inverse, &t->ctm);
	fz_printf(out, "<g");
	svg_dev_ctm(sdev, &inverse);
	fz_printf(out, ">\n");

	w = t->view.x1 - t->view.x0;
	h = t->view.y1 - t->view.y0;

	for (x = 0; x > -w; x -= t->step.x)
		for (y = 0; y > -h; y -= t->step.y)
			fz_printf(out, "<use x=\"%g\" y=\"%g\" xlink:href=\"#pac%d\"/>", x, y, t->pattern);

	fz_printf(out, "</g>\n");
	if (cp != -1)
		fz_printf(out, "</g>\n");
	fz_printf(out, "</pattern>\n");
	out = end_def(sdev);

	/* Finally, fill a rectangle with the pattern. */
	fz_printf(out, "<rect");
	svg_dev_ctm(sdev, &t->ctm);
	fz_printf(out, " fill=\"url(#pa%d)\" x=\"%g\" y=\"%g\" width=\"%g\" height=\"%g\"/>\n",
		t->pattern, t->area.x0, t->area.y0, t->area.x1 - t->area.x0, t->area.y1 - t->area.y0);
}

static void
svg_dev_free_user(fz_device *dev)
{
	svg_device *sdev = dev->user;
	fz_context *ctx = sdev->ctx;
	fz_output *out = sdev->out;

	fz_free(ctx, sdev->tiles);
	fz_drop_buffer(ctx, sdev->defs_buffer);
	fz_close_output(sdev->defs);

	fz_printf(out, "</svg>\n");

	fz_free(ctx, sdev);
}

void svg_rebind(fz_device *dev)
{
	svg_device *sdev = dev->user;

	sdev->ctx = dev->ctx;
	fz_rebind_output(sdev->out, sdev->ctx);
	fz_rebind_output(sdev->out_store, sdev->ctx);
}

fz_device *fz_new_svg_device(fz_context *ctx, fz_output *out, float page_width, float page_height)
{
	svg_device *sdev = fz_malloc_struct(ctx, svg_device);
	fz_device *dev;

	fz_try(ctx)
	{
		sdev->ctx = ctx;
		sdev->out = out;
		sdev->out_store = out;
		sdev->id = 0;

		dev = fz_new_device(ctx, sdev);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, sdev);
		fz_rethrow(ctx);
	}

	dev->rebind = svg_rebind;
	dev->free_user = svg_dev_free_user;

	dev->fill_path = svg_dev_fill_path;
	dev->stroke_path = svg_dev_stroke_path;
	dev->clip_path = svg_dev_clip_path;
	dev->clip_stroke_path = svg_dev_clip_stroke_path;

	dev->fill_text = svg_dev_fill_text;
	dev->stroke_text = svg_dev_stroke_text;
	dev->clip_text = svg_dev_clip_text;
	dev->clip_stroke_text = svg_dev_clip_stroke_text;
	dev->ignore_text = svg_dev_ignore_text;

	dev->fill_shade = svg_dev_fill_shade;
	dev->fill_image = svg_dev_fill_image;
	dev->fill_image_mask = svg_dev_fill_image_mask;
	dev->clip_image_mask = svg_dev_clip_image_mask;

	dev->pop_clip = svg_dev_pop_clip;

	dev->begin_mask = svg_dev_begin_mask;
	dev->end_mask = svg_dev_end_mask;
	dev->begin_group = svg_dev_begin_group;
	dev->end_group = svg_dev_end_group;

	dev->begin_tile = svg_dev_begin_tile;
	dev->end_tile = svg_dev_end_tile;

	dev->hints |= FZ_MAINTAIN_CONTAINER_STACK;

	fz_printf(out, "<?xml version=\"1.0\" standalone=\"no\"?>\n");
	fz_printf(out, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
	fz_printf(out, "<svg xmlns=\"http://www.w3.org/2000/svg\" "
		"xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"1.1\" "
		"width=\"%gcm\" height=\"%gcm\" viewBox=\"0 0 %g %g\">\n",
		page_width*2.54/72, page_height*2.54/72, page_width, page_height);

	return dev;
}
