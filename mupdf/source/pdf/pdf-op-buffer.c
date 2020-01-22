#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

typedef struct pdf_output_processor_s pdf_output_processor;

struct pdf_output_processor_s
{
	pdf_processor super;
	fz_output *out;
	int ahxencode;
	int extgstate;
};

/* general graphics state */

static void
pdf_out_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
		fz_write_printf(ctx, out, "%g w\n", linewidth);
}

static void
pdf_out_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
		fz_write_printf(ctx, out, "%d j\n", linejoin);
}

static void
pdf_out_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
		fz_write_printf(ctx, out, "%d J\n", linecap);
}

static void
pdf_out_M(fz_context *ctx, pdf_processor *proc, float a)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
		fz_write_printf(ctx, out, "%g M\n", a);
}

static void
pdf_out_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int ahx = ((pdf_output_processor*)proc)->ahxencode;
	if (!((pdf_output_processor*)proc)->extgstate)
	{
		pdf_print_obj(ctx, out, array, 1, ahx);
		fz_write_printf(ctx, out, " %g d\n", phase);
	}
}

static void
pdf_out_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
		fz_write_printf(ctx, out, "%n ri\n", intent);
}

static void
pdf_out_gs_OP(fz_context *ctx, pdf_processor *proc, int b)
{
}

static void
pdf_out_gs_op(fz_context *ctx, pdf_processor *proc, int b)
{
}

static void
pdf_out_gs_OPM(fz_context *ctx, pdf_processor *proc, int i)
{
}

static void
pdf_out_gs_UseBlackPtComp(fz_context *ctx, pdf_processor *proc, pdf_obj *name)
{
}

static void
pdf_out_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
		fz_write_printf(ctx, out, "%g i\n", flatness);
}

static void
pdf_out_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	((pdf_output_processor*)proc)->extgstate = 1;
	fz_write_printf(ctx, out, "%n gs\n", name);
}

static void
pdf_out_gs_end(fz_context *ctx, pdf_processor *proc)
{
	((pdf_output_processor*)proc)->extgstate = 0;
}

/* special graphics state */

static void
pdf_out_q(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "q\n");
}

static void
pdf_out_Q(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "Q\n");
}

static void
pdf_out_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g %g %g cm\n", a, b, c, d, e, f);
}

/* path construction */

static void
pdf_out_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g m\n", x, y);
}

static void
pdf_out_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g l\n", x, y);
}

static void
pdf_out_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g %g %g c\n", x1, y1, x2, y2, x3, y3);
}

static void
pdf_out_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g v\n", x2, y2, x3, y3);
}

static void
pdf_out_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g y\n", x1, y1, x3, y3);
}

static void
pdf_out_h(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "h\n");
}

static void
pdf_out_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g re\n", x, y, w, h);
}

/* path painting */

static void
pdf_out_S(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "S\n");
}

static void
pdf_out_s(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "s\n");
}

static void
pdf_out_F(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "F\n");
}

static void
pdf_out_f(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "f\n");
}

static void
pdf_out_fstar(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "f*\n");
}

static void
pdf_out_B(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "B\n");
}

static void
pdf_out_Bstar(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "B*\n");
}

static void
pdf_out_b(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "b\n");
}

static void
pdf_out_bstar(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "b*\n");
}

static void
pdf_out_n(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "n\n");
}

/* clipping paths */

static void
pdf_out_W(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "W\n");
}

static void
pdf_out_Wstar(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "W*\n");
}

/* text objects */

static void
pdf_out_BT(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "BT\n");
}

static void
pdf_out_ET(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "ET\n");
}

/* text state */

static void
pdf_out_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g Tc\n", charspace);
}

static void
pdf_out_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g Tw\n", wordspace);
}

static void
pdf_out_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	/* scale is exactly as read from the file. */
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g Tz\n", scale);
}

static void
pdf_out_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g TL\n", leading);
}

static void
pdf_out_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	if (!((pdf_output_processor*)proc)->extgstate)
	{
		fz_write_printf(ctx, out, "%n %g Tf\n", name, size);
	}
}

