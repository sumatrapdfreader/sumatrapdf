/*
 * Copyright (C) 2009-2014, 2017-2019, 2022-2026 D. R. Commander
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the libjpeg-turbo Project nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS",
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This program tests the various code paths in the TurboJPEG C API
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "tjutil.h"
#include "turbojpeg.h"
#include "md5/md5.h"
#include "jconfigint.h"
#ifdef _WIN32
#include <time.h>
#include <process.h>
#define random()  rand()
#define getpid()  _getpid()
#else
#include <unistd.h>
#endif


static void usage(char *progName)
{
  printf("\nUSAGE: %s [options]\n\n", progName);
  printf("Options:\n");
  printf("-yuv = test YUV encoding/compression/decompression/decoding\n");
  printf("       (8-bit data precision only)\n");
  printf("-noyuvpad = do not pad each row in each Y, U, and V plane to the nearest\n");
  printf("            multiple of 4 bytes\n");
  printf("-precision N = test N-bit data precision (N=2..16; default is 8; if N is not 8\n");
  printf("               or 12, then -lossless is implied)\n");
  printf("-lossless = test lossless JPEG compression/decompression\n");
  printf("-alloc = test automatic JPEG buffer allocation\n");
  printf("-bmp = test packed-pixel image I/O\n");
  exit(1);
}


#define THROW_TJ(handle) { \
  printf("TurboJPEG ERROR:\n%s\n", tj3GetErrorStr(handle)); \
  BAILOUT() \
}
#define TRY_TJ(handle, f) { if ((f) == -1) THROW_TJ(handle); }
#define THROW(m) { printf("ERROR: %s\n", m);  BAILOUT() }
#define THROW_MD5(filename, md5sum, ref) { \
  printf("\n%s has an MD5 sum of %s.\n   Should be %s.\n", filename, md5sum, \
         ref); \
  BAILOUT() \
}

static const char *subNameLong[TJ_NUMSAMP] = {
  "4:4:4", "4:2:2", "4:2:0", "GRAY", "4:4:0", "4:1:1", "4:4:1", "4:1:0", "2:4"
};
static const char *subName[TJ_NUMSAMP] = {
  "444", "422", "420", "GRAY", "440", "411", "441", "410", "24"
};

static const char *pixFormatStr[TJ_NUMPF] = {
  "RGB", "BGR", "RGBX", "BGRX", "XBGR", "XRGB", "Grayscale",
  "RGBA", "BGRA", "ABGR", "ARGB", "CMYK"
};

static const int _3sampleFormats[] = { TJPF_RGB, TJPF_BGR };
static const int _4sampleFormats[] = {
  TJPF_RGBX, TJPF_BGRX, TJPF_XBGR, TJPF_XRGB, TJPF_CMYK
};
static const int _onlyGray[] = { TJPF_GRAY };
static const int _onlyRGB[] = { TJPF_RGB };

static int doYUV = 0, lossless = 0, psv = 1, alloc = 0, yuvAlign = 4;
static int precision = 8, sampleSize, maxSample, tolerance, redToY, yellowToY;

static int exitStatus = 0;
#define BAILOUT() { exitStatus = -1;  goto bailout; }


static void setVal(void *buf, int index, int value)
{
  if (precision <= 8)
    ((unsigned char *)buf)[index] = (unsigned char)value;
  else if (precision <= 12)
    ((short *)buf)[index] = (short)value;
  else
    ((unsigned short *)buf)[index] = (unsigned short)value;
}

static void initBuf(void *buf, int w, int h, int pf, int bottomUp)
{
  int roffset = tjRedOffset[pf];
  int goffset = tjGreenOffset[pf];
  int boffset = tjBlueOffset[pf];
  int ps = tjPixelSize[pf];
  int i, index, row, col, halfway = 16;

  if (pf == TJPF_GRAY) {
    memset(buf, 0, w * h * ps * sampleSize);
    for (row = 0; row < h; row++) {
      for (col = 0; col < w; col++) {
        if (bottomUp) index = (h - row - 1) * w + col;
        else index = row * w + col;
        if (((row / 8) + (col / 8)) % 2 == 0)
          setVal(buf, index, (row < halfway) ? maxSample : 0);
        else setVal(buf, index, (row < halfway) ? redToY : yellowToY);
      }
    }
  } else if (pf == TJPF_CMYK) {
    for (i = 0; i < w * h * ps; i++)
      setVal(buf, i, maxSample);
    for (row = 0; row < h; row++) {
      for (col = 0; col < w; col++) {
        if (bottomUp) index = (h - row - 1) * w + col;
        else index = row * w + col;
        if (((row / 8) + (col / 8)) % 2 == 0) {
          if (row >= halfway) setVal(buf, index * ps + 3, 0);
        } else {
          setVal(buf, index * ps + 2, 0);
          if (row < halfway) setVal(buf, index * ps + 1, 0);
        }
      }
    }
  } else {
    memset(buf, 0, w * h * ps * sampleSize);
    for (row = 0; row < h; row++) {
      for (col = 0; col < w; col++) {
        if (bottomUp) index = (h - row - 1) * w + col;
        else index = row * w + col;
        if (((row / 8) + (col / 8)) % 2 == 0) {
          if (row < halfway) {
            setVal(buf, index * ps + roffset, maxSample);
            setVal(buf, index * ps + goffset, maxSample);
            setVal(buf, index * ps + boffset, maxSample);
          }
        } else {
          setVal(buf, index * ps + roffset, maxSample);
          if (row >= halfway) setVal(buf, index * ps + goffset, maxSample);
        }
      }
    }
  }
}


#define CHECKVAL(v, cv) { \
  if (v < cv - tolerance || v > cv + tolerance) { \
    printf("\nComp. %s at %d,%d should be %d, not %d\n", #v, row, col, cv, \
           v); \
    retval = 0;  exitStatus = -1;  goto bailout; \
  } \
}

#define CHECKVAL0(v) { \
  if (v > tolerance) { \
    printf("\nComp. %s at %d,%d should be 0, not %d\n", #v, row, col, v); \
    retval = 0;  exitStatus = -1;  goto bailout; \
  } \
}

#define CHECKVALMAX(v) { \
  if (v < maxSample - tolerance) { \
    printf("\nComp. %s at %d,%d should be %d, not %d\n", #v, row, col, \
           maxSample, v); \
    retval = 0;  exitStatus = -1;  goto bailout; \
  } \
}


static int getVal(void *buf, int index, int targetPrecision)
{
  if (targetPrecision <= 8)
    return ((unsigned char *)buf)[index];
  else if (targetPrecision <= 12)
    return ((short *)buf)[index];
  else
    return ((unsigned short *)buf)[index];
}

static int checkBuf(void *buf, int w, int h, int pf, int subsamp,
                    tjscalingfactor sf, int bottomUp)
{
  int roffset = tjRedOffset[pf];
  int goffset = tjGreenOffset[pf];
  int boffset = tjBlueOffset[pf];
  int aoffset = tjAlphaOffset[pf];
  int ps = tjPixelSize[pf];
  int index, row, col, retval = 1;
  int halfway = 16 * sf.num / sf.denom;
  int blocksize = 8 * sf.num / sf.denom;

  if (pf == TJPF_GRAY) roffset = goffset = boffset = 0;

  if (pf == TJPF_CMYK) {
    for (row = 0; row < h; row++) {
      for (col = 0; col < w; col++) {
        int c, m, y, k;

        if (bottomUp) index = (h - row - 1) * w + col;
        else index = row * w + col;
        c = getVal(buf, index * ps, precision);
        m = getVal(buf, index * ps + 1, precision);
        y = getVal(buf, index * ps + 2, precision);
        k = getVal(buf, index * ps + 3, precision);
        if (((row / blocksize) + (col / blocksize)) % 2 == 0) {
          CHECKVALMAX(c);  CHECKVALMAX(m);  CHECKVALMAX(y);
          if (row < halfway) CHECKVALMAX(k)
          else CHECKVAL0(k)
        } else {
          CHECKVALMAX(c);  CHECKVAL0(y);  CHECKVALMAX(k);
          if (row < halfway) CHECKVAL0(m)
          else CHECKVALMAX(m)
        }
      }
    }
    return 1;
  }

  for (row = 0; row < h; row++) {
    for (col = 0; col < w; col++) {
      int r, g, b, a;

      if (bottomUp) index = (h - row - 1) * w + col;
      else index = row * w + col;
      r = getVal(buf, index * ps + roffset, precision);
      g = getVal(buf, index * ps + goffset, precision);
      b = getVal(buf, index * ps + boffset, precision);
      a = aoffset >= 0 ? getVal(buf, index * ps + aoffset, precision) :
                         maxSample;
      if (((row / blocksize) + (col / blocksize)) % 2 == 0) {
        if (row < halfway) {
          CHECKVALMAX(r);  CHECKVALMAX(g);  CHECKVALMAX(b);
        } else {
          CHECKVAL0(r);  CHECKVAL0(g);  CHECKVAL0(b);
        }
      } else {
        if (subsamp == TJSAMP_GRAY) {
          if (row < halfway) {
            CHECKVAL(r, redToY);  CHECKVAL(g, redToY);  CHECKVAL(b, redToY);
          } else {
            CHECKVAL(r, yellowToY);  CHECKVAL(g, yellowToY);
            CHECKVAL(b, yellowToY);
          }
        } else {
          if (row < halfway) {
            CHECKVALMAX(r);  CHECKVAL0(g);  CHECKVAL0(b);
          } else {
            CHECKVALMAX(r);  CHECKVALMAX(g);  CHECKVAL0(b);
          }
        }
      }
      CHECKVALMAX(a);
    }
  }

bailout:
  if (retval == 0) {
    for (row = 0; row < h; row++) {
      for (col = 0; col < w; col++) {
        if (pf == TJPF_CMYK)
          printf("%.3d/%.3d/%.3d/%.3d ",
                 getVal(buf, (row * w + col) * ps, precision),
                 getVal(buf, (row * w + col) * ps + 1, precision),
                 getVal(buf, (row * w + col) * ps + 2, precision),
                 getVal(buf, (row * w + col) * ps + 3, precision));
        else
          printf("%.3d/%.3d/%.3d ",
                 getVal(buf, (row * w + col) * ps + roffset, precision),
                 getVal(buf, (row * w + col) * ps + goffset, precision),
                 getVal(buf, (row * w + col) * ps + boffset, precision));
      }
      printf("\n");
    }
  }
  return retval;
}


#define PAD(v, p)  ((v + (p) - 1) & (~((p) - 1)))

static int checkBufYUV(unsigned char *buf, int w, int h, int subsamp,
                       tjscalingfactor sf)
{
  int row, col;
  int hsf = tjMCUWidth[subsamp] / 8, vsf = tjMCUHeight[subsamp] / 8;
  int pw = PAD(w, hsf), ph = PAD(h, vsf);
  int cw = pw / hsf, ch = ph / vsf;
  int ypitch = PAD(pw, yuvAlign), uvpitch = PAD(cw, yuvAlign);
  int retval = 1;
  int halfway = 16 * sf.num / sf.denom;
  int blocksize = 8 * sf.num / sf.denom;

  for (row = 0; row < ph; row++) {
    for (col = 0; col < pw; col++) {
      unsigned char y = buf[ypitch * row + col];

      if (((row / blocksize) + (col / blocksize)) % 2 == 0) {
        if (row < halfway) CHECKVALMAX(y)
        else CHECKVAL0(y);
      } else {
        if (row < halfway) CHECKVAL(y, 76)
        else CHECKVAL(y, 225);
      }
    }
  }
  if (subsamp != TJSAMP_GRAY) {
    halfway = 16 / vsf * sf.num / sf.denom;

    for (row = 0; row < ch; row++) {
      for (col = 0; col < cw; col++) {
        unsigned char u = buf[ypitch * ph + (uvpitch * row + col)],
          v = buf[ypitch * ph + uvpitch * ch + (uvpitch * row + col)];

        if (((row * vsf / blocksize) + (col * hsf / blocksize)) % 2 == 0) {
          CHECKVAL(u, 128);  CHECKVAL(v, 128);
        } else {
          if (row < halfway) {
            CHECKVAL(u, 85);  CHECKVALMAX(v);
          } else {
            CHECKVAL0(u);  CHECKVAL(v, 149);
          }
        }
      }
    }
  }

bailout:
  if (retval == 0) {
    for (row = 0; row < ph; row++) {
      for (col = 0; col < pw; col++)
        printf("%.3d ", buf[ypitch * row + col]);
      printf("\n");
    }
    printf("\n");
    for (row = 0; row < ch; row++) {
      for (col = 0; col < cw; col++)
        printf("%.3d ", buf[ypitch * ph + (uvpitch * row + col)]);
      printf("\n");
    }
    printf("\n");
    for (row = 0; row < ch; row++) {
      for (col = 0; col < cw; col++)
        printf("%.3d ",
               buf[ypitch * ph + uvpitch * ch + (uvpitch * row + col)]);
      printf("\n");
    }
  }

  return retval;
}


static void writeJPEG(unsigned char *jpegBuf, size_t jpegSize, char *filename)
{
  FILE *file = fopen(filename, "wb");

  if (!file || fwrite(jpegBuf, jpegSize, 1, file) != 1) {
    printf("ERROR: Could not write to %s.\n%s\n", filename, strerror(errno));
    BAILOUT()
  }

bailout:
  if (file) fclose(file);
}


static void compTest(tjhandle handle, unsigned char **dstBuf, size_t *dstSize,
                     int w, int h, int pf, char *basename)
{
  char tempStr[1024];
  void *srcBuf = NULL;
  unsigned char *yuvBuf = NULL;
  const char *pfStr = pixFormatStr[pf];
  int bottomUp = tj3Get(handle, TJPARAM_BOTTOMUP);
  int subsamp = tj3Get(handle, TJPARAM_SUBSAMP);
  int jpegPSV = tj3Get(handle, TJPARAM_LOSSLESSPSV);
  int jpegQual = tj3Get(handle, TJPARAM_QUALITY);
  const char *buStrLong = bottomUp ? "Bottom-Up" : "Top-Down ";
  const char *buStr = bottomUp ? "BU" : "TD";
  tjhandle handle2 = NULL;

  if ((srcBuf = malloc(w * h * tjPixelSize[pf] * sampleSize)) == NULL)
      THROW("Memory allocation failure");
  initBuf(srcBuf, w, h, pf, bottomUp);

  if (*dstBuf && *dstSize > 0) memset(*dstBuf, 0, *dstSize);

  if (doYUV) {
    unsigned char *yuvPtr, *yuvPlanes[3] = { NULL, NULL, NULL };
    int i, yuvStrides[3] = { 0, 0, 0 }, success;
    size_t dstBufSize;
    MD5_CTX md5ctx;
    char *md5ref, md5refbuf[65], *md5sum, md5buf[65];
    size_t yuvSize = tj3YUVBufSize(w, yuvAlign, h, subsamp);

    if ((yuvBuf = (unsigned char *)malloc(yuvSize)) == NULL)
      THROW("Memory allocation failure");

    printf("%s %s -> YUV %s ... ", pfStr, buStrLong, subNameLong[subsamp]);

    if ((handle2 = tj3Init(TJINIT_COMPRESS)) == NULL)
      THROW_TJ(NULL);
    TRY_TJ(handle2, tj3Set(handle2, TJPARAM_BOTTOMUP, bottomUp));
    TRY_TJ(handle2, tj3Set(handle2, TJPARAM_SUBSAMP, subsamp));
    /* Verify that tj3EncodeYUV*8() ignores TJPARAM_LOSSLESS and
       TJPARAM_COLORSPACE. */
    TRY_TJ(handle2, tj3Set(handle2, TJPARAM_LOSSLESS, 1));
    TRY_TJ(handle2, tj3Set(handle2, TJPARAM_COLORSPACE, TJCS_RGB));

    memset(yuvBuf, 0, yuvSize);
    TRY_TJ(handle2, tj3EncodeYUV8(handle2, (unsigned char *)srcBuf, w, 0, h,
                                  pf, yuvBuf, yuvAlign));
    success = checkBufYUV(yuvBuf, w, h, subsamp, TJUNSCALED);

    /* Verify that tj3EncodeYUVPlanes8() produces the same results. */
    yuvPtr = yuvBuf;
    for (i = 0; i < (subsamp == TJSAMP_GRAY ? 1 : 3); i++) {
      int planeWidth, planeHeight;
      size_t planeSize;

      if ((planeWidth = tj3YUVPlaneWidth(i, w, subsamp)) == 0 ||
          (planeHeight = tj3YUVPlaneHeight(i, h, subsamp)) == 0)
        THROW_TJ(NULL);
      yuvStrides[i] = PAD(planeWidth, yuvAlign);
      if ((planeSize = tj3YUVPlaneSize(i, w, yuvStrides[i], h, subsamp)) == 0)
        THROW_TJ(NULL);
      yuvPlanes[i] = yuvPtr;
      yuvPtr += planeSize + yuvStrides[i] - planeWidth;
    }
    memset(yuvBuf, 0, yuvSize);
    TRY_TJ(handle2, tj3EncodeYUVPlanes8(handle2, (unsigned char *)srcBuf, w, 0,
                                        h, pf, yuvPlanes, yuvStrides));
    success &= checkBufYUV(yuvBuf, w, h, subsamp, TJUNSCALED);
    if (success) printf("Passed.\n");
    else printf("FAILED!\n");

    tj3Destroy(handle2);  handle2 = NULL;

    printf("YUV %s %s -> JPEG Q%d ... ", subNameLong[subsamp], buStrLong,
           jpegQual);

    /* Verify that tj3CompressFromYUV*8() ignores TJPARAM_LOSSLESS and
       TJPARAM_COLORSPACE. */
    TRY_TJ(handle, tj3Set(handle, TJPARAM_LOSSLESS, 1));
    TRY_TJ(handle, tj3Set(handle, TJPARAM_COLORSPACE, TJCS_RGB));
    dstBufSize = *dstSize;
    TRY_TJ(handle, tj3CompressFromYUV8(handle, yuvBuf, w, yuvAlign, h, dstBuf,
                                       dstSize));
    MD5Init(&md5ctx);
    MD5Update(&md5ctx, *dstBuf, (unsigned int)(*dstSize));
    md5ref = MD5End(&md5ctx, md5refbuf);

    /* Verify that tj3CompressFromYUVPlanes8() produces the same results. */
    *dstSize = dstBufSize;
    if (*dstBuf && *dstSize > 0) memset(*dstBuf, 0, *dstSize);
    TRY_TJ(handle, tj3CompressFromYUVPlanes8(handle,
                                             (const unsigned char **)yuvPlanes,
                                             w, yuvStrides, h, dstBuf,
                                             dstSize));
    MD5Init(&md5ctx);
    MD5Update(&md5ctx, *dstBuf, (unsigned int)(*dstSize));
    md5sum = MD5End(&md5ctx, md5buf);
    if (strcasecmp(md5sum, md5ref))
      THROW_MD5("JPEG image", md5sum, md5ref);
  } else {
    if (lossless) {
      TRY_TJ(handle, tj3Set(handle, TJPARAM_PRECISION, precision));
      printf("%s %s -> LOSSLESS PSV%d ... ", pfStr, buStrLong, jpegPSV);
    } else
      printf("%s %s -> %s Q%d ... ", pfStr, buStrLong, subNameLong[subsamp],
             jpegQual);
    if (precision <= 8) {
      TRY_TJ(handle, tj3Compress8(handle, (unsigned char *)srcBuf, w, 0, h, pf,
                                  dstBuf, dstSize));
    } else if (precision <= 12) {
      TRY_TJ(handle, tj3Compress12(handle, (short *)srcBuf, w, 0, h, pf,
                                   dstBuf, dstSize));
    } else {
      TRY_TJ(handle, tj3Compress16(handle, (unsigned short *)srcBuf, w, 0, h,
                                   pf, dstBuf, dstSize));
    }
  }

  if (lossless)
    SNPRINTF(tempStr, 1024, "%s_enc%d_%s_%s_LOSSLESS_PSV%d.jpg", basename,
             precision, pfStr, buStr, jpegPSV);
  else
    SNPRINTF(tempStr, 1024, "%s_enc%d_%s_%s_%s_Q%d.jpg", basename, precision,
             pfStr, buStr, subName[subsamp], jpegQual);
  writeJPEG(*dstBuf, *dstSize, tempStr);
  printf("Done.\n  Result in %s\n", tempStr);

