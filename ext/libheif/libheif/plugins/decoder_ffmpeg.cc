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
#include "decoder_ffmpeg.h"
#include "nalu_utils.h"
#include <string>

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif


#include <deque>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

extern "C"
{
    #include <libavcodec/avcodec.h>
}


struct ffmpeg_decoder
{
  // --- input data

  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t user_data;
  };

  std::deque<Packet> input_data;
  bool strict_decoding = false;

  // --- decoder

  const AVCodec* av_codec = NULL;
  AVCodecParserContext* av_codec_parser_context = NULL;
  AVCodecContext* av_codec_context = NULL;

  std::string error_message;

  ~ffmpeg_decoder()
  {
    if (av_codec_parser_context) av_parser_close(av_codec_parser_context);
    if (av_codec_context) avcodec_free_context(&av_codec_context);
  }
};

static const int FFMPEG_DECODER_PLUGIN_PRIORITY = 90;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];


static const char* ffmpeg_plugin_name()
{
  snprintf(plugin_name, MAX_PLUGIN_NAME_LENGTH, "FFMPEG AVC/HEVC decoder %s", av_version_info());
  plugin_name[MAX_PLUGIN_NAME_LENGTH - 1] = 0; //null-terminated

  return plugin_name;
}


static void ffmpeg_init_plugin()
{

}


static void ffmpeg_deinit_plugin()
{

}


static int ffmpeg_does_support_format(heif_compression_format format)
{
  // TODO: it should work at least also for AV1 and JPEG. Check why it isn't decoding.

  if (format == heif_compression_HEVC) {
    return FFMPEG_DECODER_PLUGIN_PRIORITY;
  }
  else if (format == heif_compression_AVC) {
    return FFMPEG_DECODER_PLUGIN_PRIORITY;
  }
#if 0
  else if (format == heif_compression_VVC) {
    return FFMPEG_DECODER_PLUGIN_PRIORITY;
  }
  else if (format == heif_compression_AV1) {
    return FFMPEG_DECODER_PLUGIN_PRIORITY;
  }
  else if (format == heif_compression_JPEG) {
    return FFMPEG_DECODER_PLUGIN_PRIORITY;
  }
  else if (format == heif_compression_JPEG2000 ||
           format == heif_compression_HTJ2K) {
    return FFMPEG_DECODER_PLUGIN_PRIORITY;
  }
#endif
  else {
    return 0;
  }
}


static int ffmpeg_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return ffmpeg_does_support_format(format->format);
}

static heif_error ffmpeg_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  ffmpeg_decoder* decoder = new ffmpeg_decoder();
  *dec = decoder;

  // Find matching video decoder
  switch (options->format) {
    case heif_compression_AVC:
      decoder->av_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
      break;
    case heif_compression_HEVC:
      decoder->av_codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
      break;
#if 0
    case heif_compression_VVC:
      decoder->av_codec = avcodec_find_decoder(AV_CODEC_ID_VVC);
      break;
    case heif_compression_AV1:
      decoder->av_codec = avcodec_find_decoder(AV_CODEC_ID_AV1);
      break;
    case heif_compression_JPEG:
      decoder->av_codec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
      break;
    case heif_compression_JPEG2000:
    case heif_compression_HTJ2K:
      decoder->av_codec = avcodec_find_decoder(AV_CODEC_ID_JPEG2000);
      break;
#endif
    default:
      assert(false);
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "FFMPEG plugin started with unsupported codec."
      };
  }

  if (!decoder->av_codec) {
    return { heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "avcodec_find_decoder() returned error" };
  }

  decoder->av_codec_parser_context = av_parser_init(decoder->av_codec->id);
  if (!decoder->av_codec_parser_context) {
    return { heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "av_parser_init returned error" };
  }

  decoder->av_codec_context = avcodec_alloc_context3(decoder->av_codec);
  if (!decoder->av_codec_context) {
    return { heif_error_Memory_allocation_error, heif_suberror_Unspecified, "avcodec_alloc_context3 returned error" };
  }

  /* open it */
  if (avcodec_open2(decoder->av_codec_context, decoder->av_codec, NULL) < 0) {
    return { heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "avcodec_open2 returned error" };
  }


  return heif_error_success;
}

static heif_error ffmpeg_new_decoder(void** dec)
{
  heif_decoder_plugin_options options;
  options.format = heif_compression_HEVC;
  options.num_threads = 0;
  options.strict_decoding = false;

  return ffmpeg_new_decoder2(dec, &options);
}

static void ffmpeg_free_decoder(void* decoder_raw)
{
  ffmpeg_decoder* decoder = (struct ffmpeg_decoder*) decoder_raw;

  delete decoder;
}