static void
pdf_out_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%d Tr\n", render);
}

static void
pdf_out_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g Ts\n", rise);
}

/* text positioning */

static void
pdf_out_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g Td\n", tx, ty);
}

static void
pdf_out_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g TD\n", tx, ty);
}

static void
pdf_out_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g %g %g Tm\n", a, b, c, d, e, f);
}

static void
pdf_out_Tstar(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "T*\n");
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
pdf_out_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int ahx = ((pdf_output_processor*)proc)->ahxencode;
	pdf_print_obj(ctx, out, array, 1, ahx);
	fz_write_string(ctx, out, " TJ\n");
}

static void
pdf_out_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_pdf_string(ctx, out, (const unsigned char *)str, len);
	fz_write_string(ctx, out, " Tj\n");
}

static void
pdf_out_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_pdf_string(ctx, out, (const unsigned char *)str, len);
	fz_write_string(ctx, out, " '\n");
}

static void
pdf_out_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g ", aw, ac);
	fz_write_pdf_string(ctx, out, (const unsigned char *)str, len);
	fz_write_string(ctx, out, " \"\n");
}

/* type 3 fonts */

static void
pdf_out_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g d0\n", wx, wy);
}

static void
pdf_out_d1(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g %g %g d1\n", wx, wy, llx, lly, urx, ury);
}

/* color */

static void
pdf_out_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n CS\n", name);
}

static void
pdf_out_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n cs\n", name);
}

static void
pdf_out_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int i;
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, out, "%g ", color[i]);
	fz_write_printf(ctx, out, "%n SCN\n", name);
}

static void
pdf_out_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int i;
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, out, "%g ", color[i]);
	fz_write_printf(ctx, out, "%n scn\n", name);
}

static void
pdf_out_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n SCN\n", name);
}

static void
pdf_out_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n scn\n", name);
}

static void
pdf_out_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int i;
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, out, "%g ", color[i]);
	fz_write_string(ctx, out, "SCN\n");
}

static void
pdf_out_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int i;
	for (i = 0; i < n; ++i)
		fz_write_printf(ctx, out, "%g ", color[i]);
	fz_write_string(ctx, out, "scn\n");
}

static void
pdf_out_G(fz_context *ctx, pdf_processor *proc, float g)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g G\n", g);
}

static void
pdf_out_g(fz_context *ctx, pdf_processor *proc, float g)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g g\n", g);
}

static void
pdf_out_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g RG\n", r, g, b);
}

static void
pdf_out_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g rg\n", r, g, b);
}

static void
pdf_out_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g K\n", c, m, y, k);
}

static void
pdf_out_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%g %g %g %g k\n", c, m, y, k);
}

/* shadings, images, xobjects */

