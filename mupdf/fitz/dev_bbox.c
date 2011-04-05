#include "fitz.h"

/* TODO: add clip stack and use to intersect bboxes */

typedef struct fz_bbox_device_s fz_bbox_device;

struct fz_bbox_device_s
{
	fz_bbox *bbox;
};

static void
fz_bbox_fill_path(void *user, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bbox_device *bdev = user;
	fz_bbox bbox = fz_round_rect(fz_bound_path(path, NULL, ctm));
	*bdev->bbox = fz_union_bbox(*bdev->bbox, bbox);
}

static void
fz_bbox_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bbox_device *bdev = user;
	fz_bbox bbox = fz_round_rect(fz_bound_path(path, stroke, ctm));
	*bdev->bbox = fz_union_bbox(*bdev->bbox, bbox);
}

static void
fz_bbox_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bbox_device *bdev = user;
	fz_bbox bbox = fz_round_rect(fz_bound_text(text, ctm));
	*bdev->bbox = fz_union_bbox(*bdev->bbox, bbox);
}

static void
fz_bbox_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bbox_device *bdev = user;
	fz_bbox bbox = fz_round_rect(fz_bound_text(text, ctm));
	*bdev->bbox = fz_union_bbox(*bdev->bbox, bbox);
}

static void
fz_bbox_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_bbox_device *bdev = user;
	fz_bbox bbox = fz_round_rect(fz_bound_shade(shade, ctm));
	*bdev->bbox = fz_union_bbox(*bdev->bbox, bbox);
}

static void
fz_bbox_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_bbox_device *bdev = user;
	fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm, fz_unit_rect));
	*bdev->bbox = fz_union_bbox(*bdev->bbox, bbox);
}

static void
fz_bbox_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bbox_fill_image(user, image, ctm, alpha);
}

static void
fz_bbox_free_user(void *user)
{
	fz_bbox_device *bdev = user;
	fz_free(bdev);
}

fz_device *
fz_new_bbox_device(fz_bbox *bboxp)
{
	fz_device *dev;
	fz_bbox_device *bdev = fz_malloc(sizeof(fz_bbox_device));
	bdev->bbox = bboxp;
	*bdev->bbox = fz_empty_bbox;

	dev = fz_new_device(bdev);
	dev->free_user = fz_bbox_free_user;
	dev->fill_path = fz_bbox_fill_path;
	dev->stroke_path = fz_bbox_stroke_path;
	dev->fill_text = fz_bbox_fill_text;
	dev->stroke_text = fz_bbox_stroke_text;
	dev->fill_shade = fz_bbox_fill_shade;
	dev->fill_image = fz_bbox_fill_image;
	dev->fill_image_mask = fz_bbox_fill_image_mask;
	return dev;
}
