#include "mupdf/fitz.h"

#define STACK_SIZE 96

typedef struct fz_bbox_data_s
{
	fz_rect *result;
	int top;
	fz_rect stack[STACK_SIZE];
	/* mask content and tiles are ignored */
	int ignore;
} fz_bbox_data;

static void
fz_bbox_add_rect(fz_device *dev, const fz_rect *rect, int clip)
{
	fz_bbox_data *data = dev->user;
	fz_rect r = *rect;

	if (0 < data->top && data->top <= STACK_SIZE)
	{
		fz_intersect_rect(&r, &data->stack[data->top-1]);
	}
	if (!clip && data->top <= STACK_SIZE && !data->ignore)
	{
		fz_union_rect(data->result, &r);
	}
	if (clip && ++data->top <= STACK_SIZE)
	{
		data->stack[data->top-1] = r;
	}
}

static void
fz_bbox_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_path(dev->ctx, path, NULL, ctm, &r), 0);
}

static void
fz_bbox_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke,
	const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_path(dev->ctx, path, stroke, ctm, &r), 0);
}

static void
fz_bbox_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_text(dev->ctx, text, NULL, ctm, &r), 0);
}

static void
fz_bbox_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke,
	const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_text(dev->ctx, text, stroke, ctm, &r), 0);
}

static void
fz_bbox_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_shade(dev->ctx, shade, ctm, &r), 0);
}

static void
fz_bbox_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha)
{
	fz_rect r = fz_unit_rect;
	fz_bbox_add_rect(dev, fz_transform_rect(&r, ctm), 0);
}

static void
fz_bbox_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_rect r = fz_unit_rect;
	fz_bbox_add_rect(dev, fz_transform_rect(&r, ctm), 0);
}

static void
fz_bbox_clip_path(fz_device *dev, fz_path *path, const fz_rect *rect, int even_odd, const fz_matrix *ctm)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_path(dev->ctx, path, NULL, ctm, &r), 1);
}

static void
fz_bbox_clip_stroke_path(fz_device *dev, fz_path *path, const fz_rect *rect, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_path(dev->ctx, path, stroke, ctm, &r), 1);
}

static void
fz_bbox_clip_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, int accumulate)
{
	fz_rect r = fz_infinite_rect;
	if (accumulate)
		fz_bbox_add_rect(dev, &r, accumulate != 2);
	else
		fz_bbox_add_rect(dev, fz_bound_text(dev->ctx, text, NULL, ctm, &r), 1);
}

static void
fz_bbox_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	fz_rect r;
	fz_bbox_add_rect(dev, fz_bound_text(dev->ctx, text, stroke, ctm, &r), 1);
}

static void
fz_bbox_clip_image_mask(fz_device *dev, fz_image *image, const fz_rect *rect, const fz_matrix *ctm)
{
	fz_rect r = *rect;
	fz_bbox_add_rect(dev, fz_transform_rect(&r, ctm), 1);
}

static void
fz_bbox_pop_clip(fz_device *dev)
{
	fz_bbox_data *data = dev->user;
	if (data->top > 0)
		data->top--;
	else
		fz_warn(dev->ctx, "unexpected pop clip");
}

static void
fz_bbox_begin_mask(fz_device *dev, const fz_rect *rect, int luminosity, fz_colorspace *colorspace, float *color)
{
	fz_bbox_data *data = dev->user;
	fz_bbox_add_rect(dev, rect, 1);
	data->ignore++;
}

static void
fz_bbox_end_mask(fz_device *dev)
{
	fz_bbox_data *data = dev->user;
	assert(data->ignore > 0);
	data->ignore--;
}

static void
fz_bbox_begin_group(fz_device *dev, const fz_rect *rect, int isolated, int knockout, int blendmode, float alpha)
{
	fz_bbox_add_rect(dev, rect, 1);
}

static void
fz_bbox_end_group(fz_device *dev)
{
	fz_bbox_pop_clip(dev);
}

static int
fz_bbox_begin_tile(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id)
{
	fz_bbox_data *data = dev->user;
	fz_rect r = *area;
	fz_bbox_add_rect(dev, fz_transform_rect(&r, ctm), 0);
	data->ignore++;
	return 0;
}

static void
fz_bbox_end_tile(fz_device *dev)
{
	fz_bbox_data *data = dev->user;
	assert(data->ignore > 0);
	data->ignore--;
}

static void
fz_bbox_free_user(fz_device *dev)
{
	fz_bbox_data *data = dev->user;
	if (data->top > 0)
		fz_warn(dev->ctx, "items left on stack in bbox device: %d", data->top);
	fz_free(dev->ctx, dev->user);
}

fz_device *
fz_new_bbox_device(fz_context *ctx, fz_rect *result)
{
	fz_device *dev;

	fz_bbox_data *user = fz_malloc_struct(ctx, fz_bbox_data);
	user->result = result;
	user->top = 0;
	user->ignore = 0;
	dev = fz_new_device(ctx, user);
	dev->free_user = fz_bbox_free_user;

	dev->fill_path = fz_bbox_fill_path;
	dev->stroke_path = fz_bbox_stroke_path;
	dev->clip_path = fz_bbox_clip_path;
	dev->clip_stroke_path = fz_bbox_clip_stroke_path;

	dev->fill_text = fz_bbox_fill_text;
	dev->stroke_text = fz_bbox_stroke_text;
	dev->clip_text = fz_bbox_clip_text;
	dev->clip_stroke_text = fz_bbox_clip_stroke_text;

	dev->fill_shade = fz_bbox_fill_shade;
	dev->fill_image = fz_bbox_fill_image;
	dev->fill_image_mask = fz_bbox_fill_image_mask;
	dev->clip_image_mask = fz_bbox_clip_image_mask;

	dev->pop_clip = fz_bbox_pop_clip;

	dev->begin_mask = fz_bbox_begin_mask;
	dev->end_mask = fz_bbox_end_mask;
	dev->begin_group = fz_bbox_begin_group;
	dev->end_group = fz_bbox_end_group;

	dev->begin_tile = fz_bbox_begin_tile;
	dev->end_tile = fz_bbox_end_tile;

	*result = fz_empty_rect;

	return dev;
}
