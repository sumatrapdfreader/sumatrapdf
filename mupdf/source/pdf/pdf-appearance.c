// Copyright (C) 2004-2021 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "mupdf/fitz.h"
#include "pdf-annot-imp.h"
#include "mupdf/ucdn.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <stdio.h>

#include "annotation-icons.h"

/* #define PDF_DEBUG_APPEARANCE_SYNTHESIS */

#define REPLACEMENT 0xB7
#define CIRCLE_MAGIC 0.551915f

static fz_point rect_center(const fz_rect rect)
{
	fz_point c;
	c.x = (rect.x0 + rect.x1) / 2.0f;
	c.y = (rect.y0 + rect.y1) / 2.0f;
	return c;
}

static fz_matrix center_rect_within_rect(const fz_rect tofit, const fz_rect within)
{
	float xscale = (within.x1 - within.x0) / (tofit.x1 - tofit.x0);
	float yscale = (within.y1 - within.y0) / (tofit.y1 - tofit.y0);
	float scale = fz_min(xscale, yscale);
	fz_point tofit_center;
	fz_point within_center;

	within_center = rect_center(within);
	tofit_center = rect_center(tofit);

	/* Translate "tofit" to be centered on the origin
	 * Scale "tofit" to a size that fits within "within"
	 * Translate "tofit" to "within's" center
	 * Do all the above in reverse order so that we can use the fz_pre_xx functions */
	return fz_pre_translate(fz_pre_scale(fz_translate(within_center.x, within_center.y), scale, -scale), -tofit_center.x, -tofit_center.y);
}

static void
pdf_write_opacity_blend_mode(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, pdf_obj **res, int bm)
{
	pdf_obj *res_egs, *res_egs_h;
	float opacity = pdf_annot_opacity(ctx, annot);

	if (bm == FZ_BLEND_NORMAL && opacity == 1)
		return;

	/* /Resources << /ExtGState << /H << /Type/ExtGState /BM/Multiply /CA %g /ca %g >> >> >> */

	if (!*res)
		*res = pdf_new_dict(ctx, annot->page->doc, 1);

	res_egs = pdf_dict_put_dict(ctx, *res, PDF_NAME(ExtGState), 1);
	res_egs_h = pdf_dict_put_dict(ctx, res_egs, PDF_NAME(H), 2);
	pdf_dict_put(ctx, res_egs_h, PDF_NAME(Type), PDF_NAME(ExtGState));

	if (bm == FZ_BLEND_MULTIPLY)
	{
		pdf_dict_put(ctx, res_egs_h, PDF_NAME(BM), PDF_NAME(Multiply));
	}

	if (opacity < 1)
	{
		pdf_dict_put_real(ctx, res_egs_h, PDF_NAME(CA), opacity);
		pdf_dict_put_real(ctx, res_egs_h, PDF_NAME(ca), opacity);
	}

	fz_append_printf(ctx, buf, "/H gs\n");
}

static void
pdf_write_opacity(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, pdf_obj **res)
{
	pdf_write_opacity_blend_mode(ctx, annot, buf, res, FZ_BLEND_NORMAL);
}

static float pdf_write_border_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf)
{
	float w = pdf_annot_border(ctx, annot);
	fz_append_printf(ctx, buf, "%g w\n", w);
	return w;
}

static int pdf_write_stroke_color_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf)
{
	float color[4];
	int n;
	pdf_annot_color(ctx, annot, &n, color);
	switch (n)
	{
	default: return 0;
	case 1: fz_append_printf(ctx, buf, "%g G\n", color[0]); break;
	case 3: fz_append_printf(ctx, buf, "%g %g %g RG\n", color[0], color[1], color[2]); break;
	case 4: fz_append_printf(ctx, buf, "%g %g %g %g K\n", color[0], color[1], color[2], color[3]); break;
	}
	return 1;
}

static int pdf_is_dark_fill_color(fz_context *ctx, pdf_annot *annot)
{
	float color[4], gray;
	int n;
	pdf_annot_color(ctx, annot, &n, color);
	switch (n)
	{
	default:
		gray = 1;
		break;
	case 1:
		gray = color[0];
		break;
	case 3:
		gray = color[0] * 0.3f + color[1] * 0.59f + color[2] * 0.11f;
		break;
	case 4:
		gray = color[0] * 0.3f + color[1] * 0.59f + color[2] * 0.11f + color[3];
		gray = 1 - fz_min(gray, 1);
		break;
	}
	return gray < 0.25f;
}

static int pdf_write_fill_color_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf)
{
	float color[4];
	int n;
	pdf_annot_color(ctx, annot, &n, color);
	switch (n)
	{
	default: return 0;
	case 1: fz_append_printf(ctx, buf, "%g g\n", color[0]); break;
	case 3: fz_append_printf(ctx, buf, "%g %g %g rg\n", color[0], color[1], color[2]); break;
	case 4: fz_append_printf(ctx, buf, "%g %g %g %g k\n", color[0], color[1], color[2], color[3]); break;
	}
	return 1;
}

static int pdf_write_interior_fill_color_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf)
{
	float color[4];
	int n;
	pdf_annot_interior_color(ctx, annot, &n, color);
	switch (n)
	{
	default: return 0;
	case 1: fz_append_printf(ctx, buf, "%g g\n", color[0]); break;
	case 3: fz_append_printf(ctx, buf, "%g %g %g rg\n", color[0], color[1], color[2]); break;
	case 4: fz_append_printf(ctx, buf, "%g %g %g %g k\n", color[0], color[1], color[2], color[3]); break;
	}
	return 1;
}

static int pdf_write_MK_BG_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf)
{
	float color[4];
	int n;
	pdf_annot_MK_BG(ctx, annot, &n, color);
	switch (n)
	{
	default: return 0;
	case 1: fz_append_printf(ctx, buf, "%g g\n", color[0]); break;
	case 3: fz_append_printf(ctx, buf, "%g %g %g rg\n", color[0], color[1], color[2]); break;
	case 4: fz_append_printf(ctx, buf, "%g %g %g %g k\n", color[0], color[1], color[2], color[3]); break;
	}
	return 1;
}

static int pdf_write_MK_BC_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf)
{
	float color[4];
	int n;
	pdf_annot_MK_BC(ctx, annot, &n, color);
	switch (n)
	{
	default: return 0;
	case 1: fz_append_printf(ctx, buf, "%g G\n", color[0]); break;
	case 3: fz_append_printf(ctx, buf, "%g %g %g RG\n", color[0], color[1], color[2]); break;
	case 4: fz_append_printf(ctx, buf, "%g %g %g %g K\n", color[0], color[1], color[2], color[3]); break;
	}
	return 1;
}

static void maybe_stroke_and_fill(fz_context *ctx, fz_buffer *buf, int sc, int ic)
{
	if (sc)
		fz_append_string(ctx, buf, ic ? "b\n" : "s\n");
	else
		fz_append_string(ctx, buf, ic ? "f\n" : "n\n");
}

static void maybe_stroke(fz_context *ctx, fz_buffer *buf, int sc)
{
	fz_append_string(ctx, buf, sc ? "S\n" : "n\n");
}

static fz_point rotate_vector(float angle, float x, float y)
{
	float ca = cosf(angle);
	float sa = sinf(angle);
	return fz_make_point(x*ca - y*sa, x*sa + y*ca);
}

static void pdf_write_arrow_appearance(fz_context *ctx, fz_buffer *buf, fz_rect *rect, float x, float y, float dx, float dy, float w)
{
	float r = fz_max(1, w);
	float angle = atan2f(dy, dx);
	fz_point v, a, b;

	v = rotate_vector(angle, 8.8f*r, 4.5f*r);
	a = fz_make_point(x + v.x, y + v.y);
	v = rotate_vector(angle, 8.8f*r, -4.5f*r);
	b = fz_make_point(x + v.x, y + v.y);

	*rect = fz_include_point_in_rect(*rect, a);
	*rect = fz_include_point_in_rect(*rect, b);
	*rect = fz_expand_rect(*rect, w);

	fz_append_printf(ctx, buf, "%g %g m\n", a.x, a.y);
	fz_append_printf(ctx, buf, "%g %g l\n", x, y);
	fz_append_printf(ctx, buf, "%g %g l\n", b.x, b.y);
}

static void include_cap(fz_rect *rect, float x, float y, float r)
{
	rect->x0 = fz_min(rect->x0, x-r);
	rect->y0 = fz_min(rect->y0, y-r);
	rect->x1 = fz_max(rect->x1, x+r);
	rect->y1 = fz_max(rect->y1, y+r);
}

static void
pdf_write_line_cap_appearance(fz_context *ctx, fz_buffer *buf, fz_rect *rect,
		float x, float y, float dx, float dy, float w,
		int sc, int ic, pdf_obj *cap)
{
	if (cap == PDF_NAME(Square))
	{
		float r = fz_max(2.5f, w * 2.5f);
		fz_append_printf(ctx, buf, "%g %g %g %g re\n", x-r, y-r, r*2, r*2);
		maybe_stroke_and_fill(ctx, buf, sc, ic);
		include_cap(rect, x, y, r);
	}
	else if (cap == PDF_NAME(Circle))
	{
		float r = fz_max(2.5f, w * 2.5f);
		float m = r * CIRCLE_MAGIC;
		fz_append_printf(ctx, buf, "%g %g m\n", x, y+r);
		fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", x+m, y+r, x+r, y+m, x+r, y);
		fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", x+r, y-m, x+m, y-r, x, y-r);
		fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", x-m, y-r, x-r, y-m, x-r, y);
		fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", x-r, y+m, x-m, y+r, x, y+r);
		maybe_stroke_and_fill(ctx, buf, sc, ic);
		include_cap(rect, x, y, r);
	}
	else if (cap == PDF_NAME(Diamond))
	{
		float r = fz_max(2.5f, w * 2.5f);
		fz_append_printf(ctx, buf, "%g %g m\n", x, y+r);
		fz_append_printf(ctx, buf, "%g %g l\n", x+r, y);
		fz_append_printf(ctx, buf, "%g %g l\n", x, y-r);
		fz_append_printf(ctx, buf, "%g %g l\n", x-r, y);
		maybe_stroke_and_fill(ctx, buf, sc, ic);
		include_cap(rect, x, y, r);
	}
	else if (cap == PDF_NAME(OpenArrow))
	{
		pdf_write_arrow_appearance(ctx, buf, rect, x, y, dx, dy, w);
		maybe_stroke(ctx, buf, sc);
	}
	else if (cap == PDF_NAME(ClosedArrow))
	{
		pdf_write_arrow_appearance(ctx, buf, rect, x, y, dx, dy, w);
		maybe_stroke_and_fill(ctx, buf, sc, ic);
	}
	/* PDF 1.5 */
	else if (cap == PDF_NAME(Butt))
	{
		float r = fz_max(3, w * 3);
		fz_point a = { x-dy*r, y+dx*r };
		fz_point b = { x+dy*r, y-dx*r };
		fz_append_printf(ctx, buf, "%g %g m\n", a.x, a.y);
		fz_append_printf(ctx, buf, "%g %g l\n", b.x, b.y);
		maybe_stroke(ctx, buf, sc);
		*rect = fz_include_point_in_rect(*rect, a);
		*rect = fz_include_point_in_rect(*rect, b);
	}
	else if (cap == PDF_NAME(ROpenArrow))
	{
		pdf_write_arrow_appearance(ctx, buf, rect, x, y, -dx, -dy, w);
		maybe_stroke(ctx, buf, sc);
	}
	else if (cap == PDF_NAME(RClosedArrow))
	{
		pdf_write_arrow_appearance(ctx, buf, rect, x, y, -dx, -dy, w);
		maybe_stroke_and_fill(ctx, buf, sc, ic);
	}
	/* PDF 1.6 */
	else if (cap == PDF_NAME(Slash))
	{
		float r = fz_max(5, w * 5);
		float angle = atan2f(dy, dx) - (30 * FZ_PI / 180);
		fz_point a, b, v;
		v = rotate_vector(angle, 0, r);
		a = fz_make_point(x + v.x, y + v.y);
		v = rotate_vector(angle, 0, -r);
		b = fz_make_point(x + v.x, y + v.y);
		fz_append_printf(ctx, buf, "%g %g m\n", a.x, a.y);
		fz_append_printf(ctx, buf, "%g %g l\n", b.x, b.y);
		maybe_stroke(ctx, buf, sc);
		*rect = fz_include_point_in_rect(*rect, a);
		*rect = fz_include_point_in_rect(*rect, b);
	}
}

