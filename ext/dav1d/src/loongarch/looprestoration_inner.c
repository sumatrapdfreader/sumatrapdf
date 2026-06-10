/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Loongson Technology Corporation Limited
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

#include "src/loongarch/looprestoration.h"

#if BITDEPTH == 8

#define REST_UNIT_STRIDE (400)

void BF(dav1d_wiener_filter_h, lsx)(int32_t *hor_ptr,
                                    uint8_t *tmp_ptr,
                                    const int16_t filterh[8],
                                    const int w, const int h);

void BF(dav1d_wiener_filter_h, lasx)(int32_t *hor_ptr,
                                     uint8_t *tmp_ptr,
                                     const int16_t filterh[8],
                                     const int w, const int h);

void BF(dav1d_wiener_filter_v, lsx)(uint8_t *p,
                                    const ptrdiff_t p_stride,
                                    const int32_t *hor,
                                    const int16_t filterv[8],
                                    const int w, const int h);

void BF(dav1d_wiener_filter_v, lasx)(uint8_t *p,
                                     const ptrdiff_t p_stride,
                                     const int32_t *hor,
                                     const int16_t filterv[8],
                                     const int w, const int h);

// This function refers to the function in the ppc/looprestoration_init_tmpl.c.
static inline void padding(uint8_t *dst, const uint8_t *p,
                           const ptrdiff_t stride, const uint8_t (*left)[4],
                           const uint8_t *lpf, int unit_w, const int stripe_h,
                           const enum LrEdgeFlags edges)
{
    const int have_left = !!(edges & LR_HAVE_LEFT);
    const int have_right = !!(edges & LR_HAVE_RIGHT);

    // Copy more pixels if we don't have to pad them
    unit_w += 3 * have_left + 3 * have_right;
    uint8_t *dst_l = dst + 3 * !have_left;
    p -= 3 * have_left;
    lpf -= 3 * have_left;

    if (edges & LR_HAVE_TOP) {
        // Copy previous loop filtered rows
        const uint8_t *const above_1 = lpf;
        const uint8_t *const above_2 = above_1 + PXSTRIDE(stride);
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

    uint8_t *dst_tl = dst_l + 3 * REST_UNIT_STRIDE;
    if (edges & LR_HAVE_BOTTOM) {
        // Copy next loop filtered rows
        const uint8_t *const below_1 = lpf + 6 * PXSTRIDE(stride);
        const uint8_t *const below_2 = below_1 + PXSTRIDE(stride);
        pixel_copy(dst_tl + stripe_h * REST_UNIT_STRIDE, below_1, unit_w);
        pixel_copy(dst_tl + (stripe_h + 1) * REST_UNIT_STRIDE, below_2, unit_w);
        pixel_copy(dst_tl + (stripe_h + 2) * REST_UNIT_STRIDE, below_2, unit_w);
    } else {
        // Pad with last row
        const uint8_t *const src = p + (stripe_h - 1) * PXSTRIDE(stride);
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
        uint8_t *pad = dst_l + unit_w;
        uint8_t *row_last = &dst_l[unit_w - 1];
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

// This function refers to the function in the ppc/looprestoration_init_tmpl.c.

// FIXME Could split into luma and chroma specific functions,
// (since first and last tops are always 0 for chroma)
// FIXME Could implement a version that requires less temporary memory
// (should be possible to implement with only 6 rows of temp storage)
void dav1d_wiener_filter_lsx(uint8_t *p, const ptrdiff_t p_stride,
                              const uint8_t (*const left)[4],
                              const uint8_t *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    const int16_t (*const filter)[8] = params->filter;

    // Wiener filtering is applied to a maximum stripe height of 64 + 3 pixels
    // of padding above and below
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, p_stride, left, lpf, w, h, edges);
    ALIGN_STK_16(int32_t, hor, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE + 64,);

    BF(dav1d_wiener_filter_h, lsx)(hor, tmp, filter[0], w, h + 6);
    BF(dav1d_wiener_filter_v, lsx)(p, p_stride, hor, filter[1], w, h);
}

void dav1d_wiener_filter_lasx(uint8_t *p, const ptrdiff_t p_stride,
                              const uint8_t (*const left)[4],
                              const uint8_t *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    const int16_t (*const filter)[8] = params->filter;

    // Wiener filtering is applied to a maximum stripe height of 64 + 3 pixels
    // of padding above and below
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, p_stride, left, lpf, w, h, edges);
    ALIGN_STK_16(int32_t, hor, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE + 64,);

    BF(dav1d_wiener_filter_h, lasx)(hor, tmp, filter[0], w, h + 6);
    BF(dav1d_wiener_filter_v, lasx)(p, p_stride, hor, filter[1], w, h);
}

void BF(dav1d_boxsum3_h, lsx)(int32_t *sumsq, int16_t *sum, pixel *src,
                              const int w, const int h);
void BF(dav1d_boxsum3_v, lsx)(int32_t *sumsq, int16_t *sum,
                              const int w, const int h);

void BF(dav1d_boxsum3_sgf_h, lsx)(int32_t *sumsq, int16_t *sum,
                                  const int w, const int h, const int w1);
void BF(dav1d_boxsum3_sgf_v, lsx)(int16_t *dst, uint8_t *tmp,
                                  int32_t *sumsq, int16_t *sum,
                                  const int w, const int h);
void BF(dav1d_sgr_3x3_finish, lsx)(pixel *p, const ptrdiff_t p_stride,
                                   int16_t *dst, int w1,
                                   const int w, const int h);

void BF(dav1d_boxsum3_h, lasx)(int32_t *sumsq, int16_t *sum, pixel *src,
                               const int w, const int h);
void BF(dav1d_boxsum3_sgf_h, lasx)(int32_t *sumsq, int16_t *sum,
                                   const int w, const int h, const int w1);
void BF(dav1d_boxsum3_sgf_v, lasx)(int16_t *dst, uint8_t *tmp,
                                   int32_t *sumsq, int16_t *sum,
                                   const int w, const int h);

static inline void boxsum3_lsx(int32_t *sumsq, coef *sum, pixel *src,
                               const int w, const int h)
{
    BF(dav1d_boxsum3_h, lsx)(sumsq, sum, src, w + 6, h + 6);
    BF(dav1d_boxsum3_v, lsx)(sumsq, sum, w + 6, h + 6);
}

static inline void boxsum3_lasx(int32_t *sumsq, coef *sum, pixel *src,
                               const int w, const int h)
{
    BF(dav1d_boxsum3_h, lasx)(sumsq, sum, src, w + 6, h + 6);
    BF(dav1d_boxsum3_v, lsx)(sumsq, sum, w + 6, h + 6);
}

void dav1d_sgr_filter_3x3_lsx(pixel *p, const ptrdiff_t p_stride,
                              const pixel (*const left)[4],
                              const pixel *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, p_stride, left, lpf, w, h, edges);
    coef dst[64 * 384];

    ALIGN_STK_16(int32_t, sumsq, 68 * REST_UNIT_STRIDE + 8, );
    ALIGN_STK_16(int16_t, sum, 68 * REST_UNIT_STRIDE + 16, );

    boxsum3_lsx(sumsq, sum, tmp, w, h);
    BF(dav1d_boxsum3_sgf_h, lsx)(sumsq, sum, w, h, params->sgr.s1);
    BF(dav1d_boxsum3_sgf_v, lsx)(dst, tmp, sumsq, sum, w, h);
    BF(dav1d_sgr_3x3_finish, lsx)(p, p_stride, dst, params->sgr.w1, w, h);
}

