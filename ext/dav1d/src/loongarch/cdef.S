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

// static int cdef_find_dir_lsx(const pixel *img, const ptrdiff_t stride,
//                            unsigned *const var HIGHBD_DECL_SUFFIX)
// param: img: a0, stride: a1, var: a2
function cdef_find_dir_8bpc_lsx
    addi.d         sp,    sp,    -64
    fst.d          f24,   sp,    0
    fst.d          f25,   sp,    8
    fst.d          f26,   sp,    16
    fst.d          f27,   sp,    24
    fst.d          f28,   sp,    32
    fst.d          f29,   sp,    40
    fst.d          f30,   sp,    48
    fst.d          f31,   sp,    56

    li.d           a3,    128
    vreplgr2vr.w   vr31,  a3

    // hv: vr0-vr3  diag: vr4-vr11  alt: vr12-vr23
.irp i, vr0, vr1, vr2, vr3, vr4, vr5, vr6, vr7, vr8, vr9, vr10, \
        vr11, vr12, vr13, vr14, vr15, vr16, vr17, vr18, vr19, \
        vr20, vr21, vr22, vr23
    vxor.v      \i,       \i,       \i
.endr

.CFDL01:  // 8
    // 0
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vadd.w         vr4,   vr4,   vr24  //diag[0][y+x]
    vadd.w         vr5,   vr5,   vr25

    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr12,  vr12,  vr26
    vadd.w         vr12,  vr12,  vr27  //alt[0][y+(x>>1)]

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr0,   a3,    0    //hv[0][y]

    vadd.w         vr15,  vr15,  vr26
    vadd.w         vr15,  vr15,  vr27  //alt[1][3+y-(x>>1)]
    vpermi.w       vr15,  vr15,  0x1b

    vadd.w         vr9,   vr9,   vr24
    vadd.w         vr8,   vr8,   vr25
    vpermi.w       vr8,   vr8,   0x1b
    vpermi.w       vr9,   vr9,   0x1b  //diag[1][7+y-x]

    vxor.v         vr28,  vr28,  vr28
    vxor.v         vr29,  vr29,  vr29
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25
    vextrins.w     vr18,  vr28,  0x30
    vshuf4i.w      vr19,  vr28,  0x39
    vextrins.w     vr19,  vr29,  0x30
    vshuf4i.w      vr20,  vr29,  0x39  //alt[2][3-(y>>1)+7]
    vinsgr2vr.w    vr20,  zero,  3

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vadd.w         vr21,  vr21,  vr24
    vadd.w         vr22,  vr22,  vr25  //alt[3][(y>>1)+x]

    add.d          a0,    a0,    a1

    // 1
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vbsrl.v        vr28,  vr4,   4  //1-4
    vbsrl.v        vr29,  vr5,   4  //5-8
    vextrins.w     vr28,  vr5,   0x30
    vadd.w         vr28,  vr28,  vr24  //diag[0][y+x]
    vadd.w         vr29,  vr29,  vr25
    vbsll.v        vr5,   vr29,  4
    vextrins.w     vr5,   vr28,  0x03
    vextrins.w     vr6,   vr29,  0x03
    vextrins.w     vr28,  vr4,   0x30
    vshuf4i.w      vr4,   vr28,  0x93

    vbsrl.v        vr28,  vr12,  4
    vextrins.w     vr28,  vr13,  0x30
    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[0][y+(x>>1)]
    vextrins.w     vr13,  vr28,  0x03
    vextrins.w     vr28,  vr12,  0x30
    vshuf4i.w      vr12,  vr28,  0x93

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr0,   a3,    1    //hv[0][y]

    vbsrl.v        vr28,  vr15,  4
    vextrins.w     vr28,  vr16,  0x30
    vpermi.w       vr28,  vr28,  0x1b
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[1][3+y-(x>>1)]
    vextrins.w     vr16,  vr28,  0x00
    vextrins.w     vr28,  vr15,  0x00
    vshuf4i.w      vr15,  vr28,  0x6c

    vbsrl.v        vr28,  vr8,   4     //4321
    vbsrl.v        vr29,  vr9,   4     //8765
    vextrins.w     vr28,  vr9,   0x30
    vpermi.w       vr28,  vr28,  0x1b
    vpermi.w       vr29,  vr29,  0x1b
    vadd.w         vr29,  vr29,  vr24
    vadd.w         vr28,  vr28,  vr25  //diag[1][7+y-x]
    vextrins.w     vr10,  vr29,  0x00
    vextrins.w     vr29,  vr28,  0x00
    vshuf4i.w      vr9,   vr29,  0x6c
    vextrins.w     vr28,  vr8,   0x00
    vshuf4i.w      vr8,   vr28,  0x6c

    vbsll.v        vr28,  vr19,  4
    vextrins.w     vr28,  vr18,  0x03
    vbsll.v        vr29,  vr20,  4
    vextrins.w     vr29,  vr19,  0x03
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[2][3-(y>>1)+7]
    vextrins.w     vr18,  vr28,  0x30
    vextrins.w     vr28,  vr29,  0x00
    vshuf4i.w      vr19,  vr28,  0x39
    vbsrl.v        vr20,  vr29,  4

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vadd.w         vr21,  vr21,  vr24
    vadd.w         vr22,  vr22,  vr25  //alt[3][(y>>1)+x]

    add.d          a0,    a0,    a1

    // 2
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vbsrl.v        vr28,  vr4,   8
    vbsrl.v        vr29,  vr5,   8
    vextrins.d     vr28,  vr5,   0x10  //2-5
    vextrins.d     vr29,  vr6,   0x10  //6-9
    vadd.w         vr28,  vr28,  vr24  //diag[0][y+x]
    vadd.w         vr29,  vr29,  vr25
    vextrins.d     vr4,   vr28,  0x10
    vextrins.d     vr5,   vr28,  0x01
    vextrins.d     vr5,   vr29,  0x10
    vextrins.d     vr6,   vr29,  0x01

    vbsrl.v        vr28,  vr12,  8
    vextrins.d     vr28,  vr13,  0x10
    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[0][y+(x>>1)]
    vextrins.d     vr12,  vr28,  0x10
    vextrins.d     vr13,  vr28,  0x01

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr0,   a3,    2    //hv[0][y]

    vbsrl.v        vr28,  vr15,  8
    vextrins.d     vr28,  vr16,  0x10
    vpermi.w       vr28,  vr28,  0x1b
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[1][3+y-(x>>1)]
    vpermi.w       vr28,  vr28,  0x1b
    vextrins.d     vr15,  vr28,  0x10
    vextrins.d     vr16,  vr28,  0x01

    vbsrl.v        vr28,  vr8,   8
    vextrins.d     vr28,  vr9,   0x10
    vbsrl.v        vr29,  vr9,   8
    vextrins.d     vr29,  vr10,  0x10
    vpermi.w       vr28,  vr28,  0x1b  //5432
    vpermi.w       vr29,  vr29,  0x1b  //9876
    vadd.w         vr29,  vr29,  vr24
    vadd.w         vr28,  vr28,  vr25
    vpermi.w       vr28,  vr28,  0x1b
    vpermi.w       vr29,  vr29,  0x1b
    vextrins.d     vr8,   vr28,  0x10
    vextrins.d     vr9,   vr28,  0x01
    vextrins.d     vr9,   vr29,  0x10
    vextrins.d     vr10,  vr29,  0x01  //diag[1][7+y-x]

    vbsrl.v        vr28,  vr18,  8
    vextrins.d     vr28,  vr19,  0x10 //2345
    vbsrl.v        vr29,  vr19,  8
    vextrins.d     vr29,  vr20,  0x10 //6789
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25
    vextrins.d     vr18,  vr28,  0x10
    vextrins.d     vr19,  vr28,  0x01
    vextrins.d     vr19,  vr29,  0x10
    vextrins.d     vr20,  vr29,  0x01   //alt[2][3-(y>>1)+7]

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vbsrl.v        vr28,  vr21,  4
    vextrins.w     vr28,  vr22,  0x30  //1234
    vbsrl.v        vr29,  vr22,  4     //5678
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[3][(y>>1)+x]
    vextrins.w     vr23,  vr29,  0x03
    vextrins.w     vr29,  vr28,  0x33
    vshuf4i.w      vr22,  vr29,  0x93
    vextrins.w     vr28,  vr21,  0x30
    vshuf4i.w      vr21,  vr28,  0x93

    add.d          a0,    a0,    a1

    // 3
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vbsll.v        vr28,  vr5,   4
    vextrins.w     vr28,  vr4,   0x03 //3456
    vbsll.v        vr29,  vr6,   4
    vextrins.w     vr29,  vr5,   0x03 //78910
    vadd.w         vr28,  vr28,  vr24  //diag[0][y+x]
    vadd.w         vr29,  vr29,  vr25
    vextrins.w     vr4,   vr28,  0x30
    vextrins.w     vr28,  vr29,  0x00
    vshuf4i.w      vr5,   vr28,  0x39
    vbsrl.v        vr6,   vr29,  4

    vbsll.v        vr28,  vr13,  4
    vextrins.w     vr28,  vr12,  0x03
    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[0][y+(x>>1)]
    vextrins.w     vr12,  vr28,  0x30
    vbsrl.v        vr13,  vr28,  4

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr0,   a3,    3    //hv[0][y]

    vbsll.v        vr28,  vr16,  4
    vextrins.w     vr28,  vr15,  0x03
    vpermi.w       vr28,  vr28,  0x1b  //6543
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[1][3+y-(x>>1)]
    vextrins.w     vr15,  vr28,  0x33
    vshuf4i.w      vr16,  vr28,  0xc6
    vinsgr2vr.w    vr16,  zero,  3

    vbsll.v        vr28,  vr9,   4
    vextrins.w     vr28,  vr8,   0x03  //3456
    vbsll.v        vr29,  vr10,  4
    vextrins.w     vr29,  vr9,   0x03  //78910
    vpermi.w       vr28,  vr28,  0x1b  //6543
    vpermi.w       vr29,  vr29,  0x1b  //10987
    vadd.w         vr29,  vr29,  vr24
    vadd.w         vr28,  vr28,  vr25  //diag[1][7+y-x]
    vextrins.w     vr8,   vr28,  0x33
    vextrins.w     vr28,  vr29,  0x33
    vshuf4i.w      vr9,   vr28,  0xc6
    vshuf4i.w      vr10,  vr29,  0xc6
    vinsgr2vr.w    vr10,  zero,  3

    vbsrl.v        vr28,  vr18,  8
    vextrins.d     vr28,  vr19,  0x10 //2345
    vbsrl.v        vr29,  vr19,  8
    vextrins.d     vr29,  vr20,  0x10 //6789
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25
    vextrins.d     vr18,  vr28,  0x10
    vextrins.d     vr19,  vr28,  0x01
    vextrins.d     vr19,  vr29,  0x10
    vextrins.d     vr20,  vr29,  0x01   //alt[2][3-(y>>1)+7]

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vbsrl.v        vr28,  vr21,  4
    vextrins.w     vr28,  vr22,  0x30  //1234
    vbsrl.v        vr29,  vr22,  4     //5678
    vextrins.w     vr29,  vr23,  0x30
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[3][(y>>1)+x]
    vextrins.w     vr23,  vr29,  0x03
    vextrins.w     vr29,  vr28,  0x33
    vshuf4i.w      vr22,  vr29,  0x93
    vextrins.w     vr28,  vr21,  0x30
    vshuf4i.w      vr21,  vr28,  0x93

    add.d          a0,    a0,    a1

    // 4
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vadd.w         vr5,   vr5,   vr24  //diag[0][y+x]
    vadd.w         vr6,   vr6,   vr25

    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr13,  vr13,  vr26
    vadd.w         vr13,  vr13,  vr27  //alt[0][y+(x>>1)]

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr1,   a3,    0    //hv[0][y]

    vpermi.w       vr16,  vr16,  0x1b
    vadd.w         vr16,  vr16,  vr26
    vadd.w         vr16,  vr16,  vr27  //alt[1][3+y-(x>>1)]
    vpermi.w       vr16,  vr16,  0x1b

    vpermi.w       vr9,   vr9,   0x1b
    vpermi.w       vr10,  vr10,  0x1b
    vadd.w         vr10,  vr10,  vr24
    vadd.w         vr9,   vr9,   vr25
    vpermi.w       vr9,   vr9,   0x1b
    vpermi.w       vr10,  vr10,  0x1b  //diag[1][7+y-x]

    vbsrl.v        vr28,  vr18,  4
    vextrins.w     vr28,  vr19,  0x30  //1234
    vbsrl.v        vr29,  vr19,  4
    vextrins.w     vr29,  vr20,  0x30  //5678
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[2][3-(y>>1)+7]
    vextrins.w     vr20,  vr29,  0x03
    vextrins.w     vr29,  vr28,  0x33
    vshuf4i.w      vr19,  vr29,  0x93
    vbsll.v        vr18,  vr28,  4

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vbsrl.v        vr28,  vr21,  8
    vextrins.d     vr28,  vr22,  0x10
    vbsrl.v        vr29,  vr22,  8
    vextrins.d     vr29,  vr23,  0x10
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25
    vextrins.d     vr21,  vr28,  0x10
    vextrins.d     vr22,  vr28,  0x01
    vextrins.d     vr22,  vr29,  0x10
    vextrins.d     vr23,  vr29,  0x01  //alt[3][(y>>1)+x]

    add.d          a0,    a0,    a1

    // 5
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vbsrl.v        vr28,  vr5,   4  //5-8
    vbsrl.v        vr29,  vr6,   4  //9-12
    vextrins.w     vr28,  vr6,   0x30
    vadd.w         vr28,  vr28,  vr24  //diag[0][y+x]
    vadd.w         vr29,  vr29,  vr25
    vextrins.w     vr7,   vr29,  0x03
    vextrins.w     vr29,  vr28,  0x33
    vshuf4i.w      vr6,   vr29,  0x93
    vextrins.w     vr28,  vr5,   0x30
    vshuf4i.w      vr5,   vr28,  0x93

    vbsrl.v        vr28,  vr13,  4
    vextrins.w     vr28,  vr14,  0x30
    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[0][y+(x>>1)]
    vextrins.w     vr14,  vr28,  0x03
    vextrins.w     vr28,  vr13,  0x30
    vshuf4i.w      vr13,  vr28,  0x93

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr1,   a3,    1    //hv[0][y]

    vbsrl.v        vr28,  vr16,  4
    vextrins.w     vr28,  vr17,  0x30
    vpermi.w       vr28,  vr28,  0x1b
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[1][3+y-(x>>1)]
    vextrins.w     vr17,  vr28,  0x00
    vextrins.w     vr28,  vr16,  0x00
    vshuf4i.w      vr16,  vr28,  0x6c

    vbsrl.v        vr28,  vr9,   4
    vbsrl.v        vr29,  vr10,  4
    vextrins.w     vr28,  vr10,  0x30
    vpermi.w       vr28,  vr28,  0x1b  //8-5
    vpermi.w       vr29,  vr29,  0x1b  //12-9
    vadd.w         vr29,  vr29,  vr24
    vadd.w         vr28,  vr28,  vr25  //diag[1][7+y-x]
    vextrins.w     vr11,  vr29,  0x00
    vextrins.w     vr29,  vr28,  0x00
    vshuf4i.w      vr10,  vr29,  0x6c
    vextrins.w     vr28,  vr9,   0x00
    vshuf4i.w      vr9,   vr28,  0x6c

    vbsrl.v        vr28,  vr18,  4
    vextrins.w     vr28,  vr19,  0x30  //1234
    vbsrl.v        vr29,  vr19,  4
    vextrins.w     vr29,  vr20,  0x30  //5678
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[2][3-(y>>1)+7]
    vextrins.w     vr20,  vr29,  0x03
    vextrins.w     vr29,  vr28,  0x33
    vshuf4i.w      vr19,  vr29,  0x93
    vbsll.v        vr18,  vr28,  4

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vbsrl.v        vr28,  vr21,  8
    vextrins.d     vr28,  vr22,  0x10
    vbsrl.v        vr29,  vr22,  8
    vextrins.d     vr29,  vr23,  0x10
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25
    vextrins.d     vr21,  vr28,  0x10
    vextrins.d     vr22,  vr28,  0x01
    vextrins.d     vr22,  vr29,  0x10
    vextrins.d     vr23,  vr29,  0x01  //alt[3][(y>>1)+x]

    add.d          a0,    a0,    a1

    // 6
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vbsrl.v        vr28,  vr5,   8
    vbsrl.v        vr29,  vr6,   8
    vextrins.d     vr28,  vr6,   0x10  //6-9
    vextrins.d     vr29,  vr7,   0x10  //10-13
    vadd.w         vr28,  vr28,  vr24  //diag[0][y+x]
    vadd.w         vr29,  vr29,  vr25
    vextrins.d     vr5,   vr28,  0x10
    vextrins.d     vr6,   vr28,  0x01
    vextrins.d     vr6,   vr29,  0x10
    vextrins.d     vr7,   vr29,  0x01

    vbsrl.v        vr28,  vr13,  8
    vextrins.d     vr28,  vr14,  0x10
    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[0][y+(x>>1)]
    vextrins.d     vr13,  vr28,  0x10
    vextrins.d     vr14,  vr28,  0x01

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr1,   a3,    2    //hv[0][y]

    vbsrl.v        vr28,  vr16,  8
    vextrins.d     vr28,  vr17,  0x10
    vpermi.w       vr28,  vr28,  0x1b
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[1][3+y-(x>>1)]
    vpermi.w       vr28,  vr28,  0x1b
    vextrins.d     vr16,  vr28,  0x10
    vextrins.d     vr17,  vr28,  0x01

    vbsrl.v        vr28,  vr9,   8
    vextrins.d     vr28,  vr10,  0x10
    vbsrl.v        vr29,  vr10,  8
    vextrins.d     vr29,  vr11,  0x10
    vpermi.w       vr28,  vr28,  0x1b  //9876
    vpermi.w       vr29,  vr29,  0x1b  //13-10
    vadd.w         vr29,  vr29,  vr24
    vadd.w         vr28,  vr28,  vr25
    vpermi.w       vr28,  vr28,  0x1b
    vpermi.w       vr29,  vr29,  0x1b
    vextrins.d     vr9,   vr28,  0x10
    vextrins.d     vr10,  vr28,  0x01
    vextrins.d     vr10,  vr29,  0x10
    vextrins.d     vr11,  vr29,  0x01  //diag[1][7+y-x]

    vadd.w         vr18,  vr18,  vr24 //0123
    vadd.w         vr19,  vr19,  vr25 //4567 alt[2][3-(y>>1)+7]

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vbsll.v        vr28,  vr22,  4
    vextrins.w     vr28,  vr21,  0x03  //3456
    vbsll.v        vr29,  vr23,  4
    vextrins.w     vr29,  vr22,  0x03  //78910
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[3][(y>>1)+x]
    vextrins.w     vr21,  vr28,  0x30
    vextrins.w     vr28,  vr29,  0x00
    vshuf4i.w      vr22,  vr28,  0x39
    vbsrl.v        vr23,  vr29,  4

    add.d          a0,    a0,    a1

    // 7
    fld.d          f24,   a0,    0  //img
    vpermi.w       vr25,  vr24,  0x01

    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr24,  vr24,  0
    vsllwil.hu.bu  vr25,  vr25,  0
    vsllwil.hu.bu  vr25,  vr25,  0

    vsub.w         vr24,  vr24,  vr31  //px
    vsub.w         vr25,  vr25,  vr31

    vbsll.v        vr28,  vr6,   4
    vextrins.w     vr28,  vr5,   0x03 //78910
    vbsll.v        vr29,  vr7,   4
    vextrins.w     vr29,  vr6,   0x03 //11-14
    vadd.w         vr28,  vr28,  vr24  //diag[0][y+x]
    vadd.w         vr29,  vr29,  vr25
    vextrins.w     vr5,   vr28,  0x30
    vextrins.w     vr28,  vr29,  0x00
    vshuf4i.w      vr6,   vr28,  0x39
    vbsrl.v        vr7,   vr29,  4

    vbsll.v        vr28,  vr14,  4
    vextrins.w     vr28,  vr13,  0x03
    vpackev.w      vr26,  vr25,  vr24
    vpackod.w      vr27,  vr25,  vr24
    vpermi.w       vr26,  vr26,  0xd8 //px0246
    vpermi.w       vr27,  vr27,  0xd8 //px1357
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[0][y+(x>>1)]
    vextrins.w     vr13,  vr28,  0x30
    vbsrl.v        vr14,  vr28,  4

    vhaddw.d.w     vr28,  vr24,  vr24
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr25,  vr25
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr1,   a3,    3    //hv[0][y]

    vbsll.v        vr28,  vr17,  4
    vextrins.w     vr28,  vr16,  0x03
    vpermi.w       vr28,  vr28,  0x1b  //10987
    vadd.w         vr28,  vr28,  vr26
    vadd.w         vr28,  vr28,  vr27  //alt[1][3+y-(x>>1)]
    vextrins.w     vr16,  vr28,  0x33
    vshuf4i.w      vr17,  vr28,  0xc6
    vinsgr2vr.w    vr17,  zero,  3

    vbsll.v        vr28,  vr10,  4
    vextrins.w     vr28,  vr9,   0x03  //7-10
    vbsll.v        vr29,  vr11,  4
    vextrins.w     vr29,  vr10,  0x03  //11-14
    vpermi.w       vr28,  vr28,  0x1b  //10-7
    vpermi.w       vr29,  vr29,  0x1b  //14-11
    vadd.w         vr29,  vr29,  vr24
    vadd.w         vr28,  vr28,  vr25  //diag[1][7+y-x]
    vextrins.w     vr9,   vr28,  0x33
    vextrins.w     vr28,  vr29,  0x33
    vshuf4i.w      vr10,  vr28,  0xc6
    vshuf4i.w      vr11,  vr29,  0xc6
    vinsgr2vr.w    vr11,  zero,  3

    vadd.w         vr18,  vr18,  vr24 //0123
    vadd.w         vr19,  vr19,  vr25 //4567 alt[2][3-(y>>1)+7]

    vadd.w         vr2,   vr2,   vr24
    vadd.w         vr3,   vr3,   vr25  //hv[1][x]

    vbsll.v        vr28,  vr22,  4
    vextrins.w     vr28,  vr21,  0x03  //3456
    vbsll.v        vr29,  vr23,  4
    vextrins.w     vr29,  vr22,  0x03  //78910
    vadd.w         vr28,  vr28,  vr24
    vadd.w         vr29,  vr29,  vr25  //alt[3][(y>>1)+x]
    vextrins.w     vr21,  vr28,  0x30
    vextrins.w     vr28,  vr29,  0x00
    vshuf4i.w      vr22,  vr28,  0x39
    vbsrl.v        vr23,  vr29,  4

    add.d          a0,    a0,    a1

    vxor.v         vr24,  vr24,  vr24  //unsigned cost[8]
    vxor.v         vr25,  vr25,  vr25

    vmul.w         vr26,  vr0,   vr0
    vmul.w         vr27,  vr1,   vr1
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vhaddw.d.w     vr28,  vr27,  vr27
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4

    vmul.w         vr26,  vr2,   vr2
    vmul.w         vr27,  vr3,   vr3
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    vhaddw.d.w     vr28,  vr27,  vr27
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a5,    vr28,  0
    add.d          a4,    a4,    a5

    li.d           a6,    105
    mul.w          a3,    a3,    a6
    mul.w          a4,    a4,    a6
    vinsgr2vr.w    vr24,  a3,    2
    vinsgr2vr.w    vr25,  a4,    2

    vxor.v         vr30,  vr30,  vr30  //div_table
    vxor.v         vr31,  vr31,  vr31
    li.d           t0,    840
    vinsgr2vr.w    vr30,  t0,    0
    li.d           t0,    420
    vinsgr2vr.w    vr30,  t0,    1
    li.d           t0,    280
    vinsgr2vr.w    vr30,  t0,    2
    li.d           t0,    210
    vinsgr2vr.w    vr30,  t0,    3
    li.d           t0,    168
    vinsgr2vr.w    vr31,  t0,    0
    li.d           t0,    140
    vinsgr2vr.w    vr31,  t0,    1
    li.d           t0,    120
    vinsgr2vr.w    vr31,  t0,    2

    vbsll.v        vr27,  vr7,   4
    vextrins.w     vr27,  vr6,   0x03
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr26,  vr4,   vr4
    vmadd.w        vr26,  vr27,  vr27
    vmul.w         vr26,  vr26,  vr30
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a3,    vr28,  0
    vbsll.v        vr27,  vr6,   4
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr26,  vr5,   vr5
    vmadd.w        vr26,  vr27,  vr27
    vmul.w         vr26,  vr26,  vr31
    vextrins.w     vr26,  vr31,  0x33
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4   //cost[0]

    vbsll.v        vr27,  vr11,  4
    vextrins.w     vr27,  vr10,  0x03
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr26,  vr8,   vr8
    vmadd.w        vr26,  vr27,  vr27
    vmul.w         vr26,  vr26,  vr30
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    vbsll.v        vr27,  vr10,  4
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr26,  vr9,   vr9
    vmadd.w        vr26,  vr27,  vr27
    vmul.w         vr26,  vr26,  vr31
    vextrins.w     vr26,  vr31,  0x33
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a5,    vr28,  0
    add.d          a4,    a4,    a5   //cost[4]

    vpickve2gr.w   a5,    vr5,   3
    mul.w          a5,    a5,    a5
    mul.w          a5,    a5,    a6
    add.w          a3,    a3,    a5
    vinsgr2vr.w    vr24,  a3,    0
    vpickve2gr.w   a5,    vr9,   3
    mul.w          a5,    a5,    a5
    mul.w          a5,    a5,    a6
    add.w          a4,    a4,    a5
    vinsgr2vr.w    vr25,  a4,    0

    //n=0
    vpickve2gr.w   a3,    vr24,  1
    vmul.w         vr26,  vr13,  vr13
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    vpickve2gr.w   a5,    vr12,  3
    mul.w          a5,    a5,    a5
    add.d          a3,    a3,    a4
    add.d          a3,    a3,    a5
    mul.w          a3,    a3,    a6  //*cost_ptr

    vextrins.w     vr29,  vr30,  0x01
    vextrins.w     vr29,  vr30,  0x13
    vextrins.w     vr29,  vr31,  0x21
    vextrins.w     vr29,  vr31,  0x33
    vbsll.v        vr27,  vr14,  4
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr28,  vr12,  vr12
    vextrins.w     vr28,  vr31,  0x33
    vmadd.w        vr28,  vr27,  vr27
    vmul.w         vr26,  vr28,  vr29
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr24,  a3,    1

    //n=1
    vpickve2gr.w   a3,    vr24,  3
    vmul.w         vr26,  vr16,  vr16
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    vpickve2gr.w   a5,    vr15,  3
    mul.w          a5,    a5,    a5
    add.d          a3,    a3,    a4
    add.d          a3,    a3,    a5
    mul.w          a3,    a3,    a6  //*cost_ptr

    vbsll.v        vr27,  vr17,  4
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr28,  vr15,  vr15
    vextrins.w     vr28,  vr31,  0x33
    vmadd.w        vr28,  vr27,  vr27
    vmul.w         vr26,  vr28,  vr29
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr24,  a3,    3

    //n=2
    vpickve2gr.w   a3,    vr25,  1
    vmul.w         vr26,  vr19,  vr19
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    vpickve2gr.w   a5,    vr18,  3
    mul.w          a5,    a5,    a5
    add.d          a3,    a3,    a4
    add.d          a3,    a3,    a5
    mul.w          a3,    a3,    a6  //*cost_ptr

    vbsll.v        vr27,  vr20,  4
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr28,  vr18,  vr18
    vextrins.w     vr28,  vr31,  0x33
    vmadd.w        vr28,  vr27,  vr27
    vmul.w         vr26,  vr28,  vr29
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr25,  a3,    1

    //n=3
    vpickve2gr.w   a3,    vr25,  3
    vmul.w         vr26,  vr22,  vr22
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    vpickve2gr.w   a5,    vr21,  3
    mul.w          a5,    a5,    a5
    add.d          a3,    a3,    a4
    add.d          a3,    a3,    a5
    mul.w          a3,    a3,    a6  //*cost_ptr

    vbsll.v        vr27,  vr23,  4
    vpermi.w       vr27,  vr27,  0x1b
    vmul.w         vr28,  vr21,  vr21
    vextrins.w     vr28,  vr31,  0x33
    vmadd.w        vr28,  vr27,  vr27
    vmul.w         vr26,  vr28,  vr29
    vhaddw.d.w     vr28,  vr26,  vr26
    vhaddw.q.d     vr28,  vr28,  vr28
    vpickve2gr.d   a4,    vr28,  0
    add.d          a3,    a3,    a4
    vinsgr2vr.w    vr25,  a3,    3

    xor            a3,    a3,    a3  //best_dir
    vpickve2gr.w   a4,    vr24,  0   //best_cost
