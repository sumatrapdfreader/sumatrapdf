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

#ifndef MUPDF_FITZ_BARCODE_H
#define MUPDF_FITZ_BARCODE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/document.h"
#include "mupdf/fitz/display-list.h"

typedef enum
{
	FZ_BARCODE_NONE = 0,
	FZ_BARCODE_AZTEC,
	FZ_BARCODE_CODABAR,
	FZ_BARCODE_CODE39,
	FZ_BARCODE_CODE93,
	FZ_BARCODE_CODE128,
	FZ_BARCODE_DATABAR,
	FZ_BARCODE_DATABAREXPANDED,
	FZ_BARCODE_DATAMATRIX,
	FZ_BARCODE_EAN8,
	FZ_BARCODE_EAN13,
	FZ_BARCODE_ITF,
	FZ_BARCODE_MAXICODE,
	FZ_BARCODE_PDF417,
	FZ_BARCODE_QRCODE,
	FZ_BARCODE_UPCA,
	FZ_BARCODE_UPCE,
	FZ_BARCODE_MICROQRCODE,
	FZ_BARCODE_RMQRCODE,
	FZ_BARCODE_DXFILMEDGE,
	FZ_BARCODE_DATABARLIMITED,

	FZ_BARCODE__LIMIT
} fz_barcode_type;

/**
	Return barcode string matching one of the above barcode types.
	All lowercase, e.g. "none", "aztec" etc.
*/
const char *fz_string_from_barcode_type(fz_barcode_type type);

/**
	Helper function to search the above list (case insensitively)
	for an exact match. Returns FZ_BARCODE_NONE if no match found.
*/
fz_barcode_type fz_barcode_type_from_string(const char *str);


/**
	Create an fz_image from a barcode definition.

	type: The type of barcode to create.
	value: The value of the barcode.
	size: The size of the barcode.
	ec_level: error correction level 0-8.
	quiet: whether to include quiet zones (0 or 1).
	hrt: whether to include human readable text below the barcode (0 or 1).

	returns a created fz_image.
*/
fz_image *fz_new_barcode_image(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt);

/**
	Create an fz_pixmap from a barcode definition.

	type: The type of barcode to create.
	value: The value of the barcode.
	size: The size of the barcode.
	ec_level: error correction level 0-8.
	quiet: whether to include quiet zones (0 or 1).
	hrt: whether to include human readable text below the barcode (0 or 1).

	returns a created fz_pixmap.
*/
fz_pixmap *fz_new_barcode_pixmap(fz_context *ctx, fz_barcode_type type, const char *value, int size, int ec_level, int quiet, int hrt);


/**
	Decode a barcode from a page.

	type: NULL, or a pointer to receive the barcode type decoded.
	page: The page to decode.
	subarea: subarea of the page to decode.
	rotate: 0, 90, 180, or 270.

	returns the decoded value.
*/
char *fz_decode_barcode_from_page(fz_context *ctx, fz_barcode_type *type, fz_page *page, fz_rect subarea, int rotate);

/**
	Decode a barcode from a pixmap.

	type: NULL, or a pointer to receive the barcode type decoded.
	pix: The pixmap to decode.
	rotate: 0, 90, 180, or 270.

	returns the decoded value as an fz_malloced block. Should
	be fz_free'd by the caller.
*/
char *fz_decode_barcode_from_pixmap(fz_context *ctx, fz_barcode_type *type, fz_pixmap *pix, int rotate);

/**
	Decode a barcode from a display list.

	type: NULL, or a pointer to receive the barcode type decoded.
	list: The display list to render to get the barcode.
	subarea: subarea of the page to decode.
	rotate: 0, 90, 180, or 270.

	returns the decoded value.
*/
char *fz_decode_barcode_from_display_list(fz_context *ctx, fz_barcode_type *type, fz_display_list *list, fz_rect subarea, int rotate);

#endif /* MUPDF_FITZ_BARCODE_H */