bailout:
  tj3Destroy(handle2);
  free(yuvBuf);
  free(srcBuf);
}


static void _decompTest(tjhandle handle, unsigned char *jpegBuf,
                        size_t jpegSize, int w, int h, int pf, char *basename,
                        int subsamp, tjscalingfactor sf)
{
  void *dstBuf = NULL;
  unsigned char *yuvBuf = NULL;
  int success = 1;
  int _hdrw = 0, _hdrh = 0, _hdrsubsamp;
  int scaledWidth = TJSCALED(w, sf);
  int scaledHeight = TJSCALED(h, sf);
  size_t dstSize = 0;
  int bottomUp = tj3Get(handle, TJPARAM_BOTTOMUP);
  tjhandle handle2 = NULL;

  TRY_TJ(handle, tj3SetScalingFactor(handle, sf));
  TRY_TJ(handle, tj3Set(handle, TJPARAM_FASTDCT, 0));

  TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
  _hdrw = tj3Get(handle, TJPARAM_JPEGWIDTH);
  _hdrh = tj3Get(handle, TJPARAM_JPEGHEIGHT);
  _hdrsubsamp = tj3Get(handle, TJPARAM_SUBSAMP);
  if (lossless && subsamp != TJSAMP_444 && subsamp != TJSAMP_GRAY)
    subsamp = TJSAMP_444;
  if (_hdrw != w || _hdrh != h || _hdrsubsamp != subsamp)
    THROW("Incorrect JPEG header");

  dstSize = scaledWidth * scaledHeight * tjPixelSize[pf];
  if ((dstBuf = malloc(dstSize * sampleSize)) == NULL)
    THROW("Memory allocation failure");
  memset(dstBuf, 0, dstSize * sampleSize);

  if (doYUV) {
    unsigned char *yuvPtr, *yuvPlanes[3] = { NULL, NULL, NULL };
    int i, yuvStrides[3] = { 0, 0, 0 };
    size_t yuvSize = tj3YUVBufSize(scaledWidth, yuvAlign, scaledHeight,
                                   subsamp);

    if ((yuvBuf = (unsigned char *)malloc(yuvSize)) == NULL)
      THROW("Memory allocation failure");

    printf("JPEG -> YUV %s ", subNameLong[subsamp]);
    if (sf.num != 1 || sf.denom != 1)
      printf("%d/%d ... ", sf.num, sf.denom);
    else printf("... ");

    /* This should have no effect, but it will cause libjpeg-turbo 3.2.0 and
       prior to produce incorrect chrominance values when decompressing a
       4:2:0, 4:1:0, or 2:4 JPEG image with a scaling factor of 1/2. */
    if (sf.num != 1 || sf.denom != 1)
      TRY_TJ(handle, tj3Set(handle, TJPARAM_FASTDCT, 1));

    memset(yuvBuf, 0, yuvSize);
    TRY_TJ(handle, tj3DecompressToYUV8(handle, jpegBuf, jpegSize, yuvBuf,
                                       yuvAlign));
    success = checkBufYUV(yuvBuf, scaledWidth, scaledHeight, subsamp, sf);

    /* Verify that tj3DecompressToYUVPlanes8() produces the same results. */
    yuvPtr = yuvBuf;
    for (i = 0; i < (subsamp == TJSAMP_GRAY ? 1 : 3); i++) {
      int planeWidth, planeHeight;
      size_t planeSize;

      if ((planeWidth = tj3YUVPlaneWidth(i, scaledWidth, subsamp)) == 0 ||
          (planeHeight = tj3YUVPlaneHeight(i, scaledHeight, subsamp)) == 0)
        THROW_TJ(NULL);
      yuvStrides[i] = PAD(planeWidth, yuvAlign);
      if ((planeSize = tj3YUVPlaneSize(i, scaledWidth, yuvStrides[i],
                                       scaledHeight, subsamp)) == 0)
        THROW_TJ(NULL);
      yuvPlanes[i] = yuvPtr;
      yuvPtr += planeSize + yuvStrides[i] - planeWidth;
    }
    memset(yuvBuf, 0, yuvSize);
    TRY_TJ(handle, tj3DecompressToYUVPlanes8(handle, jpegBuf, jpegSize,
                                             yuvPlanes, yuvStrides));
    success &= checkBufYUV(yuvBuf, scaledWidth, scaledHeight, subsamp, sf);
    if (success) printf("Passed.\n");
    else printf("FAILED!\n");

    printf("YUV %s -> %s %s ... ", subNameLong[subsamp], pixFormatStr[pf],
           bottomUp ? "Bottom-Up" : "Top-Down ");

    if ((handle2 = tj3Init(TJINIT_DECOMPRESS)) == NULL)
      THROW_TJ(NULL);
    TRY_TJ(handle2, tj3Set(handle2, TJPARAM_BOTTOMUP, bottomUp));
    TRY_TJ(handle2, tj3Set(handle2, TJPARAM_SUBSAMP, subsamp));

    TRY_TJ(handle2, tj3DecodeYUV8(handle2, yuvBuf, yuvAlign,
                                  (unsigned char *)dstBuf, scaledWidth, 0,
                                  scaledHeight, pf));
    success = checkBuf(dstBuf, scaledWidth, scaledHeight, pf, subsamp, sf,
                       bottomUp);

    /* Verify that tj3DecodeYUVPlanes8() produces the same results. */
    TRY_TJ(handle2, tj3DecodeYUVPlanes8(handle2,
                                        (const unsigned char **)yuvPlanes,
                                        yuvStrides, (unsigned char *)dstBuf,
                                        scaledWidth, 0, scaledHeight, pf));

    tj3Destroy(handle2);  handle2 = NULL;
  } else {
    printf("JPEG -> %s %s ", pixFormatStr[pf],
           bottomUp ? "Bottom-Up" : "Top-Down ");
    if (sf.num != 1 || sf.denom != 1)
      printf("%d/%d ... ", sf.num, sf.denom);
    else printf("... ");
    if (precision <= 8) {
      TRY_TJ(handle, tj3Decompress8(handle, jpegBuf, jpegSize,
                                    (unsigned char *)dstBuf, 0, pf));
    } else if (precision <= 12) {
      TRY_TJ(handle, tj3Decompress12(handle, jpegBuf, jpegSize,
                                     (short *)dstBuf, 0, pf));
    } else {
      TRY_TJ(handle, tj3Decompress16(handle, jpegBuf, jpegSize,
                                     (unsigned short *)dstBuf, 0, pf));
    }
  }

  success &= checkBuf(dstBuf, scaledWidth, scaledHeight, pf, subsamp, sf,
                      bottomUp);
  if (success) printf("Passed.");
  else printf("FAILED!");
  printf("\n");

bailout:
  tj3Destroy(handle2);
  free(yuvBuf);
  free(dstBuf);
}


