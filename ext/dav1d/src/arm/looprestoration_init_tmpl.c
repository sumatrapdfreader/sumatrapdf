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

#include "src/cpu.h"
#include "src/looprestoration.h"

#if ARCH_AARCH64
void BF(dav1d_wiener_filter7, neon)(pixel *p, const ptrdiff_t stride,
                                    const pixel (*left)[4], const pixel *lpf,
                                    const int w, int h,
                                    const LooprestorationParams *const params,
                                    const enum LrEdgeFlags edges
                                    HIGHBD_DECL_SUFFIX);
void BF(dav1d_wiener_filter5, neon)(pixel *p, const ptrdiff_t stride,
                                    const pixel (*left)[4], const pixel *lpf,
                                    const int w, int h,
                                    const LooprestorationParams *const params,
                                    const enum LrEdgeFlags edges
                                    HIGHBD_DECL_SUFFIX);
#else

// The 8bpc version calculates things slightly differently than the reference
// C version. That version calculates roughly this:
// int16_t sum = 0;
// for (int i = 0; i < 7; i++)
//     sum += src[idx] * fh[i];
// int16_t sum2 = (src[x] << 7) - (1 << (bitdepth + 6)) + rounding_off_h;
// sum = iclip(sum + sum2, INT16_MIN, INT16_MAX) >> round_bits_h;
// sum += 1 << (bitdepth + 6 - round_bits_h);
// Compared to the reference C version, this is the output of the first pass
// _subtracted_ by 1 << (bitdepth + 6 - round_bits_h) = 2048, i.e.
// with round_offset precompensated.
// The 16bpc version calculates things pretty much the same way as the
// reference C version, but with the end result subtracted by
// 1 << (bitdepth + 6 - round_bits_h).
void BF(dav1d_wiener_filter_h, neon)(int16_t *dst, const pixel (*left)[4],
                                     const pixel *src, ptrdiff_t stride,
                                     const int16_t fh[8], intptr_t w,
                                     int h, enum LrEdgeFlags edges
                                     HIGHBD_DECL_SUFFIX);
// This calculates things slightly differently than the reference C version.
// This version calculates roughly this:
// int32_t sum = 0;
// for (int i = 0; i < 7; i++)
//     sum += mid[idx] * fv[i];
// sum = (sum + rounding_off_v) >> round_bits_v;
// This function assumes that the width is a multiple of 8.
void BF(dav1d_wiener_filter_v, neon)(pixel *dst, ptrdiff_t stride,
                                     const int16_t *mid, int w, int h,
                                     const int16_t fv[8], enum LrEdgeFlags edges,
                                     ptrdiff_t mid_stride HIGHBD_DECL_SUFFIX);

static void wiener_filter_neon(pixel *const dst, const ptrdiff_t stride,
                               const pixel (*const left)[4], const pixel *lpf,
                               const int w, const int h,
                               const LooprestorationParams *const params,
                               const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    const int16_t (*const filter)[8] = params->filter;
    ALIGN_STK_16(int16_t, mid, 68 * 384,);
    int mid_stride = (w + 7) & ~7;

    // Horizontal filter
    BF(dav1d_wiener_filter_h, neon)(&mid[2 * mid_stride], left, dst, stride,
                                    filter[0], w, h, edges HIGHBD_TAIL_SUFFIX);
    if (edges & LR_HAVE_TOP)
        BF(dav1d_wiener_filter_h, neon)(mid, NULL, lpf, stride,
                                        filter[0], w, 2, edges
                                        HIGHBD_TAIL_SUFFIX);
    if (edges & LR_HAVE_BOTTOM)
        BF(dav1d_wiener_filter_h, neon)(&mid[(2 + h) * mid_stride], NULL,
                                        lpf + 6 * PXSTRIDE(stride),
                                        stride, filter[0], w, 2, edges
                                        HIGHBD_TAIL_SUFFIX);

    // Vertical filter
    BF(dav1d_wiener_filter_v, neon)(dst, stride, &mid[2*mid_stride],
                                    w, h, filter[1], edges,
                                    mid_stride * sizeof(*mid)
                                    HIGHBD_TAIL_SUFFIX);
}
#endif

