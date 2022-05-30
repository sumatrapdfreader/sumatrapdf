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

#ifndef HEIF_HEVC_H
#define HEIF_HEVC_H

#include "heif.h"
#include "box.h"
#include "error.h"

#include <memory>
#include <vector>


namespace heif {

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


  Error parse_sps_for_hvcC_configuration(const uint8_t* sps, size_t size,
                                         Box_hvcC::configuration* inout_config,
                                         int* width, int* height);

}

#endif
