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
#include <algorithm>
#include <iostream>
#include <cassert>
#include <utility>

#if ((defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__PGI)) && __GNUC__ < 9) || (defined(__clang__) && __clang_major__ < 10)
#include <type_traits>
#else
#include <bit>
#endif

#include "common_utils.h"
#include "context.h"
#include "compression.h"
#include "error.h"
#include "libheif/heif.h"
#include "unc_types.h"
#include "unc_boxes.h"
#include "unc_codec.h"
#include "unc_decoder_legacybase.h"
#include "codecs/decoder.h"


unc_decoder_legacybase::unc_decoder_legacybase(uint32_t width, uint32_t height,
                                               const std::shared_ptr<const Box_cmpd>& cmpd,
                                               const std::shared_ptr<const Box_uncC>& uncC,
                                               const std::vector<uint32_t>& uncC_index_to_comp_ids)
    : unc_decoder(width, height, cmpd, uncC, uncC_index_to_comp_ids)
{
}

void unc_decoder_legacybase::ensureChannelList(std::shared_ptr<HeifPixelImage>& img)
{
  if (channelList.empty()) {
    buildChannelList(img);
  }
}

void unc_decoder_legacybase::buildChannelList(std::shared_ptr<HeifPixelImage>& img)
{
  uint32_t uncC_index = 0;
  for (Box_uncC::Component component : m_uncC->get_components()) {
    ChannelListEntry entry = buildChannelListEntry(uncC_index, component, img);
    channelList.push_back(entry);
    uncC_index++;
  }
}

void unc_decoder_legacybase::memcpy_to_native_endian(uint8_t* dst, uint32_t value, uint32_t bytes_per_sample)
{
  // TODO: this assumes that the file endianness is always big-endian. The endianness flags in the uncC header are not taken into account yet.

  if (bytes_per_sample==1) {
    *dst = static_cast<uint8_t>(value);
    return;
  }
  else if (std::endian::native == std::endian::big) {
    for (uint32_t i = 0; i < bytes_per_sample; i++) {
      dst[bytes_per_sample - 1 - i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
  }
  else {
    for (uint32_t i = 0; i < bytes_per_sample; i++) {
      dst[i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
  }
}

void unc_decoder_legacybase::processComponentSample(UncompressedBitReader& srcBits, const ChannelListEntry& entry, uint64_t dst_row_offset, uint32_t tile_column, uint32_t tile_x)
{
  uint64_t dst_col_number = static_cast<uint64_t>(tile_column) * entry.tile_width + tile_x;
  uint64_t dst_column_offset = dst_col_number * entry.bytes_per_component_sample;
  int val = srcBits.get_bits(entry.bits_per_component_sample); // get_bits() reads input in big-endian order
  memcpy_to_native_endian(entry.dst_plane + dst_row_offset + dst_column_offset, val, entry.bytes_per_component_sample);
}

// Handles the case where a row consists of a single component type
// Not valid for Pixel interleave
// Not valid for the Cb/Cr channels in Mixed Interleave
// Not valid for multi-Y pixel interleave
void unc_decoder_legacybase::processComponentRow(ChannelListEntry& entry, UncompressedBitReader& srcBits, uint64_t dst_row_offset, uint32_t tile_column)
{
  for (uint32_t tile_x = 0; tile_x < entry.tile_width; tile_x++) {
    if (entry.component_alignment != 0) {
      srcBits.skip_to_byte_boundary();
      int numPadBits = (entry.component_alignment * 8) - entry.bits_per_component_sample;
      srcBits.skip_bits(numPadBits);
    }
    processComponentSample(srcBits, entry, dst_row_offset, tile_column, tile_x);
  }
  srcBits.skip_to_byte_boundary();
}

void unc_decoder_legacybase::processComponentTileSample(UncompressedBitReader& srcBits, const ChannelListEntry& entry, uint64_t dst_offset, uint32_t tile_x)
{
  uint64_t dst_sample_offset = uint64_t{tile_x} * entry.bytes_per_component_sample;
  int val = srcBits.get_bits(entry.bits_per_component_sample);
  memcpy_to_native_endian(entry.dst_plane + dst_offset + dst_sample_offset, val, entry.bytes_per_component_sample);
}

// Handles the case where a row consists of a single component type
// Not valid for Pixel interleave
// Not valid for the Cb/Cr channels in Mixed Interleave
// Not valid for multi-Y pixel interleave
void unc_decoder_legacybase::processComponentTileRow(ChannelListEntry& entry, UncompressedBitReader& srcBits, uint64_t dst_offset)
{
  for (uint32_t tile_x = 0; tile_x < entry.tile_width; tile_x++) {
    if (entry.component_alignment != 0) {
      srcBits.skip_to_byte_boundary();
      int numPadBits = (entry.component_alignment * 8) - entry.bits_per_component_sample;
      srcBits.skip_bits(numPadBits);
    }
    processComponentTileSample(srcBits, entry, dst_offset, tile_x);
  }
  srcBits.skip_to_byte_boundary();
}


unc_decoder_legacybase::ChannelListEntry unc_decoder_legacybase::buildChannelListEntry(uint32_t component_id,
                                                                                        Box_uncC::Component component,
                                                                                        std::shared_ptr<HeifPixelImage>& img)
{
  ChannelListEntry entry;
  entry.use_channel = map_uncompressed_component_to_channel(m_cmpd, component, &(entry.channel));
  entry.dst_plane = img->get_component(m_uncC_index_to_comp_ids[component_id], &(entry.dst_plane_stride));
  entry.tile_width = m_tile_width;
  entry.tile_height = m_tile_height;
  entry.other_chroma_dst_plane_stride = 0; // will be overwritten below if used
  if ((entry.channel == heif_channel_Cb) || (entry.channel == heif_channel_Cr)) {
    if (m_uncC->get_sampling_type() == sampling_mode_422) {
      entry.tile_width /= 2;
    }
    else if (m_uncC->get_sampling_type() == sampling_mode_420) {
      entry.tile_width /= 2;
      entry.tile_height /= 2;
    }
    if (entry.channel == heif_channel_Cb) {
      entry.other_chroma_dst_plane = img->get_channel_memory(heif_channel_Cr, &(entry.other_chroma_dst_plane_stride));
    }
    else if (entry.channel == heif_channel_Cr) {
      entry.other_chroma_dst_plane = img->get_channel_memory(heif_channel_Cb, &(entry.other_chroma_dst_plane_stride));
    }
  }
  entry.bits_per_component_sample = component.component_bit_depth;
  entry.component_alignment = component.component_align_size;
  entry.bytes_per_component_sample = (component.component_bit_depth + 7) / 8;
  entry.bytes_per_tile_row_src = entry.tile_width * entry.bytes_per_component_sample;
  return entry;
}



