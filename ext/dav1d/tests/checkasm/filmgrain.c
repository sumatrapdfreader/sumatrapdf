/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Two Orioles, LLC
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
#include "src/filmgrain.h"
#define UNIT_TEST 1
#include "src/fg_apply_tmpl.c"

#if BITDEPTH == 8
#define checkasm_check_entry(...) checkasm_check(int8_t, __VA_ARGS__)
#else
#define checkasm_check_entry(...) checkasm_check(int16_t, __VA_ARGS__)
#endif

static const char ss_name[][4] = {
    [DAV1D_PIXEL_LAYOUT_I420 - 1] = "420",
    [DAV1D_PIXEL_LAYOUT_I422 - 1] = "422",
    [DAV1D_PIXEL_LAYOUT_I444 - 1] = "444",
};

static void check_gen_grny(const Dav1dFilmGrainDSPContext *const dsp) {
    ALIGN_STK_16(entry, grain_lut_c, GRAIN_HEIGHT,[GRAIN_WIDTH]);
    ALIGN_STK_16(entry, grain_lut_a, GRAIN_HEIGHT + 1,[GRAIN_WIDTH]);

    declare_func(void, entry grain_lut[][GRAIN_WIDTH],
                 const Dav1dFilmGrainData *data HIGHBD_DECL_SUFFIX);

    for (int i = 0; i < 4; i++) {
        if (check_func(dsp->generate_grain_y, "gen_grain_y_ar%d_%dbpc", i, BITDEPTH)) {
            ALIGN_STK_16(Dav1dFilmGrainData, fg_data, 1,);
            fg_data[0].seed = rnd() & 0xFFFF;

#if BITDEPTH == 16
            const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#endif

            fg_data[0].grain_scale_shift = rnd() & 3;
            fg_data[0].ar_coeff_shift = (rnd() & 3) + 6;
            fg_data[0].ar_coeff_lag = i;
            const int num_y_pos = 2 * fg_data[0].ar_coeff_lag * (fg_data[0].ar_coeff_lag + 1);
            for (int n = 0; n < num_y_pos; n++)
                fg_data[0].ar_coeffs_y[n] = (rnd() & 0xff) - 128;

            call_ref(grain_lut_c, fg_data HIGHBD_TAIL_SUFFIX);
            call_new(grain_lut_a, fg_data HIGHBD_TAIL_SUFFIX);
            checkasm_check_entry(grain_lut_c[0], sizeof(entry) * GRAIN_WIDTH,
                                 grain_lut_a[0], sizeof(entry) * GRAIN_WIDTH,
                                 GRAIN_WIDTH, GRAIN_HEIGHT, "grain_lut");

            bench_new(grain_lut_a, fg_data HIGHBD_TAIL_SUFFIX);
        }
    }

    report("gen_grain_y");
}

