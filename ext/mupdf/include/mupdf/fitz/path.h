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

#ifndef MUPDF_FITZ_PATH_H
#define MUPDF_FITZ_PATH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"

/**
 * Vector path buffer.
 * It can be stroked and dashed, or be filled.
 * It has a fill rule (nonzero or even_odd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 */

typedef struct fz_path fz_path;

typedef enum
{
	FZ_LINECAP_BUTT = 0,
	FZ_LINECAP_ROUND = 1,
	FZ_LINECAP_SQUARE = 2,
	FZ_LINECAP_TRIANGLE = 3
} fz_linecap;

typedef enum
{
	FZ_LINEJOIN_MITER = 0,
	FZ_LINEJOIN_ROUND = 1,
	FZ_LINEJOIN_BEVEL = 2,
	FZ_LINEJOIN_MITER_XPS = 3
} fz_linejoin;

typedef struct
{
	int refs;
	fz_linecap start_cap, dash_cap, end_cap;
	fz_linejoin linejoin;
	float linewidth;
	float miterlimit;
	float dash_phase;
	int dash_len;
	float dash_list[FZ_FLEXIBLE_ARRAY];
} fz_stroke_state;

typedef struct
{
	/* Compulsory ones */
	void (*moveto)(fz_context *ctx, void *arg, float x, float y);
	void (*lineto)(fz_context *ctx, void *arg, float x, float y);
	void (*curveto)(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3);
	void (*closepath)(fz_context *ctx, void *arg);
	/* Optional ones */
	void (*quadto)(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2);
	void (*curvetov)(fz_context *ctx, void *arg, float x2, float y2, float x3, float y3);
	void (*curvetoy)(fz_context *ctx, void *arg, float x1, float y1, float x3, float y3);
	void (*rectto)(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2);
} fz_path_walker;

/**
	Walk the segments of a path, calling the
	appropriate callback function from a given set for each
	segment of the path.

	path: The path to walk.

	walker: The set of callback functions to use. The first
	4 callback pointers in the set must be non-NULL. The
	subsequent ones can either be supplied, or can be left
	as NULL, in which case the top 4 functions will be
	called as appropriate to simulate them.

	arg: An opaque argument passed in to each callback.

	Exceptions will only be thrown if the underlying callback
	functions throw them.
*/
void fz_walk_path(fz_context *ctx, const fz_path *path, const fz_path_walker *walker, void *arg);

/**
	Create a new (empty) path structure.
*/
fz_path *fz_new_path(fz_context *ctx);

/**
	Increment the reference count. Returns the same pointer.

	All paths can be kept, regardless of their packing type.

	Never throws exceptions.
*/
fz_path *fz_keep_path(fz_context *ctx, const fz_path *path);

/**
	Decrement the reference count. When the reference count hits
	zero, free the path.

	All paths can be dropped, regardless of their packing type.
	Packed paths do not own the blocks into which they are packed
	so dropping them does not free those blocks.

	Never throws exceptions.
*/
void fz_drop_path(fz_context *ctx, const fz_path *path);

/**
	Minimise the internal storage used by a path.

	As paths are constructed, the internal buffers
	grow. To avoid repeated reallocations they
	grow with some spare space. Once a path has
	been fully constructed, this call allows the
	excess space to be trimmed.
*/
void fz_trim_path(fz_context *ctx, fz_path *path);

/**
	Return the number of bytes required to pack a path.
*/
int fz_packed_path_size(const fz_path *path);

/**
	Pack a path into the given block.
	To minimise the size of paths, this function allows them to be
	packed into a buffer with other information. Paths can be used
	interchangeably regardless of how they are packed.

	pack: Pointer to a block of data to pack the path into. Should
	be aligned by the caller to the same alignment as required for
	a fz_path pointer.

	path: The path to pack.

	Returns the number of bytes within the block used. Callers can
	access the packed path data by casting the value of pack on
	entry to be a fz_path *.

	Throws exceptions on failure to allocate.

	Implementation details: Paths can be 'unpacked', 'flat', or
	'open'. Standard paths, as created are 'unpacked'. Paths
	will be packed as 'flat', unless they are too large
	(where large indicates that they exceed some private
	implementation defined limits, currently including having
	more than 256 coordinates or commands).

	Large paths are 'open' packed as a header into the given block,
	plus pointers to other data blocks.

	Users should not have to care about whether paths are 'open'
	or 'flat' packed. Simply pack a path (if required), and then
	forget about the details.
*/
size_t fz_pack_path(fz_context *ctx, uint8_t *pack, const fz_path *path);

