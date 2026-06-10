/*
 * HEIF codec.
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

#ifndef LIBHEIF_UNC_DECODER_PIXEL_INTERLEAVE_H
#define LIBHEIF_UNC_DECODER_PIXEL_INTERLEAVE_H

#include "unc_decoder_legacybase.h"
#include <memory>
#include <utility>
#include <vector>


class unc_decoder_pixel_interleave : public unc_decoder_legacybase
{
public:
  unc_decoder_pixel_interleave(uint32_t width, uint32_t height, std::shared_ptr<const Box_cmpd> cmpd, std::shared_ptr<const Box_uncC> uncC, const std::vector<uint32_t>& uncC_index_to_comp_ids) :
      unc_decoder_legacybase(width, height, std::move(cmpd), std::move(uncC), uncC_index_to_comp_ids) {}

  Result<std::vector<uint64_t>> get_tile_data_sizes() const override;

  Error decode_tile(const std::vector<uint8_t>& tile_data,
                    std::shared_ptr<HeifPixelImage>& img,
                    uint32_t out_x0, uint32_t out_y0) override;

  [[nodiscard]] Error processTile(UncompressedBitReader& srcBits, uint32_t out_x0, uint32_t out_y0);
};


class unc_decoder_factory_pixel_interleave : public unc_decoder_factory
{
private:
  bool can_decode(const std::shared_ptr<const Box_uncC>& uncC) const override;

  std::unique_ptr<unc_decoder> create(
      uint32_t width, uint32_t height,
      const std::shared_ptr<const Box_cmpd>& cmpd,
      const std::shared_ptr<const Box_uncC>& uncC,
      const std::vector<uint32_t>& uncC_index_to_comp_ids) const override;
};

#endif // LIBHEIF_UNC_DECODER_PIXEL_INTERLEAVE_H
