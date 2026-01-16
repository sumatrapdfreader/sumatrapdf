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

#include "decoder_row_interleave.h"
#include "context.h"
#include "error.h"

#include <cassert>
#include <vector>


Error RowInterleaveDecoder::decode_tile(const DataExtent& dataExtent,
                                        const UncompressedImageCodec::unci_properties& properties,
                                        std::shared_ptr<HeifPixelImage>& img,
                                        uint32_t out_x0, uint32_t out_y0,
                                        uint32_t image_width, uint32_t image_height,
                                        uint32_t tile_x, uint32_t tile_y)
{
  if (m_tile_width == 0) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "Internal error: RowInterleaveDecoder tile_width=0"};
  }

  // --- compute which file range we need to read for the tile

  uint32_t bits_per_row = 0;
  for (ChannelListEntry& entry : channelList) {
    uint32_t bits_per_component = entry.bits_per_component_sample;
    if (entry.component_alignment > 0) {
      // start at byte boundary
      bits_per_row = (bits_per_row + 7) & ~7U;

      uint32_t bytes_per_component = (bits_per_component + 7) / 8;
      skip_to_alignment(bytes_per_component, entry.component_alignment);
      bits_per_component = bytes_per_component * 8;
    }

    if (m_uncC->get_row_align_size() != 0) {
      uint32_t bytes_this_row = (bits_per_component * m_tile_width + 7) / 8;
      skip_to_alignment(bytes_this_row, m_uncC->get_row_align_size());
      bits_per_row += bytes_this_row * 8;
    }
    else {
      bits_per_row += bits_per_component * m_tile_width;
    }

    bits_per_row = (bits_per_row + 7) & ~7U;
  }

  uint32_t bytes_per_row = (bits_per_row + 7) / 8;
  if (m_uncC->get_row_align_size()) {
    skip_to_alignment(bytes_per_row, m_uncC->get_row_align_size());
  }

  uint64_t total_tile_size = 0;
  total_tile_size += bytes_per_row * static_cast<uint64_t>(m_tile_height);

  if (m_uncC->get_tile_align_size() != 0) {
    skip_to_alignment(total_tile_size, m_uncC->get_tile_align_size());
  }

  assert(m_tile_width > 0);
  uint32_t tileIdx = tile_x + tile_y * (image_width / m_tile_width);
  uint64_t tile_start_offset = total_tile_size * tileIdx;


  // --- read required file range

  std::vector<uint8_t> src_data;
  Error err = get_compressed_image_data_uncompressed(dataExtent, properties, &src_data, tile_start_offset, total_tile_size, tileIdx, nullptr);
  //Error err = context->get_heif_file()->append_data_from_iloc(image_id, src_data, tile_start_offset, total_tile_size);
  if (err) {
    return err;
  }

  UncompressedBitReader srcBits(src_data);

  processTile(srcBits, tile_y, tile_x, out_x0, out_y0);

  return Error::Ok;
}


void RowInterleaveDecoder::processTile(UncompressedBitReader& srcBits, uint32_t tile_row, uint32_t tile_column, uint32_t out_x0, uint32_t out_y0)
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

