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

// void ipred_dc_128_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int a,
//                             const int max_width, const int max_height);
function ipred_dc_128_8bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_dc_128_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        movi            v0.16b,  #128
        sub             x5,  x5,  w3, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
4:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        b.gt            4b
        ret
8:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        b.gt            8b
        ret
16:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        movi            v1.16b,  #128
32:
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        movi            v1.16b,  #128
        movi            v2.16b,  #128
        movi            v3.16b,  #128
64:
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        b.gt            64b
        ret

L(ipred_dc_128_tbl):
        .hword L(ipred_dc_128_tbl) - 640b
        .hword L(ipred_dc_128_tbl) - 320b
        .hword L(ipred_dc_128_tbl) -  16b
        .hword L(ipred_dc_128_tbl) -   8b
        .hword L(ipred_dc_128_tbl) -   4b
endfunc

// void ipred_v_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                        const pixel *const topleft,
//                        const int width, const int height, const int a,
//                        const int max_width, const int max_height);
function ipred_v_8bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_v_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        add             x2,  x2,  #1
        sub             x5,  x5,  w3, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.s}[0],  [x2]
4:
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2]
8:
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2]
16:
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b}, [x2]
32:
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x2]
64:
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        b.gt            64b
        ret

L(ipred_v_tbl):
        .hword L(ipred_v_tbl) - 640b
        .hword L(ipred_v_tbl) - 320b
        .hword L(ipred_v_tbl) - 160b
        .hword L(ipred_v_tbl) -  80b
        .hword L(ipred_v_tbl) -  40b
endfunc

// void ipred_h_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                        const pixel *const topleft,
//                        const int width, const int height, const int a,
//                        const int max_width, const int max_height);
function ipred_h_8bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_h_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        sub             x2,  x2,  #4
        sub             x5,  x5,  w3, uxtw
        mov             x7,  #-4
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
4:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7
        st1             {v3.s}[0],  [x0], x1
        st1             {v2.s}[0],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v1.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        b.gt            4b
        ret
8:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7
        st1             {v3.8b},  [x0], x1
        st1             {v2.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v1.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        b.gt            8b
        ret
16:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.16b, v1.16b, v2.16b, v3.16b},  [x2], x7
        st1             {v3.16b}, [x0], x1
        st1             {v2.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v1.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            16b
        ret
32:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.16b, v1.16b, v2.16b, v3.16b},  [x2], x7
        str             q3,  [x0, #16]
        str             q2,  [x6, #16]
        st1             {v3.16b}, [x0], x1
        st1             {v2.16b}, [x6], x1
        subs            w4,  w4,  #4
        str             q1,  [x0, #16]
        str             q0,  [x6, #16]
        st1             {v1.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            32b
        ret
64:
        AARCH64_VALID_JUMP_TARGET
        ld4r            {v0.16b, v1.16b, v2.16b, v3.16b},  [x2], x7
        str             q3,  [x0, #16]
        str             q2,  [x6, #16]
        stp             q3,  q3,  [x0, #32]
        stp             q2,  q2,  [x6, #32]
        st1             {v3.16b}, [x0], x1
        st1             {v2.16b}, [x6], x1
        subs            w4,  w4,  #4
        str             q1,  [x0, #16]
        str             q0,  [x6, #16]
        stp             q1,  q1,  [x0, #32]
        stp             q0,  q0,  [x6, #32]
        st1             {v1.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            64b
        ret

L(ipred_h_tbl):
        .hword L(ipred_h_tbl) - 64b
        .hword L(ipred_h_tbl) - 32b
        .hword L(ipred_h_tbl) - 16b
        .hword L(ipred_h_tbl) -  8b
        .hword L(ipred_h_tbl) -  4b
endfunc

// void ipred_dc_top_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int a,
//                             const int max_width, const int max_height);
function ipred_dc_top_8bpc_neon, export=1
        clz             w3,  w3
        adr             x5,  L(ipred_dc_top_tbl)
        sub             w3,  w3,  #25
        ldrh            w3,  [x5, w3, uxtw #1]
        add             x2,  x2,  #1
        sub             x5,  x5,  w3, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v0.2s},  [x2]
        uaddlv          h0,      v0.8b
        rshrn           v0.8b,   v0.8h,   #3
        dup             v0.8b,   v0.b[0]
4:
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2]
        uaddlv          h0,      v0.8b
        rshrn           v0.8b,   v0.8h,   #3
        dup             v0.8b,   v0.b[0]
8:
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        b.gt            8b
        ret
160:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2]
        uaddlv          h0,      v0.16b
        rshrn           v0.8b,   v0.8h,   #4
        dup             v0.16b,  v0.b[0]
16:
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            16b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b}, [x2]
        uaddlv          h0,      v0.16b
        uaddlv          h1,      v1.16b
        add             v2.4h,   v0.4h,   v1.4h
        rshrn           v2.8b,   v2.8h,   #5
        dup             v0.16b,  v2.b[0]
        dup             v1.16b,  v2.b[0]
32:
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        b.gt            32b
        ret
640:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x2]
        uaddlv          h0,      v0.16b
        uaddlv          h1,      v1.16b
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        add             v4.4h,   v0.4h,   v1.4h
        add             v5.4h,   v2.4h,   v3.4h
        add             v4.4h,   v4.4h,   v5.4h
        rshrn           v4.8b,   v4.8h,   #6
        dup             v0.16b,  v4.b[0]
        dup             v1.16b,  v4.b[0]
        dup             v2.16b,  v4.b[0]
        dup             v3.16b,  v4.b[0]
64:
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        b.gt            64b
        ret

L(ipred_dc_top_tbl):
        .hword L(ipred_dc_top_tbl) - 640b
        .hword L(ipred_dc_top_tbl) - 320b
        .hword L(ipred_dc_top_tbl) - 160b
        .hword L(ipred_dc_top_tbl) -  80b
        .hword L(ipred_dc_top_tbl) -  40b
endfunc

// void ipred_dc_left_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height, const int a,
//                              const int max_width, const int max_height);
function ipred_dc_left_8bpc_neon, export=1
        sub             x2,  x2,  w4, uxtw
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
        ld1r            {v0.2s},  [x2]
        uaddlv          h0,      v0.8b
        rshrn           v0.8b,   v0.8h,   #3
        dup             v0.16b,  v0.b[0]
        br              x3
L(ipred_dc_left_w4):
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        b.gt            L(ipred_dc_left_w4)
        ret

L(ipred_dc_left_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2]
        uaddlv          h0,      v0.8b
        rshrn           v0.8b,   v0.8h,   #3
        dup             v0.16b,  v0.b[0]
        br              x3
L(ipred_dc_left_w8):
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        b.gt            L(ipred_dc_left_w8)
        ret

L(ipred_dc_left_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2]
        uaddlv          h0,      v0.16b
        rshrn           v0.8b,   v0.8h,   #4
        dup             v0.16b,  v0.b[0]
        br              x3
L(ipred_dc_left_w16):
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            L(ipred_dc_left_w16)
        ret

L(ipred_dc_left_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b}, [x2]
        uaddlv          h0,      v0.16b
        uaddlv          h1,      v1.16b
        add             v0.4h,   v0.4h,   v1.4h
        rshrn           v0.8b,   v0.8h,   #5
        dup             v0.16b,  v0.b[0]
        br              x3
L(ipred_dc_left_w32):
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
1:
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        b.gt            1b
        ret

L(ipred_dc_left_h64):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x2]
        uaddlv          h0,      v0.16b
        uaddlv          h1,      v1.16b
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        add             v0.4h,   v0.4h,   v1.4h
        add             v2.4h,   v2.4h,   v3.4h
        add             v0.4h,   v0.4h,   v2.4h
        rshrn           v0.8b,   v0.8h,   #6
        dup             v0.16b,  v0.b[0]
        br              x3
L(ipred_dc_left_w64):
        AARCH64_VALID_JUMP_TARGET
        mov             v1.16b,  v0.16b
        mov             v2.16b,  v0.16b
        mov             v3.16b,  v0.16b
1:
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
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

// void ipred_dc_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                         const pixel *const topleft,
//                         const int width, const int height, const int a,
//                         const int max_width, const int max_height);
function ipred_dc_8bpc_neon, export=1
        sub             x2,  x2,  w4, uxtw
        add             w7,  w3,  w4             // width + height
        clz             w3,  w3
        clz             w6,  w4
        dup             v16.8h, w7               // width + height
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
        ushr            v16.8h,  v16.8h,  #1     // (width + height) >> 1
        dup             v17.8h,  w7              // -ctz(width + height)
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5

L(ipred_dc_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.s}[0],  [x2], #4
        ins             v0.s[1], wzr
        uaddlv          h0,      v0.8b
        add             x2,  x2,  #1
        br              x3
L(ipred_dc_w4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.s}[0],  [x2]
        ins             v1.s[1], wzr
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h1,      v1.8b
        cmp             w4,  #4
        add             v0.4h,   v0.4h,   v1.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 8/16
        mov             w16, #(0x3334/2)
        movk            w16, #(0x5556/2), lsl #16
        add             w17, w4,  w4  // w17 = 2*h = 16 or 32
        lsr             w16, w16, w17
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
1:
        dup             v0.8b,   v0.b[0]
2:
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.s}[0],  [x0], x1
        st1             {v0.s}[0],  [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2], #8
        uaddlv          h0,      v0.8b
        add             x2,  x2,  #1
        br              x3
L(ipred_dc_w8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.8b},  [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h1,      v1.8b
        cmp             w4,  #8
        add             v0.4h,   v0.4h,   v1.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 4/16/32
        cmp             w4,  #32
        mov             w16, #(0x3334/2)
        mov             w17, #(0x5556/2)
        csel            w16, w16, w17, eq
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
1:
        dup             v0.8b,   v0.b[0]
2:
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.8b},  [x0], x1
        st1             {v0.8b},  [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2], #16
        uaddlv          h0,      v0.16b
        add             x2,  x2,  #1
        br              x3
L(ipred_dc_w16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.16b}, [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h1,      v1.16b
        cmp             w4,  #16
        add             v0.4h,   v0.4h,   v1.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 4/8/32/64
        tst             w4,  #(32+16+8) // 16 added to make a consecutive bitmask
        mov             w16, #(0x3334/2)
        mov             w17, #(0x5556/2)
        csel            w16, w16, w17, eq
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
1:
        dup             v0.16b,  v0.b[0]
2:
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b}, [x0], x1
        st1             {v0.16b}, [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b}, [x2], #32
        uaddlv          h0,      v0.16b
        uaddlv          h1,      v1.16b
        add             x2,  x2,  #1
        add             v0.4h,   v0.4h,   v1.4h
        br              x3
L(ipred_dc_w32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.16b, v2.16b}, [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h1,      v1.16b
        uaddlv          h2,      v2.16b
        cmp             w4,  #32
        add             v0.4h,   v0.4h,   v1.4h
        add             v0.4h,   v0.4h,   v2.4h
        ushl            v4.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 8/16/64
        cmp             w4,  #8
        mov             w16, #(0x3334/2)
        mov             w17, #(0x5556/2)
        csel            w16, w16, w17, eq
        dup             v16.4h,  w16
        sqdmulh         v4.4h,   v4.4h,   v16.4h
1:
        dup             v0.16b,  v4.b[0]
        dup             v1.16b,  v4.b[0]
2:
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b}, [x0], x1
        st1             {v0.16b, v1.16b}, [x6], x1
        b.gt            2b
        ret

L(ipred_dc_h64):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x2], #64
        uaddlv          h0,      v0.16b
        uaddlv          h1,      v1.16b
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        add             v0.4h,   v0.4h,   v1.4h
        add             v2.4h,   v2.4h,   v3.4h
        add             x2,  x2,  #1
        add             v0.4h,   v0.4h,   v2.4h
        br              x3
L(ipred_dc_w64):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.16b, v2.16b, v3.16b, v4.16b}, [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h1,      v1.16b
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        uaddlv          h4,      v4.16b
        add             v1.4h,   v1.4h,   v2.4h
        add             v3.4h,   v3.4h,   v4.4h
        cmp             w4,  #64
        add             v0.4h,   v0.4h,   v1.4h
        add             v0.4h,   v0.4h,   v3.4h
        ushl            v4.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 16/32
        mov             w16, #(0x5556/2)
        movk            w16, #(0x3334/2), lsl #16
        lsr             w16, w16, w4
        dup             v16.4h,  w16
        sqdmulh         v4.4h,   v4.4h,   v16.4h
1:
        dup             v0.16b,  v4.b[0]
        dup             v1.16b,  v4.b[0]
        dup             v2.16b,  v4.b[0]
        dup             v3.16b,  v4.b[0]
2:
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x0], x1
        st1             {v0.16b, v1.16b, v2.16b, v3.16b}, [x6], x1
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

// void ipred_paeth_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                            const pixel *const topleft,
//                            const int width, const int height, const int a,
//                            const int max_width, const int max_height);
function ipred_paeth_8bpc_neon, export=1
        clz             w9,  w3
        adr             x5,  L(ipred_paeth_tbl)
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v4.16b},  [x2]
        add             x8,  x2,  #1
        sub             x2,  x2,  #4
        sub             x5,  x5,  w9, uxtw
        mov             x7,  #-4
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v5.4s},  [x8]
        usubl           v6.8h,   v5.8b,   v4.8b   // top - topleft
4:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7
        zip1            v0.2s,   v0.2s,   v1.2s
        zip1            v2.2s,   v2.2s,   v3.2s
        uaddw           v16.8h,  v6.8h,   v0.8b
        uaddw           v17.8h,  v6.8h,   v2.8b
        sqxtun          v16.8b,  v16.8h           // base
        sqxtun2         v16.16b, v17.8h
        zip1            v0.2d,   v0.2d,   v2.2d
        uabd            v20.16b, v5.16b,  v16.16b // tdiff
        uabd            v22.16b, v4.16b,  v16.16b // tldiff
        uabd            v16.16b, v0.16b,  v16.16b // ldiff
        umin            v18.16b, v20.16b, v22.16b // min(tdiff, tldiff)
        cmhs            v20.16b, v22.16b, v20.16b // tldiff >= tdiff
        cmhs            v16.16b, v18.16b, v16.16b // min(tdiff, tldiff) >= ldiff
        bsl             v20.16b, v5.16b,  v4.16b  // tdiff <= tldiff ? top : topleft
        bit             v20.16b, v0.16b,  v16.16b // ldiff <= min ? left : ...
        st1             {v20.s}[3], [x0], x1
        st1             {v20.s}[2], [x6], x1
        subs            w4,  w4,  #4
        st1             {v20.s}[1], [x0], x1
        st1             {v20.s}[0], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v5.2d},  [x8]
        usubl           v6.8h,   v5.8b,   v4.8b   // top - topleft
8:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7
        uaddw           v16.8h,  v6.8h,   v0.8b
        uaddw           v17.8h,  v6.8h,   v1.8b
        uaddw           v18.8h,  v6.8h,   v2.8b
        uaddw           v19.8h,  v6.8h,   v3.8b
        sqxtun          v16.8b,  v16.8h           // base
        sqxtun2         v16.16b, v17.8h
        sqxtun          v18.8b,  v18.8h
        sqxtun2         v18.16b, v19.8h
        zip1            v2.2d,   v2.2d,   v3.2d
        zip1            v0.2d,   v0.2d,   v1.2d
        uabd            v21.16b, v5.16b,  v18.16b // tdiff
        uabd            v20.16b, v5.16b,  v16.16b
        uabd            v23.16b, v4.16b,  v18.16b // tldiff
        uabd            v22.16b, v4.16b,  v16.16b
        uabd            v17.16b, v2.16b,  v18.16b // ldiff
        uabd            v16.16b, v0.16b,  v16.16b
        umin            v19.16b, v21.16b, v23.16b // min(tdiff, tldiff)
        umin            v18.16b, v20.16b, v22.16b
        cmhs            v21.16b, v23.16b, v21.16b // tldiff >= tdiff
        cmhs            v20.16b, v22.16b, v20.16b
        cmhs            v17.16b, v19.16b, v17.16b // min(tdiff, tldiff) >= ldiff
        cmhs            v16.16b, v18.16b, v16.16b
        bsl             v21.16b, v5.16b,  v4.16b  // tdiff <= tldiff ? top : topleft
        bsl             v20.16b, v5.16b,  v4.16b
        bit             v21.16b, v2.16b,  v17.16b // ldiff <= min ? left : ...
        bit             v20.16b, v0.16b,  v16.16b
        st1             {v21.d}[1], [x0], x1
        st1             {v21.d}[0], [x6], x1
        subs            w4,  w4,  #4
        st1             {v20.d}[1], [x0], x1
        st1             {v20.d}[0], [x6], x1
        b.gt            8b
        ret
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v5.16b},  [x8], #16
        mov             w9,  w3
        // Set up pointers for four rows in parallel; x0, x6, x5, x10
        add             x5,  x0,  x1
        add             x10, x6,  x1
        lsl             x1,  x1,  #1
        sub             x1,  x1,  w3, uxtw
1:
        ld4r            {v0.16b, v1.16b, v2.16b, v3.16b},  [x2], x7
2:
        usubl           v6.8h,   v5.8b,   v4.8b   // top - topleft
        usubl2          v7.8h,   v5.16b,  v4.16b
        uaddw           v24.8h,  v6.8h,   v0.8b
        uaddw           v25.8h,  v7.8h,   v0.8b
        uaddw           v26.8h,  v6.8h,   v1.8b
        uaddw           v27.8h,  v7.8h,   v1.8b
        uaddw           v28.8h,  v6.8h,   v2.8b
        uaddw           v29.8h,  v7.8h,   v2.8b
        uaddw           v30.8h,  v6.8h,   v3.8b
        uaddw           v31.8h,  v7.8h,   v3.8b
        sqxtun          v17.8b,  v26.8h           // base
        sqxtun2         v17.16b, v27.8h
        sqxtun          v16.8b,  v24.8h
        sqxtun2         v16.16b, v25.8h
        sqxtun          v19.8b,  v30.8h
        sqxtun2         v19.16b, v31.8h
        sqxtun          v18.8b,  v28.8h
        sqxtun2         v18.16b, v29.8h
        uabd            v23.16b, v5.16b,  v19.16b // tdiff
        uabd            v22.16b, v5.16b,  v18.16b
        uabd            v21.16b, v5.16b,  v17.16b
        uabd            v20.16b, v5.16b,  v16.16b
        uabd            v27.16b, v4.16b,  v19.16b // tldiff
        uabd            v26.16b, v4.16b,  v18.16b
        uabd            v25.16b, v4.16b,  v17.16b
        uabd            v24.16b, v4.16b,  v16.16b
        uabd            v19.16b, v3.16b,  v19.16b // ldiff
        uabd            v18.16b, v2.16b,  v18.16b
        uabd            v17.16b, v1.16b,  v17.16b
        uabd            v16.16b, v0.16b,  v16.16b
        umin            v31.16b, v23.16b, v27.16b // min(tdiff, tldiff)
        umin            v30.16b, v22.16b, v26.16b
        umin            v29.16b, v21.16b, v25.16b
        umin            v28.16b, v20.16b, v24.16b
        cmhs            v23.16b, v27.16b, v23.16b // tldiff >= tdiff
        cmhs            v22.16b, v26.16b, v22.16b
        cmhs            v21.16b, v25.16b, v21.16b
        cmhs            v20.16b, v24.16b, v20.16b
        cmhs            v19.16b, v31.16b, v19.16b // min(tdiff, tldiff) >= ldiff
        cmhs            v18.16b, v30.16b, v18.16b
        cmhs            v17.16b, v29.16b, v17.16b
        cmhs            v16.16b, v28.16b, v16.16b
        bsl             v23.16b, v5.16b,  v4.16b  // tdiff <= tldiff ? top : topleft
        bsl             v22.16b, v5.16b,  v4.16b
        bsl             v21.16b, v5.16b,  v4.16b
        bsl             v20.16b, v5.16b,  v4.16b
        bit             v23.16b, v3.16b,  v19.16b // ldiff <= min ? left : ...
        bit             v22.16b, v2.16b,  v18.16b
        bit             v21.16b, v1.16b,  v17.16b
        bit             v20.16b, v0.16b,  v16.16b
        subs            w3,  w3,  #16
        st1             {v23.16b}, [x0],  #16
        st1             {v22.16b}, [x6],  #16
        st1             {v21.16b}, [x5],  #16
        st1             {v20.16b}, [x10], #16
        b.le            8f
        ld1             {v5.16b},  [x8], #16
        b               2b
8:
        subs            w4,  w4,  #4
        b.le            9f
        // End of horizontal loop, move pointers to next four rows
        sub             x8,  x8,  w9, uxtw
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        // Load the top row as early as possible
        ld1             {v5.16b},  [x8], #16
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

// void ipred_smooth_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int a,
//                             const int max_width, const int max_height);
function ipred_smooth_8bpc_neon, export=1
        movrel          x10, X(sm_weights)
        add             x11, x10, w4, uxtw
        add             x10, x10, w3, uxtw
        clz             w9,  w3
        adr             x5,  L(ipred_smooth_tbl)
        sub             x12, x2,  w4, uxtw
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v4.16b},  [x12] // bottom
        add             x8,  x2,  #1
        sub             x5,  x5,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v6.2s}, [x8]             // top
        ld1r            {v7.2s}, [x10]            // weights_hor
        sub             x2,  x2,  #4
        mov             x7,  #-4
        dup             v5.16b,  v6.b[3]          // right
        usubl           v6.8h,   v6.8b,   v4.8b   // top-bottom
        uxtl            v7.8h,   v7.8b            // weights_hor
4:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7 // left
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x11], #4 // weights_ver
        shll            v20.8h,  v5.8b,   #8      // right*256
        shll            v21.8h,  v5.8b,   #8
        zip1            v1.2s,   v1.2s,   v0.2s   // left, flipped
        zip1            v0.2s,   v3.2s,   v2.2s
        zip1            v16.2s,  v16.2s,  v17.2s  // weights_ver
        zip1            v18.2s,  v18.2s,  v19.2s
        shll            v22.8h,  v4.8b,   #8      // bottom*256
        shll            v23.8h,  v4.8b,   #8
        usubl           v0.8h,   v0.8b,   v5.8b   // left-right
        usubl           v1.8h,   v1.8b,   v5.8b
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v18.8h,  v18.8b
        mla             v20.8h,  v0.8h,   v7.8h   // right*256  + (left-right)*weights_hor
        mla             v21.8h,  v1.8h,   v7.8h
        mla             v22.8h,  v6.8h,   v16.8h  // bottom*256 + (top-bottom)*weights_ver
        mla             v23.8h,  v6.8h,   v18.8h
        uhadd           v20.8h,  v20.8h,  v22.8h
        uhadd           v21.8h,  v21.8h,  v23.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn           v21.8b,  v21.8h,  #8
        st1             {v20.s}[0], [x0], x1
        st1             {v20.s}[1], [x6], x1
        subs            w4,  w4,  #4
        st1             {v21.s}[0], [x0], x1
        st1             {v21.s}[1], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v6.8b}, [x8]             // top
        ld1             {v7.8b}, [x10]            // weights_hor
        sub             x2,  x2,  #4
        mov             x7,  #-4
        dup             v5.16b,  v6.b[7]          // right
        usubl           v6.8h,   v6.8b,   v4.8b   // top-bottom
        uxtl            v7.8h,   v7.8b            // weights_hor
8:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7 // left
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x11], #4 // weights_ver
        shll            v20.8h,  v5.8b,   #8      // right*256
        shll            v21.8h,  v5.8b,   #8
        shll            v22.8h,  v5.8b,   #8
        shll            v23.8h,  v5.8b,   #8
        usubl           v0.8h,   v0.8b,   v5.8b   // left-right
        usubl           v1.8h,   v1.8b,   v5.8b
        usubl           v2.8h,   v2.8b,   v5.8b
        usubl           v3.8h,   v3.8b,   v5.8b
        shll            v24.8h,  v4.8b,   #8      // bottom*256
        shll            v25.8h,  v4.8b,   #8
        shll            v26.8h,  v4.8b,   #8
        shll            v27.8h,  v4.8b,   #8
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v17.8h,  v17.8b
        uxtl            v18.8h,  v18.8b
        uxtl            v19.8h,  v19.8b
        mla             v20.8h,  v3.8h,   v7.8h   // right*256  + (left-right)*weights_hor
        mla             v21.8h,  v2.8h,   v7.8h   // (left flipped)
        mla             v22.8h,  v1.8h,   v7.8h
        mla             v23.8h,  v0.8h,   v7.8h
        mla             v24.8h,  v6.8h,   v16.8h  // bottom*256 + (top-bottom)*weights_ver
        mla             v25.8h,  v6.8h,   v17.8h
        mla             v26.8h,  v6.8h,   v18.8h
        mla             v27.8h,  v6.8h,   v19.8h
        uhadd           v20.8h,  v20.8h,  v24.8h
        uhadd           v21.8h,  v21.8h,  v25.8h
        uhadd           v22.8h,  v22.8h,  v26.8h
        uhadd           v23.8h,  v23.8h,  v27.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn           v21.8b,  v21.8h,  #8
        rshrn           v22.8b,  v22.8h,  #8
        rshrn           v23.8b,  v23.8h,  #8
        st1             {v20.8b}, [x0], x1
        st1             {v21.8b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v22.8b}, [x0], x1
        st1             {v23.8b}, [x6], x1
        b.gt            8b
        ret
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        add             x12, x2,  w3, uxtw
        sub             x2,  x2,  #2
        mov             x7,  #-2
        ld1r            {v5.16b}, [x12]           // right
        sub             x1,  x1,  w3, uxtw
        mov             w9,  w3

1:
        ld2r            {v0.8b, v1.8b},   [x2],  x7 // left
        ld2r            {v16.8b, v17.8b}, [x11], #2 // weights_ver
        usubl           v0.8h,   v0.8b,   v5.8b   // left-right
        usubl           v1.8h,   v1.8b,   v5.8b
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v17.8h,  v17.8b
2:
        ld1             {v7.16b}, [x10],  #16     // weights_hor
        ld1             {v3.16b}, [x8],   #16     // top
        shll            v20.8h,  v5.8b,   #8      // right*256
        shll            v21.8h,  v5.8b,   #8
        shll            v22.8h,  v5.8b,   #8
        shll            v23.8h,  v5.8b,   #8
        uxtl            v6.8h,   v7.8b            // weights_hor
        uxtl2           v7.8h,   v7.16b
        usubl           v2.8h,   v3.8b,   v4.8b   // top-bottom
        usubl2          v3.8h,   v3.16b,  v4.16b
        mla             v20.8h,  v1.8h,   v6.8h   // right*256  + (left-right)*weights_hor
        mla             v21.8h,  v1.8h,   v7.8h   // (left flipped)
        mla             v22.8h,  v0.8h,   v6.8h
        mla             v23.8h,  v0.8h,   v7.8h
        shll            v24.8h,  v4.8b,   #8      // bottom*256
        shll            v25.8h,  v4.8b,   #8
        shll            v26.8h,  v4.8b,   #8
        shll            v27.8h,  v4.8b,   #8
        mla             v24.8h,  v2.8h,   v16.8h  // bottom*256 + (top-bottom)*weights_ver
        mla             v25.8h,  v3.8h,   v16.8h
        mla             v26.8h,  v2.8h,   v17.8h
        mla             v27.8h,  v3.8h,   v17.8h
        uhadd           v20.8h,  v20.8h,  v24.8h
        uhadd           v21.8h,  v21.8h,  v25.8h
        uhadd           v22.8h,  v22.8h,  v26.8h
        uhadd           v23.8h,  v23.8h,  v27.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn2          v20.16b, v21.8h,  #8
        rshrn           v22.8b,  v22.8h,  #8
        rshrn2          v22.16b, v23.8h,  #8
        subs            w3,  w3,  #16
        st1             {v20.16b}, [x0],  #16
        st1             {v22.16b}, [x6],  #16
        b.gt            2b
        subs            w4,  w4,  #2
        b.le            9f
        sub             x8,  x8,  w9, uxtw
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

// void ipred_smooth_v_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                               const pixel *const topleft,
//                               const int width, const int height, const int a,
//                               const int max_width, const int max_height);
function ipred_smooth_v_8bpc_neon, export=1
        movrel          x7,  X(sm_weights)
        add             x7,  x7,  w4, uxtw
        clz             w9,  w3
        adr             x5,  L(ipred_smooth_v_tbl)
        sub             x8,  x2,  w4, uxtw
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v4.16b},  [x8] // bottom
        add             x2,  x2,  #1
        sub             x5,  x5,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v6.2s}, [x2]             // top
        usubl           v6.8h,   v6.8b,   v4.8b   // top-bottom
4:
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x7], #4 // weights_ver
        shll            v22.8h,  v4.8b,   #8      // bottom*256
        shll            v23.8h,  v4.8b,   #8
        zip1            v16.2s,  v16.2s,  v17.2s  // weights_ver
        zip1            v18.2s,  v18.2s,  v19.2s
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v18.8h,  v18.8b
        mla             v22.8h,  v6.8h,   v16.8h  // bottom*256 + (top-bottom)*weights_ver
        mla             v23.8h,  v6.8h,   v18.8h
        rshrn           v22.8b,  v22.8h,  #8
        rshrn           v23.8b,  v23.8h,  #8
        st1             {v22.s}[0], [x0], x1
        st1             {v22.s}[1], [x6], x1
        subs            w4,  w4,  #4
        st1             {v23.s}[0], [x0], x1
        st1             {v23.s}[1], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v6.8b}, [x2]             // top
        usubl           v6.8h,   v6.8b,   v4.8b   // top-bottom
8:
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b},  [x7], #4 // weights_ver
        shll            v24.8h,  v4.8b,   #8      // bottom*256
        shll            v25.8h,  v4.8b,   #8
        shll            v26.8h,  v4.8b,   #8
        shll            v27.8h,  v4.8b,   #8
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v17.8h,  v17.8b
        uxtl            v18.8h,  v18.8b
        uxtl            v19.8h,  v19.8b
        mla             v24.8h,  v6.8h,   v16.8h  // bottom*256 + (top-bottom)*weights_ver
        mla             v25.8h,  v6.8h,   v17.8h
        mla             v26.8h,  v6.8h,   v18.8h
        mla             v27.8h,  v6.8h,   v19.8h
        rshrn           v24.8b,  v24.8h,  #8
        rshrn           v25.8b,  v25.8h,  #8
        rshrn           v26.8b,  v26.8h,  #8
        rshrn           v27.8b,  v27.8h,  #8
        st1             {v24.8b}, [x0], x1
        st1             {v25.8b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v26.8b}, [x0], x1
        st1             {v27.8b}, [x6], x1
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
        sub             x1,  x1,  w3, uxtw
        mov             w9,  w3

1:
        ld4r            {v16.8b, v17.8b, v18.8b, v19.8b}, [x7], #4 // weights_ver
        uxtl            v16.8h,  v16.8b           // weights_ver
        uxtl            v17.8h,  v17.8b
        uxtl            v18.8h,  v18.8b
        uxtl            v19.8h,  v19.8b
2:
        ld1             {v3.16b}, [x2],   #16     // top
        shll            v20.8h,  v4.8b,   #8      // bottom*256
        shll            v21.8h,  v4.8b,   #8
        shll            v22.8h,  v4.8b,   #8
        shll            v23.8h,  v4.8b,   #8
        shll            v24.8h,  v4.8b,   #8
        shll            v25.8h,  v4.8b,   #8
        shll            v26.8h,  v4.8b,   #8
        shll            v27.8h,  v4.8b,   #8
        usubl           v2.8h,   v3.8b,   v4.8b   // top-bottom
        usubl2          v3.8h,   v3.16b,  v4.16b
        mla             v20.8h,  v2.8h,   v16.8h  // bottom*256 + (top-bottom)*weights_ver
        mla             v21.8h,  v3.8h,   v16.8h
        mla             v22.8h,  v2.8h,   v17.8h
        mla             v23.8h,  v3.8h,   v17.8h
        mla             v24.8h,  v2.8h,   v18.8h
        mla             v25.8h,  v3.8h,   v18.8h
        mla             v26.8h,  v2.8h,   v19.8h
        mla             v27.8h,  v3.8h,   v19.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn2          v20.16b, v21.8h,  #8
        rshrn           v22.8b,  v22.8h,  #8
        rshrn2          v22.16b, v23.8h,  #8
        rshrn           v24.8b,  v24.8h,  #8
        rshrn2          v24.16b, v25.8h,  #8
        rshrn           v26.8b,  v26.8h,  #8
        rshrn2          v26.16b, v27.8h,  #8
        subs            w3,  w3,  #16
        st1             {v20.16b}, [x0],  #16
        st1             {v22.16b}, [x6],  #16
        st1             {v24.16b}, [x5],  #16
        st1             {v26.16b}, [x8],  #16
        b.gt            2b
        subs            w4,  w4,  #4
        b.le            9f
        sub             x2,  x2,  w9, uxtw
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

