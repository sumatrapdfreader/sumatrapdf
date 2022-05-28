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
                const int offset = (w * rnd()) & 31;
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

    check_splat_mv(&c);
}
