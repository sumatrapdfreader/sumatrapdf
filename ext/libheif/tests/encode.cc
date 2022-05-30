/*
  libheif unit tests

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

#include "catch.hpp"
#include "libheif/heif.h"


struct heif_image* createImage_RRGGBB_BE() {
  struct heif_image* image;
  struct heif_error err;
  err = heif_image_create(256,256,
                          heif_colorspace_RGB,
                          heif_chroma_interleaved_RRGGBB_BE,
                          &image);
  if (err.code) {
    return nullptr;
  }

  err = heif_image_add_plane(image,
                             heif_channel_interleaved,
                             256,256, 10);
  if (err.code) {
    heif_image_release(image);
    return nullptr;
  }

  return image;
}


struct heif_error encode_image(struct heif_image* img) {
  struct heif_context* ctx = heif_context_alloc();

  struct heif_encoder* enc;
  struct heif_error err { heif_error_Ok };

  err = heif_context_get_encoder_for_format(ctx,
                                            heif_compression_HEVC,
                                            &enc);
  if (err.code) {
    heif_context_free(ctx);
    return err;
  }


  struct heif_image_handle* hdl;
  err = heif_context_encode_image(ctx,
                                  img,
                                  enc,
                                  nullptr,
                                  &hdl);
  if (err.code) {
    heif_encoder_release(enc);
    heif_context_free(ctx);
    return err;
  }

  return err;
}


#if 0
TEST_CASE( "Create images", "[heif_image]" ) {
  auto img = createImage_RRGGBB_BE();
  REQUIRE( img != nullptr );

  heif_image_release(img);
}



TEST_CASE( "Encode HDR", "[heif_encoder]" ) {
  auto img = createImage_RRGGBB_BE();
  REQUIRE( img != nullptr );

  REQUIRE( encode_image(img).code == heif_error_Ok );

  heif_image_release(img);
}
#endif
