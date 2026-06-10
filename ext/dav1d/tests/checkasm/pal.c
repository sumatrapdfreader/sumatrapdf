/*
 * Copyright © 2023, VideoLAN and dav1d authors
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
#include "src/pal.h"

#include <stdio.h>

static void check_pal_idx_finish(const Dav1dPalDSPContext *const c) {
    ALIGN_STK_64(uint8_t, src, 64 * 64,);
    ALIGN_STK_64(uint8_t, c_dst, 32 * 64,);
    ALIGN_STK_64(uint8_t, a_dst, 32 * 64,);

    declare_func(void, uint8_t *dst, const uint8_t *src,
                 int bw, int bh, int w, int h);

    for (int bw = 4; bw <= 64; bw <<= 1) {
        if (check_func(c->pal_idx_finish, "pal_idx_finish_w%d", bw)) {
            for (int bh = imax(bw / 4, 4); bh <= imin(bw * 4, 64); bh <<= 1) {
                const int w = (rnd() & (bw - 4)) + 4;
                const int h = (rnd() & (bh - 4)) + 4;
                const int dst_bw = bw / 2;

                for (int i = 0; i < bw * bh; i++)
                    src[i] = rnd() & 7;

                memset(c_dst, 0x88, dst_bw * bh);
                memset(a_dst, 0x88, dst_bw * bh);

                call_ref(c_dst, src, bw, bh, w, h);
                call_new(a_dst, src, bw, bh, w, h);
                checkasm_check(uint8_t, c_dst, dst_bw, a_dst, dst_bw,
                                        dst_bw, bh, "dst");

                bench_new(a_dst, src, bw, bh, bw, bh);
            }
        }
    }

    report("pal_idx_finish");
}

void checkasm_check_pal(void) {
    Dav1dPalDSPContext c;
    dav1d_pal_dsp_init(&c);

    check_pal_idx_finish(&c);
}