static void
pdf_write_line_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	pdf_obj *line, *le;
	fz_point a, b;
	float w;
	int sc;
	int ic;

	pdf_write_opacity(ctx, annot, buf, res);
	w = pdf_write_border_appearance(ctx, annot, buf);
	sc = pdf_write_stroke_color_appearance(ctx, annot, buf);
	ic = pdf_write_interior_fill_color_appearance(ctx, annot, buf);

	line = pdf_dict_get(ctx, annot->obj, PDF_NAME(L));
	a.x = pdf_array_get_real(ctx, line, 0);
	a.y = pdf_array_get_real(ctx, line, 1);
	b.x = pdf_array_get_real(ctx, line, 2);
	b.y = pdf_array_get_real(ctx, line, 3);

	fz_append_printf(ctx, buf, "%g %g m\n%g %g l\n", a.x, a.y, b.x, b.y);
	maybe_stroke(ctx, buf, sc);

	rect->x0 = fz_min(a.x, b.x);
	rect->y0 = fz_min(a.y, b.y);
	rect->x1 = fz_max(a.x, b.x);
	rect->y1 = fz_max(a.y, b.y);

	le = pdf_dict_get(ctx, annot->obj, PDF_NAME(LE));
	if (pdf_array_len(ctx, le) == 2)
	{
		float dx = b.x - a.x;
		float dy = b.y - a.y;
		float l = sqrtf(dx*dx + dy*dy);
		pdf_write_line_cap_appearance(ctx, buf, rect, a.x, a.y, dx/l, dy/l, w, sc, ic, pdf_array_get(ctx, le, 0));
		pdf_write_line_cap_appearance(ctx, buf, rect, b.x, b.y, -dx/l, -dy/l, w, sc, ic, pdf_array_get(ctx, le, 1));
	}
	*rect = fz_expand_rect(*rect, fz_max(1, w));
}

static void
pdf_write_square_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	float x, y, w, h;
	float lw;
	int sc;
	int ic;

	pdf_write_opacity(ctx, annot, buf, res);
	lw = pdf_write_border_appearance(ctx, annot, buf);
	sc = pdf_write_stroke_color_appearance(ctx, annot, buf);
	ic = pdf_write_interior_fill_color_appearance(ctx, annot, buf);

	x = rect->x0 + lw;
	y = rect->y0 + lw;
	w = rect->x1 - x - lw;
	h = rect->y1 - y - lw;

	fz_append_printf(ctx, buf, "%g %g %g %g re\n", x, y, w, h);
	maybe_stroke_and_fill(ctx, buf, sc, ic);
}

static void
draw_circle(fz_context *ctx, fz_buffer *buf, float rx, float ry, float cx, float cy)
{
	float mx = rx * CIRCLE_MAGIC;
	float my = ry * CIRCLE_MAGIC;
	fz_append_printf(ctx, buf, "%g %g m\n", cx, cy+ry);
	fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", cx+mx, cy+ry, cx+rx, cy+my, cx+rx, cy);
	fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", cx+rx, cy-my, cx+mx, cy-ry, cx, cy-ry);
	fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", cx-mx, cy-ry, cx-rx, cy-my, cx-rx, cy);
	fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", cx-rx, cy+my, cx-mx, cy+ry, cx, cy+ry);
}

static void
draw_circle_in_box(fz_context *ctx, fz_buffer *buf, float lw, float x0, float y0, float x1, float y1)
{
	float rx = (x1 - x0) / 2 - lw;
	float ry = (y1 - y0) / 2 - lw;
	float cx = x0 + lw + rx;
	float cy = y0 + lw + ry;
	draw_circle(ctx, buf, rx, ry, cx, cy);
}

static void
pdf_write_circle_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	float lw;
	int sc;
	int ic;

	pdf_write_opacity(ctx, annot, buf, res);
	lw = pdf_write_border_appearance(ctx, annot, buf);
	sc = pdf_write_stroke_color_appearance(ctx, annot, buf);
	ic = pdf_write_interior_fill_color_appearance(ctx, annot, buf);

	draw_circle_in_box(ctx, buf, lw, rect->x0, rect->y0, rect->x1, rect->y1);
	maybe_stroke_and_fill(ctx, buf, sc, ic);
}

static void
pdf_write_polygon_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res, int close)
{
	pdf_obj *verts;
	fz_point p;
	int i, n;
	float lw;
	int sc;

	pdf_write_opacity(ctx, annot, buf, res);
	lw = pdf_write_border_appearance(ctx, annot, buf);
	sc = pdf_write_stroke_color_appearance(ctx, annot, buf);

	*rect = fz_empty_rect;

	verts = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
	n = pdf_array_len(ctx, verts) / 2;
	if (n > 0)
	{
		for (i = 0; i < n; ++i)
		{
			p.x = pdf_array_get_real(ctx, verts, i*2+0);
			p.y = pdf_array_get_real(ctx, verts, i*2+1);
			if (i == 0)
			{
				rect->x0 = rect->x1 = p.x;
				rect->y0 = rect->y1 = p.y;
			}
			else
				*rect = fz_include_point_in_rect(*rect, p);
			if (i == 0)
				fz_append_printf(ctx, buf, "%g %g m\n", p.x, p.y);
			else
				fz_append_printf(ctx, buf, "%g %g l\n", p.x, p.y);
		}
		if (close)
			fz_append_string(ctx, buf, "h\n");
		maybe_stroke(ctx, buf, sc);
		*rect = fz_expand_rect(*rect, lw);
	}
}

static void
pdf_write_ink_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	pdf_obj *ink_list, *stroke;
	int i, n, k, m;
	float lw;
	fz_point p;
	int sc;

	pdf_write_opacity(ctx, annot, buf, res);
	lw = pdf_write_border_appearance(ctx, annot, buf);
	sc = pdf_write_stroke_color_appearance(ctx, annot, buf);

	*rect = fz_empty_rect;

	fz_append_printf(ctx, buf, "1 J\n1 j\n");

	ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
	n = pdf_array_len(ctx, ink_list);
	for (i = 0; i < n; ++i)
	{
		stroke = pdf_array_get(ctx, ink_list, i);
		m = pdf_array_len(ctx, stroke) / 2;
		for (k = 0; k < m; ++k)
		{
			p.x = pdf_array_get_real(ctx, stroke, k*2+0);
			p.y = pdf_array_get_real(ctx, stroke, k*2+1);
			if (i == 0 && k == 0)
			{
				rect->x0 = rect->x1 = p.x;
				rect->y0 = rect->y1 = p.y;
			}
			else
				*rect = fz_include_point_in_rect(*rect, p);
			fz_append_printf(ctx, buf, "%g %g %c\n", p.x, p.y, k == 0 ? 'm' : 'l');
		}

		if (m == 1)
			fz_append_printf(ctx, buf, "%g %g %c\n", p.x, p.y, 'l');
	}
	maybe_stroke(ctx, buf, sc);
	*rect = fz_expand_rect(*rect, lw);
}

/* Contrary to the specification, the points within a QuadPoint are NOT
 * ordered in a counter-clockwise fashion starting with the lower left.
 * Experiments with Adobe's implementation indicates a cross-wise
 * ordering is intended: ul, ur, ll, lr.
 */
enum { UL, UR, LL, LR };

static float
extract_quad(fz_context *ctx, fz_point *quad, pdf_obj *obj, int i)
{
	float dx, dy;
	quad[0].x = pdf_array_get_real(ctx, obj, i+0);
	quad[0].y = pdf_array_get_real(ctx, obj, i+1);
	quad[1].x = pdf_array_get_real(ctx, obj, i+2);
	quad[1].y = pdf_array_get_real(ctx, obj, i+3);
	quad[2].x = pdf_array_get_real(ctx, obj, i+4);
	quad[2].y = pdf_array_get_real(ctx, obj, i+5);
	quad[3].x = pdf_array_get_real(ctx, obj, i+6);
	quad[3].y = pdf_array_get_real(ctx, obj, i+7);
	dx = quad[UL].x - quad[LL].x;
	dy = quad[UL].y - quad[LL].y;
	return sqrtf(dx * dx + dy * dy);
}

static void
union_quad(fz_rect *rect, const fz_point quad[4], float lw)
{
	fz_rect qbox;
	qbox.x0 = fz_min(fz_min(quad[0].x, quad[1].x), fz_min(quad[2].x, quad[3].x));
	qbox.y0 = fz_min(fz_min(quad[0].y, quad[1].y), fz_min(quad[2].y, quad[3].y));
	qbox.x1 = fz_max(fz_max(quad[0].x, quad[1].x), fz_max(quad[2].x, quad[3].x));
	qbox.y1 = fz_max(fz_max(quad[0].y, quad[1].y), fz_max(quad[2].y, quad[3].y));
	*rect = fz_union_rect(*rect, fz_expand_rect(qbox, lw));
}

static fz_point
lerp_point(fz_point a, fz_point b, float t)
{
	return fz_make_point(a.x + t * (b.x - a.x), a.y + t * (b.y - a.y));
}

static void
pdf_write_highlight_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	pdf_obj *qp;
	fz_point quad[4], mquad[4], v;
	float h, m, dx, dy, vn;
	int i, n;

	*rect = fz_empty_rect;

	pdf_write_opacity_blend_mode(ctx, annot, buf, res, FZ_BLEND_MULTIPLY);
	pdf_write_fill_color_appearance(ctx, annot, buf);

	qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	n = pdf_array_len(ctx, qp);
	if (n > 0)
	{
		for (i = 0; i < n; i += 8)
		{
			h = extract_quad(ctx, quad, qp, i);
			m = h / 4.2425f; /* magic number that matches adobe's appearance */
			dx = quad[LR].x - quad[LL].x;
			dy = quad[LR].y - quad[LL].y;
			vn = sqrtf(dx * dx + dy * dy);
			v = fz_make_point(dx * m / vn, dy * m / vn);

			mquad[LL].x = quad[LL].x - v.x - v.y;
			mquad[LL].y = quad[LL].y - v.y + v.x;
			mquad[UL].x = quad[UL].x - v.x + v.y;
			mquad[UL].y = quad[UL].y - v.y - v.x;
			mquad[LR].x = quad[LR].x + v.x - v.y;
			mquad[LR].y = quad[LR].y + v.y + v.x;
			mquad[UR].x = quad[UR].x + v.x + v.y;
			mquad[UR].y = quad[UR].y + v.y - v.x;

			fz_append_printf(ctx, buf, "%g %g m\n", quad[LL].x, quad[LL].y);
			fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n",
				mquad[LL].x, mquad[LL].y,
				mquad[UL].x, mquad[UL].y,
				quad[UL].x, quad[UL].y);
			fz_append_printf(ctx, buf, "%g %g l\n", quad[UR].x, quad[UR].y);
			fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n",
				mquad[UR].x, mquad[UR].y,
				mquad[LR].x, mquad[LR].y,
				quad[LR].x, quad[LR].y);
			fz_append_printf(ctx, buf, "f\n");

			union_quad(rect, quad, h/16);
			union_quad(rect, mquad, 0);
		}
	}
}

static void
pdf_write_underline_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	fz_point quad[4], a, b;
	float h;
	pdf_obj *qp;
	int i, n;

	*rect = fz_empty_rect;

	pdf_write_opacity(ctx, annot, buf, res);
	pdf_write_stroke_color_appearance(ctx, annot, buf);

	qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	n = pdf_array_len(ctx, qp);
	if (n > 0)
	{
		for (i = 0; i < n; i += 8)
		{
			/* Acrobat draws the line at 1/7 of the box width from the bottom
			 * of the box and 1/16 thick of the box width. */

			h = extract_quad(ctx, quad, qp, i);
			a = lerp_point(quad[LL], quad[UL], 1/7.0f);
			b = lerp_point(quad[LR], quad[UR], 1/7.0f);

			fz_append_printf(ctx, buf, "%g w\n", h/16);
			fz_append_printf(ctx, buf, "%g %g m\n", a.x, a.y);
			fz_append_printf(ctx, buf, "%g %g l\n", b.x, b.y);
			fz_append_printf(ctx, buf, "S\n");

			union_quad(rect, quad, h/16);
		}
	}
}

static void
pdf_write_strike_out_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	fz_point quad[4], a, b;
	float h;
	pdf_obj *qp;
	int i, n;

	pdf_write_opacity(ctx, annot, buf, res);
	pdf_write_stroke_color_appearance(ctx, annot, buf);

	qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	n = pdf_array_len(ctx, qp);
	if (n > 0)
	{
		*rect = fz_empty_rect;
		for (i = 0; i < n; i += 8)
		{
			/* Acrobat draws the line at 3/7 of the box width from the bottom
			 * of the box and 1/16 thick of the box width. */

			h = extract_quad(ctx, quad, qp, i);
			a = lerp_point(quad[LL], quad[UL], 3/7.0f);
			b = lerp_point(quad[LR], quad[UR], 3/7.0f);

			fz_append_printf(ctx, buf, "%g w\n", h/16);
			fz_append_printf(ctx, buf, "%g %g m\n", a.x, a.y);
			fz_append_printf(ctx, buf, "%g %g l\n", b.x, b.y);
			fz_append_printf(ctx, buf, "S\n");

			union_quad(rect, quad, h/16);
		}
	}
}

