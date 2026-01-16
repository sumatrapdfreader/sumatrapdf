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
#include <memory>
#include <vector>
#include "rgb2yuv_sharp.h"

#ifdef HAVE_LIBSHARPYUV

#include <sharpyuv/sharpyuv.h>
#include <sharpyuv/sharpyuv_csp.h>
#include "nclx.h"
#include "common_utils.h"

static inline bool PlatformIsBigEndian()
{
  int i = 1;
  return !*((char*) &i);
}

static uint16_t Shift(uint16_t v, int input_bits, int output_bits)
{
  if (input_bits == output_bits) return v;
  if (output_bits > input_bits) {
    int shift1 = output_bits - input_bits;
    int shift2 = 8 - shift1;
    return (uint16_t) ((v << shift1) | (v >> shift2));
  }
  else {
    int shift = input_bits - output_bits;
    return (uint16_t) (v >> shift);
  }
}

#endif

std::vector<ColorStateWithCost>
Op_Any_RGB_to_YCbCr_420_Sharp::state_after_conversion(
    const ColorState& input_state, const ColorState& target_state,
    const heif_color_conversion_options& options,
    const heif_color_conversion_options_ext& options_ext) const
{
#ifdef HAVE_LIBSHARPYUV
  // this Op only implements the sharp_yuv algorithm

  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (options.preferred_chroma_downsampling_algorithm != heif_chroma_downsampling_sharp_yuv &&
      options.only_use_preferred_chroma_algorithm) {
    return {};
  }

  // Only endianness matching the platform's is supported.
  const bool big_endian = PlatformIsBigEndian();
  const heif_chroma hdr_chroma = big_endian ? heif_chroma_interleaved_RRGGBB_BE
                                            : heif_chroma_interleaved_RRGGBB_LE;
  const heif_chroma hdr_with_alpha_chroma =
      big_endian ? heif_chroma_interleaved_RRGGBBAA_BE
                 : heif_chroma_interleaved_RRGGBBAA_LE;

  if (input_state.colorspace != heif_colorspace_RGB ||
      (
          input_state.chroma != heif_chroma_444 &&  // Planar input.
          input_state.chroma != heif_chroma_interleaved_RGB &&
          input_state.chroma != heif_chroma_interleaved_RGBA &&
          input_state.chroma != hdr_chroma &&
          input_state.chroma != hdr_with_alpha_chroma)) {
    return {};
  }

  if (input_state.bits_per_pixel != 8 && input_state.bits_per_pixel != 10 &&
      input_state.bits_per_pixel != 12 && input_state.bits_per_pixel != 16) {
    return {};
  }

  if (target_state.bits_per_pixel != 8 && target_state.bits_per_pixel != 10 &&
      target_state.bits_per_pixel != 12) {
    return {};
  }

  if (target_state.chroma != heif_chroma_420) {
    return {};
  }

  int matrix = target_state.nclx.get_matrix_coefficients();
  if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = heif_chroma_420;
  output_state.has_alpha = target_state.has_alpha;
  output_state.bits_per_pixel = target_state.bits_per_pixel;
  output_state.nclx = target_state.nclx;
  states.emplace_back(output_state, SpeedCosts_Slow);

  return states;
#else
  return {};
#endif
}

