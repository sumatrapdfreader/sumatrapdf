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
#include "src/loongarch/loongson_util.S"

.macro PUSH_REG
    addi.d           sp,     sp,    -64
    fst.d            f24,    sp,     0
    fst.d            f25,    sp,     8
    fst.d            f26,    sp,     16
    fst.d            f27,    sp,     24
    fst.d            f28,    sp,     32
    fst.d            f29,    sp,     40
    fst.d            f30,    sp,     48
    fst.d            f31,    sp,     56
.endm

.macro POP_REG
    fld.d            f24,    sp,     0
    fld.d            f25,    sp,     8
    fld.d            f26,    sp,     16
    fld.d            f27,    sp,     24
    fld.d            f28,    sp,     32
    fld.d            f29,    sp,     40
    fld.d            f30,    sp,     48
    fld.d            f31,    sp,     56
    addi.d           sp,     sp,     64
.endm

.macro malloc_space number
    li.w          t0,       \number
    sub.d         sp,       sp,       t0
    addi.d        sp,       sp,       -64
    PUSH_REG
.endm

.macro free_space number
    POP_REG
    li.w          t0,       \number
    add.d         sp,       sp,       t0
    addi.d        sp,       sp,       64
.endm

.macro iwht4
    vadd.h        vr0,       vr0,     vr1
    vsub.h        vr4,       vr2,     vr3
    vsub.h        vr5,       vr0,     vr4
    vsrai.h       vr5,       vr5,     1
    vsub.h        vr2,       vr5,     vr1
    vsub.h        vr1,       vr5,     vr3
    vadd.h        vr3,       vr4,     vr2
    vsub.h        vr0,       vr0,     vr1
.endm

.macro DST_ADD_W4 in0, in1, in2, in3, in4, in5
    vilvl.w       \in0,     \in1,     \in0  // 0 1  2  3  4  5  6  7 x ...
    vilvl.w       \in2,     \in3,     \in2  // 8 9 10 11 12 13 14 15 x ...
    vsllwil.hu.bu \in0,     \in0,     0
    vsllwil.hu.bu \in2,     \in2,     0
    vadd.h        \in0,     \in4,     \in0
    vadd.h        \in2,     \in5,     \in2
    vssrani.bu.h  \in2,     \in0,     0
    vstelm.w      \in2,     a0,       0,    0
    vstelmx.w     \in2,     a0,       a1,   1
    vstelmx.w     \in2,     a0,       a1,   2
    vstelmx.w     \in2,     a0,       a1,   3
.endm

.macro VLD_DST_ADD_W4 in0, in1
    vld           vr0,      a0,       0
    vldx          vr1,      a0,       a1
    vld           vr2,      t2,       0
    vldx          vr3,      t2,       a1

    DST_ADD_W4    vr0, vr1, vr2, vr3, \in0, \in1
.endm

function inv_txfm_add_wht_wht_4x4_8bpc_lsx
    vld           vr0,       a2,      0
    vld           vr2,       a2,      16

    vxor.v        vr20,      vr20,    vr20
    vsrai.h       vr0,       vr0,     2
    vsrai.h       vr2,       vr2,     2
    vst           vr20,      a2,      0
    vpickod.d     vr1,       vr0,     vr0
    vpickod.d     vr3,       vr2,     vr2
    vst           vr20,      a2,      16

    iwht4

    LSX_TRANSPOSE4x4_H vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3, vr4, vr5

    iwht4

    vilvl.d       vr4,       vr1,     vr0
    vilvl.d       vr5,       vr3,     vr2
    alsl.d        t2,        a1,      a0,    1
    VLD_DST_ADD_W4 vr4, vr5
endfunc

const idct_coeffs, align=4
    .word          2896, 2896*8, 1567, 3784
    .word          799, 4017, 3406, 2276
    .word          401, 4076, 3166, 2598
    .word          1931, 3612, 3920, 1189
    .word          201, 4091, 3035, 2751
    .word          1751, 3703, 3857, 1380
    .word          995, 3973, 3513, 2106
    .word          2440, 3290, 4052, 601
endconst

.macro vsrari_h_x4 in0, in1, in2, in3, out0, out1, out2, out3, shift
    vsrari.h      \out0,    \in0,     \shift
    vsrari.h      \out1,    \in1,     \shift
    vsrari.h      \out2,    \in2,     \shift
    vsrari.h      \out3,    \in3,     \shift
.endm

.macro vsrari_h_x8 in0, in1, in2, in3, in4, in5, in6, in7, out0, \
                   out1, out2, out3, out4, out5, out6, out7, shift
    vsrari.h      \out0,    \in0,     \shift
    vsrari.h      \out1,    \in1,     \shift
    vsrari.h      \out2,    \in2,     \shift
    vsrari.h      \out3,    \in3,     \shift
    vsrari.h      \out4,    \in4,     \shift
    vsrari.h      \out5,    \in5,     \shift
    vsrari.h      \out6,    \in6,     \shift
    vsrari.h      \out7,    \in7,     \shift
.endm

.macro vmulev_vmaddod_lsx in0, in1, in2, in3, out0, out1, sz
    vmulwev.w.h   \out0,    \in0,     \in2
    vmulwod.w.h   \out1,    \in0,     \in2
    vmaddwev.w.h  \out0,    \in1,     \in3
    vmaddwod.w.h  \out1,    \in1,     \in3
.ifc \sz, .4h
    vilvl.w       \out0,    \out1,    \out0
.else
    vilvl.w       vr22,     \out1,    \out0
    vilvh.w       \out1,    \out1,    \out0
    vor.v         \out0,    vr22,     vr22
.endif
.endm

const idct_coeffs_h, align=4
    .short          2896, 2896*8, 1567, 3784
    .short          799, 4017, 3406, 2276
    .short          401, 4076, 3166, 2598
    .short          1931, 3612, 3920, 1189
    .short          201, 4091, 3035, 2751
    .short          1751, 3703, 3857, 1380
    .short          995, 3973, 3513, 2106
    .short          2440, 3290, 4052, 601
endconst

const iadst4_coeffs, align=4
    .word          1321, 3803, 2482, 3344
endconst

.macro inv_dct4_lsx in0, in1, in2, in3, out0, out1, out2, out3, sz
    la.local      t0,       idct_coeffs_h

    vldrepl.h     vr20,     t0,       0    // 2896
    vmulev_vmaddod_lsx \in0, \in2, vr20, vr20, vr16, vr18, \sz
    vneg.h        vr21,     vr20
    vmulev_vmaddod_lsx \in0, \in2, vr20, vr21, vr17, vr19, \sz
    vssrarni.h.w  vr18,     vr16,     12   // t0
    vssrarni.h.w  vr19,     vr17,     12   // t1

    vldrepl.h     vr20,     t0,       4    // 1567
    vldrepl.h     vr21,     t0,       6    // 3784
    vmulev_vmaddod_lsx \in1, \in3, vr21, vr20, \in0, vr16, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx \in1, \in3, vr20, vr21, \in2, vr17, \sz
    vssrarni.h.w  vr16,     \in0,     12   // t3
    vssrarni.h.w  vr17,     \in2,     12   // t2

    vsadd.h       \out0,    vr18,     vr16
    vsadd.h       \out1,    vr19,     vr17
    vssub.h       \out2,    vr19,     vr17
    vssub.h       \out3,    vr18,     vr16
.endm

functionl inv_dct_4h_x4_lsx
    inv_dct4_lsx vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3, .4h
endfuncl

functionl inv_dct_8h_x4_lsx
    inv_dct4_lsx vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3, .8h
endfuncl

.macro inv_adst4_core_lsx in0, in1, in2, in3, out0, out1, out2, out3
    vsub.w        vr16,     \in0,    \in2  // in0-in2
    vmul.w        vr17,     \in0,    vr20  // in0*1321
    vmul.w        vr19,     \in0,    vr22  // in0*2482
    vmul.w        vr18,     \in1,    vr23  // in1*3344
    vmadd.w       vr17,     \in2,    vr21  // in0*1321+in2*3803
    vmsub.w       vr19,     \in2,    vr20  // in2*1321
    vadd.w        vr16,     vr16,    \in3  // in0-in2+in3
    vmadd.w       vr17,     \in3,    vr22  // in0*1321+in2*3803+in3*2482
    vmsub.w       vr19,     \in3,    vr21  // in0*2482-in2*1321-in3*3803
    vadd.w        vr15,     vr17,    vr19
    vmul.w        \out2,    vr16,    vr23  // out[2] 8  9  10 11
    vadd.w        \out0,    vr17,    vr18  // out[0] 0  1  2  3
    vadd.w        \out1,    vr19,    vr18  // out[1] 4  5  6  7
    vsub.w        \out3,    vr15,    vr18  // out[3] 12 13 14 15
.endm

.macro inv_adst4_lsx in0, in1, in2, in3, out0, out1, out2, out3
    la.local      t0,       iadst4_coeffs

    vldrepl.w     vr20,     t0,      0     // 1321
    vldrepl.w     vr21,     t0,      4     // 3803
    vldrepl.w     vr22,     t0,      8     // 2482
    vldrepl.w     vr23,     t0,      12    // 3344

    vsllwil.w.h   vr0,      \in0,    0
    vsllwil.w.h   vr1,      \in1,    0
    vsllwil.w.h   vr2,      \in2,    0
    vsllwil.w.h   vr3,      \in3,    0
    inv_adst4_core_lsx vr0, vr1, vr2, vr3, \out0, \out1, \out2, \out3
    vssrarni.h.w  \out0,    \out0,   12
    vssrarni.h.w  \out1,    \out1,   12
    vssrarni.h.w  \out2,    \out2,   12
    vssrarni.h.w  \out3,    \out3,   12
.endm

functionl inv_adst_4h_x4_lsx
    inv_adst4_lsx vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3
endfuncl

functionl inv_flipadst_4h_x4_lsx
    inv_adst4_lsx vr0, vr1, vr2, vr3, vr3, vr2, vr1, vr0
endfuncl

.macro inv_adst_8x4_lsx in0, in1, in2, in3, out0, out1, out2, out3
    la.local      t0,       iadst4_coeffs
    vldrepl.w     vr20,     t0,      0     // 1321
    vldrepl.w     vr21,     t0,      4     // 3803
    vldrepl.w     vr22,     t0,      8     // 2482
    vldrepl.w     vr23,     t0,      12    // 3344

    vsllwil.w.h   vr10,     \in0,     0     // in0
    vsllwil.w.h   vr11,     \in1,     0     // in1
    vsllwil.w.h   vr12,     \in2,     0     // in2
    vsllwil.w.h   vr13,     \in3,     0     // in3
    inv_adst4_core_lsx vr10, vr11, vr12, vr13, vr10, vr11, vr12, vr13

    vexth.w.h     \in0,      \in0           // in0
    vexth.w.h     \in1,      \in1           // in1
    vexth.w.h     \in2,      \in2           // in2
    vexth.w.h     \in3,      \in3           // in3
    inv_adst4_core_lsx \in0, \in1, \in2, \in3, \out0, \out1, \out2, \out3

    vssrarni.h.w  \out0,     vr10,    12
    vssrarni.h.w  \out1,     vr11,    12
    vssrarni.h.w  \out2,     vr12,    12
    vssrarni.h.w  \out3,     vr13,    12
.endm

functionl inv_adst_8h_x4_lsx
    inv_adst_8x4_lsx vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3
endfuncl

functionl inv_flipadst_8h_x4_lsx
    inv_adst_8x4_lsx vr0, vr1, vr2, vr3, vr3, vr2, vr1, vr0
endfuncl

functionl inv_identity_4h_x4_lsx
    li.w          t0,       1697
    vreplgr2vr.h  vr20,     t0

    vilvl.d       vr0,      vr1,      vr0
    vilvl.d       vr2,      vr3,      vr2
    vmulwev.w.h   vr16,     vr0,      vr20
    vmulwod.w.h   vr17,     vr0,      vr20
    vmulwev.w.h   vr18,     vr2,      vr20
    vmulwod.w.h   vr19,     vr2,      vr20
    vilvl.w       vr1,      vr17,     vr16
    vilvh.w       vr3,      vr17,     vr16
    vilvl.w       vr22,     vr19,     vr18
    vilvh.w       vr23,     vr19,     vr18
    vssrarni.h.w  vr3,      vr1,      12
    vssrarni.h.w  vr23,     vr22,     12
    vsadd.h       vr0,      vr3,      vr0  // t0
    vsadd.h       vr2,      vr23,     vr2  // t2
    vilvh.d       vr1,      vr0,      vr0  // t1
    vilvh.d       vr3,      vr2,      vr2  // t3
endfuncl

.macro inv_identity4_lsx1 in0, in1, in2, out0, out1
    vsllwil.w.h   vr16,     \in0,     0
    vexth.w.h     vr17,     \in1
    vmul.w        vr18,     vr16,     \in2
    vmul.w        vr19,     vr17,     \in2
    vsrari.w      vr18,     vr18,     12
    vsrari.w      vr19,     vr19,     12
    vadd.w        \out0,    vr18,     vr16
    vadd.w        \out1,    vr19,     vr17
    vssrarni.h.w  \out1,    \out0,    1
.endm

functionl inv_identity_8h_x4_lsx
    li.w          t0,        1697
    vreplgr2vr.h  vr20,      t0
    vmulwev.w.h   vr16,      vr0,     vr20
    vmulwod.w.h   vr17,      vr0,     vr20
    vmulwev.w.h   vr18,      vr1,     vr20
    vmulwod.w.h   vr19,      vr1,     vr20
    vilvl.w       vr21,      vr17,    vr16
    vilvh.w       vr22,      vr17,    vr16
    vilvl.w       vr23,      vr19,    vr18
    vilvh.w       vr16,      vr19,    vr18
    vssrarni.h.w  vr22,      vr21,    12
    vssrarni.h.w  vr16,      vr23,    12
    vsadd.h       vr0,       vr22,    vr0  // t0
    vsadd.h       vr1,       vr16,    vr1  // t1
    vmulwev.w.h   vr16,      vr2,     vr20
    vmulwod.w.h   vr17,      vr2,     vr20
    vmulwev.w.h   vr18,      vr3,     vr20
    vmulwod.w.h   vr19,      vr3,     vr20
    vilvl.w       vr21,      vr17,    vr16
    vilvh.w       vr22,      vr17,    vr16
    vilvl.w       vr23,      vr19,    vr18
    vilvh.w       vr16,      vr19,    vr18
    vssrarni.h.w  vr22,      vr21,    12
    vssrarni.h.w  vr16,      vr23,    12
    vsadd.h       vr2,       vr22,    vr2  // t2
    vsadd.h       vr3,       vr16,    vr3  // t3
endfuncl

functionl inv_identity_8h_x4_lsx1
    li.w          t0,        1697
    vreplgr2vr.w  vr20,      t0
.irp i, vr0, vr1, vr2, vr3
    inv_identity4_lsx1 \i, \i vr20, vr21, \i
.endr
endfuncl

functionl inv_txfm_add_4x4_lsx
    vxor.v        vr23,     vr23,     vr23
    vld           vr0,      a2,       0
    vld           vr2,      a2,       16
    vilvh.d       vr1,      vr0,      vr0
    vilvh.d       vr3,      vr2,      vr2
    vst           vr23,     a2,       0
    vst           vr23,     a2,       16

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

    LSX_TRANSPOSE4x4_H vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3, vr4, vr5

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vilvl.d       vr4,      vr1,      vr0
    vilvl.d       vr5,      vr3,      vr2
    vsrari.h      vr4,      vr4,      4
    vsrari.h      vr5,      vr5,      4
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W4 vr4, vr5
endfuncl

.macro idct_dc w, h, shift
    ld.h          t2,       a2,       0      // dc
    vldi          vr0,      0x8b5            // 181
    vreplgr2vr.w  vr1,      t2
    vldi          vr20,     0x880            // 128
    vmul.w        vr2,      vr0,      vr1    // dc * 181
    st.h          zero,     a2,       0
    vsrari.w      vr2,      vr2,      8      // (dc * 181 + 128) >> 8
    vld           vr10,     a0,       0      // 0 1 2 3 4 5 6 7

.if (2*\w == \h) || (2*\h == \w)
    vmul.w        vr2,      vr0,      vr2
    vsrari.w      vr2,      vr2,      8      // (dc * 181 + 128) >> 8
.endif
.if \shift>0
    vsrari.w      vr2,      vr2,      \shift      // (dc + rnd) >> shift
.endif
    vldx          vr11,     a0,       a1     // 8 9 10 11 12 13 14 15
    alsl.d        t2,       a1,       a0,    1
    vmadd.w       vr20,     vr2,      vr0
    vld           vr12,     t2,       0      // 16 17 18 19 20 21 22 23
    vssrarni.h.w  vr20,     vr20,     12
    vldx          vr13,     t2,       a1     // 24 25 26 27 28 29 30 31
.endm

.macro fun4x4 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_4x4_8bpc_lsx
.ifc \txfm1\()_\txfm2, dct_dct
    bnez          a3,       1f

    idct_dc 4, 4, 0

    DST_ADD_W4    vr10, vr11, vr12, vr13, vr20, vr20
    b             .\txfm1\()_\txfm2\()_4X4_END
1:
.endif

    la.local     t7,    inv_\txfm1\()_4h_x4_lsx
    la.local     t8,    inv_\txfm2\()_4h_x4_lsx

    b            inv_txfm_add_4x4_lsx
.\txfm1\()_\txfm2\()_4X4_END:
endfunc
.endm

fun4x4 dct, dct
fun4x4 identity, identity
fun4x4 adst, dct
fun4x4 dct, adst
fun4x4 adst, adst
fun4x4 dct, flipadst
fun4x4 flipadst, adst
fun4x4 adst, flipadst
fun4x4 flipadst, dct
fun4x4 flipadst, flipadst
fun4x4 dct, identity
fun4x4 identity, dct
fun4x4 flipadst, identity
fun4x4 identity, flipadst
fun4x4 identity, adst
fun4x4 adst, identity

const iadst8_coeffs_h, align=4
    .short          4076, 401, 3612, 1931
    .short          2598, 3166, 1189, 3920
    .short          2896, 0, 1567, 3784, 0, 0, 0, 0
endconst

.macro inv_adst8_lsx out0, out1, out2, out3, out4, out5, out6, out7, sz
    la.local      t0,       iadst8_coeffs_h

    vldrepl.h     vr20,     t0,       0     // 4076
    vldrepl.h     vr21,     t0,       2     // 401
    vmulev_vmaddod_lsx vr7, vr0, vr20, vr21, vr16, vr17, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr7, vr0, vr21, vr20, vr18, vr19, \sz
    vssrarni.h.w  vr17,     vr16,     12    // t0a
    vssrarni.h.w  vr19,     vr18,     12    // t1a

    vldrepl.h     vr20,     t0,       4     // 3612
    vldrepl.h     vr21,     t0,       6     // 1931
    vmulev_vmaddod_lsx vr5, vr2, vr20, vr21, vr0, vr16, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr5, vr2, vr21, vr20, vr7, vr18, \sz
    vssrarni.h.w  vr16,     vr0,      12    // t2a
    vssrarni.h.w  vr18,     vr7,      12    // t3a

    vldrepl.h     vr20,     t0,       8     // 2598
    vldrepl.h     vr21,     t0,       10    // 3166
    vmulev_vmaddod_lsx vr3, vr4, vr20, vr21, vr2, vr0, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr3, vr4, vr21, vr20, vr5, vr7, \sz
    vssrarni.h.w  vr0,      vr2,      12    // t4a
    vssrarni.h.w  vr7,      vr5,      12    // t5a

    vldrepl.h     vr20,     t0,       12    // 1189
    vldrepl.h     vr21,     t0,       14    // 3920
    vmulev_vmaddod_lsx vr1, vr6, vr20, vr21, vr3, vr2, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr1, vr6, vr21, vr20, vr4, vr5, \sz
    vssrarni.h.w  vr2,      vr3,      12    // t6a
    vssrarni.h.w  vr5,      vr4,      12    // t7a

    vsadd.h       vr3,      vr17,     vr0   // t0
    vssub.h       vr4,      vr17,     vr0   // t4
    vsadd.h       vr1,      vr19,     vr7   // t1
    vssub.h       vr6,      vr19,     vr7   // t5
    vsadd.h       vr17,     vr16,     vr2   // t2
    vssub.h       vr19,     vr16,     vr2   // t6
    vsadd.h       vr0,      vr18,     vr5   // t3
    vssub.h       vr7,      vr18,     vr5   // t7

    la.local      t0,       idct_coeffs_h

    vldrepl.h     vr20,     t0,       4     // 1567
    vldrepl.h     vr21,     t0,       6     // 3784
    vmulev_vmaddod_lsx vr4, vr6, vr21, vr20, vr16, vr5, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr4, vr6, vr20, vr21, vr18, vr2, \sz
    vssrarni.h.w  vr5,      vr16,     12    // t4a
    vssrarni.h.w  vr2,      vr18,     12    // t5a

    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr7, vr19, vr20, vr21, vr4, vr16, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr7, vr19, vr21, vr20, vr6, vr18, \sz
    vssrarni.h.w  vr16,     vr4,      12    // t7a
    vssrarni.h.w  vr18,     vr6,      12    // t6a

    vsadd.h       vr4,      vr5,      vr18  // out1
    vssub.h       vr19,     vr5,      vr18  // t6
    vsadd.h       vr20,     vr1,      vr0   // out7
    vssub.h       vr18,     vr1,      vr0   // t3
    vsadd.h       \out0,    vr3,      vr17  // out0
    vssub.h       vr5,      vr3,      vr17  // t2
    vsadd.h       \out6,    vr2,      vr16  // out6
    vssub.h       vr23,     vr2,      vr16  // t7

    vsllwil.w.h   vr3,      vr20,     0     // out7
    vexth.w.h     \out7,    vr20            // out7
    vsllwil.w.h   vr21,     vr4,      0     // out1
    vexth.w.h     \out1,    vr4             // out1
    vneg.w        vr3,      vr3
    vneg.w        \out7,    \out7
    vneg.w        vr21,     vr21
    vneg.w        \out1,    \out1
    vssrarni.h.w  \out7,    vr3,      0
    vssrarni.h.w  \out1,    vr21,     0

    la.local      t0,       idct_coeffs_h

    vldrepl.h     vr20,     t0,       0     // 2896
    vmulev_vmaddod_lsx vr5, vr18, vr20, vr20, vr16, \out3, \sz
    vneg.h        vr21,     vr20
    vmulev_vmaddod_lsx vr5, vr18, vr20, vr21, vr17, \out4, \sz
    vsrari.w      vr16,     vr16,     12
    vsrari.w      \out3,    \out3,    12
    vneg.w        vr16,     vr16
    vneg.w        \out3,    \out3
    vssrarni.h.w  \out3,    vr16,     0     // out3
    vssrarni.h.w  \out4,    vr17,     12    // out4

    vmulev_vmaddod_lsx vr19, vr23, vr20, vr20, vr16, \out2, \sz
    vmulev_vmaddod_lsx vr19, vr23, vr20, vr21, vr17, \out5, \sz
    vssrarni.h.w  \out2,    vr16,     12    // out2
    vsrari.w      vr17,     vr17,     12
    vsrari.w      \out5,    \out5,    12
    vneg.w        vr17,     vr17
    vneg.w        \out5,    \out5
    vssrarni.h.w  \out5,    vr17,     0     // out5
.endm

functionl inv_adst_8h_x8_lsx
    inv_adst8_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, .8h
endfuncl

functionl inv_flipadst_8h_x8_lsx
    inv_adst8_lsx vr7, vr6, vr5, vr4, vr3, vr2, vr1, vr0, .8h
endfuncl

functionl inv_adst_4h_x8_lsx
    inv_adst8_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, .8h
endfuncl

functionl inv_flipadst_4h_x8_lsx
    inv_adst8_lsx vr7, vr6, vr5, vr4, vr3, vr2, vr1, vr0, .8h
endfuncl

