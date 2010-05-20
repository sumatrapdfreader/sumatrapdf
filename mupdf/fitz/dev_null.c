#include "fitz.h"

static void fz_nullfreeuser(void *user) {}
static void fz_nullfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha) {}
static void fz_nullstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha) {}
static void fz_nullclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm) {}
static void fz_nullclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm) {}
static void fz_nullfilltext(void *user, fz_text *text, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha) {}
static void fz_nullstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha) {}
static void fz_nullcliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate) {}
static void fz_nullclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm) {}
static void fz_nullignoretext(void *user, fz_text *text, fz_matrix ctm) {}
static void fz_nullpopclip(void *user) {}
static void fz_nullfillshade(void *user, fz_shade *shade, fz_matrix ctm) {}
static void fz_nullfillimage(void *user, fz_pixmap *image, fz_matrix ctm) {}
static void fz_nullfillimagemask(void *user, fz_pixmap *image, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha) {}
static void fz_nullclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm) {}

fz_device *
fz_newdevice(void *user)
{
	fz_device *dev = fz_malloc(sizeof(fz_device));
	memset(dev, 0, sizeof(fz_device));

	dev->user = user;
	dev->freeuser = fz_nullfreeuser;

	dev->fillpath = fz_nullfillpath;
	dev->strokepath = fz_nullstrokepath;
	dev->clippath = fz_nullclippath;
	dev->clipstrokepath = fz_nullclipstrokepath;

	dev->filltext = fz_nullfilltext;
	dev->stroketext = fz_nullstroketext;
	dev->cliptext = fz_nullcliptext;
	dev->clipstroketext = fz_nullclipstroketext;
	dev->ignoretext = fz_nullignoretext;

	dev->fillshade = fz_nullfillshade;
	dev->fillimage = fz_nullfillimage;
	dev->fillimagemask = fz_nullfillimagemask;
	dev->clipimagemask = fz_nullclipimagemask;

	dev->popclip = fz_nullpopclip;

	return dev;
}

void
fz_freedevice(fz_device *dev)
{
	if (dev->freeuser)
		dev->freeuser(dev->user);
	fz_free(dev);
}

