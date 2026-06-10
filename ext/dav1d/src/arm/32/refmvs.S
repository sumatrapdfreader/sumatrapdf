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

const mv_tbls, align=4
        .byte           255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
        .byte           0, 1, 2, 3, 8, 0, 1, 2, 3, 8, 0, 1, 2, 3, 8, 0
        .byte           4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4
        .byte           4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4
endconst

const mask_mult, align=4
        .byte           1, 2, 1, 2, 0, 0, 0, 0
endconst

// void dav1d_save_tmvs_neon(refmvs_temporal_block *rp, ptrdiff_t stride,
//                           refmvs_block **rr, const uint8_t *ref_sign,
//                           int col_end8, int row_end8,
//                           int col_start8, int row_start8)
function save_tmvs_neon, export=1
        push            {r4-r11,lr}
        ldrd            r4,  r5,  [sp, #36]
        ldrd            r6,  r7,  [sp, #44]

        vmov.i8         d30, #0
        vld1.8          {d31}, [r3]
        adr             r8,  L(save_tmvs_tbl)
        movrel_local    lr,  mask_mult
        movrel_local    r12, mv_tbls
        vld1.8          {d29}, [lr]
        vext.8          d31, d30, d31, #7         // [0, ref_sign]
        mov             r3,  #5
        mul             r1,  r1,  r3              // stride *= 5
        sub             r5,  r5,  r7              // h = row_end8 - row_start8
        lsl             r7,  r7,  #1              // row_start8 <<= 1
1:
        mov             r3,  #5
        mov             r11, #12*2
        and             r9,  r7,  #30             // (y & 15) * 2
        ldr             r9,  [r2, r9, lsl #2]     // b = rr[(y & 15) * 2]
        add             r9,  r9,  #12             // &b[... + 1]
        mla             r10, r4,  r11,  r9        // end_cand_b = &b[col_end8*2 + 1]
        mla             r9,  r6,  r11,  r9        // cand_b = &b[x*2 + 1]

        mla             r3,  r6,  r3,   r0        // &rp[x]

        push            {r2,r4,r6}

2:
        ldrb            r11, [r9, #10]            // cand_b->bs
        add             lr,  r9,  #8
        vld1.8          {d0, d1}, [r9]            // cand_b->mv
        add             r11, r8,  r11, lsl #3
        vld1.16         {d2[]},  [lr]             // cand_b->ref
        ldrh            lr,  [r11]                // bw8
        mov             r2,  r8
        add             r9,  r9,  lr,  lsl #1     // cand_b += bw8*2
        cmp             r9,  r10
        vmov            d4,  d0
        bge             3f

        ldrb            r2,  [r9, #10]            // cand_b->bs
        add             lr,  r9,  #8
        vld1.8          {d6, d7}, [r9]            // cand_b->mv
        add             r2,  r8,  r2,  lsl #3
        vld1.16         {d2[1]},  [lr]            // cand_b->ref
        ldrh            lr,  [r2]                 // bw8
        add             r9,  r9,  lr,  lsl #1     // cand_b += bw8*2
        vmov            d5,  d6

3:
        vabs.s16        q2,  q2                   // abs(mv[].xy)
        vtbl.8          d2,  {d31}, d2            // ref_sign[ref]
        vshr.u16        q2,  q2,  #12             // abs(mv[].xy) >> 12
        vmull.u8        q1,  d2,  d29             // ref_sign[ref] * {1, 2}
        vceq.i32        q2,  q2,  #0              // abs(mv[].xy) <= 4096
        vmovn.i32       d4,  q2                   // abs() condition to 16 bit
        vand            d2,  d2,  d4              // h[0-3] contains conditions for mv[0-1]
        vpadd.i16       d2,  d2,  d2              // Combine condition for [1] and [0]
        vmov.u16        r4,  d2[0]                // Extract case for first block
        vmov.u16        r6,  d2[1]
        ldr             r11, [r11, #4]            // Fetch jump table entry
        ldr             r2,  [r2,  #4]
        add             r4,  r12,  r4,  lsl #4
        add             r6,  r12,  r6,  lsl #4
        vld1.8          {d2, d3}, [r4]            // Load permutation table base on case
        vld1.8          {d4, d5}, [r6]
        add             r11, r8,  r11             // Find jump table target
        add             r2,  r8,  r2
        vtbl.8          d16, {d0, d1}, d2         // Permute cand_b to output refmvs_temporal_block
        vtbl.8          d17, {d0, d1}, d3
        vtbl.8          d18, {d6, d7}, d4
        vtbl.8          d19, {d6, d7}, d5
        vmov            q0,  q8

        // q1 follows on q0 (q8), with another 3 full repetitions of the pattern.
        vext.8          q1,  q8,  q8,  #1
        vext.8          q10, q9,  q9,  #1
        // q2 ends with 3 complete repetitions of the pattern.
        vext.8          q2,  q8,  q1,  #4
        vext.8          q11, q9,  q10, #4

        blx             r11
        bge             4f  // if (cand_b >= end)
        vmov            q0,  q9
        vmov            q1,  q10
        vmov            q2,  q11
        cmp             r9,  r10
        blx             r2
        blt             2b  // if (cand_b < end)

4:
        pop             {r2,r4,r6}

        subs            r5,  r5,  #1              // h--
        add             r7,  r7,  #2              // y += 2
        add             r0,  r0,  r1              // rp += stride
        bgt             1b

        pop             {r4-r11,pc}

        .align 2
L(save_tmvs_tbl):
        .word 16 * 12
        .word 160f - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 16 * 12
        .word 160f - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 8 * 12
        .word 80f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 8 * 12
        .word 80f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 8 * 12
        .word 80f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 8 * 12
        .word 80f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 4 * 12
        .word 40f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 4 * 12
        .word 40f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 4 * 12
        .word 40f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 4 * 12
        .word 40f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 2 * 12
        .word 20f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 2 * 12
        .word 20f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 2 * 12
        .word 20f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 2 * 12
        .word 20f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 2 * 12
        .word 20f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB
        .word 1 * 12
        .word 10f  - L(save_tmvs_tbl) + CONFIG_THUMB

10:
        add             r4,  r3,  #4
        vst1.32         {d0[0]}, [r3]
        vst1.8          {d0[4]}, [r4]
        add             r3,  r3,  #5
        bx              lr
20:
        add             r4,  r3,  #8
        vst1.8          {d0}, [r3]
        vst1.16         {d1[0]}, [r4]
        add             r3,  r3,  #2*5
        bx              lr
40:
        add             r4,  r3,  #16
        vst1.8          {q0}, [r3]
        vst1.32         {d2[0]}, [r4]
        add             r3,  r3,  #4*5
        bx              lr
80:
        add             r4,  r3,  #(8*5-16)
        // This writes 6 full entries plus 2 extra bytes
        vst1.8          {q0, q1}, [r3]
        // Write the last few, overlapping with the first write.
        vst1.8          {q2}, [r4]
        add             r3,  r3,  #8*5
        bx              lr
160:
        add             r4,  r3,  #6*5
        add             r6,  r3,  #12*5
        // This writes 6 full entries plus 2 extra bytes
        vst1.8          {q0, q1}, [r3]
        // Write another 6 full entries, slightly overlapping with the first set
        vst1.8          {q0, q1}, [r4]
        add             r4,  r3,  #(16*5-16)
        // Write 8 bytes (one full entry) after the first 12
        vst1.8          {d0}, [r6]
        // Write the last 3 entries
        vst1.8          {q2}, [r4]
        add             r3,  r3,  #16*5
        bx              lr
endfunc
