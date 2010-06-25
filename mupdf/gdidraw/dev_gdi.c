#include "gdidraw.h"

static void
fz_gdifillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdistrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdiclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
}

static void
fz_gdiclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
}

static void
fz_gdifilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdistroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdicliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
}

static void
fz_gdiclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
}

static void
fz_gdiignoretext(void *user, fz_text *text, fz_matrix ctm)
{
}

static void
fz_gdipopclip(void *user)
{
}

static void
fz_gdifillshade(void *user, fz_shade *shade, fz_matrix ctm)
{
}

static void
fz_gdifillimage(void *user, fz_pixmap *image, fz_matrix ctm)
{
}

static void
fz_gdifillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdiclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
}

fz_device *
fz_newgdidevice(HDC hDC)
{
	fz_device *dev = fz_newdevice(hDC);

	dev->fillpath = fz_gdifillpath;
	dev->strokepath = fz_gdistrokepath;
	dev->clippath = fz_gdiclippath;
	dev->clipstrokepath = fz_gdiclipstrokepath;

	dev->filltext = fz_gdifilltext;
	dev->stroketext = fz_gdistroketext;
	dev->cliptext = fz_gdicliptext;
	dev->clipstroketext = fz_gdiclipstroketext;
	dev->ignoretext = fz_gdiignoretext;

	dev->fillshade = fz_gdifillshade;
	dev->fillimage = fz_gdifillimage;
	dev->fillimagemask = fz_gdifillimagemask;
	dev->clipimagemask = fz_gdiclipimagemask;

	dev->popclip = fz_gdipopclip;

	return dev;
}
