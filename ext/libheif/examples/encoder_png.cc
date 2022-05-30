/*
  libheif example application "convert".

  MIT License

  Copyright (c) 2017 struktur AG, Joachim Bauch <bauch@struktur.de>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <png.h>
#include <string.h>
#include <stdlib.h>

#include "encoder_png.h"

PngEncoder::PngEncoder() = default;

bool PngEncoder::Encode(const struct heif_image_handle* handle,
                        const struct heif_image* image, const std::string& filename)
{
  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                                nullptr, nullptr);
  if (!png_ptr) {
    fprintf(stderr, "libpng initialization failed (1)\n");
    return false;
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    png_destroy_write_struct(&png_ptr, nullptr);
    fprintf(stderr, "libpng initialization failed (2)\n");
    return false;
  }

  FILE* fp = fopen(filename.c_str(), "wb");
  if (!fp) {
    fprintf(stderr, "Can't open %s: %s\n", filename.c_str(), strerror(errno));
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return false;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
    fprintf(stderr, "Error while encoding image\n");
    return false;
  }

  png_init_io(png_ptr, fp);

  bool withAlpha = (heif_image_get_chroma_format(image) == heif_chroma_interleaved_RGBA ||
                    heif_image_get_chroma_format(image) == heif_chroma_interleaved_RRGGBBAA_BE);

  int width = heif_image_get_width(image, heif_channel_interleaved);
  int height = heif_image_get_height(image, heif_channel_interleaved);

  int bitDepth;
  int input_bpp = heif_image_get_bits_per_pixel_range(image, heif_channel_interleaved);
  if (input_bpp > 8) {
    bitDepth = 16;
  }
  else {
    bitDepth = 8;
  }

  const int colorType = withAlpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;

  png_set_IHDR(png_ptr, info_ptr, width, height, bitDepth, colorType,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  size_t profile_size = heif_image_handle_get_raw_color_profile_size(handle);
  if (profile_size > 0) {
    uint8_t* profile_data = static_cast<uint8_t*>(malloc(profile_size));
    heif_image_handle_get_raw_color_profile(handle, profile_data);
    char profile_name[] = "unknown";
    png_set_iCCP(png_ptr, info_ptr, profile_name, PNG_COMPRESSION_TYPE_BASE,
#if PNG_LIBPNG_VER < 10500
        (png_charp)profile_data,
#else
                 (png_const_bytep) profile_data,
#endif
                 (png_uint_32) profile_size);
    free(profile_data);
  }
  png_write_info(png_ptr, info_ptr);

  uint8_t** row_pointers = new uint8_t* [height];

  int stride_rgb;
  const uint8_t* row_rgb = heif_image_get_plane_readonly(image,
                                                         heif_channel_interleaved, &stride_rgb);

  for (int y = 0; y < height; ++y) {
    row_pointers[y] = const_cast<uint8_t*>(&row_rgb[y * stride_rgb]);
  }

  if (bitDepth == 16) {
    // shift image data to full 16bit range

    int shift = 16 - input_bpp;
    if (shift > 0) {
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < stride_rgb; x += 2) {
          uint8_t* p = (&row_pointers[y][x]);
          int v = (p[0] << 8) | p[1];
          v = (v << shift) | (v >> (16 - shift));
          p[0] = (uint8_t) (v >> 8);
          p[1] = (uint8_t) (v & 0xFF);
        }
      }
    }
  }


  png_write_image(png_ptr, row_pointers);
  png_write_end(png_ptr, nullptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  delete[] row_pointers;
  fclose(fp);
  return true;
}
