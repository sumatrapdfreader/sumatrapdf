#include "mupdf/fitz.h"

#include <math.h>
#include <float.h>
#include <limits.h>

#define MAX4(a,b,c,d) fz_max(fz_max(a,b), fz_max(c,d))
#define MIN4(a,b,c,d) fz_min(fz_min(a,b), fz_min(c,d))

/*	A useful macro to add with overflow detection and clamping.

	We want to do "b = a + x", but to allow for overflow. Consider the
	top bits, and the cases in which overflow occurs:

	overflow    a   x   b ~a^x  a^b   (~a^x)&(a^b)
	   no       0   0   0   1    0          0
	   yes      0   0   1   1    1          1
	   no       0   1   0   0    0          0
	   no       0   1   1   0    1          0
	   no       1   0   0   0    1          0
	   no       1   0   1   0    0          0
	   yes      1   1   0   1    1          1
	   no       1   1   1   1    0          0
*/
#define ADD_WITH_SAT(b,a,x) \
	((b) = (a) + (x), (b) = (((~(a)^(x))&((a)^(b))) < 0 ? ((x) < 0 ? INT_MIN : INT_MAX) : (b)))

/* Matrices, points and affine transformations */

const fz_matrix fz_identity = { 1, 0, 0, 1, 0, 0 };

/*
	Multiply two matrices.

	The order of the two matrices are important since matrix
	multiplication is not commutative.

	Returns result.
*/
fz_matrix
fz_concat(fz_matrix one, fz_matrix two)
{
	fz_matrix dst;
	dst.a = one.a * two.a + one.b * two.c;
	dst.b = one.a * two.b + one.b * two.d;
	dst.c = one.c * two.a + one.d * two.c;
	dst.d = one.c * two.b + one.d * two.d;
	dst.e = one.e * two.a + one.f * two.c + two.e;
	dst.f = one.e * two.b + one.f * two.d + two.f;
	return dst;
}

/*
	Create a scaling matrix.

	The returned matrix is of the form [ sx 0 0 sy 0 0 ].

	m: Pointer to the matrix to populate

	sx, sy: Scaling factors along the X- and Y-axes. A scaling
	factor of 1.0 will not cause any scaling along the relevant
	axis.

	Returns m.
*/
fz_matrix
fz_scale(float sx, float sy)
{
	fz_matrix m;
	m.a = sx; m.b = 0;
	m.c = 0; m.d = sy;
	m.e = 0; m.f = 0;
	return m;
}

/*
	Scale a matrix by premultiplication.

	m: Pointer to the matrix to scale

	sx, sy: Scaling factors along the X- and Y-axes. A scaling
	factor of 1.0 will not cause any scaling along the relevant
	axis.

	Returns m (updated).
*/
fz_matrix
fz_pre_scale(fz_matrix m, float sx, float sy)
{
	m.a *= sx;
	m.b *= sx;
	m.c *= sy;
	m.d *= sy;
	return m;
}

/*
	Scale a matrix by postmultiplication.

	m: Pointer to the matrix to scale

	sx, sy: Scaling factors along the X- and Y-axes. A scaling
	factor of 1.0 will not cause any scaling along the relevant
	axis.

	Returns m (updated).
*/
fz_matrix
fz_post_scale(fz_matrix m, float sx, float sy)
{
	m.a *= sx;
	m.b *= sy;
	m.c *= sx;
	m.d *= sy;
	m.e *= sx;
	m.f *= sy;
	return m;
}

/*
	Create a shearing matrix.

	The returned matrix is of the form [ 1 sy sx 1 0 0 ].

	m: pointer to place to store returned matrix

	sx, sy: Shearing factors. A shearing factor of 0.0 will not
	cause any shearing along the relevant axis.

	Returns m.
*/
fz_matrix
fz_shear(float h, float v)
{
	fz_matrix m;
	m.a = 1; m.b = v;
	m.c = h; m.d = 1;
	m.e = 0; m.f = 0;
	return m;
}

/*
	Premultiply a matrix with a shearing matrix.

	The shearing matrix is of the form [ 1 sy sx 1 0 0 ].

	m: pointer to matrix to premultiply

	sx, sy: Shearing factors. A shearing factor of 0.0 will not
	cause any shearing along the relevant axis.

	Returns m (updated).
*/
fz_matrix
fz_pre_shear(fz_matrix m, float h, float v)
{
	float a = m.a;
	float b = m.b;
	m.a += v * m.c;
	m.b += v * m.d;
	m.c += h * a;
	m.d += h * b;
	return m;
}

