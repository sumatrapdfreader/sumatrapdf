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
#include "src/riscv/64/cdef_common.S"

.macro constrain_vectors vec1, vec2, vec_sub, strength, shift, vec_tmp1, vec_tmp2
    vmslt.vx v0, \vec_tmp1, zero
    vneg.v \vec_tmp1, \vec_tmp1, v0.t
    vmmv.m v1, v0

    vmslt.vx v0, \vec_tmp2, zero
    vneg.v \vec_tmp2, \vec_tmp2, v0.t

    vsra.vx \vec1, \vec_tmp1, \shift
    vsra.vx \vec2, \vec_tmp2, \shift

    vrsub.vx \vec1, \vec1, \strength
    vrsub.vx \vec2, \vec2, \strength

    vmax.vx \vec1, \vec1, zero
    vmax.vx \vec2, \vec2, zero

    vmin.vv \vec_tmp1, \vec1, \vec_tmp1
    vmin.vv \vec_tmp2, \vec2, \vec_tmp2

    vneg.v \vec_tmp2, \vec_tmp2, v0.t

    vmmv.m v0, v1
    vneg.v \vec_tmp1, \vec_tmp1, v0.t
.endm

.macro padding_fn w, h
    li t5, -32768 # INT16_MIN

    andi t4, a7, 4
    li t2, -2 # y_start

.if \w == 4
    vsetivli zero, \w + 4, e16, m1, ta, ma
.else
    vsetivli zero, \w + 4, e16, m2, ta, ma
.endif
    vmv.v.x v0, t5
    bnez t4, L(top_done_\w\()x\h)

    slli t5, a1, 1
    addi t5, t5, 2
    slli t5, t5, 1
    sub t5, a0, t5

    sh1add t4, a1, t5
    vse16.v v0, (t5)
    vse16.v v0, (t4)
    li t2, 0

L(top_done_\w\()x\h):
    andi t4, a7, 8
    li t3, 2 + \h # y_end
    bnez t4, L(bottom_done_\w\()x\h)

    li t5, \h
    mul t5, a1, t5
    addi t5, t5, -2
    sh1add t5, t5, a0

    sh1add t4, a1, t5
    vse16.v v0, (t5)
    vse16.v v0, (t4)
    addi t3, t3, -2

L(bottom_done_\w\()x\h):
    andi t4, a7, 1
    li t0, -2 # x_start

.if \w == 4
    vsetivli zero, 2, e16, m1, ta, ma
.else
    vsetivli zero, 2, e16, m2, ta, ma
.endif

    bnez t4, L(left_done_\w\()x\h)

    mul t5, a1, t2
    addi t5, t5, -2
    sh1add t5, t5, a0

    sub t0, t3, t2

3:
    vse16.v v0, (t5)
    sh1add t5, a1, t5
    addi t0, t0, -1
    bnez t0, 3b

L(left_done_\w\()x\h):

    andi t4, a7, 2
    li t1, 2 + \w # x_end
    bnez t4, L(right_done_\w\()x\h)

    mul t5, t2, a1
    addi t5, t5, \w
    sh1add t5, t5, a0

    sub t1, t3, t2

4:
    vse16.v v0, (t5)
    sh1add t5, a1, t5
    addi t1, t1, -1
    bnez t1, 4b

    li t1, \w

L(right_done_\w\()x\h):

    beqz t2, L(top_skip_\w\()x\h)

    mul t5, a1, t2
    add t5, t0, t5
    sh1add a0, t5, a0 # tmp += y_start * tmp_stride + x_start
    add a5, a5, t0

    sub t5, t1, t0 # x_end - x_start
    slli t6, t0, 1
.if \w == 4
    vsetvli zero, t5, e16, m1, ta, ma
.else
    vsetvli zero, t5, e16, m2, ta, ma
.endif

5:
    vle8.v v0, (a5)
    addi t2, t2, 1
    vzext.vf2 v2, v0
    add a5, a3, a5
    vse16.v v2, (a0)
    sh1add a0, a1, a0
    bnez t2, 5b

    sub a0, a0, t6 # tmp -= x_start

L(top_skip_\w\()x\h):

    li a5, \h
    beqz t0, L(left_skip_\w\()x\h)

    sh1add a0, t0, a0 # tmp += x_start

7:
.if \w == 4
    vsetivli zero, 2, e16, m1, ta, ma
.else
    vsetivli zero, 2, e16, m2, ta, ma
.endif

    vle8.v v0, (a4)
    addi a5, a5, -1
    vzext.vf2 v2, v0
    addi a4, a4, 2
    vse16.v v2, (a0)
    sh1add a0, a1, a0
    bnez a5, 7b

    li a5, \h
    mul t5, a1, a5
    add t5, t5, t0
    slli t5, t5, 1
    sub a0, a0, t5 # tmp -= h * tmp_stride + x_start

L(left_skip_\w\()x\h):

8:
.if \w == 4
    vsetvli zero, t1, e16, m1, ta, ma
.else
    vsetvli zero, t1, e16, m2, ta, ma
.endif

    vle8.v v0, (a2)
    vzext.vf2 v2, v0
    vse16.v v2, (a0)
    add a2, a3, a2
    sh1add a0, a1, a0
    addi a5, a5, -1
    bnez a5, 8b


    li a5, \h
    sh1add a0, t0, a0 # tmp += x_start
    add a6, a6, t0 # bottom += x_start
    beq a5, t3, L(bottom_skip_\w\()x\h)

    sub t5, t1, t0
.if \w == 4
    vsetvli zero, t5, e16, m1, ta, ma
.else
    vsetvli zero, t5, e16, m2, ta, ma
.endif

9:
    vle8.v v0, (a6)
    add a6, a3, a6
    vzext.vf2 v2, v0
    addi a5, a5, 1
    vse16.v v2, (a0)
    sh1add a0, a1, a0
    bne a5, t3, 9b

L(bottom_skip_\w\()x\h):
    li t6, \h
    mul t6, a3, t6
    sub a2, a2, t6 # src -= h * src_stride
    mul t5, a1, t3
    add t5, t5, t0
    slli t5, t5, 1
    sub a0, a0, t5 # tmp -= y_end * tmp_stride + x_start
.endm


.macro cdef_fn w, h
function cdef_filter_\w\()x\h\()_8bpc_rvv, export=1, ext="v,zba,zbb"
    csrw vxrm, zero

    addi sp, sp, -32 - 144*2
    sd a5, 24(sp) # pri_strength
    sd a6, 16(sp) # sec_strength
    sd a7, 8(sp) # dir


    ld a7, 8 + 32 + 144*2(sp) # edges
    mv a6, a4 # bottom
    mv a5, a3 # top
    mv a4, a2 # left
    mv a3, a1 # dst_stride
    mv a2, a0 # dst
    li a1, 12 # tmp_stride
    addi a0, sp, 32 + 2*(2*12+2)
    padding_fn \w, \h

    ld a4, 32 + 2*144(sp) # damping
    ld a5, 24(sp) # pri_strength
    ld a6, 16(sp) # sec_strength
    ld a7, 8(sp) # dir

    beqz a5, cdef_filter_sec_only_\w\()x\h

    bnez a6, cdef_filter_pri_sec_\w\()x\h

    andi t0, a5, 1
    li t1, 4
    sub t4, t1, t0

    li t1, 63
    clz t2, a5
    sub t1, t1, t2
    sub t1, a4, t1

    li t0, \h

    la t2, dav1d_cdef_directions
    addi t3, a7, 2
    sh1add t2, t3, t2

    blt zero, t1, 1f
    mv t1, zero
