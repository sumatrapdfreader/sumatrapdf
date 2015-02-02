#ifndef MUPDF_FITZ_PIXMAP_H
#define MUPDF_FITZ_PIXMAP_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/colorspace.h"

/*
	Pixmaps represent a set of pixels for a 2 dimensional region of a
	plane. Each pixel has n components per pixel, the last of which is
	always alpha. The data is in premultiplied alpha when rendering, but
	non-premultiplied for colorspace conversions and rescaling.
*/
typedef struct fz_pixmap_s fz_pixmap;

/*
	fz_pixmap_bbox: Return the bounding box for a pixmap.
*/
fz_irect *fz_pixmap_bbox(fz_context *ctx, fz_pixmap *pix, fz_irect *bbox);

/*
	fz_pixmap_width: Return the width of the pixmap in pixels.
*/
int fz_pixmap_width(fz_context *ctx, fz_pixmap *pix);

/*
	fz_pixmap_height: Return the height of the pixmap in pixels.
*/
int fz_pixmap_height(fz_context *ctx, fz_pixmap *pix);

/*
	fz_new_pixmap: Create a new pixmap, with it's origin at (0,0)

	cs: The colorspace to use for the pixmap, or NULL for an alpha
	plane/mask.

	w: The width of the pixmap (in pixels)

	h: The height of the pixmap (in pixels)

	Returns a pointer to the new pixmap. Throws exception on failure to
	allocate.
*/
fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *cs, int w, int h);

/*
	fz_new_pixmap_with_bbox: Create a pixmap of a given size,
	location and pixel format.

	The bounding box specifies the size of the created pixmap and
	where it will be located. The colorspace determines the number
	of components per pixel. Alpha is always present. Pixmaps are
	reference counted, so drop references using fz_drop_pixmap.

	colorspace: Colorspace format used for the created pixmap. The
	pixmap will keep a reference to the colorspace.

	bbox: Bounding box specifying location/size of created pixmap.

	Returns a pointer to the new pixmap. Throws exception on failure to
	allocate.
*/
fz_pixmap *fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *colorspace, const fz_irect *bbox);

/*
	fz_new_pixmap_with_data: Create a new pixmap, with it's origin at
	(0,0) using the supplied data block.

	cs: The colorspace to use for the pixmap, or NULL for an alpha
	plane/mask.

	w: The width of the pixmap (in pixels)

	h: The height of the pixmap (in pixels)

	samples: The data block to keep the samples in.

	Returns a pointer to the new pixmap. Throws exception on failure to
	allocate.
*/
fz_pixmap *fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *colorspace, int w, int h, unsigned char *samples);

/*
	fz_new_pixmap_with_bbox_and_data: Create a pixmap of a given size,
	location and pixel format, using the supplied data block.

	The bounding box specifies the size of the created pixmap and
	where it will be located. The colorspace determines the number
	of components per pixel. Alpha is always present. Pixmaps are
	reference counted, so drop references using fz_drop_pixmap.

	colorspace: Colorspace format used for the created pixmap. The
	pixmap will keep a reference to the colorspace.

	bbox: Bounding box specifying location/size of created pixmap.

	samples: The data block to keep the samples in.

	Returns a pointer to the new pixmap. Throws exception on failure to
	allocate.
*/
fz_pixmap *fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *colorspace, const fz_irect *rect, unsigned char *samples);

/*
	fz_keep_pixmap: Take a reference to a pixmap.

	pix: The pixmap to increment the reference for.

	Returns pix. Does not throw exceptions.
*/
fz_pixmap *fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	fz_drop_pixmap: Drop a reference and free a pixmap.

	Decrement the reference count for the pixmap. When no
	references remain the pixmap will be freed.

	Does not throw exceptions.
*/
void fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	fz_pixmap_colorspace: Return the colorspace of a pixmap

	Returns colorspace. Does not throw exceptions.
*/
fz_colorspace *fz_pixmap_colorspace(fz_context *ctx, fz_pixmap *pix);

/*
	fz_pixmap_components: Return the number of components in a pixmap.

	Returns the number of components. Does not throw exceptions.
*/
int fz_pixmap_components(fz_context *ctx, fz_pixmap *pix);

/*
	fz_pixmap_samples: Returns a pointer to the pixel data of a pixmap.

	Returns the pointer. Does not throw exceptions.
*/
unsigned char *fz_pixmap_samples(fz_context *ctx, fz_pixmap *pix);

void fz_pixmap_set_resolution(fz_pixmap *pix, int res);

/*
	fz_clear_pixmap_with_value: Clears a pixmap with the given value.

	pix: The pixmap to clear.

	value: Values in the range 0 to 255 are valid. Each component
	sample for each pixel in the pixmap will be set to this value,
	while alpha will always be set to 255 (non-transparent).

	Does not throw exceptions.
*/
void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value);

