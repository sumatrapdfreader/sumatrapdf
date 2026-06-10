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

#include "unc_decoder_bytealign_component_interleave.h"
#include "unc_decoder_legacybase.h" // for skip_to_alignment
#include "unc_codec.h"
#include "unc_types.h"
#include "error.h"

#include <bit>
#include <cassert>
#include <cstring>
#include <vector>


unc_decoder_bytealign_component_interleave::unc_decoder_bytealign_component_interleave(
    uint32_t width, uint32_t height,
    std::shared_ptr<const Box_cmpd> cmpd,
    std::shared_ptr<const Box_uncC> uncC,
    const std::vector<uint32_t>& uncC_index_to_comp_ids)
    : unc_decoder(width, height, cmpd, uncC, uncC_index_to_comp_ids)
{
}


Result<std::vector<uint64_t>> unc_decoder_bytealign_component_interleave::get_tile_data_sizes() const
{
  uint64_t total_tile_size = 0;

  for (const auto& component : m_uncC->get_components()) {
    uint32_t bytes_per_sample = (component.component_bit_depth + 7) / 8;
    if (component.component_align_size > 0) {
      skip_to_alignment(bytes_per_sample, component.component_align_size);
    }

    if (bytes_per_sample != 0 && m_tile_width > UINT32_MAX / bytes_per_sample) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                   "uncompressed tile row size exceeds 32-bit range"};
    }
    uint32_t bytes_per_row = bytes_per_sample * m_tile_width;
    skip_to_alignment(bytes_per_row, m_uncC->get_row_align_size());

    total_tile_size += static_cast<uint64_t>(bytes_per_row) * m_tile_height;
  }

  if (m_uncC->get_tile_align_size() != 0) {
    skip_to_alignment(total_tile_size, m_uncC->get_tile_align_size());
  }

  return std::vector<uint64_t>{total_tile_size};
}