static void decompTest(tjhandle handle, unsigned char *jpegBuf,
                       size_t jpegSize, int w, int h, int pf, char *basename,
                       int subsamp)
{
  int i, n = 0;
  tjscalingfactor *sf = NULL;

  if (lossless) {
    _decompTest(handle, jpegBuf, jpegSize, w, h, pf, basename, subsamp,
                TJUNSCALED);
    return;
  }

  sf = tj3GetScalingFactors(&n);
  if (!sf || !n) THROW_TJ(NULL);

  for (i = 0; i < n; i++) {
    if (subsamp == TJSAMP_444 || subsamp == TJSAMP_GRAY ||
        ((subsamp == TJSAMP_411 || subsamp == TJSAMP_441) && sf[i].num == 1 &&
         (sf[i].denom == 2 || sf[i].denom == 1)) ||
        (subsamp != TJSAMP_411 && subsamp != TJSAMP_441 &&
         subsamp != TJSAMP_410 && subsamp != TJSAMP_24 && sf[i].num == 1 &&
         (sf[i].denom == 4 || sf[i].denom == 2 || sf[i].denom == 1)) ||
        (subsamp == TJSAMP_420 && sf[i].num == 1 && sf[i].denom == 8 &&
         !doYUV))
      _decompTest(handle, jpegBuf, jpegSize, w, h, pf, basename, subsamp,
                  sf[i]);
  }

bailout:
  return;
}


static void doTest(int w, int h, const int *formats, int nformats, int subsamp,
                   char *basename)
{
  tjhandle chandle = NULL, dhandle = NULL;
  unsigned char *dstBuf = NULL;
  size_t size = 0, bufSize = 0;
  int pfi, pf, i;

  if (lossless && subsamp != TJSAMP_GRAY)
    subsamp = TJSAMP_444;

  if (!alloc) {
    size = bufSize = tj3JPEGBufSize(w, h, subsamp);
    if (size == 0)
      THROW_TJ(NULL);
    if ((dstBuf = (unsigned char *)tj3Alloc(size)) == NULL)
      THROW("Memory allocation failure.");
  }

  if ((chandle = tj3Init(TJINIT_COMPRESS)) == NULL ||
      (dhandle = tj3Init(TJINIT_DECOMPRESS)) == NULL)
    THROW_TJ(NULL);

  TRY_TJ(chandle, tj3Set(chandle, TJPARAM_NOREALLOC, !alloc));
  if (lossless) {
    TRY_TJ(chandle, tj3Set(chandle, TJPARAM_LOSSLESS, lossless));
    TRY_TJ(chandle, tj3Set(chandle, TJPARAM_LOSSLESSPSV,
                           ((psv++ - 1) % 7) + 1));
  } else {
    TRY_TJ(chandle, tj3Set(chandle, TJPARAM_QUALITY, 100));
    if (subsamp == TJSAMP_422 || subsamp == TJSAMP_420 ||
        subsamp == TJSAMP_440)
      TRY_TJ(dhandle, tj3Set(dhandle, TJPARAM_FASTUPSAMPLE, 1));
  }
  TRY_TJ(chandle, tj3Set(chandle, TJPARAM_SUBSAMP, subsamp));

  for (pfi = 0; pfi < nformats; pfi++) {
    if (formats[pfi] == TJPF_CMYK &&
        (subsamp == TJSAMP_410 || subsamp == TJSAMP_24))
      continue;
    for (i = 0; i < 2; i++) {
      TRY_TJ(chandle, tj3Set(chandle, TJPARAM_BOTTOMUP, i == 1));
      TRY_TJ(dhandle, tj3Set(dhandle, TJPARAM_BOTTOMUP, i == 1));
      pf = formats[pfi];
      if (!alloc) size = bufSize;
      compTest(chandle, &dstBuf, &size, w, h, pf, basename);
      decompTest(dhandle, dstBuf, size, w, h, pf, basename, subsamp);
      if (pf >= TJPF_RGBX && pf <= TJPF_XRGB) {
        printf("\n");
        decompTest(dhandle, dstBuf, size, w, h, pf + (TJPF_RGBA - TJPF_RGBX),
                   basename, subsamp);
      }
      printf("\n");
    }
  }
  printf("--------------------\n\n");

bailout:
  tj3Destroy(chandle);
  tj3Destroy(dhandle);
  tj3Free(dstBuf);
}


