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


#ifndef LIBHEIF_UNC_IMAGE_H
#define LIBHEIF_UNC_IMAGE_H

#include "pixelimage.h"
#include "file.h"
#include "context.h"
#include "libheif/heif_uncompressed.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <set>

class HeifContext;


class ImageItem_uncompressed : public ImageItem
{
public:
  ImageItem_uncompressed(HeifContext* ctx, heif_item_id id);

  ImageItem_uncompressed(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("unci"); }

  heif_compression_format get_compression_format() const override { return heif_compression_uncompressed; }

  // Instead of storing alpha in a separate unci, this is put into the main image item
  const char* get_auxC_alpha_channel_type() const override { return nullptr; }

  bool has_coded_alpha_channel() const override;

  // const heif_color_profile_nclx* get_forced_output_nclx() const override { return nullptr; }

  bool is_ispe_essential() const override { return true; }

  void get_tile_size(uint32_t& w, uint32_t& h) const override;

  // Code from encode_uncompressed_image() has been moved to here.

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                  std::set<heif_item_id> processed_ids) const override;

  heif_image_tiling get_heif_image_tiling() const override;

  Error initialize_decoder() override;

  void set_decoder_input_data() override;

  heif_brand2 get_compatible_brand() const override;

public:

  // --- encoding

  Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_encoding_options& options,
                                         heif_image_input_class input_class) override;

  static Result<Encoder::CodedImageData> encode_static(const std::shared_ptr<HeifPixelImage>& image,
                                                       const heif_encoding_options& options);

  static Result<std::shared_ptr<ImageItem_uncompressed>> add_unci_item(HeifContext* ctx,
                                                                const heif_unci_image_parameters* parameters,
                                                                const heif_encoding_options* encoding_options,
                                                                const std::shared_ptr<const HeifPixelImage>& prototype);

  Error add_image_tile(uint32_t tile_x, uint32_t tile_y, const std::shared_ptr<const HeifPixelImage>& image, bool save_alpha);

protected:
  Result<std::shared_ptr<Decoder>> get_decoder() const override;

  std::shared_ptr<Encoder> get_encoder() const override;

private:
  std::shared_ptr<class Decoder_uncompressed> m_decoder;
  std::shared_ptr<class Encoder_uncompressed> m_encoder;

  /*
  Result<ImageItem::CodedImageData> generate_headers(const std::shared_ptr<const HeifPixelImage>& src_image,
                                                     const heif_unci_image_parameters* parameters,
                                                     const struct heif_encoding_options* options);
                                                     */

  uint64_t m_next_tile_write_pos = 0;
};

#endif //LIBHEIF_UNC_IMAGE_H
