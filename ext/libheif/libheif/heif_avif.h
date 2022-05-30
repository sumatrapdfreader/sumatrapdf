/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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

#ifndef HEIF_AVIF_H
#define HEIF_AVIF_H

#include <cassert>
#include <cmath>
#include <memory>

#include "heif.h"
#include "box.h"
#include "error.h"


namespace heif {

  class HeifPixelImage;

  Error fill_av1C_configuration(Box_av1C::configuration* inout_config, const std::shared_ptr<HeifPixelImage>& image);

  bool fill_av1C_configuration_from_stream(Box_av1C::configuration* out_config, const uint8_t* data, int dataSize);

}

#endif
