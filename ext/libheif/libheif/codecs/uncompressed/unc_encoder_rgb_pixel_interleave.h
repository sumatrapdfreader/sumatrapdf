/*
 * HEIF codec.
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_UNC_ENCODER_RGB_PIXEL_INTERLEAVE_H
#define LIBHEIF_UNC_ENCODER_RGB_PIXEL_INTERLEAVE_H

#include "unc_encoder.h"

#include <memory>
#include <vector>

class unc_encoder_rgb_pixel_interleave : public unc_encoder
{
public:
  unc_encoder_rgb_pixel_interleave(const std::shared_ptr<const HeifPixelImage>& image,
                                   const heif_encoding_options& options);

  uint64_t compute_tile_data_size_bytes(uint32_t tile_width, uint32_t tile_height) const override;

  [[nodiscard]] std::vector<uint8_t> encode_tile(const std::shared_ptr<const HeifPixelImage>& image) const override;

private:
  uint8_t m_bytes_per_pixel = 0;
};


class unc_encoder_factory_rgb_pixel_interleave : public unc_encoder_factory
{
public:

private:
  [[nodiscard]] bool can_encode(const std::shared_ptr<const HeifPixelImage>& image,
                                const heif_encoding_options& options) const override;

  std::unique_ptr<const unc_encoder> create(const std::shared_ptr<const HeifPixelImage>& image,
                                            const heif_encoding_options& options) const override;
};

#endif //LIBHEIF_UNC_ENCODER_RGB_PIXEL_INTERLEAVE_H
