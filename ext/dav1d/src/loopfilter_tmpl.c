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

#include "common/attributes.h"
#include "common/intops.h"

#include "src/loopfilter.h"

static NOINLINE void
loop_filter(pixel *dst, int E, int I, int H,
            const ptrdiff_t stridea, const ptrdiff_t strideb, const int wd
            HIGHBD_DECL_SUFFIX)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int F = 1 << bitdepth_min_8;
    E <<= bitdepth_min_8;
    I <<= bitdepth_min_8;
    H <<= bitdepth_min_8;

    for (int i = 0; i < 4; i++, dst += stridea) {
        int p6, p5, p4, p3, p2;
        int p1 = dst[strideb * -2], p0 = dst[strideb * -1];
        int q0 = dst[strideb * +0], q1 = dst[strideb * +1];
        int q2, q3, q4, q5, q6;
        int fm, flat8out, flat8in;

        fm = abs(p1 - p0) <= I && abs(q1 - q0) <= I &&
             abs(p0 - q0) * 2 + (abs(p1 - q1) >> 1) <= E;

        if (wd > 4) {
            p2 = dst[strideb * -3];
            q2 = dst[strideb * +2];

            fm &= abs(p2 - p1) <= I && abs(q2 - q1) <= I;

            if (wd > 6) {
                p3 = dst[strideb * -4];
                q3 = dst[strideb * +3];

                fm &= abs(p3 - p2) <= I && abs(q3 - q2) <= I;
            }
        }
        if (!fm) continue;

        if (wd >= 16) {
            p6 = dst[strideb * -7];
            p5 = dst[strideb * -6];
            p4 = dst[strideb * -5];
            q4 = dst[strideb * +4];
            q5 = dst[strideb * +5];
            q6 = dst[strideb * +6];

            flat8out = abs(p6 - p0) <= F && abs(p5 - p0) <= F &&
                       abs(p4 - p0) <= F && abs(q4 - q0) <= F &&
                       abs(q5 - q0) <= F && abs(q6 - q0) <= F;
        }

        if (wd >= 6)
            flat8in = abs(p2 - p0) <= F && abs(p1 - p0) <= F &&
                      abs(q1 - q0) <= F && abs(q2 - q0) <= F;

        if (wd >= 8)
            flat8in &= abs(p3 - p0) <= F && abs(q3 - q0) <= F;

        if (wd >= 16 && (flat8out & flat8in)) {
            dst[strideb * -6] = (p6 + p6 + p6 + p6 + p6 + p6 * 2 + p5 * 2 +
                                 p4 * 2 + p3 + p2 + p1 + p0 + q0 + 8) >> 4;
            dst[strideb * -5] = (p6 + p6 + p6 + p6 + p6 + p5 * 2 + p4 * 2 +
                                 p3 * 2 + p2 + p1 + p0 + q0 + q1 + 8) >> 4;
            dst[strideb * -4] = (p6 + p6 + p6 + p6 + p5 + p4 * 2 + p3 * 2 +
                                 p2 * 2 + p1 + p0 + q0 + q1 + q2 + 8) >> 4;
            dst[strideb * -3] = (p6 + p6 + p6 + p5 + p4 + p3 * 2 + p2 * 2 +
                                 p1 * 2 + p0 + q0 + q1 + q2 + q3 + 8) >> 4;
            dst[strideb * -2] = (p6 + p6 + p5 + p4 + p3 + p2 * 2 + p1 * 2 +
                                 p0 * 2 + q0 + q1 + q2 + q3 + q4 + 8) >> 4;
            dst[strideb * -1] = (p6 + p5 + p4 + p3 + p2 + p1 * 2 + p0 * 2 +
                                 q0 * 2 + q1 + q2 + q3 + q4 + q5 + 8) >> 4;
            dst[strideb * +0] = (p5 + p4 + p3 + p2 + p1 + p0 * 2 + q0 * 2 +
                                 q1 * 2 + q2 + q3 + q4 + q5 + q6 + 8) >> 4;
            dst[strideb * +1] = (p4 + p3 + p2 + p1 + p0 + q0 * 2 + q1 * 2 +
                                 q2 * 2 + q3 + q4 + q5 + q6 + q6 + 8) >> 4;
            dst[strideb * +2] = (p3 + p2 + p1 + p0 + q0 + q1 * 2 + q2 * 2 +
                                 q3 * 2 + q4 + q5 + q6 + q6 + q6 + 8) >> 4;
            dst[strideb * +3] = (p2 + p1 + p0 + q0 + q1 + q2 * 2 + q3 * 2 +
                                 q4 * 2 + q5 + q6 + q6 + q6 + q6 + 8) >> 4;
            dst[strideb * +4] = (p1 + p0 + q0 + q1 + q2 + q3 * 2 + q4 * 2 +
                                 q5 * 2 + q6 + q6 + q6 + q6 + q6 + 8) >> 4;
            dst[strideb * +5] = (p0 + q0 + q1 + q2 + q3 + q4 * 2 + q5 * 2 +
                                 q6 * 2 + q6 + q6 + q6 + q6 + q6 + 8) >> 4;
        } else if (wd >= 8 && flat8in) {
            dst[strideb * -3] = (p3 + p3 + p3 + 2 * p2 + p1 + p0 + q0 + 4) >> 3;
            dst[strideb * -2] = (p3 + p3 + p2 + 2 * p1 + p0 + q0 + q1 + 4) >> 3;
            dst[strideb * -1] = (p3 + p2 + p1 + 2 * p0 + q0 + q1 + q2 + 4) >> 3;
            dst[strideb * +0] = (p2 + p1 + p0 + 2 * q0 + q1 + q2 + q3 + 4) >> 3;
            dst[strideb * +1] = (p1 + p0 + q0 + 2 * q1 + q2 + q3 + q3 + 4) >> 3;
            dst[strideb * +2] = (p0 + q0 + q1 + 2 * q2 + q3 + q3 + q3 + 4) >> 3;
        } else if (wd == 6 && flat8in) {
            dst[strideb * -2] = (p2 + 2 * p2 + 2 * p1 + 2 * p0 + q0 + 4) >> 3;
            dst[strideb * -1] = (p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3;
            dst[strideb * +0] = (p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3;
            dst[strideb * +1] = (p0 + 2 * q0 + 2 * q1 + 2 * q2 + q2 + 4) >> 3;
        } else {
            const int hev = abs(p1 - p0) > H || abs(q1 - q0) > H;

#define iclip_diff(v) iclip(v, -128 * (1 << bitdepth_min_8), \
                                128 * (1 << bitdepth_min_8) - 1)

            if (hev) {
                int f = iclip_diff(p1 - q1), f1, f2;
                f = iclip_diff(3 * (q0 - p0) + f);

                f1 = imin(f + 4, (128 << bitdepth_min_8) - 1) >> 3;
                f2 = imin(f + 3, (128 << bitdepth_min_8) - 1) >> 3;

                dst[strideb * -1] = iclip_pixel(p0 + f2);
                dst[strideb * +0] = iclip_pixel(q0 - f1);
            } else {
                int f = iclip_diff(3 * (q0 - p0)), f1, f2;

                f1 = imin(f + 4, (128 << bitdepth_min_8) - 1) >> 3;
                f2 = imin(f + 3, (128 << bitdepth_min_8) - 1) >> 3;

                dst[strideb * -1] = iclip_pixel(p0 + f2);
                dst[strideb * +0] = iclip_pixel(q0 - f1);

                f = (f1 + 1) >> 1;
                dst[strideb * -2] = iclip_pixel(p1 + f);
                dst[strideb * +1] = iclip_pixel(q1 - f);
            }
#undef iclip_diff
        }
    }
}

