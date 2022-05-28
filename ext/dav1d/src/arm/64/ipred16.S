/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2019, Martin Storsjo
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

// void ipred_dc_128_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height, const int a,
//                              const int max_width, const int max_height,
//                              const int bitdepth_max);
function ipred_dc_128_16bpc_neon, export=1
        ldr             w8,  [sp]
        clz             w3,  w3
        adr             x5,  L(ipred_dc_128_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        dup             v0.8h,   w8
        sub             x5,  x5,  w3, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        urshr           v0.8h,   v0.8h,  #1
        br              x5
4:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        b.gt            4b
        ret
8:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
16:
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
        mov             v2.16b,  v0.16b
        mov             v3.16b,  v0.16b
32:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
        mov             v2.16b,  v0.16b
        mov             v3.16b,  v0.16b
        sub             x1,  x1,  #64
64:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            64b
        ret

L(ipred_dc_128_tbl):
        .hword L(ipred_dc_128_tbl) - 640b
        .hword L(ipred_dc_128_tbl) - 320b
        .hword L(ipred_dc_128_tbl) - 160b
        .hword L(ipred_dc_128_tbl) -   8b
        .hword L(ipred_dc_128_tbl) -   4b
endfunc

// void ipred_v_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                         const pixel *const topleft,
//                         const int width, const int height, const int a,
//                         const int max_width, const int max_height);
function ipred_v_16bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_v_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        add             x2,  x2,  #2
        sub             x5,  x5,  w3, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2]
4:
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2]
8:
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h}, [x2]
16:
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2]
32:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2], #64
        sub             x1,  x1,  #64
        ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x2]
64:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], x1
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], x1
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x6], x1
        b.gt            64b
        ret

L(ipred_v_tbl):
        .hword L(ipred_v_tbl) - 640b
        .hword L(ipred_v_tbl) - 320b
        .hword L(ipred_v_tbl) - 160b
        .hword L(ipred_v_tbl) -  80b
        .hword L(ipred_v_tbl) -  40b
endfunc