#if SIZEOF_SIZE_T == 8
#define CHECKSIZE(function) { \
  if (size && size < (size_t)0xFFFFFFFF) \
    THROW(#function " overflow"); \
}
#define CHECKSIZEUL(function) { \
  if ((unsigned long long)ulsize < (unsigned long long)0xFFFFFFFF) \
    THROW(#function " overflow"); \
}
#else
#define CHECKSIZE(function) { \
  if (size != 0 || !strcmp(tj3GetErrorStr(NULL), "No error")) \
    THROW(#function " overflow"); \
}
#define CHECKSIZEUL(function) { \
  if (ulsize != (unsigned long)(-1) || \
      !strcmp(tj3GetErrorStr(NULL), "No error")) \
    THROW(#function " overflow"); \
}
#endif
#define CHECKSIZEINT(function) { \
  if (intsize != 0 || !strcmp(tj3GetErrorStr(NULL), "No error")) \
    THROW(#function " overflow"); \
}

static void overflowTest(void)
{
  /* Ensure that the various buffer size functions don't overflow */
  size_t size;
  unsigned long ulsize;
  int intsize;

  size = tj3JPEGBufSize(26755, 26755, TJSAMP_444);
  CHECKSIZE(tj3JPEGBufSize());
  size = tj3JPEGBufSize(65536, 65536, TJSAMP_444);
  CHECKSIZE(tj3JPEGBufSize());
  ulsize = tjBufSize(26755, 26755, TJSAMP_444);
  CHECKSIZEUL(tjBufSize());
  ulsize = TJBUFSIZE(26755, 26755);
  CHECKSIZEUL(TJBUFSIZE());
  ulsize = TJBUFSIZE(65536, 65536);
  CHECKSIZEUL(TJBUFSIZE());
  size = tj3YUVBufSize(37838, 1, 37838, TJSAMP_444);
  CHECKSIZE(tj3YUVBufSize());
  size = tj3YUVBufSize(37837, 3, 37837, TJSAMP_444);
  CHECKSIZE(tj3YUVBufSize());
  size = tj3YUVBufSize(37837, -1, 37837, TJSAMP_444);
  CHECKSIZE(tj3YUVBufSize());
  size = tj3YUVBufSize(1024 * 1024 * 1024, 1024 * 1024 * 1024,
                       1024 * 1024 * 1024, TJSAMP_420);
  CHECKSIZE(tj3YUVBufSize());
  ulsize = tjBufSizeYUV2(37838, 1, 37838, TJSAMP_444);
  CHECKSIZEUL(tjBufSizeYUV2());
  ulsize = tjBufSizeYUV2(37837, 3, 37837, TJSAMP_444);
  CHECKSIZEUL(tjBufSizeYUV2());
  ulsize = tjBufSizeYUV2(37837, -1, 37837, TJSAMP_444);
  CHECKSIZEUL(tjBufSizeYUV2());
  ulsize = TJBUFSIZEYUV(37838, 37838, TJSAMP_444);
  CHECKSIZEUL(TJBUFSIZEYUV());
  ulsize = tjBufSizeYUV(37838, 37838, TJSAMP_444);
  CHECKSIZEUL(tjBufSizeYUV());
  size = tj3YUVPlaneSize(0, 65536, 0, 65536, TJSAMP_444);
  CHECKSIZE(tj3YUVPlaneSize());
  ulsize = tjPlaneSizeYUV(0, 65536, 0, 65536, TJSAMP_444);
  CHECKSIZEUL(tjPlaneSizeYUV());
  intsize = tj3YUVPlaneWidth(0, INT_MAX, TJSAMP_420);
  CHECKSIZEINT(tj3YUVPlaneWidth());
  intsize = tj3YUVPlaneHeight(0, INT_MAX, TJSAMP_420);
  CHECKSIZEINT(tj3YUVPlaneHeight());

bailout:
  return;
}


#define BYTESWAP16(array, index, value) { \
  array[index] = (unsigned char)((unsigned int)(value) >> 8); \
  array[index + 1] = (unsigned char)((unsigned int)(value) & 0xFF); \
}

#define CHECK_TJ3TRANSFORMBUFSIZE() { \
  size_t transformBufSize = tj3TransformBufSize(handle, &xform), \
         jpegSizeWithoutMarkers, dstSize0, dstSize; \
  if (transformBufSize == 0) \
    THROW_TJ(NULL) \
  else if (transformBufSize != worstCaseSize) { \
    printf("tj3TransformBufSize() returned %lu instead of %lu\n", \
           (unsigned long)transformBufSize, (unsigned long)worstCaseSize); \
    BAILOUT() \
  } \
  /* Validate the actual marker copying behavior. */ \
  jpegSizeWithoutMarkers = jpegSize - app1HeaderSize - app2HeaderSize - \
                           comHeaderSize - markerDataSize * 3; \
  dstSize = dstSize0 = worstCaseSize - tj3JPEGBufSize(w, h, subsamp) + \
                       jpegSizeWithoutMarkers; \
  TRY_TJ(handle, tj3Transform(handle, jpegBuf, jpegSize, 1, &dstBuf, \
                              &dstSize, &xform)); \
  if (dstSize != dstSize0) { \
    printf("tj3Transform() generated %lu instead of %lu bytes\n", \
           (unsigned long)dstSize, (unsigned long)dstSize0); \
    BAILOUT() \
  } \
}

static void extraMarkersTest(void)
{
  int w = 16, h = 16, i, subsamp;
  void *srcBuf = NULL;
  unsigned char *jpegBuf = NULL, *dstBuf = NULL, *iccBuf = NULL;
  tjhandle handle = NULL;
  size_t worstCaseSize = 0, jpegSize, markerDataSize = 50000,
         app1HeaderSize = 4, app2HeaderSize = 18, comHeaderSize = 4, iccSize;
  tjtransform xform;

  memset(&xform, 0, sizeof(tjtransform));

  for (subsamp = 0; subsamp < TJ_NUMSAMP; subsamp++) {
    /* Generate a JPEG image with APP1, APP2 (ICC), and COM markers. */
    if ((srcBuf = malloc(w * h * 4 * sampleSize)) == NULL)
      THROW("Memory allocation failure");
    for (i = 0; i < w * h * 4; i++) {
      if (random() < RAND_MAX / 2) setVal(srcBuf, i, 0);
      else setVal(srcBuf, i, maxSample);
    }
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp) + app1HeaderSize +
                    app2HeaderSize + comHeaderSize + markerDataSize * 3;
    if ((jpegBuf = (unsigned char *)tj3Alloc(worstCaseSize)) == NULL ||
        (dstBuf = (unsigned char *)tj3Alloc(worstCaseSize)) == NULL)
      THROW("Memory allocation failure");

    if ((handle = tj3Init(TJINIT_COMPRESS)) == NULL)
      THROW_TJ(NULL);
    TRY_TJ(handle, tj3Set(handle, TJPARAM_NOREALLOC, 0));
    TRY_TJ(handle, tj3Set(handle, TJPARAM_QUALITY, 100));
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SUBSAMP, subsamp));
    memset(jpegBuf, 'I', markerDataSize);
    TRY_TJ(handle, tj3SetICCProfile(handle, jpegBuf, markerDataSize));
    jpegSize = worstCaseSize;
    if (precision == 8) {
      TRY_TJ(handle, tj3Compress8(handle, (unsigned char *)srcBuf, w, 0, h,
                                  TJPF_BGRX, &jpegBuf, &jpegSize));
    } else {
      TRY_TJ(handle, tj3Compress12(handle, (short *)srcBuf, w, 0, h, TJPF_BGRX,
                                   &jpegBuf, &jpegSize));
    }
    free(srcBuf);  srcBuf = NULL;
    tj3Destroy(handle);  handle = NULL;

    /* Move JPEG header and image data to make room for additional markers. */
    memcpy(&jpegBuf[2 + app1HeaderSize + comHeaderSize + markerDataSize * 2],
           &jpegBuf[2], jpegSize - 2);
    /* Add APP1 marker */
    i = 2;
    jpegBuf[i++] = 0xFF;
    jpegBuf[i++] = 0xE1;
    BYTESWAP16(jpegBuf, i, markerDataSize + 2);
    i += 2;
    memset(&jpegBuf[i], '1', markerDataSize);
    i += markerDataSize;
    /* Add COM marker */
    jpegBuf[i++] = 0xFF;
    jpegBuf[i++] = 0xFE;
    BYTESWAP16(jpegBuf, i, markerDataSize + 2);
    i += 2;
    memset(&jpegBuf[i], 'C', markerDataSize);
    i += markerDataSize;
    jpegSize += app1HeaderSize + comHeaderSize + markerDataSize * 2;

    /* Verify that tj3TransformBufSize() returns the correct buffer size for
       all TJPARAM_SAVEMARKERS values and transform options, irrespective of
       the value of TJPARAM_SAVEMARKERS when tj3DecompressHeader() was
       called. */
    if ((handle = tj3Init(TJINIT_TRANSFORM)) == NULL)
      THROW_TJ(NULL);

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 2));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    TRY_TJ(handle, tj3GetICCProfile(handle, &iccBuf, &iccSize));
    if (iccBuf == NULL || iccSize != markerDataSize)
      THROW("ICC profile was not extracted");
    tj3Free(iccBuf);  iccBuf = NULL;
    CHECK_TJ3TRANSFORMBUFSIZE();
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 0));
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp);
    CHECK_TJ3TRANSFORMBUFSIZE();

    xform.options = TJXOPT_COPYNONE;
    CHECK_TJ3TRANSFORMBUFSIZE();
    xform.options = 0;

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 0));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    if (tj3GetICCProfile(handle, &iccBuf, &iccSize) != -1 || iccBuf != NULL ||
        iccSize != 0)
      THROW("ICC profile was extracted unexpectedly");
    CHECK_TJ3TRANSFORMBUFSIZE();
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 2));
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp) + app1HeaderSize +
                    app2HeaderSize + comHeaderSize + markerDataSize * 3;
    CHECK_TJ3TRANSFORMBUFSIZE();

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 4));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    TRY_TJ(handle, tj3GetICCProfile(handle, &iccBuf, &iccSize));
    if (iccBuf == NULL || iccSize != markerDataSize)
      THROW("ICC profile was not extracted");
    tj3Free(iccBuf);  iccBuf = NULL;
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp) + app2HeaderSize +
                    markerDataSize;
    CHECK_TJ3TRANSFORMBUFSIZE();
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 3));
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp) + app1HeaderSize +
                    comHeaderSize + markerDataSize * 2;
    CHECK_TJ3TRANSFORMBUFSIZE();

    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    if (tj3GetICCProfile(handle, &iccBuf, &iccSize) != -1 || iccBuf != NULL ||
        iccSize != 0)
      THROW("ICC profile was extracted unexpectedly");
    CHECK_TJ3TRANSFORMBUFSIZE();
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 4));
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp) + app2HeaderSize +
                    markerDataSize;
    CHECK_TJ3TRANSFORMBUFSIZE();

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 1));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    if (tj3GetICCProfile(handle, &iccBuf, &iccSize) != -1 || iccBuf != NULL ||
        iccSize != 0)
      THROW("ICC profile was extracted unexpectedly");
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp) + comHeaderSize +
                    markerDataSize;
    CHECK_TJ3TRANSFORMBUFSIZE();
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 0));
    worstCaseSize = tj3JPEGBufSize(w, h, subsamp);
    CHECK_TJ3TRANSFORMBUFSIZE();

    tj3Destroy(handle);  handle = NULL;

    /* Verify that ICC profile extraction works properly for all
       TJPARAM_SAVEMARKERS values in pure decompression instances. */
    if ((handle = tj3Init(TJINIT_DECOMPRESS)) == NULL)
      THROW_TJ(NULL);

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 0));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    if (tj3GetICCProfile(handle, &iccBuf, &iccSize) != -1 || iccBuf != NULL ||
        iccSize != 0)
      THROW("ICC profile was extracted unexpectedly");

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 2));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    TRY_TJ(handle, tj3GetICCProfile(handle, &iccBuf, &iccSize));
    if (iccBuf == NULL || iccSize != markerDataSize)
      THROW("ICC profile was not extracted");
    tj3Free(iccBuf);  iccBuf = NULL;

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 3));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    if (tj3GetICCProfile(handle, &iccBuf, &iccSize) != -1 || iccBuf != NULL ||
        iccSize != 0)
      THROW("ICC profile was extracted unexpectedly");

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 4));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    TRY_TJ(handle, tj3GetICCProfile(handle, &iccBuf, &iccSize));
    if (iccBuf == NULL || iccSize != markerDataSize)
      THROW("ICC profile was not extracted");
    tj3Free(iccBuf);  iccBuf = NULL;

    TRY_TJ(handle, tj3Set(handle, TJPARAM_SAVEMARKERS, 1));
    TRY_TJ(handle, tj3DecompressHeader(handle, jpegBuf, jpegSize));
    if (tj3GetICCProfile(handle, &iccBuf, &iccSize) != -1 || iccBuf != NULL ||
        iccSize != 0)
      THROW("ICC profile was extracted unexpectedly");

    tj3Destroy(handle);  handle = NULL;

    tj3Free(dstBuf);  dstBuf = NULL;
    tj3Free(jpegBuf);  jpegBuf = NULL;
  }

