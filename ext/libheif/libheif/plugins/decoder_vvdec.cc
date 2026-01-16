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
#include "decoder_vvdec.h"
#include <cstring>
#include <cassert>
#include <vector>
#include <algorithm>
#include <deque>
#include <string>

#include <vvdec/vvdec.h>
#include <utility>

#if 0
#include <iostream>
#include <logging.h>
#endif


struct vvdec_decoder
{
  vvdecDecoder* decoder = nullptr;
  vvdecAccessUnit* au = nullptr;

  bool strict_decoding = false;

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t user_data;
  };
  std::deque<Packet> nalus;
  bool end_of_stream_reached = false;

  std::string error_message;
};

static const char kSuccess[] = "Success";

static const int VVDEC_PLUGIN_PRIORITY = 100;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static const char* vvdec_plugin_name()
{
  const char* version = vvdec_get_version();

  if (strlen(version) < 60) {
    sprintf(plugin_name, "VVCDEC decoder (%s)", version);
  }
  else {
    strcpy(plugin_name, "VVDEC decoder");
  }

  return plugin_name;
}


static void vvdec_init_plugin()
{
}


static void vvdec_deinit_plugin()
{
}


static int vvdec_does_support_format(heif_compression_format format)
{
  if (format == heif_compression_VVC) {
    return VVDEC_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}


static int vvdec_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return vvdec_does_support_format(format->format);
}

heif_error vvdec_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  auto* decoder = new vvdec_decoder();

  vvdecParams params;
  vvdec_params_default(&params);
  params.logLevel = VVDEC_INFO;

  if (options->num_threads) {
    params.threads = options->num_threads;
  }
  decoder->decoder = vvdec_decoder_open(&params);

  const int MaxNaluSize = 256 * 1024;
  decoder->au = vvdec_accessUnit_alloc();
  vvdec_accessUnit_default(decoder->au);
  vvdec_accessUnit_alloc_payload(decoder->au, MaxNaluSize);

  *dec = decoder;

  return {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
}


heif_error vvdec_new_decoder(void** dec)
{
  heif_decoder_plugin_options options;
  options.format = heif_compression_VVC;
  options.num_threads = 0;
  options.strict_decoding = false;

  vvdec_new_decoder2(dec, &options);

  return {};
}


void vvdec_free_decoder(void* decoder_raw)
{
  auto* decoder = (vvdec_decoder*) decoder_raw;

  if (!decoder) {
    return;
  }

  if (decoder->au) {
    vvdec_accessUnit_free(decoder->au);
    decoder->au = nullptr;
  }

  if (decoder->decoder) {
    vvdec_decoder_close(decoder->decoder);
    decoder->decoder = nullptr;
  }

  delete decoder;
}


void vvdec_set_strict_decoding(void* decoder_raw, int flag)
{
  auto* decoder = (vvdec_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}


heif_error vvdec_push_data2(void* decoder_raw, const void* frame_data, size_t frame_size,
                            uintptr_t user_data)
{
  auto* decoder = (vvdec_decoder*) decoder_raw;

  const auto* data = (const uint8_t*) frame_data;

  for (;;) {
    uint32_t size = four_bytes_to_uint32(data[0], data[1], data[2], data[3]);

    data += 4;

    std::vector<uint8_t> nalu;
    nalu.push_back(0);
    nalu.push_back(0);
    nalu.push_back(1);
    nalu.insert(nalu.end(), data, data + size);

    decoder->nalus.push_back({std::move(nalu), user_data});
    data += size;
    frame_size -= 4 + size;
    if (frame_size == 0) {
      break;
    }
  }

  return heif_error_ok;
}

heif_error vvdec_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  return vvdec_push_data2(decoder_raw, frame_data, frame_size, 0);
}

