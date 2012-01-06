#include "fitz.h"
#include "muxps.h"

static fz_pixmap *
xps_decode_image(fz_context *ctx, byte *buf, int len)
{
	fz_pixmap *image;

	if (len < 8)
		fz_throw(ctx, "unknown image file format");

	if (buf[0] == 0xff && buf[1] == 0xd8)
		image = xps_decode_jpeg(ctx, buf, len);
	else if (memcmp(buf, "\211PNG\r\n\032\n", 8) == 0)
		image = xps_decode_png(ctx, buf, len);
	else if (memcmp(buf, "II", 2) == 0 && buf[2] == 0xBC)
		fz_throw(ctx, "JPEG-XR codec is not available");
	else if (memcmp(buf, "MM", 2) == 0 || memcmp(buf, "II", 2) == 0)
		image = xps_decode_tiff(ctx, buf, len);
	else
		fz_throw(ctx, "unknown image file format");

	return image;
}

static void
xps_paint_image_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict,
	xml_element *root, void *vimage)
{
	fz_pixmap *pixmap = vimage;
	float xs, ys;
	fz_matrix im;

	if (pixmap->xres == 0 || pixmap->yres == 0)
		return;
	xs = pixmap->w * 96 / pixmap->xres;
	ys = pixmap->h * 96 / pixmap->yres;
	im = fz_scale(xs, -ys);
	im.f = ys;
	ctm = fz_concat(im, ctm);
	fz_fill_image(doc->dev, pixmap, ctm, doc->opacity[doc->opacity_top]);
}

static xps_part *
xps_find_image_brush_source_part(xps_document *doc, char *base_uri, xml_element *root)
{
	char *image_source_att;
	char buf[1024];
	char partname[1024];
	char *image_name;
	char *profile_name;
	char *p;

	image_source_att = xml_att(root, "ImageSource");
	if (!image_source_att)
		fz_throw(doc->ctx, "cannot find image source attribute");

	/* "{ColorConvertedBitmap /Resources/Image.tiff /Resources/Profile.icc}" */
	if (strstr(image_source_att, "{ColorConvertedBitmap") == image_source_att)
	{
		image_name = NULL;
		profile_name = NULL;

		fz_strlcpy(buf, image_source_att, sizeof buf);
		p = strchr(buf, ' ');
		if (p)
		{
			image_name = p + 1;
			p = strchr(p + 1, ' ');
			if (p)
			{
				*p = 0;
				profile_name = p + 1;
				p = strchr(p + 1, '}');
				if (p)
					*p = 0;
			}
		}
	}
	else
	{
		image_name = image_source_att;
		profile_name = NULL;
	}

	if (!image_name)
		fz_throw(doc->ctx, "cannot find image source");

	xps_absolute_path(partname, base_uri, image_name, sizeof partname);

	return xps_read_part(doc, partname);
}

void
xps_parse_image_brush(xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root)
{
	xps_part *part;
	fz_pixmap *image;

	fz_try(doc->ctx)
	{
		part = xps_find_image_brush_source_part(doc, base_uri, root);
	}
	fz_catch(doc->ctx)
	{
		fz_warn(doc->ctx, "cannot find image source");
		return;
	}

	fz_try(doc->ctx)
	{
		image = xps_decode_image(doc->ctx, part->data, part->size);
	}
	fz_catch(doc->ctx)
	{
		fz_warn(doc->ctx, "cannot decode image resource");
		xps_free_part(doc, part);
		return;
	}
	xps_free_part(doc, part);

	xps_parse_tiling_brush(doc, ctm, area, base_uri, dict, root, xps_paint_image_brush, image);

	fz_drop_pixmap(doc->ctx, image);
}
