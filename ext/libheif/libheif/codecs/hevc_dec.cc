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

#include "hevc_dec.h"
#include "hevc_boxes.h"
#include "error.h"
#include "context.h"
#include "plugins/nalu_utils.h"

#include <string>


Result<std::vector<uint8_t>> Decoder_HEVC::read_bitstream_configuration_data() const
{
  std::vector<uint8_t> data;
  if (!m_hvcC->get_header_nals(&data)) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_item_data};
  }

  return data;
}


int Decoder_HEVC::get_luma_bits_per_pixel() const
{
  return m_hvcC->get_configuration().bit_depth_luma;
}


int Decoder_HEVC::get_chroma_bits_per_pixel() const
{
  return m_hvcC->get_configuration().bit_depth_chroma;
}


Result<std::optional<ImageSize>> Decoder_HEVC::get_coded_image_size_from_config() const
{
  const auto& nal_arrays = m_hvcC->get_configuration().m_nal_array;

  for (const auto& arr : nal_arrays) {
    if (arr.m_NAL_unit_type != HEVC_NAL_UNIT_SPS_NUT || arr.m_nal_units.empty()) {
      continue;
    }

    const std::vector<uint8_t>& sps = arr.m_nal_units[0];
    HEVCDecoderConfigurationRecord scratch = m_hvcC->get_configuration();
    uint32_t cropped_w = 0, cropped_h = 0;
    ImageSize coded{};
    Error e = parse_sps_for_hvcC_configuration(sps.data(), sps.size(), &scratch,
                                               &cropped_w, &cropped_h, &coded);
    if (e) {
      return e;
    }

    return std::optional<ImageSize>{coded};
  }

  return std::optional<ImageSize>{};
}


Error Decoder_HEVC::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  *out_chroma = (heif_chroma) (m_hvcC->get_configuration().chroma_format);

  if (*out_chroma == heif_chroma_monochrome) {
    *out_colorspace = heif_colorspace_monochrome;
  }
  else {
    *out_colorspace = heif_colorspace_YCbCr;
  }

  return Error::Ok;
}
