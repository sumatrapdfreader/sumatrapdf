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
#include "encoder_kvazaar.h"
#include <memory>
#include <string>   // apparently, this is a false positive of cpplint
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>
#include <deque>
#include <iostream>
#include <utility>

extern "C" {
#include <kvazaar.h>
}


static const char* kError_unspecified_error = "Unspecified encoder error";
static const char* kError_unsupported_bit_depth = "Bit depth not supported by kvazaar";
static const char* kError_unsupported_chroma = "Unsupported chroma type";
//static const char* kError_unsupported_image_size = "Images smaller than 16 pixels are not supported";


struct encoder_struct_kvazaar
{
  // --- parameters

  int quality = 75;
  bool lossless = false;

  // --- input config

  //heif_color_profile_nclx* nclx = nullptr;

  // --- encoder

  const kvz_api* api = nullptr;
  kvz_config* config = nullptr;
  kvz_encoder* kvzencoder = nullptr;

  // --- output

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t frameNr = 0;
  };

  std::deque<Packet> output_data;

  std::vector<uint8_t> active_data; // holds the data that we just returned


  ~encoder_struct_kvazaar()
  {
    /*
    if (nclx) {
      heif_nclx_color_profile_free(nclx);
    }
    */

    if (kvzencoder) {
      api->encoder_close(kvzencoder);
    }

    if (config) {
      api->config_destroy(config);
    }
  }
};

static const int kvazaar_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void kvazaar_set_default_parameters(void* encoder);


static const char* kvazaar_plugin_name()
{
  strcpy(plugin_name, "kvazaar HEVC encoder");
  return plugin_name;
}


#define MAX_NPARAMETERS 10

static heif_encoder_parameter kvazaar_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* kvazaar_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void kvazaar_init_parameters()
{
  heif_encoder_parameter* p = kvazaar_encoder_params;
  const heif_encoder_parameter** d = kvazaar_encoder_parameter_ptrs;
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


const heif_encoder_parameter** kvazaar_list_parameters(void* encoder)
{
  return kvazaar_encoder_parameter_ptrs;
}


static void kvazaar_init_plugin()
{
  kvazaar_init_parameters();
}


static void kvazaar_cleanup_plugin()
{
}


static heif_error kvazaar_new_encoder(void** enc)
{
  encoder_struct_kvazaar* encoder = new encoder_struct_kvazaar();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  kvazaar_set_default_parameters(encoder);

  return err;
}

static void kvazaar_free_encoder(void* encoder_raw)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  delete encoder;
}

static heif_error kvazaar_set_parameter_quality(void* encoder_raw, int quality)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

static heif_error kvazaar_get_parameter_quality(void* encoder_raw, int* quality)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

static heif_error kvazaar_set_parameter_lossless(void* encoder_raw, int enable)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  encoder->lossless = enable ? 1 : 0;

  return heif_error_ok;
}

static heif_error kvazaar_get_parameter_lossless(void* encoder_raw, int* enable)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  *enable = encoder->lossless;

  return heif_error_ok;
}

static heif_error kvazaar_set_parameter_logging_level(void* encoder_raw, int logging)
{
//  struct encoder_struct_kvazaar* encoder = (struct encoder_struct_kvazaar*) encoder_raw;

// return heif_error_invalid_parameter_value;

  return heif_error_ok;
}

static heif_error kvazaar_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
//  struct encoder_struct_kvazaar* encoder = (struct encoder_struct_kvazaar*) encoder_raw;

  *loglevel = 0;

  return heif_error_ok;
}


static heif_error kvazaar_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return kvazaar_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return kvazaar_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

static heif_error kvazaar_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return kvazaar_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return kvazaar_get_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}


static heif_error kvazaar_set_parameter_boolean(void* encoder, const char* name, int value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return kvazaar_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

// Unused, will use "kvazaar_get_parameter_integer" instead.
/*
static struct heif_error kvazaar_get_parameter_boolean(void* encoder, const char* name, int* value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless)==0) {
    return kvazaar_get_parameter_lossless(encoder,value);
  }

  return heif_error_unsupported_parameter;
}
*/


static heif_error kvazaar_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  return heif_error_unsupported_parameter;
}