static void check_gen_grnuv(const Dav1dFilmGrainDSPContext *const dsp) {
    ALIGN_STK_16(entry, grain_lut_y, GRAIN_HEIGHT + 1,[GRAIN_WIDTH]);
    ALIGN_STK_16(entry, grain_lut_c, GRAIN_HEIGHT,    [GRAIN_WIDTH]);
    ALIGN_STK_16(entry, grain_lut_a, GRAIN_HEIGHT + 1,[GRAIN_WIDTH]);

    declare_func(void, entry grain_lut[][GRAIN_WIDTH],
                 const entry grain_lut_y[][GRAIN_WIDTH],
                 const Dav1dFilmGrainData *data, intptr_t uv HIGHBD_DECL_SUFFIX);

    for (int layout_idx = 0; layout_idx < 3; layout_idx++) {
        const enum Dav1dPixelLayout layout = layout_idx + 1;
        const int ss_x = layout != DAV1D_PIXEL_LAYOUT_I444;
        const int ss_y = layout == DAV1D_PIXEL_LAYOUT_I420;

        for (int i = 0; i < 4; i++) {
            if (check_func(dsp->generate_grain_uv[layout_idx],
                           "gen_grain_uv_ar%d_%dbpc_%s",
                           i, BITDEPTH, ss_name[layout_idx]))
            {
                ALIGN_STK_16(Dav1dFilmGrainData, fg_data, 1,);
                fg_data[0].seed = rnd() & 0xFFFF;

#if BITDEPTH == 16
                const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#endif

                fg_data[0].num_y_points = rnd() & 1;
                fg_data[0].grain_scale_shift = rnd() & 3;
                fg_data[0].ar_coeff_shift = (rnd() & 3) + 6;
                fg_data[0].ar_coeff_lag = i;
                const int num_y_pos = 2 * fg_data[0].ar_coeff_lag * (fg_data[0].ar_coeff_lag + 1);
                for (int n = 0; n < num_y_pos; n++)
                    fg_data[0].ar_coeffs_y[n] = (rnd() & 0xff) - 128;
                dsp->generate_grain_y(grain_lut_y, fg_data HIGHBD_TAIL_SUFFIX);

                const int uv = rnd() & 1;
                const int num_uv_pos = num_y_pos + !!fg_data[0].num_y_points;
                for (int n = 0; n < num_uv_pos; n++)
                    fg_data[0].ar_coeffs_uv[uv][n] = (rnd() & 0xff) - 128;
                if (!fg_data[0].num_y_points)
                    fg_data[0].ar_coeffs_uv[uv][num_uv_pos] = 0;
                memset(grain_lut_c, 0xff, sizeof(grain_lut_c));
                memset(grain_lut_a, 0xff, sizeof(grain_lut_a));
                call_ref(grain_lut_c, grain_lut_y, fg_data, uv HIGHBD_TAIL_SUFFIX);
                call_new(grain_lut_a, grain_lut_y, fg_data, uv HIGHBD_TAIL_SUFFIX);
                int w = ss_x ? 44 : GRAIN_WIDTH;
                int h = ss_y ? 38 : GRAIN_HEIGHT;
                checkasm_check_entry(grain_lut_c[0], sizeof(entry) * GRAIN_WIDTH,
                                     grain_lut_a[0], sizeof(entry) * GRAIN_WIDTH,
                                     w, h, "grain_lut");

                bench_new(grain_lut_a, grain_lut_y, fg_data, uv HIGHBD_TAIL_SUFFIX);
            }
        }
    }

    report("gen_grain_uv");
}

