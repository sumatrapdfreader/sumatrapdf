/*
 * OpenJPH codec.
 * Copyright (c) 2023 Devon Sookhoo
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 * Copyright (C) 2024 Brad Hards
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

// Portions of this code adapted from OpenJPH's ojph_compress.cpp
// which is under the following license:
//***************************************************************************/
// This software is released under the 2-Clause BSD license, included
// below.
//
// Copyright (c) 2019, Aous Naman 
// Copyright (c) 2019, Kakadu Software Pty Ltd, Australia
// Copyright (c) 2019, The University of New South Wales, Australia
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//***************************************************************************/


#include "libheif/heif.h"
#include "libheif/heif_plugin.h"
#include "encoder_openjph.h"

#include "openjph/ojph_mem.h"
#include "openjph/ojph_defs.h"
#include "openjph/ojph_file.h"
#include "openjph/ojph_codestream.h"
#include "openjph/ojph_params.h"
#include "openjph/ojph_version.h"

#include <string.h>

#include <cassert>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <iostream>

static const int OJPH_PLUGIN_PRIORITY = 80;


struct encoder_struct_ojph
{
  // We do this for API reasons. Has no effect at this stage.
  int quality = 70;
  heif_chroma chroma = heif_chroma_undefined;

  // Context
  ojph::codestream codestream;
  std::string comment;

  // --- output
  bool data_read = false;
  ojph::mem_outfile outfile;
};


#define MAX_NPARAMETERS 10
static heif_encoder_parameter ojph_encoder_params[MAX_NPARAMETERS];
const static heif_encoder_parameter* ojph_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static const char* kParam_chroma = "chroma";
static const char* const kParam_chroma_valid_values[] = {
    "420", "422", "444", nullptr
};

static const char* kParam_num_decompositions = "num_decompositions";
static const int NUM_DECOMPOSITIONS_MIN = 0;
static const int NUM_DECOMPOSITIONS_MAX = 32;

static const char* kParam_progression_order = "progression_order";
static const char* const kParam_progression_order_valid_values[] = {
    "LRCP", "RLCP", "RPCL", "PCRL", "CPRL", nullptr
};

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
static const char* kParam_tlm_marker = "tlm_marker";
#endif

static const char* kParam_codestream_comment = "codestream_comment";

static const char* kParam_tile_size = "tile_size";

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 11
static const char* kParam_tilepart_division = "tilepart_division";
static const char* const kParam_tilepart_division_valid_values[] = {
    "none", "resolution", "component", "both", nullptr
};
#endif

static const char* kParam_block_dimensions = "block_dimensions";

static void ojph_init_encoder_parameters()
{
  heif_encoder_parameter* p = ojph_encoder_params;
  const heif_encoder_parameter** d = ojph_encoder_parameter_ptrs;
  int i = 0;

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
  p->string.default_value = "444";
  p->has_default = true;
  p->string.valid_values = kParam_chroma_valid_values;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_num_decompositions;
  p->type = heif_encoder_parameter_type_integer;
  p->integer.default_value = 5;
  p->integer.have_minimum_maximum = true;
  p->integer.minimum = NUM_DECOMPOSITIONS_MIN;
  p->integer.maximum = NUM_DECOMPOSITIONS_MAX;
  p->integer.valid_values = NULL;
  p->integer.num_valid_values = 0;
  p->has_default = true;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_progression_order;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "RPCL";
  p->has_default = true;
  p->string.valid_values = kParam_progression_order_valid_values;
  d[i++] = p++;

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_tlm_marker;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;
#endif

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_codestream_comment;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = nullptr;
  p->has_default = false;
  p->string.valid_values = nullptr;
  d[i++] = p++;

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_tile_size;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "0,0";
  p->has_default = true;
  p->string.valid_values = nullptr;
  d[i++] = p++;

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 11
  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_tilepart_division;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "none";
  p->has_default = true;
  p->string.valid_values = kParam_tilepart_division_valid_values;
  d[i++] = p++;
#endif

  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = kParam_block_dimensions;
  p->type = heif_encoder_parameter_type_string;
  p->string.default_value = "64,64";
  p->has_default = true;
  p->string.valid_values = nullptr;
  d[i++] = p++;

  d[i++] = nullptr;
}


