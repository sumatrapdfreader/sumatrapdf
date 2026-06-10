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
#include "src/arm/asm-offsets.h"

#define GRAIN_WIDTH 82
#define GRAIN_HEIGHT 73

#define SUB_GRAIN_WIDTH 44
#define SUB_GRAIN_HEIGHT 38

.macro increment_seed steps, shift=1
        lsr             w11, w2,  #3
        lsr             w12, w2,  #12
        lsr             w13, w2,  #1
        eor             w11, w2,  w11                     // (r >> 0) ^ (r >> 3)
        eor             w12, w12, w13                     // (r >> 12) ^ (r >> 1)
        eor             w11, w11, w12                     // (r >> 0) ^ (r >> 3) ^ (r >> 12) ^ (r >> 1)
.if \shift
        lsr             w2,  w2,  #\steps
.endif
        and             w11, w11, #((1 << \steps) - 1)    // bit
.if \shift
        orr             w2,  w2,  w11, lsl #(16 - \steps) // *state
.else
        orr             w2,  w2,  w11, lsl #16            // *state
.endif
.endm

.macro read_rand dest, bits, age
        ubfx            \dest,  x2,   #16 - \bits - \age, #\bits
.endm

.macro read_shift_rand dest, bits
        ubfx            \dest,  x2,   #17 - \bits, #\bits
        lsr             w2,  w2,  #1
.endm

// special calling convention:
// w2 holds seed
// x3 holds dav1d_gaussian_sequence
// clobbers x11-x15
// returns in v0.8h
function get_gaussian_neon
        increment_seed  4
        read_rand       x14, 11,  3
        read_rand       x15, 11,  2
        add             x14, x3,  x14, lsl #1
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[0], [x14]
        read_rand       x14, 11,  1
        ld1             {v0.h}[1], [x15]
        add             x14, x3,  x14, lsl #1
        read_rand       x15, 11,  0
        increment_seed  4
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[2], [x14]
        read_rand       x14, 11,  3
        ld1             {v0.h}[3], [x15]
        add             x14, x3,  x14, lsl #1
        read_rand       x15, 11,  2
        ld1             {v0.h}[4], [x14]
        add             x15, x3,  x15, lsl #1
        read_rand       x14, 11,  1
        ld1             {v0.h}[5], [x15]
        read_rand       x15, 11,  0
        add             x14, x3,  x14, lsl #1
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[6], [x14]
        ld1             {v0.h}[7], [x15]
        ret
endfunc

.macro get_grain_row r0, r1, r2, r3, r4, r5
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn             \r0\().8b,  \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn2            \r0\().16b, \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn             \r1\().8b,  \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn2            \r1\().16b, \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn             \r2\().8b,  \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn2            \r2\().16b, \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn             \r3\().8b,  \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn2            \r3\().16b, \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn             \r4\().8b,  \r5\().8h
        bl              get_gaussian_neon
        srshl           \r5\().8h,  v0.8h,  v31.8h
        xtn2            \r4\().16b, \r5\().8h
        increment_seed  2
        read_rand       x14, 11,  1
        read_rand       x15, 11,  0
        add             x14, x3,  x14, lsl #1
        add             x15, x3,  x15, lsl #1
        ld1             {\r5\().h}[0], [x14]
        ld1             {\r5\().h}[1], [x15]
        srshl           v0.4h,      \r5\().4h,  v31.4h
        xtn             \r5\().8b,  v0.8h
.endm

.macro store_grain_row r0, r1, r2, r3, r4, r5
        st1             {\r0\().16b,\r1\().16b}, [x0], #32
        st1             {\r2\().16b,\r3\().16b}, [x0], #32
        st1             {\r4\().16b},  [x0], #16
        st1             {\r5\().h}[0], [x0], #2
.endm

.macro get_grain_row_44 r0, r1, r2
        bl              get_gaussian_neon
        srshl           \r2\().8h,  v0.8h,  v31.8h
        xtn             \r0\().8b,  \r2\().8h
        bl              get_gaussian_neon
        srshl           \r2\().8h,  v0.8h,  v31.8h
        xtn2            \r0\().16b, \r2\().8h
        bl              get_gaussian_neon
        srshl           \r2\().8h,  v0.8h,  v31.8h
        xtn             \r1\().8b,  \r2\().8h
        bl              get_gaussian_neon
        srshl           \r2\().8h,  v0.8h,  v31.8h
        xtn2            \r1\().16b, \r2\().8h
        bl              get_gaussian_neon
        srshl           \r2\().8h,  v0.8h,  v31.8h
        xtn             \r2\().8b,  \r2\().8h

        increment_seed  4
        read_rand       x14, 11,  3
        read_rand       x15, 11,  2
        add             x14, x3,  x14, lsl #1
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[0], [x14]
        read_rand       x14, 11,  1
        ld1             {v0.h}[1], [x15]
        read_rand       x15, 11,  0
        add             x14, x3,  x14, lsl #1
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[2], [x14]
        ld1             {v0.h}[3], [x15]
        srshl           v0.4h,      v0.4h,  v31.4h
        xtn2            \r2\().16b, v0.8h
.endm

.macro store_grain_row_44 r0, r1, r2
        st1             {\r0\().16b,\r1\().16b}, [x0], #32
        st1             {\r2\().16b},  [x0]
        add             x0,  x0,  #GRAIN_WIDTH-32
.endm

function get_grain_2_neon
        increment_seed  2
        read_rand       x14, 11,  1
        read_rand       x15, 11,  0
        add             x14, x3,  x14, lsl #1
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[0], [x14]
        ld1             {v0.h}[1], [x15]
        srshl           v0.4h,   v0.4h,   v31.4h
        xtn             v0.8b,   v0.8h
        ret
endfunc

.macro get_grain_2 dst
        bl              get_grain_2_neon
.ifnc \dst, v0
        mov             \dst\().8b, v0.8b
.endif
.endm

