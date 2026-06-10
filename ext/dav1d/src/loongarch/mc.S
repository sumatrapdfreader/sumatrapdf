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
static void warp_affine_8x8_c(pixel *dst, const ptrdiff_t dst_stride,
                              const pixel *src, const ptrdiff_t src_stride,
                              const int16_t *const abcd, int mx, int my
                              HIGHBD_DECL_SUFFIX)
*/
.macro vld_filter_row dst, src, inc
    addi.w          t3,       \src,     512
    srai.w          t3,       t3,       10
    add.w           \src,     \src,     \inc
    addi.w          t3,       t3,       64
    slli.w          t3,       t3,       3
    fldx.d          \dst,     t4,       t3
.endm

.macro warp_filter_horz_lsx
    addi.w          t5,       a5,       0
    vld             vr10,     a2,       0
    add.d           a2,       a2,       a3

    vld_filter_row f0, t5, t0
    vld_filter_row f1, t5, t0
    vld_filter_row f2, t5, t0
    vld_filter_row f3, t5, t0
    vld_filter_row f4, t5, t0
    vld_filter_row f5, t5, t0
    vld_filter_row f6, t5, t0
    vld_filter_row f7, t5, t0

    vxor.v          vr10,     vr10,     vr20

    vbsrl.v         vr8,      vr10,     1
    vbsrl.v         vr9,      vr10,     2
    vilvl.d         vr8,      vr8,      vr10
    vilvl.d         vr0,      vr1,      vr0
    vmulwev.h.b     vr11,     vr8,      vr0
    vmulwod.h.b     vr12,     vr8,      vr0
    vbsrl.v         vr8,      vr10,     3
    vbsrl.v         vr19,     vr10,     4
    vilvl.d         vr8,      vr8,      vr9
    vilvl.d         vr2,      vr3,      vr2
    vmulwev.h.b     vr13,     vr8,      vr2
    vmulwod.h.b     vr14,     vr8,      vr2
    vbsrl.v         vr8,      vr10,     5
    vbsrl.v         vr9,      vr10,     6
    vilvl.d         vr8,      vr8,      vr19
    vilvl.d         vr4,      vr5,      vr4
    vmulwev.h.b     vr15,     vr8,      vr4
    vmulwod.h.b     vr16,     vr8,      vr4
    vbsrl.v         vr8,      vr10,     7
    vilvl.d         vr8,      vr8,      vr9
    vilvl.d         vr6,      vr7,      vr6
    vmulwev.h.b     vr17,     vr8,      vr6
    vmulwod.h.b     vr18,     vr8,      vr6

    vadd.h          vr11,     vr11,     vr12
    vadd.h          vr13,     vr13,     vr14
    vadd.h          vr15,     vr15,     vr16
    vadd.h          vr17,     vr17,     vr18
    vpickev.h       vr12,     vr13,     vr11
    vpickod.h       vr14,     vr13,     vr11
    vpickev.h       vr16,     vr17,     vr15
    vpickod.h       vr18,     vr17,     vr15
    vadd.h          vr11,     vr12,     vr14
    vadd.h          vr15,     vr16,     vr18
    vpickev.h       vr12,     vr15,     vr11
    vpickod.h       vr14,     vr15,     vr11
    vadd.h          vr11,     vr12,     vr14

    add.d           a5,       a5,       t1
.endm

.macro transpose_8x8b_extend_lsx in0, in1, in2, in3, in4, in5, in6, in7
    vilvl.b         \in0,     \in1,     \in0
    vilvl.b         \in2,     \in3,     \in2
    vilvl.b         \in4,     \in5,     \in4
    vilvl.b         \in6,     \in7,     \in6

    vpackev.h       \in1,     \in2,     \in0
    vpackod.h       \in3,     \in2,     \in0
    vpackev.h       \in5,     \in6,     \in4
    vpackod.h       \in7,     \in6,     \in4

    vpackev.w       \in0,     \in5,     \in1
    vpackod.w       \in2,     \in5,     \in1
    vpackev.w       \in1,     \in7,     \in3
    vpackod.w       \in3,     \in7,     \in3

    vexth.h.b       \in4,     \in0
    vsllwil.h.b     \in0,     \in0,     0
    vexth.h.b       \in5,     \in1
    vsllwil.h.b     \in1,     \in1,     0
    vexth.h.b       \in6,     \in2
    vsllwil.h.b     \in2,     \in2,     0
    vexth.h.b       \in7,     \in3
    vsllwil.h.b     \in3,     \in3,     0
.endm

.macro warp t, shift
function warp_affine_8x8\t\()_8bpc_lsx
    addi.d          sp,       sp,      -64
    fst.d           f24,      sp,      0
    fst.d           f25,      sp,      8
    fst.d           f26,      sp,      16
    fst.d           f27,      sp,      24
    fst.d           f28,      sp,      32
    fst.d           f29,      sp,      40
    fst.d           f30,      sp,      48
    fst.d           f31,      sp,      56

    ld.h            t0,       a4,      0
    ld.h            t1,       a4,      2
    ld.h            t2,       a4,      4
    ld.h            a4,       a4,      6

    li.d            t7,       8
    alsl.w          t3,       a3,      a3,     1
    sub.d           a2,       a2,      t3
    addi.d          a2,       a2,      -3
    la.local        t4,       dav1d_mc_warp_filter

.ifnb \t
    slli.d          a1,       a1,      1
.endif

    li.w            t3,       128
    vreplgr2vr.b    vr20,     t3
.ifb \t
    vreplgr2vr.h    vr21,     t3
.else
    li.w            t3,       2048
    vreplgr2vr.h    vr21,     t3
.endif
    warp_filter_horz_lsx
    vsrari.h        vr24,     vr11,    3
    warp_filter_horz_lsx
    vsrari.h        vr25,     vr11,    3
    warp_filter_horz_lsx
    vsrari.h        vr26,     vr11,    3
    warp_filter_horz_lsx
    vsrari.h        vr27,     vr11,    3
    warp_filter_horz_lsx
    vsrari.h        vr28,     vr11,    3
    warp_filter_horz_lsx
    vsrari.h        vr29,     vr11,    3
    warp_filter_horz_lsx
    vsrari.h        vr30,     vr11,    3

1:
    addi.d          t6,       a6,      0
    warp_filter_horz_lsx
    vsrari.h        vr31,     vr11,    3

    vld_filter_row f0, t6, t2
    vld_filter_row f1, t6, t2
    vld_filter_row f2, t6, t2
    vld_filter_row f3, t6, t2
    vld_filter_row f4, t6, t2
    vld_filter_row f5, t6, t2
    vld_filter_row f6, t6, t2
    vld_filter_row f7, t6, t2

    transpose_8x8b_extend_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    vmulwev.w.h     vr16,     vr24,    vr0
    vmulwod.w.h     vr17,     vr24,    vr0
    vmaddwev.w.h    vr16,     vr25,    vr1
    vmaddwod.w.h    vr17,     vr25,    vr1
    vmaddwev.w.h    vr16,     vr26,    vr2
    vmaddwod.w.h    vr17,     vr26,    vr2
    vmaddwev.w.h    vr16,     vr27,    vr3
    vmaddwod.w.h    vr17,     vr27,    vr3
    vmaddwev.w.h    vr16,     vr28,    vr4
    vmaddwod.w.h    vr17,     vr28,    vr4
    vmaddwev.w.h    vr16,     vr29,    vr5
    vmaddwod.w.h    vr17,     vr29,    vr5
    vmaddwev.w.h    vr16,     vr30,    vr6
    vmaddwod.w.h    vr17,     vr30,    vr6
    vmaddwev.w.h    vr16,     vr31,    vr7
    vmaddwod.w.h    vr17,     vr31,    vr7

    vssrarni.h.w    vr16,     vr16,    \shift
    vssrarni.h.w    vr17,     vr17,    \shift
    vilvl.h         vr16,     vr17,    vr16
    vadd.h          vr16,     vr16,    vr21

    vor.v           vr24,     vr25,    vr25
    vor.v           vr25,     vr26,    vr26
    vor.v           vr26,     vr27,    vr27
    vor.v           vr27,     vr28,    vr28
    vor.v           vr28,     vr29,    vr29
    vor.v           vr29,     vr30,    vr30
    vor.v           vr30,     vr31,    vr31

.ifb \t
    vssrarni.bu.h   vr16,     vr16,    0
.endif

    addi.d          t7,       t7,      -1
.ifnb \t
    vst             vr16,     a0,      0
.else
    vstelm.d        vr16,     a0,      0,   0
.endif
    add.d           a0,       a1,      a0

    add.d           a6,       a6,      a4
    blt             zero,     t7,      1b

    fld.d           f24,      sp,      0
    fld.d           f25,      sp,      8
    fld.d           f26,      sp,      16
    fld.d           f27,      sp,      24
    fld.d           f28,      sp,      32
    fld.d           f29,      sp,      40
    fld.d           f30,      sp,      48
    fld.d           f31,      sp,      56
    addi.d          sp,       sp,      64
endfunc
.endm

warp  , 11
warp t, 7

.macro FILTER_WARP_RND_P_LASX in0, in1, in2, out0, out1, out2, out3
    xvshuf.b        xr2,    \in0,     \in0,     \in2

    addi.w          t4,     \in1,     512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr3,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr4,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr5,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    addi.w          t4,     t3,       512
    srai.w          t4,     t4,       10
    addi.w          t4,     t4,       64
    slli.w          t4,     t4,       3
    vldx            vr6,    t5,       t4
    add.w           t3,     t3,       t0   // tmx += abcd[0]

    xvinsve0.d      xr3,    xr5,      1
    xvinsve0.d      xr3,    xr4,      2
    xvinsve0.d      xr3,    xr6,      3

    xvmulwev.h.bu.b xr4,    xr2,      xr3
    xvmulwod.h.bu.b xr5,    xr2,      xr3
    xvilvl.d        xr2,    xr5,      xr4
    xvilvh.d        xr3,    xr5,      xr4
    xvhaddw.w.h     xr2,    xr2,      xr2
    xvhaddw.w.h     xr3,    xr3,      xr3
    xvhaddw.d.w     xr2,    xr2,      xr2
    xvhaddw.d.w     xr3,    xr3,      xr3
    xvhaddw.q.d     xr2,    xr2,      xr2
    xvhaddw.q.d     xr3,    xr3,      xr3

    xvextrins.w     \out0,  xr2,      \out1
    xvextrins.w     \out2,  xr3,      \out3
.endm

.macro FILTER_WARP_CLIP_LASX in0, in1, in2, out0, out1
    add.w           \in0,     \in0,    \in1
    addi.w          t6,       \in0,    512
    srai.w          t6,       t6,      10
    addi.w          t6,       t6,      64
    slli.w          t6,       t6,      3
    fldx.d          f1,       t5,      t6

    add.w           t2,       t2,      t7
    addi.w          t6,       t2,      512
    srai.w          t6,       t6,      10
    addi.w          t6,       t6,      64
    slli.w          t6,       t6,      3
    fldx.d          f2,       t5,      t6

    vilvl.d         vr0,      vr2,     vr1
    vext2xv.h.b     xr0,      xr0
    xvmulwev.w.h    xr3,      \in2,    xr0
    xvmaddwod.w.h   xr3,      \in2,    xr0
    xvhaddw.d.w     xr3,      xr3,     xr3
    xvhaddw.q.d     xr3,      xr3,     xr3
    xvextrins.w     \out0,    xr3,     \out1
.endm

const shuf0
.byte  0, 1, 2, 3, 4, 5, 6, 7, 2, 3, 4, 5, 6, 7, 8, 9
.byte  1, 2, 3, 4, 5, 6, 7, 8, 3, 4, 5, 6, 7, 8, 9, 10
endconst

const warp_sh
.rept 2
.byte 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
.endr
.rept 2
.byte 18, 19, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
.endr
endconst

.macro warp_lasx t, shift
function warp_affine_8x8\t\()_8bpc_lasx
    addi.d          sp,       sp,      -16
    ld.h            t0,       a4,      0   // abcd[0]
    ld.h            t1,       a4,      2   // abcd[1]
    fst.d           f24,      sp,      0
    fst.d           f25,      sp,      8

    alsl.w          t2,       a3,      a3,     1
    addi.w          t3,       a5,      0
    la.local        t4,       warp_sh
    la.local        t5,       dav1d_mc_warp_filter
    sub.d           a2,       a2,      t2
    addi.d          a2,       a2,      -3
    vld             vr0,      a2,      0
    xvld            xr24,     t4,      0
    xvld            xr25,     t4,      32
    la.local        t2,       shuf0
    xvld            xr1,      t2,      0
    xvpermi.q       xr0,      xr0,     0x00
    xvaddi.bu        xr9,    xr1,      4
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x00, xr8, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x00, xr11, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x10, xr8, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x10, xr11, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x20, xr8, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x20, xr11, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x30, xr8, 0x30
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x30, xr11, 0x30

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x00, xr13, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x00, xr15, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x10, xr13, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x10, xr15, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x20, xr13, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x20, xr15, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr12, 0x30, xr13, 0x30
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr14, 0x30, xr15, 0x30

    xvsrarni.h.w    xr12,     xr7,     3
    xvsrarni.h.w    xr13,     xr8,     3
    xvsrarni.h.w    xr14,     xr10,    3
    xvsrarni.h.w    xr15,     xr11,    3

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x00, xr8, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x00, xr11, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x10, xr8, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x10, xr11, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x20, xr8, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x20, xr11, 0x20

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr7, 0x30, xr8, 0x30
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr10, 0x30, xr11, 0x30

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr16, 0x00, xr17, 0x00
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr18, 0x00, xr19, 0x00

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr16, 0x10, xr17, 0x10
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr18, 0x10, xr19, 0x10

    add.w           a5,       a5,      t1
    or              t3,       a5,      a5
    add.d           a2,       a2,      a3
    vld             vr0,      a2,      0
    xvpermi.q       xr0,      xr0,     0x00
    FILTER_WARP_RND_P_LASX xr0, a5, xr1, xr16, 0x20, xr17, 0x20
    FILTER_WARP_RND_P_LASX xr0, t3, xr9, xr18, 0x20, xr19, 0x20

    xvsrarni.h.w    xr16,     xr7,     3
    xvsrarni.h.w    xr17,     xr8,     3
    xvsrarni.h.w    xr18,     xr10,    3
    xvsrarni.h.w    xr19,     xr11,    3

    addi.w          t2,       a6,      0   // my
    ld.h            t7,       a4,      4   // abcd[2]
    ld.h            t8,       a4,      6   // abcd[3]

.ifnb \t
    slli.d          a1,       a1,      1
.endif

    // y = 0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     \shift
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    alsl.d          a0,       a1,       a0,     1
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     11
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    add.d            a0,      a0,       a1
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    alsl.d          a0,       a1,       a0,     1
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     11
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    add.d            a0,      a0,       a1
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif

    xvaddi.bu        xr25,     xr25,    2
    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24
    xvextrins.h      xr24,     xr25,    0x70

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr20, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr20, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr20, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr20, 0x30

    xvshuf.b         xr12,     xr16,    xr12,   xr24
    xvshuf.b         xr13,     xr17,    xr13,   xr24
    xvshuf.b         xr14,     xr18,    xr14,   xr24
    xvshuf.b         xr15,     xr19,    xr15,   xr24

    add.w           a6,       a6,      t8
    addi.w          t2,       a6,      0
    FILTER_WARP_CLIP_LASX  t2, zero, xr12,  xr21, 0x00
    FILTER_WARP_CLIP_LASX  t2, t7,   xr13,  xr21, 0x10
    FILTER_WARP_CLIP_LASX  t2, t7,   xr14,  xr21, 0x20
    FILTER_WARP_CLIP_LASX  t2, t7,   xr15,  xr21, 0x30

.ifnb \t
    xvssrarni.h.w   xr21,     xr20,     \shift
    alsl.d          a0,       a1,       a0,     1
    xvpermi.q       xr22,     xr21,     0x01
    vilvl.h         vr23,     vr22,     vr21
    vilvh.h         vr21,     vr22,     vr21
    vst             vr23,     a0,       0
    vstx            vr21,     a0,       a1
.else
    xvssrarni.hu.w   xr21,    xr20,     11
    xvssrlni.bu.h    xr22,    xr21,     0
    xvpermi.q        xr23,    xr22,     0x01
    vilvl.b          vr21,    vr23,     vr22
    add.d            a0,      a0,       a1
    fst.d            f21,     a0,       0
    add.d            a0,      a0,       a1
    vstelm.d         vr21,    a0,       0,     1
.endif
    fld.d            f24,     sp,       0
    fld.d            f25,     sp,       8
    addi.d           sp,      sp,       16
endfunc
.endm

warp_lasx , 11
warp_lasx t, 7

/*
static void w_avg_c(pixel *dst, const ptrdiff_t dst_stride,
                    const int16_t *tmp1, const int16_t *tmp2,
                    const int w, int h,
                    const int weight HIGHBD_DECL_SUFFIX)
*/

#define bpc8_sh     5     // sh = intermediate_bits + 1
#define bpcw8_sh    8     // sh = intermediate_bits + 4

#define bpc_sh   bpc8_sh
#define bpcw_sh  bpcw8_sh

function avg_8bpc_lsx
    addi.d        t8,     a0,     0

    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .AVG_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0  // The jump addresses are relative to AVG_LSX_JRTABLE
    add.d         t1,     t1,      t2 // Get absolute address
    jirl          $r0,    t1,      0

    .align   3
.AVG_LSX_JRTABLE:
    .hword .AVG_W128_LSX - .AVG_LSX_JRTABLE
    .hword .AVG_W64_LSX  - .AVG_LSX_JRTABLE
    .hword .AVG_W32_LSX  - .AVG_LSX_JRTABLE
    .hword .AVG_W16_LSX  - .AVG_LSX_JRTABLE
    .hword .AVG_W8_LSX   - .AVG_LSX_JRTABLE
    .hword .AVG_W4_LSX   - .AVG_LSX_JRTABLE

.AVG_W4_LSX:
    vld           vr0,    a2,     0
    vld           vr1,    a3,     0
    vadd.h        vr2,    vr0,    vr1
    vssrarni.bu.h vr3,    vr2,    bpc_sh
    vstelm.w      vr3,    a0,     0,    0
    add.d         a0,     a0,     a1
    vstelm.w      vr3,    a0,     0,    1
    addi.w        a5,     a5,     -2
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W4_LSX
    b             .AVG_END_LSX

.AVG_W8_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr4,    vr0,    vr1
    vadd.h        vr5,    vr2,    vr3
    vssrarni.bu.h vr5,    vr4,    bpc_sh
    addi.w        a5,     a5,     -2
    addi.d        a2,     a2,     32
    vstelm.d      vr5,    a0,     0,    0
    add.d         a0,     a0,     a1
    vstelm.d      vr5,    a0,     0,    1
    addi.d        a3,     a3,     32
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W8_LSX
    b             .AVG_END_LSX

.AVG_W16_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr4,    vr0,    vr1
    vadd.h        vr5,    vr2,    vr3
    vssrarni.bu.h vr5,    vr4,    bpc_sh
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     32
    vst           vr5,    a0,     0
    addi.d        a3,     a3,     32
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W16_LSX
    b             .AVG_END_LSX

.AVG_W32_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr4,    a2,     32
    vld           vr6,    a2,     48
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vld           vr5,    a3,     32
    vld           vr7,    a3,     48
    vadd.h        vr0,    vr0,    vr1
    vadd.h        vr2,    vr2,    vr3
    vadd.h        vr4,    vr4,    vr5
    vadd.h        vr6,    vr6,    vr7
    vssrarni.bu.h vr2,    vr0,    bpc_sh
    vssrarni.bu.h vr6,    vr4,    bpc_sh
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     64
    vst           vr2,    a0,     0
    vst           vr6,    a0,     16
    addi.d        a3,     a3,     64
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .AVG_W32_LSX
    b             .AVG_END_LSX

.AVG_W64_LSX:
.rept 4
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr0,    vr0,    vr1
    vadd.h        vr2,    vr2,    vr3
    vssrarni.bu.h vr2,    vr0,    bpc_sh
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    vst           vr2,    a0,     0
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .AVG_W64_LSX
    b             .AVG_END_LSX

.AVG_W128_LSX:
.rept 8
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vadd.h        vr0,    vr0,    vr1
    vadd.h        vr2,    vr2,    vr3
    vssrarni.bu.h vr2,    vr0,    bpc_sh
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    vst           vr2,    a0,     0
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .AVG_W128_LSX
.AVG_END_LSX:
endfunc

function avg_8bpc_lasx
    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .AVG_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.AVG_LASX_JRTABLE:
    .hword .AVG_W128_LASX - .AVG_LASX_JRTABLE
    .hword .AVG_W64_LASX  - .AVG_LASX_JRTABLE
    .hword .AVG_W32_LASX  - .AVG_LASX_JRTABLE
    .hword .AVG_W16_LASX  - .AVG_LASX_JRTABLE
    .hword .AVG_W8_LASX   - .AVG_LASX_JRTABLE
    .hword .AVG_W4_LASX   - .AVG_LASX_JRTABLE

.AVG_W4_LASX:
    vld            vr0,    a2,     0
    vld            vr1,    a3,     0
    vadd.h         vr0,    vr0,    vr1
    vssrarni.bu.h  vr1,    vr0,    bpc_sh
    vstelm.w       vr1,    a0,     0,    0
    add.d          a0,     a0,     a1
    vstelm.w       vr1,    a0,     0,    1
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     16
    addi.d         a3,     a3,     16
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W4_LASX
    b              .AVG_END_LASX
.AVG_W8_LASX:
    xvld           xr0,    a2,     0
    xvld           xr1,    a3,     0
    xvadd.h        xr2,    xr0,    xr1
    xvssrarni.bu.h xr1,    xr2,    bpc_sh
    xvstelm.d      xr1,    a0,     0,    0
    add.d          a0,     a0,     a1
    xvstelm.d      xr1,    a0,     0,    2
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     32
    addi.d         a3,     a3,     32
    add.d          a0,     a1,     a0
    blt            zero,   a5,     .AVG_W8_LASX
    b              .AVG_END_LASX
.AVG_W16_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvadd.h        xr4,    xr0,    xr1
    xvadd.h        xr5,    xr2,    xr3
    xvssrarni.bu.h xr5,    xr4,    bpc_sh
    xvpermi.d      xr2,    xr5,    0xd8
    xvpermi.d      xr3,    xr5,    0x8d
    vst            vr2,    a0,     0
    vstx           vr3,    a0,     a1
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    alsl.d         a0,     a1,     a0,   1
    blt            zero,   a5,     .AVG_W16_LASX
    b              .AVG_END_LASX
.AVG_W32_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvadd.h        xr4,    xr0,    xr1
    xvadd.h        xr5,    xr2,    xr3
    xvssrarni.bu.h xr5,    xr4,    bpc_sh
    xvpermi.d      xr6,    xr5,    0xd8
    xvst           xr6,    a0,     0
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W32_LASX
    b              .AVG_END_LASX
.AVG_W64_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr4,    a2,     64
    xvld           xr6,    a2,     96
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvld           xr5,    a3,     64
    xvld           xr7,    a3,     96
    xvadd.h        xr0,    xr0,    xr1
    xvadd.h        xr2,    xr2,    xr3
    xvadd.h        xr4,    xr4,    xr5
    xvadd.h        xr6,    xr6,    xr7
    xvssrarni.bu.h xr2,    xr0,    bpc_sh
    xvssrarni.bu.h xr6,    xr4,    bpc_sh
    xvpermi.d      xr1,    xr2,    0xd8
    xvpermi.d      xr3,    xr6,    0xd8
    xvst           xr1,    a0,     0
    xvst           xr3,    a0,     32
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     128
    addi.d         a3,     a3,     128
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W64_LASX
    b              .AVG_END_LASX
.AVG_W128_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr4,    a2,     64
    xvld           xr6,    a2,     96
    xvld           xr8,    a2,     128
    xvld           xr10,   a2,     160
    xvld           xr12,   a2,     192
    xvld           xr14,   a2,     224
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvld           xr5,    a3,     64
    xvld           xr7,    a3,     96
    xvld           xr9,    a3,     128
    xvld           xr11,   a3,     160
    xvld           xr13,   a3,     192
    xvld           xr15,   a3,     224
    xvadd.h        xr0,    xr0,    xr1
    xvadd.h        xr2,    xr2,    xr3
    xvadd.h        xr4,    xr4,    xr5
    xvadd.h        xr6,    xr6,    xr7
    xvadd.h        xr8,    xr8,    xr9
    xvadd.h        xr10,   xr10,   xr11
    xvadd.h        xr12,   xr12,   xr13
    xvadd.h        xr14,   xr14,   xr15
    xvssrarni.bu.h xr2,    xr0,    bpc_sh
    xvssrarni.bu.h xr6,    xr4,    bpc_sh
    xvssrarni.bu.h xr10,   xr8,    bpc_sh
    xvssrarni.bu.h xr14,   xr12,   bpc_sh
    xvpermi.d      xr1,    xr2,    0xd8
    xvpermi.d      xr3,    xr6,    0xd8
    xvpermi.d      xr5,    xr10,   0xd8
    xvpermi.d      xr7,    xr14,   0xd8
    xvst           xr1,    a0,     0
    xvst           xr3,    a0,     32
    xvst           xr5,    a0,     64
    xvst           xr7,    a0,     96
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     256
    addi.d         a3,     a3,     256
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .AVG_W128_LASX
.AVG_END_LASX:
endfunc

function w_avg_8bpc_lsx
    addi.d        t8,     a0,     0
    li.w          t2,     16
    sub.w         t2,     t2,     a6  // 16 - weight
    vreplgr2vr.h  vr21,   a6
    vreplgr2vr.h  vr22,   t2

    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .W_AVG_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.W_AVG_LSX_JRTABLE:
    .hword .W_AVG_W128_LSX - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W64_LSX  - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W32_LSX  - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W16_LSX  - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W8_LSX   - .W_AVG_LSX_JRTABLE
    .hword .W_AVG_W4_LSX   - .W_AVG_LSX_JRTABLE

.W_AVG_W4_LSX:
    vld           vr0,    a2,     0
    vld           vr1,    a3,     0
    vmulwev.w.h   vr2,    vr0,    vr21
    vmulwod.w.h   vr3,    vr0,    vr21
    vmaddwev.w.h  vr2,    vr1,    vr22
    vmaddwod.w.h  vr3,    vr1,    vr22
    vssrarni.hu.w vr3,    vr2,    bpcw_sh
    vssrlni.bu.h  vr1,    vr3,    0
    vpickod.w     vr4,    vr2,    vr1
    vilvl.b       vr0,    vr4,    vr1
    fst.s         f0,     a0,     0
    add.d         a0,     a0,     a1
    vstelm.w      vr0,    a0,     0,   1
    addi.w        a5,     a5,     -2
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    add.d         a0,     a1,     a0
    blt           zero,   a5,     .W_AVG_W4_LSX
    b             .W_AVG_END_LSX
.W_AVG_W8_LSX:
    vld           vr0,    a2,     0
    vld           vr1,    a3,     0
    vmulwev.w.h   vr2,    vr0,    vr21
    vmulwod.w.h   vr3,    vr0,    vr21
    vmaddwev.w.h  vr2,    vr1,    vr22
    vmaddwod.w.h  vr3,    vr1,    vr22
    vssrarni.hu.w vr3,    vr2,    bpcw_sh
    vssrlni.bu.h  vr1,    vr3,    0
    vpickod.w     vr4,    vr2,    vr1
    vilvl.b       vr0,    vr4,    vr1
    fst.d         f0,     a0,     0
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .W_AVG_W8_LSX
    b             .W_AVG_END_LSX
.W_AVG_W16_LSX:
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.w        a5,     a5,     -1
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    add.d         a0,     a0,     a1
    blt           zero,   a5,     .W_AVG_W16_LSX
    b             .W_AVG_END_LSX
.W_AVG_W32_LSX:
.rept 2
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .W_AVG_W32_LSX
    b             .W_AVG_END_LSX

.W_AVG_W64_LSX:
.rept 4
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .W_AVG_W64_LSX
    b             .W_AVG_END_LSX

.W_AVG_W128_LSX:
.rept 8
    vld           vr0,    a2,     0
    vld           vr2,    a2,     16
    vld           vr1,    a3,     0
    vld           vr3,    a3,     16
    vmulwev.w.h   vr4,    vr0,    vr21
    vmulwod.w.h   vr5,    vr0,    vr21
    vmulwev.w.h   vr6,    vr2,    vr21
    vmulwod.w.h   vr7,    vr2,    vr21
    vmaddwev.w.h  vr4,    vr1,    vr22
    vmaddwod.w.h  vr5,    vr1,    vr22
    vmaddwev.w.h  vr6,    vr3,    vr22
    vmaddwod.w.h  vr7,    vr3,    vr22
    vssrarni.hu.w vr6,    vr4,    bpcw_sh
    vssrarni.hu.w vr7,    vr5,    bpcw_sh
    vssrlrni.bu.h vr7,    vr6,    0
    vshuf4i.w     vr8,    vr7,    0x4E
    vilvl.b       vr0,    vr8,    vr7
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a0,     a0,     16
.endr
    addi.w        a5,     a5,     -1
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    blt           zero,   a5,     .W_AVG_W128_LSX
.W_AVG_END_LSX:
endfunc

function w_avg_8bpc_lasx
    addi.d        t8,     a0,     0
    li.w          t2,     16
    sub.w         t2,     t2,     a6  // 16 - weight
    xvreplgr2vr.h xr21,   a6
    xvreplgr2vr.h xr22,   t2

    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .W_AVG_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.W_AVG_LASX_JRTABLE:
    .hword .W_AVG_W128_LASX - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W64_LASX  - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W32_LASX  - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W16_LASX  - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W8_LASX   - .W_AVG_LASX_JRTABLE
    .hword .W_AVG_W4_LASX   - .W_AVG_LASX_JRTABLE

