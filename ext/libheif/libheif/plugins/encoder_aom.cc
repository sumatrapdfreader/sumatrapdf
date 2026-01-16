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

#include "libheif/heif.h"
#include "libheif/heif_plugin.h"
#include "common_utils.h"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <memory>
#include <utility>
#include "encoder_aom.h"

#include <deque>
#include <aom/aom_encoder.h>
#include <aom/aomcx.h>
#include <mutex>

// Detect whether the aom_codec_set_option() function is available.
// See https://aomedia.googlesource.com/aom/+/c1d42fe6615c96fc929257ed53c41fa094f38836%5E%21/aom/aom_codec.h.
#if AOM_CODEC_ABI_VERSION >= (6 + AOM_IMAGE_ABI_VERSION)
#define HAVE_AOM_CODEC_SET_OPTION 1
#endif

#if defined(HAVE_AOM_CODEC_SET_OPTION)
struct custom_option
{
    std::string name;
    std::string value;
};
#endif

struct encoder_struct_aom
{
  ~encoder_struct_aom()
  {
    for (auto* error : aom_errors) {
      delete[] error;
    }

    // automatically destroy aom_codec_ctx_t when we leave the function
//    auto codec_ctx_deleter = std::unique_ptr<aom_codec_ctx_t, aom_codec_err_t (*)(aom_codec_ctx_t*)>(&codec, aom_codec_destroy);

    aom_codec_destroy(&codec);
  }

  aom_codec_ctx_t codec;

  // --- parameters

  bool realtime_mode;
  int cpu_used;  // = parameter 'speed'. I guess this is a better name than 'cpu_used'.

  int quality;
  int alpha_quality;
  int min_q;
  int max_q;
  int alpha_min_q;
  int alpha_max_q;
  int threads;
  bool lossless;
  bool lossless_alpha;
  bool auto_tiles;
  bool enable_intra_block_copy;

#if defined(HAVE_AOM_CODEC_SET_OPTION)
  std::vector<custom_option> custom_options;

  void add_custom_option(const custom_option&);

  void add_custom_option(std::string name, std::string value);
#endif

  aom_tune_metric tune;

  heif_chroma chroma = heif_chroma_420;

  // --- input

  bool alpha_quality_set = false;
  bool alpha_min_q_set = false;
  bool alpha_max_q_set = false;

  // --- output

  struct Packet
  {
    std::vector<uint8_t> compressedData;
    uintptr_t frameNr = 0;
    bool is_keyframe = false;
  };

  std::deque<Packet> output_packets;
  std::vector<uint8_t> active_output_data;

  //bool data_read = false;

  // --- error message copies

  std::mutex aom_errors_mutex;
  std::vector<const char*> aom_errors;

  const char* set_aom_error(const char* aom_error_detail);
};

#if defined(HAVE_AOM_CODEC_SET_OPTION)

void encoder_struct_aom::add_custom_option(const custom_option& p)
{
  // if there is already a parameter of that name, remove it from list

  for (auto iter = custom_options.begin(); iter != custom_options.end(); ++iter) {
    if (iter->name == p.name) {
      custom_options.erase(iter);
      break;
    }
  }

  // and add the new parameter at the end of the list

  custom_options.push_back(p);
}

void encoder_struct_aom::add_custom_option(std::string name, std::string value)
{
  custom_option p;
  p.name = std::move(name);
  p.value = std::move(value);
  add_custom_option(p);
}

#endif

static const char* kError_undefined_error = "Undefined AOM error";
static const char* kError_codec_enc_config_default = "Error creating the default encoder config";

const char* encoder_struct_aom::set_aom_error(const char* aom_error)
{
  if (aom_error) {
    // We have to make a copy because the error returned from aom_codec_error_detail() is only valid
    // while the codec structure exists.

    char* err_copy = new char[strlen(aom_error) + 1];
    strcpy(err_copy, aom_error);

    std::lock_guard<std::mutex> lock(aom_errors_mutex);
    aom_errors.push_back(err_copy);

    return err_copy;
  }
  else {
    return kError_undefined_error;
  }
}

static const char* kParam_min_q = "min-q";
static const char* kParam_max_q = "max-q";
static const char* kParam_alpha_quality = "alpha-quality";
static const char* kParam_alpha_min_q = "alpha-min-q";
static const char* kParam_alpha_max_q = "alpha-max-q";
static const char* kParam_lossless_alpha = "lossless-alpha";
static const char* kParam_auto_tiles = "auto-tiles";
static const char* kParam_enable_intra_block_copy = "enable-intrabc";
static const char* kParam_threads = "threads";
static const char* kParam_realtime = "realtime";
static const char* kParam_speed = "speed";