// void ipred_smooth_h_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                               const pixel *const topleft,
//                               const int width, const int height, const int a,
//                               const int max_width, const int max_height);
function ipred_smooth_h_8bpc_neon, export=1
        movrel          x8,  X(sm_weights)
        add             x8,  x8,  w3, uxtw
        clz             w9,  w3
        adr             x5,  L(ipred_smooth_h_tbl)
        add             x12, x2,  w3, uxtw
        sub             w9,  w9,  #25
        ldrh            w9,  [x5, w9, uxtw #1]
        ld1r            {v5.16b},  [x12] // right
        sub             x5,  x5,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v7.2s}, [x8]             // weights_hor
        sub             x2,  x2,  #4
        mov             x7,  #-4
        uxtl            v7.8h,   v7.8b            // weights_hor
4:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7 // left
        shll            v20.8h,  v5.8b,   #8      // right*256
        shll            v21.8h,  v5.8b,   #8
        zip1            v1.2s,   v1.2s,   v0.2s   // left, flipped
        zip1            v0.2s,   v3.2s,   v2.2s
        usubl           v0.8h,   v0.8b,   v5.8b   // left-right
        usubl           v1.8h,   v1.8b,   v5.8b
        mla             v20.8h,  v0.8h,   v7.8h   // right*256  + (left-right)*weights_hor
        mla             v21.8h,  v1.8h,   v7.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn           v21.8b,  v21.8h,  #8
        st1             {v20.s}[0], [x0], x1
        st1             {v20.s}[1], [x6], x1
        subs            w4,  w4,  #4
        st1             {v21.s}[0], [x0], x1
        st1             {v21.s}[1], [x6], x1
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v7.8b}, [x8]             // weights_hor
        sub             x2,  x2,  #4
        mov             x7,  #-4
        uxtl            v7.8h,   v7.8b            // weights_hor
8:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},  [x2], x7 // left
        shll            v20.8h,  v5.8b,   #8      // right*256
        shll            v21.8h,  v5.8b,   #8
        shll            v22.8h,  v5.8b,   #8
        shll            v23.8h,  v5.8b,   #8
        usubl           v3.8h,   v3.8b,   v5.8b   // left-right
        usubl           v2.8h,   v2.8b,   v5.8b
        usubl           v1.8h,   v1.8b,   v5.8b
        usubl           v0.8h,   v0.8b,   v5.8b
        mla             v20.8h,  v3.8h,   v7.8h   // right*256  + (left-right)*weights_hor
        mla             v21.8h,  v2.8h,   v7.8h   // (left flipped)
        mla             v22.8h,  v1.8h,   v7.8h
        mla             v23.8h,  v0.8h,   v7.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn           v21.8b,  v21.8h,  #8
        rshrn           v22.8b,  v22.8h,  #8
        rshrn           v23.8b,  v23.8h,  #8
        st1             {v20.8b}, [x0], x1
        st1             {v21.8b}, [x6], x1
        subs            w4,  w4,  #4
        st1             {v22.8b}, [x0], x1
        st1             {v23.8b}, [x6], x1
        b.gt            8b
        ret
160:
320:
640:
        AARCH64_VALID_JUMP_TARGET
        sub             x2,  x2,  #4
        mov             x7,  #-4
        // Set up pointers for four rows in parallel; x0, x6, x5, x10
        add             x5,  x0,  x1
        add             x10, x6,  x1
        lsl             x1,  x1,  #1
        sub             x1,  x1,  w3, uxtw
        mov             w9,  w3

1:
        ld4r            {v0.8b, v1.8b, v2.8b, v3.8b},   [x2],  x7 // left
        usubl           v0.8h,   v0.8b,   v5.8b   // left-right
        usubl           v1.8h,   v1.8b,   v5.8b
        usubl           v2.8h,   v2.8b,   v5.8b
        usubl           v3.8h,   v3.8b,   v5.8b
2:
        ld1             {v7.16b}, [x8],   #16     // weights_hor
        shll            v20.8h,  v5.8b,   #8      // right*256
        shll            v21.8h,  v5.8b,   #8
        shll            v22.8h,  v5.8b,   #8
        shll            v23.8h,  v5.8b,   #8
        shll            v24.8h,  v5.8b,   #8
        shll            v25.8h,  v5.8b,   #8
        shll            v26.8h,  v5.8b,   #8
        shll            v27.8h,  v5.8b,   #8
        uxtl            v6.8h,   v7.8b            // weights_hor
        uxtl2           v7.8h,   v7.16b
        mla             v20.8h,  v3.8h,   v6.8h   // right*256  + (left-right)*weights_hor
        mla             v21.8h,  v3.8h,   v7.8h   // (left flipped)
        mla             v22.8h,  v2.8h,   v6.8h
        mla             v23.8h,  v2.8h,   v7.8h
        mla             v24.8h,  v1.8h,   v6.8h
        mla             v25.8h,  v1.8h,   v7.8h
        mla             v26.8h,  v0.8h,   v6.8h
        mla             v27.8h,  v0.8h,   v7.8h
        rshrn           v20.8b,  v20.8h,  #8
        rshrn2          v20.16b, v21.8h,  #8
        rshrn           v22.8b,  v22.8h,  #8
        rshrn2          v22.16b, v23.8h,  #8
        rshrn           v24.8b,  v24.8h,  #8
        rshrn2          v24.16b, v25.8h,  #8
        rshrn           v26.8b,  v26.8h,  #8
        rshrn2          v26.16b, v27.8h,  #8
        subs            w3,  w3,  #16
        st1             {v20.16b}, [x0],  #16
        st1             {v22.16b}, [x6],  #16
        st1             {v24.16b}, [x5],  #16
        st1             {v26.16b}, [x10], #16
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

// void ipred_filter_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                             const pixel *const topleft,
//                             const int width, const int height, const int filt_idx,
//                             const int max_width, const int max_height);
function ipred_filter_8bpc_neon, export=1
        and             w5,  w5,  #511
        movrel          x6,  X(filter_intra_taps)
        lsl             w5,  w5,  #6
        add             x6,  x6,  w5, uxtw
        ld1             {v16.8b, v17.8b, v18.8b, v19.8b}, [x6], #32
        clz             w9,  w3
        adr             x5,  L(ipred_filter_tbl)
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
        br              x5
