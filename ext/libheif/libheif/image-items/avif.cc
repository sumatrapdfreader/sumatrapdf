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

#include "pixelimage.h"
#include "avif.h"
#include "codecs/avif_dec.h"
#include "codecs/avif_enc.h"
#include "codecs/avif_boxes.h"
#include "bitstream.h"
#include "common_utils.h"
#include "api_structs.h"
#include "file.h"
#include <iomanip>
#include <limits>
#include <string>
#include <cstring>
#include <utility>

// https://aomediacodec.github.io/av1-spec/av1-spec.pdf


ImageItem_AVIF::ImageItem_AVIF(HeifContext* ctx, heif_item_id id) : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_AVIF>();
}

ImageItem_AVIF::ImageItem_AVIF(HeifContext* ctx) : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_AVIF>();
}


Error ImageItem_AVIF::initialize_decoder()
{
  auto av1C_box = get_property<Box_av1C>();
  if (!av1C_box) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_No_av1C_box};
  }

  m_decoder = std::make_shared<Decoder_AVIF>(av1C_box);

  return Error::Ok;
}

void ImageItem_AVIF::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}


Result<std::vector<uint8_t>> ImageItem_AVIF::read_bitstream_configuration_data() const
{
  return m_decoder->read_bitstream_configuration_data();
}


Result<std::shared_ptr<Decoder>> ImageItem_AVIF::get_decoder() const
{
  return {m_decoder};
}


std::shared_ptr<Encoder> ImageItem_AVIF::get_encoder() const
{
  return m_encoder;
}
