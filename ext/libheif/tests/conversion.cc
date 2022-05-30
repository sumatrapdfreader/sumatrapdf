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
#include "libheif/heif_colorconversion.h"


using namespace heif;

TEST_CASE( "Color conversion", "[heif_image]" ) {

  ColorConversionPipeline pipeline;
  bool success;

  printf("--- color conversions ---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_444, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGB, false, 8 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_YCbCr, heif_chroma_420, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGB, false, 8 } );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_YCbCr, heif_chroma_420, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGBA, true, 8 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_YCbCr, heif_chroma_420, false, 10 },
                                         { heif_colorspace_RGB, heif_chroma_444, false, 10 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_444, false, 10 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_BE, true, 10 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_444, false, 10 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_LE, true, 10 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_monochrome, heif_chroma_monochrome, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGB, false, 8 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_monochrome, heif_chroma_monochrome, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGBA, true, 8 } );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_interleaved_RGBA, true, 8 },
                                         { heif_colorspace_YCbCr, heif_chroma_420, false, 8 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_444, false, 10 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE, false, 10 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE, false, 12 },
                                         { heif_colorspace_RGB, heif_chroma_444, false, 12 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_444, false, 12 },
                                         { heif_colorspace_YCbCr, heif_chroma_420, false, 12 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_BE, false, 12 },
                                         { heif_colorspace_YCbCr, heif_chroma_420, false, 12 } );

  REQUIRE( success );

  printf("---\n");

  success = pipeline.construct_pipeline( { heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE, false, 12 },
                                         { heif_colorspace_YCbCr, heif_chroma_420, false, 12 } );

  REQUIRE( success );

  printf("--- monochrome colorspace -> interleaved RGB\n");

  success = pipeline.construct_pipeline( { heif_colorspace_monochrome, heif_chroma_monochrome, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGB, false, 8 } );

  REQUIRE( success );

  printf("--- monochrome YCbCr -> interleaved RGB\n");

  success = pipeline.construct_pipeline( { heif_colorspace_YCbCr, heif_chroma_monochrome, false, 8 },
                                         { heif_colorspace_RGB, heif_chroma_interleaved_RGB, false, 8 } );

  REQUIRE( success );
}
