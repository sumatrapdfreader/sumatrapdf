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
        push            {r4, lr}
        vld1.8          {q3},  [r1]
        ldr             r4,  [sp, #8]
        clz             r3,  r3
        adr             lr,  L(splat_tbl)
        sub             r3,  r3,  #26
        vext.8          q2,  q3,  q3,  #12
        ldr             r3,  [lr, r3, lsl #2]
        add             r2,  r2,  r2,  lsl #1
        vext.8          q0,  q2,  q3,  #4
        add             r3,  lr,  r3
        vext.8          q1,  q2,  q3,  #8
        lsl             r2,  r2,  #2
        vext.8          q2,  q2,  q3,  #12
        vmov            q3,  q0
1:
        ldr             r1,  [r0],  #4
        subs            r4,  r4,  #1
        add             r1,  r1,  r2
        bx              r3

        .align 2
L(splat_tbl):
        .word 320f - L(splat_tbl) + CONFIG_THUMB
        .word 160f - L(splat_tbl) + CONFIG_THUMB
        .word 80f  - L(splat_tbl) + CONFIG_THUMB
        .word 40f  - L(splat_tbl) + CONFIG_THUMB
        .word 20f  - L(splat_tbl) + CONFIG_THUMB
        .word 10f  - L(splat_tbl) + CONFIG_THUMB

10:
        vst1.8          {d0}, [r1]
        vstr            s2,  [r1, #8]
        bgt             1b
        pop             {r4, pc}
20:
        vst1.8          {q0}, [r1]
        vstr            d2,  [r1, #16]
        bgt             1b
        pop             {r4, pc}
40:
        vst1.8          {q0, q1}, [r1]!
        vst1.8          {q2},     [r1]
        bgt             1b
        pop             {r4, pc}
320:
        vst1.8          {q0, q1}, [r1]!
        vst1.8          {q2, q3}, [r1]!
        vst1.8          {q1, q2}, [r1]!
        vst1.8          {q0, q1}, [r1]!
        vst1.8          {q2, q3}, [r1]!
        vst1.8          {q1, q2}, [r1]!
160:
        vst1.8          {q0, q1}, [r1]!
        vst1.8          {q2, q3}, [r1]!
        vst1.8          {q1, q2}, [r1]!
80:
        vst1.8          {q0, q1}, [r1]!
        vst1.8          {q2, q3}, [r1]!
        vst1.8          {q1, q2}, [r1]
        bgt             1b
        pop             {r4, pc}
endfunc
