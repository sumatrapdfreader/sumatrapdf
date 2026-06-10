/*********************************************************************
 * Copyright (c) 2022 Loongson Technology Corporation Limited
 * Contributed by Gu Xiwei(guxiwei-hf@loongson.cn)
 *                Shiyou Yin(yinshiyou-hf@loongson.cn)
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *********************************************************************/

/*
 * This file is a LoongArch assembly helper file and available under ISC
 * license. It provides a large number of macros and alias to simplify
 * writing assembly code, especially for LSX and LASX optimizations.
 *
 * Any one can modify it or add new features for his/her own purposes.
 * Contributing a patch will be appreciated as it might be useful for
 * others as well. Send patches to loongson contributor mentioned above.
 *
 * MAJOR version: Usage changes, incompatible with previous version.
 * MINOR version: Add new macros/functions, or bug fixes.
 * MICRO version: Comment changes or implementation changes.
 */

#define LML_VERSION_MAJOR 0
#define LML_VERSION_MINOR 4
#define LML_VERSION_MICRO 0

#define DEFAULT_ALIGN    5

/* Set prefix as needed. */
#ifndef PRIVATE_PREFIX
#define PRIVATE_PREFIX dav1d_
#endif

#define PASTE(a,b) a ## b
#define CONCAT(a,b) PASTE(a,b)

#ifdef PREFIX
#define ASM_PREF CONCAT(_,PRIVATE_PREFIX)
#else
#define ASM_PREF PRIVATE_PREFIX
#endif

.macro function name, align=DEFAULT_ALIGN
.macro endfunc
    jirl    $r0, $r1, 0x0
    .size ASM_PREF\name, . - ASM_PREF\name
    .purgem endfunc
.endm
.text ;
.align \align ;
.globl ASM_PREF\name ;
.hidden ASM_PREF\name ;
.type  ASM_PREF\name, @function ;
ASM_PREF\name: ;
.endm

.macro  const name, align=DEFAULT_ALIGN
    .macro endconst
    .size  \name, . - \name
    .purgem endconst
    .endm
.section .rodata
.align   \align
\name:
.endm

/*
 *============================================================================
 * LoongArch register alias
 *============================================================================
 */

#define a0 $a0
#define a1 $a1
#define a2 $a2
#define a3 $a3
#define a4 $a4
#define a5 $a5
#define a6 $a6
#define a7 $a7

#define t0 $t0
#define t1 $t1
#define t2 $t2
#define t3 $t3
#define t4 $t4
#define t5 $t5
#define t6 $t6
#define t7 $t7
#define t8 $t8

#define s0 $s0
#define s1 $s1
#define s2 $s2
#define s3 $s3
#define s4 $s4
#define s5 $s5
#define s6 $s6
#define s7 $s7
#define s8 $s8

#define zero $zero
#define sp   $sp
#define ra   $ra

#define fa0  $fa0
#define fa1  $fa1
#define fa2  $fa2
#define fa3  $fa3
#define fa4  $fa4
#define fa5  $fa5
#define fa6  $fa6
#define fa7  $fa7
#define ft0  $ft0
#define ft1  $ft1
#define ft2  $ft2
#define ft3  $ft3
#define ft4  $ft4
#define ft5  $ft5
#define ft6  $ft6
#define ft7  $ft7
#define ft8  $ft8
#define ft9  $ft9
#define ft10 $ft10
#define ft11 $ft11
#define ft12 $ft12
#define ft13 $ft13
#define ft14 $ft14
#define ft15 $ft15
#define fs0  $fs0
#define fs1  $fs1
#define fs2  $fs2
#define fs3  $fs3
#define fs4  $fs4
#define fs5  $fs5
#define fs6  $fs6
#define fs7  $fs7

#define f0  $f0
#define f1  $f1
#define f2  $f2
#define f3  $f3
#define f4  $f4
#define f5  $f5
#define f6  $f6
#define f7  $f7
#define f8  $f8
#define f9  $f9
#define f10 $f10
#define f11 $f11
#define f12 $f12
#define f13 $f13
#define f14 $f14
#define f15 $f15
#define f16 $f16
#define f17 $f17
#define f18 $f18
#define f19 $f19
#define f20 $f20
#define f21 $f21
#define f22 $f22
#define f23 $f23
#define f24 $f24
#define f25 $f25
#define f26 $f26
#define f27 $f27
#define f28 $f28
#define f29 $f29
#define f30 $f30
#define f31 $f31

