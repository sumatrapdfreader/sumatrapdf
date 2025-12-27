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
#include "mupdf/pdf.h"

typedef struct
{
	pdf_processor super;
	fz_output *out;
	int ahxencode;
	int extgstate;
	int newlines;
	int balance;
	pdf_obj *res;
	pdf_obj *last_res;
	int sep;
} pdf_output_processor;

/* general graphics state */

static void
post_op(fz_context *ctx, pdf_output_processor *proc)
{
	if (proc->newlines)
		proc->sep = '\n';
	else
		proc->sep = 1;
}

static inline void separate(fz_context *ctx, pdf_output_processor *proc)
{
	if (!proc->sep)
		return;

	if (proc->sep == '\n')
		fz_write_byte(ctx, proc->out, '\n');
	else
		fz_write_byte(ctx, proc->out, ' ');
}

static void
pdf_out_w(fz_context *ctx, pdf_processor *proc_, float linewidth)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g w", linewidth);
	post_op(ctx, proc);
}

static void
pdf_out_j(fz_context *ctx, pdf_processor *proc_, int linejoin)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%d j", linejoin);
	post_op(ctx, proc);
}

static void
pdf_out_J(fz_context *ctx, pdf_processor *proc_, int linecap)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%d J", linecap);
	post_op(ctx, proc);
}

static void
pdf_out_M(fz_context *ctx, pdf_processor *proc_, float a)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g M", a);
	post_op(ctx, proc);
}

static void
pdf_out_d(fz_context *ctx, pdf_processor *proc_, pdf_obj *array, float phase)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;
	int ahx = proc->ahxencode;

	if (proc->extgstate != 0)
		return;

	pdf_print_encrypted_obj(ctx, proc->out, array, 1, ahx, NULL, 0, 0, &proc->sep);
	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g d", phase);
	post_op(ctx, proc);
}

static void
pdf_out_ri(fz_context *ctx, pdf_processor *proc_, const char *intent)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n ri", intent);
	post_op(ctx, proc);
}

static void
pdf_out_i(fz_context *ctx, pdf_processor *proc_, float flatness)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g i", flatness);
	post_op(ctx, proc);
}

static void
pdf_out_gs_begin(fz_context *ctx, pdf_processor *proc_, const char *name, pdf_obj *extgstate)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	proc->extgstate = 1;

	fz_write_printf(ctx, proc->out, "%n gs", name);
	post_op(ctx, proc);
}

static void
pdf_out_gs_end(fz_context *ctx, pdf_processor *proc)
{
	((pdf_output_processor*)proc)->extgstate = 0;
}

/* special graphics state */

static void
pdf_out_q(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	proc->balance++;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "q");
	post_op(ctx, proc);
}

static void
pdf_out_Q(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	proc->balance--;
	if (proc->balance < 0)
		fz_warn(ctx, "gstate underflow (too many Q operators)");

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "Q");
	post_op(ctx, proc);
}

static void
pdf_out_cm(fz_context *ctx, pdf_processor *proc_, float a, float b, float c, float d, float e, float f)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g %g %g cm", a, b, c, d, e, f);
	post_op(ctx, proc);
}

/* path construction */

static void
pdf_out_m(fz_context *ctx, pdf_processor *proc_, float x, float y)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g m", x, y);
	post_op(ctx, proc);
}

static void
pdf_out_l(fz_context *ctx, pdf_processor *proc_, float x, float y)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g l", x, y);
	post_op(ctx, proc);
}

static void
pdf_out_c(fz_context *ctx, pdf_processor *proc_, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g %g %g c", x1, y1, x2, y2, x3, y3);
	post_op(ctx, proc);
}

static void
pdf_out_v(fz_context *ctx, pdf_processor *proc_, float x2, float y2, float x3, float y3)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g v", x2, y2, x3, y3);
	post_op(ctx, proc);
}

static void
pdf_out_y(fz_context *ctx, pdf_processor *proc_, float x1, float y1, float x3, float y3)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g y", x1, y1, x3, y3);
	post_op(ctx, proc);
}

static void
pdf_out_h(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "h");
	post_op(ctx, proc);
}

static void
pdf_out_re(fz_context *ctx, pdf_processor *proc_, float x, float y, float w, float h)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g re", x, y, w, h);
	post_op(ctx, proc);
}

/* path painting */

static void
pdf_out_S(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "S");
	post_op(ctx, proc);
}

static void
pdf_out_s(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "s");
	post_op(ctx, proc);
}

static void
pdf_out_F(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "F");
	post_op(ctx, proc);
}

static void
pdf_out_f(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "f");
	post_op(ctx, proc);
}

static void
pdf_out_fstar(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "f*");
	post_op(ctx, proc);
}

static void
pdf_out_B(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "B");
	post_op(ctx, proc);
}

static void
pdf_out_Bstar(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "B*");
	post_op(ctx, proc);
}

static void
pdf_out_b(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "b");
	post_op(ctx, proc);
}

static void
pdf_out_bstar(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "b*");
	post_op(ctx, proc);
}

static void
pdf_out_n(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "n");
	post_op(ctx, proc);
}