40:
        AARCH64_VALID_JUMP_TARGET
        ldur            s0,  [x2, #1]             // top (0-3)
        sub             x2,  x2,  #2
        mov             x7,  #-2
        uxtl            v0.8h,   v0.8b            // top (0-3)
4:
        ld1             {v1.s}[0], [x2], x7       // left (0-1) + topleft (2)
        mul             v2.8h,   v17.8h,  v0.h[0] // p1(top[0]) * filter(1)
        mla             v2.8h,   v18.8h,  v0.h[1] // p2(top[1]) * filter(2)
        mla             v2.8h,   v19.8h,  v0.h[2] // p3(top[2]) * filter(3)
        uxtl            v1.8h,   v1.8b            // left (0-1) + topleft (2)
        mla             v2.8h,   v20.8h,  v0.h[3] // p4(top[3]) * filter(4)
        mla             v2.8h,   v16.8h,  v1.h[2] // p0(topleft) * filter(0)
        mla             v2.8h,   v21.8h,  v1.h[1] // p5(left[0]) * filter(5)
        mla             v2.8h,   v22.8h,  v1.h[0] // p6(left[1]) * filter(6)
        sqrshrun        v2.8b,   v2.8h,   #4
        subs            w4,  w4,  #2
        st1             {v2.s}[0], [x0], x1
        uxtl            v0.8h,   v2.8b
        st1             {v2.s}[1], [x6], x1
        ext             v0.16b,  v0.16b,  v0.16b, #8 // move top from [4-7] to [0-3]
        b.gt            4b
        ret
80:
        AARCH64_VALID_JUMP_TARGET
        ldur            d0,  [x2, #1]             // top (0-7)
        sub             x2,  x2,  #2
        mov             x7,  #-2
        uxtl            v0.8h,   v0.8b            // top (0-7)
8:
        ld1             {v1.s}[0], [x2], x7       // left (0-1) + topleft (2)
        mul             v2.8h,   v17.8h,  v0.h[0] // p1(top[0]) * filter(1)
        mla             v2.8h,   v18.8h,  v0.h[1] // p2(top[1]) * filter(2)
        mla             v2.8h,   v19.8h,  v0.h[2] // p3(top[2]) * filter(3)
        uxtl            v1.8h,   v1.8b            // left (0-1) + topleft (2)
        mla             v2.8h,   v20.8h,  v0.h[3] // p4(top[3]) * filter(4)
        mla             v2.8h,   v16.8h,  v1.h[2] // p0(topleft) * filter(0)
        mla             v2.8h,   v21.8h,  v1.h[1] // p5(left[0]) * filter(5)
        mla             v2.8h,   v22.8h,  v1.h[0] // p6(left[1]) * filter(6)
        mul             v3.8h,   v17.8h,  v0.h[4] // p1(top[0]) * filter(1)
        mla             v3.8h,   v18.8h,  v0.h[5] // p2(top[1]) * filter(2)
        mla             v3.8h,   v19.8h,  v0.h[6] // p3(top[2]) * filter(3)
        sqrshrun        v2.8b,   v2.8h,   #4
        uxtl            v1.8h,   v2.8b            // first block, in 16 bit
        mla             v3.8h,   v20.8h,  v0.h[7] // p4(top[3]) * filter(4)
        mla             v3.8h,   v16.8h,  v0.h[3] // p0(topleft) * filter(0)
        mla             v3.8h,   v21.8h,  v1.h[3] // p5(left[0]) * filter(5)
        mla             v3.8h,   v22.8h,  v1.h[7] // p6(left[1]) * filter(6)
        sqrshrun        v3.8b,   v3.8h,   #4
        subs            w4,  w4,  #2
        st2             {v2.s, v3.s}[0], [x0], x1
        zip2            v0.2s,   v2.2s,   v3.2s
        st2             {v2.s, v3.s}[1], [x6], x1
        uxtl            v0.8h,   v0.8b
        b.gt            8b
        ret
160:
320:
        AARCH64_VALID_JUMP_TARGET
        add             x8,  x2,  #1
        sub             x2,  x2,  #2
        mov             x7,  #-2
        sub             x1,  x1,  w3, uxtw
        mov             w9,  w3

1:
        ld1             {v0.s}[0], [x2], x7       // left (0-1) + topleft (2)
        uxtl            v0.8h,   v0.8b            // left (0-1) + topleft (2)
2:
        ld1             {v2.16b}, [x8],   #16     // top(0-15)
        mul             v3.8h,   v16.8h,  v0.h[2] // p0(topleft) * filter(0)
        mla             v3.8h,   v21.8h,  v0.h[1] // p5(left[0]) * filter(5)
        uxtl            v1.8h,   v2.8b            // top(0-7)
        uxtl2           v2.8h,   v2.16b           // top(8-15)
        mla             v3.8h,   v22.8h,  v0.h[0] // p6(left[1]) * filter(6)
        mla             v3.8h,   v17.8h,  v1.h[0] // p1(top[0]) * filter(1)
        mla             v3.8h,   v18.8h,  v1.h[1] // p2(top[1]) * filter(2)
        mla             v3.8h,   v19.8h,  v1.h[2] // p3(top[2]) * filter(3)
        mla             v3.8h,   v20.8h,  v1.h[3] // p4(top[3]) * filter(4)

        mul             v4.8h,   v17.8h,  v1.h[4] // p1(top[0]) * filter(1)
        mla             v4.8h,   v18.8h,  v1.h[5] // p2(top[1]) * filter(2)
        mla             v4.8h,   v19.8h,  v1.h[6] // p3(top[2]) * filter(3)
        sqrshrun        v3.8b,   v3.8h,   #4
        uxtl            v0.8h,   v3.8b            // first block, in 16 bit
        mla             v4.8h,   v20.8h,  v1.h[7] // p4(top[3]) * filter(4)
        mla             v4.8h,   v16.8h,  v1.h[3] // p0(topleft) * filter(0)
        mla             v4.8h,   v21.8h,  v0.h[3] // p5(left[0]) * filter(5)
        mla             v4.8h,   v22.8h,  v0.h[7] // p6(left[1]) * filter(6)

        mul             v5.8h,   v17.8h,  v2.h[0] // p1(top[0]) * filter(1)
        mla             v5.8h,   v18.8h,  v2.h[1] // p2(top[1]) * filter(2)
        mla             v5.8h,   v19.8h,  v2.h[2] // p3(top[2]) * filter(3)
        sqrshrun        v4.8b,   v4.8h,   #4
        uxtl            v0.8h,   v4.8b            // second block, in 16 bit
        mla             v5.8h,   v20.8h,  v2.h[3] // p4(top[3]) * filter(4)
        mla             v5.8h,   v16.8h,  v1.h[7] // p0(topleft) * filter(0)
        mla             v5.8h,   v21.8h,  v0.h[3] // p5(left[0]) * filter(5)
        mla             v5.8h,   v22.8h,  v0.h[7] // p6(left[1]) * filter(6)

        mul             v6.8h,   v17.8h,  v2.h[4] // p1(top[0]) * filter(1)
        mla             v6.8h,   v18.8h,  v2.h[5] // p2(top[1]) * filter(2)
        mla             v6.8h,   v19.8h,  v2.h[6] // p3(top[2]) * filter(3)
        sqrshrun        v5.8b,   v5.8h,   #4
        uxtl            v0.8h,   v5.8b            // third block, in 16 bit
        mla             v6.8h,   v20.8h,  v2.h[7] // p4(top[3]) * filter(4)
        mla             v6.8h,   v16.8h,  v2.h[3] // p0(topleft) * filter(0)
        mla             v6.8h,   v21.8h,  v0.h[3] // p5(left[0]) * filter(5)
        mla             v6.8h,   v22.8h,  v0.h[7] // p6(left[1]) * filter(6)

        subs            w3,  w3,  #16
        sqrshrun        v6.8b,   v6.8h,   #4

        st4             {v3.s, v4.s, v5.s, v6.s}[0], [x0], #16
        st4             {v3.s, v4.s, v5.s, v6.s}[1], [x6], #16
        b.le            8f
        ins             v0.h[2], v2.h[7]
        ins             v0.b[0], v6.b[7]
        ins             v0.b[2], v6.b[3]
        b               2b
8:
        subs            w4,  w4,  #2
        b.le            9f
        sub             x8,  x6,  w9, uxtw
        add             x0,  x0,  x1
        add             x6,  x6,  x1
        mov             w3,  w9
        b               1b
9:
        ret

L(ipred_filter_tbl):
        .hword L(ipred_filter_tbl) - 320b
        .hword L(ipred_filter_tbl) - 160b
        .hword L(ipred_filter_tbl) -  80b
        .hword L(ipred_filter_tbl) -  40b
endfunc

// void pal_pred_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                         const uint16_t *const pal, const uint8_t *idx,
//                         const int w, const int h);
function pal_pred_8bpc_neon, export=1
        ld1             {v0.8h}, [x2]
        clz             w9,  w4
        adr             x6,  L(pal_pred_tbl)
        sub             w9,  w9,  #25
        ldrh            w9,  [x6, w9, uxtw #1]
        xtn             v0.8b,  v0.8h
        sub             x6,  x6,  w9, uxtw
        add             x2,  x0,  x1
        lsl             x1,  x1,  #1
        br              x6
4:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.16b}, [x3], #16
        subs            w5,  w5,  #4
        tbl             v1.16b, {v0.16b}, v1.16b
        st1             {v1.s}[0], [x0], x1
        st1             {v1.s}[1], [x2], x1
        st1             {v1.s}[2], [x0], x1
        st1             {v1.s}[3], [x2], x1
        b.gt            4b
        ret
8:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.16b, v2.16b}, [x3], #32
        subs            w5,  w5,  #4
        tbl             v1.16b, {v0.16b}, v1.16b
        st1             {v1.d}[0], [x0], x1
        tbl             v2.16b, {v0.16b}, v2.16b
        st1             {v1.d}[1], [x2], x1
        st1             {v2.d}[0], [x0], x1
        st1             {v2.d}[1], [x2], x1
        b.gt            8b
        ret
16:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v1.16b, v2.16b, v3.16b, v4.16b}, [x3], #64
        subs            w5,  w5,  #4
        tbl             v1.16b, {v0.16b}, v1.16b
        tbl             v2.16b, {v0.16b}, v2.16b
        st1             {v1.16b}, [x0], x1
        tbl             v3.16b, {v0.16b}, v3.16b
        st1             {v2.16b}, [x2], x1
        tbl             v4.16b, {v0.16b}, v4.16b
        st1             {v3.16b}, [x0], x1
        st1             {v4.16b}, [x2], x1
        b.gt            16b
        ret
32:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v16.16b, v17.16b, v18.16b, v19.16b}, [x3], #64
        ld1             {v20.16b, v21.16b, v22.16b, v23.16b}, [x3], #64
        subs            w5,  w5,  #4
        tbl             v16.16b, {v0.16b}, v16.16b
        tbl             v17.16b, {v0.16b}, v17.16b
        tbl             v18.16b, {v0.16b}, v18.16b
        tbl             v19.16b, {v0.16b}, v19.16b
        tbl             v20.16b, {v0.16b}, v20.16b
        st1             {v16.16b, v17.16b}, [x0], x1
        tbl             v21.16b, {v0.16b}, v21.16b
        st1             {v18.16b, v19.16b}, [x2], x1
        tbl             v22.16b, {v0.16b}, v22.16b
        st1             {v20.16b, v21.16b}, [x0], x1
        tbl             v23.16b, {v0.16b}, v23.16b
        st1             {v22.16b, v23.16b}, [x2], x1
        b.gt            32b
        ret
64:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v16.16b, v17.16b, v18.16b, v19.16b}, [x3], #64
        ld1             {v20.16b, v21.16b, v22.16b, v23.16b}, [x3], #64
        subs            w5,  w5,  #2
        tbl             v16.16b, {v0.16b}, v16.16b
        tbl             v17.16b, {v0.16b}, v17.16b
        tbl             v18.16b, {v0.16b}, v18.16b
        tbl             v19.16b, {v0.16b}, v19.16b
        st1             {v16.16b, v17.16b, v18.16b, v19.16b}, [x0], x1
        tbl             v20.16b, {v0.16b}, v20.16b
        tbl             v21.16b, {v0.16b}, v21.16b
        tbl             v22.16b, {v0.16b}, v22.16b
        tbl             v23.16b, {v0.16b}, v23.16b
        st1             {v20.16b, v21.16b, v22.16b, v23.16b}, [x2], x1
        b.gt            64b
        ret

L(pal_pred_tbl):
        .hword L(pal_pred_tbl) - 64b
        .hword L(pal_pred_tbl) - 32b
        .hword L(pal_pred_tbl) - 16b
        .hword L(pal_pred_tbl) -  8b
        .hword L(pal_pred_tbl) -  4b
endfunc