.BSETDIR01:
    vpickve2gr.w   a5,    vr24,  1
    bge            a4,    a5,    .BSETDIR02
    or             a4,    a5,    a5
    ori            a3,    zero,  1
.BSETDIR02:
    vpickve2gr.w   a5,    vr24,  2
    bge            a4,    a5,    .BSETDIR03
    or             a4,    a5,    a5
    ori            a3,    zero,  2
.BSETDIR03:
    vpickve2gr.w   a5,    vr24,  3
    bge            a4,    a5,    .BSETDIR04
    or             a4,    a5,    a5
    ori            a3,    zero,  3
.BSETDIR04:
    vpickve2gr.w   a5,    vr25,  0
    bge            a4,    a5,    .BSETDIR05
    or             a4,    a5,    a5
    ori            a3,    zero,  4
.BSETDIR05:
    vpickve2gr.w   a5,    vr25,  1
    bge            a4,    a5,    .BSETDIR06
    or             a4,    a5,    a5
    ori            a3,    zero,  5
.BSETDIR06:
    vpickve2gr.w   a5,    vr25,  2
    bge            a4,    a5,    .BSETDIR07
    or             a4,    a5,    a5
    ori            a3,    zero,  6
.BSETDIR07:
    vpickve2gr.w   a5,    vr25,  3
    bge            a4,    a5,    .BSETDIREND
    or             a4,    a5,    a5
    ori            a3,    zero,  7
