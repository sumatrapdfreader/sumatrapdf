// Copyright (C) 2004-2021 Artifex Software, Inc.
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

#ifndef MUPDF_PDF_IMAGE_REWRITER_H
#define MUPDF_PDF_IMAGE_REWRITER_H

#include "mupdf/pdf/document.h"

enum
{
	FZ_SUBSAMPLE_AVERAGE,
	FZ_SUBSAMPLE_BICUBIC
};

enum
{
	FZ_RECOMPRESS_NEVER,
	FZ_RECOMPRESS_SAME,
	FZ_RECOMPRESS_LOSSLESS,
	FZ_RECOMPRESS_JPEG,
	FZ_RECOMPRESS_J2K,
	FZ_RECOMPRESS_FAX
};

typedef struct
{
	int color_lossless_image_subsample_method;
	int color_lossy_image_subsample_method;
	int color_lossless_image_subsample_threshold; /* 0, or the threshold dpi at which to subsample color images. */
	int color_lossless_image_subsample_to; /* 0, or the dpi to subsample to */
	int color_lossy_image_subsample_threshold; /* 0, or the threshold dpi at which to subsample color images. */
	int color_lossy_image_subsample_to; /* 0, or the dpi to subsample to */
	int color_lossless_image_recompress_method; /* Which compression method to use for losslessly compressed color images? */
	int color_lossy_image_recompress_method; /* Which compression method to use for lossy compressed color images? */
	char *color_lossy_image_recompress_quality;
	char *color_lossless_image_recompress_quality;
	int gray_lossless_image_subsample_method;
	int gray_lossy_image_subsample_method;
	int gray_lossless_image_subsample_threshold; /* 0, or the threshold at which to subsample gray images. */
	int gray_lossless_image_subsample_to; /* 0, or the dpi to subsample to */
	int gray_lossy_image_subsample_threshold; /* 0, or the threshold at which to subsample gray images. */
	int gray_lossy_image_subsample_to; /* 0, or the dpi to subsample to */
	int gray_lossless_image_recompress_method; /* Which compression method to use for losslessly compressed gray images? */
	int gray_lossy_image_recompress_method; /* Which compression method to use for lossy compressed gray images? */
	char *gray_lossy_image_recompress_quality;
	char *gray_lossless_image_recompress_quality;
	int bitonal_image_subsample_method;
	int bitonal_image_subsample_threshold; /* 0, or the threshold at which to subsample bitonal images. */
	int bitonal_image_subsample_to; /* 0, or the dpi to subsample to */
	int bitonal_image_recompress_method; /* Which compression method to use for bitonal images? */
	char *bitonal_image_recompress_quality;
} pdf_image_rewriter_options;

/*
	Rewrite images within the given document.
*/
void pdf_rewrite_images(fz_context *ctx, pdf_document *doc, pdf_image_rewriter_options *opts);

#endif
