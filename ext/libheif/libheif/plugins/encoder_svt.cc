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

#include "libheif/heif.h"
#include "libheif/heif_plugin.h"
#include "encoder_svt.h"
#include <vector>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <deque>
#include <memory>
#include <limits>

#include "svt-av1/EbSvtAv1.h"
#include "svt-av1/EbSvtAv1Enc.h"
#include <utility>

// TODO: for some reason, the SVT encoder outputs only keyframes for me.
//       It appears to work in libavif, but I didn't find the difference yet.

struct encoder_struct_svt
{
  ~encoder_struct_svt()
  {
    if (svt_encoder) {
      svt_av1_enc_deinit(svt_encoder);
      svt_av1_enc_deinit_handle(svt_encoder);
    }
  }

  int speed = 12; // 0-13

  int quality;

  int min_q = 0;
  int max_q = 63;
  int qp = -1;
  bool qp_set = false;
#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  bool lossless = false;
#endif

  int threads = 4;

  int tile_rows = 1; // 1,2,4,8,16,32,64
  int tile_cols = 1; // 1,2,4,8,16,32,64

#if SVT_AV1_CHECK_VERSION(0, 9, 1)
  enum Tune
  {
    Tune_VQ = 0,
    Tune_PSNR = 1,
    Tune_SSIM = 2
  };

  uint8_t tune = Tune_PSNR;
#endif

  heif_chroma chroma = heif_chroma_420; // SVT-AV1 only supports 4:2:0 as of v3.0.0
  heif_image_input_class input_class = heif_image_input_class_normal;

  // int keyframe_interval = 0; TODO: this does not seem to work (see all [1])

  // --- Encoder

  EbComponentType* svt_encoder = nullptr;
  EbSvtAv1EncConfiguration svt_config;
  EbBufferHeaderType input_buffer;

  bool still_image_mode = false;

  // --- output

  struct Packet
  {
    std::vector<uint8_t> compressedData;
    uintptr_t frameNr = 0;
    bool is_keyframe = false;
  };

  std::deque<Packet> output_packets;
  std::vector<uint8_t> active_output_data;

  //std::vector<uint8_t> compressed_data;
  //bool data_read = false;
};

//static const char* kError_out_of_memory = "Out of memory";

static const char* kParam_min_q = "min-q";
static const char* kParam_max_q = "max-q";
static const char* kParam_qp = "qp";
static const char* kParam_threads = "threads";
static const char* kParam_speed = "speed";

#if SVT_AV1_CHECK_VERSION(0, 9, 1)
static const char* kParam_tune = "tune";
static const char* const kParam_tune_valid_values[] = {"vq", "psnr", "ssim", nullptr};
#endif

/*
static const char* kParam_chroma = "chroma";
static const char* const kParam_chroma_valid_values[] = {
    "420", "422", "444", nullptr
};
*/

static int valid_tile_num_values[] = {1, 2, 4, 8, 16, 32, 64};

static heif_error heif_error_codec_library_error = {
  heif_error_Encoder_plugin_error,
  heif_suberror_Unspecified,
  "SVT-AV1 error"
};

static const int SVT_PLUGIN_PRIORITY = 40;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void svt_set_default_parameters(void* encoder);


static const char* svt_plugin_name()
{
  plugin_name[MAX_PLUGIN_NAME_LENGTH - 1] = 0;
  snprintf(plugin_name, MAX_PLUGIN_NAME_LENGTH, "SVT-AV1 encoder %s", svt_av1_get_version());

  return plugin_name;
}

int int_log2(int pow2_value)
{
  int input_value = pow2_value;
  (void) input_value;

  int v = 0;
  while (pow2_value > 1) {
    pow2_value >>= 1;
    v++;
  }

  // check that computation is correct
  assert(input_value == (1 << v));

  return v;
}

#define MAX_NPARAMETERS 11

