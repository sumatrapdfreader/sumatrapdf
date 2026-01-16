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
#include <cassert>
#include <memory>
#include <vector>
#include "rgb2yuv.h"
#include "nclx.h"
#include "common_utils.h"


template<class Pixel>
std::vector<ColorStateWithCost>
Op_RGB_to_YCbCr<Pixel>::state_after_conversion(const ColorState& input_state,
                                               const ColorState& target_state,
                                               const heif_color_conversion_options& options,
                                               const heif_color_conversion_options_ext& options_ext) const
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  if ((input_state.bits_per_pixel > 8) != hdr) {
    return {};
  }

  // TODO: add support for <8 bpp
  if (input_state.bits_per_pixel < 8) {
    return {};
  }

  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444) {
    return {};
  }

  int matrix = target_state.nclx.get_matrix_coefficients();
  if (matrix == 11 || matrix == 14) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to YCbCr

  // This Op only implements the nearest-neighbor downsampling algorithm, but it can still convert to 4:4:4.

  if (target_state.chroma != heif_chroma_444 &&
      (options.preferred_chroma_downsampling_algorithm == heif_chroma_downsampling_nearest_neighbor ||
       !options.only_use_preferred_chroma_algorithm)) {

    output_state.colorspace = heif_colorspace_YCbCr;
    output_state.chroma = target_state.chroma;
    output_state.has_alpha = input_state.has_alpha;  // we simply keep the old alpha plane
    output_state.bits_per_pixel = input_state.bits_per_pixel;
    output_state.nclx = target_state.nclx;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }
  else {
    // --- convert to YCbCr 4:4:4

    output_state.colorspace = heif_colorspace_YCbCr;
    output_state.chroma = heif_chroma_444;
    output_state.has_alpha = input_state.has_alpha;  // we simply keep the old alpha plane
    output_state.bits_per_pixel = input_state.bits_per_pixel;
    output_state.nclx = target_state.nclx;

    states.emplace_back(output_state, SpeedCosts_Unoptimized);
  }

  return states;
}

