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

#include "common_utils.h"
#include <cassert>


uint8_t chroma_h_subsampling(heif_chroma c)
{
  switch (c) {
    case heif_chroma_monochrome:
    case heif_chroma_444:
      return 1;

    case heif_chroma_420:
    case heif_chroma_422:
      return 2;

    case heif_chroma_interleaved_RGB:
    case heif_chroma_interleaved_RGBA:
    default:
      assert(false);
      return 0;
  }
}


uint8_t chroma_v_subsampling(heif_chroma c)
{
  switch (c) {
    case heif_chroma_monochrome:
    case heif_chroma_444:
    case heif_chroma_422:
      return 1;

    case heif_chroma_420:
      return 2;

    case heif_chroma_interleaved_RGB:
    case heif_chroma_interleaved_RGBA:
    default:
      assert(false);
      return 0;
  }
}


uint32_t get_subsampled_size_h(uint32_t width,
                               heif_channel channel,
                               heif_chroma chroma,
                               scaling_mode mode)
{
  if (channel == heif_channel_Cb ||
      channel == heif_channel_Cr) {
    uint8_t chromaSubH = chroma_h_subsampling(chroma);

    switch (mode) {
      case scaling_mode::round_up:
        // NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
        return (width + chromaSubH - 1) / chromaSubH;
      case scaling_mode::round_down:
        // NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
        return width / chromaSubH;
      case scaling_mode::is_divisible:
        assert(width % chromaSubH == 0);
        return width / chromaSubH;
      default:
        assert(false);
        return 0;
    }
  } else {
    return width;
  }
}


uint32_t get_subsampled_size_v(uint32_t height,
                               heif_channel channel,
                               heif_chroma chroma,
                               scaling_mode mode)
{
  if (channel == heif_channel_Cb ||
      channel == heif_channel_Cr) {
    uint8_t chromaSubV = chroma_v_subsampling(chroma);

    switch (mode) {
      case scaling_mode::round_up:
        // NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
        return (height + chromaSubV - 1) / chromaSubV;
      case scaling_mode::round_down:
        // NOLINTNEXTLINE(clang-analyzer-core.DivideZero)
        return height / chromaSubV;
      case scaling_mode::is_divisible:
        assert(height % chromaSubV == 0);
        return height / chromaSubV;
      default:
        assert(false);
        return 0;
    }
  } else {
    return height;
  }
}


void get_subsampled_size(uint32_t width, uint32_t height,
                         heif_channel channel,
                         heif_chroma chroma,
                         uint32_t* subsampled_width, uint32_t* subsampled_height)
{
  *subsampled_width = get_subsampled_size_h(width, channel, chroma, scaling_mode::round_up);
  *subsampled_height = get_subsampled_size_v(height, channel, chroma, scaling_mode::round_up);
}



uint8_t compute_avif_profile(int bits_per_pixel, heif_chroma chroma)
{
  if (bits_per_pixel <= 10 &&
      (chroma == heif_chroma_420 ||
       chroma == heif_chroma_monochrome)) {
    return 0;
  }
  else if (bits_per_pixel <= 10 &&
           chroma == heif_chroma_444) {
    return 1;
  }
  else {
    return 2;
  }
}


std::string fourcc_to_string(uint32_t code)
{
  std::string str("    ");
  str[0] = static_cast<char>((code >> 24) & 0xFF);
  str[1] = static_cast<char>((code >> 16) & 0xFF);
  str[2] = static_cast<char>((code >> 8) & 0xFF);
  str[3] = static_cast<char>((code >> 0) & 0xFF);

  return str;
}


Result<std::string> vector_to_string(const std::vector<uint8_t>& vec)
{
  if (vec.empty()) {
    return std::string{}; // return empty string
  }

  if (vec.back() != 0) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "utf8string not null-terminated"};
  }

  for (size_t i=0;i<vec.size()-1;i++) {
    if (vec[i] == 0) {
      return Error{heif_error_Invalid_input,
                   heif_suberror_Unspecified,
                   "utf8string with null character"};
    }
  }

  return std::string(vec.begin(), vec.end()-1);
}
