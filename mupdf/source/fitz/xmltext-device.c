#include "mupdf/fitz.h"


static int s_xml_starttag_begin(fz_context *ctx, fz_output *out, const char* id)
{
	fz_write_printf(ctx, out, "<%s", id);
	return 0;
}

static int s_xml_starttag_end(fz_context *ctx, fz_output *out)
{
	fz_write_printf(ctx, out, ">\n");
	return 0;
}

static int s_xml_starttag_empty_end(fz_context *ctx, fz_output *out)
{
	fz_write_printf(ctx, out, "/>\n");
	return 0;
}

static int s_xml_endtag(fz_context *ctx, fz_output *out, const char* id)
{
	fz_write_printf(ctx, out, "</%s>\n", id);
	return 0;
}

static int s_write_attribute_int(fz_context *ctx, fz_output *out, const char* id, int value)
{
	fz_write_printf(ctx, out, " %s=\"%i\"", id, value);
	return 0;
}

static int s_write_attribute_float(fz_context *ctx, fz_output *out, const char* id, float value)
{
	fz_write_printf(ctx, out, " %s=\"%f\"", id, value);
	return 0;
}

static int s_write_attribute_string(fz_context *ctx, fz_output *out, const char* id, const char* value)
{
	fz_write_printf(ctx, out, " %s=\"%s\"", id, value);
	return 0;
}

static int s_write_attribute_char(fz_context *ctx, fz_output *out, const char* id, char value)
{
	if (value == '"') fz_write_printf(ctx, out, " %s=\"\\%c\"", id, value);
	else fz_write_printf(ctx, out, " %s=\"%c\"", id, value);
	return 0;
}

static int s_write_attribute_matrix(fz_context *ctx, fz_output *out, const char* id, const fz_matrix* matrix)
{
	fz_write_printf(ctx, out,
		" %s=\"%f %f %f %f %f %f\"",
		id,
		matrix->a,
		matrix->b,
		matrix->c,
		matrix->d,
		matrix->e,
		matrix->f
		);
	return 0;
}




typedef struct
{
	fz_device super;
	fz_output *out;
} fz_xmltext_device;

static void
fz_xmltext_fill_path(fz_context *ctx, fz_device *dev_, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
}

static void
fz_xmltext_stroke_path(fz_context *ctx, fz_device *dev_, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
}

static void
fz_xmltext_clip_path(fz_context *ctx, fz_device *dev_, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
}

static void
fz_xmltext_clip_stroke_path(fz_context *ctx, fz_device *dev_, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
}

static void
fz_xmltext_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_xmltext_device *dev = (fz_xmltext_device*) dev_;

	fz_text_span *span;
	for (span = text->head; span; span = span->next) {
		int i;

		s_xml_starttag_begin(ctx, dev->out, "span");
		s_write_attribute_matrix(ctx, dev->out, "ctm", &ctm);
		s_write_attribute_string(ctx, dev->out, "font_name", span->font->name);
		if (span->font->flags.is_mono)          s_write_attribute_int(ctx, dev->out, "is_mono", 1);
		if (span->font->flags.is_serif)         s_write_attribute_int(ctx, dev->out, "is_serif", 1);
		if (span->font->flags.is_italic)        s_write_attribute_int(ctx, dev->out, "is_italic", 1);
		if (span->font->flags.ft_substitute)    s_write_attribute_int(ctx, dev->out, "ft_substitute", 1);
		if (span->font->flags.ft_stretch)       s_write_attribute_int(ctx, dev->out, "ft_stretch", 1);
		if (span->font->flags.fake_bold)        s_write_attribute_int(ctx, dev->out, "fake_bold", 1);
		if (span->font->flags.fake_italic)      s_write_attribute_int(ctx, dev->out, "fake_italic", 1);
		if (span->font->flags.has_opentype)     s_write_attribute_int(ctx, dev->out, "has_opentype", 1);
		if (span->font->flags.invalid_bbox)     s_write_attribute_int(ctx, dev->out, "invalid_bbox", 1);
		s_write_attribute_matrix(ctx, dev->out, "trm", &span->trm);
		s_write_attribute_int(ctx, dev->out, "len", span->len);
		s_write_attribute_int(ctx, dev->out, "wmode", span->wmode);
		s_write_attribute_int(ctx, dev->out, "bidi_level", span->bidi_level);
		s_write_attribute_int(ctx, dev->out, "markup_dir", span->markup_dir);
		s_write_attribute_int(ctx, dev->out, "language", span->language);
		s_write_attribute_int(ctx, dev->out, "cap", span->cap);
		s_xml_starttag_end(ctx, dev->out);

		for (i=0; i<span->len; ++i) {
			fz_text_item*   item = &span->items[i];
			float adv = 0;
			if (span->items[i].gid >= 0) {
				adv = fz_advance_glyph(ctx, span->font, span->items[i].gid, span->wmode);
			}
			s_xml_starttag_begin(ctx, dev->out, "char");
			s_write_attribute_float(ctx, dev->out, "x", item->x);
			s_write_attribute_float(ctx, dev->out, "y", item->y);
			s_write_attribute_int(ctx, dev->out, "gid", item->gid);
			s_write_attribute_int(ctx, dev->out, "ucs", item->ucs);

			/* Firefox complains if we put special characters here; it's only for debugging
			so this isn't really a problem. */
			s_write_attribute_char(ctx, dev->out, "debug_char",
				(item->ucs >= 32 && item->ucs < 128 && item->ucs != '"')
					? item->ucs : ' '
				);
			s_write_attribute_float(ctx, dev->out, "adv", adv);
			s_xml_starttag_empty_end(ctx, dev->out);
		}

		s_xml_endtag(ctx, dev->out, "span");
	}
}

