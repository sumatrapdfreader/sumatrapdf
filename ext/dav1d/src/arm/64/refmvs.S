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

#include "src/arm/asm-offsets.h"
#include "src/arm/asm.S"
#include "util.S"

#define INVALID_MV 0x80008000

// void dav1d_splat_mv_neon(refmvs_block **rr, const refmvs_block *rmv,
//                          int bx4, int bw4, int bh4)

function splat_mv_neon, export=1
        ld1             {v3.16b},  [x1]
        clz             w3,  w3
        movrel          x5,  splat_tbl
        sub             w3,  w3,  #26
        ext             v2.16b,  v3.16b,  v3.16b,  #12
        ldrsw           x3,  [x5, w3, uxtw #2]
        add             w2,  w2,  w2,  lsl #1
        ext             v0.16b,  v2.16b,  v3.16b,  #4
        add             x3,  x5,  x3
        ext             v1.16b,  v2.16b,  v3.16b,  #8
        lsl             w2,  w2,  #2
        ext             v2.16b,  v2.16b,  v3.16b,  #12
1:
        ldr             x1,  [x0],  #8
        subs            w4,  w4,  #1
        add             x1,  x1,  x2
        br              x3

10:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.8b}, [x1]
        str             s2,  [x1, #8]
        b.gt            1b
        ret
20:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b}, [x1]
        str             d1,  [x1, #16]
        b.gt            1b
        ret
320:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
160:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
80:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1], #48
40:
        AARCH64_VALID_JUMP_TARGET
        st1             {v0.16b, v1.16b, v2.16b}, [x1]
        b.gt            1b
        ret
endfunc

jumptable splat_tbl
        .word 320b  - splat_tbl
        .word 160b  - splat_tbl
        .word 80b   - splat_tbl
        .word 40b   - splat_tbl
        .word 20b   - splat_tbl
        .word 10b   - splat_tbl
