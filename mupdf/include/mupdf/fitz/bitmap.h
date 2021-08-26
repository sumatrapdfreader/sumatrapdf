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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#ifndef MUPDF_FITZ_BITMAP_H
#define MUPDF_FITZ_BITMAP_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/pixmap.h"

/**
	Bitmaps have 1 bit per component. Only used for creating
	halftoned versions of contone buffers, and saving out. Samples
	are stored msb first, akin to pbms.

	The internals of this struct are considered implementation
	details and subject to change. Where possible, accessor
	functions should be used in preference.
*/
typedef struct
{
	int refs;
	int w, h, stride, n;
	int xres, yres;
	unsigned char *samples;
} fz_bitmap;

/**
	Take an additional reference to the bitmap. The same pointer
	is returned.

	Never throws exceptions.
*/
fz_bitmap *fz_keep_bitmap(fz_context *ctx, fz_bitmap *bit);

/**
	Drop a reference to the bitmap. When the reference count reaches
	zero, the bitmap will be destroyed.

	Never throws exceptions.
*/
void fz_drop_bitmap(fz_context *ctx, fz_bitmap *bit);

/**
	A halftone is a set of threshold tiles, one per component. Each
	threshold tile is a pixmap, possibly of varying sizes and
	phases. Currently, we only provide one 'default' halftone tile
	for operating on 1 component plus alpha pixmaps (where the alpha
	is ignored). This is signified by a fz_halftone pointer to NULL.
*/
typedef struct fz_halftone fz_halftone;

/**
	Make a bitmap from a pixmap and a halftone.

	pix: The pixmap to generate from. Currently must be a single
	color component with no alpha.

	ht: The halftone to use. NULL implies the default halftone.

	Returns the resultant bitmap. Throws exceptions in the case of
	failure to allocate.
*/
fz_bitmap *fz_new_bitmap_from_pixmap(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht);

/**
	Make a bitmap from a pixmap and a
	halftone, allowing for the position of the pixmap within an
	overall banded rendering.

	pix: The pixmap to generate from. Currently must be a single
	color component with no alpha.

	ht: The halftone to use. NULL implies the default halftone.

	band_start: Vertical offset within the overall banded rendering
	(in pixels)

	Returns the resultant bitmap. Throws exceptions in the case of
	failure to allocate.
*/
fz_bitmap *fz_new_bitmap_from_pixmap_band(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht, int band_start);

/**
	Create a new bitmap.

	w, h: Width and Height for the bitmap

	n: Number of color components (assumed to be a divisor of 8)

	xres, yres: X and Y resolutions (in pixels per inch).

	Returns pointer to created bitmap structure. The bitmap
	data is uninitialised.
*/
fz_bitmap *fz_new_bitmap(fz_context *ctx, int w, int h, int n, int xres, int yres);

/**
	Retrieve details of a given bitmap.

	bitmap: The bitmap to query.

	w: Pointer to storage to retrieve width (or NULL).

	h: Pointer to storage to retrieve height (or NULL).

	n: Pointer to storage to retrieve number of color components (or
	NULL).

	stride: Pointer to storage to retrieve bitmap stride (or NULL).
*/
void fz_bitmap_details(fz_bitmap *bitmap, int *w, int *h, int *n, int *stride);

/**
	Set the entire bitmap to 0.

	Never throws exceptions.
*/
void fz_clear_bitmap(fz_context *ctx, fz_bitmap *bit);

/**
	Create a 'default' halftone structure
	for the given number of components.

	num_comps: The number of components to use.

	Returns a simple default halftone. The default halftone uses
	the same halftone tile for each plane, which may not be ideal
	for all purposes.
*/
fz_halftone *fz_default_halftone(fz_context *ctx, int num_comps);

/**
	Take an additional reference to the halftone. The same pointer
	is returned.

	Never throws exceptions.
*/
fz_halftone *fz_keep_halftone(fz_context *ctx, fz_halftone *half);

/**
	Drop a reference to the halftone. When the reference count
	reaches zero, the halftone is destroyed.

	Never throws exceptions.
*/
void fz_drop_halftone(fz_context *ctx, fz_halftone *ht);

#endif
