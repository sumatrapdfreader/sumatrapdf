// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

static fz_image *pdf_load_jpx(fz_context *ctx, pdf_document *doc, pdf_obj *dict, int forcemask);

static fz_image *
pdf_load_jpx_imp(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, fz_stream *cstm, int forcemask)
{
	fz_image *image = pdf_load_jpx(ctx, doc, dict, forcemask);

	if (forcemask)
	{
		fz_pixmap_image *cimg = (fz_pixmap_image *)image;
		fz_pixmap *mask_pixmap;
		fz_pixmap *tile = fz_pixmap_image_tile(ctx, cimg);

		if (tile->n != 1)
		{
			fz_pixmap *gray = fz_convert_pixmap(ctx, tile, fz_device_gray(ctx), NULL, NULL, fz_default_color_params, 0);
			fz_drop_pixmap(ctx, tile);
			tile = gray;
		}

		mask_pixmap = fz_alpha_from_gray(ctx, tile);
		fz_drop_pixmap(ctx, tile);
		fz_set_pixmap_image_tile(ctx, cimg, mask_pixmap);
	}

	return image;
}

static fz_image *
pdf_load_image_imp(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, fz_stream *cstm, int forcemask)
{
	fz_image *image = NULL;
	pdf_obj *obj, *res;

	int w, h, bpc, n;
	int imagemask;
	int interpolate;
	int indexed;
	fz_image *mask = NULL; /* explicit mask/soft mask image */
	int use_colorkey = 0;
	fz_colorspace *colorspace = NULL;
	float decode[FZ_MAX_COLORS * 2];
	int colorkey[FZ_MAX_COLORS * 2];
	int stride;

	int i;
	fz_compressed_buffer *buffer;

	/* special case for JPEG2000 images */
	if (pdf_is_jpx_image(ctx, dict))
		return pdf_load_jpx_imp(ctx, doc, rdb, dict, cstm, forcemask);

	w = pdf_to_int(ctx, pdf_dict_geta(ctx, dict, PDF_NAME(Width), PDF_NAME(W)));
	h = pdf_to_int(ctx, pdf_dict_geta(ctx, dict, PDF_NAME(Height), PDF_NAME(H)));
	bpc = pdf_to_int(ctx, pdf_dict_geta(ctx, dict, PDF_NAME(BitsPerComponent), PDF_NAME(BPC)));
	if (bpc == 0)
		bpc = 8;
	imagemask = pdf_to_bool(ctx, pdf_dict_geta(ctx, dict, PDF_NAME(ImageMask), PDF_NAME(IM)));
	interpolate = pdf_to_bool(ctx, pdf_dict_geta(ctx, dict, PDF_NAME(Interpolate), PDF_NAME(I)));

	indexed = 0;
	use_colorkey = 0;

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
	if (SIZE_MAX / w < (size_t)(bpc+7)/8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image is too large");
	if (SIZE_MAX / h < w * (size_t)((bpc+7)/8))
		fz_throw(ctx, FZ_ERROR_GENERIC, "image is too large");

	fz_var(mask);
	fz_var(image);
	fz_var(colorspace);

	fz_try(ctx)
	{
		obj = pdf_dict_geta(ctx, dict, PDF_NAME(ColorSpace), PDF_NAME(CS));
		if (obj && !imagemask && !forcemask)
		{
			/* colorspace resource lookup is only done for inline images */
			if (pdf_is_name(ctx, obj))
			{
				res = pdf_dict_get(ctx, pdf_dict_get(ctx, rdb, PDF_NAME(ColorSpace)), obj);
				if (res)
					obj = res;
			}

			colorspace = pdf_load_colorspace(ctx, obj);
			indexed = fz_colorspace_is_indexed(ctx, colorspace);

			n = fz_colorspace_n(ctx, colorspace);
		}
		else
		{
			n = 1;
		}

		if (SIZE_MAX / n < h * ((size_t)w) * ((bpc+7)/8))
			fz_throw(ctx, FZ_ERROR_GENERIC, "image is too large");

		obj = pdf_dict_geta(ctx, dict, PDF_NAME(Decode), PDF_NAME(D));
		if (obj)
		{
			for (i = 0; i < n * 2; i++)
				decode[i] = pdf_array_get_real(ctx, obj, i);
		}
		else if (fz_colorspace_is_lab(ctx, colorspace))
		{
			decode[0] = 0;
			decode[1] = 100;
			decode[2] = -128;
			decode[3] = 127;
			decode[4] = -128;
			decode[5] = 127;
		}
		else
		{
			float maxval = indexed ? (1 << bpc) - 1 : 1;
			for (i = 0; i < n * 2; i++)
				decode[i] = i & 1 ? maxval : 0;
		}

		obj = pdf_dict_geta(ctx, dict, PDF_NAME(SMask), PDF_NAME(Mask));
		if (pdf_is_dict(ctx, obj))
		{
			/* Not allowed for inline images or soft masks */
			if (cstm)
				fz_warn(ctx, "Ignoring invalid inline image soft mask");
			else if (forcemask)
				fz_warn(ctx, "Ignoring recursive image soft mask");
			else
			{
				mask = pdf_load_image_imp(ctx, doc, rdb, obj, NULL, 1);
				obj = pdf_dict_get(ctx, obj, PDF_NAME(Matte));
				if (pdf_is_array(ctx, obj))
				{
					use_colorkey = 1;
					for (i = 0; i < n; i++)
						colorkey[i] = fz_clamp(pdf_array_get_real(ctx, obj, i), 0, 1) * 255;
				}
			}
		}
		else if (pdf_is_array(ctx, obj))
		{
			use_colorkey = 1;
			for (i = 0; i < n * 2; i++)
			{
				if (!pdf_is_int(ctx, pdf_array_get(ctx, obj, i)))
				{
					fz_warn(ctx, "invalid value in color key mask");
					use_colorkey = 0;
				}
				colorkey[i] = pdf_array_get_int(ctx, obj, i);
			}
		}

		/* Do we load from a ref, or do we load an inline stream? */
		if (cstm == NULL)
		{
			/* Just load the compressed image data now and we can decode it on demand. */
			size_t worst_case = w * (size_t)h;
			worst_case = (worst_case * bpc + 7) >> 3;
			if (colorspace)
				worst_case *= colorspace->n;
			buffer = pdf_load_compressed_stream(ctx, doc, pdf_to_num(ctx, dict), worst_case);
			image = fz_new_image_from_compressed_buffer(ctx, w, h, bpc, colorspace, 96, 96, interpolate, imagemask, decode, use_colorkey ? colorkey : NULL, buffer, mask);
		}
		else
		{
			/* Inline stream */
			stride = (w * n * bpc + 7) / 8;
			image = fz_new_image_from_compressed_buffer(ctx, w, h, bpc, colorspace, 96, 96, interpolate, imagemask, decode, use_colorkey ? colorkey : NULL, NULL, mask);
			pdf_load_compressed_inline_image(ctx, doc, dict, stride * h, cstm, indexed, (fz_compressed_image *)image);
		}
	}
	fz_always(ctx)
	{
		fz_drop_colorspace(ctx, colorspace);
		fz_drop_image(ctx, mask);
	}
	fz_catch(ctx)
	{
		fz_drop_image(ctx, image);
		fz_rethrow(ctx);
	}
	return image;
}

fz_image *
pdf_load_inline_image(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, fz_stream *file)
{
	return pdf_load_image_imp(ctx, doc, rdb, dict, file, 0);
}

int
pdf_is_jpx_image(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *filter;
	int i, n;

	filter = pdf_dict_get(ctx, dict, PDF_NAME(Filter));
	if (pdf_name_eq(ctx, filter, PDF_NAME(JPXDecode)))
		return 1;
	n = pdf_array_len(ctx, filter);
	for (i = 0; i < n; i++)
		if (pdf_name_eq(ctx, pdf_array_get(ctx, filter, i), PDF_NAME(JPXDecode)))
			return 1;
	return 0;
}

static fz_image *
pdf_load_jpx(fz_context *ctx, pdf_document *doc, pdf_obj *dict, int forcemask)
{
	fz_buffer *buf = NULL;
	fz_colorspace *colorspace = NULL;
	fz_pixmap *pix = NULL;
	pdf_obj *obj;
	fz_image *mask = NULL;
	fz_image *img = NULL;

	fz_var(pix);
	fz_var(buf);
	fz_var(colorspace);
	fz_var(mask);

	buf = pdf_load_stream(ctx, dict);

	/* FIXME: We can't handle decode arrays for indexed images currently */
	fz_try(ctx)
	{
		unsigned char *data;
		size_t len;

		obj = pdf_dict_get(ctx, dict, PDF_NAME(ColorSpace));
		if (obj)
			colorspace = pdf_load_colorspace(ctx, obj);

		len = fz_buffer_storage(ctx, buf, &data);
		pix = fz_load_jpx(ctx, data, len, colorspace);

		obj = pdf_dict_geta(ctx, dict, PDF_NAME(SMask), PDF_NAME(Mask));
		if (pdf_is_dict(ctx, obj))
		{
			if (forcemask)
				fz_warn(ctx, "Ignoring recursive JPX soft mask");
			else
				mask = pdf_load_image_imp(ctx, doc, NULL, obj, NULL, 1);
		}

		obj = pdf_dict_geta(ctx, dict, PDF_NAME(Decode), PDF_NAME(D));
		if (obj && !fz_colorspace_is_indexed(ctx, colorspace))
		{
			float decode[FZ_MAX_COLORS * 2];
			int i;

			for (i = 0; i < pix->n * 2; i++)
				decode[i] = pdf_array_get_real(ctx, obj, i);

			fz_decode_tile(ctx, pix, decode);
		}

		img = fz_new_image_from_pixmap(ctx, pix, mask);
	}
	fz_always(ctx)
	{
		fz_drop_image(ctx, mask);
		fz_drop_pixmap(ctx, pix);
		fz_drop_colorspace(ctx, colorspace);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_morph_error(ctx, FZ_ERROR_GENERIC, FZ_ERROR_MINOR);
		fz_rethrow(ctx);
	}

	return img;
}

fz_image *
pdf_load_image(fz_context *ctx, pdf_document *doc, pdf_obj *dict)
{
	fz_image *image;

	if ((image = pdf_find_item(ctx, fz_drop_image_imp, dict)) != NULL)
		return image;

	image = pdf_load_image_imp(ctx, doc, NULL, dict, NULL, 0);
	pdf_store_item(ctx, dict, image, fz_image_size(ctx, image));
	return image;
}

struct jbig2_segment_header {
	int number;
	int flags;
	/* referred-to-segment numbers */
	int page;
	int length;
};

/* coverity[-tainted_data_return] */
static uint32_t getu32(const unsigned char *data)
{
	return ((uint32_t)data[0]<<24) | ((uint32_t)data[1]<<16) | ((uint32_t)data[2]<<8) | (uint32_t)data[3];
}

static size_t
pdf_parse_jbig2_segment_header(fz_context *ctx,
	const unsigned char *data, const unsigned char *end,
	struct jbig2_segment_header *info)
{
	uint32_t rts;
	size_t n = 5;

	if (data + 11 > end) return 0;

	info->number = getu32(data);
	info->flags = data[4];

	rts = (data[5] >> 5) & 0x7;
	if (rts == 7)
	{
		rts = getu32(data+5) & 0x1FFFFFFF;
		n += 4 + (rts + 1) / 8;
	}
	else
	{
		n += 1;
	}

	if (info->number <= 256)
		n += rts;
	else if (info->number <= 65536)
		n += rts * 2;
	else
		n += rts * 4;

	if (info->flags & 0x40)
	{
		if (data + n + 4 > end) return 0;
		info->page = getu32(data+n);
		n += 4;
	}
	else
	{
		if (data + n + 1 > end) return 0;
		info->page = data[n];
		n += 1;
	}

	if (data + n + 4 > end) return 0;
	info->length = getu32(data+n);
	return n + 4;
}

static void
pdf_copy_jbig2_segments(fz_context *ctx, fz_buffer *output, const unsigned char *data, size_t size, int page)
{
	struct jbig2_segment_header info;
	const unsigned char *end = data + size;
	size_t n;
	int type;

	while (data < end)
	{
		n = pdf_parse_jbig2_segment_header(ctx, data, end, &info);
		if (n == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "truncated jbig2 segment header");

		/* omit end of page, end of file, and segments for other pages */
		type = (info.flags & 63);
		if (type == 49 || type == 51 || (info.page > 0 && info.page != page))
		{
			data += n;
			data += info.length;
		}
		else
		{
			fz_append_data(ctx, output, data, n);
			data += n;
			if (data + info.length > end)
				fz_throw(ctx, FZ_ERROR_GENERIC, "truncated jbig2 segment data");
			fz_append_data(ctx, output, data, info.length);
			data += info.length;
		}
	}
}

static void
pdf_copy_jbig2_random_segments(fz_context *ctx, fz_buffer *output, const unsigned char *data, size_t size, int page)
{
	struct jbig2_segment_header info;
	const unsigned char *header = data;
	const unsigned char *header_end;
	const unsigned char *end = data + size;
	size_t n;
	int type;

	/* Skip headers until end-of-file segment is found. */
	while (data < end)
	{
		n = pdf_parse_jbig2_segment_header(ctx, data, end, &info);
		if (n == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "truncated jbig2 segment header");
		data += n;
		if ((info.flags & 63) == 51)
			break;
	}
	if (data >= end)
		fz_throw(ctx, FZ_ERROR_GENERIC, "truncated jbig2 segment header");

	/* Copy segment headers and segment data */
	header_end = data;
	while (data < end && header < header_end)
	{
		n = pdf_parse_jbig2_segment_header(ctx, header, header_end, &info);
		if (n == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "truncated jbig2 segment header");

		/* omit end of page, end of file, and segments for other pages */
		type = (info.flags & 63);
		if (type == 49 || type == 51 || (info.page > 0 && info.page != page))
		{
			header += n;
			data += info.length;
		}
		else
		{
			fz_append_data(ctx, output, header, n);
			header += n;
			if (data + info.length > end)
				fz_throw(ctx, FZ_ERROR_GENERIC, "truncated jbig2 segment data");
			fz_append_data(ctx, output, data, info.length);
			data += info.length;
		}
	}
}

static fz_buffer *
pdf_jbig2_stream_from_file(fz_context *ctx, fz_buffer *input, fz_jbig2_globals *globals_, int page)
{
	fz_buffer *globals = fz_jbig2_globals_data(ctx, globals_);
	size_t globals_size = globals ? globals->len : 0;
	fz_buffer *output;
	int flags;
	size_t header = 9;

	if (input->len < 9)
		return NULL; /* not enough data! */
	flags = input->data[8];
	if ((flags & 2) == 0)
	{
		if (input->len < 13)
			return NULL; /* not enough data! */
		header = 13;
	}

	output = fz_new_buffer(ctx, input->len + globals_size);
	fz_try(ctx)
	{
		if (globals_size > 0)
			fz_append_buffer(ctx, output, globals);
		if ((flags & 1) == 0)
			pdf_copy_jbig2_random_segments(ctx, output, input->data + header, input->len - header, page);
		else
			pdf_copy_jbig2_segments(ctx, output, input->data + header, input->len - header, page);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, output);
		fz_rethrow(ctx);
	}

	return output;
}

