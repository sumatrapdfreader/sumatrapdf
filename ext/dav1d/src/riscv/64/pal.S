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

function pal_idx_finish_rvv, export=1, ext="v,zba,zbb"
    csrw vxrm, zero
    srl t0, a2, 1
    sub a2, a2, a4
    srl t1, a4, 1
    mv t2, a5

    csrr t6, vlenb
    li t4, -3
    ctz a6, t0
    ctz t6, t6
    li a7, 16
    sub a6, a6, t6
    li t6, 1<<4+1

    // a6 is never > 3 for VLEN >=128
    // that would've required stripmining with a6 set to 3
    max a6, a6, t4
    li t5, 2
    andi a6, a6, 7
    addi t4, a1, 1
    ori a6, a6, 0xc0

1:
    sub t3, t0, t1
    vsetvl zero, t1, a6
    vlse8.v v0, (a1), t5
    sh1add a1, t1, a1
    vlse8.v v8, (t4), t5
    sh1add t4, t1, t4
    vmacc.vx v0, a7, v8
    vse8.v v0, (a0)
    add a0, a0, t1
    ble t3, zero, 4f

    lbu a4, -1(a1)
    mul a4, a4, t6
    vsetvl zero, t3, a6
    vmv.v.x v0, a4
    vse8.v v0, (a0)
    add a0, a0, t3
4:
    addi t2, t2, -1
    add a1, a1, a2
    add t4, t4, a2
    bnez t2, 1b

    sub t1, a3, a5

    sub t2, a0, t0
    ble t1, zero, 7f

    vsetvl zero, t0, a6
    vle8.v v0, (t2)
    add t2, a0, t0
5:
    addi t1, t1, -2
    vse8.v v0, (a0)
    vse8.v v0, (t2)
    sh1add a0, t0, a0
    sh1add t2, t0, t2

    bnez t1, 5b
7:
    ret
endfunc