static heif_encoder_parameter svt_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* svt_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void svt_init_parameters()
{
  heif_encoder_parameter* p = svt_encoder_params;
  const heif_encoder_parameter** d = svt_encoder_parameter_ptrs;
  int i = 0;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_speed;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 12;
  p->has_default = true;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 13;
  p->integer.valid_values = nullptr;
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
  p->integer.valid_values = nullptr;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = "tile-rows";
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 4;
  p->has_default = true;
  p->integer.have_minimum_maximum = false;
  p->integer.valid_values = valid_tile_num_values;
  p->integer.num_valid_values = 7;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = "tile-cols";
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 4;
  p->has_default = true;
  p->integer.have_minimum_maximum = false;
  p->integer.valid_values = valid_tile_num_values;
  p->integer.num_valid_values = 7;
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

#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = heif_encoder_parameter_name_lossless;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;
#endif

#if 0
  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_chroma;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "420";
  p->has_default = true;
  p->string.valid_values = kParam_chroma_valid_values;
  d[i++] = p++;
#endif

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_qp;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 50;
  p->has_default = false;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = 0;
  p->integer.maximum = 63;
  p->integer.valid_values = nullptr;
  p->integer.num_valid_values = 0;
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
  p->integer.valid_values = nullptr;
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
  p->integer.valid_values = nullptr;
  p->integer.num_valid_values = 0;
  d[i++] = p++;

#if SVT_AV1_CHECK_VERSION(0, 9, 1)
  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_tune;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "psnr";
  p->has_default = true;
  p->string.valid_values = kParam_tune_valid_values;
  d[i++] = p++;
#endif

  d[i++] = nullptr;
}


const heif_encoder_parameter** svt_list_parameters(void* encoder)
{
  return svt_encoder_parameter_ptrs;
}

static void svt_init_plugin()
{
  svt_init_parameters();
}


static void svt_cleanup_plugin()
{
}

heif_error svt_new_encoder(void** enc)
{
  auto* encoder = new encoder_struct_svt();
  struct heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  svt_set_default_parameters(encoder);

  return err;
}

void svt_free_encoder(void* encoder_raw)
{
  auto* encoder = (struct encoder_struct_svt*) encoder_raw;

  delete encoder;
}


heif_error svt_set_parameter_quality(void* encoder_raw, int quality)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

heif_error svt_get_parameter_quality(void* encoder_raw, int* quality)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

heif_error svt_set_parameter_lossless(void* encoder_raw, int enable)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  encoder->lossless = enable;
#else
  if (enable) {
    encoder->min_q = 0;
    encoder->max_q = 0;
    encoder->qp = 0;
    encoder->qp_set = true;
    encoder->quality = 100; // not really required, but to be consistent
  }
#endif

  return heif_error_ok;
}

heif_error svt_get_parameter_lossless(void* encoder_raw, int* enable)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  *enable = encoder->lossless;
#else
  *enable = (encoder->min_q == 0 && encoder->max_q == 0 &&
             ((encoder->qp_set && encoder->qp == 0) || encoder->quality == 100));
#endif

  return heif_error_ok;
}

heif_error svt_set_parameter_logging_level(void* encoder_raw, int logging)
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

heif_error svt_get_parameter_logging_level(void* encoder_raw, int* loglevel)
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


heif_error svt_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_svt* encoder = (encoder_struct_svt*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return svt_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return svt_set_parameter_lossless(encoder, value);
  }
  else if (strcmp(name, kParam_qp) == 0) {
    encoder->qp = value;
    encoder->qp_set = true;
    return heif_error_ok;
  }

  set_value(kParam_min_q, min_q);
  set_value(kParam_max_q, max_q);
  set_value(kParam_threads, threads);
  set_value(kParam_speed, speed);
  set_value("tile-rows", tile_rows);
  set_value("tile-cols", tile_cols);

  return heif_error_unsupported_parameter;
}

heif_error svt_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return svt_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return svt_get_parameter_lossless(encoder, value);
  }

  get_value(kParam_min_q, min_q);
  get_value(kParam_max_q, max_q);
  get_value(kParam_qp, qp); // TODO: what if qp was not set ?
  get_value(kParam_threads, threads);
  get_value(kParam_speed, speed);
  get_value("tile-rows", tile_rows);
  get_value("tile-cols", tile_cols);

  return heif_error_unsupported_parameter;
}