1:
    vsetivli zero, \w, e16, m1, ta, mu

    lb t3, 0(t2)

    vle8.v v0, (a2)
    vzext.vf2 v2, v0

    sh1add t6, t3, a0
    slli t3, t3, 1
    sub t3, a0, t3

    vle16.v v4, (t6)
    vle16.v v6, (t3)

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a5, t1, v8, v16

    vmul.vx v28, v16, t4
    vmacc.vx v28, t4, v8

    lb t3, 1(t2)

    andi t5, t4, 3
    ori t5, t5, 2

    sh1add t6, t3, a0
    slli t3, t3, 1
    sub t3, a0, t3

    vsetvli zero, zero, e16, m1, ta, mu

    vle16.v v4, (t6)
    vle16.v v6, (t3)

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a5, t1, v8, v16

    vmacc.vx v28, t5, v16
    vmacc.vx v28, t5, v8

    vmslt.vx v0, v28, zero
    vadd.vi v28, v28, -1, v0.t

    vsetvli zero, zero, e16, m1, ta, ma

    vnclip.wi v24, v28, 4

    vadd.vv v28, v2, v24

    vsetvli zero, zero, e8, mf2, ta, ma

    vnclipu.wi v24, v28, 0

    vse8.v v24, (a2)

    addi t0, t0, -1
    add a2, a2, a3
    sh1add a0, a1, a0

    bnez t0, 1b

    addi sp, sp, 32 + 144*2
    ret

cdef_filter_sec_only_\w\()x\h:
    li t1, 63
    clz t2, a6
    sub t1, t1, t2
    sub t1, a4, t1

    li t0, \h

    la t2, dav1d_cdef_directions
    addi t3, a7, 4
    sh1add t3, t3, t2
    sh1add t2, a7, t2

2:
    vsetivli zero, \w, e16, m1, ta, mu

    lb t4, 0(t3)
    lb t5, 0(t2)

    vle8.v v0, (a2)
    vzext.vf2 v2, v0

    sh1add t6, t4, a0
    slli t4, t4, 1
    sub t4, a0, t4

    vle16.v v4, (t6)
    vle16.v v6, (t4)

    sh1add t4, t5, a0
    slli t5, t5, 1
    sub t5, a0, t5

    vle16.v v8, (t4)
    vle16.v v10, (t5)

    vwsub.vv v12, v4, v2
    vwsub.vv v14, v6, v2
    vwsub.vv v16, v8, v2
    vwsub.vv v18, v10, v2

    vsetvli zero, zero, e32, m2, ta, mu

    li t4, 2
    constrain_vectors v4, v6, v12, a6, t1, v12, v14
    constrain_vectors v8, v10, v14, a6, t1, v16, v18

    vmul.vx v28, v18, t4
    vmacc.vx v28, t4, v16
    vmacc.vx v28, t4, v14
    vmacc.vx v28, t4, v12


    lb t4, 1(t3)
    lb t5, 1(t2)

    sh1add t6, t4, a0
    slli t4, t4, 1
    sub t4, a0, t4

    vsetvli zero, zero, e16, m1, ta, mu

    vle16.v v4, (t6)
    vle16.v v6, (t4)

    sh1add t4, t5, a0
    slli t5, t5, 1
    sub t5, a0, t5

    vle16.v v8, (t4)
    vle16.v v10, (t5)

    vwsub.vv v12, v4, v2
    vwsub.vv v14, v6, v2
    vwsub.vv v16, v8, v2
    vwsub.vv v18, v10, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a6, t1, v12, v14
    constrain_vectors v8, v10, v14, a6, t1, v16, v18

    vadd.vv v4, v28, v12
    vadd.vv v28, v4, v14
    vadd.vv v4, v28, v16
    vadd.vv v28, v4, v18

    vmslt.vx v0, v28, zero
    vadd.vi v28, v28, -1, v0.t

    vsetvli zero, zero, e16, m1, ta, ma

    vnclip.wi v24, v28, 4

    vadd.vv v28, v2, v24

    vsetvli zero, zero, e8, mf2, ta, ma

    vnclipu.wi v24, v28, 0

    vse8.v v24, (a2)

    addi t0, t0, -1
    add a2, a2, a3
    sh1add a0, a1, a0

    bnez t0, 2b

    addi sp, sp, 32 + 144*2
    ret
cdef_filter_pri_sec_\w\()x\h:

    li t1, 63
    clz t2, a5
    clz t3, a6
    sub t2, t1, t2
    sub t3, t1, t3
    sub t1, a4, t2
    sub t2, a4, t3

    li t0, \h

    la t3, dav1d_cdef_directions

    blt zero, t1, 3f
    mv t1, zero
3:
    vsetivli zero, \w, e16, m1, ta, ma

    li t4, 4
    andi t6, a5, 1
    addi t5, a7, 2
    sub t4, t4, t6

    sh1add t5, t5, t3

    vle8.v v0, (a2)

    lb t6, 0(t5)

    vzext.vf2 v2, v0

    sh1add a4, t6, a0
    slli t6, t6, 1
    sub t6, a0, t6

    vle16.v v4, (a4)
    vle16.v v6, (t6)

    vminu.vv v20, v4, v2
    vmax.vv v24, v4, v2
    vminu.vv v20, v6, v20
    vmax.vv v24, v6, v24

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a5, t1, v8, v16

    vmul.vx v28, v16, t4
    vmacc.vx v28, t4, v8

    lb t6, 1(t5)

    andi t4, t4, 3
    ori t4, t4, 2


    sh1add a4, t6, a0
    slli t6, t6, 1
    sub t6, a0, t6

    vsetvli zero, zero, e16, m1, ta, ma

    vle16.v v4, (a4)
    vle16.v v6, (t6)

    vminu.vv v20, v4, v20
    vmax.vv v24, v4, v24
    vminu.vv v20, v6, v20
    vmax.vv v24, v6, v24

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a5, t1, v8, v16

    addi t5, a7, 4
    vmacc.vx v28, t4, v16
    vmacc.vx v28, t4, v8

    sh1add t5, t5, t3

    lb t6, 0(t5)

    sh1add a4, t6, a0
    slli t6, t6, 1
    sub t6, a0, t6

    vsetvli zero, zero, e16, m1, ta, ma

    vle16.v v4, (a4)
    vle16.v v6, (t6)

    vminu.vv v20, v4, v20
    vmax.vv v24, v4, v24
    vminu.vv v20, v6, v20
    vmax.vv v24, v6, v24

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    li t6, 2
    constrain_vectors v4, v6, v12, a6, t2, v8, v16

    vmacc.vx v28, t6, v16
    vmacc.vx v28, t6, v8

    lb t6, 1(t5)

    sh1add a4, t6, a0
    slli t6, t6, 1
    sub t6, a0, t6

    vsetvli zero, zero, e16, m1, ta, ma

    vle16.v v4, (a4)
    vle16.v v6, (t6)

    vminu.vv v20, v4, v20
    vmax.vv v24, v4, v24
    vminu.vv v20, v6, v20
    vmax.vv v24, v6, v24

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a6, t2, v8, v16

    sh1add t5, a7, t3

    vadd.vv v4, v28, v8
    vadd.vv v28, v4, v16

    vsetvli zero, zero, e16, m1, ta, ma

    lb t6, 0(t5)

    sh1add a4, t6, a0
    slli t6, t6, 1
    sub t6, a0, t6

    vle16.v v4, (a4)
    vle16.v v6, (t6)

    vminu.vv v20, v4, v20
    vmax.vv v24, v4, v24
    vminu.vv v20, v6, v20
    vmax.vv v24, v6, v24

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    li t6, 2
    constrain_vectors v4, v6, v12, a6, t2, v8, v16

    vmacc.vx v28, t6, v16
    vmacc.vx v28, t6, v8

    lb t6, 1(t5)

    sh1add a4, t6, a0
    slli t6, t6, 1
    sub t6, a0, t6

    vsetvli zero, zero, e16, m1, ta, ma

    vle16.v v4, (a4)
    vle16.v v6, (t6)

    vminu.vv v20, v4, v20
    vmax.vv v24, v4, v24
    vminu.vv v20, v6, v20
    vmax.vv v24, v6, v24

    vwsub.vv v8, v4, v2
    vwsub.vv v16, v6, v2

    vsetvli zero, zero, e32, m2, ta, mu

    constrain_vectors v4, v6, v12, a6, t2, v8, v16

    vadd.vv v4, v28, v8
    vadd.vv v28, v4, v16

    vmslt.vx v0, v28, zero
    vadd.vi v28, v28, -1, v0.t

    vsetvli zero, zero, e16, m1, ta, mu

    vnclip.wi v16, v28, 4

    vadd.vv v28, v2, v16

    vmslt.vv v0, v20, v28
    vmerge.vvm v4, v20, v28, v0

    vmslt.vv v0, v4, v24
    vmerge.vvm v28, v24, v4, v0

    vsetvli zero, zero, e8, mf2, ta, ma

    vnclipu.wi v24, v28, 0

    vse8.v v24, (a2)

    addi t0, t0, -1
    add a2, a2, a3
    sh1add a0, a1, a0

    bnez t0, 3b

    addi sp, sp, 32 + 144*2
    ret
