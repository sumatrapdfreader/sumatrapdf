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
        pop             {r11,pc}
endfunc

.macro get_grain_2 dst
        bl              get_grain_2_neon
.ifnc \dst, d0
        vmov            \dst, d0
.endif
.endm

function get_grain_4_neon
        push            {r11,lr}
        increment_seed  4
        read_rand       r11, 11,  3
        read_rand       r12, 11,  2
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d0[0]}, [r11]
        read_rand       r11, 11,  1
        vld1.16         {d0[1]}, [r12]
        read_rand       r12, 11,  0
        add             r11, r3,  r11, lsl #1
        add             r12, r3,  r12, lsl #1
        vld1.16         {d0[2]}, [r11]
        vld1.16         {d0[3]}, [r12]
        vrshl.s16       d0,  d0,  d30
        pop             {r11,pc}
endfunc

.macro get_grain_4 dst
        bl              get_grain_4_neon
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
        mvn             lr,  r5                   // grain_min = ~grain_max
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
        vext.8          q0,  q0,  q0,  #2
        ldrsh           r12, [r3, r12]
.if \n == 1
        mla             r11, r6,  r4,  r11        // sum (above) + *coeff * prev output
        add             r6,  r11, r8              // 1 << (ar_coeff_shift - 1)
        add             r12, r12, r10
        asr             r6,  r6,  r7              // >> ar_coeff_shift
        asr             r12, r12, r9              // >> (4 - bitdepth_min_8 + grain_scale_shift)
        add             r6,  r6,  r12
        cmp             r6,  r5
.elseif \n == 2
        mla             r11, r8,  r4,  r11        // sum (above) + *coeff * prev output 1
        mla             r11, r6,  r10, r11        // += *coeff * prev output 2
        mov             r8,  r6
        add             r6,  r11, r0              // 1 << (ar_coeff_shift - 1)
        add             r12, r12, lr              // 1 << (4 - bitdepth_min_8 + grain_scale_shift - 1)
        asr             r6,  r6,  r7              // >> ar_coeff_shift
        asr             r12, r12, r9              // >> (4 - bitdepth_min_8 + grain_scale_shift)
        add             r6,  r6,  r12
        push            {lr}
        cmp             r6,  r5
        mvn             lr,  r5                   // grain_min = ~grain_max
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
        add             r12, r12, lr              // 1 << (4 - bitdepth_min_8 + grain_scale_shift - 1)
        asr             r6,  r6,  r7              // >> ar_coeff_shift
        asr             r12, r12, r9              // >> (4 - bitdepth_min_8 + grain_scale_shift)
        add             r6,  r6,  r12
        push            {lr}
        cmp             r6,  r5
        mvn             lr,  r5                   // grain_min = ~grain_max
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
        vmov.16         d1[3], r6
        bgt             1b
        pop             {r0, pc}
endfunc
.endm

output_lag 1
output_lag 2
output_lag 3


function sum_lag1_above_neon
        sub             r12, r0,  #1*GRAIN_WIDTH*2 - 16
        vld1.16         {q10}, [r12] // load top right

        vext.8          q0,  q8,  q9,  #14 // top left, top mid
        vext.8          q1,  q9,  q10, #2  // top left, top mid

        vmull.s16       q2,  d18, d28
        vmlal.s16       q2,  d0,  d27
        vmlal.s16       q2,  d2,  d29
        vmull.s16       q3,  d19, d28
        vmlal.s16       q3,  d1,  d27
        vmlal.s16       q3,  d3,  d29

        vmov            q8,  q9
        vmov            q9,  q10

        bx              lr
endfunc

.macro sum_lag_n_body lag, type, uv_layout, edge, elems, uv_coeff
.ifc \lag\()_\edge, lag3_left
        bl              sum_lag3_left_above_neon
.else
        bl              sum_\lag\()_above_neon
.endif
.ifc \type, uv_420
        vpush           {q6-q7}
        add             r12, r11, #GRAIN_WIDTH*2
        vld1.16         {q0, q1}, [r11]!
        vld1.16         {q6, q7}, [r12]!
        vpadd.i16       d0,  d0,  d1
        vpadd.i16       d1,  d2,  d3
        vpadd.i16       d12, d12, d13
        vpadd.i16       d13, d14, d15
        vadd.i16        q0,  q0,  q6
        vpop            {q6-q7}
        vrshr.s16       q0,  q0,  #2
.endif
.ifc \type, uv_422
        vld1.16         {q0, q1}, [r11]!
        vpadd.i16       d0,  d0,  d1
        vpadd.i16       d1,  d2,  d3
        vrshr.s16       q0,  q0,  #1
.endif
.ifc \type, uv_444
        vld1.16         {q0}, [r11]!
.endif
.if \uv_layout
.ifnb \uv_coeff
        vdup.8          d13, \uv_coeff
        vmovl.s8        q6,  d13
.endif
        vmlal.s16       q2,  d0,  d13
        vmlal.s16       q3,  d1,  d13
.endif
.if \uv_layout && \elems == 8
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 444 && \elems == 7
        b               sum_\lag\()_y_\edge\()_start
.elseif \uv_layout == 422 && \elems == 1
        b               sum_\lag\()_uv_420_\edge\()_start
.else
sum_\lag\()_\type\()_\edge\()_start:
        push            {r11}
.if \elems > 4
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
        vext.8          q2,  q2,  q2,  #12
.ifc \lag, lag3
        vmov.s16        r10, d1[1]
.endif
.ifnc \lag, lag1
        vmov.s16        r8,  d1[2]
.endif
        vmov.s16        r6,  d1[3]

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
.ifc \edge, right
        mov             r1,  #3
        bl              output_\lag\()_neon
        read_shift_rand r12, 11
        add             r12, r3,  r12, lsl #1
        vld1.16         {d2[0]}, [r12]
        vrshl.s16       d2,  d2,  d30
        vext.8          q0,  q0,  q1,  #2
.else
        mov             r1,  #4
        bl              output_\lag\()_neon
.endif
.else
        // elems == 1
        increment_seed  4, shift=0
        vmov            q1,  q2
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
        vext.8          q0,  q0,  q1,  #14
.endif
        vst1.16         {q0}, [r0]!
        pop             {r11}
        pop             {r1, pc}
.endif
.endm

.macro sum_lag1_func type, uv_layout, edge, elems=8
function sum_\type\()_lag1_\edge\()_neon
        push            {r1, lr}
