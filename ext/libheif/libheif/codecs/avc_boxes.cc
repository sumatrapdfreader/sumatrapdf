/*
 * HEIF AVC codec.
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

#include "avc_boxes.h"
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>
#include "file.h"
#include "context.h"
#include "avc_dec.h"
#include "hevc_boxes.h"
#include <utility>
#include <set>


Error Box_avcC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  m_configuration.configuration_version = range.read8();
  m_configuration.AVCProfileIndication = range.read8();
  m_configuration.profile_compatibility = range.read8();
  m_configuration.AVCLevelIndication = range.read8();
  uint8_t lengthSizeMinusOneWithReserved = range.read8();
  m_configuration.lengthSize =
      (lengthSizeMinusOneWithReserved & 0b00000011) + 1;

  uint8_t numOfSequenceParameterSets = (range.read8() & 0b00011111);
  for (int i = 0; i < numOfSequenceParameterSets; i++) {
    uint16_t sequenceParameterSetLength = range.read16();
    std::vector<uint8_t> sps(sequenceParameterSetLength);
    range.read(sps.data(), sps.size());
    m_sps.push_back(sps);
  }

  uint8_t numOfPictureParameterSets = range.read8();
  for (int i = 0; i < numOfPictureParameterSets; i++) {
    uint16_t pictureParameterSetLength = range.read16();
    std::vector<uint8_t> pps(pictureParameterSetLength);
    range.read(pps.data(), pps.size());
    m_pps.push_back(pps);
  }

  // See ISO/IEC 14496-15 2017 Section 5.3.3.1.2
  if (range.get_remaining_bytes() > 0) {
    if ((m_configuration.AVCProfileIndication != 66) &&
        (m_configuration.AVCProfileIndication != 77) &&
        (m_configuration.AVCProfileIndication != 88)) {
      m_configuration.chroma_format = (heif_chroma) (range.read8() & 0b00000011);
      m_configuration.bit_depth_luma = 8 + (range.read8() & 0b00000111);
      m_configuration.bit_depth_chroma = 8 + (range.read8() & 0b00000111);
      uint8_t numOfSequenceParameterSetExt = range.read8();
      for (int i = 0; i < numOfSequenceParameterSetExt; i++) {
        uint16_t sequenceParameterSetExtLength = range.read16();
        std::vector<uint8_t> sps_ext(sequenceParameterSetExtLength);
        range.read(sps_ext.data(), sps_ext.size());
        m_sps_ext.push_back(sps_ext);
      }
    }
  }

  return range.get_error();
}

Error Box_avcC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write8(m_configuration.configuration_version);
  writer.write8(m_configuration.AVCProfileIndication);
  writer.write8(m_configuration.profile_compatibility);
  writer.write8(m_configuration.AVCLevelIndication);
  uint8_t lengthSizeMinusOneWithReserved = 0b11111100 | ((m_configuration.lengthSize - 1) & 0b11);
  writer.write8(lengthSizeMinusOneWithReserved);

  if (m_sps.size() > 0b00011111) {
    return {
      heif_error_Encoding_error,
      heif_suberror_Unspecified,
      "Cannot write more than 31 PPS into avcC box."
    };
  }

  uint8_t numSpsWithReserved = 0b11100000 | (m_sps.size() & 0b00011111);
  writer.write8(numSpsWithReserved);
  for (const auto& sps : m_sps) {
    if (sps.size() > 0xFFFF) {
      return {
        heif_error_Encoding_error,
        heif_suberror_Unspecified,
        "Cannot write SPS larger than 65535 bytes into avcC box."
      };
    }
    writer.write16((uint16_t) sps.size());
    writer.write(sps);
  }

  if (m_pps.size() > 0xFF) {
    return {
      heif_error_Encoding_error,
      heif_suberror_Unspecified,
      "Cannot write more than 255 PPS into avcC box."
    };
  }

  writer.write8(m_pps.size() & 0xFF);
  for (const auto& pps : m_pps) {
    if (pps.size() > 0xFFFF) {
      return {
        heif_error_Encoding_error,
        heif_suberror_Unspecified,
        "Cannot write PPS larger than 65535 bytes into avcC box."
      };
    }
    writer.write16((uint16_t) pps.size());
    writer.write(pps);
  }

  if ((m_configuration.AVCProfileIndication != 66) &&
      (m_configuration.AVCProfileIndication != 77) &&
      (m_configuration.AVCProfileIndication != 88)) {
    writer.write8(m_configuration.chroma_format);
    writer.write8(m_configuration.bit_depth_luma - 8);
    writer.write8(m_configuration.bit_depth_chroma - 8);

    if (m_sps_ext.size() > 0xFF) {
      return {
        heif_error_Encoding_error,
        heif_suberror_Unspecified,
        "Cannot write more than 255 SPS-Ext into avcC box."
      };
    }

    writer.write8(m_sps_ext.size() & 0xFF);
    for (const auto& spsext : m_sps_ext) {
      if (spsext.size() > 0xFFFF) {
        return {
          heif_error_Encoding_error,
          heif_suberror_Unspecified,
          "Cannot write SPS-Ext larger than 65535 bytes into avcC box."
        };
      }
      writer.write16((uint16_t) spsext.size());
      writer.write(spsext);
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}

std::string Box_avcC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);
  sstr << indent << "configuration_version: " << ((int) m_configuration.configuration_version) << "\n"
      << indent << "AVCProfileIndication: " << ((int) m_configuration.AVCProfileIndication) << " (" << profileIndicationAsText() << ")\n"
      << indent << "profile_compatibility: " << ((int) m_configuration.profile_compatibility) << "\n"
      << indent << "AVCLevelIndication: " << ((int) m_configuration.AVCLevelIndication) << "\n"
      << indent << "Chroma format: ";

  switch (m_configuration.chroma_format) {
    case heif_chroma_monochrome:
      sstr << "4:0:0\n";
      break;
    case heif_chroma_420:
      sstr << "4:2:0\n";
      break;
    case heif_chroma_422:
      sstr << "4:2:2\n";
      break;
    case heif_chroma_444:
      sstr << "4:4:4\n";
      break;
    default:
      sstr << "unsupported\n";
      break;
  }

  sstr << indent << "Bit depth luma: " << ((int) m_configuration.bit_depth_luma) << "\n"
      << indent << "Bit depth chroma: " << ((int) m_configuration.bit_depth_chroma) << "\n";

  for (const auto& sps : m_sps) {
    sstr << indent << "SPS: ";
    for (uint8_t b : sps) {
      sstr << std::setfill('0') << std::setw(2) << std::hex << ((int) b) << " ";
    }
    sstr << "\n";
    sstr << std::dec;
  }

  for (const auto& spsext : m_sps_ext) {
    sstr << indent << "SPS-EXT: ";
    for (uint8_t b : spsext) {
      sstr << std::setfill('0') << std::setw(2) << std::hex << ((int) b) << " ";
    }
    sstr << "\n";
    sstr << std::dec;
  }

  for (const auto& pps : m_pps) {
    sstr << indent << "PPS: ";
    for (uint8_t b : pps) {
      sstr << std::setfill('0') << std::setw(2) << std::hex << ((int) b) << " ";
    }
    sstr << "\n";
    sstr << std::dec;
  }

  return sstr.str();
}

std::string Box_avcC::profileIndicationAsText() const
{
  // See ISO/IEC 14496-10:2022 Annex A
  switch (m_configuration.AVCProfileIndication) {
    case 44:
      return "CALVC 4:4:4";
    case 66:
      return "Constrained Baseline";
    case 77:
      return "Main";
    case 88:
      return "Extended";
    case 100:
      return "High variant";
    case 110:
      return "High 10";
    case 122:
      return "High 4:2:2";
    case 244:
      return "High 4:4:4";
    default:
      return "Unknown";
  }
}


void Box_avcC::get_header_nals(std::vector<uint8_t>& data) const
{
  for (const auto& sps : m_sps) {
    data.push_back((sps.size() >> 24) & 0xFF);
    data.push_back((sps.size() >> 16) & 0xFF);
    data.push_back((sps.size() >> 8) & 0xFF);
    data.push_back((sps.size() >> 0) & 0xFF);

    data.insert(data.end(), sps.begin(), sps.end());
  }

  for (const auto& spsext : m_sps_ext) {
    data.push_back((spsext.size() >> 24) & 0xFF);
    data.push_back((spsext.size() >> 16) & 0xFF);
    data.push_back((spsext.size() >> 8) & 0xFF);
    data.push_back((spsext.size() >> 0) & 0xFF);

    data.insert(data.end(), spsext.begin(), spsext.end());
  }

  for (const auto& pps : m_pps) {
    data.push_back((pps.size() >> 24) & 0xFF);
    data.push_back((pps.size() >> 16) & 0xFF);
    data.push_back((pps.size() >> 8) & 0xFF);
    data.push_back((pps.size() >> 0) & 0xFF);

    data.insert(data.end(), pps.begin(), pps.end());
  }
}


void Box_avcC::append_sps_nal(const uint8_t* data, size_t size)
{
  std::vector<uint8_t> vec(data, data + size);
  m_sps.emplace_back(std::move(vec));
}

void Box_avcC::append_sps_ext_nal(const uint8_t* data, size_t size)
{
  std::vector<uint8_t> vec(data, data + size);
  m_sps_ext.emplace_back(std::move(vec));
}

void Box_avcC::append_pps_nal(const uint8_t* data, size_t size)
{
  std::vector<uint8_t> vec(data, data + size);
  m_pps.emplace_back(std::move(vec));
}

void skip_scaling_list(BitReader& reader, int sizeOfScalingList)
{
  int lastScale = 8;
  int nextScale = 8;

  // TODO: it seems that this can be simplified by exiting the loop
  //       as soon as nextScale==0.

#if 0
  // original version
  for (int j = 0; j < sizeOfScalingList; j++) {
    if (nextScale != 0) {
      int delta_scale;
      reader.get_svlc(&delta_scale);
      nextScale = (lastScale + delta_scale + 256) % 256;
    }

    lastScale = (nextScale == 0) ? lastScale : nextScale;
  }
#else
  // fast version
  for (int j = 0; j < sizeOfScalingList; j++) {
    int delta_scale;
    reader.get_svlc(&delta_scale);
    nextScale = (lastScale + delta_scale + 256) % 256;

    if (nextScale == 0) {
      break;
    }

    lastScale = nextScale;
  }
#endif
}


Error parse_sps_for_avcC_configuration(const uint8_t* sps, size_t size,
                                       Box_avcC::configuration* config,
                                       int* width, int* height)
{
  // remove start-code emulation bytes from SPS header stream

  std::vector<uint8_t> sps_no_emul = remove_start_code_emulation(sps, size);

  sps = sps_no_emul.data();
  size = sps_no_emul.size();


  BitReader reader(sps, (int) size);

  // skip NAL header
  reader.skip_bits(8);

  config->configuration_version = 1;
  config->AVCProfileIndication = reader.get_bits8(8);
  config->profile_compatibility = reader.get_bits8(8);
  config->AVCLevelIndication = reader.get_bits8(8);
  config->lengthSize = 4;

  int value;
  reader.get_uvlc(&value); // SPS ID

  Error invalidUVLC{
    heif_error_Invalid_input,
    heif_suberror_Unspecified,
    "Invalid variable length code in AVC SPS header"
  };

  if (std::set<int>{100, 110, 122, 244, 44, 83, 86}.contains(config->AVCProfileIndication)) {
    if (!reader.get_uvlc(&value)) {
      return invalidUVLC;
    }

    config->chroma_format = (heif_chroma) value;
    if (config->chroma_format == heif_chroma_444) {
      reader.skip_bits(1);
    }

    if (!reader.get_uvlc(&value)) {
      return invalidUVLC;
    }
    config->bit_depth_luma = static_cast<uint8_t>(8 + value);

    if (!reader.get_uvlc(&value)) {
      return invalidUVLC;
    }
    config->bit_depth_chroma = static_cast<uint8_t>(8 + value);

    reader.skip_bits(1);
    int seq_scaling_matrix_present_flag = reader.get_bits(1);
    if (seq_scaling_matrix_present_flag) {
      for (int i = 0; i < ((config->chroma_format != heif_chroma_444) ? 8 : 12); i++) {
        int scaling_list_present_flag = reader.get_bits(1);
        if (scaling_list_present_flag) {
          if (i < 6) {
            skip_scaling_list(reader, 16);
          }
          else {
            skip_scaling_list(reader, 64);
          }
        }
      }
    }
  }
  else {
    config->chroma_format = heif_chroma_420;
    config->bit_depth_luma = 8;
    config->bit_depth_chroma = 8;
  }

  reader.get_uvlc(&value); // log2_max_frame_num_minus4
  int pic_order_cnt_type;
  reader.get_uvlc(&pic_order_cnt_type);
  if (pic_order_cnt_type == 0) {
    reader.get_uvlc(&value);
  }
  else if (pic_order_cnt_type == 1) {
    reader.get_bits(1);
    reader.get_svlc(&value);
    reader.get_svlc(&value);
    int num_ref_franes_in_pic_order_cnt_cycle;
    reader.get_uvlc(&num_ref_franes_in_pic_order_cnt_cycle);
    for (int i = 0; i < num_ref_franes_in_pic_order_cnt_cycle; i++) {
      reader.get_uvlc(&value);
    }
  }

  reader.get_uvlc(&value); // num_ref_frames
  reader.skip_bits(1);

  int pic_width_in_mbs_minus1;
  int pic_height_in_mbs_minus1;
  reader.get_uvlc(&pic_width_in_mbs_minus1);
  reader.get_uvlc(&pic_height_in_mbs_minus1);

  *width = (pic_width_in_mbs_minus1 + 1) * 16;
  *height = (pic_height_in_mbs_minus1 + 1) * 16;

  uint32_t frame_mbs_only_flag = reader.get_bits(1);
  if (!frame_mbs_only_flag) {
    reader.skip_bits(1);
  }
  reader.skip_bits(1);
  uint32_t frame_cropping_flag = reader.get_bits(1);
  if (frame_cropping_flag) {
    int left, right, top, bottom;
    reader.get_uvlc(&left);
    reader.get_uvlc(&right);
    reader.get_uvlc(&top);
    reader.get_uvlc(&bottom);

    *width -= left + right;
    *height -= top + bottom;
  }

  return {};
}
