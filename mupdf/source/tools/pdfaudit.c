// Copyright (C) 2023-2025 Artifex Software, Inc.
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

/*
 * PDF auditing tool
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SWITCH(x) switch ((intptr_t)(x))
#define CASE(x) case ((intptr_t)(x))

typedef enum
{
	AUDIT_UNKNOWN = 0,
	AUDIT_THUMBNAILS,
	AUDIT_IMAGES,
	AUDIT_BOOKMARKS,
	AUDIT_PAGE_OBJECTS,
	AUDIT_CONTENT_STREAMS,
	AUDIT_FONTS,
	AUDIT_STRUCTURE_INFO,
	AUDIT_FORMS,
	AUDIT_LINK_ANNOTATIONS,
	AUDIT_COMMENTS,
	AUDIT_3DCONTENT,
	AUDIT_NAMED_DESTINATIONS,
	//AUDIT_DOCUMENT_OVERHEAD, // FIXME
	AUDIT_COLORSPACES,
	AUDIT_FORM_XOBJ,
	AUDIT_EXTGS,
	AUDIT_PIECE_INFORMATION,
	AUDIT_EMBEDDED_FILES,
	AUDIT_TRAILER,
	AUDIT_RESOURCES,
	AUDIT_OBJSTM,
	AUDIT_METADATA,
	AUDIT__MAX
} audit_type_t;

const char *audit_type[] =
{
	"UNKNOWN",
	"THUMBNAILS",
	"IMAGES",
	"BOOKMARKS",
	"PAGE OBJECTS",
	"CONTENT_STREAMS",
	"FONTS",
	"STRUCTURE_INFO",
	"FORMS",
	"LINK_ANNOTATIONS",
	"COMMENTS",
	"3DCONTENT",
	"NAMED_DESTINATIONS",
	//"DOCUMENT_OVERHEAD",
	"COLORSPACES",
	"FORM_XOBJ",
	"EXTGS",
	"PIECE_INFORMATION",
	"EMBEDDED_FILES",
	"TRAILER",
	"RESOURCES",
	"OBJSTM",
	"METADATA"
};

typedef struct
{
	audit_type_t type;
	int is_in_objstm;
	/* The number of bytes this object will take in the file, not including any actual stream content. */
	size_t textsize;
	/* The number of bytes of overhead "1 0 R\nendobj\n" plus "stream\nendstream\n" */
	size_t overhead;
	/* Uncompressed stream size */
	size_t len;
	/* Compressed stream size (not including 'stream\nendstream\n) */
	size_t stream_len;
} obj_info_t;

enum
{
	OP_w = 0,
	OP_j,
	OP_J,
	OP_M,
	OP_d,
	OP_ri,
	OP_gs_OP,
	OP_gs_op,
	OP_gs_OPM,
	OP_gs_UseBlackPtComp,
	OP_i,
	OP_gs_begin,
	OP_gs_BM,
	OP_gs_CA,
	OP_gs_ca,
	OP_gs_SMask,
	OP_gs_end,
	OP_q,
	OP_cm,
	OP_m,
	OP_l,
	OP_c,
	OP_v,
	OP_y,
	OP_h,
	OP_re,
	OP_S,
	OP_s,
	OP_F,
	OP_f,
	OP_fstar,
	OP_B,
	OP_Bstar,
	OP_b,
	OP_bstar,
	OP_n,
	OP_W,
	OP_Wstar,
	OP_BT,
	OP_ET,
	OP_Q,
	OP_Tc,
	OP_Tw,
	OP_Tz,
	OP_TL,
	OP_Tf,
	OP_Tr,
	OP_Ts,
	OP_Td,
	OP_TD,
	OP_Tm,
	OP_Tstar,
	OP_TJ,
	OP_Tj,
	OP_squote,
	OP_dquote,
	OP_d0,
	OP_d1,
	OP_CS,
	OP_cs,
	OP_SC_pattern,
	OP_sc_pattern,
	OP_SC_shade,
	OP_sc_shade,
	OP_SC_color,
	OP_sc_color,
	OP_G,
	OP_g,
	OP_RG,
	OP_rg,
	OP_K,
	OP_k,
	OP_BI,
	OP_sh,
	OP_Do_image,
	OP_Do_form,
	OP_MP,
	OP_DP,
	OP_BMC,
	OP_BDC,
	OP_EMC,
	OP_BX,
	OP_EX,
	OP_END
};

const char *op_names[] =
{
	"w",
	"j",
	"J",
	"M",
	"d",
	"ri",
	"gs_OP",
	"gs_op",
	"gs_OPM",
	"gs_UseBlackPtComp",
	"i",
	"gs_begin",
	"gs_BM",
	"gs_CA",
	"gs_ca",
	"gs_SMask",
	"gs_end",
	"q",
	"cm",
	"m",
	"l",
	"c",
	"v",
	"y",
	"h",
	"re",
	"S",
	"s",
	"F",
	"f",
	"fstar",
	"B",
	"Bstar",
	"b",
	"bstar",
	"n",
	"W",
	"Wstar",
	"BT",
	"ET",
	"Q",
	"Tc",
	"Tw",
	"Tz",
	"TL",
	"Tf",
	"Tr",
	"Ts",
	"Td",
	"TD",
	"Tm",
	"Tstar",
	"TJ",
	"Tj",
	"squote",
	"dquote",
	"d0",
	"d1",
	"CS",
	"cs",
	"SC_pattern",
	"sc_pattern",
	"SC_shade",
	"sc_shade",
	"SC_color",
	"sc_color",
	"G",
	"g",
	"RG",
	"rg",
	"K",
	"k",
	"BI",
	"sh",
	"Do_image",
	"Do_form",
	"MP",
	"DP",
	"BMC",
	"BDC",
	"EMC",
	"BX",
	"EX",
};

typedef struct
{
	size_t len[OP_END];
} op_usage_t;

typedef struct
{
	pdf_processor super;
	pdf_document *doc;
	int structparents;
	pdf_processor *mine;
	pdf_filter_options *global_options;
	op_usage_t *op_usage;
	fz_buffer *buffer;
} pdf_opcount_processor;

/* general graphics state */

