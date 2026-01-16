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

#include "pixelimage.h"
#include "avif_boxes.h"
#include "avif_dec.h"
#include "bitstream.h"
#include "common_utils.h"
#include "api_structs.h"
#include "file.h"
#include <iomanip>
#include <limits>
#include <string>
#include <cstring>

// https://aomediacodec.github.io/av1-spec/av1-spec.pdf


Error Box_av1C::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  //parse_full_box_header(range);

  if (!has_fixed_box_size()) {
    // Note: in theory, it is allowed to have an av1C box with unspecified size (until the end of the file),
    // but that would be very uncommon and give us problems in the calculation of `configOBUs_bytes` below.
    // It's better to error on this case than to open a DoS vulnerability.
    return Error{heif_error_Invalid_input, heif_suberror_Unspecified, "av1C with unspecified box size"};
  }

  uint8_t byte;

  auto& c = m_configuration; // abbreviation

  byte = range.read8();
  if ((byte & 0x80) == 0) {
    // error: marker bit not set
  }

  c.version = byte & 0x7F;

  byte = range.read8();
  c.seq_profile = (byte >> 5) & 0x7;
  c.seq_level_idx_0 = byte & 0x1f;

  byte = range.read8();
  c.seq_tier_0 = (byte >> 7) & 1;
  c.high_bitdepth = (byte >> 6) & 1;
  c.twelve_bit = (byte >> 5) & 1;
  c.monochrome = (byte >> 4) & 1;
  c.chroma_subsampling_x = (byte >> 3) & 1;
  c.chroma_subsampling_y = (byte >> 2) & 1;
  c.chroma_sample_position = byte & 3;

  byte = range.read8();
  c.initial_presentation_delay_present = (byte >> 4) & 1;
  if (c.initial_presentation_delay_present) {
    c.initial_presentation_delay_minus_one = byte & 0x0F;
  }

  const size_t configOBUs_bytes = range.get_remaining_bytes();
  m_config_OBUs.resize(configOBUs_bytes);

  if (!range.read(m_config_OBUs.data(), configOBUs_bytes)) {
    // error
  }

  return range.get_error();
}


Error Box_av1C::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  const auto& c = m_configuration; // abbreviation

  writer.write8(c.version | 0x80);

  writer.write8((uint8_t) (((c.seq_profile & 0x7) << 5) |
                           (c.seq_level_idx_0 & 0x1f)));

  writer.write8((uint8_t) ((c.seq_tier_0 ? 0x80 : 0) |
                           (c.high_bitdepth ? 0x40 : 0) |
                           (c.twelve_bit ? 0x20 : 0) |
                           (c.monochrome ? 0x10 : 0) |
                           (c.chroma_subsampling_x ? 0x08 : 0) |
                           (c.chroma_subsampling_y ? 0x04 : 0) |
                           (c.chroma_sample_position & 0x03)));

  writer.write8(0); // TODO initial_presentation_delay

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_av1C::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  const auto& c = m_configuration; // abbreviation

  sstr << indent << "version: " << ((int) c.version) << "\n"
       << indent << "seq_profile: " << ((int) c.seq_profile) << "\n"
       << indent << "seq_level_idx_0: " << ((int) c.seq_level_idx_0) << "\n"
       << indent << "high_bitdepth: " << ((int) c.high_bitdepth) << "\n"
       << indent << "twelve_bit: " << ((int) c.twelve_bit) << "\n"
       << indent << "monochrome: " << ((int) c.monochrome) << "\n"
       << indent << "chroma_subsampling_x: " << ((int) c.chroma_subsampling_x) << "\n"
       << indent << "chroma_subsampling_y: " << ((int) c.chroma_subsampling_y) << "\n"
       << indent << "chroma_sample_position: " << ((int) c.chroma_sample_position) << "\n"
       << indent << "initial_presentation_delay: ";

  if (c.initial_presentation_delay_present) {
    sstr << c.initial_presentation_delay_minus_one + 1 << "\n";
  }
  else {
    sstr << "not present\n";
  }

  sstr << indent << "config OBUs:";
  for (size_t i = 0; i < m_config_OBUs.size(); i++) {
    sstr << " " << std::hex << std::setfill('0') << std::setw(2)
         << ((int) m_config_OBUs[i]);
  }
  sstr << std::dec << "\n";

  return sstr.str();
}