// void ipred_h_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                         const pixel *const topleft,
//                         const int width, const int height, const int a,
//                         const int max_width, const int max_height);
function ipred_h_16bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_h_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        sub             x2,  x2,  #8
        sub             x5,  x5,  w3, uxtw
        mov             x7,  #-8
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
4:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7
        st1             {v3.4h},  [x0], x1
        st1             {v2.4h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v1.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        b.gt            4b
        ret
8:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7
        st1             {v3.8h},  [x0], x1
        st1             {v2.8h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v1.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        b.gt            8b
        ret
16:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7
        str             q3,  [x0, #16]
        str             q2,  [x6, #16]
        st1             {v3.8h}, [x0], x1
        st1             {v2.8h}, [x6], x1
        subs            w4,  w4,  #4
        str             q1,  [x0, #16]
        str             q0,  [x6, #16]
        st1             {v1.8h}, [x0], x1
        st1             {v0.8h}, [x6], x1
        b.gt            16b
        ret
32:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7
        str             q3,  [x0, #16]
        str             q2,  [x6, #16]
        stp             q3,  q3,  [x0, #32]
        stp             q2,  q2,  [x6, #32]
        st1             {v3.8h}, [x0], x1
        st1             {v2.8h}, [x6], x1
        subs            w4,  w4,  #4
        str             q1,  [x0, #16]
        str             q0,  [x6, #16]
        stp             q1,  q1,  [x0, #32]
        stp             q0,  q0,  [x6, #32]
        st1             {v1.8h}, [x0], x1
        st1             {v0.8h}, [x6], x1
        b.gt            32b
        ret
64:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7
        str             q3,  [x0, #16]
        str             q2,  [x6, #16]
        stp             q3,  q3,  [x0, #32]
        stp             q2,  q2,  [x6, #32]
        stp             q3,  q3,  [x0, #64]
        stp             q2,  q2,  [x6, #64]
        stp             q3,  q3,  [x0, #96]
        stp             q2,  q2,  [x6, #96]
        st1             {v3.8h}, [x0], x1
        st1             {v2.8h}, [x6], x1
        subs            w4,  w4,  #4
        str             q1,  [x0, #16]
        str             q0,  [x6, #16]
        stp             q1,  q1,  [x0, #32]
        stp             q0,  q0,  [x6, #32]
        stp             q1,  q1,  [x0, #64]
        stp             q0,  q0,  [x6, #64]
        stp             q1,  q1,  [x0, #96]
        stp             q0,  q0,  [x6, #96]
        st1             {v1.8h}, [x0], x1
        st1             {v0.8h}, [x6], x1
        b.gt            64b
        ret

L(ipred_h_tbl):
        .hword L(ipred_h_tbl) - 64b
        .hword L(ipred_h_tbl) - 32b
        .hword L(ipred_h_tbl) - 16b
        .hword L(ipred_h_tbl) -  8b
        .hword L(ipred_h_tbl) -  4b
endfunc

// void ipred_dc_top_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height, const int a,
//                              const int max_width, const int max_height);
function ipred_dc_top_16bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_dc_top_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        add             x2,  x2,  #2
        sub             x5,  x5,  w3, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2]
        addv            h0,      v0.4h
        urshr           v0.4h,   v0.4h,   #2
        dup             v0.4h,   v0.h[0]
4:
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2]
        addv            h0,      v0.8h
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
8:
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h}, [x2]
        addp            v0.8h,   v0.8h,   v1.8h
        addv            h0,      v0.8h
        urshr           v2.4h,   v0.4h,   #4
        dup             v0.8h,   v2.h[0]
        dup             v1.8h,   v2.h[0]
16:
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2]
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v0.8h,   v0.8h,   v2.8h
        uaddlv          s0,      v0.8h
        rshrn           v4.4h,   v0.4s,   #5
        dup             v0.8h,   v4.h[0]
        dup             v1.8h,   v4.h[0]
        dup             v2.8h,   v4.h[0]
        dup             v3.8h,   v4.h[0]
32:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2], #64
        addp            v0.8h,   v0.8h,   v1.8h
        ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x2]
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        addp            v0.8h,   v0.8h,   v2.8h
        addp            v4.8h,   v4.8h,   v6.8h
        addp            v0.8h,   v0.8h,   v4.8h
        uaddlv          s0,      v0.8h
        rshrn           v4.4h,   v0.4s,   #6
        sub             x1,  x1,  #64
        dup             v0.8h,   v4.h[0]
        dup             v1.8h,   v4.h[0]
        dup             v2.8h,   v4.h[0]
        dup             v3.8h,   v4.h[0]
64:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            64b
        ret

L(ipred_dc_top_tbl):
        .hword L(ipred_dc_top_tbl) - 640b
        .hword L(ipred_dc_top_tbl) - 320b
        .hword L(ipred_dc_top_tbl) - 160b
        .hword L(ipred_dc_top_tbl) -  80b
        .hword L(ipred_dc_top_tbl) -  40b
endfunc

// void ipred_dc_left_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                               const pixel *const topleft,
//                               const int width, const int height, const int a,
//                               const int max_width, const int max_height);
function ipred_dc_left_16bpc_neon, export=1
        sub             x2,  x2,  w4, uxtw #1
        clz             w3,  w3
        clz             w7,  w4
        adr             x5,  L(ipred_dc_left_tbl)
        sub             w3,  w3,  #20 // 25 leading bits, minus table offset 5
        sub             w7,  w7,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        ldrh            w7,  [x5, w7, uxtw #1]
        sub             x3,  x5,  w3, uxtw
        sub             x5,  x5,  w7, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5

L(ipred_dc_left_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2]
        addv            h0,      v0.4h
        urshr           v0.4h,   v0.4h,   #2
        dup             v0.8h,   v0.h[0]
        br              x3
L(ipred_dc_left_w4):
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        b.gt            L(ipred_dc_left_w4)
        ret

L(ipred_dc_left_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2]
        addv            h0,      v0.8h
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        br              x3
L(ipred_dc_left_w8):
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        b.gt            L(ipred_dc_left_w8)
        ret

L(ipred_dc_left_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h}, [x2]
        addp            v0.8h,   v0.8h,   v1.8h
        addv            h0,      v0.8h
        urshr           v2.4h,   v0.4h,   #4
        dup             v0.8h,   v2.h[0]
        dup             v1.8h,   v2.h[0]
        br              x3
L(ipred_dc_left_w16):
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
1:
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        b.gt            1b
        ret

L(ipred_dc_left_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2]
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v0.8h,   v0.8h,   v2.8h
        uaddlp          v0.4s,   v0.8h
        addv            s0,      v0.4s
        rshrn           v4.4h,   v0.4s,   #5
        dup             v0.8h,   v4.h[0]
        br              x3
L(ipred_dc_left_w32):
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
        mov             v2.16b,  v0.16b
        mov             v3.16b,  v0.16b
1:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            1b
        ret

L(ipred_dc_left_h64):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2], #64
        addp            v0.8h,   v0.8h,   v1.8h
        ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x2]
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        addp            v0.8h,   v0.8h,   v2.8h
        addp            v4.8h,   v4.8h,   v6.8h
        addp            v0.8h,   v0.8h,   v4.8h
        uaddlv          s0,      v0.8h
        rshrn           v4.4h,   v0.4s,   #6
        dup             v0.8h,   v4.h[0]
        br              x3
L(ipred_dc_left_w64):
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
        mov             v2.16b,  v0.16b
        mov             v3.16b,  v0.16b
        sub             x1,  x1,  #64
1:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            1b
        ret

L(ipred_dc_left_tbl):
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_h64)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_h32)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_h16)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_h8)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_h4)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_w64)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_w32)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_w16)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_w8)
        .hword L(ipred_dc_left_tbl) - L(ipred_dc_left_w4)
endfunc

// void ipred_dc_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                          const pixel *const topleft,
//                          const int width, const int height, const int a,
//                          const int max_width, const int max_height);
function ipred_dc_16bpc_neon, export=1
        sub             x2,  x2,  w4, uxtw #1
        add             w7,  w3,  w4             // width + height
        clz             w3,  w3
        clz             w6,  w4
        dup             v16.4s, w7               // width + height
        adr             x5,  L(ipred_dc_tbl)
        rbit            w7,  w7                  // rbit(width + height)
        sub             w3,  w3,  #20            // 25 leading bits, minus table offset 5
        sub             w6,  w6,  #25
        clz             w7,  w7                  // ctz(width + height)
        ldrh            w3,  [x5, w3, uxtw #1]
        ldrh            w6,  [x5, w6, uxtw #1]
        neg             w7,  w7                  // -ctz(width + height)
        sub             x3,  x5,  w3, uxtw
        sub             x5,  x5,  w6, uxtw
        ushr            v16.4s,  v16.4s,  #1     // (width + height) >> 1
        dup             v17.4s,  w7              // -ctz(width + height)
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5

L(ipred_dc_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2], #8
        uaddlv          s0,      v0.4h
        add             x2,  x2,  #2
        br              x3
L(ipred_dc_w4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.4h},  [x2]
        add             v0.2s,   v0.2s,   v16.2s
        uaddlv          s1,      v1.4h
        cmp             w4,  #4
        add             v0.2s,   v0.2s,   v1.2s
        ushl            v0.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 8/16
        cmp             w4,  #16
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v0.2s,   v0.2s,   v16.2s
        ushr            v0.2s,   v0.2s,   #17
1:
        dup             v0.4h,   v0.h[0]
2:
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.4h},  [x0], x1
        st1             {v0.4h},  [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2], #16
        uaddlv          s0,      v0.8h
        add             x2,  x2,  #2
        br              x3
L(ipred_dc_w8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.8h},  [x2]
        add             v0.2s,   v0.2s,   v16.2s
        uaddlv          s1,      v1.8h
        cmp             w4,  #8
        add             v0.2s,   v0.2s,   v1.2s
        ushl            v0.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 4/16/32
        cmp             w4,  #32
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v0.2s,   v0.2s,   v16.2s
        ushr            v0.2s,   v0.2s,   #17
1:
        dup             v0.8h,   v0.h[0]
2:
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h},  [x0], x1
        st1             {v0.8h},  [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h}, [x2], #32
        addp            v0.8h,   v0.8h,   v1.8h
        add             x2,  x2,  #2
        uaddlv          s0,      v0.8h
        br              x3
L(ipred_dc_w16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.8h, v2.8h}, [x2]
        add             v0.2s,   v0.2s,   v16.2s
        addp            v1.8h,   v1.8h,   v2.8h
        uaddlv          s1,      v1.8h
        cmp             w4,  #16
        add             v0.2s,   v0.2s,   v1.2s
        ushl            v4.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 4/8/32/64
        tst             w4,  #(32+16+8) // 16 added to make a consecutive bitmask
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v4.2s,   v4.2s,   v16.2s
        ushr            v4.2s,   v4.2s,   #17
1:
        dup             v0.8h,   v4.h[0]
        dup             v1.8h,   v4.h[0]
2:
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], x1
        st1             {v0.8h, v1.8h}, [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2], #64
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v0.8h,   v0.8h,   v2.8h
        add             x2,  x2,  #2
        uaddlv          s0,      v0.8h
        br              x3
L(ipred_dc_w32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.8h, v2.8h, v3.8h, v4.8h}, [x2]
        add             v0.2s,   v0.2s,   v16.2s
        addp            v1.8h,   v1.8h,   v2.8h
        addp            v3.8h,   v3.8h,   v4.8h
        addp            v1.8h,   v1.8h,   v3.8h
        uaddlv          s1,      v1.8h
        cmp             w4,  #32
        add             v0.2s,   v0.2s,   v1.2s
        ushl            v4.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 8/16/64
        cmp             w4,  #8
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v4.2s,   v4.2s,   v16.2s
        ushr            v4.2s,   v4.2s,   #17
1:
        dup             v0.8h,   v4.h[0]
        dup             v1.8h,   v4.h[0]
        dup             v2.8h,   v4.h[0]
        dup             v3.8h,   v4.h[0]
2:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h64):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x2], #64
        addp            v0.8h,   v0.8h,   v1.8h
        ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x2], #64
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        addp            v0.8h,   v0.8h,   v2.8h
        addp            v4.8h,   v4.8h,   v6.8h
        addp            v0.8h,   v0.8h,   v4.8h
        add             x2,  x2,  #2
        uaddlv          s0,      v0.8h
        br              x3
L(ipred_dc_w64):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.8h, v2.8h, v3.8h, v4.8h}, [x2], #64
        add             v0.2s,   v0.2s,   v16.2s
        addp            v1.8h,   v1.8h,   v2.8h
        ld1             {v20.8h, v21.8h, v22.8h, v23.8h}, [x2]
        addp            v3.8h,   v3.8h,   v4.8h
        addp            v20.8h,  v20.8h,  v21.8h
        addp            v22.8h,  v22.8h,  v23.8h
        addp            v1.8h,   v1.8h,   v3.8h
        addp            v20.8h,  v20.8h,  v22.8h
        addp            v1.8h,   v1.8h,   v20.8h
        uaddlv          s1,      v1.8h
        cmp             w4,  #64
        add             v0.2s,   v0.2s,   v1.2s
        ushl            v4.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 16/32
        cmp             w4,  #16
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v4.2s,   v4.2s,   v16.2s
        ushr            v4.2s,   v4.2s,   #17
1:
        sub             x1,  x1,  #64
        dup             v0.8h,   v4.h[0]
        dup             v1.8h,   v4.h[0]
        dup             v2.8h,   v4.h[0]
        dup             v3.8h,   v4.h[0]
2:
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], #64
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x6], x1
        b.gt            2b
        ret

L(ipred_dc_tbl):
        .hword L(ipred_dc_tbl) - L(ipred_dc_h64)
        .hword L(ipred_dc_tbl) - L(ipred_dc_h32)
        .hword L(ipred_dc_tbl) - L(ipred_dc_h16)
        .hword L(ipred_dc_tbl) - L(ipred_dc_h8)
        .hword L(ipred_dc_tbl) - L(ipred_dc_h4)
        .hword L(ipred_dc_tbl) - L(ipred_dc_w64)
        .hword L(ipred_dc_tbl) - L(ipred_dc_w32)
        .hword L(ipred_dc_tbl) - L(ipred_dc_w16)
        .hword L(ipred_dc_tbl) - L(ipred_dc_w8)
        .hword L(ipred_dc_tbl) - L(ipred_dc_w4)
endfunc

// void ipred_paeth_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int a,
//                             const int max_width, const int max_height);
function ipred_paeth_16bpc_neon, export=1
        clz             w9,  w3
        adr             x5,  L(ipred_paeth_tbl)
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v4.8h},  [x2]
        add             x8,  x2,  #2
        sub             x2,  x2,  #8
        sub             x5,  x5,  w9, uxtw
        mov             x7,  #-8
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v5.2d},  [x8]
        sub             v6.8h,   v5.8h,   v4.8h   // top - topleft
4:
        ld4r            {v0.4h, v1.4h, v2.4h, v3.4h},  [x2], x7
        zip1            v0.2d,   v0.2d,   v1.2d
        zip1            v2.2d,   v2.2d,   v3.2d
        add             v16.8h,  v6.8h,   v0.8h   // base
        add             v17.8h,  v6.8h,   v2.8h
        sabd            v20.8h,  v5.8h,   v16.8h  // tdiff
        sabd            v21.8h,  v5.8h,   v17.8h
        sabd            v22.8h,  v4.8h,   v16.8h  // tldiff
        sabd            v23.8h,  v4.8h,   v17.8h
        sabd            v16.8h,  v0.8h,   v16.8h  // ldiff
        sabd            v17.8h,  v2.8h,   v17.8h
        umin            v18.8h,  v20.8h,  v22.8h  // min(tdiff, tldiff)
        umin            v19.8h,  v21.8h,  v23.8h
        cmge            v20.8h,  v22.8h,  v20.8h  // tldiff >= tdiff
        cmge            v21.8h,  v23.8h,  v21.8h
        cmge            v16.8h,  v18.8h,  v16.8h  // min(tdiff, tldiff) >= ldiff
        cmge            v17.8h,  v19.8h,  v17.8h
        bsl             v21.16b, v5.16b,  v4.16b  // tdiff <= tldiff ? top : topleft
        bsl             v20.16b, v5.16b,  v4.16b
        bit             v21.16b, v2.16b,  v17.16b // ldiff <= min ? left : ...
        bit             v20.16b, v0.16b,  v16.16b
        st1             {v21.d}[1], [x0], x1
        st1             {v21.d}[0], [x6], x1
        subs            w4,  w4,  #4
        st1             {v20.d}[1], [x0], x1
        st1             {v20.d}[0], [x6], x1
        b.gt            4b
        ret
80:
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v5.8h},  [x8], #16
        mov             w9,  w3
        // Set up pointers for four rows in parallel; x0, x6, x5, x10
        add             x5,  x0,  x1
        add             x10, x6,  x1
        lsl             x1,  x1,  #1
        sub             x1,  x1,  w3, uxtw #1
1:
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7
2:
        sub             v6.8h,   v5.8h,   v4.8h   // top - topleft
        add             v16.8h,  v6.8h,   v0.8h   // base
        add             v17.8h,  v6.8h,   v1.8h
        add             v18.8h,  v6.8h,   v2.8h
        add             v19.8h,  v6.8h,   v3.8h
        sabd            v20.8h,  v5.8h,   v16.8h  // tdiff
        sabd            v21.8h,  v5.8h,   v17.8h
        sabd            v22.8h,  v5.8h,   v18.8h
        sabd            v23.8h,  v5.8h,   v19.8h
        sabd            v24.8h,  v4.8h,   v16.8h  // tldiff
        sabd            v25.8h,  v4.8h,   v17.8h
        sabd            v26.8h,  v4.8h,   v18.8h
        sabd            v27.8h,  v4.8h,   v19.8h
        sabd            v16.8h,  v0.8h,   v16.8h  // ldiff
        sabd            v17.8h,  v1.8h,   v17.8h
        sabd            v18.8h,  v2.8h,   v18.8h
        sabd            v19.8h,  v3.8h,   v19.8h
        umin            v28.8h,  v20.8h,  v24.8h  // min(tdiff, tldiff)
        umin            v29.8h,  v21.8h,  v25.8h
        umin            v30.8h,  v22.8h,  v26.8h
        umin            v31.8h,  v23.8h,  v27.8h
        cmge            v20.8h,  v24.8h,  v20.8h  // tldiff >= tdiff
        cmge            v21.8h,  v25.8h,  v21.8h
        cmge            v22.8h,  v26.8h,  v22.8h
        cmge            v23.8h,  v27.8h,  v23.8h
        cmge            v16.8h,  v28.8h,  v16.8h  // min(tdiff, tldiff) >= ldiff
        cmge            v17.8h,  v29.8h,  v17.8h
        cmge            v18.8h,  v30.8h,  v18.8h
        cmge            v19.8h,  v31.8h,  v19.8h
        bsl             v23.16b, v5.16b,  v4.16b  // tdiff <= tldiff ? top : topleft
        bsl             v22.16b, v5.16b,  v4.16b
        bsl             v21.16b, v5.16b,  v4.16b
        bsl             v20.16b, v5.16b,  v4.16b
        bit             v23.16b, v3.16b,  v19.16b // ldiff <= min ? left : ...
        bit             v22.16b, v2.16b,  v18.16b
        bit             v21.16b, v1.16b,  v17.16b
        bit             v20.16b, v0.16b,  v16.16b
        st1             {v23.8h}, [x0], #16
        st1             {v22.8h}, [x6], #16
        subs            w3,  w3,  #8
        st1             {v21.8h}, [x5], #16
        st1             {v20.8h}, [x10], #16
        b.le            8f
        ld1             {v5.8h},  [x8], #16
        b               2b
8:
        subs            w4,  w4,  #4
        b.le            9f
        // End of horizontal loop, move pointers to next four rows
        sub             x8,  x8,  w9, uxtw #1
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        // Load the top row as early as possible
        ld1             {v5.8h},  [x8], #16
        add             x5,  x5,  x1
        add             x10, x10, x1
        mov             w3,  w9
        b               1b
9:
        ret

L(ipred_paeth_tbl):
        .hword L(ipred_paeth_tbl) - 640b
        .hword L(ipred_paeth_tbl) - 320b
        .hword L(ipred_paeth_tbl) - 160b
        .hword L(ipred_paeth_tbl) -  80b
        .hword L(ipred_paeth_tbl) -  40b
endfunc

// void ipred_smooth_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height, const int a,
//                              const int max_width, const int max_height);
function ipred_smooth_16bpc_neon, export=1
        movrel          x10, X(sm_weights)
        add             x11, x10, w4, uxtw
        add             x10, x10, w3, uxtw
        clz             w9,  w3
        adr             x5,  L(ipred_smooth_tbl)
        sub             x12, x2,  w4, uxtw #1
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v4.8h},  [x12] // bottom
        add             x8,  x2,  #2
        sub             x5,  x5,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v6.2d}, [x8]             // top
        ld1r            {v7.2s}, [x10]            // weights_hor
        sub             x2,  x2,  #8
        mov             x7,  #-8
        dup             v5.8h,   v6.h[3]          // right
        sub             v6.8h,   v6.8h,   v4.8h   // top-bottom
        uxtl            v7.8h,   v7.8b            // weights_hor
        add             v31.4h,  v4.4h,   v5.4h   // bottom+right
4:
        ld4r            {v0.4h, v1.4h, v2.4h, v3.4h},  [x2], x7 // left
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x11], #4 // weights_ver
        ushll           v20.4s,  v31.4h,  #8      // (bottom+right)*256
        ushll           v21.4s,  v31.4h,  #8
        ushll           v22.4s,  v31.4h,  #8
        ushll           v23.4s,  v31.4h,  #8
        zip1            v1.2d,   v1.2d,   v0.2d   // left, flipped
        zip1            v0.2d,   v3.2d,   v2.2d
        zip1            v16.2s,  v16.2s,  v17.2s  // weights_ver
        zip1            v18.2s,  v18.2s,  v19.2s
        sub             v0.8h,   v0.8h,   v5.8h   // left-right
        sub             v1.8h,   v1.8h,   v5.8h
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v18.8h,  v18.8b
        smlal           v20.4s,  v0.4h,   v7.4h   // += (left-right)*weights_hor
        smlal2          v21.4s,  v0.8h,   v7.8h
        smlal           v22.4s,  v1.4h,   v7.4h
        smlal2          v23.4s,  v1.8h,   v7.8h
        smlal           v20.4s,  v6.4h,   v16.4h  // += (top-bottom)*weights_ver
        smlal2          v21.4s,  v6.8h,   v16.8h
        smlal           v22.4s,  v6.4h,   v18.4h
        smlal2          v23.4s,  v6.8h,   v18.8h
        rshrn           v20.4h,  v20.4s,  #9
        rshrn           v21.4h,  v21.4s,  #9
        rshrn           v22.4h,  v22.4s,  #9
        rshrn           v23.4h,  v23.4s,  #9
        st1             {v20.4h}, [x0], x1
        st1             {v21.4h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v22.4h}, [x0], x1
        st1             {v23.4h}, [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v6.8h}, [x8]             // top
        ld1             {v7.8b}, [x10]            // weights_hor
        sub             x2,  x2,  #8
        mov             x7,  #-8
        dup             v5.8h,   v6.h[7]          // right
        sub             v6.8h,   v6.8h,   v4.8h   // top-bottom
        uxtl            v7.8h,   v7.8b            // weights_hor
        add             v31.4h,  v4.4h,   v5.4h   // bottom+right
8:
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7 // left
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x11], #4 // weights_ver
        ushll           v20.4s,  v31.4h,  #8      // (bottom+right)*256
        ushll           v21.4s,  v31.4h,  #8
        ushll           v22.4s,  v31.4h,  #8
        ushll           v23.4s,  v31.4h,  #8
        ushll           v24.4s,  v31.4h,  #8
        ushll           v25.4s,  v31.4h,  #8
        ushll           v26.4s,  v31.4h,  #8
        ushll           v27.4s,  v31.4h,  #8
        sub             v0.8h,   v0.8h,   v5.8h   // left-right
        sub             v1.8h,   v1.8h,   v5.8h
        sub             v2.8h,   v2.8h,   v5.8h
        sub             v3.8h,   v3.8h,   v5.8h
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v17.8h,  v17.8b
        uxtl            v18.8h,  v18.8b
        uxtl            v19.8h,  v19.8b
        smlal           v20.4s,  v3.4h,   v7.4h   // += (left-right)*weights_hor
        smlal2          v21.4s,  v3.8h,   v7.8h   // (left flipped)
        smlal           v22.4s,  v2.4h,   v7.4h
        smlal2          v23.4s,  v2.8h,   v7.8h
        smlal           v24.4s,  v1.4h,   v7.4h
        smlal2          v25.4s,  v1.8h,   v7.8h
        smlal           v26.4s,  v0.4h,   v7.4h
        smlal2          v27.4s,  v0.8h,   v7.8h
        smlal           v20.4s,  v6.4h,   v16.4h  // += (top-bottom)*weights_ver
        smlal2          v21.4s,  v6.8h,   v16.8h
        smlal           v22.4s,  v6.4h,   v17.4h
        smlal2          v23.4s,  v6.8h,   v17.8h
        smlal           v24.4s,  v6.4h,   v18.4h
        smlal2          v25.4s,  v6.8h,   v18.8h
        smlal           v26.4s,  v6.4h,   v19.4h
        smlal2          v27.4s,  v6.8h,   v19.8h
        rshrn           v20.4h,  v20.4s,  #9
        rshrn2          v20.8h,  v21.4s,  #9
        rshrn           v21.4h,  v22.4s,  #9
        rshrn2          v21.8h,  v23.4s,  #9
        rshrn           v22.4h,  v24.4s,  #9
        rshrn2          v22.8h,  v25.4s,  #9
        rshrn           v23.4h,  v26.4s,  #9
        rshrn2          v23.8h,  v27.4s,  #9
        st1             {v20.8h}, [x0], x1
        st1             {v21.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v22.8h}, [x0], x1
        st1             {v23.8h}, [x6], x1
        b.gt            8b
        ret
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        add             x12, x2,  w3, uxtw #1
        sub             x1,  x1,  w3, uxtw #1
        ld1r            {v5.8h}, [x12]            // right
        sub             x2,  x2,  #4
        mov             x7,  #-4
        mov             w9,  w3
        add             v31.4h,  v4.4h,   v5.4h   // bottom+right

