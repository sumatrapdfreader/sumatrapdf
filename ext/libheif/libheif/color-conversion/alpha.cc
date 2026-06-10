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

#include <cstdint>
#include "alpha.h"


std::vector<ColorStateWithCost>
Op_drop_alpha_plane::state_after_conversion(const ColorState& input_state,
                                            const ColorState& target_state,
                                            const heif_color_conversion_options& options,
                                            const heif_color_conversion_options_ext& options_ext) const
{
  // only drop alpha plane if it is not needed in output

  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.has_alpha == false ||
      target_state.has_alpha == true) {
    return {};
  }

  if (options_ext.alpha_composition_mode != heif_alpha_composition_mode_none) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- drop alpha plane

  output_state = input_state;
  output_state.has_alpha = false;

  states.emplace_back(output_state, SpeedCosts_Trivial);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_drop_alpha_plane::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                        const ColorState& input_state,
                                        const ColorState& target_state,
                                        const heif_color_conversion_options& options,
                                        const heif_color_conversion_options_ext& options_ext,
                                        const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height,
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_Y,
                               heif_channel_Cb,
                               heif_channel_Cr,
                               heif_channel_R,
                               heif_channel_G,
                               heif_channel_B}) {
    if (input->has_channel(channel)) {
      outimg->copy_new_channel_from(input, channel, channel, limits);
    }
  }

  return outimg;
}


template<class Pixel>
std::vector<ColorStateWithCost>
Op_flatten_alpha_plane<Pixel>::state_after_conversion(const ColorState& input_state,
                                                      const ColorState& target_state,
                                                      const heif_color_conversion_options& options,
                                                      const heif_color_conversion_options_ext& options_ext) const
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  // TODO: this Op only works when all channels are either HDR or all are SDR.
  //       But there is currently no easy way to check that.

  if ((input_state.bits_per_pixel > 8) != hdr) {
    return {};
  }

  if (input_state.has_alpha && input_state.get_alpha_bits_per_pixel() != input_state.bits_per_pixel) {
    return {};
  }

  // only drop alpha plane if it is not needed in output

  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.has_alpha == false ||
      target_state.has_alpha == true) {
    return {};
  }

  if (options_ext.alpha_composition_mode == heif_alpha_composition_mode_none) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- drop alpha plane

  output_state = input_state;
  output_state.has_alpha = false;

  states.emplace_back(output_state, SpeedCosts_Trivial);

  return states;
}


