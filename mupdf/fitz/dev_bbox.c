#include "fitz.h"

/* TODO: add clip stack and use to intersect bboxes */

typedef struct fz_bboxdevice_s fz_bboxdevice;

struct fz_bboxdevice_s
{
	fz_bbox *bbox;
};

static void
fz_bboxfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bboxdevice *bdev = user;
	fz_bbox bbox = fz_roundrect(fz_boundpath(path, nil, ctm));
	*bdev->bbox = fz_unionbbox(*bdev->bbox, bbox);
}

static void
fz_bboxstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bboxdevice *bdev = user;
	fz_bbox bbox = fz_roundrect(fz_boundpath(path, stroke, ctm));
	*bdev->bbox = fz_unionbbox(*bdev->bbox, bbox);
}

static void
fz_bboxfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bboxdevice *bdev = user;
	fz_bbox bbox = fz_roundrect(fz_boundtext(text, ctm));
	*bdev->bbox = fz_unionbbox(*bdev->bbox, bbox);
}

static void
fz_bboxstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bboxdevice *bdev = user;
	fz_bbox bbox = fz_roundrect(fz_boundtext(text, ctm));
	*bdev->bbox = fz_unionbbox(*bdev->bbox, bbox);
}

static void
fz_bboxfillshade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_bboxdevice *bdev = user;
	fz_bbox bbox = fz_roundrect(fz_boundshade(shade, ctm));
	*bdev->bbox = fz_unionbbox(*bdev->bbox, bbox);
}

static void
fz_bboxfillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_bboxdevice *bdev = user;
	fz_bbox bbox = fz_roundrect(fz_transformrect(ctm, fz_unitrect));
	*bdev->bbox = fz_unionbbox(*bdev->bbox, bbox);
}

static void
fz_bboxfillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_bboxfillimage(user, image, ctm, alpha);
}

static void
fz_bboxfreeuser(void *user)
{
	fz_bboxdevice *bdev = user;
	fz_free(bdev);
}

fz_device *
fz_newbboxdevice(fz_bbox *bboxp)
{
	fz_device *dev;
	fz_bboxdevice *bdev = fz_malloc(sizeof(fz_bboxdevice));
	bdev->bbox = bboxp;
	*bdev->bbox = fz_emptybbox;

	dev = fz_newdevice(bdev);
	dev->freeuser = fz_bboxfreeuser;
	dev->fillpath = fz_bboxfillpath;
	dev->strokepath = fz_bboxstrokepath;
	dev->filltext = fz_bboxfilltext;
	dev->stroketext = fz_bboxstroketext;
	dev->fillshade = fz_bboxfillshade;
	dev->fillimage = fz_bboxfillimage;
	dev->fillimagemask = fz_bboxfillimagemask;
	return dev;
}