bailout:
  tj3Free(iccBuf);
  tj3Destroy(handle);
  tj3Free(dstBuf);
  tj3Free(jpegBuf);
  free(srcBuf);
}


static void bufReuseTest(void)
{
  int w = 16, h = 16, i;
  unsigned char *srcBuf = NULL, *jpegSRCBuf = NULL;
  unsigned char *jpegBufs[2] = { NULL, NULL };
  unsigned char *oldJPEGBufs[2] = { NULL, NULL };
  tjhandle handle = NULL;
  size_t worstCaseSize, jpegSRCSize, jpegSizes[2] = { 0, 0 };
  tjtransform xforms[2];

  memset(xforms, 0, 2 * sizeof(tjtransform));

  if ((srcBuf = (unsigned char *)malloc(w * h * 4 * sampleSize)) == NULL)
    THROW("Memory allocation failure");
  for (i = 0; i < w * h * 4; i++) {
    if (random() < RAND_MAX / 2) setVal(srcBuf, i, 0);
    else setVal(srcBuf, i, maxSample);
  }
  worstCaseSize = tj3JPEGBufSize(w, h, TJSAMP_444);
  if ((jpegBufs[0] = (unsigned char *)tj3Alloc(worstCaseSize)) == NULL)
    THROW("Memory allocation failure");

  if ((handle = tj3Init(TJINIT_TRANSFORM)) == NULL)
    THROW_TJ(NULL);
  TRY_TJ(handle, tj3Set(handle, TJPARAM_QUALITY, 100));
  TRY_TJ(handle, tj3Set(handle, TJPARAM_SUBSAMP, TJSAMP_444));

  /* Automatic destination buffer (re)allocation disabled */
  TRY_TJ(handle, tj3Set(handle, TJPARAM_NOREALLOC, 1));

  /* Verify that no error occurs when the destination buffer is pre-allocated
     to the worst-case size. */
  jpegSizes[0] = worstCaseSize;
  TRY_TJ(handle, tj3Compress8(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBufs[0],
                              &jpegSizes[0]));

  /* Verify that an error occurs when the destination buffer is reused but the
     buffer size is not re-specified. */
  if (tj3Compress8(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBufs[0],
                   &jpegSizes[0]) != -1 ||
      strcmp(tj3GetErrorStr(handle),
             "Buffer passed to JPEG library is too small"))
    THROW("tj3Compress8() succeeded unexpectedly");

  /* Automatic destination buffer (re)allocation enabled */
  TRY_TJ(handle, tj3Set(handle, TJPARAM_NOREALLOC, 0));

  /* Verify that no reallocation occurs when the destination buffer is
     pre-allocated to the worst-case size and that the buffer size is ignored
     when the destination buffer is reused. */
  oldJPEGBufs[0] = jpegBufs[0];
  jpegSizes[0] = worstCaseSize;
  TRY_TJ(handle, tj3Compress8(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBufs[0],
                              &jpegSizes[0]));
  if (jpegBufs[0] != oldJPEGBufs[0])
    THROW("JPEG buffer unexpectedly reallocated")
  jpegSizes[0] = 0;
  TRY_TJ(handle, tj3Compress8(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBufs[0],
                              &jpegSizes[0]));
  if (jpegBufs[0] != oldJPEGBufs[0])
    THROW("JPEG buffer unexpectedly reallocated")

  /* Verify that reallocation occurs when the destination buffer is
     pre-allocated but too small and that the destination manager doesn't leak
     memory when reallocating a pre-allocated destination buffer. */
  if ((jpegBufs[1] = (unsigned char *)tj3Alloc(100)) == NULL)
    THROW("Memory allocation failure");
  oldJPEGBufs[1] = jpegBufs[1];
  jpegSizes[1] = 100;
  TRY_TJ(handle, tj3Compress8(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBufs[1],
                              &jpegSizes[1]));
  if (jpegBufs[1] == oldJPEGBufs[1])
    THROW("JPEG buffer was not reallocated")

  jpegSRCSize = jpegSizes[1];
  if ((jpegSRCBuf = (unsigned char *)tj3Alloc(jpegSRCSize)) == NULL)
    THROW("Memory allocation failure");
  memcpy(jpegSRCBuf, jpegBufs[1], jpegSRCSize);

  /* Verify that the destination buffer from a prior tj3Compress*() call can be
     reused as the first destination buffer in tj3Transform() without
     re-specifying the buffer size and that reallocation occurs when the second
     destination buffer is pre-allocated but too small. */
  oldJPEGBufs[1] = jpegBufs[0];
  jpegBufs[0] = oldJPEGBufs[0] = jpegBufs[1];
  jpegBufs[1] = oldJPEGBufs[1];
  jpegSizes[1] = jpegSizes[0];
  jpegSizes[0] = 0;
  TRY_TJ(handle, tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs,
                              jpegSizes, xforms));
  if (jpegBufs[0] != oldJPEGBufs[0])
    THROW("JPEG buffer unexpectedly reallocated")
  if (jpegBufs[1] == oldJPEGBufs[1])
    THROW("JPEG buffer was not reallocated")

  /* Verify that both destination buffers can be reused without re-specifying
     the buffer sizes. */
  oldJPEGBufs[1] = jpegBufs[1];
  jpegSizes[0] = jpegSizes[1] = 1;
  TRY_TJ(handle, tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs,
                              jpegSizes, xforms));
  if (jpegBufs[0] != oldJPEGBufs[0] || jpegBufs[1] != oldJPEGBufs[1])
    THROW("JPEG buffer unexpectedly reallocated")

  /* Verify that the destination manager doesn't leak memory when reallocating
     pre-allocated destination buffers. */
  free(jpegBufs[0]);  jpegBufs[0] = NULL;
  free(jpegBufs[1]);  jpegBufs[1] = NULL;

  jpegSizes[0] = 100;
  if ((jpegBufs[0] = oldJPEGBufs[0] =
       (unsigned char *)tj3Alloc(jpegSizes[0])) == NULL)
    THROW("Memory allocation failure");
  jpegSizes[1] = 50;
  if ((jpegBufs[1] = oldJPEGBufs[1] =
       (unsigned char *)tj3Alloc(jpegSizes[1])) == NULL)
    THROW("Memory allocation failure");
  TRY_TJ(handle, tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs,
                              jpegSizes, xforms));
  if (jpegBufs[0] == oldJPEGBufs[0] || jpegBufs[1] == oldJPEGBufs[1])
    THROW("JPEG buffer was not reallocated")

  /* Verify that the destination buffers can only be reused with the
     corresponding transforms. */
  oldJPEGBufs[1] = jpegBufs[0];
  jpegBufs[0] = oldJPEGBufs[0] = jpegBufs[1];
  jpegBufs[1] = oldJPEGBufs[1];
  TRY_TJ(handle, tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs,
                              jpegSizes, xforms));
  if (jpegBufs[0] == oldJPEGBufs[0] || jpegBufs[1] == oldJPEGBufs[1])
    THROW("JPEG buffer was not reallocated")

  /* Verify that the first destination buffer from a prior tj3Transform() call
     can be reused in tj3Compress*() without re-specifying the buffer size. */
  oldJPEGBufs[0] = jpegBufs[0];
  jpegSizes[0] = 1;
  TRY_TJ(handle, tj3Compress8(handle, srcBuf, w, 0, h, TJPF_BGRX, &jpegBufs[0],
                              &jpegSizes[0]));
  if (jpegBufs[0] != oldJPEGBufs[0])
    THROW("JPEG buffer unexpectedly reallocated")

  /* Verify that tj3Transform() no longer tracks the second destination
     buffer. */
  oldJPEGBufs[1] = jpegBufs[1];
  jpegSizes[0] = 1;
  TRY_TJ(handle, tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs,
                              jpegSizes, xforms));
  if (jpegBufs[0] != oldJPEGBufs[0])
    THROW("JPEG buffer unexpectedly reallocated")
  if (jpegBufs[1] == oldJPEGBufs[1])
    THROW("JPEG buffer was not reallocated")

  /* Verify that tj3Transform() properly handles a caller that erroneously
     specifies the same destination buffer twice.  (If the first destination
     buffer has to be reallocated, then this would cause a use-after-free
     issue.) */
  free(jpegBufs[1]);  jpegBufs[1] = NULL;

  jpegBufs[1] = jpegBufs[0];
  jpegSizes[1] = jpegSizes[0];
  if (tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs, jpegSizes,
                   xforms) != -1 ||
      strcmp(tj3GetErrorStr(handle),
             "tj3Transform(): Duplicate destination buffer")) {
    jpegBufs[1] = NULL;
    THROW("tj3Transform() succeeded unexpectedly");
  }
  jpegBufs[1] = NULL;

  /* Verify that tj3Transform() allows multiple destination buffers to be
     NULL. */
  free(jpegBufs[0]);  jpegBufs[0] = NULL;
  TRY_TJ(handle, tj3Transform(handle, jpegSRCBuf, jpegSRCSize, 2, jpegBufs,
                              jpegSizes, xforms));

