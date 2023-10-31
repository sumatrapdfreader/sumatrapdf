// Copyright (C) 2004-2022 Artifex Software, Inc.
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

extern fz_document_handler pdf_document_handler;
extern fz_document_handler xps_document_handler;
extern fz_document_handler svg_document_handler;
extern fz_document_handler cbz_document_handler;
extern fz_document_handler img_document_handler;
extern fz_document_handler fb2_document_handler;
extern fz_document_handler html_document_handler;
extern fz_document_handler xhtml_document_handler;
extern fz_document_handler mobi_document_handler;
extern fz_document_handler epub_document_handler;
extern fz_document_handler txt_document_handler;
extern fz_document_handler office_document_handler;
extern fz_document_handler gz_document_handler;

void fz_register_document_handlers(fz_context *ctx)
{
#if FZ_ENABLE_PDF
	fz_register_document_handler(ctx, &pdf_document_handler);
#endif /* FZ_ENABLE_PDF */
#if FZ_ENABLE_XPS
	fz_register_document_handler(ctx, &xps_document_handler);
#endif /* FZ_ENABLE_XPS */
#if FZ_ENABLE_SVG
	fz_register_document_handler(ctx, &svg_document_handler);
#endif /* FZ_ENABLE_SVG */
#if FZ_ENABLE_CBZ
	fz_register_document_handler(ctx, &cbz_document_handler);
#endif /* FZ_ENABLE_CBZ */
#if FZ_ENABLE_IMG
	fz_register_document_handler(ctx, &img_document_handler);
#endif /* FZ_ENABLE_IMG */
#if FZ_ENABLE_HTML
	fz_register_document_handler(ctx, &fb2_document_handler);
	fz_register_document_handler(ctx, &html_document_handler);
	fz_register_document_handler(ctx, &xhtml_document_handler);
	fz_register_document_handler(ctx, &mobi_document_handler);
	fz_register_document_handler(ctx, &txt_document_handler);
	fz_register_document_handler(ctx, &office_document_handler);
#endif /* FZ_ENABLE_HTML */
#if FZ_ENABLE_EPUB
	fz_register_document_handler(ctx, &epub_document_handler);
#endif /* FZ_ENABLE_EPUB */
	fz_register_document_handler(ctx, &gz_document_handler);
}