/* clipping paths */

static void
pdf_out_W(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "W");
	post_op(ctx, proc);
}

static void
pdf_out_Wstar(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "W*");
	post_op(ctx, proc);
}

/* text objects */

static void
pdf_out_BT(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "BT");
	post_op(ctx, proc);
}

static void
pdf_out_ET(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "ET");
	post_op(ctx, proc);
}

/* text state */

static void
pdf_out_Tc(fz_context *ctx, pdf_processor *proc_, float charspace)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g Tc", charspace);
	post_op(ctx, proc);
}

static void
pdf_out_Tw(fz_context *ctx, pdf_processor *proc_, float wordspace)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g Tw", wordspace);
	post_op(ctx, proc);
}

static void
pdf_out_Tz(fz_context *ctx, pdf_processor *proc_, float scale)
{
	/* scale is exactly as read from the file. */
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g Tz", scale);
	post_op(ctx, proc);
}

static void
pdf_out_TL(fz_context *ctx, pdf_processor *proc_, float leading)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g TL", leading);
	post_op(ctx, proc);
}

static void
pdf_out_Tf(fz_context *ctx, pdf_processor *proc_, const char *name, pdf_font_desc *font, float size)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	if (proc->extgstate != 0)
		return;

	fz_write_printf(ctx, proc->out, "%n %g Tf", name, size);
	post_op(ctx, proc);
}

static void
pdf_out_Tr(fz_context *ctx, pdf_processor *proc_, int render)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%d Tr", render);
	post_op(ctx, proc);
}

static void
pdf_out_Ts(fz_context *ctx, pdf_processor *proc_, float rise)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g Ts", rise);
	post_op(ctx, proc);
}

/* text positioning */

static void
pdf_out_Td(fz_context *ctx, pdf_processor *proc_, float tx, float ty)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g Td", tx, ty);
	post_op(ctx, proc);
}

static void
pdf_out_TD(fz_context *ctx, pdf_processor *proc_, float tx, float ty)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g TD", tx, ty);
	post_op(ctx, proc);
}

static void
pdf_out_Tm(fz_context *ctx, pdf_processor *proc_, float a, float b, float c, float d, float e, float f)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g %g %g Tm", a, b, c, d, e, f);
	post_op(ctx, proc);
}

static void
pdf_out_Tstar(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "T*");
	post_op(ctx, proc);
}

/* text showing */

static void
fz_write_pdf_string(fz_context *ctx, fz_output *out, const unsigned char *str, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		if (str[i] < 32 || str[i] >= 127)
			break;

	if (i < len)
	{
		fz_write_byte(ctx, out, '<');
		for (i = 0; i < len; ++i)
		{
			unsigned char c = str[i];
			fz_write_byte(ctx, out, "0123456789abcdef"[(c>>4)&15]);
			fz_write_byte(ctx, out, "0123456789abcdef"[(c)&15]);
		}
		fz_write_byte(ctx, out, '>');
	}
	else
	{
		fz_write_byte(ctx, out, '(');
		for (i = 0; i < len; ++i)
		{
			unsigned char c = str[i];
			if (c == '(' || c == ')' || c == '\\')
				fz_write_byte(ctx, out, '\\');
			fz_write_byte(ctx, out, c);
		}
		fz_write_byte(ctx, out, ')');
	}
}

static void
pdf_out_TJ(fz_context *ctx, pdf_processor *proc_, pdf_obj *array)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;
	int ahx = proc->ahxencode;

	pdf_print_encrypted_obj(ctx, proc->out, array, 1, ahx, NULL, 0, 0, &proc->sep);
	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "TJ");
	post_op(ctx, proc);
}

static void
pdf_out_Tj(fz_context *ctx, pdf_processor *proc_, char *str, size_t len)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_pdf_string(ctx, proc->out, (const unsigned char *)str, len);
	fz_write_string(ctx, proc->out, "Tj");
	post_op(ctx, proc);
}

static void
pdf_out_squote(fz_context *ctx, pdf_processor *proc_, char *str, size_t len)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_pdf_string(ctx, proc->out, (const unsigned char *)str, len);
	fz_write_string(ctx, proc->out, "'");
	post_op(ctx, proc);
}

static void
pdf_out_dquote(fz_context *ctx, pdf_processor *proc_, float aw, float ac, char *str, size_t len)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g ", aw, ac);
	fz_write_pdf_string(ctx, proc->out, (const unsigned char *)str, len);
	fz_write_string(ctx, proc->out, "\"");
	post_op(ctx, proc);
}

/* type 3 fonts */

static void
pdf_out_d0(fz_context *ctx, pdf_processor *proc_, float wx, float wy)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g d0", wx, wy);
	post_op(ctx, proc);
}

static void
pdf_out_d1(fz_context *ctx, pdf_processor *proc_, float wx, float wy, float llx, float lly, float urx, float ury)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g %g %g d1", wx, wy, llx, lly, urx, ury);
	post_op(ctx, proc);
}

/* color */

