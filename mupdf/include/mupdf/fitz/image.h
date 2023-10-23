// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_IMAGE_H
#define MUPDF_FITZ_IMAGE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/pixmap.h"

#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/compressed-buffer.h"

/**
	Images are storable objects from which we can obtain fz_pixmaps.
	These may be implemented as simple wrappers around a pixmap, or
	as more complex things that decode at different subsample
	settings on demand.
*/
typedef struct fz_image fz_image;
typedef struct fz_compressed_image fz_compressed_image;
typedef struct fz_pixmap_image fz_pixmap_image;

/**
	Called to get a handle to a pixmap from an image.

	image: The image to retrieve a pixmap from.

	subarea: The subarea of the image that we actually care about
	(or NULL to indicate the whole image).

	ctm: Optional, unless subarea is given. If given, then on
	entry this is the transform that will be applied to the complete
	image. It should be updated on exit to the transform to apply to
	the given subarea of the image. This is used to calculate the
	desired width/height for subsampling.

	w: If non-NULL, a pointer to an int to be updated on exit to the
	width (in pixels) that the scaled output will cover.

	h: If non-NULL, a pointer to an int to be updated on exit to the
	height (in pixels) that the scaled output will cover.

	Returns a non NULL kept pixmap pointer. May throw exceptions.
*/
fz_pixmap *fz_get_pixmap_from_image(fz_context *ctx, fz_image *image, const fz_irect *subarea, fz_matrix *ctm, int *w, int *h);

/**
	Calls fz_get_pixmap_from_image() with ctm, subarea, w and h all set to NULL.
*/
fz_pixmap *fz_get_unscaled_pixmap_from_image(fz_context *ctx, fz_image *image);

/**
	Increment the (normal) reference count for an image. Returns the
	same pointer.

	Never throws exceptions.
*/
fz_image *fz_keep_image(fz_context *ctx, fz_image *image);

/**
	Decrement the (normal) reference count for an image. When the
	total (normal + key) reference count reaches zero, the image and
	its resources are freed.

	Never throws exceptions.
*/
void fz_drop_image(fz_context *ctx, fz_image *image);

/**
	Increment the store key reference for an image. Returns the same
	pointer. (This is the count of references for an image held by
	keys in the image store).

	Never throws exceptions.
*/
fz_image *fz_keep_image_store_key(fz_context *ctx, fz_image *image);

/**
	Decrement the store key reference count for an image. When the
	total (normal + key) reference count reaches zero, the image and
	its resources are freed.

	Never throws exceptions.
*/
void fz_drop_image_store_key(fz_context *ctx, fz_image *image);

/**
	Function type to destroy an images data
	when it's reference count reaches zero.
*/
typedef void (fz_drop_image_fn)(fz_context *ctx, fz_image *image);

/**
	Function type to get a decoded pixmap for an image.

	im: The image to decode.

	subarea: NULL, or the subarea of the image required. Expressed
	in terms of a rectangle in the original width/height of the
	image. If non NULL, this should be updated by the function to
	the actual subarea decoded - which must include the requested
	area!

	w, h: The actual width and height that the whole image would
	need to be decoded to.

	l2factor: On entry, the log 2 subsample factor required. If
	possible the decode process can take care of (all or some) of
	this subsampling, and must then update the value so the caller
	knows what remains to be done.

	Returns a reference to a decoded pixmap that satisfies the
	requirements of the request. The caller owns the returned
	reference.
*/
typedef fz_pixmap *(fz_image_get_pixmap_fn)(fz_context *ctx, fz_image *im, fz_irect *subarea, int w, int h, int *l2factor);

/**
	Function type to get the given storage
	size for an image.

	Returns the size in bytes used for a given image.
*/
typedef size_t (fz_image_get_size_fn)(fz_context *, fz_image *);