/**
	Clone the data for a path.

	This is used in preference to fz_keep_path when a whole
	new copy of a path is required, rather than just a shared
	pointer. This probably indicates that the path is about to
	be modified.

	path: path to clone.

	Throws exceptions on failure to allocate.
*/
fz_path *fz_clone_path(fz_context *ctx, fz_path *path);

/**
	Find out if a path is completely empty.
*/
int fz_path_is_empty(fz_context *ctx, const fz_path *path);

/**
	Return the current point that a path has
	reached or (0,0) if empty.

	path: path to return the current point of.
*/
fz_point fz_currentpoint(fz_context *ctx, fz_path *path);

/**
	Append a 'moveto' command to a path.
	This 'opens' a path.

	path: The path to modify.

	x, y: The coordinate to move to.

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_moveto(fz_context *ctx, fz_path *path, float x, float y);

/**
	Append a 'lineto' command to an open path.

	path: The path to modify.

	x, y: The coordinate to line to.

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_lineto(fz_context *ctx, fz_path *path, float x, float y);

/**
	Append a 'rectto' command to an open path.

	The rectangle is equivalent to:
		moveto x0 y0
		lineto x1 y0
		lineto x1 y1
		lineto x0 y1
		closepath

	path: The path to modify.

	x0, y0: First corner of the rectangle.

	x1, y1: Second corner of the rectangle.

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_rectto(fz_context *ctx, fz_path *path, float x0, float y0, float x1, float y1);

/**
	Append a 'quadto' command to an open path. (For a
	quadratic bezier).

	path: The path to modify.

	x0, y0: The control coordinates for the quadratic curve.

	x1, y1: The end coordinates for the quadratic curve.

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_quadto(fz_context *ctx, fz_path *path, float x0, float y0, float x1, float y1);

/**
	Append a 'curveto' command to an open path. (For a
	cubic bezier).

	path: The path to modify.

	x0, y0: The coordinates of the first control point for the
	curve.

	x1, y1: The coordinates of the second control point for the
	curve.

	x2, y2: The end coordinates for the curve.

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_curveto(fz_context *ctx, fz_path *path, float x0, float y0, float x1, float y1, float x2, float y2);

/**
	Append a 'curvetov' command to an open path. (For a
	cubic bezier with the first control coordinate equal to
	the start point).

	path: The path to modify.

	x1, y1: The coordinates of the second control point for the
	curve.

	x2, y2: The end coordinates for the curve.

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_curvetov(fz_context *ctx, fz_path *path, float x1, float y1, float x2, float y2);

/**
	Append a 'curvetoy' command to an open path. (For a
	cubic bezier with the second control coordinate equal to
	the end point).

	path: The path to modify.

	x0, y0: The coordinates of the first control point for the
	curve.

	x2, y2: The end coordinates for the curve (and the second
	control coordinate).

	Throws exceptions on failure to allocate, or attempting to
	modify a packed path.
*/
void fz_curvetoy(fz_context *ctx, fz_path *path, float x0, float y0, float x2, float y2);

/**
	Close the current subpath.

	path: The path to modify.

	Throws exceptions on failure to allocate, attempting to modify
	a packed path, and illegal path closes (i.e. closing a non open
	path).
*/
void fz_closepath(fz_context *ctx, fz_path *path);

/**
	Transform a path by a given
	matrix.

	path: The path to modify (must not be a packed path).

	transform: The transform to apply.

	Throws exceptions if the path is packed, or on failure
	to allocate.
*/
void fz_transform_path(fz_context *ctx, fz_path *path, fz_matrix transform);

/**
	Return a bounding rectangle for a path.

	path: The path to bound.

	stroke: If NULL, the bounding rectangle given is for
	the filled path. If non-NULL the bounding rectangle
	given is for the path stroked with the given attributes.

	ctm: The matrix to apply to the path during stroking.

	r: Pointer to a fz_rect which will be used to hold
	the result.

	Returns r, updated to contain the bounding rectangle.
*/
fz_rect fz_bound_path(fz_context *ctx, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm);

