/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2020, Martin Storsjo
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

// void dav1d_wiener_filter_h_16bpc_neon(int16_t *dst, const pixel (*left)[4],
//                                       const pixel *src, const int16_t fh[8],
//                                       const int w,
//                                       enum LrEdgeFlags edges,
//                                       const int bitdepth_max);
function wiener_filter_h_16bpc_neon, export=1
        push            {r4-r6,lr}
        ldrd            r4,  r5,  [sp, #16]
        ldr             r6,       [sp, #24] // bitdepth_max
        vld1.16         {q0}, [r3, :128]
        clz             r6,  r6
        vmov.i32        q14, #1
        sub             r12, r6,  #38  // -(bitdepth + 6)
        sub             r6,  r6,  #25  // -round_bits_h
        neg             r12, r12       // bitdepth + 6
        vdup.32         q1,  r12
        vdup.32         q13, r6        // -round_bits_h
        vmov.i16        q15, #8192
        vshl.u32        q14, q14, q1   // 1 << (bitdepth + 6)
        vmvn.i16        q12, #0x8000   // 0x7fff = (1 << 15) - 1

        // Set up the src pointers to include the left edge, for LR_HAVE_LEFT, left == NULL
        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        // LR_HAVE_LEFT
        cmp             r1,  #0
        bne             0f
        // left == NULL
        sub             r2,  r2,  #6
        vld1.16         {q2, q3}, [r2]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.16         {q2, q3}, [r2]!
        vld1.16         {d3},  [r1]!
        // Move r2 back to account for the last 3 pixels we loaded earlier,
        // which we'll shift out.
        sub             r2,  r2,  #6
        vext.8          q3,  q2,  q3,  #10
        vext.8          q2,  q1,  q2,  #10
        b               2f
1:
        vld1.16         {q2, q3}, [r2]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost pixel
        // and shift q2/q3 to have 3x the first pixel at the front.
        vdup.16         q1,  d4[0]
        // Move r2 back to account for the last 3 pixels we loaded before,
        // which we shifted out.
        sub             r2,  r2,  #6
        vext.8          q3,  q2,  q3,  #10
        vext.8          q2,  q1,  q2,  #10

2:
        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+3 pixels valid in q2-q3. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is q1/2.h[w+2]. r2 points at the next input, ie
        // q1/2.h[16]. Thus read from r2[w-14] to find the padding pixel.
        sub             r12, r4,  #14
        lsl             r12, r12, #1
        // Insert padding in q2/3.h[w+3] onwards; fuse the +3 (*2) into the
        // buffer pointer.
        movrel_local    r3,  right_ext_mask, -6
        ldrh            r12, [r2, r12]
        sub             r3,  r3,  r4,  lsl #1
        vdup.16         q11, r12
        vld1.8          {q9, q10}, [r3]

        vbit            q2,  q11, q9
        vbit            q3,  q11, q10

4:      // Loop horizontally
        vext.8          q9,  q2,  q3,  #4
        vext.8          q10, q2,  q3,  #8
        vext.8          q8,  q2,  q3,  #2
        vext.8          q11, q2,  q3,  #10
        vadd.i16        q10, q10, q9
        vadd.i16        q11, q11, q8
        vext.8          q8,  q2,  q3,  #12
        vext.8          q9,  q2,  q3,  #6
        vadd.i16        q2,  q2,  q8
        vmull.s16       q8,  d18, d0[3]
        vmlal.s16       q8,  d20, d1[0]
        vmlal.s16       q8,  d22, d1[1]
        vmlal.s16       q8,  d4,  d1[2]
        vmull.s16       q9,  d19, d0[3]
        vmlal.s16       q9,  d21, d1[0]
        vmlal.s16       q9,  d23, d1[1]
        vmlal.s16       q9,  d5,  d1[2]

        vadd.i32        q8,  q8,  q14
        vadd.i32        q9,  q9,  q14
        vrshl.s32       q8,  q8,  q13
        vrshl.s32       q9,  q9,  q13
        vqmovun.s32     d16, q8
        vqmovun.s32     d17, q9
        vmin.u16        q8,  q8,  q12
        vsub.i16        q8,  q8,  q15
        subs            r4,  r4,  #8
        vst1.16         {q8}, [r0,  :128]!

        ble             9f
        vmov            q2,  q3
        tst             r5,  #2 // LR_HAVE_RIGHT
        vld1.16         {q3}, [r2]!
        bne             4b // If we don't need to pad, just keep filtering.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r6,pc}
endfunc

// void dav1d_wiener_filter_v_16bpc_neon(pixel *dst, int16_t **ptrs,
//                                       const int16_t fv[8], const int w,
//                                       const int bitdepth_max);
function wiener_filter_v_16bpc_neon, export=1
        push            {r4-r9,lr}
        vpush           {q4-q7}

        ldr             lr,  [sp, #92]  // bitdepth_max
        vld1.16         {q0},  [r2, :128]
        vdup.16         q2,  lr
        clz             lr,  lr
        sub             lr,  lr,  #11   // round_bits_v

        vdup.32         q1,  lr

        ldrd            r4,  r5,  [r1]
        ldrd            r6,  r7,  [r1, #8]
        ldrd            r8,  r9,  [r1, #16]

        vneg.s32        q1,  q1         // -round_bits_v

1:
        vld1.16         {q4,  q5},  [r4, :128]!
        vld1.16         {q6,  q7},  [r5, :128]!
        vld1.16         {q8,  q9},  [r6, :128]!
        vld1.16         {q10, q11}, [r7, :128]!
        vld1.16         {q12, q13}, [r8, :128]!
        vld1.16         {q14, q15}, [r9, :128]!

        subs            r3,  r3,  #16

        vmull.s16       q3,  d8,  d0[0]
        vmlal.s16       q3,  d12, d0[1]
        vmlal.s16       q3,  d16, d0[2]
        vmlal.s16       q3,  d20, d0[3]
        vmlal.s16       q3,  d24, d1[0]
        vmlal.s16       q3,  d28, d1[1]
        vmlal.s16       q3,  d28, d1[2]
        vmull.s16       q4,  d9,  d0[0]
        vmlal.s16       q4,  d13, d0[1]
        vmlal.s16       q4,  d17, d0[2]
        vmlal.s16       q4,  d21, d0[3]
        vmlal.s16       q4,  d25, d1[0]
        vmlal.s16       q4,  d29, d1[1]
        vmlal.s16       q4,  d29, d1[2]

        vmull.s16       q6,  d10, d0[0]
        vmlal.s16       q6,  d14, d0[1]
        vmlal.s16       q6,  d18, d0[2]
        vmlal.s16       q6,  d22, d0[3]
        vmlal.s16       q6,  d26, d1[0]
        vmlal.s16       q6,  d30, d1[1]
        vmlal.s16       q6,  d30, d1[2]
        vmull.s16       q5,  d11, d0[0]
        vmlal.s16       q5,  d15, d0[1]
        vmlal.s16       q5,  d19, d0[2]
        vmlal.s16       q5,  d23, d0[3]
        vmlal.s16       q5,  d27, d1[0]
        vmlal.s16       q5,  d31, d1[1]
        vmlal.s16       q5,  d31, d1[2]

        vrshl.s32       q3,  q3,  q1    // round_bits_v
        vrshl.s32       q4,  q4,  q1
        vrshl.s32       q6,  q6,  q1
        vrshl.s32       q5,  q5,  q1
        vqmovun.s32     d6,  q3
        vqmovun.s32     d7,  q4
        vqmovun.s32     d8,  q6
        vqmovun.s32     d9,  q5
        vmin.u16        q3,  q3,  q2    // bitdepth_max
        vmin.u16        q4,  q4,  q2
        vst1.16         {q3, q4}, [r0, :128]!
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

        vpop            {q4-q7}
        pop             {r4-r9,pc}
endfunc

// void dav1d_wiener_filter_hv_16bpc_neon(pixel *dst, const pixel (*left)[4],
//                                        const pixel *src,
//                                        const int16_t filter[2][8],
//                                        const int w,
//                                        const enum LrEdgeFlags edges,
//                                        int16_t **ptrs,
//                                        const int bitdepth_max);
function wiener_filter_hv_16bpc_neon, export=1
        push            {r4-r11,lr}
        vpush           {q4-q7}
        ldrd            r4,  r5,  [sp, #100]
        ldrd            r6,  r7,  [sp, #108]
        vld1.16         {q0, q1}, [r3, :128]
        vdup.16         q11, r7        // bitdepth_max
        clz             r7,  r7
        vmov.i32        q14, #1
        sub             r12, r7,  #38  // -(bitdepth + 6)
        sub             lr,  r7,  #11  // round_bits_v
        sub             r7,  r7,  #25  // -round_bits_h
        neg             r12, r12       // bitdepth + 6
        vdup.32         q2,  r12
        vdup.32         q13, r7        // -round_bits_h
        vdup.32         q10, lr        // round_bits_v
        mov             lr,  r6
        vmov.i16        q15, #8192
        vshl.u32        q14, q14, q2   // 1 << (bitdepth + 6)
        vneg.s32        q10, q10       // -round_bits_v

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
        sub             r2,  r2,  #6
        vld1.16         {q2, q3}, [r2]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.16         {q2, q3}, [r2]!
        vld1.16         {d9},  [r1]!
        // Move r2 back to account for the last 3 pixels we loaded earlier,
        // which we'll shift out.
        sub             r2,  r2,  #6
        vext.8          q3,  q2,  q3,  #10
        vext.8          q2,  q4,  q2,  #10
        b               2f
1:
        vld1.16         {q2, q3}, [r2]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost pixel
        // and shift q2/q3 to have 3x the first pixel at the front.
        vdup.16         q4,  d4[0]
        // Move r2 back to account for the last 3 pixels we loaded before,
        // which we shifted out.
        sub             r2,  r2,  #6
        vext.8          q3,  q2,  q3,  #10
        vext.8          q2,  q4,  q2,  #10

2:
        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f

3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+3 pixels valid in q2-q3. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // The padding pixel is q1/2.h[w+2]. r2 points at the next input, ie
        // q1/2.h[16]. Thus read from r2[w-14] to find the padding pixel.
        sub             lr,  r4,  #14
        lsl             lr,  lr,  #1
        // Insert padding in q2/3.h[w+3] onwards; fuse the +3 (*2) into the
        // buffer pointer.
        movrel_local    r3,  right_ext_mask, -6
        ldrh            lr,  [r2, lr]
        sub             r3,  r3,  r4,  lsl #1
        vdup.16         q4,  lr
        vld1.8          {q8, q9}, [r3]

        vbit            q2,  q4,  q8
        vbit            q3,  q4,  q9

4:      // Loop horizontally
        vext.8          q5,  q2,  q3,  #4
        vext.8          q6,  q2,  q3,  #8
        vext.8          q4,  q2,  q3,  #2
        vext.8          q7,  q2,  q3,  #10
        vadd.i16        q6,  q6,  q5
        vadd.i16        q7,  q7,  q4
        vext.8          q4,  q2,  q3,  #12
        vext.8          q5,  q2,  q3,  #6
        vadd.i16        q2,  q2,  q4
        vld1.16         {q4},  [r6,  :128]!
        vmull.s16       q8,  d10, d0[3]
        vmlal.s16       q8,  d12, d1[0]
        vmlal.s16       q8,  d14, d1[1]
        vmlal.s16       q8,  d4,  d1[2]
        vmull.s16       q9,  d11, d0[3]
        vmlal.s16       q9,  d13, d1[0]
        vmlal.s16       q9,  d15, d1[1]
        vmlal.s16       q9,  d5,  d1[2]
        vld1.16         {q5},  [r7,  :128]!

        vmvn.i16        q12, #0x8000   // 0x7fff = (1 << 15) - 1

        vadd.i32        q8,  q8,  q14
        vadd.i32        q9,  q9,  q14
        vld1.16         {q6},  [r8,  :128]!
        vrshl.s32       q8,  q8,  q13
        vrshl.s32       q9,  q9,  q13
        vqmovun.s32     d16, q8
        vqmovun.s32     d17, q9
        vld1.16         {q7},  [r9,  :128]!
        vmin.u16        q8,  q8,  q12
        vld1.16         {q9},  [r10, :128]!
        vsub.i16        q8,  q8,  q15

        vld1.16         {q2},  [r11, :128]!

        vmull.s16       q12, d8,  d2[0]
        vmlal.s16       q12, d10, d2[1]
        vmlal.s16       q12, d12, d2[2]
        vmlal.s16       q12, d14, d2[3]
        vmlal.s16       q12, d18, d3[0]
        vmlal.s16       q12, d4,  d3[1]
        vmlal.s16       q12, d16, d3[2]
        vmull.s16       q4,  d9,  d2[0]
        vmlal.s16       q4,  d11, d2[1]
        vmlal.s16       q4,  d13, d2[2]
        vmlal.s16       q4,  d15, d2[3]
        vmlal.s16       q4,  d19, d3[0]
        vmlal.s16       q4,  d5,  d3[1]
        vmlal.s16       q4,  d17, d3[2]

        vrshl.s32       q12, q12, q10   // round_bits_v
        vrshl.s32       q4,  q4,  q10
        vqmovun.s32     d24, q12
        vqmovun.s32     d25, q4
        vst1.16         {q8},  [r12, :128]!
        vmin.u16        q12, q12, q11   // bitdepth_max
        subs            r4,  r4,  #8
        vst1.16         {q12}, [r0, :128]!

        ble             9f
        vmov            q2,  q3
        tst             r5,  #2 // LR_HAVE_RIGHT
        vld1.16         {q3}, [r2]!
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

// void dav1d_sgr_box3_row_h_16bpc_neon(int32_t *sumsq, int16_t *sum,
//                                      const pixel (*left)[4],
//                                      const pixel *src, const int w,
//                                      const enum LrEdgeFlags edges);
function sgr_box3_row_h_16bpc_neon, export=1
        push            {r4-r5,lr}
        ldrd            r4,  r5,  [sp, #12]
        add             r4,  r4,  #2 // w += 2

        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        cmp             r2,  #0
        bne             0f

        // LR_HAVE_LEFT && left == NULL
        sub             r3,  r3,  #4
        vld1.8          {q0, q1}, [r3]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q0, q1}, [r3]!
        vld1.16         {d5},     [r2]
        // Move r3 back to account for the last 2 pixels we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #4
        vext.8          q1,  q0,  q1,  #12
        vext.8          q0,  q2,  q0,  #12
        b               2f

1:
        vld1.8          {q0, q1}, [r3]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost pixel
        // and shift q0/q1 to have 2x the first pixel at the front.
        vdup.16         q2,  d0[0]
        // Move r3 back to account for the last 2 pixels we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #4
        vext.8          q1,  q0,  q1,  #12
        vext.8          q0,  q2,  q0,  #12

2:
        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that pixel to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r4,  #(2 + 16 - 2 + 1)
        lsl             lr,  lr,  #1
        ldrh            lr,  [r3,  lr]
        // Fill q14 with the right padding pixel
        vdup.16         q14, lr
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #10
        bge             4f   // If w >= 10, all used input pixels are valid

        // 1 <= w < 10, w pixels valid in q0-q1. For w=9, this ends up called
        // again; it's not strictly needed in those cases (we pad enough here),
        // but keeping the code as simple as possible.

        // Insert padding in q0.h[w] onwards
        movrel_local    lr,  right_ext_mask
        sub             lr,  lr,  r4,  lsl #1
        vld1.8          {q12, q13}, [lr]

        vbit            q0,  q14, q12
        vbit            q1,  q14, q13

4:      // Loop horizontally
        vext.8          q8,  q0,  q1,  #2
        vext.8          q9,  q0,  q1,  #4

        vadd.i16        q2,  q0,  q8
        vmull.u16       q12, d0,  d0
        vmlal.u16       q12, d16, d16
        vmlal.u16       q12, d18, d18
        vadd.i16        q2,  q2,  q9
        vmull.u16       q13, d1,  d1
        vmlal.u16       q13, d17, d17
        vmlal.u16       q13, d19, d19
        subs            r4,  r4,  #8
        vst1.16         {q2},       [r1,  :128]!
        vst1.32         {q12, q13}, [r0,  :128]!

        ble             9f
        tst             r5,  #2 // LR_HAVE_RIGHT
        vmov            q0,  q1
        vld1.16         {q1}, [r3]!

        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r5,pc}
endfunc

// void dav1d_sgr_box5_row_h_16bpc_neon(int32_t *sumsq, int16_t *sum,
//                                      const pixel (*left)[4],
//                                      const pixel *src, const int w,
//                                      const enum LrEdgeFlags edges);
function sgr_box5_row_h_16bpc_neon, export=1
        push            {r4-r5,lr}
        ldrd            r4,  r5,  [sp, #12]
        add             r4,  r4,  #2 // w += 2

        tst             r5,  #1 // LR_HAVE_LEFT
        beq             1f
        cmp             r2,  #0
        bne             0f

        // LR_HAVE_LEFT && left == NULL
        sub             r3,  r3,  #6
        vld1.8          {q0, q1}, [r3]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q0, q1}, [r3]!
        vld1.16         {d5},     [r2]
        // Move r3 back to account for the last 2 pixels we loaded earlier,
        // which we'll shift out.
        sub             r3,  r3,  #6
        vext.8          q1,  q0,  q1,  #10
        vext.8          q0,  q2,  q0,  #10
        b               2f

1:
        vld1.8          {q0, q1}, [r3]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost pixel
        // and shift q0/q1 to have 3x the first pixel at the front.
        vdup.16         q2,  d0[0]
        // Move r3 back to account for the last 3 pixels we loaded before,
        // which we shifted out.
        sub             r3,  r3,  #6
        vext.8          q1,  q0,  q1,  #10
        vext.8          q0,  q2,  q0,  #10

2:
        tst             r5,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that pixel to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r4,  #(2 + 16 - 3 + 1)
        lsl             lr,  lr,  #1
        ldrh            lr,  [r3,  lr]
        // Fill q14 with the right padding pixel
        vdup.16         q14, lr
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r4,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+1 pixels valid in q0-q1. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // Insert padding in q0.h[w+1] onwards; fuse the +1 into the
        // buffer pointer.
        movrel_local    lr,  right_ext_mask, -2
        sub             lr,  lr,  r4,  lsl #1
        vld1.8          {q12, q13}, [lr]

        vbit            q0,  q14, q12
        vbit            q1,  q14, q13

4:      // Loop horizontally
        vext.8          q8,  q0,  q1,  #2
        vext.8          q9,  q0,  q1,  #4

        vadd.i16        q2,  q0,  q8
        vmull.u16       q12, d0,  d0
        vmlal.u16       q12, d16, d16
        vmlal.u16       q12, d18, d18
        vadd.i16        q2,  q2,  q9
        vmull.u16       q13, d1,  d1
        vmlal.u16       q13, d17, d17
        vmlal.u16       q13, d19, d19

        vext.8          q8,  q0,  q1,  #6
        vext.8          q9,  q0,  q1,  #8

        vadd.i16        q2,  q2,  q8
        vmlal.u16       q12, d16, d16
        vmlal.u16       q12, d1,  d1
        vadd.i16        q2,  q2,  q9
        vmlal.u16       q13, d17, d17
        vmlal.u16       q13, d19, d19

        subs            r4,  r4,  #8
        vst1.16         {q2},       [r1,  :128]!
        vst1.32         {q12, q13}, [r0,  :128]!

        ble             9f
        tst             r5,  #2 // LR_HAVE_RIGHT
        vmov            q0,  q1
        vld1.16         {q1}, [r3]!
        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r5,pc}
endfunc

// void dav1d_sgr_box35_row_h_16bpc_neon(int32_t *sumsq3, int16_t *sum3,
//                                       int32_t *sumsq5, int16_t *sum5,
//                                       const pixel (*left)[4],
//                                       const pixel *src, const int w,
//                                       const enum LrEdgeFlags edges);
function sgr_box35_row_h_16bpc_neon, export=1
        push            {r4-r7,lr}
        ldrd            r4,  r5,  [sp, #20]
        ldrd            r6,  r7,  [sp, #28]
        add             r6,  r6,  #2 // w += 2

        tst             r7,  #1 // LR_HAVE_LEFT
        beq             1f
        cmp             r4,  #0
        bne             0f

        // LR_HAVE_LEFT && left == NULL
        sub             r5,  r5,  #6
        vld1.8          {q0, q1}, [r5]!
        b               2f

0:
        // LR_HAVE_LEFT, left != NULL
        vld1.8          {q0, q1}, [r5]!
        vld1.16         {d5},     [r4]
        // Move r3 back to account for the last 2 pixels we loaded earlier,
        // which we'll shift out.
        sub             r5,  r5,  #6
        vext.8          q1,  q0,  q1,  #10
        vext.8          q0,  q2,  q0,  #10
        b               2f

1:
        vld1.8          {q0, q1}, [r5]!
        // !LR_HAVE_LEFT, fill q1 with the leftmost pixel
        // and shift q0/q1 to have 3x the first pixel at the front.
        vdup.16         q2,  d0[0]
        // Move r3 back to account for the last 3 pixels we loaded before,
        // which we shifted out.
        sub             r5,  r5,  #6
        vext.8          q1,  q0,  q1,  #10
        vext.8          q0,  q2,  q0,  #10

2:
        tst             r7,  #2 // LR_HAVE_RIGHT
        bne             4f
        // If we'll need to pad the right edge, load that pixel to pad with
        // here since we can find it pretty easily from here.
        sub             lr,  r6,  #(2 + 16 - 3 + 1)
        lsl             lr,  lr,  #1
        ldrh            lr,  [r5,  lr]
        // Fill q14 with the right padding pixel
        vdup.16         q14, lr
3:      // !LR_HAVE_RIGHT

        // Check whether we need to pad the right edge
        cmp             r6,  #11
        bge             4f   // If w >= 11, all used input pixels are valid

        // 1 <= w < 11, w+1 pixels valid in q0-q1. For w=9 or w=10,
        // this ends up called again; it's not strictly needed in those
        // cases (we pad enough here), but keeping the code as simple as possible.

        // Insert padding in q0.h[w+1] onwards; fuse the +1 into the
        // buffer pointer.
        movrel_local    lr,  right_ext_mask, -2
        sub             lr,  lr,  r6,  lsl #1
        vld1.8          {q12, q13}, [lr]

        vbit            q0,  q14, q12
        vbit            q1,  q14, q13

4:      // Loop horizontally
        vext.8          q8,  q0,  q1,  #2
        vext.8          q9,  q0,  q1,  #4
        vext.8          q10, q0,  q1,  #6
        vext.8          q11, q0,  q1,  #8

        vadd.i16        q2,  q8,  q9
        vadd.i16        q3,  q0,  q11
        vadd.i16        q2,  q2,  q10

        vmull.u16       q12, d16, d16
        vmlal.u16       q12, d18, d18
        vmlal.u16       q12, d20, d20
        vmull.u16       q13, d17, d17
        vmlal.u16       q13, d19, d19
        vmlal.u16       q13, d21, d21

        vadd.i16        q3,  q3,  q2
        vst1.16         {q2},       [r1,  :128]!
        vst1.32         {q12, q13}, [r0,  :128]!

        vmlal.u16       q12, d0,  d0
        vmlal.u16       q12, d22, d22
        vmlal.u16       q13, d1,  d1
        vmlal.u16       q13, d23, d23

        subs            r6,  r6,  #8
        vst1.16         {q3},       [r3,  :128]!
        vst1.32         {q12, q13}, [r2,  :128]!

        ble             9f
        tst             r7,  #2 // LR_HAVE_RIGHT
        vmov            q0,  q1
        vld1.16         {q1}, [r5]!
        bne             4b // If we don't need to pad, just keep summing.
        b               3b // If we need to pad, check how many pixels we have left.

9:
        pop             {r4-r7,pc}
endfunc

sgr_funcs 16
