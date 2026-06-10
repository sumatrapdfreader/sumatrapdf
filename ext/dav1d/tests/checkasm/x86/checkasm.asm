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
errmsg_register: db "failed to preserve register:%s", 0
errmsg_vzeroupper: db "missing vzeroupper", 0

SECTION .bss

check_vzeroupper: resd 1

SECTION .text

cextern fail_func

; max number of args used by any asm function.
; (max_args % 4) must equal 3 for stack alignment
%define max_args 15

%if UNIX64
    DECLARE_REG_TMP 0
%else
    DECLARE_REG_TMP 4
%endif

;-----------------------------------------------------------------------------
; unsigned checkasm_init_x86(char *name)
;-----------------------------------------------------------------------------
cglobal init_x86, 0, 5
%if ARCH_X86_64
    push          rbx
%endif
    movifnidn      t0, r0mp
    mov           eax, 0x80000000
    cpuid
    cmp           eax, 0x80000004
    jb .no_brand ; processor brand string not supported
    mov           eax, 0x80000002
    cpuid
    mov     [t0+4* 0], eax
    mov     [t0+4* 1], ebx
    mov     [t0+4* 2], ecx
    mov     [t0+4* 3], edx
    mov           eax, 0x80000003
    cpuid
    mov     [t0+4* 4], eax
    mov     [t0+4* 5], ebx
    mov     [t0+4* 6], ecx
    mov     [t0+4* 7], edx
    mov           eax, 0x80000004
    cpuid
    mov     [t0+4* 8], eax
    mov     [t0+4* 9], ebx
    mov     [t0+4*10], ecx
    mov     [t0+4*11], edx
    xor           eax, eax
    cpuid
    jmp .check_xcr1
.no_brand: ; use manufacturer id as a fallback
    xor           eax, eax
    mov      [t0+4*3], eax
    cpuid
    mov      [t0+4*0], ebx
    mov      [t0+4*1], edx
    mov      [t0+4*2], ecx
.check_xcr1:
    test          eax, eax
    jz .end2 ; cpuid leaf 1 not supported
    mov           t0d, eax ; max leaf
    mov           eax, 1
    cpuid
    and           ecx, 0x18000000
    cmp           ecx, 0x18000000
    jne .end2 ; osxsave/avx not supported
    cmp           t0d, 13 ; cpuid leaf 13 not supported
    jb .end2
    mov           t0d, eax ; cpuid signature
    mov           eax, 13
    mov           ecx, 1
    cpuid
    test           al, 0x04
    jz .end ; xcr1 not supported
    mov           ecx, 1
    xgetbv
    test           al, 0x04
    jnz .end ; always-dirty ymm state
%if ARCH_X86_64 == 0 && PIC
    LEA           eax, check_vzeroupper
    mov         [eax], ecx
%else
    mov [check_vzeroupper], ecx
%endif
.end:
    mov           eax, t0d
.end2:
%if ARCH_X86_64
    pop           rbx
%endif
    RET

%if ARCH_X86_64
%if WIN64
    %define stack_param rsp+32 ; shadow space
    %define num_fn_args rsp+stack_offset+17*8
    %assign num_reg_args 4
    %assign free_regs 7
    %assign clobber_mask_stack_bit 16
    DECLARE_REG_TMP 4
%else
    %define stack_param rsp
    %define num_fn_args rsp+stack_offset+11*8
    %assign num_reg_args 6
    %assign free_regs 9
    %assign clobber_mask_stack_bit 64
    DECLARE_REG_TMP 7
%endif

%macro CLOBBER_UPPER 2 ; reg, mask_bit
    mov          r13d, %1d
    or            r13, r8
    test          r9b, %2
    cmovnz         %1, r13
%endmacro

cglobal checked_call, 2, 15, 16, max_args*8+64+8
    mov          r10d, [num_fn_args]
    mov            r8, 0xdeadbeef00000000
    mov           r9d, [num_fn_args+r10*8+8] ; clobber_mask
    mov            t0, [num_fn_args+r10*8]   ; func

    ; Clobber the upper halves of 32-bit parameters
    CLOBBER_UPPER  r0, 1
    CLOBBER_UPPER  r1, 2
    CLOBBER_UPPER  r2, 4
    CLOBBER_UPPER  r3, 8
