/*
 * HEIF mask image codec.
 *
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
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


#ifndef LIBHEIF_MASK_IMAGE_H
#define LIBHEIF_MASK_IMAGE_H

#include "box.h"
#include "bitstream.h"
#include "pixelimage.h"
#include "file.h"
#include "context.h"

#include <memory>
#include <string>
#include <vector>
#include <set>

/**
  * Mask Configuration Property (mskC).
  *
  * Each mask image item (mski) shall have an associated MaskConfigurationProperty
  * that provides information required to generate the mask of the associated mask
  * item.
  */
class Box_mskC : public FullBox
{
public:

  Box_mskC()
  {
    set_short_type(fourcc("mskC"));
  }

  bool is_essential() const override { return true; }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

  uint8_t get_bits_per_pixel() const
  { return m_bits_per_pixel; }

  void set_bits_per_pixel(uint8_t bits_per_pixel)
  { m_bits_per_pixel = bits_per_pixel; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  uint8_t m_bits_per_pixel = 0;
};

class MaskImageCodec
{
public:
  static Error decode_mask_image(const HeifContext* context,
                                  heif_item_id ID,
                                  std::shared_ptr<HeifPixelImage>& img,
                                  const std::vector<uint8_t>& data);
};



class ImageItem_mask : public ImageItem
{
public:
  ImageItem_mask(HeifContext* ctx, heif_item_id id) : ImageItem(ctx, id) {}

  ImageItem_mask(HeifContext* ctx) : ImageItem(ctx) {}

  uint32_t get_infe_type() const override { return fourcc("mski"); }

  heif_compression_format get_compression_format() const override { return heif_compression_mask; }

  bool is_ispe_essential() const override { return true; }

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override { return 0; }

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                  std::set<heif_item_id> processed_ids) const override;

  Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_encoding_options& options,
                                         heif_image_input_class input_class) override;
};

#endif //LIBHEIF_MASK_IMAGE_H
