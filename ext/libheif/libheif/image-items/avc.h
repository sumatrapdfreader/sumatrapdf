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

#ifndef HEIF_AVC_H
#define HEIF_AVC_H

#include "box.h"
#include "error.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "image_item.h"


class ImageItem_AVC : public ImageItem
{
public:
  ImageItem_AVC(HeifContext* ctx, heif_item_id id);

  ImageItem_AVC(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("avc1"); }

  const char* get_auxC_alpha_channel_type() const override { return "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"; }

  heif_compression_format get_compression_format() const override { return heif_compression_AVC; }

  Error initialize_decoder() override;

  void set_decoder_input_data() override;

  heif_brand2 get_compatible_brand() const override { return heif_brand2_avci; }

protected:
  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  std::shared_ptr<Encoder> get_encoder() const override;

public:
  std::shared_ptr<class Decoder_AVC> m_decoder;
  std::shared_ptr<class Encoder_AVC> m_encoder;
};

#endif
