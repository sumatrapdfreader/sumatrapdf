#include "fitz.h"

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
	i.x0 = floor(f.x0);
	i.y0 = floor(f.y0);
	i.x1 = ceil(f.x1);
	i.y1 = ceil(f.y1);
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
