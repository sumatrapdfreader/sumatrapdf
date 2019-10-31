#ifndef MUPDF_FITZ_GLYPH_CACHE_H
#define MUPDF_FITZ_GLYPH_CACHE_H

#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/pixmap.h"

void fz_purge_glyph_cache(fz_context *ctx);
fz_pixmap *fz_render_glyph_pixmap(fz_context *ctx, fz_font*, int, fz_matrix *, const fz_irect *scissor, int aa);
void fz_render_t3_glyph_direct(fz_context *ctx, fz_device *dev, fz_font *font, int gid, fz_matrix trm, void *gstate, fz_default_colorspaces *def_cs);
void fz_prepare_t3_glyph(fz_context *ctx, fz_font *font, int gid);
void fz_dump_glyph_cache_stats(fz_context *ctx);
float fz_subpixel_adjust(fz_context *ctx, fz_matrix *ctm, fz_matrix *subpix_ctm, unsigned char *qe, unsigned char *qf);

#endif