/*
	Create a rotation matrix.

	The returned matrix is of the form
	[ cos(deg) sin(deg) -sin(deg) cos(deg) 0 0 ].

	m: Pointer to place to store matrix

	degrees: Degrees of counter clockwise rotation. Values less
	than zero and greater than 360 are handled as expected.

	Returns m.
*/
fz_matrix
fz_rotate(float theta)
{
	fz_matrix m;
	float s;
	float c;

	while (theta < 0)
		theta += 360;
	while (theta >= 360)
		theta -= 360;

	if (fabsf(0 - theta) < FLT_EPSILON)
	{
		s = 0;
		c = 1;
	}
	else if (fabsf(90.0f - theta) < FLT_EPSILON)
	{
		s = 1;
		c = 0;
	}
	else if (fabsf(180.0f - theta) < FLT_EPSILON)
	{
		s = 0;
		c = -1;
	}
	else if (fabsf(270.0f - theta) < FLT_EPSILON)
	{
		s = -1;
		c = 0;
	}
	else
	{
		s = sinf(theta * FZ_PI / 180);
		c = cosf(theta * FZ_PI / 180);
	}

	m.a = c; m.b = s;
	m.c = -s; m.d = c;
	m.e = 0; m.f = 0;
	return m;
}

/*
	Rotate a transformation by premultiplying.

	The premultiplied matrix is of the form
	[ cos(deg) sin(deg) -sin(deg) cos(deg) 0 0 ].

	m: Pointer to matrix to premultiply.

	degrees: Degrees of counter clockwise rotation. Values less
	than zero and greater than 360 are handled as expected.

	Returns m (updated).
*/
fz_matrix
fz_pre_rotate(fz_matrix m, float theta)
{
	while (theta < 0)
		theta += 360;
	while (theta >= 360)
		theta -= 360;

	if (fabsf(0 - theta) < FLT_EPSILON)
	{
		/* Nothing to do */
	}
	else if (fabsf(90.0f - theta) < FLT_EPSILON)
	{
		float a = m.a;
		float b = m.b;
		m.a = m.c;
		m.b = m.d;
		m.c = -a;
		m.d = -b;
	}
	else if (fabsf(180.0f - theta) < FLT_EPSILON)
	{
		m.a = -m.a;
		m.b = -m.b;
		m.c = -m.c;
		m.d = -m.d;
	}
	else if (fabsf(270.0f - theta) < FLT_EPSILON)
	{
		float a = m.a;
		float b = m.b;
		m.a = -m.c;
		m.b = -m.d;
		m.c = a;
		m.d = b;
	}
	else
	{
		float s = sinf(theta * FZ_PI / 180);
		float c = cosf(theta * FZ_PI / 180);
		float a = m.a;
		float b = m.b;
		m.a = c * a + s * m.c;
		m.b = c * b + s * m.d;
		m.c =-s * a + c * m.c;
		m.d =-s * b + c * m.d;
	}

	return m;
}

/*
	Create a translation matrix.

	The returned matrix is of the form [ 1 0 0 1 tx ty ].

	m: A place to store the created matrix.

	tx, ty: Translation distances along the X- and Y-axes. A
	translation of 0 will not cause any translation along the
	relevant axis.

	Returns m.
*/
fz_matrix
fz_translate(float tx, float ty)
{
	fz_matrix m;
	m.a = 1; m.b = 0;
	m.c = 0; m.d = 1;
	m.e = tx; m.f = ty;
	return m;
}

/*
	Translate a matrix by premultiplication.

	m: The matrix to translate

	tx, ty: Translation distances along the X- and Y-axes. A
	translation of 0 will not cause any translation along the
	relevant axis.

	Returns m.
*/
fz_matrix
fz_pre_translate(fz_matrix m, float tx, float ty)
{
	m.e += tx * m.a + ty * m.c;
	m.f += tx * m.b + ty * m.d;
	return m;
}