/**
	Internal function to make a new fz_image structure
	for a derived class.

	w,h: Width and height of the created image.

	bpc: Bits per component.

	colorspace: The colorspace (determines the number of components,
	and any color conversions required while decoding).

	xres, yres: The X and Y resolutions respectively.

	interpolate: 1 if interpolation should be used when decoding
	this image, 0 otherwise.

	imagemask: 1 if this is an imagemask (i.e. transparent), 0
	otherwise.

	decode: NULL, or a pointer to to a decode array. The default
	decode array is [0 1] (repeated n times, for n color components).

	colorkey: NULL, or a pointer to a colorkey array. The default
	colorkey array is [0 255] (repeated n times, for n color
	components).

	mask: NULL, or another image to use as a mask for this one.
	A new reference is taken to this image. Supplying a masked
	image as a mask to another image is illegal!

	size: The size of the required allocated structure (the size of
	the derived structure).

	get: The function to be called to obtain a decoded pixmap.

	get_size: The function to be called to return the storage size
	used by this image.

	drop: The function to be called to dispose of this image once
	the last reference is dropped.

	Returns a pointer to an allocated structure of the required size,
	with the first sizeof(fz_image) bytes initialised as appropriate
	given the supplied parameters, and the other bytes set to zero.
*/
fz_image *fz_new_image_of_size(fz_context *ctx,
		int w,
		int h,
		int bpc,
		fz_colorspace *colorspace,
		int xres,
		int yres,
		int interpolate,
		int imagemask,
		float *decode,
		int *colorkey,
		fz_image *mask,
		size_t size,
		fz_image_get_pixmap_fn *get_pixmap,
		fz_image_get_size_fn *get_size,
		fz_drop_image_fn *drop);

