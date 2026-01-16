/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#include "hevc_boxes.h"
#include "bitstream.h"
#include "error.h"
#include "file.h"
#include "hevc_dec.h"

#include <cassert>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <string>
#include <utility>
#include <algorithm>
#include "api_structs.h"


Error HEVCDecoderConfigurationRecord::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint8_t byte;

  configuration_version = range.read8();
  byte = range.read8();
  general_profile_space = (byte >> 6) & 3;
  general_tier_flag = (byte >> 5) & 1;
  general_profile_idc = (byte & 0x1F);

  general_profile_compatibility_flags = range.read32();

  for (int i = 0; i < 6; i++) {
    byte = range.read8();

    for (int b = 0; b < 8; b++) {
      general_constraint_indicator_flags[i * 8 + b] = (byte >> (7 - b)) & 1;
    }
  }

  general_level_idc = range.read8();
  min_spatial_segmentation_idc = range.read16() & 0x0FFF;
  parallelism_type = range.read8() & 0x03;
  chroma_format = range.read8() & 0x03;
  bit_depth_luma = static_cast<uint8_t>((range.read8() & 0x07) + 8);
  bit_depth_chroma = static_cast<uint8_t>((range.read8() & 0x07) + 8);
  avg_frame_rate = range.read16();

  byte = range.read8();
  constant_frame_rate = (byte >> 6) & 0x03;
  num_temporal_layers = (byte >> 3) & 0x07;
  temporal_id_nested = (byte >> 2) & 1;

  m_length_size = static_cast<uint8_t>((byte & 0x03) + 1);

  int nArrays = range.read8();

  for (int i = 0; i < nArrays && !range.error(); i++) {
    byte = range.read8();

    NalArray array;

    array.m_array_completeness = (byte >> 6) & 1;
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

  range.skip_to_end_of_box();

  return range.get_error();
}


Error HEVCDecoderConfigurationRecord::write(StreamWriter& writer) const
{
  writer.write8(configuration_version);

  writer.write8((uint8_t) (((general_profile_space & 3) << 6) |
                           ((general_tier_flag & 1) << 5) |
                           (general_profile_idc & 0x1F)));

  writer.write32(general_profile_compatibility_flags);

  for (int i = 0; i < 6; i++) {
    uint8_t byte = 0;

    for (int b = 0; b < 8; b++) {
      if (general_constraint_indicator_flags[i * 8 + b]) {
        byte |= 1;
      }

      byte = (uint8_t) (byte << 1);
    }

    writer.write8(byte);
  }

  writer.write8(general_level_idc);
  writer.write16((min_spatial_segmentation_idc & 0x0FFF) | 0xF000);
  writer.write8(parallelism_type | 0xFC);
  writer.write8(chroma_format | 0xFC);
  writer.write8((uint8_t) ((bit_depth_luma - 8) | 0xF8));
  writer.write8((uint8_t) ((bit_depth_chroma - 8) | 0xF8));
  writer.write16(avg_frame_rate);

  writer.write8((uint8_t) (((constant_frame_rate & 0x03) << 6) |
                           ((num_temporal_layers & 0x07) << 3) |
                           ((temporal_id_nested & 1) << 2) |
                           ((m_length_size - 1) & 0x03)));

  size_t nArrays = m_nal_array.size();
  // There cannot be an overflow because the nal-type is less than 8-bit.
  assert(nArrays <= 0xFF);

  writer.write8(static_cast<uint8_t>(nArrays));

  for (const HEVCDecoderConfigurationRecord::NalArray& array : m_nal_array) {

    writer.write8((uint8_t) (((array.m_array_completeness & 1) << 6) |
                             (array.m_NAL_unit_type & 0x3F)));

    size_t nUnits = array.m_nal_units.size();
    if (nUnits > 0xFFFF) {
      return Error{heif_error_Invalid_input, heif_suberror_Unspecified, "Too many NAL units in hvcC"};
    }

    writer.write16(static_cast<uint16_t>(nUnits));

    for (const std::vector<uint8_t>& nal_unit : array.m_nal_units) {
      if (nal_unit.size() > 0xFFFF) {
        return Error{heif_error_Invalid_input, heif_suberror_Unspecified, "hvcC NAL unit exceeds maximum size (64kB)"};
      }

      writer.write16(static_cast<uint16_t>(nal_unit.size()));
      writer.write(nal_unit);
    }
  }

  return Error::Ok;
}


