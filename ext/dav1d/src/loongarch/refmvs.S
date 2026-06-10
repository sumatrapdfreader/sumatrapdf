/*
 * Copyright © 2023, VideoLAN and dav1d authors
 * Copyright © 2023, Loongson Technology Corporation Limited
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

#include "src/loongarch/loongson_asm.S"

/*
static void splat_mv_c(refmvs_block **rr, const refmvs_block *const rmv,
                       const int bx4, const int bw4, int bh4)
*/

function splat_mv_lsx
    vld           vr0,      a1,       0          // 0 1 ... 11 ...
    clz.w         t4,       a3
    vaddi.bu      vr1,      vr0,      0
    addi.w        t4,       t4,       -26
    vextrins.w    vr1,      vr0,      0x30       // 0 1 2 ... 11 0 1 2 3
    la.local      t5,       .SPLAT_LSX_JRTABLE
    vbsrl.v       vr2,      vr1,      4          // 4 5 6 7...11 0 1 2 3 0 0 0 0
    alsl.d        t6,       t4,       t5,     1
    vextrins.w    vr2,      vr0,      0x31       // 4 5 6 7...11 0 1 2 3 4 5 6 7
    ld.h          t7,       t6,       0
    vbsrl.v       vr3,      vr2,      4          // 8 9 10 11 0 1 2 3 4 5 6 7 0 0 0 0
    add.d         t8,       t5,       t7
    alsl.d        a2,       a2,       a2,     1
    vextrins.w    vr3,      vr0,      0x32       // 8 9 10 11 0 1 2 3 4 5 6 7 8 9 10 11
    slli.w        a2,       a2,       2
    jirl          $r0,      t8,       0

.SPLAT_LSX_JRTABLE:
    .hword .SPLAT_W32_LSX - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W16_LSX - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W8_LSX  - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W4_LSX  - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W2_LSX  - .SPLAT_LSX_JRTABLE
    .hword .SPLAT_W1_LSX  - .SPLAT_LSX_JRTABLE

.SPLAT_W1_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    fst.d         f1,       t3,       0
    fst.s         f3,       t3,       8
    blt           zero,     a4,       .SPLAT_W1_LSX
    b             .splat_end
.SPLAT_W2_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    vst           vr1,      t3,       0
    fst.d         f2,       t3,       16
    blt           zero,     a4,       .SPLAT_W2_LSX
    b             .splat_end

.SPLAT_W4_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32
    blt           zero,     a4,       .SPLAT_W4_LSX
    b             .splat_end

.SPLAT_W8_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32

    vst           vr1,      t3,       48
    vst           vr2,      t3,       64
    vst           vr3,      t3,       80
    blt           zero,     a4,       .SPLAT_W8_LSX
    b             .splat_end

.SPLAT_W16_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

.rept 2
    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32

    vst           vr1,      t3,       48
    vst           vr2,      t3,       64
    vst           vr3,      t3,       80

    addi.d        t3,       t3,       96
.endr

    blt           zero,     a4,       .SPLAT_W16_LSX
    b             .splat_end

.SPLAT_W32_LSX:
    ld.d          t3,       a0,       0
    addi.d        a0,       a0,       8
    addi.d        a4,       a4,       -1
    add.d         t3,       t3,       a2

.rept 4
    vst           vr1,      t3,       0
    vst           vr2,      t3,       16
    vst           vr3,      t3,       32

    vst           vr1,      t3,       48
    vst           vr2,      t3,       64
    vst           vr3,      t3,       80

    addi.d        t3,       t3,       96
.endr

    blt           zero,     a4,       .SPLAT_W32_LSX

.splat_end:
endfunc

const la_div_mult
.short    0, 16384, 8192, 5461, 4096, 3276, 2730, 2340
.short 2048,  1820, 1638, 1489, 1365, 1260, 1170, 1092
.short 1024,   963,  910,  862,  819,  780,  744,  712
.short  682,   655,  630,  606,  585,  564,  546,  528
endconst