heif_error svt_set_parameter_boolean(void* encoder_raw, const char* name, int value)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return svt_set_parameter_lossless(encoder, value);
  }

  //set_value(kParam_realtime, realtime_mode);

  return heif_error_unsupported_parameter;
}

heif_error svt_get_parameter_boolean(void* encoder_raw, const char* name, int* value)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return svt_get_parameter_lossless(encoder, value);
  }

  //get_value(kParam_realtime, realtime_mode);

  return heif_error_unsupported_parameter;
}


heif_error svt_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;
  (void) encoder;

#if 0
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
#endif

#if SVT_AV1_CHECK_VERSION(0, 9, 1)
  if (strcmp(name, kParam_tune) == 0) {
    if (strcmp(value, "vq") == 0) {
      encoder->tune = encoder_struct_svt::Tune_VQ;
      return heif_error_ok;
    }
    else if (strcmp(value, "psnr") == 0) {
      encoder->tune = encoder_struct_svt::Tune_PSNR;
      return heif_error_ok;
    }
    else if (strcmp(value, "ssim") == 0) {
      encoder->tune = encoder_struct_svt::Tune_SSIM;
      return heif_error_ok;
    }
  }
#endif

  return heif_error_unsupported_parameter;
}


#if SVT_AV1_CHECK_VERSION(0, 9, 1)
static void save_strcpy(char* dst, int dst_size, const char* src)
{
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = 0;
}
#endif


heif_error svt_get_parameter_string(void* encoder_raw, const char* name,
                                    char* value, int value_size)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;
  (void) encoder;

#if 0
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
#endif

#if SVT_AV1_CHECK_VERSION(0, 9, 1)
  if (strcmp(name, kParam_tune) == 0) {
    switch (encoder->tune) {
      case encoder_struct_svt::Tune_VQ:
        save_strcpy(value, value_size, "vq");
        break;
      case encoder_struct_svt::Tune_PSNR:
        save_strcpy(value, value_size, "psnr");
        break;
      case encoder_struct_svt::Tune_SSIM:
        save_strcpy(value, value_size, "ssim");
        break;
      default:
        assert(false);

        return heif_error_invalid_parameter_value;
    }
    return heif_error_ok;
  }
#endif

  return heif_error_unsupported_parameter;
}


static void svt_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = svt_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          svt_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          svt_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          // NOLINTNEXTLINE(build/include_what_you_use)
          svt_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


void svt_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
{
  *colorspace = heif_colorspace_YCbCr;
  *chroma = heif_chroma_420;
}


void svt_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  //auto* encoder = (struct encoder_struct_svt*) encoder_raw;

  *colorspace = heif_colorspace_YCbCr;
  *chroma = heif_chroma_420;
}


void svt_query_encoded_size(void* encoder_raw, uint32_t input_width, uint32_t input_height,
                            uint32_t* encoded_width, uint32_t* encoded_height)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  // SVT-AV1 (as of version 1.2.1) can only create image sizes matching the chroma format. Add padding if necessary.

  if (input_width < 64) {
    *encoded_width = 64;
  }
  else if (encoder->chroma == heif_chroma_420 && (input_width & 1) == 1) {
    *encoded_width = input_width + 1;
  }
  else {
    *encoded_width = input_width;
  }

  if (input_height < 64) {
    *encoded_height = 64;
  }
  else if (encoder->chroma != heif_chroma_444 && (input_height & 1) == 1) {
    *encoded_height = input_height + 1;
  }
  else {
    *encoded_height = input_height;
  }
}