static heif_error kvazaar_get_parameter_string(void* encoder_raw, const char* name,
                                               char* value, int value_size)
{
  return heif_error_unsupported_parameter;
}


static void kvazaar_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = kvazaar_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          kvazaar_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          kvazaar_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          kvazaar_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


static void kvazaar_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
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


static void kvazaar_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
{
  if (*colorspace == heif_colorspace_monochrome) {
    *colorspace = heif_colorspace_monochrome;
    *chroma = heif_chroma_monochrome;
  }
  else {
    *colorspace = heif_colorspace_YCbCr;
    if (*chroma != heif_chroma_420) {
      // Encoding to 4:2:2 and 4:4:4 currently does not work with Kvazaar (https://github.com/ultravideo/kvazaar/issues/418).
      *chroma = heif_chroma_420;
    }
  }
}

void kvazaar_query_encoded_size(void* encoder_raw, uint32_t input_width, uint32_t input_height,
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

static void append_chunk_data(kvz_data_chunk* data, encoder_struct_kvazaar* encoder, uintptr_t pts)
{
  if (!data || data->len == 0) {
    return;
  }

  std::vector<uint8_t> input_data;
  while (data) {
    input_data.insert(input_data.end(), data->data, data->data + data->len);
    data = data->next;
  }

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

    encoder_struct_kvazaar::Packet pkt;
    pkt.data.resize(end_idx - start_idx - 3);
    memcpy(pkt.data.data(), input_data.data() + start_idx + 3, pkt.data.size());
    pkt.frameNr = pts;

    // std::cout << "append frameNr=" << pts << " NAL:" << ((int)pkt.data[0]>>1) << " size:" << pkt.data.size() << "\n";

    encoder->output_data.emplace_back(std::move(pkt));

    if (end_idx == input_data.size()) {
      break;
    }

    start_idx = end_idx;
  }
}


static void copy_plane(kvz_pixel* out_p, size_t out_stride, const uint8_t* in_p, size_t in_stride,
                       uint32_t w, uint32_t h, uint32_t padded_width, uint32_t padded_height,
                       int bit_depth)
{
  int bpp = (bit_depth > 8) ? 2 : 1;

  for (uint32_t y = 0; y < padded_height; y++) {
    int sy = std::min(y, h - 1); // source y
    memcpy(out_p + y * out_stride, in_p + sy * in_stride, w * bpp);

    if (padded_width > w) {
      memset(out_p + y * out_stride + w, *(in_p + sy * in_stride + w - 1), (padded_width - w) * bpp);
    }
  }
}


template<typename T, typename D>
std::unique_ptr<T, D> make_guard(T* ptr, D&& deleter) {
    return std::unique_ptr<T, D>(ptr, deleter);
}

static heif_error kvazaar_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                                         enum heif_image_input_class input_class,
                                                         uint32_t framerate_num, uint32_t framerate_denom,
                                                         const heif_sequence_encoding_options* options,
                                                         bool image_sequence)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  int bit_depth = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);

  // Kvazaar uses a hard-coded bit depth (https://github.com/ultravideo/kvazaar/issues/399).
  // Check whether this matches to the image bit depth.
  // TODO: we should check the bit depth supported by the encoder (like query_input_colorspace2()) and output a warning
  //       if we have to encode at a different bit depth than requested.
  if (bit_depth != KVZ_BIT_DEPTH) {
    return {
      heif_error_Encoder_plugin_error,
      heif_suberror_Unsupported_bit_depth,
      kError_unsupported_bit_depth
  };
  }

  const kvz_api* api = encoder->api = kvz_api_get(bit_depth);
  if (api == nullptr) {
    return {
      heif_error_Encoder_plugin_error,
      heif_suberror_Unspecified,
      "Could not initialize Kvazaar API"
  };
  }

  kvz_config* config = encoder->config = api->config_alloc();
  api->config_init(config); // param, encoder->preset.c_str(), encoder->tune.c_str());