1:
        ld2r            {v0.8h, v1.8h},   [x2],  x7 // left
        ld2r            {v16.8b, v17.8b}, [x11], #2 // weights_ver
        sub             v0.8h,   v0.8h,   v5.8h   // left-right
        sub             v1.8h,   v1.8h,   v5.8h
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v17.8h,  v17.8b
2:
        ld1             {v7.16b}, [x10],  #16     // weights_hor
        ld1             {v2.8h, v3.8h}, [x8], #32 // top
        ushll           v20.4s,  v31.4h,  #8      // (bottom+right)*256
        ushll           v21.4s,  v31.4h,  #8
        ushll           v22.4s,  v31.4h,  #8
        ushll           v23.4s,  v31.4h,  #8
        ushll           v24.4s,  v31.4h,  #8
        ushll           v25.4s,  v31.4h,  #8
        ushll           v26.4s,  v31.4h,  #8
        ushll           v27.4s,  v31.4h,  #8
        uxtl            v6.8h,   v7.8b            // weights_hor
        uxtl2           v7.8h,   v7.16b
        sub             v2.8h,   v2.8h,   v4.8h   // top-bottom
        sub             v3.8h,   v3.8h,   v4.8h
        smlal           v20.4s,  v1.4h,   v6.4h   // += (left-right)*weights_hor
        smlal2          v21.4s,  v1.8h,   v6.8h   // (left flipped)
        smlal           v22.4s,  v1.4h,   v7.4h
        smlal2          v23.4s,  v1.8h,   v7.8h
        smlal           v24.4s,  v0.4h,   v6.4h
        smlal2          v25.4s,  v0.8h,   v6.8h
        smlal           v26.4s,  v0.4h,   v7.4h
        smlal2          v27.4s,  v0.8h,   v7.8h
        smlal           v20.4s,  v2.4h,   v16.4h  // += (top-bottom)*weights_ver
        smlal2          v21.4s,  v2.8h,   v16.8h
        smlal           v22.4s,  v3.4h,   v16.4h
        smlal2          v23.4s,  v3.8h,   v16.8h
        smlal           v24.4s,  v2.4h,   v17.4h
        smlal2          v25.4s,  v2.8h,   v17.8h
        smlal           v26.4s,  v3.4h,   v17.4h
        smlal2          v27.4s,  v3.8h,   v17.8h
        rshrn           v20.4h,  v20.4s,  #9
        rshrn2          v20.8h,  v21.4s,  #9
        rshrn           v21.4h,  v22.4s,  #9
        rshrn2          v21.8h,  v23.4s,  #9
        rshrn           v22.4h,  v24.4s,  #9
        rshrn2          v22.8h,  v25.4s,  #9
        rshrn           v23.4h,  v26.4s,  #9
        rshrn2          v23.8h,  v27.4s,  #9
        subs            w3,  w3,  #16
        st1             {v20.8h, v21.8h}, [x0], #32
        st1             {v22.8h, v23.8h}, [x6], #32
        b.gt            2b
        subs            w4,  w4,  #2
        b.le            9f
        sub             x8,  x8,  w9, uxtw #1
        sub             x10, x10, w9, uxtw
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        mov             w3,  w9
        b               1b
9:
        ret

L(ipred_smooth_tbl):
        .hword L(ipred_smooth_tbl) - 640b
        .hword L(ipred_smooth_tbl) - 320b
        .hword L(ipred_smooth_tbl) - 160b
        .hword L(ipred_smooth_tbl) -  80b
        .hword L(ipred_smooth_tbl) -  40b
endfunc

// void ipred_smooth_v_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                                const pixel *const topleft,
//                                const int width, const int height, const int a,
//                                const int max_width, const int max_height);
function ipred_smooth_v_16bpc_neon, export=1
        movrel          x7,  X(sm_weights)
        add             x7,  x7,  w4, uxtw
        clz             w9,  w3
        adr             x5,  L(ipred_smooth_v_tbl)
        sub             x8,  x2,  w4, uxtw #1
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v4.8h},  [x8] // bottom
        add             x2,  x2,  #2
        sub             x5,  x5,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v6.2d}, [x2]             // top
        sub             v6.8h,   v6.8h,   v4.8h   // top-bottom
4:
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x7], #4 // weights_ver
        zip1            v16.2s,  v16.2s,  v17.2s  // weights_ver
        zip1            v18.2s,  v18.2s,  v19.2s
        ushll           v16.8h,  v16.8b,  #7      // weights_ver << 7
        ushll           v18.8h,  v18.8b,  #7
        sqrdmulh        v20.8h,  v6.8h,   v16.8h  // ((top-bottom)*weights_ver + 128) >> 8
        sqrdmulh        v21.8h,  v6.8h,   v18.8h
        add             v20.8h,  v20.8h,  v4.8h
        add             v21.8h,  v21.8h,  v4.8h
        st1             {v20.d}[0], [x0], x1
        st1             {v20.d}[1], [x6], x1
        subs            w4,  w4,  #4
        st1             {v21.d}[0], [x0], x1
        st1             {v21.d}[1], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v6.8h}, [x2]             // top
        sub             v6.8h,   v6.8h,   v4.8h   // top-bottom
8:
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x7], #4 // weights_ver
        ushll           v16.8h,  v16.8b,  #7      // weights_ver << 7
        ushll           v17.8h,  v17.8b,  #7
        ushll           v18.8h,  v18.8b,  #7
        ushll           v19.8h,  v19.8b,  #7
        sqrdmulh        v20.8h,  v6.8h,   v16.8h  // ((top-bottom)*weights_ver + 128) >> 8
        sqrdmulh        v21.8h,  v6.8h,   v17.8h
        sqrdmulh        v22.8h,  v6.8h,   v18.8h
        sqrdmulh        v23.8h,  v6.8h,   v19.8h
        add             v20.8h,  v20.8h,  v4.8h
        add             v21.8h,  v21.8h,  v4.8h
        add             v22.8h,  v22.8h,  v4.8h
        add             v23.8h,  v23.8h,  v4.8h
        st1             {v20.8h}, [x0], x1
        st1             {v21.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v22.8h}, [x0], x1
        st1             {v23.8h}, [x6], x1
        b.gt            8b
        ret
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        // Set up pointers for four rows in parallel; x0, x6, x5, x8
        add             x5,  x0,  x1
        add             x8,  x6,  x1
        lsl             x1,  x1,  #1
        sub             x1,  x1,  w3, uxtw #1
        mov             w9,  w3

1:
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b}, [x7], #4 // weights_ver
        ushll           v16.8h,  v16.8b,  #7      // weights_ver << 7
        ushll           v17.8h,  v17.8b,  #7
        ushll           v18.8h,  v18.8b,  #7
        ushll           v19.8h,  v19.8b,  #7
2:
        ld1             {v2.8h, v3.8h}, [x2], #32 // top
        sub             v2.8h,   v2.8h,   v4.8h   // top-bottom
        sub             v3.8h,   v3.8h,   v4.8h
        sqrdmulh        v20.8h,  v2.8h,   v16.8h  // ((top-bottom)*weights_ver + 128) >> 8
        sqrdmulh        v21.8h,  v3.8h,   v16.8h
        sqrdmulh        v22.8h,  v2.8h,   v17.8h
        sqrdmulh        v23.8h,  v3.8h,   v17.8h
        sqrdmulh        v24.8h,  v2.8h,   v18.8h
        sqrdmulh        v25.8h,  v3.8h,   v18.8h
        sqrdmulh        v26.8h,  v2.8h,   v19.8h
        sqrdmulh        v27.8h,  v3.8h,   v19.8h
        add             v20.8h,  v20.8h,  v4.8h
        add             v21.8h,  v21.8h,  v4.8h
        add             v22.8h,  v22.8h,  v4.8h
        add             v23.8h,  v23.8h,  v4.8h
        add             v24.8h,  v24.8h,  v4.8h
        add             v25.8h,  v25.8h,  v4.8h
        add             v26.8h,  v26.8h,  v4.8h
        add             v27.8h,  v27.8h,  v4.8h
        subs            w3,  w3,  #16
        st1             {v20.8h, v21.8h}, [x0], #32
        st1             {v22.8h, v23.8h}, [x6], #32
        st1             {v24.8h, v25.8h}, [x5], #32
        st1             {v26.8h, v27.8h}, [x8], #32
        b.gt            2b
        subs            w4,  w4,  #4
        b.le            9f
        sub             x2,  x2,  w9, uxtw #1
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        add             x5,  x5,  x1
        add             x8,  x8,  x1
        mov             w3,  w9
        b               1b
9:
        ret

L(ipred_smooth_v_tbl):
        .hword L(ipred_smooth_v_tbl) - 640b
        .hword L(ipred_smooth_v_tbl) - 320b
        .hword L(ipred_smooth_v_tbl) - 160b
        .hword L(ipred_smooth_v_tbl) -  80b
        .hword L(ipred_smooth_v_tbl) -  40b
endfunc

// void ipred_smooth_h_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                                const pixel *const topleft,
//                                const int width, const int height, const int a,
//                                const int max_width, const int max_height);
function ipred_smooth_h_16bpc_neon, export=1
        movrel          x8,  X(sm_weights)
        add             x8,  x8,  w3, uxtw
        clz             w9,  w3
        adr             x5,  L(ipred_smooth_h_tbl)
        add             x12, x2,  w3, uxtw #1
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v5.8h},  [x12] // right
        sub             x5,  x5,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v7.2s}, [x8]             // weights_hor
        sub             x2,  x2,  #8
        mov             x7,  #-8
        ushll           v7.8h,   v7.8b,   #7      // weights_hor << 7
4:
        ld4r            {v0.4h, v1.4h, v2.4h, v3.4h},  [x2], x7 // left
        zip1            v1.2d,   v1.2d,   v0.2d   // left, flipped
        zip1            v0.2d,   v3.2d,   v2.2d
        sub             v0.8h,   v0.8h,   v5.8h   // left-right
        sub             v1.8h,   v1.8h,   v5.8h
        sqrdmulh        v20.8h,  v0.8h,   v7.8h   // ((left-right)*weights_hor + 128) >> 8
        sqrdmulh        v21.8h,  v1.8h,   v7.8h
        add             v20.8h,  v20.8h,  v5.8h
        add             v21.8h,  v21.8h,  v5.8h
        st1             {v20.d}[0], [x0], x1
        st1             {v20.d}[1], [x6], x1
        subs            w4,  w4,  #4
        st1             {v21.d}[0], [x0], x1
        st1             {v21.d}[1], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v7.8b}, [x8]             // weights_hor
        sub             x2,  x2,  #8
        mov             x7,  #-8
        ushll           v7.8h,   v7.8b,   #7      // weights_hor << 7
8:
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},  [x2], x7 // left
        sub             v3.8h,   v3.8h,   v5.8h   // left-right
        sub             v2.8h,   v2.8h,   v5.8h
        sub             v1.8h,   v1.8h,   v5.8h
        sub             v0.8h,   v0.8h,   v5.8h
        sqrdmulh        v20.8h,  v3.8h,   v7.8h   // ((left-right)*weights_hor + 128) >> 8
        sqrdmulh        v21.8h,  v2.8h,   v7.8h   // (left flipped)
        sqrdmulh        v22.8h,  v1.8h,   v7.8h
        sqrdmulh        v23.8h,  v0.8h,   v7.8h
        add             v20.8h,  v20.8h,  v5.8h
        add             v21.8h,  v21.8h,  v5.8h
        add             v22.8h,  v22.8h,  v5.8h
        add             v23.8h,  v23.8h,  v5.8h
        st1             {v20.8h}, [x0], x1
        st1             {v21.8h}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v22.8h}, [x0], x1
        st1             {v23.8h}, [x6], x1
        b.gt            8b
        ret
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        sub             x2,  x2,  #8
        mov             x7,  #-8
        // Set up pointers for four rows in parallel; x0, x6, x5, x10
        add             x5,  x0,  x1
        add             x10, x6,  x1
        lsl             x1,  x1,  #1
        sub             x1,  x1,  w3, uxtw #1
        mov             w9,  w3

1:
        ld4r            {v0.8h, v1.8h, v2.8h, v3.8h},   [x2],  x7 // left
        sub             v0.8h,   v0.8h,   v5.8h   // left-right
        sub             v1.8h,   v1.8h,   v5.8h
        sub             v2.8h,   v2.8h,   v5.8h
        sub             v3.8h,   v3.8h,   v5.8h
2:
        ld1             {v7.16b}, [x8],   #16     // weights_hor
        ushll           v6.8h,   v7.8b,   #7      // weights_hor << 7
        ushll2          v7.8h,   v7.16b,  #7
        sqrdmulh        v20.8h,  v3.8h,   v6.8h   // ((left-right)*weights_hor + 128) >> 8
        sqrdmulh        v21.8h,  v3.8h,   v7.8h   // (left flipped)
        sqrdmulh        v22.8h,  v2.8h,   v6.8h
        sqrdmulh        v23.8h,  v2.8h,   v7.8h
        sqrdmulh        v24.8h,  v1.8h,   v6.8h
        sqrdmulh        v25.8h,  v1.8h,   v7.8h
        sqrdmulh        v26.8h,  v0.8h,   v6.8h
        sqrdmulh        v27.8h,  v0.8h,   v7.8h
        add             v20.8h,  v20.8h,  v5.8h
        add             v21.8h,  v21.8h,  v5.8h
        add             v22.8h,  v22.8h,  v5.8h
        add             v23.8h,  v23.8h,  v5.8h
        add             v24.8h,  v24.8h,  v5.8h
        add             v25.8h,  v25.8h,  v5.8h
        add             v26.8h,  v26.8h,  v5.8h
        add             v27.8h,  v27.8h,  v5.8h
        subs            w3,  w3,  #16
        st1             {v20.8h, v21.8h}, [x0],  #32
        st1             {v22.8h, v23.8h}, [x6],  #32
        st1             {v24.8h, v25.8h}, [x5],  #32
        st1             {v26.8h, v27.8h}, [x10], #32
        b.gt            2b
        subs            w4,  w4,  #4
        b.le            9f
        sub             x8,  x8,  w9, uxtw
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        add             x5,  x5,  x1
        add             x10, x10, x1
        mov             w3,  w9
        b               1b
