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

#ifndef HEIF_HEVC_H
#define HEIF_HEVC_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"

#include <memory>
#include <string>
#include <vector>
#include "image_item.h"


class ImageItem_HEVC : public ImageItem
{
public:
  ImageItem_HEVC(HeifContext* ctx, heif_item_id id);

  ImageItem_HEVC(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("hvc1"); }

  // TODO: MIAF says that the *:hevc:* urn is deprecated and we should use "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"
  const char* get_auxC_alpha_channel_type() const override { return "urn:mpeg:hevc:2015:auxid:1"; }

  heif_compression_format get_compression_format() const override { return heif_compression_HEVC; }

  Error initialize_decoder() override;

  void set_decoder_input_data() override;


  heif_brand2 get_compatible_brand() const override;

  // currently not used
  void set_preencoded_hevc_image(const std::vector<uint8_t>& data);

protected:
  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  std::shared_ptr<Encoder> get_encoder() const override;

private:
  std::shared_ptr<class Decoder_HEVC> m_decoder;
  std::shared_ptr<class Encoder_HEVC> m_encoder;
};

#endif