endfunc
.endm

cdef_fn 4, 4
cdef_fn 4, 8
cdef_fn 8, 8

// These lmul_* names are based on vlen=256
.macro cdef_find_dir_fn vlen, lmul_mf2, lmul_m1, lmul_m2

.hidden cdef_find_dir_vl\vlen\()_8bpc_rvv_epilogue
.global cdef_find_dir_vl\vlen\()_8bpc_rvv_epilogue

function cdef_find_dir_vl\vlen\()_8bpc_rvv, export=1, ext="v,zba,zbb"

    // SEW <= LMUL * ELEN
    vsetivli zero, 15, e16, \lmul_m1, ta, ma
    add              t0, a1, a0  # (a0) +   stride
    li               a4, 128

    //Preparing for vslideup + vadd operations
    vmv.v.i          v1, 0
    vmv.v.i          v2, 0
    vmv.v.i          v3, 0
    vmv.v.i          v4, 0
    sh1add           t1, a1, a0  # (a0) + 2*stride
    vmv.v.i          v5, 0
    sh1add           t2, a1, t0  # (a0) + 3*stride
    vmv.v.i          v6, 0
    sh2add           t3, a1, a0  # (a0) + 4*stride
    vmv.v.i          v7, 0
    sh2add           t4, a1, t0  # (a0) + 5*stride

    //Preparing for vslideup + vadd operations
    vmv.v.i          v8, 0
    vmv.v.i          v9, 0
    vmv.v.i         v10, 0
    vmv.v.i         v11, 0
    vmv.v.i         v12, 0
    sh2add           t5, a1, t1  # (a0) + 6*stride
    vmv.v.i         v13, 0
    sh2add           t6, a1, t2  # (a0) + 7*stride
    vmv.v.i         v14, 0
    vmv.v.i         v15, 0
    vmv.v.i         v31, 0

    // Load all pixels of img

    // Tail Undisturb is set. Else the pattern we are
    // trying to set gets clobbered
    .if \vlen == 1024
    vsetivli    zero, 8, e8, mf8, tu, ma
    //The normal route gave an Illegal inst.
    // SEW <= LMUL * ELEN
    vle8.v          v31, (a0)
    vle8.v           v1, (t0)
    vle8.v           v2, (t1)
    vle8.v           v3, (t2)
    vle8.v           v4, (t3)
    vle8.v           v5, (t4)
    vle8.v           v6, (t5)
    vle8.v           v7, (t6)

    vsetivli    zero, 8, e16, \lmul_mf2, tu, ma
    vid.v            v0
    .else

    vsetivli    zero, 8, e16, \lmul_mf2, tu, ma

    vid.v            v0

    vle8.v          v31, (a0)
    vmv.v.i         v24, 0
    vle8.v           v1, (t0)
    vmv.v.i         v25, 0
    vle8.v           v2, (t1)
    vmv.v.i         v26, 0
    vle8.v           v3, (t2)
    vmv.v.i         v27, 0
    vle8.v           v4, (t3)
    vmv.v.i         v28, 0
    vle8.v           v5, (t4)
    vmv.v.i         v29, 0
    vle8.v           v6, (t5)
    vmv.v.i         v30, 0
    vle8.v           v7, (t6)
    .endif

    // vmv.v.i is interleaved with vle8
    // if there remains a separate execution unit for vmv.v.i
    // it will do both in parallel

    // Zero v24-v30
    // Will always have some of its position as zero
    // for efficient vslideup and vadd operations
    // The registers will look like the following
    // after slideup instructions
    //
    //   first_reg [xxxxxxx........]
    //         v24 [.xxxxxxxx......]
    //         v25 [..xxxxxxxx.....]
    //         v26 [...xxxxxxxx....]
    //         v27 [....xxxxxxxx...]
    //         v28 [.....xxxxxxxx..]
    //         v29 [.......xxxxxxx.]
    //         v30 [........xxxxxxx]

    // Only zeroed first 8 elements as
    // last 8 are already zeroed in the beginning

    .if \vlen == 1024
    vzext.vf2        v8, v31
    vmv.v.i         v24, 0
    vzext.vf2        v9,  v1
    vmv.v.i         v25, 0
    vzext.vf2       v10,  v2
    vmv.v.i         v26, 0
    vzext.vf2       v11,  v3
    vmv.v.i         v27, 0
    vzext.vf2       v12,  v4
    vmv.v.i         v28, 0
    vzext.vf2       v13,  v5
    vmv.v.i         v29, 0
    vzext.vf2       v14,  v6
    vmv.v.i         v30, 0
    vzext.vf2       v15,  v7
    .endif

    vxor.vi         v0,  v0, 7 # [7, 6, 5, 4, 3, 2, 1, 0]

    vzext.vf2        v8, v31
    vzext.vf2        v9,  v1
    vzext.vf2       v10,  v2
    vzext.vf2       v11,  v3
    vzext.vf2       v12,  v4
    vzext.vf2       v13,  v5
    vzext.vf2       v14,  v6
    vzext.vf2       v15,  v7

