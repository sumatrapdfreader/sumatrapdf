#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

static void
fz_md5_image(fz_context *ctx, fz_image *image, unsigned char digest[16])
{
	fz_pixmap *pixmap;
	fz_md5 state;
	int h;
	unsigned char *d;

	pixmap = fz_get_pixmap_from_image(ctx, image, NULL, NULL, 0, 0);
	fz_md5_init(&state);
	d = pixmap->samples;
	h = pixmap->h;
	while (h--)
	{
		fz_md5_update(&state, d, pixmap->w * pixmap->n);
		d += pixmap->stride;
	}
	fz_md5_final(&state, digest);
	fz_drop_pixmap(ctx, pixmap);
}

/* Image specific methods */
static void
pdf_preload_image_resources(fz_context *ctx, pdf_document *doc)
{
	int len, k;
	pdf_obj *obj = NULL;
	pdf_obj *type;
	fz_image *image = NULL;
	unsigned char digest[16];

	fz_var(obj);
	fz_var(image);

	fz_try(ctx)
	{
		len = pdf_count_objects(ctx, doc);
		for (k = 1; k < len; k++)
		{
			obj = pdf_new_indirect(ctx, doc, k, 0);
			type = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));
			if (pdf_name_eq(ctx, type, PDF_NAME(Image)))
			{
				image = pdf_load_image(ctx, doc, obj);
				fz_md5_image(ctx, image, digest);
				fz_drop_image(ctx, image);
				image = NULL;

				/* Don't allow overwrites. */
				if (!fz_hash_find(ctx, doc->resources.images, digest))
				{
					fz_hash_insert(ctx, doc->resources.images, digest, obj);
					obj = NULL;
				}
			}
			pdf_drop_obj(ctx, obj);
			obj = NULL;
		}
	}
	fz_always(ctx)
	{
		fz_drop_image(ctx, image);
		pdf_drop_obj(ctx, obj);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void pdf_drop_obj_as_void(fz_context *ctx, void *obj)
{
	pdf_drop_obj(ctx, obj);
}

pdf_obj *
pdf_find_image_resource(fz_context *ctx, pdf_document *doc, fz_image *item, unsigned char digest[16])
{
	pdf_obj *res;

	if (!doc->resources.images)
	{
		doc->resources.images = fz_new_hash_table(ctx, 4096, 16, -1, pdf_drop_obj_as_void);
		pdf_preload_image_resources(ctx, doc);
	}

	/* Create md5 and see if we have the item in our table */
	fz_md5_image(ctx, item, digest);
	res = fz_hash_find(ctx, doc->resources.images, digest);
	if (res)
		pdf_keep_obj(ctx, res);
	return res;
}

pdf_obj *
pdf_insert_image_resource(fz_context *ctx, pdf_document *doc, unsigned char digest[16], pdf_obj *obj)
{
	pdf_obj *res = fz_hash_insert(ctx, doc->resources.images, digest, obj);
	if (res)
		fz_warn(ctx, "warning: image resource already present");
	else
		res = pdf_keep_obj(ctx, obj);
	return pdf_keep_obj(ctx, res);
}

/* We do need to come up with an effective way to see what is already in the
 * file to avoid adding to what is already there. This is avoided for pdfwrite
 * as we check as we add each font. For adding text to an existing file though
 * it may be more problematic. */

pdf_obj *
pdf_find_font_resource(fz_context *ctx, pdf_document *doc, int type, int encoding, fz_font *item, unsigned char digest[16])
{
	pdf_obj *res;

	if (!doc->resources.fonts)
		doc->resources.fonts = fz_new_hash_table(ctx, 4096, 16, -1, pdf_drop_obj_as_void);

	fz_font_digest(ctx, item, digest);

	digest[0] += type;
	digest[1] += encoding;

	res = fz_hash_find(ctx, doc->resources.fonts, digest);
	if (res)
		pdf_keep_obj(ctx, res);
	return res;
}

pdf_obj *
pdf_insert_font_resource(fz_context *ctx, pdf_document *doc, unsigned char digest[16], pdf_obj *obj)
{
	pdf_obj *res = fz_hash_insert(ctx, doc->resources.fonts, digest, obj);
	if (res)
		fz_warn(ctx, "warning: font resource already present");
	else
		res = pdf_keep_obj(ctx, obj);
	return pdf_keep_obj(ctx, res);
}

void
pdf_drop_resource_tables(fz_context *ctx, pdf_document *doc)
{
	if (doc)
	{
		fz_drop_hash_table(ctx, doc->resources.fonts);
		fz_drop_hash_table(ctx, doc->resources.images);
	}
}
