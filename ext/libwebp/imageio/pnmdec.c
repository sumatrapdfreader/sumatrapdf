// Copyright 2017 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
// (limited) PNM decoder

#include "./pnmdec.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "webp/encode.h"
#include "./imageio_util.h"

typedef enum {
  WIDTH_FLAG      = 1 << 0,
  HEIGHT_FLAG     = 1 << 1,
  DEPTH_FLAG      = 1 << 2,
  MAXVAL_FLAG     = 1 << 3,
  TUPLE_FLAG      = 1 << 4,
  ALL_NEEDED_FLAGS = WIDTH_FLAG | HEIGHT_FLAG | DEPTH_FLAG | MAXVAL_FLAG
} PNMFlags;

typedef struct {
  const uint8_t* data;
  size_t data_size;
  int width, height;
  int bytes_per_px;   // 1, 3, 4
  int depth;
  int max_value;
  int type;           // 5, 6 or 7
  int seen_flags;
} PNMInfo;

// -----------------------------------------------------------------------------
// PNM decoding

#define MAX_LINE_SIZE 1024
static const size_t kMinPNMHeaderSize = 3;

static size_t ReadLine(const uint8_t* const data, size_t off, size_t data_size,
                       char out[MAX_LINE_SIZE + 1], size_t* const out_size) {
  size_t i = 0;
  *out_size = 0;
 redo:
  for (i = 0; i < MAX_LINE_SIZE && off < data_size; ++i) {
    out[i] = data[off++];
    if (out[i] == '\n') break;
  }
  if (off < data_size) {
    if (i == 0) goto redo;         // empty line
    if (out[0] == '#') goto redo;  // skip comment
  }
  out[i] = 0;   // safety sentinel
  *out_size = i;
  return off;
}

static size_t FlagError(const char flag[]) {
  fprintf(stderr, "PAM header error: flags '%s' already seen.\n", flag);
  return 0;
}

// inspired from http://netpbm.sourceforge.net/doc/pam.html
static size_t ReadPAMFields(PNMInfo* const info, size_t off) {
  char out[MAX_LINE_SIZE + 1];
  size_t out_size;
  int tmp;
  int expected_depth = -1;
  assert(info != NULL);
  while (1) {
    off = ReadLine(info->data, off, info->data_size, out, &out_size);
    if (off == 0) return 0;
    if (sscanf(out, "WIDTH %d", &tmp) == 1) {
      if (info->seen_flags & WIDTH_FLAG) return FlagError("WIDTH");
      info->seen_flags |= WIDTH_FLAG;
      info->width = tmp;
    } else if (sscanf(out, "HEIGHT %d", &tmp) == 1) {
      if (info->seen_flags & HEIGHT_FLAG) return FlagError("HEIGHT");
      info->seen_flags |= HEIGHT_FLAG;
      info->height = tmp;
    } else if (sscanf(out, "DEPTH %d", &tmp) == 1) {
      if (info->seen_flags & DEPTH_FLAG) return FlagError("DEPTH");
      info->seen_flags |= DEPTH_FLAG;
      info->depth = tmp;
    } else if (sscanf(out, "MAXVAL %d", &tmp) == 1) {
      if (info->seen_flags & MAXVAL_FLAG) return FlagError("MAXVAL");
      info->seen_flags |= MAXVAL_FLAG;
      info->max_value = tmp;
    } else if (!strcmp(out, "TUPLTYPE RGB_ALPHA")) {
      expected_depth = 4;
      info->seen_flags |= TUPLE_FLAG;
    } else if (!strcmp(out, "TUPLTYPE RGB")) {
      expected_depth = 3;
      info->seen_flags |= TUPLE_FLAG;
    } else if (!strcmp(out, "TUPLTYPE GRAYSCALE")) {
      expected_depth = 1;
      info->seen_flags |= TUPLE_FLAG;
    } else if (!strcmp(out, "ENDHDR")) {
      break;
    } else {
      static const char kEllipsis[] = " ...";
      int i;
      if (out_size > 20) sprintf(out + 20 - strlen(kEllipsis), kEllipsis);
      for (i = 0; i < (int)strlen(out); ++i) {
        // isprint() might trigger a "char-subscripts" warning if given a char.
        if (!isprint((int)out[i])) out[i] = ' ';
      }
      fprintf(stderr, "PAM header error: unrecognized entry [%s]\n", out);
      return 0;
    }
  }
  if (!(info->seen_flags & ALL_NEEDED_FLAGS)) {
    fprintf(stderr, "PAM header error: missing tags%s%s%s%s\n",
            (info->seen_flags & WIDTH_FLAG) ? "" : " WIDTH",
            (info->seen_flags & HEIGHT_FLAG) ? "" : " HEIGHT",
            (info->seen_flags & DEPTH_FLAG) ? "" : " DEPTH",
            (info->seen_flags & MAXVAL_FLAG) ? "" : " MAXVAL");
    return 0;
  }
  if (expected_depth != -1 && info->depth != expected_depth) {
    fprintf(stderr, "PAM header error: expected DEPTH %d but got DEPTH %d\n",
            expected_depth, info->depth);
    return 0;
  }
  return off;
}

