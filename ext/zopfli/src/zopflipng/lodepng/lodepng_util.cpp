/*
LodePNG Utils

Copyright (c) 2005-2020 Lode Vandevenne

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
    distribution.
*/

#include "lodepng_util.h"
#include <iostream>  // TODO: remove, don't print stuff from here, return errors instead
#include <stdlib.h> /* allocations */

namespace lodepng {

LodePNGInfo getPNGHeaderInfo(const std::vector<unsigned char>& png) {
  unsigned w, h;
  lodepng::State state;
  lodepng_inspect(&w, &h, &state, &png[0], png.size());
  return state.info_png;
}

unsigned getChunkInfo(std::vector<std::string>& names, std::vector<size_t>& sizes,
                      const std::vector<unsigned char>& png) {
  // Listing chunks is based on the original file, not the decoded png info.
  const unsigned char *chunk, *end;
  end = &png.back() + 1;
  chunk = &png.front() + 8;

  while(chunk < end && end - chunk >= 8) {
    char type[5];
    lodepng_chunk_type(type, chunk);
    if(std::string(type).size() != 4) return 1;

    unsigned length = lodepng_chunk_length(chunk);
    names.push_back(type);
    sizes.push_back(length);
    chunk = lodepng_chunk_next_const(chunk, end);
  }
  return 0;
}

unsigned getChunks(std::vector<std::string> names[3],
                   std::vector<std::vector<unsigned char> > chunks[3],
                   const std::vector<unsigned char>& png) {
  const unsigned char *chunk, *next, *end;
  end = &png.back() + 1;
  chunk = &png.front() + 8;

  int location = 0;

  while(chunk < end && end - chunk >= 8) {
    char type[5];
    lodepng_chunk_type(type, chunk);
    std::string name(type);
    if(name.size() != 4) return 1;

    next = lodepng_chunk_next_const(chunk, end);

    if(name == "IHDR") {
      location = 0;
    } else if(name == "PLTE") {
      location = 1;
    } else if(name == "IDAT") {
      location = 2;
    } else if(name == "IEND") {
      break; // anything after IEND is not part of the PNG or the 3 groups here.
    } else {
      if(next >= end) return 1; // invalid chunk, content too far
      names[location].push_back(name);
      chunks[location].push_back(std::vector<unsigned char>(chunk, next));
    }

    chunk = next;
  }
  return 0;
}


unsigned insertChunks(std::vector<unsigned char>& png,
                      const std::vector<std::vector<unsigned char> > chunks[3]) {
  const unsigned char *chunk, *begin, *end;
  end = &png.back() + 1;
  begin = chunk = &png.front() + 8;

  long l0 = 0; //location 0: IHDR-l0-PLTE (or IHDR-l0-l1-IDAT)
  long l1 = 0; //location 1: PLTE-l1-IDAT (or IHDR-l0-l1-IDAT)
  long l2 = 0; //location 2: IDAT-l2-IEND

  while(chunk < end && end - chunk >= 8) {
    char type[5];
    lodepng_chunk_type(type, chunk);
    std::string name(type);
    if(name.size() != 4) return 1;

    if(name == "PLTE") {
      if(l0 == 0) l0 = chunk - begin + 8;
    } else if(name == "IDAT") {
      if(l0 == 0) l0 = chunk - begin + 8;
      if(l1 == 0) l1 = chunk - begin + 8;
    } else if(name == "IEND") {
      if(l2 == 0) l2 = chunk - begin + 8;
    }

    chunk = lodepng_chunk_next_const(chunk, end);
  }

  std::vector<unsigned char> result;
  result.insert(result.end(), png.begin(), png.begin() + l0);
  for(size_t i = 0; i < chunks[0].size(); i++) result.insert(result.end(), chunks[0][i].begin(), chunks[0][i].end());
  result.insert(result.end(), png.begin() + l0, png.begin() + l1);
  for(size_t i = 0; i < chunks[1].size(); i++) result.insert(result.end(), chunks[1][i].begin(), chunks[1][i].end());
  result.insert(result.end(), png.begin() + l1, png.begin() + l2);
  for(size_t i = 0; i < chunks[2].size(); i++) result.insert(result.end(), chunks[2][i].begin(), chunks[2][i].end());
  result.insert(result.end(), png.begin() + l2, png.end());

  png = result;
  return 0;
}

unsigned getFilterTypesInterlaced(std::vector<std::vector<unsigned char> >& filterTypes,
                                  const std::vector<unsigned char>& png) {
  //Get color type and interlace type
  lodepng::State state;
  unsigned w, h;
  unsigned error;
  error = lodepng_inspect(&w, &h, &state, &png[0], png.size());

  if(error) return 1;

  //Read literal data from all IDAT chunks
  const unsigned char *chunk, *begin, *end;
  end = &png.back() + 1;
  begin = chunk = &png.front() + 8;

  std::vector<unsigned char> zdata;

  while(chunk < end && end - chunk >= 8) {
    char type[5];
    lodepng_chunk_type(type, chunk);
    if(std::string(type).size() != 4) break; //Probably not a PNG file

    if(std::string(type) == "IDAT") {
      const unsigned char* cdata = lodepng_chunk_data_const(chunk);
      unsigned clength = lodepng_chunk_length(chunk);
      if(chunk + clength + 12 > end || clength > png.size() || chunk + clength + 12 < begin) {
        // corrupt chunk length
        return 1;
      }

      for(unsigned i = 0; i < clength; i++) {
        zdata.push_back(cdata[i]);
      }
    }

    chunk = lodepng_chunk_next_const(chunk, end);
  }

  //Decompress all IDAT data (if the while loop ended early, this might fail)
  std::vector<unsigned char> data;
  error = lodepng::decompress(data, &zdata[0], zdata.size());

  if(error) return 1;

  if(state.info_png.interlace_method == 0) {
    filterTypes.resize(1);

    //A line is 1 filter byte + all pixels
    size_t linebytes = 1 + lodepng_get_raw_size(w, 1, &state.info_png.color);

    for(size_t i = 0; i < data.size(); i += linebytes) {
      filterTypes[0].push_back(data[i]);
    }
  } else {
    //Interlaced
    filterTypes.resize(7);
    static const unsigned ADAM7_IX[7] = { 0, 4, 0, 2, 0, 1, 0 }; /*x start values*/
    static const unsigned ADAM7_IY[7] = { 0, 0, 4, 0, 2, 0, 1 }; /*y start values*/
    static const unsigned ADAM7_DX[7] = { 8, 8, 4, 4, 2, 2, 1 }; /*x delta values*/
    static const unsigned ADAM7_DY[7] = { 8, 8, 8, 4, 4, 2, 2 }; /*y delta values*/
    size_t pos = 0;
    for(size_t j = 0; j < 7; j++) {
      unsigned w2 = (w - ADAM7_IX[j] + ADAM7_DX[j] - 1) / ADAM7_DX[j];
      unsigned h2 = (h - ADAM7_IY[j] + ADAM7_DY[j] - 1) / ADAM7_DY[j];
      if(ADAM7_IX[j] >= w || ADAM7_IY[j] >= h) continue;
      size_t linebytes = 1 + lodepng_get_raw_size(w2, 1, &state.info_png.color);
      for(size_t i = 0; i < h2; i++) {
        filterTypes[j].push_back(data[pos]);
        pos += linebytes;
      }
    }
  }
  return 0; /* OK */
}


unsigned getFilterTypes(std::vector<unsigned char>& filterTypes, const std::vector<unsigned char>& png) {
  std::vector<std::vector<unsigned char> > passes;
  unsigned error = getFilterTypesInterlaced(passes, png);
  if(error) return error;

  if(passes.size() == 1) {
    filterTypes.swap(passes[0]);
  } else {
    // Simplify interlaced filter types to get a single filter value per scanline:
    // put pass 6 and 7 alternating in the one vector, these filters
    // correspond to the closest to what it would be for non-interlaced
    // image. If the image is only 1 pixel wide, pass 6 doesn't exist so the
    // alternative values column0 are used. The shift values are to match
    // the y position in the interlaced sub-images.
    // NOTE: the values 0-6 match Adam7's passes 1-7.
    const unsigned column0[8] = {0, 6, 4, 6, 2, 6, 4, 6};
    const unsigned column1[8] = {5, 6, 5, 6, 5, 6, 5, 6};
    const unsigned shift0[8] = {3, 1, 2, 1, 3, 1, 2, 1};
    const unsigned shift1[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    lodepng::State state;
    unsigned w, h;
    lodepng_inspect(&w, &h, &state, &png[0], png.size());
    const unsigned* column = w > 1 ? column1 : column0;
    const unsigned* shift = w > 1 ? shift1 : shift0;
    for(size_t i = 0; i < h; i++) {
      filterTypes.push_back(passes[column[i & 7u]][i >> shift[i & 7u]]);
    }
  }
  return 0; /* OK */
}

int getPaletteValue(const unsigned char* data, size_t i, int bits) {
  if(bits == 8) return data[i];
  else if(bits == 4) return (data[i / 2] >> ((i % 2) * 4)) & 15;
  else if(bits == 2) return (data[i / 4] >> ((i % 4) * 2)) & 3;
  else if(bits == 1) return (data[i / 8] >> (i % 8)) & 1;
  else return 0;
}


////////////////////////////////////////////////////////////////////////////////

#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS



// Only temporarily here until this is integrated into lodepng.c(pp)
#define LODEPNG_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define LODEPNG_MIN(a, b) (((a) < (b)) ? (a) : (b))

// Only temporarily here until this is integrated into lodepng.c(pp)
#ifdef LODEPNG_COMPILE_ALLOCATORS
static void* lodepng_malloc(size_t size) {
  return malloc(size);
}
static void lodepng_free(void* ptr) {
  free(ptr);
}
#else /*LODEPNG_COMPILE_ALLOCATORS*/
void* lodepng_malloc(size_t size);
void lodepng_free(void* ptr);
#endif /*LODEPNG_COMPILE_ALLOCATORS*/

/* avoid needing <float.h> for FLT_MAX. This assumes IEEE 32-bit float. */
static const float lodepng_flt_max = 3.40282346638528859811704183484516925e38f;

/* define infinity and NaN in a way compatible with ANSI C90 (no INFINITY or NAN macros) yet also with visual studio */
/* visual studio doesn't allow division through a zero literal, but allows it through non-const variable set to zero */
float lodepng_flt_zero_ = 0.0f;
static const float lodepng_flt_inf = 1.0f / lodepng_flt_zero_; /* infinity */
static const float lodepng_flt_nan = 0.0f / lodepng_flt_zero_; /* not a number */


/* powf polyfill, 5-6 digits accurate, 33-80% slower than powf, assumes IEEE
32-bit float, but other than that multiplatform and no math lib needed
(note: powf also isn't in ISO C90, and pow is slower). */
static float lodepng_powf(float x, float y) {
  float j, t0, t1, l;
  int i = 0;
  /* handle all the special floating point rules */
  if(x == 1 || y == 0) return 1; /*these cases return 1 even if the other value is NaN, as specified*/
  if(y == 1) return x;
  if(!(x > 0 && x <= lodepng_flt_max && y == y && y <= lodepng_flt_max && y >= -lodepng_flt_max)) {
    if(y == 1) return x; /* preserves negative-0 */
    if(x != x || y != y) return x + y; /* nan */
    if(x > 0) {
      if(x > lodepng_flt_max) return y <= 0 ? (y == 0 ? 1 : 0) : x; /* x = +infinity */
    } else {
      if(!(y < -1073741824.0f || y > 1073741824.0f)) { /* large y always even integer, but cast would overflow */
        i = (int)y;
        if(i != y) {
          return (x < -lodepng_flt_max) ? (y < 0 ? 0 : lodepng_flt_inf) :
              (x == 0 ? (y < 0 ? lodepng_flt_inf : 0) : lodepng_flt_nan);
        }
        if(i & 1) return x == 0 ? (y < 0 ? (1 / x) : x) : -lodepng_powf(-x, y);
      }
      if(x == 0) return y <= 0 ? lodepng_flt_inf : 0;
      if(x < -lodepng_flt_max) { /* x == -infinity */
        return y <= 0 ? (y == 0 ? 1 : 0) : ((i & 1) ?
            -lodepng_flt_inf : lodepng_flt_inf);
      }
      x = -x;
      if(x == 1) return 1;
    }
    if(y < -lodepng_flt_max || y > lodepng_flt_max) return ((x < 1) != (y > 0)) ? (y < 0 ? -y : y) : 0;
  }

  l = x;
  j = 0;
  while(l < (1.0f / 65536)) { j -= 16; l *= 65536.0f; }
  while(l > 65536) { j += 16; l *= (1.0f / 65536); }
  while(l < 1) { j--; l *= 2.0f; }
  while(l > 2) { j++; l *= 0.5f; }
  /* polynomial to approximate log2(x) with x in range 1..2 */
  t0 = -0.393118410458557f + l * (-0.0883639468229365f + l * (0.466142650227994f + l * 0.0153397331014276f));
  t1 = 0.0907447971403586f + l * (0.388892024755479f + l * 0.137228280305862f);
  l = t0 / t1 + j;

  l *= y; /* using the formula exp2(y * log2(x)) */

  /* prevent int shift overflow, 0 or inf result are ok to return since exp will be taken, 127 is max float exponent */
  if(l <= -128.0f || l >= 128.0f) return ((x > 1) == (y > 0)) ? lodepng_flt_inf : 0;
  i = (int)l;
  l -= i;
  /* polynomial to approximate exp2(x) with x in range -1..1 */
  t0 = 1.0f + l * (0.41777833582744256f + l * (0.0728482595347711f + l * 0.005635023478609625f));
  t1 = 1.0f + l * (-0.27537016151408167f + l * 0.023501446055084033f);
  while(i <= -31) { t0 *= (1.0f / 2147483648.0f); i += 31; }
  while(i >= 31) { t0 *= 2147483648.0f; i -= 31; }
  return (i < 0) ? (t0 / (t1 * (1 << -i))) : ((t0 * (1 << i)) / t1);
}

/* Parameters of a tone reproduction curve, either with a power law formula or with a lookup table. */
typedef struct {
  unsigned type; /* 0=linear, 1=lut, 2 = simple gamma, 3-6 = parametric (matches ICC parametric types 1-4) */
  float* lut; /* for type 1 */
  size_t lut_size;
  float gamma; /* for type 2 and more */
  float a, b, c, d, e, f; /* parameters for type 3-7 */
} LodePNGICCCurve;

void lodepng_icc_curve_init(LodePNGICCCurve* curve) {
  curve->lut = 0;
  curve->lut_size = 0;
}

void lodepng_icc_curve_cleanup(LodePNGICCCurve* curve) {
  lodepng_free(curve->lut);
}

/* Values parsed from ICC profile, see parseICC for more information about this subset.*/
typedef struct {
  /* 0 = color model not supported by PNG (CMYK, Lab, ...), 1 = gray, 2 = RGB */
  int inputspace;
  int version_major;
  int version_minor;
  int version_bugfix;

  /* The whitepoint of the profile connection space (PCS). Should always be D50, but parsed and used anyway.
  (to be clear, whitepoint and illuminant are synonyms in practice, but here field "illuminant" is ICC's
  "global" whitepoint that is always D50, and the field "white" below allows deriving the whitepoint of
  the particular RGB space represented here) */
  float illuminant[3];

  /* if true, has chromatic adaptation matrix that must be used. If false, you must compute a chromatic adaptation
  matrix yourself from "illuminant" and "white". */
  unsigned has_chad;
  float chad[9]; /* chromatic adaptation matrix, if given */

  /* The whitepoint of the RGB color space as stored in the ICC file. If has_chad, must be adapted with the
  chad matrix to become the one we need to go to absolute XYZ (in fact ICC implies it should then be
  exactly D50 in the file, redundantly, before this transformation with chad), else use as-is (then its
  values can actually be something else than D50, and are the ones we need). */
  unsigned has_whitepoint;
  float white[3];
  /* Chromaticities of the RGB space in XYZ color space, but given such that you must still
  whitepoint adapt them from D50 to the RGB space whitepoint to go to absolute XYZ (if has_chad,
  with chad, else with bradford adaptation matrix from illuminant to white). */
  unsigned has_chromaticity;
  float red[3];
  float green[3];
  float blue[3];

  unsigned has_trc; /* TRC = tone reproduction curve (aka "gamma correction") */

  /* TRC's for the three channels (only first one used if grayscale) */
  LodePNGICCCurve trc[3];
} LodePNGICC;

void lodepng_icc_init(LodePNGICC* icc) {
  lodepng_icc_curve_init(&icc->trc[0]);
  lodepng_icc_curve_init(&icc->trc[1]);
  lodepng_icc_curve_init(&icc->trc[2]);
}

void lodepng_icc_cleanup(LodePNGICC* icc) {
  lodepng_icc_curve_cleanup(&icc->trc[0]);
  lodepng_icc_curve_cleanup(&icc->trc[1]);
  lodepng_icc_curve_cleanup(&icc->trc[2]);
}

/* ICC tone response curve, nonlinear (encoded) to linear.
Input and output in range 0-1. If color was integer 0-255, multiply with (1.0f/255)
to get the correct floating point behavior.
Outside of range 0-1, will not clip but either return x itself, or in cases
where it makes sense, a value defined by the same function.
NOTE: ICC requires clipping, but we do that only later when converting float to integer.*/
static float iccForwardTRC(const LodePNGICCCurve* curve, float x) {
  if(curve->type == 0) {
    return x;
  }
  if(curve->type == 1) { /* Lookup table */
    float v0, v1, fraction;
    size_t index;
    if(!curve->lut) return 0; /* error */
    if(x < 0) return x;
    index = (size_t)(x * (curve->lut_size - 1));
    if(index >= curve->lut_size) return x;

    /* LERP */
    v0 = curve->lut[index];
    v1 = (index + 1 < curve->lut_size) ? curve->lut[index + 1] : 1.0f;
    fraction = (x * (curve->lut_size - 1)) - index;
    return v0 * (1 - fraction) + v1 * fraction;
  }
  if(curve->type == 2) {
    /* Gamma expansion */
    return (x > 0) ? lodepng_powf(x, curve->gamma) : x;
  }
  /* TODO: all the ones below are untested */
  if(curve->type == 3) {
    if(x < 0) return x;
    return x >= (-curve->b / curve->a) ? (lodepng_powf(curve->a * x + curve->b, curve->gamma) + curve->c) : 0;
  }
  if(curve->type == 4) {
    if(x < 0) return x;
    return x >= (-curve->b / curve->a) ? (lodepng_powf(curve->a * x + curve->b, curve->gamma) + curve->c) : curve->c;
  }
  if(curve->type == 5) {
    return x >= curve->d ? (lodepng_powf(curve->a * x + curve->b, curve->gamma)) : (curve->c * x);
  }
  if(curve->type == 6) {
    return x >= curve->d ? (lodepng_powf(curve->a * x + curve->b, curve->gamma) + curve->c) : (curve->c * x + curve->f);
  }
  return 0;
}

/* ICC tone response curve, linear to nonlinear (encoded).
Input and output in range 0-1. Outside of that range, will not clip but either
return x itself, or in cases where it makes sense, a value defined by the same function.
NOTE: ICC requires clipping, but we do that only later when converting float to integer.*/
static float iccBackwardTRC(const LodePNGICCCurve* curve, float x) {
  if(curve->type == 0) {
    return x;
  }
  if(curve->type == 1) {
    size_t a, b, m;
    float v;
    if(x <= 0) return x;
    if(x >= 1) return x;
    /* binary search in the table */
    /* TODO: use faster way of inverting the lookup table */
    a = 0;
    b = curve->lut_size;
    for(;;) {
      if(a == b) return curve->lut[a];
      if(a + 1 == b) {
        /* LERP */
        float v0 = curve->lut[a];
        float v1 = curve->lut[b];
        float fraction;
        if(v0 == v1) return v0;
        fraction = (x - v0) / (v1 - v0);
        return v0 * (1 - fraction) + v1 * fraction;
      }
      m = (a + b) / 2u;
      v = curve->lut[m];
      if(v > x) {
        b = m;
      } else {
        a = m;
      }
    }
    return 0;
  }
  if(curve->type == 2) {
    /* Gamma compression */
    return (x > 0) ? lodepng_powf(x, 1.0f / curve->gamma) : x;
  }
  /* TODO: all the ones below are untested  */
  if(curve->type == 3) {
    if(x < 0) return x;
    return x > 0 ? ((lodepng_powf(x, 1.0f / curve->gamma) - curve->b) / curve->a) : (-curve->b / curve->a);
  }
  if(curve->type == 4) {
    if(x < 0) return x;
    return x > curve->c ?
        ((lodepng_powf(x - curve->c, 1.0f / curve->gamma) - curve->b) / curve->a) :
        (-curve->b / curve->a);
  }
  if(curve->type == 5) {
    return x > (curve->c * curve->d) ?
        ((lodepng_powf(x, 1.0f / curve->gamma) - curve->b) / curve->a) :
        (x / curve->c);
  }
  if(curve->type == 6) {
    return x > (curve->c * curve->d + curve->f) ?
        ((lodepng_powf(x - curve->c, 1.0f / curve->gamma) - curve->b) / curve->a) :
        ((x - curve->f) / curve->c);
  }
  return 0;
}

static unsigned decodeICCUint16(const unsigned char* data, size_t size, size_t* pos) {
  *pos += 2;
  if (*pos > size) return 0;
  return (unsigned)((data[*pos - 2] << 8) | (data[*pos - 1]));
}

static unsigned decodeICCUint32(const unsigned char* data, size_t size, size_t* pos) {
  *pos += 4;
  if (*pos > size) return 0;
  return (unsigned)((data[*pos - 4] << 24) | (data[*pos - 3] << 16) | (data[*pos - 2] << 8) | (data[*pos - 1] << 0));
}

static int decodeICCInt32(const unsigned char* data, size_t size, size_t* pos) {
  *pos += 4;
  if (*pos > size) return 0;
  /*TODO: this is incorrect if sizeof(int) != 4*/
  return (data[*pos - 4] << 24) | (data[*pos - 3] << 16) | (data[*pos - 2] << 8) | (data[*pos - 1] << 0);
}

static float decodeICC15Fixed16(const unsigned char* data, size_t size, size_t* pos) {
  return decodeICCInt32(data, size, pos) / 65536.0;
}

static unsigned isICCword(const unsigned char* data, size_t size, size_t pos, const char* word) {
  if(pos + 4 > size) return 0;
  return data[pos + 0] == (unsigned char)word[0] &&
         data[pos + 1] == (unsigned char)word[1] &&
         data[pos + 2] == (unsigned char)word[2] &&
         data[pos + 3] == (unsigned char)word[3];
}

/* Parses a subset of the ICC profile, supporting the necessary mix of ICC v2
and ICC v4 required to correctly convert the RGB color space to XYZ.
Does not parse values not related to this specific PNG-related purpose, and
does not support non-RGB profiles or lookup-table based chroma (but it
supports lookup tables for TRC aka "gamma"). */
static unsigned parseICC(LodePNGICC* icc, const unsigned char* data, size_t size) {
  size_t i, j;
  size_t pos = 0;
  unsigned version;
  unsigned inputspace;
  size_t numtags;

  if(size < 132) return 1; /* Too small to be a valid icc profile. */

  icc->has_chromaticity = 0;
  icc->has_whitepoint = 0;
  icc->has_trc = 0;
  icc->has_chad = 0;

  icc->trc[0].type = icc->trc[1].type = icc->trc[2].type = 0;
  icc->white[0] = icc->white[1] = icc->white[2] = 0;
  icc->red[0] = icc->red[1] = icc->red[2] = 0;
  icc->green[0] = icc->green[1] = icc->green[2] = 0;
  icc->blue[0] = icc->blue[1] = icc->blue[2] = 0;

  pos = 8;
  version = decodeICCUint32(data, size, &pos);
  if(pos >= size) return 1;
  icc->version_major = (int)((version >> 24) & 255);
  icc->version_minor = (int)((version >> 20) & 15);
  icc->version_bugfix = (int)((version >> 16) & 15);

  pos = 16;
  inputspace = decodeICCUint32(data, size, &pos);
  if(pos >= size) return 1;
  if(inputspace == 0x47524159) {
    /* The string  "GRAY" as unsigned 32-bit int. */
    icc->inputspace = 1;
  } else if(inputspace == 0x52474220) {
    /* The string  "RGB " as unsigned 32-bit int. */
    icc->inputspace = 2;
  } else {
    /* unsupported by PNG (CMYK, YCbCr, Lab, HSV, ...) */
    icc->inputspace = 0;
  }

  /* Should always be 0.9642, 1.0, 0.8249 */
  pos = 68;
  icc->illuminant[0] = decodeICC15Fixed16(data, size, &pos);
  icc->illuminant[1] = decodeICC15Fixed16(data, size, &pos);
  icc->illuminant[2] = decodeICC15Fixed16(data, size, &pos);

  pos = 128;
  numtags = decodeICCUint32(data, size, &pos);
  if(pos >= size) return 1;
  /* scan for tags we want to handle */
  for(i = 0; i < numtags; i++) {
    size_t offset;
    unsigned tagsize;
    size_t namepos = pos;
    pos += 4;
    offset = decodeICCUint32(data, size, &pos);
    tagsize = decodeICCUint32(data, size, &pos);
    if(pos >= size || offset >= size) return 1;
    if(offset + tagsize > size) return 1;
    if(tagsize < 8) return 1;

    if(isICCword(data, size, namepos, "wtpt")) {
      offset += 8; /* skip tag and reserved */
      icc->white[0] = decodeICC15Fixed16(data, size, &offset);
      icc->white[1] = decodeICC15Fixed16(data, size, &offset);
      icc->white[2] = decodeICC15Fixed16(data, size, &offset);
      icc->has_whitepoint = 1;
    } else if(isICCword(data, size, namepos, "rXYZ")) {
      offset += 8; /* skip tag and reserved */
      icc->red[0] = decodeICC15Fixed16(data, size, &offset);
      icc->red[1] = decodeICC15Fixed16(data, size, &offset);
      icc->red[2] = decodeICC15Fixed16(data, size, &offset);
      icc->has_chromaticity = 1;
    } else if(isICCword(data, size, namepos, "gXYZ")) {
      offset += 8; /* skip tag and reserved */
      icc->green[0] = decodeICC15Fixed16(data, size, &offset);
      icc->green[1] = decodeICC15Fixed16(data, size, &offset);
      icc->green[2] = decodeICC15Fixed16(data, size, &offset);
      icc->has_chromaticity = 1;
    } else if(isICCword(data, size, namepos, "bXYZ")) {
      offset += 8; /* skip tag and reserved */
      icc->blue[0] = decodeICC15Fixed16(data, size, &offset);
      icc->blue[1] = decodeICC15Fixed16(data, size, &offset);
      icc->blue[2] = decodeICC15Fixed16(data, size, &offset);
      icc->has_chromaticity = 1;
    } else if(isICCword(data, size, namepos, "chad")) {
      offset += 8; /* skip datatype keyword "sf32" and reserved */
      for(j = 0; j < 9; j++) {
        icc->chad[j] = decodeICC15Fixed16(data, size, &offset);
      }
      icc->has_chad = 1;
    } else if(isICCword(data, size, namepos, "rTRC") ||
              isICCword(data, size, namepos, "gTRC") ||
              isICCword(data, size, namepos, "bTRC") ||
              isICCword(data, size, namepos, "kTRC")) {
      char c = (char)data[namepos];
      /* both 'k' and 'r' are stored in channel 0 */
      int channel = (c == 'b') ? 2 : (c == 'g' ? 1 : 0);
      /* "curv": linear, gamma power or LUT */
      if(isICCword(data, size, offset, "curv")) {
        size_t count;
        LodePNGICCCurve* trc = &icc->trc[channel];
        icc->has_trc = 1;
        offset += 8; /* skip tag "curv" and reserved */
        count = decodeICCUint32(data, size, &offset);
        if(count == 0) {
          trc->type = 0; /* linear */
        } else if(count == 1) {
          trc->type = 2; /* gamma */
          trc->gamma = decodeICCUint16(data, size, &offset) / 256.0f;
        } else {
          trc->type = 1; /* LUT */
          if(offset + count * 2 > size || count > 16777216) return 1; /* also avoid crazy count */
          trc->lut_size = count;
          trc->lut = (float*)lodepng_malloc(count * sizeof(float));
          for(j = 0; j < count; j++) {
            trc->lut[j] = decodeICCUint16(data, size, &offset) * (1.0f / 65535.0f);
          }
        }
      }
      /* "para": parametric formula with gamma power, multipliers, biases and comparison point */
      /* TODO: test this on a realistic sample */
      if(isICCword(data, size, offset, "para")) {
        unsigned type;
        LodePNGICCCurve* trc = &icc->trc[channel];
        icc->has_trc = 1;
        offset += 8; /* skip tag "para" and reserved */
        type = decodeICCUint16(data, size, &offset);
        offset += 2;
        if(type > 4) return 1; /* unknown parametric curve type */
        trc->type = type + 2;
        trc->gamma = decodeICC15Fixed16(data, size, &offset);
        if(type >= 1) {
          trc->a = decodeICC15Fixed16(data, size, &offset);
          trc->b = decodeICC15Fixed16(data, size, &offset);
        }
        if(type >= 2) {
          trc->c = decodeICC15Fixed16(data, size, &offset);
        }
        if(type >= 3) {
          trc->d = decodeICC15Fixed16(data, size, &offset);
        }
        if(type == 4) {
          trc->e = decodeICC15Fixed16(data, size, &offset);
          trc->f = decodeICC15Fixed16(data, size, &offset);
        }
      }
      /* TODO: verify: does the "chrm" tag participate in computation so should be parsed? */
    }
    /* Return error if any parse went beyond the filesize. Note that the
    parsing itself was always safe since it bound-checks inside. */
    if(offset > size) return 1;
  }

  return 0;
}

/* Multiplies 3 vector values with 3x3 matrix */
static void mulMatrix(float* x2, float* y2, float* z2, const float* m, double x, double y, double z) {
  /* double used as inputs even though in general the images are float, so the sums happen in
  double precision, because float can give numerical problems for nearby values */
  *x2 = x * m[0] + y * m[1] + z * m[2];
  *y2 = x * m[3] + y * m[4] + z * m[5];
  *z2 = x * m[6] + y * m[7] + z * m[8];
}

static void mulMatrixMatrix(float* result, const float* a, const float* b) {
  int i;
  float temp[9]; /* temp is to allow result and a or b to be the same */
  mulMatrix(&temp[0], &temp[3], &temp[6], a, b[0], b[3], b[6]);
  mulMatrix(&temp[1], &temp[4], &temp[7], a, b[1], b[4], b[7]);
  mulMatrix(&temp[2], &temp[5], &temp[8], a, b[2], b[5], b[8]);
  for(i = 0; i < 9; i++) result[i] = temp[i];
}

/* Inverts 3x3 matrix in place */
static unsigned invMatrix(float* m) {
  int i;
  /* double used instead of float for intermediate computations to avoid
  intermediate numerical precision issues */
  double e0 = (double)m[4] * m[8] - (double)m[5] * m[7];
  double e3 = (double)m[5] * m[6] - (double)m[3] * m[8];
  double e6 = (double)m[3] * m[7] - (double)m[4] * m[6];
  /* inverse determinant */
  double d = 1.0 / (m[0] * e0 + m[1] * e3 + m[2] * e6);
  float result[9];
  if((d > 0 ? d : -d) > 1e15) return 1; /* error, likely not invertible */
  result[0] = e0 * d;
  result[1] = ((double)m[2] * m[7] - (double)m[1] * m[8]) * d;
  result[2] = ((double)m[1] * m[5] - (double)m[2] * m[4]) * d;
  result[3] = e3 * d;
  result[4] = ((double)m[0] * m[8] - (double)m[2] * m[6]) * d;
  result[5] = ((double)m[3] * m[2] - (double)m[0] * m[5]) * d;
  result[6] = e6 * d;
  result[7] = ((double)m[6] * m[1] - (double)m[0] * m[7]) * d;
  result[8] = ((double)m[0] * m[4] - (double)m[3] * m[1]) * d;
  for(i = 0; i < 9; i++) m[i] = result[i];
  return 0; /* ok */
}

/* Get the matrix to go from linear RGB to XYZ given the RGB whitepoint and chromaticities in XYZ colorspace */
static unsigned getChrmMatrixXYZ(float* m,
                                 float wX, float wY, float wZ,
                                 float rX, float rY, float rZ,
                                 float gX, float gY, float gZ,
                                 float bX, float bY, float bZ) {
  float t[9];
  float rs, gs, bs;
  t[0] = rX; t[1] = gX; t[2] = bX;
  t[3] = rY; t[4] = gY; t[5] = bY;
  t[6] = rZ; t[7] = gZ; t[8] = bZ;
  if(invMatrix(t)) return 1; /* error, not invertible */
  mulMatrix(&rs, &gs, &bs, t, wX, wY, wZ);
  m[0] = rs * rX; m[1] = gs * gX; m[2] = bs * bX;
  m[3] = rs * rY; m[4] = gs * gY; m[5] = bs * bY;
  m[6] = rs * rZ; m[7] = gs * gZ; m[8] = bs * bZ;
  return 0;
}

/* Get the matrix to go from linear RGB to XYZ given the RGB whitepoint and chromaticities in xy colorspace */
static unsigned getChrmMatrixXY(float* m,
                                float wx, float wy,
                                float rx, float ry,
                                float gx, float gy,
                                float bx, float by) {
  if(wy == 0 || ry == 0 || gy == 0 || by == 0) {
    return 1; /* error, division through zero */
  } else {
    float wX = wx / wy, wY = 1, wZ = (1 - wx - wy) / wy;
    float rX = rx / ry, rY = 1, rZ = (1 - rx - ry) / ry;
    float gX = gx / gy, gY = 1, gZ = (1 - gx - gy) / gy;
    float bX = bx / by, bY = 1, bZ = (1 - bx - by) / by;
    return getChrmMatrixXYZ(m, wX, wY, wZ, rX, rY, rZ, gX, gY, gZ, bX, bY, bZ);
  }
}

/* Returns matrix that adapts from source whitepoint 0 to destination whitepoint 1.
Types: 0=XYZ scaling, 1=Bradford, 2=Vonkries */
static unsigned getAdaptationMatrix(float* m, int type,
                                    float wx0, float wy0, float wz0,
                                    float wx1, float wy1, float wz1) {
  int i;
  static const float bradford[9] = {
    0.8951f, 0.2664f, -0.1614f,
    -0.7502f, 1.7135f, 0.0367f,
    0.0389f, -0.0685f, 1.0296f
  };
  static const float bradfordinv[9] = {
    0.9869929f, -0.1470543f, 0.1599627f,
    0.4323053f, 0.5183603f, 0.0492912f,
   -0.0085287f, 0.0400428f, 0.9684867f
  };
  static const float vonkries[9] = {
    0.40024f, 0.70760f, -0.08081f,
    -0.22630f, 1.16532f, 0.04570f,
    0.00000f, 0.00000f, 0.91822f,
  };
  static const float vonkriesinv[9] = {
    1.8599364f, -1.1293816f, 0.2198974f,
    0.3611914f, 0.6388125f, -0.0000064f,
   0.0000000f, 0.0000000f, 1.0890636f
  };
  if(type == 0) {
    for(i = 0; i < 9; i++) m[i] = 0;
    m[0] = wx1 / wx0;
    m[4] = wy1 / wy0;
    m[8] = wz1 / wz0;
  } else {
    const float* cat = (type == 1) ? bradford : vonkries;
    const float* inv = (type == 1) ? bradfordinv : vonkriesinv;
    float rho0, gam0, bet0, rho1, gam1, bet1, rho2, gam2, bet2;
    mulMatrix(&rho0, &gam0, &bet0, cat, wx0, wy0, wz0);
    mulMatrix(&rho1, &gam1, &bet1, cat, wx1, wy1, wz1);
    rho2 = rho1 / rho0;
    gam2 = gam1 / gam0;
    bet2 = bet1 / bet0;
    /* Multiply diagonal matrix with cat */
    for(i = 0; i < 3; i++) {
      m[i + 0] = rho2 * cat[i + 0];
      m[i + 3] = gam2 * cat[i + 3];
      m[i + 6] = bet2 * cat[i + 6];
    }
    mulMatrixMatrix(m, inv, m);
  }
  return 0; /* ok */
}

/* validate whether the ICC profile is supported here for PNG */
static unsigned validateICC(const LodePNGICC* icc) {
  /* disable for unsupported things in the icc profile */
  if(icc->inputspace == 0) return 0;
  /* if we didn't recognize both chrm and trc, then maybe the ICC uses data
  types not supported here yet, so fall back to not using it. */
  if(icc->inputspace == 2) {
    /* RGB profile should have chromaticities */
    if(!icc->has_chromaticity) return 0;
  }
  /* An ICC profile without whitepoint is invalid for the kind of profiles used here. */
  if(!icc->has_whitepoint) return 0;
  if(!icc->has_trc) return 0;
  return 1; /* ok */
}

/* Returns chromaticity matrix for given ICC profile, adapted from ICC's
global illuminant as necessary.
Also returns the profile's whitepoint.
In case of a gray profile (icc->inputspace == 1), the identity matrix will be returned
so in that case you could skip the transform. */
static unsigned getICCChrm(float m[9], float whitepoint[3], const LodePNGICC* icc) {
  size_t i;
  if(icc->inputspace == 2) { /* RGB profile */
    float red[3], green[3], blue[3];
    float white[3]; /* the whitepoint of the RGB color space (absolute) */
    /* Adaptation matrix a.
    This is an adaptation needed for ICC's file format (due to it using
    an internal global illuminant unrelated to the actual images) */
    float a[9] = {1,0,0, 0,1,0, 0,0,1};
    /* If the profile has chromatic adaptation matrix "chad", use that one,
    else compute it from the illuminant and whitepoint. */
    if(icc->has_chad) {
      for(i = 0; i < 9; i++) a[i] = icc->chad[i];
      invMatrix(a);
    } else {
      if(getAdaptationMatrix(a, 1, icc->illuminant[0], icc->illuminant[1], icc->illuminant[2],
                             icc->white[0], icc->white[1], icc->white[2])) {
        return 1; /* error computing matrix */
      }
    }
    /* If the profile has a chad, then also the RGB's whitepoint must also be adapted from it (and the one
    given is normally D50). If it did not have a chad, then the whitepoint given is already the adapted one. */
    if(icc->has_chad) {
      mulMatrix(&white[0], &white[1], &white[2], a, icc->white[0], icc->white[1], icc->white[2]);
    } else {
      for(i = 0; i < 3; i++) white[i] = icc->white[i];
    }

    mulMatrix(&red[0], &red[1], &red[2], a, icc->red[0], icc->red[1], icc->red[2]);
    mulMatrix(&green[0], &green[1], &green[2], a, icc->green[0], icc->green[1], icc->green[2]);
    mulMatrix(&blue[0], &blue[1], &blue[2], a, icc->blue[0], icc->blue[1], icc->blue[2]);

    if(getChrmMatrixXYZ(m, white[0], white[1], white[2], red[0], red[1], red[2],
                        green[0], green[1], green[2], blue[0], blue[1], blue[2])) {
      return 1; /* error computing matrix */
    }
    /* output absolute whitepoint of the original RGB model */
    whitepoint[0] = white[0];
    whitepoint[1] = white[1];
    whitepoint[2] = white[2];
  } else {
    /* output the unity matrix, for doing no transform */
    m[0] = m[4] = m[8] = 1;
    m[1] = m[2] = m[3] = m[5] = m[6] = m[7] = 0;
    /* grayscale, don't do anything. That means we are implicitely using equal energy whitepoint "E", indicate
    this to the output. */
    whitepoint[0] = whitepoint[1] = whitepoint[2] = 1;
  }
  return 0; /* success */
}

/* Outputs whitepoint and matrix to go from the icc or info profile (depending on what was in the PNG) to XYZ,
without applying any (rendering intent related) whitepoint adaptation */
static unsigned getChrm(float m[9], float whitepoint[3], unsigned use_icc,
                        const LodePNGICC* icc, const LodePNGInfo* info) {
  size_t i;
  if(use_icc) {
    if(getICCChrm(m, whitepoint, icc)) return 1;  /* error in the matrix computations */
  } else if(info->chrm_defined && !info->srgb_defined) {
    float wx = info->chrm_white_x / 100000.0f, wy = info->chrm_white_y / 100000.0f;
    float rx = info->chrm_red_x / 100000.0f, ry = info->chrm_red_y / 100000.0f;
    float gx = info->chrm_green_x / 100000.0f, gy = info->chrm_green_y / 100000.0f;
    float bx = info->chrm_blue_x / 100000.0f, by = info->chrm_blue_y / 100000.0f;
    if(getChrmMatrixXY(m, wx, wy, rx, ry, gx, gy, bx, by)) return 1; /* returns if error */
    /* Output whitepoint, xyY to XYZ: */
    whitepoint[0] = wx / wy;
    whitepoint[1] = 1;
    whitepoint[2] = (1 - wx - wy) / wy;
  } else {
    /* the standard linear sRGB to XYZ matrix */
    static const float srgb[9] = {
        0.4124564f, 0.3575761f, 0.1804375f,
        0.2126729f, 0.7151522f, 0.0721750f,
        0.0193339f, 0.1191920f, 0.9503041f
    };
    for(i = 0; i < 9; i++) m[i] = srgb[i];
    /* sRGB's whitepoint xyY "0.3127,0.3290,1" in XYZ: */
    whitepoint[0] = 0.9504559270516716f;
    whitepoint[1] = 1;
    whitepoint[2] = 1.0890577507598784f;
  }
  return 0;
}

/* Returns whether the color chunks in info represent the default PNG sRGB,
which is when either no colorometry fields are present at all, or an srgb
field or chrm/gama field with default values are present.
ICC chunks representing sRGB are currently considered not the same. */
static unsigned isSRGB(const LodePNGInfo* info) {
  if(!info) return 1; /* the default is considered sRGB. */

  /* TODO: support some ICC profiles that represent sRGB too. Tricky due to
  possible slight deviations and many ways of representing its gamma function. */
  if(info->iccp_defined) return 0;

  if(info->srgb_defined) return 1;

  /* The gamma chunk is unable to represent sRGB's two-part gamma, so cannot
  be sRGB, even if it's the default 45455. */
  if(info->gama_defined) return 0;

  if(info->chrm_defined) {
    if(info->chrm_white_x != 31270 || info->chrm_white_y != 32900) return 0;
    if(info->chrm_red_x != 64000 || info->chrm_red_y != 33000) return 0;
    if(info->chrm_green_x != 30000 || info->chrm_green_y != 60000) return 0;
    if(info->chrm_blue_x != 15000 || info->chrm_blue_y != 6000) return 0;
  }

  return 1;
}

/* Checks whether the RGB models are equal (chromaticities, ...). The raw byte
format is allowed to be different. Input pointers are allowed to be null,
they then represent the default PNG sRGB (same as having no color model
chunks at all or an srgb chunk in the PNG) */
static unsigned modelsEqual(const LodePNGState* state_a,
                            const LodePNGState* state_b) {
  size_t i;
  const LodePNGInfo* a = state_a ? &state_a->info_png : 0;
  const LodePNGInfo* b = state_b ? &state_b->info_png : 0;
  if(isSRGB(a) != isSRGB(b)) return 0;
  /* now a and b are guaranteed to be non-NULL */
  if(a->iccp_defined != b->iccp_defined) return 0;
  if(a->iccp_defined) {
    if(a->iccp_profile_size != b->iccp_profile_size) return 0;
    /* TODO: return equal in more cases, such as when two ICC profiles that are
    not byte-for-byte equal, but represent the same color model. */
    for(i = 0; i < a->iccp_profile_size; i++) {
      if(a->iccp_profile[i] != b->iccp_profile[i]) return 0;
    }
    /* since the ICC model overrides gamma and chrm, those can be ignored. */
    /* TODO: this doesn't cover the case where the ICC profile is invalid */
    return 1;
  }

  if(a->srgb_defined != b->srgb_defined) return 0;
  if(a->srgb_defined) {
    /* since the sRGB model overrides gamma and chrm, those can be ignored.
    srgb_intent not checked since the conversion ignores it */
    return 1;
  }

  if(a->gama_defined != b->gama_defined) return 0;
  if(a->gama_defined) {
    if(a->gama_gamma != b->gama_gamma) return 0;
  }

  if(a->chrm_defined != b->chrm_defined) return 0;
  if(a->chrm_defined) {
    if(a->chrm_white_x != b->chrm_white_x) return 0;
    if(a->chrm_white_y != b->chrm_white_y) return 0;
    if(a->chrm_red_x != b->chrm_red_x) return 0;
    if(a->chrm_red_y != b->chrm_red_y) return 0;
    if(a->chrm_green_x != b->chrm_green_x) return 0;
    if(a->chrm_green_y != b->chrm_green_y) return 0;
    if(a->chrm_blue_x != b->chrm_blue_x) return 0;
    if(a->chrm_blue_y != b->chrm_blue_y) return 0;
  }

  return 1;
}

/* Converts in-place. Does not clamp. Do not use for integer input, make table instead there. */
static void convertToXYZ_gamma(float* out, const float* in, unsigned w, unsigned h,
                               const LodePNGInfo* info, unsigned use_icc, const LodePNGICC* icc) {
  size_t i, c;
  size_t n = w * h;
  for(i = 0; i < n * 4; i++) {
    out[i] = in[i];
  }
  if(use_icc) {
    for(i = 0; i < n; i++) {
      for(c = 0; c < 3; c++) {
        /* TODO: this is likely very slow */
        out[i * 4 + c] = iccForwardTRC(&icc->trc[c], in[i * 4 + c]);
      }
    }
  } else if(info->gama_defined && !info->srgb_defined) {
    /* nothing to do if gamma is 1 */
    if(info->gama_gamma != 100000) {
      float gamma = 100000.0f / info->gama_gamma;
      for(i = 0; i < n; i++) {
        for(c = 0; c < 3; c++) {
          float v = in[i * 4 + c];
          out[i * 4 + c] = (v <= 0) ? v : lodepng_powf(v, gamma);
        }
      }
    }
  } else {
    for(i = 0; i < n; i++) {
      for(c = 0; c < 3; c++) {
        /* sRGB gamma expand */
        float v = in[i * 4 + c];
        out[i * 4 + c] = (v < 0.04045f) ? (v / 12.92f) : lodepng_powf((v + 0.055f) / 1.055f, 2.4f);
      }
    }
  }
}

/* Same as convertToXYZ_gamma, but creates a lookup table rather than operating on an image */
static void convertToXYZ_gamma_table(float* out, size_t n, size_t c,
                                     const LodePNGInfo* info, unsigned use_icc, const LodePNGICC* icc) {
  size_t i;
  float mul = 1.0f / (n - 1);
  if(use_icc) {
    for(i = 0; i < n; i++) {
      float v = i * mul;
      out[i] = iccForwardTRC(&icc->trc[c], v);
    }
  } else if(info->gama_defined && !info->srgb_defined) {
    /* no power needed if gamma is 1 */
    if(info->gama_gamma == 100000) {
      for(i = 0; i < n; i++) {
        out[i] = i * mul;
      }
    } else {
      float gamma = 100000.0f / info->gama_gamma;
      for(i = 0; i < n; i++) {
        float v = i * mul;
        out[i] = lodepng_powf(v, gamma);
      }
    }
  } else {
    for(i = 0; i < n; i++) {
      /* sRGB gamma expand */
      float v = i * mul;
      out[i] = (v < 0.04045f) ? (v / 12.92f) : lodepng_powf((v + 0.055f) / 1.055f, 2.4f);
    }
  }
}

/* In-place */
static unsigned convertToXYZ_chrm(float* im, unsigned w, unsigned h,
                                  const LodePNGInfo* info, unsigned use_icc, const LodePNGICC* icc,
                                  float whitepoint[3]) {
  unsigned error = 0;
  size_t i;
  size_t n = w * h;
  float m[9]; /* XYZ to linear RGB matrix */

  /* Must be called even for grayscale, to get the correct whitepoint to output */
  error = getChrm(m, whitepoint, use_icc, icc, info);
  if(error) return error;

  /* Note: no whitepoint adaptation done to m here, because we only do the
  adaptation in convertFromXYZ (we only whitepoint adapt when going to the
  target RGB space, but here we're going from the source RGB space to XYZ) */

  /* Apply the above computed linear-RGB-to-XYZ matrix to the pixels.
  Skip the transform if it's the unit matrix (which is the case if grayscale profile) */
  if(!use_icc || icc->inputspace == 2) {
    for(i = 0; i < n; i++) {
      size_t j = i * 4;
      mulMatrix(&im[j + 0], &im[j + 1], &im[j + 2], m, im[j + 0], im[j + 1], im[j + 2]);
    }
  }

  return 0;
}

unsigned convertToXYZ(float* out, float whitepoint[3], const unsigned char* in,
                      unsigned w, unsigned h, const LodePNGState* state) {
  unsigned error = 0;
  size_t i;
  size_t n = w * h;
  const LodePNGColorMode* mode_in = &state->info_raw;
  const LodePNGInfo* info = &state->info_png;
  unsigned char* data = 0;
  float* gammatable = 0;
  int bit16 = mode_in->bitdepth > 8;
  size_t num = bit16 ? 65536 : 256;
  LodePNGColorMode tempmode = lodepng_color_mode_make(LCT_RGBA, bit16 ? 16 : 8);


  unsigned use_icc = 0;
  LodePNGICC icc;
  lodepng_icc_init(&icc);
  if(info->iccp_defined) {
    error = parseICC(&icc, info->iccp_profile, info->iccp_profile_size);
    if(error) goto cleanup; /* corrupted ICC profile */
    use_icc = validateICC(&icc);
  }

  data = (unsigned char*)lodepng_malloc((size_t)w * (size_t)h * (bit16 ? 8 : 4));
  error = lodepng_convert(data, in, &tempmode, mode_in, w, h);
  if(error) goto cleanup;

  /* Handle transfer function */
  {
    float* gammatable_r;
    float* gammatable_g;
    float* gammatable_b;

    /* RGB ICC, can have three different transfer functions */
    if(use_icc && icc.inputspace == 2) {
      gammatable = (float*)lodepng_malloc(num * 3 * sizeof(float));
      gammatable_r = &gammatable[num * 0];
      gammatable_g = &gammatable[num * 1];
      gammatable_b = &gammatable[num * 2];
      convertToXYZ_gamma_table(gammatable_r, num, 0, info, use_icc, &icc);
      convertToXYZ_gamma_table(gammatable_g, num, 1, info, use_icc, &icc);
      convertToXYZ_gamma_table(gammatable_b, num, 2, info, use_icc, &icc);
    } else {
      gammatable = (float*)lodepng_malloc(num * sizeof(float));
      gammatable_r = gammatable_g = gammatable_b = gammatable;
      convertToXYZ_gamma_table(gammatable, num, 0, info, use_icc, &icc);
    }

    if(bit16) {
      for(i = 0; i < n; i++) {
        out[i * 4 + 0] = gammatable_r[data[i * 8 + 0] * 256u + data[i * 8 + 1]];
        out[i * 4 + 1] = gammatable_g[data[i * 8 + 2] * 256u + data[i * 8 + 3]];
        out[i * 4 + 2] = gammatable_b[data[i * 8 + 4] * 256u + data[i * 8 + 5]];
        out[i * 4 + 3] = (data[i * 8 + 6] * 256 + data[i * 8 + 7]) * (1 / 65535.0f);
      }
    } else {
      for(i = 0; i < n; i++) {
        out[i * 4 + 0] = gammatable_r[data[i * 4 + 0]];
        out[i * 4 + 1] = gammatable_g[data[i * 4 + 1]];
        out[i * 4 + 2] = gammatable_b[data[i * 4 + 2]];
        out[i * 4 + 3] = data[i * 4 + 3] * (1 / 255.0f);
      }
    }
  }

  convertToXYZ_chrm(out, w, h, info, use_icc, &icc, whitepoint);

cleanup:
  lodepng_icc_cleanup(&icc);
  lodepng_free(data);
  lodepng_free(gammatable);
  return error;
}

unsigned convertToXYZFloat(float* out, float whitepoint[3], const float* in,
                           unsigned w, unsigned h, const LodePNGState* state) {
  unsigned error = 0;
  const LodePNGInfo* info = &state->info_png;

  unsigned use_icc = 0;
  LodePNGICC icc;
  lodepng_icc_init(&icc);
  if(info->iccp_defined) {
    error = parseICC(&icc, info->iccp_profile, info->iccp_profile_size);
    if(error) goto cleanup; /* corrupted ICC profile */
    use_icc = validateICC(&icc);
  }
  /* Input is floating point, so lookup table cannot be used, but it's ensured to
  use float pow, not the slower double pow. */
  convertToXYZ_gamma(out, in, w, h, info, use_icc, &icc);
  convertToXYZ_chrm(out, w, h, info, use_icc, &icc, whitepoint);

cleanup:
  lodepng_icc_cleanup(&icc);
  return error;
}

static unsigned convertFromXYZ_chrm(float* out, const float* in, unsigned w, unsigned h,
                                    const LodePNGInfo* info, unsigned use_icc, const LodePNGICC* icc,
                                    const float whitepoint[3], unsigned rendering_intent) {
  size_t i;
  size_t n = w * h;

  float m[9]; /* XYZ to linear RGB matrix */
  float white[3]; /* The whitepoint (absolute) of the target RGB space */

  if(getChrm(m, white, use_icc, icc, info)) return 1;
  if(invMatrix(m)) return 1; /* error, not invertible */

  /* for relative rendering intent (any except absolute "3"), must whitepoint adapt to the original whitepoint.
  this also ensures grayscale stays grayscale (with absolute, grayscale could become e.g. blue or sepia) */
  if(rendering_intent != 3) {
    float a[9] = {1,0,0, 0,1,0, 0,0,1};
    /* "white" = absolute whitepoint of the new target RGB space, read from the target color profile.
    "whitepoint" is original absolute whitepoint (input as parameter of this function) of an
    RGB space the XYZ data once had before it was converted to XYZ, in other words the whitepoint that
    we want to adapt our current data to to make sure values that had equal R==G==B in the old space have
    the same property now (white stays white and gray stays gray).
    Note: "absolute" whitepoint above means, can be used as-is, not needing further adaptation itself like icc.white does.*/
    if(getAdaptationMatrix(a, 1, whitepoint[0], whitepoint[1], whitepoint[2], white[0], white[1], white[2])) {
      return 1;
    }
    /* multiply the from xyz matrix with the adaptation matrix: in total,
    the resulting matrix first adapts in XYZ space, then converts to RGB*/
    mulMatrixMatrix(m, m, a);
  }

  /* Apply the above computed XYZ-to-linear-RGB matrix to the pixels.
  This transformation also includes the whitepoint adaptation. The transform
  can be skipped only if it's the unit matrix (only if grayscale profile and no
  whitepoint adaptation, such as with rendering intent 3)*/
  if(!use_icc || icc->inputspace == 2 || rendering_intent != 3) {
    for(i = 0; i < n; i++) {
      size_t j = i * 4;
      mulMatrix(&out[j + 0], &out[j + 1], &out[j + 2], m, in[j + 0], in[j + 1], in[j + 2]);
      out[j + 3] = in[j + 3];
    }
  } else {
    for(i = 0; i < n * 4; i++) {
      out[i] = in[i];
    }
  }

  return 0;
}

/* Converts in-place. Does not clamp. */
static void convertFromXYZ_gamma(float* im, unsigned w, unsigned h,
                                 const LodePNGInfo* info, unsigned use_icc, const LodePNGICC* icc) {
  size_t i, c;
  size_t n = w * h;
  if(use_icc) {
    for(i = 0; i < n; i++) {
      for(c = 0; c < 3; c++) {
        /* TODO: this is likely very slow */
        im[i * 4 + c] = iccBackwardTRC(&icc->trc[c], im[i * 4 + c]);
      }
    }
  } else if(info->gama_defined && !info->srgb_defined) {
    /* nothing to do if gamma is 1 */
    if(info->gama_gamma != 100000) {
      float gamma = info->gama_gamma / 100000.0f;
      for(i = 0; i < n; i++) {
        for(c = 0; c < 3; c++) {
          if(im[i * 4 + c] > 0) im[i * 4 + c] = lodepng_powf(im[i * 4 + c], gamma);
        }
      }
    }
  } else {
    for(i = 0; i < n; i++) {
      for(c = 0; c < 3; c++) {
        /* sRGB gamma compress */
        float* v = &im[i * 4 + c];
        *v = (*v < 0.0031308f) ? (*v * 12.92f) : (1.055f * lodepng_powf(*v, 1 / 2.4f) - 0.055f);
      }
    }
  }
}

unsigned convertFromXYZ(unsigned char* out, const float* in, unsigned w, unsigned h,
                        const LodePNGState* state,
                        const float whitepoint[3], unsigned rendering_intent) {
  unsigned error = 0;
  size_t i, c;
  size_t n = w * h;
  const LodePNGColorMode* mode_out = &state->info_raw;
  const LodePNGInfo* info = &state->info_png;
  int bit16 = mode_out->bitdepth > 8;
  float* im = 0;
  unsigned char* data = 0;

  /* parse ICC if present */
  unsigned use_icc = 0;
  LodePNGICC icc;
  lodepng_icc_init(&icc);
  if(info->iccp_defined) {
    error = parseICC(&icc, info->iccp_profile, info->iccp_profile_size);
    if(error) goto cleanup; /* corrupted ICC profile */
    use_icc = validateICC(&icc);
  }

  /* Handle gamut */
  im = (float*)lodepng_malloc(w * h * 4 * sizeof(float));
  error = convertFromXYZ_chrm(im, in, w, h, info, use_icc, &icc, whitepoint, rendering_intent);
  if(error) goto cleanup;

  /* Handle transfer function */
  /* Input is floating point, so lookup table cannot be used, but it's ensured to use float pow, not the slower double pow. */
  convertFromXYZ_gamma(im, w, h, info, use_icc, &icc);

  /* Convert to integer output */
  data = (unsigned char*)lodepng_malloc(w * h * 8);
  /* TODO: check if also 1/2/4 bit case needed: rounding is at different fine-grainedness for 8 and 16 bits below. */
  if(bit16) {
    LodePNGColorMode mode16 = lodepng_color_mode_make(LCT_RGBA, 16);
    for(i = 0; i < n; i++) {
      for(c = 0; c < 4; c++) {
        size_t j = i * 8 + c * 2;
        int i16 = (int)(0.5f + 65535.0f * LODEPNG_MIN(LODEPNG_MAX(0.0f, im[i * 4 + c]), 1.0f));
        data[j + 0] = i16 >> 8;
        data[j + 1] = i16 & 255;
      }
    }
    error = lodepng_convert(out, data, mode_out, &mode16, w, h);
    if(error) goto cleanup;
  } else {
    LodePNGColorMode mode8 = lodepng_color_mode_make(LCT_RGBA, 8);
    for(i = 0; i < n; i++) {
      for(c = 0; c < 4; c++) {
        int i8 = (int)(0.5f + 255.0f * LODEPNG_MIN(LODEPNG_MAX(0.0f, im[i * 4 + c]), 1.0f));
        data[i * 4 + c] = i8;
      }
    }
    error = lodepng_convert(out, data, mode_out, &mode8, w, h);
    if(error) goto cleanup;
  }

cleanup:
  lodepng_icc_cleanup(&icc);
  lodepng_free(im);
  lodepng_free(data);
  return error;
}

unsigned convertFromXYZFloat(float* out, const float* in, unsigned w, unsigned h,
                             const LodePNGState* state,
                             const float whitepoint[3], unsigned rendering_intent) {
  unsigned error = 0;
  const LodePNGInfo* info = &state->info_png;

  /* parse ICC if present */
  unsigned use_icc = 0;
  LodePNGICC icc;
  lodepng_icc_init(&icc);
  if(info->iccp_defined) {
    error = parseICC(&icc, info->iccp_profile, info->iccp_profile_size);
    if(error) goto cleanup; /* corrupted ICC profile */
    use_icc = validateICC(&icc);
  }

  /* Handle gamut */
  error = convertFromXYZ_chrm(out, in, w, h, info, use_icc, &icc, whitepoint, rendering_intent);
  if(error) goto cleanup;

  /* Handle transfer function */
  convertFromXYZ_gamma(out, w, h, info, use_icc, &icc);

cleanup:
  lodepng_icc_cleanup(&icc);
  return error;
}

unsigned convertRGBModel(unsigned char* out, const unsigned char* in,
                         unsigned w, unsigned h,
                         const LodePNGState* state_out,
                         const LodePNGState* state_in,
                         unsigned rendering_intent) {
  if(modelsEqual(state_in, state_out)) {
    return lodepng_convert(out, in, &state_out->info_raw, &state_in->info_raw, w, h);
  } else {
    unsigned error = 0;
    float* xyz = (float*)lodepng_malloc(w * h * 4 * sizeof(float));
    float whitepoint[3];
    error = convertToXYZ(&xyz[0], whitepoint, in, w, h, state_in);
    if (!error) error = convertFromXYZ(out, &xyz[0], w, h, state_out, whitepoint, rendering_intent);
    lodepng_free(xyz);
    return error;
  }
}

unsigned convertToSrgb(unsigned char* out, const unsigned char* in,
                       unsigned w, unsigned h,
                       const LodePNGState* state_in) {
  LodePNGState srgb;
  lodepng_state_init(&srgb);
  lodepng_color_mode_copy(&srgb.info_raw, &state_in->info_raw);
  return convertRGBModel(out, in, w, h, &srgb, state_in, 1);
}

unsigned convertFromSrgb(unsigned char* out, const unsigned char* in,
                         unsigned w, unsigned h,
                         const LodePNGState* state_out) {
  LodePNGState srgb;
  lodepng_state_init(&srgb);
  lodepng_color_mode_copy(&srgb.info_raw, &state_out->info_raw);
  return convertRGBModel(out, in, w, h, state_out, &srgb, 1);
}

#endif /*LODEPNG_COMPILE_ANCILLARY_CHUNKS*/

////////////////////////////////////////////////////////////////////////////////



//This uses a stripped down version of picoPNG to extract detailed zlib information while decompressing.
static const unsigned long LENBASE[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const unsigned long LENEXTRA[29] = {0,0,0,0,0,0,0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,  4,  5,  5,  5,  5,  0};
static const unsigned long DISTBASE[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
static const unsigned long DISTEXTRA[30] = {0,0,0,0,1,1,2, 2, 3, 3, 4, 4, 5, 5,  6,  6,  7,  7,  8,  8,   9,   9,  10,  10,  11,  11,  12,   12,   13,   13};
static const unsigned long CLCL[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15}; //code length code lengths

struct ExtractZlib { // Zlib decompression and information extraction
  std::vector<ZlibBlockInfo>* zlibinfo;
  ExtractZlib(std::vector<ZlibBlockInfo>* info) : zlibinfo(info) {};
  int error;

  unsigned long readBitFromStream(size_t& bitp, const unsigned char* bits) {
    unsigned long result = (bits[bitp >> 3] >> (bitp & 0x7)) & 1;
    bitp++;
    return result;
  }

  unsigned long readBitsFromStream(size_t& bitp, const unsigned char* bits, size_t nbits) {
    unsigned long result = 0;
    for(size_t i = 0; i < nbits; i++) result += (readBitFromStream(bitp, bits)) << i;
    return result;
  }

  struct HuffmanTree {
    int makeFromLengths(const std::vector<unsigned long>& bitlen, unsigned long maxbitlen) { //make tree given the lengths
      unsigned long numcodes = (unsigned long)(bitlen.size()), treepos = 0, nodefilled = 0;
      std::vector<unsigned long> tree1d(numcodes), blcount(maxbitlen + 1, 0), nextcode(maxbitlen + 1, 0);
      //count number of instances of each code length
      for(unsigned long bits = 0; bits < numcodes; bits++) blcount[bitlen[bits]]++;
      for(unsigned long bits = 1; bits <= maxbitlen; bits++) {
        nextcode[bits] = (nextcode[bits - 1] + blcount[bits - 1]) << 1;
      }
      //generate all the codes
      for(unsigned long n = 0; n < numcodes; n++) if(bitlen[n] != 0) tree1d[n] = nextcode[bitlen[n]]++;
      tree2d.clear(); tree2d.resize(numcodes * 2, 32767); //32767 here means the tree2d isn't filled there yet
      for(unsigned long n = 0; n < numcodes; n++) //the codes
      for(unsigned long i = 0; i < bitlen[n]; i++) { //the bits for this code
        unsigned long bit = (tree1d[n] >> (bitlen[n] - i - 1)) & 1;
        if(treepos > numcodes - 2) return 55;
        if(tree2d[2 * treepos + bit] == 32767) { //not yet filled in
          if(i + 1 == bitlen[n]) {
            //last bit
            tree2d[2 * treepos + bit] = n;
            treepos = 0;
          } else {
            //addresses are encoded as values > numcodes
            tree2d[2 * treepos + bit] = ++nodefilled + numcodes;
            treepos = nodefilled;
          }
        }
        else treepos = tree2d[2 * treepos + bit] - numcodes; //subtract numcodes from address to get address value
      }
      return 0;
    }
    int decode(bool& decoded, unsigned long& result, size_t& treepos, unsigned long bit) const { //Decodes a symbol from the tree
      unsigned long numcodes = (unsigned long)tree2d.size() / 2;
      if(treepos >= numcodes) return 11; //error: you appeared outside the codetree
      result = tree2d[2 * treepos + bit];
      decoded = (result < numcodes);
      treepos = decoded ? 0 : result - numcodes;
      return 0;
    }
    //2D representation of a huffman tree: one dimension is "0" or "1", the other contains all nodes and leaves.
    std::vector<unsigned long> tree2d;
  };

  void inflate(std::vector<unsigned char>& out, const std::vector<unsigned char>& in, size_t inpos = 0) {
    size_t bp = 0, pos = 0; //bit pointer and byte pointer
    error = 0;
    unsigned long BFINAL = 0;
    while(!BFINAL && !error) {
      size_t uncomprblockstart = pos;
      size_t bpstart = bp;
      if(bp >> 3 >= in.size()) { error = 52; return; } //error, bit pointer will jump past memory
      BFINAL = readBitFromStream(bp, &in[inpos]);
      unsigned long BTYPE = readBitFromStream(bp, &in[inpos]); BTYPE += 2 * readBitFromStream(bp, &in[inpos]);
      zlibinfo->resize(zlibinfo->size() + 1);
      zlibinfo->back().btype = BTYPE;
      if(BTYPE == 3) { error = 20; return; } //error: invalid BTYPE
      else if(BTYPE == 0) inflateNoCompression(out, &in[inpos], bp, pos, in.size());
      else inflateHuffmanBlock(out, &in[inpos], bp, pos, in.size(), BTYPE);
      size_t uncomprblocksize = pos - uncomprblockstart;
      zlibinfo->back().compressedbits = bp - bpstart;
      zlibinfo->back().uncompressedbytes = uncomprblocksize;
    }
  }

  void generateFixedTrees(HuffmanTree& tree, HuffmanTree& treeD) { //get the tree of a deflated block with fixed tree
    std::vector<unsigned long> bitlen(288, 8), bitlenD(32, 5);;
    for(size_t i = 144; i <= 255; i++) bitlen[i] = 9;
    for(size_t i = 256; i <= 279; i++) bitlen[i] = 7;
    tree.makeFromLengths(bitlen, 15);
    treeD.makeFromLengths(bitlenD, 15);
  }

  //the code tree for Huffman codes, dist codes, and code length codes
  HuffmanTree codetree, codetreeD, codelengthcodetree;
  unsigned long huffmanDecodeSymbol(const unsigned char* in, size_t& bp, const HuffmanTree& tree, size_t inlength) {
    //decode a single symbol from given list of bits with given code tree. return value is the symbol
    bool decoded; unsigned long ct;
    for(size_t treepos = 0;;) {
      if((bp & 0x07) == 0 && (bp >> 3) > inlength) { error = 10; return 0; } //error: end reached without endcode
      error = tree.decode(decoded, ct, treepos, readBitFromStream(bp, in));
      if(error) return 0; //stop, an error happened
      if(decoded) return ct;
    }
  }

  void getTreeInflateDynamic(HuffmanTree& tree, HuffmanTree& treeD,
                             const unsigned char* in, size_t& bp, size_t inlength) {
    size_t bpstart = bp;
    //get the tree of a deflated block with dynamic tree, the tree itself is also Huffman compressed with a known tree
    std::vector<unsigned long> bitlen(288, 0), bitlenD(32, 0);
    if(bp >> 3 >= inlength - 2) { error = 49; return; } //the bit pointer is or will go past the memory
    size_t HLIT =  readBitsFromStream(bp, in, 5) + 257; //number of literal/length codes + 257
    size_t HDIST = readBitsFromStream(bp, in, 5) + 1; //number of dist codes + 1
    size_t HCLEN = readBitsFromStream(bp, in, 4) + 4; //number of code length codes + 4
    zlibinfo->back().hlit = HLIT - 257;
    zlibinfo->back().hdist = HDIST - 1;
    zlibinfo->back().hclen = HCLEN - 4;
    std::vector<unsigned long> codelengthcode(19); //lengths of tree to decode the lengths of the dynamic tree
    for(size_t i = 0; i < 19; i++) codelengthcode[CLCL[i]] = (i < HCLEN) ? readBitsFromStream(bp, in, 3) : 0;
    //code length code lengths
    for(size_t i = 0; i < codelengthcode.size(); i++) zlibinfo->back().clcl.push_back(codelengthcode[i]);
    error = codelengthcodetree.makeFromLengths(codelengthcode, 7); if(error) return;
    size_t i = 0, replength;
    while(i < HLIT + HDIST) {
      unsigned long code = huffmanDecodeSymbol(in, bp, codelengthcodetree, inlength); if(error) return;
      zlibinfo->back().treecodes.push_back(code); //tree symbol code
      if(code <= 15)  { if(i < HLIT) bitlen[i++] = code; else bitlenD[i++ - HLIT] = code; } //a length code
      else if(code == 16) { //repeat previous
        if(bp >> 3 >= inlength) { error = 50; return; } //error, bit pointer jumps past memory
        replength = 3 + readBitsFromStream(bp, in, 2);
        unsigned long value; //set value to the previous code
        if((i - 1) < HLIT) value = bitlen[i - 1];
        else value = bitlenD[i - HLIT - 1];
        for(size_t n = 0; n < replength; n++) { //repeat this value in the next lengths
          if(i >= HLIT + HDIST) { error = 13; return; } //error: i is larger than the amount of codes
          if(i < HLIT) bitlen[i++] = value; else bitlenD[i++ - HLIT] = value;
        }
      } else if(code == 17) { //repeat "0" 3-10 times
        if(bp >> 3 >= inlength) { error = 50; return; } //error, bit pointer jumps past memory
        replength = 3 + readBitsFromStream(bp, in, 3);
        zlibinfo->back().treecodes.push_back(replength); //tree symbol code repetitions
        for(size_t n = 0; n < replength; n++) { //repeat this value in the next lengths
          if(i >= HLIT + HDIST) { error = 14; return; } //error: i is larger than the amount of codes
          if(i < HLIT) bitlen[i++] = 0; else bitlenD[i++ - HLIT] = 0;
        }
      } else if(code == 18) { //repeat "0" 11-138 times
        if(bp >> 3 >= inlength) { error = 50; return; } //error, bit pointer jumps past memory
        replength = 11 + readBitsFromStream(bp, in, 7);
        zlibinfo->back().treecodes.push_back(replength); //tree symbol code repetitions
        for(size_t n = 0; n < replength; n++) { //repeat this value in the next lengths
          if(i >= HLIT + HDIST) { error = 15; return; } //error: i is larger than the amount of codes
          if(i < HLIT) bitlen[i++] = 0; else bitlenD[i++ - HLIT] = 0;
        }
      }
      else { error = 16; return; } //error: somehow an unexisting code appeared. This can never happen.
    }
    if(bitlen[256] == 0) { error = 64; return; } //the length of the end code 256 must be larger than 0
    error = tree.makeFromLengths(bitlen, 15);
    if(error) return; //now we've finally got HLIT and HDIST, so generate the code trees, and the function is done
    error = treeD.makeFromLengths(bitlenD, 15);
    if(error) return;
    zlibinfo->back().treebits = bp - bpstart;
    //lit/len/end symbol lengths
    for(size_t j = 0; j < bitlen.size(); j++) zlibinfo->back().litlenlengths.push_back(bitlen[j]);
    //dist lengths
    for(size_t j = 0; j < bitlenD.size(); j++) zlibinfo->back().distlengths.push_back(bitlenD[j]);
  }

  void inflateHuffmanBlock(std::vector<unsigned char>& out,
                           const unsigned char* in, size_t& bp, size_t& pos, size_t inlength, unsigned long btype) {
    size_t numcodes = 0, numlit = 0, numlen = 0; //for logging
    if(btype == 1) { generateFixedTrees(codetree, codetreeD); }
    else if(btype == 2) { getTreeInflateDynamic(codetree, codetreeD, in, bp, inlength); if(error) return; }
    for(;;) {
      unsigned long code = huffmanDecodeSymbol(in, bp, codetree, inlength); if(error) return;
      numcodes++;
      zlibinfo->back().lz77_lcode.push_back(code); //output code
      zlibinfo->back().lz77_dcode.push_back(0);
      zlibinfo->back().lz77_lbits.push_back(0);
      zlibinfo->back().lz77_dbits.push_back(0);
      zlibinfo->back().lz77_lvalue.push_back(0);
      zlibinfo->back().lz77_dvalue.push_back(0);

      if(code == 256) {
        break; //end code
      } else if(code <= 255) { //literal symbol
        out.push_back((unsigned char)(code));
        pos++;
        numlit++;
      } else if(code >= 257 && code <= 285) { //length code
        size_t length = LENBASE[code - 257], numextrabits = LENEXTRA[code - 257];
        if((bp >> 3) >= inlength) { error = 51; return; } //error, bit pointer will jump past memory
        length += readBitsFromStream(bp, in, numextrabits);
        unsigned long codeD = huffmanDecodeSymbol(in, bp, codetreeD, inlength); if(error) return;
        if(codeD > 29) { error = 18; return; } //error: invalid dist code (30-31 are never used)
        unsigned long dist = DISTBASE[codeD], numextrabitsD = DISTEXTRA[codeD];
        if((bp >> 3) >= inlength) { error = 51; return; } //error, bit pointer will jump past memory
        dist += readBitsFromStream(bp, in, numextrabitsD);
        size_t start = pos, back = start - dist; //backwards
        for(size_t i = 0; i < length; i++) {
          out.push_back(out[back++]);
          pos++;
          if(back >= start) back = start - dist;
        }
        numlen++;
        zlibinfo->back().lz77_dcode.back() = codeD; //output distance code
        zlibinfo->back().lz77_lbits.back() = numextrabits; //output length extra bits
        zlibinfo->back().lz77_dbits.back() = numextrabitsD; //output dist extra bits
        zlibinfo->back().lz77_lvalue.back() = length; //output length
        zlibinfo->back().lz77_dvalue.back() = dist; //output dist
      }
    }
    zlibinfo->back().numlit = numlit; //output number of literal symbols
    zlibinfo->back().numlen = numlen; //output number of length symbols
  }

  void inflateNoCompression(std::vector<unsigned char>& out,
                            const unsigned char* in, size_t& bp, size_t& pos, size_t inlength) {
    while((bp & 0x7) != 0) bp++; //go to first boundary of byte
    size_t p = bp / 8;
    if(p >= inlength - 4) { error = 52; return; } //error, bit pointer will jump past memory
    unsigned long LEN = in[p] + 256u * in[p + 1], NLEN = in[p + 2] + 256u * in[p + 3]; p += 4;
    if(LEN + NLEN != 65535) { error = 21; return; } //error: NLEN is not one's complement of LEN
    if(p + LEN > inlength) { error = 23; return; } //error: reading outside of in buffer
    for(unsigned long n = 0; n < LEN; n++) {
      out.push_back(in[p++]); //read LEN bytes of literal data
      pos++;
    }
    bp = p * 8;
  }

  int decompress(std::vector<unsigned char>& out, const std::vector<unsigned char>& in) { //returns error value
    if(in.size() < 2) { return 53; } //error, size of zlib data too small
    //error: 256 * in[0] + in[1] must be a multiple of 31, the FCHECK value is supposed to be made that way
    if((in[0] * 256 + in[1]) % 31 != 0) { return 24; }
    unsigned long CM = in[0] & 15, CINFO = (in[0] >> 4) & 15, FDICT = (in[1] >> 5) & 1;
    //error: only compression method 8: inflate with sliding window of 32k is supported by the PNG spec
    if(CM != 8 || CINFO > 7) { return 25; }
    //error: the PNG spec says about the zlib stream: "The additional flags shall not specify a preset dictionary."
    if(FDICT != 0) { return 26; }
    inflate(out, in, 2);
    return error; //note: adler32 checksum was skipped and ignored
  }
};

struct ExtractPNG { //PNG decoding and information extraction
  std::vector<ZlibBlockInfo>* zlibinfo;
  ExtractPNG(std::vector<ZlibBlockInfo>* info) : zlibinfo(info) {};
  int error;
  void decode(const unsigned char* in, size_t size) {
    error = 0;
    if(size == 0 || in == 0) { error = 48; return; } //the given data is empty
    readPngHeader(&in[0], size); if(error) return;
    size_t pos = 33; //first byte of the first chunk after the header
    std::vector<unsigned char> idat; //the data from idat chunks
    bool IEND = false;
    //loop through the chunks, ignoring unknown chunks and stopping at IEND chunk.
    //IDAT data is put at the start of the in buffer
    while(!IEND) {
      //error: size of the in buffer too small to contain next chunk
      if(pos + 8 >= size) { error = 30; return; }
      size_t chunkLength = read32bitInt(&in[pos]); pos += 4;
      if(chunkLength > 2147483647) { error = 63; return; }
      //error: size of the in buffer too small to contain next chunk
      if(pos + chunkLength >= size) { error = 35; return; }
      //IDAT chunk, containing compressed image data
      if(in[pos + 0] == 'I' && in[pos + 1] == 'D' && in[pos + 2] == 'A' && in[pos + 3] == 'T') {
        idat.insert(idat.end(), &in[pos + 4], &in[pos + 4 + chunkLength]);
        pos += (4 + chunkLength);
      } else if(in[pos + 0] == 'I' && in[pos + 1] == 'E' && in[pos + 2] == 'N' && in[pos + 3] == 'D') {
          pos += 4;
          IEND = true;
      } else { //it's not an implemented chunk type, so ignore it: skip over the data
        pos += (chunkLength + 4); //skip 4 letters and uninterpreted data of unimplemented chunk
      }
      pos += 4; //step over CRC (which is ignored)
    }
    std::vector<unsigned char> out; //now the out buffer will be filled
    ExtractZlib zlib(zlibinfo); //decompress with the Zlib decompressor
    error = zlib.decompress(out, idat);
    if(error) return; //stop if the zlib decompressor returned an error
  }

  //read the information from the header and store it in the Info
  void readPngHeader(const unsigned char* in, size_t inlength) {
    if(inlength < 29) { error = 27; return; } //error: the data length is smaller than the length of the header
    if(in[0] != 137 || in[1] != 80 || in[2] != 78 || in[3] != 71
    || in[4] != 13 || in[5] != 10 || in[6] != 26 || in[7] != 10) { error = 28; return; } //no PNG signature
    //error: it doesn't start with a IHDR chunk!
    if(in[12] != 'I' || in[13] != 'H' || in[14] != 'D' || in[15] != 'R') { error = 29; return; }
  }

  unsigned long readBitFromReversedStream(size_t& bitp, const unsigned char* bits) {
    unsigned long result = (bits[bitp >> 3] >> (7 - (bitp & 0x7))) & 1;
    bitp++;
    return result;
  }

  unsigned long readBitsFromReversedStream(size_t& bitp, const unsigned char* bits, unsigned long nbits) {
    unsigned long result = 0;
    for(size_t i = nbits - 1; i < nbits; i--) result += ((readBitFromReversedStream(bitp, bits)) << i);
    return result;
  }

  void setBitOfReversedStream(size_t& bitp, unsigned char* bits, unsigned long bit) {
    bits[bitp >> 3] |=  (bit << (7 - (bitp & 0x7))); bitp++;
  }

  unsigned long read32bitInt(const unsigned char* buffer) {
    return (unsigned int)((buffer[0] << 24u) | (buffer[1] << 16u) | (buffer[2] << 8u) | buffer[3]);
  }
};

void extractZlibInfo(std::vector<ZlibBlockInfo>& zlibinfo, const std::vector<unsigned char>& in) {
  ExtractPNG decoder(&zlibinfo);
  decoder.decode(&in[0], in.size());

  if(decoder.error) std::cout << "extract error: " << decoder.error << std::endl;
}

} // namespace lodepng
