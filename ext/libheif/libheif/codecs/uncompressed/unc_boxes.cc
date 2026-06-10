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
    {component_format_signed,   "signed"},
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
  return type <= heif_cmpd_component_type_max_valid;
}

static std::map<heif_cmpd_component_type, const char*> sNames_uncompressed_component_type{
    {heif_cmpd_component_type_monochrome,   "monochrome"},
    {heif_cmpd_component_type_Y,            "Y"},
    {heif_cmpd_component_type_Cb,           "Cb"},
    {heif_cmpd_component_type_Cr,           "Cr"},
    {heif_cmpd_component_type_red,          "red"},
    {heif_cmpd_component_type_green,        "green"},
    {heif_cmpd_component_type_blue,         "blue"},
    {heif_cmpd_component_type_alpha,        "alpha"},
    {heif_cmpd_component_type_depth,        "depth"},
    {heif_cmpd_component_type_disparity,    "disparity"},
    {heif_cmpd_component_type_palette,      "palette"},
    {heif_cmpd_component_type_filter_array, "filter-array"},
    {heif_cmpd_component_type_padded,       "padded"},
    {heif_cmpd_component_type_cyan,         "cyan"},
    {heif_cmpd_component_type_magenta,      "magenta"},
    {heif_cmpd_component_type_yellow,       "yellow"},
    {heif_cmpd_component_type_key_black,    "key (black)"}
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

void Box_cmpd::set_components(const std::vector<uint16_t>& components)
{
  m_components.clear();

  for (const auto& component : components) {
    m_components.push_back({component, {}});
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
    sstr << get_name(heif_cmpd_component_type(component_type), sNames_uncompressed_component_type) << "\n";
  }
  else {
    sstr << "0x" << std::hex << component_type << std::dec << "\n";
  }

  return sstr.str();
}


bool Box_cmpd::has_component(heif_cmpd_component_type type) const
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
    switch (m_profile) {
      case fourcc("rgb3"):
      case fourcc("rgba"):
      case fourcc("abgr"):
      case fourcc("2vuy"):
      case fourcc("yuv2"):
      case fourcc("yvyu"):
      case fourcc("vyuy"):
      case fourcc("yuv1"):
      case fourcc("v308"):
      case fourcc("v408"):
      case fourcc("y210"):
      case fourcc("v410"):
      case fourcc("v210"):
      case fourcc("i420"):
      case fourcc("nv12"):
      case fourcc("nv21"):
      case fourcc("yu22"):
      case fourcc("yv22"):
      case fourcc("yv20"):
        break;
      default:
        return Error{heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "Unknown uncC v1 profile"};
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

      // When component_align_size != 0, the component is padded up to that many bytes.
      // It therefore must be large enough to hold the component's bit depth; otherwise
      // the decoder computes a negative pad-bits count and shifts by a negative amount.
      if (component.component_align_size != 0 &&
          uint32_t(component.component_align_size) * 8 < component.component_bit_depth) {
        std::stringstream sstr;
        sstr << "Component alignment (" << int(component.component_align_size)
             << " bytes) is too small for component bit depth (" << component.component_bit_depth << " bits)";
        return {heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, sstr.str()};
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

    if (limits->version >= 4 &&
        limits->max_iso23001_17_pixel_size_bytes != 0 &&
        m_pixel_size > limits->max_iso23001_17_pixel_size_bytes) {
      std::stringstream sstr;
      sstr << "uncC pixel_size (" << m_pixel_size << " bytes) exceeds security limit of "
           << limits->max_iso23001_17_pixel_size_bytes << " bytes";
      return {heif_error_Memory_allocation_error,
              heif_suberror_Security_limit_exceeded,
              sstr.str()};
    }

    m_row_align_size = range.read32();

    m_tile_align_size = range.read32();

    uint32_t num_tile_cols_minus_one = range.read32();
    uint32_t num_tile_rows_minus_one = range.read32();

    // The field is stored as `count - 1`, so 0xFFFFFFFF would mean 2^32 tiles,
    // which we cannot represent in our uint32 m_num_tile_cols/rows. Reject this
    // before the security-limit check so that disabling the security limit does
    // not silently turn this into a divide-by-zero in get_heif_image_tiling().
    if (num_tile_cols_minus_one == 0xFFFFFFFF || num_tile_rows_minus_one == 0xFFFFFFFF) {
      return {heif_error_Unsupported_feature,
              heif_suberror_Invalid_parameter_value,
              "uncC num_tile_cols/rows_minus_one of 0xFFFFFFFF (2^32 tiles) exceeds the supported range"};
    }

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

      if (component.component_index > 0xFFFF) {
        return {heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "component index must be 16 bit"};
      }
      writer.write16(static_cast<uint16_t>(component.component_index));
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


void fill_uncC_and_cmpd_from_profile(const std::shared_ptr<Box_uncC>& uncC,
                                     std::shared_ptr<Box_cmpd>& cmpd)
{
  if (uncC->get_version() != 1 || cmpd) {
    return;
  }

  // Return cached synthetic cmpd if we already created one.
  if (auto synthetic = uncC->get_synthetic_cmpd()) {
    cmpd = synthetic;
    return;
  }

  uint32_t profile = uncC->get_profile();
  cmpd = std::make_shared<Box_cmpd>();

  // Profiles from ISO/IEC 23001-17 Table 5.
  // Format: {profile, [{component_type, bit_depth_minus_1}, ...], sampling_type, interleave_type}
  // The implicit cmpd is the unique component_types in order of first appearance.
  // The uncC component_index refers into that cmpd.

  if (profile == fourcc("rgb3")) {
    // {'rgb3', [{4,7},{5,7},{6,7}], 0, 1}
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_red});
    cmpd->add_component({heif_cmpd_component_type_green});
    cmpd->add_component({heif_cmpd_component_type_blue});
    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_pixel);
  }
  else if (profile == fourcc("rgba")) {
    // {'rgba', [{4,7},{5,7},{6,7},{7,7}], 0, 1}
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    uncC->add_component({3, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_red});
    cmpd->add_component({heif_cmpd_component_type_green});
    cmpd->add_component({heif_cmpd_component_type_blue});
    cmpd->add_component({heif_cmpd_component_type_alpha});
    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_pixel);
  }
  else if (profile == fourcc("abgr")) {
    // {'abgr', [{7,7},{6,7},{5,7},{4,7}], 0, 1}
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    uncC->add_component({3, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_alpha});
    cmpd->add_component({heif_cmpd_component_type_blue});
    cmpd->add_component({heif_cmpd_component_type_green});
    cmpd->add_component({heif_cmpd_component_type_red});
    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_pixel);
  }
  else if (profile == fourcc("2vuy")) {
    // {'2vuy', [{2,7},{1,7},{3,7},{1,7}], 1, 5}  Cb Y0 Cr Y1
    // cmpd: Cb(0) Y(1) Cr(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_multi_y);
  }
  else if (profile == fourcc("yuv2")) {
    // {'yuv2', [{1,7},{2,7},{1,7},{3,7}], 1, 5}  Y0 Cb Y1 Cr
    // cmpd: Y(0) Cb(1) Cr(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_multi_y);
  }
  else if (profile == fourcc("yvyu")) {
    // {'yvyu', [{1,7},{3,7},{1,7},{2,7}], 1, 5}  Y0 Cr Y1 Cb
    // cmpd: Y(0) Cr(1) Cb(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_multi_y);
  }
  else if (profile == fourcc("vyuy")) {
    // {'vyuy', [{3,7},{1,7},{2,7},{1,7}], 1, 5}  Cr Y0 Cb Y1
    // cmpd: Cr(0) Y(1) Cb(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_multi_y);
  }
  else if (profile == fourcc("yuv1")) {
    // {'yuv1', [{1,7},{1,7},{2,7},{1,7},{1,7},{3,7}], 3, 5}  Y0 Y1 Cb Y2 Y3 Cr
    // cmpd: Y(0) Cb(1) Cr(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_411);
    uncC->set_interleave_type(interleave_mode_multi_y);
  }
  else if (profile == fourcc("v308")) {
    // {'v308', [{3,7},{1,7},{2,7}], 0, 1}  Cr Y Cb
    // cmpd: Cr(0) Y(1) Cb(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_pixel);
  }
  else if (profile == fourcc("v408")) {
    // {'v408', [{2,7},{1,7},{3,7},{7,7}], 0, 1}  Cb Y Cr A
    // cmpd: Cb(0) Y(1) Cr(2) alpha(3)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    uncC->add_component({3, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_alpha});
    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_pixel);
  }
  else if (profile == fourcc("y210")) {
    // {'y210', [{1,9},{2,9},{1,9},{3,9}], 1, 5}  Y0 Cb Y1 Cr
    // block_size=2, block_little_endian=1, block_pad_lsb=1
    // cmpd: Y(0) Cb(1) Cr(2)
    uncC->add_component({0, 10, component_format_unsigned, 0});
    uncC->add_component({1, 10, component_format_unsigned, 0});
    uncC->add_component({0, 10, component_format_unsigned, 0});
    uncC->add_component({2, 10, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_multi_y);
    uncC->set_block_size(2);
    uncC->set_block_little_endian(true);
    uncC->set_block_pad_lsb(true);
  }
  else if (profile == fourcc("v410")) {
    // {'v410', [{2,9},{1,9},{3,9}], 0, 1}  Cb Y Cr
    // block_size=4, block_little_endian=1, block_pad_lsb=1, block_reversed=1
    // cmpd: Cb(0) Y(1) Cr(2)
    uncC->add_component({0, 10, component_format_unsigned, 0});
    uncC->add_component({1, 10, component_format_unsigned, 0});
    uncC->add_component({2, 10, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_pixel);
    uncC->set_block_size(4);
    uncC->set_block_little_endian(true);
    uncC->set_block_pad_lsb(true);
    uncC->set_block_reversed(true);
  }
  else if (profile == fourcc("v210")) {
    // {'v210', [{2,9},{1,9},{3,9},{1,9}], 1, 5}  Cb Y0 Cr Y1
    // block_size=4, block_little_endian=1, block_reversed=1
    // cmpd: Cb(0) Y(1) Cr(2)
    uncC->add_component({0, 10, component_format_unsigned, 0});
    uncC->add_component({1, 10, component_format_unsigned, 0});
    uncC->add_component({2, 10, component_format_unsigned, 0});
    uncC->add_component({1, 10, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_multi_y);
    uncC->set_block_size(4);
    uncC->set_block_little_endian(true);
    uncC->set_block_reversed(true);
  }
  else if (profile == fourcc("i420")) {
    // {'i420', [{1,7},{2,7},{3,7}], 2, 0}  planar YCbCr
    // cmpd: Y(0) Cb(1) Cr(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_420);
    uncC->set_interleave_type(interleave_mode_component);
  }
  else if (profile == fourcc("nv12")) {
    // {'nv12', [{1,7},{2,7},{3,7}], 2, 2}  semi-planar YCbCr
    // cmpd: Y(0) Cb(1) Cr(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_420);
    uncC->set_interleave_type(interleave_mode_mixed);
  }
  else if (profile == fourcc("nv21")) {
    // {'nv21', [{1,7},{3,7},{2,7}], 2, 2}  semi-planar YCrCb
    // cmpd: Y(0) Cr(1) Cb(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    uncC->set_sampling_type(sampling_mode_420);
    uncC->set_interleave_type(interleave_mode_mixed);
  }
  else if (profile == fourcc("yu22")) {
    // {'yu22', [{1,7},{2,7},{3,7}], 1, 0}  planar YCbCr
    // cmpd: Y(0) Cb(1) Cr(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_component);
  }
  else if (profile == fourcc("yv22")) {
    // {'yv22', [{1,7},{3,7},{2,7}], 1, 0}  planar YCrCb
    // cmpd: Y(0) Cr(1) Cb(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    uncC->set_sampling_type(sampling_mode_422);
    uncC->set_interleave_type(interleave_mode_component);
  }
  else if (profile == fourcc("yv20")) {
    // {'yv20', [{1,7},{3,7},{2,7}], 2, 0}  planar YCrCb
    // cmpd: Y(0) Cr(1) Cb(2)
    uncC->add_component({0, 8, component_format_unsigned, 0});
    uncC->add_component({1, 8, component_format_unsigned, 0});
    uncC->add_component({2, 8, component_format_unsigned, 0});
    cmpd->add_component({heif_cmpd_component_type_Y});
    cmpd->add_component({heif_cmpd_component_type_Cr});
    cmpd->add_component({heif_cmpd_component_type_Cb});
    uncC->set_sampling_type(sampling_mode_420);
    uncC->set_interleave_type(interleave_mode_component);
  }
  else {
    cmpd.reset();
    return;
  }

  uncC->set_synthetic_cmpd(cmpd);
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
    CompressedUnitInfo unitInfo;
    if (unit_offset_code == 0) {
      unitInfo.unit_offset = implied_offset;
    } else {
      unitInfo.unit_offset = range.read_uint(unit_offset_bits);
    }

    unitInfo.unit_size = range.read_uint(unit_size_bits);

    // Reject unit_offset + unit_size wrapping past the 64 bit range.
    if (unitInfo.unit_size >= UINT64_MAX - unitInfo.unit_offset) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_parameter_value,
              "icef unit offset + size exceeds 64 bit range"};
    }

    // implied_offset is only used as unit_offset when unit_offset_code==0.
    // Accumulating it for other offset codes would be unused and could overflow, so only do it if needed.
    if (unit_offset_code == 0) {
      implied_offset += unitInfo.unit_size;
    }

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

  m_pattern.pattern_width = range.read16();
  m_pattern.pattern_height = range.read16();

  if (m_pattern.pattern_width == 0 || m_pattern.pattern_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_parameter_value,
            "Zero Bayer pattern size."};
  }

  auto max_bayer_pattern_size = limits->max_bayer_pattern_pixels;
  if (max_bayer_pattern_size && m_pattern.pattern_height > max_bayer_pattern_size / m_pattern.pattern_width) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "Maximum Bayer pattern size exceeded."};
  }

  size_t num_pixels = size_t{m_pattern.pattern_width} * m_pattern.pattern_height;
  m_pattern.pixels.resize(num_pixels);

  for (size_t i = 0; i < num_pixels; i++) {
    BayerPatternPixelCmpd pixel{};
    pixel.cmpd_index = range.read32();
    pixel.component_gain = range.read_float32();
    m_pattern.pixels[i] = pixel;
  }

  return range.get_error();
}


