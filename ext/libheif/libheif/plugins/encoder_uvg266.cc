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
#include "encoder_uvg266.h"
#include <memory>
#include <string>   // apparently, this is a false positive of cpplint
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>
#include <deque>
#include <iostream>

#include "logging.h"
#include "nalu_utils.h"
#include <utility>

extern "C" {
#include <uvg266.h>
}


static const char* kError_unspecified_error = "Unspecified encoder error";
static const char* kError_unsupported_bit_depth = "Bit depth not supported by uvg266";
static const char* kError_unsupported_chroma = "Unsupported chroma type";
//static const char* kError_unsupported_image_size = "Images smaller than 16 pixels are not supported";


struct encoder_struct_uvg266
{
  int quality = 75;
  bool lossless = false;

  // --- encoder

  const uvg_api* api = nullptr;
  uvg_config* config = nullptr;
  uvg_encoder* kvzencoder = nullptr;

  uvg_chroma_format kvzChroma;
  int chroma_stride_shift = 0;
  int chroma_height_shift = 0;
  int input_chroma_width = 0;
  int input_chroma_height = 0;

  // --- output data

  //std::vector<uint8_t> output_data;
  //size_t output_idx = 0;

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t frameNr = 0;
    bool more_nals = false;
  };

  std::deque<Packet> output_data;

  std::vector<uint8_t> active_data; // holds the data that we just returned

  void append_chunk_data(uvg_data_chunk* data, int framenr);

  ~encoder_struct_uvg266()
  {
    if (kvzencoder) {
      api->encoder_close(kvzencoder);
    }

    if (config) {
      api->config_destroy(config);
    }
  }
};

static const int uvg266_PLUGIN_PRIORITY = 50;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void uvg266_set_default_parameters(void* encoder);


static const char* uvg266_plugin_name()
{
  strcpy(plugin_name, "uvg266 VVC encoder");
  return plugin_name;
}


#define MAX_NPARAMETERS 10

static heif_encoder_parameter uvg266_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* uvg266_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void uvg266_init_parameters()
{
  heif_encoder_parameter* p = uvg266_encoder_params;
  const heif_encoder_parameter** d = uvg266_encoder_parameter_ptrs;
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

  d[i++] = nullptr;
}


const heif_encoder_parameter** uvg266_list_parameters(void* encoder)
{
  return uvg266_encoder_parameter_ptrs;
}


static void uvg266_init_plugin()
{
  uvg266_init_parameters();
}


static void uvg266_cleanup_plugin()
{
}


static heif_error uvg266_new_encoder(void** enc)
{
  encoder_struct_uvg266* encoder = new encoder_struct_uvg266();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  uvg266_set_default_parameters(encoder);

  return err;
}

static void uvg266_free_encoder(void* encoder_raw)
{
  struct encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  delete encoder;
}

static heif_error uvg266_set_parameter_quality(void* encoder_raw, int quality)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

static heif_error uvg266_get_parameter_quality(void* encoder_raw, int* quality)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

static heif_error uvg266_set_parameter_lossless(void* encoder_raw, int enable)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  encoder->lossless = enable ? 1 : 0;

  return heif_error_ok;
}

static heif_error uvg266_get_parameter_lossless(void* encoder_raw, int* enable)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  *enable = encoder->lossless;

  return heif_error_ok;
}

static heif_error uvg266_set_parameter_logging_level(void* encoder_raw, int logging)
{
//  struct encoder_struct_uvg266* encoder = (struct encoder_struct_uvg266*) encoder_raw;

// return heif_error_invalid_parameter_value;

  return heif_error_ok;
}

static heif_error uvg266_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
//  struct encoder_struct_uvg266* encoder = (struct encoder_struct_uvg266*) encoder_raw;

  *loglevel = 0;

  return heif_error_ok;
}


static heif_error uvg266_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return uvg266_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return uvg266_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

static heif_error uvg266_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return uvg266_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return uvg266_get_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}


static heif_error uvg266_set_parameter_boolean(void* encoder, const char* name, int value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return uvg266_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

// Unused, will use "uvg266_get_parameter_integer" instead.
/*
static struct heif_error uvg266_get_parameter_boolean(void* encoder, const char* name, int* value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless)==0) {
    return uvg266_get_parameter_lossless(encoder,value);
  }

  return heif_error_unsupported_parameter;
}
*/


static heif_error uvg266_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  return heif_error_unsupported_parameter;
}

static heif_error uvg266_get_parameter_string(void* encoder_raw, const char* name,
                                              char* value, int value_size)
{
  return heif_error_unsupported_parameter;
}


