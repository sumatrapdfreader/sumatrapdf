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

.macro store_grain_row r0, r1, r2, r3, r4, r5
        st1             {\r0\().16b,\r1\().16b}, [x0], #32
        st1             {\r2\().16b,\r3\().16b}, [x0], #32
        st1             {\r4\().16b},  [x0], #16
        st1             {\r5\().h}[0], [x0], #2
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
        ret
endfunc

.macro get_grain_2 dst
        bl              get_grain_2_neon
.ifnc \dst, v0
        mov             \dst\().8b, v0.8b
.endif
.endm

function get_grain_4_neon
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
        add             x15, x3,  x15, lsl #1
        ld1             {v0.h}[2], [x14]
        ld1             {v0.h}[3], [x15]
        srshl           v0.4h,   v0.4h,   v31.4h
        ret
endfunc

.macro get_grain_4 dst
        bl              get_grain_4_neon
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
        ext             v0.16b,  v0.16b,  v0.16b,  #2
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
        add             w12, w12, w10             // 1 << (4 - bitdepth_min_8 + grain_scale_shift - 1)
        asr             w14, w14, w7              // >> ar_coeff_shift
        asr             w12, w12, w9              // >> (4 - bitdepth_min_8 + grain_scale_shift)
        add             w14, w14, w12
        cmp             w14, w5
        csel            w14, w14, w5,  le
        cmp             w14, w6
        csel            w14, w14, w6,  ge
        subs            w15, w15, #1
        ext             v1.16b,  v1.16b,  v1.16b,  #4
        ins             v0.h[7], w14
        b.gt            1b
        ret
endfunc
.endm

output_lag 1
output_lag 2
output_lag 3


function sum_lag1_above_neon
        sub             x12, x0,  #1*GRAIN_WIDTH*2 - 16
        ld1             {v18.8h}, [x12] // load top right

        ext             v0.16b,  v16.16b, v17.16b, #14 // top left, top mid
        ext             v1.16b,  v17.16b, v18.16b, #2  // top mid, top right

        smull           v4.4s,   v17.4h,  v28.4h
        smlal           v4.4s,   v0.4h,   v27.4h
        smlal           v4.4s,   v1.4h,   v29.4h
        smull2          v5.4s,   v17.8h,  v28.8h
        smlal2          v5.4s,   v0.8h,   v27.8h
        smlal2          v5.4s,   v1.8h,   v29.8h

        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b

        ret
endfunc

.macro sum_lag_n_body lag, type, uv_layout, edge, elems, uv_coeff
        bl              sum_\lag\()_above_neon
.ifc \type, uv_420
        add             x12, x19, #GRAIN_WIDTH*2
        ld1             {v22.8h, v23.8h}, [x19], #32
        ld1             {v24.8h, v25.8h}, [x12]
        addp            v22.8h,  v22.8h,  v23.8h
        addp            v23.8h,  v24.8h,  v25.8h
        add             v22.8h,  v22.8h,  v23.8h
        srshr           v0.8h,   v22.8h,  #2
.endif
.ifc \type, uv_422
        ld1             {v22.8h, v23.8h}, [x19], #32
        addp            v22.8h,  v22.8h,  v23.8h
        srshr           v0.8h,   v22.8h,  #1
.endif
.ifc \type, uv_444
        ld1             {v0.8h}, [x19], #16
.endif
.if \uv_layout
.ifnb \uv_coeff
        dup             v1.8b,   \uv_coeff
        sxtl            v1.8h,   v1.8b
        smlal           v4.4s,   v0.4h,   v1.4h
        smlal2          v5.4s,   v0.8h,   v1.8h
.else
        smlal           v4.4s,   v0.4h,   v30.4h
        smlal2          v5.4s,   v0.8h,   v30.8h
.endif
.endif
.if \uv_layout && \elems == 8
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 444 && \elems == 7
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 422 && \elems == 1
        b               sum_\lag\()_uv_420_\edge\()_start
.else
sum_\lag\()_\type\()_\edge\()_start:
.if \elems > 4
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
        ext             v4.16b,  v4.16b,  v4.16b,  #12
.ifc \lag, lag3
        smov            w17, v0.h[5]
.endif
.ifnc \lag, lag1
        smov            w16, v0.h[6]
.endif
        smov            w14, v0.h[7]

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
.ifc \edge, right
        mov             w15, #3
        bl              output_\lag\()_neon
        read_shift_rand x15, 11
        add             x15, x3,  x15, lsl #1
        ld1             {v1.h}[0], [x15]
        srshl           v1.4h,   v1.4h,   v31.4h
        ext             v0.16b,  v0.16b,  v1.16b,  #2
.else
        mov             w15, #4
        bl              output_\lag\()_neon
.endif
.else
        // elems == 1
        increment_seed  4, shift=0
        mov             v1.16b,  v4.16b
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
        ext             v0.16b,  v0.16b,  v1.16b,  #14
