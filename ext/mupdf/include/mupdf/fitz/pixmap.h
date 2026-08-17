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

#ifndef MUPDF_FITZ_PIXMAP_H
#define MUPDF_FITZ_PIXMAP_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/separation.h"

/**
	Pixmaps represent a set of pixels for a 2 dimensional region of
	a plane. Each pixel has n components per pixel. The components
	are in the order process-components, spot-colors, alpha, where
	there can be 0 of any of those types. The data is in
	premultiplied alpha when rendering, but non-premultiplied for
	colorspace conversions and rescaling.
*/

typedef struct fz_overprint fz_overprint;

/**
	Return the bounding box for a pixmap.
*/
fz_irect fz_pixmap_bbox(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the width of the pixmap in pixels.
*/
int fz_pixmap_width(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the height of the pixmap in pixels.
*/
int fz_pixmap_height(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the x value of the pixmap in pixels.
*/
int fz_pixmap_x(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the y value of the pixmap in pixels.
*/
int fz_pixmap_y(fz_context *ctx, const fz_pixmap *pix);

/**
	Return sizeof fz_pixmap plus size of data, in bytes.
*/
size_t fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

/**
	Create a new pixmap, with its origin at (0,0)

	cs: The colorspace to use for the pixmap, or NULL for an alpha
	plane/mask.

	w: The width of the pixmap (in pixels)

	h: The height of the pixmap (in pixels)

	seps: Details of separations.

	alpha: 0 for no alpha, 1 for alpha.

	Returns a pointer to the new pixmap. Throws exception on failure
	to allocate.
*/
fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *cs, int w, int h, fz_separations *seps, int alpha);

/**
	Create a pixmap of a given size, location and pixel format.

	The bounding box specifies the size of the created pixmap and
	where it will be located. The colorspace determines the number
	of components per pixel. Alpha is always present. Pixmaps are
	reference counted, so drop references using fz_drop_pixmap.

	colorspace: Colorspace format used for the created pixmap. The
	pixmap will keep a reference to the colorspace.

	bbox: Bounding box specifying location/size of created pixmap.

	seps: Details of separations.

	alpha: 0 for no alpha, 1 for alpha.

	Returns a pointer to the new pixmap. Throws exception on failure
	to allocate.
*/
fz_pixmap *fz_new_pixmap_with_bbox(fz_context *ctx, fz_colorspace *colorspace, fz_irect bbox, fz_separations *seps, int alpha);

/**
	Create a new pixmap, with its origin at
	(0,0) using the supplied data block.

	cs: The colorspace to use for the pixmap, or NULL for an alpha
	plane/mask.

	w: The width of the pixmap (in pixels)

	h: The height of the pixmap (in pixels)

	seps: Details of separations.

	alpha: 0 for no alpha, 1 for alpha.

	stride: The byte offset from the pixel data in a row to the
	pixel data in the next row.

	samples: The data block to keep the samples in.

	Returns a pointer to the new pixmap. Throws exception on failure to
	allocate.
*/
fz_pixmap *fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *colorspace, int w, int h, fz_separations *seps, int alpha, int stride, unsigned char *samples);

/**
	Create a pixmap of a given size, location and pixel format,
	using the supplied data block.

	The bounding box specifies the size of the created pixmap and
	where it will be located. The colorspace determines the number
	of components per pixel. Alpha is always present. Pixmaps are
	reference counted, so drop references using fz_drop_pixmap.

	colorspace: Colorspace format used for the created pixmap. The
	pixmap will keep a reference to the colorspace.

	rect: Bounding box specifying location/size of created pixmap.

	seps: Details of separations.

	alpha: Number of alpha planes (0 or 1).

	samples: The data block to keep the samples in.

	Returns a pointer to the new pixmap. Throws exception on failure
	to allocate.
*/
fz_pixmap *fz_new_pixmap_with_bbox_and_data(fz_context *ctx, fz_colorspace *colorspace, fz_irect rect, fz_separations *seps, int alpha, unsigned char *samples);

/**
	Create a new pixmap that represents a subarea of the specified
	pixmap. A reference is taken to this pixmap that will be dropped
	on destruction.

	The supplied rectangle must be wholly contained within the
	original pixmap.

	Returns a pointer to the new pixmap. Throws exception on failure
	to allocate.
*/
fz_pixmap *fz_new_pixmap_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, const fz_irect *rect);

/**
	Clone a pixmap, copying the pixels and associated data to new
	storage.

	The reference count of 'old' is unchanged.
*/
fz_pixmap *fz_clone_pixmap(fz_context *ctx, const fz_pixmap *old);

/**
	Increment the reference count for the pixmap. The same pointer
	is returned.

	Never throws exceptions.
*/
fz_pixmap *fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix);

/**
	Decrement the reference count for the pixmap. When the
	reference count hits 0, the pixmap is freed.

	Never throws exceptions.
*/
void fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix);