void ojph_init_plugin()
{
  ojph_init_encoder_parameters();
}

void ojph_cleanup_plugin()
{
}


//////////// Integer parameter setters

// Note quality is part of the plugin API.

heif_error ojph_set_parameter_quality(void* encoder_raw, int quality)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;

  encoder->quality = quality;

  return heif_error_ok;
}

static const heif_error &ojph_set_num_decompositions(int value, encoder_struct_ojph *encoder)
{
  if ((value < NUM_DECOMPOSITIONS_MIN) || (value > NUM_DECOMPOSITIONS_MAX)) {
    return heif_error_invalid_parameter_value;
  }
  encoder->codestream.access_cod().set_num_decomposition(value);
  return heif_error_ok;
}

heif_error ojph_set_parameter_integer(void *encoder_raw, const char *name, int value)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return  ojph_set_parameter_quality(encoder, value);
  } else if (strcmp(name, kParam_num_decompositions) == 0) {
    return ojph_set_num_decompositions(value, encoder);
  } else {
    return heif_error_unsupported_parameter;
  }
}


//////////// Integer parameter getters

// Note quality is part of the plugin API

heif_error ojph_get_parameter_quality(void* encoder_raw, int* quality)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

const heif_error &ojph_get_parameter_num_decompositions(encoder_struct_ojph *encoder, int *value)
{
  *value = encoder->codestream.access_cod().get_num_decompositions();
  return heif_error_ok;
}

heif_error ojph_get_parameter_integer(void *encoder_raw, const char *name, int *value)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return ojph_get_parameter_quality(encoder, value);
  } else if (strcmp(name, kParam_num_decompositions) == 0) {
    return ojph_get_parameter_num_decompositions(encoder, value);
  } else {
    return heif_error_unsupported_parameter;
  }
}


//////////// Boolean parameter setters

// Note lossless is part of the plugin API

heif_error ojph_set_parameter_lossless(void* encoder_raw, int lossless)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;
  encoder->codestream.access_cod().set_reversible(lossless);
  return heif_error_ok;
}

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
const heif_error &ojph_set_tlm_marker_requested(encoder_struct_ojph *encoder, int value)
{
  encoder->codestream.request_tlm_marker(value);
  return heif_error_ok;
}
#endif

heif_error ojph_set_parameter_boolean(void *encoder_raw, const char *name, int value)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return ojph_set_parameter_lossless(encoder, value);
#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
  } else if (strcmp(name, kParam_tlm_marker) == 0) {
    return ojph_set_tlm_marker_requested(encoder, value);
#endif
  }
  return heif_error_unsupported_parameter;
}

//////////// Boolean parameter getters

// Note lossless is part of the plugin API

heif_error ojph_get_parameter_lossless(void* encoder_raw, int* lossless)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;
  *lossless = encoder->codestream.access_cod().is_reversible();
  return heif_error_ok;
}

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
const heif_error &ojph_get_parameter_tlm_marker(encoder_struct_ojph *encoder, int *value)
{
  *value = encoder->codestream.is_tlm_requested();
  return heif_error_ok;
}
#endif

heif_error ojph_get_parameter_boolean(void *encoder_raw, const char *name, int *value)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return ojph_get_parameter_lossless(encoder, value);
#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
  } else if (strcmp(name, kParam_tlm_marker) == 0) {
    return ojph_get_parameter_tlm_marker(encoder, value);
#endif
  } else {
    return heif_error_unsupported_parameter;
  }
}


//////////// String parameter getters

static void safe_strcpy(char* dst, int dst_size, const char* src)
{
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = 0;
}