static void uvg266_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = uvg266_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          uvg266_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          uvg266_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          uvg266_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


static void uvg266_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
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


static void uvg266_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  if (*colorspace == heif_colorspace_monochrome) {
    *colorspace = heif_colorspace_monochrome;
    *chroma = heif_chroma_monochrome;
  }
  else {
    *colorspace = heif_colorspace_YCbCr;
    if (*chroma != heif_chroma_420 &&
        *chroma != heif_chroma_422 &&
        *chroma != heif_chroma_444) {
      *chroma = heif_chroma_420;
    }
  }
}

void uvg266_query_encoded_size(void* encoder_raw, uint32_t input_width, uint32_t input_height,
                               uint32_t* encoded_width, uint32_t* encoded_height)
{
  *encoded_width = (input_width + 7) & ~0x7;
  *encoded_height = (input_height + 7) & ~0x7;
}


#if 0
static int rounded_size(int s)
{
  s = (s + 1) & ~1;

  if (s < 64) {
    s = 64;
  }

  return s;
}
#endif

void encoder_struct_uvg266::append_chunk_data(uvg_data_chunk* data, int framenr)
{
  if (!data || data->len == 0) {
    return;
  }

  std::vector<uint8_t> input_data;
  while (data) {
    input_data.insert(input_data.end(), data->data, data->data + data->len);
    data = data->next;
  }

#if 0
  std::cout << "DATA\n";
  std::cout << write_raw_data_as_hex(input_data.data(), input_data.size(), {}, {});
  std::cout << "---\n";
#endif

  //std::vector<uint8_t>& pktdata = encoder->output_data.front().data;
  size_t start_idx = 0;

  for (;;)
  {
    while (start_idx < input_data.size() - 3 &&
           (input_data[start_idx] != 0 ||
            input_data[start_idx + 1] != 0 ||
            input_data[start_idx + 2] != 1)) {
      start_idx++;
            }

    size_t end_idx = start_idx + 1;

    while (end_idx < input_data.size() - 3 &&
           (input_data[end_idx] != 0 ||
            input_data[end_idx + 1] != 0 ||
            input_data[end_idx + 2] != 1)) {
      end_idx++;
            }

    if (end_idx == input_data.size() - 3) {
      end_idx = input_data.size();
    }

    encoder_struct_uvg266::Packet pkt;
    pkt.data.resize(end_idx - start_idx - 3);
    memcpy(pkt.data.data(), input_data.data() + start_idx + 3, pkt.data.size());
    pkt.frameNr = framenr;
    pkt.more_nals = true; // will be set to 'false' for last NAL below

    // uint8_t nal_type = (pkt.data[1] >> 3);
    // std::cout << "append frameNr=" << framenr << " NAL:" << ((int)nal_type) << " size:" << pkt.data.size() << "\n";

    output_data.emplace_back(std::move(pkt));

    if (end_idx == input_data.size()) {
      break;
    }

    start_idx = end_idx;
  }

  if (!output_data.empty()) {
    output_data.back().more_nals = false;
  }
}


static void copy_plane(uvg_pixel* out_p, size_t out_stride, const uint8_t* in_p, size_t in_stride, int w, int h, int padded_width, int padded_height)
{
  for (int y = 0; y < padded_height; y++) {
    int sy = std::min(y, h - 1); // source y
    memcpy(out_p + y * out_stride, in_p + sy * in_stride, w);

    if (padded_width > w) {
      memset(out_p + y * out_stride + w, *(in_p + sy * in_stride + w - 1), padded_width - w);
    }
  }
}


static heif_error uvg266_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                                        heif_image_input_class input_class,
                                                        uint32_t framerate_num, uint32_t framerate_denom,
                                                        const heif_sequence_encoding_options* options,
                                                        bool image_sequence)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  int bit_depth = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

  const uvg_api* api = uvg_api_get(bit_depth);
  if (api == nullptr) {
    return {
      heif_error_Encoder_plugin_error,
      heif_suberror_Unsupported_bit_depth,
      kError_unsupported_bit_depth
    };
  }

  encoder->api = api;

  uvg_config* config = api->config_alloc();
  api->config_init(config); // param, encoder->preset.c_str(), encoder->tune.c_str());

  encoder->config = config;

#if HAVE_UVG266_ENABLE_LOGGING
  config->enable_logging_output = 0;
#endif