static void check_fgy_sbrow(const Dav1dFilmGrainDSPContext *const dsp) {
    PIXEL_RECT(c_dst, 128, 32);
    PIXEL_RECT(a_dst, 128, 32);
    PIXEL_RECT(src,   128, 32);
    const ptrdiff_t stride = c_dst_stride;

    declare_func(void, pixel *dst_row, const pixel *src_row, ptrdiff_t stride,
                 const Dav1dFilmGrainData *data, size_t pw,
                 const uint8_t scaling[SCALING_SIZE],
                 const entry grain_lut[][GRAIN_WIDTH],
                 int bh, int row_num HIGHBD_DECL_SUFFIX);

    if (check_func(dsp->fgy_32x32xn, "fgy_32x32xn_%dbpc", BITDEPTH)) {
        ALIGN_STK_16(Dav1dFilmGrainData, fg_data, 16,);
        ALIGN_STK_16(entry, grain_lut, GRAIN_HEIGHT + 1,[GRAIN_WIDTH]);
        ALIGN_STK_64(uint8_t, scaling, SCALING_SIZE,);
        fg_data[0].seed = rnd() & 0xFFFF;

#if BITDEPTH == 16
        const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
        const int bitdepth_max = 0xff;
#endif

        fg_data[0].grain_scale_shift = rnd() & 3;
        fg_data[0].ar_coeff_shift = (rnd() & 3) + 6;
        fg_data[0].ar_coeff_lag = rnd() & 3;
        const int num_y_pos = 2 * fg_data[0].ar_coeff_lag * (fg_data[0].ar_coeff_lag + 1);
        for (int n = 0; n < num_y_pos; n++)
            fg_data[0].ar_coeffs_y[n] = (rnd() & 0xff) - 128;
        dsp->generate_grain_y(grain_lut, fg_data HIGHBD_TAIL_SUFFIX);

        fg_data[0].num_y_points = 2 + (rnd() % 13);
        const int pad = 0xff / fg_data[0].num_y_points;
        for (int n = 0; n < fg_data[0].num_y_points; n++) {
            fg_data[0].y_points[n][0] = 0xff * n / fg_data[0].num_y_points;
            fg_data[0].y_points[n][0] += rnd() % pad;
            fg_data[0].y_points[n][1] = rnd() & 0xff;
        }
        generate_scaling(bitdepth_from_max(bitdepth_max), fg_data[0].y_points,
                         fg_data[0].num_y_points, scaling);

        fg_data[0].clip_to_restricted_range = rnd() & 1;
        fg_data[0].scaling_shift = (rnd() & 3) + 8;
        for (fg_data[0].overlap_flag = 0; fg_data[0].overlap_flag <= 1;
             fg_data[0].overlap_flag++)
        {
            for (int i = 0; i <= 2 * fg_data[0].overlap_flag; i++) {
                int w, h, row_num;
                if (fg_data[0].overlap_flag) {
                    w = 35 + (rnd() % 93);
                    if (i == 0) {
                        row_num = 0;
                        h = 1 + (rnd() % 31);
                    } else {
                        row_num = 1 + (rnd() & 0x7ff);
                        if (i == 1) {
                            h = 3 + (rnd() % 30);
                        } else {
                            h = 1 + (rnd() & 1);
                        }
                    }
                } else {
                    w = 1 + (rnd() & 127);
                    h = 1 + (rnd() & 31);
                    row_num = rnd() & 0x7ff;
                }

                for (int y = 0; y < 32; y++) {
                    // Src pixels past the right edge can be uninitialized
                    for (int x = 0; x < 128; x++)
                        src[y * PXSTRIDE(stride) + x] = rnd();
                    for (int x = 0; x < w; x++)
                        src[y * PXSTRIDE(stride) + x] &= bitdepth_max;
                }

                CLEAR_PIXEL_RECT(c_dst);
                CLEAR_PIXEL_RECT(a_dst);
                call_ref(c_dst, src, stride, fg_data, w, scaling, grain_lut, h,
                         row_num HIGHBD_TAIL_SUFFIX);
                call_new(a_dst, src, stride, fg_data, w, scaling, grain_lut, h,
                         row_num HIGHBD_TAIL_SUFFIX);

                checkasm_check_pixel_padded_align(c_dst, stride, a_dst, stride,
                                                  w, h, "dst", 32, 2);
            }
        }
        fg_data[0].overlap_flag = 1;
        for (int y = 0; y < 32; y++) {
            // Make sure all pixels are in range
            for (int x = 0; x < 128; x++)
                src[y * PXSTRIDE(stride) + x] &= bitdepth_max;
        }
        bench_new(a_dst, src, stride, fg_data, 64, scaling, grain_lut, 32,
                  1 HIGHBD_TAIL_SUFFIX);
    }

    report("fgy_32x32xn");
}

