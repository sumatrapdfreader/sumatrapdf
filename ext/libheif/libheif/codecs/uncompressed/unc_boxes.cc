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


#include <cstdint>
#include <cstring>
#include <algorithm>
#include <map>
#include <iostream>
#include <cassert>
#include <limits>

#include "libheif/heif.h"
#include "libheif/heif_experimental.h"
#include "unc_types.h"
#include "unc_boxes.h"


/**
 * Check for valid component format.
 *
 * @param format the format value to check
 * @return true if the format is a valid value, or false otherwise
 */
bool is_valid_component_format(uint8_t format)
{
  return format <= component_format_max_valid;
}

static std::map<heif_uncompressed_component_format, const char*> sNames_uncompressed_component_format{
    {component_format_unsigned, "unsigned"},
    {component_format_float,    "float"},
    {component_format_complex,  "complex"}
};


/**
 * Check for valid interleave mode.
 *
 * @param interleave the interleave value to check
 * @return true if the interleave mode is valid, or false otherwise
 */
bool is_valid_interleave_mode(uint8_t interleave)
{
  return interleave <= interleave_mode_max_valid;
}

static std::map<heif_uncompressed_interleave_mode, const char*> sNames_uncompressed_interleave_mode{
    {interleave_mode_component,      "component"},
    {interleave_mode_pixel,          "pixel"},
    {interleave_mode_mixed,          "mixed"},
    {interleave_mode_row,            "row"},
    {interleave_mode_tile_component, "tile-component"},
    {interleave_mode_multi_y,        "multi-y"}
};


/**
 * Check for valid sampling mode.
 *
 * @param sampling the sampling value to check
 * @return true if the sampling mode is valid, or false otherwise
 */
bool is_valid_sampling_mode(uint8_t sampling)
{
  return sampling <= sampling_mode_max_valid;
}

static std::map<heif_uncompressed_sampling_mode, const char*> sNames_uncompressed_sampling_mode{
    {sampling_mode_no_subsampling, "no subsampling"},
    {sampling_mode_422,            "4:2:2"},
    {sampling_mode_420,            "4:2:0"},
    {sampling_mode_411,            "4:1:1"}
};


bool is_predefined_component_type(uint16_t type)
{
  // check whether the component type can be mapped to heif_uncompressed_component_type and we have a name defined for
  // it in sNames_uncompressed_component_type.
  return type <= component_type_max_valid;
}

static std::map<heif_uncompressed_component_type, const char*> sNames_uncompressed_component_type{
    {component_type_monochrome,   "monochrome"},
    {component_type_Y,            "Y"},
    {component_type_Cb,           "Cb"},
    {component_type_Cr,           "Cr"},
    {component_type_red,          "red"},
    {component_type_green,        "green"},
    {component_type_blue,         "blue"},
    {component_type_alpha,        "alpha"},
    {component_type_depth,        "depth"},
    {component_type_disparity,    "disparity"},
    {component_type_palette,      "palette"},
    {component_type_filter_array, "filter-array"},
    {component_type_padded,       "padded"},
    {component_type_cyan,         "cyan"},
    {component_type_magenta,      "magenta"},
    {component_type_yellow,       "yellow"},
    {component_type_key_black,    "key (black)"}
};

template <typename T> const char* get_name(T val, const std::map<T, const char*>& table)
{
  auto iter = table.find(val);
  if (iter == table.end()) {
    return "unknown";
  }
  else {
    return iter->second;
  }
}

Error Box_cmpd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint32_t component_count = range.read32();

  if (limits->max_components && component_count > limits->max_components) {
    std::stringstream sstr;
    sstr << "cmpd box should contain " << component_count << " components, but security limit is set to "
         << limits->max_components << " components";

    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            sstr.str()
    };
  }

  for (unsigned int i = 0; i < component_count ; i++) {
    if (range.eof()) {
      std::stringstream sstr;
      sstr << "cmpd box should contain " << component_count << " components, but box only contained "
           << i << " components";

      return {heif_error_Invalid_input,
              heif_suberror_End_of_data,
              sstr.str()
      };
    }

    Component component;
    component.component_type = range.read16();
    if (component.component_type >= 0x8000) {
      component.component_type_uri = range.read_string();
    }
    else {
      component.component_type_uri = std::string();
    }
    m_components.push_back(component);
  }

  return range.get_error();
}

std::string Box_cmpd::Component::get_component_type_name(uint16_t component_type)
{
  std::stringstream sstr;

  if (is_predefined_component_type(component_type)) {
    sstr << get_name(heif_uncompressed_component_type(component_type), sNames_uncompressed_component_type) << "\n";
  }
  else {
    sstr << "0x" << std::hex << component_type << std::dec << "\n";
  }

  return sstr.str();
}