bailout:
  tj3Destroy(handle);
  tj3Free(jpegBufs[0]);
  tj3Free(jpegBufs[1]);
  free(jpegSRCBuf);
  free(srcBuf);
}


static void bufSizeTest(void)
{
  int w, h, i, subsamp;
  void *srcBuf = NULL;
  unsigned char *dstBuf = NULL;
  tjhandle handle = NULL;
  size_t dstSize = 0;
  int numSamp = TJ_NUMSAMP;

  if ((handle = tj3Init(TJINIT_COMPRESS)) == NULL)
    THROW_TJ(NULL);

  TRY_TJ(handle, tj3Set(handle, TJPARAM_NOREALLOC, !alloc));
  if (lossless) {
    TRY_TJ(handle, tj3Set(handle, TJPARAM_PRECISION, precision));
    TRY_TJ(handle, tj3Set(handle, TJPARAM_LOSSLESS, lossless));
    TRY_TJ(handle, tj3Set(handle, TJPARAM_LOSSLESSPSV,
                          ((psv++ - 1) % 7) + 1));
    numSamp = 1;
  } else
    TRY_TJ(handle, tj3Set(handle, TJPARAM_QUALITY, 100));

  printf("Buffer size regression test\n");
  for (subsamp = 0; subsamp < numSamp; subsamp++) {
    TRY_TJ(handle, tj3Set(handle, TJPARAM_SUBSAMP, subsamp));
    for (w = 1; w < 48; w++) {
      int maxh = (w == 1) ? 2048 : 48;

      for (h = 1; h < maxh; h++) {
        if (h % 100 == 0) printf("%.4d x %.4d\b\b\b\b\b\b\b\b\b\b\b", w, h);
        if ((srcBuf = malloc(w * h * 4 * sampleSize)) == NULL)
          THROW("Memory allocation failure");
        if (!alloc || doYUV) {
          if (doYUV) dstSize = tj3YUVBufSize(w, yuvAlign, h, subsamp);
          else dstSize = tj3JPEGBufSize(w, h, subsamp);
          if ((dstBuf = (unsigned char *)tj3Alloc(dstSize)) == NULL)
            THROW("Memory allocation failure");
        }

        for (i = 0; i < w * h * 4; i++) {
          if (random() < RAND_MAX / 2) setVal(srcBuf, i, 0);
          else setVal(srcBuf, i, maxSample);
        }

        if (doYUV) {
          /* Verify that tj3EncodeYUV*8() ignores TJPARAM_LOSSLESS and
             TJPARAM_COLORSPACE. */
          TRY_TJ(handle, tj3Set(handle, TJPARAM_LOSSLESS, 1));
          TRY_TJ(handle, tj3Set(handle, TJPARAM_COLORSPACE, TJCS_RGB));
          TRY_TJ(handle, tj3EncodeYUV8(handle, (unsigned char *)srcBuf, w, 0,
                                       h, TJPF_BGRX, dstBuf, yuvAlign));
        } else {
          /* Verify that the API is hardened against hypothetical applications
             that may erroneously set the JPEG destination buffer size to 0
             while reusing the destination buffer pointer. */
          if (alloc && (w > 1 || h > 1)) dstSize = 0;
          if (precision <= 8) {
            TRY_TJ(handle, tj3Compress8(handle, (unsigned char *)srcBuf, w, 0,
                                        h, TJPF_BGRX, &dstBuf, &dstSize));
          } else if (precision <= 12) {
            TRY_TJ(handle, tj3Compress12(handle, (short *)srcBuf, w, 0, h,
                                         TJPF_BGRX, &dstBuf, &dstSize));
          } else {
            TRY_TJ(handle, tj3Compress16(handle, (unsigned short *)srcBuf, w,
                                         0, h, TJPF_BGRX, &dstBuf, &dstSize));
          }
        }
        free(srcBuf);  srcBuf = NULL;
        if (!alloc || doYUV) {
          tj3Free(dstBuf);  dstBuf = NULL;
        }

        if ((srcBuf = malloc(h * w * 4 * sampleSize)) == NULL)
          THROW("Memory allocation failure");
        if (!alloc || doYUV) {
          if (doYUV) dstSize = tj3YUVBufSize(h, yuvAlign, w, subsamp);
          else dstSize = tj3JPEGBufSize(h, w, subsamp);
          if ((dstBuf = (unsigned char *)tj3Alloc(dstSize)) == NULL)
            THROW("Memory allocation failure");
        }

        for (i = 0; i < h * w * 4; i++) {
          if (random() < RAND_MAX / 2) setVal(srcBuf, i, 0);
          else setVal(srcBuf, i, maxSample);
        }

        if (doYUV) {
          /* Verify that tj3EncodeYUV*8() ignores TJPARAM_LOSSLESS and
             TJPARAM_COLORSPACE. */
          TRY_TJ(handle, tj3Set(handle, TJPARAM_LOSSLESS, 1));
          TRY_TJ(handle, tj3Set(handle, TJPARAM_COLORSPACE, TJCS_RGB));
          TRY_TJ(handle, tj3EncodeYUV8(handle, (unsigned char *)srcBuf, h, 0,
                                       w, TJPF_BGRX, dstBuf, yuvAlign));
        } else {
          if (alloc && (w > 1 || h > 1)) dstSize = 0;
          if (precision <= 8) {
            TRY_TJ(handle, tj3Compress8(handle, (unsigned char *)srcBuf, h, 0,
                                        w, TJPF_BGRX, &dstBuf, &dstSize));
          } else if (precision <= 12) {
            TRY_TJ(handle, tj3Compress12(handle, (short *)srcBuf, h, 0, w,
                                         TJPF_BGRX, &dstBuf, &dstSize));
          } else {
            TRY_TJ(handle, tj3Compress16(handle, (unsigned short *)srcBuf, h,
                                         0, w, TJPF_BGRX, &dstBuf, &dstSize));
          }
        }
        free(srcBuf);  srcBuf = NULL;
        if (!alloc || doYUV) {
          tj3Free(dstBuf);  dstBuf = NULL;
        }
      }
    }
  }
  printf("Done.      \n");

bailout:
  free(srcBuf);
  tj3Free(dstBuf);
  tj3Destroy(handle);
}


static void rgb_to_cmyk(int r, int g, int b, int *c, int *m, int *y, int *k)
{
  double ctmp = 1.0 - ((double)r / (double)maxSample);
  double mtmp = 1.0 - ((double)g / (double)maxSample);
  double ytmp = 1.0 - ((double)b / (double)maxSample);
  double ktmp = min(min(ctmp, mtmp), ytmp);

  if (ktmp == 1.0) ctmp = mtmp = ytmp = 0.0;
  else {
    ctmp = (ctmp - ktmp) / (1.0 - ktmp);
    mtmp = (mtmp - ktmp) / (1.0 - ktmp);
    ytmp = (ytmp - ktmp) / (1.0 - ktmp);
  }
  *c = (int)((double)maxSample - ctmp * (double)maxSample + 0.5);
  *m = (int)((double)maxSample - mtmp * (double)maxSample + 0.5);
  *y = (int)((double)maxSample - ytmp * (double)maxSample + 0.5);
  *k = (int)((double)maxSample - ktmp * (double)maxSample + 0.5);
}

static void initBitmap(void *buf, int width, int pitch, int height, int pf,
                       int bottomUp)
{
  int roffset = tjRedOffset[pf];
  int goffset = tjGreenOffset[pf];
  int boffset = tjBlueOffset[pf];
  int ps = tjPixelSize[pf];
  int i, j, ci;

  for (j = 0; j < height; j++) {
    int row = bottomUp ? height - j - 1 : j;

    for (i = 0; i < width; i++) {
      int r = (i * (maxSample + 1) / width) % (maxSample + 1);
      int g = (j * (maxSample + 1) / height) % (maxSample + 1);
      int b = (j * (maxSample + 1) / height +
               i * (maxSample + 1) / width) % (maxSample + 1);

      for (ci = 0; ci < ps; ci++)
        setVal(buf, row * pitch + i * ps + ci, 0);
      if (pf == TJPF_GRAY) setVal(buf, row * pitch + i * ps, b);
      else if (pf == TJPF_CMYK) {
        int c, m, y, k;

        rgb_to_cmyk(r, g, b, &c, &m, &y, &k);
        setVal(buf, row * pitch + i * ps + 0, c);
        setVal(buf, row * pitch + i * ps + 1, m);
        setVal(buf, row * pitch + i * ps + 2, y);
        setVal(buf, row * pitch + i * ps + 3, k);
      } else {
        setVal(buf, row * pitch + i * ps + roffset, r);
        setVal(buf, row * pitch + i * ps + goffset, g);
        setVal(buf, row * pitch + i * ps + boffset, b);
      }
    }
  }
}


static void cmyk_to_rgb(int c, int m, int y, int k, int *r, int *g, int *b,
                        int targetMaxSample)
{
  *r = (int)((double)c * (double)k / (double)targetMaxSample + 0.5);
  *g = (int)((double)m * (double)k / (double)targetMaxSample + 0.5);
  *b = (int)((double)y * (double)k / (double)targetMaxSample + 0.5);
}

