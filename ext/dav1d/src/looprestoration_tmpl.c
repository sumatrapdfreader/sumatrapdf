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

#include <stdlib.h>

#include "common/intops.h"

#include "src/looprestoration.h"
#include "src/tables.h"

// 256 * 1.5 + 3 + 3 = 390
#define REST_UNIT_STRIDE (390)

// TODO Reuse p when no padding is needed (add and remove lpf pixels in p)
// TODO Chroma only requires 2 rows of padding.
static NOINLINE void
padding(pixel *dst, const pixel *p, const ptrdiff_t stride,
        const pixel (*left)[4], const pixel *lpf, int unit_w,
        const int stripe_h, const enum LrEdgeFlags edges)
{
    const int have_left = !!(edges & LR_HAVE_LEFT);
    const int have_right = !!(edges & LR_HAVE_RIGHT);

    // Copy more pixels if we don't have to pad them
    unit_w += 3 * have_left + 3 * have_right;
    pixel *dst_l = dst + 3 * !have_left;
    p -= 3 * have_left;
    lpf -= 3 * have_left;

    if (edges & LR_HAVE_TOP) {
        // Copy previous loop filtered rows
        const pixel *const above_1 = lpf;
        const pixel *const above_2 = above_1 + PXSTRIDE(stride);
        pixel_copy(dst_l, above_1, unit_w);
        pixel_copy(dst_l + REST_UNIT_STRIDE, above_1, unit_w);
        pixel_copy(dst_l + 2 * REST_UNIT_STRIDE, above_2, unit_w);
    } else {
        // Pad with first row
        pixel_copy(dst_l, p, unit_w);
        pixel_copy(dst_l + REST_UNIT_STRIDE, p, unit_w);
        pixel_copy(dst_l + 2 * REST_UNIT_STRIDE, p, unit_w);
        if (have_left) {
            pixel_copy(dst_l, &left[0][1], 3);
            pixel_copy(dst_l + REST_UNIT_STRIDE, &left[0][1], 3);
            pixel_copy(dst_l + 2 * REST_UNIT_STRIDE, &left[0][1], 3);
        }
    }

    pixel *dst_tl = dst_l + 3 * REST_UNIT_STRIDE;
    if (edges & LR_HAVE_BOTTOM) {
        // Copy next loop filtered rows
        const pixel *const below_1 = lpf + 6 * PXSTRIDE(stride);
        const pixel *const below_2 = below_1 + PXSTRIDE(stride);
        pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, below_1, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, below_2, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, below_2, unit_w);
    } else {
        // Pad with last row
        const pixel *const src = p + (stripe_h - 1) * PXSTRIDE(stride);
        pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, src, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, src, unit_w);
        if (have_left) {
            pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, &left[stripe_h - 1][1], 3);
            pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, &left[stripe_h - 1][1], 3);
            pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, &left[stripe_h - 1][1], 3);
        }
    }

    // Inner UNIT_WxSTRIPE_H
    for (int j = 0; j < stripe_h; j++) {
        pixel_copy(dst_tl + 3 * have_left, p + 3 * have_left, unit_w - 3 * have_left);
        dst_tl += REST_UNIT_STRIDE;
        p += PXSTRIDE(stride);
    }

    if (!have_right) {
        pixel *pad = dst_l + unit_w;
        pixel *row_last = &dst_l[unit_w - 1];
        // Pad 3x(STRIPE_H+6) with last column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(pad, *row_last, 3);
            pad += REST_UNIT_STRIDE;
            row_last += REST_UNIT_STRIDE;
        }
    }

    if (!have_left) {
        // Pad 3x(STRIPE_H+6) with first column
        for (int j = 0; j < stripe_h + 6; j++) {
            pixel_set(dst, *dst_l, 3);
            dst += REST_UNIT_STRIDE;
            dst_l += REST_UNIT_STRIDE;
        }
    } else {
        dst += 3 * REST_UNIT_STRIDE;
        for (int j = 0; j < stripe_h; j++) {
            pixel_copy(dst, &left[j][1], 3);
            dst += REST_UNIT_STRIDE;
        }
    }
}

