; Copyright © 2018, VideoLAN and dav1d authors
; Copyright © 2018, Two Orioles, LLC
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice,
;    this list of conditions and the following disclaimer in the documentation
;    and/or other materials provided with the distribution.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
; ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

%include "config.asm"
%undef private_prefix
%define private_prefix checkasm
%include "ext/x86/x86inc.asm"

SECTION_RODATA 16

%if ARCH_X86_64
; just random numbers to reduce the chance of incidental match
%if WIN64
x6:  dq 0x1a1b2550a612b48c,0x79445c159ce79064
x7:  dq 0x2eed899d5a28ddcd,0x86b2536fcd8cf636
x8:  dq 0xb0856806085e7943,0x3f2bf84fc0fcca4e
x9:  dq 0xacbd382dcf5b8de2,0xd229e1f5b281303f
x10: dq 0x71aeaff20b095fd9,0xab63e2e11fa38ed9
x11: dq 0x89b0c0765892729a,0x77d410d5c42c882d
x12: dq 0xc45ea11a955d8dd5,0x24b3c1d2a024048b
x13: dq 0x2e8ec680de14b47c,0xdd7b8919edd42786
x14: dq 0x135ce6888fa02cbf,0x11e53e2b2ac655ef
x15: dq 0x011ff554472a7a10,0x6de8f4c914c334d5
n7:  dq 0x21f86d66c8ca00ce
n8:  dq 0x75b6ba21077c48ad
%endif
n9:  dq 0xed56bb2dcb3c7736
n10: dq 0x8bda43d3fd1a7e06
n11: dq 0xb64a9c9e5d318408
n12: dq 0xdf9a54b303f1d3a3
n13: dq 0x4a75479abd64e097
n14: dq 0x249214109d5d1c88
%endif

errmsg_stack: db "stack corruption", 0

SECTION .text

cextern fail_func

; max number of args used by any asm function.
; (max_args % 4) must equal 3 for stack alignment
%define max_args 15

%if ARCH_X86_64

;-----------------------------------------------------------------------------
; int checkasm_stack_clobber(uint64_t clobber, ...)
;-----------------------------------------------------------------------------
cglobal stack_clobber, 1, 2
    ; Clobber the stack with junk below the stack pointer
    %define argsize (max_args+6)*8
    SUB  rsp, argsize
    mov   r1, argsize-8
.loop:
    mov [rsp+r1], r0
    sub   r1, 8
    jge .loop
    ADD  rsp, argsize
    RET

%if WIN64
    %assign free_regs 7
    %define stack_param rsp+32 ; shadow space
    %define num_stack_params rsp+stack_offset+22*8
    DECLARE_REG_TMP 4
%else
    %assign free_regs 9
    %define stack_param rsp
    %define num_stack_params rsp+stack_offset+16*8
    DECLARE_REG_TMP 7
%endif

;-----------------------------------------------------------------------------
; void checkasm_checked_call(void *func, ...)
;-----------------------------------------------------------------------------
INIT_XMM
cglobal checked_call, 2, 15, 16, max_args*8+64+8
    mov            t0, r0

    ; All arguments have been pushed on the stack instead of registers in
    ; order to test for incorrect assumptions that 32-bit ints are
    ; zero-extended to 64-bit.
    mov            r0, r6mp
    mov            r1, r7mp
    mov            r2, r8mp
    mov            r3, r9mp
%if UNIX64
    mov            r4, r10mp
    mov            r5, r11mp
%else ; WIN64
    ; Move possible floating-point arguments to the correct registers
    movq           m0, r0
    movq           m1, r1
    movq           m2, r2
    movq           m3, r3

%assign i 6
%rep 16-6
    mova       m %+ i, [x %+ i]
    %assign i i+1
%endrep
%endif

    ; write stack canaries to the area above parameters passed on the stack
    mov           r9d, [num_stack_params]
    mov            r8, [rsp+stack_offset] ; return address
    not            r8
%assign i 0
%rep 8 ; 64 bytes
    mov [stack_param+(r9+i)*8], r8
    %assign i i+1
%endrep
    dec           r9d
    jl .stack_setup_done ; no stack parameters
.copy_stack_parameter:
    mov            r8, [stack_param+stack_offset+7*8+r9*8]
    mov [stack_param+r9*8], r8
    dec           r9d
    jge .copy_stack_parameter
.stack_setup_done:

%assign i 14
%rep 15-free_regs
    mov        r %+ i, [n %+ i]
    %assign i i-1
%endrep
    call           t0

    ; check for stack corruption
    mov           r0d, [num_stack_params]
    mov            r3, [rsp+stack_offset]
    mov            r4, [stack_param+r0*8]
    not            r3
    xor            r4, r3
%assign i 1
%rep 6
    mov            r5, [stack_param+(r0+i)*8]
    xor            r5, r3
    or             r4, r5
    %assign i i+1
%endrep
    xor            r3, [stack_param+(r0+7)*8]
    lea            r0, [errmsg_stack]
    or             r4, r3
    jnz .fail

    ; check for failure to preserve registers
%assign i 14
%rep 15-free_regs
    cmp        r %+ i, [r0-errmsg_stack+n %+ i]
    setne         r4b
    lea           r3d, [r4+r3*2]
    %assign i i-1
%endrep
%if WIN64
    lea            r0, [rsp+60] ; account for shadow space
    mov            r5, r0
    test          r3d, r3d
    jz .gpr_ok
