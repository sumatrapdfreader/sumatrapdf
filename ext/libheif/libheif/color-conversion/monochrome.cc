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

#include <cstring>
#include "monochrome.h"


std::vector<ColorStateWithCost>
Op_mono_to_YCbCr420::state_after_conversion(const ColorState& input_state,
                                            const ColorState& target_state,
                                            const heif_color_conversion_options& options,
                                            const heif_color_conversion_options_ext& options_ext) const
{
  if (input_state.colorspace != heif_colorspace_monochrome ||
      input_state.chroma != heif_chroma_monochrome) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to YCbCr420

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = heif_chroma_420;
  output_state.has_alpha = input_state.has_alpha;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_OptimizedSoftware);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_mono_to_YCbCr420::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                        const ColorState& input_state,
                                        const ColorState& target_state,
                                        const heif_color_conversion_options& options,
                                        const heif_color_conversion_options_ext& options_ext,
                                        const heif_security_limits* limits) const
{
  auto outimg = std::make_shared<HeifPixelImage>();

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  outimg->create(width, height, heif_colorspace_YCbCr, heif_chroma_420);

  int input_bpp = input->get_bits_per_pixel(heif_channel_Y);

  uint32_t chroma_width = (width + 1) / 2;
  uint32_t chroma_height = (height + 1) / 2;

  if (auto err = outimg->add_plane(heif_channel_Y, width, height, input_bpp, limits) ||
                 outimg->add_plane(heif_channel_Cb, chroma_width, chroma_height, input_bpp, limits) ||
                 outimg->add_plane(heif_channel_Cr, chroma_width, chroma_height, input_bpp, limits)) {
    return err;
  }

  int alpha_bpp = 0;
  bool has_alpha = input->has_channel(heif_channel_Alpha);
  if (has_alpha) {
    alpha_bpp = input->get_bits_per_pixel(heif_channel_Alpha);
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, alpha_bpp, limits)) {
      return err;
    }
  }


  if (input_bpp <= 8) {
    uint8_t* out_cb, * out_cr, * out_y;
    size_t out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0;

    const uint8_t* in_y;
    size_t in_y_stride = 0;

    in_y = input->get_plane(heif_channel_Y, &in_y_stride);

    out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
    out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
    out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

    auto chroma_value = static_cast<uint8_t>(1 << (input_bpp - 1));

    memset(out_cb, chroma_value, out_cb_stride * chroma_height);
    memset(out_cr, chroma_value, out_cr_stride * chroma_height);

    for (uint32_t y = 0; y < height; y++) {
      memcpy(out_y + y * out_y_stride,
             in_y + y * in_y_stride,
             width);
    }
  }
  else {
    uint16_t* out_cb, * out_cr, * out_y;
    size_t out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0;

    const uint16_t* in_y;
    size_t in_y_stride = 0;

    in_y = (const uint16_t*) input->get_plane(heif_channel_Y, &in_y_stride);

    out_y = (uint16_t*) outimg->get_plane(heif_channel_Y, &out_y_stride);
    out_cb = (uint16_t*) outimg->get_plane(heif_channel_Cb, &out_cb_stride);
    out_cr = (uint16_t*) outimg->get_plane(heif_channel_Cr, &out_cr_stride);

    in_y_stride /= 2;
    out_y_stride /= 2;
    out_cb_stride /= 2;
    out_cr_stride /= 2;

    for (uint32_t y = 0; y < chroma_height; y++)
      for (uint32_t x = 0; x < chroma_width; x++) {
        out_cb[x + y * out_cb_stride] = (uint16_t) (128 << (input_bpp - 8));
        out_cr[x + y * out_cr_stride] = (uint16_t) (128 << (input_bpp - 8));
      }

    for (uint32_t y = 0; y < height; y++) {
      memcpy(out_y + y * out_y_stride,
             in_y + y * in_y_stride,
             width * 2);
    }
  }

  if (has_alpha) {
    const uint8_t* in_a;
    uint8_t* out_a;
    size_t in_a_stride = 0;
    size_t out_a_stride = 0;

    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);

    uint32_t memory_width = (alpha_bpp > 8 ? width * 2 : width);

    for (uint32_t y = 0; y < height; y++) {
      memcpy(&out_a[y * out_a_stride], &in_a[y * in_a_stride], memory_width);
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_mono_to_RGB24_32::state_after_conversion(const ColorState& input_state,
                                            const ColorState& target_state,
                                            const heif_color_conversion_options& options,
                                            const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_monochrome ||
      input_state.chroma != heif_chroma_monochrome ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to RGB24

  if (input_state.has_alpha == false) {
    output_state.colorspace = heif_colorspace_RGB;
    output_state.chroma = heif_chroma_interleaved_RGB;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = 8;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }


  // --- convert to RGB32

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGBA;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = 8;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_mono_to_RGB24_32::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                        const ColorState& input_state,
                                        const ColorState& target_state,
                                        const heif_color_conversion_options& options,
                                        const heif_color_conversion_options_ext& options_ext,
                                        const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  if (input->get_bits_per_pixel(heif_channel_Y) != 8) {
    return Error::InternalError;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  bool has_alpha = input->has_channel(heif_channel_Alpha);

  if (target_state.has_alpha) {
    outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_32bit);
  }
  else {
    outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_24bit);
  }

  if (auto err = outimg->add_plane(heif_channel_interleaved, width, height, 8, limits)) {
    return err;
  }

  const uint8_t* in_y, * in_a = nullptr;
  size_t in_y_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  size_t out_p_stride = 0;

  in_y = input->get_plane(heif_channel_Y, &in_y_stride);
  if (has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  uint32_t x, y;
  for (y = 0; y < height; y++) {
    if (target_state.has_alpha == false) {
      for (x = 0; x < width; x++) {
        uint8_t v = in_y[x + y * in_y_stride];
        out_p[y * out_p_stride + 3 * x + 0] = v;
        out_p[y * out_p_stride + 3 * x + 1] = v;
        out_p[y * out_p_stride + 3 * x + 2] = v;
      }
    }
    else if (has_alpha) {
      for (x = 0; x < width; x++) {
        uint8_t v = in_y[x + y * in_y_stride];
        out_p[y * out_p_stride + 4 * x + 0] = v;
        out_p[y * out_p_stride + 4 * x + 1] = v;
        out_p[y * out_p_stride + 4 * x + 2] = v;
        out_p[y * out_p_stride + 4 * x + 3] = in_a[x + y * in_a_stride];
      }
    }
    else {
      for (x = 0; x < width; x++) {
        uint8_t v = in_y[x + y * in_y_stride];
        out_p[y * out_p_stride + 4 * x + 0] = v;
        out_p[y * out_p_stride + 4 * x + 1] = v;
        out_p[y * out_p_stride + 4 * x + 2] = v;
        out_p[y * out_p_stride + 4 * x + 3] = 0xFF;
      }
    }
  }

  return outimg;
}