.W_AVG_W4_LASX:
    vld            vr0,    a2,     0
    vld            vr1,    a3,     0
    xvpermi.d      xr2,    xr0,    0xD8
    xvpermi.d      xr3,    xr1,    0xD8
    xvilvl.h       xr4,    xr3,    xr2
    xvmulwev.w.h   xr0,    xr4,    xr21
    xvmaddwod.w.h  xr0,    xr4,    xr22
    xvssrarni.hu.w xr1,    xr0,    bpcw_sh
    xvssrlni.bu.h  xr0,    xr1,    0
    fst.s          f0,     a0,     0
    add.d          a0,     a0,     a1
    xvstelm.w      xr0,    a0,     0,     4
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     16
    addi.d         a3,     a3,     16
    add.d          a0,     a1,     a0
    blt            zero,   a5,     .W_AVG_W4_LASX
    b              .W_AVG_END_LASX

.W_AVG_W8_LASX:
    xvld           xr0,    a2,     0
    xvld           xr1,    a3,     0
    xvmulwev.w.h   xr2,    xr0,    xr21
    xvmulwod.w.h   xr3,    xr0,    xr21
    xvmaddwev.w.h  xr2,    xr1,    xr22
    xvmaddwod.w.h  xr3,    xr1,    xr22
    xvssrarni.hu.w xr3,    xr2,    bpcw_sh
    xvssrlni.bu.h  xr1,    xr3,    0
    xvpickod.w     xr4,    xr2,    xr1
    xvilvl.b       xr0,    xr4,    xr1
    xvstelm.d      xr0,    a0,     0,     0
    add.d          a0,     a0,     a1
    xvstelm.d      xr0,    a0,     0,     2
    addi.w         a5,     a5,     -2
    addi.d         a2,     a2,     32
    addi.d         a3,     a3,     32
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .W_AVG_W8_LASX
    b              .W_AVG_END_LASX

.W_AVG_W16_LASX:
    xvld           xr0,    a2,     0
    xvld           xr1,    a3,     0
    xvmulwev.w.h   xr2,    xr0,    xr21
    xvmulwod.w.h   xr3,    xr0,    xr21
    xvmaddwev.w.h  xr2,    xr1,    xr22
    xvmaddwod.w.h  xr3,    xr1,    xr22
    xvssrarni.hu.w xr3,    xr2,    bpcw_sh
    xvssrlni.bu.h  xr1,    xr3,    0
    xvpickod.w     xr4,    xr2,    xr1
    xvilvl.b       xr0,    xr4,    xr1
    xvpermi.d      xr1,    xr0,    0xD8
    vst            vr1,    a0,     0
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     32
    addi.d         a3,     a3,     32
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .W_AVG_W16_LASX
    b              .W_AVG_END_LSX

.W_AVG_W32_LASX:
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvmulwev.w.h   xr4,    xr0,    xr21
    xvmulwod.w.h   xr5,    xr0,    xr21
    xvmulwev.w.h   xr6,    xr2,    xr21
    xvmulwod.w.h   xr7,    xr2,    xr21
    xvmaddwev.w.h  xr4,    xr1,    xr22
    xvmaddwod.w.h  xr5,    xr1,    xr22
    xvmaddwev.w.h  xr6,    xr3,    xr22
    xvmaddwod.w.h  xr7,    xr3,    xr22
    xvssrarni.hu.w xr6,    xr4,    bpcw_sh
    xvssrarni.hu.w xr7,    xr5,    bpcw_sh
    xvssrlni.bu.h  xr7,    xr6,    0
    xvshuf4i.w     xr8,    xr7,    0x4E
    xvilvl.b       xr9,    xr8,    xr7
    xvpermi.d      xr0,    xr9,    0xD8
    xvst           xr0,    a0,     0
    addi.w         a5,     a5,     -1
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    add.d          a0,     a0,     a1
    blt            zero,   a5,     .W_AVG_W32_LASX
    b              .W_AVG_END_LASX

.W_AVG_W64_LASX:
.rept 2
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvmulwev.w.h   xr4,    xr0,    xr21
    xvmulwod.w.h   xr5,    xr0,    xr21
    xvmulwev.w.h   xr6,    xr2,    xr21
    xvmulwod.w.h   xr7,    xr2,    xr21
    xvmaddwev.w.h  xr4,    xr1,    xr22
    xvmaddwod.w.h  xr5,    xr1,    xr22
    xvmaddwev.w.h  xr6,    xr3,    xr22
    xvmaddwod.w.h  xr7,    xr3,    xr22
    xvssrarni.hu.w xr6,    xr4,    bpcw_sh
    xvssrarni.hu.w xr7,    xr5,    bpcw_sh
    xvssrlni.bu.h  xr7,    xr6,    0
    xvshuf4i.w     xr8,    xr7,    0x4E
    xvilvl.b       xr9,    xr8,    xr7
    xvpermi.d      xr0,    xr9,    0xD8
    xvst           xr0,    a0,     0
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    addi.d         a0,     a0,     32
.endr
    addi.w         a5,     a5,     -1
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    blt            zero,   a5,     .W_AVG_W64_LASX
    b              .W_AVG_END_LASX

.W_AVG_W128_LASX:
.rept 4
    xvld           xr0,    a2,     0
    xvld           xr2,    a2,     32
    xvld           xr1,    a3,     0
    xvld           xr3,    a3,     32
    xvmulwev.w.h   xr4,    xr0,    xr21
    xvmulwod.w.h   xr5,    xr0,    xr21
    xvmulwev.w.h   xr6,    xr2,    xr21
    xvmulwod.w.h   xr7,    xr2,    xr21
    xvmaddwev.w.h  xr4,    xr1,    xr22
    xvmaddwod.w.h  xr5,    xr1,    xr22
    xvmaddwev.w.h  xr6,    xr3,    xr22
    xvmaddwod.w.h  xr7,    xr3,    xr22
    xvssrarni.hu.w xr6,    xr4,    bpcw_sh
    xvssrarni.hu.w xr7,    xr5,    bpcw_sh
    xvssrlni.bu.h  xr7,    xr6,    0
    xvshuf4i.w     xr8,    xr7,    0x4E
    xvilvl.b       xr9,    xr8,    xr7
    xvpermi.d      xr0,    xr9,    0xD8
    xvst           xr0,    a0,     0
    addi.d         a2,     a2,     64
    addi.d         a3,     a3,     64
    addi.d         a0,     a0,     32
.endr

    addi.w         a5,     a5,     -1
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    blt            zero,   a5,     .W_AVG_W128_LASX
.W_AVG_END_LASX:
endfunc

#undef bpc_sh
#undef bpcw_sh

#define mask_sh         10
/*
static void mask_c(pixel *dst, const ptrdiff_t dst_stride,
                   const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                   const uint8_t *mask HIGHBD_DECL_SUFFIX)
*/
function mask_8bpc_lsx
    vldi          vr21,   0x440   // 64
    vxor.v        vr19,   vr19,   vr19
    addi.d        t8,     a0,     0
    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .MASK_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.MASK_LSX_JRTABLE:
    .hword .MASK_W128_LSX - .MASK_LSX_JRTABLE
    .hword .MASK_W64_LSX  - .MASK_LSX_JRTABLE
    .hword .MASK_W32_LSX  - .MASK_LSX_JRTABLE
    .hword .MASK_W16_LSX  - .MASK_LSX_JRTABLE
    .hword .MASK_W8_LSX   - .MASK_LSX_JRTABLE
    .hword .MASK_W4_LSX   - .MASK_LSX_JRTABLE

.MASK_W4_LSX:
    vld           vr0,     a2,     0
    vld           vr1,     a3,     0
    fld.d         f22,     a6,     0

    vilvl.b       vr2,    vr19,   vr22
    vsub.h        vr3,    vr21,   vr2

    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vssrarni.hu.w vr5,    vr4,    mask_sh
    vssrlrni.bu.h vr1,    vr5,    0
    vpickod.w     vr4,    vr2,    vr1
    vilvl.b       vr0,    vr4,    vr1
    fst.s         f0,     a0,     0
    add.d         a0,     a0,     a1
    vstelm.w      vr0,    a0,     0,    1
    addi.d        a2,     a2,     16
    addi.d        a3,     a3,     16
    addi.d        a6,     a6,     8
    add.d         a0,     a0,     a1
    addi.w        a5,     a5,     -2
    blt           zero,   a5,     .MASK_W4_LSX
    b             .MASK_END_LSX
.MASK_W8_LSX:
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0

    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12

    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    fst.d         f0,     a0,     0
    add.d         a0,     a0,     a1
    vstelm.d      vr0,    a0,     0,   1
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    add.d         a0,     a0,     a1
    addi.w        a5,     a5,     -2
    blt           zero,   a5,     .MASK_W8_LSX
    b             .MASK_END_LSX

.MASK_W16_LSX:
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0

    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12

    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    add.d         a0,     a0,     a1
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W16_LSX
    b             .MASK_END_LSX
.MASK_W32_LSX:
.rept 2
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0
    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12
    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    addi.d        a0,     a0,     16
.endr
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W32_LSX
    b             .MASK_END_LSX
.MASK_W64_LSX:
.rept 4
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0
    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12
    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    addi.d        a0,     a0,     16
.endr
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W64_LSX
    b             .MASK_END_LSX
.MASK_W128_LSX:
.rept 8
    vld           vr0,    a2,     0
    vld           vr10,   a2,     16
    vld           vr1,    a3,     0
    vld           vr11,   a3,     16
    vld           vr22,   a6,     0
    vilvl.b       vr2,    vr19,   vr22
    vilvh.b       vr12,   vr19,   vr22
    vsub.h        vr3,    vr21,   vr2
    vsub.h        vr13,   vr21,   vr12
    vmulwev.w.h   vr4,    vr0,    vr2
    vmulwod.w.h   vr5,    vr0,    vr2
    vmulwev.w.h   vr14,   vr10,   vr12
    vmulwod.w.h   vr15,   vr10,   vr12
    vmaddwev.w.h  vr4,    vr1,    vr3
    vmaddwod.w.h  vr5,    vr1,    vr3
    vmaddwev.w.h  vr14,   vr11,   vr13
    vmaddwod.w.h  vr15,   vr11,   vr13
    vssrarni.hu.w vr14,   vr4,    mask_sh
    vssrarni.hu.w vr15,   vr5,    mask_sh
    vssrlrni.bu.h vr15,   vr14,   0
    vshuf4i.w     vr6,    vr15,   0x4E
    vilvl.b       vr0,    vr6,    vr15
    vst           vr0,    a0,     0
    addi.d        a2,     a2,     32
    addi.d        a3,     a3,     32
    addi.d        a6,     a6,     16
    addi.d        a0,     a0,     16
.endr
    add.d         t8,     t8,     a1
    add.d         a0,     t8,     zero
    addi.w        a5,     a5,     -1
    blt           zero,   a5,     .MASK_W128_LSX
.MASK_END_LSX:
endfunc

function mask_8bpc_lasx
    xvldi         xr21,   0x440   // 64
    xvxor.v       xr19,   xr19,   xr19
    addi.d        t8,     a0,     0
    clz.w         t0,     a4
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .MASK_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0
    add.d         t1,     t1,      t2
    jirl          $r0,    t1,      0

    .align   3
.MASK_LASX_JRTABLE:
    .hword .MASK_W128_LASX - .MASK_LASX_JRTABLE
    .hword .MASK_W64_LASX  - .MASK_LASX_JRTABLE
    .hword .MASK_W32_LASX  - .MASK_LASX_JRTABLE
    .hword .MASK_W16_LASX  - .MASK_LASX_JRTABLE
    .hword .MASK_W8_LASX   - .MASK_LASX_JRTABLE
    .hword .MASK_W4_LASX   - .MASK_LASX_JRTABLE

.MASK_W4_LASX:
    vld            vr0,    a2,     0
    vld            vr1,    a3,     0
    fld.d          f22,    a6,     0

    vilvl.h        vr4,    vr1,    vr0
    vilvh.h        vr14,   vr1,    vr0
    vilvl.b        vr2,    vr19,   vr22
    vsub.h         vr3,    vr21,   vr2
    xvpermi.q      xr14,   xr4,    0x20
    vilvl.h        vr5,    vr3,    vr2
    vilvh.h        vr15,   vr3,    vr2
    xvpermi.q      xr15,   xr5,    0x20
    xvmulwev.w.h   xr0,    xr14,   xr15
    xvmaddwod.w.h  xr0,    xr14,   xr15
    xvssrarni.hu.w xr1,    xr0,    mask_sh
    xvssrlni.bu.h  xr2,    xr1,    0
    fst.s          f2,     a0,     0
    add.d          a0,     a0,     a1
    xvstelm.w      xr2,    a0,     0,    4

    addi.d         a2,     a2,     16
    addi.d         a3,     a3,     16
    addi.d         a6,     a6,     8
    add.d          a0,     a0,     a1
    addi.w         a5,     a5,     -2
    blt            zero,   a5,     .MASK_W4_LASX
    b              .MASK_END_LASX

.MASK_W8_LASX:
    xvld           xr0,    a2,      0
    xvld           xr1,    a3,      0
    vld            vr22,   a6,      0

    vext2xv.hu.bu  xr2,    xr22
    xvsub.h        xr3,    xr21,    xr2
    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvssrarni.hu.w xr5,    xr4,     mask_sh
    xvssrlni.bu.h  xr1,    xr5,     0
    xvpickod.w     xr4,    xr2,     xr1
    xvilvl.b       xr0,    xr4,     xr1
    fst.d          f0,     a0,      0
    add.d          a0,     a0,      a1
    xvstelm.d      xr0,    a0,      0,    2

    addi.d         a2,     a2,      32
    addi.d         a3,     a3,      32
    addi.d         a6,     a6,      16
    add.d          a0,     a0,      a1
    addi.w         a5,     a5,      -2
    blt            zero,   a5,      .MASK_W8_LASX
    b              .MASK_END_LASX

.MASK_W16_LASX:
    xvld           xr0,    a2,      0
    xvld           xr1,    a3,      0
    vld            vr22,   a6,      0

    vext2xv.hu.bu  xr2,    xr22
    xvsub.h        xr3,    xr21,    xr2
    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvssrarni.hu.w xr5,    xr4,     mask_sh
    xvssrlni.bu.h  xr1,    xr5,     0
    xvpickod.w     xr4,    xr2,    xr1
    xvilvl.b       xr0,    xr4,    xr1
    xvpermi.d      xr1,    xr0,     0xD8
    vst            vr1,    a0,      0

    addi.d         a2,     a2,      32
    addi.d         a3,     a3,      32
    addi.d         a6,     a6,      16
    add.d          a0,     a0,      a1
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W16_LASX
    b              .MASK_END_LASX
.MASK_W32_LASX:
    xvld           xr0,    a2,      0
    xvld           xr10,   a2,      32
    xvld           xr1,    a3,      0
    xvld           xr11,   a3,      32
    xvld           xr22,   a6,      0
    vext2xv.hu.bu  xr2,    xr22
    xvpermi.q      xr4,    xr22,    0x01
    vext2xv.hu.bu  xr12,   xr4
    xvsub.h        xr3,    xr21,    xr2
    xvsub.h        xr13,   xr21,    xr12

    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmulwev.w.h   xr14,   xr10,    xr12
    xvmulwod.w.h   xr15,   xr10,    xr12
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvmaddwev.w.h  xr14,   xr11,    xr13
    xvmaddwod.w.h  xr15,   xr11,    xr13
    xvssrarni.hu.w xr14,   xr4,     mask_sh
    xvssrarni.hu.w xr15,   xr5,     mask_sh
    xvssrlni.bu.h  xr15,   xr14,    0
    xvshuf4i.w     xr6,    xr15,    0x4E
    xvilvl.b       xr1,    xr6,     xr15
    xvpermi.d      xr0,    xr1,     0xD8
    xvst           xr0,    a0,      0

    addi.d         a2,     a2,      64
    addi.d         a3,     a3,      64
    addi.d         a6,     a6,      32
    add.d          a0,     a0,      a1
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W32_LASX
    b              .MASK_END_LASX

.MASK_W64_LASX:
.rept 2
    xvld           xr0,    a2,      0
    xvld           xr10,   a2,      32
    xvld           xr1,    a3,      0
    xvld           xr11,   a3,      32
    xvld           xr22,   a6,      0
    vext2xv.hu.bu  xr2,    xr22
    xvpermi.q      xr4,    xr22,    0x01
    vext2xv.hu.bu  xr12,   xr4
    xvsub.h        xr3,    xr21,    xr2
    xvsub.h        xr13,   xr21,    xr12

    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmulwev.w.h   xr14,   xr10,    xr12
    xvmulwod.w.h   xr15,   xr10,    xr12
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvmaddwev.w.h  xr14,   xr11,    xr13
    xvmaddwod.w.h  xr15,   xr11,    xr13
    xvssrarni.hu.w xr14,   xr4,     mask_sh
    xvssrarni.hu.w xr15,   xr5,     mask_sh
    xvssrlni.bu.h  xr15,   xr14,    0
    xvshuf4i.w     xr6,    xr15,    0x4E
    xvilvl.b       xr1,    xr6,     xr15
    xvpermi.d      xr0,    xr1,     0xD8
    xvst           xr0,    a0,      0
    addi.d         a2,     a2,      64
    addi.d         a3,     a3,      64
    addi.d         a6,     a6,      32
    addi.d         a0,     a0,      32
.endr
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W64_LASX
    b              .MASK_END_LASX

.MASK_W128_LASX:
.rept 4
    xvld           xr0,    a2,      0
    xvld           xr10,   a2,      32
    xvld           xr1,    a3,      0
    xvld           xr11,   a3,      32
    xvld           xr22,   a6,      0
    vext2xv.hu.bu  xr2,    xr22
    xvpermi.q      xr4,    xr22,    0x01
    vext2xv.hu.bu  xr12,   xr4
    xvsub.h        xr3,    xr21,    xr2
    xvsub.h        xr13,   xr21,    xr12

    xvmulwev.w.h   xr4,    xr0,     xr2
    xvmulwod.w.h   xr5,    xr0,     xr2
    xvmulwev.w.h   xr14,   xr10,    xr12
    xvmulwod.w.h   xr15,   xr10,    xr12
    xvmaddwev.w.h  xr4,    xr1,     xr3
    xvmaddwod.w.h  xr5,    xr1,     xr3
    xvmaddwev.w.h  xr14,   xr11,    xr13
    xvmaddwod.w.h  xr15,   xr11,    xr13
    xvssrarni.hu.w xr14,   xr4,     mask_sh
    xvssrarni.hu.w xr15,   xr5,     mask_sh
    xvssrlni.bu.h  xr15,   xr14,    0
    xvshuf4i.w     xr6,    xr15,    0x4E
    xvilvl.b       xr1,    xr6,     xr15
    xvpermi.d      xr0,    xr1,     0xD8
    xvst           xr0,    a0,      0

    addi.d         a2,     a2,      64
    addi.d         a3,     a3,      64
    addi.d         a6,     a6,      32
    addi.d         a0,     a0,      32
.endr
    add.d          t8,     t8,     a1
    add.d          a0,     t8,     zero
    addi.w         a5,     a5,      -1
    blt            zero,   a5,      .MASK_W128_LASX
.MASK_END_LASX:
endfunc

/*
static void w_mask_c(pixel *dst, const ptrdiff_t dst_stride,
                     const int16_t *tmp1, const int16_t *tmp2, const int w, int h,
                     uint8_t *mask, const int sign,
                     const int ss_hor, const int ss_ver HIGHBD_DECL_SUFFIX)
*/
function w_mask_420_8bpc_lsx
    addi.d        sp,      sp,    -24
    fst.d         f24,     sp,    0
    fst.d         f25,     sp,    8
    fst.d         f26,     sp,    16
    vldi          vr20,    0x440
    vreplgr2vr.h  vr21,    a7
    vldi          vr22,    0x426

    clz.w         t0,      a4
    li.w          t1,      24
    sub.w         t0,      t0,      t1
    la.local      t1,      .WMASK420_LSX_JRTABLE
    alsl.d        t0,      t0,      t1,    1
    ld.h          t8,      t0,      0
    add.d         t1,      t1,      t8
    jirl          $r0,     t1,      0

    .align   3
.WMASK420_LSX_JRTABLE:
    .hword .WMASK420_W128_LSX - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W64_LSX  - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W32_LSX  - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W16_LSX  - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W8_LSX   - .WMASK420_LSX_JRTABLE
    .hword .WMASK420_W4_LSX   - .WMASK420_LSX_JRTABLE

.WMASK420_W4_LSX:
    vld           vr0,     a2,       0
    vld           vr1,     a2,       16
    vld           vr2,     a3,       0
    vld           vr3,     a3,       16
    addi.w        a5,      a5,       -4

    vabsd.h       vr4,     vr0,      vr2
    vabsd.h       vr5,     vr1,      vr3
    vaddi.hu      vr4,     vr4,      8
    vaddi.hu      vr5,     vr5,      8
    vsrli.h       vr4,     vr4,      8
    vsrli.h       vr5,     vr5,      8
    vadd.h        vr4,     vr4,      vr22
    vadd.h        vr5,     vr5,      vr22
    vmin.hu       vr6,     vr4,      vr20
    vmin.hu       vr7,     vr5,      vr20
    vsub.h        vr8,     vr20,     vr6
    vsub.h        vr9,     vr20,     vr7
    vmulwev.w.h   vr4,     vr6,      vr0
    vmulwod.w.h   vr5,     vr6,      vr0
    vmulwev.w.h   vr10,    vr7,      vr1
    vmulwod.w.h   vr11,    vr7,      vr1
    vmaddwev.w.h  vr4,     vr8,      vr2
    vmaddwod.w.h  vr5,     vr8,      vr2
    vmaddwev.w.h  vr10,    vr9,      vr3
    vmaddwod.w.h  vr11,    vr9,      vr3
    vilvl.w       vr0,     vr5,      vr4
    vilvh.w       vr1,     vr5,      vr4
    vilvl.w       vr2,     vr11,     vr10
    vilvh.w       vr3,     vr11,     vr10
    vssrarni.hu.w vr1,     vr0,      10
    vssrarni.hu.w vr3,     vr2,      10
    vssrlni.bu.h  vr3,     vr1,      0
    vstelm.w      vr3,     a0,       0,    0
    add.d         a0,      a0,       a1
    vstelm.w      vr3,     a0,       0,    1
    add.d         a0,      a0,       a1
    vstelm.w      vr3,     a0,       0,    2
    add.d         a0,      a0,       a1
    vstelm.w      vr3,     a0,       0,    3
    add.d         a0,      a0,       a1
    vpickev.h     vr0,     vr7,      vr6
    vpickod.h     vr1,     vr7,      vr6
    vadd.h        vr0,     vr0,      vr1
    vshuf4i.h     vr0,     vr0,      0xd8
    vhaddw.w.h    vr2,     vr0,      vr0
    vpickev.h     vr2,     vr2,      vr2
    vsub.h        vr2,     vr2,      vr21
    vaddi.hu      vr2,     vr2,      2
    vssrani.bu.h  vr2,     vr2,      2
    vstelm.w      vr2,     a6,       0,    0

    addi.d        a2,      a2,       32
    addi.d        a3,      a3,       32
    addi.d        a6,      a6,       4
    blt           zero,    a5,       .WMASK420_W4_LSX
    b             .END_W420

.WMASK420_W8_LSX:
    vld           vr0,     a2,       0
    vld           vr1,     a2,       16
    vld           vr2,     a3,       0
    vld           vr3,     a3,       16
    addi.w        a5,      a5,       -2

    vabsd.h       vr4,     vr0,      vr2
    vabsd.h       vr5,     vr1,      vr3
    vaddi.hu      vr4,     vr4,      8
    vaddi.hu      vr5,     vr5,      8
    vsrli.h       vr4,     vr4,      8
    vsrli.h       vr5,     vr5,      8
    vadd.h        vr4,     vr4,      vr22
    vadd.h        vr5,     vr5,      vr22
    vmin.hu       vr6,     vr4,      vr20
    vmin.hu       vr7,     vr5,      vr20
    vsub.h        vr8,     vr20,     vr6
    vsub.h        vr9,     vr20,     vr7
    vmulwev.w.h   vr4,     vr6,      vr0
    vmulwod.w.h   vr5,     vr6,      vr0
    vmulwev.w.h   vr10,    vr7,      vr1
    vmulwod.w.h   vr11,    vr7,      vr1
    vmaddwev.w.h  vr4,     vr8,      vr2
    vmaddwod.w.h  vr5,     vr8,      vr2
    vmaddwev.w.h  vr10,    vr9,      vr3
    vmaddwod.w.h  vr11,    vr9,      vr3
    vssrarni.hu.w vr10,    vr4,      10
    vssrarni.hu.w vr11,    vr5,      10
    vssrlni.bu.h  vr11,    vr10,     0
    vshuf4i.w     vr0,     vr11,     0x4E
    vilvl.b       vr3,     vr0,      vr11
    vstelm.d      vr3,     a0,       0,     0
    add.d         a0,      a0,       a1
    vstelm.d      vr3,     a0,       0,     1
    add.d         a0,      a0,       a1
    vpickev.h     vr0,     vr7,      vr6
    vpickod.h     vr1,     vr7,      vr6
    vadd.h        vr0,     vr0,      vr1
    vilvh.d       vr2,     vr0,      vr0
    vadd.h        vr2,     vr2,      vr0
    vsub.h        vr2,     vr2,      vr21
    vaddi.hu      vr2,     vr2,      2
    vssrani.bu.h  vr2,     vr2,      2
    vstelm.w      vr2,     a6,       0,     0

    addi.d        a2,      a2,       32
    addi.d        a3,      a3,       32
    addi.d        a6,      a6,       4
    blt           zero,    a5,       .WMASK420_W8_LSX
    b             .END_W420

.WMASK420_W16_LSX:
    vld           vr0,     a2,       0
    vld           vr1,     a2,       16
    alsl.d        a2,      a4,       a2,    1
    vld           vr2,     a2,       0
    vld           vr3,     a2,       16
    vld           vr4,     a3,       0
    vld           vr5,     a3,       16
    alsl.d        a3,      a4,       a3,    1
    vld           vr6,     a3,       0
    vld           vr7,     a3,       16

    vabsd.h       vr8,     vr0,      vr4
    vabsd.h       vr9,     vr1,      vr5
    vabsd.h       vr10,    vr2,      vr6
    vabsd.h       vr11,    vr3,      vr7
    vaddi.hu      vr8,     vr8,      8
    vaddi.hu      vr9,     vr9,      8
    vaddi.hu      vr10,    vr10,     8
    vaddi.hu      vr11,    vr11,     8
    vsrli.h       vr8,     vr8,      8
    vsrli.h       vr9,     vr9,      8
    vsrli.h       vr10,    vr10,     8
    vsrli.h       vr11,    vr11,     8
    vadd.h        vr8,     vr8,      vr22
    vadd.h        vr9,     vr9,      vr22
    vadd.h        vr10,    vr10,     vr22
    vadd.h        vr11,    vr11,     vr22
    vmin.hu       vr12,    vr8,      vr20
    vmin.hu       vr13,    vr9,      vr20
    vmin.hu       vr14,    vr10,     vr20
    vmin.hu       vr15,    vr11,     vr20
    vsub.h        vr16,    vr20,     vr12
    vsub.h        vr17,    vr20,     vr13
    vsub.h        vr18,    vr20,     vr14
    vsub.h        vr19,    vr20,     vr15
    vmulwev.w.h   vr8,     vr12,     vr0
    vmulwod.w.h   vr9,     vr12,     vr0
    vmulwev.w.h   vr10,    vr13,     vr1
    vmulwod.w.h   vr11,    vr13,     vr1
    vmulwev.w.h   vr23,    vr14,     vr2
    vmulwod.w.h   vr24,    vr14,     vr2
    vmulwev.w.h   vr25,    vr15,     vr3
    vmulwod.w.h   vr26,    vr15,     vr3
    vmaddwev.w.h  vr8,     vr16,     vr4
    vmaddwod.w.h  vr9,     vr16,     vr4
    vmaddwev.w.h  vr10,    vr17,     vr5
    vmaddwod.w.h  vr11,    vr17,     vr5
    vmaddwev.w.h  vr23,    vr18,     vr6
    vmaddwod.w.h  vr24,    vr18,     vr6
    vmaddwev.w.h  vr25,    vr19,     vr7
    vmaddwod.w.h  vr26,    vr19,     vr7
    vssrarni.hu.w vr10,    vr8,      10
    vssrarni.hu.w vr11,    vr9,      10
    vssrarni.hu.w vr25,    vr23,     10
    vssrarni.hu.w vr26,    vr24,     10
    vssrlni.bu.h  vr11,    vr10,     0
    vssrlni.bu.h  vr26,    vr25,     0
    vshuf4i.w     vr0,     vr11,     0x4E
    vshuf4i.w     vr1,     vr26,     0x4E
    vilvl.b       vr3,     vr0,      vr11
    vilvl.b       vr7,     vr1,      vr26
    vst           vr3,     a0,       0
    vstx          vr7,     a0,       a1
    vpickev.h     vr0,     vr13,     vr12
    vpickod.h     vr1,     vr13,     vr12
    vpickev.h     vr2,     vr15,     vr14
    vpickod.h     vr3,     vr15,     vr14
    vadd.h        vr4,     vr0,      vr1
    vadd.h        vr5,     vr2,      vr3
    vadd.h        vr4,     vr4,      vr5
    vsub.h        vr4,     vr4,      vr21
    vssrarni.bu.h vr4,     vr4,      2
    vstelm.d      vr4,     a6,       0,    0

    alsl.d        a2,      a4,       a2,   1
    alsl.d        a3,      a4,       a3,   1
    alsl.d        a0,      a1,       a0,   1
    addi.d        a6,      a6,       8
    addi.w        a5,      a5,       -2
    blt           zero,    a5,       .WMASK420_W16_LSX
    b    .END_W420

