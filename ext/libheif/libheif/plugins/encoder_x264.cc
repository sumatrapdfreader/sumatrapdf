/*
 * HEIF codec.
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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
#include "encoder_x264.h"
#include <memory>
#include <sstream>
#include <string>
#include <cstring>
#include <cassert>
#include <deque>
#include <vector>

#include "logging.h"
#include <utility>

extern "C" {
#include <x264.h>
}

#if 0
static const char* naltype_table[] = {
  /*  0 */ "unspecified",
  /*  1 */ "non-IDR",
  /*  2 */ "partition A",
  /*  3 */ "partition B",
  /*  4 */ "partition C",
  /*  5 */ "IDR",
  /*  6 */ "SEI",
  /*  7 */ "SPS",
  /*  8 */ "PPS",
  /*  9 */ "AU-delimiter",
  /* 10 */ "EOSequence",
  /* 11 */ "EOStream",
  /* 12 */ "FillerData",
  /* 13 */ "SPS-extension",
  /* 14 */ "Prefix-NAL",
  /* 15 */ "Subset-SPS",
  /* 16 */ "reserved",
  /* 17 */ "reserved",
  /* 18 */ "reserved",
  /* 19 */ "aux-coded-picture",
  /* 20 */ "slice-in-scalable-extension"
};


static const char* naltype(uint8_t type)
{
  if (type <= 20) {
    return naltype_table[type];
  }
  else {
    return "reserved";
  }
}
#endif


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


struct encoder_struct_x264
{
  x264_t* encoder = nullptr;
  x264_param_t param{};

  uintptr_t out_frameNr = 0;
  int bit_depth = 0;

  heif_chroma chroma;


  // --- parameters

  std::vector<parameter> parameters;

  void add_param(const parameter&);

  void add_param(const std::string& name, int value);

  void add_param(const std::string& name, bool value);

  void add_param(const std::string& name, const std::string& value);

  parameter get_param(const std::string& name) const;

  std::string preset;
  std::string tune;

  int logLevel = X264_LOG_NONE;

  // --- output

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t frameNr = 0;
    bool more_packets = false;
  };

  std::deque<Packet> m_output_packets;
  std::vector<uint8_t> m_active_output;

  void append_nals(const x264_nal_t* nals, int num_nals, uintptr_t frameNr);

  std::string last_error_message;
};


void encoder_struct_x264::append_nals(const x264_nal_t* nals, int num_nals, uintptr_t frameNr)
{
#if 0
  std::cout << "append " << num_nals << " NALs for frame " << frameNr << "\n";

  for (int i=0;i<num_nals;i++) {
    std::cout << "dequeue header NAL : " << naltype(nals[i].i_type) << "\n";
  }
#endif

  for (int i=0;i<num_nals;i++) {
    const auto& nal = nals[i];

    Packet pkt;
    pkt.data.insert(pkt.data.end(), nal.p_payload, nal.p_payload + nal.i_payload);
    pkt.more_packets = true; // below, it will be set to 'false' for the last packet
    pkt.frameNr = frameNr;

    m_output_packets.emplace_back(std::move(pkt));
  }

  m_output_packets.back().more_packets = false;
}


void encoder_struct_x264::add_param(const parameter& p)
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


void encoder_struct_x264::add_param(const std::string& name, int value)
{
  parameter p;
  p.type = Int;
  p.name = name;
  p.value_int = value;
  add_param(p);
}

void encoder_struct_x264::add_param(const std::string& name, bool value)
{
  parameter p;
  p.type = Bool;
  p.name = name;
  p.value_int = value;
  add_param(p);
}

void encoder_struct_x264::add_param(const std::string& name, const std::string& value)
{
  parameter p;
  p.type = String;
  p.name = name;
  p.value_string = value;
  add_param(p);
}


parameter encoder_struct_x264::get_param(const std::string& name) const
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


static const int X264_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void x264_set_default_parameters(void* encoder);


static const char* x264_plugin_name()
{
  strcpy(plugin_name, "x264 AVC encoder");

  const char* x264_version = X264_VERSION;

  if (strlen(x264_version) + strlen(plugin_name) + 4 < MAX_PLUGIN_NAME_LENGTH) {
    strcat(plugin_name, " (");
    strcat(plugin_name, x264_version);
    strcat(plugin_name, ")");
  }

  return plugin_name;
}


#define MAX_NPARAMETERS 10