.endif
        st1             {v0.8h}, [x0], #16
        ldr             x30,     [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
.endif
.endm

.macro sum_lag1_func type, uv_layout, edge, elems=8
function sum_\type\()_lag1_\edge\()_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
.ifc \edge, left
        sub             x12, x0,  #1*GRAIN_WIDTH*2
        ld1             {v17.8h}, [x12] // load the previous block right above
.endif
        sum_lag_n_body  lag1, \type, \uv_layout, \edge, \elems
endfunc
.endm

sum_lag1_func y,      0,   left
sum_lag1_func y,      0,   mid
sum_lag1_func y,      0,   right, 7
sum_lag1_func uv_444, 444, left
sum_lag1_func uv_444, 444, mid
sum_lag1_func uv_444, 444, right, 7
sum_lag1_func uv_422, 422, left
sum_lag1_func uv_422, 422, mid
sum_lag1_func uv_422, 422, right, 1
sum_lag1_func uv_420, 420, left
sum_lag1_func uv_420, 420, mid
sum_lag1_func uv_420, 420, right, 1


function sum_lag2_above_neon
        sub             x12, x0,  #2*GRAIN_WIDTH*2 - 16
        sub             x13, x0,  #1*GRAIN_WIDTH*2 - 16
        ld1             {v18.8h}, [x12] // load top right
        ld1             {v21.8h}, [x13]

        dup             v26.8b,  v30.b[0]
        ext             v22.16b, v16.16b, v17.16b, #12 // top left, top mid
        dup             v27.8b,  v30.b[1]
        ext             v23.16b, v16.16b, v17.16b, #14
        sxtl            v26.8h,  v26.8b
        dup             v28.8b,  v30.b[3]
        ext             v0.16b,  v17.16b, v18.16b, #2  // top mid, top right
        sxtl            v27.8h,  v27.8b
        dup             v29.8b,  v30.b[4]
        ext             v1.16b,  v17.16b, v18.16b, #4
        sxtl            v28.8h,  v28.8b
        sxtl            v29.8h,  v29.8b

        smull           v4.4s,   v22.4h,  v26.4h
        smlal           v4.4s,   v23.4h,  v27.4h
        smlal           v4.4s,   v0.4h,   v28.4h
        smlal           v4.4s,   v1.4h,   v29.4h
        smull2          v5.4s,   v22.8h,  v26.8h
        smlal2          v5.4s,   v23.8h,  v27.8h
        smlal2          v5.4s,   v0.8h,   v28.8h
        smlal2          v5.4s,   v1.8h,   v29.8h

        dup             v26.16b, v30.b[5]
        ext             v22.16b, v19.16b, v20.16b, #12 // top left, top mid
        dup             v27.16b, v30.b[6]
        ext             v23.16b, v19.16b, v20.16b, #14
        sxtl            v26.8h,  v26.8b
        dup             v28.16b, v30.b[8]
        ext             v0.16b,  v20.16b, v21.16b, #2  // top mid, top right
        sxtl            v27.8h,  v27.8b
        dup             v29.16b, v30.b[9]
        ext             v1.16b,  v20.16b, v21.16b, #4
        sxtl            v28.8h,  v28.8b
        sxtl            v29.8h,  v29.8b

        smlal           v4.4s,   v22.4h,  v26.4h
        smlal           v4.4s,   v23.4h,  v27.4h
        smlal           v4.4s,   v0.4h,   v28.4h
        smlal           v4.4s,   v1.4h,   v29.4h
        smlal2          v5.4s,   v22.8h,  v26.8h
        smlal2          v5.4s,   v23.8h,  v27.8h
        smlal2          v5.4s,   v0.8h,   v28.8h
        smlal2          v5.4s,   v1.8h,   v29.8h

        dup             v26.16b, v30.b[2]
        dup             v27.16b, v30.b[7]
        sxtl            v26.8h,  v26.8b
        sxtl            v27.8h,  v27.8b

        smlal           v4.4s,   v17.4h,  v26.4h
        smlal           v4.4s,   v20.4h,  v27.4h
        smlal2          v5.4s,   v17.8h,  v26.8h
        smlal2          v5.4s,   v20.8h,  v27.8h
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b

        mov             v19.16b, v20.16b
        mov             v20.16b, v21.16b
        ret
endfunc

.macro sum_lag2_func type, uv_layout, edge, elems=8
function sum_\type\()_lag2_\edge\()_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
.ifc \edge, left
        sub             x12, x0,  #2*GRAIN_WIDTH*2
        sub             x13, x0,  #1*GRAIN_WIDTH*2
        ld1             {v17.8h}, [x12] // load the previous block right above
        ld1             {v20.8h}, [x13]
.endif
        sum_lag_n_body  lag2, \type, \uv_layout, \edge, \elems, v30.b[12]
endfunc
.endm

sum_lag2_func y,      0,   left
sum_lag2_func y,      0,   mid
sum_lag2_func y,      0,   right, 7
sum_lag2_func uv_444, 444, left
sum_lag2_func uv_444, 444, mid
sum_lag2_func uv_444, 444, right, 7
sum_lag2_func uv_422, 422, left
sum_lag2_func uv_422, 422, mid
sum_lag2_func uv_422, 422, right, 1
sum_lag2_func uv_420, 420, left
sum_lag2_func uv_420, 420, mid
sum_lag2_func uv_420, 420, right, 1


function sum_lag3_above_neon
        sub             x11, x0,  #3*GRAIN_WIDTH*2 - 16
        sub             x12, x0,  #2*GRAIN_WIDTH*2 - 16
        sub             x13, x0,  #1*GRAIN_WIDTH*2 - 16
        ld1             {v15.8h}, [x11] // load top right
        ld1             {v18.8h}, [x12]
        ld1             {v21.8h}, [x13]

        dup             v22.8b,  v29.b[0]
        ext             v8.16b,  v13.16b, v14.16b, #10 // top left, top mid
        dup             v23.8b,  v29.b[1]
        ext             v9.16b,  v13.16b, v14.16b, #12
        sxtl            v22.8h,  v22.8b
        dup             v24.8b,  v29.b[2]
        sxtl            v23.8h,  v23.8b
        dup             v25.8b,  v29.b[3]
        ext             v10.16b, v13.16b, v14.16b, #14
        sxtl            v24.8h,  v24.8b
        dup             v26.8b,  v29.b[4]
        ext             v11.16b, v14.16b, v15.16b, #2  // top mid, top right
        sxtl            v25.8h,  v25.8b
        dup             v27.8b,  v29.b[5]
        ext             v12.16b, v14.16b, v15.16b, #4
        sxtl            v26.8h,  v26.8b
        dup             v28.8b,  v29.b[6]
        ext             v13.16b, v14.16b, v15.16b, #6
        sxtl            v27.8h,  v27.8b
        sxtl            v28.8h,  v28.8b

        smull           v4.4s,   v8.4h,   v22.4h
        smlal           v4.4s,   v9.4h,   v23.4h
        smlal           v4.4s,   v10.4h,  v24.4h
        smlal           v4.4s,   v11.4h,  v26.4h
        smlal           v4.4s,   v12.4h,  v27.4h
        smlal           v4.4s,   v13.4h,  v28.4h
        smlal           v4.4s,   v14.4h,  v25.4h
        smull2          v5.4s,   v8.8h,   v22.8h
        smlal2          v5.4s,   v9.8h,   v23.8h
        smlal2          v5.4s,   v10.8h,  v24.8h
        smlal2          v5.4s,   v11.8h,  v26.8h
        smlal2          v5.4s,   v12.8h,  v27.8h
        smlal2          v5.4s,   v13.8h,  v28.8h
        smlal2          v5.4s,   v14.8h,  v25.8h

        dup             v22.8b,  v29.b[7]
        ext             v8.16b,  v16.16b, v17.16b, #10 // top left, top mid
        dup             v23.8b,  v29.b[8]
        ext             v9.16b,  v16.16b, v17.16b, #12
        sxtl            v22.8h,  v22.8b
        dup             v24.8b,  v29.b[9]
        sxtl            v23.8h,  v23.8b
        dup             v25.8b,  v29.b[10]
        ext             v10.16b, v16.16b, v17.16b, #14
        sxtl            v24.8h,  v24.8b
        dup             v26.8b,  v29.b[11]
        ext             v11.16b, v17.16b, v18.16b, #2  // top mid, top right
        sxtl            v25.8h,  v25.8b
        dup             v27.8b,  v29.b[12]
        ext             v12.16b, v17.16b, v18.16b, #4
        sxtl            v26.8h,  v26.8b
        dup             v28.8b,  v29.b[13]
        ext             v13.16b, v17.16b, v18.16b, #6
        sxtl            v27.8h,  v27.8b
        sxtl            v28.8h,  v28.8b

        smlal           v4.4s,   v8.4h,   v22.4h
        smlal           v4.4s,   v9.4h,   v23.4h
        smlal           v4.4s,   v10.4h,  v24.4h
        smlal           v4.4s,   v11.4h,  v26.4h
        smlal           v4.4s,   v12.4h,  v27.4h
        smlal           v4.4s,   v13.4h,  v28.4h
        smlal           v4.4s,   v17.4h,  v25.4h
        smlal2          v5.4s,   v8.8h,   v22.8h
        smlal2          v5.4s,   v9.8h,   v23.8h
        smlal2          v5.4s,   v10.8h,  v24.8h
        smlal2          v5.4s,   v11.8h,  v26.8h
        smlal2          v5.4s,   v12.8h,  v27.8h
        smlal2          v5.4s,   v13.8h,  v28.8h
        smlal2          v5.4s,   v17.8h,  v25.8h

        dup             v22.8b,  v29.b[14]
        ext             v8.16b,  v19.16b, v20.16b, #10 // top left, top mid
        dup             v23.8b,  v29.b[15]
        ext             v9.16b,  v19.16b, v20.16b, #12
        sxtl            v22.8h,  v22.8b
        dup             v24.8b,  v30.b[0]
        sxtl            v23.8h,  v23.8b
        dup             v25.8b,  v30.b[1]
        ext             v10.16b, v19.16b, v20.16b, #14
        sxtl            v24.8h,  v24.8b
        dup             v26.8b,  v30.b[2]
        ext             v11.16b, v20.16b, v21.16b, #2  // top mid, top right
        sxtl            v25.8h,  v25.8b
        dup             v27.8b,  v30.b[3]
        ext             v12.16b, v20.16b, v21.16b, #4
        sxtl            v26.8h,  v26.8b
        dup             v28.8b,  v30.b[4]
        ext             v13.16b, v20.16b, v21.16b, #6
        sxtl            v27.8h,  v27.8b
        sxtl            v28.8h,  v28.8b

        smlal           v4.4s,   v8.4h,   v22.4h
        smlal           v4.4s,   v9.4h,   v23.4h
        smlal           v4.4s,   v10.4h,  v24.4h
        smlal           v4.4s,   v11.4h,  v26.4h
        smlal           v4.4s,   v12.4h,  v27.4h
        smlal           v4.4s,   v13.4h,  v28.4h
        smlal           v4.4s,   v20.4h,  v25.4h
        mov             v16.16b, v17.16b
        mov             v17.16b, v18.16b
        smlal2          v5.4s,   v8.8h,   v22.8h
        smlal2          v5.4s,   v9.8h,   v23.8h
        smlal2          v5.4s,   v10.8h,  v24.8h
        smlal2          v5.4s,   v11.8h,  v26.8h
        smlal2          v5.4s,   v12.8h,  v27.8h
        smlal2          v5.4s,   v13.8h,  v28.8h
        smlal2          v5.4s,   v20.8h,  v25.8h

        mov             v13.16b, v14.16b
        mov             v14.16b, v15.16b

        mov             v19.16b, v20.16b
        mov             v20.16b, v21.16b
        ret
endfunc

.macro sum_lag3_func type, uv_layout, edge, elems=8
function sum_\type\()_lag3_\edge\()_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
.ifc \edge, left
        sub             x11, x0,  #3*GRAIN_WIDTH*2
        sub             x12, x0,  #2*GRAIN_WIDTH*2
        sub             x13, x0,  #1*GRAIN_WIDTH*2
        ld1             {v14.8h}, [x11] // load the previous block right above
        ld1             {v17.8h}, [x12]
        ld1             {v20.8h}, [x13]
.endif
        sum_lag_n_body  lag3, \type, \uv_layout, \edge, \elems, v30.b[8]
endfunc
.endm

sum_lag3_func y,      0,   left
sum_lag3_func y,      0,   mid
sum_lag3_func y,      0,   right, 7
sum_lag3_func uv_444, 444, left
sum_lag3_func uv_444, 444, mid
sum_lag3_func uv_444, 444, right, 7
sum_lag3_func uv_422, 422, left
sum_lag3_func uv_422, 422, mid
sum_lag3_func uv_422, 422, right, 1
sum_lag3_func uv_420, 420, left
sum_lag3_func uv_420, 420, mid
sum_lag3_func uv_420, 420, right, 1

function generate_grain_rows_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
1:
        mov             w16, #80
2:
        bl              get_gaussian_neon
        srshl           v0.8h,   v0.8h,   v31.8h
        subs            w16, w16, #8
        st1             {v0.8h}, [x0], #16
        b.gt            2b
        get_grain_2     v0
        subs            w1,  w1,  #1
        st1             {v0.s}[0], [x0], #4
        b.gt            1b
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function generate_grain_rows_44_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
1:
        mov             w16, #40
2:
        bl              get_gaussian_neon
        srshl           v0.8h,   v0.8h,   v31.8h
        subs            w16, w16, #8
        st1             {v0.8h}, [x0], #16
        b.gt            2b
        get_grain_4     v0
        subs            w1,  w1,  #1
        st1             {v0.4h}, [x0]
        add             x0,  x0,  #GRAIN_WIDTH*2-80
        b.gt            1b
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function gen_grain_uv_444_lag0_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        ld1             {v4.8h}, [x19], #16
gen_grain_uv_lag0_8_start:
        bl              get_gaussian_neon
        srshl           v0.8h,   v0.8h,   v31.8h
gen_grain_uv_lag0_8_add:
        and             v4.16b,  v4.16b,  v1.16b
        smull           v2.4s,   v4.4h,   v27.4h
        smull2          v3.4s,   v4.8h,   v27.8h
        srshl           v2.4s,   v2.4s,   v28.4s
        srshl           v3.4s,   v3.4s,   v28.4s
        sqxtn           v2.4h,   v2.4s
        sqxtn2          v2.8h,   v3.4s
        sqadd           v2.8h,   v2.8h,   v0.8h
        smin            v2.8h,   v2.8h,   v25.8h
        smax            v2.8h,   v2.8h,   v26.8h
        st1             {v2.8h}, [x0], #16
        ldr             x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret
endfunc

function gen_grain_uv_420_lag0_8_neon
        AARCH64_SIGN_LINK_REGISTER
        add             x12, x19, #GRAIN_WIDTH*2
        str             x30, [sp, #-16]!
        ld1             {v16.8h, v17.8h}, [x19], #32
        ld1             {v18.8h, v19.8h}, [x12]
        addp            v16.8h,  v16.8h,  v17.8h
        addp            v17.8h,  v18.8h,  v19.8h
        add             v16.8h,  v16.8h,  v17.8h
        srshr           v4.8h,   v16.8h,  #2
        b               gen_grain_uv_lag0_8_start
endfunc

function gen_grain_uv_422_lag0_8_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        ld1             {v16.8h, v17.8h}, [x19], #32
        addp            v16.8h,  v16.8h,  v17.8h
        srshr           v4.8h,   v16.8h,  #1
        b               gen_grain_uv_lag0_8_start
endfunc

function gen_grain_uv_420_lag0_4_neon
        add             x12, x19, #GRAIN_WIDTH*2
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        ld1             {v16.4h, v17.4h}, [x19]
        ld1             {v18.4h, v19.4h}, [x12]
        add             x19,  x19,  #32
        addp            v16.4h,  v16.4h,  v17.4h
        addp            v17.4h,  v18.4h,  v19.4h
        add             v16.4h,  v16.4h,  v17.4h
        srshr           v4.4h,   v16.4h,  #2
        get_grain_4     v0
        b               gen_grain_uv_lag0_8_add
endfunc

function gen_grain_uv_422_lag0_4_neon
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-16]!
        ld1             {v16.4h, v17.4h}, [x19]
        add             x19,  x19,  #32
        addp            v16.4h,  v16.4h,  v17.4h
        srshr           v4.4h,   v16.4h,  #1
        get_grain_4     v0
        b               gen_grain_uv_lag0_8_add
endfunc

.macro gen_grain_82 type
function generate_grain_\type\()_16bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x30, x19, [sp, #-96]!

.ifc \type, uv_444
        mov             w13, w3
        mov             w14, #28
        add             x19, x1,  #3*GRAIN_WIDTH*2
        mov             x1,  x2
        mul             w13, w13, w14
        clz             w15, w4
.else
        clz             w15, w2
.endif
        movrel          x3,  X(gaussian_sequence)
        sub             w15, w15, #24 // -bitdepth_min_8
        ldr             w2,  [x1, #FGD_SEED]
        ldr             w9,  [x1, #FGD_GRAIN_SCALE_SHIFT]
.ifc \type, y
        add             x4,  x1,  #FGD_AR_COEFFS_Y
.else
        add             x4,  x1,  #FGD_AR_COEFFS_UV
.endif
        add             w9,  w9,  w15 // grain_scale_shift - bitdepth_min_8
        movrel          x16, gen_grain_\type\()_tbl
        ldr             w17, [x1, #FGD_AR_COEFF_LAG]
        add             w9,  w9,  #4
        ldrsw           x17, [x16, w17, uxtw #2]
        dup             v31.8h,  w9    // 4 - bitdepth_min_8 + data->grain_scale_shift
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
        neg             w15, w15            // bitdepth_min_8
        mov             w8,  #1
        mov             w10, #1
        lsl             w8,  w8,  w7        // 1 << ar_coeff_shift
        lsl             w10, w10, w9        // 1 << (4 + data->grain_scale_shift)
        lsr             w8,  w8,  #1        // 1 << (ar_coeff_shift - 1)
        lsr             w10, w10, #1        // 1 << (4 + data->grain_scale_shift - 1)
        mov             w5,  #128
        lsl             w5,  w5,  w15       //   128 << bitdepth_min_8
        neg             w6,  w5             // -(128 << bitpdeth_min_8)
        sub             w5,  w5,  #1        //  (128 << bitdepth_min_8) - 1

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
        dup             v28.4s,  w7
        ld1r            {v27.8b}, [x4]      // ar_coeffs_uv[0]
        movi            v0.16b,  #0
        movi            v1.16b,  #255
        dup             v25.8h,  w5
        dup             v26.8h,  w6
        ext             v29.16b, v0.16b,  v1.16b,  #10
        ext             v30.16b, v1.16b,  v0.16b,  #2
        neg             v28.4s,  v28.4s
        sxtl            v27.8h,  v27.8b

        mov             w1,  #3
        bl              generate_grain_rows_neon
        mov             w1,  #GRAIN_HEIGHT-3
1:
        mov             v1.16b,  v29.16b
        bl              gen_grain_uv_444_lag0_neon // 8
        movi            v1.16b,  #255
        bl              gen_grain_uv_444_lag0_neon // 16
        bl              gen_grain_uv_444_lag0_neon // 24
        bl              gen_grain_uv_444_lag0_neon // 32
        bl              gen_grain_uv_444_lag0_neon // 40
        bl              gen_grain_uv_444_lag0_neon // 48
        bl              gen_grain_uv_444_lag0_neon // 56
        bl              gen_grain_uv_444_lag0_neon // 64
        bl              gen_grain_uv_444_lag0_neon // 72
        mov             v1.16b,  v30.16b
        bl              gen_grain_uv_444_lag0_neon // 80
        get_grain_2     v16
        subs            w1,  w1,  #1
        add             x19, x19, #4
        st1             {v16.s}[0], [x0], #4
        b.gt            1b
.endif
        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag1):
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v27.8b}, [x4], #1  // ar_coeffs_y[0]
        ld1r            {v28.8b}, [x4], #1  // ar_coeffs_y[1]
        ld1r            {v29.8b}, [x4]      // ar_coeffs_y[2]
.ifc \type, y
        ldrsb           w4,  [x4, #1]       // ar_coeffs_y[3]
.else
        add             x4,  x4,  #2
.endif

        mov             w1,  #3
.ifc \type, uv_444
        ld1r            {v30.8b}, [x4]      // ar_coeffs_uv[4]
        ldursb          w4,  [x4, #-1]      // ar_coeffs_uv[3]
.endif
        bl              generate_grain_rows_neon
        sxtl            v27.8h,  v27.8b
        sxtl            v28.8h,  v28.8b
        sxtl            v29.8h,  v29.8b
.ifc \type, uv_444
        sxtl            v30.8h,  v30.8b
.endif

        mov             w1,  #GRAIN_HEIGHT - 3
1:
        bl              sum_\type\()_lag1_left_neon  // 8
        bl              sum_\type\()_lag1_mid_neon   // 16
        bl              sum_\type\()_lag1_mid_neon   // 24
        bl              sum_\type\()_lag1_mid_neon   // 32
        bl              sum_\type\()_lag1_mid_neon   // 40
        bl              sum_\type\()_lag1_mid_neon   // 48
        bl              sum_\type\()_lag1_mid_neon   // 56
        bl              sum_\type\()_lag1_mid_neon   // 64
        bl              sum_\type\()_lag1_mid_neon   // 72
        bl              sum_\type\()_lag1_right_neon // 80
        get_grain_2     v16
        subs            w1,  w1,  #1
.ifc \type, uv_444
        add             x19, x19, #4
.endif
        st1             {v16.s}[0], [x0], #4
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
        bl              sum_\type\()_lag2_left_neon  // 8
        bl              sum_\type\()_lag2_mid_neon   // 16
        bl              sum_\type\()_lag2_mid_neon   // 24
        bl              sum_\type\()_lag2_mid_neon   // 32
        bl              sum_\type\()_lag2_mid_neon   // 40
        bl              sum_\type\()_lag2_mid_neon   // 48
        bl              sum_\type\()_lag2_mid_neon   // 56
        bl              sum_\type\()_lag2_mid_neon   // 64
        bl              sum_\type\()_lag2_mid_neon   // 72
        bl              sum_\type\()_lag2_right_neon // 80
        get_grain_2     v16
        subs            w1,  w1,  #1
.ifc \type, uv_444
        add             x19, x19, #4
.endif
        st1             {v16.s}[0], [x0], #4
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
        bl              sum_\type\()_lag3_left_neon  // 8
        bl              sum_\type\()_lag3_mid_neon   // 16
        bl              sum_\type\()_lag3_mid_neon   // 24
        bl              sum_\type\()_lag3_mid_neon   // 32
        bl              sum_\type\()_lag3_mid_neon   // 40
        bl              sum_\type\()_lag3_mid_neon   // 48
        bl              sum_\type\()_lag3_mid_neon   // 56
        bl              sum_\type\()_lag3_mid_neon   // 64
        bl              sum_\type\()_lag3_mid_neon   // 72
        bl              sum_\type\()_lag3_right_neon // 80
        get_grain_2     v16
        subs            w1,  w1,  #1
.ifc \type, uv_444
        add             x19, x19, #4
.endif
        st1             {v16.s}[0], [x0], #4
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
        add             \reg, \reg, #2*GRAIN_WIDTH*2-(6*32)
.else
        sub             \reg, \reg, #6*32-GRAIN_WIDTH*2
.endif
.endm

.macro gen_grain_44 type
function generate_grain_\type\()_16bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        stp             x30, x19, [sp, #-96]!

        mov             w13, w3
        mov             w14, #28
        add             x19, x1,  #(3*GRAIN_WIDTH-3)*2
        mov             x1,  x2
        mul             w13, w13, w14
        clz             w15, w4

        movrel          x3,  X(gaussian_sequence)
        sub             w15, w15, #24 // -bitdepth_min_8
        ldr             w2,  [x1, #FGD_SEED]
        ldr             w9,  [x1, #FGD_GRAIN_SCALE_SHIFT]
        add             x4,  x1,  #FGD_AR_COEFFS_UV
        add             w9,  w9,  w15 // grain_scale_shift - bitdepth_min_8
        movrel          x16, gen_grain_\type\()_tbl
        ldr             w17, [x1, #FGD_AR_COEFF_LAG]
        add             w9,  w9,  #4
        ldrsw           x17, [x16, w17, uxtw #2]
        dup             v31.8h,  w9    // 4 - bitdepth_min_8 + data->grain_scale_shift
        add             x16, x16, x17
        neg             v31.8h,  v31.8h

        cmp             w13, #0
        mov             w11, #0x49d8
        mov             w14, #0xb524
        add             x4,  x4,  w13, uxtw // Add offset to ar_coeffs_uv[1]
        csel            w11, w11, w14, ne

        ldr             w7,  [x1, #FGD_AR_COEFF_SHIFT]
        neg             w15, w15            // bitdepth_min_8
        mov             w8,  #1
        mov             w10, #1
        lsl             w8,  w8,  w7        // 1 << ar_coeff_shift
        lsl             w10, w10, w9        // 1 << (4 + data->grain_scale_shift)
        lsr             w8,  w8,  #1        // 1 << (ar_coeff_shift - 1)
        lsr             w10, w10, #1        // 1 << (4 + data->grain_scale_shift - 1)
        mov             w5,  #128
        lsl             w5,  w5,  w15       //   128 << bitdepth_min_8
        neg             w6,  w5             // -(128 << bitpdeth_min_8)
        sub             w5,  w5,  #1        //  (128 << bitdepth_min_8) - 1

        eor             w2,  w2,  w11

        br              x16

L(generate_grain_\type\()_lag0):
        AARCH64_VALID_JUMP_TARGET
        dup             v28.4s,  w7
        ld1r            {v27.8b}, [x4]      // ar_coeffs_uv[0]
        movi            v0.16b,  #0
        movi            v1.16b,  #255
        dup             v25.8h,  w5
        dup             v26.8h,  w6
        ext             v29.16b, v0.16b,  v1.16b,  #10
        ext             v30.16b, v1.16b,  v0.16b,  #14
        neg             v28.4s,  v28.4s
        sxtl            v27.8h,  v27.8b

        mov             w1,  #3
        bl              generate_grain_rows_44_neon
        set_height      w1,  \type
1:
        mov             v1.16b,  v29.16b
        bl              gen_grain_\type\()_lag0_8_neon // 8
        movi            v1.16b,  #255
        bl              gen_grain_\type\()_lag0_8_neon // 16
        bl              gen_grain_\type\()_lag0_8_neon // 24
        bl              gen_grain_\type\()_lag0_8_neon // 32
        bl              gen_grain_\type\()_lag0_8_neon // 40
        mov             v1.16b,  v30.16b
        bl              gen_grain_\type\()_lag0_4_neon // 44
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        add             x0,  x0,  #GRAIN_WIDTH*2-6*16
        b.gt            1b

        ldp             x30, x19, [sp], #96
        AARCH64_VALIDATE_LINK_REGISTER
        ret

L(generate_grain_\type\()_lag1):
        AARCH64_VALID_JUMP_TARGET
        ld1r            {v27.8b}, [x4], #1  // ar_coeffs_uv[0]
        ld1r            {v28.8b}, [x4], #1  // ar_coeffs_uv[1]
        ld1r            {v29.8b}, [x4]      // ar_coeffs_uv[2]
        add             x4,  x4,  #2

        mov             w1,  #3
        ld1r            {v30.8b}, [x4]      // ar_coeffs_u4[4]
        ldursb          w4,  [x4, #-1]      // ar_coeffs_uv[3]
        bl              generate_grain_rows_44_neon

        sxtl            v27.8h,  v27.8b
        sxtl            v28.8h,  v28.8b
        sxtl            v29.8h,  v29.8b
        sxtl            v30.8h,  v30.8b
        set_height      w1,  \type
1:
        bl              sum_\type\()_lag1_left_neon  // 8
        bl              sum_\type\()_lag1_mid_neon   // 16
        bl              sum_\type\()_lag1_mid_neon   // 24
        bl              sum_\type\()_lag1_mid_neon   // 32
        bl              sum_\type\()_lag1_mid_neon   // 40
        bl              sum_\type\()_lag1_right_neon // 44
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        add             x0,  x0,  #GRAIN_WIDTH*2-6*16
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
        bl              sum_\type\()_lag2_left_neon  // 8
        bl              sum_\type\()_lag2_mid_neon   // 16
        bl              sum_\type\()_lag2_mid_neon   // 24
        bl              sum_\type\()_lag2_mid_neon   // 32
        bl              sum_\type\()_lag2_mid_neon   // 40
        bl              sum_\type\()_lag2_right_neon // 44
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        add             x0,  x0,  #GRAIN_WIDTH*2-6*16
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
        bl              sum_\type\()_lag3_left_neon  // 8
        bl              sum_\type\()_lag3_mid_neon   // 16
        bl              sum_\type\()_lag3_mid_neon   // 24
        bl              sum_\type\()_lag3_mid_neon   // 32
        bl              sum_\type\()_lag3_mid_neon   // 40
        bl              sum_\type\()_lag3_right_neon // 44
        subs            w1,  w1,  #1
        increment_y_ptr x19, \type
        add             x0,  x0,  #GRAIN_WIDTH*2-6*16
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
        umov            w14, \src1[0]
        umov            w15, \src2[1]
        umov            w16, \src1[2]
        add             x14, x14, x3
        umov            w17, \src2[3]
        add             x15, x15, x3
        ld1             {\dst1}[0+\off], [x14]
        umov            w14, \src1[4]
        add             x16, x16, x3
        ld1             {\dst2}[1+\off], [x15]
        umov            w15, \src2[5]
        add             x17, x17, x3
        ld1             {\dst1}[2+\off], [x16]
        umov            w16, \src1[6]
        add             x14, x14, x3
        ld1             {\dst2}[3+\off], [x17]
        umov            w17, \src2[7]
        add             x15, x15, x3
        ld1             {\dst1}[4+\off], [x14]
        add             x16, x16, x3
        ld1             {\dst2}[5+\off], [x15]
        add             x17, x17, x3
        ld1             {\dst1}[6+\off], [x16]
        ld1             {\dst2}[7+\off], [x17]
.endm

.macro gather dst1, dst2, src1, src2, src3, src4
        gather_interleaved \dst1, \dst2, \src1, \src3, 0
        gather_interleaved \dst2, \dst1, \src3, \src1, 0
        gather_interleaved \dst1, \dst2, \src2, \src4, 8
        gather_interleaved \dst2, \dst1, \src4, \src2, 8
.endm

function gather32_neon
        gather          v6.b, v7.b, v0.h, v1.h, v2.h, v3.h
        ret
endfunc

function gather16_neon
        gather_interleaved v6.b, v7.b, v0.h, v1.h, 0
        gather_interleaved v7.b, v6.b, v1.h, v0.h, 0
        ins             v6.d[1], v7.d[0]
        ret
endfunc

const overlap_coeffs_0, align=4
        .short 27, 17, 0,  0
        .short 17, 27, 32, 32
endconst

const overlap_coeffs_1, align=4
        .short 23, 0,  0,  0
        .short 22, 32, 32, 32
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
        add             \dst, \dst, \offx, uxtw #1 // grain_lut += offx
.endm

// void dav1d_fgy_32x32_16bpc_neon(pixel *const dst, const pixel *const src,
//                                 const ptrdiff_t stride,
//                                 const uint8_t scaling[SCALING_SIZE],
//                                 const int scaling_shift,
//                                 const entry grain_lut[][GRAIN_WIDTH],
//                                 const int offsets[][2],
//                                 const int h, const ptrdiff_t clip,
//                                 const ptrdiff_t type,
//                                 const int bitdepth_max);
function fgy_32x32_16bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        str             x30, [sp, #-80]!
        stp             d8,  d9,  [sp, #16]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #48]
        str             d14,      [sp, #64]
        eor             w4,  w4,  #15          // 15 - scaling_shift
        ldr             w11, [x6, #8]          // offsets[1][0]
        ldr             w13, [x6, #4]          // offsets[0][1]
        ldr             w15, [x6, #12]         // offsets[1][1]
        ldr             w10, [sp, #96]         // bitdepth_max
        ldr             w6,  [x6]              // offsets[0][0]
        dup             v26.8h,  w10           // bitdepth_max
        clz             w10, w10
        ldr             w8,  [sp, #80]         // clip
        sub             w10, w10, #24          // -bitdepth_min_8
        mov             x9,  #GRAIN_WIDTH*2    // grain_lut stride
        neg             w10, w10               // bitdepth_min_8

        dup             v29.8h,  w4            // 15 - scaling_shift
        dup             v27.8h,  w10           // bitdepth_min_8

        movrel          x16, overlap_coeffs_0

        cbz             w8,  1f
        // clip
        movi            v30.8h,  #16
        movi            v31.8h,  #235
        sshl            v30.8h,  v30.8h,  v27.8h
        sshl            v31.8h,  v31.8h,  v27.8h
        b               2f
1:
        // no clip
        movi            v30.8h,  #0
        mov             v31.16b, v26.16b       // bitdepth_max
2:

        ushr            v26.8h,  v26.8h,  #1   // grain_max
        not             v25.16b, v26.16b       // grain_min

        ld1             {v27.4h, v28.4h}, [x16] // overlap_coeffs

        add             x5,  x5,  #18          // grain_lut += 9
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

        ldr             w11, [sp, #88]         // type
        movrel          x13, fgy_loop_tbl

        add             x4,  x12, #32*2        // grain_lut += FG_BLOCK_SIZE * bx
        add             x6,  x14, x9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by

        tst             w11, #1
        ldrsw           x11, [x13, w11, uxtw #2]

        add             x8,  x16, x9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             x8,  x8,  #32*2        // grain_lut += FG_BLOCK_SIZE * bx

        add             x11, x13, x11

        b.eq            1f
        // y overlap
        dup             v8.8h,   v27.h[0]
        dup             v9.8h,   v27.h[1]
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
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x1],  x2 // src
.if \ox
        ld1             {v20.4h},                         [x4],  x9 // grain_lut old
.endif
.if \oy
        ld1             {v21.8h, v22.8h, v23.8h, v24.8h}, [x6],  x9 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v14.4h},                         [x8],  x9 // grain_lut top old
.endif
        mvni            v4.8h,   #0xf0, lsl #8 // 0x0fff
        ld1             {v16.8h, v17.8h, v18.8h, v19.8h}, [x5],  x9 // grain_lut

        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        and             v0.16b,  v0.16b,  v4.16b
        and             v1.16b,  v1.16b,  v4.16b
        and             v2.16b,  v2.16b,  v4.16b
        and             v3.16b,  v3.16b,  v4.16b
        bl              gather32_neon

.if \ox
        smull           v20.4s,  v20.4h,  v27.4h
        smlal           v20.4s,  v16.4h,  v28.4h
.endif

.if \oy
.if \ox
        smull           v14.4s,  v14.4h,  v27.4h
        smlal           v14.4s,  v21.4h,  v28.4h
        sqrshrn         v20.4h,  v20.4s,  #5
        sqrshrn         v14.4h,  v14.4s,  #5
        smin            v20.4h,  v20.4h,  v26.4h
        smin            v14.4h,  v14.4h,  v26.4h
        smax            v20.4h,  v20.4h,  v25.4h
        smax            v14.4h,  v14.4h,  v25.4h
.endif

.if \ox
        smull           v10.4s,  v20.4h,  v9.4h
.else
        smull           v10.4s,  v16.4h,  v9.4h
.endif
        smull2          v11.4s,  v16.8h,  v9.8h
        smull           v12.4s,  v17.4h,  v9.4h
        smull2          v13.4s,  v17.8h,  v9.8h
        smull           v16.4s,  v18.4h,  v9.4h
        smull2          v17.4s,  v18.8h,  v9.8h
        smull           v18.4s,  v19.4h,  v9.4h
        smull2          v19.4s,  v19.8h,  v9.8h
.if \ox
        smlal           v10.4s,  v14.4h,  v8.4h
.else
        smlal           v10.4s,  v21.4h,  v8.4h
.endif
        smlal2          v11.4s,  v21.8h,  v8.8h
        smlal           v12.4s,  v22.4h,  v8.4h
        smlal2          v13.4s,  v22.8h,  v8.8h
        smlal           v16.4s,  v23.4h,  v8.4h
        smlal2          v17.4s,  v23.8h,  v8.8h
        smlal           v18.4s,  v24.4h,  v8.4h
        smlal2          v19.4s,  v24.8h,  v8.8h
        sqrshrn         v10.4h,  v10.4s,  #5
        sqrshrn2        v10.8h,  v11.4s,  #5
        sqrshrn         v11.4h,  v12.4s,  #5
        sqrshrn2        v11.8h,  v13.4s,  #5
        sqrshrn         v12.4h,  v16.4s,  #5
        sqrshrn2        v12.8h,  v17.4s,  #5
        sqrshrn         v13.4h,  v18.4s,  #5
        sqrshrn2        v13.8h,  v19.4s,  #5
        smin            v16.8h,  v10.8h,  v26.8h
        smin            v17.8h,  v11.8h,  v26.8h
        smin            v18.8h,  v12.8h,  v26.8h
        smin            v19.8h,  v13.8h,  v26.8h
        smax            v16.8h,  v16.8h,  v25.8h
        smax            v17.8h,  v17.8h,  v25.8h
        smax            v18.8h,  v18.8h,  v25.8h
        smax            v19.8h,  v19.8h,  v25.8h
.endif

        uxtl            v4.8h,   v6.8b            // scaling
.if \ox && !\oy
        sqrshrn         v20.4h,  v20.4s,  #5
.endif
        uxtl2           v5.8h,   v6.16b
.if \ox && !\oy
        smin            v20.4h,  v20.4h,  v26.4h
.endif
        uxtl            v6.8h,   v7.8b
.if \ox && !\oy
        smax            v20.4h,  v20.4h,  v25.4h
.endif
        uxtl2           v7.8h,   v7.16b
.if \ox && !\oy
        ins             v16.d[0], v20.d[0]
.endif
        ushl            v4.8h,   v4.8h,   v29.8h  // scaling << (15 - scaling_shift)
        ushl            v5.8h,   v5.8h,   v29.8h
        ushl            v6.8h,   v6.8h,   v29.8h
        ushl            v7.8h,   v7.8h,   v29.8h

        sqrdmulh        v20.8h,  v16.8h,  v4.8h   // round2((scaling << (15 - scaling_shift) * grain, 15)
        sqrdmulh        v21.8h,  v17.8h,  v5.8h
        sqrdmulh        v22.8h,  v18.8h,  v6.8h
        sqrdmulh        v23.8h,  v19.8h,  v7.8h

        usqadd          v0.8h,   v20.8h           // *src + noise
        usqadd          v1.8h,   v21.8h
        usqadd          v2.8h,   v22.8h
        usqadd          v3.8h,   v23.8h

        umax            v0.8h,   v0.8h,   v30.8h
        umax            v1.8h,   v1.8h,   v30.8h
        umax            v2.8h,   v2.8h,   v30.8h
        umax            v3.8h,   v3.8h,   v30.8h
        umin            v0.8h,   v0.8h,   v31.8h
        umin            v1.8h,   v1.8h,   v31.8h
        umin            v2.8h,   v2.8h,   v31.8h
        umin            v3.8h,   v3.8h,   v31.8h

        subs            w7,  w7,  #1
.if \oy
        dup             v8.8h,   v28.h[0]
        dup             v9.8h,   v28.h[1]
.endif
        st1             {v0.8h, v1.8h, v2.8h, v3.8h},  [x0], x2 // dst
        b.gt            1b

.if \oy
        cmp             w10, #2
        sub             w7,  w10, #2           // restore actual remaining h
        b.gt            L(loop_\ox\()0)
.endif
        ldr             d14,      [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldr             x30, [sp], #80
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

// void dav1d_fguv_32x32_420_16bpc_neon(pixel *const dst,
//                                      const pixel *const src,
//                                      const ptrdiff_t stride,
//                                      const uint8_t scaling[SCALING_SIZE],
//                                      const Dav1dFilmGrainData *const data,
//                                      const entry grain_lut[][GRAIN_WIDTH],
//                                      const pixel *const luma_row,
//                                      const ptrdiff_t luma_stride,
//                                      const int offsets[][2],
//                                      const ptrdiff_t h, const ptrdiff_t uv,
//                                      const ptrdiff_t is_id,
//                                      const ptrdiff_t type,
//                                      const int bitdepth_max);
.macro fguv layout, sx, sy
function fguv_32x32_\layout\()_16bpc_neon, export=1
        AARCH64_SIGN_LINK_REGISTER
        str             x30,      [sp, #-80]!
        stp             d8,  d9,  [sp, #16]
        stp             d10, d11, [sp, #32]
        stp             d12, d13, [sp, #48]
        stp             d14, d15, [sp, #64]

        ldp             x8,  x9,  [sp, #80]    // offsets, h
        ldp             x10, x11, [sp, #96]    // uv, is_id
        ldr             w16,      [sp, #120]   // bitdepth_max

        ldr             w13, [x4, #FGD_SCALING_SHIFT]
        ldr             w12, [x4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        dup             v23.8h,  w16           // bitdepth_max
        clz             w16, w16
        eor             w13, w13, #15          // 15 - scaling_shift
        sub             w16, w16, #24          // -bitdepth_min_8

        // !csfl
        add             x10, x4,  x10, lsl #2  // + 4*uv
        add             x14, x10, #FGD_UV_LUMA_MULT
        add             x15, x10, #FGD_UV_MULT
        add             x10, x10, #FGD_UV_OFFSET
        neg             w16, w16               // bitdepth_min_8
        ld1r            {v8.8h},  [x14]        // uv_luma_mult
        ld1r            {v24.8h}, [x10]        // uv_offset
        ld1r            {v9.8h},  [x15]        // uv_mult

        dup             v29.8h,  w13           // 15 - scaling_shift
        dup             v27.8h,  w16           // bitdepth_min_8

        cbz             w12, 1f
        // clip
        movi            v30.8h,  #16
        movi            v31.8h,  #240
        sshl            v30.8h,  v30.8h,  v27.8h
        sshl            v31.8h,  v31.8h,  v27.8h
        cbz             w11, 2f
        // is_id
        movi            v31.8h,  #235
        sshl            v31.8h,  v31.8h,  v27.8h
        b               2f
1:
        // no clip
        movi            v30.8h,  #0
        mov             v31.16b, v23.16b       // bitdepth_max
2:

        ushr            v15.8h,  v23.8h,  #1   // grain_max
        sshl            v24.8h,  v24.8h,  v27.8h // uv_offset << bitdepth_min_8
        not             v14.16b, v15.16b       // grain_min

        ldr             w12, [x8, #8]          // offsets[1][0]
        ldr             w14, [x8, #4]          // offsets[0][1]
        ldr             w16, [x8, #12]         // offsets[1][1]
        ldr             w8,  [x8]              // offsets[0][0]

        mov             x10, #GRAIN_WIDTH*2    // grain_lut stride

        add             x5,  x5,  #(2*(3 + (2 >> \sx)*3)) // grain_lut += 9 or 6
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

        add             x4,  x13, #2*(32 >> \sx)      // grain_lut += FG_BLOCK_SIZE * bx
        add             x8,  x15, x10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             x11, x17, x10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             x11, x11, #2*(32 >> \sx)      // grain_lut += FG_BLOCK_SIZE * bx

        ldr             w13, [sp, #112]        // type

        movrel          x16, overlap_coeffs_\sx
        movrel          x14, fguv_loop_sx\sx\()_tbl

        ld1             {v27.4h, v28.4h}, [x16] // overlap_coeffs
        tst             w13, #1
        ldrsw           x13, [x14, w13, uxtw #2]

        b.eq            1f
        // y overlap
        sub             w12, w9,  #(2 >> \sy)  // backup remaining h
        mov             w9,  #(2 >> \sy)

1:
        add             x13, x14, x13

.if \sy
        movi            v25.8h,  #23
        movi            v26.8h,  #22
.else
        movi            v25.8h,  #27
        movi            v26.8h,  #17
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
.if \ox
        ld1             {v4.4h}, [x4],  x10  // grain_lut old
.endif
.if \oy
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x8],  x10 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v5.4h}, [x11], x10  // grain_lut top old
.endif
        ld1             {v16.8h, v17.8h, v18.8h, v19.8h}, [x5],  x10 // grain_lut

.if \ox
        smull           v4.4s,   v4.4h,   v27.4h
        smlal           v4.4s,   v16.4h,  v28.4h
.endif

.if \oy
.if \ox
        smull           v5.4s,   v5.4h,   v27.4h
        smlal           v5.4s,   v0.4h,   v28.4h
        sqrshrn         v4.4h,   v4.4s,   #5
        sqrshrn         v5.4h,   v5.4s,   #5
        smin            v4.4h,   v4.4h,   v15.4h
        smin            v5.4h,   v5.4h,   v15.4h
        smax            v4.4h,   v4.4h,   v14.4h
        smax            v5.4h,   v5.4h,   v14.4h
        ins             v16.d[0], v4.d[0]
        ins             v0.d[0],  v5.d[0]
.endif

        smull           v6.4s,   v16.4h,  v26.4h
        smull2          v7.4s,   v16.8h,  v26.8h
        smull           v10.4s,  v17.4h,  v26.4h
        smull2          v11.4s,  v17.8h,  v26.8h
        smull           v16.4s,  v18.4h,  v26.4h
        smull2          v17.4s,  v18.8h,  v26.8h
        smull           v18.4s,  v19.4h,  v26.4h
        smull2          v19.4s,  v19.8h,  v26.8h
        smlal           v6.4s,   v0.4h,   v25.4h
        smlal2          v7.4s,   v0.8h,   v25.8h
        smlal           v10.4s,  v1.4h,   v25.4h
        smlal2          v11.4s,  v1.8h,   v25.8h
        smlal           v16.4s,  v2.4h,   v25.4h
        smlal2          v17.4s,  v2.8h,   v25.8h
        smlal           v18.4s,  v3.4h,   v25.4h
        smlal2          v19.4s,  v3.8h,   v25.8h
        sqrshrn         v6.4h,   v6.4s,   #5
        sqrshrn2        v6.8h,   v7.4s,   #5
        sqrshrn         v7.4h,   v10.4s,  #5
        sqrshrn2        v7.8h,   v11.4s,  #5
        sqrshrn         v10.4h,  v16.4s,  #5
        sqrshrn2        v10.8h,  v17.4s,  #5
        sqrshrn         v11.4h,  v18.4s,  #5
        sqrshrn2        v11.8h,  v19.4s,  #5
.endif

.if \ox && !\oy
        sqrshrn         v4.4h,   v4.4s,   #5
        smin            v4.4h,   v4.4h,   v15.4h
.endif
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x6],  x7 // luma
.if \oy
        smin            v16.8h,  v6.8h,   v15.8h
        smin            v17.8h,  v7.8h,   v15.8h
        smin            v18.8h,  v10.8h,  v15.8h
        smin            v19.8h,  v11.8h,  v15.8h
        smax            v16.8h,  v16.8h,  v14.8h
        smax            v17.8h,  v17.8h,  v14.8h
        smax            v18.8h,  v18.8h,  v14.8h
        smax            v19.8h,  v19.8h,  v14.8h
.endif

.if \ox && !\oy
        smax            v4.4h,   v4.4h,   v14.4h
.endif
        ld1             {v10.8h, v11.8h, v12.8h, v13.8h}, [x1],  x2 // src
.if \ox && !\oy
        ins             v16.d[0], v4.d[0]
.endif

.if !\csfl
        smull           v4.4s,   v0.4h,   v8.4h
        smull2          v5.4s,   v0.8h,   v8.8h
        smull           v6.4s,   v1.4h,   v8.4h
        smull2          v7.4s,   v1.8h,   v8.8h
        smull           v0.4s,   v2.4h,   v8.4h
        smull2          v1.4s,   v2.8h,   v8.8h
        smull           v2.4s,   v3.4h,   v8.4h
        smull2          v3.4s,   v3.8h,   v8.8h
        smlal           v4.4s,   v10.4h,  v9.4h
        smlal2          v5.4s,   v10.8h,  v9.8h
        smlal           v6.4s,   v11.4h,  v9.4h
        smlal2          v7.4s,   v11.8h,  v9.8h
        smlal           v0.4s,   v12.4h,  v9.4h
        smlal2          v1.4s,   v12.8h,  v9.8h
        smlal           v2.4s,   v13.4h,  v9.4h
        smlal2          v3.4s,   v13.8h,  v9.8h
        shrn            v4.4h,   v4.4s,   #6
        shrn2           v4.8h,   v5.4s,   #6
        shrn            v5.4h,   v6.4s,   #6
        shrn2           v5.8h,   v7.4s,   #6
        shrn            v6.4h,   v0.4s,   #6
        shrn2           v6.8h,   v1.4s,   #6
        shrn            v7.4h,   v2.4s,   #6
        shrn2           v7.8h,   v3.4s,   #6
        add             v0.8h,   v4.8h,   v24.8h
        add             v1.8h,   v5.8h,   v24.8h
        add             v2.8h,   v6.8h,   v24.8h
        add             v3.8h,   v7.8h,   v24.8h
        movi            v20.8h,  #0
        smin            v0.8h,   v0.8h,   v23.8h
        smin            v1.8h,   v1.8h,   v23.8h
        smin            v2.8h,   v2.8h,   v23.8h
        smin            v3.8h,   v3.8h,   v23.8h
        smax            v0.8h,   v0.8h,   v20.8h
        smax            v1.8h,   v1.8h,   v20.8h
        smax            v2.8h,   v2.8h,   v20.8h
        smax            v3.8h,   v3.8h,   v20.8h
.else
        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        and             v0.16b,  v0.16b,  v23.16b
        and             v1.16b,  v1.16b,  v23.16b
        and             v2.16b,  v2.16b,  v23.16b
        and             v3.16b,  v3.16b,  v23.16b
.endif

        bl              gather32_neon

        uxtl            v4.8h,   v6.8b            // scaling
        uxtl2           v5.8h,   v6.16b
        uxtl            v6.8h,   v7.8b
        uxtl2           v7.8h,   v7.16b

        ushl            v4.8h,   v4.8h,   v29.8h  // scaling << (15 - scaling_shift)
        ushl            v5.8h,   v5.8h,   v29.8h
        ushl            v6.8h,   v6.8h,   v29.8h
        ushl            v7.8h,   v7.8h,   v29.8h

        sqrdmulh        v16.8h,  v16.8h,  v4.8h   // round2((scaling << (15 - scaling_shift) * grain, 15)
        sqrdmulh        v17.8h,  v17.8h,  v5.8h
        sqrdmulh        v18.8h,  v18.8h,  v6.8h
        sqrdmulh        v19.8h,  v19.8h,  v7.8h

        usqadd          v10.8h,  v16.8h           // *src + noise
        usqadd          v11.8h,  v17.8h
        usqadd          v12.8h,  v18.8h
        usqadd          v13.8h,  v19.8h

        umax            v0.8h,   v10.8h,  v30.8h
        umax            v1.8h,   v11.8h,  v30.8h
        umax            v2.8h,   v12.8h,  v30.8h
        umax            v3.8h,   v13.8h,  v30.8h
        umin            v0.8h,   v0.8h,   v31.8h
        umin            v1.8h,   v1.8h,   v31.8h
        umin            v2.8h,   v2.8h,   v31.8h
        umin            v3.8h,   v3.8h,   v31.8h

        subs            w9,  w9,  #1
.if \oy
        dup             v25.8h,  v28.h[0]
        dup             v26.8h,  v28.h[1]
.endif
        st1             {v0.8h, v1.8h, v2.8h, v3.8h},  [x0], x2 // dst
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
        ldp             d14, d15, [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldr             x30,      [sp], #80
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
.if \ox
        ld1             {v18.4h}, [x4],  x10  // grain_lut old
.endif
.if \oy
        ld1             {v20.8h, v21.8h},  [x8],  x10 // grain_lut top
.endif
.if \ox && \oy
        ld1             {v19.4h}, [x11], x10  // grain_lut top old
.endif
        ld1             {v16.8h, v17.8h}, [x5],  x10 // grain_lut

.if \ox
        smull           v18.4s,  v18.4h,  v27.4h
        smlal           v18.4s,  v16.4h,  v28.4h
.endif

.if \oy
.if \ox
        smull           v19.4s,  v19.4h,  v27.4h
        smlal           v19.4s,  v20.4h,  v28.4h
        sqrshrn         v18.4h,  v18.4s,  #5
        sqrshrn         v19.4h,  v19.4s,  #5
        smin            v18.4h,  v18.4h,  v15.4h
        smin            v19.4h,  v19.4h,  v15.4h
        smax            v18.4h,  v18.4h,  v14.4h
        smax            v19.4h,  v19.4h,  v14.4h
        ins             v16.d[0], v18.d[0]
        ins             v20.d[0], v19.d[0]
.endif

        smull           v0.4s,   v16.4h,  v26.4h
        smull2          v1.4s,   v16.8h,  v26.8h
        smull           v2.4s,   v17.4h,  v26.4h
        smull2          v3.4s,   v17.8h,  v26.8h
        smlal           v0.4s,   v20.4h,  v25.4h
        smlal2          v1.4s,   v20.8h,  v25.8h
        smlal           v2.4s,   v21.4h,  v25.4h
        smlal2          v3.4s,   v21.8h,  v25.8h
        sqrshrn         v16.4h,  v0.4s,   #5
        sqrshrn2        v16.8h,  v1.4s,   #5
        sqrshrn         v17.4h,  v2.4s,   #5
        sqrshrn2        v17.8h,  v3.4s,   #5
.endif

.if \ox && !\oy
        sqrshrn         v18.4h,  v18.4s,  #5
        smin            v18.4h,  v18.4h,  v15.4h
.endif
        ld1             {v0.8h,  v1.8h,  v2.8h,  v3.8h},  [x6],  x7 // luma
.if \oy
        smin            v16.8h,  v16.8h,  v15.8h
        smin            v17.8h,  v17.8h,  v15.8h
        smax            v16.8h,  v16.8h,  v14.8h
        smax            v17.8h,  v17.8h,  v14.8h
.endif

.if \ox && !\oy
        smax            v18.4h,  v18.4h,  v14.4h
.endif
        ld1             {v10.8h, v11.8h},  [x1],  x2 // src
.if \ox && !\oy
        ins             v16.d[0], v18.d[0]
.endif
        addp            v0.8h,   v0.8h,   v1.8h
        addp            v1.8h,   v2.8h,   v3.8h
        urshr           v0.8h,   v0.8h,   #1
        urshr           v1.8h,   v1.8h,   #1
.if !\csfl
        smull           v2.4s,   v0.4h,   v8.4h
        smull2          v3.4s,   v0.8h,   v8.8h
        smull           v0.4s,   v1.4h,   v8.4h
        smull2          v1.4s,   v1.8h,   v8.8h
        smlal           v2.4s,   v10.4h,  v9.4h
        smlal2          v3.4s,   v10.8h,  v9.8h
        smlal           v0.4s,   v11.4h,  v9.4h
        smlal2          v1.4s,   v11.8h,  v9.8h
        shrn            v2.4h,   v2.4s,   #6
        shrn2           v2.8h,   v3.4s,   #6
        shrn            v3.4h,   v0.4s,   #6
        shrn2           v3.8h,   v1.4s,   #6
        add             v0.8h,   v2.8h,   v24.8h
        add             v1.8h,   v3.8h,   v24.8h
        movi            v2.8h,   #0
        smin            v0.8h,   v0.8h,   v23.8h
        smin            v1.8h,   v1.8h,   v23.8h
        smax            v0.8h,   v0.8h,   v2.8h
        smax            v1.8h,   v1.8h,   v2.8h
.else
        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        and             v0.16b,  v0.16b,  v23.16b
        and             v1.16b,  v1.16b,  v23.16b
.endif

        bl              gather16_neon

        uxtl            v4.8h,   v6.8b            // scaling
        uxtl2           v5.8h,   v6.16b

        ushl            v4.8h,   v4.8h,   v29.8h  // scaling << (15 - scaling_shift)
        ushl            v5.8h,   v5.8h,   v29.8h

        sqrdmulh        v16.8h,  v16.8h,  v4.8h   // round2((scaling << (15 - scaling_shift) * grain, 15)
        sqrdmulh        v17.8h,  v17.8h,  v5.8h

        usqadd          v10.8h,  v16.8h           // *src + noise
        usqadd          v11.8h,  v17.8h

        umax            v0.8h,   v10.8h,  v30.8h
        umax            v1.8h,   v11.8h,  v30.8h
        umin            v0.8h,   v0.8h,   v31.8h
        umin            v1.8h,   v1.8h,   v31.8h

.if \oy
        mov             v16.16b, v25.16b
.endif
        subs            w9,  w9,  #1
.if \oy
        mov             v25.16b, v26.16b
        mov             v26.16b, v16.16b
.endif
        st1             {v0.8h, v1.8h},  [x0], x2 // dst
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
        ldp             d14, d15, [sp, #64]
        ldp             d12, d13, [sp, #48]
        ldp             d10, d11, [sp, #32]
        ldp             d8,  d9,  [sp, #16]
        ldr             x30,      [sp], #80
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
