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
        lsr             r11, r2,  #3
        lsr             r12, r2,  #12
        lsr             lr,  r2,  #1
        eor             r11, r2,  r11                     // (r >> 0) ^ (r >> 3)
        eor             r12, r12, lr                      // (r >> 12) ^ (r >> 1)
        eor             r11, r11, r12                     // (r >> 0) ^ (r >> 3) ^ (r >> 12) ^ (r >> 1)
.if \shift
        lsr             r2,  r2,  #\steps
.endif
        and             r11, r11, #((1 << \steps) - 1)    // bit
.if \shift
        orr             r2,  r2,  r11, lsl #(16 - \steps) // *state
.else
        orr             r2,  r2,  r11, lsl #16            // *state
.endif
.endm

.macro read_rand dest, bits, age
        ubfx            \dest,  r2,   #16 - \bits - \age, #\bits
.endm

.macro read_shift_rand dest, bits
        ubfx            \dest,  r2,   #17 - \bits, #\bits
        lsr             r2,  r2,  #1
.endm

// special calling convention:
// r2 holds seed
// r3 holds dav1d_gaussian_sequence
// clobbers r11-r12
// returns in d0-d1
function get_gaussian_neon
        push            {r5-r6,lr}
        increment_seed  4
        read_rand       r5,  11,  3
        read_rand       r6,  11,  2
        add             r5,  r3,  r5,  lsl #1
        add             r6,  r3,  r6,  lsl #1
        vld1.16         {d0[0]}, [r5]
        read_rand       r5,  11,  1
        vld1.16         {d0[1]}, [r6]
        add             r5,  r3,  r5,  lsl #1
        read_rand       r6, 11,  0
        increment_seed  4
        add             r6,  r3,  r6,  lsl #1
        vld1.16         {d0[2]}, [r5]
        read_rand       r5,  11,  3
        vld1.16         {d0[3]}, [r6]
        add             r5,  r3,  r5,  lsl #1
        read_rand       r6,  11,  2
        vld1.16         {d1[0]}, [r5]
        add             r6,  r3,  r6,  lsl #1
        read_rand       r5,  11,  1
        vld1.16         {d1[1]}, [r6]
        read_rand       r6,  11,  0
        add             r5,  r3,  r5,  lsl #1
        add             r6,  r3,  r6,  lsl #1
        vld1.16         {d1[2]}, [r5]
        vld1.16         {d1[3]}, [r6]
        pop             {r5-r6,pc}
endfunc

.macro get_grain_row r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r0, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r1, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r2, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r3, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r4, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r5, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r6, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r7, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r8, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r9, q0
        increment_seed  2
        read_rand       r11, 11,  1
        read_rand       r12, 11,  0
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d0[0]}, [r11]
        vld1.16         {d0[1]}, [r12]
        vrshl.s16       d0,  d0,  d30
        vmovn.i16       \r10, q0
.endm

.macro store_grain_row r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10
        vst1.16         {\r0, \r1, \r2, \r3}, [r0]!
        vst1.16         {\r4, \r5, \r6, \r7}, [r0]!
        vst1.16         {\r8, \r9},           [r0]!
        vst1.16         {\r10[0]},            [r0]!
.endm

.macro get_grain_row_44 r0, r1, r2, r3, r4, r5
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r0, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r1, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r2, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r3, q0
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        vmovn.i16       \r4, q0
        increment_seed  4
        read_rand       r11, 11,  3
        read_rand       r12, 11,  2
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d0[]}, [r11]
        read_rand       r11, 11,  1
        vld1.16         {d0[1]}, [r12]
        add             r11, r3,  r11, lsl #1
        read_rand       r12, 11,  0
        vld1.16         {d0[2]}, [r11]
        add             r12, r3,  r12, lsl #1
        vld1.16         {d0[3]}, [r12]
        vrshl.s16       d0,  d0,  d30
        vmovn.i16       \r5, q0
.endm

.macro store_grain_row_44 r0, r1, r2, r3, r4, r5
        vst1.16         {\r0, \r1, \r2, \r3}, [r0]!
        vst1.16         {\r4, \r5},           [r0]
        add             r0,  r0,  #GRAIN_WIDTH-32
.endm

function get_grain_2_neon
        push            {r11,lr}
        increment_seed  2
        read_rand       r11, 11,  1
        read_rand       r12, 11,  0
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d0[0]}, [r11]
        vld1.16         {d0[1]}, [r12]
        vrshl.s16       d0,  d0,  d30
        vmovn.i16       d0,  q0
        pop             {r11,pc}
endfunc

.macro get_grain_2 dst
        bl              get_grain_2_neon
.ifnc \dst, d0
        vmov            \dst, d0
.endif
.endm

// r1 holds the number of entries to produce
// r6, r8 and r10 hold the previous output entries
// q0 holds the vector of produced entries
// q1 holds the input vector of sums from above
.macro output_lag n
function output_lag\n\()_neon
        push            {r0, lr}
.if \n == 1
        mov             lr,  #-128
.else
        mov             r0,  #1
        mov             lr,  #1
        sub             r7,  r7,  #1
        sub             r9,  r9,  #1
        lsl             r0,  r0,  r7
        lsl             lr,  lr,  r9
        add             r7,  r7,  #1
        add             r9,  r9,  #1
.endif
1:
        read_shift_rand r12, 11
        vmov.32         r11, d2[0]
        lsl             r12, r12, #1
        vext.8          q0,  q0,  q0,  #1
        ldrsh           r12, [r3, r12]
.if \n == 1
        mla             r11, r6,  r4,  r11        // sum (above) + *coeff * prev output
        add             r6,  r11, r8              // 1 << (ar_coeff_shift - 1)
        add             r12, r12, r10
        asr             r6,  r6,  r7              // >> ar_coeff_shift
        asr             r12, r12, r9              // >> (4 + grain_scale_shift)
        add             r6,  r6,  r12
        cmp             r6,  r5
.elseif \n == 2
        mla             r11, r8,  r4,  r11        // sum (above) + *coeff * prev output 1
        mla             r11, r6,  r10, r11        // += *coeff * prev output 2
        mov             r8,  r6
        add             r6,  r11, r0              // 1 << (ar_coeff_shift - 1)
        add             r12, r12, lr              // 1 << (4 + grain_scale_shift - 1)
        asr             r6,  r6,  r7              // >> ar_coeff_shift
        asr             r12, r12, r9              // >> (4 + grain_scale_shift)
        add             r6,  r6,  r12
        push            {lr}
        cmp             r6,  r5
        mov             lr,  #-128
.else
        push            {r1-r3}
        sbfx            r1,  r4,  #0,  #8
        sbfx            r2,  r4,  #8,  #8
        sbfx            r3,  r4,  #16, #8
        mla             r11, r10, r1,  r11        // sum (above) + *coeff * prev output 1
        mla             r11, r8,  r2,  r11        // sum (above) + *coeff * prev output 2
        mla             r11, r6,  r3,  r11        // += *coeff * prev output 3
        pop             {r1-r3}
        mov             r10, r8
        mov             r8,  r6

        add             r6,  r11, r0              // 1 << (ar_coeff_shift - 1)
        add             r12, r12, lr              // 1 << (4 + grain_scale_shift - 1)
        asr             r6,  r6,  r7              // >> ar_coeff_shift
        asr             r12, r12, r9              // >> (4 + grain_scale_shift)
        add             r6,  r6,  r12
        push            {lr}
        cmp             r6,  r5
        mov             lr,  #-128
.endif
        it              gt
        movgt           r6,  r5
        cmp             r6,  lr
        it              lt
        movlt           r6,  lr
.if \n >= 2
        pop             {lr}
