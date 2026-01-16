/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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
#include "decoder_libde265.h"
#include <cassert>
#include <memory>
#include <cstring>
#include <string>

#include <libde265/de265.h>



struct libde265_decoder
{
  de265_decoder_context* ctx;
  bool strict_decoding = false;
  std::string error_message;
};

static const char kEmptyString[] = "";
static const char kSuccess[] = "Success";

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


static int libde265_does_support_format(heif_compression_format format)
{
  if (format == heif_compression_HEVC) {
    return LIBDE265_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}


[[maybe_unused]]
static int libde265_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return libde265_does_support_format(format->format);
}



static heif_error convert_libde265_image_to_heif_image(libde265_decoder* decoder,
                                                       const de265_image* de265img,
                                                       heif_image** image,
                                                       const heif_security_limits* limits)
{
  bool is_mono = (de265_get_chroma_format(de265img) == de265_chroma_mono);

  heif_error err;
  err = heif_image_create(de265_get_image_width(de265img, 0),
                          de265_get_image_height(de265img, 0),
                          is_mono ? heif_colorspace_monochrome : heif_colorspace_YCbCr,
                          (heif_chroma) de265_get_chroma_format(de265img),
                          image);
  if (err.code) {
    return err;
  }

  // --- transfer data from de265_image to HeifPixelImage

  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  int bpp = de265_get_bits_per_pixel(de265img, 0);

  int num_planes = (is_mono ? 1 : 3);

  for (int c = 0; c < num_planes; c++) {
    if (de265_get_bits_per_pixel(de265img, c) != bpp) {
      heif_image_release(*image);
      err = {heif_error_Unsupported_feature,
             heif_suberror_Unsupported_color_conversion,
             "Channels with different number of bits per pixel are not supported"};
      return err;
    }

    int stride;
    const uint8_t* data = de265_get_image_plane(de265img, c, &stride);

    int w = de265_get_image_width(de265img, c);
    int h = de265_get_image_height(de265img, c);
    if (w <= 0 || h <= 0) {
      heif_image_release(*image);
      err = {heif_error_Decoder_plugin_error,
             heif_suberror_Invalid_image_size,
             kEmptyString};
      return err;
    }

    err = heif_image_add_plane_safe(*image, channel2plane[c], w,h, bpp, limits);
    if (err.code) {
      // copy error message to decoder object because heif_image will be released
      decoder->error_message = err.message;
      err.message = decoder->error_message.c_str();

      heif_image_release(*image);
      return err;
    }

    size_t dst_stride;
    uint8_t* dst_mem = heif_image_get_plane2(*image, channel2plane[c], &dst_stride);

    int bytes_per_pixel = (bpp + 7) / 8;

    for (int y = 0; y < h; y++) {
      memcpy(dst_mem + y * dst_stride, data + y * stride, w * bytes_per_pixel);
    }
  }


  return {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
}



// Create a new decoder context for decoding an image
heif_error libde265_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  libde265_decoder* decoder = new libde265_decoder();
  heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};

  decoder->ctx = de265_new_decoder();
#if defined(__EMSCRIPTEN__)
  // Speed up decoding from JavaScript.
  de265_set_parameter_bool(decoder->ctx, DE265_DECODER_PARAM_DISABLE_DEBLOCKING, 1);
  de265_set_parameter_bool(decoder->ctx, DE265_DECODER_PARAM_DISABLE_SAO, 1);
#else
  int nThreads = (options->num_threads ? options->num_threads : 1);

  // Worker threads are not supported when running on Emscripten.
  de265_start_worker_threads(decoder->ctx, nThreads);
#endif

  decoder->strict_decoding = options->strict_decoding;

  *dec = decoder;
  return err;
}


static heif_error libde265_new_decoder(void** dec)
{
  heif_decoder_plugin_options options;
  options.format = heif_compression_HEVC;
  options.num_threads = 0;
  options.strict_decoding = false;

  return libde265_new_decoder2(dec, &options);
}

static void libde265_free_decoder(void* decoder_raw)
{
  libde265_decoder* decoder = (libde265_decoder*) decoder_raw;

  de265_error err = de265_free_decoder(decoder->ctx);
  (void) err;

  delete decoder;
}


