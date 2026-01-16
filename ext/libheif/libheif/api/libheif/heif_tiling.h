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

#ifndef LIBHEIF_HEIF_TILING_H
#define LIBHEIF_HEIF_TILING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_library.h>
#include <libheif/heif_error.h>
#include <libheif/heif_image.h>

// forward declaration from other headers
typedef struct heif_encoder heif_encoder;
typedef struct heif_encoding_options heif_encoding_options;


typedef struct heif_image_tiling
{
  int version;

  // --- version 1

  uint32_t num_columns;
  uint32_t num_rows;
  uint32_t tile_width;
  uint32_t tile_height;

  uint32_t image_width;
  uint32_t image_height;

  // Position of the top left tile.
  // Usually, this is (0;0), but if a tiled image is rotated or cropped, it may be that the top left tile should be placed at a negative position.
  // The offsets define this negative shift.
  uint32_t top_offset;
  uint32_t left_offset;

  uint8_t number_of_extra_dimensions;  // 0 for normal images, 1 for volumetric (3D), ...
  uint32_t extra_dimension_size[8];    // size of extra dimensions (first 8 dimensions)
} heif_image_tiling;


// --- decoding ---

// If 'process_image_transformations' is true, this returns modified sizes.
// If it is false, the top_offset and left_offset will always be (0;0).
LIBHEIF_API
heif_error heif_image_handle_get_image_tiling(const heif_image_handle* handle, int process_image_transformations, struct heif_image_tiling* out_tiling);


// For grid images, return the image item ID of a specific grid tile.
// If 'process_image_transformations' is true, the tile positions are given in the transformed image coordinate system and
// are internally mapped to the original image tile positions.
LIBHEIF_API
heif_error heif_image_handle_get_grid_image_tile_id(const heif_image_handle* handle,
                                                    int process_image_transformations,
                                                    uint32_t tile_x, uint32_t tile_y,
                                                    heif_item_id* out_tile_item_id);


typedef struct heif_decoding_options heif_decoding_options;

// The tile position is given in tile indices, not in pixel coordinates.
// If the image transformations are processed (option->ignore_image_transformations==false), the tile position
// is given in the transformed coordinates.
LIBHEIF_API
heif_error heif_image_handle_decode_image_tile(const heif_image_handle* in_handle,
                                               heif_image** out_img,
                                               enum heif_colorspace colorspace,
                                               enum heif_chroma chroma,
                                               const heif_decoding_options* options,
                                               uint32_t tile_x, uint32_t tile_y);


// --- encoding ---

/**
 * @brief Encodes an array of images into a grid.
 *
 * @param ctx The file context
 * @param tiles User allocated array of images that will form the grid.
 * @param rows The number of rows in the grid.
 * @param columns The number of columns in the grid.
 * @param encoder Defines the encoder to use. See heif_context_get_encoder_for_format()
 * @param input_options Optional, may be nullptr.
 * @param out_image_handle Returns a handle to the grid. The caller is responsible for freeing it.
 * @return Returns an error if ctx, tiles, or encoder is nullptr. If rows or columns is 0.
 */
LIBHEIF_API
heif_error heif_context_encode_grid(heif_context* ctx,
                                    heif_image** tiles,
                                    uint16_t rows,
                                    uint16_t columns,
                                    heif_encoder* encoder,
                                    const heif_encoding_options* input_options,
                                    heif_image_handle** out_image_handle);

LIBHEIF_API
heif_error heif_context_add_grid_image(heif_context* ctx,
                                       uint32_t image_width,
                                       uint32_t image_height,
                                       uint32_t tile_columns,
                                       uint32_t tile_rows,
                                       const heif_encoding_options* encoding_options,
                                       heif_image_handle** out_grid_image_handle);

LIBHEIF_API
heif_error heif_context_add_image_tile(heif_context* ctx,
                                       heif_image_handle* tiled_image,
                                       uint32_t tile_x, uint32_t tile_y,
                                       const heif_image* image,
                                       heif_encoder* encoder);

#ifdef __cplusplus
}
#endif

#endif