void dav1d_sgr_filter_3x3_lasx(pixel *p, const ptrdiff_t p_stride,
                              const pixel (*const left)[4],
                              const pixel *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, p_stride, left, lpf, w, h, edges);
    coef dst[64 * 384];

    ALIGN_STK_16(int32_t, sumsq, 68 * REST_UNIT_STRIDE + 8, );
    ALIGN_STK_16(int16_t, sum, 68 * REST_UNIT_STRIDE + 16, );

    boxsum3_lasx(sumsq, sum, tmp, w, h);
    BF(dav1d_boxsum3_sgf_h, lasx)(sumsq, sum, w, h, params->sgr.s1);
    BF(dav1d_boxsum3_sgf_v, lasx)(dst, tmp, sumsq, sum, w, h);
    BF(dav1d_sgr_3x3_finish, lsx)(p, p_stride, dst, params->sgr.w1, w, h);
}

void BF(dav1d_boxsum5_h, lsx)(int32_t *sumsq, int16_t *sum,
                              const uint8_t *const src,
                              const int w, const int h);

void BF(dav1d_boxsum5_v, lsx)(int32_t *sumsq, int16_t *sum,
                              const int w, const int h);

void BF(dav1d_boxsum5_sgf_h, lsx)(int32_t *sumsq, int16_t *sum,
                                  const int w, const int h,
                                  const unsigned s);

