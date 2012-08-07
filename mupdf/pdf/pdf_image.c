#include "fitz-internal.h"
#include "mupdf-internal.h"

typedef struct pdf_image_key_s pdf_image_key;

struct pdf_image_key_s {
	int refs;
	fz_image *image;
	int factor;
};

static void pdf_load_jpx(pdf_document *xref, pdf_obj *dict, pdf_image *image);

static void
pdf_mask_color_key(fz_pixmap *pix, int n, int *colorkey)
{
	unsigned char *p = pix->samples;
	int len = pix->w * pix->h;
	int k, t;
	while (len--)
	{
		t = 1;
		for (k = 0; k < n; k++)
			if (p[k] < colorkey[k * 2] || p[k] > colorkey[k * 2 + 1])
				t = 0;
		if (t)
			for (k = 0; k < pix->n; k++)
				p[k] = 0;
		p += pix->n;
	}
	pix->has_alpha = pix->n > n; /* SumatraPDF: allow optimizing non-alpha pixmaps */
	pix->single_bit = 0; /* SumatraPDF: allow optimizing 1-bit pixmaps */
}

static int
pdf_make_hash_image_key(fz_store_hash *hash, void *key_)
{
	pdf_image_key *key = (pdf_image_key *)key_;

	hash->u.pi.ptr = key->image;
	hash->u.pi.i = key->factor;
	return 1;
}

static void *
pdf_keep_image_key(fz_context *ctx, void *key_)
{
	pdf_image_key *key = (pdf_image_key *)key_;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	key->refs++;
	fz_unlock(ctx, FZ_LOCK_ALLOC);

	return (void *)key;
}

static void
pdf_drop_image_key(fz_context *ctx, void *key_)
{
	pdf_image_key *key = (pdf_image_key *)key_;
	int drop;

	fz_lock(ctx, FZ_LOCK_ALLOC);
	drop = --key->refs;
	fz_unlock(ctx, FZ_LOCK_ALLOC);
	if (drop == 0)
	{
		fz_drop_image(ctx, key->image);
		fz_free(ctx, key);
	}
}

static int
pdf_cmp_image_key(void *k0_, void *k1_)
{
	pdf_image_key *k0 = (pdf_image_key *)k0_;
	pdf_image_key *k1 = (pdf_image_key *)k1_;

	return k0->image == k1->image && k0->factor == k1->factor;
}

#ifndef NDEBUG
static void
pdf_debug_image(void *key_)
{
	pdf_image_key *key = (pdf_image_key *)key_;

	printf("(image %d x %d sf=%d) ", key->image->w, key->image->h, key->factor);
}
#endif

static fz_store_type pdf_image_store_type =
{
	pdf_make_hash_image_key,
	pdf_keep_image_key,
	pdf_drop_image_key,
	pdf_cmp_image_key,
#ifndef NDEBUG
	pdf_debug_image
#endif
};