const heif_error &ojph_get_parameter_chroma(encoder_struct_ojph *encoder, char *value, int value_size)
{
  switch (encoder->chroma) {
    case heif_chroma_420:
      safe_strcpy(value, value_size, "420");
      break;
    case heif_chroma_422:
      safe_strcpy(value, value_size, "422");
      break;
    case heif_chroma_444:
      safe_strcpy(value, value_size, "444");
      break;
    case heif_chroma_undefined:
      safe_strcpy(value, value_size, "undefined");
      break;
    default:
      assert(false);
      return heif_error_invalid_parameter_value;
  }
  return heif_error_ok;
}

const heif_error &ojph_get_parameter_progression_order(encoder_struct_ojph *encoder, char *value, int value_size)
{
  safe_strcpy(value, value_size, encoder->codestream.access_cod().get_progression_order_as_string());
  return heif_error_ok;
}

const heif_error &ojph_get_parameter_codestream_comment(encoder_struct_ojph *encoder, char *value, int value_size)
{
  safe_strcpy(value, value_size, encoder->comment.c_str());
  return heif_error_ok;
}

const heif_error &ojph_get_parameter_tile_size(encoder_struct_ojph *encoder, char *value, int value_size)
{
  ojph::size tile_size = encoder->codestream.access_siz().get_tile_size();
  std::stringstream stringStream;
  stringStream << tile_size.w << "," << tile_size.h;
  safe_strcpy(value, value_size, stringStream.str().c_str());
  return heif_error_ok;
}

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 11
const heif_error &ojph_get_parameter_tilepart_division(encoder_struct_ojph *encoder, char *value, int value_size)
{
  bool res = encoder->codestream.is_tilepart_division_at_resolutions();
  bool comp = encoder->codestream.is_tilepart_division_at_components();
  if (res && comp) {
    safe_strcpy(value, value_size, "both");
  } else if (res) {
    safe_strcpy(value, value_size, "resolution");
  } else if (comp) {
    safe_strcpy(value, value_size, "component");
  } else {
    safe_strcpy(value, value_size, "none");
  }
  return heif_error_ok;
}
#endif

const heif_error &ojph_get_parameter_block_dimensions(encoder_struct_ojph *encoder, char *value, int value_size)
{
  ojph::size block_dims = encoder->codestream.access_cod().get_block_dims();
  std::stringstream stringStream;
  stringStream << block_dims.w << "," << block_dims.h;
  safe_strcpy(value, value_size, stringStream.str().c_str());
  return heif_error_ok;
}

heif_error ojph_get_parameter_string(void *encoder_raw, const char *name, char *value, int value_size)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;

  if (strcmp(name, kParam_chroma) == 0) {
    return ojph_get_parameter_chroma(encoder, value, value_size);
  } else if (strcmp(name, kParam_progression_order) == 0) {
    return ojph_get_parameter_progression_order(encoder, value, value_size);
  } else if (strcmp(name, kParam_codestream_comment) == 0) {
    return ojph_get_parameter_codestream_comment(encoder, value, value_size);
  } else if (strcmp(name, kParam_tile_size) == 0) {
    return ojph_get_parameter_tile_size(encoder, value, value_size);
#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 11
  } else if (strcmp(name, kParam_tilepart_division) == 0) {
    return ojph_get_parameter_tilepart_division(encoder, value, value_size);
#endif
  } else if (strcmp(name, kParam_block_dimensions) == 0) {
    return ojph_get_parameter_block_dimensions(encoder, value, value_size);
  } else {
    return heif_error_unsupported_parameter;
  }
}

//////////// String parameter setters

static const heif_error &ojph_set_chroma(encoder_struct_ojph *encoder, const char *value)
{
  if (strcmp(value, "420") == 0) {
    encoder->chroma = heif_chroma_420;
    return heif_error_ok;
  } else if (strcmp(value, "422") == 0) {
    encoder->chroma = heif_chroma_422;
    return heif_error_ok;
  } else if (strcmp(value, "444") == 0) {
    encoder->chroma = heif_chroma_444;
    return heif_error_ok;
  } else {
    return heif_error_invalid_parameter_value;
  }
}

