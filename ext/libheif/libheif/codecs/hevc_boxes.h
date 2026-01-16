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

#ifndef HEIF_HEVC_BOXES_H
#define HEIF_HEVC_BOXES_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"

#include <memory>
#include <string>
#include <vector>
#include "image-items/image_item.h"
#include "sequences/seq_boxes.h"


struct HEVCDecoderConfigurationRecord
{
  uint8_t configuration_version;
  uint8_t general_profile_space;
  bool general_tier_flag;
  uint8_t general_profile_idc;
  uint32_t general_profile_compatibility_flags;

  static const int NUM_CONSTRAINT_INDICATOR_FLAGS = 48;
  std::bitset<NUM_CONSTRAINT_INDICATOR_FLAGS> general_constraint_indicator_flags;

  uint8_t general_level_idc;

  uint16_t min_spatial_segmentation_idc;
  uint8_t parallelism_type;
  uint8_t chroma_format;
  uint8_t bit_depth_luma;
  uint8_t bit_depth_chroma;
  uint16_t avg_frame_rate;

  uint8_t constant_frame_rate;
  uint8_t num_temporal_layers;
  uint8_t temporal_id_nested;
  uint8_t m_length_size = 4; // default: 4 bytes for NAL unit lengths

  struct NalArray
  {
    uint8_t m_array_completeness;
    uint8_t m_NAL_unit_type;

    std::vector<std::vector<uint8_t> > m_nal_units;
  };

  enum Profile {
    Profile_Main = 1,
    Profile_Main10 = 2,
    Profile_MainStillPicture = 3,
    Profile_RExt = 4,
    Profile_HighThroughput = 5,
    Profile_ScreenCoding = 9,
    Profile_HighTHroughputScreenCoding = 11
  };

  std::vector<NalArray> m_nal_array;

  Error parse(BitstreamRange& range, const heif_security_limits* limits);

  Error write(StreamWriter& writer) const;

  bool get_general_profile_compatibility_flag(int idx) const;

  bool is_profile_compatibile(Profile) const;
};


class Box_hvcC : public Box
{
// allow access to protected parse() method
friend class Box_mini;

public:
  Box_hvcC()
  {
    set_short_type(fourcc("hvcC"));
  }

  bool is_essential() const override { return true; }


  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "HEVC Configuration Item"; }

  bool get_header_nals(std::vector<uint8_t>* dest) const;

  void set_configuration(const HEVCDecoderConfigurationRecord& config) { m_configuration = config; }

  const HEVCDecoderConfigurationRecord& get_configuration() const { return m_configuration; }

  HEVCDecoderConfigurationRecord& get_configuration() { return m_configuration; }

  // Add a header NAL to the hvcC. The data is the raw NAL data without any start-code or NAL size field.
  //
  // Note: we expect that all header NALs are added to the hvcC and
  // there are no additional header NALs in the bitstream.
  void append_nal_data(const std::vector<uint8_t>& nal);

  void append_nal_data(const uint8_t* data, size_t size);

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  HEVCDecoderConfigurationRecord m_configuration;
};


class Box_hvc1 : public Box_VisualSampleEntry
{
public:
  Box_hvc1()
  {
    set_short_type(fourcc("hvc1"));
  }
};



class SEIMessage
{
public:
  virtual ~SEIMessage() = default;
};


class SEIMessage_depth_representation_info : public SEIMessage,
                                             public heif_depth_representation_info
{
public:
};


Error decode_hevc_aux_sei_messages(const std::vector<uint8_t>& data,
                                   std::vector<std::shared_ptr<SEIMessage>>& msgs);

// Used for AVC, HEVC, and VVC.
std::vector<uint8_t> remove_start_code_emulation(const uint8_t* sps, size_t size);

Error parse_sps_for_hvcC_configuration(const uint8_t* sps, size_t size,
                                       HEVCDecoderConfigurationRecord* inout_config,
                                       int* width, int* height);

#endif