static const char* kParam_chroma = "chroma";
static const char* const kParam_chroma_valid_values[] = {
    "420", "422", "444", nullptr
};

static const char* kParam_tune = "tune";
static const char* const kParam_tune_valid_values[] = {
    "psnr", "ssim", "iq", nullptr
};

static const int AOM_PLUGIN_PRIORITY = 60;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void aom_set_default_parameters(void* encoder);


static const char* aom_plugin_name()
{
  const char* encoder_name = aom_codec_iface_name(aom_codec_av1_cx());
  if (strlen(encoder_name) < MAX_PLUGIN_NAME_LENGTH) {
    strcpy(plugin_name, encoder_name);
  }
  else {
    strcpy(plugin_name, "AOMedia AV1 encoder");
  }

  return plugin_name;
}


#define MAX_NPARAMETERS 16

static heif_encoder_parameter aom_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* aom_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void aom_init_parameters()
{
  heif_encoder_parameter* p = aom_encoder_params;
  const heif_encoder_parameter** d = aom_encoder_parameter_ptrs;
  int i = 0;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_realtime;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_speed;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 6;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  if (aom_codec_version_major() >= 3) {
    p->integer.maximum = 9;
  }
  else {
    p->integer.maximum = 8;
  }
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_threads;
  p->type = heif_encoder_parameter_type_integer;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 1;
  p->integer.maximum = 64;
  int threads = static_cast<int>(std::thread::hardware_concurrency());
  if (threads == 0) {
    // Could not autodetect, use previous default value.
    threads = 4;
  }
  threads = std::min(threads, p->integer.maximum);
  p->integer.default_value = threads;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = heif_encoder_parameter_name_quality;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 50;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 100;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = heif_encoder_parameter_name_lossless;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_chroma;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "420";
  p->has_default = true;
  p->string.valid_values = kParam_chroma_valid_values;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_tune;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "ssim";
  p->has_default = true;
  p->string.valid_values = kParam_tune_valid_values;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_min_q;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 0;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 63;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_max_q;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 63;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 63;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_alpha_quality;
  p->type = heif_encoder_parameter_type_integer;
  p->has_default = false;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 100;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_alpha_min_q;
  p->type = heif_encoder_parameter_type_integer;
  p->has_default = false;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 63;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_alpha_max_q;
  p->type = heif_encoder_parameter_type_integer;
  p->has_default = false;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 63;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_lossless_alpha;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_auto_tiles;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_enable_intra_block_copy;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = true;
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS + 1);
  d[i++] = nullptr;
}


const heif_encoder_parameter** aom_list_parameters(void* encoder)
{
  return aom_encoder_parameter_ptrs;
}

static void aom_init_plugin()
{
  aom_init_parameters();
}


static void aom_cleanup_plugin()
{
}

heif_error aom_new_encoder(void** enc)
{
  encoder_struct_aom* encoder = new encoder_struct_aom();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  aom_set_default_parameters(encoder);

  return err;
}

void aom_free_encoder(void* encoder_raw)
{
  struct encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  delete encoder;
}


heif_error aom_set_parameter_quality(void* encoder_raw, int quality)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

heif_error aom_get_parameter_quality(void* encoder_raw, int* quality)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

heif_error aom_set_parameter_lossless(void* encoder_raw, int enable)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (enable) {
    encoder->min_q = 0;
    encoder->max_q = 0;
    encoder->alpha_min_q = 0;
    encoder->alpha_min_q_set = true;
    encoder->alpha_max_q = 0;
    encoder->alpha_max_q_set = true;
  }

  encoder->lossless = enable;

  return heif_error_ok;
}

heif_error aom_get_parameter_lossless(void* encoder_raw, int* enable)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  *enable = encoder->lossless;

  return heif_error_ok;
}

struct heif_error aom_set_parameter_logging_level(void* encoder_raw, int logging)
{
#if 0
  struct encoder_struct_x265* encoder = (struct encoder_struct_x265*)encoder_raw;

  if (logging<0 || logging>4) {
    return heif_error_invalid_parameter_value;
  }

  encoder->logLevel = logging;
#endif

  return heif_error_ok;
}

struct heif_error aom_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
#if 0
  struct encoder_struct_x265* encoder = (struct encoder_struct_x265*)encoder_raw;

  *loglevel = encoder->logLevel;
#else
  *loglevel = 0;
#endif

  return heif_error_ok;
}

#define set_value(paramname, paramvar) if (strcmp(name, paramname)==0) { encoder->paramvar = value; return heif_error_ok; }
#define get_value(paramname, paramvar) if (strcmp(name, paramname)==0) { *value = encoder->paramvar; return heif_error_ok; }


