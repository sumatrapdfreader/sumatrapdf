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
#include "src/ipred.h"
#include "src/levels.h"

#include <stdio.h>

static const char *const intra_pred_mode_names[N_IMPL_INTRA_PRED_MODES] = {
    [DC_PRED]       = "dc",
    [DC_128_PRED]   = "dc_128",
    [TOP_DC_PRED]   = "dc_top",
    [LEFT_DC_PRED]  = "dc_left",
    [HOR_PRED]      = "h",
    [VERT_PRED]     = "v",
    [PAETH_PRED]    = "paeth",
    [SMOOTH_PRED]   = "smooth",
    [SMOOTH_V_PRED] = "smooth_v",
    [SMOOTH_H_PRED] = "smooth_h",
    [Z1_PRED]       = "z1",
    [Z2_PRED]       = "z2",
    [Z3_PRED]       = "z3",
    [FILTER_PRED]   = "filter"
};

static const char *const cfl_ac_names[3] = { "420", "422", "444" };

static const char *const cfl_pred_mode_names[DC_128_PRED + 1] = {
    [DC_PRED]       = "cfl",
    [DC_128_PRED]   = "cfl_128",
    [TOP_DC_PRED]   = "cfl_top",
    [LEFT_DC_PRED]  = "cfl_left",
};

static const uint8_t z_angles[27] = {
     3,  6,  9,
    14, 17, 20, 23, 26, 29, 32,
    36, 39, 42, 45, 48, 51, 54,
    58, 61, 64, 67, 70, 73, 76,
    81, 84, 87
};

static void check_intra_pred(Dav1dIntraPredDSPContext *const c) {
    PIXEL_RECT(c_dst, 64, 64);
    PIXEL_RECT(a_dst, 64, 64);
    ALIGN_STK_64(pixel, topleft_buf, 257,);
    pixel *const topleft = topleft_buf + 128;

    declare_func(void, pixel *dst, ptrdiff_t stride, const pixel *topleft,
                 int width, int height, int angle, int max_width, int max_height
                 HIGHBD_DECL_SUFFIX);

    for (int mode = 0; mode < N_IMPL_INTRA_PRED_MODES; mode++) {
        int bpc_min = BITDEPTH, bpc_max = BITDEPTH;
        if (mode == FILTER_PRED && BITDEPTH == 16) {
            bpc_min = 10;
            bpc_max = 12;
        }
        for (int bpc = bpc_min; bpc <= bpc_max; bpc += 2)
            for (int w = 4; w <= (mode == FILTER_PRED ? 32 : 64); w <<= 1)
                if (check_func(c->intra_pred[mode], "intra_pred_%s_w%d_%dbpc",
                    intra_pred_mode_names[mode], w, bpc))
                {
                    for (int h = imax(w / 4, 4); h <= imin(w * 4,
                        (mode == FILTER_PRED ? 32 : 64)); h <<= 1)
                    {
                        const ptrdiff_t stride = c_dst_stride;

                        int a = 0, maxw = 0, maxh = 0;
                        if (mode >= Z1_PRED && mode <= Z3_PRED) { /* angle */
                            a = (90 * (mode - Z1_PRED) + z_angles[rnd() % 27]) |
                                (rnd() & 0x600);
                            if (mode == Z2_PRED) {
                                maxw = rnd(), maxh = rnd();
                                maxw = 1 + (maxw & (maxw & 4096 ? 4095 : w - 1));
                                maxh = 1 + (maxh & (maxh & 4096 ? 4095 : h - 1));
                            }
                        } else if (mode == FILTER_PRED) /* filter_idx */
                            a = (rnd() % 5) | (rnd() & ~511);

                        int bitdepth_max;
                        if (bpc == 16)
                            bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
                        else
                            bitdepth_max = (1 << bpc) - 1;

                        for (int i = -h * 2; i <= w * 2; i++)
                            topleft[i] = rnd() & bitdepth_max;

                        CLEAR_PIXEL_RECT(c_dst);
                        CLEAR_PIXEL_RECT(a_dst);
                        call_ref(c_dst, stride, topleft, w, h, a, maxw, maxh
                                 HIGHBD_TAIL_SUFFIX);
                        call_new(a_dst, stride, topleft, w, h, a, maxw, maxh
                                 HIGHBD_TAIL_SUFFIX);
                        if (checkasm_check_pixel_padded(c_dst, stride,
                                                        a_dst, stride,
                                                        w, h, "dst"))
                        {
                            if (mode == Z1_PRED || mode == Z3_PRED)
                                fprintf(stderr, "angle = %d (0x%03x)\n",
                                        a & 0x1ff, a & 0x600);
                            else if (mode == Z2_PRED)
                                fprintf(stderr, "angle = %d (0x%03x), "
                                        "max_width = %d, max_height = %d\n",
                                        a & 0x1ff, a & 0x600, maxw, maxh);
                            else if (mode == FILTER_PRED)
                                fprintf(stderr, "filter_idx = %d\n", a & 0x1ff);
                        }

                        bench_new(a_dst, stride, topleft, w, h, a, 128, 128
                                  HIGHBD_TAIL_SUFFIX);
                    }
                }
    }
    report("intra_pred");
}