// void ipred_cfl_128_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height,
//                              const int16_t *ac, const int alpha);
function ipred_cfl_128_8bpc_neon, export=1
        clz             w9,  w3
        adr             x7,  L(ipred_cfl_128_tbl)
        sub             w9,  w9,  #26
        ldrh            w9,  [x7, w9, uxtw #1]
        movi            v0.8h,   #128 // dc
        dup             v1.8h,   w6   // alpha
        sub             x7,  x7,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x7
L(ipred_cfl_splat_w4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h}, [x5], #32
        mul             v2.8h,   v2.8h,   v1.8h  // diff = ac * alpha
        mul             v3.8h,   v3.8h,   v1.8h
        cmlt            v4.8h,   v2.8h,   #0     // sign
        cmlt            v5.8h,   v3.8h,   #0
        add             v2.8h,   v2.8h,   v4.8h  // diff + sign
        add             v3.8h,   v3.8h,   v5.8h
        srshr           v2.8h,   v2.8h,   #6     // (diff + sign + 32) >> 6 = apply_sign()
        srshr           v3.8h,   v3.8h,   #6
        add             v2.8h,   v2.8h,   v0.8h  // dc + apply_sign()
        add             v3.8h,   v3.8h,   v0.8h
        sqxtun          v2.8b,   v2.8h           // iclip_pixel(dc + apply_sign())
        sqxtun          v3.8b,   v3.8h
        st1             {v2.s}[0],  [x0], x1
        st1             {v2.s}[1],  [x6], x1
        subs            w4,  w4,  #4
        st1             {v3.s}[0],  [x0], x1
        st1             {v3.s}[1],  [x6], x1
        b.gt            L(ipred_cfl_splat_w4)
        ret
L(ipred_cfl_splat_w8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8h, v3.8h, v4.8h, v5.8h}, [x5], #64
        mul             v2.8h,   v2.8h,   v1.8h  // diff = ac * alpha
        mul             v3.8h,   v3.8h,   v1.8h
        mul             v4.8h,   v4.8h,   v1.8h
        mul             v5.8h,   v5.8h,   v1.8h
        cmlt            v16.8h,  v2.8h,   #0     // sign
        cmlt            v17.8h,  v3.8h,   #0
        cmlt            v18.8h,  v4.8h,   #0
        cmlt            v19.8h,  v5.8h,   #0
        add             v2.8h,   v2.8h,   v16.8h // diff + sign
        add             v3.8h,   v3.8h,   v17.8h
        add             v4.8h,   v4.8h,   v18.8h
        add             v5.8h,   v5.8h,   v19.8h
        srshr           v2.8h,   v2.8h,   #6     // (diff + sign + 32) >> 6 = apply_sign()
        srshr           v3.8h,   v3.8h,   #6
        srshr           v4.8h,   v4.8h,   #6
        srshr           v5.8h,   v5.8h,   #6
        add             v2.8h,   v2.8h,   v0.8h  // dc + apply_sign()
        add             v3.8h,   v3.8h,   v0.8h
        add             v4.8h,   v4.8h,   v0.8h
        add             v5.8h,   v5.8h,   v0.8h
        sqxtun          v2.8b,   v2.8h           // iclip_pixel(dc + apply_sign())
        sqxtun          v3.8b,   v3.8h
        sqxtun          v4.8b,   v4.8h
        sqxtun          v5.8b,   v5.8h
        st1             {v2.8b},  [x0], x1
        st1             {v3.8b},  [x6], x1
        subs            w4,  w4,  #4
        st1             {v4.8b},  [x0], x1
        st1             {v5.8b},  [x6], x1
        b.gt            L(ipred_cfl_splat_w8)
        ret
L(ipred_cfl_splat_w16):
        AARCH64_VALID_JUMP_TARGET
        add             x7,  x5,  w3, uxtw #1
        sub             x1,  x1,  w3, uxtw
        mov             w9,  w3
1:
        ld1             {v2.8h, v3.8h}, [x5], #32
        ld1             {v4.8h, v5.8h}, [x7], #32
        mul             v2.8h,   v2.8h,   v1.8h  // diff = ac * alpha
        mul             v3.8h,   v3.8h,   v1.8h
        mul             v4.8h,   v4.8h,   v1.8h
        mul             v5.8h,   v5.8h,   v1.8h
        cmlt            v16.8h,  v2.8h,   #0     // sign
        cmlt            v17.8h,  v3.8h,   #0
        cmlt            v18.8h,  v4.8h,   #0
        cmlt            v19.8h,  v5.8h,   #0
        add             v2.8h,   v2.8h,   v16.8h // diff + sign
        add             v3.8h,   v3.8h,   v17.8h
        add             v4.8h,   v4.8h,   v18.8h
        add             v5.8h,   v5.8h,   v19.8h
        srshr           v2.8h,   v2.8h,   #6     // (diff + sign + 32) >> 6 = apply_sign()
        srshr           v3.8h,   v3.8h,   #6
        srshr           v4.8h,   v4.8h,   #6
        srshr           v5.8h,   v5.8h,   #6
        add             v2.8h,   v2.8h,   v0.8h  // dc + apply_sign()
        add             v3.8h,   v3.8h,   v0.8h
        add             v4.8h,   v4.8h,   v0.8h
        add             v5.8h,   v5.8h,   v0.8h
        sqxtun          v2.8b,   v2.8h           // iclip_pixel(dc + apply_sign())
        sqxtun          v3.8b,   v3.8h
        sqxtun          v4.8b,   v4.8h
        sqxtun          v5.8b,   v5.8h
        subs            w3,  w3,  #16
        st1             {v2.8b, v3.8b},  [x0], #16
        st1             {v4.8b, v5.8b},  [x6], #16
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

// void ipred_cfl_top_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                              const pixel *const topleft,
//                              const int width, const int height,
//                              const int16_t *ac, const int alpha);
function ipred_cfl_top_8bpc_neon, export=1
        clz             w9,  w3
        adr             x7,  L(ipred_cfl_top_tbl)
        sub             w9,  w9,  #26
        ldrh            w9,  [x7, w9, uxtw #1]
        dup             v1.8h,   w6   // alpha
        add             x2,  x2,  #1
        sub             x7,  x7,  w9, uxtw
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x7
4:
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v0.2s},  [x2]
        uaddlv          h0,      v0.8b
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w4)
8:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2]
        uaddlv          h0,      v0.8b
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w8)
16:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2]
        uaddlv          h0,      v0.16b
        urshr           v0.4h,   v0.4h,   #4
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w16)
32:
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.16b, v3.16b}, [x2]
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        add             v2.4h,   v2.4h,   v3.4h
        urshr           v2.4h,   v2.4h,   #5
        dup             v0.8h,   v2.h[0]
        b               L(ipred_cfl_splat_w16)

L(ipred_cfl_top_tbl):
        .hword L(ipred_cfl_top_tbl) - 32b
        .hword L(ipred_cfl_top_tbl) - 16b
        .hword L(ipred_cfl_top_tbl) -  8b
        .hword L(ipred_cfl_top_tbl) -  4b
endfunc

// void ipred_cfl_left_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                               const pixel *const topleft,
//                               const int width, const int height,
//                               const int16_t *ac, const int alpha);
function ipred_cfl_left_8bpc_neon, export=1
        sub             x2,  x2,  w4, uxtw
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
        br              x7

L(ipred_cfl_left_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v0.2s},  [x2]
        uaddlv          h0,      v0.8b
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2]
        uaddlv          h0,      v0.8b
        urshr           v0.4h,   v0.4h,   #3
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2]
        uaddlv          h0,      v0.16b
        urshr           v0.4h,   v0.4h,   #4
        dup             v0.8h,   v0.h[0]
        br              x9

L(ipred_cfl_left_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.16b, v3.16b}, [x2]
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        add             v2.4h,   v2.4h,   v3.4h
        urshr           v2.4h,   v2.4h,   #5
        dup             v0.8h,   v2.h[0]
        br              x9

L(ipred_cfl_left_tbl):
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h32)
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h16)
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h8)
        .hword L(ipred_cfl_left_tbl) - L(ipred_cfl_left_h4)
endfunc

// void ipred_cfl_8bpc_neon(pixel *dst, const ptrdiff_t stride,
//                          const pixel *const topleft,
//                          const int width, const int height,
//                          const int16_t *ac, const int alpha);
function ipred_cfl_8bpc_neon, export=1
        sub             x2,  x2,  w4, uxtw
        add             w8,  w3,  w4             // width + height
        dup             v1.8h,   w6              // alpha
        clz             w9,  w3
        clz             w6,  w4
        dup             v16.8h, w8               // width + height
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
        ushr            v16.8h,  v16.8h,  #1     // (width + height) >> 1
        dup             v17.8h,  w8              // -ctz(width + height)
        add             x6,  x0,  x1
        lsl             x1,  x1,  #1
        br              x7

L(ipred_cfl_h4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.s}[0],  [x2], #4
        ins             v0.s[1], wzr
        add             x2,  x2,  #1
        uaddlv          h0,      v0.8b
        br              x9
L(ipred_cfl_w4):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.s}[0],  [x2]
        ins             v2.s[1], wzr
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h2,      v2.8b
        cmp             w4,  #4
        add             v0.4h,   v0.4h,   v2.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 8/16
        mov             w16, #(0x3334/2)
        movk            w16, #(0x5556/2), lsl #16
        add             w17, w4,  w4  // w17 = 2*h = 16 or 32
        lsr             w16, w16, w17
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w4)

L(ipred_cfl_h8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.8b},  [x2], #8
        uaddlv          h0,      v0.8b
        add             x2,  x2,  #1
        br              x9
L(ipred_cfl_w8):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.8b},  [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h2,      v2.8b
        cmp             w4,  #8
        add             v0.4h,   v0.4h,   v2.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 4/16/32
        cmp             w4,  #32
        mov             w16, #(0x3334/2)
        mov             w17, #(0x5556/2)
        csel            w16, w16, w17, eq
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w8)

L(ipred_cfl_h16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v0.16b}, [x2], #16
        uaddlv          h0,      v0.16b
        add             x2,  x2,  #1
        br              x9
L(ipred_cfl_w16):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.16b}, [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h2,      v2.16b
        cmp             w4,  #16
        add             v0.4h,   v0.4h,   v2.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 4/8/32
        cmp             w4,  #4
        mov             w16, #(0x3334/2)
        mov             w17, #(0x5556/2)
        csel            w16, w16, w17, eq
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
1:
        dup             v0.8h,   v0.h[0]
        b               L(ipred_cfl_splat_w16)

L(ipred_cfl_h32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.16b, v3.16b}, [x2], #32
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        add             x2,  x2,  #1
        add             v0.4h,   v2.4h,   v3.4h
        br              x9
L(ipred_cfl_w32):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v2.16b, v3.16b}, [x2]
        add             v0.4h,   v0.4h,   v16.4h
        uaddlv          h2,      v2.16b
        uaddlv          h3,      v3.16b
        cmp             w4,  #32
        add             v0.4h,   v0.4h,   v2.4h
        add             v0.4h,   v0.4h,   v3.4h
        ushl            v0.4h,   v0.4h,   v17.4h
        b.eq            1f
        // h = 8/16
        mov             w16, #(0x5556/2)
        movk            w16, #(0x3334/2), lsl #16
        add             w17, w4,  w4  // w17 = 2*h = 16 or 32
        lsr             w16, w16, w17
        dup             v16.4h,  w16
        sqdmulh         v0.4h,   v0.4h,   v16.4h
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

// void cfl_ac_420_8bpc_neon(int16_t *const ac, const pixel *const ypx,
//                           const ptrdiff_t stride, const int w_pad,
//                           const int h_pad, const int cw, const int ch);
function ipred_cfl_ac_420_8bpc_neon, export=1
        clz             w8,  w5
        lsl             w4,  w4,  #2
        adr             x7,  L(ipred_cfl_ac_420_tbl)
        sub             w8,  w8,  #27
        ldrh            w8,  [x7, w8, uxtw #1]
        movi            v16.8h,  #0
        movi            v17.8h,  #0
        movi            v18.8h,  #0
        movi            v19.8h,  #0
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
        ld1             {v0.8b},   [x1],  x2
        ld1             {v1.8b},   [x10], x2
        ld1             {v0.d}[1], [x1],  x2
        ld1             {v1.d}[1], [x10], x2
        uaddlp          v0.8h,   v0.16b
        uaddlp          v1.8h,   v1.16b
        add             v0.8h,   v0.8h,   v1.8h
        shl             v0.8h,   v0.8h,   #1
        subs            w8,  w8,  #2
        st1             {v0.8h}, [x0], #16
        add             v16.8h,  v16.8h,  v0.8h
        b.gt            1b
        trn2            v1.2d,   v0.2d,   v0.2d
        trn2            v0.2d,   v0.2d,   v0.2d
L(ipred_cfl_ac_420_w4_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], #32
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        b.gt            2b
3:
        // Aggregate the sums
        add             v0.8h,   v16.8h,  v17.8h
        uaddlv          s0,  v0.8h                // sum
        sub             x0,  x0,  w6, uxtw #3
        urshl           v4.2s,   v0.2s,   v31.2s  // (sum + (1 << (log2sz - 1))) >>= log2sz
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
        ld1             {v0.16b}, [x1],  x2
        ld1             {v1.16b}, [x10], x2
        ld1             {v2.16b}, [x1],  x2
        uaddlp          v0.8h,   v0.16b
        ld1             {v3.16b}, [x10], x2
        uaddlp          v1.8h,   v1.16b
        uaddlp          v2.8h,   v2.16b
        uaddlp          v3.8h,   v3.16b
        add             v0.8h,   v0.8h,   v1.8h
        add             v2.8h,   v2.8h,   v3.8h
        shl             v0.8h,   v0.8h,   #1
        shl             v1.8h,   v2.8h,   #1
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h}, [x0], #32
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        b.gt            1b
        mov             v0.16b,  v1.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_420_w8_wpad):
