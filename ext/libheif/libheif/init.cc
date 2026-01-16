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

#include "init.h"
#include "libheif/heif.h"
#include "error.h"
#include "plugin_registry.h"
#include "common_utils.h"
#include "color-conversion/colorconversion.h"

#if ENABLE_MULTITHREADING_SUPPORT

#include <mutex>

#endif

#if defined(_WIN32)
#include "plugins_windows.h"
#else

#include "plugins_unix.h"

#endif

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

void heif_unload_all_plugins();

#if ENABLE_PLUGIN_LOADING

void heif_unregister_encoder_plugin(const heif_encoder_plugin* plugin);

std::vector<std::string> get_plugin_paths()
{
  std::vector<std::string> plugin_paths;

#if defined(_WIN32)
  plugin_paths = get_plugin_directories_from_environment_variable_windows();
#else
  plugin_paths = get_plugin_directories_from_environment_variable_unix();
#endif

  if (plugin_paths.empty()) {
    plugin_paths.push_back(LIBHEIF_PLUGIN_DIRECTORY);
  }

  return plugin_paths;
}

std::vector<std::string> list_all_potential_plugins_in_directory(const char* directory)
{
#if defined(_WIN32)
  return list_all_potential_plugins_in_directory_windows(directory);
#else
  return list_all_potential_plugins_in_directory_unix(directory);
#endif
}

#else
std::vector<std::string> get_plugin_paths()
{
  return {};
}
#endif


static int heif_library_initialization_count = 0;
static bool default_plugins_registered = true; // because they are implicitly registered at startup


#if ENABLE_MULTITHREADING_SUPPORT

static std::recursive_mutex& heif_init_mutex()
{
  static std::recursive_mutex init_mutex;
  return init_mutex;
}

#endif


void load_plugins_if_not_initialized_yet()
{
  if (heif_library_initialization_count == 0) {
    heif_init(nullptr);
  }
}


heif_error heif_init(heif_init_params*)
{
#if ENABLE_MULTITHREADING_SUPPORT
  std::lock_guard<std::recursive_mutex> lock(heif_init_mutex());
#endif

  if (heif_library_initialization_count == 0) {

    ColorConversionPipeline::init_ops();

    // --- initialize builtin plugins

    if (!default_plugins_registered) {
      register_default_plugins();
    }

#if ENABLE_PLUGIN_LOADING
    struct heif_error err{};
    std::vector<std::string> plugin_paths = get_plugin_paths();

    for (const auto& dir : plugin_paths) {
      err = heif_load_plugins(dir.c_str(), nullptr, nullptr, 0);
      if (err.code != 0) {
        return err;
      }
    }
#endif
  }

  // Note: it is important that we increase the counter AFTER initialization such that
  // 'load_plugins_if_not_initialized_yet()' can check this without having to lock the mutex.
  heif_library_initialization_count++;

  return {heif_error_Ok, heif_suberror_Unspecified, Error::kSuccess};
}


void heif_deinit()
{
#if ENABLE_MULTITHREADING_SUPPORT
  std::lock_guard<std::recursive_mutex> lock(heif_init_mutex());
#endif

  if (heif_library_initialization_count == 0) {
    // This case should never happen (heif_deinit() is called more often than heif_init()).
    return;
  }

  if (heif_library_initialization_count == 1) {
    heif_unregister_decoder_plugins();
    heif_unregister_encoder_plugins();
    default_plugins_registered = false;

    heif_unload_all_plugins();

    ColorConversionPipeline::release_ops();
  }

  // Note: contrary to heif_init() I think it does not matter whether we decrease the counter before or after deinitialization.
  // If the client application calls heif_deinit() in parallel to some other libheif function, it is really broken.
  heif_library_initialization_count--;
}


// This could be inside ENABLE_PLUGIN_LOADING, but the "include-what-you-use" checker cannot process this.
#include <vector>
#include <string>
#include <cstring>

#if ENABLE_PLUGIN_LOADING

#if defined(_WIN32)
typedef PluginLibrary_Windows PluginLibrary_SysDep;
#else
typedef PluginLibrary_Unix PluginLibrary_SysDep;
#endif