static heif_encoder_parameter x264_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* x264_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void x264_init_parameters()
{
  heif_encoder_parameter* p = x264_encoder_params;
  const heif_encoder_parameter** d = x264_encoder_parameter_ptrs;
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


const heif_encoder_parameter** x264_list_parameters(void* encoder)
{
  return x264_encoder_parameter_ptrs;
}


static void x264_init_plugin()
{
  x264_init_parameters();
}


static void x264_cleanup_plugin()
{
}


static heif_error x264_new_encoder(void** enc)
{
  encoder_struct_x264* encoder = new encoder_struct_x264();
  heif_error err = heif_error_ok;


  // encoder has to be allocated in x264_encode_image, because it needs to know the image size
  encoder->encoder = nullptr;

  encoder->bit_depth = 8;

  *enc = encoder;


  // set default parameters

  x264_set_default_parameters(encoder);

  return err;
}

static void x264_free_encoder(void* encoder_raw)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  if (encoder->encoder) {
    x264_encoder_close(encoder->encoder);
    encoder->encoder = nullptr;
  }

  delete encoder;
}

static heif_error x264_set_parameter_quality(void* encoder_raw, int quality)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->add_param(heif_encoder_parameter_name_quality, quality);

  return heif_error_ok;
}

static heif_error x264_get_parameter_quality(void* encoder_raw, int* quality)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  parameter p = encoder->get_param(heif_encoder_parameter_name_quality);
  *quality = p.value_int;

  return heif_error_ok;
}

static heif_error x264_set_parameter_lossless(void* encoder_raw, int enable)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  encoder->add_param(heif_encoder_parameter_name_lossless, (bool) enable);

  return heif_error_ok;
}

static heif_error x264_get_parameter_lossless(void* encoder_raw, int* enable)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  parameter p = encoder->get_param(heif_encoder_parameter_name_lossless);
  *enable = p.value_int;

  return heif_error_ok;
}

static heif_error x264_set_parameter_logging_level(void* encoder_raw, int logging)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  if (logging < 0 || logging > 4) {
    return heif_error_invalid_parameter_value;
  }

  encoder->logLevel = logging;

  return heif_error_ok;
}

static heif_error x264_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  *loglevel = encoder->logLevel;

  return heif_error_ok;
}


static heif_error x264_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return x264_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return x264_set_parameter_lossless(encoder, value);
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

static heif_error x264_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return x264_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return x264_get_parameter_lossless(encoder, value);
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


