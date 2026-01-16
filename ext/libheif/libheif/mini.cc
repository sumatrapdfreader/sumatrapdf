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

#include "mini.h"
#include "file.h"
#include "codecs/avif_boxes.h"
#include "codecs/hevc_boxes.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <utility>


Error Box_mini::parse(BitstreamRange &range, const heif_security_limits *limits)
{
  uint64_t start_offset = range.get_istream()->get_position();
  std::size_t length = range.get_remaining_bytes();
  std::vector<uint8_t> mini_data(length);
  range.read(mini_data.data(), mini_data.size());
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
  bool small_dimensions_flag = bits.get_flag();
  if (small_dimensions_flag)
  {
    m_width = 1 + bits.get_bits32(7);
    m_height = 1 + bits.get_bits32(7);
  }
  else
  {
    m_width = 1 + bits.get_bits32(15);
    m_height = 1 + bits.get_bits32(15);
  }
  if ((m_chroma_subsampling == 1) || (m_chroma_subsampling == 2))
  {
    m_chroma_is_horizontally_centred = bits.get_flag();
  }
  if (m_chroma_subsampling == 1)
  {
    m_chroma_is_vertically_centred = bits.get_flag();
  }

  bool high_bit_depth_flag = false;
  if (m_float_flag)
  {
    uint8_t bit_depth_log2_minus4 = bits.get_bits8(2);
    m_bit_depth = (uint8_t)powl(2, (bit_depth_log2_minus4 + 4));
  }
  else
  {
    high_bit_depth_flag = bits.get_flag();
    if (high_bit_depth_flag)
    {
      m_bit_depth = 9 + bits.get_bits8(3);
    }
  }

  if (m_alpha_flag)
  {
    m_alpha_is_premultiplied = bits.get_flag();
  }

  if (m_explicit_cicp_flag)
  {
    m_colour_primaries = bits.get_bits8(8);
    m_transfer_characteristics = bits.get_bits8(8);
    if (m_chroma_subsampling != 0)
    {
      m_matrix_coefficients = bits.get_bits8(8);
    }
    else
    {
      m_matrix_coefficients = 2;
    }
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
      uint32_t gainmap_width_minus1 = bits.get_bits32(small_dimensions_flag ? 7 : 15);
      m_gainmap_width = gainmap_width_minus1 + 1;
      uint32_t gainmap_height_minus1 = bits.get_bits32(small_dimensions_flag ? 7 : 15);
      m_gainmap_height = gainmap_height_minus1 + 1;
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
        uint8_t bit_depth_log2_minus4 = bits.get_bits8(2);
        m_gainmap_bit_depth = (uint8_t)powl(2, (bit_depth_log2_minus4 + 4));
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
    m_ndwt_flag = bits.get_flag();

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

    if (m_ndwt_flag)
    {
      // TODO: NominalDiffuseWhite isn't published yet
      bits.skip_bits(32);
    }

    if (m_gainmap_flag)
    {
      bool tmap_clli_flag = bits.get_flag();
      bool tmap_mdcv_flag = bits.get_flag();
      bool tmap_cclv_flag = bits.get_flag();
      bool tmap_amve_flag = bits.get_flag();
      m_tmap_reve_flag = bits.get_flag();
      m_tmap_ndwt_flag = bits.get_flag();

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

      if (m_tmap_ndwt_flag)
      {
        // TODO: NominalDiffuseWhite isn't published yet
        bits.skip_bits(32);
      }
    }
  }

  // Chunk sizes
  bool few_metadata_bytes_flag = false;
  if (m_icc_flag || m_exif_flag || m_xmp_flag || (m_hdr_flag && m_gainmap_flag))
  {
    few_metadata_bytes_flag = bits.get_flag();
  }
  bool few_codec_config_bytes_flag = bits.get_flag();
  bool few_item_data_bytes_flag = bits.get_flag();

  uint32_t icc_data_size = 0;
  if (m_icc_flag)
  {
    icc_data_size = bits.get_bits32(few_metadata_bytes_flag ? 10 : 20) + 1;
  }
  uint32_t tmap_icc_data_size = 0;
  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag)
  {
    tmap_icc_data_size = bits.get_bits32(few_metadata_bytes_flag ? 10 : 20) + 1;
  }
  uint32_t gainmap_metadata_size = 0;
  if (m_hdr_flag && m_gainmap_flag)
  {
    gainmap_metadata_size = bits.get_bits32(few_metadata_bytes_flag ? 10 : 20);
  }
  if (m_hdr_flag && m_gainmap_flag)
  {
    m_gainmap_item_data_size = bits.get_bits32(few_item_data_bytes_flag ? 15 : 28);
  }
  uint32_t gainmap_item_codec_config_size = 0;
  if (m_hdr_flag && m_gainmap_flag && (m_gainmap_item_data_size > 0))
  {
    gainmap_item_codec_config_size = bits.get_bits32(few_codec_config_bytes_flag ? 3 : 12);
  }

  uint32_t main_item_codec_config_size = bits.get_bits32(few_codec_config_bytes_flag ? 3 : 12);
  m_main_item_data_size = bits.get_bits32(few_item_data_bytes_flag ? 15 : 28) + 1;

  if (m_alpha_flag)
  {
    m_alpha_item_data_size = bits.get_bits32(few_item_data_bytes_flag ? 15 : 28);
  }
  uint32_t alpha_item_codec_config_size = 0;
  if (m_alpha_flag && (m_alpha_item_data_size > 0))
  {
    alpha_item_codec_config_size = bits.get_bits32(few_codec_config_bytes_flag ? 3 : 12);
  }

  if (m_exif_flag)
  {
    m_exif_item_data_size = bits.get_bits32(few_metadata_bytes_flag ? 10 : 20) + 1;
  }
  if (m_xmp_flag)
  {
    m_xmp_item_data_size = bits.get_bits32(few_metadata_bytes_flag ? 10 : 20) + 1;
  }

  bits.skip_to_byte_boundary();

  // Chunks
  if (m_alpha_flag && (m_alpha_item_data_size > 0) && (alpha_item_codec_config_size > 0))
  {
    m_alpha_item_codec_config = bits.read_bytes(alpha_item_codec_config_size);
  }
  if (m_hdr_flag && m_gainmap_flag && (gainmap_item_codec_config_size > 0))
  {
    m_gainmap_item_codec_config = bits.read_bytes(gainmap_item_codec_config_size);
  }
  if (main_item_codec_config_size > 0)
  {
    m_main_item_codec_config = bits.read_bytes(main_item_codec_config_size);
  }

  if (m_icc_flag)
  {
    m_icc_data = bits.read_bytes(icc_data_size);
  }
  if (m_hdr_flag && m_gainmap_flag && m_tmap_icc_flag)
  {
    m_tmap_icc_data = bits.read_bytes(tmap_icc_data_size);
  }
  if (m_hdr_flag && m_gainmap_flag && (gainmap_metadata_size > 0))
  {
    m_gainmap_metadata = bits.read_bytes(gainmap_metadata_size);
  }

  if (m_alpha_flag && (m_alpha_item_data_size > 0))
  {
    m_alpha_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_alpha_item_data_size);
  }
  if (m_alpha_flag && m_gainmap_flag && (m_gainmap_item_data_size > 0))
  {
    m_gainmap_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bits(m_gainmap_item_data_size);
  }

  m_main_item_data_offset = bits.get_current_byte_index() + start_offset;
  bits.skip_bytes(m_main_item_data_size);

  if (m_exif_flag)
  {
    m_exif_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_exif_item_data_size);
  }
  if (m_xmp_flag)
  {
    m_xmp_item_data_offset = bits.get_current_byte_index() + start_offset;
    bits.skip_bytes(m_xmp_item_data_size);
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
    sstr << indent << "chroma_is_horizontally_centered: " << m_chroma_is_horizontally_centred << "\n";
  }
  if (m_chroma_subsampling == 1)
  {
    sstr << indent << "chroma_is_vertically_centered: " << m_chroma_is_vertically_centred << "\n";
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
    sstr << indent << "ndwt_flag: " << m_ndwt_flag << "\n";

    if (m_reve_flag)
    {
      // TODO - this isn't published yet
    }
    if (m_ndwt_flag)
    {
      // TODO - this isn't published yet
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
      sstr << indent << "tmap_ndwt_flag: " << m_tmap_ndwt_flag << "\n";

      if (m_tmap_reve_flag)
      {
        // TODO - this isn't published yet
      }
      if (m_tmap_ndwt_flag)
      {
        // TODO - this isn't published yet
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
    sstr << "exif_data offset: " << m_exif_item_data_offset << ", size: " << m_exif_item_data_size << "\n";
  }
  if (m_xmp_flag)
  {
    sstr << "xmp_data offset: " << m_xmp_item_data_offset << ", size: " << m_xmp_item_data_size << "\n";
  }
  return sstr.str();
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
  uint32_t minor_version = file->get_ftyp_box()->get_minor_version();
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
    file->add_infe_box(6, exif_infe_box);
  }

  if (get_xmp_flag()) {
    std::shared_ptr<Box_infe> xmp_infe_box = std::make_shared<Box_infe>();
    xmp_infe_box->set_version(2);
    xmp_infe_box->set_flags(1);
    xmp_infe_box->set_item_ID(7);
    xmp_infe_box->set_item_type_4cc(fourcc("mime"));
    xmp_infe_box->set_content_type("application/rdf+xml");
    file->add_infe_box(7, xmp_infe_box);
  }

  auto ipco_box = std::make_shared<Box_ipco>();
  file->set_ipco_box(ipco_box);

  if (get_main_item_codec_config().size() != 0) {
    std::shared_ptr<StreamReader> istr = std::make_shared<StreamReader_memory>(
        get_main_item_codec_config().data(),
        get_main_item_codec_config().size(),
        false
    );
    BitstreamRange codec_range(istr, get_main_item_codec_config().size(), nullptr);

    std::shared_ptr<Box> main_item_codec_prop;
    if (infe_type == fourcc("av01")) {
      std::shared_ptr<Box_av1C> codec_prop = std::make_shared<Box_av1C>();
      codec_prop->parse(codec_range, heif_get_global_security_limits());
      main_item_codec_prop = std::move(codec_prop);
    } else if (infe_type == fourcc("hvc1")) {
      std::shared_ptr<Box_hvcC> codec_prop = std::make_shared<Box_hvcC>();
      codec_prop->parse(codec_range, heif_get_global_security_limits());
      main_item_codec_prop = std::move(codec_prop);
    } else {
      // not found
      std::stringstream sstr;
      sstr << "Minimised file requires infe support for " << fourcc_to_string(infe_type) << " but this is not yet supported.";
      return Error(heif_error_Unsupported_filetype,
                   heif_suberror_Unspecified,
                   sstr.str());
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
    std::shared_ptr<StreamReader> istr = std::make_shared<StreamReader_memory>(
        get_alpha_item_codec_config().data(),
        get_alpha_item_codec_config().size(),
        false
    );
    BitstreamRange alpha_codec_range(istr, get_alpha_item_codec_config().size(), nullptr);
    std::shared_ptr<Box> alpha_item_codec_prop;
    if (infe_type == fourcc("av01")) {
      std::shared_ptr<Box_av1C> codec_prop = std::make_shared<Box_av1C>();
      codec_prop->parse(alpha_codec_range, heif_get_global_security_limits());
      alpha_item_codec_prop = std::move(codec_prop);
    } else if (infe_type == fourcc("hvc1")) {
      std::shared_ptr<Box_hvcC> codec_prop = std::make_shared<Box_hvcC>();
      codec_prop->parse(alpha_codec_range, heif_get_global_security_limits());
      alpha_item_codec_prop = std::move(codec_prop);
    } else {
      // not found
      std::stringstream sstr;
      sstr << "Minimised file requires infe support for " << fourcc_to_string(infe_type) << " but this is not yet supported.";
      return Error(heif_error_Unsupported_filetype,
                   heif_suberror_Unspecified,
                   sstr.str());
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

  if (get_orientation() == 2) {
    std::shared_ptr<Box_irot> irot = std::make_shared<Box_irot>();
    irot->set_rotation_ccw(2 * 90);
    ipco_box->append_child_box(irot); // entry 9
  } else if ((get_orientation() == 4) || (get_orientation() == 6) || (get_orientation() == 7)) {
    std::shared_ptr<Box_irot> irot = std::make_shared<Box_irot>();
    irot->set_rotation_ccw(1 * 90);
    ipco_box->append_child_box(irot); // entry 9
  } else if (get_orientation() == 5) {
    std::shared_ptr<Box_irot> irot = std::make_shared<Box_irot>();
    irot->set_rotation_ccw(3 * 90);
    ipco_box->append_child_box(irot); // entry 9
  } else {
    ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 9
  }

  if ((get_orientation() == 1) || (get_orientation() == 6)) {
    std::shared_ptr<Box_imir> imir = std::make_shared<Box_imir>();
    imir->set_mirror_direction(heif_transform_mirror_direction_horizontal);
    ipco_box->append_child_box(imir); // entry 10
  } else if ((get_orientation() == 3) || (get_orientation() == 4)) {
    std::shared_ptr<Box_imir> imir = std::make_shared<Box_imir>();
    imir->set_mirror_direction(heif_transform_mirror_direction_vertical);
    ipco_box->append_child_box(imir); // entry 10
  } else {
    ipco_box->append_child_box(std::make_shared<Box_free>()); // placeholder for entry 10
  }

  auto ipma_box = std::make_shared<Box_ipma>();
  file->set_ipma_box(ipma_box);
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(1)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, uint16_t(2)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{false, uint16_t(3)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(4)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(5)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(9)});
  ipma_box->add_property_for_item_ID(1, Box_ipma::PropertyAssociation{true, uint16_t(10)});

  if (get_alpha_item_data_size() != 0) {
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{true, uint16_t(6)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{false, uint16_t(2)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{true, uint16_t(7)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{false, uint16_t(8)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{true, uint16_t(9)});
    ipma_box->add_property_for_item_ID(2, Box_ipma::PropertyAssociation{true, uint16_t(10)});
  }
  // TODO: will need more once we support HDR / gainmap representation

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