#define vr0 $vr0
#define vr1 $vr1
#define vr2 $vr2
#define vr3 $vr3
#define vr4 $vr4
#define vr5 $vr5
#define vr6 $vr6
#define vr7 $vr7
#define vr8 $vr8
#define vr9 $vr9
#define vr10 $vr10
#define vr11 $vr11
#define vr12 $vr12
#define vr13 $vr13
#define vr14 $vr14
#define vr15 $vr15
#define vr16 $vr16
#define vr17 $vr17
#define vr18 $vr18
#define vr19 $vr19
#define vr20 $vr20
#define vr21 $vr21
#define vr22 $vr22
#define vr23 $vr23
#define vr24 $vr24
#define vr25 $vr25
#define vr26 $vr26
#define vr27 $vr27
#define vr28 $vr28
#define vr29 $vr29
#define vr30 $vr30
#define vr31 $vr31

#define xr0 $xr0
#define xr1 $xr1
#define xr2 $xr2
#define xr3 $xr3
#define xr4 $xr4
#define xr5 $xr5
#define xr6 $xr6
#define xr7 $xr7
#define xr8 $xr8
#define xr9 $xr9
#define xr10 $xr10
#define xr11 $xr11
#define xr12 $xr12
#define xr13 $xr13
#define xr14 $xr14
#define xr15 $xr15
#define xr16 $xr16
#define xr17 $xr17
#define xr18 $xr18
#define xr19 $xr19
#define xr20 $xr20
#define xr21 $xr21
#define xr22 $xr22
#define xr23 $xr23
#define xr24 $xr24
#define xr25 $xr25
#define xr26 $xr26
#define xr27 $xr27
#define xr28 $xr28
#define xr29 $xr29
#define xr30 $xr30
#define xr31 $xr31

/*
 *============================================================================
 * LSX/LASX synthesize instructions
 *============================================================================
 */

/*
 * Description : Dot product of byte vector elements
 * Arguments   : Inputs  - vj, vk
 *               Outputs - vd
 *               Return Type - halfword
 */
.macro vdp2.h.bu vd, vj, vk
    vmulwev.h.bu      \vd,    \vj,    \vk
    vmaddwod.h.bu     \vd,    \vj,    \vk
.endm

.macro vdp2.h.bu.b vd, vj, vk
    vmulwev.h.bu.b    \vd,    \vj,    \vk
    vmaddwod.h.bu.b   \vd,    \vj,    \vk
.endm

.macro vdp2.w.h vd, vj, vk
    vmulwev.w.h       \vd,    \vj,    \vk
    vmaddwod.w.h      \vd,    \vj,    \vk
.endm

.macro xvdp2.h.bu xd, xj, xk
    xvmulwev.h.bu    \xd,    \xj,    \xk
    xvmaddwod.h.bu   \xd,    \xj,    \xk
.endm

.macro xvdp2.h.bu.b xd, xj, xk
    xvmulwev.h.bu.b    \xd,  \xj,    \xk
    xvmaddwod.h.bu.b   \xd,  \xj,    \xk
.endm

.macro xvdp2.w.h xd, xj, xk
    xvmulwev.w.h       \xd,  \xj,    \xk
    xvmaddwod.w.h      \xd,  \xj,    \xk
.endm

/*
 * Description : Dot product & addition of halfword vector elements
 * Arguments   : Inputs  - vj, vk
 *               Outputs - vd
 *               Return Type - twice size of input
 */
.macro vdp2add.h.bu vd, vj, vk
    vmaddwev.h.bu     \vd,    \vj,    \vk
    vmaddwod.h.bu     \vd,    \vj,    \vk
.endm

.macro vdp2add.h.bu.b vd, vj, vk
    vmaddwev.h.bu.b   \vd,    \vj,    \vk
    vmaddwod.h.bu.b   \vd,    \vj,    \vk