bool Box_cmpd::has_component(heif_uncompressed_component_type type) const
{
  return std::any_of(m_components.begin(), m_components.end(),
                     [type](const auto& cmp) { return cmp.component_type == type; });
}


std::string Box_cmpd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  for (const auto& component : m_components) {
    sstr << indent << "component_type: " << component.get_component_type_name();

    if (component.component_type >= 0x8000) {
      sstr << indent << "| component_type_uri: " << component.component_type_uri << "\n";
    }
  }

  return sstr.str();
}

Error Box_cmpd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32((uint32_t) m_components.size());
  for (const auto& component : m_components) {
    writer.write16(component.component_type);
    if (component.component_type >= 0x8000) {
      writer.write(component.component_type_uri);
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}

Error Box_uncC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  m_profile = range.read32();

  if (get_version() == 1) {
    if (m_profile == fourcc("rgb3")) {
      Box_uncC::Component component0 = {0, 8, component_format_unsigned, 0};
      add_component(component0);
      Box_uncC::Component component1 = {1, 8, component_format_unsigned, 0};
      add_component(component1);
      Box_uncC::Component component2 = {2, 8, component_format_unsigned, 0};
      add_component(component2);
    }
    else if ((m_profile == fourcc("rgba")) || (m_profile == fourcc("abgr"))) {
      Box_uncC::Component component0 = {0, 8, component_format_unsigned, 0};
      add_component(component0);
      Box_uncC::Component component1 = {1, 8, component_format_unsigned, 0};
      add_component(component1);
      Box_uncC::Component component2 = {2, 8, component_format_unsigned, 0};
      add_component(component2);
      Box_uncC::Component component3 = {3, 8, component_format_unsigned, 0};
      add_component(component3);
    }
    else {
        return Error{heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "Invalid component format"};
    }
  } else if (get_version() == 0) {

    uint32_t component_count = range.read32();

    if (limits->max_components && component_count > limits->max_components) {
      std::stringstream sstr;
      sstr << "Number of image components (" << component_count << ") exceeds security limit ("
           << limits->max_components << ")";

      return {heif_error_Invalid_input,
              heif_suberror_Security_limit_exceeded,
              sstr.str()};
    }

    for (uint32_t i = 0; i < component_count && !range.error() && !range.eof(); i++) {
      Component component;
      component.component_index = range.read16();
      component.component_bit_depth = uint16_t(range.read8() + 1);
      component.component_format = range.read8();
      component.component_align_size = range.read8();
      m_components.push_back(component);

      if (!is_valid_component_format(component.component_format)) {
        return {heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "Invalid component format"};
      }
    }

    m_sampling_type = range.read8();
    if (!is_valid_sampling_mode(m_sampling_type)) {
      return {heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "Invalid sampling mode"};
    }

    m_interleave_type = range.read8();
    if (!is_valid_interleave_mode(m_interleave_type)) {
      return {heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "Invalid interleave mode"};
    }

    m_block_size = range.read8();

    uint8_t flags = range.read8();
    m_components_little_endian = !!(flags & 0x80);
    m_block_pad_lsb = !!(flags & 0x40);
    m_block_little_endian = !!(flags & 0x20);
    m_block_reversed = !!(flags & 0x10);
    m_pad_unknown = !!(flags & 0x08);

    m_pixel_size = range.read32();

    m_row_align_size = range.read32();

    m_tile_align_size = range.read32();

    uint32_t num_tile_cols_minus_one = range.read32();
    uint32_t num_tile_rows_minus_one = range.read32();

    if (limits->max_number_of_tiles &&
        static_cast<uint64_t>(num_tile_cols_minus_one) + 1 > limits->max_number_of_tiles / (static_cast<uint64_t>(num_tile_rows_minus_one) + 1)) {
      std::stringstream sstr;
      sstr << "Tiling size "
           << ((uint64_t)num_tile_cols_minus_one + 1) << " x " << ((uint64_t)num_tile_rows_minus_one + 1)
           << " exceeds the maximum allowed number " << limits->max_number_of_tiles << " set as security limit";
      return {heif_error_Memory_allocation_error,
              heif_suberror_Security_limit_exceeded,
              sstr.str()};
    }

    m_num_tile_cols = num_tile_cols_minus_one + 1;
    m_num_tile_rows = num_tile_rows_minus_one + 1;
  }

  return range.get_error();
}