heif_error aom_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return aom_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return aom_set_parameter_lossless(encoder, value);
  }
  else if (strcmp(name, kParam_alpha_quality) == 0) {
      if (value < 0 || value > 100) {
          return heif_error_invalid_parameter_value;
      }

      encoder->alpha_quality = value;
      encoder->alpha_quality_set = true;
      return heif_error_ok;
  }
  else if (strcmp(name, kParam_alpha_min_q) == 0) {
      encoder->alpha_min_q = value;
      encoder->alpha_min_q_set = true;
      return heif_error_ok;
  }
  else if (strcmp(name, kParam_alpha_max_q) == 0) {
      encoder->alpha_max_q = value;
      encoder->alpha_max_q_set = true;
      return heif_error_ok;
  }

  set_value(kParam_min_q, min_q);
  set_value(kParam_max_q, max_q);
  set_value(kParam_threads, threads);
  set_value(kParam_speed, cpu_used);

  return heif_error_unsupported_parameter;
}

heif_error aom_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return aom_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return aom_get_parameter_lossless(encoder, value);
  }
  else if (strcmp(name, kParam_alpha_quality) == 0) {
      *value = encoder->alpha_quality_set ? encoder->alpha_quality : encoder->quality;
      return heif_error_ok;
  }
  else if (strcmp(name, kParam_alpha_max_q) == 0) {
      *value = encoder->alpha_max_q_set ? encoder->alpha_max_q : encoder->max_q;
      return heif_error_ok;
  }
  else if (strcmp(name, kParam_alpha_min_q) == 0) {
      *value = encoder->alpha_min_q_set ? encoder->alpha_min_q : encoder->min_q;
      return heif_error_ok;
  }

  get_value(kParam_min_q, min_q);
  get_value(kParam_max_q, max_q);
  get_value(kParam_threads, threads);
  get_value(kParam_speed, cpu_used);

  return heif_error_unsupported_parameter;
}


heif_error aom_set_parameter_boolean(void* encoder_raw, const char* name, int value)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return aom_set_parameter_lossless(encoder, value);
  }
  else if (strcmp(name, kParam_lossless_alpha) == 0) {
      encoder->lossless_alpha = value;
      if (value) {
          encoder->alpha_max_q = 0;
          encoder->alpha_max_q_set = true;
          encoder->alpha_min_q = 0;
          encoder->alpha_min_q_set = true;
      }
      return heif_error_ok;
  } else if (strcmp(name, kParam_auto_tiles) == 0) {
      encoder->auto_tiles = value;
      return heif_error_ok;
  } else if (strcmp(name, kParam_enable_intra_block_copy) == 0) {
      encoder->enable_intra_block_copy = value;
      return heif_error_ok;
  }

  set_value(kParam_realtime, realtime_mode);

  return heif_error_unsupported_parameter;
}

heif_error aom_get_parameter_boolean(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return aom_get_parameter_lossless(encoder, value);
  }

  get_value(kParam_realtime, realtime_mode);
  get_value(kParam_lossless_alpha, lossless_alpha);
  get_value(kParam_auto_tiles, auto_tiles);
  get_value(kParam_enable_intra_block_copy, enable_intra_block_copy);

  return heif_error_unsupported_parameter;
}


heif_error aom_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (strcmp(name, kParam_chroma) == 0) {
    if (strcmp(value, "420") == 0) {
      encoder->chroma = heif_chroma_420;
      return heif_error_ok;
    }
    else if (strcmp(value, "422") == 0) {
      encoder->chroma = heif_chroma_422;
      return heif_error_ok;
    }
    else if (strcmp(value, "444") == 0) {
      encoder->chroma = heif_chroma_444;
      return heif_error_ok;
    }
    else {
      return heif_error_invalid_parameter_value;
    }
  }

  if (strcmp(name, kParam_tune) == 0) {
    if (strcmp(value, "psnr") == 0) {
      encoder->tune = AOM_TUNE_PSNR;
      return heif_error_ok;
    }
    else if (strcmp(value, "ssim") == 0) {
      encoder->tune = AOM_TUNE_SSIM;
      return heif_error_ok;
    }
#if defined(AOM_HAVE_TUNE_IQ)
    else if (strcmp(value, "iq") == 0) {
      encoder->tune = AOM_TUNE_IQ;
      return heif_error_ok;
    }
#endif
    else {
      return heif_error_invalid_parameter_value;
    }
  }

#if defined(HAVE_AOM_CODEC_SET_OPTION)
  if (strncmp(name, "aom:", 4) == 0) {
    encoder->add_custom_option(std::string(name).substr(4), std::string(value));
    return heif_error_ok;
  }
#endif

  return heif_error_unsupported_parameter;
}


