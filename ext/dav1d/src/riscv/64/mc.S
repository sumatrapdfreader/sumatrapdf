/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2024, Nathan Egge, Niklas Haas, Bogdan Gligorijevic
 * Copyright © 2025, Sungjoon Moon
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
 *****************************************************************************/

#include "src/riscv/asm.S"

function blend_vl512_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 3
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_epilog)
endfunc

function blend_vl256_8bpc_rvv, export=1, ext=zbb
  ctz t0, a3
  addi t0, t0, 0xc3
  j L(blend_epilog)
endfunc

function blend_8bpc_rvv, export=1, ext="v,zbb"
  ctz t0, a3
  addi t0, t0, 0xc4
L(blend_epilog):
  csrw vxrm, zero
  andi t0, t0, 0xc7
  vsetvl zero, a3, t0
  li t1, 64
1:
  addi a4, a4, -2
  vle8.v v4, (a2)
  add a2, a2, a3
  vle8.v v6, (a2)
  add a2, a2, a3
  vle8.v v8, (a5)
  add a5, a5, a3
  vle8.v v10, (a5)
  add a5, a5, a3
  vle8.v v0, (a0)
  add t0, a0, a1
  vle8.v v2, (t0)
  vwmulu.vv v16, v4, v8
  vwmulu.vv v20, v6, v10
  vrsub.vx v8, v8, t1
  vrsub.vx v10, v10, t1
  vwmaccu.vv v16, v0, v8
  vwmaccu.vv v20, v2, v10
  vnclipu.wi v0, v16, 6
  vnclipu.wi v2, v20, 6
  vse8.v v0, (a0)
  vse8.v v2, (t0)
  add a0, t0, a1
  bnez a4, 1b
  ret
endfunc

function blend_h_vl512_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 3
  li t2, 64
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_h_epilog)
endfunc

function blend_h_vl256_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 2
  li t2, 64
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_h_epilog)
endfunc

function blend_h_8bpc_rvv, export=1, ext="v,zbb"
  li t2, 64
  bgt a3, t2, 128f
  ctz t0, a3
  addi t0, t0, 0xc4
L(blend_h_epilog):
  csrw vxrm, zero
  andi t0, t0, 0xc7
  vsetvl zero, a3, t0
  la t1, dav1d_obmc_masks
  srai t0, a4, 2
  add t1, t1, a4
  sub a4, a4, t0
0:
  mv t5, ra
1:
  addi a4, a4, -2
  lbu t3, (t1)
  addi t1, t1, 1
  lbu t4, (t1)
  addi t1, t1, 1
  vle8.v v8, (a2)
  add a2, a2, a3
  vle8.v v12, (a2)
  add a2, a2, a3
  vle8.v v0, (a0)
  add t0, a0, a1
  vle8.v v4, (t0)
  vwmulu.vx v16, v8, t3
  vwmulu.vx v24, v12, t4
  sub t3, t2, t3
  sub t4, t2, t4
  vwmaccu.vx v16, t3, v0
  vwmaccu.vx v24, t4, v4
  vnclipu.wi v0, v16, 6
  vnclipu.wi v4, v24, 6
  vse8.v v0, (a0)
  vse8.v v4, (t0)
  add a0, t0, a1
  bgtz a4, 1b
  jr t5
128:
  csrw vxrm, zero
  vsetvli zero, t2, e8, m4, ta, ma
  la t1, dav1d_obmc_masks
  srai t0, a4, 2
  add t1, t1, a4
  sub a4, a4, t0
  mv a5, a0
  mv a6, a2
  mv a7, a4
  jal t5, 1b
  add t1, t1, a4
  add a0, a5, t2
  add a2, a6, t2
  mv a4, a7
  sub t1, t1, a4
  j 0b
endfunc

function blend_v_vl512_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 3
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_v_epilog)
endfunc

function blend_v_vl256_8bpc_rvv, export=1, ext=zbb
  srai t0, a3, 2
  ctz t0, t0
  addi t0, t0, 0xc5
  j L(blend_v_epilog)
endfunc

function blend_v_8bpc_rvv, export=1, ext="v,zbb"
  ctz t0, a3
  addi t0, t0, 0xc4
L(blend_v_epilog):
  andi t0, t0, 0xc7
  srai t1, a3, 2
  sub t1, a3, t1
  vsetvl zero, t1, t0
  csrw vxrm, zero
  la t1, dav1d_obmc_masks
  add t1, t1, a3
  vle8.v v8, (t1)
  li t0, 64
  vrsub.vx v10, v8, t0