Error unc_decoder_bytealign_component_interleave::decode_tile(const std::vector<uint8_t>& tile_data,
                                                               std::shared_ptr<HeifPixelImage>& img,
                                                               uint32_t out_x0, uint32_t out_y0)
{
  const bool little_endian = m_uncC->is_components_little_endian();

  struct ComponentInfo {
    uint32_t bytes_per_sample;
    bool use;
    uint8_t* dst_plane;
    size_t dst_plane_stride;
  };

  const auto& components = m_uncC->get_components();
  const uint32_t num_components = static_cast<uint32_t>(components.size());
  std::vector<ComponentInfo> comp(num_components);

  for (uint32_t i = 0; i < num_components; i++) {
    const auto& c = components[i];
    comp[i].bytes_per_sample = (c.component_bit_depth + 7) / 8;

    comp[i].use = true; // map_uncompressed_component_to_channel(m_cmpd, c, &channel);
#if 0
    if (comp[i].use) {
      comp[i].dst_plane = img->get_component(c.component_index, &comp[i].dst_plane_stride);
      assert(comp[i].dst_plane != nullptr);
    }
    else {
      comp[i].dst_plane = nullptr;
      comp[i].dst_plane_stride = 0;
    }
#endif

    comp[i].dst_plane = img->get_component(m_uncC_index_to_comp_ids[i], &comp[i].dst_plane_stride);
  }

  const uint8_t* src = tile_data.data();
  const uint8_t* src_end = src + tile_data.size();

  for (uint32_t c = 0; c < num_components; c++) {
    uint32_t aligned_bytes_per_sample = comp[c].bytes_per_sample;
    if (components[c].component_align_size > 0) {
      skip_to_alignment(aligned_bytes_per_sample, components[c].component_align_size);
    }

    uint32_t bytes_per_row = aligned_bytes_per_sample * m_tile_width;
    skip_to_alignment(bytes_per_row, m_uncC->get_row_align_size());

    for (uint32_t tile_y = 0; tile_y < m_tile_height; tile_y++) {
      const uint8_t* row_start = src;

      for (uint32_t tile_x = 0; tile_x < m_tile_width; tile_x++) {
        if (src + aligned_bytes_per_sample > src_end) {
          return {heif_error_Invalid_input, heif_suberror_Unspecified,
                  "Bytealign-component interleave: insufficient data"};
        }

        if (comp[c].use) {
          uint32_t dst_x = out_x0 + tile_x;
          uint32_t dst_y = out_y0 + tile_y;
          uint64_t dst_offset = static_cast<uint64_t>(dst_y) * comp[c].dst_plane_stride
                                + static_cast<uint64_t>(dst_x) * comp[c].bytes_per_sample;
          uint8_t* dst = comp[c].dst_plane + dst_offset;

          if (comp[c].bytes_per_sample == 1) {
            *dst = src[0];
          }
          else if (comp[c].bytes_per_sample == 2) {
            uint16_t value;
            if (little_endian) {
              value = static_cast<uint16_t>(src[0] | (src[1] << 8));
            }
            else {
              value = static_cast<uint16_t>((src[0] << 8) | src[1]);
            }
            std::memcpy(dst, &value, 2);
          }
          else if (comp[c].bytes_per_sample == 4) {
            uint32_t value;
            if (little_endian) {
              value = static_cast<uint32_t>(src[0])
                      | (static_cast<uint32_t>(src[1]) << 8)
                      | (static_cast<uint32_t>(src[2]) << 16)
                      | (static_cast<uint32_t>(src[3]) << 24);
            }
            else {
              value = (static_cast<uint32_t>(src[0]) << 24)
                      | (static_cast<uint32_t>(src[1]) << 16)
                      | (static_cast<uint32_t>(src[2]) << 8)
                      | static_cast<uint32_t>(src[3]);
            }
            std::memcpy(dst, &value, 4);
          }
          else if (comp[c].bytes_per_sample == 8) {
            // 8-byte sample
            uint64_t value;
            if (little_endian) {
              value = static_cast<uint64_t>(src[0])
                      | (static_cast<uint64_t>(src[1]) << 8)
                      | (static_cast<uint64_t>(src[2]) << 16)
                      | (static_cast<uint64_t>(src[3]) << 24)
                      | (static_cast<uint64_t>(src[4]) << 32)
                      | (static_cast<uint64_t>(src[5]) << 40)
                      | (static_cast<uint64_t>(src[6]) << 48)
                      | (static_cast<uint64_t>(src[7]) << 56);
            }
            else {
              value = (static_cast<uint64_t>(src[0]) << 56)
                      | (static_cast<uint64_t>(src[1]) << 48)
                      | (static_cast<uint64_t>(src[2]) << 40)
                      | (static_cast<uint64_t>(src[3]) << 32)
                      | (static_cast<uint64_t>(src[4]) << 24)
                      | (static_cast<uint64_t>(src[5]) << 16)
                      | (static_cast<uint64_t>(src[6]) << 8)
                      | static_cast<uint64_t>(src[7]);
            }
            std::memcpy(dst, &value, 8);
          }
          else if (comp[c].bytes_per_sample == 16) {
            // 16-byte sample (2* 8 complex)
            uint64_t value[2];
            if (little_endian) {
              value[0] = static_cast<uint64_t>(src[0])
                         | (static_cast<uint64_t>(src[1]) << 8)
                         | (static_cast<uint64_t>(src[2]) << 16)
                         | (static_cast<uint64_t>(src[3]) << 24)
                         | (static_cast<uint64_t>(src[4]) << 32)
                         | (static_cast<uint64_t>(src[5]) << 40)
                         | (static_cast<uint64_t>(src[6]) << 48)
                         | (static_cast<uint64_t>(src[7]) << 56);
              value[1] = static_cast<uint64_t>(src[8])
                         | (static_cast<uint64_t>(src[9]) << 8)
                         | (static_cast<uint64_t>(src[10]) << 16)
                         | (static_cast<uint64_t>(src[11]) << 24)
                         | (static_cast<uint64_t>(src[12]) << 32)
                         | (static_cast<uint64_t>(src[13]) << 40)
                         | (static_cast<uint64_t>(src[14]) << 48)
                         | (static_cast<uint64_t>(src[15]) << 56);
            }
            else {
              value[0] = (static_cast<uint64_t>(src[0]) << 56)
                         | (static_cast<uint64_t>(src[1]) << 48)
                         | (static_cast<uint64_t>(src[2]) << 40)
                         | (static_cast<uint64_t>(src[3]) << 32)
                         | (static_cast<uint64_t>(src[4]) << 24)
                         | (static_cast<uint64_t>(src[5]) << 16)
                         | (static_cast<uint64_t>(src[6]) << 8)
                         | static_cast<uint64_t>(src[7]);
              value[1] = (static_cast<uint64_t>(src[8]) << 56)
                         | (static_cast<uint64_t>(src[9]) << 48)
                         | (static_cast<uint64_t>(src[10]) << 40)
                         | (static_cast<uint64_t>(src[11]) << 32)
                         | (static_cast<uint64_t>(src[12]) << 24)
                         | (static_cast<uint64_t>(src[13]) << 16)
                         | (static_cast<uint64_t>(src[14]) << 8)
                         | static_cast<uint64_t>(src[15]);
            }
            std::memcpy(dst, &value, 2*8);
          }
          else {
            assert(false);
          }
        }

        src += aligned_bytes_per_sample;
      }

      // Skip row alignment padding
      src = row_start + bytes_per_row;
    }
  }

  return Error::Ok;
}


// --- Factory ---

bool unc_decoder_factory_bytealign_component_interleave::can_decode(const std::shared_ptr<const Box_uncC>& uncC) const
{
  if (uncC->get_interleave_type() != interleave_mode_component) {
    return false;
  }

  if (uncC->get_block_size() != 0) {
    return false;
  }

  if (uncC->get_pixel_size() != 0) {
    return false;
  }

  if (uncC->get_sampling_type() != sampling_mode_no_subsampling) {
    return false;
  }

  for (const auto& component : uncC->get_components()) {
    uint32_t d = component.component_bit_depth;
    if (d != 8 && d != 16 && d != 32 && d != 64 && d != 128) {
      return false;
    }

    if (d == 128 && component.component_format != heif_uncompressed_component_format::component_format_complex) {
      return false;
    }
  }

  return true;
}


std::unique_ptr<unc_decoder> unc_decoder_factory_bytealign_component_interleave::create(
    uint32_t width, uint32_t height,
    const std::shared_ptr<const Box_cmpd>& cmpd,
    const std::shared_ptr<const Box_uncC>& uncC,
    const std::vector<uint32_t>& uncC_index_to_comp_ids) const
{
  return std::make_unique<unc_decoder_bytealign_component_interleave>(width, height, cmpd, uncC, uncC_index_to_comp_ids);
}
