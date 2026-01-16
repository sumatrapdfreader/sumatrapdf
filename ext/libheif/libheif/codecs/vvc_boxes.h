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

#ifndef LIBHEIF_VVC_BOXES_H
#define LIBHEIF_VVC_BOXES_H

#include "box.h"
#include <string>
#include <vector>
#include "image-items/image_item.h"
#include <memory>
#include "sequences/seq_boxes.h"


class Box_vvcC : public FullBox
{
public:
  Box_vvcC()
  {
    set_short_type(fourcc("vvcC"));
  }

  bool is_essential() const override { return true; }

  struct VvcPTLRecord {
    uint8_t num_bytes_constraint_info; // 6 bits
    uint8_t general_profile_idc; // 7 bits
    uint8_t general_tier_flag; // 1 bit
    uint8_t general_level_idc; // 8 bits
    uint8_t ptl_frame_only_constraint_flag; // 1 bit
    uint8_t ptl_multi_layer_enabled_flag; // 1 bit
    std::vector<uint8_t> general_constraint_info;

    std::vector<bool> ptl_sublayer_level_present_flag; // TODO: should we save this here or can we simply derive it on the fly?

    std::vector<uint8_t> sublayer_level_idc;
    std::vector<uint32_t> general_sub_profile_idc;
  };

  struct configuration
  {
    uint8_t LengthSizeMinusOne = 3;  // 0,1,3   default: 4 bytes for NAL unit lengths
    bool ptl_present_flag = true;

    // only if PTL present
    uint16_t ols_idx; // 9 bits
    uint8_t num_sublayers; // 3 bits
    uint8_t constant_frame_rate; // 2 bits
    uint8_t chroma_format_idc; // 2 bits
    uint8_t bit_depth_minus8; // 3 bits
    struct VvcPTLRecord native_ptl;
    uint16_t max_picture_width;
    uint16_t max_picture_height;
    uint16_t avg_frame_rate;
  };


  std::string dump(Indent&) const override;

  bool get_headers(std::vector<uint8_t>* dest) const;

  void set_configuration(const configuration& config) { m_configuration = config; }

  const configuration& get_configuration() const { return m_configuration; }

  configuration& get_configuration() { return m_configuration; }

  void append_nal_data(const std::vector<uint8_t>& nal);
  void append_nal_data(const uint8_t* data, size_t size);

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
    struct NalArray
    {
      bool m_array_completeness;
      uint8_t m_NAL_unit_type;

      std::vector<std::vector<uint8_t> > m_nal_units; // only one NAL item for DCI and OPI
    };

  configuration m_configuration;
  std::vector<NalArray> m_nal_array;
};


class Box_vvc1 : public Box_VisualSampleEntry
{
public:
  Box_vvc1()
  {
    set_short_type(fourcc("vvc1"));
  }
};


Error parse_sps_for_vvcC_configuration(const uint8_t* sps, size_t size,
                                       Box_vvcC::configuration* inout_config,
                                       int* width, int* height);

#endif // LIBHEIF_VVC_BOXES_H