static void
pdf_out_CS(fz_context *ctx, pdf_processor *proc_, const char *name, fz_colorspace *cs)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n CS", name);
	post_op(ctx, proc);
}

static void
pdf_out_cs(fz_context *ctx, pdf_processor *proc_, const char *name, fz_colorspace *cs)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n cs", name);
	post_op(ctx, proc);
}

static void
pdf_out_SC_pattern(fz_context *ctx, pdf_processor *proc_, const char *name, pdf_pattern *pat, int n, float *color)
{
	int i;
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, proc->out, "%g ", color[i]);
	fz_write_printf(ctx, proc->out, "%n SCN", name);
	post_op(ctx, proc);
}

static void
pdf_out_sc_pattern(fz_context *ctx, pdf_processor *proc_, const char *name, pdf_pattern *pat, int n, float *color)
{
	int i;
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, proc->out, "%g ", color[i]);
	fz_write_printf(ctx, proc->out, "%n scn", name);
	post_op(ctx, proc);
}

static void
pdf_out_SC_shade(fz_context *ctx, pdf_processor *proc_, const char *name, fz_shade *shade)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n SCN", name);
	post_op(ctx, proc);
}

static void
pdf_out_sc_shade(fz_context *ctx, pdf_processor *proc_, const char *name, fz_shade *shade)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n scn", name);
	post_op(ctx, proc);
}

static void
pdf_out_SC_color(fz_context *ctx, pdf_processor *proc_, int n, float *color)
{
	int i;
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, proc->out, "%g ", color[i]);
	fz_write_string(ctx, proc->out, "SCN");
	post_op(ctx, proc);
}

static void
pdf_out_sc_color(fz_context *ctx, pdf_processor *proc_, int n, float *color)
{
	int i;
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, proc->out, "%g ", color[i]);
	fz_write_string(ctx, proc->out, "scn");
	post_op(ctx, proc);
}

static void
pdf_out_G(fz_context *ctx, pdf_processor *proc_, float g)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g G", g);
	post_op(ctx, proc);
}

static void
pdf_out_g(fz_context *ctx, pdf_processor *proc_, float g)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g g", g);
	post_op(ctx, proc);
}

static void
pdf_out_RG(fz_context *ctx, pdf_processor *proc_, float r, float g, float b)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g RG", r, g, b);
	post_op(ctx, proc);
}

static void
pdf_out_rg(fz_context *ctx, pdf_processor *proc_, float r, float g, float b)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g rg", r, g, b);
	post_op(ctx, proc);
}

static void
pdf_out_K(fz_context *ctx, pdf_processor *proc_, float c, float m, float y, float k)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g K", c, m, y, k);
	post_op(ctx, proc);
}

static void
pdf_out_k(fz_context *ctx, pdf_processor *proc_, float c, float m, float y, float k)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%g %g %g %g k", c, m, y, k);
	post_op(ctx, proc);
}

/* shadings, images, xobjects */

