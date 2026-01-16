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

#ifndef LIBHEIF_JPEG_H
#define LIBHEIF_JPEG_H

#include "box.h"
#include <string>
#include <vector>
#include "image_item.h"
#include <memory>


class ImageItem_JPEG : public ImageItem
{
public:
  ImageItem_JPEG(HeifContext* ctx, heif_item_id id);

  ImageItem_JPEG(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("jpeg"); }

  heif_compression_format get_compression_format() const override { return heif_compression_JPEG; }

  Error initialize_decoder() override;

  void set_decoder_input_data() override;

  heif_brand2 get_compatible_brand() const override;

protected:
  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  std::shared_ptr<Encoder> get_encoder() const override;

  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

private:
  std::shared_ptr<class Decoder_JPEG> m_decoder;
  std::shared_ptr<class Encoder_JPEG> m_encoder;
};

#endif // LIBHEIF_JPEG_H