.macro inv_dct8_lsx in0, in1, in2, in3, in4, in5, in6, in7, sz
    inv_dct4_lsx \in0, \in2, \in4, \in6, \in0, \in2, \in4, \in6, \sz

    la.local      t0,       idct_coeffs_h

    vldrepl.h     vr20,     t0,       8        // 799
    vldrepl.h     vr21,     t0,       10       // 4017
    vmulev_vmaddod_lsx  \in1, \in7, vr21, vr20, vr16, vr17, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx  \in1, \in7, vr20, vr21, vr18, vr19, \sz
    vssrarni.h.w  vr17,     vr16,     12       // t7a
    vssrarni.h.w  vr19,     vr18,     12       // t4a

    vldrepl.h     vr20,     t0,       12       // 3406
    vldrepl.h     vr21,     t0,       14       // 2276
    vmulev_vmaddod_lsx  \in5, \in3, vr21, vr20, \in1, vr16, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx  \in5, \in3, vr20, vr21, \in7, vr18, \sz
    vssrarni.h.w  vr16,     \in1,       12      // t6a
    vssrarni.h.w  vr18,     \in7,       12      // t5a

    vssub.h       \in7,     vr19,      vr18     // t5a
    vsadd.h       vr18,     vr19,      vr18     // t4
    vssub.h       \in5,     vr17,      vr16     // t6a
    vsadd.h       vr16,     vr17,      vr16     // t7

    vldrepl.h     vr20,     t0,        0        // 2896
    vmulev_vmaddod_lsx  \in5, \in7, vr20, vr20, \in1, vr17, \sz
    vneg.h        vr21,     vr20
    vmulev_vmaddod_lsx  \in5, \in7, vr20, vr21, vr23, vr19, \sz
    vssrarni.h.w  vr17,     \in1,      12       // t6
    vssrarni.h.w  vr19,     vr23,      12       // t5

    vssub.h       \in7,      \in0,     vr16     //c[7]
    vsadd.h       \in0,      \in0,     vr16     //c[0]
    vssub.h       \in5,      \in4,     vr19     //c[5]
    vsadd.h       vr23,      \in4,     vr19     //c[2]
    vssub.h       \in4,      \in6,     vr18     //c[4]
    vsadd.h       \in3,      \in6,     vr18     //c[3]
    vssub.h       \in6,      \in2,     vr17     //c[6]
    vsadd.h       \in1,      \in2,     vr17     //c[1]
    vor.v         \in2,      vr23,     vr23
.endm

functionl inv_dct_8h_x8_lsx
    inv_dct8_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, .8h
endfuncl

functionl inv_dct_4h_x8_lsx
    inv_dct8_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, .4h
endfuncl

.macro DST_ADD_W8 in0, in1, in2, in3, in4, in5, in6, in7
    vsllwil.hu.bu vr0,      \in0,     0
    vsllwil.hu.bu vr1,      \in1,     0
    vsllwil.hu.bu vr2,      \in2,     0
    vsllwil.hu.bu vr3,      \in3,     0
    vadd.h        vr0,      \in4,     vr0
    vadd.h        vr1,      \in5,     vr1
    vadd.h        vr2,      \in6,     vr2
    vadd.h        vr3,      \in7,     vr3
    vssrani.bu.h  vr1,      vr0,      0
    vssrani.bu.h  vr3,      vr2,      0
    vstelm.d      vr1,      a0,       0,    0
    vstelmx.d     vr1,      a0,       a1,   1
    vstelmx.d     vr3,      a0,       a1,   0
    vstelmx.d     vr3,      a0,       a1,   1
.endm

.macro VLD_DST_ADD_W8 in0, in1, in2, in3
    vld           vr0,      a0,       0
    vldx          vr1,      a0,       a1
    vld           vr2,      t2,       0
    vldx          vr3,      t2,       a1

    DST_ADD_W8 vr0, vr1, vr2, vr3, \in0, \in1, \in2, \in3
.endm

functionl inv_identity_8h_x8_lsx
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsadd.h       \i,       \i,       \i
.endr
endfuncl

functionl inv_identity_4h_x8_lsx
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsadd.h       \i,       \i,       \i
.endr
endfuncl

.macro def_fn_8x8_base variant
functionl inv_txfm_\variant\()add_8x8_lsx
    vxor.v  vr23, vr23, vr23
    vld_x8 a2, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
.irp i, 0, 16, 32, 48, 64, 80, 96, 112
    vst           vr23,     a2,       \i
.endr

.ifc \variant, identity_
    // The identity shl #1 and downshift srshr #1 cancel out
    b             .itx_8x8_epilog
.else

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsrari.h      \i,       \i,       1
.endr

.itx_8x8_epilog:
    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vsrari_h_x8 vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23, 4

    alsl.d        t2,       a1,       a0,     1
    VLD_DST_ADD_W8 vr16, vr17, vr18, vr19
    add.d         a0,       a0,       a1
    alsl.d        t2,       a1,       a0,     1
    VLD_DST_ADD_W8 vr20, vr21, vr22, vr23
.endif
endfuncl
.endm

def_fn_8x8_base identity_
def_fn_8x8_base

.macro fn8x8 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_8x8_8bpc_lsx
.ifc \txfm1\()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_8x8

    idct_dc 8, 8, 1

    DST_ADD_W8 vr10, vr11, vr12, vr13, vr20, vr20, vr20, vr20

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,     1
    VLD_DST_ADD_W8 vr20, vr20, vr20, vr20

    b             .\txfm1\()_\txfm2\()_8X8_END
.NO_HAS_DCONLY_8x8:
.endif
    la.local      t8,       inv_\txfm2\()_8h_x8_lsx
.ifc \txfm1, identity
    b             inv_txfm_identity_add_8x8_lsx
.else
    la.local      t7,       inv_\txfm1\()_8h_x8_lsx
    b             inv_txfm_add_8x8_lsx
.endif
.\txfm1\()_\txfm2\()_8X8_END:
endfunc
.endm

fn8x8 dct, dct
fn8x8 identity, identity
fn8x8 dct, adst
fn8x8 dct, flipadst
fn8x8 dct, identity
fn8x8 adst, dct
fn8x8 adst, adst
fn8x8 adst, flipadst
fn8x8 flipadst, dct
fn8x8 flipadst, adst
fn8x8 flipadst, flipadst
fn8x8 identity, dct
fn8x8 adst, identity
fn8x8 flipadst, identity
fn8x8 identity, adst
fn8x8 identity, flipadst

.macro rect2_lsx in0, in1, out0
    vsllwil.w.h   vr22,     \in0,     0     // in1
    vexth.w.h     \in0,     \in0            // in1
    vmul.w        vr22,     vr22,     \in1
    vmul.w        \out0,    \in0,     \in1
    vssrarni.h.w  \out0,    vr22,     12
.endm

.macro LSX_TRANSPOSE8x4_H in0, in1, in2, in3, in4, in5, in6, in7, out0, out1, \
                          out2, out3, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5
    vilvl.h       \tmp0,    \in1,     \in0
    vilvl.h       \tmp1,    \in3,     \in2
    vilvl.w       \tmp2,    \tmp1,    \tmp0
    vilvh.w       \tmp3,    \tmp1,    \tmp0
    vilvl.h       \tmp0,    \in5,     \in4
    vilvl.h       \tmp1,    \in7,     \in6
    vilvl.w       \tmp4,    \tmp1,    \tmp0
    vilvh.w       \tmp5,    \tmp1,    \tmp0
    vilvl.d       \out0,    \tmp4,    \tmp2
    vilvh.d       \out1,    \tmp4,    \tmp2
    vilvl.d       \out2,    \tmp5,    \tmp3
    vilvh.d       \out3,    \tmp5,    \tmp3
.endm

functionl inv_txfm_add_8x4_lsx
    vxor.v        vr23,     vr23,     vr23
    vld           vr0,      a2,       0
    vld           vr2,      a2,       16
    vld           vr4,      a2,       32
    vld           vr6,      a2,       48
.irp i, 0, 16, 32, 48
    vst           vr23,     a2,       \i
.endr

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
    rect2_lsx     vr0,      vr23,     vr0
    rect2_lsx     vr2,      vr23,     vr2
    rect2_lsx     vr4,      vr23,     vr4
    rect2_lsx     vr6,      vr23,     vr6

    vilvh.d       vr1,      vr0,      vr0
    vilvh.d       vr3,      vr2,      vr2
    vilvh.d       vr5,      vr4,      vr4
    vilvh.d       vr7,      vr6,      vr6

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

    LSX_TRANSPOSE8x4_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, vr0, vr1, \
                       vr2, vr3, vr16, vr17, vr18, vr19, vr20, vr21

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vsrari_h_x4 vr0, vr1, vr2, vr3, vr16, vr17, vr18, vr19, 4

    alsl.d        t2,       a1,       a0,     1
    VLD_DST_ADD_W8 vr16, vr17, vr18, vr19
endfuncl

.macro LSX_TRANSPOSE4x8_H in0, in1, in2, in3, out0, out1, out2, out3, out4, \
                          out5, out6, out7, tmp0, tmp1, tmp2, tmp3
    vilvl.h       \tmp0,    \in1,     \in0
    vilvl.h       \tmp1,    \in3,     \in2
    vilvh.h       \tmp2,    \in1,     \in0
    vilvh.h       \tmp3,    \in3,     \in2
    vilvl.w       \out0,    \tmp1,    \tmp0
    vilvh.w       \out2,    \tmp1,    \tmp0
    vilvl.w       \out4,    \tmp3,    \tmp2
    vilvh.w       \out6,    \tmp3,    \tmp2

    vbsrl.v       \out1,    \out0,    8
    vbsrl.v       \out3,    \out2,    8
    vbsrl.v       \out5,    \out4,    8
    vbsrl.v       \out7,    \out6,    8
    vinsgr2vr.d   \out0,    zero,     1
    vinsgr2vr.d   \out2,    zero,     1
    vinsgr2vr.d   \out4,    zero,     1
    vinsgr2vr.d   \out6,    zero,     1
.endm

functionl inv_txfm_add_4x8_lsx
    vxor.v        vr23,     vr23,     vr23
    vld           vr0,      a2,       0
    vld           vr1,      a2,       16
    vld           vr2,      a2,       32
    vld           vr3,      a2,       48
.irp i, 0, 16, 32, 48
    vst           vr23,     a2,       \i
.endr

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
    rect2_lsx     vr0,      vr23,     vr0
    rect2_lsx     vr1,      vr23,     vr1
    rect2_lsx     vr2,      vr23,     vr2
    rect2_lsx     vr3,      vr23,     vr3

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

    LSX_TRANSPOSE4x8_H vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3, vr4, vr5, \
                       vr6, vr7, vr16, vr17, vr18, vr19

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vilvl.d       vr0,      vr1,      vr0
    vilvl.d       vr1,      vr3,      vr2
    vilvl.d       vr2,      vr5,      vr4
    vilvl.d       vr3,      vr7,      vr6

    vsrari_h_x4 vr0, vr1, vr2, vr3, vr16, vr17, vr18, vr19, 4

    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W4 vr16, vr17
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W4 vr18, vr19
endfuncl

.macro fn8x4 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_8x4_8bpc_lsx
.ifc \txfm1()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_8x4

    idct_dc 8, 4, 0

    DST_ADD_W8 vr10, vr11, vr12, vr13, vr5, vr5, vr5, vr5

    b             .\txfm1\()_\txfm2\()_8X4_END
.NO_HAS_DCONLY_8x4:
.endif
    la.local      t7,       inv_\txfm1\()_4h_x8_lsx
    la.local      t8,       inv_\txfm2\()_8h_x4_lsx
    b             inv_txfm_add_8x4_lsx
.\txfm1\()_\txfm2\()_8X4_END:
endfunc
.endm

fn8x4 dct, dct
fn8x4 identity, identity
fn8x4 dct, adst
fn8x4 dct, flipadst
fn8x4 dct, identity
fn8x4 adst, dct
fn8x4 adst, adst
fn8x4 adst, flipadst
fn8x4 flipadst, dct
fn8x4 flipadst, adst
fn8x4 flipadst, flipadst
fn8x4 identity, dct
fn8x4 adst, identity
fn8x4 flipadst, identity
fn8x4 identity, adst
fn8x4 identity, flipadst

.macro fn4x8 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_4x8_8bpc_lsx
.ifc \txfm1()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_4x8

    idct_dc 4, 8, 0

    DST_ADD_W4 vr10, vr11, vr12, vr13, vr20, vr20

    add.d         a0,       a0,       a1
    alsl.d        t2,       a1,       a0,   1
    VLD_DST_ADD_W4 vr5, vr5
    b             .\txfm1\()_\txfm2\()_4X8_END
.NO_HAS_DCONLY_4x8:
.endif
    la.local      t7,       inv_\txfm1\()_8h_x4_lsx
    la.local      t8,       inv_\txfm2\()_4h_x8_lsx
    b             inv_txfm_add_4x8_lsx
.\txfm1\()_\txfm2\()_4X8_END:
endfunc
.endm

fn4x8 dct, dct
fn4x8 identity, identity
fn4x8 dct, adst
fn4x8 dct, flipadst
fn4x8 dct, identity
fn4x8 adst, dct
fn4x8 adst, adst
fn4x8 adst, flipadst
fn4x8 flipadst, dct
fn4x8 flipadst, adst
fn4x8 flipadst, flipadst
fn4x8 identity, dct
fn4x8 adst, identity
fn4x8 flipadst, identity
fn4x8 identity, adst
fn4x8 identity, flipadst

.macro inv_identity4_lsx_x2 in0, in1, in2, in3, in4, out0, out1
    vsllwil.w.h   vr4,      \in0,    0
    vexth.w.h     vr5,      \in0
    vsllwil.w.h   vr6,      \in1,    0
    vexth.w.h     vr7,      \in1
    vmul.w        vr4,      vr4,     \in2
    vmul.w        vr5,      vr5,     \in2
    vmul.w        vr6,      vr6,     \in2
    vmul.w        vr7,      vr7,     \in2
    vssrarni.h.w  vr5,      vr4,     12
    vssrarni.h.w  vr7,      vr6,     12
    vsadd.h       \out0,    vr5,     \in3
    vsadd.h       \out1,    vr7,     \in4
.endm

.macro vmul_vmadd_w in0, in1, in2, in3, out0, out1
    vsllwil.w.h   vr22,     \in0,     0
    vexth.w.h     vr23,     \in0
    vmul.w        \out0,    vr22,     \in2
    vmul.w        \out1,    vr23,     \in2
    vsllwil.w.h   vr22,     \in1,     0
    vexth.w.h     vr23,     \in1
    vmadd.w       \out0,    vr22,     \in3
    vmadd.w       \out1,    vr23,     \in3
.endm

.macro vmul_vmsub_w in0, in1, in2, in3, out0, out1
    vsllwil.w.h   vr22,     \in0,     0
    vexth.w.h     vr23,     \in0
    vmul.w        \out0,    vr22,     \in2
    vmul.w        \out1,    vr23,     \in2
    vsllwil.w.h   vr22,     \in1,     0
    vexth.w.h     vr23,     \in1
    vmsub.w       \out0,    vr22,     \in3
    vmsub.w       \out1,    vr23,     \in3
.endm

.macro inv_dct16_lsx sz
    inv_dct8_lsx vr0, vr2, vr4, vr6, vr8, vr10, vr12, vr14, \sz

    la.local      t0,       idct_coeffs_h
    vldrepl.h     vr20,     t0,       16        // 401
    vldrepl.h     vr21,     t0,       18        // 4076
    vmulev_vmaddod_lsx vr1, vr15, vr21, vr20, vr16, vr17, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr1, vr15, vr20, vr21, vr18, vr19, \sz
    vssrarni.h.w  vr17,     vr16,     12        // t15a
    vssrarni.h.w  vr19,     vr18,     12        // t8a
    vldrepl.h     vr20,     t0,       20        // 3166 -> 1583
    vldrepl.h     vr21,     t0,       22        // 2598 -> 1299
    vmulev_vmaddod_lsx vr9, vr7, vr21, vr20, vr1, vr16, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr9, vr7, vr20, vr21, vr15, vr18, \sz
    vssrarni.h.w  vr16,     vr1,      12        // t14a
    vssrarni.h.w  vr18,     vr15,     12        // t9a
    vldrepl.h     vr20,     t0,       24        // 1931
    vldrepl.h     vr21,     t0,       26        // 3612
    vmulev_vmaddod_lsx vr5, vr11, vr21, vr20, vr7, vr1, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr5, vr11, vr20, vr21, vr9, vr15, \sz
    vssrarni.h.w  vr1,      vr7,      12        // t13a
    vssrarni.h.w  vr15,     vr9,      12        // t10a
    vldrepl.h     vr20,     t0,       28        // 3920
    vldrepl.h     vr21,     t0,       30        // 1189
    vmulev_vmaddod_lsx vr13, vr3, vr21, vr20, vr5, vr7, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr13, vr3, vr20, vr21, vr11, vr9, \sz
    vssrarni.h.w  vr7,      vr5,      12        // t12a
    vssrarni.h.w  vr9,      vr11,     12        // t11a

    vsadd.h       vr5,      vr19,     vr18     // t8
    vssub.h       vr11,     vr19,     vr18     // t9
    vssub.h       vr3,      vr9,      vr15     // t10
    vsadd.h       vr13,     vr9,      vr15     // t11
    vsadd.h       vr18,     vr7,      vr1      // t12
    vssub.h       vr19,     vr7,      vr1      // t13
    vssub.h       vr9,      vr17,     vr16     // t14
    vsadd.h       vr15,     vr17,     vr16     // t15

    vldrepl.h     vr20,     t0,       4        // 1567
    vldrepl.h     vr21,     t0,       6        // 3784
    vmulev_vmaddod_lsx vr9, vr11, vr21, vr20, vr1, vr16, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr9, vr11, vr20, vr21, vr7, vr17, \sz
    vssrarni.h.w  vr16,     vr1,      12       // t14a
    vssrarni.h.w  vr17,     vr7,      12       // t9a

    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr19, vr3, vr21, vr20, vr9, vr1, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr19, vr3, vr20, vr21, vr11, vr7, \sz
    vneg.w        vr1,      vr1
    vneg.w        vr9,      vr9
    vssrarni.h.w  vr7,      vr11,     12       // t13a
    vssrarni.h.w  vr1,      vr9,      12       // t10a
    vsadd.h       vr9,      vr5,      vr13     // t8a
    vssub.h       vr11,     vr5,      vr13     // t11a
    vssub.h       vr3,      vr15,     vr18     // t12a
    vsadd.h       vr19,     vr15,     vr18     // t15a
    vsadd.h       vr5,      vr17,     vr1      // t9
    vssub.h       vr13,     vr17,     vr1      // t10
    vssub.h       vr15,     vr16,     vr7      // t13
    vsadd.h       vr18,     vr16,     vr7      // t14

    vldrepl.h     vr20,     t0,       0        // 2896
    vmulev_vmaddod_lsx vr15, vr13, vr20, vr20, vr1, vr7, \sz
    vneg.h        vr21,     vr20
    vmulev_vmaddod_lsx vr15, vr13, vr20, vr21, vr17, vr16, \sz
    vssrarni.h.w  vr7,      vr1,      12       // t13a
    vssrarni.h.w  vr16,     vr17,     12       // t10a

    vmulev_vmaddod_lsx vr3, vr11, vr20, vr20, vr13, vr23, \sz
    vmulev_vmaddod_lsx vr3, vr11, vr20, vr21, vr15, vr17, \sz
    vssrarni.h.w  vr23,     vr13,     12       // t12
    vssrarni.h.w  vr17,     vr15,     12       // t11

    vssub.h       vr15,     vr0,     vr19      // c[15]
    vsadd.h       vr0,      vr0,     vr19      // c[0]
    vsadd.h       vr1,      vr2,     vr18      // c[1]
    vssub.h       vr20,     vr2,     vr18      // c[14]
    vsadd.h       vr2,      vr4,     vr7       // c[2]
    vssub.h       vr13,     vr4,     vr7       // c[13]
    vsadd.h       vr3,      vr6,     vr23      // c[3]
    vssub.h       vr21,     vr6,     vr23      // c[12]
    vsadd.h       vr4,      vr8,     vr17      // c[4]
    vssub.h       vr11,     vr8,     vr17      // c[11]
    vsadd.h       vr7,      vr14,    vr9       // c[7]
    vssub.h       vr8,      vr14,    vr9       // c[8]
    vsadd.h       vr6,      vr12,    vr5       // c[6]
    vssub.h       vr9,      vr12,    vr5       // c[9]
    vsadd.h       vr5,      vr10,    vr16      // c[5]
    vssub.h       vr10,     vr10,    vr16      // c[10]
    vor.v         vr14,     vr20,    vr20
    vor.v         vr12,     vr21,    vr21
.endm

functionl inv_dct_8h_x16_lsx
    inv_dct16_lsx .8h
endfuncl

functionl inv_dct_4h_x16_lsx
    inv_dct16_lsx .4h
endfuncl

.macro VLD_DST_ADD_W4_x4 in0, in1, in2, in3, in4, in5, in6 ,in7
    alsl.d        t2,       a1,       a0,    1

    VLD_DST_ADD_W4 \in0, \in1

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W4 \in2, \in3

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W4 \in4, \in5

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W4 \in6, \in7
.endm

.macro def_fn_4x16_base txfm
functionl inv_txfm_\txfm\()add_4x16_lsx
    PUSH_REG
    blt           a3,       t5,       416f
    vld           vr0,      a2,       16
    vld           vr1,      a2,       48
    vld           vr2,      a2,       80
    vld           vr3,      a2,       112
    vxor.v        vr23,     vr23,     vr23
.irp i, 16, 48, 80, 112
    vst           vr23,     a2,       \i
.endr

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

.ifnc \txfm, identity_
    vsrari.h      vr0,      vr0,      1
    vsrari.h      vr1,      vr1,      1
    vsrari.h      vr2,      vr2,      1
    vsrari.h      vr3,      vr3,      1
.endif

    LSX_TRANSPOSE4x8_H vr0, vr1, vr2, vr3, vr8, vr9, vr24, vr25, vr26, \
                       vr27, vr14, vr28, vr10, vr11, vr12, vr13

416:
    bge           a3,       t5,       416416f
.irp i, vr8, vr9, vr24, vr25, vr26, vr27, vr14, vr28
    vxor.v        \i,       \i,       \i
.endr

416416:
    vld           vr0,      a2,       0
    vld           vr1,      a2,       32
    vld           vr2,      a2,       64
    vld           vr3,      a2,       96
    vxor.v        vr23,     vr23,     vr23
.irp i, 0, 32, 64, 96
    vst           vr23,     a2,       \i
.endr

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

.ifnc \txfm, identity_
    vsrari.h      vr0,      vr0,      1
    vsrari.h      vr1,      vr1,      1
    vsrari.h      vr2,      vr2,      1
    vsrari.h      vr3,      vr3,      1
.endif

    LSX_TRANSPOSE4x8_H vr0, vr1, vr2, vr3, vr0, vr1, vr2, vr3, vr4, vr5, \
                       vr6, vr7, vr16, vr17, vr18, vr19

    vor.v         vr10,     vr24,     vr24
    vor.v         vr11,     vr25,     vr25
    vor.v         vr12,     vr26,     vr26
    vor.v         vr13,     vr27,     vr27
    vor.v         vr15,     vr28,     vr28

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vilvl.d       vr16,     vr1,      vr0
    vilvl.d       vr17,     vr3,      vr2
    vilvl.d       vr18,     vr5,      vr4
    vilvl.d       vr19,     vr7,      vr6
    vilvl.d       vr20,     vr9,      vr8
    vilvl.d       vr21,     vr11,     vr10
    vilvl.d       vr22,     vr13,     vr12
    vilvl.d       vr23,     vr15,     vr14

.irp i, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    vsrari.h     \i,       \i,       4
.endr

    VLD_DST_ADD_W4_x4 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    POP_REG
endfuncl
.endm

def_fn_4x16_base identity_
def_fn_4x16_base

.macro fn4x16 txfm1, txfm2, eob_half
function inv_txfm_add_\txfm1\()_\txfm2\()_4x16_8bpc_lsx
.ifc \txfm1()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_4x16

    idct_dc 4, 16, 1

    DST_ADD_W4 vr10, vr11, vr12, vr13, vr5, vr5

.rept 3
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,   1

    VLD_DST_ADD_W4 vr5, vr5
.endr
    b             .\txfm1\()_\txfm2\()_4X16_END

.NO_HAS_DCONLY_4x16:
.endif
    li.w          t5,       \eob_half
    la.local      t7,       inv_\txfm1\()_8h_x4_lsx
.ifc \txfm1, identity
    la.local      t7,       inv_\txfm1\()_8h_x4_lsx1
.endif
    la.local      t8,       inv_\txfm2\()_4h_x16_lsx

.ifc \txfm1, identity
    b             inv_txfm_identity_add_4x16_lsx
.else
    b             inv_txfm_add_4x16_lsx
.endif
.\txfm1\()_\txfm2\()_4X16_END:
endfunc
.endm

fn4x16 dct, dct, 29
fn4x16 identity, identity, 29
fn4x16 dct, adst, 29
fn4x16 dct, flipadst, 29
fn4x16 dct, identity, 8
fn4x16 adst, dct, 29
fn4x16 adst, adst, 29
fn4x16 adst, flipadst, 29
fn4x16 flipadst, dct, 29
fn4x16 flipadst, adst, 29
fn4x16 flipadst, flipadst, 29
fn4x16 identity, dct, 32
fn4x16 adst, identity, 8
fn4x16 flipadst, identity, 8
fn4x16 identity, adst, 32
fn4x16 identity, flipadst, 32

