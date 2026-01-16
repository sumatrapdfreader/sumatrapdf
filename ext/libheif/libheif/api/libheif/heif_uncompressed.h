/*
 * HEIF codec.
 * Copyright (c) 2024-2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_UNCOMPRESSED_H
#define LIBHEIF_HEIF_UNCOMPRESSED_H

#include "libheif/heif.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @file heif_uncompressed.h
 * @brief Functions for adding ISO 23001-17 (uncompressed) images to a HEIF file.
 *        Despite its name, this is not limited to uncompressed images.
 *        It is also possible to add images with lossless compression methods.
 *        See heif_metadata_compression for more information.
 */

// --- 'unci' images

// This is similar to heif_metadata_compression. We should try to keep the integers compatible, but each enum will just
// contain the allowed values.
enum heif_unci_compression
{
  heif_unci_compression_off = 0,
  //heif_unci_compression_auto = 1,
  //heif_unci_compression_unknown = 2, // only used when reading unknown method from input file
  heif_unci_compression_deflate = 3,
  heif_unci_compression_zlib = 4,
  heif_unci_compression_brotli = 5
};


typedef struct heif_unci_image_parameters
{
  int version;

  // --- version 1

  uint32_t image_width;
  uint32_t image_height;

  uint32_t tile_width;
  uint32_t tile_height;

  enum heif_unci_compression compression;

  // TODO: interleave type, padding
} heif_unci_image_parameters;

LIBHEIF_API
heif_unci_image_parameters* heif_unci_image_parameters_alloc(void);

LIBHEIF_API
void heif_unci_image_parameters_copy(heif_unci_image_parameters* dst,
                                     const heif_unci_image_parameters* src);

LIBHEIF_API
void heif_unci_image_parameters_release(heif_unci_image_parameters*);


/*
 * This adds an empty iso23001-17 (uncompressed) image to the HEIF file.
 * The actual image data is added later using heif_context_add_image_tile().
 * If you do not need tiling, you can use heif_context_encode_image() instead.
 * However, this will by default disable any compression and any control about
 * the data layout.
 *
 * @param ctx The file context
 * @param parameters The parameters for the image, must not be NULL.
 * @param encoding_options Optional, may be NULL.
 * @param prototype An image with the same channel configuration as the image data
 *                  that will be later inserted. The image size need not match this.
 *                  Must not be NULL.
 * @param out_unci_image_handle Returns a handle to the image. The caller is responsible for freeing it.
 *                  Must not be NULL because this is required to fill in image data.
 * @return Returns an error if the passed parameters are incorrect.
 *         If ISO23001-17 images are not supported, returns heif_error_Unsupported_feature.
 */
LIBHEIF_API
heif_error heif_context_add_empty_unci_image(heif_context* ctx,
                                             const heif_unci_image_parameters* parameters,
                                             const heif_encoding_options* encoding_options,
                                             const heif_image* prototype,
                                             heif_image_handle** out_unci_image_handle);

#ifdef __cplusplus
}
#endif

#endif