static heif_error x264_set_parameter_boolean(void* encoder, const char* name, int value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return x264_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

// Unused, will use "x264_get_parameter_integer" instead.
/*
static struct heif_error x264_get_parameter_boolean(void* encoder, const char* name, int* value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless)==0) {
    return x264_get_parameter_lossless(encoder,value);
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


static heif_error x264_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

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
  else if (strncmp(name, "x264:", 5) == 0) {
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

static heif_error x264_get_parameter_string(void* encoder_raw, const char* name,
                                            char* value, int value_size)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

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


static void x264_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = x264_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          x264_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          x264_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          x264_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


static void x264_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
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


static void x264_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  auto* encoder = (encoder_struct_x264*) encoder_raw;

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


static heif_error x264_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                       heif_image_input_class input_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options,
                                       bool image_sequence)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;


  // close previous encoder if there is still one hanging around
  if (encoder->encoder) {
    x264_encoder_close(encoder->encoder);
    encoder->encoder = nullptr;
  }


  int bit_depth = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);
  heif_chroma chroma = heif_image_get_chroma_format(image);


  x264_param_t& param = encoder->param;

  x264_param_default_preset(&param, encoder->preset.c_str(), encoder->tune.c_str());

  if (bit_depth == 8) {
    x264_param_apply_profile(&param, "main");
  }
  else if (bit_depth == 10) x264_param_apply_profile(&param, "main10-intra");
  else if (bit_depth == 12) x264_param_apply_profile(&param, "main12-intra");
  else {
    return heif_error_unsupported_parameter;
  }


  param.i_fps_num = framerate_num;
  param.i_fps_den = framerate_denom;

  if (options) {
    if (options->keyframe_distance_min) {
      param.i_keyint_min = options->keyframe_distance_min;
    }

    if (options->keyframe_distance_max) {
      param.i_keyint_max = options->keyframe_distance_max;
    }

    switch (options->gop_structure) {
      case heif_sequence_gop_structure_intra_only:
        param.i_bframe = 0;
        param.i_keyint_min=1;
        param.i_keyint_max=1;
        break;
      case heif_sequence_gop_structure_lowdelay:
        param.i_bframe = 0;
        break;
      case heif_sequence_gop_structure_unrestricted:
        //param.i_bframe = 1;
        break;
    }
  }

  // BPG uses CQP. It does not seem to be better though.
  //  param->rc.rateControlMode = X264_RC_CQP;
  //  param->rc.qp = (100 - encoder->quality)/2;
  param.i_frame_total = image_sequence ? 0 : 1; // TODO

  if (isGreyscale) {
    param.i_csp = X264_CSP_I400;
  }
  else if (chroma == heif_chroma_420) {
    param.i_csp = X264_CSP_I420;
  }
  else if (chroma == heif_chroma_422) {
    param.i_csp = X264_CSP_I422;
  }
  else if (chroma == heif_chroma_444) {
    param.i_csp = X264_CSP_I444;
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

  // TODO: which of these exist ?
  x264_param_parse(&param, "info", "0");
  x264_param_parse(&param, "limit-modes", "0");
  x264_param_parse(&param, "limit-refs", "0");
  x264_param_parse(&param, "rskip", "0");

  x264_param_parse(&param, "rect", "1");
  x264_param_parse(&param, "amp", "1");
  x264_param_parse(&param, "aq-mode", "1");
  x264_param_parse(&param, "psy-rd", "1.0");
  x264_param_parse(&param, "psy-rdoq", "1.0");

  heif_color_profile_nclx* nclx = nullptr;
  heif_error err = heif_image_get_nclx_color_profile(image, &nclx);
  if (err.code != heif_error_Ok) {
    assert(nclx == nullptr);
  }

  // make sure NCLX profile is deleted at end of function
  auto nclx_deleter = std::unique_ptr<heif_color_profile_nclx, void (*)(heif_color_profile_nclx*)>(nclx, heif_nclx_color_profile_free);

  if (nclx) {
    x264_param_parse(&param, "range", nclx->full_range_flag ? "full" : "limited");
  }
  else {
    x264_param_parse(&param, "range", "full");
  }

  if (nclx &&
      (input_class == heif_image_input_class_normal ||
       input_class == heif_image_input_class_thumbnail)) {

    {
      std::stringstream sstr;
      sstr << nclx->color_primaries;
      x264_param_parse(&param, "colorprim", sstr.str().c_str());
    }

    {
      std::stringstream sstr;
      sstr << nclx->transfer_characteristics;
      x264_param_parse(&param, "transfer", sstr.str().c_str());
    }

    {
      std::stringstream sstr;
      sstr << nclx->matrix_coefficients;
      x264_param_parse(&param, "colormatrix", sstr.str().c_str());
    }
  }

  for (const auto& p : encoder->parameters) {
    if (p.name == heif_encoder_parameter_name_quality) {
      // quality=0   -> crf=50
      // quality=50  -> crf=25
      // quality=100 -> crf=0

      param.rc.f_rf_constant = (float)(100 - p.value_int) / 2.0f;
    }
    else if (p.name == heif_encoder_parameter_name_lossless) {
      if (p.value_int) {
        param.rc.i_qp_constant = 0;
      }
    }
    else if (p.name == kParam_complexity) {
      const int complexity = p.value_int;

      if (complexity >= 60) {
        x264_param_parse(&param, "rd-refine", "1"); // increases computation time
        x264_param_parse(&param, "rd", "6");
      }
    }
    else if (strncmp(p.name.c_str(), "x264:", 5) == 0) {
      std::string x264p = p.name.substr(5);
      if (x264_param_parse(&param, x264p.c_str(), p.value_string.c_str()) < 0) {
        encoder->last_error_message = std::string{"Unsupported x264 encoder parameter: "} + x264p;

        return {
          .code = heif_error_Usage_error,
          .subcode = heif_suberror_Unsupported_parameter,
          .message = encoder->last_error_message.c_str()
        };
      }
    }
  }

  param.i_log_level = encoder->logLevel;


  param.i_width = heif_image_get_width(image, heif_channel_Y);
  param.i_height = heif_image_get_height(image, heif_channel_Y);
  //param->internalBitDepth = bit_depth;

  param.i_width = rounded_size(param.i_width);
  param.i_height = rounded_size(param.i_height);

#if 0
  // This enforces that x264 will not queue frames.
  // Without this, it needs to be flushed 18x until it returns some NALs.
  // -> we now use x264_encoder_delayed_frames(), which works fine.

  param.b_sliced_threads = 1;
  param.i_slice_count_max = 1;
  param.i_slice_count = 1;
  param.i_slice_max_mbs = 0;
  param.i_slice_max_size = 0;
#endif

  // output NALs with size, no startcodes (actually, we don't care since we skip the 4 bytes anyways)
  param.b_annexb = 0;

  encoder->bit_depth = bit_depth;

  encoder->encoder = x264_encoder_open(&param);
  if (!encoder->encoder) {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Unspecified,
      "Cannot create x264 encoder"
    };
  }

  if (image_sequence) {
    x264_nal_t* nals = nullptr;
    int num_nals = 0;

    x264_encoder_headers(encoder->encoder,
                         &nals,
                         &num_nals);

    if (num_nals) {
      encoder->append_nals(nals, num_nals, 0);
    }
  }

  return heif_error_ok;
}


static heif_error x264_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                       enum heif_image_input_class input_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options)
{
  return x264_start_sequence_encoding_intern(encoder_raw, image, input_class,
                                             framerate_num, framerate_denom, options, true);
}


static heif_error x264_encode_sequence_frame(void* encoder_raw, const heif_image* image,
                                             uintptr_t frame_nr)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;
  x264_param_t& param = encoder->param;

  if (!encoder->encoder) {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "called plugin encode_sequence_frame() without start_sequence_encoding()"
    };
  }

  heif_error err;

  // Note: it is ok to cast away the const, as the image content is not changed.
  // However, we have to guarantee that there are no plane pointers or stride values kept over calling the svt_encode_image() function.
  err = heif_image_extend_padding_to_size(const_cast<heif_image*>(image),
                                          param.i_width,
                                          param.i_height);
  if (err.code) {
    return err;
  }

  x264_picture_t pic;
  x264_picture_init(&pic);

  pic.img.i_csp = param.i_csp;

  // TODO: check that all input images use the same color format

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);

  if (isGreyscale) {
    pic.img.plane[0] = const_cast<uint8_t*>(heif_image_get_plane_readonly(image, heif_channel_Y, &pic.img.i_stride[0]));
  }
  else {
    pic.img.plane[0] = const_cast<uint8_t*>(heif_image_get_plane_readonly(image, heif_channel_Y, &pic.img.i_stride[0]));
    pic.img.plane[1] = const_cast<uint8_t*>(heif_image_get_plane_readonly(image, heif_channel_Cb, &pic.img.i_stride[1]));
    pic.img.plane[2] = const_cast<uint8_t*>(heif_image_get_plane_readonly(image, heif_channel_Cr, &pic.img.i_stride[2]));
  }

  // pic->bitDepth = encoder->bit_depth;
  pic.i_pts = frame_nr;

  x264_nal_t* nals = nullptr;
  int num_nals = 0;

  x264_picture_t out_pic;
  int result = x264_encoder_encode(encoder->encoder,
                                   &nals,
                                   &num_nals,
                                   &pic,
                                   &out_pic);
  (void)result; // TODO: check for error

  if (num_nals) {
    encoder->append_nals(nals, num_nals, out_pic.i_pts);
  }

  return heif_error_ok;
}


static heif_error x264_end_sequence_encoding(void* encoder_raw)
{
  encoder_struct_x264* encoder = (encoder_struct_x264*) encoder_raw;

#if 0
  // TODO (hack)
  if (encoder->nal_output_counter < encoder->num_nals) {
    return heif_error_ok;
  }
#endif

  // check that all NALs have been drained
  //assert(encoder->nal_output_counter == encoder->num_nals);
  //encoder->nal_output_counter = 0;

  x264_nal_t* nals = nullptr;
  int num_nals = 0;

  while (x264_encoder_delayed_frames(encoder->encoder) > 0) {
    x264_picture_t out_pic;
    int result = x264_encoder_encode(encoder->encoder,
                                     &nals,
                                     &num_nals,
                                     nullptr,
                                     &out_pic);
    (void)result; // TODO: check for error
    encoder->out_frameNr = out_pic.i_pts;

    if (num_nals) {
      encoder->append_nals(nals, num_nals, out_pic.i_pts);
      break;
    }
  }

  x264_param_cleanup(&encoder->param);

  //encoder->nal_output_counter = 0; // TODO: is this needed ?

  return heif_error_ok;
}


static heif_error x264_encode_image(void* encoder_raw, const heif_image* image,
                                    heif_image_input_class input_class)
{
  heif_error err;
  err = x264_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = x264_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  x264_end_sequence_encoding(encoder_raw);

  return heif_error_ok;
}


static heif_error x264_get_compressed_data_intern(void* encoder_raw, uint8_t** data, int* size,
                                                  uintptr_t* out_frame_nr, int* more_packets)
{
  encoder_struct_x264* encoder = ( encoder_struct_x264*) encoder_raw;

  if (encoder->encoder == nullptr) {
    *data = nullptr;
    *size = 0;

    return heif_error_ok;
  }

  if (encoder->m_output_packets.empty()) {
    *data = nullptr;
    *size = 0;

    return heif_error_ok;
  }

  auto& pkt = encoder->m_output_packets.front();
  encoder->m_active_output = std::move(pkt.data);

  if (out_frame_nr) {
    *out_frame_nr = pkt.frameNr;
  }

  if (more_packets) {
    *more_packets = pkt.more_packets;
  }

  *data = encoder->m_active_output.data() + 4;
  *size = (int)encoder->m_active_output.size() - 4;

  encoder->m_output_packets.pop_front();

#if 0
  // const x264_api* api = x264_api_get(encoder->bit_depth);

  /*for (;;)*/ {
    while (encoder->nal_output_counter < encoder->num_nals) {
      *data = encoder->nals[encoder->nal_output_counter].p_payload;
      *size = encoder->nals[encoder->nal_output_counter].i_payload;
      encoder->nal_output_counter++;

      // --- skip start code ---

      // skip '0' bytes
      while (**data == 0 && *size > 0) {
        (*data)++;
        (*size)--;
      }

      // skip '1' byte
      (*data)++;
      (*size)--;

      const uint8_t nal_type = ((*data)[0] & 0x1f);
      std::cout << "NAL type: " << ((int)nal_type) << "\n";
      std::cout << write_raw_data_as_hex(*data, *size,
                              "data: ",
                              "      ");

      // --- skip NALs with irrelevant data ---

      if (*size >= 3 && (*data)[0] == 0x4e && (*data)[2] == 5) {
        // skip "unregistered user data SEI"

      }
      else {
        // output NAL

        if (out_frame_nr) {
          *out_frame_nr = encoder->out_frameNr;
        }

        return heif_error_ok;
      }
    }
  }

  *data = nullptr;
  *size = 0;

  if (out_frame_nr) {
    *out_frame_nr = 0;
  }
#endif

  return heif_error_ok;
}