1:      // Copy and subsample input, padding 4
        ld1             {v0.8b},   [x1],  x2
        ld1             {v1.8b},   [x10], x2
        ld1             {v0.d}[1], [x1],  x2
        ld1             {v1.d}[1], [x10], x2
        uaddlp          v0.8h,   v0.16b
        uaddlp          v1.8h,   v1.16b
        add             v0.8h,   v0.8h,   v1.8h
        shl             v0.8h,   v0.8h,   #1
        dup             v1.4h,   v0.h[3]
        dup             v3.4h,   v0.h[7]
        trn2            v2.2d,   v0.2d,   v0.2d
        subs            w8,  w8,  #2
        st1             {v0.4h, v1.4h, v2.4h, v3.4h}, [x0], #32
        add             v16.4h,  v16.4h,  v0.4h
        add             v17.4h,  v17.4h,  v1.4h
        add             v18.4h,  v18.4h,  v2.4h
        add             v19.4h,  v19.4h,  v3.4h
        b.gt            1b
        trn1            v0.2d,   v2.2d,   v3.2d
        trn1            v1.2d,   v2.2d,   v3.2d

L(ipred_cfl_ac_420_w8_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h}, [x0], #32
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        st1             {v0.8h, v1.8h}, [x0], #32
        add             v18.8h,  v18.8h,  v0.8h
        add             v19.8h,  v19.8h,  v1.8h
        b.gt            2b
3:

L(ipred_cfl_ac_420_w8_calc_subtract_dc):
        // Aggregate the sums
        add             v0.8h,   v16.8h,  v17.8h
        add             v2.8h,   v18.8h,  v19.8h
        uaddlp          v0.4s,   v0.8h
        uaddlp          v2.4s,   v2.8h
        add             v0.4s,   v0.4s,   v2.4s
        addv            s0,  v0.4s                // sum
        sub             x0,  x0,  w6, uxtw #4
        urshl           v4.2s,   v0.2s,   v31.2s  // (sum + (1 << (log2sz - 1))) >>= log2sz
        dup             v4.8h,   v4.h[0]
L(ipred_cfl_ac_420_w8_subtract_dc):
6:      // Subtract dc from ac
        ld1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0]
        subs            w6,  w6,  #4
        sub             v0.8h,   v0.8h,   v4.8h
        sub             v1.8h,   v1.8h,   v4.8h
        sub             v2.8h,   v2.8h,   v4.8h
        sub             v3.8h,   v3.8h,   v4.8h
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        b.gt            6b
        ret

