/*
 * HEIF JPEG 2000 codec.
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
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

#include "jpeg2000.h"
#include "api_structs.h"
#include "codecs/jpeg2000_dec.h"
#include "codecs/jpeg2000_enc.h"
#include "codecs/jpeg2000_boxes.h"
#include <cstdint>
#include <utility>


ImageItem_JPEG2000::ImageItem_JPEG2000(HeifContext* ctx, heif_item_id id)
    : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_JPEG2000>();
}


ImageItem_JPEG2000::ImageItem_JPEG2000(HeifContext* ctx)
    : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_JPEG2000>();
}


Result<std::vector<uint8_t>> ImageItem_JPEG2000::read_bitstream_configuration_data() const
{
  // --- get codec configuration

  std::shared_ptr<Box_j2kH> j2kH_box = get_property<Box_j2kH>();
  if (!j2kH_box)
  {
    // TODO - Correctly Find the j2kH box
    //  return Error(heif_error_Invalid_input,
    //               heif_suberror_Unspecified);
  }
  // else if (!j2kH_box->get_headers(data)) {
  //   return Error(heif_error_Invalid_input,
  //                heif_suberror_No_item_data);
  // }

  return std::vector<uint8_t>{};
}


Result<std::shared_ptr<Decoder>> ImageItem_JPEG2000::get_decoder() const
{
  return {m_decoder};
}


std::shared_ptr<class Encoder> ImageItem_JPEG2000::get_encoder() const
{
  return m_encoder;
}


Error ImageItem_JPEG2000::initialize_decoder()
{
  auto j2kH = get_property<Box_j2kH>();
  if (!j2kH) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "No j2kH box found."};
  }

  m_decoder = std::make_shared<Decoder_JPEG2000>(j2kH);

  return Error::Ok;
}


void ImageItem_JPEG2000::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}

heif_brand2 ImageItem_JPEG2000::get_compatible_brand() const
{
  return heif_brand2_j2ki;
}
