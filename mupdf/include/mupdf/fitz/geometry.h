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

#ifndef MUPDF_FITZ_MATH_H
#define MUPDF_FITZ_MATH_H

#include "mupdf/fitz/system.h"

#include <math.h>
#include <assert.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
	Multiply scaled two integers in the 0..255 range
*/
static inline int fz_mul255(int a, int b)
{
	/* see Jim Blinn's book "Dirty Pixels" for how this works */
	int x = a * b + 128;
	x += x >> 8;
	return x >> 8;
}

/**
	Undo alpha premultiplication.
*/
static inline int fz_div255(int c, int a)
{
	return a ? c * (255 * 256 / a) >> 8 : 0;
}

/**
	Expand a value A from the 0...255 range to the 0..256 range
*/
#define FZ_EXPAND(A) ((A)+((A)>>7))

/**
	Combine values A (in any range) and B (in the 0..256 range),
	to give a single value in the same range as A was.
*/
#define FZ_COMBINE(A,B) (((A)*(B))>>8)

/**
	Combine values A and C (in the same (any) range) and B and D (in
	the 0..256 range), to give a single value in the same range as A
	and C were.
*/
#define FZ_COMBINE2(A,B,C,D) (((A) * (B) + (C) * (D))>>8)

/**
	Blend SRC and DST (in the same range) together according to
	AMOUNT (in the 0...256 range).
*/
#define FZ_BLEND(SRC, DST, AMOUNT) ((((SRC)-(DST))*(AMOUNT) + ((DST)<<8))>>8)

/**
	Range checking atof
*/
float fz_atof(const char *s);

/**
	atoi that copes with NULL
*/
int fz_atoi(const char *s);

/**
	64bit atoi that copes with NULL
*/
int64_t fz_atoi64(const char *s);

/**
	size_t atoi that copes with NULL.

	NOTE: limited to 63bits. Negative numbers
	are returned as 0.
*/
size_t fz_atoz(const char *s);

/**
	Some standard math functions, done as static inlines for speed.
	People with compilers that do not adequately implement inline
	may like to reimplement these using macros.
*/
static inline float fz_abs(float f)
{
	return (f < 0 ? -f : f);
}

static inline int fz_absi(int i)
{
	return (i < 0 ? -i : i);
}

static inline float fz_min(float a, float b)
{
	return (a < b ? a : b);
}

static inline int fz_mini(int a, int b)
{
	return (a < b ? a : b);
}

static inline size_t fz_minz(size_t a, size_t b)
{
	return (a < b ? a : b);
}

static inline int64_t fz_mini64(int64_t a, int64_t b)
{
	return (a < b ? a : b);
}

static inline float fz_max(float a, float b)
{
	return (a > b ? a : b);
}

static inline int fz_maxi(int a, int b)
{
	return (a > b ? a : b);
}

static inline size_t fz_maxz(size_t a, size_t b)
{
	return (a > b ? a : b);
}

static inline int64_t fz_maxi64(int64_t a, int64_t b)
{
	return (a > b ? a : b);
}

static inline float fz_clamp(float x, float min, float max)
{
	return x < min ? min : x > max ? max : x;
}

static inline int fz_clampi(int x, int min, int max)
{
	return x < min ? min : x > max ? max : x;
}

static inline int64_t fz_clamp64(int64_t x, int64_t min, int64_t max)
{
	return x < min ? min : x > max ? max : x;
}

static inline double fz_clampd(double x, double min, double max)
{
	return x < min ? min : x > max ? max : x;
}

static inline void *fz_clampp(void *x, void *min, void *max)
{
	return x < min ? min : x > max ? max : x;
}

#define DIV_BY_ZERO(a, b, min, max) (((a) < 0) ^ ((b) < 0) ? (min) : (max))

/**
	fz_point is a point in a two-dimensional space.
*/
typedef struct
{
	float x, y;
} fz_point;

