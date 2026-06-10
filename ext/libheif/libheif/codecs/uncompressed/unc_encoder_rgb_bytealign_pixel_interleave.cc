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

#include "unc_encoder_rgb_bytealign_pixel_interleave.h"

#include <cstring>

#include "image/pixelimage.h"
#include "unc_boxes.h"
#include "unc_types.h"


bool unc_encoder_factory_rgb_bytealign_pixel_interleave::can_encode(const std::shared_ptr<const HeifPixelImage>& image,
                                            const heif_encoding_options& options) const
{
  if (image->get_colorspace() != heif_colorspace_RGB) {
    return false;
  }

  switch (image->get_chroma_format()) {
    case heif_chroma_interleaved_RRGGBB_LE:
    case heif_chroma_interleaved_RRGGBB_BE:
    case heif_chroma_interleaved_RRGGBBAA_LE:
    case heif_chroma_interleaved_RRGGBBAA_BE:
      break;
    default:
      return false;
  }

  return true;
}


std::unique_ptr<const unc_encoder> unc_encoder_factory_rgb_bytealign_pixel_interleave::create(const std::shared_ptr<const HeifPixelImage>& image,
                                                                      const heif_encoding_options& options) const
{
  return std::make_unique<unc_encoder_rgb_bytealign_pixel_interleave>(image, options);
}


unc_encoder_rgb_bytealign_pixel_interleave::unc_encoder_rgb_bytealign_pixel_interleave(const std::shared_ptr<const HeifPixelImage>& image,
                                       const heif_encoding_options& options)
    : unc_encoder(image)
{
  auto cmpd_ids = image->get_component_ids_interleaved();

  bool save_alpha = image->has_alpha();

  m_bytes_per_pixel = save_alpha ? 8 : 6;
  assert(cmpd_ids.size() == m_bytes_per_pixel/2);

  bool little_endian = (image->get_chroma_format() == heif_chroma_interleaved_RRGGBB_LE ||
                        image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE);

  uint16_t bpp = image->get_bits_per_pixel(heif_channel_interleaved);

  uint8_t component_align_size = 2;
  if (bpp == 16) {
    component_align_size = 0;
  }

  // make sure that we always save as big-endian so that we can read it with out own reader (unc_decoder_pixel_interleave, which only supports big-endian)
  m_swap_endianess = little_endian;

  m_uncC->set_interleave_type(interleave_mode_pixel);
  m_uncC->set_sampling_type(sampling_mode_no_subsampling);
  m_uncC->set_components_little_endian(false); // little_endian);
  m_uncC->set_pixel_size(m_bytes_per_pixel);

  m_uncC->add_component({m_map_id_to_cmpd_index[cmpd_ids[0]], bpp, component_format_unsigned, component_align_size});
  m_uncC->add_component({m_map_id_to_cmpd_index[cmpd_ids[1]], bpp, component_format_unsigned, component_align_size});
  m_uncC->add_component({m_map_id_to_cmpd_index[cmpd_ids[2]], bpp, component_format_unsigned, component_align_size});
  if (save_alpha) {
    m_uncC->add_component({m_map_id_to_cmpd_index[cmpd_ids[3]], bpp, component_format_unsigned, component_align_size});
  }
}


uint64_t unc_encoder_rgb_bytealign_pixel_interleave::compute_tile_data_size_bytes(uint32_t tile_width, uint32_t tile_height) const
{
  return static_cast<uint64_t>(tile_width) * tile_height * m_bytes_per_pixel;
}


void *memcpy_swap16(uint8_t *dst, const uint8_t *src, size_t n)
{
  assert(n % 2 == 0);

  /* swap pairs */
  for (size_t i = 0; i + 1 < n; i += 2) {
    dst[i] = src[i + 1];
    dst[i + 1] = src[i];
  }

  return dst;
}


std::vector<uint8_t> unc_encoder_rgb_bytealign_pixel_interleave::encode_tile(const std::shared_ptr<const HeifPixelImage>& src_image) const
{
  std::vector<uint8_t> data;

  size_t src_stride;
  const uint8_t* src_data = src_image->get_channel_memory(heif_channel_interleaved, &src_stride);

  uint64_t out_size = static_cast<uint64_t>(src_image->get_height()) * src_image->get_width() * m_bytes_per_pixel;
  data.resize(out_size);

  for (uint32_t y = 0; y < src_image->get_height(); y++) {
    if (m_swap_endianess) {
      memcpy_swap16(data.data() + y * src_image->get_width() * m_bytes_per_pixel, src_data + src_stride * y, src_image->get_width() * m_bytes_per_pixel);
    }
    else {
      memcpy(data.data() + y * src_image->get_width() * m_bytes_per_pixel, src_data + src_stride * y, src_image->get_width() * m_bytes_per_pixel);
    }
  }

  return data;
}