/*
 *  temp reg: a6 a7
 */
.macro LOAD_SET_LOOP is_odd
    slli.d          a6,      t6,     2
    add.d           a6,      a6,     t6  // col_w * 5
0:
    addi.d          a7,      zero,   0   // x
.if \is_odd
    stx.w           t7,      t3,     a7
    addi.d          a7,      a7,     5
    bge             a7,      a6,     2f
.endif

1:
    stx.w           t7,      t3,     a7
    addi.d          a7,      a7,     5
    stx.w           t7,      t3,     a7
    addi.d          a7,      a7,     5
    blt             a7,      a6,     1b
2:
    add.d           t3,      t3,     t2
    addi.d          t5,      t5,     1
    blt             t5,      a5,     0b
.endm

/*
 * static void load_tmvs_c(const refmvs_frame *const rf, int tile_row_idx,
 *                         const int col_start8, const int col_end8,
 *                         const int row_start8, int row_end8)
 */
function load_tmvs_lsx
    addi.d         sp,      sp,       -80
    st.d           s0,      sp,       0
    st.d           s1,      sp,       8
    st.d           s2,      sp,       16
    st.d           s3,      sp,       24
    st.d           s4,      sp,       32
    st.d           s5,      sp,       40
    st.d           s6,      sp,       48
    st.d           s7,      sp,       56
    st.d           s8,      sp,       64

    vld           vr16,     a0,       16
    vld           vr0,      a0,       48    // rf->mfmv_ref, rf->mfmv_ref2cur
    ld.w          s8,       a0,       80    // [0] - rf->n_mfmvs
    vld           vr17,     a0,       96    // [0] - rp_ref| [1]- rp_proj
    ld.d          t1,       a0,       112   // stride
    ld.w          t0,       a0,       128
    addi.w        t0,       t0,       -1
    bnez          t0,       1f
    addi.w        a1,       zero,     0
1:
    addi.d        t0,       a3,       8
    vinsgr2vr.w   vr1,      t0,       0
    vinsgr2vr.w   vr1,      a5,       1
    vmin.w        vr1,      vr1,      vr16  // [0] col_end8i [1] row_end8
    addi.d        t0,       a2,       -8
    bge           t0,       zero,     2f
    addi.w        t0,       zero,     0     // t0 col_start8i
2:
    vpickve2gr.d  t4,       vr17,     1     // rf->rp_proj
    slli.d        t2,       t1,       2
    add.d         t2,       t2,       t1    // stride * 5
    slli.d        a1,       a1,       4     // tile_row_idx * 16
    andi          t3,       a4,       0xf
    add.d         t3,       t3,       a1    // tile_row_idx * 16 + row_start8 & 15
    mul.w         t3,       t3,       t2
    mul.w         t8,       a1,       t2
    vpickve2gr.w  a5,       vr1,      1
    addi.d        t5,       a4,       0
    sub.d         t6,       a3,       a2     // col_end8 - col_start8
    li.w          t7,       0x80008000
    slli.d        a7,       a2,       2
    add.d         t3,       t3,       a2
    add.d         t3,       t3,       a7
    add.d         t3,       t3,       t4     // rp_proj
    andi          a6,       t6,       1
    bnez          a6,       3f
    LOAD_SET_LOOP 0
    b             4f
3:
    LOAD_SET_LOOP 1