.ifc \edge, left
        sub             r12, r0,  #1*GRAIN_WIDTH*2
        vld1.8          {q9},  [r12] // load the previous block right above
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
        push            {lr}
        sub             r12, r0,  #2*GRAIN_WIDTH*2 - 16
        sub             lr,  r0,  #1*GRAIN_WIDTH*2 - 16
        vld1.16         {q10}, [r12] // load top right
        vld1.16         {q13}, [lr]

        vdup.8          d10, d28[0]
        vext.8          q0,  q8,  q9,  #12 // top left, top mid
        vdup.8          d12, d28[1]
        vext.8          q1,  q8,  q9,  #14
        vdup.8          d14, d28[3]
        vext.8          q4,  q9,  q10, #2  // top mid, top right
        vmovl.s8        q5,  d10
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14

        vmull.s16       q2,  d0,  d10
        vmlal.s16       q2,  d2,  d12
        vmlal.s16       q2,  d8,  d14
        vmull.s16       q3,  d1,  d10
        vmlal.s16       q3,  d3,  d12
        vmlal.s16       q3,  d9,  d14

        vdup.8          d10, d28[4]
        vext.8          q0,  q9,  q10, #4  // top mid, top right
        vdup.8          d12, d28[5]
        vext.8          q1,  q11, q12, #12 // top left, top mid
        vdup.8          d14, d28[6]
        vext.8          q4,  q11, q12, #14
        vmovl.s8        q5,  d10
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14

        vmlal.s16       q2,  d0,  d10
        vmlal.s16       q2,  d2,  d12
        vmlal.s16       q2,  d8,  d14
        vmlal.s16       q3,  d1,  d10
        vmlal.s16       q3,  d3,  d12
        vmlal.s16       q3,  d9,  d14

        vdup.8          d10, d29[0]
        vext.8          q0,  q12, q13, #2  // top mid, top right
        vdup.8          d12, d29[1]
        vext.8          q1,  q12, q13, #4

        vdup.8          d14, d28[2]
        vdup.8          d8,  d28[7]

        vmovl.s8        q5,  d10
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14
        vmovl.s8        q4,  d8

        vmlal.s16       q2,  d0,  d10
        vmlal.s16       q2,  d2,  d12
        vmlal.s16       q2,  d18, d14
        vmlal.s16       q2,  d24, d8
        vmlal.s16       q3,  d1,  d10
        vmlal.s16       q3,  d3,  d12
        vmlal.s16       q3,  d19, d14
        vmlal.s16       q3,  d25, d8

        vmov            q8,  q9
        vmov            q9,  q10

        vmov            q11, q12
        vmov            q12, q13

        pop             {pc}
endfunc

.macro sum_lag2_func type, uv_layout, edge, elems=8
function sum_\type\()_lag2_\edge\()_neon
        push            {r1, lr}
.ifc \edge, left
        sub             r12, r0,  #2*GRAIN_WIDTH*2
        sub             lr,  r0,  #1*GRAIN_WIDTH*2
        vld1.16         {q9},  [r12] // load the previous block right above
        vld1.16         {q12}, [lr]
.endif
        sum_lag_n_body  lag2, \type, \uv_layout, \edge, \elems, uv_coeff=d29[4]
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


function sum_lag3_left_above_neon
        // A separate codepath for the left edge, to avoid reading outside
        // of the edge of the buffer.
        sub             r12, r0,  #3*GRAIN_WIDTH*2
        vld1.8          {q11, q12}, [r12]
        vext.8          q12, q11, q12, #10
        vext.8          q11, q11, q11, #10
        b               sum_lag3_above_start
endfunc

function sum_lag3_above_neon
        movw            r12, #(3*GRAIN_WIDTH + 3)*2
        sub             r12, r0,  r12
        vld1.8          {q11, q12}, [r12]

sum_lag3_above_start:
        vdup.8          d12, d26[0]
        vext.8          q1,  q11, q12, #2
        vdup.8          d14, d26[1]
        vext.8          q4,  q11, q12, #4
        vdup.8          d16, d26[2]
        vext.8          q5,  q11, q12, #6
        vdup.8          d18, d26[3]
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14
        vmovl.s8        q8,  d16
        vmovl.s8        q9,  d18

        movw            r12, #(2*GRAIN_WIDTH + 3)*2
        sub             r12, r0,  r12

        vmull.s16       q2,  d22, d12
        vmlal.s16       q2,  d2,  d14
        vmlal.s16       q2,  d8,  d16
        vmlal.s16       q2,  d10, d18
        vmull.s16       q3,  d23, d12
        vmlal.s16       q3,  d3,  d14
        vmlal.s16       q3,  d9,  d16
        vmlal.s16       q3,  d11, d18

        vdup.8          d12, d26[4]
        vext.8          q0,  q11, q12, #8
        vdup.8          d14, d26[5]
        vext.8          q1,  q11, q12, #10
        vdup.8          d16, d26[6]
        vext.8          q4,  q11, q12, #12
        vld1.8          {q11, q12}, [r12]
        vdup.8          d18, d26[7]
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14
        vmovl.s8        q8,  d16
        vmovl.s8        q9,  d18

        vmlal.s16       q2,  d0,  d12
        vmlal.s16       q2,  d2,  d14
        vmlal.s16       q2,  d8,  d16
        vmlal.s16       q2,  d22, d18
        vmlal.s16       q3,  d1,  d12
        vmlal.s16       q3,  d3,  d14
        vmlal.s16       q3,  d9,  d16
        vmlal.s16       q3,  d23, d18

        vdup.8          d12, d27[0]
        vext.8          q0,  q11, q12, #2
        vdup.8          d14, d27[1]
        vext.8          q1,  q11, q12, #4
        vdup.8          d16, d27[2]
        vext.8          q4,  q11, q12, #6
        vdup.8          d18, d27[3]
        vext.8          q5,  q11, q12, #8
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14
        vmovl.s8        q8,  d16
        vmovl.s8        q9,  d18

        sub             r12, r0,  #(1*GRAIN_WIDTH + 3)*2

        vmlal.s16       q2,  d0,  d12
        vmlal.s16       q2,  d2,  d14
        vmlal.s16       q2,  d8,  d16
        vmlal.s16       q2,  d10, d18
        vmlal.s16       q3,  d1,  d12
        vmlal.s16       q3,  d3,  d14
        vmlal.s16       q3,  d9,  d16
        vmlal.s16       q3,  d11, d18

        vdup.8          d12, d27[4]
        vext.8          q0,  q11, q12, #10
        vdup.8          d14, d27[5]
        vext.8          q1,  q11, q12, #12
        vld1.8          {q11, q12}, [r12]
        vdup.8          d16, d27[6]
        vdup.8          d18, d27[7]
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14
        vext.8          q5,  q11, q12, #2
        vmovl.s8        q8,  d16
        vmovl.s8        q9,  d18

        vmlal.s16       q2,  d0,  d12
        vmlal.s16       q2,  d2,  d14
        vmlal.s16       q2,  d22, d16
        vmlal.s16       q2,  d10, d18
        vmlal.s16       q3,  d1,  d12
        vmlal.s16       q3,  d3,  d14
        vmlal.s16       q3,  d23, d16
        vmlal.s16       q3,  d11, d18

        vdup.8          d12, d28[0]
        vext.8          q0,  q11, q12, #4
        vdup.8          d14, d28[1]
        vext.8          q1,  q11, q12, #6
        vdup.8          d16, d28[2]
        vext.8          q4,  q11, q12, #8
        vdup.8          d18, d28[3]
        vext.8          q5,  q11, q12, #10
        vmovl.s8        q6,  d12
        vmovl.s8        q7,  d14
        vmovl.s8        q8,  d16
        vmovl.s8        q9,  d18

        vmlal.s16       q2,  d0,  d12
        vmlal.s16       q2,  d2,  d14
        vmlal.s16       q2,  d8,  d16
        vmlal.s16       q2,  d10, d18
        vmlal.s16       q3,  d1,  d12
        vmlal.s16       q3,  d3,  d14
        vmlal.s16       q3,  d9,  d16
        vmlal.s16       q3,  d11, d18

        vdup.8          d12, d28[4]
        vext.8          q0,  q11, q12, #12
        vmovl.s8        q6,  d12

        vmlal.s16       q2,  d0,  d12
        vmlal.s16       q3,  d1,  d12

        bx              lr
