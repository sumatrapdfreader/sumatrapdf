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

#ifndef LIBHEIF_JPEG2000_H
#define LIBHEIF_JPEG2000_H

#include "box.h"
#include "file.h"
#include "context.h"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>


class ImageItem_JPEG2000 : public ImageItem
{
public:
  ImageItem_JPEG2000(HeifContext* ctx, heif_item_id id);

  ImageItem_JPEG2000(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("j2k1"); }

  heif_compression_format get_compression_format() const override { return heif_compression_JPEG2000; }

  heif_brand2 get_compatible_brand() const override;

protected:
  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  std::shared_ptr<Encoder> get_encoder() const override;

public:
  Error initialize_decoder() override;

  void set_decoder_input_data() override;

private:
  std::shared_ptr<class Decoder_JPEG2000> m_decoder;
  std::shared_ptr<class Encoder_JPEG2000> m_encoder;
};

#endif // LIBHEIF_JPEG2000_H