struct loaded_plugin
{
  PluginLibrary_SysDep plugin_library_handle;
  struct heif_plugin_info* info = nullptr;
  int openCnt = 0;
};

static std::vector<loaded_plugin> sLoadedPlugins;

MAYBE_UNUSED heif_error error_dlopen{heif_error_Plugin_loading_error, heif_suberror_Plugin_loading_error, "Cannot open plugin (dlopen)."};
MAYBE_UNUSED heif_error error_plugin_not_loaded{heif_error_Plugin_loading_error, heif_suberror_Plugin_is_not_loaded, "Trying to remove a plugin that is not loaded."};
MAYBE_UNUSED heif_error error_cannot_read_plugin_directory{heif_error_Plugin_loading_error, heif_suberror_Cannot_read_plugin_directory, "Cannot read plugin directory."};

MAYBE_UNUSED static void unregister_plugin(const heif_plugin_info* info)
{
  switch (info->type) {
    case heif_plugin_type_encoder: {
      auto* encoder_plugin = static_cast<const heif_encoder_plugin*>(info->plugin);
      heif_unregister_encoder_plugin(encoder_plugin);
      break;
    }
    case heif_plugin_type_decoder: {
      // TODO
    }
  }
}


struct heif_error heif_load_plugin(const char* filename, struct heif_plugin_info const** out_plugin)
{
#if ENABLE_MULTITHREADING_SUPPORT
  std::lock_guard<std::recursive_mutex> lock(heif_init_mutex());
#endif

  PluginLibrary_SysDep plugin;
  auto err = plugin.load_from_file(filename);
  if (err.code) {
    return err;
  }

  heif_plugin_info* plugin_info = plugin.get_plugin_info();

  // --- check whether the plugin is already loaded
  // If yes, return pointer to existing plugin.

  for (auto& p : sLoadedPlugins) {
    if (p.plugin_library_handle == plugin) {
      if (out_plugin) {
        *out_plugin = p.info;
        p.openCnt++;
        return heif_error_ok;
      }
    }
  }

  loaded_plugin loadedPlugin;
  loadedPlugin.plugin_library_handle = plugin;
  loadedPlugin.openCnt = 1;
  loadedPlugin.info = plugin_info;
  sLoadedPlugins.push_back(loadedPlugin);

  *out_plugin = plugin_info;

  switch (loadedPlugin.info->type) {
    case heif_plugin_type_encoder: {
      auto* encoder_plugin = static_cast<const heif_encoder_plugin*>(plugin_info->plugin);
      if (encoder_plugin->plugin_api_version < heif_encoder_plugin_minimum_version) {
        return {heif_error_Plugin_loading_error,
                heif_suberror_Unsupported_plugin_version,
                "Encoder plugin needs to be at least version " STRINGIFY(heif_encoder_plugin_latest_version)
        };
      }

      if (encoder_plugin->plugin_api_version >= 4 &&
          encoder_plugin->minimum_required_libheif_version > heif_get_version_number()) {
        return {
          heif_error_Plugin_loading_error,
          heif_suberror_Unsupported_plugin_version,
          "Encoder plugin requires at least libheif version " LIBHEIF_VERSION
        };
      }

      struct heif_error err = heif_register_encoder_plugin(encoder_plugin);
      if (err.code) {
        return err;
      }
      break;
    }

    case heif_plugin_type_decoder: {
      auto* decoder_plugin = static_cast<const heif_decoder_plugin*>(plugin_info->plugin);
      if (decoder_plugin->plugin_api_version < heif_decoder_plugin_minimum_version) {
        return {heif_error_Plugin_loading_error,
                heif_suberror_Unsupported_plugin_version,
                "Decoder plugin needs to be at least version " STRINGIFY(heif_decoder_plugin_latest_version)
        };
      }

      if (decoder_plugin->plugin_api_version >= 4 &&
          decoder_plugin->minimum_required_libheif_version > heif_get_version_number()) {
        return {
          heif_error_Plugin_loading_error,
          heif_suberror_Unsupported_plugin_version,
          "Decoder plugin requires at least libheif version " LIBHEIF_VERSION
        };
      }

      struct heif_error err = heif_register_decoder_plugin(decoder_plugin);
      if (err.code) {
        return err;
      }
      break;
    }
  }