// FIXME Could split into luma and chroma specific functions,
// (since first and last tops are always 0 for chroma)
// FIXME Could implement a version that requires less temporary memory
// (should be possible to implement with only 6 rows of temp storage)
static void wiener_c(pixel *p, const ptrdiff_t stride,
                     const pixel (*const left)[4],
                     const pixel *lpf, const int w, const int h,
                     const LooprestorationParams *const params,
                     const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    // Wiener filtering is applied to a maximum stripe height of 64 + 3 pixels
    // of padding above and below
    pixel tmp[70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE];
    pixel *tmp_ptr = tmp;

    padding(tmp, p, stride, left, lpf, w, h, edges);

    // Values stored between horizontal and vertical filtering don't
    // fit in a uint8_t.
    uint16_t hor[70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE];
    uint16_t *hor_ptr = hor;

    const int16_t (*const filter)[8] = params->filter;
    const int bitdepth = bitdepth_from_max(bitdepth_max);
    const int round_bits_h = 3 + (bitdepth == 12) * 2;
    const int rounding_off_h = 1 << (round_bits_h - 1);
    const int clip_limit = 1 << (bitdepth + 1 + 7 - round_bits_h);
    for (int j = 0; j < h + 6; j++) {
        for (int i = 0; i < w; i++) {
            int sum = (1 << (bitdepth + 6));
#if BITDEPTH == 8
            sum += tmp_ptr[i + 3] * 128;
#endif

            for (int k = 0; k < 7; k++) {
                sum += tmp_ptr[i + k] * filter[0][k];
            }

            hor_ptr[i] =
                iclip((sum + rounding_off_h) >> round_bits_h, 0, clip_limit - 1);
        }
        tmp_ptr += REST_UNIT_STRIDE;
        hor_ptr += REST_UNIT_STRIDE;
    }

    const int round_bits_v = 11 - (bitdepth == 12) * 2;
    const int rounding_off_v = 1 << (round_bits_v - 1);
    const int round_offset = 1 << (bitdepth + (round_bits_v - 1));
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int sum = -round_offset;

            for (int k = 0; k < 7; k++) {
                sum += hor[(j + k) * REST_UNIT_STRIDE + i] * filter[1][k];
            }

            p[j * PXSTRIDE(stride) + i] =
                iclip_pixel((sum + rounding_off_v) >> round_bits_v);
        }
    }
}

// Sum over a 3x3 area
// The dst and src pointers are positioned 3 pixels above and 3 pixels to the
// left of the top left corner. However, the self guided filter only needs 1
// pixel above and one pixel to the left. As for the pixels below and to the
// right they must be computed in the sums, but don't need to be stored.
//
// Example for a 4x4 block:
//      x x x x x x x x x x
//      x c c c c c c c c x
//      x i s s s s s s i x
//      x i s s s s s s i x
//      x i s s s s s s i x
//      x i s s s s s s i x
//      x i s s s s s s i x
//      x i s s s s s s i x
//      x c c c c c c c c x
//      x x x x x x x x x x
//
// s: Pixel summed and stored
// i: Pixel summed and stored (between loops)
// c: Pixel summed not stored
// x: Pixel not summed not stored
static void boxsum3(int32_t *sumsq, coef *sum, const pixel *src,
                    const int w, const int h)
{
    // We skip the first row, as it is never used
    src += REST_UNIT_STRIDE;

    // We skip the first and last columns, as they are never used
    for (int x = 1; x < w - 1; x++) {
        coef *sum_v = sum + x;
        int32_t *sumsq_v = sumsq + x;
        const pixel *s = src + x;
        int a = s[0], a2 = a * a;
        int b = s[REST_UNIT_STRIDE], b2 = b * b;

        // We skip the first 2 rows, as they are skipped in the next loop and
        // we don't need the last 2 row as it is skipped in the next loop
        for (int y = 2; y < h - 2; y++) {
            s += REST_UNIT_STRIDE;
            const int c = s[REST_UNIT_STRIDE];
            const int c2 = c * c;
            sum_v += REST_UNIT_STRIDE;
            sumsq_v += REST_UNIT_STRIDE;
            *sum_v = a + b + c;
            *sumsq_v = a2 + b2 + c2;
            a = b;
            a2 = b2;
            b = c;
            b2 = c2;
        }
     }

    // We skip the first row as it is never read
    sum += REST_UNIT_STRIDE;
    sumsq += REST_UNIT_STRIDE;
    // We skip the last 2 rows as it is never read
    for (int y = 2; y < h - 2; y++) {
        int a = sum[1], a2 = sumsq[1];
        int b = sum[2], b2 = sumsq[2];

        // We don't store the first column as it is never read and
        // we don't store the last 2 columns as they are never read
        for (int x = 2; x < w - 2; x++) {
            const int c = sum[x + 1], c2 = sumsq[x + 1];
            sum[x] = a + b + c;
            sumsq[x] = a2 + b2 + c2;
            a = b;
            a2 = b2;
            b = c;
            b2 = c2;
        }
        sum += REST_UNIT_STRIDE;
        sumsq += REST_UNIT_STRIDE;
    }
}

