#ifndef _RAR_COMPRESS_
#define _RAR_COMPRESS_

#define MAX_LZ_MATCH          0x1001
#define MAX3_LZ_MATCH          0x101 // Maximum match length for RAR v3.

#define LOW_DIST_REP_COUNT 16

#define NC 306  /* alphabet = {0, 1, 2, ..., NC - 1} */
#define DC  64
#define LDC 16
#define RC  44
#define HUFF_TABLE_SIZE (NC+DC+RC+LDC)
#define BC  20

#define NC30 299  /* alphabet = {0, 1, 2, ..., NC - 1} */
#define DC30  60
#define LDC30 17
#define RC30  28
#define BC30  20
#define HUFF_TABLE_SIZE30 (NC30+DC30+RC30+LDC30)

#define NC20 298  /* alphabet = {0, 1, 2, ..., NC - 1} */
#define DC20 48
#define RC20 28
#define BC20 19
#define MC20 257

// Largest alphabet size among all values listed above.
#define LARGEST_TABLE_SIZE 306

enum {CODE_HUFFMAN,CODE_LZ,CODE_REPEATLZ,CODE_CACHELZ,
      CODE_STARTFILE,CODE_ENDFILE,CODE_FILTER,CODE_FILTERDATA};


enum FilterType {
  // These values must not be changed, because we use them directly
  // in RAR5 compression and decompression code.
  FILTER_DELTA=0, FILTER_E8, FILTER_E8E9, FILTER_ARM, 
  FILTER_AUDIO, FILTER_RGB, FILTER_ITANIUM, FILTER_PPM, FILTER_NONE
};

#endif