static void save_strcpy(char* dst, int dst_size, const char* src)
{
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = 0;
}


heif_error aom_get_parameter_string(void* encoder_raw, const char* name,
                                    char* value, int value_size)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (strcmp(name, kParam_chroma) == 0) {
    switch (encoder->chroma) {
      case heif_chroma_420:
        save_strcpy(value, value_size, "420");
        break;
      case heif_chroma_422:
        save_strcpy(value, value_size, "422");
        break;
      case heif_chroma_444:
        save_strcpy(value, value_size, "444");
        break;
      default:
        assert(false);
        return heif_error_invalid_parameter_value;
    }
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_tune) == 0) {
    switch (encoder->tune) {
      case AOM_TUNE_PSNR:
        save_strcpy(value, value_size, "psnr");
        break;
      case AOM_TUNE_SSIM:
        save_strcpy(value, value_size, "ssim");
        break;
#if defined(AOM_HAVE_TUNE_IQ)
      case AOM_TUNE_IQ:
        save_strcpy(value, value_size, "iq");
        break;
#endif
      default:
        assert(false);
        return heif_error_invalid_parameter_value;
    }
    return heif_error_ok;
  }

  return heif_error_unsupported_parameter;
}


static void aom_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = aom_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          aom_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          aom_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          aom_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


void aom_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
{
  *colorspace = heif_colorspace_YCbCr;
  *chroma = heif_chroma_420;
}


void aom_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  if (*colorspace == heif_colorspace_monochrome) {
    // keep the monochrome colorspace
  }
  else {
    *colorspace = heif_colorspace_YCbCr;
    *chroma = encoder->chroma;
  }
}

// returns 'true' when an error was detected
// Note: some older AOM versions take a non-const pointer to aom_codec_error(). Thus, we also have to use a non-const pointer here.
static bool check_aom_error(aom_codec_err_t aom_error, /*const*/ aom_codec_ctx_t* codec, encoder_struct_aom* encoder, struct heif_error* heif_error)
{
  if (aom_error == AOM_CODEC_OK) {
    return false;
  }

  std::stringstream sstr;
  sstr << "AOM encoder error: " << aom_codec_error(codec) << " - " << aom_codec_error_detail(codec);

  heif_error->code = heif_error_Encoder_plugin_error;
  heif_error->subcode = heif_suberror_Unsupported_parameter;
  heif_error->message = encoder->set_aom_error(sstr.str().c_str());

  return true;
}

#define CHECK_ERROR \
if (check_aom_error(aom_error, &codec, encoder, &err)) { \
  return err; \
}

struct chroma_info
{
  aom_img_fmt_t img_format = AOM_IMG_FMT_NONE;
  int chroma_height = 0;
  int chroma_sample_position = AOM_CSP_UNKNOWN;
};


chroma_info get_chroma_info(heif_chroma chroma,
                            int bpp_y, int source_height)
{
  chroma_info info;

  switch (chroma) {
    case heif_chroma_420:
    case heif_chroma_monochrome:
      info.img_format = AOM_IMG_FMT_I420;
      info.chroma_height = (source_height+1)/2;
      info.chroma_sample_position = AOM_CSP_UNKNOWN; // TODO: change this to CSP_CENTER in the future (https://github.com/AOMediaCodec/av1-avif/issues/88)
      break;
    case heif_chroma_422:
      info.img_format = AOM_IMG_FMT_I422;
      info.chroma_height = (source_height+1)/2;
      info.chroma_sample_position = AOM_CSP_COLOCATED;
      break;
    case heif_chroma_444:
      info.img_format = AOM_IMG_FMT_I444;
      info.chroma_height = source_height;
      info.chroma_sample_position = AOM_CSP_COLOCATED;
      break;
    default:
      info.img_format = AOM_IMG_FMT_NONE;
      info.chroma_sample_position = AOM_CSP_UNKNOWN;
      assert(false);
      break;
  }

  if (bpp_y > 8) {
    info.img_format = (aom_img_fmt_t) (info.img_format | AOM_IMG_FMT_HIGHBITDEPTH);
  }

  return info;
}



