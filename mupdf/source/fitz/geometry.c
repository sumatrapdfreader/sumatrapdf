#include "mupdf/fitz.h"

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

fz_matrix *
fz_concat(fz_matrix *dst, const fz_matrix *one, const fz_matrix *two)
{
	fz_matrix dst2;
	dst2.a = one->a * two->a + one->b * two->c;
	dst2.b = one->a * two->b + one->b * two->d;
	dst2.c = one->c * two->a + one->d * two->c;
	dst2.d = one->c * two->b + one->d * two->d;
	dst2.e = one->e * two->a + one->f * two->c + two->e;
	dst2.f = one->e * two->b + one->f * two->d + two->f;
	*dst = dst2;
	return dst;
}

fz_matrix *
fz_scale(fz_matrix *m, float sx, float sy)
{
	m->a = sx; m->b = 0;
	m->c = 0; m->d = sy;
	m->e = 0; m->f = 0;
	return m;
}

fz_matrix *
fz_pre_scale(fz_matrix *mat, float sx, float sy)
{
	mat->a *= sx;
	mat->b *= sx;
	mat->c *= sy;
	mat->d *= sy;
	return mat;
}

fz_matrix *
fz_shear(fz_matrix *mat, float h, float v)
{
	mat->a = 1; mat->b = v;
	mat->c = h; mat->d = 1;
	mat->e = 0; mat->f = 0;
	return mat;
}

fz_matrix *
fz_pre_shear(fz_matrix *mat, float h, float v)
{
	float a = mat->a;
	float b = mat->b;
	mat->a += v * mat->c;
	mat->b += v * mat->d;
	mat->c += h * a;
	mat->d += h * b;
	return mat;
}

fz_matrix *
fz_rotate(fz_matrix *m, float theta)
{
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

	m->a = c; m->b = s;
	m->c = -s; m->d = c;
	m->e = 0; m->f = 0;
	return m;
}

fz_matrix *
fz_pre_rotate(fz_matrix *m, float theta)
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
		float a = m->a;
		float b = m->b;
		m->a = m->c;
		m->b = m->d;
		m->c = -a;
		m->d = -b;
	}
	else if (fabsf(180.0f - theta) < FLT_EPSILON)
	{
		m->a = -m->a;
		m->b = -m->b;
		m->c = -m->c;
		m->d = -m->d;
	}
	else if (fabsf(270.0f - theta) < FLT_EPSILON)
	{
		float a = m->a;
		float b = m->b;
		m->a = -m->c;
		m->b = -m->d;
		m->c = a;
		m->d = b;
	}
	else
	{
		float s = sinf(theta * (float)M_PI / 180);
		float c = cosf(theta * (float)M_PI / 180);
		float a = m->a;
		float b = m->b;
		m->a = c * a + s * m->c;
		m->b = c * b + s * m->d;
		m->c =-s * a + c * m->c;
		m->d =-s * b + c * m->d;
	}

	return m;
}

fz_matrix *
fz_translate(fz_matrix *m, float tx, float ty)
{
	m->a = 1; m->b = 0;
	m->c = 0; m->d = 1;
	m->e = tx; m->f = ty;
	return m;
}

fz_matrix *
fz_pre_translate(fz_matrix *mat, float tx, float ty)
{
	mat->e += tx * mat->a + ty * mat->c;
	mat->f += tx * mat->b + ty * mat->d;
	return mat;
}

fz_matrix *
fz_invert_matrix(fz_matrix *dst, const fz_matrix *src)
{
	/* Be careful to cope with dst == src */
	float a = src->a;
	float det = a * src->d - src->b * src->c;
	if (det < -FLT_EPSILON || det > FLT_EPSILON)
	{
		float rdet = 1 / det;
		dst->a = src->d * rdet;
		dst->b = -src->b * rdet;
		dst->c = -src->c * rdet;
		dst->d = a * rdet;
		a = -src->e * dst->a - src->f * dst->c;
		dst->f = -src->e * dst->b - src->f * dst->d;
		dst->e = a;
	}
	else
		*dst = *src;
	return dst;
}

