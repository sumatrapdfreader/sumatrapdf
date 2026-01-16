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

#include "heif_library.h"
#include "heif_plugin.h"
#include "api_structs.h"
#include "plugin_registry.h"

#ifdef _WIN32
// for _write
#include <io.h>
#else

#include <unistd.h>

#endif

#include <cassert>


static heif_error error_unsupported_plugin_version = {heif_error_Usage_error,
                                                             heif_suberror_Unsupported_plugin_version,
                                                             "Unsupported plugin version"};


const char* heif_get_version()
{
  return (LIBHEIF_VERSION);
}

uint32_t heif_get_version_number()
{
  return (LIBHEIF_NUMERIC_VERSION);
}

int heif_get_version_number_major()
{
  return ((LIBHEIF_NUMERIC_VERSION) >> 24) & 0xFF;
}

int heif_get_version_number_minor()
{
  return ((LIBHEIF_NUMERIC_VERSION) >> 16) & 0xFF;
}

int heif_get_version_number_maintenance()
{
  return ((LIBHEIF_NUMERIC_VERSION) >> 8) & 0xFF;
}



heif_error heif_register_decoder_plugin(const heif_decoder_plugin* decoder_plugin)
{
  if (!decoder_plugin) {
    return heif_error_null_pointer_argument;
  }
  else if (decoder_plugin->plugin_api_version > heif_decoder_plugin_latest_version) {
    return error_unsupported_plugin_version;
  }

  register_decoder(decoder_plugin);
  return heif_error_success;
}

heif_error heif_register_encoder_plugin(const heif_encoder_plugin* encoder_plugin)
{
  if (!encoder_plugin) {
    return heif_error_null_pointer_argument;
  }
  else if (encoder_plugin->plugin_api_version > heif_encoder_plugin_latest_version) {
    return error_unsupported_plugin_version;
  }

  register_encoder(encoder_plugin);
  return heif_error_success;
}


void heif_string_release(const char* str)
{
  delete[] str;
}


// DEPRECATED
heif_error heif_register_decoder(heif_context* heif, const heif_decoder_plugin* decoder_plugin)
{
  return heif_register_decoder_plugin(decoder_plugin);
}
