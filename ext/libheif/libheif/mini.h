/*
 * HEIF codec.
 * Copyright (c) 2024 Brad Hards <bradh@frogmouth.net>
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

#ifndef LIBHEIF_MINI_H
#define LIBHEIF_MINI_H

#include "libheif/heif.h"
#include "box.h"

#include <memory>
#include <string>
#include <vector>
#include <utility>

class Box_mini : public Box
{
public:
  Box_mini()
  {
    set_short_type(fourcc("mini"));
  }

  Error create_expanded_boxes(class HeifFile* file);

  // --- Getters ---

  bool get_icc_flag() const { return m_icc_flag; }
  bool get_exif_flag() const { return m_exif_flag; }
  bool get_xmp_flag() const { return m_xmp_flag; }

  uint32_t get_width() const { return m_width; }
  uint32_t get_height() const { return m_height; }

  uint8_t get_bit_depth() const { return m_bit_depth; }

  uint8_t get_orientation() const { return m_orientation; }

  std::vector<uint8_t> get_main_item_codec_config() const { return m_main_item_codec_config; }
  std::vector<uint8_t> get_alpha_item_codec_config() const { return m_alpha_item_codec_config; }
  std::vector<uint8_t> get_gainmap_item_codec_config() const { return m_gainmap_item_codec_config; }

  std::vector<uint8_t> get_icc_data() const { return m_icc_data; }

  uint64_t get_main_item_data_offset() const { return m_main_item_data_offset; }
  uint32_t get_main_item_data_size() const { return m_main_item_data_size; }
  uint64_t get_alpha_item_data_offset() const { return m_alpha_item_data_offset; }
  uint32_t get_alpha_item_data_size() const { return m_alpha_item_data_size; }
  uint64_t get_exif_item_data_offset() const { return m_exif_item_data_offset; }
  uint32_t get_exif_item_data_size() const { return m_exif_data_size; }
  uint64_t get_xmp_item_data_offset() const { return m_xmp_item_data_offset; }
  uint32_t get_xmp_item_data_size() const { return m_xmp_data_size; }

  uint16_t get_colour_primaries() const { return m_colour_primaries; }
  uint16_t get_transfer_characteristics() const { return m_transfer_characteristics; }
  uint16_t get_matrix_coefficients() const { return m_matrix_coefficients; }
  bool get_full_range_flag() const { return m_full_range_flag; }

  // --- Setters (for write path) ---

  void set_version(uint8_t v) { m_version = v; }
  void set_explicit_codec_types_flag(bool f) { m_explicit_codec_types_flag = f; }
  void set_float_flag(bool f) { m_float_flag = f; }
  void set_full_range_flag(bool f) { m_full_range_flag = f; }
  void set_alpha_flag(bool f) { m_alpha_flag = f; }
  void set_explicit_cicp_flag(bool f) { m_explicit_cicp_flag = f; }
  void set_hdr_flag(bool f) { m_hdr_flag = f; }
  void set_icc_flag(bool f) { m_icc_flag = f; }
  void set_exif_flag(bool f) { m_exif_flag = f; }
  void set_xmp_flag(bool f) { m_xmp_flag = f; }
  void set_chroma_subsampling(uint8_t cs) { m_chroma_subsampling = cs; }
  void set_orientation(uint8_t o) { m_orientation = o; }
  void set_width(uint32_t w) { m_width = w; }
  void set_height(uint32_t h) { m_height = h; }
  void set_bit_depth(uint8_t bd) { m_bit_depth = bd; }
  void set_chroma_is_horizontally_centered(bool f) { m_chroma_is_horizontally_centered = f; }
  void set_chroma_is_vertically_centered(bool f) { m_chroma_is_vertically_centered = f; }
  void set_alpha_is_premultiplied(bool f) { m_alpha_is_premultiplied = f; }
  void set_colour_primaries(uint16_t cp) { m_colour_primaries = cp; }
  void set_transfer_characteristics(uint16_t tc) { m_transfer_characteristics = tc; }
  void set_matrix_coefficients(uint16_t mc) { m_matrix_coefficients = mc; }
  void set_infe_type(uint32_t t) { m_infe_type = t; }
  void set_codec_config_type(uint32_t t) { m_codec_config_type = t; }
  void set_exif_xmp_compressed_flag(bool f) { m_exif_xmp_compressed_flag = f; }

  void set_main_item_codec_config(std::vector<uint8_t> data) { m_main_item_codec_config = std::move(data); }
  void set_alpha_item_codec_config(std::vector<uint8_t> data) { m_alpha_item_codec_config = std::move(data); }
  void set_gainmap_item_codec_config(std::vector<uint8_t> data) { m_gainmap_item_codec_config = std::move(data); }
  void set_icc_data(std::vector<uint8_t> data) { m_icc_data = std::move(data); }

  void set_main_item_data(std::vector<uint8_t> data) { m_main_item_data = std::move(data); }
  void set_alpha_item_data(std::vector<uint8_t> data) { m_alpha_item_data = std::move(data); }
  void set_gainmap_item_data(std::vector<uint8_t> data) { m_gainmap_item_data = std::move(data); }
  void set_exif_data(std::vector<uint8_t> data) { m_exif_data_bytes = std::move(data); }
  void set_xmp_data(std::vector<uint8_t> data) { m_xmp_data_bytes = std::move(data); }

  // Gainmap setters
  void set_gainmap_flag(bool f) { m_gainmap_flag = f; }
  void set_gainmap_width(uint32_t w) { m_gainmap_width = w; }
  void set_gainmap_height(uint32_t h) { m_gainmap_height = h; }
  void set_gainmap_matrix_coefficients(uint8_t mc) { m_gainmap_matrix_coefficients = mc; }
  void set_gainmap_full_range_flag(bool f) { m_gainmap_full_range_flag = f; }
  void set_gainmap_chroma_subsampling(uint8_t cs) { m_gainmap_chroma_subsampling = cs; }
  void set_gainmap_float_flag(bool f) { m_gainmap_float_flag = f; }
  void set_gainmap_bit_depth(uint8_t bd) { m_gainmap_bit_depth = bd; }
  void set_tmap_icc_flag(bool f) { m_tmap_icc_flag = f; }
  void set_tmap_explicit_cicp_flag(bool f) { m_tmap_explicit_cicp_flag = f; }
  void set_tmap_colour_primaries(uint16_t cp) { m_tmap_colour_primaries = cp; }
  void set_tmap_transfer_characteristics(uint16_t tc) { m_tmap_transfer_characteristics = tc; }
  void set_tmap_matrix_coefficients(uint16_t mc) { m_tmap_matrix_coefficients = mc; }
  void set_tmap_full_range_flag(bool f) { m_tmap_full_range_flag = f; }
  void set_tmap_icc_data(std::vector<uint8_t> data) { m_tmap_icc_data = std::move(data); }
  void set_gainmap_metadata(std::vector<uint8_t> data) { m_gainmap_metadata = std::move(data); }

  // HDR metadata setters
  void set_clli(std::shared_ptr<Box_clli> box) { m_clli = std::move(box); }
  void set_mdcv(std::shared_ptr<Box_mdcv> box) { m_mdcv = std::move(box); }
  void set_cclv(std::shared_ptr<Box_cclv> box) { m_cclv = std::move(box); }
  void set_amve(std::shared_ptr<Box_amve> box) { m_amve = std::move(box); }
  void set_ndwt(std::shared_ptr<Box_ndwt> box) { m_ndwt = std::move(box); }
  void set_tmap_clli(std::shared_ptr<Box_clli> box) { m_tmap_clli = std::move(box); }
  void set_tmap_mdcv(std::shared_ptr<Box_mdcv> box) { m_tmap_mdcv = std::move(box); }
  void set_tmap_cclv(std::shared_ptr<Box_cclv> box) { m_tmap_cclv = std::move(box); }
  void set_tmap_amve(std::shared_ptr<Box_amve> box) { m_tmap_amve = std::move(box); }
  void set_tmap_ndwt(std::shared_ptr<Box_ndwt> box) { m_tmap_ndwt = std::move(box); }

  std::string dump(Indent &) const override;

  Error write(StreamWriter& writer) const override;

  // Check if a HeifFile can be represented as a mini box.
  // Returns true if conversion is possible. If false, out_reason explains why not.
  static bool can_convert_to_mini(const class HeifFile* file, std::string& out_reason);

  // Create a Box_mini from a HeifFile's meta box structure.
  // This is the inverse of create_expanded_boxes().
  // Returns nullptr if conversion is not possible.
  static std::shared_ptr<Box_mini> create_from_heif_file(class HeifFile* file);

  // Compose the cumulative EXIF-style orientation of a sequence of property
  // boxes by composing irot/imir transforms in list order via
  // heif_orientation_concat(). Non-transform properties are ignored.
  // Returns heif_orientation_normal (1) for an empty list.
  static heif_orientation compute_orientation_from_properties(
      const std::vector<std::shared_ptr<Box>>& properties);

protected:
  Error parse(BitstreamRange &range, const heif_security_limits *limits) override;

private:
  uint8_t m_version = 0;
  bool m_explicit_codec_types_flag = false;
  bool m_float_flag = false;
  bool m_full_range_flag = false;
  bool m_alpha_flag = false;
  bool m_explicit_cicp_flag = false;
  bool m_hdr_flag = false;
  bool m_icc_flag = false;
  bool m_exif_flag = false;
  bool m_xmp_flag = false;
  bool m_exif_xmp_compressed_flag = false; // when enabled, data is compressed with 'deflate'
  uint8_t m_chroma_subsampling = 0;
  uint8_t m_orientation = 0;

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  uint8_t m_bit_depth = 8;
  bool m_chroma_is_horizontally_centered = false;
  bool m_chroma_is_vertically_centered = false;
  bool m_alpha_is_premultiplied = false;
  uint16_t m_colour_primaries = 0;
  uint16_t m_transfer_characteristics = 0;
  uint16_t m_matrix_coefficients = 0;

  uint32_t m_infe_type = 0;
  uint32_t m_codec_config_type = 0;

  bool m_gainmap_flag = false;

  uint32_t m_gainmap_width = 0;
  uint32_t m_gainmap_height = 0;
  uint8_t m_gainmap_matrix_coefficients = 0;
  bool m_gainmap_full_range_flag = false;
  uint8_t m_gainmap_chroma_subsampling = 0;
  bool m_gainmap_chroma_is_horizontally_centred = false;
  bool m_gainmap_chroma_is_vertically_centred = false;
  bool m_gainmap_float_flag = false;
  uint8_t m_gainmap_bit_depth = 8;
  bool m_tmap_icc_flag = false;
  bool m_tmap_explicit_cicp_flag = false;
  uint16_t m_tmap_colour_primaries = 0;
  uint16_t m_tmap_transfer_characteristics = 0;
  uint16_t m_tmap_matrix_coefficients = 0;
  bool m_tmap_full_range_flag = false;

  bool m_reve_flag = false;
  std::shared_ptr<Box_clli> m_clli;
  std::shared_ptr<Box_mdcv> m_mdcv;
  std::shared_ptr<Box_cclv> m_cclv;
  std::shared_ptr<Box_amve> m_amve;
  // std::shared_ptr<Box_reve> m_reve;
  std::shared_ptr<Box_ndwt> m_ndwt;

  bool m_tmap_reve_flag = false;
  std::shared_ptr<Box_clli> m_tmap_clli;
  std::shared_ptr<Box_mdcv> m_tmap_mdcv;
  std::shared_ptr<Box_cclv> m_tmap_cclv;
  std::shared_ptr<Box_amve> m_tmap_amve;
  // std::shared_ptr<Box_reve> m_tmap_reve;
  std::shared_ptr<Box_ndwt> m_tmap_ndwt;

  std::vector<uint8_t> m_alpha_item_codec_config;
  std::vector<uint8_t> m_gainmap_item_codec_config;
  std::vector<uint8_t> m_main_item_codec_config;
  std::vector<uint8_t> m_icc_data;
  std::vector<uint8_t> m_tmap_icc_data;
  std::vector<uint8_t> m_gainmap_metadata;

  uint64_t m_alpha_item_data_offset = 0;
  uint32_t m_alpha_item_data_size = 0;
  uint64_t m_main_item_data_offset = 0;
  uint32_t m_main_item_data_size = 0;
  uint64_t m_gainmap_item_data_offset = 0;
  uint32_t m_gainmap_item_data_size = 0;
  uint64_t m_exif_item_data_offset = 0;
  uint32_t m_exif_data_size = 0;
  uint64_t m_xmp_item_data_offset = 0;
  uint32_t m_xmp_data_size = 0;

  // Image data for write path (not populated during parse)
  std::vector<uint8_t> m_main_item_data;
  std::vector<uint8_t> m_alpha_item_data;
  std::vector<uint8_t> m_gainmap_item_data;
  std::vector<uint8_t> m_exif_data_bytes;
  std::vector<uint8_t> m_xmp_data_bytes;
};

#endif
