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
#include "encoder_x265.h"
#include <memory>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <deque>
#include <vector>
#include <utility>

extern "C" {
#include <x265.h>
}


static const char* kError_unsupported_bit_depth = "Bit depth not supported by x265";
static const char* kError_unsupported_image_size = "Images smaller than 16 pixels are not supported";
static const char* kError_unsupported_ctu_size = "Unsupported CTU size";


enum parameter_type
{
  UndefinedType, Int, Bool, String
};

struct parameter
{
  parameter_type type = UndefinedType;
  std::string name;

  int value_int = 0; // also used for boolean
  std::string value_string;
};


struct encoder_struct_x265
{
  x265_encoder* encoder = nullptr;
  const x265_api* api = nullptr;
  x265_param* param = nullptr;

  int bit_depth = 0;

  heif_chroma chroma;


  // --- output

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t frameNr = 0;
  };

  std::deque<Packet> output_data;
  std::vector<uint8_t> active_output_nal;

  void append_nal_packets(x265_nal* nals, uint32_t num_nals, uintptr_t frameNr);

  // --- parameters

  std::vector<parameter> parameters;

  void add_param(const parameter&);

  void add_param(const std::string& name, int value);

  void add_param(const std::string& name, bool value);

  void add_param(const std::string& name, const std::string& value);

  parameter get_param(const std::string& name) const;

  std::string preset;
  std::string tune;

  int logLevel = X265_LOG_NONE;

  std::string last_error_message;
};


void encoder_struct_x265::add_param(const parameter& p)
{
  // if there is already a parameter of that name, remove it from list

  for (size_t i = 0; i < parameters.size(); i++) {
    if (parameters[i].name == p.name) {
      for (size_t k = i + 1; k < parameters.size(); k++) {
        parameters[k - 1] = parameters[k];
      }
      parameters.pop_back();
      break;
    }
  }

  // and add the new parameter at the end of the list

  parameters.push_back(p);
}


void encoder_struct_x265::add_param(const std::string& name, int value)
{
  parameter p;
  p.type = Int;
  p.name = name;
  p.value_int = value;
  add_param(p);
}

void encoder_struct_x265::add_param(const std::string& name, bool value)
{
  parameter p;
  p.type = Bool;
  p.name = name;
  p.value_int = value;
  add_param(p);
}

void encoder_struct_x265::add_param(const std::string& name, const std::string& value)
{
  parameter p;
  p.type = String;
  p.name = name;
  p.value_string = value;
  add_param(p);
}


parameter encoder_struct_x265::get_param(const std::string& name) const
{
  for (size_t i = 0; i < parameters.size(); i++) {
    if (parameters[i].name == name) {
      return parameters[i];
    }
  }

  return parameter();
}


static const char* kParam_preset = "preset";
static const char* kParam_tune = "tune";
static const char* kParam_TU_intra_depth = "tu-intra-depth";
static const char* kParam_complexity = "complexity";

static const char* const kParam_preset_valid_values[] = {
    "ultrafast", "superfast", "veryfast", "faster", "fast", "medium",
    "slow", "slower", "veryslow", "placebo", nullptr
};

static const char* const kParam_tune_valid_values[] = {
    "psnr", "ssim", "grain", "fastdecode", nullptr
    // note: zerolatency is missing, because we do not need it for single images
};

static const char* kParam_chroma = "chroma";
static const char* const kParam_chroma_valid_values[] = {
    "420", "422", "444", nullptr
};


static const int X265_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void x265_set_default_parameters(void* encoder);


static const char* x265_plugin_name()
{
  strcpy(plugin_name, "x265 HEVC encoder");

  const x265_api* api = x265_api_get(0);
  const char* x265_version = ((api != nullptr && api->version_str != nullptr) ? api->version_str : "null");

  if (strlen(x265_version) + strlen(plugin_name) + 4 < MAX_PLUGIN_NAME_LENGTH) {
    strcat(plugin_name, " (");
    strcat(plugin_name, x265_version);
    strcat(plugin_name, ")");
  }

  return plugin_name;
}


#define MAX_NPARAMETERS 10