1:
  addi a4, a4, -2
  vle8.v v4, (a2)
  add a2, a2, a3
  vle8.v v6, (a2)
  add a2, a2, a3
  vle8.v v0, (a0)
  add t0, a0, a1
  vle8.v v2, (t0)
  vwmulu.vv v12, v4, v8
  vwmulu.vv v16, v6, v8
  vwmaccu.vv v12, v0, v10
  vwmaccu.vv v16, v2, v10
  vnclipu.wi v0, v12, 6
  vnclipu.wi v2, v16, 6
  vse8.v v0, (a0)
  vse8.v v2, (t0)
  add a0, t0, a1
  bnez a4, 1b
  ret
endfunc

.macro avg va, vb, vm
    vadd.vv \va, \va, \vb
.endm

.macro w_avg va, vb, vm
    vwmul.vx v24, \va, a6
    vwmacc.vx v24, a7, \vb
    vnclip.wi \va, v24, 8
.endm

.macro mask va, vb, vm
    vwmul.vv v24, \va, \vm
    vrsub.vx \vm, \vm, a7
    vwmacc.vv v24, \vb, \vm
    vnclip.wi \va, v24, 10
.endm

.macro bidir_fn type, shift
function \type\()_8bpc_rvv, export=1, ext="v,zba,zbb"
.ifc \type, w_avg
    li a7, 16
    sub a7, a7, a6
.endif
.ifc \type, mask
    li a7, 64
.endif
    li t0, 4
    csrw vxrm, zero
    beq t0, a4, 4f
    csrr t0, vlenb
    ctz t1, a4
    ctz t0, t0
    li t2, 1
    sub t0, t1, t0
    li t4, -3
    bgt t0, t2, 2f
    max t0, t0, t4
    andi t1, t0, 0x7
    addi t0, t1, 1 # may overflow into E16 bit
    ori t0, t0, MA | TA | E16
    ori t1, t1, MA | TA | E8
1:
    addi a5, a5, -4
.rept 2
    vsetvl zero, a4, t0
    sh1add t3, a4, a2
    vle16.v v0, (a2)
    sh1add a2, a4, t3
    vle16.v v4, (t3)
    sh1add t3, a4, a3
    vle16.v v8, (a3)
    sh1add a3, a4, t3
    vle16.v v12, (t3)
.ifc \type, mask
    add t3, a4, a6
    vle8.v v24, (a6)
    add a6, a4, t3
    vle8.v v26, (t3)
    vzext.vf2 v16, v24
    vzext.vf2 v20, v26
.endif
    \type v0, v8, v16
    \type v4, v12, v20
    vmax.vx v8, v0, zero
    vmax.vx v12, v4, zero
    vsetvl zero, zero, t1
    vnclipu.wi v0, v8,  \shift
    vnclipu.wi v2, v12, \shift
    add t3, a1, a0
    vse8.v v0, (a0)
    add a0, a1, t3
    vse8.v v2, (t3)
.endr
    bnez a5, 1b
    ret
2:
    mv t0, a0
    neg t4, a4
    add a0, a1, a0
    addi a5, a5, -1
20:
    vsetvli t2, a4, e16, m4, ta, ma
    sh1add t4, t2, t4
    sh1add t3, t2, a2
    vle16.v v0, (a2)
    sh1add a2, t2, t3
    vle16.v v4, (t3)
    sh1add t3, t2, a3
    vle16.v v8, (a3)
    sh1add a3, t2, t3
    vle16.v v12, (t3)
.ifc \type, mask
    add t3, t2, a6
    vle8.v v24, (a6)
    add a6, t2, t3
    vle8.v v26, (t3)
    vzext.vf2 v16, v24
    vzext.vf2 v20, v26
.endif
    \type v0, v8, v16
    \type v4, v12, v20
    vmax.vx v8, v0, zero
    vmax.vx v12, v4, zero
    vsetvli zero, zero, e8, m2, ta, ma
    vnclipu.wi v0, v8,  \shift
    vnclipu.wi v2, v12, \shift
    add t3, t2, t0
    vse8.v v0, (t0)
    add t0, t2, t3
    vse8.v v2, (t3)
    bnez t4, 20b
    bnez a5, 2b
    ret
