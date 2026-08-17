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

#ifndef MUPDF_FITZ_SHADE_H
#define MUPDF_FITZ_SHADE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/compressed-buffer.h"

/**
 * The shading code uses gouraud shaded triangle meshes.
 */

enum
{
	FZ_FUNCTION_BASED = 1,
	FZ_LINEAR = 2,
	FZ_RADIAL = 3,
	FZ_MESH_TYPE4 = 4,
	FZ_MESH_TYPE5 = 5,
	FZ_MESH_TYPE6 = 6,
	FZ_MESH_TYPE7 = 7
};

/**
	Structure is public to allow derived classes. Do not
	access the members directly.
*/
typedef struct
{
	fz_storable storable;

	fz_rect bbox;		/* can be fz_infinite_rect */
	fz_colorspace *colorspace;

	fz_matrix matrix;	/* matrix from pattern dict */
	int use_background;	/* background color for fills but not 'sh' */
	float background[FZ_MAX_COLORS];

	/* Just to be confusing, PDF Shadings of Type 1 (Function Based
	 * Shadings), do NOT use function, but all the others do. This
	 * is because Type 1 shadings take 2 inputs, whereas all the
	 * others (when used with a function take 1 input. The type 1
	 * data is in the 'f' field of the union below. */
	/* If function_stride = 0, then function is not used. Otherwise
	 * function points to 256*function_stride entries. */
	int function_stride;
	float *function;

	int type; /* function, linear, radial, mesh */
	union
	{
		struct
		{
			int extend[2];
			float coords[2][3]; /* (x,y,r) twice */
		} l_or_r;
		struct
		{
			int vprow;
			int bpflag;
			int bpcoord;
			int bpcomp;
			float x0, x1;
			float y0, y1;
			float c0[FZ_MAX_COLORS];
			float c1[FZ_MAX_COLORS];
		} m;
		struct
		{
			fz_matrix matrix;
			int xdivs;
			int ydivs;
			float domain[2][2];
			float *fn_vals;
		} f;
	} u;

	fz_compressed_buffer *buffer;
} fz_shade;

/**
	Increment the reference count for the shade structure. The
	same pointer is returned.

	Never throws exceptions.
*/
fz_shade *fz_keep_shade(fz_context *ctx, fz_shade *shade);

/**
	Decrement the reference count for the shade structure. When
	the reference count hits zero, the structure is freed.

	Never throws exceptions.
*/
void fz_drop_shade(fz_context *ctx, fz_shade *shade);

/**
	Bound a given shading.

	shade: The shade to bound.

	ctm: The transform to apply to the shade before bounding.

	r: Pointer to storage to put the bounds in.

	Returns r, updated to contain the bounds for the shading.
*/
fz_rect fz_bound_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm);

typedef struct fz_shade_color_cache fz_shade_color_cache;

void fz_drop_shade_color_cache(fz_context *ctx, fz_shade_color_cache *cache);

/**
	Render a shade to a given pixmap.

	shade: The shade to paint.

	override_cs: NULL, or colorspace to override the shades
	inbuilt colorspace.

	ctm: The transform to apply.

	dest: The pixmap to render into.

	color_params: The color rendering settings

	bbox: Pointer to a bounding box to limit the rendering
	of the shade.

	eop: NULL, or pointer to overprint bitmap.

	cache: *cache is used to cache color information. If *cache is NULL it
	is set to point to a new fz_shade_color_cache. If cache is NULL it is
	ignored.
*/
void fz_paint_shade(fz_context *ctx, fz_shade *shade, fz_colorspace *override_cs, fz_matrix ctm, fz_pixmap *dest, fz_color_params color_params, fz_irect bbox, const fz_overprint *eop, fz_shade_color_cache **cache);

/**
 *	Handy routine for processing mesh based shades
 */
typedef struct
{
	fz_point p;
	float c[FZ_MAX_COLORS];
} fz_vertex;

/**
	Callback function type for use with
	fz_process_shade.

	arg: Opaque pointer from fz_process_shade caller.

	v: Pointer to a fz_vertex structure to populate.

	c: Pointer to an array of floats used to populate v.
*/
typedef void (fz_shade_prepare_fn)(fz_context *ctx, void *arg, fz_vertex *v, const float *c);

/**
	Callback function type for use with
	fz_process_shade.

	arg: Opaque pointer from fz_process_shade caller.

	av, bv, cv: Pointers to a fz_vertex structure describing
	the corner locations and colors of a triangle to be
	filled.
*/
typedef void (fz_shade_process_fn)(fz_context *ctx, void *arg, fz_vertex *av, fz_vertex *bv, fz_vertex *cv);

/**
	Process a shade, using supplied callback functions. This
	decomposes the shading to a mesh (even ones that are not
	natively meshes, such as linear or radial shadings), and
	processes triangles from those meshes.

	shade: The shade to process.

	ctm: The transform to use

	prepare: Callback function to 'prepare' each vertex.
	This function is passed an array of floats, and populates
	a fz_vertex structure.

	process: This function is passed 3 pointers to vertex
	structures, and actually performs the processing (typically
	filling the area between the vertices).

	process_arg: An opaque argument passed through from caller
	to callback functions.
*/
void fz_process_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_rect scissor,
			fz_shade_prepare_fn *prepare,
			fz_shade_process_fn *process,
			void *process_arg);


/* Implementation details: subject to change. */

/**
	Internal function to destroy a
	shade. Only exposed for use with the fz_store.

	shade: The reference to destroy.
*/
void fz_drop_shade_imp(fz_context *ctx, fz_storable *shade);

#endif
