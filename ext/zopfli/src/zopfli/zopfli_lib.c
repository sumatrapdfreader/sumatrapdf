/*
Copyright 2011 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author: lode.vandevenne@gmail.com (Lode Vandevenne)
Author: jyrki.alakuijala@gmail.com (Jyrki Alakuijala)
*/

#include "zopfli.h"

#include "deflate.h"
#include "gzip_container.h"
#include "zlib_container.h"

#include <assert.h>

void ZopfliCompress(const ZopfliOptions* options, ZopfliFormat output_type,
                    const unsigned char* in, size_t insize,
                    unsigned char** out, size_t* outsize) {
  if (output_type == ZOPFLI_FORMAT_GZIP) {
    ZopfliGzipCompress(options, in, insize, out, outsize);
  } else if (output_type == ZOPFLI_FORMAT_ZLIB) {
    ZopfliZlibCompress(options, in, insize, out, outsize);
  } else if (output_type == ZOPFLI_FORMAT_DEFLATE) {
    unsigned char bp = 0;
    ZopfliDeflate(options, 2 /* Dynamic block */, 1,
                  in, insize, &bp, out, outsize);
  } else {
    assert(0);
  }
}
