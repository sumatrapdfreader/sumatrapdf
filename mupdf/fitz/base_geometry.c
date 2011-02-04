#include "fitz.h"

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
fz_invertmatrix(fz_matrix src)
{
	fz_matrix dst;
	float rdet = 1 / (src.a * src.d - src.b * src.c);
	dst.a = src.d * rdet;
	dst.b = -src.b * rdet;
	dst.c = -src.c * rdet;
	dst.d = src.a * rdet;
	dst.e = -src.e * dst.a - src.f * dst.c;
	dst.f = -src.e * dst.b - src.f * dst.d;
	return dst;
}

int
fz_isrectilinear(fz_matrix m)
{
	return (fabsf(m.b) < FLT_EPSILON && fabsf(m.c) < FLT_EPSILON) ||
		(fabsf(m.a) < FLT_EPSILON && fabsf(m.d) < FLT_EPSILON);
}

float
fz_matrixexpansion(fz_matrix m)
{
	return sqrtf(fabsf(m.a * m.d - m.b * m.c));
}

fz_point
fz_transformpoint(fz_matrix m, fz_point p)
{
	fz_point t;
	t.x = p.x * m.a + p.y * m.c + m.e;
	t.y = p.x * m.b + p.y * m.d + m.f;
	return t;
}

fz_point
fz_transformvector(fz_matrix m, fz_point p)
{
	fz_point t;
	t.x = p.x * m.a + p.y * m.c;
	t.y = p.x * m.b + p.y * m.d;
	return t;
}

/* Rectangles and bounding boxes */

const fz_rect fz_infiniterect = { 1, 1, -1, -1 };
const fz_rect fz_emptyrect = { 0, 0, 0, 0 };
const fz_rect fz_unitrect = { 0, 0, 1, 1 };

const fz_bbox fz_infinitebbox = { 1, 1, -1, -1 };
const fz_bbox fz_emptybbox = { 0, 0, 0, 0 };
const fz_bbox fz_unitbbox = { 0, 0, 1, 1 };

fz_bbox
fz_roundrect(fz_rect f)
{
	fz_bbox i;
	i.x0 = floorf(f.x0 + 0.001f); /* adjust by 0.001 to compensate for precision errors */
	i.y0 = floorf(f.y0 + 0.001f);
	i.x1 = ceilf(f.x1 - 0.001f);
	i.y1 = ceilf(f.y1 - 0.001f);
	return i;
}

fz_bbox
fz_intersectbbox(fz_bbox a, fz_bbox b)
{
	fz_bbox r;
	if (fz_isinfiniterect(a)) return b;
	if (fz_isinfiniterect(b)) return a;
	if (fz_isemptyrect(a)) return fz_emptybbox;
	if (fz_isemptyrect(b)) return fz_emptybbox;
	r.x0 = MAX(a.x0, b.x0);
	r.y0 = MAX(a.y0, b.y0);
	r.x1 = MIN(a.x1, b.x1);
	r.y1 = MIN(a.y1, b.y1);
	return (r.x1 < r.x0 || r.y1 < r.y0) ? fz_emptybbox : r;
}

fz_bbox
fz_unionbbox(fz_bbox a, fz_bbox b)
{
	fz_bbox r;
	if (fz_isinfiniterect(a)) return a;
	if (fz_isinfiniterect(b)) return b;
	if (fz_isemptyrect(a)) return b;
	if (fz_isemptyrect(b)) return a;
	r.x0 = MIN(a.x0, b.x0);
	r.y0 = MIN(a.y0, b.y0);
	r.x1 = MAX(a.x1, b.x1);
	r.y1 = MAX(a.y1, b.y1);
	return r;
}

fz_rect
fz_transformrect(fz_matrix m, fz_rect r)
{
	fz_point s, t, u, v;

	if (fz_isinfiniterect(r))
		return r;

	s.x = r.x0; s.y = r.y0;
	t.x = r.x0; t.y = r.y1;
	u.x = r.x1; u.y = r.y1;
	v.x = r.x1; v.y = r.y0;
	s = fz_transformpoint(m, s);
	t = fz_transformpoint(m, t);
	u = fz_transformpoint(m, u);
	v = fz_transformpoint(m, v);
	r.x0 = MIN4(s.x, t.x, u.x, v.x);
	r.y0 = MIN4(s.y, t.y, u.y, v.y);
	r.x1 = MAX4(s.x, t.x, u.x, v.x);
	r.y1 = MAX4(s.y, t.y, u.y, v.y);
	return r;
}

fz_bbox
fz_transformbbox(fz_matrix m, fz_bbox b)
{
	fz_point s, t, u, v;

	if (fz_isinfinitebbox(b))
		return b;

	s.x = b.x0; s.y = b.y0;
	t.x = b.x0; t.y = b.y1;
	u.x = b.x1; u.y = b.y1;
	v.x = b.x1; v.y = b.y0;
	s = fz_transformpoint(m, s);
	t = fz_transformpoint(m, t);
	u = fz_transformpoint(m, u);
	v = fz_transformpoint(m, v);
	b.x0 = MIN4(s.x, t.x, u.x, v.x);
	b.y0 = MIN4(s.y, t.y, u.y, v.y);
	b.x1 = MAX4(s.x, t.x, u.x, v.x);
	b.y1 = MAX4(s.y, t.y, u.y, v.y);
	return b;

}