int
fz_is_rectilinear(const fz_matrix *m)
{
	return (fabsf(m->b) < FLT_EPSILON && fabsf(m->c) < FLT_EPSILON) ||
		(fabsf(m->a) < FLT_EPSILON && fabsf(m->d) < FLT_EPSILON);
}

float
fz_matrix_expansion(const fz_matrix *m)
{
	return sqrtf(fabsf(m->a * m->d - m->b * m->c));
}

float
fz_matrix_max_expansion(const fz_matrix *m)
{
	float max = fabsf(m->a);
	float x = fabsf(m->b);
	if (max < x)
		max = x;
	x = fabsf(m->c);
	if (max < x)
		max = x;
	x = fabsf(m->d);
	if (max < x)
		max = x;
	return max;
}

fz_point *
fz_transform_point(fz_point *restrict p, const fz_matrix *restrict m)
{
	float x = p->x;
	p->x = x * m->a + p->y * m->c + m->e;
	p->y = x * m->b + p->y * m->d + m->f;
	return p;
}

fz_point *
fz_transform_vector(fz_point *restrict p, const fz_matrix *restrict m)
{
	float x = p->x;
	p->x = x * m->a + p->y * m->c;
	p->y = x * m->b + p->y * m->d;
	return p;
}

void
fz_normalize_vector(fz_point *p)
{
	float len = p->x * p->x + p->y * p->y;
	if (len != 0)
	{
		len = sqrtf(len);
		p->x /= len;
		p->y /= len;
	}
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

fz_irect *
fz_irect_from_rect(fz_irect *restrict b, const fz_rect *restrict r)
{
	b->x0 = fz_clamp(floorf(r->x0), MIN_SAFE_INT, MAX_SAFE_INT);
	b->y0 = fz_clamp(floorf(r->y0), MIN_SAFE_INT, MAX_SAFE_INT);
	b->x1 = fz_clamp(ceilf(r->x1), MIN_SAFE_INT, MAX_SAFE_INT);
	b->y1 = fz_clamp(ceilf(r->y1), MIN_SAFE_INT, MAX_SAFE_INT);
	return b;
}

fz_rect *
fz_rect_from_irect(fz_rect *restrict r, const fz_irect *restrict a)
{
	r->x0 = a->x0;
	r->y0 = a->y0;
	r->x1 = a->x1;
	r->y1 = a->y1;
	return r;
}

fz_irect *
fz_round_rect(fz_irect * restrict b, const fz_rect *restrict r)
{
	int i;

	i = floorf(r->x0 + 0.001);
	b->x0 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);
	i = floorf(r->y0 + 0.001);
	b->y0 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);
	i = ceilf(r->x1 - 0.001);
	b->x1 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);
	i = ceilf(r->y1 - 0.001);
	b->y1 = fz_clamp(i, MIN_SAFE_INT, MAX_SAFE_INT);

	return b;
}

fz_rect *
fz_intersect_rect(fz_rect *restrict a, const fz_rect *restrict b)
{
	/* Check for empty box before infinite box */
	if (fz_is_empty_rect(a)) return a;
	if (fz_is_empty_rect(b)) {
		*a = fz_empty_rect;
		return a;
	}
	if (fz_is_infinite_rect(b)) return a;
	if (fz_is_infinite_rect(a)) {
		*a = *b;
		return a;
	}
	if (a->x0 < b->x0)
		a->x0 = b->x0;
	if (a->y0 < b->y0)
		a->y0 = b->y0;
	if (a->x1 > b->x1)
		a->x1 = b->x1;
	if (a->y1 > b->y1)
		a->y1 = b->y1;
	if (a->x1 < a->x0 || a->y1 < a->y0)
		*a = fz_empty_rect;
	return a;
}