/*
	Create transform matrix to draw page
	at a given resolution and rotation. Adjusts the scaling
	factors so that the page covers whole number of
	pixels and adjust the page origin to be at 0,0.
*/
fz_matrix
fz_transform_page(fz_rect mediabox, float resolution, float rotate)
{
	float user_w, user_h, pixel_w, pixel_h;
	fz_rect pixel_box;
	fz_matrix matrix;

	/* Adjust scaling factors to cover whole pixels */
	user_w = mediabox.x1 - mediabox.x0;
	user_h = mediabox.y1 - mediabox.y0;
	pixel_w = floorf(user_w * resolution / 72 + 0.5f);
	pixel_h = floorf(user_h * resolution / 72 + 0.5f);

	matrix = fz_pre_rotate(fz_scale(pixel_w / user_w, pixel_h / user_h), rotate);

	/* Adjust the page origin to sit at 0,0 after rotation */
	pixel_box = fz_transform_rect(mediabox, matrix);
	matrix.e -= pixel_box.x0;
	matrix.f -= pixel_box.y0;

	return matrix;
}

/*
	Create an inverse matrix.

	inverse: Place to store inverse matrix.

	matrix: Matrix to invert. A degenerate matrix, where the
	determinant is equal to zero, can not be inverted and the
	original matrix is returned instead.

	Returns inverse.
*/
fz_matrix
fz_invert_matrix(fz_matrix src)
{
	float a = src.a;
	float det = a * src.d - src.b * src.c;
	if (det < -FLT_EPSILON || det > FLT_EPSILON)
	{
		fz_matrix dst;
		float rdet = 1 / det;
		dst.a = src.d * rdet;
		dst.b = -src.b * rdet;
		dst.c = -src.c * rdet;
		dst.d = a * rdet;
		a = -src.e * dst.a - src.f * dst.c;
		dst.f = -src.e * dst.b - src.f * dst.d;
		dst.e = a;
		return dst;
	}
	return src;
}

/*
	Attempt to create an inverse matrix.

	inverse: Place to store inverse matrix.

	matrix: Matrix to invert. A degenerate matrix, where the
	determinant is equal to zero, can not be inverted.

	Returns 1 if matrix is degenerate (singular), or 0 otherwise.
*/
int
fz_try_invert_matrix(fz_matrix *dst, fz_matrix src)
{
	double sa = (double)src.a;
	double sb = (double)src.b;
	double sc = (double)src.c;
	double sd = (double)src.d;
	double da, db, dc, dd;
	double det = sa * sd - sb * sc;
	if (det >= -DBL_EPSILON && det <= DBL_EPSILON)
		return 1;
	det = 1 / det;
	da = sd * det;
	dst->a = (float)da;
	db = -sb * det;
	dst->b = (float)db;
	dc = -sc * det;
	dst->c = (float)dc;
	dd = sa * det;
	dst->d = (float)dd;
	da = -src.e * da - src.f * dc;
	dst->f = (float)(-src.e * db - src.f * dd);
	dst->e = (float)da;
	return 0;
}

/*
	Check if a transformation is rectilinear.

	Rectilinear means that no shearing is present and that any
	rotations present are a multiple of 90 degrees. Usually this
	is used to make sure that axis-aligned rectangles before the
	transformation are still axis-aligned rectangles afterwards.
*/
int
fz_is_rectilinear(fz_matrix m)
{
	return (fabsf(m.b) < FLT_EPSILON && fabsf(m.c) < FLT_EPSILON) ||
		(fabsf(m.a) < FLT_EPSILON && fabsf(m.d) < FLT_EPSILON);
}

/*
	Calculate average scaling factor of matrix.
*/
float
fz_matrix_expansion(fz_matrix m)
{
	return sqrtf(fabsf(m.a * m.d - m.b * m.c));
}

float
fz_matrix_max_expansion(fz_matrix m)
{
	float max = fabsf(m.a);
	float x = fabsf(m.b);
	if (max < x)
		max = x;
	x = fabsf(m.c);
	if (max < x)
		max = x;
	x = fabsf(m.d);
	if (max < x)
		max = x;
	return max;
}

