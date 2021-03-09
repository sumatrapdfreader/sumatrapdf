#include "glyphbox.h"

int fz_glyph_entirely_outside_box(fz_context *ctx, fz_matrix *ctm, fz_text_span *span, fz_text_item *item, fz_rect *box)
{
	fz_rect glyph_rect = fz_bound_glyph(ctx, span->font, item->gid, span->trm);
	glyph_rect.x0 += item->x;
	glyph_rect.y0 += item->y;
	glyph_rect.x1 += item->x;
	glyph_rect.y1 += item->y;
	glyph_rect = fz_transform_rect(glyph_rect, *ctm);
	if (glyph_rect.x1 <= box->x0 ||
			glyph_rect.y1 <= box->y0 ||
			glyph_rect.x0 >= box->x1 ||
			glyph_rect.y0 >= box->y1)
		return 1;
	return 0;
}