cdef_find_dir_vl\vlen\()_8bpc_rvv_epilogue:

    // const int px = (img[x] >> bitdepth_min_8) - 128;
    vsub.vx          v8,   v8, a4
    vsub.vx          v9,   v9, a4
    vsub.vx         v10,  v10, a4
    vsub.vx         v11,  v11, a4
    vsub.vx         v12,  v12, a4
    vsub.vx         v13,  v13, a4
    vsub.vx         v14,  v14, a4
    vsub.vx         v15,  v15, a4

    // Reversing the halfword positions
    vrgather.vv     v31,  v8, v0
    vrgather.vv      v1,  v9, v0
    vrgather.vv      v2, v10, v0
    vrgather.vv      v3, v11, v0
    vrgather.vv      v4, v12, v0
    vrgather.vv      v5, v13, v0
    vrgather.vv      v6, v14, v0
    vrgather.vv      v7, v15, v0

    // Calculating partial_sum_diag[0..1]
    vsetivli    zero, 15, e16, \lmul_m1, ta, ma

    vslideup.vi     v24,  v9, 1
    vslideup.vi     v25, v10, 2
    vslideup.vi     v26, v11, 3
    vslideup.vi     v27, v12, 4
    vslideup.vi     v28, v13, 5
    vslideup.vi     v29, v14, 6
    vslideup.vi     v30, v15, 7

    vadd.vv         v18,  v8, v24
    vadd.vv         v19, v25, v26
    vadd.vv         v20, v27, v28
    vadd.vv         v21, v29, v30

    vslideup.vi     v24,  v1, 1
    vslideup.vi     v25,  v2, 2
    vslideup.vi     v26,  v3, 3
    vslideup.vi     v27,  v4, 4
    vslideup.vi     v28,  v5, 5
    vslideup.vi     v29,  v6, 6
    vslideup.vi     v30,  v7, 7

    vadd.vv         v16, v31, v24
    vadd.vv         v17, v25, v26
    vadd.vv         v22, v27, v28
    vadd.vv         v23, v29, v30

    vadd.vv         v19, v18, v19
    vadd.vv         v21, v20, v21
    vadd.vv         v16, v16, v17
    vadd.vv         v17, v22, v23

    .if \vlen >= 512
    vwadd.vv        v18, v19, v21 # partial_sum_diag[0]
    vwadd.vv        v19, v16, v17 # partial_sum_diag[1]
    .else
    vadd.vv         v18, v19, v21 # partial_sum_diag[0]
    vadd.vv         v19, v16, v17 # partial_sum_diag[1]
    .endif

    // Calculating partial_sum_alt[2..3]
    vsetivli    zero, 11, e16, \lmul_m1, ta, ma

    vadd.vv         v31,  v8,  v9
    vadd.vv          v1, v10, v11
    vadd.vv          v2, v12, v13
    vadd.vv          v3, v14, v15

    vslideup.vi     v24,  v1, 1
    vslideup.vi     v25,  v2, 2
    vslideup.vi     v26,  v3, 3

    addi             sp, sp, -32
    vadd.vv          v4, v31, v24
    vadd.vv          v5, v25, v26

    vslideup.vi     v24,  v2, 1
    vslideup.vi     v25,  v1, 2
    vslideup.vi     v26, v31, 3

    li               a4, 0b01010101 # alt_mask
    vadd.vv          v6,  v3, v24
    vadd.vv          v7, v25, v26

    .if \vlen >= 512
    vwadd.vv        v23,  v4,  v5 # partial_sum_alt[3]
    vwadd.vv        v22,  v6,  v7 # partial_sum_alt[2]
    .else
    vadd.vv         v23,  v4,  v5 # partial_sum_alt[3]
    vadd.vv         v22,  v6,  v7 # partial_sum_alt[2]
    .endif

    // Calculating partial_sum_hv[0..1]
    vsetivli    zero, 8, e16, \lmul_mf2, ta, ma
    li               a5, 105

    vredsum.vs       v0,  v8, v24 # using v24 which is already has a zero
    vredsum.vs       v1,  v9, v24 # on the first element
    vredsum.vs       v2, v10, v24
    vredsum.vs       v3, v11, v24
    vredsum.vs       v4, v12, v24
    vredsum.vs       v5, v13, v24
    vredsum.vs       v6, v14, v24
    vredsum.vs       v7, v15, v24

    vmv.x.s          t0,  v0
    vadd.vv          v0,  v8,  v9
    vmv.x.s          t1,  v1
    vadd.vv          v1, v10, v11
    vmv.x.s          t2,  v2
    vadd.vv          v2, v12, v13
    vmv.x.s          t3,  v3
    vadd.vv          v3, v14, v15
    vmv.x.s          t4,  v4
    vadd.vv          v1,  v0,  v1
    vmv.x.s          t5,  v5
    vadd.vv          v3,  v2,  v3
    vmv.x.s          t6,  v6
    vmv.s.x          v0,  a4      # alt_mask
    vmv.x.s          a7,  v7

    // partial_sum_hv[0] and cost[2] is being calculated in the
    // scalar pipeline

    // Calculating partial_sum_alt[0..1]

    // Finding sum of pairs for
    // position [0,1][2,3][4,5][6,7]

    mul               t0,  t0,  t0
    vslide1down.vx   v31,  v8, zero
    vslide1down.vx    v2, v10, zero
    vslide1down.vx    v4, v12, zero
    vwadd.vv         v17,  v1,  v3 # partial_sum_hv[1]

    mul               t1,  t1,  t1
    vslide1down.vx    v1,  v9, zero
    vslide1down.vx    v3, v11, zero
    vslide1down.vx    v5, v13, zero

    mul               t2,  t2,  t2
    vslide1down.vx    v6, v14, zero
    vslide1down.vx    v7, v15, zero

    vadd.vv          v31, v31,  v8, v0.t

    mul               t3,  t3,  t3
    vadd.vv           v2,  v2, v10, v0.t
    vadd.vv           v4,  v4, v12, v0.t
    vadd.vv           v1,  v1,  v9, v0.t

    mul               t4,  t4,  t4
    vadd.vv           v3,  v3, v11, v0.t
    vadd.vv           v5,  v5, v13, v0.t
    vadd.vv           v6,  v6, v14, v0.t

    mul               t5,  t5,  t5
    vadd.vv           v7,  v7, v15, v0.t

    //Hinting using zero, zero that previous vl did not change
    vsetvli    zero, zero, e16, \lmul_mf2, tu, ma
    // Zero v8-v15 before vslideup and vadd operations
    // Its targetting elements [4:7] to be zeroed
    // [XXXXXXXX........] -> [XXXX0000........] from the start
    // Tail Undisturbed, so that the previous zero sets don't disappear
    // Thus the result should come out like this: [000000000000000.]
    vmv.v.i           v8, 0
    vmv.v.i           v9, 0
    vmv.v.i          v10, 0

    mul               t6,  t6,  t6
    vmv.v.i          v11, 0
    vmv.v.i          v12, 0
    vmv.v.i          v13, 0

    mul               a7,  a7,  a7
    vmv.v.i          v14, 0
    vmv.v.i          v15, 0

    // vcompress.vm will have this pattern now
    // [XXXX00000000000.]
    // v0 has a pattern 10101010
    vcompress.vm      v8, v31, v0

    add               t0,  t0,  t1
    vcompress.vm      v9,  v1, v0
    vcompress.vm     v10,  v2, v0
    vcompress.vm     v11,  v3, v0

    add               t1,  t2,  t3
    vcompress.vm     v12,  v4, v0
    vcompress.vm     v13,  v5, v0
    vcompress.vm     v14,  v6, v0

    add               t2,  t4,  t5
    vcompress.vm     v15,  v7, v0

    vsetivli    zero, 11, e16, \lmul_m1, ta, ma

    vslideup.vi      v24,    v9, 1
    vslideup.vi      v25,   v10, 2

    add              t3,   t6,  a7
    vslideup.vi      v26,   v11, 3
    vslideup.vi      v27,   v12, 4
    vslideup.vi      v28,   v13, 5

    add               t0,  t0,  t1
    vslideup.vi      v29,   v14, 6
    vslideup.vi      v30,   v15, 7

    vadd.vv           v1,  v8, v24

    add               t1,  t2,  t3
    vadd.vv           v2, v25, v26
    vadd.vv           v3, v27, v28
    vadd.vv           v4, v29, v30

    add               t2,  t0,  t1 # cost[2]

    .if \vlen == 256
    vid.v             v0
    .endif

    vadd.vv           v1,  v1,  v2
    vadd.vv           v3,  v3,  v4

    vmv.v.i           v7, 0
    .if \vlen >= 512
    vwadd.vv         v20,  v1,  v3 # partial_sum_alt[0]
    .else
    vadd.vv          v20,  v1,  v3 # partial_sum_alt[0]
    .endif

    .if \vlen >= 512
    // LMUL ≥ SEWMIN/ELEN
    // V Extension implements Zve64d, SEWMIN=8, ELEN=64

    // Tail Undisturb to preserve the zeroes
    vsetivli    zero, 8, e8, mf8, tu, ma
    vid.v             v0
    vxor.vi           v0,  v0, 6   # [6,7,4,5,2,3,0,1]
    vle16.v          v16, (sp)     # partial_sum_hv[0], without widening
    .else
    vsetivli    zero, 4, e16, mf4, tu, ma
    vxor.vi           v0,  v0, 3   # [3, 2, 1, 0]
    .endif

    // Reversiong the halfword positions
    mul               t2,  t2,  a5 # cost[2]
    vrgather.vv       v7,  v8, v0
    vrgather.vv       v8,  v9, v0
    vrgather.vv       v9, v10, v0

    vrgather.vv      v10, v11, v0
    vrgather.vv      v11, v12, v0
    vrgather.vv      v12, v13, v0

    vrgather.vv      v13, v14, v0
    vrgather.vv      v14, v15, v0

    vsetivli    zero, 11, e16, \lmul_m1, ta, ma

    vslideup.vi      v24,  v8, 1
    vslideup.vi      v25,  v9, 2
    vslideup.vi      v26, v10, 3

    vslideup.vi      v27, v11, 4
    vslideup.vi      v28, v12, 5
    vslideup.vi      v29, v13, 6

    vslideup.vi      v30, v14, 7

    vadd.vv           v1,  v7, v24
    vadd.vv           v2, v25, v26
    vadd.vv           v3, v27, v28

    vadd.vv           v4, v29, v30

    la a5, div_table_alt   # Moved away from vle16.v to reduce hazards
    vadd.vv           v1,  v1,  v2
    vadd.vv           v3,  v3,  v4

    vle16.v v29, (a5) # load div_table_alt early

    .if \vlen >= 512
    vwadd.vv         v21,  v1,  v3 # partial_sum_alt[1]
    .else
    vadd.vv          v21,  v1,  v3 # partial_sum_alt[1]
    .endif

    vsetivli    zero, 11, e32, \lmul_m2, ta, ma

    // Completeing calculation of cost[2] using the scalar pipeline

    .if \vlen >= 512
    vmul.vv           v2, v20, v20 # cost[1]
    vmul.vv           v6, v22, v22 # cost[5]
    vsext.vf2        v26, v29      # Avoid load-use hazard by moving it away from load
    vmul.vv           v8, v23, v23 # cost[7]
    vmul.vv           v4, v21, v21 # cost[3]


    .else
    vsext.vf2         v2, v20
    vsext.vf2         v6, v22
    vsext.vf2         v8, v23
    vsext.vf2         v4, v21
    vsext.vf2        v26, v29

    vmul.vv           v2,  v2,  v2 # cost[1]
    vmul.vv           v6,  v6,  v6 # cost[5]
    vmul.vv           v8,  v8,  v8 # cost[7]
    vmul.vv           v4,  v4,  v4 # cost[3]
    .endif

    la a4, div_table_diag # interleaving independent inst. to hide vmul latency

    vmul.vv           v2,  v2, v26
    vmul.vv           v6,  v6, v26
    vmul.vv           v8,  v8, v26
    vmul.vv           v4,  v4, v26

    sw                t2, 8(sp)     # best_cost calculation
    vredsum.vs        v2,  v2, v30  # cost[1]
    vredsum.vs        v6,  v6, v30  # cost[5]
    vredsum.vs        v8,  v8, v30  # cost[7]
    vredsum.vs        v4,  v4, v30  # cost[3]

    // Calculating cost[0], cost[4]
    vsetivli    zero, 15, e32, \lmul_m2, ta, ma
    slli              t2,  t2, 3    # best_cost calculation

    .if \vlen >= 512
    vle16.v   v28, (a4)

    vmul.vv          v10, v18, v18 # cost[0]
    vmul.vv          v12, v19, v19 # cost[4]

    .else
    vsext.vf2        v10, v18
    vsext.vf2        v12, v19
    vle16.v   v28, (a4)

    vmul.vv          v10, v10, v10 # cost[0]
    vmul.vv          v12, v12, v12 # cost[4]
    .endif

    ori               t2,  t2,  5   # best_cost calculation
    // interleaving independent inst. to hide vmul latency
    vmv.x.s           t1,  v2      # cost[1]
    vmv.x.s           t3,  v4      # cost[3]
    vsext.vf2        v24, v28
    vmv.x.s           t5,  v6      # cost[5]
    vmv.x.s           a7,  v8      # cost[7]

    vmul.vv          v10, v10, v24
    vmul.vv          v12, v12, v24

    // interleaving independent inst. to hide vmul latency
    li                a6, 105
    // Store these to find cost[best_dir ^ 4] efficiently
    sw                t1,  4(sp)
    sw                t3, 12(sp)
    sw                t5, 20(sp)
    sw                a7, 28(sp)

    vredsum.vs       v10, v10, v30  # cost[0]
    vredsum.vs       v12, v12, v30  # cost[4]

    .if \vlen == 1024
    vsetivli    zero,  8, e32, mf2, ta, ma # e32 is not supported of mf4
    .else
    vsetivli    zero,  8, e32, \lmul_m1, ta, ma
    .endif

    vmul.vv           v1, v17, v17 # cost[6]

    vmv.x.s           t0, v10      # cost[0]
    slli              t1,  t1, 3
    //slli            t2,  t2, 3 # Completed by interleaving with vector inst.
    slli              t3,  t3, 3
    slli              t5,  t5, 3
    slli              a7,  a7, 3

    vmul.vx           v1, v1, a6

    vmv.x.s           t4, v12      # cost[4]
    ori               t1,  t1,  6
    //ori             t2,  t2,  5  # Completed by interleaving with vector inst.
    ori               t3,  t3,  4
    ori               t5,  t5,  2
    //ori             a7,  a7,  0

    vredsum.vs        v1,  v1, v30 # cost[6]

    sw                t0,  0(sp)
    slli              t0,  t0,  3
    max               t1,  t1, a7
    //sw              t2,  8(sp)   # Completed by interleaving with vector inst.
    sw                t4, 16(sp)
    slli              t4,  t4,  3
    max               t3,  t3, t5

    vmv.x.s           t6,  v1      # cost[6]
    max               t1,  t1, t2

    // Calculating best_cost, best_dir
    // All costs in t0..t6,a7 are left shifted by 3
    // Then, we set their priority using ori
    // The priority is reversed from the position of cost
    // Thus, for cost[0..7], priority -> 7..0

    // Lower 3 priority bits don't affect max because comparison
    // is weighted by higher bits
    ori               t0,  t0,  7
    ori               t4,  t4,  3

    max               t1,  t1, t3

    sw                t6,  24(sp)
    max               t0,  t0, t4

    slli              t6,  t6, 3
    max               t0,  t0, t1

    ori               t6,  t6,  1
    max               t0,  t0, t6

    // Calculating best_dir, best_cost and *var
    andi             a0,  t0,  7  # 7 - best_dir; get last 3 bits
    xori             a4,  a0,  3  # a4 = (7 - best_dir) ^ 3 = best_dir ^ 4
    sh2add           a4,  a4, sp
    lw               a4,  (a4)    # cost[best_dir ^ 4]
    xori             a0,  a0,  7  # best_dir; xor by 0x7 to find complement
    srli             a1,  t0,  3  # best_cost
    addi             sp,  sp, 32
    sub              a1,  a1, a4
    srli             a1,  a1, 10
    sw               a1, (a2)

    ret