static void
fz_xmltext_fill_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_xmltext_text(ctx, dev_, text, ctm, colorspace, color, alpha, color_params);
}

static void
fz_xmltext_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_xmltext_text(ctx, dev_, text, ctm, colorspace, color, alpha, color_params);
}

static void
fz_xmltext_clip_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_color_params color_params = {0};
	fz_xmltext_text(ctx, dev_, text, ctm, NULL, NULL, 0 /*alpha*/, color_params);
}

static void
fz_xmltext_clip_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_color_params color_params = {0};
	fz_xmltext_text(ctx, dev_, text, ctm, NULL, 0, 0, color_params);
}

static void
fz_xmltext_ignore_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm)
{
}

static void
fz_xmltext_fill_image(fz_context *ctx, fz_device *dev_, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
}

static void
fz_xmltext_fill_shade(fz_context *ctx, fz_device *dev_, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
}

static void
fz_xmltext_fill_image_mask(fz_context *ctx, fz_device *dev_, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
}

static void
fz_xmltext_clip_image_mask(fz_context *ctx, fz_device *dev_, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
}

static void
fz_xmltext_pop_clip(fz_context *ctx, fz_device *dev_)
{
}

static void
fz_xmltext_begin_mask(fz_context *ctx, fz_device *dev_, fz_rect bbox, int luminosity, fz_colorspace *colorspace, const float *color, fz_color_params color_params)
{
}

static void
fz_xmltext_end_mask(fz_context *ctx, fz_device *dev_)
{
}

static void
fz_xmltext_begin_group(fz_context *ctx, fz_device *dev_, fz_rect bbox, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
}

static void
fz_xmltext_end_group(fz_context *ctx, fz_device *dev_)
{
}

static int
fz_xmltext_begin_tile(fz_context *ctx, fz_device *dev_, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	return 0;
}

static void
fz_xmltext_end_tile(fz_context *ctx, fz_device *dev_)
{
}

static void
fz_xmltext_begin_layer(fz_context *ctx, fz_device *dev_, const char *name)
{
}

static void
fz_xmltext_end_layer(fz_context *ctx, fz_device *dev_)
{
}

static void
fz_xmltext_render_flags(fz_context *ctx, fz_device *dev_, int set, int clear)
{
}

static void
fz_xmltext_set_default_colorspaces(fz_context *ctx, fz_device *dev_, fz_default_colorspaces *dcs)
{
}

static void
fz_stext_close_device(fz_context *ctx, fz_device *dev_)
{
}


fz_device *fz_new_xmltext_device(fz_context *ctx, fz_output *out)
{
	static int page_number = 0;
	fz_xmltext_device *dev = fz_new_derived_device(ctx, fz_xmltext_device);

	dev->super.close_device = fz_stext_close_device;

	dev->super.fill_path = fz_xmltext_fill_path;
	dev->super.stroke_path = fz_xmltext_stroke_path;
	dev->super.clip_path = fz_xmltext_clip_path;
	dev->super.clip_stroke_path = fz_xmltext_clip_stroke_path;

	dev->super.fill_text = fz_xmltext_fill_text;
	dev->super.stroke_text = fz_xmltext_stroke_text;
	dev->super.clip_text = fz_xmltext_clip_text;
	dev->super.clip_stroke_text = fz_xmltext_clip_stroke_text;
	dev->super.ignore_text = fz_xmltext_ignore_text;

	dev->super.fill_shade = fz_xmltext_fill_shade;
	dev->super.fill_image = fz_xmltext_fill_image;
	dev->super.fill_image_mask = fz_xmltext_fill_image_mask;
	dev->super.clip_image_mask = fz_xmltext_clip_image_mask;

	dev->super.pop_clip = fz_xmltext_pop_clip;

	dev->super.begin_mask = fz_xmltext_begin_mask;
	dev->super.end_mask = fz_xmltext_end_mask;
	dev->super.begin_group = fz_xmltext_begin_group;
	dev->super.end_group = fz_xmltext_end_group;

	dev->super.begin_tile = fz_xmltext_begin_tile;
	dev->super.end_tile = fz_xmltext_end_tile;

	dev->super.begin_layer = fz_xmltext_begin_layer;
	dev->super.end_layer = fz_xmltext_end_layer;

	dev->super.render_flags = fz_xmltext_render_flags;
	dev->super.set_default_colorspaces = fz_xmltext_set_default_colorspaces;

	dev->out = out;
	page_number += 1;

	return (fz_device*)dev;
}
