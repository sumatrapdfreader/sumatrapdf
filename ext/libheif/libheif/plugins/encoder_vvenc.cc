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
#include "encoder_vvenc.h"
#include <memory>
#include <string>   // apparently, this is a false positive of cpplint
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>
#include <deque>
#include <utility>

extern "C" {
#include <vvenc/vvenc.h>
}


// TODO: it seems that the encoder does not support monochrome input. This affects also images with alpha channels.

static const char* kError_unspecified_error = "Unspecified encoder error";
static const char* kError_unsupported_bit_depth = "Bit depth not supported by vvenc";
static const char* kError_unsupported_chroma = "Unsupported chroma type";
//static const char* kError_unsupported_image_size = "Images smaller than 16 pixels are not supported";


struct encoder_struct_vvenc
{
  int quality = 32;
  bool lossless = false;

  // --- encoder

  vvencEncoder* vvencoder = nullptr;
  vvencChromaFormat vvencChroma;
  uint32_t encoded_width=0, encoded_height=0;

  // --- output

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t frameNr = 0;
    bool more_nals = false;
  };

  std::deque<Packet> output_data;

  std::vector<uint8_t> active_data; // holds the data that we just returned

  ~encoder_struct_vvenc()
  {
    if (vvencoder) {
      vvenc_encoder_close(vvencoder);
    }
  }
};

static const int vvenc_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static void vvenc_set_default_parameters(void* encoder);


static const char* vvenc_plugin_name()
{
  strcpy(plugin_name, "vvenc VVC encoder");
  return plugin_name;
}


#define MAX_NPARAMETERS 10

static heif_encoder_parameter vvenc_encoder_params[MAX_NPARAMETERS];
static const heif_encoder_parameter* vvenc_encoder_parameter_ptrs[MAX_NPARAMETERS + 1];

static void vvenc_init_parameters()
{
  heif_encoder_parameter* p = vvenc_encoder_params;
  const heif_encoder_parameter** d = vvenc_encoder_parameter_ptrs;
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


const heif_encoder_parameter** vvenc_list_parameters(void* encoder)
{
  return vvenc_encoder_parameter_ptrs;
}


static void vvenc_init_plugin()
{
  vvenc_init_parameters();
}


static void vvenc_cleanup_plugin()
{
}


static heif_error vvenc_new_encoder(void** enc)
{
  encoder_struct_vvenc* encoder = new encoder_struct_vvenc();
  heif_error err = heif_error_ok;

  *enc = encoder;

  // set default parameters

  vvenc_set_default_parameters(encoder);

  return err;
}

static void vvenc_free_encoder(void* encoder_raw)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  delete encoder;
}

static heif_error vvenc_set_parameter_quality(void* encoder_raw, int quality)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  if (quality < 0 || quality > 100) {
    return heif_error_invalid_parameter_value;
  }

  encoder->quality = quality;

  return heif_error_ok;
}

static heif_error vvenc_get_parameter_quality(void* encoder_raw, int* quality)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  *quality = encoder->quality;

  return heif_error_ok;
}

static heif_error vvenc_set_parameter_lossless(void* encoder_raw, int enable)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  encoder->lossless = enable ? 1 : 0;

  return heif_error_ok;
}

static heif_error vvenc_get_parameter_lossless(void* encoder_raw, int* enable)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  *enable = encoder->lossless;

  return heif_error_ok;
}

static heif_error vvenc_set_parameter_logging_level(void* encoder_raw, int logging)
{
//  struct encoder_struct_vvenc* encoder = (struct encoder_struct_vvenc*) encoder_raw;

// return heif_error_invalid_parameter_value;

  return heif_error_ok;
}

static heif_error vvenc_get_parameter_logging_level(void* encoder_raw, int* loglevel)
{
//  struct encoder_struct_vvenc* encoder = (struct encoder_struct_vvenc*) encoder_raw;

  *loglevel = 0;

  return heif_error_ok;
}