static fz_pixmap *
decomp_image_from_stream(fz_context *ctx, fz_stream *stm, pdf_image *image, int in_line, int indexed, int factor, int cache)
{
	fz_pixmap *tile = NULL;
	fz_pixmap *existing_tile;
	int stride, len, i;
	unsigned char *samples = NULL;
	int w = (image->base.w + (factor-1)) / factor;
	int h = (image->base.h + (factor-1)) / factor;
	pdf_image_key *key;

	fz_var(tile);
	fz_var(samples);

	fz_try(ctx)
	{
		tile = fz_new_pixmap(ctx, image->base.colorspace, w, h);
		tile->interpolate = image->interpolate;

		stride = (w * image->n * image->bpc + 7) / 8;

		samples = fz_malloc_array(ctx, h, stride);

		len = fz_read(stm, samples, h * stride);
		if (len < 0)
		{
			fz_throw(ctx, "cannot read image data");
		}

		/* Make sure we read the EOF marker (for inline images only) */
		/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1980 */
		if (in_line && 0)
		{
			unsigned char tbuf[512];
			fz_try(ctx)
			{
				int tlen = fz_read(stm, tbuf, sizeof tbuf);
				if (tlen > 0)
					fz_warn(ctx, "ignoring garbage at end of image");
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "ignoring error at end of image");
			}
		}

		/* Pad truncated images */
		if (len < stride * h)
		{
			fz_warn(ctx, "padding truncated image");
			memset(samples + len, 0, stride * h - len);
		}

		/* Invert 1-bit image masks */
		if (image->imagemask)
		{
			/* 0=opaque and 1=transparent so we need to invert */
			unsigned char *p = samples;
			len = h * stride;
			for (i = 0; i < len; i++)
				p[i] = ~p[i];
		}

		fz_unpack_tile(tile, samples, image->n, image->bpc, stride, indexed);

		fz_free(ctx, samples);
		samples = NULL;

		if (image->usecolorkey)
			pdf_mask_color_key(tile, image->n, image->colorkey);

		if (indexed)
		{
			fz_pixmap *conv;
			fz_decode_indexed_tile(tile, image->decode, (1 << image->bpc) - 1);
			conv = pdf_expand_indexed_pixmap(ctx, tile);
			fz_drop_pixmap(ctx, tile);
			tile = conv;
		}
		else
		{
			fz_decode_tile(tile, image->decode);
		}
	}
	fz_always(ctx)
	{
		fz_close(stm);
	}
	fz_catch(ctx)
	{
		if (tile)
			fz_drop_pixmap(ctx, tile);
		fz_free(ctx, samples);

		fz_rethrow(ctx);
	}

	if (!cache)
		return tile;

	/* Now we try to cache the pixmap. Any failure here will just result
	 * in us not caching. */
	fz_try(ctx)
	{
		key = fz_malloc_struct(ctx, pdf_image_key);
		key->refs = 1;
		key->image = fz_keep_image(ctx, &image->base);
		key->factor = factor;
		existing_tile = fz_store_item(ctx, key, tile, fz_pixmap_size(ctx, tile), &pdf_image_store_type);
		if (existing_tile)
		{
			/* We already have a tile. This must have been produced by a
			 * racing thread. We'll throw away ours and use that one. */
			fz_drop_pixmap(ctx, tile);
			tile = existing_tile;
		}
	}
	fz_always(ctx)
	{
		pdf_drop_image_key(ctx, key);
	}
	fz_catch(ctx)
	{
		/* Do nothing */
	}

	return tile;
}

static void
pdf_free_image(fz_context *ctx, fz_storable *image_)
{
	pdf_image *image = (pdf_image *)image_;

	if (image == NULL)
		return;
	fz_drop_pixmap(ctx, image->tile);
	fz_drop_buffer(ctx, image->buffer);
	fz_drop_colorspace(ctx, image->base.colorspace);
	fz_drop_image(ctx, image->base.mask);
	fz_free(ctx, image);
}

static fz_pixmap *
pdf_image_get_pixmap(fz_context *ctx, fz_image *image_, int w, int h)
{
	pdf_image *image = (pdf_image *)image_;
	fz_pixmap *tile;
	fz_stream *stm;
	int factor;
	pdf_image_key key;

	/* Check for 'simple' images which are just pixmaps */
	if (image->buffer == NULL)
	{
		tile = image->tile;
		if (!tile)
			return NULL;
		return fz_keep_pixmap(ctx, tile); /* That's all we can give you! */
	}

	/* Ensure our expectations for tile size are reasonable */
	if (w > image->base.w)
		w = image->base.w;
	if (h > image->base.h)
		h = image->base.h;

	/* What is our ideal factor? */
	if (w == 0 || h == 0)
		factor = 1;
	else
		for (factor=1; image->base.w/(2*factor) >= w && image->base.h/(2*factor) >= h && factor < 8; factor *= 2);

	/* Can we find any suitable tiles in the cache? */
	key.refs = 1;
	key.image = &image->base;
	key.factor = factor;
	do
	{
		tile = fz_find_item(ctx, fz_free_pixmap_imp, &key, &pdf_image_store_type);
		if (tile)
			return tile;
		key.factor >>= 1;
	}
	while (key.factor > 0);

	/* We need to make a new one. */
	stm = pdf_open_image_decomp_stream(ctx, image->buffer, &image->params, &factor);

	return decomp_image_from_stream(ctx, stm, image, 0, 0, factor, 1);
}

