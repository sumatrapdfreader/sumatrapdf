#ifndef MUPDF_FITZ_GLYPH_H
#define MUPDF_FITZ_GLYPH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/store.h"

/*
	Glyphs represent a run length encoded set of pixels for a 2
	dimensional region of a plane.
*/
typedef struct fz_glyph_s fz_glyph;

fz_irect fz_glyph_bbox(fz_context *ctx, fz_glyph *glyph);

int fz_glyph_width(fz_context *ctx, fz_glyph *glyph);

int fz_glyph_height(fz_context *ctx, fz_glyph *glyph);

fz_glyph *fz_new_glyph_from_pixmap(fz_context *ctx, fz_pixmap *pix);

fz_glyph *fz_new_glyph_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

fz_glyph *fz_new_glyph_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

fz_glyph *fz_keep_glyph(fz_context *ctx, fz_glyph *pix);

void fz_drop_glyph(fz_context *ctx, fz_glyph *pix);

/*
	Glyphs represent a set of pixels for a 2 dimensional region of a
	plane.

	x, y: The minimum x and y coord of the region in pixels.

	w, h: The width and height of the region in pixels.

	samples: The sample data. The sample data is in a compressed format
	designed to give reasonable compression, and to be fast to plot from.

	The first sizeof(int) * h bytes of the table, when interpreted as
	ints gives the offset within the data block of that lines data. An
	offset of 0 indicates that that line is completely blank.

	The data for individual lines is a sequence of bytes:
	 00000000 = end of lines data
	 LLLLLL00 = extend the length given in the next run by the 6 L bits
		given here.
	 LLLLLL01 = A run of length L+1 transparent pixels.
	 LLLLLE10 = A run of length L+1 solid pixels. If E then this is the
		last run on this line.
	 LLLLLE11 = A run of length L+1 intermediate pixels followed by L+1
		bytes of literal pixel data. If E then this is the last run
		on this line.
*/
struct fz_glyph_s
{
	fz_storable storable;
	int x, y, w, h;
	fz_pixmap *pixmap;
	size_t size;
	unsigned char data[1];
};

fz_irect fz_glyph_bbox_no_ctx(fz_glyph *src);

static inline size_t
fz_glyph_size(fz_context *ctx, fz_glyph *glyph)
{
	if (glyph == NULL)
		return 0;
	return sizeof(fz_glyph) + glyph->size + fz_pixmap_size(ctx, glyph->pixmap);
}

fz_path *fz_outline_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix ctm);
fz_path *fz_outline_ft_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm);
fz_glyph *fz_render_ft_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, int aa);
fz_pixmap *fz_render_ft_glyph_pixmap(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, int aa);
fz_glyph *fz_render_t3_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, fz_colorspace *model, const fz_irect *scissor, int aa);
fz_pixmap *fz_render_t3_glyph_pixmap(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, fz_colorspace *model, const fz_irect *scissor, int aa);
fz_glyph *fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, const fz_stroke_state *state, int aa);
fz_pixmap *fz_render_ft_stroked_glyph_pixmap(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, const fz_stroke_state *state, int aa);
fz_glyph *fz_render_glyph(fz_context *ctx, fz_font*, int gid, fz_matrix *, fz_colorspace *model, const fz_irect *scissor, int alpha, int aa);
fz_glyph *fz_render_stroked_glyph(fz_context *ctx, fz_font*, int, fz_matrix *, fz_matrix, const fz_stroke_state *stroke, const fz_irect *scissor, int aa);
fz_pixmap *fz_render_stroked_glyph_pixmap(fz_context *ctx, fz_font*, int, fz_matrix *, fz_matrix, const fz_stroke_state *stroke, const fz_irect *scissor, int aa);


#endif