template<class Pixel>
Result<std::shared_ptr<HeifPixelImage>>
Op_flatten_alpha_plane<Pixel>::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input_raw,
                                                  const ColorState& input_state,
                                                  const ColorState& target_state,
                                                  const heif_color_conversion_options& options,
                                                  const heif_color_conversion_options_ext& options_ext,
                                                  const heif_security_limits* limits) const
{
  std::shared_ptr<const HeifPixelImage> input = input_raw;

  heif_color_conversion_options_ext options_ext_skip_alpha = options_ext;
  options_ext_skip_alpha.alpha_composition_mode = heif_alpha_composition_mode_none;

  if (options_ext.alpha_composition_mode != heif_alpha_composition_mode_none) {
    Result<std::shared_ptr<const HeifPixelImage>> convInput = ::convert_colorspace(input,
                                                                                   heif_colorspace_RGB,
                                                                                   heif_chroma_444,
                                                                                   input_state.nclx,
                                                                                   input_state.bits_per_pixel,
                                                                                   options, &options_ext_skip_alpha,
                                                                                   limits);
    if (!convInput) {
      return convInput.error();
    }
    else {
      input = *convInput;
    }
  }

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height,
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_R,
                               heif_channel_G,
                               heif_channel_B}) {
    outimg->add_channel(channel, width, height, target_state.bits_per_pixel, limits);

    const Pixel* p_alpha;
    size_t stride_alpha;
    p_alpha = (const Pixel*)input->get_channel_memory(heif_channel_Alpha, &stride_alpha);
    int bpp_alpha = input->get_bits_per_pixel(heif_channel_Alpha);
    Pixel alpha_max = (Pixel)((1 << bpp_alpha) - 1);

    const Pixel* p_in;
    size_t stride_in;
    p_in = (const Pixel*)input->get_channel_memory(channel, &stride_in);

    Pixel* p_out;
    size_t stride_out;
    p_out = (Pixel*)outimg->get_channel_memory(channel, &stride_out);

    if (sizeof(Pixel) == 2) {
      stride_alpha /= 2;
      stride_in /= 2;
      stride_out /= 2;
    }

    if (options_ext.alpha_composition_mode == heif_alpha_composition_mode_solid_color ||
        (options_ext.alpha_composition_mode == heif_alpha_composition_mode_checkerboard && options_ext.checkerboard_square_size == 0)) {
      uint16_t bkg16;

      switch (channel) {
        case heif_channel_R:
          bkg16 = options_ext.background_red;
          break;
        case heif_channel_G:
          bkg16 = options_ext.background_green;
          break;
        case heif_channel_B:
          bkg16 = options_ext.background_blue;
          break;
        default:
          assert(false);
          bkg16 = 0;
      }

      Pixel bkg = static_cast<Pixel>(bkg16 >> (16 - input->get_bits_per_pixel(channel)));

      for (uint32_t y = 0; y < height; y++)
        for (uint32_t x = 0; x < width; x++) {
          int a = p_alpha[y * stride_alpha + x];
          p_out[y * stride_out + x] = static_cast<Pixel>((p_in[y * stride_in + x] * a + bkg * (alpha_max - a)) >> bpp_alpha);
        }
    }
    else {
      uint16_t bkg16_1, bkg16_2;

      switch (channel) {
        case heif_channel_R:
          bkg16_1 = options_ext.background_red;
          bkg16_2 = options_ext.secondary_background_red;
          break;
        case heif_channel_G:
          bkg16_1 = options_ext.background_green;
          bkg16_2 = options_ext.secondary_background_green;
          break;
        case heif_channel_B:
          bkg16_1 = options_ext.background_blue;
          bkg16_2 = options_ext.secondary_background_blue;
          break;
        default:
          assert(false);
          bkg16_1 = bkg16_2 = 0;
      }

      Pixel bkg1 = static_cast<Pixel>(bkg16_1 >> (16 - input->get_bits_per_pixel(channel)));
      Pixel bkg2 = static_cast<Pixel>(bkg16_2 >> (16 - input->get_bits_per_pixel(channel)));

      for (uint32_t y = 0; y < height; y++)
        for (uint32_t x = 0; x < width; x++) {
          uint8_t parity = (x / options_ext.checkerboard_square_size + y / options_ext.checkerboard_square_size) % 2;
          Pixel bkg = parity ? bkg1 : bkg2;

          int a = p_alpha[y * stride_alpha + x];
          p_out[y * stride_out + x] = static_cast<Pixel>((p_in[y * stride_in + x] * a + bkg * (alpha_max - a)) >> bpp_alpha);
        }
    }

  }

  if (options_ext.alpha_composition_mode != heif_alpha_composition_mode_none) {
    Result<std::shared_ptr<HeifPixelImage>> convOutput = ::convert_colorspace(outimg,
                                                                              input_raw->get_colorspace(),
                                                                              input_raw->get_chroma_format(),
                                                                              input_state.nclx,
                                                                              input_state.bits_per_pixel,
                                                                              options, &options_ext_skip_alpha,
                                                                              limits);
    if (!convOutput) {
      return convOutput.error();
    }
    else {
      return convOutput;
    }
  }
  else {
    return outimg;
  }
}

template class Op_flatten_alpha_plane<uint8_t>;
template class Op_flatten_alpha_plane<uint16_t>;