.BSETDIREND:
    xori           a5,    a3,    4
    li.d           a1,    4
    bge            a5,    a1,    .GETCOST01
    vreplve.w      vr26,  vr24,  a5
    b              .GETCOST02
.GETCOST01:
    vreplve.w      vr26,  vr25,  a5
.GETCOST02:
    vpickve2gr.w   a5,    vr26,  0
    sub.w          a5,    a4,    a5
    srai.d         a5,    a5,    10
    st.w           a5,    a2,    0
    or             a0,    a3,    a3

    fld.d          f24,   sp,    0
    fld.d          f25,   sp,    8
    fld.d          f26,   sp,    16
    fld.d          f27,   sp,    24
    fld.d          f28,   sp,    32
    fld.d          f29,   sp,    40
    fld.d          f30,   sp,    48
    fld.d          f31,   sp,    56
    addi.d         sp,    sp,    64

endfunc

.macro cdef_fill tmp, stride, w, h
    beqz          \h,     700f         //h
    or            t0,     zero,  zero  //y
100:
    or            t1,     zero,  zero  //xx
    srai.d        s6,     \w,    3     //x
    beqz          s6,     300f
200:
    vstx          vr18,   \tmp,    t1
    addi.d        t1,     t1,    16
    addi.d        s6,     s6,    -1
    bnez          s6,     200b
