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

#ifndef HAVE_ZXINGCPP
#error "FZ_ENABLE_BARCODE set without HAVE_ZXINGCPP!"
#endif

#include "ReadBarcode.h"

#ifdef ZXING_EXPERIMENTAL_API
#include "WriteBarcode.h"
#else
#include "BitMatrix.h"
#include "MultiFormatWriter.h"
#endif

using namespace ZXing;

extern "C"
{

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

BarcodeFormat formats[FZ_BARCODE__LIMIT] =
{
	BarcodeFormat::None,
	BarcodeFormat::Aztec,
	BarcodeFormat::Codabar,
	BarcodeFormat::Code39,
	BarcodeFormat::Code93,
	BarcodeFormat::Code128,
	BarcodeFormat::DataBar,
	BarcodeFormat::DataBarExpanded,
	BarcodeFormat::DataMatrix,
	BarcodeFormat::EAN8,
	BarcodeFormat::EAN13,
	BarcodeFormat::ITF,
	BarcodeFormat::MaxiCode,
	BarcodeFormat::PDF417,
	BarcodeFormat::QRCode,
	BarcodeFormat::UPCA,
	BarcodeFormat::UPCE,
	BarcodeFormat::MicroQRCode,
#ifdef ZXING_EXPERIMENTAL_API
	BarcodeFormat::RMQRCode,
	BarcodeFormat::DXFilmEdge,
	BarcodeFormat::DataBarLimited,
#endif
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

static fz_barcode_type
format_to_fz(BarcodeFormat format)
{
	int n = nelem(formats);
	int i;

	for (i = 1; i < n; i++)
	{
		if (format == formats[i])
			return (fz_barcode_type)i;
	}
	return FZ_BARCODE_NONE;
}

#ifdef ZXING_EXPERIMENTAL_API

fz_pixmap *fz_new_barcode_pixmap(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt)
{
	fz_pixmap *pix;
	int x, y, w, h, ps, rs;
	uint8_t *tmp_copy = NULL;

	if (type <= FZ_BARCODE_NONE || type >= FZ_BARCODE__LIMIT)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported barcode type");
	if (ec_level < 0 || ec_level >= 8)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported barcode error correction level");

	/* The following has been carefully constructed to ensure
	 * that the mixture of C++ and fz error handling works OK.
	 * The try here doesn't call anything that might fz_throw.
	 * When the try exits, all the C++ objects should be
	 * destructed, leaving us just with a malloced temporary
	 * copy of the bitmap data. */
	{
		char exception_text[256] = "";
		try
		{
			char ec_str[16];
			BarcodeFormat format = formats[type];
			sprintf(ec_str, "%d", ec_level);
			CreatorOptions cOpts = CreatorOptions(format).ecLevel(ec_str);
			Barcode barcode = CreateBarcodeFromText(value, cOpts);
			WriterOptions wOpts = WriterOptions().withQuietZones(!!quiet).withHRT(!!hrt).rotate(0);
			if (size)
				wOpts.sizeHint(size);
			else
				wOpts.scale(2); /* Smallest size that gives text */
			Image bitmap = WriteBarcodeToImage(barcode, wOpts);
			const uint8_t *src = bitmap.data();
			if (src != NULL)
			{
				h = bitmap.height();
				w = bitmap.width();
				tmp_copy = (uint8_t *)malloc(w * h);
			}
			if (tmp_copy != NULL)
			{
				uint8_t *dst = tmp_copy;
				ps = bitmap.pixStride();
				rs = bitmap.rowStride();

				for (y = 0; y < h; y++)
				{
					const uint8_t *s = src;
					src += rs;
					for (x = 0; x < w; x++)
					{
						*dst++ = *s;
						s += ps;
					}
				}
			}
		}
		catch (std::exception & e)
		{
			/* If C++ throws an exception to here, we can trust
			 * that it will have freed everything in the above
			 * block. That just leaves tmp_copy outstanding. */
			free(tmp_copy);
			fz_strlcpy(exception_text, e.what(), sizeof(exception_text));
		}
		if (exception_text[0])
			fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode exception: %s", exception_text);
	}

	if (tmp_copy == NULL)
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode generation failed");

	fz_try(ctx)
	{
		pix = fz_new_pixmap(ctx, fz_device_gray(ctx), w, h, NULL, 0);
		pix->xres = 72;
		pix->yres = 72;
		memcpy(pix->samples, tmp_copy, w*h);
	}
	fz_always(ctx)
		free(tmp_copy);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return pix;
}

#else

fz_pixmap *fz_new_barcode_pixmap(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt)
{
	fz_pixmap *pix;
	int x, y, w, h;
	uint8_t *tmp_copy = NULL;

	if (type <= FZ_BARCODE_NONE || type >= FZ_BARCODE__LIMIT)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported barcode type");
	if (ec_level < 0 || ec_level >= 8)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported barcode error correction level");

	/* The following has been carefully constructed to ensure
	 * that the mixture of C++ and fz error handling works OK.
	 * The try here doesn't call anything that might fz_throw.
	 * When the try exits, all the C++ objects should be
	 * destructed, leaving us just with a malloced temporary
	 * copy of the bitmap data. */
	{
		char exception_text[256] = "";
		try
		{
			auto format = formats[type];
			auto writer = MultiFormatWriter(format);
			BitMatrix matrix;
			writer.setEncoding(CharacterSet::UTF8);
			writer.setMargin(quiet ? 10 : 0);
			writer.setEccLevel(ec_level);
			matrix = writer.encode(value, size, size ? size : 50);
			auto bitmap = ToMatrix<uint8_t>(matrix);

			// TODO: human readable text (not available with non-experimental API)

			const uint8_t *src = bitmap.data();
			if (src != NULL)
			{
				h = bitmap.height();
				w = bitmap.width();
				tmp_copy = (uint8_t *)malloc(w * h);
			}
			if (tmp_copy != NULL)
			{
				uint8_t *dst = tmp_copy;
				for (y = 0; y < h; y++)
				{
					const uint8_t *s = src;
					src += w;
					for (x = 0; x < w; x++)
					{
						*dst++ = *s;
						s ++;
					}
				}
			}
		}
		catch (std::exception & e)
		{
			/* If C++ throws an exception to here, we can trust
			 * that it will have freed everything in the above
			 * block. That just leaves tmp_copy outstanding. */
			free(tmp_copy);
			fz_strlcpy(exception_text, e.what(), sizeof(exception_text));
		}
		if (exception_text[0])
			fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode exception: %s", exception_text);
	}

	if (tmp_copy == NULL)
		fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode generation failed");

	fz_try(ctx)
	{
		pix = fz_new_pixmap(ctx, fz_device_gray(ctx), w, h, NULL, 0);
		pix->xres = 36;
		pix->yres = 36;
		memcpy(pix->samples, tmp_copy, w*h);
	}
	fz_always(ctx)
		free(tmp_copy);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return pix;
}

#endif

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


char *fz_decode_barcode_from_pixmap(fz_context *ctx, fz_barcode_type *type, fz_pixmap *pix, int rotate)
{
	ImageFormat format;
	char *ret = NULL;
	char *tmp_copy = NULL;

	if (pix == NULL)
		return NULL;

	if (pix->n == 1)
		format = ImageFormat::Lum;
	else if (pix->n == 3)
		format = ImageFormat::RGB;
	else
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Barcodes can be greyscale or RGB only");

	/* The following has been carefully constructed to ensure
	 * that the mixture of C++ and fz error handling works OK.
	 * The try here doesn't call anything that might fz_throw.
	 * When the try exits, all the C++ objects should be
	 * destructed, leaving us just with a malloced temporary
	 * copy of the results. */
	{
		char exception_text[256] = "";
		try
		{
			ImageView image{pix->samples, pix->w, pix->h, format};
			auto barcode = ReadBarcode(image.rotated(rotate));
			std::string str;

			if (barcode.isValid())
				str = barcode.text(TextMode::Escaped);
			else if (barcode.error())
				str = ToString(barcode.error());
			else
				str = "Unknown " + ToString(barcode.format());
			if (type)
				*type = format_to_fz(barcode.format());

			tmp_copy = strdup(str.c_str());
		}
		catch (std::exception & e)
		{
			/* If C++ throws an exception to here, we can trust
			 * that it will have freed everything in the above
			 * block. That just leaves tmp_copy outstanding. */
			free(tmp_copy);
			fz_strlcpy(exception_text, e.what(), sizeof(exception_text));
		}
		if (exception_text[0])
			fz_throw(ctx, FZ_ERROR_LIBRARY, "Barcode exception: %s", exception_text);
	}

	fz_try(ctx)
		if (tmp_copy)
			ret = fz_strdup(ctx, tmp_copy);
	fz_always(ctx)
		free(tmp_copy);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
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

}

#else

extern "C"
{

char *fz_decode_barcode_from_display_list(fz_context *ctx, fz_barcode_type *type, fz_display_list *list, fz_rect subarea, int rotate)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
}

char *fz_decode_barcode_from_pixmap(fz_context *ctx, fz_barcode_type *type, fz_pixmap *pix, int rotate)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
}

fz_image *fz_new_barcode_image(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
}

char *fz_decode_barcode_from_page(fz_context *ctx, fz_barcode_type *type, fz_page *page, fz_rect subarea, int rotate)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
}

fz_pixmap *fz_new_barcode_pixmap(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Barcode functionality not included");
}

fz_barcode_type fz_barcode_type_from_string(const char *str)
{
	return FZ_BARCODE_NONE;
}

const char *fz_string_from_barcode_type(fz_barcode_type type)
{
	return "unknown";
}

}

#endif /* FZ_ENABLE_BARCODE */