static bool string_list_contains(const char* const* values_list, const char* value)
{
  for (int i = 0; values_list[i]; i++) {
    if (strcmp(values_list[i], value) == 0) {
      return true;
    }
  }

  return false;
}

static const heif_error &ojph_set_progression_order(encoder_struct_ojph *encoder, const char *value)
{
  if (string_list_contains(kParam_progression_order_valid_values, value)) {
    encoder->codestream.access_cod().set_progression_order(value);
    return heif_error_ok;
  } else {
    return heif_error_invalid_parameter_value;
  }
}

static const heif_error &ojph_set_codestream_comment(encoder_struct_ojph *encoder, const char *value)
{
  if (value != nullptr) {
    encoder->comment = std::string(value);
  }
  return heif_error_ok;
}

static const heif_error &ojph_set_tile_size(encoder_struct_ojph *encoder, const char *value)
{
  std::string valueStr(value);
  size_t commaOffset = valueStr.find(",");
  if (commaOffset == std::string::npos) {
    return heif_error_invalid_parameter_value;
  }
  std::string xTSizText = valueStr.substr(0, commaOffset);
  unsigned long xTSiz = std::stoul(xTSizText);
  std::string yTSizText = valueStr.substr(commaOffset + 1);
  unsigned long yTSiz = std::stoul(yTSizText);
  if ((xTSiz < 1)
    || (xTSiz > std::numeric_limits<unsigned int>::max())
    || (yTSiz < 1)
    || (yTSiz > std::numeric_limits<unsigned int>::max())) {
    return heif_error_invalid_parameter_value;
  }
  encoder->codestream.access_siz().set_tile_size(ojph::size((ojph::ui32)xTSiz, (ojph::ui32)yTSiz));
  return heif_error_ok;
}

#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 11
static const heif_error &ojph_set_tilepart_division(encoder_struct_ojph *encoder, const char *value)
{
  if (strcmp(value, "none") == 0) {
    encoder->codestream.set_tilepart_divisions(false, false);
    return heif_error_ok;
  } else if (strcmp(value, "resolution") == 0) {
    encoder->codestream.set_tilepart_divisions(true, false);
    return heif_error_ok;
  } else if (strcmp(value, "component") == 0) {
    encoder->codestream.set_tilepart_divisions(false, true);
    return heif_error_ok;
  } else if (strcmp(value, "both") == 0) {
    encoder->codestream.set_tilepart_divisions(true, true);
    return heif_error_ok;
  } else {
    return heif_error_invalid_parameter_value;
  }
}
#endif

// Get the base 2 logarithm for code block sizes. See ITU-T T.800 (11/2015) Table A.18
// Values are encoded 0 to 8 (i.e. its -2)
static const int log_base_2(unsigned long v)
{
  switch (v) {
    case 4:
      return 2 - 2;
    case 8:
      return 3 - 2;
    case 16:
      return 4 - 2;
    case 32:
      return 5 - 2;
    case 64:
      return 6 - 2;
    case 128:
      return 7 - 2;
    case 256:
      return 8 - 2;
    case 512:
      return 9 - 2;
    case 1024:
      return 10 - 2;
    default:
      // any other value is invalid
      return -1;
  }
}

static const heif_error &ojph_set_block_dimensions(encoder_struct_ojph *encoder, const char *value)
{
  std::string valueStr(value);
  size_t commaOffset = valueStr.find(",");
  if (commaOffset == std::string::npos) {
    return heif_error_invalid_parameter_value;
  }
  std::string widthText = valueStr.substr(0, commaOffset);
  unsigned long width = std::stoul(widthText);
  std::string heightText = valueStr.substr(commaOffset + 1);
  unsigned long height = std::stoul(heightText);
  int xcb = log_base_2(width);
  int ycb = log_base_2(height);
  if ((xcb == -1) || (ycb == -1) || (xcb + ycb > 12)) {
    return heif_error_invalid_parameter_value;
  }
  encoder->codestream.access_cod().set_block_dims((ojph::ui32)width, (ojph::ui32)height);
  return heif_error_ok;
}