.macro inv_identity16_lsx in0, in1, in2, out0, sz
.ifc \sz, .8h
    vsllwil.w.h   vr16,     \in0,     0
    vexth.w.h     vr17,     \in0
    vmul.w        vr16,     vr16,     \in1
    vmul.w        vr17,     vr17,     \in1
    vsadd.h       \in2,     \in2,     \in2
    vssrarni.h.w  vr17,     vr16,     11
    vsadd.h       \out0,    vr17,     \in2
.else
    vsllwil.w.h   vr16,     \in0,     0
    vmul.w        vr16,     vr16,     \in1
    vsadd.h       \in2,     \in2,     \in2
    vssrarni.h.w  vr16,     vr16,     11
    vsadd.h       \out0,    vr16,     \in2
.endif
.endm

.macro inv_identity16_lsx1 in0, in1, in2, out0
    vsllwil.w.h   vr16,     \in0,     0
    vexth.w.h     vr17,     \in1
    vmul.w        vr18,     vr16,     \in2
    vmul.w        vr19,     vr17,     \in2
    vsrari.w      vr18,     vr18,     11
    vsrari.w      vr19,     vr19,     11
    vslli.w       vr16,     vr16,     1
    vslli.w       vr17,     vr17,     1
    vadd.w        vr16,     vr18,     vr16
    vadd.w        \out0,    vr19,     vr17
    vssrarni.h.w  \out0,    vr16,     1
.endm

functionl inv_identity_8h_x16_lsx
    li.w          t0,       1697
    vreplgr2vr.w  vr20,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, vr8, \
    vr9, vr10, vr11, vr12, vr13, vr14, vr15
    inv_identity16_lsx \i, vr20, \i, \i, .8h
.endr
endfuncl

functionl inv_identity_4h_x16_lsx
    li.w          t0,       1697
    vreplgr2vr.w  vr20,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, vr8, \
    vr9, vr10, vr11, vr12, vr13, vr14, vr15
    inv_identity16_lsx \i, vr20, \i, \i, .4h
.endr
endfuncl

functionl inv_identity_8h_x16_lsx1
    li.w          t0,       1697
    vreplgr2vr.w  vr20,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, vr8, \
    vr9, vr10, vr11, vr12, vr13, vr14, vr15
    inv_identity16_lsx1 \i, \i, vr20, \i
.endr
endfuncl

const iadst16_coeffs_h, align=4
    .short         4091, 201, 3973, 995
    .short         3703, 1751, 3290, 2440
    .short         2751, 3035, 2106, 3513
    .short         1380, 3857, 601, 4052
endconst

.macro inv_adst16_lsx txfm, sz
    la.local      t0,       iadst16_coeffs_h
    vldrepl.h     vr20,     t0,        0        // 4091
    vldrepl.h     vr21,     t0,        2        // 201
    vmulev_vmaddod_lsx vr15, vr0, vr20, vr21, vr16, vr18, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr15, vr0, vr21, vr20, vr17, vr19, \sz
    vssrarni.h.w  vr18,     vr16,      12       // t0
    vssrarni.h.w  vr19,     vr17,      12       // t1
    vldrepl.h     vr20,     t0,        4        // 3973
    vldrepl.h     vr21,     t0,        6        // 995
    vmulev_vmaddod_lsx vr13, vr2, vr20, vr21, vr16, vr0, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr13, vr2, vr21, vr20, vr17, vr15, \sz
    vssrarni.h.w  vr0,      vr16,      12       // t2
    vssrarni.h.w  vr15,     vr17,      12       // t3
    vldrepl.h     vr20,     t0,        8       // 3703
    vldrepl.h     vr21,     t0,        10       // 1751
    vmulev_vmaddod_lsx vr11, vr4, vr20, vr21, vr16, vr2, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr11, vr4, vr21, vr20, vr17, vr13, \sz
    vssrarni.h.w  vr2,      vr16,      12       // t4
    vssrarni.h.w  vr13,     vr17,      12       // t5
    vldrepl.h     vr20,     t0,        12       // 3290 -> 1645
    vldrepl.h     vr21,     t0,        14       // 2440 -> 1220
    vmulev_vmaddod_lsx vr9, vr6, vr20, vr21, vr16, vr4, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr9, vr6, vr21, vr20, vr17, vr11, \sz
    vssrarni.h.w  vr4,      vr16,      12       // t6
    vssrarni.h.w  vr11,     vr17,      12       // t7
    vldrepl.h     vr20,     t0,        16       // 2751
    vldrepl.h     vr21,     t0,        18       // 3035
    vmulev_vmaddod_lsx vr7, vr8, vr20, vr21, vr16, vr6, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr7, vr8, vr21, vr20, vr17, vr9, \sz
    vssrarni.h.w  vr6,      vr16,      12       // t8
    vssrarni.h.w  vr9,      vr17,      12       // t9
    vldrepl.h     vr20,     t0,        20       // 2106
    vldrepl.h     vr21,     t0,        22       // 3513
    vmulev_vmaddod_lsx vr5, vr10, vr20, vr21, vr16, vr7, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr5, vr10, vr21, vr20, vr17, vr8, \sz
    vssrarni.h.w  vr7,      vr16,      12       // t10
    vssrarni.h.w  vr8,      vr17,      12       // t11
    vldrepl.h     vr20,     t0,        24       // 1380
    vldrepl.h     vr21,     t0,        26       // 3857
    vmulev_vmaddod_lsx vr3, vr12, vr20, vr21, vr16, vr5, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr3, vr12, vr21, vr20, vr17, vr10, \sz
    vssrarni.h.w  vr5,      vr16,      12       // t12
    vssrarni.h.w  vr10,     vr17,      12       // t13
    vldrepl.h     vr20,     t0,        28       // 601
    vldrepl.h     vr21,     t0,        30       // 4052
    vmulev_vmaddod_lsx vr1, vr14, vr20, vr21, vr16, vr3, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr1, vr14, vr21, vr20, vr17, vr12, \sz
    vssrarni.h.w  vr3,      vr16,      12       // t14
    vssrarni.h.w  vr12,     vr17,      12       // t15

    vsadd.h       vr1,      vr18,      vr6      // t0a
    vssub.h       vr14,     vr18,      vr6      // t8a
    vsadd.h       vr16,     vr19,      vr9      // t1a
    vssub.h       vr17,     vr19,      vr9      // t9a
    vsadd.h       vr6,      vr0,       vr7      // t2a
    vssub.h       vr18,     vr0,       vr7      // t10a
    vsadd.h       vr9,      vr15,      vr8      // t3a
    vssub.h       vr19,     vr15,      vr8      // t11a
    vsadd.h       vr0,      vr2,       vr5      // t4a
    vssub.h       vr7,      vr2,       vr5      // t12a
    vsadd.h       vr8,      vr13,      vr10     // t5a
    vssub.h       vr15,     vr13,      vr10     // t13a
    vsadd.h       vr2,      vr4,       vr3      // t6a
    vssub.h       vr5,      vr4,       vr3      // t14a
    vsadd.h       vr10,     vr11,      vr12     // t7a
    vssub.h       vr13,     vr11,      vr12     // t15a

    la.local      t0,       idct_coeffs_h

    vldrepl.h     vr20,     t0,        8        // 799
    vldrepl.h     vr21,     t0,        10       // 4017
    vmulev_vmaddod_lsx vr14, vr17, vr21, vr20, vr3, vr11, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr14, vr17, vr20, vr21, vr4, vr12, \sz
    vssrarni.h.w  vr11,     vr3,       12       // t8
    vssrarni.h.w  vr12,     vr4,       12       // t9
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr15, vr7, vr20, vr21, vr3, vr14, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr15, vr7, vr21, vr20, vr4, vr17, \sz
    vssrarni.h.w  vr14,     vr3,       12       // t13
    vssrarni.h.w  vr17,     vr4,       12       // t12
    vldrepl.h     vr20,     t0,        12       // 3406
    vldrepl.h     vr21,     t0,        14       // 2276
    vmulev_vmaddod_lsx vr18, vr19, vr21, vr20, vr3, vr7, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr18, vr19, vr20, vr21, vr4, vr15, \sz
    vssrarni.h.w  vr7,      vr3,       12       // t10
    vssrarni.h.w  vr15,     vr4,       12       // t11
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr13, vr5, vr20, vr21, vr3, vr18, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr13, vr5, vr21, vr20, vr4, vr19, \sz
    vssrarni.h.w  vr18,     vr3,       12       // t15
    vssrarni.h.w  vr19,     vr4,       12       // t14

    vsadd.h       vr5,      vr1,       vr0      // t0
    vssub.h       vr13,     vr1,       vr0      // t4
    vsadd.h       vr3,      vr16,      vr8      // t1
    vssub.h       vr4,      vr16,      vr8      // t5
    vsadd.h       vr0,      vr6,       vr2      // t2
    vssub.h       vr1,      vr6,       vr2      // t6
    vsadd.h       vr8,      vr9,       vr10     // t3
    vssub.h       vr16,     vr9,       vr10     // t7
    vsadd.h       vr2,      vr11,      vr17     // t8a
    vssub.h       vr6,      vr11,      vr17     // t12a
    vsadd.h       vr9,      vr12,      vr14     // t9a
    vssub.h       vr10,     vr12,      vr14     // t13a
    vsadd.h       vr11,     vr7,       vr19     // t10a
    vssub.h       vr17,     vr7,       vr19     // t14a
    vsadd.h       vr12,     vr15,      vr18     // t11a
    vssub.h       vr14,     vr15,      vr18     // t15a

    vldrepl.h     vr20,     t0,        4        // 1567
    vldrepl.h     vr21,     t0,        6       // 3784
    vmulev_vmaddod_lsx vr13, vr4, vr21, vr20, vr7, vr18, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr13, vr4, vr20, vr21, vr15, vr19, \sz
    vssrarni.h.w  vr18,     vr7,       12       // t4a
    vssrarni.h.w  vr19,     vr15,      12       // t5a
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr16, vr1, vr20, vr21, vr7, vr4, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr16, vr1, vr21, vr20, vr15, vr13, \sz
    vssrarni.h.w  vr4,      vr7,       12       // t7a
    vssrarni.h.w  vr13,     vr15,      12       // t6a
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr6, vr10, vr21, vr20, vr7, vr1, \sz
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr6, vr10, vr20, vr21, vr15, vr16, \sz
    vssrarni.h.w  vr1,      vr7,       12       // t12
    vssrarni.h.w  vr16,     vr15,      12       // t13
    vneg.h        vr21,     vr21
    vmulev_vmaddod_lsx vr14, vr17, vr20, vr21, vr7, vr6, \sz
    vneg.h        vr20,     vr20
    vmulev_vmaddod_lsx vr14, vr17, vr21, vr20, vr15, vr10, \sz
    vssrarni.h.w  vr6,      vr7,       12       // t15
    vssrarni.h.w  vr10,     vr15,      12       // t14

    vssub.h       vr17,     vr5,       vr0      // t2a
    vsadd.h       vr14,     vr5,       vr0      // out[0]
    vssub.h       vr7,      vr3,       vr8      // t3a
    vsadd.h       vr15,     vr3,       vr8      // out[15]
    vsllwil.w.h   vr22,     vr15,      0
    vexth.w.h     vr15,     vr15
    vneg.w        vr22,     vr22
    vneg.w        vr15,     vr15
    vssrarni.h.w  vr15,     vr22,      0        // out[15]

    vsadd.h       vr3,      vr19,      vr4      // out[12]
    vssub.h       vr8,      vr19,      vr4      // t7
    vssub.h       vr0,      vr18,      vr13     // t6
    vsadd.h       vr5,      vr18,      vr13     // out[3]
    vsllwil.w.h   vr22,     vr5,       0
    vexth.w.h     vr5,      vr5
    vneg.w        vr22,     vr22
    vneg.w        vr5,      vr5
    vssrarni.h.w  vr5,      vr22,      0        // out[3]

    vsadd.h       vr13,     vr9,       vr12     // out[14]
    vssub.h       vr19,     vr9,       vr12     // t11
    vssub.h       vr4,      vr2,       vr11     // t10
    vsadd.h       vr18,     vr2,       vr11     // out[1]
    vsllwil.w.h   vr22,     vr18,      0
    vexth.w.h     vr18,     vr18
    vneg.w        vr22,     vr22
    vneg.w        vr18,     vr18
    vssrarni.h.w  vr18,     vr22,      0        // out[1]

    vsadd.h       vr2,      vr1,       vr10     // out[2]
    vssub.h       vr11,     vr1,       vr10     // t14a
    vssub.h       vr12,     vr16,      vr6      // t15a
    vsadd.h       vr9,      vr16,      vr6      // out[13]
    vsllwil.w.h   vr22,     vr9,       0
    vexth.w.h     vr9,      vr9
    vneg.w        vr22,     vr22
    vneg.w        vr9,      vr9
    vssrarni.h.w  vr9,      vr22,      0        // out[13]

    vldrepl.h     vr20,     t0,        0        // 2896
    vmulev_vmaddod_lsx vr17, vr7, vr20, vr20, vr6, vr10, \sz
    vneg.h        vr21,     vr20
    vmulev_vmaddod_lsx vr17, vr7, vr20, vr21, vr16, vr1, \sz
    vssrarni.h.w  vr1,      vr16,      12       // out[8]
    vsrari.w      vr6,      vr6,       12
    vsrari.w      vr10,     vr10,      12
    vneg.w        vr6,      vr6
    vneg.w        vr10,     vr10
    vssrarni.h.w  vr10,     vr6,       0        // out[7]
    vmulev_vmaddod_lsx vr0, vr8, vr20, vr21, vr16, vr17, \sz
    vmulev_vmaddod_lsx vr0, vr8, vr20, vr20, vr6, vr7, \sz
    vssrarni.h.w  vr7,      vr6,       12       // out[4]
    vsrari.w      vr16,     vr16,      12
    vsrari.w      vr17,     vr17,      12
    vneg.w        vr16,     vr16
    vneg.w        vr17,     vr17
    vssrarni.h.w  vr17,     vr16,       0        // out[11]

    vmulev_vmaddod_lsx vr4, vr19, vr20, vr21, vr16, vr0, \sz
    vmulev_vmaddod_lsx vr4, vr19, vr20, vr20, vr6, vr8, \sz
    vssrarni.h.w  vr8,      vr6,       12       // out[6]
    vsrari.w      vr16,     vr16,      12
    vsrari.w      vr0,      vr0,       12
    vneg.w        vr16,     vr16
    vneg.w        vr0,      vr0
    vssrarni.h.w  vr0,      vr16,      0    // out[9]

    vmulev_vmaddod_lsx vr11, vr12, vr20, vr20, vr6, vr4, \sz
    vmulev_vmaddod_lsx vr11, vr12, vr20, vr21, vr16, vr19, \sz
    vssrarni.h.w  vr19,     vr16,      12       // out[10]
    vsrari.w      vr6,      vr6,       12
    vsrari.w      vr4,      vr4,       12
    vneg.w        vr6,      vr6
    vneg.w        vr4,      vr4
    vssrarni.h.w  vr4,      vr6,       0        // out[5]

.ifc \txfm, adst
    vor.v         vr12,     vr3,       vr3
    vor.v         vr3,      vr5,       vr5
    vor.v         vr5,      vr4,       vr4
    vor.v         vr4,      vr7,       vr7
    vor.v         vr7,      vr10,      vr10
    vor.v         vr10,     vr19,      vr19
    vor.v         vr6,      vr8,       vr8
    vor.v         vr8,      vr1,       vr1
    vor.v         vr11,     vr17,      vr17
    vor.v         vr20,     vr13,      vr13
    vor.v         vr13,     vr9,       vr9
    vor.v         vr9,      vr0,       vr0
    vor.v         vr0,      vr14,      vr14
    vor.v         vr14,     vr20,      vr20
    vor.v         vr1,      vr18,      vr18
.else
    vor.v         vr6,      vr0,       vr0
    vor.v         vr0,      vr15,      vr15
    vor.v         vr15,     vr14,      vr14
    vor.v         vr14,     vr18,      vr18
    vor.v         vr11,     vr7,       vr7
    vor.v         vr7,      vr1,       vr1
    vor.v         vr1,      vr13,      vr13
    vor.v         vr13,     vr2,       vr2
    vor.v         vr2,      vr9,       vr9
    vor.v         vr9,      vr8,       vr8
    vor.v         vr8,      vr10,      vr10
    vor.v         vr10,     vr4,       vr4
    vor.v         vr4,      vr17,      vr17
    vor.v         vr12,     vr5,       vr5
    vor.v         vr5,      vr19,      vr19
.endif
.endm // inv_adst16_lsx

functionl inv_adst_8h_x16_lsx
    inv_adst16_lsx adst, 8h
endfuncl

functionl inv_flipadst_8h_x16_lsx
    inv_adst16_lsx flipadst, 8h
endfuncl

functionl inv_adst_4h_x16_lsx
    inv_adst16_lsx adst, 4h
endfuncl

functionl inv_flipadst_4h_x16_lsx
    inv_adst16_lsx flipadst, 4h
endfuncl

.macro VLD_DST_ADD_W8_x4 in0, in1, in2, in3, in4, in5, in6, in7, in8, \
                         in9, in10, in11, in12, in13, in14, in15

    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W8 \in0, \in1, \in2, \in3

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W8 \in4, \in5, \in6, \in7

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W8 \in8, \in9, \in10, \in11

    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W8 \in12, \in13, \in14, \in15
.endm

.macro def_base_8x16 txfm1
functionl inv_txfm_\txfm1\()add_8x16_lsx
    blt     a3,    t5,   816f
    vld_x8 a2, 16, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vxor.v        vr23,     vr23,     vr23
.irp i, 16, 48, 80, 112, 144, 176, 208, 240
    vst           vr23,     a2,       \i
.endr

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    rect2_lsx     \i,       vr23,     \i
.endr

.ifc \txfm1, identity_
    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
.else
    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

    vsrari_h_x8 vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, 1

    LSX_TRANSPOSE8x8_H vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
.endif

816:
    bge           a3,       t5,       816816f
.irp i, vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    vxor.v        \i,       \i,       \i
.endr

816816:
    vld_x8 a2, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vxor.v        vr23,     vr23,     vr23
.irp i, 0, 32, 64, 96, 128, 160, 192, 224
    vst           vr23,     a2,       \i
.endr

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    rect2_lsx     \i,       vr23,     \i
.endr

.ifc \txfm1, identity_

.else
    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsrari.h      \i,       \i,       1
.endr
.endif

    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vor.v   vr0, vr0, vr0
    vsrari_h_x8 vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23, 4
    vsrari_h_x8 vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, 4

    VLD_DST_ADD_W8_x4 vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                      vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
endfuncl
.endm

def_base_8x16 identity_
def_base_8x16

.macro DST_ADD_W16 in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, in11
    vsllwil.hu.bu vr4,      \in0,     0
    vexth.hu.bu   vr0,      \in0
    vsllwil.hu.bu vr5,      \in1,     0
    vexth.hu.bu   vr1,      \in1
    vsllwil.hu.bu vr6,      \in2,     0
    vexth.hu.bu   vr2,      \in2
    vsllwil.hu.bu vr7,      \in3,     0
    vexth.hu.bu   vr3,      \in3
    vadd.h        vr4,      vr4,      \in4
    vadd.h        vr0,      vr0,      \in5
    vadd.h        vr5,      vr5,      \in6
    vadd.h        vr1,      vr1,      \in7
    vadd.h        vr6,      vr6,      \in8
    vadd.h        vr2,      vr2,      \in9
    vadd.h        vr7,      vr7,      \in10
    vadd.h        vr3,      vr3,      \in11
    vssrani.bu.h  vr0,      vr4,      0
    vssrani.bu.h  vr1,      vr5,      0
    vssrani.bu.h  vr2,      vr6,      0
    vssrani.bu.h  vr3,      vr7,      0
    vst           vr0,      a0,       0
    vstx          vr1,      a0,       a1
    vst           vr2,      t2,       0
    vstx          vr3,      t2,       a1
.endm

.macro VLD_DST_ADD_W16 in0, in1, in2, in3, in4, in5, in6, in7
    vld           vr0,      a0,       0
    vldx          vr1,      a0,       a1
    vld           vr2,      t2,       0
    vldx          vr3,      t2,       a1
    DST_ADD_W16 vr0, vr1, vr2, vr3, \in0, \in1, \in2, \in3, \
                \in4, \in5, \in6, \in7
.endm

.macro def_fn_16x8 txfm1
functionl inv_txfm_\txfm1\()add_16x8_lsx
    PUSH_REG

    vld_x16 a2, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    vxor.v        vr23,     vr23,     vr23
.irp i, 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, \
    176, 192, 208, 224, 240
    vst           vr23,     a2,       \i
.endr

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
    vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    rect2_lsx     \i,       vr23,     \i
.endr

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

.ifnc \txfm1, identity_
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
    vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    vsrari.h       \i,       \i,       1
.endr
.endif

    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vsrari_h_x8 vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                vr24, vr25, vr26, vr27, vr28, vr29, vr30, vr31, 4

    LSX_TRANSPOSE8x8_H vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vsrari_h_x8 vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, 4

    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W16 vr24, vr8, vr25, vr9, vr26, vr10, vr27, vr11

    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W16 vr28, vr12, vr29, vr13, vr30, vr14, vr31, vr15

    POP_REG
endfuncl
.endm

def_fn_16x8 identity_
def_fn_16x8

.macro fun16x8 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_16x8_8bpc_lsx
.ifc \txfm1\()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_16x8

    idct_dc 16, 8, 1

    DST_ADD_W16 vr10, vr11, vr12, vr13, vr20, vr20, vr20, \
                vr20, vr20, vr20, vr20, vr20

    alsl.d        a0,       a1,       a0,     2
    alsl.d        t2,       a1,       a0,     1
    VLD_DST_ADD_W16 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
    b             .\txfm1\()_\txfm2\()_16x8_END
.NO_HAS_DCONLY_16x8:
.endif

    la.local     t7,    inv_\txfm1\()_8h_x16_lsx
.ifc \txfm1, identity
    la.local     t7,    inv_identity_8h_x16_lsx1
.endif

    la.local     t8,    inv_\txfm2\()_8h_x8_lsx

.ifc \txfm1, identity
    b            inv_txfm_identity_add_16x8_lsx
.else
    b            inv_txfm_add_16x8_lsx
.endif

.\txfm1\()_\txfm2\()_16x8_END:
endfunc
.endm

fun16x8 dct, dct
fun16x8 identity, identity
fun16x8 dct, adst
fun16x8 dct, flipadst
fun16x8 dct, identity
fun16x8 adst, dct
fun16x8 adst, adst
fun16x8 adst, flipadst
fun16x8 flipadst, dct
fun16x8 flipadst, adst
fun16x8 flipadst, flipadst
fun16x8 identity, dct
fun16x8 adst, identity
fun16x8 flipadst, identity
fun16x8 identity, adst
fun16x8 identity, flipadst

.macro fun8x16 txfm1, txfm2, eob_half
function inv_txfm_add_\txfm1\()_\txfm2\()_8x16_8bpc_lsx
.ifc \txfm1\()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_8x16

    idct_dc 8, 16, 1

    DST_ADD_W8 vr10, vr11, vr12, vr13, vr20, vr20, vr20, vr20
.rept 3
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,     1
    VLD_DST_ADD_W8 vr20, vr20, vr20, vr20
.endr

    b             .\txfm1\()_\txfm2\()_8x16_END
.NO_HAS_DCONLY_8x16:
.endif
    li.w         t5,    \eob_half
.ifnc \txfm1, identity
    la.local     t7,    inv_\txfm1\()_8h_x8_lsx
.endif

    la.local     t8,    inv_\txfm2\()_8h_x16_lsx
.ifc \txfm1, identity
    b            inv_txfm_identity_add_8x16_lsx
.else
    b            inv_txfm_add_8x16_lsx
.endif
.\txfm1\()_\txfm2\()_8x16_END:
endfunc
.endm

fun8x16 dct, dct, 43
fun8x16 identity, identity, 43
fun8x16 dct, adst, 43
fun8x16 dct, flipadst, 43
fun8x16 dct, identity, 8
fun8x16 adst, dct, 43
fun8x16 adst, adst, 43
fun8x16 adst, flipadst, 43
fun8x16 flipadst, dct, 43
fun8x16 flipadst, adst, 43
fun8x16 flipadst, flipadst, 43
fun8x16 identity, dct, 64
fun8x16 adst, identity, 8
fun8x16 flipadst, identity, 8
fun8x16 identity, adst, 64
fun8x16 identity, flipadst, 64