std::string Box_uncC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  sstr << indent << "profile: " << m_profile;
  if (m_profile != 0) {
    sstr << " (" << fourcc_to_string(m_profile) << ")";
  }
  sstr << "\n";
  if (get_version() == 0) {
    for (const auto& component : m_components) {
      sstr << indent << "component_index: " << component.component_index << "\n";
      indent++;
      sstr << indent << "component_bit_depth: " << (int) component.component_bit_depth << "\n";
      sstr << indent << "component_format: " << get_name(heif_uncompressed_component_format(component.component_format), sNames_uncompressed_component_format) << "\n";
      sstr << indent << "component_align_size: " << (int) component.component_align_size << "\n";
      indent--;
    }

    sstr << indent << "sampling_type: " << get_name(heif_uncompressed_sampling_mode(m_sampling_type), sNames_uncompressed_sampling_mode) << "\n";

    sstr << indent << "interleave_type: " << get_name(heif_uncompressed_interleave_mode(m_interleave_type), sNames_uncompressed_interleave_mode) << "\n";

    sstr << indent << "block_size: " << (int) m_block_size << "\n";

    sstr << indent << "components_little_endian: " << m_components_little_endian << "\n";
    sstr << indent << "block_pad_lsb: " << m_block_pad_lsb << "\n";
    sstr << indent << "block_little_endian: " << m_block_little_endian << "\n";
    sstr << indent << "block_reversed: " << m_block_reversed << "\n";
    sstr << indent << "pad_unknown: " << m_pad_unknown << "\n";

    sstr << indent << "pixel_size: " << m_pixel_size << "\n";

    sstr << indent << "row_align_size: " << m_row_align_size << "\n";

    sstr << indent << "tile_align_size: " << m_tile_align_size << "\n";

    sstr << indent << "num_tile_cols: " << m_num_tile_cols << "\n";

    sstr << indent << "num_tile_rows: " << m_num_tile_rows << "\n";
  }
  return sstr.str();
}


Error Box_uncC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);
  writer.write32(m_profile);
  if (get_version() == 1) {
  }
  else if (get_version() == 0) {
    writer.write32((uint32_t)m_components.size());
    for (const auto &component : m_components) {
      if (component.component_bit_depth < 1 || component.component_bit_depth > 256) {
        return {heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "component bit-depth out of range [1..256]"};
      }

      writer.write16(component.component_index);
      writer.write8(uint8_t(component.component_bit_depth - 1));
      writer.write8(component.component_format);
      writer.write8(component.component_align_size);
    }
    writer.write8(m_sampling_type);
    writer.write8(m_interleave_type);
    writer.write8(m_block_size);
    uint8_t flags = 0;
    flags |= (m_components_little_endian ? 0x80 : 0);
    flags |= (m_block_pad_lsb ? 0x40 : 0);
    flags |= (m_block_little_endian ? 0x20 : 0);
    flags |= (m_block_reversed ? 0x10 : 0);
    flags |= (m_pad_unknown ? 0x08 : 0);
    writer.write8(flags);
    writer.write32(m_pixel_size);
    writer.write32(m_row_align_size);
    writer.write32(m_tile_align_size);
    writer.write32(m_num_tile_cols - 1);
    writer.write32(m_num_tile_rows - 1);
  }
  prepend_header(writer, box_start);

  return Error::Ok;
}


uint64_t Box_uncC::compute_tile_data_size_bytes(uint32_t tile_width, uint32_t tile_height) const
{
  if (m_profile != 0) {
    switch (m_profile) {
      case fourcc("rgba"):
        return 4 * uint64_t{tile_width} * tile_height;

      case fourcc("rgb3"):
        return 3 * uint64_t{tile_width} * tile_height;

      default:
        assert(false);
        return 0;
    }
  }

  switch (m_interleave_type) {
    case interleave_mode_component:
    case interleave_mode_pixel: {
      uint32_t bytes_per_pixel = 0;

      for (const auto& comp : m_components) {
        assert(comp.component_bit_depth % 8 == 0); // TODO: component sizes that are no multiples of bytes
        bytes_per_pixel += comp.component_bit_depth / 8;
      }

      return bytes_per_pixel * uint64_t{tile_width} * tile_height;
    }
    default:
      assert(false);
      return 0;
  }
}


Error Box_cmpC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("cmpC");
  }

  m_compression_type = range.read32();

  uint8_t unit_type = range.read8();
  if (unit_type > heif_cmpC_compressed_unit_type_image_pixel) {
    return {heif_error_Invalid_input,
            heif_suberror_Unsupported_parameter,
            "Unsupported cmpC compressed unit type"};
  };

  m_compressed_unit_type = static_cast<heif_cmpC_compressed_unit_type>(unit_type);

  return range.get_error();
}