static void
pdf_write_squiggly_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	fz_point quad[4], a, b, c, v;
	float h, x, w;
	pdf_obj *qp;
	int i, n;

	*rect = fz_empty_rect;

	pdf_write_opacity(ctx, annot, buf, res);
	pdf_write_stroke_color_appearance(ctx, annot, buf);

	qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	n = pdf_array_len(ctx, qp);
	if (n > 0)
	{
		for (i = 0; i < n; i += 8)
		{
			int up = 1;
			h = extract_quad(ctx, quad, qp, i);
			v = fz_make_point(quad[LR].x - quad[LL].x, quad[LR].y - quad[LL].y);
			w = sqrtf(v.x * v.x + v.y * v.y);
			x = 0;

			fz_append_printf(ctx, buf, "%g w\n", h/16);
			fz_append_printf(ctx, buf, "%g %g m\n", quad[LL].x, quad[LL].y);
			while (x < w)
			{
				x += h/7;
				a = lerp_point(quad[LL], quad[LR], x/w);
				if (up)
				{
					b = lerp_point(quad[UL], quad[UR], x/w);
					c = lerp_point(a, b, 1/7.0f);
					fz_append_printf(ctx, buf, "%g %g l\n", c.x, c.y);
				}
				else
					fz_append_printf(ctx, buf, "%g %g l\n", a.x, a.y);
				up = !up;
			}
			fz_append_printf(ctx, buf, "S\n");

			union_quad(rect, quad, h/16);
		}
	}
}

static void
pdf_write_redact_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, pdf_obj **res)
{
	fz_point quad[4];
	pdf_obj *qp;
	int i, n;

	pdf_write_opacity(ctx, annot, buf, res);

	fz_append_printf(ctx, buf, "1 0 0 RG\n");

	qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	n = pdf_array_len(ctx, qp);
	if (n > 0)
	{
		*rect = fz_empty_rect;
		for (i = 0; i < n; i += 8)
		{
			extract_quad(ctx, quad, qp, i);
			fz_append_printf(ctx, buf, "%g %g m\n", quad[LL].x, quad[LL].y);
			fz_append_printf(ctx, buf, "%g %g l\n", quad[LR].x, quad[LR].y);
			fz_append_printf(ctx, buf, "%g %g l\n", quad[UR].x, quad[UR].y);
			fz_append_printf(ctx, buf, "%g %g l\n", quad[UL].x, quad[UL].y);
			fz_append_printf(ctx, buf, "s\n");
			union_quad(rect, quad, 1);
		}
	}
	else
	{
		fz_append_printf(ctx, buf, "%g %g m\n", rect->x0+1, rect->y0+1);
		fz_append_printf(ctx, buf, "%g %g l\n", rect->x1-1, rect->y0+1);
		fz_append_printf(ctx, buf, "%g %g l\n", rect->x1-1, rect->y1-1);
		fz_append_printf(ctx, buf, "%g %g l\n", rect->x0+1, rect->y1-1);
		fz_append_printf(ctx, buf, "s\n");
	}
}

static void
pdf_write_caret_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, fz_rect *bbox, pdf_obj **res)
{
	float xc = (rect->x0 + rect->x1) / 2;
	float yc = (rect->y0 + rect->y1) / 2;

	pdf_write_opacity(ctx, annot, buf, res);
	pdf_write_fill_color_appearance(ctx, annot, buf);

	fz_append_string(ctx, buf, "0 0 m\n");
	fz_append_string(ctx, buf, "10 0 10 7 10 14 c\n");
	fz_append_string(ctx, buf, "10 7 10 0 20 0 c\n");
	fz_append_string(ctx, buf, "f\n");

	*rect = fz_make_rect(xc - 10, yc - 7, xc + 10, yc + 7);
	*bbox = fz_make_rect(0, 0, 20, 14);
}

static void
pdf_write_icon_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, fz_rect *bbox, pdf_obj **res)
{
	const char *name;
	float xc = (rect->x0 + rect->x1) / 2;
	float yc = (rect->y0 + rect->y1) / 2;

	pdf_write_opacity(ctx, annot, buf, res);

	if (!pdf_write_fill_color_appearance(ctx, annot, buf))
		fz_append_string(ctx, buf, "1 g\n");

	fz_append_string(ctx, buf, "1 w\n0.5 0.5 15 15 re\nb\n");
	fz_append_string(ctx, buf, "1 0 0 -1 4 12 cm\n");

	if (pdf_is_dark_fill_color(ctx, annot))
		fz_append_string(ctx, buf, "1 g\n");
	else
		fz_append_string(ctx, buf, "0 g\n");

	name = pdf_annot_icon_name(ctx, annot);

	/* Text names */
	if (!strcmp(name, "Comment"))
		fz_append_string(ctx, buf, icon_comment);
	else if (!strcmp(name, "Key"))
		fz_append_string(ctx, buf, icon_key);
	else if (!strcmp(name, "Note"))
		fz_append_string(ctx, buf, icon_note);
	else if (!strcmp(name, "Help"))
		fz_append_string(ctx, buf, icon_help);
	else if (!strcmp(name, "NewParagraph"))
		fz_append_string(ctx, buf, icon_new_paragraph);
	else if (!strcmp(name, "Paragraph"))
		fz_append_string(ctx, buf, icon_paragraph);
	else if (!strcmp(name, "Insert"))
		fz_append_string(ctx, buf, icon_insert);

	/* FileAttachment names */
	else if (!strcmp(name, "Graph"))
		fz_append_string(ctx, buf, icon_graph);
	else if (!strcmp(name, "PushPin"))
		fz_append_string(ctx, buf, icon_push_pin);
	else if (!strcmp(name, "Paperclip"))
		fz_append_string(ctx, buf, icon_paperclip);
	else if (!strcmp(name, "Tag"))
		fz_append_string(ctx, buf, icon_tag);

	/* Sound names */
	else if (!strcmp(name, "Speaker"))
		fz_append_string(ctx, buf, icon_speaker);
	else if (!strcmp(name, "Mic"))
		fz_append_string(ctx, buf, icon_mic);

	/* Unknown */
	else
		fz_append_string(ctx, buf, icon_star);

	*rect = fz_make_rect(xc - 9, yc - 9, xc + 9, yc + 9);
	*bbox = fz_make_rect(0, 0, 16, 16);
}

static float
measure_stamp_string(fz_context *ctx, fz_font *font, const char *text)
{
	float w = 0;
	while (*text)
	{
		int c, g;
		text += fz_chartorune(&c, text);
		if (fz_windows_1252_from_unicode(c) < 0)
			c = REPLACEMENT;
		g = fz_encode_character(ctx, font, c);
		w += fz_advance_glyph(ctx, font, g, 0);
	}
	return w;
}

static void
write_stamp_string(fz_context *ctx, fz_buffer *buf, fz_font *font, const char *text)
{
	fz_append_byte(ctx, buf, '(');
	while (*text)
	{
		int c;
		text += fz_chartorune(&c, text);
		c = fz_windows_1252_from_unicode(c);
		if (c < 0) c = REPLACEMENT;
		if (c == '(' || c == ')' || c == '\\')
			fz_append_byte(ctx, buf, '\\');
		fz_append_byte(ctx, buf, c);
	}
	fz_append_byte(ctx, buf, ')');
}

static void
write_stamp(fz_context *ctx, fz_buffer *buf, fz_font *font, const char *text, float y, float h)
{
	float tw = measure_stamp_string(ctx, font, text) * h;
	fz_append_string(ctx, buf, "BT\n");
	fz_append_printf(ctx, buf, "/Times %g Tf\n", h);
	fz_append_printf(ctx, buf, "%g %g Td\n", (190-tw)/2, y);
	write_stamp_string(ctx, buf, font, text);
	fz_append_string(ctx, buf, " Tj\n");
	fz_append_string(ctx, buf, "ET\n");
}

static void
pdf_write_stamp_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, fz_rect *rect, fz_rect *bbox, pdf_obj **res)
{
	fz_font *font;
	pdf_obj *res_font;
	pdf_obj *name;
	float w, h, xs, ys;
	fz_matrix rotate;

	name = pdf_dict_get(ctx, annot->obj, PDF_NAME(Name));
	if (!name)
		name = PDF_NAME(Draft);

	h = rect->y1 - rect->y0;
	w = rect->x1 - rect->x0;
	xs = w / 190;
	ys = h / 50;

	font = fz_new_base14_font(ctx, "Times-Bold");
	fz_try(ctx)
	{
		/* /Resources << /Font << /Times %d 0 R >> >> */
		if (!*res)
			*res = pdf_new_dict(ctx, annot->page->doc, 1);
		res_font = pdf_dict_put_dict(ctx, *res, PDF_NAME(Font), 1);
		pdf_dict_put_drop(ctx, res_font, PDF_NAME(Times), pdf_add_simple_font(ctx, annot->page->doc, font, 0));

		pdf_write_opacity(ctx, annot, buf, res);
		pdf_write_fill_color_appearance(ctx, annot, buf);
		pdf_write_stroke_color_appearance(ctx, annot, buf);
		rotate = fz_rotate(0.6f);
		fz_append_printf(ctx, buf, "%M cm\n", &rotate);
		fz_append_string(ctx, buf, "2 w\n2 2 186 44 re\nS\n");

		if (name == PDF_NAME(Approved))
			write_stamp(ctx, buf, font, "APPROVED", 13, 30);
		else if (name == PDF_NAME(AsIs))
			write_stamp(ctx, buf, font, "AS IS", 13, 30);
		else if (name == PDF_NAME(Confidential))
			write_stamp(ctx, buf, font, "CONFIDENTIAL", 17, 20);
		else if (name == PDF_NAME(Departmental))
			write_stamp(ctx, buf, font, "DEPARTMENTAL", 17, 20);
		else if (name == PDF_NAME(Experimental))
			write_stamp(ctx, buf, font, "EXPERIMENTAL", 17, 20);
		else if (name == PDF_NAME(Expired))
			write_stamp(ctx, buf, font, "EXPIRED", 13, 30);
		else if (name == PDF_NAME(Final))
			write_stamp(ctx, buf, font, "FINAL", 13, 30);
		else if (name == PDF_NAME(ForComment))
			write_stamp(ctx, buf, font, "FOR COMMENT", 17, 20);
		else if (name == PDF_NAME(ForPublicRelease))
		{
			write_stamp(ctx, buf, font, "FOR PUBLIC", 26, 18);
			write_stamp(ctx, buf, font, "RELEASE", 8.5f, 18);
		}
		else if (name == PDF_NAME(NotApproved))
			write_stamp(ctx, buf, font, "NOT APPROVED", 17, 20);
		else if (name == PDF_NAME(NotForPublicRelease))
		{
			write_stamp(ctx, buf, font, "NOT FOR", 26, 18);
			write_stamp(ctx, buf, font, "PUBLIC RELEASE", 8.5, 18);
		}
		else if (name == PDF_NAME(Sold))
			write_stamp(ctx, buf, font, "SOLD", 13, 30);
		else if (name == PDF_NAME(TopSecret))
			write_stamp(ctx, buf, font, "TOP SECRET", 14, 26);
		else if (name == PDF_NAME(Draft))
			write_stamp(ctx, buf, font, "DRAFT", 13, 30);
		else
			write_stamp(ctx, buf, font, pdf_to_name(ctx, name), 17, 20);
	}
	fz_always(ctx)
		fz_drop_font(ctx, font);
	fz_catch(ctx)
		fz_rethrow(ctx);

	*bbox = fz_make_rect(0, 0, 190, 50);
	if (xs > ys)
	{
		float xc = (rect->x1+rect->x0) / 2;
		rect->x0 = xc - 95 * ys;
		rect->x1 = xc + 95 * ys;
	}
	else
	{
		float yc = (rect->y1+rect->y0) / 2;
		rect->y0 = yc - 25 * xs;
		rect->y1 = yc + 25 * xs;
	}
}