endfunc
.endm

cdef_find_dir_fn  256, mf2, m1, m2
cdef_find_dir_fn  512, mf4, mf2, m1
cdef_find_dir_fn 1024, mf4, mf4, mf2

.hidden cdef_find_dir_vl128_8bpc_rvv_epilogue
.global cdef_find_dir_vl128_8bpc_rvv_epilogue

function cdef_find_dir_vl128_8bpc_rvv, export=1, ext="v,zba,zbb"

    // SEW <= LMUL * ELEN
    vsetvli    a7, zero, e16, m1, ta, ma //max amount of elements
    add              t0, a1, a0  # (a0) +   stride
    li               a4, 128

    // Preparing for vslideup + vadd operations
    // Zero v24-v30
    // Will always have some of its position as zero
    // for efficient vslideup and vadd operations
    // The registers will look like the following
    // after slideup instructions

    // Pattern we want to set:
    //  [xxxxxxxx........]
    //  [.xxxxxxxx.......]
    //  [..xxxxxxxx......]
    //  [...xxxxxxxx.....]
    //  [....xxxxxxxx....]
    //  [.....xxxxxxxx...]
    //  [.......xxxxxxx..]
    //  [........xxxxxxx.]

    // Only zeroed first 8 elements as
    // last 8 are already zeroed in the beginning

    // Load all pixels of img
    sh1add           t1, a1, a0  # (a0) + 2*stride
    vmv.v.i          v16, 0
    vle8.v           v0, (a0)
    sh1add           t2, a1, t0  # (a0) + 3*stride
    vmv.v.i          v17, 0
    vle8.v           v1, (t0)
    sh2add           t3, a1, a0  # (a0) + 4*stride
    vmv2r.v          v18, v16
    vle8.v           v2, (t1)
    sh2add           t4, a1, t0  # (a0) + 5*stride
    vmv4r.v          v20, v16
    vle8.v           v3, (t2)

    sh2add           t5, a1, t1  # (a0) + 6*stride
    vmv8r.v          v24, v16
    vid.v            v31
    vle8.v           v4, (t3)
    sh2add           t6, a1, t2  # (a0) + 7*stride

    vle8.v           v5, (t4)
    vle8.v           v6, (t5)
    vle8.v           v7, (t6)

    vxor.vi         v31,  v31, 7 # [7, 6, 5, 4, 3, 2, 1, 0]

    vzext.vf2        v8,  v0
    vzext.vf2        v9,  v1
    vzext.vf2       v10,  v2
    vzext.vf2       v11,  v3
    vzext.vf2       v12,  v4
    vzext.vf2       v13,  v5
    vzext.vf2       v14,  v6
    vzext.vf2       v15,  v7