Error fill_av1C_configuration(Box_av1C::configuration* inout_config, const std::shared_ptr<HeifPixelImage>& image)
{
  int bpp = image->get_bits_per_pixel(heif_channel_Y);
  heif_chroma chroma = image->get_chroma_format();

  uint8_t profile = compute_avif_profile(bpp, chroma);

  int width = image->get_width(heif_channel_Y);
  int height = image->get_height(heif_channel_Y);

  uint8_t level;

  if (width <= 8192 && height <= 4352 && (width * height) <= 8912896) {
    level = 13; // 5.1
  }
  else if (width <= 16384 && height <= 8704 && (width * height) <= 35651584) {
    level = 17; // 6.1
  }
  else {
    level = 31; // maximum
  }

  inout_config->seq_profile = profile;
  inout_config->seq_level_idx_0 = level;
  inout_config->high_bitdepth = (bpp > 8) ? 1 : 0;
  inout_config->twelve_bit = (bpp >= 12) ? 1 : 0;
  inout_config->monochrome = (chroma == heif_chroma_monochrome) ? 1 : 0;
  inout_config->chroma_subsampling_x = uint8_t(chroma_h_subsampling(chroma) >> 1);
  inout_config->chroma_subsampling_y = uint8_t(chroma_v_subsampling(chroma) >> 1);

  // 0 - CSP_UNKNOWN
  // 1 - CSP_VERTICAL
  // 2 - CSP_COLOCATED
  // 3 - CSP_RESERVED

  inout_config->chroma_sample_position = (chroma == heif_chroma_420 ? 0 : 2);


  return Error::Ok;
}


Error Box_a1op::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  op_index = range.read8();

  return range.get_error();
}


std::string Box_a1op::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  sstr << indent << "op-index: " << ((int) op_index) << "\n";

  return sstr.str();
}


Error Box_a1op::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write8(op_index);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_a1lx::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint8_t flags = range.read8();

  for (int i = 0; i < 3; i++) {
    if (flags & 1) {
      layer_size[i] = range.read32();
    }
    else {
      layer_size[i] = range.read16();
    }
  }

  return range.get_error();
}


std::string Box_a1lx::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  sstr << indent << "layer-sizes: [" << layer_size[0] << "," << layer_size[1] << "," << layer_size[2] << "]\n";

  return sstr.str();
}