static void
add_required_fonts(fz_context *ctx, pdf_document *doc, pdf_obj *res_font,
	fz_text_language lang, fz_font *font, const char *fontname, const char *text)
{
	fz_font *cjk_font;
	char buf[40];

	int add_latin = 0;
	int add_greek = 0;
	int add_cyrillic = 0;
	int add_korean = 0;
	int add_japanese = 0;
	int add_bopomofo = 0;
	int add_han = 0;
	int add_hans = 0;
	int add_hant = 0;

	while (*text)
	{
		int c;
		text += fz_chartorune(&c, text);
		switch (ucdn_get_script(c))
		{
		default: add_latin = 1; /* for fallback bullet character */ break;
		case UCDN_SCRIPT_COMMON: break;
		case UCDN_SCRIPT_INHERITED: break;
		case UCDN_SCRIPT_LATIN: add_latin = 1; break;
		case UCDN_SCRIPT_GREEK: add_greek = 1; break;
		case UCDN_SCRIPT_CYRILLIC: add_cyrillic = 1; break;
		case UCDN_SCRIPT_HANGUL: add_korean = 1; break;
		case UCDN_SCRIPT_HIRAGANA: add_japanese = 1; break;
		case UCDN_SCRIPT_KATAKANA: add_japanese = 1; break;
		case UCDN_SCRIPT_BOPOMOFO: add_bopomofo = 1; break;
		case UCDN_SCRIPT_HAN: add_han = 1; break;
		}
	}

	if (add_han)
	{
		switch (lang)
		{
		case FZ_LANG_ko: add_korean = 1; break;
		default: /* fall through */
		case FZ_LANG_ja: add_japanese = 1; break;
		case FZ_LANG_zh: /* fall through */
		case FZ_LANG_zh_Hant: add_hant = 1; break;
		case FZ_LANG_zh_Hans: add_hans = 1; break;
		}
	}

	if (add_bopomofo)
	{
		if (lang == FZ_LANG_zh_Hans)
			add_hans = 1;
		else
			add_hant = 1;
	}

	if (!add_greek && !add_cyrillic && !add_korean && !add_japanese && !add_hant && !add_hans)
		add_latin = 1;

	if (add_latin)
	{
		if (!pdf_dict_gets(ctx, res_font, fontname))
			pdf_dict_puts_drop(ctx, res_font, fontname,
				pdf_add_simple_font(ctx, doc, font, PDF_SIMPLE_ENCODING_LATIN));
	}
	if (add_greek)
	{
		fz_snprintf(buf, sizeof buf, "%sGRK", fontname);
		if (!pdf_dict_gets(ctx, res_font, buf))
			pdf_dict_puts_drop(ctx, res_font, buf,
				pdf_add_simple_font(ctx, doc, font, PDF_SIMPLE_ENCODING_GREEK));
	}
	if (add_cyrillic)
	{
		fz_snprintf(buf, sizeof buf, "%sCYR", fontname);
		if (!pdf_dict_gets(ctx, res_font, buf))
			pdf_dict_puts_drop(ctx, res_font, buf,
				pdf_add_simple_font(ctx, doc, font, PDF_SIMPLE_ENCODING_CYRILLIC));
	}
	if (add_korean && !pdf_dict_gets(ctx, res_font, "Batang"))
	{
		cjk_font = fz_new_cjk_font(ctx, FZ_ADOBE_KOREA);
		pdf_dict_puts_drop(ctx, res_font, "Batang",
			pdf_add_cjk_font(ctx, doc, font, FZ_ADOBE_KOREA, 0, 1));
		fz_drop_font(ctx, cjk_font);
	}
	if (add_japanese && !pdf_dict_gets(ctx, res_font, "Mincho"))
	{
		cjk_font = fz_new_cjk_font(ctx, FZ_ADOBE_JAPAN);
		pdf_dict_puts_drop(ctx, res_font, "Mincho",
			pdf_add_cjk_font(ctx, doc, font, FZ_ADOBE_JAPAN, 0, 1));
		fz_drop_font(ctx, cjk_font);
	}
	if (add_hant && !pdf_dict_gets(ctx, res_font, "Ming"))
	{
		cjk_font = fz_new_cjk_font(ctx, FZ_ADOBE_CNS);
		pdf_dict_puts_drop(ctx, res_font, "Ming",
			pdf_add_cjk_font(ctx, doc, font, FZ_ADOBE_CNS, 0, 1));
		fz_drop_font(ctx, cjk_font);
	}
	if (add_hans && !pdf_dict_gets(ctx, res_font, "Song"))
	{
		cjk_font = fz_new_cjk_font(ctx, FZ_ADOBE_GB);
		pdf_dict_puts_drop(ctx, res_font, "Song",
			pdf_add_cjk_font(ctx, doc, font, FZ_ADOBE_GB, 0, 1));
		fz_drop_font(ctx, cjk_font);
	}
}

static int find_initial_script(const char *text)
{
	int script = UCDN_SCRIPT_COMMON;
	int c;
	while (*text)
	{
		text += fz_chartorune(&c, text);
		script = ucdn_get_script(c);
		if (script != UCDN_SCRIPT_COMMON && script != UCDN_SCRIPT_INHERITED)
			break;
	}
	if (script == UCDN_SCRIPT_COMMON || script == UCDN_SCRIPT_INHERITED)
		script = UCDN_SCRIPT_LATIN;
	return script;
}

enum { ENC_LATIN = 1, ENC_GREEK, ENC_CYRILLIC, ENC_KOREAN, ENC_JAPANESE, ENC_HANT, ENC_HANS };

struct text_walk_state
{
	const char *text, *end;
	fz_font *font;
	fz_text_language lang;
	int enc, u, c, n, last_script;
	float w;
};

static void init_text_walk(fz_context *ctx, struct text_walk_state *state, fz_text_language lang, fz_font *font, const char *text, const char *end)
{
	state->text = text;
	state->end = end ? end : text + strlen(text);
	state->lang = lang;
	state->font = font;
	state->last_script = find_initial_script(text);
	state->n = 0;
}

static int next_text_walk(fz_context *ctx, struct text_walk_state *state)
{
	int script, g;

	state->text += state->n;
	if (state->text >= state->end)
	{
		state->n = 0;
		return 0;
	}

	state->n = fz_chartorune(&state->u, state->text);
	script = ucdn_get_script(state->u);
	if (script == UCDN_SCRIPT_COMMON || script == UCDN_SCRIPT_INHERITED)
		script = state->last_script;
	state->last_script = script;

	switch (script)
	{
	default:
		state->enc = ENC_LATIN;
		state->c = REPLACEMENT;
		break;
	case UCDN_SCRIPT_LATIN:
		state->enc = ENC_LATIN;
		state->c = fz_windows_1252_from_unicode(state->u);
		break;
	case UCDN_SCRIPT_GREEK:
		state->enc = ENC_GREEK;
		state->c = fz_iso8859_7_from_unicode(state->u);
		break;
	case UCDN_SCRIPT_CYRILLIC:
		state->enc = ENC_CYRILLIC;
		state->c = fz_koi8u_from_unicode(state->u);
		break;
	case UCDN_SCRIPT_HANGUL:
		state->enc = ENC_KOREAN;
		state->c = state->u;
		break;
	case UCDN_SCRIPT_HIRAGANA:
	case UCDN_SCRIPT_KATAKANA:
		state->enc = ENC_JAPANESE;
		state->c = state->u;
		break;
	case UCDN_SCRIPT_BOPOMOFO:
		state->enc = (state->lang == FZ_LANG_zh_Hans) ? ENC_HANS : ENC_HANT;
		state->c = state->u;
		break;
	case UCDN_SCRIPT_HAN:
		switch (state->lang)
		{
		case FZ_LANG_ko: state->enc = ENC_KOREAN; break;
		default: /* fall through */
		case FZ_LANG_ja: state->enc = ENC_JAPANESE; break;
		case FZ_LANG_zh: /* fall through */
		case FZ_LANG_zh_Hant: state->enc = ENC_HANT; break;
		case FZ_LANG_zh_Hans: state->enc = ENC_HANS; break;
		}
		state->c = state->u;
		break;
	}

	/* TODO: check that character is encodable with ENC_KOREAN/etc */
	if (state->c < 0)
	{
		state->enc = ENC_LATIN;
		state->c = REPLACEMENT;
	}

	if (state->enc >= ENC_KOREAN)
	{
		state->w = 1;
	}
	else
	{
		if (state->font != NULL)
		{
			g = fz_encode_character(ctx, state->font, state->u);
			state->w = fz_advance_glyph(ctx, state->font, g, 0);
		}
	}

	return 1;
}

static float
measure_string(fz_context *ctx, fz_text_language lang, fz_font *font, const char *a)
{
	struct text_walk_state state;
	float w = 0;
	init_text_walk(ctx, &state, lang, font, a, NULL);
	while (next_text_walk(ctx, &state))
		w += state.w;
	return w;
}


static float
break_string(fz_context *ctx, fz_text_language lang, fz_font *font, float size, const char *text, const char **endp, float maxw)
{
	struct text_walk_state state;
	const char *space = NULL;
	float space_x, x = 0;
	init_text_walk(ctx, &state, lang, font, text, NULL);
	while (next_text_walk(ctx, &state))
	{
		if (state.u == '\n' || state.u == '\r')
			break;
		if (state.u == ' ')
		{
			space = state.text + state.n;
			space_x = x;
		}
		x += state.w * size;
		if (space && x > maxw)
			return *endp = space, space_x;
	}
	return *endp = state.text + state.n, x;
}

static void
write_string(fz_context *ctx, fz_buffer *buf,
	fz_text_language lang, fz_font *font, const char *fontname, float size, const char *text, const char *end)
{
	struct text_walk_state state;
	int last_enc = 0;
	init_text_walk(ctx, &state, lang, font, text, end);
	while (next_text_walk(ctx, &state))
	{
		if (state.enc != last_enc)
		{
			if (last_enc)
			{
				if (last_enc < ENC_KOREAN)
					fz_append_byte(ctx, buf, ')');
				else
					fz_append_byte(ctx, buf, '>');
				fz_append_string(ctx, buf, " Tj\n");
			}

			switch (state.enc)
			{
			case ENC_LATIN: fz_append_printf(ctx, buf, "/%s %g Tf\n", fontname, size); break;
			case ENC_GREEK: fz_append_printf(ctx, buf, "/%sGRK %g Tf\n", fontname, size); break;
			case ENC_CYRILLIC: fz_append_printf(ctx, buf, "/%sCYR %g Tf\n", fontname, size); break;
			case ENC_KOREAN: fz_append_printf(ctx, buf, "/Batang %g Tf\n", size); break;
			case ENC_JAPANESE: fz_append_printf(ctx, buf, "/Mincho %g Tf\n", size); break;
			case ENC_HANT: fz_append_printf(ctx, buf, "/Ming %g Tf\n", size); break;
			case ENC_HANS: fz_append_printf(ctx, buf, "/Song %g Tf\n", size); break;
			}

			if (state.enc < ENC_KOREAN)
				fz_append_byte(ctx, buf, '(');
			else
				fz_append_byte(ctx, buf, '<');

			last_enc = state.enc;
		}

		if (state.enc < ENC_KOREAN)
		{
			if (state.c == '(' || state.c == ')' || state.c == '\\')
				fz_append_byte(ctx, buf, '\\');
			fz_append_byte(ctx, buf, state.c);
		}
		else
		{
			fz_append_printf(ctx, buf, "%04x", state.c);
		}
	}

	if (last_enc)
	{
		if (last_enc < ENC_KOREAN)
			fz_append_byte(ctx, buf, ')');
		else
			fz_append_byte(ctx, buf, '>');
		fz_append_string(ctx, buf, " Tj\n");
	}
}

static void
write_string_with_quadding(fz_context *ctx, fz_buffer *buf,
	fz_text_language lang, const char *fontname,
	fz_font *font, float size, float lineheight,
	const char *a, float maxw, int q)
{
	const char *b;
	float px = 0, x = 0, w;
	while (*a)
	{
		w = break_string(ctx, lang, font, size, a, &b, maxw);
		if (b > a)
		{
			if (q == 0)
				x = 0;
			else if (q == 1)
				x = (maxw - w) / 2;
			else
				x = (maxw - w);
			fz_append_printf(ctx, buf, "%g %g Td\n", x - px, -lineheight);
			if (b[-1] == '\n' || b[-1] == '\r')
				write_string(ctx, buf, lang, font, fontname, size, a, b-1);
			else
				write_string(ctx, buf, lang, font, fontname, size, a, b);
			a = b;
			px = x;
		}
	}
}

static void
write_comb_string(fz_context *ctx, fz_buffer *buf,
	fz_text_language lang, const char *fontname,
	fz_font *font, float size, const char *text, float cell_w)
{
	struct text_walk_state state;
	int last_enc = 0;
	float pad, carry = 0;

	init_text_walk(ctx, &state, lang, font, text, text + strlen(text));

	while (next_text_walk(ctx, &state))
	{
		if (state.enc != last_enc)
		{
			if (last_enc)
				fz_append_string(ctx, buf, "] TJ\n");

			switch (state.enc)
			{
			case ENC_LATIN: fz_append_printf(ctx, buf, "/%s %g Tf\n", fontname, size); break;
			case ENC_GREEK: fz_append_printf(ctx, buf, "/%sGRK %g Tf\n", fontname, size); break;
			case ENC_CYRILLIC: fz_append_printf(ctx, buf, "/%sCYR %g Tf\n", fontname, size); break;
			case ENC_KOREAN: fz_append_printf(ctx, buf, "/Batang %g Tf\n", size); break;
			case ENC_JAPANESE: fz_append_printf(ctx, buf, "/Mincho %g Tf\n", size); break;
			case ENC_HANT: fz_append_printf(ctx, buf, "/Ming %g Tf\n", size); break;
			case ENC_HANS: fz_append_printf(ctx, buf, "/Song %g Tf\n", size); break;
			}

			fz_append_byte(ctx, buf, '[');

			last_enc = state.enc;
		}

		pad = (cell_w - state.w * 1000) / 2;
		fz_append_printf(ctx, buf, "%g", -(carry + pad));
		carry = pad;

		if (state.enc < ENC_KOREAN)
		{
			fz_append_byte(ctx, buf, '(');
			if (state.c == '(' || state.c == ')' || state.c == '\\')
				fz_append_byte(ctx, buf, '\\');
			fz_append_byte(ctx, buf, state.c);
			fz_append_byte(ctx, buf, ')');
		}
		else
		{
			fz_append_printf(ctx, buf, "<%04x>", state.c);
		}
	}
	if (last_enc)
		fz_append_string(ctx, buf, "] TJ\n");
}