.WMASK420_W32_LSX:
.WMASK420_W64_LSX:
.WMASK420_W128_LSX:

.LOOP_W32_420_LSX:
    add.d         t1,       a2,       zero
    add.d         t2,       a3,       zero
    add.d         t3,       a0,       zero
    add.d         t4,       a6,       zero
    alsl.d        t5,       a4,       t1,     1
    alsl.d        t6,       a4,       t2,     1
    or            t7,       a4,       a4

.W32_420_LSX:
    vld           vr0,      t1,       0
    vld           vr1,      t1,       16
    vld           vr2,      t2,       0
    vld           vr3,      t2,       16
    vld           vr4,      t5,       0
    vld           vr5,      t5,       16
    vld           vr6,      t6,       0
    vld           vr7,      t6,       16
    addi.d        t1,       t1,       32
    addi.d        t2,       t2,       32
    addi.d        t5,       t5,       32
    addi.d        t6,       t6,       32
    addi.w        t7,       t7,       -16
    vabsd.h       vr8,      vr0,      vr2
    vabsd.h       vr9,      vr1,      vr3
    vabsd.h       vr10,     vr4,      vr6
    vabsd.h       vr11,     vr5,      vr7
    vaddi.hu      vr8,      vr8,      8
    vaddi.hu      vr9,      vr9,      8
    vaddi.hu      vr10,     vr10,     8
    vaddi.hu      vr11,     vr11,     8
    vsrli.h       vr8,      vr8,      8
    vsrli.h       vr9,      vr9,      8
    vsrli.h       vr10,     vr10,     8
    vsrli.h       vr11,     vr11,     8
    vadd.h        vr8,      vr8,      vr22
    vadd.h        vr9,      vr9,      vr22
    vadd.h        vr10,     vr10,     vr22
    vadd.h        vr11,     vr11,     vr22
    vmin.hu       vr12,     vr8,      vr20
    vmin.hu       vr13,     vr9,      vr20
    vmin.hu       vr14,     vr10,     vr20
    vmin.hu       vr15,     vr11,     vr20
    vsub.h        vr16,     vr20,     vr12
    vsub.h        vr17,     vr20,     vr13
    vsub.h        vr18,     vr20,     vr14
    vsub.h        vr19,     vr20,     vr15
    vmulwev.w.h   vr8,      vr12,     vr0
    vmulwod.w.h   vr9,      vr12,     vr0
    vmulwev.w.h   vr10,     vr13,     vr1
    vmulwod.w.h   vr11,     vr13,     vr1
    vmulwev.w.h   vr23,     vr14,     vr4
    vmulwod.w.h   vr24,     vr14,     vr4
    vmulwev.w.h   vr25,     vr15,     vr5
    vmulwod.w.h   vr26,     vr15,     vr5
    vmaddwev.w.h  vr8,      vr16,     vr2
    vmaddwod.w.h  vr9,      vr16,     vr2
    vmaddwev.w.h  vr10,     vr17,     vr3
    vmaddwod.w.h  vr11,     vr17,     vr3
    vmaddwev.w.h  vr23,     vr18,     vr6
    vmaddwod.w.h  vr24,     vr18,     vr6
    vmaddwev.w.h  vr25,     vr19,     vr7
    vmaddwod.w.h  vr26,     vr19,     vr7
    vssrarni.hu.w vr10,     vr8,      10
    vssrarni.hu.w vr11,     vr9,      10
    vssrarni.hu.w vr25,     vr23,     10
    vssrarni.hu.w vr26,     vr24,     10
    vssrlni.bu.h  vr11,     vr10,     0
    vssrlni.bu.h  vr26,     vr25,     0
    vshuf4i.w     vr8,      vr11,     0x4E
    vshuf4i.w     vr9,      vr26,     0x4E
    vilvl.b       vr3,      vr8,      vr11
    vilvl.b       vr7,      vr9,      vr26
    vst           vr3,      t3,       0
    vstx          vr7,      a1,       t3
    addi.d        t3,       t3,       16
    vpickev.h     vr8,      vr13,     vr12
    vpickod.h     vr9,      vr13,     vr12
    vpickev.h     vr10,     vr15,     vr14
    vpickod.h     vr11,     vr15,     vr14
    vadd.h        vr8,      vr8,      vr9
    vadd.h        vr10,     vr10,     vr11
    vadd.h        vr12,     vr8,      vr10
    vsub.h        vr12,     vr12,     vr21
    vssrarni.bu.h vr12,     vr12,     2
    vstelm.d      vr12,     t4,       0,     0
    addi.d        t4,       t4,       8
    bne           t7,       zero,     .W32_420_LSX

    alsl.d        a2,       a4,       a2,     2
    alsl.d        a3,       a4,       a3,     2
    alsl.d        a0,       a1,       a0,     1
    srai.w        t8,       a4,       1
    add.d         a6,       a6,       t8
    addi.w        a5,       a5,       -2
    blt           zero,     a5,       .LOOP_W32_420_LSX

.END_W420:
    fld.d            f24,     sp,    0
    fld.d            f25,     sp,    8
    fld.d            f26,     sp,    16
    addi.d           sp,      sp,    24
endfunc

function w_mask_420_8bpc_lasx
    xvldi          xr20,    0x440
    xvreplgr2vr.h  xr21,    a7
    xvldi          xr22,    0x426

    clz.w          t0,      a4
    li.w           t1,      24
    sub.w          t0,      t0,      t1
    la.local       t1,      .WMASK420_LASX_JRTABLE
    alsl.d         t0,      t0,      t1,    1
    ld.h           t8,      t0,      0
    add.d          t1,      t1,      t8
    jirl           $r0,     t1,      0

    .align   3
.WMASK420_LASX_JRTABLE:
    .hword .WMASK420_W128_LASX - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W64_LASX  - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W32_LASX  - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W16_LASX  - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W8_LASX   - .WMASK420_LASX_JRTABLE
    .hword .WMASK420_W4_LASX   - .WMASK420_LASX_JRTABLE

.WMASK420_W4_LASX:
    xvld           xr0,     a2,     0
    xvld           xr1,     a3,     0
    addi.w         a5,      a5,     -4

    xvabsd.h       xr2,     xr0,    xr1
    xvaddi.hu      xr2,     xr2,    8
    xvsrli.h       xr2,     xr2,    8
    xvadd.h        xr2,     xr2,    xr22
    xvmin.hu       xr3,     xr2,    xr20
    xvsub.h        xr4,     xr20,   xr3
    xvmulwev.w.h   xr5,     xr3,    xr0
    xvmulwod.w.h   xr6,     xr3,    xr0
    xvmaddwev.w.h  xr5,     xr4,    xr1
    xvmaddwod.w.h  xr6,     xr4,    xr1
    xvilvl.w       xr7,     xr6,    xr5
    xvilvh.w       xr8,     xr6,    xr5
    xvssrarni.hu.w xr8,     xr7,    10
    xvssrlni.bu.h  xr9,     xr8,    0
    vstelm.w       vr9,     a0,     0,     0
    add.d          a0,      a0,     a1
    vstelm.w       vr9,     a0,     0,     1
    add.d          a0,      a0,     a1
    xvstelm.w      xr9,     a0,     0,     4
    add.d          a0,      a0,     a1
    xvstelm.w      xr9,     a0,     0,     5
    add.d          a0,      a0,     a1

    xvhaddw.w.h    xr3,     xr3,    xr3
    xvpermi.d      xr4,     xr3,    0xb1
    xvadd.h        xr3,     xr3,    xr4
    xvpickev.h     xr3,     xr3,    xr3
    xvsub.h        xr3,     xr3,    xr21
    xvssrarni.bu.h xr3,     xr3,    2
    vstelm.h       vr3,     a6,     0,     0
    xvstelm.h      xr3,     a6,     2,     8

    addi.d         a2,     a2,      32
    addi.d         a3,     a3,      32
    addi.d         a6,     a6,      4
    blt            zero,   a5,      .WMASK420_W4_LASX
    b              .END_W420_LASX

.WMASK420_W8_LASX:
    xvld           xr0,      a2,     0
    xvld           xr1,      a2,     32
    xvld           xr2,      a3,     0
    xvld           xr3,      a3,     32
    addi.w         a5,       a5,     -4

    xvabsd.h       xr4,      xr0,    xr2
    xvabsd.h       xr5,      xr1,    xr3
    xvaddi.hu      xr4,      xr4,    8
    xvaddi.hu      xr5,      xr5,    8
    xvsrli.h       xr4,      xr4,    8
    xvsrli.h       xr5,      xr5,    8
    xvadd.h        xr4,      xr4,    xr22
    xvadd.h        xr5,      xr5,    xr22
    xvmin.hu       xr6,      xr4,    xr20
    xvmin.hu       xr7,      xr5,    xr20
    xvsub.h        xr8,      xr20,   xr6
    xvsub.h        xr9,      xr20,   xr7
    xvmulwev.w.h   xr10,     xr6,    xr0
    xvmulwod.w.h   xr11,     xr6,    xr0
    xvmulwev.w.h   xr12,     xr7,    xr1
    xvmulwod.w.h   xr13,     xr7,    xr1
    xvmaddwev.w.h  xr10,     xr8,    xr2
    xvmaddwod.w.h  xr11,     xr8,    xr2
    xvmaddwev.w.h  xr12,     xr9,    xr3
    xvmaddwod.w.h  xr13,     xr9,    xr3
    xvssrarni.hu.w xr12,     xr10,   10
    xvssrarni.hu.w xr13,     xr11,   10
    xvssrlni.bu.h  xr13,     xr12,   0
    xvshuf4i.w     xr1,      xr13,   0x4E
    xvilvl.b       xr17,     xr1,    xr13
    vstelm.d       vr17,     a0,     0,     0
    add.d          a0,       a0,     a1
    xvstelm.d      xr17,     a0,     0,     2
    add.d          a0,       a0,     a1
    xvstelm.d      xr17,     a0,     0,     1
    add.d          a0,       a0,     a1
    xvstelm.d      xr17,     a0,     0,     3
    add.d          a0,       a0,     a1

    xvhaddw.w.h    xr6,      xr6,    xr6
    xvhaddw.w.h    xr7,      xr7,    xr7
    xvpickev.h     xr8,      xr7,    xr6
    xvpermi.q      xr9,      xr8,    0x01
    vadd.h         vr8,      vr8,    vr9
    vsub.h         vr8,      vr8,    vr21
    vssrarni.bu.h  vr8,      vr8,    2
    vstelm.d       vr8,      a6,     0,    0
    addi.d         a2,       a2,     64
    addi.d         a3,       a3,     64
    addi.d         a6,       a6,     8
    blt            zero,     a5,     .WMASK420_W8_LASX
    b              .END_W420_LASX

.WMASK420_W16_LASX:
    xvld           xr0,      a2,     0
    xvld           xr1,      a2,     32
    xvld           xr2,      a3,     0
    xvld           xr3,      a3,     32
    addi.w         a5,       a5,     -2

    xvabsd.h       xr4,      xr0,    xr2
    xvabsd.h       xr5,      xr1,    xr3
    xvaddi.hu      xr4,      xr4,    8
    xvaddi.hu      xr5,      xr5,    8
    xvsrli.h       xr4,      xr4,    8
    xvsrli.h       xr5,      xr5,    8
    xvadd.h        xr4,      xr4,    xr22
    xvadd.h        xr5,      xr5,    xr22
    xvmin.hu       xr4,      xr4,    xr20
    xvmin.hu       xr5,      xr5,    xr20
    xvsub.h        xr6,      xr20,   xr4
    xvsub.h        xr7,      xr20,   xr5
    xvmulwev.w.h   xr8,      xr4,    xr0
    xvmulwod.w.h   xr9,      xr4,    xr0
    xvmulwev.w.h   xr10,     xr5,    xr1
    xvmulwod.w.h   xr11,     xr5,    xr1
    xvmaddwev.w.h  xr8,      xr6,    xr2
    xvmaddwod.w.h  xr9,      xr6,    xr2
    xvmaddwev.w.h  xr10,     xr7,    xr3
    xvmaddwod.w.h  xr11,     xr7,    xr3
    xvssrarni.hu.w xr10,     xr8,    10
    xvssrarni.hu.w xr11,     xr9,    10
    xvssrlni.bu.h  xr11,     xr10,   0
    xvshuf4i.w     xr8,      xr11,   0x4E
    xvilvl.b       xr15,     xr8,    xr11
    xvpermi.d      xr16,     xr15,   0xd8
    vst            vr16,     a0,     0
    add.d          a0,       a0,     a1
    xvpermi.q      xr16,     xr16,   0x01
    vst            vr16,     a0,     0
    add.d          a0,       a0,     a1

    xvhaddw.w.h    xr4,      xr4,    xr4
    xvhaddw.w.h    xr5,      xr5,    xr5
    xvadd.h        xr4,      xr5,    xr4
    xvpickev.h     xr6,      xr4,    xr4
    xvpermi.d      xr7,      xr6,    0x08
    vsub.h         vr7,      vr7,    vr21
    vssrarni.bu.h  vr7,      vr7,    2
    vstelm.d       vr7,      a6,     0,    0

    addi.d         a2,       a2,     64
    addi.d         a3,       a3,     64
    addi.d         a6,       a6,     8
    blt            zero,     a5,     .WMASK420_W16_LASX
    b              .END_W420_LASX

.WMASK420_W32_LASX:
.WMASK420_W64_LASX:
.WMASK420_W128_LASX:

.LOOP_W32_420_LASX:
    add.d          t1,       a2,       zero
    add.d          t2,       a3,       zero
    add.d          t3,       a0,       zero
    add.d          t4,       a6,       zero
    alsl.d         t5,       a4,       t1,     1
    alsl.d         t6,       a4,       t2,     1
    or             t7,       a4,       a4
.W32_420_LASX:
    xvld           xr0,      t1,       0
    xvld           xr1,      t2,       0
    xvld           xr2,      t5,       0
    xvld           xr3,      t6,       0
    addi.d         t1,       t1,       32
    addi.d         t2,       t2,       32
    addi.d         t5,       t5,       32
    addi.d         t6,       t6,       32
    addi.w         t7,       t7,       -16
    xvabsd.h       xr4,      xr0,      xr1
    xvabsd.h       xr5,      xr2,      xr3
    xvaddi.hu      xr4,      xr4,      8
    xvaddi.hu      xr5,      xr5,      8
    xvsrli.h       xr4,      xr4,      8
    xvsrli.h       xr5,      xr5,      8
    xvadd.h        xr4,      xr4,      xr22
    xvadd.h        xr5,      xr5,      xr22
    xvmin.hu       xr6,      xr4,      xr20
    xvmin.hu       xr7,      xr5,      xr20
    xvsub.h        xr8,      xr20,     xr6
    xvsub.h        xr9,      xr20,     xr7
    xvmulwev.w.h   xr10,     xr6,      xr0
    xvmulwod.w.h   xr11,     xr6,      xr0
    xvmulwev.w.h   xr12,     xr7,      xr2
    xvmulwod.w.h   xr13,     xr7,      xr2
    xvmaddwev.w.h  xr10,     xr8,      xr1
    xvmaddwod.w.h  xr11,     xr8,      xr1
    xvmaddwev.w.h  xr12,     xr9,      xr3
    xvmaddwod.w.h  xr13,     xr9,      xr3
    xvssrarni.hu.w xr12,     xr10,     10
    xvssrarni.hu.w xr13,     xr11,     10
    xvssrlni.bu.h  xr13,     xr12,     0
    xvshuf4i.w     xr10,     xr13,     0x4E
    xvilvl.b       xr17,     xr10,     xr13
    xvpermi.d      xr18,     xr17,     0x08
    xvpermi.d      xr19,     xr17,     0x0d
    vst            vr18,     t3,       0
    vstx           vr19,     t3,       a1
    addi.d         t3,       t3,       16

    xvhaddw.w.h    xr6,      xr6,      xr6
    xvhaddw.w.h    xr7,      xr7,      xr7
    xvadd.h        xr6,      xr7,      xr6
    xvpickev.h     xr7,      xr6,      xr6
    xvpermi.d      xr8,      xr7,      0x08
    vsub.h         vr9,      vr8,      vr21
    vssrarni.bu.h  vr9,      vr9,      2
    vstelm.d       vr9,      t4,       0,      0
    addi.d         t4,       t4,       8
    bne            t7,       zero,     .W32_420_LASX

    alsl.d         a2,       a4,       a2,     2
    alsl.d         a3,       a4,       a3,     2
    alsl.d         a0,       a1,       a0,     1
    srai.w         t8,       a4,       1
    add.d          a6,       a6,       t8
    addi.w         a5,       a5,       -2
    blt            zero,     a5,       .LOOP_W32_420_LASX

.END_W420_LASX:
endfunc

#undef bpc_sh
#undef bpcw_sh

.macro  vhaddw.d.h  in0
    vhaddw.w.h  \in0,  \in0,  \in0
    vhaddw.d.w  \in0,  \in0,  \in0
.endm
.macro  vhaddw.q.w  in0
    vhaddw.d.w  \in0,  \in0,  \in0
    vhaddw.q.d  \in0,  \in0,  \in0
.endm
.macro PUT_H_8W in0
    vshuf.b          vr2,    \in0,  \in0,   vr6
    vshuf.b          vr3,    \in0,  \in0,   vr7
    vshuf.b          vr4,    \in0,  \in0,   vr8
    vmulwev.h.bu.b   vr12,   vr2,   vr10
    vmulwev.h.bu.b   vr13,   vr3,   vr11
    vmulwev.h.bu.b   vr14,   vr3,   vr10
    vmulwev.h.bu.b   vr15,   vr4,   vr11
    vmaddwod.h.bu.b  vr12,   vr2,   vr10
    vmaddwod.h.bu.b  vr13,   vr3,   vr11
    vmaddwod.h.bu.b  vr14,   vr3,   vr10
    vmaddwod.h.bu.b  vr15,   vr4,   vr11
    vadd.h           vr12,   vr12,  vr13
    vadd.h           vr14,   vr14,  vr15
    vhaddw.w.h       vr12,   vr12,  vr12
    vhaddw.w.h       vr14,   vr14,  vr14
    vpickev.h        \in0,   vr14,  vr12
    vadd.h           \in0,   \in0,  vr9
.endm

const subpel_h_shuf0
.byte 0, 1, 2, 3, 1, 2, 3, 4, 16, 17, 18, 19, 17, 18, 19, 20
endconst
const subpel_h_shuf1
.byte 0, 1, 2, 3, 1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6
endconst
const subpel_h_shuf2
.byte 0, 1, 2, 3, 1, 2, 3, 4,  8,  9, 10, 11,  9, 10, 11, 12
.byte 2, 3, 4, 5, 3, 4, 5, 6, 10, 11, 12, 13, 11, 12, 13, 14
endconst
const subpel_h_shuf3
.byte 0, 4, 1, 5, 2, 6, 3, 7, 4, 8, 5, 9, 6, 10, 7, 11
.byte 0, 4, 1, 5, 2, 6, 3, 7, 4, 8, 5, 9, 6, 10, 7, 11
endconst

.macro FILTER_8TAP_8W in0
    vshuf.b         vr13,    \in0,  \in0,  vr7
    vshuf.b         vr14,    \in0,  \in0,  vr11
    vshuf.b         vr15,    \in0,  \in0,  vr12
    vmulwev.h.bu.b  vr16,    vr13,  vr8
    vmulwev.h.bu.b  vr17,    vr14,  vr10
    vmulwev.h.bu.b  vr18,    vr14,  vr8
    vmulwev.h.bu.b  vr19,    vr15,  vr10
    vmaddwod.h.bu.b vr16,    vr13,  vr8
    vmaddwod.h.bu.b vr17,    vr14,  vr10
    vmaddwod.h.bu.b vr18,    vr14,  vr8
    vmaddwod.h.bu.b vr19,    vr15,  vr10
    vadd.h          vr16,    vr16,  vr17
    vadd.h          vr18,    vr18,  vr19
    vhaddw.w.h      vr16,    vr16,  vr16
    vhaddw.w.h      \in0,    vr18,  vr18
    vssrarni.h.w    \in0,    vr16,  2
.endm

.macro PUT_8TAP_8BPC_LSX lable
    li.w             t0,     4
    la.local         t6,     dav1d_mc_subpel_filters
    slli.d           t2,     a3,    1  //src_stride*2
    add.d            t3,     t2,    a3 //src_stride*3
    slli.d           t4,     t2,    1  //src_stride*4

    bnez             a6,     .l_\lable\()put_h //mx
    bnez             a7,     .l_\lable\()put_v //my

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_hv0_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_hv0_jtable:
    .dword .l_\lable\()put_hv0_128w - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_64w  - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_32w  - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_16w  - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_8w   - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_4w   - .l_\lable\()put_hv0_jtable
    .dword .l_\lable\()put_hv0_2w   - .l_\lable\()put_hv0_jtable

.l_\lable\()put_hv0_2w:
    vldrepl.h        vr0,    a2,    0
    add.d            a2,     a2,    a3
    vldrepl.h        vr1,    a2,    0
    vstelm.h         vr0,    a0,    0,     0
    add.d            a0,     a0,    a1
    vstelm.h         vr1,    a0,    0,     0
    add.d            a2,     a2,    a3
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_2w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_4w:
    fld.s            f0,     a2,    0
    fldx.s           f1,     a2,    a3
    fst.s            f0,     a0,    0
    fstx.s           f1,     a0,    a1
    alsl.d           a2,     a3,    a2,    1
    alsl.d           a0,     a1,    a0,    1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_4w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_8w:
    fld.d            f0,     a2,    0
    fldx.d           f1,     a2,    a3
    fst.d            f0,     a0,    0
    fstx.d           f1,     a0,    a1
    alsl.d           a2,     a3,    a2,    1
    alsl.d           a0,     a1,    a0,    1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_8w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_16w:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    vst              vr0,    a0,    0
    vstx             vr1,    a0,    a1
    alsl.d           a2,     a3,    a2,    1
    alsl.d           a0,     a1,    a0,    1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_16w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_32w:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    16
    add.d            a2,     a2,    a3
    vld              vr2,    a2,    0
    vld              vr3,    a2,    16
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    add.d            a0,     a0,    a1
    vst              vr2,    a0,    0
    vst              vr3,    a0,    16
    add.d            a2,     a2,    a3
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_32w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_64w:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    16
    vld              vr2,    a2,    32
    vld              vr3,    a2,    48
    add.d            a2,     a2,    a3
    vld              vr4,    a2,    0
    vld              vr5,    a2,    16
    vld              vr6,    a2,    32
    vld              vr7,    a2,    48
    add.d            a2,     a2,    a3
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    vst              vr2,    a0,    32
    vst              vr3,    a0,    48
    add.d            a0,     a0,    a1
    vst              vr4,    a0,    0
    vst              vr5,    a0,    16
    vst              vr6,    a0,    32
    vst              vr7,    a0,    48
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_64w
    b                .l_\lable\()end_put_8tap
.l_\lable\()put_hv0_128w:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    16
    vld              vr2,    a2,    32
    vld              vr3,    a2,    48
    vld              vr4,    a2,    64
    vld              vr5,    a2,    80
    vld              vr6,    a2,    96
    vld              vr7,    a2,    112
    add.d            a2,     a2,    a3
    vld              vr8,    a2,    0
    vld              vr9,    a2,    16
    vld              vr10,   a2,    32
    vld              vr11,   a2,    48
    vld              vr12,   a2,    64
    vld              vr13,   a2,    80
    vld              vr14,   a2,    96
    vld              vr15,   a2,    112
    add.d            a2,     a2,    a3
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    vst              vr2,    a0,    32
    vst              vr3,    a0,    48
    vst              vr4,    a0,    64
    vst              vr5,    a0,    80
    vst              vr6,    a0,    96
    vst              vr7,    a0,    112
    add.d            a0,     a0,    a1
    vst              vr8,    a0,    0
    vst              vr9,    a0,    16
    vst              vr10,   a0,    32
    vst              vr11,   a0,    48
    vst              vr12,   a0,    64
    vst              vr13,   a0,    80
    vst              vr14,   a0,    96
    vst              vr15,   a0,    112
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv0_128w
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h:
    bnez             a7,     .l_\lable\()put_hv //if(fh) && if (fv)
    ld.d             t5,     sp,    0  //filter_type
    andi             t1,     t5,    3
    blt              t0,     a4,    .l_\lable\()put_h_idx_fh
    andi             t1,     t5,    1
    addi.w           t1,     t1,    3

.l_\lable\()put_h_idx_fh:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t7,     t6,    t1 //fh's offset
    li.w             t1,     34
    vreplgr2vr.h     vr9,    t1

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_h_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_h_jtable:
    .dword .l_\lable\()put_h_128w - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_64w  - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_32w  - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_16w  - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_8w   - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_4w   - .l_\lable\()put_h_jtable
    .dword .l_\lable\()put_h_2w   - .l_\lable\()put_h_jtable

.l_\lable\()put_h_2w:
    addi.d           t7,     t7,    2
    addi.d           a2,     a2,    -1
    vldrepl.w        vr8,    t7,    0
    la.local         t7,     subpel_h_shuf0
    vld              vr7,    t7,    0
.l_\lable\()put_h_2w_loop:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2

    vshuf.b          vr0,    vr1,   vr0,   vr7
    vdp2.h.bu.b      vr1,    vr0,   vr8
    vhaddw.w.h       vr0,    vr1,   vr1
    vpickev.h        vr0,    vr0,   vr0
    vadd.h           vr0,    vr0,   vr9
    vssrani.bu.h     vr0,    vr0,   6

    vstelm.h         vr0,    a0,    0,     0
    add.d            a0,     a0,    a1
    vstelm.h         vr0,    a0,    0,     1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_2w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h_4w:
    addi.d           t7,     t7,    2
    addi.d           a2,     a2,    -1
    vldrepl.w        vr8,    t7,    0
    la.local         t7,     subpel_h_shuf1
    vld              vr7,    t7,    0
.l_\lable\()put_h_4w_loop:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2

    vshuf.b          vr0,    vr0,   vr0,   vr7
    vshuf.b          vr1,    vr1,   vr1,   vr7
    vmulwev.h.bu.b   vr2,    vr0,   vr8
    vmulwev.h.bu.b   vr3,    vr1,   vr8
    vmaddwod.h.bu.b  vr2,    vr0,   vr8
    vmaddwod.h.bu.b  vr3,    vr1,   vr8
    vhaddw.w.h       vr0,    vr2,   vr2
    vhaddw.w.h       vr1,    vr3,   vr3
    vpickev.h        vr0,    vr1,   vr0
    vadd.h           vr0,    vr0,   vr9
    vssrani.bu.h     vr0,    vr0,   6

    vstelm.w         vr0,    a0,    0,     0
    add.d            a0,     a0,    a1
    vstelm.w         vr0,    a0,    0,     1
    add.d            a0,     a0,    a1
    addi.d           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_4w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h_8w:
    fld.d            f10,    t7,    0
    vreplvei.w       vr11,   vr10,  1
    vreplvei.w       vr10,   vr10,  0
    la.local         t7,     subpel_h_shuf1
    vld              vr6,    t7,    0
    vaddi.bu         vr7,    vr6,   4
    vaddi.bu         vr8,    vr6,   8
    addi.d           a2,     a2,    -3
.l_\lable\()put_h_8w_loop:
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    add.d            a2,     a2,    t2
    PUT_H_8W         vr0
    PUT_H_8W         vr1
    vssrani.bu.h     vr1,    vr0,   6
    vstelm.d         vr1,    a0,    0,    0
    add.d            a0,     a0,    a1
    vstelm.d         vr1,    a0,    0,    1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_h_8w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_h_16w:
.l_\lable\()put_h_32w:
.l_\lable\()put_h_64w:
.l_\lable\()put_h_128w:
    fld.d            f10,    t7,    0
    vreplvei.w       vr11,   vr10,  1
    vreplvei.w       vr10,   vr10,  0
    la.local         t7,     subpel_h_shuf1
    vld              vr6,    t7,    0
    vaddi.bu         vr7,    vr6,   4
    vaddi.bu         vr8,    vr6,   8
    addi.d           a2,     a2,    -3
    addi.d           t0,     a2,    0 //src
    addi.w           t5,     a5,    0 //h
    addi.d           t8,     a0,    0 //dst
.l_\lable\()put_h_16w_loop:
    vld              vr0,    a2,    0
    vld              vr1,    a2,    8
    add.d            a2,     a2,    a3
    PUT_H_8W         vr0
    PUT_H_8W         vr1
    vssrani.bu.h     vr1,    vr0,   6
    vst              vr1,    a0,    0
    add.d            a0,     a0,    a1
    addi.d           a5,     a5,    -1
    bnez             a5,     .l_\lable\()put_h_16w_loop
    addi.d           a2,     t0,    16
    addi.d           t0,     t0,    16
    addi.d           a0,     t8,    16
    addi.d           t8,     t8,    16
    addi.w           a5,     t5,    0
    addi.w           a4,     a4,    -16
    bnez             a4,     .l_\lable\()put_h_16w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_v:
    ld.d             t1,     sp,    0  //filter_type
    srli.w           t1,     t1,    2
    blt              t0,     a5,    .l_\lable\()put_v_idx_fv
    andi             t1,     t1,    1
    addi.w           t1,     t1,    3

