/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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

#include "heif_image.h"
#include "heif_avif.h"
#include "bitstream.h"
#include <limits>

using namespace heif;

// https://aomediacodec.github.io/av1-spec/av1-spec.pdf


Error heif::fill_av1C_configuration(Box_av1C::configuration* inout_config, const std::shared_ptr<HeifPixelImage>& image)
{
  int bpp = image->get_bits_per_pixel(heif_channel_Y);
  heif_chroma chroma = image->get_chroma_format();

  uint8_t profile;

  if (bpp <= 10 &&
      (chroma == heif_chroma_420 ||
       chroma == heif_chroma_monochrome)) {
    profile = 0;
  }
  else if (bpp <= 10 &&
           chroma == heif_chroma_444) {
    profile = 1;
  }
  else {
    profile = 2;
  }

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

bool heif::fill_av1C_configuration_from_stream(Box_av1C::configuration* out_config, const uint8_t* data, int dataSize)
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
    int force_screen_content_tools = 0;
    if (reader.get_bits(1)) {
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
