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

#include <cassert>
#include "hdr_sdr.h"


std::vector<ColorStateWithCost>
Op_to_hdr_planes::state_after_conversion(const ColorState& input_state,
                                         const ColorState& target_state,
                                         const heif_color_conversion_options& options,
                                         const heif_color_conversion_options_ext& options_ext) const
{
  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.bits_per_pixel != 8) { // TODO: support for <8 bpp
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- increase bit depth

  output_state = input_state;
  output_state.bits_per_pixel = target_state.bits_per_pixel;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_to_hdr_planes::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                     const ColorState& input_state,
                                     const ColorState& target_state,
                                     const heif_color_conversion_options& options,
                                     const heif_color_conversion_options_ext& options_ext,
                                     const heif_security_limits* limits) const
{
  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(input->get_width(),
                 input->get_height(),
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_Y,
                               heif_channel_Cb,
                               heif_channel_Cr,
                               heif_channel_R,
                               heif_channel_G,
                               heif_channel_B,
                               heif_channel_Alpha}) {
    if (input->has_channel(channel)) {
      uint32_t width = input->get_width(channel);
      uint32_t height = input->get_height(channel);
      if (auto err = outimg->add_plane(channel, width, height, target_state.bits_per_pixel, limits)) {
        return err;
      }

      int input_bits = input->get_bits_per_pixel(channel);
      int output_bits = target_state.bits_per_pixel;

      int shift1 = output_bits - input_bits;
      int shift2 = 2 * input_bits - output_bits;

      const uint8_t* p_in;
      size_t stride_in;
      p_in = input->get_plane(channel, &stride_in);

      uint16_t* p_out;
      size_t stride_out;
      p_out = (uint16_t*) outimg->get_plane(channel, &stride_out);
      stride_out /= 2;

      for (uint32_t y = 0; y < height; y++)
        for (uint32_t x = 0; x < width; x++) {
          int in = p_in[y * stride_in + x];
          // TODO: support for <8 bpp may need more than two copies of the input bit pattern
          p_out[y * stride_out + x] = (uint16_t) ((in << shift1) | (in >> shift2));
        }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_to_sdr_planes::state_after_conversion(const ColorState& input_state,
                                         const ColorState& target_state,
                                         const heif_color_conversion_options& options,
                                         const heif_color_conversion_options_ext& options_ext) const
{
  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.bits_per_pixel == 8) {
    return {};
  }

  if (target_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- output bit depth = 8

  output_state = input_state;
  output_state.bits_per_pixel = 8;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_to_sdr_planes::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                     const ColorState& input_state,
                                     const ColorState& target_state,
                                     const heif_color_conversion_options& options,
                                     const heif_color_conversion_options_ext& options_ext,
                                     const heif_security_limits* limits) const
{

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(input->get_width(),
                 input->get_height(),
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_Y,
                               heif_channel_Cb,
                               heif_channel_Cr,
                               heif_channel_R,
                               heif_channel_G,
                               heif_channel_B,
                               heif_channel_Alpha}) {
    if (input->has_channel(channel)) {
      int input_bits = input->get_bits_per_pixel(channel);

      if (input_bits > 8) {
        uint32_t width = input->get_width(channel);
        uint32_t height = input->get_height(channel);
        if (auto err = outimg->add_plane(channel, width, height, 8, limits)) {
          return err;
        }

        int shift = input_bits - 8;

        const uint16_t* p_in;
        size_t stride_in;
        p_in = (uint16_t*) input->get_plane(channel, &stride_in);
        stride_in /= 2;

        uint8_t* p_out;
        size_t stride_out;
        p_out = outimg->get_plane(channel, &stride_out);

        for (uint32_t y = 0; y < height; y++)
          for (uint32_t x = 0; x < width; x++) {
            int in = p_in[y * stride_in + x];
            p_out[y * stride_out + x] = (uint8_t) (in >> shift); // TODO: I think no rounding here, but am not sure.
          }
      } else if (input_bits < 8) {
        uint32_t width = input->get_width(channel);
        uint32_t height = input->get_height(channel);
        if (auto err = outimg->add_plane(channel, width, height, 8, limits)) {
          return err;
        }

        // We also want to support converting inputs with < 4 bits per pixel covering the whole output range.
        // E.g. a 1-bit input should map to the output 0x00 / 0xFF.
        // We do so by constructing a fixed-point multiplication factor that effectively shifts and combines the input to
        // a string of bits that completely fills the output.
        //
        // Example: input 3 bit.
        // Factor (binary): 00100100|10010010
        // Input copies:    AAABBBCC|CDDDEEE0
        //                  \      /
        //                   output

        assert(input_bits > 0 && input_bits < 8);
        auto bit = static_cast<uint16_t>(1 << (16 - input_bits));
        uint16_t mulFactor = bit;

        for (;;) {
          bit >>= input_bits;
          if (!bit) {
            break;
          }

          mulFactor |= bit;
        }

        size_t stride_in;
        const uint8_t* p_in = input->get_plane(channel, &stride_in);

        size_t stride_out;
        uint8_t* p_out = outimg->get_plane(channel, &stride_out);

        for (uint32_t y = 0; y < height; y++)
          for (uint32_t x = 0; x < width; x++) {
            int in = p_in[y * stride_in + x];
            p_out[y * stride_out + x] = (uint8_t) ((in * mulFactor) >> 8);
          }
      } else {
        outimg->copy_new_plane_from(input, channel, channel, limits);
      }
    }
  }

  return outimg;
}

