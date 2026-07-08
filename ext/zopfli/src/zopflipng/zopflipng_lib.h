// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: lode.vandevenne@gmail.com (Lode Vandevenne)
// Author: jyrki.alakuijala@gmail.com (Jyrki Alakuijala)

// Library to recompress and optimize PNG images. Uses Zopfli as the compression
// backend, chooses optimal PNG color model, and tries out several PNG filter
// strategies.

#ifndef ZOPFLIPNG_LIB_H_
#define ZOPFLIPNG_LIB_H_

#ifdef __cplusplus

#include <string>
#include <vector>

extern "C" {

#endif

#include <stdlib.h>

enum ZopfliPNGFilterStrategy {
  kStrategyZero = 0,
  kStrategyOne = 1,
  kStrategyTwo = 2,
  kStrategyThree = 3,
  kStrategyFour = 4,
  kStrategyMinSum,
  kStrategyEntropy,
  kStrategyPredefined,
  kStrategyBruteForce,
  kNumFilterStrategies /* Not a strategy but used for the size of this enum */
};

typedef struct CZopfliPNGOptions {
  int lossy_transparent;
  int lossy_8bit;

  enum ZopfliPNGFilterStrategy* filter_strategies;
  // How many strategies to try.
  int num_filter_strategies;

  int auto_filter_strategy;

  char** keepchunks;
  // How many entries in keepchunks.
  int num_keepchunks;

  int use_zopfli;

  int num_iterations;

  int num_iterations_large;

  int block_split_strategy;
} CZopfliPNGOptions;

// Sets the default options
// Does not allocate or set keepchunks or filter_strategies
void CZopfliPNGSetDefaults(CZopfliPNGOptions *png_options);

// Returns 0 on success, error code otherwise
// The caller must free resultpng after use
int CZopfliPNGOptimize(const unsigned char* origpng,
    const size_t origpng_size,
    const CZopfliPNGOptions* png_options,
    int verbose,
    unsigned char** resultpng,
    size_t* resultpng_size);

#ifdef __cplusplus
}  // extern "C"
#endif

// C++ API
#ifdef __cplusplus

struct ZopfliPNGOptions {
  ZopfliPNGOptions();

  bool verbose;

  // Allow altering hidden colors of fully transparent pixels
  bool lossy_transparent;
  // Convert 16-bit per channel images to 8-bit per channel
  bool lossy_8bit;

  // Filter strategies to try
  std::vector<ZopfliPNGFilterStrategy> filter_strategies;

  // Automatically choose filter strategy using less good compression
  bool auto_filter_strategy;

  // Keep original color type (RGB, RGBA, gray, gray+alpha or palette) and bit
  // depth of the PNG.
  // This results in a loss of compression opportunities, e.g. it will no
  // longer convert a 4-channel RGBA image to 2-channel gray+alpha if the image
  // only had translucent gray pixels.
  // May be useful if a device does not support decoding PNGs of a particular
  // color type.
  // Default value: false.
  bool keep_colortype;

  // PNG chunks to keep
  // chunks to literally copy over from the original PNG to the resulting one
  std::vector<std::string> keepchunks;

  // Use Zopfli deflate compression
  bool use_zopfli;

  // Zopfli number of iterations
  int num_iterations;

  // Zopfli number of iterations on large images
  int num_iterations_large;

  // Unused, left for backwards compatiblity.
  int block_split_strategy;
};

// Returns 0 on success, error code otherwise.
// If verbose is true, it will print some info while working.
int ZopfliPNGOptimize(const std::vector<unsigned char>& origpng,
    const ZopfliPNGOptions& png_options,
    bool verbose,
    std::vector<unsigned char>* resultpng);

#endif  // __cplusplus

#endif  // ZOPFLIPNG_LIB_H_
