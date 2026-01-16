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

#ifndef HEIF_AVC_BOXES_H
#define HEIF_AVC_BOXES_H

#include "box.h"
#include "error.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include "image-items/image_item.h"
#include "sequences/seq_boxes.h"


class Box_avcC : public Box {
public:
  Box_avcC() { set_short_type(fourcc("avcC")); }

  bool is_essential() const override { return true; }

  struct configuration {
    uint8_t configuration_version = 0;
    uint8_t AVCProfileIndication = 0; // profile_idc
    uint8_t profile_compatibility = 0; // constraint set flags
    uint8_t AVCLevelIndication = 0; // level_idc
    uint8_t lengthSize = 0; // length of NAL size field. Must be one of: 1,2,4
    heif_chroma chroma_format = heif_chroma_420; // Note: avcC integer value can be cast to heif_chroma enum
    uint8_t bit_depth_luma = 8;
    uint8_t bit_depth_chroma = 8;
  };

  void set_configuration(const configuration& config)
  {
    m_configuration = config;
  }

  const configuration& get_configuration() const
  {
    return m_configuration;
  }

  configuration& get_configuration()
  {
    return m_configuration;
  }

  const std::vector< std::vector<uint8_t> > getSequenceParameterSets() const
  {
    return m_sps;
  }

  const std::vector< std::vector<uint8_t> > getPictureParameterSets() const
  {
    return m_pps;
  }

  const std::vector< std::vector<uint8_t> > getSequenceParameterSetExt() const
  {
    return m_sps_ext;
  }

  void get_header_nals(std::vector<uint8_t>& data) const;

  void append_sps_nal(const uint8_t* data, size_t size);

  void append_sps_ext_nal(const uint8_t* data, size_t size);

  void append_pps_nal(const uint8_t* data, size_t size);

  std::string dump(Indent &) const override;

  Error write(StreamWriter &writer) const override;

protected:
  Error parse(BitstreamRange &range, const heif_security_limits* limits) override;

  std::string profileIndicationAsText() const;

private:
  configuration m_configuration;
  std::vector< std::vector<uint8_t> > m_sps;
  std::vector< std::vector<uint8_t> > m_pps;
  std::vector< std::vector<uint8_t> > m_sps_ext;
};



class Box_avc1 : public Box_VisualSampleEntry
{
public:
  Box_avc1()
  {
    set_short_type(fourcc("avc1"));
  }
};

Error parse_sps_for_avcC_configuration(const uint8_t* sps, size_t size,
                                       Box_avcC::configuration* inout_config,
                                       int* width, int* height);

#endif