endfunc

.macro sum_lag3_func type, uv_layout, edge, elems=8
function sum_\type\()_lag3_\edge\()_neon
        push            {r1, lr}
        sum_lag_n_body  lag3, \type, \uv_layout, \edge, \elems, uv_coeff=d29[0]
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
        push            {r10-r11,lr}
1:
        mov             r10, #80
2:
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        subs            r10, r10, #8
        vst1.16         {q0}, [r0]!
        bgt             2b
        get_grain_2     d0
        subs            r1,  r1,  #1
        vst1.32         {d0[0]}, [r0]!
        bgt             1b
        pop             {r10-r11,pc}
endfunc

function generate_grain_rows_44_neon
        push            {r10-r11,lr}
1:
        mov             r10, #40
2:
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
        subs            r10, r10, #8
        vst1.16         {q0}, [r0]!
        bgt             2b
        get_grain_4     d0
        subs            r1,  r1,  #1
        vst1.16         {d0}, [r0]
        add             r0,  r0,  #GRAIN_WIDTH*2-80
        bgt             1b
        pop             {r10-r11,pc}
endfunc

function gen_grain_uv_444_lag0_neon
        vld1.16         {q3}, [r11]!
gen_grain_uv_lag0_8_start:
        push            {r11,lr}
        bl              get_gaussian_neon
        vrshl.s16       q0,  q0,  q15
gen_grain_uv_lag0_8_add:
        vand            q3,  q3,  q1
        vmull.s16       q2,  d6,  d22
        vmull.s16       q3,  d7,  d22
        vrshl.s32       q2,  q2,  q12
        vrshl.s32       q3,  q3,  q12
        vqmovn.s32      d4,  q2
        vqmovn.s32      d5,  q3
        vqadd.s16       q2,  q2,  q0
        vmin.s16        q2,  q2,  q9
        vmax.s16        q2,  q2,  q10
        vst1.16         {q2}, [r0]!
        pop             {r11,pc}
endfunc

function gen_grain_uv_420_lag0_8_neon
        add             r12, r11, #GRAIN_WIDTH*2
        vld1.16         {q2,q3}, [r11]!
        vld1.16         {q4,q5}, [r12]
        vpadd.i16       d4,  d4,  d5
        vpadd.i16       d5,  d6,  d7
        vpadd.i16       d8,  d8,  d9
        vpadd.i16       d9,  d10, d11
        vadd.i16        q2,  q2,  q4
        vrshr.s16       q3,  q2,  #2
        b               gen_grain_uv_lag0_8_start
endfunc

function gen_grain_uv_422_lag0_8_neon
        vld1.16         {q2,q3}, [r11]!
        vpadd.i16       d4,  d4,  d5
        vpadd.i16       d5,  d6,  d7
        vrshr.s16       q3,  q2,  #1
        b               gen_grain_uv_lag0_8_start
endfunc

function gen_grain_uv_420_lag0_4_neon
        add             r12, r11, #GRAIN_WIDTH*2
        vld1.16         {q2}, [r11]
        vld1.16         {q0}, [r12]
        add             r11, r11, #32
        vpadd.i16       d4,  d4,  d5
        vpadd.i16       d0,  d0,  d1
        vadd.i16        d4,  d4,  d0
        vrshr.s16       d6,  d4,  #2
        push            {r11,lr}
        get_grain_4     d0
        b               gen_grain_uv_lag0_8_add
endfunc

function gen_grain_uv_422_lag0_4_neon
        vld1.16         {q2}, [r11]
        add             r11, r11, #32
        vpadd.i16       d4,  d4,  d5
        vrshr.s16       d6,  d4,  #1
        push            {r11,lr}
        get_grain_4     d0
        b               gen_grain_uv_lag0_8_add
endfunc

.macro gen_grain_82 type
function generate_grain_\type\()_16bpc_neon, export=1
        push            {r4-r11,lr}