std::string Box_cpat::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << FullBox::dump(indent);
  sstr << indent << "pattern_width: " << get_pattern_width() << "\n";
  sstr << indent << "pattern_height: " << get_pattern_height() << "\n";

  for (const auto& pixel : m_pattern.pixels) {
    sstr << indent << "component index: " << pixel.cmpd_index << ", gain: " << pixel.component_gain << "\n";
  }
  return sstr.str();
}


Error Box_cpat::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (m_pattern.pattern_width * size_t{m_pattern.pattern_height} != m_pattern.pixels.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "incorrect number of pattern components"};
  }

  writer.write16(m_pattern.pattern_width);
  writer.write16(m_pattern.pattern_height);

  for (const auto& pixel : m_pattern.pixels) {
    writer.write32(pixel.cmpd_index);
    writer.write_float32(pixel.component_gain);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_splz::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("splz");
  }

  uint32_t component_count = range.read32();
  if (limits->max_components && component_count > limits->max_components) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "Number of components in splz box exceeds the security limits."};
  }

  m_pattern.component_ids.resize(component_count);
  for (uint32_t i = 0; i < component_count; i++) {
    m_pattern.component_ids[i] = range.read32();
  }

  m_pattern.pattern_width = range.read16();
  m_pattern.pattern_height = range.read16();

  if (m_pattern.pattern_width == 0 || m_pattern.pattern_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_parameter_value,
            "Zero polarization pattern size."};
  }

  auto max_pattern_size = limits->max_bayer_pattern_pixels;
  if (max_pattern_size && m_pattern.pattern_height > max_pattern_size / m_pattern.pattern_width) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "Maximum polarization pattern size exceeded."};
  }

  size_t num_pixels = size_t{m_pattern.pattern_width} * m_pattern.pattern_height;
  m_pattern.polarization_angles.resize(num_pixels);

  for (size_t i = 0; i < num_pixels; i++) {
    m_pattern.polarization_angles[i] = range.read_float32();
  }

  return range.get_error();
}