static void
pdf_opcount_w(fz_context *ctx, pdf_processor *proc, float linewidth)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_w)
		p->mine->op_w(ctx, p->mine, linewidth);

	z = p->buffer->len - z;
	p->op_usage->len[OP_w] += z;
}

static void
pdf_opcount_j(fz_context *ctx, pdf_processor *proc, int linejoin)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_j)
		p->mine->op_j(ctx, p->mine, linejoin);

	z = p->buffer->len - z;
	p->op_usage->len[OP_j] += z;
}

static void
pdf_opcount_J(fz_context *ctx, pdf_processor *proc, int linecap)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_J)
		p->mine->op_J(ctx, p->mine, linecap);

	z = p->buffer->len - z;
	p->op_usage->len[OP_J] += z;
}

static void
pdf_opcount_M(fz_context *ctx, pdf_processor *proc, float miterlimit)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_M)
		p->mine->op_M(ctx, p->mine, miterlimit);

	z = p->buffer->len - z;
	p->op_usage->len[OP_M] += z;
}

static void
pdf_opcount_d(fz_context *ctx, pdf_processor *proc, pdf_obj *array, float phase)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_d)
		p->mine->op_d(ctx, p->mine, array, phase);

	z = p->buffer->len - z;
	p->op_usage->len[OP_d] += z;
}

static void
pdf_opcount_ri(fz_context *ctx, pdf_processor *proc, const char *intent)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_ri)
		p->mine->op_ri(ctx, p->mine, intent);

	z = p->buffer->len - z;
	p->op_usage->len[OP_ri] += z;
}

static void
pdf_opcount_gs_OP(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_OP)
		p->mine->op_gs_OP(ctx, p->mine, b);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_OP] += z;
}

static void
pdf_opcount_gs_op(fz_context *ctx, pdf_processor *proc, int b)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_op)
		p->mine->op_gs_op(ctx, p->mine, b);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_op] += z;
}

static void
pdf_opcount_gs_OPM(fz_context *ctx, pdf_processor *proc, int i)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_OPM)
		p->mine->op_gs_OPM(ctx, p->mine, i);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_OPM] += z;
}

static void
pdf_opcount_gs_UseBlackPtComp(fz_context *ctx, pdf_processor *proc, pdf_obj *name)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_UseBlackPtComp)
		p->mine->op_gs_UseBlackPtComp(ctx, p->mine, name);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_UseBlackPtComp] += z;
}

static void
pdf_opcount_i(fz_context *ctx, pdf_processor *proc, float flatness)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_i)
		p->mine->op_i(ctx, p->mine, flatness);

	z = p->buffer->len - z;
	p->op_usage->len[OP_i] += z;
}

static void
pdf_opcount_gs_begin(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *extgstate)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_begin)
		p->mine->op_gs_begin(ctx, p->mine, name, extgstate);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_begin] += z;
}

static void
pdf_opcount_gs_BM(fz_context *ctx, pdf_processor *proc, const char *blendmode)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_BM)
		p->mine->op_gs_BM(ctx, p->mine, blendmode);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_BM] += z;
}

static void
pdf_opcount_gs_CA(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_CA)
		p->mine->op_gs_CA(ctx, p->mine, alpha);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_CA] += z;
}

static void
pdf_opcount_gs_ca(fz_context *ctx, pdf_processor *proc, float alpha)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_ca)
		p->mine->op_gs_ca(ctx, p->mine, alpha);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_ca] += z;
}

static void
pdf_opcount_gs_SMask(fz_context *ctx, pdf_processor *proc, pdf_obj *smask, fz_colorspace *smask_cs, float *bc, int luminosity, pdf_obj *obj)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_SMask)
		p->mine->op_gs_SMask(ctx, p->mine, smask, smask_cs, bc, luminosity, obj);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_SMask] += z;
}

static void
pdf_opcount_gs_end(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_gs_end)
		p->mine->op_gs_end(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_gs_end] += z;
}

/* special graphics state */

static void
pdf_opcount_q(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_q)
		p->mine->op_q(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_q] += z;
}

static void
pdf_opcount_cm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_cm)
		p->mine->op_cm(ctx, p->mine, a, b, c, d, e, f);

	z = p->buffer->len - z;
	p->op_usage->len[OP_cm] += z;
}

/* path construction */

static void
pdf_opcount_m(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_m)
		p->mine->op_m(ctx, p->mine, x, y);

	z = p->buffer->len - z;
	p->op_usage->len[OP_m] += z;
}

static void
pdf_opcount_l(fz_context *ctx, pdf_processor *proc, float x, float y)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_l)
		p->mine->op_l(ctx, p->mine, x, y);

	z = p->buffer->len - z;
	p->op_usage->len[OP_l] += z;
}

static void
pdf_opcount_c(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x2, float y2, float x3, float y3)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_c)
		p->mine->op_c(ctx, p->mine, x1, y1, x2, y2, x3, y3);

	z = p->buffer->len - z;
	p->op_usage->len[OP_c] += z;
}

static void
pdf_opcount_v(fz_context *ctx, pdf_processor *proc, float x2, float y2, float x3, float y3)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_v)
		p->mine->op_v(ctx, p->mine, x2, y2, x3, y3);

	z = p->buffer->len - z;
	p->op_usage->len[OP_v] += z;
}

static void
pdf_opcount_y(fz_context *ctx, pdf_processor *proc, float x1, float y1, float x3, float y3)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_y)
		p->mine->op_y(ctx, p->mine, x1, y1, x3, y3);

	z = p->buffer->len - z;
	p->op_usage->len[OP_y] += z;
}

static void
pdf_opcount_h(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_h)
		p->mine->op_h(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_h] += z;
}

static void
pdf_opcount_re(fz_context *ctx, pdf_processor *proc, float x, float y, float w, float h)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_re)
		p->mine->op_re(ctx, p->mine, x, y, w, h);

	z = p->buffer->len - z;
	p->op_usage->len[OP_re] += z;
}

/* path painting */

static void
pdf_opcount_S(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_S)
		p->mine->op_S(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_S] += z;
}

static void
pdf_opcount_s(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_s)
		p->mine->op_s(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_s] += z;
}