static inline fz_point fz_make_point(float x, float y)
{
	fz_point p = { x, y };
	return p;
}

/**
	fz_rect is a rectangle represented by two diagonally opposite
	corners at arbitrary coordinates.

	Rectangles are always axis-aligned with the X- and Y- axes. We
	wish to distinguish rectangles in 3 categories; infinite, finite,
	and invalid. Zero area rectangles are a sub-category of finite
	ones.

	For all valid rectangles, x0 <= x1 and y0 <= y1 in all cases.
	Infinite rectangles have x0 = y0 = FZ_MIN_INF_RECT,
	x1 = y1 = FZ_MAX_INF_RECT. For any non infinite valid rectangle,
	the area is defined as (x1 - x0) * (y1 - y0).

	To check for empty or infinite rectangles use fz_is_empty_rect
	and fz_is_infinite_rect. To check for valid rectangles use
	fz_is_valid_rect.

	We choose this representation, so that we can easily distinguish
	the difference between intersecting 2 valid rectangles and
	getting an invalid one, as opposed to getting a zero area one
	(which nonetheless has valid bounds within the plane).

	x0, y0: The top left corner.

	x1, y1: The bottom right corner.

	We choose FZ_{MIN,MAX}_INF_RECT to be the largest 32bit signed
	integer values that survive roundtripping to floats.
*/
#define FZ_MIN_INF_RECT ((int)0x80000000)
#define FZ_MAX_INF_RECT ((int)0x7fffff80)

typedef struct
{
	float x0, y0;
	float x1, y1;
} fz_rect;

static inline fz_rect fz_make_rect(float x0, float y0, float x1, float y1)
{
	fz_rect r = { x0, y0, x1, y1 };
	return r;
}

/**
	fz_irect is a rectangle using integers instead of floats.

	It's used in the draw device and for pixmap dimensions.
*/
typedef struct
{
	int x0, y0;
	int x1, y1;
} fz_irect;

static inline fz_irect fz_make_irect(int x0, int y0, int x1, int y1)
{
	fz_irect r = { x0, y0, x1, y1 };
	return r;
}

/**
	A rectangle with sides of length one.

	The bottom left corner is at (0, 0) and the top right corner
	is at (1, 1).
*/
FZ_DATA extern const fz_rect fz_unit_rect;

/**
	An empty rectangle with an area equal to zero.
*/
FZ_DATA extern const fz_rect fz_empty_rect;
FZ_DATA extern const fz_irect fz_empty_irect;

/**
	An infinite rectangle.
*/
FZ_DATA extern const fz_rect fz_infinite_rect;
FZ_DATA extern const fz_irect fz_infinite_irect;

/**
	An invalid rectangle.
*/
FZ_DATA extern const fz_rect fz_invalid_rect;
FZ_DATA extern const fz_irect fz_invalid_irect;

/**
	Check if rectangle is empty.

	An empty rectangle is defined as one whose area is zero.
	All invalid rectangles are empty.
*/
static inline int fz_is_empty_rect(fz_rect r)
{
	return (r.x0 >= r.x1 || r.y0 >= r.y1);
}

static inline int fz_is_empty_irect(fz_irect r)
{
	return (r.x0 >= r.x1 || r.y0 >= r.y1);
}

/**
	Check if rectangle is infinite.
*/
static inline int fz_is_infinite_rect(fz_rect r)
{
	return (r.x0 == FZ_MIN_INF_RECT && r.x1 == FZ_MAX_INF_RECT &&
		r.y0 == FZ_MIN_INF_RECT && r.y1 == FZ_MAX_INF_RECT);
}

/**
	Check if an integer rectangle
	is infinite.
*/
static inline int fz_is_infinite_irect(fz_irect r)
{
	return (r.x0 == FZ_MIN_INF_RECT && r.x1 == FZ_MAX_INF_RECT &&
		r.y0 == FZ_MIN_INF_RECT && r.y1 == FZ_MAX_INF_RECT);
}

