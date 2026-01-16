/*
 * HEIF AVC codec.
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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

#include "avc.h"
#include "context.h"
#include "codecs/avc_dec.h"
#include "codecs/avc_boxes.h"
#include <utility>

#include "codecs/avc_enc.h"


ImageItem_AVC::ImageItem_AVC(HeifContext* ctx, heif_item_id id)
  : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_AVC>();
}


ImageItem_AVC::ImageItem_AVC(HeifContext* ctx)
  : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_AVC>();
}


Result<std::shared_ptr<Decoder>> ImageItem_AVC::get_decoder() const
{
  return {m_decoder};
}


std::shared_ptr<Encoder> ImageItem_AVC::get_encoder() const
{
  return m_encoder;
}


Error ImageItem_AVC::initialize_decoder()
{
  auto avcC_box = get_property<Box_avcC>();
  if (!avcC_box) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_av1C_box};
  }

  m_decoder = std::make_shared<Decoder_AVC>(avcC_box);

  return Error::Ok;
}

void ImageItem_AVC::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}
