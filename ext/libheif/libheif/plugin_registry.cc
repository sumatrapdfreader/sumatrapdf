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

#include <utility>
#include <cstring>
#include <algorithm>

#include "plugin_registry.h"
#include "init.h"

#if HAVE_WEBCODECS
#include "plugins/decoder_webcodecs.h"
#endif

#if HAVE_LIBDE265
#include "plugins/decoder_libde265.h"
#endif

#if HAVE_X265
#include "plugins/encoder_x265.h"
#endif

#if HAVE_KVAZAAR
#include "plugins/encoder_kvazaar.h"
#endif

#if HAVE_UVG266
#include "plugins/encoder_uvg266.h"
#endif

#if HAVE_VVDEC
#include "plugins/decoder_vvdec.h"
#endif

#if HAVE_VVENC
#include "plugins/encoder_vvenc.h"
#endif

#if HAVE_AOM_ENCODER
#include "plugins/encoder_aom.h"
#endif

#if HAVE_AOM_DECODER
#include "plugins/decoder_aom.h"
#endif

#if HAVE_RAV1E
#include "plugins/encoder_rav1e.h"
#endif

#if HAVE_DAV1D
#include "plugins/decoder_dav1d.h"
#endif

#if HAVE_SvtEnc
#include "plugins/encoder_svt.h"
#endif

#if HAVE_FFMPEG_DECODER
#include "plugins/decoder_ffmpeg.h"
#endif

#if WITH_UNCOMPRESSED_CODEC
#include "plugins/encoder_uncompressed.h"
#include "plugins/decoder_uncompressed.h"
#endif

#if HAVE_JPEG_DECODER
#include "plugins/decoder_jpeg.h"
#endif

#if HAVE_JPEG_ENCODER
#include "plugins/encoder_jpeg.h"
#endif

#if HAVE_X264
#include "plugins/encoder_x264.h"
#endif

#if HAVE_OpenH264_DECODER
#include "plugins/decoder_openh264.h"
#endif

#if HAVE_OPENJPEG_ENCODER
#include "plugins/encoder_openjpeg.h"
#endif

#if HAVE_OPENJPEG_DECODER
#include "plugins/decoder_openjpeg.h"
#endif

#include "plugins/encoder_mask.h"

#if HAVE_OPENJPH_ENCODER
#include "plugins/encoder_openjph.h"
#endif

std::set<const heif_decoder_plugin*> s_decoder_plugins;

std::multiset<std::unique_ptr<heif_encoder_descriptor>,
              encoder_descriptor_priority_order> s_encoder_descriptors;

std::set<const heif_decoder_plugin*>& get_decoder_plugins()
{
  load_plugins_if_not_initialized_yet();

  return s_decoder_plugins;
}

extern std::multiset<std::unique_ptr<heif_encoder_descriptor>,
                     encoder_descriptor_priority_order>& get_encoder_descriptors()
{
  load_plugins_if_not_initialized_yet();

  return s_encoder_descriptors;
}


// Note: we cannot move this to 'heif_init' because we have to make sure that this is initialized
// AFTER the two global std::set above.
static class Register_Default_Plugins
{
public:
  Register_Default_Plugins()
  {
    register_default_plugins();
  }
} dummy;


void register_default_plugins()
{
#if HAVE_WEBCODECS
  register_decoder(get_decoder_plugin_webcodecs());
#endif

#if HAVE_LIBDE265
  register_decoder(get_decoder_plugin_libde265());
#endif

#if HAVE_X265
  register_encoder(get_encoder_plugin_x265());
#endif

#if HAVE_KVAZAAR
  register_encoder(get_encoder_plugin_kvazaar());
#endif

#if HAVE_UVG266
  register_encoder(get_encoder_plugin_uvg266());
#endif

#if HAVE_VVENC
  register_encoder(get_encoder_plugin_vvenc());
#endif

#if HAVE_VVDEC
  register_decoder(get_decoder_plugin_vvdec());
#endif

#if HAVE_AOM_ENCODER
  register_encoder(get_encoder_plugin_aom());
#endif

#if HAVE_AOM_DECODER
  register_decoder(get_decoder_plugin_aom());
#endif

#if HAVE_RAV1E
  register_encoder(get_encoder_plugin_rav1e());
#endif

#if HAVE_DAV1D
  register_decoder(get_decoder_plugin_dav1d());
#endif

#if HAVE_SvtEnc
  register_encoder(get_encoder_plugin_svt());
#endif

#if HAVE_FFMPEG_DECODER
  register_decoder(get_decoder_plugin_ffmpeg());
#endif

#if HAVE_JPEG_DECODER
  register_decoder(get_decoder_plugin_jpeg());
#endif

#if HAVE_JPEG_ENCODER
  register_encoder(get_encoder_plugin_jpeg());
#endif

#if HAVE_OPENJPEG_ENCODER
  register_encoder(get_encoder_plugin_openjpeg());
#endif

#if HAVE_OPENJPEG_DECODER
  register_decoder(get_decoder_plugin_openjpeg());
#endif

#if HAVE_OPENJPH_ENCODER
  register_encoder(get_encoder_plugin_openjph());
#endif

#if HAVE_OpenH264_DECODER
  register_decoder(get_decoder_plugin_openh264());
#endif

#if HAVE_X264
  register_encoder(get_encoder_plugin_x264());
#endif

#if WITH_UNCOMPRESSED_CODEC
  register_encoder(get_encoder_plugin_uncompressed());
  register_decoder(get_decoder_plugin_uncompressed());
#endif

  register_encoder(get_encoder_plugin_mask());
}