static heif_error vvenc_set_parameter_integer(void* encoder_raw, const char* name, int value)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return vvenc_set_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return vvenc_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

static heif_error vvenc_get_parameter_integer(void* encoder_raw, const char* name, int* value)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  if (strcmp(name, heif_encoder_parameter_name_quality) == 0) {
    return vvenc_get_parameter_quality(encoder, value);
  }
  else if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return vvenc_get_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}


static heif_error vvenc_set_parameter_boolean(void* encoder, const char* name, int value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless) == 0) {
    return vvenc_set_parameter_lossless(encoder, value);
  }

  return heif_error_unsupported_parameter;
}

// Unused, will use "vvenc_get_parameter_integer" instead.
/*
static struct heif_error vvenc_get_parameter_boolean(void* encoder, const char* name, int* value)
{
  if (strcmp(name, heif_encoder_parameter_name_lossless)==0) {
    return vvenc_get_parameter_lossless(encoder,value);
  }

  return heif_error_unsupported_parameter;
}
*/


static heif_error vvenc_set_parameter_string(void* encoder_raw, const char* name, const char* value)
{
  return heif_error_unsupported_parameter;
}

static heif_error vvenc_get_parameter_string(void* encoder_raw, const char* name,
                                             char* value, int value_size)
{
  return heif_error_unsupported_parameter;
}


static void vvenc_set_default_parameters(void* encoder)
{
  for (const heif_encoder_parameter** p = vvenc_encoder_parameter_ptrs; *p; p++) {
    const heif_encoder_parameter* param = *p;

    if (param->has_default) {
      switch (param->type) {
        case heif_encoder_parameter_type_integer:
          vvenc_set_parameter_integer(encoder, param->name, param->integer.default_value);
          break;
        case heif_encoder_parameter_type_boolean:
          vvenc_set_parameter_boolean(encoder, param->name, param->boolean.default_value);
          break;
        case heif_encoder_parameter_type_string:
          vvenc_set_parameter_string(encoder, param->name, param->string.default_value);
          break;
      }
    }
  }
}


static void vvenc_query_input_colorspace(heif_colorspace* colorspace, heif_chroma* chroma)
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


static void vvenc_query_input_colorspace2(void* encoder_raw, heif_colorspace* colorspace, heif_chroma* chroma)
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

void vvenc_query_encoded_size(void* encoder_raw, uint32_t input_width, uint32_t input_height,
                              uint32_t* encoded_width, uint32_t* encoded_height)
{
  *encoded_width = (input_width + 7) & ~0x7;
  *encoded_height = (input_height + 7) & ~0x7;
}


#include <iostream>
#include <logging.h>

static void append_chunk_data(struct encoder_struct_vvenc* encoder, vvencAccessUnit* au,
                              uintptr_t pts)
{
#if 0
  std::cout << "DATA pts=" << pts << "\n";
  std::cout << write_raw_data_as_hex(au->payload, au->payloadUsedSize, {}, {});
  std::cout << "---\n";
#endif

  std::vector<uint8_t> dat;
  dat.resize(au->payloadUsedSize);
  memcpy(dat.data(), au->payload, au->payloadUsedSize);

  int start_idx = 0;

  for (;;)
  {
    while (start_idx < au->payloadUsedSize - 3 &&
           (au->payload[start_idx] != 0 ||
            au->payload[start_idx + 1] != 0 ||
            au->payload[start_idx + 2] != 1)) {
      start_idx++;
            }

    int end_idx = start_idx + 1;

    while (end_idx < au->payloadUsedSize - 3 &&
           (au->payload[end_idx] != 0 ||
            au->payload[end_idx + 1] != 0 ||
            au->payload[end_idx + 2] != 1)) {
      end_idx++;
            }

    if (end_idx == au->payloadUsedSize - 3) {
      end_idx = au->payloadUsedSize;
    }

    encoder_struct_vvenc::Packet pkt;
    pkt.data.resize(end_idx - start_idx - 3);
    memcpy(pkt.data.data(), au->payload + start_idx + 3, pkt.data.size());
    pkt.frameNr = pts;
    pkt.more_nals = true; // will be set to 'false' for last NAL below.

    //std::cout << "append frameNr=" << pts << " NAL:" << ((int)pkt.data[1]>>3) << " size:" << pkt.data.size() << "\n";

    encoder->output_data.emplace_back(std::move(pkt));

    if (end_idx == au->payloadUsedSize) {
      break;
    }

    start_idx = end_idx;
  }

  if (!encoder->output_data.empty()) {
    encoder->output_data.back().more_nals = false;
  }
}