void BF(dav1d_sgr_box3_h, neon)(int32_t *sumsq, int16_t *sum,
                                const pixel (*left)[4],
                                const pixel *src, const ptrdiff_t stride,
                                const int w, const int h,
                                const enum LrEdgeFlags edges);
void dav1d_sgr_box3_v_neon(int32_t *sumsq, int16_t *sum,
                           const int w, const int h,
                           const enum LrEdgeFlags edges);
void dav1d_sgr_calc_ab1_neon(int32_t *a, int16_t *b,
                             const int w, const int h, const int strength,
                             const int bitdepth_max);
void BF(dav1d_sgr_finish_filter1, neon)(int16_t *tmp,
                                        const pixel *src, const ptrdiff_t stride,
                                        const int32_t *a, const int16_t *b,
                                        const int w, const int h);

/* filter with a 3x3 box (radius=1) */
static void dav1d_sgr_filter1_neon(int16_t *tmp,
                                   const pixel *src, const ptrdiff_t stride,
                                   const pixel (*left)[4], const pixel *lpf,
                                   const int w, const int h, const int strength,
                                   const enum LrEdgeFlags edges
                                   HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int32_t, sumsq_mem, (384 + 16) * 68 + 8,);
    int32_t *const sumsq = &sumsq_mem[(384 + 16) * 2 + 8], *const a = sumsq;
    ALIGN_STK_16(int16_t, sum_mem, (384 + 16) * 68 + 16,);
    int16_t *const sum = &sum_mem[(384 + 16) * 2 + 16], *const b = sum;

    BF(dav1d_sgr_box3_h, neon)(sumsq, sum, left, src, stride, w, h, edges);
    if (edges & LR_HAVE_TOP)
        BF(dav1d_sgr_box3_h, neon)(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)],
                                   NULL, lpf, stride, w, 2, edges);

    if (edges & LR_HAVE_BOTTOM)
        BF(dav1d_sgr_box3_h, neon)(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)],
                                   NULL, lpf + 6 * PXSTRIDE(stride),
                                   stride, w, 2, edges);

    dav1d_sgr_box3_v_neon(sumsq, sum, w, h, edges);
    dav1d_sgr_calc_ab1_neon(a, b, w, h, strength, BITDEPTH_MAX);
    BF(dav1d_sgr_finish_filter1, neon)(tmp, src, stride, a, b, w, h);
}

void BF(dav1d_sgr_box5_h, neon)(int32_t *sumsq, int16_t *sum,
                                const pixel (*left)[4],
                                const pixel *src, const ptrdiff_t stride,
                                const int w, const int h,
                                const enum LrEdgeFlags edges);
void dav1d_sgr_box5_v_neon(int32_t *sumsq, int16_t *sum,
                           const int w, const int h,
                           const enum LrEdgeFlags edges);
void dav1d_sgr_calc_ab2_neon(int32_t *a, int16_t *b,
                             const int w, const int h, const int strength,
                             const int bitdepth_max);
void BF(dav1d_sgr_finish_filter2, neon)(int16_t *tmp,
                                        const pixel *src, const ptrdiff_t stride,
                                        const int32_t *a, const int16_t *b,
                                        const int w, const int h);

/* filter with a 5x5 box (radius=2) */
static void dav1d_sgr_filter2_neon(int16_t *tmp,
                                   const pixel *src, const ptrdiff_t stride,
                                   const pixel (*left)[4], const pixel *lpf,
                                   const int w, const int h, const int strength,
                                   const enum LrEdgeFlags edges
                                   HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int32_t, sumsq_mem, (384 + 16) * 68 + 8,);
    int32_t *const sumsq = &sumsq_mem[(384 + 16) * 2 + 8], *const a = sumsq;
    ALIGN_STK_16(int16_t, sum_mem, (384 + 16) * 68 + 16,);
    int16_t *const sum = &sum_mem[(384 + 16) * 2 + 16], *const b = sum;

    BF(dav1d_sgr_box5_h, neon)(sumsq, sum, left, src, stride, w, h, edges);
    if (edges & LR_HAVE_TOP)
        BF(dav1d_sgr_box5_h, neon)(&sumsq[-2 * (384 + 16)], &sum[-2 * (384 + 16)],
                                   NULL, lpf, stride, w, 2, edges);

    if (edges & LR_HAVE_BOTTOM)
        BF(dav1d_sgr_box5_h, neon)(&sumsq[h * (384 + 16)], &sum[h * (384 + 16)],
                                   NULL, lpf + 6 * PXSTRIDE(stride),
                                   stride, w, 2, edges);

    dav1d_sgr_box5_v_neon(sumsq, sum, w, h, edges);
    dav1d_sgr_calc_ab2_neon(a, b, w, h, strength, BITDEPTH_MAX);
    BF(dav1d_sgr_finish_filter2, neon)(tmp, src, stride, a, b, w, h);
}

