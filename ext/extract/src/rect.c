#include "extract/extract.h"
#include "document.h"

static inline double
mind(double a, double b)
{
	return (a < b) ? a : b;
}

static inline double
maxd(double a, double b)
{
	return (a > b) ? a : b;
}

rect_t extract_rect_intersect(rect_t a, rect_t b)
{
	rect_t r;

	r.min.x = maxd(a.min.x, b.min.x);
	r.min.y = maxd(a.min.y, b.min.y);
	r.max.x = mind(a.max.x, b.max.x);
	r.max.y = mind(a.max.y, b.max.y);

	return r;
}

rect_t extract_rect_union(rect_t a, rect_t b)
{
	rect_t r;

	r.min.x = mind(a.min.x, b.min.x);
	r.min.y = mind(a.min.y, b.min.y);
	r.max.x = maxd(a.max.x, b.max.x);
	r.max.y = maxd(a.max.y, b.max.y);

	return r;
}

rect_t extract_rect_union_point(rect_t a, point_t b)
{
	rect_t r;

	r.min.x = mind(a.min.x, b.x);
	r.min.y = mind(a.min.y, b.y);
	r.max.x = maxd(a.max.x, b.x);
	r.max.y = maxd(a.max.y, b.y);

	return r;
}

int extract_rect_contains_rect(rect_t a, rect_t b)
{
	if (a.min.x > b.min.x)
		return 0;
	if (a.min.y > b.min.y)
		return 0;
	if (a.max.x < b.max.x)
		return 0;
	if (a.max.y < b.max.y)
		return 0;

	return 1;
}

int extract_rect_valid(rect_t a)
{
	return (a.min.x <= a.max.x && a.min.y <= a.max.y);
}