%if UNIX64
    CLOBBER_UPPER  r4, 16
    CLOBBER_UPPER  r5, 32
%else ; WIN64
%assign i 6
%rep 16-6
    mova       m %+ i, [x %+ i]
    %assign i i+1
%endrep
%endif

    xor          r11d, r11d
    sub          r10d, num_reg_args
    cmovs        r10d, r11d ; num stack args

    ; write stack canaries to the area above parameters passed on the stack
    mov           r12, [rsp+stack_offset] ; return address
    not           r12
%assign i 0
%rep 8 ; 64 bytes
    mov [stack_param+(r10+i)*8], r12
    %assign i i+1
%endrep

    test         r10d, r10d
    jz .stack_setup_done ; no stack parameters
.copy_stack_parameter:
    mov           r12, [stack_param+stack_offset+8+r11*8]
    CLOBBER_UPPER r12, clobber_mask_stack_bit
    shr           r9d, 1
    mov [stack_param+r11*8], r12
    inc          r11d
    cmp          r11d, r10d
    jl .copy_stack_parameter
.stack_setup_done:

%assign i 14
%rep 15-free_regs
    mov        r %+ i, [n %+ i]
    %assign i i-1
%endrep
    call           t0

    ; check for stack corruption
    mov           r0d, [num_fn_args]
    xor           r3d, r3d
    sub           r0d, num_reg_args
    cmovs         r0d, r3d ; num stack args

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
    or             r4, r3
    jz .stack_ok
    ; Save the return value located in rdx:rax first to prevent clobbering.
    mov           r10, rax
    mov           r11, rdx
    lea            r0, [errmsg_stack]
    jmp .fail
.stack_ok:

    ; check for failure to preserve registers
%assign i 14
%rep 15-free_regs
    cmp        r %+ i, [n %+ i]
    setne         r4b
    lea           r3d, [r4+r3*2]
    %assign i i-1
%endrep
%if WIN64
    lea            r0, [rsp+32] ; account for shadow space
    mov            r5, r0
    test          r3d, r3d
    jz .gpr_ok
%else
    test          r3d, r3d
    jz .gpr_xmm_ok
    mov            r0, rsp
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
    je .gpr_xmm_ok
    mov     byte [r0], 0
    mov           r11, rdx
    mov            r1, r5
%else
    mov     byte [r0], 0
    mov           r11, rdx
    mov            r1, rsp
%endif
    mov           r10, rax
    lea            r0, [errmsg_register]
    jmp .fail
.gpr_xmm_ok:
    ; Check for dirty YMM state, i.e. missing vzeroupper
    mov           ecx, [check_vzeroupper]
    test          ecx, ecx
    jz .ok ; not supported, skip
    mov           r10, rax
    mov           r11, rdx
    xgetbv
    test           al, 0x04
    jz .restore_retval ; clean ymm state
    lea            r0, [errmsg_vzeroupper]
    vzeroupper
.fail:
    ; Call fail_func() with a descriptive message to mark it as a failure.
    xor           eax, eax
    call fail_func
.restore_retval:
    mov           rax, r10
    mov           rdx, r11
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
    mov       [esp+4], r1
%assign i 3
%rep 4
    mov    dword [r1], " r0" + (i << 16)
    lea            r4, [r1+3]
    test           r3, 1 << ((6 - i) * 8)
    cmovnz         r1, r4
    %assign i i+1
%endrep
    mov     byte [r1], 0
    mov            r5, eax
    mov            r6, edx
    LEA            r1, errmsg_register
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
    mov            r5, eax
    mov            r6, edx
    test           r4, r4
    jz .stack_ok
    LEA            r1, errmsg_stack
    jmp .fail
.stack_ok:
    ; check for dirty YMM state, i.e. missing vzeroupper
    LEA           ecx, check_vzeroupper
    mov           ecx, [ecx]
    test          ecx, ecx
    jz .ok ; not supported, skip
    xgetbv
    test           al, 0x04
    jz .ok ; clean ymm state
    LEA            r1, errmsg_vzeroupper
    vzeroupper
.fail:
    mov         [esp], r1
    call fail_func
.ok:
    add           esp, 27*4
    mov           eax, r5
    mov           edx, r6
    RET

%endif ; ARCH_X86_64
