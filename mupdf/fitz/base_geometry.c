#include "fitz-internal.h"

#define MAX4(a,b,c,d) MAX(MAX(a,b), MAX(c,d))
#define MIN4(a,b,c,d) MIN(MIN(a,b), MIN(c,d))

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
fz_shear(float h, float v)
{
	fz_matrix m;
	m.a = 1; m.b = v;
	m.c = h; m.d = 1;
	m.e = 0; m.f = 0;
	return m;
}

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
		s = sinf(theta * (float)M_PI / 180);
		c = cosf(theta * (float)M_PI / 180);
	}

	m.a = c; m.b = s;
	m.c = -s; m.d = c;
	m.e = 0; m.f = 0;
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
fz_invert_matrix(fz_matrix src)
{
	float det = src.a * src.d - src.b * src.c;
	if (det < -FLT_EPSILON || det > FLT_EPSILON)
	{
		fz_matrix dst;
		float rdet = 1 / det;
		dst.a = src.d * rdet;
		dst.b = -src.b * rdet;
		dst.c = -src.c * rdet;
		dst.d = src.a * rdet;
		dst.e = -src.e * dst.a - src.f * dst.c;
		dst.f = -src.e * dst.b - src.f * dst.d;
		return dst;
	}
	return src;
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
fz_transform_point(fz_matrix m, fz_point p)
{
	fz_point t;
	t.x = p.x * m.a + p.y * m.c + m.e;
	t.y = p.x * m.b + p.y * m.d + m.f;
	return t;
}

fz_point
fz_transform_vector(fz_matrix m, fz_point p)
{
	fz_point t;
	t.x = p.x * m.a + p.y * m.c;
	t.y = p.x * m.b + p.y * m.d;
	return t;
}

/* Rectangles and bounding boxes */

const fz_rect fz_infinite_rect = { 1, 1, -1, -1 };
const fz_rect fz_empty_rect = { 0, 0, 0, 0 };
const fz_rect fz_unit_rect = { 0, 0, 1, 1 };

const fz_bbox fz_infinite_bbox = { 1, 1, -1, -1 };
const fz_bbox fz_empty_bbox = { 0, 0, 0, 0 };
const fz_bbox fz_unit_bbox = { 0, 0, 1, 1 };

#define SAFE_INT(f) ((f > INT_MAX) ? INT_MAX : ((f < INT_MIN) ? INT_MIN : (int)f))
fz_bbox
fz_bbox_covering_rect(fz_rect f)
{
	fz_bbox i;
	f.x0 = floorf(f.x0);
	f.y0 = floorf(f.y0);
	f.x1 = ceilf(f.x1);
	f.y1 = ceilf(f.y1);
	i.x0 = SAFE_INT(f.x0);
	i.y0 = SAFE_INT(f.y0);
	i.x1 = SAFE_INT(f.x1);
	i.y1 = SAFE_INT(f.y1);
	return i;
}

fz_bbox
fz_round_rect(fz_rect f)
{
	fz_bbox i;
	f.x0 = floorf(f.x0 + 0.001);
	f.y0 = floorf(f.y0 + 0.001);
	f.x1 = ceilf(f.x1 - 0.001);
	f.y1 = ceilf(f.y1 - 0.001);
	i.x0 = SAFE_INT(f.x0);
	i.y0 = SAFE_INT(f.y0);
	i.x1 = SAFE_INT(f.x1);
	i.y1 = SAFE_INT(f.y1);
	return i;
}

fz_rect
fz_intersect_rect(fz_rect a, fz_rect b)
{
	fz_rect r;
	if (fz_is_infinite_rect(a)) return b;
	if (fz_is_infinite_rect(b)) return a;
	if (fz_is_empty_rect(a)) return fz_empty_rect;
	if (fz_is_empty_rect(b)) return fz_empty_rect;
	r.x0 = MAX(a.x0, b.x0);
	r.y0 = MAX(a.y0, b.y0);
	r.x1 = MIN(a.x1, b.x1);
	r.y1 = MIN(a.y1, b.y1);
	return (r.x1 < r.x0 || r.y1 < r.y0) ? fz_empty_rect : r;
}

fz_rect
fz_union_rect(fz_rect a, fz_rect b)
{
	fz_rect r;
	if (fz_is_infinite_rect(a)) return a;
	if (fz_is_infinite_rect(b)) return b;
	if (fz_is_empty_rect(a)) return b;
	if (fz_is_empty_rect(b)) return a;
	r.x0 = MIN(a.x0, b.x0);
	r.y0 = MIN(a.y0, b.y0);
	r.x1 = MAX(a.x1, b.x1);
	r.y1 = MAX(a.y1, b.y1);
	return r;
}

fz_bbox
fz_intersect_bbox(fz_bbox a, fz_bbox b)
{
	fz_bbox r;
	if (fz_is_infinite_rect(a)) return b;
	if (fz_is_infinite_rect(b)) return a;
	if (fz_is_empty_rect(a)) return fz_empty_bbox;
	if (fz_is_empty_rect(b)) return fz_empty_bbox;
	r.x0 = MAX(a.x0, b.x0);
	r.y0 = MAX(a.y0, b.y0);
	r.x1 = MIN(a.x1, b.x1);
	r.y1 = MIN(a.y1, b.y1);
	return (r.x1 < r.x0 || r.y1 < r.y0) ? fz_empty_bbox : r;
}

fz_bbox
fz_union_bbox(fz_bbox a, fz_bbox b)
{
	fz_bbox r;
	if (fz_is_infinite_rect(a)) return a;
	if (fz_is_infinite_rect(b)) return b;
	if (fz_is_empty_rect(a)) return b;
	if (fz_is_empty_rect(b)) return a;
	r.x0 = MIN(a.x0, b.x0);
	r.y0 = MIN(a.y0, b.y0);
	r.x1 = MAX(a.x1, b.x1);
	r.y1 = MAX(a.y1, b.y1);
	return r;
}

fz_rect
fz_transform_rect(fz_matrix m, fz_rect r)
{
	fz_point s, t, u, v;

	if (fz_is_infinite_rect(r))
		return r;

	s.x = r.x0; s.y = r.y0;
	t.x = r.x0; t.y = r.y1;
	u.x = r.x1; u.y = r.y1;
	v.x = r.x1; v.y = r.y0;
	s = fz_transform_point(m, s);
	t = fz_transform_point(m, t);
	u = fz_transform_point(m, u);
	v = fz_transform_point(m, v);
	r.x0 = MIN4(s.x, t.x, u.x, v.x);
	r.y0 = MIN4(s.y, t.y, u.y, v.y);
	r.x1 = MAX4(s.x, t.x, u.x, v.x);
	r.y1 = MAX4(s.y, t.y, u.y, v.y);
	return r;
}

fz_bbox
fz_transform_bbox(fz_matrix m, fz_bbox b)
{
	fz_point s, t, u, v;

	if (fz_is_infinite_bbox(b))
		return b;

	s.x = b.x0; s.y = b.y0;
	t.x = b.x0; t.y = b.y1;
	u.x = b.x1; u.y = b.y1;
	v.x = b.x1; v.y = b.y0;
	s = fz_transform_point(m, s);
	t = fz_transform_point(m, t);
	u = fz_transform_point(m, u);
	v = fz_transform_point(m, v);
	b.x0 = MIN4(s.x, t.x, u.x, v.x);
	b.y0 = MIN4(s.y, t.y, u.y, v.y);
	b.x1 = MAX4(s.x, t.x, u.x, v.x);
	b.y1 = MAX4(s.y, t.y, u.y, v.y);
	return b;

}