static void
pdf_opcount_F(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_F)
		p->mine->op_F(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_F] += z;
}

static void
pdf_opcount_f(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_f)
		p->mine->op_f(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_f] += z;
}

static void
pdf_opcount_fstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_fstar)
		p->mine->op_fstar(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_fstar] += z;
}

static void
pdf_opcount_B(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_B)
		p->mine->op_B(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_B] += z;
}

static void
pdf_opcount_Bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Bstar)
		p->mine->op_Bstar(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Bstar] += z;
}

static void
pdf_opcount_b(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_b)
		p->mine->op_b(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_b] += z;
}

static void
pdf_opcount_bstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_bstar)
		p->mine->op_bstar(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_bstar] += z;
}

static void
pdf_opcount_n(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_n)
		p->mine->op_n(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_n] += z;
}

/* clipping paths */

static void
pdf_opcount_W(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_W)
		p->mine->op_W(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_W] += z;
}

static void
pdf_opcount_Wstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Wstar)
		p->mine->op_Wstar(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Wstar] += z;
}

/* text objects */

static void
pdf_opcount_BT(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_BT)
		p->mine->op_BT(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_BT] += z;
}

static void
pdf_opcount_ET(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_ET)
		p->mine->op_ET(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_ET] += z;
}

static void
pdf_opcount_Q(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Q)
		p->mine->op_Q(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Q] += z;
}

/* text state */

static void
pdf_opcount_Tc(fz_context *ctx, pdf_processor *proc, float charspace)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tc)
		p->mine->op_Tc(ctx, p->mine, charspace);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tc] += z;
}

static void
pdf_opcount_Tw(fz_context *ctx, pdf_processor *proc, float wordspace)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tw)
		p->mine->op_Tw(ctx, p->mine, wordspace);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tw] += z;
}

static void
pdf_opcount_Tz(fz_context *ctx, pdf_processor *proc, float scale)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tz)
		p->mine->op_Tz(ctx, p->mine, scale);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tz] += z;
}

static void
pdf_opcount_TL(fz_context *ctx, pdf_processor *proc, float leading)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_TL)
		p->mine->op_TL(ctx, p->mine, leading);

	z = p->buffer->len - z;
	p->op_usage->len[OP_TL] += z;
}

static void
pdf_opcount_Tf(fz_context *ctx, pdf_processor *proc, const char *name, pdf_font_desc *font, float size)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tf)
		p->mine->op_Tf(ctx, p->mine, name, font, size);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tf] += z;
}

static void
pdf_opcount_Tr(fz_context *ctx, pdf_processor *proc, int render)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tr)
		p->mine->op_Tr(ctx, p->mine, render);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tr] += z;
}

static void
pdf_opcount_Ts(fz_context *ctx, pdf_processor *proc, float rise)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Ts)
		p->mine->op_Ts(ctx, p->mine, rise);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Ts] += z;
}

/* text positioning */

static void
pdf_opcount_Td(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Td)
		p->mine->op_Td(ctx, p->mine, tx, ty);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Td] += z;
}

static void
pdf_opcount_TD(fz_context *ctx, pdf_processor *proc, float tx, float ty)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_TD)
		p->mine->op_TD(ctx, p->mine, tx, ty);

	z = p->buffer->len - z;
	p->op_usage->len[OP_TD] += z;
}

static void
pdf_opcount_Tm(fz_context *ctx, pdf_processor *proc, float a, float b, float c, float d, float e, float f)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tm)
		p->mine->op_Tm(ctx, p->mine, a, b, c, d, e, f);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tm] += z;
}

static void
pdf_opcount_Tstar(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tstar)
		p->mine->op_Tstar(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tstar] += z;
}

/* text showing */

static void
pdf_opcount_TJ(fz_context *ctx, pdf_processor *proc, pdf_obj *array)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_TJ)
		p->mine->op_TJ(ctx, p->mine, array);

	z = p->buffer->len - z;
	p->op_usage->len[OP_TJ] += z;
}

static void
pdf_opcount_Tj(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Tj)
		p->mine->op_Tj(ctx, p->mine, str, len);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Tj] += z;
}

static void
pdf_opcount_squote(fz_context *ctx, pdf_processor *proc, char *str, size_t len)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_squote)
		p->mine->op_squote(ctx, p->mine, str, len);

	z = p->buffer->len - z;
	p->op_usage->len[OP_squote] += z;
}

static void
pdf_opcount_dquote(fz_context *ctx, pdf_processor *proc, float aw, float ac, char *str, size_t len)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_dquote)
		p->mine->op_dquote(ctx, p->mine, aw, ac, str, len);

	z = p->buffer->len - z;
	p->op_usage->len[OP_dquote] += z;
}

/* type 3 fonts */

static void
pdf_opcount_d0(fz_context *ctx, pdf_processor *proc, float wx, float wy)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_d0)
		p->mine->op_d0(ctx, p->mine, wx, wy);

	z = p->buffer->len - z;
	p->op_usage->len[OP_d0] += z;
}

static void
pdf_opcount_d1(fz_context *ctx, pdf_processor *proc, float wx, float wy, float llx, float lly, float urx, float ury)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_d1)
		p->mine->op_d1(ctx, p->mine, wx, wy, llx, lly, urx, ury);

	z = p->buffer->len - z;
	p->op_usage->len[OP_d1] += z;
}

/* color */

static void
pdf_opcount_CS(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_opcount_processor *p = (pdf_opcount_processor *)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_CS)
		p->mine->op_CS(ctx, p->mine, name, cs);

	z = p->buffer->len - z;
	p->op_usage->len[OP_CS] += z;
}

static void
pdf_opcount_cs(fz_context *ctx, pdf_processor *proc, const char *name, fz_colorspace *cs)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_cs)
		p->mine->op_cs(ctx, p->mine, name, cs);

	z = p->buffer->len - z;
	p->op_usage->len[OP_cs] += z;
}

static void
pdf_opcount_SC_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_SC_pattern)
		p->mine->op_SC_pattern(ctx, p->mine, name, pat, n, color);

	z = p->buffer->len - z;
	p->op_usage->len[OP_SC_pattern] += z;
}