heif_error vvdec_decode_next_image2(void* decoder_raw, heif_image** out_img,
                                    uintptr_t* out_user_data,
                                    const heif_security_limits* limits)
{
  auto* decoder = (vvdec_decoder*) decoder_raw;

  vvdecFrame* frame = nullptr;

  // --- prepare AU payload with maximum NALU size

  size_t max_payload_size = 0;
  for (const auto& nalu : decoder->nalus) {
    max_payload_size = std::max(max_payload_size, nalu.data.size());
  }

  if (decoder->au == nullptr || max_payload_size > (size_t) decoder->au->payloadSize) {
    if (decoder->au) {
      vvdec_accessUnit_free(decoder->au);
    }

    decoder->au = vvdec_accessUnit_alloc();
    vvdec_accessUnit_default(decoder->au);
    vvdec_accessUnit_alloc_payload(decoder->au, (int)max_payload_size);
  }

  // --- feed NALUs into decoder, flush when done

  for (;;) {
    int ret;

    // -> end of stream reached
    if (decoder->nalus.empty() && decoder->end_of_stream_reached) {
      ret = vvdec_flush(decoder->decoder, &frame);
    }
    // -> not enough data to decode an image
    else if (decoder->nalus.empty()) {
      *out_img = nullptr;
      return heif_error_ok;
    }
    // -> push NALs from queue into decoder
    else {
      const auto& nalu = decoder->nalus.front();

      memcpy(decoder->au->payload, nalu.data.data(), nalu.data.size());
      decoder->au->payloadUsedSize = (int) nalu.data.size();
      decoder->au->cts = nalu.user_data;
      decoder->au->ctsValid = true;
      decoder->nalus.pop_front();
      ret = vvdec_decode(decoder->decoder, decoder->au, &frame);
    }

    if (ret == VVDEC_OK && frame) {
      break;
    }

    if (ret == VVDEC_EOF) {
      assert(!frame);
      *out_img = nullptr;
      return heif_error_ok;
    }

    if (ret != VVDEC_OK && ret != VVDEC_TRY_AGAIN) {
      return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "vvdec decoding error"};
    }
  }

  if (out_user_data) {
    *out_user_data = frame->cts;
  }


  // --- convert decoded frame to heif_image

  heif_chroma chroma;
  heif_colorspace colorspace;

  if (frame->colorFormat == VVDEC_CF_YUV400_PLANAR) {
    chroma = heif_chroma_monochrome;
    colorspace = heif_colorspace_monochrome;
  }
  else {
    if (frame->colorFormat == VVDEC_CF_YUV444_PLANAR) {
      chroma = heif_chroma_444;
    }
    else if (frame->colorFormat == VVDEC_CF_YUV422_PLANAR) {
      chroma = heif_chroma_422;
    }
    else {
      chroma = heif_chroma_420;
    }
    colorspace = heif_colorspace_YCbCr;
  }

  heif_image* heif_img = nullptr;
  heif_error err = heif_image_create((int) frame->width,
                                     (int) frame->height,
                                     colorspace,
                                     chroma,
                                     &heif_img);
  if (err.code != heif_error_Ok) {
    assert(heif_img == nullptr);
    return err;
  }


  // --- read nclx parameters from decoded AV1 bitstream

#if 0
  heif_color_profile_nclx nclx;
  nclx.version = 1;
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_color_primaries(&nclx, static_cast<uint16_t>(img->cp)), { heif_image_release(heif_img); });
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_transfer_characteristics(&nclx, static_cast<uint16_t>(img->tc)), { heif_image_release(heif_img); });
  HEIF_WARN_OR_FAIL(decoder->strict_decoding, heif_img, heif_nclx_color_profile_set_matrix_coefficients(&nclx, static_cast<uint16_t>(img->mc)), { heif_image_release(heif_img); });
  nclx.full_range_flag = (img->range == AOM_CR_FULL_RANGE);
  heif_image_set_nclx_color_profile(heif_img, &nclx);
#endif

  // --- transfer data from vvdecFrame to HeifPixelImage

  heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
  };


  int num_planes = (chroma == heif_chroma_monochrome ? 1 : 3);

  for (int c = 0; c < num_planes; c++) {
    int bpp = (int)frame->bitDepth;

    const auto& plane = frame->planes[c];
    const uint8_t* data = plane.ptr;
    int stride = (int)plane.stride;

    int w = (int)plane.width;
    int h = (int)plane.height;

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

#if 0
      std::cout << "DATA " << c << " " << w << " " << h << " bpp:" << bpp << "\n";
      std::cout << write_raw_data_as_hex(dst_mem, w*h, {}, {});
      std::cout << "---\n";
#endif
  }

  *out_img = heif_img;

  vvdec_frame_unref(decoder->decoder, frame);

  return err;
}

heif_error vvdec_decode_next_image(void* decoder_raw, heif_image** out_img,
                                   const heif_security_limits* limits)
{
  return vvdec_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}

heif_error vvdec_decode_image(void* decoder_raw, heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return vvdec_decode_next_image(decoder_raw, out_img, limits);
}

heif_error vvdec_flush_data(void* decoder_raw)
{
  auto* decoder = (vvdec_decoder*) decoder_raw;
  decoder->end_of_stream_reached = true;
  return heif_error_ok;
}

static const heif_decoder_plugin decoder_vvdec
    {
      5,
      vvdec_plugin_name,
      vvdec_init_plugin,
      vvdec_deinit_plugin,
      vvdec_does_support_format,
      vvdec_new_decoder,
      vvdec_free_decoder,
      vvdec_push_data,
      vvdec_decode_image,
      vvdec_set_strict_decoding,
      "vvdec",
      vvdec_decode_next_image,
      /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
      vvdec_does_support_format2,
      vvdec_new_decoder2,
      vvdec_push_data2,
      vvdec_flush_data,
      vvdec_decode_next_image2
    };


const heif_decoder_plugin* get_decoder_plugin_vvdec()
{
  return &decoder_vvdec;
}


#if PLUGIN_VVDEC
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_decoder,
  &decoder_vvdec
};
#endif
