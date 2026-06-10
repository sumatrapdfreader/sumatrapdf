/*
 * Copyright © 2018, Niklas Haas
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

#include "common/attributes.h"
#include "common/intops.h"

#include "src/filmgrain.h"
#include "src/tables.h"

#define SUB_GRAIN_WIDTH 44
#define SUB_GRAIN_HEIGHT 38

static inline int get_random_number(const int bits, unsigned *const state) {
    const int r = *state;
    unsigned bit = ((r >> 0) ^ (r >> 1) ^ (r >> 3) ^ (r >> 12)) & 1;
    *state = (r >> 1) | (bit << 15);

    return (*state >> (16 - bits)) & ((1 << bits) - 1);
}

static inline int round2(const int x, const uint64_t shift) {
    return (x + ((1 << shift) >> 1)) >> shift;
}

static void generate_grain_y_c(entry buf[][GRAIN_WIDTH],
                               const Dav1dFilmGrainData *const data
                               HIGHBD_DECL_SUFFIX)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    unsigned seed = data->seed;
    const int shift = 4 - bitdepth_min_8 + data->grain_scale_shift;
    const int grain_ctr = 128 << bitdepth_min_8;
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    for (int y = 0; y < GRAIN_HEIGHT; y++) {
        for (int x = 0; x < GRAIN_WIDTH; x++) {
            const int value = get_random_number(11, &seed);
            buf[y][x] = round2(dav1d_gaussian_sequence[ value ], shift);
        }
    }

    const int ar_pad = 3;
    const int ar_lag = data->ar_coeff_lag;

    for (int y = ar_pad; y < GRAIN_HEIGHT; y++) {
        for (int x = ar_pad; x < GRAIN_WIDTH - ar_pad; x++) {
            const int8_t *coeff = data->ar_coeffs_y;
            int sum = 0;
            for (int dy = -ar_lag; dy <= 0; dy++) {
                for (int dx = -ar_lag; dx <= ar_lag; dx++) {
                    if (!dx && !dy)
                        break;
                    sum += *(coeff++) * buf[y + dy][x + dx];
                }
            }

            const int grain = buf[y][x] + round2(sum, data->ar_coeff_shift);
            buf[y][x] = iclip(grain, grain_min, grain_max);
        }
    }
}

static NOINLINE void
generate_grain_uv_c(entry buf[][GRAIN_WIDTH],
                    const entry buf_y[][GRAIN_WIDTH],
                    const Dav1dFilmGrainData *const data, const intptr_t uv,
                    const int subx, const int suby HIGHBD_DECL_SUFFIX)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    unsigned seed = data->seed ^ (uv ? 0x49d8 : 0xb524);
    const int shift = 4 - bitdepth_min_8 + data->grain_scale_shift;
    const int grain_ctr = 128 << bitdepth_min_8;
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    const int chromaW = subx ? SUB_GRAIN_WIDTH  : GRAIN_WIDTH;
    const int chromaH = suby ? SUB_GRAIN_HEIGHT : GRAIN_HEIGHT;

    for (int y = 0; y < chromaH; y++) {
        for (int x = 0; x < chromaW; x++) {
            const int value = get_random_number(11, &seed);
            buf[y][x] = round2(dav1d_gaussian_sequence[ value ], shift);
        }
    }

    const int ar_pad = 3;
    const int ar_lag = data->ar_coeff_lag;

    for (int y = ar_pad; y < chromaH; y++) {
        for (int x = ar_pad; x < chromaW - ar_pad; x++) {
            const int8_t *coeff = data->ar_coeffs_uv[uv];
            int sum = 0;
            for (int dy = -ar_lag; dy <= 0; dy++) {
                for (int dx = -ar_lag; dx <= ar_lag; dx++) {
                    // For the final (current) pixel, we need to add in the
                    // contribution from the luma grain texture
                    if (!dx && !dy) {
                        if (!data->num_y_points)
                            break;
                        int luma = 0;
                        const int lumaX = ((x - ar_pad) << subx) + ar_pad;
                        const int lumaY = ((y - ar_pad) << suby) + ar_pad;
                        for (int i = 0; i <= suby; i++) {
                            for (int j = 0; j <= subx; j++) {
                                luma += buf_y[lumaY + i][lumaX + j];
                            }
                        }
                        luma = round2(luma, subx + suby);
                        sum += luma * (*coeff);
                        break;
                    }

                    sum += *(coeff++) * buf[y + dy][x + dx];
                }
            }

            const int grain = buf[y][x] + round2(sum, data->ar_coeff_shift);
            buf[y][x] = iclip(grain, grain_min, grain_max);
        }
    }
}

#define gnuv_ss_fn(nm, ss_x, ss_y) \
static decl_generate_grain_uv_fn(generate_grain_uv_##nm##_c) { \
    generate_grain_uv_c(buf, buf_y, data, uv, ss_x, ss_y HIGHBD_TAIL_SUFFIX); \
}

gnuv_ss_fn(420, 1, 1);
gnuv_ss_fn(422, 1, 0);
gnuv_ss_fn(444, 0, 0);

// samples from the correct block of a grain LUT, while taking into account the
// offsets provided by the offsets cache
static inline entry sample_lut(const entry grain_lut[][GRAIN_WIDTH],
                               const int offsets[2][2], const int subx, const int suby,
                               const int bx, const int by, const int x, const int y)
{
    const int randval = offsets[bx][by];
    const int offx = 3 + (2 >> subx) * (3 + (randval >> 4));
    const int offy = 3 + (2 >> suby) * (3 + (randval & 0xF));
    return grain_lut[offy + y + (FG_BLOCK_SIZE >> suby) * by]
                    [offx + x + (FG_BLOCK_SIZE >> subx) * bx];
}

static void fgy_32x32xn_c(pixel *const dst_row, const pixel *const src_row,
                          const ptrdiff_t stride,
                          const Dav1dFilmGrainData *const data, const size_t pw,
                          const uint8_t scaling[SCALING_SIZE],
                          const entry grain_lut[][GRAIN_WIDTH],
                          const int bh, const int row_num HIGHBD_DECL_SUFFIX)
{
    const int rows = 1 + (data->overlap_flag && row_num > 0);
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int grain_ctr = 128 << bitdepth_min_8;
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    int min_value, max_value;
    if (data->clip_to_restricted_range) {
        min_value = 16 << bitdepth_min_8;
        max_value = 235 << bitdepth_min_8;
    } else {
        min_value = 0;
        max_value = BITDEPTH_MAX;
    }

    // seed[0] contains the current row, seed[1] contains the previous
    unsigned seed[2];
    for (int i = 0; i < rows; i++) {
        seed[i] = data->seed;
        seed[i] ^= (((row_num - i) * 37  + 178) & 0xFF) << 8;
        seed[i] ^= (((row_num - i) * 173 + 105) & 0xFF);
    }

    assert(stride % (FG_BLOCK_SIZE * sizeof(pixel)) == 0);

    int offsets[2 /* col offset */][2 /* row offset */];

    // process this row in FG_BLOCK_SIZE^2 blocks
    for (unsigned bx = 0; bx < pw; bx += FG_BLOCK_SIZE) {
        const int bw = imin(FG_BLOCK_SIZE, (int) pw - bx);

        if (data->overlap_flag && bx) {
            // shift previous offsets left
            for (int i = 0; i < rows; i++)
                offsets[1][i] = offsets[0][i];
        }

        // update current offsets
        for (int i = 0; i < rows; i++)
            offsets[0][i] = get_random_number(8, &seed[i]);

        // x/y block offsets to compensate for overlapped regions
        const int ystart = data->overlap_flag && row_num ? imin(2, bh) : 0;
        const int xstart = data->overlap_flag && bx      ? imin(2, bw) : 0;

        static const int w[2][2] = { { 27, 17 }, { 17, 27 } };

#define add_noise_y(x, y, grain)                                                  \
        const pixel *const src = src_row + (y) * PXSTRIDE(stride) + (x) + bx;     \
        pixel *const dst = dst_row + (y) * PXSTRIDE(stride) + (x) + bx;           \
        const int noise = round2(scaling[ *src ] * (grain), data->scaling_shift); \
        *dst = iclip(*src + noise, min_value, max_value);

        for (int y = ystart; y < bh; y++) {
            // Non-overlapped image region (straightforward)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                add_noise_y(x, y, grain);
            }

            // Special case for overlapped column
            for (int x = 0; x < xstart; x++) {
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, 0, 0, 1, 0, x, y);
                grain = round2(old * w[x][0] + grain * w[x][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_y(x, y, grain);
            }
        }

        for (int y = 0; y < ystart; y++) {
            // Special case for overlapped row (sans corner)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, 0, 0, 0, 1, x, y);
                grain = round2(old * w[y][0] + grain * w[y][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_y(x, y, grain);
            }

            // Special case for doubly-overlapped corner
            for (int x = 0; x < xstart; x++) {
                // Blend the top pixel with the top left block
                int top = sample_lut(grain_lut, offsets, 0, 0, 0, 1, x, y);
                int old = sample_lut(grain_lut, offsets, 0, 0, 1, 1, x, y);
                top = round2(old * w[x][0] + top * w[x][1], 5);
                top = iclip(top, grain_min, grain_max);

                // Blend the current pixel with the left block
                int grain = sample_lut(grain_lut, offsets, 0, 0, 0, 0, x, y);
                old = sample_lut(grain_lut, offsets, 0, 0, 1, 0, x, y);
                grain = round2(old * w[x][0] + grain * w[x][1], 5);
                grain = iclip(grain, grain_min, grain_max);

                // Mix the row rows together and apply grain
                grain = round2(top * w[y][0] + grain * w[y][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_y(x, y, grain);
            }
        }
    }
}

static NOINLINE void
fguv_32x32xn_c(pixel *const dst_row, const pixel *const src_row,
               const ptrdiff_t stride, const Dav1dFilmGrainData *const data,
               const size_t pw, const uint8_t scaling[SCALING_SIZE],
               const entry grain_lut[][GRAIN_WIDTH], const int bh,
               const int row_num, const pixel *const luma_row,
               const ptrdiff_t luma_stride, const int uv, const int is_id,
               const int sx, const int sy HIGHBD_DECL_SUFFIX)
{
    const int rows = 1 + (data->overlap_flag && row_num > 0);
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int grain_ctr = 128 << bitdepth_min_8;
    const int grain_min = -grain_ctr, grain_max = grain_ctr - 1;

    int min_value, max_value;
    if (data->clip_to_restricted_range) {
        min_value = 16 << bitdepth_min_8;
        max_value = (is_id ? 235 : 240) << bitdepth_min_8;
    } else {
        min_value = 0;
        max_value = BITDEPTH_MAX;
    }

    // seed[0] contains the current row, seed[1] contains the previous
    unsigned seed[2];
    for (int i = 0; i < rows; i++) {
        seed[i] = data->seed;
        seed[i] ^= (((row_num - i) * 37  + 178) & 0xFF) << 8;
        seed[i] ^= (((row_num - i) * 173 + 105) & 0xFF);
    }

    assert(stride % (FG_BLOCK_SIZE * sizeof(pixel)) == 0);

    int offsets[2 /* col offset */][2 /* row offset */];

    // process this row in FG_BLOCK_SIZE^2 blocks (subsampled)
    for (unsigned bx = 0; bx < pw; bx += FG_BLOCK_SIZE >> sx) {
        const int bw = imin(FG_BLOCK_SIZE >> sx, (int)(pw - bx));
        if (data->overlap_flag && bx) {
            // shift previous offsets left
            for (int i = 0; i < rows; i++)
                offsets[1][i] = offsets[0][i];
        }

        // update current offsets
        for (int i = 0; i < rows; i++)
            offsets[0][i] = get_random_number(8, &seed[i]);

        // x/y block offsets to compensate for overlapped regions
        const int ystart = data->overlap_flag && row_num ? imin(2 >> sy, bh) : 0;
        const int xstart = data->overlap_flag && bx      ? imin(2 >> sx, bw) : 0;

        static const int w[2 /* sub */][2 /* off */][2] = {
            { { 27, 17 }, { 17, 27 } },
            { { 23, 22 } },
        };

#define add_noise_uv(x, y, grain)                                                    \
            const int lx = (bx + x) << sx;                                           \
            const int ly = y << sy;                                                  \
            const pixel *const luma = luma_row + ly * PXSTRIDE(luma_stride) + lx;    \
            pixel avg = luma[0];                                                     \
            if (sx)                                                                  \
                avg = (avg + luma[1] + 1) >> 1;                                      \
            const pixel *const src = src_row + (y) * PXSTRIDE(stride) + (bx + (x));  \
            pixel *const dst = dst_row + (y) * PXSTRIDE(stride) + (bx + (x));        \
            int val = avg;                                                           \
            if (!data->chroma_scaling_from_luma) {                                   \
                const int combined = avg * data->uv_luma_mult[uv] +                  \
                               *src * data->uv_mult[uv];                             \
                val = iclip_pixel( (combined >> 6) +                                 \
                                   (data->uv_offset[uv] * (1 << bitdepth_min_8)) );  \
            }                                                                        \
            const int noise = round2(scaling[ val ] * (grain), data->scaling_shift); \
            *dst = iclip(*src + noise, min_value, max_value);

        for (int y = ystart; y < bh; y++) {
            // Non-overlapped image region (straightforward)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                add_noise_uv(x, y, grain);
            }

            // Special case for overlapped column
            for (int x = 0; x < xstart; x++) {
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, sx, sy, 1, 0, x, y);
                grain = round2(old * w[sx][x][0] + grain * w[sx][x][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_uv(x, y, grain);
            }
        }

        for (int y = 0; y < ystart; y++) {
            // Special case for overlapped row (sans corner)
            for (int x = xstart; x < bw; x++) {
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                int old   = sample_lut(grain_lut, offsets, sx, sy, 0, 1, x, y);
                grain = round2(old * w[sy][y][0] + grain * w[sy][y][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_uv(x, y, grain);
            }

            // Special case for doubly-overlapped corner
            for (int x = 0; x < xstart; x++) {
                // Blend the top pixel with the top left block
                int top = sample_lut(grain_lut, offsets, sx, sy, 0, 1, x, y);
                int old = sample_lut(grain_lut, offsets, sx, sy, 1, 1, x, y);
                top = round2(old * w[sx][x][0] + top * w[sx][x][1], 5);
                top = iclip(top, grain_min, grain_max);

                // Blend the current pixel with the left block
                int grain = sample_lut(grain_lut, offsets, sx, sy, 0, 0, x, y);
                old = sample_lut(grain_lut, offsets, sx, sy, 1, 0, x, y);
                grain = round2(old * w[sx][x][0] + grain * w[sx][x][1], 5);
                grain = iclip(grain, grain_min, grain_max);

                // Mix the row rows together and apply to image
                grain = round2(top * w[sy][y][0] + grain * w[sy][y][1], 5);
                grain = iclip(grain, grain_min, grain_max);
                add_noise_uv(x, y, grain);
            }
        }
    }
}

#define fguv_ss_fn(nm, ss_x, ss_y) \
static decl_fguv_32x32xn_fn(fguv_32x32xn_##nm##_c) { \
    fguv_32x32xn_c(dst_row, src_row, stride, data, pw, scaling, grain_lut, bh, \
                   row_num, luma_row, luma_stride, uv_pl, is_id, ss_x, ss_y \
                   HIGHBD_TAIL_SUFFIX); \
}

fguv_ss_fn(420, 1, 1);
fguv_ss_fn(422, 1, 0);
fguv_ss_fn(444, 0, 0);

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
#include "src/arm/filmgrain.h"
#elif ARCH_X86
#include "src/x86/filmgrain.h"
#endif
#endif

COLD void bitfn(dav1d_film_grain_dsp_init)(Dav1dFilmGrainDSPContext *const c) {
    c->generate_grain_y = generate_grain_y_c;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I420 - 1] = generate_grain_uv_420_c;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I422 - 1] = generate_grain_uv_422_c;
    c->generate_grain_uv[DAV1D_PIXEL_LAYOUT_I444 - 1] = generate_grain_uv_444_c;

    c->fgy_32x32xn = fgy_32x32xn_c;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I420 - 1] = fguv_32x32xn_420_c;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I422 - 1] = fguv_32x32xn_422_c;
    c->fguv_32x32xn[DAV1D_PIXEL_LAYOUT_I444 - 1] = fguv_32x32xn_444_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    film_grain_dsp_init_arm(c);
#elif ARCH_X86
    film_grain_dsp_init_x86(c);
#endif
#endif
}