/**
	Return the colorspace of a pixmap

	Returns colorspace.
*/
fz_colorspace *fz_pixmap_colorspace(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the number of components in a pixmap.

	Returns the number of components (including spots and alpha).
*/
int fz_pixmap_components(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the number of colorants in a pixmap.

	Returns the number of colorants (components, less any spots and
	alpha).
*/
int fz_pixmap_colorants(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the number of spots in a pixmap.

	Returns the number of spots (components, less colorants and
	alpha). Does not throw exceptions.
*/
int fz_pixmap_spots(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the number of alpha planes in a pixmap.

	Returns the number of alphas. Does not throw exceptions.
*/
int fz_pixmap_alpha(fz_context *ctx, const fz_pixmap *pix);

/**
	Returns a pointer to the pixel data of a pixmap.

	Returns the pointer.
*/
unsigned char *fz_pixmap_samples(fz_context *ctx, const fz_pixmap *pix);

/**
	Return the number of bytes in a row in the pixmap.
*/
int fz_pixmap_stride(fz_context *ctx, const fz_pixmap *pix);

/**
	Set the pixels per inch resolution of the pixmap.
*/
void fz_set_pixmap_resolution(fz_context *ctx, fz_pixmap *pix, int xres, int yres);

/**
	Clears a pixmap with the given value.

	pix: The pixmap to clear.

	value: Values in the range 0 to 255 are valid. Each component
	sample for each pixel in the pixmap will be set to this value,
	while alpha will always be set to 255 (non-transparent).

	This function is horrible, and should be removed from the
	API and replaced with a less magic one.
*/
void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value);

/**
	Fill pixmap with solid color.
*/
void fz_fill_pixmap_with_color(fz_context *ctx, fz_pixmap *pix, fz_colorspace *colorspace, float *color, fz_color_params color_params);

/**
	Clears a subrect of a pixmap with the given value.

	pix: The pixmap to clear.

	value: Values in the range 0 to 255 are valid. Each component
	sample for each pixel in the pixmap will be set to this value,
	while alpha will always be set to 255 (non-transparent).

	r: the rectangle.
*/
void fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *pix, int value, fz_irect r);

/**
	Sets all components (including alpha) of
	all pixels in a pixmap to 0.

	pix: The pixmap to clear.
*/
void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix);

/**
	Invert all the pixels in a pixmap. All components (process and
	spots) of all pixels are inverted (except alpha, which is
	unchanged).
*/
void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix);

/**
	Invert the alpha of all the pixels in a pixmap.
*/
void fz_invert_pixmap_alpha(fz_context *ctx, fz_pixmap *pix);

/**
	Transform the pixels in a pixmap so that luminance of each
	pixel is inverted, and the chrominance remains unchanged (as
	much as accuracy allows).

	All components of all pixels are inverted (except alpha, which
	is unchanged). Only supports Grey and RGB bitmaps.
*/
void fz_invert_pixmap_luminance(fz_context *ctx, fz_pixmap *pix);

/**
	Tint all the pixels in an RGB, BGR, or Gray pixmap.

	black: Map black to this hexadecimal RGB color.

	white: Map white to this hexadecimal RGB color.
*/
void fz_tint_pixmap(fz_context *ctx, fz_pixmap *pix, int black, int white);

/**
	Invert all the pixels in a given rectangle of a (premultiplied)
	pixmap. All components of all pixels in the rectangle are
	inverted (except alpha, which is unchanged).
*/
void fz_invert_pixmap_rect(fz_context *ctx, fz_pixmap *image, fz_irect rect);

/**
	Invert all the pixels in a non-premultiplied pixmap in a
	very naive manner.
*/
void fz_invert_pixmap_raw(fz_context *ctx, fz_pixmap *pix);

/**
	Apply gamma correction to a pixmap. All components
	of all pixels are modified (except alpha, which is unchanged).

	gamma: The gamma value to apply; 1.0 for no change.
*/
void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma);

/**
	Convert an existing pixmap to a desired
	colorspace. Other properties of the pixmap, such as resolution
	and position are copied to the converted pixmap.

	pix: The pixmap to convert.

	default_cs: If NULL pix->colorspace is used. It is possible that
	the data may need to be interpreted as one of the color spaces
	in default_cs.

	cs_des: Desired colorspace, may be NULL to denote alpha-only.

	prf: Proofing color space through which we need to convert.

	color_params: Parameters that may be used in conversion (e.g.
	ri).

	keep_alpha: If 0 any alpha component is removed, otherwise
	alpha is kept if present in the pixmap.
*/
fz_pixmap *fz_convert_pixmap(fz_context *ctx, const fz_pixmap *pix, fz_colorspace *cs_des, fz_colorspace *prf, fz_default_colorspaces *default_cs, fz_color_params color_params, int keep_alpha);

/**
	Check if the pixmap is a 1-channel image containing samples with
	only values 0 and 255
*/
int fz_is_pixmap_monochrome(fz_context *ctx, fz_pixmap *pixmap);

/* Implementation details: subject to change.*/

fz_pixmap *fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray);
fz_pixmap *fz_alpha_from_rgb(fz_context *ctx, fz_pixmap *color);
void fz_decode_tile(fz_context *ctx, fz_pixmap *pix, const float *decode);
void fz_md5_pixmap(fz_context *ctx, fz_pixmap *pixmap, unsigned char digest[16]);

