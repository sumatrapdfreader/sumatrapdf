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

#include "bayer_bilinear.h"
#include <libheif/heif_uncompressed.h>
#include <array>
#include <cassert>
#include <utility>


std::vector<ColorStateWithCost>
Op_bayer_bilinear_to_RGB24_32::state_after_conversion(const ColorState& input_state,
                                             const ColorState& target_state,
                                             const heif_color_conversion_options& options,
                                             const heif_color_conversion_options_ext& options_ext) const
{
  if (input_state.colorspace != heif_colorspace_filter_array ||
      input_state.chroma != heif_chroma_planar) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  output_state.colorspace = heif_colorspace_RGB;
  output_state.has_alpha = false;

  if (input_state.bits_per_pixel == 8) {
    output_state.chroma = heif_chroma_interleaved_RGB;
    output_state.bits_per_pixel = 8;
  }
  else if (input_state.bits_per_pixel > 8 && input_state.bits_per_pixel <= 16) {
    output_state.chroma = heif_chroma_interleaved_RRGGBB_LE;
    output_state.bits_per_pixel = input_state.bits_per_pixel;
  }
  else {
    return {};
  }

  states.emplace_back(output_state, SpeedCosts_Unoptimized);

  return states;
}


// Map uncompressed component types to R/G/B output channel indices.
// Returns -1 for unknown component types.
static int component_type_to_rgb_index(uint16_t component_type)
{
  switch (component_type) {
    case heif_cmpd_component_type_red:
      return 0;
    case heif_cmpd_component_type_green:
      return 1;
    case heif_cmpd_component_type_blue:
      return 2;
    default:
      return -1;
  }
}


Result<std::shared_ptr<HeifPixelImage>>
Op_bayer_bilinear_to_RGB24_32::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                         const ColorState& input_state,
                                         const ColorState& target_state,
                                         const heif_color_conversion_options& options,
                                         const heif_color_conversion_options_ext& options_ext,
                                         const heif_security_limits* limits) const
{
  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  if (!input->has_any_bayer_pattern()) {
    return Error::InternalError;
  }

  const BayerPattern& pattern = input->get_any_bayer_pattern();
  uint16_t pw = pattern.pattern_width;
  uint16_t ph = pattern.pattern_height;

  if (pw == 0 || ph == 0) {
    return Error::InternalError;
  }

  int bpp = input->get_bits_per_pixel(heif_channel_filter_array);
  bool hdr = bpp > 8;

  heif_chroma out_chroma = hdr ? heif_chroma_interleaved_RRGGBB_LE : heif_chroma_interleaved_RGB;

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_RGB, out_chroma);

  if (auto err = outimg->add_channel(heif_channel_interleaved, width, height, bpp, limits)) {
    return err;
  }

  size_t in_stride = 0;
  const uint8_t* in_p = input->get_channel_memory(heif_channel_filter_array, &in_stride);

  size_t out_stride = 0;
  uint8_t* out_p = outimg->get_channel_memory(heif_channel_interleaved, &out_stride);

  // Build a lookup table: for each pattern position, which RGB channel (0=R,1=G,2=B) does it provide?
  std::vector<int> pattern_channel(pw * ph);
  for (int i = 0; i < pw * ph; i++) {
    uint16_t comp_type = input->get_component_type(pattern.pixels[i].component_id);
    pattern_channel[i] = component_type_to_rgb_index(comp_type);
    if (pattern_channel[i] < 0) {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "Bayer pattern contains component types that we currently cannot convert to RGB");
    }
  }

  // Precompute neighbor offset tables for each pattern position and channel.
  // neighbor_offsets[py * pw + px][ch] = list of (dx, dy) offsets to average.
  // For the channel this position directly provides: single entry (0, 0).
  // For other channels: all neighbor offsets within the search radius that provide that channel.
  std::vector<std::array<std::vector<std::pair<int, int>>, 3>> neighbor_offsets(pw * ph);

  for (int py = 0; py < ph; py++) {
    for (int px = 0; px < pw; px++) {
      int this_ch = pattern_channel[py * pw + px];
      auto& offsets = neighbor_offsets[py * pw + px];

      // The channel this position directly provides: just read from (0,0)
      offsets[this_ch].emplace_back(0, 0);

      // For the other two channels: collect neighbor offsets
      int search_radius_x = pw - 1;
      int search_radius_y = ph - 1;

      for (int dy = -search_radius_y; dy <= search_radius_y; dy++) {
        for (int dx = -search_radius_x; dx <= search_radius_x; dx++) {
          if (dx == 0 && dy == 0) {
            continue;
          }

          int npx = (((px + dx) % pw) + pw) % pw;
          int npy = (((py + dy) % ph) + ph) % ph;
          int neighbor_ch = pattern_channel[npy * pw + npx];

          if (neighbor_ch != this_ch) {
            offsets[neighbor_ch].emplace_back(dx, dy);
          }
        }
      }
    }
  }

  // Bilinear demosaicing using precomputed offset tables.
  auto demosaic = [&]<typename Pixel>(const Pixel* in, Pixel* out,
                                      size_t in_str, size_t out_str) {
    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        const auto& offsets = neighbor_offsets[(y % ph) * pw + (x % pw)];

        Pixel* out_pixel = &out[y * out_str + x * 3];

        for (int ch = 0; ch < 3; ch++) {
          const auto& ch_offsets = offsets[ch];
          int sum = 0;
          int count = 0;

          for (const auto& [dx, dy] : ch_offsets) {
            int nx = static_cast<int>(x) + dx;
            int ny = static_cast<int>(y) + dy;

            if (nx < 0 || nx >= static_cast<int>(width) ||
                ny < 0 || ny >= static_cast<int>(height)) {
              continue;
            }

            sum += in[ny * in_str + nx];
            count++;
          }

          out_pixel[ch] = count > 0 ? static_cast<Pixel>((sum + count / 2) / count) : 0;
        }
      }
    }
  };

  if (hdr) {
    demosaic(reinterpret_cast<const uint16_t*>(in_p),
             reinterpret_cast<uint16_t*>(out_p),
             in_stride / 2, out_stride / 2);
  }
  else {
    demosaic(in_p, out_p, in_stride, out_stride);
  }

  return outimg;
}