static void
pdf_out_BI(fz_context *ctx, pdf_processor *proc, fz_image *img, const char *colorspace)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int ahx = ((pdf_output_processor*)proc)->ahxencode;
	fz_compressed_buffer *cbuf;
	fz_buffer *buf;
	int i;
	unsigned char *data;
	size_t len;

	if (img == NULL)
		return;
	cbuf = fz_compressed_image_buffer(ctx, img);
	if (cbuf == NULL)
		return;
	buf = cbuf->buffer;
	if (buf == NULL)
		return;

	fz_write_string(ctx, out, "BI\n");
	fz_write_printf(ctx, out, "/W %d\n", img->w);
	fz_write_printf(ctx, out, "/H %d\n", img->h);
	fz_write_printf(ctx, out, "/BPC %d\n", img->bpc);
	if (img->imagemask)
		fz_write_string(ctx, out, "/IM true\n");
	else if (img->colorspace == fz_device_gray(ctx))
		fz_write_string(ctx, out, "/CS/G\n");
	else if (img->colorspace == fz_device_rgb(ctx))
		fz_write_string(ctx, out, "/CS/RGB\n");
	else if (img->colorspace == fz_device_cmyk(ctx))
		fz_write_string(ctx, out, "/CS/CMYK\n");
	else if (colorspace)
		fz_write_printf(ctx, out, "/CS%n/n", colorspace);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "BI operator can only show ImageMask, Gray, RGB, or CMYK images");
	if (img->interpolate)
		fz_write_string(ctx, out, "/I true\n");
	fz_write_string(ctx, out, "/D[");
	for (i = 0; i < img->n * 2; ++i)
	{
		if (i > 0)
			fz_write_byte(ctx, out, ' ');
		fz_write_printf(ctx, out, "%g", img->decode[i]);
	}
	fz_write_string(ctx, out, "]\n");

	switch (cbuf->params.type)
	{
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown compressed buffer type");
		break;

	case FZ_IMAGE_JPEG:
		fz_write_string(ctx, out, ahx ? "/F[/AHx/DCT]\n" : "/F/DCT\n");
		if (cbuf->params.u.jpeg.color_transform != -1)
			fz_write_printf(ctx, out, "/DP<</ColorTransform %d>>\n",
				cbuf->params.u.jpeg.color_transform);
		break;

	case FZ_IMAGE_FAX:
		fz_write_string(ctx, out, ahx ? "/F[/AHx/CCF]\n/DP[null<<\n" : "/F/CCF\n/DP<<\n");
		fz_write_printf(ctx, out, "/K %d\n", cbuf->params.u.fax.k);
		if (cbuf->params.u.fax.columns != 1728)
			fz_write_printf(ctx, out, "/Columns %d\n", cbuf->params.u.fax.columns);
		if (cbuf->params.u.fax.rows > 0)
			fz_write_printf(ctx, out, "/Rows %d\n", cbuf->params.u.fax.rows);
		if (cbuf->params.u.fax.end_of_line)
			fz_write_string(ctx, out, "/EndOfLine true\n");
		if (cbuf->params.u.fax.encoded_byte_align)
			fz_write_string(ctx, out, "/EncodedByteAlign true\n");
		if (!cbuf->params.u.fax.end_of_block)
			fz_write_string(ctx, out, "/EndOfBlock false\n");
		if (cbuf->params.u.fax.black_is_1)
			fz_write_string(ctx, out, "/BlackIs1 true\n");
		if (cbuf->params.u.fax.damaged_rows_before_error > 0)
			fz_write_printf(ctx, out, "/DamagedRowsBeforeError %d\n",
				cbuf->params.u.fax.damaged_rows_before_error);
		fz_write_string(ctx, out, ahx ? ">>]\n" : ">>\n");
		break;

	case FZ_IMAGE_RAW:
		if (ahx)
			fz_write_string(ctx, out, "/F/AHx\n");
		break;

	case FZ_IMAGE_RLD:
		fz_write_string(ctx, out, ahx ? "/F[/AHx/RL]\n" : "/F/RL\n");
		break;

	case FZ_IMAGE_FLATE:
		fz_write_string(ctx, out, ahx ? "/F[/AHx/Fl]\n" : "/F/Fl\n");
		if (cbuf->params.u.flate.predictor > 1)
		{
			fz_write_string(ctx, out, ahx ? "/DP[null<<\n" : "/DP<<\n");
			fz_write_printf(ctx, out, "/Predictor %d\n", cbuf->params.u.flate.predictor);
			if (cbuf->params.u.flate.columns != 1)
				fz_write_printf(ctx, out, "/Columns %d\n", cbuf->params.u.flate.columns);
			if (cbuf->params.u.flate.colors != 1)
				fz_write_printf(ctx, out, "/Colors %d\n", cbuf->params.u.flate.colors);
			if (cbuf->params.u.flate.bpc != 8)
				fz_write_printf(ctx, out, "/BitsPerComponent %d\n", cbuf->params.u.flate.bpc);
			fz_write_string(ctx, out, ahx ? ">>]\n" : ">>\n");
		}
		break;

	case FZ_IMAGE_LZW:
		fz_write_string(ctx, out, ahx ? "/F[/AHx/LZW]\n" : "/F/LZW\n");
		if (cbuf->params.u.lzw.predictor > 1)
		{
			fz_write_string(ctx, out, ahx ? "/DP[<<null\n" : "/DP<<\n");
			fz_write_printf(ctx, out, "/Predictor %d\n", cbuf->params.u.lzw.predictor);
			if (cbuf->params.u.lzw.columns != 1)
				fz_write_printf(ctx, out, "/Columns %d\n", cbuf->params.u.lzw.columns);
			if (cbuf->params.u.lzw.colors != 1)
				fz_write_printf(ctx, out, "/Colors %d\n", cbuf->params.u.lzw.colors);
			if (cbuf->params.u.lzw.bpc != 8)
				fz_write_printf(ctx, out, "/BitsPerComponent %d\n", cbuf->params.u.lzw.bpc);
			if (cbuf->params.u.lzw.early_change != 1)
				fz_write_printf(ctx, out, "/EarlyChange %d\n", cbuf->params.u.lzw.early_change);
			fz_write_string(ctx, out, ahx ? ">>]\n" : ">>\n");
		}
		break;
	}

	fz_write_string(ctx, out, "ID\n");
	len = fz_buffer_storage(ctx, buf, &data);
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
	fz_write_string(ctx, out, "\nEI\n");
}