static void loop_filter_h_sb128y_c(pixel *dst, const ptrdiff_t stride,
                                   const uint32_t *const vmask,
                                   const uint8_t (*l)[4], ptrdiff_t b4_stride,
                                   const Av1FilterLUT *lut, const int h
                                   HIGHBD_DECL_SUFFIX)
{
    const unsigned vm = vmask[0] | vmask[1] | vmask[2];
    for (unsigned y = 1; vm & ~(y - 1);
         y <<= 1, dst += 4 * PXSTRIDE(stride), l += b4_stride)
    {
        if (vm & y) {
            const int L = l[0][0] ? l[0][0] : l[-1][0];
            if (!L) continue;
            const int H = L >> 4;
            const int E = lut->e[L], I = lut->i[L];
            const int idx = (vmask[2] & y) ? 2 : !!(vmask[1] & y);
            loop_filter(dst, E, I, H, PXSTRIDE(stride), 1, 4 << idx
                        HIGHBD_TAIL_SUFFIX);
        }
    }
}

static void loop_filter_v_sb128y_c(pixel *dst, const ptrdiff_t stride,
                                   const uint32_t *const vmask,
                                   const uint8_t (*l)[4], ptrdiff_t b4_stride,
                                   const Av1FilterLUT *lut, const int w
                                   HIGHBD_DECL_SUFFIX)
{
    const unsigned vm = vmask[0] | vmask[1] | vmask[2];
    for (unsigned x = 1; vm & ~(x - 1); x <<= 1, dst += 4, l++) {
        if (vm & x) {
            const int L = l[0][0] ? l[0][0] : l[-b4_stride][0];
            if (!L) continue;
            const int H = L >> 4;
            const int E = lut->e[L], I = lut->i[L];
            const int idx = (vmask[2] & x) ? 2 : !!(vmask[1] & x);
            loop_filter(dst, E, I, H, 1, PXSTRIDE(stride), 4 << idx
                        HIGHBD_TAIL_SUFFIX);
        }
    }
}