bool HEVCDecoderConfigurationRecord::get_general_profile_compatibility_flag(int idx) const
{
  return general_profile_compatibility_flags & (UINT32_C(0x80000000) >> idx);
}


bool HEVCDecoderConfigurationRecord::is_profile_compatibile(Profile profile) const
{
  return (general_profile_idc == profile ||
          get_general_profile_compatibility_flag(profile));
}


Error Box_hvcC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  return m_configuration.parse(range, limits);
}


std::string Box_hvcC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  const auto& c = m_configuration; // abbreviation

  sstr << indent << "configuration_version: " << ((int) c.configuration_version) << "\n"
       << indent << "general_profile_space: " << ((int) c.general_profile_space) << "\n"
       << indent << "general_tier_flag: " << c.general_tier_flag << "\n"
       << indent << "general_profile_idc: " << ((int) c.general_profile_idc) << "\n";

  sstr << indent << "general_profile_compatibility_flags: ";
  for (int i = 0; i < 32; i++) {
    sstr << ((c.general_profile_compatibility_flags >> (31 - i)) & 1);
    if ((i % 8) == 7) sstr << ' ';
    else if ((i % 4) == 3) sstr << '.';
  }
  sstr << "\n";

  sstr << indent << "general_constraint_indicator_flags: ";
  int cnt = 0;
  for (int i = 0; i < HEVCDecoderConfigurationRecord::NUM_CONSTRAINT_INDICATOR_FLAGS; i++) {
    bool b = c.general_constraint_indicator_flags[i];

    sstr << (b ? 1 : 0);
    cnt++;
    if ((cnt % 8) == 0)
      sstr << ' ';
  }
  sstr << "\n";

  sstr << indent << "general_level_idc: " << ((int) c.general_level_idc) << "\n"
       << indent << "min_spatial_segmentation_idc: " << c.min_spatial_segmentation_idc << "\n"
       << indent << "parallelism_type: " << ((int) c.parallelism_type) << "\n"
       << indent << "chroma_format: ";

  switch (c.chroma_format) {
    case 0:
      sstr << "monochrome";
      break;
    case 1:
      sstr << "4:2:0";
      break;
    case 2:
      sstr << "4:2:2";
      break;
    case 3:
      sstr << "4:4:4";
      break;
    default:
      sstr << ((int) c.chroma_format);
      break;
  }

  sstr << "\n"
       << indent << "bit_depth_luma: " << ((int) c.bit_depth_luma) << "\n"
       << indent << "bit_depth_chroma: " << ((int) c.bit_depth_chroma) << "\n"
       << indent << "avg_frame_rate: " << c.avg_frame_rate << "\n"
       << indent << "constant_frame_rate: " << ((int) c.constant_frame_rate) << "\n"
       << indent << "num_temporal_layers: " << ((int) c.num_temporal_layers) << "\n"
       << indent << "temporal_id_nested: " << ((int) c.temporal_id_nested) << "\n"
       << indent << "length_size: " << ((int) c.m_length_size) << "\n";

  for (const auto& array : c.m_nal_array) {
    sstr << indent << "<array>\n";

    indent++;
    sstr << indent << "array_completeness: " << ((int) array.m_array_completeness) << "\n"
         << indent << "NAL_unit_type: " << ((int) array.m_NAL_unit_type) << "\n";

    for (const auto& unit : array.m_nal_units) {
      //sstr << "  unit with " << unit.size() << " bytes of data\n";
      sstr << indent;
      for (uint8_t b : unit) {
        sstr << std::setfill('0') << std::setw(2) << std::hex << ((int) b) << " ";
      }
      sstr << "\n";
      sstr << std::dec;
    }

    indent--;
  }

  return sstr.str();
}


