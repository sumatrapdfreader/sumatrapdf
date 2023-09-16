// Copyright 2020 Google Inc.
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

#include "examples/anim_util.h"
#include "imageio/imageio_util.h"
#include "src/webp/demux.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // WebPAnimDecoderGetInfo() is too late to check the canvas size as
  // WebPAnimDecoderNew() will handle the allocations.
  WebPBitstreamFeatures features;
  if (WebPGetFeatures(data, size, &features) == VP8_STATUS_OK) {
    if (!ImgIoUtilCheckSizeArgumentsOverflow(features.width * 4,
                                             features.height)) {
      return 0;
    }
  }

  // decode everything as an animation
  WebPData webp_data = { data, size };
  WebPAnimDecoder* const dec = WebPAnimDecoderNew(&webp_data, NULL);
  if (dec == NULL) return 0;

  WebPAnimInfo info;
  if (!WebPAnimDecoderGetInfo(dec, &info)) goto End;
  if (!ImgIoUtilCheckSizeArgumentsOverflow(info.canvas_width * 4,
                                           info.canvas_height)) {
    goto End;
  }

  while (WebPAnimDecoderHasMoreFrames(dec)) {
    uint8_t* buf;
    int timestamp;
    if (!WebPAnimDecoderGetNext(dec, &buf, &timestamp)) break;
  }
 End:
  WebPAnimDecoderDelete(dec);
  return 0;
}