heif_error ojph_set_parameter_string(void *encoder_raw, const char *name, const char *value)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;

  if (strcmp(name, kParam_chroma) == 0) {
    return ojph_set_chroma(encoder, value);
  } else if (strcmp(name, kParam_progression_order) == 0) {
    return ojph_set_progression_order(encoder, value);
  } else if (strcmp(name, kParam_codestream_comment) == 0) {
    return ojph_set_codestream_comment(encoder, value);
  } else if (strcmp(name, kParam_tile_size) == 0) {
    return ojph_set_tile_size(encoder, value);
#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 11
  } else if (strcmp(name, kParam_tilepart_division) == 0) {
    return ojph_set_tilepart_division(encoder, value);
#endif
  } else if (strcmp(name, kParam_block_dimensions) == 0) {
    return ojph_set_block_dimensions(encoder, value);
  } else {
    return heif_error_unsupported_parameter;
  }
}

static void ojph_set_default_parameters(void* encoder_raw)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;
  for (const heif_encoder_parameter** p = ojph_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          ojph_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          ojph_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          ojph_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}

///// Actual encoding functionality

heif_error ojph_new_encoder(void** encoder_out)
{
  encoder_struct_ojph* encoder = new encoder_struct_ojph();
  encoder->outfile.open();
  *encoder_out = encoder;

  ojph_set_default_parameters(encoder);

  return heif_error_ok;
}

void ojph_free_encoder(void* encoder_raw)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;
  encoder->codestream.close();
  delete encoder;
}

heif_error ojph_set_parameter_logging_level(void* encoder, int logging)
{
  // No logging level options in OpenJPH
  return heif_error_ok;
}

heif_error ojph_get_parameter_logging_level(void* encoder, int* logging)
{
  // No logging level options in OpenJPH
  return heif_error_ok;
}

const heif_encoder_parameter** ojph_list_parameters(void* encoder_raw)
{
  return ojph_encoder_parameter_ptrs;
}


void ojph_query_input_colorspace(heif_colorspace* inout_colorspace, heif_chroma* inout_chroma)
{
  // Replace the input colorspace/chroma with the one that is supported by the encoder and that
  // comes as close to the input colorspace/chroma as possible.

  if (*inout_colorspace == heif_colorspace_monochrome) {
    *inout_colorspace = heif_colorspace_monochrome;
    *inout_chroma = heif_chroma_monochrome;
  }
  else {
    *inout_colorspace = heif_colorspace_YCbCr;
    *inout_chroma = heif_chroma_444;
  }
}

void ojph_query_input_colorspace2(void* encoder_raw, heif_colorspace* inout_colorspace, heif_chroma* inout_chroma)
{
  auto* encoder = (encoder_struct_ojph*) encoder_raw;

  if (*inout_colorspace == heif_colorspace_monochrome) {
    *inout_colorspace = heif_colorspace_monochrome;
    *inout_chroma = heif_chroma_monochrome;
  }
  else {
    *inout_colorspace = heif_colorspace_YCbCr;

    if (encoder->chroma != heif_chroma_undefined) {
      *inout_chroma = encoder->chroma;
    }
    else {
      *inout_chroma = heif_chroma_444;
    }
  }
}

