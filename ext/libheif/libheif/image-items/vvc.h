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

#ifndef LIBHEIF_VVC_H
#define LIBHEIF_VVC_H

#include "box.h"
#include <string>
#include <vector>
#include "image_item.h"
#include <memory>


class ImageItem_VVC : public ImageItem
{
public:
  ImageItem_VVC(HeifContext* ctx, heif_item_id id);

  ImageItem_VVC(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("vvc1"); }

  const char* get_auxC_alpha_channel_type() const override { return "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"; }

  heif_compression_format get_compression_format() const override { return heif_compression_VVC; }

  Error initialize_decoder() override;

  void set_decoder_input_data() override;

  std::shared_ptr<Encoder> get_encoder() const override;

  heif_brand2 get_compatible_brand() const override;

protected:
  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

private:
  std::shared_ptr<class Decoder_VVC> m_decoder;
  std::shared_ptr<class Encoder_VVC> m_encoder;
};

#endif // LIBHEIF_VVC_H