static void copy_plane(int16_t* dst_p, size_t dst_stride, const uint8_t* in_p, size_t in_stride, int w, int h, int padded_width, int padded_height)
{
  for (int y = 0; y < padded_height; y++) {
    int sy = std::min(y, h - 1); // source y

    for (int x = 0; x < w; x++) {
      dst_p[y * dst_stride + x] = in_p[sy * in_stride + x];
    }

    for (int x = w; x < padded_width; x++) {
      dst_p[y * dst_stride + x] = in_p[sy * in_stride + w - 1];
    }
  }
}


static heif_error vvenc_start_sequence_encoding_intern(void* encoder_raw, const heif_image* image,
                                                       enum heif_image_input_class input_class,
                                                       uint32_t framerate_num, uint32_t framerate_denom,
                                                       const heif_sequence_encoding_options* options,
                                                       bool image_sequence)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  vvenc_config params;

  int bit_depth = heif_image_get_bits_per_pixel_range(image, heif_channel_Y);
  if (bit_depth != 8) {
    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Unsupported_image_type,
      kError_unsupported_bit_depth
  };
  }

  int input_width = heif_image_get_width(image, heif_channel_Y);
  int input_height = heif_image_get_height(image, heif_channel_Y);

  vvenc_query_encoded_size(encoder_raw, input_width, input_height,
    &encoder->encoded_width, &encoder->encoded_height);


  // invert encoder quality range and scale to 0-63
  int encoder_quality = 63 - encoder->quality*63/100;

  int ret = vvenc_init_default(&params,
                               static_cast<int>(encoder->encoded_width),
                               static_cast<int>(encoder->encoded_height),
                               static_cast<int>((framerate_denom + framerate_num / 2) / framerate_num),
                               0,
                               encoder_quality,
                               VVENC_MEDIUM);
  if (ret != VVENC_OK) {
    // TODO: cleanup memory

    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Encoder_encoding,
      kError_unspecified_error
  };
  }

  params.m_inputBitDepth[0] = bit_depth;
  params.m_inputBitDepth[1] = bit_depth;
  params.m_outputBitDepth[0] = bit_depth;
  params.m_outputBitDepth[1] = bit_depth;
  params.m_internalBitDepth[0] = bit_depth;
  params.m_internalBitDepth[1] = bit_depth;

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);
  if (isGreyscale) {
    params.m_internChromaFormat = VVENC_CHROMA_400;
  }

  params.m_FrameRate = framerate_denom;
  params.m_FrameScale = framerate_num;

  if (image_sequence) {
    if (options->keyframe_distance_max) {
      params.m_IntraPeriod = options->keyframe_distance_max;
    }

    switch (options->gop_structure) {
      case heif_sequence_gop_structure_intra_only:
        params.m_IntraPeriod = 1;
        break;
      case heif_sequence_gop_structure_lowdelay:
        params.m_picReordering = 0;
        params.m_GOPSize = 8; // as of vvcenc 1.13.1, this only works with GOPSize=8. see https://github.com/fraunhoferhhi/vvenc/issues/284
        params.m_DecodingRefreshType = VVENC_DRT_NONE;
        params.m_poc0idr = true;
        break;
      case heif_sequence_gop_structure_unrestricted:
        ;
    }
  }

  vvencEncoder* vvencoder = encoder->vvencoder = vvenc_encoder_create();

  //ret = vvenc_check_config(vvencoder, &params);
  //const char* err = vvenc_get_last_error(vvencoder);

  ret = vvenc_encoder_open(vvencoder, &params);
  //err = vvenc_get_last_error(vvencoder);
  if (ret != VVENC_OK) {
    // TODO: cleanup memory

    return heif_error{
      heif_error_Encoder_plugin_error,
      heif_suberror_Encoder_encoding,
      kError_unspecified_error
  };
  }

  return heif_error_ok;
}