4:
    addi.d        a6,       zero,     0      // n
    bge           a6,       s8,       .end_load
    add.d         t3,       t8,       t4     // rp_proj
    mul.w         t6,       a4,       t2
    addi.d        s7,       zero,     40
    vpickve2gr.w  t1,       vr1,      0      // col_end8i
    addi.d        t5,       a0,       58     // rf->mfmv_ref2ref - 1
    la.local      t8,       la_div_mult
    vld           vr6,      t8,       0
    vld           vr7,      t8,       16
    vld           vr8,      t8,       32
    vld           vr9,      t8,       48
    li.w          t8,       0x3fff
    vreplgr2vr.h  vr21,     t8
    vxor.v        vr18,     vr18,     vr18   // zero
    vsub.h        vr20,     vr18,     vr21
    vpickev.b     vr12,     vr7,      vr6
    vpickod.b     vr13,     vr7,      vr6
    vpickev.b     vr14,     vr9,      vr8
    vpickod.b     vr15,     vr9,      vr8
    vpickve2gr.d  s6,       vr17,     0       // rf->rp_ref
5:
    vld           vr10,     t5,       0       // ref2ref [1...7]
    vpickve2gr.b  t8,       vr0,      8       // ref2cur
    vbsrl.v       vr0,      vr0,      1
    addi.w        t4,       t8,       32
    beqz          t4,       8f                // INVALID_REF2CUR

    vreplgr2vr.h  vr23,     t8
    vshuf.b       vr6,      vr14,     vr12,    vr10
    vshuf.b       vr7,      vr15,     vr13,    vr10
    vilvl.b       vr8,      vr7,      vr6
    vmulwev.w.h   vr6,      vr8,      vr23
    vmulwod.w.h   vr7,      vr8,      vr23

    vpickve2gr.b  s0,       vr0,      4       // ref
    slli.d        t8,       s0,       3
    ldx.d         s1,       s6,       t8      // rf->rp_ref[ref]
    addi.d        s0,       s0,       -4      // ref_sign
    vreplgr2vr.h  vr19,     s0
    add.d         s1,       s1,       t6      // &rf->rp_ref[ref][row_start8 * stride]
    addi.d        s2,       a4,       0       // y
    vilvl.w       vr8,      vr7,      vr6
    vilvh.w       vr9,      vr7,      vr6
6:                                            // for (int y = row_start8;
    andi          s3,       s2,       0xff8

    addi.d        s4,       s3,       8
    blt           a4,       s3,       0f
    addi.d        s3,       a4,       0        // y_proj_start
0:
    blt           s4,       a5,       0f
    addi.d        s4,       a5,       0        // y_proj_end
0:
    addi.d        s5,       t0,       0        // x
7:                                             // for (int x = col_start8i;
    slli.d        a7,       s5,       2
    add.d         a7,       a7,       s5
    add.d         a7,       s1,       a7      // rb
    vld           vr3,      a7,       0       // [rb]
    vpickve2gr.b  t4,       vr3,      4       // b_ref
    beqz          t4,       .end_x
    vreplve.b     vr11,     vr10,     t4
    vpickve2gr.b  t7,       vr11,     4       // ref2ref
    beqz          t7,       .end_x
    vsllwil.w.h   vr4,      vr3,      0
    vreplgr2vr.w  vr6,      t4
    vshuf.w       vr6,      vr9,      vr8      // frac
    vmul.w        vr5,      vr6,      vr4
    vsrai.w       vr4,      vr5,      31
    vadd.w        vr4,      vr4,      vr5
    vssrarni.h.w  vr4,      vr4,      14
    vclip.h       vr4,      vr4,      vr20,    vr21  // offset
    vxor.v        vr5,      vr4,      vr19    // offset.x ^ ref_sign
    vori.b        vr5,      vr5,      0x1     // offset.x ^ ref_sign
    vabsd.h       vr4,      vr4,      vr18
    vsrli.h       vr4,      vr4,      6       // abs(offset.x) >> 6
    vsigncov.h    vr4,      vr5,      vr4     // apply_sign
    vpickve2gr.h  s0,       vr4,      0
    add.d         s0,       s2,       s0      // pos_y
    blt           s0,       s3,       .n_posy
    bge           s0,       s4,       .n_posy
    andi          s0,       s0,       0xf
    mul.w         s0,       s0,       t2      // pos
    vpickve2gr.h  t7,       vr4,      1
    add.d         t7,       t7,       s5      // pos_x
    add.d         s0,       t3,       s0      // rp_proj + pos