bool Box_hvcC::get_header_nals(std::vector<uint8_t>* dest) const
{
  // Concatenate all header NALs, each prefixed by a 4-byte size.

  for (const auto& array : m_configuration.m_nal_array) {
    for (const auto& unit : array.m_nal_units) {

      // Write 4-byte NALs size

      dest->push_back((unit.size() >> 24) & 0xFF);
      dest->push_back((unit.size() >> 16) & 0xFF);
      dest->push_back((unit.size() >> 8) & 0xFF);
      dest->push_back((unit.size() >> 0) & 0xFF);

      // Copy NAL data

      dest->insert(dest->end(), unit.begin(), unit.end());
    }
  }

  return true;
}

void Box_hvcC::append_nal_data(const uint8_t* data, size_t size)
{
  std::vector<uint8_t> nal;
  nal.resize(size);
  memcpy(nal.data(), data, size);

  append_nal_data(nal);
}

void Box_hvcC::append_nal_data(const std::vector<uint8_t>& nal)
{
  for (auto& nal_array : m_configuration.m_nal_array) {
    if (nal_array.m_NAL_unit_type == uint8_t(nal[0] >> 1)) {

      // kvazaar may send the same headers multiple times. Filter out the identical copies.

      for (auto& nal_unit : nal_array.m_nal_units) {

        // Note: sometimes kvazaar even sends the same packet twice, but with an extra zero byte.
        //       We detect this by comparing only the common length. This is correct since each NAL
        //       packet must be complete and thus, if a packet is longer than another complete packet,
        //       its extra data must be superfluous.
        //
        // Example:
        //| | | <array>
        //| | | | array_completeness: 1
        //| | | | NAL_unit_type: 34
        //| | | | 44 01 c1 71 82 99 20 00
        //| | | | 44 01 c1 71 82 99 20

        // Check whether packets have similar content.

        const size_t common_length = std::min(nal_unit.size(), nal.size());
        bool similar = true;
        for (size_t i = 0; i < common_length; i++) {
          if (nal_unit[i] != nal[i]) {
            similar = false;
            break;
          }
        }

        if (similar) {
          // If they are similar, keep the smaller one.

          if (nal_unit.size() > nal.size()) {
            nal_unit = std::move(nal);
          }

          // Exit. Do not add a copy of the packet.

          return;
        }
      }

      nal_array.m_nal_units.push_back(std::move(nal));

      return;
    }
  }

  // This is a new NAL type. Add a new NAL array.

  HEVCDecoderConfigurationRecord::NalArray array;
  array.m_array_completeness = 1;
  array.m_NAL_unit_type = uint8_t(nal[0] >> 1);
  array.m_nal_units.push_back(std::move(nal));

  m_configuration.m_nal_array.push_back(array);
}