.ifc \type, uv_444
        ldr             r4,  [sp, #36]
        mov             r12, r3
        mov             lr,  #28
        add             r11, r1,  #3*GRAIN_WIDTH*2
        mov             r1,  r2
        mul             r12, r12, lr
        clz             lr,  r4
.else
        clz             lr,  r2
.endif
        movrel          r3,  X(gaussian_sequence)
        sub             lr,  lr,  #24 // -bitdepth_min_8
        ldr             r2,  [r1, #FGD_SEED]
        ldr             r9,  [r1, #FGD_GRAIN_SCALE_SHIFT]
.ifc \type, y
        add             r4,  r1,  #FGD_AR_COEFFS_Y
.else
        add             r4,  r1,  #FGD_AR_COEFFS_UV
.endif
        add             r9,  r9,  lr // grain_scale_shift - bitdepth_min_8
        adr             r5,  L(gen_grain_\type\()_tbl)
        ldr             r6,  [r1, #FGD_AR_COEFF_LAG]
        add             r9,  r9,  #4
        ldr             r6,  [r5, r6, lsl #2]
        vdup.16         q15, r9    // 4 - bitdepth_min_8 + data->grain_scale_shift
        add             r5,  r5,  r6
        vneg.s16        q15, q15

.ifc \type, uv_444
        push            {lr}
        cmp             r12, #0
        movw            r10, #0x49d8
        movw            lr,  #0xb524
        // Intentionally using a separate register instead of moveq with an
        // immediate constant, to avoid armv8 deprecated it instruction forms.
        it              eq
        moveq           r10, lr
        add             r4,  r4,  r12       // Add offset to ar_coeffs_uv[1]
        eor             r2,  r2,  r10
        pop             {lr}
.endif

        ldr             r7,  [r1, #FGD_AR_COEFF_SHIFT]
        neg             lr,  lr             // bitdepth_min_8
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
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
        mvn             r6,  r5             // grain_min = ~grain_max

        mov             r1,  #3
        bl              generate_grain_rows_neon
        mov             r1,  #GRAIN_HEIGHT-3

        vdup.32         q12, r7
        vld1.8          {d22[]}, [r4]       // ar_coeffs_uv[0]
        vmov.i8         q0,  #0
        vmov.i8         q1,  #255
        vdup.16         q9,  r5
        vdup.16         q10, r6
        vext.8          q13, q0,  q1,  #10
        vext.8          q14, q1,  q0,  #2
        vneg.s32        q12, q12
        vmovl.s8        q11, d22

1:
        vmov            q1,  q13
        bl              gen_grain_uv_444_lag0_neon // 8
        vmov.i8         q1,  #255
        bl              gen_grain_uv_444_lag0_neon // 16
        bl              gen_grain_uv_444_lag0_neon // 24
        bl              gen_grain_uv_444_lag0_neon // 32
        bl              gen_grain_uv_444_lag0_neon // 40
        bl              gen_grain_uv_444_lag0_neon // 48
        bl              gen_grain_uv_444_lag0_neon // 56
        bl              gen_grain_uv_444_lag0_neon // 64
        bl              gen_grain_uv_444_lag0_neon // 72
        vmov            q1,  q14
        bl              gen_grain_uv_444_lag0_neon // 80
        get_grain_2     d16
        subs            r1,  r1,  #1
        add             r11, r11, #4
        vst1.32         {d16[0]}, [r0]!
        bgt             1b
.endif
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag1):
        vpush           {q4-q7}
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
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
        vmovl.s8        q13, d27
        vmovl.s8        q12, d29
        vmovl.s8        q14, d28
        vmov            d29, d24
.ifc \type, uv_444
        vmovl.s8        q6,  d13
.endif

        mov             r1,  #GRAIN_HEIGHT - 3
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
        get_grain_2     d16
        subs            r1,  r1,  #1
.ifc \type, uv_444
        add             r11, r11, #4
.endif
        vst1.32         {d16[0]}, [r0]!
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag2):
        vpush           {q4-q7}
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
        vld1.8          {d28,d29}, [r4]     // ar_coeffs_y[0-11], ar_coeffs_uv[0-12]

        vmov.s8         r4,  d29[2]
        vmov.s8         r10, d29[3]

        mov             r1,  #3
        bl              generate_grain_rows_neon

        mov             r1,  #GRAIN_HEIGHT - 3
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
        get_grain_2     d16
        subs            r1,  r1,  #1
.ifc \type, uv_444
        add             r11, r11, #4
.endif
        vst1.32         {d16[0]}, [r0]!
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag3):
        vpush           {q4-q7}
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
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
        get_grain_2     d16
        subs            r1,  r1,  #1
.ifc \type, uv_444
        add             r11, r11, #4
.endif
        vst1.32         {d16[0]}, [r0]!
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
        add             \reg, \reg, #2*GRAIN_WIDTH*2-(6*32)
.else
        sub             \reg, \reg, #6*32-GRAIN_WIDTH*2
.endif
.endm

.macro gen_grain_44 type
function generate_grain_\type\()_16bpc_neon, export=1
        push            {r4-r11,lr}

        ldr             r4,  [sp, #36]
        mov             r12, r3
        movw            r11, #(3*GRAIN_WIDTH-3)*2
        mov             lr,  #28
        add             r11, r1,  r11
        mov             r1,  r2
        mul             r12, r12, lr
        clz             lr,  r4

        movrel          r3,  X(gaussian_sequence)
        sub             lr,  lr,  #24 // -bitdepth_min_8
        ldr             r2,  [r1, #FGD_SEED]
        ldr             r9,  [r1, #FGD_GRAIN_SCALE_SHIFT]
        add             r4,  r1,  #FGD_AR_COEFFS_UV
        add             r9,  r9,  lr // grain_scale_shift - bitdepth_min_8
        adr             r5,  L(gen_grain_\type\()_tbl)
        ldr             r6,  [r1, #FGD_AR_COEFF_LAG]
        add             r9,  r9,  #4
        ldr             r6,  [r5, r6, lsl #2]
        vdup.16         q15, r9    // 4 - bitdepth_min_8 + data->grain_scale_shift
        add             r5,  r5,  r6
        vneg.s16        q15, q15

        push            {lr}
        cmp             r12, #0
        movw            r10, #0x49d8
        movw            lr,  #0xb524
        // Intentionally using a separate register instead of moveq with an
        // immediate constant, to avoid armv8 deprecated it instruction forms.
        it              eq
        moveq           r10, lr
        add             r4,  r4,  r12       // Add offset to ar_coeffs_uv[1]
        eor             r2,  r2,  r10
        pop             {lr}

        ldr             r7,  [r1, #FGD_AR_COEFF_SHIFT]
        neg             lr,  lr
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
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
        mvn             r6,  r5             // grain_min = ~grain_max

        mov             r1,  #3
        bl              generate_grain_rows_44_neon
        set_height      r1,  \type

        vdup.32         q12, r7
        vld1.8          {d22[]}, [r4]       // ar_coeffs_uv[0]
        vmov.i8         q0,  #0
        vmov.i8         q1,  #255
        vdup.16         q9,  r5
        vdup.16         q10, r6
        vext.8          q13, q0,  q1,  #10
        vext.8          q14, q1,  q0,  #14
        vneg.s32        q12, q12
        vmovl.s8        q11, d22

1:
        vmov            q1,  q13
        bl              gen_grain_\type\()_lag0_8_neon // 8
        vmov.i8         q1,  #255
        bl              gen_grain_\type\()_lag0_8_neon // 16
        bl              gen_grain_\type\()_lag0_8_neon // 24
        bl              gen_grain_\type\()_lag0_8_neon // 32
        bl              gen_grain_\type\()_lag0_8_neon // 40
        vmov            q1,  q14
        bl              gen_grain_\type\()_lag0_4_neon // 44
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        add             r0,  r0,  #GRAIN_WIDTH*2-6*16
        bgt             1b

.ifc \type, uv_420
        vpop            {q4-q5}
.endif
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag1):
        vpush           {q4-q7}
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
        vld1.8          {d27[]}, [r4]!      // ar_coeffs_uv[0]
        vld1.8          {d28[]}, [r4]!      // ar_coeffs_uv[1]
        vld1.8          {d29[]}, [r4]       // ar_coeffs_uv[2]
        add             r4,  r4,  #2

        mov             r1,  #3
        vld1.8          {d13[]}, [r4]       // ar_coeffs_uv[4]
        ldrsb           r4,  [r4, #-1]      // ar_coeffs_uv[3]
        bl              generate_grain_rows_44_neon
        vmovl.s8        q13, d27
        vmovl.s8        q12, d29
        vmovl.s8        q14, d28
        vmov            d29, d24
        vmovl.s8        q6,  d13

        set_height      r1,  \type
1:
        bl              sum_\type\()_lag1_left_neon  // 8
        bl              sum_\type\()_lag1_mid_neon   // 16
        bl              sum_\type\()_lag1_mid_neon   // 24
        bl              sum_\type\()_lag1_mid_neon   // 32
        bl              sum_\type\()_lag1_mid_neon   // 40
        bl              sum_\type\()_lag1_right_neon // 44
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        add             r0,  r0,  #GRAIN_WIDTH*2-6*16
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag2):
        vpush           {q4-q7}
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
        vld1.8          {d28,d29}, [r4]     // ar_coeffs_uv[0-12]

        vmov.s8         r4,  d29[2]
        vmov.s8         r10, d29[3]

        mov             r1,  #3
        bl              generate_grain_rows_44_neon

        set_height      r1,  \type
1:
        bl              sum_\type\()_lag2_left_neon  // 8
        bl              sum_\type\()_lag2_mid_neon   // 16
        bl              sum_\type\()_lag2_mid_neon   // 24
        bl              sum_\type\()_lag2_mid_neon   // 32
        bl              sum_\type\()_lag2_mid_neon   // 40
        bl              sum_\type\()_lag2_right_neon // 44
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        add             r0,  r0,  #GRAIN_WIDTH*2-6*16
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}

L(generate_grain_\type\()_lag3):
        vpush           {q4-q7}
        mov             r5,  #128
        lsl             r5,  r5,  lr        //  128 << bitdepth_min_8
        sub             r5,  r5,  #1        // (128 << bitdepth_min_8) - 1
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
        bl              sum_\type\()_lag3_left_neon  // 8
        bl              sum_\type\()_lag3_mid_neon   // 16
        bl              sum_\type\()_lag3_mid_neon   // 24
        bl              sum_\type\()_lag3_mid_neon   // 32
        bl              sum_\type\()_lag3_mid_neon   // 40
        bl              sum_\type\()_lag3_right_neon // 44
        subs            r1,  r1,  #1
        increment_y_ptr r11, \type
        add             r0,  r0,  #GRAIN_WIDTH*2-6*16
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc
.endm

gen_grain_44 uv_420
gen_grain_44 uv_422

.macro gather_interleaved dst1, dst2, src1, src2, src3, src4, off
        vmov.u16        r11, \src1[0+\off]
        vmov.u16        r12, \src3[0+\off]
        add             r11, r11, r3
        vmov.u16        lr,  \src1[2+\off]
        add             r12, r12, r3
        vld1.8          {\dst1[0+\off]}, [r11]
        vmov.u16        r11, \src3[2+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst2[0+\off]}, [r12]
        vmov.u16        r12, \src2[0+\off]
        add             r11, r11, r3
        vld1.8          {\dst1[2+\off]}, [lr]
        vmov.u16        lr,  \src4[0+\off]
        add             r12, r12, r3
        vld1.8          {\dst2[2+\off]}, [r11]
        vmov.u16        r11, \src2[2+\off]
        add             lr,  lr,  r3
        vld1.8          {\dst1[4+\off]}, [r12]
        vmov.u16        r12, \src4[2+\off]
        add             r11, r11, r3
        vld1.8          {\dst2[4+\off]}, [lr]
        add             r12, r12, r3
        vld1.8          {\dst1[6+\off]}, [r11]
        vld1.8          {\dst2[6+\off]}, [r12]
.endm

.macro gather dst1, dst2, dst3, dst4, src1, src2, src3, src4, src5, src6, src7, src8
        gather_interleaved \dst1, \dst3, \src1, \src2, \src5, \src6, 0
        gather_interleaved \dst1, \dst3, \src1, \src2, \src5, \src6, 1
        gather_interleaved \dst2, \dst4, \src3, \src4, \src7, \src8, 0
        gather_interleaved \dst2, \dst4, \src3, \src4, \src7, \src8, 1
.endm

function gather32_neon
        push            {r11-r12,lr}
        gather          d8,  d9,  d10, d11, d0,  d1,  d2,  d3,  d4,  d5,  d6,  d7
        pop             {r11-r12,pc}
endfunc

function gather16_neon
        push            {r11-r12,lr}
        gather_interleaved d8,  d9,  d0,  d1,  d2,  d3,  0
        gather_interleaved d8,  d9,  d0,  d1,  d2,  d3,  1
        pop             {r11-r12,pc}
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
        mla             \dst, \stride, \offy, \src // grain_lut += grain_stride * offy
        add             \dst, \dst, \offx, lsl #1  // grain_lut += offx
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
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // scaling_shift, grain_lut
        ldrd            r6,  r7,  [sp, #108]   // offsets, h
        ldr             r8,       [sp, #116]   // clip
        mov             r9,  #GRAIN_WIDTH*2    // grain_lut stride
        ldr             r10,      [sp, #124]   // bitdepth_max

        eor             r4,  r4,  #15          // 15 - scaling_shift
        vdup.16         q6,  r10               // bitdepth_max
        clz             r10, r10
        vdup.16         q13, r4                // 15 - scaling_shift
        rsb             r10, r10, #24          // bitdepth_min_8
        cmp             r8,  #0
        vdup.16         q12, r10               // bitdepth_min_8

        movrel_local    r12, overlap_coeffs_0

        beq             1f
        // clip
        vmov.i16        q14, #16
        vmov.i16        q15, #235
        vshl.s16        q14, q14, q12
        vshl.s16        q15, q15, q12
        b               2f
1:
        // no clip
        vmov.i16        q14, #0
        vmov            q15, q6
2:
        vshr.u16        q6,  q6,  #1           // grain_max

        vld1.16         {d24, d25}, [r12, :128] // overlap_coeffs

        add             r5,  r5,  #18          // grain_lut += 9
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

        add             r4,  r4,  #32*2        // grain_lut += FG_BLOCK_SIZE * bx
        add             r6,  r11, r9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by

        ldr             r10, [sp, #120]        // type
        adr             r11, L(fgy_loop_tbl)

        tst             r10, #1
        ldr             r10, [r11, r10, lsl #2]

        add             r8,  r8,  r9,  lsl #5  // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             r8,  r8,  #32*2        // grain_lut += FG_BLOCK_SIZE * bx

        add             r11, r11, r10

        beq             1f
        // y overlap
        vdup.16         d14, d24[0]
        vdup.16         d15, d24[1]
        mov             r10, r7                // backup actual h
        mov             r7,  #2
1:
        sub             r2,  r2,  #32          // src_stride   -= 32
        sub             r9,  r9,  #32          // grain_stride -= 32
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
        vld1.16         {d0},       [r4],       r9 // grain_lut old
.endif
.if \oy
        vld1.16         {q2,  q3},  [r6]!          // grain_lut top
.endif
.if \ox && \oy
        vld1.16         {d2},       [r8],       r9 // grain_lut top old
.endif
.if \oy
        vld1.16         {q4,  q5},  [r6],       r9 // grain_lut top
.endif
.if !\ox && !\oy
        vld1.16         {q0,  q1},  [r1, :128]!    // src
.endif
        vld1.16         {q8,  q9},  [r5]!          // grain_lut
.if !\ox && !\oy
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
.endif
.if !\oy
        vmvn.i16        q5,  #0xf000               // 0x0fff
.endif
        vld1.16         {q10, q11}, [r5],       r9 // grain_lut

.if \ox
        add             r4,  r4,  #32
        vmull.s16       q0,  d0,  d24
        vmlal.s16       q0,  d16, d25
.endif

.if \oy
.if \ox
        add             r8,  r8,  #32
        vmull.s16       q1,  d2,  d24
        vmlal.s16       q1,  d4,  d25
        vqrshrn.s32     d16, q0,  #5
        vmvn            d0,  d12                   // grain_min
        vqrshrn.s32     d4,  q1,  #5
        vmin.s16        d16, d16, d12
        vmin.s16        d4,  d4,  d12
        vmax.s16        d16, d16, d0
        vmax.s16        d4,  d4,  d0
.endif

        vmull.s16       q0,  d4,  d14
        vmull.s16       q1,  d5,  d14
        vmull.s16       q2,  d6,  d14
        vmull.s16       q3,  d7,  d14
        vmlal.s16       q0,  d16, d15
        vmlal.s16       q1,  d17, d15
        vmlal.s16       q2,  d18, d15
        vmlal.s16       q3,  d19, d15
        vmull.s16       q8,  d20, d15
        vmull.s16       q9,  d21, d15
        vmull.s16       q10, d22, d15
        vmull.s16       q11, d23, d15
        vmlal.s16       q8,  d8,  d14
        vmlal.s16       q9,  d9,  d14
        vmlal.s16       q10, d10, d14
        vmlal.s16       q11, d11, d14
        vmvn            q4,  q6                   // grain_min
        vqrshrn.s32     d0,  q0,  #5
        vqrshrn.s32     d1,  q1,  #5
        vqrshrn.s32     d2,  q2,  #5
        vqrshrn.s32     d3,  q3,  #5
        vqrshrn.s32     d4,  q8,  #5
        vqrshrn.s32     d5,  q9,  #5
        vqrshrn.s32     d6,  q10, #5
        vqrshrn.s32     d7,  q11, #5
        vmin.s16        q8,  q0,  q6
        vmin.s16        q9,  q1,  q6
        vld1.16         {q0,  q1},  [r1, :128]!    // src
        vmin.s16        q10, q2,  q6
        vmin.s16        q11, q3,  q6
        vmax.s16        q8,  q8,  q4
        vmax.s16        q9,  q9,  q4
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
        vmvn.i16        q5,  #0xf000               // 0x0fff
        vmax.s16        q10, q10, q4
        vmax.s16        q11, q11, q4
.elseif \ox
        vmvn            d4,  d12                   // grain_min
        vqrshrn.s32     d16, q0,  #5
        vld1.16         {q0,  q1},  [r1, :128]!    // src
        vmin.s16        d16, d16, d12
        vmax.s16        d16, d16, d4
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
.endif

        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        vand            q0,  q0,  q5
        vand            q1,  q1,  q5
        vand            q2,  q2,  q5
        vand            q3,  q3,  q5

        bl              gather32_neon

.if \ox || \oy
        vpush           {q6-q7}
.endif

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vshl.u16        q6,  q6,  q13  // scaling << (15 - scaling_shift)
        vshl.u16        q7,  q7,  q13
        vshl.u16        q4,  q4,  q13
        vshl.u16        q5,  q5,  q13

        vqrdmulh.s16    q8,  q8,  q6   // round2((scaling << (15 - scaling_shift) * grain, 15)
        vqrdmulh.s16    q9,  q9,  q7
        vqrdmulh.s16    q10, q10, q4
        vqrdmulh.s16    q11, q11, q5

.if \ox || \oy
        vpop            {q6-q7}
.endif

        vqadd.s16       q0,  q0,  q8   // *src + noise
        vqadd.s16       q1,  q1,  q9
        vqadd.s16       q2,  q2,  q10
        vqadd.s16       q3,  q3,  q11

        vmax.s16        q0,  q0,  q14
        vmax.s16        q1,  q1,  q14
        vmax.s16        q2,  q2,  q14
        vmax.s16        q3,  q3,  q14
        vmin.s16        q0,  q0,  q15
        vmin.s16        q1,  q1,  q15
        vmin.s16        q2,  q2,  q15
        vmin.s16        q3,  q3,  q15

        vst1.16         {q0, q1}, [r0, :128]!    // dst
        subs            r7,  r7,  #1
.if \oy
        vdup.16         d14, d25[0]
        vdup.16         d15, d25[1]
.endif
        vst1.16         {q2, q3}, [r0, :128], r2 // dst
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
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]   // data, grain_lut
        ldrd            r10, r11, [sp, #124]   // uv, is_id
        ldr             r6,       [sp, #136]   // bitdepth_max

        clz             r7,  r6
        rsb             r7,  r7,  #24          // bitdepth_min_8

        // !csfl
        add             r10, r4,  r10, lsl #2  // + 4*uv
        add             r12, r10, #FGD_UV_LUMA_MULT
        add             lr,  r10, #FGD_UV_MULT
        ldrh            r10, [r10, #FGD_UV_OFFSET] // uv_offset
        vld1.16         {d30[]},  [r12]        // uv_luma_mult
        lsl             r10, r10, r7           // uv_offset << bitdepth_min_8
        vld1.16         {d30[1]}, [lr]         // uv_mult

        ldr             lr,  [r4, #FGD_SCALING_SHIFT]
        ldr             r12, [r4, #FGD_CLIP_TO_RESTRICTED_RANGE]
        eor             lr,  lr,  #15          // 15 - scaling_shift

        vmov.16         d30[2], r10            // uv_offset << bitdepth_min_8

        cmp             r12, #0
        vdup.16         q13, lr                // 15 - scaling_shift

        beq             1f
        // clip
        cmp             r11, #0
        mov             r8,  #16
        mov             r9,  #240
        lsl             r8,  r8,  r7
        lsl             r9,  r9,  r7
        beq             2f
        // is_id
        mov             r9,  #235
        lsl             r9,  r9,  r7
        b               2f
1:
        // no clip
        mov             r8,  #0
        mov             r9,  r6                // bitdepth_max
2:
        vmov.16         d30[3], r6             // bitdepth_max
        vdup.16         d31, r8                // clip_min

        mov             r10, #GRAIN_WIDTH*2    // grain_lut stride

.if \sy
        mov             r6,  #23
        mov             r7,  #22
.else
        mov             r6,  #27
        mov             r7,  #17
.endif
        vmov.16         d31[1], r9             // clip_max

        ldrd            r8,  r9,  [sp, #116]   // offsets, h

        add             r5,  r5,  #(2*(3 + (2 >> \sx)*3)) // grain_lut += 9 or 6
.if \sy
        add             r5,  r5,  r10, lsl #2  // grain_lut += 4 * grain_stride
        add             r5,  r5,  r10, lsl #1  // grain_lut += 2 * grain_stride
.else
        add             r5,  r5,  r10, lsl #3  // grain_lut += 8 * grain_stride
        add             r5,  r5,  r10          // grain_lut += grain_stride
.endif
        vmov.16         d31[2], r6             // overlap y [0]

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

        vmov.16         d31[3], r7             // overlap y [1]

        add             r4,  r4,  #2*(32 >> \sx)      // grain_lut += FG_BLOCK_SIZE * bx
        add             r8,  lr,  r10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             r11, r11, r10, lsl #(5 - \sy) // grain_lut += grain_stride * FG_BLOCK_SIZE * by
        add             r11, r11, #2*(32 >> \sx)      // grain_lut += FG_BLOCK_SIZE * bx

        movrel_local    r12, overlap_coeffs_\sx
        ldr             lr,       [sp, #132]   // type
        ldrd            r6,  r7,  [sp, #108]   // luma_row, luma_stride

        vld1.16         {d24, d25}, [r12, :128] // overlap_coeffs

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
        add             r7,  r7,  r7           // luma_stride *= 2
.endif
        sub             r7,  r7,  #32          // luma_stride -= 32

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
        sub             r2,  r2,  #32          // src_stride   -= 32
        sub             r10, r10, #32          // grain_stride -= 32
.if \oy
        mov             r12, lr
.endif
L(fguv_loop_sx0_csfl\csfl\()_\ox\oy\()_loopstart):
1:
.if \ox
        vld1.16         {d0},       [r4],       r10 // grain_lut old
.endif
.if \oy
        vld1.16         {q2,  q3},  [r8]!           // grain_lut top
.endif
.if \ox && \oy
        vld1.16         {d2},       [r11],      r10 // grain_lut top old
.endif
.if !\ox && !\oy
        vld1.16         {q0,  q1},  [r6, :128]!     // luma
.endif
        vld1.16         {q8,  q9},  [r5]!           // grain_lut
.if \oy
        vld1.16         {q4,  q5},  [r8],       r10 // grain_lut top
.endif
.if !\ox && !\oy
        vld1.16         {q2,  q3},  [r6, :128], r7  // luma
.endif
.if \oy
        vdup.16         d28, d31[2]                 // overlap y coeff
        vdup.16         d29, d31[3]                 // overlap y coeff
.endif
        vld1.16         {q10, q11}, [r5],       r10 // grain_lut

.if \ox
        vdup.16         q7,  d30[3]                // bitdepth_max
        add             r4,  r4,  #32
        vmull.s16       q0,  d0,  d24
        vshr.u16        q7,  q7,  #1               // grain_max
        vmlal.s16       q0,  d16, d25
        vmvn            q6,  q7                    // grain_min
.endif

.if \oy
.if \ox
        add             r11, r11, #32
        vmull.s16       q1,  d2,  d24
        vmlal.s16       q1,  d4,  d25
        vqrshrn.s32     d16, q0,  #5
        vqrshrn.s32     d4,  q1,  #5
        vmin.s16        d4,  d4,  d14
        vmin.s16        d16, d16, d14
        vmax.s16        d4,  d4,  d12
        vmax.s16        d16, d16, d12
.endif

        vmull.s16       q0,  d4,  d28
        vmull.s16       q1,  d5,  d28
        vmull.s16       q2,  d6,  d28
        vmull.s16       q3,  d7,  d28
.if !\ox
        vdup.16         q7,  d30[3]                // bitdepth_max
.endif
        vmlal.s16       q0,  d16, d29
        vmlal.s16       q1,  d17, d29
        vmlal.s16       q2,  d18, d29
        vmlal.s16       q3,  d19, d29
.if !\ox
        vshr.u16        q7,  q7,  #1               // grain_max
.endif
        vmull.s16       q8,  d20, d29
        vmull.s16       q9,  d21, d29
        vmull.s16       q10, d22, d29
        vmull.s16       q11, d23, d29
.if !\ox
        vmvn            q6,  q7                    // grain_min
.endif
        vmlal.s16       q8,  d8,  d28
        vmlal.s16       q9,  d9,  d28
        vmlal.s16       q10, d10, d28
        vmlal.s16       q11, d11, d28
        vqrshrn.s32     d0,  q0,  #5
        vqrshrn.s32     d1,  q1,  #5
        vqrshrn.s32     d2,  q2,  #5
        vqrshrn.s32     d3,  q3,  #5
        vqrshrn.s32     d4,  q8,  #5
        vqrshrn.s32     d5,  q9,  #5
        vqrshrn.s32     d6,  q10, #5
        vqrshrn.s32     d7,  q11, #5
        vmin.s16        q8,  q0,  q7
        vmin.s16        q9,  q1,  q7
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        q10, q2,  q7
        vmin.s16        q11, q3,  q7
        vmax.s16        q8,  q8,  q6
        vmax.s16        q9,  q9,  q6
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
        vmax.s16        q10, q10, q6
        vmax.s16        q11, q11, q6
.elseif \ox
        vqrshrn.s32     d16, q0,  #5
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        d16, d16, d14
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
        vmax.s16        d16, d16, d12
.endif

.if !\csfl
        vdup.16         d28, d30[0]   // uv_luma_mult
        vld1.16         {q4,  q5},  [r1, :128]! // src
        vdup.16         d29, d30[1]   // uv_mult
        vmull.s16       q6,  d0,  d28
        vmull.s16       q7,  d1,  d28
        vmull.s16       q0,  d2,  d28
        vmull.s16       q1,  d3,  d28
        vmlal.s16       q6,  d8,  d29
        vmlal.s16       q7,  d9,  d29
        vmlal.s16       q0,  d10, d29
        vmlal.s16       q1,  d11, d29
        vld1.16         {q4,  q5},  [r1, :128]  // src
        sub             r1,  r1,  #32
        vshrn.s32       d12, q6,  #6
        vshrn.s32       d13, q7,  #6
        vshrn.s32       d14, q0,  #6
        vshrn.s32       d15, q1,  #6
        vmull.s16       q0,  d4,  d28
        vmull.s16       q1,  d5,  d28
        vmull.s16       q2,  d6,  d28
        vmull.s16       q3,  d7,  d28
        vmlal.s16       q0,  d8,  d29
        vmlal.s16       q1,  d9,  d29
        vmlal.s16       q2,  d10, d29
        vmlal.s16       q3,  d11, d29
        vdup.16         q14, d30[2]   // uv_offset
        vshrn.s32       d0,  q0,  #6
        vshrn.s32       d1,  q1,  #6
        vshrn.s32       d2,  q2,  #6
        vshrn.s32       d3,  q3,  #6
        vdup.16         q4,  d30[3]   // bitdepth_max
        vmov.i16        q5,  #0
        vadd.i16        q6,  q6,  q14
        vadd.i16        q7,  q7,  q14
        vadd.i16        q2,  q0,  q14
        vadd.i16        q3,  q1,  q14
        vmin.s16        q0,  q6,  q4
        vmin.s16        q1,  q7,  q4
        vmin.s16        q2,  q2,  q4
        vmin.s16        q3,  q3,  q4
        vmax.s16        q0,  q0,  q5
        vmax.s16        q1,  q1,  q5
        vmax.s16        q2,  q2,  q5
        vmax.s16        q3,  q3,  q5
.else
        vdup.16         q14, d30[3]  // bitdepth_max
        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        vand            q0,  q0,  q14
        vand            q1,  q1,  q14
        vand            q2,  q2,  q14
        vand            q3,  q3,  q14
.endif

        bl              gather32_neon

        vld1.16         {q0,  q1},  [r1, :128]!    // src

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9
        vmovl.u8        q4,  d10
        vmovl.u8        q5,  d11

        vld1.16         {q2,  q3},  [r1, :128], r2 // src

        vshl.u16        q6,  q6,  q13  // scaling << (15 - scaling_shift)
        vshl.u16        q7,  q7,  q13
        vshl.u16        q4,  q4,  q13
        vshl.u16        q5,  q5,  q13

        vqrdmulh.s16    q8,  q8,  q6   // round2((scaling << (15 - scaling_shift) * grain, 15)
        vqrdmulh.s16    q9,  q9,  q7
        vqrdmulh.s16    q10, q10, q4
        vqrdmulh.s16    q11, q11, q5


        vdup.16         q4,  d31[0]    // clip_min
        vdup.16         q5,  d31[1]    // clip_max

        vqadd.s16       q0,  q0,  q8   // *src + noise
        vqadd.s16       q1,  q1,  q9
        vqadd.s16       q2,  q2,  q10
        vqadd.s16       q3,  q3,  q11

.if \oy
        vmov.32         lr,  d25[0] // 2 first 16 bit coeffs from overlap x
.endif

        vmax.s16        q0,  q0,  q4
        vmax.s16        q1,  q1,  q4
        vmax.s16        q2,  q2,  q4
        vmax.s16        q3,  q3,  q4
        vmin.s16        q0,  q0,  q5
        vmin.s16        q1,  q1,  q5
        vmin.s16        q2,  q2,  q5
        vmin.s16        q3,  q3,  q5

        vst1.16         {q0, q1}, [r0, :128]! // dst

        subs            r9,  r9,  #1
.if \oy
        vmov.32         d31[1], lr  // new coeffs for overlap y
.endif

        vst1.16         {q2, q3}, [r0, :128], r2 // dst
        bgt             1b

.if \oy
        cmp             r12, #0
        mov             r9,  r12               // restore actual remaining h
        bgt             L(fguv_loop_sx0_csfl\csfl\()_\ox\()0_loopstart)
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
        vld1.16         {d0},       [r4],       r10 // grain_lut old
.endif
.if \ox && \oy
        vld1.16         {d2},       [r11],      r10 // grain_lut top old
.endif
.if \oy
        vld1.16         {q2,  q3},  [r8],       r10 // grain_lut top
.endif
.if !\ox && !\oy
        vld1.16         {q0,  q1},  [r6, :128]!     // luma
.endif
        vld1.16         {q8,  q9},  [r5],       r10 // grain_lut
.if \oy
        vdup.16         d28, d31[2]                 // overlap y coeff
        vdup.16         d29, d31[3]                 // overlap y coeff
.endif
.if !\ox && !\oy
        vld1.16         {q2,  q3},  [r6, :128], r7  // luma
.endif

.if \ox
        vdup.16         q7,  d30[3]                // bitdepth_max
        vmull.s16       q0,  d0,  d24
        vshr.u16        q7,  q7,  #1               // grain_max
        vmlal.s16       q0,  d16, d25
        vmvn            q6,  q7                    // grain_min
.endif

.if \oy
.if \ox
        vmull.s16       q1,  d2,  d24
        vmlal.s16       q1,  d4,  d25
        vqrshrn.s32     d16, q0,  #5
        vqrshrn.s32     d4,  q1,  #5
        vmin.s16        d4,  d4,  d14
        vmin.s16        d16, d16, d14
        vmax.s16        d4,  d4,  d12
        vmax.s16        d16, d16, d12
.endif

        vmull.s16       q0,  d4,  d28
        vmull.s16       q1,  d5,  d28
        vmull.s16       q2,  d6,  d28
        vmull.s16       q3,  d7,  d28
.if !\ox
        vdup.16         q7,  d30[3]                // bitdepth_max
.endif
        vmlal.s16       q0,  d16, d29
        vmlal.s16       q1,  d17, d29
        vmlal.s16       q2,  d18, d29
        vmlal.s16       q3,  d19, d29
.if !\ox
        vshr.u16        q7,  q7,  #1               // grain_max
.endif
        vqrshrn.s32     d16, q0,  #5
        vqrshrn.s32     d17, q1,  #5
        vqrshrn.s32     d18, q2,  #5
        vqrshrn.s32     d19, q3,  #5
.if !\ox
        vmvn            q6,  q7                    // grain_min
.endif
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        q8,  q8,  q7
        vmin.s16        q9,  q9,  q7
        vmax.s16        q8,  q8,  q6
        vmax.s16        q9,  q9,  q6
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
.elseif \ox
        vqrshrn.s32     d16, q0,  #5
        vld1.16         {q0,  q1},  [r6, :128]!    // luma
        vmin.s16        d16, d16, d14
        vld1.16         {q2,  q3},  [r6, :128], r7 // luma
        vmax.s16        d16, d16, d12
.endif

        vpadd.i16       d0,  d0,  d1
        vpadd.i16       d1,  d2,  d3
        vpadd.i16       d2,  d4,  d5
        vpadd.i16       d3,  d6,  d7
        vrshr.u16       q0,  q0,  #1
        vrshr.u16       q1,  q1,  #1
.if !\csfl
        vdup.16         d28, d30[0]   // uv_luma_mult
        vld1.16         {q2,  q3},  [r1, :128], r2 // src
        vdup.16         d29, d30[1]   // uv_mult
        vmull.s16       q6,  d0,  d28
        vmull.s16       q7,  d1,  d28
        vmull.s16       q0,  d2,  d28
        vmull.s16       q1,  d3,  d28
        vmlal.s16       q6,  d4,  d29
        vmlal.s16       q7,  d5,  d29
        vmlal.s16       q0,  d6,  d29
        vmlal.s16       q1,  d7,  d29
        vshrn.s32       d12, q6,  #6
        vshrn.s32       d13, q7,  #6
        vshrn.s32       d14, q0,  #6
        vshrn.s32       d15, q1,  #6
        vdup.16         q14, d30[2]   // uv_offset
        vdup.16         q4,  d30[3]   // bitdepth_max
        vmov.i16        q5,  #0
        vadd.i16        q6,  q6,  q14
        vadd.i16        q7,  q7,  q14
        vmin.s16        q0,  q6,  q4
        vmin.s16        q1,  q7,  q4
        vmax.s16        q0,  q0,  q5
        vmax.s16        q1,  q1,  q5
.else
        vdup.16         q14, d30[3]  // bitdepth_max
        vld1.16         {q2,  q3},  [r1, :128], r2 // src

        // Make sure that uninitialized pixels out of range past the right
        // edge are in range; their actual values shouldn't matter.
        vand            q0,  q0,  q14
        vand            q1,  q1,  q14
.endif

        bl              gather16_neon

        vmovl.u8        q6,  d8        // scaling
        vmovl.u8        q7,  d9

        vshl.u16        q6,  q6,  q13  // scaling << (15 - scaling_shift)
        vshl.u16        q7,  q7,  q13

        vqrdmulh.s16    q8,  q8,  q6   // round2((scaling << (15 - scaling_shift) * grain, 15)
        vqrdmulh.s16    q9,  q9,  q7


        vdup.16         q4,  d31[0]    // clip_min
        vdup.16         q5,  d31[1]    // clip_max

        vqadd.s16       q0,  q2,  q8   // *src + noise
        vqadd.s16       q1,  q3,  q9

.if \oy
        // Swap the two last coefficients of d31, place them first in d28
        vrev64.16       d28, d31
.endif

        vmax.s16        q0,  q0,  q4
        vmax.s16        q1,  q1,  q4
        vmin.s16        q0,  q0,  q5
        vmin.s16        q1,  q1,  q5

        subs            r9,  r9,  #1
.if \oy
        // Take the first two 16 bit coefficients of d28 and place them at the
        // end of d31
        vtrn.32         d31, d28
.endif

        vst1.16         {q0, q1}, [r0, :128], r2 // dst
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
