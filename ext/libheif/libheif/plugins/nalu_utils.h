/*
 * HEIF codec.
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

#include <map>
#include <memory>
#include "libheif/heif.h"

static const int AVC_NAL_UNIT_MAX_VCL    = 5;
static const int AVC_NAL_UNIT_SPS_NUT    = 7;
static const int AVC_NAL_UNIT_PPS_NUT    = 8;
static const int AVC_NAL_UNIT_SPS_EXT_NUT = 13;

static const int HEVC_NAL_UNIT_MAX_VCL    = 31;
static const int HEVC_NAL_UNIT_VPS_NUT    = 32;
static const int HEVC_NAL_UNIT_SPS_NUT    = 33;
static const int HEVC_NAL_UNIT_PPS_NUT    = 34;
static const int HEVC_NAL_UNIT_IDR_W_RADL = 19;
static const int HEVC_NAL_UNIT_IDR_N_LP   = 20;

static const int VVC_NAL_UNIT_MAX_VCL    = 11;
static const int VVC_NAL_UNIT_VPS_NUT    = 14;
static const int VVC_NAL_UNIT_SPS_NUT    = 15;
static const int VVC_NAL_UNIT_PPS_NUT    = 16;
static const int VVC_NAL_UNIT_SUFFIX_SEI_NUT = 24;


class NalUnit
{
public:
    NalUnit();
    ~NalUnit() = default;
    bool set_data(const unsigned char* in_data, int n);
    int size() const { return nal_data_size; }
    int unit_type() const { return nal_unit_type;  }
    const unsigned char* data() const { return nal_data_ptr; }
    int bitExtracted(int number, int bits_count, int position_nr);
private:
    const unsigned char* nal_data_ptr;
    int nal_unit_type;
    int nal_data_size;
};

class NalMap
{
public:
    size_t count(int nal_type);

    const unsigned char* data(int nal_type);

    int size(int nal_type);

    const heif_error parseHevcNalu(const uint8_t *cdata, size_t size);

    void clear();
private:
    std::map<int, std::unique_ptr<NalUnit>> map;
};