static int cmpBitmap(void *buf, int width, int pitch, int height, int pf,
                     int bottomUp, int gray2rgb, int targetPrecision,
                     const char *ext)
{
  int roffset = tjRedOffset[pf];
  int goffset = tjGreenOffset[pf];
  int boffset = tjBlueOffset[pf];
  int aoffset = tjAlphaOffset[pf];
  int ps = tjPixelSize[pf];
  int targetMaxSample, i, j;

  if (!strcasecmp(ext, "bmp"))
    targetPrecision = 8;
  targetMaxSample = (1 << targetPrecision) - 1;

  for (j = 0; j < height; j++) {
    int row = bottomUp ? height - j - 1 : j;

    for (i = 0; i < width; i++) {
      int r = (i * (maxSample + 1) / width) % (maxSample + 1);
      int g = (j * (maxSample + 1) / height) % (maxSample + 1);
      int b = (j * (maxSample + 1) / height +
               i * (maxSample + 1) / width) % (maxSample + 1);

      if (precision != targetPrecision) {
        long halfMaxSample = maxSample / 2;

        /* The expected results are slightly different with PNG files, because
         * the samples are scaled up to 8 or 16 bits of data precision by the
         * PNG writer and scaled down to the target data precision by the
         * reader.
         */
        if (!strcasecmp(ext, "png")) {
          if (precision <= 8) {
            r = (int)((r * 255 + halfMaxSample) / maxSample);
            r = (int)((r * ((1 << targetPrecision) - 1) + 127) / 255);
            g = (int)((g * 255 + halfMaxSample) / maxSample);
            g = (int)((g * ((1 << targetPrecision) - 1) + 127) / 255);
            b = (int)((b * 255 + halfMaxSample) / maxSample);
            b = (int)((b * ((1 << targetPrecision) - 1) + 127) / 255);
          } else {
            unsigned int rtemp =
              (unsigned int)((r * 65535U + halfMaxSample) / maxSample);
            unsigned int gtemp =
              (unsigned int)((g * 65535U + halfMaxSample) / maxSample);
            unsigned int btemp =
              (unsigned int)((b * 65535U + halfMaxSample) / maxSample);

            r = (int)((rtemp * ((1 << targetPrecision) - 1) + 32767) / 65535);
            g = (int)((gtemp * ((1 << targetPrecision) - 1) + 32767) / 65535);
            b = (int)((btemp * ((1 << targetPrecision) - 1) + 32767) / 65535);
          }
        } else {
          r = (int)((r * ((1 << targetPrecision) - 1) + halfMaxSample) /
                    maxSample);
          g = (int)((g * ((1 << targetPrecision) - 1) + halfMaxSample) /
                    maxSample);
          b = (int)((b * ((1 << targetPrecision) - 1) + halfMaxSample) /
                    maxSample);
        }
      }

      if (pf == TJPF_GRAY) {
        if (getVal(buf, row * pitch + i * ps, targetPrecision) != b)
          return 0;
      } else if (pf == TJPF_CMYK) {
        int rf, gf, bf;

        cmyk_to_rgb(getVal(buf, row * pitch + i * ps + 0, targetPrecision),
                    getVal(buf, row * pitch + i * ps + 1, targetPrecision),
                    getVal(buf, row * pitch + i * ps + 2, targetPrecision),
                    getVal(buf, row * pitch + i * ps + 3, targetPrecision),
                    &rf, &gf, &bf, targetMaxSample);
        if (gray2rgb) {
          if (rf != b || gf != b || bf != b)
            return 0;
        } else if (rf != r || gf != g || bf != b) return 0;
      } else {
        if (gray2rgb) {
          if (getVal(buf, row * pitch + i * ps + roffset,
                     targetPrecision) != b ||
              getVal(buf, row * pitch + i * ps + goffset,
                     targetPrecision) != b ||
              getVal(buf, row * pitch + i * ps + boffset,
                     targetPrecision) != b)
            return 0;
        } else if (getVal(buf, row * pitch + i * ps + roffset,
                          targetPrecision) != r ||
                   getVal(buf, row * pitch + i * ps + goffset,
                          targetPrecision) != g ||
                   getVal(buf, row * pitch + i * ps + boffset,
                          targetPrecision) != b)
          return 0;
        if (aoffset >= 0 &&
            getVal(buf, row * pitch + i * ps + aoffset,
                   targetPrecision) != targetMaxSample)
          return 0;
      }
    }
  }
  return 1;
}


static int doBmpTest(const char *ext, int width, int align, int height, int pf,
                     int bottomUp)
{
  tjhandle handle = NULL;
  char filename[80], *md5sum, md5buf[65];
  int ps = tjPixelSize[pf], pitch = PAD(width * ps, align), loadWidth = 0,
    loadHeight = 0, retval = 0, pixelFormat = pf;
  void *buf = NULL;
  char *md5ref;
  char *colorPNGRefs[17] = {
    "", "", "17aa360ae532ae6e91b2a28d38bd624a",
    "e2a50ec3401ad862738d6987c6ceb7a1", "463085239d02088e0dc797ec548d8ca2",
    "6568db6ab8cca36e28c49d54a5cf72d2", "7ef966e63e71c9a1d36fc7b063a6b188",
    "9b729cea39a6da8927c7d3e42fa8aa2a", "82795201e05fd48c3992d1a7277ff291",
    "80c1ea7e0d9d6a0a8d12722bc349f94c", "6a13b4aefcc08f136952cb77f934c969",
    "c459ba1d57d66b5025798cba6f9cb137", "ce7e67e24d223120ba49f81278549389",
    "1ebb0a8fc1d7d1b291f07a1743710427", "b68fd59466d37a4405fef22f50708c3e",
    "06941dae98e471279433de7edb9fe4e5", "8df69d8b7ddd4fc45675c1404f8301df"
  };
  char *grayPNGRefs[17] = {
    "", "", "36b39208ebecab7e44d3e7d16540d9dd",
    "a23ea89e91a33e47d487da5e51aaf73c", "9fd24370306a37faab455f8569e364f1",
    "8190d5e4e51adeb7345b735b6dbdc07c", "8842cd8a1d8ee88cdf8f4ee3ec35ef9f",
    "7e8f8c89d62ccc14e062024f47896c78", "412f0ac480f413d39a36c8cd3f0ffff5",
    "d7d3588dc714e41e537acda1327dceaa", "1d18b75a701fae306e78d2c8d29ee8cd",
    "e65d72d45858b7f6edd22d49285086dc", "33ed8ea0a2c3faa1e24a766e0706f241",
    "8da798e493c3f0d335278dc69004444b", "f7c4aa691f459aef9e9796f2f8b10f3e",
    "7e601698088be2559538efc750b978a5", "0580aea16346e6afb4317399539bf75e"
  };
  char *colorPPMRefs[17] = {
    "", "", "0bad09d9ef38eda566848fb7c0b7fd0a",
    "7ef2c87261a8bd6838303b541563cf27", "28a37cf9636ff6bb9ed6b206bdac60db",
    "723307791d42e0b5f9e91625c7636086", "d729c4bcd3addc14abc16b656c6bbc98",
    "5d7636eedae3cf579b6de13078227548", "c0c9f772b464d1896326883a5c79c545",
    "fcf6490e0445569427f1d95baf5f8fcb", "5cbc3b0ccba23f5781d950a72e0ccc83",
    "0d4e26d6d16d7bfee380f6feb10f7e53", "2ff5299287017502832c99718450c90a",
    "44ae6cd70c798ea583ab0c8c03621092", "697b2fe03892bc9a75396ad3e73d9203",
    "599732f973eb7c0849a888e783bbe27e", "623f54661b928d170bd2324bc3620565"
  };
  char *grayPPMRefs[17] = {
    "", "", "7565be35a2ce909cae016fa282af8efa",
    "e86b9ea57f7d53f6b5497653740992b5", "8924d4d81fe0220c684719294f93407a",
    "e2e69ba70efcfae317528c91651c7ae2", "e6154aafc1eb9e4333d68ce7ad9df051",
    "3d7fe831d6fbe55d3fa12f52059c15d3", "112c682e82ce5de1cca089e20d60000b",
    "05a7ce86c649dda86d6fed185ab78a67", "0b723c0bc087592816523fbc906b7c3a",
    "5da422b1ddfd44c7659094d42ba5580c", "0d1895c7e6f2b2c9af6e821a655c239c",
    "00fc2803bca103ff75785ea0dca992aa", "d8c91fac522c16b029e514d331a22bc4",
    "e50cff0b3562ed7e64dbfc093440e333", "64f3320b226ea37fb58080713b4df1b2"
  };
  int targetPrecision, maxTargetPrecision = 16;

  if ((handle = tj3Init(TJINIT_TRANSFORM)) == NULL)
    THROW_TJ(NULL);
  TRY_TJ(handle, tj3Set(handle, TJPARAM_BOTTOMUP, bottomUp));
  TRY_TJ(handle, tj3Set(handle, TJPARAM_PRECISION, precision));

  if (precision == 8 && !strcasecmp(ext, "bmp")) {
    md5ref = (pf == TJPF_GRAY ? "51976530acf75f02beddf5d21149101d" :
                                "6d659071b9bfcdee2def22cb58ddadca");
    maxTargetPrecision = 8;
  } else if (!strcasecmp(ext, "png"))
    md5ref = (pf == TJPF_GRAY ? grayPNGRefs[precision] :
                                colorPNGRefs[precision]);
  else
    md5ref = (pf == TJPF_GRAY ? grayPPMRefs[precision] :
                                colorPPMRefs[precision]);

  if ((buf = tj3Alloc(pitch * height * sampleSize)) == NULL)
    THROW("Could not allocate memory");
  initBitmap(buf, width, pitch, height, pf, bottomUp);

  SNPRINTF(filename, 80, "test_bmp%d_%s_%d_%s_%d.%s", precision,
           pixFormatStr[pf], align, bottomUp ? "bu" : "td", getpid(), ext);
  if (precision <= 8) {
    TRY_TJ(handle, tj3SaveImage8(handle, filename, (unsigned char *)buf, width,
                                 pitch, height, pf));
  } else if (precision <= 12) {
    TRY_TJ(handle, tj3SaveImage12(handle, filename, (short *)buf, width, pitch,
                                  height, pf));
  } else {
    TRY_TJ(handle, tj3SaveImage16(handle, filename, (unsigned short *)buf,
                                  width, pitch, height, pf));
  }
  tj3Free(buf);  buf = NULL;
  md5sum = MD5File(filename, md5buf);
  if (!md5sum) {
    printf("\n   Could not determine MD5 sum of %s\n", filename);
    retval = -1;  goto bailout;
  }
  if (strcasecmp(md5sum, md5ref))
    THROW_MD5(filename, md5sum, md5ref);

  for (targetPrecision = 2; targetPrecision <= maxTargetPrecision;
       targetPrecision++) {
    TRY_TJ(handle, tj3Set(handle, TJPARAM_PRECISION, targetPrecision));
    pf = pixelFormat;

    if (targetPrecision <= 8) {
      if ((buf = tj3LoadImage8(handle, filename, &loadWidth, align,
                               &loadHeight, &pf)) == NULL)
        THROW_TJ(handle);
    } else if (targetPrecision <= 12) {
      if ((buf = tj3LoadImage12(handle, filename, &loadWidth, align,
                                &loadHeight, &pf)) == NULL)
        THROW_TJ(handle);
    } else {
      if ((buf = tj3LoadImage16(handle, filename, &loadWidth, align,
                                &loadHeight, &pf)) == NULL)
        THROW_TJ(handle);
    }
    if (width != loadWidth || height != loadHeight) {
      printf("\n   Image dimensions of %s are bogus\n", filename);
      retval = -1;  goto bailout;
    }
    pitch = PAD(width * tjPixelSize[pf], align);
    if (!cmpBitmap(buf, width, pitch, height, pf, bottomUp, 0, targetPrecision,
                   ext)) {
      printf("\n   Pixel data in %s is bogus\n", filename);
      printf("   (target data precision = %d)\n", targetPrecision);
      retval = -1;  goto bailout;
    }
    tj3Free(buf);  buf = NULL;

    if (pf == TJPF_GRAY) {
      pf = TJPF_XBGR;
      if (targetPrecision <= 8) {
        if ((buf = tj3LoadImage8(handle, filename, &loadWidth, align,
                                 &loadHeight, &pf)) == NULL)
          THROW_TJ(handle);
      } else if (targetPrecision <= 12) {
        if ((buf = tj3LoadImage12(handle, filename, &loadWidth, align,
                                  &loadHeight, &pf)) == NULL)
          THROW_TJ(handle);
      } else {
        if ((buf = tj3LoadImage16(handle, filename, &loadWidth, align,
                                  &loadHeight, &pf)) == NULL)
          THROW_TJ(handle);
      }
      pitch = PAD(width * tjPixelSize[pf], align);
      if (!cmpBitmap(buf, width, pitch, height, pf, bottomUp, 1,
                     targetPrecision, ext)) {
        printf("\n   Converting %s to RGB failed\n", filename);
        printf("   (target data precision = %d)\n", targetPrecision);
        retval = -1;  goto bailout;
      }
      tj3Free(buf);  buf = NULL;

      pf = TJPF_CMYK;
      if (targetPrecision <= 8) {
        if ((buf = tj3LoadImage8(handle, filename, &loadWidth, align,
                                 &loadHeight, &pf)) == NULL)
          THROW_TJ(handle);
      } else if (targetPrecision <= 12) {
        if ((buf = tj3LoadImage12(handle, filename, &loadWidth, align,
                                  &loadHeight, &pf)) == NULL)
          THROW_TJ(handle);
      } else {
        if ((buf = tj3LoadImage16(handle, filename, &loadWidth, align,
                                  &loadHeight, &pf)) == NULL)
          THROW_TJ(handle);
      }
      pitch = PAD(width * tjPixelSize[pf], align);
      if (!cmpBitmap(buf, width, pitch, height, pf, bottomUp, 1,
                     targetPrecision, ext)) {
        printf("\n   Converting %s to CMYK failed\n", filename);
        printf("   (target data precision = %d)\n", targetPrecision);
        retval = -1;  goto bailout;
      }
      tj3Free(buf);  buf = NULL;
    }

    /* Verify that tj3LoadImage*() returns the proper "preferred" pixel format
       for the file type. */
    pf = pixelFormat;
    pixelFormat = TJPF_UNKNOWN;
    if (targetPrecision <= 8) {
      if ((buf = tj3LoadImage8(handle, filename, &loadWidth, align,
                               &loadHeight, &pixelFormat)) == NULL)
        THROW_TJ(handle);
    } else if (targetPrecision <= 12) {
      if ((buf = tj3LoadImage12(handle, filename, &loadWidth, align,
                                &loadHeight, &pixelFormat)) == NULL)
        THROW_TJ(handle);
    } else {
      if ((buf = tj3LoadImage16(handle, filename, &loadWidth, align,
                                &loadHeight, &pixelFormat)) == NULL)
        THROW_TJ(handle);
    }
    tj3Free(buf);  buf = NULL;
    if ((pf == TJPF_GRAY && pixelFormat != TJPF_GRAY) ||
        (pf != TJPF_GRAY && !strcasecmp(ext, "bmp") &&
         pixelFormat != TJPF_BGR) ||
        (pf != TJPF_GRAY && !strcasecmp(ext, "ppm") &&
         pixelFormat != TJPF_RGB)) {
      printf("\n   tj3LoadImage8() returned unexpected pixel format: %s\n",
             pixFormatStr[pixelFormat]);
      retval = -1;
    }
  }
  unlink(filename);

bailout:
  tj3Destroy(handle);
  tj3Free(buf);
  if (exitStatus < 0) return exitStatus;
  return retval;
}


