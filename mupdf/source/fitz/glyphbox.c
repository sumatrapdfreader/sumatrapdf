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