static void
pdf_out_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n sh\n", name);
}

static void
pdf_out_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n Do\n", name);
}

static void
pdf_out_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj, pdf_obj *page_resources)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n Do\n", name);
}

/* marked content */

static void
pdf_out_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n MP\n", tag);
}

static void
pdf_out_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int ahx = ((pdf_output_processor*)proc)->ahxencode;
	fz_write_printf(ctx, out, "%n ", tag);
	pdf_print_obj(ctx, out, raw, 1, ahx);
	fz_write_string(ctx, out, " DP\n");
}

static void
pdf_out_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_printf(ctx, out, "%n BMC\n", tag);
}

static void
pdf_out_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	int ahx = ((pdf_output_processor*)proc)->ahxencode;
	fz_write_printf(ctx, out, "%n ", tag);
	pdf_print_obj(ctx, out, raw, 1, ahx);
	fz_write_string(ctx, out, " BDC\n");
}

static void
pdf_out_EMC(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "EMC\n");
}

/* compatibility */

static void
pdf_out_BX(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "BX\n");
}

static void
pdf_out_EX(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_write_string(ctx, out, "EX\n");
}

static void
pdf_close_output_processor(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_close_output(ctx, out);
}

static void
pdf_drop_output_processor(fz_context *ctx, pdf_processor *proc)
{
	fz_output *out = ((pdf_output_processor*)proc)->out;
	fz_drop_output(ctx, out);
}

/*
	Create an output processor. This
	sends the incoming PDF operator stream to an fz_output stream.

	out: The output stream to which operators will be sent.

	ahxencode: If 0, then image streams will be send as binary,
	otherwise they will be asciihexencoded.
*/
pdf_processor *
pdf_new_output_processor(fz_context *ctx, fz_output *out, int ahxencode)
{
	pdf_output_processor *proc = pdf_new_processor(ctx, sizeof *proc);
	{
		proc->super.close_processor = pdf_close_output_processor;
		proc->super.drop_processor = pdf_drop_output_processor;

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
		proc->super.op_gs_OP = pdf_out_gs_OP;
		proc->super.op_gs_op = pdf_out_gs_op;
		proc->super.op_gs_OPM = pdf_out_gs_OPM;
		proc->super.op_gs_UseBlackPtComp = pdf_out_gs_UseBlackPtComp;
	}

	proc->out = out;
	proc->ahxencode = ahxencode;

	return (pdf_processor*)proc;
}

/*
	Create a buffer processor. This
	collects the incoming PDF operator stream into an fz_buffer.

	buffer: The (possibly empty) buffer to which operators will be
	appended.

	ahxencode: If 0, then image streams will be send as binary,
	otherwise they will be asciihexencoded.
*/
pdf_processor *
pdf_new_buffer_processor(fz_context *ctx, fz_buffer *buffer, int ahxencode)
{
	pdf_processor *proc = NULL;
	fz_output *out = fz_new_output_with_buffer(ctx, buffer);
	fz_try(ctx)
	{
		proc = pdf_new_output_processor(ctx, out, ahxencode);
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return proc;
}
