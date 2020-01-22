#ifndef MUPDF_FITZ_MATH_H
#define MUPDF_FITZ_MATH_H

#include "mupdf/fitz/system.h"

/*
	Multiply scaled two integers in the 0..255 range
*/
static inline int fz_mul255(int a, int b)
{
	/* see Jim Blinn's book "Dirty Pixels" for how this works */
	int x = a * b + 128;
	x += x >> 8;
	return x >> 8;
}

/*
	Undo alpha premultiplication.
*/
static inline int fz_div255(int c, int a)
{
	return a ? c * (255 * 256 / a) >> 8 : 0;
}

/*
	Expand a value A from the 0...255 range to the 0..256 range
*/
#define FZ_EXPAND(A) ((A)+((A)>>7))

/*
	Combine values A (in any range) and B (in the 0..256 range),
	to give a single value in the same range as A was.
*/
#define FZ_COMBINE(A,B) (((A)*(B))>>8)

/*
	Combine values A and C (in the same (any) range) and B and D (in
	the 0..256 range), to give a single value in the same range as A
	and C were.
*/
#define FZ_COMBINE2(A,B,C,D) (((A) * (B) + (C) * (D))>>8)

/*
	Blend SRC and DST (in the same range) together according to
	AMOUNT (in the 0...256 range).
*/
#define FZ_BLEND(SRC, DST, AMOUNT) ((((SRC)-(DST))*(AMOUNT) + ((DST)<<8))>>8)

float fz_atof(const char *s);

int fz_atoi(const char *s);
int64_t fz_atoi64(const char *s);

/*
	Some standard math functions, done as static inlines for speed.
	People with compilers that do not adequately implement inlines may
	like to reimplement these using macros.
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

static inline float fz_clamp(float f, float min, float max)
{
	return (f > min ? (f < max ? f : max) : min);
}

static inline int fz_clampi(int i, int min, int max)
{
	return (i > min ? (i < max ? i : max) : min);
}

static inline double fz_clampd(double d, double min, double max)
{
	return (d > min ? (d < max ? d : max) : min);
}

static inline void *fz_clampp(void *p, void *min, void *max)
{
	return (p > min ? (p < max ? p : max) : min);
}

#define DIV_BY_ZERO(a, b, min, max) (((a) < 0) ^ ((b) < 0) ? (min) : (max))

/*
	fz_point is a point in a two-dimensional space.
*/
typedef struct fz_point_s fz_point;
struct fz_point_s
{
	float x, y;
};

static inline fz_point fz_make_point(float x, float y)
{
	fz_point p = { x, y };
	return p;
}

/*
	fz_rect is a rectangle represented by two diagonally opposite
	corners at arbitrary coordinates.

	Rectangles are always axis-aligned with the X- and Y- axes.
	The relationship between the coordinates are that x0 <= x1 and
	y0 <= y1 in all cases except for infinite rectangles. The area
	of a rectangle is defined as (x1 - x0) * (y1 - y0). If either
	x0 > x1 or y0 > y1 is true for a given rectangle then it is
	defined to be infinite.

	To check for empty or infinite rectangles use fz_is_empty_rect
	and fz_is_infinite_rect.

	x0, y0: The top left corner.

	x1, y1: The bottom right corner.
*/
typedef struct fz_rect_s fz_rect;
struct fz_rect_s
{
	float x0, y0;
	float x1, y1;
};

static inline fz_rect fz_make_rect(float x0, float y0, float x1, float y1)
{
	fz_rect r = { x0, y0, x1, y1 };
	return r;
}

/*
	fz_irect is a rectangle using integers instead of floats.

	It's used in the draw device and for pixmap dimensions.
*/
typedef struct fz_irect_s fz_irect;
struct fz_irect_s
{
	int x0, y0;
	int x1, y1;
};

static inline fz_irect fz_make_irect(int x0, int y0, int x1, int y1)
{
	fz_irect r = { x0, y0, x1, y1 };
	return r;
}

/*
	A rectangle with sides of length one.

	The bottom left corner is at (0, 0) and the top right corner
	is at (1, 1).
*/
extern const fz_rect fz_unit_rect;

/*
	An empty rectangle with an area equal to zero.

	Both the top left and bottom right corner are at (0, 0).
*/
extern const fz_rect fz_empty_rect;
extern const fz_irect fz_empty_irect;

/*
	An infinite rectangle with negative area.

	The corner (x0, y0) is at (1, 1) while the corner (x1, y1) is
	at (-1, -1).
*/
extern const fz_rect fz_infinite_rect;
extern const fz_irect fz_infinite_irect;