static size_t ReadHeader(PNMInfo* const info) {
  size_t off = 0;
  char out[MAX_LINE_SIZE + 1];
  size_t out_size;
  if (info == NULL) return 0;
  if (info->data == NULL || info->data_size < kMinPNMHeaderSize) return 0;

  info->width = info->height = 0;
  info->type = -1;
  info->seen_flags = 0;
  info->bytes_per_px = 0;
  info->depth = 0;
  info->max_value = 0;

  off = ReadLine(info->data, off, info->data_size, out, &out_size);
  if (off == 0 || sscanf(out, "P%d", &info->type) != 1) return 0;
  if (info->type == 7) {
    off = ReadPAMFields(info, off);
  } else {
    off = ReadLine(info->data, off, info->data_size, out, &out_size);
    if (off == 0 || sscanf(out, "%d %d", &info->width, &info->height) != 2) {
      return 0;
    }
    off = ReadLine(info->data, off, info->data_size, out, &out_size);
    if (off == 0 || sscanf(out, "%d", &info->max_value) != 1) return 0;

    // finish initializing missing fields
    info->depth = (info->type == 5) ? 1 : 3;
  }
  // perform some basic numerical validation
  if (info->width <= 0 || info->height <= 0 ||
      info->type <= 0 || info->type >= 9 ||
      info->depth <= 0 || info->depth == 2 || info->depth > 4 ||
      info->max_value <= 0 || info->max_value >= 65536) {
    return 0;
  }
  info->bytes_per_px = info->depth * (info->max_value > 255 ? 2 : 1);
  return off;
}

int ReadPNM(const uint8_t* const data, size_t data_size,
            WebPPicture* const pic, int keep_alpha,
            struct Metadata* const metadata) {
  int ok = 0;
  int i, j;
  uint64_t stride, pixel_bytes;
  uint8_t* rgb = NULL, *tmp_rgb;
  size_t offset;
  PNMInfo info;

  info.data = data;
  info.data_size = data_size;
  offset = ReadHeader(&info);
  if (offset == 0) {
    fprintf(stderr, "Error parsing PNM header.\n");
    goto End;
  }

  if (info.type < 5 || info.type > 7) {
    fprintf(stderr, "Unsupported P%d PNM format.\n", info.type);
    goto End;
  }

  // Some basic validations.
  if (pic == NULL) goto End;
  if (info.width > WEBP_MAX_DIMENSION || info.height > WEBP_MAX_DIMENSION) {
    fprintf(stderr, "Invalid %dx%d dimension for PNM\n",
                    info.width, info.height);
    goto End;
  }

  pixel_bytes = (uint64_t)info.width * info.height * info.bytes_per_px;
  if (data_size < offset + pixel_bytes) {
    fprintf(stderr, "Truncated PNM file (P%d).\n", info.type);
    goto End;
  }
  stride =
      (uint64_t)(info.bytes_per_px < 3 ? 3 : info.bytes_per_px) * info.width;
  if (stride != (size_t)stride ||
      !ImgIoUtilCheckSizeArgumentsOverflow(stride, info.height)) {
    goto End;
  }

  rgb = (uint8_t*)malloc((size_t)stride * info.height);
  if (rgb == NULL) goto End;

  // Convert input
  tmp_rgb = rgb;
  for (j = 0; j < info.height; ++j) {
    assert(offset + info.bytes_per_px * info.width <= data_size);
    if (info.depth == 1) {
      // convert grayscale -> RGB
      for (i = 0; i < info.width; ++i) {
        const uint8_t v = data[offset + i];
        tmp_rgb[3 * i + 0] = tmp_rgb[3 * i + 1] = tmp_rgb[3 * i + 2] = v;
      }
    } else if (info.depth == 3) {   // RGB
      memcpy(tmp_rgb, data + offset, 3 * info.width * sizeof(*data));
    } else if (info.depth == 4) {   // RGBA
      memcpy(tmp_rgb, data + offset, 4 * info.width * sizeof(*data));
    }
    offset += info.bytes_per_px * info.width;
    tmp_rgb += stride;
  }

  // WebP conversion.
  pic->width = info.width;
  pic->height = info.height;
  ok = (info.depth == 4) ? WebPPictureImportRGBA(pic, rgb, (int)stride)
                         : WebPPictureImportRGB(pic, rgb, (int)stride);
  if (!ok) goto End;

  ok = 1;
 End:
  free((void*)rgb);

  (void)metadata;
  (void)keep_alpha;
  return ok;
}

// -----------------------------------------------------------------------------