/**
	Check if rectangle is valid.
*/
static inline int fz_is_valid_rect(fz_rect r)
{
	return (r.x0 <= r.x1 && r.y0 <= r.y1);
}

/**
	Check if an integer rectangle is valid.
*/
static inline int fz_is_valid_irect(fz_irect r)
{
	return (r.x0 <= r.x1 && r.y0 <= r.y1);
}

/**
	Return the width of an irect. Invalid irects return 0.
*/
static inline unsigned int
fz_irect_width(fz_irect r)
{
	unsigned int w;
	if (r.x0 >= r.x1)
		return 0;
	/* Check for w overflowing. This should never happen, but
	 * if it does, it's pretty likely an indication of a severe
	 * problem. */
	w = (unsigned int)r.x1 - r.x0;
	assert((int)w >= 0);
	if ((int)w < 0)
		return 0;
	return (int)w;
}

/**
	Return the height of an irect. Invalid irects return 0.
*/
static inline int
fz_irect_height(fz_irect r)
{
	unsigned int h;
	if (r.y0 >= r.y1)
		return 0;
	/* Check for h overflowing. This should never happen, but
	 * if it does, it's pretty likely an indication of a severe
	 * problem. */
	h = (unsigned int)(r.y1 - r.y0);
	assert((int)h >= 0);
	if ((int)h < 0)
		return 0;
	return (int)h;
}

/**
	fz_matrix is a row-major 3x3 matrix used for representing
	transformations of coordinates throughout MuPDF.

	Since all points reside in a two-dimensional space, one vector
	is always a constant unit vector; hence only some elements may
	vary in a matrix. Below is how the elements map between
	different representations.

	/ a b 0 \
	| c d 0 | normally represented as [ a b c d e f ].
	\ e f 1 /
*/
typedef struct
{
	float a, b, c, d, e, f;
} fz_matrix;

/**
	Identity transform matrix.
*/
FZ_DATA extern const fz_matrix fz_identity;

static inline fz_matrix fz_make_matrix(float a, float b, float c, float d, float e, float f)
{
	fz_matrix m = { a, b, c, d, e, f };
	return m;
}

static inline int fz_is_identity(fz_matrix m)
{
	return m.a == 1 && m.b == 0 && m.c == 0 && m.d == 1 && m.e == 0 && m.f == 0;
}

/**
	Multiply two matrices.

	The order of the two matrices are important since matrix
	multiplication is not commutative.

	Returns result.
*/
fz_matrix fz_concat(fz_matrix left, fz_matrix right);

/**
	Create a scaling matrix.

	The returned matrix is of the form [ sx 0 0 sy 0 0 ].

	m: Pointer to the matrix to populate

	sx, sy: Scaling factors along the X- and Y-axes. A scaling
	factor of 1.0 will not cause any scaling along the relevant
	axis.

	Returns m.
*/
fz_matrix fz_scale(float sx, float sy);

/**
	Scale a matrix by premultiplication.

	m: Pointer to the matrix to scale

	sx, sy: Scaling factors along the X- and Y-axes. A scaling
	factor of 1.0 will not cause any scaling along the relevant
	axis.

	Returns m (updated).
*/
fz_matrix fz_pre_scale(fz_matrix m, float sx, float sy);

/**
	Scale a matrix by postmultiplication.

	m: Pointer to the matrix to scale

	sx, sy: Scaling factors along the X- and Y-axes. A scaling
	factor of 1.0 will not cause any scaling along the relevant
	axis.

	Returns m (updated).
*/
fz_matrix fz_post_scale(fz_matrix m, float sx, float sy);

/**
	Create a shearing matrix.

	The returned matrix is of the form [ 1 sy sx 1 0 0 ].

	m: pointer to place to store returned matrix

	sx, sy: Shearing factors. A shearing factor of 0.0 will not
	cause any shearing along the relevant axis.

	Returns m.
*/
fz_matrix fz_shear(float sx, float sy);

