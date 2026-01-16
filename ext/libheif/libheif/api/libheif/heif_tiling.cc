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

#include "heif_tiling.h"
#include "api_structs.h"
#include "image-items/grid.h"
#include "image-items/tiled.h"

#if WITH_UNCOMPRESSED_CODEC
#include "image-items/unc_image.h"
#endif

#include <memory>
#include <utility>
#include <vector>
#include <array>


heif_error heif_image_handle_get_image_tiling(const heif_image_handle* handle, int process_image_transformations, struct heif_image_tiling* tiling)
{
  if (!handle || !tiling) {
    return heif_error_null_pointer_argument;
  }

  *tiling = handle->image->get_heif_image_tiling();

  if (process_image_transformations) {
    Error error = handle->image->process_image_transformations_on_tiling(*tiling);
    if (error) {
      return error.error_struct(handle->context.get());
    }
  }

  return heif_error_ok;
}


heif_error heif_image_handle_get_grid_image_tile_id(const heif_image_handle* handle,
                                                    int process_image_transformations,
                                                    uint32_t tile_x, uint32_t tile_y,
                                                    heif_item_id* tile_item_id)
{
  if (!handle || !tile_item_id) {
    return heif_error_null_pointer_argument;
  }

  std::shared_ptr<ImageItem_Grid> gridItem = std::dynamic_pointer_cast<ImageItem_Grid>(handle->image);
  if (!gridItem) {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "Image is no grid image"
    };
  }

  const ImageGrid& gridspec = gridItem->get_grid_spec();
  if (tile_x >= gridspec.get_columns() || tile_y >= gridspec.get_rows()) {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "Grid tile index out of range"
    };
  }

  if (process_image_transformations) {
    gridItem->transform_requested_tile_position_to_original_tile_position(tile_x, tile_y);
  }

  *tile_item_id = gridItem->get_grid_tiles()[tile_y * gridspec.get_columns() + tile_x];

  return heif_error_ok;
}


heif_error heif_image_handle_decode_image_tile(const heif_image_handle* in_handle,
                                               heif_image** out_img,
                                               heif_colorspace colorspace,
                                               heif_chroma chroma,
                                               const heif_decoding_options* input_options,
                                               uint32_t x0, uint32_t y0)
{
  if (!in_handle) {
    return heif_error_null_pointer_argument;
  }

  heif_item_id id = in_handle->image->get_id();

  heif_decoding_options* dec_options = heif_decoding_options_alloc();
  heif_decoding_options_copy(dec_options, input_options);

  Result<std::shared_ptr<HeifPixelImage> > decodingResult = in_handle->context->decode_image(id,
                                                                                             colorspace,
                                                                                             chroma,
                                                                                             *dec_options,
                                                                                             true, x0, y0,
                                                                                             {});
  heif_decoding_options_free(dec_options);

  if (!decodingResult) {
    return decodingResult.error_struct(in_handle->image.get());
  }

  std::shared_ptr<HeifPixelImage> img = *decodingResult;

  *out_img = new heif_image();
  (*out_img)->image = std::move(img);

  return Error::Ok.error_struct(in_handle->image.get());
}


// --- encoding ---