std::string Box_splz::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << FullBox::dump(indent);

  sstr << indent << "component_count: " << m_pattern.component_ids.size() << "\n";
  for (size_t i = 0; i < m_pattern.component_ids.size(); i++) {
    sstr << indent << "  component_index[" << i << "]: " << m_pattern.component_ids[i] << "\n";
  }

  sstr << indent << "pattern_width: " << m_pattern.pattern_width << "\n";
  sstr << indent << "pattern_height: " << m_pattern.pattern_height << "\n";

  for (uint16_t y = 0; y < m_pattern.pattern_height; y++) {
    for (uint16_t x = 0; x < m_pattern.pattern_width; x++) {
      float angle = m_pattern.polarization_angles[y * m_pattern.pattern_width + x];
      if (heif_polarization_angle_is_no_filter(angle)) {
        sstr << indent << "  [" << x << "," << y << "]: no filter\n";
      }
      else {
        sstr << indent << "  [" << x << "," << y << "]: " << angle << " degrees\n";
      }
    }
  }

  return sstr.str();
}


Error Box_splz::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (m_pattern.pattern_width * size_t{m_pattern.pattern_height} != m_pattern.polarization_angles.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "incorrect number of polarization pattern angles"};
  }

  writer.write32(static_cast<uint32_t>(m_pattern.component_ids.size()));
  for (uint32_t idx : m_pattern.component_ids) {
    writer.write32(idx);
  }

  writer.write16(m_pattern.pattern_width);
  writer.write16(m_pattern.pattern_height);

  for (float angle : m_pattern.polarization_angles) {
    writer.write_float32(angle);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_sbpm::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("sbpm");
  }

  uint32_t component_count = range.read32();

  if (limits->max_components && component_count > limits->max_components) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "sbpm component_count exceeds security limit."};
  }

  m_map.component_ids.resize(component_count);
  for (uint32_t i = 0; i < component_count; i++) {
    m_map.component_ids[i] = range.read32();
  }

  uint8_t flags = range.read8();
  m_map.correction_applied = !!(flags & 0x80);

  uint32_t num_bad_rows = range.read32();
  uint32_t num_bad_cols = range.read32();
  uint32_t num_bad_pixels = range.read32();

  // Security check: limit total number of entries
  uint64_t total_entries = static_cast<uint64_t>(num_bad_rows) + num_bad_cols + num_bad_pixels;
  if (limits->max_bad_pixels && total_entries > limits->max_bad_pixels) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "sbpm total bad pixel entries exceed security limit."};
  }

  m_map.bad_rows.resize(num_bad_rows);
  for (uint32_t i = 0; i < num_bad_rows; i++) {
    m_map.bad_rows[i] = range.read32();
  }

  m_map.bad_columns.resize(num_bad_cols);
  for (uint32_t i = 0; i < num_bad_cols; i++) {
    m_map.bad_columns[i] = range.read32();
  }

  m_map.bad_pixels.resize(num_bad_pixels);
  for (uint32_t i = 0; i < num_bad_pixels; i++) {
    m_map.bad_pixels[i].row = range.read32();
    m_map.bad_pixels[i].column = range.read32();
  }

  return range.get_error();
}