// Sum over a 5x5 area
// The dst and src pointers are positioned 3 pixels above and 3 pixels to the
// left of the top left corner. However, the self guided filter only needs 1
// pixel above and one pixel to the left. As for the pixels below and to the
// right they must be computed in the sums, but don't need to be stored.
//
// Example for a 4x4 block:
//      c c c c c c c c c c
//      c c c c c c c c c c
//      i i s s s s s s i i
//      i i s s s s s s i i
//      i i s s s s s s i i
//      i i s s s s s s i i
//      i i s s s s s s i i
//      i i s s s s s s i i
//      c c c c c c c c c c
//      c c c c c c c c c c
//
// s: Pixel summed and stored
// i: Pixel summed and stored (between loops)
// c: Pixel summed not stored
// x: Pixel not summed not stored
static void boxsum5(int32_t *sumsq, coef *sum, const pixel *const src,
                    const int w, const int h)
{
    for (int x = 0; x < w; x++) {
        coef *sum_v = sum + x;
        int32_t *sumsq_v = sumsq + x;
        const pixel *s = src + 3 * REST_UNIT_STRIDE + x;
        int a = s[-3 * REST_UNIT_STRIDE], a2 = a * a;
        int b = s[-2 * REST_UNIT_STRIDE], b2 = b * b;
        int c = s[-1 * REST_UNIT_STRIDE], c2 = c * c;
        int d = s[0], d2 = d * d;

        // We skip the first 2 rows, as they are skipped in the next loop and
        // we don't need the last 2 row as it is skipped in the next loop
        for (int y = 2; y < h - 2; y++) {
            s += REST_UNIT_STRIDE;
            const int e = *s, e2 = e * e;
            sum_v += REST_UNIT_STRIDE;
            sumsq_v += REST_UNIT_STRIDE;
            *sum_v = a + b + c + d + e;
            *sumsq_v = a2 + b2 + c2 + d2 + e2;
            a = b;
            b = c;
            c = d;
            d = e;
            a2 = b2;
            b2 = c2;
            c2 = d2;
            d2 = e2;
        }
    }

    // We skip the first row as it is never read
    sum += REST_UNIT_STRIDE;
    sumsq += REST_UNIT_STRIDE;
    for (int y = 2; y < h - 2; y++) {
        int a = sum[0], a2 = sumsq[0];
        int b = sum[1], b2 = sumsq[1];
        int c = sum[2], c2 = sumsq[2];
        int d = sum[3], d2 = sumsq[3];

        for (int x = 2; x < w - 2; x++) {
            const int e = sum[x + 2], e2 = sumsq[x + 2];
            sum[x] = a + b + c + d + e;
            sumsq[x] = a2 + b2 + c2 + d2 + e2;
            a = b;
            b = c;
            c = d;
            d = e;
            a2 = b2;
            b2 = c2;
            c2 = d2;
            d2 = e2;
        }
        sum += REST_UNIT_STRIDE;
        sumsq += REST_UNIT_STRIDE;
    }
}