heif_error heif_context_encode_grid(heif_context* ctx,
                                    heif_image** tiles,
                                    uint16_t columns,
                                    uint16_t rows,
                                    heif_encoder* encoder,
                                    const heif_encoding_options* input_options,
                                    heif_image_handle** out_image_handle)
{
  if (!encoder || !tiles) {
    return heif_error_null_pointer_argument;
  }
  else if (rows == 0 || columns == 0) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value).error_struct(ctx->context.get());
  }

  // TODO: Don't repeat this code from heif_context_encode_image()
  heif_encoding_options* options = heif_encoding_options_alloc();

  heif_color_profile_nclx nclx;
  if (input_options) {
    heif_encoding_options_copy(options, input_options);

    if (options->output_nclx_profile == nullptr) {
      if (tiles[0]->image->has_nclx_color_profile()) {
        nclx_profile input_nclx = tiles[0]->image->get_color_profile_nclx();

        options->output_nclx_profile = &nclx;
        nclx.version = 1;
        nclx.color_primaries = input_nclx.get_colour_primaries();
        nclx.transfer_characteristics = input_nclx.get_transfer_characteristics();
        nclx.matrix_coefficients = input_nclx.get_matrix_coefficients();
        nclx.full_range_flag = input_nclx.get_full_range_flag();
      }
    }
  }

  // Convert heif_images to a vector of HeifPixelImages
  std::vector<std::shared_ptr<HeifPixelImage> > pixel_tiles;
  for (int i = 0; i < rows * columns; i++) {
    pixel_tiles.push_back(tiles[i]->image);
  }

  // Encode Grid
  std::shared_ptr<ImageItem> out_grid;
  auto addGridResult = ImageItem_Grid::add_and_encode_full_grid(ctx->context.get(),
                                                                pixel_tiles,
                                                                rows, columns,
                                                                encoder,
                                                                *options);
  heif_encoding_options_free(options);

  if (!addGridResult) {
    return addGridResult.error_struct(ctx->context.get());
  }

  out_grid = *addGridResult;

  // Mark as primary image
  if (ctx->context->is_primary_image_set() == false) {
    ctx->context->set_primary_image(out_grid);
  }

  if (out_image_handle) {
    *out_image_handle = new heif_image_handle;
    (*out_image_handle)->image = std::move(out_grid);
    (*out_image_handle)->context = ctx->context;
  }

  return heif_error_success;
}


heif_error heif_context_add_grid_image(heif_context* ctx,
                                       uint32_t image_width,
                                       uint32_t image_height,
                                       uint32_t tile_columns,
                                       uint32_t tile_rows,
                                       const heif_encoding_options* encoding_options,
                                       heif_image_handle** out_grid_image_handle)
{
  if (!ctx) {
    return heif_error_null_pointer_argument;
  }

  if (tile_rows == 0 || tile_columns == 0) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value).error_struct(ctx->context.get());
  }
  else if (tile_rows > 0xFFFF || tile_columns > 0xFFFF) {
    return heif_error{
      heif_error_Usage_error,
      heif_suberror_Invalid_image_size,
      "Number of tile rows/columns may not exceed 65535"
    };
  }

  auto generateGridItemResult = ImageItem_Grid::add_new_grid_item(ctx->context.get(),
                                                                  image_width,
                                                                  image_height,
                                                                  static_cast<uint16_t>(tile_rows),
                                                                  static_cast<uint16_t>(tile_columns),
                                                                  encoding_options);
  if (!generateGridItemResult) {
    return generateGridItemResult.error_struct(ctx->context.get());
  }

  if (out_grid_image_handle) {
    *out_grid_image_handle = new heif_image_handle;
    (*out_grid_image_handle)->image = *generateGridItemResult;
    (*out_grid_image_handle)->context = ctx->context;
  }

  return heif_error_success;
}


heif_error heif_context_add_image_tile(heif_context* ctx,
                                       heif_image_handle* tiled_image,
                                       uint32_t tile_x, uint32_t tile_y,
                                       const heif_image* image,
                                       heif_encoder* encoder)
{
  if (!ctx || !tiled_image || !image || !encoder) {
    return heif_error_null_pointer_argument;
  }

  if (auto tili_image = std::dynamic_pointer_cast<ImageItem_Tiled>(tiled_image->image)) {
    Error err = tili_image->add_image_tile(tile_x, tile_y, image->image, encoder);
    return err.error_struct(ctx->context.get());
  }
#if WITH_UNCOMPRESSED_CODEC
  else if (auto unci = std::dynamic_pointer_cast<ImageItem_uncompressed>(tiled_image->image)) {
    Error err = unci->add_image_tile(tile_x, tile_y, image->image, true); // TODO: how do we handle 'save_alpha=false' ?
    return err.error_struct(ctx->context.get());
  }
#endif
  else if (auto grid_item = std::dynamic_pointer_cast<ImageItem_Grid>(tiled_image->image)) {
    Error err = grid_item->add_image_tile(tile_x, tile_y, image->image, encoder);
    return err.error_struct(ctx->context.get());
  }
  else {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "Cannot add tile to a non-tiled image"
    };
  }
}
