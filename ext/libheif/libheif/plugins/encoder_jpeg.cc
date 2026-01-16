/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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
#include "encoder_jpeg.h"
#include <vector>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <memory>
#include <cstdio>
#include <csetjmp>

extern "C" {
#include <jpeglib.h>
}


struct encoder_struct_jpeg
{
  int quality;

  // --- output

  std::vector<uint8_t> compressed_data;
  bool data_read = false;
};


static const int JPEG_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void jpeg_set_default_parameters(void* encoder);


#define xstr(s) str(s)
#define str(s) #s

static const char* jpeg_plugin_name()
{
#ifdef LIBJPEG_TURBO_VERSION
  snprintf(plugin_name, MAX_PLUGIN_NAME_LENGTH - 1, "libjpeg-turbo " xstr(LIBJPEG_TURBO_VERSION) " (libjpeg %d.%d)", JPEG_LIB_VERSION / 10, JPEG_LIB_VERSION % 10);
  plugin_name[MAX_PLUGIN_NAME_LENGTH - 1] = 0;
#else
  sprintf(plugin_name, "libjpeg %d.%d", JPEG_LIB_VERSION/10, JPEG_LIB_VERSION%10);
#endif

  return plugin_name;
}

#define MAX_NPARAMETERS 10

static heif_encoder_parameter jpeg_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* jpeg_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void jpeg_init_parameters()
{
  heif_encoder_parameter* p = jpeg_encoder_params;
  const heif_encoder_parameter** d = jpeg_encoder_parameter_ptrs;
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

  /*
  assert(i < MAX_NPARAMETERS);
  p->version = 2;
  p->name = heif_encoder_parameter_name_lossless;
  p->type = heif_encoder_parameter_type_boolean;
  p->boolean.default_value = false;
  p->has_default = true;
  d[i++] = p++;
*/

  d[i++] = nullptr;
}


const heif_encoder_parameter** jpeg_list_parameters(void* encoder)
{
  return jpeg_encoder_parameter_ptrs;
}

static void jpeg_init_plugin()
{
  jpeg_init_parameters();
}


static void jpeg_cleanup_plugin()
{
}

heif_error jpeg_new_encoder(void** enc)
{
  auto* encoder = new encoder_struct_jpeg();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  jpeg_set_default_parameters(encoder);

  return err;
}

void jpeg_free_encoder(void* encoder_raw)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  delete encoder;
}


heif_error jpeg_set_parameter_quality(void* encoder_raw, int quality)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

heif_error jpeg_get_parameter_quality(void* encoder_raw, int* quality)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

heif_error jpeg_set_parameter_lossless(void* encoder_raw, int enable)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (enable) {
    encoder->quality = 100; // not really lossless, but the best we can do
  }

  return heif_error_ok;
}

heif_error jpeg_get_parameter_lossless(void* encoder_raw, int* enable)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  *enable = (encoder->quality == 100);  // not really correct, but matches the setting above

  return heif_error_ok;
}

heif_error jpeg_set_parameter_logging_level(void* encoder_raw, int logging)
{
  return heif_error_ok;
}

heif_error jpeg_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
  *loglevel = 0;

  return heif_error_ok;
}

#define set_value(paramname, paramvar) if (strcmp(name, paramname)==0) { encoder->paramvar = value; return heif_error_ok; }
#define get_value(paramname, paramvar) if (strcmp(name, paramname)==0) { *value = encoder->paramvar; return heif_error_ok; }


heif_error jpeg_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_jpeg* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return jpeg_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return jpeg_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

heif_error jpeg_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return jpeg_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return jpeg_get_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}


heif_error jpeg_set_parameter_boolean(void* encoder_raw, const char* name, int value)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return jpeg_set_parameter_lossless(encoder, value);
  }

  //set_value(kParam_realtime, realtime_mode);

  return heif_error_unsupported_parameter;
}

heif_error jpeg_get_parameter_boolean(void* encoder_raw, const char* name, int* value)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return jpeg_get_parameter_lossless(encoder, value);
  }

  //get_value(kParam_realtime, realtime_mode);

  return heif_error_unsupported_parameter;
}


heif_error jpeg_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  //auto* encoder = (struct encoder_struct_jpeg*) encoder_raw;

  return heif_error_unsupported_parameter;
}


