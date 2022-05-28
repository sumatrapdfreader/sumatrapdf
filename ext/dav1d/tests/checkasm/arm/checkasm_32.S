/******************************************************************************
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2015 Martin Storsjo
 * Copyright © 2015 Janne Grunau
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

#define PRIVATE_PREFIX checkasm_

#include "src/arm/asm.S"
#include "src/arm/32/util.S"

const register_init, align=3
        .quad 0x21f86d66c8ca00ce
        .quad 0x75b6ba21077c48ad
        .quad 0xed56bb2dcb3c7736
        .quad 0x8bda43d3fd1a7e06
        .quad 0xb64a9c9e5d318408
        .quad 0xdf9a54b303f1d3a3
        .quad 0x4a75479abd64e097
        .quad 0x249214109d5d1c88
endconst

const error_message_fpscr
        .asciz "failed to preserve register FPSCR, changed bits: %x"
error_message_gpr:
        .asciz "failed to preserve register r%d"
error_message_vfp:
        .asciz "failed to preserve register d%d"
error_message_stack:
        .asciz "failed to preserve stack"
endconst

@ max number of args used by any asm function.
#define MAX_ARGS 15

#define ARG_STACK 4*(MAX_ARGS - 4)

@ Align the used stack space to 8 to preserve the stack alignment.
@ +8 for stack canary reference.
#define ARG_STACK_A (((ARG_STACK + pushed + 7) & ~7) - pushed + 8)

.macro clobbercheck variant
.equ pushed, 4*9
function checked_call_\variant, export=1
        push            {r4-r11, lr}
.ifc \variant, vfp
        vpush           {d8-d15}
        fmrx            r4,  FPSCR
        push            {r4}
.equ pushed, pushed + 16*4 + 4
.endif

        movrel          r12, register_init
.ifc \variant, vfp
        vldm            r12, {d8-d15}
.endif
        ldm             r12, {r4-r11}

        sub             sp,  sp,  #ARG_STACK_A
.equ pos, 0
.rept MAX_ARGS-4
        ldr             r12, [sp, #ARG_STACK_A + pushed + 8 + pos]
        str             r12, [sp, #pos]
.equ pos, pos + 4
.endr

        @ For stack overflows, the callee is free to overwrite the parameters
        @ that were passed on the stack (if any), so we can only check after
        @ that point. First figure out how many parameters the function
        @ really took on the stack:
        ldr             r12, [sp, #ARG_STACK_A + pushed + 8 + 4*(MAX_ARGS-4)]
        @ Load the first non-parameter value from the stack, that should be
        @ left untouched by the function. Store a copy of it inverted, so that
        @ e.g. overwriting everything with zero would be noticed.
        ldr             r12, [sp, r12, lsl #2]
        mvn             r12, r12
        str             r12, [sp, #ARG_STACK_A - 4]

        mov             r12, r0
        mov             r0,  r2
        mov             r1,  r3
        ldrd            r2,  r3,  [sp, #ARG_STACK_A + pushed]
        @ Call the target function
        blx             r12

        @ Load the number of stack parameters, stack canary and its reference
        ldr             r12, [sp, #ARG_STACK_A + pushed + 8 + 4*(MAX_ARGS-4)]
        ldr             r2,  [sp, r12, lsl #2]
        ldr             r3,  [sp, #ARG_STACK_A - 4]

        add             sp,  sp,  #ARG_STACK_A
        push            {r0, r1}

        mvn             r3,  r3
        cmp             r2,  r3
        bne             5f

        movrel          r12, register_init
.ifc \variant, vfp
.macro check_reg_vfp, dreg, offset
        ldrd            r2,  r3,  [r12, #8 * (\offset)]
        vmov            r0,  lr,  \dreg
        eor             r2,  r2,  r0
        eor             r3,  r3,  lr
        orrs            r2,  r2,  r3
        bne             4f
.endm

.irp n, 8, 9, 10, 11, 12, 13, 14, 15
        @ keep track of the checked double/SIMD register
        mov             r1,  #\n
        check_reg_vfp   d\n, \n-8
.endr
.purgem check_reg_vfp

        fmrx            r1,  FPSCR
        ldr             r3,  [sp, #8]
        eor             r1,  r1,  r3
        @ Ignore changes in bits 0-4 and 7
        bic             r1,  r1,  #0x9f
        @ Ignore changes in the topmost 5 bits
        bics            r1,  r1,  #0xf8000000
        bne             3f
.endif

        @ keep track of the checked GPR
        mov             r1,  #4
.macro check_reg reg1, reg2=
        ldrd            r2,  r3,  [r12], #8
        eors            r2,  r2,  \reg1
        bne             2f
        add             r1,  r1,  #1
.ifnb \reg2
        eors            r3,  r3,  \reg2
        bne             2f
.endif
        add             r1,  r1,  #1
.endm
        check_reg       r4,  r5
        check_reg       r6,  r7
@ r9 is a volatile register in the ios ABI
#ifdef __APPLE__
        check_reg       r8
#else
        check_reg       r8,  r9
#endif
        check_reg       r10, r11
.purgem check_reg

        b               0f
5:
        movrel          r0, error_message_stack
        b               1f
4:
        movrel          r0, error_message_vfp
        b               1f
3:
        movrel          r0, error_message_fpscr
        b               1f
2:
        movrel          r0, error_message_gpr
1:
#ifdef PREFIX
        bl              _checkasm_fail_func
#else
        bl              checkasm_fail_func
#endif
0:
        pop             {r0, r1}
.ifc \variant, vfp
        pop             {r2}
        fmxr            FPSCR, r2
        vpop            {d8-d15}
.endif
        pop             {r4-r11, pc}
endfunc
.endm

clobbercheck vfp