std::vector<ColorStateWithCost>
Op_adjust_alpha_bit_depth::state_after_conversion(const ColorState& input_state,
                                                  const ColorState& target_state,
                                                  const heif_color_conversion_options& options,
                                                  const heif_color_conversion_options_ext& options_ext) const
{
  // Only applicable when alpha BPP differs from color BPP
  if (!input_state.has_alpha ||
      input_state.get_alpha_bits_per_pixel() == input_state.bits_per_pixel) {
    return {};
  }

  // Only for planar formats with alpha
  if (input_state.chroma != heif_chroma_monochrome &&
      input_state.chroma != heif_chroma_420 &&
      input_state.chroma != heif_chroma_422 &&
      input_state.chroma != heif_chroma_444) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state = input_state;
  output_state.alpha_bits_per_pixel = input_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_adjust_alpha_bit_depth::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                              const ColorState& input_state,
                                              const ColorState& target_state,
                                              const heif_color_conversion_options& options,
                                              const heif_color_conversion_options_ext& options_ext,
                                              const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();
  outimg->create(width, height, input->get_colorspace(), input->get_chroma_format());

  // Copy all non-alpha channels unchanged
  for (heif_channel channel : {heif_channel_Y, heif_channel_Cb, heif_channel_Cr,
                                heif_channel_R, heif_channel_G, heif_channel_B}) {
    if (input->has_channel(channel)) {
      outimg->copy_new_channel_from(input, channel, channel, limits);
    }
  }

  if (!input->has_channel(heif_channel_Alpha)) {
    return outimg;
  }

  int input_alpha_bpp = input->get_bits_per_pixel(heif_channel_Alpha);
  int target_bpp = input_state.bits_per_pixel;

  uint32_t alpha_width = input->get_width(heif_channel_Alpha);
  uint32_t alpha_height = input->get_height(heif_channel_Alpha);

  if (auto err = outimg->add_channel(heif_channel_Alpha, alpha_width, alpha_height, target_bpp, limits)) {
    return err;
  }

  if (input_alpha_bpp <= 8 && target_bpp > 8) {
    // Upscale: 8-bit alpha -> HDR using pattern replication
    const uint8_t* p_in;
    size_t stride_in;
    p_in = input->get_channel_memory(heif_channel_Alpha, &stride_in);

    uint16_t* p_out;
    size_t stride_out;
    p_out = (uint16_t*) outimg->get_channel_memory(heif_channel_Alpha, &stride_out);
    stride_out /= 2;

    int shift1 = target_bpp - input_alpha_bpp;
    int shift2 = 2 * input_alpha_bpp - target_bpp;

    for (uint32_t y = 0; y < alpha_height; y++)
      for (uint32_t x = 0; x < alpha_width; x++) {
        int in = p_in[y * stride_in + x];
        p_out[y * stride_out + x] = (uint16_t) ((in << shift1) | (in >> shift2));
      }
  }
  else if (input_alpha_bpp > 8 && target_bpp <= 8) {
    // Downscale: HDR alpha -> 8-bit
    const uint16_t* p_in;
    size_t stride_in;
    p_in = (const uint16_t*) input->get_channel_memory(heif_channel_Alpha, &stride_in);
    stride_in /= 2;

    uint8_t* p_out;
    size_t stride_out;
    p_out = outimg->get_channel_memory(heif_channel_Alpha, &stride_out);

    int shift = input_alpha_bpp - 8;

    for (uint32_t y = 0; y < alpha_height; y++)
      for (uint32_t x = 0; x < alpha_width; x++) {
        p_out[y * stride_out + x] = (uint8_t) (p_in[y * stride_in + x] >> shift);
      }
  }
  else if (input_alpha_bpp > 8 && target_bpp > 8) {
    // HDR alpha -> different HDR: rescale within uint16_t
    const uint16_t* p_in;
    size_t stride_in;
    p_in = (const uint16_t*) input->get_channel_memory(heif_channel_Alpha, &stride_in);
    stride_in /= 2;

    uint16_t* p_out;
    size_t stride_out;
    p_out = (uint16_t*) outimg->get_channel_memory(heif_channel_Alpha, &stride_out);
    stride_out /= 2;

    if (target_bpp > input_alpha_bpp) {
      int shift1 = target_bpp - input_alpha_bpp;
      int shift2 = 2 * input_alpha_bpp - target_bpp;
      for (uint32_t y = 0; y < alpha_height; y++)
        for (uint32_t x = 0; x < alpha_width; x++) {
          int in = p_in[y * stride_in + x];
          p_out[y * stride_out + x] = (uint16_t) ((in << shift1) | (in >> shift2));
        }
    }
    else {
      int shift = input_alpha_bpp - target_bpp;
      for (uint32_t y = 0; y < alpha_height; y++)
        for (uint32_t x = 0; x < alpha_width; x++) {
          p_out[y * stride_out + x] = (uint16_t) (p_in[y * stride_in + x] >> shift);
        }
    }
  }
  else {
    // SDR alpha -> different SDR (both <= 8)
    const uint8_t* p_in;
    size_t stride_in;
    p_in = input->get_channel_memory(heif_channel_Alpha, &stride_in);

    uint8_t* p_out;
    size_t stride_out;
    p_out = outimg->get_channel_memory(heif_channel_Alpha, &stride_out);

    if (target_bpp > input_alpha_bpp) {
      int shift1 = target_bpp - input_alpha_bpp;
      int shift2 = 2 * input_alpha_bpp - target_bpp;
      for (uint32_t y = 0; y < alpha_height; y++)
        for (uint32_t x = 0; x < alpha_width; x++) {
          int in = p_in[y * stride_in + x];
          p_out[y * stride_out + x] = (uint8_t) ((in << shift1) | (in >> shift2));
        }
    }
    else {
      int shift = input_alpha_bpp - target_bpp;
      for (uint32_t y = 0; y < alpha_height; y++)
        for (uint32_t x = 0; x < alpha_width; x++) {
          p_out[y * stride_out + x] = (uint8_t) (p_in[y * stride_in + x] >> shift);
        }
    }
  }

  return outimg;
}