9:
        ret

L(ipred_smooth_h_tbl):
        .hword L(ipred_smooth_h_tbl) - 640b
        .hword L(ipred_smooth_h_tbl) - 320b
        .hword L(ipred_smooth_h_tbl) - 160b
        .hword L(ipred_smooth_h_tbl) -  80b
        .hword L(ipred_smooth_h_tbl) -  40b
endfunc

// void ipred_filter_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height, const int filt_idx,
//                              const int max_width, const int max_height,
//                              const int bitdepth_max);
.macro filter_fn bpc
function ipred_filter_\bpc\()bpc_neon
        and             w5,  w5,  #511
        movrel          x6,  X(filter_intra_taps)
        lsl             w5,  w5,  #6
        add             x6,  x6,  w5, uxtw
        ld1             {v16.8b, v17.8b, v18.8b, v19.8b}, [x6], #32
        clz             w9,  w3
        adr             x5,  L(ipred_filter\bpc\()_tbl)
        ld1             {v20.8b, v21.8b, v22.8b}, [x6]
        sub             w9,  w9,  #26
        ldrh            w9,  [x5, w9, uxtw #1]
        sxtl            v16.8h,  v16.8b
        sxtl            v17.8h,  v17.8b
        sub             x5,  x5,  w9, uxtw
        sxtl            v18.8h,  v18.8b
        sxtl            v19.8h,  v19.8b
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        sxtl            v20.8h,  v20.8b
        sxtl            v21.8h,  v21.8b
        sxtl            v22.8h,  v22.8b
        dup             v31.8h,  w8
.if \bpc == 10
        movi            v30.8h,  #0
.endif
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ldur            d0,  [x2, #2]             // top (0-3)
        sub             x2,  x2,  #4
        mov             x7,  #-4
4:
        ld1             {v1.4h}, [x2], x7         // left (0-1) + topleft (2)
.if \bpc == 10
        mul             v2.8h,   v17.8h,  v0.h[0] // p1(top[0]) * filter(1)
        mla             v2.8h,   v18.8h,  v0.h[1] // p2(top[1]) * filter(2)
        mla             v2.8h,   v19.8h,  v0.h[2] // p3(top[2]) * filter(3)
        mla             v2.8h,   v20.8h,  v0.h[3] // p4(top[3]) * filter(4)
        mla             v2.8h,   v16.8h,  v1.h[2] // p0(topleft) * filter(0)
        mla             v2.8h,   v21.8h,  v1.h[1] // p5(left[0]) * filter(5)
        mla             v2.8h,   v22.8h,  v1.h[0] // p6(left[1]) * filter(6)
        srshr           v2.8h,   v2.8h,   #4
        smax            v2.8h,   v2.8h,   v30.8h
.else
        smull           v2.4s,   v17.4h,  v0.h[0] // p1(top[0]) * filter(1)
        smlal           v2.4s,   v18.4h,  v0.h[1] // p2(top[1]) * filter(2)
        smlal           v2.4s,   v19.4h,  v0.h[2] // p3(top[2]) * filter(3)
        smlal           v2.4s,   v20.4h,  v0.h[3] // p4(top[3]) * filter(4)
        smlal           v2.4s,   v16.4h,  v1.h[2] // p0(topleft) * filter(0)
        smlal           v2.4s,   v21.4h,  v1.h[1] // p5(left[0]) * filter(5)
        smlal           v2.4s,   v22.4h,  v1.h[0] // p6(left[1]) * filter(6)
        smull2          v3.4s,   v17.8h,  v0.h[0] // p1(top[0]) * filter(1)
        smlal2          v3.4s,   v18.8h,  v0.h[1] // p2(top[1]) * filter(2)
        smlal2          v3.4s,   v19.8h,  v0.h[2] // p3(top[2]) * filter(3)
        smlal2          v3.4s,   v20.8h,  v0.h[3] // p4(top[3]) * filter(4)
        smlal2          v3.4s,   v16.8h,  v1.h[2] // p0(topleft) * filter(0)
        smlal2          v3.4s,   v21.8h,  v1.h[1] // p5(left[0]) * filter(5)
        smlal2          v3.4s,   v22.8h,  v1.h[0] // p6(left[1]) * filter(6)
        sqrshrun        v2.4h,   v2.4s,   #4
        sqrshrun2       v2.8h,   v3.4s,   #4
.endif
        smin            v2.8h,   v2.8h,   v31.8h
        subs            w4,  w4,  #2
        st1             {v2.d}[0], [x0], x1
        ext             v0.16b,  v2.16b,  v2.16b, #8 // move top from [4-7] to [0-3]
        st1             {v2.d}[1], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ldur            q0,  [x2, #2]             // top (0-7)
        sub             x2,  x2,  #4
        mov             x7,  #-4
8:
        ld1             {v1.4h}, [x2], x7         // left (0-1) + topleft (2)
.if \bpc == 10
        mul             v2.8h,   v17.8h,  v0.h[0] // p1(top[0]) * filter(1)
        mla             v2.8h,   v18.8h,  v0.h[1] // p2(top[1]) * filter(2)
        mla             v2.8h,   v19.8h,  v0.h[2] // p3(top[2]) * filter(3)
        mla             v2.8h,   v20.8h,  v0.h[3] // p4(top[3]) * filter(4)
        mla             v2.8h,   v16.8h,  v1.h[2] // p0(topleft) * filter(0)
        mla             v2.8h,   v21.8h,  v1.h[1] // p5(left[0]) * filter(5)
        mla             v2.8h,   v22.8h,  v1.h[0] // p6(left[1]) * filter(6)
        mul             v3.8h,   v17.8h,  v0.h[4] // p1(top[0]) * filter(1)
        mla             v3.8h,   v18.8h,  v0.h[5] // p2(top[1]) * filter(2)
        mla             v3.8h,   v19.8h,  v0.h[6] // p3(top[2]) * filter(3)
        srshr           v2.8h,   v2.8h,   #4
        smax            v2.8h,   v2.8h,   v30.8h
        smin            v2.8h,   v2.8h,   v31.8h
        mla             v3.8h,   v20.8h,  v0.h[7] // p4(top[3]) * filter(4)
        mla             v3.8h,   v16.8h,  v0.h[3] // p0(topleft) * filter(0)
        mla             v3.8h,   v21.8h,  v2.h[3] // p5(left[0]) * filter(5)
        mla             v3.8h,   v22.8h,  v2.h[7] // p6(left[1]) * filter(6)
        srshr           v3.8h,   v3.8h,   #4
        smax            v3.8h,   v3.8h,   v30.8h
.else
        smull           v2.4s,   v17.4h,  v0.h[0] // p1(top[0]) * filter(1)
        smlal           v2.4s,   v18.4h,  v0.h[1] // p2(top[1]) * filter(2)
        smlal           v2.4s,   v19.4h,  v0.h[2] // p3(top[2]) * filter(3)
        smlal           v2.4s,   v20.4h,  v0.h[3] // p4(top[3]) * filter(4)
        smlal           v2.4s,   v16.4h,  v1.h[2] // p0(topleft) * filter(0)
        smlal           v2.4s,   v21.4h,  v1.h[1] // p5(left[0]) * filter(5)
        smlal           v2.4s,   v22.4h,  v1.h[0] // p6(left[1]) * filter(6)
        smull2          v3.4s,   v17.8h,  v0.h[0] // p1(top[0]) * filter(1)
        smlal2          v3.4s,   v18.8h,  v0.h[1] // p2(top[1]) * filter(2)
        smlal2          v3.4s,   v19.8h,  v0.h[2] // p3(top[2]) * filter(3)
        smlal2          v3.4s,   v20.8h,  v0.h[3] // p4(top[3]) * filter(4)
        smlal2          v3.4s,   v16.8h,  v1.h[2] // p0(topleft) * filter(0)
        smlal2          v3.4s,   v21.8h,  v1.h[1] // p5(left[0]) * filter(5)
        smlal2          v3.4s,   v22.8h,  v1.h[0] // p6(left[1]) * filter(6)
        smull           v4.4s,   v17.4h,  v0.h[4] // p1(top[0]) * filter(1)
        smlal           v4.4s,   v18.4h,  v0.h[5] // p2(top[1]) * filter(2)
        smlal           v4.4s,   v19.4h,  v0.h[6] // p3(top[2]) * filter(3)
        sqrshrun        v2.4h,   v2.4s,   #4
        sqrshrun2       v2.8h,   v3.4s,   #4
        smin            v2.8h,   v2.8h,   v31.8h
        smlal           v4.4s,   v20.4h,  v0.h[7] // p4(top[3]) * filter(4)
        smlal           v4.4s,   v16.4h,  v0.h[3] // p0(topleft) * filter(0)
        smlal           v4.4s,   v21.4h,  v2.h[3] // p5(left[0]) * filter(5)
        smlal           v4.4s,   v22.4h,  v2.h[7] // p6(left[1]) * filter(6)
        smull2          v5.4s,   v17.8h,  v0.h[4] // p1(top[0]) * filter(1)
        smlal2          v5.4s,   v18.8h,  v0.h[5] // p2(top[1]) * filter(2)
        smlal2          v5.4s,   v19.8h,  v0.h[6] // p3(top[2]) * filter(3)
        smlal2          v5.4s,   v20.8h,  v0.h[7] // p4(top[3]) * filter(4)
        smlal2          v5.4s,   v16.8h,  v0.h[3] // p0(topleft) * filter(0)
        smlal2          v5.4s,   v21.8h,  v2.h[3] // p5(left[0]) * filter(5)
        smlal2          v5.4s,   v22.8h,  v2.h[7] // p6(left[1]) * filter(6)
        sqrshrun        v3.4h,   v4.4s,   #4
        sqrshrun2       v3.8h,   v5.4s,   #4
.endif
        smin            v3.8h,   v3.8h,   v31.8h
        subs            w4,  w4,  #2
        st2             {v2.d, v3.d}[0], [x0], x1
        zip2            v0.2d,   v2.2d,   v3.2d
        st2             {v2.d, v3.d}[1], [x6], x1
        b.gt            8b
        ret
160:
320:
        AARCH64_VALID_JUMP_TARGET
        add             x8,  x2,  #2
        sub             x2,  x2,  #4
        mov             x7,  #-4
        sub             x1,  x1,  w3, uxtw #1
        mov             w9,  w3

1:
        ld1             {v0.4h}, [x2], x7         // left (0-1) + topleft (2)
2:
        ld1             {v1.8h, v2.8h}, [x8], #32 // top(0-15)
.if \bpc == 10
        mul             v3.8h,   v16.8h,  v0.h[2] // p0(topleft) * filter(0)
        mla             v3.8h,   v21.8h,  v0.h[1] // p5(left[0]) * filter(5)
        mla             v3.8h,   v22.8h,  v0.h[0] // p6(left[1]) * filter(6)
        mla             v3.8h,   v17.8h,  v1.h[0] // p1(top[0]) * filter(1)
        mla             v3.8h,   v18.8h,  v1.h[1] // p2(top[1]) * filter(2)
        mla             v3.8h,   v19.8h,  v1.h[2] // p3(top[2]) * filter(3)
        mla             v3.8h,   v20.8h,  v1.h[3] // p4(top[3]) * filter(4)

        mul             v4.8h,   v17.8h,  v1.h[4] // p1(top[0]) * filter(1)
        mla             v4.8h,   v18.8h,  v1.h[5] // p2(top[1]) * filter(2)
        mla             v4.8h,   v19.8h,  v1.h[6] // p3(top[2]) * filter(3)
        srshr           v3.8h,   v3.8h,   #4
        smax            v3.8h,   v3.8h,   v30.8h
        smin            v3.8h,   v3.8h,   v31.8h
        mla             v4.8h,   v20.8h,  v1.h[7] // p4(top[3]) * filter(4)
        mla             v4.8h,   v16.8h,  v1.h[3] // p0(topleft) * filter(0)
        mla             v4.8h,   v21.8h,  v3.h[3] // p5(left[0]) * filter(5)
        mla             v4.8h,   v22.8h,  v3.h[7] // p6(left[1]) * filter(6)

        mul             v5.8h,   v17.8h,  v2.h[0] // p1(top[0]) * filter(1)
        mla             v5.8h,   v18.8h,  v2.h[1] // p2(top[1]) * filter(2)
        mla             v5.8h,   v19.8h,  v2.h[2] // p3(top[2]) * filter(3)
        srshr           v4.8h,   v4.8h,   #4
        smax            v4.8h,   v4.8h,   v30.8h
        smin            v4.8h,   v4.8h,   v31.8h
        mla             v5.8h,   v20.8h,  v2.h[3] // p4(top[3]) * filter(4)
        mla             v5.8h,   v16.8h,  v1.h[7] // p0(topleft) * filter(0)
        mla             v5.8h,   v21.8h,  v4.h[3] // p5(left[0]) * filter(5)
        mla             v5.8h,   v22.8h,  v4.h[7] // p6(left[1]) * filter(6)

        mul             v6.8h,   v17.8h,  v2.h[4] // p1(top[0]) * filter(1)
        mla             v6.8h,   v18.8h,  v2.h[5] // p2(top[1]) * filter(2)
        mla             v6.8h,   v19.8h,  v2.h[6] // p3(top[2]) * filter(3)
        srshr           v5.8h,   v5.8h,   #4
        smax            v5.8h,   v5.8h,   v30.8h
        smin            v5.8h,   v5.8h,   v31.8h
        mla             v6.8h,   v20.8h,  v2.h[7] // p4(top[3]) * filter(4)
        mla             v6.8h,   v16.8h,  v2.h[3] // p0(topleft) * filter(0)
        mla             v6.8h,   v21.8h,  v5.h[3] // p5(left[0]) * filter(5)
        mla             v6.8h,   v22.8h,  v5.h[7] // p6(left[1]) * filter(6)

        subs            w3,  w3,  #16
        srshr           v6.8h,   v6.8h,   #4
        smax            v6.8h,   v6.8h,   v30.8h
.else
        smull           v3.4s,   v16.4h,  v0.h[2] // p0(topleft) * filter(0)
        smlal           v3.4s,   v21.4h,  v0.h[1] // p5(left[0]) * filter(5)
        smlal           v3.4s,   v22.4h,  v0.h[0] // p6(left[1]) * filter(6)
        smlal           v3.4s,   v17.4h,  v1.h[0] // p1(top[0]) * filter(1)
        smlal           v3.4s,   v18.4h,  v1.h[1] // p2(top[1]) * filter(2)
        smlal           v3.4s,   v19.4h,  v1.h[2] // p3(top[2]) * filter(3)
        smlal           v3.4s,   v20.4h,  v1.h[3] // p4(top[3]) * filter(4)
        smull2          v4.4s,   v16.8h,  v0.h[2] // p0(topleft) * filter(0)
        smlal2          v4.4s,   v21.8h,  v0.h[1] // p5(left[0]) * filter(5)
        smlal2          v4.4s,   v22.8h,  v0.h[0] // p6(left[1]) * filter(6)
        smlal2          v4.4s,   v17.8h,  v1.h[0] // p1(top[0]) * filter(1)
        smlal2          v4.4s,   v18.8h,  v1.h[1] // p2(top[1]) * filter(2)
        smlal2          v4.4s,   v19.8h,  v1.h[2] // p3(top[2]) * filter(3)
        smlal2          v4.4s,   v20.8h,  v1.h[3] // p4(top[3]) * filter(4)

        smull           v5.4s,   v17.4h,  v1.h[4] // p1(top[0]) * filter(1)
        smlal           v5.4s,   v18.4h,  v1.h[5] // p2(top[1]) * filter(2)
        smlal           v5.4s,   v19.4h,  v1.h[6] // p3(top[2]) * filter(3)
        sqrshrun        v3.4h,   v3.4s,   #4
        sqrshrun2       v3.8h,   v4.4s,   #4
        smin            v3.8h,   v3.8h,   v31.8h
        smlal           v5.4s,   v20.4h,  v1.h[7] // p4(top[3]) * filter(4)
        smlal           v5.4s,   v16.4h,  v1.h[3] // p0(topleft) * filter(0)
        smlal           v5.4s,   v21.4h,  v3.h[3] // p5(left[0]) * filter(5)
        smlal           v5.4s,   v22.4h,  v3.h[7] // p6(left[1]) * filter(6)
        smull2          v6.4s,   v17.8h,  v1.h[4] // p1(top[0]) * filter(1)
        smlal2          v6.4s,   v18.8h,  v1.h[5] // p2(top[1]) * filter(2)
        smlal2          v6.4s,   v19.8h,  v1.h[6] // p3(top[2]) * filter(3)
        smlal2          v6.4s,   v20.8h,  v1.h[7] // p4(top[3]) * filter(4)
        smlal2          v6.4s,   v16.8h,  v1.h[3] // p0(topleft) * filter(0)
        smlal2          v6.4s,   v21.8h,  v3.h[3] // p5(left[0]) * filter(5)
        smlal2          v6.4s,   v22.8h,  v3.h[7] // p6(left[1]) * filter(6)

        smull           v24.4s,  v17.4h,  v2.h[0] // p1(top[0]) * filter(1)
        smlal           v24.4s,  v18.4h,  v2.h[1] // p2(top[1]) * filter(2)
        smlal           v24.4s,  v19.4h,  v2.h[2] // p3(top[2]) * filter(3)
        sqrshrun        v4.4h,   v5.4s,   #4
        sqrshrun2       v4.8h,   v6.4s,   #4
        smin            v4.8h,   v4.8h,   v31.8h
        smlal           v24.4s,  v20.4h,  v2.h[3] // p4(top[3]) * filter(4)
        smlal           v24.4s,  v16.4h,  v1.h[7] // p0(topleft) * filter(0)
        smlal           v24.4s,  v21.4h,  v4.h[3] // p5(left[0]) * filter(5)
        smlal           v24.4s,  v22.4h,  v4.h[7] // p6(left[1]) * filter(6)
        smull2          v25.4s,  v17.8h,  v2.h[0] // p1(top[0]) * filter(1)
        smlal2          v25.4s,  v18.8h,  v2.h[1] // p2(top[1]) * filter(2)
        smlal2          v25.4s,  v19.8h,  v2.h[2] // p3(top[2]) * filter(3)
        smlal2          v25.4s,  v20.8h,  v2.h[3] // p4(top[3]) * filter(4)
        smlal2          v25.4s,  v16.8h,  v1.h[7] // p0(topleft) * filter(0)
        smlal2          v25.4s,  v21.8h,  v4.h[3] // p5(left[0]) * filter(5)
        smlal2          v25.4s,  v22.8h,  v4.h[7] // p6(left[1]) * filter(6)

        smull           v26.4s,  v17.4h,  v2.h[4] // p1(top[0]) * filter(1)
        smlal           v26.4s,  v18.4h,  v2.h[5] // p2(top[1]) * filter(2)
        smlal           v26.4s,  v19.4h,  v2.h[6] // p3(top[2]) * filter(3)
        sqrshrun        v5.4h,   v24.4s,  #4
        sqrshrun2       v5.8h,   v25.4s,  #4
        smin            v5.8h,   v5.8h,   v31.8h
        smlal           v26.4s,  v20.4h,  v2.h[7] // p4(top[3]) * filter(4)
        smlal           v26.4s,  v16.4h,  v2.h[3] // p0(topleft) * filter(0)
        smlal           v26.4s,  v21.4h,  v5.h[3] // p5(left[0]) * filter(5)
        smlal           v26.4s,  v22.4h,  v5.h[7] // p6(left[1]) * filter(6)
        smull2          v27.4s,  v17.8h,  v2.h[4] // p1(top[0]) * filter(1)
        smlal2          v27.4s,  v18.8h,  v2.h[5] // p2(top[1]) * filter(2)
        smlal2          v27.4s,  v19.8h,  v2.h[6] // p3(top[2]) * filter(3)
        smlal2          v27.4s,  v20.8h,  v2.h[7] // p4(top[3]) * filter(4)
        smlal2          v27.4s,  v16.8h,  v2.h[3] // p0(topleft) * filter(0)
        smlal2          v27.4s,  v21.8h,  v5.h[3] // p5(left[0]) * filter(5)
        smlal2          v27.4s,  v22.8h,  v5.h[7] // p6(left[1]) * filter(6)

        subs            w3,  w3,  #16
        sqrshrun        v6.4h,   v26.4s,  #4
        sqrshrun2       v6.8h,   v27.4s,  #4
.endif
        smin            v6.8h,   v6.8h,   v31.8h

        ins             v0.h[2], v2.h[7]
        st4             {v3.d, v4.d, v5.d, v6.d}[0], [x0], #32
        ins             v0.h[0], v6.h[7]
        st4             {v3.d, v4.d, v5.d, v6.d}[1], [x6], #32
        ins             v0.h[1], v6.h[3]
        b.gt            2b
        subs            w4,  w4,  #2
        b.le            9f
        sub             x8,  x6,  w9, uxtw #1
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        mov             w3,  w9
        b               1b
9:
        ret

L(ipred_filter\bpc\()_tbl):
        .hword L(ipred_filter\bpc\()_tbl) - 320b
        .hword L(ipred_filter\bpc\()_tbl) - 160b
        .hword L(ipred_filter\bpc\()_tbl) -  80b
        .hword L(ipred_filter\bpc\()_tbl) -  40b
endfunc
.endm

filter_fn 10
filter_fn 12

function ipred_filter_16bpc_neon, export=1
        ldr             w8,  [sp]
        cmp             w8,  0x3ff
        b.le            ipred_filter_10bpc_neon
        b               ipred_filter_12bpc_neon
endfunc

// void pal_pred_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                          const uint16_t *const pal, const uint8_t *idx,
//                          const int w, const int h);
function pal_pred_16bpc_neon, export=1
        ld1             {v30.8h}, [x2]
        clz             w9,  w4
        adr             x6,  L(pal_pred_tbl)
        sub             w9,  w9,  #25
        ldrh            w9,  [x6, w9, uxtw #1]
        movi            v31.8h,  #1, lsl #8
        sub             x6,  x6,  w9, uxtw
        br              x6
40:
        AARCH64_VALID_JUMP_TARGET
        add             x2,  x0,  x1
        lsl             x1,  x1,  #1
4:
        ld1             {v1.16b}, [x3], #16
        subs            w5,  w5,  #4
        // Restructure v1 from a, b, c, ... into 2*a, 2*a+1, 2*b, 2*b+1, 2*c, 2*c+1, ...
        add             v1.16b,  v1.16b,  v1.16b
        zip1            v0.16b,  v1.16b,  v1.16b
        zip2            v1.16b,  v1.16b,  v1.16b
        add             v0.8h,   v0.8h,   v31.8h
        add             v1.8h,   v1.8h,   v31.8h
        tbl             v0.16b, {v30.16b}, v0.16b
        st1             {v0.d}[0], [x0], x1
        tbl             v1.16b, {v30.16b}, v1.16b
        st1             {v0.d}[1], [x2], x1
        st1             {v1.d}[0], [x0], x1
        st1             {v1.d}[1], [x2], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        add             x2,  x0,  x1
        lsl             x1,  x1,  #1
8:
        ld1             {v2.16b, v3.16b}, [x3], #32
        subs            w5,  w5,  #4
        add             v2.16b,  v2.16b,  v2.16b
        add             v3.16b,  v3.16b,  v3.16b
        zip1            v0.16b,  v2.16b,  v2.16b
        zip2            v1.16b,  v2.16b,  v2.16b
        zip1            v2.16b,  v3.16b,  v3.16b
        zip2            v3.16b,  v3.16b,  v3.16b
        add             v0.8h,   v0.8h,   v31.8h
        add             v1.8h,   v1.8h,   v31.8h
        add             v2.8h,   v2.8h,   v31.8h
        add             v3.8h,   v3.8h,   v31.8h
        tbl             v0.16b, {v30.16b}, v0.16b
        tbl             v1.16b, {v30.16b}, v1.16b
        st1             {v0.8h}, [x0], x1
        tbl             v2.16b, {v30.16b}, v2.16b
        st1             {v1.8h}, [x2], x1
        tbl             v3.16b, {v30.16b}, v3.16b
        st1             {v2.8h}, [x0], x1
        st1             {v3.8h}, [x2], x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        add             x2,  x0,  x1
        lsl             x1,  x1,  #1
16:
        ld1             {v4.16b, v5.16b, v6.16b, v7.16b}, [x3], #64
        subs            w5,  w5,  #4
        add             v4.16b,  v4.16b,  v4.16b
        add             v5.16b,  v5.16b,  v5.16b
        add             v6.16b,  v6.16b,  v6.16b
        add             v7.16b,  v7.16b,  v7.16b
        zip1            v0.16b,  v4.16b,  v4.16b
        zip2            v1.16b,  v4.16b,  v4.16b
        zip1            v2.16b,  v5.16b,  v5.16b
        zip2            v3.16b,  v5.16b,  v5.16b
        zip1            v4.16b,  v6.16b,  v6.16b
        zip2            v5.16b,  v6.16b,  v6.16b
        zip1            v6.16b,  v7.16b,  v7.16b
        zip2            v7.16b,  v7.16b,  v7.16b
        add             v0.8h,   v0.8h,   v31.8h
        add             v1.8h,   v1.8h,   v31.8h
        add             v2.8h,   v2.8h,   v31.8h
        add             v3.8h,   v3.8h,   v31.8h
        add             v4.8h,   v4.8h,   v31.8h
        tbl             v0.16b, {v30.16b}, v0.16b
        add             v5.8h,   v5.8h,   v31.8h
        tbl             v1.16b, {v30.16b}, v1.16b
        add             v6.8h,   v6.8h,   v31.8h
        tbl             v2.16b, {v30.16b}, v2.16b
        add             v7.8h,   v7.8h,   v31.8h
        tbl             v3.16b, {v30.16b}, v3.16b
        tbl             v4.16b, {v30.16b}, v4.16b
        tbl             v5.16b, {v30.16b}, v5.16b
        st1             {v0.8h, v1.8h}, [x0], x1
        tbl             v6.16b, {v30.16b}, v6.16b
        st1             {v2.8h, v3.8h}, [x2], x1
        tbl             v7.16b, {v30.16b}, v7.16b
        st1             {v4.8h, v5.8h}, [x0], x1
        st1             {v6.8h, v7.8h}, [x2], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        add             x2,  x0,  x1
        lsl             x1,  x1,  #1
32:
        ld1             {v4.16b, v5.16b, v6.16b, v7.16b}, [x3], #64
        subs            w5,  w5,  #2
        add             v4.16b,  v4.16b,  v4.16b
        add             v5.16b,  v5.16b,  v5.16b
        add             v6.16b,  v6.16b,  v6.16b
        add             v7.16b,  v7.16b,  v7.16b
        zip1            v0.16b,  v4.16b,  v4.16b
        zip2            v1.16b,  v4.16b,  v4.16b
        zip1            v2.16b,  v5.16b,  v5.16b
        zip2            v3.16b,  v5.16b,  v5.16b
        zip1            v4.16b,  v6.16b,  v6.16b
        zip2            v5.16b,  v6.16b,  v6.16b
        zip1            v6.16b,  v7.16b,  v7.16b
        zip2            v7.16b,  v7.16b,  v7.16b
        add             v0.8h,   v0.8h,   v31.8h
        add             v1.8h,   v1.8h,   v31.8h
        add             v2.8h,   v2.8h,   v31.8h
        add             v3.8h,   v3.8h,   v31.8h
        add             v4.8h,   v4.8h,   v31.8h
        tbl             v0.16b, {v30.16b}, v0.16b
        add             v5.8h,   v5.8h,   v31.8h
        tbl             v1.16b, {v30.16b}, v1.16b
        add             v6.8h,   v6.8h,   v31.8h
        tbl             v2.16b, {v30.16b}, v2.16b
        add             v7.8h,   v7.8h,   v31.8h
        tbl             v3.16b, {v30.16b}, v3.16b
        tbl             v4.16b, {v30.16b}, v4.16b
        tbl             v5.16b, {v30.16b}, v5.16b
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        tbl             v6.16b, {v30.16b}, v6.16b
        tbl             v7.16b, {v30.16b}, v7.16b
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x2], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        add             x2,  x0,  #64
64:
        ld1             {v4.16b, v5.16b, v6.16b, v7.16b}, [x3], #64
        subs            w5,  w5,  #1
        add             v4.16b,  v4.16b,  v4.16b
        add             v5.16b,  v5.16b,  v5.16b
        add             v6.16b,  v6.16b,  v6.16b
        add             v7.16b,  v7.16b,  v7.16b
        zip1            v0.16b,  v4.16b,  v4.16b
        zip2            v1.16b,  v4.16b,  v4.16b
        zip1            v2.16b,  v5.16b,  v5.16b
        zip2            v3.16b,  v5.16b,  v5.16b
        zip1            v4.16b,  v6.16b,  v6.16b
        zip2            v5.16b,  v6.16b,  v6.16b
        zip1            v6.16b,  v7.16b,  v7.16b
        zip2            v7.16b,  v7.16b,  v7.16b
        add             v0.8h,   v0.8h,   v31.8h
        add             v1.8h,   v1.8h,   v31.8h
        add             v2.8h,   v2.8h,   v31.8h
        add             v3.8h,   v3.8h,   v31.8h
        add             v4.8h,   v4.8h,   v31.8h
        tbl             v0.16b, {v30.16b}, v0.16b
        add             v5.8h,   v5.8h,   v31.8h
        tbl             v1.16b, {v30.16b}, v1.16b
        add             v6.8h,   v6.8h,   v31.8h
        tbl             v2.16b, {v30.16b}, v2.16b
        add             v7.8h,   v7.8h,   v31.8h
        tbl             v3.16b, {v30.16b}, v3.16b
        tbl             v4.16b, {v30.16b}, v4.16b
        tbl             v5.16b, {v30.16b}, v5.16b
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], x1
        tbl             v6.16b, {v30.16b}, v6.16b
        tbl             v7.16b, {v30.16b}, v7.16b
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x2], x1
        b.gt            64b
        ret

L(pal_pred_tbl):
        .hword L(pal_pred_tbl) - 640b
        .hword L(pal_pred_tbl) - 320b
        .hword L(pal_pred_tbl) - 160b
        .hword L(pal_pred_tbl) -  80b
        .hword L(pal_pred_tbl) -  40b
endfunc

// void ipred_cfl_128_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                               const pixel *const topleft,
//                               const int width, const int height,
//                               const int16_t *ac, const int alpha,
//                               const int bitdepth_max);
function ipred_cfl_128_16bpc_neon, export=1
        dup             v31.8h,  w7   // bitdepth_max
        clz             w9,  w3
        adr             x7,  L(ipred_cfl_128_tbl)
        sub             w9,  w9,  #26
        ldrh            w9,  [x7, w9, uxtw #1]
        urshr           v0.8h,   v31.8h,  #1
        dup             v1.8h,   w6   // alpha
        sub             x7,  x7,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        movi            v30.8h,  #0
        br              x7
L(ipred_cfl_splat_w4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v4.8h, v5.8h}, [x5], #32
        subs            w4,  w4,  #4
        smull           v2.4s,   v4.4h,   v1.4h  // diff = ac * alpha
        smull2          v3.4s,   v4.8h,   v1.8h
        smull           v4.4s,   v5.4h,   v1.4h
        smull2          v5.4s,   v5.8h,   v1.8h
        cmlt            v16.4s,  v2.4s,   #0     // sign
        cmlt            v17.4s,  v3.4s,   #0
        cmlt            v18.4s,  v4.4s,   #0
        cmlt            v19.4s,  v5.4s,   #0
        add             v2.4s,   v2.4s,   v16.4s // diff + sign
        add             v3.4s,   v3.4s,   v17.4s
        add             v4.4s,   v4.4s,   v18.4s
        add             v5.4s,   v5.4s,   v19.4s
        rshrn           v2.4h,   v2.4s,   #6     // (diff + sign + 32) >> 6 = apply_sign()
        rshrn2          v2.8h,   v3.4s,   #6
        rshrn           v3.4h,   v4.4s,   #6
        rshrn2          v3.8h,   v5.4s,   #6
        add             v2.8h,   v2.8h,   v0.8h  // dc + apply_sign()
        add             v3.8h,   v3.8h,   v0.8h
        smax            v2.8h,   v2.8h,   v30.8h
        smax            v3.8h,   v3.8h,   v30.8h
        smin            v2.8h,   v2.8h,   v31.8h
        smin            v3.8h,   v3.8h,   v31.8h
        st1             {v2.d}[0],  [x0], x1
        st1             {v2.d}[1],  [x6], x1
        st1             {v3.d}[0],  [x0], x1
        st1             {v3.d}[1],  [x6], x1
        b.gt            L(ipred_cfl_splat_w4)
        ret
L(ipred_cfl_splat_w8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v4.8h, v5.8h}, [x5], #32
        subs            w4,  w4,  #2
        smull           v2.4s,   v4.4h,   v1.4h  // diff = ac * alpha
        smull2          v3.4s,   v4.8h,   v1.8h
        smull           v4.4s,   v5.4h,   v1.4h
        smull2          v5.4s,   v5.8h,   v1.8h
        cmlt            v16.4s,  v2.4s,   #0     // sign
        cmlt            v17.4s,  v3.4s,   #0
        cmlt            v18.4s,  v4.4s,   #0
        cmlt            v19.4s,  v5.4s,   #0
        add             v2.4s,   v2.4s,   v16.4s // diff + sign
        add             v3.4s,   v3.4s,   v17.4s
        add             v4.4s,   v4.4s,   v18.4s
        add             v5.4s,   v5.4s,   v19.4s
        rshrn           v2.4h,   v2.4s,   #6     // (diff + sign + 32) >> 6 = apply_sign()
        rshrn2          v2.8h,   v3.4s,   #6
        rshrn           v3.4h,   v4.4s,   #6
        rshrn2          v3.8h,   v5.4s,   #6
        add             v2.8h,   v2.8h,   v0.8h  // dc + apply_sign()
        add             v3.8h,   v3.8h,   v0.8h
        smax            v2.8h,   v2.8h,   v30.8h
        smax            v3.8h,   v3.8h,   v30.8h
        smin            v2.8h,   v2.8h,   v31.8h
        smin            v3.8h,   v3.8h,   v31.8h
        st1             {v2.8h},  [x0], x1
        st1             {v3.8h},  [x6], x1
        b.gt            L(ipred_cfl_splat_w8)
        ret
L(ipred_cfl_splat_w16):
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x5,  w3, uxtw #1
        sub             x1,  x1,  w3, uxtw #1
        mov             w9,  w3
1:
        ld1             {v2.8h, v3.8h}, [x5], #32
        ld1             {v4.8h, v5.8h}, [x7], #32
        subs            w3,  w3,  #16
        smull           v16.4s,  v2.4h,   v1.4h  // diff = ac * alpha
        smull2          v17.4s,  v2.8h,   v1.8h
        smull           v18.4s,  v3.4h,   v1.4h
        smull2          v19.4s,  v3.8h,   v1.8h
        smull           v2.4s,   v4.4h,   v1.4h
        smull2          v3.4s,   v4.8h,   v1.8h
        smull           v4.4s,   v5.4h,   v1.4h
        smull2          v5.4s,   v5.8h,   v1.8h
        cmlt            v20.4s,  v16.4s,  #0     // sign
        cmlt            v21.4s,  v17.4s,  #0
        cmlt            v22.4s,  v18.4s,  #0
        cmlt            v23.4s,  v19.4s,  #0
        cmlt            v24.4s,  v2.4s,   #0
        cmlt            v25.4s,  v3.4s,   #0
        cmlt            v26.4s,  v4.4s,   #0
        cmlt            v27.4s,  v5.4s,   #0
        add             v16.4s,  v16.4s,  v20.4s // diff + sign
        add             v17.4s,  v17.4s,  v21.4s
        add             v18.4s,  v18.4s,  v22.4s
        add             v19.4s,  v19.4s,  v23.4s
        add             v2.4s,   v2.4s,   v24.4s
        add             v3.4s,   v3.4s,   v25.4s
        add             v4.4s,   v4.4s,   v26.4s
        add             v5.4s,   v5.4s,   v27.4s
        rshrn           v16.4h,  v16.4s,  #6     // (diff + sign + 32) >> 6 = apply_sign()
        rshrn2          v16.8h,  v17.4s,  #6
        rshrn           v17.4h,  v18.4s,  #6
        rshrn2          v17.8h,  v19.4s,  #6
        rshrn           v6.4h,   v2.4s,   #6
        rshrn2          v6.8h,   v3.4s,   #6
        rshrn           v7.4h,   v4.4s,   #6
        rshrn2          v7.8h,   v5.4s,   #6
        add             v2.8h,   v16.8h,  v0.8h  // dc + apply_sign()
        add             v3.8h,   v17.8h,  v0.8h
        add             v4.8h,   v6.8h,   v0.8h
        add             v5.8h,   v7.8h,   v0.8h
        smax            v2.8h,   v2.8h,   v30.8h
        smax            v3.8h,   v3.8h,   v30.8h
        smax            v4.8h,   v4.8h,   v30.8h
        smax            v5.8h,   v5.8h,   v30.8h
        smin            v2.8h,   v2.8h,   v31.8h
        smin            v3.8h,   v3.8h,   v31.8h
        smin            v4.8h,   v4.8h,   v31.8h
        smin            v5.8h,   v5.8h,   v31.8h
        st1             {v2.8h, v3.8h},  [x0], #32
        st1             {v4.8h, v5.8h},  [x6], #32
        b.gt            1b
        subs            w4,  w4,  #2
        add             x5,  x5,  w9, uxtw #1
        add             x7,  x7,  w9, uxtw #1
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        mov             w3,  w9
        b.gt            1b
        ret

L(ipred_cfl_128_tbl):
L(ipred_cfl_splat_tbl):
        .hword L(ipred_cfl_128_tbl) - L(ipred_cfl_splat_w16)
        .hword L(ipred_cfl_128_tbl) - L(ipred_cfl_splat_w16)
        .hword L(ipred_cfl_128_tbl) - L(ipred_cfl_splat_w8)
        .hword L(ipred_cfl_128_tbl) - L(ipred_cfl_splat_w4)
endfunc

// void ipred_cfl_top_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                               const pixel *const topleft,
//                               const int width, const int height,
//                               const int16_t *ac, const int alpha,
//                               const int bitdepth_max);
function ipred_cfl_top_16bpc_neon, export=1
        dup             v31.8h,  w7   // bitdepth_max
        clz             w9,  w3
        adr             x7,  L(ipred_cfl_top_tbl)
        sub             w9,  w9,  #26
        ldrh            w9,  [x7, w9, uxtw #1]
        dup             v1.8h,   w6   // alpha
        add             x2,  x2,  #2
        sub             x7,  x7,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        movi            v30.8h,  #0
        br              x7
4:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2]
        addv            h0,      v0.4h
        urshr           v0.4h,   v0.4h,   #2
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w4)
8:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2]
        addv            h0,      v0.8h
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w8)
16:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h}, [x2]
        addp            v0.8h,   v2.8h,   v3.8h
        addv            h0,      v0.8h
        urshr           v0.4h,   v0.4h,   #4
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w16)
32:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h, v4.8h, v5.8h}, [x2]
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v0.8h,   v2.8h,   v4.8h
        uaddlv          s0,      v0.8h
        rshrn           v0.4h,   v0.4s,   #5
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w16)

