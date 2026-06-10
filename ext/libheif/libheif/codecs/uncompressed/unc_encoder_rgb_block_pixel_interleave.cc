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

#include "unc_encoder_rgb_block_pixel_interleave.h"

#include "image/pixelimage.h"
#include "unc_boxes.h"
#include "unc_types.h"


bool unc_encoder_factory_rgb_block_pixel_interleave::can_encode(const std::shared_ptr<const HeifPixelImage>& image,
                                                               const heif_encoding_options& options) const
{
  if (image->get_colorspace() != heif_colorspace_RGB) {
    return false;
  }

  switch (image->get_chroma_format()) {
    case heif_chroma_interleaved_RRGGBB_LE:
    case heif_chroma_interleaved_RRGGBB_BE:
      break;
    default:
      return false;
  }

  if (image->get_bits_per_pixel(heif_channel_interleaved) >= 14) {
    return false;
  }

  return true;
}


std::unique_ptr<const unc_encoder> unc_encoder_factory_rgb_block_pixel_interleave::create(const std::shared_ptr<const HeifPixelImage>& image,
                                                                                          const heif_encoding_options& options) const
{
  return std::make_unique<unc_encoder_rgb_block_pixel_interleave>(image, options);
}


unc_encoder_rgb_block_pixel_interleave::unc_encoder_rgb_block_pixel_interleave(const std::shared_ptr<const HeifPixelImage>& image,
                                                                               const heif_encoding_options& options)
    : unc_encoder(image)
{
  auto ids = image->get_component_ids_interleaved();
  assert(ids.size() == 3);

  uint16_t bpp = image->get_bits_per_pixel(heif_channel_interleaved);

  uint16_t nBits = 3 * bpp;
  assert(nBits <= 256-8);
  m_bytes_per_pixel = static_cast<uint8_t>((nBits + 7) / 8);

  m_uncC->set_interleave_type(interleave_mode_pixel);
  m_uncC->set_pixel_size(m_bytes_per_pixel);
  m_uncC->set_block_size(m_bytes_per_pixel);
  m_uncC->set_sampling_type(sampling_mode_no_subsampling);
  m_uncC->set_block_little_endian(false);

  m_uncC->add_component({m_map_id_to_cmpd_index.find(ids[0])->second, bpp, component_format_unsigned, 0});
  m_uncC->add_component({m_map_id_to_cmpd_index.find(ids[1])->second, bpp, component_format_unsigned, 0});
  m_uncC->add_component({m_map_id_to_cmpd_index.find(ids[2])->second, bpp, component_format_unsigned, 0});
}


uint64_t unc_encoder_rgb_block_pixel_interleave::compute_tile_data_size_bytes(uint32_t tile_width, uint32_t tile_height) const
{
  return static_cast<uint64_t>(tile_width) * tile_height * m_bytes_per_pixel;
}


std::vector<uint8_t> unc_encoder_rgb_block_pixel_interleave::encode_tile(const std::shared_ptr<const HeifPixelImage>& src_image) const
{
  std::vector<uint8_t> data;

  uint16_t bpp = src_image->get_bits_per_pixel(heif_channel_interleaved);

  size_t src_stride;
  const auto* src_data = reinterpret_cast<const uint16_t*>(src_image->get_channel_memory(heif_channel_interleaved, &src_stride));
  src_stride /= 2;

  uint64_t out_size = static_cast<uint64_t>(src_image->get_height()) * src_image->get_width() * m_bytes_per_pixel;
  data.resize(out_size);

  uint8_t* p = data.data();

  for (uint32_t y = 0; y < src_image->get_height(); y++) {
    for (uint32_t x = 0; x < src_image->get_width(); x++) {
      uint16_t r = src_data[src_stride * y + 3 * x + 0];
      uint16_t g = src_data[src_stride * y + 3 * x + 1];
      uint16_t b = src_data[src_stride * y + 3 * x + 2];

      uint64_t combined_pixel = (static_cast<uint64_t>(r) << (2 * bpp)) | (static_cast<uint64_t>(g) << bpp) | b;

      if (m_bytes_per_pixel > 4) {
        *p++ = static_cast<uint8_t>((combined_pixel >> 32) & 0xFF);
      }

      *p++ = static_cast<uint8_t>((combined_pixel >> 24) & 0xFF);
      *p++ = static_cast<uint8_t>((combined_pixel >> 16) & 0xFF);
      *p++ = static_cast<uint8_t>((combined_pixel >> 8) & 0xFF);
      *p++ = static_cast<uint8_t>(combined_pixel & 0xFF);
    }
  }

  return data;
}
