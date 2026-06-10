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

// void ipred_v_8bpc_rvv(pixel *dst, const ptrdiff_t stride,
//                       const pixel *const topleft,
//                       const int width, const int height, const int a,
//                       const int max_width, const int max_height)
function ipred_v_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    addi a2, a2, 1
    vsetvli t0, a3, e8, m1, ta, ma
    bne t0, a3, 3f  // Go to slow path - whole row doesn't fit in regsiter
1:
    // Fast path - row fits in register
    add t1, a0, a1
    vle8.v v4, (a2)
2:
    vse8.v v4, (a0)
    sh1add a0, a1, a0
    vse8.v v4, (t1)
    sh1add t1, a1, t1
    addi a4, a4, -2
    bnez a4, 2b
    ret

    // Row doesn't fit in register.
3:
    vsetvli t0, a3, e8, m2, ta, ma  // Try using 2 registers at once (LMUL=2)
    beq t0, a3, 1b  // Back to fast path - now it fits

4:
    // No need for more vsetli, since both width and VLEN are power of 2, so there is no tail.
    vle8.v v4, (a2)
    mv t2, a0
    mv t1, a4
5:
    vse8.v v4, (t2)
    add t2, t2, a1
    addi t1, t1, -1
    bnez t1, 5b  // Loop over rows.

    sub a3, a3, t0
    add a2, a2, t0
    add a0, a0, t0
    bnez a3, 4b  // Loop over columns

    ret
endfunc


function dc_gen_8bpc_rvv, export=1, ext="v,zbb"
    .variant_cc dav1d_dc_gen_8bpc_rvv
    add t1, a1, a2
    srli t5, t1, 1
    mv t1, a1
    addi t2, a0, 1
    vsetvli zero, t1, e16, m4, ta, ma
    vmv.v.x v0, zero
1:
    vsetvli t3, t1, e8, m2, tu, ma
    vle8.v v4, (t2)
    vwaddu.wv v0, v0, v4

    sub t1, t1, t3
    add t2, t2, t3
    bnez t1, 1b

    mv t1, a2
    mv t2, a0
    vsetvli zero, t1, e16, m4, ta, ma
    vmv.v.x v8, zero
2:
    vsetvli t3, t1, e8, m2, tu, ma
    sub t2, t2, t3
    vle8.v v4, (t2)
    vwaddu.wv v8, v8, v4
    sub t1, t1, t3

    bnez t1, 2b

    vsetvli zero, zero, e32, m8, ta, ma
    vmv.s.x v16, t5
    vmv.s.x v12, zero
    vsetvli zero, a1, e16, m4, ta, ma
    vwredsum.vs v24, v0, v16
    vsetvli zero, a2, e16, m4, ta, ma
    vwredsum.vs v16, v8, v12
    vsetvli zero, zero, e32, m8, ta, ma
    vmv.x.s t5, v24
    vmv.x.s t1, v16
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

    li t1, 0x5556
    j 4f
3:
    li t1, 0x3334
4:
    mul a0, a0, t1
    srli a0, a0, 16
5:
    jr t0
endfunc

function dc_gen_top_8bpc_rvv, export=1, ext="v,zbb"
    .variant_cc dav1d_dc_gen_top_8bpc_rvv
    mv t1, a1
    srli t5, a1, 1
    addi a0, a0, 1
    vsetvli zero, t1, e16, m4, ta, ma
    vmv.v.x v0, zero
1:
    vsetvli t3, t1, e8, m2, tu, ma
    vle8.v v4, (a0)
    vwaddu.wv v0, v0, v4
    sub t1, t1, t3

    add a0, a0, t3
    bnez t1, 1b
    j dc_gen_sum_up_8bpc_rvv
endfunc

function dc_gen_left_8bpc_rvv, export=1, ext="v,zbb"
    .variant_cc dav1d_dc_gen_left_8bpc_rvv
    mv t1, a1
    srli t5, a1, 1
    vsetvli t2, t1, e16, m4, ta, ma
    vmv.v.x v0, zero

1:
    vsetvli t3, t1, e8, m2, tu, ma
    sub a0, a0, t3
    vle8.v v4, (a0)
    vwaddu.wv v0, v0, v4
    sub t1, t1, t3
    bnez t1, 1b

    j dc_gen_sum_up_8bpc_rvv
endfunc

