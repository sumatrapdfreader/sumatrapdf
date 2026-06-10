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

// depending on how many pixels need to be stored, returns:
// t4 = (1 << 0) : 0 pixels
// t4 = (1 << 4) : inner 4 pixels
// t4 = (1 << 6) : inner 6 pixels
// t4 = 0 : all pixels
.macro FILTER wd
functionl lpf_16_wd\wd\()_lsx
    vabsd.bu         vr0,   vr22,  vr23 // abs(p1 - p0)
    vabsd.bu         vr1,   vr25,  vr24 // abs(q1 - q0)
    vabsd.bu         vr2,   vr23,  vr24 // abs(p0 - q0)
    vabsd.bu         vr3,   vr22,  vr25 // abs(p1 - q1)
.if \wd >= 6
    vabsd.bu         vr4,   vr21,  vr22 // abs(p2 - p1)
    vabsd.bu         vr5,   vr26,  vr25 // abs(q2 - q1)
.endif
.if \wd >= 8
    vabsd.bu         vr6,   vr20,  vr21 // abs(p3 - p2)
    vabsd.bu         vr7,   vr27,  vr26 // abs(q3 - q3)
.endif
.if \wd >= 6
    vmax.bu          vr4,   vr4,   vr5
.endif
    vsadd.bu         vr2,   vr2,   vr2  // abs(p0 - q0) * 2
.if \wd >= 8
    vmax.bu          vr6,   vr6,   vr7
.endif
    vsrli.b          vr3,   vr3,   1    // abs(p1 - q1) >> 1
.if \wd >= 8
    vmax.bu          vr4,   vr4,   vr6
.endif
.if \wd >= 6
    vand.v           vr4,   vr4,   vr14
.endif
    vmax.bu          vr0,   vr0,   vr1  // max(abs(p1 - p0), abs(q1 - q0))
    vsadd.bu         vr2,   vr2,   vr3  // abs(p0 - q0) * 2 + abs(p1 - q1) >> 1
.if \wd >= 6
    vmax.bu          vr4,   vr0,   vr4
    vsle.bu          vr1,   vr4,   vr11 // max(abs(p1 - p0), abs(q1 - q0), abs(), abs(), ...) <= I
.else
    vsle.bu          vr1,   vr0,   vr11 // max(abs(p1 - p0), abs(q1 - q0)) <= I
.endif
    vsle.bu          vr2,   vr2,   vr10 // abs(p0 - q0) * 2 + abs(p1 - q1) >> 1 <= E
    vand.v           vr1,   vr1,   vr2  // fm
    vand.v           vr1,   vr1,   vr13 // fm && wd >= 4
.if \wd >= 6
    vand.v           vr14,  vr14,  vr1  // fm && wd > 4
.endif
.if \wd >= 16
    vand.v           vr15,  vr15,  vr1  // fm && wd == 16
.endif
    vhaddw.qu.du     vr8,   vr1,   vr1
    vpickve2gr.du    t6,    vr8,   0
    bnez             t6,    9f          // if (!fm || wd < 4) return;
    li.w             t4,    1 << 0
    jirl             zero,  ra,    0x00

9:
.if \wd >= 6
    vabsd.bu         vr2,   vr21,  vr23 // abs(p2 - p0)
    vabsd.bu         vr3,   vr22,  vr23 // abs(p1 - p0)
    vabsd.bu         vr4,   vr25,  vr24 // abs(q1 - q0)
    vabsd.bu         vr5,   vr26,  vr24 // abs(q2 - q0)
.if \wd >= 8
    vabsd.bu         vr6,   vr20,  vr23 // abs(p3 - p0)
    vabsd.bu         vr7,   vr27,  vr24 // abs(q3 - q0)
.endif
    vmax.bu          vr2,   vr2,   vr3
    vmax.bu          vr4,   vr4,   vr5
.if \wd >= 8
    vmax.bu          vr6,   vr6,   vr7
.endif
    vmax.bu          vr2,   vr2,   vr4
.if \wd >= 8
    vmax.bu          vr2,   vr2,   vr6
.endif

.if \wd == 16
    vabsd.bu         vr3,   vr17,  vr23 // abs(p6 - p0)
    vabsd.bu         vr4,   vr18,  vr23 // abs(p5 - p0)
    vabsd.bu         vr5,   vr19,  vr23 // abs(p4 - p0)
.endif
    vslei.bu         vr2,   vr2,   1    // flat8in
.if \wd == 16
    vabsd.bu         vr6,   vr28,  vr24 // abs(q4 - q0)
    vabsd.bu         vr7,   vr29,  vr24 // abs(q5 - q0)
    vabsd.bu         vr8,   vr30,  vr24 // abs(q6 - q0)
.endif
    vand.v           vr14,  vr2,   vr14 // flat8in && fm && wd > 4
    vandn.v          vr1,   vr14,  vr1  // fm && wd >= 4 && !flat8in
.if \wd == 16
    vmax.bu          vr3,   vr3,   vr4
    vmax.bu          vr5,   vr5,   vr6
.endif
    vhaddw.qu.du     vr9,   vr1,   vr1
.if \wd == 16
    vmax.bu          vr7,   vr7,   vr8
    vmax.bu          vr3,   vr3,   vr5
    vmax.bu          vr3,   vr3,   vr7
    vslei.bu         vr3,   vr3,   1    // flat8out
.endif
    vpickve2gr.du    t6,    vr9,   0
.if \wd == 16
    vand.v           vr15,  vr15,  vr3  // flat8out && fm && wd == 16
    vand.v           vr15,  vr15,  vr14 // flat8out && flat8in && fm && wd == 16
    vandn.v          vr14,  vr15,  vr14 // flat8in && fm && wd >= 4 && !flat8out
.endif
    beqz             t6,    1f          // skip wd == 4 case