static heif_error vvenc_encode_sequence_frame(void* encoder_raw, const heif_image* image,
                                              uintptr_t framenr)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;
  vvencEncoder* vvencoder = encoder->vvencoder;

  bool isGreyscale = (heif_image_get_colorspace(image) == heif_colorspace_monochrome);
  heif_chroma chroma = heif_image_get_chroma_format(image);



  int input_width = heif_image_get_width(image, heif_channel_Y);
  int input_height = heif_image_get_height(image, heif_channel_Y);

  int input_chroma_width = 0;
  int input_chroma_height = 0;

  uint32_t encoded_width, encoded_height;
  vvenc_query_encoded_size(encoder_raw, input_width, input_height, &encoded_width, &encoded_height);

  vvencChromaFormat vvencChroma;
  int chroma_stride_shift = 0;
  int chroma_height_shift = 0;

  if (isGreyscale) {
    vvencChroma = VVENC_CHROMA_400;
  }
  else if (chroma == heif_chroma_420) {
    vvencChroma = VVENC_CHROMA_420;
    chroma_stride_shift = 1;
    chroma_height_shift = 1;
    input_chroma_width = (input_width + 1) / 2;
    input_chroma_height = (input_height + 1) / 2;
  }
  else if (chroma == heif_chroma_422) {
    vvencChroma = VVENC_CHROMA_422;
    chroma_stride_shift = 1;
    chroma_height_shift = 0;
    input_chroma_width = (input_width + 1) / 2;
    input_chroma_height = input_height;
  }
  else if (chroma == heif_chroma_444) {
    vvencChroma = VVENC_CHROMA_444;
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

  encoder->vvencChroma = vvencChroma;

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

#if 0
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
#endif

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

  vvencYUVBuffer* yuvbuf = vvenc_YUVBuffer_alloc();
  vvenc_YUVBuffer_alloc_buffer(yuvbuf, vvencChroma, encoded_width, encoded_height);

  vvencAccessUnit* au = vvenc_accessUnit_alloc();

  const int auSizeScale = (vvencChroma <= VVENC_CHROMA_420 ? 2 : 3);
  vvenc_accessUnit_alloc_payload(au, auSizeScale * encoded_width * encoded_height + 1024);

  // vvenc_init_pass( encoder, pass, statsfilename );

  if (isGreyscale) {
    size_t stride;
    const uint8_t* data = heif_image_get_plane_readonly2(image, heif_channel_Y, &stride);

    copy_plane(yuvbuf->planes[0].ptr, yuvbuf->planes[0].stride, data, stride, input_width, input_height, encoded_width, encoded_height);
  }
  else {
    size_t stride;
    const uint8_t* data;

    data = heif_image_get_plane_readonly2(image, heif_channel_Y, &stride);
    copy_plane(yuvbuf->planes[0].ptr, yuvbuf->planes[0].stride, data, stride, input_width, input_height, encoded_width, encoded_height);

    data = heif_image_get_plane_readonly2(image, heif_channel_Cb, &stride);
    copy_plane(yuvbuf->planes[1].ptr, yuvbuf->planes[1].stride, data, stride, input_chroma_width, input_chroma_height,
               encoded_width >> chroma_stride_shift, encoded_height >> chroma_height_shift);

    data = heif_image_get_plane_readonly2(image, heif_channel_Cr, &stride);
    copy_plane(yuvbuf->planes[2].ptr, yuvbuf->planes[2].stride, data, stride, input_chroma_width, input_chroma_height,
               encoded_width >> chroma_stride_shift, encoded_height >> chroma_height_shift);
  }


  yuvbuf->cts     = framenr;
  yuvbuf->ctsValid = true;


  bool encDone;

  int ret = vvenc_encode(vvencoder, yuvbuf, au, &encDone);
  if (ret != VVENC_OK) {
    //vvenc_encoder_close(vvencoder);
    vvenc_YUVBuffer_free_buffer(yuvbuf);
    vvenc_YUVBuffer_free(yuvbuf, false); // release storage and payload memory
    vvenc_accessUnit_free(au, true); // release storage and payload memory

    return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
  }

  if (au->payloadUsedSize > 0) {
    append_chunk_data(encoder, au, au->cts);
  }

  vvenc_YUVBuffer_free_buffer(yuvbuf);
  vvenc_YUVBuffer_free(yuvbuf, false); // release storage and payload memory
  vvenc_accessUnit_free(au, true); // release storage and payload memory

  /*
  delete[] yptr;
  delete[] cbptr;
  delete[] crptr;
*/

  return heif_error_ok;
}