function dc_gen_sum_up_8bpc_rvv, export=1, ext="v,zbb"
    .variant_cc dav1d_dc_gen_sum_up_8bpc_rvv
    vsetvli zero, a1, e32, m8, ta, ma
    vmv.s.x v4, t5
    vsetvli zero, zero, e16, m4, ta, ma
    vwredsum.vs v8, v0, v4
    vsetvli zero, zero, e32, m8, ta, ma
    vmv.x.s t5, v8

    ctz t1, a1

    srl a0, t5, t1
    jr t0
endfunc

function cfl_pred_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
1:
    li t2, 0
    mv t3, a2
2:
    vsetvli t0, t3, e16, m2, ta, ma
    add t4, a0, t2
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
    vsetvli zero, zero, e16, m2, ta, ma
    vnclipu.wi v4, v0, 0
    vsetvli zero, zero, e8, m1, ta, ma
    vnclipu.wi v0, v4, 0
    vse8.v v0, (t4)
    add t2, t0, t2
    sub t3, t3, t0
    bnez t3, 2b
    addi a3, a3, -1
    add a0, a0, a1

    bnez a3, 1b
    ret
endfunc

function ipred_cfl_8bpc_rvv, export=1, ext=v
    mv t6, a0 # dst
    mv a0, a2 # topleft
    mv t4, a1 # stride
    mv a1, a3 # width
    mv a2, a4 # height
    jal t0, dc_gen_8bpc_rvv
    mv a2, a3 # width
    mv a3, a4 # height
    mv a4, a0 # dc_get_top
    mv a0, t6 # dst
    mv a1, t4 # stride
    j cfl_pred_8bpc_rvv
endfunc

function ipred_cfl_128_8bpc_rvv, export=1, ext="v,zba"
    # dc = 128, then just rearrange registers
    mv a2, a3
    mv a3, a4
    li a4, 128

    j cfl_pred_8bpc_rvv
endfunc

function ipred_cfl_top_8bpc_rvv, export=1, ext=v
    mv t6, a0 # dst
    mv a0, a2 # topleft
    mv t4, a1 # stride
    mv a1, a3 # width
    jal t0, dc_gen_top_8bpc_rvv
    mv a3, a4 # height
    mv a4, a0 # dc_get_top
    mv a0, t6 # dst
    mv a2, a1 # width
    mv a1, t4 # stride
    j cfl_pred_8bpc_rvv
endfunc

function ipred_cfl_left_8bpc_rvv, export=1, ext="v,zba"
    mv t6, a0 # dst
    mv a0, a2 # topleft
    mv t4, a1 # stride
    mv a1, a4 # height
    mv a2, a3 # width
    jal t0, dc_gen_left_8bpc_rvv
    mv a3, a4 # height
    mv a4, a0 # dc_get_left
    mv a1, t4 # stride
    mv a0, t6 # dst
    j cfl_pred_8bpc_rvv
endfunc

function ipred_paeth_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    li t0, 0
    mv t3, a2
    lbu t1, (a2)
    addi a6, a2, -1
    addi a2, a2, 1
1:
    lbu t2, (a6)
    mv t3, a3
2:
    sub t5, a3, t3
    add t5, a2, t5
    vsetvli t6, t3, e8, m1, ta, ma
    vle8.v v2, (t5)
    vwaddu.vx v4, v2, t2
    vsetvli zero, zero, e16, m2, ta, ma
    vwsub.vx v8, v4, t1

    vsetvli zero, zero, e32, m4, ta, mu
    vzext.vf4 v24, v2
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

    vsetvli zero, zero, e8, m1, ta, ma
    vmerge.vxm v8, v2, t1, v0
    vmmv.m v0, v6
    add t5, a0, t5
    sub t3, t3, t6
    vmerge.vxm v4, v8, t2, v0

    vse8.v v4, (t5)

    bnez t3, 2b

    addi a4, a4, -1
    addi a6, a6, -1
    add a0, a0, a1
    bnez a4, 1b
    ret
endfunc

function ipred_smooth_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    la t0, dav1d_sm_weights
    add t1, t0, a3
    add t2, a2, a3
    add t0, t0, a4
    lbu t2, (t2)
    sub t3, a2, a4
    addi a6, a2, -1
    addi a2, a2, 1
    lbu t3, (t3)