Error Box_hvcC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  const auto& c = m_configuration; // abbreviation

  Error err = c.write(writer);
  if (err) {
    return err;
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


static double read_depth_rep_info_element(BitReader& reader)
{
  uint8_t sign_flag = reader.get_bits8(1);
  int exponent = reader.get_bits(7);
  auto mantissa_len = static_cast<uint8_t>(reader.get_bits8(5) + 1);
  if (mantissa_len < 1 || mantissa_len > 32) {
    // TODO err
  }

  if (exponent == 127) {
    // TODO value unspecified
  }

  uint32_t mantissa = reader.get_bits32(mantissa_len);
  double value;

  //printf("sign:%d exponent:%d mantissa_len:%d mantissa:%d\n",sign_flag,exponent,mantissa_len,mantissa);

  // TODO: this seems to be wrong. 'exponent' is never negative. How to read it correctly?
  if (exponent > 0) {
    value = pow(2.0, exponent - 31) * (1.0 + mantissa / pow(2.0, mantissa_len));
  }
  else {
    value = pow(2.0, -(30 + mantissa_len)) * mantissa;
  }

  if (sign_flag) {
    value = -value;
  }

  return value;
}


static Result<std::shared_ptr<SEIMessage>> read_depth_representation_info(BitReader& reader)
{
  auto msg = std::make_shared<SEIMessage_depth_representation_info>();


  // default values

  msg->version = 1;

  msg->disparity_reference_view = 0;
  msg->depth_nonlinear_representation_model_size = 0;
  msg->depth_nonlinear_representation_model = nullptr;


  // read header

  msg->has_z_near = (uint8_t) reader.get_bits(1);
  msg->has_z_far = (uint8_t) reader.get_bits(1);
  msg->has_d_min = (uint8_t) reader.get_bits(1);
  msg->has_d_max = (uint8_t) reader.get_bits(1);

  int rep_type;
  if (!reader.get_uvlc(&rep_type)) {
    return Error{heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "invalid depth representation type in input"};
  }

  if (rep_type < 0 || rep_type > 3) {
    return Error{heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "input depth representation type out of range"};
  }

  msg->depth_representation_type = (enum heif_depth_representation_type) rep_type;

  //printf("flags: %d %d %d %d\n",msg->has_z_near,msg->has_z_far,msg->has_d_min,msg->has_d_max);
  //printf("type: %d\n",rep_type);

  if (msg->has_d_min || msg->has_d_max) {
    int ref_view;
    if (!reader.get_uvlc(&ref_view)) {
      return Error{heif_error_Invalid_input, heif_suberror_Invalid_parameter_value, "invalid disparity_reference_view in input"};
    }
    msg->disparity_reference_view = ref_view;

    //printf("ref_view: %d\n",msg->disparity_reference_view);
  }

  if (msg->has_z_near) msg->z_near = read_depth_rep_info_element(reader);
  if (msg->has_z_far) msg->z_far = read_depth_rep_info_element(reader);
  if (msg->has_d_min) msg->d_min = read_depth_rep_info_element(reader);
  if (msg->has_d_max) msg->d_max = read_depth_rep_info_element(reader);

  /*
  printf("z_near: %f\n",msg->z_near);
  printf("z_far: %f\n",msg->z_far);
  printf("dmin: %f\n",msg->d_min);
  printf("dmax: %f\n",msg->d_max);
  */

  if (msg->depth_representation_type == heif_depth_representation_type_nonuniform_disparity) {
    // TODO: load non-uniform response curve
  }

  return {msg};
}


// aux subtypes: 00 00 00 11 / 00 00 00 0d / 4e 01 / b1 09 / 35 1e 78 c8 01 03 c5 d0 20

Error decode_hevc_aux_sei_messages(const std::vector<uint8_t>& data,
                                   std::vector<std::shared_ptr<SEIMessage>>& msgs)
{
  // TODO: we probably do not need a full BitReader just for the array size.
  // Read this and the NAL size directly on the array data.

  BitReader reader(data.data(), (int) data.size());
  if (reader.get_bits_remaining() < 32) {
    return {heif_error_Invalid_input,
            heif_suberror_End_of_data,
            "HEVC SEI NAL too short"};
  }

  uint32_t len = reader.get_bits32(32);

  if (len > data.size() - 4) {
    // ERROR: read past end of data
  }

  while (reader.get_current_byte_index() < (int) len) {
    int currPos = reader.get_current_byte_index();

    BitReader sei_reader(data.data() + currPos, (int) data.size() - currPos);

    if (sei_reader.get_bits_remaining() < 32+8) {
      return {heif_error_Invalid_input,
              heif_suberror_End_of_data,
              "HEVC SEI NAL too short"};
    }

    uint32_t nal_size = sei_reader.get_bits32(32);
    (void) nal_size;

    auto nal_type = static_cast<uint8_t>(sei_reader.get_bits8(8) >> 1);
    sei_reader.skip_bits(8);

    // SEI

    if (nal_type == 39 ||
        nal_type == 40) {

      if (sei_reader.get_bits_remaining() < 16) {
        return {heif_error_Invalid_input,
                heif_suberror_End_of_data,
                "HEVC SEI NAL too short"};
      }

      // TODO: loading of multi-byte sei headers
      uint8_t payload_id = sei_reader.get_bits8(8);
      uint8_t payload_size = sei_reader.get_bits8(8);
      (void) payload_size;

      if (payload_id == 177) {
        // depth_representation_info
        Result<std::shared_ptr<SEIMessage>> seiResult = read_depth_representation_info(sei_reader);
        if (!seiResult) {
          return seiResult.error();
        }

        msgs.push_back(*seiResult);
      }
    }

    break; // TODO: read next SEI
  }


  return Error::Ok;
}


// Used for AVC, HEVC, and VVC.
std::vector<uint8_t> remove_start_code_emulation(const uint8_t* sps, size_t size)
{
  std::vector<uint8_t> out_data;

  for (size_t i = 0; i < size; i++) {
    if (i + 2 < size &&
        sps[i] == 0 &&
        sps[i + 1] == 0 &&
        sps[i + 2] == 3) {
      out_data.push_back(0);
      out_data.push_back(0);
      i += 2;
    }
    else {
      out_data.push_back(sps[i]);
    }
  }

  return out_data;
}


Error parse_sps_for_hvcC_configuration(const uint8_t* sps, size_t size,
                                       HEVCDecoderConfigurationRecord* config,
                                       int* width, int* height)
{
  // remove start-code emulation bytes from SPS header stream

  std::vector<uint8_t> sps_no_emul = remove_start_code_emulation(sps, size);

  sps = sps_no_emul.data();
  size = sps_no_emul.size();


  BitReader reader(sps, (int) size);

  // skip NAL header
  reader.skip_bits(2 * 8);

  // skip VPS ID
  reader.skip_bits(4);

  uint8_t nMaxSubLayersMinus1 = reader.get_bits8(3);

  config->temporal_id_nested = reader.get_bits8(1);

  // --- profile_tier_level ---

  config->general_profile_space = reader.get_bits8(2);
  config->general_tier_flag = reader.get_bits8(1);
  config->general_profile_idc = reader.get_bits8(5);
  config->general_profile_compatibility_flags = reader.get_bits32(32);

  reader.skip_bits(16); // skip reserved bits
  reader.skip_bits(16); // skip reserved bits
  reader.skip_bits(16); // skip reserved bits

  config->general_level_idc = reader.get_bits8(8);

  std::vector<bool> layer_profile_present(nMaxSubLayersMinus1);
  std::vector<bool> layer_level_present(nMaxSubLayersMinus1);

  for (int i = 0; i < nMaxSubLayersMinus1; i++) {
    layer_profile_present[i] = reader.get_bits(1);
    layer_level_present[i] = reader.get_bits(1);
  }

  if (nMaxSubLayersMinus1 > 0) {
    for (int i = nMaxSubLayersMinus1; i < 8; i++) {
      reader.skip_bits(2);
    }
  }

  for (int i = 0; i < nMaxSubLayersMinus1; i++) {
    if (layer_profile_present[i]) {
      reader.skip_bits(2 + 1 + 5);
      reader.skip_bits(32);
      reader.skip_bits(16);
    }

    if (layer_level_present[i]) {
      reader.skip_bits(8);
    }
  }


  // --- SPS continued ---

  int dummy, value;
  reader.get_uvlc(&dummy); // skip seq_parameter_seq_id

  reader.get_uvlc(&value);
  config->chroma_format = (uint8_t) value;

  if (config->chroma_format == 3) {
    reader.skip_bits(1);
  }

  reader.get_uvlc(width);
  reader.get_uvlc(height);

  bool conformance_window = reader.get_bits(1);
  if (conformance_window) {
    int left, right, top, bottom;
    reader.get_uvlc(&left);
    reader.get_uvlc(&right);
    reader.get_uvlc(&top);
    reader.get_uvlc(&bottom);

    //printf("conformance borders: %d %d %d %d\n",left,right,top,bottom);

    int subH = 1, subV = 1;
    if (config->chroma_format == 1) {
      subV = 2;
      subH = 2;
    }
    if (config->chroma_format == 2) { subH = 2; }

    *width -= subH * (left + right);
    *height -= subV * (top + bottom);
  }

  reader.get_uvlc(&value);
  config->bit_depth_luma = (uint8_t) (value + 8);

  reader.get_uvlc(&value);
  config->bit_depth_chroma = (uint8_t) (value + 8);



  // --- init static configuration fields ---

  config->configuration_version = 1;
  config->min_spatial_segmentation_idc = 0; // TODO: get this value from the VUI, 0 should be safe
  config->parallelism_type = 0; // TODO, 0 should be safe
  config->avg_frame_rate = 0; // makes no sense for HEIF (TODO)
  config->constant_frame_rate = 0; // makes no sense for HEIF (TODO)
  config->num_temporal_layers = 1; // makes no sense for HEIF

  return Error::Ok;
}
