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

#include "tests/checkasm/checkasm.h"

#include <string.h>

#include "src/levels.h"
#include "src/loopfilter.h"

static void init_lpf_border(pixel *const dst, const ptrdiff_t stride,
                            int E, int I, const int bitdepth_max)
{
    const int bitdepth_min_8 = bitdepth_from_max(bitdepth_max) - 8;
    const int F = 1 << bitdepth_min_8;
    E <<= bitdepth_min_8;
    I <<= bitdepth_min_8;

    const int filter_type = rnd() % 4;
    const int edge_diff = rnd() % ((E + 2) * 4) - 2 * (E + 2);
    switch (filter_type) {
    case 0: // random, unfiltered
        for (int i = -8; i < 8; i++)
            dst[i * stride] = rnd() & bitdepth_max;
        break;
    case 1: // long flat
        dst[-8 * stride] = rnd() & bitdepth_max;
        dst[+7 * stride] = rnd() & bitdepth_max;
        dst[+0 * stride] = rnd() & bitdepth_max;
        dst[-1 * stride] = iclip_pixel(dst[+0 * stride] + edge_diff);
        for (int i = 1; i < 7; i++) {
            dst[-(1 + i) * stride] = iclip_pixel(dst[-1 * stride] +
                                                 rnd() % (2 * (F + 1)) - (F + 1));
            dst[+(0 + i) * stride] = iclip_pixel(dst[+0 * stride] +
                                                 rnd() % (2 * (F + 1)) - (F + 1));
        }
        break;
    case 2: // short flat
        for (int i = 4; i < 8; i++) {
            dst[-(1 + i) * stride] = rnd() & bitdepth_max;
            dst[+(0 + i) * stride] = rnd() & bitdepth_max;
        }
        dst[+0 * stride] = rnd() & bitdepth_max;
        dst[-1 * stride] = iclip_pixel(dst[+0 * stride] + edge_diff);
        for (int i = 1; i < 4; i++) {
            dst[-(1 + i) * stride] = iclip_pixel(dst[-1 * stride] +
                                                 rnd() % (2 * (F + 1)) - (F + 1));
            dst[+(0 + i) * stride] = iclip_pixel(dst[+0 * stride] +
                                                 rnd() % (2 * (F + 1)) - (F + 1));
        }
        break;
    case 3: // normal or hev
        for (int i = 4; i < 8; i++) {
            dst[-(1 + i) * stride] = rnd() & bitdepth_max;
            dst[+(0 + i) * stride] = rnd() & bitdepth_max;
        }
        dst[+0 * stride] = rnd() & bitdepth_max;
        dst[-1 * stride] = iclip_pixel(dst[+0 * stride] + edge_diff);
        for (int i = 1; i < 4; i++) {
            dst[-(1 + i) * stride] = iclip_pixel(dst[-(0 + i) * stride] +
                                                 rnd() % (2 * (I + 1)) - (I + 1));
            dst[+(0 + i) * stride] = iclip_pixel(dst[+(i - 1) * stride] +
                                                 rnd() % (2 * (I + 1)) - (I + 1));
        }
        break;
    }
}