.endm

.macro vdp2add.w.h vd, vj, vk
    vmaddwev.w.h      \vd,    \vj,    \vk
    vmaddwod.w.h      \vd,    \vj,    \vk
.endm

.macro xvdp2add.h.bu.b xd, xj, xk
    xvmaddwev.h.bu.b   \xd,  \xj,    \xk
    xvmaddwod.h.bu.b   \xd,  \xj,    \xk
.endm

.macro xvdp2add.w.h xd, xj, xk
    xvmaddwev.w.h      \xd,  \xj,    \xk
    xvmaddwod.w.h      \xd,  \xj,    \xk
.endm

/*
 * Description : Range element vj[i] to vk[i] ~ vj[i]
 * clip: vj > vk ? vj : vk && vj < va ? vj : va
 */
.macro vclip.h  vd,  vj, vk, va
    vmax.h    \vd,  \vj,   \vk
    vmin.h    \vd,  \vd,   \va
.endm

.macro vclip.w  vd,  vj, vk, va
    vmax.w    \vd,  \vj,   \vk
    vmin.w    \vd,  \vd,   \va
.endm

.macro xvclip.h  xd,  xj, xk, xa
    xvmax.h    \xd,  \xj,   \xk
    xvmin.h    \xd,  \xd,   \xa
.endm

.macro xvclip.w  xd,  xj, xk, xa
    xvmax.w    \xd,  \xj,   \xk
    xvmin.w    \xd,  \xd,   \xa
.endm

/*
 * Description : Range element vj[i] to 0 ~ 255
 * clip255: vj < 255 ? vj : 255 && vj > 0 ? vj : 0
 */
.macro vclip255.h  vd, vj
    vmaxi.h   \vd,   \vj,  0
    vsat.hu   \vd,   \vd,  7
.endm

.macro vclip255.w  vd, vj
    vmaxi.w   \vd,   \vj,  0
    vsat.wu   \vd,   \vd,  7
.endm

.macro xvclip255.h  xd, xj
    xvmaxi.h   \xd,   \xj,  0
    xvsat.hu   \xd,   \xd,  7
.endm

.macro xvclip255.w  xd, xj
    xvmaxi.w   \xd,   \xj,  0
    xvsat.wu   \xd,   \xd,  7
.endm

/*
 * Description : Store elements of vector
 * vd : Data vector to be stroed
 * rk : Address of data storage
 * ra : Offset of address
 * si : Index of data in vd
 */
.macro vstelmx.b vd, rk, ra, si
    add.d      \rk,  \rk,  \ra
    vstelm.b   \vd,  \rk,  0, \si
.endm

.macro vstelmx.h vd, rk, ra, si
    add.d      \rk,  \rk,  \ra
    vstelm.h   \vd,  \rk,  0, \si
.endm

.macro vstelmx.w vd, rk, ra, si
    add.d      \rk,  \rk,  \ra
    vstelm.w   \vd,  \rk,  0, \si
.endm

.macro vstelmx.d  vd, rk, ra, si
    add.d      \rk,  \rk,  \ra
    vstelm.d   \vd,  \rk,  0, \si
.endm

.macro vmov xd, xj
    vor.v  \xd,  \xj,  \xj
.endm

.macro xmov xd, xj
    xvor.v  \xd,  \xj,  \xj
.endm

.macro xvstelmx.d  xd, rk, ra, si
    add.d      \rk, \rk,  \ra
    xvstelm.d  \xd, \rk,  0, \si
.endm

/*
 *============================================================================
 * LSX/LASX custom macros
 *============================================================================
 */

/*
 * Load 4 float, double, V128, v256 elements with stride.
 */
.macro FLDS_LOADX_4 src, stride, stride2, stride3, out0, out1, out2, out3
    fld.s     \out0,    \src,    0
    fldx.s    \out1,    \src,    \stride
    fldx.s    \out2,    \src,    \stride2
    fldx.s    \out3,    \src,    \stride3
.endm

.macro FLDD_LOADX_4 src, stride, stride2, stride3, out0, out1, out2, out3
    fld.d     \out0,    \src,    0
    fldx.d    \out1,    \src,    \stride
    fldx.d    \out2,    \src,    \stride2
    fldx.d    \out3,    \src,    \stride3