.loop_posx:
    andi          t4,       s5,       0xff8 // x_sb_align

    blt           t7,       a2,       .n_posx
    addi.d        t8,       t4,       -8
    blt           t7,       t8,       .n_posx

    bge           t7,       a3,       .n_posx
    addi.d        t4,       t4,       16
    bge           t7,       t4,       .n_posx

    slli.d        t4,       t7,       2
    add.d         t4,       t4,       t7      // pos_x * 5
    add.d         t4,       s0,       t4      // rp_proj[pos + pos_x]
    vstelm.w      vr3,      t4,       0,   0
    vstelm.b      vr11,     t4,       4,   4

.n_posx:
    addi.d        s5,       s5,       1       // x + 1
    bge           s5,       t1,       .ret_posx
    addi.d        a7,       a7,       5       // rb + 1
    vld           vr4,      a7,       0       // [rb]
    vseq.b        vr5,      vr4,      vr3

    vpickve2gr.d  t8,       vr5,      0
    cto.d         t8,       t8
    blt           t8,       s7,       7b

    addi.d        t7,       t7,       1       // pos_x + 1

    /*  Core computing loop expansion(sencond)  */
    andi          t4,       s5,       0xff8 // x_sb_align

    blt           t7,       a2,       .n_posx
    addi.d        t8,       t4,       -8
    blt           t7,       t8,       .n_posx

    bge           t7,       a3,       .n_posx
    addi.d        t4,       t4,       16
    bge           t7,       t4,       .n_posx

    slli.d        t4,       t7,       2
    add.d         t4,       t4,       t7      // pos_x * 5
    add.d         t4,       s0,       t4      // rp_proj[pos + pos_x]
    vstelm.w      vr3,      t4,       0,   0
    vstelm.b      vr11,     t4,       4,   4

    addi.d        s5,       s5,       1       // x + 1
    bge           s5,       t1,       .ret_posx
    addi.d        a7,       a7,       5       // rb + 1
    vld           vr4,      a7,       0       // [rb]
    vseq.b        vr5,      vr4,      vr3

    vpickve2gr.d  t8,       vr5,      0
    cto.d         t8,       t8
    blt           t8,       s7,       7b

    addi.d        t7,       t7,       1       // pos_x + 1

    /*  Core computing loop expansion(third)  */
    andi          t4,       s5,       0xff8 // x_sb_align

    blt           t7,       a2,       .n_posx
    addi.d        t8,       t4,       -8
    blt           t7,       t8,       .n_posx

    bge           t7,       a3,       .n_posx
    addi.d        t4,       t4,       16
    bge           t7,       t4,       .n_posx

    slli.d        t4,       t7,       2
    add.d         t4,       t4,       t7      // pos_x * 5
    add.d         t4,       s0,       t4      // rp_proj[pos + pos_x]
    vstelm.w      vr3,      t4,       0,   0
    vstelm.b      vr11,     t4,       4,   4

    addi.d        s5,       s5,       1       // x + 1
    bge           s5,       t1,       .ret_posx
    addi.d        a7,       a7,       5       // rb + 1
    vld           vr4,      a7,       0       // [rb]
    vseq.b        vr5,      vr4,      vr3

    vpickve2gr.d  t8,       vr5,      0
    cto.d         t8,       t8
    blt           t8,       s7,       7b

    addi.d        t7,       t7,       1       // pos_x + 1

    b             .loop_posx

.n_posy:
    addi.d        s5,       s5,       1       // x + 1
    bge           s5,       t1,       .ret_posx
    addi.d        a7,       a7,       5       // rb + 1
    vld           vr4,      a7,       0       // [rb]
    vseq.b        vr5,      vr4,      vr3

    vpickve2gr.d  t8,       vr5,      0
    cto.d         t8,       t8
    blt           t8,       s7,       7b

    addi.d        s5,       s5,       1       // x + 1
    bge           s5,       t1,       .ret_posx
    addi.d        a7,       a7,       5       // rb + 1
    vld           vr4,      a7,       0       // [rb]
    vseq.b        vr5,      vr4,      vr3

    vpickve2gr.d  t8,       vr5,      0
    cto.d         t8,       t8
    blt           t8,       s7,       7b

    b             .n_posy

