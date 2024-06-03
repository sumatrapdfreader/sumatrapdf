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

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

int pdf_count_document_associated_files(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *af = pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), PDF_NAME(AF), NULL);

	return pdf_array_len(ctx, af);
}

pdf_obj *pdf_document_associated_file(fz_context *ctx, pdf_document *doc, int idx)
{
	pdf_obj *af = pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root), PDF_NAME(AF), NULL);

	return pdf_array_get(ctx, af, idx);
}

int pdf_count_page_associated_files(fz_context *ctx, pdf_page *page)
{
	pdf_obj *af = pdf_dict_get(ctx, page->obj, PDF_NAME(AF));

	return pdf_array_len(ctx, af);
}

pdf_obj *pdf_page_associated_file(fz_context *ctx, pdf_page *page, int idx)
{
	pdf_obj *af = pdf_dict_get(ctx, page->obj, PDF_NAME(AF));

	return pdf_array_get(ctx, af, idx);
}
