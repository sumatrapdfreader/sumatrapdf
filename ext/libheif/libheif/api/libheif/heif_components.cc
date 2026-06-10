/*
 * HEIF codec.
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#include "heif_components.h"
#include "api_structs.h"
#include "image/pixelimage.h"


// --- id-based component access on a decoded heif_image

uint32_t heif_image_get_number_of_used_components(const heif_image* image)
{
  if (!image || !image->image) {
    return 0;
  }
  return image->image->get_number_of_used_components();
}


void heif_image_get_used_component_ids(const heif_image* image, uint32_t* out_component_ids)
{
  if (!image || !image->image || !out_component_ids) {
    return;
  }

  auto indices = image->image->get_used_component_ids();
  for (size_t i = 0; i < indices.size(); i++) {
    out_component_ids[i] = indices[i];
  }
}


heif_channel heif_image_get_component_channel(const heif_image* image, uint32_t component_id)
{
  if (!image || !image->image) {
    return heif_channel_Y;
  }
  return image->image->get_component_channel(component_id);
}


uint32_t heif_image_get_component_width(const heif_image* image, uint32_t component_id)
{
  if (!image || !image->image) {
    return 0;
  }
  return image->image->get_component_width(component_id);
}


uint32_t heif_image_get_component_height(const heif_image* image, uint32_t component_id)
{
  if (!image || !image->image) {
    return 0;
  }
  return image->image->get_component_height(component_id);
}


int heif_image_get_component_bits_per_pixel(const heif_image* image, uint32_t component_id)
{
  if (!image || !image->image) {
    return 0;
  }
  return image->image->get_component_bits_per_pixel(component_id);
}


uint16_t heif_image_get_component_type(const heif_image* image, uint32_t component_id)
{
  if (!image || !image->image) {
    return 0;
  }
  return image->image->get_component_type(component_id);
}


heif_component_datatype heif_image_get_component_datatype(const heif_image* image, uint32_t component_id)
{
  if (!image || !image->image) {
    return heif_component_datatype_undefined;
  }
  return image->image->get_component_datatype(component_id);
}


// --- handle-side per-component getters
//
// These read from ImageItem::m_components (populated at parse time by
// ImageItem::populate_component_descriptions). For unci images, the ids are
// the same numerical values that heif_image_get_used_component_ids() will
// report after decoding.

uint32_t heif_image_handle_get_number_of_components(const heif_image_handle* handle)
{
  if (!handle) {
    return 0;
  }
  return static_cast<uint32_t>(handle->image->get_component_descriptions().size());
}


void heif_image_handle_get_used_component_ids(const heif_image_handle* handle, uint32_t* out_component_ids)
{
  if (!handle || !out_component_ids) {
    return;
  }
  const auto& comps = handle->image->get_component_descriptions();
  for (size_t i = 0; i < comps.size(); i++) {
    out_component_ids[i] = comps[i].component_id;
  }
}


uint16_t heif_image_handle_get_component_type(const heif_image_handle* handle, uint32_t component_id)
{
  if (!handle) {
    return 0;
  }
  if (auto* desc = handle->image->find_component_description(component_id)) {
    return desc->component_type;
  }
  return 0;
}


int heif_image_handle_get_component_bits_per_pixel(const heif_image_handle* handle, uint32_t component_id)
{
  if (!handle) {
    return -1;
  }
  if (auto* desc = handle->image->find_component_description(component_id)) {
    return static_cast<int>(desc->bit_depth);
  }
  return -1;
}


heif_component_datatype heif_image_handle_get_component_datatype(const heif_image_handle* handle, uint32_t component_id)
{
  if (!handle) {
    return heif_component_datatype_undefined;
  }
  if (auto* desc = handle->image->find_component_description(component_id)) {
    return desc->datatype;
  }
  return heif_component_datatype_undefined;
}


// --- adding components

heif_error heif_image_add_component(heif_image* image,
                                    int width, int height,
                                    uint16_t component_type,
                                    heif_component_datatype datatype,
                                    int bit_depth,
                                    uint32_t* out_component_id)
{
  if (!image || !image->image) {
    return heif_error_null_pointer_argument;
  }

  auto result = image->image->add_component(width, height, component_type, datatype, bit_depth, nullptr);
  if (!result) {
    return result.error_struct(image->image.get());
  }

  if (out_component_id) {
    *out_component_id = *result;
  }

  return heif_error_success;
}


// --- untyped uint8 accessors

const uint8_t* heif_image_get_component_readonly(const heif_image* image, uint32_t component_id, size_t* out_stride)
{
  if (!image || !image->image) {
    if (out_stride) *out_stride = 0;
    return nullptr;
  }
  return image->image->get_component(component_id, out_stride);
}


uint8_t* heif_image_get_component(heif_image* image, uint32_t component_id, size_t* out_stride)
{
  if (!image || !image->image) {
    if (out_stride) *out_stride = 0;
    return nullptr;
  }
  return image->image->get_component(component_id, out_stride);
}


// Typed accessors: convert the internal byte stride to element count for the
// public API. libheif's allocator pads rows to a 16-byte boundary (see
// pixelimage.cc), which is a multiple of sizeof(T) for every supported T
// (<= 16 bytes), so the division is always exact.
#define heif_image_get_component_X(name, type) \
const type* heif_image_get_component_ ## name ## _readonly(const struct heif_image* image, \
                                                            uint32_t component_id, \
                                                            size_t* out_row_elements) \
{                                                            \
  if (!image || !image->image) {                             \
    if (out_row_elements) *out_row_elements = 0;             \
    return nullptr;                                          \
  }                                                          \
  size_t byte_stride = 0;                                    \
  const type* p = image->image->get_component_memory<type>(component_id, &byte_stride); \
  if (out_row_elements) *out_row_elements = byte_stride / sizeof(type); \
  return p;                                                  \
}                                                            \
                                                             \
type* heif_image_get_component_ ## name (struct heif_image* image, \
                                         uint32_t component_id,  \
                                         size_t* out_row_elements) \
{                                                            \
  if (!image || !image->image) {                             \
    if (out_row_elements) *out_row_elements = 0;             \
    return nullptr;                                          \
  }                                                          \
  size_t byte_stride = 0;                                    \
  type* p = image->image->get_component_memory<type>(component_id, &byte_stride); \
  if (out_row_elements) *out_row_elements = byte_stride / sizeof(type); \
  return p;                                                  \
}

heif_image_get_component_X(uint16, uint16_t)
heif_image_get_component_X(uint32, uint32_t)
heif_image_get_component_X(uint64, uint64_t)
heif_image_get_component_X(int8, int8_t)
heif_image_get_component_X(int16, int16_t)
heif_image_get_component_X(int32, int32_t)
heif_image_get_component_X(int64, int64_t)
heif_image_get_component_X(float32, float)
heif_image_get_component_X(float64, double)
heif_image_get_component_X(complex32, heif_complex32)
heif_image_get_component_X(complex64, heif_complex64)


heif_error heif_image_set_gimi_component_content_id(heif_image* image,
                                                    uint32_t component_id,
                                                    const char* content_id)
{
  if (image == nullptr || content_id == nullptr) {
    return heif_error_null_pointer_argument;
  }

  auto* desc = image->image->find_component_description(component_id);
  if (!desc) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "No component with the requested component_id exists."};
  }
  desc->gimi_content_id = content_id;

  return heif_error_success;
}
