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

#ifndef LIBHEIF_HEIF_PLUGIN_REGISTRY_H
#define LIBHEIF_HEIF_PLUGIN_REGISTRY_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "error.h"

#include "heif.h"
#include "heif_plugin.h"


struct heif_encoder_descriptor
{
  const struct heif_encoder_plugin* plugin;

  const char* get_name() const
  { return plugin->get_plugin_name(); }

  enum heif_compression_format get_compression_format() const
  { return plugin->compression_format; }
};


namespace heif {

  extern std::set<const struct heif_decoder_plugin*> s_decoder_plugins;

  void register_decoder(const heif_decoder_plugin* decoder_plugin);

  void register_encoder(const heif_encoder_plugin* encoder_plugin);

  const struct heif_decoder_plugin* get_decoder(enum heif_compression_format type);

  const struct heif_encoder_plugin* get_encoder(enum heif_compression_format type);

  std::vector<const struct heif_encoder_descriptor*>
  get_filtered_encoder_descriptors(enum heif_compression_format,
                                   const char* name);
}

#endif