static heif_error aom_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                                     enum heif_image_input_class input_class,
                                                     uint32_t framerate_num, uint32_t framerate_denom,
                                                     const heif_sequence_encoding_options* options,
                                                     bool image_sequence)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  heif_error err;

  const int source_width = heif_image_get_width(image, heif_channel_Y);
  const int source_height = heif_image_get_height(image, heif_channel_Y);

  const heif_chroma chroma = heif_image_get_chroma_format(image);

  int bpp_y = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);


  // --- check for AOM 3.6.0 bug

  bool is_aom_3_6_0 = (aom_codec_version() == 0x030600);

  if (is_aom_3_6_0) {
    // This bound might be too tight, as I still could encode images with 8193 x 4353 correctly. Even 8200x4400, but 8200x4800 fails.
    // Let's still keep it as most images will be smaller anyway.
    if (!(source_width <= 8192 * 2 && source_height <= 4352 * 2 && source_width * source_height <= 8192 * 4352)) {
      err = {heif_error_Encoding_error,
             heif_suberror_Encoder_encoding,
             "AOM v3.6.0 has a bug when encoding large images. Please upgrade to at least AOM v3.6.1."};
      return err;
    }
  }


  // --- copy libheif image to aom image

  chroma_info chroma_info = get_chroma_info(chroma, bpp_y, source_height);


  // --- configure codec

  aom_codec_iface_t* iface;
  aom_codec_ctx_t& codec = encoder->codec;

  iface = aom_codec_av1_cx();
  //encoder->encoder = get_aom_encoder_by_name("av1");
  if (!iface) {
    return {
      heif_error_Unsupported_feature,
      heif_suberror_Unsupported_codec,
      "Unsupported codec: AOMedia Project AV1 Encoder"
    };
  }


#if defined(AOM_USAGE_ALL_INTRA)
  // aom 3.1.0
  unsigned int aomUsage = AOM_USAGE_ALL_INTRA;
#else
  // aom 2.0
  unsigned int aomUsage = AOM_USAGE_GOOD_QUALITY;
#endif

  if (image_sequence &&
      options->gop_structure != heif_sequence_gop_structure_intra_only &&
      options->keyframe_distance_max != 1) {
    aomUsage = AOM_USAGE_GOOD_QUALITY;
  }

  if (encoder->realtime_mode) {
    aomUsage = AOM_USAGE_REALTIME;
  }

  aom_codec_enc_cfg_t cfg;
  aom_codec_err_t res = aom_codec_enc_config_default(iface, &cfg, aomUsage);
  if (res) {
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Encoder_initialization,
           kError_codec_enc_config_default};
    return err;
  }

  int seq_profile = compute_avif_profile(heif_image_get_bits_per_pixel_range(image, heif_channel_Y),
                                         heif_image_get_chroma_format(image));

  cfg.g_w = source_width;
  cfg.g_h = source_height;
  // Set the max number of frames to encode to 1. This makes the libaom encoder
  // set still_picture and reduced_still_picture_header to 1 in the AV1 sequence
  // header OBU.
  if (!image_sequence) {
    cfg.g_limit = 1;
  }

  // Use the default settings of the new AOM_USAGE_ALL_INTRA (added in
  // https://crbug.com/aomedia/2959).
  //
  // Set g_lag_in_frames to 0 to reduce the number of frame buffers (from 20
  // to 2) in libaom's lookahead structure. This reduces memory consumption when
  // encoding a single image.
  cfg.g_lag_in_frames = 0;
  // Disable automatic placement of key frames by the encoder.
  cfg.kf_mode = AOM_KF_DISABLED;

  if (!image_sequence) {
    // Tell libaom that all frames will be key frames.
    cfg.kf_max_dist = 0;
  }
  else if (options->gop_structure == heif_sequence_gop_structure_intra_only) {
    cfg.kf_max_dist = 0;
  }
  else {
    if (options->keyframe_distance_min) {
      cfg.kf_min_dist = options->keyframe_distance_min;
    }

    if (options->keyframe_distance_max) {
      cfg.kf_max_dist = options->keyframe_distance_max;
    }
  }

  cfg.g_profile = seq_profile;
  cfg.g_bit_depth = (aom_bit_depth_t) bpp_y;
  cfg.g_input_bit_depth = bpp_y;

  cfg.rc_end_usage = AOM_Q;

  int min_q = encoder->min_q;
  int max_q = encoder->max_q;

  if (input_class == heif_image_input_class_alpha && encoder->alpha_min_q_set && encoder->alpha_max_q_set) {
    min_q = encoder->alpha_min_q;
    max_q = encoder->alpha_max_q;
  }

  int quality = encoder->quality;

  if (input_class == heif_image_input_class_alpha && encoder->alpha_quality_set) {
    quality = encoder->alpha_quality;
  }

  int cq_level = ((100 - quality) * 63 + 50) / 100;

  // Work around the bug in libaom v2.0.2 or older fixed by
  // https://aomedia-review.googlesource.com/c/aom/+/113064. If using a libaom
  // release with the bug, set cfg.rc_min_quantizer to cq_level to prevent
  // libaom from incorrectly using a quantizer index lower than cq_level.
  bool aom_2_0_2_or_older = aom_codec_version() <= 0x020002;

  cfg.rc_min_quantizer = aom_2_0_2_or_older ? cq_level : min_q;
  cfg.rc_max_quantizer = max_q;
  cfg.g_error_resilient = 0;
  cfg.g_threads = encoder->threads;

  if (chroma == heif_chroma_monochrome) {
    cfg.monochrome = 1;
  }

  cfg.g_timebase.num = static_cast<int>(framerate_num);
  cfg.g_timebase.den = static_cast<int>(framerate_denom);

  // --- initialize codec

  aom_codec_flags_t encoder_flags = 0;
  if (bpp_y > 8) {
    encoder_flags = (aom_codec_flags_t) (encoder_flags | AOM_CODEC_USE_HIGHBITDEPTH);
  }

  // allocate aom_codec_ctx_t
  if (aom_codec_enc_init(&codec, iface, &cfg, encoder_flags)) {
    // AOM makes sure that the error text returned by aom_codec_error_detail() is always a static
    // text that is valid even though the codec allocation failed (#788).
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Encoder_initialization,
           encoder->set_aom_error(aom_codec_error_detail(&codec))};
    return err;
  }

  aom_codec_err_t aom_error;

  aom_error = aom_codec_control(&codec, AOME_SET_CPUUSED, encoder->cpu_used); CHECK_ERROR;

  aom_error = aom_codec_control(&codec, AOME_SET_CQ_LEVEL, cq_level); CHECK_ERROR;

  if (encoder->threads > 1) {
#if defined(AOM_CTRL_AV1E_SET_ROW_MT)
    // aom 2.0
    aom_error = aom_codec_control(&codec, AV1E_SET_ROW_MT, 1); CHECK_ERROR;
#endif
  }

