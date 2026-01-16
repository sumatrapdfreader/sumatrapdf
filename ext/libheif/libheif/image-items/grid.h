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

#ifndef LIBHEIF_IMAGEITEM_GRID_H
#define LIBHEIF_IMAGEITEM_GRID_H

#include "image_item.h"
#include <vector>
#include <string>
#include <memory>
#include <set>


class ImageGrid
{
public:
  Error parse(const std::vector<uint8_t>& data);

  std::vector<uint8_t> write() const;

  std::string dump() const;

  uint32_t get_width() const { return m_output_width; }

  uint32_t get_height() const { return m_output_height; }

  uint16_t get_rows() const
  {
    return m_rows;
  }

  uint16_t get_columns() const
  {
    return m_columns;
  }

  void set_num_tiles(uint16_t columns, uint16_t rows)
  {
    m_rows = rows;
    m_columns = columns;
  }

  void set_output_size(uint32_t width, uint32_t height)
  {
    m_output_width = width;
    m_output_height = height;
  }

private:
  uint16_t m_rows = 0;
  uint16_t m_columns = 0;
  uint32_t m_output_width = 0;
  uint32_t m_output_height = 0;
};





class ImageItem_Grid : public ImageItem
{
public:
  ImageItem_Grid(HeifContext* ctx, heif_item_id id);

  ImageItem_Grid(HeifContext* ctx);

  ~ImageItem_Grid() override;

  uint32_t get_infe_type() const override { return fourcc("grid"); }

  static Result<std::shared_ptr<ImageItem_Grid>> add_new_grid_item(HeifContext* ctx,
                                                                   uint32_t output_width,
                                                                   uint32_t output_height,
                                                                   uint16_t tile_rows,
                                                                   uint16_t tile_columns,
                                                                   const heif_encoding_options* encoding_options);

  Error add_image_tile(uint32_t tile_x, uint32_t tile_y,
                       const std::shared_ptr<HeifPixelImage>& image,
                       heif_encoder* encoder);

  static Result<std::shared_ptr<ImageItem_Grid>> add_and_encode_full_grid(HeifContext* ctx,
                                                                          const std::vector<std::shared_ptr<HeifPixelImage>>& tiles,
                                                                          uint16_t rows,
                                                                          uint16_t columns,
                                                                          heif_encoder* encoder,
                                                                          const heif_encoding_options& options);


  // TODO: nclx depends on contained format
  // const heif_color_profile_nclx* get_forced_output_nclx() const override { return nullptr; }

  // heif_compression_format get_compression_format() const override { return heif_compression_HEVC; }

  Error initialize_decoder() override;

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  void set_encoding_options(const heif_encoding_options* options) {
    heif_encoding_options_copy(m_encoding_options, options);
  }

  const heif_encoding_options* get_encoding_options() const { return m_encoding_options; }

  Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_encoding_options& options,
                                         heif_image_input_class input_class) override
  {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unspecified, "Cannot encode image to 'grid'"};
  }

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                  std::set<heif_item_id> processed_ids) const override;

  heif_brand2 get_compatible_brand() const override;

protected:
  Result<std::shared_ptr<Decoder>> get_decoder() const override;

public:

  // --- grid specific

  const ImageGrid& get_grid_spec() const { return m_grid_spec; }

  void set_grid_spec(const ImageGrid& grid) { m_grid_spec = grid; m_grid_tile_ids.resize(size_t{grid.get_rows()} * grid.get_columns()); }

  const std::vector<heif_item_id>& get_grid_tiles() const { return m_grid_tile_ids; }

  void set_grid_tile_id(uint32_t tile_x, uint32_t tile_y, heif_item_id);

  heif_image_tiling get_heif_image_tiling() const override;

  void get_tile_size(uint32_t& w, uint32_t& h) const override;

private:
  ImageGrid m_grid_spec;
  std::vector<heif_item_id> m_grid_tile_ids;

  heif_encoding_options* m_encoding_options = nullptr;

  Error read_grid_spec();

  Result<std::shared_ptr<HeifPixelImage>> decode_full_grid_image(const heif_decoding_options& options, std::set<heif_item_id> processed_ids) const;

  Result<std::shared_ptr<HeifPixelImage>> decode_grid_tile(const heif_decoding_options& options, uint32_t tx, uint32_t ty, std::set<heif_item_id> processed_ids) const;

  Error decode_and_paste_tile_image(heif_item_id tileID, uint32_t x0, uint32_t y0,
                                    std::shared_ptr<HeifPixelImage>& inout_image,
                                    const heif_decoding_options& options, int& progress_counter,
                                    std::shared_ptr<std::vector<Error> > warnings,
                                    std::set<heif_item_id> processed_ids) const;
};


#endif //LIBHEIF_GRID_H
