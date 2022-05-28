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

#include <stdio.h>
#include <string.h>

#include "src/levels.h"
#include "src/looprestoration.h"
#include "src/tables.h"

static int to_binary(int x) { /* 0-15 -> 0000-1111 */
    return (x & 1) + 5 * (x & 2) + 25 * (x & 4) + 125 * (x & 8);
}

static void init_tmp(pixel *buf, const ptrdiff_t stride,
                     const int w, const int h, const int bitdepth_max)
{
    const int noise_mask = bitdepth_max >> 4;
    const int x_off = rnd() & 7, y_off = rnd() & 7;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            buf[x] = (((x + x_off) ^ (y + y_off)) & 8 ? bitdepth_max : 0) ^
                     (rnd() & noise_mask);
        }
        buf += PXSTRIDE(stride);
    }
}

static void check_wiener(Dav1dLoopRestorationDSPContext *const c, const int bpc) {
    ALIGN_STK_64(pixel, c_src, 448 * 64 + 64,), *const c_dst = c_src + 64;
    ALIGN_STK_64(pixel, a_src, 448 * 64 + 64,), *const a_dst = a_src + 64;
    ALIGN_STK_64(pixel, edge_buf, 448 * 8 + 64,), *const h_edge = edge_buf + 64;
    pixel left[64][4];
    LooprestorationParams params;
    int16_t (*const filter)[8] = params.filter;

    declare_func(void, pixel *dst, ptrdiff_t dst_stride,
                 const pixel (*const left)[4],
                 const pixel *lpf, int w, int h,
                 const LooprestorationParams *params,
                 enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX);

    for (int t = 0; t < 2; t++) {
        if (check_func(c->wiener[t], "wiener_%dtap_%dbpc", t ? 5 : 7, bpc)) {
            filter[0][0] = filter[0][6] = t ? 0 : (rnd() & 15) - 5;
            filter[0][1] = filter[0][5] = (rnd() & 31) - 23;
            filter[0][2] = filter[0][4] = (rnd() & 63) - 17;
            filter[0][3] = -(filter[0][0] + filter[0][1] + filter[0][2]) * 2;
#if BITDEPTH != 8
            filter[0][3] += 128;
#endif

            filter[1][0] = filter[1][6] = t ? 0 : (rnd() & 15) - 5;
            filter[1][1] = filter[1][5] = (rnd() & 31) - 23;
            filter[1][2] = filter[1][4] = (rnd() & 63) - 17;
            filter[1][3] = 128 - (filter[1][0] + filter[1][1] + filter[1][2]) * 2;

            const int base_w = 1 + (rnd() % 384);
            const int base_h = 1 + (rnd() & 63);
            const int bitdepth_max = (1 << bpc) - 1;

            init_tmp(c_src, 448 * sizeof(pixel), 448, 64, bitdepth_max);
            init_tmp(edge_buf, 448 * sizeof(pixel), 448, 8, bitdepth_max);
            init_tmp((pixel *) left, 4 * sizeof(pixel), 4, 64, bitdepth_max);

            for (enum LrEdgeFlags edges = 0; edges <= 0xf; edges++) {
                const int w = edges & LR_HAVE_RIGHT ? 256 : base_w;
                const int h = edges & LR_HAVE_BOTTOM ? 64 : base_h;

                memcpy(a_src, c_src, 448 * 64 * sizeof(pixel));

                call_ref(c_dst, 448 * sizeof(pixel), left,
                         h_edge, w, h, &params, edges HIGHBD_TAIL_SUFFIX);
                call_new(a_dst, 448 * sizeof(pixel), left,
                         h_edge, w, h, &params, edges HIGHBD_TAIL_SUFFIX);
                if (checkasm_check_pixel(c_dst, 448 * sizeof(pixel),
                                         a_dst, 448 * sizeof(pixel),
                                         w, h, "dst"))
                {
                    fprintf(stderr, "size = %dx%d, edges = %04d\n",
                            w, h, to_binary(edges));
                    break;
                }
            }
            bench_new(alternate(c_dst, a_dst), 448 * sizeof(pixel), left,
                      h_edge, 256, 64, &params, 0xf HIGHBD_TAIL_SUFFIX);
        }
    }
}