L(ipred_cfl_top_tbl):
        .hword L(ipred_cfl_top_tbl) - 32b
        .hword L(ipred_cfl_top_tbl) - 16b
        .hword L(ipred_cfl_top_tbl) -  8b
        .hword L(ipred_cfl_top_tbl) -  4b
endfunc

// void ipred_cfl_left_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                                const pixel *const topleft,
//                                const int width, const int height,
//                                const int16_t *ac, const int alpha,
//                                const int bitdepth_max);
function ipred_cfl_left_16bpc_neon, export=1
        dup             v31.8h,  w7   // bitdepth_max
        sub             x2,  x2,  w4, uxtw #1
        clz             w9,  w3
        clz             w8,  w4
        adr             x10, L(ipred_cfl_splat_tbl)
        adr             x7,  L(ipred_cfl_left_tbl)
        sub             w9,  w9,  #26
        sub             w8,  w8,  #26
        ldrh            w9,  [x10, w9, uxtw #1]
        ldrh            w8,  [x7,  w8, uxtw #1]
        dup             v1.8h,   w6   // alpha
        sub             x9,  x10, w9, uxtw
        sub             x7,  x7,  w8, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        movi            v30.8h,  #0
        br              x7

L(ipred_cfl_left_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2]
        addv            h0,      v0.4h
        urshr           v0.4h,   v0.4h,   #2
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2]
        addv            h0,      v0.8h
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h}, [x2]
        addp            v0.8h,   v2.8h,   v3.8h
        addv            h0,      v0.8h
        urshr           v0.4h,   v0.4h,   #4
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h, v4.8h, v5.8h}, [x2]
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v0.8h,   v2.8h,   v4.8h
        uaddlv          s0,      v0.8h
        rshrn           v0.4h,   v0.4s,   #5
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_tbl):
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h32)
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h16)
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h8)
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h4)
endfunc

