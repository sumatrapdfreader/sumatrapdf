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

#ifndef LIBHEIF_PLUGIN_REGISTRY_H
#define LIBHEIF_PLUGIN_REGISTRY_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "error.h"

#include "libheif/heif.h"
#include "libheif/heif_plugin.h"


struct heif_encoder_descriptor
{
  const heif_encoder_plugin* plugin;

  const char* get_name() const { return plugin->get_plugin_name(); }

  heif_compression_format get_compression_format() const { return plugin->compression_format; }
};


struct encoder_descriptor_priority_order
{
  bool operator()(const std::unique_ptr<heif_encoder_descriptor>& a,
                  const std::unique_ptr<heif_encoder_descriptor>& b) const
  {
    return a->plugin->priority > b->plugin->priority;  // highest priority first
  }
};


extern std::set<const heif_decoder_plugin*>& get_decoder_plugins();

extern std::multiset<std::unique_ptr<heif_encoder_descriptor>,
                     encoder_descriptor_priority_order>& get_encoder_descriptors();

void register_default_plugins();

void register_decoder(const heif_decoder_plugin* decoder_plugin);

void register_encoder(const heif_encoder_plugin* encoder_plugin);

void heif_unregister_decoder_plugins();

void heif_unregister_encoder_plugins();

#if ENABLE_PLUGIN_LOADING
void heif_unregister_encoder_plugin(const heif_encoder_plugin* plugin);
#endif

bool has_decoder(heif_compression_format type, const char* name_id);

const heif_decoder_plugin* get_decoder(heif_compression_format type, const char* name_id);

const heif_encoder_plugin* get_encoder(heif_compression_format type);

std::vector<const heif_encoder_descriptor*>
get_filtered_encoder_descriptors(heif_compression_format,
                                 const char* name);

#endif
