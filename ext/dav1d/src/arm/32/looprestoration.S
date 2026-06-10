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

const right_ext_mask_buf
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        .byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
right_ext_mask:
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        .byte 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
endconst

// void dav1d_wiener_filter_h_8bpc_neon(int16_t *dst, const pixel (*left)[4],
//                                      const pixel *src, const int16_t fh[8],
//                                      const int w,
//                                      const enum LrEdgeFlags edges);
function wiener_filter_h_8bpc_neon, export=1
        push            {r4-r5,lr}
        ldrd            r4,  r5,  [sp, #12]
        vld1.16         {q0},  [r3, :128]
        movw            r12, #(1 << 14) - (1 << 2)
        vdup.16         q14, r12
        vmov.s16        q15, #2048

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        // LR_HAVE_LEFT
        cmp             r1,  #0
        bne             0f
        // left == NULL
        sub             r2,  r2,  #3
        vld1.8          {q2},  [r2]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q2},  [r2]!
        vld1.32         {d3[1]},  [r1]
        // Move r2 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             r2,  r2,  #3
        vext.8          q2,  q1,  q2,  #13
        b               2f

1:
        vld1.8          {q2},  [r2]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q2 to have 3x the first byte at the front.
        vdup.8          q1,  d4[0]
        // Move r2 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             r2,  r2,  #3
        vext.8          q2,  q1,  q2,  #13

2:
        vmovl.u8        q1,  d4
        vmovl.u8        q2,  d5

        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+3 pixels valid in q1-q2. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is q1/2.h[w+2]. r2 points at the next input, ie
        // q1/2.h[16]. Thus read from r2[w-14] to find the padding pixel.
        sub             r12, r4,  #14
        // Insert padding in q1/2.h[w+3] onwards; fuse the +3 (*2) into the
        // buffer pointer.
        movrel_local    r3,  right_ext_mask, -6
        ldrb            r12, [r2, r12]
        sub             r3,  r3,  r4,  lsl #1
        vdup.16         q13, r12
        vld1.8          {q10, q11}, [r3]

        vbit            q1,  q13, q10
        vbit            q2,  q13, q11

4:      // Loop horizontally
        vext.8          q10, q1,  q2,  #4
        vext.8          q11, q1,  q2,  #8
        vext.8          q9,  q1,  q2,  #2
        vext.8          q12, q1,  q2,  #10
        vext.8          q13, q1,  q2,  #12
        vext.8          q8,  q1,  q2,  #6
        vadd.i16        q10, q10, q11
        vadd.i16        q9,  q9,  q12
        vadd.i16        q13, q13, q1
        vshl.s16        q1,  q8,  #7
        vmul.s16        q3,  q8,  d0[3]
        vmla.s16        q3,  q10, d1[0]
        vmla.s16        q3,  q9,  d1[1]
        vmla.s16        q3,  q13, d1[2]

        vsub.s16        q1,  q1,  q14
        vqadd.s16       q3,  q3,  q1
        vshr.s16        q3,  q3,  #3
        vadd.s16        q3,  q3,  q15
        subs            r4,  r4,  #8
        vst1.16         {q3},  [r0,  :128]!

        ble             9f
        vmov            q1,  q2
        vld1.8          {d4},  [r2]!
        tst             r5,  #2 // LR_HAVE_RIGHT
        vmovl.u8        q2,  d4
        bne             4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r5,pc}
endfunc