functionl inv_txfm_add_16x16_lsx
    malloc_space 512

    addi.d        t1,       sp,       64
    addi.d        t2,       a2,       0
.rept 2
    vld_x16 a2, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vxor.v        vr23,     vr23,     vr23
.irp i, 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, \
    384, 416, 448, 480
    vst           vr23,     a2,       \i
.endr

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6

    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    LSX_TRANSPOSE8x8_H vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
    vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    vsrari.h       \i,       \i,       2
.endr
    vst_x8 t1, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vst_x8 t1, 16, 32, vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    addi.d         t1,       t1,       256
    addi.d         a2,       a2,       16
    blt            a3,       t5,       1616f
.endr

1616:
    bge           a3,       t5,       16161616f
    addi.d        t1,       sp,       320
    vxor.v        vr23,     vr23,     vr23
.irp i, 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224, \
    240
    vst           vr23,     t1,       \i
.endr

16161616:
    addi.d        t1,       sp,       64
.rept 2
    vld_x16 t1, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vst_x16 t1, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    addi.d        t1,       t1,       16
.endr
    alsl.d        t2,       a1,       a0,    1
    addi.d        t1,       sp,       64
.rept 4
    vld_x8 t1, 0, 16, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    vsrari_h_x8 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23, \
                vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23 4
    VLD_DST_ADD_W16 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       a0,    1
    addi.d        t1,       t1,       128
.endr
    free_space 512
endfuncl

.macro fun16x16 txfm1, txfm2, eob_half
function inv_txfm_add_\txfm1\()_\txfm2\()_16x16_8bpc_lsx
.ifc \txfm1\()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_16x16

    idct_dc 16, 16, 2

    DST_ADD_W16 vr10, vr11, vr12, vr13, vr20, vr20, vr20, \
                    vr20, vr20, vr20, vr20, vr20
.rept 3
    alsl.d        a0,       a1,       a0,     2
    alsl.d        t2,       a1,       a0,     1

    VLD_DST_ADD_W16 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
.endr
    b             .\txfm1\()_\txfm2\()_16x16_END
.NO_HAS_DCONLY_16x16:
.endif
    li.w         t5,    \eob_half
    la.local     t7,    inv_\txfm1\()_8h_x16_lsx
    la.local     t8,    inv_\txfm2\()_8h_x16_lsx

    b            inv_txfm_add_16x16_lsx
.\txfm1\()_\txfm2\()_16x16_END:
endfunc
.endm

fun16x16 dct, dct, 36
fun16x16 adst, adst, 36
fun16x16 adst, dct, 36
fun16x16 dct, adst, 36
fun16x16 flipadst, dct, 36
fun16x16 dct, flipadst, 36
fun16x16 adst, flipadst, 36
fun16x16 flipadst, adst, 36

.macro dct_8x32_core_lsx in1, in2, vld_st0, vld_st1, vld_stride, \
                         vst_st0, vst_st1, vst_st2, vst_st3, vst_stride, \
                         transpose8x8, shift
    la.local      t0,       idct_coeffs
    vldrepl.w     vr20,     t0,       64           // 201
    vldrepl.w     vr21,     t0,       68           // 4091
    vmul_vmadd_w vr0, vr30, vr21, vr20, vr8, vr9
    vmul_vmsub_w vr0, vr30, vr20, vr21, vr11, vr10
    vssrarni.h.w  vr9,      vr8,      12           // t31a
    vssrarni.h.w  vr10,     vr11,     12           // t16a
    vldrepl.w     vr20,     t0,       72           // 3035
    vldrepl.w     vr21,     t0,       76           // 2751
    vmul_vmadd_w vr19, vr7, vr21, vr20, vr8, vr0
    vmul_vmsub_w vr19, vr7, vr20, vr21, vr11, vr30
    vssrarni.h.w  vr0,      vr8,      12           // t30a
    vssrarni.h.w  vr30,     vr11,     12           // t17a
    vldrepl.w     vr20,     t0,       80           // 1751
    vldrepl.w     vr21,     t0,       84           // 3703
    vmul_vmadd_w vr4, vr26, vr21, vr20, vr8, vr7
    vmul_vmsub_w vr4, vr26, vr20, vr21, vr11, vr19
    vssrarni.h.w  vr7,      vr8,      12           // t29a
    vssrarni.h.w  vr19,     vr11,     12           // t18a
    vldrepl.w     vr20,     t0,       88           // 3857
    vldrepl.w     vr21,     t0,       92           // 1380
    vmul_vmadd_w vr27, vr3, vr21, vr20, vr8, vr4
    vmul_vmsub_w vr27, vr3, vr20, vr21, vr11, vr26
    vssrarni.h.w  vr4,      vr8,      12           // t28a
    vssrarni.h.w  vr26,     vr11,     12           // t19a
    vldrepl.w     vr20,     t0,       96           // 995
    vldrepl.w     vr21,     t0,       100          // 3973
    vmul_vmadd_w vr2, vr28, vr21, vr20, vr8, vr3
    vmul_vmsub_w vr2, vr28, vr20, vr21, vr11, vr27
    vssrarni.h.w  vr3,      vr8,      12           // t27a
    vssrarni.h.w  vr27,     vr11,     12           // t20a
    vldrepl.w     vr20,     t0,       104          // 3513
    vldrepl.w     vr21,     t0,       108          // 2106
    vmul_vmadd_w vr25, vr5, vr21, vr20, vr8, vr2
    vmul_vmsub_w vr25, vr5, vr20, vr21, vr11, vr28
    vssrarni.h.w  vr2,      vr8,      12           // t26a
    vssrarni.h.w  vr28,     vr11,     12           // t21a
    vldrepl.w     vr20,     t0,       112          // 2440 -> 1220
    vldrepl.w     vr21,     t0,       116          // 3290 -> 1645
    vmul_vmadd_w vr6, vr24, vr21, vr20, vr8, vr5
    vmul_vmsub_w vr6, vr24, vr20, vr21, vr11, vr25
    vssrarni.h.w  vr5,      vr8,      12           // t25a
    vssrarni.h.w  vr25,     vr11,     12           // t22a
    vldrepl.w     vr20,     t0,       120          // 4052
    vldrepl.w     vr21,     t0,       124          // 601
    vmul_vmadd_w vr29, vr1, vr21, vr20, vr8, vr6
    vmul_vmsub_w vr29, vr1, vr20, vr21, vr11, vr24
    vssrarni.h.w  vr6,      vr8,      12           // t24a
    vssrarni.h.w  vr24,     vr11,     12           // t23a

    vsadd.h       vr1,      vr10,     vr30         // t16
    vssub.h       vr29,     vr10,     vr30         // t17
    vssub.h       vr8,      vr26,     vr19         // t18
    vsadd.h       vr31,     vr26,     vr19         // t19
    vsadd.h       vr10,     vr27,     vr28         // t20
    vssub.h       vr30,     vr27,     vr28         // t21
    vssub.h       vr19,     vr24,     vr25         // t22
    vsadd.h       vr26,     vr24,     vr25         // t23
    vsadd.h       vr27,     vr6,      vr5          // t24
    vssub.h       vr28,     vr6,      vr5          // t25
    vssub.h       vr24,     vr3,      vr2          // t26
    vsadd.h       vr25,     vr3,      vr2          // t27
    vsadd.h       vr5,      vr4,      vr7          // t28
    vssub.h       vr6,      vr4,      vr7          // t29
    vssub.h       vr2,      vr9,      vr0          // t30
    vsadd.h       vr3,      vr9,      vr0          // t31

    vldrepl.w     vr20,     t0,       16           // 799
    vldrepl.w     vr21,     t0,       20           // 4017
    vmul_vmadd_w vr2, vr29, vr21, vr20, vr4, vr7
    vmul_vmsub_w vr2, vr29, vr20, vr21, vr11, vr0
    vssrarni.h.w  vr7,      vr4,      12           // t30a
    vssrarni.h.w  vr0,      vr11,     12           // t17a
    vmul_vmadd_w vr6, vr8, vr21, vr20, vr4, vr9
    vneg.w        vr4,      vr4
    vneg.w        vr9,      vr9
    vmul_vmsub_w vr6, vr8, vr20, vr21, vr11, vr2
    vssrarni.h.w  vr9,      vr4,      12           // t18a
    vssrarni.h.w  vr2,      vr11,     12           // t29a
    vldrepl.w     vr20,     t0,       24           // 3406 -> 1703
    vldrepl.w     vr21,     t0,       28           // 2276 -> 1138
    vmul_vmadd_w vr24, vr30, vr21, vr20, vr4, vr29
    vmul_vmsub_w vr24, vr30, vr20, vr21, vr11, vr6
    vssrarni.h.w  vr29,     vr4,      12           // t26a
    vssrarni.h.w  vr6,      vr11,     12           // t21a
    vmul_vmadd_w vr28, vr19, vr21, vr20, vr4, vr8
    vneg.w        vr4,      vr4
    vneg.w        vr8,      vr8
    vmul_vmsub_w vr28, vr19, vr20, vr21, vr11, vr24
    vssrarni.h.w  vr8,      vr4,      12           // t22a
    vssrarni.h.w  vr24,     vr11,     12           // t25a

    vsadd.h       vr4,      vr1,      vr31         // t16a
    vssub.h       vr30,     vr1,      vr31         // t19a
    vsadd.h       vr19,     vr0,      vr9          // t17
    vssub.h       vr28,     vr0,      vr9          // t18
    vssub.h       vr1,      vr26,     vr10         // t20a
    vsadd.h       vr31,     vr26,     vr10         // t23a
    vssub.h       vr0,      vr8,      vr6          // t21
    vsadd.h       vr9,      vr8,      vr6          // t22
    vsadd.h       vr10,     vr27,     vr25         // t24a
    vssub.h       vr26,     vr27,     vr25         // t27a
    vsadd.h       vr6,      vr24,     vr29         // t25
    vssub.h       vr8,      vr24,     vr29         // t26
    vssub.h       vr25,     vr3,      vr5          // t28a
    vsadd.h       vr27,     vr3,      vr5          // t31a
    vssub.h       vr24,     vr7,      vr2          // t29
    vsadd.h       vr29,     vr7,      vr2          // t30

    vldrepl.w     vr20,     t0,       8            // 1567
    vldrepl.w     vr21,     t0,       12           // 3784
    vmul_vmadd_w vr24, vr28, vr21, vr20, vr3, vr5
    vmul_vmsub_w vr24, vr28, vr20, vr21, vr11, vr2
    vssrarni.h.w  vr5,      vr3,      12           // t29a
    vssrarni.h.w  vr2,      vr11,     12           // 18a
    vmul_vmadd_w vr25, vr30, vr21, vr20, vr3, vr7
    vmul_vmsub_w vr25, vr30, vr20, vr21, vr11, vr24
    vssrarni.h.w  vr7,      vr3,      12           // t28
    vssrarni.h.w  vr24,     vr11,     12           // t19
    vmul_vmadd_w vr26, vr1, vr21, vr20, vr3, vr28
    vneg.w        vr3,      vr3
    vneg.w        vr28,     vr28
    vmul_vmsub_w vr26, vr1, vr20, vr21, vr11, vr25
    vssrarni.h.w  vr28,     vr3,      12           // t20
    vssrarni.h.w  vr25,     vr11,     12           // t27
    vmul_vmadd_w vr8, vr0, vr21, vr20, vr3, vr30
    vneg.w        vr3,      vr3
    vneg.w        vr30,     vr30
    vmul_vmsub_w vr8, vr0, vr20, vr21, vr11, vr1
    vssrarni.h.w  vr30,     vr3,      12           // t21a
    vssrarni.h.w  vr1,      vr11,     12           // t26a

    vsadd.h       vr3,      vr4,      vr31         // t16
    vssub.h       vr26,     vr4,      vr31         // t23
    vsadd.h       vr0,      vr19,     vr9          // t17a
    vssub.h       vr8,      vr19,     vr9          // t22a
    vsadd.h       vr4,      vr2,      vr30         // t18
    vssub.h       vr31,     vr2,      vr30         // t21
    vsadd.h       vr9,      vr24,     vr28         // t19a
    vssub.h       vr19,     vr24,     vr28         // t20a
    vssub.h       vr2,      vr27,     vr10         // t24
    vsadd.h       vr30,     vr27,     vr10         // t31
    vssub.h       vr24,     vr29,     vr6          // t25a
    vsadd.h       vr28,     vr29,     vr6          // t30a
    vssub.h       vr10,     vr5,      vr1          // t26
    vsadd.h       vr27,     vr5,      vr1          // t29
    vssub.h       vr6,      vr7,      vr25         // t27a
    vsadd.h       vr29,     vr7,      vr25         // t28a

    vldrepl.w     vr20,     t0,       0            // 2896
    vmul_vmsub_w vr6, vr19, vr20, vr20, vr1, vr5
    vmul_vmadd_w vr6, vr19, vr20, vr20, vr11, vr7
    vssrarni.h.w  vr5,      vr1,      12           // t20
    vssrarni.h.w  vr7,      vr11,     12           // t27
    vmul_vmsub_w vr10, vr31, vr20, vr20, vr1, vr25
    vmul_vmadd_w vr10, vr31, vr20, vr20, vr11, vr6
    vssrarni.h.w  vr25,     vr1,      12           // t21a
    vssrarni.h.w  vr6,      vr11,     12           // t26a
    vmul_vmsub_w vr24, vr8, vr20, vr20, vr1, vr19
    vmul_vmadd_w vr24, vr8, vr20, vr20, vr11, vr10
    vssrarni.h.w  vr19,     vr1,      12           // t22
    vssrarni.h.w  vr10,     vr11,     12           // t25
    vmul_vmsub_w vr2, vr26, vr20, vr20, vr1, vr31
    vmul_vmadd_w vr2, vr26, vr20, vr20, vr11, vr8
    vssrarni.h.w  vr31,     vr1,      12           // t23a
    vssrarni.h.w  vr8,      vr11,     12           // t24a

    // t31 t30a t29 t28a t27 t26a t25 t24a t23a t22 t21a t20 t19a t18 t17a t16
    // vr30 vr28 vr27 vr29 vr7 vr6 vr10 vr8 vr31 vr19 vr25 vr5 vr9 vr4 vr0 vr3
    vld_x8 \in2, \vld_st0, \vld_stride, vr11, vr12, vr13, vr14, vr15, vr16, vr17, vr18

    vsadd.h       vr1,      vr11,     vr30         // c[0]
    vssub.h       vr2,      vr11,     vr30         // c[31]
    vsadd.h       vr24,     vr12,     vr28         // c[1]
    vssub.h       vr26,     vr12,     vr28         // c[30]
    vsadd.h       vr11,     vr13,     vr27         // c[2]
    vssub.h       vr30,     vr13,     vr27         // c[29]
    vsadd.h       vr12,     vr14,     vr29         // c[3]
    vssub.h       vr28,     vr14,     vr29         // c[28]
    vsadd.h       vr13,     vr15,     vr7          // c[4]
    vssub.h       vr27,     vr15,     vr7          // c[27]
    vsadd.h       vr14,     vr16,     vr6          // c[5]
    vssub.h       vr29,     vr16,     vr6          // c[26]
    vsadd.h       vr7,      vr17,     vr10         // c[6]
    vssub.h       vr15,     vr17,     vr10         // c[25]
    vsadd.h       vr6,      vr18,     vr8          // c[7]
    vssub.h       vr16,     vr18,     vr8          // c[24]

.ifnb \transpose8x8
    LSX_TRANSPOSE8x8_H vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6, \
                       vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6, \
                       vr8, vr10, vr17, vr18, vr20, vr21, vr22, vr23
.endif

.ifnb \shift
.irp i, vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6
    vsrari.h      \i,       \i,       \shift
.endr
.endif

    vst_x8 \in1, \vst_st0, \vst_stride, vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6

.ifnb \transpose8x8
    LSX_TRANSPOSE8x8_H vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2, \
                       vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2, \
                       vr8, vr10, vr17, vr18, vr20, vr21, vr22, vr23
.endif

.ifnb \shift
.irp i, vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2
    vsrari.h      \i,       \i,       \shift
.endr
.endif

    vst_x8 \in1, \vst_st1, \vst_stride, vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2

    vld_x8 \in2, \vld_st1, \vld_stride, vr11, vr12, vr13, vr14, vr15, vr16, vr17, vr18

    vsadd.h       vr1,      vr11,     vr31         // c[8]
    vssub.h       vr2,      vr11,     vr31         // c[23]
    vsadd.h       vr24,     vr12,     vr19         // c[9]
    vssub.h       vr26,     vr12,     vr19         // c[22]
    vsadd.h       vr11,     vr13,     vr25         // c[10]
    vssub.h       vr30,     vr13,     vr25         // c[21]
    vsadd.h       vr12,     vr14,     vr5          // c[11]
    vssub.h       vr28,     vr14,     vr5          // c[20]
    vsadd.h       vr13,     vr15,     vr9          // c[12]
    vssub.h       vr27,     vr15,     vr9          // c[19]
    vsadd.h       vr14,     vr16,     vr4          // c[13]
    vssub.h       vr29,     vr16,     vr4          // c[18]
    vsadd.h       vr7,      vr17,     vr0          // c[14]
    vssub.h       vr15,     vr17,     vr0          // c[17]
    vsadd.h       vr6,      vr18,     vr3          // c[15]
    vssub.h       vr16,     vr18,     vr3          // c[16]

.ifnb \transpose8x8
    LSX_TRANSPOSE8x8_H vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6, \
                       vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6, \
                       vr8, vr10, vr17, vr18, vr20, vr21, vr22, vr23
.endif

.ifnb \shift
.irp i, vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6
    vsrari.h      \i,       \i,       \shift
.endr
.endif

    vst_x8 \in1, \vst_st2, \vst_stride, vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6

.ifnb \transpose8x8
    LSX_TRANSPOSE8x8_H vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2, \
                       vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2, \
                       vr8, vr10, vr17, vr18, vr20, vr21, vr22, vr23
.endif

.ifnb \shift
.irp i, vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2
    vsrari.h      \i,       \i,       \shift
.endr
.endif

    vst_x8 \in1, \vst_st3, \vst_stride, vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2
.endm

const eob_32x32
        .short 36, 136, 300, 1024
endconst

const eob_8x32
        .short 43, 107, 171, 256
endconst

const eob_16x32
        .short 36, 151, 279, 512
endconst

.macro DST_ADD_W32 in0, in1, in2, in3, in4, in5, in6, in7
    vsllwil.hu.bu vr4,      vr10,     0
    vsllwil.hu.bu vr5,      vr11,     0
    vsllwil.hu.bu vr6,      vr12,     0
    vsllwil.hu.bu vr7,      vr13,     0
    vexth.hu.bu   vr10,     vr10
    vexth.hu.bu   vr11,     vr11
    vexth.hu.bu   vr12,     vr12
    vexth.hu.bu   vr13,     vr13
    vadd.h        vr4,      vr4,      \in0
    vadd.h        vr10,     vr10,     \in1
    vadd.h        vr5,      vr5,      \in2
    vadd.h        vr11,     vr11,     \in3
    vadd.h        vr6,      vr6,      \in4
    vadd.h        vr12,     vr12,     \in5
    vadd.h        vr7,      vr7,      \in6
    vadd.h        vr13,     vr13,     \in7
    vssrani.bu.h  vr10,     vr4,      0
    vssrani.bu.h  vr11,     vr5,      0
    vssrani.bu.h  vr12,     vr6,      0
    vssrani.bu.h  vr13,     vr7,      0
    vst           vr10,     a0,       0
    vst           vr11,     a0,       16
    vst           vr12,     t2,       0
    vst           vr13,     t2,       16
.endm

.macro idct_dc_w32 w, h, shift
    ld.h          t2,       a2,       0      // dc
    vldi          vr0,      0x8b5            // 181
    vreplgr2vr.w  vr1,      t2
    vldi          vr20,     0x880            // 128
    vmul.w        vr2,      vr0,      vr1    // dc * 181
    st.h          zero,     a2,       0
    add.d         t2,       a0,       a1
    vsrari.w      vr2,      vr2,      8      // (dc * 181 + 128) >> 8
    vld           vr13,     t2,       16

.if (2*\w == \h) || (2*\h == \w)
    vmul.w        vr2,      vr2,      vr0
    vsrari.w      vr2,      vr2,      8
.endif

.if \shift>0
    vsrari.w      vr2,      vr2,      \shift      // (dc + rnd) >> shift
.endif
    vld           vr11,     a0,       16
    vmadd.w       vr20,     vr2,      vr0
    vld           vr12,     t2,       0
    vssrarni.h.w  vr20,     vr20,     12
    vld           vr10,     a0,       0
.endm

function inv_txfm_add_dct_dct_32x8_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_32x8

    idct_dc_w32 32, 8, 2

    DST_ADD_W32 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20

.rept 3
    alsl.d        a0,       a1,       a0,     1
    add.d         t2,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     t2,       0
    vld           vr13,     t2,       16
    DST_ADD_W32 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
.endr
    b             .DCT_DCT_32X8_END
.NO_HAS_DCONLY_32x8:
    malloc_space 512+256

    addi.d        t1,       sp,       64
    addi.d        t2,       a2,       0
    addi.d        t3,       sp,       64
    addi.d        t3,       t3,       512

    vld_x16 t2, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vxor.v        vr31,     vr31,     vr31
    vst_x16 t2, 0, 32, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    inv_dct16_lsx .8h

    vst_x16 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vld_x16 t2, 16, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    vxor.v        vr31,     vr31,     vr31

    vst_x16 t2, 16, 32, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    dct_8x32_core_lsx t1, t3, 0, 128, 16, 0, 48, 16, 32, 64, transpose8x8, 2

    addi.d        t2,       sp,       64
.rept 4
    vld_x8 t2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    inv_dct8_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, .8h

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsrari.h      \i,       \i,       4
.endr

    vst_x8 t2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    addi.d        t2,       t2,       16
.endr

    addi.d        t0,       sp,       64
.rept 4
    add.d         t2,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     t2,       0
    vld           vr13,     t2,       16
    vld_x8 t0, 0, 16, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    DST_ADD_W32 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    alsl.d        a0,       a1,       a0,     1
    addi.d        t0,       t0,       128
.endr
    free_space 512+256
.DCT_DCT_32X8_END:
endfunc

function inv_txfm_add_dct_dct_32x16_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_32x16

    idct_dc_w32 32, 16, 1

    DST_ADD_W32 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20

.rept 7
    alsl.d        a0,       a1,       a0,     1
    add.d         t2,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     t2,       0
    vld           vr13,     t2,       16
    DST_ADD_W32 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
.endr
    b             .DCT_DCT_32X16_END
.NO_HAS_DCONLY_32x16:
    malloc_space 1024+256                            // 32*32*2+512
    addi.d        t1,       sp,       64
    addi.d        t2,       a2,       0
    addi.d        t3,       sp,       64
    addi.d        t3,       t3,       1024
.rept 2
    vld_x16 t2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vxor.v        vr31,     vr31,     vr31
    vst_x16 t2, 0, 64, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
     vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    rect2_lsx   \i, vr23, \i
.endr

    inv_dct16_lsx .8h

    vst_x16 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vld_x16 t2, 32, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    la.local      t0,       idct_coeffs
    vldrepl.w     vr23,     t0,       0        // 2896
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
    vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30
    rect2_lsx \i, vr23, \i
.endr
    vxor.v        vr31,     vr31,     vr31
    vst_x16 t2, 32, 64, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    dct_8x32_core_lsx t1, t3, 0, 128, 16, 0, 48, 16, 32, 64, transpose8x8, 1

    addi.d        t2,       t2,       16
    addi.d        t1,       t1,       512
.endr

    addi.d        t2,       sp,       64
.rept 4
    vld_x16 t2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    inv_dct16_lsx .8h

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
    vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    vsrari.h      \i,       \i,       4
.endr

    vst_x16 t2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    addi.d        t2,       t2,       16
.endr

    addi.d        t0,       sp,       64
.rept 8
    add.d         t2,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     t2,       0
    vld           vr13,     t2,       16
    vld_x8 t0, 0, 16, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    DST_ADD_W32 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    alsl.d        a0,       a1,       a0,     1
    addi.d        t0,       t0,       128
.endr
    free_space 1024+256
.DCT_DCT_32X16_END:
endfunc

function inv_txfm_add_dct_dct_32x32_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_32x32

    idct_dc_w32 32, 32, 2

    DST_ADD_W32 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
.rept 15
    alsl.d        a0,       a1,       a0,     1
    add.d         t2,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     t2,       0
    vld           vr13,     t2,       16
    DST_ADD_W32 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
.endr
    b             .DCT_DCT_32X32_END
