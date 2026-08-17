// Copyright (C) 2024 Artifex Software, Inc.
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

#ifndef MUPDF_PDF_RECOLOR_H
#define MUPDF_PDF_RECOLOR_H

#include "mupdf/pdf/document.h"

typedef struct
{
	/* For gray, use num_comp = 1.
	 * For rgb, use num_comp = 3.
	 * For cmyk use num_comp = 4.
	 * All other values reserved. */
	int num_comp;
} pdf_recolor_options;

/*
	Recolor a given document page.

	All other values reserved.
*/
void pdf_recolor_page(fz_context *ctx, pdf_document *doc, int pagenum, const pdf_recolor_options *opts);

/*
	Remove output intents from a document.
*/
void pdf_remove_output_intents(fz_context *ctx, pdf_document *doc);

#endif
