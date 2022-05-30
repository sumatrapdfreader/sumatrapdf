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

#ifndef HEIF_API_STRUCTS_H
#define HEIF_API_STRUCTS_H

#include "heif_image.h"
#include "heif_context.h"

#include <memory>

struct heif_image_handle
{
  std::shared_ptr<heif::HeifContext::Image> image;

  // store reference to keep the context alive while we are using the handle (issue #147)
  std::shared_ptr<heif::HeifContext> context;
};


struct heif_image
{
  std::shared_ptr<heif::HeifPixelImage> image;
};


struct heif_context
{
  std::shared_ptr<heif::HeifContext> context;
};


struct heif_encoder
{
  heif_encoder(const struct heif_encoder_plugin* plugin);

  ~heif_encoder();

  struct heif_error alloc();

  void release();


  const struct heif_encoder_plugin* plugin;
  void* encoder = nullptr;
};


#endif