std::string Box_sbpm::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << FullBox::dump(indent);

  sstr << indent << "component_count: " << m_map.component_ids.size() << "\n";
  for (size_t i = 0; i < m_map.component_ids.size(); i++) {
    sstr << indent << "  component_index[" << i << "]: " << m_map.component_ids[i] << "\n";
  }

  sstr << indent << "correction_applied: " << m_map.correction_applied << "\n";

  sstr << indent << "num_bad_rows: " << m_map.bad_rows.size() << "\n";
  for (size_t i = 0; i < m_map.bad_rows.size(); i++) {
    sstr << indent << "  bad_row[" << i << "]: " << m_map.bad_rows[i] << "\n";
  }

  sstr << indent << "num_bad_columns: " << m_map.bad_columns.size() << "\n";
  for (size_t i = 0; i < m_map.bad_columns.size(); i++) {
    sstr << indent << "  bad_column[" << i << "]: " << m_map.bad_columns[i] << "\n";
  }

  sstr << indent << "num_bad_pixels: " << m_map.bad_pixels.size() << "\n";
  for (size_t i = 0; i < m_map.bad_pixels.size(); i++) {
    sstr << indent << "  bad_pixel[" << i << "]: row=" << m_map.bad_pixels[i].row
         << ", column=" << m_map.bad_pixels[i].column << "\n";
  }

  return sstr.str();
}