static void check_cfl_ac(Dav1dIntraPredDSPContext *const c) {
    ALIGN_STK_64(int16_t, c_dst, 32 * 32,);
    ALIGN_STK_64(int16_t, a_dst, 32 * 32,);
    ALIGN_STK_64(pixel, luma, 32 * 32,);

    declare_func(void, int16_t *ac, const pixel *y, ptrdiff_t stride,
                 int w_pad, int h_pad, int cw, int ch);

    for (int layout = 1; layout <= DAV1D_PIXEL_LAYOUT_I444; layout++) {
        const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;
        const int h_step = 2 >> ss_hor, v_step = 2 >> ss_ver;
        for (int w = 4; w <= (32 >> ss_hor); w <<= 1)
            if (check_func(c->cfl_ac[layout - 1], "cfl_ac_%s_w%d_%dbpc",
                cfl_ac_names[layout - 1], w, BITDEPTH))
            {
                for (int h = imax(w / 4, 4);
                     h <= imin(w * 4, (32 >> ss_ver)); h <<= 1)
                {
                    const ptrdiff_t stride = 32 * sizeof(pixel);
                    for (int w_pad = imax((w >> 2) - h_step, 0);
                         w_pad >= 0; w_pad -= h_step)
                    {
                        for (int h_pad = imax((h >> 2) - v_step, 0);
                             h_pad >= 0; h_pad -= v_step)
                        {
#if BITDEPTH == 16
                            const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                            const int bitdepth_max = 0xff;
#endif
                            for (int y = 0; y < (h << ss_ver); y++)
                                for (int x = 0; x < (w << ss_hor); x++)
                                    luma[y * 32 + x] = rnd() & bitdepth_max;

                            call_ref(c_dst, luma, stride, w_pad, h_pad, w, h);
                            call_new(a_dst, luma, stride, w_pad, h_pad, w, h);
                            checkasm_check(int16_t, c_dst, w * sizeof(*c_dst),
                                                    a_dst, w * sizeof(*a_dst),
                                                    w, h, "dst");
                        }
                    }

                    bench_new(a_dst, luma, stride, 0, 0, w, h);
                }
            }
    }
    report("cfl_ac");
}

