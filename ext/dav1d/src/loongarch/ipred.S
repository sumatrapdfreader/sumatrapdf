/*
 * Copyright © 2024, VideoLAN and dav1d authors
 * Copyright © 2024, Loongson Technology Corporation Limited
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

.macro ipred_dc_gen topleft, width, height
    add.d          t0,      \width,  \height //dc
    srai.d         t0,      t0,      1
    addi.d         t3,      \topleft,1

    or             t1,      zero,    zero  //data index
    srai.d         t2,      \width,  4     //loop param
    beqz           t2,      2f

1:  // width/16
    vldx           vr0,     t3,      t1
    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0
    vhaddw.qu.du   vr0,     vr0,     vr0

    vpickve2gr.du  t4,      vr0,     0
    add.d          t0,      t0,      t4

    addi.d         t1,      t1,      16
    addi.d         t2,      t2,      -1
    bnez           t2,      1b
    b              4f

2:  // &8
    andi           t2,      \width,  8
    beqz           t2,      3f

    vxor.v         vr0,     vr0,     vr0
    fldx.d         f0,      t3,      t1

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0

    vpickve2gr.du  t4,      vr0,     0
    add.d          t0,      t0,      t4
    addi.d         t1,      t1,      8
    b              4f

3:  // &4
    andi           t2,      \width,  4
    beqz           t2,      4f

    vxor.v         vr0,     vr0,     vr0
    fldx.s         f0,      t3,      t1

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0

    vpickve2gr.wu  t4,      vr0,     0
    add.d          t0,      t0,      t4
    addi.d         t1,      t1,      4

4:
    addi.d         t3,      \topleft,0
    srai.d         t2,      \height, 4     //loop param
    beqz           t2,      8f

7:  // height/16
    addi.d         t3,      t3,      -16
    vld            vr0,     t3,      0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0
    vhaddw.qu.du   vr0,     vr0,     vr0

    vpickve2gr.du  t4,      vr0,     0
    add.d          t0,      t0,      t4

    addi.d         t2,      t2,      -1
    bnez           t2,      7b
    b              10f

8:  // &8
    andi           t2,      \height, 8
    beqz           t2,      9f

    addi.d         t3,      t3,      -8
    vxor.v         vr0,     vr0,     vr0
    fld.d          f0,      t3,      0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0

    vpickve2gr.du  t4,      vr0,     0
    add.d          t0,      t0,      t4
    b              10f

9:  // &4
    andi           t2,      \height, 4
    beqz           t2,      10f

    addi.d         t3,      t3,      -4
    vxor.v         vr0,     vr0,     vr0
    fld.s          f0,      t3,      0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0

    vpickve2gr.wu  t4,      vr0,     0
    add.d          t0,      t0,      t4

10:
    add.d          t1,      \width,  \height
    ctz.w          t1,      t1
    sra.w          t0,      t0,      t1

    // w != h
    beq            \width,  \height, 16f
    add.d          t2,      \height, \height
    add.d          t3,      \width,  \width
    slt            t2,      t2,      \width
    slt            t3,      t3,      \height
    or             t2,      t2,      t3
    li.w           t3,      0x3334
    maskeqz        t1,      t3,      t2
    li.w           t3,      0x5556
    masknez        t2,      t3,      t2
    or             t1,      t1,      t2
    mul.w          t0,      t0,      t1
    srai.w         t0,      t0,      16

16:
.endm

.macro ipred_splat_dc dst, stride, width, height, dc
    li.w           t1,      4
    blt            t1,      \width,  2f

    li.w           t1,      0x01010101
    mulw.d.wu      t1,      \dc,     t1
    beqz           \height, 7f
    or             t2,      \dst,    \dst
1:  // width <= 4
    st.w           t1,      t2,      0
    add.d          t2,      t2,      \stride
    addi.d         \height, \height, -1
    bnez           \height, 1b
    b              7f

2:  //width > 4
    li.d           t1,      0x0101010101010101
    mul.d          t1,      \dc,     t1
    vreplgr2vr.d   vr0,     t1
    or             t4,      \dst,    \dst
    beqz           \height, 7f

3:
    andi           t5,      \width,  64
    beqz           t5,      4f
    vst            vr0,     t4,      0
    vst            vr0,     t4,      16
    vst            vr0,     t4,      32
    vst            vr0,     t4,      48
    b              6f

4:
    andi           t5,      \width,  32
    beqz           t5,      41f
    vst            vr0,     t4,      0
    vst            vr0,     t4,      16
    b              6f

41:
    andi           t5,      \width,  16
    beqz           t5,      5f
    vst            vr0,     t4,      0
    b              6f

5:
    fst.d          f0,      t4,      0

6:
    add.d          t4,      t4,      \stride
    addi.d         \height, \height, -1
    bnez           \height, 3b

7:
.endm

.macro ipred_dc_gen_top topleft, width
    srai.d         t0,      \width,  1
    addi.d         t1,      \topleft,1

    srai.d         t2,      \width,  4
    beqz           t2,      2f
1:
    vld            vr0,     t1,      0
    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0
    vhaddw.qu.du   vr0,     vr0,     vr0

    vpickve2gr.du  t3,      vr0,     0
    add.d          t0,      t0,      t3

    addi.d         t1,      t1,      16
    addi.d         t2,      t2,      -1
    bnez           t2,      1b
    b              4f

2:  // &8
    andi           t2,      \width,  8
    beqz           t2,      3f

    vxor.v         vr0,     vr0,     vr0
    fld.d          f0,      t1,      0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0

    vpickve2gr.du  t2,      vr0,     0
    add.d          t0,      t0,      t2

    addi.d         t1,      t1,      8
    b              4f

3:  // &4
    andi           t2,      \width,  4
    beqz           t2,      4f

    vxor.v         vr0,     vr0,     vr0
    fld.s          f0,      t1,      0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0

    vpickve2gr.du  t2,      vr0,     0
    add.d          t0,      t0,      t2
    addi.d         t1,      t1,      4

4:
    ctz.w          t1,      \width
    sra.w          t0,      t0,      t1
.endm

.macro ipred_dc_gen_left topleft, height
    srai.d         t0,      \height, 1
    srai.d         t2,      \height, 4     //loop param
    beqz           t2,      8f

7:  // height/16
    addi.d         \topleft,\topleft,-16
    vld            vr0,     \topleft,0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0
    vhaddw.qu.du   vr0,     vr0,     vr0

    vpickve2gr.du  t4,      vr0,     0
    add.d          t0,      t0,      t4

    addi.d         t2,      t2,      -1
    bnez           t2,      7b
    b              10f

8:  // &8
    andi           t2,      \height, 8
    beqz           t2,      9f

    addi.d         \topleft,\topleft,-8
    vxor.v         vr0,     vr0,     vr0
    fld.d          f0,      \topleft,0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0
    vhaddw.du.wu   vr0,     vr0,     vr0

    vpickve2gr.du  t4,      vr0,     0
    add.d          t0,      t0,      t4
    b              10f

9:  // &4
    andi           t2,      \height, 4
    beqz           t2,      10f

    addi.d         \topleft,\topleft,-4
    vxor.v         vr0,     vr0,     vr0
    fld.s          f0,      \topleft,0

    vhaddw.hu.bu   vr0,     vr0,     vr0
    vhaddw.wu.hu   vr0,     vr0,     vr0

    vpickve2gr.wu  t4,      vr0,     0
    add.d          t0,      t0,      t4

10:
    ctz.w          t1,      \height
    sra.w          t0,      t0,      t1

.endm

// void ipred_dc_lsx(pixel *dst, const ptrdiff_t stride,
//                   const pixel *const topleft,
//                   const int width, const int height, const int a,
//                   const int max_width, const int max_height
//                   HIGHBD_DECL_SUFFIX)
function ipred_dc_8bpc_lsx
    ipred_dc_gen   a2, a3, a4
    ipred_splat_dc a0, a1, a3, a4, t0

endfunc

// void ipred_dc_128_lsx(pixel *dst, const ptrdiff_t stride,
//                       const pixel *const topleft,
//                       const int width, const int height, const int a,
//                       const int max_width, const int max_height
//                       HIGHBD_DECL_SUFFIX)
function ipred_dc_128_8bpc_lsx
    li.w           t0,      128
    ipred_splat_dc a0, a1, a3, a4, t0

endfunc

// void ipred_dc_top_c(pixel *dst, const ptrdiff_t stride,
//                     const pixel *const topleft,
//                     const int width, const int height, const int a,
//                     const int max_width, const int max_height
//                     HIGHBD_DECL_SUFFIX)
function ipred_dc_top_8bpc_lsx
    ipred_dc_gen_top a2, a3
    ipred_splat_dc   a0, a1, a3, a4, t0

endfunc

// void ipred_dc_left_c(pixel *dst, const ptrdiff_t stride,
//                      const pixel *const topleft,
//                      const int width, const int height, const int a,
//                      const int max_width, const int max_height
//                      HIGHBD_DECL_SUFFIX)
function ipred_dc_left_8bpc_lsx
    ipred_dc_gen_left a2, a4
    ipred_splat_dc    a0, a1, a3, a4, t0

endfunc

.macro pixel_set_8bpc dst_ptr, src_ptr, width
    vldrepl.b      vr0,     \src_ptr, 0
1:
    andi           a5,      \width,   64
    beqz           a5,      2f

    vst            vr0,     \dst_ptr, 0
    vst            vr0,     \dst_ptr, 16
    vst            vr0,     \dst_ptr, 32
    vst            vr0,     \dst_ptr, 48
    b              6f
2:
    andi           a5,      \width,   32
    beqz           a5,      3f

    vst            vr0,     \dst_ptr, 0
    vst            vr0,     \dst_ptr, 16
    b              6f
3:
    andi           a5,      \width,   16
    beqz           a5,      4f

    vst            vr0,     \dst_ptr, 0
    b              6f
4:
    andi           a5,      \width,   8
    beqz           a5,      5f

    fst.d          f0,      \dst_ptr, 0
    b              6f
5:
    andi           a5,      \width,   4
    beqz           a5,      6f

    fst.s          f0,      \dst_ptr, 0
6:
.endm

// void ipred_h_c(pixel *dst, const ptrdiff_t stride,
//                const pixel *const topleft,
//                const int width, const int height, const int a,
//                const int max_width, const int max_height
//                HIGHBD_DECL_SUFFIX)
function ipred_h_8bpc_lsx
    beqz           a4,      .IPRED_H_END
.IPRED_H_LOOP:
    addi.d         a2,      a2,      -1

    pixel_set_8bpc a0, a2, a3

    add.d          a0,      a0,      a1
    addi.d         a4,      a4,      -1
    bnez           a4,      .IPRED_H_LOOP

.IPRED_H_END:
endfunc

.macro pixel_copy_8bpc dst_ptr, src_ptr, width
1:
    andi           a5,      \width,   64
    beqz           a5,      2f

    vld            vr0,     \src_ptr, 0
    vld            vr1,     \src_ptr, 16
    vld            vr2,     \src_ptr, 32
    vld            vr3,     \src_ptr, 48

    vst            vr0,     \dst_ptr, 0
    vst            vr1,     \dst_ptr, 16
    vst            vr2,     \dst_ptr, 32
    vst            vr3,     \dst_ptr, 48

    b              6f
2:
    andi           a5,      \width,   32
    beqz           a5,      3f

    vld            vr0,     \src_ptr, 0
    vld            vr1,     \src_ptr, 16

    vst            vr0,     \dst_ptr, 0
    vst            vr1,     \dst_ptr, 16

    b              6f
3:
    andi           a5,      \width,   16
    beqz           a5,      4f

    vld            vr0,     \src_ptr, 0
    vst            vr0,     \dst_ptr, 0

    b              6f
4:
    andi           a5,      \width,   8
    beqz           a5,      5f

    fld.d          f0,      \src_ptr, 0
    fst.d          f0,      \dst_ptr, 0

    b              6f
5:
    andi           a5,      \width,   4
    beqz           a5,      6f

    fld.s          f0,      \src_ptr, 0
    fst.s          f0,      \dst_ptr, 0
6:
.endm

// void ipred_v_lsx(pixel *dst, const ptrdiff_t stride,
//                  const pixel *const topleft,
//                  const int width, const int height, const int a,
//                  const int max_width, const int max_height
//                  HIGHBD_DECL_SUFFIX)
function ipred_v_8bpc_lsx
    beqz           a4,      .IPRED_V_END
    addi.d         a2,      a2,      1
.IPRED_V_LOOP:
    pixel_copy_8bpc  a0, a2, a3

    add.d          a0,      a0,      a1
    addi.d         a4,      a4,      -1
    bnez           a4,      .IPRED_V_LOOP

.IPRED_V_END:
endfunc

// void ipred_paeth_lsx(pixel *dst, const ptrdiff_t stride,
//                      const pixel *const tl_ptr,
//                      const int width, const int height, const int a,
//                      const int max_width, const int max_height
//                      HIGHBD_DECL_SUFFIX)
function ipred_paeth_8bpc_lsx
    vldrepl.b      vr0,     a2,      0    //topleft
    vsllwil.hu.bu  vr0,     vr0,     0
    or             a6,      a2,      a2
    addi.d         a7,      a2,      1

.IPRED_PAETH_H_LOOP:
    addi.d         a6,      a6,      -1
    vldrepl.b      vr1,     a6,      0   //left
    vsllwil.hu.bu  vr1,     vr1,     0

.IPRED_PAETH_W_LOOP64:
    andi           a5,      a3,      64
    beqz           a5,      .IPRED_PAETH_W_LOOP32

    vld            vr2,     a7,      0   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      0

    vld            vr2,     a7,      16   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      16

    vld            vr2,     a7,      32   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      32

    vld            vr2,     a7,      48   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      48

    b              .IPRED_PAETH_W_LOOPEND

.IPRED_PAETH_W_LOOP32:
    andi           a5,      a3,      32
    beqz           a5,      .IPRED_PAETH_W_LOOP16

    vld            vr2,     a7,      0   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      0

    vld            vr2,     a7,      16   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      16

    b              .IPRED_PAETH_W_LOOPEND

.IPRED_PAETH_W_LOOP16:
    andi           a5,      a3,      16
    beqz           a5,      .IPRED_PAETH_W_LOOP8

    vld            vr2,     a7,      0   //top
    vpermi.w       vr9,     vr2,     0x0e
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr9,     vr9,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff
    vabsd.hu       vr10,    vr0,     vr9

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vadd.h         vr11,    vr1,     vr9
    vabsd.hu       vr6,     vr3,     vr6  //tldiff
    vabsd.hu       vr11,    vr3,     vr11 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0

    vsle.hu        vr12,    vr5,     vr11
    vbitsel.v      vr7,     vr0,     vr9,    vr12
    vsle.hu        vr12,    vr10,    vr5
    vsle.hu        vr8,     vr10,    vr11
    vand.v         vr12,    vr12,    vr8
    vbitsel.v      vr12,    vr7,     vr1,    vr12
    vsrlni.b.h     vr12,    vr12,    0

    vpermi.w       vr12,    vr3,     0x44

    vst            vr12,    a0,      0

    b              .IPRED_PAETH_W_LOOPEND

.IPRED_PAETH_W_LOOP8:
    andi           a5,      a3,      8
    beqz           a5,      .IPRED_PAETH_W_LOOP4

    fld.d          f2,      a7,      0   //top
    vsllwil.hu.bu  vr2,     vr2,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vabsd.hu       vr6,     vr3,     vr6 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0
    fst.d          f3,      a0,      0

    b              .IPRED_PAETH_W_LOOPEND

.IPRED_PAETH_W_LOOP4:
    andi           a5,      a3,      4
    beqz           a5,      .IPRED_PAETH_W_LOOPEND

    fld.s          f2,      a7,      0   //top
    vsllwil.hu.bu  vr2,     vr2,     0

    vabsd.hu       vr5,     vr0,     vr1  //tdiff
    vabsd.hu       vr4,     vr0,     vr2  //ldiff

    vadd.h         vr3,     vr0,     vr0
    vadd.h         vr6,     vr1,     vr2
    vabsd.hu       vr6,     vr3,     vr6 //tldiff

    vsle.hu        vr3,     vr5,     vr6
    vbitsel.v      vr7,     vr0,     vr2,    vr3
    vsle.hu        vr3,     vr4,     vr5
    vsle.hu        vr8,     vr4,     vr6
    vand.v         vr3,     vr3,     vr8
    vbitsel.v      vr3,     vr7,     vr1,    vr3
    vsrlni.b.h     vr3,     vr3,     0
    fst.s          f3,      a0,      0

    b              .IPRED_PAETH_W_LOOPEND

.IPRED_PAETH_W_LOOPEND:
    add.d         a0,       a0,      a1
    addi.d        a4,       a4,      -1
    bnez          a4,       .IPRED_PAETH_H_LOOP
endfunc

const dav1d_sm_weights
    .byte  0,   0
    // bs = 2
    .byte  255, 128
    // bs = 4
    .byte  255, 149,  85,  64
    // bs = 8
    .byte  255, 197, 146, 105,  73,  50,  37,  32
    // bs = 16
    .byte  255, 225, 196, 170, 145, 123, 102,  84
    .byte  68,  54,  43,  33,  26,  20,  17,  16
    // bs = 32
    .byte  255, 240, 225, 210, 196, 182, 169, 157
    .byte  145, 133, 122, 111, 101,  92,  83,  74
    .byte  66,  59,  52,  45,  39,  34,  29,  25
    .byte  21,  17,  14,  12,  10,   9,   8,   8
    // bs = 64
    .byte  255, 248, 240, 233, 225, 218, 210, 203
    .byte  196, 189, 182, 176, 169, 163, 156, 150
    .byte  144, 138, 133, 127, 121, 116, 111, 106
    .byte  101,  96,  91,  86,  82,  77,  73,  69
    .byte  65,  61,  57,  54,  50,  47,  44,  41
    .byte  38,  35,  32,  29,  27,  25,  22,  20
    .byte  18,  16,  15,  13,  12,  10,   9,   8
    .byte  7,   6,   6,   5,   5,   4,   4,   4
endconst

// void ipred_smooth_lsx(pixel *dst, const ptrdiff_t stride,
//                       const pixel *const topleft,
//                       const int width, const int height, const int a,
//                       const int max_width, const int max_height
//                       HIGHBD_DECL_SUFFIX)
function ipred_smooth_8bpc_lsx
    la.local       a5,      dav1d_sm_weights
    add.d          a6,      a5,      a3  //hor
    add.d          a5,      a5,      a4  //ver

    add.d          a7,      a2,      a3
    sub.d          t0,      a2,      a4

    vldrepl.b      vr0,     a7,      0  //right
    vldrepl.b      vr1,     t0,      0  //bottom

    vsllwil.hu.bu  vr0,     vr0,     0
    vsllwil.wu.hu  vr0,     vr0,     0
    vsllwil.hu.bu  vr1,     vr1,     0
    vsllwil.wu.hu  vr1,     vr1,     0

    li.w           t0,      256
    vreplgr2vr.w   vr6,     t0

    addi.d         t0,      a2,      1   //ptr topleft[x]
    addi.d         t3,      a2,      -1  //ptr topleft[y]

.IPRED_SMOOTH_H_LOOP:
    vldrepl.b      vr2,     a5,      0  //ver[y]
    vldrepl.b      vr3,     t3,      0  //topleft[y]

    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.wu.hu  vr2,     vr2,     0
    vsllwil.hu.bu  vr3,     vr3,     0
    vsllwil.wu.hu  vr3,     vr3,     0

    vsub.w         vr7,     vr6,     vr2  //256-ver[y]

    or             t1,      zero,    zero  //xx
    srai.d         t2,      a3,      2     //loop max

.IPRED_SMOOTH_W_LOOP:
    fldx.s         f4,      t0,      t1   //topleft[x]
    fldx.s         f5,      a6,      t1   //hor[x]

    vsllwil.hu.bu  vr4,     vr4,     0
    vsllwil.wu.hu  vr4,     vr4,     0
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.wu.hu  vr5,     vr5,     0

    vsub.w         vr8,     vr6,     vr5  //256-hor[x]

    vmul.w         vr9,     vr8,     vr0
    vmadd.w        vr9,     vr5,     vr3
    vmadd.w        vr9,     vr7,     vr1
    vmadd.w        vr9,     vr2,     vr4  //pred

    vadd.w         vr9,     vr9,     vr6
    vsrlni.h.w     vr9,     vr9,     9
    vsrlni.b.h     vr9,     vr9,     0

    fstx.s         f9,      a0,      t1

    addi.d         t1,      t1,      4
    addi.d         t2,      t2,      -1
    bnez           t2,      .IPRED_SMOOTH_W_LOOP

.IPRED_SMOOTH_W_LOOP_END:
    addi.d         t3,      t3,      -1
    addi.d         a5,      a5,      1
    add.d          a0,      a0,      a1
    addi.d         a4,      a4,      -1
    bnez           a4,      .IPRED_SMOOTH_H_LOOP

endfunc

// void ipred_smooth_v_lsx(pixel *dst, const ptrdiff_t stride,
//                         const pixel *const topleft,
//                         const int width, const int height, const int a,
//                         const int max_width, const int max_height
//                         HIGHBD_DECL_SUFFIX)
function ipred_smooth_v_8bpc_lsx
    la.local       a5,      dav1d_sm_weights
    add.d          a5,      a5,      a4  //ver

    sub.d          t0,      a2,      a4
    vldrepl.b      vr0,     t0,      0  //bottom
    vsllwil.hu.bu  vr0,     vr0,     0

    li.w           t0,      256
    vreplgr2vr.h   vr2,     t0
    li.w           t0,      128
    vreplgr2vr.h   vr3,     t0

    addi.d         t0,      a2,      1   //ptr topleft[x]

.IPRED_SMOOTH_V_H_LOOP:
    vldrepl.b      vr1,     a5,      0  //ver[y]
    vsllwil.hu.bu  vr1,     vr1,     0
    vsub.h         vr5,     vr2,     vr1  //256-ver[y]

    or             t1,      zero,    zero  //xx
    srai.d         t2,      a3,      3     //loop max
    beqz           t2,      .IPRED_SMOOTH_V_W_LOOP4

.IPRED_SMOOTH_V_W_LOOP8:
    fldx.d         f4,      t0,      t1   //topleft[x]
    vsllwil.hu.bu  vr4,     vr4,     0

    vmul.h         vr6,     vr5,     vr0
    vmadd.h        vr6,     vr1,     vr4  //pred
    vadd.h         vr6,     vr6,     vr3
    vsrlni.b.h     vr6,     vr6,     8

    fstx.d         f6,      a0,      t1

    addi.d         t1,      t1,      8
    addi.d         t2,      t2,      -1
    bnez           t2,      .IPRED_SMOOTH_V_W_LOOP8
    b              .IPRED_SMOOTH_V_W_LOOP_END

.IPRED_SMOOTH_V_W_LOOP4:
    fldx.s         f4,      t0,      t1   //topleft[x]
    vsllwil.hu.bu  vr4,     vr4,     0

    vmul.h         vr6,     vr5,     vr0
    vmadd.h        vr6,     vr1,     vr4  //pred
    vadd.h         vr6,     vr6,     vr3
    vsrai.h        vr6,     vr6,     8
    vsrlni.b.h     vr6,     vr6,     0

    fstx.s         f6,      a0,      t1

    addi.d         t1,      t1,      4

.IPRED_SMOOTH_V_W_LOOP_END:
    addi.d         a5,      a5,      1
    add.d          a0,      a0,      a1
    addi.d         a4,      a4,      -1
    bnez           a4,      .IPRED_SMOOTH_V_H_LOOP

endfunc

// void ipred_smooth_h_lsx(pixel *dst, const ptrdiff_t stride,
//                         const pixel *const topleft,
//                         const int width, const int height, const int a,
//                         const int max_width, const int max_height
//                         HIGHBD_DECL_SUFFIX)
function ipred_smooth_h_8bpc_lsx
    la.local       a5,      dav1d_sm_weights
    add.d          a6,      a5,      a3  //hor

    add.d          a7,      a2,      a3
    vldrepl.b      vr0,     a7,      0  //right
    vsllwil.hu.bu  vr0,     vr0,     0

    li.w           t0,      256
    vreplgr2vr.h   vr1,     t0
    li.w           t0,      128
    vreplgr2vr.h   vr2,     t0

    addi.d         t3,      a2,      -1  //ptr topleft[y]

.IPRED_SMOOTH_H_H_LOOP:
    vldrepl.b      vr3,     t3,      0  //topleft[y]
    vsllwil.hu.bu  vr3,     vr3,     0

    or             t1,      zero,    zero  //xx
    srai.d         t2,      a3,      3     //loop max
    beqz           t2,      .IPRED_SMOOTH_H_W_LOOP4

.IPRED_SMOOTH_H_W_LOOP8:
    fldx.d         f5,      a6,      t1   //hor[x]
    vsllwil.hu.bu  vr5,     vr5,     0
    vsub.h         vr4,     vr1,     vr5  //256-hor[x]

    vmul.h         vr6,     vr4,     vr0
    vmadd.h        vr6,     vr5,     vr3  //pred
    vadd.h         vr6,     vr6,     vr2
    vsrlni.b.h     vr6,     vr6,     8

    fstx.d         f6,      a0,      t1

    addi.d         t1,      t1,      8
    addi.d         t2,      t2,      -1
    bnez           t2,      .IPRED_SMOOTH_H_W_LOOP8
    b              .IPRED_SMOOTH_W_H_LOOP_END

.IPRED_SMOOTH_H_W_LOOP4:
    fldx.s         f5,      a6,      t1   //hor[x]
    vsllwil.hu.bu  vr5,     vr5,     0
    vsub.h         vr4,     vr1,     vr5  //256-hor[x]

    vmul.h         vr6,     vr4,     vr0
    vmadd.h        vr6,     vr5,     vr3  //pred
    vadd.h         vr6,     vr6,     vr2
    vsrai.h        vr6,     vr6,     8
    vsrlni.b.h     vr6,     vr6,     0

    fstx.s         f6,      a0,      t1

    addi.d         t1,      t1,      4

.IPRED_SMOOTH_W_H_LOOP_END:
    addi.d         t3,      t3,      -1
    add.d          a0,      a0,      a1
    addi.d         a4,      a4,      -1
    bnez           a4,      .IPRED_SMOOTH_H_H_LOOP

endfunc

// void pal_pred_lsx(pixel *dst, const ptrdiff_t stride,
//                   const pixel *const pal, const uint8_t *idx,
//                   const int w, const int h)
function pal_pred_8bpc_lsx
    srai.d         a7,      a5,      2

.PAL_PRED_WLOOP4:
    andi           a6,      a4,      4
    beqz           a6,      .PAL_PRED_WLOOP8
    fld.d          f0,      a3,      0
    vsrli.b        vr1,     vr0,     4
    vandi.b        vr2,     vr0,     7
    vilvl.b        vr0,     vr1,     vr2
    fld.d          f1,      a2,      0
    vshuf.b        vr2,     vr1,     vr1,    vr0

    vstelm.w       vr2,     a0,      0,      0
    add.d          a0,      a0,      a1
    vstelm.w       vr2,     a0,      0,      1
    add.d          a0,      a0,      a1
    vstelm.w       vr2,     a0,      0,      2
    add.d          a0,      a0,      a1
    vstelm.w       vr2,     a0,      0,      3
    add.d          a0,      a0,      a1

    addi.d         a3,      a3,      8
    addi.d         a7,      a7,      -1
    bnez           a7,      .PAL_PRED_WLOOP4
    b              .PAL_PRED_END

.PAL_PRED_WLOOP8:
    andi           a6,      a4,      8
    beqz           a6,      .PAL_PRED_WLOOP16

    vld            vr0,     a3,      0
    vsrli.b        vr1,     vr0,     4
    vandi.b        vr2,     vr0,     7
    vilvl.b        vr0,     vr1,     vr2
    vilvh.b        vr3,     vr1,     vr2
    fld.d          f1,      a2,      0
    vshuf.b        vr0,     vr1,     vr1,    vr0
    vshuf.b        vr3,     vr1,     vr1,    vr3

    vstelm.d       vr0,     a0,      0,      0
    add.d          a0,      a0,      a1
    vstelm.d       vr0,     a0,      0,      1
    add.d          a0,      a0,      a1

    vstelm.d       vr3,     a0,      0,      0
    add.d          a0,      a0,      a1
    vstelm.d       vr3,     a0,      0,      1
    add.d          a0,      a0,      a1

    addi.d         a3,      a3,      16
    addi.d         a7,      a7,      -1
    bnez           a7,      .PAL_PRED_WLOOP8
    b              .PAL_PRED_END

.PAL_PRED_WLOOP16:
    andi           a6,      a4,      16
    beqz           a6,      .PAL_PRED_WLOOP32

    vld            vr0,     a3,      0
    vld            vr1,     a3,      16
    fld.d          f6,      a2,      0
    vsrli.b        vr2,     vr0,     4
    vandi.b        vr3,     vr0,     7
    vsrli.b        vr4,     vr1,     4
    vandi.b        vr5,     vr1,     7
    vilvl.b        vr0,     vr2,     vr3
    vilvh.b        vr1,     vr2,     vr3
    vilvl.b        vr2,     vr4,     vr5
    vilvh.b        vr3,     vr4,     vr5
    vshuf.b        vr0,     vr6,     vr6,    vr0
    vshuf.b        vr1,     vr6,     vr6,    vr1
    vshuf.b        vr2,     vr6,     vr6,    vr2
    vshuf.b        vr3,     vr6,     vr6,    vr3

    vst            vr0,     a0,      0
    add.d          a0,      a0,      a1
    vst            vr1,     a0,      0
    add.d          a0,      a0,      a1
    vst            vr2,     a0,      0
    add.d          a0,      a0,      a1
    vst            vr3,     a0,      0
    add.d          a0,      a0,      a1

    addi.d         a3,      a3,      32
    addi.d         a7,      a7,      -1
    bnez           a7,      .PAL_PRED_WLOOP16
    b              .PAL_PRED_END

.PAL_PRED_WLOOP32:
    andi           a6,      a4,      32
    beqz           a6,      .PAL_PRED_WLOOP64

    vld            vr0,     a3,      0
    vld            vr1,     a3,      16
    vld            vr2,     a3,      32
    vld            vr3,     a3,      48
    fld.d          f4,      a2,      0
    vsrli.b        vr5,     vr0,     4
    vandi.b        vr6,     vr0,     7
    vsrli.b        vr7,     vr1,     4
    vandi.b        vr8,     vr1,     7
    vsrli.b        vr9,     vr2,     4
    vandi.b        vr10,    vr2,     7
    vsrli.b        vr11,    vr3,     4
    vandi.b        vr12,    vr3,     7
    vilvl.b        vr0,     vr5,     vr6
    vilvh.b        vr1,     vr5,     vr6
    vilvl.b        vr2,     vr7,     vr8
    vilvh.b        vr3,     vr7,     vr8
    vilvl.b        vr5,     vr9,     vr10
    vilvh.b        vr6,     vr9,     vr10
    vilvl.b        vr7,     vr11,    vr12
    vilvh.b        vr8,     vr11,    vr12
    vshuf.b        vr0,     vr4,     vr4,    vr0
    vshuf.b        vr1,     vr4,     vr4,    vr1
    vshuf.b        vr2,     vr4,     vr4,    vr2
    vshuf.b        vr3,     vr4,     vr4,    vr3
    vshuf.b        vr5,     vr4,     vr4,    vr5
    vshuf.b        vr6,     vr4,     vr4,    vr6
    vshuf.b        vr7,     vr4,     vr4,    vr7
    vshuf.b        vr8,     vr4,     vr4,    vr8

    vst            vr0,     a0,      0
    vst            vr1,     a0,      16
    add.d          a0,      a0,      a1
    vst            vr2,     a0,      0
    vst            vr3,     a0,      16
    add.d          a0,      a0,      a1
    vst            vr5,     a0,      0
    vst            vr6,     a0,      16
    add.d          a0,      a0,      a1
    vst            vr7,     a0,      0
    vst            vr8,     a0,      16
    add.d          a0,      a0,      a1

    addi.d         a3,      a3,      64
    addi.d         a7,      a7,      -1
    bnez           a7,      .PAL_PRED_WLOOP32
    b              .PAL_PRED_END

.PAL_PRED_WLOOP64:
    vld            vr0,     a3,      0
    vld            vr1,     a3,      16
    fld.d          f2,      a2,      0
    vsrli.b        vr3,     vr0,     4
    vandi.b        vr4,     vr0,     7
    vsrli.b        vr5,     vr1,     4
    vandi.b        vr6,     vr1,     7
    vilvl.b        vr0,     vr3,     vr4
    vilvh.b        vr1,     vr3,     vr4
    vilvl.b        vr3,     vr5,     vr6
    vilvh.b        vr4,     vr5,     vr6
    vshuf.b        vr0,     vr2,     vr2,    vr0
    vshuf.b        vr1,     vr2,     vr2,    vr1
    vshuf.b        vr3,     vr2,     vr2,    vr3
    vshuf.b        vr4,     vr2,     vr2,    vr4

    vst            vr0,     a0,      0
    vst            vr1,     a0,      16
    vst            vr3,     a0,      32
    vst            vr4,     a0,      48

    add.d          a0,      a0,      a1
    addi.d         a3,      a3,      32
    addi.d         a5,      a5,      -1
    bnez           a5,      .PAL_PRED_WLOOP64

.PAL_PRED_END:
endfunc

.macro apply_sign_vrh v, s, vrzero, vrt0 ,out
    vslt.h         \vrt0,   \s,      \vrzero
    vandn.v        \s,      \vrt0,   \v
    vsigncov.h     \v,      \vrt0,   \v
    vor.v          \out,    \s,      \v
.endm

.macro iclip_pixel_vrh in0, in1, in2, tmp0, tmp1, out
    vmin.h         \tmp0,   \in2,    \in0
    vslt.h         \in0,    \in0,    \in1
    vand.v         \tmp1,   \in0,    \in1
    vandn.v        \tmp0,   \in0,    \tmp0
    vor.v          \out,    \tmp1,   \tmp0
.endm

.macro ipred_cfl_pred dst, stride, w, h, dc, ac, alpha
    vreplgr2vr.h   vr2,     \alpha
    vreplgr2vr.h   vr7,     \dc
    li.w           t1,      32
    vreplgr2vr.h   vr3,     t1
    vxor.v         vr4,     vr4,     vr4
    li.w           t1,      255
    vreplgr2vr.h   vr6,     t1
    add.d          t4,      \w,      \w

1:
    or             t1,      zero,    zero
    or             t2,      zero,    zero
    srai.d         t3,      \w,      3
    beqz           t3,      3f

2:
    vldx           vr0,     \ac,     t1
    vmul.h         vr1,     vr2,     vr0
    vadda.h        vr0,     vr1,     vr3
    vsrai.h        vr0,     vr0,     6
    apply_sign_vrh vr0, vr1, vr4, vr5, vr0
    vadd.h         vr1,     vr0,     vr7
    iclip_pixel_vrh vr1, vr4, vr6, vr5, vr8, vr0
    vsrlni.b.h     vr0,     vr0,     0
    fstx.d         f0,      \dst,    t2

    addi.d         t1,      t1,      16
    addi.d         t2,      t2,      8
    addi.d         t3,      t3,      -1
    bnez           t3,      2b
    b              4f

3:
    fld.d          f0,      \ac,     0
    vmul.h         vr1,     vr2,     vr0
    vadda.h        vr0,     vr1,     vr3
    vsrai.h        vr0,     vr0,     6
    apply_sign_vrh vr0, vr1, vr4, vr5, vr0
    vadd.h         vr1,     vr0,     vr7
    iclip_pixel_vrh vr1, vr4, vr6, vr5, vr8, vr0
    vsrlni.b.h     vr0,     vr0,     0
    fst.s          f0,      \dst,    0

4:
    add.d          \ac,     \ac,     t4
    add.d          \dst,    \dst,    \stride
    addi.d         \h,      \h,      -1
    bnez           \h,      1b
.endm

function ipred_cfl_8bpc_lsx
    ipred_dc_gen   a2, a3, a4
    ipred_cfl_pred a0, a1, a3, a4, t0, a5, a6
endfunc

function ipred_cfl_top_8bpc_lsx
    ipred_dc_gen_top   a2, a3
    ipred_cfl_pred a0, a1, a3, a4, t0, a5, a6
endfunc

function ipred_cfl_left_8bpc_lsx
    ipred_dc_gen_left   a2, a4
    ipred_cfl_pred a0, a1, a3, a4, t0, a5, a6
endfunc

function ipred_cfl_128_8bpc_lsx
    li.w           t0,      128
    ipred_cfl_pred a0, a1, a3, a4, t0, a5, a6
endfunc

const dav1d_filter_intra_taps_lsx
    //arr0  8*7
.byte    -6, -5, -3, -3, -4, -3, -3, -3
.byte    10,  2,  1,  1,  6,  2,  2,  1
.byte    0, 10,  1,  1,  0,  6,  2,  2
.byte    0,  0, 10,  2,  0,  0,  6,  2
.byte    0,  0,  0, 10,  0,  0,  0,  6
.byte    12,  9,  7,  5,  2,  2,  2,  3
.byte    0,  0,  0,  0, 12,  9,  7,  5
    //arr1
.byte    -10,  -6,  -4,  -2, -10,  -6,  -4,  -2
.byte    16,   0,   0,   0,  16,   0,   0,   0
.byte    0,  16,   0,   0,   0,  16,   0,   0
.byte    0,   0,  16,   0,   0,   0,  16,   0
.byte    0,   0,   0,  16,   0,   0,   0,  16
.byte    10,   6,   4,   2,   0,   0,   0,   0
.byte    0,   0,   0,   0,  10,   6,   4,   2
    //arr2
.byte    -8,  -8,  -8,  -8,  -4,  -4,  -4,  -4
.byte    8,   0,   0,   0,   4,   0,   0,   0
.byte    0,   8,   0,   0,   0,   4,   0,   0
.byte    0,   0,   8,   0,   0,   0,   4,   0
.byte    0,   0,   0,   8,   0,   0,   0,   4
.byte    16,  16,  16,  16,   0,   0,   0,   0
.byte    0,   0,   0,   0,  16,  16,  16,  16
    //arr3
.byte    -2,  -1,  -1,   0,  -1,  -1,  -1,  -1
.byte    8,   3,   2,   1,   4,   3,   2,   2
.byte    0,   8,   3,   2,   0,   4,   3,   2
.byte    0,   0,   8,   3,   0,   0,   4,   3
.byte    0,   0,   0,   8,   0,   0,   0,   4
.byte    10,   6,   4,   2,   3,   4,   4,   3
.byte    0,   0,   0,   0,  10,   6,   4,   3
    //arr4
.byte    -12, -10,  -9,  -8, -10,  -9,  -8,  -7
.byte    14,   0,   0,   0,  12,   1,   0,   0
.byte    0,  14,   0,   0,   0,  12,   0,   0
.byte    0,   0,  14,   0,   0,   0,  12,   1
.byte    0,   0,   0,  14,   0,   0,   0,  12
.byte    14,  12,  11,  10,   0,   0,   1,   1
.byte    0,   0,   0,   0,  14,  12,  11,   9
endconst

.macro ipred_filter_load_p
    vldrepl.b      vr0,     t0,      0
    vldrepl.b      vr1,     a7,      0
    vldrepl.b      vr2,     a7,      1
    vldrepl.b      vr3,     a7,      2
    vldrepl.b      vr4,     a7,      3
    vldrepl.b      vr5,     t1,      0
    vldrepl.b      vr6,     t1,      -1

    vsllwil.hu.bu  vr0,     vr0,     0
    vsllwil.hu.bu  vr1,     vr1,     0
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr3,     vr3,     0
    vsllwil.hu.bu  vr4,     vr4,     0
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.hu.bu  vr6,     vr6,     0
.endm

.macro ipred_filter_loadx_p
    vldrepl.b      vr0,     t0,      0
    vldrepl.b      vr1,     a7,      0
    vldrepl.b      vr2,     a7,      1
    vldrepl.b      vr3,     a7,      2
    vldrepl.b      vr4,     a7,      3
    vldrepl.b      vr5,     t1,      0
    ldx.bu         t3,      t1,      a1
    vreplgr2vr.b   vr6,     t3

    vsllwil.hu.bu  vr0,     vr0,     0
    vsllwil.hu.bu  vr1,     vr1,     0
    vsllwil.hu.bu  vr2,     vr2,     0
    vsllwil.hu.bu  vr3,     vr3,     0
    vsllwil.hu.bu  vr4,     vr4,     0
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.hu.bu  vr6,     vr6,     0
.endm

.macro ipred_filter_load_fltptr
    fld.d          f7,      a6,      0
    fld.d          f8,      a6,      8
    fld.d          f9,      a6,      16
    fld.d          f10,     a6,      24
    fld.d          f11,     a6,      32
    fld.d          f12,     a6,      40
    fld.d          f13,     a6,      48

    vsllwil.h.b    vr7,     vr7,     0
    vsllwil.h.b    vr8,     vr8,     0
    vsllwil.h.b    vr9,     vr9,     0
    vsllwil.h.b    vr10,    vr10,    0
    vsllwil.h.b    vr11,    vr11,    0
    vsllwil.h.b    vr12,    vr12,    0
    vsllwil.h.b    vr13,    vr13,    0
.endm

.macro ipred_filter_calc_acc
    vmul.h         vr7,     vr7,     vr0
    vmadd.h        vr7,     vr8,     vr1
    vmadd.h        vr7,     vr9,     vr2
    vmadd.h        vr7,     vr10,    vr3
    vmadd.h        vr7,     vr11,    vr4
    vmadd.h        vr7,     vr12,    vr5
    vmadd.h        vr7,     vr13,    vr6
    vaddi.hu       vr7,     vr7,     8
    vsrai.h        vr7,     vr7,     4
    iclip_pixel_vrh vr7, vr14, vr15, vr9, vr10, vr8
    vsrlni.b.h     vr8,     vr8,     0
.endm

// void ipred_filter_lsx(pixel *dst, const ptrdiff_t stride,
//                       const pixel *const topleft_in,
//                       const int width, const int height, int filt_idx,
//                       const int max_width, const int max_height
//                       HIGHBD_DECL_SUFFIX)
function ipred_filter_8bpc_lsx
    andi           a5,      a5,      511
    la.local       a6,      dav1d_filter_intra_taps_lsx
    li.w           a7,      56
    mul.w          a7,      a7,      a5
    add.d          a6,      a6,      a7   //*filter
    addi.d         a7,      a2,      1    //*top
    or             a5,      zero,    zero //y
    vxor.v         vr14,    vr14,    vr14
    li.w           t0,      255
    vreplgr2vr.h   vr15,    t0

.FILTER_LOOP_H:
    sub.d          t0,      a2,      a5   //*topleft
    addi.d         t1,      t0,      -1   //left

    ctz.w          t2,      a3
    addi.d         t3,      t2,      -2
    beqz           t3,      .FILTER_LOOP_W4
    addi.d         t3,      t2,      -3
    beqz           t3,      .FILTER_LOOP_W8
    addi.d         t3,      t2,      -4
    beqz           t3,      .FILTER_LOOP_W16
    addi.d         t3,      t2,      -5
    beqz           t3,      .FILTER_LOOP_W32

.FILTER_LOOP_W4:
    ipred_filter_load_p

    or             t3,      a0,      a0  //*ptr

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    b              .FILTER_LOOP_W_END

.FILTER_LOOP_W8:
    ipred_filter_load_p

    or             t3,      a0,      a0

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      3
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      4

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    b              .FILTER_LOOP_W_END

.FILTER_LOOP_W16:
    ipred_filter_load_p

    or             t3,      a0,      a0

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      3
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      4

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      7
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      8

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      11
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      12

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    b              .FILTER_LOOP_W_END

.FILTER_LOOP_W32:
    ipred_filter_load_p

    or             t3,      a0,      a0

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      3
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      4

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      7
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      8

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      11
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      12

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      15
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      16

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      19
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      20

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      23
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      24

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

    addi.d         t1,      a0,      27
    addi.d         a7,      a7,      4
    addi.d         t0,      a7,      -1

    ipred_filter_loadx_p

    addi.d         t3,      a0,      28

    ipred_filter_load_fltptr
    ipred_filter_calc_acc

    fst.s          f8,      t3,      0
    add.d          t3,      t3,      a1
    vstelm.w       vr8,     t3,      0,      1
    add.d          t3,      t3,      a1

.FILTER_LOOP_W_END:
    add.d          a7,      a0,      a1
    add.d          t2,      a1,      a1
    add.d          a0,      a0,      t2
    addi.d         a5,      a5,      2
    blt            a5,      a4,      .FILTER_LOOP_H
endfunc

const dav1d_dr_intra_derivative
    // Values that are 0 will never be used
    .short  0         // Angles:
    .short  1023, 0   //  3,  93, 183
    .short  547       //  6,  96, 186
    .short  372, 0, 0 //  9,  99, 189
    .short  273       // 14, 104, 194
    .short  215, 0    // 17, 107, 197
    .short  178       // 20, 110, 200
    .short  151, 0    // 23, 113, 203 (113 & 203 are base angles)
    .short  132       // 26, 116, 206
    .short  116, 0    // 29, 119, 209
    .short  102, 0    // 32, 122, 212
    .short  90        // 36, 126, 216
    .short  80, 0     // 39, 129, 219
    .short  71        // 42, 132, 222
    .short  64, 0     // 45, 135, 225 (45 & 135 are base angles)
    .short  57        // 48, 138, 228
    .short  51, 0     // 51, 141, 231
    .short  45, 0     // 54, 144, 234
    .short  40        // 58, 148, 238
    .short  35, 0     // 61, 151, 241
    .short  31        // 64, 154, 244
    .short  27, 0     // 67, 157, 247 (67 & 157 are base angles)
    .short  23        // 70, 160, 250
    .short  19, 0     // 73, 163, 253
    .short  15, 0     // 76, 166, 256
    .short  11, 0     // 81, 171, 261
    .short  7         // 84, 174, 264
    .short  3         // 87, 177, 267
endconst

const z1_upsample_edge_kernel
    .short  -1, 9, 9, -1, -1, 9, 9, -1
endconst

const ipred_filter_edge_kernel1
    .short  0, 4, 8, 4, 0, 4, 8, 4
    .short  0, 5, 6, 5, 0, 5, 6, 5
    .short  2, 4, 4, 4, 2, 4, 4, 4
endconst

const ipred_filter_edge_kernel2
    .short  0, 0, 0, 0, 0, 0, 0, 0
    .short  0, 0, 0, 0, 0, 0, 0, 0
    .short  2, 2, 2, 2, 2, 2, 2, 2
endconst

.macro z1_upsample_edge_calc_loop
    vsllwil.hu.bu  vr10,    vr7,     0
    vsllwil.hu.bu  vr11,    vr11,    0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0

    vmul.h         vr10,    vr10,    vr0
    vmul.h         vr11,    vr11,    vr0
    vmul.h         vr12,    vr12,    vr0
    vmul.h         vr13,    vr13,    vr0

    vhaddw.w.h     vr10,    vr10,    vr10
    vhaddw.w.h     vr11,    vr11,    vr11
    vhaddw.w.h     vr12,    vr12,    vr12
    vhaddw.w.h     vr13,    vr13,    vr13
    vhaddw.d.w     vr10,    vr10,    vr10
    vhaddw.d.w     vr11,    vr11,    vr11
    vhaddw.d.w     vr12,    vr12,    vr12
    vhaddw.d.w     vr13,    vr13,    vr13

    vpackev.h      vr10,    vr11,    vr10
    vpackev.h      vr11,    vr13,    vr12
    vpackev.w      vr12,    vr11,    vr10  //s:01234567
    vsrari.h       vr12,    vr12,    4
    iclip_pixel_vrh vr12, vr15, vr16, vr10, vr11, vr12
    vsrlni.b.h     vr12,    vr12,    0  //out: 13579...
    vbsrl.v        vr11,    vr7,     1  //out:02468...
    vilvl.b        vr13,    vr12,    vr11
.endm

.macro z1_upsample_edge_data_init1
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
.endm

.macro z1_upsample_edge_data_init2
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_upsample_edge_calc_loop
.endm

.macro z1_upsample_edge_calc_other
    vsllwil.hu.bu  vr10,    vr7,     0
    vmul.h         vr10,    vr10,    vr0
    vhaddw.w.h     vr10,    vr10,    vr10
    vhaddw.d.w     vr10,    vr10,    vr10
    vreplvei.h     vr12,    vr10,    0   //s0-s7
    vsrari.h       vr12,    vr12,    4

    iclip_pixel_vrh vr12, vr15, vr16, vr10, vr11, vr12
    vsrlni.b.h     vr12,    vr12,    0
    vilvl.b        vr13,    vr12,    vr7
.endm

.macro z1_filter_edge_calc_loop1
    vmul.h         vr10,    vr10,    vr1
    vmul.h         vr11,    vr11,    vr1
    vmul.h         vr12,    vr12,    vr1
    vmul.h         vr13,    vr13,    vr1

    vhaddw.w.h     vr10,    vr10,    vr10
    vhaddw.w.h     vr11,    vr11,    vr11
    vhaddw.w.h     vr12,    vr12,    vr12
    vhaddw.w.h     vr13,    vr13,    vr13
    vhaddw.d.w     vr10,    vr10,    vr10
    vhaddw.d.w     vr11,    vr11,    vr11
    vhaddw.d.w     vr12,    vr12,    vr12
    vhaddw.d.w     vr13,    vr13,    vr13

    vpackev.h      vr10,    vr11,    vr10
    vpackev.h      vr11,    vr13,    vr12
    vpackev.w      vr10,    vr11,    vr10  //s:01234567
.endm

.macro z1_filter_edge_calc_loop2
    vsllwil.hu.bu  vr13,    vr13,    0
    vmadd.h        vr10,    vr13,    vr6
    vsrari.h       vr12,    vr10,    4
    vsrlni.b.h     vr12,    vr12,    0  //out: 0-7
.endm

.macro z1_filter_edge_calc_other
    vsllwil.hu.bu  vr10,    vr10,    0
    vmul.h         vr11,    vr10,    vr1
    vhaddw.w.h     vr11,    vr11,    vr11
    vhaddw.d.w     vr11,    vr11,    vr11
    vreplvei.h     vr12,    vr11,    4
    vextrins.h     vr12,    vr11,    0x00

    vreplvei.h     vr13,    vr10,    1
    vmadd.h        vr12,    vr13,    vr6
    vsrari.h       vr12,    vr12,    4
    vsrlni.b.h     vr12,    vr12,    0  //out: 0-7
.endm

.macro z1_filter_edge_data_init1
    vbsll.v        vr10,    vr7,     1
    vextrins.b     vr10,    vr10,    0x01
    vbsrl.v        vr12,    vr7,     1
    vbsrl.v        vr13,    vr7,     2
    vsllwil.hu.bu  vr10,    vr10,    0
    vsllwil.hu.bu  vr11,    vr7,     0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0
    z1_filter_edge_calc_loop1
.endm

.macro z1_filter_edge_data_init2
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vbsrl.v        vr13,    vr7,     3
    vsllwil.hu.bu  vr10,    vr7,     0
    vsllwil.hu.bu  vr11,    vr11,    0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0
    z1_filter_edge_calc_loop1
.endm

.macro z1_filter_edge_data_init3
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x76
    vsllwil.hu.bu  vr10,    vr7,     0
    vsllwil.hu.bu  vr11,    vr11,    0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0
    z1_filter_edge_calc_loop1
.endm

.macro z1_filter_edge_data_init4
    vbsll.v        vr10,    vr7,     1
    vextrins.b     vr10,    vr10,    0x01
    vbsrl.v        vr12,    vr7,     1
    vbsrl.v        vr13,    vr7,     2
    vextrins.b     vr13,    vr13,    0x76
    vsllwil.hu.bu  vr10,    vr10,    0
    vsllwil.hu.bu  vr11,    vr7,     0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0
    z1_filter_edge_calc_loop1
.endm

.macro pixel_set_8bpc_allw dst_ptr, src_ptr, width, tmp0, tmp1
    vldrepl.b      vr10,    \src_ptr, 0
    or             \tmp1,   zero,     zero
    srai.d         \tmp0,   \width,   4
    beqz           \tmp0,   2f
1:
    vstx           vr10,    \dst_ptr, \tmp1
    addi.d         \tmp1,   \tmp1,    16
    addi.d         \tmp0,   \tmp0,    -1
    bnez           \tmp0,   1b
2:
    andi           \tmp0,   \width,   8
    beqz           \tmp0,   3f
    fstx.d         f10,     \dst_ptr, \tmp1
    addi.d         \tmp1,   \tmp1,    8
3:
    andi           \tmp0,   \width,   4
    beqz           \tmp0,   4f
    fstx.s         f10,     \dst_ptr, \tmp1
    addi.d         \tmp1,   \tmp1,    4
4:
    andi           \tmp0,   \width,   2
    beqz           \tmp0,   5f
    ldx.bu         \tmp0,   \src_ptr, zero
    stx.b          \tmp0,   \dst_ptr, \tmp1
    addi.d         \tmp1,   \tmp1,    1
    stx.b          \tmp0,   \dst_ptr, \tmp1
    addi.d         \tmp1,   \tmp1,    1
5:
    andi           \tmp0,   \width,   1
    beqz           \tmp0,   6f
    ldx.bu         \tmp0,   \src_ptr, zero
    stx.b          \tmp0,   \dst_ptr, \tmp1
6:
.endm

// void ipred_z1_lsx(pixel *dst, const ptrdiff_t stride,
//                   const pixel *const topleft_in,
//                   const int width, const int height, int angle,
//                   const int max_width, const int max_height
//                   HIGHBD_DECL_SUFFIX)
function ipred_z1_8bpc_lsx
    addi.d         a2,      a2,      1   //&topleft_in[1]
    addi.d         sp,      sp,      -128
    or             t2,      sp,      sp  //top_out
    srai.d         a6,      a5,      9
    andi           a6,      a6,      1   //is_sum
    srai.d         a7,      a5,      10  //enable_intra_edge_filter
    andi           a5,      a5,      511

    la.local       t0,      dav1d_dr_intra_derivative
    andi           t1,      a5,      0xFFE
    ldx.hu         t1,      t0,      t1  //dx

    beqz           a7,      .IPRED_Z1_NOTUA
    add.d          t3,      a3,      a4
    li.w           t4,      90
    sub.w          t4,      t4,      a5
    // ipred_get_upsample t5:upsample_above
    li.w           t6,      16
    sra.d          t6,      t6,      a6
    bge            t6,      t3,      .Z1_GETUS1
    addi.d         t5,      zero,    0
    b              .Z1_GETUS2
.Z1_GETUS1:
    addi.d         t5,      zero,    1
.Z1_GETUS2:
    li.w           t6,      40
    blt            t4,      t6,      .Z1_GETUS3
    addi.d         t6,      zero,    0
    b              .Z1_GETUS4
.Z1_GETUS3:
    addi.d         t6,      zero,    1
.Z1_GETUS4:
    and            t5,      t5,      t6

    beqz           t5,      .IPRED_Z1_NOTUA

    la.local       t0,      z1_upsample_edge_kernel
    vld            vr0,     t0,      0   //kernel
    vxor.v         vr15,    vr15,    vr15
    li.w           t0,      255
    vreplgr2vr.h   vr16,    t0

.Z1_UEDGE_W4:
    andi           t6,      a3,     4
    beqz           t6,      .Z1_UEDGE_W8
.Z1_UEDGE_W4_H4:
    andi           t6,      a4,     4
    beqz           t6,      .Z1_UEDGE_W4_H8

    //0-6
    vld            vr7,     a2,      -1
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop

    fst.d          f13,     t2,     0
    vstelm.w       vr13,    t2,     8,    2
    vstelm.h       vr13,    t2,     12,   6

    ld.bu          t7,      a2,     7
    st.b           t7,      t2,     14

    b              .Z1_UEDGE_END

.Z1_UEDGE_W4_H8:
    andi           t6,      a4,     8
    beqz           t6,      .Z1_UEDGE_W4_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     0

    //8-10
    vldrepl.b      vr7,     a2,     7
    z1_upsample_edge_calc_other

    vstelm.w       vr13,    t2,     16,   0
    vstelm.h       vr13,    t2,     20,   2

    ld.bu          t7,      a2,     7
    st.b           t7,      t2,     22

    b              .Z1_UEDGE_END

.Z1_UEDGE_W4_H16:
    andi           t6,      a4,     16
    beqz           t6,      .Z1_UEDGE_W4_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     0

    //8-15
    vldrepl.b      vr7,     a2,     7
    z1_upsample_edge_calc_other
    vst            vr13,    t2,     16

    //16-18
    vstelm.w       vr13,    t2,     32,   0
    vstelm.h       vr13,    t2,     36,   2

    ld.bu          t7,      a2,     7
    st.b           t7,      t2,     38

    b              .Z1_UEDGE_END

.Z1_UEDGE_W4_H32:
    andi           t6,      a4,     32
    beqz           t6,      .Z1_UEDGE_W4_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     0

    //8-15
    vldrepl.b      vr7,     a2,     7
    z1_upsample_edge_calc_other
    vst            vr13,    t2,     16

    vst            vr13,    t2,     32 //16-23
    vst            vr13,    t2,     48 //24-31

    //32-34
    vstelm.w       vr13,    t2,     64,   0
    vstelm.h       vr13,    t2,     68,   2

    ld.bu          t7,      a2,     7
    st.b           t7,      t2,     70

    b              .Z1_UEDGE_END

.Z1_UEDGE_W4_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     0

    //8-15
    vldrepl.b      vr7,     a2,     7
    z1_upsample_edge_calc_other
    vst            vr13,    t2,     16

    vst            vr13,    t2,     32 //16-23
    vst            vr13,    t2,     48 //24-31
    vst            vr13,    t2,     64 //32-39
    vst            vr13,    t2,     80 //40-47
    vst            vr13,    t2,     96 //48-55
    vst            vr13,    t2,     112 //56-63

    //64-66
    vstelm.w       vr13,    t2,     128,   0
    vstelm.h       vr13,    t2,     132,   2

    ld.bu          t7,      a2,     7
    st.b           t7,      t2,     134

    b              .Z1_UEDGE_END

.Z1_UEDGE_W8:
    andi           t6,      a3,     8
    beqz           t6,      .Z1_UEDGE_W16
.Z1_UEDGE_W8_H4:
    andi           t6,      a4,     4
    beqz           t6,      .Z1_UEDGE_W8_H8

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x32
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x21
    vextrins.b     vr13,    vr13,    0x31
    z1_upsample_edge_calc_loop
    vstelm.w       vr13,    t2,     16,    0
    vstelm.h       vr13,    t2,     20,    2

    ld.bu          t7,      a2,     11
    st.b           t7,      t2,     22
    b              .Z1_UEDGE_END

.Z1_UEDGE_W8_H8:
    andi           t6,      a4,     8
    beqz           t6,      .Z1_UEDGE_W8_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-14
    vld            vr7,     a2,      7
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
    fst.d          f13,     t2,     16
    vstelm.w       vr13,    t2,     24,    2
    vstelm.h       vr13,    t2,     28,    6

    ld.bu          t7,      a2,     15
    st.b           t7,      t2,     30
    b              .Z1_UEDGE_END

.Z1_UEDGE_W8_H16:
    andi           t6,      a4,     16
    beqz           t6,      .Z1_UEDGE_W8_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     16

    //16-22
    vldrepl.b      vr7,     a2,     15
    z1_upsample_edge_calc_other
    fst.d          f13,     t2,     32
    vstelm.w       vr13,    t2,     40,   2
    vstelm.h       vr13,    t2,     44,   6

    ld.bu          t7,      a2,     15
    st.b           t7,      t2,     46
    b              .Z1_UEDGE_END

.Z1_UEDGE_W8_H32:
    andi           t6,      a4,     32
    beqz           t6,      .Z1_UEDGE_W8_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     16

    //16-23
    vldrepl.b      vr7,     a2,     15
    z1_upsample_edge_calc_other
    vst            vr13,    t2,     32

    vst            vr13,    t2,     48 //24-31

    //32-38
    fst.d          f13,     t2,     64
    vstelm.w       vr13,    t2,     72,   2
    vstelm.h       vr13,    t2,     76,   6

    ld.bu          t7,      a2,     15
    st.b           t7,      t2,     78
    b              .Z1_UEDGE_END

.Z1_UEDGE_W8_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init2
    vst            vr13,    t2,     16

    //16-23
    vldrepl.b      vr7,     a2,     15
    z1_upsample_edge_calc_other
    vst            vr13,    t2,     32

    vst            vr13,    t2,     48 //24-31
    vst            vr13,    t2,     64 //32-39
    vst            vr13,    t2,     80 //40-47
    vst            vr13,    t2,     96 //48-55
    vst            vr13,    t2,     112 //56-63

    //64-70
    fst.d          f13,     t2,     128
    vstelm.w       vr13,    t2,     136,   2
    vstelm.h       vr13,    t2,     140,   6

    ld.bu          t7,      a2,     15
    st.b           t7,      t2,     142
    b              .Z1_UEDGE_END

.Z1_UEDGE_W16:
    andi           t6,      a3,     16
    beqz           t6,      .Z1_UEDGE_W32
.Z1_UEDGE_W16_H4:
    andi           t6,      a4,     4
    beqz           t6,      .Z1_UEDGE_W16_H8

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     16

    //16-18
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vstelm.w       vr13,    t2,     32,    0
    vstelm.h       vr13,    t2,     36,    2

    ld.bu          t7,      a2,     19
    st.b           t7,      t2,     38
    b              .Z1_UEDGE_END

.Z1_UEDGE_W16_H8:
    andi           t6,      a4,     8
    beqz           t6,      .Z1_UEDGE_W16_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-22
    vld            vr7,     a2,      15
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
    fst.d          f13,     t2,     32
    vstelm.w       vr13,    t2,     40,    2
    vstelm.h       vr13,    t2,     44,    6

    ld.bu          t7,      a2,     23
    st.b           t7,      t2,     46
    b              .Z1_UEDGE_END

.Z1_UEDGE_W16_H16:
    andi           t6,      a4,     16
    beqz           t6,      .Z1_UEDGE_W16_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-30
    vld            vr7,     a2,      23
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
    fst.d          f13,     t2,     48
    vstelm.w       vr13,    t2,     56,    2
    vstelm.h       vr13,    t2,     60,    6

    ld.bu          t7,      a2,     31
    st.b           t7,      t2,     62
    b              .Z1_UEDGE_END

.Z1_UEDGE_W16_H32:
    andi           t6,      a4,     32
    beqz           t6,      .Z1_UEDGE_W16_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init2
    vst            vr13,    t2,      48

    //32-39
    vldrepl.b      vr7,     a2,      31
    z1_upsample_edge_calc_other
    vst            vr13,    t2,      64

    //40-46
    fst.d          f13,     t2,     80
    vstelm.w       vr13,    t2,     88,    2
    vstelm.h       vr13,    t2,     92,    6

    ld.bu          t7,      a2,     31
    st.b           t7,      t2,     94
    b              .Z1_UEDGE_END

.Z1_UEDGE_W16_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init2
    vst            vr13,    t2,      48

    //32-39
    vldrepl.b      vr7,     a2,      31
    z1_upsample_edge_calc_other
    vst            vr13,    t2,      64

    vst            vr13,    t2,      80  //40-47
    vst            vr13,    t2,      96  //48-55
    vst            vr13,    t2,      112 //56-63
    vst            vr13,    t2,      128 //64-71

    //72-78
    fst.d          f13,     t2,     144
    vstelm.w       vr13,    t2,     152,    2
    vstelm.h       vr13,    t2,     156,    6

    ld.bu          t7,      a2,     31
    st.b           t7,      t2,     158
    b              .Z1_UEDGE_END

.Z1_UEDGE_W32:
    andi           t6,      a3,     32
    beqz           t6,      .Z1_UEDGE_W64
.Z1_UEDGE_W32_H8:
    andi           t6,      a4,     8
    beqz           t6,      .Z1_UEDGE_W32_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-38
    vld            vr7,     a2,      31
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
    fst.d          f13,     t2,      64
    vstelm.w       vr13,    t2,      72,    2
    vstelm.h       vr13,    t2,      76,    6

    ld.bu          t7,      a2,     39
    st.b           t7,      t2,     78
    b              .Z1_UEDGE_END

.Z1_UEDGE_W32_H16:
    andi           t6,      a4,     16
    beqz           t6,      .Z1_UEDGE_W32_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-39
    vld            vr7,     a2,      31
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      64

    //40-46
    vld            vr7,     a2,      39
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
    fst.d          f13,     t2,      80
    vstelm.w       vr13,    t2,      88,    2
    vstelm.h       vr13,    t2,      92,    6

    ld.bu          t7,      a2,     47
    st.b           t7,      t2,     94
    b              .Z1_UEDGE_END

.Z1_UEDGE_W32_H32:
    andi           t6,      a4,     32
    beqz           t6,      .Z1_UEDGE_W32_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-39
    vld            vr7,     a2,      31
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      64

    //40-47
    vld            vr7,     a2,      39
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      80

    //48-55
    vld            vr7,     a2,      47
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      96

    //56-62
    vld            vr7,     a2,      55
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vextrins.b     vr12,    vr12,    0x76
    vbsrl.v        vr13,    vr7,     3
    z1_upsample_edge_calc_loop
    fst.d          f13,     t2,      112
    vstelm.w       vr13,    t2,      120,   2
    vstelm.h       vr13,    t2,      124,   6

    ld.bu          t7,      a2,     63
    st.b           t7,      t2,     126
    b              .Z1_UEDGE_END

.Z1_UEDGE_W32_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-39
    vld            vr7,     a2,      31
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      64

    //40-47
    vld            vr7,     a2,      39
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      80

    //48-55
    vld            vr7,     a2,      47
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      96

    //56-63
    vld            vr7,     a2,      55
    z1_upsample_edge_data_init2
    vst            vr13,    t2,      112

    //64-71
    vldrepl.b      vr7,     a2,      63
    z1_upsample_edge_calc_other
    vst            vr13,    t2,      128

    vst            vr13,    t2,      144 //72-79
    vst            vr13,    t2,      160 //80-87

    //88-94
    fst.d          f13,     t2,     176
    vstelm.w       vr13,    t2,     184,    2
    vstelm.h       vr13,    t2,     188,    6

    ld.bu          t7,      a2,     63
    st.b           t7,      t2,     190
    b              .Z1_UEDGE_END

.Z1_UEDGE_W64:
.Z1_UEDGE_W64_H16:
    andi           t6,      a4,     16
    beqz           t6,      .Z1_UEDGE_W64_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-39
    vld            vr7,     a2,      31
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      64

    //40-47
    vld            vr7,     a2,      39
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      80

    //48-55
    vld            vr7,     a2,      47
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      96

    //56-63
    vld            vr7,     a2,      55
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      112

    //64-71
    vld            vr7,     a2,      63
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      128

    //72-78
    vld            vr7,     a2,      71
    z1_upsample_edge_data_init2
    fst.d          f13,     t2,     144
    vstelm.w       vr13,    t2,     152,    2
    vstelm.h       vr13,    t2,     156,    6

    ld.bu          t7,      a2,     79
    st.b           t7,      t2,     158
    b              .Z1_UEDGE_END

.Z1_UEDGE_W64_H32:
    andi           t6,      a4,     32
    beqz           t6,      .Z1_UEDGE_W64_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-39
    vld            vr7,     a2,      31
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      64

    //40-47
    vld            vr7,     a2,      39
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      80

    //48-55
    vld            vr7,     a2,      47
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      96

    //56-63
    vld            vr7,     a2,      55
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      112

    //64-71
    vld            vr7,     a2,      63
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      128

    //72-79
    vld            vr7,     a2,      71
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      144

    //80-87
    vld            vr7,     a2,      79
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      160

    //88-94
    vld            vr7,     a2,      87
    z1_upsample_edge_data_init2
    fst.d          f13,     t2,     176
    vstelm.w       vr13,    t2,     184,    2
    vstelm.h       vr13,    t2,     188,    6

    ld.bu          t7,      a2,     95
    st.b           t7,      t2,     190
    b              .Z1_UEDGE_END

.Z1_UEDGE_W64_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_upsample_edge_data_init1
    vst            vr13,    t2,     0

    //8-15
    vld            vr7,     a2,      7
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      16

    //16-23
    vld            vr7,     a2,      15
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      32

    //24-31
    vld            vr7,     a2,      23
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      48

    //32-39
    vld            vr7,     a2,      31
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      64

    //40-47
    vld            vr7,     a2,      39
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      80

    //48-55
    vld            vr7,     a2,      47
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      96

    //56-63
    vld            vr7,     a2,      55
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      112

    //64-71
    vld            vr7,     a2,      63
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      128

    //72-79
    vld            vr7,     a2,      71
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      144

    //80-87
    vld            vr7,     a2,      79
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      160

    //88-95
    vld            vr7,     a2,      87
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      176

    //96-103
    vld            vr7,     a2,      95
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      192

    //104-111
    vld            vr7,     a2,      103
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      208

    //112-119
    vld            vr7,     a2,      111
    z1_upsample_edge_data_init1
    vst            vr13,    t2,      224

    //120-126
    vld            vr7,     a2,      119
    z1_upsample_edge_data_init2
    fst.d          f13,     t2,      240
    vstelm.w       vr13,    t2,      248,    2
    vstelm.h       vr13,    t2,      252,    6

    ld.bu          t7,      a2,      127
    st.b           t7,      t2,      254
    b              .Z1_UEDGE_END

.Z1_UEDGE_END:
    //upsample_edge end

    or             a7,      t2,      t2   //top
    add.d          t0,      a3,      a4
    slli.d         t0,      t0,      1
    addi.d         t0,      t0,      -2   //max_base_x
    slli.d         t1,      t1,      1
    b              .IPRED_Z1_UA_END

.IPRED_Z1_NOTUA:
    or             t5,      zero,    zero  //upsample_above=0
    beqz           a7,      .IPRED_Z1_NOTFS
    add.d          a7,      a3,      a4  //w+h
    li.w           t4,      90
    sub.d          t4,      t4,      a5
    // ipred_get_filter_strength a6:filter_strength
    beqz           a6,      .Z1_GETFS20
.Z1_GETFS10:  //wh<=8
    addi.d         t6,      a7,      -8
    blt            zero,    t6,      .Z1_GETFS11
    addi.d         t6,      t4,      -64
    blt            t6,      zero,    .Z1_GETFS101
    ori            a6,      zero,    2
    b              .Z1_GETFS40
.Z1_GETFS101:
    addi.d         t6,      t4,      -40
    blt            t6,      zero,    .Z1_GETFS30
    ori            a6,      zero,    1
    b              .Z1_GETFS40
.Z1_GETFS11:  //wh<=16
    addi.d         t6,      a7,      -16
    blt            zero,    t6,      .Z1_GETFS12
    addi.d         t6,      t4,      -48
    blt            t6,      zero,    .Z1_GETFS111
    ori            a6,      zero,    2
    b              .Z1_GETFS40
.Z1_GETFS111:
    addi.d         t6,      t4,      -20
    blt            t6,      zero,    .Z1_GETFS30
    ori            a6,      zero,    1
    b              .Z1_GETFS40
.Z1_GETFS12:  //wh<=24
    addi.d         t6,      a7,      -24
    blt            zero,    t6,      .Z1_GETFS13
    addi.d         t6,      t4,      -4
    blt            t6,      zero,    .Z1_GETFS30
    ori            a6,      zero,    3
    b              .Z1_GETFS40
.Z1_GETFS13:
    ori            a6,      zero,    3
    b              .Z1_GETFS40

.Z1_GETFS20:  //wh<=8
    addi.d         t6,      a7,      -8
    blt            zero,    t6,      .Z1_GETFS21
    addi.d         t6,      t4,      -56
    blt            t6,      zero,    .Z1_GETFS30
    ori            a6,      zero,    1
    b              .Z1_GETFS40
.Z1_GETFS21:  //wh<=16
    addi.d         t6,      a7,      -16
    blt            zero,    t6,      .Z1_GETFS22
    addi.d         t6,      t4,      -40
    blt            t6,      zero,    .Z1_GETFS30
    ori            a6,      zero,    1
    b              .Z1_GETFS40
.Z1_GETFS22:  //wh<=24
    addi.d         t6,      a7,      -24
    blt            zero,    t6,      .Z1_GETFS23
    addi.d         t6,      t4,      -32
    blt            t6,      zero,    .Z1_GETFS221
    ori            a6,      zero,    3
    b              .Z1_GETFS40
.Z1_GETFS221:
    addi.d         t6,      t4,      -16
    blt            t6,      zero,    .Z1_GETFS222
    ori            a6,      zero,    2
    b              .Z1_GETFS40
.Z1_GETFS222:
    addi.d         t6,      t4,      -8
    blt            t6,      zero,    .Z1_GETFS30
    ori            a6,      zero,    1
    b              .Z1_GETFS40
.Z1_GETFS23:  //wh<=32
    addi.d         t6,      a7,      -32
    blt            zero,    t6,      .Z1_GETFS24
    addi.d         t6,      t4,      -32
    blt            t6,      zero,    .Z1_GETFS231
    ori            a6,      zero,    3
    b              .Z1_GETFS40
.Z1_GETFS231:
    addi.d         t6,      t4,      -4
    blt            t6,      zero,    .Z1_GETFS232
    ori            a6,      zero,    2
    b              .Z1_GETFS40
.Z1_GETFS232:
    ori            a6,      zero,    1
    b              .Z1_GETFS40
.Z1_GETFS24:
    ori            a6,      zero,    3
    b              .Z1_GETFS40
.Z1_GETFS30:
   or              a6,      zero,    zero
.Z1_GETFS40:

    beqz           a6,      .IPRED_Z1_NOTFS

.IPRED_Z1_IFFS:
    // filter_edge
    addi.d         a6,      a6,      -1
    slli.d         a6,      a6,      4
    la.local       t0,      ipred_filter_edge_kernel1
    vldx           vr1,     t0,      a6    //kernel[0-3]

    la.local       t0,      ipred_filter_edge_kernel2
    vldx           vr6,     t0,      a6    //kernel[4]

.IPRED_Z1_FS_W4:
    andi           t0,      a3,      4
    beqz           t0,      .IPRED_Z1_FS_W8
.IPRED_Z1_FS_W4_H4:
    andi           t0,      a4,      4
    beqz           t0,      .IPRED_Z1_FS_W4_H8

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init4
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0
    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W4_H8:
    andi           t0,      a4,      8
    beqz           t0,      .IPRED_Z1_FS_W4_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init4
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-11
    vreplvei.b     vr10,    vr7,     8
    vextrins.b     vr10,    vr7,     0x07
    z1_filter_edge_calc_other
    fst.s          f12,     t2,      8

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W4_H16:
    andi           t0,      a4,      16
    beqz           t0,      .IPRED_Z1_FS_W4_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init4
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vreplvei.b     vr10,    vr7,     8
    vextrins.b     vr10,    vr7,     0x07
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      8

    //16-19
    vreplvei.b     vr12,    vr12,    1
    fst.s          f12,     t2,      16

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W4_H32:
    andi           t0,      a4,      32
    beqz           t0,      .IPRED_Z1_FS_W4_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init4
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vreplvei.b     vr10,    vr7,     8
    vextrins.b     vr10,    vr7,     0x07
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      8

    //16-23
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      16

    fst.d          f12,     t2,      24 //24-31
    fst.s          f12,     t2,      32 //32-35

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W4_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init4
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vreplvei.b     vr10,    vr7,     8
    vextrins.b     vr10,    vr7,     0x07
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      8

    //16-23
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      16

    fst.d          f12,     t2,      24 //24-31
    fst.d          f12,     t2,      32 //32-39
    fst.d          f12,     t2,      40 //40-47
    fst.d          f12,     t2,      48 //48-55
    fst.d          f12,     t2,      56 //56-63
    fst.s          f12,     t2,      64 //64-67

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W8:
    andi           t0,      a3,      8
    beqz           t0,      .IPRED_Z1_FS_W16
.IPRED_Z1_FS_W8_H4:
    andi           t0,      a4,      4
    beqz           t0,      .IPRED_Z1_FS_W8_H8

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-11
    vld            vr7,     a2,      6
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x32
    vsllwil.hu.bu  vr10,    vr7,     0
    vsllwil.hu.bu  vr11,    vr11,    0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0
    z1_filter_edge_calc_loop1

    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x21
    vextrins.b     vr13,    vr13,    0x31
    z1_filter_edge_calc_loop2
    fst.s          f12,     t2,      8
    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W8_H8:
    andi           t0,      a4,      8
    beqz           t0,      .IPRED_Z1_FS_W8_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8
    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W8_H16:
    andi           t0,      a4,      16
    beqz           t0,      .IPRED_Z1_FS_W8_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vreplvei.b     vr10,    vr7,     9
    vextrins.b     vr10,    vr7,     0x08
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      16

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W8_H32:
    andi           t0,      a4,      32
    beqz           t0,      .IPRED_Z1_FS_W8_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vreplvei.b     vr10,    vr7,     9
    vextrins.b     vr10,    vr7,     0x08
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      16

    //24-31
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      24

    //32-39
    fst.d          f12,     t2,      32

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W8_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vreplvei.b     vr10,    vr7,     9
    vextrins.b     vr10,    vr7,     0x08
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      16

    //24-31
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      24

    fst.d          f12,     t2,      32  //32-39
    fst.d          f12,     t2,      40  //40-47
    fst.d          f12,     t2,      48  //48-55
    fst.d          f12,     t2,      56  //56-63
    fst.d          f12,     t2,      64  //64-71

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W16:
    andi           t0,      a3,      16
    beqz           t0,      .IPRED_Z1_FS_W32
.IPRED_Z1_FS_W16_H4:
    andi           t0,      a4,      4
    beqz           t0,      .IPRED_Z1_FS_W16_H8

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-19
    vld            vr7,     a2,      14
    vbsrl.v        vr11,    vr7,     1
    vbsrl.v        vr12,    vr7,     2
    vbsrl.v        vr13,    vr7,     3
    vextrins.b     vr13,    vr13,    0x32
    vsllwil.hu.bu  vr10,    vr7,     0
    vsllwil.hu.bu  vr11,    vr11,    0
    vsllwil.hu.bu  vr12,    vr12,    0
    vsllwil.hu.bu  vr13,    vr13,    0
    z1_filter_edge_calc_loop1

    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x21
    vextrins.b     vr13,    vr13,    0x31
    z1_filter_edge_calc_loop2
    fst.s          f12,     t2,      16
    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W16_H8:
    andi           t0,      a4,      8
    beqz           t0,      .IPRED_Z1_FS_W16_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16
    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W16_H16:
    andi           t0,      a4,      16
    beqz           t0,      .IPRED_Z1_FS_W16_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24
    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W16_H32:
    andi           t0,      a4,      32
    beqz           t0,      .IPRED_Z1_FS_W16_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vreplvei.b     vr10,    vr7,     9
    vextrins.b     vr10,    vr7,     0x08
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      32

    //40-47
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      40

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W16_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vreplvei.b     vr10,    vr7,     9
    vextrins.b     vr10,    vr7,     0x08
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      32

    //40-47
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      40

    fst.d          f12,     t2,      48 //48-55
    fst.d          f12,     t2,      56 //56-63
    fst.d          f12,     t2,      64 //64-71
    fst.d          f12,     t2,      72 //72-81

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W32:
    andi           t0,      a3,      32
    beqz           t0,      .IPRED_Z1_FS_W64
.IPRED_Z1_FS_W32_H8:
    andi           t0,      a4,      8
    beqz           t0,      .IPRED_Z1_FS_W32_H16

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W32_H16:
    andi           t0,      a4,      16
    beqz           t0,      .IPRED_Z1_FS_W32_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2

    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    //40-47
    vld            vr7,     a2,      38
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      40

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W32_H32:
    andi           t0,      a4,      32
    beqz           t0,      .IPRED_Z1_FS_W32_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    //40-47
    vld            vr7,     a2,      38
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      40

    //48-55
    vld            vr7,     a2,      46
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      48

    //56-63
    vld            vr7,     a2,      54
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      56

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W32_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    //40-47
    vld            vr7,     a2,      38
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      40

    //48-55
    vld            vr7,     a2,      46
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      48

    //56-63
    vld            vr7,     a2,      54
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      56

    //64-71
    vreplvei.b     vr10,    vr7,     9
    vextrins.b     vr10,    vr7,     0x08
    z1_filter_edge_calc_other
    fst.d          f12,     t2,      64

    //72-89
    vreplvei.b     vr12,    vr12,    1
    fst.d          f12,     t2,      72

    fst.d          f12,     t2,      80 //80-87
    fst.d          f12,     t2,      88 //88-95

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W64:
.IPRED_Z1_FS_W64_H16:
    andi           t0,      a4,      16
    beqz           t0,      .IPRED_Z1_FS_W64_H32

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    //40-47
    vld            vr7,     a2,      38
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      40

    //48-55
    vld            vr7,     a2,      46
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      48

    //56-63
    vld            vr7,     a2,      54
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      56

    //64-71
    vld            vr7,     a2,      62
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      64

    //72-79
    vld            vr7,     a2,      70
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      72

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W64_H32:
    andi           t0,      a4,      32
    beqz           t0,      .IPRED_Z1_FS_W64_H64

    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    //40-47
    vld            vr7,     a2,      38
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      40

    //48-55
    vld            vr7,     a2,      46
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      48

    //56-63
    vld            vr7,     a2,      54
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      56

    //64-71
    vld            vr7,     a2,      62
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      64

    //72-79
    vld            vr7,     a2,      70
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      72

    //80-87
    vld            vr7,     a2,      78
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      80

    //88-95
    vld            vr7,     a2,      86
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      88

    b              .IPRED_Z1_FS_END

.IPRED_Z1_FS_W64_H64:
    //0-7
    vld            vr7,     a2,      -1
    z1_filter_edge_data_init1
    vbsrl.v        vr13,    vr7,     3
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      0

    //8-15
    vld            vr7,     a2,      6
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      8

    //16-23
    vld            vr7,     a2,      14
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      16

    //24-31
    vld            vr7,     a2,      22
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      24

    //32-39
    vld            vr7,     a2,      30
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      32

    //40-47
    vld            vr7,     a2,      38
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      40

    //48-55
    vld            vr7,     a2,      46
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      48

    //56-63
    vld            vr7,     a2,      54
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      56

    //64-71
    vld            vr7,     a2,      62
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      64

    //72-79
    vld            vr7,     a2,      70
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      72

    //80-87
    vld            vr7,     a2,      78
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      80

    //88-95
    vld            vr7,     a2,      86
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      88

    //96-103
    vld            vr7,     a2,      94
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      96

    //104-111
    vld            vr7,     a2,      102
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      104

    //112-119
    vld            vr7,     a2,      110
    z1_filter_edge_data_init2
    vbsrl.v        vr13,    vr7,     4
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      112

    //120-127
    vld            vr7,     a2,      118
    z1_filter_edge_data_init3
    vbsrl.v        vr13,    vr7,     4
    vextrins.b     vr13,    vr13,    0x65
    vextrins.b     vr13,    vr13,    0x75
    z1_filter_edge_calc_loop2
    fst.d          f12,     t2,      120

.IPRED_Z1_FS_END:
    addi.d         t0,      a7,      -1   //max_base_x
    or             a7,      t2,      t2   //top
    b              .IPRED_Z1_UA_END

.IPRED_Z1_NOTFS:
    or             a7,      a2,      a2   //top
    // imin_gr
    blt            a3,      a4,      .Z1_IMIN1
    or             t0,      a4,      a4
    b              .Z1_IMIN2
.Z1_IMIN1:
    or             t0,      a3,      a3
.Z1_IMIN2:

    add.d          t0,      a3,      t0
    addi.d         t0,      t0,      -1   //max_base_x

.IPRED_Z1_UA_END:
    //st dst, t1:dx  a2 a6 t6 t7
    beqz           t5,      .Z1_UA0

    li.w           a5,      64
    vreplgr2vr.h   vr0,     a5
    vsrai.h        vr7,     vr0,     1
    or             t2,      zero,    zero  //y
    or             t3,      t1,      t1    //xpos
.Z1_LOOPY:
    andi           t4,      t3,      0x3e  //frac
    vreplgr2vr.h   vr1,     t4
    vsub.h         vr2,     vr0,     vr1
    or             a6,      zero,    zero  //x
    or             a2,      zero,    zero  //base_num
    srai.d         t6,      t3,      6     //base

    or             t7,      t6,      t6
    bge            t7,      t0,      .Z1_LOOPX
.Z1_BASENUM:
    addi.d         a2,      a2,      1
    addi.d         t7,      t7,      2
    blt            t7,      t0,      .Z1_BASENUM

.Z1_LOOPX:
    blt            a2,      a3,      .Z1_LOOPX_BASEMAX

    srai.d         t8,      a3,      3  //loop param
    beqz           t8,      .Z1_LOOPX_W4
.Z1_LOOPX_W8:
    add.d          t5,      a7,      t6
    vld            vr3,     t5,      0
    vpickev.b      vr5,     vr3,     vr3  //0 2 4 6...
    vpickod.b      vr6,     vr3,     vr3  //1 3 5 7...
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.hu.bu  vr6,     vr6,     0

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.d         f3,      a0,      a6

    addi.d         a6,      a6,      8
    addi.d         t6,      t6,      16
    addi.d         t8,      t8,      -1
    bnez           t8,      .Z1_LOOPX_W8
    b              .Z1_LOOPY_END
.Z1_LOOPX_W4:
    vldx           vr3,     a7,      t6
    vsllwil.hu.bu  vr3,     vr3,     0
    vpickev.h      vr5,     vr3,     vr3  //0 2 4 6...
    vpickod.h      vr6,     vr3,     vr3  //1 3 5 7...

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.s         f3,      a0,      a6
    b              .Z1_LOOPY_END
.Z1_LOOPX_BASEMAX:
    srai.d         t8,      a2,      3  //loop param
    beqz           t8,      .Z1_LOOPX_BASEMAX4
.Z1_LOOPX_BASEMAX8:
    add.d          t5,      a7,      t6
    vld            vr3,     t5,      0
    vpickev.b      vr5,     vr3,     vr3  //0 2 4 6...
    vpickod.b      vr6,     vr3,     vr3  //1 3 5 7...
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.hu.bu  vr6,     vr6,     0

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.d         f3,      a0,      a6

    addi.d         a6,      a6,      8
    addi.d         t6,      t6,      16
    addi.d         t8,      t8,      -1
    bnez           t8,      .Z1_LOOPX_BASEMAX8
.Z1_LOOPX_BASEMAX4:
    andi           t8,      a2,      4
    beqz           t8,      .Z1_LOOPX_BASEMAX2

    vldx           vr3,     a7,      t6
    vsllwil.hu.bu  vr3,     vr3,     0
    vpickev.h      vr5,     vr3,     vr3  //0 2 4 6...
    vpickod.h      vr6,     vr3,     vr3  //1 3 5 7...

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.s         f3,      a0,      a6

    addi.d         a6,      a6,      4
    addi.d         t6,      t6,      8
.Z1_LOOPX_BASEMAX2:
    andi           t8,      a2,     2
    beqz           t8,      .Z1_LOOPX_BASEMAX1

    vldx           vr3,     a7,      t6
    vsllwil.hu.bu  vr3,     vr3,     0
    vpickev.h      vr5,     vr3,     vr3  //0 2 4 6...
    vpickod.h      vr6,     vr3,     vr3  //1 3 5 7...

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    vpickve2gr.bu  t7,      vr3,     0
    vpickve2gr.bu  t8,      vr3,     1
    stx.b          t7,      a0,      a6
    addi.d         a6,      a6,      1
    stx.b          t8,      a0,      a6
    addi.d         a6,      a6,      1
    addi.d         t6,      t6,      4
.Z1_LOOPX_BASEMAX1:
    andi           t8,      a2,     1
    beqz           t8,      .Z1_LOOPX_BASEMAX_MSET

    add.d          a2,      a7,      t6
    sub.d          t7,      a5,      t4
    ld.bu          t8,      a2,      0
    mul.w          t7,      t7,      t8
    ld.bu          t8,      a2,      1
    mul.w          t8,      t8,      t4
    add.d          t7,      t7,      t8
    addi.d         t7,      t7,      32
    srai.d         t7,      t7,      6
    stx.b          t7,      a0,      a6

    addi.d         a6,      a6,      1
.Z1_LOOPX_BASEMAX_MSET:  //memset
    add.d          t6,      a0,      a6  //dst
    add.d          t7,      a7,      t0  //src
    sub.d          a2,      a3,      a6  //size
    pixel_set_8bpc_allw t6, t7, a2, t8, t4
.Z1_LOOPY_END:
    addi.d         t2,      t2,      1
    add.d          a0,      a0,      a1
    add.d          t3,      t3,      t1
    blt            t2,      a4,      .Z1_LOOPY
    b              .Z1_END

.Z1_UA0:
    li.w           a5,      64
    vreplgr2vr.h   vr0,     a5
    vsrai.h        vr7,     vr0,     1
    or             t2,      zero,    zero  //y
    or             t3,      t1,      t1    //xpos
.Z1_UA0_LOOPY:
    andi           t4,      t3,      0x3e  //frac
    vreplgr2vr.h   vr1,     t4
    vsub.h         vr2,     vr0,     vr1
    or             a6,      zero,    zero  //x
    srai.d         t6,      t3,      6     //base

    sub.d          a2,      t0,      t6     //a2:base_num
    blt            a2,      zero,    .Z1_UA0_BASENUM
    b              .Z1_UA0_LOOPX
.Z1_UA0_BASENUM:
    or             a2,      zero,    zero

.Z1_UA0_LOOPX:
    blt            a2,      a3,      .Z1_UA0_LOOPX_BASEMAX

    srai.d         t8,      a3,      3  //loop param
    beqz           t8,      .Z1_UA0_LOOPX_W4
.Z1_UA0_LOOPX_W8:
    add.d          t5,      a7,      t6
    vld            vr5,     t5,      0
    vld            vr6,     t5,      1
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.hu.bu  vr6,     vr6,     0

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.d         f3,      a0,      a6

    addi.d         a6,      a6,      8
    addi.d         t6,      t6,      8
    addi.d         t8,      t8,      -1
    bnez           t8,      .Z1_UA0_LOOPX_W8
    b              .Z1_UA0_LOOPY_END
.Z1_UA0_LOOPX_W4:
    vldx           vr5,     a7,      t6
    vsllwil.hu.bu  vr5,     vr5,     0
    vbsrl.v        vr6,     vr5,     2

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.s         f3,      a0,      a6
    b              .Z1_UA0_LOOPY_END
.Z1_UA0_LOOPX_BASEMAX:
    srai.d         t8,      a2,      3  //loop param
    beqz           t8,      .Z1_UA0_LOOPX_BASEMAX4
.Z1_UA0_LOOPX_BASEMAX8:
    add.d          t5,      a7,      t6
    vld            vr5,     t5,      0
    vld            vr6,     t5,      1
    vsllwil.hu.bu  vr5,     vr5,     0
    vsllwil.hu.bu  vr6,     vr6,     0

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.d         f3,      a0,      a6

    addi.d         a6,      a6,      8
    addi.d         t6,      t6,      8
    addi.d         t8,      t8,      -1
    bnez           t8,      .Z1_UA0_LOOPX_BASEMAX8
.Z1_UA0_LOOPX_BASEMAX4:
    andi           t8,      a2,      4
    beqz           t8,      .Z1_UA0_LOOPX_BASEMAX2

    vldx           vr5,     a7,      t6
    vsllwil.hu.bu  vr5,     vr5,     0
    vbsrl.v        vr6,     vr5,     2

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    fstx.s         f3,      a0,      a6

    addi.d         a6,      a6,      4
    addi.d         t6,      t6,      4
.Z1_UA0_LOOPX_BASEMAX2:
    andi           t8,      a2,     2
    beqz           t8,      .Z1_UA0_LOOPX_BASEMAX1

    vldx           vr5,     a7,      t6
    vsllwil.hu.bu  vr5,     vr5,     0
    vbsrl.v        vr6,     vr5,     2

    vmul.h         vr3,     vr5,     vr2
    vmadd.h        vr3,     vr6,     vr1
    vadd.h         vr3,     vr3,     vr7
    vsrai.h        vr3,     vr3,     6
    vsrlni.b.h     vr3,     vr3,     0
    vpickve2gr.bu  t7,      vr3,     0
    vpickve2gr.bu  t8,      vr3,     1
    stx.b          t7,      a0,      a6
    addi.d         a6,      a6,      1
    stx.b          t8,      a0,      a6
    addi.d         a6,      a6,      1
    addi.d         t6,      t6,      2
.Z1_UA0_LOOPX_BASEMAX1:
    andi           t8,      a2,     1
    beqz           t8,      .Z1_UA0_LOOPX_BASEMAX_MSET

    add.d          a2,      a7,      t6
    sub.d          t7,      a5,      t4
    ld.bu          t8,      a2,      0
    mul.w          t7,      t7,      t8
    ld.bu          t8,      a2,      1
    mul.w          t8,      t8,      t4
    add.d          t7,      t7,      t8
    addi.d         t7,      t7,      32
    srai.d         t7,      t7,      6
    stx.b          t7,      a0,      a6

    addi.d         a6,      a6,      1
.Z1_UA0_LOOPX_BASEMAX_MSET:  //memset
    add.d          t6,      a0,      a6  //dst
    add.d          t7,      a7,      t0  //src
    sub.d          a2,      a3,      a6  //size
    pixel_set_8bpc_allw t6, t7, a2, t8, t4
.Z1_UA0_LOOPY_END:
    addi.d         t2,      t2,      1
    add.d          a0,      a0,      a1
    add.d          t3,      t3,      t1
    blt            t2,      a4,      .Z1_UA0_LOOPY

.Z1_END:
    addi.d         sp,      sp,      128
endfunc

