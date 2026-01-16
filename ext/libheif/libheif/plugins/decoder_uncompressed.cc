/*
 * JPEG codec.
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

#include "libheif/heif.h"
#include "libheif/heif_plugin.h"
#include "decoder_uncompressed.h"

struct uncompressed_decoder
{
};

static const int UNCOMPRESSED_PLUGIN_PRIORITY = 100;


static const char* uncompressed_plugin_name()
{
  return "builtin";
}


static int uncompressed_does_support_format(enum heif_compression_format format)
{
  if (format == heif_compression_uncompressed) {
    return UNCOMPRESSED_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}




static const heif_decoder_plugin decoder_uncompressed
    {
        5,
        uncompressed_plugin_name,
        nullptr,
        nullptr,
        uncompressed_does_support_format,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        "uncompressed",
        nullptr,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,0,0),
        nullptr
    };


const heif_decoder_plugin* get_decoder_plugin_uncompressed()
{
  return &decoder_uncompressed;
}
