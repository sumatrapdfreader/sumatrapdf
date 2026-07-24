/*
 * Copyright © 2024, VideoLAN and dav1d authors
 * Copyright © 2024, Mohd Zaid
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

#include "src/riscv/asm.S"
#include "src/riscv/asm-offsets.h"

#define GRAIN_WIDTH         82
#define GRAIN_HEIGHT        73

function generate_grain_y_16bpc_rvv, export=1, ext="v,zba,zbb"
    addi sp, sp, -32
    sd   s0, 0(sp)
    sd   s1, 8(sp)
    sd   s2, 16(sp)

    // bitdepth scaling
    clzw t0, a2
    li   t1, 24
    sub  t0, t1, t0

    li   t3, 128
    sll  t3, t3, t0

    lbu  t1, FGD_GRAIN_SCALE_SHIFT(a1)
    li   t2, 4
    sub  t2, t2, t0
    add  t1, t1, t2

    sub  s1, zero, t3
    addi s2, t3, -1

    lla  a4, dav1d_gaussian_sequence
    lwu  a5, FGD_SEED(a1)

    addi sp, sp, -256

    csrwi vxrm, 0
    li t2, GRAIN_HEIGHT
0:
    li t3, GRAIN_WIDTH
1:
    vsetvli t0, t3, e16, m8, ta, ma
    mv t6, t0
    mv a6, sp
2:
    srliw t4, a5, 1
    xor   t5, a5, t4
    srliw t4, a5, 3
    xor   t5, t5, t4
    srliw t4, a5, 12
    xor   t5, t5, t4
    andi  t5, t5, 3

    srliw a5, a5, 2
    slli  t5, t5, 14
    or    a5, a5, t5

    srliw t4, a5, 4
    andi  t4, t4, 2047
    slli  t4, t4, 1

    srliw a7, a5, 5
    andi  a7, a7, 2047
    slli  a7, a7, 17
    or    t4, t4, a7

    sw t4, 0(a6)
    addi a6, a6, 4
    addi t6, t6, -2
    bnez t6, 2b

    vsetvli zero, t0, e16, m8, ta, ma
    vle16.v v8, (sp)

    vloxei16.v v16, (a4), v8

    vssra.vx   v8, v16, t1
    vse16.v    v8, (a0)

    slli t4, t0, 1
    add  a0, a0, t4
    sub  t3, t3, t0
    bnez t3, 1b

    addi t2, t2, -1
    bnez t2, 0b

    addi sp, sp, 256

    // AR FILTER

3:
    li   t0, GRAIN_WIDTH * GRAIN_HEIGHT * 2
    sub  a0, a0, t0

    lb   t0, FGD_AR_COEFF_LAG(a1)
    beqz t0, 15f

    addi sp, sp, -512

    lbu  t2, FGD_AR_COEFF_SHIFT(a1)
    addi t3, a1, FGD_AR_COEFFS_Y

    li   t4, 3
    li   a2, GRAIN_WIDTH

4:
    li  a4, GRAIN_HEIGHT
    bge t4, a4, 14f

    li  a4, GRAIN_WIDTH - 6
    li  t6, 3
    mv  a6, sp

5:
    vsetvli a7, a4, e16, m4, ta, ma
    vsetvli zero, zero, e32, m8, ta, ma
    vmv.v.i v24, 0

    sub a5, zero, t0
    mv  s0, t3

    add a3, t4, a5
    mul a3, a3, a2
    add a3, a3, t6
    slli a3, a3, 1
    add a3, a3, a0

    vsetvli zero, zero, e16, m4, ta, ma

6:
    bge a5, zero, 9f

    sub t1, zero, t0
    slli t1, t1, 1
    add a3, a3, t1

    sub t1, zero, t0
7:
    bgt t1, t0, 8f

    lb   t5, 0(s0)
    addi s0, s0, 1

    vle16.v   v16, (a3)
    addi      a3, a3, 2

    vwmacc.vx v24, t5, v16

    addi t1, t1, 1
    j    7b

8:
    slli t1, a2, 1
    add  a3, a3, t1
    slli t1, t0, 1
    sub  a3, a3, t1
    addi a3, a3, -2

    addi a5, a5, 1
    j    6b

9:
    vsetvli zero, a7, e32, m8, ta, ma
    vse32.v v24, (a6)

    add  t6, t6, a7
    slli a3, a7, 2
    add  a6, a6, a3
    sub  a4, a4, a7
    bnez a4, 5b

    // scalar horizontal pass (dx < 0, dy = 0)
    li  a4, 3
    mv  a6, sp
    mul a3, t4, a2
    slli a3, a3, 1
    add a3, a3, a0

10:
    li  a7, GRAIN_WIDTH - 3
    bge a4, a7, 13f

    lw   a5, 0(a6)
    addi a6, a6, 4

    mv   s0, t3
    li   a7, 2
    mul  a7, a7, t0
    addi a7, a7, 1
    mul  a7, a7, t0
    add  s0, s0, a7

    slli t6, a4, 1
    add  t6, a3, t6
    sub  t1, zero, t0

11:
    bge t1, zero, 12f

    lb   t5, 0(s0)
    addi s0, s0, 1

    slli a7, t1, 1
    add  a7, t6, a7
    lh   a7, 0(a7)
    mul  a7, a7, t5
    add  a5, a5, a7

    addi t1, t1, 1
    j    11b

12:
    li   a7, 1
    sll  a7, a7, t2
    srai a7, a7, 1
    add  a5, a5, a7
    sra  a5, a5, t2

    lh  t5, 0(t6)
    add a5, a5, t5

    max a5, a5, s1
    min a5, a5, s2

    sh   a5, 0(t6)
    addi a4, a4, 1
    j    10b

13:
    addi t4, t4, 1
    j    4b

14:
    addi sp, sp, 512
15:
    ld   s0, 0(sp)
    ld   s1, 8(sp)
    ld   s2, 16(sp)
    addi sp, sp, 32
    ret
endfunc
