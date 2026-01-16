/*
 * HEIF codec.
 * Copyright (c) 2022 Dirk Farin <dirk.farin@gmail.com>
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


#ifndef LIBHEIF_INIT_H
#define LIBHEIF_INIT_H

#include "libheif/heif.h"
#include <string>
#include <vector>

extern heif_error error_dlopen;
extern heif_error error_plugin_not_loaded;
extern heif_error error_cannot_read_plugin_directory;

// TODO: later, we might defer the default plugin initialization to when they are actually used for the first time.
// That would prevent them from being initialized every time at program start, even when the application software uses heif_init() later on.

// Note: the loaded plugin is not released automatically then the class is released, because this would require that
// we reference-count the handle. We do not really need this since releasing the library explicitly with release() is simple enough.
class PluginLibrary
{
public:
  virtual heif_error load_from_file(const char*) = 0;

  virtual void release() = 0;

  virtual heif_plugin_info* get_plugin_info() = 0;
};


std::vector<std::string> get_plugin_paths();

// This is for implicit initialization when heif_init() is not called.
void load_plugins_if_not_initialized_yet();

#endif //LIBHEIF_INIT_H
