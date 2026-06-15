// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/box_content_decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "lib/jxl/base/sanitizers.h"

namespace jxl {

JxlBoxContentDecoder::JxlBoxContentDecoder() = default;

JxlBoxContentDecoder::~JxlBoxContentDecoder() {
  if (brotli_dec) {
    BrotliDecoderDestroyInstance(brotli_dec);
  }
}

void JxlBoxContentDecoder::StartBox(bool brob_decode, bool box_until_eof,
                                    size_t contents_size) {
  if (brotli_dec) {
    BrotliDecoderDestroyInstance(brotli_dec);
    brotli_dec = nullptr;
  }
  header_done_ = false;
  brob_decode_ = brob_decode;
  box_until_eof_ = box_until_eof;
  remaining_ = box_until_eof ? 0 : contents_size;
  pos_ = 0;
}

JxlDecoderStatus JxlBoxContentDecoder::Process(const uint8_t* next_in,
                                               size_t avail_in, size_t box_pos,
                                               uint8_t** next_out,
                                               size_t* avail_out) {
  next_in += pos_ - box_pos;
  avail_in -= pos_ - box_pos;

  if (brob_decode_) {
    if (!header_done_) {
      if (avail_in < 4) return JXL_DEC_NEED_MORE_INPUT;
      if (!box_until_eof_) {
        if (remaining_ < 4) return JXL_DEC_ERROR;
        remaining_ -= 4;
      }
      next_in += 4;
      avail_in -= 4;
      pos_ += 4;
      header_done_ = true;
    }

    if (!brotli_dec) {
      brotli_dec = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    }

    const uint8_t* next_in_before = next_in;
    uint8_t* next_out_before = *next_out;
    msan::MemoryIsInitialized(next_in, avail_in);
    BrotliDecoderResult res = BrotliDecoderDecompressStream(
        brotli_dec, &avail_in, &next_in, avail_out, next_out, nullptr);
    size_t consumed = next_in - next_in_before;
    size_t produced = *next_out - next_out_before;
    if (res == BROTLI_DECODER_RESULT_ERROR) {
      return JXL_DEC_ERROR;
    }
    msan::UnpoisonMemory(next_out_before, produced);
    pos_ += consumed;
    if (!box_until_eof_) remaining_ -= consumed;
    if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
      return JXL_DEC_NEED_MORE_INPUT;
    }
    if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
      return JXL_DEC_BOX_NEED_MORE_OUTPUT;
    }
    if (res == BROTLI_DECODER_RESULT_SUCCESS) {
      return JXL_DEC_BOX_COMPLETE;
    }
    // unknown Brotli result
    return JXL_DEC_ERROR;
  } else {
    // remaining box bytes as seen from dec->file_pos
    size_t can_read = avail_in;
    if (!box_until_eof_) can_read = std::min<size_t>(can_read, remaining_);
    size_t to_write = std::min<size_t>(can_read, *avail_out);
    memcpy(*next_out, next_in, to_write);

    *next_out += to_write;
    *avail_out -= to_write;
    if (!box_until_eof_) remaining_ -= to_write;
    pos_ += to_write;

    if (to_write < can_read) return JXL_DEC_BOX_NEED_MORE_OUTPUT;

    if (!box_until_eof_ && remaining_ > 0) return JXL_DEC_NEED_MORE_INPUT;

    return JXL_DEC_BOX_COMPLETE;
  }
}

}  // namespace jxl