static void
pdf_out_BI(fz_context *ctx, pdf_processor *proc_, fz_image *img, const char *colorspace)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;
	fz_output *out = proc->out;
	int ahx = proc->ahxencode;
	fz_compressed_buffer *cbuf;
	fz_buffer *buf = NULL;
	int i, w, h, bpc;
	unsigned char *data;
	size_t len;
	fz_pixmap *pix = NULL;
	fz_colorspace *cs;
	int type;

	if (img == NULL)
		return;
	cbuf = fz_compressed_image_buffer(ctx, img);
	if (cbuf == NULL)
	{
		pix = fz_get_pixmap_from_image(ctx, img, NULL, NULL, &w, &h);
		bpc = 8;
		cs = pix->colorspace;
		type = FZ_IMAGE_RAW;
	}
	else
	{
		buf = cbuf->buffer;
		if (buf == NULL)
			return;
		w = img->w;
		h = img->h;
		bpc = img->bpc;
		cs = img->colorspace;
		type = cbuf->params.type;
	}

	fz_try(ctx)
	{
		separate(ctx, proc);
		fz_write_string(ctx, out, "BI ");
		fz_write_printf(ctx, out, "/W %d", w);
		fz_write_printf(ctx, out, "/H %d", h);
		fz_write_printf(ctx, out, "/BPC %d", bpc);
		if (img->imagemask)
			fz_write_string(ctx, out, "/IM true");
		else if (cs == fz_device_gray(ctx))
			fz_write_string(ctx, out, "/CS/G");
		else if (cs == fz_device_rgb(ctx))
			fz_write_string(ctx, out, "/CS/RGB");
		else if (cs == fz_device_cmyk(ctx))
			fz_write_string(ctx, out, "/CS/CMYK");
		else if (cs)
			fz_write_printf(ctx, out, "/CS%n", colorspace);
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "BI operator can only show ImageMask, Gray, RGB, or CMYK images");
		if (img->interpolate)
			fz_write_string(ctx, out, "/I true");
		fz_write_string(ctx, out, "/D[");
		for (i = 0; i < img->n * 2; ++i)
		{
			if (i > 0)
				fz_write_byte(ctx, out, ' ');
			fz_write_printf(ctx, out, "%g", img->decode[i]);
		}
		fz_write_string(ctx, out, "]");
		proc->sep = 0;

		switch (type)
		{
		default:
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "unknown compressed buffer type");
			break;

		case FZ_IMAGE_JPEG:
			fz_write_string(ctx, out, ahx ? "/F[/AHx/DCT]" : "/F/DCT");
			proc->sep = !ahx;
			if (cbuf->params.u.jpeg.color_transform >= 0)
			{
				fz_write_printf(ctx, out, "/DP<</ColorTransform %d>>", cbuf->params.u.jpeg.color_transform);
				proc->sep = 0;
			}
			if (cbuf->params.u.jpeg.invert_cmyk && img->n == 4)
			{
				fz_write_string(ctx, out, "/D[1 0 1 0 1 0 1 0]");
				proc->sep = 0;
			}
			break;

		case FZ_IMAGE_FAX:
			fz_write_string(ctx, out, ahx ? "/F[/AHx/CCF]/DP[null<<" : "/F/CCF/DP<<");
			fz_write_printf(ctx, out, "/K %d", cbuf->params.u.fax.k);
			if (cbuf->params.u.fax.columns != 1728)
				fz_write_printf(ctx, out, "/Columns %d", cbuf->params.u.fax.columns);
			if (cbuf->params.u.fax.rows > 0)
				fz_write_printf(ctx, out, "/Rows %d", cbuf->params.u.fax.rows);
			if (cbuf->params.u.fax.end_of_line)
				fz_write_string(ctx, out, "/EndOfLine true");
			if (cbuf->params.u.fax.encoded_byte_align)
				fz_write_string(ctx, out, "/EncodedByteAlign true");
			if (!cbuf->params.u.fax.end_of_block)
				fz_write_string(ctx, out, "/EndOfBlock false");
			if (cbuf->params.u.fax.black_is_1)
				fz_write_string(ctx, out, "/BlackIs1 true");
			if (cbuf->params.u.fax.damaged_rows_before_error > 0)
				fz_write_printf(ctx, out, "/DamagedRowsBeforeError %d",
					cbuf->params.u.fax.damaged_rows_before_error);
			fz_write_string(ctx, out, ahx ? ">>]" : ">>");
			proc->sep = 0;
			break;

		case FZ_IMAGE_RAW:
			if (ahx)
			{
				fz_write_string(ctx, out, "/F/AHx");
				proc->sep = 1;
			}
			break;

		case FZ_IMAGE_RLD:
			fz_write_string(ctx, out, ahx ? "/F[/AHx/RL]" : "/F/RL");
			proc->sep = !ahx;
			break;

		case FZ_IMAGE_FLATE:
			fz_write_string(ctx, out, ahx ? "/F[/AHx/Fl]" : "/F/Fl");
			proc->sep = !ahx;
			if (cbuf->params.u.flate.predictor > 1)
			{
				fz_write_string(ctx, out, ahx ? "/DP[null<<" : "/DP<<");
				fz_write_printf(ctx, out, "/Predictor %d", cbuf->params.u.flate.predictor);
				if (cbuf->params.u.flate.columns != 1)
					fz_write_printf(ctx, out, "/Columns %d", cbuf->params.u.flate.columns);
				if (cbuf->params.u.flate.colors != 1)
					fz_write_printf(ctx, out, "/Colors %d", cbuf->params.u.flate.colors);
				if (cbuf->params.u.flate.bpc != 8)
					fz_write_printf(ctx, out, "/BitsPerComponent %d", cbuf->params.u.flate.bpc);
				fz_write_string(ctx, out, ahx ? ">>]" : ">>");
				proc->sep = 0;
			}
			break;

		case FZ_IMAGE_LZW:
			fz_write_string(ctx, out, ahx ? "/F[/AHx/LZW]" : "/F/LZW");
			proc->sep = !ahx;
			if (cbuf->params.u.lzw.predictor > 1)
			{
				fz_write_string(ctx, out, ahx ? "/DP[null<<" : "/DP<<");
				fz_write_printf(ctx, out, "/Predictor %d", cbuf->params.u.lzw.predictor);
				if (cbuf->params.u.lzw.columns != 1)
					fz_write_printf(ctx, out, "/Columns %d", cbuf->params.u.lzw.columns);
				if (cbuf->params.u.lzw.colors != 1)
					fz_write_printf(ctx, out, "/Colors %d", cbuf->params.u.lzw.colors);
				if (cbuf->params.u.lzw.bpc != 8)
					fz_write_printf(ctx, out, "/BitsPerComponent %d", cbuf->params.u.lzw.bpc);
				if (cbuf->params.u.lzw.early_change != 1)
					fz_write_printf(ctx, out, "/EarlyChange %d", cbuf->params.u.lzw.early_change);
				fz_write_string(ctx, out, ahx ? ">>]" : ">>");
				proc->sep = 0;
			}
			break;

		case FZ_IMAGE_BROTLI:
			fz_write_string(ctx, out, ahx ? "/F[/AHx/Br]\n" : "/F/Br\n");
			if (cbuf->params.u.brotli.predictor > 1)
			{
				fz_write_string(ctx, out, ahx ? "/DP[null<<\n" : "/DP<<\n");
				fz_write_printf(ctx, out, "/Predictor %d\n", cbuf->params.u.brotli.predictor);
				if (cbuf->params.u.brotli.columns != 1)
					fz_write_printf(ctx, out, "/Columns %d\n", cbuf->params.u.brotli.columns);
				if (cbuf->params.u.brotli.colors != 1)
					fz_write_printf(ctx, out, "/Colors %d\n", cbuf->params.u.brotli.colors);
				if (cbuf->params.u.brotli.bpc != 8)
					fz_write_printf(ctx, out, "/BitsPerComponent %d\n", cbuf->params.u.brotli.bpc);
				fz_write_string(ctx, out, ahx ? ">>]\n" : ">>\n");
			}
			break;
		}

		separate(ctx, proc);
		fz_write_string(ctx, out, "ID ");
		if (buf)
			len = fz_buffer_storage(ctx, buf, &data);
		else
		{
			data = pix->samples;
			len = ((size_t)w) * h * pix->n;
		}
		if (ahx)
		{
			size_t z;
			for (z = 0; z < len; ++z)
			{
				int c = data[z];
				fz_write_byte(ctx, out, "0123456789abcdef"[(c >> 4) & 0xf]);
				fz_write_byte(ctx, out, "0123456789abcdef"[c & 0xf]);
				if ((z & 31) == 31)
					fz_write_byte(ctx, out, '\n');
			}
			fz_write_byte(ctx, out, '>');
		}
		else
		{
			fz_write_data(ctx, out, data, len);
		}
		fz_write_string(ctx, out, " EI");
		proc->sep = 1;
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pix);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_out_sh(fz_context *ctx, pdf_processor *proc_, const char *name, fz_shade *shade)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n sh", name);
	post_op(ctx, proc);
}