.endif
        subs            r1,  r1,  #1
        vext.8          q1,  q1,  q1,  #4
        vmov.8          d1[7], r6
        bgt             1b
        pop             {r0, pc}
endfunc
.endm

output_lag 1
output_lag 2
output_lag 3


function sum_lag1_above_neon
        vmull.s8        q2,  d6,  d28
        vmull.s8        q3,  d7,  d28
        vmull.s8        q4,  d0,  d27
        vmull.s8        q5,  d1,  d27

        vaddl.s16       q0,  d4,  d8
        vaddl.s16       q2,  d5,  d9
        vaddl.s16       q4,  d6,  d10
        vaddl.s16       q5,  d7,  d11

        vmull.s8        q3,  d3,  d29
        vmull.s8        q1,  d2,  d29

        vaddw.s16       q4,  q4,  d6
        vaddw.s16       q5,  q5,  d7
        vaddw.s16       q3,  q2,  d3
        vaddw.s16       q2,  q0,  d2
        bx              lr
endfunc

.macro sum_lag_n_body lag, type, uv_layout, edge, elems, store, uv_coeff
.ifc \lag\()_\edge, lag3_left
        bl              sum_lag3_left_above_neon
.else
        bl              sum_\lag\()_above_neon
.endif
.ifc \type, uv_420
        vpush           {q6-q7}
        add             r12, r11, #GRAIN_WIDTH
        vld1.16         {q0, q1}, [r11]!
        vld1.16         {q6, q7}, [r12]!
        vpaddl.s8       q0,  q0
        vpaddl.s8       q1,  q1
        vpaddl.s8       q6,  q6
        vpaddl.s8       q7,  q7
        vadd.i16        q0,  q0,  q6
        vadd.i16        q1,  q1,  q7
        vpop            {q6-q7}
        vrshrn.s16      d0,  q0,  #2
        vrshrn.s16      d1,  q1,  #2
.endif
.ifc \type, uv_422
        vld1.8          {q0, q1}, [r11]!
        vpaddl.s8       q0,  q0
        vpaddl.s8       q1,  q1
        vrshrn.s16      d0,  q0,  #1
        vrshrn.s16      d1,  q1,  #1
.endif
.ifc \type, uv_444
        vld1.8          {q0}, [r11]!
.endif
.if \uv_layout
.ifnb \uv_coeff
        vdup.8          d13, \uv_coeff
.endif
        vmull.s8        q1,  d0,  d13
        vmull.s8        q0,  d1,  d13
        vaddw.s16       q2,  q2,  d2
        vaddw.s16       q3,  q3,  d3
        vaddw.s16       q4,  q4,  d0
        vaddw.s16       q5,  q5,  d1
.endif
.if \uv_layout && \elems == 16
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 444 && \elems == 15
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 422 && \elems == 9
        b               sum_\lag\()_uv_420_\edge\()_start
.else
sum_\lag\()_\type\()_\edge\()_start:
        push            {r11}
.ifc \edge, left
        increment_seed  4
        read_rand       r11, 11,  3
        read_rand       r12, 11,  2
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d1[1]}, [r11]
        read_rand       r11, 11,  1
        vld1.16         {d1[2]}, [r12]
        add             r11, r3,  r11, lsl #1
        vld1.16         {d1[3]}, [r11]
        lsl             r2,  r2,  #1             // shift back the state as if we'd done increment_seed with shift=0
        vrshl.s16       d1,  d1,  d30
        vmovn.i16       d1,  q0
        vext.8          q2,  q2,  q2,  #12
.ifc \lag, lag3
        vmov.s8         r10, d1[5]
.endif
.ifnc \lag, lag1
        vmov.s8         r8,  d1[6]
.endif
        vmov.s8         r6,  d1[7]

        vmov            q1,  q2
        mov             r1,  #1
        bl              output_\lag\()_neon
.else
        increment_seed  4, shift=0
        vmov            q1,  q2
        mov             r1,  #4
        bl              output_\lag\()_neon
.endif

        increment_seed  4, shift=0
        vmov            q1,  q3
        mov             r1,  #4
        bl              output_\lag\()_neon

        increment_seed  4, shift=0
        vmov            q1,  q4
.if \elems == 9
        mov             r1,  #1
        bl              output_\lag\()_neon
        lsr             r2,  r2,  #3

        read_rand       r11, 11,  2
        read_rand       r12, 11,  1
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d2[0]}, [r11]
        read_rand       r11, 11,  0
        vld1.16         {d2[1]}, [r12]
        add             r11, r3,  r11, lsl #1
        vld1.16         {d2[2]}, [r11]
        vrshl.s16       d2,  d2,  d30
        vmovn.i16       d2,  q1
        vext.8          q0,  q0,  q1,  #7
.else
        mov             r1,  #4
        bl              output_\lag\()_neon

        increment_seed  4, shift=0
        vmov            q1,  q5

.ifc \edge, right
        mov             r1,  #3
        bl              output_\lag\()_neon
        read_shift_rand r11, 11
        add             r11, r3,  r11, lsl #1
        vld1.16         {d2[0]}, [r11]
        vrshl.s16       d2,  d2,  d30
        vext.8          q0,  q0,  q1,  #1
.else
        mov             r1,  #4
        bl              output_\lag\()_neon
.endif
.endif
.if \store
        vst1.8          {q0}, [r0]!
.endif
        pop             {r11}
        pop             {r1, pc}
.endif
.endm

.macro sum_lag1_func type, uv_layout, edge, elems=16
function sum_\type\()_lag1_\edge\()_neon
        push            {r1, lr}
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
        vmov            q3,  \mid
        vext.8          q0,  \left, \mid,   #15
        vext.8          q1,  \mid,  \right, #1
        bl              sum_\type\()_lag1_\edge\()_neon
        vmov            \dst, q0
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
        push            {lr}
        sub             r12, r0,  #2*GRAIN_WIDTH - 16
        sub             lr,  r0,  #1*GRAIN_WIDTH - 16
        vld1.8          {q10}, [r12] // load top right
        vld1.8          {q13}, [lr]

        vext.8          q6,  q8,  q9,  #14 // top left, top mid
        vdup.8          d14, d28[0]
        vext.8          q8,  q8,  q9,  #15
        vdup.8          d15, d28[1]

        vmull.s8        q0,  d12, d14
        vmull.s8        q1,  d13, d14
        vmull.s8        q6,  d16, d15
        vmull.s8        q8,  d17, d15

        vaddl.s16       q2,  d0,  d12
        vaddl.s16       q3,  d1,  d13
        vaddl.s16       q4,  d2,  d16
        vaddl.s16       q5,  d3,  d17

        vext.8          q6,  q9,  q10, #1  // top mid, top right
        vdup.8          d14, d28[3]
        vext.8          q8,  q9,  q10, #2
        vdup.8          d15, d28[4]

        vmull.s8        q0,  d12, d14
        vmull.s8        q1,  d13, d14
        vmull.s8        q6,  d16, d15
        vmull.s8        q8,  d17, d15

        vaddl.s16       q7,  d0,  d12
        vaddl.s16       q0,  d1,  d13
        vaddl.s16       q6,  d2,  d16
        vaddl.s16       q1,  d3,  d17

        vadd.i32        q2,  q2,  q7
        vadd.i32        q3,  q3,  q0
        vadd.i32        q4,  q4,  q6
        vadd.i32        q5,  q5,  q1

        vext.8          q6,  q11, q12, #14 // top left, top mid
        vdup.8          d14, d28[5]
        vext.8          q8,  q11, q12, #15
        vdup.8          d15, d28[6]

        vmull.s8        q0,  d12, d14
        vmull.s8        q1,  d13, d14
        vmull.s8        q6,  d16, d15
        vmull.s8        q8,  d17, d15

        vaddl.s16       q7,  d0,  d12
        vaddl.s16       q0,  d1,  d13
        vaddl.s16       q6,  d2,  d16
        vaddl.s16       q1,  d3,  d17

        vadd.i32        q2,  q2,  q7
        vadd.i32        q3,  q3,  q0
        vadd.i32        q4,  q4,  q6
        vadd.i32        q5,  q5,  q1

        vext.8          q6,  q12, q13, #1  // top mid, top right
        vdup.8          d14, d29[0]
        vext.8          q8,  q12, q13, #2
        vdup.8          d15, d29[1]

        vmull.s8        q0,  d12, d14
        vmull.s8        q1,  d13, d14
        vmull.s8        q6,  d16, d15
        vmull.s8        q8,  d17, d15

        vaddl.s16       q7,  d0,  d12
        vaddl.s16       q0,  d1,  d13
        vaddl.s16       q6,  d2,  d16
        vaddl.s16       q1,  d3,  d17

        vadd.i32        q2,  q2,  q7
        vadd.i32        q3,  q3,  q0
        vadd.i32        q4,  q4,  q6
        vadd.i32        q5,  q5,  q1

        vdup.8          d14, d28[2]
        vdup.8          d15, d28[7]

        vmull.s8        q0,  d18, d14
        vmull.s8        q1,  d19, d14
        vmull.s8        q6,  d24, d15
        vmull.s8        q8,  d25, d15

        vaddl.s16       q7,  d0,  d12
        vaddl.s16       q0,  d1,  d13
        vaddl.s16       q6,  d2,  d16
        vaddl.s16       q1,  d3,  d17

        vmov            q8,  q9
        vmov            q9,  q10

        vadd.i32        q2,  q2,  q7
        vadd.i32        q3,  q3,  q0
        vadd.i32        q4,  q4,  q6
        vadd.i32        q5,  q5,  q1

        vmov            q11, q12
        vmov            q12, q13

        pop             {pc}