std::string Box_cmpC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);
  sstr << indent << "compression_type: " << fourcc_to_string(m_compression_type) << "\n";
  sstr << indent << "compressed_entity_type: " << (int)m_compressed_unit_type << "\n";
  return sstr.str();
}

Error Box_cmpC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(m_compression_type);
  writer.write8(m_compressed_unit_type);

  prepend_header(writer, box_start);

  return Error::Ok;
}


static uint8_t unit_offset_bits_table[] = {0, 16, 24, 32, 64 };
static uint8_t unit_size_bits_table[] = {8, 16, 24, 32, 64 };

Error Box_icef::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("icef");
  }
  uint8_t codes = range.read8();
  uint8_t unit_offset_code = (codes & 0b11100000) >> 5;
  uint8_t unit_size_code = (codes & 0b00011100) >> 2;
  uint32_t num_compressed_units = range.read32();
  uint64_t implied_offset = 0;

  if (unit_offset_code > 4) {
    return {heif_error_Usage_error, heif_suberror_Unsupported_parameter, "Unsupported icef unit offset code"};
  }

  if (unit_size_code > 4) {
    return {heif_error_Usage_error, heif_suberror_Unsupported_parameter, "Unsupported icef unit size code"};
  }

  // --- precompute fields lengths

  uint8_t unit_offset_bits = unit_offset_bits_table[unit_offset_code];
  uint8_t unit_size_bits = unit_size_bits_table[unit_size_code];

  // --- check if box is large enough for all the data

  uint64_t data_size_bytes = static_cast<uint64_t>(num_compressed_units) * (unit_offset_bits + unit_size_bits) / 8;
  if (data_size_bytes > range.get_remaining_bytes()) {
    uint64_t contained_units = range.get_remaining_bytes() / ((unit_offset_bits + unit_size_bits) * 8);
    std::stringstream sstr;
    sstr << "icef box declares " << num_compressed_units << " units, but only " << contained_units
         << " were contained in the file";
    return {heif_error_Invalid_input,
            heif_suberror_End_of_data,
            sstr.str()};
  }

  // TODO: should we impose some security limit?

  // --- read box content

  m_unit_infos.resize(num_compressed_units);

  for (uint32_t r = 0; r < num_compressed_units; r++) {
    struct CompressedUnitInfo unitInfo;
    if (unit_offset_code == 0) {
      unitInfo.unit_offset = implied_offset;
    } else {
      unitInfo.unit_offset = range.read_uint(unit_offset_bits);
    }

    unitInfo.unit_size = range.read_uint(unit_size_bits);

    if (unitInfo.unit_size >= UINT64_MAX - implied_offset) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_parameter_value,
              "cumulative offsets too large for 64 bit file size"};
    }

    implied_offset += unitInfo.unit_size;

    if (range.get_error() != Error::Ok) {
      return range.get_error();
    }

    m_unit_infos[r] = unitInfo;
  }

  return range.get_error();
}


std::string Box_icef::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);
  sstr << indent << "num_compressed_units: " << m_unit_infos.size() << "\n";
  for (CompressedUnitInfo unit_info: m_unit_infos) {
    sstr << indent << "unit_offset: " << unit_info.unit_offset << ", unit_size: " << unit_info.unit_size << "\n";
  }
  return sstr.str();
}

Error Box_icef::write(StreamWriter& writer) const
{
  // check that all units have a non-zero size

  for (const CompressedUnitInfo& unit_info: m_unit_infos) {
    if (unit_info.unit_size == 0) {
      return {
        heif_error_Usage_error,
        heif_suberror_Unspecified,
        "tiled 'unci' image has an undefined tile."
      };
    }
  }

  size_t box_start = reserve_box_header_space(writer);

  uint8_t unit_offset_code = 1;
  uint8_t unit_size_code = 0;
  uint64_t implied_offset = 0;
  bool can_use_implied_offsets = true;
  for (const CompressedUnitInfo& unit_info: m_unit_infos) {
    if (unit_info.unit_offset != implied_offset) {
      can_use_implied_offsets = false;
    }
    if (unit_info.unit_size > (std::numeric_limits<uint64_t>::max() - implied_offset)) {
      can_use_implied_offsets = false;
    } else {
      implied_offset += unit_info.unit_size;
    }
    uint8_t required_offset_code = get_required_offset_code(unit_info.unit_offset);
    if (required_offset_code > unit_offset_code) {
      unit_offset_code = required_offset_code;
    }
    uint8_t required_size_code = get_required_size_code(unit_info.unit_size);
    if (required_size_code > unit_size_code) {
      unit_size_code = required_size_code;
    }
  }
  if (can_use_implied_offsets) {
    unit_offset_code = 0;
  }
  uint8_t code_bits = (uint8_t)((unit_offset_code << 5) | (unit_size_code << 2));
  writer.write8(code_bits);
  writer.write32((uint32_t)m_unit_infos.size());
  for (CompressedUnitInfo unit_info: m_unit_infos) {
    if (unit_offset_code == 0) {
      // nothing
    } else if (unit_offset_code == 1) {
      writer.write16((uint16_t)unit_info.unit_offset);
    } else if (unit_offset_code == 2) {
      writer.write24((uint32_t)unit_info.unit_offset);
    } else if (unit_offset_code == 3) {
      writer.write32((uint32_t)unit_info.unit_offset);
    } else {
      writer.write64(unit_info.unit_offset);
    }
    if (unit_size_code == 0) {
      writer.write8((uint8_t)unit_info.unit_size);
    } else if (unit_size_code == 1) {
      writer.write16((uint16_t)unit_info.unit_size);
    } else if (unit_size_code == 2) {
      writer.write24((uint32_t)unit_info.unit_size);
    } else if (unit_size_code == 3) {
      writer.write32((uint32_t)unit_info.unit_size);
    } else {
      writer.write64(unit_info.unit_size);
    }
  }
  prepend_header(writer, box_start);

  return Error::Ok;
}

