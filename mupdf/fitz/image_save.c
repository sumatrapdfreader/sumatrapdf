#include "fitz-internal.h"

void fz_write_pixmap(fz_context *ctx, fz_pixmap *img, char *file, int rgb)
{
	char name[1024];
	fz_pixmap *converted = NULL;

	if (!img)
		return;

	if (rgb && img->colorspace && img->colorspace != fz_device_rgb)
	{
		converted = fz_new_pixmap_with_bbox(ctx, fz_device_rgb, fz_pixmap_bbox(ctx, img));
		fz_convert_pixmap(ctx, converted, img);
		img = converted;
	}

	if (img->n <= 4)
	{
		sprintf(name, "%s.png", file);
		printf("extracting image %s\n", name);
		fz_write_png(ctx, img, name, 0);
	}
	else
	{
		sprintf(name, "%s.pam", file);
		printf("extracting image %s\n", name);
		fz_write_pam(ctx, img, name, 0);
	}

	fz_drop_pixmap(ctx, converted);
}
