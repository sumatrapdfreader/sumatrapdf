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

#ifndef LIBHEIF_OVERLAY_H
#define LIBHEIF_OVERLAY_H

#include "image_item.h"
#include <vector>
#include <string>
#include <memory>
#include <set>


class ImageOverlay
{
public:
  Error parse(size_t num_images, const std::vector<uint8_t>& data);

  std::vector<uint8_t> write() const;

  std::string dump() const;

  void get_background_color(uint16_t col[4]) const;

  uint32_t get_canvas_width() const { return m_width; }

  uint32_t get_canvas_height() const { return m_height; }

  size_t get_num_offsets() const { return m_offsets.size(); }

  void get_offset(size_t image_index, int32_t* x, int32_t* y) const;

  void set_background_color(const uint16_t rgba_color[4])
  {
    for (int i = 0; i < 4; i++) {
      m_background_color[i] = rgba_color[i];
    }
  }

  void set_canvas_size(uint32_t width, uint32_t height)
  {
    m_width = width;
    m_height = height;
  }

  void add_image_on_top(heif_item_id image_id, int32_t offset_x, int32_t offset_y)
  {
    m_offsets.emplace_back(ImageWithOffset{image_id, offset_x, offset_y});
  }

  struct ImageWithOffset
  {
    heif_item_id image_id;
    int32_t x, y;
  };

  const std::vector<ImageWithOffset>& get_overlay_stack() const { return m_offsets; }

private:
  uint8_t m_version = 0;
  uint8_t m_flags = 0;
  uint16_t m_background_color[4]{0, 0, 0, 0};
  uint32_t m_width = 0;
  uint32_t m_height = 0;

  std::vector<ImageWithOffset> m_offsets;
};


class ImageItem_Overlay : public ImageItem
{
public:
  ImageItem_Overlay(HeifContext* ctx, heif_item_id id);

  ImageItem_Overlay(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("iovl"); }

  // TODO: nclx depends on contained format
  // const heif_color_profile_nclx* get_forced_output_nclx() const override { return nullptr; }

  // heif_compression_format get_compression_format() const override { return heif_compression_HEVC; }

  Error initialize_decoder() override;

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  Error get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const override;

  heif_brand2 get_compatible_brand() const override;

  Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_encoding_options& options,
                                         heif_image_input_class input_class) override
  {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unspecified, "Cannot encode image to 'iovl'"};
  }

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                  std::set<heif_item_id> processed_ids) const override;


  // --- iovl specific

  static Result<std::shared_ptr<ImageItem_Overlay>> add_new_overlay_item(HeifContext* ctx, const ImageOverlay& overlayspec);

private:
  ImageOverlay m_overlay_spec;
  std::vector<heif_item_id> m_overlay_image_ids;

  Error read_overlay_spec();

  Result<std::shared_ptr<HeifPixelImage>> decode_overlay_image(const heif_decoding_options& options,
                                                               std::set<heif_item_id> processed_ids) const;
};


#endif //LIBHEIF_OVERLAY_H