void libde265_set_strict_decoding(void* decoder_raw, int flag)
{
  libde265_decoder* decoder = (libde265_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}


#if LIBDE265_NUMERIC_VERSION >= 0x02000000

static heif_error libde265_v2_push_data(void* decoder_raw, const void* data, size_t size)
{
  libde265_decoder* decoder = (libde265_decoder*)decoder_raw;

  const uint8_t* cdata = (const uint8_t*)data;

  size_t ptr=0;
  while (ptr < size) {
    if (4 > size - ptr) {
      return { heif_error_Decoder_plugin_error,
               heif_suberror_End_of_data,
               kEmptyString };
    }

    // TODO: the size of the NAL unit length variable is defined in the hvcC header.
    // We should not assume that it is always 4 bytes.
    uint32_t nal_size = (uint32_t)((cdata[ptr]<<24) | (cdata[ptr+1]<<16) | (cdata[ptr+2]<<8) | (cdata[ptr+3]));
    ptr+=4;

    if (nal_size > size - ptr) {
      //sstr << "NAL size (" << size32 << ") exceeds available data in file ("
      //<< data_bytes_left_to_read << ")";

      return { heif_error_Decoder_plugin_error,
               heif_suberror_End_of_data,
               kEmptyString };
    }

    de265_push_NAL(decoder->ctx, cdata+ptr, nal_size, 0, nullptr);
    ptr += nal_size;
  }


  return { heif_error_Ok, heif_suberror_Unspecified, kSuccess };
}


static heif_error libde265_v2_decode_next_image(void* decoder_raw,
                                                heif_image** out_img,
                                                const heif_security_limits* limits)
{
  libde265_decoder* decoder = (libde265_decoder*)decoder_raw;

  de265_push_end_of_stream(decoder->ctx);

  int action = de265_get_action(decoder->ctx, 1);

  // TODO: read NCLX from h265 bitstream

  // TODO(farindk): Set "err" if no image was decoded.
  if (action==de265_action_get_image) {
    const de265_image* img = de265_get_next_picture(decoder->ctx);
    if (img) {
      heif_error err = convert_libde265_image_to_heif_image(decoder, img,
                                                                   out_img, limits);
      de265_release_picture(img);

      return err;
    }
  }

  return { heif_error_Decoder_plugin_error, heif_suberror_Unspecified, kEmptyString };
}

static heif_error libde265_v2_decode_image(void* decoder_raw,
                                           heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return libde265_v2_decode_next_image(decoder_raw, out_img, limits);
}
#else

static heif_error libde265_v1_push_data2(void* decoder_raw, const void* data, size_t size, uintptr_t user_data)
{
  libde265_decoder* decoder = (libde265_decoder*) decoder_raw;

  const uint8_t* cdata = (const uint8_t*) data;

  size_t ptr = 0;
  while (ptr < size) {
    if (4 > size - ptr) {
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_End_of_data,
        kEmptyString
      };
    }

    uint32_t nal_size = static_cast<uint32_t>((cdata[ptr] << 24) | (cdata[ptr + 1] << 16) | (cdata[ptr + 2] << 8) | (cdata[ptr + 3]));
    ptr += 4;

    if (nal_size > size - ptr) {
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_End_of_data,
        kEmptyString
      };
    }

#if 0
    FILE* fh = fopen("data.h265", "a");
    fputc(0, fh);
    fputc(0, fh);
    fputc(1, fh);
    fwrite(cdata + ptr, nal_size, 1, fh);
    fclose(fh);

    printf("put nal with size %d %x\n", nal_size, *(cdata+ptr));
#endif

    de265_push_NAL(decoder->ctx, cdata + ptr, nal_size, 0, (void*)user_data);
    ptr += nal_size;
  }

  // TODO(farindk): Set "err" if data could not be pushed
  //de265_push_data(decoder->ctx, data, size, 0, nullptr);

  return {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
}

static heif_error libde265_v1_push_data(void* decoder_raw, const void* data, size_t size)
{
  return libde265_v1_push_data2(decoder_raw, data, size, 0);
}

static heif_error libde265_flush_data(void* decoder_raw)
{
  libde265_decoder* decoder = (libde265_decoder*) decoder_raw;

  de265_flush_data(decoder->ctx);

  return heif_error_ok;
}