#if defined(AOM_CTRL_AV1E_SET_AUTO_TILES)
  // aom 3.10.0
  aom_error = aom_codec_control(&codec, AV1E_SET_AUTO_TILES, encoder->auto_tiles); CHECK_ERROR;
#endif

  // TODO: set AV1E_SET_TILE_ROWS and AV1E_SET_TILE_COLUMNS.


  heif_color_profile_nclx* nclx = nullptr;
  err = heif_image_get_nclx_color_profile(image, &nclx);
  if (err.code != heif_error_Ok) {
    assert(nclx == nullptr);
  }

  // make sure NCLX profile is deleted at end of function
  auto nclx_deleter = std::unique_ptr<heif_color_profile_nclx, void (*)(heif_color_profile_nclx*)>(nclx, heif_nclx_color_profile_free);

  // In aom, color_range defaults to limited range (0). Set it to full range (1).
  aom_error = aom_codec_control(&codec, AV1E_SET_COLOR_RANGE, nclx ? nclx->full_range_flag : 1); CHECK_ERROR;
  aom_error = aom_codec_control(&codec, AV1E_SET_CHROMA_SAMPLE_POSITION, chroma_info.chroma_sample_position); CHECK_ERROR;

  if (nclx &&
      (input_class == heif_image_input_class_normal ||
       input_class == heif_image_input_class_thumbnail)) {
    aom_error = aom_codec_control(&codec, AV1E_SET_COLOR_PRIMARIES, nclx->color_primaries); CHECK_ERROR
    aom_error = aom_codec_control(&codec, AV1E_SET_MATRIX_COEFFICIENTS, nclx->matrix_coefficients); CHECK_ERROR;
    aom_error = aom_codec_control(&codec, AV1E_SET_TRANSFER_CHARACTERISTICS, nclx->transfer_characteristics); CHECK_ERROR;
       }

  aom_error = aom_codec_control(&codec, AOME_SET_TUNING, encoder->tune); CHECK_ERROR;

  if (encoder->lossless || (input_class == heif_image_input_class_alpha && encoder->lossless_alpha)) {
    aom_error = aom_codec_control(&codec, AV1E_SET_LOSSLESS, 1); CHECK_ERROR;
  }

#if defined(AOM_CTRL_AV1E_SET_SKIP_POSTPROC_FILTERING)
  if (cfg.g_usage == AOM_USAGE_ALL_INTRA) {
    // Enable AV1E_SET_SKIP_POSTPROC_FILTERING for still-picture encoding,
    // which is disabled by default.
    aom_error = aom_codec_control(&codec, AV1E_SET_SKIP_POSTPROC_FILTERING, 1); CHECK_ERROR;
  }
#endif

  aom_error = aom_codec_control(&codec, AV1E_SET_ENABLE_INTRABC, encoder->enable_intra_block_copy); CHECK_ERROR;