fz_irect *
fz_intersect_irect(fz_irect *restrict a, const fz_irect *restrict b)
{
	/* Check for empty box before infinite box */
	if (fz_is_empty_irect(a)) return a;
	if (fz_is_empty_irect(b))
	{
		*a = fz_empty_irect;
		return a;
	}
	if (fz_is_infinite_irect(b)) return a;
	if (fz_is_infinite_irect(a))
	{
		*a = *b;
		return a;
	}
	if (a->x0 < b->x0)
		a->x0 = b->x0;
	if (a->y0 < b->y0)
		a->y0 = b->y0;
	if (a->x1 > b->x1)
		a->x1 = b->x1;
	if (a->y1 > b->y1)
		a->y1 = b->y1;
	if (a->x1 < a->x0 || a->y1 < a->y0)
		*a = fz_empty_irect;
	return a;
}

fz_rect *
fz_union_rect(fz_rect *restrict a, const fz_rect *restrict b)
{
	/* Check for empty box before infinite box */
	if (fz_is_empty_rect(b)) return a;
	if (fz_is_empty_rect(a)) {
		*a = *b;
		return a;
	}
	if (fz_is_infinite_rect(a)) return a;
	if (fz_is_infinite_rect(b)) {
		*a = *b;
		return a;
	}
	if (a->x0 > b->x0)
		a->x0 = b->x0;
	if (a->y0 > b->y0)
		a->y0 = b->y0;
	if (a->x1 < b->x1)
		a->x1 = b->x1;
	if (a->y1 < b->y1)
		a->y1 = b->y1;
	return a;
}

fz_irect *
fz_translate_irect(fz_irect *a, int xoff, int yoff)
{
	int t;

	if (fz_is_empty_irect(a)) return a;
	if (fz_is_infinite_irect(a)) return a;
	a->x0 = ADD_WITH_SAT(t, a->x0, xoff);
	a->y0 = ADD_WITH_SAT(t, a->y0, yoff);
	a->x1 = ADD_WITH_SAT(t, a->x1, xoff);
	a->y1 = ADD_WITH_SAT(t, a->y1, yoff);
	return a;
}

fz_rect *
fz_transform_rect(fz_rect *restrict r, const fz_matrix *restrict m)
{
	fz_point s, t, u, v;

	if (fz_is_infinite_rect(r))
		return r;

	if (fabsf(m->b) < FLT_EPSILON && fabsf(m->c) < FLT_EPSILON)
	{
		if (m->a < 0)
		{
			float f = r->x0;
			r->x0 = r->x1;
			r->x1 = f;
		}
		if (m->d < 0)
		{
			float f = r->y0;
			r->y0 = r->y1;
			r->y1 = f;
		}
		fz_transform_point(fz_rect_min(r), m);
		fz_transform_point(fz_rect_max(r), m);
		return r;
	}

	s.x = r->x0; s.y = r->y0;
	t.x = r->x0; t.y = r->y1;
	u.x = r->x1; u.y = r->y1;
	v.x = r->x1; v.y = r->y0;
	fz_transform_point(&s, m);
	fz_transform_point(&t, m);
	fz_transform_point(&u, m);
	fz_transform_point(&v, m);
	r->x0 = MIN4(s.x, t.x, u.x, v.x);
	r->y0 = MIN4(s.y, t.y, u.y, v.y);
	r->x1 = MAX4(s.x, t.x, u.x, v.x);
	r->y1 = MAX4(s.y, t.y, u.y, v.y);
	return r;
}

fz_rect *
fz_expand_rect(fz_rect *a, float expand)
{
	if (fz_is_empty_rect(a)) return a;
	if (fz_is_infinite_rect(a)) return a;
	a->x0 -= expand;
	a->y0 -= expand;
	a->x1 += expand;
	a->y1 += expand;
	return a;
}

fz_rect *fz_include_point_in_rect(fz_rect *r, const fz_point *p)
{
	if (p->x < r->x0) r->x0 = p->x;
	if (p->x > r->x1) r->x1 = p->x;
	if (p->y < r->y0) r->y0 = p->y;
	if (p->y > r->y1) r->y1 = p->y;

	return r;
}