static heif_error vvenc_end_sequence_encoding(void* encoder_raw)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;
  vvencEncoder* vvencoder = encoder->vvencoder;

  vvencAccessUnit* au = vvenc_accessUnit_alloc();

  const int auSizeScale = (encoder->vvencChroma <= VVENC_CHROMA_420 ? 2 : 3);
  vvenc_accessUnit_alloc_payload(au, auSizeScale * encoder->encoded_width * encoder->encoded_height + 1024);

  bool encDone = false;

  while (!encDone) {
    int ret = vvenc_encode(vvencoder, nullptr, au, &encDone);
    if (ret != VVENC_OK) {
      //vvenc_encoder_close(vvencoder);
      vvenc_accessUnit_free(au, true); // release storage and payload memory

      return heif_error{
        heif_error_Encoder_plugin_error,
        heif_suberror_Encoder_encoding,
        kError_unspecified_error
    };
    }

    if (au->payloadUsedSize > 0) {
      append_chunk_data(encoder, au, au->cts);
    }
  }

  vvenc_accessUnit_free(au, true); // release storage and payload memory

  return heif_error_ok;
}


static heif_error vvenc_encode_image(void* encoder_raw, const heif_image* image,
                                     heif_image_input_class input_class)
{
  heif_error err;
  err = vvenc_start_sequence_encoding_intern(encoder_raw, image, input_class, 1,25, nullptr, false);
  if (err.code) {
    return err;
  }

  err = vvenc_encode_sequence_frame(encoder_raw, image, 0);
  if (err.code) {
    return err;
  }

  vvenc_end_sequence_encoding(encoder_raw);

  return heif_error_ok;
}

static heif_error vvenc_start_sequence_encoding(void* encoder_raw, const heif_image* image,
                                                enum heif_image_input_class input_class,
                                                uint32_t framerate_num, uint32_t framerate_denom,
                                                const heif_sequence_encoding_options* options)
{
  return vvenc_start_sequence_encoding_intern(encoder_raw, image, input_class, framerate_num, framerate_denom, options, true);
}