.l_\lable\()put_v_idx_fv:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a7,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fv's offset
    vldrepl.d        vr8,    t1,    0
    sub.d            a2,     a2,    t3

    vilvl.h          vr8,    vr8,   vr8
    vreplvei.w       vr9,    vr8,   1
    vreplvei.w       vr10,   vr8,   2
    vreplvei.w       vr11,   vr8,   3
    vreplvei.w       vr8,    vr8,   0

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_v_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_v_jtable:
    .dword .l_\lable\()put_v_128w - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_64w  - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_32w  - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_16w  - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_8w   - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_4w   - .l_\lable\()put_v_jtable
    .dword .l_\lable\()put_v_2w   - .l_\lable\()put_v_jtable

.l_\lable\()put_v_2w:
    fld.s            f0,     a2,    0
    fldx.s           f1,     a2,    a3
    fldx.s           f2,     a2,    t2
    add.d            a2,     a2,    t3
    fld.s            f3,     a2,    0
    fldx.s           f4,     a2,    a3
    fldx.s           f5,     a2,    t2
    fldx.s           f6,     a2,    t3
    add.d            a2,     a2,    t4

    vilvl.h          vr0,    vr1,   vr0 //0 1
    vilvl.h          vr1,    vr2,   vr1 //1 2
    vilvl.b          vr0,    vr1,   vr0 //01 12
    vilvl.h          vr2,    vr3,   vr2 //2 3
    vilvl.h          vr3,    vr4,   vr3 //3 4
    vilvl.b          vr1,    vr3,   vr2 //23 34
    vilvl.h          vr2,    vr5,   vr4 //4 5
    vilvl.h          vr3,    vr6,   vr5 //5 6
    vilvl.b          vr2,    vr3,   vr2 //45 56
.l_\lable\()put_v_2w_loop:
    fld.s            f7,     a2,    0
    vilvl.h          vr3,    vr7,   vr6 //6 7
    fldx.s           f6,     a2,    a3
    add.d            a2,     a2,    t2
    vilvl.h          vr4,    vr6,   vr7 //7 8
    vilvl.b          vr3,    vr4,   vr3 //67 78

    vmulwev.h.bu.b   vr12,   vr0,   vr8
    vmulwev.h.bu.b   vr13,   vr1,   vr9
    vmulwev.h.bu.b   vr14,   vr2,   vr10
    vmulwev.h.bu.b   vr15,   vr3,   vr11
    vmaddwod.h.bu.b  vr12,   vr0,   vr8
    vmaddwod.h.bu.b  vr13,   vr1,   vr9
    vmaddwod.h.bu.b  vr14,   vr2,   vr10
    vmaddwod.h.bu.b  vr15,   vr3,   vr11
    vaddi.hu         vr0,    vr1,   0
    vaddi.hu         vr1,    vr2,   0
    vaddi.hu         vr2,    vr3,   0
    vadd.h           vr12,   vr12,  vr13
    vadd.h           vr12,   vr12,  vr14
    vadd.h           vr12,   vr12,  vr15

    vssrarni.bu.h    vr12,   vr12,  6
    vstelm.h         vr12,   a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.h         vr12,   a0,    0,   1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_v_2w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_v_4w:
    fld.s            f0,     a2,    0
    fldx.s           f1,     a2,    a3
    fldx.s           f2,     a2,    t2
    add.d            a2,     a2,    t3
    fld.s            f3,     a2,    0
    fldx.s           f4,     a2,    a3
    fldx.s           f5,     a2,    t2
    fldx.s           f6,     a2,    t3
    add.d            a2,     a2,    t4

    vilvl.w          vr0,    vr1,   vr0
    vilvl.w          vr1,    vr2,   vr1
    vilvl.b          vr0,    vr1,   vr0
    vilvl.w          vr1,    vr3,   vr2
    vilvl.w          vr2,    vr4,   vr3
    vilvl.b          vr1,    vr2,   vr1
    vilvl.w          vr2,    vr5,   vr4
    vilvl.w          vr3,    vr6,   vr5
    vilvl.b          vr2,    vr3,   vr2
.l_\lable\()put_v_4w_loop:
    fld.s            f7,     a2,    0

    vilvl.w          vr3,    vr7,   vr6
    fldx.s           f6,     a2,    a3
    add.d            a2,     a2,    t2
    vilvl.w          vr4,    vr6,   vr7
    vilvl.b          vr3,    vr4,   vr3

    vmulwev.h.bu.b   vr12,   vr0,   vr8
    vmulwev.h.bu.b   vr13,   vr1,   vr9
    vmulwev.h.bu.b   vr14,   vr2,   vr10
    vmulwev.h.bu.b   vr15,   vr3,   vr11
    vmaddwod.h.bu.b  vr12,   vr0,   vr8
    vmaddwod.h.bu.b  vr13,   vr1,   vr9
    vmaddwod.h.bu.b  vr14,   vr2,   vr10
    vmaddwod.h.bu.b  vr15,   vr3,   vr11
    vaddi.hu         vr0,    vr1,   0
    vaddi.hu         vr1,    vr2,   0
    vaddi.hu         vr2,    vr3,   0
    vadd.h           vr12,   vr12,  vr13
    vadd.h           vr12,   vr12,  vr14
    vadd.h           vr12,   vr12,  vr15

    vssrarni.bu.h    vr12,   vr12,  6
    vstelm.w         vr12,   a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.w         vr12,   a0,    0,   1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_v_4w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_v_8w:
.l_\lable\()put_v_16w:
.l_\lable\()put_v_32w:
.l_\lable\()put_v_64w:
.l_\lable\()put_v_128w:
    addi.d           t0,     a2,    0 //src
    addi.d           t5,     a5,    0 //h
    addi.d           t8,     a0,    0 //dst
.l_\lable\()put_v_8w_loop0:
    fld.d            f0,     a2,    0
    fldx.d           f1,     a2,    a3
    fldx.d           f2,     a2,    t2
    add.d            a2,     a2,    t3
    fld.d            f3,     a2,    0
    fldx.d           f4,     a2,    a3
    fldx.d           f5,     a2,    t2
    fldx.d           f6,     a2,    t3
    add.d            a2,     a2,    t4

    vilvl.b          vr0,    vr1,   vr0 //0 1
    vilvl.b          vr1,    vr2,   vr1 //1 2
    vilvl.b          vr2,    vr3,   vr2 //2 3
    vilvl.b          vr3,    vr4,   vr3 //3 4
    vilvl.b          vr4,    vr5,   vr4 //4 5
    vilvl.b          vr5,    vr6,   vr5 //5 6
.l_\lable\()put_v_8w_loop:
    fld.d            f7,     a2,    0
    vilvl.b          vr12,   vr7,   vr6 //6 7
    fldx.d           f6,     a2,    a3
    add.d            a2,     a2,    t2
    vilvl.b          vr13,   vr6,   vr7 //7 8

    vmulwev.h.bu.b   vr14,   vr0,   vr8
    vmulwev.h.bu.b   vr15,   vr1,   vr8
    vmulwev.h.bu.b   vr16,   vr2,   vr9
    vmulwev.h.bu.b   vr17,   vr3,   vr9
    vmulwev.h.bu.b   vr18,   vr4,   vr10
    vmulwev.h.bu.b   vr19,   vr5,   vr10
    vmulwev.h.bu.b   vr20,   vr12,  vr11
    vmulwev.h.bu.b   vr21,   vr13,  vr11
    vmaddwod.h.bu.b  vr14,   vr0,   vr8
    vmaddwod.h.bu.b  vr15,   vr1,   vr8
    vmaddwod.h.bu.b  vr16,   vr2,   vr9
    vmaddwod.h.bu.b  vr17,   vr3,   vr9
    vmaddwod.h.bu.b  vr18,   vr4,   vr10
    vmaddwod.h.bu.b  vr19,   vr5,   vr10
    vmaddwod.h.bu.b  vr20,   vr12,  vr11
    vmaddwod.h.bu.b  vr21,   vr13,  vr11

    vaddi.hu         vr0,    vr2,   0
    vaddi.hu         vr1,    vr3,   0
    vaddi.hu         vr2,    vr4,   0
    vaddi.hu         vr3,    vr5,   0
    vaddi.hu         vr4,    vr12,  0
    vaddi.hu         vr5,    vr13,  0
    vadd.h           vr14,   vr14,  vr16
    vadd.h           vr14,   vr14,  vr18
    vadd.h           vr14,   vr14,  vr20
    vadd.h           vr15,   vr15,  vr17
    vadd.h           vr15,   vr15,  vr19
    vadd.h           vr15,   vr15,  vr21

    vssrarni.bu.h    vr15,   vr14,  6
    vstelm.d         vr15,   a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.d         vr15,   a0,    0,   1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_v_8w_loop
    addi.d           a2,     t0,    8
    addi.d           t0,     t0,    8
    addi.d           a0,     t8,    8
    addi.d           t8,     t8,    8
    addi.d           a5,     t5,    0
    addi.w           a4,     a4,    -8
    bnez             a4,     .l_\lable\()put_v_8w_loop0
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_hv:
    ld.d             t5,     sp,    0  //filter_type
    andi             t1,     t5,    3
    blt              t0,     a4,    .l_\lable\()put_hv_idx_fh
    andi             t1,     t5,    1
    addi.w           t1,     t1,    3
.l_\lable\()put_hv_idx_fh:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fh's offset
    vldrepl.d        vr8,    t1,    0
    ld.d             t1,     sp,    0  //filter_type
    srli.w           t1,     t1,    2
    blt              t0,     a5,    .l_\lable\()put_hv_idx_fv
    andi             t1,     t1,    1
    addi.w           t1,     t1,    3
.l_\lable\()put_hv_idx_fv:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a7,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fv's offset
    vldrepl.d        vr9,    t1,    0
    vexth.h.b        vr9,    vr9

    sub.d            a2,     a2,    t3
    addi.d           a2,     a2,    -3

    clz.w            t1,     a4
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()put_hv_jtable
    alsl.d           t1,     t1,    t5,   3
    ld.d             t6,     t1,    0
    add.d            t5,     t5,    t6
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()put_hv_jtable:
    .dword .l_\lable\()put_hv_128w - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_64w  - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_32w  - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_16w  - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_8w   - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_4w   - .l_\lable\()put_hv_jtable
    .dword .l_\lable\()put_hv_2w   - .l_\lable\()put_hv_jtable

.l_\lable\()put_hv_2w:
    addi.d           a2,     a2,    2
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    vldx             vr2,    a2,    t2
    add.d            a2,     a2,    t3
    vld              vr3,    a2,    0
    vldx             vr4,    a2,    a3
    vldx             vr5,    a2,    t2
    vldx             vr6,    a2,    t3
    add.d            a2,     a2,    t4

    la.local         t1,     subpel_h_shuf0
    vld              vr7,    t1,    0
    vbsrl.v          vr8,    vr8,   2
    vreplvei.w       vr8,    vr8,   0

    //fv
    vreplvei.w       vr14,   vr9,   1
    vreplvei.w       vr15,   vr9,   2
    vreplvei.w       vr16,   vr9,   3
    vreplvei.w       vr9,    vr9,   0

    vshuf.b          vr0,    vr1,   vr0,  vr7
    vshuf.b          vr1,    vr3,   vr2,  vr7
    vshuf.b          vr2,    vr5,   vr4,  vr7
    vshuf.b          vr3,    vr6,   vr6,  vr7
    vmulwev.h.bu.b   vr10,   vr0,   vr8
    vmulwev.h.bu.b   vr11,   vr1,   vr8
    vmulwev.h.bu.b   vr12,   vr2,   vr8
    vmulwev.h.bu.b   vr13,   vr3,   vr8
    vmaddwod.h.bu.b  vr10,   vr0,   vr8
    vmaddwod.h.bu.b  vr11,   vr1,   vr8
    vmaddwod.h.bu.b  vr12,   vr2,   vr8
    vmaddwod.h.bu.b  vr13,   vr3,   vr8
    vhaddw.w.h       vr0,    vr10,  vr10
    vhaddw.w.h       vr1,    vr11,  vr11
    vssrarni.h.w     vr1,    vr0,   2 //h0 h1 h2 h3
    vhaddw.w.h       vr2,    vr12,  vr12
    vhaddw.w.h       vr3,    vr13,  vr13
    vssrarni.h.w     vr3,    vr2,   2 //h4 h5 h6 ~
    vbsrl.v          vr2,    vr1,   4
    vextrins.w       vr2,    vr3,   0x30 //h1 h2 h3 h4
    vilvl.h          vr4,    vr2,   vr1 //h0 h1 h1 h2 --
    vilvh.h          vr5,    vr2,   vr1 //h2 h3 h3 h4 --
    vbsrl.v          vr6,    vr3,   4
    vilvl.h          vr6,    vr6,   vr3 //h4 h5 h5 h6 --
    vbsrl.v          vr3,    vr3,   8  //h6 ~
.l_\lable\()put_hv_2w_loop:
    vld              vr0,    a2,    0
    vldx             vr2,    a2,    a3
    add.d            a2,     a2,    t2
    vshuf.b          vr0,    vr2,   vr0,  vr7
    vdp2.h.bu.b      vr17,   vr0,   vr8
    vhaddw.w.h       vr17,   vr17,  vr17
    vssrarni.h.w     vr17,   vr17,  2 //h7 h8
    vextrins.w       vr3,    vr17,  0x10 //h6 h7
    vilvl.h          vr3,    vr17,  vr3  //h6 h7 h7 h8 --

    vmulwev.w.h      vr18,   vr4,   vr9
    vmulwev.w.h      vr19,   vr5,   vr14
    vmulwev.w.h      vr20,   vr6,   vr15
    vmulwev.w.h      vr21,   vr3,   vr16
    vmaddwod.w.h     vr18,   vr4,   vr9
    vmaddwod.w.h     vr19,   vr5,   vr14
    vmaddwod.w.h     vr20,   vr6,   vr15
    vmaddwod.w.h     vr21,   vr3,   vr16
    vaddi.hu         vr4,    vr5,   0
    vaddi.hu         vr5,    vr6,   0
    vaddi.hu         vr6,    vr3,   0
    vbsrl.v          vr3,    vr17,  4 //h8 ~
    vadd.w           vr18,   vr18,  vr19
    vadd.w           vr18,   vr18,  vr20
    vadd.w           vr18,   vr18,  vr21

    vssrarni.hu.w    vr0,    vr18,  10
    vssrani.bu.h     vr0,    vr0,   0
    vstelm.h         vr0,    a0,    0,   0
    add.d            a0,     a0,    a1
    vstelm.h         vr0,    a0,    0,   1
    add.d            a0,     a0,    a1
    addi.d           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv_2w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_hv_4w:
    addi.d           a2,     a2,    2 //ignore leading 0
    vld              vr0,    a2,    0
    vldx             vr1,    a2,    a3
    vldx             vr2,    a2,    t2
    add.d            a2,     a2,    t3
    vld              vr3,    a2,    0
    vldx             vr4,    a2,    a3
    vldx             vr5,    a2,    t2
    vldx             vr6,    a2,    t3
    add.d            a2,     a2,    t4

    la.local         t1,     subpel_h_shuf1
    vld              vr7,    t1,    0
    vbsrl.v          vr8,    vr8,   2
    vreplvei.w       vr8,    vr8,   0

    //fv
    vreplvei.w       vr17,   vr9,   0
    vreplvei.w       vr18,   vr9,   1
    vreplvei.w       vr19,   vr9,   2
    vreplvei.w       vr20,   vr9,   3

    //DAV1D_FILTER_8TAP_RND
    vshuf.b          vr0,    vr0,   vr0,  vr7
    vshuf.b          vr1,    vr1,   vr1,  vr7
    vshuf.b          vr2,    vr2,   vr2,  vr7
    vshuf.b          vr3,    vr3,   vr3,  vr7
    vshuf.b          vr4,    vr4,   vr4,  vr7
    vshuf.b          vr5,    vr5,   vr5,  vr7
    vshuf.b          vr6,    vr6,   vr6,  vr7

    vmulwev.h.bu.b   vr10,   vr0,   vr8
    vmulwev.h.bu.b   vr11,   vr1,   vr8
    vmulwev.h.bu.b   vr12,   vr2,   vr8
    vmulwev.h.bu.b   vr13,   vr3,   vr8
    vmulwev.h.bu.b   vr14,   vr4,   vr8
    vmulwev.h.bu.b   vr15,   vr5,   vr8
    vmulwev.h.bu.b   vr16,   vr6,   vr8
    vmaddwod.h.bu.b  vr10,   vr0,   vr8
    vmaddwod.h.bu.b  vr11,   vr1,   vr8
    vmaddwod.h.bu.b  vr12,   vr2,   vr8
    vmaddwod.h.bu.b  vr13,   vr3,   vr8
    vmaddwod.h.bu.b  vr14,   vr4,   vr8
    vmaddwod.h.bu.b  vr15,   vr5,   vr8
    vmaddwod.h.bu.b  vr16,   vr6,   vr8

    vhaddw.w.h       vr10,   vr10,  vr10
    vhaddw.w.h       vr11,   vr11,  vr11
    vhaddw.w.h       vr12,   vr12,  vr12
    vhaddw.w.h       vr13,   vr13,  vr13
    vhaddw.w.h       vr14,   vr14,  vr14
    vhaddw.w.h       vr15,   vr15,  vr15
    vhaddw.w.h       vr16,   vr16,  vr16

    vssrarni.h.w     vr10,   vr10,  2 //h0
    vssrarni.h.w     vr11,   vr11,  2 //h1
    vssrarni.h.w     vr12,   vr12,  2 //h2
    vssrarni.h.w     vr13,   vr13,  2 //h3
    vssrarni.h.w     vr14,   vr14,  2 //h4
    vssrarni.h.w     vr15,   vr15,  2 //h5
    vssrarni.h.w     vr16,   vr16,  2 //h6

    //h0
    vilvl.h          vr0,    vr11,  vr10 //01
    vilvl.h          vr1,    vr13,  vr12 //23
    vilvl.h          vr2,    vr15,  vr14 //45
    //h1
    vilvl.h          vr4,    vr12,  vr11 //12
    vilvl.h          vr5,    vr14,  vr13 //34
    vilvl.h          vr6,    vr16,  vr15 //56

.l_\lable\()put_hv_4w_loop:
    vld              vr9,    a2,    0
    vldx             vr10,   a2,    a3
    add.d            a2,     a2,    t2

    //DAV1D_FILTER_8TAP_CLIP
    vshuf.b          vr9,    vr9,   vr9,  vr7
    vshuf.b          vr10,   vr10,  vr10, vr7
    vmulwev.h.bu.b   vr11,   vr9,   vr8
    vmulwev.h.bu.b   vr12,   vr10,  vr8
    vmaddwod.h.bu.b  vr11,   vr9,   vr8
    vmaddwod.h.bu.b  vr12,   vr10,  vr8
    vhaddw.w.h       vr11,   vr11,  vr11
    vhaddw.w.h       vr12,   vr12,  vr12
    vssrarni.h.w     vr11,   vr11,  2 //h7
    vssrarni.h.w     vr12,   vr12,  2 //h8
    vilvl.h          vr3,    vr11,  vr16 //67
    vilvl.h          vr13,   vr12,  vr11 //78

    vmulwev.w.h      vr9,    vr0,   vr17
    vmulwev.w.h      vr10,   vr1,   vr18
    vmulwev.w.h      vr14,   vr2,   vr19
    vmulwev.w.h      vr15,   vr3,   vr20
    vmaddwod.w.h     vr9,    vr0,   vr17
    vmaddwod.w.h     vr10,   vr1,   vr18
    vmaddwod.w.h     vr14,   vr2,   vr19
    vmaddwod.w.h     vr15,   vr3,   vr20
    vadd.w           vr16,   vr9,   vr10
    vadd.w           vr16,   vr16,  vr14
    vadd.w           vr16,   vr16,  vr15

    vmulwev.w.h      vr9,    vr4,   vr17
    vmulwev.w.h      vr10,   vr5,   vr18
    vmulwev.w.h      vr14,   vr6,   vr19
    vmulwev.w.h      vr15,   vr13,  vr20
    vmaddwod.w.h     vr9,    vr4,   vr17
    vmaddwod.w.h     vr10,   vr5,   vr18
    vmaddwod.w.h     vr14,   vr6,   vr19
    vmaddwod.w.h     vr15,   vr13,  vr20
    vadd.w           vr21,   vr9,   vr10
    vadd.w           vr21,   vr21,  vr14
    vadd.w           vr21,   vr21,  vr15

    vssrarni.hu.w    vr21,   vr16,  10
    vssrani.bu.h     vr21,   vr21,  0
    //cache
    vaddi.hu         vr0,    vr1,   0
    vaddi.hu         vr1,    vr2,   0
    vaddi.hu         vr2,    vr3,   0
    vaddi.hu         vr4,    vr5,   0
    vaddi.hu         vr5,    vr6,   0
    vaddi.hu         vr6,    vr13,  0
    vaddi.hu         vr16,   vr12,  0

    vstelm.w         vr21,   a0,    0,    0
    add.d            a0,     a0,    a1
    vstelm.w         vr21,   a0,    0,    1
    add.d            a0,     a0,    a1
    addi.w           a5,     a5,    -2
    bnez             a5,     .l_\lable\()put_hv_4w_loop
    b                .l_\lable\()end_put_8tap

.l_\lable\()put_hv_8w:
.l_\lable\()put_hv_16w:
.l_\lable\()put_hv_32w:
.l_\lable\()put_hv_64w:
.l_\lable\()put_hv_128w:
    addi.d          sp,      sp,    -8*8
    fst.d           f24,     sp,    0
    fst.d           f25,     sp,    8
    fst.d           f26,     sp,    16
    fst.d           f27,     sp,    24
    fst.d           f28,     sp,    32
    fst.d           f29,     sp,    40
    fst.d           f30,     sp,    48
    fst.d           f31,     sp,    56
    addi.d          t0,      a2,    0 //src
    addi.d          t5,      a5,    0 //h
    addi.d          t8,      a0,    0 //dst
    la.local        t1,      subpel_h_shuf1
    vld             vr7,     t1,    0
    vaddi.bu        vr11,    vr7,   4
    vaddi.bu        vr12,    vr7,   8
    vreplvei.w      vr10,    vr8,   1
    vreplvei.w      vr8,     vr8,   0
    vreplvei.w      vr20,    vr9,   1
    vreplvei.w      vr21,    vr9,   2
    vreplvei.w      vr22,    vr9,   3
    vreplvei.w      vr9,     vr9,   0
.l_\lable\()put_hv_8w_loop0:
    vld             vr0,     a2,    0
    vldx            vr1,     a2,    a3
    vldx            vr2,     a2,    t2
    add.d           a2,      a2,    t3
    vld             vr3,     a2,    0
    vldx            vr4,     a2,    a3
    vldx            vr5,     a2,    t2
    vldx            vr6,     a2,    t3
    add.d           a2,      a2,    t4

    FILTER_8TAP_8W  vr0 //h0
    FILTER_8TAP_8W  vr1 //h1
    FILTER_8TAP_8W  vr2 //h2
    FILTER_8TAP_8W  vr3 //h3
    FILTER_8TAP_8W  vr4 //h4
    FILTER_8TAP_8W  vr5 //h5
    FILTER_8TAP_8W  vr6 //h6

    //h0' low part
    vilvl.h         vr23,    vr1,   vr0 //01
    vilvl.h         vr24,    vr3,   vr2 //23
    vilvl.h         vr25,    vr5,   vr4 //45
    //h0' high part
    vilvh.h         vr26,    vr1,   vr0 //01
    vilvh.h         vr27,    vr3,   vr2 //23
    vilvh.h         vr28,    vr5,   vr4 //45

    //h1' low part
    vilvl.h         vr29,    vr2,   vr1 //12
    vilvl.h         vr30,    vr4,   vr3 //34
    vilvl.h         vr31,    vr6,   vr5 //56
    //h1' high part
    vilvh.h         vr0,     vr2,   vr1 //12
    vilvh.h         vr1,     vr4,   vr3 //34
    vilvh.h         vr2,     vr6,   vr5 //56

.l_\lable\()put_hv_8w_loop:
    vld             vr3,     a2,    0
    vldx            vr4,     a2,    a3
    add.d           a2,      a2,    t2

    FILTER_8TAP_8W  vr3 //h7
    FILTER_8TAP_8W  vr4 //h8

    //h0' low part
    vilvl.h         vr16,    vr3,   vr6 //67 ~low
    vmulwev.w.h     vr13,    vr23,  vr9
    vmulwev.w.h     vr14,    vr24,  vr20
    vmulwev.w.h     vr15,    vr25,  vr21
    vmulwev.w.h     vr17,    vr16,  vr22
    vmaddwod.w.h    vr13,    vr23,  vr9
    vmaddwod.w.h    vr14,    vr24,  vr20
    vmaddwod.w.h    vr15,    vr25,  vr21
    vmaddwod.w.h    vr17,    vr16,  vr22
    vadd.w          vr13,    vr13,  vr14
    vadd.w          vr13,    vr13,  vr15
    vadd.w          vr13,    vr13,  vr17
    //cache
    vaddi.hu        vr23,    vr24,  0
    vaddi.hu        vr24,    vr25,  0
    vaddi.hu        vr25,    vr16,  0

    //h0' high part
    vilvh.h         vr17,    vr3,   vr6 //67 ~high
    vmulwev.w.h     vr14,    vr26,  vr9
    vmulwev.w.h     vr15,    vr27,  vr20
    vmulwev.w.h     vr16,    vr28,  vr21
    vmulwev.w.h     vr18,    vr17,  vr22
    vmaddwod.w.h    vr14,    vr26,  vr9
    vmaddwod.w.h    vr15,    vr27,  vr20
    vmaddwod.w.h    vr16,    vr28,  vr21
    vmaddwod.w.h    vr18,    vr17,  vr22
    vadd.w          vr14,    vr14,  vr15
    vadd.w          vr14,    vr14,  vr16
    vadd.w          vr14,    vr14,  vr18
    vssrarni.hu.w   vr14,    vr13,  10
    vssrarni.bu.h   vr5,     vr14,  0
    vstelm.d        vr5,     a0,    0,   0
    add.d           a0,      a0,    a1
    //cache
    vaddi.hu        vr26,    vr27,  0
    vaddi.hu        vr27,    vr28,  0
    vaddi.hu        vr28,    vr17,  0
    vaddi.hu        vr6,     vr4,   0

    vilvl.h         vr5,     vr4,   vr3 //78 ~low
    vilvh.h         vr4,     vr4,   vr3 //78 ~high

    //h1' low part
    vmulwev.w.h     vr13,    vr29,  vr9
    vmulwev.w.h     vr14,    vr30,  vr20
    vmulwev.w.h     vr15,    vr31,  vr21
    vmulwev.w.h     vr16,    vr5,   vr22
    vmaddwod.w.h    vr13,    vr29,  vr9
    vmaddwod.w.h    vr14,    vr30,  vr20
    vmaddwod.w.h    vr15,    vr31,  vr21
    vmaddwod.w.h    vr16,    vr5,   vr22
    vadd.w          vr13,    vr13,  vr14
    vadd.w          vr13,    vr13,  vr15
    vadd.w          vr13,    vr13,  vr16
    //cache
    vaddi.hu        vr29,    vr30,  0
    vaddi.hu        vr30,    vr31,  0
    vaddi.hu        vr31,    vr5,   0

    //h1' high part
    vmulwev.w.h     vr14,    vr0,   vr9
    vmulwev.w.h     vr15,    vr1,   vr20
    vmulwev.w.h     vr16,    vr2,   vr21
    vmulwev.w.h     vr17,    vr4,   vr22
    vmaddwod.w.h    vr14,    vr0,   vr9
    vmaddwod.w.h    vr15,    vr1,   vr20
    vmaddwod.w.h    vr16,    vr2,   vr21
    vmaddwod.w.h    vr17,    vr4,   vr22
    vadd.w          vr14,    vr14,  vr15
    vadd.w          vr14,    vr14,  vr16
    vadd.w          vr14,    vr14,  vr17
    vssrarni.hu.w   vr14,    vr13,  10
    vssrarni.bu.h   vr5,     vr14,  0
    vstelm.d        vr5,     a0,    0,   0
    add.d           a0,      a0,    a1
    //cache
    vaddi.hu        vr0,     vr1,   0
    vaddi.hu        vr1,     vr2,   0
    vaddi.hu        vr2,     vr4,   0

    addi.w          a5,      a5,    -2
    bnez            a5,      .l_\lable\()put_hv_8w_loop
    addi.d          a2,      t0,    8
    addi.d          t0,      t0,    8
    addi.d          a0,      t8,    8
    addi.d          t8,      t8,    8
    addi.d          a5,      t5,    0
    addi.w          a4,      a4,    -8
    bnez            a4,      .l_\lable\()put_hv_8w_loop0
    fld.d           f24,     sp,    0
    fld.d           f25,     sp,    8
    fld.d           f26,     sp,    16
    fld.d           f27,     sp,    24
    fld.d           f28,     sp,    32
    fld.d           f29,     sp,    40
    fld.d           f30,     sp,    48
    fld.d           f31,     sp,    56
    addi.d          sp,      sp,    8*8
.l_\lable\()end_put_8tap:
.endm

function put_8tap_regular_8bpc_lsx
    addi.d   sp, sp,  -16
    st.d   zero, sp,  0
    PUT_8TAP_8BPC_LSX 0
    addi.d   sp, sp,  16
endfunc

function put_8tap_smooth_regular_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 1
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 1
    addi.d   sp, sp,  16
endfunc

function put_8tap_sharp_regular_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 2
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 2
    addi.d   sp, sp,  16
endfunc

function put_8tap_regular_smooth_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 4
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 4
    addi.d   sp, sp,  16
endfunc

