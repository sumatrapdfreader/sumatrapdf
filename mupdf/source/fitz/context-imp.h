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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef FITZ_CONTEXT_IMP_H
#define FITZ_CONTEXT_IMP_H

#include "mupdf/fitz.h"

extern fz_alloc_context fz_alloc_default;
extern fz_locks_context fz_locks_default;

/* FIXME: Make all these static? */
double fz_drand48(fz_context *ctx);
int32_t fz_lrand48(fz_context *ctx);
int32_t fz_mrand48(fz_context *ctx);
double fz_erand48(fz_context *ctx, uint16_t xsubi[3]);
int32_t fz_jrand48(fz_context *ctx, uint16_t xsubi[3]);
int32_t fz_nrand48(fz_context *ctx, uint16_t xsubi[3]);
void fz_lcong48(fz_context *ctx, uint16_t param[7]);
uint16_t *fz_seed48(fz_context *ctx, uint16_t seed16v[3]);
void fz_srand48(fz_context *ctx, int32_t seedval);

void fz_new_colorspace_context(fz_context *ctx);
fz_colorspace_context *fz_keep_colorspace_context(fz_context *ctx);
void fz_drop_colorspace_context(fz_context *ctx);

void fz_new_font_context(fz_context *ctx);
fz_font_context *fz_keep_font_context(fz_context *ctx);
void fz_drop_font_context(fz_context *ctx);

void fz_new_hyph_context(fz_context *ctx);
fz_hyph_context *fz_keep_hyph_context(fz_context *ctx);
void fz_drop_hyph_context(fz_context *ctx);

struct fz_tuning_context
{
	int refs;
	fz_tune_image_decode_fn *image_decode;
	void *image_decode_arg;
	fz_tune_image_scale_fn *image_scale;
	void *image_scale_arg;
};

void fz_default_image_decode(void *arg, int w, int h, int l2factor, fz_irect *subarea);
int fz_default_image_scale(void *arg, int dst_w, int dst_h, int src_w, int src_h);

void fz_init_aa_context(fz_context *ctx);

void fz_new_glyph_cache_context(fz_context *ctx);
fz_glyph_cache *fz_keep_glyph_cache(fz_context *ctx);
void fz_drop_glyph_cache_context(fz_context *ctx);

void fz_new_document_handler_context(fz_context *ctx);
void fz_drop_document_handler_context(fz_context *ctx);
fz_document_handler_context *fz_keep_document_handler_context(fz_context *ctx);

void fz_new_archive_handler_context(fz_context *ctx);
void fz_drop_archive_handler_context(fz_context *ctx);
fz_archive_handler_context *fz_keep_archive_handler_context(fz_context *ctx);

void fz_log_activity(fz_context *ctx, fz_activity_reason reason, void *arg);

int fz_new_document_id(fz_context *ctx);

#endif