static heif_error svt_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                                     enum heif_image_input_class input_class,
                                                     uint32_t framerate_num, uint32_t framerate_denom,
                                                     const heif_sequence_encoding_options* options,
                                                     bool image_sequence)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;
  encoder->input_class = input_class;
  EbErrorType res = EB_ErrorNone;
  heif_error err;

  // encoder->compressed_data.clear();

  int w = heif_image_get_width(image, heif_channel_Y);
  int h = heif_image_get_height(image, heif_channel_Y);

  uint32_t encoded_width, encoded_height;
  svt_query_encoded_size(encoder_raw, w, h, &encoded_width, &encoded_height);

  const heif_chroma chroma = heif_image_get_chroma_format(image);
  int bitdepth_y = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

  EbColorFormat color_format = EB_YUV420;

  if (input_class == heif_image_input_class_alpha) {
    color_format = EB_YUV420;
    //chromaPosition = RA_CHROMA_SAMPLE_POSITION_UNKNOWN;
  }
  else {
    switch (chroma) {
      case heif_chroma_444:
        color_format = EB_YUV444;
        //chromaPosition = RA_CHROMA_SAMPLE_POSITION_COLOCATED;
        break;
      case heif_chroma_422:
        color_format = EB_YUV422;
        //chromaPosition = RA_CHROMA_SAMPLE_POSITION_COLOCATED;
        break;
      case heif_chroma_420:
        color_format = EB_YUV420;
        //chromaPosition = RA_CHROMA_SAMPLE_POSITION_UNKNOWN; // TODO: set to CENTER when AV1 and svt supports this
        break;
      default:
        return heif_error_codec_library_error;
    }
  }


  // --- initialize the encoder

  EbComponentType*& svt_encoder = encoder->svt_encoder;
  EbSvtAv1EncConfiguration& svt_config = encoder->svt_config;
  memset(&svt_config, 0, sizeof(EbSvtAv1EncConfiguration));

#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  res = svt_av1_enc_init_handle(&svt_encoder, &svt_config);
#else
  res = svt_av1_enc_init_handle(&svt_encoder, nullptr, &svt_config);
#endif
  if (res != EB_ErrorNone) {
    //goto cleanup;
    return heif_error_codec_library_error;
  }

  svt_config.encoder_color_format = color_format;
  svt_config.encoder_bit_depth = (uint8_t) bitdepth_y;
  //svt_config.is_16bit_pipeline = bitdepth_y > 8;

#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  svt_config.lossless = encoder->lossless;
#endif

  heif_color_profile_nclx* nclx = nullptr;
  err = heif_image_get_nclx_color_profile(image, &nclx);
  if (err.code != heif_error_Ok) {
    nclx = nullptr;
  }

  // make sure NCLX profile is deleted at end of function
  auto nclx_deleter = std::unique_ptr<heif_color_profile_nclx, void (*)(heif_color_profile_nclx*)>(nclx, heif_nclx_color_profile_free);

  if (nclx) {
#if !SVT_AV1_CHECK_VERSION(3, 0, 0)
    svt_config.color_description_present_flag = true;
#endif
#if SVT_AV1_VERSION_MAJOR >= 1
    svt_config.color_primaries = static_cast<EbColorPrimaries>(nclx->color_primaries);
    svt_config.transfer_characteristics = static_cast<EbTransferCharacteristics>(nclx->transfer_characteristics);
    svt_config.matrix_coefficients = static_cast<EbMatrixCoefficients>(nclx->matrix_coefficients);
    svt_config.color_range = nclx->full_range_flag ? EB_CR_FULL_RANGE : EB_CR_STUDIO_RANGE;
#else
    svt_config.color_primaries = static_cast<uint8_t>(nclx->color_primaries);
    svt_config.transfer_characteristics = static_cast<uint8_t>(nclx->transfer_characteristics);
    svt_config.matrix_coefficients = static_cast<uint8_t>(nclx->matrix_coefficients);
    svt_config.color_range = nclx->full_range_flag ? 1 : 0;
#endif


#if !SVT_AV1_CHECK_VERSION(3, 0, 0)
    // Follow comment in svt header: set if input is HDR10 BT2020 using SMPTE ST2084.
    svt_config.high_dynamic_range_input = (bitdepth_y == 10 && // TODO: should this be >8 ?
                                           nclx->color_primaries == heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 &&
                                           nclx->transfer_characteristics == heif_transfer_characteristic_ITU_R_BT_2100_0_PQ &&
                                           nclx->matrix_coefficients == heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance);
#endif
  }
  else {
#if !SVT_AV1_CHECK_VERSION(3, 0, 0)
    svt_config.color_description_present_flag = false;
#endif
  }


  svt_config.source_width = encoded_width;
  svt_config.source_height = encoded_height;