static void
pdf_opcount_sc_pattern(fz_context *ctx, pdf_processor *proc, const char *name, pdf_pattern *pat, int n, float *color)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_sc_pattern)
		p->mine->op_sc_pattern(ctx, p->mine, name, pat, n, color);

	z = p->buffer->len - z;
	p->op_usage->len[OP_sc_pattern] += z;
}

static void
pdf_opcount_SC_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_SC_shade)
		p->mine->op_SC_shade(ctx, p->mine, name, shade);

	z = p->buffer->len - z;
	p->op_usage->len[OP_SC_shade] += z;
}

static void
pdf_opcount_sc_shade(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_sc_shade)
		p->mine->op_sc_shade(ctx, p->mine, name, shade);

	z = p->buffer->len - z;
	p->op_usage->len[OP_sc_shade] += z;
}

static void
pdf_opcount_SC_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_SC_color)
		p->mine->op_SC_color(ctx, p->mine, n, color);

	z = p->buffer->len - z;
	p->op_usage->len[OP_SC_color] += z;
}

static void
pdf_opcount_sc_color(fz_context *ctx, pdf_processor *proc, int n, float *color)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_sc_color)
		p->mine->op_sc_color(ctx, p->mine, n, color);

	z = p->buffer->len - z;
	p->op_usage->len[OP_sc_color] += z;
}

static void
pdf_opcount_G(fz_context *ctx, pdf_processor *proc, float g)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_G)
		p->mine->op_G(ctx, p->mine, g);

	z = p->buffer->len - z;
	p->op_usage->len[OP_G] += z;
}

static void
pdf_opcount_g(fz_context *ctx, pdf_processor *proc, float g)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_g)
		p->mine->op_g(ctx, p->mine, g);

	z = p->buffer->len - z;
	p->op_usage->len[OP_g] += z;
}

static void
pdf_opcount_RG(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_RG)
		p->mine->op_RG(ctx, p->mine, r, g, b);

	z = p->buffer->len - z;
	p->op_usage->len[OP_RG] += z;
}

static void
pdf_opcount_rg(fz_context *ctx, pdf_processor *proc, float r, float g, float b)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_rg)
		p->mine->op_rg(ctx, p->mine, r, g, b);

	z = p->buffer->len - z;
	p->op_usage->len[OP_rg] += z;
}

static void
pdf_opcount_K(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_K)
		p->mine->op_K(ctx, p->mine, c, m, y, k);

	z = p->buffer->len - z;
	p->op_usage->len[OP_K] += z;
}

static void
pdf_opcount_k(fz_context *ctx, pdf_processor *proc, float c, float m, float y, float k)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_k)
		p->mine->op_k(ctx, p->mine, c, m, y, k);

	z = p->buffer->len - z;
	p->op_usage->len[OP_k] += z;
}

/* shadings, images, xobjects */

static void
pdf_opcount_BI(fz_context *ctx, pdf_processor *proc, fz_image *image, const char *colorspace)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_BI)
		p->mine->op_BI(ctx, p->mine, image, colorspace);

	z = p->buffer->len - z;
	p->op_usage->len[OP_BI] += z;
}

static void
pdf_opcount_sh(fz_context *ctx, pdf_processor *proc, const char *name, fz_shade *shade)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_sh)
		p->mine->op_sh(ctx, p->mine, name, shade);

	z = p->buffer->len - z;
	p->op_usage->len[OP_sh] += z;
}

static void
pdf_opcount_Do_image(fz_context *ctx, pdf_processor *proc, const char *name, fz_image *image)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Do_image)
		p->mine->op_Do_image(ctx, p->mine, name, image);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Do_image] += z;
}

static void
pdf_opcount_Do_form(fz_context *ctx, pdf_processor *proc, const char *name, pdf_obj *xobj)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_Do_form)
		p->mine->op_Do_form(ctx, p->mine, name, xobj);

	z = p->buffer->len - z;
	p->op_usage->len[OP_Do_form] += z;
}

/* marked content */

static void
pdf_opcount_MP(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_MP)
		p->mine->op_MP(ctx, p->mine, tag);

	z = p->buffer->len - z;
	p->op_usage->len[OP_MP] += z;
}

static void
pdf_opcount_DP(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_DP)
		p->mine->op_DP(ctx, p->mine, tag, raw, cooked);

	z = p->buffer->len - z;
	p->op_usage->len[OP_DP] += z;
}

static void
pdf_opcount_BMC(fz_context *ctx, pdf_processor *proc, const char *tag)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_BMC)
		p->mine->op_BMC(ctx, p->mine, tag);

	z = p->buffer->len - z;
	p->op_usage->len[OP_BMC] += z;
}

static void
pdf_opcount_BDC(fz_context *ctx, pdf_processor *proc, const char *tag, pdf_obj *raw, pdf_obj *cooked)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_BDC)
		p->mine->op_BDC(ctx, p->mine, tag, raw, cooked);

	z = p->buffer->len - z;
	p->op_usage->len[OP_BDC] += z;
}

static void
pdf_opcount_EMC(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_EMC)
		p->mine->op_EMC(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_EMC] += z;
}

/* compatibility */

static void
pdf_opcount_BX(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_BX)
		p->mine->op_BX(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_BX] += z;
}

static void
pdf_opcount_EX(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;
	size_t z = p->buffer->len;

	if (p->mine->op_EX)
		p->mine->op_EX(ctx, p->mine);

	z = p->buffer->len - z;
	p->op_usage->len[OP_EX] += z;
}

static void
pdf_opcount_END(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;

	if (p->mine->op_END)
		p->mine->op_END(ctx, p->mine);
}

static void
pdf_close_opcount_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;

	pdf_close_processor(ctx, p->mine);
	pdf_close_processor(ctx, p->super.chain);
}

static void
pdf_drop_opcount_processor(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;

	fz_drop_buffer(ctx, p->buffer);
	pdf_drop_processor(ctx, p->mine);
}

static void
pdf_opcount_push_resources(fz_context *ctx, pdf_processor *proc, pdf_obj *res)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;

	pdf_processor_push_resources(ctx, p->mine, res);
}

