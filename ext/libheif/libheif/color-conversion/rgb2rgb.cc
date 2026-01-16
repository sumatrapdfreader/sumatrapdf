/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>
#include "rgb2rgb.h"


std::vector<ColorStateWithCost>
Op_RGB_to_RGB24_32::state_after_conversion(const ColorState& input_state,
                                           const ColorState& target_state,
                                           const heif_color_conversion_options& options,
                                           const heif_color_conversion_options_ext& options_ext) const
{
  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444 ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  //ColorConversionCosts costs;

  // --- convert to RGBA (with alpha)

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGBA;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = 8;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  // --- convert to RGB (without alpha)

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGB;
  output_state.has_alpha = false;
  output_state.bits_per_pixel = 8;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RGB_to_RGB24_32::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                       const ColorState& input_state,
                                       const ColorState& target_state,
                                       const heif_color_conversion_options& options,
                                       const heif_color_conversion_options_ext& options_ext,
                                       const heif_security_limits* limits) const
{
  bool has_alpha = input->has_channel(heif_channel_Alpha);
  bool want_alpha = target_state.has_alpha;

  if (input->get_bits_per_pixel(heif_channel_R) != 8 ||
      input->get_bits_per_pixel(heif_channel_G) != 8 ||
      input->get_bits_per_pixel(heif_channel_B) != 8) {
    return Error::InternalError;
  }

  if (has_alpha && input->get_bits_per_pixel(heif_channel_Alpha) != 8) {
    return Error::InternalError;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB,
                 want_alpha ? heif_chroma_interleaved_32bit : heif_chroma_interleaved_24bit);

  if (auto err = outimg->add_plane(heif_channel_interleaved, width, height, 8, limits)) {
    return err;
  }

  const uint8_t* in_r, * in_g, * in_b, * in_a = nullptr;
  size_t in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  size_t out_p_stride = 0;

  in_r = input->get_plane(heif_channel_R, &in_r_stride);
  in_g = input->get_plane(heif_channel_G, &in_g_stride);
  in_b = input->get_plane(heif_channel_B, &in_b_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  if (has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  uint32_t x, y;
  for (y = 0; y < height; y++) {

    if (has_alpha && want_alpha) {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 4 * x + 0] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 4 * x + 1] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 4 * x + 2] = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + 4 * x + 3] = in_a[x + y * in_a_stride];
      }
    }
    else if (!want_alpha) {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 3 * x + 0] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 3 * x + 1] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 3 * x + 2] = in_b[x + y * in_b_stride];
      }
    }
    else {
      assert(want_alpha && !has_alpha);

      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 4 * x + 0] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 4 * x + 1] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 4 * x + 2] = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + 4 * x + 3] = 0xFF;
      }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RGB_HDR_to_RRGGBBaa_BE::state_after_conversion(const ColorState& input_state,
                                                  const ColorState& target_state,
                                                  const heif_color_conversion_options& options,
                                                  const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444 ||
      input_state.bits_per_pixel <= 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to RRGGBB_BE

  if (input_state.has_alpha == false) {
    output_state.colorspace = heif_colorspace_RGB;
    output_state.chroma = heif_chroma_interleaved_RRGGBB_BE;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }


  // --- convert to RRGGBBAA_BE

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RRGGBBAA_BE;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);


  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RGB_HDR_to_RRGGBBaa_BE::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                              const ColorState& input_state,
                                              const ColorState& target_state,
                                              const heif_color_conversion_options& options,
                                              const heif_color_conversion_options_ext& options_ext,
                                              const heif_security_limits* limits) const
{
  if (input->get_bits_per_pixel(heif_channel_R) <= 8 ||
      input->get_bits_per_pixel(heif_channel_G) <= 8 ||
      input->get_bits_per_pixel(heif_channel_B) <= 8) {
    return Error::InternalError;
  }

  bool input_has_alpha = input->has_channel(heif_channel_Alpha);
  bool output_has_alpha = input_has_alpha || target_state.has_alpha;

  if (input_has_alpha) {
    if (input->get_bits_per_pixel(heif_channel_Alpha) <= 8) {
      return Error::InternalError;
    }

    if (input->get_width(heif_channel_Alpha) != input->get_width(heif_channel_G) ||
        input->get_height(heif_channel_Alpha) != input->get_height(heif_channel_G)) {
      return Error::InternalError;
    }
  }
  int bpp = input->get_bits_per_pixel(heif_channel_R);
  if (bpp <= 0) return Error::InternalError;

  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB,
                 output_has_alpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE);
  if (auto err = outimg->add_plane(heif_channel_interleaved, width, height, bpp, limits)) {
    return err;
  }

  const uint16_t* in_r, * in_g, * in_b, * in_a = nullptr;
  size_t in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  size_t out_p_stride = 0;

  in_r = (uint16_t*) input->get_plane(heif_channel_R, &in_r_stride);
  in_g = (uint16_t*) input->get_plane(heif_channel_G, &in_g_stride);
  in_b = (uint16_t*) input->get_plane(heif_channel_B, &in_b_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  if (input_has_alpha) {
    in_a = (uint16_t*) input->get_plane(heif_channel_Alpha, &in_a_stride);
    assert(in_a != nullptr);

    // should never happen, but makes clang-tidy happy
    if (in_a == nullptr) {
      return Error::InternalError;
    }
  }

  in_r_stride /= 2;
  in_g_stride /= 2;
  in_b_stride /= 2;
  in_a_stride /= 2;

  const int pixelsize = (output_has_alpha ? 8 : 6);

  uint32_t x, y;
  auto alpha_max = static_cast<uint16_t>((1 << bpp) - 1);
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      uint16_t r = in_r[x + y * in_r_stride];
      uint16_t g = in_g[x + y * in_g_stride];
      uint16_t b = in_b[x + y * in_b_stride];
      out_p[y * out_p_stride + pixelsize * x + 0] = (uint8_t)(r >> 8);
      out_p[y * out_p_stride + pixelsize * x + 1] = (uint8_t)(r & 0xFF);
      out_p[y * out_p_stride + pixelsize * x + 2] = (uint8_t)(g >> 8);
      out_p[y * out_p_stride + pixelsize * x + 3] = (uint8_t)(g & 0xFF);
      out_p[y * out_p_stride + pixelsize * x + 4] = (uint8_t)(b >> 8);
      out_p[y * out_p_stride + pixelsize * x + 5] = (uint8_t)(b & 0xFF);
      if (output_has_alpha) {
        uint16_t a = input_has_alpha ? in_a[x + y * in_a_stride] : alpha_max;
        out_p[y * out_p_stride + pixelsize * x + 6] = (uint8_t)(a >> 8);
        out_p[y * out_p_stride + pixelsize * x + 7] = (uint8_t)(a & 0xFF);
      }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RGB_to_RRGGBBaa_BE::state_after_conversion(const ColorState& input_state,
                                              const ColorState& target_state,
                                              const heif_color_conversion_options& options,
                                              const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444 ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to RRGGBB_BE

  if (input_state.has_alpha == false) {
    output_state.colorspace = heif_colorspace_RGB;
    output_state.chroma = heif_chroma_interleaved_RRGGBB_BE;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }


  // --- convert to RRGGBBAA_BE

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RRGGBBAA_BE;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RGB_to_RRGGBBaa_BE::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                          const ColorState& input_state,
                                          const ColorState& target_state,
                                          const heif_color_conversion_options& options,
                                          const heif_color_conversion_options_ext& options_ext,
                                          const heif_security_limits* limits) const
{
  if (input->get_bits_per_pixel(heif_channel_R) != 8 ||
      input->get_bits_per_pixel(heif_channel_G) != 8 ||
      input->get_bits_per_pixel(heif_channel_B) != 8) {
    return Error::InternalError;
  }

  //int bpp = input->get_bits_per_pixel(heif_channel_R);

  bool input_has_alpha = input->has_channel(heif_channel_Alpha);
  bool output_has_alpha = input_has_alpha || target_state.has_alpha;

  if (input_has_alpha && input->get_bits_per_pixel(heif_channel_Alpha) != 8) {
    return Error::InternalError;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB,
                 output_has_alpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE);

  if (auto err = outimg->add_plane(heif_channel_interleaved, width, height, input->get_bits_per_pixel(heif_channel_R), limits)) {
    return err;
  }

  const uint8_t* in_r, * in_g, * in_b, * in_a = nullptr;
  size_t in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  size_t out_p_stride = 0;

  in_r = input->get_plane(heif_channel_R, &in_r_stride);
  in_g = input->get_plane(heif_channel_G, &in_g_stride);
  in_b = input->get_plane(heif_channel_B, &in_b_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  if (input_has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  const int pixelsize = (output_has_alpha ? 8 : 6);

  uint32_t x, y;
  for (y = 0; y < height; y++) {

    if (input_has_alpha) {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 8 * x + 0] = 0;
        out_p[y * out_p_stride + 8 * x + 1] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 8 * x + 2] = 0;
        out_p[y * out_p_stride + 8 * x + 3] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 8 * x + 4] = 0;
        out_p[y * out_p_stride + 8 * x + 5] = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + 8 * x + 6] = 0;
        out_p[y * out_p_stride + 8 * x + 7] = in_a[x + y * in_a_stride];
      }
    }
    else {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + pixelsize * x + 0] = 0;
        out_p[y * out_p_stride + pixelsize * x + 1] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + pixelsize * x + 2] = 0;
        out_p[y * out_p_stride + pixelsize * x + 3] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + pixelsize * x + 4] = 0;
        out_p[y * out_p_stride + pixelsize * x + 5] = in_b[x + y * in_b_stride];

        if (output_has_alpha) {
          out_p[y * out_p_stride + pixelsize * x + 6] = 0;
          out_p[y * out_p_stride + pixelsize * x + 7] = 0xFF;
        }
      }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RRGGBBaa_BE_to_RGB_HDR::state_after_conversion(const ColorState& input_state,
                                                  const ColorState& target_state,
                                                  const heif_color_conversion_options& options,
                                                  const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RRGGBB_BE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBBAA_BE) ||
      input_state.bits_per_pixel <= 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to RRGGBB_BE

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_444;
  output_state.has_alpha = target_state.has_alpha;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RRGGBBaa_BE_to_RGB_HDR::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                              const ColorState& input_state,
                                              const ColorState& target_state,
                                              const heif_color_conversion_options& options,
                                              const heif_color_conversion_options_ext& options_ext,
                                              const heif_security_limits* limits) const
{
  bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE ||
                    input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE);
  bool want_alpha = target_state.has_alpha;

  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();
  int bpp = input->get_bits_per_pixel(heif_channel_interleaved);

  outimg->create(width, height, heif_colorspace_RGB, heif_chroma_444);

  if (auto err = outimg->add_plane(heif_channel_R, width, height, bpp, limits) ||
                 outimg->add_plane(heif_channel_G, width, height, bpp, limits) ||
                 outimg->add_plane(heif_channel_B, width, height, bpp, limits)) {
    return err;
  }

  if (want_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, bpp, limits)) {
      return err;
    }
  }

  const uint8_t* in_p;
  size_t in_p_stride = 0;
  int in_pix_size = has_alpha ? 8 : 6;

  uint16_t* out_r, * out_g, * out_b, * out_a = nullptr;
  size_t out_r_stride = 0, out_g_stride = 0, out_b_stride = 0, out_a_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);

  out_r = (uint16_t*) outimg->get_plane(heif_channel_R, &out_r_stride);
  out_g = (uint16_t*) outimg->get_plane(heif_channel_G, &out_g_stride);
  out_b = (uint16_t*) outimg->get_plane(heif_channel_B, &out_b_stride);

  if (want_alpha) {
    out_a = (uint16_t*) outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }

  out_r_stride /= 2;
  out_g_stride /= 2;
  out_b_stride /= 2;
  out_a_stride /= 2;

  auto alpha_max = static_cast<uint16_t>((1 << bpp) - 1);
  uint32_t x, y;
  for (y = 0; y < height; y++) {

    for (x = 0; x < width; x++) {
      uint16_t r = (uint16_t) ((in_p[y * in_p_stride + in_pix_size * x + 0] << 8) |
                               in_p[y * in_p_stride + in_pix_size * x + 1]);
      uint16_t g = (uint16_t) ((in_p[y * in_p_stride + in_pix_size * x + 2] << 8) |
                               in_p[y * in_p_stride + in_pix_size * x + 3]);
      uint16_t b = (uint16_t) ((in_p[y * in_p_stride + in_pix_size * x + 4] << 8) |
                               in_p[y * in_p_stride + in_pix_size * x + 5]);

      out_r[x + y * out_r_stride] = r;
      out_g[x + y * out_g_stride] = g;
      out_b[x + y * out_b_stride] = b;

      if (want_alpha) {
        // in_pix_size is always 8 when we have alpha channel
        uint16_t a = has_alpha ? (uint16_t) ((in_p[y * in_p_stride + 8 * x + 6] << 8) |
                                 in_p[y * in_p_stride + 8 * x + 7]) : alpha_max;

        out_a[x + y * out_a_stride] = a;
      }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RGB24_32_to_RGB::state_after_conversion(const ColorState& input_state,
                                           const ColorState& target_state,
                                           const heif_color_conversion_options& options,
                                           const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RGB &&
       input_state.chroma != heif_chroma_interleaved_RGBA) ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to planar RGB

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_444;
  output_state.has_alpha = target_state.has_alpha;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RGB24_32_to_RGB::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                       const ColorState& input_state,
                                       const ColorState& target_state,
                                       const heif_color_conversion_options& options,
                                       const heif_color_conversion_options_ext& options_ext,
                                       const heif_security_limits* limits) const
{
  bool has_alpha = input->get_chroma_format() == heif_chroma_interleaved_RGBA;
  bool want_alpha = target_state.has_alpha;

  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB, heif_chroma_444);

  if (auto err = outimg->add_plane(heif_channel_R, width, height, 8, limits) ||
                 outimg->add_plane(heif_channel_G, width, height, 8, limits) ||
                 outimg->add_plane(heif_channel_B, width, height, 8, limits)) {
    return err;
  }

  if (want_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, 8, limits)) {
      return err;
    }
  }

  const uint8_t* in_p;
  size_t in_p_stride = 0;
  int in_pix_size = has_alpha ? 4 : 3;

  uint8_t* out_r, * out_g, * out_b, * out_a = nullptr;
  size_t out_r_stride = 0, out_g_stride = 0, out_b_stride = 0, out_a_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);

  out_r = outimg->get_plane(heif_channel_R, &out_r_stride);
  out_g = outimg->get_plane(heif_channel_G, &out_g_stride);
  out_b = outimg->get_plane(heif_channel_B, &out_b_stride);

  if (want_alpha) {
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }

  uint32_t x, y;
  for (y = 0; y < height; y++) {

    for (x = 0; x < width; x++) {
      out_r[x + y * out_r_stride] = in_p[y * in_p_stride + in_pix_size * x + 0];
      out_g[x + y * out_g_stride] = in_p[y * in_p_stride + in_pix_size * x + 1];
      out_b[x + y * out_b_stride] = in_p[y * in_p_stride + in_pix_size * x + 2];

      if (want_alpha) {
        uint8_t a = has_alpha ? in_p[y * in_p_stride + in_pix_size * x + 3] : 0xff;
        out_a[x + y * out_a_stride] = a;
      }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RRGGBBaa_swap_endianness::state_after_conversion(const ColorState& input_state,
                                                    const ColorState& target_state,
                                                    const heif_color_conversion_options& options,
                                                    const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RRGGBB_LE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBB_BE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBBAA_LE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBBAA_BE)) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- swap RRGGBB

  if (input_state.chroma == heif_chroma_interleaved_RRGGBB_LE ||
      input_state.chroma == heif_chroma_interleaved_RRGGBB_BE) {
    output_state.colorspace = heif_colorspace_RGB;

    if (input_state.chroma == heif_chroma_interleaved_RRGGBB_LE) {
      output_state.chroma = heif_chroma_interleaved_RRGGBB_BE;
    }
    else {
      output_state.chroma = heif_chroma_interleaved_RRGGBB_LE;
    }

    output_state.has_alpha = false;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }


  // --- swap RRGGBBAA

  if (input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE ||
      input_state.chroma == heif_chroma_interleaved_RRGGBBAA_BE) {
    output_state.colorspace = heif_colorspace_RGB;

    if (input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE) {
      output_state.chroma = heif_chroma_interleaved_RRGGBBAA_BE;
    }
    else {
      output_state.chroma = heif_chroma_interleaved_RRGGBBAA_LE;
    }

    output_state.has_alpha = true;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RRGGBBaa_swap_endianness::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                const ColorState& input_state,
                                                const ColorState& target_state,
                                                const heif_color_conversion_options& options,
                                                const heif_color_conversion_options_ext& options_ext,
                                                const heif_security_limits* limits) const
{
  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  switch (input->get_chroma_format()) {
    case heif_chroma_interleaved_RRGGBB_LE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_BE);
      break;
    case heif_chroma_interleaved_RRGGBB_BE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE);
      break;
    case heif_chroma_interleaved_RRGGBBAA_LE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_BE);
      break;
    case heif_chroma_interleaved_RRGGBBAA_BE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_LE);
      break;
    default:
      return Error::InternalError;
  }

  if (auto err = outimg->add_plane(heif_channel_interleaved, width, height,
                                   input->get_bits_per_pixel(heif_channel_interleaved), limits)) {
    return err;
  }

  const uint8_t* in_p = nullptr;
  size_t in_p_stride = 0;

  uint8_t* out_p;
  size_t out_p_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  size_t n_bytes = std::min(in_p_stride, out_p_stride);

  uint32_t x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < n_bytes; x += 2) {
      out_p[y * out_p_stride + x + 0] = in_p[y * in_p_stride + x + 1];
      out_p[y * out_p_stride + x + 1] = in_p[y * in_p_stride + x + 0];
    }
  }

  return outimg;
}
