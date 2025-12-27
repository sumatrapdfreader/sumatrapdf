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

static BarcodeFormat formats[FZ_BARCODE__LIMIT] =
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

unsigned char *barcode_create(fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt, int *w, int *h, char *exception, int exception_len)
{
	int x, y, ps, rs;
	uint8_t *ret = NULL;

	/* The following has been carefully constructed to ensure
	 * that the mixture of C++ and fz error handling works OK.
	 * The try here doesn't call anything that might fz_throw.
	 * When the try exits, all the C++ objects should be
	 * destructed, leaving us just with a malloced temporary
	 * copy of the bitmap data. */
	{
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
				*h = bitmap.height();
				*w = bitmap.width();
				ret = (uint8_t *)malloc(*w * *h);
			}
			if (ret != NULL)
			{
				uint8_t *dst = ret;
				ps = bitmap.pixStride();
				rs = bitmap.rowStride();

				for (y = 0; y < *h; y++)
				{
					const uint8_t *s = src;
					src += rs;
					for (x = 0; x < *w; x++)
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
			 * block. That just leaves ret outstanding. */
			fz_strlcpy(exception, e.what(), exception_len);
		}
	}

	return ret;
}

#else

unsigned char *barcode_create(fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt, int *w, int *h, char *exception, int exception_len)
{
	int x, y;
	uint8_t *ret = NULL;

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
				*h = bitmap.height();
				*w = bitmap.width();
				ret = (uint8_t *)malloc(*w * *h);
			}
			if (ret != NULL)
			{
				uint8_t *dst = ret;
				for (y = 0; y < *h; y++)
				{
					const uint8_t *s = src;
					src += *w;
					for (x = 0; x < *w; x++)
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
			 * block. That just leaves ret outstanding. */
			fz_strlcpy(exception_text, e.what(), sizeof(exception_text));
		}
	}

	return ret;
}

#endif

char *barcode_decode_from_samples(fz_barcode_type *type, int n, int w, int h, unsigned char *samples, int rotate, char *exception, int exception_len)
{
	ImageFormat format;
	char *ret = NULL;

	if (n == 1)
		format = ImageFormat::Lum;
	else if (n == 3)
		format = ImageFormat::RGB;

	/* The following has been carefully constructed to ensure
	 * that the mixture of C++ and fz error handling works OK.
	 * The try here doesn't call anything that might fz_throw.
	 * When the try exits, all the C++ objects should be
	 * destructed, leaving us just with a malloced temporary
	 * copy of the results. */
	{
		try
		{
			ImageView image{samples, w, h, format};
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

			ret = strdup(str.c_str());
		}
		catch (std::exception & e)
		{
			/* If C++ throws an exception to here, we can trust
			 * that it will have freed everything in the above
			 * block. That just leaves ret outstanding. */
			fz_strlcpy(exception, e.what(), exception_len);
		}
	}

	return ret;
}

}

#endif /* FZ_ENABLE_BARCODE */