static void
layout_comb_string(fz_context *ctx, fz_layout_block *out, float x, float y,
	const char *a, const char *b, fz_font *font, float size, float cell_w)
{
	int n, c, g;
	int first = 1;
	float w;
	if (a == b)
		fz_add_layout_line(ctx, out, x + cell_w / 2, y, size, a);
	while (a < b)
	{
		n = fz_chartorune(&c, a);
		c = fz_windows_1252_from_unicode(c);
		if (c < 0) c = REPLACEMENT;
		g = fz_encode_character(ctx, font, c);
		w = fz_advance_glyph(ctx, font, g, 0) * size;
		if (first)
		{
			fz_add_layout_line(ctx, out, x + (cell_w - w) / 2, y, size, a);
			first = 0;
		}
		fz_add_layout_char(ctx, out, x + (cell_w - w) / 2, w, a);
		a += n;
		x += cell_w;
	}
}

static void
layout_string(fz_context *ctx, fz_layout_block *out,
	fz_text_language lang, fz_font *font, float size,
	float x, float y, const char *a, const char *b)
{
	struct text_walk_state state;
	fz_add_layout_line(ctx, out, x, y, size, a);
	init_text_walk(ctx, &state, lang, font, a, b);
	while (next_text_walk(ctx, &state))
	{
		fz_add_layout_char(ctx, out, x, state.w * size, state.text);
		x += state.w * size;
	}
}

static void
layout_string_with_quadding(fz_context *ctx, fz_layout_block *out,
	fz_text_language lang, fz_font *font, float size, float lineheight,
	float xorig, float y, const char *a, float maxw, int q)
{
	const char *b;
	float x = 0, w;
	int add_line_at_end = 0;

	if (!*a)
		add_line_at_end = 1;

	while (*a)
	{
		w = break_string(ctx, lang, font, size, a, &b, maxw);
		if (b > a)
		{
			if (q > 0)
			{
				if (q == 1)
					x = (maxw - w) / 2;
				else
					x = (maxw - w);
			}
			if (b[-1] == '\n' || b[-1] == '\r')
			{
				layout_string(ctx, out, lang, font, size, xorig+x, y, a, b-1);
				add_line_at_end = 1;
			}
			else
			{
				layout_string(ctx, out, lang, font, size, xorig+x, y, a, b);
				add_line_at_end = 0;
			}
			a = b;
			y -= lineheight;
		}
	}
	if (add_line_at_end)
		fz_add_layout_line(ctx, out, xorig, y, size, a);
}

static const char *full_font_name(const char **name)
{
	if (!strcmp(*name, "Cour")) return "Courier";
	if (!strcmp(*name, "Helv")) return "Helvetica";
	if (!strcmp(*name, "TiRo")) return "Times-Roman";
	if (!strcmp(*name, "Symb")) return "Symbol";
	if (!strcmp(*name, "ZaDb")) return "ZapfDingbats";
	return *name = "Helv", "Helvetica";
}

static void
write_variable_text(fz_context *ctx, pdf_annot *annot, fz_buffer *buf, pdf_obj **res,
	fz_text_language lang, const char *text,
	const char *fontname, float size, int n, float *color, int q,
	float w, float h, float padding, float baseline, float lineheight,
	int multiline, int comb, int adjust_baseline)
{
	fz_font *font;
	pdf_obj *res_font;

	w -= padding * 2;
	h -= padding * 2;

	font = fz_new_base14_font(ctx, full_font_name(&fontname));
	fz_try(ctx)
	{
		if (!*res)
			*res = pdf_new_dict(ctx, annot->page->doc, 1);
		res_font = pdf_dict_put_dict(ctx, *res, PDF_NAME(Font), 1);
		add_required_fonts(ctx, annot->page->doc, res_font, lang, font, fontname, text);

		if (size == 0)
		{
			if (multiline)
				size = 12;
			else
			{
				size = w / measure_string(ctx, lang, font, text);
				if (size > h)
					size = h;
			}
		}

		lineheight = size * lineheight;
		baseline = size * baseline;

		if (adjust_baseline)
		{
			/* Make sure baseline is inside rectangle */
			if (baseline + 0.2f * size > h)
				baseline = h - 0.2f * size;
		}

		fz_append_string(ctx, buf, "BT\n");
		if (n == 4)
			fz_append_printf(ctx, buf, "%g %g %g %g k\n", color[0], color[1], color[2], color[3]);
		else if (n == 3)
			fz_append_printf(ctx, buf, "%g %g %g rg\n", color[0], color[1], color[2]);
		else if (n == 1)
			fz_append_printf(ctx, buf, "%g g\n", color[0]);
		else if (n == 0)
			fz_append_printf(ctx, buf, "0 g\n");
		if (multiline)
		{
			fz_append_printf(ctx, buf, "%g %g Td\n", padding, padding+h-baseline+lineheight);
			write_string_with_quadding(ctx, buf, lang, fontname, font, size, lineheight, text, w, q);
		}
		else if (comb > 0)
		{
			float ty = (h - size) / 2;
			fz_append_printf(ctx, buf, "%g %g Td\n", padding, padding+h-baseline-ty);
			write_comb_string(ctx, buf, lang, fontname, font, size, text, (w * 1000 / size) / comb);
		}
		else
		{
			float tx = 0, ty = (h - size) / 2;
			if (q > 0)
			{
				float tw = measure_string(ctx, lang, font, text) * size;
				if (q == 1)
					tx = (w - tw) / 2;
				else
					tx = (w - tw);
			}
			fz_append_printf(ctx, buf, "%g %g Td\n", padding+tx, padding+h-baseline-ty);
			write_string(ctx, buf, lang, font, fontname, size, text, text + strlen(text));
		}
		fz_append_string(ctx, buf, "ET\n");
	}
	fz_always(ctx)
		fz_drop_font(ctx, font);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
layout_variable_text(fz_context *ctx, fz_layout_block *out,
	const char *text, fz_text_language lang, const char *fontname, float size, int q,
	float x, float y, float w, float h, float padding, float baseline, float lineheight,
	int multiline, int comb, int adjust_baseline)
{
	fz_font *font;

	w -= padding * 2;
	h -= padding * 2;

	font = fz_new_base14_font(ctx, full_font_name(&fontname));
	fz_try(ctx)
	{
		if (size == 0)
		{
			if (multiline)
				size = 12;
			else
			{
				size = w / measure_string(ctx, lang, font, text);
				if (size > h)
					size = h;
			}
		}

		lineheight = size * lineheight;
		baseline = size * baseline;

		if (adjust_baseline)
		{
			/* Make sure baseline is inside rectangle */
			if (baseline + 0.2f * size > h)
				baseline = h - 0.2f * size;
		}

		if (multiline)
		{
			x += padding;
			y += padding + h - baseline;
			layout_string_with_quadding(ctx, out, lang, font, size, lineheight, x, y, text, w, q);
		}
		else if (comb > 0)
		{
			float ty = (h - size) / 2;
			x += padding;
			y += padding + h - baseline - ty;
			layout_comb_string(ctx, out, x, y, text, text + strlen(text), font, size, w / comb);
		}
		else
		{
			float tx = 0, ty = (h - size) / 2;
			if (q > 0)
			{
				float tw = measure_string(ctx, lang, font, text) * size;
				if (q == 1)
					tx = (w - tw) / 2;
				else
					tx = (w - tw);
			}
			x += padding + tx;
			y += padding + h - baseline - ty;
			layout_string(ctx, out, lang, font, size, x, y, text, text + strlen(text));
		}
	}
	fz_always(ctx)
		fz_drop_font(ctx, font);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_write_free_text_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf,
	fz_rect *rect, fz_rect *bbox, fz_matrix *matrix, pdf_obj **res)
{
	const char *font;
	float size, color[4];
	const char *text;
	float w, h, t, b;
	int q, r, n;
	int lang;

	/* /Rotate is an undocumented annotation property supported by Adobe */
	text = pdf_annot_contents(ctx, annot);
	r = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(Rotate));
	q = pdf_annot_quadding(ctx, annot);
	pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	lang = pdf_annot_language(ctx, annot);

	w = rect->x1 - rect->x0;
	h = rect->y1 - rect->y0;
	if (r == 90 || r == 270)
		t = h, h = w, w = t;

	*matrix = fz_rotate(r);
	*bbox = fz_make_rect(0, 0, w, h);

	pdf_write_opacity(ctx, annot, buf, res);

	if (pdf_write_fill_color_appearance(ctx, annot, buf))
		fz_append_printf(ctx, buf, "0 0 %g %g re\nf\n", w, h);

	b = pdf_write_border_appearance(ctx, annot, buf);
	if (b > 0)
	{
		if (n == 4)
			fz_append_printf(ctx, buf, "%g %g %g %g K\n", color[0], color[1], color[2], color[3]);
		else if (n == 3)
			fz_append_printf(ctx, buf, "%g %g %g RG\n", color[0], color[1], color[2]);
		else if (n == 1)
			fz_append_printf(ctx, buf, "%g G\n", color[0]);
		else if (n == 0)
			fz_append_printf(ctx, buf, "0 G\n");
		fz_append_printf(ctx, buf, "%g %g %g %g re\nS\n", b/2, b/2, w-b, h-b);
	}

	fz_append_printf(ctx, buf, "%g %g %g %g re\nW\nn\n", b, b, w-b*2, h-b*2);

	write_variable_text(ctx, annot, buf, res, lang, text, font, size, n, color, q, w, h, b*2,
		0.8f, 1.2f, 1, 0, 0);
}

static void
pdf_write_tx_widget_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf,
	fz_rect *rect, fz_rect *bbox, fz_matrix *matrix, pdf_obj **res,
	const char *text, int ff)
{
	fz_text_language lang;
	const char *font;
	float size, color[4];
	float w, h, t, b;
	int has_bc = 0;
	int q, r, n;

	r = pdf_dict_get_int(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(R));
	q = pdf_annot_quadding(ctx, annot);
	pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	lang = pdf_annot_language(ctx, annot);

	w = rect->x1 - rect->x0;
	h = rect->y1 - rect->y0;
	r = r % 360;
	if (r == 90 || r == 270)
		t = h, h = w, w = t;
	*matrix = fz_rotate(r);
	*bbox = fz_make_rect(0, 0, w, h);

	fz_append_string(ctx, buf, "/Tx BMC\nq\n");

	if (pdf_write_MK_BG_appearance(ctx, annot, buf))
		fz_append_printf(ctx, buf, "0 0 %g %g re\nf\n", w, h);

	b = pdf_write_border_appearance(ctx, annot, buf);
	if (b > 0 && pdf_write_MK_BC_appearance(ctx, annot, buf))
	{
		fz_append_printf(ctx, buf, "%g %g %g %g re\ns\n", b/2, b/2, w-b, h-b);
		has_bc = 1;
	}

	fz_append_printf(ctx, buf, "%g %g %g %g re\nW\nn\n", b, b, w-b*2, h-b*2);

	if (ff & PDF_TX_FIELD_IS_MULTILINE)
	{
		write_variable_text(ctx, annot, buf, res, lang, text, font, size, n, color, q, w, h, b*2,
			1.116f, 1.116f, 1, 0, 1);
	}
	else if (ff & PDF_TX_FIELD_IS_COMB)
	{
		int maxlen = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(MaxLen)));
		if (has_bc && maxlen > 1)
		{
			float cell_w = (w - 2 * b) / maxlen;
			int i;
			for (i = 1; i < maxlen; ++i)
			{
				float x = b + cell_w * i;
				fz_append_printf(ctx, buf, "%g %g m %g %g l s\n", x, b, x, h-b);
			}
		}
		write_variable_text(ctx, annot, buf, res, lang, text, font, size, n, color, q, w, h, 0,
			0.8f, 1.2f, 0, maxlen, 0);
	}
	else
	{
		write_variable_text(ctx, annot, buf, res, lang, text, font, size, n, color, q, w, h, b*2,
			0.8f, 1.2f, 0, 0, 0);
	}

	fz_append_string(ctx, buf, "Q\nEMC\n");
}