.NO_HAS_DCONLY_32x32:
    malloc_space 2560                              // 32*32*2+512

    addi.d        t1,       sp,       64
    addi.d        t2,       a2,       0
    addi.d        t3,       sp,       1024
    addi.d        t3,       t3,       1024
    addi.d        t3,       t3,       64

    la.local      t8,       eob_32x32
.DCT_DCT_EOB_32x32:
    ld.h          t7,       t8,       0
    addi.d        t8,       t8,       2

    vld_x16 t2, 0, 128, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vxor.v        vr31,     vr31,     vr31
    vst_x16 t2, 0, 128, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    inv_dct16_lsx .8h

    vst_x16 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vld_x16 t2, 64, 128, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    vxor.v        vr31,     vr31,     vr31

    vst_x16 t2, 64, 128, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    dct_8x32_core_lsx t1, t3, 0, 128, 16, 0, 48, 16, 32, 64, transpose8x8, 2

    addi.d        t2,       t2,       16
    addi.d        t1,       t1,       512
    bge           a3,       t7,       .DCT_DCT_EOB_32x32

    la.local      t8,       eob_32x32
    vxor.v        vr31,     vr31,     vr31
    ld.h          t7,       t8,       4
    bge           a3,       t7,       .DCT_DCT_EOB_32x32_END   // a3>=t7
    vst_x16 sp, 64+1536, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
        vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d        t1,       sp,       256+64
    vst_x16 t1, 1536, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
        vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    ld.h          t7,       t8,       2
    bge           a3,       t7,       .DCT_DCT_EOB_32x32_END
    vst_x16 sp, 64+1024, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
        vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    vst_x16 t1, 1024, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
        vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    ld.h          t7,       t8,       0
    bge           a3,       t7,       .DCT_DCT_EOB_32x32_END
    vst_x16 sp, 64+512, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
        vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    vst_x16 t1, 512, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
        vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

.DCT_DCT_EOB_32x32_END:
    addi.d        t2,       sp,       64
    addi.d        t1,       sp,       64
.rept 4
    vld_x16 t2, 0, 128, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    inv_dct16_lsx .8h

    vst_x16 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vld_x16 t2, 64, 128, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    dct_8x32_core_lsx t1, t3, 0, 128, 16, 0, 1536, 512, 1024, 64, , 4

    addi.d        t2,       t2,       16
    addi.d        t1,       t1,       16
.endr

    addi.d        t0,       sp,       64
.rept 16
    add.d         t2,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     t2,       0
    vld           vr13,     t2,       16
    vld_x8 t0, 0, 16, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    DST_ADD_W32 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    alsl.d        a0,       a1,       a0,     1
    addi.d        t0,       t0,       128
.endr

    free_space 2560                                // 32*32*2+512
.DCT_DCT_32X32_END:
endfunc

/*
 * temp: vr8, vr9, vr10, vr12, vr20, vr21, vr22, vr23
 */
.macro dct_8x8_tx64_core_lsx in0, in1, in2, in3, in4, in5, in6, in7, out0, \
                             out1, out2, out3, out4, out5, out6, out7, rect2

    la.local      t0,       idct_coeffs

.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      t0,       0        // 2896
.irp i, \in0, \in1, \in2, \in3, \in4, \in5, \in6, \in7
    rect2_lsx \i, vr23, \i
.endr
.endif

    la.local      t0,       idct_coeffs

    vldrepl.w     vr20,     t0,       8            // 1567
    vldrepl.w     vr21,     t0,       12           // 3784
    vsllwil.w.h   vr22,     \in2,     0
    vexth.w.h     vr23,     \in2
    vmul.w        vr8,      vr22,     vr20
    vmul.w        vr10,     vr23,     vr20
    vmul.w        \in2,     vr22,     vr21
    vmul.w        vr9,      vr23,     vr21
    vssrarni.h.w  vr10,     vr8,      12           // t2
    vssrarni.h.w  vr9,      \in2,     12           // t3

    vldrepl.w     vr20,     t0,       0            // 2896
    vsllwil.w.h   vr22,     \in0,     0
    vexth.w.h     vr23,     \in0
    vmul.w        vr8,      vr22,     vr20
    vmul.w        \in2,     vr23,     vr20
    vssrarni.h.w  \in2,     vr8,      12

    vsadd.h       vr8,      \in2,     vr9          // c[0]
    vssub.h       vr9,      \in2,     vr9          // c[3]
    vsadd.h       \in0,     \in2,     vr10         // c[1]
    vssub.h       vr10,     \in2,     vr10         // c[2]

    // inv_dct8_1d_internal_c tx64
    // in1 in3
    vldrepl.w     vr20,     t0,       16           // 799
    vldrepl.w     vr21,     t0,       20           // 4017

    vsllwil.w.h   vr22,     \in1,     0
    vexth.w.h     vr23,     \in1
    vmul.w        \in2,     vr22,     vr21
    vmul.w        \in4,     vr23,     vr21
    vmul.w        \in1,     vr22,     vr20
    vmul.w        \in6,     vr23,     vr20
    vssrarni.h.w  \in4,     \in2,     12           // t7a
    vssrarni.h.w  \in6,     \in1,     12           // t4a

    vldrepl.w     vr20,     t0,       24           // 3406
    vldrepl.w     vr21,     t0,       28           // 2276

    vsllwil.w.h   vr22,     \in3,     0
    vexth.w.h     vr23,     \in3
    vneg.w        vr21,     vr21
    vmul.w        \in2,     vr22,     vr20
    vmul.w        \in1,     vr23,     vr20
    vmul.w        \in3,     vr22,     vr21
    vmul.w        \in7,     vr23,     vr21
    vssrarni.h.w  \in1,     \in2,     12           // t6a
    vssrarni.h.w  \in7,     \in3,     12           // t5a

    vsadd.h       \in3,     \in6,     \in7         // t4
    vssub.h       \in6,     \in6,     \in7         // t5a
    vsadd.h       \in5,     \in4,     \in1         // t7
    vssub.h       \in4,     \in4,     \in1         // t6a

    vldrepl.w     vr20,     t0,       0            // 2896
    vmul_vmadd_w  \in4, \in6, vr20, vr20, vr21, \in1
    vmul_vmsub_w  \in4, \in6, vr20, vr20, \in2, \in7
    vssrarni.h.w  \in1,     vr21,     12           // t6
    vssrarni.h.w  \in7,     \in2,     12           // t5

    vsadd.h       \out0,    vr8,      \in5         // c[0]
    vssub.h       \out7,    vr8,      \in5         // c[7]
    vsadd.h       \out1,    \in0,     \in1         // c[1]
    vssub.h       \out6,    \in0,     \in1         // c[6]
    vsadd.h       \out2,    vr10,     \in7         // c[2]
    vssub.h       \out5,    vr10,     \in7         // c[5]
    vsadd.h       \out3,    vr9,      \in3         // c[3]
    vssub.h       \out4,    vr9,      \in3         // c[4]
.endm

/*
 * input:  in0,  in1,  in2,  in3,  in4,  in5,  in6,  in7       (fixed)
 *         vr0,  vr1,  vr2,  vr3,  vr4,  vr5,  vr6,  vr7
 *         in8,  in9,  in10, in11, in12, in13, in14, in15
 *         vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30
 * output: out0, out1, out2, out3, out4, out5, out6, out7      (fixed)
 *         vr22, vr18, vr17, vr28, vr20, vr14, vr15, vr16
 *         out8, out9, out10, out11, out12, out13, out14, out15
 *         vr27, vr30, vr23,  vr21,  vr29,  vr26,  vr25,  vr24
 */
.macro dct_8x16_tx64_core_lsx rect2
    dct_8x8_tx64_core_lsx vr0, vr2, vr4, vr6, vr19, vr25, vr27, vr29, vr11, \
                          vr12, vr13, vr14, vr15, vr16, vr17, vr18, \rect2

    // in1 in3 in5 in7 in9  in11 in13 in15
    // vr1 vr3 vr5 vr7 vr24 vr26 vr28 vr30
    la.local      t0,       idct_coeffs

.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      t0,       0        // 2896
.irp i, vr1, vr3, vr5, vr7, vr24, vr26, vr28, vr30
    rect2_lsx \i, vr23, \i
.endr
.endif

    vldrepl.w     vr20,     t0,       32           // 401
    vldrepl.w     vr21,     t0,       36           // 4076
    vsllwil.w.h   vr22,     vr1,      0
    vexth.w.h     vr23,     vr1
    vmul.w        vr0,      vr22,     vr21
    vmul.w        vr10,     vr23,     vr21
    vmul.w        vr1,      vr22,     vr20
    vmul.w        vr29,     vr23,     vr20
    vssrarni.h.w  vr10,     vr0,      12           // t15a
    vssrarni.h.w  vr29,     vr1,      12           // t8a

    vldrepl.w     vr20,     t0,       40           // 3166 -> 1583
    vldrepl.w     vr21,     t0,       44           // 2598 -> 1299
    vsllwil.w.h   vr22,     vr7,      0
    vexth.w.h     vr23,     vr7
    vneg.w        vr21,     vr21
    vmul.w        vr0,      vr22,     vr20
    vmul.w        vr30,     vr23,     vr20
    vmul.w        vr7,      vr22,     vr21
    vmul.w        vr31,     vr23,     vr21
    vssrarni.h.w  vr30,     vr0,      12           // t14a
    vssrarni.h.w  vr31,     vr7,      12           // t9a

    vldrepl.w     vr20,     t0,       48           // 1931
    vldrepl.w     vr21,     t0,       52           // 3612
    vsllwil.w.h   vr22,     vr5,      0
    vexth.w.h     vr23,     vr5
    vmul.w        vr0,      vr22,     vr21
    vmul.w        vr24,     vr23,     vr21
    vmul.w        vr5,      vr22,     vr20
    vmul.w        vr25,     vr23,     vr20
    vssrarni.h.w  vr24,     vr0,      12           // t13a
    vssrarni.h.w  vr25,     vr5,      12           // t10a

    vldrepl.w     vr20,     t0,       56           // 3920
    vldrepl.w     vr21,     t0,       60           // 1189
    vsllwil.w.h   vr22,     vr3,      0
    vexth.w.h     vr23,     vr3
    vneg.w        vr21,     vr21
    vmul.w        vr0,      vr22,     vr20
    vmul.w        vr26,     vr23,     vr20
    vmul.w        vr3,      vr22,     vr21
    vmul.w        vr27,     vr23,     vr21
    vssrarni.h.w  vr26,     vr0,      12           // t12a
    vssrarni.h.w  vr27,     vr3,      12           // t11a

    // vr22 vr23 vr30 vr31 vr24 vr25 vr26 vr27
    vsadd.h       vr28,     vr29,      vr31        // t8
    vssub.h       vr19,     vr29,      vr31        // t9
    vssub.h       vr29,     vr27,      vr25        // t10
    vsadd.h       vr9,      vr27,      vr25        // t11
    vsadd.h       vr31,     vr26,      vr24        // t12
    vssub.h       vr25,     vr26,      vr24        // t13
    vssub.h       vr27,     vr10,      vr30        // t14
    vsadd.h       vr24,     vr10,      vr30        // t15

    vldrepl.w     vr20,     t0,       8            // 1567
    vldrepl.w     vr21,     t0,       12           // 3784
    vmul_vmadd_w vr27, vr19, vr21, vr20, vr0, vr26
    vmul_vmsub_w vr27, vr19, vr20, vr21, vr1, vr30
    vssrarni.h.w  vr26,     vr0,       12          // t14a
    vssrarni.h.w  vr30,     vr1,       12          // t9a

    vmul_vmadd_w vr25, vr29, vr21, vr20, vr0, vr19
    vneg.w        vr0,      vr0
    vneg.w        vr19,     vr19
    vmul_vmsub_w vr25, vr29, vr20, vr21, vr1, vr27
    vssrarni.h.w  vr19,     vr0,       12          // t10a
    vssrarni.h.w  vr27,     vr1,       12          // t13a

    vsadd.h       vr25,     vr28,     vr9          // t8a
    vssub.h       vr29,     vr28,     vr9          // t11a
    vssub.h       vr28,     vr24,     vr31         // t12a
    vsadd.h       vr10,     vr24,     vr31         // t15a
    vsadd.h       vr9,      vr30,     vr19         // t9
    vssub.h       vr31,     vr30,     vr19         // t10
    vssub.h       vr30,     vr26,     vr27         // t13
    vsadd.h       vr24,     vr26,     vr27         // t14

    vldrepl.w     vr20,     t0,       0            // 2896
    vmul_vmadd_w vr30, vr31, vr20, vr20, vr0, vr26
    vmul_vmsub_w vr30, vr31, vr20, vr20, vr1, vr27
    vssrarni.h.w  vr26,     vr0,      12           // t13a
    vssrarni.h.w  vr27,     vr1,      12           // t10a

    vmul_vmadd_w vr28, vr29, vr20, vr20, vr0, vr31
    vmul_vmsub_w vr28, vr29, vr20, vr20, vr1, vr30
    vssrarni.h.w  vr31,     vr0,      12           // t12
    vssrarni.h.w  vr30,     vr1,      12           // t11

    // vr11 vr12 ... vr18
    vsadd.h       vr28,     vr14,     vr31         // c[3]
    vssub.h       vr29,     vr14,     vr31         // c[12]
    vsadd.h       vr20,     vr15,     vr30         // c[4]
    vssub.h       vr21,     vr15,     vr30         // c[11]
    vsadd.h       vr14,     vr16,     vr27         // c[5]
    vssub.h       vr23,     vr16,     vr27         // c[10]
    vsadd.h       vr15,     vr17,     vr9          // c[6]
    vssub.h       vr30,     vr17,     vr9          // c[9]
    vsadd.h       vr16,     vr18,     vr25         // c[7]
    vssub.h       vr27,     vr18,     vr25         // c[8]
    vsadd.h       vr17,     vr13,     vr26         // c[2]
    vssub.h       vr26,     vr13,     vr26         // c[13]
    vsadd.h       vr18,     vr12,     vr24         // c[1]
    vssub.h       vr25,     vr12,     vr24         // c[14]
    vsadd.h       vr22,     vr11,     vr10         // c[0]
    vssub.h       vr24,     vr11,     vr10         // c[15]
.endm // dct_8x16_tx64_core_lsx

.macro vmul_vssrarni_hw in0, in1, in2, tmp0, tmp1, out0, out1
    vsllwil.w.h   vr22,      \in0,     0
    vexth.w.h     vr23,      \in0
    vmul.w        \tmp0,     vr22,     \in1
    vmul.w        \out0,     vr23,     \in1
    vmul.w        \tmp1,     vr22,     \in2
    vmul.w        \out1,     vr23,     \in2
    vssrarni.h.w  \out0,     \tmp0,    12
    vssrarni.h.w  \out1,     \tmp1,    12
.endm

const idct64_coeffs, align=4
    .word         101, 4095, 2967, -2824
    .word         1660, 3745, 3822, -1474
    .word         4076, 401, 4017, 799
    .word         4036, -700, 2359, 3349
    .word         3461, -2191, 897, 3996
    .word         -3166, -2598, -799, -4017
    .word         501, 4065, 3229, -2520
    .word         2019, 3564, 3948, -1092
    .word         3612, 1931, 2276, 3406
    .word         4085, -301, 2675, 3102
    .word         3659, -1842, 1285, 3889
    .word         -3920, -1189, -3406, -2276
endconst

.macro dct64_step1_lsx
    vldrepl.w     vr20,     t0,       0            // 101
    vldrepl.w     vr21,     t0,       4            // 4095
    vmul_vssrarni_hw vr0, vr20, vr21, vr16, vr0, vr8, vr9    // vr8 t32a vr9 t63a
    vldrepl.w     vr20,     t0,       8            // 2967
    vldrepl.w     vr21,     t0,       12           // -2824
    vmul_vssrarni_hw vr1, vr20, vr21, vr16, vr1, vr10, vr11  // vr10 t62a vr11 t33a
    vldrepl.w     vr20,     t0,       16           // 1660
    vldrepl.w     vr21,     t0,       20           // 3745
    vmul_vssrarni_hw vr2, vr20, vr21, vr16, vr2, vr12, vr13  // vr12 t34a vr13 t61a
    vldrepl.w     vr20,     t0,       24           // 3822
    vldrepl.w     vr21,     t0,       28           // -1474
    vmul_vssrarni_hw vr3, vr20, vr21, vr16, vr3, vr14, vr15  // vr14 t60a vr15 t35a

    vsadd.h       vr0,      vr8,      vr11         // t32
    vssub.h       vr1,      vr8,      vr11         // t33
    vssub.h       vr2,      vr15,     vr12         // t34
    vsadd.h       vr3,      vr15,     vr12         // t35
    vsadd.h       vr4,      vr14,     vr13         // t60
    vssub.h       vr5,      vr14,     vr13         // t61
    vssub.h       vr6,      vr9,      vr10         // t62
    vsadd.h       vr7,      vr9,      vr10         // t63

    vldrepl.w     vr20,     t0,       32           // 4076
    vldrepl.w     vr21,     t0,       36           // 401
    vmul_vmadd_w vr6, vr1, vr20, vr21, vr9, vr10
    vmul_vmsub_w vr6, vr1, vr21, vr20, vr13, vr11
    vssrarni.h.w  vr10,     vr9,      12           // t62a
    vssrarni.h.w  vr11,     vr13,     12           // t33a

    vmul_vmadd_w vr5, vr2, vr20, vr21, vr9, vr1
    vmul_vmsub_w vr5, vr2, vr21, vr20, vr13, vr6
    vneg.w        vr9,      vr9
    vneg.w        vr1,      vr1
    vssrarni.h.w  vr6,      vr13,     12           // t61a
    vssrarni.h.w  vr1,      vr9,      12           // t34a

    vsadd.h       vr2,      vr0,      vr3          // t32a
    vssub.h       vr5,      vr0,      vr3          // t35a
    vsadd.h       vr9,      vr11,     vr1          // t33
    vssub.h       vr13,     vr11,     vr1          // t34
    vssub.h       vr0,      vr7,      vr4          // t60a
    vsadd.h       vr3,      vr7,      vr4          // t63a
    vssub.h       vr1,      vr10,     vr6          // t61
    vsadd.h       vr11,     vr10,     vr6          // t62

    vldrepl.w     vr20,     t0,       40           // 4017
    vldrepl.w     vr21,     t0,       44           // 799
    vmul_vmadd_w vr1, vr13, vr20, vr21, vr8, vr4
    vmul_vmsub_w vr1, vr13, vr21, vr20, vr12, vr7
    vssrarni.h.w  vr4,      vr8,      12           // t61a
    vssrarni.h.w  vr7,      vr12,     12           // t34a

    vmul_vmadd_w vr0, vr5, vr20, vr21, vr8, vr6
    vmul_vmsub_w vr0, vr5, vr21, vr20, vr12, vr10
    vssrarni.h.w  vr6,      vr8,      12           // t60
    vssrarni.h.w  vr10,     vr12,     12           // t35

    vst_x8 t6, 0, 16, vr2, vr9, vr7, vr10, vr6, vr4, vr11, vr3
.endm // dct64_step1

    // in1/31/17/15 -> t32a/33/34a/35/60/61a/62/63a
    // in7/25/23/ 9 -> t56a/57/58a/59/36/37a/38/39a
    // in5/27/21/11 -> t40a/41/42a/43/52/53a/54/55a
    // in3/29/19/13 -> t48a/49/50a/51/44/45a/46/47a
.macro dct64_step2_lsx
    vld           vr0,      t5,       0            // t32a
    vld           vr2,      t4,       0            // t63a
    vld           vr3,      t5,       16*8         // t56a
    vld           vr1,      t4,       16*8         // t39a
    vld           vr4,      t5,       16*16        // t40a
    vld           vr6,      t4,       16*16        // t55a
    vld           vr7,      t5,       16*24        // t48a
    vld           vr5,      t4,       16*24        // t47a

    vsadd.h       vr8,      vr0,      vr1          // t32
    vssub.h       vr9,      vr0,      vr1          // t39
    vsadd.h       vr10,     vr2,      vr3          // t63
    vssub.h       vr11,     vr2,      vr3          // t56
    vssub.h       vr12,     vr5,      vr4          // t40
    vsadd.h       vr13,     vr5,      vr4          // t47
    vsadd.h       vr14,     vr7,      vr6          // t48
    vssub.h       vr15,     vr7,      vr6          // t55
    vldrepl.w     vr20,     t0,       8            // 1567
    vldrepl.w     vr21,     t0,       12           // 3784
    vmul_vmadd_w  vr11, vr9, vr21, vr20, vr0, vr2
    vmul_vmsub_w  vr11, vr9, vr20, vr21, vr1, vr3
    vssrarni.h.w  vr2,      vr0,      12           // t56a
    vssrarni.h.w  vr3,      vr1,      12           // t39a
    vmul_vmadd_w  vr15, vr12, vr21, vr20, vr0, vr4
    vmul_vmsub_w  vr15, vr12, vr20, vr21, vr1, vr5
    vneg.w        vr0,      vr0
    vneg.w        vr4,      vr4
    vssrarni.h.w  vr5,      vr1,      12           // t55a
    vssrarni.h.w  vr4,      vr0,      12           // t40a
    vsadd.h       vr9,      vr8,      vr13         // t32a
    vssub.h       vr11,     vr8,      vr13         // t47a
    vsadd.h       vr6,      vr3,      vr4          // t39
    vssub.h       vr7,      vr3,      vr4          // t40
    vssub.h       vr12,     vr10,     vr14         // t48a
    vsadd.h       vr15,     vr10,     vr14         // t63a
    vssub.h       vr0,      vr2,      vr5          // t55
    vsadd.h       vr1,      vr2,      vr5          // t56

    vldrepl.w     vr20,     t0,       0            // 2896
    vmul_vmsub_w vr0, vr7, vr20, vr20, vr8, vr13
    vmul_vmadd_w vr0, vr7, vr20, vr20, vr3, vr4
    vssrarni.h.w  vr13,     vr8,      12           // t40a
    vssrarni.h.w  vr4,      vr3,      12           // t55a
    vmul_vmsub_w vr12, vr11, vr20, vr20, vr8, vr10
    vmul_vmadd_w vr12, vr11, vr20, vr20, vr3, vr14
    vssrarni.h.w  vr10,     vr8,      12           // t47
    vssrarni.h.w  vr14,     vr3,      12           // t48

    // t32a t39 t40a t47  t48  t55a t56 t63a
    // vr9  vr6 vr13 vr10 vr14 vr4  vr1 vr15
    vst           vr9,      t5,       0            // t32a
    vst           vr6,      t4,       0            // t39
    vst           vr13,     t5,       16*8         // t40a
    vst           vr10,     t4,       16*8         // t47
    vst           vr14,     t5,       16*16        // t48
    vst           vr4,      t4,       16*16        // t55a
    vst           vr1,      t5,       16*24        // t56
    vst           vr15,     t4,       16*24        // t63a
.endm // dct64_step2_lsx

.macro dct64_step3_lsx
    //                t0   t1   t2   t3   t4    t5    t6    t7
    vld_x8 t3, 0, 16, vr2, vr3, vr7, vr8, vr11, vr12, vr16, vr17
    vld           vr9,      t5,       16*24    // t56
    vld           vr6,      t5,       16*24+16 // t57a
    vld           vr13,     t5,       16*24+32 // t58
    vld           vr10,     t5,       16*24+48 // t59a
    vld           vr14,     t4,       16*24-48 // t60
    vld           vr4,      t4,       16*24-32 // t61a
    vld           vr1,      t4,       16*24-16 // t62
    vld           vr15,     t4,       16*24    // t63a
    vsadd.h       vr20,     vr2,      vr15     // c[0]
    vssub.h       vr21,     vr2,      vr15     // c[63]
    vsadd.h       vr22,     vr3,      vr1      // c[1]
    vssub.h       vr23,     vr3,      vr1      // c[62]
    vsadd.h       vr24,     vr7,      vr4      // c[2]
    vssub.h       vr25,     vr7,      vr4      // c[61]
    vsadd.h       vr26,     vr8,      vr14     // c[3]
    vssub.h       vr27,     vr8,      vr14     // c[60]
    vsadd.h       vr28,     vr11,     vr10     // c[4]
    vssub.h       vr29,     vr11,     vr10     // c[59]
    vsadd.h       vr30,     vr12,     vr13     // c[5]
    vssub.h       vr31,     vr12,     vr13     // c[58]
    vsadd.h       vr2,      vr16,     vr6      // c[6]
    vssub.h       vr15,     vr16,     vr6      // c[57]
    vsadd.h       vr1,      vr17,     vr9      // c[7]
    vssub.h       vr3,      vr17,     vr9      // c[56]
.endm // dct64_step3_lsx

.macro dct64_step4_lsx transpose8x8, shift, start0, stride0, start1, stride1
    dct64_step3_lsx