Result<std::shared_ptr<HeifPixelImage>>
Op_Any_RGB_to_YCbCr_420_Sharp::convert_colorspace(
    const std::shared_ptr<const HeifPixelImage>& input,
    const ColorState& input_state,
    const ColorState& target_state,
    const heif_color_conversion_options& options,
    const heif_color_conversion_options_ext& options_ext,
    const heif_security_limits* limits) const
{
#ifdef HAVE_LIBSHARPYUV
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  heif_chroma input_chroma = input->get_chroma_format();
  heif_chroma output_chroma = target_state.chroma;
  assert(output_chroma == heif_chroma_420);  // Only 420 is supported by libsharpyuv.
  uint8_t chromaSubH = chroma_h_subsampling(output_chroma);
  uint8_t chromaSubV = chroma_v_subsampling(output_chroma);

  outimg->create(width, height, heif_colorspace_YCbCr, output_chroma);

  uint32_t chroma_width = (width + chromaSubH - 1) / chromaSubH;
  uint32_t chroma_height = (height + chromaSubV - 1) / chromaSubV;

  bool has_alpha =
      input->get_chroma_format() == heif_chroma_interleaved_RGBA ||
      input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE ||
      input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
      (input->get_chroma_format() == heif_chroma_444 &&
       input->has_channel(heif_channel_Alpha));
  bool want_alpha = target_state.has_alpha;

  int output_bits = target_state.bits_per_pixel;
  if (auto err = outimg->add_plane(heif_channel_Y, width, height, output_bits, limits) ||
                 outimg->add_plane(heif_channel_Cb, chroma_width, chroma_height, output_bits, limits) ||
                 outimg->add_plane(heif_channel_Cr, chroma_width, chroma_height, output_bits, limits)) {
    return err;
  }

  if (want_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, output_bits, limits)) {
      return err;
    }
  }

  int input_bytes_per_sample =
      (input_chroma == heif_chroma_interleaved_RGB ||
       input_chroma == heif_chroma_interleaved_RGBA ||
       (input_chroma == heif_chroma_444 &&
        input->get_bits_per_pixel(heif_channel_R) <= 8))
      ? 1
      : 2;

  const uint8_t* in_r, * in_g, * in_b, * in_a = nullptr;
  size_t in_stride = 0;
  size_t in_a_stride = 0;
  bool planar_input = input_chroma == heif_chroma_444;
  int input_bits = 0;
  if (planar_input) {
    size_t in_r_stride = 0, in_g_stride = 0, in_b_stride = 0;
    in_r = input->get_plane(heif_channel_R, &in_r_stride);
    in_g = input->get_plane(heif_channel_G, &in_g_stride);
    in_b = input->get_plane(heif_channel_B, &in_b_stride);
    // The stride must be the same for all channels.
    if (in_r_stride != in_g_stride || in_r_stride != in_b_stride) {
      return Error::InternalError;
    }
    in_stride = in_r_stride;
    // Bpp must also be the same.
    input_bits = input->get_bits_per_pixel(heif_channel_R);
    if (input_bits != input->get_bits_per_pixel(heif_channel_G) ||
        input_bits != input->get_bits_per_pixel(heif_channel_B)) {
      return Error::InternalError;
    }
    if (has_alpha) {
      in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
    }
  }
  else {
    const uint8_t* in_p = input->get_plane(heif_channel_interleaved, &in_stride);
    input_bits = input->get_bits_per_pixel(heif_channel_interleaved);
    in_r = &in_p[input_bytes_per_sample * 0];
    in_g = &in_p[input_bytes_per_sample * 1];
    in_b = &in_p[input_bytes_per_sample * 2];
    if (has_alpha) {
      in_a = &in_p[input_bytes_per_sample * 3];
      in_a_stride = in_stride;
    }
  }

  size_t out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0;
  uint8_t* out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
  uint8_t* out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  uint8_t* out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  bool full_range_flag = true;
  Kr_Kb kr_kb = Kr_Kb::defaults();
  full_range_flag = target_state.nclx.get_full_range_flag();
  kr_kb =
      get_Kr_Kb(target_state.nclx.get_matrix_coefficients(),
                target_state.nclx.get_colour_primaries());

  SharpYuvColorSpace color_space = {
      kr_kb.Kr, kr_kb.Kb, output_bits,
      full_range_flag ? kSharpYuvRangeFull : kSharpYuvRangeLimited};
  SharpYuvConversionMatrix yuv_matrix;
  SharpYuvComputeConversionMatrix(&color_space, &yuv_matrix);
  int input_bytes_per_pixel = (has_alpha ? 4 : 3) * input_bytes_per_sample;
  int rgb_step = planar_input ? input_bytes_per_sample : input_bytes_per_pixel;

  int sharpyuv_ok =
      SharpYuvConvert(in_r, in_g, in_b, rgb_step, (int)in_stride,
                      input_bits, out_y, (int)out_y_stride, out_cb, (int)out_cb_stride,
                      out_cr, (int)out_cr_stride, output_bits,
                      input->get_width(), input->get_height(), &yuv_matrix);
  if (!sharpyuv_ok) {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_color_conversion,
                 "SharpYuv color conversion failed"};
  }

  if (want_alpha) {
    int le = (input_chroma == heif_chroma_interleaved_RRGGBBAA_LE ||
              input_chroma == heif_chroma_interleaved_RRGGBB_LE ||
              (planar_input && !PlatformIsBigEndian()))
             ? 1
             : 0;
    size_t out_a_stride;

    uint8_t* out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
    uint16_t alpha_max = static_cast<uint16_t>((1 << input_bits) - 1);
    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        const uint8_t* in = has_alpha ? &in_a[y * in_a_stride + x * rgb_step] : nullptr;
        uint16_t a = has_alpha
                     ? ((input_bits == 8)
                        ? in[0]
                        : (uint16_t) ((in[0 + le] << 8) | in[1 - le]))
                     : alpha_max;
        if (output_bits == 8) {
          out_a[y * out_a_stride + x] = (uint8_t) Shift(a, input_bits, output_bits);
        }
        else {
          uint16_t* out_a16 = reinterpret_cast<uint16_t*>(out_a);
          out_a16[y * out_a_stride / 2 + x] = Shift(a, input_bits, output_bits);
        }
      }
    }
  }

  return outimg;
#else
  return Error::InternalError;
#endif
}
