/*
 * HEIF codec.
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef THIRD_PARTY_LIBHEIF_LIBHEIF_PLUGINS_DECODER_WEBCODECS_H_
#define THIRD_PARTY_LIBHEIF_LIBHEIF_PLUGINS_DECODER_WEBCODECS_H_

#include "common_utils.h"

const struct heif_decoder_plugin* get_decoder_plugin_webcodecs();

#if PLUGIN_WEBCODECS
extern "C" {
MAYBE_UNUSED LIBHEIF_API extern heif_plugin_info plugin_info;
}
#endif

#endif  // THIRD_PARTY_LIBHEIF_LIBHEIF_PLUGINS_DECODER_WEBCODECS_H_
