/*
 * HEIF codec.
 * Copyright (c) 2022 Dirk Farin <dirk.farin@gmail.com>
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


#if HAVE_ZLIB

#include <zlib.h>
#include <cstring>
#include <iostream>

std::vector<uint8_t> compress(const uint8_t* input, size_t size, int windowSize)
{
  std::vector<uint8_t> output;

  // initialize compressor

  const int outBufferSize = 8192;
  uint8_t dst[outBufferSize];

  z_stream strm;
  memset(&strm, 0, sizeof(z_stream));

  strm.avail_in = (uInt)size;
  strm.next_in = (Bytef*)input;

  strm.avail_out = outBufferSize;
  strm.next_out = (Bytef*) dst;

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  int err = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, windowSize, 8, Z_DEFAULT_STRATEGY);
  if (err != Z_OK) {
    return {}; // TODO: return error
  }

  do {
    strm.next_out = dst;
    strm.avail_out = outBufferSize;

    err = deflate(&strm, Z_FINISH);
    if (err == Z_BUF_ERROR || err == Z_OK) {
      // this is the usual case when we run out of buffer space
      // -> do nothing
    }
    else if (err == Z_STREAM_ERROR) {
      return {}; // TODO: return error
    }


    // append decoded data to output

    output.insert(output.end(), dst, dst + outBufferSize - strm.avail_out);
  } while (err != Z_STREAM_END);

  deflateEnd(&strm);

  return output;
}


Result<std::vector<uint8_t>> do_inflate(const std::vector<uint8_t>& compressed_input, int windowSize)
{
  if (compressed_input.empty()) {
    return Error(heif_error_Invalid_input, heif_suberror_Decompression_invalid_data,
                 "Empty zlib compressed data.");
  }

  std::vector<uint8_t> output;

  // decompress data with zlib

  std::vector<uint8_t> dst;
  dst.resize(8192);

  z_stream strm;
  memset(&strm, 0, sizeof(z_stream));

  strm.avail_in = (int)compressed_input.size();
  strm.next_in = (Bytef*) compressed_input.data();

  strm.avail_out = (uInt)dst.size();
  strm.next_out = (Bytef*) dst.data();

  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;

  int err = -1;

  err = inflateInit2(&strm, windowSize);
  if (err != Z_OK) {
    std::stringstream sstr;
    sstr << "Error initialising zlib inflate: " << (strm.msg ? strm.msg : "NULL") << " (" << err << ")\n";
    return Error(heif_error_Memory_allocation_error, heif_suberror_Compression_initialisation_error, sstr.str());
  }

  do {
    strm.avail_out = (uInt)dst.size();
    strm.next_out = (Bytef*) dst.data();

    err = inflate(&strm, Z_NO_FLUSH);

    if (err == Z_BUF_ERROR) {
      if (dst.size() >= 65536) { // TODO: make this a security limit
        inflateEnd(&strm);
        std::stringstream sstr;
        sstr << "Error performing zlib inflate: maximum output buffer size exceeded\n";
        return Error(heif_error_Memory_allocation_error, heif_suberror_Compression_initialisation_error, sstr.str());
      }

      dst.resize(dst.size() * 2);
      strm.next_out = dst.data();
      strm.avail_out = (uInt)dst.size();
      continue;
    }

    if (err == Z_NEED_DICT || err == Z_DATA_ERROR || err == Z_STREAM_ERROR) {
      inflateEnd(&strm);
      std::stringstream sstr;
      sstr << "Error performing zlib inflate: " << (strm.msg ? strm.msg : "NULL") << " (" << err << ")\n";
      return Error(heif_error_Invalid_input, heif_suberror_Decompression_invalid_data, sstr.str());
    }

    // append decoded data to output
    output.insert(output.end(), dst.begin(), dst.end() - strm.avail_out);
  } while (err != Z_STREAM_END);


  inflateEnd(&strm);

  return output;
}

std::vector<uint8_t> compress_zlib(const uint8_t* input, size_t size)
{
  return compress(input, size, 15);
}

std::vector<uint8_t> compress_deflate(const uint8_t* input, size_t size)
{
  return compress(input, size, -15);
}


Result<std::vector<uint8_t>> decompress_zlib(const std::vector<uint8_t>& compressed_input)
{
  return do_inflate(compressed_input, 15);
}

Result<std::vector<uint8_t>> decompress_deflate(const std::vector<uint8_t>& compressed_input)
{
  return do_inflate(compressed_input, -15);
}
#endif