#if HAVE_KVAZAAR_ENABLE_LOGGING
  config->enable_logging_output = 0;
#endif

#if !ENABLE_MULTITHREADING_SUPPORT
  // 0: Process everything with main thread
  // -1 (default): Select automatically.
  config->threads = 0;
#endif

  heif_chroma chroma = heif_image_get_chroma_format(image);
  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);

  int input_width = heif_image_get_width(image, heif_channel_Y);
  int input_height = heif_image_get_height(image, heif_channel_Y);

  uint32_t encoded_width, encoded_height;
  kvazaar_query_encoded_size(encoder_raw, input_width, input_height, &encoded_width, &encoded_height);

  config->framerate_num = framerate_num;
  config->framerate_denom = framerate_denom;

  if (isGreyscale) {
    config->input_format = KVZ_FORMAT_P400;
  }
  else if (chroma == heif_chroma_420) {
    config->input_format = KVZ_FORMAT_P420;
  }
  else if (chroma == heif_chroma_422) {
    config->input_format = KVZ_FORMAT_P422;
  }
  else if (chroma == heif_chroma_444) {
    config->input_format = KVZ_FORMAT_P444;
  }
  else {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Unsupported_image_type,
      kError_unsupported_chroma
    };
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

  config->width = encoded_width;
  config->height = encoded_height;

  if (image_sequence) {
    // TODO
    /*
    config->target_bitrate = options->sequence_bitrate;
    */

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

  kvz_encoder* kvzencoder = encoder->kvzencoder = api->encoder_open(config);
  if (!kvzencoder) {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Encoder_encoding,
      kError_unspecified_error
  };
  }


  // --- encode headers

  // Not needed. Headers are also output by kvazaar together with the images.
#if 0
  kvz_data_chunk* data = nullptr;
  uint32_t data_len;
  int success;
  success = api->encoder_headers(encoder->kvzencoder, &data, &data_len);
  if (!success) {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Encoder_encoding,
      kError_unspecified_error
  };
  }

  append_chunk_data(data, encoder, 0);

  if (data) {
    api->chunk_free(data);
    data = nullptr;
  }
#endif

  return heif_error_ok;
}