heif_error jpeg_get_parameter_string(void* encoder_raw, const char* name,
                                     char* value, int value_size)
{
  //auto* encoder = (struct encoder_struct_jpeg*) encoder_raw;

  return heif_error_unsupported_parameter;
}


static void jpeg_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = jpeg_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          jpeg_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          jpeg_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          // NOLINTNEXTLINE(build/include_what_you_use)
          jpeg_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


void jpeg_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
{
  *colorspace = heif_colorspace_YCbCr;
  *chroma = heif_chroma_420;
}


void jpeg_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  //auto* encoder = (struct encoder_struct_jpeg*) encoder_raw;

  // TODO: support encoding greyscale JPEGs

  *colorspace = heif_colorspace_YCbCr;
  *chroma = heif_chroma_420;
}


void jpeg_query_encoded_size(void* encoder_raw, uint32_t input_width, uint32_t input_height,
                             uint32_t* encoded_width, uint32_t* encoded_height)
{
  *encoded_width = input_width;
  *encoded_height = input_height;
}


struct ErrorHandler
{
  jpeg_error_mgr pub;  /* "public" fields */
  jmp_buf setjmp_buffer;  /* for return to caller */
};

static void OnJpegError(j_common_ptr cinfo)
{
  ErrorHandler* handler = reinterpret_cast<ErrorHandler*>(cinfo->err);
  longjmp(handler->setjmp_buffer, 1);
}


heif_error jpeg_encode_image(void* encoder_raw, const heif_image* image,
                             heif_image_input_class input_class)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;


  jpeg_compress_struct cinfo;
  ErrorHandler jerr;
  cinfo.err = jpeg_std_error(reinterpret_cast<jpeg_error_mgr*>(&jerr));
  jerr.pub.error_exit = &OnJpegError;
  if (setjmp(jerr.setjmp_buffer)) {
    cinfo.err->output_message(reinterpret_cast<j_common_ptr>(&cinfo));
    jpeg_destroy_compress(&cinfo);
    return heif_error{heif_error_Encoding_error, heif_suberror_Encoder_encoding, "JPEG encoding error"};
  }

  if (heif_image_get_bits_per_pixel(image, heif_channel_Y) != 8) {
    jpeg_destroy_compress(&cinfo);
    return heif_error{heif_error_Encoding_error, heif_suberror_Encoder_encoding, "Cannot write JPEG image with >8 bpp."};
  }

  uint8_t* outbuffer = nullptr;
#if defined(LIBJPEG_TURBO_VERSION) || (JPEG_LIB_VERSION_MAJOR < 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR < 4))
  unsigned long outlength = 0;
#else
  size_t outlength = 0;
#endif

  jpeg_create_compress(&cinfo);
  jpeg_mem_dest(&cinfo, &outbuffer, &outlength);

  cinfo.image_width = heif_image_get_width(image, heif_channel_Y);
  cinfo.image_height = heif_image_get_height(image, heif_channel_Y);
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_YCbCr;
  jpeg_set_defaults(&cinfo);
  static const boolean kForceBaseline = TRUE;
  jpeg_set_quality(&cinfo, encoder->quality, kForceBaseline);
  static const boolean kWriteAllTables = TRUE;
  jpeg_start_compress(&cinfo, kWriteAllTables);



  size_t stride_y;
  const uint8_t* row_y = heif_image_get_plane_readonly2(image, heif_channel_Y,
                                                        &stride_y);
  size_t stride_u;
  const uint8_t* row_u = heif_image_get_plane_readonly2(image, heif_channel_Cb,
                                                        &stride_u);
  size_t stride_v;
  const uint8_t* row_v = heif_image_get_plane_readonly2(image, heif_channel_Cr,
                                                        &stride_v);

  JSAMPARRAY buffer = cinfo.mem->alloc_sarray(
      reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE,
      cinfo.image_width * cinfo.input_components, 1);
  JSAMPROW row[1] = {buffer[0]};

  while (cinfo.next_scanline < cinfo.image_height) {
    size_t offset_y = cinfo.next_scanline * stride_y;
    const uint8_t* start_y = &row_y[offset_y];
    size_t offset_u = (cinfo.next_scanline / 2) * stride_u;
    const uint8_t* start_u = &row_u[offset_u];
    size_t offset_v = (cinfo.next_scanline / 2) * stride_v;
    const uint8_t* start_v = &row_v[offset_v];

    JOCTET* bufp = buffer[0];
    for (JDIMENSION x = 0; x < cinfo.image_width; ++x) {
      *bufp++ = start_y[x];
      *bufp++ = start_u[x / 2];
      *bufp++ = start_v[x / 2];
    }
    jpeg_write_scanlines(&cinfo, row, 1);
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  encoder->data_read = false;
  encoder->compressed_data.resize(outlength);
  memcpy(encoder->compressed_data.data(), outbuffer, outlength);

  free(outbuffer);

  return heif_error_ok;
}