Error Box_a1lx::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  bool large = (layer_size[0] > 0xFFFF || layer_size[1] > 0xFFFF || layer_size[2] > 0xFFFF);
  writer.write8(large ? 1 : 0);

  for (int i = 0; i < 3; i++) {
    if (large) {
      writer.write32(layer_size[i]);
    }
    else {
      writer.write16((uint16_t) layer_size[i]);
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


static uint64_t leb128(BitReader& reader)
{
  uint64_t val = 0;
  for (int i = 0; i < 8; i++) {
    int64_t v = reader.get_bits(8);
    val |= (v & 0x7F) << (i * 7);
    if (!(v & 0x80)) {
      break;
    }
  }

  return val;
}


struct obu_header_info
{
  int type;
  bool has_size;
  uint64_t size = 0;
};

static obu_header_info read_obu_header_type(BitReader& reader)
{
  obu_header_info info;

  reader.skip_bits(1);
  info.type = reader.get_bits(4);
  bool has_extension = reader.get_bits(1);
  info.has_size = reader.get_bits(1);
  reader.skip_bits(1);

  if (has_extension) {
    reader.skip_bits(8);
  }

  if (info.has_size) {
    info.size = leb128(reader);
  }

  return info;
}


const static int HEIF_OBU_SEQUENCE_HEADER = 1;
const static int CP_UNSPECIFIED = 2;
const static int TC_UNSPECIFIED = 2;
const static int MC_UNSPECIFIED = 2;
const static int CP_BT_709 = 1;
const static int TC_SRGB = 13;
const static int MC_IDENTITY = 0;

const static int HEIF_CSP_UNKNOWN = 0;
// 1 - CSP_VERTICAL
// 2 - CSP_COLOCATED
// 3 - CSP_RESERVED

bool fill_av1C_configuration_from_stream(Box_av1C::configuration* out_config, const uint8_t* data, int dataSize)
{
  BitReader reader(data, dataSize);

  // --- find OBU_SEQUENCE_HEADER

  bool seq_header_found = false;

  while (reader.get_bits_remaining() > 0) {
    obu_header_info header_info = read_obu_header_type(reader);
    if (header_info.type == HEIF_OBU_SEQUENCE_HEADER) {
      seq_header_found = true;
      break;
    }
    else if (header_info.has_size) {
      if (header_info.size > (uint64_t)std::numeric_limits<int>::max()) {
        return false;
      }

      reader.skip_bytes((int)header_info.size);
    }
    else {
      return false;
    }
  }

  if (!seq_header_found) {
    return false;
  }


  // --- read sequence header

  int dummy; // throw away value

  bool decoder_model_info_present = false;
  int buffer_delay_length_minus1 = 0;

  out_config->seq_profile = (uint8_t)reader.get_bits(3);
  bool still_picture = reader.get_bits(1);
  (void) still_picture;

  bool reduced_still_picture = reader.get_bits(1);
  if (reduced_still_picture) {
    out_config->seq_level_idx_0 = (uint8_t)reader.get_bits(5);
    out_config->seq_tier_0 = 0;
  }
  else {
    bool timing_info_present_flag = reader.get_bits(1);
    if (timing_info_present_flag) {
      // --- skip timing info
      reader.skip_bytes(2 * 4);
      bool equal_picture_interval = reader.get_bits(1);
      if (equal_picture_interval) {
        reader.get_uvlc(&dummy);
      }

      // --- skip decoder_model_info
      decoder_model_info_present = reader.get_bits(1);
      if (decoder_model_info_present) {
        buffer_delay_length_minus1 = reader.get_bits(5);
        reader.skip_bits(32);
        reader.skip_bits(10);
      }
    }

    bool initial_display_delay_present_flag = reader.get_bits(1);
    int operating_points_cnt_minus1 = reader.get_bits(5);
    for (int i = 0; i <= operating_points_cnt_minus1; i++) {
      reader.skip_bits(12);
      auto level = (uint8_t) reader.get_bits(5);
      if (i == 0) {
        out_config->seq_level_idx_0 = level;
      }
      if (level > 7) {
        auto tier = (uint8_t) reader.get_bits(1);
        if (i == 0) {
          out_config->seq_tier_0 = tier;
        }
      }

      if (decoder_model_info_present) {
        bool decoder_model_present_for_this = reader.get_bits(1);
        if (decoder_model_present_for_this) {
          int n = buffer_delay_length_minus1 + 1;
          reader.skip_bits(n);
          reader.skip_bits(n);
          reader.skip_bits(1);
        }
      }

      if (initial_display_delay_present_flag) {
        bool initial_display_delay_present_for_this = reader.get_bits(1);
        if (i==0) {
          out_config->initial_presentation_delay_present = initial_display_delay_present_for_this;
        }

        if (initial_display_delay_present_for_this) {
          auto delay = (uint8_t)reader.get_bits(4);
          if (i==0) {
            out_config->initial_presentation_delay_minus_one = delay;
          }
        }
      }
    }
  }

  int frame_width_bits_minus1 = reader.get_bits(4);
  int frame_height_bits_minus1 = reader.get_bits(4);
  int max_frame_width_minus1 = reader.get_bits(frame_width_bits_minus1 + 1);
  int max_frame_height_minus1 = reader.get_bits(frame_height_bits_minus1 + 1);
  (void)max_frame_width_minus1;
  (void)max_frame_height_minus1;

  // printf("max size: %d x %d\n", max_frame_width_minus1+1, max_frame_height_minus1+1);

  int frame_id_numbers_present_flag = 0;
  if (!reduced_still_picture) {
    frame_id_numbers_present_flag = reader.get_bits(1);
  }
  if (frame_id_numbers_present_flag) {
    reader.skip_bits(7);
  }

  reader.skip_bits(3);
  if (!reduced_still_picture) {
    reader.skip_bits(4);

    // order hint
    bool enable_order_hint = reader.get_bits(1);
    if (enable_order_hint) {
      reader.skip_bits(2);
    }

    // screen content
    int force_screen_content_tools = 2;
    if (reader.get_bits(1) == 0) {
      force_screen_content_tools = reader.get_bits(1);
    }

    if (force_screen_content_tools > 0) {
      // integer mv
      if (reader.get_bits(1) == 0) {
        reader.skip_bits(1);
      }
    }

    if (enable_order_hint) {
      reader.skip_bits(3);
    }
  }

  reader.skip_bits(3);

  // --- color config

  out_config->high_bitdepth = (uint8_t)reader.get_bits(1);
  if (out_config->seq_profile == 2 && out_config->high_bitdepth) {
    out_config->twelve_bit = (uint8_t)reader.get_bits(1);
  }
  else {
    out_config->twelve_bit = 0;
  }

  if (out_config->seq_profile == 1) {
    out_config->monochrome = 0;
  }
  else {
    out_config->monochrome = (uint8_t)reader.get_bits(1);
  }

  int color_primaries = CP_UNSPECIFIED;
  int transfer_characteristics = TC_UNSPECIFIED;
  int matrix_coefficients = MC_UNSPECIFIED;

  bool color_description_preset_flag = reader.get_bits(1);
  if (color_description_preset_flag) {
    color_primaries = reader.get_bits(8);
    transfer_characteristics = reader.get_bits(8);
    matrix_coefficients = reader.get_bits(8);
  }
  else {
    // color description unspecified
  }

  if (out_config->monochrome) {
    reader.skip_bits(1);
    out_config->chroma_subsampling_x = 1;
    out_config->chroma_subsampling_y = 1;
    out_config->chroma_sample_position = HEIF_CSP_UNKNOWN;
  }
  else if (color_primaries == CP_BT_709 &&
           transfer_characteristics == TC_SRGB &&
           matrix_coefficients == MC_IDENTITY) {
    out_config->chroma_subsampling_x = 0;
    out_config->chroma_subsampling_y = 0;
  }
  else {
    reader.skip_bits(1);
    if (out_config->seq_profile == 0) {
      out_config->chroma_subsampling_x = 1;
      out_config->chroma_subsampling_y = 1;
    }
    else if (out_config->seq_profile == 1) {
      out_config->chroma_subsampling_x = 0;
      out_config->chroma_subsampling_y = 0;
    }
    else {
      if (out_config->twelve_bit) {
        out_config->chroma_subsampling_x = (uint8_t)reader.get_bits(1);
        if (out_config->chroma_subsampling_x) {
          out_config->chroma_subsampling_y = (uint8_t)reader.get_bits(1);
        }
        else {
          out_config->chroma_subsampling_y = 0;
        }
      }
      else {
        out_config->chroma_subsampling_x = 1;
        out_config->chroma_subsampling_y = 0;
      }
    }

    if (out_config->chroma_subsampling_x &&
        out_config->chroma_subsampling_y) {
      out_config->chroma_sample_position = (uint8_t)reader.get_bits(2);
    }
  }

  reader.skip_bits(1); // separate_uv_delta

  return true;
}
