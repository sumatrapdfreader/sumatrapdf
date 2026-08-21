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

#ifndef MUPDF_FITZ_GLYPH_H
#define MUPDF_FITZ_GLYPH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/path.h"

/**
	Glyphs represent a run length encoded set of pixels for a 2
	dimensional region of a plane.
*/
typedef struct fz_glyph fz_glyph;

/**
	Return the bounding box of the glyph in pixels.
*/
fz_irect fz_glyph_bbox(fz_context *ctx, fz_glyph *glyph);
fz_irect fz_glyph_bbox_no_ctx(fz_glyph *src);

/**
	Return the width of the glyph in pixels.
*/
int fz_glyph_width(fz_context *ctx, fz_glyph *glyph);

/**
	Return the height of the glyph in pixels.
*/
int fz_glyph_height(fz_context *ctx, fz_glyph *glyph);

/**
	Take a reference to a glyph.

	pix: The glyph to increment the reference for.

	Returns pix.
*/
fz_glyph *fz_keep_glyph(fz_context *ctx, fz_glyph *pix);

/**
	Drop a reference and free a glyph.

	Decrement the reference count for the glyph. When no
	references remain the glyph will be freed.
*/
void fz_drop_glyph(fz_context *ctx, fz_glyph *pix);

/**
	Look a glyph up from a font, and return the outline of the
	glyph using the given transform.

	The caller owns the returned path, and so is responsible for
	ensuring that it eventually gets dropped.
*/
fz_path *fz_outline_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix ctm);

#endif
