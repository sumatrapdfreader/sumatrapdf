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

#include "unc_decoder_row_interleave.h"
#include "context.h"
#include "error.h"

#include <vector>


Result<std::vector<uint64_t>> unc_decoder_row_interleave::get_tile_data_sizes() const
{
  uint32_t bits_per_row = 0;
  for (const ChannelListEntry& entry : channelList) {
    uint32_t bits_per_component = entry.bits_per_component_sample;
    if (entry.component_alignment > 0) {
      // start at byte boundary
      bits_per_row = (bits_per_row + 7) & ~7U;

      uint32_t bytes_per_component = (bits_per_component + 7) / 8;
      skip_to_alignment(bytes_per_component, entry.component_alignment);
      bits_per_component = bytes_per_component * 8;
    }

    uint32_t row_bits;
    if (bits_per_component != 0 && m_tile_width > UINT32_MAX / bits_per_component) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                   "uncompressed tile row size exceeds 32-bit range"};
    }
    if (m_uncC->get_row_align_size() != 0) {
      uint32_t bytes_this_row = (bits_per_component * m_tile_width + 7) / 8;
      skip_to_alignment(bytes_this_row, m_uncC->get_row_align_size());
      if (bytes_this_row > UINT32_MAX / 8) {
        return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                     "uncompressed tile row size exceeds 32-bit range"};
      }
      row_bits = bytes_this_row * 8;
    }
    else {
      row_bits = bits_per_component * m_tile_width;
    }

    if (row_bits > UINT32_MAX - bits_per_row) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                   "uncompressed tile row size exceeds 32-bit range"};
    }
    bits_per_row += row_bits;

    if (bits_per_row > UINT32_MAX - 7) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                   "uncompressed tile row size exceeds 32-bit range"};
    }
    bits_per_row = (bits_per_row + 7) & ~7U;
  }

  uint32_t bytes_per_row = (bits_per_row + 7) / 8;
  if (m_uncC->get_row_align_size()) {
    skip_to_alignment(bytes_per_row, m_uncC->get_row_align_size());
  }

  uint64_t total_tile_size = bytes_per_row * static_cast<uint64_t>(m_tile_height);

  if (m_uncC->get_tile_align_size() != 0) {
    skip_to_alignment(total_tile_size, m_uncC->get_tile_align_size());
  }

  return std::vector<uint64_t>{total_tile_size};
}


Error unc_decoder_row_interleave::decode_tile(const std::vector<uint8_t>& tile_data,
                                               std::shared_ptr<HeifPixelImage>& img,
                                               uint32_t out_x0, uint32_t out_y0)
{
  UncompressedBitReader srcBits(tile_data);

  processTile(srcBits, out_x0, out_y0);

  return Error::Ok;
}


void unc_decoder_row_interleave::processTile(UncompressedBitReader& srcBits, uint32_t out_x0, uint32_t out_y0)
{
  for (uint32_t tile_y = 0; tile_y < m_tile_height; tile_y++) {
    for (ChannelListEntry& entry : channelList) {
      srcBits.markRowStart();
      if (entry.use_channel) {
        uint64_t dst_row_offset = entry.getDestinationRowOffset(0, tile_y + out_y0);
        processComponentRow(entry, srcBits, dst_row_offset + out_x0 * entry.bytes_per_component_sample, 0);
      }
      else {
        srcBits.skip_bytes(entry.bytes_per_tile_row_src);
      }
      srcBits.handleRowAlignment(m_uncC->get_row_align_size());
    }
  }
}


bool unc_decoder_factory_row_interleave::can_decode(const std::shared_ptr<const Box_uncC>& uncC) const
{
  if (!check_common_requirements(uncC)) {
    return false;
  }

  if (uncC->get_interleave_type() != interleave_mode_row) {
    return false;
  }

  if (uncC->get_sampling_type() != sampling_mode_no_subsampling) {
    return false;
  }

  if (uncC->get_pixel_size() != 0) {
    return false;
  }

  if (uncC->is_components_little_endian() && has_any_multi_byte_components(uncC)) {
    return false;
  }

  return true;
}

std::unique_ptr<unc_decoder> unc_decoder_factory_row_interleave::create(
    uint32_t width, uint32_t height,
    const std::shared_ptr<const Box_cmpd>& cmpd,
    const std::shared_ptr<const Box_uncC>& uncC,
    const std::vector<uint32_t>& uncC_index_to_comp_ids) const
{
  return std::make_unique<unc_decoder_row_interleave>(width, height, cmpd, uncC, uncC_index_to_comp_ids);
}