static heif_encoder_parameter x265_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* x265_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void x265_init_parameters()
{
  heif_encoder_parameter* p = x265_encoder_params;
  const heif_encoder_parameter** d = x265_encoder_parameter_ptrs;
  int i = 0;

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
  p->name = kParam_preset;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "slow";  // increases computation time
  p->has_default = true;
  p->string.valid_values = kParam_preset_valid_values;
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
  p->name = kParam_TU_intra_depth;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 2;  // increases computation time
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 1;
  p->integer.maximum = 4;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_complexity;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 50;
  p->has_default = false;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 100;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_chroma;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "420";
  p->has_default = true;
  p->string.valid_values = kParam_chroma_valid_values;
  d[i++] = p++;

  d[i++] = nullptr;
}


const heif_encoder_parameter** x265_list_parameters(void* encoder)
{
  return x265_encoder_parameter_ptrs;
}


static void x265_init_plugin()
{
  x265_init_parameters();
}


static void x265_cleanup_plugin()
{
  x265_cleanup();
}


static heif_error x265_new_encoder(void** enc)
{
  encoder_struct_x265* encoder = new encoder_struct_x265();
  heif_error err = heif_error_ok;


  // encoder has to be allocated in x265_encode_image, because it needs to know the image size
  encoder->encoder = nullptr;

  encoder->bit_depth = 8;

  *enc = encoder;


  // set default parameters

  x265_set_default_parameters(encoder);

  return err;
}

static void x265_free_encoder(void* encoder_raw)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (encoder->encoder) {
    const x265_api* api = x265_api_get(encoder->bit_depth);
    api->encoder_close(encoder->encoder);
  }

  delete encoder;
}

static heif_error x265_set_parameter_quality(void* encoder_raw, int quality)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->add_param(heif_encoder_parameter_name_quality, quality);

  return heif_error_ok;
}

static heif_error x265_get_parameter_quality(void* encoder_raw, int* quality)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  parameter p = encoder->get_param(heif_encoder_parameter_name_quality);
  *quality = p.value_int;

  return heif_error_ok;
}

static heif_error x265_set_parameter_lossless(void* encoder_raw, int enable)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  encoder->add_param(heif_encoder_parameter_name_lossless, (bool) enable);

  return heif_error_ok;
}

static heif_error x265_get_parameter_lossless(void* encoder_raw, int* enable)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  parameter p = encoder->get_param(heif_encoder_parameter_name_lossless);
  *enable = p.value_int;

  return heif_error_ok;
}

static heif_error x265_set_parameter_logging_level(void* encoder_raw, int logging)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (logging < 0 || logging > 4) {
    return heif_error_invalid_parameter_value;
  }

  encoder->logLevel = logging;

  return heif_error_ok;
}

static heif_error x265_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  *loglevel = encoder->logLevel;

  return heif_error_ok;
}


static heif_error x265_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return x265_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return x265_set_parameter_lossless(encoder, value);
  }
  else if (strcmp(name, kParam_TU_intra_depth) == 0) {
    if (value < 1 || value > 4) {
      return heif_error_invalid_parameter_value;
    }

    encoder->add_param(name, value);
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_complexity) == 0) {
    if (value < 0 || value > 100) {
      return heif_error_invalid_parameter_value;
    }

    encoder->add_param(name, value);
    return heif_error_ok;
  }

  return heif_error_unsupported_parameter;
}

static heif_error x265_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return x265_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return x265_get_parameter_lossless(encoder, value);
  }
  else if (strcmp(name, kParam_TU_intra_depth) == 0) {
    *value = encoder->get_param(name).value_int;
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_complexity) == 0) {
    *value = encoder->get_param(name).value_int;
    return heif_error_ok;
  }

  return heif_error_unsupported_parameter;
}


static heif_error x265_set_parameter_boolean(void* encoder, const char* name, int value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return x265_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

// Unused, will use "x265_get_parameter_integer" instead.
/*
static struct heif_error x265_get_parameter_boolean(void* encoder, const char* name, int* value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless)==0) {
    return x265_get_parameter_lossless(encoder,value);
  }

  return heif_error_unsupported_parameter;
}
*/


static bool string_list_contains(const char* const* values_list, const char* value)
{
  for (int i = 0; values_list[i]; i++) {
    if (strcmp(values_list[i], value) == 0) {
      return true;
    }
  }

  return false;
}


static heif_error x265_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (strcmp(name, kParam_preset) == 0) {
    if (!string_list_contains(kParam_preset_valid_values, value)) {
      return heif_error_invalid_parameter_value;
    }

    encoder->preset = value;
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_tune) == 0) {
    if (!string_list_contains(kParam_tune_valid_values, value)) {
      return heif_error_invalid_parameter_value;
    }

    encoder->tune = value;
    return heif_error_ok;
  }
  else if (strncmp(name, "x265:", 5) == 0) {
    encoder->add_param(name, std::string(value));
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_chroma) == 0) {
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

  return heif_error_unsupported_parameter;
}

