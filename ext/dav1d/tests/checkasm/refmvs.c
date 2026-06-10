/*
 * Copyright © 2021, VideoLAN and dav1d authors
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
#include "src/refmvs.h"

#include <stdio.h>

static inline int gen_mv(const int total_bits, int spel_bits) {
    int bits = rnd() & ((1 << spel_bits) - 1);
    do {
        bits |= (rnd() & 1) << spel_bits;
    } while (rnd() & 1 && ++spel_bits < total_bits);
    // the do/while makes it relatively more likely to be close to zero (fpel)
    // than far away
    return rnd() & 1 ? -bits : bits;
}

#define ARRAY_SIZE(n) (sizeof(n)/sizeof(*(n)))

static inline int get_min_mv_val(const int idx) {
    if      (idx <= 9)  return idx;
    else if (idx <= 18) return (idx - 9) * 10;
    else if (idx <= 27) return (idx - 18) * 100;
    else if (idx <= 36) return (idx - 27) * 1000;
    else                return (idx - 36) * 10000;
}

static inline void gen_tmv(refmvs_temporal_block *const rb, const uint8_t *const ref2ref) {
    rb->ref = rnd() % 7;
    if (!rb->ref) return;
    static const int x_prob[] = {
        26447556, 6800591, 3708783,  2198592, 1635940, 1145901, 1052602, 1261759,
         1099739,  755108, 6075404,  4355916, 3254908, 2897157, 2273676, 2154432,
         1937436, 1694818, 1466863, 10203087, 5241546, 3328819, 2187483, 1458997,
         1030842,  806863,  587219,   525024, 1858953,  422368,  114626,   16992
    };
    static const int y_prob[] = {
        33845001, 7591218,  6425971, 4115838, 4032161, 2515962, 2614601, 2343656,
         2898897, 1397254, 10125350, 5124449, 3232914, 2185499, 1608775, 1342585,
          980208,  795714,   649665, 3369250, 1298716,  486002,  279588,  235990,
          110318,   89372,    66895,   46980,  153322,   32960,    4500,     389
    };
    const int prob = rnd() % 100000000;
    int acc = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(x_prob); i++) {
        acc += x_prob[i];
        if (prob < acc) {
            const int min = get_min_mv_val(i);
            const int max = get_min_mv_val(i + 1);
            const int val = min + rnd() % (max - min);
            rb->mv.x = iclip(val * ref2ref[rb->ref], -(1 << 15), (1 << 15) - 1);
            break;
        }
    }
    acc = 0;
    for (unsigned i = 0; i < ARRAY_SIZE(y_prob); i++) {
        acc += y_prob[i];
        if (prob < acc) {
            const int min = get_min_mv_val(i);
            const int max = get_min_mv_val(i + 1);
            const int val = min + rnd() % (max - min);
            rb->mv.y = iclip(val * ref2ref[rb->ref], -(1 << 15), (1 << 15) - 1);
            break;
        }
    }
}

static inline int get_ref2cur(void) {
    const int prob = rnd() % 100;
    static const uint8_t ref2cur[11] = { 35, 55, 67, 73, 78, 83, 84, 87, 90, 93, 100 };
    for (int i = 0; i < 11; i++)
        if (prob < ref2cur[i])
            return rnd() & 1 ? -(i + 1) : i + 1;
    return 0;
}

static inline int get_seqlen(void) {
    int len = 0, max_len;
    const int prob = rnd() % 100000;
    // =1 =2 =3 =4  <8  =8 <16 =16 <32 =32 <48 =48 <64 =64 >64 eq240
    //  5 17 1.5 16  5  10  5   7   4   3  1.5  2   1    2   20   15   chimera blocks
    // 25 38 2.5 19 3.5 5.5 2 1.87 .86 .4  .18 .2 .067 .165 .478 .28   chimera sequences

    if      (prob < 25000) len = 1;       // =1   5%
    else if (prob < 63000) len = 2;       // =2   17%
    else if (prob < 65500) len = 3;       // =3   1.5%
    else if (prob < 84500) len = 4;       // =4   16%
    else if (prob < 88000) max_len = 7;   // <8   5% (43.5% tot <8)
    else if (prob < 93500) len = 8;       // =8   10%
    else if (prob < 95500) max_len = 15;  // <16  5%
    else if (prob < 97370) len = 16;      // =16  7%
    else if (prob < 98230) max_len = 31;  // <32  4%
    else if (prob < 98630) len = 32;      // =32  3%
    else if (prob < 98810) max_len = 47;  // <48  1.5%
    else if (prob < 99010) len = 48;      // =48  2%
    else if (prob < 99077) max_len = 63;  // <64  1%
    else if (prob < 99242) len = 64;      // =64  2%
    else if (prob < 99720) max_len = 239; // <240 5%
    else                   len = 240;     // =240 15%

    if (!len) len = 1 + rnd() % max_len;
    return len;
}

static inline void init_rp_ref(refmvs_frame const *const rf,
                               const int col_start8, const int col_end8,
                               const int row_start8, const int row_end8)
{
    const int col_start8i = imax(col_start8 - 8, 0);
    const int col_end8i = imin(col_end8 + 8, rf->iw8);
    for (int n = 0; n < rf->n_mfmvs; n++) {
        refmvs_temporal_block *rp_ref = rf->rp_ref[rf->mfmv_ref[n]];
        for (int i = row_start8; i < imin(row_end8, rf->ih8); i++) {
            for (int j = col_start8i; j < col_end8i;) {
                refmvs_temporal_block rb;
                gen_tmv(&rb, rf->mfmv_ref2ref[n]);
                for (int k = get_seqlen(); k && j < col_end8i; k--, j++)
                    rp_ref[i * rf->iw8 + j] = rb;
            }
        }
    }
}

static void check_load_tmvs(const Dav1dRefmvsDSPContext *const c) {
    refmvs_temporal_block *rp_ref[7] = {0};
    refmvs_temporal_block c_rp_proj[240 * 63];
    refmvs_temporal_block a_rp_proj[240 * 63];
    refmvs_frame rf = {
        .rp_ref = rp_ref,
        .rp_stride = 240, .iw8 = 240, .ih8 = 63,
        .n_mfmvs = 3
    };
    const size_t rp_ref_sz = rf.ih8 * rf.rp_stride * sizeof(refmvs_temporal_block);

    declare_func(void, const refmvs_frame *rf, int tile_row_idx,
                 int col_start8, int col_end8, int row_start8, int row_end8);

    if (check_func(c->load_tmvs, "load_tmvs")) {
        const int row_start8 = (rnd() & 3) << 4;
        const int row_end8 = row_start8 + 16;
        const int col_start8 = rnd() & 31;
        const int col_end8 = rf.iw8 - (rnd() & 31);

        for (int n = 0; n < rf.n_mfmvs; n++) {
            rf.mfmv_ref[n] = rnd() % 7;
            rf.mfmv_ref2cur[n] = get_ref2cur();
            for (int r = 0; r < 7; r++)
                rf.mfmv_ref2ref[n][r] = rnd() & 31;
        }
        for (int n = 0; n < rf.n_mfmvs; n++) {
            refmvs_temporal_block **p_rp_ref = &rp_ref[rf.mfmv_ref[n]];
            if (!*p_rp_ref)
                *p_rp_ref = malloc(rp_ref_sz);
        }
        init_rp_ref(&rf, 0, rf.iw8, row_start8, row_end8);
        for (int i = 0; i < rf.iw8 * rf.ih8; i++) {
            c_rp_proj[i].mv.n = a_rp_proj[i].mv.n = 0xdeadbeef;
            c_rp_proj[i].ref = a_rp_proj[i].ref = 0xdd;
        }

        rf.n_tile_threads = 1;

        rf.rp_proj = c_rp_proj;
        call_ref(&rf, 0, col_start8, col_end8, row_start8, row_end8);
        rf.rp_proj = a_rp_proj;
        call_new(&rf, 0, col_start8, col_end8, row_start8, row_end8);

        for (int i = 0; i < rf.ih8; i++)
            for (int j = 0; j < rf.iw8; j++)
                if (c_rp_proj[i * rf.iw8 + j].mv.n != a_rp_proj[i * rf.iw8 + j].mv.n ||
                    (c_rp_proj[i * rf.iw8 + j].ref != a_rp_proj[i * rf.iw8 + j].ref &&
                     c_rp_proj[i * rf.iw8 + j].mv.n != INVALID_MV))
                {
                    if (fail()) {
                        fprintf(stderr, "[%d][%d] c_rp.mv.x = 0x%x a_rp.mv.x = 0x%x\n",
                                i, j, c_rp_proj[i * rf.iw8 + j].mv.x, a_rp_proj[i * rf.iw8 + j].mv.x);
                        fprintf(stderr, "[%d][%d] c_rp.mv.y = 0x%x a_rp.mv.y = 0x%x\n",
                                i, j, c_rp_proj[i * rf.iw8 + j].mv.y, a_rp_proj[i * rf.iw8 + j].mv.y);
                        fprintf(stderr, "[%d][%d] c_rp.ref = %u a_rp.ref = %u\n",
                                i, j, c_rp_proj[i * rf.iw8 + j].ref, a_rp_proj[i * rf.iw8 + j].ref);
                    }
                }

        if (checkasm_bench_func()) {
            for (int n = 0; n < rf.n_mfmvs; n++) {
                rf.mfmv_ref2cur[n] = 1;
                for (int r = 0; r < 7; r++)
                    rf.mfmv_ref2ref[n][r] = 1;
            }
            bench_new(&rf, 0, 0, rf.iw8, row_start8, row_end8);
        }

        for (int n = 0; n < rf.n_mfmvs; n++) {
            free(rp_ref[rf.mfmv_ref[n]]);
            rp_ref[rf.mfmv_ref[n]] = NULL;
        }
    }

    report("load_tmvs");
}

static void check_save_tmvs(const Dav1dRefmvsDSPContext *const c) {
    refmvs_block *rr[31];
    refmvs_block r[31 * 256];
    ALIGN_STK_64(refmvs_temporal_block, c_rp, 128 * 16,);
    ALIGN_STK_64(refmvs_temporal_block, a_rp, 128 * 16,);
    uint8_t ref_sign[7];

    for (int i = 0; i < 31; i++)
        rr[i] = &r[i * 256];

    declare_func(void, refmvs_temporal_block *rp, const ptrdiff_t stride,
                 refmvs_block *const *const rr, const uint8_t *const ref_sign,
                 int col_end8, int row_end8, int col_start8, int row_start8);

    if (check_func(c->save_tmvs, "save_tmvs")) {
        const int row_start8 = rnd() & 7;
        const int row_end8 = 8 + (rnd() & 7);
        const int col_start8 = rnd() & 31;
        const int col_end8 = 96 + (rnd() & 31);

        for (int i = 0; i < 7; i++)
            ref_sign[i] = rnd() & 1;

        for (int i = row_start8; i < row_end8; i++)
            for (int j = col_start8; j < col_end8;) {
                int bs = rnd() % N_BS_SIZES;
                while (j + ((dav1d_block_dimensions[bs][0] + 1) >> 1) > col_end8)
                    bs++;
                rr[i * 2][j * 2 + 1] = (refmvs_block) {
                    .mv.mv[0].x = gen_mv(14, 10),
                    .mv.mv[0].y = gen_mv(14, 10),
                    .mv.mv[1].x = gen_mv(14, 10),
                    .mv.mv[1].y = gen_mv(14, 10),
                    .ref.ref = { (rnd() % 9) - 1, (rnd() % 9) - 1 },
                    .bs = bs
                };
                for (int k = 0; k < (dav1d_block_dimensions[bs][0] + 1) >> 1; k++, j++) {
                    c_rp[i * 128 + j].mv.n = 0xdeadbeef;
                    c_rp[i * 128 + j].ref = 0xdd;
                }
            }

        call_ref(c_rp + row_start8 * 128, 128, rr, ref_sign,
                 col_end8, row_end8, col_start8, row_start8);
        call_new(a_rp + row_start8 * 128, 128, rr, ref_sign,
                 col_end8, row_end8, col_start8, row_start8);
        for (int i = row_start8; i < row_end8; i++)
            for (int j = col_start8; j < col_end8; j++)
                if (c_rp[i * 128 + j].mv.n != a_rp[i * 128 + j].mv.n ||
                    c_rp[i * 128 + j].ref != a_rp[i * 128 + j].ref)
                {
                    if (fail()) {
                        fprintf(stderr, "[%d][%d] c_rp.mv.x = 0x%x a_rp.mv.x = 0x%x\n",
                                i, j, c_rp[i * 128 + j].mv.x, a_rp[i * 128 + j].mv.x);
                        fprintf(stderr, "[%d][%d] c_rp.mv.y = 0x%x a_rp.mv.y = 0x%x\n",
                                i, j, c_rp[i * 128 + j].mv.y, a_rp[i * 128 + j].mv.y);
                        fprintf(stderr, "[%d][%d] c_rp.ref = %u a_rp.ref = %u\n",
                                i, j, c_rp[i * 128 + j].ref, a_rp[i * 128 + j].ref);
                    }
                }

        for (int bs = BS_4x4; bs < N_BS_SIZES; bs++) {
            const int bw8 = (dav1d_block_dimensions[bs][0] + 1) >> 1;
            for (int i = 0; i < 16; i++)
                for (int j = 0; j < 128; j += bw8) {
                    rr[i * 2][j * 2 + 1].ref.ref[0] = (rnd() % 9) - 1;
                    rr[i * 2][j * 2 + 1].ref.ref[1] = (rnd() % 9) - 1;
                    rr[i * 2][j * 2 + 1].bs = bs;
                }
            bench_new(alternate(c_rp, a_rp), 128, rr, ref_sign, 128, 16, 0, 0);
        }
    }

    report("save_tmvs");
}

static void check_splat_mv(const Dav1dRefmvsDSPContext *const c) {
    ALIGN_STK_64(refmvs_block, c_buf, 32 * 32,);
    ALIGN_STK_64(refmvs_block, a_buf, 32 * 32,);
    refmvs_block *c_dst[32];
    refmvs_block *a_dst[32];
    const size_t stride = 32 * sizeof(refmvs_block);

    for (int i = 0; i < 32; i++) {
        c_dst[i] = c_buf + 32 * i;
        a_dst[i] = a_buf + 32 * i;
    }

    declare_func(void, refmvs_block **rr, const refmvs_block *rmv,
                 int bx4, int bw4, int bh4);

    for (int w = 1; w <= 32; w *= 2) {
        if (check_func(c->splat_mv, "splat_mv_w%d", w)) {
            const int h_min = imax(w / 4, 1);
            const int h_max = imin(w * 4, 32);
            const int w_uint32 = w * sizeof(refmvs_block) / sizeof(uint32_t);
            for (int h = h_min; h <= h_max; h *= 2) {
                const int offset = (int) ((unsigned) w * rnd()) & 31;
                union {
                    refmvs_block rmv;
                    uint32_t u32[3];
                } ALIGN(tmp, 16);
                tmp.u32[0] = rnd();
                tmp.u32[1] = rnd();
                tmp.u32[2] = rnd();

                call_ref(c_dst, &tmp.rmv, offset, w, h);
                call_new(a_dst, &tmp.rmv, offset, w, h);
                checkasm_check(uint32_t, (uint32_t*)(c_buf + offset), stride,
                                         (uint32_t*)(a_buf + offset), stride,
                                         w_uint32, h, "dst");

                bench_new(a_dst, &tmp.rmv, 0, w, h);
            }
        }
    }
    report("splat_mv");
}

void checkasm_check_refmvs(void) {
    Dav1dRefmvsDSPContext c;
    dav1d_refmvs_dsp_init(&c);

    check_load_tmvs(&c);
    check_save_tmvs(&c);
    check_splat_mv(&c);
}