void register_decoder(const heif_decoder_plugin* decoder_plugin)
{
  if (decoder_plugin->init_plugin) {
    (*decoder_plugin->init_plugin)();
  }

  s_decoder_plugins.insert(decoder_plugin);
}


bool has_decoder(heif_compression_format type, const char* name_id)
{
  load_plugins_if_not_initialized_yet();

  for (const auto* plugin : s_decoder_plugins) {
    int priority = plugin->does_support_format(type);
    if (priority > 0 && strcmp(name_id, plugin->id_name) == 0) {
      return true;
    }
  }

  return false;
}


const heif_decoder_plugin* get_decoder(heif_compression_format type, const char* name_id)
{
  load_plugins_if_not_initialized_yet();

  int highest_priority = 0;
  const struct heif_decoder_plugin* best_plugin = nullptr;

  for (const auto* plugin : s_decoder_plugins) {

    int priority = plugin->does_support_format(type);

    if (priority > 0 && name_id && plugin->plugin_api_version >= 3) {
      if (strcmp(name_id, plugin->id_name) == 0) {
        return plugin;
      }
    }

    if (priority > highest_priority) {
      highest_priority = priority;
      best_plugin = plugin;
    }
  }

  return best_plugin;
}


void register_encoder(const heif_encoder_plugin* encoder_plugin)
{
  if (encoder_plugin->init_plugin) {
    (*encoder_plugin->init_plugin)();
  }

  auto descriptor = std::unique_ptr<heif_encoder_descriptor>(new heif_encoder_descriptor);
  descriptor->plugin = encoder_plugin;

  s_encoder_descriptors.insert(std::move(descriptor));
}


const heif_encoder_plugin* get_encoder(heif_compression_format type)
{
  auto filtered_encoder_descriptors = get_filtered_encoder_descriptors(type, nullptr);
  if (filtered_encoder_descriptors.size() > 0) {
    return filtered_encoder_descriptors[0]->plugin;
  }
  else {
    return nullptr;
  }
}


std::vector<const heif_encoder_descriptor*>
get_filtered_encoder_descriptors(heif_compression_format format,
                                 const char* name)
{
  load_plugins_if_not_initialized_yet();

  std::vector<const heif_encoder_descriptor*> filtered_descriptors;

  for (const auto& descr : s_encoder_descriptors) {
    const heif_encoder_plugin* plugin = descr->plugin;

    if (plugin->compression_format == format || format == heif_compression_undefined) {
      if (name == nullptr || strcmp(name, plugin->id_name) == 0) {
        filtered_descriptors.push_back(descr.get());
      }
    }
  }


  // Note: since our std::set<> is ordered by priority, we do not have to sort our output

  return filtered_descriptors;
}


void heif_unregister_decoder_plugins()
{
  for (const auto* plugin : s_decoder_plugins) {
    if (plugin->deinit_plugin) {
      (*plugin->deinit_plugin)();
    }
  }
  s_decoder_plugins.clear();
}

void heif_unregister_encoder_plugins()
{
  for (const auto& plugin : s_encoder_descriptors) {
    if (plugin->plugin->cleanup_plugin) {
      (*plugin->plugin->cleanup_plugin)();
    }
  }
  s_encoder_descriptors.clear();
}

#if ENABLE_PLUGIN_LOADING
void heif_unregister_encoder_plugin(const heif_encoder_plugin* plugin)
{
  if (plugin->cleanup_plugin) {
    (*plugin->cleanup_plugin)();
  }

  for (auto iter = s_encoder_descriptors.begin() ; iter != s_encoder_descriptors.end(); ++iter) {
    if ((*iter)->plugin == plugin) {
      s_encoder_descriptors.erase(iter);
      return;
    }
  }
}
#endif
