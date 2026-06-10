/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2024, Bogdan Gligorijevic
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

function dc_gen_16bpc_rvv, export=1, ext="v,zba,zbb"
    .variant_cc dav1d_dc_gen_16bpc_rvv
    add t1, a1, a2
    srli t5, t1, 1
    mv t1, a1
    addi t2, a0, 2
    vsetvli zero, t1, e32, m8, ta, ma
    vmv.v.x v0, zero
1:
    vsetvli t3, t1, e16, m4, tu, ma
    vle16.v v8, (t2)
    vwaddu.wv v0, v0, v8
    sub t1, t1, t3

    sh1add t2, t3, t2
    bnez t1, 1b

    mv t1, a2
    mv t2, a0
    vsetvli zero, t1, e32, m8, ta, ma
    vmv.v.x v16, zero
2:
    vsetvli t3, t1, e16, m4, tu, ma
    sub t1, t1, t3
    sll t3, t3, 1
    sub t2, t2, t3
    vle16.v v8, (t2)
    vwaddu.wv v16, v16, v8

    bnez t1, 2b

    vsetvli zero, a1, e32, m8, ta, ma
    vmv.s.x v24, t5
    vmv.s.x v25, zero
    vredsum.vs v8, v0, v24
    vsetvli zero, a2, e32, m8, ta, ma
    vredsum.vs v0, v16, v25
    vmv.x.s t5, v8
    vmv.x.s t1, v0
    add t5, t5, t1

    add t1, a1, a2
    ctz t1, t1

    srl a0, t5, t1

    beq a1, a2, 5f
    slli t1, a1, 1
    sltu t2, t1, a2
    slli t3, a2, 1
    sltu t1, t3, a1
    or t1, t1, t2
    bnez t1, 3f

    li t1, 0xAAAB
    j 4f
3:
    li t1, 0x6667
4:
    mul a0, a0, t1
    li t1, 17
    srl a0, a0, t1
5:
    jr t0
endfunc

function dc_gen_top_16bpc_rvv, export=1, ext="v,zba,zbb"
    .variant_cc dav1d_dc_gen_top_16bpc_rvv
    mv t1, a1
    srli t5, a1, 1
    addi a0, a0, 2
    vsetvli zero, t1, e32, m2, ta, ma
    vmv.v.x v0, zero
1:
    vsetvli t3, t1, e16, m1, tu, ma
    vle16.v v4, (a0)
    vwaddu.wv v0, v0, v4

    sh1add a0, t3, a0
    sub t1, t1, t3
    bnez t1, 1b

    j dc_gen_sum_up_16bpc_rvv
endfunc

function dc_gen_left_16bpc_rvv, export=1, ext="v,zba,zbb"
    .variant_cc dav1d_dc_gen_left_16bpc_rvv
    mv t1, a1
    srli t5, a1, 1
    vsetvli zero, t1, e32, m2, ta, ma
    vmv.v.x v0, zero
1:
    vsetvli t3, t1, e16, m1, tu, ma
    sub t1, t1, t3
    slli t3, t3, 1
    sub a0, a0, t3
    vle16.v v4, (a0)
    vwaddu.wv v0, v0, v4

    bnez t1, 1b

    j dc_gen_sum_up_16bpc_rvv
endfunc

function dc_gen_sum_up_16bpc_rvv, export=1, ext="v,zba,zbb"
    .variant_cc dav1d_dc_gen_sum_up_16bpc_rvv

    vsetvli zero, a1, e32, m2, ta, ma
    vmv.s.x v4, t5
    vredsum.vs v8, v0, v4
    vmv.x.s t5, v8

    ctz t1, a1

    srl a0, t5, t1
    jr t0
endfunc

function cfl_pred_16bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
1:
    li t2, 0
    mv t3, a2
2:
    vsetvli t0, t3, e16, m2, ta, ma
    sh1add t4, t2, a0
    vle16.v v0, (a5)
    sh1add a5, t0, a5

    vwmul.vx v4, v0, a6
    vsetvli zero, zero, e32, m4, ta, mu
    vneg.v v8, v4
    vmslt.vx v0, v4, x0
    vmax.vv v12, v8, v4
    vssra.vi v16, v12, 6
    vneg.v v16, v16, v0.t
    vadd.vx v20, v16, a4
    vmax.vx v0, v20, zero
    vmin.vx v0, v0, a7
    vsetvli zero, zero, e16, m2, ta, ma
    vnclipu.wi v4, v0, 0
    vse16.v v4, (t4)
    add t2, t0, t2
    sub t3, t3, t0
    bnez t3, 2b
    addi a3, a3, -1
    add a0, a0, a1

    bnez a3, 1b
    ret