static void
pdf_out_Do_image(fz_context *ctx, pdf_processor *proc_, const char *name, fz_image *image)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n Do", name);
	post_op(ctx, proc);
}

static void
pdf_out_Do_form(fz_context *ctx, pdf_processor *proc_, const char *name, pdf_obj *xobj)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n Do", name);
	post_op(ctx, proc);
}

/* marked content */

static void
pdf_out_MP(fz_context *ctx, pdf_processor *proc_, const char *tag)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n MP", tag);
	post_op(ctx, proc);
}

static void
pdf_out_DP(fz_context *ctx, pdf_processor *proc_, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;
	int ahx = proc->ahxencode;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n", tag);
	proc->sep = 1;
	pdf_print_encrypted_obj(ctx, proc->out, raw, 1, ahx, NULL, 0, 0, &proc->sep);
	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "DP");
	post_op(ctx, proc);
}

static void
pdf_out_BMC(fz_context *ctx, pdf_processor *proc_, const char *tag)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n BMC", tag);
	post_op(ctx, proc);
}

static void
pdf_out_BDC(fz_context *ctx, pdf_processor *proc_, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;
	int ahx = proc->ahxencode;

	separate(ctx, proc);
	fz_write_printf(ctx, proc->out, "%n", tag);
	proc->sep = 1;
	pdf_print_encrypted_obj(ctx, proc->out, raw, 1, ahx, NULL, 0, 0, &proc->sep);
	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "BDC");
	post_op(ctx, proc);
}

static void
pdf_out_EMC(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "EMC");
	post_op(ctx, proc);
}

/* compatibility */

static void
pdf_out_BX(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "BX");
	post_op(ctx, proc);
}

static void
pdf_out_EX(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor *)proc_;

	separate(ctx, proc);
	fz_write_string(ctx, proc->out, "EX");
	post_op(ctx, proc);
}

static void
pdf_close_output_processor(fz_context *ctx, pdf_processor *proc_)
{
	pdf_output_processor *proc = (pdf_output_processor*)proc_;
	fz_output *out = proc->out;

	/* Add missing 'Q' operators to get back to zero. */
	/* We can't prepend missing 'q' operators to guarantee we don't underflow. */
	while (proc->balance > 0)
	{
		proc->balance--;
		separate(ctx, proc);
		fz_write_byte(ctx, out, 'Q');
		post_op(ctx, proc);
	}

	fz_close_output(ctx, out);
}

static void
pdf_drop_output_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_output_processor *p = (pdf_output_processor *)proc;

	fz_drop_output(ctx, p->out);
}

static void
pdf_reset_output_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_output_processor *p = (pdf_output_processor *)proc;

	fz_reset_output(ctx, p->out);
}

