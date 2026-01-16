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

#include "encoder_uncompressed.h"
#include "libheif/heif.h"
#include "libheif/heif_plugin.h"
#include <algorithm>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <memory>

#include <mutex>


static const char* kParam_interleave = "interleave";

static const int PLUGIN_PRIORITY = 60;

static const char* plugin_name = "builtin";


static void uncompressed_set_default_parameters(void* encoder);


static const char* uncompressed_plugin_name()
{
  return plugin_name;
}


#define MAX_NPARAMETERS 14

static heif_encoder_parameter uncompressed_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* uncompressed_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void uncompressed_init_parameters()
{
  heif_encoder_parameter* p = uncompressed_encoder_params;
  const heif_encoder_parameter** d = uncompressed_encoder_parameter_ptrs;
  int i = 0;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_interleave;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "planar";
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS + 1);
  d[i++] = nullptr;
}


const heif_encoder_parameter** uncompressed_list_parameters(void* encoder)
{
  return uncompressed_encoder_parameter_ptrs;
}

static void uncompressed_init_plugin()
{
  uncompressed_init_parameters();
}


static void uncompressed_cleanup_plugin()
{
}

heif_error uncompressed_new_encoder(void** enc)
{
  encoder_struct_uncompressed* encoder = new encoder_struct_uncompressed();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  uncompressed_set_default_parameters(encoder);

  return err;
}

void uncompressed_free_encoder(void* encoder_raw)
{
  encoder_struct_uncompressed* encoder = (encoder_struct_uncompressed*) encoder_raw;

  delete encoder;
}


heif_error uncompressed_set_parameter_quality(void* encoder_raw, int quality)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_ok;
}

heif_error uncompressed_get_parameter_quality(void* encoder_raw, int* quality)
{
  *quality = 100;

  return heif_error_ok;
}

heif_error uncompressed_set_parameter_lossless(void* encoder_raw, int enable)
{
  return heif_error_ok;
}

heif_error uncompressed_get_parameter_lossless(void* encoder_raw, int* enable)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  *enable = true;

  return heif_error_ok;
}

heif_error uncompressed_set_parameter_logging_level(void* encoder_raw, int logging)
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

heif_error uncompressed_get_parameter_logging_level(void* encoder_raw, int* loglevel)
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


heif_error uncompressed_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_unsupported_parameter;
}

heif_error uncompressed_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error uncompressed_set_parameter_boolean(void* encoder_raw, const char* name, int value)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_unsupported_parameter;
}

heif_error uncompressed_get_parameter_boolean(void* encoder_raw, const char* name, int* value)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error uncompressed_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error uncompressed_get_parameter_string(void* encoder_raw, const char* name,
                                             char* value, int value_size)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  return heif_error_unsupported_parameter;
}


static void uncompressed_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = uncompressed_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          uncompressed_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          uncompressed_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          uncompressed_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


void uncompressed_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
{
  //*colorspace = heif_colorspace_YCbCr;
  //*chroma = heif_chroma_420;
}


void uncompressed_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  if (*colorspace == heif_colorspace_monochrome) {
    // keep the monochrome colorspace
  }
  else {
    //*colorspace = heif_colorspace_YCbCr;
    //*chroma = encoder->chroma;
  }
}


heif_error uncompressed_encode_image(void* encoder_raw, const heif_image* image,
                                     heif_image_input_class input_class)
{
  //struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  // Note: this is not used. It is a dummy plugin.

  return heif_error_ok;
}


heif_error uncompressed_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                            heif_encoded_data_type* type)
{
#if 0
  struct encoder_struct_uncompressed* encoder = (struct encoder_struct_uncompressed*) encoder_raw;

  if (encoder->data_read) {
    *size = 0;
    *data = nullptr;
  }
  else {
    *size = (int) encoder->compressedData.size();
    *data = encoder->compressedData.data();
    encoder->data_read = true;
  }
#endif

  return heif_error_ok;
}


heif_error uncompressed_start_sequence_encoding(void* encoder, const heif_image* image,
                                                enum heif_image_input_class image_class,
                                                uint32_t framerate_num, uint32_t framerate_denom,
                                                const heif_sequence_encoding_options* options)
{
  return heif_error_ok;
}

heif_error uncompressed_encode_sequence_frame(void* encoder, const heif_image* image, uintptr_t frame_nr)
{
  return heif_error_ok;
}

heif_error uncompressed_end_sequence_encoding(void* encoder)
{
  return heif_error_ok;
}

heif_error uncompressed_get_compressed_data2(void* encoder, uint8_t** data, int* size,
                                             uintptr_t* frame_nr,
                                             int* is_keyframe, int* more_frame_packets)
{
  return heif_error_ok;
}


static const heif_encoder_plugin encoder_plugin_uncompressed
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_uncompressed,
        /* id_name */ "uncompressed",
        /* priority */ PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ uncompressed_plugin_name,
        /* init_plugin */ uncompressed_init_plugin,
        /* cleanup_plugin */ uncompressed_cleanup_plugin,
        /* new_encoder */ uncompressed_new_encoder,
        /* free_encoder */ uncompressed_free_encoder,
        /* set_parameter_quality */ uncompressed_set_parameter_quality,
        /* get_parameter_quality */ uncompressed_get_parameter_quality,
        /* set_parameter_lossless */ uncompressed_set_parameter_lossless,
        /* get_parameter_lossless */ uncompressed_get_parameter_lossless,
        /* set_parameter_logging_level */ uncompressed_set_parameter_logging_level,
        /* get_parameter_logging_level */ uncompressed_get_parameter_logging_level,
        /* list_parameters */ uncompressed_list_parameters,
        /* set_parameter_integer */ uncompressed_set_parameter_integer,
        /* get_parameter_integer */ uncompressed_get_parameter_integer,
        /* set_parameter_boolean */ uncompressed_set_parameter_boolean,
        /* get_parameter_boolean */ uncompressed_get_parameter_boolean,
        /* set_parameter_string */ uncompressed_set_parameter_string,
        /* get_parameter_string */ uncompressed_get_parameter_string,
        /* query_input_colorspace */ uncompressed_query_input_colorspace,
        /* encode_image */ uncompressed_encode_image,
        /* get_compressed_data */ uncompressed_get_compressed_data,
        /* query_input_colorspace (v2) */ uncompressed_query_input_colorspace2,
        /* query_encoded_size (v3) */ nullptr,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ uncompressed_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ uncompressed_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ uncompressed_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ uncompressed_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 1
    };

const heif_encoder_plugin* get_encoder_plugin_uncompressed()
{
  return &encoder_plugin_uncompressed;
}


#if 0 // PLUGIN_uncompressed_ENCODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_uncompressed
};
#endif
