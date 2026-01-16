/*
 * AVIF codec.
 * Copyright (c) 2019 Dirk Farin <dirk.farin@gmail.com>
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
#include "decoder_aom.h"
#include <memory>
#include <cstring>
#include <cassert>
#include <deque>
#include <string>

#include <aom/aom_decoder.h>
#include <aom/aomdx.h>


struct aom_decoder
{
  ~aom_decoder()
  {
    // --- free images that are still in the output queue

    for (auto& pkt : output_queue) {
      if (pkt.img) {
        heif_image_release(pkt.img);
      }
    }
  }

  aom_codec_ctx_t codec;
  bool codec_initialized = false;

  aom_codec_iface_t* iface;

  struct Packet
  {
    heif_image* img;
    uintptr_t user_data = 0;
  };

  std::deque<Packet> output_queue;
  bool strict_decoding = false;
  std::string error_message;
};

static const char kSuccess[] = "Success";
static const char kEmptyString[] = "";

static const int AOM_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static const char* aom_plugin_name()
{
  if (strlen(aom_codec_iface_name(aom_codec_av1_dx())) < MAX_PLUGIN_NAME_LENGTH) {
    strcpy(plugin_name, aom_codec_iface_name(aom_codec_av1_dx()));
  }
  else {
    strcpy(plugin_name, "AOMedia AV1 decoder");
  }

  return plugin_name;
}


static void aom_init_plugin()
{
}


static void aom_deinit_plugin()
{
}


static int aom_does_support_format(heif_compression_format format)
{
  if (format == heif_compression_AV1) {
    return AOM_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}

static int aom_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return aom_does_support_format(format->format);
}

heif_error aom_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  aom_decoder* decoder = new aom_decoder();

  decoder->iface = aom_codec_av1_dx();

  aom_codec_dec_cfg_t cfg{};

  if (options->num_threads) {
    cfg.threads = options->num_threads;
  }
  cfg.w = 0;
  cfg.h = 0;
  cfg.allow_lowbitdepth = 1;

  aom_codec_err_t aomerr = aom_codec_dec_init(&decoder->codec, decoder->iface, &cfg, 0);
  if (aomerr) {
    *dec = NULL;

    delete decoder;

    heif_error err = {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, aom_codec_err_to_string(aomerr)};
    return err;
  }

  decoder->codec_initialized = true;
  *dec = decoder;

  heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


heif_error aom_new_decoder(void** dec)
{
  heif_decoder_plugin_options options;
  options.format = heif_compression_AV1;
  options.strict_decoding = false;
  options.num_threads = 0;

  return aom_new_decoder2(dec, &options);
}

void aom_free_decoder(void* decoder_raw)
{
  aom_decoder* decoder = (aom_decoder*) decoder_raw;

  if (!decoder) {
    return;
  }

  if (decoder->codec_initialized) {
    aom_codec_destroy(&decoder->codec);
    decoder->codec_initialized = false;
  }

  delete decoder;
}


void aom_set_strict_decoding(void* decoder_raw, int flag)
{
  aom_decoder* decoder = (aom_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}


static heif_error get_next_image_from_decoder(aom_decoder* decoder,
                                              aom_codec_iter_t* inout_iter,
                                              heif_image** out_img,
                                              uintptr_t* out_user_data,
                                              const heif_security_limits* limits)
{
  aom_image_t* img = aom_codec_get_frame(&decoder->codec, inout_iter);

  if (img == NULL) {
    *out_img = NULL;
    return {};
  }

  if (out_user_data) {
    *out_user_data = (uintptr_t)img->user_priv;
  }

  /*
  if (img == NULL) {
    return {
      heif_error_Decoder_plugin_error,
      heif_suberror_Unspecified,
      kEmptyString
    };
  }
*/

  if (img->fmt != AOM_IMG_FMT_I420 &&
      img->fmt != AOM_IMG_FMT_I42016 &&
      img->fmt != AOM_IMG_FMT_I422 &&
      img->fmt != AOM_IMG_FMT_I42216 &&
      img->fmt != AOM_IMG_FMT_I444 &&
      img->fmt != AOM_IMG_FMT_I44416) {
    return {
      heif_error_Decoder_plugin_error,
      heif_suberror_Unsupported_image_type,
      kEmptyString
    };
  }

  heif_chroma chroma;
  heif_colorspace colorspace;

  if (img->monochrome) {
      chroma = heif_chroma_monochrome;
      colorspace = heif_colorspace_monochrome;
  }
  else {
    if (img->fmt == AOM_IMG_FMT_I444 ||
        img->fmt == AOM_IMG_FMT_I44416) {
      chroma = heif_chroma_444;
    }
    else if (img->fmt == AOM_IMG_FMT_I422 ||
             img->fmt == AOM_IMG_FMT_I42216) {
      chroma = heif_chroma_422;
    }
    else {
      chroma = heif_chroma_420;
    }
    colorspace = heif_colorspace_YCbCr;
  }

  heif_image* heif_img = nullptr;
  heif_error err = heif_image_create(img->d_w, img->d_h,
                                     colorspace,
                                     chroma,
                                     &heif_img);
  if (err.code != heif_error_Ok) {
    assert(heif_img==nullptr);
    return err;
  }


  // --- read nclx parameters from decoded AV1 bitstream

  heif_color_profile_nclx nclx;
  nclx.version = 1;
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_color_primaries(&nclx, static_cast<uint16_t>(img->cp)), { heif_image_release(heif_img); });
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_transfer_characteristics(&nclx, static_cast<uint16_t>(img->tc)), { heif_image_release(heif_img); });
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_matrix_coefficients(&nclx, static_cast<uint16_t>(img->mc)), { heif_image_release(heif_img); });
  nclx.full_range_flag = (img->range == AOM_CR_FULL_RANGE);
  heif_image_set_nclx_color_profile(heif_img, &nclx);


  // --- transfer data from aom_image_t to HeifPixelImage

  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  int num_planes = (chroma == heif_chroma_monochrome ? 1 : 3);

  for (int c = 0; c < num_planes; c++) {
    int bpp = img->bit_depth;

    const uint8_t* data = img->planes[c];
    int stride = img->stride[c];

    int w = img->d_w;
    int h = img->d_h;

    if (c > 0 && chroma == heif_chroma_420) {
      w = (w + 1) / 2;
      h = (h + 1) / 2;
    }
    else if (c > 0 && chroma == heif_chroma_422) {
      w = (w + 1) / 2;
    }

    err = heif_image_add_plane_safe(heif_img, channel2plane[c], w, h, bpp, limits);
    if (err.code != heif_error_Ok) {
      // copy error message to decoder object because heif_image will be released
      decoder->error_message = err.message;
      err.message = decoder->error_message.c_str();

      heif_image_release(heif_img);
      return err;
    }

    size_t dst_stride;
    uint8_t* dst_mem = heif_image_get_plane2(heif_img, channel2plane[c], &dst_stride);

    int bytes_per_pixel = (bpp + 7) / 8;

    for (int y = 0; y < h; y++) {
      memcpy(dst_mem + y * dst_stride, data + y * stride, w * bytes_per_pixel);
    }
  }

  *out_img = heif_img;
  return err;
}



