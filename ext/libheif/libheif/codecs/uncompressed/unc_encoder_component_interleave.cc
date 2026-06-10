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

#include "unc_encoder_component_interleave.h"

#include <bit>
#include <cstring>

#include "image/pixelimage.h"
#include "unc_boxes.h"


bool unc_encoder_factory_component_interleave::can_encode(const std::shared_ptr<const HeifPixelImage>& image,
                                                          const heif_encoding_options& options) const
{
  if (image->has_channel(heif_channel_interleaved)) {
    return false;
  }

  // Only consider components with an actual data plane. cpat reference
  // components have bpp=0 and no buffer; they're included in
  // get_used_component_ids() but do not affect what the encoder writes.
  // (Matches what the encoder constructor below uses to build m_components.)
  auto component_ids = image->get_used_planar_component_ids();

  // If any component is not byte-aligned, we use the bit-packing path which
  // reads samples as uint32_t, limiting all components to 32 bpp.
  bool any_non_aligned = false;
  for (uint32_t id : component_ids) {
    uint16_t bpp = image->get_component_bits_per_pixel(id);
    if (bpp % 8 != 0) {
      any_non_aligned = true;
    }
  }

  if (any_non_aligned) {
    for (uint32_t id : component_ids) {
      if (image->get_component_bits_per_pixel(id) > 32) {
        return false;
      }
    }
  }

  if (!any_non_aligned) {
    // All components are byte-aligned. Only accept typical integer widths.
    for (uint32_t id : component_ids) {
      uint16_t bpp = image->get_component_bits_per_pixel(id);
      switch (bpp) {
        case 8:
        case 16:
        case 32:
        case 64:
        case 128:
        case 256:
          break;
        default:
          return false;
      }
    }
  }

  return true;
}


std::unique_ptr<const unc_encoder> unc_encoder_factory_component_interleave::create(const std::shared_ptr<const HeifPixelImage>& image,
                                                                                    const heif_encoding_options& options) const
{
  return std::make_unique<unc_encoder_component_interleave>(image, options);
}


unc_encoder_component_interleave::unc_encoder_component_interleave(const std::shared_ptr<const HeifPixelImage>& image,
                                                                   const heif_encoding_options& options)
    : unc_encoder(image)
{
  bool is_custom = (image->get_colorspace() == heif_colorspace_custom);
  //uint32_t num_components = image->get_number_of_used_components();

  auto componentIds = image->get_used_planar_component_ids();

  for (uint32_t id : componentIds) {
    heif_cmpd_component_type comp_type;
    heif_channel ch = heif_channel_Y; // default for nonvisual

    if (is_custom) {
      comp_type = static_cast<heif_cmpd_component_type>(image->get_component_type(id));
    }
    else {
      ch = image->get_component_channel(id);
      if (ch == heif_channel_Y && !image->has_channel(heif_channel_Cb)) {
        comp_type = heif_cmpd_component_type_monochrome;
      }
      else {
        comp_type = heif_channel_to_component_type(ch);
      }
    }

    uint16_t bpp = image->get_component_bits_per_pixel(id);
    auto comp_format = to_unc_component_format(image->get_component_datatype(id));
    bool aligned = (bpp % 8 == 0);

    m_components.push_back({id, ch, comp_type, comp_format, bpp, aligned});
  }

  // Build cmpd/uncC boxes
  m_use_memcpy = true;

  for (const auto& comp : m_components) {
    if (!comp.byte_aligned) {
      m_use_memcpy = false;
      break;
    }
  }

  for (const auto& comp : m_components) {
    m_uncC->add_component({m_map_id_to_cmpd_index[comp.component_id], comp.bpp, comp.component_format, 0});
  }

  m_uncC->set_interleave_type(interleave_mode_component);
  if (m_use_memcpy) {
    m_uncC->set_components_little_endian(std::endian::native == std::endian::little);
  }
  else {
    // we use dense packing
    m_uncC->set_components_little_endian(false);
  }
  m_uncC->set_block_size(0);

  if (image->get_chroma_format() == heif_chroma_420) {
    m_uncC->set_sampling_type(sampling_mode_420);
  }
  else if (image->get_chroma_format() == heif_chroma_422) {
    m_uncC->set_sampling_type(sampling_mode_422);
  }
  else {
    m_uncC->set_sampling_type(sampling_mode_no_subsampling);
  }
}