static heif_error libde265_v1_decode_next_image2(void* decoder_raw,
                                                 heif_image** out_img,
                                                 uintptr_t* out_user_data,
                                                 const heif_security_limits* limits)
{
  libde265_decoder* decoder = (libde265_decoder*) decoder_raw;
  heif_error err = {heif_error_Ok, heif_suberror_Unspecified, kSuccess};

  // TODO(251119) : de265_flush_data(decoder->ctx);

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

    const de265_image* image = de265_get_next_picture(decoder->ctx);
    if (image) {
      // TODO(farindk): Should we return the first image instead?
      if (*out_img) {
        heif_image_release(*out_img);
      }

      if (out_user_data) {
        *out_user_data = (uintptr_t)de265_get_image_user_data(image);
      }

      err = convert_libde265_image_to_heif_image(decoder, image, out_img, limits);
      if (err.code != heif_error_Ok) {
        return err;
      }

      heif_color_profile_nclx* nclx = heif_nclx_color_profile_alloc();
#if LIBDE265_NUMERIC_VERSION >= 0x01000700
      HEIF_WARN_OR_FAIL(decoder->strict_decoding, *out_img, heif_nclx_color_profile_set_color_primaries(nclx, static_cast<uint16_t>(de265_get_image_colour_primaries(image))),
                        {
                          heif_nclx_color_profile_free(nclx);
                          heif_image_release(*out_img);
                          *out_img = nullptr;
                        });
      HEIF_WARN_OR_FAIL(decoder->strict_decoding, *out_img, heif_nclx_color_profile_set_transfer_characteristics(nclx, static_cast<uint16_t>(de265_get_image_transfer_characteristics(image))),
                        {
                          heif_nclx_color_profile_free(nclx);
                          heif_image_release(*out_img);
                          *out_img = nullptr;
                        });
      HEIF_WARN_OR_FAIL(decoder->strict_decoding, *out_img, heif_nclx_color_profile_set_matrix_coefficients(nclx, static_cast<uint16_t>(de265_get_image_matrix_coefficients(image))),
                        {
                          heif_nclx_color_profile_free(nclx);
                          heif_image_release(*out_img);
                          *out_img = nullptr;
                        });
      nclx->full_range_flag = (bool) de265_get_image_full_range_flag(image);
#endif
      heif_image_set_nclx_color_profile(*out_img, nclx);
      heif_nclx_color_profile_free(nclx);

      de265_release_next_picture(decoder->ctx);
      return heif_error_ok;
    }
  } while (more);

  return err;
}


static heif_error libde265_v1_decode_next_image(void* decoder_raw,
                                                heif_image** out_img,
                                                const heif_security_limits* limits)
{
  return libde265_v1_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}

static heif_error libde265_v1_decode_image(void* decoder_raw,
                                           heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return libde265_v1_decode_next_image(decoder_raw, out_img, limits);
}

#endif


#if LIBDE265_NUMERIC_VERSION >= 0x02000000

static const heif_decoder_plugin decoder_libde265
{
  1,
  libde265_plugin_name,
  libde265_init_plugin,
  libde265_deinit_plugin,
  libde265_does_support_format,
  libde265_new_decoder,
  libde265_free_decoder,
  libde265_v2_push_data,
  libde265_v2_decode_image,
  libde265_set_strict_decoding,
  "libde265",
  libde265_v2_decode_next_image
};

#else

static const heif_decoder_plugin decoder_libde265
    {
        5,
        libde265_plugin_name,
        libde265_init_plugin,
        libde265_deinit_plugin,
        libde265_does_support_format,
        libde265_new_decoder,
        libde265_free_decoder,
        libde265_v1_push_data,
        libde265_v1_decode_image,
        libde265_set_strict_decoding,
        "libde265",
        libde265_v1_decode_next_image,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        libde265_does_support_format2,
        libde265_new_decoder2,
        libde265_v1_push_data2,
        libde265_flush_data,
        libde265_v1_decode_next_image2
    };

#endif

const heif_decoder_plugin* get_decoder_plugin_libde265()
{
  return &decoder_libde265;
}



#if PLUGIN_LIBDE265
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_decoder,
  &decoder_libde265
};
#endif
