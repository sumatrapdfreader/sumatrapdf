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

// See zopflipng_lib.h

#include "zopflipng_lib.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <set>
#include <vector>

#include "lodepng/lodepng.h"
#include "lodepng/lodepng_util.h"
#include "../zopfli/deflate.h"

ZopfliPNGOptions::ZopfliPNGOptions()
  : verbose(false)
  , lossy_transparent(false)
  , lossy_8bit(false)
  , auto_filter_strategy(true)
  , keep_colortype(false)
  , use_zopfli(true)
  , num_iterations(15)
  , num_iterations_large(5)
  , block_split_strategy(1) {
}

// Deflate compressor passed as fuction pointer to LodePNG to have it use Zopfli
// as its compression backend.
unsigned CustomPNGDeflate(unsigned char** out, size_t* outsize,
                          const unsigned char* in, size_t insize,
                          const LodePNGCompressSettings* settings) {
  const ZopfliPNGOptions* png_options =
      static_cast<const ZopfliPNGOptions*>(settings->custom_context);
  unsigned char bp = 0;
  ZopfliOptions options;
  ZopfliInitOptions(&options);

  options.verbose = png_options->verbose;
  options.numiterations = insize < 200000
      ? png_options->num_iterations : png_options->num_iterations_large;

  ZopfliDeflate(&options, 2 /* Dynamic */, 1, in, insize, &bp, out, outsize);

  return 0;  // OK
}

// Returns 32-bit integer value for RGBA color.
static unsigned ColorIndex(const unsigned char* color) {
  return color[0] + 256u * color[1] + 65536u * color[2] + 16777216u * color[3];
}

// Counts amount of colors in the image, up to 257. If transparent_counts_as_one
// is enabled, any color with alpha channel 0 is treated as a single color with
// index 0.
void CountColors(std::set<unsigned>* unique,
                 const unsigned char* image, unsigned w, unsigned h,
                 bool transparent_counts_as_one) {
  unique->clear();
  for (size_t i = 0; i < w * h; i++) {
    unsigned index = ColorIndex(&image[i * 4]);
    if (transparent_counts_as_one && image[i * 4 + 3] == 0) index = 0;
    unique->insert(index);
    if (unique->size() > 256) break;
  }
}

// Remove RGB information from pixels with alpha=0
void LossyOptimizeTransparent(lodepng::State* inputstate, unsigned char* image,
    unsigned w, unsigned h) {
  // First check if we want to preserve potential color-key background color,
  // or instead use the last encountered RGB value all the time to save bytes.
  bool key = true;
  for (size_t i = 0; i < w * h; i++) {
    if (image[i * 4 + 3] > 0 && image[i * 4 + 3] < 255) {
      key = false;
      break;
    }
  }
  std::set<unsigned> count;  // Color count, up to 257.
  CountColors(&count, image, w, h, true);
  // If true, means palette is possible so avoid using different RGB values for
  // the transparent color.
  bool palette = count.size() <= 256;

  // Choose the color key or first initial background color.
  int r = 0, g = 0, b = 0;
  if (key || palette) {
    for (size_t i = 0; i < w * h; i++) {
      if (image[i * 4 + 3] == 0) {
        // Use RGB value of first encountered transparent pixel. This can be
        // used as a valid color key, or in case of palette ensures a color
        // existing in the input image palette is used.
        r = image[i * 4 + 0];
        g = image[i * 4 + 1];
        b = image[i * 4 + 2];
        break;
      }
    }
  }

  for (size_t i = 0; i < w * h; i++) {
    // if alpha is 0, alter the RGB value to a possibly more efficient one.
    if (image[i * 4 + 3] == 0) {
      image[i * 4 + 0] = r;
      image[i * 4 + 1] = g;
      image[i * 4 + 2] = b;
    } else {
      if (!key && !palette) {
        // Use the last encountered RGB value if no key or palette is used: that
        // way more values can be 0 thanks to the PNG filter types.
        r = image[i * 4 + 0];
        g = image[i * 4 + 1];
        b = image[i * 4 + 2];
      }
    }
  }

  // If there are now less colors, update palette of input image to match this.
  if (palette && inputstate->info_png.color.palettesize > 0) {
    CountColors(&count, image, w, h, false);
    if (count.size() < inputstate->info_png.color.palettesize) {
      std::vector<unsigned char> palette_out;
      unsigned char* palette_in = inputstate->info_png.color.palette;
      for (size_t i = 0; i < inputstate->info_png.color.palettesize; i++) {
        if (count.count(ColorIndex(&palette_in[i * 4])) != 0) {
          palette_out.push_back(palette_in[i * 4 + 0]);
          palette_out.push_back(palette_in[i * 4 + 1]);
          palette_out.push_back(palette_in[i * 4 + 2]);
          palette_out.push_back(palette_in[i * 4 + 3]);
        }
      }
      inputstate->info_png.color.palettesize = palette_out.size() / 4;
      for (size_t i = 0; i < palette_out.size(); i++) {
        palette_in[i] = palette_out[i];
      }
    }
  }
}