static void save_strcpy(char* dst, int dst_size, const char* src)
{
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = 0;
}

static heif_error x265_get_parameter_string(void* encoder_raw, const char* name,
                                            char* value, int value_size)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (strcmp(name, kParam_preset) == 0) {
    save_strcpy(value, value_size, encoder->preset.c_str());
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_tune) == 0) {
    save_strcpy(value, value_size, encoder->tune.c_str());
    return heif_error_ok;
  }
  else if (strcmp(name, kParam_chroma) == 0) {
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

  return heif_error_unsupported_parameter;
}


static void x265_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = x265_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          x265_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          x265_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          x265_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


static void x265_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
{
  if (*colorspace == heif_colorspace_monochrome) {
    *colorspace = heif_colorspace_monochrome;
    *chroma = heif_chroma_monochrome;
  }
  else {
    *colorspace = heif_colorspace_YCbCr;
    *chroma = heif_chroma_420;
  }
}


static void x265_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  auto* encoder = (encoder_struct_x265*) encoder_raw;

  if (*colorspace == heif_colorspace_monochrome) {
    *colorspace = heif_colorspace_monochrome;
    *chroma = heif_chroma_monochrome;
  }
  else {
    *colorspace = heif_colorspace_YCbCr;
    *chroma = encoder->chroma;
  }
}


static int rounded_size(int s)
{
  s = (s + 1) & ~1;

  if (s < 64) {
    s = 64;
  }

  return s;
}

#if 0
static const char* naltype_table[] = {
  /*  0 */ "TRAIL_N",
  /*  1 */ "TRAIL_R",
  /*  2 */ "TSA_N",
  /*  3 */ "TSA_R",
  /*  4 */ "STSA_N",
  /*  5 */ "STSA_R",
  /*  6 */ "RADL_N",
  /*  7 */ "RADL_R",
  /*  8 */ "RASL_N",
  /*  9 */ "RASL_R",
  /* 10 */ "RSV_VCL_N10",
  /* 11 */ "RSV_VCL_R11",
  /* 12 */ "RSV_VCL_N12",
  /* 13 */ "RSV_VCL_R13",
  /* 14 */ "RSV_VCL_N14",
  /* 15 */ "RSV_VCL_R15",
  /* 16 */ "BLA_W_LP",
  /* 17 */ "BLA_W_RADL",
  /* 18 */ "BLA_N_LP",
  /* 19 */ "IDR_W_RADL",
  /* 20 */ "IDR_N_LP",
  /* 21 */ "CRA_NUT",
  /* 22 */ "RSV_IRAP_VCL22",
  /* 23 */ "RSV_IRAP_VCL23",
  /* 24 */ "RSV_VCL24",
  /* 25 */ "RSV_VCL25",
  /* 26 */ "RSV_VCL26",
  /* 27 */ "RSV_VCL27",
  /* 28 */ "RSV_VCL28",
  /* 29 */ "RSV_VCL29",
  /* 30 */ "RSV_VCL30",
  /* 31 */ "RSV_VCL31",
  /* 32 */ "VPS_NUT",
  /* 33 */ "SPS_NUT",
  /* 34 */ "PPS_NUT",
  /* 35 */ "AUD_NUT",
  /* 36 */ "EOS_NUT",
  /* 37 */ "EOB_NUT",
  /* 38 */ "FD_NUT",
  /* 39 */ "PREFIX_SEI_NUT",
  /* 40 */ "SUFFIX_SEI_NUT"
};


static const char* naltype(uint8_t type)
{
  if (type <= 40) {
    return naltype_table[type];
  }
  else {
    return "reserved";
  }
}
#endif