static void check_lpf_sb(loopfilter_sb_fn fn, const char *const name,
                         const int n_blks, const int lf_idx,
                         const int is_chroma, const int dir)
{
    ALIGN_STK_64(pixel, c_dst_mem, 128 * 16,);
    ALIGN_STK_64(pixel, a_dst_mem, 128 * 16,);

    declare_func(void, pixel *dst, ptrdiff_t dst_stride, const uint32_t *mask,
                 const uint8_t (*l)[4], ptrdiff_t b4_stride,
                 const Av1FilterLUT *lut, int w HIGHBD_DECL_SUFFIX);

    pixel *a_dst, *c_dst;
    ptrdiff_t stride, b4_stride;
    int w, h;
    if (dir) {
        a_dst = a_dst_mem + n_blks * 4 * 8;
        c_dst = c_dst_mem + n_blks * 4 * 8;
        w = n_blks * 4;
        h = 16;
        b4_stride = 32;
    } else {
        a_dst = a_dst_mem + 8;
        c_dst = c_dst_mem + 8;
        w = 16;
        h = n_blks * 4;
        b4_stride = 2;
    }
    stride = w * sizeof(pixel);

    Av1FilterLUT lut;
    const int sharp = rnd() & 7;
    for (int level = 0; level < 64; level++) {
        int limit = level;

        if (sharp > 0) {
            limit >>= (sharp + 3) >> 2;
            limit = imin(limit, 9 - sharp);
        }
        limit = imax(limit, 1);

        lut.i[level] = limit;
        lut.e[level] = 2 * (level + 2) + limit;
    }
    lut.sharp[0] = (sharp + 3) >> 2;
    lut.sharp[1] = sharp ? 9 - sharp : 0xff;

    const int n_strengths = is_chroma ? 2 : 3;
    for (int i = 0; i < n_strengths; i++) {
        if (check_func(fn, "%s_w%d_%dbpc", name,
                       is_chroma ? 4 + 2 * i : 4 << i, BITDEPTH))
        {
            uint32_t vmask[4] = { 0 };
            uint8_t l[32 * 2][4];

            for (int j = 0; j < n_blks; j++) {
                const int idx = rnd() % (i + 2);
                if (idx) vmask[idx - 1] |= 1U << j;
                if (dir) {
                    l[j][lf_idx] = rnd() & 63;
                    l[j + 32][lf_idx] = rnd() & 63;
                } else {
                    l[j * 2][lf_idx] = rnd() & 63;
                    l[j * 2 + 1][lf_idx] = rnd() & 63;
                }
            }
#if BITDEPTH == 16
            const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
            const int bitdepth_max = 0xff;
#endif

            for (int i = 0; i < 4 * n_blks; i++) {
                const int x = i >> 2;
                int L;
                if (dir) {
                    L = l[32 + x][lf_idx] ? l[32 + x][lf_idx] : l[x][lf_idx];
                } else {
                    L = l[2 * x + 1][lf_idx] ? l[2 * x + 1][lf_idx] : l[2 * x][lf_idx];
                }
                init_lpf_border(c_dst + i * (dir ? 1 : 16), dir ? n_blks * 4 : 1,
                                lut.e[L], lut.i[L], bitdepth_max);
            }
            memcpy(a_dst_mem, c_dst_mem, 128 * sizeof(pixel) * 16);

            call_ref(c_dst, stride, vmask,
                     (const uint8_t(*)[4]) &l[dir ? 32 : 1][lf_idx],
                     b4_stride, &lut, n_blks HIGHBD_TAIL_SUFFIX);
            call_new(a_dst, stride, vmask,
                     (const uint8_t(*)[4]) &l[dir ? 32 : 1][lf_idx],
                     b4_stride, &lut, n_blks HIGHBD_TAIL_SUFFIX);

            checkasm_check_pixel(c_dst_mem, stride, a_dst_mem, stride,
                                 w, h, "dst");
            bench_new(alternate(c_dst, a_dst), stride, vmask,
                      (const uint8_t(*)[4]) &l[dir ? 32 : 1][lf_idx],
                      b4_stride, &lut, n_blks HIGHBD_TAIL_SUFFIX);
        }
    }
    report(name);
}

void bitfn(checkasm_check_loopfilter)(void) {
    Dav1dLoopFilterDSPContext c;

    bitfn(dav1d_loop_filter_dsp_init)(&c);

    check_lpf_sb(c.loop_filter_sb[0][0], "lpf_h_sb_y", 32, 0, 0, 0);
    check_lpf_sb(c.loop_filter_sb[0][1], "lpf_v_sb_y", 32, 1, 0, 1);
    check_lpf_sb(c.loop_filter_sb[1][0], "lpf_h_sb_uv", 16, 2, 1, 0);
    check_lpf_sb(c.loop_filter_sb[1][1], "lpf_v_sb_uv", 16, 2, 1, 1);
}