.endm

.macro LSX_LOADX_4 src, stride, stride2, stride3, out0, out1, out2, out3
    vld     \out0,    \src,    0
    vldx    \out1,    \src,    \stride
    vldx    \out2,    \src,    \stride2
    vldx    \out3,    \src,    \stride3
.endm

.macro LASX_LOADX_4 src, stride, stride2, stride3, out0, out1, out2, out3
    xvld    \out0,    \src,    0
    xvldx   \out1,    \src,    \stride
    xvldx   \out2,    \src,    \stride2
    xvldx   \out3,    \src,    \stride3
.endm

/*
 * Description : Transpose 4x4 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 */
.macro LSX_TRANSPOSE4x4_H in0, in1, in2, in3, out0, out1, out2, out3, \
                          tmp0, tmp1
    vilvl.h   \tmp0,  \in1,   \in0
    vilvl.h   \tmp1,  \in3,   \in2
    vilvl.w   \out0,  \tmp1,  \tmp0
    vilvh.w   \out2,  \tmp1,  \tmp0
    vilvh.d   \out1,  \out0,  \out0
    vilvh.d   \out3,  \out0,  \out2
.endm

/*
 * Description : Transpose 4x4 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     :
 * Example     :
 *               1, 2, 3, 4            1, 5, 9,13
 *               5, 6, 7, 8    to      2, 6,10,14
 *               9,10,11,12  =====>    3, 7,11,15
 *              13,14,15,16            4, 8,12,16
 */
.macro LSX_TRANSPOSE4x4_W in0, in1, in2, in3, out0, out1, out2, out3, \
                          tmp0, tmp1

    vilvl.w    \tmp0,   \in1,    \in0
    vilvh.w    \out1,   \in1,    \in0
    vilvl.w    \tmp1,   \in3,    \in2
    vilvh.w    \out3,   \in3,    \in2

    vilvl.d    \out0,   \tmp1,   \tmp0
    vilvl.d    \out2,   \out3,   \out1
    vilvh.d    \out3,   \out3,   \out1
    vilvh.d    \out1,   \tmp1,   \tmp0
.endm

/*
 * Description : Transpose 8x8 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 */
.macro LSX_TRANSPOSE8x8_H in0, in1, in2, in3, in4, in5, in6, in7, out0, out1,   \
                          out2, out3, out4, out5, out6, out7, tmp0, tmp1, tmp2, \
                          tmp3, tmp4, tmp5, tmp6, tmp7
    vilvl.h      \tmp0,    \in6,   \in4
    vilvl.h      \tmp1,    \in7,   \in5
    vilvl.h      \tmp2,    \in2,   \in0
    vilvl.h      \tmp3,    \in3,   \in1

    vilvl.h      \tmp4,    \tmp1,  \tmp0
    vilvh.h      \tmp5,    \tmp1,  \tmp0
    vilvl.h      \tmp6,    \tmp3,  \tmp2
    vilvh.h      \tmp7,    \tmp3,  \tmp2

    vilvh.h      \tmp0,    \in6,   \in4
    vilvh.h      \tmp1,    \in7,   \in5
    vilvh.h      \tmp2,    \in2,   \in0
    vilvh.h      \tmp3,    \in3,   \in1

    vpickev.d    \out0,    \tmp4,  \tmp6
    vpickod.d    \out1,    \tmp4,  \tmp6
    vpickev.d    \out2,    \tmp5,  \tmp7
    vpickod.d    \out3,    \tmp5,  \tmp7

    vilvl.h      \tmp4,    \tmp1,  \tmp0
    vilvh.h      \tmp5,    \tmp1,  \tmp0
    vilvl.h      \tmp6,    \tmp3,  \tmp2
    vilvh.h      \tmp7,    \tmp3,  \tmp2

    vpickev.d    \out4,    \tmp4,  \tmp6
    vpickod.d    \out5,    \tmp4,  \tmp6
    vpickev.d    \out6,    \tmp5,  \tmp7
    vpickod.d    \out7,    \tmp5,  \tmp7