void encoder_struct_x265::append_nal_packets(x265_nal* nals, uint32_t num_nals, uintptr_t frameNr)
{
  for (uint32_t nal_idx=0 ; nal_idx < num_nals; nal_idx++) {
    uint8_t* data = nals[nal_idx].payload;
    uint32_t size = nals[nal_idx].sizeBytes;

    // --- skip start code ---

    // skip '0' bytes
    while (*data == 0 && size > 0) {
      data++;
      size--;
    }

    // skip '1' byte
    data++;
    size--;


    // --- skip NALs with irrelevant data ---

    if (size >= 3 && data[0] == 0x4e && data[2] == 5) {
      // skip "unregistered user data SEI"
    }
    else {
      // output NAL

      Packet pkt;
      pkt.data.resize(size);
      memcpy(pkt.data.data(), data, size);
      pkt.frameNr = frameNr;

      output_data.push_back(pkt);
    }
  }
}


static heif_error x265_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                       enum heif_image_input_class input_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options,
                                       bool image_sequence)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;


    // close previous encoder if there is still one hanging around
  if (encoder->encoder) {
    const x265_api* api = x265_api_get(encoder->bit_depth);
    api->encoder_close(encoder->encoder);
    encoder->encoder = nullptr;
  }


  int bit_depth = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);
  heif_chroma chroma = heif_image_get_chroma_format(image);

  const x265_api* api = x265_api_get(bit_depth);
  encoder->api = api;
  if (api == nullptr) {
    struct heif_error err = {
        heif_error_Encoder_plugin_error,
        heif_suberror_Unsupported_bit_depth,
        kError_unsupported_bit_depth
    };
    return err;
  }

  x265_param* param = api->param_alloc();
  if (encoder->param) {
    api->param_free(encoder->param);
  }
  encoder->param = param;

  api->param_default_preset(param, encoder->preset.c_str(), encoder->tune.c_str());

  if (bit_depth == 8) {
    if (image_sequence) {
      api->param_apply_profile(param, "main");
    }
    else {
      api->param_apply_profile(param, "mainstillpicture");
    }
  }
  else if (bit_depth == 10) api->param_apply_profile(param, "main10-intra");
  else if (bit_depth == 12) api->param_apply_profile(param, "main12-intra");
  else {
    api->param_free(param);
    return heif_error_unsupported_parameter;
  }


  // x265 cannot encode images smaller than one CTU size
  // https://bitbucket.org/multicoreware/x265/issues/475/x265-does-not-allow-image-sizes-smaller
  // -> use smaller CTU sizes for very small images
  const char* ctu = NULL;
  int ctuSize = 64;

#if 1
  while (ctuSize > 16 &&
         (heif_image_get_width(image, heif_channel_Y) < ctuSize ||
          heif_image_get_height(image, heif_channel_Y) < ctuSize)) {
    ctuSize /= 2;
  }

  if (ctuSize < 16) {
    api->param_free(param);
    return {
        heif_error_Encoder_plugin_error,
        heif_suberror_Invalid_parameter_value,
        kError_unsupported_image_size
    };
  }
#else
  // TODO: There seems to be a bug in x265 where increasing the CTU size between
  // multiple encoding jobs causes a segmentation fault. E.g. encoding multiple
  // times with a CTU of 16 works, the next encoding with a CTU of 32 crashes.
  // Use hardcoded value of 64 and reject images that are too small.

  if (heif_image_get_width(image, heif_channel_Y) < ctuSize ||
      heif_image_get_height(image, heif_channel_Y) < ctuSize) {
    api->param_free(param);
    struct heif_error err = {
      heif_error_Encoder_plugin_error,
      heif_suberror_Invalid_parameter_value,
      kError_unsupported_image_size
    };
    return err;
  }