template<class Pixel>
Result<std::shared_ptr<HeifPixelImage>>
Op_RGB_to_YCbCr<Pixel>::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                           const ColorState& input_state,
                                           const ColorState& target_state,
                                           const heif_color_conversion_options& options,
                                           const heif_color_conversion_options_ext& options_ext,
                                           const heif_security_limits* limits) const
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  heif_chroma chroma = target_state.chroma;
  int subH = chroma_h_subsampling(chroma);
  int subV = chroma_v_subsampling(chroma);

  int bpp = input->get_bits_per_pixel(heif_channel_R);
  if (bpp < 8 || (bpp > 8) != hdr) {
    return Error::InternalError;
  }

  bool has_alpha = input->has_channel(heif_channel_Alpha);

  if (has_alpha && input->get_bits_per_pixel(heif_channel_Alpha) != bpp) {
    return Error::InternalError;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_YCbCr, chroma);

  uint32_t cwidth = (width + subH - 1) / subH;
  uint32_t cheight = (height + subV - 1) / subV;

  if (auto err = outimg->add_plane(heif_channel_Y, width, height, bpp, limits) ||
                 outimg->add_plane(heif_channel_Cb, cwidth, cheight, bpp, limits) ||
                 outimg->add_plane(heif_channel_Cr, cwidth, cheight, bpp, limits)) {
    return err;
  }

  if (has_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, bpp, limits)) {
      return err;
    }
  }

  const Pixel* in_r, * in_g, * in_b;
  size_t in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  Pixel* out_y, * out_cb, * out_cr;
  size_t out_y_stride = 0, out_cb_stride = 0, out_cr_stride = 0, out_a_stride = 0;

  in_r = (const Pixel*) input->get_plane(heif_channel_R, &in_r_stride);
  in_g = (const Pixel*) input->get_plane(heif_channel_G, &in_g_stride);
  in_b = (const Pixel*) input->get_plane(heif_channel_B, &in_b_stride);
  out_y = (Pixel*) outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = (Pixel*) outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = (Pixel*) outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  const uint8_t* in_a;
  uint8_t* out_a;

  if (has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }
  else {
    in_a = nullptr;
    out_a = nullptr;
  }

  if (hdr) {
    in_r_stride /= 2;
    in_g_stride /= 2;
    in_b_stride /= 2;
    out_y_stride /= 2;
    out_cb_stride /= 2;
    out_cr_stride /= 2;
  }

  uint16_t halfRange = (uint16_t) (1 << (bpp - 1));
  int32_t fullRange = (1 << bpp) - 1;
  float limited_range_offset = static_cast<float>(16 << (bpp - 8));

  int matrix_coeffs = 2;
  RGB_to_YCbCr_coefficients coeffs = RGB_to_YCbCr_coefficients::defaults();
  bool full_range_flag = true;
  full_range_flag = target_state.nclx.get_full_range_flag();
  matrix_coeffs = target_state.nclx.get_matrix_coefficients();
  coeffs = get_RGB_to_YCbCr_coefficients(target_state.nclx.get_matrix_coefficients(),
                                         target_state.nclx.get_colour_primaries());

  uint32_t x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (matrix_coeffs == 0) {
        if (full_range_flag) {
          out_y[y * out_y_stride + x] = in_g[y * in_g_stride + x];
        }
        else {
          float v = (((in_g[y * in_g_stride + x] * 219.0f) / 256) + limited_range_offset);
          out_y[y * out_y_stride + x] = (Pixel) clip_f_u16(v, fullRange);
        }
      }
      else if (matrix_coeffs == 8) {
        // Note: this is the YCgCo transform for equal Y/C bit depths, H.273(2024) Eq.51-57.
        // To avoid a loss in accuracy, input bit-depth must be extended by two bits.
        out_y[y * out_y_stride + x] = static_cast<Pixel>(in_g[y * in_g_stride + x] / 2 + (in_r[y * in_r_stride + x] + in_b[y * in_b_stride + x]) / 4);
      }
      else {
        float r = in_r[y * in_r_stride + x];
        float g = in_g[y * in_g_stride + x];
        float b = in_b[y * in_b_stride + x];

        float v = r * coeffs.c[0][0] + g * coeffs.c[0][1] + b * coeffs.c[0][2];
        if (!full_range_flag) {
          v = (((v * 219) / 256) + limited_range_offset);
        }

        Pixel pix = (Pixel) clip_f_u16(v, fullRange);

        out_y[y * out_y_stride + x] = pix;
      }
    }
  }

  for (y = 0; y < height; y += subV) {
    for (x = 0; x < width; x += subH) {
      if (matrix_coeffs == 0) {
        if (full_range_flag) {
          out_cb[(y / subV) * out_cb_stride + (x / subH)] = in_b[y * in_b_stride + x];
          out_cr[(y / subV) * out_cb_stride + (x / subH)] = in_r[y * in_b_stride + x];
        }
        else {
          out_cb[(y / subV) * out_cb_stride + (x / subH)] = (Pixel) clip_f_u16(
              ((in_b[y * in_b_stride + x] * 224.0f) / 256) + limited_range_offset, fullRange);
          out_cr[(y / subV) * out_cb_stride + (x / subH)] = (Pixel) clip_f_u16(
              ((in_r[y * in_b_stride + x] * 224.0f) / 256) + limited_range_offset, fullRange);
        }
      }
      else if (matrix_coeffs == 8) {
        // Note: this is the YCgCo transform for equal Y/C bit depths, H.273(2024) Eq.51-57.
        // To avoid a loss in accuracy, input bit-depth must be extended by two bits.
        out_cb[(y / subV) * out_cb_stride + (x / subH)] = static_cast<Pixel>(clip_int_u16(in_g[y * in_g_stride + x] / 2
                                                                                          - (in_r[y * in_r_stride + x] + in_b[y * in_b_stride + x]) / 4
                                                                                          + halfRange, (uint16_t) fullRange));
        out_cr[(y / subV) * out_cr_stride + (x / subH)] = static_cast<Pixel>(clip_int_u16((in_r[y * in_r_stride + x] - in_b[y * in_b_stride + x]) / 2
                                                                                          + halfRange, (uint16_t) fullRange));
      }
      else {
        float r = in_r[y * in_r_stride + x];
        float g = in_g[y * in_g_stride + x];
        float b = in_b[y * in_b_stride + x];

        if (subH > 1 || subV > 1) {
          uint32_t x2 = (x + 1 < width && subH == 2 && subV == 2) ? x + 1 : x;  // subV==2 -> Do not center for 4:2:2 (see comment in Op_RGB24_32_to_YCbCr, github issue #521)
          uint32_t y2 = (y + 1 < height && subV == 2) ? y + 1 : y;

          r += in_r[y * in_r_stride + x2];
          g += in_g[y * in_g_stride + x2];
          b += in_b[y * in_b_stride + x2];

          r += in_r[y2 * in_r_stride + x];
          g += in_g[y2 * in_g_stride + x];
          b += in_b[y2 * in_b_stride + x];

          r += in_r[y2 * in_r_stride + x2];
          g += in_g[y2 * in_g_stride + x2];
          b += in_b[y2 * in_b_stride + x2];

          r *= 0.25f;
          g *= 0.25f;
          b *= 0.25f;
        }

        float cb, cr;

        cb = r * coeffs.c[1][0] + g * coeffs.c[1][1] + b * coeffs.c[1][2];
        cr = r * coeffs.c[2][0] + g * coeffs.c[2][1] + b * coeffs.c[2][2];

        if (!full_range_flag) {
          cb = (cb * 224) / 256;
          cr = (cr * 224) / 256;
        }

        out_cb[(y / subV) * out_cb_stride + (x / subH)] = (Pixel) clip_f_u16(cb + halfRange, fullRange);
        out_cr[(y / subV) * out_cr_stride + (x / subH)] = (Pixel) clip_f_u16(cr + halfRange, fullRange);
      }
    }
  }


  if (has_alpha) {
    int bpp_a = input->get_bits_per_pixel(heif_channel_Alpha);
    int alphaCopyWidth = (bpp_a > 8 ? width * 2 : width);

    for (y = 0; y < height; y++) {
      memcpy(&out_a[y * out_a_stride], &in_a[y * in_a_stride], alphaCopyWidth);
    }
  }

  return outimg;
}