/**
	Given a rectangle (assumed to be the bounding box for a path),
	expand it to allow for the expansion of the bbox that would be
	seen by stroking the path with the given stroke state and
	transform.
*/
fz_rect fz_adjust_rect_for_stroke(fz_context *ctx, fz_rect rect, const fz_stroke_state *stroke, fz_matrix ctm);

/**
	A sane 'default' stroke state.
*/
FZ_DATA extern const fz_stroke_state fz_default_stroke_state;

/**
	Create a new (empty) stroke state structure (with no dash
	data) and return a reference to it.

	Throws exception on failure to allocate.
*/
fz_stroke_state *fz_new_stroke_state(fz_context *ctx);

/**
	Create a new (empty) stroke state structure, with room for
	dash data of the given length, and return a reference to it.

	len: The number of dash elements to allow room for.

	Throws exception on failure to allocate.
*/
fz_stroke_state *fz_new_stroke_state_with_dash_len(fz_context *ctx, int len);

/**
	Take an additional reference to a stroke state structure.

	No modifications should be carried out on a stroke
	state to which more than one reference is held, as
	this can cause race conditions.
*/
fz_stroke_state *fz_keep_stroke_state(fz_context *ctx, const fz_stroke_state *stroke);
int fz_stroke_state_eq(fz_context *ctx, const fz_stroke_state *a, const fz_stroke_state *b);

/**
	Drop a reference to a stroke state structure, destroying the
	structure if it is the last reference.
*/
void fz_drop_stroke_state(fz_context *ctx, const fz_stroke_state *stroke);

/**
	Given a reference to a (possibly) shared stroke_state structure,
	return a reference to an equivalent stroke_state structure
	that is guaranteed to be unshared (i.e. one that can
	safely be modified).

	shared: The reference to a (possibly) shared structure
	to unshare. Ownership of this reference is passed in
	to this function, even in the case of exceptions being
	thrown.

	Exceptions may be thrown in the event of failure to
	allocate if required.
*/
fz_stroke_state *fz_unshare_stroke_state(fz_context *ctx, fz_stroke_state *shared);

/**
	Given a reference to a (possibly) shared stroke_state structure,
	return a reference to a stroke_state structure (with room for a
	given amount of dash data) that is guaranteed to be unshared
	(i.e. one that can safely be modified).

	shared: The reference to a (possibly) shared structure
	to unshare. Ownership of this reference is passed in
	to this function, even in the case of exceptions being
	thrown.

	Exceptions may be thrown in the event of failure to
	allocate if required.
*/
fz_stroke_state *fz_unshare_stroke_state_with_dash_len(fz_context *ctx, fz_stroke_state *shared, int len);

/**
	Create an identical stroke_state structure and return a
	reference to it.

	stroke: The stroke state reference to clone.

	Exceptions may be thrown in the event of a failure to
	allocate.
*/
fz_stroke_state *fz_clone_stroke_state(fz_context *ctx, const fz_stroke_state *stroke);

fz_linecap fz_linecap_from_string(const char *s);
const char *fz_string_from_linecap(fz_linecap cap);
fz_linejoin fz_linejoin_from_string(const char *s);
const char *fz_string_from_linejoin(fz_linejoin join);

/**
	Check whether a given path, under the given transform
	is an axis-aligned rectangle.

	We accept zero width or height rectangles, so
	"move 100, 100; line 200, 100" would count as
	a rectangle too.
*/
int fz_path_is_rect(fz_context *ctx, const fz_path *path, fz_matrix ctm);

/**
	Check whether a given path, under the given transform
	is an axis-aligned rectangle.

	We accept zero width or height rectangles, so
	"move 100, 100; line 200, 100" would count as
	a rectangle too.

	bounds = NULL, or place to return the rectangle
	bounds if the path is a rectangle.
*/
int fz_path_is_rect_with_bounds(fz_context *ctx, const fz_path *path, fz_matrix ctm, fz_rect *bounds);

/**
	Check whether a given path has all its segments closed.

	This includes both explicit closepaths, and where segments are implicitly
	closed by segments that end where they started.
*/
int fz_path_is_closed(fz_context *ctx, const fz_path *path);


#endif