static pdf_image *
pdf_load_image_imp(pdf_document *xref, pdf_obj *rdb, pdf_obj *dict, fz_stream *cstm, int forcemask)
{
	fz_stream *stm = NULL;
	pdf_image *image = NULL;
	pdf_obj *obj, *res;

	int w, h, bpc, n;
	int imagemask;
	int interpolate;
	int indexed;
	fz_image *mask = NULL; /* explicit mask/softmask image */
	int usecolorkey;

	int i;
	fz_context *ctx = xref->ctx;

	fz_var(stm);
	fz_var(mask);

	image = fz_malloc_struct(ctx, pdf_image);

	fz_try(ctx)
	{
		/* special case for JPEG2000 images */
		if (pdf_is_jpx_image(ctx, dict))
		{
			pdf_load_jpx(xref, dict, image);

			if (forcemask)
			{
				fz_pixmap *mask_pixmap;
				if (image->n != 2)
					fz_throw(ctx, "softmask must be grayscale");
				mask_pixmap = fz_alpha_from_gray(ctx, image->tile, 1);
				fz_drop_pixmap(ctx, image->tile);
				image->tile = mask_pixmap;
			}
			break; /* Out of fz_try */
		}

		w = pdf_to_int(pdf_dict_getsa(dict, "Width", "W"));
		h = pdf_to_int(pdf_dict_getsa(dict, "Height", "H"));
		bpc = pdf_to_int(pdf_dict_getsa(dict, "BitsPerComponent", "BPC"));
		imagemask = pdf_to_bool(pdf_dict_getsa(dict, "ImageMask", "IM"));
		interpolate = pdf_to_bool(pdf_dict_getsa(dict, "Interpolate", "I"));

		indexed = 0;
		usecolorkey = 0;
		mask = NULL;

		if (imagemask)
			bpc = 1;

		if (w <= 0)
			fz_throw(ctx, "image width is zero (or less)");
		if (h <= 0)
			fz_throw(ctx, "image height is zero (or less)");
		if (bpc <= 0)
			fz_throw(ctx, "image depth is zero (or less)");
		if (bpc > 16)
			fz_throw(ctx, "image depth is too large: %d", bpc);
		if (w > (1 << 16))
			fz_throw(ctx, "image is too wide");
		if (h > (1 << 16))
			fz_throw(ctx, "image is too high");

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

			image->base.colorspace = pdf_load_colorspace(xref, obj);

			if (!strcmp(image->base.colorspace->name, "Indexed"))
				indexed = 1;

			n = image->base.colorspace->n;
		}
		else
		{
			n = 1;
		}

		obj = pdf_dict_getsa(dict, "Decode", "D");
		if (obj)
		{
			for (i = 0; i < n * 2; i++)
				image->decode[i] = pdf_to_real(pdf_array_get(obj, i));
		}
		else
		{
			float maxval = indexed ? (1 << bpc) - 1 : 1;
			for (i = 0; i < n * 2; i++)
				image->decode[i] = i & 1 ? maxval : 0;
		}

		obj = pdf_dict_getsa(dict, "SMask", "Mask");
		if (pdf_is_dict(obj))
		{
			/* Not allowed for inline images */
			if (!cstm)
			{
				mask = (fz_image *)pdf_load_image_imp(xref, rdb, obj, NULL, 1);
			}
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
				image->colorkey[i] = pdf_to_int(pdf_array_get(obj, i));
			}
		}

		/* Now, do we load a ref, or do we load the actual thing? */
		image->params.type = PDF_IMAGE_RAW;
		FZ_INIT_STORABLE(&image->base, 1, pdf_free_image);
		image->base.get_pixmap = pdf_image_get_pixmap;
		image->base.w = w;
		image->base.h = h;
		image->n = n;
		image->bpc = bpc;
		image->interpolate = interpolate;
		image->imagemask = imagemask;
		image->usecolorkey = usecolorkey;
		image->base.mask = mask;
		image->params.colorspace = image->base.colorspace; /* Uses the same ref as for the base one */
		if (!indexed && !cstm)
		{
			/* Just load the compressed image data now and we can
			 * decode it on demand. */
			int num = pdf_to_num(dict);
			int gen = pdf_to_gen(dict);
			image->buffer = pdf_load_image_stream(xref, num, gen, num, gen, &image->params);
			break; /* Out of fz_try */
		}

		/* We need to decompress the image now */
		if (cstm)
		{
			int stride = (w * image->n * image->bpc + 7) / 8;
			stm = pdf_open_inline_stream(xref, dict, stride * h, cstm, NULL);
		}
		else
		{
			stm = pdf_open_stream(xref, pdf_to_num(dict), pdf_to_gen(dict));
		}

		image->tile = decomp_image_from_stream(ctx, stm, image, cstm != NULL, indexed, 1, 0);
	}
	fz_catch(ctx)
	{
		pdf_free_image(ctx, (fz_storable *) image);
		fz_rethrow(ctx);
	}
	return image;
}