pdf_processor *
pdf_new_output_processor(fz_context *ctx, fz_output *out, int ahxencode, int newlines)
{
	pdf_output_processor *proc = pdf_new_processor(ctx, sizeof *proc);

	proc->super.close_processor = pdf_close_output_processor;
	proc->super.drop_processor = pdf_drop_output_processor;
	proc->super.reset_processor = pdf_reset_output_processor;

	/* general graphics state */
	proc->super.op_w = pdf_out_w;
	proc->super.op_j = pdf_out_j;
	proc->super.op_J = pdf_out_J;
	proc->super.op_M = pdf_out_M;
	proc->super.op_d = pdf_out_d;
	proc->super.op_ri = pdf_out_ri;
	proc->super.op_i = pdf_out_i;
	proc->super.op_gs_begin = pdf_out_gs_begin;
	proc->super.op_gs_end = pdf_out_gs_end;

	/* transparency graphics state */
	proc->super.op_gs_BM = NULL;
	proc->super.op_gs_CA = NULL;
	proc->super.op_gs_ca = NULL;
	proc->super.op_gs_SMask = NULL;

	/* special graphics state */
	proc->super.op_q = pdf_out_q;
	proc->super.op_Q = pdf_out_Q;
	proc->super.op_cm = pdf_out_cm;

	/* path construction */
	proc->super.op_m = pdf_out_m;
	proc->super.op_l = pdf_out_l;
	proc->super.op_c = pdf_out_c;
	proc->super.op_v = pdf_out_v;
	proc->super.op_y = pdf_out_y;
	proc->super.op_h = pdf_out_h;
	proc->super.op_re = pdf_out_re;

	/* path painting */
	proc->super.op_S = pdf_out_S;
	proc->super.op_s = pdf_out_s;
	proc->super.op_F = pdf_out_F;
	proc->super.op_f = pdf_out_f;
	proc->super.op_fstar = pdf_out_fstar;
	proc->super.op_B = pdf_out_B;
	proc->super.op_Bstar = pdf_out_Bstar;
	proc->super.op_b = pdf_out_b;
	proc->super.op_bstar = pdf_out_bstar;
	proc->super.op_n = pdf_out_n;

	/* clipping paths */
	proc->super.op_W = pdf_out_W;
	proc->super.op_Wstar = pdf_out_Wstar;

	/* text objects */
	proc->super.op_BT = pdf_out_BT;
	proc->super.op_ET = pdf_out_ET;

	/* text state */
	proc->super.op_Tc = pdf_out_Tc;
	proc->super.op_Tw = pdf_out_Tw;
	proc->super.op_Tz = pdf_out_Tz;
	proc->super.op_TL = pdf_out_TL;
	proc->super.op_Tf = pdf_out_Tf;
	proc->super.op_Tr = pdf_out_Tr;
	proc->super.op_Ts = pdf_out_Ts;

	/* text positioning */
	proc->super.op_Td = pdf_out_Td;
	proc->super.op_TD = pdf_out_TD;
	proc->super.op_Tm = pdf_out_Tm;
	proc->super.op_Tstar = pdf_out_Tstar;

	/* text showing */
	proc->super.op_TJ = pdf_out_TJ;
	proc->super.op_Tj = pdf_out_Tj;
	proc->super.op_squote = pdf_out_squote;
	proc->super.op_dquote = pdf_out_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = pdf_out_d0;
	proc->super.op_d1 = pdf_out_d1;

	/* color */
	proc->super.op_CS = pdf_out_CS;
	proc->super.op_cs = pdf_out_cs;
	proc->super.op_SC_color = pdf_out_SC_color;
	proc->super.op_sc_color = pdf_out_sc_color;
	proc->super.op_SC_pattern = pdf_out_SC_pattern;
	proc->super.op_sc_pattern = pdf_out_sc_pattern;
	proc->super.op_SC_shade = pdf_out_SC_shade;
	proc->super.op_sc_shade = pdf_out_sc_shade;

	proc->super.op_G = pdf_out_G;
	proc->super.op_g = pdf_out_g;
	proc->super.op_RG = pdf_out_RG;
	proc->super.op_rg = pdf_out_rg;
	proc->super.op_K = pdf_out_K;
	proc->super.op_k = pdf_out_k;

	/* shadings, images, xobjects */
	proc->super.op_BI = pdf_out_BI;
	proc->super.op_sh = pdf_out_sh;
	proc->super.op_Do_image = pdf_out_Do_image;
	proc->super.op_Do_form = pdf_out_Do_form;

	/* marked content */
	proc->super.op_MP = pdf_out_MP;
	proc->super.op_DP = pdf_out_DP;
	proc->super.op_BMC = pdf_out_BMC;
	proc->super.op_BDC = pdf_out_BDC;
	proc->super.op_EMC = pdf_out_EMC;

	/* compatibility */
	proc->super.op_BX = pdf_out_BX;
	proc->super.op_EX = pdf_out_EX;

	/* extgstate */
	proc->super.op_gs_OP = NULL;
	proc->super.op_gs_op = NULL;
	proc->super.op_gs_OPM = NULL;
	proc->super.op_gs_UseBlackPtComp = NULL;

	proc->out = out;
	proc->ahxencode = ahxencode;
	proc->newlines = newlines;

	proc->super.requirements = PDF_PROCESSOR_REQUIRES_DECODED_IMAGES;

	proc->balance = 0;

	return (pdf_processor*)proc;
}

