/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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

#include "heif.h"
#include "heif_plugin.h"
#include "heif_avif.h"
#include "heif_api_structs.h"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <algorithm>
#include <cstring>
#include <cassert>
#include <vector>

#include <aom/aom_encoder.h>
#include <aom/aomcx.h>

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

  std::vector<uint8_t> compressedData;
  bool data_read = false;
};

#if defined(HAVE_AOM_CODEC_SET_OPTION)
void encoder_struct_aom::add_custom_option(const custom_option& p)
{
  // if there is already a parameter of that name, remove it from list

  for (size_t i = 0; i < custom_options.size(); i++) {
    if (custom_options[i].name == p.name) {
      for (size_t k = i + 1; k < custom_options.size(); k++) {
        custom_options[k - 1] = custom_options[k];
      }
      custom_options.pop_back();
      break;
    }
  }

  // and add the new parameter at the end of the list

  custom_options.push_back(p);
}

void encoder_struct_aom::add_custom_option(std::string name, std::string value)
{
    custom_option p;
    p.name = name;
    p.value = value;
    add_custom_option(p);
}
#endif

static const char* kError_out_of_memory = "Out of memory";
static const char* kError_encode_frame = "Failed to encode frame";

static const char* kParam_min_q = "min-q";
static const char* kParam_max_q = "max-q";
static const char* kParam_alpha_quality = "alpha-quality";
static const char* kParam_alpha_min_q = "alpha-min-q";
static const char* kParam_alpha_max_q = "alpha-max-q";
static const char* kParam_lossless_alpha = "lossless-alpha";
static const char* kParam_threads = "threads";
static const char* kParam_realtime = "realtime";
static const char* kParam_speed = "speed";

static const char* kParam_chroma = "chroma";
static const char* const kParam_chroma_valid_values[] = {
    "420", "422", "444", nullptr
};

static const char* kParam_tune = "tune";
static const char* const kParam_tune_valid_values[] = {
    "psnr", "ssim", nullptr
};

static const int AOM_PLUGIN_PRIORITY = 40;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void aom_set_default_parameters(void* encoder);


static const char* aom_plugin_name()
{
  if (strlen(aom_codec_iface_name(aom_codec_av1_cx())) < MAX_PLUGIN_NAME_LENGTH) {
    strcpy(plugin_name, aom_codec_iface_name(aom_codec_av1_cx()));
  }
  else {
    strcpy(plugin_name, "AOMedia AV1 encoder");
  }

  return plugin_name;
}


#define MAX_NPARAMETERS 14

static struct heif_encoder_parameter aom_encoder_params[MAX_NPARAMETERS];
static const struct heif_encoder_parameter* aom_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void aom_init_parameters()
{
  struct heif_encoder_parameter* p = aom_encoder_params;
  const struct heif_encoder_parameter** d = aom_encoder_parameter_ptrs;
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
  p->integer.default_value = 5;
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
  p->integer.default_value = 4;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 1;
  p->integer.maximum = 16;
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
  p->integer.default_value = 1;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 1;
  p->integer.maximum = 62;
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
  p->integer.minimum = 1;
  p->integer.maximum = 62;
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

  d[i++] = nullptr;
}


const struct heif_encoder_parameter** aom_list_parameters(void* encoder)
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

struct heif_error aom_new_encoder(void** enc)
{
  struct encoder_struct_aom* encoder = new encoder_struct_aom();
  struct heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  aom_set_default_parameters(encoder);

  return err;
}

void aom_free_encoder(void* encoder_raw)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  delete encoder;
}


struct heif_error aom_set_parameter_quality(void* encoder_raw, int quality)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

struct heif_error aom_get_parameter_quality(void* encoder_raw, int* quality)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

struct heif_error aom_set_parameter_lossless(void* encoder_raw, int enable)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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

struct heif_error aom_get_parameter_lossless(void* encoder_raw, int* enable)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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


struct heif_error aom_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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

struct heif_error aom_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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


struct heif_error aom_set_parameter_boolean(void* encoder_raw, const char* name, int value)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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
  }

  set_value(kParam_realtime, realtime_mode);

  return heif_error_unsupported_parameter;
}

struct heif_error aom_get_parameter_boolean(void* encoder_raw, const char* name, int* value)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return aom_get_parameter_lossless(encoder, value);
  }

  get_value(kParam_realtime, realtime_mode);
  get_value(kParam_lossless_alpha, lossless_alpha);

  return heif_error_unsupported_parameter;
}


