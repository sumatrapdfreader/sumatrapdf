/*
 * Copyright © 2021, VideoLAN and dav1d authors
 * Copyright © 2021, Martin Storsjo
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

#include "src/arm/asm.S"
#include "util.S"

// void dav1d_splat_mv_neon(refmvs_block **rr, const refmvs_block *rmv,
//                          int bx4, int bw4, int bh4)

function splat_mv_neon, export=1
        ld1             {v3.16b},  [x1]
        clz             w3,  w3
        adr             x5,  L(splat_tbl)
        sub             w3,  w3,  #26
        ext             v2.16b,  v3.16b,  v3.16b,  #12
        ldrh            w3,  [x5, w3, uxtw #1]
        add             w2,  w2,  w2,  lsl #1
        ext             v0.16b,  v2.16b,  v3.16b,  #4
        sub             x3,  x5,  w3, uxtw
        ext             v1.16b,  v2.16b,  v3.16b,  #8
        lsl             w2,  w2,  #2
        ext             v2.16b,  v2.16b,  v3.16b,  #12
1:
        ldr             x1,  [x0],  #8
        subs            w4,  w4,  #1
        add             x1,  x1,  x2
        br              x3

10:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8b}, [x1]
        str             s2,  [x1, #8]
        b.gt            1b
        ret
20:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b}, [x1]
        str             d1,  [x1, #16]
        b.gt            1b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
160:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
80:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
40:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1]
        b.gt            1b
        ret

L(splat_tbl):
        .hword L(splat_tbl) -  320b
        .hword L(splat_tbl) -  160b
        .hword L(splat_tbl) -   80b
        .hword L(splat_tbl) -   40b
        .hword L(splat_tbl) -   20b
        .hword L(splat_tbl) -   10b
endfunc