template class Op_RGB_to_YCbCr<uint8_t>;
template class Op_RGB_to_YCbCr<uint16_t>;


std::vector<ColorStateWithCost>
Op_RRGGBBxx_HDR_to_YCbCr420::state_after_conversion(const ColorState& input_state,
                                                    const ColorState& target_state,
                                                    const heif_color_conversion_options& options,
                                                    const heif_color_conversion_options_ext& options_ext) const
{
  // this Op only implements the nearest-neighbor algorithm

  if (target_state.chroma != heif_chroma_444) {
    if (options.preferred_chroma_downsampling_algorithm != heif_chroma_downsampling_nearest_neighbor &&
        options.only_use_preferred_chroma_algorithm) {
      return {};
    }
  }

  if (input_state.colorspace != heif_colorspace_RGB ||
      !(input_state.chroma == heif_chroma_interleaved_RRGGBB_BE ||
        input_state.chroma == heif_chroma_interleaved_RRGGBB_LE ||
        input_state.chroma == heif_chroma_interleaved_RRGGBBAA_BE ||
        input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE) ||
      input_state.bits_per_pixel <= 8) {
    return {};
  }

  int matrix = target_state.nclx.get_matrix_coefficients();
  if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
    return {};
  }
  if (!target_state.nclx.get_full_range_flag()) {
    return {};
  }

  if (target_state.chroma != heif_chroma_420) {
    return {};
  }


  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  // --- convert to YCbCr

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = heif_chroma_420;
  output_state.has_alpha = input_state.has_alpha;  // we generate an alpha plane if the source contains data
  output_state.bits_per_pixel = input_state.bits_per_pixel;
  output_state.nclx = target_state.nclx;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RRGGBBxx_HDR_to_YCbCr420::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                const ColorState& input_state,
                                                const ColorState& target_state,
                                                const heif_color_conversion_options& options,
                                                const heif_color_conversion_options_ext& options_ext,
                                                const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  int bpp = input->get_bits_per_pixel(heif_channel_interleaved);

  bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
                    input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE);

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_YCbCr, heif_chroma_420);

  int bytesPerPixel = has_alpha ? 8 : 6;

  uint32_t cwidth = (width + 1) / 2;
  uint32_t cheight = (height + 1) / 2;

  if (auto err = outimg->add_plane(heif_channel_Y, width, height, bpp, limits) ||
                 outimg->add_plane(heif_channel_Cb, cwidth, cheight, bpp, limits) ||
                 outimg->add_plane(heif_channel_Cr, cwidth, cheight, bpp, limits)) {
    return err;
  }

  if (has_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, bpp, limits)) {
      return err;
    }
  }

  const uint8_t* in_p;
  size_t in_p_stride = 0;

  uint16_t* out_y, * out_cb, * out_cr, * out_a = nullptr;
  size_t out_y_stride = 0, out_cb_stride = 0, out_cr_stride = 0, out_a_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);
  out_y = (uint16_t*) outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = (uint16_t*) outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = (uint16_t*) outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (has_alpha) {
    out_a = (uint16_t*) outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }


  // adapt stride as we are pointing to 16bit integers
  out_y_stride /= 2;
  out_cb_stride /= 2;
  out_cr_stride /= 2;
  out_a_stride /= 2;

  uint16_t halfRange = (uint16_t) (1 << (bpp - 1));
  int32_t fullRange = (1 << bpp) - 1;
  float limited_range_offset = static_cast<float>(16 << (bpp - 8));

  // le=1 for little endian, le=0 for big endian
  int le = (input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE ||
            input->get_chroma_format() == heif_chroma_interleaved_RRGGBB_LE) ? 1 : 0;

  bool full_range_flag = target_state.nclx.get_full_range_flag();
  RGB_to_YCbCr_coefficients coeffs = get_RGB_to_YCbCr_coefficients(
      target_state.nclx.get_matrix_coefficients(),
      target_state.nclx.get_colour_primaries());

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {

      const uint8_t* in = &in_p[y * in_p_stride + bytesPerPixel * x];

      float r = static_cast<float>((in[0 + le] << 8) | in[1 - le]);
      float g = static_cast<float>((in[2 + le] << 8) | in[3 - le]);
      float b = static_cast<float>((in[4 + le] << 8) | in[5 - le]);

      float v = r * coeffs.c[0][0] + g * coeffs.c[0][1] + b * coeffs.c[0][2];

      if (!full_range_flag) {
        v = v * 0.85547f + limited_range_offset;  // 0.85547 = 219/256
      }

      out_y[y * out_y_stride + x] = clip_f_u16(v, fullRange);

      if (has_alpha) {
        uint16_t a = (uint16_t) ((in[6 + le] << 8) | in[7 - le]);
        out_a[y * out_a_stride + x] = a;
      }
    }
  }

  for (uint32_t y = 0; y < height; y += 2) {
    for (uint32_t x = 0; x < width; x += 2) {
      const uint8_t* in = &in_p[y * in_p_stride + bytesPerPixel * x];

      float r = static_cast<float>((in[0 + le] << 8) | in[1 - le]);
      float g = static_cast<float>((in[2 + le] << 8) | in[3 - le]);
      float b = static_cast<float>((in[4 + le] << 8) | in[5 - le]);

      int dx = (x + 1 < width) ? bytesPerPixel : 0;
      int dy = (y + 1 < height) ? (int)in_p_stride : 0;

      r += static_cast<float>((in[0 + le + dx] << 8) | in[1 - le + dx]);
      g += static_cast<float>((in[2 + le + dx] << 8) | in[3 - le + dx]);
      b += static_cast<float>((in[4 + le + dx] << 8) | in[5 - le + dx]);

      r += static_cast<float>((in[0 + le + dy] << 8) | in[1 - le + dy]);
      g += static_cast<float>((in[2 + le + dy] << 8) | in[3 - le + dy]);
      b += static_cast<float>((in[4 + le + dy] << 8) | in[5 - le + dy]);

      r += static_cast<float>((in[0 + le + dx + dy] << 8) | in[1 - le + dx + dy]);
      g += static_cast<float>((in[2 + le + dx + dy] << 8) | in[3 - le + dx + dy]);
      b += static_cast<float>((in[4 + le + dx + dy] << 8) | in[5 - le + dx + dy]);

      r *= 0.25f;
      g *= 0.25f;
      b *= 0.25f;

      float cb = r * coeffs.c[1][0] + g * coeffs.c[1][1] + b * coeffs.c[1][2];
      float cr = r * coeffs.c[2][0] + g * coeffs.c[2][1] + b * coeffs.c[2][2];

      if (!full_range_flag) {
        cb = cb * 0.8750f;  // 0.8750 = 224/256
        cr = cr * 0.8750f;  // 0.8750 = 224/256
      }

      out_cb[(y / 2) * out_cb_stride + (x / 2)] = clip_f_u16(halfRange + cb, fullRange);
      out_cr[(y / 2) * out_cr_stride + (x / 2)] = clip_f_u16(halfRange + cr, fullRange);
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RGB24_32_to_YCbCr::state_after_conversion(const ColorState& input_state,
                                             const ColorState& target_state,
                                             const heif_color_conversion_options& options,
                                             const heif_color_conversion_options_ext& options_ext) const
{
  // this Op only implements the nearest-neighbor algorithm

  if (target_state.chroma != heif_chroma_444) {
    if (options.preferred_chroma_downsampling_algorithm != heif_chroma_downsampling_nearest_neighbor &&
        options.only_use_preferred_chroma_algorithm) {
      return {};
    }
  }

  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RGB &&
       input_state.chroma != heif_chroma_interleaved_RGBA)) {
    return {};
  }

  if (target_state.chroma != heif_chroma_420 &&
      target_state.chroma != heif_chroma_422 &&
      target_state.chroma != heif_chroma_444) {
    return {};
  }

  int matrix = target_state.nclx.get_matrix_coefficients();
  if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = target_state.chroma;
  output_state.has_alpha = target_state.has_alpha;
  output_state.bits_per_pixel = 8;
  output_state.nclx = target_state.nclx;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


inline void set_chroma_pixels(uint8_t* out_cb, uint8_t* out_cr,
                              uint8_t r, uint8_t g, uint8_t b,
                              const RGB_to_YCbCr_coefficients& coeffs,
                              bool full_range_flag)
{
  float cb = r * coeffs.c[1][0] + g * coeffs.c[1][1] + b * coeffs.c[1][2];
  float cr = r * coeffs.c[2][0] + g * coeffs.c[2][1] + b * coeffs.c[2][2];

  if (full_range_flag) {
    *out_cb = clip_f_u8(cb + 128);
    *out_cr = clip_f_u8(cr + 128);
  }
  else {
    *out_cb = (uint8_t) clip_f_u8(cb * 0.875f + 128.0f);
    *out_cr = (uint8_t) clip_f_u8(cr * 0.875f + 128.0f);
  }
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RGB24_32_to_YCbCr::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                         const ColorState& input_state,
                                         const ColorState& target_state,
                                         const heif_color_conversion_options& options,
                                         const heif_color_conversion_options_ext& options_ext,
                                         const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  auto chroma = target_state.chroma;
  uint8_t chromaSubH = chroma_h_subsampling(chroma);
  uint8_t chromaSubV = chroma_v_subsampling(chroma);

  outimg->create(width, height, heif_colorspace_YCbCr, chroma);

  int chroma_width = (width + chromaSubH - 1) / chromaSubH;
  int chroma_height = (height + chromaSubV - 1) / chromaSubV;

  const bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_32bit);
  const bool want_alpha = target_state.has_alpha;

  if (auto err = outimg->add_plane(heif_channel_Y, width, height, 8, limits) ||
                 outimg->add_plane(heif_channel_Cb, chroma_width, chroma_height, 8, limits) ||
                 outimg->add_plane(heif_channel_Cr, chroma_width, chroma_height, 8, limits)) {
    return err;
  }

  if (want_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, 8, limits)) {
      return err;
    }
  }

  uint8_t* out_cb, * out_cr, * out_y, * out_a;
  size_t out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0, out_a_stride = 0;

  const uint8_t* in_p;
  size_t in_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_stride);

  out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (want_alpha) {
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }
  else {
    out_a = nullptr;
  }


  RGB_to_YCbCr_coefficients coeffs = RGB_to_YCbCr_coefficients::defaults();
  bool full_range_flag = true;
  full_range_flag = target_state.nclx.get_full_range_flag();
  coeffs = get_RGB_to_YCbCr_coefficients(target_state.nclx.get_matrix_coefficients(),
                                         target_state.nclx.get_colour_primaries());


  int bytes_per_pixel = (has_alpha ? 4 : 3);

  for (uint32_t y = 0; y < height; y++) {
    const uint8_t* p = &in_p[y * in_stride];

    for (uint32_t x = 0; x < width; x++) {
      uint8_t r = p[0];
      uint8_t g = p[1];
      uint8_t b = p[2];
      p += bytes_per_pixel;

      float yv = r * coeffs.c[0][0] + g * coeffs.c[0][1] + b * coeffs.c[0][2];

      if (full_range_flag) {
        out_y[y * out_y_stride + x] = clip_f_u8(yv);
      }
      else {
        out_y[y * out_y_stride + x] = (uint8_t) (clip_f_u16(yv * 0.85547f, 219) + 16);
      }
    }
  }

  if (chromaSubH == 1 && chromaSubV == 1) {
    // chroma 4:4:4

    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* p = &in_p[y * in_stride];

      for (uint32_t x = 0; x < width; x++) {
        uint8_t r = p[0];
        uint8_t g = p[1];
        uint8_t b = p[2];
        p += bytes_per_pixel;

        set_chroma_pixels(out_cb + y * out_cb_stride + x,
                          out_cr + y * out_cr_stride + x,
                          r, g, b,
                          coeffs, full_range_flag);
      }
    }
  }
  else if (chromaSubH == 2 && chromaSubV == 2) {
    // chroma 4:2:0

    for (uint32_t y = 0; y < (height & ~1U); y += 2) {
      const uint8_t* p = &in_p[y * in_stride];

      for (uint32_t x = 0; x < (width & ~1U); x += 2) {
        uint8_t r = uint8_t((p[0] + p[bytes_per_pixel + 0] + p[in_stride + 0] + p[bytes_per_pixel + in_stride + 0]) / 4);
        uint8_t g = uint8_t((p[1] + p[bytes_per_pixel + 1] + p[in_stride + 1] + p[bytes_per_pixel + in_stride + 1]) / 4);
        uint8_t b = uint8_t((p[2] + p[bytes_per_pixel + 2] + p[in_stride + 2] + p[bytes_per_pixel + in_stride + 2]) / 4);

        p += bytes_per_pixel * 2;

        set_chroma_pixels(out_cb + (y / 2) * out_cb_stride + (x / 2),
                          out_cr + (y / 2) * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);
      }
    }

    // 4:2:0 right column (if odd width)
    if (width & 1) {
      uint32_t x = width - 1;
      const uint8_t* p = &in_p[x * bytes_per_pixel];

      for (uint32_t y = 0; y < height; y += 2) {
        uint8_t r, g, b;
        if (y + 1 < height) {
          r = uint8_t((p[0] + p[in_stride + 0]) / 2);
          g = uint8_t((p[1] + p[in_stride + 1]) / 2);
          b = uint8_t((p[2] + p[in_stride + 2]) / 2);
        }
        else {
          r = p[0];
          g = p[1];
          b = p[2];
        }

        set_chroma_pixels(out_cb + (y / 2) * out_cb_stride + (x / 2),
                          out_cr + (y / 2) * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);

        p += in_stride * 2;
      }
    }

    // 4:2:0 bottom row (if odd height)
    if (height & 1) {
      uint32_t y = height - 1;
      const uint8_t* p = &in_p[y * in_stride];

      for (uint32_t x = 0; x < width; x += 2) {
        uint8_t r, g, b;
        if (x + 1 < width) {
          r = uint8_t((p[0] + p[bytes_per_pixel + 0]) / 2);
          g = uint8_t((p[1] + p[bytes_per_pixel + 1]) / 2);
          b = uint8_t((p[2] + p[bytes_per_pixel + 2]) / 2);
        }
        else {
          r = p[0];
          g = p[1];
          b = p[2];
        }

        set_chroma_pixels(out_cb + (y / 2) * out_cb_stride + (x / 2),
                          out_cr + (y / 2) * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);

        p += bytes_per_pixel * 2;
      }
    }
  }
  else if (chromaSubH == 2 && chromaSubV == 1) {
    // chroma 4:2:2

    for (uint32_t y = 0; y < height; y++) {
      const uint8_t* p = &in_p[y * in_stride];

      for (uint32_t x = 0; x < width; x += 2) {
        uint8_t r, g, b;

        // TODO: it still is an open question where the 'correct' chroma sample positions are for 4:2:2
        // Since 4:2:2 is primarily used for video content and as there is no way to signal center position for h.265,
        // we currently use left-aligned sampling. See the discussion here: https://github.com/strukturag/libheif/issues/521
#if USE_CENTER_CHROMA_422
        if (x + 1 < width) {
          r = uint8_t((p[0] + p[bytes_per_pixel + 0]) / 2);
          g = uint8_t((p[1] + p[bytes_per_pixel + 1]) / 2);
          b = uint8_t((p[2] + p[bytes_per_pixel + 2]) / 2);
        }
        else {
          r = p[0];
          g = p[1];
          b = p[2];
        }
#else
        r = p[0];
        g = p[1];
        b = p[2];
#endif

        p += bytes_per_pixel * 2;

        set_chroma_pixels(out_cb + y * out_cb_stride + (x / 2),
                          out_cr + y * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);
      }
    }
  }

  if (want_alpha) {
    if (has_alpha) {
      assert(bytes_per_pixel == 4);
    }

    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        uint8_t a = has_alpha ? in_p[y * in_stride + x * 4 + 3] : 0xff;

        // alpha
        out_a[y * out_a_stride + x] = a;
      }
    }
  }

  return outimg;
}