/*
	fz_clear_pixmap_with_value: Clears a subrect of a pixmap with the given value.

	pix: The pixmap to clear.

	value: Values in the range 0 to 255 are valid. Each component
	sample for each pixel in the pixmap will be set to this value,
	while alpha will always be set to 255 (non-transparent).

	r: the rectangle.

	Does not throw exceptions.
*/
void fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *pix, int value, const fz_irect *r);

/*
	fz_clear_pixmap_with_value: Sets all components (including alpha) of
	all pixels in a pixmap to 0.

	pix: The pixmap to clear.

	Does not throw exceptions.
*/
void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	fz_invert_pixmap: Invert all the pixels in a pixmap. All components
	of all pixels are inverted (except alpha, which is unchanged).

	Does not throw exceptions.
*/
void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	fz_tint_pixmap: Tint all the pixels in an RGB or Gray pixmap.

	Multiplies all the samples with the input color argument.

	r,g,b: The color to tint with, in 0 to 255 range.
*/
void fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int r, int g, int b);

/*
	fz_invert_pixmap: Invert all the pixels in a given rectangle of a
	pixmap. All components of all pixels in the rectangle are inverted
	(except alpha, which is unchanged).

	Does not throw exceptions.
*/
void fz_invert_pixmap_rect(fz_pixmap *image, const fz_irect *rect);

/*
	fz_gamma_pixmap: Apply gamma correction to a pixmap. All components
	of all pixels are modified (except alpha, which is unchanged).

	gamma: The gamma value to apply; 1.0 for no change.

	Does not throw exceptions.
*/
void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma);

/*
	fz_unmultiply_pixmap: Convert a pixmap from premultiplied to
	non-premultiplied format.

	Does not throw exceptions.
*/
void fz_unmultiply_pixmap(fz_context *ctx, fz_pixmap *pix);

/*
	fz_convert_pixmap: Convert from one pixmap to another (assumed to be
	the same size, but possibly with a different colorspace).

	dst: the source pixmap.

	src: the destination pixmap.
*/
void fz_convert_pixmap(fz_context *ctx, fz_pixmap *dst, fz_pixmap *src);

/*
	Pixmaps represent a set of pixels for a 2 dimensional region of a
	plane. Each pixel has n components per pixel, the last of which is
	always alpha. The data is in premultiplied alpha when rendering, but
	non-premultiplied for colorspace conversions and rescaling.

	x, y: The minimum x and y coord of the region in pixels.

	w, h: The width and height of the region in pixels.

	n: The number of color components in the image. Always
	includes a separate alpha channel. For mask images n=1, for greyscale
	(plus alpha) images n=2, for rgb (plus alpha) images n=3.

	interpolate: A boolean flag set to non-zero if the image
	will be drawn using linear interpolation, or set to zero if
	image will be using nearest neighbour sampling.

	xres, yres: Image resolution in dpi. Default is 96 dpi.

	colorspace: Pointer to a colorspace object describing the colorspace
	the pixmap is in. If NULL, the image is a mask.

	samples: A simple block of memory w * h * n bytes of memory in which
	the components are stored. The first n bytes are components 0 to n-1
	for the pixel at (x,y). Each successive n bytes gives another pixel
	in scanline order. Subsequent scanlines follow on with no padding.

	free_samples: Is zero when an application has provided its own
	buffer for pixel data through fz_new_pixmap_with_bbox_and_data.
	If not zero the buffer will be freed when fz_drop_pixmap is
	called for the pixmap.
*/
struct fz_pixmap_s
{
	fz_storable storable;
	int x, y, w, h, n;
	int interpolate;
	int xres, yres;
	fz_colorspace *colorspace;
	unsigned char *samples;
	int free_samples;
};

void fz_free_pixmap_imp(fz_context *ctx, fz_storable *pix);

void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, const fz_irect *r);
void fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
fz_pixmap *fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray, int luminosity);
unsigned int fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, fz_irect *clip);

typedef struct fz_scale_cache_s fz_scale_cache;

fz_scale_cache *fz_new_scale_cache(fz_context *ctx);
void fz_free_scale_cache(fz_context *ctx, fz_scale_cache *cache);
fz_pixmap *fz_scale_pixmap_cached(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip, fz_scale_cache *cache_x, fz_scale_cache *cache_y);

void fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor);

fz_irect *fz_pixmap_bbox_no_ctx(fz_pixmap *src, fz_irect *bbox);

void fz_decode_tile(fz_pixmap *pix, float *decode);
void fz_decode_indexed_tile(fz_pixmap *pix, float *decode, int maxval);
void fz_unpack_tile(fz_pixmap *dst, unsigned char * restrict src, int n, int depth, int stride, int scale);

/*
	fz_md5_pixmap: Return the md5 digest for a pixmap
*/
void fz_md5_pixmap(fz_pixmap *pixmap, unsigned char digest[16]);

fz_pixmap *fz_new_pixmap_from_8bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);
fz_pixmap *fz_new_pixmap_from_1bpp_data(fz_context *ctx, int x, int y, int w, int h, unsigned char *sp, int span);

#endif
