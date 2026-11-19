/*
 * Copyright © 2026, VideoLAN and dav1d authors
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

#include "tests/checkasm/internal.h"
#include "src/lf_mask.h"

#include <stdlib.h>
#include <string.h>

static void check_fill_levelcache_block(Dav1dLfMaskDSPContext *c)
{
    declare_func(void, uint8_t (*level_cache_ptr)[2], ptrdiff_t b_stride,
                 int bw, int bh, unsigned v0, unsigned v1);

    ALIGN_STK_16(uint8_t, c_dst, 32 * 64, [2]);
    ALIGN_STK_16(uint8_t, a_dst, 32 * 64, [2]);

    unsigned v0 = rnd() & 0xff, v1 = rnd() & 0xff;
    for (int w = 1; w <= 32; ++w) {
        int h = rnd() % 32 + 1;
        if (!check_func(c->fill_levelcache_block, "fill_levelcache_block"))
            continue;

        ptrdiff_t stride = w + rnd() % 32;

        memset(c_dst, 0xab, stride * h);
        memset(a_dst, 0xab, stride * h);

        checkasm_call_ref(c_dst, stride, w, h, v0, v1);
        checkasm_call_new(a_dst, stride, w, h, v0, v1);

        checkasm_check1d(uint8_t, (uint8_t*)c_dst, (uint8_t*)a_dst,
                         (int)(stride * h), "level_cache_ptr");
    }

    for (int w = 1; w <= 32; w <<= 1) {
        if (check_func(c->fill_levelcache_block,
                       "fill_levelcache_block_w%d", w)) {
            ptrdiff_t stride = 32;
            int h = w;

            bench_new(alternate(c_dst, a_dst), stride, w, h, v0, v1);
        }
    }


    checkasm_report("fill_levelcache_block");
}

void checkasm_check_lf_mask(void)
{
    Dav1dLfMaskDSPContext c;
    dav1d_lf_mask_dsp_init(&c);

    check_fill_levelcache_block(&c);
}