.endif
    vxori.b          vr2,   vr22,  128  // p1 - 128
    vxori.b          vr3,   vr25,  128  // q1 - 128
    vslt.bu          vr0,   vr12,  vr0  // hev
    vssub.b          vr2,   vr2,   vr3  // iclip_diff(p1 - q1)
    vand.v           vr4,   vr2,   vr0  // if (hev) iclip_diff(p1 - q1)
    vandn.v          vr0,   vr0,   vr1  // (fm && wd >= 4 && !hev)
    vxor.v           vr5,   vr5,   vr5
    vaddi.hu         vr5,   vr5,   3
    vsubwev.h.bu     vr2,   vr24,  vr23
    vsubwod.h.bu     vr3,   vr24,  vr23
    vmul.h           vr2,   vr2,   vr5
    vmul.h           vr3,   vr3,   vr5
    vxor.v           vr6,   vr6,   vr6
    vaddwev.h.b      vr7,   vr4,   vr6
    vaddwod.h.b      vr6,   vr4,   vr6
    vadd.h           vr2,   vr2,   vr7
    vadd.h           vr3,   vr3,   vr6
    vssrani.b.h      vr2,   vr2,   0
    vssrani.b.h      vr3,   vr3,   0
    vilvl.b          vr2,   vr3,   vr2 // f
    vxor.v           vr6,   vr6,   vr6
    vaddi.bu         vr5,   vr6,   3
    vaddi.bu         vr6,   vr6,   4   // 4
    vsadd.b          vr4,   vr6,   vr2 // imin(f + 4, 127)
    vsadd.b          vr5,   vr5,   vr2 // imin(f + 3, 127)
    vsrai.b          vr4,   vr4,   3   // f1
    vsrai.b          vr5,   vr5,   3   // f2
    vaddi.bu         vr2,   vr23,  0   // p0
    vaddi.bu         vr3,   vr24,  0   // q0
    vxori.b          vr2,   vr2,   128
    vxori.b          vr3,   vr3,   128
    vsadd.b          vr2,   vr2,   vr5 // p0 + f2 out p0
    vssub.b          vr3,   vr3,   vr4 // q0 - f1 out q0
    vxori.b          vr2,   vr2,   128
    vxori.b          vr3,   vr3,   128
    vsrari.b         vr4,   vr4,   1   // (f1 + 1) >> 1
    vbitsel.v        vr23,  vr23,  vr2,   vr1 // if (fm && wd >= 4)
    vbitsel.v        vr24,  vr24,  vr3,   vr1 // if (fm && wd >= 4)
    vaddi.bu         vr2,   vr22,  0   // p1
    vaddi.bu         vr3,   vr25,  0   // q1
    vxori.b          vr2,   vr2,   128
    vxori.b          vr3,   vr3,   128
    vsadd.b          vr2,   vr2,   vr4 // out p1
    vssub.b          vr3,   vr3,   vr4 // out q1
    vxori.b          vr2,   vr2,   128
    vxori.b          vr3,   vr3,   128
    vbitsel.v        vr22,  vr22,  vr2,   vr0 // if (fm && wd >= 4 && !hev)
    vbitsel.v        vr25,  vr25,  vr3,   vr0 // if (fm && wd >= 4 && !hev)
1:

.if \wd == 6
    vhaddw.qu.du     vr0,   vr14,  vr14
    vpickve2gr.du    t6,    vr0,   0
    beqz             t6,    2f        // skip if there's no flat8in

    vaddwev.h.bu     vr0,   vr21,  vr21
    vaddwod.h.bu     vr1,   vr21,  vr21 // p2 * 2
    vaddwev.h.bu     vr2,   vr21,  vr22
    vaddwod.h.bu     vr3,   vr21,  vr22 // p2 + p1
    vaddwev.h.bu     vr4,   vr22,  vr23
    vaddwod.h.bu     vr5,   vr22,  vr23 // p1 + p0
    vaddwev.h.bu     vr6,   vr23,  vr24
    vaddwod.h.bu     vr7,   vr23,  vr24 // p0 + q0
    vadd.h           vr8,   vr0,   vr2
    vadd.h           vr9,   vr1,   vr3
    vadd.h           vr10,  vr4,   vr6
    vadd.h           vr11,  vr5,   vr7
    vaddwev.h.bu     vr12,  vr24,  vr25
    vaddwod.h.bu     vr13,  vr24,  vr25 // q0 + q1
    vadd.h           vr8,   vr8,   vr10
    vadd.h           vr9,   vr9,   vr11
    vsub.h           vr12,  vr12,  vr0
    vsub.h           vr13,  vr13,  vr1
    vaddwev.h.bu     vr10,  vr25,  vr26
    vaddwod.h.bu     vr11,  vr25,  vr26 // q1 + q2
    vssrlrni.bu.h    vr0,   vr8,   3
    vssrlrni.bu.h    vr1,   vr9,   3
    vilvl.b          vr0,   vr1,   vr0  // out p1

    vadd.h           vr8,   vr8,   vr12
    vadd.h           vr9,   vr9,   vr13
    vsub.h           vr10,  vr10,  vr2
    vsub.h           vr11,  vr11,  vr3
    vaddwev.h.bu     vr12,  vr26,  vr26 // q2 + q2
    vaddwod.h.bu     vr13,  vr26,  vr26
    vssrlrni.bu.h    vr1,   vr8,   3
    vssrlrni.bu.h    vr2,   vr9,   3
    vilvl.b          vr1,   vr2,   vr1  // out p0

    vadd.h           vr8,   vr8,   vr10
    vadd.h           vr9,   vr9,   vr11
    vsub.h           vr12,  vr12,  vr4
    vsub.h           vr13,  vr13,  vr5
    vssrlrni.bu.h    vr2,   vr8,   3
    vssrlrni.bu.h    vr3,   vr9,   3
    vilvl.b          vr2,   vr3,   vr2  // out q0

    vbitsel.v        vr22,  vr22,  vr0,  vr14
    vadd.h           vr8,   vr8,   vr12
    vadd.h           vr9,   vr9,   vr13
    vbitsel.v        vr23,  vr23,  vr1,  vr14
    vssrlrni.bu.h    vr3,   vr8,   3
    vssrlrni.bu.h    vr4,   vr9,   3
    vilvl.b          vr3,   vr4,   vr3
    vbitsel.v        vr24,  vr24,  vr2,  vr14
    vbitsel.v        vr25,  vr25,  vr3,  vr14
.elseif \wd >= 8
    vhaddw.qu.du     vr0,   vr14,  vr14
    vpickve2gr.du    t6,    vr0,   0
.if \wd == 8
    beqz             t6,    8f  // skip if there's no flat8in
.else
    beqz             t6,    2f  // skip if there's no flat8in