.endm

/*
 * Description : Transpose 16x8 block with byte elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 */
.macro LASX_TRANSPOSE16X8_B in0, in1, in2, in3, in4, in5, in6, in7,        \
                            in8, in9, in10, in11, in12, in13, in14, in15,  \
                            out0, out1, out2, out3, out4, out5, out6, out7,\
                            tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7
    xvilvl.b   \tmp0,    \in2,     \in0
    xvilvl.b   \tmp1,    \in3,     \in1
    xvilvl.b   \tmp2,    \in6,     \in4
    xvilvl.b   \tmp3,    \in7,     \in5
    xvilvl.b   \tmp4,    \in10,    \in8
    xvilvl.b   \tmp5,    \in11,    \in9
    xvilvl.b   \tmp6,    \in14,    \in12
    xvilvl.b   \tmp7,    \in15,    \in13
    xvilvl.b   \out0,    \tmp1,    \tmp0
    xvilvh.b   \out1,    \tmp1,    \tmp0
    xvilvl.b   \out2,    \tmp3,    \tmp2
    xvilvh.b   \out3,    \tmp3,    \tmp2
    xvilvl.b   \out4,    \tmp5,    \tmp4
    xvilvh.b   \out5,    \tmp5,    \tmp4
    xvilvl.b   \out6,    \tmp7,    \tmp6
    xvilvh.b   \out7,    \tmp7,    \tmp6
    xvilvl.w   \tmp0,    \out2,    \out0
    xvilvh.w   \tmp2,    \out2,    \out0
    xvilvl.w   \tmp4,    \out3,    \out1
    xvilvh.w   \tmp6,    \out3,    \out1
    xvilvl.w   \tmp1,    \out6,    \out4
    xvilvh.w   \tmp3,    \out6,    \out4
    xvilvl.w   \tmp5,    \out7,    \out5
    xvilvh.w   \tmp7,    \out7,    \out5
    xvilvl.d   \out0,    \tmp1,    \tmp0
    xvilvh.d   \out1,    \tmp1,    \tmp0
    xvilvl.d   \out2,    \tmp3,    \tmp2
    xvilvh.d   \out3,    \tmp3,    \tmp2
    xvilvl.d   \out4,    \tmp5,    \tmp4
    xvilvh.d   \out5,    \tmp5,    \tmp4
    xvilvl.d   \out6,    \tmp7,    \tmp6
    xvilvh.d   \out7,    \tmp7,    \tmp6
.endm

/*
 * Description : Transpose 4x4 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 */
.macro LASX_TRANSPOSE4x4_H in0, in1, in2, in3, out0, out1, out2, out3, \
                           tmp0, tmp1
    xvilvl.h   \tmp0,  \in1,   \in0
    xvilvl.h   \tmp1,  \in3,   \in2
    xvilvl.w   \out0,  \tmp1,  \tmp0
    xvilvh.w   \out2,  \tmp1,  \tmp0
    xvilvh.d   \out1,  \out0,  \out0
    xvilvh.d   \out3,  \out0,  \out2
.endm

/*
 * Description : Transpose 4x8 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 */
.macro LASX_TRANSPOSE4x8_H in0, in1, in2, in3, out0, out1, out2, out3, \
                           tmp0, tmp1
    xvilvl.h      \tmp0,    \in2,   \in0
    xvilvl.h      \tmp1,    \in3,   \in1
    xvilvl.h      \out2,    \tmp1,  \tmp0
    xvilvh.h      \out3,    \tmp1,  \tmp0

    xvilvl.d      \out0,    \out2,  \out2
    xvilvh.d      \out1,    \out2,  \out2
    xvilvl.d      \out2,    \out3,  \out3
    xvilvh.d      \out3,    \out3,  \out3
.endm

/*
 * Description : Transpose 8x8 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *               Outputs - out0, out1, out2, out3, out4, out5, out6, out7
 */
