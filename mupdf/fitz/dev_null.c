#include "fitz-internal.h"

fz_device *
fz_new_device(fz_context *ctx, void *user)
{
	fz_device *dev = fz_malloc_struct(ctx, fz_device);
	dev->hints = 0;
	dev->flags = 0;
	dev->user = user;
	dev->ctx = ctx;
	return dev;
}

void
fz_free_device(fz_device *dev)
{
	if (dev == NULL)
		return;
	if (dev->free_user)
		dev->free_user(dev);
	fz_free(dev->ctx, dev);
}

void
fz_fill_path(fz_device *dev, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->fill_path)
		dev->fill_path(dev, path, even_odd, ctm, colorspace, color, alpha);
}

void
fz_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->stroke_path)
		dev->stroke_path(dev, path, stroke, ctm, colorspace, color, alpha);
}

void
fz_clip_path(fz_device *dev, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
	if (dev->clip_path)
		dev->clip_path(dev, path, rect, even_odd, ctm);
}

void
fz_clip_stroke_path(fz_device *dev, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	if (dev->clip_stroke_path)
		dev->clip_stroke_path(dev, path, rect, stroke, ctm);
}

void
fz_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->fill_text)
		dev->fill_text(dev, text, ctm, colorspace, color, alpha);
}

void
fz_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->stroke_text)
		dev->stroke_text(dev, text, stroke, ctm, colorspace, color, alpha);
}

void
fz_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate)
{
	if (dev->clip_text)
		dev->clip_text(dev, text, ctm, accumulate);
}

void
fz_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	if (dev->clip_stroke_text)
		dev->clip_stroke_text(dev, text, stroke, ctm);
}

void
fz_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm)
{
	if (dev->ignore_text)
		dev->ignore_text(dev, text, ctm);
}

void
fz_pop_clip(fz_device *dev)
{
	if (dev->pop_clip)
		dev->pop_clip(dev);
}

void
fz_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha)
{
	if (dev->fill_shade)
		dev->fill_shade(dev, shade, ctm, alpha);
}

void
fz_fill_image(fz_device *dev, fz_image *image, fz_matrix ctm, float alpha)
{
	if (dev->fill_image)
		dev->fill_image(dev, image, ctm, alpha);
}

void
fz_fill_image_mask(fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->fill_image_mask)
		dev->fill_image_mask(dev, image, ctm, colorspace, color, alpha);
}

void
fz_clip_image_mask(fz_device *dev, fz_image *image, fz_rect *rect, fz_matrix ctm)
{
	if (dev->clip_image_mask)
		dev->clip_image_mask(dev, image, rect, ctm);
}

void
fz_begin_mask(fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, float *bc)
{
	if (dev->begin_mask)
		dev->begin_mask(dev, area, luminosity, colorspace, bc);
}

void
fz_end_mask(fz_device *dev)
{
	if (dev->end_mask)
		dev->end_mask(dev);
}

void
fz_begin_group(fz_device *dev, fz_rect area, int isolated, int knockout, int blendmode, float alpha)
{
	if (dev->begin_group)
		dev->begin_group(dev, area, isolated, knockout, blendmode, alpha);
}

void
fz_end_group(fz_device *dev)
{
	if (dev->end_group)
		dev->end_group(dev);
}

void
fz_begin_tile(fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	if (dev->begin_tile)
		dev->begin_tile(dev, area, view, xstep, ystep, ctm);
}

void
fz_end_tile(fz_device *dev)
{
	if (dev->end_tile)
		dev->end_tile(dev);
}