.endif

    vaddwev.h.bu     vr0,   vr20,  vr21
    vaddwod.h.bu     vr1,   vr20,  vr21 // p3 + p2
    vaddwev.h.bu     vr2,   vr22,  vr25
    vaddwod.h.bu     vr3,   vr22,  vr25 // p1 + q1
    vaddwev.h.bu     vr4,   vr20,  vr22
    vaddwod.h.bu     vr5,   vr20,  vr22 // p3 + p1
    vaddwev.h.bu     vr6,   vr23,  vr26
    vaddwod.h.bu     vr7,   vr23,  vr26 // p0 + q2
    vadd.h           vr8,   vr0,   vr0
    vadd.h           vr9,   vr1,   vr1  // 2 * (p3 + p2)
    vxor.v           vr10,  vr10,  vr10
    vaddwev.h.bu     vr11,  vr23,  vr10
    vaddwod.h.bu     vr12,  vr23,  vr10
    vaddwev.h.bu     vr13,  vr24,  vr10
    vaddwod.h.bu     vr10,  vr24,  vr10
    vadd.h           vr8,   vr8,   vr11 // + p0
    vadd.h           vr9,   vr9,   vr12
    vadd.h           vr8,   vr8,   vr13 // + q0
    vadd.h           vr9,   vr9,   vr10
    vadd.h           vr8,   vr8,   vr4
    vadd.h           vr9,   vr9,   vr5  // + p3 + p1
    vsub.h           vr2,   vr2,   vr0
    vsub.h           vr3,   vr3,   vr1  // p1 + q1 - p3 - p2
    vsub.h           vr6,   vr6,   vr4
    vsub.h           vr7,   vr7,   vr5  // p0 + q2 - p3 - p1
    vssrlrni.bu.h    vr10,  vr8,   3
    vssrlrni.bu.h    vr11,  vr9,   3
    vilvl.b          vr10,  vr11,  vr10 // out p2

    vadd.h           vr8,   vr8,   vr2
    vadd.h           vr9,   vr9,   vr3
    vaddwev.h.bu     vr0,   vr20,  vr23
    vaddwod.h.bu     vr1,   vr20,  vr23 // p3 + p0
    vaddwev.h.bu     vr2,   vr24,  vr27
    vaddwod.h.bu     vr3,   vr24,  vr27 // q0 + q3
    vssrlrni.bu.h    vr11,  vr8,   3
    vssrlrni.bu.h    vr12,  vr9,   3
    vilvl.b          vr11,  vr12,  vr11 // out p1

    vadd.h           vr8,   vr8,   vr6
    vadd.h           vr9,   vr9,   vr7
    vsub.h           vr2,   vr2,   vr0 // q0 + q3 - p3 - p0
    vsub.h           vr3,   vr3,   vr1
    vaddwev.h.bu     vr4,   vr21,  vr24 // p2 + q0
    vaddwod.h.bu     vr5,   vr21,  vr24
    vaddwev.h.bu     vr6,   vr25,  vr27 // q1 + q3
    vaddwod.h.bu     vr7,   vr25,  vr27
    vssrlrni.bu.h    vr12,  vr8,   3
    vssrlrni.bu.h    vr13,  vr9,   3
    vilvl.b          vr12,  vr13,  vr12 // out p0

    vadd.h           vr8,   vr8,   vr2
    vadd.h           vr9,   vr9,   vr3
    vsub.h           vr6,   vr6,   vr4 // q1 + q3 - p2 - q0
    vsub.h           vr7,   vr7,   vr5
    vaddwev.h.bu     vr0,   vr22,  vr25 // p1 + q1
    vaddwod.h.bu     vr1,   vr22,  vr25
    vaddwev.h.bu     vr2,   vr26,  vr27
    vaddwod.h.bu     vr3,   vr26,  vr27 // q2 + q3
    vssrlrni.bu.h    vr13,  vr8,   3
    vssrlrni.bu.h    vr4,   vr9,   3
    vilvl.b          vr13,  vr4,   vr13 // out q0

    vadd.h           vr8,   vr8,   vr6
    vadd.h           vr9,   vr9,   vr7
    vsub.h           vr2,   vr2,   vr0  // q2 + q3 - p1 - q1
    vsub.h           vr3,   vr3,   vr1
    vssrlrni.bu.h    vr0,   vr8,   3
    vssrlrni.bu.h    vr1,   vr9,   3
    vilvl.b          vr0,   vr1,   vr0 // out q1

    vadd.h           vr8,   vr8,   vr2
    vadd.h           vr9,   vr9,   vr3

    vbitsel.v        vr21,  vr21,  vr10,  vr14
    vbitsel.v        vr22,  vr22,  vr11,  vr14
    vbitsel.v        vr23,  vr23,  vr12,  vr14
    vbitsel.v        vr24,  vr24,  vr13,  vr14
    vssrlrni.bu.h    vr1,   vr8,   3
    vssrlrni.bu.h    vr2,   vr9,   3
    vilvl.b          vr1,   vr2,  vr1 // out q2
    vbitsel.v        vr25,  vr25,  vr0,   vr14
    vbitsel.v        vr26,  vr26,  vr1,   vr14
.endif
2:
.if \wd == 16
    vhaddw.qu.du     vr2,   vr15,  vr15
    vpickve2gr.du    t6,    vr2,   0
    bnez             t6,    1f                 // check if flat8out is needed
    vhaddw.qu.du     vr2,   vr14,  vr14
    vpickve2gr.du    t6,    vr2,   0
    beqz             t6,    8f                 // if there was no flat8in, just write the inner 4 pixels
    b                7f                        // if flat8in was used, write the inner 6 pixels
