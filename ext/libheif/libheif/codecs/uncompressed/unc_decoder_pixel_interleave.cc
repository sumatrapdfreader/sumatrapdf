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

#include "unc_decoder_pixel_interleave.h"
#include "context.h"
#include "error.h"

#include <vector>


Result<std::vector<uint64_t>> unc_decoder_pixel_interleave::get_tile_data_sizes() const
{
  uint32_t bits_per_row = 0;
  for (uint32_t x = 0; x < m_tile_width; x++) {
    uint32_t bits_per_pixel = 0;

    for (const ChannelListEntry& entry : channelList) {
      uint32_t bits_per_component = entry.bits_per_component_sample;
      if (entry.component_alignment > 0) {
        // start at byte boundary
        bits_per_row = (bits_per_row + 7) & ~7U;

        uint32_t bytes_per_component = (bits_per_component + 7) / 8;
        skip_to_alignment(bytes_per_component, entry.component_alignment);
        bits_per_component = bytes_per_component * 8;
      }

      bits_per_pixel += bits_per_component;
    }

    if (m_uncC->get_pixel_size() != 0) {
      uint32_t bytes_per_pixel = (bits_per_pixel + 7) / 8;
      skip_to_alignment(bytes_per_pixel, m_uncC->get_pixel_size());
      bits_per_pixel = bytes_per_pixel * 8;
    }

    if (bits_per_pixel > UINT32_MAX - bits_per_row) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                   "uncompressed tile row size exceeds 32-bit range"};
    }
    bits_per_row += bits_per_pixel;
  }

  uint32_t bytes_per_row = (bits_per_row + 7) / 8;
  skip_to_alignment(bytes_per_row, m_uncC->get_row_align_size());

  uint64_t total_tile_size = bytes_per_row * static_cast<uint64_t>(m_tile_height);
  if (m_uncC->get_tile_align_size() != 0) {
    skip_to_alignment(total_tile_size, m_uncC->get_tile_align_size());
  }

  return std::vector<uint64_t>{total_tile_size};
}


Error unc_decoder_pixel_interleave::decode_tile(const std::vector<uint8_t>& tile_data,
                                                 std::shared_ptr<HeifPixelImage>& img,
                                                 uint32_t out_x0, uint32_t out_y0)
{
  UncompressedBitReader srcBits(tile_data);

  return processTile(srcBits, out_x0, out_y0);
}


Error unc_decoder_pixel_interleave::processTile(UncompressedBitReader& srcBits, uint32_t out_x0, uint32_t out_y0)
{
  for (uint32_t tile_y = 0; tile_y < m_tile_height; tile_y++) {
    srcBits.markRowStart();
    for (uint32_t tile_x = 0; tile_x < m_tile_width; tile_x++) {
      srcBits.markPixelStart();
      for (ChannelListEntry& entry : channelList) {
        if (entry.use_channel) {
          uint64_t dst_row_offset = entry.getDestinationRowOffset(0, tile_y + out_y0);
          if (entry.component_alignment != 0) {
            srcBits.skip_to_byte_boundary();
            int numPadBits = (entry.component_alignment * 8) - entry.bits_per_component_sample;
            srcBits.skip_bits(numPadBits);
          }
          processComponentSample(srcBits, entry, dst_row_offset, 0, out_x0 + tile_x);
        }
        else {
          srcBits.skip_bytes(entry.bytes_per_component_sample);
        }
      }
      auto err = srcBits.handlePixelAlignment(m_uncC->get_pixel_size());
      if (err) {
        return err;
      }
    }

    srcBits.handleRowAlignment(m_uncC->get_row_align_size());
  }

  return {};
}


bool unc_decoder_factory_pixel_interleave::can_decode(const std::shared_ptr<const Box_uncC>& uncC) const
{
  if (!check_common_requirements(uncC)) {
    return false;
  }

  if (uncC->get_interleave_type() != interleave_mode_pixel) {
    return false;
  }

  if (uncC->get_sampling_type() != sampling_mode_no_subsampling) {
    return false;
  }

  if (uncC->is_components_little_endian()) {
    return false;
  }

  return true;
}

std::unique_ptr<unc_decoder> unc_decoder_factory_pixel_interleave::create(
    uint32_t width, uint32_t height,
    const std::shared_ptr<const Box_cmpd>& cmpd,
    const std::shared_ptr<const Box_uncC>& uncC,
    const std::vector<uint32_t>& uncC_index_to_comp_ids) const
{
  return std::make_unique<unc_decoder_pixel_interleave>(width, height, cmpd, uncC, uncC_index_to_comp_ids);
}