endfunc

.macro sum_lag2_func type, uv_layout, edge, elems=16
function sum_\type\()_lag2_\edge\()_neon
        push            {r1, lr}
.ifc \edge, left
        sub             r12, r0,  #2*GRAIN_WIDTH
        sub             lr,  r0,  #1*GRAIN_WIDTH
        vld1.8          {q9},  [r12] // load the previous block right above
        vld1.8          {q12}, [lr]
.endif
        sum_lag_n_body  lag2, \type, \uv_layout, \edge, \elems, store=1, uv_coeff=d29[4]
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


function sum_lag3_left_above_neon
        // A separate codepath for the left edge, to avoid reading outside
        // of the edge of the buffer.
        sub             r12, r0,  #3*GRAIN_WIDTH
        vld1.8          {q11, q12}, [r12]
        vext.8          q12, q11, q12, #13
        vext.8          q11, q11, q11, #13
        b               sum_lag3_above_start
endfunc

function sum_lag3_above_neon
        sub             r12, r0,  #3*GRAIN_WIDTH + 3
        vld1.8          {q11, q12}, [r12]

sum_lag3_above_start:
        vdup.8          d20, d26[0]
        vext.8          q9,  q11, q12, #1
        vdup.8          d21, d26[1]

        vmull.s8        q0,  d22, d20
        vmull.s8        q1,  d23, d20
        vmull.s8        q6,  d18, d21
        vmull.s8        q7,  d19, d21

        vext.8          q8,  q11, q12, #2
        vdup.8          d20, d26[2]
        vext.8          q9,  q11, q12, #3
        vdup.8          d21, d26[3]

        vaddl.s16       q2,  d0,  d12
        vaddl.s16       q3,  d1,  d13
        vaddl.s16       q4,  d2,  d14
        vaddl.s16       q5,  d3,  d15

        vmull.s8        q0,  d16, d20
        vmull.s8        q1,  d17, d20
        vmull.s8        q6,  d18, d21
        vmull.s8        q7,  d19, d21

        vaddl.s16       q8,  d0,  d12
        vaddl.s16       q9,  d1,  d13
        vaddl.s16       q0,  d2,  d14
        vaddl.s16       q1,  d3,  d15

        vext.8          q6,  q11, q12, #4
        vdup.8          d20, d26[4]
        vext.8          q7,  q11, q12, #5
        vdup.8          d21, d26[5]

        vadd.i32        q2,  q2,  q8
        vadd.i32        q3,  q3,  q9
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d12, d20
        vmull.s8        q1,  d13, d20
        vmull.s8        q8,  d14, d21
        vmull.s8        q9,  d15, d21

        sub             r12, r0,  #2*GRAIN_WIDTH + 3

        vaddl.s16       q6,  d0,  d16
        vaddl.s16       q7,  d1,  d17
        vaddl.s16       q0,  d2,  d18
        vaddl.s16       q1,  d3,  d19

        vext.8          q8,  q11, q12, #6
        vld1.8          {q11, q12}, [r12]
        vdup.8          d20, d26[6]
        vdup.8          d21, d26[7]

        vadd.i32        q2,  q2,  q6
        vadd.i32        q3,  q3,  q7
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d16, d20
        vmull.s8        q1,  d17, d20
        vmull.s8        q6,  d22, d21
        vmull.s8        q7,  d23, d21

        vaddl.s16       q8,  d0,  d12
        vaddl.s16       q9,  d1,  d13
        vaddl.s16       q0,  d2,  d14
        vaddl.s16       q1,  d3,  d15

        vext.8          q6,  q11, q12, #1
        vdup.8          d20, d27[0]
        vext.8          q7,  q11, q12, #2
        vdup.8          d21, d27[1]

        vadd.i32        q2,  q2,  q8
        vadd.i32        q3,  q3,  q9
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d12, d20
        vmull.s8        q1,  d13, d20
        vmull.s8        q8,  d14, d21
        vmull.s8        q9,  d15, d21

        vaddl.s16       q6,  d0,  d16
        vaddl.s16       q7,  d1,  d17
        vaddl.s16       q0,  d2,  d18
        vaddl.s16       q1,  d3,  d19

        vext.8          q8,  q11, q12, #3
        vdup.8          d20, d27[2]
        vext.8          q9,  q11, q12, #4
        vdup.8          d21, d27[3]

        vadd.i32        q2,  q2,  q6
        vadd.i32        q3,  q3,  q7
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d16, d20
        vmull.s8        q1,  d17, d20
        vmull.s8        q6,  d18, d21
        vmull.s8        q7,  d19, d21

        sub             r12, r0,  #1*GRAIN_WIDTH + 3

        vaddl.s16       q8,  d0,  d12
        vaddl.s16       q9,  d1,  d13
        vaddl.s16       q0,  d2,  d14
        vaddl.s16       q1,  d3,  d15

        vext.8          q6,  q11, q12, #5
        vdup.8          d20, d27[4]
        vext.8          q7,  q11, q12, #6
        vdup.8          d21, d27[5]

        vld1.8          {q11, q12}, [r12]

        vadd.i32        q2,  q2,  q8
        vadd.i32        q3,  q3,  q9
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d12, d20
        vmull.s8        q1,  d13, d20
        vmull.s8        q8,  d14, d21
        vmull.s8        q9,  d15, d21

        vaddl.s16       q6,  d0,  d16
        vaddl.s16       q7,  d1,  d17
        vaddl.s16       q0,  d2,  d18
        vaddl.s16       q1,  d3,  d19

        vdup.8          d20, d27[6]
        vext.8          q9,  q11, q12, #1
        vdup.8          d21, d27[7]

        vadd.i32        q2,  q2,  q6
        vadd.i32        q3,  q3,  q7
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d22, d20
        vmull.s8        q1,  d23, d20
        vmull.s8        q6,  d18, d21
        vmull.s8        q7,  d19, d21

        vaddl.s16       q8,  d0,  d12
        vaddl.s16       q9,  d1,  d13
        vaddl.s16       q0,  d2,  d14
        vaddl.s16       q1,  d3,  d15

        vext.8          q6,  q11, q12, #2
        vdup.8          d20, d28[0]
        vext.8          q7,  q11, q12, #3
        vdup.8          d21, d28[1]

        vadd.i32        q2,  q2,  q8
        vadd.i32        q3,  q3,  q9
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d12, d20
        vmull.s8        q1,  d13, d20
        vmull.s8        q8,  d14, d21
        vmull.s8        q9,  d15, d21

        vaddl.s16       q6,  d0,  d16
        vaddl.s16       q7,  d1,  d17
        vaddl.s16       q0,  d2,  d18
        vaddl.s16       q1,  d3,  d19

        vext.8          q8,  q11, q12, #4
        vdup.8          d20, d28[2]
        vext.8          q9,  q11, q12, #5
        vdup.8          d21, d28[3]

        vadd.i32        q2,  q2,  q6
        vadd.i32        q3,  q3,  q7
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d16, d20
        vmull.s8        q1,  d17, d20
        vmull.s8        q6,  d18, d21
        vmull.s8        q7,  d19, d21

        vaddl.s16       q8,  d0,  d12
        vaddl.s16       q9,  d1,  d13
        vaddl.s16       q0,  d2,  d14
        vaddl.s16       q1,  d3,  d15

        vext.8          q6,  q11, q12, #6
        vdup.8          d20, d28[4]

        vadd.i32        q2,  q2,  q8
        vadd.i32        q3,  q3,  q9
        vadd.i32        q4,  q4,  q0
        vadd.i32        q5,  q5,  q1

        vmull.s8        q0,  d12, d20
        vmull.s8        q1,  d13, d20

        vaddw.s16       q2,  q2,  d0
        vaddw.s16       q3,  q3,  d1
        vaddw.s16       q4,  q4,  d2
        vaddw.s16       q5,  q5,  d3

        bx              lr