#endif

  // ctuSize should be a power of 2 in [16;64]
  switch (ctuSize) {
    case 64:
      ctu = "64";
      break;
    case 32:
      ctu = "32";
      break;
    case 16:
      ctu = "16";
      break;
    default:
      return {
          heif_error_Encoder_plugin_error,
          heif_suberror_Invalid_parameter_value,
          kError_unsupported_ctu_size
      };
  }

  if (options) {
    if (options->keyframe_distance_min) {
      param->keyframeMin = options->keyframe_distance_min;
    }

    if (options->keyframe_distance_max) {
      param->keyframeMax = options->keyframe_distance_max;
    }

    switch (options->gop_structure) {
      case heif_sequence_gop_structure_intra_only:
        param->bframes = 0;
        param->keyframeMin=1;
        param->keyframeMax=1;
        break;
      case heif_sequence_gop_structure_lowdelay:
        param->bframes = 0;
        break;
      case heif_sequence_gop_structure_unrestricted:
        param->bframes = 1;
        break;
    }
  }

  // BPG uses CQP. It does not seem to be better though.
  //  param->rc.rateControlMode = X265_RC_CQP;
  //  param->rc.qp = (100 - encoder->quality)/2;
  param->totalFrames = image_sequence ? 0 : 1; // TODO

  if (isGreyscale) {
    param->internalCsp = X265_CSP_I400;
  }
  else if (chroma == heif_chroma_420) {
    param->internalCsp = X265_CSP_I420;
  }
  else if (chroma == heif_chroma_422) {
    param->internalCsp = X265_CSP_I422;
  }
  else if (chroma == heif_chroma_444) {
    param->internalCsp = X265_CSP_I444;
  }

  if (chroma != heif_chroma_monochrome) {
    int w = heif_image_get_width(image, heif_channel_Y);
    int h = heif_image_get_height(image, heif_channel_Y);
    if (chroma != heif_chroma_444) { w = (w + 1) / 2; }
    if (chroma == heif_chroma_420) { h = (h + 1) / 2; }

    assert(heif_image_get_width(image, heif_channel_Cb)==w);
    assert(heif_image_get_width(image, heif_channel_Cr)==w);
    assert(heif_image_get_height(image, heif_channel_Cb)==h);
    assert(heif_image_get_height(image, heif_channel_Cr)==h);
    (void) w;
    (void) h;
  }

  api->param_parse(param, "info", "0");
  api->param_parse(param, "limit-modes", "0");
  api->param_parse(param, "limit-refs", "0");
  api->param_parse(param, "ctu", ctu);
  api->param_parse(param, "rskip", "0");

  api->param_parse(param, "rect", "1");
  api->param_parse(param, "amp", "1");
  api->param_parse(param, "aq-mode", "1");
  api->param_parse(param, "psy-rd", "1.0");
  api->param_parse(param, "psy-rdoq", "1.0");

  heif_color_profile_nclx* nclx = nullptr;
  heif_error err = heif_image_get_nclx_color_profile(image, &nclx);
  if (err.code != heif_error_Ok) {
    assert(nclx == nullptr);
  }

  // make sure NCLX profile is deleted at end of function
  auto nclx_deleter = std::unique_ptr<heif_color_profile_nclx, void (*)(heif_color_profile_nclx*)>(nclx, heif_nclx_color_profile_free);

  if (nclx) {
    api->param_parse(param, "range", nclx->full_range_flag ? "full" : "limited");
  }
  else {
    api->param_parse(param, "range", "full");
  }

  if (nclx &&
      (input_class == heif_image_input_class_normal ||
       input_class == heif_image_input_class_thumbnail)) {

    {
      std::stringstream sstr;
      sstr << nclx->color_primaries;
      api->param_parse(param, "colorprim", sstr.str().c_str());
    }

    {
      std::stringstream sstr;
      sstr << nclx->transfer_characteristics;
      api->param_parse(param, "transfer", sstr.str().c_str());
    }

    {
      std::stringstream sstr;
      sstr << nclx->matrix_coefficients;
      api->param_parse(param, "colormatrix", sstr.str().c_str());
    }
  }

  for (const auto& p : encoder->parameters) {
    if (p.name == heif_encoder_parameter_name_quality) {
      // quality=0   -> crf=50
      // quality=50  -> crf=25
      // quality=100 -> crf=0

      param->rc.rfConstant = (100 - p.value_int) / 2.0;
    }
    else if (p.name == heif_encoder_parameter_name_lossless) {
      param->bLossless = p.value_int;
    }
    else if (p.name == kParam_TU_intra_depth) {
      auto valString = std::to_string(p.value_int);
      api->param_parse(param, "tu-intra-depth", valString.c_str());
    }
    else if (p.name == kParam_complexity) {
      const int complexity = p.value_int;

      if (complexity >= 60) {
        api->param_parse(param, "rd-refine", "1"); // increases computation time
        api->param_parse(param, "rd", "6");
      }

      if (complexity >= 70) {
        api->param_parse(param, "cu-lossless", "1"); // increases computation time
      }

      if (complexity >= 90) {
        api->param_parse(param, "wpp", "0"); // setting to 0 significantly increases computation time
      }
    }
    else if (strncmp(p.name.c_str(), "x265:", 5) == 0) {
      std::string x265p = p.name.substr(5);
      if (api->param_parse(param, x265p.c_str(), p.value_string.c_str()) < 0) {
        encoder->last_error_message = std::string{"Unsupported x265 encoder parameter: "} + x265p;

        return {
          .code = heif_error_Usage_error,
          .subcode = heif_suberror_Unsupported_parameter,
          .message = encoder->last_error_message.c_str()
        };
      }
    }
  }

  param->logLevel = encoder->logLevel;


  param->sourceWidth = heif_image_get_width(image, heif_channel_Y);
  param->sourceHeight = heif_image_get_height(image, heif_channel_Y);
  param->internalBitDepth = bit_depth;

  param->sourceWidth = rounded_size(param->sourceWidth);
  param->sourceHeight = rounded_size(param->sourceHeight);

  param->fpsNum = framerate_num;
  param->fpsDenom = framerate_denom;

  encoder->bit_depth = bit_depth;

  encoder->encoder = api->encoder_open(param);

  if (image_sequence) {
    x265_nal* nals = nullptr;
    uint32_t num_nals = 0;

    api->encoder_headers(encoder->encoder,
                         &nals,
                         &num_nals);

    encoder->append_nal_packets(nals, num_nals, 0);

    for (uint32_t i = 0; i < num_nals; i++) {
      //std::cout << "dequeue header NAL : " << naltype(encoder->nals[i].type) << "\n";
    }
  }

  return heif_error_ok;
}