function put_8tap_smooth_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 5
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 5
    addi.d   sp, sp,  16
endfunc

function put_8tap_sharp_smooth_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 6
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 6
    addi.d   sp, sp,  16
endfunc

function put_8tap_regular_sharp_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 8
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 8
    addi.d   sp, sp,  16
endfunc

function put_8tap_smooth_sharp_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 9
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 9
    addi.d   sp, sp,  16
endfunc

function put_8tap_sharp_8bpc_lsx
    addi.d   sp, sp,  -16
    li.w     t0, 10
    st.d     t0, sp,  0
    PUT_8TAP_8BPC_LSX 10
    addi.d   sp, sp,  16
endfunc

const shufb1
.byte 0,1,2,3,4,5,6,7,1,2,3,4,5,6,7,8
endconst

.macro PREP_H_8W in0
    vshuf.b          vr2,    \in0,  \in0,   vr6
    vshuf.b          vr3,    \in0,  \in0,   vr7
    vshuf.b          vr4,    \in0,  \in0,   vr8
    vmulwev.h.bu.b   vr12,   vr2,   vr22
    vmulwev.h.bu.b   vr13,   vr3,   vr23
    vmulwev.h.bu.b   vr14,   vr3,   vr22
    vmulwev.h.bu.b   vr15,   vr4,   vr23
    vmaddwod.h.bu.b  vr12,   vr2,   vr22
    vmaddwod.h.bu.b  vr13,   vr3,   vr23
    vmaddwod.h.bu.b  vr14,   vr3,   vr22
    vmaddwod.h.bu.b  vr15,   vr4,   vr23
    vadd.h           vr12,   vr12,  vr13
    vadd.h           vr14,   vr14,  vr15
    vhaddw.w.h       vr12,   vr12,  vr12
    vhaddw.w.h       \in0,   vr14,  vr14
    vssrarni.h.w     \in0,   vr12,  2
.endm

.macro PREP_HV_8W_LASX in0
    xvshuf.b         xr4,   \in0,  \in0,   xr19
    xvshuf.b         xr5,   \in0,  \in0,   xr20
    xvshuf.b         xr6,   \in0,  \in0,   xr21
    xvmulwev.h.bu.b  xr7,   xr4,   xr22
    xvmulwev.h.bu.b  xr9,   xr5,   xr23
    xvmulwev.h.bu.b  xr10,  xr5,   xr22
    xvmulwev.h.bu.b  xr11,  xr6,   xr23
    xvmaddwod.h.bu.b xr7,   xr4,   xr22
    xvmaddwod.h.bu.b xr9,   xr5,   xr23
    xvmaddwod.h.bu.b xr10,  xr5,   xr22
    xvmaddwod.h.bu.b xr11,  xr6,   xr23
    xvadd.h          xr7,   xr7,   xr9
    xvadd.h          xr9,   xr10,  xr11
    xvhaddw.w.h      xr7,   xr7,   xr7
    xvhaddw.w.h      \in0,  xr9,   xr9
    xvssrarni.h.w    \in0,  xr7,   2
.endm

.macro PREP_8TAP_8BPC_LASX lable
    li.w             t0,     4
    la.local         t6,     dav1d_mc_subpel_filters
    slli.d           t2,     a2,    1  //src_stride*2
    add.d            t3,     t2,    a2 //src_stride*3
    slli.d           t4,     t2,    1

    bnez             a5,     .l_\lable\()h_lasx //mx
    bnez             a6,     .l_\lable\()v_lasx

    clz.w            t1,     a3
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()prep_hv0_jtable_lasx
    alsl.d           t1,     t1,    t5,   1
    ld.h             t8,     t1,    0
    add.d            t5,     t5,    t8
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()prep_hv0_jtable_lasx:
    .hword .l_\lable\()hv0_128w_lasx - .l_\lable\()prep_hv0_jtable_lasx
    .hword .l_\lable\()hv0_64w_lasx  - .l_\lable\()prep_hv0_jtable_lasx
    .hword .l_\lable\()hv0_32w_lasx  - .l_\lable\()prep_hv0_jtable_lasx
    .hword .l_\lable\()hv0_16w_lasx  - .l_\lable\()prep_hv0_jtable_lasx
    .hword .l_\lable\()hv0_8w_lasx   - .l_\lable\()prep_hv0_jtable_lasx
    .hword .l_\lable\()hv0_4w_lasx   - .l_\lable\()prep_hv0_jtable_lasx

.l_\lable\()hv0_4w_lasx:
    fld.s            f0,     a1,    0
    fldx.s           f1,     a1,    a2
    fldx.s           f2,     a1,    t2
    fldx.s           f3,     a1,    t3
    add.d            a1,     a1,    t4
    xvpackev.w       xr0,    xr1,   xr0
    xvpackev.w       xr1,    xr3,   xr2
    xvpermi.q        xr0,    xr1,   0x02
    xvsllwil.hu.bu   xr0,    xr0,   4
    xvst             xr0,    a0,    0
    addi.d           a0,     a0,    32
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_4w_lasx
    b                .l_\lable\()end_pre_8tap_lasx
.l_\lable\()hv0_8w_lasx:
    fld.d            f0,     a1,    0
    fldx.d           f1,     a1,    a2
    fldx.d           f2,     a1,    t2
    fldx.d           f3,     a1,    t3
    add.d            a1,     a1,    t4
    xvpermi.q        xr0,    xr1,   0x02
    xvpermi.q        xr2,    xr3,   0x02
    xvsllwil.hu.bu   xr0,    xr0,   4
    xvsllwil.hu.bu   xr2,    xr2,   4
    xvst             xr0,    a0,    0
    xvst             xr2,    a0,    32
    addi.d           a0,     a0,    64
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_8w_lasx
    b                .l_\lable\()end_pre_8tap_lasx
.l_\lable\()hv0_16w_lasx:
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    vldx             vr2,    a1,    t2
    vldx             vr3,    a1,    t3
    add.d            a1,     a1,    t4
    vext2xv.hu.bu    xr0,    xr0
    vext2xv.hu.bu    xr1,    xr1
    vext2xv.hu.bu    xr2,    xr2
    vext2xv.hu.bu    xr3,    xr3
    xvslli.h         xr0,    xr0,   4
    xvslli.h         xr1,    xr1,   4
    xvslli.h         xr2,    xr2,   4
    xvslli.h         xr3,    xr3,   4
    xvst             xr0,    a0,    0
    xvst             xr1,    a0,    32
    xvst             xr2,    a0,    64
    xvst             xr3,    a0,    96
    addi.d           a0,     a0,    128
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_16w_lasx
    b                .l_\lable\()end_pre_8tap_lasx
.l_\lable\()hv0_32w_lasx:
    xvld             xr0,    a1,    0
    xvldx            xr1,    a1,    a2
    xvldx            xr2,    a1,    t2
    xvldx            xr3,    a1,    t3
    add.d            a1,     a1,    t4
    xvpermi.d        xr4,    xr0,   0xD8
    xvpermi.d        xr5,    xr1,   0xD8
    xvpermi.d        xr6,    xr2,   0xD8
    xvpermi.d        xr7,    xr3,   0xD8
    xvpermi.d        xr10,   xr0,   0x32
    xvpermi.d        xr11,   xr1,   0x32
    xvpermi.d        xr12,   xr2,   0x32
    xvpermi.d        xr13,   xr3,   0x32
    xvsllwil.hu.bu   xr0,    xr4,   4
    xvsllwil.hu.bu   xr1,    xr5,   4
    xvsllwil.hu.bu   xr2,    xr6,   4
    xvsllwil.hu.bu   xr3,    xr7,   4
    xvsllwil.hu.bu   xr4,    xr10,  4
    xvsllwil.hu.bu   xr5,    xr11,  4
    xvsllwil.hu.bu   xr6,    xr12,  4
    xvsllwil.hu.bu   xr7,    xr13,  4
    xvst             xr0,    a0,    0
    xvst             xr4,    a0,    32
    xvst             xr1,    a0,    64
    xvst             xr5,    a0,    96
    xvst             xr2,    a0,    128
    xvst             xr6,    a0,    160
    xvst             xr3,    a0,    192
    xvst             xr7,    a0,    224
    addi.d           a0,     a0,    256
    addi.d           a4,     a4,    -4
    bnez             a4,     .l_\lable\()hv0_32w_lasx
    b                .l_\lable\()end_pre_8tap_lasx
.l_\lable\()hv0_64w_lasx:
.l_\lable\()hv0_128w_lasx:
    addi.d           t0,     a1,    0
    addi.d           t5,     a4,    0
    srli.w           t7,     a3,    5
    slli.w           t7,     t7,    6
    addi.d           t8,     a0,    0
.l_\lable\()hv0_32_loop_lasx:
    xvld             xr0,    a1,    0
    xvldx            xr1,    a1,    a2
    xvldx            xr2,    a1,    t2
    xvldx            xr3,    a1,    t3
    add.d            a1,     a1,    t4
    xvpermi.d        xr4,    xr0,   0xD8
    xvpermi.d        xr5,    xr1,   0xD8
    xvpermi.d        xr6,    xr2,   0xD8
    xvpermi.d        xr7,    xr3,   0xD8
    xvpermi.d        xr10,   xr0,   0x32
    xvpermi.d        xr11,   xr1,   0x32
    xvpermi.d        xr12,   xr2,   0x32
    xvpermi.d        xr13,   xr3,   0x32
    xvsllwil.hu.bu   xr0,    xr4,   4
    xvsllwil.hu.bu   xr1,    xr5,   4
    xvsllwil.hu.bu   xr2,    xr6,   4
    xvsllwil.hu.bu   xr3,    xr7,   4
    xvsllwil.hu.bu   xr4,    xr10,  4
    xvsllwil.hu.bu   xr5,    xr11,  4
    xvsllwil.hu.bu   xr6,    xr12,  4
    xvsllwil.hu.bu   xr7,    xr13,  4
    xvst             xr0,    a0,    0
    xvst             xr4,    a0,    32
    add.d            t1,     a0,    t7
    xvst             xr1,    t1,    0
    xvst             xr5,    t1,    32
    add.d            t1,     t1,    t7
    xvst             xr2,    t1,    0
    xvst             xr6,    t1,    32
    add.d            t1,     t1,    t7
    xvst             xr3,    t1,    0
    xvst             xr7,    t1,    32
    add.d            a0,     t1,    t7
    addi.d           a4,     a4,   -4
    bnez             a4,     .l_\lable\()hv0_32_loop_lasx
    addi.d           a1,     t0,    32
    addi.d           t0,     t0,    32
    addi.d           a0,     t8,    64
    addi.d           t8,     t8,    64
    addi.d           a4,     t5,    0
    addi.d           a3,     a3,   -32
    bnez             a3,     .l_\lable\()hv0_32_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()h_lasx:
    bnez             a6,     .l_\lable\()hv_lasx //if(fh) && if (fv)

    andi             t1,    a7,    3
    blt              t0,    a3,    .l_\lable\()h_idx_fh_lasx
    andi             t1,    a7,    1
    addi.w           t1,    t1,    3
.l_\lable\()h_idx_fh_lasx:
    addi.w           t5,    zero,  120
    mul.w            t1,    t1,    t5
    addi.w           t5,    a5,    -1
    slli.w           t5,    t5,    3
    add.w            t1,    t1,    t5
    add.d            t1,    t6,    t1 //fh's offset
    xvldrepl.d       xr22,  t1,    0

    addi.d           a1,    a1,    -3
    clz.w            t1,    a3
    li.w             t5,    24
    sub.w            t1,    t1,    t5
    la.local         t5,    .l_\lable\()prep_h_jtable_lasx
    alsl.d           t1,    t1,    t5,   1
    ld.h             t8,    t1,    0
    add.d            t5,    t5,    t8
    jirl             $r0,   t5,    0

    .align   3
.l_\lable\()prep_h_jtable_lasx:
    .hword .l_\lable\()h_128w_lasx - .l_\lable\()prep_h_jtable_lasx
    .hword .l_\lable\()h_64w_lasx  - .l_\lable\()prep_h_jtable_lasx
    .hword .l_\lable\()h_32w_lasx  - .l_\lable\()prep_h_jtable_lasx
    .hword .l_\lable\()h_16w_lasx  - .l_\lable\()prep_h_jtable_lasx
    .hword .l_\lable\()h_8w_lasx   - .l_\lable\()prep_h_jtable_lasx
    .hword .l_\lable\()h_4w_lasx   - .l_\lable\()prep_h_jtable_lasx

.l_\lable\()h_4w_lasx:
    addi.d           a1,    a1,    2
    la.local         t7,    subpel_h_shuf1
    vld              vr7,   t7,    0
    xvreplve0.q      xr7,   xr7
    xvbsrl.v         xr22,  xr22,  2
    xvreplve0.w      xr22,  xr22
.l_\lable\()h_4w_loop_lasx:
    vld              vr0,   a1,    0
    vldx             vr1,   a1,    a2
    vldx             vr2,   a1,    t2
    vldx             vr3,   a1,    t3
    add.d            a1,    a1,    t4
    xvpermi.q        xr1,   xr0,   0x20
    xvpermi.q        xr3,   xr2,   0x20
    xvshuf.b         xr1,   xr1,   xr1,   xr7
    xvshuf.b         xr3,   xr3,   xr3,   xr7
    xvmulwev.h.bu.b  xr0,   xr1,   xr22
    xvmulwev.h.bu.b  xr2,   xr3,   xr22
    xvmaddwod.h.bu.b xr0,   xr1,   xr22
    xvmaddwod.h.bu.b xr2,   xr3,   xr22
    xvhaddw.w.h      xr0,   xr0,   xr0
    xvhaddw.w.h      xr2,   xr2,   xr2
    xvssrarni.h.w    xr2,   xr0,   2
    xvpermi.d        xr2,   xr2,   0xd8
    xvst             xr2,   a0,    0
    addi.d           a0,    a0,    32
    addi.w           a4,    a4,    -4
    bnez             a4,    .l_\lable\()h_4w_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()h_8w_lasx:
    la.local         t7,    subpel_h_shuf1
    vld              vr6,   t7,    0
    vbsrl.v          vr23,  vr22,  4 //fh
    xvreplve0.w      xr23,  xr23
    xvreplve0.w      xr22,  xr22
    xvreplve0.q      xr19,  xr6
    xvaddi.bu        xr20,  xr19,  4
    xvaddi.bu        xr21,  xr19,  8
.l_\lable\()h_8w_loop_lasx:
    xvld             xr0,   a1,    0
    xvldx            xr1,   a1,    a2
    add.d            a1,    a1,    t2
    xvpermi.q        xr0,   xr1,   0x02
    PREP_HV_8W_LASX  xr0
    xvst             xr0,   a0,    0
    addi.d           a0,    a0,    32
    addi.d           a4,    a4,   -2
    bnez             a4,    .l_\lable\()h_8w_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()h_16w_lasx:
    la.local         t7,    subpel_h_shuf1
    vld              vr6,   t7,    0
    vbsrl.v          vr23,  vr22,  4 //fh
    xvreplve0.w      xr23,  xr23
    xvreplve0.w      xr22,  xr22
    xvreplve0.q      xr19,  xr6
    xvaddi.bu        xr20,  xr19,  4
    xvaddi.bu        xr21,  xr19,  8
.l_\lable\()h_16w_loop_lasx:
    xvld             xr0,   a1,    0
    xvld             xr1,   a1,    8
    add.d            a1,    a1,    a2
    xvpermi.q        xr0,   xr1,   0x02
    PREP_HV_8W_LASX  xr0
    xvst             xr0,   a0,    0
    xvld             xr0,   a1,    0
    xvld             xr1,   a1,    8
    add.d            a1,    a1,    a2
    xvpermi.q        xr0,   xr1,   0x02
    PREP_HV_8W_LASX  xr0
    xvst             xr0,   a0,    32
    addi.d           a0,    a0,    64
    addi.w           a4,    a4,    -2
    bnez             a4,     .l_\lable\()h_16w_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()h_32w_lasx:
.l_\lable\()h_64w_lasx:
.l_\lable\()h_128w_lasx:
    la.local         t7,    subpel_h_shuf1
    vld              vr6,   t7,    0
    vbsrl.v          vr23,  vr22,  4 //fh
    xvreplve0.w      xr23,  xr23
    xvreplve0.w      xr22,  xr22
    xvreplve0.q      xr19,  xr6
    xvaddi.bu        xr20,  xr19,  4
    xvaddi.bu        xr21,  xr19,  8
    addi.d           t5,    a1,    0 //src
    addi.d           t6,    a3,    0 //w
    slli.w           t7,    a3,    1 //store offset
    addi.d           t8,    a0,    0 //dst
.l_\lable\()h_16_loop_lasx:
    xvld             xr0,   a1,    0
    xvld             xr1,   a1,    8
    xvpermi.q        xr0,   xr1,   0x02
    PREP_HV_8W_LASX  xr0
    xvst             xr0,   a0,    0
    xvld             xr0,   a1,    16
    xvld             xr1,   a1,    24
    xvpermi.q        xr0,   xr1,   0x02
    PREP_HV_8W_LASX  xr0
    xvst             xr0,   a0,    32
    addi.d           a0,    a0,    64
    addi.d           a1,    a1,    32
    addi.d           a3,    a3,   -32
    bnez             a3,    .l_\lable\()h_16_loop_lasx
    add.d            a1,    t5,    a2
    add.d            t5,    t5,    a2
    add.d            a0,    t8,    t7
    add.d            t8,    t8,    t7
    addi.d           a3,    t6,    0
    addi.d           a4,    a4,    -1
    bnez             a4,    .l_\lable\()h_16_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()hv_lasx:
    andi             t1,    a7,    3
    blt              t0,    a3,    .l_\lable\()hv_idx_fh_lasx
    andi             t1,    a7,    1
    addi.w           t1,    t1,    3
.l_\lable\()hv_idx_fh_lasx:
    addi.w           t5,    zero,  120
    mul.w            t1,    t1,    t5
    addi.w           t5,    a5,    -1
    slli.w           t5,    t5,    3
    add.w            t1,    t1,    t5
    add.d            t1,    t6,    t1 //fh's offset
    xvldrepl.d       xr22,  t1,    0
    srli.w           a7,    a7,    2
    blt              t0,    a4,    .l_\lable\()hv_idx_fv_lasx
    andi             a7,    a7,    1
    addi.w           a7,    a7,    3
.l_\lable\()hv_idx_fv_lasx:
    addi.w           t5,    zero,  120
    mul.w            a7,    a7,    t5
    addi.w           t5,    a6,    -1
    slli.w           t5,    t5,    3
    add.w            a7,    a7,    t5
    add.d            a7,    t6,    a7 //fv's offset
    xvldrepl.d       xr8,   a7,    0
    xvsllwil.h.b     xr8,   xr8,   0
    sub.d            a1,    a1,    t3
    addi.d           a1,    a1,    -1 //ignore leading 0s
    beq              a3,    t0,    .l_\lable\()hv_4w_lasx
    addi.d           a1,    a1,    -2
    b                .l_\lable\()hv_8w_lasx
.l_\lable\()hv_4w_lasx:
    xvld             xr0,   a1,    0
    xvldx            xr1,   a1,    a2
    xvldx            xr2,   a1,    t2
    xvldx            xr3,   a1,    t3
    add.d            a1,    a1,    t4
    xvld             xr4,   a1,    0
    xvldx            xr5,   a1,    a2
    xvldx            xr6,   a1,    t2
    la.local         t1,    subpel_h_shuf2
    xvld             xr7,   t1,    0
    vbsrl.v          vr22,  vr22,  2
    xvreplve0.w      xr22,  xr22
    xvreplve0.q      xr8,   xr8
    xvrepl128vei.w   xr12,  xr8,   0
    xvrepl128vei.w   xr13,  xr8,   1
    xvrepl128vei.w   xr14,  xr8,   2
    xvrepl128vei.w   xr15,  xr8,   3
    xvilvl.d         xr0,   xr1,   xr0
    xvilvl.d         xr2,   xr3,   xr2
    xvilvl.d         xr4,   xr5,   xr4
    xvreplve0.q      xr0,   xr0
    xvreplve0.q      xr2,   xr2
    xvreplve0.q      xr4,   xr4
    xvreplve0.q      xr6,   xr6
    xvshuf.b         xr0,   xr0,   xr0,   xr7
    xvshuf.b         xr2,   xr2,   xr2,   xr7
    xvshuf.b         xr4,   xr4,   xr4,   xr7
    xvshuf.b         xr6,   xr6,   xr6,   xr7
    xvmulwev.h.bu.b  xr1,   xr0,   xr22
    xvmulwev.h.bu.b  xr3,   xr2,   xr22
    xvmulwev.h.bu.b  xr5,   xr4,   xr22
    xvmulwev.h.bu.b  xr9,   xr6,   xr22
    xvmaddwod.h.bu.b xr1,   xr0,   xr22
    xvmaddwod.h.bu.b xr3,   xr2,   xr22
    xvmaddwod.h.bu.b xr5,   xr4,   xr22
    xvmaddwod.h.bu.b xr9,   xr6,   xr22
    xvhaddw.w.h      xr1,   xr1,   xr1  // a0 b0 a1 b1  c0 d0 c1 d1
    xvhaddw.w.h      xr3,   xr3,   xr3  // a2 b2 a3 b3  c2 d2 c3 d3
    xvhaddw.w.h      xr5,   xr5,   xr5  // a4 b4 a5 b5  c4 d4 c5 d5
    xvhaddw.w.h      xr9,   xr9,   xr9  // a6 b6 -  -   c6 d6 -  -
    xvssrarni.h.w    xr3,   xr1,   2    // a0 b0 a1 b1  a2 b2 a3 b3  c0 d0 c1 d1  c2 d2 c3 d3
    xvssrarni.h.w    xr9,   xr5,   2    // a4 b4 a5 b5  a6 b6 -  -   c4 d4 c5 d5  c6 d6 -  -
    xvbsrl.v         xr4,   xr3,   4
    xvextrins.w      xr4,   xr9,   0x30 // a1 b1 a2 b2  a3 b3 a4 b4  c1 d1 c2 d2  c3 d3 c4 d4
    xvilvl.h         xr5,   xr4,   xr3  // a0 a1 b0 b1  a1 a2 b1 b2  c0 c1 d0 d1  c1 c2 d1 d2
    xvilvh.h         xr6,   xr4,   xr3  // a2 a3 b2 b3  a3 a4 b3 b4  c2 c3 d2 d3  c3 c4 d3 d4
    xvbsrl.v         xr10,  xr9,   4    // a5 b5 a6 b6  -  -  -  -   c5 d5 c6 d6  -  -  -  -
    xvilvl.h         xr11,  xr10,  xr9  // a4 a5 b4 b5  a5 a6 b5 b6  c4 c5 d4 d5  c5 c6 d5 d6
.l_\lable\()hv_w4_loop_lasx:
    xvmulwev.w.h     xr16,  xr5,   xr12 //a0 a1 (h0)
    xvmulwev.w.h     xr17,  xr6,   xr12 //a2 a3 (h1)
    xvmulwev.w.h     xr18,  xr6,   xr13 //a2 a3 (h0)
    xvmulwev.w.h     xr19,  xr11,  xr13 //a4 a5 (h1)
    xvmulwev.w.h     xr20,  xr11,  xr14 //a4 a5 (h0)
    xvmaddwod.w.h    xr16,  xr5,   xr12 //
    xvmaddwod.w.h    xr17,  xr6,   xr12 //
    xvmaddwod.w.h    xr18,  xr6,   xr13 //
    xvmaddwod.w.h    xr19,  xr11,  xr13 //
    xvmaddwod.w.h    xr20,  xr11,  xr14 //
    xvaddi.wu        xr5,   xr11,   0
    xvadd.w          xr16,  xr16,  xr18 //a0 a1 + a2 a3
    xvldx            xr18,  a1,    t3   //a7 b7 c7 d7
    add.d            a1,    a1,    t4
    xvadd.w          xr17,  xr17,  xr19 //a2 a3 + a4 a5
    xvld             xr19,  a1,    0    //a8 b8 c8 d8
    xvadd.w          xr16,  xr16,  xr20 //a0 a1 + a2 a3 + a4 a5
    xvldx            xr20,  a1,    a2   //a9 b9 c9 d9
    xvilvl.d         xr18,  xr19,  xr18
    xvreplve0.q      xr18,  xr18
    xvldx            xr19,  a1,    t2   //aa ba ca da
    xvilvl.d         xr20,  xr19,  xr20
    xvreplve0.q      xr20,  xr20
    xvshuf.b         xr18,  xr18,  xr18,  xr7
    xvshuf.b         xr20,  xr20,  xr20,  xr7
    xvmulwev.h.bu.b  xr21,  xr18,  xr22
    xvmulwev.h.bu.b  xr23,  xr20,  xr22
    xvmaddwod.h.bu.b xr21,  xr18,  xr22
    xvmaddwod.h.bu.b xr23,  xr20,  xr22
    xvhaddw.w.h      xr21,  xr21,  xr21 //a7 b7 a8 b8 c7 d7 c8 d8
    xvhaddw.w.h      xr23,  xr23,  xr23 //a9 b9 aa ba c9 d9 ca da
    xvssrarni.h.w    xr23,  xr21,  2    //a7 b7 a8 b8  a9 b9 aa ba  c7 d7 c8 d8  c9 d9 ca da
    xvbsll.v         xr0,   xr23,  4
    xvextrins.w      xr0,   xr9,   0x02 //a6 b6 a7 b7  a8 b8 a9 b9  c6 d6 c7 d7  c8 d8 c9 d9
    xvilvl.h         xr6,   xr23,  xr0  //a6 a7 b6 b7  a7 a8 b7 b8  c6 c7 d6 d7  c7 c8 d7 d8
    xvilvh.h         xr11,  xr23,  xr0  //a8 a9 b8 b9  a9 aa b9 ba  c8 c9 d8 d9  c9 ca d9 da
    xvbsrl.v         xr9,   xr23,  4
    xvmulwev.w.h     xr1 ,  xr6,   xr14 //a6 a7 (h0)
    xvmulwev.w.h     xr2 ,  xr6,   xr15 //a6 a7 (h1)
    xvmulwev.w.h     xr3 ,  xr11,  xr15 //a8 a9 (h1)
    xvmaddwod.w.h    xr1 ,  xr6,   xr14
    xvmaddwod.w.h    xr2 ,  xr6,   xr15
    xvmaddwod.w.h    xr3 ,  xr11,  xr15
    xvadd.w          xr17,  xr17,  xr1  //a2 a3 + a4 a5 + a6 a7
    xvadd.w          xr16,  xr16,  xr2  //a0 a1 + a2 a3 + a4 a5 + a6 a7
    xvadd.w          xr17,  xr17,  xr3  //a2 a3 + a4 a5 + a6 a7 + a8 a9
    xvssrarni.h.w    xr17,  xr16,  6    //a01 b01 a12 b12  a23 b23 a34 b34  c01 d01 c12 d12  c23 d23 c34 d34
    xvpermi.d        xr17,  xr17,  0xd8 //a01 b01 a12 b12  c01 d01 c12 d12  a23 b23 a34 b34  c23 d23 c34 d34
    xvshuf4i.w       xr17,  xr17,  0xd8
    xvst             xr17,  a0,    0
    addi.d           a0,    a0,    32
    addi.d           a4,    a4,    -4
    bnez             a4,    .l_\lable\()hv_w4_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()hv_8w_lasx:
    addi.d           sp,    sp,   -4*8
    fst.d            f24,   sp,    0
    fst.d            f25,   sp,    8
    fst.d            f26,   sp,    16
    fst.d            f27,   sp,    24
    la.local         t1,    subpel_h_shuf1
    vld              vr19,  t1,    0
    addi.d           t0,    a1,    0
    addi.d           t5,    a4,    0
    slli.w           t7,    a3,    1 // store offset
    addi.d           t8,    a0,    0
    xvreplve0.q      xr19,  xr19
    xvaddi.bu        xr20,  xr19,  4
    xvaddi.bu        xr21,  xr19,  8
    vbsrl.v          vr23,  vr22,  4
    xvreplve0.w      xr22,  xr22 //f0f1f2f3
    xvreplve0.w      xr23,  xr23 //f4f5f6f7
    xvreplve0.q      xr8,   xr8
    xvrepl128vei.w   xr24,  xr8,   0
    xvrepl128vei.w   xr25,  xr8,   1
    xvrepl128vei.w   xr26,  xr8,   2
    xvrepl128vei.w   xr27,  xr8,   3
.l_\lable\()hv_8w_loop0_lasx:
    xvld             xr0,   a1,    0
    xvldx            xr1,   a1,    a2
    xvldx            xr2,   a1,    t2
    add.d            a1,    a1,    t3
    xvld             xr3,   a1,    0
    xvldx            xr4,   a1,    a2
    xvldx            xr5,   a1,    t2
    xvldx            xr6,   a1,    t3
    add.d            a1,    a1,    t4
    xvpermi.q        xr0,   xr3,   0x02 //0 3
    xvpermi.q        xr1,   xr4,   0x02 //1 4
    xvpermi.q        xr2,   xr5,   0x02 //2 5
    xvpermi.q        xr3,   xr6,   0x02 //3 6
    PREP_HV_8W_LASX  xr0 //a0b0c0d0 e0f0g0h0 a3b3c3d3 e3f3g3h3
    PREP_HV_8W_LASX  xr1 //a1b1c1d1 e1f1g1h1 a4b4c4d4 e4f4g4h4
    PREP_HV_8W_LASX  xr2 //a2b2c2d2 e2f2g2h2 a5b5c5d5 e5f5g5h5
    PREP_HV_8W_LASX  xr3 //a3b3c3d3 e3f3g3h3 a6b6c6d6 e6f6g6h6
    xvpermi.d        xr0,   xr0,   0xd8
    xvpermi.d        xr1,   xr1,   0xd8
    xvpermi.d        xr2,   xr2,   0xd8
    xvpermi.d        xr18,  xr3,   0xd8
    xvilvl.h         xr12,  xr1,   xr0 //a0a1b0b1c0c1d0d1 e0e1f0f1g0g1h0h1
    xvilvh.h         xr13,  xr1,   xr0 //a3a4b3b4c3c4d3d4 e3e4f3f4g3g4h3h4
    xvilvl.h         xr14,  xr2,   xr1 //a1a2b1b2c1c2d1d2 e1e2f1f2g1g2h1h2
    xvilvh.h         xr15,  xr2,   xr1 //a4a5b4b5c4c5d4d5 e4e5f4f5g4g5h4h5
    xvilvl.h         xr16,  xr18,  xr2 //a2a3b2b3c2c3d2d3 e2e3f2f3g2g3h2h3
    xvilvh.h         xr17,  xr18,  xr2 //a5a6b5b6c5c6d5d6 e5e6f5f6g5g6h5h6