endfunc

.macro sum_lag3_func type, uv_layout, edge, elems=16
function sum_\type\()_lag3_\edge\()_neon
        push            {r1, lr}
        sum_lag_n_body  lag3, \type, \uv_layout, \edge, \elems, store=1, uv_coeff=d29[0]
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
        push            {r11,lr}
1:
        get_grain_row   d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26
        subs            r1,  r1,  #1
        store_grain_row d16, d17, d18, d19, d20, d21, d22, d23, d24, d25, d26
        bgt             1b
        pop             {r11,pc}
endfunc

function generate_grain_rows_44_neon
        push            {r11,lr}
1:
        get_grain_row_44 d16, d17, d18, d19, d20, d21
        subs            r1,  r1,  #1
        store_grain_row_44 d16, d17, d18, d19, d20, d21
        bgt             1b
        pop             {r11,pc}
endfunc

function gen_grain_uv_444_lag0_neon
        vld1.8          {q3}, [r11]!
        push            {r11,lr}
        bl              get_gaussian_neon
        vrshl.s16       q8,  q0,  q15
        bl              get_gaussian_neon
        vrshl.s16       q9,  q0,  q15
        vqmovn.s16      d0,  q8
        vqmovn.s16      d1,  q9

        vand            q3,  q3,  q1
        vmull.s8        q2,  d6,  d22
        vmull.s8        q3,  d7,  d22
        vrshl.s16       q2,  q2,  q12
        vrshl.s16       q3,  q3,  q12
        vaddw.s8        q2,  q2,  d0
        vaddw.s8        q3,  q3,  d1
        vqmovn.s16      d4,  q2
        vqmovn.s16      d5,  q3
        vst1.8          {q2}, [r0]!
        pop             {r11,pc}
endfunc

function get_grain_row_44_neon
        push            {r11,lr}
        get_grain_row_44 d16, d17, d18, d19, d20, d21
        pop             {r11,pc}
endfunc

function add_uv_420_coeff_lag0_neon
        vld1.16         {q2, q3}, [r11]!
        vld1.16         {q4, q5}, [r12]!
        vpaddl.s8       q2,  q2
        vpaddl.s8       q3,  q3
        vpaddl.s8       q4,  q4
        vpaddl.s8       q5,  q5
        vadd.i16        q2,  q2,  q4
        vadd.i16        q3,  q3,  q5
        vrshrn.s16      d4,  q2,  #2
        vrshrn.s16      d5,  q3,  #2
        b               add_coeff_lag0_start
endfunc

function add_uv_422_coeff_lag0_neon
        vld1.16         {q2, q3}, [r11]!
        vpaddl.s8       q2,  q2
        vpaddl.s8       q3,  q3
        vrshrn.s16      d4,  q2,  #1
        vrshrn.s16      d5,  q3,  #1

add_coeff_lag0_start:
        vand            q3,  q2,  q1
        vmull.s8        q2,  d6,  d22
        vmull.s8        q3,  d7,  d22
        vrshl.s16       q2,  q2,  q12
        vrshl.s16       q3,  q3,  q12
        vaddw.s8        q2,  q2,  d0
        vaddw.s8        q3,  q3,  d1
        vqmovn.s16      d4,  q2
        vqmovn.s16      d5,  q3
        bx              lr
endfunc

.macro gen_grain_82 type
function generate_grain_\type\()_8bpc_neon, export=1
        push            {r4-r11,lr}

.ifc \type, uv_444
        mov             r12, r3
        mov             lr,  #28
        add             r11, r1,  #3*GRAIN_WIDTH
        mov             r1,  r2
        mul             r12, r12, lr
