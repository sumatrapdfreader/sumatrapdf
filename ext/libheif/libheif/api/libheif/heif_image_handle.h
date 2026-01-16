/*
 * HEIF codec.
 * Copyright (c) 2017-2025 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBHEIF_HEIF_IMAGE_HANDLE_H
#define LIBHEIF_HEIF_IMAGE_HANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <libheif/heif_library.h>
#include <libheif/heif_image.h>


// ========================= heif_image_handle =========================

// An heif_image_handle is a handle to a logical image in the HEIF file.
// To get the actual pixel data, you have to decode the handle to an heif_image.
// An heif_image_handle also gives you access to the thumbnails and Exif data
// associated with an image.

// Once you obtained an heif_image_handle, you can already release the heif_context,
// since it is internally ref-counted.

// Release image handle.
LIBHEIF_API
void heif_image_handle_release(const heif_image_handle*);

// Check whether the given image_handle is the primary image of the file.
LIBHEIF_API
int heif_image_handle_is_primary_image(const heif_image_handle* handle);

LIBHEIF_API
heif_item_id heif_image_handle_get_item_id(const heif_image_handle* handle);

/** Get the image width.
 *
 * If 'handle' is invalid (NULL) or if the image size exceeds the range of `int`, 0 is returned.
 */
LIBHEIF_API
int heif_image_handle_get_width(const heif_image_handle* handle);

/** Get the image height.
 *
 * If 'handle' is invalid (NULL) or if the image size exceeds the range of `int`, 0 is returned.
 */
LIBHEIF_API
int heif_image_handle_get_height(const heif_image_handle* handle);

LIBHEIF_API
int heif_image_handle_has_alpha_channel(const heif_image_handle*);

LIBHEIF_API
int heif_image_handle_is_premultiplied_alpha(const heif_image_handle*);

// Returns -1 on error, e.g. if this information is not present in the image.
// Only defined for images coded in the YCbCr or monochrome colorspace.
LIBHEIF_API
int heif_image_handle_get_luma_bits_per_pixel(const heif_image_handle*);

// Returns -1 on error, e.g. if this information is not present in the image.
// Only defined for images coded in the YCbCr colorspace.
LIBHEIF_API
int heif_image_handle_get_chroma_bits_per_pixel(const heif_image_handle*);

// Return the colorspace that libheif proposes to use for decoding.
// Usually, these will be either YCbCr or Monochrome, but it may also propose RGB for images
// encoded with matrix_coefficients=0 or for images coded natively in RGB.
// It may also return *_undefined if the file misses relevant information to determine this without decoding.
// These are only proposed values that avoid colorspace conversions as much as possible.
// You can still request the output in your preferred colorspace, but this may involve an internal conversion.
LIBHEIF_API
heif_error heif_image_handle_get_preferred_decoding_colorspace(const heif_image_handle* image_handle,
                                                               enum heif_colorspace* out_colorspace,
                                                               enum heif_chroma* out_chroma);

// Get the image width from the 'ispe' box. This is the original image size without
// any transformations applied to it. Do not use this unless you know exactly what
// you are doing.
LIBHEIF_API
int heif_image_handle_get_ispe_width(const heif_image_handle* handle);

LIBHEIF_API
int heif_image_handle_get_ispe_height(const heif_image_handle* handle);

// Returns whether the image has 'pixel aspect ratio information' information. If 0 is returned, the output is filled with the 1:1 default.
LIBHEIF_API
int heif_image_handle_get_pixel_aspect_ratio(const heif_image_handle*, uint32_t* aspect_h, uint32_t* aspect_v);


// This gets the context associated with the image handle.
// Note that you have to release the returned context with heif_context_free() in any case.
//
// This means: when you have several image-handles that originate from the same file and you get the
// context of each of them, the returned pointer may be different even though it refers to the same
// logical context. You have to call heif_context_free() on all those context pointers.
// After you freed a context pointer, you can still use the context through a different pointer that you
// might have acquired from elsewhere.
LIBHEIF_API
heif_context* heif_image_handle_get_context(const heif_image_handle* handle);

LIBHEIF_API
const char* heif_image_handle_get_gimi_content_id(const heif_image_handle* handle);

#ifdef __cplusplus
}
#endif

#endif