%else
    test          r3d, r3d
    jz .ok
    lea            r0, [rsp+28]
%endif
%assign i free_regs
%rep 15-free_regs
%if i < 10
    mov    dword [r0], " r0" + (i << 16)
    lea            r4, [r0+3]
%else
    mov    dword [r0], " r10" + ((i - 10) << 24)
    lea            r4, [r0+4]
%endif
    test          r3b, 1 << (i - free_regs)
    cmovnz         r0, r4
    %assign i i+1
%endrep
%if WIN64 ; xmm registers
.gpr_ok:
%assign i 6
%rep 16-6
    pxor       m %+ i, [x %+ i]
    %assign i i+1
%endrep
    packsswb       m6, m7
    packsswb       m8, m9
    packsswb      m10, m11
    packsswb      m12, m13
    packsswb      m14, m15
    packsswb       m6, m6
    packsswb       m8, m10
    packsswb      m12, m14
    packsswb       m6, m6
    packsswb       m8, m12
    packsswb       m6, m8
    pxor           m7, m7
    pcmpeqb        m6, m7
    pmovmskb      r3d, m6
    cmp           r3d, 0xffff
    je .xmm_ok
    mov           r7d, " xmm"
%assign i 6
%rep 16-6
    mov        [r0+0], r7d
%if i < 10
    mov   byte [r0+4], "0" + i
    lea            r4, [r0+5]
%else
    mov   word [r0+4], "10" + ((i - 10) << 8)
    lea            r4, [r0+6]
%endif
    test          r3d, 1 << i
    cmovz          r0, r4
    %assign i i+1
%endrep
.xmm_ok:
    cmp            r0, r5
    je .ok
    mov     byte [r0], 0
    lea            r0, [r5-28]
%else
    mov     byte [r0], 0
    mov            r0, rsp
%endif
    mov dword [r0+ 0], "fail"
    mov dword [r0+ 4], "ed t"
    mov dword [r0+ 8], "o pr"
    mov dword [r0+12], "eser"
    mov dword [r0+16], "ve r"
    mov dword [r0+20], "egis"
    mov dword [r0+24], "ter:"
.fail:
    ; Call fail_func() with a descriptive message to mark it as a failure.
    ; Save the return value located in rdx:rax first to prevent clobbering.
    mov            r9, rax
    mov           r10, rdx
    xor           eax, eax
    call fail_func
    mov           rdx, r10
    mov           rax, r9
.ok:
    RET

; trigger a warmup of vector units
%macro WARMUP 0
cglobal warmup, 0, 0
    xorps          m0, m0
    mulps          m0, m0
    RET
%endmacro

INIT_YMM avx2
WARMUP
INIT_ZMM avx512
WARMUP

%else

; just random numbers to reduce the chance of incidental match
%assign n3 0x6549315c
%assign n4 0xe02f3e23
%assign n5 0xb78d0d1d
%assign n6 0x33627ba7

;-----------------------------------------------------------------------------
; void checkasm_checked_call(void *func, ...)
;-----------------------------------------------------------------------------
cglobal checked_call, 1, 7
    mov            r3, [esp+stack_offset]      ; return address
    mov            r1, [esp+stack_offset+17*4] ; num_stack_params
    mov            r2, 27
    not            r3
    sub            r2, r1
.push_canary:
    push           r3
    dec            r2
    jg .push_canary
.push_parameter:
    push dword [esp+32*4]
    dec            r1
    jg .push_parameter
    mov            r3, n3
    mov            r4, n4
    mov            r5, n5
    mov            r6, n6
    call           r0

    ; check for failure to preserve registers
    cmp            r3, n3
    setne         r3h
    cmp            r4, n4
    setne         r3b
    shl           r3d, 16
    cmp            r5, n5
    setne         r3h
    cmp            r6, n6
    setne         r3b
    test           r3, r3
    jz .gpr_ok
    lea            r1, [esp+16]
    mov dword [r1+ 0], "fail"
    mov dword [r1+ 4], "ed t"
    mov dword [r1+ 8], "o pr"
    mov dword [r1+12], "eser"
    mov dword [r1+16], "ve r"
    mov dword [r1+20], "egis"
    mov dword [r1+24], "ter:"
    lea            r4, [r1+28]
%assign i 3
%rep 4
    mov dword    [r4], " r0" + (i << 16)
    lea            r5, [r4+3]
    test           r3, 1 << ((6 - i) * 8)
    cmovnz         r4, r5
    %assign i i+1
%endrep
    mov     byte [r4], 0
    jmp .fail
.gpr_ok:
    ; check for stack corruption
    mov            r3, [esp+48*4] ; num_stack_params
    mov            r6, [esp+31*4] ; return address
    mov            r4, [esp+r3*4]
    sub            r3, 26
    not            r6
    xor            r4, r6
.check_canary:
    mov            r5, [esp+(r3+27)*4]
    xor            r5, r6
    or             r4, r5
    inc            r3
    jl .check_canary
    test           r4, r4
    jz .ok
    LEA            r1, errmsg_stack
.fail:
    mov            r3, eax
    mov            r4, edx
    mov         [esp], r1
    call fail_func
    mov           edx, r4
    mov           eax, r3
.ok:
    add           esp, 27*4
    RET

%endif ; ARCH_X86_64