// Tries to optimize given a single PNG filter strategy.
// Returns 0 if ok, other value for error
unsigned TryOptimize(
    const std::vector<unsigned char>& image, unsigned w, unsigned h,
    const lodepng::State& inputstate, bool bit16, bool keep_colortype,
    const std::vector<unsigned char>& origfile,
    ZopfliPNGFilterStrategy filterstrategy,
    bool use_zopfli, int windowsize, const ZopfliPNGOptions* png_options,
    std::vector<unsigned char>* out) {
  unsigned error = 0;

  lodepng::State state;
  state.encoder.zlibsettings.windowsize = windowsize;
  if (use_zopfli && png_options->use_zopfli) {
    state.encoder.zlibsettings.custom_deflate = CustomPNGDeflate;
    state.encoder.zlibsettings.custom_context = png_options;
  }

  if (keep_colortype) {
    state.encoder.auto_convert = 0;
    lodepng_color_mode_copy(&state.info_png.color, &inputstate.info_png.color);
  }
  if (inputstate.info_png.color.colortype == LCT_PALETTE) {
    // Make it preserve the original palette order
    lodepng_color_mode_copy(&state.info_raw, &inputstate.info_png.color);
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
  }
  if (bit16) {
    state.info_raw.bitdepth = 16;
  }

  state.encoder.filter_palette_zero = 0;

  std::vector<unsigned char> filters;
  switch (filterstrategy) {
    case kStrategyZero:
      state.encoder.filter_strategy = LFS_ZERO;
      break;
    case kStrategyMinSum:
      state.encoder.filter_strategy = LFS_MINSUM;
      break;
    case kStrategyEntropy:
      state.encoder.filter_strategy = LFS_ENTROPY;
      break;
    case kStrategyBruteForce:
      state.encoder.filter_strategy = LFS_BRUTE_FORCE;
      break;
    case kStrategyOne:
      state.encoder.filter_strategy = LFS_ONE;
      break;
    case kStrategyTwo:
      state.encoder.filter_strategy = LFS_TWO;
      break;
    case kStrategyThree:
      state.encoder.filter_strategy = LFS_THREE;
      break;
    case kStrategyFour:
      state.encoder.filter_strategy = LFS_FOUR;
      break;
    case kStrategyPredefined:
      lodepng::getFilterTypes(filters, origfile);
      if (filters.size() != h) return 1;  // Error getting filters
      state.encoder.filter_strategy = LFS_PREDEFINED;
      state.encoder.predefined_filters = &filters[0];
      break;
    default:
      break;
  }

  state.encoder.add_id = false;
  state.encoder.text_compression = 1;

  error = lodepng::encode(*out, image, w, h, state);

  // For very small output, also try without palette, it may be smaller thanks
  // to no palette storage overhead.
  if (!error && out->size() < 4096 && !keep_colortype) {
    if (lodepng::getPNGHeaderInfo(*out).color.colortype == LCT_PALETTE) {
      LodePNGColorStats stats;
      lodepng_color_stats_init(&stats);
      lodepng_compute_color_stats(&stats, &image[0], w, h, &state.info_raw);
      // Too small for tRNS chunk overhead.
      if (w * h <= 16 && stats.key) stats.alpha = 1;
      state.encoder.auto_convert = 0;
      state.info_png.color.colortype = (stats.alpha ? LCT_RGBA : LCT_RGB);
      state.info_png.color.bitdepth = 8;
      state.info_png.color.key_defined = (stats.key && !stats.alpha);
      if (state.info_png.color.key_defined) {
        state.info_png.color.key_defined = 1;
        state.info_png.color.key_r = (stats.key_r & 255u);
        state.info_png.color.key_g = (stats.key_g & 255u);
        state.info_png.color.key_b = (stats.key_b & 255u);
      }

      std::vector<unsigned char> out2;
      error = lodepng::encode(out2, image, w, h, state);
      if (out2.size() < out->size()) out->swap(out2);
    }
  }

  if (error) {
    printf("Encoding error %u: %s\n", error, lodepng_error_text(error));
    return error;
  }

  return 0;
}

