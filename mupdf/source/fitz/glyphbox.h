#ifndef MUPDF_GLYPHBOX_H
#define MUPDF_GLYPHBOX_H

#include "mupdf/fitz.h"

/*
 * Returns 1 if glyph is entirely outside box; otherwise returns 0.
 *
 * Uses rectangle bbox for the glyph internally and can return false negative -
 * i.e. can return zero even if actually no part of the glyph is inside box.
 */
int fz_glyph_entirely_outside_box(fz_context *ctx, fz_matrix *ctm, fz_text_span *span, fz_text_item *item, fz_rect *box);

#endif