300:
    andi          s6,     \w,    4
    beqz          s6,     400f
    fstx.d        f18,    \tmp,    t1
    addi.d        t1,     t1,    8
400:
    andi          s6,     \w,    2
    beqz          s6,     500f
    fstx.s        f18,    \tmp,    t1
    addi.d        t1,     t1,    4
500:
    andi          s6,     \w,    1
    beqz          s6,     600f
    li.w          s6,     -16384
    stx.h         s6,     \tmp,    t1
    addi.d        t1,     t1,    2
600:
    add.d         \tmp,     \tmp,    \stride
    add.d         \tmp,     \tmp,    \stride
    addi.d        t0,     t0,    1
    blt           t0,     \h,    100b
700:
.endm

const dav1d_cdef_directions
.byte   1 * 12 + 0,  2 * 12 + 0
.byte   1 * 12 + 0,  2 * 12 - 1
.byte   -1 * 12 + 1, -2 * 12 + 2
.byte   0 * 12 + 1, -1 * 12 + 2
.byte   0 * 12 + 1,  0 * 12 + 2
.byte   0 * 12 + 1,  1 * 12 + 2
.byte   1 * 12 + 1,  2 * 12 + 2
.byte   1 * 12 + 0,  2 * 12 + 1
.byte   1 * 12 + 0,  2 * 12 + 0
.byte   1 * 12 + 0,  2 * 12 - 1
.byte   -1 * 12 + 1, -2 * 12 + 2
.byte   0 * 12 + 1, -1 * 12 + 2
endconst

.macro constrain_vrh in0, in1, in2, tmp0, tmp1, out
    vabsd.h        \tmp0, \in0,  vr23   //adiff
    vsra.h         \tmp1, \tmp0, \in2
    vsub.h         \tmp1, \in1,  \tmp1
    vmax.h         \tmp1, vr23,  \tmp1  //imax
    vmin.h         \tmp0, \tmp0, \tmp1  //imin

    //apply_sign
    vslt.h         \tmp1, \in0,  vr23
    vandn.v        \in0,  \tmp1, \tmp0
    vsigncov.h     \tmp0, \tmp1, \tmp0
    vor.v          \out,  \in0,  \tmp0
.endm

.macro iclip_vrh in0, in1, in2, tmp0, tmp1, out
    vmin.h         \tmp0, \in2,  \in0
    vslt.h         \in0,  \in0,  \in1
    vand.v         \tmp1, \in0,  \in1
    vandn.v        \tmp0, \in0,  \tmp0
    vor.v          \out,  \tmp1, \tmp0
.endm

.macro cdef_padding_data
    //y < 0
    beqz          t7,     90f
4:
    or            t4,     t5,    t5  //data index xx
    slli.d        t0,     t4,    1
    mul.w         t2,     t7,    s5
    slli.d        t2,     t2,    1
    add.d         t2,     s4,    t2

    sub.d         t3,     t6,    t5  //loop param x
    srai.d        t3,     t3,    3
    add.d         t3,     t3,    t5
    beq           t5,     t3,    6f
5:  // /8
    fldx.d        f18,    a3,    t4
    vsllwil.hu.bu vr18,   vr18,  0
    vstx          vr18,   t2,    t0
    addi.d        t0,     t0,    16
    addi.d        t4,     t4,    8

    addi.d        t3,     t3,    -1
    bne           t5,     t3,    5b
6:  // &4
    sub.d         t1,     t6,    t5
    andi          t1,     t1,    4
    beqz          t1,     7f

    fldx.s        f18,    a3,    t4
    vsllwil.hu.bu vr18,   vr18,  0
    fstx.d        f18,    t2,    t0
    addi.d        t0,     t0,    8
    addi.d        t4,     t4,    4
7:  // &2
    sub.d         t1,     t6,    t5
    andi          t1,     t1,    2
    beqz          t1,     9f

    ldx.bu        t1,     a3,    t4
    stx.h         t1,     t2,    t0
    addi.d        t0,     t0,    2
    addi.d        t4,     t4,    1
    ldx.bu        t1,     a3,    t4
    stx.h         t1,     t2,    t0
    addi.d        t0,     t0,    2
    addi.d        t4,     t4,    1
