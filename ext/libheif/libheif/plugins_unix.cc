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


#include "plugins_unix.h"
#include "libheif/heif_plugin.h"
#include <sstream>

#include <dlfcn.h>
#include <cstring>

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <dirent.h>


std::vector<std::string> get_plugin_directories_from_environment_variable_unix()
{
  char* path_variable = getenv("LIBHEIF_PLUGIN_PATH");
  if (path_variable == nullptr) {
    return {};
  }

  // --- split LIBHEIF_PLUGIN_PATH value at ':' into separate directories

  std::vector<std::string> plugin_paths;

  std::istringstream paths(path_variable);
  std::string dir;
  while (getline(paths, dir, ':')) {
    plugin_paths.push_back(dir);
  }

  return plugin_paths;
}


std::vector<std::string> list_all_potential_plugins_in_directory_unix(const char* directory)
{
  std::vector<std::string> result;

  DIR* dir = opendir(directory);
  if (dir == nullptr) {
    return {}; // TODO: return error_cannot_read_plugin_directory;
  }

  struct dirent* d;
  for (;;) {
    d = readdir(dir);
    if (d == nullptr) {
      break;
    }

    bool correct_filetype = true;
#ifdef DT_REG
    correct_filetype = (d->d_type == DT_REG || d->d_type == DT_LNK || d->d_type == DT_UNKNOWN);
#endif

    if (correct_filetype &&
	strlen(d->d_name) > 3 &&
        strcmp(d->d_name + strlen(d->d_name) - 3, ".so") == 0) {
      std::string filename = directory;
      filename += '/';
      filename += d->d_name;
      //printf("load %s\n", filename.c_str());

      result.push_back(filename);
    }
  }

  closedir(dir);

  return result;
}


heif_error PluginLibrary_Unix::load_from_file(const char* filename)
{
  m_library_handle = dlopen(filename, RTLD_LAZY);
  if (!m_library_handle) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return error_dlopen;
  }

  m_plugin_info = (heif_plugin_info*) dlsym(m_library_handle, "plugin_info");
  if (!m_plugin_info) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return error_dlopen;
  }

  return heif_error_ok;
}

void PluginLibrary_Unix::release()
{
  if (m_library_handle) {
    dlclose(m_library_handle);
    m_library_handle = nullptr;
  }
}

