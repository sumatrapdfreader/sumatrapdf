#ifndef MUPDF_FITZ_GLYPH_CACHE_H
#define MUPDF_FITZ_GLYPH_CACHE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/path.h"
#include "mupdf/fitz/pixmap.h"
#include "mupdf/fitz/device.h"

/*
 * Glyph cache
 */

void fz_new_glyph_cache_context(fz_context *ctx);
fz_glyph_cache *fz_keep_glyph_cache(fz_context *ctx);
void fz_drop_glyph_cache_context(fz_context *ctx);
void fz_purge_glyph_cache(fz_context *ctx);

fz_path *fz_outline_ft_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm);
fz_path *fz_outline_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *ctm);
fz_pixmap *fz_render_ft_glyph(fz_context *ctx, fz_font *font, int cid, const fz_matrix *trm, int aa);
fz_pixmap *fz_render_t3_glyph(fz_context *ctx, fz_font *font, int cid, const fz_matrix *trm, fz_colorspace *model, fz_irect scissor);
fz_pixmap *fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, const fz_matrix *trm, const fz_matrix *ctm, fz_stroke_state *state);
fz_pixmap *fz_render_glyph(fz_context *ctx, fz_font*, int, const fz_matrix *, fz_colorspace *model, fz_irect scissor);
fz_pixmap *fz_render_stroked_glyph(fz_context *ctx, fz_font*, int, const fz_matrix *, const fz_matrix *, fz_stroke_state *stroke, fz_irect scissor);
void fz_render_t3_glyph_direct(fz_context *ctx, fz_device *dev, fz_font *font, int gid, const fz_matrix *trm, void *gstate, int nestedDepth);
void fz_prepare_t3_glyph(fz_context *ctx, fz_font *font, int gid, int nestedDepth);

#endif