9:
    add.d         a3,     a3,    a1
    addi.d        t7,     t7,    1
    bnez          t7,     4b

90:
    // y < h
    beqz          s1,     12f
    beqz          t5,     12f
    or            t7,     zero,  zero  //y
10:
    or            t4,     t5,    t5  //data index x
11:
    slli.d        t3,     t7,    1
    addi.d        t3,     t3,    2
    add.d         t3,     t3,    t4
    ldx.bu        t1,     a2,    t3

    mul.w         t3,     t7,    s5
    add.d         t3,     t3,    t4
    slli.d        t3,     t3,    1
    stx.h         t1,     s4,    t3

    addi.d        t4,     t4,    1
    bnez          t4,     11b

    addi.d        t7,     t7,    1
    bne           t7,     s1,    10b

12:
    // y = 0 ; y < h
    or            s0,     s4,    s4
    beqz          s1,     20f
    or            s6,     a0,    a0
    or            t7,     zero,  zero  //y
    srai.d        t4,     t6,    3    //loop max
13:
    or            t0,     zero,  zero //loop param
    or            t3,     t0,    t0   //data index src
    or            t1,     t0,    t0   //data index tmp
    beqz          t4,     16f
15:  // /8
    fldx.d        f18,    s6,    t3
    vsllwil.hu.bu vr18,   vr18,  0
    vstx          vr18,   s0,    t1
    addi.d        t3,     t3,    8
    addi.d        t1,     t1,    16

    addi.d        t0,     t0,    1
    blt           t0,     t4,    15b
16:  // &4
    andi          t0,     t6,    4
    beqz          t0,     17f

    fldx.s        f18,    s6,    t3
    vsllwil.hu.bu vr18,   vr18,  0
    fstx.d        f18,    s0,    t1
    addi.d        t3,     t3,    4
    addi.d        t1,     t1,    8
17:  // &2
    andi          t0,     t6,    2
    beqz          t0,     19f

    ldx.bu        t2,     s6,    t3
    stx.h         t2,     s0,    t1
    addi.d        t3,     t3,    1
    addi.d        t1,     t1,    2
    ldx.bu        t2,     s6,    t3
    stx.h         t2,     s0,    t1
    addi.d        t3,     t3,    1
    addi.d        t1,     t1,    2
19: // src+ tmp+
    add.d         s6,     s6,    a1
    add.d         s0,     s0,    s5
    add.d         s0,     s0,    s5

    addi.d        t7,     t7,    1
    blt           t7,     s1,    13b

    // y = h ; y < y_end
20:
    beq           s1,     t8,    27f
    or            t7,     s1,    s1  //y
    sub.d         t4,     t6,    t5
    srai.d        t4,     t4,    3
    add.d         t4,     t4,    t5   //8 loop max
21:
    or            t0,     t5,    t5   //xx
    or            t3,     t0,    t0   //data index bottom
    slli.d        t1,     t0,    1    //data index tmp
    beq           t5,     t4,    23f
22:  // /8
    fldx.d        f18,    a4,    t3
    vsllwil.hu.bu vr18,   vr18,  0
    vstx          vr18,   s0,    t1
    addi.d        t3,     t3,    8
    addi.d        t1,     t1,    16

    addi.d        t0,     t0,    1
    blt           t0,     t4,    22b
23:  // &4
    sub.d         t0,     t6,    t5
    andi          t0,     t0,    4
    beqz          t0,     24f

    fldx.s        f18,    a4,    t3
    vsllwil.hu.bu vr18,   vr18,  0
    fstx.d        f18,    s0,    t1
    addi.d        t3,     t3,    4
    addi.d        t1,     t1,    8
24:  // &2
    sub.d         t0,     t6,    t5
    andi          t0,     t0,    2
    beqz          t0,     26f

    ldx.bu        t2,     a4,    t3
    stx.h         t2,     s0,    t1
    addi.d        t3,     t3,    1
    addi.d        t1,     t1,    2
    ldx.bu        t2,     a4,    t3
    stx.h         t2,     s0,    t1
    addi.d        t3,     t3,    1
    addi.d        t1,     t1,    2
26: // bottom+ tmp+
    add.d         a4,     a4,    a1
    add.d         s0,     s0,    s5
    add.d         s0,     s0,    s5

    addi.d        t7,     t7,    1
    blt           t7,     t8,    21b
27:
    // padding end
.endm

.macro cdef_pri_sec_init
    clz.w          t3,    a6
    sub.w          t3,    t2,    t3
    sub.w          t3,    s7,    t3  //sec_shift

    vreplgr2vr.h   vr4,   t0         //pri_tap_k
    vreplgr2vr.h   vr9,   a5         //pri_strength
    vreplgr2vr.h   vr10,  t1         //pri_shift
    vreplgr2vr.h   vr18,  a6         //sec_strength
    vreplgr2vr.h   vr19,  t3         //sec_shift

    or             t2,    s1,    s1  //dowhile loop param
    addi.d         s1,    a7,    2
    slli.d         s1,    s1,    1   //directions dir+2
    addi.d         s2,    a7,    4
    slli.d         s2,    s2,    1   //directions dir+4
    slli.d         s3,    a7,    1   //directions dir+0

    la.local       t0,    dav1d_cdef_directions
    add.d          s1,    t0,    s1
    ld.b           a2,    s1,    0  //off01
    ld.b           a3,    s1,    1  //off11
    add.d          s2,    t0,    s2
    ld.b           s1,    s2,    0  //off02
    ld.b           s2,    s2,    1  //off12
    add.d          s3,    t0,    s3
    ld.b           t0,    s3,    0  //off03
    ld.b           s3,    s3,    1  //off13

    slli.d         a2,    a2,    1
    slli.d         a3,    a3,    1
    slli.d         s1,    s1,    1
    slli.d         s2,    s2,    1
    slli.d         t0,    t0,    1
    slli.d         s3,    s3,    1
.endm

.macro cdef_pri_init
    vreplgr2vr.h   vr4,   t0         //pri_tap_k
    vreplgr2vr.h   vr9,   a5         //pri_strength
    vreplgr2vr.h   vr10,  t1         //pri_shift

    or             t2,    s1,    s1  //dowhile loop param
    addi.d         s1,    a7,    2
    slli.d         s1,    s1,    1   //directions dir+2

    la.local       t0,    dav1d_cdef_directions
    add.d          s1,    t0,    s1
    ld.b           a2,    s1,    0  //off01
    ld.b           a3,    s1,    1  //off11

    slli.d         a2,    a2,    1
    slli.d         a3,    a3,    1
.endm

.macro cdef_sec_init
    clz.w          t3,    a6
    li.w           t2,    31
    sub.w          t3,    t2,    t3
    sub.w          t3,    s7,    t3  //sec_shift

    vreplgr2vr.h   vr18,  a6         //sec_strength
    vreplgr2vr.h   vr19,  t3         //sec_shift

    or             t2,    s1,    s1  //dowhile loop param
    addi.d         s2,    a7,    4
    slli.d         s2,    s2,    1   //directions dir+4
    slli.d         s3,    a7,    1   //directions dir+0

    la.local       t0,    dav1d_cdef_directions
    add.d          s1,    t0,    s1
    add.d          s2,    t0,    s2
    ld.b           s1,    s2,    0  //off02
    ld.b           s2,    s2,    1  //off12
    add.d          s3,    t0,    s3
    ld.b           t0,    s3,    0  //off03
    ld.b           s3,    s3,    1  //off13

    slli.d         s1,    s1,    1
    slli.d         s2,    s2,    1
    slli.d         t0,    t0,    1
    slli.d         s3,    s3,    1
.endm

.macro cdef_process_data_w8 in0, in1
    vsub.h       vr11,   vr5,   vr0
    vsub.h       vr12,   vr6,   vr0
    vsub.h       vr13,   vr7,   vr0
    vsub.h       vr14,   vr8,   vr0

    constrain_vrh   vr11,  \in0,   \in1,  vr16,  vr17,  vr11
    constrain_vrh   vr12,  \in0,   \in1,  vr16,  vr17,  vr12
    constrain_vrh   vr13,  \in0,   \in1,  vr16,  vr17,  vr13
    constrain_vrh   vr14,  \in0,   \in1,  vr16,  vr17,  vr14
.endm

.macro cdef_process_data_w4 in0, in1
    vpermi.w       vr6,  vr5,  0x44
    vpermi.w       vr8,  vr7,  0x44

    vsub.h         vr12,  vr6,   vr0
    vsub.h         vr14,  vr8,   vr0

    constrain_vrh   vr12,  \in0,   \in1,  vr16,  vr17,  vr12
    constrain_vrh   vr14,  \in0,   \in1,  vr16,  vr17,  vr14
.endm

.macro cdef_calc_sum_tapchange_w8
    vmul.h         vr1,   vr15,  vr11  //sum
    vmadd.h        vr1,   vr15,  vr12  //sum
    vand.v         vr15,  vr15,  vr21
    vor.v          vr15,  vr15,  vr22
    vmadd.h        vr1,   vr15,  vr13  //sum
    vmadd.h        vr1,   vr15,  vr14  //sum