void BF(dav1d_sgr_weighted1, neon)(pixel *dst, const ptrdiff_t dst_stride,
                                   const pixel *src, const ptrdiff_t src_stride,
                                   const int16_t *t1, const int w, const int h,
                                   const int wt HIGHBD_DECL_SUFFIX);
void BF(dav1d_sgr_weighted2, neon)(pixel *dst, const ptrdiff_t dst_stride,
                                   const pixel *src, const ptrdiff_t src_stride,
                                   const int16_t *t1, const int16_t *t2,
                                   const int w, const int h,
                                   const int16_t wt[2] HIGHBD_DECL_SUFFIX);

static void sgr_filter_5x5_neon(pixel *const dst, const ptrdiff_t stride,
                                const pixel (*const left)[4], const pixel *lpf,
                                const int w, const int h,
                                const LooprestorationParams *const params,
                                const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int16_t, tmp, 64 * 384,);
    dav1d_sgr_filter2_neon(tmp, dst, stride, left, lpf,
                           w, h, params->sgr.s0, edges HIGHBD_TAIL_SUFFIX);
    BF(dav1d_sgr_weighted1, neon)(dst, stride, dst, stride,
                                  tmp, w, h, params->sgr.w0 HIGHBD_TAIL_SUFFIX);
}

static void sgr_filter_3x3_neon(pixel *const dst, const ptrdiff_t stride,
                                const pixel (*const left)[4], const pixel *lpf,
                                const int w, const int h,
                                const LooprestorationParams *const params,
                                const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int16_t, tmp, 64 * 384,);
    dav1d_sgr_filter1_neon(tmp, dst, stride, left, lpf,
                           w, h, params->sgr.s1, edges HIGHBD_TAIL_SUFFIX);
    BF(dav1d_sgr_weighted1, neon)(dst, stride, dst, stride,
                                  tmp, w, h, params->sgr.w1 HIGHBD_TAIL_SUFFIX);
}

static void sgr_filter_mix_neon(pixel *const dst, const ptrdiff_t stride,
                                const pixel (*const left)[4], const pixel *lpf,
                                const int w, const int h,
                                const LooprestorationParams *const params,
                                const enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX)
{
    ALIGN_STK_16(int16_t, tmp1, 64 * 384,);
    ALIGN_STK_16(int16_t, tmp2, 64 * 384,);
    dav1d_sgr_filter2_neon(tmp1, dst, stride, left, lpf,
                           w, h, params->sgr.s0, edges HIGHBD_TAIL_SUFFIX);
    dav1d_sgr_filter1_neon(tmp2, dst, stride, left, lpf,
                           w, h, params->sgr.s1, edges HIGHBD_TAIL_SUFFIX);
    const int16_t wt[2] = { params->sgr.w0, params->sgr.w1 };
    BF(dav1d_sgr_weighted2, neon)(dst, stride, dst, stride,
                                  tmp1, tmp2, w, h, wt HIGHBD_TAIL_SUFFIX);
}

COLD void bitfn(dav1d_loop_restoration_dsp_init_arm)(Dav1dLoopRestorationDSPContext *const c, int bpc) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

#if ARCH_AARCH64
    c->wiener[0] = BF(dav1d_wiener_filter7, neon);
    c->wiener[1] = BF(dav1d_wiener_filter5, neon);
#else
    c->wiener[0] = c->wiener[1] = wiener_filter_neon;
#endif
    if (bpc <= 10) {
        c->sgr[0] = sgr_filter_5x5_neon;
        c->sgr[1] = sgr_filter_3x3_neon;
        c->sgr[2] = sgr_filter_mix_neon;
    }
}