.ifnb \transpose8x8
    LSX_TRANSPOSE8x8_H vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1, \
                       vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1, \
                       vr4, vr7, vr8, vr14, vr10, vr11, vr12, vr13

    LSX_TRANSPOSE8x8_H vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21, \
                       vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21, \
                       vr4, vr7, vr8, vr14, vr10, vr11, vr12, vr13
.endif

.ifnb \shift
.irp i, vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1, \
     vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21
     vsrari.h     \i,       \i,       \shift
.endr
.endif

    vst_x8 t7, \start0, \stride0, vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1

    vst_x8 t7, \start1, \stride1, vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21
.endm // dct64_step4_lsx

.macro dct64_step5_lsx in0, in1, in2, in3, in4, in5, in6, in7
    fld.d         f4,       t0,       0
    fldx.d        f5,       t0,       a1
    fld.d         f6,       t6,       0
    fldx.d        f7,       t6,       a1
    alsl.d        t0,       a1,       t0,    2
    alsl.d        t6,       a1,       t6,    2
    fld.d         f8,       t0,       0
    fldx.d        f9,       t0,       a1
    fld.d         f10,      t6,       0
    fldx.d        f11,      t6,       a1
.irp i, vr4, vr5, vr6, vr7, vr8, vr9, vr10, vr11
    vsllwil.hu.bu   \i,      \i,       0
.endr
    vsrari.h      vr20,     \in0,     4
    vsrari.h      vr22,     \in1,     4
    vsrari.h      vr24,     \in2,     4
    vsrari.h      vr26,     \in3,     4
    vsrari.h      vr28,     \in4,     4
    vsrari.h      vr30,     \in5,     4
    vsrari.h      vr2,      \in6,     4
    vsrari.h      vr1,      \in7,     4
    vadd.h        vr4,      vr4,      vr20
    vadd.h        vr5,      vr5,      vr22
    vadd.h        vr6,      vr6,      vr24
    vadd.h        vr7,      vr7,      vr26
    vadd.h        vr8,      vr8,      vr28
    vadd.h        vr9,      vr9,      vr30
    vadd.h        vr10,     vr10,     vr2
    vadd.h        vr11,     vr11,     vr1
    vssrani.bu.h  vr5,      vr4,      0
    vssrani.bu.h  vr7,      vr6,      0
    vssrani.bu.h  vr9,      vr8,      0
    vssrani.bu.h  vr11,     vr10,     0

    vstelm.d      vr5,      t1,       0,     0
    vstelm.d      vr5,      t2,       0,     1
    alsl.d        t1,       a1,       t1,    1
    alsl.d        t2,       a1,       t2,    1
    vstelm.d      vr7,      t1,       0,     0
    vstelm.d      vr7,      t2,       0,     1
    alsl.d        t1,       a1,       t1,    1
    alsl.d        t2,       a1,       t2,    1
    vstelm.d      vr9,      t1,       0,     0
    vstelm.d      vr9,      t2,       0,     1
    alsl.d        t1,       a1,       t1,    1
    alsl.d        t2,       a1,       t2,    1
    vstelm.d      vr11,     t1,       0,     0
    vstelm.d      vr11,     t2,       0,     1
.endm // dct64_step5_lsx

.macro dct_8x32_tx64_new_lsx vld_loc0, stride0, vld_loc1, stride1, rect2
    vld_x8 t2, \vld_loc0, \stride0, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    dct_8x16_tx64_core_lsx \rect2

    vst_x16 t3, 0, 16, vr22, vr18, vr17, vr28, vr20, vr14, vr15, vr16, \
            vr27, vr30, vr23, vr21, vr29, vr26, vr25, vr24

    vxor.v        vr31,     vr31,     vr31
    vst_x8 t2, \vld_loc0, \stride0, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    vld_x8 t2, \vld_loc1, \stride1, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    vst_x8 t2, \vld_loc1, \stride1, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    la.local      t0,       idct_coeffs

.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      t0,       0        // 2896
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    rect2_lsx \i, vr23, \i
.endr
.endif

    vldrepl.w     vr20,     t0,       64           // 201
    vldrepl.w     vr21,     t0,       68           // 4091
    vsllwil.w.h   vr22,     vr0,      0
    vexth.w.h     vr23,     vr0
    vmul.w        vr8,      vr22,     vr21
    vmul.w        vr9,      vr23,     vr21
    vmul.w        vr0,      vr22,     vr20
    vmul.w        vr10,     vr23,     vr20
    vssrarni.h.w  vr9,      vr8,      12           // t31a
    vssrarni.h.w  vr10,     vr0,      12           // t16a

    vldrepl.w     vr20,     t0,       72           // 3035
    vldrepl.w     vr21,     t0,       76           // 2751
    vsllwil.w.h   vr22,     vr7,      0
    vexth.w.h     vr23,     vr7
    vneg.w        vr21,     vr21
    vmul.w        vr8,      vr22,     vr20
    vmul.w        vr0,      vr23,     vr20
    vmul.w        vr7,      vr22,     vr21
    vmul.w        vr30,     vr23,     vr21
    vssrarni.h.w  vr0,      vr8,      12           // t30a
    vssrarni.h.w  vr30,     vr7,      12           // t17a

    vldrepl.w     vr20,     t0,       80           // 1751
    vldrepl.w     vr21,     t0,       84           // 3703
    vsllwil.w.h   vr22,     vr4,      0
    vexth.w.h     vr23,     vr4
    vmul.w        vr8,      vr22,     vr21
    vmul.w        vr7,      vr23,     vr21
    vmul.w        vr4,      vr22,     vr20
    vmul.w        vr19,     vr23,     vr20
    vssrarni.h.w  vr7,      vr8,      12           // t29a
    vssrarni.h.w  vr19,     vr4,      12           // t18a

    vldrepl.w     vr20,     t0,       88           // 3857
    vldrepl.w     vr21,     t0,       92           // 1380
    vsllwil.w.h   vr22,     vr3,      0
    vexth.w.h     vr23,     vr3
    vneg.w        vr21,     vr21
    vmul.w        vr8,      vr22,     vr20
    vmul.w        vr4,      vr23,     vr20
    vmul.w        vr3,      vr22,     vr21
    vmul.w        vr26,     vr23,     vr21
    vssrarni.h.w  vr4,      vr8,      12           // t28a
    vssrarni.h.w  vr26,     vr3,      12           // t19a

    vldrepl.w     vr20,     t0,       96           // 995
    vldrepl.w     vr21,     t0,       100          // 3973
    vsllwil.w.h   vr22,     vr2,      0
    vexth.w.h     vr23,     vr2
    vmul.w        vr8,      vr22,     vr21
    vmul.w        vr3,      vr23,     vr21
    vmul.w        vr2,      vr22,     vr20
    vmul.w        vr27,     vr23,     vr20
    vssrarni.h.w  vr3,      vr8,      12           // t27a
    vssrarni.h.w  vr27,     vr2,      12           // t20a

    vldrepl.w     vr20,     t0,       104          // 3513
    vldrepl.w     vr21,     t0,       108          // 2106
    vsllwil.w.h   vr22,     vr5,      0
    vexth.w.h     vr23,     vr5
    vneg.w        vr21,     vr21
    vmul.w        vr8,      vr22,     vr20
    vmul.w        vr2,      vr23,     vr20
    vmul.w        vr5,      vr22,     vr21
    vmul.w        vr28,     vr23,     vr21
    vssrarni.h.w  vr2,      vr8,      12           // t26a
    vssrarni.h.w  vr28,     vr5,      12           // t21a

    vldrepl.w     vr20,     t0,       112          // 2440 -> 1220
    vldrepl.w     vr21,     t0,       116          // 3290 -> 1645
    vsllwil.w.h   vr22,     vr6,      0
    vexth.w.h     vr23,     vr6
    vmul.w        vr8,      vr22,     vr21
    vmul.w        vr5,      vr23,     vr21
    vmul.w        vr6,      vr22,     vr20
    vmul.w        vr25,     vr23,     vr20
    vssrarni.h.w  vr5,      vr8,      12           // t25a
    vssrarni.h.w  vr25,     vr6,      12           // t22a

    vldrepl.w     vr20,     t0,       120          // 4052
    vldrepl.w     vr21,     t0,       124          // 601
    vsllwil.w.h   vr22,     vr1,      0
    vexth.w.h     vr23,     vr1
    vneg.w        vr21,     vr21
    vmul.w        vr8,      vr22,     vr20
    vmul.w        vr6,      vr23,     vr20
    vmul.w        vr1,      vr22,     vr21
    vmul.w        vr24,     vr23,     vr21
    vssrarni.h.w  vr6,      vr8,      12           // t24a
    vssrarni.h.w  vr24,     vr1,      12           // t23a

    vsadd.h       vr1,      vr10,     vr30         // t16
    vssub.h       vr29,     vr10,     vr30         // t17
    vssub.h       vr8,      vr26,     vr19         // t18
    vsadd.h       vr31,     vr26,     vr19         // t19
    vsadd.h       vr10,     vr27,     vr28         // t20
    vssub.h       vr30,     vr27,     vr28         // t21
    vssub.h       vr19,     vr24,     vr25         // t22
    vsadd.h       vr26,     vr24,     vr25         // t23
    vsadd.h       vr27,     vr6,      vr5          // t24
    vssub.h       vr28,     vr6,      vr5          // t25
    vssub.h       vr24,     vr3,      vr2          // t26
    vsadd.h       vr25,     vr3,      vr2          // t27
    vsadd.h       vr5,      vr4,      vr7          // t28
    vssub.h       vr6,      vr4,      vr7          // t29
    vssub.h       vr2,      vr9,      vr0          // t30
    vsadd.h       vr3,      vr9,      vr0          // t31

    vldrepl.w     vr20,     t0,       16           // 799
    vldrepl.w     vr21,     t0,       20           // 4017
    vmul_vmadd_w vr2, vr29, vr21, vr20, vr4, vr7
    vmul_vmsub_w vr2, vr29, vr20, vr21, vr11, vr0
    vssrarni.h.w  vr7,      vr4,      12           // t30a
    vssrarni.h.w  vr0,      vr11,     12           // t17a
    vmul_vmadd_w vr6, vr8, vr21, vr20, vr4, vr9
    vneg.w        vr4,      vr4
    vneg.w        vr9,      vr9
    vmul_vmsub_w vr6, vr8, vr20, vr21, vr11, vr2
    vssrarni.h.w  vr9,      vr4,      12           // t18a
    vssrarni.h.w  vr2,      vr11,     12           // t29a

    vldrepl.w     vr20,     t0,       24           // 3406 -> 1703
    vldrepl.w     vr21,     t0,       28           // 2276 -> 1138
    vmul_vmadd_w vr24, vr30, vr21, vr20, vr4, vr29
    vmul_vmsub_w vr24, vr30, vr20, vr21, vr11, vr6
    vssrarni.h.w  vr29,     vr4,      12           // t26a
    vssrarni.h.w  vr6,      vr11,     12           // t21a

    vmul_vmadd_w vr28, vr19, vr21, vr20, vr4, vr8
    vneg.w        vr4,      vr4
    vneg.w        vr8,      vr8
    vmul_vmsub_w vr28, vr19, vr20, vr21, vr11, vr24
    vssrarni.h.w  vr8,      vr4,      12           // t22a
    vssrarni.h.w  vr24,     vr11,     12           // t25a

    vsadd.h       vr4,      vr1,      vr31         // t16a
    vssub.h       vr30,     vr1,      vr31         // t19a
    vsadd.h       vr19,     vr0,      vr9          // t17
    vssub.h       vr28,     vr0,      vr9          // t18
    vssub.h       vr1,      vr26,     vr10         // t20a
    vsadd.h       vr31,     vr26,     vr10         // t23a
    vssub.h       vr0,      vr8,      vr6          // t21
    vsadd.h       vr9,      vr8,      vr6          // t22
    vsadd.h       vr10,     vr27,     vr25         // t24a
    vssub.h       vr26,     vr27,     vr25         // t27a
    vsadd.h       vr6,      vr24,     vr29         // t25
    vssub.h       vr8,      vr24,     vr29         // t26
    vssub.h       vr25,     vr3,      vr5          // t28a
    vsadd.h       vr27,     vr3,      vr5          // t31a
    vssub.h       vr24,     vr7,      vr2          // t29
    vsadd.h       vr29,     vr7,      vr2          // t30

    vldrepl.w     vr20,     t0,       8            // 1567
    vldrepl.w     vr21,     t0,       12           // 3784
    vmul_vmadd_w vr24, vr28, vr21, vr20, vr3, vr5
    vmul_vmsub_w vr24, vr28, vr20, vr21, vr11, vr2
    vssrarni.h.w  vr5,      vr3,      12           // t29a
    vssrarni.h.w  vr2,      vr11,     12           // 18a

    vmul_vmadd_w vr25, vr30, vr21, vr20, vr3, vr7
    vmul_vmsub_w vr25, vr30, vr20, vr21, vr11, vr24
    vssrarni.h.w  vr7,      vr3,      12           // t28
    vssrarni.h.w  vr24,     vr11,     12           // t19

    vmul_vmadd_w vr26, vr1, vr21, vr20, vr3, vr28
    vneg.w        vr3,      vr3
    vneg.w        vr28,     vr28
    vmul_vmsub_w vr26, vr1, vr20, vr21, vr11, vr25
    vssrarni.h.w  vr28,     vr3,      12           // t20
    vssrarni.h.w  vr25,     vr11,     12           // t27

    vmul_vmadd_w vr8, vr0, vr21, vr20, vr3, vr30
    vneg.w        vr3,      vr3
    vneg.w        vr30,     vr30
    vmul_vmsub_w vr8, vr0, vr20, vr21, vr11, vr1
    vssrarni.h.w  vr30,     vr3,      12           // t21a
    vssrarni.h.w  vr1,      vr11,     12           // t26a

    vsadd.h       vr3,      vr4,      vr31         // t16
    vssub.h       vr26,     vr4,      vr31         // t23
    vsadd.h       vr0,      vr19,     vr9          // t17a
    vssub.h       vr8,      vr19,     vr9          // t22a
    vsadd.h       vr4,      vr2,      vr30         // t18
    vssub.h       vr31,     vr2,      vr30         // t21
    vsadd.h       vr9,      vr24,     vr28         // t19a
    vssub.h       vr19,     vr24,     vr28         // t20a
    vssub.h       vr2,      vr27,     vr10         // t24
    vsadd.h       vr30,     vr27,     vr10         // t31
    vssub.h       vr24,     vr29,     vr6          // t25a
    vsadd.h       vr28,     vr29,     vr6          // t30a
    vssub.h       vr10,     vr5,      vr1          // t26
    vsadd.h       vr27,     vr5,      vr1          // t29
    vssub.h       vr6,      vr7,      vr25         // t27a
    vsadd.h       vr29,     vr7,      vr25         // t28a

    vldrepl.w     vr20,     t0,       0            // 2896
    vmul_vmsub_w vr6, vr19, vr20, vr20, vr1, vr5
    vmul_vmadd_w vr6, vr19, vr20, vr20, vr11, vr7
    vssrarni.h.w  vr5,      vr1,      12           // t20
    vssrarni.h.w  vr7,      vr11,     12           // t27

    vmul_vmsub_w vr10, vr31, vr20, vr20, vr1, vr25
    vmul_vmadd_w vr10, vr31, vr20, vr20, vr11, vr6
    vssrarni.h.w  vr25,     vr1,      12           // t21a
    vssrarni.h.w  vr6,      vr11,     12           // t26a

    vmul_vmsub_w vr24, vr8, vr20, vr20, vr1, vr19
    vmul_vmadd_w vr24, vr8, vr20, vr20, vr11, vr10
    vssrarni.h.w  vr19,     vr1,      12           // t22
    vssrarni.h.w  vr10,     vr11,     12           // t25

    vmul_vmsub_w vr2, vr26, vr20, vr20, vr1, vr31
    vmul_vmadd_w vr2, vr26, vr20, vr20, vr11, vr8
    vssrarni.h.w  vr31,     vr1,      12           // t23a
    vssrarni.h.w  vr8,      vr11,     12           // t24a

    // t31 t30a t29 t28a t27 t26a t25 t24a t23a t22 t21a t20 t19a t18 t17a t16
    // vr30 vr28 vr27 vr29 vr7 vr6 vr10 vr8 vr31 vr19 vr25 vr5 vr9 vr4 vr0 vr3
    vld_x8 t3, 0, 16, vr11, vr12, vr13, vr14, vr15, vr16, vr17, vr18

    vsadd.h       vr1,      vr11,     vr30         // c[0]
    vssub.h       vr2,      vr11,     vr30         // c[31]
    vsadd.h       vr24,     vr12,     vr28         // c[1]
    vssub.h       vr26,     vr12,     vr28         // c[30]
    vsadd.h       vr11,     vr13,     vr27         // c[2]
    vssub.h       vr30,     vr13,     vr27         // c[29]
    vsadd.h       vr12,     vr14,     vr29         // c[3]
    vssub.h       vr28,     vr14,     vr29         // c[28]
    vsadd.h       vr13,     vr15,     vr7          // c[4]
    vssub.h       vr27,     vr15,     vr7          // c[27]
    vsadd.h       vr14,     vr16,     vr6          // c[5]
    vssub.h       vr29,     vr16,     vr6          // c[26]
    vsadd.h       vr7,      vr17,     vr10         // c[6]
    vssub.h       vr15,     vr17,     vr10         // c[25]
    vsadd.h       vr6,      vr18,     vr8          // c[7]
    vssub.h       vr16,     vr18,     vr8          // c[24]

    vst_x8 t3, 0, 16, vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6

    vst_x8 t3, 384, 16, vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2

    vld_x8 t3, 128, 16, vr11, vr12, vr13, vr14, vr15, vr16, vr17, vr18

    vsadd.h       vr1,      vr11,     vr31         // c[8]
    vssub.h       vr2,      vr11,     vr31         // c[23]
    vsadd.h       vr24,     vr12,     vr19         // c[9]
    vssub.h       vr26,     vr12,     vr19         // c[22]
    vsadd.h       vr11,     vr13,     vr25         // c[10]
    vssub.h       vr30,     vr13,     vr25         // c[21]
    vsadd.h       vr12,     vr14,     vr5          // c[11]
    vssub.h       vr28,     vr14,     vr5          // c[20]
    vsadd.h       vr13,     vr15,     vr9          // c[12]
    vssub.h       vr27,     vr15,     vr9          // c[19]
    vsadd.h       vr14,     vr16,     vr4          // c[13]
    vssub.h       vr29,     vr16,     vr4          // c[18]
    vsadd.h       vr7,      vr17,     vr0          // c[14]
    vssub.h       vr15,     vr17,     vr0          // c[17]
    vsadd.h       vr6,      vr18,     vr3          // c[15]
    vssub.h       vr16,     vr18,     vr3          // c[16]

    vst_x8 t3, 128, 16, vr1, vr24, vr11, vr12, vr13, vr14, vr7, vr6

    vst_x8 t3, 256, 16, vr16, vr15, vr29, vr27, vr28, vr30, vr26, vr2
.endm // dct_8x32_tx64_new_lsx

.macro DST_ADD_W64 in0, in1, in2, in3, in4, in5, in6, in7
    vsllwil.hu.bu vr4,      vr10,     0
    vsllwil.hu.bu vr5,      vr11,     0
    vsllwil.hu.bu vr6,      vr12,     0
    vsllwil.hu.bu vr7,      vr13,     0
    vexth.hu.bu   vr10,     vr10
    vexth.hu.bu   vr11,     vr11
    vexth.hu.bu   vr12,     vr12
    vexth.hu.bu   vr13,     vr13
    vadd.h        vr4,      vr4,      \in0
    vadd.h        vr10,     vr10,     \in1
    vadd.h        vr5,      vr5,      \in2
    vadd.h        vr11,     vr11,     \in3
    vadd.h        vr6,      vr6,      \in4
    vadd.h        vr12,     vr12,     \in5
    vadd.h        vr7,      vr7,      \in6
    vadd.h        vr13,     vr13,     \in7
    vssrani.bu.h  vr10,     vr4,      0
    vssrani.bu.h  vr11,     vr5,      0
    vssrani.bu.h  vr12,     vr6,      0
    vssrani.bu.h  vr13,     vr7,      0
    vst           vr10,     a0,       0
    vst           vr11,     a0,       16
    vst           vr12,     a0,       32
    vst           vr13,     a0,       48
.endm

.macro idct_dc_w64 w, h, shift
    ld.h          t2,       a2,       0
    vldi          vr0,      0x8b5
    vreplgr2vr.w  vr1,      t2
    vldi          vr20,     0x880
    vmul.w        vr2,      vr0,      vr1
    st.h          zero,     a2,       0
    vsrari.w      vr2,      vr2,      8
    vld           vr13,     a0,       48

.if (2*\w == \h) || (2*\h == \w)
    vmul.w        vr2,      vr2,      vr0
    vsrari.w      vr2,      vr2,      8
.endif

.if \shift>0
    vsrari.w      vr2,      vr2,      \shift
.endif
    vld           vr11,     a0,       16
    vmadd.w       vr20,     vr2,      vr0
    vld           vr12,     a0,       32
    vssrarni.h.w  vr20,     vr20,     12
    vld           vr10,     a0,       0
.endm

function inv_txfm_add_dct_dct_64x64_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_64x64

    idct_dc_w64 64, 64, 2

    DST_ADD_W64 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20

    li.w          t3,       63
.loop63:
    add.d         a0,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     a0,       32
    vld           vr13,     a0,       48
    DST_ADD_W64 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
    addi.d        t3,       t3,       -1
    blt           zero,     t3,       .loop63
    b             .DCT_DCT_64X64_END
.NO_HAS_DCONLY_64x64:

    malloc_space  64*32*2+512+512

.macro dct64x64_core1_lsx shift, rect2
    //addi.d        t2,       a2,       \in0
    //addi.d        t7,       t7,       \in1
    li.w          t4,       64*32*2+64
    add.d         t3,       sp,       t4
    addi.d        t6,       t3,       512
    add.d         t5,       t6,       zero

    dct_8x32_tx64_new_lsx 0, 256, 128, 256, \rect2

    la.local      t0,       idct64_coeffs
    vxor.v        vr31,     vr31,     vr31

    //addi.d        a4,       a2,       \in2         // 32 ...
    // in1/31/17/15 -> t32a/33/34a/35/60/61a/62/63a
    vld           vr0,      a4,       128*0        // in1
    vld           vr1,      a4,       128*15       // in31
    vld           vr2,      a4,       128*8        // in17
    vld           vr3,      a4,       128*7        // in15
    la.local      a6,       idct_coeffs
.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      a6,       0        // 2896
.irp i, vr0, vr1, vr2, vr3
    rect2_lsx \i, vr23, \i
.endr
.endif
    vst           vr31,     a4,       128*0
    vst           vr31,     a4,       128*15
    vst           vr31,     a4,       128*8
    vst           vr31,     a4,       128*7
    dct64_step1_lsx

    addi.d        t0,       t0,       48
    addi.d        t6,       t6,       128
    // in7/25/23/ 9 -> t56a/57/58a/59/36/37a/38/39a
    vld           vr0,      a4,       128*3        // in7
    vld           vr1,      a4,       128*12       // in25
    vld           vr2,      a4,       128*11       // in23
    vld           vr3,      a4,       128*4        // in9
    la.local      a6,       idct_coeffs
.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      a6,       0        // 2896
.irp i, vr0, vr1, vr2, vr3
    rect2_lsx \i, vr23, \i
.endr
.endif
    vst           vr31,     a4,       128*3
    vst           vr31,     a4,       128*12
    vst           vr31,     a4,       128*11
    vst           vr31,     a4,       128*4
    dct64_step1_lsx

    addi.d        t0,       t0,       48
    addi.d        t6,       t6,       128
    // in5/27/21/11 -> t40a/41/42a/43/52/53a/54/55a
    vld           vr0,      a4,       128*2        // in5
    vld           vr1,      a4,       128*13       // in27
    vld           vr2,      a4,       128*10       // in21
    vld           vr3,      a4,       128*5        // in11
    la.local      a6,       idct_coeffs
.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      a6,      0        // 2896
.irp i, vr0, vr1, vr2, vr3
    rect2_lsx \i, vr23, \i
.endr
.endif
    vst           vr31,     a4,       128*2
    vst           vr31,     a4,       128*13
    vst           vr31,     a4,       128*10
    vst           vr31,     a4,       128*5
    dct64_step1_lsx

    addi.d        t0,       t0,       48
    addi.d        t6,       t6,       128
    // in3/29/19/13 -> t48a/49/50a/51/44/45a/46/47a
    vld           vr0,      a4,       128*1        // in3
    vld           vr1,      a4,       128*14       // in29
    vld           vr2,      a4,       128*9        // in19
    vld           vr3,      a4,       128*6        // in13
    la.local      a6,       idct_coeffs