4:
    slli t0, a5, 2
    vsetvli t1, t0, e16, m4, ta, ma
    vle16.v v0, (a2)
    sh1add a2, t1, a2
    vle16.v v4, (a3)
    sh1add a3, t1, a3
.ifc \type, mask
    vle8.v v16, (a6)
    add a6, t1, a6
    vzext.vf2 v8, v16
.endif
    \type v0, v4, v8
    vmax.vx v8, v0, zero
    vsetvli zero, zero, e8, m2, ta, ma
    vnclipu.wi v0, v8, \shift
    vsetvli t1, a5, e32, m2, ta, ma
    vsse32.v v0, (a0), a1
    ctz t0, t1
    sub a5, a5, t1
    sll t0, a1, t0
    add a0, t0, a0
    bnez a5, 4b
    ret
endfunc
.endm

bidir_fn avg,   5
bidir_fn w_avg, 0
bidir_fn mask,  0

function warp_8x8_8bpc_rvv, export=1, ext="v"
    csrw vxrm, zero

    vsetivli zero, 8, e16, m1, ta, ma
    addi sp, sp, -2*15*8
    mv t5, sp
    li t0, 3
    mul t0, a3, t0
    sub a2, a2, t0
    addi a2, a2, -3

    li t0, 64
    addi a3, a3, -8
    li t1, 15
    la t2, dav1d_mc_warp_filter

    lh t6, (a4)
    lh t4, 2(a4)
    vid.v v30
    vwmul.vx v28, v30, t6
1:
    addi t1, t1, -1


    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a5
    add a5, a5, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24

    vsetvli zero, zero, e16, m1, ta, ma
.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle8.v v10, (a2)
    addi a2, a2, 1

    vsext.vf2 v14, v\i
    vzext.vf2 v16, v10

.if \i == 2
    vwmulsu.vv v12, v14, v16
.else
    vwmaccsu.vv v12, v14, v16
.endif
.endr
    vnclip.wi v10, v12, 3

    add a2, a2, a3
    vse16.v v10, (t5)
    addi t5, t5, 16

    bnez t1, 1b

    mv t5, sp
    li t1, 8

    lh t6, 4(a4)
    lh t4, 6(a4)
    vwmul.vx v28, v30, t6
2:
    addi t1, t1, -1

    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a6

    add a6, a6, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24
    vsetvli zero, zero, e16, m1, ta, ma

.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle16.v v10, (t5)
    addi t5, t5, 16

    vsext.vf2 v14, v\i

.if \i == 2
    vwmul.vv v12, v14, v10
.else
    vwmacc.vv v12, v14, v10
.endif
.endr
    addi t5, t5, -16*7
    vnclip.wi v10, v12, 11

    vmax.vx v10, v10, zero
    vsetvli zero, zero, e8, mf2, ta, ma

    vnclipu.wi v12, v10, 0

    vse8.v v12, (a0)
    add a0, a0, a1

    bnez t1, 2b

    addi sp, sp, 2*15*8

    ret
endfunc

function warp_8x8t_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero

    vsetivli zero, 8, e16, m1, ta, ma
    addi sp, sp, -2*15*8
    mv t5, sp
    li t0, 3
    mul t0, a3, t0
    sub a2, a2, t0
    addi a2, a2, -3

    li t0, 64
    addi a3, a3, -8
    li t1, 15
    la t2, dav1d_mc_warp_filter

    lh t6, (a4)
    lh t4, 2(a4)
    vid.v v30
    vwmul.vx v28, v30, t6
1:
    addi t1, t1, -1


    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a5
    add a5, a5, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24

    vsetvli zero, zero, e16, m1, ta, ma
.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle8.v v10, (a2)
    addi a2, a2, 1

    vsext.vf2 v14, v\i
    vzext.vf2 v16, v10

.if \i == 2
    vwmulsu.vv v12, v14, v16
.else
    vwmaccsu.vv v12, v14, v16
.endif
.endr
    vnclip.wi v10, v12, 3

    add a2, a2, a3
    vse16.v v10, (t5)
    addi t5, t5, 16

    bnez t1, 1b

    mv t5, sp
    li t1, 8

    lh t6, 4(a4)
    lh t4, 6(a4)
    vwmul.vx v28, v30, t6
2:
    addi t1, t1, -1

    vsetvli zero, zero, e32, m2, ta, ma
    vadd.vx v4, v28, a6
    add a6, a6, t4
    vssra.vi v2, v4, 10
    vadd.vx v2, v2, t0
    vsll.vi v24, v2, 3
    vsetvli zero, zero, e8, mf2, ta, ma

    vluxseg8ei32.v v2, (t2), v24
    vsetvli zero, zero, e16, m1, ta, ma