static void check_fguv_sbrow(const Dav1dFilmGrainDSPContext *const dsp) {
    PIXEL_RECT(c_dst,    128, 32);
    PIXEL_RECT(a_dst,    128, 32);
    PIXEL_RECT(src,      128, 32);
    PIXEL_RECT(luma_src, 128, 32);
    const ptrdiff_t lstride = luma_src_stride;

    declare_func(void, pixel *dst_row, const pixel *src_row, ptrdiff_t stride,
                 const Dav1dFilmGrainData *data, size_t pw,
                 const uint8_t scaling[SCALING_SIZE],
                 const entry grain_lut[][GRAIN_WIDTH], int bh, int row_num,
                 const pixel *luma_row, ptrdiff_t luma_stride, int uv_pl,
                 int is_identity HIGHBD_DECL_SUFFIX);

    for (int layout_idx = 0; layout_idx < 3; layout_idx++) {
        const enum Dav1dPixelLayout layout = layout_idx + 1;
        const int ss_x = layout != DAV1D_PIXEL_LAYOUT_I444;
        const int ss_y = layout == DAV1D_PIXEL_LAYOUT_I420;
        const ptrdiff_t stride = c_dst_stride;

        for (int csfl = 0; csfl <= 1; csfl++) {
            if (check_func(dsp->fguv_32x32xn[layout_idx],
                           "fguv_32x32xn_%dbpc_%s_csfl%d",
                           BITDEPTH, ss_name[layout_idx], csfl))
            {
                ALIGN_STK_16(Dav1dFilmGrainData, fg_data, 1,);
                ALIGN_STK_16(entry, grain_lut, 2,[GRAIN_HEIGHT + 1][GRAIN_WIDTH]);
                ALIGN_STK_64(uint8_t, scaling, SCALING_SIZE,);

                fg_data[0].seed = rnd() & 0xFFFF;

#if BITDEPTH == 16
                const int bitdepth_max = rnd() & 1 ? 0x3ff : 0xfff;
#else
                const int bitdepth_max = 0xff;
#endif
                const int uv_pl = rnd() & 1;
                const int is_identity = rnd() & 1;

                fg_data[0].grain_scale_shift = rnd() & 3;
                fg_data[0].ar_coeff_shift = (rnd() & 3) + 6;
                fg_data[0].ar_coeff_lag = rnd() & 3;
                fg_data[0].num_y_points = csfl ? 2 + (rnd() % 13) : 0;
                const int num_y_pos = 2 * fg_data[0].ar_coeff_lag * (fg_data[0].ar_coeff_lag + 1);
                for (int n = 0; n < num_y_pos; n++)
                    fg_data[0].ar_coeffs_y[n] = (rnd() & 0xff) - 128;
                const int num_uv_pos = num_y_pos + 1;
                for (int n = 0; n < num_uv_pos; n++)
                    fg_data[0].ar_coeffs_uv[uv_pl][n] = (rnd() & 0xff) - 128;
                dsp->generate_grain_y(grain_lut[0], fg_data HIGHBD_TAIL_SUFFIX);
                dsp->generate_grain_uv[layout_idx](grain_lut[1], grain_lut[0],
                                                   fg_data, uv_pl HIGHBD_TAIL_SUFFIX);

                if (csfl) {
                    const int pad = 0xff / fg_data[0].num_y_points;
                    for (int n = 0; n < fg_data[0].num_y_points; n++) {
                        fg_data[0].y_points[n][0] = 0xff * n / fg_data[0].num_y_points;
                        fg_data[0].y_points[n][0] += rnd() % pad;
                        fg_data[0].y_points[n][1] = rnd() & 0xff;
                    }
                    generate_scaling(bitdepth_from_max(bitdepth_max), fg_data[0].y_points,
                                     fg_data[0].num_y_points, scaling);
                } else {
                    fg_data[0].num_uv_points[uv_pl] = 2 + (rnd() % 9);
                    const int pad = 0xff / fg_data[0].num_uv_points[uv_pl];
                    for (int n = 0; n < fg_data[0].num_uv_points[uv_pl]; n++) {
                        fg_data[0].uv_points[uv_pl][n][0] = 0xff * n / fg_data[0].num_uv_points[uv_pl];
                        fg_data[0].uv_points[uv_pl][n][0] += rnd() % pad;
                        fg_data[0].uv_points[uv_pl][n][1] = rnd() & 0xff;
                    }
                    generate_scaling(bitdepth_from_max(bitdepth_max), fg_data[0].uv_points[uv_pl],
                                     fg_data[0].num_uv_points[uv_pl], scaling);

                    fg_data[0].uv_mult[uv_pl] = (rnd() & 0xff) - 128;
                    fg_data[0].uv_luma_mult[uv_pl] = (rnd() & 0xff) - 128;
                    fg_data[0].uv_offset[uv_pl] = (rnd() & 0x1ff) - 256;
                }

                fg_data[0].clip_to_restricted_range = rnd() & 1;
                fg_data[0].scaling_shift = (rnd() & 3) + 8;
                fg_data[0].chroma_scaling_from_luma = csfl;
                for (fg_data[0].overlap_flag = 0; fg_data[0].overlap_flag <= 1;
                     fg_data[0].overlap_flag++)
                {
                    for (int i = 0; i <= 2 * fg_data[0].overlap_flag; i++) {
                        int w, h, row_num;
                        if (fg_data[0].overlap_flag) {
                            w = (36 >> ss_x) + (rnd() % (92 >> ss_x));
                            if (i == 0) {
                                row_num = 0;
                                h = 1 + (rnd() & (31 >> ss_y));
                            } else {
                                row_num = 1 + (rnd() & 0x7ff);
                                if (i == 1) {
                                    h = (ss_y ? 2 : 3) + (rnd() % (ss_y ? 15 : 30));
                                } else {
                                    h = ss_y ? 1 : 1 + (rnd() & 1);
                                }
                            }
                        } else {
                            w = 1 + (rnd() & (127 >> ss_x));
                            h = 1 + (rnd() & (31 >> ss_y));
                            row_num = rnd() & 0x7ff;
                        }

                        for (int y = 0; y < 32; y++) {
                            // Src pixels past the right edge can be uninitialized
                            for (int x = 0; x < 128; x++) {
                                src[y * PXSTRIDE(stride) + x] = rnd();
                                luma_src[y * PXSTRIDE(lstride) + x] = rnd();
                            }
                            for (int x = 0; x < w; x++)
                                src[y * PXSTRIDE(stride) + x] &= bitdepth_max;
                            for (int x = 0; x < (w << ss_x); x++)
                                luma_src[y * PXSTRIDE(lstride) + x] &= bitdepth_max;
                        }

                        CLEAR_PIXEL_RECT(c_dst);
                        CLEAR_PIXEL_RECT(a_dst);
                        call_ref(c_dst, src, stride, fg_data, w, scaling, grain_lut[1], h,
                                 row_num, luma_src, lstride, uv_pl, is_identity HIGHBD_TAIL_SUFFIX);
                        call_new(a_dst, src, stride, fg_data, w, scaling, grain_lut[1], h,
                                 row_num, luma_src, lstride, uv_pl, is_identity HIGHBD_TAIL_SUFFIX);

                        checkasm_check_pixel_padded_align(c_dst, stride,
                                                          a_dst, stride,
                                                          w, h, "dst",
                                                          32 >> ss_x, 4);
                    }
                }

                fg_data[0].overlap_flag = 1;
                for (int y = 0; y < 32; y++) {
                    // Make sure all pixels are in range
                    for (int x = 0; x < 128; x++) {
                        src[y * PXSTRIDE(stride) + x] &= bitdepth_max;
                        luma_src[y * PXSTRIDE(lstride) + x] &= bitdepth_max;
                    }
                }
                bench_new(a_dst, src, stride, fg_data, 64 >> ss_x, scaling, grain_lut[1], 32 >> ss_y,
                          1, luma_src, lstride, uv_pl, is_identity HIGHBD_TAIL_SUFFIX);
            }
        }
    }

    report("fguv_32x32xn");
}

void bitfn(checkasm_check_filmgrain)(void) {
    Dav1dFilmGrainDSPContext c;

    bitfn(dav1d_film_grain_dsp_init)(&c);

    check_gen_grny(&c);
    check_gen_grnuv(&c);
    check_fgy_sbrow(&c);
    check_fguv_sbrow(&c);
}
