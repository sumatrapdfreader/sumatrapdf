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

#ifndef HEIF_AVIF_DEC_H
#define HEIF_AVIF_DEC_H

#include "libheif/heif.h"
#include "error.h"

#include <memory>
#include <vector>
#include <codecs/decoder.h>

class Box_av1C;


class Decoder_AVIF : public Decoder
{
public:
  explicit Decoder_AVIF(const std::shared_ptr<const Box_av1C>& av1C) : m_av1C(av1C) {}

  heif_compression_format get_compression_format() const override { return heif_compression_AV1; }

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  Error get_coded_image_colorspace(heif_colorspace*, heif_chroma*) const override;

  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

private:
  const std::shared_ptr<const Box_av1C> m_av1C;
};

#endif