void ffmpeg_set_strict_decoding(void* decoder_raw, int flag)
{
  ffmpeg_decoder* decoder = (ffmpeg_decoder*) decoder_raw;

  decoder->strict_decoding = flag;
}

static heif_error ffmpeg_push_data2(void *decoder_raw, const void *data, size_t size, uintptr_t user_data)
{
  ffmpeg_decoder* decoder = (struct ffmpeg_decoder*) decoder_raw;

  const uint8_t* cdata = (const uint8_t*) data;

  ffmpeg_decoder::Packet pkt;

  size_t ptr = 0;
  while (ptr < size)
  {
    if (size - ptr < 4) {
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_End_of_data,
        "insufficient data"
      };
    }

    uint32_t nal_size = four_bytes_to_uint32(cdata[ptr + 0],
                                             cdata[ptr + 1],
                                             cdata[ptr + 2],
                                             cdata[ptr + 3]);
    ptr += 4;

    if (nal_size > size - ptr) {
      return {
        heif_error_Decoder_plugin_error,
        heif_suberror_End_of_data,
        "insufficient data"
      };
    }

    pkt.data.push_back(0);
    pkt.data.push_back(0);
    pkt.data.push_back(1);
    pkt.data.insert(pkt.data.end(), cdata + ptr, cdata + ptr + nal_size);

    ptr += nal_size;
  }

  pkt.user_data = user_data;

  decoder->input_data.emplace_back(std::move(pkt));

  return heif_error_success;
}

static heif_error ffmpeg_push_data(void *decoder_raw, const void *data, size_t size)
{
  return ffmpeg_push_data2(decoder_raw, data, size, 0);
}


static heif_chroma ffmpeg_get_chroma_format(AVPixelFormat pix_fmt) {
  switch (pix_fmt) {
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_GRAY10LE:
      return heif_chroma_monochrome;

    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV420P14LE:
    case AV_PIX_FMT_YUV420P16LE:
      return heif_chroma_420;

    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV422P14LE:
    case AV_PIX_FMT_YUV422P16LE:
      return heif_chroma_422;

    case AV_PIX_FMT_YUV444P:
    case AV_PIX_FMT_YUV444P10LE:
    case AV_PIX_FMT_YUV444P12LE:
    case AV_PIX_FMT_YUV444P14LE:
    case AV_PIX_FMT_YUV444P16LE:
      return heif_chroma_444;

    default:
      // Unsupported pix_fmt
      return heif_chroma_undefined;
  }
}

static int ffmpeg_get_chroma_width(const AVFrame* frame, heif_channel channel, heif_chroma chroma)
{
    if (channel == heif_channel_Y)
    {
        return frame->width;
    }
    else if (chroma == heif_chroma_420 || chroma == heif_chroma_422)
    {
        return (frame->width + 1) / 2;
    }
    else
    {
        return frame->width;
    }
}

static int ffmpeg_get_chroma_height(const AVFrame* frame, heif_channel channel, heif_chroma chroma)
{
    if (channel == heif_channel_Y)
    {
        return frame->height;
    }
    else if (chroma == heif_chroma_420)
    {
        return (frame->height + 1) / 2;
    }
    else
    {
        return frame->height;
    }
}

static int get_ffmpeg_format_bpp(AVPixelFormat pix_fmt)
{
  switch (pix_fmt) {
    case AV_PIX_FMT_GRAY8:
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
    case AV_PIX_FMT_YUV422P:
    case AV_PIX_FMT_YUV444P:
      return 8;
    case AV_PIX_FMT_GRAY10LE:
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV444P10LE:
      return 10;
    case AV_PIX_FMT_GRAY12LE:
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV444P12LE:
      return 12;
    case AV_PIX_FMT_GRAY14LE:
    case AV_PIX_FMT_YUV420P14LE:
    case AV_PIX_FMT_YUV422P14LE:
    case AV_PIX_FMT_YUV444P14LE:
      return 14;
    case AV_PIX_FMT_GRAY16LE:
    case AV_PIX_FMT_YUV420P16LE:
    case AV_PIX_FMT_YUV422P16LE:
    case AV_PIX_FMT_YUV444P16LE:
      return 16;
    default:
      return 0;
  }
}