Error Box_sbpm::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_map.component_ids.size()));
  for (uint32_t idx : m_map.component_ids) {
    writer.write32(idx);
  }

  uint8_t flags = m_map.correction_applied ? 0x80 : 0;
  writer.write8(flags);

  writer.write32(static_cast<uint32_t>(m_map.bad_rows.size()));
  writer.write32(static_cast<uint32_t>(m_map.bad_columns.size()));
  writer.write32(static_cast<uint32_t>(m_map.bad_pixels.size()));

  for (uint32_t row : m_map.bad_rows) {
    writer.write32(row);
  }

  for (uint32_t col : m_map.bad_columns) {
    writer.write32(col);
  }

  for (const auto& pixel : m_map.bad_pixels) {
    writer.write32(pixel.row);
    writer.write32(pixel.column);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_snuc::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("snuc");
  }

  uint32_t component_count = range.read32();

  if (limits->max_components && component_count > limits->max_components) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "snuc component_count exceeds security limit."};
  }

  m_nuc.component_ids.resize(component_count);
  for (uint32_t i = 0; i < component_count; i++) {
    m_nuc.component_ids[i] = range.read32();
  }

  uint8_t flags = range.read8();
  m_nuc.nuc_is_applied = !!(flags & 0x80);

  m_nuc.image_width = range.read32();
  m_nuc.image_height = range.read32();

  if (m_nuc.image_width == 0 || m_nuc.image_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_parameter_value,
            "snuc image width and height must be non-zero."};
  }

  uint64_t num_pixels = static_cast<uint64_t>(m_nuc.image_width) * m_nuc.image_height;

  if (limits->max_image_size_pixels && num_pixels > limits->max_image_size_pixels) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "snuc image dimensions exceed security limit."};
  }

  Error err = m_memory_handle.alloc(num_pixels, 2 * sizeof(float), limits, "snuc box");
  if (err) {
    return err;
  }

  m_nuc.nuc_gains.resize(num_pixels);
  for (uint64_t i = 0; i < num_pixels; i++) {
    m_nuc.nuc_gains[i] = range.read_float32();
  }

  m_nuc.nuc_offsets.resize(num_pixels);
  for (uint64_t i = 0; i < num_pixels; i++) {
    m_nuc.nuc_offsets[i] = range.read_float32();
  }

  return range.get_error();
}


