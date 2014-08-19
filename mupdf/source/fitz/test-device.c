#include <mupdf/fitz.h>

static void
fz_test_colorspace(fz_context *ctx, fz_colorspace *colorspace, int *iscolor)
{
	if (colorspace && colorspace != fz_device_gray(ctx))
		*iscolor = 1;
}

static void
fz_test_color(fz_context *ctx, fz_colorspace *colorspace, float *color, int *iscolor)
{
	if (!*iscolor && colorspace && colorspace != fz_device_gray(ctx))
	{
		float rgb[3];
		fz_convert_color(ctx, fz_device_rgb(ctx), rgb, colorspace, color);
		if (rgb[0] != rgb[1] || rgb[0] != rgb[2])
			*iscolor = 1;
	}
}

static void
fz_test_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_test_color(dev->ctx, colorspace, color, dev->user);
}

static void
fz_test_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke,
	const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	fz_test_color(dev->ctx, colorspace, color, dev->user);
}

static void
fz_test_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_test_color(dev->ctx, colorspace, color, dev->user);
}

static void
fz_test_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke,
	const fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	fz_test_color(dev->ctx, colorspace, color, dev->user);
}

static void
fz_test_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha)
{
	fz_test_colorspace(dev->ctx, shade->colorspace, dev->user);
}

static void
fz_test_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha)
{
	fz_test_colorspace(dev->ctx, image->colorspace, dev->user);
}

static void
fz_test_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_test_color(dev->ctx, colorspace, color, dev->user);
}

fz_device *
fz_new_test_device(fz_context *ctx, int *iscolor)
{
	fz_device *dev = fz_new_device(ctx, iscolor);

	dev->fill_path = fz_test_fill_path;
	dev->stroke_path = fz_test_stroke_path;
	dev->fill_text = fz_test_fill_text;
	dev->stroke_text = fz_test_stroke_text;
	dev->fill_shade = fz_test_fill_shade;
	dev->fill_image = fz_test_fill_image;
	dev->fill_image_mask = fz_test_fill_image_mask;

	*iscolor = 0;

	return dev;
}