L(ipred_cfl_ac_420_w16):
        AARCH64_VALID_JUMP_TARGET
        adr             x7,  L(ipred_cfl_ac_420_w16_tbl)
        ldrh            w3,  [x7, w3, uxtw #1]
        sub             x7,  x7,  w3, uxtw
        br              x7

L(ipred_cfl_ac_420_w16_wpad0):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, without padding
        ld1             {v0.16b, v1.16b}, [x1],  x2
        ld1             {v2.16b, v3.16b}, [x10], x2
        uaddlp          v0.8h,   v0.16b
        ld1             {v4.16b, v5.16b}, [x1],  x2
        uaddlp          v1.8h,   v1.16b
        ld1             {v6.16b, v7.16b}, [x10], x2
        uaddlp          v2.8h,   v2.16b
        uaddlp          v3.8h,   v3.16b
        uaddlp          v4.8h,   v4.16b
        uaddlp          v5.8h,   v5.16b
        uaddlp          v6.8h,   v6.16b
        uaddlp          v7.8h,   v7.16b
        add             v0.8h,   v0.8h,   v2.8h
        add             v1.8h,   v1.8h,   v3.8h
        add             v4.8h,   v4.8h,   v6.8h
        add             v5.8h,   v5.8h,   v7.8h
        shl             v0.8h,   v0.8h,   #1
        shl             v1.8h,   v1.8h,   #1
        shl             v2.8h,   v4.8h,   #1
        shl             v3.8h,   v5.8h,   #1
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_420_w16_wpad1):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 4
        ldr             d1,  [x1,  #16]
        ld1             {v0.16b}, [x1],  x2
        ldr             d3,  [x10, #16]
        ld1             {v2.16b}, [x10], x2
        uaddlp          v1.4h,   v1.8b
        ldr             d5,  [x1,  #16]
        uaddlp          v0.8h,   v0.16b
        ld1             {v4.16b}, [x1],  x2
        uaddlp          v3.4h,   v3.8b
        ldr             d7,  [x10, #16]
        uaddlp          v2.8h,   v2.16b
        ld1             {v6.16b}, [x10], x2
        uaddlp          v5.4h,   v5.8b
        uaddlp          v4.8h,   v4.16b
        uaddlp          v7.4h,   v7.8b
        uaddlp          v6.8h,   v6.16b
        add             v1.4h,   v1.4h,   v3.4h
        add             v0.8h,   v0.8h,   v2.8h
        add             v5.4h,   v5.4h,   v7.4h
        add             v4.8h,   v4.8h,   v6.8h
        shl             v1.4h,   v1.4h,   #1
        shl             v0.8h,   v0.8h,   #1
        shl             v3.4h,   v5.4h,   #1
        shl             v2.8h,   v4.8h,   #1
        dup             v4.4h,   v1.h[3]
        dup             v5.4h,   v3.h[3]
        trn1            v1.2d,   v1.2d,   v4.2d
        trn1            v3.2d,   v3.2d,   v5.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_420_w16_wpad2):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 8
        ld1             {v0.16b}, [x1],  x2
        ld1             {v2.16b}, [x10], x2
        ld1             {v4.16b}, [x1],  x2
        uaddlp          v0.8h,   v0.16b
        ld1             {v6.16b}, [x10], x2
        uaddlp          v2.8h,   v2.16b
        uaddlp          v4.8h,   v4.16b
        uaddlp          v6.8h,   v6.16b
        add             v0.8h,   v0.8h,   v2.8h
        add             v4.8h,   v4.8h,   v6.8h
        shl             v0.8h,   v0.8h,   #1
        shl             v2.8h,   v4.8h,   #1
        dup             v1.8h,   v0.h[7]
        dup             v3.8h,   v2.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_420_w16_wpad3):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 12
        ld1             {v0.8b}, [x1],  x2
        ld1             {v2.8b}, [x10], x2
        ld1             {v4.8b}, [x1],  x2
        uaddlp          v0.4h,   v0.8b
        ld1             {v6.8b}, [x10], x2
        uaddlp          v2.4h,   v2.8b
        uaddlp          v4.4h,   v4.8b
        uaddlp          v6.4h,   v6.8b
        add             v0.4h,   v0.4h,   v2.4h
        add             v4.4h,   v4.4h,   v6.4h
        shl             v0.4h,   v0.4h,   #1
        shl             v2.4h,   v4.4h,   #1
        dup             v1.8h,   v0.h[3]
        dup             v3.8h,   v2.h[3]
        trn1            v0.2d,   v0.2d,   v1.2d
        trn1            v2.2d,   v2.2d,   v3.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b

L(ipred_cfl_ac_420_w16_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            2b
3:

        // Double the height and reuse the w8 summing/subtracting
        lsl             w6,  w6,  #1
        b               L(ipred_cfl_ac_420_w8_calc_subtract_dc)

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

// void cfl_ac_422_8bpc_neon(int16_t *const ac, const pixel *const ypx,
//                           const ptrdiff_t stride, const int w_pad,
//                           const int h_pad, const int cw, const int ch);
function ipred_cfl_ac_422_8bpc_neon, export=1
        clz             w8,  w5
        lsl             w4,  w4,  #2
        adr             x7,  L(ipred_cfl_ac_422_tbl)
        sub             w8,  w8,  #27
        ldrh            w8,  [x7, w8, uxtw #1]
        movi            v16.8h,  #0
        movi            v17.8h,  #0
        movi            v18.8h,  #0
        movi            v19.8h,  #0
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
        ld1             {v0.8b},   [x1],  x2
        ld1             {v0.d}[1], [x10], x2
        ld1             {v1.8b},   [x1],  x2
        ld1             {v1.d}[1], [x10], x2
        uaddlp          v0.8h,   v0.16b
        uaddlp          v1.8h,   v1.16b
        shl             v0.8h,   v0.8h,   #2
        shl             v1.8h,   v1.8h,   #2
        subs            w8,  w8,  #4
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        st1             {v0.8h, v1.8h}, [x0], #32
        b.gt            1b
        trn2            v0.2d,   v1.2d,   v1.2d
        trn2            v1.2d,   v1.2d,   v1.2d
        b               L(ipred_cfl_ac_420_w4_hpad)

L(ipred_cfl_ac_422_w8):
        AARCH64_VALID_JUMP_TARGET
        cbnz            w3,  L(ipred_cfl_ac_422_w8_wpad)
1:      // Copy and subsample input, without padding
        ld1             {v0.16b}, [x1],  x2
        ld1             {v1.16b}, [x10], x2
        ld1             {v2.16b}, [x1],  x2
        uaddlp          v0.8h,   v0.16b
        ld1             {v3.16b}, [x10], x2
        uaddlp          v1.8h,   v1.16b
        uaddlp          v2.8h,   v2.16b
        uaddlp          v3.8h,   v3.16b
        shl             v0.8h,   v0.8h,   #2
        shl             v1.8h,   v1.8h,   #2
        shl             v2.8h,   v2.8h,   #2
        shl             v3.8h,   v3.8h,   #2
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v3.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_422_w8_wpad):
1:      // Copy and subsample input, padding 4
        ld1             {v0.8b},   [x1],  x2
        ld1             {v0.d}[1], [x10], x2
        ld1             {v2.8b},   [x1],  x2
        ld1             {v2.d}[1], [x10], x2
        uaddlp          v0.8h,   v0.16b
        uaddlp          v2.8h,   v2.16b
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
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
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
        ld1             {v0.16b, v1.16b}, [x1],  x2
        ld1             {v2.16b, v3.16b}, [x10], x2
        uaddlp          v0.8h,   v0.16b
        uaddlp          v1.8h,   v1.16b
        uaddlp          v2.8h,   v2.16b
        uaddlp          v3.8h,   v3.16b
        shl             v0.8h,   v0.8h,   #2
        shl             v1.8h,   v1.8h,   #2
        shl             v2.8h,   v2.8h,   #2
        shl             v3.8h,   v3.8h,   #2
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_w16_wpad1):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 4
        ldr             d1,  [x1,  #16]
        ld1             {v0.16b}, [x1],  x2
        ldr             d3,  [x10, #16]
        ld1             {v2.16b}, [x10], x2
        uaddlp          v1.4h,   v1.8b
        uaddlp          v0.8h,   v0.16b
        uaddlp          v3.4h,   v3.8b
        uaddlp          v2.8h,   v2.16b
        shl             v1.4h,   v1.4h,   #2
        shl             v0.8h,   v0.8h,   #2
        shl             v3.4h,   v3.4h,   #2
        shl             v2.8h,   v2.8h,   #2
        dup             v4.4h,   v1.h[3]
        dup             v5.4h,   v3.h[3]
        trn1            v1.2d,   v1.2d,   v4.2d
        trn1            v3.2d,   v3.2d,   v5.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_w16_wpad2):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 8
        ld1             {v0.16b}, [x1],  x2
        ld1             {v2.16b}, [x10], x2
        uaddlp          v0.8h,   v0.16b
        uaddlp          v2.8h,   v2.16b
        shl             v0.8h,   v0.8h,   #2
        shl             v2.8h,   v2.8h,   #2
        dup             v1.8h,   v0.h[7]
        dup             v3.8h,   v2.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v2.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_422_w16_wpad3):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and subsample input, padding 12
        ld1             {v0.8b}, [x1],  x2
        ld1             {v2.8b}, [x10], x2
        uaddlp          v0.4h,   v0.8b
        uaddlp          v2.4h,   v2.8b
        shl             v0.4h,   v0.4h,   #2
        shl             v2.4h,   v2.4h,   #2
        dup             v1.8h,   v0.h[3]
        dup             v3.8h,   v2.h[3]
        trn1            v0.2d,   v0.2d,   v1.2d
        trn1            v2.2d,   v2.2d,   v3.2d
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
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

// void cfl_ac_444_8bpc_neon(int16_t *const ac, const pixel *const ypx,
//                           const ptrdiff_t stride, const int w_pad,
//                           const int h_pad, const int cw, const int ch);
function ipred_cfl_ac_444_8bpc_neon, export=1
        clz             w8,  w5
        lsl             w4,  w4,  #2
        adr             x7,  L(ipred_cfl_ac_444_tbl)
        sub             w8,  w8,  #26
        ldrh            w8,  [x7, w8, uxtw #1]
        movi            v16.8h,  #0
        movi            v17.8h,  #0
        movi            v18.8h,  #0
        movi            v19.8h,  #0
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
        ld1             {v0.s}[0], [x1],  x2
        ld1             {v0.s}[1], [x10], x2
        ld1             {v1.s}[0], [x1],  x2
        ld1             {v1.s}[1], [x10], x2
        ushll           v0.8h,   v0.8b,   #3
        ushll           v1.8h,   v1.8b,   #3
        subs            w8,  w8,  #4
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        st1             {v0.8h, v1.8h}, [x0], #32
        b.gt            1b
        trn2            v0.2d,   v1.2d,   v1.2d
        trn2            v1.2d,   v1.2d,   v1.2d
        b               L(ipred_cfl_ac_420_w4_hpad)

L(ipred_cfl_ac_444_w8):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input
        ld1             {v0.8b}, [x1],  x2
        ld1             {v1.8b}, [x10], x2
        ld1             {v2.8b}, [x1],  x2
        ushll           v0.8h,   v0.8b,   #3
        ld1             {v3.8b}, [x10], x2
        ushll           v1.8h,   v1.8b,   #3
        ushll           v2.8h,   v2.8b,   #3
        ushll           v3.8h,   v3.8b,   #3
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        b.gt            1b
        mov             v0.16b,  v3.16b
        mov             v1.16b,  v3.16b
        b               L(ipred_cfl_ac_420_w8_hpad)

L(ipred_cfl_ac_444_w16):
        AARCH64_VALID_JUMP_TARGET
        cbnz            w3,  L(ipred_cfl_ac_444_w16_wpad)
1:      // Copy and expand input, without padding
        ld1             {v0.16b}, [x1],  x2
        ld1             {v2.16b}, [x10], x2
        ld1             {v4.16b}, [x1],  x2
        ushll2          v1.8h,   v0.16b,  #3
        ushll           v0.8h,   v0.8b,   #3
        ld1             {v6.16b}, [x10], x2
        ushll2          v3.8h,   v2.16b,  #3
        ushll           v2.8h,   v2.8b,   #3
        ushll2          v5.8h,   v4.16b,  #3
        ushll           v4.8h,   v4.8b,   #3
        ushll2          v7.8h,   v6.16b,  #3
        ushll           v6.8h,   v6.8b,   #3
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            1b
        mov             v0.16b,  v6.16b
        mov             v1.16b,  v7.16b
        mov             v2.16b,  v6.16b
        mov             v3.16b,  v7.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_444_w16_wpad):
1:      // Copy and expand input, padding 8
        ld1             {v0.8b}, [x1],  x2
        ld1             {v2.8b}, [x10], x2
        ld1             {v4.8b}, [x1],  x2
        ld1             {v6.8b}, [x10], x2
        ushll           v0.8h,   v0.8b,   #3
        ushll           v2.8h,   v2.8b,   #3
        ushll           v4.8h,   v4.8b,   #3
        ushll           v6.8h,   v6.8b,   #3
        dup             v1.8h,   v0.h[7]
        dup             v3.8h,   v2.h[7]
        dup             v5.8h,   v4.h[7]
        dup             v7.8h,   v6.h[7]
        subs            w8,  w8,  #4
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            1b
        mov             v0.16b,  v6.16b
        mov             v1.16b,  v7.16b
        mov             v2.16b,  v6.16b
        mov             v3.16b,  v7.16b
        b               L(ipred_cfl_ac_420_w16_hpad)

L(ipred_cfl_ac_444_w32):
        AARCH64_VALID_JUMP_TARGET
        adr             x7,  L(ipred_cfl_ac_444_w32_tbl)
        ldrh            w3,  [x7, w3, uxtw] // (w3>>1) << 1
        sub             x7,  x7,  w3, uxtw
        br              x7

L(ipred_cfl_ac_444_w32_wpad0):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, without padding
        ld1             {v2.16b, v3.16b}, [x1],  x2
        ld1             {v6.16b, v7.16b}, [x10], x2
        ushll           v0.8h,   v2.8b,   #3
        ushll2          v1.8h,   v2.16b,  #3
        ushll           v2.8h,   v3.8b,   #3
        ushll2          v3.8h,   v3.16b,  #3
        ushll           v4.8h,   v6.8b,   #3
        ushll2          v5.8h,   v6.16b,  #3
        ushll           v6.8h,   v7.8b,   #3
        ushll2          v7.8h,   v7.16b,  #3
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            1b
        b               L(ipred_cfl_ac_444_w32_hpad)

L(ipred_cfl_ac_444_w32_wpad2):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, padding 8
        ldr             d2,  [x1,  #16]
        ld1             {v1.16b}, [x1],  x2
        ldr             d6,  [x10, #16]
        ld1             {v5.16b}, [x10], x2
        ushll           v2.8h,   v2.8b,   #3
        ushll           v0.8h,   v1.8b,   #3
        ushll2          v1.8h,   v1.16b,  #3
        ushll           v6.8h,   v6.8b,   #3
        ushll           v4.8h,   v5.8b,   #3
        ushll2          v5.8h,   v5.16b,  #3
        dup             v3.8h,   v2.h[7]
        dup             v7.8h,   v6.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            1b
        b               L(ipred_cfl_ac_444_w32_hpad)

L(ipred_cfl_ac_444_w32_wpad4):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, padding 16
        ld1             {v1.16b}, [x1],  x2
        ld1             {v5.16b}, [x10], x2
        ushll           v0.8h,   v1.8b,   #3
        ushll2          v1.8h,   v1.16b,  #3
        ushll           v4.8h,   v5.8b,   #3
        ushll2          v5.8h,   v5.16b,  #3
        dup             v2.8h,   v1.h[7]
        dup             v3.8h,   v1.h[7]
        dup             v6.8h,   v5.h[7]
        dup             v7.8h,   v5.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            1b
        b               L(ipred_cfl_ac_444_w32_hpad)

L(ipred_cfl_ac_444_w32_wpad6):
        AARCH64_VALID_JUMP_TARGET
1:      // Copy and expand input, padding 24
        ld1             {v0.8b}, [x1],  x2
        ld1             {v4.8b}, [x10], x2
        ushll           v0.8h,   v0.8b,   #3
        ushll           v4.8h,   v4.8b,   #3
        dup             v1.8h,   v0.h[7]
        dup             v2.8h,   v0.h[7]
        dup             v3.8h,   v0.h[7]
        dup             v5.8h,   v4.h[7]
        dup             v6.8h,   v4.h[7]
        dup             v7.8h,   v4.h[7]
        subs            w8,  w8,  #2
        st1             {v0.8h, v1.8h, v2.8h, v3.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v0.8h
        add             v17.8h,  v17.8h,  v1.8h
        add             v18.8h,  v18.8h,  v2.8h
        add             v19.8h,  v19.8h,  v3.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            1b

L(ipred_cfl_ac_444_w32_hpad):
        cbz             w4,  3f
2:      // Vertical padding (h_pad > 0)
        subs            w4,  w4,  #2
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        st1             {v4.8h, v5.8h, v6.8h, v7.8h}, [x0], #64
        add             v16.8h,  v16.8h,  v4.8h
        add             v17.8h,  v17.8h,  v5.8h
        add             v18.8h,  v18.8h,  v6.8h
        add             v19.8h,  v19.8h,  v7.8h
        b.gt            2b
3:

        // Quadruple the height and reuse the w8 subtracting
        lsl             w6,  w6,  #2
        // Aggregate the sums, with wider intermediates earlier than in
        // ipred_cfl_ac_420_w8_calc_subtract_dc.
        uaddlp          v0.4s,   v16.8h
        uaddlp          v1.4s,   v17.8h
        uaddlp          v2.4s,   v18.8h
        uaddlp          v3.4s,   v19.8h
        add             v0.4s,   v0.4s,   v1.4s
        add             v2.4s,   v2.4s,   v3.4s
        add             v0.4s,   v0.4s,   v2.4s
        addv            s0,  v0.4s                // sum
        sub             x0,  x0,  w6, uxtw #4
        urshl           v4.2s,   v0.2s,   v31.2s  // (sum + (1 << (log2sz - 1))) >>= log2sz
        dup             v4.8h,   v4.h[0]
        b               L(ipred_cfl_ac_420_w8_subtract_dc)

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
