/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#include "hevc.h"
#include "codecs/hevc_boxes.h"
#include "bitstream.h"
#include "error.h"
#include "file.h"
#include "codecs/hevc_dec.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <string>
#include <utility>
#include "api_structs.h"
#include "codecs/hevc_enc.h"


ImageItem_HEVC::ImageItem_HEVC(HeifContext* ctx, heif_item_id id)
    : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_HEVC>();
}


ImageItem_HEVC::ImageItem_HEVC(HeifContext* ctx)
    : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_HEVC>();
}


Error ImageItem_HEVC::initialize_decoder()
{
  auto hvcC_box = get_property<Box_hvcC>();
  if (!hvcC_box) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_hvcC_box};
  }

  m_decoder = std::make_shared<Decoder_HEVC>(hvcC_box);

  return Error::Ok;
}


void ImageItem_HEVC::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}


heif_brand2 ImageItem_HEVC::get_compatible_brand() const
{
  auto hvcC = get_property<Box_hvcC>();

  if (has_essential_property_other_than(std::set{fourcc("hvcC"),
                                                 fourcc("irot"),
                                                 fourcc("imir"),
                                                 fourcc("clap")})) {
    return 0;
  }

  const auto& config = hvcC->get_configuration();
  if (config.is_profile_compatibile(HEVCDecoderConfigurationRecord::Profile_Main) ||
      config.is_profile_compatibile(HEVCDecoderConfigurationRecord::Profile_MainStillPicture)) {
    return heif_brand2_heic;
  }

  if (config.is_profile_compatibile(HEVCDecoderConfigurationRecord::Profile_Main10) ||
      config.is_profile_compatibile(HEVCDecoderConfigurationRecord::Profile_RExt)) {
    return heif_brand2_heix;
  }

  // TODO: what brand should we use for this case?

  return heif_brand2_heix;
}


Result<std::vector<uint8_t>> ImageItem_HEVC::read_bitstream_configuration_data() const
{
  return m_decoder->read_bitstream_configuration_data();
}


Result<std::shared_ptr<Decoder>> ImageItem_HEVC::get_decoder() const
{
  return {m_decoder};
}


std::shared_ptr<Encoder> ImageItem_HEVC::get_encoder() const
{
  return m_encoder;
}


void ImageItem_HEVC::set_preencoded_hevc_image(const std::vector<uint8_t>& data)
{
  auto hvcC = std::make_shared<Box_hvcC>();


  // --- parse the h265 stream and set hvcC headers and compressed image data

  int state = 0;

  bool first = true;
  bool eof = false;

  int prev_start_code_start = -1; // init to an invalid value, will always be overwritten before use
  int start_code_start;
  int ptr = 0;

  for (;;) {
    bool dump_nal = false;

    uint8_t c = data[ptr++];

    if (state == 3) {
      state = 0;
    }

    if (c == 0 && state <= 1) {
      state++;
    }
    else if (c == 0) {
      // NOP
    }
    else if (c == 1 && state == 2) {
      start_code_start = ptr - 3;
      dump_nal = true;
      state = 3;
    }
    else {
      state = 0;
    }

    if (ptr == (int) data.size()) {
      start_code_start = (int) data.size();
      dump_nal = true;
      eof = true;
    }

    if (dump_nal) {
      if (first) {
        first = false;
      }
      else {
        std::vector<uint8_t> nal_data;
        size_t length = start_code_start - (prev_start_code_start + 3);

        nal_data.resize(length);

        assert(prev_start_code_start >= 0);
        memcpy(nal_data.data(), data.data() + prev_start_code_start + 3, length);

        int nal_type = (nal_data[0] >> 1);

        switch (nal_type) {
          case 0x20:
          case 0x21:
          case 0x22:
            hvcC->append_nal_data(nal_data);
            break;

          default: {
            std::vector<uint8_t> nal_data_with_size;
            nal_data_with_size.resize(nal_data.size() + 4);

            memcpy(nal_data_with_size.data() + 4, nal_data.data(), nal_data.size());
            nal_data_with_size[0] = ((nal_data.size() >> 24) & 0xFF);
            nal_data_with_size[1] = ((nal_data.size() >> 16) & 0xFF);
            nal_data_with_size[2] = ((nal_data.size() >> 8) & 0xFF);
            nal_data_with_size[3] = ((nal_data.size() >> 0) & 0xFF);

            get_file()->append_iloc_data(get_id(), nal_data_with_size, 0);
          }
            break;
        }
      }

      prev_start_code_start = start_code_start;
    }

    if (eof) {
      break;
    }
  }

  get_file()->add_property(get_id(), hvcC, true);
}