.end_x:
    addi.d        s5,       s5,       1       // x + 1
    blt           s5,       t1,       7b

.ret_posx:
    add.d         s1,       s1,       t2      // r + stride
    addi.d        s2,       s2,       1       // y + 1
    blt           s2,       a5,       6b
8:
    addi.d        a6,       a6,       1       // n + 1
    addi.d        t5,       t5,       7       // mfmv_ref2ref(offset) + 7
    blt           a6,       s8,       5b

.end_load:
    ld.d           s0,      sp,       0
    ld.d           s1,      sp,       8
    ld.d           s2,      sp,       16
    ld.d           s3,      sp,       24
    ld.d           s4,      sp,       32
    ld.d           s5,      sp,       40
    ld.d           s6,      sp,       48
    ld.d           s7,      sp,       56
    ld.d           s8,      sp,       64
    addi.d         sp,      sp,       80
endfunc

const mv_tbls
    .byte           255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255
    .byte           0, 1, 2, 3, 8, 0, 1, 2, 3, 8, 0, 1, 2, 3, 8, 0
    .byte           4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4
    .byte           4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4, 5, 6, 7, 9, 4
endconst

const mask_mult
    .byte           1, 0, 2, 0, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0
endconst

const mask_mv0
    .byte           1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
endconst

const mask_mv1
    .byte           4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19
endconst

// void dav1d_save_tmvs_lsx(refmvs_temporal_block *rp, ptrdiff_t stride,
//                          refmvs_block **rr, const uint8_t *ref_sign,
//                          int col_end8, int row_end8,
//                          int col_start8, int row_start8)
function save_tmvs_lsx
    addi.d      sp,         sp,        -0x28
    st.d        s0,         sp,         0x00
    st.d        s1,         sp,         0x08
    st.d        s2,         sp,         0x10
    st.d        s3,         sp,         0x18
    st.d        s4,         sp,         0x20
    move        t0,         ra

    vxor.v      vr10,       vr10,       vr10
    vld         vr11,       a3,         0       // Load ref_sign[0] ~ Load ref_sign[7]
    la.local    t2,         .save_tevs_tbl
    la.local    s1,         mask_mult
    la.local    t7,         mv_tbls
    vld         vr9,        s1,         0       // Load mask_mult
    vslli.d     vr11,       vr11,       8       // 0, ref_sign[0], ... ,ref_sign[6]
    la.local    s3,         mask_mv0
    vld         vr8,        s3,         0       // Load mask_mv0
    la.local    s4,         mask_mv1
    vld         vr7,        s4,         0       // Load mask_mv1
    li.d        s0,         5
    li.d        t8,         12 * 2
    mul.d       a1,         a1,         s0     // stride *= 5
    sub.d       a5,         a5,         a7      // h = row_end8 - row_start8
    slli.d      a7,         a7,         1       // row_start8 <<= 1
1:
    li.d        s0,         5
    andi        t3,         a7,         30      // (y & 15) * 2
    slli.d      s4,         t3,         3
    ldx.d       t3,         a2,         s4      // b = rr[(y & 15) * 2]
    addi.d      t3,         t3,         12      // &b[... + 1]
    mul.d       s4,         a4,         t8
    add.d       t4,         s4,         t3      // end_cand_b = &b[col_end8*2 + 1]
    mul.d       s3,         a6,         t8
    add.d       t3,         s3,         t3      // cand_b = &b[x*2 + 1]
    mul.d       s4,         a6,         s0
    add.d       a3,         s4,         a0      // &rp[x]