#if SVT_AV1_CHECK_VERSION(3, 0, 0)
  svt_config.level_of_parallelism = encoder->threads;
#else
  svt_config.logical_processors = encoder->threads;
#endif

  // disable 2-pass
  svt_config.rc_stats_buffer = SvtAv1FixedBuf{nullptr, 0};

  svt_config.rate_control_mode = 0; // constant rate factor
  //svt_config.enable_adaptive_quantization = 0;   // 2 is CRF (the default), 0 would be CQP
  int qp;
  if (encoder->qp_set) {
    qp = encoder->qp;
  }
  else {
    qp = ((100 - encoder->quality) * 63 + 50) / 100;
  }
  svt_config.qp = qp;
  svt_config.min_qp_allowed = encoder->min_q;
  svt_config.max_qp_allowed = encoder->max_q;

  svt_config.tile_rows = int_log2(encoder->tile_rows);
  svt_config.tile_columns = int_log2(encoder->tile_cols);

#if SVT_AV1_CHECK_VERSION(0, 9, 1)
  svt_config.tune = encoder->tune;
#endif

  svt_config.enc_mode = (int8_t) encoder->speed;

  if (image_sequence) {
    // svt_config.force_key_frames = true; TODO: this does not seem to work (see all [1])

#if 1
    // TODO: setting pref_structure to SVT_AV1_PRED_LOW_DELAY_B hangs the encoder

    switch (options->gop_structure) {
      case heif_sequence_gop_structure_intra_only:
        //svt_config.pred_structure = SvtAv1PredStructure::SVT_AV1_PRED_LOW_DELAY_B;
        break;
      case heif_sequence_gop_structure_lowdelay:
        //svt_config.pred_structure = SvtAv1PredStructure::SVT_AV1_PRED_LOW_DELAY_B;
        break;
      case heif_sequence_gop_structure_unrestricted:
        svt_config.pred_structure = SvtAv1PredStructure::SVT_AV1_PRED_RANDOM_ACCESS;
        break;
    }

    if (options->keyframe_distance_max) {
      svt_config.intra_period_length = options->keyframe_distance_max; // TODO -1 ?
    }
#endif
  }
  else {
    // TODO: enable when https://gitlab.com/AOMediaCodec/SVT-AV1/-/issues/2245 is resolved
    // svt_config.avif = true;
    // encoder->still_image_mode = true;
  }

  if (color_format == EB_YUV422 || bitdepth_y > 10) {
    svt_config.profile = PROFESSIONAL_PROFILE;
  }
  else if (color_format == EB_YUV444) {
    svt_config.profile = HIGH_PROFILE;
  }

  svt_config.frame_rate_numerator = framerate_num;
  svt_config.frame_rate_denominator = framerate_denom;

  res = svt_av1_enc_set_parameter(svt_encoder, &svt_config);
  if (res == EB_ErrorBadParameter) {
    return heif_error_codec_library_error;
  }

  res = svt_av1_enc_init(svt_encoder);
  if (res != EB_ErrorNone) {
    return heif_error_codec_library_error;
  }

  return {};
}