std::vector<heif_channel> build_SIZ(encoder_struct_ojph *encoder, const heif_image *image)
{
  std::vector<heif_channel> sourceChannels;
  ojph::param_siz siz = encoder->codestream.access_siz();
  int width = heif_image_get_primary_width(image);
  int height = heif_image_get_primary_height(image);
  siz.set_image_extent(ojph::point(width, height));

  heif_chroma chroma = heif_image_get_chroma_format(image);

  encoder->codestream.set_planar(true);
  sourceChannels = {heif_channel_Y, heif_channel_Cb, heif_channel_Cr};
  siz.set_num_components((ojph::ui32)sourceChannels.size());
  for (ojph::ui32 i = 0; i < siz.get_num_components(); i++) {
    int bit_depth = heif_image_get_bits_per_pixel_range(image, sourceChannels[i]);
    if (sourceChannels[i] == heif_channel_Y) {
      siz.set_component(i, ojph::point(1, 1), bit_depth, false);
    } else { // Cb or Cr
      if (chroma == heif_chroma_444) {
        siz.set_component(i, ojph::point(1, 1), bit_depth, false);
      } else if (chroma == heif_chroma_422) {
        siz.set_component(i, ojph::point(2, 1), bit_depth, false);
      } else {
        siz.set_component(i, ojph::point(2, 2), bit_depth, false);
      }
    }
  }
  siz.set_image_offset(ojph::point(0, 0));
  siz.set_tile_offset(ojph::point(0, 0));
  return sourceChannels;
}

void build_COD(encoder_struct_ojph *encoder)
{
  ojph::param_cod cod = encoder->codestream.access_cod();
  cod.set_color_transform(false);
}

heif_error ojph_encode_image(void *encoder_raw, const heif_image *image, heif_image_input_class image_class)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;

  if (heif_image_get_colorspace(image) != heif_colorspace_YCbCr) {
    return {
      heif_error_Encoding_error,
      heif_suberror_Unspecified,
      "OpenJPH encoder plugin received image with invalid colorspace."
    };
  }

  // reset output position to start
  encoder->outfile.seek(0, ojph::outfile_base::seek::OJPH_SEEK_SET);
  encoder->data_read = false;

  std::vector<heif_channel> sourceChannels = build_SIZ(encoder, image);
  build_COD(encoder);
#if OPENJPH_MAJOR_VERSION > 1 || OPENJPH_MINOR_VERSION > 10
  bool hasComment = (encoder->comment.length() > 0);
  ojph::comment_exchange com_ex;
  if (hasComment) {
    com_ex.set_string(encoder->comment.c_str());
  }
  encoder->codestream.write_headers(&(encoder->outfile), &com_ex, hasComment ? 1 : 0);
#else
  encoder->codestream.write_headers(&(encoder->outfile));
#endif
  ojph::ui32 next_comp;
  ojph::line_buf* cur_line = encoder->codestream.exchange(NULL, next_comp);

  for (const auto& sourceChannel : sourceChannels) {
    size_t stride;
    const uint8_t *data = heif_image_get_plane_readonly2(image, sourceChannel, &stride);
    uint32_t component_height = heif_image_get_height(image, sourceChannel);
    for (uint32_t y = 0; y < component_height; y++) {
      const uint8_t *sourceLine = data + y * stride;
      size_t outputWidth = cur_line->size;
      ojph::si32 *targetLine = cur_line->i32;
      for (uint32_t x = 0; x < outputWidth; x++) {
        targetLine[x] = sourceLine[x];
      }
      cur_line = encoder->codestream.exchange(cur_line, next_comp);
    }
  }
  encoder->codestream.flush();

  return heif_error_ok;
}

heif_error ojph_get_compressed_data(void* encoder_raw, uint8_t** data, int* size, heif_encoded_data_type* type)
{
  encoder_struct_ojph* encoder = (encoder_struct_ojph*) encoder_raw;

  if (encoder->data_read) {
    *size = 0;
    *data = nullptr;
  }
  else {
    *size = (int) encoder->outfile.tell();
    *data = (uint8_t*) encoder->outfile.get_data();
    encoder->data_read = true;
  }
  return heif_error_ok;
}


static const int MAX_PLUGIN_NAME_LENGTH = 80;
static char plugin_name[MAX_PLUGIN_NAME_LENGTH];