.irp i, 2, 3, 4, 5, 6, 7, 8, 9
    vle16.v v10, (t5)
    addi t5, t5, 16

    vsext.vf2 v14, v\i

.if \i == 2
    vwmul.vv v12, v14, v10
.else
    vwmacc.vv v12, v14, v10
.endif

.endr
    addi t5, t5, -16*7
    vnclip.wi v10, v12, 7

    vse16.v v10, (a0)
    sh1add a0, a1, a0

    bnez t1, 2b

    addi sp, sp, 2*15*8

    ret
endfunc

function emu_edge_8bpc_rvv, export=1, ext="v,zbb"
  ld t0, 0(sp)
  ld t1, 8(sp)

  // int cx = iclip((int) x, 0, (int) iw - 1);
  max t2, a4, zero
  addi t4, a2, -1
  min t2, t2, t4

  // int cy = iclip((int) y, 0, (int) ih - 1);
  max t3, a5, zero
  addi t5, a3, -1
  min t3, t3, t5

  // ref += cy*PXSTRIDE(ref_stride) + cx
  mul t3, t3, t1
  add t3, t3, t2

  add t0, t0, t3

  addi t4, a0, -1

  neg t2, a4
  add t3, a4, a0
  sub t3, t3, a2

  // int left_ext = iclip((int) -x, 0, (int) bw - 1);
  max t2, t2, zero
  min a2, t2, t4 # a2 = left_ext

  // int right_ext = iclip((int) (x + bw - iw), 0, (int) bw - 1);
  max t3, t3, zero
  min a4, t3, t4 # a4 = right_ext

  addi t6, a1, -1

  neg t4, a5
  add t5, a5, a1
  sub t5, t5, a3

  // int top_ext = iclip((int) -y, 0, (int) bh - 1);
  max t4, t4, zero
  min a3, t4, t6 # a3 = top_ext

  // int bottom_ext = iclip((int) (x + bh - ih), 0, (int) bh - 1);
  max t5, t5, zero
  min a5, t5, t6 # a5 = bottom_ext

  sub t4, a1, a3
  sub t4, t4, a5 # t4 = center_h

  mul t5, a3, a7
  add a1, a6, t5 # blk = dst + top_ext * dst_stride

  sub t3, a0, a2
  sub t3, t3, a4 # t3 = center_w = bw - left_ext - right_ext

.macro v_loop need_left, need_right
9:
  # pixel_copy()
  add t5, a1, a2 # t5 = blk + left_ext
  mv t2, t0 # ref
0:
  vsetvli t6, t3, e8, m1, ta, ma
  vle8.v v8, (t2)
  add t2, t2, t6

  vse8.v v8, (t5)
  sub t3, t3, t6
  add t5, t5, t6
  bnez t3, 0b

  sub t3, a0, a2
  sub t3, t3, a4 # t3 = center_w = bw - left_ext - right_ext

.if \need_left
  lb t2, (t0) # ref[0]
  # pixel_set()
  vsetvli t6, a2, e8, m1, ta, ma
  vmv.v.x v8, t2
  mv t2, a2 # left_ext
  mv t5, a1 # blk
0:
  vse8.v v8, (t5)
  sub t2, t2, t6 # left_ext -= t6
  add t5, t5, t6 # blk += t6
  vsetvli t6, t2, e8, m1, ta, ma
  bnez t2, 0b
.endif

.if \need_right
  add t5, a1, a2 # t5 = blk + left_ext
  add t5, t5, t3 # t5 = blk + left_ext + center_w
  lb t2, -1(t5) # blk[left_ext + center_w - 1]
  # pixel_set()
  vsetvli t6, a4, e8, m1, ta, ma
  vmv.v.x v8, t2
  mv t2, a4 # right_ext
0:
  vse8.v v8, (t5)
  sub t2, t2, t6
  add t5, t5, t6
  vsetvli t6, t2, e8, m1, ta, ma
  bnez t2, 0b
.endif

  add t0, t0, t1 # ref += ref_stride
  add a1, a1, a7 # blk += dst_stride
  addi t4, t4, -1 # center_h--
  bnez t4, 9b
.endm

L(emu_edge_center):
  blez t4, L(emu_edge_bottom)

  beqz a2, 1f # if (left_ext)
  beqz a4, 2f # if (right_ext)
  v_loop 1, 1
  j L(emu_edge_bottom)