fz_stream *
fz_unpack_stream(fz_context *ctx, fz_stream *src, int depth, int w, int h, int n, int indexed, int pad, int skip);

/**
	Pixmaps represent a set of pixels for a 2 dimensional region of
	a plane. Each pixel has n components per pixel. The components
	are in the order process-components, spot-colors, alpha, where
	there can be 0 of any of those types. The data is in
	premultiplied alpha when rendering, but non-premultiplied for
	colorspace conversions and rescaling.

	x, y: The minimum x and y coord of the region in pixels.

	w, h: The width and height of the region in pixels.

	n: The number of color components in the image.
		n = num composite colors + num spots + num alphas

	s: The number of spot channels in the image.

	alpha: 0 for no alpha, 1 for alpha present.

	flags: flag bits.
		Bit 0: If set, draw the image with linear interpolation.
		Bit 1: If set, free the samples buffer when the pixmap
		is destroyed.

	stride: The byte offset from the data for any given pixel
	to the data for the same pixel on the row below.

	seps: NULL, or a pointer to a separations structure. If NULL,
	s should be 0.

	xres, yres: Image resolution in dpi. Default is 96 dpi.

	colorspace: Pointer to a colorspace object describing the
	colorspace the pixmap is in. If NULL, the image is a mask.

	samples: Pointer to the first byte of the pixmap sample data.
	This is typically a simple block of memory w * h * n bytes of
	memory in which the components are stored linearly, but with the
	use of appropriate stride values, scanlines can be stored in
	different orders, and have different amounts of padding. The
	first n bytes are components 0 to n-1 for the pixel at (x,y).
	Each successive n bytes gives another pixel in scanline order
	as we move across the line. The start of each scanline is offset
	the start of the previous one by stride bytes.
*/
struct fz_pixmap
{
	fz_storable storable;
	int x, y, w, h;
	unsigned char n;
	unsigned char s;
	unsigned char alpha;
	unsigned char flags;
	ptrdiff_t stride;
	fz_separations *seps;
	int xres, yres;
	fz_colorspace *colorspace;
	unsigned char *samples;
	fz_pixmap *underlying;
};

enum
{
	FZ_PIXMAP_FLAG_INTERPOLATE = 1,
	FZ_PIXMAP_FLAG_FREE_SAMPLES = 2
};

/* Create a new pixmap from a warped section of another.
 *
 * Colorspace, resolution etc are inherited from the original.
 * points give the corner points within the original pixmap of a
 * (convex) quadrilateral. These corner points will be 'warped' to be
 * the corner points of the returned bitmap, which will have the given
 * width/height.
 */
fz_pixmap *
fz_warp_pixmap(fz_context *ctx, fz_pixmap *src, fz_quad points, int width, int height);

/* As for fz_warp_pixmap, where width/height are automatically 'guessed'. */
fz_pixmap *
fz_autowarp_pixmap(fz_context *ctx, fz_pixmap *src, fz_quad points);

/* Search for a "document" within a pixmap (greyscale or rgb, no alpha).
 *
 * points should point to an array of 4 fz_points.
 *
 * If the function return false, no document was found.
 * If true, points has been updated to include the corner positions of
 * the detected document within the src image.
 */
int
fz_detect_document(fz_context *ctx, fz_quad *points, fz_pixmap *src);

/*
	Convert between different separation results.
*/
fz_pixmap *fz_clone_pixmap_area_with_different_seps(fz_context *ctx, fz_pixmap *src, const fz_irect *bbox, fz_colorspace *dcs, fz_separations *seps, fz_color_params color_params, fz_default_colorspaces *default_cs);

/*
 * Extract alpha channel as a separate pixmap.
 * Returns NULL if there is no alpha channel in the source.
 */
fz_pixmap *fz_new_pixmap_from_alpha_channel(fz_context *ctx, fz_pixmap *src);

/*
 * Combine a pixmap without an alpha channel with a soft mask.
 */
fz_pixmap *fz_new_pixmap_from_color_and_mask(fz_context *ctx, fz_pixmap *color, fz_pixmap *mask);

/*
 * Scale the pixmap up or down in size to fit the rectangle. Will return `NULL`
 * if the scaling factors are out of range. This applies fancy filtering and
 * will anti-alias the edges for subpixel positioning if using non-integer
 * coordinates. If the clip rectangle is set, the returned pixmap may be subset
 * to fit the clip rectangle. Pass `NULL` to the clip if you want the whole
 * pixmap scaled.
 */
fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, const fz_irect *clip);

/*
 * Reduces size to:
 * tile->w => (tile->w + 2^factor-1) / 2^factor
 * tile->h => (tile->h + 2^factor-1) / 2^factor
 */
void fz_subsample_pixmap(fz_context *ctx, fz_pixmap *tile, int factor);

/*
 * Copies r (clipped to both src and dest) in src to dest.
 */
void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_irect r, const fz_default_colorspaces *default_cs);

#endif
