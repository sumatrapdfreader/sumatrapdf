/*
 * HEIF codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef HEIF_JPEG_DEC_H
#define HEIF_JPEG_DEC_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"
#include "codecs/decoder.h"

#include <memory>
#include <vector>
#include <optional>

class Box_jpgC;


class Decoder_JPEG : public Decoder
{
public:
  explicit Decoder_JPEG(const std::shared_ptr<const Box_jpgC>& jpgC) : m_jpgC(jpgC) {}

  heif_compression_format get_compression_format() const override { return heif_compression_JPEG; }

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  Error get_coded_image_colorspace(heif_colorspace*, heif_chroma*) const override;

  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

private:
  const std::shared_ptr<const Box_jpgC> m_jpgC; // Optional jpgC box. May be NULL.

  struct ConfigInfo {
    uint8_t sample_precision = 0;
    heif_chroma chroma = heif_chroma_undefined;

    uint8_t nComponents = 0;
    uint8_t h_sampling[3]{};
    uint8_t v_sampling[3]{};
  };

  std::optional<ConfigInfo> m_config;

  Error parse_SOF();
};

#endif