// void ipred_cfl_16bpc_neon(pixel *dst, const ptrdiff_t stride,
//                           const pixel *const topleft,
//                           const int width, const int height,
//                           const int16_t *ac, const int alpha,
//                           const int bitdepth_max);
function ipred_cfl_16bpc_neon, export=1
        dup             v31.8h,  w7              // bitdepth_max
        sub             x2,  x2,  w4, uxtw #1
        add             w8,  w3,  w4             // width + height
        dup             v1.8h,   w6              // alpha
        clz             w9,  w3
        clz             w6,  w4
        dup             v16.4s, w8               // width + height
        adr             x7,  L(ipred_cfl_tbl)
        rbit            w8,  w8                  // rbit(width + height)
        sub             w9,  w9,  #22            // 26 leading bits, minus table offset 4
        sub             w6,  w6,  #26
        clz             w8,  w8                  // ctz(width + height)
        ldrh            w9,  [x7, w9, uxtw #1]
        ldrh            w6,  [x7, w6, uxtw #1]
        neg             w8,  w8                  // -ctz(width + height)
        sub             x9,  x7,  w9, uxtw
        sub             x7,  x7,  w6, uxtw
        ushr            v16.4s,  v16.4s,  #1     // (width + height) >> 1
        dup             v17.4s,  w8              // -ctz(width + height)
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        movi            v30.8h,  #0
        br              x7

L(ipred_cfl_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.4h},  [x2], #8
        uaddlv          s0,      v0.4h
        add             x2,  x2,  #2
        br              x9
L(ipred_cfl_w4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.4h},  [x2]
        add             v0.2s,   v0.2s,   v16.2s
        uaddlv          s2,      v2.4h
        cmp             w4,  #4
        add             v0.2s,   v0.2s,   v2.2s
        ushl            v0.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 8/16
        cmp             w4,  #16
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v0.2s,   v0.2s,   v16.2s
        ushr            v0.2s,   v0.2s,   #17
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w4)

L(ipred_cfl_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8h},  [x2], #16
        uaddlv          s0,      v0.8h
        add             x2,  x2,  #2
        br              x9
L(ipred_cfl_w8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h},  [x2]
        add             v0.2s,   v0.2s,   v16.2s
        uaddlv          s2,      v2.8h
        cmp             w4,  #8
        add             v0.2s,   v0.2s,   v2.2s
        ushl            v0.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 4/16/32
        cmp             w4,  #32
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v0.2s,   v0.2s,   v16.2s
        ushr            v0.2s,   v0.2s,   #17
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w8)