2:
    /* First cand_b */
    ld.b        t5,         t3,         10      // cand_b->bs
    vld         vr0,        t3,         0       // cand_b->mv and ref
    alsl.d      t5,         t5,         t2,     2  // bt2 index
    ld.h        s3,         t3,         8       // cand_b->ref
    ld.h        t6,         t5,         0       // bt2
    move        s0,         t2
    alsl.d      t3,         t6,         t3,     1   // Next cand_b += bt2 * 2
    vor.v       vr2,        vr0,        vr0
    vinsgr2vr.h vr1,        s3,         0
    move        t1 ,        t3
    bge         t3,         t4,        3f

    /* Next cand_b */
    ld.b        s0,         t3,         10      // cand_b->bs
    vld         vr4,        t3,         0       // cand_b->mv and ref
    alsl.d      s0,         s0,         t2,     2 // bt2 index
    ld.h        s4,         t3,         8       // cand_b->ref
    ld.h        t6,         s0,         0       // bt2
    alsl.d      t3,         t6,         t3,     1   // Next cand_b += bt2*2
    vpackev.d   vr2,        vr4,        vr0     // a0.mv[0] a0.mv[1] a1.mv[0], a1.mv[1]
    vinsgr2vr.h vr1,        s4,         1   // a0.ref[0] a0.ref[1], a1.ref[0], a1.ref[1]
3:
    vabsd.h     vr2,        vr2,        vr10    // abs(mv[].xy)
    vsle.b      vr16,       vr10,       vr1
    vand.v      vr1,        vr16,       vr1
    vshuf.b     vr1,        vr11,       vr11,   vr1     // ref_sign[ref]
    vsrli.h     vr2,        vr2,        12      // abs(mv[].xy) >> 12
    vilvl.b     vr1,        vr1,        vr1
    vmulwev.h.bu    vr1,    vr1,        vr9    // ef_sign[ref] * {1, 2}

    vseqi.w     vr2,        vr2,        0       // abs(mv[].xy) <= 4096
    vpickev.h   vr2,        vr2,        vr2     // abs() condition to 16 bit

    vand.v      vr1,        vr2,        vr1     // h[0-3] contains conditions for mv[0-1]
    vhaddw.wu.hu    vr1,    vr1,        vr1     // Combine condition for [1] and [0]
    vpickve2gr.wu   s1,     vr1,        0       // Extract case for first block
    vpickve2gr.wu   s2,     vr1,        1

    ld.hu           t5,     t5,         2       // Fetch jump table entry
    ld.hu           s0,     s0,         2
    alsl.d          s3,     s1,         t7,    4   // Load permutation table base on case
    vld             vr1,    s3,         0
    alsl.d          s4,     s2,         t7,    4
    vld             vr5,    s4,         0
    sub.d           t5,     t2,         t5     // Find jump table target
    sub.d           s0,     t2,         s0

    vshuf.b         vr0,    vr0,        vr0,    vr1 // Permute cand_b to output refmvs_temporal_block
    vshuf.b         vr4,    vr4,        vr4,    vr5
    vsle.b          vr16,   vr10,       vr1
    vand.v          vr0,    vr16,       vr0

    vsle.b          vr17,   vr10,       vr5
    vand.v          vr4,    vr17,       vr4
    // v1 follows on v0, with another 3 full repetitions of the pattern.
    vshuf.b         vr1,    vr0,        vr0,    vr8 // 1, 2, 3, ... , 15, 16
    vshuf.b         vr5,    vr4,        vr4,    vr8 // 1, 2, 3, ... , 15, 16
    // v2 ends with 3 complete repetitions of the pattern.
    vshuf.b         vr2,    vr1,        vr0,    vr7
    vshuf.b         vr6,    vr5,        vr4,    vr7    // 4, 5, 6, 7, ... , 12, 13, 14, 15, 16, 17, 18, 19

    jirl            ra,     t5,         0
    bge             t1 ,    t4,         4f      // if (cand_b >= end)
    vor.v           vr0,    vr4,        vr4
    vor.v           vr1,    vr5,        vr5
    vor.v           vr2,    vr6,        vr6
    jirl            ra,     s0,         0
    blt             t3,     t4,         2b      // if (cand_b < end)

