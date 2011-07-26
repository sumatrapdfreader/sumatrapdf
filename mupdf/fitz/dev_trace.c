#include "fitz.h"

static void
fz_trace_matrix(fz_matrix ctm)
{
	printf("matrix=\"%g %g %g %g %g %g\" ",
		ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f);
}

static void
fz_trace_color(fz_colorspace *colorspace, float *color, float alpha)
{
	int i;
	printf("colorspace=\"%s\" color=\"", colorspace->name);
	for (i = 0; i < colorspace->n; i++)
		printf("%s%g", i == 0 ? "" : " ", color[i]);
	printf("\" ");
	if (alpha < 1)
		printf("alpha=\"%g\" ", alpha);
}

static void
fz_trace_path(fz_path *path, int indent)
{
	float x, y;
	int i = 0;
	int n;
	while (i < path->len)
	{
		for (n = 0; n < indent; n++)
			putchar(' ');
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			printf("<moveto x=\"%g\" y=\"%g\" />\n", x, y);
			break;
		case FZ_LINETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			printf("<lineto x=\"%g\" y=\"%g\" />\n", x, y);
			break;
		case FZ_CURVETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			printf("<curveto x1=\"%g\" y1=\"%g\" ", x, y);
			x = path->items[i++].v;
			y = path->items[i++].v;
			printf("x2=\"%g\" y2=\"%g\" ", x, y);
			x = path->items[i++].v;
			y = path->items[i++].v;
			printf("x3=\"%g\" y3=\"%g\" />\n", x, y);
			break;
		case FZ_CLOSE_PATH:
			printf("<closepath />\n");
			break;
		}
	}
}

static void
fz_trace_fill_path(void *user, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<fill_path ");
	if (even_odd)
		printf("winding=\"eofill\" ");
	else
		printf("winding=\"nonzero\" ");
	fz_trace_color(colorspace, color, alpha);
	fz_trace_matrix(ctm);
	printf(">\n");
	fz_trace_path(path, 0);
	printf("</fill_path>\n");
}

static void
fz_trace_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	int i;

	printf("<stroke_path ");
	printf("linewidth=\"%g\" ", stroke->linewidth);
	printf("miterlimit=\"%g\" ", stroke->miterlimit);
	printf("linecap=\"%d,%d,%d\" ", stroke->start_cap, stroke->dash_cap, stroke->end_cap);
	printf("linejoin=\"%d\" ", stroke->linejoin);

	if (stroke->dash_len)
	{
		printf("dash_phase=\"%g\" dash=\"", stroke->dash_phase);
		for (i = 0; i < stroke->dash_len; i++)
			printf("%g ", stroke->dash_list[i]);
		printf("\"");
	}

	fz_trace_color(colorspace, color, alpha);
	fz_trace_matrix(ctm);
	printf(">\n");

	fz_trace_path(path, 0);

	printf("</stroke_path>\n");
}

static void
fz_trace_clip_path(void *user, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
	printf("<clip_path ");
	if (even_odd)
		printf("winding=\"eofill\" ");
	else
		printf("winding=\"nonzero\" ");
	fz_trace_matrix(ctm);
	printf("contentbbox=\"%g %g %g %g\">\n",
		rect->x0, rect->y0, rect->x1, rect->y1);
	fz_trace_path(path, 0);
	printf("</clip_path>\n");
}

static void
fz_trace_clip_stroke_path(void *user, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	printf("<clip_stroke_path ");
	fz_trace_matrix(ctm);
	printf(">\n");
	fz_trace_path(path, 0);
	printf("</clip_stroke_path>\n");
}

static void
fz_trace_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<fill_text font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_trace_color(colorspace, color, alpha);
	fz_trace_matrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debug_text(text, 0);
	printf("</fill_text>\n");
}

static void
fz_trace_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<stroke_text font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_trace_color(colorspace, color, alpha);
	fz_trace_matrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debug_text(text, 0);
	printf("</stroke_text>\n");
}

static void
fz_trace_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	printf("<clip_text font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	printf("accumulate=\"%d\" ", accumulate);
	fz_trace_matrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debug_text(text, 0);
	printf("</clip_text>\n");
}