.endm

.macro cdef_calc_sum_tapchange_w4
    vmul.h         vr1,   vr15,  vr12  //sum
    vand.v         vr15,  vr15,  vr21
    vor.v          vr15,  vr15,  vr22
    vmadd.h        vr1,   vr15,  vr14  //sum
.endm

.macro cdef_calc_sum_no_tapchange_w4 in0
    vmadd.h        vr1,   \in0,  vr12
    vmadd.h        vr1,   \in0,  vr14
.endm

.macro cdef_calc_sum_no_tapchange_w8 in0
    vmadd.h        vr1,   \in0,  vr11  //sum
    vmadd.h        vr1,   \in0,  vr12
    vmadd.h        vr1,   \in0,  vr13
    vmadd.h        vr1,   \in0,  vr14
.endm

.macro cdef_calc_maxmin_w4
    vmin.hu        vr3,   vr6,   vr3
    vmax.h         vr2,   vr6,   vr2
    vmin.hu        vr3,   vr8,   vr3  //min
    vmax.h         vr2,   vr8,   vr2  //max
.endm

.macro cdef_calc_maxmin_w8
    vmin.hu        vr3,   vr5,   vr3
    vmax.h         vr2,   vr5,   vr2
    vmin.hu        vr3,   vr6,   vr3
    vmax.h         vr2,   vr6,   vr2
    vmin.hu        vr3,   vr7,   vr3
    vmax.h         vr2,   vr7,   vr2
    vmin.hu        vr3,   vr8,   vr3  //min
    vmax.h         vr2,   vr8,   vr2  //max
.endm

.macro cdef_calc_dst
    vslti.h        vr5,   vr1,   0
    vand.v         vr5,   vr5,   vr20
    vsub.h         vr5,   vr1,   vr5
    vaddi.hu       vr5,   vr5,   8
    vsrai.h        vr5,   vr5,   4
    vadd.h         vr5,   vr0,   vr5
.endm

//static NOINLINE void cdef_filter_block_lsx
//                    (pixel *dst, const ptrdiff_t dst_stride,
//                     const pixel (*left)[2], const pixel *const top,
//                     const int pri_strength, const int sec_strength,
//                     const int dir, const int damping, const int w, int h,
//                     const enum CdefEdgeFlags edges HIGHBD_DECL_SUFFIX)
// w=4 h=4
//param: dst:a0, dst_stride:a1, left:a2, top:a3, bottom:a4, pri_strength:a5
//sec_strength:a6, dir:a7, damping:s7, w:s0, h:s1, edges:s2
function cdef_filter_block_4x4_8bpc_lsx
    ld.w           t0,    sp,    0
    ld.w           t1,    sp,    8
    addi.d         sp,    sp,    -(64+288)
    st.d           s0,    sp,    0
    st.d           s1,    sp,    8
    st.d           s2,    sp,    16
    st.d           s3,    sp,    24
    st.d           s4,    sp,    32
    st.d           s5,    sp,    40
    st.d           s6,    sp,    48
    st.d           s7,    sp,    56

    li.w           s0,    4         //w
    li.w           s1,    4         //h
    or             s2,    t1,    t1 //edges
    or             s7,    t0,    t0 //damping

    li.d           s5,    12         //tmp_stride
    addi.d         s4,    sp,    64
    slli.d         t0,    s5,    1
    addi.d         t0,    t0,    2
    slli.d         t0,    t0,    1
    add.d          s4,    s4,    t0  //ptr tmp
    vxor.v         vr23,  vr23,  vr23
    li.w           t2,    1
    vreplgr2vr.h   vr20,  t2
    vaddi.hu       vr21,  vr20,  2
    vaddi.hu       vr22,  vr20,  1

    li.w          t0,     -16384
    vreplgr2vr.h  vr18,   t0

    //padding
    li.w          t5,     -2        //x_start
    addi.d        t6,     s0,    2  //x_end
    li.w          t7,     -2        //y_start
    addi.d        t8,     s1,    2  //y_end
    li.w          t2,     2

    andi          t4,     s2,    4
    bnez          t4,     1f

    //CDEF_HAVE_TOP
    slli.d        t3,     s5,    2
    addi.d        t4,     s4,    -4
    sub.d         t4,     t4,    t3
    addi.d        t3,     s0,    4

    cdef_fill     t4,     s5,    t3,     t2

    or            t7,     zero,  zero

1:  //CDEF_HAVE_BOTTOM
    andi          t4,     s2,8
    bnez          t4,     2f

    mul.w         t3,     s1,    s5
    slli.d        t3,     t3,  1
    add.d         t4,     s4,  t3
    addi.d        t4,     t4,    -4
    li.d          t3,     8

    cdef_fill     t4,     s5,    t3,     t2

    addi.d        t8,     t8,    -2

2:  //CDEF_HAVE_LEFT
    andi          t4,     s2,1
    bnez          t4,     3f

    mul.w         t3,     t7,    s5
    slli.d        t3,     t3,    1
    add.d         t4,     s4,    t3
    addi.d        t4,     t4,    -4
    sub.d         t3,     t8,    t7

    cdef_fill     t4,     s5,    t2,     t3

    or            t5,     zero,  zero

3:  //CDEF_HAVE_RIGHT
    andi          t4,     s2,2
    bnez          t4,     40f

    mul.w         t3,     t7,    s5
    slli.d        t3,     t3,    1
    add.d         t4,     s4,    t3
    addi.d        t4,     t4,    8
    sub.d         t3,     t8,    t7

    cdef_fill     t4,     s5,    t2,     t3

    addi.d        t6,     t6,    -2

40:
    cdef_padding_data

    beqz           a5,    33f

28:  //if (pri_strength)
    li.w           t0,    4
    andi           t1,    a5,    1
    sub.d          t0,    t0,    t1  //pri_tap

    clz.w          t1,    a5
    li.d           t2,    31
    sub.w          t1,    t2,    t1
    sub.w          t1,    s7,    t1

    blt            t1,    zero,  281f
    or             t1,    t1,    t1
    b              282f
281:
    or             t1,    zero,  zero   //t1: pri_shift
282:

    beqz           a6,    31f

29:  //if (sec_strength)
    cdef_pri_sec_init

30:
    fld.s          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0
    vpermi.w       vr0,   vr0,   0x44

    vxor.v         vr1,   vr1,   vr1   //sum
    vor.v          vr2,   vr0,   vr0   //max
    vor.v          vr3,   vr0,   vr0   //min
    vor.v          vr15,  vr4,   vr4   //pri_tap_k

    sub.d          t4,    s4,    a2
    sub.d          t5,    s4,    a3

    fldx.d         f5,    s4,    a2   //p0_00
    fld.d          f6,    t4,    0    //p0_01
    fldx.d         f7,    s4,    a3   //p0_10
    fld.d          f8,    t5,    0    //p0_11

    cdef_process_data_w4 vr9,   vr10
    cdef_calc_sum_tapchange_w4
    cdef_calc_maxmin_w4

    sub.d          t4,    s4,    s1  //tmp[-off02]
    sub.d          t5,    s4,    t0  //tmp[-off03]

    fldx.d         f5,    s4,    s1   //s0_00
    fld.d          f6,    t4,    0    //s0_01
    fldx.d         f7,    s4,    t0   //s0_02
    fld.d          f8,    t5,    0    //s0_03

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr22
    cdef_calc_maxmin_w4

    sub.d          t4,    s4,    s2  //tmp[-off12]
    sub.d          t5,    s4,    s3  //tmp[-off13]

    fldx.d         f5,    s4,    s2   //s0_10
    fld.d          f6,    t4,    0    //s0_11
    fldx.d         f7,    s4,    s3   //s0_12
    fld.d          f8,    t5,    0    //s0_13

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr20
    cdef_calc_maxmin_w4

    vshuf4i.w      vr5,   vr1,   0x0e
    vshuf4i.w      vr6,   vr3,   0x0e
    vshuf4i.w      vr7,   vr2,   0x0e
    vadd.h         vr1,   vr1,   vr5
    vmin.hu        vr3,   vr6,   vr3
    vmax.h         vr2,   vr7,   vr2

    cdef_calc_dst
    iclip_vrh       vr5,   vr3,   vr2,  vr16,  vr17,  vr5

    vsrlni.b.h     vr5,   vr5,   0
    fst.s          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    30b
    b              35f

31:  // pri_strength only
    cdef_pri_init

32:
    fld.s          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0
    vpermi.w       vr0,   vr0,   0x44

    vxor.v         vr1,   vr1,   vr1   //sum
    vor.v          vr15,  vr4,   vr4   //pri_tap_k

    sub.d          t4,    s4,    a2
    sub.d          t5,    s4,    a3

    fldx.d         f5,    s4,    a2   //p0_00
    fld.d          f6,    t4,    0    //p0_01
    fldx.d         f7,    s4,    a3   //p0_10
    fld.d          f8,    t5,    0    //p0_11

    cdef_process_data_w4 vr9,   vr10
    cdef_calc_sum_tapchange_w4

    vshuf4i.w      vr5,   vr1,   0x0e
    vadd.h         vr1,   vr1,   vr5

    cdef_calc_dst

    vsrlni.b.h     vr5,   vr5,   0
    fst.s          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    32b
    b              35f

