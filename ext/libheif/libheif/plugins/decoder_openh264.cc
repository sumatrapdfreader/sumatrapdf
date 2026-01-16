/*
 * openh264 codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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
#include "decoder_openh264.h"
#include <cstring>
#include <cassert>
#include <vector>
#include <cstdio>
#include <deque>

#include <wels/codec_api.h>
#include <string>
#include <utility>

// TODO: the openh264 decoder seems to fail with some images.
//       I have not figured out yet which images are affected.
//       Maybe it has something to do with the image size. I got the error with large images.

struct openh264_decoder
{
  struct Packet
  {
    std::vector<uint8_t> data;
    uintptr_t pts;
  };

  std::deque<Packet> input_data;
  bool m_eof_reached = false;

  std::string error_message;


  // --- decoder

  ISVCDecoder* decoder = nullptr;

  ~openh264_decoder()
  {
    if (decoder) {
      // Step 6:uninitialize the decoder and memory free

      decoder->Uninitialize(); // TODO: do we have to Uninitialize when an error is returned?


      WelsDestroyDecoder(decoder);
    }
  }
};

static const char kSuccess[] = "Success";

// Reduced priority because OpenH264 cannot pass through user-data.
// We need this feature for decoding sequences with SAI.
// Prefer to use the FFMPEG plugin.
static const int OpenH264_PLUGIN_PRIORITY = 70;

#define MAX_PLUGIN_NAME_LENGTH 80

static char plugin_name[MAX_PLUGIN_NAME_LENGTH];

static heif_error kError_EOF = {heif_error_Decoder_plugin_error, heif_suberror_End_of_data, "Insufficient input data"};


static const char* openh264_plugin_name()
{
  OpenH264Version version = WelsGetCodecVersion();

  sprintf(plugin_name, "OpenH264 %d.%d.%d", version.uMajor, version.uMinor, version.uRevision);

  return plugin_name;
}


static void openh264_init_plugin()
{
}


static void openh264_deinit_plugin()
{
}


static int openh264_does_support_format(heif_compression_format format)
{
  if (format == heif_compression_AVC) {
    return OpenH264_PLUGIN_PRIORITY;
  }
  else {
    return 0;
  }
}

static int openh264_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return openh264_does_support_format(format->format);
}


heif_error openh264_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  auto* decoder = new openh264_decoder();
  *dec = decoder;

  // Step 2:decoder creation
  WelsCreateDecoder(&decoder->decoder);
  if (!decoder->decoder) {
    return {
      heif_error_Decoder_plugin_error,
      heif_suberror_Unspecified,
      "Cannot create OpenH264 decoder"
    };
  }

  // Step 3:declare required parameter, used to differentiate Decoding only and Parsing only
  SDecodingParam sDecParam{};
  sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;

  //for Parsing only, the assignment is mandatory
  // sDecParam.bParseOnly = true;

  // Step 4:initialize the parameter and decoder context, allocate memory
  decoder->decoder->Initialize(&sDecParam);

  return {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
}


heif_error openh264_new_decoder(void** dec)
{
  heif_decoder_plugin_options options;
  options.format = heif_compression_AVC;
  options.num_threads = 0;
  options.strict_decoding = false;

  return openh264_new_decoder2(dec, &options);
}

void openh264_free_decoder(void* decoder_raw)
{
  auto* decoder = (openh264_decoder*) decoder_raw;

  if (!decoder) {
    return;
  }

  delete decoder;
}


void openh264_set_strict_decoding(void* decoder_raw, int flag)
{
  //  auto* decoder = (openh264_decoder*) decoder_raw;
}


heif_error openh264_push_data2(void* decoder_raw, const void* frame_data, size_t frame_size, uintptr_t user_data)
{
  auto* decoder = (openh264_decoder*) decoder_raw;

  const auto* input_data = (const uint8_t*) frame_data;

  assert(frame_size > 4);

  openh264_decoder::Packet pkt;
  pkt.data.insert(pkt.data.end(), input_data, input_data + frame_size);
  pkt.pts = user_data;
  decoder->input_data.push_back(std::move(pkt));

  return {heif_error_Ok, heif_suberror_Unspecified, kSuccess};
}

heif_error openh264_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  return openh264_push_data2(decoder_raw, frame_data, frame_size, 0);
}


heif_error openh264_decode_next_image2(void* decoder_raw, heif_image** out_img,
                                       uintptr_t* out_user_data,
                                       const heif_security_limits* limits)
{
  auto* decoder = (openh264_decoder*) decoder_raw;
  ISVCDecoder* pSvcDecoder = decoder->decoder;

  // --- Not enough data, but no EOF yet.

  if (decoder->input_data.empty() && !decoder->m_eof_reached) {
    *out_img = nullptr;
    return heif_error_ok;
  }


  //output: [0~2] for Y,U,V buffer for Decoding only
  unsigned char* pData[3] = {nullptr, nullptr, nullptr};

  // in-out: for Decoding only: declare and initialize the output buffer info, this should never co-exist with Parsing only

  SBufferInfo sDstBufInfo;
  memset(&sDstBufInfo, 0, sizeof(SBufferInfo));

  int iRet;

  if (!decoder->input_data.empty()) {
    sDstBufInfo.uiInBsTimeStamp = decoder->input_data.front().pts;

    const std::vector<uint8_t>& indata = decoder->input_data.front().data;
    std::vector<uint8_t> scdata;

    size_t idx = 0;
    while (idx < indata.size()) {
      if (indata.size() - 4 < idx) {
        return kError_EOF;
      }

      uint32_t size = ((indata[idx] << 24) | (indata[idx + 1] << 16) | (indata[idx + 2] << 8) | indata[idx + 3]);
      idx += 4;

      if (indata.size() < size || indata.size() - size < idx) {
        return kError_EOF;
      }

      scdata.push_back(0);
      scdata.push_back(0);
      scdata.push_back(1);

      // check for need of start code emulation prevention

      bool do_start_code_emulation_check = true;

      while (do_start_code_emulation_check && size >= 3) {
        bool found_start_code_emulation = false;

        for (size_t i = 0; i < size - 3; i++) {
          if (indata[idx + 0] == 0 &&
              indata[idx + 1] == 0 &&
              (indata[idx + 2] >= 0 && indata[idx + 2] <= 3)) {
            scdata.push_back(0);
            scdata.push_back(0);
            scdata.push_back(3);

            scdata.insert(scdata.end(), &indata[idx + 2], indata.data() + idx + i + 2);
            idx += i + 2;
            size -= (uint32_t) (i + 2);
            found_start_code_emulation = true;
            break;
          }
        }

        do_start_code_emulation_check = found_start_code_emulation;
      }

      assert(size > 0);
      // Note: we cannot write &indata[idx + size] since that would use the operator[] on an element beyond the vector range.
      scdata.insert(scdata.end(), &indata[idx], indata.data() + idx + size);

      idx += size;
    }

    if (idx != indata.size()) {
      decoder->input_data.pop_front();
      return kError_EOF;
    }

    decoder->input_data.pop_front();

    // input: encoded bitstream start position; should include start code prefix
    unsigned char* pBuf = scdata.data();

    // input: encoded bit stream length; should include the size of start code prefix
    int iSize = static_cast<int>(scdata.size());


    // Step 5:do actual decoding process in slice level; this can be done in a loop until data ends


    //for Decoding only
    iRet = pSvcDecoder->DecodeFrameNoDelay(pBuf, iSize, pData, &sDstBufInfo);
  }
  else {
    iRet = pSvcDecoder->FlushFrame(pData, &sDstBufInfo);
  }

  if (iRet != 0) {
    return {
      heif_error_Decoder_plugin_error,
      heif_suberror_Unspecified,
      "OpenH264 decoder error"
    };
  }

  if (sDstBufInfo.iBufferStatus != 1) {
    *out_img = nullptr;
    return heif_error_ok;
  }

  if (out_user_data) {
    *out_user_data = sDstBufInfo.uiOutYuvTimeStamp;
  }

  /*
  // TODO: I receive an iBufferStatus==0, but the output image is still decoded
  if (sDstBufInfo.iBufferStatus == 0) {
    return {heif_error_Decoder_plugin_error,
            heif_suberror_Unspecified,
            "OpenH264 decoder did not output any image"};
  }
  */

  uint32_t width = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
  uint32_t height = sDstBufInfo.UsrData.sSystemBuffer.iHeight;

  heif_image* heif_img;
  heif_error err{};

  uint32_t cwidth, cheight;

  if (sDstBufInfo.UsrData.sSystemBuffer.iFormat == videoFormatI420) {
    cwidth = (width + 1) / 2;
    cheight = (height + 1) / 2;

    err = heif_image_create(width, height,
                            heif_colorspace_YCbCr,
                            heif_chroma_420,
                            &heif_img);
    if (err.code != heif_error_Ok) {
      assert(heif_img == nullptr);
      return err;
    }

    *out_img = heif_img;

    err = heif_image_add_plane_safe(heif_img, heif_channel_Y, width, height, 8, limits);
    if (err.code != heif_error_Ok) {
      // copy error message to decoder object because heif_image will be released
      decoder->error_message = err.message;
      err.message = decoder->error_message.c_str();

      heif_image_release(heif_img);
      return err;
    }

    err = heif_image_add_plane_safe(heif_img, heif_channel_Cb, cwidth, cheight, 8, limits);
    if (err.code != heif_error_Ok) {
      // copy error message to decoder object because heif_image will be released
      decoder->error_message = err.message;
      err.message = decoder->error_message.c_str();

      heif_image_release(heif_img);
      return err;
    }

    err = heif_image_add_plane_safe(heif_img, heif_channel_Cr, cwidth, cheight, 8, limits);
    if (err.code != heif_error_Ok) {
      // copy error message to decoder object because heif_image will be released
      decoder->error_message = err.message;
      err.message = decoder->error_message.c_str();

      heif_image_release(heif_img);
      return err;
    }

    size_t y_stride;
    size_t cb_stride;
    size_t cr_stride;
    uint8_t* py = heif_image_get_plane2(heif_img, heif_channel_Y, &y_stride);
    uint8_t* pcb = heif_image_get_plane2(heif_img, heif_channel_Cb, &cb_stride);
    uint8_t* pcr = heif_image_get_plane2(heif_img, heif_channel_Cr, &cr_stride);

    int ystride = sDstBufInfo.UsrData.sSystemBuffer.iStride[0];
    int cstride = sDstBufInfo.UsrData.sSystemBuffer.iStride[1];

    for (uint32_t y = 0; y < height; y++) {
      memcpy(py + y * y_stride, sDstBufInfo.pDst[0] + y * ystride, width);
    }

    for (uint32_t y = 0; y < (height + 1) / 2; y++) {
      memcpy(pcb + y * cb_stride, sDstBufInfo.pDst[1] + y * cstride, (width + 1) / 2);
      memcpy(pcr + y * cr_stride, sDstBufInfo.pDst[2] + y * cstride, (width + 1) / 2);
    }
  }
  else {
    return {
      heif_error_Decoder_plugin_error,
      heif_suberror_Unspecified,
      "Unsupported image pixel format"
    };
  }

  // decoder->data.clear();

  return heif_error_ok;
}


heif_error openh264_decode_next_image(void* decoder_raw, heif_image** out_img,
                                      const heif_security_limits* limits)
{
  return openh264_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}

heif_error openh264_decode_image(void* decoder_raw, heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return openh264_decode_next_image(decoder_raw, out_img, limits);
}


heif_error openh264_flush_data(void* decoder_raw)
{
  auto* decoder = (struct openh264_decoder*) decoder_raw;

  decoder->m_eof_reached = true;

  return heif_error_ok;
}


static const heif_decoder_plugin decoder_openh264{
  5,
  openh264_plugin_name,
  openh264_init_plugin,
  openh264_deinit_plugin,
  openh264_does_support_format,
  openh264_new_decoder,
  openh264_free_decoder,
  openh264_push_data,
  openh264_decode_image,
  openh264_set_strict_decoding,
  "openh264",
  openh264_decode_next_image,
  /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
  openh264_does_support_format2,
  openh264_new_decoder2,
  openh264_push_data2,
  openh264_flush_data,
  openh264_decode_next_image2
};


const heif_decoder_plugin* get_decoder_plugin_openh264()
{
  return &decoder_openh264;
}


#if PLUGIN_OpenH264_DECODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_decoder,
  &decoder_openh264
};
#endif