#define fz_new_derived_image(CTX,W,H,B,CS,X,Y,I,IM,D,C,M,T,G,S,Z) \
	((T*)Memento_label(fz_new_image_of_size(CTX,W,H,B,CS,X,Y,I,IM,D,C,M,sizeof(T),G,S,Z),#T))

/**
	Create an image based on
	the data in the supplied compressed buffer.

	w,h: Width and height of the created image.

	bpc: Bits per component.

	colorspace: The colorspace (determines the number of components,
	and any color conversions required while decoding).

	xres, yres: The X and Y resolutions respectively.

	interpolate: 1 if interpolation should be used when decoding
	this image, 0 otherwise.

	imagemask: 1 if this is an imagemask (i.e. transparency bitmap
	mask), 0 otherwise.

	decode: NULL, or a pointer to to a decode array. The default
	decode array is [0 1] (repeated n times, for n color components).

	colorkey: NULL, or a pointer to a colorkey array. The default
	colorkey array is [0 255] (repeated n times, for n color
	components).

	buffer: Buffer of compressed data and compression parameters.
	Ownership of this reference is passed in.

	mask: NULL, or another image to use as a mask for this one.
	A new reference is taken to this image. Supplying a masked
	image as a mask to another image is illegal!
*/
fz_image *fz_new_image_from_compressed_buffer(fz_context *ctx, int w, int h, int bpc, fz_colorspace *colorspace, int xres, int yres, int interpolate, int imagemask, float *decode, int *colorkey, fz_compressed_buffer *buffer, fz_image *mask);

/**
	Create an image from the given
	pixmap.

	pixmap: The pixmap to base the image upon. A new reference
	to this is taken.

	mask: NULL, or another image to use as a mask for this one.
	A new reference is taken to this image. Supplying a masked
	image as a mask to another image is illegal!
*/
fz_image *fz_new_image_from_pixmap(fz_context *ctx, fz_pixmap *pixmap, fz_image *mask);

/**
	Create a new image from a
	buffer of data, inferring its type from the format
	of the data.
*/
fz_image *fz_new_image_from_buffer(fz_context *ctx, fz_buffer *buffer);

/**
	Create a new image from the contents
	of a file, inferring its type from the format of the
	data.
*/
fz_image *fz_new_image_from_file(fz_context *ctx, const char *path);

/**
	Internal destructor exposed for fz_store integration.
*/
void fz_drop_image_imp(fz_context *ctx, fz_storable *image);

/**
	Internal destructor for the base image class members.

	Exposed to allow derived image classes to be written.
*/
void fz_drop_image_base(fz_context *ctx, fz_image *image);

/**
	Decode a subarea of a compressed image. l2factor is the amount
	of subsampling inbuilt to the stream (i.e. performed by the
	decoder). If non NULL, l2extra is the extra amount of
	subsampling that should be performed by this routine. This will
	be updated on exit to the amount of subsampling that is still
	required to be done.

	Returns a kept reference.
*/
fz_pixmap *fz_decomp_image_from_stream(fz_context *ctx, fz_stream *stm, fz_compressed_image *image, fz_irect *subarea, int indexed, int l2factor, int *l2extra);

/**
	Convert pixmap from indexed to base colorspace.

	This creates a new bitmap containing the converted pixmap data.
 */
fz_pixmap *fz_convert_indexed_pixmap_to_base(fz_context *ctx, const fz_pixmap *src);

/**
	Convert pixmap from DeviceN/Separation to base colorspace.

	This creates a new bitmap containing the converted pixmap data.
*/
fz_pixmap *fz_convert_separation_pixmap_to_base(fz_context *ctx, const fz_pixmap *src);

/**
	Return the size of the storage used by an image.
*/
size_t fz_image_size(fz_context *ctx, fz_image *im);

/**
	Return the type of a compressed image.

	Any non-compressed image will have the type returned as UNKNOWN.
*/
int fz_compressed_image_type(fz_context *ctx, fz_image *image);


/**
	Structure is public to allow other structures to
	be derived from it. Do not access members directly.
*/
struct fz_image
{
	fz_key_storable key_storable;
	int w, h;
	uint8_t n;
	uint8_t bpc;
	unsigned int imagemask:1;
	unsigned int interpolate:1;
	unsigned int use_colorkey:1;
	unsigned int use_decode:1;
	unsigned int decoded:1;
	unsigned int scalable:1;
	uint8_t orientation;
	fz_image *mask;
	int xres; /* As given in the image, not necessarily as rendered */
	int yres; /* As given in the image, not necessarily as rendered */
	fz_colorspace *colorspace;
	fz_drop_image_fn *drop_image;
	fz_image_get_pixmap_fn *get_pixmap;
	fz_image_get_size_fn *get_size;
	int colorkey[FZ_MAX_COLORS * 2];
	float decode[FZ_MAX_COLORS * 2];
};

/**
	Request the natural resolution
	of an image.

	xres, yres: Pointers to ints to be updated with the
	natural resolution of an image (or a sensible default
	if not encoded).
*/
void fz_image_resolution(fz_image *image, int *xres, int *yres);

/**
	Request the natural orientation of an image.

	This is for images (such as JPEG) that can contain internal
	specifications of rotation/flips. This is ignored by all the
	internal decode/rendering routines, but can be used by callers
	(such as the image document handler) to respect such
	specifications.

	The values used by MuPDF are as follows, with the equivalent
	Exif specifications given for information:

	0: Undefined
	1: 0 degree ccw rotation. (Exif = 1)
	2: 90 degree ccw rotation. (Exif = 8)
	3: 180 degree ccw rotation. (Exif = 3)
	4: 270 degree ccw rotation. (Exif = 6)
	5: flip on X. (Exif = 2)
	6: flip on X, then rotate ccw by 90 degrees. (Exif = 5)
	7: flip on X, then rotate ccw by 180 degrees. (Exif = 4)
	8: flip on X, then rotate ccw by 270 degrees. (Exif = 7)
*/
uint8_t fz_image_orientation(fz_context *ctx, fz_image *image);

fz_matrix
fz_image_orientation_matrix(fz_context *ctx, fz_image *image);

/**
	Retrieve the underlying compressed data for an image.

	Returns a pointer to the underlying data buffer for an image,
	or NULL if this image is not based upon a compressed data
	buffer.

	This is not a reference counted structure, so no reference is
	returned. Lifespan is limited to that of the image itself.
*/
fz_compressed_buffer *fz_compressed_image_buffer(fz_context *ctx, fz_image *image);
void fz_set_compressed_image_buffer(fz_context *ctx, fz_compressed_image *cimg, fz_compressed_buffer *buf);

/**
	Retrieve the underlying fz_pixmap for an image.

	Returns a pointer to the underlying fz_pixmap for an image,
	or NULL if this image is not based upon an fz_pixmap.

	No reference is returned. Lifespan is limited to that of
	the image itself. If required, use fz_keep_pixmap to take
	a reference to keep it longer.
*/
fz_pixmap *fz_pixmap_image_tile(fz_context *ctx, fz_pixmap_image *cimg);
void fz_set_pixmap_image_tile(fz_context *ctx, fz_pixmap_image *cimg, fz_pixmap *pix);

/* Implementation details: subject to change. */

/**
	Exposed for PDF.
*/
fz_pixmap *fz_load_jpx(fz_context *ctx, const unsigned char *data, size_t size, fz_colorspace *cs);

/**
	Exposed because compression and decompression need to share this.
*/
void opj_lock(fz_context *ctx);
void opj_unlock(fz_context *ctx);


/**
	Exposed for CBZ.
*/
int fz_load_tiff_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_tiff_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);
int fz_load_pnm_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_pnm_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);
int fz_load_jbig2_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_jbig2_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);
int fz_load_bmp_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len);
fz_pixmap *fz_load_bmp_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage);

#endif
