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


#ifndef LIBHEIF_UNC_BOXES_H
#define LIBHEIF_UNC_BOXES_H

#include "box.h"
#include "bitstream.h"
#include "image/pixelimage.h"
#include "unc_types.h"
#include "sequences/seq_boxes.h"

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <utility>


/**
 * Component definition (cmpd) box.
 */
class Box_cmpd : public Box
{
public:
  Box_cmpd()
  {
    set_short_type(fourcc("cmpd"));
  }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

  bool is_essential() const override { return true; }

  struct Component
  {
    uint16_t component_type;
    std::string component_type_uri;

    std::string get_component_type_name() const { return get_component_type_name(component_type); }

    static std::string get_component_type_name(uint16_t type);
  };

  const std::vector<Component>& get_components() const { return m_components; }

  bool has_component(heif_cmpd_component_type) const;

  uint16_t add_component(const Component& component)
  {
    auto index = static_cast<uint16_t>(m_components.size());
    m_components.push_back(component);
    return index;
  }

  void set_components(const std::vector<uint16_t>&);

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  std::vector<Component> m_components;
};

/**
 * Uncompressed Frame Configuration Box
*/
class Box_uncC : public FullBox
{
public:
  Box_uncC() {
    set_short_type(fourcc("uncC"));
  }

  bool is_essential() const override { return true; }

  bool is_minimized() const
  {
    return m_profile != 0 && m_num_tile_cols==1 && m_num_tile_rows==1;
  }

  void derive_box_version() override
  {
    set_version(is_minimized() ? 1 : 0);
  }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

  struct Component
  {
    uint32_t component_index;
    uint16_t component_bit_depth; // range [1..256]
    uint8_t component_format;
    uint8_t component_align_size;
  };

  const std::vector<Component>& get_components() const { return m_components; }

  void add_component(Component component)
  {
    m_components.push_back(component);
  }

  uint32_t get_profile() const { return m_profile; }

  void set_profile(const uint32_t profile)
  {
    m_profile = profile;
  }

  uint8_t get_sampling_type() const { return m_sampling_type; }

  void set_sampling_type(const uint8_t sampling_type)
  {
    m_sampling_type = sampling_type;
  }

  uint8_t get_interleave_type() const { return m_interleave_type; }

  void set_interleave_type(const uint8_t interleave_type)
  {
    m_interleave_type = interleave_type;
  }

  uint8_t get_block_size() const { return m_block_size; }

  void set_block_size(const uint8_t block_size)
  {
    m_block_size = block_size;
  }

  bool is_components_little_endian() const { return m_components_little_endian; }

  void set_components_little_endian (const bool components_little_endian)
  {
    m_components_little_endian = components_little_endian;
  }

  bool is_block_pad_lsb() const { return m_block_pad_lsb; }

  void set_block_pad_lsb(const bool block_pad_lsb)
  {
    m_block_pad_lsb = block_pad_lsb;
  }

  bool is_block_little_endian() const { return m_block_little_endian; }

  void set_block_little_endian(const bool block_little_endian)
  {
    m_block_little_endian = block_little_endian;
  }

  bool is_block_reversed() const { return m_block_reversed; }

  void set_block_reversed(const bool block_reversed)
  {
    m_block_reversed = block_reversed;
  }

  bool is_pad_unknown() const { return m_pad_unknown; }

  void set_pad_unknown(const bool pad_unknown)
  {
    m_pad_unknown = pad_unknown;
  }

  uint32_t get_pixel_size() const { return m_pixel_size; }

  void set_pixel_size(const uint32_t pixel_size)
  {
    m_pixel_size = pixel_size;
  }

  uint32_t get_row_align_size() const { return m_row_align_size; }

  void set_row_align_size(const uint32_t row_align_size)
  {
    m_row_align_size = row_align_size;
  }

  uint32_t get_tile_align_size() const { return m_tile_align_size; }

  void set_tile_align_size(const uint32_t tile_align_size)
  {
    m_tile_align_size = tile_align_size;
  }

  uint32_t get_number_of_tile_columns() const { return m_num_tile_cols; }

  void set_number_of_tile_columns(const uint32_t num_tile_cols)
  {
    m_num_tile_cols = num_tile_cols;
  }

