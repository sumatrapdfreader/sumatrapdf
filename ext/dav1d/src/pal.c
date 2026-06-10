/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Two Orioles, LLC
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

#include <string.h>

#include "common/attributes.h"

#include "src/pal.h"

// fill invisible edges and pack to 4-bit (2 pixels per byte)
static void pal_idx_finish_c(uint8_t *dst, const uint8_t *src,
                             const int bw, const int bh,
                             const int w, const int h)
{
    assert(bw >= 4 && bw <= 64 && !(bw & (bw - 1)));
    assert(bh >= 4 && bh <= 64 && !(bh & (bh - 1)));
    assert(w  >= 4 && w <= bw && !(w & 3));
    assert(h  >= 4 && h <= bh && !(h & 3));

    const int dst_w = w / 2;
    const int dst_bw = bw / 2;

    for (int y = 0; y < h; y++, src += bw, dst += dst_bw) {
        for (int x = 0; x < dst_w; x++)
            dst[x] = src[x * 2 + 0] | (src[x * 2 + 1] << 4);
        if (dst_w < dst_bw)
            memset(dst + dst_w, src[w - 1] * 0x11, dst_bw - dst_w);
    }

    if (h < bh) {
        const uint8_t *const last_row = &dst[-dst_bw];
        for (int y = h; y < bh; y++, dst += dst_bw)
            memcpy(dst, last_row, dst_bw);
    }
}

#if HAVE_ASM
#if ARCH_RISCV
#include "riscv/pal.h"
#elif ARCH_X86
#include "x86/pal.h"
#endif
#endif

COLD void dav1d_pal_dsp_init(Dav1dPalDSPContext *const c) {
    c->pal_idx_finish = pal_idx_finish_c;

#if HAVE_ASM
#if ARCH_RISCV
    pal_dsp_init_riscv(c);
#elif ARCH_X86
    pal_dsp_init_x86(c);
#endif
#endif
}