fz_layout_block *
pdf_layout_text_widget(fz_context *ctx, pdf_annot *annot)
{
	fz_text_language lang;
	fz_layout_block *out;
	const char *font;
	const char *text;
	fz_rect rect;
	float size, color[4];
	float w, h, t, b, x, y;
	int q, r, n;
	int ff;

	rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	text = pdf_field_value(ctx, annot->obj);
	ff = pdf_field_flags(ctx, annot->obj);

	b = pdf_annot_border(ctx, annot);
	r = pdf_dict_get_int(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(R));
	q = pdf_annot_quadding(ctx, annot);
	pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	lang = pdf_annot_language(ctx, annot);

	w = rect.x1 - rect.x0;
	h = rect.y1 - rect.y0;
	r = r % 360;
	if (r == 90 || r == 270)
		t = h, h = w, w = t;

	x = rect.x0;
	y = rect.y0;

	out = fz_new_layout(ctx);
	fz_try(ctx)
	{
		pdf_page_transform(ctx, annot->page, NULL, &out->matrix);
		out->matrix = fz_concat(out->matrix, fz_rotate(r));
		out->inv_matrix = fz_invert_matrix(out->matrix);

		if (ff & PDF_TX_FIELD_IS_MULTILINE)
		{
			layout_variable_text(ctx, out, text, lang, font, size, q, x, y, w, h, b*2, 1.116f, 1.116f, 1, 0, 1);
		}
		else if (ff & PDF_TX_FIELD_IS_COMB)
		{
			int maxlen = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(MaxLen)));
			layout_variable_text(ctx, out, text, lang, font, size, q, x, y, w, h, 0, 0.8f, 1.2f, 0, maxlen, 0);
		}
		else
		{
			layout_variable_text(ctx, out, text, lang, font, size, q, x, y, w, h, b*2, 0.8f, 1.2f, 0, 0, 0);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_layout(ctx, out);
		fz_rethrow(ctx);
	}
	return out;
}

static void
pdf_write_ch_widget_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf,
	fz_rect *rect, fz_rect *bbox, fz_matrix *matrix, pdf_obj **res)
{
	int ff = pdf_field_flags(ctx, annot->obj);
	if (ff & PDF_CH_FIELD_IS_COMBO)
	{
		/* TODO: Pop-down arrow */
		pdf_write_tx_widget_appearance(ctx, annot, buf, rect, bbox, matrix, res,
			pdf_field_value(ctx, annot->obj), 0);
	}
	else
	{
		fz_buffer *text = fz_new_buffer(ctx, 1024);
		fz_try(ctx)
		{
			pdf_obj *opt = pdf_dict_get(ctx, annot->obj, PDF_NAME(Opt));
			int i = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(TI));
			int n = pdf_array_len(ctx, opt);
			/* TODO: Scrollbar */
			/* TODO: Highlight selected items */
			if (i < 0)
				i = 0;
			for (; i < n; ++i)
			{
				pdf_obj *val = pdf_array_get(ctx, opt, i);
				if (pdf_is_array(ctx, val))
					fz_append_string(ctx, text, pdf_array_get_text_string(ctx, val, 1));
				else
					fz_append_string(ctx, text, pdf_to_text_string(ctx, val));
				fz_append_byte(ctx, text, '\n');
			}
			pdf_write_tx_widget_appearance(ctx, annot, buf, rect, bbox, matrix, res,
				fz_string_from_buffer(ctx, text), PDF_TX_FIELD_IS_MULTILINE);
		}
		fz_always(ctx)
			fz_drop_buffer(ctx, text);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static void
pdf_write_sig_widget_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf,
	fz_rect *rect, fz_rect *bbox, fz_matrix *matrix, pdf_obj **res)
{
	float x0 = rect->x0 + 1;
	float y0 = rect->y0 + 1;
	float x1 = rect->x1 - 1;
	float y1 = rect->y1 - 1;
	float w = x1 - x0;
	float h = y1 - y0;
	fz_append_printf(ctx, buf, "1 w\n0 G\n");
	fz_append_printf(ctx, buf, "%g %g %g %g re\n", x0, y0, w, h);
	fz_append_printf(ctx, buf, "%g %g m %g %g l\n", x0, y0, x1, y1);
	fz_append_printf(ctx, buf, "%g %g m %g %g l\n", x1, y0, x0, y1);
	fz_append_printf(ctx, buf, "s\n");
	*bbox = *rect;
	*matrix = fz_identity;
}

static void
pdf_write_widget_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf,
	fz_rect *rect, fz_rect *bbox, fz_matrix *matrix, pdf_obj **res)
{
	pdf_obj *ft = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(FT));
	if (pdf_name_eq(ctx, ft, PDF_NAME(Tx)))
	{
		int ff = pdf_field_flags(ctx, annot->obj);
		char *format = NULL;
		const char *text = NULL;
		if (!annot->ignore_trigger_events)
		{
			format = pdf_field_event_format(ctx, annot->page->doc, annot->obj);
			if (format)
				text = format;
			else
				text = pdf_field_value(ctx, annot->obj);
		}
		else
		{
			text = pdf_field_value(ctx, annot->obj);
		}
		fz_try(ctx)
			pdf_write_tx_widget_appearance(ctx, annot, buf, rect, bbox, matrix, res, text, ff);
		fz_always(ctx)
			fz_free(ctx, format);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else if (pdf_name_eq(ctx, ft, PDF_NAME(Ch)))
	{
		pdf_write_ch_widget_appearance(ctx, annot, buf, rect, bbox, matrix, res);
	}
	else if (pdf_name_eq(ctx, ft, PDF_NAME(Sig)))
	{
		pdf_write_sig_widget_appearance(ctx, annot, buf, rect, bbox, matrix, res);
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot create appearance stream for %s widgets", pdf_to_name(ctx, ft));
	}
}

static void
pdf_write_appearance(fz_context *ctx, pdf_annot *annot, fz_buffer *buf,
	fz_rect *rect, fz_rect *bbox, fz_matrix *matrix, pdf_obj **res)
{
	switch (pdf_annot_type(ctx, annot))
	{
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot create appearance stream for %s annotations",
			pdf_dict_get_name(ctx, annot->obj, PDF_NAME(Subtype)));
	case PDF_ANNOT_WIDGET:
		pdf_write_widget_appearance(ctx, annot, buf, rect, bbox, matrix, res);
		break;
	case PDF_ANNOT_INK:
		pdf_write_ink_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_POLYGON:
		pdf_write_polygon_appearance(ctx, annot, buf, rect, res, 1);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_POLY_LINE:
		pdf_write_polygon_appearance(ctx, annot, buf, rect, res, 0);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_LINE:
		pdf_write_line_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_SQUARE:
		pdf_write_square_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_CIRCLE:
		pdf_write_circle_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_CARET:
		pdf_write_caret_appearance(ctx, annot, buf, rect, bbox, res);
		*matrix = fz_identity;
		break;
	case PDF_ANNOT_TEXT:
	case PDF_ANNOT_FILE_ATTACHMENT:
	case PDF_ANNOT_SOUND:
		pdf_write_icon_appearance(ctx, annot, buf, rect, bbox, res);
		*matrix = fz_identity;
		break;
	case PDF_ANNOT_HIGHLIGHT:
		pdf_write_highlight_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_UNDERLINE:
		pdf_write_underline_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_STRIKE_OUT:
		pdf_write_strike_out_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_SQUIGGLY:
		pdf_write_squiggly_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_REDACT:
		pdf_write_redact_appearance(ctx, annot, buf, rect, res);
		*matrix = fz_identity;
		*bbox = *rect;
		break;
	case PDF_ANNOT_STAMP:
		pdf_write_stamp_appearance(ctx, annot, buf, rect, bbox, res);
		*matrix = fz_identity;
		break;
	case PDF_ANNOT_FREE_TEXT:
		pdf_write_free_text_appearance(ctx, annot, buf, rect, bbox, matrix, res);
		break;
	}
}

static pdf_obj *draw_push_button(fz_context *ctx, pdf_annot *annot, fz_rect bbox, fz_matrix matrix, float w, float h,
	const char *caption, const char *font, float size, int n, float *color,
	int down)
{
	pdf_obj *ap, *res = NULL;
	fz_buffer *buf;
	float bc[3] = { 0, 0, 0 };
	float bg[3] = { 0.8f, 0.8f, 0.8f };
	float hi[3], sh[3];
	int has_bg, has_bc;
	float b;
	int i;

	buf = fz_new_buffer(ctx, 1024);
	fz_var(res);
	fz_try(ctx)
	{
		b = pdf_annot_border(ctx, annot);
		has_bc = pdf_annot_MK_BC_rgb(ctx, annot, bc);
		has_bg = pdf_annot_MK_BG_rgb(ctx, annot, bg);

		for (i = 0; i < 3; ++i)
		{
			if (down)
			{
				sh[i] = 1 - (1 - bg[i]) / 2;
				hi[i] = bg[i] / 2;
			}
			else
			{
				hi[i] = 1 - (1 - bg[i]) / 2;
				sh[i] = bg[i] / 2;
			}
		}

		fz_append_string(ctx, buf, "q\n");
		fz_append_printf(ctx, buf, "%g w\n", b);
		if (has_bg)
		{
			fz_append_printf(ctx, buf, "%g %g %g rg\n", bg[0], bg[1], bg[2]);
			fz_append_printf(ctx, buf, "0 0 %g %g re\nf\n", 0, 0, w, h);
		}
		if (has_bc && b > 0)
		{
			fz_append_printf(ctx, buf, "%g %g %g RG\n", bc[0], bc[1], bc[2]);
			fz_append_printf(ctx, buf, "%g %g %g %g re\nS\n", b/2, b/2, w-b, h-b);
		}
		if (has_bg)
		{
			fz_append_printf(ctx, buf, "%g %g %g rg\n", hi[0], hi[1], hi[2]);
			fz_append_printf(ctx, buf, "%g %g m %g %g l %g %g l %g %g l %g %g l %g %g l f\n",
				b, b, b, h-b, w-b, h-b, w-b-2, h-b-2, b+2, h-b-2, b+2, b+2);
			fz_append_printf(ctx, buf, "%g %g %g rg\n", sh[0], sh[1], sh[2]);
			fz_append_printf(ctx, buf, "%g %g m %g %g l %g %g l %g %g l %g %g l %g %g l f\n",
				b, b, b+2, b+2, w-b-2, b+2, w-b-2, h-b-2, w-b, h-b, w-b, b);
		}
		if (down)
			fz_append_string(ctx, buf, "1 0 0 1 2 -2 cm\n");
		write_variable_text(ctx, annot, buf, &res, FZ_LANG_UNSET, caption, font, size, n, color, 1, w, h, b+6, 0.8f, 1.2f, 0, 0, 0);
		fz_append_string(ctx, buf, "Q\n");

		ap = pdf_new_xobject(ctx, annot->page->doc, bbox, matrix, res, buf);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, res);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
	return ap;
}

