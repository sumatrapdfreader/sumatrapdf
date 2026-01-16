/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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


#ifndef LIBHEIF_PLUGINS_WINDOWS_H
#define LIBHEIF_PLUGINS_WINDOWS_H

#include <vector>
#include <string>
#include "init.h"
#include <windows.h>

std::vector<std::string> get_plugin_directories_from_environment_variable_windows();

std::vector<std::string> list_all_potential_plugins_in_directory_windows(const char*);

class PluginLibrary_Windows : public PluginLibrary
{
public:
  heif_error load_from_file(const char* filename) override;

  void release() override;

  heif_plugin_info* get_plugin_info() override { return m_plugin_info; }

  bool operator==(const PluginLibrary_Windows& b) const
  {
    return m_filename == b.m_filename;
  }

private:
  std::string m_filename;
  HMODULE m_library_handle = nullptr;
  heif_plugin_info* m_plugin_info = nullptr;
};

#endif //LIBHEIF_PLUGINS_WINDOWS_H