.ifc \rect2, rect2_lsx
    vldrepl.w     vr23,      a6,       0        // 2896
.irp i, vr0, vr1, vr2, vr3
    rect2_lsx \i, vr23, \i
.endr
.endif
    vst           vr31,     a4,       128*1
    vst           vr31,     a4,       128*14
    vst           vr31,     a4,       128*9
    vst           vr31,     a4,       128*6
    dct64_step1_lsx

    la.local      t0,       idct_coeffs
    addi.d        t4,       t5,       16*7
    // t32a/t39/t40a/t47/t48/t55a/t56/t63a
    dct64_step2_lsx

    addi.d        t5,       t5,       16
    addi.d        t4,       t4,       -16
    // t33/t38a/t41/t46a/t49a/t54/t57a/t62
    dct64_step2_lsx

    addi.d        t5,       t5,       16
    addi.d        t4,       t4,       -16
    // t34a/t37/t42a/t45/t50/t53a/t58/t61a
    dct64_step2_lsx

    addi.d        t5,       t5,       16
    addi.d        t4,       t4,       -16
    // t35/t36a/t43/t44a/t51a/t52/t59a/t60
    dct64_step2_lsx

    li.w          t4,       64*32*2+64+512
    add.d         t5,       t4,       sp
    addi.d        t4,       t5,       16*7
    dct64_step4_lsx transpose8x8, \shift, 0, 128, 112, 128

    addi.d        t3,       t3,       128
    addi.d        t4,       t4,       -16*8
    addi.d        t5,       t5,       -16*8
    dct64_step4_lsx transpose8x8, \shift, 16, 128, 96, 128

    addi.d        t5,       t5,       -16*8
    addi.d        t4,       t4,       -16*8
    addi.d        t3,       t3,       128
    dct64_step4_lsx transpose8x8, \shift, 32, 128, 80, 128

    addi.d        t5,       t5,       -16*8
    addi.d        t4,       t4,       -16*8
    addi.d        t3,       t3,       128
    dct64_step4_lsx transpose8x8, \shift, 48, 128, 64, 128
.endm
    la.local      t8,       eob_32x32
    addi.d        t2,       a2,       0
    addi.d        t7,       sp,       64
    addi.d        t7,       t7,       0
    addi.d        a4,       a2,       64
.DCT_DCT_EOB_64x64:
    ld.h          a5,       t8,       0
    addi.d        t8,       t8,       2
    dct64x64_core1_lsx 2, no_rect2
    addi.d        t2,       t2,       16
    addi.d        t7,       t7,       128*8
    addi.d        a4,       a4,       16
    bge           a3,       a5,       .DCT_DCT_EOB_64x64

    la.local      t8,       eob_32x32
    vxor.v        vr31,     vr31,     vr31

    ld.h          t7,       t8,       4
    bge           a3,       t7,       .DCT_DCT_EOB_64x64_END
    li.d          t1,       1024*3+64
    add.d         t0,       sp,       t1
.rept 4
    vst_x16 t0, 0, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d t0, t0, 256
.endr

    ld.h          t7,       t8,       2
    bge           a3,       t7,       .DCT_DCT_EOB_64x64_END
    li.d          t1,       1024*2+64
    add.d         t0,       sp,       t1
.rept 4
    vst_x16 t0, 0, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d        t0,       t0,       256
.endr
    ld.h          t7,       t8,       0
    bge           a3,       t7,       .DCT_DCT_EOB_64x64_END

    li.d          t1,       1024*1+64
    add.d         t0,       sp,       t1
.rept 4
    vst_x16 t0, 0, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d        t0,       t0,       256
.endr

.DCT_DCT_EOB_64x64_END:

.macro dct64x64_core2_lsx in0, in1, rect2
    addi.d        t2,       sp,       64+\in0
    addi.d        t7,       sp,       64+\in0
    li.w          t4,       64*32*2+64
    add.d         t3,       sp,       t4
    addi.d        t6,       t3,       512
    add.d         t5,       t6,       zero

    addi.d        t2,       t2,       1024
    addi.d        t2,       t2,       1024
    dct_8x32_tx64_new_lsx -2048, 512, 256-2048, 512, \rect2

    la.local      t0,       idct64_coeffs
    addi.d        t2,       sp,       64+64*2+\in0
    addi.d        t4,       t2,       256*7
    addi.d        t4,       t4,       256

    vld           vr0,      t2,       256*0        // in1
    vld           vr1,      t4,       256*7        // in31
    vld           vr2,      t4,       256*0        // in17
    vld           vr3,      t2,       256*7        // in15
    dct64_step1_lsx

    addi.d        t0,       t0,       48
    addi.d        t6,       t6,       128
    vld           vr0,      t2,       256*3        // in7
    vld           vr1,      t4,       256*4        // in25
    vld           vr2,      t4,       256*3        // in23
    vld           vr3,      t2,       256*4        // in9
    dct64_step1_lsx

    addi.d        t0,        t0,       48
    addi.d        t6,        t6,       128
    vld           vr0,       t2,       256*2       // in5
    vld           vr1,       t4,       256*5       // in27
    vld           vr2,       t4,       256*2       // in21
    vld           vr3,       t2,       256*5       // in11
    dct64_step1_lsx

    addi.d        t0,        t0,       48
    addi.d        t6,        t6,       128
    vld           vr0,       t2,       256*1       // in3
    vld           vr1,       t4,       256*6       // in29
    vld           vr2,       t4,       256*1       // in19
    vld           vr3,       t2,       256*6       // in13
    dct64_step1_lsx

    la.local      t0,       idct_coeffs
    addi.d        t4,       t5,       16*7
    // t32a/t39/t40a/t47/t48/t55a/t56/t63a
    dct64_step2_lsx

    addi.d        t5,       t5,       16
    addi.d        t4,       t4,       -16
    // t33/t38a/t41/t46a/t49a/t54/t57a/t62
    dct64_step2_lsx

    addi.d        t5,       t5,       16
    addi.d        t4,       t4,       -16
    // t34a/t37/t42a/t45/t50/t53a/t58/t61a
    dct64_step2_lsx

    addi.d        t5,       t5,       16
    addi.d        t4,       t4,       -16
    // t35/t36a/t43/t44a/t51a/t52/t59a/t60
    dct64_step2_lsx

    li.w          t4,       64*32*2+64+512
    add.d         t5,       t4,       sp
    addi.d        t4,       t5,       16*7
    addi.d        a0,       a0,       \in1
    // 0 - 7, 56 -63
    dct64_step3_lsx
    li.w          t8,       0
    mul.w         t0,       t8,       a1
    add.d         t0,       a0,       t0
    alsl.d        t6,       a1,       t0,      1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1
    li.w          t8,       56
    mul.w         t0,       t8,       a1
    add.d         t0,       a0,       t0
    alsl.d        t6,       a1,       t0,      1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21

    // 8 - 15, 48 - 55
    addi.d        t3,       t3,       128
    addi.d        t4,       t4,       -16*8
    addi.d        t5,       t5,       -16*8
    dct64_step3_lsx
    li.w          t8,       8
    mul.w         t0,       t8,       a1
    add.d         t0,       t0,       a0
    alsl.d        t6,       a1,       t0,     1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1
    li.w          t8,       48
    mul.w         t0,       t8,       a1
    add.d         t0,       t0,       a0
    alsl.d        t6,       a1,       t0,     1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21

    // 16 - 23, 40 - 47
    addi.d        t3,       t3,       128
    addi.d        t4,       t4,       -16*8
    addi.d        t5,       t5,       -16*8
    dct64_step3_lsx
    li.w          t8,       16
    mul.w         t0,       t8,       a1
    add.d         t0,       t0,       a0
    alsl.d        t6,       a1,       t0,     1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1
    li.w          t8,       40
    mul.w         t0,       t8,       a1
    add.d         t0,       t0,       a0
    alsl.d        t6,       a1,       t0,     1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21

    // 24 - 31, 32 - 39
    addi.d        t3,       t3,       128
    addi.d        t4,       t4,       -16*8
    addi.d        t5,       t5,       -16*8
    dct64_step3_lsx
    li.w          t8,       24
    mul.w         t0,       t8,       a1
    add.d         t0,       t0,       a0
    alsl.d        t6,       a1,       t0,     1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr20, vr22, vr24, vr26, vr28, vr30, vr2, vr1
    li.w          t8,       32
    mul.w         t0,       t8,       a1
    add.d         t0,       t0,       a0
    alsl.d        t6,       a1,       t0,     1
    addi.d        t1,       t0,       0
    add.d         t2,       t0,       a1
    dct64_step5_lsx vr3, vr15, vr31, vr29, vr27, vr25, vr23, vr21
.endm
    dct64x64_core2_lsx 16*0, 0, no_rect2
    dct64x64_core2_lsx 16*1, 8, no_rect2
    dct64x64_core2_lsx 16*2, 8, no_rect2
    dct64x64_core2_lsx 16*3, 8, no_rect2
    dct64x64_core2_lsx 16*4, 8, no_rect2
    dct64x64_core2_lsx 16*5, 8, no_rect2
    dct64x64_core2_lsx 16*6, 8, no_rect2
    dct64x64_core2_lsx 16*7, 8, no_rect2

    free_space 64*32*2+512+512
.DCT_DCT_64X64_END:
endfunc

function inv_txfm_add_dct_dct_64x32_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_64x32

    idct_dc_w64 64, 32, 1

    DST_ADD_W64 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20

    li.w          t3,       31
.loop31:
    add.d         a0,       a0,       a1
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     a0,       32
    vld           vr13,     a0,       48
    DST_ADD_W64 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
    addi.d        t3,       t3,       -1
    blt           zero,     t3,       .loop31
    b             .DCT_DCT_64X32_END
.NO_HAS_DCONLY_64x32:
    malloc_space  64*32*2+512+512

    la.local      t8,       eob_32x32
    addi.d        t2,       a2,       0
    addi.d        t7,       sp,       64
    addi.d        t7,       t7,       0
    addi.d        a4,       a2,       64
.DCT_DCT_EOB_64x32:
    ld.h          a5,       t8,       0
    addi.d        t8,       t8,       2
    dct64x64_core1_lsx 1, rect2_lsx
    addi.d        t2,       t2,       16
    addi.d        t7,       t7,       128*8
    addi.d        a4,       a4,       16
    bge           a3,       a5,       .DCT_DCT_EOB_64x32

    la.local      t8,       eob_32x32
    vxor.v        vr31,     vr31,     vr31

    ld.h          t7,       t8,       4
    bge           a3,       t7,       .DCT_DCT_EOB_64x32_END
    li.d          t1,       1024*3+64
    add.d         t0,       sp,       t1
.rept 4
    vst_x16 t0, 0, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d t0, t0, 256
.endr

    ld.h          t7,       t8,       2
    bge           a3,       t7,       .DCT_DCT_EOB_64x32_END
    li.d          t1,       1024*2+64
    add.d         t0,       sp,       t1
.rept 4
    vst_x16 t0, 0, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d        t0,       t0,       256
.endr

    ld.h          t7,       t8,       0
    bge           a3,       t7,       .DCT_DCT_EOB_64x32_END
    li.d          t1,       1024*1+64
    add.d         t0,       sp,       t1
.rept 4
    vst_x16 t0, 0, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31, \
            vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    addi.d        t0,       t0,       256
.endr

.DCT_DCT_EOB_64x32_END:
    addi.d        t2,       sp,       64
    li.w          t4,       64*32*2+64
    add.d         t3,       sp,       t4
    addi.d        t5,       sp,       64
    addi.d        t5,       t5,       1024
    addi.d        t5,       t5,       1024
.rept 8
    vld_x8 t2, 0, 256, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    addi.d        t4,       t2,       1024
    addi.d        t4,       t4,       1024

    vld_x8 t4, 0, 256, vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    inv_dct16_lsx no_rect2

    vst_x16 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    addi.d        t4,       t2,       128
    vld_x8 t4, 0, 256, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    addi.d        t4,       t4,       1024
    addi.d        t4,       t4,       1024

    vld_x8 t4, 0, 256, vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    dct_8x32_core_lsx t5, t3, 0, 128, 16, -2048, 1024, -1024, 0, 128, , 4

    addi.d        t2,       t2,       16
    addi.d        t5,       t5,       16
    addi.d        t1,       t1,       16
.endr
    addi.d        t2,       sp,       64
    li.w          t3,       32
.loop32:
    vld           vr10,     a0,       0
    vld           vr11,     a0,       16
    vld           vr12,     a0,       32
    vld           vr13,     a0,       48
    vld_x8 t2, 0, 16, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    DST_ADD_W64 vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    add.d         a0,       a0,       a1
    addi.d        t2,       t2,       128
    addi.d        t3,       t3,       -1
    blt           zero,     t3,       .loop32

    free_space  64*32*2+512+512
.DCT_DCT_64X32_END:
endfunc

.macro VLD_DST_ADD_W8_H32 in0
    vld           vr4,      t3,       0
    vld           vr5,      t3,       16
    vld           vr6,      t3,       32
    vld           vr7,      t3,       48
    VLD_DST_ADD_W8 vr4, vr5, vr6, vr7
    addi.d        t3,       t3,       64
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       t2,     2
    vld           vr4,      t3,       0
    vld           vr5,      t3,       16
    vld           vr6,      t3,       32
    vld           vr7,      t3,       48
    VLD_DST_ADD_W8 vr4, vr5, vr6, vr7
    addi.d        t3,       sp,       \in0
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       t2,     2
.endm

function inv_txfm_add_dct_dct_8x32_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_8x32

    idct_dc 8, 32, 2

    DST_ADD_W8 vr10, vr11, vr12, vr13, vr20, vr20, vr20, vr20
.rept 7
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,     1

    VLD_DST_ADD_W8 vr20, vr20, vr20, vr20
.endr
    b             .DCT_DCT_8X32_END
.NO_HAS_DCONLY_8x32:
    malloc_space 512

    la.local      t8,       eob_8x32
    addi.d        t3,       sp,       64
    addi.d        t2,       a2,       0
.DCT_DCT_EOB_8x32:
    ld.h          t7,       t8,       0
    addi.d        t8,       t8,       2

    vld_x8 a2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    inv_dct8_lsx vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, .8h

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsrari.h      \i,       \i,       2
.endr

    vxor.v        vr31,     vr31,     vr31
    vst_x8 a2, 0, 64, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    vst_x8 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    addi.d        a2,       a2,       16
    addi.d        t3,       t3,       128
    bge           a3,       t7,       .DCT_DCT_EOB_8x32

    la.local      t8,       eob_8x32
    vxor.v        vr31,     vr31,     vr31
    ld.h          t7,       t8,       4
    bge           a3,       t7,       .DCT_DCT_EOB_8x32_END
    vst_x8 sp, 64+384, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    ld.h          t7,       t8,       2
    bge           a3,       t7,       .DCT_DCT_EOB_8x32_END
    vst_x8 sp, 64+256, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    ld.h          t7,       t8,       0
    bge           a3,       t7,       .DCT_DCT_EOB_8x32_END
    vst_x8 sp, 64+128, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
.DCT_DCT_EOB_8x32_END:
    addi.d        t2,       sp,       64
    addi.d        t3,       sp,       64

    vld_x16 t2, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    inv_dct16_lsx .8h

    vst_x16 t3, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vld_x16 t2, 16, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    dct_8x32_core_lsx t2, t3, 0, 256, 32, 0, 128, 256, 384, 16, , 4

    alsl.d        t2,       a1,       a0,     1
    addi.d        t3,       sp,       64

    VLD_DST_ADD_W8_H32 320
    VLD_DST_ADD_W8_H32 448
    VLD_DST_ADD_W8_H32 192
    VLD_DST_ADD_W8_H32 0

    free_space 512
.DCT_DCT_8X32_END:
endfunc

function inv_txfm_add_identity_identity_8x32_8bpc_lsx
    la.local      t7,       eob_8x32
    alsl.d        t2,       a1,       a0,     1

.IDENTITY_IDENTITY_EOB_8x32:
    ld.h          t6,       t7,       0
    addi.d        t7,       t7,       2
    vld_x8 a2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7

    vxor.v        vr23,     vr23,     vr23
    vst_x8 a2, 0, 64, vr23, vr23, vr23, vr23, vr23, vr23, vr23, vr23

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vsrari.h       \i,       \i,       1
.endr

    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                   vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23, \
                   vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

.irp i, vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23
    vsrari.h       \i,       \i,       2
.endr
    VLD_DST_ADD_W8 vr16, vr17, vr18, vr19
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,     1

    VLD_DST_ADD_W8 vr20, vr21, vr22, vr23
    add.d         a0,       a1,       a0
    alsl.d        t2,       a1,       a0,     1

    addi.d        a2,       a2,       16
    bge           a3,       t6,       .IDENTITY_IDENTITY_EOB_8x32
endfunc

.macro def_fn_16x4_base txfm
functionl inv_txfm_\txfm\()add_16x4_lsx
    vld_x8 a2, 0, 16, vr0, vr2, vr4, vr6, vr8, vr10, vr12, vr14

.ifc \txfm, identity_
    li.w          t0,       1697
    vreplgr2vr.w  vr20,     t0
.irp i, vr0, vr2, vr4, vr6, vr8, vr10, vr12, vr14
    inv_identity16_lsx \i, vr20, \i, \i, .8h
.endr

    vilvh.d       vr1,      vr0,      vr0
    vilvh.d       vr3,      vr2,      vr2
    vilvh.d       vr5,      vr4,      vr4
    vilvh.d       vr7,      vr6,      vr6
    vilvh.d       vr9,      vr8,      vr8
    vilvh.d       vr11,     vr10,     vr10
    vilvh.d       vr13,     vr12,     vr12
    vilvh.d       vr15,     vr14,     vr14
.else
    vilvh.d       vr1,      vr0,      vr0
    vilvh.d       vr3,      vr2,      vr2
    vilvh.d       vr5,      vr4,      vr4
    vilvh.d       vr7,      vr6,      vr6
    vilvh.d       vr9,      vr8,      vr8
    vilvh.d       vr11,     vr10,     vr10
    vilvh.d       vr13,     vr12,     vr12
    vilvh.d       vr15,     vr14,     vr14

    move          t6,       ra
    jirl          ra,       t7,       0
    move          ra,       t6
.endif

    vxor.v        vr23,     vr23,     vr23
    vst_x8 a2, 0, 16, vr23, vr23, vr23, vr23, vr23, vr23, vr23, vr23

    LSX_TRANSPOSE8x4_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, vr0, vr1, \
                       vr2, vr3, vr16, vr17, vr18, vr19, vr20, vr21

    LSX_TRANSPOSE8x4_H vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, vr4, \
                       vr5, vr6, vr7, vr16, vr17, vr18, vr19, vr20, vr21

    vsrari.h      vr0,      vr0,      1
    vsrari.h      vr1,      vr1,      1
    vsrari.h      vr2,      vr2,      1
    vsrari.h      vr3,      vr3,      1
    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vsrari.h      vr8,      vr0,      4
    vsrari.h      vr9,      vr1,      4
    vsrari.h      vr10,     vr2,      4
    vsrari.h      vr11,     vr3,      4
    vsrari.h      vr0,      vr4,      1
    vsrari.h      vr1,      vr5,      1
    vsrari.h      vr2,      vr6,      1
    vsrari.h      vr3,      vr7,      1

    move          t6,       ra
    jirl          ra,       t8,       0
    move          ra,       t6

    vsrari.h      vr16,     vr0,      4
    vsrari.h      vr17,     vr1,      4
    vsrari.h      vr18,     vr2,      4
    vsrari.h      vr19,     vr3,      4

    alsl.d        t2,       a1,       a0,    1
    VLD_DST_ADD_W16 vr8, vr16, vr9, vr17, vr10, vr18, vr11, vr19
endfuncl
.endm

def_fn_16x4_base identity_
def_fn_16x4_base

.macro fn_16x4 txfm1, txfm2
function inv_txfm_add_\txfm1\()_\txfm2\()_16x4_8bpc_lsx
.ifc \txfm1\()_\txfm2, dct_dct
    bnez          a3,       .NO_HAS_DCONLY_16x4

    idct_dc 16, 4, 1

    DST_ADD_W16 vr10, vr11, vr12, vr13, vr20, vr20, vr20, \
                vr20, vr20, vr20, vr20, vr20
    b             .\txfm1\()_\txfm2\()_16x4_END
.NO_HAS_DCONLY_16x4:
.endif

.ifnc \txfm1, identity
    la.local     t7,    inv_\txfm1\()_4h_x16_lsx
.endif
    la.local     t8,    inv_\txfm2\()_8h_x4_lsx

.ifc \txfm1, identity
    b            inv_txfm_identity_add_16x4_lsx
.else
    b            inv_txfm_add_16x4_lsx
.endif
.\txfm1\()_\txfm2\()_16x4_END:
endfunc
.endm

fn_16x4 dct, dct
fn_16x4 identity, identity
fn_16x4 adst, dct

.macro VLD_DST_ADD_W16_H32 in0
    vld           vr14,     t3,       0
    vld           vr15,     t3,       16
    vld           vr16,     t3,       32
    vld           vr17,     t3,       48
    vld           vr18,     t5,       0
    vld           vr19,     t5,       16
    vld           vr20,     t5,       32
    vld           vr21,     t5,       48
    vsrari_h_x8 vr14, vr18, vr15, vr19, vr16, vr20, vr17, vr21, \
                vr14, vr18, vr15, vr19, vr16, vr20, vr17, vr21, 4
    VLD_DST_ADD_W16 vr14, vr18, vr15, vr19, vr16, vr20, vr17, vr21
    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       t2,    2
    addi.d        t3,       t3,       64
    addi.d        t5,       t5,       64
    vld           vr14,     t3,       0
    vld           vr15,     t3,       16
    vld           vr16,     t3,       32
    vld           vr17,     t3,       48
    vld           vr18,     t5,       0
    vld           vr19,     t5,       16
    vld           vr20,     t5,       32
    vld           vr21,     t5,       48
    vsrari_h_x8 vr14, vr18, vr15, vr19, vr16, vr20, vr17, vr21, \
                vr14, vr18, vr15, vr19, vr16, vr20, vr17, vr21, 4
    VLD_DST_ADD_W16 vr14, vr18, vr15, vr19, vr16, vr20, vr17, vr21
    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       t2,    2
    addi.d        t3,       sp,       \in0
    addi.d        t5,       sp,       \in0+512
.endm

function inv_txfm_add_dct_dct_16x32_8bpc_lsx
    bnez          a3,       .NO_HAS_DCONLY_16x32

    idct_dc 16, 32, 1

    DST_ADD_W16 vr10, vr11, vr12, vr13, vr20, vr20, vr20, \
                    vr20, vr20, vr20, vr20, vr20
.rept 7
    alsl.d        a0,       a1,       a0,     2
    alsl.d        t2,       a1,       a0,     1

    VLD_DST_ADD_W16 vr20, vr20, vr20, vr20, vr20, vr20, vr20, vr20
.endr
    b             .DCT_DCT_16x32_END
.NO_HAS_DCONLY_16x32:
    malloc_space 512+512

    addi.d        t3,       sp,       64
    la.local      t8,       eob_16x32

.DCT_DCT_EOB_16x32:
    ld.h          t7,       t8,       0
    addi.d        t8,       t8,       2
    vld_x16 a2, 0, 64, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vxor.v        vr31,     vr31,     vr31
.irp i, 0, 64, 128, 192, 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960
    vst           vr31,     a2,       \i
.endr

    li.w          t0,       2896
    vreplgr2vr.w  vr23,     t0
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
     vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    rect2_lsx   \i, vr23, \i
.endr

    inv_dct16_lsx .8h

    LSX_TRANSPOSE8x8_H vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

    LSX_TRANSPOSE8x8_H vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15, \
                       vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23

.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
    vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15
    vsrari.h       \i,       \i,       1
.endr

    vst_x8 t3, 0, 16, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7
    vst_x8 t3, 512, 16, vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    addi.d        a2,       a2,       16
    addi.d        t3,       t3,       128
    bge           a3,       t7,       .DCT_DCT_EOB_16x32

    la.local      t8,       eob_16x32
    vxor.v        vr31,     vr31,     vr31

    ld.h          t7,       t8,       4
    bge           a3,       t7,       .DCT_DCT_EOB_16x32_END
    vst_x8 sp, 64+384, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    vst_x8 sp, 64+896, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    ld.h          t7,       t8,       2
    bge           a3,       t7,       .DCT_DCT_EOB_16x32_END
    vst_x8 sp, 64+256, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    vst_x8 sp, 64+768, 16,  vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

    ld.h          t7,       t8,       0
    bge           a3,       t7,       .DCT_DCT_EOB_16x32_END
    vst_x8 sp, 64+128, 16, vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31
    vst_x8 sp, 64+512+128, 16  vr31, vr31, vr31, vr31, vr31, vr31, vr31, vr31