endfunc

function ipred_cfl_16bpc_rvv, export=1, ext=v
    mv t6, a0 # dst
    mv a0, a2 # topleft
    mv t4, a1 # stride
    mv a1, a3 # width
    mv a2, a4 # height
    jal t0, dc_gen_16bpc_rvv
    mv a2, a3 # width
    mv a3, a4 # height
    mv a4, a0 # dc_get_top
    mv a0, t6 # dst
    mv a1, t4 # stride
    j cfl_pred_16bpc_rvv
endfunc

function ipred_cfl_128_16bpc_rvv, export=1, ext="v,zba"
    # dc = (bitdepth_max + 1) >> 1, then just rearrange registers
    mv a2, a3
    mv a3, a4
    addi a4, a7, 1
    srli a4, a4, 1

    j cfl_pred_16bpc_rvv
endfunc

function ipred_cfl_top_16bpc_rvv, export=1, ext=v
    mv t6, a0 # dst
    mv a0, a2 # topleft
    mv t4, a1 # stride
    mv a1, a3 # width
    jal t0, dc_gen_top_16bpc_rvv
    mv a3, a4 # height
    mv a4, a0 # dc_get_top
    mv a0, t6 # dst
    mv a2, a1 # width
    mv a1, t4 # stride
    j cfl_pred_16bpc_rvv
endfunc

function ipred_cfl_left_16bpc_rvv, export=1, ext=v
    mv t6, a0 # dst
    mv a0, a2 # topleft
    mv t4, a1 # stride
    mv a1, a4 # height
    mv a2, a3 # width
    jal t0, dc_gen_left_16bpc_rvv
    mv a3, a4 # height
    mv a4, a0 # dc_get_top
    mv a1, t4 # stride
    mv a0, t6 # dst
    j cfl_pred_16bpc_rvv
endfunc

function ipred_paeth_16bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    li t0, 0
    mv t3, a2
    lhu t1, (a2)
    addi a6, a2, -2
    addi a2, a2, 2
1:
    lhu t2, (a6)
    mv t3, a3
2:
    sub t5, a3, t3
    sh1add t5, t5, a2
    vsetvli t6, t3, e16, m2, ta, ma
    vle16.v v2, (t5)
    vwaddu.vx v4, v2, t2

    vsetvli zero, zero, e32, m4, ta, mu
    vsub.vx v8, v4, t1
    vzext.vf2 v24, v2
    vsub.vx v12, v8, t1
    vmslt.vx v0, v12, zero
    vneg.v v12, v12, v0.t
    vsub.vx v16, v8, t2
    vmslt.vx v0, v16, zero
    vneg.v v16, v16, v0.t
    vsub.vv v20, v8, v24
    vmslt.vx v0, v20, zero
    vneg.v v20, v20, v0.t

    sub t5, a3, t3
    vmsleu.vv v4, v16, v20
    vmsleu.vv v5, v16, v12
    vmsgtu.vv v0, v20, v12
    vmand.mm v6, v4, v5

    vsetvli zero, zero, e16, m2, ta, ma
    vmerge.vxm v8, v2, t1, v0
    vmmv.m v0, v6
    sh1add t5, t5, a0
    sub t3, t3, t6
    vmerge.vxm v4, v8, t2, v0

    vse16.v v4, (t5)

    bnez t3, 2b

    addi a4, a4, -1
    addi a6, a6, -2
    add a0, a0, a1
    bnez a4, 1b
    ret
endfunc

function ipred_smooth_16bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    la t0, dav1d_sm_weights
    add t1, t0, a3
    sh1add t2, a3, a2
    slli t3, a4, 1
    add t0, t0, a4
    lhu t2, (t2)
    sub t3, a2, t3
    addi a6, a2, -2
    addi a2, a2, 2
    lhu t3, (t3)
1:
    mv t6, a3

    lhu a7, (a6)
    lbu t4, (t0)