1:
  beqz a4, 3f
  v_loop 0, 1
  j L(emu_edge_bottom)

2:
  v_loop 1, 0
  j L(emu_edge_bottom)

3:
  v_loop 0, 0

L(emu_edge_bottom): # copy bottom
  blez a5, L(emu_edge_top)
  mv t2, a0 # bw
2:
  mv t5, a5 # bottom_ext
  mv t1, a1 # dst

  vsetvli t6, t2, e8, m1, ta, ma
  sub t0, t1, a7 # dst - dst_stride
  vle8.v v8, (t0)
0:
  vse8.v v8, (t1)
  add t1, t1, a7
  addi t5, t5, -1
  bnez t5, 0b

  sub t2, t2, t6
  add a1, a1, t6
  bnez t2, 2b

L(emu_edge_top): # copy top
  blez a3, L(emu_edge_end)
  mul t5, a3, a7
  add t1, a6, t5 # blk = dst + top_ext * PXSTRIDE(dst_stride)
  # a6 = dst
1:
  mv t0, a3 # top_ext
  mv t4, a6 # dst

  vsetvli t6, a0, e8, m1, ta, ma
  vle8.v v8, (t1)
0:
  vse8.v v8, (t4)
  add t4, t4, a7
  vse8.v v8, (t4)
  add t4, t4, a7
  addi t0, t0, -2
  bgtz t0, 0b

  sub a0, a0, t6
  add t1, t1, t6
  add a6, a6, t6

  bnez a0, 1b

L(emu_edge_end):
  ret
endfunc

.macro w_mask_fn type vlen
function w_mask_\type\()_\vlen\()8bpc_rvv, export=1, ext="v,zba,zbb"
  csrw vxrm, zero
  li t1, 38*256+8
.ifc \vlen, vl256_
  addi t0, zero, 64
  bgt a4, t0, 2f
  li t2, 0xCAC9C8CFCE0000
  li t3, 0xC1C0C7C6C50000
.else
  addi t0, zero, 32
  bgt a4, t0, 2f
  li t2, 0xCAC9C8CF0000
  li t3, 0xC1C0C7C60000
.endif
  ctz t4, a4
  slli t4, t4, 3
  srl t2, t2, t4
  andi t2, t2, 0xFF
  srl t3, t3, t4
  andi t3, t3, 0xFF

1:
.if \type == 444
w_mask_body 444 narrow

  sh1add a0, a1, a0 # dst += dst_stride
  add a6, a6, a4 # mask += w
.elseif \type == 422
w_mask_body 422 narrow

  sh1add a0, a1, a0 # dst += dst_stride
  srli t4, a4, 1
  add a6, a6, t4 # mask += w >> 1
.elseif \type == 420
w_mask_body 420 narrow

  sh1add a0, a1, a0 # dst += dst_stride
.endif

  sh1add a2, a4, a2
  sh1add a3, a4, a3

  addi a5, a5, -2
  bnez a5, 1b

  ret

2:
  li t2, 0xca
  li t3, 0xc1

3:
  mv t5, zero

.if \type == 444
w_mask_body 444 wide # VLEN>=256
.elseif \type == 422
w_mask_body 422 wide # VLEN>=256
.elseif \type == 420
w_mask_body 420 wide # VLEN>=256
.endif

  add t5, t5, t6
  bne t5, a4, 4b

  sh1add a0, a1, a0 # dst += dst_stride
.if \type == 444
  add a6, a6, a4 # mask += w
.elseif \type == 422
  srli t4, a4, 1
  add a6, a6, t4 # mask += w >> 1
.elseif \type == 420
.endif

  sh1add a2, a4, a2
  sh1add a3, a4, a3

  addi a5, a5, -2
  bnez a5, 3b

  ret

endfunc
.endm

.macro w_mask_body type size
  mv t0, a0 # dst

