/*
 * HEIF codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#include "avc_dec.h"
#include "avc_boxes.h"
#include "error.h"
#include "context.h"

#include <string>


Result<std::vector<uint8_t>> Decoder_AVC::read_bitstream_configuration_data() const
{
  std::vector<uint8_t> data;
  m_avcC->get_header_nals(data);

  return data;
}


int Decoder_AVC::get_luma_bits_per_pixel() const
{
  return m_avcC->get_configuration().bit_depth_luma;
}


int Decoder_AVC::get_chroma_bits_per_pixel() const
{
  return m_avcC->get_configuration().bit_depth_chroma;
}


Result<std::optional<ImageSize>> Decoder_AVC::get_coded_image_size_from_config() const
{
  const auto& sps_set = m_avcC->getSequenceParameterSets();

  for (const auto& sps : sps_set) {
    if (sps.empty()) continue;
    Box_avcC::configuration scratch = m_avcC->get_configuration();
    uint32_t cropped_w = 0, cropped_h = 0;
    ImageSize coded{};

    Error e = parse_sps_for_avcC_configuration(sps.data(), sps.size(), &scratch,
                                               &cropped_w, &cropped_h, &coded);
    if (e) {
      return e;
    }

    return std::optional<ImageSize>{coded};
  }

  return std::optional<ImageSize>{};
}


Error Decoder_AVC::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  *out_chroma = m_avcC->get_configuration().chroma_format;

  if (*out_chroma == heif_chroma_monochrome) {
    *out_colorspace = heif_colorspace_monochrome;
  }
  else {
    *out_colorspace = heif_colorspace_YCbCr;
  }

  return Error::Ok;
}