uint64_t unc_encoder_component_interleave::compute_tile_data_size_bytes(uint32_t tile_width, uint32_t tile_height) const
{
  uint64_t total = 0;
  for (const auto& comp : m_components) {
    uint32_t plane_width = tile_width;
    uint32_t plane_height = tile_height;

    if (comp.channel == heif_channel_Cb || comp.channel == heif_channel_Cr) {
      // Adjust for chroma subsampling
      if (m_uncC->get_sampling_type() == sampling_mode_420) {
        plane_width = (plane_width + 1) / 2;
        plane_height = (plane_height + 1) / 2;
      }
      else if (m_uncC->get_sampling_type() == sampling_mode_422) {
        plane_width = (plane_width + 1) / 2;
      }
    }

    uint64_t row_bytes;
    if (comp.byte_aligned) {
      row_bytes = static_cast<uint64_t>(plane_width) * ((comp.bpp + 7) / 8);
    }
    else {
      row_bytes = (static_cast<uint64_t>(plane_width) * comp.bpp + 7) / 8;
    }
    total += row_bytes * plane_height;
  }
  return total;
}


std::vector<uint8_t> unc_encoder_component_interleave::encode_tile(const std::shared_ptr<const HeifPixelImage>& src_image) const
{
  uint64_t total_size = compute_tile_data_size_bytes(src_image->get_width(), src_image->get_height());
  std::vector<uint8_t> data;
  data.resize(total_size);

  uint64_t out_pos = 0;

  for (const auto& comp : m_components) {
    uint32_t plane_width = src_image->get_component_width(comp.component_id);
    uint32_t plane_height = src_image->get_component_height(comp.component_id);
    uint16_t bpp = comp.bpp;

    size_t src_stride;
    const uint8_t* src_data = src_image->get_component(comp.component_id, &src_stride);

    if (m_use_memcpy) {
      assert(comp.byte_aligned);

      // Byte-aligned path: memcpy per row
      int bytes_per_pixel = (bpp + 7) / 8;

      for (uint32_t y = 0; y < plane_height; y++) {
        memcpy(data.data() + out_pos,
               src_data + src_stride * y,
               plane_width * bytes_per_pixel);
        out_pos += plane_width * bytes_per_pixel;
      }
    }
    else {
      // Bit-packed path: bit accumulator with row-end flush
      for (uint32_t y = 0; y < plane_height; y++) {
        const uint8_t* row = src_data + src_stride * y;

        uint64_t accumulator = 0;
        int accumulated_bits = 0;

        for (uint32_t x = 0; x < plane_width; x++) {
          uint32_t sample;

          if (bpp <= 8) {
            sample = row[x];
          }
          else if (bpp <= 16) {
            sample = reinterpret_cast<const uint16_t*>(row)[x];
          }
          else {
            sample = reinterpret_cast<const uint32_t*>(row)[x];
          }

          accumulator = (accumulator << bpp) | sample;
          accumulated_bits += bpp;

          while (accumulated_bits >= 8) {
            accumulated_bits -= 8;
            data[out_pos++] = static_cast<uint8_t>(accumulator >> accumulated_bits);
            accumulator &= (uint64_t{1} << accumulated_bits) - 1;
          }
        }

        // Flush partial byte at row end (pad with zeros in LSBs)
        if (accumulated_bits > 0) {
          data[out_pos++] = static_cast<uint8_t>(accumulator << (8 - accumulated_bits));
        }
      }
    }
  }

  return data;
}
