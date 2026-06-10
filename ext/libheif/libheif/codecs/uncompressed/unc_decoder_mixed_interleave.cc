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

#include "unc_decoder_mixed_interleave.h"
#include "context.h"
#include "error.h"

#include <cstring>
#include <vector>


Result<std::vector<uint64_t>> unc_decoder_mixed_interleave::get_tile_data_sizes() const
{
  uint64_t tile_size = 0;

  for (const ChannelListEntry& entry : channelList) {
    uint32_t bits_per_component = entry.bits_per_component_sample;
    if (entry.channel != heif_channel_Cb && entry.channel != heif_channel_Cr
        && entry.component_alignment > 0) {
      uint32_t bytes_per_component = (bits_per_component + 7) / 8;
      skip_to_alignment(bytes_per_component, entry.component_alignment);
      bits_per_component = bytes_per_component * 8;
    }

    if (bits_per_component != 0 && entry.tile_width > UINT32_MAX / bits_per_component) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_image_size,
                   "uncompressed tile row size exceeds 32-bit range"};
    }
    uint32_t bits_per_row = bits_per_component * entry.tile_width;
    bits_per_row = (bits_per_row + 7) & ~7U; // align to byte boundary

    tile_size += uint64_t{bits_per_row} / 8 * entry.tile_height;
  }

  if (m_uncC->get_tile_align_size() != 0) {
    skip_to_alignment(tile_size, m_uncC->get_tile_align_size());
  }

  return std::vector<uint64_t>{tile_size};
}


Error unc_decoder_mixed_interleave::decode_tile(const std::vector<uint8_t>& tile_data,
                                                 std::shared_ptr<HeifPixelImage>& img,
                                                 uint32_t out_x0, uint32_t out_y0)
{
  UncompressedBitReader srcBits(tile_data);

  processTile(srcBits, out_x0, out_y0);

  return Error::Ok;
}


void unc_decoder_mixed_interleave::processTile(UncompressedBitReader& srcBits, uint32_t out_x0, uint32_t out_y0)
{
  // out_x0/out_y0 are in full-resolution image coordinates. For subsampled chroma
  // channels (4:2:0 or 4:2:2), entry.tile_width/tile_height and entry.dst_plane_stride
  // refer to the subsampled chroma plane, so the destination origin must be scaled
  // to each channel's grid. Failing to do so wrote past the chroma plane on any
  // non-first tile row/column — see GHSA-5x55-x5pf-9c6g.
  uint32_t tile_col = out_x0 / m_tile_width;
  uint32_t tile_row = out_y0 / m_tile_height;

  bool haveProcessedChromaForThisTile = false;
  for (ChannelListEntry& entry : channelList) {
    if (entry.use_channel) {
      uint64_t channel_x0 = uint64_t{tile_col} * entry.tile_width;
      uint64_t channel_y0 = uint64_t{tile_row} * entry.tile_height;
      if ((entry.channel == heif_channel_Cb) || (entry.channel == heif_channel_Cr)) {
        if (!haveProcessedChromaForThisTile) {
          for (uint32_t tile_y = 0; tile_y < entry.tile_height; tile_y++) {
            // TODO: row padding
            uint64_t dst_row_number = tile_y + channel_y0;
            uint64_t dst_row_offset = dst_row_number * entry.dst_plane_stride;
            for (uint32_t tile_x = 0; tile_x < entry.tile_width; tile_x++) {
              uint64_t dst_column_number = channel_x0 + tile_x;
              uint64_t dst_column_offset = dst_column_number * entry.bytes_per_component_sample;
              int val = srcBits.get_bits(entry.bytes_per_component_sample * 8);
              memcpy_to_native_endian(entry.dst_plane + dst_row_offset + dst_column_offset, val, entry.bytes_per_component_sample);
              val = srcBits.get_bits(entry.bytes_per_component_sample * 8);

              uint64_t other_dst_row_offset = dst_row_number * entry.other_chroma_dst_plane_stride;
              memcpy_to_native_endian(entry.other_chroma_dst_plane + other_dst_row_offset + dst_column_offset, val, entry.bytes_per_component_sample);
            }
            haveProcessedChromaForThisTile = true;
          }
        }
      }
      else {
        for (uint32_t tile_y = 0; tile_y < entry.tile_height; tile_y++) {
          uint64_t dst_row_offset = (channel_y0 + tile_y) * entry.dst_plane_stride;
          processComponentTileRow(entry, srcBits, dst_row_offset + channel_x0 * entry.bytes_per_component_sample);
        }
      }
    }
    else {
      // skip over the data we are not using
      srcBits.skip_bytes(entry.get_bytes_per_tile());
      continue;
    }
  }
}


bool unc_decoder_factory_mixed_interleave::can_decode(const std::shared_ptr<const Box_uncC>& uncC) const
{
  if (!check_common_requirements(uncC)) {
    return false;
  }

  if (uncC->get_interleave_type() != interleave_mode_mixed) {
    return false;
  }

  auto sampling = uncC->get_sampling_type();
  if (sampling != sampling_mode_422 && sampling != sampling_mode_420) {
    return false;
  }

  if (sampling == sampling_mode_422) {
    if (uncC->get_tile_align_size() != 0 && uncC->get_tile_align_size() % 2 != 0) {
      return false;
    }
  }

  if (sampling == sampling_mode_420) {
    if (uncC->get_tile_align_size() != 0 && uncC->get_tile_align_size() % 4 != 0) {
      return false;
    }
  }

  if (uncC->get_pixel_size() != 0) {
    return false;
  }

  if (uncC->is_components_little_endian() && has_any_multi_byte_components(uncC)) {
    return false;
  }

  return true;
}

std::unique_ptr<unc_decoder> unc_decoder_factory_mixed_interleave::create(
    uint32_t width, uint32_t height,
    const std::shared_ptr<const Box_cmpd>& cmpd,
    const std::shared_ptr<const Box_uncC>& uncC,
    const std::vector<uint32_t>& uncC_index_to_comp_ids) const
{
  return std::make_unique<unc_decoder_mixed_interleave>(width, height, cmpd, uncC, uncC_index_to_comp_ids);
}
