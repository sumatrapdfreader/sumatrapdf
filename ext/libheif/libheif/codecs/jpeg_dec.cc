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

#include "jpeg_dec.h"
#include "jpeg_boxes.h"
#include "error.h"
#include "context.h"

#include <string>
#include <algorithm>


Result<std::vector<uint8_t>> Decoder_JPEG::read_bitstream_configuration_data() const
{
  if (m_jpgC) {
    return m_jpgC->get_data();
  }
  else {
    return std::vector<uint8_t>{};
  }
}


// This checks whether a start code FFCx with nibble 'x' is a SOF marker.
// E.g. FFC0-FFC3 are, while FFC4 is not.
static bool isSOF[16] = {true, true, true, true, false, true, true, true,
                         false, true, true, true, false, true, true, true};

Error Decoder_JPEG::parse_SOF()
{
  if (m_config) {
    return Error::Ok;
  }

  // image data, usually from 'mdat'

  auto dataResult = get_compressed_data(true);
  if (!dataResult) {
    return dataResult.error();
  }

  const std::vector<uint8_t>& data = *dataResult;

  const Error error_invalidSOF{heif_error_Invalid_input,
                               heif_suberror_Unspecified,
                               "Invalid JPEG SOF header"};

  for (size_t i = 0; i + 1 < data.size(); i++) {
    if (data[i] == 0xFF && (data[i + 1] & 0xF0) == 0xC0 && isSOF[data[i + 1] & 0x0F]) {

      if (i + 9 >= data.size()) {
        return error_invalidSOF;
      }

      ConfigInfo info;
      info.sample_precision = data[i + 4];
      info.nComponents = data[i + 9];

      if (i + 11 + 3 * info.nComponents >= data.size()) {
        return error_invalidSOF;
      }

      for (int c = 0; c < std::min(info.nComponents, uint8_t(3)); c++) {
        int ss = data[i + 11 + 3 * c];
        info.h_sampling[c] = (ss >> 4) & 0xF;
        info.v_sampling[c] = ss & 0xF;
      }

      if (info.nComponents == 1) {
        info.chroma = heif_chroma_monochrome;
      }
      else if (info.nComponents != 3) {
        return error_invalidSOF;
      }
      else {
        if (info.h_sampling[1] != info.h_sampling[2] ||
            info.v_sampling[1] != info.v_sampling[2]) {
          return error_invalidSOF;
        }

        if (info.h_sampling[0] == 2 && info.v_sampling[0] == 2 &&
            info.h_sampling[1] == 1 && info.v_sampling[1] == 1) {
          info.chroma = heif_chroma_420;
        }
        else if (info.h_sampling[0] == 2 && info.v_sampling[0] == 1 &&
                 info.h_sampling[1] == 1 && info.v_sampling[1] == 1) {
          info.chroma = heif_chroma_422;
        }
        else if (info.h_sampling[0] == 1 && info.v_sampling[0] == 1 &&
                 info.h_sampling[1] == 1 && info.v_sampling[1] == 1) {
          info.chroma = heif_chroma_444;
        }
        else {
          return error_invalidSOF;
        }
      }

      m_config = info;

      return Error::Ok;
    }
  }

  return error_invalidSOF;
}


int Decoder_JPEG::get_luma_bits_per_pixel() const
{
  Error err = const_cast<Decoder_JPEG*>(this)->parse_SOF();
  if (err) {
    return -1;
  }
  else {
    return m_config->sample_precision;
  }
}


int Decoder_JPEG::get_chroma_bits_per_pixel() const
{
  return get_luma_bits_per_pixel();
}


Error Decoder_JPEG::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  Error err = const_cast<Decoder_JPEG*>(this)->parse_SOF();
  if (err) {
    return err;
  }

  *out_chroma = m_config->chroma;

  if (*out_chroma == heif_chroma_monochrome) {
    *out_colorspace = heif_colorspace_monochrome;
  }
  else {
    *out_colorspace = heif_colorspace_YCbCr;
  }

  return Error::Ok;
}