static void check_sgr(Dav1dLoopRestorationDSPContext *const c, const int bpc) {
    ALIGN_STK_64(pixel, c_src, 448 * 64 + 64,), *const c_dst = c_src + 64;
    ALIGN_STK_64(pixel, a_src, 448 * 64 + 64,), *const a_dst = a_src + 64;
    ALIGN_STK_64(pixel, edge_buf, 448 * 8 + 64,), *const h_edge = edge_buf + 64;
    pixel left[64][4];
    LooprestorationParams params;

    declare_func(void, pixel *dst, ptrdiff_t dst_stride,
                 const pixel (*const left)[4],
                 const pixel *lpf, int w, int h,
                 const LooprestorationParams *params,
                 enum LrEdgeFlags edges HIGHBD_DECL_SUFFIX);

    static const struct { char name[4]; uint8_t idx; } sgr_data[3] = {
        { "5x5", 14 },
        { "3x3", 10 },
        { "mix",  0 },
    };

    for (int i = 0; i < 3; i++) {
        if (check_func(c->sgr[i], "sgr_%s_%dbpc", sgr_data[i].name, bpc)) {
            const uint16_t *const sgr_params = dav1d_sgr_params[sgr_data[i].idx];
            params.sgr.s0 = sgr_params[0];
            params.sgr.s1 = sgr_params[1];
            params.sgr.w0 = sgr_params[0] ? (rnd() & 127) - 96 : 0;
            params.sgr.w1 = (sgr_params[1] ? 160 - (rnd() & 127) : 33) - params.sgr.w0;

            const int base_w = 1 + (rnd() % 384);
            const int base_h = 1 + (rnd() & 63);
            const int bitdepth_max = (1 << bpc) - 1;

            init_tmp(c_src, 448 * sizeof(pixel), 448, 64, bitdepth_max);
            init_tmp(edge_buf, 448 * sizeof(pixel), 448, 8, bitdepth_max);
            init_tmp((pixel *) left, 4 * sizeof(pixel), 4, 64, bitdepth_max);

            for (enum LrEdgeFlags edges = 0; edges <= 0xf; edges++) {
                const int w = edges & LR_HAVE_RIGHT ? 256 : base_w;
                const int h = edges & LR_HAVE_BOTTOM ? 64 : base_h;

                memcpy(a_src, c_src, 448 * 64 * sizeof(pixel));

                call_ref(c_dst, 448 * sizeof(pixel), left, h_edge,
                         w, h, &params, edges HIGHBD_TAIL_SUFFIX);
                call_new(a_dst, 448 * sizeof(pixel), left, h_edge,
                         w, h, &params, edges HIGHBD_TAIL_SUFFIX);
                if (checkasm_check_pixel(c_dst, 448 * sizeof(pixel),
                                         a_dst, 448 * sizeof(pixel),
                                         w, h, "dst"))
                {
                    fprintf(stderr, "size = %dx%d, edges = %04d\n",
                            w, h, to_binary(edges));
                    break;
                }
            }
            bench_new(alternate(c_dst, a_dst), 448 * sizeof(pixel), left,
                      h_edge, 256, 64, &params, 0xf HIGHBD_TAIL_SUFFIX);
        }
    }
}

void bitfn(checkasm_check_looprestoration)(void) {
#if BITDEPTH == 16
    const int bpc_min = 10, bpc_max = 12;
#else
    const int bpc_min = 8, bpc_max = 8;
#endif
    for (int bpc = bpc_min; bpc <= bpc_max; bpc += 2) {
        Dav1dLoopRestorationDSPContext c;
        bitfn(dav1d_loop_restoration_dsp_init)(&c, bpc);
        check_wiener(&c, bpc);
    }
    report("wiener");
    for (int bpc = bpc_min; bpc <= bpc_max; bpc += 2) {
        Dav1dLoopRestorationDSPContext c;
        bitfn(dav1d_loop_restoration_dsp_init)(&c, bpc);
        check_sgr(&c, bpc);
    }
    report("sgr");
}
