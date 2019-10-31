#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <string.h>

fz_device *
fz_new_device_of_size(fz_context *ctx, int size)
{
	fz_device *dev = Memento_label(fz_calloc(ctx, 1, size), "fz_device");
	dev->refs = 1;
	return dev;
}

static void
fz_disable_device(fz_context *ctx, fz_device *dev)
{
	dev->close_device = NULL;
	dev->fill_path = NULL;
	dev->stroke_path = NULL;
	dev->clip_path = NULL;
	dev->clip_stroke_path = NULL;
	dev->fill_text = NULL;
	dev->stroke_text = NULL;
	dev->clip_text = NULL;
	dev->clip_stroke_text = NULL;
	dev->ignore_text = NULL;
	dev->fill_shade = NULL;
	dev->fill_image = NULL;
	dev->fill_image_mask = NULL;
	dev->clip_image_mask = NULL;
	dev->pop_clip = NULL;
	dev->begin_mask = NULL;
	dev->end_mask = NULL;
	dev->begin_group = NULL;
	dev->end_group = NULL;
	dev->begin_tile = NULL;
	dev->end_tile = NULL;
	dev->render_flags = NULL;
	dev->set_default_colorspaces = NULL;
	dev->begin_layer = NULL;
	dev->end_layer = NULL;
}