static pdf_obj *draw_radio_button(fz_context *ctx, pdf_annot *annot, fz_rect bbox, fz_matrix matrix, float w, float h, int yes)
{
	pdf_obj *ap;
	fz_buffer *buf;
	float b;

	buf = fz_new_buffer(ctx, 1024);
	fz_try(ctx)
	{
		fz_append_string(ctx, buf, "q\n");
		if (pdf_write_MK_BG_appearance(ctx, annot, buf))
		{
			draw_circle_in_box(ctx, buf, 0, 0, 0, w, h);
			fz_append_string(ctx, buf, "f\n");
		}
		b = pdf_write_border_appearance(ctx, annot, buf);
		if (b > 0 && pdf_write_MK_BC_appearance(ctx, annot, buf))
		{
			draw_circle_in_box(ctx, buf, b, 0, 0, w, h);
			fz_append_string(ctx, buf, "s\n");
		}
		if (yes)
		{
			fz_append_string(ctx, buf, "0 g\n");
			draw_circle(ctx, buf, (w-b*2)/4, (h-b*2)/4, w/2, h/2);
			fz_append_string(ctx, buf, "f\n");
		}
		fz_append_string(ctx, buf, "Q\n");
		ap = pdf_new_xobject(ctx, annot->page->doc, bbox, matrix, NULL, buf);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return ap;
}

static pdf_obj *draw_check_button(fz_context *ctx, pdf_annot *annot, fz_rect bbox, fz_matrix matrix, float w, float h, int yes)
{
	float black[1] = { 0 };
	pdf_obj *ap, *res = NULL;
	fz_buffer *buf;
	float b;

	fz_var(res);

	buf = fz_new_buffer(ctx, 1024);
	fz_try(ctx)
	{
		fz_append_string(ctx, buf, "q\n");
		if (pdf_write_MK_BG_appearance(ctx, annot, buf))
			fz_append_printf(ctx, buf, "0 0 %g %g re\nf\n", w, h);
		b = pdf_write_border_appearance(ctx, annot, buf);
		if (b > 0 && pdf_write_MK_BC_appearance(ctx, annot, buf))
			fz_append_printf(ctx, buf, "%g %g %g %g re\nS\n", b/2, b/2, w-b, h-b);
		if (yes)
			write_variable_text(ctx, annot, buf, &res, FZ_LANG_UNSET, "3", "ZaDb", h, nelem(black), black, 0, w, h, b+h/10, 0.8f, 1.2f, 0, 0, 0);
		fz_append_string(ctx, buf, "Q\n");
		ap = pdf_new_xobject(ctx, annot->page->doc, bbox, matrix, res, buf);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, res);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
	return ap;
}

static void pdf_update_button_appearance(fz_context *ctx, pdf_annot *annot)
{
	int ff = pdf_field_flags(ctx, annot->obj);
	fz_rect rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	fz_matrix matrix;
	fz_rect bbox;
	float w, h, t;
	int r;

	r = pdf_dict_get_int(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(R));
	w = rect.x1 - rect.x0;
	h = rect.y1 - rect.y0;
	r = r % 360;
	if (r == 90 || r == 270)
		t = h, h = w, w = t;
	matrix = fz_rotate(r);
	bbox = fz_make_rect(0, 0, w, h);


	if (ff & PDF_BTN_FIELD_IS_PUSHBUTTON)
	{
		pdf_obj *ap_n = NULL;
		pdf_obj *ap_d = NULL;
		fz_var(ap_n);
		fz_var(ap_d);
		fz_try(ctx)
		{
			pdf_obj *ap, *MK, *CA, *AC;
			const char *font;
			const char *label;
			float size, color[4];
			int n;

			pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);

			MK = pdf_dict_get(ctx, annot->obj, PDF_NAME(MK));
			CA = pdf_dict_get(ctx, MK, PDF_NAME(CA));
			AC = pdf_dict_get(ctx, MK, PDF_NAME(AC));

			label = pdf_to_text_string(ctx, CA);
			ap_n = draw_push_button(ctx, annot, bbox, matrix, w, h, label, font, size, n, color, 0);

			label = pdf_to_text_string(ctx, AC ? AC : CA);
			ap_d = draw_push_button(ctx, annot, bbox, matrix, w, h, label, font, size, n, color, 1);

			ap = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(AP), 2);
			pdf_dict_put(ctx, ap, PDF_NAME(N), ap_n);
			pdf_dict_put(ctx, ap, PDF_NAME(D), ap_d);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(ctx, ap_n);
			pdf_drop_obj(ctx, ap_d);
		}
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else
	{
		pdf_obj *as_yes = NULL;
		pdf_obj *ap_off = NULL;
		pdf_obj *ap_yes = NULL;
		fz_var(ap_off);
		fz_var(ap_yes);
		fz_var(as_yes);
		fz_try(ctx)
		{
			pdf_obj *ap, *ap_n, *as;

			if (w > h) w = h;
			if (h > w) h = w;

			if (ff & PDF_BTN_FIELD_IS_RADIO)
			{
				ap_off = draw_radio_button(ctx, annot, bbox, matrix, w, h, 0);
				ap_yes = draw_radio_button(ctx, annot, bbox, matrix, w, h, 1);
			}
			else
			{
				ap_off = draw_check_button(ctx, annot, bbox, matrix, w, h, 0);
				ap_yes = draw_check_button(ctx, annot, bbox, matrix, w, h, 1);
			}

			as = pdf_dict_get(ctx, annot->obj, PDF_NAME(AS));
			if (!as)
			{
				pdf_dict_put(ctx, annot->obj, PDF_NAME(AS), PDF_NAME(Off));
				as = PDF_NAME(Off);
			}

			if (as == PDF_NAME(Off))
				as_yes = pdf_keep_obj(ctx, pdf_button_field_on_state(ctx, annot->obj));
			else
				as_yes = pdf_keep_obj(ctx, as);

			ap = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(AP), 2);
			ap_n = pdf_dict_put_dict(ctx, ap, PDF_NAME(N), 2);
			pdf_dict_put(ctx, ap_n, PDF_NAME(Off), ap_off);
			pdf_dict_put(ctx, ap_n, as_yes, ap_yes);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(ctx, as_yes);
			pdf_drop_obj(ctx, ap_yes);
			pdf_drop_obj(ctx, ap_off);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}
	pdf_set_annot_resynthesised(ctx, annot);
}

static void draw_logo(fz_context *ctx, fz_path *path)
{
	/* Use mupdf logo for signature appearance background. */
	fz_moveto(ctx, path, 122.25f, 0.0f);
	fz_lineto(ctx, path, 122.25f, 14.249f);
	fz_curveto(ctx, path, 125.98f, 13.842f, 129.73f, 13.518f, 133.5f, 13.277f);
	fz_lineto(ctx, path, 133.5f, 0.0f);
	fz_lineto(ctx, path, 122.25f, 0.0f);
	fz_closepath(ctx, path);
	fz_moveto(ctx, path, 140.251f, 0.0f);
	fz_lineto(ctx, path, 140.251f, 12.935f);
	fz_curveto(ctx, path, 152.534f, 12.477f, 165.03f, 12.899f, 177.75f, 14.249f);
	fz_lineto(ctx, path, 177.75f, 21.749f);
	fz_curveto(ctx, path, 165.304f, 20.413f, 152.809f, 19.871f, 140.251f, 20.348f);
	fz_lineto(ctx, path, 140.251f, 39.0f);
	fz_lineto(ctx, path, 133.5f, 39.0f);
	fz_lineto(ctx, path, 133.5f, 20.704f);
	fz_curveto(ctx, path, 129.756f, 20.956f, 126.006f, 21.302f, 122.25f, 21.749f);
	fz_lineto(ctx, path, 122.25f, 50.999f);
	fz_lineto(ctx, path, 177.751f, 50.999f);
	fz_lineto(ctx, path, 177.751f, 0.0f);
	fz_lineto(ctx, path, 140.251f, 0.0f);
	fz_closepath(ctx, path);
	fz_moveto(ctx, path, 23.482f, 129.419f);
	fz_curveto(ctx, path, -20.999f, 199.258f, -0.418f, 292.039f, 69.42f, 336.519f);
	fz_curveto(ctx, path, 139.259f, 381.0f, 232.04f, 360.419f, 276.52f, 290.581f);
	fz_curveto(ctx, path, 321.001f, 220.742f, 300.42f, 127.961f, 230.582f, 83.481f);
	fz_curveto(ctx, path, 160.743f, 39.0f, 67.962f, 59.581f, 23.482f, 129.419f);
	fz_closepath(ctx, path);
	fz_moveto(ctx, path, 254.751f, 128.492f);
	fz_curveto(ctx, path, 303.074f, 182.82f, 295.364f, 263.762f, 237.541f, 309.165f);
	fz_curveto(ctx, path, 179.718f, 354.568f, 93.57f, 347.324f, 45.247f, 292.996f);
	fz_curveto(ctx, path, -3.076f, 238.668f, 4.634f, 157.726f, 62.457f, 112.323f);
	fz_curveto(ctx, path, 120.28f, 66.92f, 206.428f, 74.164f, 254.751f, 128.492f);
	fz_closepath(ctx, path);
	fz_moveto(ctx, path, 111.0f, 98.999f);
	fz_curveto(ctx, path, 87.424f, 106.253f, 68.25f, 122.249f, 51.75f, 144.749f);
	fz_lineto(ctx, path, 103.5f, 297.749f);
	fz_lineto(ctx, path, 213.75f, 298.499f);
	fz_curveto(ctx, path, 206.25f, 306.749f, 195.744f, 311.478f, 185.25f, 314.249f);
	fz_curveto(ctx, path, 164.22f, 319.802f, 141.22f, 319.775f, 120.0f, 314.999f);
	fz_curveto(ctx, path, 96.658f, 309.745f, 77.25f, 298.499f, 55.5f, 283.499f);
	fz_curveto(ctx, path, 69.75f, 299.249f, 84.617f, 311.546f, 102.75f, 319.499f);
	fz_curveto(ctx, path, 117.166f, 325.822f, 133.509f, 327.689f, 149.25f, 327.749f);
	fz_curveto(ctx, path, 164.21f, 327.806f, 179.924f, 326.532f, 193.5f, 320.249f);
	fz_curveto(ctx, path, 213.95f, 310.785f, 232.5f, 294.749f, 245.25f, 276.749f);
	fz_lineto(ctx, path, 227.25f, 276.749f);
	fz_curveto(ctx, path, 213.963f, 276.749f, 197.25f, 263.786f, 197.25f, 250.499f);
	fz_lineto(ctx, path, 197.25f, 112.499f);
	fz_curveto(ctx, path, 213.75f, 114.749f, 228.0f, 127.499f, 241.5f, 140.999f);
	fz_curveto(ctx, path, 231.75f, 121.499f, 215.175f, 109.723f, 197.25f, 101.249f);
	fz_curveto(ctx, path, 181.5f, 95.249f, 168.412f, 94.775f, 153.0f, 94.499f);
	fz_curveto(ctx, path, 139.42f, 94.256f, 120.75f, 95.999f, 111.0f, 98.999f);
	fz_closepath(ctx, path);
	fz_moveto(ctx, path, 125.25f, 105.749f);
	fz_lineto(ctx, path, 125.25f, 202.499f);
	fz_lineto(ctx, path, 95.25f, 117.749f);
	fz_curveto(ctx, path, 105.75f, 108.749f, 114.0f, 105.749f, 125.25f, 105.749f);
	fz_closepath(ctx, path);
};

static float logo_color[3] = { (float)0xa4 / (float)0xFF, (float)0xca / (float)0xFF, (float)0xf5 / (float)0xFF };

fz_display_list *
pdf_signature_appearance_signed(fz_context *ctx, fz_rect rect, fz_text_language lang, fz_image *img, const char *left_text, const char *right_text, int include_logo)
{
	fz_display_list *dlist = NULL;
	fz_device *dev = NULL;
	fz_text *text = NULL;
	fz_colorspace *cs = NULL;
	fz_path *path = NULL;
	fz_font *font = NULL;

	fz_var(path);
	fz_var(dlist);
	fz_var(dev);
	fz_var(text);
	fz_var(font);
	fz_try(ctx)
	{
		fz_rect prect;
		fz_rect logo_bounds;
		fz_matrix logo_tm;
		float color[] = { 0.0, 0.0, 0.0 };

		font = fz_new_base14_font(ctx, "Helvetica");

		dlist = fz_new_display_list(ctx, rect);
		dev = fz_new_list_device(ctx, dlist);
		cs = fz_device_rgb(ctx);

		if (include_logo)
		{
			path = fz_new_path(ctx);
			draw_logo(ctx, path);
			logo_bounds = fz_bound_path(ctx, path, NULL, fz_identity);
			logo_tm = center_rect_within_rect(logo_bounds, rect);
			fz_fill_path(ctx, dev, path, 0, logo_tm, cs, logo_color, 1.0f, fz_default_color_params);
		}

		prect = rect;
		/* If there is to be info on the right then use only the left half of the rectangle for
		 * what is intended for the left */
		if (right_text)
			prect.x1 = (prect.x0 + prect.x1) / 2.0f;

		if (img)
		{
			float img_aspect = (float) img->w / img->h;
			float rectw = prect.x1 - prect.x0;
			float recth = prect.y1 - prect.y0;
			float midx = (prect.x0 + prect.x1) / 2.0;
			float midy = (prect.y0 + prect.y1) / 2.0;
			float rect_aspect = rectw / recth;
			float scale = img_aspect > rect_aspect ? rectw / img->w : recth / img->h;
			fz_matrix ctm = fz_pre_translate(fz_pre_scale(fz_translate(midx, midy), scale * img->w, scale * img->h), -0.5, -0.5);
			fz_fill_image(ctx, dev, img, ctm, 1.0, fz_default_color_params);
		}

		if (left_text)
		{
			text = pdf_layout_fit_text(ctx, font, lang, left_text, prect);
			fz_fill_text(ctx, dev, text, fz_identity, cs, color, 1.0f, fz_default_color_params);
			fz_drop_text(ctx, text);
			text = NULL;
		}

		prect = rect;
		/* If there is to be info on the left then use only the right half of the rectangle for
		 * what is intended for the right */
		if (img || left_text)
			prect.x0 = (prect.x0 + prect.x1) / 2.0f;

		if (right_text)
		{
			text = pdf_layout_fit_text(ctx, font, lang, right_text, prect);
			fz_fill_text(ctx, dev, text, fz_identity, cs, color, 1.0f, fz_default_color_params);
		}
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_path(ctx, path);
		fz_drop_text(ctx, text);
		fz_drop_font(ctx, font);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, dlist);
		fz_rethrow(ctx);
	}

	return dlist;
}

fz_display_list *
pdf_signature_appearance_unsigned(fz_context *ctx, fz_rect rect, fz_text_language lang)
{
	fz_display_list *dlist = NULL;
	fz_device *dev = NULL;
	fz_text *text = NULL;
	fz_colorspace *cs = NULL;
	fz_path *path = NULL;
	fz_font *font = NULL;

	fz_var(path);
	fz_var(dlist);
	fz_var(dev);
	fz_var(text);
	fz_var(font);
	fz_try(ctx)
	{
		float text_color[] = { 1.0f, 1.0f, 1.0f };
		float arrow_color[] = { 0.95f, 0.33f, 0.18f };

		dlist = fz_new_display_list(ctx, rect);
		dev = fz_new_list_device(ctx, dlist);

		rect.y1 = rect.y0 + (rect.y1 - rect.y0) / 6;
		rect.x1 = rect.x0 + (rect.y1 - rect.y0) * 4;
		font = fz_new_base14_font(ctx, "Helvetica");

		path = fz_new_path(ctx);
		/* Draw a rectangle with a protusion to the right [xxxxx> */
		fz_moveto(ctx, path, rect.x0, rect.y0);
		fz_lineto(ctx, path, rect.x1, rect.y0);
		fz_lineto(ctx, path, rect.x1 + (rect.y1 - rect.y0) / 2.0, (rect.y0 + rect.y1) / 2.0);
		fz_lineto(ctx, path, rect.x1, rect.y1);
		fz_lineto(ctx, path, rect.x0, rect.y1);
		fz_closepath(ctx, path);
		cs = fz_device_rgb(ctx);
		fz_fill_path(ctx, dev, path, 0, fz_identity, cs, arrow_color, 1.0f, fz_default_color_params);

		text = pdf_layout_fit_text(ctx, font, lang, "SIGN", rect);
		fz_fill_text(ctx, dev, text, fz_identity, cs, text_color, 1.0f, fz_default_color_params);
		fz_drop_text(ctx, text);
		text = NULL;
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_path(ctx, path);
		fz_drop_text(ctx, text);
		fz_drop_font(ctx, font);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, dlist);
		fz_rethrow(ctx);
	}

	return dlist;
}

