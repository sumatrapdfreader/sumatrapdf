// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#if FZ_ENABLE_BARCODE
#include "zxingbarcode.h"
#endif

fz_pixmap *fz_new_barcode_pixmap(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt)
{
#if !FZ_ENABLE_BARCODE
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
#else
	char exception[256] = { 0 };
	unsigned char *ret;
	fz_pixmap *pix;
	int w, h;

	if (type <= FZ_BARCODE_NONE || type >= FZ_BARCODE__LIMIT)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported barcode type");
	if (ec_level < 0 || ec_level >= 8)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported barcode error correction level");

	ret = barcode_create(type, value, size, ec_level, quiet, hrt, &w, &h, exception, nelem(exception));
	if (ret == NULL || exception[0])
	{
		free(ret);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode generation failed");
	}
	fz_try(ctx)
	{
		pix = fz_new_pixmap(ctx, fz_device_gray(ctx), w, h, NULL, 0);
#ifdef ZXING_EXPERIMENTAL_API
		pix->xres = 72;
		pix->yres = 72;
#else
		pix->xres = 36;
		pix->yres = 36;
#endif
		memcpy(pix->samples, ret, w*h);
	}
	fz_always(ctx)
		free(ret);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return pix;
#endif
}

char *fz_decode_barcode_from_pixmap(fz_context *ctx, fz_barcode_type *type, fz_pixmap *pix, int rotate)
{
#if !FZ_ENABLE_BARCODE
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
#else
	char *ret, *contents;
	char exception[256] = { 0 };
	if (pix == NULL)
		return NULL;
	if (pix->n != 1 && pix->n != 3)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Barcodes can be greyscale or RGB only");
	ret = barcode_decode_from_samples(type, pix->n, pix->w, pix->h, pix->samples, rotate, exception, nelem(exception));
	if (exception[0])
	{
		free(ret);
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode exception: %s", exception);
	}
	fz_try(ctx)
		contents = fz_strdup(ctx, ret);
	fz_always(ctx)
		free(ret);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return contents;
#endif
}

static const char *fz_barcode_type_strings[FZ_BARCODE__LIMIT] =
{
	"none",
	"aztec",
	"codabar",
	"code39",
	"code93",
	"code128",
	"databar",
	"databarexpanded",
	"datamatrix",
	"ean8",
	"ean13",
	"itf",
	"maxicode",
	"pdf417",
	"qrcode",
	"upca",
	"upce",
	"microqrcode",
	"rmqrcode",
	"dxfilmedge",
	"databarlimited"
};

fz_barcode_type
fz_barcode_type_from_string(const char *str)
{
	int n = nelem(fz_barcode_type_strings);
	int i;

	if (str == NULL)
		return FZ_BARCODE_NONE;

	for (i = 1; i < n; i++)
	{
		if (!fz_strcasecmp(str, fz_barcode_type_strings[i]))
			return (fz_barcode_type)i;
	}
	return FZ_BARCODE_NONE;
}

const char *
fz_string_from_barcode_type(fz_barcode_type type)
{
	if (type < 0 || type >= FZ_BARCODE__LIMIT)
		return "unknown";
	return fz_barcode_type_strings[type];
}

fz_image *fz_new_barcode_image(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt)
{
	fz_pixmap *pix = fz_new_barcode_pixmap(ctx, type, value, size, ec_level, quiet, hrt);
	fz_image *image;

	fz_try(ctx)
		image = fz_new_image_from_pixmap(ctx, pix, NULL);
	fz_always(ctx)
		fz_drop_pixmap(ctx, pix);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return image;
}

char *fz_decode_barcode_from_display_list(fz_context *ctx, fz_barcode_type *type, fz_display_list *list, fz_rect subarea, int rotate)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	char *str;

	rect = fz_bound_display_list(ctx, list);
	rect = fz_intersect_rect(rect, subarea);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, fz_device_gray(ctx), bbox, NULL, 0);
	fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		fz_fill_pixmap_from_display_list(ctx, list, fz_identity, pix);
		str = fz_decode_barcode_from_pixmap(ctx, type, pix, rotate);
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return str;
}

char *fz_decode_barcode_from_page(fz_context *ctx, fz_barcode_type *type, fz_page *page, fz_rect subarea, int rotate)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev = NULL;
	char *str;

	fz_var(dev);

	rect = fz_bound_page(ctx, page);
	rect = fz_intersect_rect(rect, subarea);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, fz_device_gray(ctx), bbox, NULL, 0);
	fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, fz_identity, pix);
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);

		str = fz_decode_barcode_from_pixmap(ctx, type, pix, rotate);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_pixmap(ctx, pix);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return str;
}