static NOINLINE void
selfguided_filter(coef *dst, const pixel *src, const ptrdiff_t src_stride,
                  const int w, const int h, const int n, const unsigned s
                  HIGHBD_DECL_SUFFIX)
{
    const unsigned sgr_one_by_x = n == 25 ? 164 : 455;

    // Selfguided filter is applied to a maximum stripe height of 64 + 3 pixels
    // of padding above and below
    int32_t sumsq[68 /*(64 + 2 + 2)*/ * REST_UNIT_STRIDE];
    int32_t *A = sumsq + 2 * REST_UNIT_STRIDE + 3;
    // By inverting A and B after the boxsums, B can be of size coef instead
    // of int32_t
    coef sum[68 /*(64 + 2 + 2)*/ * REST_UNIT_STRIDE];
    coef *B = sum + 2 * REST_UNIT_STRIDE + 3;

    const int step = (n == 25) + 1;
    if (n == 25)
        boxsum5(sumsq, sum, src, w + 6, h + 6);
    else
        boxsum3(sumsq, sum, src, w + 6, h + 6);
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;

    int32_t *AA = A - REST_UNIT_STRIDE;
    coef *BB = B - REST_UNIT_STRIDE;
    for (int j = -1; j < h + 1; j+= step) {
        for (int i = -1; i < w + 1; i++) {
            const int a =
                (AA[i] + ((1 << (2 * bitdepth_min_8)) >> 1)) >> (2 * bitdepth_min_8);
            const int b =
                (BB[i] + ((1 << bitdepth_min_8) >> 1)) >> bitdepth_min_8;

            const unsigned p = imax(a * n - b * b, 0);
            const unsigned z = (p * s + (1 << 19)) >> 20;
            const unsigned x = dav1d_sgr_x_by_x[umin(z, 255)];

            // This is where we invert A and B, so that B is of size coef.
            AA[i] = (x * BB[i] * sgr_one_by_x + (1 << 11)) >> 12;
            BB[i] = x;
        }
        AA += step * REST_UNIT_STRIDE;
        BB += step * REST_UNIT_STRIDE;
    }

    src += 3 * REST_UNIT_STRIDE + 3;
    if (n == 25) {
        int j = 0;
#define SIX_NEIGHBORS(P, i)\
    ((P[i - REST_UNIT_STRIDE]     + P[i + REST_UNIT_STRIDE]) * 6 +   \
     (P[i - 1 - REST_UNIT_STRIDE] + P[i - 1 + REST_UNIT_STRIDE] +    \
      P[i + 1 - REST_UNIT_STRIDE] + P[i + 1 + REST_UNIT_STRIDE]) * 5)
        for (; j < h - 1; j+=2) {
            for (int i = 0; i < w; i++) {
                const int a = SIX_NEIGHBORS(B, i);
                const int b = SIX_NEIGHBORS(A, i);
                dst[i] = (b - a * src[i] + (1 << 8)) >> 9;
            }
            dst += 384 /* Maximum restoration width is 384 (256 * 1.5) */;
            src += REST_UNIT_STRIDE;
            B += REST_UNIT_STRIDE;
            A += REST_UNIT_STRIDE;
            for (int i = 0; i < w; i++) {
                const int a = B[i] * 6 + (B[i - 1] + B[i + 1]) * 5;
                const int b = A[i] * 6 + (A[i - 1] + A[i + 1]) * 5;
                dst[i] = (b - a * src[i] + (1 << 7)) >> 8;
            }
            dst += 384 /* Maximum restoration width is 384 (256 * 1.5) */;
            src += REST_UNIT_STRIDE;
            B += REST_UNIT_STRIDE;
            A += REST_UNIT_STRIDE;
        }
        if (j + 1 == h) { // Last row, when number of rows is odd
            for (int i = 0; i < w; i++) {
                const int a = SIX_NEIGHBORS(B, i);
                const int b = SIX_NEIGHBORS(A, i);
                dst[i] = (b - a * src[i] + (1 << 8)) >> 9;
            }
        }
#undef SIX_NEIGHBORS
    } else {
#define EIGHT_NEIGHBORS(P, i)\
    ((P[i] + P[i - 1] + P[i + 1] + P[i - REST_UNIT_STRIDE] + P[i + REST_UNIT_STRIDE]) * 4 + \
     (P[i - 1 - REST_UNIT_STRIDE] + P[i - 1 + REST_UNIT_STRIDE] +                           \
      P[i + 1 - REST_UNIT_STRIDE] + P[i + 1 + REST_UNIT_STRIDE]) * 3)
        for (int j = 0; j < h; j++) {
            for (int i = 0; i < w; i++) {
                const int a = EIGHT_NEIGHBORS(B, i);
                const int b = EIGHT_NEIGHBORS(A, i);
                dst[i] = (b - a * src[i] + (1 << 8)) >> 9;
            }
            dst += 384;
            src += REST_UNIT_STRIDE;
            B += REST_UNIT_STRIDE;
            A += REST_UNIT_STRIDE;
        }
    }
#undef EIGHT_NEIGHBORS
}