2:
    li a5, 256
    vsetvli t5, t6, e16, m2, ta, ma
    vle8.v v2, (t1)
    add t1, t1, t5
    vle16.v v4, (a2)
    sh1add a2, t5, a2
    sub a5, a5, t4

    vwmul.vx v8, v4, t4
    mul a5, a5, t3

    vsetvli zero, zero, e32, m4, ta, ma
    vadd.vx v4, v8, a5

    li a5, 256
    vzext.vf4 v12, v2
    vmul.vx v8, v12, a7

    vrsub.vx v12, v12, a5
    vmacc.vx v8, t2, v12
    vadd.vv v12, v4, v8
    vsetvli zero, zero, e32, m4, ta, ma

    sub a5, a3, t6
    sub t6, t6, t5
    sh1add a5, a5, a0
    vsetvli zero, zero, e16, m2, ta, ma
    vnclipu.wi v2, v12, 9
    vse16.v v2, (a5)

    bnez t6, 2b

    sub t1, t1, a3
    slli t6, a3, 1
    add a0, a0, a1
    sub a2, a2, t6
    addi a4, a4, -1
    addi t0, t0, 1
    addi a6, a6, -2
    bnez a4, 1b

    ret
endfunc

function ipred_smooth_v_16bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    la t0, dav1d_sm_weights
    slli t3, a4, 1
    add t0, t0, a4
    sub t3, a2, t3
    addi a2, a2, 2
    lhu t3, (t3)
1:
    mv t6, a3

    lbu t4, (t0)
2:
    li a5, 256
    vsetvli t5, t6, e16, m2, ta, ma
    vle16.v v4, (a2)
    sh1add a2, t5, a2
    sub a5, a5, t4

    vwmul.vx v8, v4, t4
    mul a5, a5, t3

    vsetvli zero, zero, e32, m4, ta, ma
    vadd.vx v4, v8, a5
    vsetvli zero, zero, e32, m4, ta, ma

    sub a5, a3, t6
    sub t6, t6, t5
    sh1add a5, a5, a0
    vsetvli zero, zero, e16, m2, ta, ma
    vnclipu.wi v2, v4, 8
    vse16.v v2, (a5)

    bnez t6, 2b

    slli t6, a3, 1
    add a0, a0, a1
    sub a2, a2, t6
    addi a4, a4, -1
    addi t0, t0, 1
    bnez a4, 1b

    ret
endfunc

function ipred_smooth_h_16bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    la t0, dav1d_sm_weights
    add t1, t0, a3
    sh1add t2, a3, a2
    lhu t2, (t2)
    addi a6, a2, -2
1:
    mv t6, a3

    lhu a7, (a6)
2:
    vsetvli t5, t6, e16, m2, ta, ma
    vle8.v v2, (t1)
    add t1, t1, t5

    li a5, 256
    vsetvli zero, zero, e32, m4, ta, ma
    vzext.vf4 v12, v2
    vmul.vx v8, v12, a7

    vrsub.vx v12, v12, a5
    vmacc.vx v8, t2, v12

    sub a5, a3, t6
    sub t6, t6, t5
    sh1add a5, a5, a0
    vsetvli zero, zero, e16, m2, ta, ma
    vnclipu.wi v2, v8, 8
    vse16.v v2, (a5)

    bnez t6, 2b

    sub t1, t1, a3
    add a0, a0, a1
    addi a4, a4, -1
    addi a6, a6, -2
    bnez a4, 1b

    ret
endfunc

function pal_pred_16bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    vsetivli t5, 8, e16, m1, ta, ma
    vle16.v v30, (a2)
    li t0, 4
    srli t1, a4, 1
    li t2, 1
1:
    mv t4, a4
2:
    vsetvli t5, t1, e8, mf2, ta, ma
    vle8.v v0, (a3)
    add a3, a3, t5
    vand.vi v1, v0, 7
    sub t6, a4, t4
    vsrl.vi v2, v0, 4
    vwmul.vx v4, v1, t2
    vwmul.vx v6, v2, t2
    vsetvli zero, zero, e16, m1, ta, ma
    sh1add t6, t6, a0
    vrgather.vv v8, v30, v4
    addi t3, t6, 2
    vrgather.vv v10, v30, v6
    slli t5, t5, 1
    vsse16.v v8, (t6), t0
    vsse16.v v10, (t3), t0

    sub t4, t4, t5
    bnez t4, 2b
    add a0, a0, a1
    addi a5, a5, -1
    bnez a5, 1b
    ret
endfunc
