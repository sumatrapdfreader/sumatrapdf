/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#include "heif_image.h"
#include "heif.h"
#include "image/pixelimage.h"
#include "api_structs.h"
#include "error.h"
#include <limits>

#include <algorithm>
#include <memory>
#include <utility>
#include <cstring>
#include <array>


heif_colorspace heif_image_get_colorspace(const heif_image* img)
{
  return img->image->get_colorspace();
}

heif_chroma heif_image_get_chroma_format(const heif_image* img)
{
  return img->image->get_chroma_format();
}


static int uint32_to_int(uint32_t v)
{
  if (v == 0 || v > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
    return -1;
  }
  else {
    return static_cast<int>(v);
  }
}


int heif_image_get_width(const heif_image* img, heif_channel channel)
{
  return uint32_to_int(img->image->get_width(channel));
}


int heif_image_get_height(const heif_image* img, heif_channel channel)
{
  return uint32_to_int(img->image->get_height(channel));
}


int heif_image_get_primary_width(const heif_image* img)
{
  return uint32_to_int(img->image->get_width());
}


int heif_image_get_primary_height(const heif_image* img)
{
  return uint32_to_int(img->image->get_height());
}


heif_error heif_image_crop(heif_image* img,
                           int left, int right, int top, int bottom)
{
  uint32_t w = img->image->get_width();
  uint32_t h = img->image->get_height();

  // Margins must be non-negative and must not consume the entire image.
  // Without this check, `left + right >= w` would underflow the unsigned
  // `w - 1 - right` computation below and cause an OOB read (issue #1746).
  if (left < 0 || right < 0 || top < 0 || bottom < 0 ||
      static_cast<int64_t>(left) + right >= w ||
      static_cast<int64_t>(top) + bottom >= h) {
    return heif_error{
      heif_error_Usage_error,
      heif_suberror_Invalid_parameter_value,
      "Invalid crop margins"
    };
  }

  auto cropResult = img->image->crop(left, w - 1 - right, top, h - 1 - bottom, nullptr);
  if (!cropResult) {
    return cropResult.error_struct(img->image.get());
  }

  img->image = *cropResult;

  return heif_error_success;
}


heif_error heif_image_extract_area(const heif_image* srcimg,
                                   uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                                   const heif_security_limits* limits,
                                   heif_image** out_image)
{
  auto extractResult = srcimg->image->extract_image_area(x0, y0, w, h, limits);
  if (!extractResult) {
    return extractResult.error_struct(srcimg->image.get());
  }

  heif_image* area = new heif_image;
  area->image = *extractResult;

  *out_image = area;

  return heif_error_success;
}


int heif_image_get_bits_per_pixel(const heif_image* img, heif_channel channel)
{
  // get_storage_bits_per_pixel() returns 0 for a non-existing channel;
  // the public API documents -1 for that case.
  uint16_t bpp = img->image->get_storage_bits_per_pixel(channel);
  return bpp == 0 ? -1 : bpp;
}


int heif_image_get_bits_per_pixel_range(const heif_image* img, heif_channel channel)
{
  // get_bits_per_pixel() returns 0 for a non-existing channel;
  // keep the public API's -1 result for that case.
  uint16_t bpp = img->image->get_bits_per_pixel(channel);
  return bpp == 0 ? -1 : bpp;
}


int heif_image_has_channel(const heif_image* img, heif_channel channel)
{
  return img->image->has_channel(channel);
}


const uint8_t* heif_image_get_plane_readonly(const heif_image* image,
                                             heif_channel channel,
                                             int* out_stride)
{
  if (!out_stride) {
    return nullptr;
  }

  if (!image || !image->image) {
    *out_stride = 0;
    return nullptr;
  }

  size_t stride;
  const auto* p = image->image->get_channel_memory(channel, &stride);

  // TODO: use C++20 std::cmp_greater()
  if (stride > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
    return nullptr;
  }

  *out_stride = static_cast<int>(stride);
  return p;
}


uint8_t* heif_image_get_plane(heif_image* image,
                              heif_channel channel,
                              int* out_stride)
{
  if (!out_stride) {
    return nullptr;
  }

  if (!image || !image->image) {
    *out_stride = 0;
    return nullptr;
  }

  size_t stride;
  uint8_t* p = image->image->get_channel_memory(channel, &stride);

  // TODO: use C++20 std::cmp_greater()
  if (stride > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
    return nullptr;
  }

  *out_stride = static_cast<int>(stride);
  return p;
}


const uint8_t* heif_image_get_plane_readonly2(const heif_image* image,
                                              heif_channel channel,
                                              size_t* out_stride)
{
  if (!out_stride) {
    return nullptr;
  }

  if (!image || !image->image) {
    *out_stride = 0;
    return nullptr;
  }

  return image->image->get_channel_memory(channel, out_stride);
}


uint8_t* heif_image_get_plane2(heif_image* image,
                               heif_channel channel,
                               size_t* out_stride)
{
  if (!out_stride) {
    return nullptr;
  }

  if (!image || !image->image) {
    *out_stride = 0;
    return nullptr;
  }

  return image->image->get_channel_memory(channel, out_stride);
}


