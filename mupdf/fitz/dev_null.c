#include "fitz-internal.h"

fz_device *
fz_new_device(fz_context *ctx, void *user)
{
	fz_device *dev = fz_malloc_struct(ctx, fz_device);
	dev->hints = 0;
	dev->flags = 0;
	dev->user = user;
	dev->ctx = ctx;
	dev->error_depth = 0;
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
	if (dev->error_depth)
		return;
	if (dev->fill_path)
		dev->fill_path(dev, path, even_odd, ctm, colorspace, color, alpha);
}

void
fz_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->error_depth)
		return;
	if (dev->stroke_path)
		dev->stroke_path(dev, path, stroke, ctm, colorspace, color, alpha);
}

void
fz_clip_path(fz_device *dev, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->clip_path)
			dev->clip_path(dev, path, rect, even_odd, ctm);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_clip_stroke_path(fz_device *dev, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->clip_stroke_path)
			dev->clip_stroke_path(dev, path, rect, stroke, ctm);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->error_depth)
		return;
	if (dev->fill_text)
		dev->fill_text(dev, text, ctm, colorspace, color, alpha);
}

void
fz_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->error_depth)
		return;
	if (dev->stroke_text)
		dev->stroke_text(dev, text, stroke, ctm, colorspace, color, alpha);
}

void
fz_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		if (accumulate == 0 || accumulate == 1)
			dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->clip_text)
			dev->clip_text(dev, text, ctm, accumulate);
	}
	fz_catch(ctx)
	{
		if (accumulate == 2)
			fz_rethrow(ctx);
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->clip_stroke_text)
			dev->clip_stroke_text(dev, text, stroke, ctm);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm)
{
	if (dev->error_depth)
		return;
	if (dev->ignore_text)
		dev->ignore_text(dev, text, ctm);
}

void
fz_pop_clip(fz_device *dev)
{
	if (dev->error_depth)
	{
		dev->error_depth--;
		if (dev->error_depth == 0)
			fz_throw(dev->ctx, "%s", dev->errmess);
	}
	if (dev->pop_clip)
		dev->pop_clip(dev);
}

void
fz_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha)
{
	if (dev->error_depth)
		return;
	if (dev->fill_shade)
		dev->fill_shade(dev, shade, ctm, alpha);
}

void
fz_fill_image(fz_device *dev, fz_image *image, fz_matrix ctm, float alpha)
{
	if (dev->error_depth)
		return;
	if (dev->fill_image)
		dev->fill_image(dev, image, ctm, alpha);
}

void
fz_fill_image_mask(fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	if (dev->error_depth)
		return;
	if (dev->fill_image_mask)
		dev->fill_image_mask(dev, image, ctm, colorspace, color, alpha);
}

void
fz_clip_image_mask(fz_device *dev, fz_image *image, fz_rect *rect, fz_matrix ctm)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->clip_image_mask)
			dev->clip_image_mask(dev, image, rect, ctm);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_begin_mask(fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, float *bc)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->begin_mask)
			dev->begin_mask(dev, area, luminosity, colorspace, bc);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_end_mask(fz_device *dev)
{
	if (dev->error_depth)
	{
		/* Converts from mask to clip, so no change in stack depth */
		return;
	}
	if (dev->end_mask)
		dev->end_mask(dev);
}

void
fz_begin_group(fz_device *dev, fz_rect area, int isolated, int knockout, int blendmode, float alpha)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->begin_group)
			dev->begin_group(dev, area, isolated, knockout, blendmode, alpha);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_end_group(fz_device *dev)
{
	if (dev->error_depth)
	{
		dev->error_depth--;
		if (dev->error_depth == 0)
			fz_throw(dev->ctx, "%s", dev->errmess);
	}
	if (dev->end_group)
		dev->end_group(dev);
}

void
fz_begin_tile(fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	fz_context *ctx = dev->ctx;

	if (dev->error_depth)
	{
		dev->error_depth++;
		return;
	}

	fz_try(ctx)
	{
		if (dev->begin_tile)
			dev->begin_tile(dev, area, view, xstep, ystep, ctm);
	}
	fz_catch(ctx)
	{
		dev->error_depth = 1;
		strcpy(dev->errmess, fz_caught(ctx));
		/* Error swallowed */
	}
}

void
fz_end_tile(fz_device *dev)
{
	if (dev->error_depth)
	{
		dev->error_depth--;
		if (dev->error_depth == 0)
			fz_throw(dev->ctx, "%s", dev->errmess);
	}
	if (dev->end_tile)
		dev->end_tile(dev);
}

/* SumatraPDF: support transfer functions */
void
fz_apply_transfer_function(fz_device *dev, fz_transfer_function *tr, int for_mask)
{
	if (dev->apply_transfer_function)
		dev->apply_transfer_function(dev, tr, for_mask);
}
