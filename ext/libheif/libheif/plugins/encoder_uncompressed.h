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

#ifndef LIBHEIF_ENCODER_UNCOMPRESSED_H
#define LIBHEIF_ENCODER_UNCOMPRESSED_H

#include "common_utils.h"

// This is a dummy module. It does not actually do anything except parameter parsing.
// The actual codec is included in the library.

struct encoder_struct_uncompressed
{
  // --- parameters

  enum class InterleaveFormat
  {
    Planar, Row, Pixel
  };

  InterleaveFormat interleaveFormat = InterleaveFormat::Planar;

  // --- output

  //bool data_read = false;
  //std::vector<uint8_t> compressedData;
};


const struct heif_encoder_plugin* get_encoder_plugin_uncompressed();

#if 0 // PLUGIN_UNCOMPRESSED
extern "C" {
MAYBE_UNUSED LIBHEIF_API extern heif_plugin_info plugin_info;
}
#endif

#endif //LIBHEIF_ENCODER_UNCOMPRESSED_H