static pdf_obj *
pdf_opcount_pop_resources(fz_context *ctx, pdf_processor *proc)
{
	pdf_opcount_processor *p = (pdf_opcount_processor*)proc;

	return pdf_processor_pop_resources(ctx, p->mine);
}

pdf_processor *
pdf_new_opcount_filter(
	fz_context *ctx,
	pdf_document *doc,
	pdf_processor *chain,
	int struct_parents,
	fz_matrix transform,
	pdf_filter_options *global_options,
	void *options_)
{
	pdf_opcount_processor *proc = pdf_new_processor(ctx, sizeof * proc);

	fz_try(ctx)
	{
		proc->buffer = fz_new_buffer(ctx, 1024);
		proc->mine = pdf_new_buffer_processor(ctx, proc->buffer, 0, 0);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	proc->op_usage = (op_usage_t *)options_;

	proc->super.close_processor = pdf_close_opcount_processor;
	proc->super.drop_processor = pdf_drop_opcount_processor;

	proc->super.push_resources = pdf_opcount_push_resources;
	proc->super.pop_resources = pdf_opcount_pop_resources;

	/* general graphics state */
	proc->super.op_w = pdf_opcount_w;
	proc->super.op_j = pdf_opcount_j;
	proc->super.op_J = pdf_opcount_J;
	proc->super.op_M = pdf_opcount_M;
	proc->super.op_d = pdf_opcount_d;
	proc->super.op_ri = pdf_opcount_ri;
	proc->super.op_i = pdf_opcount_i;
	proc->super.op_gs_begin = pdf_opcount_gs_begin;
	proc->super.op_gs_end = pdf_opcount_gs_end;

	/* transparency graphics state */
	proc->super.op_gs_BM = pdf_opcount_gs_BM;
	proc->super.op_gs_CA = pdf_opcount_gs_CA;
	proc->super.op_gs_ca = pdf_opcount_gs_ca;
	proc->super.op_gs_SMask = pdf_opcount_gs_SMask;

	/* special graphics state */
	proc->super.op_q = pdf_opcount_q;
	proc->super.op_Q = pdf_opcount_Q;
	proc->super.op_cm = pdf_opcount_cm;

	/* path construction */
	proc->super.op_m = pdf_opcount_m;
	proc->super.op_l = pdf_opcount_l;
	proc->super.op_c = pdf_opcount_c;
	proc->super.op_v = pdf_opcount_v;
	proc->super.op_y = pdf_opcount_y;
	proc->super.op_h = pdf_opcount_h;
	proc->super.op_re = pdf_opcount_re;

	/* path painting */
	proc->super.op_S = pdf_opcount_S;
	proc->super.op_s = pdf_opcount_s;
	proc->super.op_F = pdf_opcount_F;
	proc->super.op_f = pdf_opcount_f;
	proc->super.op_fstar = pdf_opcount_fstar;
	proc->super.op_B = pdf_opcount_B;
	proc->super.op_Bstar = pdf_opcount_Bstar;
	proc->super.op_b = pdf_opcount_b;
	proc->super.op_bstar = pdf_opcount_bstar;
	proc->super.op_n = pdf_opcount_n;

	/* clipping paths */
	proc->super.op_W = pdf_opcount_W;
	proc->super.op_Wstar = pdf_opcount_Wstar;

	/* text objects */
	proc->super.op_BT = pdf_opcount_BT;
	proc->super.op_ET = pdf_opcount_ET;

	/* text state */
	proc->super.op_Tc = pdf_opcount_Tc;
	proc->super.op_Tw = pdf_opcount_Tw;
	proc->super.op_Tz = pdf_opcount_Tz;
	proc->super.op_TL = pdf_opcount_TL;
	proc->super.op_Tf = pdf_opcount_Tf;
	proc->super.op_Tr = pdf_opcount_Tr;
	proc->super.op_Ts = pdf_opcount_Ts;

	/* text positioning */
	proc->super.op_Td = pdf_opcount_Td;
	proc->super.op_TD = pdf_opcount_TD;
	proc->super.op_Tm = pdf_opcount_Tm;
	proc->super.op_Tstar = pdf_opcount_Tstar;

	/* text showing */
	proc->super.op_TJ = pdf_opcount_TJ;
	proc->super.op_Tj = pdf_opcount_Tj;
	proc->super.op_squote = pdf_opcount_squote;
	proc->super.op_dquote = pdf_opcount_dquote;

	/* type 3 fonts */
	proc->super.op_d0 = pdf_opcount_d0;
	proc->super.op_d1 = pdf_opcount_d1;

	/* color */
	proc->super.op_CS = pdf_opcount_CS;
	proc->super.op_cs = pdf_opcount_cs;
	proc->super.op_SC_color = pdf_opcount_SC_color;
	proc->super.op_sc_color = pdf_opcount_sc_color;
	proc->super.op_SC_pattern = pdf_opcount_SC_pattern;
	proc->super.op_sc_pattern = pdf_opcount_sc_pattern;
	proc->super.op_SC_shade = pdf_opcount_SC_shade;
	proc->super.op_sc_shade = pdf_opcount_sc_shade;

	proc->super.op_G = pdf_opcount_G;
	proc->super.op_g = pdf_opcount_g;
	proc->super.op_RG = pdf_opcount_RG;
	proc->super.op_rg = pdf_opcount_rg;
	proc->super.op_K = pdf_opcount_K;
	proc->super.op_k = pdf_opcount_k;

	/* shadings, images, xobjects */
	proc->super.op_BI = pdf_opcount_BI;
	proc->super.op_sh = pdf_opcount_sh;
	proc->super.op_Do_image = pdf_opcount_Do_image;
	proc->super.op_Do_form = pdf_opcount_Do_form;

	/* marked content */
	proc->super.op_MP = pdf_opcount_MP;
	proc->super.op_DP = pdf_opcount_DP;
	proc->super.op_BMC = pdf_opcount_BMC;
	proc->super.op_BDC = pdf_opcount_BDC;
	proc->super.op_EMC = pdf_opcount_EMC;

	/* compatibility */
	proc->super.op_BX = pdf_opcount_BX;
	proc->super.op_EX = pdf_opcount_EX;

	/* extgstate */
	proc->super.op_gs_OP = pdf_opcount_gs_OP;
	proc->super.op_gs_op = pdf_opcount_gs_op;
	proc->super.op_gs_OPM = pdf_opcount_gs_OPM;
	proc->super.op_gs_UseBlackPtComp = pdf_opcount_gs_UseBlackPtComp;

	proc->super.op_END = pdf_opcount_END;

	proc->global_options = global_options;
	proc->super.chain = chain;

	return (pdf_processor*)proc;
}

static void
filter_page(fz_context *ctx, pdf_document *doc, op_usage_t *op_usage, int page_num)
{
	pdf_page *page = pdf_load_page(ctx, doc, page_num);
	pdf_filter_options options = { 0 };
	pdf_filter_factory list[2] = { 0 };
	pdf_annot *annot;

	options.filters = list;
	options.recurse = 1;
	options.no_update = 1;
	list[0].filter = pdf_new_opcount_filter;
	list[0].options = op_usage;

	fz_try(ctx)
	{
		pdf_filter_page_contents(ctx, doc, page, &options);

		for (annot = pdf_first_annot(ctx, page); annot != NULL; annot = pdf_next_annot(ctx, annot))
			pdf_filter_annot_contents(ctx, doc, annot, &options);
	}
	fz_always(ctx)
		fz_drop_page(ctx, &page->super);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
filter_page_streams(fz_context *ctx, pdf_document *pdf, op_usage_t *ou)
{
	int i, n = pdf_count_pages(ctx, pdf);

	for (i= 0; i < n; i++)
	{
		filter_page(ctx, pdf, ou, i);
	}
}

static void
filter_buffer(fz_context *ctx, obj_info_t *oi, fz_buffer *buf)
{
	oi->len = buf->len;
}

static void
filter_stream(fz_context *ctx, pdf_document *pdf, int i, obj_info_t *oi)
{
	fz_buffer *buf = pdf_load_stream_number(ctx, pdf, i);

	fz_try(ctx)
		filter_buffer(ctx, oi, buf);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
filter_obj(fz_context *ctx, obj_info_t *oi, pdf_obj *obj)
{
	fz_buffer *buf = fz_new_buffer(ctx, 1024);
	fz_output *out = NULL;

	fz_var(out);

	fz_try(ctx)
	{
		out = fz_new_output_with_buffer(ctx, buf);
		pdf_print_obj(ctx, out, obj, 1, 0);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		if (buf)
			oi->textsize = buf->len;
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

typedef struct
{
	int len;
	int max;
	struct {
		pdf_obj *obj;
		int pos;
		int state;
	} *stack;
} walk_stack_t;

static void
walk(fz_context *ctx, walk_stack_t *ws, int n, obj_info_t *oi, pdf_obj *obj, audit_type_t type)
{
	int num = 0;
	do
	{
		if (pdf_is_indirect(ctx, obj))
		{
			num = pdf_to_num(ctx, obj);
			if (num < 0 || num >= n)
				fz_throw(ctx, FZ_ERROR_GENERIC, "object outside of xref range");
			if (oi[num].type != AUDIT_UNKNOWN)
				goto visited;
			if (pdf_mark_obj(ctx, obj))
			{
				/* We've already visited this one! */
				goto visited;
			}
		}
		/* Push the object onto the stack. */
		if (ws->len == ws->max)
		{
			int newmax = ws->max * 2;
			if (newmax == 0)
				newmax = 32;
			ws->stack = fz_realloc(ctx, ws->stack, sizeof(ws->stack[0]) * newmax);
			ws->max = newmax;
		}

		/* If the object we are about to stack is a dict, then check to see if
		 * we should be changing type because of it. */
		if (pdf_is_dict(ctx, obj))
		{
			pdf_obj *otype = pdf_dict_get(ctx, obj, PDF_NAME(Type));
			pdf_obj *subtype = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));

			if (pdf_name_eq(ctx, otype, PDF_NAME(Annot)))
			{
				if (pdf_name_eq(ctx, subtype, PDF_NAME(Link)))
					type = AUDIT_LINK_ANNOTATIONS;
				else if (pdf_name_eq(ctx, subtype, PDF_NAME(Text)))
					type = AUDIT_COMMENTS;
				else if (pdf_name_eq(ctx, subtype, PDF_NAME(FreeText)))
					type = AUDIT_COMMENTS;
				else if (pdf_name_eq(ctx, subtype, PDF_NAME(Popup)))
					type = AUDIT_COMMENTS;
				else if (pdf_name_eq(ctx, subtype, PDF_NAME(3D)))
					type = AUDIT_3DCONTENT;
				else if (pdf_name_eq(ctx, subtype, PDF_NAME(PieceInfo)))
					type = AUDIT_PIECE_INFORMATION;
			}
			else if (pdf_name_eq(ctx, otype, PDF_NAME(Font)))
				type = AUDIT_FONTS;
			else if (pdf_name_eq(ctx, otype, PDF_NAME(FontDescriptor)))
				type = AUDIT_FONTS;
			else if (pdf_name_eq(ctx, otype, PDF_NAME(XObject)))
			{
				if (pdf_name_eq(ctx, subtype, PDF_NAME(Image)))
					type = AUDIT_IMAGES;
				else if (pdf_name_eq(ctx, subtype, PDF_NAME(Form)))
					type = AUDIT_FORM_XOBJ;
			}
			else if (pdf_name_eq(ctx, otype, PDF_NAME(Page)))
				type = AUDIT_PAGE_OBJECTS;
			else if (pdf_name_eq(ctx, otype, PDF_NAME(Pages)))
				type = AUDIT_PAGE_OBJECTS;
			else if (pdf_name_eq(ctx, otype, PDF_NAME(Metadata)))
				type = AUDIT_METADATA;
		}
		ws->stack[ws->len].obj = obj;
		ws->stack[ws->len].pos = 0;
		ws->stack[ws->len].state = type;
		ws->len++;

		/* So we have stepped successfully onto obj. */
		/* Record its type. */
		if (type != AUDIT_UNKNOWN)
		{
			num = pdf_obj_parent_num(ctx, obj);
			oi[num].type = type;
		}

		/* Step onwards to any children. */
		if (pdf_is_dict(ctx, obj))
		{
			pdf_obj *key;
			/* We've just stepped onto a dict. */
step_next_dict_child:
			if (ws->stack[ws->len-1].pos == pdf_dict_len(ctx, obj))
				goto pop;
			key = pdf_dict_get_key(ctx, ws->stack[ws->len-1].obj, ws->stack[ws->len-1].pos);
			ws->stack[ws->len-1].pos++;

			if (pdf_name_eq(ctx, key, PDF_NAME(Parent)))
				goto step_next_dict_child;

			if (pdf_name_eq(ctx, key, PDF_NAME(Thumb)))
				type = AUDIT_THUMBNAILS;
			else if (pdf_name_eq(ctx, key, PDF_NAME(Outlines)))
				type = AUDIT_BOOKMARKS;
			else if (pdf_name_eq(ctx, key, PDF_NAME(Contents)))
				type = AUDIT_CONTENT_STREAMS;
			else if (pdf_name_eq(ctx, key, PDF_NAME(StructTreeRoot)))
				type = AUDIT_STRUCTURE_INFO;
			else if (pdf_name_eq(ctx, key, PDF_NAME(AcroForm)))
				type = AUDIT_FORMS;
			else if (pdf_name_eq(ctx, key, PDF_NAME(ColorSpace)))
				type = AUDIT_COLORSPACES;
			else if (pdf_name_eq(ctx, key, PDF_NAME(CS)))
				type = AUDIT_COLORSPACES;
			else if (pdf_name_eq(ctx, key, PDF_NAME(Dests)))
				type = AUDIT_NAMED_DESTINATIONS;
			else if (pdf_name_eq(ctx, key, PDF_NAME(ExtGState)))
				type = AUDIT_EXTGS;
			else if (pdf_name_eq(ctx, key, PDF_NAME(Resources)))
				type = AUDIT_RESOURCES;
			else if (pdf_name_eq(ctx, key, PDF_NAME(EmbeddedFile)))
				type = AUDIT_EMBEDDED_FILES;
			else if (pdf_name_eq(ctx, key, PDF_NAME(Metadata)))
				type = AUDIT_METADATA;

			/* OK. step onto the val. */
			obj = pdf_dict_get_val(ctx, ws->stack[ws->len-1].obj, ws->stack[ws->len-1].pos-1);
			continue;
		}
		else if (pdf_is_array(ctx, obj))
		{
step_next_array_child:
			if (ws->stack[ws->len-1].pos == pdf_array_len(ctx, obj))
				goto pop;

			obj = pdf_array_get(ctx, ws->stack[ws->len-1].obj, ws->stack[ws->len-1].pos);
			ws->stack[ws->len-1].pos++;
			continue;
		}

		/* Nothing more to do with this object. Pop back up. */
pop:
		if (pdf_is_indirect(ctx, obj))
		{
			pdf_unmark_obj(ctx, obj);
		}
		ws->len--;
visited:
		if (ws->len > 0)
		{
			/* We should either have stepped up to a dict or an array. */
			obj = ws->stack[ws->len-1].obj;
			type = ws->stack[ws->len-1].state;
			num = pdf_obj_parent_num(ctx, obj);
			if (pdf_is_dict(ctx, obj))
				goto step_next_dict_child;
			else if (pdf_is_array(ctx, obj))
				goto step_next_array_child;
			else
				fz_throw(ctx, FZ_ERROR_GENERIC, "this should never happen!");
		}
	}
	while (ws->len > 0);
}

static void
classify_by_walking(fz_context *ctx, pdf_document *doc, int n, obj_info_t *oi)
{
	walk_stack_t ws = { 0 };

	fz_try(ctx)
		walk(ctx, &ws, n, oi, pdf_trailer(ctx, doc), AUDIT_TRAILER);
	fz_always(ctx)
		fz_free(ctx, ws.stack);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
output_size(fz_context *ctx, fz_output *out, uint64_t file_size, const char *str, uint64_t size, uint64_t size2)
{
	fz_write_printf(ctx, out, "<tr align=right><td align=left>%s<td>%,ld<td>%.2f%%<td>%,ld<td>%.2f%%</tr>\n", str, size, 100.0f * size/file_size, size2, 100.0f * size2/file_size);
}

static void
filter_file(fz_context *ctx, fz_output *out, const char *filename)
{
	pdf_document *pdf = NULL;
	int i, n;
	pdf_obj *obj = NULL;
	obj_info_t *oi = NULL;
	op_usage_t ou = { 0 };

	fz_var(pdf);
	fz_var(obj);
	fz_var(i);
	fz_var(oi);

	fz_try(ctx)
	{
		pdf = pdf_open_document(ctx, filename);

		/* ensure we don't get tripped up by repair */
		pdf_check_document(ctx, pdf);

		n = pdf_xref_len(ctx, pdf);
		oi = fz_malloc_array(ctx, n, obj_info_t);
		memset(oi, 0, n * sizeof(obj_info_t));
		for (i = 1; i < n; i++)
		{
			fz_try(ctx)
			{
				for (; i <n; i++)
				{
					pdf_xref_entry *entry = pdf_cache_object(ctx, pdf, i);
					pdf_obj *type, *subtype;
					char text[128];

					if (entry == NULL || entry->obj == NULL)
						continue;
					oi[i].is_in_objstm = entry->type == 'o';
					if (!oi[i].is_in_objstm)
					{
						sprintf(text, "%d 0 obj\nendobj\n", i);
						oi[i].overhead = strlen(text);
					}
					else
					{
						sprintf(text, "%d %zd ", i, (size_t)entry->ofs);
						oi[i].overhead = strlen(text);
					}
					type = pdf_dict_get(ctx, entry->obj, PDF_NAME(Type));
					SWITCH (type)
					{
					CASE(PDF_NAME(ObjStm)):
						oi[i].type = AUDIT_OBJSTM;
						break;

					}
					subtype = pdf_dict_get(ctx, entry->obj, PDF_NAME(Subtype));
					SWITCH (subtype)
					{
					CASE(PDF_NAME(Image)):
						oi[i].type = AUDIT_IMAGES;
						break;
					}

					filter_obj(ctx, &oi[i], entry->obj);
					if (pdf_obj_num_is_stream(ctx, pdf, i))
					{
						filter_stream(ctx, pdf, i, &oi[i]);
						oi[i].stream_len = pdf_dict_get_int64(ctx, entry->obj, PDF_NAME(Length));
					}
					pdf_drop_obj(ctx, obj);
					obj = NULL;
				}
			}
			fz_catch(ctx)
			{
				i++;
				/* Swallow error */
			}
		}

		/* Walk the doc structure. */
		classify_by_walking(ctx, pdf, n, oi);

		/* Filter the content streams to establish operator usage */
		filter_page_streams(ctx, pdf, &ou);

		fz_write_printf(ctx, out, "<html><title>PDF Audit: %s</title><body>\n", filename);
		fz_write_printf(ctx, out, "<H1>Input file: %s</H1>\n", filename);
		fz_write_printf(ctx, out, "<p>Total file size: %,zd bytes</p>", pdf->file_size);

		fz_write_printf(ctx, out, "<H3>Total file usage</H3>\n");
		/* Sum the results */
		{
			struct {
				size_t obj;
				size_t objstm;
			} counts[AUDIT__MAX] = { 0 };
			size_t total_obj = 0;
			size_t total_objstm = 0;
			size_t overhead = 0;
			size_t objstm_overhead = 0;
			size_t total_stream_uncomp = 0;
			size_t total_stream_comp = 0;
			for (i = 1; i < n; i++)
			{
				size_t z = oi[i].textsize + oi[i].overhead + oi[i].stream_len;
				total_stream_uncomp += oi[i].len;
				total_stream_comp += oi[i].stream_len;
				if (oi[i].is_in_objstm)
				{
					objstm_overhead += oi[i].overhead;
					total_objstm += oi[i].textsize;
					counts[oi[i].type].objstm += z;
				}
				else
				{
					overhead += oi[i].overhead;
					total_obj += oi[i].textsize;
					counts[oi[i].type].obj += z;
				}
			}
			fz_write_printf(ctx, out, "<table border=1><thead><th><th colspan=2>not in objstms<th colspan=2>in objstms</thead>\n");
			output_size(ctx, out, pdf->file_size, "object text", total_obj, total_objstm);
			output_size(ctx, out, pdf->file_size, "object overhead", overhead, objstm_overhead);
			fz_write_printf(ctx, out, "<thead><th><th colspan=2>uncompressed<th colspan=2>compressed</thead>\n");
			output_size(ctx, out, pdf->file_size, "streams", total_stream_uncomp, total_stream_comp);
			fz_write_printf(ctx, out, "</table>\n");
			fz_write_printf(ctx, out, "<p>NOTE: The uncompressed streams percentage figure is misleading,"
						" as it is the percentage of the complete file which typically includes compression.</p>\n");
			fz_write_printf(ctx, out, "<table border=1><thead><th><th colspan=2>not in objstms<th colspan=2>in objstms</thead>\n");
			fz_write_printf(ctx, out, "<H3>Classified file usage</H3>\n");
			for (i = 0; i < AUDIT__MAX; i++)
			{
				output_size(ctx, out, pdf->file_size, audit_type[i], counts[i].obj, counts[i].objstm);
			}
			fz_write_printf(ctx, out, "</table>\n");
			fz_write_printf(ctx, out, "<p>NOTE: The percentages are as percentages of the complete file. This again means that"
						" the percentages in the &quot;in objstms&quot; column are misleading as the objstms are"
						" typically compressed!</p>\n");
		}

		/* List some unknown objects. */
		{
			int count = 0;

			for (i = 1; i < n; i++)
			{
				if (oi[i].type != AUDIT_UNKNOWN || oi[i].textsize == 0)
					continue;

				if (count == 0)
					fz_write_printf(ctx, out, "<p>Some objects still unknown: e.g. ");
				fz_write_printf(ctx, out, "%d ", i);
				count++;
				if (count == 10)
					break;
			}
			if (count > 0)
				fz_write_printf(ctx, out, "</p>\n");
		}

		/* Write out the operator usage */
		fz_write_printf(ctx, out, "<H3>Operator usage within streams</H3>\n");
		{
			size_t total = 0;
			for (i = 0; i < OP_END; i++)
				total += ou.len[i];
			fz_write_printf(ctx, out, "<table border=1><thead><th>Op<th>bytes<th></thead>\n");
			for (i = 0; i < OP_END; i++)
			{
				fz_write_printf(ctx, out, "<tr align=right><td align=left>%s<td>%,zd<td>%.2f%%</tr>\n", op_names[i], ou.len[i], 100.f * ou.len[i] / total);
			}
			fz_write_printf(ctx, out, "</table>\n");
			fz_write_printf(ctx, out, "<p>NOTE: The percentages are of the operator stream content found.</p>\n");
		}

	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, obj);
		pdf_drop_document(ctx, pdf);
		fz_free(ctx, oi);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool audit [options] input.pdf [input2.pdf ...]\n"
		"\t-o -\toutput file\n"
		);
	return 1;
}

int pdfaudit_main(int argc, char **argv)
{
	char *outfile = "-";
	int c;
	int errors = 0;
	int append = 0;
	fz_context *ctx;
	fz_output *out = NULL;
	const fz_getopt_long_options longopts[] =
	{
		{ NULL, NULL, NULL }
	};

	while ((c = fz_getopt_long(argc, argv, "o:", longopts)) != -1)
	{
		switch (c)
		{
		case 'o': outfile = fz_optpath(fz_optarg); break;
		case 0:
		{
			SWITCH(fz_optlong->opaque)
			{
			// Any future long options go here.
			default:
			case 0:
				assert(!"Never happens");
				break;
			break;
			}
		}
		default: return usage();
		}
	}

	if (argc - fz_optind < 1)
		return usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

#ifdef _WIN32
	fz_set_stddbg(ctx, fz_stdods(ctx));
#endif

	fz_var(out);

	fz_try(ctx)
	{
		out = fz_new_output_with_path(ctx, outfile, append);
		while (fz_optind < argc)
			filter_file(ctx, out, argv[fz_optind++]);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		errors++;
	}
	fz_drop_context(ctx);

	return errors != 0;
}