std::vector<ColorStateWithCost>
Op_RGB24_32_to_YCbCr444_GBR::state_after_conversion(const ColorState& input_state,
                                                    const ColorState& target_state,
                                                    const heif_color_conversion_options& options,
                                                    const heif_color_conversion_options_ext& options_ext) const
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RGB &&
       input_state.chroma != heif_chroma_interleaved_RGBA)) {
    return {};
  }

  if (target_state.nclx.get_matrix_coefficients() != 0) {
    return {};
  }

  if (!target_state.nclx.get_full_range_flag()) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = heif_chroma_444;
  output_state.has_alpha = target_state.has_alpha;
  output_state.bits_per_pixel = 8;
  output_state.nclx = target_state.nclx;

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


Result<std::shared_ptr<HeifPixelImage>>
Op_RGB24_32_to_YCbCr444_GBR::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                const ColorState& input_state,
                                                const ColorState& target_state,
                                                const heif_color_conversion_options& options,
                                                const heif_color_conversion_options_ext& options_ext,
                                                const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_YCbCr, heif_chroma_444);

  const bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_32bit);
  const bool want_alpha = target_state.has_alpha;

  if (auto err = outimg->add_plane(heif_channel_Y, width, height, 8, limits) ||
                 outimg->add_plane(heif_channel_Cb, width, height, 8, limits) ||
                 outimg->add_plane(heif_channel_Cr, width, height, 8, limits)) {
    return err;
  }

  if (want_alpha) {
    if (auto err = outimg->add_plane(heif_channel_Alpha, width, height, 8, limits)) {
      return err;
    }
  }

  uint8_t* out_cb, * out_cr, * out_y, * out_a = nullptr;
  size_t out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0, out_a_stride = 0;

  const uint8_t* in_p;
  size_t in_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_stride);

  out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (want_alpha) {
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }


  assert(target_state.nclx.get_matrix_coefficients() == 0);
  int bytes_per_pixel = (has_alpha ? 4 : 3);

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      uint8_t r = in_p[y * in_stride + x * bytes_per_pixel + 0];
      uint8_t g = in_p[y * in_stride + x * bytes_per_pixel + 1];
      uint8_t b = in_p[y * in_stride + x * bytes_per_pixel + 2];

      out_y[y * out_y_stride + x] = g;
      out_cb[y * out_cb_stride + x] = b;
      out_cr[y * out_cr_stride + x] = r;

      if (want_alpha) {
        uint8_t a =
            has_alpha ? in_p[y * in_stride + x * bytes_per_pixel + 3] : 0xff;
        out_a[y * out_a_stride + x] = a;
      }
    }
  }

  return outimg;
}
