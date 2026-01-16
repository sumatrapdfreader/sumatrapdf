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
