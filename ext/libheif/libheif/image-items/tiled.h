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

#ifndef LIBHEIF_TILED_H
#define LIBHEIF_TILED_H

#include "image_item.h"
#include "codecs/decoder.h"
#include "box.h"
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include "libheif/heif_experimental.h"
#include <set>


uint64_t number_of_tiles(const heif_tiled_image_parameters& params);

uint32_t nTiles_h(const heif_tiled_image_parameters& params);

uint32_t nTiles_v(const heif_tiled_image_parameters& params);


class Box_tilC : public FullBox
{
  /*
   * Flags:
   * bit 0-1 - number of bits for offsets   (0: 32, 1: 40, 2: 48, 3: 64)
   * bit 2-3 - number of bits for tile size (0:  0, 1: 24; 2: 32, 3: 64)
   * bit 4   - sequential ordering hint
   * bit 5   - use 64 bit dimensions (currently unused because ispe is limited to 32 bit)
   */
public:
  Box_tilC()
  {
    set_short_type(fourcc("tilC"));

    init_heif_tiled_image_parameters(m_parameters);
  }

  bool is_essential() const override { return true; }

  void derive_box_version() override;

  void set_parameters(const heif_tiled_image_parameters& params) { m_parameters = params; }

  void set_compression_format(heif_compression_format format) { m_parameters.compression_format_fourcc = ImageItem::compression_format_to_fourcc_infe_type(format); }

  const heif_tiled_image_parameters& get_parameters() const { return m_parameters; }

  Error write(StreamWriter& writer) const override;

  std::string dump(Indent&) const override;

  std::vector<std::shared_ptr<Box>>& get_tile_properties() { return m_children; }

  const std::vector<std::shared_ptr<Box>>& get_tile_properties() const { return m_children; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  heif_tiled_image_parameters m_parameters;

  static void init_heif_tiled_image_parameters(heif_tiled_image_parameters& params);
};


#define TILD_OFFSET_NOT_AVAILABLE 0
#define TILD_OFFSET_SEE_LOWER_RESOLUTION_LAYER 1
#define TILD_OFFSET_NOT_LOADED 10

class TiledHeader
{
public:
  Error set_parameters(const heif_tiled_image_parameters& params);

  const heif_tiled_image_parameters& get_parameters() const { return m_parameters; }

  void set_compression_format(heif_compression_format format) { m_parameters.compression_format_fourcc = ImageItem::compression_format_to_fourcc_infe_type(format); }

  Error read_full_offset_table(const std::shared_ptr<HeifFile>& file, heif_item_id tild_id, const heif_security_limits* limits);

  Error read_offset_table_range(const std::shared_ptr<HeifFile>& file, heif_item_id tild_id,
                                uint64_t start, uint64_t end);

  std::vector<uint8_t> write_offset_table();

  std::string dump() const;

  void set_tild_tile_range(uint32_t tile_x, uint32_t tile_y, uint64_t offset, uint32_t size);

  size_t get_header_size() const;

  uint64_t get_tile_offset(uint32_t idx) const { return m_offsets[idx].offset; }

  uint32_t get_tile_size(uint32_t idx) const { return m_offsets[idx].size; }

  bool is_tile_offset_known(uint32_t idx) const { return m_offsets[idx].offset != TILD_OFFSET_NOT_LOADED; }

  uint32_t get_offset_table_entry_size() const;

  // Assuming that we have to read offset table 'idx', but we'd like to read more entries at once to reduce
  // the load of small network transfers (preferred: 'nEntries'). Return a range of indices to read.
  // This may be less than 'nEntries'. The returned range is [start, end)
  [[nodiscard]] std::pair<uint32_t, uint32_t> get_tile_offset_table_range_to_read(uint32_t idx, uint32_t nEntries) const;

private:
  heif_tiled_image_parameters m_parameters;

  struct TileOffset {
    uint64_t offset = TILD_OFFSET_NOT_LOADED;
    uint32_t size = 0;
  };

  // TODO uint64_t m_start_of_offset_table_in_file = 0;
  std::vector<TileOffset> m_offsets;

  // TODO size_t m_offset_table_start = 0; // start of offset table (= number of bytes in header)
  size_t m_header_size = 0; // including offset table
};


class ImageItem_Tiled : public ImageItem
{
public:
  ImageItem_Tiled(HeifContext* ctx, heif_item_id id);

  ImageItem_Tiled(HeifContext* ctx);

  uint32_t get_infe_type() const override { return fourcc("tili"); }

  // TODO: nclx depends on contained format
  // const heif_color_profile_nclx* get_forced_output_nclx() const override { return nullptr; }

  heif_compression_format get_compression_format() const override;

  static Result<std::shared_ptr<ImageItem_Tiled>> add_new_tiled_item(HeifContext* ctx, const heif_tiled_image_parameters* parameters,
                                                                     const heif_encoder* encoder);

  Error add_image_tile(uint32_t tile_x, uint32_t tile_y,
                       const std::shared_ptr<HeifPixelImage>& image,
                       struct heif_encoder* encoder);


  Error initialize_decoder() override;

  void process_before_write() override;

  Error get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const override;

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_encoding_options& options,
                                         heif_image_input_class input_class) override
  {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unspecified, "Cannot encode image to 'tild'"};
  }

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                  std::set<heif_item_id> processed_ids) const override;

  heif_brand2 get_compatible_brand() const override;

  // --- tild

  void set_tild_header(const TiledHeader& header) { m_tild_header = header; }

  TiledHeader& get_tild_header() { return m_tild_header; }

  uint64_t get_next_tild_position() const { return m_next_tild_position; }

  void set_next_tild_position(uint64_t pos) { m_next_tild_position = pos; }

  heif_image_tiling get_heif_image_tiling() const override;

  void get_tile_size(uint32_t& w, uint32_t& h) const override;

private:
  TiledHeader m_tild_header;
  uint64_t m_next_tild_position = 0;

  uint32_t mReadChunkSize_bytes = 64*1024; // 64 kiB
  bool m_preload_offset_table = false;

  std::shared_ptr<ImageItem> m_tile_item;
  std::shared_ptr<class Decoder> m_tile_decoder;

  Result<DataExtent>
  get_compressed_data_for_tile(uint32_t tx, uint32_t ty) const;

  Result<std::shared_ptr<HeifPixelImage>> decode_grid_tile(const heif_decoding_options& options, uint32_t tx, uint32_t ty) const;

  Error load_tile_offset_entry(uint32_t idx);

  Error append_compressed_tile_data(std::vector<uint8_t>& data, uint32_t tx, uint32_t ty) const;
};


#endif //LIBHEIF_TILED_H