.endif
        movrel          r3,  X(gaussian_sequence)
        ldr             r2,  [r1, #FGD_SEED]
        ldr             r9,  [r1, #FGD_GRAIN_SCALE_SHIFT]
.ifc \type, y
        add             r4,  r1,  #FGD_AR_COEFFS_Y
.else
        add             r4,  r1,  #FGD_AR_COEFFS_UV
.endif
        adr             r5,  L(gen_grain_\type\()_tbl)
        ldr             r6,  [r1, #FGD_AR_COEFF_LAG]
        add             r9,  r9,  #4
        ldr             r6,  [r5, r6, lsl #2]
        vdup.16         q15, r9    // 4 + data->grain_scale_shift
        add             r5,  r5,  r6
        vneg.s16        q15, q15

.ifc \type, uv_444
        cmp             r12, #0
        movw            r10, #0x49d8
        movw            lr,  #0xb524
        // Intentionally using a separate register instead of moveq with an
        // immediate constant, to avoid armv8 deprecated it instruction forms.
        it              eq
        moveq           r10, lr
        add             r4,  r4,  r12       // Add offset to ar_coeffs_uv[1]
        eor             r2,  r2,  r10
.endif

        ldr             r7,  [r1, #FGD_AR_COEFF_SHIFT]
        mov             r8,  #1
        mov             r10, #1
        lsl             r8,  r8,  r7        // 1 << ar_coeff_shift
        lsl             r10, r10, r9        // 1 << (4 + data->grain_scale_shift)
        lsr             r8,  r8,  #1        // 1 << (ar_coeff_shift - 1)
        lsr             r10, r10, #1        // 1 << (4 + data->grain_scale_shift - 1)

        bx              r5

        .align 2
L(gen_grain_\type\()_tbl):
        .word L(generate_grain_\type\()_lag0) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB
        .word L(generate_grain_\type\()_lag1) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB
        .word L(generate_grain_\type\()_lag2) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB
        .word L(generate_grain_\type\()_lag3) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB

L(generate_grain_\type\()_lag0):
.ifc \type, y
        mov             r1,  #GRAIN_HEIGHT
        bl              generate_grain_rows_neon
.else

        mov             r1,  #3
        bl              generate_grain_rows_neon
        mov             r1,  #GRAIN_HEIGHT-3

        vdup.16         q12, r7
        vld1.8          {d22[]}, [r4]       // ar_coeffs_uv[0]
        vmov.i8         q0,  #0
        vmov.i8         q1,  #255
        vext.8          q13, q0,  q1,  #13
        vext.8          q14, q1,  q0,  #1
        vneg.s16        q12, q12

1:
        vmov            q1,  q13
        bl              gen_grain_uv_444_lag0_neon // 16
        vmov.i8         q1,  #255
        bl              gen_grain_uv_444_lag0_neon // 32
        bl              gen_grain_uv_444_lag0_neon // 48
        bl              gen_grain_uv_444_lag0_neon // 64
        vmov            q1,  q14
        bl              gen_grain_uv_444_lag0_neon // 80
        get_grain_2     d16
        subs            r1,  r1,  #1
        add             r11, r11, #2
        vst1.16         {d16[0]}, [r0]!
        bgt             1b
.endif
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag1):
        vpush           {q4-q7}
        mov             r5,  #127
        vld1.8          {d27[]}, [r4]!      // ar_coeffs_y[0]
        vld1.8          {d28[]}, [r4]!      // ar_coeffs_y[1]
        vld1.8          {d29[]}, [r4]       // ar_coeffs_y[2]
.ifc \type, y
        ldrsb           r4,  [r4, #1]       // ar_coeffs_y[3]
.else
        add             r4,  r4,  #2
.endif

        mov             r1,  #3
.ifc \type, uv_444
        vld1.8          {d13[]}, [r4]       // ar_coeffs_uv[4]
        ldrsb           r4,  [r4, #-1]      // ar_coeffs_uv[3]
.endif
        bl              generate_grain_rows_neon

        mov             r1,  #GRAIN_HEIGHT - 3
1:
        sum_\type\()_lag1 q7,  q8,  q8,  q9,  left
        sum_\type\()_lag1 q8,  q8,  q9,  q10
        sum_\type\()_lag1 q9,  q9,  q10, q11
        sum_\type\()_lag1 q10, q10, q11, q12
        sum_\type\()_lag1 q12, q11, q12, q13, right
        get_grain_2     d26
        subs            r1,  r1,  #1
.ifc \type, uv_444
        add             r11, r11, #2
.endif
        store_grain_row d14, d15, d16, d17, d18, d19, d20, d21, d24, d25, d26
        vmov            q11, q10
        vmov            q10, q9
        vmov            q9,  q8
        vmov            q8,  q7
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag2):
        vpush           {q4-q7}
        mov             r5,  #127
        vld1.8          {d28,d29}, [r4]     // ar_coeffs_y[0-11], ar_coeffs_uv[0-12]

        vmov.s8         r4,  d29[2]
        vmov.s8         r10, d29[3]

        mov             r1,  #3
        bl              generate_grain_rows_neon

        mov             r1,  #GRAIN_HEIGHT - 3
1:
        bl              sum_\type\()_lag2_left_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_right_neon
        get_grain_2     d16
        subs            r1,  r1,  #1
.ifc \type, uv_444
        add             r11, r11, #2
.endif
        vst1.16         {d16[0]}, [r0]!
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag3):
        vpush           {q4-q7}
        mov             r5,  #127
        vld1.8          {q13, q14}, [r4]    // ar_coeffs_y[0-23], ar_coeffs_uv[0-24]

        vmov.u8         r4,  d28[5]
        vmov.u8         r10, d28[6]
        vmov.u8         r12, d28[7]

        orr             r4,  r4,  r10, lsl #8
        orr             r4,  r4,  r12, lsl #16

        mov             r1,  #3
        vpush           {d26}
        bl              generate_grain_rows_neon
        vpop            {d26}

        mov             r1,  #GRAIN_HEIGHT - 3
1:
        bl              sum_\type\()_lag3_left_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_right_neon
        get_grain_2     d16
        subs            r1,  r1,  #1
.ifc \type, uv_444
        add             r11, r11, #2
.endif
        vst1.16         {d16[0]}, [r0]!
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc
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
        push            {r4-r11,lr}

        mov             r12, r3
        mov             lr,  #28
        add             r11, r1,  #3*GRAIN_WIDTH-3
        mov             r1,  r2
        mul             r12, r12, lr

        movrel          r3,  X(gaussian_sequence)
        ldr             r2,  [r1, #FGD_SEED]
        ldr             r9,  [r1, #FGD_GRAIN_SCALE_SHIFT]
        add             r4,  r1,  #FGD_AR_COEFFS_UV
        adr             r5,  L(gen_grain_\type\()_tbl)
        ldr             r6,  [r1, #FGD_AR_COEFF_LAG]
        add             r9,  r9,  #4
        ldr             r6,  [r5, r6, lsl #2]
        vdup.16         q15, r9    // 4 + data->grain_scale_shift
        add             r5,  r5,  r6
        vneg.s16        q15, q15

        cmp             r12, #0
        movw            r10, #0x49d8
        movw            lr,  #0xb524
        // Intentionally using a separate register instead of moveq with an
        // immediate constant, to avoid armv8 deprecated it instruction forms.
        it              eq
        moveq           r10, lr
        add             r4,  r4,  r12       // Add offset to ar_coeffs_uv[1]
        eor             r2,  r2,  r10

        ldr             r7,  [r1, #FGD_AR_COEFF_SHIFT]
        mov             r8,  #1
        mov             r10, #1
        lsl             r8,  r8,  r7        // 1 << ar_coeff_shift
        lsl             r10, r10, r9        // 1 << (4 + data->grain_scale_shift)
        lsr             r8,  r8,  #1        // 1 << (ar_coeff_shift - 1)
        lsr             r10, r10, #1        // 1 << (4 + data->grain_scale_shift - 1)
        bx              r5

        .align 2
L(gen_grain_\type\()_tbl):
        .word L(generate_grain_\type\()_lag0) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB
        .word L(generate_grain_\type\()_lag1) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB
        .word L(generate_grain_\type\()_lag2) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB
        .word L(generate_grain_\type\()_lag3) - L(gen_grain_\type\()_tbl) + CONFIG_THUMB

L(generate_grain_\type\()_lag0):
.ifc \type, uv_420
        vpush           {q4-q5}
.endif
        mov             r1,  #3
        bl              generate_grain_rows_44_neon
        set_height      r1,  \type

        vdup.16         q12, r7
        vld1.8          {d22[]}, [r4]       // ar_coeffs_uv[0]
        vmov.i8         q0,  #0
        vmov.i8         q1,  #255
        vext.8          q13, q0,  q1,  #13
        vext.8          q14, q1,  q0,  #7
        vneg.s16        q12, q12

1:
        bl              get_grain_row_44_neon
.ifc \type, uv_420
        add             r12, r11, #GRAIN_WIDTH
.endif
        vmov            q1,  q13
        vmov            q0,  q8
        bl              add_\type\()_coeff_lag0_neon
        vmov.i8         q1,  #255
        vmov            q0,  q9
        vmov            q8,  q2
        bl              add_\type\()_coeff_lag0_neon
        vmov.i8         q1,  q14
        vmov            q0,  q10
        vmov            q9,  q2
        bl              add_\type\()_coeff_lag0_neon
        vmov            q10, q2
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        store_grain_row_44 d16, d17, d18, d19, d20, d21
        bgt             1b

.ifc \type, uv_420
        vpop            {q4-q5}
.endif
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag1):
        vpush           {q4-q7}
        mov             r5,  #127
        vld1.8          {d27[]}, [r4]!      // ar_coeffs_uv[0]
        vld1.8          {d28[]}, [r4]!      // ar_coeffs_uv[1]
        vld1.8          {d29[]}, [r4]       // ar_coeffs_uv[2]
        add             r4,  r4,  #2

        mov             r1,  #3
        vld1.8          {d13[]}, [r4]       // ar_coeffs_uv[4]
        ldrsb           r4,  [r4, #-1]      // ar_coeffs_uv[3]
        bl              generate_grain_rows_44_neon

        set_height      r1,  \type
1:
        sum_\type\()_lag1 q7,  q8,  q8,  q9,  left
        sum_\type\()_lag1 q8,  q8,  q9,  q10
        sum_\type\()_lag1 q10, q9,  q10, q11, right
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        store_grain_row_44 d14, d15, d16, d17, d20, d21
        vmov            q9,  q8
        vmov            q8,  q7
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag2):
        vpush           {q4-q7}
        mov             r5,  #127
        vld1.8          {d28,d29}, [r4]     // ar_coeffs_uv[0-12]

        vmov.s8         r4,  d29[2]
        vmov.s8         r10, d29[3]

        mov             r1,  #3
        bl              generate_grain_rows_44_neon

        set_height      r1,  \type
1:
        bl              sum_\type\()_lag2_left_neon
        bl              sum_\type\()_lag2_mid_neon
        bl              sum_\type\()_lag2_right_neon
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        add             r0,  r0,  #GRAIN_WIDTH-48
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag3):
        vpush           {q4-q7}
        mov             r5,  #127
        vld1.8          {q13, q14}, [r4]    // ar_coeffs_y[0-23], ar_coeffs_uv[0-24]

        vmov.u8         r4,  d28[5]
        vmov.u8         r10, d28[6]
        vmov.u8         r12, d28[7]

        orr             r4,  r4,  r10, lsl #8
        orr             r4,  r4,  r12, lsl #16

        mov             r1,  #3
        bl              generate_grain_rows_44_neon

        set_height      r1,  \type
1:
        bl              sum_\type\()_lag3_left_neon
        bl              sum_\type\()_lag3_mid_neon
        bl              sum_\type\()_lag3_right_neon
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        add             r0,  r0,  #GRAIN_WIDTH-48
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc
.endm

gen_grain_44 uv_420
gen_grain_44 uv_422

.macro gather_interleaved dst1, dst2, src1, src2, off
        vmov.u8         r11, \src1[0+\off]
        vmov.u8         r12, \src2[0+\off]
        add             r11, r11, r3
        vmov.u8         lr,  \src1[2+\off]
        add             r12, r12, r3
        vld1.8          {\dst1[0+\off]}, [r11]
        vmov.u8         r11, \src2[2+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst2[0+\off]}, [r12]
        vmov.u8         r12, \src1[4+\off]
        add             r11, r11, r3
        vld1.8          {\dst1[2+\off]}, [lr]
        vmov.u8         lr,  \src2[4+\off]
        add             r12, r12, r3
        vld1.8          {\dst2[2+\off]}, [r11]
        vmov.u8         r11, \src1[6+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst1[4+\off]}, [r12]
        vmov.u8         r12, \src2[6+\off]
        add             r11, r11, r3
        vld1.8          {\dst2[4+\off]}, [lr]
        add             r12, r12, r3
        vld1.8          {\dst1[6+\off]}, [r11]
        vld1.8          {\dst2[6+\off]}, [r12]
.endm

.macro gather dst1, dst2, dst3, dst4, src1, src2, src3, src4
        gather_interleaved \dst1, \dst3, \src1, \src3, 0
        gather_interleaved \dst1, \dst3, \src1, \src3, 1
        gather_interleaved \dst2, \dst4, \src2, \src4, 0
        gather_interleaved \dst2, \dst4, \src2, \src4, 1
.endm

function gather32_neon
        push            {r11-r12,lr}
        gather          d8,  d9,  d10, d11, d0,  d1,  d2,  d3
        pop             {r11-r12,pc}
endfunc

function gather16_neon
        push            {r11-r12,lr}
        gather_interleaved d8,  d9,  d0,  d1,  0
        gather_interleaved d8,  d9,  d0,  d1,  1
        pop             {r11-r12,pc}
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
        mla             \dst, \stride, \offy, \src // grain_lut += grain_stride * offy
        add             \dst, \dst, \offx          // grain_lut += offx
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
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // scaling_shift, grain_lut
        ldrd            r6,  r7,  [sp, #108]   // offsets, h
        ldr             r8,       [sp, #116]   // clip
        mov             r9,  #GRAIN_WIDTH      // grain_lut stride

        neg             r4,  r4
        vdup.16         q13, r4                // -scaling_shift
        cmp             r8,  #0

        movrel_local    r12, overlap_coeffs_0

        beq             1f
        // clip
        vmov.i8         q14, #16
        vmov.i8         q15, #235
        b               2f
1:
        // no clip
        vmov.i8         q14, #0
        vmov.i8         q15, #255
2:

        vld1.8          {d24, d25}, [r12, :128] // overlap_coeffs

        add             r5,  r5,  #9           // grain_lut += 9
        add             r5,  r5,  r9,  lsl #3  // grain_lut += 8 * grain_stride
        add             r5,  r5,  r9           // grain_lut += grain_stride

        ldr             r10, [r6, #8]          // offsets[1][0]
        calc_offset     r10, r4,  r10, 0,   0
        add_offset      r4,  r10, r4,  r5,  r9
        ldr             r10, [r6, #4]          // offsets[0][1]
        calc_offset     r10, r11, r10, 0,   0
        add_offset      r11, r10, r11, r5,  r9
        ldr             r10, [r6, #12]         // offsets[1][1]
        calc_offset     r10, r8,  r10, 0,   0
        add_offset      r8,  r10, r8,  r5,  r9
        ldr             r6,  [r6]              // offsets[0][0]
        calc_offset     r6,  lr,  r6,  0,   0
        add_offset      r5,  r6,  lr,  r5,  r9

        add             r4,  r4,  #32          // grain_lut += FG_BLOCK_SIZE * bx
        add             r6,  r11, r9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by

        ldr             r10, [sp, #120]        // type
        adr             r11, L(fgy_loop_tbl)

        tst             r10, #1
        ldr             r10, [r11, r10, lsl #2]

        add             r8,  r8,  r9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             r8,  r8,  #32          // grain_lut += FG_BLOCK_SIZE * bx

        add             r11, r11, r10

        beq             1f
        // y overlap
        vdup.8          d14, d24[0]
        vdup.8          d15, d24[1]
        mov             r10, r7                // backup actual h
        mov             r7,  #2
1:
        bx              r11
endfunc

function fgy_loop_neon
L(fgy_loop_tbl):
        .word L(loop_00) - L(fgy_loop_tbl) + CONFIG_THUMB
        .word L(loop_01) - L(fgy_loop_tbl) + CONFIG_THUMB
        .word L(loop_10) - L(fgy_loop_tbl) + CONFIG_THUMB
        .word L(loop_11) - L(fgy_loop_tbl) + CONFIG_THUMB

.macro fgy ox, oy
L(loop_\ox\oy):
1:
.if \ox
        vld1.8          {d8},       [r4],       r9 // grain_lut old
.endif
.if \oy
        vld1.8          {q2, q3},   [r6],       r9 // grain_lut top
.endif
.if \ox && \oy
        vld1.8          {d10},      [r8],       r9 // grain_lut top old
.endif
        vld1.8          {q0,  q1},  [r1, :128], r2 // src
        vld1.8          {q10, q11}, [r5],       r9 // grain_lut

.if \ox
        vmull.s8        q4,  d8,  d24
        vmlal.s8        q4,  d20, d25
.endif

.if \oy
.if \ox
        vmull.s8        q5,  d10, d24
        vmlal.s8        q5,  d4,  d25
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d4,  q5,  #5
.endif

        vmull.s8        q4,  d20, d15
        vmull.s8        q5,  d21, d15
        vmull.s8        q8,  d22, d15
        vmull.s8        q9,  d23, d15
        vmlal.s8        q4,  d4,  d14
        vmlal.s8        q5,  d5,  d14
        vmlal.s8        q8,  d6,  d14
        vmlal.s8        q9,  d7,  d14
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d21, q5,  #5
        vqrshrn.s16     d22, q8,  #5
        vqrshrn.s16     d23, q9,  #5
.elseif \ox
        vqrshrn.s16     d20, q4,  #5
.endif

        bl              gather32_neon

        vmovl.s8        q8,  d20       // grain
        vmovl.s8        q9,  d21
        vmovl.s8        q10, d22
        vmovl.s8        q11, d23

        vmovl.u8        q2,  d8        // scaling
        vmovl.u8        q3,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vmul.i16        q8,  q8,  q2   // scaling * grain
        vmul.i16        q9,  q9,  q3
        vmul.i16        q10, q10, q4
        vmul.i16        q11, q11, q5

        vrshl.s16       q8,  q8,  q13  // round2(scaling * grain, scaling_shift)
        vrshl.s16       q9,  q9,  q13
        vrshl.s16       q10, q10, q13
        vrshl.s16       q11, q11, q13

        vaddw.u8        q8,  q8,  d0   // *src + noise
        vaddw.u8        q9,  q9,  d1
        vaddw.u8        q10, q10, d2
        vaddw.u8        q11, q11, d3

        vqmovun.s16     d0,  q8
        vqmovun.s16     d1,  q9
        vqmovun.s16     d2,  q10
        vqmovun.s16     d3,  q11

        vmax.u8         q0,  q0,  q14
        vmax.u8         q1,  q1,  q14
        vmin.u8         q0,  q0,  q15
        vmin.u8         q1,  q1,  q15

        subs            r7,  r7,  #1
.if \oy
        vdup.8          d14, d25[0]
        vdup.8          d15, d25[1]
.endif
        vst1.8          {q0, q1}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r10, #2
        sub             r7,  r10, #2           // restore actual remaining h
        bgt             L(loop_\ox\()0)
.endif
        vpop            {q4-q7}
        pop             {r4-r11,pc}
.endm

        fgy             0, 0
        fgy             0, 1
        fgy             1, 0
        fgy             1, 1
endfunc

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
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // data, grain_lut
        ldrd            r6,  r7,  [sp, #108]   // luma_row, luma_stride
        ldrd            r8,  r9,  [sp, #116]   // offsets, h
        ldrd            r10, r11, [sp, #124]   // uv, is_id

        // !csfl
        add             r10, r4,  r10, lsl #2  // + 4*uv
        add             r12, r10, #FGD_UV_LUMA_MULT
        add             lr,  r10, #FGD_UV_MULT
        add             r10, r10, #FGD_UV_OFFSET
        vld1.16         {d4[]},  [r12]         // uv_luma_mult
        vld1.16         {d4[2]}, [r10]         // uv_offset
        vld1.16         {d4[1]}, [lr]          // uv_mult

        ldr             lr,  [r4, #FGD_SCALING_SHIFT]
        ldr             r12, [r4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        neg             lr,  lr                // -scaling_shift

        cmp             r12, #0
        vdup.16         q13, lr                // -scaling_shift

        beq             1f
        // clip
        cmp             r11, #0
        vmov.i8         q14, #16
        vmov.i8         q15, #240
        beq             2f
        // is_id
        vmov.i8         q15, #235
        b               2f
1:
        // no clip
        vmov.i8         q14, #0
        vmov.i8         q15, #255
2:

        mov             r10, #GRAIN_WIDTH      // grain_lut stride

        add             r5,  r5,  #(3 + (2 >> \sx)*3) // grain_lut += 9 or 6
.if \sy
        add             r5,  r5,  r10, lsl #2  // grain_lut += 4 * grain_stride
        add             r5,  r5,  r10, lsl #1  // grain_lut += 2 * grain_stride
.else
        add             r5,  r5,  r10, lsl #3  // grain_lut += 8 * grain_stride
        add             r5,  r5,  r10          // grain_lut += grain_stride
.endif

        ldr             r12, [r8, #8]          // offsets[1][0]
        calc_offset     r12, r4,  r12, \sx, \sy
        add_offset      r4,  r12, r4,  r5,  r10

        ldr             r12, [r8, #4]          // offsets[0][1]
        calc_offset     r12, lr,  r12, \sx, \sy
        add_offset      lr,  r12, lr,  r5,  r10

        ldr             r12, [r8, #12]         // offsets[1][1]
        calc_offset     r12, r11, r12, \sx, \sy
        add_offset      r11, r12, r11, r5,  r10

        ldr             r8,  [r8]              // offsets[0][0]
        calc_offset     r8,  r12, r8,  \sx, \sy
        add_offset      r5,  r8,  r12, r5,  r10

        add             r4,  r4,  #(32 >> \sx) // grain_lut += FG_BLOCK_SIZE * bx
        add             r8,  lr,  r10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             r11, r11, r10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             r11, r11, #(32 >> \sx) // grain_lut += FG_BLOCK_SIZE * bx

        movrel_local    r12, overlap_coeffs_\sx
        ldr             lr,  [sp, #132]        // type

        vld1.8          {d24, d25}, [r12, :128] // overlap_coeffs

        movrel_local    r12, L(fguv_loop_sx\sx\()_tbl)
#if CONFIG_THUMB
        // This uses movrel_local instead of adr above, because the target
        // can be out of range for adr. But movrel_local leaves the thumb bit
        // set on COFF (but probably wouldn't if building for thumb on ELF),
        // thus try to clear the bit for robustness.
        bic             r12, r12, #1
#endif

        tst             lr,  #1
        ldr             lr,  [r12, lr,  lsl #2]

        add             r12, r12, lr

        beq             1f
        // y overlap
        sub             lr,  r9,  #(2 >> \sy)  // backup remaining h
        mov             r9,  #(2 >> \sy)

1:

.if \sy
        vmov.i8         d6,  #23
        vmov.i8         d7,  #22
.else
        vmov.i8         d6,  #27
        vmov.i8         d7,  #17
.endif

.if \sy
        add             r7,  r7,  r7           // luma_stride *= 2
.endif

        bx              r12
endfunc
.endm

fguv 420, 1, 1
fguv 422, 1, 0
fguv 444, 0, 0

function fguv_loop_sx0_neon
L(fguv_loop_sx0_tbl):
        .word L(fguv_loop_sx0_csfl0_00) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl0_01) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl0_10) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl0_11) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_00) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_01) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_10) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx0_csfl1_11) - L(fguv_loop_sx0_tbl) + CONFIG_THUMB

.macro fguv_loop_sx0 csfl, ox, oy
L(fguv_loop_sx0_csfl\csfl\()_\ox\oy):
.if \oy
        mov             r12, lr
.endif
1:
.if \ox
        vld1.8          {d8},       [r4],        r10 // grain_lut old
.endif
.if \oy
        vld1.8          {q8, q9},   [r8],        r10 // grain_lut top
.endif
.if \ox && \oy
        vld1.8          {d10},      [r11],       r10 // grain_lut top old
.endif
        vld1.8          {q0,  q1},  [r6, :128],  r7  // luma
        vld1.8          {q10, q11}, [r5],        r10 // grain_lut

.if \ox
        vmull.s8        q4,  d8,  d24
        vmlal.s8        q4,  d20, d25
.endif

.if \oy
.if \ox
        vmull.s8        q5,  d10, d24
        vmlal.s8        q5,  d16, d25
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d16, q5,  #5
.endif

        vmull.s8        q4,  d20, d7
        vmull.s8        q5,  d21, d7
        vmull.s8        q6,  d22, d7
        vmull.s8        q7,  d23, d7
        vmlal.s8        q4,  d16, d6
        vmlal.s8        q5,  d17, d6
        vmlal.s8        q6,  d18, d6
        vmlal.s8        q7,  d19, d6
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d21, q5,  #5
        vqrshrn.s16     d22, q6,  #5
        vqrshrn.s16     d23, q7,  #5
.elseif \ox
        vqrshrn.s16     d20, q4,  #5
.endif
.if !\csfl
        vld1.8          {q8,  q9},  [r1, :128] // src
        vmovl.u8        q4,  d0
        vmovl.u8        q5,  d1
        vmovl.u8        q6,  d2
        vmovl.u8        q7,  d3
        vmovl.u8        q0,  d16
        vmovl.u8        q1,  d17
        vmovl.u8        q8,  d18
        vmovl.u8        q9,  d19
        vmul.i16        q4,  q4,  d4[0]
        vmul.i16        q5,  q5,  d4[0]
        vmul.i16        q6,  q6,  d4[0]
        vmul.i16        q7,  q7,  d4[0]
        vmul.i16        q0,  q0,  d4[1]
        vmul.i16        q1,  q1,  d4[1]
        vmul.i16        q8,  q8,  d4[1]
        vmul.i16        q9,  q9,  d4[1]
        vqadd.s16       q4,  q4,  q0
        vqadd.s16       q5,  q5,  q1
        vqadd.s16       q6,  q6,  q8
        vqadd.s16       q7,  q7,  q9
        vdup.16         q0,  d4[2]
        vshr.s16        q4,  q4,  #6
        vshr.s16        q5,  q5,  #6
        vshr.s16        q6,  q6,  #6
        vshr.s16        q7,  q7,  #6
        vadd.i16        q4,  q4,  q0
        vadd.i16        q5,  q5,  q0
        vadd.i16        q6,  q6,  q0
        vadd.i16        q7,  q7,  q0
        vqmovun.s16     d0,  q4
        vqmovun.s16     d1,  q5
        vqmovun.s16     d2,  q6
        vqmovun.s16     d3,  q7
.endif

        bl              gather32_neon

        vld1.8          {q0,  q1},  [r1, :128], r2 // src

        vmovl.s8        q8,  d20       // grain
        vmovl.s8        q9,  d21
        vmovl.s8        q10, d22
        vmovl.s8        q11, d23

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vmul.i16        q8,  q8,  q6   // scaling * grain
        vmul.i16        q9,  q9,  q7
        vmul.i16        q10, q10, q4
        vmul.i16        q11, q11, q5

        vrshl.s16       q8,  q8,  q13  // round2(scaling * grain, scaling_shift)
        vrshl.s16       q9,  q9,  q13
        vrshl.s16       q10, q10, q13
        vrshl.s16       q11, q11, q13

        vaddw.u8        q8,  q8,  d0   // *src + noise
        vaddw.u8        q9,  q9,  d1
        vaddw.u8        q10, q10, d2
        vaddw.u8        q11, q11, d3

        vqmovun.s16     d0,  q8
        vqmovun.s16     d1,  q9
        vqmovun.s16     d2,  q10
        vqmovun.s16     d3,  q11

        vmax.u8         q0,  q0,  q14
        vmax.u8         q1,  q1,  q14
        vmin.u8         q0,  q0,  q15
        vmin.u8         q1,  q1,  q15

        subs            r9,  r9,  #1
.if \oy
        vdup.8          d6,  d25[0]
        vdup.8          d7,  d25[1]
.endif

        vst1.8          {q0, q1}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r12, #0
        mov             r9,  r12               // restore actual remaining h
        bgt             L(fguv_loop_sx0_csfl\csfl\()_\ox\()0)
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
        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc

function fguv_loop_sx1_neon
L(fguv_loop_sx1_tbl):
        .word L(fguv_loop_sx1_csfl0_00) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl0_01) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl0_10) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl0_11) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_00) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_01) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_10) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB
        .word L(fguv_loop_sx1_csfl1_11) - L(fguv_loop_sx1_tbl) + CONFIG_THUMB

.macro fguv_loop_sx1 csfl, ox, oy
L(fguv_loop_sx1_csfl\csfl\()_\ox\oy):
.if \oy
        mov             r12, lr
.endif
1:
.if \ox
        vld1.8          {d8},       [r4],        r10 // grain_lut old
.endif
.if \oy
        vld1.8          {q8},       [r8],        r10 // grain_lut top
.endif
.if \ox && \oy
        vld1.8          {d10},      [r11],       r10 // grain_lut top old
.endif
        vld1.8          {q0,  q1},  [r6, :128],  r7  // luma
        vld1.8          {q10},      [r5],        r10 // grain_lut
        vld1.8          {q11},      [r1, :128],  r2  // src

.if \ox
        vmull.s8        q4,  d8,  d24
        vmlal.s8        q4,  d20, d25
.endif

        vpaddl.u8       q0,  q0
        vpaddl.u8       q1,  q1
.if \oy
.if \ox
        vmull.s8        q5,  d10, d24
        vmlal.s8        q5,  d16, d25
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d16, q5,  #5
.endif

        vmull.s8        q4,  d20, d7
        vmull.s8        q5,  d21, d7
        vmlal.s8        q4,  d16, d6
        vmlal.s8        q5,  d17, d6
        vqrshrn.s16     d20, q4,  #5
        vqrshrn.s16     d21, q5,  #5
.elseif \ox
        vqrshrn.s16     d20, q4,  #5
.endif
.if \csfl
        vrshrn.u16      d0,  q0,  #1
        vrshrn.u16      d1,  q1,  #1
.else
        vrshr.u16       q4,  q0,  #1
        vrshr.u16       q5,  q1,  #1
        vmovl.u8        q0,  d22
        vmovl.u8        q1,  d23
        vmul.i16        q4,  q4,  d4[0]
        vmul.i16        q5,  q5,  d4[0]
        vmul.i16        q0,  q0,  d4[1]
        vmul.i16        q1,  q1,  d4[1]
        vqadd.s16       q4,  q4,  q0
        vqadd.s16       q5,  q5,  q1
        vdup.16         q0,  d4[2]
        vshr.s16        q4,  q4,  #6
        vshr.s16        q5,  q5,  #6
        vadd.i16        q4,  q4,  q0
        vadd.i16        q5,  q5,  q0
        vqmovun.s16     d0,  q4
        vqmovun.s16     d1,  q5
.endif

        bl              gather16_neon

        vmovl.s8        q8,  d20       // grain
        vmovl.s8        q9,  d21

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9

        vmul.i16        q8,  q8,  q6   // scaling * grain
        vmul.i16        q9,  q9,  q7

        vrshl.s16       q8,  q8,  q13  // round2(scaling * grain, scaling_shift)
        vrshl.s16       q9,  q9,  q13

        vaddw.u8        q8,  q8,  d22  // *src + noise
        vaddw.u8        q9,  q9,  d23

        vqmovun.s16     d0,  q8
        vqmovun.s16     d1,  q9

        vmax.u8         q0,  q0,  q14
        vmin.u8         q0,  q0,  q15

        subs            r9,  r9,  #1
.if \oy
        vswp            d6,  d7
.endif
        vst1.8          {q0}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r12, #0
        mov             r9,  r12               // restore actual remaining h
        bgt             L(fguv_loop_sx1_csfl\csfl\()_\ox\()0)
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
        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc
