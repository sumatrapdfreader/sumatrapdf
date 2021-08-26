// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"

#include <assert.h>

#define STACK_SIZE 96

typedef struct fz_bbox_device_s
{
	fz_device super;

	fz_rect *result;
	int top;
	fz_rect stack[STACK_SIZE];
	/* mask content and tiles are ignored */
	int ignore;
} fz_bbox_device;

static void
fz_bbox_add_rect(fz_context *ctx, fz_device *dev, fz_rect rect, int clip)
{
	fz_bbox_device *bdev = (fz_bbox_device*)dev;

	if (0 < bdev->top && bdev->top <= STACK_SIZE)
	{
		rect = fz_intersect_rect(rect, bdev->stack[bdev->top-1]);
	}
	if (!clip && bdev->top <= STACK_SIZE && !bdev->ignore)
	{
		*bdev->result = fz_union_rect(*bdev->result, rect);
	}
	if (clip && ++bdev->top <= STACK_SIZE)
	{
		bdev->stack[bdev->top-1] = rect;
	}
}

static void
fz_bbox_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_path(ctx, path, NULL, ctm), 0);
}

static void
fz_bbox_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_path(ctx, path, stroke, ctm), 0);
}

static void
fz_bbox_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_text(ctx, text, NULL, ctm), 0);
}

static void
fz_bbox_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_text(ctx, text, stroke, ctm), 0);
}

static void
fz_bbox_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_shade(ctx, shade, ctm), 0);
}

static void
fz_bbox_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_transform_rect(fz_unit_rect, ctm), 0);
}

static void
fz_bbox_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_bbox_add_rect(ctx, dev, fz_transform_rect(fz_unit_rect, ctm), 0);
}

static void
fz_bbox_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_path(ctx, path, NULL, ctm), 1);
}

static void
fz_bbox_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_path(ctx, path, stroke, ctm), 1);
}

static void
fz_bbox_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_text(ctx, text, NULL, ctm), 1);
}

static void
fz_bbox_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_bbox_add_rect(ctx, dev, fz_bound_text(ctx, text, stroke, ctm), 1);
}

static void
fz_bbox_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
	fz_bbox_add_rect(ctx, dev, fz_transform_rect(fz_unit_rect, ctm), 1);
}

static void
fz_bbox_pop_clip(fz_context *ctx, fz_device *dev)
{
	fz_bbox_device *bdev = (fz_bbox_device*)dev;
	if (bdev->top > 0)
		bdev->top--;
	else
		fz_warn(ctx, "unexpected pop clip");
}

static void
fz_bbox_begin_mask(fz_context *ctx, fz_device *dev, fz_rect rect, int luminosity, fz_colorspace *colorspace, const float *color, fz_color_params color_params)
{
	fz_bbox_device *bdev = (fz_bbox_device*)dev;
	fz_bbox_add_rect(ctx, dev, rect, 1);
	bdev->ignore++;
}

static void
fz_bbox_end_mask(fz_context *ctx, fz_device *dev)
{
	fz_bbox_device *bdev = (fz_bbox_device*)dev;
	assert(bdev->ignore > 0);
	bdev->ignore--;
}

static void
fz_bbox_begin_group(fz_context *ctx, fz_device *dev, fz_rect rect, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	fz_bbox_add_rect(ctx, dev, rect, 1);
}

static void
fz_bbox_end_group(fz_context *ctx, fz_device *dev)
{
	fz_bbox_pop_clip(ctx, dev);
}

static int
fz_bbox_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	fz_bbox_device *bdev = (fz_bbox_device*)dev;
	fz_bbox_add_rect(ctx, dev, fz_transform_rect(area, ctm), 0);
	bdev->ignore++;
	return 0;
}

static void
fz_bbox_end_tile(fz_context *ctx, fz_device *dev)
{
	fz_bbox_device *bdev = (fz_bbox_device*)dev;
	assert(bdev->ignore > 0);
	bdev->ignore--;
}

fz_device *
fz_new_bbox_device(fz_context *ctx, fz_rect *result)
{
	fz_bbox_device *dev = fz_new_derived_device(ctx, fz_bbox_device);

	dev->super.fill_path = fz_bbox_fill_path;
	dev->super.stroke_path = fz_bbox_stroke_path;
	dev->super.clip_path = fz_bbox_clip_path;
	dev->super.clip_stroke_path = fz_bbox_clip_stroke_path;

	dev->super.fill_text = fz_bbox_fill_text;
	dev->super.stroke_text = fz_bbox_stroke_text;
	dev->super.clip_text = fz_bbox_clip_text;
	dev->super.clip_stroke_text = fz_bbox_clip_stroke_text;

	dev->super.fill_shade = fz_bbox_fill_shade;
	dev->super.fill_image = fz_bbox_fill_image;
	dev->super.fill_image_mask = fz_bbox_fill_image_mask;
	dev->super.clip_image_mask = fz_bbox_clip_image_mask;

	dev->super.pop_clip = fz_bbox_pop_clip;

	dev->super.begin_mask = fz_bbox_begin_mask;
	dev->super.end_mask = fz_bbox_end_mask;
	dev->super.begin_group = fz_bbox_begin_group;
	dev->super.end_group = fz_bbox_end_group;

	dev->super.begin_tile = fz_bbox_begin_tile;
	dev->super.end_tile = fz_bbox_end_tile;

	dev->result = result;
	dev->top = 0;
	dev->ignore = 0;

	*result = fz_empty_rect;

	return (fz_device*)dev;
}
