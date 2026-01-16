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

#include "encoder_mask.h"
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

static const char* plugin_name = "mask";


static void mask_set_default_parameters(void* encoder);


static const char* mask_plugin_name()
{
  return plugin_name;
}


#define MAX_NPARAMETERS 14

static heif_encoder_parameter mask_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* mask_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void mask_init_parameters()
{
  heif_encoder_parameter* p = mask_encoder_params;
  const heif_encoder_parameter** d = mask_encoder_parameter_ptrs;
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


const heif_encoder_parameter** mask_list_parameters(void* encoder)
{
  return mask_encoder_parameter_ptrs;
}

static void mask_init_plugin()
{
  mask_init_parameters();
}


static void mask_cleanup_plugin()
{
}

heif_error mask_new_encoder(void** enc)
{
  encoder_struct_mask* encoder = new encoder_struct_mask();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  mask_set_default_parameters(encoder);

  return err;
}

void mask_free_encoder(void* encoder_raw)
{
  encoder_struct_mask* encoder = (encoder_struct_mask*) encoder_raw;

  delete encoder;
}


heif_error mask_set_parameter_quality(void* encoder_raw, int quality)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_ok;
}

heif_error mask_get_parameter_quality(void* encoder_raw, int* quality)
{
  *quality = 100;

  return heif_error_ok;
}

heif_error mask_set_parameter_lossless(void* encoder_raw, int enable)
{
  return heif_error_ok;
}

heif_error mask_get_parameter_lossless(void* encoder_raw, int* enable)
{
  *enable = true;

  return heif_error_ok;
}

heif_error mask_set_parameter_logging_level(void* encoder_raw, int logging)
{
  return heif_error_ok;
}

heif_error mask_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
  *loglevel = 0;

  return heif_error_ok;
}

#define set_value(paramname, paramvar) if (strcmp(name, paramname)==0) { encoder->paramvar = value; return heif_error_ok; }
#define get_value(paramname, paramvar) if (strcmp(name, paramname)==0) { *value = encoder->paramvar; return heif_error_ok; }


heif_error mask_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_unsupported_parameter;
}

heif_error mask_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error mask_set_parameter_boolean(void* encoder_raw, const char* name, int value)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_unsupported_parameter;
}

heif_error mask_get_parameter_boolean(void* encoder_raw, const char* name, int* value)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error mask_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error mask_get_parameter_string(void* encoder_raw, const char* name,
                                                    char* value, int value_size)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  return heif_error_unsupported_parameter;
}


static void mask_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = mask_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          mask_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          mask_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          mask_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


void mask_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
{
}


void mask_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  if (*colorspace == heif_colorspace_monochrome) {
    // keep the monochrome colorspace
  }
  else {
    //*colorspace = heif_colorspace_YCbCr;
    //*chroma = encoder->chroma;
  }
}


heif_error mask_encode_image(void* encoder_raw, const heif_image* image,
                             heif_image_input_class input_class)
{
  //struct encoder_struct_mask* encoder = (struct encoder_struct_mask*) encoder_raw;

  // Note: this is not used. It is a dummy plugin.

  return heif_error_ok;
}


heif_error mask_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                    heif_encoded_data_type* type)
{
  return heif_error_ok;
}


static const heif_encoder_plugin encoder_plugin_mask
    {
        /* plugin_api_version */ 3,
        /* compression_format */ heif_compression_mask,
        /* id_name */ "mask",
        /* priority */ PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ mask_plugin_name,
        /* init_plugin */ mask_init_plugin,
        /* cleanup_plugin */ mask_cleanup_plugin,
        /* new_encoder */ mask_new_encoder,
        /* free_encoder */ mask_free_encoder,
        /* set_parameter_quality */ mask_set_parameter_quality,
        /* get_parameter_quality */ mask_get_parameter_quality,
        /* set_parameter_lossless */ mask_set_parameter_lossless,
        /* get_parameter_lossless */ mask_get_parameter_lossless,
        /* set_parameter_logging_level */ mask_set_parameter_logging_level,
        /* get_parameter_logging_level */ mask_get_parameter_logging_level,
        /* list_parameters */ mask_list_parameters,
        /* set_parameter_integer */ mask_set_parameter_integer,
        /* get_parameter_integer */ mask_get_parameter_integer,
        /* set_parameter_boolean */ mask_set_parameter_boolean,
        /* get_parameter_boolean */ mask_get_parameter_boolean,
        /* set_parameter_string */ mask_set_parameter_string,
        /* get_parameter_string */ mask_get_parameter_string,
        /* query_input_colorspace */ mask_query_input_colorspace,
        /* encode_image */ mask_encode_image,
        /* get_compressed_data */ mask_get_compressed_data,
        /* query_input_colorspace (v2) */ mask_query_input_colorspace2,
        /* query_encoded_size (v3) */ nullptr
    };

const heif_encoder_plugin* get_encoder_plugin_mask()
{
  return &encoder_plugin_mask;
}


#if 0 // PLUGIN_mask_ENCODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_mask
};
#endif