static heif_error read_encoder_output_packets(void* encoder_raw, bool done_sending_pics)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;
  EbComponentType*& svt_encoder = encoder->svt_encoder;
  EbErrorType res = EB_ErrorNone;


  // --- read compressed picture

  int encode_at_eos = 0;

  do {
    EbBufferHeaderType* output_buf = nullptr;

    res = svt_av1_enc_get_packet(svt_encoder, &output_buf, (uint8_t) done_sending_pics);
    if (output_buf != nullptr) {
      encode_at_eos = ((output_buf->flags & EB_BUFFERFLAG_EOS) == EB_BUFFERFLAG_EOS);
      if (output_buf->p_buffer && (output_buf->n_filled_len > 0)) {
        uint8_t* data = output_buf->p_buffer;
        uint32_t n = output_buf->n_filled_len;

        encoder_struct_svt::Packet output_packet;
        output_packet.frameNr = output_buf->pts;
        output_packet.is_keyframe = (output_buf->pic_type == EbAv1PictureType::EB_AV1_KEY_PICTURE);

        encoder->output_packets.emplace_back(output_packet);
        encoder->output_packets.back().compressedData.resize(n);

        memcpy(encoder->output_packets.back().compressedData.data(),
               data, n);

        /* TODO: this is a hack
         * When using
         *           svt_config.pred_structure = SvtAv1PredStructure::SVT_AV1_PRED_LOW_DELAY_B;
         * svt_av1_enc_get_packet() hangs on the second call. As a workaround, we can leave the
         * loop when we got the image:
         */
        if (output_packet.is_keyframe)
          break;
      }
      svt_av1_enc_release_out_buffer(&output_buf);
    }
  } while (res == EB_ErrorNone && !encode_at_eos);


  // delete input_buffer.p_buffer;

  if (!done_sending_pics && ((res == EB_ErrorNone) || (res == EB_NoErrorEmptyQueue))) {
    return heif_error_ok;
  }
  else {
    return (res == EB_ErrorNone ? heif_error_ok : heif_error_codec_library_error);
  }
}