#if defined(HAVE_AOM_CODEC_SET_OPTION)
  // Apply the custom AOM encoder options.
  // These should always be applied last as they can override the values that were set above.
  for (const auto& p : encoder->custom_options) {
    if (aom_codec_set_option(&codec, p.name.c_str(), p.value.c_str()) != AOM_CODEC_OK) {
      std::stringstream sstr;
      sstr << "Cannot set AOM encoder option (name: " << p.name << ", value: " << p.value << "): "
           << aom_codec_error(&codec) << " - " << aom_codec_error_detail(&codec);

      err = {
        heif_error_Encoder_plugin_error,
        heif_suberror_Unsupported_parameter,
        encoder->set_aom_error(sstr.str().c_str())
      };
      return err;
    }
  }
#endif

  return {};
}



static heif_error aom_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                       enum heif_image_input_class input_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options)
{
  return aom_start_sequence_encoding_intern(encoder_raw, image, input_class, framerate_num, framerate_denom, options,
    true);
}


static heif_error aom_encode_sequence_frame(void* encoder_raw, const heif_image* image,
                                            uintptr_t frame_nr)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;
  aom_codec_ctx_t& codec = encoder->codec;

  heif_error err;

  const int source_width = heif_image_get_width(image, heif_channel_Y);
  const int source_height = heif_image_get_height(image, heif_channel_Y);

  const heif_chroma chroma = heif_image_get_chroma_format(image);

  int bpp_y = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

  chroma_info chroma_info = get_chroma_info(chroma, bpp_y, source_height);

  std::unique_ptr<aom_image_t, void (*)(aom_image_t*)> input_image(aom_img_alloc(nullptr,
                                                                                 chroma_info.img_format,
                                                                                 source_width,
                                                                                 source_height,
                                                                                 1),
                                                                   aom_img_free);
  if (!input_image) {
    err = {heif_error_Memory_allocation_error,
           heif_suberror_Unspecified,
           "Failed to allocate image"};
    return err;
  }


  for (int plane = 0; plane < 3; plane++) {
    unsigned char* buf = input_image->planes[plane];
    const int stride = input_image->stride[plane];

    if (chroma == heif_chroma_monochrome && plane != 0) {
      if (bpp_y == 8) {
        memset(buf, 128, chroma_info.chroma_height * stride);
      }
      else {
        uint16_t* buf16 = (uint16_t*) buf;
        uint16_t half_range = (uint16_t) (1 << (bpp_y - 1));
        for (int i = 0; i < chroma_info.chroma_height * stride / 2; i++) {
          buf16[i] = half_range;
        }
      }

      continue;
    }

    /*
    const int w = aom_img_plane_width(img, plane) *
                  ((img->fmt & AOM_IMG_FMT_HIGHBITDEPTH) ? 2 : 1);
    const int h = aom_img_plane_height(img, plane);
    */

    size_t in_stride = 0;
    const uint8_t* in_p = heif_image_get_plane_readonly2(image, (heif_channel) plane, &in_stride);

    int w = source_width;
    int h = source_height;

    if (plane != 0) {
      if (chroma != heif_chroma_444) { w = (w + 1) / 2; }
      if (chroma == heif_chroma_420) { h = (h + 1) / 2; }

      assert(w == heif_image_get_width(image, (heif_channel) plane));
      assert(h == heif_image_get_height(image, (heif_channel) plane));
    }

    if (bpp_y > 8) {
      w *= 2;
    }

    for (int y = 0; y < h; y++) {
      memcpy(buf, &in_p[y * in_stride], w);
      buf += stride;
    }
  }

  //input_image->user_priv = (void*)frame_nr;

  // --- encode frame

  aom_codec_err_t res = aom_codec_encode(&codec, input_image.get(),
                                         frame_nr, // PTS (only encoding a single frame) TODO
                                         1,
                                         0); // no flags

  if (res != AOM_CODEC_OK) {
    err = {
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        encoder->set_aom_error(aom_codec_error_detail(&codec))
    };
    return err;
  }

  // TODO: do we need this ? encoder->compressedData.clear();

  const aom_codec_cx_pkt_t* pkt = NULL;
  aom_codec_iter_t iter = NULL; // for extracting the compressed packets

  while ((pkt = aom_codec_get_cx_data(&codec, &iter)) != NULL) {

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      //std::cerr.write((char*)pkt->data.frame.buf, pkt->data.frame.sz);

      //printf("packet of size: %d\n",(int)pkt->data.frame.sz);


      // TODO: split the received data into separate OBUs
      // This allows libheif to easily extract the sequence header for the av1C header

      size_t n = pkt->data.frame.sz;

      encoder_struct_aom::Packet output_packet;
      output_packet.frameNr = pkt->data.frame.pts;
      output_packet.is_keyframe = (pkt->data.frame.flags & AOM_FRAME_IS_INTRAONLY);

      encoder->output_packets.emplace_back(output_packet);
      encoder->output_packets.back().compressedData.resize(n);

      memcpy(encoder->output_packets.back().compressedData.data(),
             pkt->data.frame.buf,
             n);
    }
  }


  return heif_error_ok;
}


