/*
 * AVIF codec.
 * Copyright (c) 2020 Dirk Farin <dirk.farin@gmail.com>
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
#include "security_limits.h"
#include "common_utils.h"
#include "decoder_dav1d.h"
#include <cstring>
#include <cassert>
#include <cstdio>
#include <deque>
#include <limits>
#include <string>

#include <dav1d/version.h>
#include <dav1d/dav1d.h>


struct dav1d_decoder
{
  Dav1dSettings settings{};
  Dav1dContext* context{};
  std::deque<Dav1dData> queued_data;
  bool strict_decoding = false;
  std::string error_message;
};

static constexpr char kEmptyString[] = "";
static constexpr char kSuccess[] = "Success";

static constexpr int DAV1D_PLUGIN_PRIORITY = 150;

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


static int dav1d_does_support_format(heif_compression_format format)
{
  if (format == heif_compression_AV1) {
    return DAV1D_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}


static int dav1d_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return dav1d_does_support_format(format->format);
}

heif_error dav1d_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  auto* decoder = new dav1d_decoder();

  dav1d_default_settings(&decoder->settings);

  if (heif_get_global_security_limits()->max_image_size_pixels > std::numeric_limits<unsigned int>::max()) {
    decoder->settings.frame_size_limit = 0;
  }
  else {
    decoder->settings.frame_size_limit = static_cast<unsigned int>(heif_get_global_security_limits()->max_image_size_pixels);
  }

  decoder->settings.all_layers = 0;

  if (dav1d_open(&decoder->context, &decoder->settings) != 0) {
    delete decoder;
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, kSuccess};
  }

  if (options->num_threads) {
    decoder->settings.n_threads = options->num_threads;
  }

  *dec = decoder;

  return heif_error_ok;
}

heif_error dav1d_new_decoder(void** dec)
{
  struct heif_decoder_plugin_options options;
  options.format = heif_compression_AV1;
  options.strict_decoding = false;
  options.num_threads = 0;

  return dav1d_new_decoder2(dec, &options);
}

void dav1d_free_decoder(void* decoder_raw)
{
  auto* decoder = (dav1d_decoder*) decoder_raw;

  if (!decoder) {
    return;
  }

  // free queued data

  for (auto& pkt : decoder->queued_data) {
    dav1d_data_unref(&pkt);
  }
  decoder->queued_data.clear();

  // free decoder context

  if (decoder->context) {
    dav1d_close(&decoder->context);
  }

  delete decoder;
}


void dav1d_set_strict_decoding(void* decoder_raw, int flag)
{
  dav1d_decoder* decoder = (dav1d_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}


static heif_error push_pending_data_into_decoder(dav1d_decoder* decoder)
{
  while (!decoder->queued_data.empty()) {

    // send data

    int res = dav1d_send_data(decoder->context, &decoder->queued_data.front());

    // decoder does not accept more data at this moment

    if (res == DAV1D_ERR(EAGAIN)) {
      break;
    }

    // Decoder has accepted data. Remove packet and check for error.

    decoder->queued_data.pop_front();

    if ((res < 0) && (res != DAV1D_ERR(EAGAIN))) {
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_Unspecified,
        kEmptyString
      };
    }
  }

  return heif_error_ok;
}


heif_error dav1d_push_data2(void* decoder_raw, const void* frame_data, size_t frame_size, uintptr_t user_data)
{
  auto* decoder = (struct dav1d_decoder*) decoder_raw;

  // --- copy input data into Dav1dData packet

  Dav1dData packet{};

  uint8_t* d = dav1d_data_create(&packet, frame_size);
  if (d == nullptr) {
    return {heif_error_Memory_allocation_error, heif_suberror_Unspecified, kSuccess};
  }

  memcpy(d, frame_data, frame_size);

  packet.m.user_data.data = (uint8_t*)user_data;

  // --- put data into queue

  decoder->queued_data.push_back(packet);

  // --- push pending data to decoder

  return push_pending_data_into_decoder(decoder);
}


heif_error dav1d_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  return dav1d_push_data2(decoder_raw, frame_data, frame_size, 0);
}


heif_error dav1d_decode_next_image2(void* decoder_raw, heif_image** out_img,
                                    uintptr_t* out_user_data,
                                    const heif_security_limits* limits)
{
  auto* decoder = (struct dav1d_decoder*) decoder_raw;

  heif_error err;

  Dav1dPicture frame{};

  for (;;) {

    // --- send more pending data to decoder

    err = push_pending_data_into_decoder(decoder);
    if (err.code) {
      return err;
    }

    // --- try to get decoded image

    int res = dav1d_get_picture(decoder->context, &frame);

    // We got a picture from the decoder. Continue with processing it.
    if (res == 0) {
      break;
    }

    // decoder wants more data, but queue is empty
    if (res == DAV1D_ERR(EAGAIN) && decoder->queued_data.empty()) {
      *out_img = nullptr;
      return heif_error_ok;
    }

    // continue feeding more data from queue
    if (res == DAV1D_ERR(EAGAIN)) {
      continue;
    }

    // decoder error
    if (res < 0) {
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_Unspecified,
        kEmptyString
      };
    }
  }

  // --- convert image to heif_image

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
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_Unspecified,
        kEmptyString
      };
    }
  }

  if (out_user_data) {
    *out_user_data = (uintptr_t)frame.m.user_data.data;
  }

  heif_image* heif_img = nullptr;
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
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_color_primaries(&nclx, static_cast<uint16_t>(frame.seq_hdr->pri)), {});
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_transfer_characteristics(&nclx, static_cast<uint16_t>(frame.seq_hdr->trc)), {});
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_matrix_coefficients(&nclx, static_cast<uint16_t>(frame.seq_hdr->mtrx)), {});
  nclx.full_range_flag = (frame.seq_hdr->color_range != 0);
  heif_image_set_nclx_color_profile(heif_img, &nclx);


  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  // --- copy image data

  int num_planes = (chroma == heif_chroma_monochrome ? 1 : 3);

  for (int c = 0; c < num_planes; c++) {
    int bpp = frame.p.bpc;

    const uint8_t* data = (uint8_t*) frame.data[c];
    int stride = (int) frame.stride[c > 0 ? 1 : 0];

    uint32_t w, h;
    get_subsampled_size(frame.p.w, frame.p.h,
                        channel2plane[c], chroma, &w, &h);

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

    const int bytes_per_pixel = (bpp + 7) / 8;

    for (uint32_t y = 0; y < h; y++) {
      memcpy(dst_mem + y * dst_stride, data + y * stride, w * bytes_per_pixel);
    }
  }

  dav1d_picture_unref(&frame);

  *out_img = heif_img;


  return {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
}


heif_error dav1d_decode_next_image(void* decoder_raw, heif_image** out_img,
                                   const heif_security_limits* limits)
{
  return dav1d_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}


heif_error dav1d_decode_image(void* decoder_raw, struct heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return dav1d_decode_next_image(decoder_raw, out_img, limits);
}


heif_error dav1d_flush_data(void* decoder_raw)
{
  auto* decoder = (struct dav1d_decoder*) decoder_raw;

  constexpr Dav1dData packet{}; // empty packet

  // --- put data into queue

  decoder->queued_data.push_back(packet);

  // --- push pending data to decoder

  return push_pending_data_into_decoder(decoder);
}


static const heif_decoder_plugin decoder_dav1d
    {
        5,
        dav1d_plugin_name,
        dav1d_init_plugin,
        dav1d_deinit_plugin,
        dav1d_does_support_format,
        dav1d_new_decoder,
        dav1d_free_decoder,
        dav1d_push_data,
        dav1d_decode_image,
        dav1d_set_strict_decoding,
        "dav1d",
        dav1d_decode_next_image,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        dav1d_does_support_format2,
        dav1d_new_decoder2,
        dav1d_push_data2,
        dav1d_flush_data,
        dav1d_decode_next_image2
    };


const heif_decoder_plugin* get_decoder_plugin_dav1d()
{
  return &decoder_dav1d;
}


#if PLUGIN_DAV1D
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_decoder,
  &decoder_dav1d
};
#endif
