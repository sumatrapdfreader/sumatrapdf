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

#ifndef LIBHEIF_COMMON_UTILS_H
#define LIBHEIF_COMMON_UTILS_H

#include <cinttypes>
#include <string>
#include <vector>

#include "libheif/heif.h"
#include "error.h"

#ifdef _MSC_VER
#define MAYBE_UNUSED
#else
#define MAYBE_UNUSED __attribute__((unused))
#endif


constexpr uint32_t four_bytes_to_uint32(uint8_t msb, uint8_t b, uint8_t c, uint8_t lsb)
{
  return (static_cast<uint32_t>(msb << 24) |
          static_cast<uint32_t>(b << 16) |
          static_cast<uint32_t>(c << 8) |
          static_cast<uint32_t>(lsb));
}

constexpr uint16_t two_bytes_to_uint16(uint8_t msb, uint8_t lsb)
{
  return (static_cast<uint16_t>(msb << 8) |
          static_cast<uint16_t>(lsb));
}

constexpr uint32_t fourcc(const char* id)
{
  return four_bytes_to_uint32(static_cast<uint8_t>(id[0]),
                              static_cast<uint8_t>(id[1]),
                              static_cast<uint8_t>(id[2]),
                              static_cast<uint8_t>(id[3]));
}

std::string fourcc_to_string(uint32_t code);


// Functions for common use in libheif and the plugins.

uint8_t chroma_h_subsampling(heif_chroma c);

uint8_t chroma_v_subsampling(heif_chroma c);

enum class scaling_mode : uint8_t {
  round_down,
  round_up,
  is_divisible
};

uint32_t get_subsampled_size_h(uint32_t width,
                               heif_channel channel,
                               heif_chroma chroma,
                               scaling_mode mode);

uint32_t get_subsampled_size_v(uint32_t height,
                               heif_channel channel,
                               heif_chroma chroma,
                               scaling_mode mode);

void get_subsampled_size(uint32_t width, uint32_t height,
                         heif_channel channel,
                         heif_chroma chroma,
                         uint32_t* subsampled_width, uint32_t* subsampled_height);

uint8_t compute_avif_profile(int bits_per_pixel, heif_chroma chroma);


inline uint8_t clip_int_u8(int x)
{
  if (x < 0) return 0;
  if (x > 255) return 255;
  return static_cast<uint8_t>(x);
}

inline uint16_t clip_int_u16(int32_t x, uint16_t maxi)
{
  if (x < 0) return 0;
  if (x > maxi) return maxi;
  return static_cast<uint16_t>(x);
}


inline uint16_t clip_f_u16(float fx, int32_t maxi)
{
  int32_t x = (int32_t) (fx + 0.5f);
  if (x < 0) return 0;
  if (x > maxi) return (uint16_t) maxi;
  return static_cast<uint16_t>(x);
}

inline uint8_t clip_f_u8(float fx)
{
  int32_t x = (int32_t) (fx + 0.5f);
  if (x < 0) return 0;
  if (x > 255) return 255;
  return static_cast<uint8_t>(x);
}

Result<std::string> vector_to_string(const std::vector<uint8_t>& vec);

#endif //LIBHEIF_COMMON_UTILS_H