static int bmpTest(void)
{
  int align, width = 35, height = 39, format;

  for (align = 1; align <= 8; align *= 2) {
    for (format = 0; format < TJ_NUMPF; format++) {
      if (precision == 8) {
        printf("%s Top-Down BMP (row alignment = %d samples)  ...  ",
               pixFormatStr[format], align);
        if (doBmpTest("bmp", width, align, height, format, 0) == -1)
          return -1;
        printf("OK.\n");
      }

      printf("%s Top-Down PNG (row alignment = %d samples)  ...  ",
             pixFormatStr[format], align);
      if (doBmpTest("png", width, align, height, format, 0) == -1)
        return -1;
      printf("OK.\n");

      printf("%s Top-Down PPM (row alignment = %d samples)  ...  ",
             pixFormatStr[format], align);
      if (doBmpTest("ppm", width, align, height, format, 0) == -1)
        return -1;
      printf("OK.\n");

      if (precision == 8) {
        printf("%s Bottom-Up BMP (row alignment = %d samples)  ...  ",
               pixFormatStr[format], align);
        if (doBmpTest("bmp", width, align, height, format, 1) == -1)
          return -1;
        printf("OK.\n");
      }

      printf("%s Bottom-Up PNG (row alignment = %d samples)  ...  ",
             pixFormatStr[format], align);
      if (doBmpTest("png", width, align, height, format, 1) == -1)
        return -1;
      printf("OK.\n");

      printf("%s Bottom-Up PPM (row alignment = %d samples)  ...  ",
             pixFormatStr[format], align);
      if (doBmpTest("ppm", width, align, height, format, 1) == -1)
        return -1;
      printf("OK.\n");
    }
  }

  return 0;
}


int main(int argc, char *argv[])
{
  int i, bmp = 0, num4bf = 5;

#ifdef _WIN32
  srand((unsigned int)time(NULL));
#endif
  if (argc > 1) {
    for (i = 1; i < argc; i++) {
      if (!strcasecmp(argv[i], "-yuv")) doYUV = 1;
      else if (!strcasecmp(argv[i], "-noyuvpad")) yuvAlign = 1;
      else if (!strcasecmp(argv[i], "-lossless")) lossless = 1;
      else if (!strcasecmp(argv[i], "-alloc")) alloc = 1;
      else if (!strcasecmp(argv[i], "-bmp")) bmp = 1;
      else if (!strcasecmp(argv[i], "-precision") && i < argc - 1) {
        int tempi = atoi(argv[++i]);

        if (tempi < 2 || tempi > 16)
          usage(argv[0]);
        precision = tempi;
        if (precision != 8 && precision != 12) lossless = 1;
      } else
        usage(argv[0]);
    }
  }
  if (lossless && doYUV)
    THROW("Lossless JPEG and YUV encoding/decoding are incompatible.");
  if (precision != 8 && doYUV)
    THROW("YUV encoding/decoding requires 8-bit data precision.");

  printf("Testing %d-bit precision\n", precision);
  sampleSize = (precision <= 8 ? 1 : 2);
  maxSample = (1 << precision) - 1;
  tolerance = (lossless ? 0 : (precision > 8 ? 2 : 1));
  redToY = (19595U * maxSample) >> 16;
  yellowToY = (58065U * maxSample) >> 16;

  if (bmp) return bmpTest();
  if (alloc) printf("Testing automatic buffer allocation\n");
  if (doYUV) num4bf = 4;
  overflowTest();
  if (!lossless && !alloc && !doYUV)
    extraMarkersTest();
  if (!lossless && alloc && !doYUV && precision == 8)
    bufReuseTest();
  doTest(35, 39, _3sampleFormats, 2, TJSAMP_444, "test");
  doTest(39, 41, _4sampleFormats, num4bf, TJSAMP_444, "test");
  doTest(41, 35, _3sampleFormats, 2, TJSAMP_422, "test");
  if (!lossless) {
    doTest(35, 39, _4sampleFormats, num4bf, TJSAMP_422, "test");
    doTest(39, 41, _3sampleFormats, 2, TJSAMP_420, "test");
    doTest(41, 35, _4sampleFormats, num4bf, TJSAMP_420, "test");
    doTest(35, 39, _3sampleFormats, 2, TJSAMP_440, "test");
    doTest(39, 41, _4sampleFormats, num4bf, TJSAMP_440, "test");
    doTest(41, 35, _3sampleFormats, 2, TJSAMP_411, "test");
    doTest(35, 39, _4sampleFormats, num4bf, TJSAMP_411, "test");
    doTest(39, 41, _3sampleFormats, 2, TJSAMP_441, "test");
    doTest(41, 35, _4sampleFormats, num4bf, TJSAMP_441, "test");
    doTest(35, 41, _3sampleFormats, 2, TJSAMP_410, "test");
    doTest(39, 35, _4sampleFormats, num4bf, TJSAMP_410, "test");
    doTest(41, 39, _3sampleFormats, 2, TJSAMP_24, "test");
    doTest(35, 41, _4sampleFormats, num4bf, TJSAMP_24, "test");
  }
  doTest(39, 41, _onlyGray, 1, TJSAMP_GRAY, "test");
  if (!lossless) {
    doTest(41, 35, _3sampleFormats, 2, TJSAMP_GRAY, "test");
    doTest(35, 39, _4sampleFormats, 4, TJSAMP_GRAY, "test");
  }
  bufSizeTest();
  if (doYUV) {
    printf("\n--------------------\n\n");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_444, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_422, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_420, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_440, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_411, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_441, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_410, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_24, "test_yuv0");
    doTest(48, 48, _onlyRGB, 1, TJSAMP_GRAY, "test_yuv0");
    doTest(48, 48, _onlyGray, 1, TJSAMP_GRAY, "test_yuv0");
  }

  bailout:
  return exitStatus;
}
