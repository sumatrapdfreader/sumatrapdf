#include "mupdf/pdf.h"

static fz_image *pdf_load_jpx(pdf_document *doc, pdf_obj *dict, int forcemask);

static fz_image *
pdf_load_image_imp(pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, fz_stream *cstm, int forcemask)
{
	fz_stream *stm = NULL;
	fz_image *image = NULL;
	pdf_obj *obj, *res;

	int w, h, bpc, n;
	int imagemask;
	int interpolate;
	int indexed;
	fz_image *mask = NULL; /* explicit mask/soft mask image */
	int usecolorkey = 0;
	fz_colorspace *colorspace = NULL;
	float decode[FZ_MAX_COLORS * 2];
	int colorkey[FZ_MAX_COLORS * 2];

	int i;
	fz_context *ctx = doc->ctx;

	fz_var(stm);
	fz_var(mask);
	fz_var(image);
	fz_var(colorspace);

	fz_try(ctx)
	{
		/* special case for JPEG2000 images */
		if (pdf_is_jpx_image(ctx, dict))
		{
			image = pdf_load_jpx(doc, dict, forcemask);

			if (forcemask)
			{
				fz_pixmap *mask_pixmap;
				if (image->n != 2)
				{
					/* SumatraPDF: ignore invalid JPX softmasks */
					fz_warn(ctx, "soft mask must be grayscale");
					mask_pixmap = fz_new_pixmap(ctx, NULL, image->tile->w, image->tile->h);
					fz_clear_pixmap_with_value(ctx, mask_pixmap, 255);
				}
				else
				mask_pixmap = fz_alpha_from_gray(ctx, image->tile, 1);
				fz_drop_pixmap(ctx, image->tile);
				image->tile = mask_pixmap;
			}
			break; /* Out of fz_try */
		}

		w = pdf_to_int(pdf_dict_getsa(dict, "Width", "W"));
		h = pdf_to_int(pdf_dict_getsa(dict, "Height", "H"));
		bpc = pdf_to_int(pdf_dict_getsa(dict, "BitsPerComponent", "BPC"));
		if (bpc == 0)
			bpc = 8;
		imagemask = pdf_to_bool(pdf_dict_getsa(dict, "ImageMask", "IM"));
		interpolate = pdf_to_bool(pdf_dict_getsa(dict, "Interpolate", "I"));

		indexed = 0;
		usecolorkey = 0;

		if (imagemask)
			bpc = 1;

		if (w <= 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "image width is zero (or less)");
		if (h <= 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "image height is zero (or less)");
		if (bpc <= 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "image depth is zero (or less)");
		if (bpc > 16)
			fz_throw(ctx, FZ_ERROR_GENERIC, "image depth is too large: %d", bpc);
		if (w > (1 << 16))
			fz_throw(ctx, FZ_ERROR_GENERIC, "image is too wide");
		if (h > (1 << 16))
			fz_throw(ctx, FZ_ERROR_GENERIC, "image is too high");

		obj = pdf_dict_getsa(dict, "ColorSpace", "CS");
		if (obj && !imagemask && !forcemask)
		{
			/* colorspace resource lookup is only done for inline images */
			if (pdf_is_name(obj))
			{
				res = pdf_dict_get(pdf_dict_gets(rdb, "ColorSpace"), obj);
				if (res)
					obj = res;
			}

			colorspace = pdf_load_colorspace(doc, obj);

			if (!strcmp(colorspace->name, "Indexed"))
				indexed = 1;

			n = colorspace->n;
		}
		else
		{
			n = 1;
		}

		obj = pdf_dict_getsa(dict, "Decode", "D");
		if (obj)
		{
			for (i = 0; i < n * 2; i++)
				decode[i] = pdf_to_real(pdf_array_get(obj, i));
		}
		else
		{
			float maxval = indexed ? (1 << bpc) - 1 : 1;
			for (i = 0; i < n * 2; i++)
				decode[i] = i & 1 ? maxval : 0;
		}

		obj = pdf_dict_getsa(dict, "SMask", "Mask");
		if (pdf_is_dict(obj))
		{
			/* Not allowed for inline images or soft masks */
			if (cstm)
				fz_warn(ctx, "Ignoring invalid inline image soft mask");
			else if (forcemask)
				fz_warn(ctx, "Ignoring recursive image soft mask");
			else
				mask = pdf_load_image_imp(doc, rdb, obj, NULL, 1);
		}
		else if (pdf_is_array(obj))
		{
			usecolorkey = 1;
			for (i = 0; i < n * 2; i++)
			{
				if (!pdf_is_int(pdf_array_get(obj, i)))
				{
					fz_warn(ctx, "invalid value in color key mask");
					usecolorkey = 0;
				}
				colorkey[i] = pdf_to_int(pdf_array_get(obj, i));
			}
		}

		/* Now, do we load a ref, or do we load the actual thing? */
		if (!cstm)
		{
			/* Just load the compressed image data now and we can
			 * decode it on demand. */
			int num = pdf_to_num(dict);
			int gen = pdf_to_gen(dict);
			fz_compressed_buffer *buffer = pdf_load_compressed_stream(doc, num, gen);
			image = fz_new_image(ctx, w, h, bpc, colorspace, 96, 96, interpolate, imagemask, decode, usecolorkey ? colorkey : NULL, buffer, mask);
			break; /* Out of fz_try */
		}

		/* We need to decompress the image now */
		if (cstm)
		{
			int stride = (w * n * bpc + 7) / 8;
			stm = pdf_open_inline_stream(doc, dict, stride * h, cstm, NULL);
		}
		else
		{
			stm = pdf_open_stream(doc, pdf_to_num(dict), pdf_to_gen(dict));
		}

		image = fz_new_image(ctx, w, h, bpc, colorspace, 96, 96, interpolate, imagemask, decode, usecolorkey ? colorkey : NULL, NULL, mask);
		colorspace = NULL;
		mask = NULL;
		image->tile = fz_decomp_image_from_stream(ctx, stm, image, cstm != NULL, indexed, 0, 0);
	}
	fz_catch(ctx)
	{
		fz_drop_colorspace(ctx, colorspace);
		fz_drop_image(ctx, mask);
		fz_drop_image(ctx, image);
		fz_rethrow(ctx);
	}

	/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=693517 */
	fz_try(ctx)
	{
		obj = pdf_dict_getp(dict, "SMask/Matte");
		if (pdf_is_array(obj) && image->mask)
		{
			assert(!image->usecolorkey);
			image->usecolorkey = 2;
			for (i = 0; i < image->n; i++)
				image->colorkey[i] = pdf_to_int(pdf_array_get(obj, i));
		}
	}
	fz_catch(ctx)
	{
		fz_drop_image(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}

fz_image *
pdf_load_inline_image(pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, fz_stream *file)
{
	return pdf_load_image_imp(doc, rdb, dict, file, 0);
}

int
pdf_is_jpx_image(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *filter;
	int i, n;

	filter = pdf_dict_gets(dict, "Filter");
	if (!strcmp(pdf_to_name(filter), "JPXDecode"))
		return 1;
	n = pdf_array_len(filter);
	for (i = 0; i < n; i++)
		if (!strcmp(pdf_to_name(pdf_array_get(filter, i)), "JPXDecode"))
			return 1;
	return 0;
}

static fz_image *
pdf_load_jpx(pdf_document *doc, pdf_obj *dict, int forcemask)
{
	fz_buffer *buf = NULL;
	fz_colorspace *colorspace = NULL;
	fz_pixmap *img = NULL;
	pdf_obj *obj;
	fz_context *ctx = doc->ctx;
	int indexed = 0;
	fz_image *mask = NULL;

	fz_var(img);
	fz_var(buf);
	fz_var(colorspace);
	fz_var(mask);

	buf = pdf_load_stream(doc, pdf_to_num(dict), pdf_to_gen(dict));

	/* FIXME: We can't handle decode arrays for indexed images currently */
	fz_try(ctx)
	{
		obj = pdf_dict_gets(dict, "ColorSpace");
		if (obj)
		{
			colorspace = pdf_load_colorspace(doc, obj);
			indexed = !strcmp(colorspace->name, "Indexed");
		}

		img = fz_load_jpx(ctx, buf->data, buf->len, colorspace, indexed);

		if (img && colorspace == NULL)
			colorspace = fz_keep_colorspace(ctx, img->colorspace);

		fz_drop_buffer(ctx, buf);
		buf = NULL;

		obj = pdf_dict_getsa(dict, "SMask", "Mask");
		if (pdf_is_dict(obj))
		{
			if (forcemask)
				fz_warn(ctx, "Ignoring recursive JPX soft mask");
			else
				mask = pdf_load_image_imp(doc, NULL, obj, NULL, 1);
		}

		obj = pdf_dict_getsa(dict, "Decode", "D");
		if (obj && !indexed)
		{
			float decode[FZ_MAX_COLORS * 2];
			int i;

			for (i = 0; i < img->n * 2; i++)
				decode[i] = pdf_to_real(pdf_array_get(obj, i));

			fz_decode_tile(img, decode);
		}
	}
	fz_catch(ctx)
	{
		if (colorspace)
			fz_drop_colorspace(ctx, colorspace);
		fz_drop_buffer(ctx, buf);
		fz_drop_pixmap(ctx, img);
		fz_rethrow(ctx);
	}
	return fz_new_image_from_pixmap(ctx, img, mask);
}

static int
fz_image_size(fz_context *ctx, fz_image *im)
{
	if (im == NULL)
		return 0;
	return sizeof(*im) + fz_pixmap_size(ctx, im->tile) + (im->buffer && im->buffer->buffer ? im->buffer->buffer->cap : 0);
}

fz_image *
pdf_load_image(pdf_document *doc, pdf_obj *dict)
{
	fz_context *ctx = doc->ctx;
	fz_image *image;

	if ((image = pdf_find_item(ctx, fz_free_image, dict)))
	{
		return (fz_image *)image;
	}

	image = pdf_load_image_imp(doc, NULL, dict, NULL, 0);

	pdf_store_item(ctx, dict, image, fz_image_size(ctx, image));

	return (fz_image *)image;
}
