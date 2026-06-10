/*
 * HEIF codec.
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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
#include "common_utils.h"


uint32_t unci_compression_to_fourcc(heif_unci_compression method)
{
  switch (method) {
    case heif_unci_compression_off:
      return 0;
    case heif_unci_compression_deflate:
      return fourcc("defl");
    case heif_unci_compression_zlib:
      return fourcc("zlib");
    case heif_unci_compression_brotli:
      return fourcc("brot");
    default:
      return 0;
  }
}


Result<std::vector<uint8_t>> compress_unci_fourcc(uint32_t fourcc_code,
                                                   const uint8_t* data, size_t size)
{
  switch (fourcc_code) {
#if HAVE_ZLIB
    case fourcc("defl"):
      return {compress_deflate(data, size)};
    case fourcc("zlib"):
      return {compress_zlib(data, size)};
#endif
#if HAVE_BROTLI
    case fourcc("brot"):
      return {compress_brotli(data, size)};
#endif
    default:
      return Error{heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_generic_compression_method,
                   "Unsupported unci compression method."};
  }
}