static heif_error svt_encode_sequence_frame(void* encoder_raw, const heif_image* image, uintptr_t frame_nr)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;
  EbComponentType*& svt_encoder = encoder->svt_encoder;
  EbErrorType res = EB_ErrorNone;

  int w = heif_image_get_width(image, heif_channel_Y);
  int h = heif_image_get_height(image, heif_channel_Y);
  const heif_chroma chroma = heif_image_get_chroma_format(image);
  int bitdepth_y = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

  uint32_t encoded_width, encoded_height;
  svt_query_encoded_size(encoder_raw, w, h, &encoded_width, &encoded_height);

  // Note: it is ok to cast away the const, as the image content is not changed.
  // However, we have to guarantee that there are no plane pointers or stride values kept over calling the svt_encode_image() function.
  heif_error err = heif_image_extend_padding_to_size(const_cast<struct heif_image*>(image),
                                                     (int) encoded_width,
                                                     (int) encoded_height);
  if (err.code) {
    return err;
  }


  uint8_t yShift = 0;

  if (encoder->input_class == heif_image_input_class_alpha) {
    yShift = 1;
  }
  else if (chroma == heif_chroma_420) {
    yShift = 1;
  }


  // --- copy libheif image to svt image

  EbBufferHeaderType& input_buffer = encoder->input_buffer;

  auto svt_io_format = std::make_unique<EbSvtIOFormat>();
  input_buffer.p_buffer = (uint8_t*) svt_io_format.get();
  memset(input_buffer.p_buffer, 0, sizeof(EbSvtIOFormat));

  input_buffer.size = sizeof(EbBufferHeaderType);
  input_buffer.p_app_private = nullptr;
  input_buffer.pic_type = EB_AV1_INVALID_PICTURE;
  input_buffer.metadata = nullptr;
  input_buffer.pts = frame_nr;

  /* TODO: this does not seem to work (see all [1])
  if (encoder->keyframe_interval) {
    if (frame_nr % encoder->keyframe_interval == 0) {
      input_buffer.pic_type = EB_AV1_KEY_PICTURE;
    }
  }
  */

  auto* input_picture_buffer = (EbSvtIOFormat*) input_buffer.p_buffer;

  int bytesPerPixel = bitdepth_y > 8 ? 2 : 1;
  std::vector<uint8_t> dummy_color_plane;
  if (encoder->input_class == heif_image_input_class_alpha) {
    size_t stride64;
    input_picture_buffer->luma = (uint8_t*) heif_image_get_plane_readonly2(image, heif_channel_Y, &stride64);

    uint32_t stride32;
    if (stride64 > std::numeric_limits<uint32_t>::max()) {
      return {
        heif_error_Encoder_plugin_error,
        heif_suberror_Unspecified,
        "Image too wide for encoder"
      };
    }
    else {
      stride32 = static_cast<uint32_t>(stride64);
    }

    input_picture_buffer->y_stride = stride32 / bytesPerPixel;
    input_buffer.n_filled_len = stride32 * encoded_height;

    uint32_t uvWidth = get_subsampled_size_h(encoded_width, heif_channel_Cb, heif_chroma_420, scaling_mode::round_up);
    uint32_t uvHeight = get_subsampled_size_v(encoded_height, heif_channel_Cb, heif_chroma_420, scaling_mode::round_up);
    dummy_color_plane.resize(uvWidth * uvHeight);

    if (bitdepth_y <= 8) {
      uint8_t val = static_cast<uint8_t>(1 << (bitdepth_y - 1));
      memset(dummy_color_plane.data(), val, uvWidth * uvHeight * bytesPerPixel);
    }
    else {
      assert(bitdepth_y > 8 && bitdepth_y <= 16);
      uint16_t val = static_cast<uint16_t>(1 << (bitdepth_y - 1));
      uint8_t high = static_cast<uint8_t>((val >> 8) & 0xFF);
      uint8_t low = static_cast<uint8_t>(val & 0xFF);

      for (uint32_t i = 0; i < uvWidth * uvHeight; i++) {
        dummy_color_plane[2 * i] = low;
        dummy_color_plane[2 * i + 1] = high;
      }
    }

    input_buffer.n_filled_len += 2 * uvWidth * uvHeight;
    input_picture_buffer->cb_stride = uvWidth;
    input_picture_buffer->cr_stride = uvWidth;

    input_picture_buffer->cb = dummy_color_plane.data();
    input_picture_buffer->cr = dummy_color_plane.data();
  }
  else {
    size_t stride64;
    input_picture_buffer->luma = (uint8_t*) heif_image_get_plane_readonly2(image, heif_channel_Y, &stride64);

    uint32_t stride32;
    if (stride64 > std::numeric_limits<uint32_t>::max()) {
      return {
        heif_error_Encoder_plugin_error,
        heif_suberror_Unspecified,
        "Image too wide for encoder"
      };
    }
    else {
      stride32 = static_cast<uint32_t>(stride64);
    }

    input_picture_buffer->y_stride = stride32 / bytesPerPixel;
    input_buffer.n_filled_len = stride32 * encoded_height;

    uint32_t uvHeight = (encoded_height + yShift) >> yShift;
    input_picture_buffer->cb = (uint8_t*) heif_image_get_plane_readonly2(image, heif_channel_Cb, &stride64);
    stride32 = static_cast<uint32_t>(stride64); // chroma stride should always be smaller than luma stride
    input_buffer.n_filled_len += stride32 * uvHeight;
    input_picture_buffer->cb_stride = stride32 / bytesPerPixel;

    input_picture_buffer->cr = (uint8_t*) heif_image_get_plane_readonly2(image, heif_channel_Cr, &stride64);
    stride32 = static_cast<uint32_t>(stride64); // chroma stride should always be smaller than luma stride
    input_buffer.n_filled_len += stride32 * uvHeight;
    input_picture_buffer->cr_stride = stride32 / bytesPerPixel;
  }

  input_buffer.flags = 0;
  // input_buffer.pts = 0;

  EbAv1PictureType frame_type = EB_AV1_KEY_PICTURE;

  input_buffer.pic_type = frame_type;

  res = svt_av1_enc_send_picture(svt_encoder, &input_buffer);
  if (res != EB_ErrorNone) {
    delete input_buffer.p_buffer;
    return heif_error_codec_library_error;
  }

  return read_encoder_output_packets(encoder_raw, false);
}


static heif_error svt_end_sequence_encoding(void* encoder_raw)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

  EbComponentType*& svt_encoder = encoder->svt_encoder;
  //EbBufferHeaderType& input_buffer = encoder->input_buffer;

  // --- flush encoder

  EbErrorType ret = EB_ErrorNone;

  EbBufferHeaderType flush_input_buffer;
  flush_input_buffer.n_alloc_len = 0;
  flush_input_buffer.n_filled_len = 0;
  flush_input_buffer.n_tick_count = 0;
  flush_input_buffer.p_app_private = nullptr;
  flush_input_buffer.flags = EB_BUFFERFLAG_EOS;
  flush_input_buffer.p_buffer = nullptr;
  flush_input_buffer.metadata = nullptr;

  ret = svt_av1_enc_send_picture(svt_encoder, &flush_input_buffer);

  if (ret != EB_ErrorNone) {
    // delete input_buffer.p_buffer;
    return heif_error_codec_library_error;
  }

  return read_encoder_output_packets(encoder_raw, true);
}


