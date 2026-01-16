/*
 * HEIF VVC codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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

#include "vvc.h"
#include "codecs/vvc_dec.h"
#include "codecs/vvc_enc.h"
#include "codecs/vvc_boxes.h"
#include <cstring>
#include <string>
#include <cassert>
#include "api_structs.h"
#include <utility>


ImageItem_VVC::ImageItem_VVC(HeifContext* ctx, heif_item_id id) : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_VVC>();
}

ImageItem_VVC::ImageItem_VVC(HeifContext* ctx) : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_VVC>();
}


Result<std::vector<uint8_t>> ImageItem_VVC::read_bitstream_configuration_data() const
{
  // --- get codec configuration

  std::shared_ptr<Box_vvcC> vvcC_box = get_property<Box_vvcC>();
  if (!vvcC_box)
  {
    assert(false);
    return Error(heif_error_Invalid_input,
                 heif_suberror_No_vvcC_box);
  }

  std::vector<uint8_t> data;
  if (!vvcC_box->get_headers(&data))
  {
    return Error(heif_error_Invalid_input,
                 heif_suberror_No_item_data);
  }

  return data;
}


Result<std::shared_ptr<Decoder>> ImageItem_VVC::get_decoder() const
{
  return {m_decoder};
}


std::shared_ptr<class Encoder> ImageItem_VVC::get_encoder() const
{
  return m_encoder;
}


Error ImageItem_VVC::initialize_decoder()
{
  auto vvcC_box = get_property<Box_vvcC>();
  if (!vvcC_box) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_av1C_box};
  }

  m_decoder = std::make_shared<Decoder_VVC>(vvcC_box);

  return Error::Ok;
}

void ImageItem_VVC::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}

heif_brand2 ImageItem_VVC::get_compatible_brand() const
{
  return heif_brand2_vvic;
}
