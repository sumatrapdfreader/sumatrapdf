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

#ifndef MUPDF_FITZ_SEPARATION_H
#define MUPDF_FITZ_SEPARATION_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/color.h"

/**
	A fz_separation structure holds details of a set of separations
	(such as might be used on within a page of the document).

	The app might control the separations by enabling/disabling them,
	and subsequent renders would take this into account.
*/

enum
{
	FZ_MAX_SEPARATIONS = 64
};

typedef struct fz_separations fz_separations;

typedef enum
{
	/* "Composite" separations are rendered using process
	 * colors using the equivalent colors */
	FZ_SEPARATION_COMPOSITE = 0,
	/* Spot colors are rendered into their own spot plane. */
	FZ_SEPARATION_SPOT = 1,
	/* Disabled colors are not rendered at all in the final
	 * output. */
	FZ_SEPARATION_DISABLED = 2
} fz_separation_behavior;

/**
	Create a new separations structure (initially empty)
*/
fz_separations *fz_new_separations(fz_context *ctx, int controllable);

/**
	Increment the reference count for a separations structure.
	Returns the same pointer.

	Never throws exceptions.
*/
fz_separations *fz_keep_separations(fz_context *ctx, fz_separations *sep);

/**
	Decrement the reference count for a separations structure.
	When the reference count hits zero, the separations structure
	is freed.

	Never throws exceptions.
*/
void fz_drop_separations(fz_context *ctx, fz_separations *sep);

/**
	Add a separation (null terminated name, colorspace)
*/
void fz_add_separation(fz_context *ctx, fz_separations *sep, const char *name, fz_colorspace *cs, int cs_channel);

/**
	Add a separation with equivalents (null terminated name,
	colorspace)

	(old, deprecated)
*/
void fz_add_separation_equivalents(fz_context *ctx, fz_separations *sep, uint32_t rgba, uint32_t cmyk, const char *name);

/**
	Control the rendering of a given separation.
*/
void fz_set_separation_behavior(fz_context *ctx, fz_separations *sep, int separation, fz_separation_behavior behavior);

/**
	Test for the current behavior of a separation.
*/
fz_separation_behavior fz_separation_current_behavior(fz_context *ctx, const fz_separations *sep, int separation);

const char *fz_separation_name(fz_context *ctx, const fz_separations *sep, int separation);
int fz_count_separations(fz_context *ctx, const fz_separations *sep);

/**
	Return the number of active separations.
*/
int fz_count_active_separations(fz_context *ctx, const fz_separations *seps);

/**
	Compare 2 separations structures (or NULLs).

	Return 0 if identical, non-zero if not identical.
*/
int fz_compare_separations(fz_context *ctx, const fz_separations *sep1, const fz_separations *sep2);


/**
	Return a separations object with all the spots in the input
	separations object that are set to composite, reset to be
	enabled. If there ARE no spots in the object, this returns
	NULL. If the object already has all its spots enabled, then
	just returns another handle on the same object.
*/
fz_separations *fz_clone_separations_for_overprint(fz_context *ctx, fz_separations *seps);

/**
	Convert a color given in terms of one colorspace,
	to a color in terms of another colorspace/separations.
*/
void fz_convert_separation_colors(fz_context *ctx, fz_colorspace *src_cs, const float *src_color, fz_separations *dst_seps, fz_colorspace *dst_cs, float *dst_color, fz_color_params color_params);

/**
	Get the equivalent separation color in a given colorspace.
*/
void fz_separation_equivalent(fz_context *ctx, const fz_separations *seps, int idx, fz_colorspace *dst_cs, float *dst_color, fz_colorspace *prf, fz_color_params color_params);

#endif