.macro LASX_TRANSPOSE8x8_H in0, in1, in2, in3, in4, in5, in6, in7,         \
                           out0, out1, out2, out3, out4, out5, out6, out7, \
                           tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7
    xvilvl.h     \tmp0,   \in6,     \in4
    xvilvl.h     \tmp1,   \in7,     \in5
    xvilvl.h     \tmp2,   \in2,     \in0
    xvilvl.h     \tmp3,   \in3,     \in1

    xvilvl.h     \tmp4,   \tmp1,    \tmp0
    xvilvh.h     \tmp5,   \tmp1,    \tmp0
    xvilvl.h     \tmp6,   \tmp3,    \tmp2
    xvilvh.h     \tmp7,   \tmp3,    \tmp2

    xvilvh.h     \tmp0,   \in6,     \in4
    xvilvh.h     \tmp1,   \in7,     \in5
    xvilvh.h     \tmp2,   \in2,     \in0
    xvilvh.h     \tmp3,   \in3,     \in1

    xvpickev.d   \out0,   \tmp4,    \tmp6
    xvpickod.d   \out1,   \tmp4,    \tmp6
    xvpickev.d   \out2,   \tmp5,    \tmp7
    xvpickod.d   \out3,   \tmp5,    \tmp7

    xvilvl.h     \tmp4,   \tmp1,    \tmp0
    xvilvh.h     \tmp5,   \tmp1,    \tmp0
    xvilvl.h     \tmp6,   \tmp3,    \tmp2
    xvilvh.h     \tmp7,   \tmp3,    \tmp2

    xvpickev.d   \out4,   \tmp4,    \tmp6
    xvpickod.d   \out5,   \tmp4,    \tmp6
    xvpickev.d   \out6,   \tmp5,    \tmp7
    xvpickod.d   \out7,   \tmp5,    \tmp7
.endm

/*
 * Description : Transpose 2x4x4 block with half-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 */
.macro LASX_TRANSPOSE2x4x4_H in0, in1, in2, in3, out0, out1, out2, out3, \
                             tmp0, tmp1, tmp2
    xvilvh.h   \tmp1,    \in0,     \in1
    xvilvl.h   \out1,    \in0,     \in1
    xvilvh.h   \tmp0,    \in2,     \in3
    xvilvl.h   \out3,    \in2,     \in3

    xvilvh.w   \tmp2,    \out3,    \out1
    xvilvl.w   \out3,    \out3,    \out1

    xvilvl.w   \out2,    \tmp0,    \tmp1
    xvilvh.w   \tmp1,    \tmp0,    \tmp1

    xvilvh.d   \out0,    \out2,    \out3
    xvilvl.d   \out2,    \out2,    \out3
    xvilvh.d   \out1,    \tmp1,    \tmp2
    xvilvl.d   \out3,    \tmp1,    \tmp2
.endm

/*
 * Description : Transpose 4x4 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Details     :
 * Example     :
 *               1, 2, 3, 4,  1, 2, 3, 4        1,5, 9,13, 1,5, 9,13
 *               5, 6, 7, 8,  5, 6, 7, 8   to   2,6,10,14, 2,6,10,14
 *               9,10,11,12,  9,10,11,12 =====> 3,7,11,15, 3,7,11,15
 *              13,14,15,16, 13,14,15,16        4,8,12,16, 4,8,12,16
 */
.macro LASX_TRANSPOSE4x4_W in0, in1, in2, in3, out0, out1, out2, out3, \
                           tmp0, tmp1

    xvilvl.w    \tmp0,   \in1,    \in0
    xvilvh.w    \out1,   \in1,    \in0
    xvilvl.w    \tmp1,   \in3,    \in2
    xvilvh.w    \out3,   \in3,    \in2

    xvilvl.d    \out0,   \tmp1,   \tmp0
    xvilvl.d    \out2,   \out3,   \out1
    xvilvh.d    \out3,   \out3,   \out1
    xvilvh.d    \out1,   \tmp1,   \tmp0
.endm

