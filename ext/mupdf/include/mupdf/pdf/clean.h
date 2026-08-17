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

#ifndef MUPDF_PDF_CLEAN_H
#define MUPDF_PDF_CLEAN_H

#include "mupdf/pdf/document.h"
#include "mupdf/pdf/image-rewriter.h"

/**
	Specifies whether the PDF's structure tree should be dropped or
	kept when rearranging or subsetting its pages.

	PDF_CLEAN_STRUCTURE_DROP: Remove the structure tree entirely.

	PDF_CLEAN_STRUCTURE_KEEP: Preserve the structure tree. When
	redacting a document, preserving the structure tree might leak
	information.
 */
typedef enum {
	PDF_CLEAN_STRUCTURE_DROP = 0,
	PDF_CLEAN_STRUCTURE_KEEP = 1
} pdf_clean_options_structure;

typedef enum {
	PDF_CLEAN_VECTORIZE_NO = 0,
	PDF_CLEAN_VECTORIZE_YES = 1
} pdf_clean_options_vectorize;

typedef struct
{
	pdf_write_options write;
	pdf_image_rewriter_options image;

	int subset_fonts;

	/* PDF_CLEAN_STRUCTURE_DROP to drop the structure tree (default).
	 * PDF_CLEAN_STRUCTURE_KEEP to keep it unchanged.
	 * Future values reserved.
	 */
	pdf_clean_options_structure structure;

	/* PDF_CLEAN_VECTORIZE_NO to leave the pages unchanged.
	 * PDF_CLEAN_VECTORIZE_YES to vectorize each page (and flatten any Type 3 fonts).
	 * Future values reserved.
	 */
	pdf_clean_options_vectorize vectorize;
} pdf_clean_options;

/*
	Read infile, and write selected pages to outfile with the given options.
*/
void pdf_clean_file(fz_context *ctx, char *infile, char *outfile, char *password, pdf_clean_options *opts, int retainlen, char *retainlist[]);

/*
	Recreate page tree to include only the pages listed in the array, in the order listed.
*/
void pdf_rearrange_pages(fz_context *ctx, pdf_document *doc, int count, const int *pages, pdf_clean_options_structure structure);

/*
	Recreate given page list (or all pages if count == 0), with text being vectorized.
*/
void pdf_vectorize_pages(fz_context *ctx, pdf_document *doc, int count, const int *new_page_list, pdf_clean_options_vectorize vectorize);

#endif