#if !ENABLE_MULTITHREADING_SUPPORT
  // 0: Process everything with main thread
  // -1 (default): Select automatically.
  config->threads = 0;
#endif

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);
  heif_chroma chroma = heif_image_get_chroma_format(image);

  int input_width = heif_image_get_width(image, heif_channel_Y);
  int input_height = heif_image_get_height(image, heif_channel_Y);

  if (isGreyscale) {
    config->input_format = UVG_FORMAT_P400;
    encoder->kvzChroma = UVG_CSP_400;
  }
  else if (chroma == heif_chroma_420) {
    config->input_format = UVG_FORMAT_P420;
    encoder->kvzChroma = UVG_CSP_420;
    encoder->chroma_stride_shift = 1;
    encoder->chroma_height_shift = 1;
    encoder->input_chroma_width = (input_width + 1) / 2;
    encoder->input_chroma_height = (input_height + 1) / 2;
  }
  else if (chroma == heif_chroma_422) {
    config->input_format = UVG_FORMAT_P422;
    encoder->kvzChroma = UVG_CSP_422;
    encoder->chroma_stride_shift = 1;
    encoder->chroma_height_shift = 0;
    encoder->input_chroma_width = (input_width + 1) / 2;
    encoder->input_chroma_height = input_height;
  }
  else if (chroma == heif_chroma_444) {
    config->input_format = UVG_FORMAT_P444;
    encoder->kvzChroma = UVG_CSP_444;
    encoder->chroma_stride_shift = 0;
    encoder->chroma_height_shift = 0;
    encoder->input_chroma_width = input_width;
    encoder->input_chroma_height = input_height;
  }
  else {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Unsupported_image_type,
      kError_unsupported_chroma
    };
  }

  if (chroma != heif_chroma_monochrome) {
    int w = heif_image_get_width(image, heif_channel_Y);
    int h = heif_image_get_height(image, heif_channel_Y);
    if (chroma != heif_chroma_444) { w = (w + 1) / 2; }
    if (chroma == heif_chroma_420) { h = (h + 1) / 2; }

    assert(heif_image_get_width(image, heif_channel_Cb) == w);
    assert(heif_image_get_width(image, heif_channel_Cr) == w);
    assert(heif_image_get_height(image, heif_channel_Cb) == h);
    assert(heif_image_get_height(image, heif_channel_Cr) == h);
    (void) w;
    (void) h;
  }

  heif_color_profile_nclx* nclx = nullptr;
  heif_error err = heif_image_get_nclx_color_profile(image, &nclx);
  if (err.code != heif_error_Ok) {
    nclx = nullptr;
  }

  // make sure NCLX profile is deleted at end of function
  auto nclx_deleter = std::unique_ptr<heif_color_profile_nclx, void (*)(heif_color_profile_nclx*)>(nclx, heif_nclx_color_profile_free);

  if (nclx) {
    config->vui.fullrange = nclx->full_range_flag;
  }
  else {
    config->vui.fullrange = 1;
  }

  if (nclx &&
      (input_class == heif_image_input_class_normal ||
       input_class == heif_image_input_class_thumbnail)) {
    config->vui.colorprim = nclx->color_primaries;
    config->vui.transfer = nclx->transfer_characteristics;
    config->vui.colormatrix = nclx->matrix_coefficients;
  }

  config->qp = ((100 - encoder->quality) * 51 + 50) / 100;
  config->lossless = encoder->lossless ? 1 : 0;

  if (image_sequence) {
    if (options->keyframe_distance_max) {
      config->intra_period = options->keyframe_distance_max;
    }

    switch (options->gop_structure) {
      case heif_sequence_gop_structure_intra_only:
        config->intra_period = 1;
        break;
      case heif_sequence_gop_structure_lowdelay:
        config->gop_lowdelay = 1;
        break;
      case heif_sequence_gop_structure_unrestricted:
        break;
    }
  }

  config->framerate_num = framerate_num;
  config->framerate_denom = framerate_denom;

  uint32_t encoded_width, encoded_height;
  uvg266_query_encoded_size(encoder_raw, input_width, input_height, &encoded_width, &encoded_height);

  config->width = encoded_width;
  config->height = encoded_height;

  uvg_encoder* kvzencoder = api->encoder_open(config);
  if (!kvzencoder) {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Encoder_encoding,
      kError_unspecified_error
    };
  }

  encoder->kvzencoder = kvzencoder;

  return heif_error_ok;
}