.l_\lable\()hv_8w_loop_lasx:
    xvld             xr0,   a1,    0
    xvldx            xr1,   a1,    a2
    add.d            a1,    a1,    t2
    xvpermi.q        xr0,   xr1,   0x02 //7 8
    PREP_HV_8W_LASX  xr0                //a7b7c7d7e7f7g7h7 a8b8c8d8e8f8g8h8
    xvpermi.q        xr3,   xr0,   0x03 //a6b6c6d6e6f6g6h6 a7b7c7d7e7f7g7h7
    xvpermi.d        xr3,   xr3,   0xd8 //a6b6c6d6a7b7c7d7 e6f6g6h6e7f7g7h7
    xvpermi.d        xr1,   xr0,   0xd8 //a7b7c7d7a8b8c8d8 e7f7g7h7e8f8g8h8
    xvilvl.h         xr18,  xr1,   xr3  //a6a7b6b7c6c7d6d7 e6e7f6f7g6g7h6h7
    xvilvh.h         xr2,   xr1,   xr3  //a7a8b7b8c7c8d7d8 e7e8f7f8g7g8h7h8
    xvaddi.hu        xr3,   xr0,   0
    xvmulwev.w.h     xr4,   xr12,  xr24 //01
    xvmulwev.w.h     xr5,   xr14,  xr24 //12
    xvmulwev.w.h     xr6,   xr16,  xr25 //23
    xvmulwev.w.h     xr7,   xr13,  xr25 //34
    xvmulwev.w.h     xr8,   xr15,  xr26 //45
    xvmulwev.w.h     xr9,   xr17,  xr26 //56
    xvmulwev.w.h     xr10,  xr18,  xr27 //67
    xvmulwev.w.h     xr11,  xr2,   xr27 //78
    xvmaddwod.w.h    xr4,   xr12,  xr24 //01
    xvmaddwod.w.h    xr5,   xr14,  xr24 //12
    xvmaddwod.w.h    xr6,   xr16,  xr25 //23
    xvmaddwod.w.h    xr7,   xr13,  xr25 //34
    xvmaddwod.w.h    xr8,   xr15,  xr26 //45
    xvmaddwod.w.h    xr9,   xr17,  xr26 //56
    xvmaddwod.w.h    xr10,  xr18,  xr27 //67
    xvmaddwod.w.h    xr11,  xr2,   xr27 //78
    xvadd.w          xr4,   xr4,   xr6
    xvadd.w          xr5,   xr5,   xr7
    xvadd.w          xr4,   xr4,   xr8
    xvadd.w          xr5,   xr5,   xr9
    xvadd.w          xr4,   xr4,   xr10
    xvadd.w          xr5,   xr5,   xr11
    xvaddi.hu        xr12,  xr16,  0 //01 <-- 23
    xvaddi.hu        xr14,  xr13,  0 //12 <-- 34
    xvaddi.hu        xr16,  xr15,  0 //23 <-- 45
    xvaddi.hu        xr13,  xr17,  0 //34 <-- 56
    xvaddi.hu        xr15,  xr18,  0 //45 <-- 67
    xvaddi.hu        xr17,  xr2,   0 //56 <-- 78
    xvssrarni.h.w    xr5,   xr4,   6
    xvpermi.d        xr5,   xr5,   0xd8
    vst              vr5,   a0,    0
    xvpermi.q        xr5,   xr5,   0x11
    vstx             vr5,   a0,    t7
    alsl.d           a0,    t7,    a0,  1
    addi.d           a4,    a4,   -2
    bnez             a4,    .l_\lable\()hv_8w_loop_lasx
    addi.d           a1,    t0,    8
    addi.d           t0,    t0,    8
    addi.d           a0,    t8,    16
    addi.d           t8,    t8,    16
    addi.d           a4,    t5,    0
    addi.d           a3,    a3,   -8
    bnez             a3,    .l_\lable\()hv_8w_loop0_lasx
    fld.d            f24,   sp,    0
    fld.d            f25,   sp,    8
    fld.d            f26,   sp,    16
    fld.d            f27,   sp,    24
    addi.d           sp,    sp,    4*8
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()v_lasx:
    srli.w           a7,    a7,    2
    blt              t0,    a4,    .l_\lable\()v_idx_fv_lasx
    andi             a7,    a7,    1
    addi.w           a7,    a7,    3
.l_\lable\()v_idx_fv_lasx:
    addi.w           t5,    zero,  120
    mul.w            a7,    a7,    t5
    addi.w           t5,    a6,    -1
    slli.w           t5,    t5,    3
    add.w            a7,    a7,    t5
    add.d            a7,    t6,    a7 //fv's offset
    xvldrepl.d       xr8,   a7,    0
    xvrepl128vei.h   xr12,  xr8,   0
    xvrepl128vei.h   xr13,  xr8,   1
    xvrepl128vei.h   xr14,  xr8,   2
    xvrepl128vei.h   xr15,  xr8,   3
    sub.d            a1,    a1,    t3
    beq              a3,    t0,    .l_\lable\()v_4w_lasx
    addi.w           t0,    t0,    4
    beq              a3,    t0,    .l_\lable\()v_8w_lasx
    blt              t0,    a3,    .l_\lable\()v_16w_lasx
.l_\lable\()v_4w_lasx:
    la.local         t6,    subpel_h_shuf3
    xvld             xr11,  t6,    0
    fld.s            f0,    a1,    0   //a0b0c0d0
    fldx.s           f1,    a1,    a2  //a1b1c1d1
    fldx.s           f2,    a1,    t2  //a2b2c2d2
    add.d            a1,    a1,    t3
    fld.s            f3,    a1,    0   //a3b3c3d3
    fldx.s           f4,    a1,    a2  //a4b4c4d4
    fldx.s           f5,    a1,    t2  //a5b5c5d5
    fldx.s           f6,    a1,    t3  //a6b6c6d6
    vilvl.w          vr0,   vr1,   vr0 //01
    vilvl.w          vr1,   vr3,   vr2 //23
    vilvl.d          vr0,   vr1,   vr0 //0123
    vilvl.w          vr2,   vr5,   vr4 //45
    vilvl.d          vr1,   vr2,   vr1 //2345
    xvpermi.q        xr0,   xr1,   0x02 //0123 2345
    xvbsrl.v         xr1,   xr0,   4    //123- 345-
    xvpermi.q        xr4,   xr6,   0x02
    xvextrins.w      xr1,   xr4,   0x30 //1234 3456
    xvilvl.b         xr2,   xr1,   xr0  //0112 2334         //a0a1b0b1c0c1d0d1 a1a2b1b2c1c2d1d2 a2a3b2b3c2c3d2d3 a3a4b3b4c3c4d3d4
    xvilvh.b         xr3,   xr1,   xr0  //2334 4556         //a2a3b2b3c2c3d2d3 a3a4b3b4c3c4d3d4 a4a5b4b5c4c5d4d5 a5a6b5b6c5c6d5d6
.l_\lable\()v_4w_loop_lasx:
    add.d            a1,    a1,    t4
    fld.s            f0,    a1,    0  //a7b7c7d7
    fldx.s           f1,    a1,    a2 //a8b8c8d8
    fldx.s           f4,    a1,    t2 //a9b9c9d9
    fldx.s           f5,    a1,    t3 //aabacada
    vilvl.w          vr7,   vr0,   vr6 //67
    vilvl.w          vr10,  vr4,   vr1 //89
    vextrins.w       vr7,   vr1,   0x20//678-
    vextrins.w       vr10,  vr5,   0x20//89a-
    xvpermi.q        xr7,   xr10,  0x02//678- 89a-
    xvshuf.b         xr4,   xr7,   xr7,  xr11 //67 78 89 9a //a6a7b6b7c6c7d6d7 a7a8b7b8c7c8d7d8 a8a9b8b9c8c9d8d9 a9aab9bac9cad9da
    xvpermi.q        xr7,   xr3,   0x11 //4556
    xvpermi.q        xr7,   xr4,   0x02 //45 56 67 78       //a4a5b4b5c4c5d4d5 a5a6b5b6c5c6d5d6 a6a7b6b7c6c7d6d7 a7a8b7b8c7c8d7d8
    xvmulwev.h.bu.b  xr16,  xr2,   xr12
    xvmulwev.h.bu.b  xr17,  xr3,   xr13
    xvmulwev.h.bu.b  xr18,  xr7,   xr14
    xvmulwev.h.bu.b  xr19,  xr4,   xr15
    xvmaddwod.h.bu.b xr16,  xr2,   xr12
    xvmaddwod.h.bu.b xr17,  xr3,   xr13
    xvmaddwod.h.bu.b xr18,  xr7,   xr14
    xvmaddwod.h.bu.b xr19,  xr4,   xr15
    xvadd.h          xr16,  xr16,  xr17
    xvadd.h          xr16,  xr16,  xr18
    xvadd.h          xr16,  xr16,  xr19
    xvsrari.h        xr16,  xr16,  2
    xvaddi.bu        xr2,   xr7,   0
    xvaddi.bu        xr3,   xr4,   0
    xvaddi.bu        xr6,   xr5,   0
    xvst             xr16,  a0,    0
    addi.d           a0,    a0,    32
    addi.w           a4,    a4,   -4
    bnez             a4,    .l_\lable\()v_4w_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()v_8w_lasx:
    fld.d            f0,    a1,    0
    fldx.d           f1,    a1,    a2
    fldx.d           f2,    a1,    t2
    add.d            a1,    a1,    t3
    fld.d            f3,    a1,    0
    fldx.d           f4,    a1,    a2
    fldx.d           f5,    a1,    t2
    fldx.d           f6,    a1,    t3
    xvpermi.q        xr0,   xr1,   0x02
    xvpermi.q        xr1,   xr2,   0x02
    xvilvl.b         xr0,   xr1,   xr0 //01 12
    xvpermi.q        xr2,   xr3,   0x02
    xvpermi.q        xr3,   xr4,   0x02
    xvilvl.b         xr2,   xr3,   xr2 //23 34
    xvpermi.q        xr4,   xr5,   0x02
    xvpermi.q        xr5,   xr6,   0x02
    xvilvl.b         xr4,   xr5,   xr4 //45 56
.l_\lable\()v_8w_loop_lasx:
    add.d            a1,    a1,    t4
    fld.d            f7,    a1,    0   //7
    fldx.d           f10,   a1,    a2  //8
    fldx.d           f11,   a1,    t2  //9
    fldx.d           f18,   a1,    t3  //a
    xvpermi.q        xr6,   xr7,   0x02
    xvpermi.q        xr7,   xr10,  0x02
    xvilvl.b         xr6,   xr7,   xr6  //67 78
    xvpermi.q        xr10,  xr11,  0x02
    xvpermi.q        xr11,  xr18,  0x02
    xvilvl.b         xr10,  xr11,  xr10 //89 9a
    xvmulwev.h.bu.b  xr1,   xr0,   xr12
    xvmulwev.h.bu.b  xr3,   xr2,   xr13
    xvmulwev.h.bu.b  xr5,   xr4,   xr14
    xvmulwev.h.bu.b  xr7,   xr6,   xr15
    xvmulwev.h.bu.b  xr9,   xr2,   xr12
    xvmulwev.h.bu.b  xr11,  xr4,   xr13
    xvmulwev.h.bu.b  xr16,  xr6,   xr14
    xvmulwev.h.bu.b  xr17,  xr10,  xr15
    xvmaddwod.h.bu.b xr1,   xr0,   xr12
    xvmaddwod.h.bu.b xr3,   xr2,   xr13
    xvmaddwod.h.bu.b xr5,   xr4,   xr14
    xvmaddwod.h.bu.b xr7,   xr6,   xr15
    xvmaddwod.h.bu.b xr9,   xr2,   xr12
    xvmaddwod.h.bu.b xr11,  xr4,   xr13
    xvmaddwod.h.bu.b xr16,  xr6,   xr14
    xvmaddwod.h.bu.b xr17,  xr10,  xr15
    xvadd.h          xr1,   xr1,   xr3
    xvadd.h          xr1,   xr1,   xr5
    xvadd.h          xr1,   xr1,   xr7
    xvadd.h          xr9,   xr9,   xr11
    xvadd.h          xr9,   xr9,   xr16
    xvadd.h          xr9,   xr9,   xr17
    xvaddi.bu        xr0,   xr4,   0
    xvaddi.bu        xr2,   xr6,   0
    xvaddi.bu        xr4,   xr10,  0
    xvaddi.bu        xr6,   xr18,  0
    xvsrari.h        xr1,   xr1,   2
    xvsrari.h        xr9,   xr9,   2
    xvst             xr1,   a0,    0
    xvst             xr9,   a0,    32
    addi.d           a0,    a0,    64
    addi.w           a4,    a4,   -4
    bnez             a4,    .l_\lable\()v_8w_loop_lasx
    b                .l_\lable\()end_pre_8tap_lasx

.l_\lable\()v_16w_lasx:
    addi.d           t0,    a0,    0 //dst
    addi.d           t5,    a1,    0 //src
    slli.w           t7,    a3,    1 //w
    addi.d           t8,    a4,    0 //h
.l_\lable\()v_16w_loop0_lasx:
    vld              vr0,   a1,    0
    vldx             vr1,   a1,    a2
    vldx             vr2,   a1,    t2
    add.d            a1,    a1,    t3
    vld              vr3,   a1,    0
    vldx             vr4,   a1,    a2
    vldx             vr5,   a1,    t2
    vldx             vr6,   a1,    t3
    add.d            a1,    a1,    t4
    xvpermi.d        xr0,   xr0,   0xd8
    xvpermi.d        xr1,   xr1,   0xd8
    xvpermi.d        xr2,   xr2,   0xd8
    xvpermi.d        xr3,   xr3,   0xd8
    xvpermi.d        xr4,   xr4,   0xd8
    xvpermi.d        xr5,   xr5,   0xd8
    xvpermi.d        xr6,   xr6,   0xd8
    xvilvl.b         xr0,   xr1,   xr0 //01
    xvilvl.b         xr1,   xr2,   xr1 //12
    xvilvl.b         xr2,   xr3,   xr2 //23
    xvilvl.b         xr3,   xr4,   xr3 //34
    xvilvl.b         xr4,   xr5,   xr4 //45
    xvilvl.b         xr5,   xr6,   xr5 //56
.l_\lable\()v_16w_loop_lasx:
    vld              vr7,   a1,    0   //7
    vldx             vr10,  a1,    a2  //8
    add.d            a1,    a1,    t2
    xvpermi.d        xr7,   xr7,   0xd8
    xvpermi.d        xr10,  xr10,  0xd8
    xvilvl.b         xr6,   xr7,   xr6 //67
    xvilvl.b         xr7,   xr10,  xr7 //78
    xvmulwev.h.bu.b  xr9,   xr0,   xr12
    xvmulwev.h.bu.b  xr11,  xr2,   xr13
    xvmulwev.h.bu.b  xr16,  xr4,   xr14
    xvmulwev.h.bu.b  xr17,  xr6,   xr15
    xvmulwev.h.bu.b  xr18,  xr1,   xr12
    xvmulwev.h.bu.b  xr19,  xr3,   xr13
    xvmulwev.h.bu.b  xr20,  xr5,   xr14
    xvmulwev.h.bu.b  xr21,  xr7,   xr15
    xvmaddwod.h.bu.b xr9,   xr0,   xr12
    xvmaddwod.h.bu.b xr11,  xr2,   xr13
    xvmaddwod.h.bu.b xr16,  xr4,   xr14
    xvmaddwod.h.bu.b xr17,  xr6,   xr15
    xvmaddwod.h.bu.b xr18,  xr1,   xr12
    xvmaddwod.h.bu.b xr19,  xr3,   xr13
    xvmaddwod.h.bu.b xr20,  xr5,   xr14
    xvmaddwod.h.bu.b xr21,  xr7,   xr15
    xvadd.h          xr9,   xr9,   xr11
    xvadd.h          xr9,   xr9,   xr16
    xvadd.h          xr9,   xr9,   xr17
    xvadd.h          xr11,  xr18,  xr19
    xvadd.h          xr11,  xr11,  xr20
    xvadd.h          xr11,  xr11,  xr21
    xvsrari.h        xr9,   xr9,   2
    xvsrari.h        xr11,  xr11,  2
    xvaddi.bu        xr0,   xr2,   0
    xvaddi.bu        xr1,   xr3,   0
    xvaddi.bu        xr2,   xr4,   0
    xvaddi.bu        xr3,   xr5,   0
    xvaddi.bu        xr4,   xr6,   0
    xvaddi.bu        xr5,   xr7,   0
    xvaddi.bu        xr6,   xr10,  0
    xvst             xr9,   a0,    0
    xvstx            xr11,  a0,    t7
    alsl.d           a0,    t7,    a0,  1
    addi.d           a4,    a4,   -2
    bnez             a4,    .l_\lable\()v_16w_loop_lasx
    addi.d           a3,    a3,   -16
    addi.d           a0,    t0,    32
    addi.d           t0,    t0,    32
    addi.d           a1,    t5,    16
    addi.d           t5,    t5,    16
    addi.d           a4,    t8,    0
    bnez             a3,    .l_\lable\()v_16w_loop0_lasx
.l_\lable\()end_pre_8tap_lasx:
.endm

function prep_8tap_regular_8bpc_lasx
    addi.w a7, zero, 0
    PREP_8TAP_8BPC_LASX 0
endfunc

function prep_8tap_smooth_regular_8bpc_lasx
    addi.w a7, zero, 1
    PREP_8TAP_8BPC_LASX 1
endfunc

function prep_8tap_sharp_regular_8bpc_lasx
    addi.w a7, zero, 2
    PREP_8TAP_8BPC_LASX 2
endfunc

function prep_8tap_regular_smooth_8bpc_lasx
    addi.w a7, zero, 4
    PREP_8TAP_8BPC_LASX 4
endfunc

function prep_8tap_smooth_8bpc_lasx
    addi.w a7, zero, 5
    PREP_8TAP_8BPC_LASX 5
endfunc

function prep_8tap_sharp_smooth_8bpc_lasx
    addi.w a7, zero, 6
    PREP_8TAP_8BPC_LASX 6
endfunc

function prep_8tap_regular_sharp_8bpc_lasx
    addi.w a7, zero, 8
    PREP_8TAP_8BPC_LASX 8
endfunc

function prep_8tap_smooth_sharp_8bpc_lasx
    addi.w a7, zero, 9
    PREP_8TAP_8BPC_LASX 9
endfunc

function prep_8tap_sharp_8bpc_lasx
    addi.w a7, zero, 10
    PREP_8TAP_8BPC_LASX 10
endfunc

.macro PREP_8TAP_8BPC_LSX lable
    li.w             t0,     4
    la.local         t6,     dav1d_mc_subpel_filters
    la.local         t7,     shufb1
    vld              vr23,   t7,    0
    slli.d           t2,     a2,    1  //src_stride*2
    add.d            t3,     t2,    a2 //src_stride*3
    slli.d           t4,     t2,    1

    bnez             a5,     .l_\lable\()h_lsx //mx
    bnez             a6,     .l_\lable\()v_lsx

    clz.w            t1,     a3
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()prep_hv0_jtable_lsx
    alsl.d           t1,     t1,    t5,   1
    ld.h             t8,     t1,    0
    add.d            t5,     t5,    t8
    jirl             $r0,    t5,    0
    .align   3
.l_\lable\()prep_hv0_jtable_lsx:
    .hword .l_\lable\()hv0_128w_lsx - .l_\lable\()prep_hv0_jtable_lsx
    .hword .l_\lable\()hv0_64w_lsx  - .l_\lable\()prep_hv0_jtable_lsx
    .hword .l_\lable\()hv0_32w_lsx  - .l_\lable\()prep_hv0_jtable_lsx
    .hword .l_\lable\()hv0_16w_lsx  - .l_\lable\()prep_hv0_jtable_lsx
    .hword .l_\lable\()hv0_8w_lsx   - .l_\lable\()prep_hv0_jtable_lsx
    .hword .l_\lable\()hv0_4w_lsx   - .l_\lable\()prep_hv0_jtable_lsx

.l_\lable\()hv0_4w_lsx:
    fld.s            f0,     a1,    0
    fldx.s           f1,     a1,    a2
    add.d            a1,     a1,    t2
    vilvl.w          vr0,    vr1,   vr0
    vsllwil.hu.bu    vr0,    vr0,   4
    vst              vr0,    a0,    0
    addi.d           a0,     a0,    16
    addi.d           a4,     a4,    -2
    bnez             a4,     .l_\lable\()hv0_4w_lsx
    b                .l_\lable\()end_pre_8tap_lsx
.l_\lable\()hv0_8w_lsx:
    fld.d            f0,     a1,    0
    fldx.d           f1,     a1,    a2
    add.d            a1,     a1,    t2
    vsllwil.hu.bu    vr0,    vr0,   4
    vsllwil.hu.bu    vr1,    vr1,   4
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    addi.d           a0,     a0,    32
    addi.d           a4,     a4,    -2
    bnez             a4,     .l_\lable\()hv0_8w_lsx
    b                .l_\lable\()end_pre_8tap_lsx
.l_\lable\()hv0_16w_lsx:
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    add.d            a1,     a1,    t2
    vsllwil.hu.bu    vr2,    vr0,   4
    vsllwil.hu.bu    vr4,    vr1,   4
    vexth.hu.bu      vr3,    vr0
    vexth.hu.bu      vr5,    vr1
    vslli.h          vr3,    vr3,   4
    vslli.h          vr5,    vr5,   4
    vst              vr2,    a0,    0
    vst              vr3,    a0,    16
    vst              vr4,    a0,    32
    vst              vr5,    a0,    48
    addi.d           a0,     a0,    64
    addi.d           a4,     a4,    -2
    bnez             a4,     .l_\lable\()hv0_16w_lsx
    b                .l_\lable\()end_pre_8tap_lsx
.l_\lable\()hv0_32w_lsx:
.l_\lable\()hv0_64w_lsx:
.l_\lable\()hv0_128w_lsx:
    addi.d           t0,     a1,    0
    addi.d           t5,     a4,    0
    srli.w           t7,     a3,    4
    slli.w           t7,     t7,    5
    addi.d           t8,     a0,    0
.l_\lable\()hv0_16_loop_lsx:
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    add.d            a1,     a1,    t2
    vsllwil.hu.bu    vr2,    vr0,   4
    vsllwil.hu.bu    vr3,    vr1,   4
    vexth.hu.bu      vr0,    vr0
    vexth.hu.bu      vr1,    vr1
    vslli.h          vr0,    vr0,   4
    vslli.h          vr1,    vr1,   4
    vst              vr2,    a0,    0
    vst              vr0,    a0,    16
    add.d            a0,     a0,    t7
    vst              vr3,    a0,    0
    vst              vr1,    a0,    16
    add.d            a0,     a0,    t7
    addi.d           a4,     a4,    -2
    bnez             a4,     .l_\lable\()hv0_16_loop_lsx
    addi.d           a1,     t0,    16
    addi.d           t0,     t0,    16
    addi.d           a0,     t8,    32
    addi.d           t8,     t8,    32
    addi.d           a4,     t5,    0
    addi.d           a3,     a3,    -16
    bnez             a3,     .l_\lable\()hv0_16_loop_lsx
    b                .l_\lable\()end_pre_8tap_lsx
.l_\lable\()h_lsx:
    bnez             a6,     .l_\lable\()hv_lsx //if(fh) && if (fv)

    andi             t1,     a7,    3
    blt              t0,     a3,    .l_\lable\()h_idx_fh_lsx
    andi             t1,     a7,    1
    addi.w           t1,     t1,    3
.l_\lable\()h_idx_fh_lsx:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a5,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fh's offset
    vldrepl.d        vr23,   t1,    0

    addi.d           a1,     a1,    -3
    clz.w            t1,     a3
    li.w             t5,     24
    sub.w            t1,     t1,    t5
    la.local         t5,     .l_\lable\()prep_h_jtable_lsx
    alsl.d           t1,     t1,    t5,   1
    ld.h             t8,     t1,    0
    add.d            t5,     t5,    t8
    jirl             $r0,    t5,    0

    .align   3
.l_\lable\()prep_h_jtable_lsx:
    .hword .l_\lable\()h_128w_lsx - .l_\lable\()prep_h_jtable_lsx
    .hword .l_\lable\()h_64w_lsx  - .l_\lable\()prep_h_jtable_lsx
    .hword .l_\lable\()h_32w_lsx  - .l_\lable\()prep_h_jtable_lsx
    .hword .l_\lable\()h_16w_lsx  - .l_\lable\()prep_h_jtable_lsx
    .hword .l_\lable\()h_8w_lsx   - .l_\lable\()prep_h_jtable_lsx
    .hword .l_\lable\()h_4w_lsx   - .l_\lable\()prep_h_jtable_lsx

.l_\lable\()h_4w_lsx:
    addi.d           a1,     a1,    2
    la.local         t7,     subpel_h_shuf1
    vld              vr7,    t7,    0
    vbsrl.v          vr23,   vr23,  2
    vreplvei.w       vr23,   vr23,  0
.l_\lable\()h_4w_loop_lsx:
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    add.d            a1,     a1,    t2
    vshuf.b          vr0,    vr0,   vr0,   vr7
    vshuf.b          vr1,    vr1,   vr1,   vr7
    vmulwev.h.bu.b   vr2,    vr0,   vr23
    vmulwev.h.bu.b   vr3,    vr1,   vr23
    vmaddwod.h.bu.b  vr2,    vr0,   vr23
    vmaddwod.h.bu.b  vr3,    vr1,   vr23
    vhaddw.w.h       vr0,    vr2,   vr2
    vhaddw.w.h       vr1,    vr3,   vr3
    vssrarni.h.w     vr1,    vr0,   2
    vst              vr1,    a0,    0
    addi.d           a0,     a0,    16
    addi.w           a4,     a4,    -2
    bnez             a4,     .l_\lable\()h_4w_loop_lsx
    b                .l_\lable\()end_pre_8tap_lsx

.l_\lable\()h_8w_lsx:
    vreplvei.w       vr22,   vr23,  0 //fh
    vreplvei.w       vr23,   vr23,  1
    la.local         t7,     subpel_h_shuf1
    vld              vr6,    t7,    0
    vaddi.bu         vr7,    vr6,   4
    vaddi.bu         vr8,    vr6,   8
.l_\lable\()h_8w_loop_lsx:
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    add.d            a1,     a1,    t2
    PREP_H_8W        vr0
    PREP_H_8W        vr1
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    addi.d           a0,     a0,    32
    addi.d           a4,     a4,    -2
    bnez             a4,     .l_\lable\()h_8w_loop_lsx
    b                .l_\lable\()end_pre_8tap_lsx

.l_\lable\()h_16w_lsx:
.l_\lable\()h_32w_lsx:
.l_\lable\()h_64w_lsx:
.l_\lable\()h_128w_lsx:
    vreplvei.w       vr22,   vr23,  0 //fh
    vreplvei.w       vr23,   vr23,  1
    la.local         t7,     subpel_h_shuf1
    vld              vr6,    t7,    0
    vaddi.bu         vr7,    vr6,   4
    vaddi.bu         vr8,    vr6,   8
    srli.w           t7,     a3,    4
    slli.w           t6,     t7,    5
.l_\lable\()h_16w_loop0_lsx:
    addi.d           t0,     a1,    0 //src
    addi.d           t5,     a4,    0 //h
    addi.d           t8,     a0,    0 //dst
.l_\lable\()h_16w_loop_lsx:
    vld              vr0,    a1,    0
    vld              vr1,    a1,    8
    add.d            a1,     a1,    a2
    PREP_H_8W        vr0
    PREP_H_8W        vr1
    vst              vr0,    a0,    0
    vst              vr1,    a0,    16
    add.d            a0,     a0,    t6
    addi.d           t5,     t5,    -1
    bnez             t5,     .l_\lable\()h_16w_loop_lsx
    addi.d           a1,     t0,    16
    addi.d           a0,     t8,    32
    addi.w           t7,     t7,    -1
    bnez             t7,     .l_\lable\()h_16w_loop0_lsx
    b                .l_\lable\()end_pre_8tap_lsx

.l_\lable\()hv_lsx:
    andi             t1,     a7,    3
    blt              t0,     a3,    .l_\lable\()hv_idx_fh_lsx
    andi             t1,     a7,    1
    addi.w           t1,     t1,    3
.l_\lable\()hv_idx_fh_lsx:
    addi.w           t5,     zero,  120
    mul.w            t1,     t1,    t5
    addi.w           t5,     a5,    -1
    slli.w           t5,     t5,    3
    add.w            t1,     t1,    t5
    add.d            t1,     t6,    t1 //fh's offset
    vldrepl.d        vr8,    t1,    0
    srli.w           a7,     a7,    2
    blt              t0,     a4,    .l_\lable\()hv_idx_fv_lsx
    andi             a7,     a7,    1
    addi.w           a7,     a7,    3