// Use fast compression to check which PNG filter strategy gives the smallest
// output. This allows to then do the slow and good compression only on that
// filter type.
unsigned AutoChooseFilterStrategy(const std::vector<unsigned char>& image,
                                  unsigned w, unsigned h,
                                  const lodepng::State& inputstate,
                                  bool bit16, bool keep_colortype,
                                  const std::vector<unsigned char>& origfile,
                                  int numstrategies,
                                  ZopfliPNGFilterStrategy* strategies,
                                  bool* enable) {
  std::vector<unsigned char> out;
  size_t bestsize = 0;
  int bestfilter = 0;

  // A large window size should still be used to do the quick compression to
  // try out filter strategies: which filter strategy is the best depends
  // largely on the window size, the closer to the actual used window size the
  // better.
  int windowsize = 8192;

  for (int i = 0; i < numstrategies; i++) {
    out.clear();
    unsigned error = TryOptimize(image, w, h, inputstate, bit16, keep_colortype,
                                 origfile, strategies[i], false, windowsize, 0,
                                 &out);
    if (error) return error;
    if (bestsize == 0 || out.size() < bestsize) {
      bestsize = out.size();
      bestfilter = i;
    }
  }

  for (int i = 0; i < numstrategies; i++) {
    enable[i] = (i == bestfilter);
  }

  return 0;  /* OK */
}

// Outputs the intersection of keepnames and non-essential chunks which are in
// the PNG image.
void ChunksToKeep(const std::vector<unsigned char>& origpng,
                  const std::vector<std::string>& keepnames,
                  std::set<std::string>* result) {
  std::vector<std::string> names[3];
  std::vector<std::vector<unsigned char> > chunks[3];

  lodepng::getChunks(names, chunks, origpng);

  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < names[i].size(); j++) {
      for (size_t k = 0; k < keepnames.size(); k++) {
        if (keepnames[k] == names[i][j]) {
          result->insert(names[i][j]);
        }
      }
    }
  }
}

// Keeps chunks with given names from the original png by literally copying them
// into the new png
void KeepChunks(const std::vector<unsigned char>& origpng,
                const std::vector<std::string>& keepnames,
                std::vector<unsigned char>* png) {
  std::vector<std::string> names[3];
  std::vector<std::vector<unsigned char> > chunks[3];

  lodepng::getChunks(names, chunks, origpng);
  std::vector<std::vector<unsigned char> > keepchunks[3];

  // There are 3 distinct locations in a PNG file for chunks: between IHDR and
  // PLTE, between PLTE and IDAT, and between IDAT and IEND. Keep each chunk at
  // its corresponding location in the new PNG.
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = 0; j < names[i].size(); j++) {
      for (size_t k = 0; k < keepnames.size(); k++) {
        if (keepnames[k] == names[i][j]) {
          keepchunks[i].push_back(chunks[i][j]);
        }
      }
    }
  }

  lodepng::insertChunks(*png, keepchunks);
}