endjumptable

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
        AARCH64_SIGN_LINK_REGISTER
        stp             x29, x30, [sp, #-16]!
        mov             x29, sp

        movi            v30.8b,  #0
        ld1             {v31.8b}, [x3]
        movrel          x8,  save_tmvs_tbl
        movrel          x16, mask_mult
        movrel          x13, mv_tbls
        ld1             {v29.8b}, [x16]
        ext             v31.8b,  v30.8b,  v31.8b,  #7 // [0, ref_sign]
        mov             w15, #5
        mov             w14, #12*2
        sxtw            x4,  w4
        sxtw            x6,  w6
        mul             w1,  w1,  w15             // stride *= 5
        sub             w5,  w5,  w7              // h = row_end8 - row_start8
        lsl             w7,  w7,  #1              // row_start8 <<= 1
1:
        mov             w15, #5
        and             w9,  w7,  #30             // (y & 15) * 2
        ldr             x9,  [x2, w9, uxtw #3]    // b = rr[(y & 15) * 2]
        add             x9,  x9,  #12             // &b[... + 1]
        madd            x10, x4,  x14,  x9        // end_cand_b = &b[col_end8*2 + 1]
        madd            x9,  x6,  x14,  x9        // cand_b = &b[x*2 + 1]

        madd            x3,  x6,  x15,  x0        // &rp[x]

2:
        ldrb            w11, [x9, #10]            // cand_b->bs
        ld1             {v0.16b}, [x9]            // cand_b->mv
        add             x11, x8,  w11, uxtw #3
        ldr             h1,  [x9, #8]             // cand_b->ref
        ldr             w12, [x11]                // bw8
        mov             x15, x8
        add             x9,  x9,  w12, uxtw #1    // cand_b += bw8*2
        cmp             x9,  x10
        mov             v2.8b,   v0.8b
        b.ge            3f

        ldrb            w15, [x9, #10]            // cand_b->bs
        add             x16, x9,  #8
        ld1             {v4.16b}, [x9]            // cand_b->mv
        add             x15, x8,  w15, uxtw #3
        ld1             {v1.h}[1], [x16]          // cand_b->ref
        ldr             w12, [x15]                // bw8
        add             x9,  x9,  w12, uxtw #1    // cand_b += bw8*2
        trn1            v2.2d,   v0.2d,   v4.2d

3:
        abs             v2.8h,   v2.8h            // abs(mv[].xy)
        tbl             v1.8b, {v31.16b}, v1.8b   // ref_sign[ref]
        ushr            v2.8h,   v2.8h,   #12     // abs(mv[].xy) >> 12
        umull           v1.8h,   v1.8b,   v29.8b  // ref_sign[ref] * {1, 2}
        cmeq            v2.4s,   v2.4s,   #0      // abs(mv[].xy) <= 4096
        xtn             v2.4h,   v2.4s            // abs() condition to 16 bit
        and             v1.8b,   v1.8b,   v2.8b   // h[0-3] contains conditions for mv[0-1]
        addp            v1.4h,   v1.4h,   v1.4h   // Combine condition for [1] and [0]
        umov            w16, v1.h[0]              // Extract case for first block
        umov            w17, v1.h[1]
        ldrsw           x11, [x11, #4]            // Fetch jump table entry
        ldrsw           x15, [x15, #4]
        ldr             q1, [x13, w16, uxtw #4]   // Load permutation table base on case
        ldr             q5, [x13, w17, uxtw #4]
        add             x11, x8,  x11             // Find jump table target
        add             x15, x8,  x15
        tbl             v0.16b, {v0.16b}, v1.16b  // Permute cand_b to output refmvs_temporal_block
        tbl             v4.16b, {v4.16b}, v5.16b

        // v1 follows on v0, with another 3 full repetitions of the pattern.
        ext             v1.16b,  v0.16b,  v0.16b,  #1
        ext             v5.16b,  v4.16b,  v4.16b,  #1
        // v2 ends with 3 complete repetitions of the pattern.
        ext             v2.16b,  v0.16b,  v1.16b,  #4
        ext             v6.16b,  v4.16b,  v5.16b,  #4

        blr             x11
        b.ge            4f  // if (cand_b >= end)
        mov             v0.16b,  v4.16b
        mov             v1.16b,  v5.16b
        mov             v2.16b,  v6.16b
        cmp             x9,  x10
        blr             x15
        b.lt            2b  // if (cand_b < end)

4:
        subs            w5,  w5,  #1              // h--
        add             w7,  w7,  #2              // y += 2
        add             x0,  x0,  x1              // rp += stride
        b.gt            1b

        ldp             x29, x30, [sp], #16
        AARCH64_VALIDATE_LINK_REGISTER
        ret

10:
        AARCH64_VALID_CALL_TARGET
        add             x16, x3,  #4
        st1             {v0.s}[0], [x3]
        st1             {v0.b}[4], [x16]
        add             x3,  x3,  #5
        ret
20:
        AARCH64_VALID_CALL_TARGET
        add             x16, x3,  #8
        st1             {v0.d}[0], [x3]
        st1             {v0.h}[4], [x16]
        add             x3,  x3,  #2*5
        ret
40:
        AARCH64_VALID_CALL_TARGET
        st1             {v0.16b}, [x3]
        str             s1, [x3, #16]
        add             x3,  x3,  #4*5
        ret
80:
        AARCH64_VALID_CALL_TARGET
        // This writes 6 full entries plus 2 extra bytes
        st1             {v0.16b, v1.16b}, [x3]
        // Write the last few, overlapping with the first write.
        stur            q2, [x3, #(8*5-16)]
        add             x3,  x3,  #8*5
        ret
160:
        AARCH64_VALID_CALL_TARGET
        add             x16, x3,  #6*5
        add             x17, x3,  #12*5
        // This writes 6 full entries plus 2 extra bytes
        st1             {v0.16b, v1.16b}, [x3]
        // Write another 6 full entries, slightly overlapping with the first set
        st1             {v0.16b, v1.16b}, [x16]
        // Write 8 bytes (one full entry) after the first 12
        st1             {v0.8b}, [x17]
        // Write the last 3 entries
        str             q2, [x3, #(16*5-16)]
        add             x3,  x3,  #16*5
        ret
endfunc

jumptable save_tmvs_tbl
        .word 16 * 12
        .word 160b - save_tmvs_tbl
        .word 16 * 12
        .word 160b - save_tmvs_tbl
        .word 8 * 12
        .word 80b  - save_tmvs_tbl
        .word 8 * 12
        .word 80b  - save_tmvs_tbl
        .word 8 * 12
        .word 80b  - save_tmvs_tbl
        .word 8 * 12
        .word 80b  - save_tmvs_tbl
        .word 4 * 12
        .word 40b  - save_tmvs_tbl
        .word 4 * 12
        .word 40b  - save_tmvs_tbl
        .word 4 * 12
        .word 40b  - save_tmvs_tbl
        .word 4 * 12
        .word 40b  - save_tmvs_tbl
        .word 2 * 12
        .word 20b  - save_tmvs_tbl
        .word 2 * 12
        .word 20b  - save_tmvs_tbl
        .word 2 * 12
        .word 20b  - save_tmvs_tbl
        .word 2 * 12
        .word 20b  - save_tmvs_tbl
        .word 2 * 12
        .word 20b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
        .word 1 * 12
        .word 10b  - save_tmvs_tbl
endjumptable

// void dav1d_load_tmvs_neon(const refmvs_frame *const rf, int tile_row_idx,
//                           const int col_start8, const int col_end8,
//                           const int row_start8, int row_end8)
function load_tmvs_neon, export=1
        rf              .req x0
        tile_row_idx    .req w1
        col_start8      .req w2
        col_end8        .req w3
        row_start8      .req w4
        row_end8        .req w5
        col_start8i     .req w6
        col_end8i       .req w7
        rp_proj         .req x8
        stride5         .req x9
        wstride5        .req w9
        stp             x28, x27, [sp, #-96]!
        stp             x26, x25, [sp, #16]
        stp             x24, x23, [sp, #32]
        stp             x22, x21, [sp, #48]
        stp             x20, x19, [sp, #64]
        stp             x29, x30, [sp, #80]

        ldr             w15, [rf, #RMVSF_N_TILE_THREADS]
        ldp             w16, w17, [rf, #RMVSF_IW8]          // include rf->ih8 too
        sub             col_start8i, col_start8, #8         // col_start8 - 8
        add             col_end8i, col_end8, #8             // col_end8 + 8
        ldr             wstride5, [rf, #RMVSF_RP_STRIDE]
        ldr             rp_proj, [rf, #RMVSF_RP_PROJ]

        cmp             w15, #1
        csel            tile_row_idx, wzr, tile_row_idx, eq // if (rf->n_tile_threads == 1) tile_row_idx = 0

        bic             col_start8i, col_start8i, col_start8i, asr #31  // imax(col_start8 - 8, 0)
        cmp             col_end8i, w16
        csel            col_end8i, col_end8i, w16, lt       // imin(col_end8 + 8, rf->iw8)

        lsl             tile_row_idx, tile_row_idx, #4      // 16 * tile_row_idx

        cmp             row_end8, w17
        csel            row_end8, row_end8, w17, lt         // imin(row_end8, rf->ih8)

        add             wstride5, wstride5, wstride5, lsl #2    // stride * sizeof(refmvs_temporal_block)
        and             w15, row_start8, #15                // row_start8 & 15
        add             w10, col_start8, col_start8, lsl #2 // col_start8 * sizeof(refmvs_temporal_block)
        smaddl          rp_proj, tile_row_idx, wstride5, rp_proj    // &rf->rp_proj[16 * stride * tile_row_idx]
        smaddl          x10, w15, wstride5, x10             // ((row_start8 & 15) * stride + col_start8) * sizeof(refmvs_temporal_block)
        mov             w15, #INVALID_MV
        sub             w11, col_end8, col_start8           // xfill loop count
        add             x10, x10, rp_proj                   // &rf->rp_proj[16 * stride * tile_row_idx + (row_start8 & 15) * stride + col_start8]
        add             x15, x15, x15, lsl #40              // first 64b of 4 [INVALID_MV, 0]... patterns
        mov             w17, #(INVALID_MV >> 8)             // last 32b of 4 patterns
        sub             w12, row_end8, row_start8           // yfill loop count
        ror             x16, x15, #48                       // second 64b of 4 patterns
        ldr             w19, [rf, #RMVSF_N_MFMVS]

5:      // yfill loop
        and             w13, w11, #-4           // xfill 4x count by patterns
        mov             x14, x10                // fill_ptr = row_ptr
        add             x10, x10, stride5       // row_ptr += stride
        sub             w12, w12, #1            // y--

        cbz             w13, 3f

4:      // xfill loop 4x
        sub             w13, w13, #4            // xfill 4x count -= 4
        stp             x15, x16, [x14]
        str             w17, [x14, #16]
        add             x14, x14, #20           // fill_ptr += 4 * sizeof(refmvs_temporal_block)
        cbnz            w13, 4b

3:      // up to 3 residuals
        tbz             w11, #1, 1f
        str             x15, [x14]
        strh            w16, [x14, #8]
        add             x14, x14, #10           // fill_ptr += 2 * sizeof(refmvs_temporal_block)

1:      // up to 1 residual
        tbz             w11, #0, 2f
        str             w15, [x14]
2:
        cbnz            w12, 5b                 // yfill loop

        cbz             w19, 11f                // if (!rf->n_mfmvs) skip nloop

        add             x29, rf, #RMVSF_MFMV_REF2CUR
        mov             w10, #0                 // n = 0
        movi            v3.2s, #255             // 0x3FFF >> 6, for MV clamp
        movrel          x1, div_mult_tbl

10:     // nloop
        ldrsb           w16, [x29, x10]         // ref2cur = rf->mfmv_ref2cur[n]
        cmp             w16, #-32
        b.eq            9f                      // if (ref2cur == INVALID_REF2CUR) continue

        add             x17, x10, #(RMVSF_MFMV_REF - RMVSF_MFMV_REF2CUR)    // n - (&rf->mfmv_ref - &rf->mfmv_ref2cur)
        mov             x20, #4
        ldrb            w17, [x29, x17]         // ref = rf->mfmv_ref[n]
        ldr             x13, [x29, #(RMVSF_RP_REF - RMVSF_MFMV_REF2CUR)]
        sub             x21, x10, x10, lsl #3   // -(n * 7)
        smaddl          x20, row_start8, wstride5, x20  // row_start8 * stride * sizeof(refmvs_temporal_block) + 4
        mov             w12, row_start8         // y = row_start8
        add             x28, x29, #(RMVSF_MFMV_REF2REF - RMVSF_MFMV_REF2CUR - 1)    // &rf->mfmv_ref2ref - 1
        ldr             x13, [x13, x17, lsl #3] // rf->rp_ref[ref]
        sub             x28, x28, x21           // rf->mfmv_ref2ref[n] - 1
        sub             w17, w17, #4            // ref_sign = ref - 4
        add             x13, x13, x20           // r = &rf->rp_ref[ref][row_start8 * stride].ref
        dup             v0.2s, w17              // ref_sign

5:      // yloop
        and             w14, w12, #-8           // y_sb_align = y & ~7
        mov             w11, col_start8i        // x = col_start8i
        add             w15, w14, #8            // y_sb_align + 8
        cmp             w14, row_start8
        csel            w14, w14, row_start8, gt    // imax(y_sb_align, row_start8)
        cmp             w15, row_end8
        csel            w15, w15, row_end8, lt  // imin(y_sb_align + 8, row_end8)

4:      // xloop
        add             x23, x13, x11, lsl #2   // partial &r[x] address
        ldrb            w22, [x23, x11]         // b_ref = rb->ref
        cbz             w22, 6f                 // if (!b_ref) continue

        ldrb            w24, [x28, x22]         // ref2ref = rf->mfmv_ref2ref[n][b_ref - 1]
        cbz             w24, 6f                 // if (!ref2ref) continue

        ldrh            w20, [x1, x24, lsl #1]  // div_mult[ref2ref]
        add             x23, x23, x11           // &r[x]
        mul             w20, w20, w16           // frac = ref2cur * div_mult[ref2ref]

        ldur            s1, [x23, #-4]          // mv{y, x} = rb->mv
        fmov            s2, w20                 // frac
        sxtl            v1.4s, v1.4h
        mul             v1.2s, v1.2s, v2.s[0]   // offset{y, x} = frac * mv{y, x}

        ssra            v1.2s, v1.2s, #31       // offset{y, x} + (offset{y, x} >> 31)
        ldur            w25, [x23, #-4]         // b_mv = rb->mv
        srshr           v1.2s, v1.2s, #14       // (offset{y, x} + (offset{y, x} >> 31) + 8192) >> 14

        abs             v2.2s, v1.2s            // abs(offset{y, x})
        eor             v1.8b, v1.8b, v0.8b     // offset{y, x} ^ ref_sign

        sshr            v2.2s, v2.2s, #6        // abs(offset{y, x}) >> 6
        cmlt            v1.2s, v1.2s, #0        // sign(offset{y, x} ^ ref_sign): -1 or 0
        umin            v2.2s, v2.2s, v3.2s     // iclip(abs(offset{y, x}) >> 6, 0, 0x3FFF >> 6)

        neg             v4.2s, v2.2s
        bsl             v1.8b, v4.8b, v2.8b     // apply_sign(iclip(abs(offset{y, x}) >> 6, 0, 0x3FFF >> 6))
        fmov            x20, d1                 // offset{y, x}

        add             w21, w12, w20           // pos_y = y + offset.y
        cmp             w21, w14                // pos_y >= y_proj_start
        b.lt            1f
        cmp             w21, w15                // pos_y < y_proj_end
        b.ge            1f
        add             x26, x11, x20, asr #32  // pos_x = x + offset.x
        and             w27, w21, #15           // pos_y & 15
        add             x21, x26, x26, lsl #2   // pos_x * sizeof(refmvs_temporal_block)
        umaddl          x27, w27, wstride5, rp_proj // &rp_proj[(pos_y & 15) * stride]
        add             x27, x27, x21           // &rp_proj[(pos_y & 15) * stride + pos_x]

3:      // copy loop
        and             w20, w11, #-8           // x_sb_align = x & ~7
        sub             w21, w20, #8            // x_sb_align - 8
        cmp             w21, col_start8
        csel            w21, w21, col_start8, gt    // imax(x_sb_align - 8, col_start8)
        cmp             w26, w21                // pos_x >= imax(x_sb_align - 8, col_start8)
        b.lt            2f
        add             w20, w20, #16           // x_sb_align + 16
        cmp             w20, col_end8
        csel            w20, w20, col_end8, lt  // imin(x_sb_align + 16, col_end8)
        cmp             w26, w20                // pos_x < imin(x_sb_align + 16, col_end8)
        b.ge            2f
        str             w25, [x27]              // rp_proj[pos + pos_x].mv = rb->mv (b_mv)
        strb            w24, [x27, #4]          // rp_proj[pos + pos_x].ref = ref2ref

2:      // search part of copy loop
        add             w11, w11, #1            // x++
        cmp             w11, col_end8i          // if (++x >= col_end8i) break xloop
        b.ge            8f

        ldrb            w20, [x23, #5]!         // rb++; rb->ref
        cmp             w20, w22                // if (rb->ref != b_ref) break
        b.ne            7f

        ldur            w21, [x23, #-4]         // rb->mv.n
        cmp             w21, w25                // if (rb->mv.n != b_mv.n) break
        b.ne            7f

        add             w26, w26, #1            // pos_x++
        add             x27, x27, #5            // advance &rp_proj[(pos_y & 15) * stride + pos_x]
        b               3b                      // copy loop

1:      // search loop
        add             w11, w11, #1            // x++
        cmp             w11, col_end8i          // if (++x >= col_end8i) break xloop
        b.ge            8f

        ldrb            w20, [x23, #5]!         // rb++; rb->ref
        cmp             w20, w22                // if (rb->ref != b_ref) break
        b.ne            7f

        ldur            w21, [x23, #-4]         // rb->mv.n
        cmp             w21, w25                // if (rb->mv.n == b_mv.n) continue
        b.eq            1b                      // search loop
7:
        cmp             w11, col_end8i          // x < col_end8i
        b.lt            4b                      // xloop

6:      // continue case of xloop
        add             w11, w11, #1            // x++
        cmp             w11, col_end8i          // x < col_end8i
        b.lt            4b                      // xloop
8:
        add             w12, w12, #1            // y++
        add             x13, x13, stride5       // r += stride
        cmp             w12, row_end8           // y < row_end8
        b.lt            5b                      // yloop
9:
        add             w10, w10, #1
        cmp             w10, w19                // n < rf->n_mfmvs
        b.lt            10b                     // nloop
11:
        ldp             x29, x30, [sp, #80]
        ldp             x20, x19, [sp, #64]
        ldp             x22, x21, [sp, #48]
        ldp             x24, x23, [sp, #32]
        ldp             x26, x25, [sp, #16]
        ldp             x28, x27, [sp], #96
        ret
        .unreq          rf
        .unreq          tile_row_idx
        .unreq          col_start8
        .unreq          col_end8
        .unreq          row_start8
        .unreq          row_end8
        .unreq          col_start8i
        .unreq          col_end8i
        .unreq          rp_proj
        .unreq          stride5
        .unreq          wstride5
endfunc

const div_mult_tbl
        .hword             0, 16384, 8192, 5461, 4096, 3276, 2730, 2340
        .hword          2048,  1820, 1638, 1489, 1365, 1260, 1170, 1092
        .hword          1024,   963,  910,  862,  819,  780,  744,  712
        .hword           682,   655,  630,  606,  585,  564,  546,  528
endconst