33:   // sec_strength only
    cdef_sec_init

34:
    fld.s          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0
    vpermi.w       vr0,   vr0,   0x44

    vxor.v         vr1,   vr1,   vr1   //sum

    sub.d          t4,    s4,    s1  //tmp[-off02]
    sub.d          t5,    s4,    t0  //tmp[-off03]

    fldx.d         f5,    s4,    s1   //s0_00
    fld.d          f6,    t4,    0    //s0_01
    fldx.d         f7,    s4,    t0   //s0_02
    fld.d          f8,    t5,    0    //s0_03

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr22

    sub.d          t4,    s4,    s2  //tmp[-off12]
    sub.d          t5,    s4,    s3  //tmp[-off13]

    fldx.d         f5,    s4,    s2   //s0_10
    fld.d          f6,    t4,    0    //s0_11
    fldx.d         f7,    s4,    s3   //s0_12
    fld.d          f8,    t5,    0    //s0_13

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr20

    vshuf4i.w      vr5,   vr1,   0x0e
    vadd.h         vr1,   vr1,   vr5

    cdef_calc_dst

    vsrlni.b.h     vr5,   vr5,   0
    fst.s          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    34b

35:
    ld.d           s0,    sp,    0
    ld.d           s1,    sp,    8
    ld.d           s2,    sp,    16
    ld.d           s3,    sp,    24
    ld.d           s4,    sp,    32
    ld.d           s5,    sp,    40
    ld.d           s6,    sp,    48
    ld.d           s7,    sp,    56
    addi.d         sp,    sp,    (64+288)
endfunc

function cdef_filter_block_4x8_8bpc_lsx
    ld.w           t0,    sp,    0
    ld.w           t1,    sp,    8
    addi.d         sp,    sp,    -(64+288)
    st.d           s0,    sp,    0
    st.d           s1,    sp,    8
    st.d           s2,    sp,    16
    st.d           s3,    sp,    24
    st.d           s4,    sp,    32
    st.d           s5,    sp,    40
    st.d           s6,    sp,    48
    st.d           s7,    sp,    56

    li.w           s0,    4         //w
    li.w           s1,    8         //h
    or             s2,    t1,    t1 //edges
    or             s7,    t0,    t0 //damping

    li.d           s5,    12         //tmp_stride
    addi.d         s4,    sp,    64
    slli.d         t0,    s5,    1
    addi.d         t0,    t0,    2
    slli.d         t0,    t0,    1
    add.d          s4,    s4,    t0  //ptr tmp
    vxor.v         vr23,  vr23,  vr23
    li.w           t2,    1
    vreplgr2vr.h   vr20,  t2
    vaddi.hu       vr21,  vr20,  2
    vaddi.hu       vr22,  vr20,  1

    li.w          t0,     -16384
    vreplgr2vr.h  vr18,   t0

    //padding
    li.w          t5,     -2        //x_start
    addi.d        t6,     s0,    2  //x_end
    li.w          t7,     -2        //y_start
    addi.d        t8,     s1,    2  //y_end
    li.w          t2,     2

    andi          t4,     s2,    4
    bnez          t4,     1f

    //CDEF_HAVE_TOP
    slli.d        t3,     s5,    2
    addi.d        t4,     s4,    -4
    sub.d         t4,     t4,    t3
    addi.d        t3,     s0,    4

    cdef_fill     t4,     s5,    t3,     t2

    or            t7,     zero,  zero

1:  //CDEF_HAVE_BOTTOM
    andi          t4,     s2,8
    bnez          t4,     2f

    mul.w         t3,     s1,    s5
    slli.d        t3,     t3,  1
    add.d         t4,     s4,  t3
    addi.d        t4,     t4,    -4
    li.d          t3,     8

    cdef_fill     t4,     s5,    t3,     t2

    addi.d        t8,     t8,    -2

2:  //CDEF_HAVE_LEFT
    andi          t4,     s2,1
    bnez          t4,     3f

    mul.w         t3,     t7,    s5
    slli.d        t3,     t3,    1
    add.d         t4,     s4,    t3
    addi.d        t4,     t4,    -4
    sub.d         t3,     t8,    t7

    cdef_fill     t4,     s5,    t2,     t3

    or            t5,     zero,  zero

3:  //CDEF_HAVE_RIGHT
    andi          t4,     s2,2
    bnez          t4,     40f

    mul.w         t3,     t7,    s5
    slli.d        t3,     t3,    1
    add.d         t4,     s4,    t3
    addi.d        t4,     t4,    8
    sub.d         t3,     t8,    t7

    cdef_fill     t4,     s5,    t2,     t3

    addi.d        t6,     t6,    -2

40:
    cdef_padding_data

    beqz           a5,    33f

28:  //if (pri_strength)
    li.w           t0,    4
    andi           t1,    a5,    1
    sub.d          t0,    t0,    t1  //pri_tap

    clz.w          t1,    a5
    li.d           t2,    31
    sub.w          t1,    t2,    t1
    sub.w          t1,    s7,    t1

    blt            t1,    zero,  281f
    or             t1,    t1,    t1
    b              282f
281:
    or             t1,    zero,  zero   //t1: pri_shift
282:

    beqz           a6,    31f

29:  //if (sec_strength)
    cdef_pri_sec_init

30:
    fld.s          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0
    vpermi.w       vr0,   vr0,   0x44

    vxor.v         vr1,   vr1,   vr1   //sum
    vor.v          vr2,   vr0,   vr0   //max
    vor.v          vr3,   vr0,   vr0   //min
    vor.v          vr15,  vr4,   vr4   //pri_tap_k

    sub.d          t4,    s4,    a2
    sub.d          t5,    s4,    a3

    fldx.d         f5,    s4,    a2   //p0_00
    fld.d          f6,    t4,    0    //p0_01
    fldx.d         f7,    s4,    a3   //p0_10
    fld.d          f8,    t5,    0    //p0_11

    cdef_process_data_w4 vr9,   vr10
    cdef_calc_sum_tapchange_w4
    cdef_calc_maxmin_w4

    sub.d          t4,    s4,    s1  //tmp[-off02]
    sub.d          t5,    s4,    t0  //tmp[-off03]

    fldx.d         f5,    s4,    s1   //s0_00
    fld.d          f6,    t4,    0    //s0_01
    fldx.d         f7,    s4,    t0   //s0_02
    fld.d          f8,    t5,    0    //s0_03

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr22
    cdef_calc_maxmin_w4

    sub.d          t4,    s4,    s2  //tmp[-off12]
    sub.d          t5,    s4,    s3  //tmp[-off13]

    fldx.d         f5,    s4,    s2   //s0_10
    fld.d          f6,    t4,    0    //s0_11
    fldx.d         f7,    s4,    s3   //s0_12
    fld.d          f8,    t5,    0    //s0_13

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr20
    cdef_calc_maxmin_w4

    vshuf4i.w      vr5,   vr1,   0x0e
    vshuf4i.w      vr6,   vr3,   0x0e
    vshuf4i.w      vr7,   vr2,   0x0e
    vadd.h         vr1,   vr1,   vr5
    vmin.hu        vr3,   vr6,   vr3
    vmax.h         vr2,   vr7,   vr2

    cdef_calc_dst
    iclip_vrh       vr5,   vr3,   vr2,  vr16,  vr17,  vr5

    vsrlni.b.h     vr5,   vr5,   0
    fst.s          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    30b
    b              35f

31:  // pri_strength only
    cdef_pri_init

32:
    fld.s          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0
    vpermi.w       vr0,   vr0,   0x44

    vxor.v         vr1,   vr1,   vr1   //sum
    vor.v          vr15,  vr4,   vr4   //pri_tap_k

    sub.d          t4,    s4,    a2
    sub.d          t5,    s4,    a3

    fldx.d         f5,    s4,    a2   //p0_00
    fld.d          f6,    t4,    0    //p0_01
    fldx.d         f7,    s4,    a3   //p0_10
    fld.d          f8,    t5,    0    //p0_11

    cdef_process_data_w4 vr9,   vr10
    cdef_calc_sum_tapchange_w4

    vshuf4i.w      vr5,   vr1,   0x0e
    vadd.h         vr1,   vr1,   vr5

    cdef_calc_dst

    vsrlni.b.h     vr5,   vr5,   0
    fst.s          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    32b
    b              35f

33:   // sec_strength only
    cdef_sec_init

