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

#ifndef HEIF_AVIF_H
#define HEIF_AVIF_H

#include <cassert>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "libheif/heif.h"
#include "box.h"
#include "error.h"
#include "image_item.h"


class ImageItem_AVIF : public ImageItem
{
public:
  ImageItem_AVIF(HeifContext* ctx, heif_item_id id);

  ImageItem_AVIF(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("av01"); }

  const char* get_auxC_alpha_channel_type() const override { return "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"; }

  heif_compression_format get_compression_format() const override { return heif_compression_AV1; }

  heif_brand2 get_compatible_brand() const override { return heif_brand2_avif; }

  Error initialize_decoder() override;

  void set_decoder_input_data() override;

protected:
  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  std::shared_ptr<Encoder> get_encoder() const override;

private:
  std::shared_ptr<class Decoder_AVIF> m_decoder;

  std::shared_ptr<class Encoder_AVIF> m_encoder;
};

#endif
