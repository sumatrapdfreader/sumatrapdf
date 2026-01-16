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

#ifndef LIBHEIF_IDEN_H
#define LIBHEIF_IDEN_H

#include "image_item.h"
#include <vector>
#include <string>
#include <memory>
#include <set>


class ImageItem_iden : public ImageItem
{
public:
  ImageItem_iden(HeifContext* ctx, heif_item_id id);

  ImageItem_iden(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("iden"); }

  // TODO: nclx depends on contained format
  // const heif_color_profile_nclx* get_forced_output_nclx() const override { return nullptr; }

  // heif_compression_format get_compression_format() const override { return heif_compression_HEVC; }

  //Error on_load_file() override;

  Error get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const override;

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_encoding_options& options,
                                         heif_image_input_class input_class) override
  {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unspecified, "Cannot encode image to 'iden'"};
  }

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                  std::set<heif_item_id> processed_ids) const override;

  heif_brand2 get_compatible_brand() const override;

private:
};


#endif //LIBHEIF_IDEN_H