static void sgr_5x5_c(pixel *p, const ptrdiff_t stride,
                      const pixel (*const left)[4], const pixel *lpf,
                      const int w, const int h,
                      const LooprestorationParams *const params,
                      const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    // Selfguided filter is applied to a maximum stripe height of 64 + 3 pixels
    // of padding above and below
    pixel tmp[70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE];

    // Selfguided filter outputs to a maximum stripe height of 64 and a
    // maximum restoration width of 384 (256 * 1.5)
    coef dst[64 * 384];

    padding(tmp, p, stride, left, lpf, w, h, edges);
    selfguided_filter(dst, tmp, REST_UNIT_STRIDE, w, h, 25,
                      params->sgr.s0 HIGHBD_TAIL_SUFFIX);

    const int w0 = params->sgr.w0;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            const int v = w0 * dst[j * 384 + i];
            p[i] = iclip_pixel(p[i] + ((v + (1 << 10)) >> 11));
        }
        p += PXSTRIDE(stride);
    }
}

static void sgr_3x3_c(pixel *p, const ptrdiff_t stride,
                      const pixel (*const left)[4], const pixel *lpf,
                      const int w, const int h,
                      const LooprestorationParams *const params,
                      const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    pixel tmp[70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE];
    coef dst[64 * 384];

    padding(tmp, p, stride, left, lpf, w, h, edges);
    selfguided_filter(dst, tmp, REST_UNIT_STRIDE, w, h, 9,
                      params->sgr.s1 HIGHBD_TAIL_SUFFIX);

    const int w1 = params->sgr.w1;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            const int v = w1 * dst[j * 384 + i];
            p[i] = iclip_pixel(p[i] + ((v + (1 << 10)) >> 11));
        }
        p += PXSTRIDE(stride);
    }
}

static void sgr_mix_c(pixel *p, const ptrdiff_t stride,
                      const pixel (*const left)[4], const pixel *lpf,
                      const int w, const int h,
                      const LooprestorationParams *const params,
                      const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    pixel tmp[70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE];
    coef dst0[64 * 384];
    coef dst1[64 * 384];

    padding(tmp, p, stride, left, lpf, w, h, edges);
    selfguided_filter(dst0, tmp, REST_UNIT_STRIDE, w, h, 25,
                      params->sgr.s0 HIGHBD_TAIL_SUFFIX);
    selfguided_filter(dst1, tmp, REST_UNIT_STRIDE, w, h,  9,
                      params->sgr.s1 HIGHBD_TAIL_SUFFIX);

    const int w0 = params->sgr.w0;
    const int w1 = params->sgr.w1;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            const int v = w0 * dst0[j * 384 + i] + w1 * dst1[j * 384 + i];
            p[i] = iclip_pixel(p[i] + ((v + (1 << 10)) >> 11));
        }
        p += PXSTRIDE(stride);
    }
}

COLD void bitfn(dav1d_loop_restoration_dsp_init)(Dav1dLoopRestorationDSPContext *const c,
                                                 const int bpc)
{
    c->wiener[0] = c->wiener[1] = wiener_c;
    c->sgr[0] = sgr_5x5_c;
    c->sgr[1] = sgr_3x3_c;
    c->sgr[2] = sgr_mix_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    bitfn(dav1d_loop_restoration_dsp_init_arm)(c, bpc);
#elif ARCH_PPC64LE
    bitfn(dav1d_loop_restoration_dsp_init_ppc)(c, bpc);
#elif ARCH_X86
    bitfn(dav1d_loop_restoration_dsp_init_x86)(c, bpc);
#endif
#endif
}