L(ipred_cfl_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h}, [x2], #32
        addp            v0.8h,   v2.8h,   v3.8h
        add             x2,  x2,  #2
        uaddlv          s0,      v0.8h
        br              x9
L(ipred_cfl_w16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h}, [x2]
        add             v0.2s,   v0.2s,   v16.2s
        addp            v2.8h,   v2.8h,   v3.8h
        uaddlv          s2,      v2.8h
        cmp             w4,  #16
        add             v0.2s,   v0.2s,   v2.2s
        ushl            v0.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 4/8/32
        tst             w4,  #(32+16+8) // 16 added to make a consecutive bitmask
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v0.2s,   v0.2s,   v16.2s
        ushr            v0.2s,   v0.2s,   #17
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w16)

L(ipred_cfl_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h, v4.8h, v5.8h}, [x2], #64
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v0.8h,   v2.8h,   v4.8h
        add             x2,  x2,  #2
        uaddlv          s0,      v0.8h
        br              x9
L(ipred_cfl_w32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h, v4.8h, v5.8h}, [x2]
        add             v0.4s,   v0.4s,   v16.4s
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v2.8h,   v2.8h,   v4.8h
        cmp             w4,  #32
        uaddlv          s2,      v2.8h
        add             v0.2s,   v0.2s,   v2.2s
        ushl            v0.2s,   v0.2s,   v17.2s
        b.eq            1f
        // h = 8/16
        cmp             w4,  #8
        mov             w16, #0x6667
        mov             w17, #0xAAAB
        csel            w16, w16, w17, eq
        dup             v16.2s,  w16
        mul             v0.2s,   v0.2s,   v16.2s
        ushr            v0.2s,   v0.2s,   #17
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w16)

L(ipred_cfl_tbl):
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_h32)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_h16)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_h8)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_h4)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_w32)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_w16)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_w8)
        .hword L(ipred_cfl_tbl) - L(ipred_cfl_w4)
endfunc