static heif_error vvenc_get_compressed_data_intern(void* encoder_raw, uint8_t** data, int* size,
                                              uintptr_t* frame_nr, int* more_frame_packets)
{
  encoder_struct_vvenc* encoder = (encoder_struct_vvenc*) encoder_raw;

  if (encoder->output_data.empty()) {
    *data = nullptr;
    *size = 0;

    return heif_error_ok;
  }

  std::vector<uint8_t>& pktdata = encoder->output_data.front().data;

  if (frame_nr) {
    *frame_nr = encoder->output_data.front().frameNr;
  }
/*
  if (more_frame_packets &&
      encoder->output_data.size() > 1 &&
      encoder->output_data[0].frameNr == encoder->output_data[1].frameNr) {
    *more_frame_packets = 1;
  }
*/

  if (more_frame_packets) {
    *more_frame_packets = encoder->output_data.front().more_nals;
  }

  encoder->active_data = std::move(pktdata);
  encoder->output_data.pop_front();

  *data = encoder->active_data.data();
  *size = static_cast<int>(encoder->active_data.size());

  return heif_error_ok;
}


static heif_error vvenc_get_compressed_data(void* encoder_raw, uint8_t** data, int* size,
                                           heif_encoded_data_type* type)
{
  return vvenc_get_compressed_data_intern(encoder_raw, data, size, nullptr, nullptr);
}

static heif_error vvenc_get_compressed_data2(void* encoder_raw, uint8_t** data, int* size,
                                            uintptr_t* frame_nr, int* is_keyframe, int* more_frame_packets)
{
  return vvenc_get_compressed_data_intern(encoder_raw, data, size, frame_nr, more_frame_packets);
}


static const heif_encoder_plugin encoder_plugin_vvenc
    {
        /* plugin_api_version */ 4,
        /* compression_format */ heif_compression_VVC,
        /* id_name */ "vvenc",
        /* priority */ vvenc_PLUGIN_PRIORITY,
        /* supports_lossy_compression */ true,
        /* supports_lossless_compression */ true,
        /* get_plugin_name */ vvenc_plugin_name,
        /* init_plugin */ vvenc_init_plugin,
        /* cleanup_plugin */ vvenc_cleanup_plugin,
        /* new_encoder */ vvenc_new_encoder,
        /* free_encoder */ vvenc_free_encoder,
        /* set_parameter_quality */ vvenc_set_parameter_quality,
        /* get_parameter_quality */ vvenc_get_parameter_quality,
        /* set_parameter_lossless */ vvenc_set_parameter_lossless,
        /* get_parameter_lossless */ vvenc_get_parameter_lossless,
        /* set_parameter_logging_level */ vvenc_set_parameter_logging_level,
        /* get_parameter_logging_level */ vvenc_get_parameter_logging_level,
        /* list_parameters */ vvenc_list_parameters,
        /* set_parameter_integer */ vvenc_set_parameter_integer,
        /* get_parameter_integer */ vvenc_get_parameter_integer,
        /* set_parameter_boolean */ vvenc_set_parameter_integer, // boolean also maps to integer function
        /* get_parameter_boolean */ vvenc_get_parameter_integer, // boolean also maps to integer function
        /* set_parameter_string */ vvenc_set_parameter_string,
        /* get_parameter_string */ vvenc_get_parameter_string,
        /* query_input_colorspace */ vvenc_query_input_colorspace,
        /* encode_image */ vvenc_encode_image,
        /* get_compressed_data */ vvenc_get_compressed_data,
        /* query_input_colorspace (v2) */ vvenc_query_input_colorspace2,
        /* query_encoded_size (v3) */ vvenc_query_encoded_size,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        /* start_sequence_encoding (v4) */ vvenc_start_sequence_encoding,
        /* encode_sequence_frame (v4) */ vvenc_encode_sequence_frame,
        /* end_sequence_encoding (v4) */ vvenc_end_sequence_encoding,
        /* get_compressed_data2 (v4) */ vvenc_get_compressed_data2,
        /* does_indicate_keyframes (v4) */ 0
    };

const heif_encoder_plugin* get_encoder_plugin_vvenc()
{
  return &encoder_plugin_vvenc;
}


#if PLUGIN_VVENC
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_encoder,
  &encoder_plugin_vvenc
};
#endif
