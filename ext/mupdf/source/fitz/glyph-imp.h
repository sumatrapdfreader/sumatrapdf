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

#ifndef FITZ_GLYPH_IMP_H
#define FITZ_GLYPH_IMP_H


/*
	Glyphs represent a run length encoded set of pixels for a 2
	dimensional region of a plane.

	x, y: The minimum x and y coord of the region in pixels.

	w, h: The width and height of the region in pixels.

	samples: The sample data. The sample data is in a compressed
	format designed to give reasonable compression, and to be fast
	to plot from.

	The first sizeof(int) * h bytes of the table, when interpreted
	as ints gives the offset within the data block of that lines
	data. An offset of 0 indicates that that line is completely
	blank.

	The data for individual lines is a sequence of bytes:
	 00000000 = end of lines data
	 LLLLLL00 = extend the length given in the next run by the 6 L
		bits given here.
	 LLLLLL01 = A run of length L+1 transparent pixels.
	 LLLLLE10 = A run of length L+1 solid pixels. If E then this is
		the last run on this line.
	 LLLLLE11 = A run of length L+1 intermediate pixels followed by
		L+1 bytes of literal pixel data. If E then this is the
		last run on this line.
*/
struct fz_glyph
{
	fz_storable storable;
	int x, y, w, h;
	fz_pixmap *pixmap;
	size_t size;
	unsigned char data[FZ_FLEXIBLE_ARRAY];
};

/*
	Create a new glyph from a pixmap

	Returns a pointer to the new glyph. Throws exception on failure
	to allocate.
*/
fz_glyph *fz_new_glyph_from_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	Create a new glyph from 8bpp data

	x, y: X and Y position for the glyph

	w, h: Width and Height for the glyph

	sp: Source Pointer to data

	span: Increment from line to line of data

	Returns a pointer to the new glyph. Throws exception on failure
	to allocate.
*/
fz_glyph *fz_new_glyph_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

/*
	Create a new glyph from 1bpp data

	x, y: X and Y position for the glyph

	w, h: Width and Height for the glyph

	sp: Source Pointer to data

	span: Increment from line to line of data

	Returns a pointer to the new glyph. Throws exception on failure
	to allocate.
*/
fz_glyph *fz_new_glyph_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

fz_path *fz_outline_ft_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm);
fz_glyph *fz_render_ft_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, int aa);
fz_pixmap *fz_render_ft_glyph_pixmap(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, int aa);
fz_glyph *fz_render_t3_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, fz_colorspace *model, const fz_irect *scissor, int aa);
fz_pixmap *fz_render_t3_glyph_pixmap(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, fz_colorspace *model, const fz_irect *scissor, int aa);
fz_glyph *fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, const fz_stroke_state *state, int aa);
fz_glyph *fz_render_glyph(fz_context *ctx, fz_font*, int gid, fz_matrix *, fz_colorspace *model, const fz_irect *scissor, int alpha, int aa);
fz_glyph *fz_render_stroked_glyph(fz_context *ctx, fz_font*, int, fz_matrix *, fz_matrix, fz_colorspace *model, const fz_stroke_state *stroke, const fz_irect *scissor, int aa);

#endif