static heif_error ffmpeg_av_decode(ffmpeg_decoder* decoder, AVCodecContext* av_dec_ctx, AVFrame* av_frame, heif_image** image,
                                   uintptr_t* out_user_data,
                                   const heif_security_limits* limits)
{
  int ret;

  ret = avcodec_receive_frame(av_dec_ctx, av_frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR(AVERROR_EOF)) {
    *image = nullptr;
    return heif_error_ok;
  }

  if (ret < 0) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "Error in avcodec_receive_frame"};
  }

  if (out_user_data) {
    *out_user_data = av_frame->pts;
  }

  heif_chroma chroma = ffmpeg_get_chroma_format(av_dec_ctx->pix_fmt);
  if (chroma != heif_chroma_undefined) {
    bool is_mono = (chroma == heif_chroma_monochrome);

    heif_error err;
    err = heif_image_create(av_frame->width,
                            av_frame->height,
                            is_mono ? heif_colorspace_monochrome : heif_colorspace_YCbCr,
                            chroma,
                            image);
    if (err.code) {
      return err;
    }

    heif_channel channel2plane[3] = {
      heif_channel_Y,
      heif_channel_Cb,
      heif_channel_Cr
    };

    int nPlanes = is_mono ? 1 : 3;

    for (int channel = 0; channel < nPlanes; channel++) {
      int bpp = get_ffmpeg_format_bpp(av_dec_ctx->pix_fmt);
      if (bpp == 0) {
        heif_image_release(*image);
        err = {
          heif_error_Decoder_plugin_error,
          heif_suberror_Unsupported_color_conversion,
          "Pixel format not implemented"
        };
        return err;
      }

      int stride = av_frame->linesize[channel];
      const uint8_t* data = av_frame->data[channel];

      int w = ffmpeg_get_chroma_width(av_frame, channel2plane[channel], chroma);
      int h = ffmpeg_get_chroma_height(av_frame, channel2plane[channel], chroma);
      if (w <= 0 || h <= 0) {
        heif_image_release(*image);
        err = {
          heif_error_Decoder_plugin_error,
          heif_suberror_Invalid_image_size,
          "invalid image size"
        };
        return err;
      }

      err = heif_image_add_plane_safe(*image, channel2plane[channel], w, h, bpp, limits);
      if (err.code) {
        // copy error message to decoder object because heif_image will be released
        decoder->error_message = err.message;
        err.message = decoder->error_message.c_str();

        heif_image_release(*image);
        return err;
      }

      size_t dst_stride;
      uint8_t* dst_mem = heif_image_get_plane2(*image, channel2plane[channel], &dst_stride);

      int bytes_per_pixel = (bpp + 7) / 8;

      for (int y = 0; y < h; y++) {
        memcpy(dst_mem + y * dst_stride, data + y * stride, w * bytes_per_pixel);
      }
    }

    return heif_error_success;
  }
  else {
    return {
      heif_error_Unsupported_feature,
      heif_suberror_Unsupported_color_conversion,
      "Pixel format not implemented"
    };
  }
}