// void dav1d_wiener_filter_v_8bpc_neon(pixel *dst, int16_t **ptrs,
//                                      const int16_t fv[8], const int w);
function wiener_filter_v_8bpc_neon, export=1
        push            {r4-r9,lr}
        vpush           {q4-q6}

        vld1.16         {q0},  [r2, :128]

        ldrd            r4,  r5,  [r1]
        ldrd            r6,  r7,  [r1, #8]
        ldrd            r8,  r9,  [r1, #16]

1:
        vld1.16         {q1,  q2},  [r4, :128]!
        vld1.16         {q8,  q9},  [r9, :128]!

        vld1.16         {q5,  q6},  [r5, :128]!

        vld1.16         {q10, q11}, [r6, :128]!
        vld1.16         {q12, q13}, [r8, :128]!

        vld1.16         {q14, q15}, [r7, :128]!

        subs            r3,  r3,  #16

        vadd.i16        q1,  q1,  q8
        vadd.i16        q2,  q2,  q9

        vadd.i16        q5,  q5,  q8
        vadd.i16        q6,  q6,  q9

        vadd.i16        q10, q10, q12
        vadd.i16        q11, q11, q13

        vmull.s16       q3,  d28, d0[3]
        vmlal.s16       q3,  d2,  d0[0]
        vmlal.s16       q3,  d10, d0[1]
        vmlal.s16       q3,  d20, d0[2]

        vmull.s16       q4,  d29, d0[3]
        vmlal.s16       q4,  d3,  d0[0]
        vmlal.s16       q4,  d11, d0[1]
        vmlal.s16       q4,  d21, d0[2]

        vmull.s16       q8,  d30, d0[3]
        vmlal.s16       q8,  d4,  d0[0]
        vmlal.s16       q8,  d12, d0[1]
        vmlal.s16       q8,  d22, d0[2]

        vmull.s16       q9,  d31, d0[3]
        vmlal.s16       q9,  d5,  d0[0]
        vmlal.s16       q9,  d13, d0[1]
        vmlal.s16       q9,  d23, d0[2]

        vqrshrun.s32    d6,  q3,  #11
        vqrshrun.s32    d7,  q4,  #11
        vqrshrun.s32    d16, q8,  #11
        vqrshrun.s32    d17, q9,  #11
        vqmovun.s16     d6,  q3
        vqmovun.s16     d7,  q8
        vst1.8          {q3}, [r0, :128]!
        bgt             1b

        // Shift the pointers, but only update the first 5; the 6th pointer is
        // kept as it was before (and the 7th is implicitly identical to the
        // 6th).
        ldrd            r4,  r5,  [r1, #4]
        ldrd            r6,  r7,  [r1, #12]
        ldr             r8,       [r1, #20]
        strd            r4,  r5,  [r1]
        strd            r6,  r7,  [r1, #8]
        str             r8,       [r1, #16]

        vpop            {q4-q6}
        pop             {r4-r9,pc}
endfunc

// void dav1d_wiener_filter_hv_8bpc_neon(pixel *dst, const pixel (*left)[4],
//                                       const pixel *src,
//                                       const int16_t filter[2][8],
//                                       const int w,
//                                       const enum LrEdgeFlags edges,
//                                       int16_t **ptrs);
function wiener_filter_hv_8bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldr             lr,       [sp, #108]
        vld1.16         {q0, q1}, [r3, :128]
        movw            r12, #(1 << 14) - (1 << 2)
        vdup.16         q14, r12
        vmov.s16        q15, #2048

        ldrd            r6,  r7,  [lr]
        ldrd            r8,  r9,  [lr, #8]
        ldrd            r10, r11, [lr, #16]
        ldr             r12,      [lr, #24]

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        // LR_HAVE_LEFT
        cmp             r1,  #0
        bne             0f
        // left == NULL
        sub             r2,  r2,  #3
        vld1.8          {q2},  [r2]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q2},  [r2]!
        vld1.32         {d3[1]},  [r1]
        // Move r2 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             r2,  r2,  #3
        vext.8          q2,  q1,  q2,  #13
        b               2f

1:
        vld1.8          {q2},  [r2]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q2 to have 3x the first byte at the front.
        vdup.8          q3,  d4[0]
        // Move r2 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             r2,  r2,  #3
        vext.8          q2,  q3,  q2,  #13

2:
        vmovl.u8        q3,  d5
        vmovl.u8        q2,  d4

        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+3 pixels valid in q1-q2. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is q1/2.h[w+2]. r2 points at the next input, ie
        // q1/2.h[16]. Thus read from r2[w-14] to find the padding pixel.
        sub             lr,  r4,  #14
        // Insert padding in q1/2.h[w+3] onwards; fuse the +3 (*2) into the
        // buffer pointer.
        movrel_local    r3,  right_ext_mask, -6
        ldrb            lr,  [r2, lr]
        sub             r3,  r3,  r4,  lsl #1
        vdup.16         q13, lr
        vld1.8          {q10, q11}, [r3]

        vbit            q2,  q13, q10
        vbit            q3,  q13, q11

4:      // Loop horizontally
        vext.8          q10, q2,  q3,  #4
        vext.8          q11, q2,  q3,  #8
        vext.8          q9,  q2,  q3,  #2
        vext.8          q12, q2,  q3,  #10
        vext.8          q13, q2,  q3,  #12
        vext.8          q8,  q2,  q3,  #6
        vadd.i16        q10, q10, q11
        vadd.i16        q9,  q9,  q12
        vadd.i16        q13, q13, q2
        vld1.16         {q6},   [r7,  :128]!
        vshl.s16        q2,  q8,  #7
        vld1.16         {q11},  [r11, :128]!
        vsub.s16        q2,  q2,  q14
        vld1.16         {q7},   [r8,  :128]!
        vmul.s16        q4,  q8,  d0[3]
        vmla.s16        q4,  q10, d1[0]
        vmla.s16        q4,  q9,  d1[1]
        vmla.s16        q4,  q13, d1[2]

        vld1.16         {q10},  [r10, :128]!
        vqadd.s16       q4,  q4,  q2

        vld1.16         {q9},   [r9,  :128]!
        vshr.s16        q4,  q4,  #3
        vld1.16         {q5},   [r6,  :128]!
        vadd.s16        q4,  q4,  q15

        vadd.s16        q6,  q6,  q11
        vadd.s16        q7,  q7,  q10
        vadd.s16        q5,  q5,  q4

        vmull.s16       q8,  d18, d2[3]
        vmlal.s16       q8,  d12, d2[1]
        vmlal.s16       q8,  d14, d2[2]
        vmlal.s16       q8,  d10, d2[0]

        vmull.s16       q9,  d19, d2[3]
        vmlal.s16       q9,  d13, d2[1]
        vmlal.s16       q9,  d15, d2[2]
        vmlal.s16       q9,  d11, d2[0]

        vqrshrun.s32    d16, q8,  #11
        vqrshrun.s32    d17, q9,  #11
        vst1.16         {q4},  [r12, :128]!
        vqmovun.s16     d16, q8
        subs            r4,  r4,  #8
        vst1.8          {d16}, [r0, :64]!

        ble             9f
        vmov            q2,  q3
        vld1.8          {d6},  [r2]!
        tst             r5,  #2 // LR_HAVE_RIGHT
        vmovl.u8        q3,  d6
        bne             4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        // Reload ptrs from arguments on the stack
        ldr             lr,       [sp, #108]
        // Rotate the window of pointers. Shift the 6 pointers downwards one step.
        ldrd            r6,  r7,  [lr, #4]
        ldrd            r8,  r9,  [lr, #12]
        ldrd            r10, r11, [lr, #20]

        strd            r6,  r7,  [lr]
        strd            r8,  r9,  [lr, #8]
        strd            r10, r11, [lr, #16]
        // The topmost pointer, ptrs[6], which isn't used as input, is set to
        // ptrs[0], which will be used as output for the next _hv call.
        // At the start of the filtering, the caller may set ptrs[6] to the
        // right next buffer to fill in, instead.
        str             r6,       [lr, #24]

        vpop            {q4-q7}
        pop             {r4-r11,pc}
endfunc

#include "looprestoration_tmpl.S"

// void dav1d_sgr_box3_row_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                     const pixel (*left)[4],
//                                     const pixel *src, const int w,
//                                     const enum LrEdgeFlags edges);
function sgr_box3_row_h_8bpc_neon, export=1
        push            {r4-r5,lr}
        ldrd            r4,  r5,  [sp, #12]
        add             r4,  r4,  #2 // w += 2

        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        cmp             r2,  #0
        bne             0f

        // LR_HAVE_LEFT && left == NULL
        sub             r3,  r3,  #2
        vld1.8          {q0}, [r3]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q0},   [r3]!
        vld1.32         {d3[]}, [r2]
        // Move r3 back to account for the last 2 bytes we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #2
        vext.8          q0,  q1,  q0,  #14
        b               2f

1:
        vld1.8          {q0},   [r3]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q0 to have 2x the first byte at the front.
        vdup.8          q1,  d0[0]
        // Move r3 back to account for the last 2 bytes we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #2
        vext.8          q0,  q1,  q0,  #14

2:
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1

        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r4,  #(2 + 16 - 2 + 1)
        ldrb            lr,  [r3,  lr]
        // Fill q14 with the right padding pixel
        vdup.8          q14, lr
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #10
        bge             4f   // If w >= 10, all used input pixels are valid

        // 1 <= w < 10, w pixels valid in q0. For w=9, this ends up called
        // again; it's not strictly needed in those cases (we pad enough here),
        // but keeping the code as simple as possible.

        // Insert padding in q0.b[w] onwards
        movrel_local    lr,  right_ext_mask
        sub             lr,  lr,  r4
        vld1.8          {q13}, [lr]

        vbit            q0,  q14, q13

        // Update the precalculated squares
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1

4:      // Loop horizontally
        vext.8          d16, d0,  d1,  #1
        vext.8          d17, d0,  d1,  #2
        vaddl.u8        q3,  d0,  d16
        vext.8          q9,  q1,  q2,  #2
        vaddw.u8        q3,  q3,  d17

        vext.8          q10, q1,  q2,  #4

        vaddl.u16       q12, d2,  d18
        vaddl.u16       q13, d3,  d19
        vaddw.u16       q12, q12, d20
        vaddw.u16       q13, q13, d21

        subs            r4,  r4,  #8
        vst1.16         {q3},       [r1,  :128]!
        vst1.32         {q12, q13}, [r0,  :128]!

        ble             9f
        tst             r5,  #2 // LR_HAVE_RIGHT
        vld1.8          {d6},  [r3]!
        vmov            q1,  q2
        vext.8          q0,  q0,  q3,  #8
        vmull.u8        q2,  d6,  d6

        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r5,pc}
endfunc

// void dav1d_sgr_box5_row_h_8bpc_neon(int32_t *sumsq, int16_t *sum,
//                                     const pixel (*left)[4],
//                                     const pixel *src, const int w,
//                                     const enum LrEdgeFlags edges);
function sgr_box5_row_h_8bpc_neon, export=1
        push            {r4-r5,lr}
        ldrd            r4,  r5,  [sp, #12]
        add             r4,  r4,  #2 // w += 2

        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        cmp             r2,  #0
        bne             0f

        // LR_HAVE_LEFT && left == NULL
        sub             r3,  r3,  #3
        vld1.8          {q0}, [r3]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q0},   [r3]!
        vld1.32         {d3[]}, [r2]
        // Move r3 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #3
        vext.8          q0,  q1,  q0,  #13
        b               2f

1:
        vld1.8          {q0},   [r3]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q0 to have 3x the first byte at the front.
        vdup.8          q1,  d0[0]
        // Move r3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #3
        vext.8          q0,  q1,  q0,  #13

2:
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1

        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r4,  #(2 + 16 - 3 + 1)
        ldrb            lr,  [r3,  lr]
        // Fill q14 with the right padding pixel
        vdup.8          q14, lr
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+1 pixels valid in q0. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // Insert padding in q0.b[w+1] onwards; fuse the +1 into the
        // buffer pointer.
        movrel_local    lr,  right_ext_mask, -1
        sub             lr,  lr,  r4
        vld1.8          {q13}, [lr]

        vbit            q0,  q14, q13

        // Update the precalculated squares
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1

4:      // Loop horizontally
        vext.8          d16, d0,  d1,  #1
        vext.8          d17, d0,  d1,  #2
        vext.8          d18, d0,  d1,  #3
        vext.8          d19, d0,  d1,  #4
        vaddl.u8        q3,  d0,  d16
        vaddl.u8        q12, d17, d18
        vaddw.u8        q3,  q3,  d19
        vadd.u16        q3,  q3,  q12

        vext.8          q8,  q1,  q2,  #2
        vext.8          q9,  q1,  q2,  #4
        vext.8          q10, q1,  q2,  #6
        vext.8          q11, q1,  q2,  #8
        vaddl.u16       q12, d2,  d16
        vaddl.u16       q13, d3,  d17
        vaddl.u16       q8,  d18, d20
        vaddl.u16       q9,  d19, d21
        vaddw.u16       q12, q12, d22
        vaddw.u16       q13, q13, d23
        vadd.i32        q12, q12, q8
        vadd.i32        q13, q13, q9

        subs            r4,  r4,  #8
        vst1.16         {q3},       [r1,  :128]!
        vst1.32         {q12, q13}, [r0,  :128]!

        ble             9f
        tst             r5,  #2 // LR_HAVE_RIGHT
        vld1.8          {d6},  [r3]!
        vmov            q1,  q2
        vext.8          q0,  q0,  q3,  #8
        vmull.u8        q2,  d6,  d6
        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r5,pc}
endfunc

// void dav1d_sgr_box35_row_h_8bpc_neon(int32_t *sumsq3, int16_t *sum3,
//                                      int32_t *sumsq5, int16_t *sum5,
//                                      const pixel (*left)[4],
//                                      const pixel *src, const int w,
//                                      const enum LrEdgeFlags edges);
function sgr_box35_row_h_8bpc_neon, export=1
        push            {r4-r7,lr}
        ldrd            r4,  r5,  [sp, #20]
        ldrd            r6,  r7,  [sp, #28]
        add             r6,  r6,  #2 // w += 2

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             1f
        cmp             r4,  #0
        bne             0f

        // LR_HAVE_LEFT && left == NULL
        sub             r5,  r5,  #3
        vld1.8          {q0}, [r5]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q0},   [r5]!
        vld1.32         {d3[]}, [r4]
        // Move r3 back to account for the last 3 bytes we loaded earlier,
        // which we'll shift out.
        sub             r5,  r5,  #3
        vext.8          q0,  q1,  q0,  #13
        b               2f

1:
        vld1.8          {q0},   [r5]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost byte
        // and shift q0 to have 3x the first byte at the front.
        vdup.8          q1,  d0[0]
        // Move r3 back to account for the last 3 bytes we loaded before,
        // which we shifted out.
        sub             r5,  r5,  #3
        vext.8          q0,  q1,  q0,  #13

2:
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1

        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that byte to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r6,  #(2 + 16 - 3 + 1)
        ldrb            lr,  [r5,  lr]
        // Fill q14 with the right padding pixel
        vdup.8          q14, lr
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r6,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+1 pixels valid in q0. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // Insert padding in q0.b[w+1] onwards; fuse the +1 into the
        // buffer pointer.
        movrel_local    lr,  right_ext_mask, -1
        sub             lr,  lr,  r6
        vld1.8          {q13}, [lr]

        vbit            q0,  q14, q13

        // Update the precalculated squares
        vmull.u8        q1,  d0,  d0
        vmull.u8        q2,  d1,  d1

4:      // Loop horizontally
        vext.8          d16, d0,  d1,  #1
        vext.8          d17, d0,  d1,  #2
        vext.8          d18, d0,  d1,  #3
        vext.8          d19, d0,  d1,  #4
        vaddl.u8        q3,  d16, d17
        vaddl.u8        q12, d0,  d19
        vaddw.u8        q3,  q3,  d18

        vext.8          q8,  q1,  q2,  #2
        vext.8          q9,  q1,  q2,  #4
        vext.8          q10, q1,  q2,  #6
        vext.8          q11, q1,  q2,  #8

        vst1.16         {q3},       [r1,  :128]!
        vadd.u16        q3,  q3,  q12

        vaddl.u16       q12, d16, d18
        vaddl.u16       q13, d17, d19
        vaddl.u16       q8,  d2,  d22
        vaddl.u16       q9,  d3,  d23
        vaddw.u16       q12, q12, d20
        vaddw.u16       q13, q13, d21

        vst1.32         {q12, q13}, [r0,  :128]!
        vadd.i32        q12, q12, q8
        vadd.i32        q13, q13, q9

        subs            r6,  r6,  #8
        vst1.16         {q3},       [r3,  :128]!
        vst1.32         {q12, q13}, [r2,  :128]!

        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vld1.8          {d6},  [r5]!
        vmov            q1,  q2
        vext.8          q0,  q0,  q3,  #8
        vmull.u8        q2,  d6,  d6
        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r7,pc}
endfunc

sgr_funcs 8