/*
	Apply a transformation to a point.

	transform: Transformation matrix to apply. See fz_concat,
	fz_scale, fz_rotate and fz_translate for how to create a
	matrix.

	point: Pointer to point to update.

	Returns transform (unchanged).
*/
fz_point
fz_transform_point(fz_point p, fz_matrix m)
{
	float x = p.x;
	p.x = x * m.a + p.y * m.c + m.e;
	p.y = x * m.b + p.y * m.d + m.f;
	return p;
}

fz_point
fz_transform_point_xy(float x, float y, fz_matrix m)
{
	fz_point p;
	p.x = x * m.a + y * m.c + m.e;
	p.y = x * m.b + y * m.d + m.f;
	return p;
}

/*
	Apply a transformation to a vector.

	transform: Transformation matrix to apply. See fz_concat,
	fz_scale and fz_rotate for how to create a matrix. Any
	translation will be ignored.

	vector: Pointer to vector to update.
*/
fz_point
fz_transform_vector(fz_point p, fz_matrix m)
{
	float x = p.x;
	p.x = x * m.a + p.y * m.c;
	p.y = x * m.b + p.y * m.d;
	return p;
}

/*
	Normalize a vector to length one.
*/
fz_point
fz_normalize_vector(fz_point p)
{
	float len = p.x * p.x + p.y * p.y;
	if (len != 0)
	{
		len = sqrtf(len);
		p.x /= len;
		p.y /= len;
	}
	return p;
}

/* Rectangles and bounding boxes */

/* biggest and smallest integers that a float can represent perfectly (i.e. 24 bits) */
#define MAX_SAFE_INT 16777216
#define MIN_SAFE_INT -16777216

const fz_rect fz_infinite_rect = { 1, 1, -1, -1 };
const fz_rect fz_empty_rect = { 0, 0, 0, 0 };
const fz_rect fz_unit_rect = { 0, 0, 1, 1 };

const fz_irect fz_infinite_irect = { 1, 1, -1, -1 };
const fz_irect fz_empty_irect = { 0, 0, 0, 0 };
const fz_irect fz_unit_bbox = { 0, 0, 1, 1 };

/*
	Convert a rect into the minimal bounding box
	that covers the rectangle.

	Coordinates in a bounding box are integers, so rounding of the
	rects coordinates takes place. The top left corner is rounded
	upwards and left while the bottom right corner is rounded
	downwards and to the right.
*/
fz_irect
fz_irect_from_rect(fz_rect r)
{
	fz_irect b;
	if (fz_is_empty_rect(r))
	{
		b.x0 = 0;
		b.y0 = 0;
		b.x1 = 0;
		b.y1 = 0;
	}
	else
	{
		b.x0 = fz_clamp(floorf(r.x0), MIN_SAFE_INT, MAX_SAFE_INT);
		b.y0 = fz_clamp(floorf(r.y0), MIN_SAFE_INT, MAX_SAFE_INT);
		b.x1 = fz_clamp(ceilf(r.x1), MIN_SAFE_INT, MAX_SAFE_INT);
		b.y1 = fz_clamp(ceilf(r.y1), MIN_SAFE_INT, MAX_SAFE_INT);
	}
	return b;
}

/*
	Convert a bbox into a rect.

	For our purposes, a rect can represent all the values we meet in
	a bbox, so nothing can go wrong.

	rect: A place to store the generated rectangle.

	bbox: The bbox to convert.

	Returns rect (updated).
*/
fz_rect
fz_rect_from_irect(fz_irect a)
{
	fz_rect r;
	r.x0 = a.x0;
	r.y0 = a.y0;
	r.x1 = a.x1;
	r.y1 = a.y1;
	return r;
}

/*
	Round rectangle coordinates.

	Coordinates in a bounding box are integers, so rounding of the
	rects coordinates takes place. The top left corner is rounded
	upwards and left while the bottom right corner is rounded
	downwards and to the right.

	This differs from fz_irect_from_rect, in that fz_irect_from_rect
	slavishly follows the numbers (i.e any slight over/under calculations
	can cause whole extra pixels to be added). fz_round_rect
	allows for a small amount of rounding error when calculating
	the bbox.
*/
fz_irect
fz_round_rect(fz_rect r)
{
	fz_irect b;
	int i;

	i = floorf(r.x0 + 0.001f);
	b.x0 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);
	i = floorf(r.y0 + 0.001f);
	b.y0 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);
	i = ceilf(r.x1 - 0.001f);
	b.x1 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);
	i = ceilf(r.y1 - 0.001f);
	b.y1 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);

	return b;
}