static heif_error kvazaar_encode_sequence_frame(void* encoder_raw, const heif_image* image,
                                             uintptr_t frame_nr)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

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

  const kvz_api* api = encoder->api;

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);
  heif_chroma chroma = heif_image_get_chroma_format(image);

  int input_width = heif_image_get_width(image, heif_channel_Y);
  int input_height = heif_image_get_height(image, heif_channel_Y);

  uint32_t encoded_width, encoded_height;
  kvazaar_query_encoded_size(encoder_raw, input_width, input_height, &encoded_width, &encoded_height);

  kvz_chroma_format kvzChroma;
  int chroma_stride_shift = 0;
  int chroma_height_shift = 0;
  int input_chroma_width = 0;
  int input_chroma_height = 0;

  if (isGreyscale) {
    kvzChroma = KVZ_CSP_400;
  }
  else if (chroma == heif_chroma_420) {
    kvzChroma = KVZ_CSP_420;
    chroma_stride_shift = 1;
    chroma_height_shift = 1;
    input_chroma_width = (input_width + 1) / 2;
    input_chroma_height = (input_height + 1) / 2;
  }
  else if (chroma == heif_chroma_422) {
    kvzChroma = KVZ_CSP_422;
    chroma_stride_shift = 1;
    chroma_height_shift = 0;
    input_chroma_width = (input_width + 1) / 2;
    input_chroma_height = input_height;
  }
  else if (chroma == heif_chroma_444) {
    kvzChroma = KVZ_CSP_444;
    chroma_stride_shift = 0;
    chroma_height_shift = 0;
    input_chroma_width = input_width;
    input_chroma_height = input_height;
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


  auto upic = make_guard(api->picture_alloc_csp(kvzChroma, encoded_width, encoded_height), [api](kvz_picture* pic) { api->picture_free(pic); });
  kvz_picture* pic = upic.get();
  if (!pic) {
    return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
  }

  int bit_depth = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
  int bit_depth_chroma = 0;

  if (!isGreyscale) {
    bit_depth_chroma = heif_image_get_bits_per_pixel_range(image, heif_channel_Cb);
    if (bit_depth != bit_depth_chroma) {
      return {
        heif_error_Encoder_plugin_error,
        heif_suberror_Unsupported_bit_depth,
        "Luma bit depth must equal the chroma bit depth"
      };
    }
  }

  if (isGreyscale) {
    size_t stride;
    const uint8_t* data = heif_image_get_plane_readonly2(image, heif_channel_Y, &stride);

    copy_plane(pic->y, pic->stride, data, stride, input_width, input_height, encoded_width, encoded_height, bit_depth);
  }
  else {
    size_t stride;
    const uint8_t* data;

    data = heif_image_get_plane_readonly2(image, heif_channel_Y, &stride);
    copy_plane(pic->y, pic->stride, data, stride, input_width, input_height, encoded_width, encoded_height, bit_depth);

    data = heif_image_get_plane_readonly2(image, heif_channel_Cb, &stride);
    copy_plane(pic->u, pic->stride >> chroma_stride_shift, data, stride, input_chroma_width, input_chroma_height,
               encoded_width >> chroma_stride_shift, encoded_height >> chroma_height_shift, bit_depth_chroma);

    data = heif_image_get_plane_readonly2(image, heif_channel_Cr, &stride);
    copy_plane(pic->v, pic->stride >> chroma_stride_shift, data, stride, input_chroma_width, input_chroma_height,
               encoded_width >> chroma_stride_shift, encoded_height >> chroma_height_shift, bit_depth_chroma);
  }

  pic->pts = frame_nr;

  kvz_data_chunk* data = nullptr;
  uint32_t data_len;

  kvz_picture* picture_out = nullptr;

  int success;
  success = api->encoder_encode(encoder->kvzencoder,
                                pic,
                                &data, &data_len,
                                &picture_out, nullptr, nullptr);
  if (!success) {
    return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
  }

  append_chunk_data(data, encoder, picture_out ? picture_out->pts : 0);

  if (data) {
    api->chunk_free(data);
    data = nullptr;
  }

  api->picture_free(picture_out);

  (void) success;
  return heif_error_ok;
}


static heif_error kvazaar_end_sequence_encoding(void* encoder_raw)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;
  const kvz_api* api = encoder->api;


  for (;;) {
    kvz_data_chunk* data = nullptr;
    uint32_t data_len;

    kvz_picture* picture_out;

    int success = api->encoder_encode(encoder->kvzencoder,
                                  nullptr,
                                  &data, &data_len,
                                  &picture_out, nullptr, nullptr);
    if (!success) {
      return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
    }

    if (data == nullptr || data->len == 0) {
      break;
    }

    append_chunk_data(data, encoder, picture_out->pts);

    api->picture_free(picture_out);
    api->chunk_free(data);
  }

  return heif_error_ok;
}


static heif_error kvazaar_get_compressed_data_intern(void* encoder_raw, uint8_t** data, int* size,
                                              uintptr_t* frame_nr, int* more_frame_packets)
{
  encoder_struct_kvazaar* encoder = (encoder_struct_kvazaar*) encoder_raw;

  if (encoder->output_data.empty()) {
    *data = nullptr;
    *size = 0;

    return heif_error_ok;
  }

  std::vector<uint8_t>& pktdata = encoder->output_data.front().data;
#if 0
  size_t start_idx = 0;

  while (start_idx < pktdata.size() - 3 &&
         (pktdata[start_idx] != 0 ||
          pktdata[start_idx + 1] != 0 ||
          pktdata[start_idx + 2] != 1)) {
    start_idx++;
  }

  size_t end_idx = start_idx + 1;

  while (end_idx < pktdata.size() - 3 &&
         (pktdata[end_idx] != 0 ||
          pktdata[end_idx + 1] != 0 ||
          pktdata[end_idx + 2] != 1)) {
    end_idx++;
  }

  if (end_idx == encoder->output_data.size() - 3) {
    end_idx = encoder->output_data.size();
  }
#endif

  if (frame_nr) {
    *frame_nr = encoder->output_data.front().frameNr;
  }

  if (more_frame_packets) {
    if (encoder->output_data.size() > 1 &&
        encoder->output_data[0].frameNr == encoder->output_data[1].frameNr) {
      *more_frame_packets = 1;
    }
    else {
      *more_frame_packets = 0;
    }
  }

  encoder->active_data = std::move(pktdata);
  encoder->output_data.pop_front();

  *data = encoder->active_data.data();
  *size = static_cast<int>(encoder->active_data.size());

  return heif_error_ok;
}