char *
pdf_signature_info(fz_context *ctx, const char *name, pdf_pkcs7_distinguished_name *dn, const char *reason, const char *location, int64_t date, int include_labels)
{
	fz_buffer *fzbuf = NULL;
	char *dn_str = NULL;
	char *full_str = NULL;
	time_t tdate = (time_t)date;

	fz_var(fzbuf);
	fz_var(dn_str);
	fz_try(ctx)
	{
#ifdef _POSIX_SOURCE
		struct tm tmbuf, *tm = localtime_r(&tdate, &tmbuf);
#else
		struct tm *tm = localtime(&tdate);
#endif
		char now_str[40];
		size_t len = 0;
#ifdef CLUSTER
		memset(&date, 0, sizeof(date));
		memset(tm, 0, sizeof(*tm));
#endif

		fzbuf = fz_new_buffer(ctx, 256);
		if (name)
		{
			if (include_labels)
				fz_append_string(ctx, fzbuf, "Digitally signed by ");
			fz_append_string(ctx, fzbuf, name);
		}

		if (dn)
		{
			fz_append_string(ctx, fzbuf, "\n");
			if (include_labels)
				fz_append_string(ctx, fzbuf, "DN: ");
			dn_str = pdf_signature_format_distinguished_name(ctx, dn);
			fz_append_string(ctx, fzbuf, dn_str);
		}

		if (reason)
		{
			fz_append_string(ctx, fzbuf, "\n");
			if (include_labels)
				fz_append_string(ctx, fzbuf, "Reason: ");
			fz_append_string(ctx, fzbuf, reason);
		}

		if (location)
		{
			fz_append_string(ctx, fzbuf, "\n");
			if (include_labels)
				fz_append_string(ctx, fzbuf, "Location: ");
			fz_append_string(ctx, fzbuf, location);
		}

		if (date >= 0)
		{
			len = strftime(now_str, sizeof now_str, "%FT%T%z", tm);
			if (len)
			{
				fz_append_string(ctx, fzbuf, "\n");
				if (include_labels)
					fz_append_string(ctx, fzbuf, "Date: ");
				fz_append_string(ctx, fzbuf, now_str);
			}
		}

		fz_terminate_buffer(ctx, fzbuf);
		(void)fz_buffer_extract(ctx, fzbuf, (unsigned char **)&full_str);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, fzbuf);
		fz_free(ctx, dn_str);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return full_str;
}

void
pdf_annot_push_local_xref(fz_context *ctx, pdf_annot *annot)
{
	pdf_document *doc = annot->page->doc;

#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
	if (doc->local_xref_nesting == 0 && doc->local_xref)
		fz_write_printf(ctx, fz_stddbg(ctx), "push local_xref for annot\n");
#endif
	doc->local_xref_nesting++;
}

void
pdf_annot_ensure_local_xref(fz_context *ctx, pdf_annot *annot)
{
	pdf_document *doc = annot->page->doc;

	if (doc->local_xref != NULL)
		return;

#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
	fz_write_printf(ctx, fz_stddbg(ctx), "creating local_xref\n");
#endif

	/* We have no local_xref, but we want to be using one. */
	/* First off, create one. */
	doc->local_xref = pdf_new_local_xref(ctx, doc);
}

void
pdf_annot_pop_local_xref(fz_context *ctx, pdf_annot *annot)
{
	pdf_document *doc = annot->page->doc;

	--doc->local_xref_nesting;
#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
	if (doc->local_xref_nesting == 0 && doc->local_xref)
		fz_write_printf(ctx, fz_stddbg(ctx), "pop local_xref for annot\n");
#endif
}

void pdf_annot_pop_and_discard_local_xref(fz_context *ctx, pdf_annot *annot)
{
	pdf_document *doc = annot->page->doc;

#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
	if (doc->local_xref)
		fz_write_printf(ctx, fz_stddbg(ctx), "pop and discard local_xref for annot\n");
#endif
	--doc->local_xref_nesting;
	assert(doc->local_xref_nesting == 0);
	pdf_drop_local_xref_and_resources(ctx, doc);
}

static void pdf_update_appearance(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *subtype;
	pdf_obj *ft = NULL;
	pdf_obj *ap_n;
	int pop_local_xref = 1;

retry_after_repair:
	/* Must have any local xref in place in order to check if it's dirty. */
	pdf_annot_push_local_xref(ctx, annot);

	pdf_begin_implicit_operation(ctx, annot->page->doc);
	fz_start_throw_on_repair(ctx);

	fz_var(pop_local_xref);

	fz_try(ctx)
	{
		int needs_resynth;
		int local_synthesis = 0;

		/* Never update Popup and Link annotations */
		subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
		if (subtype == PDF_NAME(Popup) || subtype == PDF_NAME(Link))
			break;

		/* Never update signed Signature widgets */
		if (subtype == PDF_NAME(Widget))
		{
			ft = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(FT));
			if (ft == PDF_NAME(Sig))
			{
				/* We cannot synthesise an appearance for a signed Sig, so don't even try. */
				if (pdf_signature_is_signed(ctx, annot->page->doc, annot->obj))
					break;
			}
		}

		/* Check if the field is dirtied by JS events */
		if (pdf_obj_is_dirty(ctx, annot->obj))
			pdf_annot_request_resynthesis(ctx, annot);

		/* Find the current appearance stream, if one exists. */
		ap_n = pdf_annot_ap(ctx, annot);

		/* If there is no appearance stream, we need to create a local one for display purposes. */
		if (!ap_n)
			local_synthesis = 1;

		/* Ignore appearance streams not created by us (so not local)
		 * for unsigned digital signature widgets. They are often blank
		 * and we want the "sign" arrow to be visible. Never write back
		 * the forced appearance stream for unsigned signatures. */
		if (subtype == PDF_NAME(Widget) && ft == PDF_NAME(Sig))
		{
			if (ap_n && !pdf_is_local_object(ctx, annot->page->doc, ap_n))
				local_synthesis = 1;
		}

		/* We need to put this appearance stream back into the document. */
		needs_resynth = pdf_annot_needs_resynthesis(ctx, annot);
		if (needs_resynth)
			local_synthesis = 0;

		if (local_synthesis || needs_resynth)
		{
			fz_display_list *dlist;
			fz_rect rect, bbox;
			fz_buffer *buf;
			pdf_obj *res = NULL;
			pdf_obj *new_ap_n = NULL;
			fz_var(res);
			fz_var(new_ap_n);

#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
			fz_write_printf(ctx, fz_stddbg(ctx), "Update Appearance: %d\n", pdf_to_num(ctx, annot->obj));
			pdf_debug_obj(ctx, annot->obj);
			fz_write_printf(ctx, fz_stddbg(ctx), "\n");
#endif

			if (local_synthesis)
			{
#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
				fz_write_printf(ctx, fz_stddbg(ctx), "Local synthesis\n");
#endif
				pdf_annot_ensure_local_xref(ctx, annot);
			}
			else
			{
#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
				fz_write_printf(ctx, fz_stddbg(ctx), "Non-Local synthesis\n");
#endif
				/* We don't want to be using any local xref, so
				 * bin any that we have. */
				pdf_annot_pop_and_discard_local_xref(ctx, annot);
				/* Binning the xref may leave us holding pointers
				 * to the wrong versions of ap_n. */
				ap_n = pdf_annot_ap(ctx, annot);
				pop_local_xref = 0;
			}

			if (subtype == PDF_NAME(Widget) && ft == PDF_NAME(Btn))
			{
				/* Special case for Btn widgets that need multiple appearance streams. */
				pdf_update_button_appearance(ctx, annot);
			}
			else if (subtype == PDF_NAME(Widget) && ft == PDF_NAME(Sig))
			{
				/* Special case for unsigned signature widgets,
				 * which are most easily created via a display list. */
				rect = pdf_annot_rect(ctx, annot);
				dlist = pdf_signature_appearance_unsigned(ctx, rect, pdf_annot_language(ctx, annot));
				fz_try(ctx)
					pdf_set_annot_appearance_from_display_list(ctx, annot, "N", NULL, fz_identity, dlist);
				fz_always(ctx)
					fz_drop_display_list(ctx, dlist);
				fz_catch(ctx)
					fz_rethrow(ctx);
			}
			else
			{
				buf = fz_new_buffer(ctx, 1024);
				fz_try(ctx)
				{
					fz_matrix matrix = fz_identity;
					rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
					pdf_write_appearance(ctx, annot, buf, &rect, &bbox, &matrix, &res);
					pdf_dict_put_rect(ctx, annot->obj, PDF_NAME(Rect), rect);
					pdf_set_annot_appearance(ctx, annot, "N", NULL, matrix, bbox, res, buf);
				}
				fz_always(ctx)
				{
					fz_drop_buffer(ctx, buf);
					pdf_drop_obj(ctx, res);
					pdf_drop_obj(ctx, new_ap_n);
				}
				fz_catch(ctx)
				{
					fz_rethrow_if(ctx, FZ_ERROR_REPAIRED);
					fz_warn(ctx, "cannot create appearance stream");
				}
			}

#ifdef PDF_DEBUG_APPEARANCE_SYNTHESIS
			fz_write_printf(ctx, fz_stddbg(ctx), "Annot obj after synthesis\n");
			pdf_debug_obj(ctx, annot->obj);
			fz_write_printf(ctx, fz_stddbg(ctx), "\nAppearance after synthesis\n");
			pdf_debug_obj(ctx, pdf_dict_getp(ctx, annot->obj, "AP/N"));
			fz_write_printf(ctx, fz_stddbg(ctx), "\n");
#endif
		}

		pdf_clean_obj(ctx, annot->obj);
	}
	fz_always(ctx)
	{
		if (pop_local_xref)
			pdf_annot_pop_local_xref(ctx, annot);
		fz_end_throw_on_repair(ctx);
		pdf_end_operation(ctx, annot->page->doc);
	}
	fz_catch(ctx)
	{
		/* If we hit a repair while synthesising, we need to give it another
		 * go. Do that directly here, rather than waiting for the next time
		 * we are called, because we don't want to risk discarding any
		 * local_xrefs on the second pass through the list of annotations.
		 * Repairs only ever happen once for a document, so no infinite
		 * loop potential here. */
		if (fz_caught(ctx) == FZ_ERROR_REPAIRED)
			goto retry_after_repair;
		fz_rethrow(ctx);
	}
}

static void *
update_appearances(fz_context *ctx, fz_page *page_, void *state)
{
	pdf_page *page = (pdf_page *)page_;
	pdf_annot *annot;

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
		pdf_update_appearance(ctx, annot);
	for (annot = pdf_first_widget(ctx, page); annot; annot = pdf_next_widget(ctx, annot))
		pdf_update_appearance(ctx, annot);

	return NULL;
}

static void
update_all_appearances(fz_context *ctx, pdf_page *page)
{
	pdf_document *doc = page->doc;

	/* Update all the annotations on all the pages open in the document.
	 * At least one annotation should be resynthesised because we'll
	 * only reach here if resynth_required was set. Any such resynthesis
	 * that changes the document will throw away any local_xref. */
	fz_process_opened_pages(ctx, &doc->super, update_appearances, NULL);
	/* If the page isn't linked in yet (as is the case whilst loading
	 * the annots for a page), process that too. */
	if (page->super.prev == NULL && page->super.next == NULL)
		update_appearances(ctx, &page->super, NULL);

	/* Run it a second time, so that any annotations whose synthesised
	 * appearances live in the local_xref (such as unsigned sigs) can
	 * be regenerated too. Running this for annots which are up to date
	 * should be fast. */
	fz_process_opened_pages(ctx, &doc->super, update_appearances, NULL);
	/* And cope with a non-linked in page again. */
	if (page->super.prev == NULL && page->super.next == NULL)
		update_appearances(ctx, &page->super, NULL);

	doc->resynth_required = 0;
}

int
pdf_update_annot(fz_context *ctx, pdf_annot *annot)
{
	int changed;

	if (annot->page->doc->resynth_required)
		update_all_appearances(ctx, annot->page);

	changed = annot->has_new_ap;
	annot->has_new_ap = 0;
	return changed;
}
