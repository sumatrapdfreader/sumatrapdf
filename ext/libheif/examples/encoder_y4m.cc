/*
  libheif example application "convert".

  MIT License

  Copyright (c) 2019 struktur AG, Dirk Farin <farin@struktur.de>

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
#include "encoder_y4m.h"

#include <errno.h>
#include <string.h>
#include <assert.h>


Y4MEncoder::Y4MEncoder() = default;


void Y4MEncoder::UpdateDecodingOptions(const struct heif_image_handle* handle,
                                       struct heif_decoding_options* options) const
{
}


bool Y4MEncoder::Encode(const struct heif_image_handle* handle,
                        const struct heif_image* image,
                        const std::string& filename)
{
  FILE* fp = fopen(filename.c_str(), "wb");
  if (!fp) {
    fprintf(stderr, "Can't open %s: %s\n", filename.c_str(), strerror(errno));
    return false;
  }

  int y_stride, cb_stride, cr_stride;
  const uint8_t* yp = heif_image_get_plane_readonly(image, heif_channel_Y, &y_stride);
  const uint8_t* cbp = heif_image_get_plane_readonly(image, heif_channel_Cb, &cb_stride);
  const uint8_t* crp = heif_image_get_plane_readonly(image, heif_channel_Cr, &cr_stride);

  assert(y_stride > 0);
  assert(cb_stride > 0);
  assert(cr_stride > 0);

  int yw = heif_image_get_width(image, heif_channel_Y);
  int yh = heif_image_get_height(image, heif_channel_Y);
  int cw = heif_image_get_width(image, heif_channel_Cb);
  int ch = heif_image_get_height(image, heif_channel_Cb);

  fprintf(fp, "YUV4MPEG2 W%d H%d F30:1\nFRAME\n", yw, yh);

  for (int y = 0; y < yh; y++) {
    fwrite(yp + y * y_stride, 1, yw, fp);
  }

  for (int y = 0; y < ch; y++) {
    fwrite(cbp + y * cb_stride, 1, cw, fp);
  }

  for (int y = 0; y < ch; y++) {
    fwrite(crp + y * cr_stride, 1, cw, fp);
  }

  fclose(fp);

  return true;
}