static heif_error kvazaar_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                           heif_encoded_data_type* type)
{
  return kvazaar_get_compressed_data_intern(encoder_raw, data, size, nullptr, nullptr);
}

static heif_error kvazaar_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                            uintptr_t* frame_nr, int* is_keyframe, int* more_frame_packets)
{
  return kvazaar_get_compressed_data_intern(encoder_raw, data, size, frame_nr, more_frame_packets);
}


static heif_error kvazaar_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                                  enum heif_image_input_class input_class,
                                                  uint32_t framerate_num, uint32_t framerate_denom,
                                                  const heif_sequence_encoding_options* options)
{
  return kvazaar_start_sequence_encoding_intern(encoder_raw, image, input_class, framerate_num, framerate_denom, options, true);
}


static heif_error kvazaar_encode_image(void* encoder_raw, const heif_image* image,
                                       heif_image_input_class input_class)
{
  heif_error err;
  err = kvazaar_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = kvazaar_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  return kvazaar_end_sequence_encoding(encoder_raw);
}


static const heif_encoder_plugin encoder_plugin_kvazaar
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_HEVC,
        /* id_name */ "kvazaar",
        /* priority */ kvazaar_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ kvazaar_plugin_name,
        /* init_plugin */ kvazaar_init_plugin,
        /* cleanup_plugin */ kvazaar_cleanup_plugin,
        /* new_encoder */ kvazaar_new_encoder,
        /* free_encoder */ kvazaar_free_encoder,
        /* set_parameter_quality */ kvazaar_set_parameter_quality,
        /* get_parameter_quality */ kvazaar_get_parameter_quality,
        /* set_parameter_lossless */ kvazaar_set_parameter_lossless,
        /* get_parameter_lossless */ kvazaar_get_parameter_lossless,
        /* set_parameter_logging_level */ kvazaar_set_parameter_logging_level,
        /* get_parameter_logging_level */ kvazaar_get_parameter_logging_level,
        /* list_parameters */ kvazaar_list_parameters,
        /* set_parameter_integer */ kvazaar_set_parameter_integer,
        /* get_parameter_integer */ kvazaar_get_parameter_integer,
        /* set_parameter_boolean */ kvazaar_set_parameter_integer, // boolean also maps to integer function
        /* get_parameter_boolean */ kvazaar_get_parameter_integer, // boolean also maps to integer function
        /* set_parameter_string */ kvazaar_set_parameter_string,
        /* get_parameter_string */ kvazaar_get_parameter_string,
        /* query_input_colorspace */ kvazaar_query_input_colorspace,
        /* encode_image */ kvazaar_encode_image,
        /* get_compressed_data */ kvazaar_get_compressed_data,
        /* query_input_colorspace (v2) */ kvazaar_query_input_colorspace2,
        /* query_encoded_size (v3) */ kvazaar_query_encoded_size,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ kvazaar_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ kvazaar_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ kvazaar_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ kvazaar_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 0
    };

const heif_encoder_plugin* get_encoder_plugin_kvazaar()
{
  return &encoder_plugin_kvazaar;
}


#if PLUGIN_KVAZAAR
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_kvazaar
};
#endif
