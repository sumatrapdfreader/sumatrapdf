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

#ifndef HEIF_API_STRUCTS_H
#define HEIF_API_STRUCTS_H

#include "pixelimage.h"
#include "context.h"

#include <memory>
#include <vector>
#include <string>
#include "image-items/image_item.h"

struct heif_image_handle
{
  std::shared_ptr<ImageItem> image;

  // store reference to keep the context alive while we are using the handle (issue #147)
  std::shared_ptr<HeifContext> context;
};


struct heif_track
{
  std::shared_ptr<Track> track;

  // store reference to keep the context alive while we are using the handle (issue #147)
  std::shared_ptr<HeifContext> context;
};

struct heif_raw_sequence_sample
{
  ~heif_raw_sequence_sample()
  {
    heif_tai_timestamp_packet_release(timestamp);
  }

  std::vector<uint8_t> data;
  uint32_t duration = 0;

  heif_tai_timestamp_packet* timestamp = nullptr;
  std::string gimi_sample_content_id;
};


struct heif_image
{
  std::shared_ptr<HeifPixelImage> image;
};


struct heif_context
{
  std::shared_ptr<HeifContext> context;
};


struct heif_encoder
{
  explicit heif_encoder(const heif_encoder_plugin* plugin);

  ~heif_encoder();

  heif_error alloc();

  void release();


  const struct heif_encoder_plugin* plugin;
  void* encoder = nullptr;
};


struct heif_region_item
{
  std::shared_ptr<HeifContext> context;
  std::shared_ptr<RegionItem> region_item;
};


struct heif_region
{
  std::shared_ptr<HeifContext> context; // we need this to perform coordinate transformation
  //heif_item_id parent_region_item_id; // we need this to perform coordinate transformation
  std::shared_ptr<RegionItem> region_item;
  std::shared_ptr<RegionGeometry> region;
};

struct heif_text_item
{
  std::shared_ptr<HeifContext> context;
  std::shared_ptr<TextItem> text_item;
};


#endif
