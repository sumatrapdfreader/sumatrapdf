/*
 * HEIF VVC codec.
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

#include "vvc_boxes.h"
#include "hevc_boxes.h"
#include "file.h"
#include <cstring>
#include <string>
#include <cassert>
#include <iomanip>
#include <utility>
#include "api_structs.h"


Error Box_vvcC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  uint8_t byte;

  auto& c = m_configuration; // abbreviation

  byte = range.read8();

  c.LengthSizeMinusOne = (byte >> 1) & 3;
  c.ptl_present_flag = !!(byte & 1);

  if (c.ptl_present_flag) {
    uint16_t word = range.read16();
    c.ols_idx = (word >> 7) & 0x1FF;
    c.num_sublayers = (word >> 4) & 0x07;
    c.constant_frame_rate = (word >> 2) & 0x03;
    c.chroma_format_idc = word & 0x03;

    byte = range.read8();
    c.bit_depth_minus8 = (byte >> 5) & 0x07;

    // VvcPTLRecord

    auto& ptl = c.native_ptl; // abbreviation

    byte = range.read8();
    ptl.num_bytes_constraint_info = byte & 0x3f;

    if (ptl.num_bytes_constraint_info == 0) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_parameter_value,
              "vvcC with num_bytes_constraint_info==0 is not allowed."};
    }

    byte = range.read8();
    ptl.general_profile_idc = (byte >> 1) & 0x7f;
    ptl.general_tier_flag = (byte & 1);

    ptl.general_level_idc = range.read8();

    for (int i = 0; i < ptl.num_bytes_constraint_info; i++) {
      byte = range.read8();
      if (i == 0) {
        ptl.ptl_frame_only_constraint_flag = (byte >> 7) & 1;
        ptl.ptl_multi_layer_enabled_flag = (byte >> 6) & 1;
        byte &= 0x3f;
      }

      ptl.general_constraint_info.push_back(byte);
    }

    if (c.num_sublayers > 1) {
      ptl.ptl_sublayer_level_present_flag.resize(c.num_sublayers - 1);

      byte = range.read8();
      uint8_t mask = 0x80;

      for (int i = c.num_sublayers - 2; i >= 0; i--) {
        ptl.ptl_sublayer_level_present_flag[i] = !!(byte & mask);
        mask >>= 1;
      }
    }

    ptl.sublayer_level_idc.resize(c.num_sublayers);
    if (c.num_sublayers > 0) {
      ptl.sublayer_level_idc[c.num_sublayers - 1] = ptl.general_level_idc;

      for (int i = c.num_sublayers - 2; i >= 0; i--) {
        if (ptl.ptl_sublayer_level_present_flag[i]) {
          ptl.sublayer_level_idc[i] = range.read8();
        }
        else {
          ptl.sublayer_level_idc[i] = ptl.sublayer_level_idc[i + 1];
        }
      }
    }

    uint8_t ptl_num_sub_profiles = range.read8();
    for (int j=0; j < ptl_num_sub_profiles; j++) {
      ptl.general_sub_profile_idc.push_back(range.read32());
    }


    // remaining fields

    c.max_picture_width = range.read16();
    c.max_picture_height = range.read16();
    c.avg_frame_rate = range.read16();
  }
  else {
    return Error{
      heif_error_Unsupported_feature,
      heif_suberror_Unspecified,
      "Reading vvcC configuration with ptl_present_flag=0 is not supported."
    };
  }

  // read NAL arrays

  int nArrays = range.read8();

  for (int i = 0; i < nArrays && !range.error(); i++) {
    byte = range.read8();

    NalArray array;

    array.m_array_completeness = (byte >> 7) & 1;
    array.m_NAL_unit_type = (byte & 0x3F);

    int nUnits = range.read16();
    for (int u = 0; u < nUnits && !range.error(); u++) {

      std::vector<uint8_t> nal_unit;
      int size = range.read16();
      if (!size) {
        // Ignore empty NAL units.
        continue;
      }

      if (range.prepare_read(size)) {
        nal_unit.resize(size);
        bool success = range.get_istream()->read((char*) nal_unit.data(), size);
        if (!success) {
          return Error{heif_error_Invalid_input, heif_suberror_End_of_data, "error while reading hvcC box"};
        }
      }

      array.m_nal_units.push_back(std::move(nal_unit));
    }

    m_nal_array.push_back(std::move(array));
  }

  return range.get_error();
}


bool Box_vvcC::get_headers(std::vector<uint8_t>* dest) const
{
  for (const auto& nal_array : m_nal_array) {
    for (const auto& nal : nal_array.m_nal_units) {
      assert(nal.size() <= 0xFFFF);
      auto size = static_cast<uint16_t>(nal.size());

      dest->push_back(0);
      dest->push_back(0);
      dest->push_back(static_cast<uint8_t>(size >> 8));
      dest->push_back(static_cast<uint8_t>(size & 0xFF));

      dest->insert(dest->end(), nal.begin(), nal.end());
    }
  }

  return true;
}


void Box_vvcC::append_nal_data(const std::vector<uint8_t>& nal)
{
  assert(nal.size()>=2);
  uint8_t nal_type = (nal[1] >> 3) & 0x1F;

  // insert into existing array if it exists

  for (auto& nalarray : m_nal_array) {
    if (nalarray.m_NAL_unit_type == nal_type) {
      nalarray.m_nal_units.push_back(nal);
      return;
    }
  }

  // generate new NAL array

  NalArray array;
  array.m_array_completeness = true;
  array.m_NAL_unit_type = uint8_t((nal[1] >> 3) & 0x1F);
  array.m_nal_units.push_back(nal);

  m_nal_array.push_back(array);
}


void Box_vvcC::append_nal_data(const uint8_t* data, size_t size)
{
  std::vector<uint8_t> nal;
  nal.resize(size);
  memcpy(nal.data(), data, size);

  append_nal_data(nal);
}


Error Box_vvcC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  const auto& c = m_configuration;

  uint8_t byte;

  byte = uint8_t(0xF8 | (c.LengthSizeMinusOne<<1) | (c.ptl_present_flag ? 1 : 0));
  writer.write8(byte);

  if (c.ptl_present_flag) {
    assert(c.ols_idx <= 0x1FF);
    assert(c.num_sublayers <= 7);
    assert(c.constant_frame_rate <= 3);
    assert(c.chroma_format_idc <= 3);
    assert(c.bit_depth_minus8 <= 7);

    auto word = uint16_t((c.ols_idx << 7) | (c.num_sublayers << 4) | (c.constant_frame_rate << 2) | (c.chroma_format_idc));
    writer.write16(word);

    writer.write8(uint8_t((c.bit_depth_minus8<<5) | 0x1F));

    const auto& ptl = c.native_ptl;

    assert(ptl.general_profile_idc <= 0x7F);

    writer.write8(ptl.num_bytes_constraint_info & 0x3f);
    writer.write8(static_cast<uint8_t>((ptl.general_profile_idc<<1) | ptl.general_tier_flag));
    writer.write8(ptl.general_level_idc);

    for (int i=0;i<ptl.num_bytes_constraint_info;i++) {
      if (i==0) {
        assert(ptl.ptl_frame_only_constraint_flag <= 1);
        assert(ptl.ptl_multi_layer_enabled_flag <= 1);
        assert(ptl.general_constraint_info[0] <= 0x3F);
        byte = static_cast<uint8_t>((ptl.ptl_frame_only_constraint_flag << 7) | (ptl.ptl_multi_layer_enabled_flag << 6) | ptl.general_constraint_info[0]);
      }
      else {
        byte = ptl.general_constraint_info[i];
      }

      writer.write8(byte);
    }

    byte = 0;
    if (c.num_sublayers > 1) {
      uint8_t mask = 0x80;

      for (int i = c.num_sublayers - 2; i >= 0; i--) {
        if (ptl.ptl_sublayer_level_present_flag[i]) {
          byte |= mask;
        }
        mask >>= 1;
      }

      writer.write8(byte);
    }

    for (int i=c.num_sublayers-2; i >= 0; i--) {
      if (ptl.ptl_sublayer_level_present_flag[i]) {
        writer.write8(ptl.sublayer_level_idc[i]);
      }
    }

    assert(ptl.general_sub_profile_idc.size() <= 0xFF);
    byte = static_cast<uint8_t>(ptl.general_sub_profile_idc.size());
    writer.write8(byte);

    for (int j=0; j < byte; j++) {
      writer.write32(ptl.general_sub_profile_idc[j]);
    }

    writer.write16(c.max_picture_width);
    writer.write16(c.max_picture_height);
    writer.write16(c.avg_frame_rate);
  }

  // --- write configuration NALs

  if (m_nal_array.size() > 255) {
    return {heif_error_Encoding_error, heif_suberror_Unspecified, "Too many VVC NAL arrays."};
  }

  writer.write8((uint8_t)m_nal_array.size());
  for (const NalArray& nal_array : m_nal_array) {
    uint8_t v2 = (nal_array.m_array_completeness ? 0x80 : 0);
    v2 |= nal_array.m_NAL_unit_type;
    writer.write8(v2);

    if (nal_array.m_nal_units.size() > 0xFFFF) {
      return {heif_error_Encoding_error, heif_suberror_Unspecified, "Too many VVC NAL units."};
    }

    writer.write16((uint16_t)nal_array.m_nal_units.size());
    for (const auto& nal : nal_array.m_nal_units) {

      if (nal.size() > 0xFFFF) {
        return {heif_error_Encoding_error, heif_suberror_Unspecified, "VVC NAL too large."};
      }

      writer.write16((uint16_t)nal.size());
      writer.write(nal);
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


static const char* vvc_chroma_names[4] = {"mono", "4:2:0", "4:2:2", "4:4:4"};

const char* NAL_name(uint8_t nal_type)
{
  switch (nal_type) {
    case 12: return "OPI";
    case 13: return "DCI";
    case 14: return "VPS";
    case 15: return "SPS";
    case 16: return "PPS";
    case 17: return "PREFIX_APS";
    case 18: return "SUFFIX_APS";
    case 19: return "PH";
    default: return "?";
  }
}


std::string Box_vvcC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);

  const auto& c = m_configuration; // abbreviation

  sstr << indent << "NAL length size: " << ((int) c.LengthSizeMinusOne + 1) << "\n";
  if (c.ptl_present_flag) {
    const auto& ptl = c.native_ptl;
    sstr << indent << "ols-index: " << c.ols_idx << "\n"
         << indent << "num sublayers: " << ((int) c.num_sublayers) << "\n"
         << indent << "constant frame rate: " << (c.constant_frame_rate == 1 ? "constant" : (c.constant_frame_rate == 2 ? "multi-layer" : "unknown")) << "\n"
         << indent << "chroma-format: " << vvc_chroma_names[c.chroma_format_idc] << "\n"
         << indent << "bit-depth: " << ((int) c.bit_depth_minus8 + 8) << "\n"
         << indent << "max picture width:  " << c.max_picture_width << "\n"
         << indent << "max picture height: " << c.max_picture_height << "\n";

    sstr << indent << "general profile: " << ((int)ptl.general_profile_idc) << "\n"
         << indent << "tier flag: " << ((int)ptl.general_tier_flag) << "\n"
         << indent << "general level:" << ((int)ptl.general_level_idc) << "\n"
         << indent << "ptl frame only constraint flag: " << ((int)ptl.ptl_frame_only_constraint_flag) << "\n"
         << indent << "ptl multi layer enabled flag: " << ((int)ptl.ptl_multi_layer_enabled_flag) << "\n";
  }


  sstr << indent << "num of arrays: " << m_nal_array.size() << "\n";

  sstr << indent << "config NALs:\n";
  for (const auto& nal_array : m_nal_array) {
    indent++;
    sstr << indent << "NAL type: " << ((int)nal_array.m_NAL_unit_type) << " (" << NAL_name(nal_array.m_NAL_unit_type) << ")\n";
    sstr << indent << "array completeness: " << ((int)nal_array.m_array_completeness) << "\n";

    for (const auto& nal : nal_array.m_nal_units) {
      indent++;
      std::string ind = indent.get_string();
      sstr << write_raw_data_as_hex(nal.data(), nal.size(), ind, ind);
      indent--;
    }
    indent--;
  }

  return sstr.str();
}


Error parse_sps_for_vvcC_configuration(const uint8_t* sps, size_t size,
                                       Box_vvcC::configuration* config,
                                       int* width, int* height)
{
  // remove start-code emulation bytes from SPS header stream

  std::vector<uint8_t> sps_no_emul = remove_start_code_emulation(sps, size);

  sps = sps_no_emul.data();
  size = sps_no_emul.size();

  BitReader reader(sps, (int) size);

  // skip NAL header
  reader.skip_bits(2 * 8);

  // skip SPS ID
  reader.skip_bits(4);

  // skip VPS ID
  reader.skip_bits(4);

  config->ols_idx = 0;
  config->num_sublayers = reader.get_bits8(3) + 1;
  config->chroma_format_idc = reader.get_bits8(2);
  reader.skip_bits(2);

  bool sps_ptl_dpb_hrd_params_present_flag = reader.get_bits(1);
  if (sps_ptl_dpb_hrd_params_present_flag) {
    // profile_tier_level( 1, sps_max_sublayers_minus1 )

    auto& ptl = config->native_ptl;

    if (true /*profileTierPresentFlag*/) {
      ptl.general_profile_idc = reader.get_bits8(7);
      ptl.general_tier_flag = reader.get_bits8(1);
    }
    ptl.general_level_idc = reader.get_bits8(8);
    ptl.ptl_frame_only_constraint_flag = reader.get_bits8(1);
    ptl.ptl_multi_layer_enabled_flag = reader.get_bits8(1);

    if (true /* profileTierPresentFlag*/ ) {
      // general_constraints_info()

      bool gci_present_flag = reader.get_bits(1);
      if (gci_present_flag) {
        assert(false);
      }
      else {
        ptl.num_bytes_constraint_info = 1;
        ptl.general_constraint_info.push_back(0);
      }

      reader.skip_to_byte_boundary();
    }

    ptl.ptl_sublayer_level_present_flag.resize(config->num_sublayers);
    for (int i = config->num_sublayers-2; i >= 0; i--) {
      ptl.ptl_sublayer_level_present_flag[i] = reader.get_bits(1);
    }

    reader.skip_to_byte_boundary();

    ptl.sublayer_level_idc.resize(config->num_sublayers);
    for (int i = config->num_sublayers-2; i >= 0; i--) {
      if (ptl.ptl_sublayer_level_present_flag[i]) {
        ptl.sublayer_level_idc[i] = reader.get_bits8(8);
      }
    }

    if (true /*profileTierPresentFlag*/) {
      int ptl_num_sub_profiles = reader.get_bits(8);
      ptl.general_sub_profile_idc.resize(ptl_num_sub_profiles);

      for (int i = 0; i < ptl_num_sub_profiles; i++) {
        ptl.general_sub_profile_idc[i] = reader.get_bits(32);
      }
    }
  }

  reader.skip_bits(1); // sps_gdr_enabled_flag
  bool sps_ref_pic_resampling_enabled_flag = reader.get_bits(1);
  if (sps_ref_pic_resampling_enabled_flag) {
    reader.skip_bits(1); // sps_res_change_in_clvs_allowed_flag
  }

  int sps_pic_width_max_in_luma_samples;
  int sps_pic_height_max_in_luma_samples;

  bool success;
  success = reader.get_uvlc(&sps_pic_width_max_in_luma_samples);
  (void)success;
  success = reader.get_uvlc(&sps_pic_height_max_in_luma_samples);
  (void)success;

  *width = sps_pic_width_max_in_luma_samples;
  *height = sps_pic_height_max_in_luma_samples;

  if (sps_pic_width_max_in_luma_samples > 0xFFFF ||
      sps_pic_height_max_in_luma_samples > 0xFFFF) {
    return {heif_error_Encoding_error,
            heif_suberror_Invalid_parameter_value,
            "SPS max picture width or height exceeds maximum (65535)"};
  }

  config->max_picture_width = static_cast<uint16_t>(sps_pic_width_max_in_luma_samples);
  config->max_picture_height = static_cast<uint16_t>(sps_pic_height_max_in_luma_samples);

  int sps_conformance_window_flag = reader.get_bits(1);
  if (sps_conformance_window_flag) {
    int left,right,top,bottom;
    reader.get_uvlc(&left);
    reader.get_uvlc(&right);
    reader.get_uvlc(&top);
    reader.get_uvlc(&bottom);
  }

  bool sps_subpic_info_present_flag = reader.get_bits(1);
  if (sps_subpic_info_present_flag) {
    assert(false); // TODO
  }

  int bitDepth_minus8;
  success = reader.get_uvlc(&bitDepth_minus8);
  (void)success;

  if (bitDepth_minus8 > 0xFF - 8) {
    return {heif_error_Encoding_error, heif_suberror_Unspecified, "VCC bit depth out of range."};
  }

  config->bit_depth_minus8 = static_cast<uint8_t>(bitDepth_minus8);

  config->constant_frame_rate = 1; // is constant (TODO: where do we get this from)

  return Error::Ok;
}

