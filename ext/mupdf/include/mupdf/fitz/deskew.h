// Copyright (C) 2004-2024 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_DESKEW_H
#define MUPDF_FITZ_DESKEW_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

enum
{
	FZ_DESKEW_BORDER_INCREASE = 0,
	FZ_DESKEW_BORDER_MAINTAIN = 1,
	FZ_DESKEW_BORDER_DECREASE = 2
};

fz_pixmap *fz_deskew_pixmap(fz_context *ctx,
			fz_pixmap *src,
			double degrees,
			int border);

/* Skew detection */

double fz_detect_skew(fz_context *ctx, fz_pixmap *pixmap);


#endif /* MUPDF_FITZ_DESKEW_H */
