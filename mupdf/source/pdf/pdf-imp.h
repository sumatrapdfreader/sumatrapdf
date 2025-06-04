// Copyright (C) 2025 Artifex Software, Inc.
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

// Internal PDF functions

#ifndef MUPDF_PDF_PDF_IMP_H

#define MUPDF_PDF_PDF_IMP_H

#include "mupdf/pdf.h"

void pdf_repair_xref_aux(fz_context *ctx, pdf_document *doc, void (*mid)(fz_context *ctx, pdf_document *doc));

pdf_obj *pdf_lookup_mcid_in_mcids(fz_context *ctx, int id, pdf_obj *mcids);

#endif /* MUPDF_PDF_PDF_IMP_H */