static void loop_filter_h_sb128uv_c(pixel *dst, const ptrdiff_t stride,
                                    const uint32_t *const vmask,
                                    const uint8_t (*l)[4], ptrdiff_t b4_stride,
                                    const Av1FilterLUT *lut, const int h
                                    HIGHBD_DECL_SUFFIX)
{
    const unsigned vm = vmask[0] | vmask[1];
    for (unsigned y = 1; vm & ~(y - 1);
         y <<= 1, dst += 4 * PXSTRIDE(stride), l += b4_stride)
    {
        if (vm & y) {
            const int L = l[0][0] ? l[0][0] : l[-1][0];
            if (!L) continue;
            const int H = L >> 4;
            const int E = lut->e[L], I = lut->i[L];
            const int idx = !!(vmask[1] & y);
            loop_filter(dst, E, I, H, PXSTRIDE(stride), 1, 4 + 2 * idx
                        HIGHBD_TAIL_SUFFIX);
        }
    }
}

static void loop_filter_v_sb128uv_c(pixel *dst, const ptrdiff_t stride,
                                    const uint32_t *const vmask,
                                    const uint8_t (*l)[4], ptrdiff_t b4_stride,
                                    const Av1FilterLUT *lut, const int w
                                    HIGHBD_DECL_SUFFIX)
{
    const unsigned vm = vmask[0] | vmask[1];
    for (unsigned x = 1; vm & ~(x - 1); x <<= 1, dst += 4, l++) {
        if (vm & x) {
            const int L = l[0][0] ? l[0][0] : l[-b4_stride][0];
            if (!L) continue;
            const int H = L >> 4;
            const int E = lut->e[L], I = lut->i[L];
            const int idx = !!(vmask[1] & x);
            loop_filter(dst, E, I, H, 1, PXSTRIDE(stride), 4 + 2 * idx
                        HIGHBD_TAIL_SUFFIX);
        }
    }
}

COLD void bitfn(dav1d_loop_filter_dsp_init)(Dav1dLoopFilterDSPContext *const c) {
    c->loop_filter_sb[0][0] = loop_filter_h_sb128y_c;
    c->loop_filter_sb[0][1] = loop_filter_v_sb128y_c;
    c->loop_filter_sb[1][0] = loop_filter_h_sb128uv_c;
    c->loop_filter_sb[1][1] = loop_filter_v_sb128uv_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    bitfn(dav1d_loop_filter_dsp_init_arm)(c);
#elif ARCH_X86
    bitfn(dav1d_loop_filter_dsp_init_x86)(c);
#endif
#endif
}