heif_error svt_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                    uintptr_t* out_framenr, int* out_is_keyframe,
                                    int* more_frame_packets)
{
  auto* encoder = (encoder_struct_svt*) encoder_raw;

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


heif_error svt_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                   heif_encoded_data_type* type)
{
  return svt_get_compressed_data2(encoder_raw, data, size, nullptr, nullptr, nullptr);
}


static heif_error svt_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                              enum heif_image_input_class input_class,
                                              uint32_t framerate_num, uint32_t framerate_denom,
                                              const heif_sequence_encoding_options* options)
{
  return svt_start_sequence_encoding_intern(encoder_raw, image, input_class,
                                            framerate_num, framerate_denom, options, true);
}


heif_error svt_encode_image(void* encoder_raw, const heif_image* image,
                            heif_image_input_class input_class)
{
  heif_error err;
  err = svt_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = svt_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  err = svt_end_sequence_encoding(encoder_raw);
  if (err.code) {
    return err;
  }

  return {};
}


static const heif_encoder_plugin encoder_plugin_svt
{
  /* plugin_api_version */ 4,
                           /* compression_format */ heif_compression_AV1,
                           /* id_name */ "svt",
                           /* priority */ SVT_PLUGIN_PRIORITY,
                           /* supports_lossy_compression */ true,
#if SVT_AV1_CHECK_VERSION(3, 0, 0)
                           /* supports_lossless_compression */ true,
#else
        /* supports_lossless_compression */ false,
#endif
                           /* get_plugin_name */ svt_plugin_name,
                           /* init_plugin */ svt_init_plugin,
                           /* cleanup_plugin */ svt_cleanup_plugin,
                           /* new_encoder */ svt_new_encoder,
                           /* free_encoder */ svt_free_encoder,
                           /* set_parameter_quality */ svt_set_parameter_quality,
                           /* get_parameter_quality */ svt_get_parameter_quality,
                           /* set_parameter_lossless */ svt_set_parameter_lossless,
                           /* get_parameter_lossless */ svt_get_parameter_lossless,
                           /* set_parameter_logging_level */ svt_set_parameter_logging_level,
                           /* get_parameter_logging_level */ svt_get_parameter_logging_level,
                           /* list_parameters */ svt_list_parameters,
                           /* set_parameter_integer */ svt_set_parameter_integer,
                           /* get_parameter_integer */ svt_get_parameter_integer,
                           /* set_parameter_boolean */ svt_set_parameter_boolean,
                           /* get_parameter_boolean */ svt_get_parameter_boolean,
                           /* set_parameter_string */ svt_set_parameter_string,
                           /* get_parameter_string */ svt_get_parameter_string,
                           /* query_input_colorspace */ svt_query_input_colorspace,
                           /* encode_image */ svt_encode_image,
                           /* get_compressed_data */ svt_get_compressed_data,
                           /* query_input_colorspace (v2) */ svt_query_input_colorspace2,
                           /* query_encoded_size (v3) */ svt_query_encoded_size,
                           /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
                           /* start_sequence_encoding (v4) */ svt_start_sequence_encoding,
                           /* encode_sequence_frame (v4) */ svt_encode_sequence_frame,
                           /* end_sequence_encoding (v4) */ svt_end_sequence_encoding,
                           /* get_compressed_data2 (v4) */ svt_get_compressed_data2,
                           /* does_indicate_keyframes (v4) */ 1
};

const heif_encoder_plugin* get_encoder_plugin_svt()
{
  return &encoder_plugin_svt;
}


#if PLUGIN_SvtEnc
heif_plugin_info plugin_info{
    1,
    heif_plugin_type_encoder,
    &encoder_plugin_svt
};
#endif