static void check_cfl_pred(Dav1dIntraPredDSPContext *const c) {
    PIXEL_RECT(c_dst, 32, 32);
    PIXEL_RECT(a_dst, 32, 32);
    ALIGN_STK_64(int16_t, ac, 32 * 32,);
    ALIGN_STK_64(pixel, topleft_buf, 257,);
    pixel *const topleft = topleft_buf + 128;

    declare_func(void, pixel *dst, ptrdiff_t stride, const pixel *topleft,
                 int width, int height, const int16_t *ac, int alpha
                 HIGHBD_DECL_SUFFIX);

    for (int mode = 0; mode <= DC_128_PRED; mode += 1 + 2 * !mode)
        for (int w = 4; w <= 32; w <<= 1)
            if (check_func(c->cfl_pred[mode], "cfl_pred_%s_w%d_%dbpc",
                cfl_pred_mode_names[mode], w, BITDEPTH))
            {
                for (int h = imax(w / 4, 4); h <= imin(w * 4, 32); h <<= 1)
                {
#if BITDEPTH == 16
                    const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                    const int bitdepth_max = 0xff;
#endif

                    int alpha = ((rnd() & 15) + 1) * (1 - (rnd() & 2));

                    for (int i = -h * 2; i <= w * 2; i++)
                        topleft[i] = rnd() & bitdepth_max;

                    int luma_avg = w * h >> 1;
                    for (int i = 0; i < w * h; i++)
                        luma_avg += ac[i] = rnd() & (bitdepth_max << 3);
                    luma_avg /= w * h;
                    for (int i = 0; i < w * h; i++)
                        ac[i] -= luma_avg;

                    CLEAR_PIXEL_RECT(c_dst);
                    CLEAR_PIXEL_RECT(a_dst);

                    call_ref(c_dst, c_dst_stride, topleft, w, h, ac, alpha
                             HIGHBD_TAIL_SUFFIX);
                    call_new(a_dst, a_dst_stride, topleft, w, h, ac, alpha
                             HIGHBD_TAIL_SUFFIX);
                    checkasm_check_pixel_padded(c_dst, c_dst_stride, a_dst, a_dst_stride,
                                                w, h, "dst");

                    bench_new(a_dst, a_dst_stride, topleft, w, h, ac, alpha
                              HIGHBD_TAIL_SUFFIX);
                }
            }
    report("cfl_pred");
}

static void check_pal_pred(Dav1dIntraPredDSPContext *const c) {
    PIXEL_RECT(c_dst, 64, 64);
    PIXEL_RECT(a_dst, 64, 64);
    ALIGN_STK_64(uint8_t, idx, 64 * 64,);
    ALIGN_STK_16(uint16_t, pal, 8,);

    declare_func(void, pixel *dst, ptrdiff_t stride, const uint16_t *pal,
                 const uint8_t *idx, int w, int h);

    for (int w = 4; w <= 64; w <<= 1)
        if (check_func(c->pal_pred, "pal_pred_w%d_%dbpc", w, BITDEPTH))
            for (int h = imax(w / 4, 4); h <= imin(w * 4, 64); h <<= 1)
            {
#if BITDEPTH == 16
                const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                const int bitdepth_max = 0xff;
#endif

                for (int i = 0; i < 8; i++)
                    pal[i] = rnd() & bitdepth_max;

                for (int i = 0; i < w * h; i++)
                    idx[i] = rnd() & 7;

                CLEAR_PIXEL_RECT(c_dst);
                CLEAR_PIXEL_RECT(a_dst);

                call_ref(c_dst, c_dst_stride, pal, idx, w, h);
                call_new(a_dst, a_dst_stride, pal, idx, w, h);
                checkasm_check_pixel_padded(c_dst, c_dst_stride,
                                            a_dst, a_dst_stride, w, h, "dst");

                bench_new(a_dst, a_dst_stride, pal, idx, w, h);
            }
    report("pal_pred");
}

void bitfn(checkasm_check_ipred)(void) {
    Dav1dIntraPredDSPContext c;
    bitfn(dav1d_intra_pred_dsp_init)(&c);

    check_intra_pred(&c);
    check_cfl_ac(&c);
    check_cfl_pred(&c);
    check_pal_pred(&c);
}