static heif_error x265_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                       enum heif_image_input_class input_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options)
{
  return x265_start_sequence_encoding_intern(encoder_raw, image, input_class,
                                             framerate_num, framerate_denom, options, true);
}


static heif_error x265_encode_sequence_frame(void* encoder_raw, const heif_image* image,
                                             uintptr_t frame_nr)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  if (!encoder->api) {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "called plugin encode_sequence_frame() without start_sequence_encoding()"
    };
  }

  const x265_api* api = encoder->api;

  heif_error err;

  // Note: it is ok to cast away the const, as the image content is not changed.
  // However, we have to guarantee that there are no plane pointers or stride values kept over calling the svt_encode_image() function.
  err = heif_image_extend_padding_to_size(const_cast<heif_image*>(image),
                                          encoder->param->sourceWidth,
                                          encoder->param->sourceHeight);
  if (err.code) {
    return err;
  }

  x265_picture* pic = api->picture_alloc();
  api->picture_init(encoder->param, pic);

  // TODO: check that all input images use the same color format

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);

  if (isGreyscale) {
    pic->planes[0] = (void*) heif_image_get_plane_readonly(image, heif_channel_Y, &pic->stride[0]);
  }
  else {
    pic->planes[0] = (void*) heif_image_get_plane_readonly(image, heif_channel_Y, &pic->stride[0]);
    pic->planes[1] = (void*) heif_image_get_plane_readonly(image, heif_channel_Cb, &pic->stride[1]);
    pic->planes[2] = (void*) heif_image_get_plane_readonly(image, heif_channel_Cr, &pic->stride[2]);
  }

  pic->bitDepth = encoder->bit_depth;
  pic->userData = reinterpret_cast<void*>(frame_nr);

  x265_nal* nals = nullptr;
  uint32_t num_nals = 0;

#if X265_BUILD == 212
  // In x265 build version 212, the signature of the encoder_encode() function was changed. But it was changed back in version 213.
  // https://bitbucket.org/multicoreware/x265_git/issues/952/crash-in-libheif-tests
  x265_picture* out_pic = NULL;
  api->encoder_encode(encoder->encoder,
                      &nals,
                      &num_nals,
                      pic,
                      &out_pic);
  uintptr_t frameNr = reinterpret_cast<uintptr_t>(out_pic->userData);
#else
  x265_picture out_pic;
  api->encoder_encode(encoder->encoder,
                      &nals,
                      &num_nals,
                      pic,
                      &out_pic);

  uintptr_t frameNr = reinterpret_cast<uintptr_t>(out_pic.userData);
  for (uint32_t i = 0; i < num_nals; i++) {
    //std::cout << " dequeue frame " << encoder->out_frameNr << ": " << naltype(encoder->nals[i].type) << "\n";
  }

  encoder->append_nal_packets(nals, num_nals, frameNr);

#endif

  api->picture_free(pic);

  return heif_error_ok;
}


