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
function cdef_filter_block_\w\()x\h\()_8bpc_rvv, export=1, ext="v,zba,zbb"
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