static heif_error aom_end_sequence_encoding(void *encoder_raw)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;
  aom_codec_ctx_t& codec = encoder->codec;

  heif_error err;

  int flags = 0;
  aom_codec_err_t res = aom_codec_encode(&codec, NULL, -1, 0, flags);
  if (res != AOM_CODEC_OK) {
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Encoder_encoding,
           encoder->set_aom_error(aom_codec_error_detail(&codec))};
    return err;
  }


  aom_codec_iter_t iter = NULL; // for extracting the compressed packets

  const aom_codec_cx_pkt_t* pkt = NULL;
  while ((pkt = aom_codec_get_cx_data(&codec, &iter)) != NULL) {

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      //std::cerr.write((char*)pkt->data.frame.buf, pkt->data.frame.sz);

      //printf("packet of size: %d\n",(int)pkt->data.frame.sz);


      // TODO: split the received data into separate OBUs
      // This allows libheif to easily extract the sequence header for the av1C header

      size_t n = pkt->data.frame.sz;

      encoder_struct_aom::Packet output_packet;
      output_packet.frameNr = pkt->data.frame.pts;

      encoder->output_packets.emplace_back(output_packet);
      encoder->output_packets.back().compressedData.resize(n);

      memcpy(encoder->output_packets.back().compressedData.data(),
             pkt->data.frame.buf,
             n);
    }
  }

  return {};
}


static heif_error aom_encode_image(void* encoder_raw, const heif_image* image,
                                   heif_image_input_class input_class)
{
  heif_error err;
  err = aom_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = aom_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  return aom_end_sequence_encoding(encoder_raw);
}


heif_error aom_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                    uintptr_t* out_framenr, int* out_is_keyframe,
                                    int* more_frame_packets)
{
  encoder_struct_aom* encoder = (encoder_struct_aom*) encoder_raw;

  encoder->active_output_data.clear();

  if (encoder->output_packets.empty()) {
    *size = 0;
    *data = nullptr;
  }
  else {
    encoder->active_output_data = std::move(encoder->output_packets.front().compressedData);
    if (out_framenr) {
      *out_framenr = encoder->output_packets.front().frameNr;
    }

    if (out_is_keyframe) {
      *out_is_keyframe = encoder->output_packets.front().is_keyframe;
    }

    encoder->output_packets.pop_front();

    *size = (int) encoder->active_output_data.size();
    *data = encoder->active_output_data.data();
  }

  return heif_error_ok;
}


static heif_error aom_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                          heif_encoded_data_type* type)
{
  return aom_get_compressed_data2(encoder_raw, data, size, nullptr, nullptr, nullptr);
}


static const heif_encoder_plugin encoder_plugin_aom
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_AV1,
        /* id_name */ "aom",
        /* priority */ AOM_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ aom_plugin_name,
        /* init_plugin */ aom_init_plugin,
        /* cleanup_plugin */ aom_cleanup_plugin,
        /* new_encoder */ aom_new_encoder,
        /* free_encoder */ aom_free_encoder,
        /* set_parameter_quality */ aom_set_parameter_quality,
        /* get_parameter_quality */ aom_get_parameter_quality,
        /* set_parameter_lossless */ aom_set_parameter_lossless,
        /* get_parameter_lossless */ aom_get_parameter_lossless,
        /* set_parameter_logging_level */ aom_set_parameter_logging_level,
        /* get_parameter_logging_level */ aom_get_parameter_logging_level,
        /* list_parameters */ aom_list_parameters,
        /* set_parameter_integer */ aom_set_parameter_integer,
        /* get_parameter_integer */ aom_get_parameter_integer,
        /* set_parameter_boolean */ aom_set_parameter_boolean,
        /* get_parameter_boolean */ aom_get_parameter_boolean,
        /* set_parameter_string */ aom_set_parameter_string,
        /* get_parameter_string */ aom_get_parameter_string,
        /* query_input_colorspace */ aom_query_input_colorspace,
        /* encode_image */ aom_encode_image,
        /* get_compressed_data */ aom_get_compressed_data,
        /* query_input_colorspace (v2) */ aom_query_input_colorspace2,
        /* query_encoded_size (v3) */ nullptr,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ aom_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ aom_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ aom_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ aom_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 1
    };

const heif_encoder_plugin* get_encoder_plugin_aom()
{
  return &encoder_plugin_aom;
}


#if PLUGIN_AOM_ENCODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_aom
};
#endif
