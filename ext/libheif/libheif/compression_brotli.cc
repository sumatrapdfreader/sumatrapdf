/*
 * HEIF codec.
 * Copyright (c) 2024 Brad Hards <bradh@frogmouth.net>
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

#include "compression.h"

#if HAVE_BROTLI

const size_t BUF_SIZE = (1 << 18);
#include <brotli/decode.h>
#include <brotli/encode.h>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#include "error.h"


Result<std::vector<uint8_t>> decompress_brotli(const std::vector<uint8_t> &compressed_input)
{
    BrotliDecoderResult result = BROTLI_DECODER_RESULT_ERROR;
    std::vector<uint8_t> buffer(BUF_SIZE, 0);
    size_t available_in = compressed_input.size();
    const std::uint8_t *next_in = reinterpret_cast<const std::uint8_t *>(compressed_input.data());
    size_t available_out = buffer.size();
    std::uint8_t *next_output = buffer.data();

    std::unique_ptr<BrotliDecoderState, void(*)(BrotliDecoderState*)> state(BrotliDecoderCreateInstance(0, 0, 0), BrotliDecoderDestroyInstance);

    std::vector<uint8_t> output;

    while (true)
    {
        result = BrotliDecoderDecompressStream(state.get(), &available_in, &next_in, &available_out, &next_output, 0);

        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT)
        {
            output.insert(output.end(), buffer.data(), buffer.data() + std::distance(buffer.data(), next_output));
            available_out = buffer.size();
            next_output = buffer.data();
        }
        else if (result == BROTLI_DECODER_RESULT_SUCCESS)
        {
            output.insert(output.end(), buffer.data(), buffer.data() + std::distance(buffer.data(), next_output));
            break;
        }
        else if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT)
        {
            std::stringstream sstr;
            sstr << "Error performing brotli inflate - insufficient data.\n";
            return Error(heif_error_Invalid_input, heif_suberror_Decompression_invalid_data, sstr.str());
        }
        else if (result == BROTLI_DECODER_RESULT_ERROR)
        {
            const char* errorMessage = BrotliDecoderErrorString(BrotliDecoderGetErrorCode(state.get()));
            std::stringstream sstr;
            sstr << "Error performing brotli inflate - " << errorMessage << "\n";
            return Error(heif_error_Invalid_input, heif_suberror_Decompression_invalid_data, sstr.str());
        }
        else
        {
            const char* errorMessage = BrotliDecoderErrorString(BrotliDecoderGetErrorCode(state.get()));
            std::stringstream sstr;
            sstr << "Unknown error performing brotli inflate - " << errorMessage << "\n";
            return Error(heif_error_Invalid_input, heif_suberror_Decompression_invalid_data, sstr.str());
        }
    }

    return output;
}


std::vector<uint8_t> compress_brotli(const uint8_t* input, size_t size)
{
  std::unique_ptr<BrotliEncoderState, void(*)(BrotliEncoderState*)> state(BrotliEncoderCreateInstance(nullptr, nullptr, nullptr), BrotliEncoderDestroyInstance);

  size_t available_in = size;
  const uint8_t* next_in = input;

  std::vector<uint8_t> tmp(BUF_SIZE);
  size_t available_out = BUF_SIZE;
  uint8_t* next_out = tmp.data();

  std::vector<uint8_t> result;

  for (;;) {
    BROTLI_BOOL success = BrotliEncoderCompressStream(state.get(),
                                                      BROTLI_OPERATION_FINISH,
                                                      &available_in,
                                                      &next_in,
                                                      &available_out,
                                                      &next_out,
                                                      nullptr);
    if (!success) {
      return {};
    }

    if (next_out != tmp.data()) {
      result.insert(result.end(), tmp.data(), tmp.data() + std::distance(tmp.data(), next_out));
      available_out = BUF_SIZE;
      next_out = tmp.data();
    }

    if (BrotliEncoderIsFinished(state.get())) {
      break;
    }
  }

  return result;
}

#endif
