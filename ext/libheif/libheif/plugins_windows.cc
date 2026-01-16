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


#include "plugins_windows.h"
#include "libheif/heif_plugin.h"
#include <sstream>


std::vector<std::string> get_plugin_directories_from_environment_variable_windows()
{
  char* path_variable = getenv("LIBHEIF_PLUGIN_PATH");
  if (path_variable == nullptr) {
    return {};
  }

  // --- split LIBHEIF_PLUGIN_PATH value at ';' into separate directories

  std::vector<std::string> plugin_paths;

  std::istringstream paths(path_variable);
  std::string dir;
  while (getline(paths, dir, ';')) {
    plugin_paths.push_back(dir);
  }

  return plugin_paths;
}


std::vector<std::string> list_all_potential_plugins_in_directory_windows(const char* directory)
{
  std::vector<std::string> result;

  HANDLE hFind;
  WIN32_FIND_DATA FindFileData;

  std::string findPattern{directory};
  findPattern += "\\*.dll";

#if 0
  DIR* dir = opendir(directory);
  if (dir == nullptr) {
    return error_cannot_read_plugin_directory;
  }
#endif

 if ((hFind = FindFirstFile(findPattern.c_str(), &FindFileData)) != INVALID_HANDLE_VALUE) {
    do {
      std::string filename = directory;
      filename += '/';
      filename += FindFileData.cFileName;

      result.push_back(filename);
    } while (FindNextFile(hFind, &FindFileData));

    FindClose(hFind);
  }

  return result;
}



heif_error PluginLibrary_Windows::load_from_file(const char* filename)
{
  m_library_handle = LoadLibraryA(filename);
  if (!m_library_handle) {
    fprintf(stderr, "LoadLibraryA error: %lu\n", GetLastError());
    return error_dlopen;
  }

  m_plugin_info = (heif_plugin_info*) GetProcAddress(m_library_handle, "plugin_info");
  if (!m_plugin_info) {
    fprintf(stderr, "GetProcAddress error for dll '%s': %lu\n", filename, GetLastError());
    return error_dlopen;
  }

  // Remember filename for comparison whether the plugin was already loaded.
  // We need this since LoadLibraryA() returns a separate instance if we load the same DLL twice.
  m_filename = filename;

  return heif_error_ok;
}

void PluginLibrary_Windows::release()
{
  if (m_library_handle) {
    FreeLibrary(m_library_handle);
    m_library_handle = nullptr;
  }
}