  uint32_t get_number_of_tile_rows() const { return m_num_tile_rows; }

  void set_number_of_tile_rows(const uint32_t num_tile_rows)
  {
    m_num_tile_rows = num_tile_rows;
  }

  uint32_t get_number_of_tiles() const { return m_num_tile_rows * m_num_tile_rows; }

  std::shared_ptr<Box_cmpd> get_synthetic_cmpd() const { return m_synthetic_cmpd; }

  void set_synthetic_cmpd(std::shared_ptr<Box_cmpd> cmpd) { m_synthetic_cmpd = std::move(cmpd); }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  uint32_t m_profile = 0; // 0 = not compliant to any profile

  std::vector<Component> m_components;
  uint8_t m_sampling_type = sampling_mode_no_subsampling; // no subsampling
  uint8_t m_interleave_type = interleave_mode_pixel; // component interleaving
  uint8_t m_block_size = 0;
  bool m_components_little_endian = false;
  bool m_block_pad_lsb = false;
  bool m_block_little_endian = false;
  bool m_block_reversed = false;
  bool m_pad_unknown = false;
  uint32_t m_pixel_size = 0;
  uint32_t m_row_align_size = 0;
  uint32_t m_tile_align_size = 0;
  uint32_t m_num_tile_cols = 1;
  uint32_t m_num_tile_rows = 1;

  std::shared_ptr<Box_cmpd> m_synthetic_cmpd;
};


enum heif_cmpC_compressed_unit_type {
  heif_cmpC_compressed_unit_type_full_item = 0,
  heif_cmpC_compressed_unit_type_image = 1,
  heif_cmpC_compressed_unit_type_image_tile = 2,
  heif_cmpC_compressed_unit_type_image_row = 3,
  heif_cmpC_compressed_unit_type_image_pixel = 4
};

/**
 * Generic compression configuration box (cmpC).
 *
 * This is from ISO/IEC 23001-17 Amd 2.
 */
class Box_cmpC : public FullBox
{
public:
  Box_cmpC()
  {
    set_short_type(fourcc("cmpC"));
  }

  std::string dump(Indent&) const override;

  uint32_t get_compression_type() const { return m_compression_type; }

  heif_cmpC_compressed_unit_type get_compressed_unit_type() const { return m_compressed_unit_type; }

  void set_compression_type(uint32_t type) { m_compression_type = type; }

  void set_compressed_unit_type(heif_cmpC_compressed_unit_type type) { m_compressed_unit_type = type; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  uint32_t m_compression_type = 0;
  heif_cmpC_compressed_unit_type m_compressed_unit_type = heif_cmpC_compressed_unit_type_full_item;
};

/**
 * Generically compressed units item info (icef).
 *
 * This describes the units of compressed data for an item.
 *
 * The box is from ISO/IEC 23001-17 Amd 2.
 */
class Box_icef : public FullBox
{
public:
  Box_icef()
  {
    set_short_type(fourcc("icef"));
  }

  struct CompressedUnitInfo
  {
    uint64_t unit_offset = 0;
    uint64_t unit_size = 0;
  };

  const std::vector<CompressedUnitInfo>& get_units() const { return m_unit_infos; }

  void add_component(const CompressedUnitInfo& unit_info)
  {
    m_unit_infos.push_back(unit_info);
  }

  void set_component(uint32_t tile_idx, const CompressedUnitInfo& unit_info)
  {
    if (tile_idx >= m_unit_infos.size()) {
      m_unit_infos.resize(tile_idx+1);
    }

    m_unit_infos[tile_idx] = unit_info;
  }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  std::vector<CompressedUnitInfo> m_unit_infos;

private:
  const uint8_t get_required_offset_code(uint64_t offset) const;
  const uint8_t get_required_size_code(uint64_t size) const;
};


/**
 * Component pattern definition box (cpat).
 *
 * The component pattern is used when representing filter array
 * data, such as Bayer. It defines the filter mask in the raw
 * data.
 *
 * This is from ISO/IEC 23001-17 Section 6.1.3.
 */
class Box_cpat : public FullBox
{
public:
  Box_cpat()
  {
    set_short_type(fourcc("cpat"));
  }

  uint16_t get_pattern_width() const { return m_pattern.pattern_width; }

  uint16_t get_pattern_height() const { return m_pattern.pattern_height; }