static heif_error uvg266_encode_sequence_frame(void* encoder_raw, const heif_image* image,
                                               uintptr_t framenr)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);

  int input_width = heif_image_get_width(image, heif_channel_Y);
  int input_height = heif_image_get_height(image, heif_channel_Y);

  uint32_t encoded_width, encoded_height;
  uvg266_query_encoded_size(encoder_raw, input_width, input_height, &encoded_width, &encoded_height);

  // Note: it is ok to cast away the const, as the image content is not changed.
  // However, we have to guarantee that there are no plane pointers or stride values kept over calling the svt_encode_image() function.
  /*
  err = heif_image_extend_padding_to_size(const_cast<struct heif_image*>(image),
                                          param->sourceWidth,
                                          param->sourceHeight);
  if (err.code) {
    return err;
  }
*/

  uvg_picture* pic = encoder->api->picture_alloc_csp(encoder->kvzChroma, encoded_width, encoded_height);
  if (!pic) {
    return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
  }

  if (isGreyscale) {
    size_t stride;
    const uint8_t* data = heif_image_get_plane_readonly2(image, heif_channel_Y, &stride);

    copy_plane(pic->y, pic->stride, data, stride, input_width, input_height, encoded_width, encoded_height);
  }
  else {
    size_t stride;
    const uint8_t* data;

    data = heif_image_get_plane_readonly2(image, heif_channel_Y, &stride);
    copy_plane(pic->y, pic->stride, data, stride, input_width, input_height, encoded_width, encoded_height);

    data = heif_image_get_plane_readonly2(image, heif_channel_Cb, &stride);
    copy_plane(pic->u, pic->stride >> encoder->chroma_stride_shift, data, stride,
               encoder->input_chroma_width, encoder->input_chroma_height,
               encoded_width >> encoder->chroma_stride_shift,
               encoded_height >> encoder->chroma_height_shift);

    data = heif_image_get_plane_readonly2(image, heif_channel_Cr, &stride);
    copy_plane(pic->v, pic->stride >> encoder->chroma_stride_shift, data, stride,
               encoder->input_chroma_width, encoder->input_chroma_height,
               encoded_width >> encoder->chroma_stride_shift, encoded_height >> encoder->chroma_height_shift);
  }


  uvg_data_chunk* data = nullptr;
  uint32_t data_len;
  int success;
#if 0
  success = encoder->api->encoder_headers(encoder->kvzencoder, &data, &data_len);
  if (!success) {
    encoder->api->picture_free(pic);

    return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
  }
#endif

  pic->pts = framenr;

  // If we write this, the data is twice in the output
  //append_chunk_data(data, encoder->output_data);

  uvg_picture* src_out = nullptr;

  success = encoder->api->encoder_encode(encoder->kvzencoder,
                                pic,
                                &data, &data_len,
                                nullptr, &src_out, nullptr);
  if (!success) {
    encoder->api->chunk_free(data);
    encoder->api->picture_free(pic);
    encoder->api->picture_free(src_out);

    return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
  }

  if (src_out) {
    encoder->append_chunk_data(data, (int)src_out->pts);

    encoder->api->picture_free(src_out);
    src_out = nullptr;
  }

  encoder->api->picture_free(pic);


  encoder->api->chunk_free(data);

  return heif_error_ok;
}


static heif_error uvg266_get_compressed_data_intern(void* encoder_raw, uint8_t** data, int* size,
                                                    uintptr_t* frame_nr, int* is_keyframe, int* more_frame_packets)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  if (encoder->output_data.empty()) {
    *data = nullptr;
    *size = 0;

    return heif_error_ok;
  }

  auto& pkg = encoder->output_data.front();

  if (frame_nr) {
    *frame_nr = pkg.frameNr;
  }

  if (more_frame_packets) {
    *more_frame_packets = pkg.more_nals;
  }

  encoder->active_data = std::move(pkg.data);
  encoder->output_data.pop_front();

  *data = encoder->active_data.data();
  *size = (int)encoder->active_data.size();

#if 0
  size_t start_idx = encoder->output_idx;

  while (start_idx < encoder->output_data.size() - 3 &&
         (encoder->output_data[start_idx] != 0 ||
          encoder->output_data[start_idx + 1] != 0 ||
          encoder->output_data[start_idx + 2] != 1)) {
    start_idx++;
  }

  size_t end_idx = start_idx + 1;

  while (end_idx < encoder->output_data.size() - 3 &&
         (encoder->output_data[end_idx] != 0 ||
          encoder->output_data[end_idx + 1] != 0 ||
          encoder->output_data[end_idx + 2] != 1)) {
    end_idx++;
  }

  if (end_idx == encoder->output_data.size() - 3) {
    end_idx = encoder->output_data.size();
  }

  *data = encoder->output_data.data() + start_idx + 3;
  *size = (int) (end_idx - start_idx - 3);

  encoder->output_idx = end_idx;
