/*
 * HEIF codec.
 * Copyright (c) 2019 struktur AG, Joachim Bauch <bauch@struktur.de>
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

#include <assert.h>

#include <sstream>

#include "bitstream.h"
#include "heif_colorconversion.h"
#include "heif_image.h"

static bool is_valid_chroma(uint8_t chroma)
{
  switch (chroma) {
    case heif_chroma_monochrome:
    case heif_chroma_420:
    case heif_chroma_422:
    case heif_chroma_444:
    case heif_chroma_interleaved_RGB:
    case heif_chroma_interleaved_RGBA:
    case heif_chroma_interleaved_RRGGBB_BE:
    case heif_chroma_interleaved_RRGGBBAA_BE:
    case heif_chroma_interleaved_RRGGBB_LE:
    case heif_chroma_interleaved_RRGGBBAA_LE:
      return true;
    default:
      return false;
  }
}

static bool is_valid_colorspace(uint8_t colorspace)
{
  switch (colorspace) {
    case heif_colorspace_YCbCr:
    case heif_colorspace_RGB:
    case heif_colorspace_monochrome:
      return true;
    default:
      return false;
  }
}

static bool read_plane(heif::BitstreamRange* range,
                       std::shared_ptr<heif::HeifPixelImage> image, heif_channel channel,
                       int width, int height, int bit_depth)
{
  if (width <= 0 || height <= 0) {
    return false;
  }
  if (!range->prepare_read(static_cast<uint64_t>(width) * height)) {
    return false;
  }
  if (!image->add_plane(channel, width, height, bit_depth)) {
    return false;
  }
  int stride;
  uint8_t* plane = image->get_plane(channel, &stride);
  assert(stride >= width);
  auto stream = range->get_istream();
  for (int y = 0; y < height; y++, plane += stride) {
    assert(stream->read(plane, width));
  }
  return true;
}

static bool read_plane_interleaved(heif::BitstreamRange* range,
                                   std::shared_ptr<heif::HeifPixelImage> image, heif_channel channel,
                                   int width, int height, int bit_depth, int comps)
{
  if (width <= 0 || height <= 0) {
    return false;
  }
  if (!range->prepare_read(static_cast<uint64_t>(width) * height * comps)) {
    return false;
  }
  if (!image->add_plane(channel, width, height, bit_depth)) {
    return false;
  }
  int stride;
  uint8_t* plane = image->get_plane(channel, &stride);
  assert(stride >= width * comps);
  auto stream = range->get_istream();
  for (int y = 0; y < height; y++, plane += stride) {
    assert(stream->read(plane, width * comps));
  }
  return true;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  auto reader = std::make_shared<heif::StreamReader_memory>(data, size, false);
  heif::BitstreamRange range(reader, size);

  int width;
  int height;
  int bit_depth;
  bool alpha;
  uint8_t in_chroma;
  uint8_t in_colorspace;
  uint8_t out_chroma;
  uint8_t out_colorspace;
  if (!range.prepare_read(10)) {
    return 0;
  }

  width = range.read16();
  height = range.read16();
  bit_depth = range.read8();
  alpha = range.read8() == 1;
  in_chroma = range.read8();
  in_colorspace = range.read8();
  out_chroma = range.read8();
  out_colorspace = range.read8();

  // Width / height must be a multiple of 2.
  if (width == 0 || height == 0 || (width & 1) != 0 || (height & 1) != 0) {
    return 0;
  }

  switch (bit_depth) {
    case 8:
      break;
    default:
      // TODO: Add support for more color depths.
      return 0;
  }

  if (!is_valid_chroma(in_chroma) || !is_valid_colorspace(in_colorspace) ||
      !is_valid_chroma(out_chroma) || !is_valid_colorspace(out_colorspace)) {
    return 0;
  }

  auto in_image = std::make_shared<heif::HeifPixelImage>();
  in_image->create(width, height, static_cast<heif_colorspace>(in_colorspace),
                   static_cast<heif_chroma>(in_chroma));

  switch (in_colorspace) {
    case heif_colorspace_YCbCr:
      switch (in_chroma) {
        case heif_chroma_420:
          if (!read_plane(&range, in_image, heif_channel_Y,
                          width, height, bit_depth)) {
            return 0;
          }
          if (!read_plane(&range, in_image, heif_channel_Cb,
                          width / 2, height / 2, bit_depth)) {
            return 0;
          }
          if (!read_plane(&range, in_image, heif_channel_Cr,
                          width / 2, height / 2, bit_depth)) {
            return 0;
          }
          break;
        case heif_chroma_422:
          if (!read_plane(&range, in_image, heif_channel_Y,
                          width, height, bit_depth)) {
            return 0;
          }
          if (!read_plane(&range, in_image, heif_channel_Cb,
                          width / 2, height, bit_depth)) {
            return 0;
          }
          if (!read_plane(&range, in_image, heif_channel_Cr,
                          width / 2, height, bit_depth)) {
            return 0;
          }
          break;
        case heif_chroma_444:
          if (!read_plane(&range, in_image, heif_channel_Y,
                          width, height, bit_depth)) {
            return 0;
          }
          if (!read_plane(&range, in_image, heif_channel_Cb,
                          width, height, bit_depth)) {
            return 0;
          }
          if (!read_plane(&range, in_image, heif_channel_Cr,
                          width, height, bit_depth)) {
            return 0;
          }
          break;
        default:
          return 0;
      }
      break;
    case heif_colorspace_RGB:
      switch (in_chroma) {
        case heif_chroma_interleaved_RGB:
          if (!read_plane_interleaved(&range, in_image,
                                      heif_channel_interleaved, width, height, bit_depth, 3)) {
            return 0;
          }
          break;
        case heif_chroma_interleaved_RGBA:
          if (!read_plane_interleaved(&range, in_image,
                                      heif_channel_interleaved, width, height, bit_depth, 4)) {
            return 0;
          }
          alpha = false;  // Already part of interleaved data.
          break;
        default:
          // TODO: Support other RGB chromas.
          return 0;
      }
      break;
    case heif_colorspace_monochrome:
      if (in_chroma != heif_chroma_monochrome) {
        return 0;
      }
      if (!read_plane(&range, in_image, heif_channel_Y,
                      width, height, bit_depth)) {
        return 0;
      }
      break;
    default:
      assert(false);
  }

  if (alpha) {
    if (!read_plane(&range, in_image, heif_channel_Alpha,
                    width, height, bit_depth)) {
      return 0;
    }
  }

  auto out_image = convert_colorspace(in_image,
                                      static_cast<heif_colorspace>(out_colorspace),
                                      static_cast<heif_chroma>(out_chroma),
                                      nullptr);
  if (!out_image) {
    // Conversion is not supported.
    return 0;
  }

  assert(out_image->get_width() == width);
  assert(out_image->get_height() == height);
  assert(out_image->get_chroma_format() ==
         static_cast<heif_chroma>(out_chroma));
  assert(out_image->get_colorspace() ==
         static_cast<heif_colorspace>(out_colorspace));
  return 0;
}
