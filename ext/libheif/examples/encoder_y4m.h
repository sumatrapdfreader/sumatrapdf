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
#ifndef EXAMPLE_ENCODER_Y4M_H
#define EXAMPLE_ENCODER_Y4M_H

#include <string>

#include "encoder.h"

class Y4MEncoder : public Encoder
{
public:
  Y4MEncoder();

  heif_colorspace colorspace(bool has_alpha) const override
  {
    return heif_colorspace_YCbCr;
  }

  heif_chroma chroma(bool has_alpha, int bit_depth) const override
  {
    return heif_chroma_420;
  }

  void UpdateDecodingOptions(const struct heif_image_handle* handle,
                             struct heif_decoding_options* options) const override;

  bool Encode(const struct heif_image_handle* handle,
              const struct heif_image* image, const std::string& filename) override;

private:
};

#endif  // EXAMPLE_ENCODER_Y4M_H