#endif

  return heif_error_ok;
}

static heif_error uvg266_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                                 heif_image_input_class input_class,
                                                 uint32_t framerate_num, uint32_t framerate_denom,
                                                 const heif_sequence_encoding_options* options)
{
  return uvg266_start_sequence_encoding_intern(encoder_raw, image, input_class,
                                               framerate_num, framerate_denom, options, true);
}

static heif_error uvg266_end_sequence_encoding(void* encoder_raw)
{
  encoder_struct_uvg266* encoder = (encoder_struct_uvg266*) encoder_raw;

  // --- flush

  uvg_data_chunk* data = nullptr;
  uint32_t data_len;

  // uvg_picture* src_out = nullptr;

  int success;
  uvg_picture* src_out = nullptr;

  for (;;) {
    success = encoder->api->encoder_encode(encoder->kvzencoder,
                                  nullptr,
                                  &data, &data_len,
                                  nullptr, &src_out, nullptr);
    if (!success) {
      encoder->api->chunk_free(data);
      encoder->api->picture_free(src_out);

      return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
    }

    if (data == nullptr || data->len == 0) {
      break;
    }

    encoder->append_chunk_data(data, (int)src_out->pts);

    encoder->api->picture_free(src_out);
    src_out = nullptr;
  }

  (void) success;

  if (src_out) {
    encoder->append_chunk_data(data, (int)src_out->pts);

    encoder->api->picture_free(src_out);
    src_out = nullptr;
  }


  return heif_error_ok;
}

static heif_error uvg266_encode_image(void* encoder_raw, const heif_image* image,
                                      heif_image_input_class input_class)
{
  heif_error err;
  err = uvg266_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = uvg266_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  uvg266_end_sequence_encoding(encoder_raw);

  return heif_error_ok;
}


static heif_error uvg266_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                             heif_encoded_data_type* type)
{
  return uvg266_get_compressed_data_intern(encoder_raw, data, size, nullptr, nullptr, nullptr);
}

static heif_error uvg266_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                            uintptr_t* frame_nr, int* is_keyframe, int* more_frame_packets)
{
  return uvg266_get_compressed_data_intern(encoder_raw, data, size, frame_nr, is_keyframe, more_frame_packets);
}


static const heif_encoder_plugin encoder_plugin_uvg266
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_VVC,
        /* id_name */ "uvg266",
        /* priority */ uvg266_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ uvg266_plugin_name,
        /* init_plugin */ uvg266_init_plugin,
        /* cleanup_plugin */ uvg266_cleanup_plugin,
        /* new_encoder */ uvg266_new_encoder,
        /* free_encoder */ uvg266_free_encoder,
        /* set_parameter_quality */ uvg266_set_parameter_quality,
        /* get_parameter_quality */ uvg266_get_parameter_quality,
        /* set_parameter_lossless */ uvg266_set_parameter_lossless,
        /* get_parameter_lossless */ uvg266_get_parameter_lossless,
        /* set_parameter_logging_level */ uvg266_set_parameter_logging_level,
        /* get_parameter_logging_level */ uvg266_get_parameter_logging_level,
        /* list_parameters */ uvg266_list_parameters,
        /* set_parameter_integer */ uvg266_set_parameter_integer,
        /* get_parameter_integer */ uvg266_get_parameter_integer,
        /* set_parameter_boolean */ uvg266_set_parameter_integer, // boolean also maps to integer function
        /* get_parameter_boolean */ uvg266_get_parameter_integer, // boolean also maps to integer function
        /* set_parameter_string */ uvg266_set_parameter_string,
        /* get_parameter_string */ uvg266_get_parameter_string,
        /* query_input_colorspace */ uvg266_query_input_colorspace,
        /* encode_image */ uvg266_encode_image,
        /* get_compressed_data */ uvg266_get_compressed_data,
        /* query_input_colorspace (v2) */ uvg266_query_input_colorspace2,
        /* query_encoded_size (v3) */ uvg266_query_encoded_size,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ uvg266_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ uvg266_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ uvg266_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ uvg266_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 0
    };

const struct heif_encoder_plugin* get_encoder_plugin_uvg266() {
  return &encoder_plugin_uvg266;
}


#if PLUGIN_UVG266
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_uvg266
};
#endif