.l_\lable\()hv_idx_fv_lsx:
    addi.w           t5,     zero,  120
    mul.w            a7,     a7,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            a7,     a7,    t5
    add.d            a7,     t6,    a7 //fv's offset
    vldrepl.d        vr9,    a7,    0
    vsllwil.h.b      vr9,    vr9,   0

    sub.d            a1,     a1,    t3
    addi.d           a1,     a1,    -3
    beq              a3,     t0,    .l_\lable\()hv_4w_lsx
    b                .l_\lable\()hv_8w_lsx
.l_\lable\()hv_4w_lsx:
    addi.d           a1,     a1,    2 //ignore leading 0s
    vld              vr0,    a1,    0
    vldx             vr1,    a1,    a2
    vldx             vr2,    a1,    t2
    add.d            a1,     a1,    t3
    vld              vr3,    a1,    0
    vldx             vr4,    a1,    a2
    vldx             vr5,    a1,    t2
    vldx             vr6,    a1,    t3
    add.d            a1,     a1,    t4

    la.local         t1,     subpel_h_shuf1
    vld              vr7,    t1,    0
    vbsrl.v          vr8,    vr8,   2
    vreplvei.w       vr8,    vr8,   0

    //fv
    vreplvei.w       vr17,   vr9,   0
    vreplvei.w       vr18,   vr9,   1
    vreplvei.w       vr19,   vr9,   2
    vreplvei.w       vr20,   vr9,   3

    //DAV1D_FILTER_8TAP_RND
    vshuf.b          vr0,    vr0,   vr0,  vr7
    vshuf.b          vr1,    vr1,   vr1,  vr7
    vshuf.b          vr2,    vr2,   vr2,  vr7
    vshuf.b          vr3,    vr3,   vr3,  vr7
    vshuf.b          vr4,    vr4,   vr4,  vr7
    vshuf.b          vr5,    vr5,   vr5,  vr7
    vshuf.b          vr6,    vr6,   vr6,  vr7

    vmulwev.h.bu.b   vr10,   vr0,   vr8
    vmulwev.h.bu.b   vr11,   vr1,   vr8
    vmulwev.h.bu.b   vr12,   vr2,   vr8
    vmulwev.h.bu.b   vr13,   vr3,   vr8
    vmulwev.h.bu.b   vr14,   vr4,   vr8
    vmulwev.h.bu.b   vr15,   vr5,   vr8
    vmulwev.h.bu.b   vr16,   vr6,   vr8
    vmaddwod.h.bu.b  vr10,   vr0,   vr8
    vmaddwod.h.bu.b  vr11,   vr1,   vr8
    vmaddwod.h.bu.b  vr12,   vr2,   vr8
    vmaddwod.h.bu.b  vr13,   vr3,   vr8
    vmaddwod.h.bu.b  vr14,   vr4,   vr8
    vmaddwod.h.bu.b  vr15,   vr5,   vr8
    vmaddwod.h.bu.b  vr16,   vr6,   vr8

    vhaddw.w.h       vr10,   vr10,  vr10
    vhaddw.w.h       vr11,   vr11,  vr11
    vhaddw.w.h       vr12,   vr12,  vr12
    vhaddw.w.h       vr13,   vr13,  vr13
    vhaddw.w.h       vr14,   vr14,  vr14
    vhaddw.w.h       vr15,   vr15,  vr15
    vhaddw.w.h       vr16,   vr16,  vr16

    vssrarni.h.w     vr10,   vr10,  2 //h0
    vssrarni.h.w     vr11,   vr11,  2 //h1
    vssrarni.h.w     vr12,   vr12,  2 //h2
    vssrarni.h.w     vr13,   vr13,  2 //h3
    vssrarni.h.w     vr14,   vr14,  2 //h4
    vssrarni.h.w     vr15,   vr15,  2 //h5
    vssrarni.h.w     vr16,   vr16,  2 //h6

    //h0
    vilvl.h          vr0,    vr11,  vr10 //01
    vilvl.h          vr1,    vr13,  vr12 //23
    vilvl.h          vr2,    vr15,  vr14 //45
    //h1
    vilvl.h          vr4,    vr12,  vr11 //12
    vilvl.h          vr5,    vr14,  vr13 //34
    vilvl.h          vr6,    vr16,  vr15 //56

.l_\lable\()hv_w4_loop_lsx:
    vld              vr9,    a1,    0
    vldx             vr10,   a1,    a2
    add.d            a1,     a1,    t2

    //DAV1D_FILTER_8TAP_CLIP
    vshuf.b          vr9,    vr9,   vr9,  vr7
    vshuf.b          vr10,   vr10,  vr10, vr7
    vmulwev.h.bu.b   vr11,   vr9,   vr8
    vmulwev.h.bu.b   vr12,   vr10,  vr8
    vmaddwod.h.bu.b  vr11,   vr9,   vr8
    vmaddwod.h.bu.b  vr12,   vr10,  vr8
    vhaddw.w.h       vr11,   vr11,  vr11
    vhaddw.w.h       vr12,   vr12,  vr12
    vssrarni.h.w     vr11,   vr11,  2 //7h
    vssrarni.h.w     vr12,   vr12,  2 //h8
    vilvl.h          vr3,    vr11,  vr16 //67
    vilvl.h          vr13,   vr12,  vr11 //78

    vmulwev.w.h      vr9,    vr0,   vr17
    vmulwev.w.h      vr10,   vr1,   vr18
    vmulwev.w.h      vr14,   vr2,   vr19
    vmulwev.w.h      vr15,   vr3,   vr20
    vmaddwod.w.h     vr9,    vr0,   vr17
    vmaddwod.w.h     vr10,   vr1,   vr18
    vmaddwod.w.h     vr14,   vr2,   vr19
    vmaddwod.w.h     vr15,   vr3,   vr20
    vadd.w           vr16,   vr9,   vr10
    vadd.w           vr16,   vr16,  vr14
    vadd.w           vr16,   vr16,  vr15

    vmulwev.w.h      vr9,    vr4,   vr17
    vmulwev.w.h      vr10,   vr5,   vr18
    vmulwev.w.h      vr14,   vr6,   vr19
    vmulwev.w.h      vr15,   vr13,  vr20
    vmaddwod.w.h     vr9,    vr4,   vr17
    vmaddwod.w.h     vr10,   vr5,   vr18
    vmaddwod.w.h     vr14,   vr6,   vr19
    vmaddwod.w.h     vr15,   vr13,  vr20
    vadd.w           vr21,   vr9,   vr10
    vadd.w           vr21,   vr21,  vr14
    vadd.w           vr21,   vr21,  vr15

    vssrarni.h.w     vr21,   vr16,  6
    //cache
    vaddi.hu         vr0,    vr1,   0
    vaddi.hu         vr1,    vr2,   0
    vaddi.hu         vr2,    vr3,   0
    vaddi.hu         vr4,    vr5,   0
    vaddi.hu         vr5,    vr6,   0
    vaddi.hu         vr6,    vr13,  0
    vaddi.hu         vr16,   vr12,  0

    vst              vr21,   a0,    0
    addi.d           a0,     a0,    16
    addi.d           a4,     a4,    -2
    bnez             a4,     .l_\lable\()hv_w4_loop_lsx
    b                .l_\lable\()end_pre_8tap_lsx

.l_\lable\()hv_8w_lsx:
.l_\lable\()hv_16w_lsx:
.l_\lable\()hv_32w_lsx:
.l_\lable\()hv_64w_lsx:
.l_\lable\()hv_128w_lsx:
    addi.d          sp,      sp,    -8*8
    fst.d           f24,     sp,    0
    fst.d           f25,     sp,    8
    fst.d           f26,     sp,    16
    fst.d           f27,     sp,    24
    fst.d           f28,     sp,    32
    fst.d           f29,     sp,    40
    fst.d           f30,     sp,    48
    fst.d           f31,     sp,    56
    addi.d          t0,      a1,    0 //src
    addi.d          t5,      a4,    0 //h
    addi.d          t8,      a0,    0 //dst
    slli.w          t6,      a3,    1
    la.local        t1,      subpel_h_shuf1
    vld             vr7,     t1,    0
    vaddi.bu        vr11,    vr7,   4
    vaddi.bu        vr12,    vr7,   8
    vreplvei.w      vr10,    vr8,   1
    vreplvei.w      vr8,     vr8,   0
    vreplvei.w      vr20,    vr9,   1
    vreplvei.w      vr21,    vr9,   2
    vreplvei.w      vr22,    vr9,   3
    vreplvei.w      vr9,     vr9,   0
.l_\lable\()prep_hv_8w_loop0_lsx:
    vld             vr0,     a1,    0
    vldx            vr1,     a1,    a2
    vldx            vr2,     a1,    t2
    add.d           a1,      a1,    t3
    vld             vr3,     a1,    0
    vldx            vr4,     a1,    a2
    vldx            vr5,     a1,    t2
    vldx            vr6,     a1,    t3
    add.d           a1,      a1,    t4

    FILTER_8TAP_8W  vr0 //h0
    FILTER_8TAP_8W  vr1 //h1
    FILTER_8TAP_8W  vr2 //h2
    FILTER_8TAP_8W  vr3 //h3
    FILTER_8TAP_8W  vr4 //h4
    FILTER_8TAP_8W  vr5 //h5
    FILTER_8TAP_8W  vr6 //h6

    //h0' low part
    vilvl.h         vr23,    vr1,   vr0 //01
    vilvl.h         vr24,    vr3,   vr2 //23
    vilvl.h         vr25,    vr5,   vr4 //45
    //h0' high part
    vilvh.h         vr26,    vr1,   vr0 //01
    vilvh.h         vr27,    vr3,   vr2 //23
    vilvh.h         vr28,    vr5,   vr4 //45

    //h1' low part
    vilvl.h         vr29,    vr2,   vr1 //12
    vilvl.h         vr30,    vr4,   vr3 //34
    vilvl.h         vr31,    vr6,   vr5 //56
    //h1' high part
    vilvh.h         vr0,     vr2,   vr1 //12
    vilvh.h         vr1,     vr4,   vr3 //34
    vilvh.h         vr2,     vr6,   vr5 //56

.l_\lable\()prep_hv_8w_loop_lsx:
    vld             vr3,     a1,    0
    vldx            vr4,     a1,    a2
    add.d           a1,      a1,    t2

    FILTER_8TAP_8W  vr3 //h7
    FILTER_8TAP_8W  vr4 //h8

    //h0' low part
    vilvl.h         vr16,    vr3,   vr6 //67 ~low
    vmulwev.w.h     vr13,    vr23,  vr9
    vmulwev.w.h     vr14,    vr24,  vr20
    vmulwev.w.h     vr15,    vr25,  vr21
    vmulwev.w.h     vr17,    vr16,  vr22
    vmaddwod.w.h    vr13,    vr23,  vr9
    vmaddwod.w.h    vr14,    vr24,  vr20
    vmaddwod.w.h    vr15,    vr25,  vr21
    vmaddwod.w.h    vr17,    vr16,  vr22
    vadd.w          vr13,    vr13,  vr14
    vadd.w          vr13,    vr13,  vr15
    vadd.w          vr13,    vr13,  vr17
    //cache
    vaddi.hu        vr23,    vr24,  0
    vaddi.hu        vr24,    vr25,  0
    vaddi.hu        vr25,    vr16,  0

    //h0' high part
    vilvh.h         vr17,    vr3,   vr6 //67 ~high
    vmulwev.w.h     vr14,    vr26,  vr9
    vmulwev.w.h     vr15,    vr27,  vr20
    vmulwev.w.h     vr16,    vr28,  vr21
    vmulwev.w.h     vr18,    vr17,  vr22
    vmaddwod.w.h    vr14,    vr26,  vr9
    vmaddwod.w.h    vr15,    vr27,  vr20
    vmaddwod.w.h    vr16,    vr28,  vr21
    vmaddwod.w.h    vr18,    vr17,  vr22
    vadd.w          vr14,    vr14,  vr15
    vadd.w          vr14,    vr14,  vr16
    vadd.w          vr14,    vr14,  vr18
    vssrarni.h.w    vr14,    vr13,  6
    vst             vr14,    a0,    0
    add.d           a0,      a0,    t6
    //cache
    vaddi.hu        vr26,    vr27,  0
    vaddi.hu        vr27,    vr28,  0
    vaddi.hu        vr28,    vr17,  0
    vaddi.hu        vr6,     vr4,   0

    vilvl.h         vr5,     vr4,   vr3 //78 ~low
    vilvh.h         vr4,     vr4,   vr3 //78 ~high

    //h1' low part
    vmulwev.w.h     vr13,    vr29,  vr9
    vmulwev.w.h     vr14,    vr30,  vr20
    vmulwev.w.h     vr15,    vr31,  vr21
    vmulwev.w.h     vr16,    vr5,   vr22
    vmaddwod.w.h    vr13,    vr29,  vr9
    vmaddwod.w.h    vr14,    vr30,  vr20
    vmaddwod.w.h    vr15,    vr31,  vr21
    vmaddwod.w.h    vr16,    vr5,   vr22
    vadd.w          vr13,    vr13,  vr14
    vadd.w          vr13,    vr13,  vr15
    vadd.w          vr13,    vr13,  vr16
    //cache
    vaddi.hu        vr29,    vr30,  0
    vaddi.hu        vr30,    vr31,  0
    vaddi.hu        vr31,    vr5,   0

    //h1' high part
    vmulwev.w.h     vr14,    vr0,   vr9
    vmulwev.w.h     vr15,    vr1,   vr20
    vmulwev.w.h     vr16,    vr2,   vr21
    vmulwev.w.h     vr17,    vr4,   vr22
    vmaddwod.w.h    vr14,    vr0,   vr9
    vmaddwod.w.h    vr15,    vr1,   vr20
    vmaddwod.w.h    vr16,    vr2,   vr21
    vmaddwod.w.h    vr17,    vr4,   vr22
    vadd.w          vr14,    vr14,  vr15
    vadd.w          vr14,    vr14,  vr16
    vadd.w          vr14,    vr14,  vr17
    vssrarni.h.w    vr14,    vr13,  6
    vst             vr14,    a0,    0
    add.d           a0,      a0,    t6
    //cache
    vaddi.hu        vr0,     vr1,   0
    vaddi.hu        vr1,     vr2,   0
    vaddi.hu        vr2,     vr4,   0
    addi.w          a4,      a4,    -2
    bnez            a4,      .l_\lable\()prep_hv_8w_loop_lsx
    addi.d          a1,      t0,    8
    addi.d          t0,      t0,    8
    addi.d          a0,      t8,    16
    addi.d          t8,      t8,    16
    addi.d          a4,      t5,    0
    addi.w          a3,      a3,    -8
    bnez            a3,      .l_\lable\()prep_hv_8w_loop0_lsx
    fld.d           f24,     sp,    0
    fld.d           f25,     sp,    8
    fld.d           f26,     sp,    16
    fld.d           f27,     sp,    24
    fld.d           f28,     sp,    32
    fld.d           f29,     sp,    40
    fld.d           f30,     sp,    48
    fld.d           f31,     sp,    56
    addi.d          sp,      sp,    8*8
    b                .l_\lable\()end_pre_8tap_lsx

.l_\lable\()v_lsx:
    srli.w           a7,    a7,     2
    blt              t0,    a4,     .l_\lable\()v_idx_fv_lsx
    andi             a7,    a7,     1
    addi.w           a7,    a7,     3
.l_\lable\()v_idx_fv_lsx:
    addi.w           t5,     zero,  120
    mul.w            a7,     a7,    t5
    addi.w           t5,     a6,    -1
    slli.w           t5,     t5,    3
    add.w            a7,     a7,    t5
    add.d            a7,     t6,    a7 //fv's offset
    vldrepl.d        vr8,    a7,    0

    vilvl.h          vr8,    vr8,   vr8
    vreplvei.w       vr9,    vr8,   1
    vreplvei.w       vr10,   vr8,   2
    vreplvei.w       vr11,   vr8,   3
    vreplvei.w       vr8,    vr8,   0

    sub.d            a1,     a1,    t3
    beq              a3,     t0,    .l_\lable\()v_4w_lsx
    blt              t0,     a3,    .l_\lable\()v_8w_lsx
.l_\lable\()v_4w_lsx:
    fld.s            f0,     a1,    0
    fldx.s           f1,     a1,    a2
    fldx.s           f2,     a1,    t2
    add.d            a1,     a1,    t3
    fld.s            f3,     a1,    0
    fldx.s           f4,     a1,    a2
    fldx.s           f5,     a1,    t2
    fldx.s           f6,     a1,    t3
    add.d            a1,     a1,    t4

    vilvl.w          vr0,    vr1,   vr0
    vilvl.w          vr1,    vr2,   vr1
    vilvl.b          vr0,    vr1,   vr0 //0 1 1 2
    vilvl.w          vr1,    vr3,   vr2
    vilvl.w          vr2,    vr4,   vr3
    vilvl.b          vr1,    vr2,   vr1 //2 3 3 4
    vilvl.w          vr2,    vr5,   vr4
    vilvl.w          vr3,    vr6,   vr5
    vilvl.b          vr2,    vr3,   vr2 //4 5 5 6
.l_\lable\()v_4w_loop_lsx:
    fld.s            f7,     a1,     0

    vilvl.w          vr3,    vr7,   vr6
    fldx.s           f6,     a1,    a2
    add.d            a1,     a1,    t2
    vilvl.w          vr4,    vr6,   vr7
    vilvl.b          vr3,    vr4,   vr3 //6 7 7 8

    vmulwev.h.bu.b   vr12,   vr0,   vr8
    vmulwev.h.bu.b   vr13,   vr1,   vr9
    vmulwev.h.bu.b   vr14,   vr2,   vr10
    vmulwev.h.bu.b   vr15,   vr3,   vr11
    vmaddwod.h.bu.b  vr12,   vr0,   vr8
    vmaddwod.h.bu.b  vr13,   vr1,   vr9
    vmaddwod.h.bu.b  vr14,   vr2,   vr10
    vmaddwod.h.bu.b  vr15,   vr3,   vr11
    vaddi.hu         vr0,    vr1,   0
    vaddi.hu         vr1,    vr2,   0
    vaddi.hu         vr2,    vr3,   0
    vadd.h           vr12,   vr12,  vr13
    vadd.h           vr12,   vr12,  vr14
    vadd.h           vr12,   vr12,  vr15

    vsrari.h         vr12,   vr12,  2
    vst              vr12,   a0,    0
    addi.d           a0,     a0,    16
    addi.w           a4,     a4,    -2
    bnez             a4,     .l_\lable\()v_4w_loop_lsx
    b                .l_\lable\()end_pre_8tap_lsx

.l_\lable\()v_8w_lsx:
    addi.d           t0,     a1,    0
    addi.d           t5,     a4,    0
    addi.d           t8,     a0,    0
    slli.w           t6,     a3,    1
.l_\lable\()v_8w_loop0_lsx:
    fld.d            f0,     a1,    0
    fldx.d           f1,     a1,    a2
    fldx.d           f2,     a1,    t2
    add.d            a1,     a1,    t3
    fld.d            f3,     a1,    0
    fldx.d           f4,     a1,    a2
    fldx.d           f5,     a1,    t2
    fldx.d           f6,     a1,    t3
    add.d            a1,     a1,    t4

    vilvl.b          vr0,    vr1,   vr0 //0 1
    vilvl.b          vr1,    vr2,   vr1 //1 2
    vilvl.b          vr2,    vr3,   vr2 //2 3
    vilvl.b          vr3,    vr4,   vr3 //3 4
    vilvl.b          vr4,    vr5,   vr4 //4 5
    vilvl.b          vr5,    vr6,   vr5 //5 6
.l_\lable\()v_8w_loop_lsx:
    fld.d            f7,     a1,    0
    vilvl.b          vr12,   vr7,   vr6 //6 7
    fldx.d           f6,     a1,    a2
    add.d            a1,     a1,    t2
    vilvl.b          vr13,   vr6,   vr7 //7 8

    vmulwev.h.bu.b   vr14,   vr0,   vr8
    vmulwev.h.bu.b   vr15,   vr1,   vr8
    vmulwev.h.bu.b   vr16,   vr2,   vr9
    vmulwev.h.bu.b   vr17,   vr3,   vr9
    vmulwev.h.bu.b   vr18,   vr4,   vr10
    vmulwev.h.bu.b   vr19,   vr5,   vr10
    vmulwev.h.bu.b   vr20,   vr12,  vr11
    vmulwev.h.bu.b   vr21,   vr13,  vr11
    vmaddwod.h.bu.b  vr14,   vr0,   vr8
    vmaddwod.h.bu.b  vr15,   vr1,   vr8
    vmaddwod.h.bu.b  vr16,   vr2,   vr9
    vmaddwod.h.bu.b  vr17,   vr3,   vr9
    vmaddwod.h.bu.b  vr18,   vr4,   vr10
    vmaddwod.h.bu.b  vr19,   vr5,   vr10
    vmaddwod.h.bu.b  vr20,   vr12,  vr11
    vmaddwod.h.bu.b  vr21,   vr13,  vr11

    vaddi.hu         vr0,    vr2,   0
    vaddi.hu         vr1,    vr3,   0
    vaddi.hu         vr2,    vr4,   0
    vaddi.hu         vr3,    vr5,   0
    vaddi.hu         vr4,    vr12,  0
    vaddi.hu         vr5,    vr13,  0
    vadd.h           vr14,   vr14,  vr16
    vadd.h           vr14,   vr14,  vr18
    vadd.h           vr14,   vr14,  vr20
    vadd.h           vr15,   vr15,  vr17
    vadd.h           vr15,   vr15,  vr19
    vadd.h           vr15,   vr15,  vr21

    vsrari.h         vr14,   vr14,  2
    vsrari.h         vr15,   vr15,  2
    vst              vr14,   a0,    0
    add.d            a0,     a0,    t6
    vst              vr15,   a0,    0
    add.d            a0,     a0,    t6
    addi.w           a4,     a4,    -2
    bnez             a4,     .l_\lable\()v_8w_loop_lsx
    addi.d           a1,     t0,    8
    addi.d           t0,     t0,    8
    addi.d           a0,     t8,    16
    addi.d           t8,     t8,    16
    addi.d           a4,     t5,    0
    addi.d           a3,     a3,    -8
    bnez             a3,     .l_\lable\()v_8w_loop0_lsx
.l_\lable\()end_pre_8tap_lsx:
.endm

function prep_8tap_regular_8bpc_lsx
    addi.w a7, zero, 0
    PREP_8TAP_8BPC_LSX 0
endfunc

function prep_8tap_smooth_regular_8bpc_lsx
    addi.w a7, zero, 1
    PREP_8TAP_8BPC_LSX 1
endfunc

function prep_8tap_sharp_regular_8bpc_lsx
    addi.w a7, zero, 2
    PREP_8TAP_8BPC_LSX 2
endfunc

function prep_8tap_regular_smooth_8bpc_lsx
    addi.w a7, zero, 4
    PREP_8TAP_8BPC_LSX 4
endfunc

function prep_8tap_smooth_8bpc_lsx
    addi.w a7, zero, 5
    PREP_8TAP_8BPC_LSX 5
endfunc

function prep_8tap_sharp_smooth_8bpc_lsx
    addi.w a7, zero, 6
    PREP_8TAP_8BPC_LSX 6
endfunc

function prep_8tap_regular_sharp_8bpc_lsx
    addi.w a7, zero, 8
    PREP_8TAP_8BPC_LSX 8
endfunc

function prep_8tap_smooth_sharp_8bpc_lsx
    addi.w a7, zero, 9
    PREP_8TAP_8BPC_LSX 9
endfunc

function prep_8tap_sharp_8bpc_lsx
    addi.w a7, zero, 10
    PREP_8TAP_8BPC_LSX 10
endfunc

/*
 * static void blend_lsx(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                         const int w, int h, const uint8_t *mask)
 */
function blend_8bpc_lsx
    addi.d        t8,     zero,    64
    vreplgr2vr.b  vr23,   t8

    clz.w         t0,     a3
    li.w          t1,     26
    sub.w         t0,     t0,      t1
    la.local      t1,     .BLEND_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0  // The jump addresses are relative to JRTABLE
    add.d         t1,     t1,      t2 // Get absolute address
    jirl          $r0,    t1,      0

    .align   3
.BLEND_LSX_JRTABLE:
    .hword .BLEND_W32_LSX  - .BLEND_LSX_JRTABLE
    .hword .BLEND_W16_LSX  - .BLEND_LSX_JRTABLE
    .hword .BLEND_W8_LSX   - .BLEND_LSX_JRTABLE
    .hword .BLEND_W4_LSX   - .BLEND_LSX_JRTABLE

.BLEND_W4_LSX:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0
    vld             vr2,    a5,      0

    vsllwil.hu.bu   vr1,    vr1,     0
    vsllwil.hu.bu   vr4,    vr2,     0
    vmul.h          vr1,    vr1,     vr4  //b*m
    vsub.b          vr3,    vr23,    vr2
    vsllwil.hu.bu   vr0,    vr0,     0
    vsllwil.hu.bu   vr3,    vr3,     0
    vmadd.h         vr1,    vr0,     vr3
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.w        vr1,    a0,      0,   0
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      4
    addi.d          a5,     a5,      4

    blt             zero,   a4,     .BLEND_W4_LSX
    b              .BLEND_END_LSX
.BLEND_W8_LSX:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0
    vld             vr2,    a5,      0

    vsllwil.hu.bu   vr1,    vr1,     0
    vsllwil.hu.bu   vr4,    vr2,     0
    vmul.h          vr1,    vr1,     vr4  //b*m
    vsub.b          vr3,    vr23,    vr2
    vsllwil.hu.bu   vr0,    vr0,     0
    vsllwil.hu.bu   vr3,    vr3,     0
    vmadd.h         vr1,    vr0,     vr3
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.d        vr1,    a0,      0,   0
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      8
    addi.d          a5,     a5,      8

    blt             zero,   a4,     .BLEND_W8_LSX
    b               .BLEND_END_LSX
.BLEND_W16_LSX:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0
    vld             vr2,    a5,      0

    vexth.hu.bu     vr5,    vr1
    vsllwil.hu.bu   vr1,    vr1,     0
    vexth.hu.bu     vr6,    vr2
    vsllwil.hu.bu   vr4,    vr2,     0
    vmul.h          vr1,    vr1,     vr4  //b*m
    vmul.h          vr5,    vr5,     vr6  //b*m
    vsub.b          vr3,    vr23,    vr2
    vexth.hu.bu     vr7,    vr0
    vexth.hu.bu     vr8,    vr3
    vmadd.h         vr5,    vr7,     vr8
    vsllwil.hu.bu   vr0,    vr0,     0
    vsllwil.hu.bu   vr3,    vr3,     0
    vmadd.h         vr1,    vr0,     vr3
    vssrarni.bu.h   vr5,    vr1,     6

    vst             vr5,    a0,      0
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      16
    addi.d          a5,     a5,      16

    blt             zero,   a4,     .BLEND_W16_LSX
    b               .BLEND_END_LSX
.BLEND_W32_LSX:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0
    vld             vr2,    a5,      0

    vexth.hu.bu     vr5,    vr1
    vsllwil.hu.bu   vr1,    vr1,     0
    vexth.hu.bu     vr6,    vr2
    vsllwil.hu.bu   vr4,    vr2,     0
    vmul.h          vr1,    vr1,     vr4  //b*m
    vmul.h          vr5,    vr5,     vr6  //b*m
    vsub.b          vr3,    vr23,    vr2
    vexth.hu.bu     vr7,    vr0
    vexth.hu.bu     vr8,    vr3
    vmadd.h         vr5,    vr7,     vr8
    vsllwil.hu.bu   vr0,    vr0,     0
    vsllwil.hu.bu   vr3,    vr3,     0
    vmadd.h         vr1,    vr0,     vr3
    vssrarni.bu.h   vr5,    vr1,     6

    vst             vr5,    a0,      0

    /* sencond */
    vld             vr0,    a0,      16
    vld             vr1,    a2,      16
    vld             vr2,    a5,      16

    vexth.hu.bu     vr5,    vr1
    vsllwil.hu.bu   vr1,    vr1,     0
    vexth.hu.bu     vr6,    vr2
    vsllwil.hu.bu   vr4,    vr2,     0
    vmul.h          vr1,    vr1,     vr4  //b*m
    vmul.h          vr5,    vr5,     vr6  //b*m
    vsub.b          vr3,    vr23,    vr2
    vexth.hu.bu     vr7,    vr0
    vexth.hu.bu     vr8,    vr3
    vmadd.h         vr5,    vr7,     vr8
    vsllwil.hu.bu   vr0,    vr0,     0
    vsllwil.hu.bu   vr3,    vr3,     0
    vmadd.h         vr1,    vr0,     vr3
    vssrarni.bu.h   vr5,    vr1,     6

    vst             vr5,    a0,      16
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      32
    addi.d          a5,     a5,      32

    blt             zero,   a4,     .BLEND_W32_LSX
.BLEND_END_LSX:

endfunc

