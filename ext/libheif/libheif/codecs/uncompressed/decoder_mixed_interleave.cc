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

#include "decoder_mixed_interleave.h"
#include "context.h"
#include "error.h"

#include <cstring>
#include <cassert>
#include <vector>


Error MixedInterleaveDecoder::decode_tile(const DataExtent& dataExtent,
                                          const UncompressedImageCodec::unci_properties& properties,
                                          std::shared_ptr<HeifPixelImage>& img,
                                          uint32_t out_x0, uint32_t out_y0,
                                          uint32_t image_width, uint32_t image_height,
                                          uint32_t tile_x, uint32_t tile_y)
{
  if (m_tile_width == 0) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "Internal error: MixedInterleaveDecoder tile_width=0"};
  }

  // --- compute which file range we need to read for the tile

  uint64_t tile_size = 0;

  for (ChannelListEntry& entry : channelList) {
    if (entry.channel == heif_channel_Cb || entry.channel == heif_channel_Cr) {
      uint32_t bits_per_row = entry.bits_per_component_sample * entry.tile_width;
      bits_per_row = (bits_per_row + 7) & ~7U; // align to byte boundary

      tile_size += uint64_t{bits_per_row} / 8 * entry.tile_height;
    }
    else {
      uint32_t bits_per_component = entry.bits_per_component_sample;
      if (entry.component_alignment > 0) {
        uint32_t bytes_per_component = (bits_per_component + 7) / 8;
        skip_to_alignment(bytes_per_component, entry.component_alignment);
        bits_per_component = bytes_per_component * 8;
      }

      uint32_t bits_per_row = bits_per_component * entry.tile_width;
      bits_per_row = (bits_per_row + 7) & ~7U; // align to byte boundary

      tile_size += uint64_t{bits_per_row} / 8 * entry.tile_height;
    }
  }


  if (m_uncC->get_tile_align_size() != 0) {
    skip_to_alignment(tile_size, m_uncC->get_tile_align_size());
  }

  assert(m_tile_width > 0);
  uint32_t tileIdx = tile_x + tile_y * (image_width / m_tile_width);
  uint64_t tile_start_offset = tile_size * tileIdx;


  // --- read required file range

  std::vector<uint8_t> src_data;
  Error err = get_compressed_image_data_uncompressed(dataExtent, properties, &src_data, tile_start_offset, tile_size, tileIdx, nullptr);
  //Error err = context->get_heif_file()->append_data_from_iloc(image_id, src_data, tile_start_offset, tile_size);
  if (err) {
    return err;
  }

  UncompressedBitReader srcBits(src_data);

  processTile(srcBits, tile_y, tile_x, out_x0, out_y0);

  return Error::Ok;
}


void MixedInterleaveDecoder::processTile(UncompressedBitReader& srcBits, uint32_t tile_row, uint32_t tile_column, uint32_t out_x0, uint32_t out_y0)
{
  bool haveProcessedChromaForThisTile = false;
  for (ChannelListEntry& entry : channelList) {
    if (entry.use_channel) {
      if ((entry.channel == heif_channel_Cb) || (entry.channel == heif_channel_Cr)) {
        if (!haveProcessedChromaForThisTile) {
          for (uint32_t tile_y = 0; tile_y < entry.tile_height; tile_y++) {
            // TODO: row padding
            uint64_t dst_row_number = tile_y + out_y0;
            uint64_t dst_row_offset = dst_row_number * entry.dst_plane_stride;
            for (uint32_t tile_x = 0; tile_x < entry.tile_width; tile_x++) {
              uint64_t dst_column_number = out_x0 + tile_x;
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
          uint64_t dst_row_offset = entry.getDestinationRowOffset(tile_row, tile_y);
          processComponentRow(entry, srcBits, dst_row_offset, tile_column);
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