const char* ojph_plugin_name()
{
  snprintf(plugin_name, MAX_PLUGIN_NAME_LENGTH,
      "OpenJPH %s.%s.%s",
      OJPH_INT_TO_STRING(OPENJPH_VERSION_MAJOR),
      OJPH_INT_TO_STRING(OPENJPH_VERSION_MINOR),
      OJPH_INT_TO_STRING(OPENJPH_VERSION_PATCH)
  );
  plugin_name[MAX_PLUGIN_NAME_LENGTH - 1] = 0;

  return plugin_name;
}


heif_error ojph_start_sequence_encoding(void* encoder, const heif_image* image,
                                       enum heif_image_input_class image_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options)
{
  return heif_error_ok;
}

heif_error ojph_encode_sequence_frame(void* encoder, const heif_image* image, uintptr_t frame_nr)
{
  return ojph_encode_image(encoder, image, heif_image_input_class_normal);
}

heif_error ojph_end_sequence_encoding(void* encoder)
{
  return heif_error_ok;
}

heif_error ojph_get_compressed_data2(void* encoder, uint8_t** data, int* size,
                                    uintptr_t* frame_nr,
                                    int* is_keyframe, int* more_frame_packets)
{
  heif_error err = ojph_get_compressed_data(encoder, data, size, nullptr);

  if (is_keyframe) {
    *is_keyframe = true;
  }

  if (more_frame_packets) {
    *more_frame_packets = true;
  }

  return err;
}

static const heif_encoder_plugin encoder_plugin_openjph {
    /* plugin_api_version */ 4,
    /* compression_format */ heif_compression_HTJ2K,
    /* id_name */ "openjph",
    /* priority */ OJPH_PLUGIN_PRIORITY,
    /* supports_lossy_compression */ true,
    /* supports_lossless_compression */ true,
    /* get_plugin_name */ ojph_plugin_name,
    /* init_plugin */ ojph_init_plugin,
    /* cleanup_plugin */ ojph_cleanup_plugin,
    /* new_encoder */ ojph_new_encoder,
    /* free_encoder */ ojph_free_encoder,
    /* set_parameter_quality */ ojph_set_parameter_quality,
    /* get_parameter_quality */ ojph_get_parameter_quality,
    /* set_parameter_lossless */ ojph_set_parameter_lossless,
    /* get_parameter_lossless */ ojph_get_parameter_lossless,
    /* set_parameter_logging_level */ ojph_set_parameter_logging_level,
    /* get_parameter_logging_level */ ojph_get_parameter_logging_level,
    /* list_parameters */ ojph_list_parameters,
    /* set_parameter_integer */ ojph_set_parameter_integer,
    /* get_parameter_integer */ ojph_get_parameter_integer,
    /* set_parameter_boolean */ ojph_set_parameter_boolean,
    /* get_parameter_boolean */ ojph_get_parameter_boolean,
    /* set_parameter_string */ ojph_set_parameter_string,
    /* get_parameter_string */ ojph_get_parameter_string,
    /* query_input_colorspace */ ojph_query_input_colorspace,
    /* encode_image */ ojph_encode_image,
    /* get_compressed_data */ ojph_get_compressed_data,
    /* query_input_colorspace (v2) */ ojph_query_input_colorspace2,
    /* query_encoded_size (v3) */ nullptr,
    /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
    /* start_sequence_encoding (v4) */ ojph_start_sequence_encoding,
    /* encode_sequence_frame (v4) */ ojph_encode_sequence_frame,
    /* end_sequence_encoding (v4) */ ojph_end_sequence_encoding,
    /* get_compressed_data2 (v4) */ ojph_get_compressed_data2,
    /* does_indicate_keyframes (v4) */ 1
};

const heif_encoder_plugin* get_encoder_plugin_openjph()
{
  return &encoder_plugin_openjph;
}


#if PLUGIN_OPENJPH_ENCODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_openjph
};
#endif
