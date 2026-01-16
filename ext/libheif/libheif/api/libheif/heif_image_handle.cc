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

#include "heif_image_handle.h"
#include "api_structs.h"
#include <climits>
#include <string>


void heif_image_handle_release(const heif_image_handle* handle)
{
  delete handle;
}


int heif_image_handle_is_primary_image(const heif_image_handle* handle)
{
  return handle->image->is_primary();
}


heif_item_id heif_image_handle_get_item_id(const heif_image_handle* handle)
{
  return handle->image->get_id();
}


int heif_image_handle_get_width(const heif_image_handle* handle)
{
  if (handle && handle->image) {
    uint32_t w = handle->image->get_width();
    if (w > INT_MAX) {
      return 0;
    }
    else {
      return static_cast<int>(w);
    }
  }
  else {
    return 0;
  }
}


int heif_image_handle_get_height(const heif_image_handle* handle)
{
  if (handle && handle->image) {
    uint32_t h = handle->image->get_height();
    if (h > INT_MAX) {
      return 0;
    }
    else {
      return static_cast<int>(h);
    }
  }
  else {
    return 0;
  }
}


int heif_image_handle_has_alpha_channel(const heif_image_handle* handle)
{
  // TODO: for now, also scan the grid tiles for alpha information (issue #708), but depending about
  // how the discussion about this structure goes forward, we might remove this again.

  return handle->context->has_alpha(handle->image->get_id()); // handle case in issue #708
  //return handle->image->get_alpha_channel() != nullptr;       // old alpha check that fails on alpha in grid tiles
}


int heif_image_handle_is_premultiplied_alpha(const heif_image_handle* handle)
{
  // TODO: what about images that have the alpha in the grid tiles (issue #708) ?
  return handle->image->is_premultiplied_alpha();
}


int heif_image_handle_get_luma_bits_per_pixel(const heif_image_handle* handle)
{
  return handle->image->get_luma_bits_per_pixel();
}


int heif_image_handle_get_chroma_bits_per_pixel(const heif_image_handle* handle)
{
  return handle->image->get_chroma_bits_per_pixel();
}


heif_error heif_image_handle_get_preferred_decoding_colorspace(const heif_image_handle* image_handle,
                                                               heif_colorspace* out_colorspace,
                                                               heif_chroma* out_chroma)
{
  Error err = image_handle->image->get_coded_image_colorspace(out_colorspace, out_chroma);
  if (err) {
    return err.error_struct(image_handle->image.get());
  }

  return heif_error_success;
}


int heif_image_handle_get_ispe_width(const heif_image_handle* handle)
{
  if (handle && handle->image) {
    return handle->image->get_ispe_width();
  }
  else {
    return 0;
  }
}


int heif_image_handle_get_ispe_height(const heif_image_handle* handle)
{
  if (handle && handle->image) {
    return handle->image->get_ispe_height();
  }
  else {
    return 0;
  }
}


int heif_image_handle_get_pixel_aspect_ratio(const heif_image_handle* handle, uint32_t* aspect_h, uint32_t* aspect_v)
{
  auto pasp = handle->image->get_property<Box_pasp>();
  if (pasp) {
    *aspect_h = pasp->hSpacing;
    *aspect_v = pasp->vSpacing;
    return 1;
  }
  else {
    *aspect_h = 1;
    *aspect_v = 1;
    return 0;
  }
}


heif_context* heif_image_handle_get_context(const heif_image_handle* handle)
{
  auto ctx = new heif_context();
  ctx->context = handle->context;
  return ctx;
}


const char* heif_image_handle_get_gimi_content_id(const heif_image_handle* handle)
{
  if (!handle->image->has_gimi_sample_content_id()) {
    return nullptr;
  }

  std::string id = handle->image->get_gimi_sample_content_id();
  char* idstring = new char[id.size() + 1];
  strcpy(idstring, id.c_str());
  return idstring;
}
