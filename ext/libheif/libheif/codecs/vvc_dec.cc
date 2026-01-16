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

#include "vvc_dec.h"
#include "vvc_boxes.h"
#include "error.h"
#include "context.h"

#include <string>


Result<std::vector<uint8_t>> Decoder_VVC::read_bitstream_configuration_data() const
{
  std::vector<uint8_t> data;
  if (!m_vvcC->get_headers(&data)) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_item_data};
  }

  return data;
}


int Decoder_VVC::get_luma_bits_per_pixel() const
{
  const Box_vvcC::configuration& config = m_vvcC->get_configuration();
  if (config.ptl_present_flag) {
    return config.bit_depth_minus8 + 8;
  }
  else {
    return 8; // TODO: what shall we do if the bit-depth is unknown? Use PIXI?
  }
}


int Decoder_VVC::get_chroma_bits_per_pixel() const
{
  return get_luma_bits_per_pixel();
}


Error Decoder_VVC::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  *out_chroma = (heif_chroma) (m_vvcC->get_configuration().chroma_format_idc);

  if (*out_chroma == heif_chroma_monochrome) {
    *out_colorspace = heif_colorspace_monochrome;
  }
  else {
    *out_colorspace = heif_colorspace_YCbCr;
  }

  return Error::Ok;
}