fz_image *
pdf_load_inline_image(pdf_document *xref, pdf_obj *rdb, pdf_obj *dict, fz_stream *file)
{
	return (fz_image *)pdf_load_image_imp(xref, rdb, dict, file, 0);
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

static void
pdf_load_jpx(pdf_document *xref, pdf_obj *dict, pdf_image *image)
{
	fz_buffer *buf = NULL;
	fz_colorspace *colorspace = NULL;
	fz_pixmap *img = NULL;
	pdf_obj *obj;
	fz_context *ctx = xref->ctx;
	int indexed = 0;

	fz_var(img);
	fz_var(buf);
	fz_var(colorspace);

	buf = pdf_load_stream(xref, pdf_to_num(dict), pdf_to_gen(dict));

	/* FIXME: We can't handle decode arrays for indexed images currently */
	fz_try(ctx)
	{
		obj = pdf_dict_gets(dict, "ColorSpace");
		if (obj)
		{
			colorspace = pdf_load_colorspace(xref, obj);
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
			image->base.mask = (fz_image *)pdf_load_image_imp(xref, NULL, obj, NULL, 1);
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
	image->params.type = PDF_IMAGE_RAW;
	FZ_INIT_STORABLE(&image->base, 1, pdf_free_image);
	image->base.get_pixmap = pdf_image_get_pixmap;
	image->base.w = img->w;
	image->base.h = img->h;
	image->base.colorspace = colorspace;
	image->tile = img;
	image->n = img->n;
	image->bpc = 8;
	image->interpolate = 0;
	image->imagemask = 0;
	image->usecolorkey = 0;
	image->params.colorspace = colorspace; /* Uses the same ref as for the base one */
}

static int
pdf_image_size(fz_context *ctx, pdf_image *im)
{
	if (im == NULL)
		return 0;
	return sizeof(*im) + fz_pixmap_size(ctx, im->tile) + (im->buffer ? im->buffer->cap : 0);
}

fz_image *
pdf_load_image(pdf_document *xref, pdf_obj *dict)
{
	fz_context *ctx = xref->ctx;
	pdf_image *image;

	if ((image = pdf_find_item(ctx, pdf_free_image, dict)))
	{
		return (fz_image *)image;
	}

	image = pdf_load_image_imp(xref, NULL, dict, NULL, 0);

	pdf_store_item(ctx, dict, image, pdf_image_size(ctx, image));

	return (fz_image *)image;
}
