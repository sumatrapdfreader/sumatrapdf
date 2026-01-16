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
#include <utility>
#include "nalu_utils.h"

NalUnit::NalUnit()
{
    nal_data_ptr = NULL;
    nal_unit_type = 0;
    nal_data_size = 0;
}

bool NalUnit::set_data(const unsigned char* in_data, int n)
{
    nal_data_ptr = in_data;
    nal_unit_type = bitExtracted(nal_data_ptr[0], 6, 2);
    nal_data_size = n;
    return true;
}

int NalUnit::bitExtracted(int number, int bits_count, int position_nr)
{
    return (((1 << bits_count) - 1) & (number >> (position_nr - 1)));
}

size_t NalMap::count(int nal_type)
{
    return map.count(nal_type);
}

const unsigned char* NalMap::data(int nal_type) 
{
    return map[nal_type]->data();
}

int NalMap::size(int nal_type) 
{
    return map[nal_type]->size();
}

const heif_error NalMap::parseHevcNalu(const uint8_t *cdata, size_t size)
{
    size_t ptr = 0;
    while (ptr < size)
    {
        if (4 > size - ptr)
        {
            struct heif_error err = {heif_error_Decoder_plugin_error,
                                    heif_suberror_End_of_data,
                                    "insufficient data"};
            return err;
        }

        uint32_t nal_size = (cdata[ptr] << 24) | (cdata[ptr + 1] << 16) | (cdata[ptr + 2] << 8) | (cdata[ptr + 3]);
        ptr += 4;

        if (nal_size > size - ptr)
        {
            struct heif_error err = {heif_error_Decoder_plugin_error,
                                    heif_suberror_End_of_data,
                                    "insufficient data"};
            return err;
        }

        std::unique_ptr<NalUnit> nal_unit = std::unique_ptr<NalUnit>(new NalUnit());
        nal_unit->set_data(cdata + ptr, nal_size);

        // overwrite NalMap (frees old NalUnit, if it was set)
        map[nal_unit->unit_type()] = std::move(nal_unit);

        ptr += nal_size;
    }

    return heif_error_success;
}

void NalMap::clear() { map.clear(); }