std::string Box_snuc::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << FullBox::dump(indent);

  sstr << indent << "component_count: " << m_nuc.component_ids.size() << "\n";
  for (size_t i = 0; i < m_nuc.component_ids.size(); i++) {
    sstr << indent << "  component_index[" << i << "]: " << m_nuc.component_ids[i] << "\n";
  }

  sstr << indent << "nuc_is_applied: " << m_nuc.nuc_is_applied << "\n";
  sstr << indent << "image_width: " << m_nuc.image_width << "\n";
  sstr << indent << "image_height: " << m_nuc.image_height << "\n";

  uint64_t num_pixels = static_cast<uint64_t>(m_nuc.image_width) * m_nuc.image_height;
  sstr << indent << "nuc_gains: " << num_pixels << " values\n";
  sstr << indent << "nuc_offsets: " << num_pixels << " values\n";

  return sstr.str();
}


Error Box_snuc::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_nuc.component_ids.size()));
  for (uint32_t idx : m_nuc.component_ids) {
    writer.write32(idx);
  }

  uint8_t flags = m_nuc.nuc_is_applied ? 0x80 : 0;
  writer.write8(flags);

  writer.write32(m_nuc.image_width);
  writer.write32(m_nuc.image_height);

  for (float gain : m_nuc.nuc_gains) {
    writer.write_float32(gain);
  }

  for (float offset : m_nuc.nuc_offsets) {
    writer.write_float32(offset);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_cloc::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() != 0) {
    return unsupported_version_error("cloc");
  }

  m_chroma_location = range.read8();

  if (m_chroma_location > 6) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_parameter_value,
            "cloc chroma_location value out of range (must be 0-6)."};
  }

  return range.get_error();
}