/*
 * Description : Transpose 8x8 block with word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3, in4, in5, in6, in7
 *               Outputs - out0, out1, out2, out3, out4, out5, out6,
 *               _out7
 * Example     : LASX_TRANSPOSE8x8_W
 *         in0 : 1,2,3,4,5,6,7,8
 *         in1 : 2,2,3,4,5,6,7,8
 *         in2 : 3,2,3,4,5,6,7,8
 *         in3 : 4,2,3,4,5,6,7,8
 *         in4 : 5,2,3,4,5,6,7,8
 *         in5 : 6,2,3,4,5,6,7,8
 *         in6 : 7,2,3,4,5,6,7,8
 *         in7 : 8,2,3,4,5,6,7,8
 *
 *        out0 : 1,2,3,4,5,6,7,8
 *        out1 : 2,2,2,2,2,2,2,2
 *        out2 : 3,3,3,3,3,3,3,3
 *        out3 : 4,4,4,4,4,4,4,4
 *        out4 : 5,5,5,5,5,5,5,5
 *        out5 : 6,6,6,6,6,6,6,6
 *        out6 : 7,7,7,7,7,7,7,7
 *        out7 : 8,8,8,8,8,8,8,8
 */
.macro LASX_TRANSPOSE8x8_W in0, in1, in2, in3, in4, in5, in6, in7,\
                           out0, out1, out2, out3, out4, out5, out6, out7,\
                           tmp0, tmp1, tmp2, tmp3
    xvilvl.w    \tmp0,   \in2,    \in0
    xvilvl.w    \tmp1,   \in3,    \in1
    xvilvh.w    \tmp2,   \in2,    \in0
    xvilvh.w    \tmp3,   \in3,    \in1
    xvilvl.w    \out0,   \tmp1,   \tmp0
    xvilvh.w    \out1,   \tmp1,   \tmp0
    xvilvl.w    \out2,   \tmp3,   \tmp2
    xvilvh.w    \out3,   \tmp3,   \tmp2

    xvilvl.w    \tmp0,   \in6,    \in4
    xvilvl.w    \tmp1,   \in7,    \in5
    xvilvh.w    \tmp2,   \in6,    \in4
    xvilvh.w    \tmp3,   \in7,    \in5
    xvilvl.w    \out4,   \tmp1,   \tmp0
    xvilvh.w    \out5,   \tmp1,   \tmp0
    xvilvl.w    \out6,   \tmp3,   \tmp2
    xvilvh.w    \out7,   \tmp3,   \tmp2

    xmov        \tmp0,   \out0
    xmov        \tmp1,   \out1
    xmov        \tmp2,   \out2
    xmov        \tmp3,   \out3
    xvpermi.q   \out0,   \out4,   0x02
    xvpermi.q   \out1,   \out5,   0x02
    xvpermi.q   \out2,   \out6,   0x02
    xvpermi.q   \out3,   \out7,   0x02
    xvpermi.q   \out4,   \tmp0,   0x31
    xvpermi.q   \out5,   \tmp1,   0x31
    xvpermi.q   \out6,   \tmp2,   0x31
    xvpermi.q   \out7,   \tmp3,   0x31
.endm

/*
 * Description : Transpose 4x4 block with double-word elements in vectors
 * Arguments   : Inputs  - in0, in1, in2, in3
 *               Outputs - out0, out1, out2, out3
 * Example     : LASX_TRANSPOSE4x4_D
 *         in0 : 1,2,3,4
 *         in1 : 1,2,3,4
 *         in2 : 1,2,3,4
 *         in3 : 1,2,3,4
 *
 *        out0 : 1,1,1,1
 *        out1 : 2,2,2,2
 *        out2 : 3,3,3,3
 *        out3 : 4,4,4,4
 */
.macro LASX_TRANSPOSE4x4_D in0, in1, in2, in3, out0, out1, out2, out3, \
                           tmp0, tmp1
    xvilvl.d    \tmp0,   \in1,    \in0
    xvilvh.d    \out1,   \in1,    \in0
    xvilvh.d    \tmp1,   \in3,    \in2
    xvilvl.d    \out2,   \in3,    \in2

    xvor.v      \out0,   \tmp0,   \tmp0
    xvor.v      \out3,   \tmp1,   \tmp1

    xvpermi.q   \out0,   \out2,   0x02
    xvpermi.q   \out2,   \tmp0,   0x31
    xvpermi.q   \out3,   \out1,   0x31
    xvpermi.q   \out1,   \tmp1,   0x02
.endm
