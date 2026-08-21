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

#ifndef MUPDF_PDF_ZUGFERD_H
#define MUPDF_PDF_ZUGFERD_H

#include "mupdf/pdf/document.h"

enum pdf_zugferd_profile
{
	PDF_NOT_ZUGFERD = 0,
	/* ZUGFeRD 1.0 */
	PDF_ZUGFERD_COMFORT,
	PDF_ZUGFERD_BASIC,
	PDF_ZUGFERD_EXTENDED,
	/* ZUGFeRD 2.01 */
	PDF_ZUGFERD_BASIC_WL,
	PDF_ZUGFERD_MINIMUM,
	/* ZUGFeRD 2.2 */
	PDF_ZUGFERD_XRECHNUNG,
	PDF_ZUGFERD_UNKNOWN
};

enum pdf_zugferd_profile pdf_zugferd_profile(fz_context *ctx, pdf_document *doc, float *version);

fz_buffer *pdf_zugferd_xml(fz_context *ctx, pdf_document *doc);

const char *pdf_zugferd_profile_to_string(fz_context *ctx, enum pdf_zugferd_profile profile);

#endif