std::string Box_cloc::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << FullBox::dump(indent);

  static const char* location_names[] = {
    "h=0,   v=0.5",   // 0
    "h=0.5, v=0.5",   // 1
    "h=0,   v=0",     // 2
    "h=0.5, v=0",     // 3
    "h=0,   v=1",     // 4
    "h=0.5, v=1",     // 5
    "Cr:0,0 / Cb:1,0" // 6
  };

  sstr << indent << "chroma_location: " << static_cast<int>(m_chroma_location);
  if (m_chroma_location <= 6) {
    sstr << " (" << location_names[m_chroma_location] << ")";
  }
  sstr << "\n";

  return sstr.str();
}


Error Box_cloc::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write8(m_chroma_location);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_gimi_component_content_ids::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint32_t number_of_components = range.read32();

  if (limits->max_components && number_of_components > limits->max_components) {
    std::stringstream sstr;
    sstr << "GIMI component content IDs box contains " << number_of_components
         << " components, but security limit is set to " << limits->max_components << " components";
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};
  }

  for (uint32_t i = 0; i < number_of_components; i++) {
    if (range.eof()) {
      return {heif_error_Invalid_input,
              heif_suberror_End_of_data,
              "Not enough data for all component content IDs"};
    }
    m_content_ids.push_back(range.read_string());
  }

  return range.get_error();
}


Error Box_gimi_component_content_ids::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_content_ids.size()));

  for (const auto& id : m_content_ids) {
    writer.write(id);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_gimi_component_content_ids::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  for (size_t i = 0; i < m_content_ids.size(); i++) {
    sstr << indent << "[" << i << "] content ID: " << m_content_ids[i] << "\n";
  }

  return sstr.str();
}