/**
	Premultiply a matrix with a shearing matrix.

	The shearing matrix is of the form [ 1 sy sx 1 0 0 ].

	m: pointer to matrix to premultiply

	sx, sy: Shearing factors. A shearing factor of 0.0 will not
	cause any shearing along the relevant axis.

	Returns m (updated).
*/
fz_matrix fz_pre_shear(fz_matrix m, float sx, float sy);

/**
	Create a rotation matrix.

	The returned matrix is of the form
	[ cos(deg) sin(deg) -sin(deg) cos(deg) 0 0 ].

	m: Pointer to place to store matrix

	degrees: Degrees of counter clockwise rotation. Values less
	than zero and greater than 360 are handled as expected.

	Returns m.
*/
fz_matrix fz_rotate(float degrees);

/**
	Rotate a transformation by premultiplying.

	The premultiplied matrix is of the form
	[ cos(deg) sin(deg) -sin(deg) cos(deg) 0 0 ].

	m: Pointer to matrix to premultiply.

	degrees: Degrees of counter clockwise rotation. Values less
	than zero and greater than 360 are handled as expected.

	Returns m (updated).
*/
fz_matrix fz_pre_rotate(fz_matrix m, float degrees);

/**
	Create a translation matrix.

	The returned matrix is of the form [ 1 0 0 1 tx ty ].

	m: A place to store the created matrix.

	tx, ty: Translation distances along the X- and Y-axes. A
	translation of 0 will not cause any translation along the
	relevant axis.

	Returns m.
*/
fz_matrix fz_translate(float tx, float ty);

/**
	Translate a matrix by premultiplication.

	m: The matrix to translate

	tx, ty: Translation distances along the X- and Y-axes. A
	translation of 0 will not cause any translation along the
	relevant axis.

	Returns m.
*/
fz_matrix fz_pre_translate(fz_matrix m, float tx, float ty);

/**
	Create transform matrix to draw page
	at a given resolution and rotation. Adjusts the scaling
	factors so that the page covers whole number of
	pixels and adjust the page origin to be at 0,0.
*/
fz_matrix fz_transform_page(fz_rect mediabox, float resolution, float rotate);

/**
	Create an inverse matrix.

	matrix: Matrix to invert. A degenerate matrix, where the
	determinant is equal to zero, can not be inverted and the
	original matrix is returned instead.

	Returns inverse.
*/
fz_matrix fz_invert_matrix(fz_matrix matrix);

/**
	Attempt to create an inverse matrix.

	inv: Place to store inverse matrix.

	src: Matrix to invert. A degenerate matrix, where the
	determinant is equal to zero, can not be inverted.

	Returns 1 if matrix is degenerate (singular), or 0 otherwise.
*/
int fz_try_invert_matrix(fz_matrix *inv, fz_matrix src);

/**
	Check if a transformation is rectilinear.

	Rectilinear means that no shearing is present and that any
	rotations present are a multiple of 90 degrees. Usually this
	is used to make sure that axis-aligned rectangles before the
	transformation are still axis-aligned rectangles afterwards.
*/
int fz_is_rectilinear(fz_matrix m);

/**
	Calculate average scaling factor of matrix.
*/
float fz_matrix_expansion(fz_matrix m);

/**
	Compute intersection of two rectangles.

	Given two rectangles, update the first to be the smallest
	axis-aligned rectangle that covers the area covered by both
	given rectangles. If either rectangle is empty then the
	intersection is also empty. If either rectangle is infinite
	then the intersection is simply the non-infinite rectangle.
	Should both rectangles be infinite, then the intersection is
	also infinite.
*/
fz_rect fz_intersect_rect(fz_rect a, fz_rect b);

