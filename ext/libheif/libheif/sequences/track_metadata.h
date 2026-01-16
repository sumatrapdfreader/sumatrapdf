/*
 * HEIF image base codec.
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

#ifndef LIBHEIF_TRACK_METADATA_H
#define LIBHEIF_TRACK_METADATA_H

#include "track.h"
#include <string>
#include <memory>
#include <vector>


class Track_Metadata : public Track {
public:
  //Track(HeifContext* ctx);

  Track_Metadata(HeifContext* ctx, uint32_t track_id, std::string uri, const TrackOptions* options);

  Track_Metadata(HeifContext* ctx);

  ~Track_Metadata() override = default;

  // load track from file
  Error load(const std::shared_ptr<Box_trak>&) override;

  Error write_raw_metadata(const heif_raw_sequence_sample*);

private:
  std::string m_uri;
};

#endif //LIBHEIF_TRACK_METADATA_H