/*
	Compute intersection of two rectangles.

	Given two rectangles, update the first to be the smallest
	axis-aligned rectangle that covers the area covered by both
	given rectangles. If either rectangle is empty then the
	intersection is also empty. If either rectangle is infinite
	then the intersection is simply the non-infinite rectangle.
	Should both rectangles be infinite, then the intersection is
	also infinite.
*/
fz_rect
fz_intersect_rect(fz_rect a, fz_rect b)
{
	/* Check for empty box before infinite box */
	if (fz_is_empty_rect(a)) return fz_empty_rect;
	if (fz_is_empty_rect(b)) return fz_empty_rect;
	if (fz_is_infinite_rect(b)) return a;
	if (fz_is_infinite_rect(a)) return b;
	if (a.x0 < b.x0)
		a.x0 = b.x0;
	if (a.y0 < b.y0)
		a.y0 = b.y0;
	if (a.x1 > b.x1)
		a.x1 = b.x1;
	if (a.y1 > b.y1)
		a.y1 = b.y1;
	if (a.x1 < a.x0 || a.y1 < a.y0)
		return fz_empty_rect;
	return a;
}

/*
	Compute intersection of two bounding boxes.

	Similar to fz_intersect_rect but operates on two bounding
	boxes instead of two rectangles.
*/
fz_irect
fz_intersect_irect(fz_irect a, fz_irect b)
{
	/* Check for empty box before infinite box */
	if (fz_is_empty_irect(a)) return fz_empty_irect;
	if (fz_is_empty_irect(b)) return fz_empty_irect;
	if (fz_is_infinite_irect(b)) return a;
	if (fz_is_infinite_irect(a)) return b;
	if (a.x0 < b.x0)
		a.x0 = b.x0;
	if (a.y0 < b.y0)
		a.y0 = b.y0;
	if (a.x1 > b.x1)
		a.x1 = b.x1;
	if (a.y1 > b.y1)
		a.y1 = b.y1;
	if (a.x1 < a.x0 || a.y1 < a.y0)
		return fz_empty_irect;
	return a;
}

/*
	Compute union of two rectangles.

	Given two rectangles, update the first to be the smallest
	axis-aligned rectangle that encompasses both given rectangles.
	If either rectangle is infinite then the union is also infinite.
	If either rectangle is empty then the union is simply the
	non-empty rectangle. Should both rectangles be empty, then the
	union is also empty.
*/
fz_rect
fz_union_rect(fz_rect a, fz_rect b)
{
	/* Check for empty box before infinite box */
	if (fz_is_empty_rect(b)) return a;
	if (fz_is_empty_rect(a)) return b;
	if (fz_is_infinite_rect(a)) return a;
	if (fz_is_infinite_rect(b)) return b;
	if (a.x0 > b.x0)
		a.x0 = b.x0;
	if (a.y0 > b.y0)
		a.y0 = b.y0;
	if (a.x1 < b.x1)
		a.x1 = b.x1;
	if (a.y1 < b.y1)
		a.y1 = b.y1;
	return a;
}

/*
	Translate bounding box.

	Translate a bbox by a given x and y offset. Allows for overflow.
*/
fz_rect
fz_translate_rect(fz_rect a, float xoff, float yoff)
{
	if (fz_is_empty_rect(a)) return a;
	if (fz_is_infinite_rect(a)) return a;
	a.x0 += xoff;
	a.y0 += yoff;
	a.x1 += xoff;
	a.y1 += yoff;
	return a;
}

fz_irect
fz_translate_irect(fz_irect a, int xoff, int yoff)
{
	int t;

	if (fz_is_empty_irect(a)) return a;
	if (fz_is_infinite_irect(a)) return a;
	a.x0 = ADD_WITH_SAT(t, a.x0, xoff);
	a.y0 = ADD_WITH_SAT(t, a.y0, yoff);
	a.x1 = ADD_WITH_SAT(t, a.x1, xoff);
	a.y1 = ADD_WITH_SAT(t, a.y1, yoff);
	return a;
}