/**
	Compute intersection of two bounding boxes.

	Similar to fz_intersect_rect but operates on two bounding
	boxes instead of two rectangles.
*/
fz_irect fz_intersect_irect(fz_irect a, fz_irect b);

/**
	Compute union of two rectangles.

	Given two rectangles, update the first to be the smallest
	axis-aligned rectangle that encompasses both given rectangles.
	If either rectangle is infinite then the union is also infinite.
	If either rectangle is empty then the union is simply the
	non-empty rectangle. Should both rectangles be empty, then the
	union is also empty.
*/
fz_rect fz_union_rect(fz_rect a, fz_rect b);

/**
	Convert a rect into the minimal bounding box
	that covers the rectangle.

	Coordinates in a bounding box are integers, so rounding of the
	rects coordinates takes place. The top left corner is rounded
	upwards and left while the bottom right corner is rounded
	downwards and to the right.
*/
fz_irect fz_irect_from_rect(fz_rect rect);

/**
	Round rectangle coordinates.

	Coordinates in a bounding box are integers, so rounding of the
	rects coordinates takes place. The top left corner is rounded
	upwards and left while the bottom right corner is rounded
	downwards and to the right.

	This differs from fz_irect_from_rect, in that fz_irect_from_rect
	slavishly follows the numbers (i.e any slight over/under
	calculations can cause whole extra pixels to be added).
	fz_round_rect allows for a small amount of rounding error when
	calculating the bbox.
*/
fz_irect fz_round_rect(fz_rect rect);

/**
	Convert a bbox into a rect.

	For our purposes, a rect can represent all the values we meet in
	a bbox, so nothing can go wrong.

	rect: A place to store the generated rectangle.

	bbox: The bbox to convert.

	Returns rect (updated).
*/
fz_rect fz_rect_from_irect(fz_irect bbox);

/**
	Expand a bbox by a given amount in all directions.
*/
fz_rect fz_expand_rect(fz_rect b, float expand);
fz_irect fz_expand_irect(fz_irect a, int expand);

/**
	Expand a bbox to include a given point.
	To create a rectangle that encompasses a sequence of points, the
	rectangle must first be set to be the empty rectangle at one of
	the points before including the others.
*/
fz_rect fz_include_point_in_rect(fz_rect r, fz_point p);

/**
	Translate bounding box.

	Translate a bbox by a given x and y offset. Allows for overflow.
*/
fz_rect fz_translate_rect(fz_rect a, float xoff, float yoff);
fz_irect fz_translate_irect(fz_irect a, int xoff, int yoff);

/**
	Test rectangle inclusion.

	Return true if a entirely contains b.
*/
int fz_contains_rect(fz_rect a, fz_rect b);

/**
	Test rectangle overlap.

	Returns true if the area of the overlap is
	non zero.
*/
int fz_overlaps_rect(fz_rect a, fz_rect b);

/**
	Apply a transformation to a point.

	transform: Transformation matrix to apply. See fz_concat,
	fz_scale, fz_rotate and fz_translate for how to create a
	matrix.

	point: Pointer to point to update.

	Returns transform (unchanged).
*/
fz_point fz_transform_point(fz_point point, fz_matrix m);
fz_point fz_transform_point_xy(float x, float y, fz_matrix m);

/**
	Apply a transformation to a vector.

	transform: Transformation matrix to apply. See fz_concat,
	fz_scale and fz_rotate for how to create a matrix. Any
	translation will be ignored.

	vector: Pointer to vector to update.
*/
fz_point fz_transform_vector(fz_point vector, fz_matrix m);

/**
	Apply a transform to a rectangle.

	After the four corner points of the axis-aligned rectangle
	have been transformed it may not longer be axis-aligned. So a
	new axis-aligned rectangle is created covering at least the
	area of the transformed rectangle.

	transform: Transformation matrix to apply. See fz_concat,
	fz_scale and fz_rotate for how to create a matrix.

	rect: Rectangle to be transformed. The two special cases
	fz_empty_rect and fz_infinite_rect, may be used but are
	returned unchanged as expected.
*/
fz_rect fz_transform_rect(fz_rect rect, fz_matrix m);