static uint64_t MAX_OFFSET_UNIT_CODE_1 = std::numeric_limits<uint16_t>::max();
static uint64_t MAX_OFFSET_UNIT_CODE_2 = (1ULL << 24) - 1;
static uint64_t MAX_OFFSET_UNIT_CODE_3 = std::numeric_limits<uint32_t>::max();

const uint8_t Box_icef::get_required_offset_code(uint64_t offset) const
{
  if (offset <= MAX_OFFSET_UNIT_CODE_1) {
    return 1;
  }
  if (offset <= MAX_OFFSET_UNIT_CODE_2) {
    return 2;
  }
  if (offset <= MAX_OFFSET_UNIT_CODE_3) {
    return 3;
  }
  return 4;
}

static uint64_t MAX_SIZE_UNIT_CODE_0 = std::numeric_limits<uint8_t>::max();
static uint64_t MAX_SIZE_UNIT_CODE_1 = std::numeric_limits<uint16_t>::max();
static uint64_t MAX_SIZE_UNIT_CODE_2 = (1ULL << 24) - 1;
static uint64_t MAX_SIZE_UNIT_CODE_3 = std::numeric_limits<uint32_t>::max();

const uint8_t Box_icef::get_required_size_code(uint64_t size) const
{
  if (size <= MAX_SIZE_UNIT_CODE_0) {
    return 0;
  }
  if (size <= MAX_SIZE_UNIT_CODE_1) {
    return 1;
  }
  if (size <= MAX_SIZE_UNIT_CODE_2) {
    return 2;
  }
  if (size <= MAX_SIZE_UNIT_CODE_3) {
    return 3;
  }
  return 4;
}


Error Box_cpat::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("cpat");
  }

  m_pattern_width = range.read16();
  m_pattern_height = range.read16();

  if (m_pattern_width == 0 || m_pattern_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_parameter_value,
            "Zero Bayer pattern size."};
  }

  auto max_bayer_pattern_size = limits->max_bayer_pattern_pixels;
  if (max_bayer_pattern_size && m_pattern_height > max_bayer_pattern_size / m_pattern_width) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "Maximum Bayer pattern size exceeded."};
  }

  m_components.resize(size_t{m_pattern_width} * m_pattern_height);

  for (uint16_t i = 0; i < m_pattern_height; i++) {
    for (uint16_t j = 0; j < m_pattern_width; j++) {
      PatternComponent component{};
      component.component_index = range.read32();
      component.component_gain = range.read_float32();
      m_components[i] = component;
    }
  }

  return range.get_error();
}


std::string Box_cpat::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << FullBox::dump(indent);
  sstr << indent << "pattern_width: " << get_pattern_width() << "\n";
  sstr << indent << "pattern_height: " << get_pattern_height() << "\n";

  for (const auto& component : m_components) {
    sstr << indent << "component index: " << component.component_index << ", gain: " << component.component_gain << "\n";
  }
  return sstr.str();
}


Error Box_cpat::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (m_pattern_width * size_t{m_pattern_height} != m_components.size()) {
    // needs to be rectangular
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "incorrect number of pattern components"};
  }

  writer.write16(m_pattern_width);
  writer.write16(m_pattern_height);

  for (const auto& component : m_components) {
    writer.write32(component.component_index);
    writer.write_float32(component.component_gain);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}