void BF(dav1d_boxsum5_sgf_v, lsx)(int16_t *dst, uint8_t *src,
                                  int32_t *sumsq, int16_t *sum,
                                  const int w, const int h);

void BF(dav1d_sgr_mix_finish, lsx)(uint8_t *p, const ptrdiff_t stride,
                                   const int16_t *dst0, const int16_t *dst1,
                                   const int w0, const int w1,
                                   const int w, const int h);

static inline void boxsum5_lsx(int32_t *sumsq, coef *sum, pixel *src,
                               const int w, const int h)
{
    BF(dav1d_boxsum5_h, lsx)(sumsq, sum, src, w + 6, h + 6);
    BF(dav1d_boxsum5_v, lsx)(sumsq, sum, w + 6, h + 6);
}

void dav1d_sgr_filter_5x5_lsx(pixel *p, const ptrdiff_t p_stride,
                              const pixel (*const left)[4],
                              const pixel *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, p_stride, left, lpf, w, h, edges);
    coef dst[64 * 384];

    ALIGN_STK_16(int32_t, sumsq, 68 * REST_UNIT_STRIDE + 8, );
    ALIGN_STK_16(int16_t, sum, 68 * REST_UNIT_STRIDE + 16, );

    boxsum5_lsx(sumsq, sum, tmp, w, h);
    BF(dav1d_boxsum5_sgf_h, lsx)(sumsq, sum, w, h, params->sgr.s0);
    BF(dav1d_boxsum5_sgf_v, lsx)(dst, tmp, sumsq, sum, w, h);
    BF(dav1d_sgr_3x3_finish, lsx)(p, p_stride, dst, params->sgr.w0, w, h);
}

void dav1d_sgr_filter_mix_lsx(pixel *p, const ptrdiff_t p_stride,
                              const pixel (*const left)[4],
                              const pixel *lpf,
                              const int w, const int h,
                              const LooprestorationParams *const params,
                              const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(uint8_t, tmp, 70 /*(64 + 3 + 3)*/ * REST_UNIT_STRIDE,);
    padding(tmp, p, p_stride, left, lpf, w, h, edges);
    coef dst0[64 * 384];
    coef dst1[64 * 384];

    ALIGN_STK_16(int32_t, sumsq0, 68 * REST_UNIT_STRIDE + 8, );
    ALIGN_STK_16(int16_t, sum0, 68 * REST_UNIT_STRIDE + 16, );

    boxsum5_lsx(sumsq0, sum0, tmp, w, h);
    BF(dav1d_boxsum5_sgf_h, lsx)(sumsq0, sum0, w, h, params->sgr.s0);
    BF(dav1d_boxsum5_sgf_v, lsx)(dst0, tmp, sumsq0, sum0, w, h);

    boxsum3_lsx(sumsq0, sum0, tmp, w, h);
    BF(dav1d_boxsum3_sgf_h, lsx)(sumsq0, sum0, w, h, params->sgr.s1);
    BF(dav1d_boxsum3_sgf_v, lsx)(dst1, tmp, sumsq0, sum0, w, h);

    BF(dav1d_sgr_mix_finish, lsx)(p, p_stride, dst0, dst1, params->sgr.w0,
                                   params->sgr.w1, w, h);
}
#endif