/**
	Normalize a vector to length one.
*/
fz_point fz_normalize_vector(fz_point p);

/**
	Grid fit a matrix.

	as_tiled = 0 => adjust the matrix so that the image of the unit
	square completely covers any pixel that was touched by the
	image of the unit square under the original matrix.

	as_tiled = 1 => adjust the matrix so that the corners of the
	image of the unit square align with the closest integer corner
	of the image of the unit square under the original matrix.
*/
fz_matrix fz_gridfit_matrix(int as_tiled, fz_matrix m);

/**
	Find the largest expansion performed by this matrix.
	(i.e. max(abs(m.a),abs(m.b),abs(m.c),abs(m.d))
*/
float fz_matrix_max_expansion(fz_matrix m);

/**
	A representation for a region defined by 4 points.

	The significant difference between quads and rects is that
	the edges of quads are not axis aligned.
*/
typedef struct
{
	fz_point ul, ur, ll, lr;
} fz_quad;

/**
	Inline convenience construction function.
*/
static inline fz_quad fz_make_quad(
	float ul_x, float ul_y,
	float ur_x, float ur_y,
	float ll_x, float ll_y,
	float lr_x, float lr_y)
{
	fz_quad q = {
		{ ul_x, ul_y },
		{ ur_x, ur_y },
		{ ll_x, ll_y },
		{ lr_x, lr_y },
	};
	return q;
}

FZ_DATA extern const fz_quad fz_invalid_quad;
FZ_DATA extern const fz_quad fz_infinite_quad;

/**
	Is a quad valid?
*/
int fz_is_valid_quad(fz_quad q);

/**
	Is a quad empty?
*/
int fz_is_empty_quad(fz_quad q);

/**
	Is a quad infinite?
*/
int fz_is_infinite_quad(fz_quad q);

/**
	Convert a rect to a quad (losslessly).
*/
fz_quad fz_quad_from_rect(fz_rect r);

/**
	Convert a quad to the smallest rect that covers it.
*/
fz_rect fz_rect_from_quad(fz_quad q);

/**
	Transform a quad by a matrix.
*/
fz_quad fz_transform_quad(fz_quad q, fz_matrix m);

/**
	Inclusion test for quads.
*/
int fz_is_point_inside_quad(fz_point p, fz_quad q);

/**
	Inclusion test for rects. (Rect is assumed to be open, i.e.
	top right corner is not included).
*/
int fz_is_point_inside_rect(fz_point p, fz_rect r);

/**
	Inclusion test for irects. (Rect is assumed to be open, i.e.
	top right corner is not included).
*/
int fz_is_point_inside_irect(int x, int y, fz_irect r);

/**
	Inclusion test for rects.

	rects are assumed to be both open or both closed.

	No invalid rect can include any other rect.
	No invalid rect can be included by any rect.
	Empty (point) rects can include themselves.
	Empty (line) rects can include many (subline) rects.
*/
int fz_is_rect_inside_rect(fz_rect inner, fz_rect outer);

/**
	Inclusion test for irects.

	rects are assumed to be both open or both closed.

	No invalid rect can include any other rect.
	No invalid rect can be included by any rect.
	Empty (point) rects can include themselves.
	Empty (line) rects can include many (subline) rects.
*/
int fz_is_irect_inside_irect(fz_irect inner, fz_irect outer);

/**
	Inclusion test for quad in quad.

	This may break down if quads are not 'well formed'.
*/
int fz_is_quad_inside_quad(fz_quad needle, fz_quad haystack);

/**
	Intersection test for quads.

	This may break down if quads are not 'well formed'.
*/
int fz_is_quad_intersecting_quad(fz_quad a, fz_quad b);

#endif