pdf_processor *
pdf_new_buffer_processor(fz_context *ctx, fz_buffer *buffer, int ahxencode, int newlines)
{
	pdf_processor *proc = NULL;
	fz_output *out = fz_new_output_with_buffer(ctx, buffer);
	fz_try(ctx)
	{
		proc = pdf_new_output_processor(ctx, out, ahxencode, newlines);
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return proc;
}

/* Simplified processor that only counts matching q/Q pairs. */

typedef struct
{
	pdf_processor super;
	int *balance;
	int *min_q;
	int *min_op_q;
	int first;
	int ending;
} pdf_balance_processor;

static void
pdf_balance_q(fz_context *ctx, pdf_processor *proc_)
{
	pdf_balance_processor *proc = (pdf_balance_processor*)proc_;
	(*proc->balance)++;
}

static void
pdf_balance_Q(fz_context *ctx, pdf_processor *proc_)
{
	pdf_balance_processor *proc = (pdf_balance_processor*)proc_;

	if (proc->ending)
		return;

	(*proc->balance)--;
	if (*proc->balance < *proc->min_q)
		*proc->min_q = *proc->balance;
}

static void
pdf_balance_void(fz_context *ctx, pdf_processor *proc_)
{
	pdf_balance_processor *proc = (pdf_balance_processor*)proc_;
	if (*proc->balance < *proc->min_op_q)
		*proc->min_op_q = *proc->balance;
}

#define BALANCE { pdf_balance_void(ctx, p); }

static void pdf_balance_string(fz_context *ctx, pdf_processor *p, const char *x) BALANCE
static void pdf_balance_int(fz_context *ctx, pdf_processor *p, int x) BALANCE
static void pdf_balance_float(fz_context *ctx, pdf_processor *p, float x) BALANCE
static void pdf_balance_float2(fz_context *ctx, pdf_processor *p, float x, float y) BALANCE
static void pdf_balance_float3(fz_context *ctx, pdf_processor *p, float x, float y, float z) BALANCE
static void pdf_balance_float4(fz_context *ctx, pdf_processor *p, float x, float y, float z, float w) BALANCE
static void pdf_balance_float6(fz_context *ctx, pdf_processor *p, float a, float b, float c, float d, float e, float f) BALANCE

static void pdf_balance_d(fz_context *ctx, pdf_processor *p, pdf_obj *array, float phase) BALANCE
static void pdf_balance_gs_begin(fz_context *ctx, pdf_processor *p, const char *name, pdf_obj *extgstate) BALANCE
static void pdf_balance_Tf(fz_context *ctx, pdf_processor *p, const char *name, pdf_font_desc *font, float size) BALANCE
static void pdf_balance_TJ(fz_context *ctx, pdf_processor *p, pdf_obj *array) BALANCE
static void pdf_balance_Tj(fz_context *ctx, pdf_processor *p, char *str, size_t len) BALANCE
static void pdf_balance_squote(fz_context *ctx, pdf_processor *p, char *str, size_t len) BALANCE
static void pdf_balance_dquote(fz_context *ctx, pdf_processor *p, float aw, float ac, char *str, size_t len) BALANCE
static void pdf_balance_cs(fz_context *ctx, pdf_processor *p, const char *name, fz_colorspace *cs) BALANCE
static void pdf_balance_sc_pattern(fz_context *ctx, pdf_processor *p, const char *name, pdf_pattern *pat, int n, float *color) BALANCE
static void pdf_balance_sc_shade(fz_context *ctx, pdf_processor *p, const char *name, fz_shade *shade) BALANCE
static void pdf_balance_sc_color(fz_context *ctx, pdf_processor *p, int n, float *color) BALANCE
static void pdf_balance_BDC(fz_context *ctx, pdf_processor *p, const char *tag, pdf_obj *raw, pdf_obj *cooked) BALANCE
static void pdf_balance_BI(fz_context *ctx, pdf_processor *p, fz_image *img, const char *colorspace) BALANCE
static void pdf_balance_sh(fz_context *ctx, pdf_processor *p, const char *name, fz_shade *shade) BALANCE
static void pdf_balance_Do_image(fz_context *ctx, pdf_processor *p, const char *name, fz_image *image) BALANCE
static void pdf_balance_Do_form(fz_context *ctx, pdf_processor *p, const char *name, pdf_obj *xobj) BALANCE

static void pdf_balance_EOD(fz_context *ctx, pdf_processor *p)
{
	pdf_balance_processor *proc = (pdf_balance_processor *)p;

	proc->ending = 1;
}

static pdf_processor *
pdf_new_balance_processor(fz_context *ctx, int *balance, int *min_q, int *min_op_q)
{
	pdf_balance_processor *proc = pdf_new_processor(ctx, sizeof *proc);

	proc->super.op_q = pdf_balance_q;
	proc->super.op_Q = pdf_balance_Q;

	/* general graphics state */
	proc->super.op_w = pdf_balance_float;
	proc->super.op_j = pdf_balance_int;
	proc->super.op_J = pdf_balance_int;
	proc->super.op_M = pdf_balance_float;
	proc->super.op_d = pdf_balance_d;
	proc->super.op_ri = pdf_balance_string;
	proc->super.op_i = pdf_balance_float;
	proc->super.op_gs_begin = pdf_balance_gs_begin;

	/* special graphics state */
	proc->super.op_cm = pdf_balance_float6;

	/* path construction */
	proc->super.op_m = pdf_balance_float2;
	proc->super.op_l = pdf_balance_float2;
	proc->super.op_c = pdf_balance_float6;
	proc->super.op_v = pdf_balance_float4;
	proc->super.op_y = pdf_balance_float4;
	proc->super.op_h = pdf_balance_void;
	proc->super.op_re = pdf_balance_float4;

	/* path painting */
	proc->super.op_S = pdf_balance_void;
	proc->super.op_s = pdf_balance_void;
	proc->super.op_F = pdf_balance_void;
	proc->super.op_f = pdf_balance_void;
	proc->super.op_fstar = pdf_balance_void;
	proc->super.op_B = pdf_balance_void;
	proc->super.op_Bstar = pdf_balance_void;
	proc->super.op_b = pdf_balance_void;
	proc->super.op_bstar = pdf_balance_void;
	proc->super.op_n = pdf_balance_void;

	/* clipping paths */
	proc->super.op_W = pdf_balance_void;
	proc->super.op_Wstar = pdf_balance_void;

	/* text objects */
	proc->super.op_BT = pdf_balance_void;
	proc->super.op_ET = pdf_balance_void;

	/* text state */
	proc->super.op_Tc = pdf_balance_float;
	proc->super.op_Tw = pdf_balance_float;
	proc->super.op_Tz = pdf_balance_float;
	proc->super.op_TL = pdf_balance_float;
	proc->super.op_Tf = pdf_balance_Tf;
	proc->super.op_Tr = pdf_balance_int;
	proc->super.op_Ts = pdf_balance_float;

	/* text positioning */
	proc->super.op_Td = pdf_balance_float2;
	proc->super.op_TD = pdf_balance_float2;
	proc->super.op_Tm = pdf_balance_float6;
	proc->super.op_Tstar = pdf_balance_void;

	/* text showing */
	proc->super.op_TJ = pdf_balance_TJ;
	proc->super.op_Tj = pdf_balance_Tj;
	proc->super.op_squote = pdf_balance_squote;
	proc->super.op_dquote = pdf_balance_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = pdf_balance_float2;
	proc->super.op_d1 = pdf_balance_float6;

	/* color */
	proc->super.op_CS = pdf_balance_cs;
	proc->super.op_cs = pdf_balance_cs;
	proc->super.op_SC_color = pdf_balance_sc_color;
	proc->super.op_sc_color = pdf_balance_sc_color;
	proc->super.op_SC_pattern = pdf_balance_sc_pattern;
	proc->super.op_sc_pattern = pdf_balance_sc_pattern;
	proc->super.op_SC_shade = pdf_balance_sc_shade;
	proc->super.op_sc_shade = pdf_balance_sc_shade;

	proc->super.op_G = pdf_balance_float;
	proc->super.op_g = pdf_balance_float;
	proc->super.op_RG = pdf_balance_float3;
	proc->super.op_rg = pdf_balance_float3;
	proc->super.op_K = pdf_balance_float4;
	proc->super.op_k = pdf_balance_float4;

	/* shadings, images, xobjects */
	proc->super.op_BI = pdf_balance_BI;
	proc->super.op_sh = pdf_balance_sh;
	proc->super.op_Do_image = pdf_balance_Do_image;
	proc->super.op_Do_form = pdf_balance_Do_form;

	/* marked content */
	proc->super.op_MP = pdf_balance_string;
	proc->super.op_DP = pdf_balance_BDC;
	proc->super.op_BMC = pdf_balance_string;
	proc->super.op_BDC = pdf_balance_BDC;
	proc->super.op_EMC = pdf_balance_void;

	/* compatibility */
	proc->super.op_BX = pdf_balance_void;
	proc->super.op_EX = pdf_balance_void;

	proc->super.op_EOD = pdf_balance_EOD;

	proc->balance = balance;
	proc->min_q = min_q;
	proc->min_op_q = min_op_q;

	return (pdf_processor*)proc;
}

void
pdf_count_q_balance(fz_context *ctx, pdf_document *doc, pdf_obj *res, pdf_obj *stm, int *prepend, int *append)
{
	pdf_processor *proc;

	int end_q = 0;
	int min_q = 0;
	int min_op_q = 1;

	proc = pdf_new_balance_processor(ctx, &end_q, &min_q, &min_op_q);
	fz_try(ctx)
	{
		pdf_process_contents(ctx, proc, doc, res, stm, NULL, NULL);
		pdf_close_processor(ctx, proc);
	}
	fz_always(ctx)
		pdf_drop_processor(ctx, proc);
	fz_catch(ctx)
		fz_rethrow(ctx);

	/* normally zero, but in bad files there could be more Q than q */
	*prepend = -min_q;

	/* how many Q are missing at the end */
	*append = end_q - min_q;

	/* if there are unguarded operators we must add one level of q/Q around everything */
	if (min_op_q == min_q)
	{
		*prepend += 1;
		*append += 1;
	}
}
