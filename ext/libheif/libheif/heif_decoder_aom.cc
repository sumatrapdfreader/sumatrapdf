/*
 * AVIF codec.
 * Copyright (c) 2019 struktur AG, Dirk Farin <farin@struktur.de>
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <memory>
#include <cstring>
#include <cassert>

#include <aom/aom_decoder.h>
#include <aom/aomdx.h>


struct aom_decoder
{
  aom_codec_ctx_t codec;
  bool codec_initialized = false;

  aom_codec_iface_t* iface;

  bool strict_decoding = false;
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


static int aom_does_support_format(enum heif_compression_format format)
{
  if (format == heif_compression_AV1) {
    return AOM_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}


struct heif_error aom_new_decoder(void** dec)
{
  struct aom_decoder* decoder = new aom_decoder();

  decoder->iface = aom_codec_av1_dx();

  aom_codec_err_t aomerr = aom_codec_dec_init(&decoder->codec, decoder->iface, NULL, 0);
  if (aomerr) {
    *dec = NULL;

    delete decoder;

    struct heif_error err = {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, aom_codec_err_to_string(aomerr)};
    return err;
  }

  decoder->codec_initialized = true;
  *dec = decoder;

  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


void aom_free_decoder(void* decoder_raw)
{
  struct aom_decoder* decoder = (aom_decoder*) decoder_raw;

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
  struct aom_decoder* decoder = (aom_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}


struct heif_error aom_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  struct aom_decoder* decoder = (struct aom_decoder*) decoder_raw;

  const char* ver = aom_codec_version_str();
  (void)ver;

  aom_codec_err_t aomerr;
  aomerr = aom_codec_decode(&decoder->codec, (const uint8_t*) frame_data, frame_size, NULL);
  if (aomerr) {
    struct heif_error err = {heif_error_Invalid_input, heif_suberror_Unspecified, aom_codec_err_to_string(aomerr)};
    return err;
  }


  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


struct heif_error aom_decode_image(void* decoder_raw, struct heif_image** out_img)
{
  struct aom_decoder* decoder = (struct aom_decoder*) decoder_raw;

  aom_codec_iter_t iter = NULL;
  aom_image_t* img = NULL;

  img = aom_codec_get_frame(&decoder->codec, &iter);

  if (img == NULL) {
    struct heif_error err = {heif_error_Decoder_plugin_error,
                             heif_suberror_Unspecified,
                             kEmptyString};
    return err;
  }


  if (img->fmt != AOM_IMG_FMT_I420 &&
      img->fmt != AOM_IMG_FMT_I42016 &&
      img->fmt != AOM_IMG_FMT_I422 &&
      img->fmt != AOM_IMG_FMT_I42216 &&
      img->fmt != AOM_IMG_FMT_I444 &&
      img->fmt != AOM_IMG_FMT_I44416) {
    struct heif_error err = {heif_error_Decoder_plugin_error,
                             heif_suberror_Unsupported_image_type,
                             kEmptyString};
    return err;
  }


  heif_chroma chroma;
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

  struct heif_image* heif_img = nullptr;
  struct heif_error err = heif_image_create(img->d_w, img->d_h,
                                            heif_colorspace_YCbCr,
                                            chroma,
                                            &heif_img);
  if (err.code != heif_error_Ok) {
    assert(heif_img==nullptr);
    return err;
  }


  // --- read nclx parameters from decoded AV1 bitstream

  heif_color_profile_nclx nclx;
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_color_primaries(&nclx, img->cp));
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_transfer_characteristics(&nclx, img->tc));
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_matrix_coefficients(&nclx, img->mc));
  nclx.full_range_flag = (img->range == AOM_CR_FULL_RANGE);
  heif_image_set_nclx_color_profile(heif_img, &nclx);


  // --- transfer data from aom_image_t to HeifPixelImage

  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  for (int c = 0; c < 3; c++) {
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

    err = heif_image_add_plane(heif_img, channel2plane[c], w, h, bpp);
    if (err.code != heif_error_Ok) {
      heif_image_release(heif_img);
      return err;
    }

    int dst_stride;
    uint8_t* dst_mem = heif_image_get_plane(heif_img, channel2plane[c], &dst_stride);

    int bytes_per_pixel = (bpp + 7) / 8;

    for (int y = 0; y < h; y++) {
      memcpy(dst_mem + y * dst_stride, data + y * stride, w * bytes_per_pixel);
    }
  }

  *out_img = heif_img;
  return err;
}


static const struct heif_decoder_plugin decoder_aom
    {
        2,
        aom_plugin_name,
        aom_init_plugin,
        aom_deinit_plugin,
        aom_does_support_format,
        aom_new_decoder,
        aom_free_decoder,
        aom_push_data,
        aom_decode_image,
        aom_set_strict_decoding
    };


const struct heif_decoder_plugin* get_decoder_plugin_aom()
{
  return &decoder_aom;
}