pdf_obj *
pdf_add_image(fz_context *ctx, pdf_document *doc, fz_image *image)
{
	fz_pixmap *pixmap = NULL;
	pdf_obj *imobj = NULL;
	pdf_obj *dp;
	fz_buffer *buffer = NULL;
	fz_compressed_buffer *cbuffer;
	fz_pixmap *smask_pixmap = NULL;
	fz_image *smask_image = NULL;
	int i, n;

	fz_var(pixmap);
	fz_var(buffer);
	fz_var(imobj);
	fz_var(smask_pixmap);
	fz_var(smask_image);

	pdf_begin_operation(ctx, doc, "Add image");

	fz_try(ctx)
	{
		/* If we can maintain compression, do so */
		cbuffer = fz_compressed_image_buffer(ctx, image);

		imobj = pdf_add_new_dict(ctx, doc, 3);

		dp = pdf_dict_put_dict(ctx, imobj, PDF_NAME(DecodeParms), 3);
		pdf_dict_put(ctx, imobj, PDF_NAME(Type), PDF_NAME(XObject));
		pdf_dict_put(ctx, imobj, PDF_NAME(Subtype), PDF_NAME(Image));

		if (cbuffer)
		{
			fz_compression_params *cp = &cbuffer->params;
			switch (cp->type)
			{
			default:
				goto unknown_compression;
			case FZ_IMAGE_RAW:
				break;
			case FZ_IMAGE_JPEG:
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(DCTDecode));
				if (cp->u.jpeg.color_transform >= 0)
					pdf_dict_put_int(ctx, dp, PDF_NAME(ColorTransform), cp->u.jpeg.color_transform);
				if (cp->u.jpeg.invert_cmyk && image->n == 4)
				{
					pdf_obj *arr;
					arr = pdf_dict_put_array(ctx, imobj, PDF_NAME(Decode), 8);
					pdf_array_push_int(ctx, arr, 1);
					pdf_array_push_int(ctx, arr, 0);
					pdf_array_push_int(ctx, arr, 1);
					pdf_array_push_int(ctx, arr, 0);
					pdf_array_push_int(ctx, arr, 1);
					pdf_array_push_int(ctx, arr, 0);
					pdf_array_push_int(ctx, arr, 1);
					pdf_array_push_int(ctx, arr, 0);
				}
				break;
			case FZ_IMAGE_JPX:
				if (cp->u.jpx.smask_in_data)
					pdf_dict_put_int(ctx, dp, PDF_NAME(SMaskInData), cp->u.jpx.smask_in_data);
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(JPXDecode));
				break;
			case FZ_IMAGE_JBIG2:
				if (cp->u.jbig2.embedded && cp->u.jbig2.globals)
				{
					pdf_obj *globals_ref = pdf_add_new_dict(ctx, doc, 1);
					pdf_update_stream(ctx, doc, globals_ref, fz_jbig2_globals_data(ctx, cp->u.jbig2.globals), 0);
					pdf_dict_put(ctx, dp, PDF_NAME(JBIG2Globals), globals_ref);
				}
				else
					buffer = pdf_jbig2_stream_from_file(ctx, cbuffer->buffer,
						cp->u.jbig2.globals,
						1);
				if (!buffer)
					goto unknown_compression;
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(JBIG2Decode));
				break;
			case FZ_IMAGE_FAX:
				if (cp->u.fax.columns)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Columns), cp->u.fax.columns);
				if (cp->u.fax.rows)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Rows), cp->u.fax.rows);
				if (cp->u.fax.k)
					pdf_dict_put_int(ctx, dp, PDF_NAME(K), cp->u.fax.k);
				if (cp->u.fax.end_of_line)
					pdf_dict_put_bool(ctx, dp, PDF_NAME(EndOfLine), cp->u.fax.end_of_line);
				if (cp->u.fax.encoded_byte_align)
					pdf_dict_put_bool(ctx, dp, PDF_NAME(EncodedByteAlign), cp->u.fax.encoded_byte_align);
				if (cp->u.fax.end_of_block)
					pdf_dict_put_bool(ctx, dp, PDF_NAME(EndOfBlock), cp->u.fax.end_of_block);
				if (cp->u.fax.black_is_1)
					pdf_dict_put_bool(ctx, dp, PDF_NAME(BlackIs1), cp->u.fax.black_is_1);
				if (cp->u.fax.damaged_rows_before_error)
					pdf_dict_put_int(ctx, dp, PDF_NAME(DamagedRowsBeforeError), cp->u.fax.damaged_rows_before_error);
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(CCITTFaxDecode));
				break;
			case FZ_IMAGE_FLATE:
				if (cp->u.flate.columns)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Columns), cp->u.flate.columns);
				if (cp->u.flate.colors)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Colors), cp->u.flate.colors);
				if (cp->u.flate.predictor)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Predictor), cp->u.flate.predictor);
				if (cp->u.flate.bpc)
					pdf_dict_put_int(ctx, dp, PDF_NAME(BitsPerComponent), cp->u.flate.bpc);
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(FlateDecode));
				break;
			case FZ_IMAGE_LZW:
				if (cp->u.lzw.columns)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Columns), cp->u.lzw.columns);
				if (cp->u.lzw.colors)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Colors), cp->u.lzw.colors);
				if (cp->u.lzw.predictor)
					pdf_dict_put_int(ctx, dp, PDF_NAME(Predictor), cp->u.lzw.predictor);
				if (cp->u.lzw.early_change)
					pdf_dict_put_int(ctx, dp, PDF_NAME(EarlyChange), cp->u.lzw.early_change);
				if (cp->u.lzw.bpc)
					pdf_dict_put_int(ctx, dp, PDF_NAME(BitsPerComponent), cp->u.lzw.bpc);
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(LZWDecode));
				break;
			case FZ_IMAGE_RLD:
				pdf_dict_put(ctx, imobj, PDF_NAME(Filter), PDF_NAME(RunLengthDecode));
				break;
			}

			if (!pdf_dict_len(ctx, dp))
				pdf_dict_del(ctx, imobj, PDF_NAME(DecodeParms));

			pdf_dict_put_int(ctx, imobj, PDF_NAME(BitsPerComponent), image->bpc);
			pdf_dict_put_int(ctx, imobj, PDF_NAME(Width), image->w);
			pdf_dict_put_int(ctx, imobj, PDF_NAME(Height), image->h);

			if (!buffer)
				buffer = fz_keep_buffer(ctx, cbuffer->buffer);

			if (image->use_decode)
			{
				pdf_obj *ary = pdf_dict_put_array(ctx, imobj, PDF_NAME(Decode), image->n * 2);
				for (i = 0; i < image->n * 2; ++i)
					pdf_array_push_real(ctx, ary, image->decode[i]);
			}
		}
		else
		{
unknown_compression:

			pixmap = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
			n = pixmap->n - pixmap->alpha - pixmap->s; /* number of colorants */
			if (n == 0)
				n = 1; /* treat pixmaps with only alpha or spots as grayscale */

			pdf_dict_put_int(ctx, imobj, PDF_NAME(Width), pixmap->w);
			pdf_dict_put_int(ctx, imobj, PDF_NAME(Height), pixmap->h);

			if (fz_is_pixmap_monochrome(ctx, pixmap))
			{
				int stride = (image->w + 7) / 8;
				int h = pixmap->h;
				int w = pixmap->w;
				unsigned char *s = pixmap->samples;
				unsigned char *d = fz_calloc(ctx, h, stride);
				buffer = fz_new_buffer_from_data(ctx, d, (size_t)h * stride);

				pdf_dict_put_int(ctx, imobj, PDF_NAME(BitsPerComponent), 1);

				while (h--)
				{
					int x;
					for (x = 0; x < w; ++x)
						if (s[x] > 0)
							d[x>>3] |= 1 << (7 - (x & 7));
					s += pixmap->stride;
					d += stride;
				}
			}
			else
			{
				size_t size = (size_t)pixmap->w * n;
				int h = pixmap->h;
				unsigned char *s = pixmap->samples;
				unsigned char *d = Memento_label(fz_malloc(ctx, size * h), "pdf_image_samples");
				buffer = fz_new_buffer_from_data(ctx, d, size * h);

				pdf_dict_put_int(ctx, imobj, PDF_NAME(BitsPerComponent), 8);

				if (n == pixmap->n)
				{
					/* If we use all channels, we can copy the data as is. */
					while (h--)
					{
						memcpy(d, s, size);
						d += size;
						s += pixmap->stride;
					}
				}
				else
				{
					size_t line_skip;
					int skip;

					/* Need to extract the alpha into a SMask and remove spot planes. */
					/* TODO: convert spots to colors. */

					if (pixmap->alpha && !image->mask)
					{
						smask_pixmap = fz_new_pixmap_from_alpha_channel(ctx, pixmap);
						smask_image = fz_new_image_from_pixmap(ctx, smask_pixmap, NULL);
						pdf_dict_put_drop(ctx, imobj, PDF_NAME(SMask), pdf_add_image(ctx, doc, smask_image));
						fz_drop_image(ctx, smask_image);
						smask_image = NULL;
						fz_drop_pixmap(ctx, smask_pixmap);
						smask_pixmap = NULL;
					}

					line_skip = pixmap->stride - pixmap->w * (size_t)pixmap->n;
					skip = pixmap->n - n;
					if (pixmap->alpha)
					{
						int n1 = pixmap->n-1;
						while (h--)
						{
							int w = pixmap->w;
							while (w--)
							{
								int a = s[n1];
								int inva = a ? 255 * 256 / a : 0;
								int k;
								for (k = 0; k < n; k++)
									*d++ = (*s++ * inva) >> 8;
								s += skip;
							}
							s += line_skip;
						}
					}
					else
					{
						while (h--)
						{
							int w = pixmap->w;
							while (w--)
							{
								int k;
								for (k = 0; k < n; ++k)
									*d++ = *s++;
								s += skip;
							}
							s += line_skip;
						}
					}
				}
			}
		}

		if (image->imagemask)
		{
			pdf_dict_put_bool(ctx, imobj, PDF_NAME(ImageMask), 1);
		}
		else
		{
			fz_colorspace *cs;

			cs = pixmap ? pixmap->colorspace : image->colorspace;
			switch (fz_colorspace_type(ctx, cs))
			{
			case FZ_COLORSPACE_INDEXED:
				{
					fz_colorspace *basecs;
					unsigned char *lookup = NULL;
					int high = 0;
					int basen;
					pdf_obj *arr;

					basecs = cs->u.indexed.base;
					high = cs->u.indexed.high;
					lookup = cs->u.indexed.lookup;
					basen = basecs->n;

					arr = pdf_dict_put_array(ctx, imobj, PDF_NAME(ColorSpace), 4);

					pdf_array_push(ctx, arr, PDF_NAME(Indexed));
					switch (fz_colorspace_type(ctx, basecs))
					{
					case FZ_COLORSPACE_GRAY:
						pdf_array_push(ctx, arr, PDF_NAME(DeviceGray));
						break;
					case FZ_COLORSPACE_RGB:
						pdf_array_push(ctx, arr, PDF_NAME(DeviceRGB));
						break;
					case FZ_COLORSPACE_CMYK:
						pdf_array_push(ctx, arr, PDF_NAME(DeviceCMYK));
						break;
					default:
						// TODO: convert to RGB!
						fz_throw(ctx, FZ_ERROR_GENERIC, "only indexed Gray, RGB, and CMYK colorspaces supported");
						break;
					}

					pdf_array_push_int(ctx, arr, high);
					pdf_array_push_string(ctx, arr, (char *) lookup, (size_t)basen * (high + 1));
				}
				break;
			case FZ_COLORSPACE_NONE:
			case FZ_COLORSPACE_GRAY:
				pdf_dict_put(ctx, imobj, PDF_NAME(ColorSpace), PDF_NAME(DeviceGray));
				break;
			case FZ_COLORSPACE_RGB:
				pdf_dict_put(ctx, imobj, PDF_NAME(ColorSpace), PDF_NAME(DeviceRGB));
				break;
			case FZ_COLORSPACE_CMYK:
				pdf_dict_put(ctx, imobj, PDF_NAME(ColorSpace), PDF_NAME(DeviceCMYK));
				break;
			case FZ_COLORSPACE_LAB:
				pdf_dict_put(ctx, imobj, PDF_NAME(ColorSpace), PDF_NAME(Lab));
				break;
			default:
				// TODO: convert to RGB!
				fz_throw(ctx, FZ_ERROR_GENERIC, "only Gray, RGB, and CMYK colorspaces supported");
				break;
			}
		}

		if (image->mask)
		{
			if (image->mask->imagemask)
				pdf_dict_put_drop(ctx, imobj, PDF_NAME(Mask), pdf_add_image(ctx, doc, image->mask));
			else
				pdf_dict_put_drop(ctx, imobj, PDF_NAME(SMask), pdf_add_image(ctx, doc, image->mask));
		}

		pdf_update_stream(ctx, doc, imobj, buffer, 1);
		pdf_end_operation(ctx, doc);
	}
	fz_always(ctx)
	{
		fz_drop_image(ctx, smask_image);
		fz_drop_pixmap(ctx, smask_pixmap);
		fz_drop_pixmap(ctx, pixmap);
		fz_drop_buffer(ctx, buffer);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, imobj);
		pdf_abandon_operation(ctx, doc);
		fz_rethrow(ctx);
	}
	return imobj;
}