const obmc_masks_la
/* Unused */
.byte 0,  0,  0,  0
/* 2 */
.byte 45, 19, 64, 0
/* 4 */
.byte 39, 25, 50, 14, 59,  5, 64,  0
/* 8 */
.byte 36, 28, 42, 22, 48, 16, 53, 11, 57,  7, 61,  3, 64,  0, 64,  0
/* 16 */
.byte 34, 30, 37, 27, 40, 24, 43, 21, 46, 18, 49, 15, 52, 12, 54, 10
.byte 56,  8, 58,  6, 60,  4, 61,  3, 64,  0, 64,  0, 64,  0, 64,  0
/* 32 */
.byte 33, 31, 35, 29, 36, 28, 38, 26, 40, 24, 41, 23, 43, 21, 44, 20
.byte 45, 19, 47, 17, 48, 16, 50, 14, 51, 13, 52, 12, 53, 11, 55,  9
.byte 56,  8, 57,  7, 58,  6, 59,  5, 60,  4, 60,  4, 61,  3, 62,  2
endconst

/*
 * static void blend_v_lsx(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                           const int w, int h)
 */
function blend_v_8bpc_lsx
    la.local      t8,     obmc_masks_la

    clz.w         t0,     a3
    li.w          t1,     26
    sub.w         t0,     t0,      t1
    la.local      t1,     .BLEND_V_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0  // The jump addresses are relative to JRTABLE
    add.d         t1,     t1,      t2 // Get absolute address
    jirl          $r0,    t1,      0

    .align   3
.BLEND_V_LSX_JRTABLE:
    .hword .BLEND_V_W32_LSX  - .BLEND_V_LSX_JRTABLE
    .hword .BLEND_V_W16_LSX  - .BLEND_V_LSX_JRTABLE
    .hword .BLEND_V_W8_LSX   - .BLEND_V_LSX_JRTABLE
    .hword .BLEND_V_W4_LSX   - .BLEND_V_LSX_JRTABLE
    .hword .BLEND_V_W2_LSX   - .BLEND_V_LSX_JRTABLE
    .hword .BLEND_V_W2_LSX_1 - .BLEND_V_LSX_JRTABLE  //Instructions must be 4-byte aligned

.BLEND_V_W2_LSX:
    ld.bu           t6,     t8,      4
    ld.bu           t7,     t8,      5

.BLEND_V_W2_LSX_1:
    ld.bu           t0,     a0,      0
    ld.bu           t1,     a2,      0
    mul.d           t0,     t0,      t6
    mul.d           t1,     t1,      t7
    addi.d          t0,     t0,      32
    add.d           t0,     t0,      t1
    srli.d          t0,     t0,      6
    st.b            t0,     a0,      0

    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      2
    addi.d          a5,     a5,      2

    blt             zero,   a4,     .BLEND_V_W2_LSX_1
    b               .BLEND_V_END_LSX

.BLEND_V_W4_LSX:
    vld             vr20,   t8,      8

.BLEND_V_W4_LSX_1:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.h        vr1,    a0,      0,   0
    vstelm.b        vr1,    a0,      2,   2
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      4

    blt             zero,   a4,     .BLEND_V_W4_LSX_1
    b              .BLEND_V_END_LSX

.BLEND_V_W8_LSX:
    vld             vr20,   t8,      16

.BLEND_V_W8_LSX_1:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.w        vr1,    a0,      0,   0
    vstelm.h        vr1,    a0,      4,   2
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      8

    blt             zero,   a4,     .BLEND_V_W8_LSX_1
    b              .BLEND_V_END_LSX

.BLEND_V_W16_LSX:
    vld             vr20,   t8,      32
    vld             vr21,   t8,      48

.BLEND_V_W16_LSX_1:
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr2,    vr1,     vr0
    vilvh.b         vr3,    vr1,     vr0
    vmulwev.h.bu    vr4,    vr2,     vr20
    vmulwev.h.bu    vr5,    vr3,     vr21
    vmaddwod.h.bu   vr4,    vr2,     vr20
    vmaddwod.h.bu   vr5,    vr3,     vr21
    vssrarni.bu.h   vr5,    vr4,     6

    vstelm.d        vr5,    a0,      0,   0
    vstelm.w        vr5,    a0,      8,   2
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      16

    blt             zero,   a4,     .BLEND_V_W16_LSX_1
    b              .BLEND_V_END_LSX

.BLEND_V_W32_LSX:
    vld             vr20,   t8,      64
    vld             vr21,   t8,      80
    vld             vr22,   t8,      96

.BLEND_V_W32_LSX_1:
    vld             vr0,    a0,      0
    vld             vr1,    a0,      16
    vld             vr2,    a2,      0
    vld             vr3,    a2,      16

    vilvl.b         vr4,    vr2,     vr0
    vmulwev.h.bu    vr7,    vr4,     vr20
    vilvh.b         vr5,    vr2,     vr0
    vmulwev.h.bu    vr8,    vr5,     vr21
    vilvl.b         vr6,    vr3,     vr1
    vmulwev.h.bu    vr9,    vr6,     vr22
    vmaddwod.h.bu   vr7,    vr4,     vr20
    vmaddwod.h.bu   vr8,    vr5,     vr21
    vmaddwod.h.bu   vr9,    vr6,     vr22
    vssrarni.bu.h   vr8,    vr7,     6
    vssrarni.bu.h   vr9,    vr9,     6

    vst             vr8,    a0,      0
    vstelm.d        vr9,    a0,      16,   0
    addi.w          a4,     a4,      -1
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      32

    blt             zero,   a4,     .BLEND_V_W32_LSX_1

.BLEND_V_END_LSX:

endfunc

/*
 * static void blend_h_lsx(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                           const int w, int h)
 */
function blend_h_8bpc_lsx
    la.local      t8,     obmc_masks_la
    alsl.d        t8,     a4,      t8,    1
    srli.d        t0,     a4,      1
    srli.d        t1,     a4,      2
    add.d         a4,     t0,      t1  // h = (h * 3) >> 2;
    slli.d        a4,     a4,      1
    add.d         a4,     a4,      t8

    clz.w         t0,     a3
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .BLEND_H_LSX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0  // The jump addresses are relative to JRTABLE
    add.d         t1,     t1,      t2 // Get absolute address
    jirl          $r0,    t1,      0

    .align   3
.BLEND_H_LSX_JRTABLE:
    .hword .BLEND_H_W128_LSX - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_W64_LSX  - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_W32_LSX  - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_W16_LSX  - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_W8_LSX   - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_W4_LSX   - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_W2_LSX   - .BLEND_H_LSX_JRTABLE
    .hword .BLEND_H_END_LSX  - .BLEND_H_LSX_JRTABLE  //Instructions must be 4-byte aligned

.BLEND_H_W2_LSX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.h        vr1,    a0,      0,   0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      2

    blt             t8,     a4,     .BLEND_H_W2_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W4_LSX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.w        vr1,    a0,      0,   0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      4

    blt             t8,     a4,     .BLEND_H_W4_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W8_LSX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.d        vr1,    a0,      0,   0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      8

    blt             t8,     a4,     .BLEND_H_W8_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W16_LSX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr2,    vr1,     vr0
    vilvh.b         vr3,    vr1,     vr0
    vmulwev.h.bu    vr4,    vr2,     vr20
    vmulwev.h.bu    vr5,    vr3,     vr20
    vmaddwod.h.bu   vr4,    vr2,     vr20
    vmaddwod.h.bu   vr5,    vr3,     vr20
    vssrarni.bu.h   vr5,    vr4,     6

    vst             vr5,    a0,      0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      16

    blt             t8,     a4,     .BLEND_H_W16_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W32_LSX:
    vldrepl.h       vr20,   t8,      0

    vld             vr0,    a0,      0
    vld             vr1,    a0,      16
    vld             vr2,    a2,      0
    vld             vr3,    a2,      16

    vilvl.b         vr4,    vr2,     vr0
    vilvh.b         vr5,    vr2,     vr0
    vilvl.b         vr6,    vr3,     vr1
    vilvh.b         vr3,    vr3,     vr1
    vmulwev.h.bu    vr7,    vr4,     vr20
    vmulwev.h.bu    vr8,    vr5,     vr20
    vmulwev.h.bu    vr9,    vr6,     vr20
    vmulwev.h.bu    vr0,    vr3,     vr20
    vmaddwod.h.bu   vr7,    vr4,     vr20
    vmaddwod.h.bu   vr8,    vr5,     vr20
    vmaddwod.h.bu   vr9,    vr6,     vr20
    vmaddwod.h.bu   vr0,    vr3,     vr20
    vssrarni.bu.h   vr8,    vr7,     6
    vssrarni.bu.h   vr0,    vr9,     6

    vst             vr8,    a0,      0
    vst             vr0,    a0,      16
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      32

    blt             t8,     a4,     .BLEND_H_W32_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W64_LSX:
    vldrepl.h       vr20,   t8,      0

    vld             vr0,    a0,      0
    vld             vr1,    a0,      16
    vld             vr2,    a0,      32
    vld             vr3,    a0,      48
    vld             vr4,    a2,      0
    vld             vr5,    a2,      16
    vld             vr6,    a2,      32
    vld             vr7,    a2,      48

    vilvl.b         vr8,    vr4,     vr0
    vilvh.b         vr9,    vr4,     vr0
    vilvl.b         vr10,   vr5,     vr1
    vilvh.b         vr11,   vr5,     vr1
    vilvl.b         vr12,   vr6,     vr2
    vilvh.b         vr13,   vr6,     vr2
    vilvl.b         vr14,   vr7,     vr3
    vilvh.b         vr15,   vr7,     vr3
    vmulwev.h.bu    vr0,    vr8,     vr20
    vmulwev.h.bu    vr1,    vr9,     vr20
    vmulwev.h.bu    vr2,    vr10,    vr20
    vmulwev.h.bu    vr3,    vr11,    vr20
    vmulwev.h.bu    vr4,    vr12,    vr20
    vmulwev.h.bu    vr5,    vr13,    vr20
    vmulwev.h.bu    vr6,    vr14,    vr20
    vmulwev.h.bu    vr7,    vr15,    vr20

    vmaddwod.h.bu   vr0,    vr8,     vr20
    vmaddwod.h.bu   vr1,    vr9,     vr20
    vmaddwod.h.bu   vr2,    vr10,    vr20
    vmaddwod.h.bu   vr3,    vr11,    vr20
    vmaddwod.h.bu   vr4,    vr12,    vr20
    vmaddwod.h.bu   vr5,    vr13,    vr20
    vmaddwod.h.bu   vr6,    vr14,    vr20
    vmaddwod.h.bu   vr7,    vr15,    vr20

    vssrarni.bu.h   vr1,    vr0,     6
    vssrarni.bu.h   vr3,    vr2,     6
    vssrarni.bu.h   vr5,    vr4,     6
    vssrarni.bu.h   vr7,    vr6,     6

    vst             vr1,    a0,      0
    vst             vr3,    a0,      16
    vst             vr5,    a0,      32
    vst             vr7,    a0,      48
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      64

    blt             t8,     a4,     .BLEND_H_W64_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W128_LSX:
    vldrepl.h       vr20,   t8,      0

    vld             vr0,    a0,      0
    vld             vr1,    a0,      16
    vld             vr2,    a0,      32
    vld             vr3,    a0,      48
    vld             vr4,    a2,      0
    vld             vr5,    a2,      16
    vld             vr6,    a2,      32
    vld             vr7,    a2,      48

    vilvl.b         vr8,    vr4,     vr0
    vilvh.b         vr9,    vr4,     vr0
    vilvl.b         vr10,   vr5,     vr1
    vilvh.b         vr11,   vr5,     vr1
    vilvl.b         vr12,   vr6,     vr2
    vilvh.b         vr13,   vr6,     vr2
    vilvl.b         vr14,   vr7,     vr3
    vilvh.b         vr15,   vr7,     vr3
    vmulwev.h.bu    vr0,    vr8,     vr20
    vmulwev.h.bu    vr1,    vr9,     vr20
    vmulwev.h.bu    vr2,    vr10,    vr20
    vmulwev.h.bu    vr3,    vr11,    vr20
    vmulwev.h.bu    vr4,    vr12,    vr20
    vmulwev.h.bu    vr5,    vr13,    vr20
    vmulwev.h.bu    vr6,    vr14,    vr20
    vmulwev.h.bu    vr7,    vr15,    vr20

    vmaddwod.h.bu   vr0,    vr8,     vr20
    vmaddwod.h.bu   vr1,    vr9,     vr20
    vmaddwod.h.bu   vr2,    vr10,    vr20
    vmaddwod.h.bu   vr3,    vr11,    vr20
    vmaddwod.h.bu   vr4,    vr12,    vr20
    vmaddwod.h.bu   vr5,    vr13,    vr20
    vmaddwod.h.bu   vr6,    vr14,    vr20
    vmaddwod.h.bu   vr7,    vr15,    vr20

    vssrarni.bu.h   vr1,    vr0,     6
    vssrarni.bu.h   vr3,    vr2,     6
    vssrarni.bu.h   vr5,    vr4,     6
    vssrarni.bu.h   vr7,    vr6,     6

    vst             vr1,    a0,      0
    vst             vr3,    a0,      16
    vst             vr5,    a0,      32
    vst             vr7,    a0,      48

    /* second */
    vld             vr0,    a0,      64
    vld             vr1,    a0,      80
    vld             vr2,    a0,      96
    vld             vr3,    a0,      112
    vld             vr4,    a2,      64
    vld             vr5,    a2,      80
    vld             vr6,    a2,      96
    vld             vr7,    a2,      112

    vilvl.b         vr8,    vr4,     vr0
    vilvh.b         vr9,    vr4,     vr0
    vilvl.b         vr10,   vr5,     vr1
    vilvh.b         vr11,   vr5,     vr1
    vilvl.b         vr12,   vr6,     vr2
    vilvh.b         vr13,   vr6,     vr2
    vilvl.b         vr14,   vr7,     vr3
    vilvh.b         vr15,   vr7,     vr3
    vmulwev.h.bu    vr0,    vr8,     vr20
    vmulwev.h.bu    vr1,    vr9,     vr20
    vmulwev.h.bu    vr2,    vr10,    vr20
    vmulwev.h.bu    vr3,    vr11,    vr20
    vmulwev.h.bu    vr4,    vr12,    vr20
    vmulwev.h.bu    vr5,    vr13,    vr20
    vmulwev.h.bu    vr6,    vr14,    vr20
    vmulwev.h.bu    vr7,    vr15,    vr20

    vmaddwod.h.bu   vr0,    vr8,     vr20
    vmaddwod.h.bu   vr1,    vr9,     vr20
    vmaddwod.h.bu   vr2,    vr10,    vr20
    vmaddwod.h.bu   vr3,    vr11,    vr20
    vmaddwod.h.bu   vr4,    vr12,    vr20
    vmaddwod.h.bu   vr5,    vr13,    vr20
    vmaddwod.h.bu   vr6,    vr14,    vr20
    vmaddwod.h.bu   vr7,    vr15,    vr20

    vssrarni.bu.h   vr1,    vr0,     6
    vssrarni.bu.h   vr3,    vr2,     6
    vssrarni.bu.h   vr5,    vr4,     6
    vssrarni.bu.h   vr7,    vr6,     6

    vst             vr1,    a0,      64
    vst             vr3,    a0,      80
    vst             vr5,    a0,      96
    vst             vr7,    a0,      112

    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      128

    blt             t8,     a4,     .BLEND_H_W128_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_END_LSX:

endfunc

/*
 * static void blend_h_lsx(pixel *dst, const ptrdiff_t dst_stride, const pixel *tmp,
                           const int w, int h)
 */
function blend_h_8bpc_lasx
    la.local      t8,     obmc_masks_la
    alsl.d        t8,     a4,      t8,    1
    srli.d        t0,     a4,      1
    srli.d        t1,     a4,      2
    add.d         a4,     t0,      t1  // h = (h * 3) >> 2;
    slli.d        a4,     a4,      1
    add.d         a4,     a4,      t8

    clz.w         t0,     a3
    li.w          t1,     24
    sub.w         t0,     t0,      t1
    la.local      t1,     .BLEND_H_LASX_JRTABLE
    alsl.d        t0,     t0,      t1,    1
    ld.h          t2,     t0,      0  // The jump addresses are relative to JRTABLE
    add.d         t1,     t1,      t2 // Get absolute address
    jirl          $r0,    t1,      0

    .align   3
.BLEND_H_LASX_JRTABLE:
    .hword .BLEND_H_W128_LASX - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_W64_LASX  - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_W32_LASX  - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_W16_LASX  - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_W8_LASX   - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_W4_LASX   - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_W2_LASX   - .BLEND_H_LASX_JRTABLE
    .hword .BLEND_H_END_LASX  - .BLEND_H_LASX_JRTABLE  //Instructions must be 4-byte aligned

.BLEND_H_W2_LASX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.h        vr1,    a0,      0,   0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      2

    blt             t8,     a4,     .BLEND_H_W2_LASX
    b               .BLEND_H_END_LASX

.BLEND_H_W4_LASX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.w        vr1,    a0,      0,   0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      4

    blt             t8,     a4,     .BLEND_H_W4_LASX
    b               .BLEND_H_END_LASX

.BLEND_H_W8_LASX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr0,    vr1,     vr0
    vdp2.h.bu       vr1,    vr0,     vr20
    vssrarni.bu.h   vr1,    vr1,     6

    vstelm.d        vr1,    a0,      0,   0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      8

    blt             t8,     a4,     .BLEND_H_W8_LASX
    b               .BLEND_H_END_LASX

.BLEND_H_W16_LASX:
    vldrepl.h       vr20,   t8,      0
    vld             vr0,    a0,      0
    vld             vr1,    a2,      0

    vilvl.b         vr2,    vr1,     vr0
    vilvh.b         vr3,    vr1,     vr0
    vmulwev.h.bu    vr4,    vr2,     vr20
    vmulwev.h.bu    vr5,    vr3,     vr20
    vmaddwod.h.bu   vr4,    vr2,     vr20
    vmaddwod.h.bu   vr5,    vr3,     vr20
    vssrarni.bu.h   vr5,    vr4,     6

    vst             vr5,    a0,      0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      16

    blt             t8,     a4,     .BLEND_H_W16_LSX
    b               .BLEND_H_END_LSX

.BLEND_H_W32_LASX:
    xvldrepl.h      xr20,   t8,      0

    xvld            xr0,    a0,      0
    xvld            xr1,    a2,      0

    xvilvl.b        xr2,    xr1,     xr0
    xvilvh.b        xr3,    xr1,     xr0

    xvmulwev.h.bu   xr4,    xr2,     xr20
    xvmulwev.h.bu   xr5,    xr3,     xr20
    xvmaddwod.h.bu  xr4,    xr2,     xr20
    xvmaddwod.h.bu  xr5,    xr3,     xr20
    xvssrarni.bu.h  xr5,    xr4,     6

    xvst            xr5,    a0,      0
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      32

    blt             t8,     a4,     .BLEND_H_W32_LASX
    b               .BLEND_H_END_LASX

.BLEND_H_W64_LASX:
    xvldrepl.h      xr20,   t8,      0

    xvld            xr0,    a0,      0
    xvld            xr1,    a0,      32
    xvld            xr2,    a2,      0
    xvld            xr3,    a2,      32

    xvilvl.b        xr4,    xr2,     xr0
    xvilvh.b        xr5,    xr2,     xr0
    xvilvl.b        xr6,    xr3,     xr1
    xvilvh.b        xr7,    xr3,     xr1

    xvmulwev.h.bu   xr0,    xr4,     xr20
    xvmulwev.h.bu   xr1,    xr5,     xr20
    xvmulwev.h.bu   xr2,    xr6,     xr20
    xvmulwev.h.bu   xr3,    xr7,     xr20
    xvmaddwod.h.bu  xr0,    xr4,     xr20
    xvmaddwod.h.bu  xr1,    xr5,     xr20
    xvmaddwod.h.bu  xr2,    xr6,     xr20
    xvmaddwod.h.bu  xr3,    xr7,     xr20
    xvssrarni.bu.h  xr1,    xr0,     6
    xvssrarni.bu.h  xr3,    xr2,     6

    xvst            xr1,    a0,      0
    xvst            xr3,    a0,      32
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      64

    blt             t8,     a4,     .BLEND_H_W64_LASX
    b               .BLEND_H_END_LASX

.BLEND_H_W128_LASX:
    xvldrepl.h      xr20,   t8,      0

    xvld            xr0,    a0,      0
    xvld            xr1,    a0,      32
    xvld            xr2,    a0,      64
    xvld            xr3,    a0,      96
    xvld            xr4,    a2,      0
    xvld            xr5,    a2,      32
    xvld            xr6,    a2,      64
    xvld            xr7,    a2,      96

    xvilvl.b        xr8,    xr4,     xr0
    xvilvh.b        xr9,    xr4,     xr0
    xvilvl.b        xr10,   xr5,     xr1
    xvilvh.b        xr11,   xr5,     xr1
    xvilvl.b        xr12,   xr6,     xr2
    xvilvh.b        xr13,   xr6,     xr2
    xvilvl.b        xr14,   xr7,     xr3
    xvilvh.b        xr15,   xr7,     xr3

    xvmulwev.h.bu   xr0,    xr8,     xr20
    xvmulwev.h.bu   xr1,    xr9,     xr20
    xvmulwev.h.bu   xr2,    xr10,    xr20
    xvmulwev.h.bu   xr3,    xr11,    xr20
    xvmulwev.h.bu   xr4,    xr12,    xr20
    xvmulwev.h.bu   xr5,    xr13,    xr20
    xvmulwev.h.bu   xr6,    xr14,    xr20
    xvmulwev.h.bu   xr7,    xr15,    xr20
    xvmaddwod.h.bu  xr0,    xr8,     xr20
    xvmaddwod.h.bu  xr1,    xr9,     xr20
    xvmaddwod.h.bu  xr2,    xr10,    xr20
    xvmaddwod.h.bu  xr3,    xr11,    xr20
    xvmaddwod.h.bu  xr4,    xr12,    xr20
    xvmaddwod.h.bu  xr5,    xr13,    xr20
    xvmaddwod.h.bu  xr6,    xr14,    xr20
    xvmaddwod.h.bu  xr7,    xr15,    xr20
    xvssrarni.bu.h  xr1,    xr0,     6
    xvssrarni.bu.h  xr3,    xr2,     6
    xvssrarni.bu.h  xr5,    xr4,     6
    xvssrarni.bu.h  xr7,    xr6,     6

    xvst            xr1,    a0,      0
    xvst            xr3,    a0,      32
    xvst            xr5,    a0,      64
    xvst            xr7,    a0,      96
    addi.d          t8,     t8,      2
    add.d           a0,     a0,      a1
    addi.d          a2,     a2,      128

    blt             t8,     a4,     .BLEND_H_W128_LASX
    b               .BLEND_H_END_LASX

.BLEND_H_END_LASX:

endfunc

/*
 *  a1=16 | a2=8 | a3=4
 *  temp reg: a4
 */
.macro PIXEL_COPY_LSX _dst, _src, _size
    blt             \_size,  a1,     8f
16:
    vld             vr0,     \_src,  0
    vst             vr0,     \_dst,  0
    addi.d          \_size,  \_size, -16
    addi.d          \_dst,   \_dst,  16
    addi.d          \_src,   \_src,  16
    blt             a1,      \_size, 16b
8:
    blt             \_size,  a2,     14f
    ld.d            a4,      \_src,  0
    st.d            a4,      \_dst,  0
    addi.d          \_size,  \_size, -8
    addi.d          \_dst,   \_dst,  8
    addi.d          \_src,   \_src,  8
14:
    blt             \_size,  a3,     11f
    ld.w            a4,      \_src,  0
    st.w            a4,      \_dst,  0
    addi.d          \_size,  \_size, -4
    addi.d          \_dst,   \_dst,  4
    addi.d          \_src,   \_src,  4
11:
    beqz            \_size,  110f
111:
    ld.b            a4,      \_src,  0
    st.b            a4,      \_dst,  0
    addi.d          \_size,  \_size, -1
    addi.d          \_dst,   \_dst,  1
    addi.d          \_src,   \_src,  1
    bnez            \_size,  111b
110:
.endm

/*
 *  a1=16 | a2=8 | a3=4
 */
.macro PIXEL_SET_LSX _dst, _vsrc, _size
    blt             \_size,  a1,     8f
16:
    vst             \_vsrc,  \_dst,  0
    addi.d          \_size,  \_size, -16
    addi.d          \_dst,   \_dst,  16
    blt             a1,      \_size, 16b
8:
    blt             \_size,  a2,     14f
    vstelm.d        \_vsrc,  \_dst,  0,   0
    addi.d          \_size,  \_size, -8
    addi.d          \_dst,   \_dst,  8
14:
    blt             \_size,  a3,     11f
    vstelm.w        \_vsrc,  \_dst,  0,   0
    addi.d          \_size,  \_size, -4
    addi.d          \_dst,   \_dst,  4
11:
    beqz            \_size,  110f
111:
    vstelm.b        \_vsrc,  \_dst,  0,   0
    addi.d          \_size,  \_size, -1
    addi.d          \_dst,   \_dst,  1
    bnez            \_size,  111b
110:
.endm

/*
 *  temp reg: a4 a5 t2 t3 vr0
 */
.macro DEGE_LOOP need_left, need_right
0:
    addi.d          t2,      t6,     0   // dst
    addi.d          t3,      t7,     0   // src
.if \need_left
    vldrepl.b       vr0,     t3,     0
    addi.d          a5,      t0,     0
    PIXEL_SET_LSX t2, vr0, a5
.endif

    addi.d          a5,      t4,     0
    PIXEL_COPY_LSX t2, t3, a5

.if \need_right
    vldrepl.b       vr0,     t3,     -1
    addi.d          a5,      t1,     0
    PIXEL_SET_LSX t2, vr0, a5
.endif

    addi.d          t5,      t5,     -1
    add.d           t7,      t7,     t8
    add.d           t6,      t6,     a7
    bnez            t5,      0b
.endm

/*
 * static void emu_edge_c(const intptr_t bw, const intptr_t bh,
 *                        const intptr_t iw, const intptr_t ih,
 *                        const intptr_t x, const intptr_t y,
 *                        pixel *dst, const ptrdiff_t dst_stride,
 *                        const pixel *ref, const ptrdiff_t ref_stride)
 */
function emu_edge_8bpc_lsx
    vxor.v          vr23,   vr23,    vr23   // zero
    addi.d          t0,     a3,      -1     // ih - 1
    addi.d          t1,     a2,      -1     // iw - 1
    vreplgr2vr.w    vr22,   t0
    vinsgr2vr.w     vr22,   t1,        1
    vreplgr2vr.w    vr0,    a5
    vinsgr2vr.w     vr0,    a4,        1     // [0] - h | [1] - w

    vclip.w         vr2,    vr0,      vr23,    vr22
    vpickve2gr.w    t0,     vr2,      0
    ld.d            t2,     sp,       0
    ld.d            t8,     sp,       8     // ref_stride
    mul.w           t0,     t0,       t8
    vpickve2gr.w    t1,     vr2,      1
    add.d           t2,     t2,       t1
    add.d           t7,     t0,       t2    // ref

    addi.d          t0,     a0,       -1     // bw - 1
    addi.d          t1,     a1,       -1     // bh - 1
    vreplgr2vr.w    vr21,   t0
    vreplgr2vr.w    vr22,   t1
    vilvl.d         vr21,   vr22,      vr21
    sub.d           t2,     zero,      a4    // -x
    add.d           t3,     a0,        a4
    sub.d           t3,     t3,        a2    // x + bw - iw
    sub.d           t4,     zero,      a5    // -y
    add.d           t5,     a1,        a5
    sub.d           t5,     t5,        a3    // y + bh - ih
    vreplgr2vr.w    vr0,    t2
    vinsgr2vr.w     vr0,    t3,        1
    vinsgr2vr.w     vr0,    t4,        2
    vinsgr2vr.w     vr0,    t5,        3
    vclip.w         vr2,    vr0,       vr23,    vr21
    vpickve2gr.w    t0,     vr2,       0     // left_ext
    vpickve2gr.w    t1,     vr2,       1     // right_ext
    vpickve2gr.w    t2,     vr2,       2     // top_ext
    vpickve2gr.w    t3,     vr2,       3     // bottom_ext

    mul.w           t6,     t2,        a7
    add.d           t4,     t0,        t1
    add.d           t5,     t2,        t3
    sub.d           t4,     a0,        t4    // center_w
    sub.d           t5,     a1,        t5    // center_h

    addi.d          a1,     zero,      16
    addi.d          a2,     zero,      8
    addi.d          a3,     zero,      4
    add.d           t6,     t6,        a6    // blk

    beqz            t0,     2f
    // need_left
    beqz            t1,     3f
    // need_left + need_right
    DEGE_LOOP       1,   1
    b               5f

2:
    // !need_left
    beqz            t1,     4f
    // !need_left + need_right
    DEGE_LOOP       0,   1
    b               5f

3:
    // need_left + !need_right
    DEGE_LOOP       1,   0
    b               5f

4:
    // !need_left + !need_right
    DEGE_LOOP       0,   0

5:
    vpickve2gr.w    t2,     vr2,       2     // top_ext
    vpickve2gr.w    t3,     vr2,       3     // bottom_ext
    sub.d           t7,     a7,        a0    // dst_stride - bw
    mul.w           t8,     t2,        a7

    beqz            t3,     2f
    // need_bottom
    sub.d           t0,     t6,        a7    //  &dst[-PXSTRIDE(dst_stride)]
1:
    addi.d          t1,     t0,        0
    addi.d          a5,     a0,        0
    PIXEL_COPY_LSX t6, t1, a5
    add.d           t6,     t6,        t7
    addi.d          t3,     t3,   -1
    bnez            t3,     1b
2:
    beqz            t2,     3f
    // need_top
    add.d           t8,     t8,        a6    // blk
1:
    addi.d          t1,     t8,        0
    addi.d          a5,     a0,        0
    PIXEL_COPY_LSX a6, t1, a5
    add.d           a6,     a6,        t7
    addi.d          t2,     t2,   -1
    bnez            t2,     1b
3:

endfunc