4:
  vsetvl t6, a4, t2

  # load tmp1 and tmp2
  vle16.v v0, (a2) # tmp1[x]

  sh1add t4, a4, a2 # tmp1
  vle16.v v16, (t4) # tmp1[x]
  sh1add a2, t6, a2 # tmp1 += w / k

  vle16.v v4, (a3) # tmp2[x]

  sh1add t4, a4, a3 # tmp2
  vle16.v v20, (t4) # tmp2[x]
  sh1add a3, t6, a3 # tmp2 += w / k

  # v12 = abs(tmp1[x] - tmp2[x])
  vsub.vv v12, v0, v4 # tmp1[x] - tmp2[x]
  vsub.vv v8, v4, v0 # tmp2[x] - tmp1[x]
  vmax.vv v8, v12, v8

  vsub.vv v28, v16, v20 # tmp1[x] - tmp2[x]
  vsub.vv v24, v20, v16 # tmp2[x] - tmp1[x]
  vmax.vv v24, v28, v24

  li t4, 64

  # min(38 + (v12 + 8) >> 8, 64) -> min((v12 + 38*256 + 8) >> 8, 64)
  vadd.vx v8, v8, t1
  vsra.vi v8, v8, 8
  vmin.vx v8, v8, t4

  vadd.vx v24, v24, t1
  vsra.vi v24, v24, 8
  vmin.vx v24, v24, t4

  # dst[x] = (tmp1[x] - tmp2[x]) * m + 64 * tmp2[x];
  # v12, v28 = tmp1[x] - tmp2[x]
  # v8, v24 = {m,n}
  vwmul.vx v0, v4, t4
  vwmacc.vv v0, v8, v12
  vnclipu.wi v0, v0, 10
  vmax.vx v0, v0, zero

  vwmul.vx v16, v20, t4
  vwmacc.vv v16, v24, v28
  vnclipu.wi v16, v16, 10
  vmax.vx v16, v16, zero

.if \type == 444
  vsetvl zero, zero, t3

  vnclipu.wi v0, v0, 0
  vnclipu.wi v16, v16, 0

  vse8.v v0, (t0) # dst[x] =
  add t4, t0, a1
  vse8.v v16, (t4) # dst[x] =
  add t0, t0, t6

  vnsrl.wi v8, v8, 0
  vnsrl.wi v24, v24, 0

  vse8.v v8, (a6) # mask[x] = m
  add t4, a6, a4
  vse8.v v24, (t4) # mask[x] = m
  add a6, a6, t6

.elseif \type == 422
  # v4, v20 = m
  # v12, v28 = n
  vnsrl.wi v4, v8, 0
  vnsrl.wi v8, v8, 16

  vnsrl.wi v20, v24, 0
  vnsrl.wi v24, v24, 16

  # v8, v24 = m + n - sign
  vadd.vv v8, v4, v8
  vsub.vx v8, v8, a7

  vadd.vv v24, v20, v24
  vsub.vx v24, v24, a7

  vsetvl zero, zero, t3

  vnclipu.wi v0, v0, 0
  vnclipu.wi v16, v16, 0

  vse8.v v0, (t0) # dst[x] =
  add t4, t0, a1
  vse8.v v16, (t4) # dst[x] =
  add t0, t0, t6

  vnclipu.wi v8, v8, 1
  vnclipu.wi v24, v24, 1

.ifc \size, wide
  srli t4, t6, 1
  vsetvl zero, t4, t3
.endif

  vse8.v v8, (a6) # mask[x] = m + n + 1 - sign
  srli t4, a4, 1
  add t4, a6, t4
  vse8.v v24, (t4) # mask[x] = m + n + 1 - sign
  srli t4, t6, 1
  add a6, a6, t4
.elseif \type == 420
  # v4, v20 = m
  # v12, v28 = n
  vnsrl.wi v4, v8, 0
  vnsrl.wi v8, v8, 16

  vnsrl.wi v20, v24, 0
  vnsrl.wi v24, v24, 16

  # v8 = m + n + mask[x >> 1]
  vadd.vv v8, v4, v8
  vadd.vv v24, v20, v24
  vadd.vv v8, v8, v24
  vsub.vx v8, v8, a7

  vsetvl zero, zero, t3

  vnclipu.wi v0, v0, 0
  vnclipu.wi v16, v16, 0

  vse8.v v0, (t0) # dst[x] =
  add t4, t0, a1
  vse8.v v16, (t4) # dst[x] =
  add t0, t0, t6

  vnclipu.wi v8, v8, 2

  vse8.v v8, (a6) # mask[x] = (m + n + mask[x >> 1] + 2 - sign) >> 2;
  srli t4, t6, 1
  add a6, a6, t4
.endif
.endm

w_mask_fn 444
w_mask_fn 444 vl256_
w_mask_fn 422
w_mask_fn 422 vl256_
w_mask_fn 420
w_mask_fn 420 vl256_