1:

    vaddwev.h.bu     vr2,   vr17,  vr17  // p6 + p6
    vaddwod.h.bu     vr3,   vr17,  vr17
    vaddwev.h.bu     vr4,   vr17,  vr18
    vaddwod.h.bu     vr5,   vr17,  vr18  // p6 + p5
    vaddwev.h.bu     vr6,   vr17,  vr19
    vaddwod.h.bu     vr7,   vr17,  vr19  // p6 + p4
    vaddwev.h.bu     vr8,   vr17,  vr20
    vaddwod.h.bu     vr9,   vr17,  vr20  // p6 + p3
    vadd.h           vr12,  vr2,   vr4
    vadd.h           vr13,  vr3,   vr5
    vadd.h           vr10,  vr6,   vr8
    vadd.h           vr11,  vr7,   vr9
    vaddwev.h.bu     vr6,   vr17,  vr21
    vaddwod.h.bu     vr7,   vr17,  vr21  // p6 + p2
    vadd.h           vr12,  vr12,  vr10
    vadd.h           vr13,  vr13,  vr11
    vaddwev.h.bu     vr8,   vr17,  vr22
    vaddwod.h.bu     vr9,   vr17,  vr22  // p6 + p1
    vaddwev.h.bu     vr10,  vr18,  vr23
    vaddwod.h.bu     vr11,  vr18,  vr23  // p5 + p0
    vadd.h           vr6,   vr6,   vr8
    vadd.h           vr7,   vr7,   vr9
    vaddwev.h.bu     vr8,   vr19,  vr24
    vaddwod.h.bu     vr9,   vr19,  vr24  // p4 + q0
    vadd.h           vr12,  vr12,  vr6
    vadd.h           vr13,  vr13,  vr7
    vadd.h           vr10,  vr10,  vr8
    vadd.h           vr11,  vr11,  vr9
    vaddwev.h.bu     vr6,   vr20,  vr25
    vaddwod.h.bu     vr7,   vr20,  vr25  // p3 + q1
    vadd.h           vr12,  vr12,  vr10
    vadd.h           vr13,  vr13,  vr11
    vsub.h           vr6,   vr6,   vr2
    vsub.h           vr7,   vr7,   vr3
    vaddwev.h.bu     vr2,   vr21,  vr26
    vaddwod.h.bu     vr3,   vr21,  vr26  // p2 + q2
    vssrlrni.bu.h    vr0,   vr12,  4
    vssrlrni.bu.h    vr1,   vr13,  4
    vilvl.b          vr0,   vr1,   vr0   // out p5
    vadd.h           vr12,  vr12,  vr6
    vadd.h           vr13,  vr13,  vr7   // - (p6 + p6) + (p3 + q1)
    vsub.h           vr2,   vr2,   vr4
    vsub.h           vr3,   vr3,   vr5
    vaddwev.h.bu     vr4,   vr22,  vr27
    vaddwod.h.bu     vr5,   vr22,  vr27  // p1 + q3
    vaddwev.h.bu     vr6,   vr17,  vr19
    vaddwod.h.bu     vr7,   vr17,  vr19  // p6 + p4
    vssrlrni.bu.h    vr1,   vr12,  4
    vssrlrni.bu.h    vr8,   vr13,  4
    vilvl.b          vr1,   vr8,   vr1   // out p4
    vadd.h           vr12,  vr12,  vr2
    vadd.h           vr13,  vr13,  vr3   // - (p6 + p5) + (p2 + q2)
    vsub.h           vr4,   vr4,   vr6
    vsub.h           vr5,   vr5,   vr7
    vaddwev.h.bu     vr6,   vr23,  vr28
    vaddwod.h.bu     vr7,   vr23,  vr28  // p0 + q4
    vaddwev.h.bu     vr8,   vr17,  vr20
    vaddwod.h.bu     vr9,   vr17,  vr20  // p6 + p3
    vssrlrni.bu.h    vr2,   vr12,  4
    vssrlrni.bu.h    vr10,  vr13,  4
    vilvl.b          vr2,   vr10,  vr2   // out p3
    vadd.h           vr12,  vr12,  vr4
    vadd.h           vr13,  vr13,  vr5   // - (p6 + p4) + (p1 + q3)
    vsub.h           vr6,   vr6,   vr8
    vsub.h           vr7,   vr7,   vr9
    vaddwev.h.bu     vr8,   vr24,  vr29
    vaddwod.h.bu     vr9,   vr24,  vr29  // q0 + q5
    vaddwev.h.bu     vr4,   vr17,  vr21
    vaddwod.h.bu     vr5,   vr17,  vr21  // p6 + p2
    vssrlrni.bu.h    vr3,   vr12,  4
    vssrlrni.bu.h    vr11,  vr13,  4
    vilvl.b          vr3,   vr11,  vr3   // out p2
    vadd.h           vr12,  vr12,  vr6
    vadd.h           vr13,  vr13,  vr7   // - (p6 + p3) + (p0 + q4)
    vsub.h           vr8,   vr8,   vr4
    vsub.h           vr9,   vr9,   vr5
    vaddwev.h.bu     vr6,   vr25,  vr30
    vaddwod.h.bu     vr7,   vr25,  vr30  // q1 + q6
    vaddwev.h.bu     vr10,  vr17,  vr22
    vaddwod.h.bu     vr11,  vr17,  vr22  // p6 + p1
    vssrlrni.bu.h    vr4,   vr12,  4
    vssrlrni.bu.h    vr5,   vr13,  4
    vilvl.b          vr4,   vr5,   vr4   // out p1

    vadd.h           vr12,  vr12,  vr8
    vadd.h           vr13,  vr13,  vr9   // - (p6 + p2) + (q0 + q5)
    vsub.h           vr6,   vr6,   vr10
    vsub.h           vr7,   vr7,   vr11
    vaddwev.h.bu     vr8,   vr26,  vr30
    vaddwod.h.bu     vr9,   vr26,  vr30  // q2 + q6
    vbitsel.v        vr0,   vr18,  vr0,  vr15  // out p5
    vaddwev.h.bu     vr10,  vr18,  vr23
    vaddwod.h.bu     vr11,  vr18,  vr23  // p5 + p0
    vssrlrni.bu.h    vr5,   vr12,  4
    vssrlrni.bu.h    vr18,  vr13,  4
    vilvl.b          vr5,   vr18,  vr5   // out p0

    vadd.h           vr12,  vr12,  vr6
    vadd.h           vr13,  vr13,  vr7   // - (p6 + p1) + (q1 + q6)
    vsub.h           vr8,   vr8,   vr10
    vsub.h           vr9,   vr9,   vr11
    vaddwev.h.bu     vr10,  vr27,  vr30
    vaddwod.h.bu     vr11,  vr27,  vr30  // q3 + q6
    vbitsel.v        vr1,   vr19,  vr1,  vr15  // out p4

    vaddwev.h.bu     vr18,  vr19,  vr24
    vaddwod.h.bu     vr19,  vr19,  vr24  // p4 + q0
    vssrlrni.bu.h    vr6,   vr12,  4
    vssrlrni.bu.h    vr7,   vr13,  4
    vilvl.b          vr6,   vr7,   vr6   // out q0

    vadd.h           vr12,  vr12,  vr8
    vadd.h           vr13,  vr13,  vr9   // - (p5 + p0) + (q2 + q6)
    vsub.h           vr10,  vr10,  vr18
    vsub.h           vr11,  vr11,  vr19
    vaddwev.h.bu     vr8,   vr28,  vr30
    vaddwod.h.bu     vr9,   vr28,  vr30  // q4 + q6
    vbitsel.v        vr2,   vr20,  vr2,  vr15  // out p3
    vaddwev.h.bu     vr18,  vr20,  vr25
    vaddwod.h.bu     vr19,  vr20,  vr25  // p3 + q1
    vssrlrni.bu.h    vr7,   vr12,  4
    vssrlrni.bu.h    vr20,  vr13,  4
    vilvl.b          vr7,   vr20,  vr7   // out q1

    vadd.h           vr12,  vr12,  vr10
    vadd.h           vr13,  vr13,  vr11  // - (p4 + q0) + (q3 + q6)
    vsub.h           vr18,  vr8,   vr18
    vsub.h           vr19,  vr9,   vr19
    vaddwev.h.bu     vr10,  vr29,  vr30
    vaddwod.h.bu     vr11,  vr29,  vr30  // q5 + q6
    vbitsel.v        vr3,   vr21,  vr3,  vr15  // out p2
    vaddwev.h.bu     vr20,  vr21,  vr26
    vaddwod.h.bu     vr21,  vr21,  vr26  // p2 + q2
    vssrlrni.bu.h    vr8,   vr12,  4
    vssrlrni.bu.h    vr9,   vr13,  4
    vilvl.b          vr8,   vr9,   vr8   // out q2

    vadd.h           vr12,  vr12,  vr18
    vadd.h           vr13,  vr13,  vr19  // - (p3 + q1) + (q4 + q6)
    vsub.h           vr10,  vr10,  vr20
    vsub.h           vr11,  vr11,  vr21
    vaddwev.h.bu     vr18,  vr30,  vr30
    vaddwod.h.bu     vr19,  vr30,  vr30  // q6 + q6
    vbitsel.v        vr4,   vr22,  vr4,  vr15  // out p1
    vaddwev.h.bu     vr20,  vr22,  vr27
    vaddwod.h.bu     vr21,  vr22,  vr27  // p1 + q3
    vssrlrni.bu.h    vr9,   vr12,  4
    vssrlrni.bu.h    vr22,  vr13,  4
    vilvl.b          vr9,   vr22,  vr9   // out q3
    vadd.h           vr12,  vr12,  vr10
    vadd.h           vr13,  vr13,  vr11  // - (p2 + q2) + (q5 + q6)
    vsub.h           vr18,  vr18,  vr20
    vsub.h           vr19,  vr19,  vr21
    vbitsel.v        vr5,   vr23,  vr5,  vr15  // out p0
    vssrlrni.bu.h    vr10,  vr12,  4
    vssrlrni.bu.h    vr23,  vr13,  4
    vilvl.b          vr10,  vr23,  vr10  // out q4
    vadd.h           vr12,  vr12,  vr18
    vadd.h           vr13,  vr13,  vr19  // - (p1 + q3) + (q6 + q6)
    vssrlrni.bu.h    vr11,  vr12,  4
    vssrlrni.bu.h    vr12,  vr13,  4
    vilvl.b          vr11,  vr12,  vr11  // out q5
    vbitsel.v        vr6,   vr24,  vr6,  vr15
    vbitsel.v        vr7,   vr25,  vr7,  vr15
    vbitsel.v        vr8,   vr26,  vr8,  vr15
    vbitsel.v        vr9,   vr27,  vr9,  vr15
    vbitsel.v        vr10,  vr28,  vr10, vr15
    vbitsel.v        vr11,  vr29,  vr11, vr15