struct heif_error aom_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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


struct heif_error aom_get_parameter_string(void* encoder_raw, const char* name,
                                           char* value, int value_size)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

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
  }
  else if (strcmp(name, kParam_tune) == 0) {
    switch (encoder->tune) {
      case AOM_TUNE_PSNR:
        save_strcpy(value, value_size, "psnr");
        break;
      case AOM_TUNE_SSIM:
        save_strcpy(value, value_size, "ssim");
        break;
      default:
        assert(false);
        return heif_error_invalid_parameter_value;
    }
  }

  return heif_error_unsupported_parameter;
}


static void aom_set_default_parameters(void* encoder)
{
  for (const struct heif_encoder_parameter** p = aom_encoder_parameter_ptrs; *p; p++) {
    const struct heif_encoder_parameter* param = *p;

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
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  if (*colorspace == heif_colorspace_monochrome) {
    // keep the monochrome colorspace
  }
  else {
    *colorspace = heif_colorspace_YCbCr;
    *chroma = encoder->chroma;
  }
}


void aom_query_encoded_size(void* encoder, uint32_t input_width, uint32_t input_height,
                            uint32_t* encoded_width, uint32_t* encoded_height)
{
  *encoded_width = input_width;
  *encoded_height = input_height;
}


static heif_error encode_frame(aom_codec_ctx_t* codec, aom_image_t* img)
{
  //aom_codec_iter_t iter = NULL;
  int frame_index = 0; // only encoding a single frame
  int flags = 0; // no flags

  //const aom_codec_cx_pkt_t *pkt = NULL;
  const aom_codec_err_t res = aom_codec_encode(codec, img, frame_index, 1, flags);
  if (res != AOM_CODEC_OK) {
    struct heif_error err = {
        heif_error_Encoder_plugin_error,
        heif_suberror_Unspecified,
        kError_encode_frame
    };
    return err;
  }

  return heif_error_ok;
}


struct heif_error aom_encode_image(void* encoder_raw, const struct heif_image* image,
                                   heif_image_input_class input_class)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  struct heif_error err;

  bool success = image->image->extend_padding_to_size(image->image->get_width(),
                                                      image->image->get_height());
  if (!success) {
    err = {heif_error_Memory_allocation_error,
           heif_suberror_Unspecified,
           kError_out_of_memory};
    return err;
  }


  const int source_width = heif_image_get_width(image, heif_channel_Y);
  const int source_height = heif_image_get_height(image, heif_channel_Y);

  const heif_chroma chroma = heif_image_get_chroma_format(image);

  int bpp_y = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

  // --- copy libheif image to aom image

  aom_image_t input_image;

  aom_img_fmt_t img_format = AOM_IMG_FMT_NONE;

  int chroma_height = 0;
  int chroma_sample_position = AOM_CSP_UNKNOWN;

  switch (chroma) {
    case heif_chroma_420:
    case heif_chroma_monochrome:
      img_format = AOM_IMG_FMT_I420;
      chroma_height = (source_height+1)/2;
      chroma_sample_position = AOM_CSP_UNKNOWN; // TODO: change this to CSP_CENTER in the future (https://github.com/AOMediaCodec/av1-avif/issues/88)
      break;
    case heif_chroma_422:
      img_format = AOM_IMG_FMT_I422;
      chroma_height = (source_height+1)/2;
      chroma_sample_position = AOM_CSP_COLOCATED;
      break;
    case heif_chroma_444:
      img_format = AOM_IMG_FMT_I444;
      chroma_height = source_height;
      chroma_sample_position = AOM_CSP_COLOCATED;
      break;
    default:
      img_format = AOM_IMG_FMT_NONE;
      chroma_sample_position = AOM_CSP_UNKNOWN;
      assert(false);
      break;
  }

  if (bpp_y > 8) {
    img_format = (aom_img_fmt_t) (img_format | AOM_IMG_FMT_HIGHBITDEPTH);
  }

  if (!aom_img_alloc(&input_image, img_format,
                     source_width, source_height, 1)) {
    err = {heif_error_Memory_allocation_error,
           heif_suberror_Unspecified,
           "Failed to allocate image"};
    return err;
  }


  for (int plane = 0; plane < 3; plane++) {
    unsigned char* buf = input_image.planes[plane];
    const int stride = input_image.stride[plane];

    if (chroma == heif_chroma_monochrome && plane != 0) {
      if (bpp_y == 8) {
        memset(buf, 128, chroma_height * stride);
      }
      else {
        uint16_t* buf16 = (uint16_t*) buf;
        uint16_t half_range = (uint16_t) (1 << (bpp_y - 1));
        for (int i = 0; i < chroma_height * stride / 2; i++) {
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

    int in_stride = 0;
    const uint8_t* in_p = heif_image_get_plane_readonly(image, (heif_channel) plane, &in_stride);

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



  // --- configure codec

  aom_codec_iface_t* iface;
  aom_codec_ctx_t codec;

  iface = aom_codec_av1_cx();
  //encoder->encoder = get_aom_encoder_by_name("av1");
  if (!iface) {
    err = {heif_error_Unsupported_feature,
           heif_suberror_Unsupported_codec,
           "Unsupported codec: AOMedia Project AV1 Encoder"};
    return err;
  }


  unsigned int aomUsage = 0;
#if defined(AOM_USAGE_ALL_INTRA)
  // aom 3.1.0
  aomUsage = (encoder->realtime_mode ? AOM_USAGE_REALTIME : AOM_USAGE_ALL_INTRA);
#elif defined(AOM_USAGE_REALTIME)
  // aom 2.0
  aomUsage = (encoder->realtime_mode ? AOM_USAGE_REALTIME : AOM_USAGE_GOOD_QUALITY);
#endif

  aom_codec_enc_cfg_t cfg;
  aom_codec_err_t res = aom_codec_enc_config_default(iface, &cfg, aomUsage);
  if (res) {
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Unspecified,
           "Failed to get default codec config"};
    return err;
  }

  heif::Box_av1C::configuration inout_config;
  heif::fill_av1C_configuration(&inout_config, image->image);

  cfg.g_w = source_width;
  cfg.g_h = source_height;
  // Set the max number of frames to encode to 1. This makes the libaom encoder
  // set still_picture and reduced_still_picture_header to 1 in the AV1 sequence
  // header OBU.
  cfg.g_limit = 1;

  cfg.g_profile = inout_config.seq_profile;
  cfg.g_bit_depth = (aom_bit_depth_t) bpp_y;
  cfg.g_input_bit_depth = bpp_y;

  cfg.rc_end_usage = AOM_Q;

  int min_q = encoder->min_q;
  int max_q = encoder->max_q;

  if (input_class == heif_image_input_class_alpha && encoder->alpha_min_q_set && encoder->alpha_max_q_set) {
      min_q = encoder->alpha_min_q;
      max_q = encoder->alpha_max_q;
  }

  cfg.rc_min_quantizer = min_q;
  cfg.rc_max_quantizer = max_q;
  cfg.g_error_resilient = 0;
  cfg.g_threads = encoder->threads;

  if (chroma == heif_chroma_monochrome) {
    cfg.monochrome = 1;
  }

  // --- initialize codec

  aom_codec_flags_t encoder_flags = 0;
  if (bpp_y > 8) {
    encoder_flags = (aom_codec_flags_t) (encoder_flags | AOM_CODEC_USE_HIGHBITDEPTH);
  }

  if (aom_codec_enc_init(&codec, iface, &cfg, encoder_flags)) {
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Unspecified,
           "Failed to initialize encoder"};
    return err;
  }

  aom_codec_control(&codec, AOME_SET_CPUUSED, encoder->cpu_used);

  int quality = encoder->quality;

  if (input_class == heif_image_input_class_alpha && encoder->alpha_quality_set) {
      quality = encoder->alpha_quality;
  }

  int cq_level = ((100 - quality) * 63 + 50) / 100;
  aom_codec_control(&codec, AOME_SET_CQ_LEVEL, cq_level);

  if (encoder->threads > 1) {
#if defined(AV1E_SET_ROW_MT)
    // aom 2.0
    aom_codec_control(&encoder->codec, AV1E_SET_ROW_MT, 1);
#endif
  }


  auto nclx = image->image->get_color_profile_nclx();

  // In aom, color_range defaults to limited range (0). Set it to full range (1).
  aom_codec_control(&codec, AV1E_SET_COLOR_RANGE, nclx ? nclx->get_full_range_flag() : 1);
  aom_codec_control(&codec, AV1E_SET_CHROMA_SAMPLE_POSITION, chroma_sample_position);

  if (nclx &&
      (input_class == heif_image_input_class_normal ||
       input_class == heif_image_input_class_thumbnail)) {
    aom_codec_control(&codec, AV1E_SET_COLOR_PRIMARIES, nclx->get_colour_primaries());
    aom_codec_control(&codec, AV1E_SET_MATRIX_COEFFICIENTS, nclx->get_matrix_coefficients());
    aom_codec_control(&codec, AV1E_SET_TRANSFER_CHARACTERISTICS, nclx->get_transfer_characteristics());
  }

  aom_codec_control(&codec, AOME_SET_TUNING, encoder->tune);

  if (encoder->lossless || (input_class == heif_image_input_class_alpha && encoder->lossless_alpha)) {
    aom_codec_control(&codec, AV1E_SET_LOSSLESS, 1);
  }

#if defined(HAVE_AOM_CODEC_SET_OPTION)
  // Apply the custom AOM encoder options.
  // These should always be applied last as they can override the values that were set above.
  for (const auto& p : encoder->custom_options) {
    aom_codec_set_option(&codec, p.name.c_str(), p.value.c_str());
  }
#endif

  // --- encode frame

  err = encode_frame(&codec, &input_image); //, frame_count++, flags, writer);
  if (err.code != heif_error_Ok) {
    return err;
  }

  encoder->compressedData.clear();
  const aom_codec_cx_pkt_t* pkt = NULL;
  aom_codec_iter_t iter = NULL; // for extracting the compressed packets

  while ((pkt = aom_codec_get_cx_data(&codec, &iter)) != NULL) {

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      //std::cerr.write((char*)pkt->data.frame.buf, pkt->data.frame.sz);

      //printf("packet of size: %d\n",(int)pkt->data.frame.sz);


      // TODO: split the received data into separate OBUs
      // This allows libheif to easily extract the sequence header for the av1C header

      size_t n = pkt->data.frame.sz;
      size_t oldSize = encoder->compressedData.size();
      encoder->compressedData.resize(oldSize + n);

      memcpy(encoder->compressedData.data() + oldSize,
             pkt->data.frame.buf,
             n);

      encoder->data_read = false;
    }
  }

  int flags = 0;
  res = aom_codec_encode(&codec, NULL, -1, 0, flags);
  if (res != AOM_CODEC_OK) {
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Unspecified,
           kError_encode_frame};
    return err;
  }

  iter = NULL;

  while ((pkt = aom_codec_get_cx_data(&codec, &iter)) != NULL) {

    if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      //std::cerr.write((char*)pkt->data.frame.buf, pkt->data.frame.sz);

      //printf("packet of size: %d\n",(int)pkt->data.frame.sz);


      // TODO: split the received data into separate OBUs
      // This allows libheif to easily extract the sequence header for the av1C header

      size_t n = pkt->data.frame.sz;
      size_t oldSize = encoder->compressedData.size();
      encoder->compressedData.resize(oldSize + n);

      memcpy(encoder->compressedData.data() + oldSize,
             pkt->data.frame.buf,
             n);

      encoder->data_read = false;
    }
  }


  // --- clean up

  aom_img_free(&input_image);

  if (aom_codec_destroy(&codec)) {
    err = {heif_error_Encoder_plugin_error,
           heif_suberror_Unspecified,
           "Failed to destroy codec"};
    return err;
  }

  return heif_error_ok;
}


struct heif_error aom_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                          enum heif_encoded_data_type* type)
{
  struct encoder_struct_aom* encoder = (struct encoder_struct_aom*) encoder_raw;

  if (encoder->data_read) {
    *size = 0;
    *data = nullptr;
  }
  else {
    *size = (int) encoder->compressedData.size();
    *data = encoder->compressedData.data();
    encoder->data_read = true;
  }

  return heif_error_ok;
}


static const struct heif_encoder_plugin encoder_plugin_aom
    {
        /* plugin_api_version */ 3,
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
        /* query_encoded_size (v3) */ aom_query_encoded_size
    };

const struct heif_encoder_plugin* get_encoder_plugin_aom()
{
  return &encoder_plugin_aom;
}