static heif_error x265_end_sequence_encoding(void* encoder_raw)
{
  encoder_struct_x265* encoder = (encoder_struct_x265*) encoder_raw;

  const x265_api* api = encoder->api;

  x265_nal* nals = nullptr;
  uint32_t num_nals = 0;

#if X265_BUILD == 212
  x265_picture* out_pic = NULL;
  int result = api->encoder_encode(encoder->encoder,
                                   &nals,
                                   &num_nals,
                                   NULL,
                                   &out_pic);
  uintptr_t frameNr = reinterpret_cast<uintptr_t>(out_pic->userData);
#else
  x265_picture out_pic;
  int result = api->encoder_encode(encoder->encoder,
                                   &nals,
                                   &num_nals,
                                   NULL,
                                   &out_pic);
  uintptr_t frameNr = reinterpret_cast<uintptr_t>(out_pic.userData);

  for (uint32_t i = 0; i < num_nals; i++) {
    //std::cout << "EOS flush, frame " << encoder->out_frameNr << ": " << naltype(encoder->nals[i].type) << "\n";
  }
#endif
  if (result <= 0) {
    // TODO: do we need this ?
    //*data = nullptr;
    //*size = 0;

    return heif_error_ok; // ?
  }

  encoder->append_nal_packets(nals, num_nals, frameNr);

  encoder->api->param_free(encoder->param);
  encoder->param = nullptr;

  return heif_error_ok;
}


static heif_error x265_encode_image(void* encoder_raw, const heif_image* image,
                                    heif_image_input_class input_class)
{
  heif_error err;
  err = x265_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = x265_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  x265_end_sequence_encoding(encoder_raw);

  return heif_error_ok;
}


static heif_error x265_get_compressed_data_intern(void* encoder_raw, uint8_t** data, int* size,
                                                  uintptr_t* out_frame_nr)
{
  encoder_struct_x265* encoder = ( encoder_struct_x265*) encoder_raw;

  if (encoder->output_data.empty()) {
    *data = nullptr;
    *size = 0;

    return heif_error_ok;
  }

  encoder->active_output_nal = std::move(encoder->output_data.front().data);

  if (out_frame_nr) {
    *out_frame_nr = encoder->output_data.front().frameNr;
  }

  encoder->output_data.pop_front();

  *data = encoder->active_output_nal.data();
  *size = static_cast<int>(encoder->active_output_nal.size());

  return heif_error_ok;
}


static heif_error x265_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                           heif_encoded_data_type* type)
{
  return x265_get_compressed_data_intern(encoder_raw, data, size, nullptr);
}

static heif_error x265_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                            uintptr_t* frame_nr, int* is_keyframe,
                                            int* more_frame_packets)
{
  return x265_get_compressed_data_intern(encoder_raw, data, size, frame_nr);
}


static const heif_encoder_plugin encoder_plugin_x265
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_HEVC,
        /* id_name */ "x265",
        /* priority */ X265_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ x265_plugin_name,
        /* init_plugin */ x265_init_plugin,
        /* cleanup_plugin */ x265_cleanup_plugin,
        /* new_encoder */ x265_new_encoder,
        /* free_encoder */ x265_free_encoder,
        /* set_parameter_quality */ x265_set_parameter_quality,
        /* get_parameter_quality */ x265_get_parameter_quality,
        /* set_parameter_lossless */ x265_set_parameter_lossless,
        /* get_parameter_lossless */ x265_get_parameter_lossless,
        /* set_parameter_logging_level */ x265_set_parameter_logging_level,
        /* get_parameter_logging_level */ x265_get_parameter_logging_level,
        /* list_parameters */ x265_list_parameters,
        /* set_parameter_integer */ x265_set_parameter_integer,
        /* get_parameter_integer */ x265_get_parameter_integer,
        /* set_parameter_boolean */ x265_set_parameter_integer, // boolean also maps to integer function
        /* get_parameter_boolean */ x265_get_parameter_integer, // boolean also maps to integer function
        /* set_parameter_string */ x265_set_parameter_string,
        /* get_parameter_string */ x265_get_parameter_string,
        /* query_input_colorspace */ x265_query_input_colorspace,
        /* encode_image */ x265_encode_image,
        /* get_compressed_data */ x265_get_compressed_data,
        /* query_input_colorspace (v2) */ x265_query_input_colorspace2,
        /* query_encoded_size (v3) */ nullptr,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ x265_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ x265_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ x265_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ x265_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 0
    };

const heif_encoder_plugin* get_encoder_plugin_x265()
{
  return &encoder_plugin_x265;
}


#if PLUGIN_X265
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_x265
};
#endif
