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

// void dav1d_sgr_box3_row_v_neon(int32_t **sumsq, int16_t **sum,
//                                int32_t *sumsq_out, int16_t *sum_out,
//                                const int w);
function sgr_box3_row_v_neon, export=1
        push            {r4-r9,lr}
        ldr             r4,  [sp, #28]
        ldrd            r6,  r7,  [r0]
        ldr             r0,       [r0, #8]
        add             r4,  r4,  #2
        ldrd            r8,  r9,  [r1]
        ldr             r1,       [r1, #8]

1:
        vld1.32         {q8,  q9},  [r6]!
        vld1.32         {q10, q11}, [r7]!
        vld1.16         {q14},      [r8]!
        vld1.16         {q15},      [r9]!
        subs            r4,  r4,  #8

        vadd.i32        q8, q8, q10
        vadd.i32        q9, q9, q11

        vld1.32         {q12, q13}, [r0]!

        vadd.i16        q14, q14, q15

        vld1.16         {q15},      [r1]!
        vadd.i32        q8,  q8,  q12
        vadd.i32        q9,  q9,  q13
        vadd.i16        q14, q14, q15

        vst1.32         {q8,  q9},  [r2]!
        vst1.16         {q14},      [r3]!

        bgt             1b
        pop             {r4-r9,pc}
endfunc

// void dav1d_sgr_box5_row_v_neon(int32_t **sumsq, int16_t **sum,
//                                int32_t *sumsq_out, int16_t *sum_out,
//                                const int w);
function sgr_box5_row_v_neon, export=1
        push            {r4-r11,lr}
        ldr             lr,  [sp, #36]

        ldrd            r4,  r5,  [r0]
        ldrd            r6,  r7,  [r0, #8]
        ldr             r0,       [r0, #16]
        add             lr,  lr,  #2
        ldrd            r8,  r9,  [r1]
        ldrd            r10, r11, [r1, #8]
        ldr             r1,       [r1, #16]

1:
        vld1.32         {q8,  q9},  [r4]!
        vld1.32         {q10, q11}, [r5]!
        vld1.32         {q12, q13}, [r6]!
        vld1.32         {q14, q15}, [r7]!
        vld1.16         {q0},       [r8]!
        vld1.16         {q1},       [r9]!
        vld1.16         {q2},       [r10]!
        vld1.16         {q3},       [r11]!
        subs            lr,  lr,  #8

        vadd.i32        q8,  q8,  q10
        vadd.i32        q9,  q9,  q11
        vadd.i32        q12, q12, q14
        vadd.i32        q13, q13, q15

        vld1.32         {q14, q15}, [r0]!

        vadd.i16        q0,  q0,  q1
        vadd.i16        q2,  q2,  q3

        vld1.16         {q3},       [r1]!
        vadd.i32        q8,  q8,  q12
        vadd.i32        q9,  q9,  q13
        vadd.i16        q0,  q0,  q2

        vadd.i32        q8,  q8,  q14
        vadd.i32        q9,  q9,  q15
        vadd.i16        q0,  q0,  q3

        vst1.32         {q8,  q9},  [r2]!
        vst1.16         {q0},       [r3]!

        bgt             1b
        pop             {r4-r11,pc}
endfunc

// void dav1d_sgr_calc_row_ab1_neon(int32_t *a, int16_t *b,
//                                  const int w, const int strength,
//                                  const int bitdepth_max);
// void dav1d_sgr_calc_row_ab2_neon(int32_t *a, int16_t *b,
//                                  const int w, const int strength,
//                                  const int bitdepth_max);
function sgr_calc_row_ab1_neon, export=1
        push            {r4-r7,lr}
        vpush           {q4-q7}
        ldr             r4,  [sp, #84]
        clz             r6,  r4
        vmov.i32        q15, #9        // n
        movw            r5,  #455
        b               sgr_calc_ab_neon
endfunc

function sgr_calc_row_ab2_neon, export=1
        push            {r4-r7,lr}
        vpush           {q4-q7}
        ldr             r4,  [sp, #84]
        clz             r6,  r4
        vmov.i32        q15, #25       // n
        mov             r5,  #164
endfunc

function sgr_calc_ab_neon
        movrel          r12, X(sgr_x_by_x)
        sub             r6,  r6,  #24  // -bitdepth_min_8
        vld1.8          {q8, q9}, [r12, :128]!
        add             r7,  r6,  r6   // -2*bitdepth_min_8
        vmov.i8         q11, #5
        vmov.i8         d10, #55       // idx of last 5
        vld1.8          {q10},    [r12, :128]
        vmov.i8         d11, #72       // idx of last 4
        vmov.i8         d12, #101      // idx of last 3
        vmov.i8         d13, #169      // idx of last 2
        vmov.i8         d14, #254      // idx of last 1
        vmov.i8         d15, #32       // elements consumed in first vtbl
        add             r2,  r2,  #2   // w += 2
        vdup.32         q12, r3
        vsub.i8         q8,  q8,  q11
        vsub.i8         q9,  q9,  q11
        vsub.i8         q10, q10, q11
        vdup.32         q13, r7        // -2*bitdepth_min_8
1:
        vld1.32         {q0, q1}, [r0, :128] // a
        vld1.16         {q2},     [r1, :128] // b
        vdup.16         q14, r6        // -bitdepth_min_8
        subs            r2,  r2,  #8
        vrshl.s32       q0,  q0,  q13
        vrshl.s32       q1,  q1,  q13
        vrshl.s16       q4,  q2,  q14
        vmul.i32        q0,  q0,  q15  // a * n
        vmul.i32        q1,  q1,  q15  // a * n
        vmull.u16       q3,  d8,  d8   // b * b
        vmull.u16       q4,  d9,  d9   // b * b
        vqsub.u32       q0,  q0,  q3   // imax(a * n - b * b, 0)
        vqsub.u32       q1,  q1,  q4   // imax(a * n - b * b, 0)
        vmul.i32        q0,  q0,  q12  // p * s
        vmul.i32        q1,  q1,  q12  // p * s
        vqshrn.u32      d0,  q0,  #16
        vqshrn.u32      d1,  q1,  #16
        vqrshrn.u16     d0,  q0,  #4   // imin(z, 255)

        vcgt.u8         d2,  d0,  d10  // = -1 if sgr_x_by_x[d0] < 5
        vcgt.u8         d3,  d0,  d11  // = -1 if sgr_x_by_x[d0] < 4
        vtbl.8          d1,  {q8, q9}, d0
        vcgt.u8         d6,  d0,  d12  // = -1 if sgr_x_by_x[d0] < 3
        vsub.i8         d9,  d0,  d15  // indices for vtbx
        vcgt.u8         d7,  d0,  d13  // = -1 if sgr_x_by_x[d0] < 2
        vadd.i8         d2,  d2,  d3
        vtbx.8          d1,  {q10}, d9
        vcgt.u8         d8,  d0,  d14  // = -1 if sgr_x_by_x[d0] < 1
        vadd.i8         d6,  d6,  d7
        vadd.i8         d8,  d8,  d22
        vadd.i8         d2,  d2,  d6
        vadd.i8         d1,  d1,  d8
        vadd.i8         d1,  d1,  d2
        vmovl.u8        q0,  d1        // x

        vdup.32         q14, r5        // one_by_x

        vmull.u16       q1,  d0,  d4   // x * BB[i]
        vmull.u16       q2,  d1,  d5   // x * BB[i]
        vmul.i32        q1,  q1,  q14  // x * BB[i] * sgr_one_by_x
        vmul.i32        q2,  q2,  q14  // x * BB[i] * sgr_one_by_x
        vrshr.s32       q1,  q1,  #12  // AA[i]
        vrshr.s32       q2,  q2,  #12  // AA[i]

        vst1.32         {q1, q2}, [r0, :128]!
        vst1.16         {q0},     [r1, :128]!
        bgt             1b

        vpop            {q4-q7}
        pop             {r4-r7,pc}
endfunc