.endif
    li.w             t4,    0
    jirl             zero,  ra,    0x00
.if \wd == 16
7:
    // Return to a shorter epilogue, writing only the inner 6 pixels
    li.w             t4,    1 << 6
    jirl             zero,  ra,    0x00
.endif
.if \wd >= 8
8:
    // Return to a shorter epilogue, writing only the inner 4 pixels
    li.w             t4,    1 << 4
    jirl             zero,  ra,    0x00
.endif
endfuncl
.endm

FILTER 16
FILTER 8
FILTER 6
FILTER 4

.macro LPF_16_WD16
    move             t7,    ra
    bl               lpf_16_wd16_lsx
    move             ra,    t7
    beqz             t4,    1f
    andi             t5,    t4,    1 << 6
    bnez             t5,    7f
    andi             t5,    t4,    1 << 4
    bnez             t5,    8f
    jirl             zero,  ra,    0x00
1:
.endm

.macro LPF_16_WD8
    move             t7,    ra
    bl               lpf_16_wd8_lsx
    move             ra,    t7
    beqz             t4,    1f
    andi             t5,    t4,    1 << 4
    bnez             t5,    8f
    jirl             zero,  ra,    0x00
1:
.endm

.macro LPF_16_WD6
    move             t7,    ra
    bl               lpf_16_wd6_lsx
    move             ra,    t7
    beqz             t4,    1f
    jirl             zero,  ra,    0x00
1:
.endm

.macro LPF_16_WD4
    move             t7,    ra
    bl               lpf_16_wd4_lsx
    move             ra,    t7
    beqz             t4,    1f
    jirl             zero,  ra,    0x00
1:
.endm

functionl lpf_v_4_16_lsx
    slli.d           t3,    a1,    1
    sub.d            t3,    a0,    t3
    vld              vr22,  t3,    0   // p1
    vldx             vr23,  t3,    a1  // p0
    vld              vr24,  a0,    0   // q0
    vldx             vr25,  a0,    a1  // q1

    LPF_16_WD4

    vst              vr22,  t3,    0   // p1
    vstx             vr23,  t3,    a1  // p0
    vst              vr24,  a0,    0   // q0
    vstx             vr25,  a0,    a1  // q1
endfuncl

functionl lpf_h_4_16_lsx
    addi.d           t3,    a0,   -2
    fld.s            f22,   t3,    0
    fldx.s           f23,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.s            f24,   t3,    0
    fldx.s           f25,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.s            f17,   t3,    0
    fldx.s           f18,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.s            f19,   t3,    0
    fldx.s           f20,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vilvl.w          vr22,  vr17,  vr22
    vilvl.w          vr23,  vr18,  vr23
    vilvl.w          vr24,  vr19,  vr24
    vilvl.w          vr25,  vr20,  vr25
    fld.s            f17,   t3,    0
    fldx.s           f18,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.s            f19,   t3,    0
    fldx.s           f20,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.s            f26,   t3,    0
    fldx.s           f27,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.s            f28,   t3,    0
    fldx.s           f29,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vilvl.w          vr17,  vr26,  vr17
    vilvl.w          vr18,  vr27,  vr18
    vilvl.w          vr19,  vr28,  vr19
    vilvl.w          vr20,  vr29,  vr20
    vilvl.d          vr22,  vr17,  vr22
    vilvl.d          vr23,  vr18,  vr23
    vilvl.d          vr24,  vr19,  vr24
    vilvl.d          vr25,  vr20,  vr25
    addi.d           a0,    t3,    2

    TRANSPOSE_4x16B  vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    LPF_16_WD4

    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_4x16B  vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    addi.d           a0,    a0,   -2
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    0
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    1
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    2
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    3
    add.d            a0,    a0,    a1