static heif_error x264_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                           heif_encoded_data_type* type)
{
  return x264_get_compressed_data_intern(encoder_raw, data, size, nullptr, nullptr);
}

static heif_error x264_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                            uintptr_t* frame_nr, int* is_keyframe,
                                            int* more_frame_packets)
{
  return x264_get_compressed_data_intern(encoder_raw, data, size, frame_nr, more_frame_packets);
}


static const heif_encoder_plugin encoder_plugin_x264
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_AVC,
        /* id_name */ "x264",
        /* priority */ X264_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ x264_plugin_name,
        /* init_plugin */ x264_init_plugin,
        /* cleanup_plugin */ x264_cleanup_plugin,
        /* new_encoder */ x264_new_encoder,
        /* free_encoder */ x264_free_encoder,
        /* set_parameter_quality */ x264_set_parameter_quality,
        /* get_parameter_quality */ x264_get_parameter_quality,
        /* set_parameter_lossless */ x264_set_parameter_lossless,
        /* get_parameter_lossless */ x264_get_parameter_lossless,
        /* set_parameter_logging_level */ x264_set_parameter_logging_level,
        /* get_parameter_logging_level */ x264_get_parameter_logging_level,
        /* list_parameters */ x264_list_parameters,
        /* set_parameter_integer */ x264_set_parameter_integer,
        /* get_parameter_integer */ x264_get_parameter_integer,
        /* set_parameter_boolean */ x264_set_parameter_integer, // boolean also maps to integer function
        /* get_parameter_boolean */ x264_get_parameter_integer, // boolean also maps to integer function
        /* set_parameter_string */ x264_set_parameter_string,
        /* get_parameter_string */ x264_get_parameter_string,
        /* query_input_colorspace */ x264_query_input_colorspace,
        /* encode_image */ x264_encode_image,
        /* get_compressed_data */ x264_get_compressed_data,
        /* query_input_colorspace (v2) */ x264_query_input_colorspace2,
        /* query_encoded_size (v3) */ nullptr,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ x264_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ x264_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ x264_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ x264_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 0
    };

const heif_encoder_plugin* get_encoder_plugin_x264()
{
  return &encoder_plugin_x264;
}


#if PLUGIN_X264
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_x264
};
#endif
