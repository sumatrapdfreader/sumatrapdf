#include "muxps-internal.h"

static fz_image *
xps_load_image(fz_context *ctx, xps_part *part)
{
	/* Ownership of data always passes in here */
	byte *data = part->data;
	part->data = NULL;
	return fz_new_image_from_buffer(ctx, data, part->size);
}

/* FIXME: area unused! */
static void
xps_paint_image_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict,
	fz_xml *root, void *vimage)
{
	fz_image *image = vimage;
	float xs, ys;
	fz_matrix local_ctm = *ctm;

	if (image->xres == 0 || image->yres == 0)
		return;
	xs = image->w * 96 / image->xres;
	ys = image->h * 96 / image->yres;
	fz_pre_scale(&local_ctm, xs, ys);
	fz_fill_image(doc->dev, image, &local_ctm, doc->opacity[doc->opacity_top]);
}

static xps_part *
xps_find_image_brush_source_part(xps_document *doc, char *base_uri, fz_xml *root)
{
	char *image_source_att;
	char buf[1024];
	char partname[1024];
	char *image_name;
	char *profile_name;
	char *p;

	image_source_att = fz_xml_att(root, "ImageSource");
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

	xps_resolve_url(partname, base_uri, image_name, sizeof partname);

	return xps_read_part(doc, partname);
}

/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2094 */
typedef struct {
	fz_storable storable;
	char *part_name;
} xps_image_key;

static void
xps_free_image_key(fz_context *ctx, fz_storable *key)
{
	fz_free(ctx, ((xps_image_key *)key)->part_name);
	fz_free(ctx, key);
}

static xps_image_key *
xps_new_image_key(fz_context *ctx, char *name)
{
	xps_image_key *key = fz_malloc_struct(ctx, xps_image_key);
	key->part_name = fz_strdup(ctx, name);
	FZ_INIT_STORABLE(key, 1, xps_free_image_key);
	return key;
}

static int
xps_cmp_image_key(void *k1, void *k2)
{
	return strcmp(((xps_image_key *)k1)->part_name, ((xps_image_key *)k2)->part_name);
}

#ifndef NDEBUG
static void
xps_debug_image(FILE *out, void *key)
{
	fprintf(out, "(image part=%s) ", ((xps_image_key *)key)->part_name);
}
#endif

static fz_store_type xps_image_store_type =
{
	NULL,
	fz_keep_storable,
	fz_drop_storable,
	xps_cmp_image_key,
#ifndef NDEBUG
	xps_debug_image
#endif
};

void
xps_parse_image_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area,
	char *base_uri, xps_resource *dict, fz_xml *root)
{
	xps_part *part;
	fz_image *image;
	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2094 */
	xps_image_key *key = NULL;
	fz_var(key);

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
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2094 */
		key = xps_new_image_key(doc->ctx, part->name);
		if (!(image = fz_find_item(doc->ctx, fz_free_image, key, &xps_image_store_type)))
		{

		image = xps_load_image(doc->ctx, part);

			image->from_xps = 1; /* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2250 */
			fz_store_item(doc->ctx, key, image, sizeof(fz_image) + part->size, &xps_image_store_type);
		}
	}
	fz_always(doc->ctx)
	{
		fz_drop_storable(doc->ctx, &key->storable);
		xps_free_part(doc, part);
	}
	fz_catch(doc->ctx)
	{
		fz_warn(doc->ctx, "cannot decode image resource");
		return;
	}

	xps_parse_tiling_brush(doc, ctm, area, base_uri, dict, root, xps_paint_image_brush, image);

	fz_drop_image(doc->ctx, image);
}