.endr
    addi.d           a0,    a0,    2
endfuncl

functionl lpf_v_6_16_lsx
    slli.d           t3,    a1,    1
    sub.d            t3,    a0,    t3
    sub.d            s0,    t3,    a1
    vld              vr21,  s0,    0   // p2
    vldx             vr22,  s0,    a1  // p1
    alsl.d           s0,    a1,    s0,   1
    vld              vr23,  s0,    0   // p0
    vldx             vr24,  s0,    a1  // q0
    alsl.d           s0,    a1,    s0,   1
    vld              vr25,  s0,    0   // q1
    vldx             vr26,  s0,    a1  // q2

    LPF_16_WD6

    vst              vr22,  t3,    0   // p1
    vstx             vr23,  t3,    a1  // p0
    vst              vr24,  a0,    0   // q0
    vstx             vr25,  a0,    a1  // q1
endfuncl

functionl lpf_h_6_16_lsx
    addi.d           t3,    a0,   -4
    fld.d            f20,   t3,    0
    fldx.d           f21,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f22,   t3,    0
    fldx.d           f23,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f24,   t3,    0
    fldx.d           f25,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f26,   t3,    0
    fldx.d           f27,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f16,   t3,    0
    fldx.d           f17,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f18,   t3,    0
    fldx.d           f19,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f28,   t3,    0
    fldx.d           f29,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f30,   t3,    0
    fldx.d           f31,   t3,    a1
    alsl.d           t3,    a1,    t3,   1

    vilvl.d          vr20,  vr16,  vr20
    vilvl.d          vr21,  vr17,  vr21
    vilvl.d          vr22,  vr18,  vr22
    vilvl.d          vr23,  vr19,  vr23
    vilvl.d          vr24,  vr28,  vr24
    vilvl.d          vr25,  vr29,  vr25
    vilvl.d          vr26,  vr30,  vr26
    vilvl.d          vr27,  vr31,  vr27
    addi.d           a0,    t3,    4

    TRANSPOSE_8x16B vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    LPF_16_WD6

    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_4x16B vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    addi.d           a0,    a0,   -2
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    0
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    1
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    2
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    3
    add.d            a0,    a0,    a1
.endr
    addi.d           a0,    a0,    2
endfuncl

functionl lpf_v_8_16_lsx
    slli.d           t3,    a1,    2
    sub.d            s0,    a0,    t3
    vld              vr20,  s0,    0   // p3
    vldx             vr21,  s0,    a1  // p2
    alsl.d           s0,    a1,    s0,   1
    vld              vr22,  s0,    0   // p1
    vldx             vr23,  s0,    a1  // p0
    alsl.d           s0,    a1,    s0,   1
    vld              vr24,  s0,    0   // q0
    vldx             vr25,  s0,    a1  // q1
    alsl.d           s0,    a1,    s0,   1
    vld              vr26,  s0,    0   // q2
    vldx             vr27,  s0,    a1  // q3

    LPF_16_WD8

    sub.d            t3,    a0,    t3
    add.d            t3,    t3,    a1  // -3
    vst              vr21,  t3,    0   // p2
    vstx             vr22,  t3,    a1  // p1
    alsl.d           t3,    a1,    t3,   1
    vst              vr23,  t3,    0   // p0
    vstx             vr24,  t3,    a1  // q0
    alsl.d           t3,    a1,    t3,   1
    vst              vr25,  t3,    0   // q1
    vstx             vr26,  t3,    a1  // q2
    jirl             zero,  ra,    0x00
8:
    slli.d           t3,    a1,    1
    sub.d            t3,    a0,    t3
    vst              vr22,  t3,    0   // p1
    vstx             vr23,  t3,    a1  // p0
    alsl.d           t3,    a1,    t3,   1
    vst              vr24,  t3,    0   // q0
    vstx             vr25,  t3,    a1  // q1
endfuncl

functionl lpf_h_8_16_lsx
    addi.d           t3,    a0,   -4
    fld.d            f20,   t3,    0
    fldx.d           f21,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f22,   t3,    0
    fldx.d           f23,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f24,   t3,    0
    fldx.d           f25,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f26,   t3,    0
    fldx.d           f27,   t3,    a1

    alsl.d           t3,    a1,    t3,   1
    fld.d            f16,   t3,    0
    fldx.d           f17,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f18,   t3,    0
    fldx.d           f19,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f28,   t3,    0
    fldx.d           f29,   t3,    a1
    alsl.d           t3,    a1,    t3,   1
    fld.d            f30,   t3,    0
    fldx.d           f31,   t3,    a1
    alsl.d           t3,    a1,    t3,   1

    vilvl.d          vr20,  vr16,  vr20
    vilvl.d          vr21,  vr17,  vr21
    vilvl.d          vr22,  vr18,  vr22
    vilvl.d          vr23,  vr19,  vr23
    vilvl.d          vr24,  vr28,  vr24
    vilvl.d          vr25,  vr29,  vr25
    vilvl.d          vr26,  vr30,  vr26
    vilvl.d          vr27,  vr31,  vr27
    addi.d           a0,    t3,    4

    TRANSPOSE_8x16B vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    LPF_16_WD8

    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_8x16B vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    addi.d           a0,    a0,   -4
.irp i, vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27
    vstelm.d         \i,    a0,    0,    0
    add.d            a0,    a0,    a1
.endr
.irp i, vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27
    vstelm.d         \i,    a0,    0,    1
    add.d            a0,    a0,    a1
.endr
    addi.d           a0,    a0,    4
    jirl             zero,  ra,    0x00

8:
    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_4x16B vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    addi.d           a0,    a0,   -2
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    0
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    1
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    2
    add.d            a0,    a0,    a1
.endr
.irp i, vr22, vr23, vr24, vr25
    vstelm.w         \i,    a0,    0,    3
    add.d            a0,    a0,    a1
.endr
    addi.d           a0,    a0,    2
endfuncl

