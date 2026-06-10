/*
 * HEIF codec.
 * Copyright (c) 2024 Brad Hards <bradh@frogmouth.net>
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#include "mini.h"
#include "file.h"
#include "nclx.h"
#include "security_limits.h"
#include "codecs/avif_boxes.h"
#include "codecs/hevc_boxes.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <utility>


Error Box_mini::parse(BitstreamRange &range, const heif_security_limits *limits)
{
  uint64_t start_offset = range.get_istream()->get_position();
  std::size_t length = range.get_remaining_bytes();

  // Register the payload allocation with the total-memory tracker (also
  // checks against max_memory_block_size). The buffer is local to parse(),
  // so use a scoped handle that releases the budget when parse() returns.
  MemoryHandle mini_data_handle;
  if (auto err = mini_data_handle.alloc(length, limits, "MinimizedImageBox payload")) {
    return err;
  }

  std::vector<uint8_t> mini_data(length);
  if (!range.read(mini_data.data(), mini_data.size())) {
    return range.get_error();
  }

  BitReader bits(mini_data.data(), (int)(mini_data.size()));

  m_version = bits.get_bits8(2);
  m_explicit_codec_types_flag = bits.get_flag();
  m_float_flag = bits.get_flag();
  m_full_range_flag = bits.get_flag();
  m_alpha_flag = bits.get_flag();
  m_explicit_cicp_flag = bits.get_flag();
  m_hdr_flag = bits.get_flag();
  m_icc_flag = bits.get_flag();
  m_exif_flag = bits.get_flag();
  m_xmp_flag = bits.get_flag();
  m_chroma_subsampling = bits.get_bits8(2);
  m_orientation = bits.get_bits8(3) + 1;

  bool large_dimensions_flag = bits.get_flag();
  // large_dimensions_flag = !large_dimensions_flag;  // HACK to get old behavior
  m_width = 1 + bits.get_bits32(large_dimensions_flag ? 15 : 7);
  m_height = 1 + bits.get_bits32(large_dimensions_flag ? 15 : 7);

  if ((m_chroma_subsampling == 1) || (m_chroma_subsampling == 2))
  {
    m_chroma_is_horizontally_centered = bits.get_flag();
  }
  if (m_chroma_subsampling == 1)
  {
    m_chroma_is_vertically_centered = bits.get_flag();
  }

  bool high_bit_depth_flag = false;
  if (m_float_flag)
  {
    uint8_t bit_depth_log2 = bits.get_bits8(2) + 4; // [4;6] (7 is reserved)
    if (bit_depth_log2 > 6) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_mini_box,
              "Reserved float bit_depth_log2 value 7 in MinimizedImageBox"};
    }
    m_bit_depth = (uint8_t)powl(2, (bit_depth_log2)); // [16,32,64]
  }
  else
  {
    high_bit_depth_flag = bits.get_flag();
    if (high_bit_depth_flag)
    {
      m_bit_depth = bits.get_bits8(3) + 9; // [9;16]
    }
  }

  if (m_alpha_flag)
  {
    m_alpha_is_premultiplied = bits.get_flag();
  }

  if (m_explicit_cicp_flag)
  {
    // Per ISO/IEC 23008-12 Annex O.3.2, matrix_coefficients is always
    // present (8 bits) when explicit_cicp_flag is set, irrespective of
    // chroma_subsampling. The chroma_subsampling==0 ⇒ MC=2 default is
    // only used in the non-explicit branch (Table O.3, row 1).
    m_colour_primaries = bits.get_bits8(8);
    m_transfer_characteristics = bits.get_bits8(8);
    m_matrix_coefficients = bits.get_bits8(8);
  }
  else
  {
    m_colour_primaries = m_icc_flag ? 2 : 1;
    m_transfer_characteristics = m_icc_flag ? 2 : 13;
    m_matrix_coefficients = (m_chroma_subsampling == 0) ? 2 : 6;
  }

  if (m_explicit_codec_types_flag)
  {
    m_infe_type = bits.get_bits32(32);
    m_codec_config_type = bits.get_bits32(32);
  }

  if (m_hdr_flag)
  {
    m_gainmap_flag = bits.get_flag();
    if (m_gainmap_flag)
    {
      bool gainmap_dimension_same_as_main_item_flag = bits.get_flag();
      if (gainmap_dimension_same_as_main_item_flag) {
        m_gainmap_width = m_width;
        m_gainmap_height = m_height;
      }
      else {
        m_gainmap_width = bits.get_bits32(large_dimensions_flag ? 15 : 7) + 1;
        m_gainmap_height = bits.get_bits32(large_dimensions_flag ? 15 : 7) + 1;
      }

      m_gainmap_matrix_coefficients = bits.get_bits8(8);
      m_gainmap_full_range_flag = bits.get_flag();
      m_gainmap_chroma_subsampling = bits.get_bits8(2);
      if ((m_gainmap_chroma_subsampling == 1) || (m_gainmap_chroma_subsampling == 2))
      {
        m_gainmap_chroma_is_horizontally_centred = bits.get_flag();
      }
      if (m_gainmap_chroma_subsampling == 1)
      {
        m_gainmap_chroma_is_vertically_centred = bits.get_flag();
      }

      m_gainmap_float_flag = bits.get_flag();

      bool gainmap_high_bit_depth_flag = false;
      if (m_gainmap_float_flag)
      {
        uint8_t bit_depth_log2 = bits.get_bits8(2) + 4;
        if (bit_depth_log2 > 6) {
          return {heif_error_Invalid_input,
                  heif_suberror_Invalid_mini_box,
                  "Reserved float gainmap bit_depth_log2 value 7 in MinimizedImageBox"};
        }
        m_gainmap_bit_depth = (uint8_t)powl(2, (bit_depth_log2));
      }
      else
      {
        gainmap_high_bit_depth_flag = bits.get_flag();
        if (gainmap_high_bit_depth_flag)
        {
          m_gainmap_bit_depth = 9 + bits.get_bits8(3);
        }
      }

      m_tmap_icc_flag = bits.get_flag();
      m_tmap_explicit_cicp_flag = bits.get_flag();
      if (m_tmap_explicit_cicp_flag)
      {
        m_tmap_colour_primaries = bits.get_bits8(8);
        m_tmap_transfer_characteristics = bits.get_bits8(8);
        m_tmap_matrix_coefficients = bits.get_bits8(8);
        m_tmap_full_range_flag = bits.get_flag();
      }
      else
      {
        m_tmap_colour_primaries = 1;
        m_tmap_transfer_characteristics = 13;
        m_tmap_matrix_coefficients = 6;
        m_tmap_full_range_flag = true;
      }
    }

    bool clli_flag = bits.get_flag();
    bool mdcv_flag = bits.get_flag();
    bool cclv_flag = bits.get_flag();
    bool amve_flag = bits.get_flag();
    m_reve_flag = bits.get_flag();
    bool ndwt_flag = bits.get_flag();

    if (clli_flag)
    {
      m_clli = std::make_shared<Box_clli>();
      m_clli->clli.max_content_light_level = bits.get_bits16(16);
      m_clli->clli.max_pic_average_light_level = bits.get_bits16(16);
    }

    if (mdcv_flag)
    {
      m_mdcv = std::make_shared<Box_mdcv>();
      for (int c = 0; c < 3; c++)
      {
        m_mdcv->mdcv.display_primaries_x[c] = bits.get_bits16(16);
        m_mdcv->mdcv.display_primaries_y[c] = bits.get_bits16(16);
      }

      m_mdcv->mdcv.white_point_x = bits.get_bits16(16);
      m_mdcv->mdcv.white_point_y = bits.get_bits16(16);
      m_mdcv->mdcv.max_display_mastering_luminance = bits.get_bits32(32);
      m_mdcv->mdcv.min_display_mastering_luminance = bits.get_bits32(32);
    }

    if (cclv_flag)
    {
      m_cclv = std::make_shared<Box_cclv>();
      bits.skip_bits(2);
      bool ccv_primaries_present_flag = bits.get_flag();
      bool ccv_min_luminance_value_present_flag = bits.get_flag();
      bool ccv_max_luminance_value_present_flag = bits.get_flag();
      bool ccv_avg_luminance_value_present_flag = bits.get_flag();
      bits.skip_bits(2);
      if (ccv_primaries_present_flag)
      {
        int32_t x0 = bits.get_bits32s();
        int32_t y0 = bits.get_bits32s();
        int32_t x1 = bits.get_bits32s();
        int32_t y1 = bits.get_bits32s();
        int32_t x2 = bits.get_bits32s();
        int32_t y2 = bits.get_bits32s();
        m_cclv->set_primaries(x0, y0, x1, y1, x2, y2);
      }
      if (ccv_min_luminance_value_present_flag)
      {
        m_cclv->set_min_luminance(bits.get_bits32(32));
      }
      if (ccv_max_luminance_value_present_flag)
      {
        m_cclv->set_max_luminance(bits.get_bits32(32));
      }
      if (ccv_avg_luminance_value_present_flag)
      {
        m_cclv->set_avg_luminance(bits.get_bits32(32));
      }
    }

    if (amve_flag)
    {
      m_amve = std::make_shared<Box_amve>();
      m_amve->amve.ambient_illumination = bits.get_bits32(32);
      m_amve->amve.ambient_light_x = bits.get_bits16(16);
      m_amve->amve.ambient_light_y = bits.get_bits16(16);
    }

    if (m_reve_flag)
    {
      // TODO: ReferenceViewingEnvironment isn't published yet
      bits.skip_bits(32);
      bits.skip_bits(16);
      bits.skip_bits(16);
      bits.skip_bits(32);
      bits.skip_bits(16);
      bits.skip_bits(16);
    }

    if (ndwt_flag)
    {
      m_ndwt = std::make_shared<Box_ndwt>();
      m_ndwt->set_diffuse_white_luminance(bits.get_bits32(32));
    }

    if (m_gainmap_flag)
    {
      bool tmap_clli_flag = bits.get_flag();
      bool tmap_mdcv_flag = bits.get_flag();
      bool tmap_cclv_flag = bits.get_flag();
      bool tmap_amve_flag = bits.get_flag();
      m_tmap_reve_flag = bits.get_flag();
      bool tmap_ndwt_flag = bits.get_flag();

      if (tmap_clli_flag)
      {
        m_tmap_clli = std::make_shared<Box_clli>();
        m_tmap_clli->clli.max_content_light_level = (uint16_t)bits.get_bits32(16);
        m_tmap_clli->clli.max_pic_average_light_level = (uint16_t)bits.get_bits32(16);
      }

      if (tmap_mdcv_flag)
      {
        m_tmap_mdcv = std::make_shared<Box_mdcv>();
        for (int c = 0; c < 3; c++)
        {
          m_tmap_mdcv->mdcv.display_primaries_x[c] = bits.get_bits16(16);
          m_tmap_mdcv->mdcv.display_primaries_y[c] = bits.get_bits16(16);
        }

        m_tmap_mdcv->mdcv.white_point_x = bits.get_bits16(16);
        m_tmap_mdcv->mdcv.white_point_y = bits.get_bits16(16);
        m_tmap_mdcv->mdcv.max_display_mastering_luminance = bits.get_bits32(32);
        m_tmap_mdcv->mdcv.min_display_mastering_luminance = bits.get_bits32(32);
      }

      if (tmap_cclv_flag)
      {
        m_tmap_cclv = std::make_shared<Box_cclv>();
        bits.skip_bits(2); // skip ccv_cancel_flag and ccv_persistence_flag
        bool ccv_primaries_present_flag = bits.get_flag();
        bool ccv_min_luminance_value_present_flag = bits.get_flag();
        bool ccv_max_luminance_value_present_flag = bits.get_flag();
        bool ccv_avg_luminance_value_present_flag = bits.get_flag();
        bits.skip_bits(2);
        if (ccv_primaries_present_flag)
        {
          int32_t x0 = bits.get_bits32s();
          int32_t y0 = bits.get_bits32s();
          int32_t x1 = bits.get_bits32s();
          int32_t y1 = bits.get_bits32s();
          int32_t x2 = bits.get_bits32s();
          int32_t y2 = bits.get_bits32s();
          m_tmap_cclv->set_primaries(x0, y0, x1, y1, x2, y2);
        }
        if (ccv_min_luminance_value_present_flag)
        {
          m_tmap_cclv->set_min_luminance(bits.get_bits32(32));
        }
        if (ccv_max_luminance_value_present_flag)
        {
          m_tmap_cclv->set_max_luminance(bits.get_bits32(32));
        }
        if (ccv_avg_luminance_value_present_flag)
        {
          m_tmap_cclv->set_avg_luminance(bits.get_bits32(32));
        }
      }

      if (tmap_amve_flag)
      {
        m_tmap_amve = std::make_shared<Box_amve>();
        m_tmap_amve->amve.ambient_illumination = bits.get_bits32(32);
        m_tmap_amve->amve.ambient_light_x = bits.get_bits16(16);
        m_tmap_amve->amve.ambient_light_y = bits.get_bits16(16);
      }

      if (m_tmap_reve_flag)
      {
        // TODO: ReferenceViewingEnvironment isn't published yet
        bits.skip_bits(32);
        bits.skip_bits(16);
        bits.skip_bits(16);
        bits.skip_bits(32);
        bits.skip_bits(16);
        bits.skip_bits(16);
      }

      if (tmap_ndwt_flag)
      {
        m_tmap_ndwt = std::make_shared<Box_ndwt>();
        m_tmap_ndwt->set_diffuse_white_luminance(bits.get_bits32(32));
      }
    }
  }

  // Chunk sizes
  bool large_metadata_flag = false;
  if (m_icc_flag || m_exif_flag || m_xmp_flag || (m_hdr_flag && m_gainmap_flag))
  {
    large_metadata_flag = bits.get_flag();
  }

  bool large_codec_config_flag = bits.get_flag();
  bool large_item_data_flag = bits.get_flag();

  uint32_t icc_data_size = 0;
  if (m_icc_flag)
  {
    icc_data_size = bits.get_bits32(large_metadata_flag ? 20 : 10) + 1;
  }

  uint32_t tmap_icc_data_size = 0;
  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag)
  {
    tmap_icc_data_size = bits.get_bits32(large_metadata_flag ? 20 : 10) + 1;
  }

  uint32_t gainmap_metadata_size = 0;
  if (m_hdr_flag && m_gainmap_flag)
  {
    gainmap_metadata_size = bits.get_bits32(large_metadata_flag ? 20 : 10);
  }

  if (m_hdr_flag && m_gainmap_flag)
  {
    m_gainmap_item_data_size = bits.get_bits32(large_item_data_flag ? 28 : 15);
  }

  uint32_t gainmap_item_codec_config_size = 0;
  if (m_hdr_flag && m_gainmap_flag && m_gainmap_item_data_size > 0)
  {
    gainmap_item_codec_config_size = bits.get_bits32(large_codec_config_flag ? 12 : 3);
  }

  uint32_t main_item_codec_config_size = bits.get_bits32(large_codec_config_flag ? 12 : 3);
  m_main_item_data_size = bits.get_bits32(large_item_data_flag ? 28 : 15) + 1;

  if (m_alpha_flag)
  {
    m_alpha_item_data_size = bits.get_bits32(large_item_data_flag ? 28 : 15);
  }

  uint32_t alpha_item_codec_config_size = 0;
  if (m_alpha_flag && m_alpha_item_data_size > 0)
  {
    alpha_item_codec_config_size = bits.get_bits32(large_codec_config_flag ? 12 : 3);
  }

  if (m_exif_flag || m_xmp_flag)
  {
    m_exif_xmp_compressed_flag = bits.get_flag();
  }

  if (m_exif_flag)
  {
    m_exif_data_size = bits.get_bits32(large_metadata_flag ? 20 : 10) + 1;
  }
  if (m_xmp_flag)
  {
    m_xmp_data_size = bits.get_bits32(large_metadata_flag ? 20 : 10) + 1;
  }

  bits.skip_to_byte_boundary();

  // Validate that the declared chunk sizes don't exceed the remaining payload.
  // Without this, a malformed mini box with 28-bit *_item_data_size fields set
  // near 2^28 makes skip_bytes/read_bytes loop hundreds of millions of times
  // past EOF, eventually triggering signed-int overflow in BitReader::refill().
  {
    uint64_t required_bytes = (uint64_t) main_item_codec_config_size + m_main_item_data_size;
    if (m_alpha_flag && m_alpha_item_data_size > 0) {
      required_bytes += alpha_item_codec_config_size;
      required_bytes += m_alpha_item_data_size;
    }
    if (m_hdr_flag && m_gainmap_flag && m_gainmap_item_data_size > 0) {
      required_bytes += gainmap_item_codec_config_size;
      required_bytes += m_gainmap_item_data_size;
    }
    if (m_icc_flag) required_bytes += icc_data_size;
    if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag) required_bytes += tmap_icc_data_size;
    if (m_hdr_flag && m_gainmap_flag) required_bytes += gainmap_metadata_size;
    if (m_exif_flag) required_bytes += m_exif_data_size;
    if (m_xmp_flag) required_bytes += m_xmp_data_size;

    int64_t remaining_bits = bits.get_bits_remaining();
    if (remaining_bits < 0 || required_bytes > (uint64_t) remaining_bits / 8) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_mini_box,
              "Declared chunk sizes in MinimizedImageBox exceed available payload."};
    }
  }

  // Enforce the color-profile size limit on embedded ICC blobs, matching
  // the check applied to regular 'colr' boxes in nclx.cc.
  if (limits && limits->max_color_profile_size) {
    if (m_icc_flag && icc_data_size > limits->max_color_profile_size) {
      return {heif_error_Invalid_input,
              heif_suberror_Security_limit_exceeded,
              "ICC color profile in MinimizedImageBox exceeds maximum supported size"};
    }
    if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag &&
        tmap_icc_data_size > limits->max_color_profile_size) {
      return {heif_error_Invalid_input,
              heif_suberror_Security_limit_exceeded,
              "Tone-map ICC color profile in MinimizedImageBox exceeds maximum supported size"};
    }
  }

  if (main_item_codec_config_size > 0)
  {
    m_main_item_codec_config = bits.read_bytes(main_item_codec_config_size);
  }

  // Chunks
  if (m_alpha_flag && m_alpha_item_data_size > 0) {
    if (alpha_item_codec_config_size == 0) {
      m_alpha_item_codec_config = m_main_item_codec_config;
    }
    else {
      // TODO: should we have a flag indicating that we have explicit config for alpha?
      m_alpha_item_codec_config = bits.read_bytes(alpha_item_codec_config_size);
    }
  }

  if (m_hdr_flag && m_gainmap_flag && m_gainmap_item_data_size > 0)
  {
    if (gainmap_item_codec_config_size == 0) {
      m_gainmap_item_codec_config = m_main_item_codec_config;
    }
    else {
      // TODO: should we have a flag indicating that we have explicit config for the gain map?
      m_gainmap_item_codec_config = bits.read_bytes(gainmap_item_codec_config_size);
    }
  }

  if (m_icc_flag)
  {
    m_icc_data = bits.read_bytes(icc_data_size);
  }

  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag)
  {
    m_tmap_icc_data = bits.read_bytes(tmap_icc_data_size);
  }

  if (m_hdr_flag && m_gainmap_flag && gainmap_metadata_size > 0)
  {
    m_gainmap_metadata = bits.read_bytes(gainmap_metadata_size);
  }

  if (m_alpha_flag && m_alpha_item_data_size > 0)
  {
    m_alpha_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_alpha_item_data_size);
  }
  if (m_hdr_flag && m_gainmap_flag && m_gainmap_item_data_size > 0)
  {
    m_gainmap_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_gainmap_item_data_size);
  }

  m_main_item_data_offset = bits.get_current_byte_index() + start_offset;
  bits.skip_bytes(m_main_item_data_size);

  if (m_exif_flag)
  {
    m_exif_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_exif_data_size);
  }
  if (m_xmp_flag)
  {
    m_xmp_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_xmp_data_size);
  }

  return range.get_error();
}

std::string Box_mini::dump(Indent &indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);
  sstr << indent << "version: " << (int)m_version << "\n";

  sstr << indent << "explicit_codec_types_flag: " << m_explicit_codec_types_flag << "\n";
  sstr << indent << "float_flag: " << m_float_flag << "\n";
  sstr << indent << "full_range_flag: " << m_full_range_flag << "\n";
  sstr << indent << "alpha_flag: " << m_alpha_flag << "\n";
  sstr << indent << "explicit_cicp_flag: " << m_explicit_cicp_flag << "\n";
  sstr << indent << "hdr_flag: " << m_hdr_flag << "\n";
  sstr << indent << "icc_flag: " << m_icc_flag << "\n";
  sstr << indent << "exif_flag: " << m_exif_flag << "\n";
  sstr << indent << "xmp_flag: " << m_xmp_flag << "\n";

  sstr << indent << "chroma_subsampling: " << (int)m_chroma_subsampling << "\n";
  sstr << indent << "orientation: " << (int)m_orientation << "\n";

  sstr << indent << "width: " << m_width << "\n";
  sstr << indent << "height: " << m_height << "\n";

  if ((m_chroma_subsampling == 1) || (m_chroma_subsampling == 2))
  {
    sstr << indent << "chroma_is_horizontally_centered: " << m_chroma_is_horizontally_centered << "\n";
  }
  if (m_chroma_subsampling == 1)
  {
    sstr << indent << "chroma_is_vertically_centered: " << m_chroma_is_vertically_centered << "\n";
  }

  sstr << "bit_depth: " << (int)m_bit_depth << "\n";

  if (m_alpha_flag)
  {
    sstr << "alpha_is_premultiplied: " << m_alpha_is_premultiplied << "\n";
  }

  sstr << "colour_primaries: " << (int)m_colour_primaries << "\n";
  sstr << "transfer_characteristics: " << (int)m_transfer_characteristics << "\n";
  sstr << "matrix_coefficients: " << (int)m_matrix_coefficients << "\n";

  if (m_explicit_codec_types_flag)
  {
    sstr << "infe_type: " << fourcc_to_string(m_infe_type) << " (" << m_infe_type << ")" << "\n";
    sstr << "codec_config_type: " << fourcc_to_string(m_codec_config_type) << " (" << m_codec_config_type << ")" << "\n";
  }

  if (m_hdr_flag)
  {
    sstr << indent << "gainmap_flag: " << m_gainmap_flag << "\n";
    if (m_gainmap_flag)
    {
      sstr << indent << "gainmap_width: " << m_gainmap_width << "\n";
      sstr << indent << "gainmap_height: " << m_gainmap_height << "\n";
      sstr << indent << "gainmap_matrix_coefficients: " << (int)m_gainmap_matrix_coefficients << "\n";
      sstr << indent << "gainmap_full_range_flag: " << m_gainmap_full_range_flag << "\n";
      sstr << indent << "gainmap_chroma_subsampling: " << (int)m_gainmap_chroma_subsampling << "\n";
      if ((m_gainmap_chroma_subsampling == 1) || (m_gainmap_chroma_subsampling == 2))
      {
        sstr << indent << "gainmap_chroma_is_horizontally_centred: " << m_gainmap_chroma_is_horizontally_centred << "\n";
      }
      if (m_gainmap_chroma_subsampling == 1)
      {
        sstr << indent << "gainmap_chroma_is_vertically_centred: " << m_gainmap_chroma_is_vertically_centred << "\n";
      }
      sstr << indent << "gainmap_float_flag: " << m_gainmap_float_flag << "\n";
      sstr << "gainmap_bit_depth: " << (int)m_gainmap_bit_depth << "\n";
      sstr << indent << "tmap_icc_flag: " << m_tmap_icc_flag << "\n";
      sstr << indent << "tmap_explicit_cicp_flag: " << m_tmap_explicit_cicp_flag << "\n";
      if (m_tmap_explicit_cicp_flag)
      {
        sstr << "tmap_colour_primaries: " << (int)m_tmap_colour_primaries << "\n";
        sstr << "tmap_transfer_characteristics: " << (int)m_tmap_transfer_characteristics << "\n";
        sstr << "tmap_matrix_coefficients: " << (int)m_tmap_matrix_coefficients << "\n";
        sstr << "tmap_full_range_flag: " << m_tmap_full_range_flag << "\n";
      }
    }

    if (m_clli)
    {
      sstr << indent << "ccli.max_content_light_level: " << m_clli->clli.max_content_light_level << "\n";
      sstr << indent << "ccli.max_pic_average_light_level: " << m_clli->clli.max_pic_average_light_level << "\n";
    }
    else {
      sstr << indent << "clli: ---\n";
    }

    if (m_mdcv)
    {
      sstr << indent << "mdcv.display_primaries (x,y): ";
      sstr << "(" << m_mdcv->mdcv.display_primaries_x[0] << ";" << m_mdcv->mdcv.display_primaries_y[0] << "), ";
      sstr << "(" << m_mdcv->mdcv.display_primaries_x[1] << ";" << m_mdcv->mdcv.display_primaries_y[1] << "), ";
      sstr << "(" << m_mdcv->mdcv.display_primaries_x[2] << ";" << m_mdcv->mdcv.display_primaries_y[2] << ")\n";

      sstr << indent << "mdcv.white point (x,y): (" << m_mdcv->mdcv.white_point_x << ";" << m_mdcv->mdcv.white_point_y << ")\n";
      sstr << indent << "mdcv.max display mastering luminance: " << m_mdcv->mdcv.max_display_mastering_luminance << "\n";
      sstr << indent << "mdcv.min display mastering luminance: " << m_mdcv->mdcv.min_display_mastering_luminance << "\n";
    }
    else {
      sstr << indent << "mdcv: ---\n";
    }

    if (m_cclv)
    {
      sstr << indent << "cclv.ccv_primaries_present_flag: " << m_cclv->ccv_primaries_are_valid() << "\n";
      sstr << indent << "cclv.ccv_min_luminance_value_present_flag: " << m_cclv->min_luminance_is_valid() << "\n";
      sstr << indent << "cclv.ccv_max_luminance_value_present_flag: " << m_cclv->max_luminance_is_valid() << "\n";
      sstr << indent << "cclv.ccv_avg_luminance_value_present_flag: " << m_cclv->avg_luminance_is_valid() << "\n";
      if (m_cclv->ccv_primaries_are_valid())
      {
        sstr << indent << "cclv.ccv_primaries (x,y): ";
        sstr << "(" << m_cclv->get_ccv_primary_x0() << ";" << m_cclv->get_ccv_primary_y0() << "), ";
        sstr << "(" << m_cclv->get_ccv_primary_x1() << ";" << m_cclv->get_ccv_primary_y1() << "), ";
        sstr << "(" << m_cclv->get_ccv_primary_x2() << ";" << m_cclv->get_ccv_primary_y2() << ")\n";
      }
      if (m_cclv->min_luminance_is_valid())
      {
        sstr << indent << "cclv.ccv_min_luminance_value: " << m_cclv->get_min_luminance() << "\n";
      }
      if (m_cclv->max_luminance_is_valid())
      {
        sstr << indent << "cclv.ccv_max_luminance_value: " << m_cclv->get_max_luminance() << "\n";
      }
      if (m_cclv->avg_luminance_is_valid())
      {
        sstr << indent << "cclv.ccv_avg_luminance_value: " << m_cclv->get_avg_luminance() << "\n";
      }
    }
    else {
      sstr << indent << "cclv: ---\n";
    }

    if (m_amve)
    {
      sstr << indent << "amve.ambient_illumination: " << m_amve->amve.ambient_illumination << "\n";
      sstr << indent << "amve.ambient_light_x: " << m_amve->amve.ambient_light_x << "\n";
      sstr << indent << "amve.ambient_light_y: " << m_amve->amve.ambient_light_y << "\n";
    }
    else {
      sstr << indent << "amve: ---\n";
    }

    sstr << indent << "reve_flag: " << m_reve_flag << "\n";

    if (m_reve_flag)
    {
      // TODO - this isn't published yet
    }
    if (m_ndwt)
    {
      sstr << indent << "ndwt.diffuse_white_luminance: " << m_ndwt->get_diffuse_white_luminance() << "\n";
    }
    else {
      sstr << indent << "ndwt: ---\n";
    }

    if (m_gainmap_flag)
    {
      if (m_tmap_clli)
      {
        sstr << indent << "tmap_clli.max_content_light_level: " << m_tmap_clli->clli.max_content_light_level << "\n";
        sstr << indent << "tmap_clli.max_pic_average_light_level: " << m_tmap_clli->clli.max_pic_average_light_level << "\n";
      }
      else {
        sstr << indent << "tmap_clli: ---\n";
      }

      if (m_tmap_mdcv)
      {
        sstr << indent << "tmap_mdcv.display_primaries (x,y): ";
        sstr << "(" << m_tmap_mdcv->mdcv.display_primaries_x[0] << ";" << m_tmap_mdcv->mdcv.display_primaries_y[0] << "), ";
        sstr << "(" << m_tmap_mdcv->mdcv.display_primaries_x[1] << ";" << m_tmap_mdcv->mdcv.display_primaries_y[1] << "), ";
        sstr << "(" << m_tmap_mdcv->mdcv.display_primaries_x[2] << ";" << m_tmap_mdcv->mdcv.display_primaries_y[2] << ")\n";

        sstr << indent << "tmap_mdcv.white point (x,y): (" << m_tmap_mdcv->mdcv.white_point_x << ";" << m_tmap_mdcv->mdcv.white_point_y << ")\n";
        sstr << indent << "tmap_mdcv.max display mastering luminance: " << m_tmap_mdcv->mdcv.max_display_mastering_luminance << "\n";
        sstr << indent << "tmap_mdcv.min display mastering luminance: " << m_tmap_mdcv->mdcv.min_display_mastering_luminance << "\n";
      }
      else {
        sstr << indent << "tmap_mdcv: ---\n";
      }

      if (m_tmap_cclv)
      {
        sstr << indent << "tmap_cclv.ccv_primaries_present_flag: " << m_tmap_cclv->ccv_primaries_are_valid() << "\n";
        sstr << indent << "tmap_cclv.ccv_min_luminance_value_present_flag: " << m_tmap_cclv->min_luminance_is_valid() << "\n";
        sstr << indent << "tmap_cclv.ccv_max_luminance_value_present_flag: " << m_tmap_cclv->max_luminance_is_valid() << "\n";
        sstr << indent << "tmap_cclv.ccv_avg_luminance_value_present_flag: " << m_tmap_cclv->avg_luminance_is_valid() << "\n";
        if (m_tmap_cclv->ccv_primaries_are_valid())
        {
          sstr << indent << "tmap_cclv.ccv_primaries (x,y): ";
          sstr << "(" << m_tmap_cclv->get_ccv_primary_x0() << ";" << m_tmap_cclv->get_ccv_primary_y0() << "), ";
          sstr << "(" << m_tmap_cclv->get_ccv_primary_x1() << ";" << m_tmap_cclv->get_ccv_primary_y1() << "), ";
          sstr << "(" << m_tmap_cclv->get_ccv_primary_x2() << ";" << m_tmap_cclv->get_ccv_primary_y2() << ")\n";
        }
        if (m_tmap_cclv->min_luminance_is_valid())
        {
          sstr << indent << "tmap_cclv.ccv_min_luminance_value: " << m_tmap_cclv->get_min_luminance() << "\n";
        }
        if (m_tmap_cclv->max_luminance_is_valid())
        {
          sstr << indent << "tmap_cclv.ccv_max_luminance_value: " << m_tmap_cclv->get_max_luminance() << "\n";
        }
        if (m_tmap_cclv->avg_luminance_is_valid())
        {
          sstr << indent << "tmap_cclv.ccv_avg_luminance_value: " << m_tmap_cclv->get_avg_luminance() << "\n";
        }
      }
      else {
        sstr << indent << "tmap_cclv: ---\n";
      }

      if (m_tmap_amve)
      {
        sstr << indent << "tmap_amve.ambient_illumination: " << m_tmap_amve->amve.ambient_illumination << "\n";
        sstr << indent << "tmap_amve.ambient_light_x: " << m_tmap_amve->amve.ambient_light_x << "\n";
        sstr << indent << "tmap_amve.ambient_light_y: " << m_tmap_amve->amve.ambient_light_y << "\n";
      }
      else {
        sstr << indent << "tmap_amve: ---\n";
      }

      sstr << indent << "tmap_reve_flag: " << m_tmap_reve_flag << "\n";

      if (m_tmap_reve_flag)
      {
        // TODO - this isn't published yet
      }
      if (m_tmap_ndwt)
      {
        sstr << indent << "tmap_ndwt.diffuse_white_luminance: " << m_tmap_ndwt->get_diffuse_white_luminance() << "\n";
      }
      else {
        sstr << indent << "tmap_ndwt: ---\n";
      }
    }
  }

  if (m_alpha_flag && (m_alpha_item_data_size > 0) && (m_alpha_item_codec_config.size() > 0))
  {
    sstr << "alpha_item_code_config size: " << m_alpha_item_codec_config.size() << "\n";
  }
  if (m_hdr_flag && m_gainmap_flag && m_gainmap_item_codec_config.size() > 0)
  {
    sstr << "gainmap_item_codec_config size: " << m_gainmap_item_codec_config.size() << "\n";
  }
  if (m_main_item_codec_config.size() > 0)
  {
    sstr << "main_item_code_config size: " << m_main_item_codec_config.size() << "\n";
  }

  if (m_icc_flag)
  {
    sstr << "icc_data size: " << m_icc_data.size() << "\n";
  }
  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag)
  {
    sstr << "tmap_icc_data size: " << m_tmap_icc_data.size() << "\n";
  }
  if (m_hdr_flag && m_gainmap_flag && m_gainmap_metadata.size() > 0)
  {
    sstr << "gainmap_metadata size: " << m_gainmap_metadata.size() << "\n";
  }

  if (m_alpha_flag && (m_alpha_item_data_size > 0))
  {
    sstr << "alpha_item_data offset: " << m_alpha_item_data_offset << ", size: " << m_alpha_item_data_size << "\n";
  }
  if (m_hdr_flag && m_gainmap_flag && (m_gainmap_item_data_size > 0))
  {
    sstr << "gainmap_item_data offset: " << m_gainmap_item_data_offset << ", size: " << m_gainmap_item_data_size << "\n";
  }

  sstr << "main_item_data offset: " << m_main_item_data_offset << ", size: " << m_main_item_data_size << "\n";

  if (m_exif_flag)
  {
    sstr << "exif_data offset: " << m_exif_item_data_offset << ", size: " << m_exif_data_size << "\n";
  }
  if (m_xmp_flag)
  {
    sstr << "xmp_data offset: " << m_xmp_item_data_offset << ", size: " << m_xmp_data_size << "\n";
  }
  return sstr.str();
}


static void write_cclv_to_bits(BitWriter& bits, const Box_cclv& cclv)
{
  bool primaries_present = cclv.ccv_primaries_are_valid();
  bool min_lum_present = cclv.min_luminance_is_valid();
  bool max_lum_present = cclv.max_luminance_is_valid();
  bool avg_lum_present = cclv.avg_luminance_is_valid();

  bits.write_bits(0, 2); // ccv_cancel_flag, ccv_persistence_flag
  bits.write_flag(primaries_present);
  bits.write_flag(min_lum_present);
  bits.write_flag(max_lum_present);
  bits.write_flag(avg_lum_present);
  bits.write_bits(0, 2); // reserved

  if (primaries_present) {
    bits.write_bits32s(cclv.get_ccv_primary_x0());
    bits.write_bits32s(cclv.get_ccv_primary_y0());
    bits.write_bits32s(cclv.get_ccv_primary_x1());
    bits.write_bits32s(cclv.get_ccv_primary_y1());
    bits.write_bits32s(cclv.get_ccv_primary_x2());
    bits.write_bits32s(cclv.get_ccv_primary_y2());
  }
  if (min_lum_present) {
    bits.write_bits32(cclv.get_min_luminance(), 32);
  }
  if (max_lum_present) {
    bits.write_bits32(cclv.get_max_luminance(), 32);
  }
  if (avg_lum_present) {
    bits.write_bits32(cclv.get_avg_luminance(), 32);
  }
}


Error Box_mini::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  BitWriter bits;

  // --- Bit-packed header ---

  // First byte: version(2) + 6 flags
  bits.write_bits8(m_version, 2);
  bits.write_flag(m_explicit_codec_types_flag);
  bits.write_flag(m_float_flag);
  bits.write_flag(m_full_range_flag);
  bits.write_flag(m_alpha_flag);
  bits.write_flag(m_explicit_cicp_flag);
  bits.write_flag(m_hdr_flag);

  // Second byte area: icc(1), exif(1), xmp(1), chroma_subsampling(2), orientation(3)
  bits.write_flag(m_icc_flag);
  bits.write_flag(m_exif_flag);
  bits.write_flag(m_xmp_flag);
  bits.write_bits8(m_chroma_subsampling, 2);
  assert(m_orientation >= 1 && m_orientation <= 8);
  bits.write_bits8(m_orientation - 1, 3);

  // Dimensions
  bool large_dimensions_flag = (m_width > 128) || (m_height > 128);
  bits.write_flag(large_dimensions_flag);
  bits.write_bits32(m_width - 1, large_dimensions_flag ? 15 : 7);
  bits.write_bits32(m_height - 1, large_dimensions_flag ? 15 : 7);

  // Chroma centering
  if (m_chroma_subsampling == 1 || m_chroma_subsampling == 2) {
    bits.write_flag(m_chroma_is_horizontally_centered);
  }
  if (m_chroma_subsampling == 1) {
    bits.write_flag(m_chroma_is_vertically_centered);
  }

  // Bit depth
  if (m_float_flag) {
    // bit_depth_log2 = log2(m_bit_depth), stored as (log2 - 4)
    uint8_t bit_depth_log2;
    switch (m_bit_depth) {
      case 16:  bit_depth_log2 = 4; break;
      case 32:  bit_depth_log2 = 5; break;
      case 64:  bit_depth_log2 = 6; break;
      case 128: bit_depth_log2 = 7; break;
      default:  bit_depth_log2 = 4; break; // fallback
    }
    bits.write_bits8(bit_depth_log2 - 4, 2);
  }
  else {
    bool high_bit_depth_flag = (m_bit_depth > 8);
    bits.write_flag(high_bit_depth_flag);
    if (high_bit_depth_flag) {
      bits.write_bits8(m_bit_depth - 9, 3);
    }
  }

  // Alpha premultiplied
  if (m_alpha_flag) {
    bits.write_flag(m_alpha_is_premultiplied);
  }

  // CICP
  if (m_explicit_cicp_flag) {
    // matrix_coefficients is always 8 bits in the explicit branch
    // (ISO/IEC 23008-12 Annex O.3.2).
    bits.write_bits8(static_cast<uint8_t>(m_colour_primaries), 8);
    bits.write_bits8(static_cast<uint8_t>(m_transfer_characteristics), 8);
    bits.write_bits8(static_cast<uint8_t>(m_matrix_coefficients), 8);
  }

  // Explicit codec types
  if (m_explicit_codec_types_flag) {
    bits.write_bits32(m_infe_type, 32);
    bits.write_bits32(m_codec_config_type, 32);
  }

  // --- HDR block ---
  if (m_hdr_flag) {
    bits.write_flag(m_gainmap_flag);

    if (m_gainmap_flag) {
      bool gainmap_dimension_same_as_main_item_flag =
          (m_gainmap_width == m_width && m_gainmap_height == m_height);
      bits.write_flag(gainmap_dimension_same_as_main_item_flag);
      if (!gainmap_dimension_same_as_main_item_flag) {
        bits.write_bits32(m_gainmap_width - 1, large_dimensions_flag ? 15 : 7);
        bits.write_bits32(m_gainmap_height - 1, large_dimensions_flag ? 15 : 7);
      }
      bits.write_bits8(m_gainmap_matrix_coefficients, 8);
      bits.write_flag(m_gainmap_full_range_flag);
      bits.write_bits8(m_gainmap_chroma_subsampling, 2);

      if (m_gainmap_chroma_subsampling == 1 || m_gainmap_chroma_subsampling == 2) {
        bits.write_flag(m_gainmap_chroma_is_horizontally_centred);
      }
      if (m_gainmap_chroma_subsampling == 1) {
        bits.write_flag(m_gainmap_chroma_is_vertically_centred);
      }

      bits.write_flag(m_gainmap_float_flag);

      if (m_gainmap_float_flag) {
        uint8_t gm_bit_depth_log2;
        switch (m_gainmap_bit_depth) {
          case 16:  gm_bit_depth_log2 = 4; break;
          case 32:  gm_bit_depth_log2 = 5; break;
          case 64:  gm_bit_depth_log2 = 6; break;
          case 128: gm_bit_depth_log2 = 7; break;
          default:  gm_bit_depth_log2 = 4; break;
        }
        bits.write_bits8(gm_bit_depth_log2 - 4, 2);
      }
      else {
        bool gainmap_high_bit_depth_flag = (m_gainmap_bit_depth > 8);
        bits.write_flag(gainmap_high_bit_depth_flag);
        if (gainmap_high_bit_depth_flag) {
          bits.write_bits8(m_gainmap_bit_depth - 9, 3);
        }
      }

      bits.write_flag(m_tmap_icc_flag);
      bits.write_flag(m_tmap_explicit_cicp_flag);
      if (m_tmap_explicit_cicp_flag) {
        bits.write_bits8(static_cast<uint8_t>(m_tmap_colour_primaries), 8);
        bits.write_bits8(static_cast<uint8_t>(m_tmap_transfer_characteristics), 8);
        bits.write_bits8(static_cast<uint8_t>(m_tmap_matrix_coefficients), 8);
        bits.write_flag(m_tmap_full_range_flag);
      }
    }

    // HDR metadata flags
    bits.write_flag(m_clli != nullptr);
    bits.write_flag(m_mdcv != nullptr);
    bits.write_flag(m_cclv != nullptr);
    bits.write_flag(m_amve != nullptr);
    bits.write_flag(m_reve_flag);
    bits.write_flag(m_ndwt != nullptr);

    if (m_clli) {
      bits.write_bits16(m_clli->clli.max_content_light_level, 16);
      bits.write_bits16(m_clli->clli.max_pic_average_light_level, 16);
    }

    if (m_mdcv) {
      for (int c = 0; c < 3; c++) {
        bits.write_bits16(m_mdcv->mdcv.display_primaries_x[c], 16);
        bits.write_bits16(m_mdcv->mdcv.display_primaries_y[c], 16);
      }
      bits.write_bits16(m_mdcv->mdcv.white_point_x, 16);
      bits.write_bits16(m_mdcv->mdcv.white_point_y, 16);
      bits.write_bits32(m_mdcv->mdcv.max_display_mastering_luminance, 32);
      bits.write_bits32(m_mdcv->mdcv.min_display_mastering_luminance, 32);
    }

    if (m_cclv) {
      write_cclv_to_bits(bits, *m_cclv);
    }

    if (m_amve) {
      bits.write_bits32(m_amve->amve.ambient_illumination, 32);
      bits.write_bits16(m_amve->amve.ambient_light_x, 16);
      bits.write_bits16(m_amve->amve.ambient_light_y, 16);
    }

    if (m_reve_flag) {
      // TODO: ReferenceViewingEnvironment isn't published yet — write zeros
      bits.write_bits32(0, 32);
      bits.write_bits16(0, 16);
      bits.write_bits16(0, 16);
      bits.write_bits32(0, 32);
      bits.write_bits16(0, 16);
      bits.write_bits16(0, 16);
    }

    if (m_ndwt) {
      bits.write_bits32(m_ndwt->get_diffuse_white_luminance(), 32);
    }

    // Tmap HDR metadata (if gainmap)
    if (m_gainmap_flag) {
      bits.write_flag(m_tmap_clli != nullptr);
      bits.write_flag(m_tmap_mdcv != nullptr);
      bits.write_flag(m_tmap_cclv != nullptr);
      bits.write_flag(m_tmap_amve != nullptr);
      bits.write_flag(m_tmap_reve_flag);
      bits.write_flag(m_tmap_ndwt != nullptr);

      if (m_tmap_clli) {
        bits.write_bits16(m_tmap_clli->clli.max_content_light_level, 16);
        bits.write_bits16(m_tmap_clli->clli.max_pic_average_light_level, 16);
      }

      if (m_tmap_mdcv) {
        for (int c = 0; c < 3; c++) {
          bits.write_bits16(m_tmap_mdcv->mdcv.display_primaries_x[c], 16);
          bits.write_bits16(m_tmap_mdcv->mdcv.display_primaries_y[c], 16);
        }
        bits.write_bits16(m_tmap_mdcv->mdcv.white_point_x, 16);
        bits.write_bits16(m_tmap_mdcv->mdcv.white_point_y, 16);
        bits.write_bits32(m_tmap_mdcv->mdcv.max_display_mastering_luminance, 32);
        bits.write_bits32(m_tmap_mdcv->mdcv.min_display_mastering_luminance, 32);
      }

      if (m_tmap_cclv) {
        write_cclv_to_bits(bits, *m_tmap_cclv);
      }

      if (m_tmap_amve) {
        bits.write_bits32(m_tmap_amve->amve.ambient_illumination, 32);
        bits.write_bits16(m_tmap_amve->amve.ambient_light_x, 16);
        bits.write_bits16(m_tmap_amve->amve.ambient_light_y, 16);
      }

      if (m_tmap_reve_flag) {
        bits.write_bits32(0, 32);
        bits.write_bits16(0, 16);
        bits.write_bits16(0, 16);
        bits.write_bits32(0, 32);
        bits.write_bits16(0, 16);
        bits.write_bits16(0, 16);
      }

      if (m_tmap_ndwt) {
        bits.write_bits32(m_tmap_ndwt->get_diffuse_white_luminance(), 32);
      }
    }
  }

  // --- Size fields ---

  // Determine actual data sizes for write path
  uint32_t icc_data_size = static_cast<uint32_t>(m_icc_data.size());
  uint32_t tmap_icc_data_size = static_cast<uint32_t>(m_tmap_icc_data.size());
  uint32_t gainmap_metadata_size = static_cast<uint32_t>(m_gainmap_metadata.size());
  uint32_t main_item_data_size = static_cast<uint32_t>(m_main_item_data.size());
  uint32_t alpha_item_data_size = static_cast<uint32_t>(m_alpha_item_data.size());
  uint32_t gainmap_item_data_size = static_cast<uint32_t>(m_gainmap_item_data.size());
  uint32_t exif_data_size = static_cast<uint32_t>(m_exif_data_bytes.size());
  uint32_t xmp_data_size = static_cast<uint32_t>(m_xmp_data_bytes.size());

  uint32_t main_item_codec_config_size = static_cast<uint32_t>(m_main_item_codec_config.size());
  uint32_t alpha_item_codec_config_size = 0;
  if (m_alpha_flag && alpha_item_data_size > 0) {
    // If alpha codec config differs from main, we need to write it separately
    if (m_alpha_item_codec_config != m_main_item_codec_config) {
      alpha_item_codec_config_size = static_cast<uint32_t>(m_alpha_item_codec_config.size());
    }
    // else size stays 0, meaning "reuse main config"
  }
  uint32_t gainmap_item_codec_config_size = 0;
  if (m_hdr_flag && m_gainmap_flag && gainmap_item_data_size > 0) {
    if (m_gainmap_item_codec_config != m_main_item_codec_config) {
      gainmap_item_codec_config_size = static_cast<uint32_t>(m_gainmap_item_codec_config.size());
    }
  }

  // Compute "large" flags based on actual sizes
  bool large_metadata_flag = false;
  if (m_icc_flag || m_exif_flag || m_xmp_flag || (m_hdr_flag && m_gainmap_flag)) {
    // Check if any metadata size exceeds 10-bit capacity
    // ICC/exif/xmp store (size-1), max representable size with 10 bits = 1024
    // gainmap_metadata/tmap_icc store raw or (size-1), same limit
    uint32_t max_meta = 0;
    if (m_icc_flag) max_meta = std::max(max_meta, icc_data_size);
    if (m_exif_flag) max_meta = std::max(max_meta, exif_data_size);
    if (m_xmp_flag) max_meta = std::max(max_meta, xmp_data_size);
    if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag) max_meta = std::max(max_meta, tmap_icc_data_size);
    if (m_hdr_flag && m_gainmap_flag) max_meta = std::max(max_meta, gainmap_metadata_size + 1); // gainmap_metadata is raw, others are size-1
    large_metadata_flag = (max_meta > 1024);

    bits.write_flag(large_metadata_flag);
  }

  bool large_codec_config_flag = (main_item_codec_config_size > 7 ||
                                  alpha_item_codec_config_size > 7 ||
                                  gainmap_item_codec_config_size > 7);
  bits.write_flag(large_codec_config_flag);

  bool large_item_data_flag = (main_item_data_size > 32768 ||  // main stores size-1, max representable = 32768
                               alpha_item_data_size > 32767 ||
                               gainmap_item_data_size > 32767);
  bits.write_flag(large_item_data_flag);

  // Write size fields in parse order
  if (m_icc_flag) {
    bits.write_bits32(icc_data_size - 1, large_metadata_flag ? 20 : 10);
  }

  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag) {
    bits.write_bits32(tmap_icc_data_size - 1, large_metadata_flag ? 20 : 10);
  }

  if (m_hdr_flag && m_gainmap_flag) {
    bits.write_bits32(gainmap_metadata_size, large_metadata_flag ? 20 : 10);
  }

  if (m_hdr_flag && m_gainmap_flag) {
    bits.write_bits32(gainmap_item_data_size, large_item_data_flag ? 28 : 15);
  }

  if (m_hdr_flag && m_gainmap_flag && gainmap_item_data_size > 0) {
    bits.write_bits32(gainmap_item_codec_config_size, large_codec_config_flag ? 12 : 3);
  }

  bits.write_bits32(main_item_codec_config_size, large_codec_config_flag ? 12 : 3);
  bits.write_bits32(main_item_data_size - 1, large_item_data_flag ? 28 : 15);

  if (m_alpha_flag) {
    bits.write_bits32(alpha_item_data_size, large_item_data_flag ? 28 : 15);
  }

  if (m_alpha_flag && alpha_item_data_size > 0) {
    bits.write_bits32(alpha_item_codec_config_size, large_codec_config_flag ? 12 : 3);
  }

  if (m_exif_flag || m_xmp_flag) {
    bits.write_flag(m_exif_xmp_compressed_flag);
  }

  if (m_exif_flag) {
    bits.write_bits32(exif_data_size - 1, large_metadata_flag ? 20 : 10);
  }
  if (m_xmp_flag) {
    bits.write_bits32(xmp_data_size - 1, large_metadata_flag ? 20 : 10);
  }

  // --- Byte alignment ---
  bits.skip_to_byte_boundary();

  // --- Byte-aligned data blocks ---

  // Codec configs
  if (main_item_codec_config_size > 0) {
    bits.write_bytes(m_main_item_codec_config);
  }

  if (m_alpha_flag && alpha_item_data_size > 0) {
    if (alpha_item_codec_config_size > 0) {
      bits.write_bytes(m_alpha_item_codec_config);
    }
  }

  if (m_hdr_flag && m_gainmap_flag && gainmap_item_data_size > 0) {
    if (gainmap_item_codec_config_size > 0) {
      bits.write_bytes(m_gainmap_item_codec_config);
    }
  }

  // ICC and metadata
  if (m_icc_flag) {
    bits.write_bytes(m_icc_data);
  }

  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag) {
    bits.write_bytes(m_tmap_icc_data);
  }

  if (m_hdr_flag && m_gainmap_flag && gainmap_metadata_size > 0) {
    bits.write_bytes(m_gainmap_metadata);
  }

  // Image data (order: alpha, gainmap, main, exif, xmp)
  if (m_alpha_flag && alpha_item_data_size > 0) {
    bits.write_bytes(m_alpha_item_data);
  }

  if (m_hdr_flag && m_gainmap_flag && gainmap_item_data_size > 0) {
    bits.write_bytes(m_gainmap_item_data);
  }

  bits.write_bytes(m_main_item_data);

  if (m_exif_flag) {
    bits.write_bytes(m_exif_data_bytes);
  }

  if (m_xmp_flag) {
    bits.write_bytes(m_xmp_data_bytes);
  }

  // Flush to StreamWriter
  writer.write(bits.get_data());

  prepend_header(writer, box_start);
  return Error::Ok;
}


static uint32_t get_item_type_for_brand(const heif_brand2 brand)
{
  switch(brand) {
    case heif_brand2_avif:
      return fourcc("av01");
    case heif_brand2_heic:
      return fourcc("hvc1");
    default:
      return 0;
  }
}


// Parse a codec config blob (raw av1C/hvcC payload, no box header) by wrapping
// it in a synthetic box header so Box::read can dispatch and m_size is set
// properly (av1C/hvcC parse refuses to run with unspecified box size).
static Error parse_codec_config_box(const std::vector<uint8_t>& config_bytes,
                                    uint32_t type_4cc,
                                    std::shared_ptr<Box>* out_box)
{
  const size_t header_size = 8;
  const size_t total_size = header_size + config_bytes.size();
  if (total_size > 0x7FFFFFFFu) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_mini_box,
            "Codec config in MinimizedImageBox is too large"};
  }

  std::vector<uint8_t> framed(total_size);
  framed[0] = (uint8_t)((total_size >> 24) & 0xff);
  framed[1] = (uint8_t)((total_size >> 16) & 0xff);
  framed[2] = (uint8_t)((total_size >> 8) & 0xff);
  framed[3] = (uint8_t)(total_size & 0xff);
  framed[4] = (uint8_t)((type_4cc >> 24) & 0xff);
  framed[5] = (uint8_t)((type_4cc >> 16) & 0xff);
  framed[6] = (uint8_t)((type_4cc >> 8) & 0xff);
  framed[7] = (uint8_t)(type_4cc & 0xff);
  std::copy(config_bytes.begin(), config_bytes.end(), framed.begin() + header_size);

  auto istr = std::make_shared<StreamReader_memory>(framed.data(), framed.size(), false);
  BitstreamRange range(istr, framed.size(), nullptr);
  return Box::read(range, out_box, heif_get_global_security_limits());
}


Error Box_mini::create_expanded_boxes(class HeifFile* file)
{
  file->init_meta_box();

  auto hdlr_box = std::make_shared<Box_hdlr>();
  hdlr_box->set_handler_type(fourcc("pict"));
  file->set_hdlr_box(hdlr_box);

  file->set_primary_item_id(1);

  std::shared_ptr<Box_infe> primary_infe_box = std::make_shared<Box_infe>();
  primary_infe_box->set_version(2);
  primary_infe_box->set_item_ID(1);

  // TODO: check explicit codec flag
  auto ftyp_box = file->get_ftyp_box();
  if (!ftyp_box) {
    return {heif_error_Invalid_input,
            heif_suberror_No_ftyp_box,
            "MinimizedImageBox requires an ftyp box to identify the codec brand"};
  }
  uint32_t minor_version = ftyp_box->get_minor_version();
  heif_brand2 mini_brand = minor_version;
  uint32_t infe_type = get_item_type_for_brand(mini_brand);
  if (infe_type == 0) {
    // not found
    std::stringstream sstr;
    sstr << "Minimised file requires brand " << fourcc_to_string(mini_brand) << " but this is not yet supported.";
    return Error(heif_error_Unsupported_filetype,
                 heif_suberror_Unspecified,
                 sstr.str());
  }
  primary_infe_box->set_item_type_4cc(infe_type);
  file->add_infe_box(1, primary_infe_box);

  if (get_alpha_item_data_size() != 0) {
    std::shared_ptr<Box_infe> alpha_infe_box = std::make_shared<Box_infe>();
    alpha_infe_box->set_version(2);
    alpha_infe_box->set_flags(1);
    alpha_infe_box->set_item_ID(2);
    alpha_infe_box->set_item_type_4cc(infe_type);
    file->add_infe_box(2, alpha_infe_box);
  }

  if (get_exif_flag()) {
    std::shared_ptr<Box_infe> exif_infe_box = std::make_shared<Box_infe>();
    exif_infe_box->set_version(2);
    exif_infe_box->set_flags(1);
    exif_infe_box->set_item_ID(6);
    exif_infe_box->set_item_type_4cc(fourcc("Exif"));
    if (m_exif_xmp_compressed_flag) {
      exif_infe_box->set_content_encoding("deflate");
    }
    file->add_infe_box(6, exif_infe_box);
  }

  if (get_xmp_flag()) {
    std::shared_ptr<Box_infe> xmp_infe_box = std::make_shared<Box_infe>();
    xmp_infe_box->set_version(2);
    xmp_infe_box->set_flags(1);
    xmp_infe_box->set_item_ID(7);
    xmp_infe_box->set_item_type_4cc(fourcc("mime"));
    xmp_infe_box->set_content_type("application/rdf+xml");
    if (m_exif_xmp_compressed_flag) {
      xmp_infe_box->set_content_encoding("deflate");
    }
    file->add_infe_box(7, xmp_infe_box);
  }

  auto ipco_box = std::make_shared<Box_ipco>();
  file->set_ipco_box(ipco_box);

  if (get_main_item_codec_config().size() != 0) {
    uint32_t config_type;
    if (infe_type == fourcc("av01")) {
      config_type = fourcc("av1C");
    } else if (infe_type == fourcc("hvc1")) {
      config_type = fourcc("hvcC");
    } else {
      std::stringstream sstr;
      sstr << "Minimised file requires infe support for " << fourcc_to_string(infe_type) << " but this is not yet supported.";
      return Error(heif_error_Unsupported_filetype,
                   heif_suberror_Unspecified,
                   sstr.str());
    }
    std::shared_ptr<Box> main_item_codec_prop;
    if (auto err = parse_codec_config_box(get_main_item_codec_config(), config_type, &main_item_codec_prop)) {
      return err;
    }
    ipco_box->append_child_box(main_item_codec_prop); // entry 1
  } else {
    ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 1
  }

  std::shared_ptr<Box_ispe> ispe = std::make_shared<Box_ispe>();
  ispe->set_size(get_width(), get_height());
  ipco_box->append_child_box(ispe); // entry 2

  std::shared_ptr<Box_pixi> pixi = std::make_shared<Box_pixi>();
  pixi->set_version(0);
  // pixi->set_version(1); // TODO: when we support version 1
  // TODO: there is more when we do version 1, and anything other than RGB
  pixi->add_channel_bits(get_bit_depth());
  pixi->add_channel_bits(get_bit_depth());
  pixi->add_channel_bits(get_bit_depth());
  ipco_box->append_child_box(pixi); // entry 3

  std::shared_ptr<Box_colr> colr = std::make_shared<Box_colr>();
  nclx_profile colorProfile;
  colorProfile.set_colour_primaries(get_colour_primaries());
  colorProfile.set_transfer_characteristics(get_transfer_characteristics());
  colorProfile.set_matrix_coefficients(get_matrix_coefficients());
  colorProfile.set_full_range_flag(get_full_range_flag());
  std::shared_ptr<color_profile_nclx> nclx = std::make_shared<color_profile_nclx>(colorProfile);
  colr->set_color_profile(nclx);
  ipco_box->append_child_box(colr); // entry 4

  if (get_icc_flag()) {
    std::shared_ptr<Box_colr> colr_icc = std::make_shared<Box_colr>();
    std::shared_ptr<color_profile_raw> icc = std::make_shared<color_profile_raw>(fourcc("prof"), get_icc_data());
    colr_icc->set_color_profile(icc);
    ipco_box->append_child_box(colr_icc); // entry 5
  } else {
    ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 5
  }

  if (get_alpha_item_codec_config().size() != 0) {
    uint32_t config_type;
    if (infe_type == fourcc("av01")) {
      config_type = fourcc("av1C");
    } else if (infe_type == fourcc("hvc1")) {
      config_type = fourcc("hvcC");
    } else {
      std::stringstream sstr;
      sstr << "Minimised file requires infe support for " << fourcc_to_string(infe_type) << " but this is not yet supported.";
      return Error(heif_error_Unsupported_filetype,
                   heif_suberror_Unspecified,
                   sstr.str());
    }
    std::shared_ptr<Box> alpha_item_codec_prop;
    if (auto err = parse_codec_config_box(get_alpha_item_codec_config(), config_type, &alpha_item_codec_prop)) {
      return err;
    }
    ipco_box->append_child_box(alpha_item_codec_prop); // entry 6
  } else {
    ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 6
  }

  if (get_alpha_item_data_size() != 0) {
    std::shared_ptr<Box_auxC> aux_type = std::make_shared<Box_auxC>();
    aux_type->set_aux_type("urn:mpeg:mpegB:cicp:systems:auxiliary:alpha");
    ipco_box->append_child_box(aux_type); // entry 7
  } else {
    ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 7
  }

  // TODO: replace this placeholder with pixi box version 1 once that is supported
  ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 8

  // HDR metadata: expand the parsed mini-bitstream fields into separate ipco
  // property boxes so the standard property-walk machinery (and the public
  // heif_image*_has/get_content_light_level / mastering_display_colour_volume
  // / ambient_viewing_environment / nominal_diffuse_white APIs) sees them.
  uint16_t hdr_clli_prop_index = 0;
  uint16_t hdr_mdcv_prop_index = 0;
  uint16_t hdr_cclv_prop_index = 0;
  uint16_t hdr_amve_prop_index = 0;
  uint16_t hdr_ndwt_prop_index = 0;
  // append_child_box() returns the 0-based child index; ipma uses 1-based
  // property indices.
  if (m_clli) {
    hdr_clli_prop_index = static_cast<uint16_t>(ipco_box->append_child_box(m_clli) + 1);
  }
  if (m_mdcv) {
    hdr_mdcv_prop_index = static_cast<uint16_t>(ipco_box->append_child_box(m_mdcv) + 1);
  }
  if (m_cclv) {
    hdr_cclv_prop_index = static_cast<uint16_t>(ipco_box->append_child_box(m_cclv) + 1);
  }
  if (m_amve) {
    hdr_amve_prop_index = static_cast<uint16_t>(ipco_box->append_child_box(m_amve) + 1);
  }
  if (m_ndwt) {
    hdr_ndwt_prop_index = static_cast<uint16_t>(ipco_box->append_child_box(m_ndwt) + 1);
  }

  auto ipma_box = std::make_shared<Box_ipma>();
  file->set_ipma_box(ipma_box);
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(1)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, uint16_t(2)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, uint16_t(3)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(4)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(5)});

  if (get_alpha_item_data_size() != 0) {
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{true, uint16_t(6)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{false, uint16_t(2)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{true, uint16_t(7)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{false, uint16_t(8)});
  }

  // Associate HDR-metadata properties with the primary item (non-essential —
  // matches what ImageItem::set_clli/set_mdcv/set_amve/set_ndwt emit on
  // the write path).
  if (hdr_clli_prop_index) {
    ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, hdr_clli_prop_index});
  }
  if (hdr_mdcv_prop_index) {
    ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, hdr_mdcv_prop_index});
  }
  if (hdr_cclv_prop_index) {
    ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, hdr_cclv_prop_index});
  }
  if (hdr_amve_prop_index) {
    ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, hdr_amve_prop_index});
  }
  if (hdr_ndwt_prop_index) {
    ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, hdr_ndwt_prop_index});
  }
  // TODO: gainmap (tmap) HDR metadata expansion is still missing.

  // Append irot/imir (only as needed) and associate them with the items.
  // mini's orientation field uses standard EXIF orientation values 1..8,
  // matching the heif_orientation enum.
  auto orientation = static_cast<heif_orientation>(get_orientation());
  file->add_orientation_properties(1, orientation);
  if (get_alpha_item_data_size() != 0) {
    file->add_orientation_properties(2, orientation);
  }

  auto iloc_box = std::make_shared<Box_iloc>();
  file->set_iloc_box(iloc_box);
  Box_iloc::Item main_item;
  main_item.item_ID = 1;
  main_item.construction_method = 0;
  main_item.base_offset = 0;
  main_item.data_reference_index = 0;
  Box_iloc::Extent main_item_extent;
  main_item_extent.offset = get_main_item_data_offset();
  main_item_extent.length = get_main_item_data_size();
  main_item.extents.push_back(main_item_extent);
  iloc_box->append_item(main_item);

  if (get_alpha_item_data_size() != 0) {
    Box_iloc::Item alpha_item;
    alpha_item.item_ID = 2;
    alpha_item.base_offset = 0;
    alpha_item.data_reference_index = 0;
    Box_iloc::Extent alpha_item_extent;
    alpha_item_extent.offset = get_alpha_item_data_offset();
    alpha_item_extent.length = get_alpha_item_data_size();
    alpha_item.extents.push_back(alpha_item_extent);
    iloc_box->append_item(alpha_item);
  }
  if (get_exif_flag()) {
    Box_iloc::Item exif_item;
    exif_item.item_ID = 6;
    exif_item.base_offset = 0;
    exif_item.data_reference_index = 0;
    Box_iloc::Extent exif_item_extent;
    exif_item_extent.offset = get_exif_item_data_offset();
    exif_item_extent.length = get_exif_item_data_size();
    exif_item.extents.push_back(exif_item_extent);
    iloc_box->append_item(exif_item);
  }
  if (get_xmp_flag()) {
    Box_iloc::Item xmp_item;
    xmp_item.item_ID = 7;
    xmp_item.base_offset = 0;
    xmp_item.data_reference_index = 0;
    Box_iloc::Extent xmp_item_extent;
    xmp_item_extent.offset = get_xmp_item_data_offset();
    xmp_item_extent.length = get_xmp_item_data_size();
    xmp_item.extents.push_back(xmp_item_extent);
    iloc_box->append_item(xmp_item);
  }

  auto iref_box = std::make_shared<Box_iref>();
  file->set_iref_box(iref_box);
  std::vector<uint32_t> to_items = {1};
  if (get_alpha_item_data_size() != 0) {
    iref_box->add_references(2, fourcc("auxl"), to_items);
  }
  // TODO: if alpha prem
  // TODO: if gainmap flag && item 4
  // TODO: if gainmap flag && !item 4
  if (get_exif_flag()) {
    iref_box->add_references(6, fourcc("cdsc"), to_items);
  }
  if (get_xmp_flag()) {
    iref_box->add_references(7, fourcc("cdsc"), to_items);
  }

  return Error::Ok;
}


// --- Map a single irot/imir box to its equivalent heif_orientation ---
//
// Used together with heif_orientation_concat() to compose the cumulative
// orientation of a sequence of transform boxes. Box order in ipma is not
// strictly fixed across files, so accumulating via concat lets us recover
// the right orientation regardless of whether irot or imir appears first.

static heif_orientation transform_box_to_orientation(const std::shared_ptr<Box>& box)
{
  if (auto irot = std::dynamic_pointer_cast<Box_irot>(box)) {
    // irot stores the rotation in counter-clockwise degrees.
    switch (irot->get_rotation_ccw()) {
      case 0:   return heif_orientation_normal;
      case 90:  return heif_orientation_rotate_270_cw; // 90 ccw == 270 cw
      case 180: return heif_orientation_rotate_180;
      case 270: return heif_orientation_rotate_90_cw;  // 270 ccw == 90 cw
      default:  return heif_orientation_normal;
    }
  }
  if (auto imir = std::dynamic_pointer_cast<Box_imir>(box)) {
    switch (imir->get_mirror_direction()) {
      case heif_transform_mirror_direction_horizontal:
        return heif_orientation_flip_horizontally;
      case heif_transform_mirror_direction_vertical:
        return heif_orientation_flip_vertically;
      case heif_transform_mirror_direction_invalid:
        return heif_orientation_normal;
    }
  }
  return heif_orientation_normal;
}


heif_orientation Box_mini::compute_orientation_from_properties(
    const std::vector<std::shared_ptr<Box>>& properties)
{
  heif_orientation orientation = heif_orientation_normal;
  for (auto& prop : properties) {
    if (std::dynamic_pointer_cast<Box_irot>(prop) ||
        std::dynamic_pointer_cast<Box_imir>(prop)) {
      orientation = heif_orientation_concat(orientation, transform_box_to_orientation(prop));
    }
  }
  return orientation;
}


// --- Extract codec config as raw bytes (without box header) ---

static std::vector<uint8_t> extract_codec_config_bytes(const std::shared_ptr<Box>& codec_config_box)
{
  if (!codec_config_box) {
    return {};
  }

  StreamWriter temp_writer;
  codec_config_box->write(temp_writer);
  auto full_data = temp_writer.get_data();

  // Strip the 8-byte box header (size + fourcc)
  if (full_data.size() <= 8) {
    return {};
  }
  return std::vector<uint8_t>(full_data.begin() + 8, full_data.end());
}


// --- Eligibility check ---

bool Box_mini::can_convert_to_mini(const HeifFile* file, std::string& out_reason)
{
  // Must have a primary item
  heif_item_id primary_id = file->get_primary_image_ID();
  if (primary_id == 0) {
    out_reason = "no primary item";
    return false;
  }

  // Check primary item type
  uint32_t item_type = file->get_item_type_4cc(primary_id);
  if (item_type != fourcc("av01") && item_type != fourcc("hvc1")) {
    out_reason = "primary item type not supported for mini (need av01 or hvc1)";
    return false;
  }

  // Check dimensions
  std::vector<std::shared_ptr<Box>> properties;
  file->get_properties(primary_id, properties);

  for (auto& prop : properties) {
    if (auto ispe = std::dynamic_pointer_cast<Box_ispe>(prop)) {
      if (ispe->get_width() > 32768 || ispe->get_height() > 32768) {
        out_reason = "dimensions exceed mini box limits";
        return false;
      }
    }
  }

  // Check that we don't have unsupported derived image types
  auto item_ids = file->get_item_IDs();
  heif_item_id alpha_id = 0;
  heif_item_id exif_id = 0;
  heif_item_id xmp_id = 0;

  for (auto id : item_ids) {
    if (id == primary_id) continue;

    uint32_t type = file->get_item_type_4cc(id);
    if (type == fourcc("grid") || type == fourcc("iovl") || type == fourcc("iden")) {
      out_reason = "derived image items (grid/overlay/identity) not supported in mini";
      return false;
    }

    // Check for alpha auxiliary
    auto iref = file->get_iref_box();
    if (iref) {
      auto refs = iref->get_references(id, fourcc("auxl"));
      if (!refs.empty() && refs[0] == primary_id) {
        if (alpha_id != 0) {
          out_reason = "multiple alpha items not supported in mini";
          return false;
        }
        alpha_id = id;
        continue;
      }
      auto cdsc_refs = iref->get_references(id, fourcc("cdsc"));
      if (!cdsc_refs.empty() && cdsc_refs[0] == primary_id) {
        if (type == fourcc("Exif")) {
          if (exif_id != 0) {
            out_reason = "multiple EXIF items not supported in mini";
            return false;
          }
          exif_id = id;
          continue;
        }
        if (type == fourcc("mime")) {
          auto infe = file->get_infe_box(id);
          if (infe && infe->get_content_type() == "application/rdf+xml") {
            if (xmp_id != 0) {
              out_reason = "multiple XMP items not supported in mini";
              return false;
            }
            xmp_id = id;
            continue;
          }
          else {
            out_reason = "unsupported mime item type for mini: " + (infe ? infe->get_content_type() : "unknown");
            return false;
          }
        }
      }
    }

    // If it's a hidden item or an item type we know about, skip it.
    // Otherwise, it's unsupported for mini.
    auto infe = file->get_infe_box(id);
    if (infe && !infe->is_hidden_item() && type != item_type) {
      out_reason = "unsupported additional item type for mini: " + fourcc_to_string(type);
      return false;
    }
  }

  // The mini box has a single compressed flag for both EXIF and XMP.
  // If both are present, they must use the same compression method.
  if (exif_id != 0 && xmp_id != 0) {
    auto exif_infe = file->get_infe_box(exif_id);
    auto xmp_infe = file->get_infe_box(xmp_id);
    bool exif_compressed = (exif_infe && exif_infe->get_content_encoding() == "deflate");
    bool xmp_compressed = (xmp_infe && xmp_infe->get_content_encoding() == "deflate");
    if (exif_compressed != xmp_compressed) {
      out_reason = "EXIF and XMP have different compression methods";
      return false;
    }
  }

  return true;
}


// --- Meta-to-Mini conversion ---

std::shared_ptr<Box_mini> Box_mini::create_from_heif_file(HeifFile* file)
{
  std::string reason;
  if (!can_convert_to_mini(file, reason)) {
    return nullptr;
  }

  auto mini = std::make_shared<Box_mini>();
  mini->set_version(0);

  heif_item_id primary_id = file->get_primary_image_ID();
  uint32_t item_type = file->get_item_type_4cc(primary_id);

  bool is_avif = (item_type == fourcc("av01"));

  // For av01/hvc1 the codec is identified via the ftyp minor version brand,
  // so we don't need explicit codec type fields in the mini bitstream.
  mini->set_explicit_codec_types_flag(false);

  // Get properties for primary item
  std::vector<std::shared_ptr<Box>> properties;
  file->get_properties(primary_id, properties);

  // Extract properties
  std::shared_ptr<Box_ispe> ispe;
  std::shared_ptr<Box_pixi> pixi;
  std::shared_ptr<Box_colr> colr_nclx;
  std::shared_ptr<Box_colr> colr_icc;
  std::shared_ptr<Box> codec_config;
  std::shared_ptr<Box_clli> clli_box;
  std::shared_ptr<Box_mdcv> mdcv_box;
  std::shared_ptr<Box_cclv> cclv_box;
  std::shared_ptr<Box_amve> amve_box;
  std::shared_ptr<Box_ndwt> ndwt_box;

  for (auto& prop : properties) {
    if (auto p = std::dynamic_pointer_cast<Box_ispe>(prop)) {
      ispe = p;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_pixi>(prop)) {
      pixi = p;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_colr>(prop)) {
      if (p->get_color_profile_type() == fourcc("nclx")) {
        colr_nclx = p;
      }
      else {
        colr_icc = p;
      }
    }
    else if (std::dynamic_pointer_cast<Box_av1C>(prop) || std::dynamic_pointer_cast<Box_hvcC>(prop)) {
      codec_config = prop;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_clli>(prop)) {
      clli_box = p;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_mdcv>(prop)) {
      mdcv_box = p;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_cclv>(prop)) {
      cclv_box = p;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_amve>(prop)) {
      amve_box = p;
    }
    else if (auto p = std::dynamic_pointer_cast<Box_ndwt>(prop)) {
      ndwt_box = p;
    }
  }

  heif_orientation orientation = compute_orientation_from_properties(properties);

  // Dimensions
  if (ispe) {
    mini->set_width(ispe->get_width());
    mini->set_height(ispe->get_height());
  }

  // Bit depth
  if (pixi && pixi->get_num_channels() > 0) {
    mini->set_bit_depth(static_cast<uint8_t>(pixi->get_bits_per_channel(0)));
  }
  mini->set_float_flag(false); // TODO: detect float from codec config

  // CICP / color
  bool has_icc = (colr_icc != nullptr);
  mini->set_icc_flag(has_icc);

  if (has_icc) {
    auto raw_profile = std::dynamic_pointer_cast<const color_profile_raw>(colr_icc->get_color_profile());
    if (raw_profile) {
      mini->set_icc_data(raw_profile->get_data());
    }
  }

  if (colr_nclx) {
    auto nclx = std::dynamic_pointer_cast<const color_profile_nclx>(colr_nclx->get_color_profile());
    if (nclx) {
      auto profile = nclx->get_nclx_color_profile();
      mini->set_colour_primaries(profile.m_colour_primaries);
      mini->set_transfer_characteristics(profile.m_transfer_characteristics);
      mini->set_matrix_coefficients(profile.m_matrix_coefficients);
      mini->set_full_range_flag(profile.m_full_range_flag);
    }
  }

  // Determine chroma subsampling from codec config
  // mini chroma_subsampling values: 0=monochrome, 1=4:2:0, 2=4:2:2, 3=4:4:4
  uint8_t chroma_sub = 0;
  if (is_avif) {
    auto av1c = std::dynamic_pointer_cast<Box_av1C>(codec_config);
    if (av1c) {
      auto& config = av1c->get_configuration();
      if (config.chroma_subsampling_x == 1 && config.chroma_subsampling_y == 1) {
        chroma_sub = 1; // 4:2:0
      }
      else if (config.chroma_subsampling_x == 1 && config.chroma_subsampling_y == 0) {
        chroma_sub = 2; // 4:2:2
      }
      else if (config.chroma_subsampling_x == 0 && config.chroma_subsampling_y == 0) {
        if (config.monochrome) {
          chroma_sub = 0;
        }
        else {
          chroma_sub = 3; // 4:4:4
        }
      }
    }
  }
  else if (item_type == fourcc("hvc1")) {
    auto hvcc = std::dynamic_pointer_cast<Box_hvcC>(codec_config);
    if (hvcc) {
      // HEVC chroma_format uses the same values as mini chroma_subsampling
      chroma_sub = hvcc->get_configuration().chroma_format;
    }
  }
  mini->set_chroma_subsampling(chroma_sub);

  // Determine if explicit CICP is needed (vs implicit defaults)
  bool need_explicit_cicp = true;
  uint16_t default_primaries = has_icc ? 2 : 1;
  uint16_t default_transfer = has_icc ? 2 : 13;
  uint16_t default_matrix = (chroma_sub == 0) ? 2 : 6;

  if (colr_nclx) {
    auto nclx = std::dynamic_pointer_cast<const color_profile_nclx>(colr_nclx->get_color_profile());
    if (nclx) {
      auto profile = nclx->get_nclx_color_profile();
      if (profile.m_colour_primaries == default_primaries &&
          profile.m_transfer_characteristics == default_transfer &&
          profile.m_matrix_coefficients == default_matrix) {
        need_explicit_cicp = false;
      }
    }
  }
  else {
    // No NCLX profile, use defaults
    mini->set_colour_primaries(default_primaries);
    mini->set_transfer_characteristics(default_transfer);
    mini->set_matrix_coefficients(default_matrix);
    need_explicit_cicp = false;
  }
  mini->set_explicit_cicp_flag(need_explicit_cicp);

  // Orientation (accumulated via heif_orientation_concat during the property walk above)
  mini->set_orientation(static_cast<uint8_t>(orientation));

  // Codec config
  auto config_bytes = extract_codec_config_bytes(codec_config);
  mini->set_main_item_codec_config(config_bytes);

  // Find alpha, exif, xmp items
  heif_item_id alpha_id = 0;
  heif_item_id exif_id = 0;
  heif_item_id xmp_id = 0;

  auto item_ids = file->get_item_IDs();
  auto iref = file->get_iref_box();

  for (auto id : item_ids) {
    if (id == primary_id) continue;
    if (!iref) continue;

    auto auxl_refs = iref->get_references(id, fourcc("auxl"));
    if (!auxl_refs.empty() && auxl_refs[0] == primary_id) {
      alpha_id = id;
      continue;
    }

    auto cdsc_refs = iref->get_references(id, fourcc("cdsc"));
    if (!cdsc_refs.empty() && cdsc_refs[0] == primary_id) {
      uint32_t type = file->get_item_type_4cc(id);
      if (type == fourcc("Exif")) {
        exif_id = id;
      }
      else if (type == fourcc("mime")) {
        auto infe = file->get_infe_box(id);
        if (infe && infe->get_content_type() == "application/rdf+xml") {
          xmp_id = id;
        }
      }
    }
  }

  // Alpha
  mini->set_alpha_flag(alpha_id != 0);
  if (alpha_id != 0) {
    mini->set_alpha_is_premultiplied(false); // TODO: detect from auxC

    // Alpha codec config
    std::vector<std::shared_ptr<Box>> alpha_props;
    file->get_properties(alpha_id, alpha_props);
    for (auto& prop : alpha_props) {
      if (std::dynamic_pointer_cast<Box_av1C>(prop) || std::dynamic_pointer_cast<Box_hvcC>(prop)) {
        auto alpha_config_bytes = extract_codec_config_bytes(prop);
        mini->set_alpha_item_codec_config(alpha_config_bytes);
        break;
      }
    }

    // Alpha item data from iloc
    auto iloc = file->get_iloc_box();
    if (iloc) {
      for (auto& item : iloc->get_items()) {
        if (item.item_ID == alpha_id) {
          std::vector<uint8_t> data;
          for (auto& extent : item.extents) {
            data.insert(data.end(), extent.data.begin(), extent.data.end());
          }
          mini->set_alpha_item_data(std::move(data));
          break;
        }
      }
    }
  }

  // EXIF and XMP share a single compressed flag in the mini box.
  // Determine it from whichever item is present (can_convert_to_mini already
  // verified they agree when both exist).
  mini->set_exif_flag(exif_id != 0);
  mini->set_xmp_flag(xmp_id != 0);

  bool metadata_compressed = false;
  if (exif_id != 0) {
    auto infe = file->get_infe_box(exif_id);
    if (infe && infe->get_content_encoding() == "deflate") {
      metadata_compressed = true;
    }
  }
  if (xmp_id != 0) {
    auto infe = file->get_infe_box(xmp_id);
    if (infe && infe->get_content_encoding() == "deflate") {
      metadata_compressed = true;
    }
  }
  mini->set_exif_xmp_compressed_flag(metadata_compressed);

  if (exif_id != 0) {
    auto iloc = file->get_iloc_box();
    if (iloc) {
      for (auto& item : iloc->get_items()) {
        if (item.item_ID == exif_id) {
          std::vector<uint8_t> data;
          for (auto& extent : item.extents) {
            data.insert(data.end(), extent.data.begin(), extent.data.end());
          }
          mini->set_exif_data(std::move(data));
          break;
        }
      }
    }
  }

  if (xmp_id != 0) {
    auto iloc = file->get_iloc_box();
    if (iloc) {
      for (auto& item : iloc->get_items()) {
        if (item.item_ID == xmp_id) {
          std::vector<uint8_t> data;
          for (auto& extent : item.extents) {
            data.insert(data.end(), extent.data.begin(), extent.data.end());
          }
          mini->set_xmp_data(std::move(data));
          break;
        }
      }
    }
  }

  // Main item data from iloc
  {
    auto iloc = file->get_iloc_box();
    if (iloc) {
      for (auto& item : iloc->get_items()) {
        if (item.item_ID == primary_id) {
          std::vector<uint8_t> data;
          for (auto& extent : item.extents) {
            data.insert(data.end(), extent.data.begin(), extent.data.end());
          }
          mini->set_main_item_data(std::move(data));
          break;
        }
      }
    }
  }

  // HDR metadata on the primary item (clli/mdcv/cclv/amve/ndwt). The mini
  // bitstream gates these via hdr_flag — set it whenever at least one of
  // those property boxes is present.
  // Gainmap (tmap) conversion is still TODO; m_gainmap_flag stays false here.
  bool has_hdr_metadata = clli_box || mdcv_box || cclv_box || amve_box || ndwt_box;
  mini->set_hdr_flag(has_hdr_metadata);
  if (clli_box) mini->set_clli(clli_box);
  if (mdcv_box) mini->set_mdcv(mdcv_box);
  if (cclv_box) mini->set_cclv(cclv_box);
  if (amve_box) mini->set_amve(amve_box);
  if (ndwt_box) mini->set_ndwt(ndwt_box);

  return mini;
}