/*
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
fz_rect
fz_transform_rect(fz_rect r, fz_matrix m)
{
	fz_point s, t, u, v;

	if (fz_is_infinite_rect(r))
		return r;

	if (fabsf(m.b) < FLT_EPSILON && fabsf(m.c) < FLT_EPSILON)
	{
		if (m.a < 0)
		{
			float f = r.x0;
			r.x0 = r.x1;
			r.x1 = f;
		}
		if (m.d < 0)
		{
			float f = r.y0;
			r.y0 = r.y1;
			r.y1 = f;
		}
		s = fz_transform_point_xy(r.x0, r.y0, m);
		t = fz_transform_point_xy(r.x1, r.y1, m);
		r.x0 = s.x; r.y0 = s.y;
		r.x1 = t.x; r.y1 = t.y;
		return r;
	}

	s.x = r.x0; s.y = r.y0;
	t.x = r.x0; t.y = r.y1;
	u.x = r.x1; u.y = r.y1;
	v.x = r.x1; v.y = r.y0;
	s = fz_transform_point(s, m);
	t = fz_transform_point(t, m);
	u = fz_transform_point(u, m);
	v = fz_transform_point(v, m);
	r.x0 = MIN4(s.x, t.x, u.x, v.x);
	r.y0 = MIN4(s.y, t.y, u.y, v.y);
	r.x1 = MAX4(s.x, t.x, u.x, v.x);
	r.y1 = MAX4(s.y, t.y, u.y, v.y);
	return r;
}

fz_irect
fz_expand_irect(fz_irect a, int expand)
{
	if (fz_is_infinite_irect(a)) return a;
	a.x0 -= expand;
	a.y0 -= expand;
	a.x1 += expand;
	a.y1 += expand;
	return a;
}

/*
	Expand a bbox by a given amount in all directions.
*/
fz_rect
fz_expand_rect(fz_rect a, float expand)
{
	if (fz_is_infinite_rect(a)) return a;
	a.x0 -= expand;
	a.y0 -= expand;
	a.x1 += expand;
	a.y1 += expand;
	return a;
}

/*
	Expand a bbox to include a given point.
	To create a rectangle that encompasses a sequence of points, the
	rectangle must first be set to be the empty rectangle at one of
	the points before including the others.
*/
fz_rect fz_include_point_in_rect(fz_rect r, fz_point p)
{
	if (fz_is_infinite_rect(r)) return r;
	if (p.x < r.x0) r.x0 = p.x;
	if (p.x > r.x1) r.x1 = p.x;
	if (p.y < r.y0) r.y0 = p.y;
	if (p.y > r.y1) r.y1 = p.y;
	return r;
}

/*
	Test rectangle inclusion.

	Return true if a entirely contains b.
*/
int fz_contains_rect(fz_rect a, fz_rect b)
{
	if (fz_is_empty_rect(b))
		return 1;
	if (fz_is_empty_rect(a))
		return 0;
	return ((a.x0 <= b.x0) &&
		(a.y0 <= b.y0) &&
		(a.x1 >= b.x1) &&
		(a.y1 >= b.y1));
}

fz_rect
fz_rect_from_quad(fz_quad q)
{
	fz_rect r;
	r.x0 = MIN4(q.ll.x, q.lr.x, q.ul.x, q.ur.x);
	r.y0 = MIN4(q.ll.y, q.lr.y, q.ul.y, q.ur.y);
	r.x1 = MAX4(q.ll.x, q.lr.x, q.ul.x, q.ur.x);
	r.y1 = MAX4(q.ll.y, q.lr.y, q.ul.y, q.ur.y);
	return r;
}

fz_quad
fz_transform_quad(fz_quad q, fz_matrix m)
{
	q.ul = fz_transform_point(q.ul, m);
	q.ur = fz_transform_point(q.ur, m);
	q.ll = fz_transform_point(q.ll, m);
	q.lr = fz_transform_point(q.lr, m);
	return q;
}

int fz_is_point_inside_rect(fz_point p, fz_rect r)
{
	return (p.x >= r.x0 && p.x < r.x1 && p.y >= r.y0 && p.y < r.y1);
}

int fz_is_point_inside_irect(int x, int y, fz_irect r)
{
	return (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1);
}

int fz_is_point_inside_quad(fz_point p, fz_quad q)
{
	// TODO: non-rectilinear quads
	return fz_is_point_inside_rect(p, fz_rect_from_quad(q));
}