functionl lpf_v_16_16_lsx
    slli.d           t3,    a1,    3
    sub.d            s0,    a0,    t3
    add.d            s0,    s0,    a1
    vld              vr17,  s0,    0        // p6
    vldx             vr18,  s0,    a1       // p5
    alsl.d           s0,    a1,    s0,   1
    vld              vr19,  s0,    0        // p4
    vldx             vr20,  s0,    a1       // p3
    alsl.d           s0,    a1,    s0,   1
    vld              vr21,  s0,    0        // p2
    vldx             vr22,  s0,    a1       // p1
    alsl.d           s0,    a1,    s0,   1
    vld              vr23,  s0,    0        // p0
    vldx             vr24,  s0,    a1       // q0
    alsl.d           s0,    a1,    s0,   1
    vld              vr25,  s0,    0        // q1
    vldx             vr26,  s0,    a1       // q2
    alsl.d           s0,    a1,    s0,   1
    vld              vr27,  s0,    0        // q3
    vldx             vr28,  s0,    a1       // q4
    alsl.d           s0,    a1,    s0,   1
    vld              vr29,  s0,    0        // q5
    vldx             vr30,  s0,    a1       // q6

    LPF_16_WD16

    sub.d            s0,    a0,    t3
    alsl.d           s0,    a1,    s0,   1
    vst              vr0,   s0,    0        // p5
    vstx             vr1,   s0,    a1       // p4
    alsl.d           s0,    a1,    s0,   1
    vst              vr2,   s0,    0        // p3
    vstx             vr3,   s0,    a1       // p2
    alsl.d           s0,    a1,    s0,   1
    vst              vr4,   s0,    0        // p1
    vstx             vr5,   s0,    a1       // p0
    alsl.d           s0,    a1,    s0,   1
    vst              vr6,   s0,    0        // q0
    vstx             vr7,   s0,    a1       // q1
    alsl.d           s0,    a1,    s0,   1
    vst              vr8,   s0,    0        // q2
    vstx             vr9,   s0,    a1       // q3
    alsl.d           s0,    a1,    s0,   1
    vst              vr10,  s0,    0        // q4
    vstx             vr11,  s0,    a1       // q5
    jirl             zero,  ra,    0x00
7:
    slli.d           t3,    a1,    1
    add.d            t3,    t3,    a1
    sub.d            s0,    a0,    t3
    vst              vr21,  s0,    0        // p2
    vstx             vr22,  s0,    a1       // p1
    alsl.d           s0,    a1,    s0,   1
    vst              vr23,  s0,    0        // p0
    vstx             vr24,  s0,    a1       // q0
    alsl.d           s0,    a1,    s0,   1
    vst              vr25,  s0,    0        // q1
    vstx             vr26,  s0,    a1       // q2
    jirl             zero,  ra,    0x00
8:
    slli.d           t3,    a1,    1
    sub.d            s0,    a0,    t3
    vst              vr22,  s0,    0        // p1
    vstx             vr23,  s0,    a1       // p0
    alsl.d           s0,    a1,    s0,   1
    vst              vr24,  s0,    0        // q0
    vstx             vr25,  s0,    a1       // q1
endfuncl

functionl lpf_h_16_16_lsx
    addi.d           t3,    a0,   -8
    vld              vr16,  t3,    0
    vldx             vr17,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr18,  t3,    0
    vldx             vr19,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr20,  t3,    0
    vldx             vr21,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr22,  t3,    0
    vldx             vr23,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr24,  t3,    0
    vldx             vr25,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr26,  t3,    0
    vldx             vr27,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr28,  t3,    0
    vldx             vr29,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
    vld              vr30,  t3,    0
    vldx             vr31,  t3,    a1
    alsl.d           t3,    a1,    t3,   1
.macro SWAPD in0, in1
    vaddi.bu         vr0,   \in0,  0
    vilvl.d          \in0,  \in1,  \in0
    vilvh.d          \in1,  \in1,  vr0
.endm
    SWAPD            vr16,  vr24
    SWAPD            vr17,  vr25
    SWAPD            vr18,  vr26
    SWAPD            vr19,  vr27
    SWAPD            vr20,  vr28
    SWAPD            vr21,  vr29
    SWAPD            vr22,  vr30
    SWAPD            vr23,  vr31
    addi.d           a0,    t3,    8

    TRANSPOSE_8x16B vr16, vr17, vr18, vr19, vr20, vr21, vr22, vr23, vr0, vr1
    TRANSPOSE_8x16B vr24, vr25, vr26, vr27, vr28, vr29, vr30, vr31, vr0, vr1

    LPF_16_WD16

    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_8x16B vr16, vr17, vr0,  vr1,  vr2,  vr3,  vr4,  vr5,  vr18, vr19
    TRANSPOSE_8x16B vr6,  vr7,  vr8,  vr9,  vr10, vr11, vr30, vr31, vr18, vr19

    addi.d           t3,    a0,   -8
.irp i, vr16, vr17, vr0, vr1, vr2, vr3, vr4, vr5
    vstelm.d         \i,    t3,    0,    0
    add.d            t3,    t3,    a1
.endr
.irp i, vr16, vr17, vr0, vr1, vr2, vr3, vr4, vr5
    vstelm.d         \i,    t3,    0,    1
    add.d            t3,    t3,    a1
.endr
.irp i, vr6, vr7, vr8, vr9, vr10, vr11, vr30, vr31
    vstelm.d         \i,    a0,    0,    0
    add.d            a0,    a0,    a1
.endr
.irp i, vr6, vr7, vr8, vr9, vr10, vr11, vr30, vr31
    vstelm.d         \i,    a0,    0,    1
    add.d            a0,    a0,    a1
.endr
    jirl             zero,  ra,    0x00

7:
    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_8x16B vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    addi.d           a0,    a0,   -4
.irp i, vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27
    vstelm.d         \i,    a0,    0,    0
    add.d            a0,    a0,    a1
.endr
.irp i, vr20, vr21, vr22, vr23, vr24, vr25, vr26, vr27
    vstelm.d         \i,    a0,    0,    1
    add.d            a0,    a0,    a1
.endr
    addi.d           a0,    a0,    4
    jirl             zero,  ra,    0x00
8:

    slli.d           t3,    a1,    4
    sub.d            a0,    a0,    t3

    TRANSPOSE_4x16B vr22, vr23, vr24, vr25, vr26, vr27, vr28, vr29

    addi.d           a0,    a0,   -2
.irp i, 0, 1, 2, 3
    vstelm.w         vr22,  a0,    0,    \i
    add.d            a0,    a0,    a1
    vstelm.w         vr23,  a0,    0,    \i
    add.d            a0,    a0,    a1
    vstelm.w         vr24,  a0,    0,    \i
    add.d            a0,    a0,    a1
    vstelm.w         vr25,  a0,    0,    \i
    add.d            a0,    a0,    a1