static heif_error ffmpeg_decode_next_image2(void* decoder_raw,
                                            heif_image** out_img,
                                            uintptr_t* out_user_data,
                                            const heif_security_limits* limits)
{
  ffmpeg_decoder* decoder = (ffmpeg_decoder*) decoder_raw;

  AVPacket* av_pkt = NULL;
  AVFrame* av_frame = NULL;
  AVCodecParameters* av_codecParam = NULL;
  heif_color_profile_nclx* nclx = NULL;
  int ret = 0;

  heif_error err = heif_error_success;

  if (!decoder->input_data.empty()) {
    uint8_t* parse_av_data = NULL;
    int parse_av_data_size = 0;

    ffmpeg_decoder::Packet& first_pkt = decoder->input_data.front();

    if (first_pkt.data.empty()) {
      // send 'flush' packet
      int ret = avcodec_send_packet(decoder->av_codec_context, nullptr);
      if (ret < 0) {
        return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "Error in avcodec_send_packet"};
      }

      decoder->input_data.pop_front();
    }
    else {
      av_pkt = av_packet_alloc();
      auto pkt_deleter = std::unique_ptr<AVPacket, void (*)(AVPacket*)>(av_pkt, [](AVPacket* pkt){av_packet_free(&pkt);});

      if (!av_pkt) {
        return { heif_error_Memory_allocation_error, heif_suberror_Unspecified, "av_packet_alloc returned error" };
      }

      parse_av_data = first_pkt.data.data();
      parse_av_data_size = (int) first_pkt.data.size();
      size_t n_bytes_consumed = 0;

      while (parse_av_data_size > 0) {
        decoder->av_codec_parser_context->flags = PARSER_FLAG_COMPLETE_FRAMES;
        ret = av_parser_parse2(decoder->av_codec_parser_context, decoder->av_codec_context, &av_pkt->data, &av_pkt->size,
                               parse_av_data, parse_av_data_size,
                               AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
          return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "av_parser_parse2 returned error"};
        }

        // std::cout << "decode packet of size: " << ret << "\n";

        parse_av_data += ret;
        parse_av_data_size -= ret;
        n_bytes_consumed += ret;

        if (av_pkt->size) {
          av_pkt->pts = first_pkt.user_data;

          ret = avcodec_send_packet(decoder->av_codec_context, av_pkt);
          if (ret < 0) {
            char buf[100];
            av_make_error_string(buf, 100, ret);
            return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "Error in avcodec_send_packet"};
          }
        }
        else {
          break;
        }
      }

      if (n_bytes_consumed == first_pkt.data.size()) {
        decoder->input_data.pop_front();
      }
      else {
        if (n_bytes_consumed > 0) {
          memmove(first_pkt.data.data(), first_pkt.data.data() + n_bytes_consumed,
                  first_pkt.data.size() - n_bytes_consumed);
          decoder->input_data.resize(decoder->input_data.size() - n_bytes_consumed);
        }
      }
    }
  }

  //uint8_t nal_type = (decoder->input_data.front().data[3] >> 1);

  av_frame = av_frame_alloc();
  auto frame_deleter = std::unique_ptr<AVFrame, void (*)(AVFrame*)>(av_frame, [](AVFrame* frame){av_frame_free(&frame);});

  if (!av_frame) {
    return { heif_error_Memory_allocation_error, heif_suberror_Unspecified, "av_frame_alloc returned error" };
  }

  err = ffmpeg_av_decode(decoder, decoder->av_codec_context, av_frame, out_img, out_user_data, limits);
  if (err.code != heif_error_Ok)
    return err;

  if (*out_img == nullptr) {
    return heif_error_ok;
  }

  av_codecParam = avcodec_parameters_alloc();
  auto param_deleter = std::unique_ptr<AVCodecParameters, void (*)(AVCodecParameters*)>(av_codecParam, [](AVCodecParameters* params){avcodec_parameters_free(&params);});

  if (!av_codecParam) {
    return { heif_error_Memory_allocation_error, heif_suberror_Unspecified, "avcodec_parameters_alloc returned error" };
  }
  if (avcodec_parameters_from_context(av_codecParam, decoder->av_codec_context) < 0)
  {
    return { heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "avcodec_parameters_from_context returned error" };
  }

  uint8_t video_full_range_flag = 0;
  uint8_t color_primaries = 0;
  uint8_t transfer_characteristics = 0;
  uint8_t matrix_coefficients = 0;

  video_full_range_flag = (av_codecParam->color_range == AVCOL_RANGE_JPEG) ? 1 : 0;
  color_primaries = av_codecParam->color_primaries;
  transfer_characteristics = av_codecParam->color_trc;
  matrix_coefficients = av_codecParam->color_space;

  nclx = heif_nclx_color_profile_alloc();
  auto nclx_deleter = std::unique_ptr<heif_color_profile_nclx, void (*)(heif_color_profile_nclx*)>(nclx, [](heif_color_profile_nclx* nclx){heif_nclx_color_profile_free(nclx);});

  heif_nclx_color_profile_set_color_primaries(nclx, static_cast<uint16_t>(color_primaries));
  heif_nclx_color_profile_set_transfer_characteristics(nclx, static_cast<uint16_t>(transfer_characteristics));
  heif_nclx_color_profile_set_matrix_coefficients(nclx, static_cast<uint16_t>(matrix_coefficients));
  nclx->full_range_flag = (bool)video_full_range_flag;
  heif_image_set_nclx_color_profile(*out_img, nclx);

  return heif_error_ok;
}

static heif_error ffmpeg_decode_next_image(void* decoder_raw,
                                            heif_image** out_img,
                                            const heif_security_limits* limits)
{
  return ffmpeg_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}


static heif_error ffmpeg_decode_image(void* decoder_raw,
                                         heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return ffmpeg_decode_next_image(decoder_raw, out_img, limits);
}

heif_error ffmpeg_flush_data(void* decoder_raw)
{
  auto* decoder = (struct ffmpeg_decoder*) decoder_raw;

  decoder->input_data.push_back({});

  return heif_error_ok;
}


static const heif_decoder_plugin decoder_ffmpeg
    {
        5,
        ffmpeg_plugin_name,
        ffmpeg_init_plugin,
        ffmpeg_deinit_plugin,
        ffmpeg_does_support_format,
        ffmpeg_new_decoder,
        ffmpeg_free_decoder,
        ffmpeg_push_data,
        ffmpeg_decode_image,
        ffmpeg_set_strict_decoding,
        "ffmpeg",
        ffmpeg_decode_next_image,
        /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
        ffmpeg_does_support_format2,
        ffmpeg_new_decoder2,
        ffmpeg_push_data2,
        ffmpeg_flush_data,
        ffmpeg_decode_next_image2
    };

const heif_decoder_plugin* get_decoder_plugin_ffmpeg()
{
  return &decoder_ffmpeg;
}

#if PLUGIN_FFMPEG_DECODER
heif_plugin_info plugin_info{
  1,
  heif_plugin_type_decoder,
  &decoder_ffmpeg
};
#endif
