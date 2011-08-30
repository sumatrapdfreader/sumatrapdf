#include "fitz.h"
#include "mupdf.h"

/* TODO: store JPEG compressed samples */
/* TODO: store flate compressed samples */

static fz_error pdf_load_jpx_image(fz_pixmap **imgp, pdf_xref *xref, fz_obj *dict);

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
}

static fz_error
pdf_load_image_imp(fz_pixmap **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *cstm, int forcemask)
{
	fz_stream *stm;
	fz_pixmap *tile;
	fz_obj *obj, *res;
	fz_error error;

	int w, h, bpc, n;
	int imagemask;
	int interpolate;
	int indexed;
	fz_colorspace *colorspace;
	fz_pixmap *mask; /* explicit mask/softmask image */
	int usecolorkey;
	int colorkey[FZ_MAX_COLORS * 2];
	float decode[FZ_MAX_COLORS * 2];

	int stride;
	unsigned char *samples;
	int i, len;

	/* special case for JPEG2000 images */
	if (pdf_is_jpx_image(dict))
	{
		tile = NULL;
		error = pdf_load_jpx_image(&tile, xref, dict);
		if (error)
			return fz_rethrow(error, "cannot load jpx image");
		if (forcemask)
		{
			if (tile->n != 2)
			{
				fz_drop_pixmap(tile);
				return fz_throw("softmask must be grayscale");
			}
			mask = fz_alpha_from_gray(tile, 1);
			fz_drop_pixmap(tile);
			*imgp = mask;
			return fz_okay;
		}
		*imgp = tile;
		return fz_okay;
	}

	w = fz_to_int(fz_dict_getsa(dict, "Width", "W"));
	h = fz_to_int(fz_dict_getsa(dict, "Height", "H"));
	bpc = fz_to_int(fz_dict_getsa(dict, "BitsPerComponent", "BPC"));
	imagemask = fz_to_bool(fz_dict_getsa(dict, "ImageMask", "IM"));
	interpolate = fz_to_bool(fz_dict_getsa(dict, "Interpolate", "I"));

	indexed = 0;
	usecolorkey = 0;
	colorspace = NULL;
	mask = NULL;

	if (imagemask)
		bpc = 1;

	if (w == 0)
		return fz_throw("image width is zero");
	if (h == 0)
		return fz_throw("image height is zero");
	if (bpc == 0)
		return fz_throw("image depth is zero");
	if (bpc > 16)
		return fz_throw("image depth is too large: %d", bpc);
	if (w > (1 << 16))
		return fz_throw("image is too wide");
	if (h > (1 << 16))
		return fz_throw("image is too high");

	obj = fz_dict_getsa(dict, "ColorSpace", "CS");
	if (obj && !imagemask && !forcemask)
	{
		/* colorspace resource lookup is only done for inline images */
		if (fz_is_name(obj))
		{
			res = fz_dict_get(fz_dict_gets(rdb, "ColorSpace"), obj);
			if (res)
				obj = res;
		}

		error = pdf_load_colorspace(&colorspace, xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load image colorspace");

		if (!strcmp(colorspace->name, "Indexed"))
			indexed = 1;

		n = colorspace->n;
	}
	else
	{
		n = 1;
	}

	obj = fz_dict_getsa(dict, "Decode", "D");
	if (obj)
	{
		for (i = 0; i < n * 2; i++)
			decode[i] = fz_to_real(fz_array_get(obj, i));
	}
	else
	{
		float maxval = indexed ? (1 << bpc) - 1 : 1;
		for (i = 0; i < n * 2; i++)
			decode[i] = i & 1 ? maxval : 0;
	}

	obj = fz_dict_getsa(dict, "SMask", "Mask");
	if (fz_is_dict(obj))
	{
		/* Not allowed for inline images */
		if (!cstm)
		{
			error = pdf_load_image_imp(&mask, xref, rdb, obj, NULL, 1);
			if (error)
			{
				if (colorspace)
					fz_drop_colorspace(colorspace);
				return fz_rethrow(error, "cannot load image mask/softmask");
			}
		}
	}
	else if (fz_is_array(obj))
	{
		usecolorkey = 1;
		for (i = 0; i < n * 2; i++)
			colorkey[i] = fz_to_int(fz_array_get(obj, i));
	}

	/* Allocate now, to fail early if we run out of memory */
	tile = fz_new_pixmap_with_limit(colorspace, w, h);
	if (!tile)
	{
		if (colorspace)
			fz_drop_colorspace(colorspace);
		if (mask)
			fz_drop_pixmap(mask);
		return fz_throw("out of memory");
	}

	if (colorspace)
		fz_drop_colorspace(colorspace);

	tile->mask = mask;
	tile->interpolate = interpolate;

	stride = (w * n * bpc + 7) / 8;

	if (cstm)
	{
		stm = pdf_open_inline_stream(cstm, xref, dict, stride * h);
	}
	else
	{
		error = pdf_open_stream(&stm, xref, fz_to_num(dict), fz_to_gen(dict));
		if (error)
		{
			fz_drop_pixmap(tile);
			return fz_rethrow(error, "cannot open image data stream (%d 0 R)", fz_to_num(dict));
		}
	}

	samples = fz_calloc(h, stride);

	len = fz_read(stm, samples, h * stride);
	if (len < 0)
	{
		fz_close(stm);
		fz_free(samples);
		fz_drop_pixmap(tile);
		return fz_rethrow(len, "cannot read image data");
	}

	/* Make sure we read the EOF marker (for inline images only) */
	if (cstm)
	{
		unsigned char tbuf[512];
		int tlen = fz_read(stm, tbuf, sizeof tbuf);
		if (tlen < 0)
			fz_catch(tlen, "ignoring error at end of image");
		if (tlen > 0)
			fz_warn("ignoring garbage at end of image");
	}

	fz_close(stm);

	/* Pad truncated images */
	if (len < stride * h)
	{
		fz_warn("padding truncated image (%d 0 R)", fz_to_num(dict));
		memset(samples + len, 0, stride * h - len);
	}

	/* Invert 1-bit image masks */
	if (imagemask)
	{
		/* 0=opaque and 1=transparent so we need to invert */
		unsigned char *p = samples;
		len = h * stride;
		for (i = 0; i < len; i++)
			p[i] = ~p[i];
	}

	fz_unpack_tile(tile, samples, n, bpc, stride, indexed);

	fz_free(samples);

	if (usecolorkey)
		pdf_mask_color_key(tile, n, colorkey);

	if (indexed)
	{
		fz_pixmap *conv;
		fz_decode_indexed_tile(tile, decode, (1 << bpc) - 1);
		conv = pdf_expand_indexed_pixmap(tile);
		fz_drop_pixmap(tile);
		tile = conv;
	}
	else
	{
		fz_decode_tile(tile, decode);
	}

	*imgp = tile;
	return fz_okay;
}

fz_error
pdf_load_inline_image(fz_pixmap **pixp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *file)
{
	fz_error error;

	error = pdf_load_image_imp(pixp, xref, rdb, dict, file, 0);
	if (error)
		return fz_rethrow(error, "cannot load inline image");

	return fz_okay;
}

int
pdf_is_jpx_image(fz_obj *dict)
{
	fz_obj *filter;
	int i;

	filter = fz_dict_gets(dict, "Filter");
	if (!strcmp(fz_to_name(filter), "JPXDecode"))
		return 1;
	for (i = 0; i < fz_array_len(filter); i++)
		if (!strcmp(fz_to_name(fz_array_get(filter, i)), "JPXDecode"))
			return 1;
	return 0;
}

static fz_error
pdf_load_jpx_image(fz_pixmap **imgp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_buffer *buf;
	fz_colorspace *colorspace;
	fz_pixmap *img;
	fz_obj *obj;

	colorspace = NULL;

	error = pdf_load_stream(&buf, xref, fz_to_num(dict), fz_to_gen(dict));
	if (error)
		return fz_rethrow(error, "cannot load jpx image data");

	obj = fz_dict_gets(dict, "ColorSpace");
	if (obj)
	{
		error = pdf_load_colorspace(&colorspace, xref, obj);
		if (error)
			fz_catch(error, "cannot load image colorspace");
	}

	error = fz_load_jpx_image(&img, buf->data, buf->len, colorspace);
	if (error)
	{
		if (colorspace)
			fz_drop_colorspace(colorspace);
		fz_drop_buffer(buf);
		return fz_rethrow(error, "cannot load jpx image");
	}

	if (colorspace)
		fz_drop_colorspace(colorspace);
	fz_drop_buffer(buf);

	obj = fz_dict_getsa(dict, "SMask", "Mask");
	if (fz_is_dict(obj))
	{
		error = pdf_load_image_imp(&img->mask, xref, NULL, obj, NULL, 1);
		if (error)
		{
			fz_drop_pixmap(img);
			return fz_rethrow(error, "cannot load image mask/softmask");
		}
	}

	*imgp = img;
	return fz_okay;
}

fz_error
pdf_load_image(fz_pixmap **pixp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;

	if ((*pixp = pdf_find_item(xref->store, fz_drop_pixmap, dict)))
	{
		fz_keep_pixmap(*pixp);
		return fz_okay;
	}

	error = pdf_load_image_imp(pixp, xref, NULL, dict, NULL, 0);
	if (error)
		return fz_rethrow(error, "cannot load image (%d 0 R)", fz_to_num(dict));

	pdf_store_item(xref->store, fz_keep_pixmap, fz_drop_pixmap, dict, *pixp);

	return fz_okay;
}