1:
    mv t6, a3

    lbu a7, (a6)
    lbu t4, (t0)
2:
    li a5, 256
    vsetvli t5, t6, e8, m1, ta, ma
    vle8.v v2, (t1)
    add t1, t1, t5
    vle8.v v4, (a2)
    add a2, a2, t5
    sub a5, a5, t4

    vwmulu.vx v8, v4, t4
    vsetvli zero, zero, e16, m2, ta, ma
    mul a5, a5, t3

    vadd.vx v4, v8, a5
    vsetvli zero, zero, e8, m1, ta, ma
    vwmulu.vx v8, v2, a7

    vneg.v v12, v2
    vwmaccu.vx v8, t2, v12
    vsetvli zero, zero, e16, m2, ta, ma
    vwaddu.vv v12, v4, v8

    sub a5, a3, t6
    sub t6, t6, t5
    add a5, a5, a0
    vnclipu.wi v2, v12, 9
    vsetvli zero, zero, e8, m1, ta, ma
    vnclipu.wi v0, v2, 0
    vse8.v v0, (a5)

    bnez t6, 2b

    sub t1, t1, a3
    add a0, a0, a1
    sub a2, a2, a3
    addi a4, a4, -1
    addi t0, t0, 1
    addi a6, a6, -1
    bnez a4, 1b

    ret
endfunc

function ipred_smooth_v_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    la t0, dav1d_sm_weights
    add t2, a2, a3
    add t0, t0, a4
    sub t3, a2, a4
    addi a2, a2, 1
    lbu t3, (t3)
1:
    mv t6, a3

    lbu t4, (t0)
2:
    li a5, 256
    vsetvli t5, t6, e8, m1, ta, ma
    vle8.v v4, (a2)
    add a2, a2, t5
    sub a5, a5, t4

    vwmulu.vx v8, v4, t4
    vsetvli zero, zero, e16, m2, ta, ma
    mul a5, a5, t3
    vwaddu.vx v4, v8, a5

    sub a5, a3, t6
    sub t6, t6, t5
    add a5, a5, a0
    vsetvli zero, zero, e16, m2, ta, ma
    vnclipu.wi v2, v4, 8
    vsetvli zero, zero, e8, m1, ta, ma
    vnclipu.wi v0, v2, 0
    vse8.v v0, (a5)

    bnez t6, 2b

    add a0, a0, a1
    sub a2, a2, a3
    addi a4, a4, -1
    addi t0, t0, 1
    bnez a4, 1b

    ret
endfunc

function ipred_smooth_h_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    la t0, dav1d_sm_weights
    add t1, t0, a3
    add t2, a2, a3
    lbu t2, (t2)
    addi a6, a2, -1
1:
    mv t6, a3

    lbu a7, (a6)
2:
    vsetvli t5, t6, e8, m1, ta, ma
    vle8.v v2, (t1)
    add t1, t1, t5

    vwmulu.vx v8, v2, a7

    vneg.v v12, v2
    vwmaccu.vx v8, t2, v12

    sub a5, a3, t6
    sub t6, t6, t5
    add a5, a5, a0
    vsetvli zero, zero, e8, m1, ta, ma
    vnclipu.wi v0, v8, 8
    vse8.v v0, (a5)

    bnez t6, 2b

    sub t1, t1, a3
    add a0, a0, a1
    addi a4, a4, -1
    addi a6, a6, -1
    bnez a4, 1b

    ret
endfunc

function pal_pred_8bpc_rvv, export=1, ext="v,zba"
    csrw vxrm, zero
    vsetivli t5, 8, e8, m1, ta, ma
    vle8.v v30, (a2)
    li t0, 2
    srli t1, a4, 1
1:
    mv t4, a4
2:
    vsetvli t5, t1, e8, m1, ta, ma
    vle8.v v0, (a3)
    add a3, a3, t5
    vsrl.vi v2, v0, 4
    sub t6, a4, t4
    vand.vi v1, v0, 7
    add t6, a0, t6
    vrgather.vv v3, v30, v1
    addi t2, t6, 1
    vrgather.vv v4, v30, v2
    slli t5, t5, 1
    vsse8.v v3, (t6), t0
    sub t4, t4, t5
    vsse8.v v4, (t2), t0

    bnez t4, 2b
    addi a5, a5, -1
    add a0, a0, a1
    bnez a5, 1b
    ret
endfunc
