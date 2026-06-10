/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdint.h>
#include <string.h>

#include "common/intops.h"

#include "src/wedge.h"

enum WedgeDirectionType {
    WEDGE_HORIZONTAL = 0,
    WEDGE_VERTICAL = 1,
    WEDGE_OBLIQUE27 = 2,
    WEDGE_OBLIQUE63 = 3,
    WEDGE_OBLIQUE117 = 4,
    WEDGE_OBLIQUE153 = 5,
    N_WEDGE_DIRECTIONS
};

typedef struct {
    uint8_t /* enum WedgeDirectionType */ direction;
    uint8_t x_offset;
    uint8_t y_offset;
} wedge_code_type;

static const wedge_code_type wedge_codebook_16_hgtw[16] = {
    { WEDGE_OBLIQUE27,  4, 4 }, { WEDGE_OBLIQUE63,  4, 4 },
    { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
    { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 4 },
    { WEDGE_HORIZONTAL, 4, 6 }, { WEDGE_VERTICAL,   4, 4 },
    { WEDGE_OBLIQUE27,  4, 2 }, { WEDGE_OBLIQUE27,  4, 6 },
    { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
    { WEDGE_OBLIQUE63,  2, 4 }, { WEDGE_OBLIQUE63,  6, 4 },
    { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static const wedge_code_type wedge_codebook_16_hltw[16] = {
    { WEDGE_OBLIQUE27,  4, 4 }, { WEDGE_OBLIQUE63,  4, 4 },
    { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
    { WEDGE_VERTICAL,   2, 4 }, { WEDGE_VERTICAL,   4, 4 },
    { WEDGE_VERTICAL,   6, 4 }, { WEDGE_HORIZONTAL, 4, 4 },
    { WEDGE_OBLIQUE27,  4, 2 }, { WEDGE_OBLIQUE27,  4, 6 },
    { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
    { WEDGE_OBLIQUE63,  2, 4 }, { WEDGE_OBLIQUE63,  6, 4 },
    { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static const wedge_code_type wedge_codebook_16_heqw[16] = {
    { WEDGE_OBLIQUE27,  4, 4 }, { WEDGE_OBLIQUE63,  4, 4 },
    { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
    { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 6 },
    { WEDGE_VERTICAL,   2, 4 }, { WEDGE_VERTICAL,   6, 4 },
    { WEDGE_OBLIQUE27,  4, 2 }, { WEDGE_OBLIQUE27,  4, 6 },
    { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
    { WEDGE_OBLIQUE63,  2, 4 }, { WEDGE_OBLIQUE63,  6, 4 },
    { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

Dav1dMasks dav1d_masks;

static void insert_border(uint8_t *const dst, const uint8_t *const src,
                          const int ctr)
{
    if (ctr > 4) memset(dst, 0, ctr - 4);
    memcpy(dst + imax(ctr, 4) - 4, src + imax(4 - ctr, 0), imin(64 - ctr, 8));
    if (ctr < 64 - 4)
        memset(dst + ctr + 4, 64, 64 - 4 - ctr);
}

static void transpose(uint8_t *const dst, const uint8_t *const src) {
    for (int y = 0, y_off = 0; y < 64; y++, y_off += 64)
        for (int x = 0, x_off = 0; x < 64; x++, x_off += 64)
            dst[x_off + y] = src[y_off + x];
}

static void hflip(uint8_t *const dst, const uint8_t *const src) {
    for (int y = 0, y_off = 0; y < 64; y++, y_off += 64)
        for (int x = 0; x < 64; x++)
            dst[y_off + 64 - 1 - x] = src[y_off + x];
}

static void copy2d(uint8_t *dst, const uint8_t *src, int sign,
                   const int w, const int h, const int x_off, const int y_off)
{
    src += y_off * 64 + x_off;
    if (sign) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++)
                dst[x] = 64 - src[x];
            src += 64;
            dst += w;
        }
    } else {
        for (int y = 0; y < h; y++) {
            memcpy(dst, src, w);
            src += 64;
            dst += w;
        }
    }
}

#define MASK_OFFSET(x) ((uint16_t)(((uintptr_t)(x) - (uintptr_t)&dav1d_masks) >> 3))

static COLD uint16_t init_chroma(uint8_t *chroma, const uint8_t *luma,
                                 const int sign, const int w, const int h,
                                 const int ss_ver)
{
    const uint16_t offset = MASK_OFFSET(chroma);
    for (int y = 0; y < h; y += 1 + ss_ver) {
        for (int x = 0; x < w; x += 2) {
            int sum = luma[x] + luma[x + 1] + 1;
            if (ss_ver) sum += luma[w + x] + luma[w + x + 1] + 1;
            chroma[x >> 1] = (sum - sign) >> (1 + ss_ver);
        }
        luma += w << ss_ver;
        chroma += w >> 1;
    }
    return offset;
}

static COLD void fill2d_16x2(const int w, const int h, const enum BlockSize bs,
                             const uint8_t (*const master)[64 * 64],
                             const wedge_code_type *const cb,
                             uint8_t *masks_444, uint8_t *masks_422,
                             uint8_t *masks_420, unsigned signs)
{
    const int n_stride_444 = (w * h);
    const int n_stride_422 = n_stride_444 >> 1;
    const int n_stride_420 = n_stride_444 >> 2;
    const int sign_stride_422 = 16 * n_stride_422;
    const int sign_stride_420 = 16 * n_stride_420;

    // assign pointer offsets in lookup table
    for (int n = 0; n < 16; n++) {
        const int sign = signs & 1;

        copy2d(masks_444, master[cb[n].direction], sign, w, h,
               32 - (w * cb[n].x_offset >> 3), 32 - (h * cb[n].y_offset >> 3));

        // not using !sign is intentional here, since 444 does not require
        // any rounding since no chroma subsampling is applied.
        dav1d_masks.offsets[0][bs].wedge[0][n] =
        dav1d_masks.offsets[0][bs].wedge[1][n] = MASK_OFFSET(masks_444);

        dav1d_masks.offsets[1][bs].wedge[0][n] =
            init_chroma(&masks_422[ sign * sign_stride_422], masks_444, 0, w, h, 0);
        dav1d_masks.offsets[1][bs].wedge[1][n] =
            init_chroma(&masks_422[!sign * sign_stride_422], masks_444, 1, w, h, 0);
        dav1d_masks.offsets[2][bs].wedge[0][n] =
            init_chroma(&masks_420[ sign * sign_stride_420], masks_444, 0, w, h, 1);
        dav1d_masks.offsets[2][bs].wedge[1][n] =
            init_chroma(&masks_420[!sign * sign_stride_420], masks_444, 1, w, h, 1);

        signs >>= 1;
        masks_444 += n_stride_444;
        masks_422 += n_stride_422;
        masks_420 += n_stride_420;
    }
}

static COLD void build_nondc_ii_masks(uint8_t *const mask_v, const int w,
                                      const int h, const int step)
{
    static const uint8_t ii_weights_1d[32] = {
        60, 52, 45, 39, 34, 30, 26, 22, 19, 17, 15, 13, 11, 10,  8,  7,
         6,  6,  5,  4,  4,  3,  3,  2,  2,  2,  2,  1,  1,  1,  1,  1,
    };

    uint8_t *const mask_h  = &mask_v[w * h];
    uint8_t *const mask_sm = &mask_h[w * h];
    for (int y = 0, off = 0; y < h; y++, off += w) {
        memset(&mask_v[off], ii_weights_1d[y * step], w);
        for (int x = 0; x < w; x++) {
            mask_sm[off + x] = ii_weights_1d[imin(x, y) * step];
            mask_h[off + x] = ii_weights_1d[x * step];
        }
    }
}

COLD void dav1d_init_ii_wedge_masks(void) {
    // This function is guaranteed to be called only once

    enum WedgeMasterLineType {
        WEDGE_MASTER_LINE_ODD,
        WEDGE_MASTER_LINE_EVEN,
        WEDGE_MASTER_LINE_VERT,
        N_WEDGE_MASTER_LINES,
    };
    static const uint8_t wedge_master_border[N_WEDGE_MASTER_LINES][8] = {
        [WEDGE_MASTER_LINE_ODD]  = {  1,  2,  6, 18, 37, 53, 60, 63 },
        [WEDGE_MASTER_LINE_EVEN] = {  1,  4, 11, 27, 46, 58, 62, 63 },
        [WEDGE_MASTER_LINE_VERT] = {  0,  2,  7, 21, 43, 57, 62, 64 },
    };
    uint8_t master[6][64 * 64];

    // create master templates
    for (int y = 0, off = 0; y < 64; y++, off += 64)
        insert_border(&master[WEDGE_VERTICAL][off],
                      wedge_master_border[WEDGE_MASTER_LINE_VERT], 32);
    for (int y = 0, off = 0, ctr = 48; y < 64; y += 2, off += 128, ctr--)
    {
        insert_border(&master[WEDGE_OBLIQUE63][off],
                      wedge_master_border[WEDGE_MASTER_LINE_EVEN], ctr);
        insert_border(&master[WEDGE_OBLIQUE63][off + 64],
                      wedge_master_border[WEDGE_MASTER_LINE_ODD], ctr - 1);
    }

    transpose(master[WEDGE_OBLIQUE27], master[WEDGE_OBLIQUE63]);
    transpose(master[WEDGE_HORIZONTAL], master[WEDGE_VERTICAL]);
    hflip(master[WEDGE_OBLIQUE117], master[WEDGE_OBLIQUE63]);
    hflip(master[WEDGE_OBLIQUE153], master[WEDGE_OBLIQUE27]);

#define fill(w, h, sz_422, sz_420, hvsw, signs) \
    fill2d_16x2(w, h, BS_##w##x##h - BS_32x32, \
                master, wedge_codebook_16_##hvsw, \
                dav1d_masks.wedge_444_##w##x##h, \
                dav1d_masks.wedge_422_##sz_422, \
                dav1d_masks.wedge_420_##sz_420, signs)

    fill(32, 32, 16x32, 16x16, heqw, 0x7bfb);
    fill(32, 16, 16x16, 16x8,  hltw, 0x7beb);
    fill(32,  8, 16x8,  16x4,  hltw, 0x6beb);
    fill(16, 32,  8x32,  8x16, hgtw, 0x7beb);
    fill(16, 16,  8x16,  8x8,  heqw, 0x7bfb);
    fill(16,  8,  8x8,   8x4,  hltw, 0x7beb);
    fill( 8, 32,  4x32,  4x16, hgtw, 0x7aeb);
    fill( 8, 16,  4x16,  4x8,  hgtw, 0x7beb);
    fill( 8,  8,  4x8,   4x4,  heqw, 0x7bfb);
#undef fill

    memset(dav1d_masks.ii_dc, 32, 32 * 32);
    for (int c = 0; c < 3; c++) {
        dav1d_masks.offsets[c][BS_32x32-BS_32x32].ii[II_DC_PRED] =
        dav1d_masks.offsets[c][BS_32x16-BS_32x32].ii[II_DC_PRED] =
        dav1d_masks.offsets[c][BS_16x32-BS_32x32].ii[II_DC_PRED] =
        dav1d_masks.offsets[c][BS_16x16-BS_32x32].ii[II_DC_PRED] =
        dav1d_masks.offsets[c][BS_16x8 -BS_32x32].ii[II_DC_PRED] =
        dav1d_masks.offsets[c][BS_8x16 -BS_32x32].ii[II_DC_PRED] =
        dav1d_masks.offsets[c][BS_8x8  -BS_32x32].ii[II_DC_PRED] =
            MASK_OFFSET(dav1d_masks.ii_dc);
    }

#define BUILD_NONDC_II_MASKS(w, h, step) \
    build_nondc_ii_masks(dav1d_masks.ii_nondc_##w##x##h, w, h, step)

#define ASSIGN_NONDC_II_OFFSET(bs, w444, h444, w422, h422, w420, h420) \
    dav1d_masks.offsets[0][bs-BS_32x32].ii[p + 1] = \
        MASK_OFFSET(&dav1d_masks.ii_nondc_##w444##x##h444[p*w444*h444]); \
    dav1d_masks.offsets[1][bs-BS_32x32].ii[p + 1] = \
        MASK_OFFSET(&dav1d_masks.ii_nondc_##w422##x##h422[p*w422*h422]); \
    dav1d_masks.offsets[2][bs-BS_32x32].ii[p + 1] = \
        MASK_OFFSET(&dav1d_masks.ii_nondc_##w420##x##h420[p*w420*h420])

    BUILD_NONDC_II_MASKS(32, 32, 1);
    BUILD_NONDC_II_MASKS(16, 32, 1);
    BUILD_NONDC_II_MASKS(16, 16, 2);
    BUILD_NONDC_II_MASKS( 8, 32, 1);
    BUILD_NONDC_II_MASKS( 8, 16, 2);
    BUILD_NONDC_II_MASKS( 8,  8, 4);
    BUILD_NONDC_II_MASKS( 4, 16, 2);
    BUILD_NONDC_II_MASKS( 4,  8, 4);
    BUILD_NONDC_II_MASKS( 4,  4, 8);
    for (int p = 0; p < 3; p++) {
        ASSIGN_NONDC_II_OFFSET(BS_32x32, 32, 32, 16, 32, 16, 16);
        ASSIGN_NONDC_II_OFFSET(BS_32x16, 32, 32, 16, 16, 16, 16);
        ASSIGN_NONDC_II_OFFSET(BS_16x32, 16, 32,  8, 32,  8, 16);
        ASSIGN_NONDC_II_OFFSET(BS_16x16, 16, 16,  8, 16,  8,  8);
        ASSIGN_NONDC_II_OFFSET(BS_16x8,  16, 16,  8,  8,  8,  8);
        ASSIGN_NONDC_II_OFFSET(BS_8x16,   8, 16,  4, 16,  4,  8);
        ASSIGN_NONDC_II_OFFSET(BS_8x8,    8,  8,  4,  8,  4,  4);
    }
}
