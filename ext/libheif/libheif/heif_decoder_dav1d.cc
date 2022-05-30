/*
 * AVIF codec.
 * Copyright (c) 2020 struktur AG, Dirk Farin <farin@struktur.de>
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
#include "heif_limits.h"
#include "heif_image.h"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <memory>
#include <cstring>
#include <cassert>

#include <dav1d/version.h>
#include <dav1d/dav1d.h>

struct dav1d_decoder
{
  Dav1dSettings settings;
  Dav1dContext* context;
  Dav1dData data;
  bool strict_decoding = false;
};

static const char kEmptyString[] = "";

static const int DAV1D_PLUGIN_PRIORITY = 150;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static const char* dav1d_plugin_name()
{
  snprintf(plugin_name, MAX_PLUGIN_NAME_LENGTH, "dav1d v%d.%d.%d",
           DAV1D_API_VERSION_MAJOR,
           DAV1D_API_VERSION_MINOR,
           DAV1D_API_VERSION_PATCH);

  // make sure that the string is null-terminated
  plugin_name[MAX_PLUGIN_NAME_LENGTH - 1] = 0;

  return plugin_name;
}


static void dav1d_init_plugin()
{
}


static void dav1d_deinit_plugin()
{
}


static int dav1d_does_support_format(enum heif_compression_format format)
{
  if (format == heif_compression_AV1) {
    return DAV1D_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}


struct heif_error dav1d_new_decoder(void** dec)
{
  auto* decoder = new dav1d_decoder();

  dav1d_default_settings(&decoder->settings);

  decoder->settings.frame_size_limit = MAX_IMAGE_WIDTH * MAX_IMAGE_HEIGHT;
  decoder->settings.all_layers = 0;

  if (dav1d_open(&decoder->context, &decoder->settings) != 0) {
    struct heif_error err = {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, kSuccess};
    return err;
  }

  memset(&decoder->data, 0, sizeof(Dav1dData));

  *dec = decoder;

  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


void dav1d_free_decoder(void* decoder_raw)
{
  auto* decoder = (dav1d_decoder*) decoder_raw;

  if (!decoder) {
    return;
  }

  if (decoder->data.sz) {
    dav1d_data_unref(&decoder->data);
  }
  if (decoder->context) {
    dav1d_close(&decoder->context);
  }

  delete decoder;
}


void dav1d_set_strict_decoding(void* decoder_raw, int flag)
{
  struct dav1d_decoder* decoder = (dav1d_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}

struct heif_error dav1d_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  auto* decoder = (struct dav1d_decoder*) decoder_raw;

  assert(decoder->data.sz == 0);

  uint8_t* d = dav1d_data_create(&decoder->data, frame_size);
  if (d == nullptr) {
    struct heif_error err = {heif_error_Memory_allocation_error, heif_suberror_Unspecified, kSuccess};
    return err;
  }

  memcpy(d, frame_data, frame_size);

  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


struct heif_error dav1d_decode_image(void* decoder_raw, struct heif_image** out_img)
{
  auto* decoder = (struct dav1d_decoder*) decoder_raw;

  struct heif_error err;

  Dav1dPicture frame;
  memset(&frame, 0, sizeof(Dav1dPicture));

  bool flushed = false;

  for (;;) {

    int res = dav1d_send_data(decoder->context, &decoder->data);
    if ((res < 0) && (res != DAV1D_ERR(EAGAIN))) {
      err = {heif_error_Decoder_plugin_error,
             heif_suberror_Unspecified,
             kEmptyString};
      return err;
    }

    res = dav1d_get_picture(decoder->context, &frame);
    if (!flushed && res == DAV1D_ERR(EAGAIN)) {
      if (decoder->data.sz == 0) {
        flushed = true;
      }
      continue;
    }
    else if (res < 0) {
      err = {heif_error_Decoder_plugin_error,
             heif_suberror_Unspecified,
             kEmptyString};
      return err;
    }
    else {
      break;
    }
  }

  heif_chroma chroma;
  heif_colorspace colorspace;
  switch (frame.p.layout) {
    case DAV1D_PIXEL_LAYOUT_I420:
      chroma = heif_chroma_420;
      colorspace = heif_colorspace_YCbCr;
      break;
    case DAV1D_PIXEL_LAYOUT_I422:
      chroma = heif_chroma_422;
      colorspace = heif_colorspace_YCbCr;
      break;
    case DAV1D_PIXEL_LAYOUT_I444:
      chroma = heif_chroma_444;
      colorspace = heif_colorspace_YCbCr;
      break;
    case DAV1D_PIXEL_LAYOUT_I400:
      chroma = heif_chroma_monochrome;
      colorspace = heif_colorspace_monochrome;
      break;
    default: {
      err = {heif_error_Decoder_plugin_error,
             heif_suberror_Unspecified,
             kEmptyString};
      return err;
    }
  }


  struct heif_image* heif_img = nullptr;
  err = heif_image_create(frame.p.w, frame.p.h,
                          colorspace,
                          chroma,
                          &heif_img);
  if (err.code != heif_error_Ok) {
    assert(heif_img == nullptr);
    return err;
  }


  // --- read nclx parameters from decoded AV1 bitstream

  heif_color_profile_nclx nclx;
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_color_primaries(&nclx, frame.seq_hdr->pri));
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_transfer_characteristics(&nclx, frame.seq_hdr->trc));
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_matrix_coefficients(&nclx, frame.seq_hdr->mtrx));
  nclx.full_range_flag = (frame.seq_hdr->color_range != 0);
  heif_image_set_nclx_color_profile(heif_img, &nclx);



  // --- transfer data from aom_image_t to HeifPixelImage

  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  // --- copy image data

  for (int c = 0; c < 3; c++) {
    if (chroma == heif_chroma_monochrome && c > 0) {
      break;
    }

    int bpp = frame.p.bpc;

    const uint8_t* data = (uint8_t*) frame.data[c];
    int stride = (int) frame.stride[c > 0 ? 1 : 0];

    int w, h;
    heif::get_subsampled_size(frame.p.w, frame.p.h,
                              channel2plane[c], chroma, &w, &h);

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

  dav1d_picture_unref(&frame);

  *out_img = heif_img;


  err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


static const struct heif_decoder_plugin decoder_dav1d
    {
        2,
        dav1d_plugin_name,
        dav1d_init_plugin,
        dav1d_deinit_plugin,
        dav1d_does_support_format,
        dav1d_new_decoder,
        dav1d_free_decoder,
        dav1d_push_data,
        dav1d_decode_image,
        dav1d_set_strict_decoding
    };


const struct heif_decoder_plugin* get_decoder_plugin_dav1d()
{
  return &decoder_dav1d;
}
