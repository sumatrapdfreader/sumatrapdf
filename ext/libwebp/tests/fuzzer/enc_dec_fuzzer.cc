// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>

#include "./fuzz_utils.h"
#include "src/webp/decode.h"
#include "src/webp/encode.h"

namespace {

const VP8CPUInfo default_VP8GetCPUInfo = VP8GetCPUInfo;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* const data, size_t size) {
  uint32_t bit_pos = 0;

  ExtractAndDisableOptimizations(default_VP8GetCPUInfo, data, size, &bit_pos);

  // Init the source picture.
  WebPPicture pic;
  if (!WebPPictureInit(&pic)) {
    fprintf(stderr, "WebPPictureInit failed.\n");
    abort();
  }
  pic.use_argb = Extract(1, data, size, &bit_pos);

  // Read the source picture.
  if (!ExtractSourcePicture(&pic, data, size, &bit_pos)) {
    const WebPEncodingError error_code = pic.error_code;
    WebPPictureFree(&pic);
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
    fprintf(stderr, "Can't read input image. Error code: %d\n", error_code);
    abort();
  }

  // Crop and scale.
  if (!ExtractAndCropOrScale(&pic, data, size, &bit_pos)) {
    const WebPEncodingError error_code = pic.error_code;
    WebPPictureFree(&pic);
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY) return 0;
    fprintf(stderr, "ExtractAndCropOrScale failed. Error code: %d\n",
            error_code);
    abort();
  }

  // Extract a configuration from the packed bits.
  WebPConfig config;
  if (!ExtractWebPConfig(&config, data, size, &bit_pos)) {
    fprintf(stderr, "ExtractWebPConfig failed.\n");
    abort();
  }
  // Skip slow settings on big images, it's likely to timeout.
  if (pic.width * pic.height > 32 * 32) {
    if (config.lossless) {
      if (config.quality > 99.0f && config.method >= 5) {
        config.quality = 99.0f;
        config.method = 5;
      }
    } else {
      if (config.quality > 99.0f && config.method == 6) {
        config.quality = 99.0f;
      }
    }
    if (config.alpha_quality == 100 && config.method == 6) {
      config.alpha_quality = 99;
    }
  }

  // Encode.
  WebPMemoryWriter memory_writer;
  WebPMemoryWriterInit(&memory_writer);
  pic.writer = WebPMemoryWrite;
  pic.custom_ptr = &memory_writer;
  if (!WebPEncode(&config, &pic)) {
    const WebPEncodingError error_code = pic.error_code;
    WebPMemoryWriterClear(&memory_writer);
    WebPPictureFree(&pic);
    if (error_code == VP8_ENC_ERROR_OUT_OF_MEMORY ||
        error_code == VP8_ENC_ERROR_BAD_WRITE) {
      return 0;
    }
    fprintf(stderr, "WebPEncode failed. Error code: %d\n", error_code);
    abort();
  }

  // Try decoding the result.
  const uint8_t* const out_data = memory_writer.mem;
  const size_t out_size = memory_writer.size;
  WebPDecoderConfig dec_config;
  if (!WebPInitDecoderConfig(&dec_config)) {
    fprintf(stderr, "WebPInitDecoderConfig failed.\n");
    WebPMemoryWriterClear(&memory_writer);
    WebPPictureFree(&pic);
    abort();
  }

  dec_config.output.colorspace = MODE_BGRA;
  const VP8StatusCode status = WebPDecode(out_data, out_size, &dec_config);
  if ((status != VP8_STATUS_OK && status != VP8_STATUS_OUT_OF_MEMORY &&
       status != VP8_STATUS_USER_ABORT) ||
      (status == VP8_STATUS_OK && (dec_config.output.width != pic.width ||
                                   dec_config.output.height != pic.height))) {
    fprintf(stderr, "WebPDecode failed. status: %d.\n", status);
    WebPFreeDecBuffer(&dec_config.output);
    WebPMemoryWriterClear(&memory_writer);
    WebPPictureFree(&pic);
    abort();
  }

  if (status == VP8_STATUS_OK) {
    const uint8_t* const rgba = dec_config.output.u.RGBA.rgba;
    const int w = dec_config.output.width;
    const int h = dec_config.output.height;

    // Compare the results if exact encoding.
    if (pic.use_argb && config.lossless && config.near_lossless == 100) {
      const uint32_t* src1 = (const uint32_t*)rgba;
      const uint32_t* src2 = pic.argb;
      for (int y = 0; y < h; ++y, src1 += w, src2 += pic.argb_stride) {
        for (int x = 0; x < w; ++x) {
          uint32_t v1 = src1[x], v2 = src2[x];
          if (!config.exact) {
            if ((v1 & 0xff000000u) == 0 || (v2 & 0xff000000u) == 0) {
              // Only keep alpha for comparison of fully transparent area.
              v1 &= 0xff000000u;
              v2 &= 0xff000000u;
            }
          }
          if (v1 != v2) {
            fprintf(stderr, "Lossless compression failed pixel-exactness.\n");
            WebPFreeDecBuffer(&dec_config.output);
            WebPMemoryWriterClear(&memory_writer);
            WebPPictureFree(&pic);
            abort();
          }
        }
      }
    }
  }

  WebPFreeDecBuffer(&dec_config.output);
  WebPMemoryWriterClear(&memory_writer);
  WebPPictureFree(&pic);
  return 0;
}
