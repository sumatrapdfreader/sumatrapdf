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

#include "jpeg2000_dec.h"
#include "jpeg2000_boxes.h"
#include "error.h"
#include "context.h"

#include <string>


Result<std::vector<uint8_t>> Decoder_JPEG2000::read_bitstream_configuration_data() const
{
  return std::vector<uint8_t>{};
}


int Decoder_JPEG2000::get_luma_bits_per_pixel() const
{
  Result<std::vector<uint8_t>> imageDataResult = get_compressed_data(true);
  if (!imageDataResult) {
    return -1;
  }

  JPEG2000MainHeader header;
  Error err = header.parseHeader(*imageDataResult);
  if (err) {
    return -1;
  }
  return header.get_precision(0);
}


int Decoder_JPEG2000::get_chroma_bits_per_pixel() const
{
  Result<std::vector<uint8_t>> imageDataResult = get_compressed_data(true);
  if (!imageDataResult) {
    return -1;
  }

  JPEG2000MainHeader header;
  Error err = header.parseHeader(*imageDataResult);
  if (err) {
    return -1;
  }
  return header.get_precision(1);
}


Error Decoder_JPEG2000::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
#if 0
  *out_chroma = (heif_chroma) (m_hvcC->get_configuration().chroma_format);

  if (*out_chroma == heif_chroma_monochrome) {
    *out_colorspace = heif_colorspace_monochrome;
  }
  else {
    *out_colorspace = heif_colorspace_YCbCr;
  }
#endif

  *out_colorspace = heif_colorspace_YCbCr;
  *out_chroma = heif_chroma_444;

  return Error::Ok;
}
