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

#ifndef MUPDF_PDF_FONT_H
#define MUPDF_PDF_FONT_H

#include "mupdf/pdf/cmap.h"

enum
{
	PDF_FD_FIXED_PITCH = 1 << 0,
	PDF_FD_SERIF = 1 << 1,
	PDF_FD_SYMBOLIC = 1 << 2,
	PDF_FD_SCRIPT = 1 << 3,
	PDF_FD_NONSYMBOLIC = 1 << 5,
	PDF_FD_ITALIC = 1 << 6,
	PDF_FD_ALL_CAP = 1 << 16,
	PDF_FD_SMALL_CAP = 1 << 17,
	PDF_FD_FORCE_BOLD = 1 << 18
};

void pdf_load_encoding(const char **estrings, const char *encoding);

typedef struct
{
	unsigned short lo;
	unsigned short hi;
	int w;	/* type3 fonts can be big! */
} pdf_hmtx;

typedef struct
{
	unsigned short lo;
	unsigned short hi;
	short x;
	short y;
	short w;
} pdf_vmtx;

typedef struct
{
	fz_storable storable;
	size_t size;

	fz_font *font;

	/* FontDescriptor */
	int flags;
	float italic_angle;
	float ascent;
	float descent;
	float cap_height;
	float x_height;
	float missing_width;

	/* Encoding (CMap) */
	pdf_cmap *encoding;
	pdf_cmap *to_ttf_cmap;
	size_t cid_to_gid_len;
	unsigned short *cid_to_gid;

	/* ToUnicode */
	pdf_cmap *to_unicode;
	size_t cid_to_ucs_len;
	unsigned short *cid_to_ucs;

	/* Metrics (given in the PDF file) */
	int wmode;

	int hmtx_len, hmtx_cap;
	pdf_hmtx dhmtx;
	pdf_hmtx *hmtx;

	int vmtx_len, vmtx_cap;
	pdf_vmtx dvmtx;
	pdf_vmtx *vmtx;

	int is_embedded;
} pdf_font_desc;

void pdf_set_font_wmode(fz_context *ctx, pdf_font_desc *font, int wmode);
void pdf_set_default_hmtx(fz_context *ctx, pdf_font_desc *font, int w);
void pdf_set_default_vmtx(fz_context *ctx, pdf_font_desc *font, int y, int w);
void pdf_add_hmtx(fz_context *ctx, pdf_font_desc *font, int lo, int hi, int w);
void pdf_add_vmtx(fz_context *ctx, pdf_font_desc *font, int lo, int hi, int x, int y, int w);
void pdf_end_hmtx(fz_context *ctx, pdf_font_desc *font);
void pdf_end_vmtx(fz_context *ctx, pdf_font_desc *font);
pdf_hmtx pdf_lookup_hmtx(fz_context *ctx, pdf_font_desc *font, int cid);
pdf_vmtx pdf_lookup_vmtx(fz_context *ctx, pdf_font_desc *font, int cid);

void pdf_load_to_unicode(fz_context *ctx, pdf_document *doc, pdf_font_desc *font, const char **strings, char *collection, pdf_obj *cmapstm);

int pdf_font_cid_to_gid(fz_context *ctx, pdf_font_desc *fontdesc, int cid);
const char *pdf_clean_font_name(const char *fontname);

const unsigned char *pdf_lookup_substitute_font(fz_context *ctx, int mono, int serif, int bold, int italic, int *len);

pdf_font_desc *pdf_load_type3_font(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *obj);
void pdf_load_type3_glyphs(fz_context *ctx, pdf_document *doc, pdf_font_desc *fontdesc);
pdf_font_desc *pdf_load_font(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *obj);
pdf_font_desc *pdf_load_hail_mary_font(fz_context *ctx, pdf_document *doc);

pdf_font_desc *pdf_new_font_desc(fz_context *ctx);
pdf_font_desc *pdf_keep_font(fz_context *ctx, pdf_font_desc *fontdesc);
void pdf_drop_font(fz_context *ctx, pdf_font_desc *font);

void pdf_print_font(fz_context *ctx, fz_output *out, pdf_font_desc *fontdesc);

void pdf_run_glyph(fz_context *ctx, pdf_document *doc, pdf_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm, void *gstate, fz_default_colorspaces *default_cs);

pdf_obj *pdf_add_simple_font(fz_context *ctx, pdf_document *doc, fz_font *font, int encoding);

/*
	Creates CID font with Identity-H CMap and a ToUnicode CMap that
	is created by using the TTF cmap table "backwards" to go from
	the GID to a Unicode value.

	We can possibly get width information that may have been embedded
	in the PDF /W array (or W2 if vertical text)
*/
pdf_obj *pdf_add_cid_font(fz_context *ctx, pdf_document *doc, fz_font *font);

/*
	Add a non-embedded UTF16-encoded CID-font for the CJK scripts:
	CNS1, GB1, Japan1, or Korea1
*/
pdf_obj *pdf_add_cjk_font(fz_context *ctx, pdf_document *doc, fz_font *font, int script, int wmode, int serif);

/*
	Add a substitute font for any script.
*/
pdf_obj *pdf_add_substitute_font(fz_context *ctx, pdf_document *doc, fz_font *font);

int pdf_font_writing_supported(fz_font *font);

#endif