  return heif_error_ok;
}

struct heif_error heif_unload_plugin(const struct heif_plugin_info* plugin)
{
#if ENABLE_MULTITHREADING_SUPPORT
  std::lock_guard<std::recursive_mutex> lock(heif_init_mutex());
#endif

  for (size_t i = 0; i < sLoadedPlugins.size(); i++) {
    auto& p = sLoadedPlugins[i];

    if (p.info == plugin) {
      p.plugin_library_handle.release();
      p.openCnt--;

      if (p.openCnt == 0) {
        unregister_plugin(plugin);

        sLoadedPlugins[i] = sLoadedPlugins.back();
        sLoadedPlugins.pop_back();
      }

      return heif_error_ok;
    }
  }

  return error_plugin_not_loaded;
}

void heif_unload_all_plugins()
{
#if ENABLE_MULTITHREADING_SUPPORT
  std::lock_guard<std::recursive_mutex> lock(heif_init_mutex());
#endif

  for (auto& p : sLoadedPlugins) {
    unregister_plugin(p.info);

    for (int i = 0; i < p.openCnt; i++) {
      p.plugin_library_handle.release();
    }
  }

  sLoadedPlugins.clear();
}


struct heif_error heif_load_plugins(const char* directory,
                                    const struct heif_plugin_info** out_plugins,
                                    int* out_nPluginsLoaded,
                                    int output_array_size)
{
  auto libraryFiles = list_all_potential_plugins_in_directory(directory);

  int nPlugins = 0;

  // Loading the plugins may return several errors, but we can only return one of them,
  // which is remembered here.
  heif_error err_result = heif_error_ok;

  for (const auto& filename : libraryFiles) {
    const struct heif_plugin_info* info = nullptr;
    auto err = heif_load_plugin(filename.c_str(), &info);
    if (err.code == 0) {
      if (out_plugins) {
        if (nPlugins == output_array_size) {
          break;
        }

        out_plugins[nPlugins] = info;
      }

      nPlugins++;
    }
    else {
      // remember error
      err_result = err;
    }
  }

  if (nPlugins < output_array_size && out_plugins) {
    out_plugins[nPlugins] = nullptr;
  }

  if (out_nPluginsLoaded) {
    *out_nPluginsLoaded = nPlugins;
  }

  return err_result;
}

#else
static heif_error heif_error_plugins_unsupported{heif_error_Unsupported_feature, heif_suberror_Unspecified, "Plugins are not supported"};

struct heif_error heif_load_plugin(const char* filename, struct heif_plugin_info const** out_plugin)
{
  return heif_error_plugins_unsupported;
}


struct heif_error heif_unload_plugin(const struct heif_plugin_info* plugin)
{
  return heif_error_plugins_unsupported;
}


void heif_unload_all_plugins() {}

struct heif_error heif_load_plugins(const char* directory,
                                    const struct heif_plugin_info** out_plugins,
                                    int* out_nPluginsLoaded,
                                    int output_array_size)
{
  if (out_nPluginsLoaded) {
    *out_nPluginsLoaded = 0;
  }

  return heif_error_plugins_unsupported;
}

#endif


const char* const* heif_get_plugin_directories()
{
  auto plugin_paths = get_plugin_paths();
  size_t n = plugin_paths.size();

  auto out_paths = new char* [n + 1];
  for (size_t i = 0; i < n; i++) {
    out_paths[i] = new char[plugin_paths[i].size() + 1];
    strcpy(out_paths[i], plugin_paths[i].c_str());
  }

  out_paths[n] = nullptr;

  return out_paths;
}


void heif_free_plugin_directories(const char* const* paths)
{
  for (int i = 0; paths[i]; i++) {
    delete[] paths[i];
  }

  delete[] paths;
}