.endr
    addi.d           a0,    a0,    2
endfuncl

.macro PUSH_REG
    addi.d           sp,    sp,   -64-8
    fst.d            f24,   sp,    0
    fst.d            f25,   sp,    8
    fst.d            f26,   sp,    16
    fst.d            f27,   sp,    24
    fst.d            f28,   sp,    32
    fst.d            f29,   sp,    40
    fst.d            f30,   sp,    48
    fst.d            f31,   sp,    56
    st.d             s0,    sp,    64
.endm
.macro POP_REG
    fld.d            f24,   sp,    0
    fld.d            f25,   sp,    8
    fld.d            f26,   sp,    16
    fld.d            f27,   sp,    24
    fld.d            f28,   sp,    32
    fld.d            f29,   sp,    40
    fld.d            f30,   sp,    48
    fld.d            f31,   sp,    56
    ld.d             s0,    sp,    64
    addi.d           sp,    sp,    64+8
.endm

const mask_1248
.word 1, 2, 4, 8
endconst

.macro LPF_FUNC DIR, TYPE
function lpf_\DIR\()_sb_\TYPE\()_8bpc_lsx
    PUSH_REG
    move             t8,    ra
    vld              vr0,   a2,    0 //vmask
    vpickve2gr.wu    t0,    vr0,   0
    vpickve2gr.wu    t1,    vr0,   1
.ifc \TYPE, y
    vpickve2gr.wu    t2,    vr0,   2
.endif
    addi.d           a5,    a5,    128 // Move to sharp part of lut
.ifc \TYPE, y
    or               t1,    t1,    t2 // vmask[1] |= vmaks[2]
.endif
    slli.d           a4,    a4,    2
.ifc \DIR, v
    sub.d            a4,    a3,    a4
.else
    addi.d           a3,    a3,   -4
.endif
    or               t0,    t0,    t1 // vmaks[0] |= vmask[1]
1:
    andi             t3,    t0,    0x0f
.ifc \DIR, v
    vld              vr0,   a4,    0 // l[-b4_stride][]
    addi.d           a4,    a4,    16
    vld              vr1,   a3,    0 // l[0][]
    addi.d           a3,    a3,    16
.else
    fld.d            f0,    a3,    0
    fldx.d           f1,    a3,    a4
    alsl.d           a3,    a4,    a3,   1
    fld.d            f2,    a3,    0
    fldx.d           f3,    a3,    a4
    alsl.d           a3,    a4,    a3,   1
    vilvl.w          vr1,   vr1,   vr0
    vilvl.w          vr2,   vr3,   vr2
    vilvl.d          vr0,   vr2,   vr1
    vilvh.d          vr1,   vr2,   vr1
.endif
    beqz             t3,    7f
    //l[0][] ? l[0][] : l[-b4_stride][]
    vseqi.b          vr2,   vr1,    0
    vbitsel.v        vr1,   vr1,   vr0,   vr2
    li.w             t3,    0xff
    vreplgr2vr.w     vr3,   t3
    vand.v           vr1,   vr1,   vr3
    vshuf4i.b        vr1,   vr1,   0x00 // L --       1  0  2   0
    vseqi.w          vr2,   vr1,   0    //            0 -1  0  -1
    vseqi.w          vr2,   vr2,   0    // L != 0 -- -1  0 -1   0
    vhaddw.qu.du     vr3,   vr2,   vr2
    vpickve2gr.du    t4,    vr3,   0
    beqz             t4,    7f          // if (!L) continue
    la.local         t3,    mask_1248   // bits x
    vld              vr16,  t3,    0
    vreplgr2vr.w     vr13,  t0          // vmask[0]
    vreplgr2vr.w     vr14,  t1          // vmaks[1]
    vand.v           vr13,  vr13,  vr16
    vseqi.w          vr13,  vr13,  0
    vseqi.w          vr13,  vr13,  0    // if (vmask[0] & x)
    vand.v           vr13,  vr13,  vr2  // vmask[0] &= L != 0
    vand.v           vr14,  vr14,  vr16
    vseqi.w          vr14,  vr14,  0
    vseqi.w          vr14,  vr14,  0    // if (vmask[1] & x)
.ifc \TYPE, y
    vreplgr2vr.w     vr15,  t2          // vmask[2]
    vand.v           vr15,  vr15,  vr16
    vseqi.w          vr15,  vr15,  0
    vseqi.w          vr15,  vr15,  0    // if (vmask[2] & x)
.endif
    vldrepl.b        vr5,   a5,    0    // sharp[0]
    addi.d           t5,    a5,    8
    vldrepl.b        vr6,   t5,    0    // sharp[1]
    vsrl.b           vr3,   vr1,   vr5  // L >> sharp[0]
    vsrli.b          vr12,  vr1,   4    // H
    vmin.bu          vr3,   vr3,   vr6  // imin(L >> sharp[0], sharp[1])
    vaddi.bu         vr0,   vr1,   2    // L + 2
    vmaxi.bu         vr11,  vr3,   1    // imax(imin(), 1) = limit = I
    vslli.b          vr0,   vr0,   1    // 2*(L + 2)
    vadd.b           vr10,  vr0,   vr11 // 2*(L + 2) + limit = E
.ifc \TYPE, y
    andi             t3,    t2,    0x0f
    beqz             t3,    2f
    //wd16
    bl               lpf_\DIR\()_16_16_lsx
    b                8f
2:
.endif
    andi             t3,    t1,    0x0f
    beqz             t3,    3f
.ifc \TYPE, y
    // wd8
    bl               lpf_\DIR\()_8_16_lsx
.else
    // wd6
    bl               lpf_\DIR\()_6_16_lsx
.endif
    b                8f
3:
    // wd4
    bl               lpf_\DIR\()_4_16_lsx
.ifc \DIR, h
    b                8f
7:
    // For dir h, the functions above increment a0.
    // If the whole function is skipped, increment it here instead.
    alsl.d           a0,    a1,    a0,    4
.else
7:
.endif
8:
    srli.d           t0,    t0,    4
    srli.d           t1,    t1,    4
.ifc \TYPE, y
    srli.d           t2,    t2,    4
.endif
.ifc \DIR, v
    addi.d           a0,    a0,    16
.else
    // For dir h, a0 is returned incremented
.endif
    bnez             t0,    1b
    move             ra,    t8
    POP_REG
endfunc
.endm

LPF_FUNC h, y
LPF_FUNC v, y
LPF_FUNC h, uv
LPF_FUNC v, uv