cdef_find_dir_vl128_8bpc_rvv_epilogue:
    // const int px = (img[x] >> bitdepth_min_8) - 128;
    vsub.vx          v8,   v8, a4
    vsub.vx          v9,   v9, a4
    vsub.vx         v10,  v10, a4
    vsub.vx         v11,  v11, a4
    vsub.vx         v12,  v12, a4
    vsub.vx         v13,  v13, a4
    vsub.vx         v14,  v14, a4
    vsub.vx         v15,  v15, a4

    // Calculating partial_sum_diag[0..1]

    //        v22  first_reg [xxxxxxxx]     [........]  <- Also used for
    //                   v24 [.xxxxxxx] v16 [x.......]  <- Reductions
    // Also used for  -> v25 [..xxxxxx] v17 [xx......]  <- at the end of
    // intermediate   -> v26 [...xxxxx] v18 [xxx.....]  <- Additions
    // additions      -> v27 [....xxxx] v19 [xxxx....]
    //                   v28 [.....xxx] v20 [xxxxx...]
    //                   v29 [......xx] v21 [xxxxxx..]
    //                   v30 [.......x] v23 [xxxxxxx.]

    // vslideups for mapping them to register as shown
    // Reversing the halfword positions using vrgather
    // Both can execute independently of each other, thus interleaved
    vrgather.vv      v0,  v8, v31
    vslideup.vi     v24,  v9, 1
    vrgather.vv      v1,  v9, v31
    vslideup.vi     v25, v10, 2
    vrgather.vv      v2, v10, v31
    vslideup.vi     v26, v11, 3
    vrgather.vv      v3, v11, v31
    vslideup.vi     v27, v12, 4
    vrgather.vv      v4, v12, v31
    vslideup.vi     v28, v13, 5
    vrgather.vv      v5, v13, v31
    vslideup.vi     v29, v14, 6
    vrgather.vv      v6, v14, v31
    vslideup.vi     v30, v15, 7
    vrgather.vv      v7, v15, v31

    // Map for stack stores
    /*NOTE:
                                  0                31 32               63
        partial_sum_diag[0..1] -  [xxxxxxxxxxxxxxx0]  [xxxxxxxxxxxxxxx0]
                                  64               95 96               127
        partial_sum_alt[2..3]  -  [xxxxxxxxxxx00000]  [xxxxxxxxxxx00000]
                                 128               159
        partial_sum_hv[1]  -      [xxxxxxxx00000000]

    */

    addi             sp,  sp, -160 # partial_sum_diag[0]

    vslidedown.vi   v16,  v9, 7
    vslidedown.vi   v17, v10, 6
    vslidedown.vi   v18, v11, 5

    addi             a0,  sp, 32   # partial_sum_diag[1]
    vslidedown.vi   v19, v12, 4
    vadd.vv         v22,  v8, v24
    vslidedown.vi   v20, v13, 3
    vadd.vv         v25, v25, v26
    vslidedown.vi   v21, v14, 2
    vadd.vv         v27, v27, v28
    vslidedown.vi   v23, v15, 1
    vadd.vv         v29, v29, v30

    addi             a1,  sp, 64   # partial_sum_alt[2]
    vadd.vv         v17, v16, v17
    vadd.vv         v19, v18, v19
    vadd.vv         v21, v20, v21

    addi             a3,  sp, 96   # partial_sum_alt[3]
    vadd.vv         v22, v22, v25
    vadd.vv         v27, v27, v29

    vslideup.vi     v24,  v1, 1
    vadd.vv         v19, v17, v19
    vslideup.vi     v25,  v2, 2
    vadd.vv         v23, v21, v23

    addi             a5,  sp, 128  # partial_sum_hv[1]
    vslideup.vi     v26,  v3, 3
    vadd.vv         v22, v22, v27
    vslideup.vi     v27,  v4, 4
    vadd.vv         v23, v19, v23
    vslideup.vi     v28,  v5, 5
    vslideup.vi     v29,  v6, 6
    vslideup.vi     v30,  v7, 7

    vslidedown.vi   v16,  v1, 7
    vslidedown.vi   v17,  v2, 6
    vs2r.v          v22, (sp)     # store partial_sum_diag[0]
    vslidedown.vi   v18,  v3, 5

    vadd.vv         v22,  v0, v24
    vslidedown.vi   v19,  v4, 4
    vadd.vv         v25, v25, v26
    vslidedown.vi   v20,  v5, 3
    vadd.vv         v27, v27, v28
    vslidedown.vi   v21,  v6, 2
    vadd.vv         v29, v29, v30
    vslidedown.vi   v23,  v7, 1

    vadd.vv         v17, v16, v17
    vadd.vv         v19, v18, v19
    vadd.vv         v21, v20, v21

    vadd.vv         v22, v22, v25
    vadd.vv         v27, v27, v29
    vadd.vv         v19, v17, v19
    vadd.vv         v23, v21, v23

    // Calculating partial_sum_alt[2..3]

    vadd.vv          v0,  v8,  v9
    vadd.vv          v1, v10, v11
    vadd.vv          v2, v12, v13
    vadd.vv          v3, v14, v15

    vslideup.vi     v24,  v1, 1
    vadd.vv         v22, v22, v27
    vslideup.vi     v25,  v2, 2
    vadd.vv         v23, v19, v23
    vslideup.vi     v26,  v3, 3

    vslidedown.vi   v16,  v1, 7
    vslidedown.vi   v17,  v2, 6
    vslidedown.vi   v18,  v3, 5

    vs2r.v          v22, (a0)     # store partial_sum_diag[1]

    vadd.vv          v4,  v0, v24
    vadd.vv          v5, v25, v26
    vslideup.vi     v24,  v2, 1
    vmv1r.v          v6, v16
    vslideup.vi     v25,  v1, 2
    vadd.vv          v7, v17, v18
    vslideup.vi     v26,  v0, 3

    li               a4, 0b01010101 # alt_mask
    vslidedown.vi   v16,  v2, 7
    vslidedown.vi   v17,  v1, 6
    vadd.vv          v4,  v4,  v5
    vslidedown.vi   v18,  v0, 5
    vadd.vv          v5,  v6,  v7

    vadd.vv          v3,  v3, v24
    vmv1r.v          v1, v16

    // Calculating partial_sum_hv[0..1]
    //vsetivli    zero, 8, e16, mf2, ta, ma

    vadd.vv          v2, v25, v26
    vredsum.vs       v7,  v8, v24 # using v24 which already has a zero
    vadd.vv          v0, v17, v18
    vredsum.vs       v16,  v9, v24 # on the first element
    vredsum.vs       v17, v10, v24
    vs2r.v           v4, (a1)   # store partial_sum_alt[2]
    vredsum.vs       v18, v11, v24
    vredsum.vs       v20, v12, v24
    vadd.vv          v2,  v2,  v3
    vredsum.vs       v21, v13, v24
    vadd.vv          v3,  v0,  v1
    vredsum.vs       v25, v14, v24
    vmv.s.x           v0,  a4      # alt_mask
    vredsum.vs       v23, v15, v24

    // Calculating partial_sum_alt[0..1]

    // Finding sum of pairs for
    // position [0,1][2,3][4,5][6,7]

    // Only vslide1down.vx instructions are associated with
    // partial_sum_alt here, and has been interleaved for performance

    vmv.x.s           t0,  v7
    vadd.vv           v7,  v8,  v9
    vslide1down.vx    v31, v8, zero

    vmv.x.s           t1, v16
    vadd.vv           v6, v10, v11
    vs2r.v            v2, (a3)   # store partial_sum_alt[3]
    vslide1down.vx    v1,  v9, zero

    vmv.x.s           t2, v17
    vadd.vv           v5, v12, v13
    vslide1down.vx    v2, v10, zero

    li                a4, 105
    vmv.x.s           t3, v18
    vadd.vv          v22, v14, v15
    vslide1down.vx    v3, v11, zero

    vmv.x.s           t4, v20
    vadd.vv           v7,  v6,  v7
    vslide1down.vx    v4, v12, zero

    vmv.x.s           t5,  v21
    vadd.vv          v22,  v22,  v5
    vslide1down.vx    v5,  v13, zero

    vmv.x.s           t6,  v25
    vadd.vv          v22,  v22,  v7
    vslide1down.vx    v6,  v14, zero

    vmv.x.s           a7,  v23

    // Zero v8-v15 before vslideup and vadd operations
    vadd.vv          v31, v31,  v8, v0.t
    vslide1down.vx    v7, v15, zero

    mul               t0,  t0,  t0
    vmv.v.i           v8, 0
    vadd.vv           v1,  v1,  v9, v0.t
    vmv.v.i           v9, 0

    mul               t1,  t1,  t1
    vadd.vv           v2,  v2, v10, v0.t
    vmv.v.i          v10, 0
    vadd.vv           v3,  v3, v11, v0.t

    vs1r.v           v22, (a5)   # store partial_sum_hv[1]

    mul               t2,  t2,  t2
    vmv.v.i          v11, 0
    vadd.vv           v4,  v4, v12, v0.t
    vmv.v.i          v12, 0

    mul               t3,  t3,  t3
    vadd.vv           v5,  v5, v13, v0.t
    vmv.v.i          v13, 0
    vadd.vv           v6,  v6, v14, v0.t

    mul               t4,  t4,  t4
    vmv.v.i          v14, 0
    vadd.vv           v7,  v7, v15, v0.t
    vmv.v.i          v15, 0

    vsetvli    zero, zero, e16, m1, tu, ma
    mul               t5,  t5,  t5
    vcompress.vm      v8, v31,  v0
    vcompress.vm      v9,  v1,  v0
    vcompress.vm     v10,  v2,  v0

    mul               t6,  t6,  t6
    vmv.v.i          v25, 0
    vcompress.vm     v11,  v3,  v0
    vcompress.vm     v12,  v4,  v0

    mul               a7,  a7,  a7
    vcompress.vm     v13,  v5,  v0
    vcompress.vm     v14,  v6,  v0
    vcompress.vm     v15,  v7,  v0

    vsetvli    zero, zero, e16, m1, ta, ma
    vid.v           v0

    add               t0,  t0,  t1
    vslideup.vi      v24,  v9, 1
    vslideup.vi      v25, v10, 2
    vslideup.vi      v26, v11, 3

    add               t1,  t2,  t3
    vslideup.vi      v27, v12, 4
    vslideup.vi      v28, v13, 5
    vslideup.vi      v29, v14, 6

    add               t2,  t4,  t5
    vslideup.vi      v30, v15, 7

    vxor.vi           v0,  v0, 3   # [3, 2, 1, 0]

    vadd.vv          v22,  v8, v24

    add               t3,  t6,  a7
    vslidedown.vi    v20, v13, 3
    vadd.vv          v25, v25, v26
    vslidedown.vi    v21, v14, 2

    add               t0,  t0,  t1
    vadd.vv          v27, v27, v28
    vslidedown.vi    v23, v15, 1
    vadd.vv          v29, v29, v30 # The addition continues after inversing the element order
                                   # to interleve the instruction

    vsetivli     zero, 4, e16, mf2, tu, ma
    add               t1,  t2,  t3

    // Reversiong the halfword positions
    vrgather.vv      v19,  v8, v0
    vrgather.vv       v8,  v9, v0
    vrgather.vv       v9, v10, v0

    add               t2,  t0, t1
    vrgather.vv      v10, v11, v0
    vrgather.vv      v11, v12, v0
    vrgather.vv      v12, v13, v0

    mul               t2,  t2, a4  # cost[2]
    vrgather.vv      v13, v14, v0
    vrgather.vv      v14, v15, v0

    vsetivli     zero, 8, e16, m1, ta, ma

    vadd.vv          v21, v20, v21 # Addition continues
    vslideup.vi      v24,  v8, 1
    vadd.vv          v22, v22, v25

    la a6, div_table_alt   # Moved away from vle16.v to reduce hazards
    vslideup.vi      v25,  v9, 2
    vadd.vv          v27, v27, v29

    vl2re16.v         v8, (a1) # load partial_sum_alt[2]

    vwadd.vv          v6, v21, v23
    vslideup.vi      v26, v10, 3
    vwadd.vv          v4, v22, v27
    vslideup.vi      v27, v11, 4

    vl2re16.v         v10, (a3) # load partial_sum_alt[3]

    vslideup.vi      v28, v12, 5
    vslideup.vi      v29, v13, 6
    vslideup.vi      v30, v14, 7

    vslidedown.vi    v20, v12, 3
    vslidedown.vi    v21, v13, 2
    vslidedown.vi    v23, v14, 1

    vl2re16.v         v14, (a6) # div_table_alt

    vadd.vv          v19, v19, v24
    vadd.vv          v25, v25, v26
    vadd.vv          v27, v27, v28
    vadd.vv          v29, v29, v30

    vadd.vv          v21, v20, v21

    vadd.vv          v19, v19, v25
    vadd.vv          v27, v27, v29

    vwadd.vv         v22, v21, v23
    vwadd.vv         v20, v19, v27

    vsetivli    zero, 15, e32, m4, ta, ma
    la a6, div_table_diag

    vsext.vf2         v0,  v8
    vsext.vf2         v8, v10
    vsext.vf2        v12, v14 # div_table_alt

    vmul.vv           v4,  v4,  v4 # cost[1]
    vmul.vv          v20, v20, v20 # cost[3]
    vmul.vv           v0,  v0,  v0 # cost[7]
    vmul.vv           v8,  v8,  v8 # cost[5]

    vl2re16.v        v16, (sp)      # partial_sum_diag[0]
    vl2re16.v        v30, (a0)      # partial_sum_diag[1]

    vmul.vv           v4,  v4,  v12 # cost[1]
    vmul.vv          v20, v20,  v12 # cost[3]
    vmul.vv           v0,  v0,  v12 # cost[7]
    vmul.vv           v8,  v8,  v12 # cost[5]

    sw                t2, 8(sp)    # best_cost calculation
    vsext.vf2        v24, v16      # cost[0]
    vsext.vf2        v16, v30      # cost[4]

    vredsum.vs       v12,  v4, v28 # cost[1]
    vredsum.vs        v4, v20, v28 # cost[3]
    vredsum.vs       v20,  v0, v28 # cost[7]
    vredsum.vs        v0,  v8, v28 # cost[5]

    vl2re16.v         v8, (a6) //div
    vmul.vv          v24, v24, v24 # cost[0]
    vmul.vv          v16, v16, v16 # cost[4]

    vmv.x.s           t1,  v12     # cost[1]
    vmv.x.s           t3,  v4      # cost[3]

    vsext.vf2         v4, v8       # div

    slli              t2,  t2, 3   # best_cost calculation
    vmv.x.s           a7, v20      # cost[7]
    vmv.x.s           t5,  v0      # cost[5]

    vmul.vv          v24, v24,  v4 # cost[0]
    sw                t1,  4(sp)
    vmul.vv          v16, v16,  v4 # cost[4]
    sw                t3, 12(sp)

    ori               t2,  t2,  5  # best_cost calculation
    vl2re16.v        v20, (a5)     # cost[6]
    sw                t5, 20(sp)

    vredsum.vs        v8, v24, v28 # cost[0]
    sw                a7, 28(sp)
    vredsum.vs       v24, v16, v28 # cost[4]

    li                a6, 105
    vsetivli   zero, 8, e32, m2, ta, ma

    vmv.x.s           t0,  v8      # cost[0]
    vsext.vf2         v0, v20      # cost[6]

    // Calculating best_cost, best_dir
    // All costs in t0..t6,a7 are left shifted by 3
    // Then, we set their priority using ori
    // The priority is reversed from the position of cost
    // Thus, for cost[0..7], priority -> 7..0

    // Lower 3 priority bits don't affect max because comparison
    // is weighted by higher bits

    vmul.vv           v0,  v0,  v0
    slli              t1,  t1, 3
    //slli            t2,  t2, 3   # Completed by interleaving with vector inst.
    vmv.x.s           t4, v24      # cost[4]
    slli              t3,  t3, 3
    slli              t5,  t5, 3
    slli              a7,  a7, 3

    vmul.vx           v0,  v0,  a6
    ori               t1,  t1,  6
    //ori             t2,  t2,  5  # Completed by interleaving with vector inst.
    ori               t3,  t3,  4
    ori               t5,  t5,  2
    //ori             a7,  a7,  0

    sw                t0,  0(sp)
    slli              t0,  t0,  3
    max               t1,  t1, a7
    vredsum.vs        v8,  v0, v28 # cost[6]
    //sw              t2,  8(sp)   # Completed by interleaving with vector inst.
    sw                t4, 16(sp)
    slli              t4,  t4,  3
    max               t3,  t3, t5

    max               t1,  t1, t2

    vmv.x.s           t6, v8       # cost[6]
    max               t1,  t1, t2

    ori               t0,  t0,  7
    ori               t4,  t4,  3

    max               t1,  t1, t3

    sw                t6,  24(sp)
    max               t0,  t0, t4

    slli              t6,  t6, 3
    max               t0,  t0, t1

    ori               t6,  t6,  1
    max               t0,  t0, t6

    // Calculating best_dir, best_cost and *var
    andi             a0,  t0,  7  # 7 - best_dir; get last 3 bits
    xori             a4,  a0,  3  # a4 = (7 - best_dir) ^ 3 = best_dir ^ 4
    sh2add           a4,  a4, sp
    lw               a4,  (a4)    # cost[best_dir ^ 4]
    xori             a0,  a0,  7  # best_dir; xor by 0x7 to find complement
    srli             a1,  t0,  3  # best_cost
    addi             sp,  sp, 160
    sub              a1,  a1, a4
    srli             a1,  a1, 10
    sw               a1, (a2)

    ret

endfunc