// void cfl_ac_420_16bpc_neon(int16_t *const ac, const pixel *const ypx,
//                            const ptrdiff_t stride, const int w_pad,
//                            const int h_pad, const int cw, const int ch);
function ipred_cfl_ac_420_16bpc_neon, export=1
        clz             w8,  w5
        lsl             w4,  w4,  #2
        adr             x7,  L(ipred_cfl_ac_420_tbl)
        sub             w8,  w8,  #27
        ldrh            w8,  [x7, w8, uxtw #1]
        movi            v24.4s,  #0
        movi            v25.4s,  #0
        movi            v26.4s,  #0
        movi            v27.4s,  #0
        sub             x7,  x7,  w8, uxtw
        sub             w8,  w6,  w4         // height - h_pad
        rbit            w9,  w5              // rbit(width)
        rbit            w10, w6              // rbit(height)
        clz             w9,  w9              // ctz(width)
        clz             w10, w10             // ctz(height)
        add             w9,  w9,  w10        // log2sz
        add             x10, x1,  x2
        dup             v31.4s,  w9
        lsl             x2,  x2,  #1
        neg             v31.4s,  v31.4s      // -log2sz
        br              x7

L(ipred_cfl_ac_420_w4):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input
        ld1             {v0.8h}, [x1],  x2
        ld1             {v1.8h}, [x10], x2
        ld1             {v2.8h}, [x1],  x2
        ld1             {v3.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v2.8h
        addp            v1.8h,   v1.8h,   v3.8h
        add             v0.8h,   v0.8h,   v1.8h
        shl             v0.8h,   v0.8h,   #1
        subs            w8,  w8,  #2
        st1             {v0.8h}, [x0], #16
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        b.gt            1b
        trn2            v1.2d,   v0.2d,   v0.2d
        trn2            v0.2d,   v0.2d,   v0.2d
L(ipred_cfl_ac_420_w4_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        b.gt            2b
3:
L(ipred_cfl_ac_420_w4_calc_subtract_dc):
        // Aggregate the sums
        add             v24.4s,  v24.4s,  v25.4s
        add             v26.4s,  v26.4s,  v27.4s
        add             v0.4s,   v24.4s,  v26.4s
        addv            s0,  v0.4s                // sum
        sub             x0,  x0,  w6, uxtw #3
        urshl           v4.2s,   v0.2s,   v31.2s  // (sum + (1 << (log2sz - 1)))  >>= log2sz
        dup             v4.8h,   v4.h[0]
6:      // Subtract dc from ac
        ld1             {v0.8h, v1.8h}, [x0]
        subs            w6,  w6,  #4
        sub             v0.8h,   v0.8h,   v4.8h
        sub             v1.8h,   v1.8h,   v4.8h
        st1             {v0.8h, v1.8h}, [x0], #32
        b.gt            6b
        ret

L(ipred_cfl_ac_420_w8):
        AARCH64_VALID_JUMP_TARGET
        cbnz            w3,  L(ipred_cfl_ac_420_w8_wpad)
1:      // Copy and subsample input, without padding
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ld1             {v2.8h, v3.8h}, [x10], x2
        ld1             {v4.8h, v5.8h}, [x1],  x2
        addp            v0.8h,   v0.8h,   v1.8h
        ld1             {v6.8h, v7.8h}, [x10], x2
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        add             v0.8h,   v0.8h,   v2.8h
        add             v4.8h,   v4.8h,   v6.8h
        shl             v0.8h,   v0.8h,   #1
        shl             v1.8h,   v4.8h,   #1
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        b.gt            1b
        mov             v0.16b,  v1.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_420_w8_wpad):
1:      // Copy and subsample input, padding 4
        ld1             {v0.8h}, [x1],  x2
        ld1             {v1.8h}, [x10], x2
        ld1             {v2.8h}, [x1],  x2
        ld1             {v3.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v2.8h
        addp            v1.8h,   v1.8h,   v3.8h
        add             v0.8h,   v0.8h,   v1.8h
        shl             v0.8h,   v0.8h,   #1
        dup             v1.4h,   v0.h[3]
        dup             v3.4h,   v0.h[7]
        trn2            v2.2d,   v0.2d,   v0.2d
        subs            w8,  w8,  #2
        st1             {v0.4h, v1.4h, v2.4h, v3.4h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw           v25.4s,  v25.4s,  v1.4h
        uaddw           v26.4s,  v26.4s,  v2.4h
        uaddw           v27.4s,  v27.4s,  v3.4h
        b.gt            1b
        trn1            v0.2d,   v2.2d,   v3.2d
        trn1            v1.2d,   v2.2d,   v3.2d

L(ipred_cfl_ac_420_w8_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        st1             {v0.8h, v1.8h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        b.gt            2b
3:

        // Double the height and reuse the w4 summing/subtracting
        lsl             w6,  w6,  #1
        b               L(ipred_cfl_ac_420_w4_calc_subtract_dc)

L(ipred_cfl_ac_420_w16):
        AARCH64_VALID_JUMP_TARGET
        adr             x7,  L(ipred_cfl_ac_420_w16_tbl)
        ldrh            w3,  [x7, w3, uxtw #1]
        sub             x7,  x7,  w3, uxtw
        br              x7

L(ipred_cfl_ac_420_w16_wpad0):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, without padding
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x1],  x2
        ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        ld1             {v16.8h, v17.8h, v18.8h, v19.8h}, [x1],  x2
        add             v0.8h,   v0.8h,   v4.8h
        ld1             {v20.8h, v21.8h, v22.8h, v23.8h}, [x10], x2
        add             v2.8h,   v2.8h,   v6.8h
        addp            v16.8h,  v16.8h,  v17.8h
        addp            v18.8h,  v18.8h,  v19.8h
        addp            v20.8h,  v20.8h,  v21.8h
        addp            v22.8h,  v22.8h,  v23.8h
        add             v16.8h,  v16.8h,  v20.8h
        add             v18.8h,  v18.8h,  v22.8h
        shl             v0.8h,   v0.8h,   #1
        shl             v1.8h,   v2.8h,   #1
        shl             v2.8h,   v16.8h,  #1
        shl             v3.8h,   v18.8h,  #1
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_420_w16_wpad1):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 4
        ldr             q2,  [x1,  #32]
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ldr             q5,  [x10, #32]
        ld1             {v3.8h, v4.8h}, [x10], x2
        addp            v2.8h,   v2.8h,   v2.8h
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v5.8h,   v5.8h,   v5.8h
        addp            v3.8h,   v3.8h,   v4.8h
        ldr             q18, [x1,  #32]
        add             v2.4h,   v2.4h,   v5.4h
        ld1             {v16.8h, v17.8h}, [x1],  x2
        add             v0.8h,   v0.8h,   v3.8h
        ldr             q21, [x10, #32]
        ld1             {v19.8h, v20.8h}, [x10], x2
        addp            v18.8h,  v18.8h,  v18.8h
        addp            v16.8h,  v16.8h,  v17.8h
        addp            v21.8h,  v21.8h,  v21.8h
        addp            v19.8h,  v19.8h,  v20.8h
        add             v18.4h,  v18.4h,  v21.4h
        add             v16.8h,  v16.8h,  v19.8h
        shl             v1.4h,   v2.4h,   #1
        shl             v0.8h,   v0.8h,   #1
        shl             v3.4h,   v18.4h,  #1
        shl             v2.8h,   v16.8h,  #1
        dup             v4.4h,   v1.h[3]
        dup             v5.4h,   v3.h[3]
        trn1            v1.2d,   v1.2d,   v4.2d
        trn1            v3.2d,   v3.2d,   v5.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_420_w16_wpad2):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 8
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ld1             {v2.8h, v3.8h}, [x10], x2
        ld1             {v4.8h, v5.8h}, [x1],  x2
        addp            v0.8h,   v0.8h,   v1.8h
        ld1             {v6.8h, v7.8h}, [x10], x2
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        add             v0.8h,   v0.8h,   v2.8h
        add             v4.8h,   v4.8h,   v6.8h
        shl             v0.8h,   v0.8h,   #1
        shl             v2.8h,   v4.8h,   #1
        dup             v1.8h,   v0.h[7]
        dup             v3.8h,   v2.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_420_w16_wpad3):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 12
        ld1             {v0.8h}, [x1],  x2
        ld1             {v2.8h}, [x10], x2
        ld1             {v4.8h}, [x1],  x2
        ld1             {v6.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v4.8h
        addp            v2.8h,   v2.8h,   v6.8h
        add             v0.8h,   v0.8h,   v2.8h
        shl             v0.8h,   v0.8h,   #1
        dup             v1.8h,   v0.h[3]
        dup             v3.8h,   v0.h[7]
        trn2            v2.2d,   v0.2d,   v3.2d
        trn1            v0.2d,   v0.2d,   v1.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b

L(ipred_cfl_ac_420_w16_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            2b
3:

        // Quadruple the height and reuse the w4 summing/subtracting
        lsl             w6,  w6,  #2
        b               L(ipred_cfl_ac_420_w4_calc_subtract_dc)

L(ipred_cfl_ac_420_tbl):
        .hword L(ipred_cfl_ac_420_tbl) - L(ipred_cfl_ac_420_w16)
        .hword L(ipred_cfl_ac_420_tbl) - L(ipred_cfl_ac_420_w8)
        .hword L(ipred_cfl_ac_420_tbl) - L(ipred_cfl_ac_420_w4)
        .hword 0

L(ipred_cfl_ac_420_w16_tbl):
        .hword L(ipred_cfl_ac_420_w16_tbl) - L(ipred_cfl_ac_420_w16_wpad0)
        .hword L(ipred_cfl_ac_420_w16_tbl) - L(ipred_cfl_ac_420_w16_wpad1)
        .hword L(ipred_cfl_ac_420_w16_tbl) - L(ipred_cfl_ac_420_w16_wpad2)
        .hword L(ipred_cfl_ac_420_w16_tbl) - L(ipred_cfl_ac_420_w16_wpad3)
endfunc

// void cfl_ac_422_16bpc_neon(int16_t *const ac, const pixel *const ypx,
//                            const ptrdiff_t stride, const int w_pad,
//                            const int h_pad, const int cw, const int ch);
function ipred_cfl_ac_422_16bpc_neon, export=1
        clz             w8,  w5
        lsl             w4,  w4,  #2
        adr             x7,  L(ipred_cfl_ac_422_tbl)
        sub             w8,  w8,  #27
        ldrh            w8,  [x7, w8, uxtw #1]
        movi            v24.4s,  #0
        movi            v25.4s,  #0
        movi            v26.4s,  #0
        movi            v27.4s,  #0
        sub             x7,  x7,  w8, uxtw
        sub             w8,  w6,  w4         // height - h_pad
        rbit            w9,  w5              // rbit(width)
        rbit            w10, w6              // rbit(height)
        clz             w9,  w9              // ctz(width)
        clz             w10, w10             // ctz(height)
        add             w9,  w9,  w10        // log2sz
        add             x10, x1,  x2
        dup             v31.4s,  w9
        lsl             x2,  x2,  #1
        neg             v31.4s,  v31.4s      // -log2sz
        br              x7

L(ipred_cfl_ac_422_w4):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input
        ld1             {v0.8h}, [x1],  x2
        ld1             {v1.8h}, [x10], x2
        ld1             {v2.8h}, [x1],  x2
        ld1             {v3.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        shl             v0.8h,   v0.8h,   #2
        shl             v1.8h,   v2.8h,   #2
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        b.gt            1b
        trn2            v0.2d,   v1.2d,   v1.2d
        trn2            v1.2d,   v1.2d,   v1.2d
        b               L(ipred_cfl_ac_420_w4_hpad)

L(ipred_cfl_ac_422_w8):
        AARCH64_VALID_JUMP_TARGET
        cbnz            w3,  L(ipred_cfl_ac_422_w8_wpad)
1:      // Copy and subsample input, without padding
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ld1             {v2.8h, v3.8h}, [x10], x2
        ld1             {v4.8h, v5.8h}, [x1],  x2
        addp            v0.8h,   v0.8h,   v1.8h
        ld1             {v6.8h, v7.8h}, [x10], x2
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        shl             v0.8h,   v0.8h,   #2
        shl             v1.8h,   v2.8h,   #2
        shl             v2.8h,   v4.8h,   #2
        shl             v3.8h,   v6.8h,   #2
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v3.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_422_w8_wpad):
1:      // Copy and subsample input, padding 4
        ld1             {v0.8h}, [x1],  x2
        ld1             {v1.8h}, [x10], x2
        ld1             {v2.8h}, [x1],  x2
        ld1             {v3.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        shl             v0.8h,   v0.8h,   #2
        shl             v2.8h,   v2.8h,   #2
        dup             v4.4h,   v0.h[3]
        dup             v5.8h,   v0.h[7]
        dup             v6.4h,   v2.h[3]
        dup             v7.8h,   v2.h[7]
        trn2            v1.2d,   v0.2d,   v5.2d
        trn1            v0.2d,   v0.2d,   v4.2d
        trn2            v3.2d,   v2.2d,   v7.2d
        trn1            v2.2d,   v2.2d,   v6.2d
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v3.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_422_w16):
        AARCH64_VALID_JUMP_TARGET
        adr             x7,  L(ipred_cfl_ac_422_w16_tbl)
        ldrh            w3,  [x7, w3, uxtw #1]
        sub             x7,  x7,  w3, uxtw
        br              x7

L(ipred_cfl_ac_422_w16_wpad0):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, without padding
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x1],  x2
        ld1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        addp            v4.8h,   v4.8h,   v5.8h
        addp            v6.8h,   v6.8h,   v7.8h
        shl             v0.8h,   v0.8h,   #2
        shl             v1.8h,   v2.8h,   #2
        shl             v2.8h,   v4.8h,   #2
        shl             v3.8h,   v6.8h,   #2
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_w16_wpad1):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 4
        ldr             q2,  [x1,  #32]
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ldr             q6,  [x10, #32]
        ld1             {v4.8h, v5.8h}, [x10], x2
        addp            v2.8h,   v2.8h,   v2.8h
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v6.8h,   v6.8h,   v6.8h
        addp            v4.8h,   v4.8h,   v5.8h
        shl             v1.4h,   v2.4h,   #2
        shl             v0.8h,   v0.8h,   #2
        shl             v3.4h,   v6.4h,   #2
        shl             v2.8h,   v4.8h,   #2
        dup             v4.4h,   v1.h[3]
        dup             v5.4h,   v3.h[3]
        trn1            v1.2d,   v1.2d,   v4.2d
        trn1            v3.2d,   v3.2d,   v5.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_w16_wpad2):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 8
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ld1             {v2.8h, v3.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v2.8h,   v2.8h,   v3.8h
        shl             v0.8h,   v0.8h,   #2
        shl             v2.8h,   v2.8h,   #2
        dup             v1.8h,   v0.h[7]
        dup             v3.8h,   v2.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_w16_wpad3):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 12
        ld1             {v0.8h}, [x1],  x2
        ld1             {v2.8h}, [x10], x2
        addp            v0.8h,   v0.8h,   v0.8h
        addp            v2.8h,   v2.8h,   v2.8h
        shl             v0.4h,   v0.4h,   #2
        shl             v2.4h,   v2.4h,   #2
        dup             v1.8h,   v0.h[3]
        dup             v3.8h,   v2.h[3]
        trn1            v0.2d,   v0.2d,   v1.2d
        trn1            v2.2d,   v2.2d,   v3.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_tbl):
        .hword L(ipred_cfl_ac_422_tbl) - L(ipred_cfl_ac_422_w16)
        .hword L(ipred_cfl_ac_422_tbl) - L(ipred_cfl_ac_422_w8)
        .hword L(ipred_cfl_ac_422_tbl) - L(ipred_cfl_ac_422_w4)
        .hword 0

L(ipred_cfl_ac_422_w16_tbl):
        .hword L(ipred_cfl_ac_422_w16_tbl) - L(ipred_cfl_ac_422_w16_wpad0)
        .hword L(ipred_cfl_ac_422_w16_tbl) - L(ipred_cfl_ac_422_w16_wpad1)
        .hword L(ipred_cfl_ac_422_w16_tbl) - L(ipred_cfl_ac_422_w16_wpad2)
        .hword L(ipred_cfl_ac_422_w16_tbl) - L(ipred_cfl_ac_422_w16_wpad3)
endfunc

// void cfl_ac_444_16bpc_neon(int16_t *const ac, const pixel *const ypx,
//                            const ptrdiff_t stride, const int w_pad,
//                            const int h_pad, const int cw, const int ch);
function ipred_cfl_ac_444_16bpc_neon, export=1
        clz             w8,  w5
        lsl             w4,  w4,  #2
        adr             x7,  L(ipred_cfl_ac_444_tbl)
        sub             w8,  w8,  #26
        ldrh            w8,  [x7, w8, uxtw #1]
        movi            v24.4s,  #0
        movi            v25.4s,  #0
        movi            v26.4s,  #0
        movi            v27.4s,  #0
        sub             x7,  x7,  w8, uxtw
        sub             w8,  w6,  w4         // height - h_pad
        rbit            w9,  w5              // rbit(width)
        rbit            w10, w6              // rbit(height)
        clz             w9,  w9              // ctz(width)
        clz             w10, w10             // ctz(height)
        add             w9,  w9,  w10        // log2sz
        add             x10, x1,  x2
        dup             v31.4s,  w9
        lsl             x2,  x2,  #1
        neg             v31.4s,  v31.4s      // -log2sz
        br              x7

L(ipred_cfl_ac_444_w4):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input
        ld1             {v0.4h},   [x1],  x2
        ld1             {v0.d}[1], [x10], x2
        ld1             {v1.4h},   [x1],  x2
        ld1             {v1.d}[1], [x10], x2
        shl             v0.8h,   v0.8h,   #3
        shl             v1.8h,   v1.8h,   #3
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h}, [x0], #32
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        b.gt            1b
        trn2            v0.2d,   v1.2d,   v1.2d
        trn2            v1.2d,   v1.2d,   v1.2d
        b               L(ipred_cfl_ac_420_w4_hpad)

L(ipred_cfl_ac_444_w8):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input
        ld1             {v0.8h}, [x1],  x2
        ld1             {v1.8h}, [x10], x2
        ld1             {v2.8h}, [x1],  x2
        shl             v0.8h,   v0.8h,   #3
        ld1             {v3.8h}, [x10], x2
        shl             v1.8h,   v1.8h,   #3
        shl             v2.8h,   v2.8h,   #3
        shl             v3.8h,   v3.8h,   #3
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v3.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_444_w16):
        AARCH64_VALID_JUMP_TARGET
        cbnz            w3,  L(ipred_cfl_ac_444_w16_wpad)
1:      // Copy and expand input, without padding
        ld1             {v0.8h, v1.8h}, [x1],  x2
        ld1             {v2.8h, v3.8h}, [x10], x2
        shl             v0.8h,   v0.8h,   #3
        shl             v1.8h,   v1.8h,   #3
        shl             v2.8h,   v2.8h,   #3
        shl             v3.8h,   v3.8h,   #3
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_444_w16_wpad):
1:      // Copy and expand input, padding 8
        ld1             {v0.8h}, [x1],  x2
        ld1             {v2.8h}, [x10], x2
        shl             v0.8h,   v0.8h,   #3
        shl             v2.8h,   v2.8h,   #3
        dup             v1.8h,   v0.h[7]
        dup             v3.8h,   v2.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_444_w32):
        AARCH64_VALID_JUMP_TARGET
        adr             x7,  L(ipred_cfl_ac_444_w32_tbl)
        ldrh            w3,  [x7, w3, uxtw] // (w3>>1) << 1
        lsr             x2,  x2,  #1 // Restore the stride to one line increments
        sub             x7,  x7,  w3, uxtw
        br              x7

L(ipred_cfl_ac_444_w32_wpad0):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, without padding
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x1],  x2
        shl             v0.8h,   v0.8h,   #3
        shl             v1.8h,   v1.8h,   #3
        shl             v2.8h,   v2.8h,   #3
        shl             v3.8h,   v3.8h,   #3
        subs            w8,  w8,  #1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        b               L(ipred_cfl_ac_444_w32_hpad)

L(ipred_cfl_ac_444_w32_wpad2):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, padding 8
        ld1             {v0.8h, v1.8h, v2.8h}, [x1],  x2
        shl             v2.8h,   v2.8h,   #3
        shl             v0.8h,   v0.8h,   #3
        shl             v1.8h,   v1.8h,   #3
        dup             v3.8h,   v2.h[7]
        subs            w8,  w8,  #1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        b               L(ipred_cfl_ac_444_w32_hpad)

L(ipred_cfl_ac_444_w32_wpad4):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, padding 16
        ld1             {v0.8h, v1.8h}, [x1],  x2
        shl             v1.8h,   v1.8h,   #3
        shl             v0.8h,   v0.8h,   #3
        dup             v2.8h,   v1.h[7]
        dup             v3.8h,   v1.h[7]
        subs            w8,  w8,  #1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b
        b               L(ipred_cfl_ac_444_w32_hpad)

L(ipred_cfl_ac_444_w32_wpad6):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, padding 24
        ld1             {v0.8h}, [x1],  x2
        shl             v0.8h,   v0.8h,   #3
        dup             v1.8h,   v0.h[7]
        dup             v2.8h,   v0.h[7]
        dup             v3.8h,   v0.h[7]
        subs            w8,  w8,  #1
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            1b

L(ipred_cfl_ac_444_w32_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        uaddw           v24.4s,  v24.4s,  v0.4h
        uaddw2          v25.4s,  v25.4s,  v0.8h
        uaddw           v26.4s,  v26.4s,  v1.4h
        uaddw2          v27.4s,  v27.4s,  v1.8h
        uaddw           v24.4s,  v24.4s,  v2.4h
        uaddw2          v25.4s,  v25.4s,  v2.8h
        uaddw           v26.4s,  v26.4s,  v3.4h
        uaddw2          v27.4s,  v27.4s,  v3.8h
        b.gt            2b
3:

        //  Multiply the height by eight and reuse the w4 subtracting
        lsl             w6,  w6,  #3
        b               L(ipred_cfl_ac_420_w4_calc_subtract_dc)

L(ipred_cfl_ac_444_tbl):
        .hword L(ipred_cfl_ac_444_tbl) - L(ipred_cfl_ac_444_w32)
        .hword L(ipred_cfl_ac_444_tbl) - L(ipred_cfl_ac_444_w16)
        .hword L(ipred_cfl_ac_444_tbl) - L(ipred_cfl_ac_444_w8)
        .hword L(ipred_cfl_ac_444_tbl) - L(ipred_cfl_ac_444_w4)

L(ipred_cfl_ac_444_w32_tbl):
        .hword L(ipred_cfl_ac_444_w32_tbl) - L(ipred_cfl_ac_444_w32_wpad0)
        .hword L(ipred_cfl_ac_444_w32_tbl) - L(ipred_cfl_ac_444_w32_wpad2)
        .hword L(ipred_cfl_ac_444_w32_tbl) - L(ipred_cfl_ac_444_w32_wpad4)
        .hword L(ipred_cfl_ac_444_w32_tbl) - L(ipred_cfl_ac_444_w32_wpad6)
endfunc
