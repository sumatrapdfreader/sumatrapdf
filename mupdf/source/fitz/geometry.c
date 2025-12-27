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

fz_matrix
fz_scale(float sx, float sy)
{
	fz_matrix m;
	m.a = sx; m.b = 0;
	m.c = 0; m.d = sy;
	m.e = 0; m.f = 0;
	return m;
}

fz_matrix
fz_pre_scale(fz_matrix m, float sx, float sy)
{
	m.a *= sx;
	m.b *= sx;
	m.c *= sy;
	m.d *= sy;
	return m;
}

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

fz_matrix
fz_shear(float h, float v)
{
	fz_matrix m;
	m.a = 1; m.b = v;
	m.c = h; m.d = 1;
	m.e = 0; m.f = 0;
	return m;
}

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

fz_matrix
fz_rotate(float theta)
{
	fz_matrix m;
	float s;
	float c;

	theta = fmod(theta, 360);
	if (theta < 0)
		theta += 360;

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

fz_matrix
fz_pre_rotate(fz_matrix m, float theta)
{
	theta = fmod(theta, 360);
	if (theta < 0)
		theta += 360;

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

fz_matrix
fz_translate(float tx, float ty)
{
	fz_matrix m;
	m.a = 1; m.b = 0;
	m.c = 0; m.d = 1;
	m.e = tx; m.f = ty;
	return m;
}

fz_matrix
fz_pre_translate(fz_matrix m, float tx, float ty)
{
	m.e += tx * m.a + ty * m.c;
	m.f += tx * m.b + ty * m.d;
	return m;
}

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

fz_matrix
fz_invert_matrix(fz_matrix src)
{
	double sa = (double)src.a;
	double sb = (double)src.b;
	double sc = (double)src.c;
	double sd = (double)src.d;
	double da, db, dc, dd, de, df;
	double det = sa * sd - sb * sc;
	// If we cannot invert the matrix, return a degenerate one so we can
	// more easily spot it when debugging!
	if (det >= -DBL_EPSILON && det <= DBL_EPSILON)
		return fz_make_matrix(0, 0, 0, 0, 0, 0);
	det = 1 / det;
	da = sd * det;
	db = -sb * det;
	dc = -sc * det;
	dd = sa * det;
	de = -src.e * da - src.f * dc;
	df = -src.e * db - src.f * dd;
	return fz_make_matrix(da, db, dc, dd, de, df);
}

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

int
fz_is_rectilinear(fz_matrix m)
{
	return (fabsf(m.b) < FLT_EPSILON && fabsf(m.c) < FLT_EPSILON) ||
		(fabsf(m.a) < FLT_EPSILON && fabsf(m.d) < FLT_EPSILON);
}

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

fz_point
fz_transform_vector(fz_point p, fz_matrix m)
{
	float x = p.x;
	p.x = x * m.a + p.y * m.c;
	p.y = x * m.b + p.y * m.d;
	return p;
}

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

const fz_rect fz_infinite_rect = { FZ_MIN_INF_RECT, FZ_MIN_INF_RECT, FZ_MAX_INF_RECT, FZ_MAX_INF_RECT };
const fz_rect fz_empty_rect = { FZ_MAX_INF_RECT, FZ_MAX_INF_RECT, FZ_MIN_INF_RECT, FZ_MIN_INF_RECT };
const fz_rect fz_invalid_rect = { 0, 0, -1, -1 };
const fz_rect fz_unit_rect = { 0, 0, 1, 1 };

const fz_irect fz_infinite_irect = { FZ_MIN_INF_RECT, FZ_MIN_INF_RECT, FZ_MAX_INF_RECT, FZ_MAX_INF_RECT };
const fz_irect fz_empty_irect = { FZ_MAX_INF_RECT, FZ_MAX_INF_RECT, FZ_MIN_INF_RECT, FZ_MIN_INF_RECT };
const fz_irect fz_invalid_irect = { 0, 0, -1, -1 };
const fz_irect fz_unit_bbox = { 0, 0, 1, 1 };

const fz_quad fz_infinite_quad = { { -INFINITY, INFINITY}, {INFINITY, INFINITY}, {-INFINITY, -INFINITY}, {INFINITY, -INFINITY} };
const fz_quad fz_invalid_quad = { {NAN, NAN}, {NAN, NAN}, {NAN, NAN}, {NAN, NAN} };

fz_irect
fz_irect_from_rect(fz_rect r)
{
	fz_irect b;
	if (fz_is_infinite_rect(r))
		return fz_infinite_irect;
	if (!fz_is_valid_rect(r))
		return fz_invalid_irect;

	b.x0 = fz_clamp(floorf(r.x0), MIN_SAFE_INT, MAX_SAFE_INT);
	b.y0 = fz_clamp(floorf(r.y0), MIN_SAFE_INT, MAX_SAFE_INT);
	b.x1 = fz_clamp(ceilf(r.x1), MIN_SAFE_INT, MAX_SAFE_INT);
	b.y1 = fz_clamp(ceilf(r.y1), MIN_SAFE_INT, MAX_SAFE_INT);

	return b;
}

fz_rect
fz_rect_from_irect(fz_irect a)
{
	fz_rect r;

	if (fz_is_infinite_irect(a))
		return fz_infinite_rect;

	r.x0 = a.x0;
	r.y0 = a.y0;
	r.x1 = a.x1;
	r.y1 = a.y1;
	return r;
}

fz_irect
fz_round_rect(fz_rect r)
{
	fz_irect b;
	float f;

	f = floorf(r.x0 + 0.001f);
	b.x0 = fz_clamp(f, MIN_SAFE_INT, MAX_SAFE_INT);
	f = floorf(r.y0 + 0.001f);
	b.y0 = fz_clamp(f, MIN_SAFE_INT, MAX_SAFE_INT);
	f = ceilf(r.x1 - 0.001f);
	b.x1 = fz_clamp(f, MIN_SAFE_INT, MAX_SAFE_INT);
	f = ceilf(r.y1 - 0.001f);
	b.y1 = fz_clamp(f, MIN_SAFE_INT, MAX_SAFE_INT);

	return b;
}

fz_rect
fz_intersect_rect(fz_rect a, fz_rect b)
{
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
	return a;
}

fz_irect
fz_intersect_irect(fz_irect a, fz_irect b)
{
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
	return a;
}

fz_rect
fz_union_rect(fz_rect a, fz_rect b)
{
	/* Check for empty box before infinite box */
	if (!fz_is_valid_rect(b)) return a;
	if (!fz_is_valid_rect(a)) return b;
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

fz_rect
fz_translate_rect(fz_rect a, float xoff, float yoff)
{
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

fz_rect
fz_transform_rect(fz_rect r, fz_matrix m)
{
	fz_point s, t, u, v;
	int invalid;

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
		/* If r was invalid coming in, it'll still be invalid now. */
		return r;
	}
	else if (fabsf(m.a) < FLT_EPSILON && fabsf(m.d) < FLT_EPSILON)
	{
		if (m.b < 0)
		{
			float f = r.x0;
			r.x0 = r.x1;
			r.x1 = f;
		}
		if (m.c < 0)
		{
			float f = r.y0;
			r.y0 = r.y1;
			r.y1 = f;
		}
		s = fz_transform_point_xy(r.x0, r.y0, m);
		t = fz_transform_point_xy(r.x1, r.y1, m);
		r.x0 = s.x; r.y0 = s.y;
		r.x1 = t.x; r.y1 = t.y;
		/* If r was invalid coming in, it'll still be invalid now. */
		return r;
	}

	invalid = (r.x0 > r.x1) || (r.y0 > r.y1);
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

	/* If we were called with an invalid rectangle, return an
	 * invalid rectangle after transformation. */
	if (invalid)
	{
		float f;
		f = r.x0; r.x0 = r.x1; r.x1 = f;
		f = r.y0; r.y0 = r.y1; r.y1 = f;
	}
	return r;
}

fz_irect
fz_expand_irect(fz_irect a, int expand)
{
	if (fz_is_infinite_irect(a)) return a;
	if (!fz_is_valid_irect(a)) return a;
	a.x0 -= expand;
	a.y0 -= expand;
	a.x1 += expand;
	a.y1 += expand;
	return a;
}

fz_rect
fz_expand_rect(fz_rect a, float expand)
{
	if (fz_is_infinite_rect(a)) return a;
	if (!fz_is_valid_rect(a)) return a;
	a.x0 -= expand;
	a.y0 -= expand;
	a.x1 += expand;
	a.y1 += expand;
	return a;
}

/* Adding a point to an invalid rectangle makes the zero area rectangle
 * that contains just that point. */
fz_rect fz_include_point_in_rect(fz_rect r, fz_point p)
{
	if (fz_is_infinite_rect(r)) return r;
	if (p.x < r.x0) r.x0 = p.x;
	if (p.x > r.x1) r.x1 = p.x;
	if (p.y < r.y0) r.y0 = p.y;
	if (p.y > r.y1) r.y1 = p.y;
	return r;
}

int fz_contains_rect(fz_rect a, fz_rect b)
{
	/* An invalid rect can't contain anything */
	if (!fz_is_valid_rect(a))
		return 0;
	/* Any valid rect contains all invalid rects */
	if (!fz_is_valid_rect(b))
		return 1;
	return ((a.x0 <= b.x0) &&
		(a.y0 <= b.y0) &&
		(a.x1 >= b.x1) &&
		(a.y1 >= b.y1));
}

int fz_overlaps_rect(fz_rect a, fz_rect b)
{
	return !fz_is_empty_rect(fz_intersect_rect(a, b));
}

int
fz_is_valid_quad(fz_quad q)
{
	return (!isnan(q.ll.x) &&
		!isnan(q.ll.y) &&
		!isnan(q.ul.x) &&
		!isnan(q.ul.y) &&
		!isnan(q.lr.x) &&
		!isnan(q.lr.y) &&
		!isnan(q.ur.x) &&
		!isnan(q.ur.y));
}

int
fz_is_infinite_quad(fz_quad q)
{
	/* For a quad to be infinite, all the ordinates need to be infinite. */
	if (!isinf(q.ll.x) ||
		!isinf(q.ll.y) ||
		!isinf(q.ul.x) ||
		!isinf(q.ul.y) ||
		!isinf(q.lr.x) ||
		!isinf(q.lr.y) ||
		!isinf(q.ur.x) ||
		!isinf(q.ur.y))
		return 0;

	/* Just because all the ordinates are infinite, we don't necessarily have an infinite quad. */
	/* The quad points in order are: ll, ul, ur, lr  OR  reversed:  ll, lr, ur, ul */
	/* The required points are: (-inf, -inf) (-inf, inf) (inf, inf) (inf, -inf) */

	/* Not the fastest way to code it, but easy to understand! */
#define INF_QUAD_TEST(A,B,C,D) \
	if (q.A.x < 0 && q.A.y < 0 && q.B.x < 0 && q.B.y > 0 && q.C.x > 0 && q.C.y > 0 && q.D.x > 0 && q.D.y < 0) return 1

	INF_QUAD_TEST(ll, ul, ur, lr);
	INF_QUAD_TEST(ul, ur, lr, ll);
	INF_QUAD_TEST(ur, lr, ll, ul);
	INF_QUAD_TEST(lr, ll, ul, ur);
	INF_QUAD_TEST(ll, lr, ur, ul);
	INF_QUAD_TEST(lr, ur, ul, ll);
	INF_QUAD_TEST(ur, ul, ll, lr);
	INF_QUAD_TEST(ul, ll, lr, ur);
#undef INF_QUAD_TEST

	return 0;
}

int fz_is_empty_quad(fz_quad q)
{
	/* Using "Shoelace" formula */
	float area;

	if (fz_is_infinite_quad(q))
		return 0;
	if (!fz_is_valid_quad(q))
		return 1; /* All invalid quads are empty */

	area = q.ll.x * q.lr.y +
		q.lr.x * q.ur.y +
		q.ur.x * q.ul.y +
		q.ul.x * q.ll.y -
		q.lr.x * q.ll.y -
		q.ur.x * q.lr.y -
		q.ul.x * q.ur.y -
		q.ll.x * q.ul.y;

	return area == 0;
}

fz_rect
fz_rect_from_quad(fz_quad q)
{
	fz_rect r;

	if (!fz_is_valid_quad(q))
		return fz_invalid_rect;
	if (fz_is_infinite_quad(q))
		return fz_infinite_rect;

	r.x0 = MIN4(q.ll.x, q.lr.x, q.ul.x, q.ur.x);
	r.y0 = MIN4(q.ll.y, q.lr.y, q.ul.y, q.ur.y);
	r.x1 = MAX4(q.ll.x, q.lr.x, q.ul.x, q.ur.x);
	r.y1 = MAX4(q.ll.y, q.lr.y, q.ul.y, q.ur.y);
	return r;
}

fz_quad
fz_transform_quad(fz_quad q, fz_matrix m)
{
	if (!fz_is_valid_quad(q))
		return q;
	if (fz_is_infinite_quad(q))
		return q;

	q.ul = fz_transform_point(q.ul, m);
	q.ur = fz_transform_point(q.ur, m);
	q.ll = fz_transform_point(q.ll, m);
	q.lr = fz_transform_point(q.lr, m);
	return q;
}

fz_quad
fz_quad_from_rect(fz_rect r)
{
	fz_quad q;

	if (!fz_is_valid_rect(r))
		return fz_invalid_quad;
	if (fz_is_infinite_rect(r))
		return fz_infinite_quad;

	q.ul = fz_make_point(r.x0, r.y0);
	q.ur = fz_make_point(r.x1, r.y0);
	q.ll = fz_make_point(r.x0, r.y1);
	q.lr = fz_make_point(r.x1, r.y1);
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

int fz_is_rect_inside_rect(fz_rect inner, fz_rect outer)
{
	if (!fz_is_valid_rect(outer))
		return 0;
	if (!fz_is_valid_rect(inner))
		return 0;
	if (inner.x0 >= outer.x0 && inner.x1 <= outer.x1 && inner.y0 >= outer.y0 && inner.y1 <= outer.y1)
		return 1;
	return 0;
}

int fz_is_irect_inside_irect(fz_irect inner, fz_irect outer)
{
	if (!fz_is_valid_irect(outer))
		return 0;
	if (!fz_is_valid_irect(inner))
		return 0;
	if (inner.x0 >= outer.x0 && inner.x1 <= outer.x1 && inner.y0 >= outer.y0 && inner.y1 <= outer.y1)
		return 1;
	return 0;
}

/* cross (b-a) with (p-a) */
static float
cross(fz_point a, fz_point b, fz_point p)
{
	b.x -= a.x;
	b.y -= a.y;
	p.x -= a.x;
	p.y -= a.y;
	return b.x * p.y - b.y * p.x;
}

static int fz_is_point_inside_triangle(fz_point p, fz_point a, fz_point b, fz_point c)
{
	/* Consider the following:
	 *
	 *       P
	 *      /|
	 *     / |
	 *    /  |
	 * A +---+-------+ B
	 *       M
	 *
	 * The cross product of vector AB and vector AP is the distance PM.
	 * The sign of this distance depends on what side of the line AB, P lies on.
	 *
	 * So, for a triangle ABC, if we take cross products of:
	 *
	 *  AB and AP
	 *  BC and BP
	 *  CA and CP
	 *
	 * P can only be inside the triangle if the signs are all identical.
	 *
	 * One of the cross products being 0 indicates that the point is on a line.
	 * Two of the cross products being 0 indicates that the point is on a vertex.
	 *
	 * If 2 of the vertices are the same, the algorithm still works.
	 * Iff all 3 of the vertices are the same, the cross products are all zero. The
	 * value of p is irrelevant.
	 */
	float crossa = cross(a, b, p);
	float crossb = cross(b, c, p);
	float crossc = cross(c, a, p);

	/* Check for degenerate case. All vertices the same. */
	if (crossa == 0 && crossb == 0 && crossc == 0)
		return a.x == p.x && a.y == p.y;

	if (crossa >= 0 && crossb >= 0 && crossc >= 0)
		return 1;
	if (crossa <= 0 && crossb <= 0 && crossc <= 0)
		return 1;

	return 0;
}

int fz_is_point_inside_quad(fz_point p, fz_quad q)
{
	if (!fz_is_valid_quad(q))
		return 0;
	if (fz_is_infinite_quad(q))
		return 1;

	return
		fz_is_point_inside_triangle(p, q.ul, q.ur, q.lr) ||
		fz_is_point_inside_triangle(p, q.ul, q.lr, q.ll);
}

int fz_is_quad_inside_quad(fz_quad needle, fz_quad haystack)
{
	if (!fz_is_valid_quad(needle) || !fz_is_valid_quad(haystack))
		return 0;
	if (fz_is_infinite_quad(haystack))
		return 1;

	return
		fz_is_point_inside_quad(needle.ul, haystack) &&
		fz_is_point_inside_quad(needle.ur, haystack) &&
		fz_is_point_inside_quad(needle.ll, haystack) &&
		fz_is_point_inside_quad(needle.lr, haystack);
}

int fz_is_quad_intersecting_quad(fz_quad a, fz_quad b)
{
	fz_rect ar = fz_rect_from_quad(a);
	fz_rect br = fz_rect_from_quad(b);
	return !fz_is_empty_rect(fz_intersect_rect(ar, br));
}
