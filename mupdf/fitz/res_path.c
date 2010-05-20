#include "fitz.h"

fz_path *
fz_newpath(void)
{
	fz_path *path;

	path = fz_malloc(sizeof(fz_path));
	path->len = 0;
	path->cap = 0;
	path->els = nil;

	return path;
}

fz_path *
fz_clonepath(fz_path *old)
{
	fz_path *path;

	path = fz_malloc(sizeof(fz_path));
	memcpy(path, old, sizeof(fz_path));

	path->len = old->len;
	path->cap = path->len;
	path->els = fz_malloc(path->cap * sizeof(fz_pathel));
	memcpy(path->els, old->els, sizeof(fz_pathel) * path->len);

	return path;
}

void
fz_freepath(fz_path *path)
{
	fz_free(path->els);
	fz_free(path);
}

static void
growpath(fz_path *path, int n)
{
	if (path->len + n < path->cap)
		return;
	while (path->len + n > path->cap)
		path->cap = path->cap + 36;
	path->els = fz_realloc(path->els, sizeof (fz_pathel) * path->cap);
}

void
fz_moveto(fz_path *path, float x, float y)
{
	growpath(path, 3);
	path->els[path->len++].k = FZ_MOVETO;
	path->els[path->len++].v = x;
	path->els[path->len++].v = y;
}

void
fz_lineto(fz_path *path, float x, float y)
{
	if (path->len == 0)
		fz_moveto(path, 0, 0);
	growpath(path, 3);
	path->els[path->len++].k = FZ_LINETO;
	path->els[path->len++].v = x;
	path->els[path->len++].v = y;
}

void
fz_curveto(fz_path *path,
	float x1, float y1,
	float x2, float y2,
	float x3, float y3)
{
	if (path->len == 0)
		fz_moveto(path, 0, 0);
	growpath(path, 7);
	path->els[path->len++].k = FZ_CURVETO;
	path->els[path->len++].v = x1;
	path->els[path->len++].v = y1;
	path->els[path->len++].v = x2;
	path->els[path->len++].v = y2;
	path->els[path->len++].v = x3;
	path->els[path->len++].v = y3;
}

void
fz_curvetov(fz_path *path, float x2, float y2, float x3, float y3)
{
	float x1 = path->els[path->len-2].v;
	float y1 = path->els[path->len-1].v;
	fz_curveto(path, x1, y1, x2, y2, x3, y3);
}

void
fz_curvetoy(fz_path *path, float x1, float y1, float x3, float y3)
{
	fz_curveto(path, x1, y1, x3, y3, x3, y3);
}

void
fz_closepath(fz_path *path)
{
	if (path->len == 0)
		return;
	growpath(path, 1);
	path->els[path->len++].k = FZ_CLOSEPATH;
}

static inline fz_rect boundexpand(fz_rect r, fz_point p)
{
	if (p.x < r.x0) r.x0 = p.x;
	if (p.y < r.y0) r.y0 = p.y;
	if (p.x > r.x1) r.x1 = p.x;
	if (p.y > r.y1) r.y1 = p.y;
	return r;
}

fz_rect
fz_boundpath(fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	fz_point p;
	fz_rect r = fz_emptyrect;
	int i = 0;

	if (path->len)
	{
		p.x = path->els[1].v;
		p.y = path->els[2].v;
		p = fz_transformpoint(ctm, p);
		r.x0 = r.x1 = p.x;
		r.y0 = r.y1 = p.y;
	}

	while (i < path->len)
	{
		switch (path->els[i++].k)
		{
		case FZ_CURVETO:
			p.x = path->els[i++].v;
			p.y = path->els[i++].v;
			r = boundexpand(r, fz_transformpoint(ctm, p));
			p.x = path->els[i++].v;
			p.y = path->els[i++].v;
			r = boundexpand(r, fz_transformpoint(ctm, p));
		case FZ_MOVETO:
		case FZ_LINETO:
			p.x = path->els[i++].v;
			p.y = path->els[i++].v;
			r = boundexpand(r, fz_transformpoint(ctm, p));
			break;
		case FZ_CLOSEPATH:
			break;
		}
	}

	if (stroke)
	{
		float miterlength = sin(stroke->miterlimit / 2.0);
		float linewidth = stroke->linewidth;
		float expand = MAX(miterlength, linewidth) / 2.0;
		r.x0 -= expand;
		r.y0 -= expand;
		r.x1 += expand;
		r.y1 += expand;
	}

	return r;
}

void
fz_debugpath(fz_path *path, int indent)
{
	float x, y;
	int i = 0;
	int n;
	while (i < path->len)
	{
		for (n = 0; n < indent; n++)
			putchar(' ');
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("%g %g m\n", x, y);
			break;
		case FZ_LINETO:
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("%g %g l\n", x, y);
			break;
		case FZ_CURVETO:
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("%g %g ", x, y);
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("%g %g ", x, y);
			x = path->els[i++].v;
			y = path->els[i++].v;
			printf("%g %g c\n", x, y);
			break;
		case FZ_CLOSEPATH:
			printf("h\n");
		}
	}
}

