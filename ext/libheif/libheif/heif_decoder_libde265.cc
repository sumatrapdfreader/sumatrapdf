/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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
#include "heif_colorconversion.h"
#include "heif_api_structs.h"

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <assert.h>
#include <memory>
#include <cstring>

#include <libde265/de265.h>

using namespace heif;


struct libde265_decoder
{
  de265_decoder_context* ctx;
  bool strict_decoding = false;
};

static const char kEmptyString[] = "";

static const int LIBDE265_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static const char* libde265_plugin_name()
{
  strcpy(plugin_name, "libde265 HEVC decoder");

  const char* libde265_version = de265_get_version();

  if (strlen(libde265_version) + 10 < MAX_PLUGIN_NAME_LENGTH) {
    strcat(plugin_name, ", version ");
    strcat(plugin_name, libde265_version);
  }

  return plugin_name;
}


static void libde265_init_plugin()
{
  de265_init();
}


static void libde265_deinit_plugin()
{
  de265_free();
}


static int libde265_does_support_format(enum heif_compression_format format)
{
  if (format == heif_compression_HEVC) {
    return LIBDE265_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}


static struct heif_error convert_libde265_image_to_heif_image(struct libde265_decoder* decoder,
                                                              const struct de265_image* de265img,
                                                              struct heif_image** image)
{
  bool is_mono = (de265_get_chroma_format(de265img) == de265_chroma_mono);

  std::shared_ptr<HeifPixelImage> yuv_img = std::make_shared<HeifPixelImage>();
  yuv_img->create(de265_get_image_width(de265img, 0),
                  de265_get_image_height(de265img, 0),
                  is_mono ? heif_colorspace_monochrome : heif_colorspace_YCbCr,
                  (heif_chroma) de265_get_chroma_format(de265img));

  // --- transfer data from de265_image to HeifPixelImage

  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  int bpp = de265_get_bits_per_pixel(de265img, 0);

  int nPlanes = (is_mono ? 1 : 3);

  for (int c = 0; c < nPlanes; c++) {
    if (de265_get_bits_per_pixel(de265img, c) != bpp) {
      struct heif_error err = {heif_error_Unsupported_feature,
                               heif_suberror_Unsupported_color_conversion,
                               "Channels with different number of bits per pixel are not supported"};
      return err;
    }

    int stride;
    const uint8_t* data = de265_get_image_plane(de265img, c, &stride);

    int w = de265_get_image_width(de265img, c);
    int h = de265_get_image_height(de265img, c);
    if (w <= 0 || h <= 0) {
      struct heif_error err = {heif_error_Decoder_plugin_error,
                               heif_suberror_Invalid_image_size,
                               kEmptyString};
      return err;
    }

    if (!yuv_img->add_plane(channel2plane[c], w, h, bpp)) {
      struct heif_error err = {heif_error_Memory_allocation_error,
                               heif_suberror_Unspecified,
                               "Cannot allocate memory for image plane"};
      return err;
    }

    int dst_stride;
    uint8_t* dst_mem = yuv_img->get_plane(channel2plane[c], &dst_stride);

    int bytes_per_pixel = (bpp + 7) / 8;

    for (int y = 0; y < h; y++) {
      memcpy(dst_mem + y * dst_stride, data + y * stride, w * bytes_per_pixel);
    }
  }


  *image = new heif_image;
  (*image)->image = yuv_img;

  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


static struct heif_error libde265_new_decoder(void** dec)
{
  struct libde265_decoder* decoder = new libde265_decoder();
  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};

  decoder->ctx = de265_new_decoder();
#if defined(__EMSCRIPTEN__)
  // Speed up decoding from JavaScript.
  de265_set_parameter_bool(decoder->ctx, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, 1);
  de265_set_parameter_bool(decoder->ctx, DE265_DECODER_PARAM_DISABLE_SAO, 1);
#else
  // Worker threads are not supported when running on Emscripten.
  de265_start_worker_threads(decoder->ctx, 1);
#endif

  *dec = decoder;
  return err;
}

static void libde265_free_decoder(void* decoder_raw)
{
  struct libde265_decoder* decoder = (struct libde265_decoder*) decoder_raw;

  de265_error err = de265_free_decoder(decoder->ctx);
  (void) err;

  delete decoder;
}


void libde265_set_strict_decoding(void* decoder_raw, int flag)
{
  struct libde265_decoder* decoder = (libde265_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}


#if LIBDE265_NUMERIC_VERSION >= 0x02000000

static struct heif_error libde265_v2_push_data(void* decoder_raw, const void* data, size_t size)
{
  struct libde265_decoder* decoder = (struct libde265_decoder*)decoder_raw;

  const uint8_t* cdata = (const uint8_t*)data;

  size_t ptr=0;
  while (ptr < size) {
    if (4 > size - ptr) {
      struct heif_error err = { heif_error_Decoder_plugin_error,
                                heif_suberror_End_of_data,
                                kEmptyString };
      return err;
    }

    // TODO: the size of the NAL unit length variable is defined in the hvcC header.
    // We should not assume that it is always 4 bytes.
    uint32_t nal_size = (cdata[ptr]<<24) | (cdata[ptr+1]<<16) | (cdata[ptr+2]<<8) | (cdata[ptr+3]);
    ptr+=4;

    if (nal_size > size - ptr) {
      //sstr << "NAL size (" << size32 << ") exceeds available data in file ("
      //<< data_bytes_left_to_read << ")";

      struct heif_error err = { heif_error_Decoder_plugin_error,
                                heif_suberror_End_of_data,
                                kEmptyString };
      return err;
    }

    de265_push_NAL(decoder->ctx, cdata+ptr, nal_size, 0, nullptr);
    ptr += nal_size;
  }


  struct heif_error err = { heif_error_Ok, heif_suberror_Unspecified, kSuccess };
  return err;
}


static struct heif_error libde265_v2_decode_image(void* decoder_raw,
                                                  struct heif_image** out_img)
{
  struct libde265_decoder* decoder = (struct libde265_decoder*)decoder_raw;

  de265_push_end_of_stream(decoder->ctx);

  int action = de265_get_action(decoder->ctx, 1);

  // TODO: read NCLX from h265 bitstream

  // TODO(farindk): Set "err" if no image was decoded.
  if (action==de265_action_get_image) {
    const de265_image* img = de265_get_next_picture(decoder->ctx);
    if (img) {
      struct heif_error err = convert_libde265_image_to_heif_image(decoder, img,
                                                                   out_img);
      de265_release_picture(img);

      return err;
    }
  }

  struct heif_error err = { heif_error_Decoder_plugin_error, heif_suberror_Unspecified, kEmptyString };
  return err;
}

#else

static struct heif_error libde265_v1_push_data(void* decoder_raw, const void* data, size_t size)
{
  struct libde265_decoder* decoder = (struct libde265_decoder*) decoder_raw;

  const uint8_t* cdata = (const uint8_t*) data;

  size_t ptr = 0;
  while (ptr < size) {
    if (4 > size - ptr) {
      struct heif_error err = {heif_error_Decoder_plugin_error,
                               heif_suberror_End_of_data,
                               kEmptyString};
      return err;
    }

    uint32_t nal_size = (cdata[ptr] << 24) | (cdata[ptr + 1] << 16) | (cdata[ptr + 2] << 8) | (cdata[ptr + 3]);
    ptr += 4;

    if (nal_size > size - ptr) {
      struct heif_error err = {heif_error_Decoder_plugin_error,
                               heif_suberror_End_of_data,
                               kEmptyString};
      return err;
    }

    de265_push_NAL(decoder->ctx, cdata + ptr, nal_size, 0, nullptr);
    ptr += nal_size;
  }

  // TODO(farindk): Set "err" if data could not be pushed
  //de265_push_data(decoder->ctx, data, size, 0, nullptr);

  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
  return err;
}


static struct heif_error libde265_v1_decode_image(void* decoder_raw,
                                                  struct heif_image** out_img)
{
  struct libde265_decoder* decoder = (struct libde265_decoder*) decoder_raw;
  struct heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};

  de265_flush_data(decoder->ctx);

  // TODO(farindk): Set "err" if no image was decoded.
  int more;
  de265_error decode_err;
  *out_img = nullptr;
  do {
    more = 0;
    decode_err = de265_decode(decoder->ctx, &more);
    if (decode_err != DE265_OK) {
      // printf("Error decoding: %s (%d)\n", de265_get_error_text(decode_err), decode_err);
      break;
    }

    // TODO: read NCLX from h265 bitstream

    const struct de265_image* image = de265_get_next_picture(decoder->ctx);
    if (image) {
      // TODO(farindk): Should we return the first image instead?
      if (*out_img) {
        heif_image_release(*out_img);
      }
      err = convert_libde265_image_to_heif_image(decoder, image, out_img);
      if (err.code != heif_error_Ok) {
        return err;
      }

      struct heif_color_profile_nclx* nclx = heif_nclx_color_profile_alloc();
#if LIBDE265_NUMERIC_VERSION >= 0x01000700
      HEIF_WARN_OR_FAIL(decoder->strict_decoding, *out_img, heif_nclx_color_profile_set_color_primaries(nclx, (uint16_t)de265_get_image_colour_primaries(image)));
      HEIF_WARN_OR_FAIL(decoder->strict_decoding, *out_img, heif_nclx_color_profile_set_transfer_characteristics(nclx, (uint16_t)de265_get_image_transfer_characteristics(image)));
      HEIF_WARN_OR_FAIL(decoder->strict_decoding, *out_img, heif_nclx_color_profile_set_matrix_coefficients(nclx, (uint16_t)de265_get_image_matrix_coefficients(image)));
      nclx->full_range_flag = (bool)de265_get_image_full_range_flag(image);
#endif
      heif_image_set_nclx_color_profile(*out_img, nclx);
      heif_nclx_color_profile_free(nclx);

      de265_release_next_picture(decoder->ctx);
    }
  } while (more);

  return err;
}


#endif


#if LIBDE265_NUMERIC_VERSION >= 0x02000000

static const struct heif_decoder_plugin decoder_libde265
{
  1,
  libde265_plugin_name,
  libde265_init_plugin,
  libde265_deinit_plugin,
  libde265_does_support_format,
  libde265_new_decoder,
  libde265_free_decoder,
  libde265_v2_push_data,
  libde265_v2_decode_image
};

#else

static const struct heif_decoder_plugin decoder_libde265
    {
        2,
        libde265_plugin_name,
        libde265_init_plugin,
        libde265_deinit_plugin,
        libde265_does_support_format,
        libde265_new_decoder,
        libde265_free_decoder,
        libde265_v1_push_data,
        libde265_v1_decode_image,
        libde265_set_strict_decoding
    };

#endif

const struct heif_decoder_plugin* get_decoder_plugin_libde265()
{
  return &decoder_libde265;
}