heif_error heif_image_scale_image(const heif_image* input,
                                  heif_image** output,
                                  int width, int height,
                                  const heif_scaling_options* options)
{
  std::shared_ptr<HeifPixelImage> out_img;

  Error err = input->image->scale_nearest_neighbor(out_img, width, height, nullptr);
  if (err) {
    return err.error_struct(input->image.get());
  }

  *output = new heif_image;
  (*output)->image = std::move(out_img);

  return Error::Ok.error_struct(input->image.get());
}


heif_error heif_image_extend_to_size_fill_with_zero(heif_image* image,
                                                    uint32_t width, uint32_t height)
{
  Error err = image->image->extend_to_size_with_zero(width, height, nullptr);
  if (err) {
    return err.error_struct(image->image.get());
  }

  return heif_error_ok;
}


int heif_image_get_decoding_warnings(heif_image* image,
                                     int first_warning_idx,
                                     heif_error* out_warnings,
                                     int max_output_buffer_entries)
{
  if (max_output_buffer_entries == 0) {
    return (int) image->image->get_warnings().size();
  }
  else {
    const auto& warnings = image->image->get_warnings();
    int n;
    for (n = 0; n + first_warning_idx < (int) warnings.size() && n < max_output_buffer_entries; n++) {
      out_warnings[n] = warnings[n + first_warning_idx].error_struct(image->image.get());
    }
    return n;
  }
}

void heif_image_add_decoding_warning(heif_image* image,
                                     heif_error err)
{
  image->image->add_warning(Error(err.code, err.subcode));
}


void heif_image_release(const heif_image* img)
{
  delete img;
}


void heif_image_get_pixel_aspect_ratio(const heif_image* image, uint32_t* aspect_h, uint32_t* aspect_v)
{
  image->image->get_pixel_ratio(aspect_h, aspect_v);
}


void heif_image_set_pixel_aspect_ratio(heif_image* image, uint32_t aspect_h, uint32_t aspect_v)
{
  image->image->set_pixel_ratio(aspect_h, aspect_v);
}

void heif_image_handle_set_pixel_aspect_ratio(heif_image_handle* handle, uint32_t aspect_h, uint32_t aspect_v)
{
  handle->image->set_pixel_ratio(aspect_h, aspect_v);
}

heif_error heif_image_create(int width, int height,
                             heif_colorspace colorspace,
                             heif_chroma chroma,
                             heif_image** image)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  // auto-correct colorspace_YCbCr + chroma_planar (formerly chroma_monochrome)
  // to colorspace_monochrome
  // TODO: this should return an error in a later version (see below)
  if (chroma == heif_chroma_planar && colorspace == heif_colorspace_YCbCr) {
    colorspace = heif_colorspace_monochrome;

    std::cerr << "libheif warning: heif_image_create() used with an illegal colorspace/chroma combination. This will return an error in a future version.\n";
  }

  // return error if invalid colorspace + chroma combination is used.
  // (RGB + planar canonicalization happens later in HeifPixelImage::create.)
  auto validChroma = get_valid_chroma_values_for_colorspace(colorspace);
  if (std::find(validChroma.begin(), validChroma.end(), chroma) == validChroma.end()) {
    *image = nullptr;
    return {heif_error_Usage_error, heif_suberror_Invalid_parameter_value, "Invalid colorspace/chroma combination."};
  }

  heif_image* img = new heif_image;
  img->image = std::make_shared<HeifPixelImage>();

  img->image->create(width, height, colorspace, chroma);

  *image = img;

  return heif_error_success;
}

heif_error heif_image_add_plane(heif_image* image,
                                heif_channel channel, int width, int height, int bit_depth)
{
  // Note: no security limit, because this is explicitly requested by the user.
  if (auto err = image->image->add_channel(channel, width, height, bit_depth, nullptr)) {
    return err.error_struct(image->image.get());
  }
  else {
    return heif_error_success;
  }
}


heif_error heif_image_add_plane_safe(heif_image* image,
                                     heif_channel channel, int width, int height, int bit_depth,
                                     const heif_security_limits* limits)
{
  if (auto err = image->image->add_channel(channel, width, height, bit_depth, limits)) {
    return err.error_struct(image->image.get());
  }
  else {
    return heif_error_success;
  }
}


void heif_image_set_premultiplied_alpha(heif_image* image,
                                        int is_premultiplied_alpha)
{
  if (image == nullptr) {
    return;
  }

  image->image->set_premultiplied_alpha(is_premultiplied_alpha);
}


int heif_image_is_premultiplied_alpha(heif_image* image)
{
  if (image == nullptr) {
    return 0;
  }

  return image->image->is_premultiplied_alpha();
}


heif_error heif_image_extend_padding_to_size(heif_image* image, int min_physical_width, int min_physical_height)
{
  Error err = image->image->extend_padding_to_size(min_physical_width, min_physical_height, false, nullptr);
  if (err) {
    return err.error_struct(image->image.get());
  }
  else {
    return heif_error_success;
  }
}