/*
	Signal the end of input, and flush any buffered output.
	This is NOT called implicitly on fz_drop_device.
*/
void
fz_close_device(fz_context *ctx, fz_device *dev)
{
	fz_try(ctx)
	{
		if (dev->close_device)
			dev->close_device(ctx, dev);
	}
	fz_always(ctx)
		fz_disable_device(ctx, dev);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

fz_device *
fz_keep_device(fz_context *ctx, fz_device *dev)
{
	return fz_keep_imp(ctx, dev, &dev->refs);
}

/*
	Free a device of any type and its resources.
	Don't forget to call fz_close_device before dropping the device,
	or you may get incomplete output!
*/
void
fz_drop_device(fz_context *ctx, fz_device *dev)
{
	if (fz_drop_imp(ctx, dev, &dev->refs))
	{
		if (dev->close_device)
			fz_warn(ctx, "dropping unclosed device");
		if (dev->drop_device)
			dev->drop_device(ctx, dev);
		fz_free(ctx, dev->container);
		fz_free(ctx, dev);
	}
}

void
fz_enable_device_hints(fz_context *ctx, fz_device *dev, int hints)
{
	dev->hints |= hints;
}

void
fz_disable_device_hints(fz_context *ctx, fz_device *dev, int hints)
{
	dev->hints &= ~hints;
}

static void
push_clip_stack(fz_context *ctx, fz_device *dev, fz_rect rect, int type)
{
	if (dev->container_len == dev->container_cap)
	{
		int newmax = dev->container_cap * 2;
		if (newmax == 0)
			newmax = 4;
		dev->container = fz_realloc_array(ctx, dev->container, newmax, fz_device_container_stack);
		dev->container_cap = newmax;
	}
	if (dev->container_len == 0)
		dev->container[0].scissor = rect;
	else
	{
		dev->container[dev->container_len].scissor = fz_intersect_rect(dev->container[dev->container_len-1].scissor, rect);
	}
	dev->container[dev->container_len].type = type;
	dev->container[dev->container_len].user = 0;
	dev->container_len++;
}

static void
pop_clip_stack(fz_context *ctx, fz_device *dev, int type)
{
	if (dev->container_len == 0 || dev->container[dev->container_len-1].type != type)
	{
		fz_disable_device(ctx, dev);
		fz_throw(ctx, FZ_ERROR_GENERIC, "device calls unbalanced");
	}
	dev->container_len--;
}

static void
pop_push_clip_stack(fz_context *ctx, fz_device *dev, int pop_type, int push_type)
{
	if (dev->container_len == 0 || dev->container[dev->container_len-1].type != pop_type)
	{
		fz_disable_device(ctx, dev);
		fz_throw(ctx, FZ_ERROR_GENERIC, "device calls unbalanced");
	}
	dev->container[dev->container_len-1].type = push_type;
}

void
fz_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->fill_path)
	{
		fz_try(ctx)
			dev->fill_path(ctx, dev, path, even_odd, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->stroke_path)
	{
		fz_try(ctx)
			dev->stroke_path(ctx, dev, path, stroke, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_path(ctx, path, NULL, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_path)
	{
		fz_try(ctx)
			dev->clip_path(ctx, dev, path, even_odd, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_path(ctx, path, stroke, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_stroke_path)
	{
		fz_try(ctx)
			dev->clip_stroke_path(ctx, dev, path, stroke, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->fill_text)
	{
		fz_try(ctx)
			dev->fill_text(ctx, dev, text, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->stroke_text)
	{
		fz_try(ctx)
			dev->stroke_text(ctx, dev, text, stroke, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_text(ctx, text, NULL, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_text)
	{
		fz_try(ctx)
			dev->clip_text(ctx, dev, text, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_text(ctx, text, stroke, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_stroke_text)
	{
		fz_try(ctx)
			dev->clip_stroke_text(ctx, dev, text, stroke, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	if (dev->ignore_text)
	{
		fz_try(ctx)
			dev->ignore_text(ctx, dev, text, ctm);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_pop_clip(fz_context *ctx, fz_device *dev)
{
	pop_clip_stack(ctx, dev, fz_device_container_stack_is_clip);

	if (dev->pop_clip)
	{
		fz_try(ctx)
			dev->pop_clip(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	if (dev->fill_shade)
	{
		fz_try(ctx)
			dev->fill_shade(ctx, dev, shade, ctm, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	if (dev->fill_image)
	{
		fz_try(ctx)
			dev->fill_image(ctx, dev, image, ctm, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->fill_image_mask)
	{
		fz_try(ctx)
			dev->fill_image_mask(ctx, dev, image, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_transform_rect(fz_unit_rect, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_image_mask)
	{
		fz_try(ctx)
			dev->clip_image_mask(ctx, dev, image, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_begin_mask(fz_context *ctx, fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, const float *bc, fz_color_params color_params)
{
	push_clip_stack(ctx, dev, area, fz_device_container_stack_is_mask);

	if (dev->begin_mask)
	{
		fz_try(ctx)
			dev->begin_mask(ctx, dev, area, luminosity, colorspace, bc, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_end_mask(fz_context *ctx, fz_device *dev)
{
	pop_push_clip_stack(ctx, dev, fz_device_container_stack_is_mask, fz_device_container_stack_is_clip);

	if (dev->end_mask)
	{
		fz_try(ctx)
			dev->end_mask(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_begin_group(fz_context *ctx, fz_device *dev, fz_rect area, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	push_clip_stack(ctx, dev, area, fz_device_container_stack_is_group);

	if (dev->begin_group)
	{
		fz_try(ctx)
			dev->begin_group(ctx, dev, area, cs, isolated, knockout, blendmode, alpha);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_end_group(fz_context *ctx, fz_device *dev)
{
	pop_clip_stack(ctx, dev, fz_device_container_stack_is_group);

	if (dev->end_group)
	{
		fz_try(ctx)
			dev->end_group(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	(void)fz_begin_tile_id(ctx, dev, area, view, xstep, ystep, ctm, 0);
}

int
fz_begin_tile_id(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	int result = 0;

	push_clip_stack(ctx, dev, area, fz_device_container_stack_is_tile);

	if (xstep < 0)
		xstep = -xstep;
	if (ystep < 0)
		ystep = -ystep;
	if (dev->begin_tile)
	{
		fz_try(ctx)
			result = dev->begin_tile(ctx, dev, area, view, xstep, ystep, ctm, id);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}

	return result;
}

void
fz_end_tile(fz_context *ctx, fz_device *dev)
{
	pop_clip_stack(ctx, dev, fz_device_container_stack_is_tile);

	if (dev->end_tile)
	{
		fz_try(ctx)
			dev->end_tile(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_render_flags(fz_context *ctx, fz_device *dev, int set, int clear)
{
	if (dev->render_flags)
	{
		fz_try(ctx)
			dev->render_flags(ctx, dev, set, clear);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *default_cs)
{
	if (dev->set_default_colorspaces)
	{
		fz_try(ctx)
			dev->set_default_colorspaces(ctx, dev, default_cs);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_begin_layer(fz_context *ctx, fz_device *dev, const char *layer_name)
{
	if (dev->begin_layer)
	{
		fz_try(ctx)
			dev->begin_layer(ctx, dev, layer_name);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_end_layer(fz_context *ctx, fz_device *dev)
{
	if (dev->end_layer)
	{
		fz_try(ctx)
			dev->end_layer(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

/*
	Find current scissor region as tracked by the device.
*/
fz_rect
fz_device_current_scissor(fz_context *ctx, fz_device *dev)
{
	if (dev->container_len > 0)
		return dev->container[dev->container_len-1].scissor;
	return fz_infinite_rect;
}
