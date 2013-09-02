#ifndef MUPDF_FITZ_GLYPH_H
#define MUPDF_FITZ_GLYPH_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/colorspace.h"

/*
	Glyphs represent a run length encoded set of pixels for a 2
	dimensional region of a plane.
*/
typedef struct fz_glyph_s fz_glyph;

/*
	fz_glyph_bbox: Return the bounding box for a glyph.
*/
fz_irect *fz_glyph_bbox(fz_context *ctx, fz_glyph *glyph, fz_irect *bbox);

/*
	fz_glyph_width: Return the width of the glyph in pixels.
*/
int fz_glyph_width(fz_context *ctx, fz_glyph *glyph);

/*
	fz_glyph_height: Return the height of the glyph in pixels.
*/
int fz_glyph_height(fz_context *ctx, fz_glyph *glyph);

/*
	fz_new_glyph_from_pixmap: Create a new glyph from a pixmap

	Returns a pointer to the new glyph. Throws exception on failure to
	allocate.
*/
fz_glyph *fz_new_glyph_from_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	fz_new_glyph_from_8bpp_data: Create a new glyph from 8bpp data

	x, y: X and Y position for the glyph

	w, h: Width and Height for the glyph

	sp: Source Pointer to data

	span: Increment from line to line of data

	Returns a pointer to the new glyph. Throws exception on failure to
	allocate.
*/
fz_glyph *fz_new_glyph_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

/*
	fz_new_glyph_from_1bpp_data: Create a new glyph from 1bpp data

	x, y: X and Y position for the glyph

	w, h: Width and Height for the glyph

	sp: Source Pointer to data

	span: Increment from line to line of data

	Returns a pointer to the new glyph. Throws exception on failure to
	allocate.
*/fz_glyph *fz_new_glyph_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);


/*
	fz_keep_glyph: Take a reference to a glyph.

	pix: The glyph to increment the reference for.

	Returns pix. Does not throw exceptions.
*/
fz_glyph *fz_keep_glyph(fz_context *ctx, fz_glyph *pix);

/*
	fz_drop_glyph: Drop a reference and free a glyph.

	Decrement the reference count for the glyph. When no
	references remain the glyph will be freed.

	Does not throw exceptions.
*/
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
	int size;
	unsigned char data[1];
};

static unsigned int fz_glyph_size(fz_context *ctx, fz_glyph *glyph);

fz_irect *fz_glyph_bbox_no_ctx(fz_glyph *src, fz_irect *bbox);

static inline unsigned int
fz_glyph_size(fz_context *ctx, fz_glyph *glyph)
{
	if (glyph == NULL)
		return 0;
	return sizeof(fz_glyph) + glyph->size + fz_pixmap_size(ctx, glyph->pixmap);
}

#endif