heif_error jpeg_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                    heif_encoded_data_type* type)
{
  auto* encoder = (encoder_struct_jpeg*) encoder_raw;

  if (encoder->data_read) {
    *data = nullptr;
    *size = 0;
  }
  else {
    *data = encoder->compressed_data.data();
    *size = (int) encoder->compressed_data.size();
    encoder->data_read = true;
  }

  return heif_error_ok;
}


heif_error jpeg_start_sequence_encoding(void* encoder, const heif_image* image,
                                       enum heif_image_input_class image_class,
                                       uint32_t framerate_num, uint32_t framerate_denom,
                                       const heif_sequence_encoding_options* options)
{
  return heif_error_ok;
}

heif_error jpeg_encode_sequence_frame(void* encoder, const heif_image* image, uintptr_t frame_nr)
{
  return jpeg_encode_image(encoder, image, heif_image_input_class_normal);
}

heif_error jpeg_end_sequence_encoding(void* encoder)
{
  return heif_error_ok;
}

heif_error jpeg_get_compressed_data2(void* encoder, uint8_t** data, int* size,
                                    uintptr_t* frame_nr,
                                    int* is_keyframe, int* more_frame_packets)
{
  heif_error err = jpeg_get_compressed_data(encoder, data, size, nullptr);

  if (is_keyframe) {
    *is_keyframe = true;
  }

  if (more_frame_packets) {
    *more_frame_packets = true;
  }

  return err;
}


static const heif_encoder_plugin encoder_plugin_jpeg
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_JPEG,
        /* id_name */ "jpeg",
        /* priority */ JPEG_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ false,
        /* get_plugin_name */ jpeg_plugin_name,
        /* init_plugin */ jpeg_init_plugin,
        /* cleanup_plugin */ jpeg_cleanup_plugin,
        /* new_encoder */ jpeg_new_encoder,
        /* free_encoder */ jpeg_free_encoder,
        /* set_parameter_quality */ jpeg_set_parameter_quality,
        /* get_parameter_quality */ jpeg_get_parameter_quality,
        /* set_parameter_lossless */ jpeg_set_parameter_lossless,
        /* get_parameter_lossless */ jpeg_get_parameter_lossless,
        /* set_parameter_logging_level */ jpeg_set_parameter_logging_level,
        /* get_parameter_logging_level */ jpeg_get_parameter_logging_level,
        /* list_parameters */ jpeg_list_parameters,
        /* set_parameter_integer */ jpeg_set_parameter_integer,
        /* get_parameter_integer */ jpeg_get_parameter_integer,
        /* set_parameter_boolean */ jpeg_set_parameter_boolean,
        /* get_parameter_boolean */ jpeg_get_parameter_boolean,
        /* set_parameter_string */ jpeg_set_parameter_string,
        /* get_parameter_string */ jpeg_get_parameter_string,
        /* query_input_colorspace */ jpeg_query_input_colorspace,
        /* encode_image */ jpeg_encode_image,
        /* get_compressed_data */ jpeg_get_compressed_data,
        /* query_input_colorspace (v2) */ jpeg_query_input_colorspace2,
        /* query_encoded_size (v3) */ jpeg_query_encoded_size,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ jpeg_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ jpeg_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ jpeg_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ jpeg_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 1
    };

const heif_encoder_plugin* get_encoder_plugin_jpeg()
{
  return &encoder_plugin_jpeg;
}


#if PLUGIN_JPEG_ENCODER
heif_plugin_info plugin_info{
    1,
    heif_plugin_type_encoder,
    &encoder_plugin_jpeg
};
#endif