4:
    addi.d          a5,     a5,         -1      // h--
    addi.d          a7,     a7,         2       // y += 2
    add.d           a0,     a0,         a1      // rp += stride
    blt             zero,   a5,         1b

    ld.d        s0,         sp,         0x00
    ld.d        s1,         sp,         0x08
    ld.d        s2,         sp,         0x10
    ld.d        s3,         sp,         0x18
    ld.d        s4,         sp,         0x20
    addi.d      sp,         sp,         0x28

    move            ra,     t0
    jirl            zero,   ra,         0x00

10:
    addi.d          s1,     a3,         4
    vstelm.w        vr0,    a3,         0,      0   // .mv
    vstelm.b        vr0,    s1,         0,      4   // .ref
    addi.d          a3,     a3,         5
    jirl            zero,   ra,         0x00
20:
    addi.d          s1,     a3,         8
    vstelm.d        vr0,    a3,         0,      0   // .mv
    vstelm.h        vr0,    s1,         0,      4   // .ref
    addi.d          a3,     a3,         2 * 5
    jirl            zero,   ra,         0x00
40:
    vst             vr0,    a3,         0
    vstelm.w        vr1,    a3,         0x10,   0
    addi.d          a3,     a3,         4 * 5
    jirl            zero,   ra,         0x00

80:
    vst             vr0,    a3,         0
    vst             vr1,    a3,         0x10           // This writes 6 full entries plus 2 extra bytes
    vst             vr2,    a3,         5 * 8 - 16     // Write the last few, overlapping with the first write.
    addi.d          a3,     a3,         8 * 5
    jirl            zero,   ra,         0x00
160:
    addi.d          s1,     a3,         6 * 5
    addi.d          s2,     a3,         12 * 5
    vst             vr0,    a3,         0
    vst             vr1,    a3,         0x10          // This writes 6 full entries plus 2 extra bytes
    vst             vr0,    a3,         6 * 5
    vst             vr1,    a3,         6 * 5 + 16    // Write another 6 full entries, slightly overlapping with the first set
    vstelm.d        vr0,    s2,         0,      0     // Write 8 bytes (one full entry) after the first 12
    vst             vr2,    a3,         5 * 16 - 16   // Write the last 3 entries
    addi.d          a3,     a3,         16 * 5
    jirl            zero,   ra,         0x00

.save_tevs_tbl:
        .hword 16 * 12   // bt2 * 12, 12 is sizeof(refmvs_block)
        .hword .save_tevs_tbl - 160b
        .hword 16 * 12
        .hword .save_tevs_tbl - 160b
        .hword 8 * 12
        .hword .save_tevs_tbl -  80b
        .hword 8 * 12
        .hword .save_tevs_tbl -  80b
        .hword 8 * 12
        .hword .save_tevs_tbl -  80b
        .hword 8 * 12
        .hword .save_tevs_tbl -  80b
        .hword 4 * 12
        .hword .save_tevs_tbl -  40b
        .hword 4 * 12
        .hword .save_tevs_tbl -  40b
        .hword 4 * 12
        .hword .save_tevs_tbl -  40b
        .hword 4 * 12
        .hword .save_tevs_tbl -  40b
        .hword 2 * 12
        .hword .save_tevs_tbl -  20b
        .hword 2 * 12
        .hword .save_tevs_tbl -  20b
        .hword 2 * 12
        .hword .save_tevs_tbl -  20b
        .hword 2 * 12
        .hword .save_tevs_tbl -  20b
        .hword 2 * 12
        .hword .save_tevs_tbl -  20b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
        .hword 1 * 12
        .hword .save_tevs_tbl -  10b
endfunc