  const BayerPatternCmpd& get_pattern() const { return m_pattern; }

  void set_pattern(const BayerPatternCmpd& pattern) { m_pattern = pattern; }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  BayerPatternCmpd m_pattern;
};


/**
 * Polarization pattern definition box (splz).
 *
 * Describes the polarization filter array pattern on an image sensor.
 * Multiple splz boxes can exist (one per set of components with
 * different polarization filters).
 *
 * This is from ISO/IEC 23001-17 Section 6.1.5.
 */
class Box_splz : public FullBox
{
public:
  Box_splz() { set_short_type(fourcc("splz")); }

  const PolarizationPattern& get_pattern() const { return m_pattern; }

  void set_pattern(const PolarizationPattern& pattern) { m_pattern = pattern; }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  PolarizationPattern m_pattern;
};


/**
 * Sensor bad pixels map box (sbpm).
 *
 * Identifies bad pixels on a sensor for which at least one component
 * value is corrupted. Supports bad rows, bad columns, and individual
 * bad pixel coordinates.
 *
 * This is from ISO/IEC 23001-17 Section 6.1.7.
 */
class Box_sbpm : public FullBox
{
public:
  Box_sbpm() { set_short_type(fourcc("sbpm")); }

  const SensorBadPixelsMap& get_bad_pixels_map() const { return m_map; }
  void set_bad_pixels_map(const SensorBadPixelsMap& map) { m_map = map; }

  std::string dump(Indent&) const override;
  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  SensorBadPixelsMap m_map;
};


/**
 * Sensor non-uniformity correction box (snuc).
 *
 * Provides per-pixel gain and offset tables for sensor non-uniformity
 * correction. The correction equation is: y = nuc_gain * x + nuc_offset.
 *
 * This is from ISO/IEC 23001-17 Section 6.1.6.
 */
class Box_snuc : public FullBox
{
public:
  Box_snuc() { set_short_type(fourcc("snuc")); }

  const SensorNonUniformityCorrection& get_nuc() const { return m_nuc; }
  void set_nuc(const SensorNonUniformityCorrection& nuc) { m_nuc = nuc; }

  std::string dump(Indent&) const override;
  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  MemoryHandle m_memory_handle;
  SensorNonUniformityCorrection m_nuc;
};


/**
 * Chroma location box (cloc).
 *
 * Signals the chroma sample position for subsampled images.
 *
 * This is from ISO/IEC 23001-17 Section 6.1.4.
 */
class Box_cloc : public FullBox
{
public:
  Box_cloc() { set_short_type(fourcc("cloc")); }

  uint8_t get_chroma_location() const { return m_chroma_location; }
  void set_chroma_location(uint8_t loc) { m_chroma_location = loc; }

  std::string dump(Indent&) const override;
  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

  uint8_t m_chroma_location = 0;
};


void fill_uncC_and_cmpd_from_profile(const std::shared_ptr<Box_uncC>& uncC,
                                     std::shared_ptr<Box_cmpd>& cmpd);


class Box_uncv : public Box_VisualSampleEntry
{
public:
  Box_uncv()
  {
    set_short_type(fourcc("uncv"));
  }
};


/**
 * GIMI ItemComponentContentIDProperty.
 *
 * A UUID-type item property that assigns a unique Content ID string
 * to each cmpd component of an image item.
 *
 * UUID: 9db9dd6e-373c-5a4e-8110-21fc83a911fd
 */
class Box_gimi_component_content_ids : public Box
{
public:
  Box_gimi_component_content_ids()
  {
    set_uuid_type(std::vector<uint8_t>{0x9d, 0xb9, 0xdd, 0x6e, 0x37, 0x3c, 0x5a, 0x4e,
                                       0x81, 0x10, 0x21, 0xfc, 0x83, 0xa9, 0x11, 0xfd});
  }

  bool is_essential() const override { return false; }

  bool is_transformative_property() const override { return false; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "GIMI Component Content IDs"; }

  const std::vector<std::string>& get_content_ids() const { return m_content_ids; }

  void set_content_ids(const std::vector<std::string>& ids) { m_content_ids = ids; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::ignorable; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

private:
  std::vector<std::string> m_content_ids;
};


#endif //LIBHEIF_UNC_BOXES_H
