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

#ifndef UNCI_DECODER_ROW_INTERLEAVE_H
#define UNCI_DECODER_ROW_INTERLEAVE_H

#include "decoder_abstract.h"
#include <memory>
#include <utility>


class RowInterleaveDecoder : public AbstractDecoder
{
public:
  RowInterleaveDecoder(uint32_t width, uint32_t height,
                       std::shared_ptr<const Box_cmpd> cmpd, std::shared_ptr<const Box_uncC> uncC) :
      AbstractDecoder(width, height, std::move(cmpd), std::move(uncC)) {}


  Error decode_tile(const DataExtent& dataExtent,
                    const UncompressedImageCodec::unci_properties& properties,
                    std::shared_ptr<HeifPixelImage>& img,
                    uint32_t out_x0, uint32_t out_y0,
                    uint32_t image_width, uint32_t image_height,
                    uint32_t tile_x, uint32_t tile_y) override;

private:
  void processTile(UncompressedBitReader& srcBits, uint32_t tile_row, uint32_t tile_column,
                   uint32_t out_x0, uint32_t out_y0);
};

#endif // UNCI_DECODER_ROW_INTERLEAVE_H