static void
fz_trace_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	printf("<clip_stroke_text font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_trace_matrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debug_text(text, 0);
	printf("</clip_stroke_text>\n");
}

static void
fz_trace_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
	printf("<ignore_text font=\"%s\" wmode=\"%d\" ", text->font->name, text->wmode);
	fz_trace_matrix(fz_concat(ctm, text->trm));
	printf(">\n");
	fz_debug_text(text, 0);
	printf("</ignore_text>\n");
}

static void
fz_trace_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	printf("<fill_image alpha=\"%g\" ", alpha);
	fz_trace_matrix(ctm);
	printf("/>\n");
}

static void
fz_trace_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	printf("<fill_shade alpha=\"%g\" ", alpha);
	fz_trace_matrix(ctm);
	printf("/>\n");
}

static void
fz_trace_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
fz_colorspace *colorspace, float *color, float alpha)
{
	printf("<fill_image_mask ");
	fz_trace_matrix(ctm);
	fz_trace_color(colorspace, color, alpha);
	printf("/>\n");
}

static void
fz_trace_clip_image_mask(void *user, fz_pixmap *image, fz_rect *rect, fz_matrix ctm)
{
	printf("<clip_image_mask ");
	fz_trace_matrix(ctm);
	printf("/>\n");
}

static void
fz_trace_pop_clip(void *user)
{
	printf("<pop_clip />\n");
}

static void
fz_trace_begin_mask(void *user, fz_rect bbox, int luminosity, fz_colorspace *colorspace, float *color)
{
	printf("<mask bbox=\"%g %g %g %g\" s=\"%s\" ",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		luminosity ? "luminosity" : "alpha");
//	fz_trace_color(colorspace, color, 1);
	printf(">\n");
}

static void
fz_trace_end_mask(void *user)
{
	printf("</mask>\n");
}

static void
fz_trace_begin_group(void *user, fz_rect bbox, int isolated, int knockout, int blendmode, float alpha)
{
	printf("<group bbox=\"%g %g %g %g\" isolated=\"%d\" knockout=\"%d\" blendmode=\"%s\" alpha=\"%g\">\n",
		bbox.x0, bbox.y0, bbox.x1, bbox.y1,
		isolated, knockout, fz_blendmode_name(blendmode), alpha);
}

static void
fz_trace_end_group(void *user)
{
	printf("</group>\n");
}

static void
fz_trace_begin_tile(void *user, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	printf("<tile ");
	printf("area=\"%g %g %g %g\" ", area.x0, area.y0, area.x1, area.y1);
	printf("view=\"%g %g %g %g\" ", view.x0, view.y0, view.x1, view.y1);
	printf("xstep=\"%g\" ystep=\"%g\" ", xstep, ystep);
	fz_trace_matrix(ctm);
	printf(">\n");
}

static void
fz_trace_end_tile(void *user)
{
	printf("</tile>\n");
}

fz_device *fz_new_trace_device(void)
{
	fz_device *dev = fz_new_device(NULL);

	dev->fill_path = fz_trace_fill_path;
	dev->stroke_path = fz_trace_stroke_path;
	dev->clip_path = fz_trace_clip_path;
	dev->clip_stroke_path = fz_trace_clip_stroke_path;

	dev->fill_text = fz_trace_fill_text;
	dev->stroke_text = fz_trace_stroke_text;
	dev->clip_text = fz_trace_clip_text;
	dev->clip_stroke_text = fz_trace_clip_stroke_text;
	dev->ignore_text = fz_trace_ignore_text;

	dev->fill_shade = fz_trace_fill_shade;
	dev->fill_image = fz_trace_fill_image;
	dev->fill_image_mask = fz_trace_fill_image_mask;
	dev->clip_image_mask = fz_trace_clip_image_mask;

	dev->pop_clip = fz_trace_pop_clip;

	dev->begin_mask = fz_trace_begin_mask;
	dev->end_mask = fz_trace_end_mask;
	dev->begin_group = fz_trace_begin_group;
	dev->end_group = fz_trace_end_group;

	dev->begin_tile = fz_trace_begin_tile;
	dev->end_tile = fz_trace_end_tile;

	return dev;
}
