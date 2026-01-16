/*
 * HEIF JPEG codec.
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

#include "jpeg.h"
#include "codecs/jpeg_dec.h"
#include "codecs/jpeg_enc.h"
#include "codecs/jpeg_boxes.h"
#include "security_limits.h"
#include "pixelimage.h"
#include "api_structs.h"
#include <cstring>
#include <utility>


ImageItem_JPEG::ImageItem_JPEG(HeifContext* ctx, heif_item_id id)
    : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_JPEG>();
}

ImageItem_JPEG::ImageItem_JPEG(HeifContext* ctx)
    : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_JPEG>();
}


Result<std::vector<uint8_t>> ImageItem_JPEG::read_bitstream_configuration_data() const
{
  return m_decoder->read_bitstream_configuration_data();
}


Result<std::shared_ptr<Decoder>> ImageItem_JPEG::get_decoder() const
{
  return {m_decoder};
}


std::shared_ptr<Encoder> ImageItem_JPEG::get_encoder() const
{
  return m_encoder;
}


Error ImageItem_JPEG::initialize_decoder()
{
  // Note: jpgC box is optional. NULL is a valid value.
  auto jpgC_box = get_property<Box_jpgC>();

  m_decoder = std::make_shared<Decoder_JPEG>(jpgC_box);

  return Error::Ok;
}


void ImageItem_JPEG::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}


heif_brand2 ImageItem_JPEG::get_compatible_brand() const
{
  return heif_brand2_jpeg;
}
