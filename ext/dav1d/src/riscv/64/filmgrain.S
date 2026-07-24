/*
 * Copyright © 2026, VideoLAN and dav1d authors
 * Copyright © 2026, Mohd Zaid
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

function generate_grain_y_8bpc_rvv, export=1, ext="v,zba,zbb"
    lbu t1, FGD_GRAIN_SCALE_SHIFT(a1)
    lla a2, dav1d_gaussian_sequence
    lwu a5, FGD_SEED(a1)

    addi t1, t1, 4
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
    // get random number

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

    // gaussian sequence [value]

    srliw t4, a5, 4
    andi  t4, t4, 2047
    slli  t4, t4, 1

    srliw a7, a5, 5
    andi  a7, a7, 2047
    slli  a7, a7, 17
    or    t4, t4, a7

    // push data onto stack

    sw t4, 0(a6)
    addi a6, a6, 4
    addi t6, t6, -2
    bnez t6, 2b

    // round2 and store

    vsetvli zero, t0, e16, m8, ta, ma
    vle16.v v8, (sp)
    vloxei16.v v16, (a2), v8

    vsetvli zero, t0, e8, m4, ta, ma
    vnclip.wx v4, v16, t1
    vse8.v v4, (a0)

    add  a0, a0, t0
    sub  t3, t3, t0
    bnez t3, 1b

    addi t2, t2, -1
    bnez t2, 0b

    addi sp, sp, 256

    // AR FILTER

3:
    li   t0, GRAIN_WIDTH * GRAIN_HEIGHT
    sub  a0, a0, t0

    lb   t1, FGD_AR_COEFF_LAG(a1)
    beqz t1, 15f

    addi sp, sp, -512

    lbu  t2, FGD_AR_COEFF_SHIFT(a1)
    addi t3, a1, FGD_AR_COEFFS_Y

    li   t4, 3
    li   a2, GRAIN_WIDTH

    // vector pre-sum (dy < 0 taps)

4:
    li  a1, GRAIN_HEIGHT
    bge t4, a1, 14f

    li  a4, GRAIN_WIDTH - 6
    li  t6, 3
    mv  t0, sp

5:
    vsetvli a1, a4, e8,  m2, ta, ma
    vsetvli zero, zero, e32, m8, ta, ma
    vmv.v.i v24, 0

    sub a7, zero, t1
    mv  a6, t3

    add a3, t4, a7
    mul a3, a3, a2
    add a3, a3, a0
    add a3, a3, t6

    vsetvli zero, zero, e16, m4, ta, ma

6:
    bge a7, zero, 9f

    sub a5, zero, t1
    add a3, a3, a5

    // dx loop
    sub a5, zero, t1
7:
    bgt a5, t1, 8f

    lb   t5, 0(a6)
    addi a6, a6, 1

    vle8.v v16, (a3)
    addi a3, a3, 1

    vsext.vf2 v20, v16
    vwmacc.vx v24, t5, v20

    addi a5, a5, 1
    j    7b

8:
    add  a3, a3, a2
    sub  a3, a3, t1
    addi a3, a3, -1

    addi a7, a7, 1
    j    6b

9:
    vsetvli zero, a1, e32, m8, ta, ma
    vse32.v v24, (t0)

    add  t6, t6, a1
    slli a3, a1, 2
    add  t0, t0, a3
    sub  a4, a4, a1
    bnez a4, 5b

    // scalar horizontal pass (dx < 0, dy = 0)

    li  a4, 3
    mv  t0, sp
    mul a3, t4, a2
    add a3, a3, a0

10:
    li  a1, GRAIN_WIDTH - 3
    bge a4, a1, 13f

    lw   a5, 0(t0)
    addi t0, t0, 4

    mv   a6, t3
    li   a1, 2
    mul  a1, a1, t1
    addi a1, a1, 1
    mul  a1, a1, t1
    add  a6, a6, a1

    add a1, a3, a4
    sub a7, zero, t1

11:
    bge a7, zero, 12f

    lb   t5, 0(a6)
    addi a6, a6, 1
    add  t6, a1, a7
    lb   t6, 0(t6)
    mul  t6, t6, t5
    add  a5, a5, t6

    addi a7, a7, 1
    j    11b

12:
    li   a1, 1
    sll  a1, a1, t2
    srai a1, a1, 1
    add  a5, a5, a1
    sra  a5, a5, t2

    add a1, a3, a4
    lb  t5, 0(a1)
    add a5, a5, t5

    li  t5, -128
    max a5, a5, t5
    li  t5, 127
    min a5, a5, t5

    sb   a5, 0(a1)
    addi a4, a4, 1
    j    10b

13:
    addi t4, t4, 1
    j    4b

14:
    addi sp, sp, 512
15:
    ret
endfunc