.DCT_DCT_EOB_16x32_END:
    addi.d      t7,   sp,    64
.rept 2
    vld_x16 t7, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    inv_dct16_lsx .8h

    vst_x16 t7, 0, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr8, vr9, vr10, vr11, vr12, vr13, vr14, vr15

    vld_x16 t7, 16, 32, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, \
            vr19, vr24, vr25, vr26, vr27, vr28, vr29, vr30

    dct_8x32_core_lsx t7, t7, 0, 256, 32, 0, 128, 256, 384, 16, ,

    addi.d        t7,       t7,       512
.endr
    alsl.d        t2,       a1,       a0,    1
    addi.d        t3,       sp,       64
    addi.d        t5,       sp,       512+64

    VLD_DST_ADD_W16_H32 320
    VLD_DST_ADD_W16_H32 448
    VLD_DST_ADD_W16_H32 192
    VLD_DST_ADD_W16_H32 0

    free_space 512+512
.DCT_DCT_16x32_END:
endfunc

.macro xvmulev_xvmaddod_lasx in0, in1, in2, in3, out0, out1
    xvmulwev.w.h   \out0,    \in0,     \in2
    xvmulwod.w.h   \out1,    \in0,     \in2
    xvmaddwev.w.h  \out0,    \in1,     \in3
    xvmaddwod.w.h  \out1,    \in1,     \in3
.endm

.macro xvsrari_h_x16 in0, in1, in2, in3, in4, in5, in6, in7, in8, in9, in10, \
                     in11, in12, in13, in14, in15, out0, out1, out2, out3, \
                     out4, out5, out6, out7, out8, out9, out10, out11, out12, \
                     out13, out14, out15, shift
    xvsrari.h  \out0,       \in0,     \shift
    xvsrari.h  \out1,       \in1,     \shift
    xvsrari.h  \out2,       \in2,     \shift
    xvsrari.h  \out3,       \in3,     \shift
    xvsrari.h  \out4,       \in4,     \shift
    xvsrari.h  \out5,       \in5,     \shift
    xvsrari.h  \out6,       \in6,     \shift
    xvsrari.h  \out7,       \in7,     \shift
    xvsrari.h  \out8,       \in8,     \shift
    xvsrari.h  \out9,       \in9,     \shift
    xvsrari.h  \out10,      \in10,    \shift
    xvsrari.h  \out11,      \in11,    \shift
    xvsrari.h  \out12,      \in12,    \shift
    xvsrari.h  \out13,      \in13,    \shift
    xvsrari.h  \out14,      \in14,    \shift
    xvsrari.h  \out15,      \in15,    \shift
.endm

.macro xvpermi_q_x2 in0, in1, in2, in3, out0, out1, out2, out3, tmp0, tmp1
    xvor.v      \tmp0,      \in0,     \in0
    xvor.v      \tmp1,      \in1,     \in1
    xvpermi.q   \out0,      \in2,     0x02
    xvpermi.q   \out1,      \in3,     0x02
    xvpermi.q   \out2,      \tmp0,    0x31
    xvpermi.q   \out3,      \tmp1,    0x31
.endm

.macro DST_ADD_W16_LASX in0, in1, in2, in3, in4, in5, in6, in7
    vext2xv.hu.bu xr0,      \in0
    vext2xv.hu.bu xr1,      \in1
    vext2xv.hu.bu xr2,      \in2
    vext2xv.hu.bu xr3,      \in3
    xvadd.h       xr0,      xr0,      \in4
    xvadd.h       xr1,      xr1,      \in5
    xvadd.h       xr2,      xr2,      \in6
    xvadd.h       xr3,      xr3,      \in7
    xvssrani.bu.h xr1,      xr0,      0
    xvssrani.bu.h xr3,      xr2,      0
    xvpermi.d     xr0,      xr1,      0b11011000
    xvpermi.d     xr2,      xr3,      0b11011000
    xvpermi.d     xr1,      xr0,      0b00001110
    xvpermi.d     xr3,      xr2,      0b00001110
    vst           vr0,      a0,       0
    vstx          vr1,      a0,       a1
    vst           vr2,      t2,       0
    vstx          vr3,      t2,       a1
.endm

.macro XVLD_DST_ADD_W16 in0, in1, in2, in3
    vld           vr0,      a0,       0
    vldx          vr1,      a0,       a1
    vld           vr2,      t2,       0
    vldx          vr3,      t2,       a1
    DST_ADD_W16_LASX xr0, xr1, xr2, xr3, \in0, \in1, \in2, \in3
.endm

.macro inv_adst16_lasx
    la.local      t0,       iadst16_coeffs_h

    xvldrepl.h    xr20,     t0,       0        // 4091
    xvldrepl.h    xr21,     t0,       2        // 201
    xvmulev_xvmaddod_lasx xr15, xr0, xr20, xr21, xr16, xr18
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr15, xr0, xr21, xr20, xr17, xr19
    xvilvl.w      xr15,     xr18,     xr16
    xvilvl.w      xr0,      xr19,     xr17
    xvilvh.w      xr18,     xr18,     xr16
    xvilvh.w      xr19,     xr19,     xr17
    xvssrarni.h.w xr18,     xr15,     12       // t0
    xvssrarni.h.w xr19,     xr0,      12       // t1

    xvldrepl.h    xr20,     t0,       4        // 3973
    xvldrepl.h    xr21,     t0,       6        // 995
    xvmulev_xvmaddod_lasx xr13, xr2, xr20, xr21, xr16, xr0
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr13, xr2, xr21, xr20, xr17, xr15
    xvilvl.w      xr13,     xr0,      xr16
    xvilvl.w      xr2,      xr15,     xr17
    xvilvh.w      xr0,      xr0,      xr16
    xvilvh.w      xr15,     xr15,     xr17
    xvssrarni.h.w xr0,      xr13,     12       // t2
    xvssrarni.h.w xr15,     xr2,      12       // t3

    xvldrepl.h    xr20,     t0,        8       // 3703
    xvldrepl.h    xr21,     t0,        10      // 1751
    xvmulev_xvmaddod_lasx xr11, xr4, xr20, xr21, xr16, xr2
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr11, xr4, xr21, xr20, xr17, xr13
    xvilvl.w      xr11,     xr2,       xr16
    xvilvl.w      xr4,      xr13,      xr17
    xvilvh.w      xr2,      xr2,       xr16
    xvilvh.w      xr13,     xr13,      xr17
    xvssrarni.h.w xr2,      xr11,      12       // t4
    xvssrarni.h.w xr13,     xr4,       12       // t5

    xvldrepl.h    xr20,     t0,        12       // 3290 -> 1645
    xvldrepl.h    xr21,     t0,        14       // 2440 -> 1220
    xvmulev_xvmaddod_lasx xr9, xr6, xr20, xr21, xr16, xr4
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr9, xr6, xr21, xr20, xr17, xr11
    xvilvl.w      xr9,      xr4,       xr16
    xvilvl.w      xr6,      xr11,      xr17
    xvilvh.w      xr4,      xr4,       xr16
    xvilvh.w      xr11,     xr11,      xr17
    xvssrarni.h.w xr4,      xr9,       12       // t6
    xvssrarni.h.w xr11,     xr6,       12       // t7

    xvldrepl.h    xr20,     t0,        16       // 2751
    xvldrepl.h    xr21,     t0,        18       // 3035
    xvmulev_xvmaddod_lasx xr7, xr8, xr20, xr21, xr16, xr6
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr7, xr8, xr21, xr20, xr17, xr9
    xvilvl.w      xr7,      xr6,       xr16
    xvilvl.w      xr8,      xr9,       xr17
    xvilvh.w      xr6,      xr6,       xr16
    xvilvh.w      xr9,      xr9,       xr17
    xvssrarni.h.w xr6,      xr7,       12       // t8
    xvssrarni.h.w xr9,      xr8,       12       // t9

    xvldrepl.h    xr20,     t0,        20       // 2106
    xvldrepl.h    xr21,     t0,        22       // 3513
    xvmulev_xvmaddod_lasx xr5, xr10, xr20, xr21, xr16, xr7
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr5, xr10, xr21, xr20, xr17, xr8
    xvilvl.w      xr5,      xr7,       xr16
    xvilvl.w      xr10,     xr8,       xr17
    xvilvh.w      xr7,      xr7,       xr16
    xvilvh.w      xr8,      xr8,       xr17
    xvssrarni.h.w xr7,      xr5,       12       // t10
    xvssrarni.h.w xr8,      xr10,      12       // t11

    xvldrepl.h    xr20,     t0,        24       // 1380
    xvldrepl.h    xr21,     t0,        26       // 3857
    xvmulev_xvmaddod_lasx xr3, xr12, xr20, xr21, xr16, xr5
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr3, xr12, xr21, xr20, xr17, xr10
    xvilvl.w      xr3,      xr5,       xr16
    xvilvl.w      xr12,     xr10,      xr17
    xvilvh.w      xr5,      xr5,       xr16
    xvilvh.w      xr10,     xr10,      xr17
    xvssrarni.h.w xr5,      xr3,       12       // t12
    xvssrarni.h.w xr10,     xr12,      12       // t13

    xvldrepl.h    xr20,     t0,        28       // 601
    xvldrepl.h    xr21,     t0,        30       // 4052
    xvmulev_xvmaddod_lasx xr1, xr14, xr20, xr21, xr16, xr3
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr1, xr14, xr21, xr20, xr17, xr12
    xvilvl.w      xr1,      xr3,       xr16
    xvilvl.w      xr14,     xr12,      xr17
    xvilvh.w      xr3,      xr3,       xr16
    xvilvh.w      xr12,     xr12,      xr17
    xvssrarni.h.w xr3,      xr1,       12       // t14
    xvssrarni.h.w xr12,     xr14,      12       // t15

    xvsadd.h      xr1,      xr18,      xr6      // t0a
    xvssub.h      xr14,     xr18,      xr6      // t8a
    xvsadd.h      xr16,     xr19,      xr9      // t1a
    xvssub.h      xr17,     xr19,      xr9      // t9a
    xvsadd.h      xr6,      xr0,       xr7      // t2a
    xvssub.h      xr18,     xr0,       xr7      // t10a
    xvsadd.h      xr9,      xr15,      xr8      // t3a
    xvssub.h      xr19,     xr15,      xr8      // t11a
    xvsadd.h      xr0,      xr2,       xr5      // t4a
    xvssub.h      xr7,      xr2,       xr5      // t12a
    xvsadd.h      xr8,      xr13,      xr10     // t5a
    xvssub.h      xr15,     xr13,      xr10     // t13a
    xvsadd.h      xr2,      xr4,       xr3      // t6a
    xvssub.h      xr5,      xr4,       xr3      // t14a
    xvsadd.h      xr10,     xr11,      xr12     // t7a
    xvssub.h      xr13,     xr11,      xr12     // t15a

    la.local      t0,       idct_coeffs_h

    xvldrepl.h    xr20,     t0,        8        // 799
    xvldrepl.h    xr21,     t0,        10       // 4017
    xvmulev_xvmaddod_lasx xr14, xr17, xr21, xr20, xr3, xr11
    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr14, xr17, xr20, xr21, xr4, xr12
    xvilvl.w      xr14,     xr11,      xr3
    xvilvl.w      xr17,     xr12,      xr4
    xvilvh.w      xr11,     xr11,      xr3
    xvilvh.w      xr12,     xr12,      xr4
    xvssrarni.h.w xr11,     xr14,      12       // t8
    xvssrarni.h.w xr12,     xr17,      12       // t9

    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr15, xr7, xr20, xr21, xr3, xr14
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr15, xr7, xr21, xr20, xr4, xr17
    xvilvl.w      xr15,     xr14,      xr3
    xvilvl.w      xr7,      xr17,      xr4
    xvilvh.w      xr14,     xr14,      xr3
    xvilvh.w      xr17,     xr17,      xr4
    xvssrarni.h.w xr14,     xr15,      12       // t13
    xvssrarni.h.w xr17,     xr7,       12       // t12

    xvldrepl.h    xr20,     t0,        12       // 3406
    xvldrepl.h    xr21,     t0,        14       // 2276
    xvmulev_xvmaddod_lasx xr18, xr19, xr21, xr20, xr3, xr7
    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr18, xr19, xr20, xr21, xr4, xr15
    xvilvl.w      xr18,     xr7,       xr3
    xvilvl.w      xr19,     xr15,      xr4
    xvilvh.w      xr7,      xr7,       xr3
    xvilvh.w      xr15,     xr15,      xr4
    xvssrarni.h.w xr7,      xr18,      12       // t10
    xvssrarni.h.w xr15,     xr19,      12       // t11

    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr13, xr5, xr20, xr21, xr3, xr18
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr13, xr5, xr21, xr20, xr4, xr19
    xvilvl.w      xr13,     xr18,      xr3
    xvilvl.w      xr5,      xr19,      xr4
    xvilvh.w      xr18,     xr18,      xr3
    xvilvh.w      xr19,     xr19,      xr4
    xvssrarni.h.w xr18,     xr13,      12       // t15
    xvssrarni.h.w xr19,     xr5,       12       // t14

    xvsadd.h      xr5,      xr1,       xr0      // t0
    xvssub.h      xr13,     xr1,       xr0      // t4
    xvsadd.h      xr3,      xr16,      xr8      // t1
    xvssub.h      xr4,      xr16,      xr8      // t5
    xvsadd.h      xr0,      xr6,       xr2      // t2
    xvssub.h      xr1,      xr6,       xr2      // t6
    xvsadd.h      xr8,      xr9,       xr10     // t3
    xvssub.h      xr16,     xr9,       xr10     // t7
    xvsadd.h      xr2,      xr11,      xr17     // t8a
    xvssub.h      xr6,      xr11,      xr17     // t12a
    xvsadd.h      xr9,      xr12,      xr14     // t9a
    xvssub.h      xr10,     xr12,      xr14     // t13a
    xvsadd.h      xr11,     xr7,       xr19     // t10a
    xvssub.h      xr17,     xr7,       xr19     // t14a
    xvsadd.h      xr12,     xr15,      xr18     // t11a
    xvssub.h      xr14,     xr15,      xr18     // t15a

    la.local      t0,       idct_coeffs_h

    xvldrepl.h    xr20,     t0,        4        // 1567
    xvldrepl.h    xr21,     t0,        6        // 3784
    xvmulev_xvmaddod_lasx xr13, xr4, xr21, xr20, xr7, xr18
    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr13, xr4, xr20, xr21, xr15, xr19
    xvilvl.w      xr13,     xr18,      xr7
    xvilvl.w      xr4,      xr19,      xr15
    xvilvh.w      xr18,     xr18,      xr7
    xvilvh.w      xr19,     xr19,      xr15
    xvssrarni.h.w xr18,     xr13,      12       // t4a
    xvssrarni.h.w xr19,     xr4,       12       // t5a

    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr16, xr1, xr20, xr21, xr7, xr4
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr16, xr1, xr21, xr20, xr15, xr13
    xvilvl.w      xr16,     xr4,       xr7
    xvilvl.w      xr1,      xr13,      xr15
    xvilvh.w      xr4,      xr4,       xr7
    xvilvh.w      xr13,     xr13,      xr15
    xvssrarni.h.w xr4,      xr16,      12       // t7a
    xvssrarni.h.w xr13,     xr1,       12       // t6a

    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr6, xr10, xr21, xr20, xr7, xr1
    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr6, xr10, xr20, xr21, xr15, xr16
    xvilvl.w      xr6,      xr1,       xr7
    xvilvl.w      xr10,     xr16,      xr15
    xvilvh.w      xr1,      xr1,       xr7
    xvilvh.w      xr16,     xr16,      xr15
    xvssrarni.h.w xr1,      xr6,       12       // t12
    xvssrarni.h.w xr16,     xr10,      12       // t13

    xvneg.h       xr21,     xr21
    xvmulev_xvmaddod_lasx xr14, xr17, xr20, xr21, xr7, xr6
    xvneg.h       xr20,     xr20
    xvmulev_xvmaddod_lasx xr14, xr17, xr21, xr20, xr15, xr10
    xvilvl.w      xr14,     xr6,       xr7
    xvilvl.w      xr17,     xr10,      xr15
    xvilvh.w      xr6,      xr6,       xr7
    xvilvh.w      xr10,     xr10,      xr15
    xvssrarni.h.w xr6,      xr14,      12       // t15
    xvssrarni.h.w xr10,     xr17,      12       // t14

    xvsadd.h       xr14,     xr5,       xr0      // out[0]
    xvssub.h       xr17,     xr5,       xr0      // t2a
    xvssub.h       xr7,      xr3,       xr8      // t3a
    xvsadd.h       xr15,     xr3,       xr8      // out[15]
    xvsllwil.w.h   xr22,     xr15,      0
    xvexth.w.h     xr15,     xr15
    xvneg.w        xr22,     xr22
    xvneg.w        xr15,     xr15
    xvssrarni.h.w  xr15,     xr22,      0        // out[15]
    xvssub.h       xr7,      xr3,       xr8      // t3a

    xvsadd.h       xr3,      xr19,      xr4      // out[12]
    xvssub.h       xr8,      xr19,      xr4      // t7
    xvssub.h       xr0,      xr18,      xr13     // t6
    xvsadd.h       xr5,      xr18,      xr13     // out[3]
    xvsllwil.w.h   xr22,     xr5,       0
    xvexth.w.h     xr5,      xr5
    xvneg.w        xr22,     xr22
    xvneg.w        xr5,      xr5
    xvssrarni.h.w  xr5,      xr22,      0        // out[3]

    xvsadd.h       xr13,     xr9,       xr12     // out[14]
    xvssub.h       xr19,     xr9,       xr12     // t11
    xvssub.h       xr4,      xr2,       xr11     // t10
    xvsadd.h       xr18,     xr2,       xr11     // out[1]
    xvsllwil.w.h   xr22,     xr18,      0
    xvexth.w.h     xr18,     xr18
    xvneg.w        xr22,     xr22
    xvneg.w        xr18,     xr18
    xvssrarni.h.w  xr18,     xr22,      0        // out[1]

    xvsadd.h       xr2,      xr1,       xr10     // out[2]
    xvssub.h       xr11,     xr1,       xr10     // t14a
    xvssub.h       xr12,     xr16,      xr6      // t15a
    xvsadd.h       xr9,      xr16,      xr6      // out[13]
    xvsllwil.w.h   xr22,     xr9,       0
    xvexth.w.h     xr9,      xr9
    xvneg.w        xr22,     xr22
    xvneg.w        xr9,      xr9
    xvssrarni.h.w  xr9,      xr22,      0        // out[13]

    xvldrepl.h     xr20,     t0,        0        // 2896
    xvmulev_xvmaddod_lasx xr17, xr7, xr20, xr20, xr6, xr10
    xvneg.h        xr21,     xr20
    xvmulev_xvmaddod_lasx xr17, xr7, xr20, xr21, xr16, xr1
    xvilvl.w       xr17,     xr10,      xr6
    xvilvl.w       xr7,      xr1,       xr16
    xvilvh.w       xr10,     xr10,      xr6
    xvilvh.w       xr1,      xr1,       xr16
    xvssrarni.h.w  xr1,      xr7,       12       // out[8]
    xvsrari.w      xr17,     xr17,      12
    xvsrari.w      xr10,     xr10,      12
    xvneg.w        xr17,     xr17
    xvneg.w        xr10,     xr10
    xvssrarni.h.w  xr10,     xr17,      0        // out[7]

    xvmulev_xvmaddod_lasx xr0, xr8, xr20, xr21, xr16, xr17
    xvmulev_xvmaddod_lasx xr0, xr8, xr20, xr20, xr6, xr7
    xvilvl.w       xr0,      xr17,      xr16
    xvilvl.w       xr8,      xr7,       xr6
    xvilvh.w       xr17,     xr17,      xr16
    xvilvh.w       xr7,      xr7,       xr6
    xvssrarni.h.w  xr7,      xr8,       12       // out[4]
    xvsrari.w      xr0,      xr0,       12
    xvsrari.w      xr17,     xr17,      12
    xvneg.w        xr0,      xr0
    xvneg.w        xr17,     xr17
    xvssrarni.h.w xr17,      xr0,       0        // out[11]

    xvmulev_xvmaddod_lasx xr4, xr19, xr20, xr21, xr16, xr0
    xvmulev_xvmaddod_lasx xr4, xr19, xr20, xr20, xr6, xr8
    xvilvl.w       xr4,      xr0,       xr16
    xvilvl.w       xr19,     xr8,       xr6
    xvilvh.w       xr0,      xr0,       xr16
    xvilvh.w       xr8,      xr8,       xr6
    xvssrarni.h.w  xr8,      xr19,      12       // out[6]
    xvsrari.w      xr4,      xr4,       12
    xvsrari.w      xr0,      xr0,       12
    xvneg.w        xr4,      xr4
    xvneg.w        xr0,      xr0
    xvssrarni.h.w  xr0,      xr4,       0        // out[9]
    xvmulev_xvmaddod_lasx xr11, xr12, xr20, xr20, xr6, xr4
    xvmulev_xvmaddod_lasx xr11, xr12, xr20, xr21, xr16, xr19
    xvilvl.w       xr11,     xr4,       xr6
    xvilvl.w       xr12,     xr19,      xr16
    xvilvh.w       xr4,      xr4,       xr6
    xvilvh.w       xr19,     xr19,      xr16
    xvssrarni.h.w  xr19,     xr12,      12       // out[10]
    xvsrari.w      xr11,     xr11,      12
    xvsrari.w      xr4,      xr4,       12
    xvneg.w        xr11,     xr11
    xvneg.w        xr4,      xr4
    xvssrarni.h.w  xr4,      xr11,      0        // out[5]
.endm

function inv_txfm_add_adst_adst_16x16_8bpc_lasx
    PUSH_REG
    xvld_x16 a2, 0, 32, xr0, xr1, xr2, xr3, xr4, xr5, xr6, xr7, \
             xr8, xr9, xr10, xr11, xr12, xr13, xr14, xr15

    inv_adst16_lasx

    LASX_TRANSPOSE8x8_H xr14, xr18, xr2, xr5, xr7, xr4, xr8, xr10, \
                        xr14, xr18, xr2, xr5, xr7, xr28, xr6, xr10, \
                        xr20, xr21, xr22, xr23, xr24, xr25, xr26, xr27

    LASX_TRANSPOSE8x8_H xr1,  xr0,  xr19, xr17, xr3, xr9, xr13, xr15, \
                        xr29, xr30, xr11, xr17, xr31, xr19, xr16, xr15, \
                        xr20, xr21, xr22, xr23, xr24, xr25, xr26, xr27

    xvsrari_h_x16 xr14, xr18, xr2, xr5, xr7, xr28, xr6, xr10, \
                  xr29, xr30, xr11, xr17, xr31, xr19, xr16, xr15, \
                  xr0, xr1, xr2, xr3, xr4, xr5, xr6, xr7, \
                  xr8, xr9, xr10, xr11, xr12, xr13, xr14, xr15, 2

    xvpermi_q_x2 xr0, xr1, xr8, xr9, xr0, xr1, xr8, xr9, xr20, xr21
    xvpermi_q_x2 xr2, xr3, xr10, xr11, xr2, xr3, xr10, xr11, xr20, xr21
    xvpermi_q_x2 xr4, xr5, xr12, xr13, xr4, xr5, xr12, xr13, xr20, xr21
    xvpermi_q_x2 xr6, xr7, xr14, xr15, xr6, xr7, xr14, xr15, xr20, xr21

    inv_adst16_lasx

    xvsrari_h_x16 xr14, xr18, xr2,  xr5,  xr7,  xr4, xr8,  xr10, \
                  xr1,  xr0,  xr19, xr17, xr3,  xr9, xr13, xr15, \
                  xr14, xr18, xr11, xr5,  xr7,  xr4, xr8,  xr10, \
                  xr12, xr16, xr19, xr17, xr20, xr9, xr13, xr15, 4

    xvxor.v       xr23,     xr23,     xr23
.irp i, 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480
    xvst          xr23,     a2,       \i
.endr
    alsl.d        t2,       a1,       a0,    1
    XVLD_DST_ADD_W16 xr14, xr18, xr11, xr5
    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       a0,    1
    XVLD_DST_ADD_W16 xr7, xr4, xr8, xr10
    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       a0,    1
    XVLD_DST_ADD_W16 xr12, xr16, xr19, xr17
    alsl.d        a0,       a1,       a0,    2
    alsl.d        t2,       a1,       a0,    1
    XVLD_DST_ADD_W16 xr20, xr9, xr13, xr15
    POP_REG
endfunc