heif_error aom_push_data2(void* decoder_raw, const void* frame_data, size_t frame_size, uintptr_t user_data)
{
  aom_decoder* decoder = (struct aom_decoder*) decoder_raw;

  const char* ver = aom_codec_version_str();
  (void)ver;

  aom_codec_err_t aomerr;
  aomerr = aom_codec_decode(&decoder->codec, (const uint8_t*) frame_data, frame_size, (void*)user_data);
  if (aomerr) {
    heif_error err = {heif_error_Invalid_input, heif_suberror_Unspecified, aom_codec_err_to_string(aomerr)};
    return err;
  }


  // --- decode images and fill into output queue

  aom_codec_iter_t iter = NULL;
  for (;;) {
    heif_image* img;
    uintptr_t out_user_data;
    heif_error err = get_next_image_from_decoder(decoder, &iter, &img, &out_user_data, nullptr); // TODO: send limits);
    if (err.code) {
      return err;
    }

    if (img) {
      decoder->output_queue.push_back({img, out_user_data});
    }
    else {
      break;
    }
  }

  heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


heif_error aom_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  return aom_push_data2(decoder_raw, frame_data, frame_size, 0);
}


static heif_error aom_decode_next_image2(void* decoder_raw, heif_image** out_img,
                                         uintptr_t* out_user_data,
                                         const heif_security_limits* limits)
{
  aom_decoder* decoder = (struct aom_decoder*) decoder_raw;

  // --- if there are images in the output queue, pass them back

  if (!decoder->output_queue.empty()) {
    if (out_user_data) {
      *out_user_data = decoder->output_queue.front().user_data;
    }

    heif_image* next_image = decoder->output_queue.front().img;
    decoder->output_queue.pop_front();

    *out_img = next_image;
    return {};
  }
  else {
    *out_img = nullptr;
    return {};
  }
}


static heif_error aom_decode_next_image(void* decoder_raw, heif_image** out_img,
                                        const heif_security_limits* limits)
{
  return aom_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}

heif_error aom_decode_image(void* decoder_raw, heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return aom_decode_next_image(decoder_raw, out_img, limits);
}


static heif_error aom_flush_data(void* decoder_raw)
{
  // aom_decoder* decoder = (aom_decoder*) decoder_raw;

  return heif_error_ok;
}


static const heif_decoder_plugin decoder_aom
    {
        5,
        aom_plugin_name,
        aom_init_plugin,
        aom_deinit_plugin,
        aom_does_support_format,
        aom_new_decoder,
        aom_free_decoder,
        aom_push_data,
        aom_decode_image,
        aom_set_strict_decoding,
        "aom",
        aom_decode_next_image,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        aom_does_support_format2,
        aom_new_decoder2,
        aom_push_data2,
        aom_flush_data,
        aom_decode_next_image2
    };


const heif_decoder_plugin* get_decoder_plugin_aom()
{
  return &decoder_aom;
}


#if PLUGIN_AOM_DECODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_decoder,
  &decoder_aom
};
#endif
