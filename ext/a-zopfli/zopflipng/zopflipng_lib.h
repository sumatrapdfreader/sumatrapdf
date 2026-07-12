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
  kNumFilterStrategies
};

typedef struct CZopfliPNGOptions {
  int lossy_transparent;
  int lossy_8bit;

  enum ZopfliPNGFilterStrategy* filter_strategies;

  int num_filter_strategies;

  int auto_filter_strategy;

  char** keepchunks;

  int num_keepchunks;

  int use_zopfli;

  int num_iterations;

  int num_iterations_large;

  int block_split_strategy;
} CZopfliPNGOptions;

void CZopfliPNGSetDefaults(CZopfliPNGOptions *png_options);

int CZopfliPNGOptimize(const unsigned char* origpng,
    const size_t origpng_size,
    const CZopfliPNGOptions* png_options,
    int verbose,
    unsigned char** resultpng,
    size_t* resultpng_size);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

struct ZopfliPNGOptions {
  ZopfliPNGOptions();

  bool verbose;

  bool lossy_transparent;

  bool lossy_8bit;

  std::vector<ZopfliPNGFilterStrategy> filter_strategies;

  bool auto_filter_strategy;

  bool keep_colortype;

  std::vector<std::string> keepchunks;

  bool use_zopfli;

  int num_iterations;

  int num_iterations_large;

  int block_split_strategy;
};

int ZopfliPNGOptimize(const std::vector<unsigned char>& origpng,
    const ZopfliPNGOptions& png_options,
    bool verbose,
    std::vector<unsigned char>* resultpng);

#endif

#endif