int ZopfliPNGOptimize(const std::vector<unsigned char>& origpng,
    const ZopfliPNGOptions& png_options,
    bool verbose,
    std::vector<unsigned char>* resultpng) {
  // Use the largest possible deflate window size
  int windowsize = 32768;

  ZopfliPNGFilterStrategy filterstrategies[kNumFilterStrategies] = {
    kStrategyZero, kStrategyOne, kStrategyTwo, kStrategyThree, kStrategyFour,
    kStrategyMinSum, kStrategyEntropy, kStrategyPredefined, kStrategyBruteForce
  };
  bool strategy_enable[kNumFilterStrategies] = {
    false, false, false, false, false, false, false, false, false
  };
  std::string strategy_name[kNumFilterStrategies] = {
    "zero", "one", "two", "three", "four",
    "minimum sum", "entropy", "predefined", "brute force"
  };
  for (size_t i = 0; i < png_options.filter_strategies.size(); i++) {
    strategy_enable[png_options.filter_strategies[i]] = true;
  }

  std::vector<unsigned char> image;
  unsigned w, h;
  unsigned error;
  lodepng::State inputstate;
  error = lodepng::decode(image, w, h, inputstate, origpng);

  bool keep_colortype = png_options.keep_colortype;

  if (!png_options.keepchunks.empty()) {
    // If the user wants to keep the non-essential chunks bKGD or sBIT, the
    // input color type has to be kept since the chunks format depend on it.
    // This may severely hurt compression if it is not an ideal color type.
    // Ideally these chunks should not be kept for web images. Handling of bKGD
    // chunks could be improved by changing its color type but not done yet due
    // to its additional complexity, for sBIT such improvement is usually not
    // possible.
    std::set<std::string> keepchunks;
    ChunksToKeep(origpng, png_options.keepchunks, &keepchunks);
    if (keepchunks.count("bKGD") || keepchunks.count("sBIT")) {
      if (!keep_colortype && verbose) {
        printf("Forced to keep original color type due to keeping bKGD or sBIT"
               " chunk.\n");
      }
      keep_colortype = true;
    }
  }

  if (error) {
    if (verbose) {
      if (error == 1) {
        printf("Decoding error\n");
      } else {
        printf("Decoding error %u: %s\n", error, lodepng_error_text(error));
      }
    }
    return error;
  }

  bool bit16 = false;  // Using 16-bit per channel raw image
  if (inputstate.info_png.color.bitdepth == 16 &&
      (keep_colortype || !png_options.lossy_8bit)) {
    // Decode as 16-bit
    image.clear();
    error = lodepng::decode(image, w, h, origpng, LCT_RGBA, 16);
    bit16 = true;
  }

  if (!error) {
    // If lossy_transparent, remove RGB information from pixels with alpha=0
    if (png_options.lossy_transparent && !bit16) {
      LossyOptimizeTransparent(&inputstate, &image[0], w, h);
    }

    if (png_options.auto_filter_strategy) {
      error = AutoChooseFilterStrategy(image, w, h, inputstate, bit16,
                                       keep_colortype, origpng,
                                       /* Don't try brute force */
                                       kNumFilterStrategies - 1,
                                       filterstrategies, strategy_enable);
    }
  }

  if (!error) {
    size_t bestsize = 0;

    for (int i = 0; i < kNumFilterStrategies; i++) {
      if (!strategy_enable[i]) continue;

      std::vector<unsigned char> temp;
      error = TryOptimize(image, w, h, inputstate, bit16, keep_colortype,
                          origpng, filterstrategies[i], true /* use_zopfli */,
                          windowsize, &png_options, &temp);
      if (!error) {
        if (verbose) {
          printf("Filter strategy %s: %d bytes\n",
                 strategy_name[i].c_str(), (int) temp.size());
        }
        if (bestsize == 0 || temp.size() < bestsize) {
          bestsize = temp.size();
          (*resultpng).swap(temp);  // Store best result so far in the output.
        }
      }
    }

    if (!png_options.keepchunks.empty()) {
      KeepChunks(origpng, png_options.keepchunks, resultpng);
    }
  }

  return error;
}

extern "C" void CZopfliPNGSetDefaults(CZopfliPNGOptions* png_options) {

  memset(png_options, 0, sizeof(*png_options));
  // Constructor sets the defaults
  ZopfliPNGOptions opts;

  png_options->lossy_transparent    = opts.lossy_transparent;
  png_options->lossy_8bit           = opts.lossy_8bit;
  png_options->auto_filter_strategy = opts.auto_filter_strategy;
  png_options->use_zopfli           = opts.use_zopfli;
  png_options->num_iterations       = opts.num_iterations;
  png_options->num_iterations_large = opts.num_iterations_large;
  png_options->block_split_strategy = opts.block_split_strategy;
}

extern "C" int CZopfliPNGOptimize(const unsigned char* origpng,
                                  const size_t origpng_size,
                                  const CZopfliPNGOptions* png_options,
                                  int verbose,
                                  unsigned char** resultpng,
                                  size_t* resultpng_size) {
  ZopfliPNGOptions opts;

  // Copy over to the C++-style struct
  opts.lossy_transparent    = !!png_options->lossy_transparent;
  opts.lossy_8bit           = !!png_options->lossy_8bit;
  opts.auto_filter_strategy = !!png_options->auto_filter_strategy;
  opts.use_zopfli           = !!png_options->use_zopfli;
  opts.num_iterations       = png_options->num_iterations;
  opts.num_iterations_large = png_options->num_iterations_large;
  opts.block_split_strategy = png_options->block_split_strategy;

  for (int i = 0; i < png_options->num_filter_strategies; i++) {
    opts.filter_strategies.push_back(png_options->filter_strategies[i]);
  }

  for (int i = 0; i < png_options->num_keepchunks; i++) {
    opts.keepchunks.push_back(png_options->keepchunks[i]);
  }

  const std::vector<unsigned char> origpng_cc(origpng, origpng + origpng_size);
  std::vector<unsigned char> resultpng_cc;

  int ret = ZopfliPNGOptimize(origpng_cc, opts, !!verbose, &resultpng_cc);
  if (ret) {
    return ret;
  }

  *resultpng_size = resultpng_cc.size();
  *resultpng      = (unsigned char*) malloc(resultpng_cc.size());
  if (!(*resultpng)) {
    return ENOMEM;
  }

  memcpy(*resultpng,
         reinterpret_cast<unsigned char*>(&resultpng_cc[0]),
         resultpng_cc.size());

  return 0;
}