// w15 holds the number of entries to produce
// w14, w16 and w17 hold the previous output entries
// v0 holds the vector of produced entries
// v1 holds the input vector of sums from above
.macro output_lag n
function output_lag\n\()_neon
1:
        read_shift_rand x13, 11
        mov             w11, v1.s[0]
        ldrsh           w12, [x3, x13, lsl #1]
        ext             v0.16b,  v0.16b,  v0.16b,  #1
.if \n == 1
        madd            w11, w14, w4,  w11        // sum (above) + *coeff * prev output
.elseif \n == 2
        madd            w11, w16, w4,  w11        // sum (above) + *coeff * prev output 1
        madd            w11, w14, w17, w11        // += *coeff * prev output 2
        mov             w16, w14
.else
        madd            w11, w17, w4,  w11        // sum (above) + *coeff * prev output 1
        madd            w11, w16, w20, w11        // sum (above) + *coeff * prev output 2
        madd            w11, w14, w21, w11        // += *coeff * prev output 3
        mov             w17, w16
        mov             w16, w14
.endif
        add             w14, w11, w8              // 1 << (ar_coeff_shift - 1)
        add             w12, w12, w10             // 1 << (4 + grain_scale_shift - 1)
        asr             w14, w14, w7              // >> ar_coeff_shift
        asr             w12, w12, w9              // >> (4 + grain_scale_shift)
        add             w14, w14, w12
        cmp             w14, w5
        csel            w14, w14, w5,  le
        cmp             w14, w6
        csel            w14, w14, w6,  ge
        subs            w15, w15, #1
        ext             v1.16b,  v1.16b,  v1.16b,  #4
        ins             v0.b[15], w14
        b.gt            1b
        ret
endfunc
.endm

output_lag 1
output_lag 2
output_lag 3


function sum_lag1_above_neon
        smull           v2.8h,   v3.8b,   v28.8b
        smull2          v3.8h,   v3.16b,  v28.16b
        smull           v4.8h,   v0.8b,   v27.8b
        smull2          v5.8h,   v0.16b,  v27.16b
        smull           v6.8h,   v1.8b,   v29.8b
        smull2          v7.8h,   v1.16b,  v29.16b
        saddl           v0.4s,   v2.4h,   v4.4h
        saddl2          v1.4s,   v2.8h,   v4.8h
        saddl           v2.4s,   v3.4h,   v5.4h
        saddl2          v3.4s,   v3.8h,   v5.8h
        saddw           v4.4s,   v0.4s,   v6.4h
        saddw2          v5.4s,   v1.4s,   v6.8h
        saddw           v6.4s,   v2.4s,   v7.4h
        saddw2          v7.4s,   v3.4s,   v7.8h
        ret
endfunc

.macro sum_lag_n_body lag, type, uv_layout, edge, elems, store, uv_coeff
        bl              sum_\lag\()_above_neon
.ifc \type, uv_420
        add             x12, x19, #GRAIN_WIDTH
        ld1             {v22.16b, v23.16b}, [x19], #32
        ld1             {v24.16b, v25.16b}, [x12]
        saddlp          v22.8h,  v22.16b
        saddlp          v23.8h,  v23.16b
        saddlp          v24.8h,  v24.16b
        saddlp          v25.8h,  v25.16b
        add             v22.8h,  v22.8h,  v24.8h
        add             v23.8h,  v23.8h,  v25.8h
        rshrn           v0.8b,   v22.8h,  #2
        rshrn2          v0.16b,  v23.8h,  #2
.endif
.ifc \type, uv_422
        ld1             {v22.16b, v23.16b}, [x19], #32
        saddlp          v22.8h,  v22.16b
        saddlp          v23.8h,  v23.16b
        rshrn           v0.8b,   v22.8h,  #1
        rshrn2          v0.16b,  v23.8h,  #1
.endif
.ifc \type, uv_444
        ld1             {v0.16b}, [x19], #16
.endif
.if \uv_layout
.ifnb \uv_coeff
        dup             v1.16b,  \uv_coeff
        smull           v2.8h,   v0.8b,   v1.8b
        smull2          v3.8h,   v0.16b,  v1.16b
.else
        smull           v2.8h,   v0.8b,   v30.8b
        smull2          v3.8h,   v0.16b,  v30.16b
.endif
        saddw           v4.4s,   v4.4s,   v2.4h
        saddw2          v5.4s,   v5.4s,   v2.8h
        saddw           v6.4s,   v6.4s,   v3.4h
        saddw2          v7.4s,   v7.4s,   v3.8h
.endif
.if \uv_layout && \elems == 16
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 444 && \elems == 15
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 422 && \elems == 9
        b               sum_\lag\()_uv_420_\edge\()_start
.else
sum_\lag\()_\type\()_\edge\()_start:
.ifc \edge, left
        increment_seed  4
        read_rand       x12, 11,  3
        read_rand       x13, 11,  2
        read_rand       x14, 11,  1
        add             x12, x3,  x12, lsl #1
        add             x13, x3,  x13, lsl #1
        add             x14, x3,  x14, lsl #1
        ld1             {v0.h}[5], [x12]
        ld1             {v0.h}[6], [x13]
        ld1             {v0.h}[7], [x14]
        lsl             x2,  x2,  #1             // shift back the state as if we'd done increment_seed with shift=0
        srshl           v0.8h,   v0.8h,   v31.8h
        xtn2            v0.16b,  v0.8h
        ext             v4.16b,  v4.16b,  v4.16b,  #12
.ifc \lag, lag3
        smov            w17, v0.b[13]
.endif
.ifnc \lag, lag1
        smov            w16, v0.b[14]
.endif
        smov            w14, v0.b[15]

        mov             v1.16b,  v4.16b
        mov             w15, #1
        bl              output_\lag\()_neon
.else
        increment_seed  4, shift=0
        mov             v1.16b,  v4.16b
        mov             w15, #4
        bl              output_\lag\()_neon
.endif

        increment_seed  4, shift=0
        mov             v1.16b,  v5.16b
        mov             w15, #4
        bl              output_\lag\()_neon

        increment_seed  4, shift=0
        mov             v1.16b,  v6.16b
.if \elems == 9
        mov             w15, #1
        bl              output_\lag\()_neon
        lsr             w2,  w2,  #3

        read_rand       x12, 11,  2
        read_rand       x13, 11,  1
        read_rand       x14, 11,  0
        add             x12, x3,  x12, lsl #1
        add             x13, x3,  x13, lsl #1
        add             x14, x3,  x14, lsl #1
        ld1             {v1.h}[0], [x12]
        ld1             {v1.h}[1], [x13]
        ld1             {v1.h}[2], [x14]
        srshl           v1.4h,   v1.4h,   v31.4h
        xtn             v1.8b,   v1.8h
        ext             v0.16b,  v0.16b,  v1.16b,  #7
.else
        mov             w15, #4
        bl              output_\lag\()_neon

        increment_seed  4, shift=0
        mov             v1.16b,  v7.16b

.ifc \edge, right
        mov             w15, #3
        bl              output_\lag\()_neon
        read_shift_rand x15, 11
        add             x15, x3,  x15, lsl #1
        ld1             {v1.h}[0], [x15]
        srshl           v1.4h,   v1.4h,   v31.4h
        ext             v0.16b,  v0.16b,  v1.16b,  #1
.else
        mov             w15, #4
        bl              output_\lag\()_neon
.endif
.endif
.if \store
        st1             {v0.16b}, [x0], #16
.endif
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
.endif
.endm

.macro sum_lag1_func type, uv_layout, edge, elems=16
function sum_\type\()_lag1_\edge\()_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        sum_lag_n_body  lag1, \type, \uv_layout, \edge, \elems, store=0
endfunc
.endm

sum_lag1_func y,      0,   left
sum_lag1_func y,      0,   mid
sum_lag1_func y,      0,   right, 15
sum_lag1_func uv_444, 444, left
sum_lag1_func uv_444, 444, mid
sum_lag1_func uv_444, 444, right, 15
sum_lag1_func uv_422, 422, left
sum_lag1_func uv_422, 422, mid
sum_lag1_func uv_422, 422, right, 9
sum_lag1_func uv_420, 420, left
sum_lag1_func uv_420, 420, mid
sum_lag1_func uv_420, 420, right, 9

.macro sum_lag1 type, dst, left, mid, right, edge=mid
        mov             v3.16b,  \mid\().16b
        ext             v0.16b,  \left\().16b, \mid\().16b,   #15
        ext             v1.16b,  \mid\().16b,  \right\().16b, #1
        bl              sum_\type\()_lag1_\edge\()_neon
        mov             \dst\().16b, v0.16b
.endm

.macro sum_y_lag1 dst, left, mid, right, edge=mid
        sum_lag1        y, \dst, \left, \mid, \right, \edge
.endm

.macro sum_uv_444_lag1 dst, left, mid, right, edge=mid
        sum_lag1        uv_444, \dst, \left, \mid, \right, \edge
.endm

.macro sum_uv_422_lag1 dst, left, mid, right, edge=mid
        sum_lag1        uv_422, \dst, \left, \mid, \right, \edge
.endm

.macro sum_uv_420_lag1 dst, left, mid, right, edge=mid
        sum_lag1        uv_420, \dst, \left, \mid, \right, \edge
.endm


function sum_lag2_above_neon
        sub             x12, x0,  #2*GRAIN_WIDTH - 16
        sub             x13, x0,  #1*GRAIN_WIDTH - 16
        ld1             {v18.16b}, [x12] // load top right
        ld1             {v21.16b}, [x13]

        ext             v22.16b, v16.16b, v17.16b, #14 // top left, top mid
        dup             v26.16b, v30.b[0]
        ext             v23.16b, v16.16b, v17.16b, #15
        dup             v27.16b, v30.b[1]
        ext             v0.16b,  v17.16b, v18.16b, #1  // top mid, top right
        dup             v28.16b, v30.b[3]
        ext             v1.16b,  v17.16b, v18.16b, #2
        dup             v29.16b, v30.b[4]

        smull           v2.8h,   v22.8b,  v26.8b
        smull2          v3.8h,   v22.16b, v26.16b
        smull           v4.8h,   v23.8b,  v27.8b
        smull2          v5.8h,   v23.16b, v27.16b
        smull           v6.8h,   v0.8b,   v28.8b
        smull2          v7.8h,   v0.16b,  v28.16b
        smull           v0.8h,   v1.8b,   v29.8b
        smull2          v1.8h,   v1.16b,  v29.16b
        saddl           v22.4s,  v2.4h,   v4.4h
        saddl2          v23.4s,  v2.8h,   v4.8h
        saddl           v26.4s,  v3.4h,   v5.4h
        saddl2          v27.4s,  v3.8h,   v5.8h
        saddl           v2.4s,   v0.4h,   v6.4h
        saddl2          v3.4s,   v0.8h,   v6.8h
        saddl           v6.4s,   v1.4h,   v7.4h
        saddl2          v7.4s,   v1.8h,   v7.8h
        add             v4.4s,   v22.4s,  v2.4s
        add             v5.4s,   v23.4s,  v3.4s
        add             v6.4s,   v26.4s,  v6.4s
        add             v7.4s,   v27.4s,  v7.4s

        ext             v22.16b, v19.16b, v20.16b, #14 // top left, top mid
        dup             v26.16b, v30.b[5]
        ext             v23.16b, v19.16b, v20.16b, #15
        dup             v27.16b, v30.b[6]
        ext             v0.16b,  v20.16b, v21.16b, #1  // top mid, top right
        dup             v28.16b, v30.b[8]
        ext             v1.16b,  v20.16b, v21.16b, #2
        dup             v29.16b, v30.b[9]

        smull           v2.8h,   v22.8b,  v26.8b
        smull2          v3.8h,   v22.16b, v26.16b
        smull           v22.8h,  v23.8b,  v27.8b
        smull2          v23.8h,  v23.16b, v27.16b
        smull           v26.8h,  v0.8b,   v28.8b
        smull2          v27.8h,  v0.16b,  v28.16b
        smull           v28.8h,  v1.8b,   v29.8b
        smull2          v29.8h,  v1.16b,  v29.16b
        saddl           v0.4s,   v2.4h,   v22.4h
        saddl2          v1.4s,   v2.8h,   v22.8h
        saddl           v2.4s,   v3.4h,   v23.4h
        saddl2          v3.4s,   v3.8h,   v23.8h
        saddl           v22.4s,  v26.4h,  v28.4h
        saddl2          v23.4s,  v26.8h,  v28.8h
        saddl           v26.4s,  v27.4h,  v29.4h
        saddl2          v27.4s,  v27.8h,  v29.8h
        add             v0.4s,   v0.4s,   v22.4s
        add             v1.4s,   v1.4s,   v23.4s
        add             v2.4s,   v2.4s,   v26.4s
        add             v3.4s,   v3.4s,   v27.4s
        dup             v26.16b, v30.b[2]
        dup             v27.16b, v30.b[7]
        smull           v22.8h,  v17.8b,  v26.8b
        smull2          v23.8h,  v17.16b, v26.16b
        smull           v24.8h,  v20.8b,  v27.8b
        smull2          v25.8h,  v20.16b, v27.16b
        add             v4.4s,   v4.4s,   v0.4s
        add             v5.4s,   v5.4s,   v1.4s
        add             v6.4s,   v6.4s,   v2.4s
        add             v7.4s,   v7.4s,   v3.4s

        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b

        saddl           v0.4s,   v22.4h,  v24.4h
        saddl2          v1.4s,   v22.8h,  v24.8h
        saddl           v2.4s,   v23.4h,  v25.4h
        saddl2          v3.4s,   v23.8h,  v25.8h
        mov             v19.16b, v20.16b
        mov             v20.16b, v21.16b
        add             v4.4s,   v4.4s,   v0.4s
        add             v5.4s,   v5.4s,   v1.4s
        add             v6.4s,   v6.4s,   v2.4s
        add             v7.4s,   v7.4s,   v3.4s
        ret
endfunc

.macro sum_lag2_func type, uv_layout, edge, elems=16
function sum_\type\()_lag2_\edge\()_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
.ifc \edge, left
        sub             x12, x0,  #2*GRAIN_WIDTH
        sub             x13, x0,  #1*GRAIN_WIDTH
        ld1             {v17.16b}, [x12] // load the previous block right above
        ld1             {v20.16b}, [x13]
.endif
        sum_lag_n_body  lag2, \type, \uv_layout, \edge, \elems, store=1, uv_coeff=v30.b[12]
endfunc
.endm

sum_lag2_func y,      0,   left
sum_lag2_func y,      0,   mid
sum_lag2_func y,      0,   right, 15
sum_lag2_func uv_444, 444, left
sum_lag2_func uv_444, 444, mid
sum_lag2_func uv_444, 444, right, 15
sum_lag2_func uv_422, 422, left
sum_lag2_func uv_422, 422, mid
sum_lag2_func uv_422, 422, right, 9
sum_lag2_func uv_420, 420, left
sum_lag2_func uv_420, 420, mid
sum_lag2_func uv_420, 420, right, 9


function sum_lag3_above_neon
        sub             x11, x0,  #3*GRAIN_WIDTH - 16
        sub             x12, x0,  #2*GRAIN_WIDTH - 16
        sub             x13, x0,  #1*GRAIN_WIDTH - 16
        ld1             {v15.16b}, [x11] // load top right
        ld1             {v18.16b}, [x12]
        ld1             {v21.16b}, [x13]

        ext             v8.16b,  v13.16b, v14.16b, #13 // top left, top mid
        dup             v22.16b, v29.b[0]
        ext             v9.16b,  v13.16b, v14.16b, #14
        dup             v23.16b, v29.b[1]
        ext             v10.16b, v13.16b, v14.16b, #15
        dup             v24.16b, v29.b[2]
        dup             v25.16b, v29.b[3]
        ext             v11.16b, v14.16b, v15.16b, #1  // top mid, top right
        dup             v26.16b, v29.b[4]
        ext             v12.16b, v14.16b, v15.16b, #2
        dup             v27.16b, v29.b[5]
        ext             v13.16b, v14.16b, v15.16b, #3
        dup             v28.16b, v29.b[6]

        smull           v0.8h,   v8.8b,   v22.8b
        smull2          v1.8h,   v8.16b,  v22.16b
        smull           v2.8h,   v9.8b,   v23.8b
        smull2          v3.8h,   v9.16b,  v23.16b
        smull           v8.8h,   v10.8b,  v24.8b
        smull2          v9.8h,   v10.16b, v24.16b
        smull           v10.8h,  v11.8b,  v26.8b
        smull2          v11.8h,  v11.16b, v26.16b
        saddl           v22.4s,  v0.4h,   v2.4h
        saddl2          v23.4s,  v0.8h,   v2.8h
        saddl           v24.4s,  v1.4h,   v3.4h
        saddl2          v26.4s,  v1.8h,   v3.8h
        saddl           v0.4s,   v8.4h,   v10.4h
        saddl2          v1.4s,   v8.8h,   v10.8h
        saddl           v2.4s,   v9.4h,   v11.4h
        saddl2          v3.4s,   v9.8h,   v11.8h
        smull           v8.8h,   v12.8b,  v27.8b
        smull2          v9.8h,   v12.16b, v27.16b
        smull           v10.8h,  v13.8b,  v28.8b
        smull2          v11.8h,  v13.16b, v28.16b
        smull           v12.8h,  v14.8b,  v25.8b
        smull2          v13.8h,  v14.16b, v25.16b
        add             v4.4s,   v22.4s,  v0.4s
        add             v5.4s,   v23.4s,  v1.4s
        add             v6.4s,   v24.4s,  v2.4s
        add             v7.4s,   v26.4s,  v3.4s
        saddl           v0.4s,   v8.4h,   v10.4h
        saddl2          v1.4s,   v8.8h,   v10.8h
        saddl           v2.4s,   v9.4h,   v11.4h
        saddl2          v3.4s,   v9.8h,   v11.8h
        add             v4.4s,   v4.4s,   v0.4s
        add             v5.4s,   v5.4s,   v1.4s
        add             v6.4s,   v6.4s,   v2.4s
        add             v7.4s,   v7.4s,   v3.4s
        saddw           v4.4s,   v4.4s,   v12.4h
        saddw2          v5.4s,   v5.4s,   v12.8h
        saddw           v6.4s,   v6.4s,   v13.4h
        saddw2          v7.4s,   v7.4s,   v13.8h

        ext             v8.16b,  v16.16b, v17.16b, #13 // top left, top mid
        dup             v22.16b, v29.b[7]
        ext             v9.16b,  v16.16b, v17.16b, #14
        dup             v23.16b, v29.b[8]
        ext             v10.16b, v16.16b, v17.16b, #15
        dup             v24.16b, v29.b[9]
        dup             v25.16b, v29.b[10]
        ext             v11.16b, v17.16b, v18.16b, #1  // top mid, top right
        dup             v26.16b, v29.b[11]
        ext             v12.16b, v17.16b, v18.16b, #2
        dup             v27.16b, v29.b[12]
        ext             v13.16b, v17.16b, v18.16b, #3
        dup             v28.16b, v29.b[13]

        smull           v0.8h,   v8.8b,   v22.8b
        smull2          v1.8h,   v8.16b,  v22.16b
        smull           v2.8h,   v9.8b,   v23.8b
        smull2          v3.8h,   v9.16b,  v23.16b
        smull           v8.8h,   v10.8b,  v24.8b
        smull2          v9.8h,   v10.16b, v24.16b
        smull           v10.8h,  v11.8b,  v26.8b
        smull2          v11.8h,  v11.16b, v26.16b
        saddl           v22.4s,  v0.4h,   v2.4h
        saddl2          v23.4s,  v0.8h,   v2.8h
        saddl           v24.4s,  v1.4h,   v3.4h
        saddl2          v26.4s,  v1.8h,   v3.8h
        saddl           v0.4s,   v8.4h,   v10.4h
        saddl2          v1.4s,   v8.8h,   v10.8h
        saddl           v2.4s,   v9.4h,   v11.4h
        saddl2          v3.4s,   v9.8h,   v11.8h
        smull           v8.8h,   v12.8b,  v27.8b
        smull2          v9.8h,   v12.16b, v27.16b
        smull           v10.8h,  v13.8b,  v28.8b
        smull2          v11.8h,  v13.16b, v28.16b
        smull           v12.8h,  v17.8b,  v25.8b
        smull2          v13.8h,  v17.16b, v25.16b
        add             v22.4s,  v22.4s,  v0.4s
        add             v23.4s,  v23.4s,  v1.4s
        add             v24.4s,  v24.4s,  v2.4s
        add             v26.4s,  v26.4s,  v3.4s
        saddl           v0.4s,   v8.4h,   v10.4h
        saddl2          v1.4s,   v8.8h,   v10.8h
        saddl           v2.4s,   v9.4h,   v11.4h
        saddl2          v3.4s,   v9.8h,   v11.8h
        add             v4.4s,   v4.4s,   v22.4s
        add             v5.4s,   v5.4s,   v23.4s
        add             v6.4s,   v6.4s,   v24.4s
        add             v7.4s,   v7.4s,   v26.4s
        add             v4.4s,   v4.4s,   v0.4s
        add             v5.4s,   v5.4s,   v1.4s
        add             v6.4s,   v6.4s,   v2.4s
        add             v7.4s,   v7.4s,   v3.4s
        saddw           v4.4s,   v4.4s,   v12.4h
        saddw2          v5.4s,   v5.4s,   v12.8h
        saddw           v6.4s,   v6.4s,   v13.4h
        saddw2          v7.4s,   v7.4s,   v13.8h

        ext             v8.16b,  v19.16b, v20.16b, #13 // top left, top mid
        dup             v22.16b, v29.b[14]
        ext             v9.16b,  v19.16b, v20.16b, #14
        dup             v23.16b, v29.b[15]
        ext             v10.16b, v19.16b, v20.16b, #15
        dup             v24.16b, v30.b[0]
        dup             v25.16b, v30.b[1]
        ext             v11.16b, v20.16b, v21.16b, #1  // top mid, top right
        dup             v26.16b, v30.b[2]
        ext             v12.16b, v20.16b, v21.16b, #2
        dup             v27.16b, v30.b[3]
        ext             v13.16b, v20.16b, v21.16b, #3
        dup             v28.16b, v30.b[4]

        smull           v0.8h,   v8.8b,   v22.8b
        smull2          v1.8h,   v8.16b,  v22.16b
        smull           v2.8h,   v9.8b,   v23.8b
        smull2          v3.8h,   v9.16b,  v23.16b
        smull           v8.8h,   v10.8b,  v24.8b
        smull2          v9.8h,   v10.16b, v24.16b
        smull           v10.8h,  v11.8b,  v26.8b
        smull2          v11.8h,  v11.16b, v26.16b
        saddl           v22.4s,  v0.4h,   v2.4h
        saddl2          v23.4s,  v0.8h,   v2.8h
        saddl           v24.4s,  v1.4h,   v3.4h
        saddl2          v26.4s,  v1.8h,   v3.8h
        saddl           v0.4s,   v8.4h,   v10.4h
        saddl2          v1.4s,   v8.8h,   v10.8h
        saddl           v2.4s,   v9.4h,   v11.4h
        saddl2          v3.4s,   v9.8h,   v11.8h
        smull           v8.8h,   v12.8b,  v27.8b
        smull2          v9.8h,   v12.16b, v27.16b
        smull           v10.8h,  v13.8b,  v28.8b
        smull2          v11.8h,  v13.16b, v28.16b
        smull           v12.8h,  v20.8b,  v25.8b
        smull2          v19.8h,  v20.16b, v25.16b
        add             v22.4s,  v22.4s,  v0.4s
        add             v23.4s,  v23.4s,  v1.4s
        add             v24.4s,  v24.4s,  v2.4s
        add             v26.4s,  v26.4s,  v3.4s
        saddl           v0.4s,   v8.4h,   v10.4h
        saddl2          v1.4s,   v8.8h,   v10.8h
        saddl           v2.4s,   v9.4h,   v11.4h
        saddl2          v3.4s,   v9.8h,   v11.8h
        add             v4.4s,   v4.4s,   v22.4s
        add             v5.4s,   v5.4s,   v23.4s
        add             v6.4s,   v6.4s,   v24.4s
        add             v7.4s,   v7.4s,   v26.4s
        mov             v13.16b, v14.16b
        mov             v14.16b, v15.16b
        add             v4.4s,   v4.4s,   v0.4s
        add             v5.4s,   v5.4s,   v1.4s
        add             v6.4s,   v6.4s,   v2.4s
        add             v7.4s,   v7.4s,   v3.4s
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b
        saddw           v4.4s,   v4.4s,   v12.4h
        saddw2          v5.4s,   v5.4s,   v12.8h
        saddw           v6.4s,   v6.4s,   v19.4h
        saddw2          v7.4s,   v7.4s,   v19.8h

        mov             v19.16b, v20.16b
        mov             v20.16b, v21.16b
        ret
endfunc

.macro sum_lag3_func type, uv_layout, edge, elems=16
function sum_\type\()_lag3_\edge\()_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
.ifc \edge, left
        sub             x11, x0,  #3*GRAIN_WIDTH
        sub             x12, x0,  #2*GRAIN_WIDTH
        sub             x13, x0,  #1*GRAIN_WIDTH
        ld1             {v14.16b}, [x11] // load the previous block right above
        ld1             {v17.16b}, [x12]
        ld1             {v20.16b}, [x13]
.endif
        sum_lag_n_body  lag3, \type, \uv_layout, \edge, \elems, store=1, uv_coeff=v30.b[8]
endfunc
.endm

sum_lag3_func y,      0,   left
sum_lag3_func y,      0,   mid
sum_lag3_func y,      0,   right, 15
sum_lag3_func uv_444, 444, left
sum_lag3_func uv_444, 444, mid
sum_lag3_func uv_444, 444, right, 15
sum_lag3_func uv_422, 422, left
sum_lag3_func uv_422, 422, mid
sum_lag3_func uv_422, 422, right, 9
sum_lag3_func uv_420, 420, left
sum_lag3_func uv_420, 420, mid
sum_lag3_func uv_420, 420, right, 9

function generate_grain_rows_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
1:
        get_grain_row   v16, v17, v18, v19, v20, v21
        subs            w1,  w1,  #1
        store_grain_row v16, v17, v18, v19, v20, v21
        b.gt            1b
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function generate_grain_rows_44_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
1:
        get_grain_row_44 v16, v17, v18
        subs            w1,  w1,  #1
        store_grain_row_44 v16, v17, v18
        b.gt            1b
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function get_grain_row_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        get_grain_row   v16, v17, v18, v19, v20, v21
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function get_grain_row_44_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        get_grain_row_44 v16, v17, v18
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function add_uv_444_coeff_lag0_neon
add_coeff_lag0_start:
        smull           v2.8h,   v0.8b,   v27.8b
        smull2          v3.8h,   v0.16b,  v27.16b
        srshl           v2.8h,   v2.8h,   v28.8h
        srshl           v3.8h,   v3.8h,   v28.8h
        saddw           v2.8h,   v2.8h,   v1.8b
        saddw2          v3.8h,   v3.8h,   v1.16b
        sqxtn           v2.8b,   v2.8h
        sqxtn2          v2.16b,  v3.8h
        ret
endfunc

function add_uv_420_coeff_lag0_neon
        ld1             {v4.16b, v5.16b}, [x19], #32
        ld1             {v6.16b, v7.16b}, [x12], #32
        saddlp          v4.8h,   v4.16b
        saddlp          v5.8h,   v5.16b
        saddlp          v6.8h,   v6.16b
        saddlp          v7.8h,   v7.16b
        add             v4.8h,   v4.8h,   v6.8h
        add             v5.8h,   v5.8h,   v7.8h
        rshrn           v4.8b,   v4.8h,   #2
        rshrn2          v4.16b,  v5.8h,   #2
        and             v0.16b,  v4.16b,  v0.16b
        b               add_coeff_lag0_start
endfunc

function add_uv_422_coeff_lag0_neon
        ld1             {v4.16b, v5.16b}, [x19], #32
        saddlp          v4.8h,   v4.16b
        saddlp          v5.8h,   v5.16b
        rshrn           v4.8b,   v4.8h,   #1
        rshrn2          v4.16b,  v5.8h,   #1
        and             v0.16b,  v4.16b,  v0.16b
        b               add_coeff_lag0_start
endfunc

.macro gen_grain_82 type
function generate_grain_\type\()_8bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x30, x19, [sp, #-96]!

.ifc \type, uv_444
        mov             w13, w3
        mov             w14, #28
        add             x19, x1,  #3*GRAIN_WIDTH
        mov             x1,  x2
        mul             w13, w13, w14
.endif
        movrel          x3,  X(gaussian_sequence)
        ldr             w2,  [x1, #FGD_SEED]
        ldr             w9,  [x1, #FGD_GRAIN_SCALE_SHIFT]
.ifc \type, y
        add             x4,  x1,  #FGD_AR_COEFFS_Y
.else
        add             x4,  x1,  #FGD_AR_COEFFS_UV
.endif
        movrel          x16, gen_grain_\type\()_tbl
        ldr             w17, [x1, #FGD_AR_COEFF_LAG]
        add             w9,  w9,  #4
        ldrsw           x17, [x16, w17, uxtw #2]
        dup             v31.8h,  w9    // 4 + data->grain_scale_shift
        add             x16, x16, x17
        neg             v31.8h,  v31.8h

.ifc \type, uv_444
        cmp             w13, #0
        mov             w11, #0x49d8
        mov             w14, #0xb524
        add             x4,  x4,  w13, uxtw // Add offset to ar_coeffs_uv[1]
        csel            w11, w11, w14, ne
.endif

        ldr             w7,  [x1, #FGD_AR_COEFF_SHIFT]
        mov             w8,  #1
        mov             w10, #1
        lsl             w8,  w8,  w7        // 1 << ar_coeff_shift
        lsl             w10, w10, w9        // 1 << (4 + data->grain_scale_shift)
        lsr             w8,  w8,  #1        // 1 << (ar_coeff_shift - 1)
        lsr             w10, w10, #1        // 1 << (4 + data->grain_scale_shift - 1)
        mov             w5,  #127
        mov             w6,  #-128

.ifc \type, uv_444
        eor             w2,  w2,  w11
.endif

        br              x16

L(generate_grain_\type\()_lag0):
        AARCH64_VALID_JUMP_TARGET
.ifc \type, y
        mov             w1,  #GRAIN_HEIGHT
        bl              generate_grain_rows_neon
.else
        dup             v28.8h,  w7
        ld1r            {v27.16b}, [x4]     // ar_coeffs_uv[0]
        movi            v0.16b,  #0
        movi            v1.16b,  #255
        ext             v29.16b, v0.16b,  v1.16b,  #13
        ext             v30.16b, v1.16b,  v0.16b,  #1
        neg             v28.8h,  v28.8h

        mov             w1,  #3
        bl              generate_grain_rows_neon
        mov             w1,  #GRAIN_HEIGHT-3
1:
        ld1             {v22.16b, v23.16b, v24.16b, v25.16b}, [x19], #64
        bl              get_grain_row_neon
        and             v0.16b,  v22.16b, v29.16b
        mov             v1.16b,  v16.16b
        bl              add_uv_444_coeff_lag0_neon
        mov             v0.16b,  v23.16b
        mov             v1.16b,  v17.16b
        mov             v16.16b, v2.16b
        bl              add_uv_444_coeff_lag0_neon
        ld1             {v26.16b}, [x19], #16
        mov             v0.16b,  v24.16b
        mov             v1.16b,  v18.16b
        mov             v17.16b, v2.16b
        bl              add_uv_444_coeff_lag0_neon
        add             x19, x19, #2
        mov             v0.16b,  v25.16b
        mov             v1.16b,  v19.16b
        mov             v18.16b, v2.16b
        bl              add_uv_444_coeff_lag0_neon
        and             v0.16b,  v26.16b, v30.16b
        mov             v1.16b,  v20.16b
        mov             v19.16b, v2.16b
        bl              add_uv_444_coeff_lag0_neon
        mov             v20.16b, v2.16b
        subs            w1,  w1,  #1
        store_grain_row v16, v17, v18, v19, v20, v21
        b.gt            1b
.endif
        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag1):
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v27.16b}, [x4], #1 // ar_coeffs_y[0]
        ld1r            {v28.16b}, [x4], #1 // ar_coeffs_y[1]
        ld1r            {v29.16b}, [x4]     // ar_coeffs_y[2]
.ifc \type, y
        ldrsb           w4,  [x4, #1]       // ar_coeffs_y[3]
.else
        add             x4,  x4,  #2
.endif

        mov             w1,  #3
.ifc \type, uv_444
        ld1r            {v30.16b}, [x4]     // ar_coeffs_uv[4]
        ldursb          w4,  [x4, #-1]      // ar_coeffs_uv[3]
.endif
        bl              generate_grain_rows_neon

        mov             w1,  #GRAIN_HEIGHT - 3
1:
        sum_\type\()_lag1 v22, v16, v16, v17, left
        sum_\type\()_lag1 v23, v16, v17, v18
        sum_\type\()_lag1 v24, v17, v18, v19
        sum_\type\()_lag1 v25, v18, v19, v20
        sum_\type\()_lag1 v20, v19, v20, v21, right
        get_grain_2     v21
        subs            w1,  w1,  #1
.ifc \type, uv_444
        add             x19, x19, #2
.endif
        store_grain_row v22, v23, v24, v25, v20, v21
        mov             v16.16b, v22.16b
        mov             v17.16b, v23.16b
        mov             v18.16b, v24.16b
        mov             v19.16b, v25.16b
        b.gt            1b

        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag2):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v30.16b}, [x4]     // ar_coeffs_y[0-11], ar_coeffs_uv[0-12]

        smov            w4,  v30.b[10]
        smov            w17, v30.b[11]

        mov             w1,  #3
        bl              generate_grain_rows_neon

        mov             w1,  #GRAIN_HEIGHT - 3
1:
        bl              sum_\type\()_lag2_left_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_right_neon
        get_grain_2     v16
        subs            w1,  w1,  #1
.ifc \type, uv_444
        add             x19, x19, #2
.endif
        st1             {v16.h}[0], [x0], #2
        b.gt            1b

        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag3):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v29.16b, v30.16b}, [x4] // ar_coeffs_y[0-23], ar_coeffs_uv[0-24]
        stp             d8,  d9,  [sp, #16]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #48]
        stp             d14, d15, [sp, #64]
        stp             x20, x21, [sp, #80]

        smov            w4,  v30.b[5]
        smov            w20, v30.b[6]
        smov            w21, v30.b[7]

        mov             w1,  #3
        bl              generate_grain_rows_neon

        mov             w1,  #GRAIN_HEIGHT - 3
1:
        bl              sum_\type\()_lag3_left_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_right_neon
        get_grain_2     v16
        subs            w1,  w1,  #1
.ifc \type, uv_444
        add             x19, x19, #2
.endif
        st1             {v16.h}[0], [x0], #2
        b.gt            1b

        ldp             x20, x21, [sp, #80]
        ldp             d14, d15, [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

jumptable gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag0) - gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag1) - gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag2) - gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag3) - gen_grain_\type\()_tbl
endjumptable
.endm

gen_grain_82 y
gen_grain_82 uv_444

.macro set_height dst, type
.ifc \type, uv_420
        mov             \dst,  #SUB_GRAIN_HEIGHT-3
.else
        mov             \dst,  #GRAIN_HEIGHT-3
.endif
.endm

.macro increment_y_ptr reg, type
.ifc \type, uv_420
        add             \reg, \reg, #2*GRAIN_WIDTH-(3*32)
.else
        sub             \reg, \reg, #3*32-GRAIN_WIDTH
.endif
.endm

.macro gen_grain_44 type
function generate_grain_\type\()_8bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x30, x19, [sp, #-96]!

        mov             w13, w3
        mov             w14, #28
        add             x19, x1,  #3*GRAIN_WIDTH-3
        mov             x1,  x2
        mul             w13, w13, w14

        movrel          x3,  X(gaussian_sequence)
        ldr             w2,  [x1, #FGD_SEED]
        ldr             w9,  [x1, #FGD_GRAIN_SCALE_SHIFT]
        add             x4,  x1,  #FGD_AR_COEFFS_UV
        movrel          x16, gen_grain_\type\()_tbl
        ldr             w17, [x1, #FGD_AR_COEFF_LAG]
        add             w9,  w9,  #4
        ldrsw           x17, [x16, w17, uxtw #2]
        dup             v31.8h,  w9    // 4 + data->grain_scale_shift
        add             x16, x16, x17
        neg             v31.8h,  v31.8h

        cmp             w13, #0
        mov             w11, #0x49d8
        mov             w14, #0xb524
        add             x4,  x4,  w13, uxtw // Add offset to ar_coeffs_uv[1]
        csel            w11, w11, w14, ne

        ldr             w7,  [x1, #FGD_AR_COEFF_SHIFT]
        mov             w8,  #1
        mov             w10, #1
        lsl             w8,  w8,  w7        // 1 << ar_coeff_shift
        lsl             w10, w10, w9        // 1 << (4 + data->grain_scale_shift)
        lsr             w8,  w8,  #1        // 1 << (ar_coeff_shift - 1)
        lsr             w10, w10, #1        // 1 << (4 + data->grain_scale_shift - 1)
        mov             w5,  #127
        mov             w6,  #-128

        eor             w2,  w2,  w11

        br              x16

L(generate_grain_\type\()_lag0):
        AARCH64_VALID_JUMP_TARGET
        dup             v28.8h,  w7
        ld1r            {v27.16b}, [x4]     // ar_coeffs_uv[0]
        movi            v0.16b,  #0
        movi            v1.16b,  #255
        ext             v29.16b, v0.16b,  v1.16b,  #13
        ext             v30.16b, v1.16b,  v0.16b,  #7
        neg             v28.8h,  v28.8h

        mov             w1,  #3
        bl              generate_grain_rows_44_neon
        set_height      w1,  \type
1:
        bl              get_grain_row_44_neon
.ifc \type, uv_420
        add             x12, x19, #GRAIN_WIDTH
.endif
        mov             v0.16b,  v29.16b
        mov             v1.16b,  v16.16b
        bl              add_\type\()_coeff_lag0_neon
        movi            v0.16b,  #255
        mov             v1.16b,  v17.16b
        mov             v16.16b, v2.16b
        bl              add_\type\()_coeff_lag0_neon
        mov             v0.16b,  v30.16b
        mov             v1.16b,  v18.16b
        mov             v17.16b, v2.16b
        bl              add_\type\()_coeff_lag0_neon
        mov             v18.16b, v2.16b
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        store_grain_row_44 v16, v17, v18
        b.gt            1b

        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag1):
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v27.16b}, [x4], #1 // ar_coeffs_uv[0]
        ld1r            {v28.16b}, [x4], #1 // ar_coeffs_uv[1]
        ld1r            {v29.16b}, [x4]     // ar_coeffs_uv[2]
        add             x4,  x4,  #2

        mov             w1,  #3
        ld1r            {v30.16b}, [x4]     // ar_coeffs_u4[4]
        ldursb          w4,  [x4, #-1]      // ar_coeffs_uv[3]
        bl              generate_grain_rows_44_neon

        set_height      w1,  \type
1:
        sum_\type\()_lag1 v20, v16, v16, v17, left
        sum_\type\()_lag1 v21, v16, v17, v18
        sum_\type\()_lag1 v18, v17, v18, v18, right
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        store_grain_row_44 v20, v21, v18
        mov             v16.16b, v20.16b
        mov             v17.16b, v21.16b
        b.gt            1b

        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag2):
        AARCH64_VALID_JUMP_TARGET
        ld1             {v30.16b}, [x4]     // ar_coeffs_uv[0-12]

        smov            w4,  v30.b[10]
        smov            w17, v30.b[11]

        mov             w1,  #3
        bl              generate_grain_rows_44_neon

        set_height      w1,  \type
1:
        bl              sum_\type\()_lag2_left_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_right_neon
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        add             x0,  x0,  #GRAIN_WIDTH-48
        b.gt            1b

        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag3):
        AARCH64_VALID_JUMP_TARGET
        ldr             q29,      [x4]      // ar_coeffs_uv[0-15]
        ldr             q30,      [x4, #16] // ar_coeffs_uv[16-24]
        stp             d8,  d9,  [sp, #16]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #48]
        stp             d14, d15, [sp, #64]
        stp             x20, x21, [sp, #80]

        smov            w4,  v30.b[5]
        smov            w20, v30.b[6]
        smov            w21, v30.b[7]

        mov             w1,  #3
        bl              generate_grain_rows_44_neon

        set_height      w1,  \type
1:
        bl              sum_\type\()_lag3_left_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_right_neon
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        add             x0,  x0,  #GRAIN_WIDTH-48
        b.gt            1b

        ldp             x20, x21, [sp, #80]
        ldp             d14, d15, [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

jumptable gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag0) - gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag1) - gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag2) - gen_grain_\type\()_tbl
        .word L(generate_grain_\type\()_lag3) - gen_grain_\type\()_tbl
endjumptable
.endm

gen_grain_44 uv_420
gen_grain_44 uv_422

.macro gather_interleaved dst1, dst2, src1, src2, off
        umov            w14, \src1[0+\off]
        umov            w15, \src2[8+\off]
        umov            w16, \src1[2+\off]
        add             x14, x14, x3
        umov            w17, \src2[10+\off]
        add             x15, x15, x3
        ld1             {\dst1}[0+\off],  [x14]
        umov            w14, \src1[4+\off]
        add             x16, x16, x3
        ld1             {\dst2}[8+\off],  [x15]
        umov            w15, \src2[12+\off]
        add             x17, x17, x3
        ld1             {\dst1}[2+\off],  [x16]
        umov            w16, \src1[6+\off]
        add             x14, x14, x3
        ld1             {\dst2}[10+\off], [x17]
        umov            w17, \src2[14+\off]
        add             x15, x15, x3
        ld1             {\dst1}[4+\off],  [x14]
        add             x16, x16, x3
        ld1             {\dst2}[12+\off], [x15]
        add             x17, x17, x3
        ld1             {\dst1}[6+\off],  [x16]
        ld1             {\dst2}[14+\off], [x17]
.endm

.macro gather dst1, dst2, src1, src2
        gather_interleaved \dst1, \dst2, \src1, \src2, 0
        gather_interleaved \dst2, \dst1, \src2, \src1, 0
        gather_interleaved \dst1, \dst2, \src1, \src2, 1
        gather_interleaved \dst2, \dst1, \src2, \src1, 1
.endm

function gather32_neon
        gather          v4.b, v5.b, v0.b, v1.b
        ret
endfunc

function gather16_neon
        gather_interleaved v4.b, v5.b, v0.b, v0.b, 0
        gather_interleaved v4.b, v5.b, v0.b, v0.b, 1
        ins             v4.d[1], v5.d[1]
        ret
endfunc

const overlap_coeffs_0, align=4
        .byte 27, 17, 0,  0,  0,  0,  0,  0
        .byte 17, 27, 32, 32, 32, 32, 32, 32
endconst

const overlap_coeffs_1, align=4
        .byte 23, 0,  0,  0,  0,  0,  0,  0
        .byte 22, 32, 32, 32, 32, 32, 32, 32
endconst

.macro calc_offset offx, offy, src, sx, sy
        and             \offy, \src,  #0xF     // randval & 0xF
        lsr             \offx, \src,  #4       // randval >> 4
.if \sy == 0
        add             \offy, \offy, \offy    // 2 * (randval & 0xF)
.endif
.if \sx == 0
        add             \offx, \offx, \offx    // 2 * (randval >> 4)
.endif
.endm

.macro add_offset dst, offx, offy, src, stride
        madd            \dst, \stride, \offy, \src // grain_lut += grain_stride * offy
        add             \dst, \dst, \offx, uxtw // grain_lut += offx
.endm

// void dav1d_fgy_32x32_8bpc_neon(pixel *const dst, const pixel *const src,
//                                const ptrdiff_t stride,
//                                const uint8_t scaling[SCALING_SIZE],
//                                const int scaling_shift,
//                                const entry grain_lut[][GRAIN_WIDTH],
//                                const int offsets[][2],
//                                const int h, const ptrdiff_t clip,
//                                const ptrdiff_t type);
function fgy_32x32_8bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        ldr             w11, [x6, #8]          // offsets[1][0]
        ldr             w13, [x6, #4]          // offsets[0][1]
        ldr             w15, [x6, #12]         // offsets[1][1]
        ldr             w6,  [x6]              // offsets[0][0]
        ldr             w8,  [sp, #16]         // clip
        mov             x9,  #GRAIN_WIDTH      // grain_lut stride

        neg             w4,  w4
        dup             v29.8h,  w4            // -scaling_shift

        movrel          x16, overlap_coeffs_0

        cbz             w8,  1f
        // clip
        movi            v30.16b, #16
        movi            v31.16b, #235
        b               2f
1:
        // no clip
        movi            v30.16b, #0
        movi            v31.16b, #255
2:

        ld1             {v27.8b, v28.8b}, [x16] // overlap_coeffs

        add             x5,  x5,  #9           // grain_lut += 9
        add             x5,  x5,  x9,  lsl #3  // grain_lut += 8 * grain_stride
        add             x5,  x5,  x9           // grain_lut += grain_stride

        calc_offset     w11, w12, w11, 0,  0
        calc_offset     w13, w14, w13, 0,  0
        calc_offset     w15, w16, w15, 0,  0
        calc_offset     w6,  w10, w6,  0,  0

        add_offset      x12, w11, x12, x5,  x9
        add_offset      x14, w13, x14, x5,  x9
        add_offset      x16, w15, x16, x5,  x9
        add_offset      x5,  w6,  x10, x5,  x9

        ldr             w11, [sp, #24]         // type
        movrel          x13, fgy_loop_tbl

        add             x4,  x12, #32          // grain_lut += FG_BLOCK_SIZE * bx
        add             x6,  x14, x9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by

        tst             w11, #1
        ldrsw           x11, [x13, w11, uxtw #2]

        add             x8,  x16, x9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             x8,  x8,  #32          // grain_lut += FG_BLOCK_SIZE * bx

        add             x11, x13, x11

        b.eq            1f
        // y overlap
        dup             v6.16b,  v27.b[0]
        dup             v7.16b,  v27.b[1]
        mov             w10, w7                // backup actual h
        mov             w7,  #2
1:
        br              x11
endfunc

function fgy_loop_neon
.macro fgy ox, oy
L(loop_\ox\oy):
        AARCH64_VALID_JUMP_TARGET
1:
        ld1             {v0.16b,  v1.16b},  [x1],  x2 // src
.if \ox
        ld1             {v20.8b},           [x4],  x9 // grain_lut old
.endif
.if \oy
        ld1             {v22.16b, v23.16b}, [x6],  x9 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v21.8b},           [x8],  x9 // grain_lut top old
.endif
        ld1             {v18.16b, v19.16b}, [x5],  x9 // grain_lut

        bl              gather32_neon

.if \ox
        smull           v20.8h,  v20.8b,  v27.8b
        smlal           v20.8h,  v18.8b,  v28.8b
.endif

.if \oy
.if \ox
        smull           v21.8h,  v21.8b,  v27.8b
        smlal           v21.8h,  v22.8b,  v28.8b
        sqrshrn         v20.8b,  v20.8h,  #5
        sqrshrn         v21.8b,  v21.8h,  #5
.endif

.if \ox
        smull           v16.8h,  v20.8b,  v7.8b
.else
        smull           v16.8h,  v18.8b,  v7.8b
.endif
        smull2          v17.8h,  v18.16b, v7.16b
        smull           v18.8h,  v19.8b,  v7.8b
        smull2          v19.8h,  v19.16b, v7.16b
.if \ox
        smlal           v16.8h,  v21.8b,  v6.8b
.else
        smlal           v16.8h,  v22.8b,  v6.8b
.endif
        smlal2          v17.8h,  v22.16b, v6.16b
        smlal           v18.8h,  v23.8b,  v6.8b
        smlal2          v19.8h,  v23.16b, v6.16b
        sqrshrn         v22.8b,  v16.8h,  #5
        sqrshrn2        v22.16b, v17.8h,  #5
        sqrshrn         v23.8b,  v18.8h,  #5
        sqrshrn2        v23.16b, v19.8h,  #5
.endif

        // sxtl of grain
.if \oy
        sxtl            v16.8h,  v22.8b
        sxtl2           v17.8h,  v22.16b
        sxtl            v18.8h,  v23.8b
        sxtl2           v19.8h,  v23.16b
.elseif \ox
        sqrshrn         v20.8b,  v20.8h,  #5
        sxtl2           v17.8h,  v18.16b
        sxtl            v18.8h,  v19.8b
        sxtl2           v19.8h,  v19.16b
        sxtl            v16.8h,  v20.8b
.else
        sxtl            v16.8h,  v18.8b
        sxtl2           v17.8h,  v18.16b
        sxtl            v18.8h,  v19.8b
        sxtl2           v19.8h,  v19.16b
.endif

        uxtl            v2.8h,   v4.8b   // scaling
        uxtl2           v3.8h,   v4.16b
        uxtl            v4.8h,   v5.8b
        uxtl2           v5.8h,   v5.16b

        mul             v16.8h,  v16.8h,  v2.8h   // scaling * grain
        mul             v17.8h,  v17.8h,  v3.8h
        mul             v18.8h,  v18.8h,  v4.8h
        mul             v19.8h,  v19.8h,  v5.8h

        srshl           v16.8h,  v16.8h,  v29.8h  // round2(scaling * grain, scaling_shift)
        srshl           v17.8h,  v17.8h,  v29.8h
        srshl           v18.8h,  v18.8h,  v29.8h
        srshl           v19.8h,  v19.8h,  v29.8h

        uaddw           v16.8h,  v16.8h,  v0.8b   // *src + noise
        uaddw2          v17.8h,  v17.8h,  v0.16b
        uaddw           v18.8h,  v18.8h,  v1.8b
        uaddw2          v19.8h,  v19.8h,  v1.16b

        sqxtun          v0.8b,   v16.8h
        sqxtun2         v0.16b,  v17.8h
        sqxtun          v1.8b,   v18.8h
        sqxtun2         v1.16b,  v19.8h

        umax            v0.16b,  v0.16b,  v30.16b
        umax            v1.16b,  v1.16b,  v30.16b
        umin            v0.16b,  v0.16b,  v31.16b
        umin            v1.16b,  v1.16b,  v31.16b

        subs            w7,  w7,  #1
.if \oy
        dup             v6.16b,  v28.b[0]
        dup             v7.16b,  v28.b[1]
.endif
        st1             {v0.16b,  v1.16b},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w10, #2
        sub             w7,  w10, #2           // restore actual remaining h
        b.gt            L(loop_\ox\()0)
.endif
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
.endm

        fgy             0, 0
        fgy             0, 1
        fgy             1, 0
        fgy             1, 1
endfunc

jumptable fgy_loop_tbl
        .word L(loop_00) - fgy_loop_tbl
        .word L(loop_01) - fgy_loop_tbl
        .word L(loop_10) - fgy_loop_tbl
        .word L(loop_11) - fgy_loop_tbl
endjumptable

// void dav1d_fguv_32x32_420_8bpc_neon(pixel *const dst,
//                                     const pixel *const src,
//                                     const ptrdiff_t stride,
//                                     const uint8_t scaling[SCALING_SIZE],
//                                     const Dav1dFilmGrainData *const data,
//                                     const entry grain_lut[][GRAIN_WIDTH],
//                                     const pixel *const luma_row,
//                                     const ptrdiff_t luma_stride,
//                                     const int offsets[][2],
//                                     const ptrdiff_t h, const ptrdiff_t uv,
//                                     const ptrdiff_t is_id,
//                                     const ptrdiff_t type);
.macro fguv layout, sx, sy
function fguv_32x32_\layout\()_8bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        str             x30,      [sp, #-32]!
        str             d8,       [sp, #16]
        ldp             x8,  x9,  [sp, #32]    // offsets, h
        ldp             x10, x11, [sp, #48]    // uv, is_id

        ldr             w13, [x4, #FGD_SCALING_SHIFT]
        ldr             w12, [x4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        neg             w13, w13               // -scaling_shift

        // !csfl
        add             x10, x4,  x10, lsl #2  // + 4*uv
        add             x14, x10, #FGD_UV_LUMA_MULT
        add             x15, x10, #FGD_UV_MULT
        add             x10, x10, #FGD_UV_OFFSET
        ld1             {v8.h}[0], [x14]       // uv_luma_mult
        ld1r            {v24.8h},  [x10]       // uv_offset
        ld1             {v8.h}[1], [x15]       // uv_mult

        dup             v29.8h,  w13           // -scaling_shift

        cbz             w12, 1f
        // clip
        movi            v30.16b, #16
        movi            v31.16b, #240
        cbz             w11, 2f
        // is_id
        movi            v31.16b, #235
        b               2f
1:
        // no clip
        movi            v30.16b, #0
        movi            v31.16b, #255
2:

        ldr             w12, [x8, #8]          // offsets[1][0]
        ldr             w14, [x8, #4]          // offsets[0][1]
        ldr             w16, [x8, #12]         // offsets[1][1]
        ldr             w8,  [x8]              // offsets[0][0]

        mov             x10, #GRAIN_WIDTH      // grain_lut stride

        add             x5,  x5,  #(3 + (2 >> \sx)*3) // grain_lut += 9 or 6
.if \sy
        add             x5,  x5,  x10, lsl #2  // grain_lut += 4 * grain_stride
        add             x5,  x5,  x10, lsl #1  // grain_lut += 2 * grain_stride
.else
        add             x5,  x5,  x10, lsl #3  // grain_lut += 8 * grain_stride
        add             x5,  x5,  x10          // grain_lut += grain_stride
.endif

        calc_offset     w12, w13, w12, \sx, \sy
        calc_offset     w14, w15, w14, \sx, \sy
        calc_offset     w16, w17, w16, \sx, \sy
        calc_offset     w8,  w11, w8,  \sx, \sy

        add_offset      x13, w12, x13, x5,  x10
        add_offset      x15, w14, x15, x5,  x10
        add_offset      x17, w16, x17, x5,  x10
        add_offset      x5,  w8,  x11, x5,  x10

        add             x4,  x13, #(32 >> \sx) // grain_lut += FG_BLOCK_SIZE * bx
        add             x8,  x15, x10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             x11, x17, x10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             x11, x11, #(32 >> \sx) // grain_lut += FG_BLOCK_SIZE * bx

        ldr             w13, [sp, #64]         // type

        movrel          x16, overlap_coeffs_\sx
        movrel          x14, fguv_loop_sx\sx\()_tbl

        ld1             {v27.8b, v28.8b}, [x16] // overlap_coeffs
        tst             w13, #1
        ldrsw           x13, [x14, w13, uxtw #2]

        b.eq            1f
        // y overlap
        sub             w12, w9,  #(2 >> \sy)  // backup remaining h
        mov             w9,  #(2 >> \sy)

1:
        add             x13, x14, x13

.if \sy
        movi            v25.16b, #23
        movi            v26.16b, #22
.else
        movi            v25.16b, #27
        movi            v26.16b, #17
.endif

.if \sy
        add             x7,  x7,  x7           // luma_stride *= 2
.endif

        br              x13
endfunc
.endm

fguv 420, 1, 1
fguv 422, 1, 0
fguv 444, 0, 0

function fguv_loop_sx0_neon
.macro fguv_loop_sx0 csfl, ox, oy
L(fguv_loop_sx0_csfl\csfl\()_\ox\oy):
        AARCH64_VALID_JUMP_TARGET
1:
        ld1             {v0.16b,  v1.16b},  [x6],  x7  // luma
        ld1             {v6.16b,  v7.16b},  [x1],  x2  // src
.if \ox
        ld1             {v20.8b},           [x4],  x10 // grain_lut old
.endif
.if \oy
        ld1             {v22.16b, v23.16b}, [x8],  x10 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v21.8b},           [x11], x10 // grain_lut top old
.endif
        ld1             {v18.16b, v19.16b}, [x5],  x10 // grain_lut

.if !\csfl
        uxtl            v2.8h,   v0.8b
        uxtl2           v3.8h,   v0.16b
        uxtl            v4.8h,   v1.8b
        uxtl2           v5.8h,   v1.16b
        uxtl            v0.8h,   v6.8b
        uxtl2           v1.8h,   v6.16b
        uxtl            v16.8h,  v7.8b
        uxtl2           v17.8h,  v7.16b
        mul             v2.8h,   v2.8h,   v8.h[0]
        mul             v3.8h,   v3.8h,   v8.h[0]
        mul             v4.8h,   v4.8h,   v8.h[0]
        mul             v5.8h,   v5.8h,   v8.h[0]
        mul             v0.8h,   v0.8h,   v8.h[1]
        mul             v1.8h,   v1.8h,   v8.h[1]
        mul             v16.8h,  v16.8h,  v8.h[1]
        mul             v17.8h,  v17.8h,  v8.h[1]
        sqadd           v2.8h,   v2.8h,   v0.8h
        sqadd           v3.8h,   v3.8h,   v1.8h
        sqadd           v4.8h,   v4.8h,   v16.8h
        sqadd           v5.8h,   v5.8h,   v17.8h
        sshr            v2.8h,   v2.8h,   #6
        sshr            v3.8h,   v3.8h,   #6
        sshr            v4.8h,   v4.8h,   #6
        sshr            v5.8h,   v5.8h,   #6
        add             v2.8h,   v2.8h,   v24.8h
        add             v3.8h,   v3.8h,   v24.8h
        add             v4.8h,   v4.8h,   v24.8h
        add             v5.8h,   v5.8h,   v24.8h
        sqxtun          v0.8b,   v2.8h
        sqxtun2         v0.16b,  v3.8h
        sqxtun          v1.8b,   v4.8h
        sqxtun2         v1.16b,  v5.8h
.endif

        bl              gather32_neon

.if \ox
        smull           v20.8h,  v20.8b,  v27.8b
        smlal           v20.8h,  v18.8b,  v28.8b
.endif

.if \oy
.if \ox
        smull           v21.8h,  v21.8b,  v27.8b
        smlal           v21.8h,  v22.8b,  v28.8b
        sqrshrn         v20.8b,  v20.8h,  #5
        sqrshrn         v21.8b,  v21.8h,  #5
.endif

.if \ox
        smull           v16.8h,  v20.8b,  v26.8b
.else
        smull           v16.8h,  v18.8b,  v26.8b
.endif
        smull2          v17.8h,  v18.16b, v26.16b
        smull           v18.8h,  v19.8b,  v26.8b
        smull2          v19.8h,  v19.16b, v26.16b
.if \ox
        smlal           v16.8h,  v21.8b,  v25.8b
.else
        smlal           v16.8h,  v22.8b,  v25.8b
.endif
        smlal2          v17.8h,  v22.16b, v25.16b
        smlal           v18.8h,  v23.8b,  v25.8b
        smlal2          v19.8h,  v23.16b, v25.16b
        sqrshrn         v22.8b,  v16.8h,  #5
        sqrshrn2        v22.16b, v17.8h,  #5
        sqrshrn         v23.8b,  v18.8h,  #5
        sqrshrn2        v23.16b, v19.8h,  #5
.endif

        // sxtl of grain
.if \oy
        sxtl            v16.8h,  v22.8b
        sxtl2           v17.8h,  v22.16b
        sxtl            v18.8h,  v23.8b
        sxtl2           v19.8h,  v23.16b
.elseif \ox
        sqrshrn         v20.8b,  v20.8h,  #5
        sxtl2           v17.8h,  v18.16b
        sxtl            v18.8h,  v19.8b
        sxtl2           v19.8h,  v19.16b
        sxtl            v16.8h,  v20.8b
.else
        sxtl            v16.8h,  v18.8b
        sxtl2           v17.8h,  v18.16b
        sxtl            v18.8h,  v19.8b
        sxtl2           v19.8h,  v19.16b
.endif

        uxtl            v2.8h,   v4.8b   // scaling
        uxtl2           v3.8h,   v4.16b
        uxtl            v4.8h,   v5.8b
        uxtl2           v5.8h,   v5.16b

        mul             v16.8h,  v16.8h,  v2.8h   // scaling * grain
        mul             v17.8h,  v17.8h,  v3.8h
        mul             v18.8h,  v18.8h,  v4.8h
        mul             v19.8h,  v19.8h,  v5.8h

        srshl           v16.8h,  v16.8h,  v29.8h  // round2(scaling * grain, scaling_shift)
        srshl           v17.8h,  v17.8h,  v29.8h
        srshl           v18.8h,  v18.8h,  v29.8h
        srshl           v19.8h,  v19.8h,  v29.8h

        uaddw           v16.8h,  v16.8h,  v6.8b   // *src + noise
        uaddw2          v17.8h,  v17.8h,  v6.16b
        uaddw           v18.8h,  v18.8h,  v7.8b
        uaddw2          v19.8h,  v19.8h,  v7.16b

        sqxtun          v0.8b,   v16.8h
        sqxtun2         v0.16b,  v17.8h
        sqxtun          v1.8b,   v18.8h
        sqxtun2         v1.16b,  v19.8h

        umax            v0.16b,  v0.16b,  v30.16b
        umax            v1.16b,  v1.16b,  v30.16b
        umin            v0.16b,  v0.16b,  v31.16b
        umin            v1.16b,  v1.16b,  v31.16b

        subs            w9,  w9,  #1
.if \oy
        dup             v25.16b, v28.b[0]
        dup             v26.16b, v28.b[1]
.endif
        st1             {v0.16b,  v1.16b},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w12, #0
        mov             w9,  w12               // restore actual remaining h
        b.gt            L(fguv_loop_sx0_csfl\csfl\()_\ox\()0)
.endif
        b               9f
.endm
        fguv_loop_sx0   0, 0, 0
        fguv_loop_sx0   0, 0, 1
        fguv_loop_sx0   0, 1, 0
        fguv_loop_sx0   0, 1, 1
        fguv_loop_sx0   1, 0, 0
        fguv_loop_sx0   1, 0, 1
        fguv_loop_sx0   1, 1, 0
        fguv_loop_sx0   1, 1, 1

9:
        ldr             d8,       [sp, #16]
        ldr             x30,      [sp], #32
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

jumptable fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl0_00) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl0_01) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl0_10) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl0_11) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl1_00) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl1_01) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl1_10) - fguv_loop_sx0_tbl
        .word L(fguv_loop_sx0_csfl1_11) - fguv_loop_sx0_tbl
endjumptable

function fguv_loop_sx1_neon
.macro fguv_loop_sx1 csfl, ox, oy
L(fguv_loop_sx1_csfl\csfl\()_\ox\oy):
        AARCH64_VALID_JUMP_TARGET
1:
        ld1             {v0.16b, v1.16b},  [x6],  x7  // luma
        ld1             {v6.16b},          [x1],  x2  // src
.if \ox
        ld1             {v20.8b},          [x4],  x10 // grain_lut old
.endif
.if \oy
        ld1             {v22.16b},         [x8],  x10 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v21.8b},          [x11], x10 // grain_lut top old
.endif
        ld1             {v18.16b},         [x5],  x10 // grain_lut

        uaddlp          v2.8h,   v0.16b
        uaddlp          v3.8h,   v1.16b
.if \csfl
        rshrn           v0.8b,   v2.8h,   #1
        rshrn2          v0.16b,  v3.8h,   #1
.else
        urshr           v2.8h,   v2.8h,   #1
        urshr           v3.8h,   v3.8h,   #1
        uxtl            v0.8h,   v6.8b
        uxtl2           v1.8h,   v6.16b
        mul             v2.8h,   v2.8h,   v8.h[0]
        mul             v3.8h,   v3.8h,   v8.h[0]
        mul             v0.8h,   v0.8h,   v8.h[1]
        mul             v1.8h,   v1.8h,   v8.h[1]
        sqadd           v2.8h,   v2.8h,   v0.8h
        sqadd           v3.8h,   v3.8h,   v1.8h
        sshr            v2.8h,   v2.8h,   #6
        sshr            v3.8h,   v3.8h,   #6
        add             v2.8h,   v2.8h,   v24.8h
        add             v3.8h,   v3.8h,   v24.8h
        sqxtun          v0.8b,   v2.8h
        sqxtun2         v0.16b,  v3.8h
.endif

        bl              gather16_neon

.if \ox
        smull           v20.8h,  v20.8b,  v27.8b
        smlal           v20.8h,  v18.8b,  v28.8b
.endif

.if \oy
.if \ox
        smull           v21.8h,  v21.8b,  v27.8b
        smlal           v21.8h,  v22.8b,  v28.8b
        sqrshrn         v20.8b,  v20.8h,  #5
        sqrshrn         v21.8b,  v21.8h,  #5
.endif

.if \ox
        smull           v16.8h,  v20.8b,  v26.8b
.else
        smull           v16.8h,  v18.8b,  v26.8b
.endif
        smull2          v17.8h,  v18.16b, v26.16b
.if \ox
        smlal           v16.8h,  v21.8b,  v25.8b
.else
        smlal           v16.8h,  v22.8b,  v25.8b
.endif
        smlal2          v17.8h,  v22.16b, v25.16b
        sqrshrn         v22.8b,  v16.8h,  #5
        sqrshrn2        v22.16b, v17.8h,  #5
.endif

        // sxtl of grain
.if \oy
        sxtl            v16.8h,  v22.8b
        sxtl2           v17.8h,  v22.16b
.elseif \ox
        sqrshrn         v20.8b,  v20.8h,  #5
        sxtl2           v17.8h,  v18.16b
        sxtl            v16.8h,  v20.8b
.else
        sxtl            v16.8h,  v18.8b
        sxtl2           v17.8h,  v18.16b
.endif

        uxtl            v2.8h,   v4.8b   // scaling
        uxtl2           v3.8h,   v4.16b

        mul             v16.8h,  v16.8h,  v2.8h   // scaling * grain
        mul             v17.8h,  v17.8h,  v3.8h

        srshl           v16.8h,  v16.8h,  v29.8h  // round2(scaling * grain, scaling_shift)
        srshl           v17.8h,  v17.8h,  v29.8h

        uaddw           v16.8h,  v16.8h,  v6.8b   // *src + noise
        uaddw2          v17.8h,  v17.8h,  v6.16b

        sqxtun          v0.8b,   v16.8h
        sqxtun2         v0.16b,  v17.8h

        umax            v0.16b,  v0.16b,  v30.16b
        umin            v0.16b,  v0.16b,  v31.16b

.if \oy
        mov             v16.16b, v25.16b
.endif
        subs            w9,  w9,  #1
.if \oy
        mov             v25.16b, v26.16b
        mov             v26.16b, v16.16b
.endif
        st1             {v0.16b},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w12, #0
        mov             w9,  w12               // restore actual remaining h
        b.gt            L(fguv_loop_sx1_csfl\csfl\()_\ox\()0)
.endif

        b               9f
.endm
        fguv_loop_sx1   0, 0, 0
        fguv_loop_sx1   0, 0, 1
        fguv_loop_sx1   0, 1, 0
        fguv_loop_sx1   0, 1, 1
        fguv_loop_sx1   1, 0, 0
        fguv_loop_sx1   1, 0, 1
        fguv_loop_sx1   1, 1, 0
        fguv_loop_sx1   1, 1, 1

9:
        ldr             d8,       [sp, #16]
        ldr             x30,      [sp], #32
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

jumptable fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl0_00) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl0_01) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl0_10) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl0_11) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl1_00) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl1_01) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl1_10) - fguv_loop_sx1_tbl
        .word L(fguv_loop_sx1_csfl1_11) - fguv_loop_sx1_tbl
endjumptable