/*
	Check if rectangle is empty.

	An empty rectangle is defined as one whose area is zero.
*/
static inline int fz_is_empty_rect(fz_rect r)
{
	return (r.x0 == r.x1 || r.y0 == r.y1);
}

static inline int fz_is_empty_irect(fz_irect r)
{
	return (r.x0 == r.x1 || r.y0 == r.y1);
}

/*
	Check if rectangle is infinite.

	An infinite rectangle is defined as one where either of the
	two relationships between corner coordinates are not true.
*/
static inline int fz_is_infinite_rect(fz_rect r)
{
	return (r.x0 > r.x1 || r.y0 > r.y1);
}

/*
	Check if an integer rectangle
	is infinite.

	An infinite rectangle is defined as one where either of the
	two relationships between corner coordinates are not true.
*/
static inline int fz_is_infinite_irect(fz_irect r)
{
	return (r.x0 > r.x1 || r.y0 > r.y1);
}

/*
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
typedef struct fz_matrix_s fz_matrix;
struct fz_matrix_s
{
	float a, b, c, d, e, f;
};

/*
	Identity transform matrix.
*/
extern const fz_matrix fz_identity;

static inline fz_matrix fz_make_matrix(float a, float b, float c, float d, float e, float f)
{
	fz_matrix m = { a, b, c, d, e, f };
	return m;
}

static inline int fz_is_identity(fz_matrix m)
{
	return m.a == 1 && m.b == 0 && m.c == 0 && m.d == 1 && m.e == 0 && m.f == 0;
}

fz_matrix fz_concat(fz_matrix left, fz_matrix right);

fz_matrix fz_scale(float sx, float sy);

fz_matrix fz_pre_scale(fz_matrix m, float sx, float sy);

fz_matrix fz_post_scale(fz_matrix m, float sx, float sy);

fz_matrix fz_shear(float sx, float sy);

fz_matrix fz_pre_shear(fz_matrix m, float sx, float sy);

fz_matrix fz_rotate(float degrees);

fz_matrix fz_pre_rotate(fz_matrix m, float degrees);

fz_matrix fz_translate(float tx, float ty);

fz_matrix fz_pre_translate(fz_matrix m, float tx, float ty);

fz_matrix fz_transform_page(fz_rect mediabox, float resolution, float rotate);

fz_matrix fz_invert_matrix(fz_matrix matrix);

int fz_try_invert_matrix(fz_matrix *inv, fz_matrix src);

int fz_is_rectilinear(fz_matrix m);

float fz_matrix_expansion(fz_matrix m);

fz_rect fz_intersect_rect(fz_rect a, fz_rect b);

fz_irect fz_intersect_irect(fz_irect a, fz_irect b);

fz_rect fz_union_rect(fz_rect a, fz_rect b);

fz_irect fz_irect_from_rect(fz_rect rect);

fz_irect fz_round_rect(fz_rect rect);

fz_rect fz_rect_from_irect(fz_irect bbox);

fz_rect fz_expand_rect(fz_rect b, float expand);
fz_irect fz_expand_irect(fz_irect a, int expand);

fz_rect fz_include_point_in_rect(fz_rect r, fz_point p);

fz_rect fz_translate_rect(fz_rect a, float xoff, float yoff);
fz_irect fz_translate_irect(fz_irect a, int xoff, int yoff);

int fz_contains_rect(fz_rect a, fz_rect b);

fz_point fz_transform_point(fz_point point, fz_matrix m);
fz_point fz_transform_point_xy(float x, float y, fz_matrix m);

fz_point fz_transform_vector(fz_point vector, fz_matrix m);

fz_rect fz_transform_rect(fz_rect rect, fz_matrix m);

fz_point fz_normalize_vector(fz_point p);

fz_matrix fz_gridfit_matrix(int as_tiled, fz_matrix m);

float fz_matrix_max_expansion(fz_matrix m);

typedef struct fz_quad_s fz_quad;
struct fz_quad_s
{
	fz_point ul, ur, ll, lr;
};

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

fz_rect fz_rect_from_quad(fz_quad q);
fz_quad fz_transform_quad(fz_quad q, fz_matrix m);

int fz_is_point_inside_quad(fz_point p, fz_quad q);
int fz_is_point_inside_rect(fz_point p, fz_rect r);
int fz_is_point_inside_irect(int x, int y, fz_irect r);

#endif