34:
    fld.s          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0
    vpermi.w       vr0,   vr0,   0x44

    vxor.v         vr1,   vr1,   vr1   //sum

    sub.d          t4,    s4,    s1  //tmp[-off02]
    sub.d          t5,    s4,    t0  //tmp[-off03]

    fldx.d         f5,    s4,    s1   //s0_00
    fld.d          f6,    t4,    0    //s0_01
    fldx.d         f7,    s4,    t0   //s0_02
    fld.d          f8,    t5,    0    //s0_03

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr22

    sub.d          t4,    s4,    s2  //tmp[-off12]
    sub.d          t5,    s4,    s3  //tmp[-off13]

    fldx.d         f5,    s4,    s2   //s0_10
    fld.d          f6,    t4,    0    //s0_11
    fldx.d         f7,    s4,    s3   //s0_12
    fld.d          f8,    t5,    0    //s0_13

    cdef_process_data_w4 vr18, vr19
    cdef_calc_sum_no_tapchange_w4 vr20

    vshuf4i.w      vr5,   vr1,   0x0e
    vadd.h         vr1,   vr1,   vr5

    cdef_calc_dst

    vsrlni.b.h     vr5,   vr5,   0
    fst.s          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    34b

35:
    ld.d           s0,    sp,    0
    ld.d           s1,    sp,    8
    ld.d           s2,    sp,    16
    ld.d           s3,    sp,    24
    ld.d           s4,    sp,    32
    ld.d           s5,    sp,    40
    ld.d           s6,    sp,    48
    ld.d           s7,    sp,    56
    addi.d         sp,    sp,    (64+288)
endfunc

function cdef_filter_block_8x8_8bpc_lsx
    ld.w           t0,    sp,    0
    ld.w           t1,    sp,    8
    addi.d         sp,    sp,    -(64+288)
    st.d           s0,    sp,    0
    st.d           s1,    sp,    8
    st.d           s2,    sp,    16
    st.d           s3,    sp,    24
    st.d           s4,    sp,    32
    st.d           s5,    sp,    40
    st.d           s6,    sp,    48
    st.d           s7,    sp,    56

    li.w           s0,    8         //w
    li.w           s1,    8         //h
    or             s2,    t1,    t1 //edges
    or             s7,    t0,    t0 //damping

    // cdef_filter_block_kernel
    li.d           s5,    12         //tmp_stride
    addi.d         s4,    sp,    64
    slli.d         t0,    s5,    1
    addi.d         t0,    t0,    2
    slli.d         t0,    t0,    1
    add.d          s4,    s4,    t0  //ptr tmp
    vxor.v         vr23,  vr23,  vr23
    li.w           t2,    1
    vreplgr2vr.h   vr20,  t2
    vaddi.hu       vr21,  vr20,  2
    vaddi.hu       vr22,  vr20,  1

    li.w          t0,     -16384
    vreplgr2vr.h  vr18,   t0

    //padding
    li.w          t5,     -2        //x_start
    addi.d        t6,     s0,    2  //x_end
    li.w          t7,     -2        //y_start
    addi.d        t8,     s1,    2  //y_end
    li.w          t2,     2

    andi          t4,     s2,    4
    bnez          t4,     1f

    //CDEF_HAVE_TOP
    slli.d        t3,     s5,    2
    addi.d        t4,     s4,    -4
    sub.d         t4,     t4,    t3
    addi.d        t3,     s0,    4

    cdef_fill     t4,     s5,    t3,     t2

    or            t7,     zero,  zero

1:  //CDEF_HAVE_BOTTOM
    andi          t4,     s2,8
    bnez          t4,     2f

    mul.w         t3,     s1,    s5
    slli.d        t3,     t3,  1
    add.d         t4,     s4,  t3
    addi.d        t4,     t4,    -4
    li.d          t3,     12

    cdef_fill     t4,     s5,    t3,    t2

    addi.d        t8,     t8,    -2

2:  //CDEF_HAVE_LEFT
    andi          t4,     s2,1
    bnez          t4,     3f

    mul.w         t3,     t7,    s5
    slli.d        t3,     t3,    1
    add.d         t4,     s4,    t3
    addi.d        t4,     t4,    -4
    sub.d         t3,     t8,    t7
    li.d          t2,     2

    cdef_fill     t4,     s5,    t2,    t3

    or            t5,     zero,  zero

3:  //CDEF_HAVE_RIGHT
    andi          t4,     s2,2
    bnez          t4,     40f

    mul.w         t3,     t7,    s5
    slli.d        t3,     t3,    1
    add.d         t4,     s4,    t3
    addi.d        t4,     t4,    16
    sub.d         t3,     t8,    t7
    li.d          t2,     2

    cdef_fill     t4,     s5,    t2,    t3

    addi.d        t6,     t6,    -2

40:
    cdef_padding_data

    beqz           a5,    33f

28:  //if (pri_strength)
    li.w           t0,    4
    andi           t1,    a5,    1
    sub.d          t0,    t0,    t1  //pri_tap

    //edit
    clz.w          t1,    a5
    li.d           t2,    31
    sub.w          t3,    t2,    t1
    sub.w          t3,    s7,    t3

    or             t1,    zero,  zero   //t1: pri_shift
    blt            t3,    zero,  281f
    or             t1,    t3,    t3
281:

    beqz           a6,    31f

29:  //if (sec_strength)
    cdef_pri_sec_init

301:
    fld.d          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0

    vxor.v         vr1,   vr1,   vr1   //sum
    vor.v          vr2,   vr0,   vr0   //max
    vor.v          vr3,   vr0,   vr0   //min
    vor.v          vr15,  vr4,   vr4   //pri_tap_k

    sub.d          t4,    s4,    a2
    sub.d          t5,    s4,    a3

    vldx           vr5,  s4,    a2
    vld            vr6,  t4,    0
    vldx           vr7,  s4,    a3
    vld            vr8,  t5,    0

    cdef_process_data_w8 vr9, vr10
    cdef_calc_sum_tapchange_w8
    cdef_calc_maxmin_w8

    //s 00-03
    sub.d          t4,    s4,    s1  //tmp[-off02]
    sub.d          t5,    s4,    t0  //tmp[-off03]

    vldx           vr5,  s4,    s1
    vld            vr6,  t4,    0
    vldx           vr7,  s4,    t0
    vld            vr8,  t5,    0

    cdef_process_data_w8 vr18, vr19
    cdef_calc_sum_no_tapchange_w8 vr22
    cdef_calc_maxmin_w8

    //s 10-13
    sub.d          t4,    s4,    s2  //tmp[-off12]
    sub.d          t5,    s4,    s3  //tmp[-off13]

    vldx           vr5,  s4,    s2
    vld            vr6,  t4,    0
    vldx           vr7,  s4,    s3
    vld            vr8,  t5,    0

    cdef_process_data_w8 vr18, vr19
    cdef_calc_sum_no_tapchange_w8 vr20

    cdef_calc_maxmin_w8
    cdef_calc_dst

    iclip_vrh       vr5,   vr3,   vr2,  vr16,  vr17,  vr5

    vsrlni.b.h     vr5,   vr5,   0
    fst.d          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    301b
    b              35f

31:  // pri_strength only
    cdef_pri_init

32:
    fld.d          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0

    vxor.v         vr1,   vr1,   vr1   //sum
    vor.v          vr15,  vr4,   vr4   //pri_tap_k

    sub.d          t4,    s4,    a2
    sub.d          t5,    s4,    a3

    vldx           vr5,  s4,    a2
    vld            vr6,  t4,    0
    vldx           vr7,  s4,    a3
    vld            vr8,  t5,    0

    cdef_process_data_w8 vr9, vr10
    cdef_calc_sum_tapchange_w8
    cdef_calc_dst

    vsrlni.b.h     vr5,   vr5,   0
    fst.d          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    32b
    b              35f

33:   // sec_strength only
    cdef_sec_init

34:
    fld.d          f0,    a0,    0     //px
    vsllwil.hu.bu  vr0,   vr0,   0

    vxor.v         vr1,   vr1,   vr1   //sum

    sub.d          t4,    s4,    s1  //tmp[-off02]
    sub.d          t5,    s4,    t0  //tmp[-off03]

    vldx           vr5,  s4,    s1
    vld            vr6,  t4,    0
    vldx           vr7,  s4,    t0
    vld            vr8,  t5,    0

    cdef_process_data_w8 vr18,  vr19
    cdef_calc_sum_no_tapchange_w8 vr22

    sub.d          t4,    s4,    s2  //tmp[-off12]
    sub.d          t5,    s4,    s3  //tmp[-off13]

    vldx           vr5,  s4,    s2
    vld            vr6,  t4,    0
    vldx           vr7,  s4,    s3
    vld            vr8,  t5,    0

    cdef_process_data_w8 vr18,  vr19
    cdef_calc_sum_no_tapchange_w8 vr20
    cdef_calc_dst

    vsrlni.b.h     vr5,   vr5,   0
    fst.d          f5,    a0,    0

    add.d          a0,    a0,    a1
    add.d          s4,    s4,    s5
    add.d          s4,    s4,    s5

    addi.d         t2,    t2,    -1
    blt            zero,  t2,    34b

35:
    ld.d           s0,    sp,    0
    ld.d           s1,    sp,    8
    ld.d           s2,    sp,    16
    ld.d           s3,    sp,    24
    ld.d           s4,    sp,    32
    ld.d           s5,    sp,    40
    ld.d           s6,    sp,    48
    ld.d           s7,    sp,    56
    addi.d         sp,    sp,    (64+288)
endfunc

