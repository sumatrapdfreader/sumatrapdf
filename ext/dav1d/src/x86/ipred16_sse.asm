; Copyright © 2021, VideoLAN and dav1d authors
; Copyright © 2021, Two Orioles, LLC
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
%include "ext/x86/x86inc.asm"

SECTION_RODATA

filter_shuf:   db  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,  4,  5,  2,  3, -1, -1
pal_pred_shuf: db  0,  2,  4,  6,  8, 10, 12, 14,  1,  3,  5,  7,  9, 11, 13, 15

pb_0_1:  times 4 db 0, 1
pb_2_3:  times 4 db 2, 3
pw_1:    times 4 dw 1
pw_2:    times 4 dw 2
pw_4:    times 4 dw 4
pw_512:  times 4 dw 512
pw_2048: times 4 dw 2048

%macro JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1_%2)
    %%table:
    %rep %0 - 2
        dd %%base %+ .%3 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

%define ipred_dc_splat_16bpc_ssse3_table (ipred_dc_16bpc_ssse3_table + 10*4)
%define ipred_dc_128_16bpc_ssse3_table   (ipred_dc_16bpc_ssse3_table + 15*4)
%define ipred_cfl_splat_16bpc_ssse3_table (ipred_cfl_16bpc_ssse3_table + 8*4)

JMP_TABLE ipred_dc_left_16bpc,    ssse3, h4, h8, h16, h32, h64
JMP_TABLE ipred_dc_16bpc,         ssse3, h4, h8, h16, h32, h64, w4, w8, w16, w32, w64, \
                                         s4-10*4, s8-10*4, s16-10*4, s32-10*4, s64-10*4, \
                                         s4-15*4, s8-15*4, s16c-15*4, s32c-15*4, s64-15*4
JMP_TABLE ipred_h_16bpc,          ssse3, w4, w8, w16, w32, w64
JMP_TABLE ipred_cfl_16bpc,        ssse3, h4, h8, h16, h32, w4, w8, w16, w32, \
                                         s4-8*4, s8-8*4, s16-8*4, s32-8*4
JMP_TABLE ipred_cfl_left_16bpc,   ssse3, h4, h8, h16, h32
JMP_TABLE ipred_cfl_ac_444_16bpc, ssse3, w4, w8, w16, w32
JMP_TABLE pal_pred_16bpc,         ssse3, w4, w8, w16, w32, w64

cextern smooth_weights_1d_16bpc
cextern smooth_weights_2d_16bpc
cextern filter_intra_taps

SECTION .text

INIT_XMM ssse3
cglobal ipred_dc_top_16bpc, 3, 7, 6, dst, stride, tl, w, h
    LEA                  r5, ipred_dc_left_16bpc_ssse3_table
    movd                 m4, wm
    tzcnt                wd, wm
    add                 tlq, 2
    movifnidn            hd, hm
    pxor                 m3, m3
    pavgw                m4, m3
    movd                 m5, wd
    movu                 m0, [tlq]
    movsxd               r6, [r5+wq*4]
    add                  r6, r5
    add                  r5, ipred_dc_128_16bpc_ssse3_table-ipred_dc_left_16bpc_ssse3_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6

cglobal ipred_dc_left_16bpc, 3, 7, 6, dst, stride, tl, w, h, stride3
    LEA                  r5, ipred_dc_left_16bpc_ssse3_table
    mov                  hd, hm
    movd                 m4, hm
    tzcnt               r6d, hd
    sub                 tlq, hq
    tzcnt                wd, wm
    pxor                 m3, m3
    sub                 tlq, hq
    pavgw                m4, m3
    movd                 m5, r6d
    movu                 m0, [tlq]
    movsxd               r6, [r5+r6*4]
    add                  r6, r5
    add                  r5, ipred_dc_128_16bpc_ssse3_table-ipred_dc_left_16bpc_ssse3_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6
.h64:
    movu                 m2, [tlq+112]
    movu                 m1, [tlq+ 96]
    paddw                m0, m2
    movu                 m2, [tlq+ 80]
    paddw                m1, m2
    movu                 m2, [tlq+ 64]
    paddw                m0, m2
    paddw                m0, m1
.h32:
    movu                 m1, [tlq+ 48]
    movu                 m2, [tlq+ 32]
    paddw                m1, m2
    paddw                m0, m1
.h16:
    movu                 m1, [tlq+ 16]
    paddw                m0, m1
.h8:
    movhlps              m1, m0
    paddw                m0, m1
.h4:
    punpcklwd            m0, m3
    paddd                m4, m0
    punpckhqdq           m0, m0
    paddd                m0, m4
    pshuflw              m4, m0, q1032
    paddd                m0, m4
    psrld                m0, m5
    lea            stride3q, [strideq*3]
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    jmp                  wq

cglobal ipred_dc_16bpc, 4, 7, 6, dst, stride, tl, w, h, stride3
    movifnidn            hd, hm
    tzcnt               r6d, hd
    lea                 r5d, [wq+hq]
    movd                 m4, r5d
    tzcnt               r5d, r5d
    movd                 m5, r5d
    LEA                  r5, ipred_dc_16bpc_ssse3_table
    tzcnt                wd, wd
    movsxd               r6, [r5+r6*4]
    movsxd               wq, [r5+wq*4+5*4]
    pxor                 m3, m3
    psrlw                m4, 1
    add                  r6, r5
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  r6
.h4:
    movq                 m0, [tlq-8]
    jmp                  wq
.w4:
    movq                 m1, [tlq+2]
    paddw                m1, m0
    punpckhwd            m0, m3
    punpcklwd            m1, m3
    paddd                m0, m1
    paddd                m4, m0
    punpckhqdq           m0, m0
    paddd                m0, m4
    pshuflw              m1, m0, q1032
    paddd                m0, m1
    cmp                  hd, 4
    jg .w4_mul
    psrlw                m0, 3
    jmp .w4_end
.w4_mul:
    mov                 r2d, 0xAAAB
    mov                 r3d, 0x6667
    cmp                  hd, 16
    cmove               r2d, r3d
    psrld                m0, 2
    movd                 m1, r2d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w4_end:
    pshuflw              m0, m0, q0000
.s4:
    movq   [dstq+strideq*0], m0
    movq   [dstq+strideq*1], m0
    movq   [dstq+strideq*2], m0
    movq   [dstq+stride3q ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s4
    RET
.h8:
    mova                 m0, [tlq-16]
    jmp                  wq
.w8:
    movu                 m1, [tlq+2]
    paddw                m0, m1
    punpcklwd            m1, m0, m3
    punpckhwd            m0, m3
    paddd                m0, m1
    paddd                m4, m0
    punpckhqdq           m0, m0
    paddd                m0, m4
    pshuflw              m1, m0, q1032
    paddd                m0, m1
    psrld                m0, m5
    cmp                  hd, 8
    je .w8_end
    mov                 r2d, 0xAAAB
    mov                 r3d, 0x6667
    cmp                  hd, 32
    cmove               r2d, r3d
    movd                 m1, r2d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w8_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s8:
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m0
    mova   [dstq+strideq*2], m0
    mova   [dstq+stride3q ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s8
    RET
.h16:
    mova                 m0, [tlq-32]
    paddw                m0, [tlq-16]
    jmp                  wq
.w16:
    movu                 m1, [tlq+ 2]
    movu                 m2, [tlq+18]
    paddw                m1, m2
    paddw                m0, m1
    punpckhwd            m1, m0, m3
    punpcklwd            m0, m3
    paddd                m0, m1
    paddd                m4, m0
    punpckhqdq           m0, m0
    paddd                m0, m4
    pshuflw              m1, m0, q1032
    paddd                m0, m1
    psrld                m0, m5
    cmp                  hd, 16
    je .w16_end
    mov                 r2d, 0xAAAB
    mov                 r3d, 0x6667
    test                 hd, 8|32
    cmovz               r2d, r3d
    movd                 m1, r2d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w16_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s16c:
    mova                 m1, m0
.s16:
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*0+16*1], m1
    mova [dstq+strideq*1+16*0], m0
    mova [dstq+strideq*1+16*1], m1
    mova [dstq+strideq*2+16*0], m0
    mova [dstq+strideq*2+16*1], m1
    mova [dstq+stride3q +16*0], m0
    mova [dstq+stride3q +16*1], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s16
    RET
.h32:
    mova                 m0, [tlq-64]
    paddw                m0, [tlq-48]
    paddw                m0, [tlq-32]
    paddw                m0, [tlq-16]
    jmp                  wq
.w32:
    movu                 m1, [tlq+ 2]
    movu                 m2, [tlq+18]
    paddw                m1, m2
    movu                 m2, [tlq+34]
    paddw                m0, m2
    movu                 m2, [tlq+50]
    paddw                m1, m2
    paddw                m0, m1
    punpcklwd            m1, m0, m3
    punpckhwd            m0, m3
    paddd                m0, m1
    paddd                m4, m0
    punpckhqdq           m0, m0
    paddd                m0, m4
    pshuflw              m1, m0, q1032
    paddd                m0, m1
    psrld                m0, m5
    cmp                  hd, 32
    je .w32_end
    mov                 r2d, 0xAAAB
    mov                 r3d, 0x6667
    cmp                  hd, 8
    cmove               r2d, r3d
    movd                 m1, r2d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w32_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s32c:
    mova                 m1, m0
    mova                 m2, m0
    mova                 m3, m0
.s32:
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*0+16*1], m1
    mova [dstq+strideq*0+16*2], m2
    mova [dstq+strideq*0+16*3], m3
    mova [dstq+strideq*1+16*0], m0
    mova [dstq+strideq*1+16*1], m1
    mova [dstq+strideq*1+16*2], m2
    mova [dstq+strideq*1+16*3], m3
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .s32
    RET
.h64:
    mova                 m0, [tlq-128]
    mova                 m1, [tlq-112]
    paddw                m0, [tlq- 96]
    paddw                m1, [tlq- 80]
    paddw                m0, [tlq- 64]
    paddw                m1, [tlq- 48]
    paddw                m0, [tlq- 32]
    paddw                m1, [tlq- 16]
    paddw                m0, m1
    jmp                  wq
.w64:
    movu                 m1, [tlq+  2]
    movu                 m2, [tlq+ 18]
    paddw                m1, m2
    movu                 m2, [tlq+ 34]
    paddw                m0, m2
    movu                 m2, [tlq+ 50]
    paddw                m1, m2
    movu                 m2, [tlq+ 66]
    paddw                m0, m2
    movu                 m2, [tlq+ 82]
    paddw                m1, m2
    movu                 m2, [tlq+ 98]
    paddw                m0, m2
    movu                 m2, [tlq+114]
    paddw                m1, m2
    paddw                m0, m1
    punpcklwd            m1, m0, m3
    punpckhwd            m0, m3
    paddd                m0, m1
    paddd                m4, m0
    punpckhqdq           m0, m0
    paddd                m0, m4
    pshuflw              m1, m0, q1032
    paddd                m0, m1
    psrld                m0, m5
    cmp                  hd, 64
    je .w64_end
    mov                 r2d, 0xAAAB
    mov                 r3d, 0x6667
    cmp                  hd, 16
    cmove               r2d, r3d
    movd                 m1, r2d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w64_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s64:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m0
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m0
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m0
    mova        [dstq+16*6], m0
    mova        [dstq+16*7], m0
    add                dstq, strideq
    dec                  hd
    jg .s64
    RET

cglobal ipred_dc_128_16bpc, 2, 7, 6, dst, stride, tl, w, h, stride3
    mov                 r6d, r8m
    LEA                  r5, ipred_dc_128_16bpc_ssse3_table
    tzcnt                wd, wm
    shr                 r6d, 11
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    movddup              m0, [r5-ipred_dc_128_16bpc_ssse3_table+pw_512+r6*8]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

cglobal ipred_v_16bpc, 4, 7, 6, dst, stride, tl, w, h, stride3
    LEA                  r5, ipred_dc_splat_16bpc_ssse3_table
    movifnidn            hd, hm
    movu                 m0, [tlq+  2]
    movu                 m1, [tlq+ 18]
    movu                 m2, [tlq+ 34]
    movu                 m3, [tlq+ 50]
    cmp                  wd, 64
    je .w64
    tzcnt                wd, wd
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq
.w64:
    WIN64_SPILL_XMM 8
    movu                 m4, [tlq+ 66]
    movu                 m5, [tlq+ 82]
    movu                 m6, [tlq+ 98]
    movu                 m7, [tlq+114]
.w64_loop:
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    mova        [dstq+16*2], m2
    mova        [dstq+16*3], m3
    mova        [dstq+16*4], m4
    mova        [dstq+16*5], m5
    mova        [dstq+16*6], m6
    mova        [dstq+16*7], m7
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET

cglobal ipred_h_16bpc, 3, 6, 4, dst, stride, tl, w, h, stride3
%define base r5-ipred_h_16bpc_ssse3_table
    tzcnt                wd, wm
    LEA                  r5, ipred_h_16bpc_ssse3_table
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    movddup              m2, [base+pb_0_1]
    movddup              m3, [base+pb_2_3]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    sub                 tlq, 8
    movq                 m3, [tlq]
    pshuflw              m0, m3, q3333
    pshuflw              m1, m3, q2222
    pshuflw              m2, m3, q1111
    pshuflw              m3, m3, q0000
    movq   [dstq+strideq*0], m0
    movq   [dstq+strideq*1], m1
    movq   [dstq+strideq*2], m2
    movq   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
.w8:
    sub                 tlq, 8
    movq                 m3, [tlq]
    punpcklwd            m3, m3
    pshufd               m0, m3, q3333
    pshufd               m1, m3, q2222
    pshufd               m2, m3, q1111
    pshufd               m3, m3, q0000
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8
    RET
.w16:
    sub                 tlq, 4
    movd                 m1, [tlq]
    pshufb               m0, m1, m3
    pshufb               m1, m2
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*0+16*1], m0
    mova [dstq+strideq*1+16*0], m1
    mova [dstq+strideq*1+16*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16
    RET
.w32:
    sub                 tlq, 4
    movd                 m1, [tlq]
    pshufb               m0, m1, m3
    pshufb               m1, m2
    mova [dstq+strideq*0+16*0], m0
    mova [dstq+strideq*0+16*1], m0
    mova [dstq+strideq*0+16*2], m0
    mova [dstq+strideq*0+16*3], m0
    mova [dstq+strideq*1+16*0], m1
    mova [dstq+strideq*1+16*1], m1
    mova [dstq+strideq*1+16*2], m1
    mova [dstq+strideq*1+16*3], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32
    RET
.w64:
    sub                 tlq, 2
    movd                 m0, [tlq]
    pshufb               m0, m2
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m0
    mova        [dstq+16*2], m0
    mova        [dstq+16*3], m0
    mova        [dstq+16*4], m0
    mova        [dstq+16*5], m0
    mova        [dstq+16*6], m0
    mova        [dstq+16*7], m0
    add                dstq, strideq
    dec                  hd
    jg .w64
    RET

cglobal ipred_paeth_16bpc, 4, 6, 8, dst, stride, tl, w, h, left
%define base r5-ipred_paeth_16bpc_ssse3_table
    movifnidn            hd, hm
    pshuflw              m4, [tlq], q0000
    mov               leftq, tlq
    add                  hd, hd
    punpcklqdq           m4, m4      ; topleft
    sub               leftq, hq
    and                  wd, ~7
    jnz .w8
    movddup              m5, [tlq+2] ; top
    psubw                m6, m5, m4
    pabsw                m7, m6
.w4_loop:
    movd                 m1, [leftq+hq-4]
    punpcklwd            m1, m1
    punpckldq            m1, m1      ; left
%macro PAETH 0
    paddw                m0, m6, m1
    psubw                m2, m4, m0  ; tldiff
    psubw                m0, m5      ; tdiff
    pabsw                m2, m2
    pabsw                m0, m0
    pminsw               m2, m0
    pcmpeqw              m0, m2
    pand                 m3, m5, m0
    pandn                m0, m4
    por                  m0, m3
    pcmpgtw              m3, m7, m2
    pand                 m0, m3
    pandn                m3, m1
    por                  m0, m3
%endmacro
    PAETH
    movhps [dstq+strideq*0], m0
    movq   [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2*2
    jg .w4_loop
    RET
.w8:
%if ARCH_X86_32
    PUSH                 r6
    %define             r7d  hm
    %assign regs_used     7
%elif WIN64
    movaps              r4m, m8
    PUSH                 r7
    %assign regs_used     8
%endif
%if ARCH_X86_64
    movddup              m8, [pb_0_1]
%endif
    lea                 tlq, [tlq+wq*2+2]
    neg                  wq
    mov                 r7d, hd
.w8_loop0:
    movu                 m5, [tlq+wq*2]
    mov                  r6, dstq
    add                dstq, 16
    psubw                m6, m5, m4
    pabsw                m7, m6
.w8_loop:
    movd                 m1, [leftq+hq-2]
%if ARCH_X86_64
    pshufb               m1, m8
%else
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
%endif
    PAETH
    mova               [r6], m0
    add                  r6, strideq
    sub                  hd, 1*2
    jg .w8_loop
    mov                  hd, r7d
    add                  wq, 8
    jl .w8_loop0
%if WIN64
    movaps               m8, r4m
%endif
    RET

%if ARCH_X86_64
DECLARE_REG_TMP 7
%else
DECLARE_REG_TMP 4
%endif

cglobal ipred_smooth_v_16bpc, 4, 6, 6, dst, stride, tl, w, h, weights
    LEA            weightsq, smooth_weights_1d_16bpc
    mov                  hd, hm
    lea            weightsq, [weightsq+hq*4]
    neg                  hq
    movd                 m5, [tlq+hq*2] ; bottom
    pshuflw              m5, m5, q0000
    punpcklqdq           m5, m5
    cmp                  wd, 4
    jne .w8
    movddup              m4, [tlq+2]    ; top
    lea                  r3, [strideq*3]
    psubw                m4, m5         ; top - bottom
.w4_loop:
    movq                 m1, [weightsq+hq*2]
    punpcklwd            m1, m1
    pshufd               m0, m1, q1100
    punpckhdq            m1, m1
    pmulhrsw             m0, m4
    pmulhrsw             m1, m4
    paddw                m0, m5
    paddw                m1, m5
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    movq   [dstq+strideq*2], m1
    movhps [dstq+r3       ], m1
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w4_loop
    RET
.w8:
%if ARCH_X86_32
    PUSH                 r6
    %assign regs_used     7
    mov                  hm, hq
    %define              hq  hm
%elif WIN64
    PUSH                 r7
    %assign regs_used     8
%endif
.w8_loop0:
    mov                  t0, hq
    movu                 m4, [tlq+2]
    add                 tlq, 16
    mov                  r6, dstq
    add                dstq, 16
    psubw                m4, m5
.w8_loop:
    movq                 m3, [weightsq+t0*2]
    punpcklwd            m3, m3
    pshufd               m0, m3, q0000
    pshufd               m1, m3, q1111
    pshufd               m2, m3, q2222
    pshufd               m3, m3, q3333
    REPX   {pmulhrsw x, m4}, m0, m1, m2, m3
    REPX   {paddw    x, m5}, m0, m1, m2, m3
    mova     [r6+strideq*0], m0
    mova     [r6+strideq*1], m1
    lea                  r6, [r6+strideq*2]
    mova     [r6+strideq*0], m2
    mova     [r6+strideq*1], m3
    lea                  r6, [r6+strideq*2]
    add                  t0, 4
    jl .w8_loop
    sub                  wd, 8
    jg .w8_loop0
    RET

cglobal ipred_smooth_h_16bpc, 3, 6, 6, dst, stride, tl, w, h, weights
    LEA            weightsq, smooth_weights_1d_16bpc
    mov                  wd, wm
    movifnidn            hd, hm
    movd                 m5, [tlq+wq*2] ; right
    sub                 tlq, 8
    add                  hd, hd
    pshuflw              m5, m5, q0000
    sub                 tlq, hq
    punpcklqdq           m5, m5
    cmp                  wd, 4
    jne .w8
    movddup              m4, [weightsq+4*2]
    lea                  r3, [strideq*3]
.w4_loop:
    movq                 m1, [tlq+hq]   ; left
    punpcklwd            m1, m1
    psubw                m1, m5         ; left - right
    pshufd               m0, m1, q3322
    punpckldq            m1, m1
    pmulhrsw             m0, m4
    pmulhrsw             m1, m4
    paddw                m0, m5
    paddw                m1, m5
    movhps [dstq+strideq*0], m0
    movq   [dstq+strideq*1], m0
    movhps [dstq+strideq*2], m1
    movq   [dstq+r3       ], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4*2
    jg .w4_loop
    RET
.w8:
    lea            weightsq, [weightsq+wq*4]
    neg                  wq
%if ARCH_X86_32
    PUSH                 r6
    %assign regs_used     7
    %define              hd  hm
%elif WIN64
    PUSH                 r7
    %assign regs_used     8
%endif
.w8_loop0:
    mov                 t0d, hd
    mova                 m4, [weightsq+wq*2]
    mov                  r6, dstq
    add                dstq, 16
.w8_loop:
    movq                 m3, [tlq+t0*(1+ARCH_X86_32)]
    punpcklwd            m3, m3
    psubw                m3, m5
    pshufd               m0, m3, q3333
    pshufd               m1, m3, q2222
    pshufd               m2, m3, q1111
    pshufd               m3, m3, q0000
    REPX   {pmulhrsw x, m4}, m0, m1, m2, m3
    REPX   {paddw    x, m5}, m0, m1, m2, m3
    mova     [r6+strideq*0], m0
    mova     [r6+strideq*1], m1
    lea                  r6, [r6+strideq*2]
    mova     [r6+strideq*0], m2
    mova     [r6+strideq*1], m3
    lea                  r6, [r6+strideq*2]
    sub                 t0d, 4*(1+ARCH_X86_64)
    jg .w8_loop
    add                  wq, 8
    jl .w8_loop0
    RET

%if ARCH_X86_64
DECLARE_REG_TMP 10
%else
DECLARE_REG_TMP 3
%endif

cglobal ipred_smooth_16bpc, 3, 7, 8, dst, stride, tl, w, h, \
                                     h_weights, v_weights, top
    LEA          h_weightsq, smooth_weights_2d_16bpc
    mov                  wd, wm
    mov                  hd, hm
    movd                 m7, [tlq+wq*2] ; right
    lea          v_weightsq, [h_weightsq+hq*8]
    neg                  hq
    movd                 m6, [tlq+hq*2] ; bottom
    pshuflw              m7, m7, q0000
    pshuflw              m6, m6, q0000
    cmp                  wd, 4
    jne .w8
    movq                 m4, [tlq+2]    ; top
    mova                 m5, [h_weightsq+4*4]
    punpcklwd            m4, m6         ; top, bottom
    pxor                 m6, m6
.w4_loop:
    movq                 m1, [v_weightsq+hq*4]
    sub                 tlq, 4
    movd                 m3, [tlq]      ; left
    pshufd               m0, m1, q0000
    pshufd               m1, m1, q1111
    pmaddwd              m0, m4
    punpcklwd            m3, m7         ; left, right
    pmaddwd              m1, m4
    pshufd               m2, m3, q1111
    pshufd               m3, m3, q0000
    pmaddwd              m2, m5
    pmaddwd              m3, m5
    paddd                m0, m2
    paddd                m1, m3
    psrld                m0, 8
    psrld                m1, 8
    packssdw             m0, m1
    pavgw                m0, m6
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    add                  hq, 2
    jl .w4_loop
    RET
.w8:
%if ARCH_X86_32
    lea          h_weightsq, [h_weightsq+wq*4]
    mov                  t0, tlq
    mov                 r1m, tlq
    mov                 r2m, hq
    %define              m8  [h_weightsq+16*0]
    %define              m9  [h_weightsq+16*1]
%else
%if WIN64
    movaps              r4m, m8
    movaps              r6m, m9
    PUSH                 r7
    PUSH                 r8
%endif
    PUSH                 r9
    PUSH                r10
    %assign       regs_used  11
    lea          h_weightsq, [h_weightsq+wq*8]
    lea                topq, [tlq+wq*2]
    neg                  wq
    mov                  r8, tlq
    mov                  r9, hq
%endif
    punpcklqdq           m6, m6
.w8_loop0:
%if ARCH_X86_32
    movu                 m5, [t0+2]
    add                  t0, 16
    mov                 r0m, t0
%else
    movu                 m5, [topq+wq*2+2]
    mova                 m8, [h_weightsq+wq*4+16*0]
    mova                 m9, [h_weightsq+wq*4+16*1]
%endif
    mov                  t0, dstq
    add                dstq, 16
    punpcklwd            m4, m5, m6
    punpckhwd            m5, m6
.w8_loop:
    movd                 m1, [v_weightsq+hq*4]
    sub                 tlq, 2
    movd                 m3, [tlq]      ; left
    pshufd               m1, m1, q0000
    pmaddwd              m0, m4, m1
    pshuflw              m3, m3, q0000
    pmaddwd              m1, m5
    punpcklwd            m3, m7         ; left, right
    pmaddwd              m2, m8, m3
    pmaddwd              m3, m9
    paddd                m0, m2
    paddd                m1, m3
    psrld                m0, 8
    psrld                m1, 8
    packssdw             m0, m1
    pxor                 m1, m1
    pavgw                m0, m1
    mova               [t0], m0
    add                  t0, strideq
    inc                  hq
    jl .w8_loop
%if ARCH_X86_32
    mov                  t0, r0m
    mov                 tlq, r1m
    add          h_weightsq, 16*2
    mov                  hq, r2m
    sub            dword wm, 8
    jg .w8_loop0
%else
    mov                 tlq, r8
    mov                  hq, r9
    add                  wq, 8
    jl .w8_loop0
%endif
%if WIN64
    movaps               m8, r4m
    movaps               m9, r6m
%endif
    RET

%if ARCH_X86_64
cglobal ipred_filter_16bpc, 4, 7, 16, dst, stride, tl, w, h, filter
%else
cglobal ipred_filter_16bpc, 4, 7, 8, -16*8, dst, stride, tl, w, h, filter
%define  m8 [esp+16*0]
%define  m9 [esp+16*1]
%define m10 [esp+16*2]
%define m11 [esp+16*3]
%define m12 [esp+16*4]
%define m13 [esp+16*5]
%define m14 [esp+16*6]
%define m15 [esp+16*7]
%endif
%define base r6-$$
    movifnidn            hd, hm
    movd                 m6, r8m     ; bitdepth_max
%ifidn filterd, filterm
    movzx           filterd, filterb
%else
    movzx           filterd, byte filterm
%endif
    LEA                  r6, $$
    shl             filterd, 6
    movu                 m0, [tlq-6] ; __ l1 l0 tl t0 t1 t2 t3
    mova                 m1, [base+filter_intra_taps+filterq+16*0]
    mova                 m2, [base+filter_intra_taps+filterq+16*1]
    mova                 m3, [base+filter_intra_taps+filterq+16*2]
    mova                 m4, [base+filter_intra_taps+filterq+16*3]
    pxor                 m5, m5
%if ARCH_X86_64
    punpcklbw            m8, m5, m1  ; place 8-bit coefficients in the upper
    punpckhbw            m9, m5, m1  ; half of each 16-bit word to avoid
    punpcklbw           m10, m5, m2  ; having to perform sign-extension.
    punpckhbw           m11, m5, m2
    punpcklbw           m12, m5, m3
    punpckhbw           m13, m5, m3
    punpcklbw           m14, m5, m4
    punpckhbw           m15, m5, m4
%else
    punpcklbw            m7, m5, m1
    mova                 m8, m7
    punpckhbw            m7, m5, m1
    mova                 m9, m7
    punpcklbw            m7, m5, m2
    mova                m10, m7
    punpckhbw            m7, m5, m2
    mova                m11, m7
    punpcklbw            m7, m5, m3
    mova                m12, m7
    punpckhbw            m7, m5, m3
    mova                m13, m7
    punpcklbw            m7, m5, m4
    mova                m14, m7
    punpckhbw            m7, m5, m4
    mova                m15, m7
%endif
    mova                 m7, [base+filter_shuf]
    add                  hd, hd
    mov                  r5, dstq
    pshuflw              m6, m6, q0000
    mov                  r6, tlq
    punpcklqdq           m6, m6
    sub                 tlq, hq
.left_loop:
    pshufb               m0, m7      ; tl t0 t1 t2 t3 l0 l1 __
    pshufd               m1, m0, q0000
    pmaddwd              m2, m8, m1
    pmaddwd              m1, m9
    pshufd               m4, m0, q1111
    pmaddwd              m3, m10, m4
    pmaddwd              m4, m11
    paddd                m2, m3
    paddd                m1, m4
    pshufd               m4, m0, q2222
    pmaddwd              m3, m12, m4
    pmaddwd              m4, m13
    paddd                m2, m3
    paddd                m1, m4
    pshufd               m3, m0, q3333
    pmaddwd              m0, m14, m3
    pmaddwd              m3, m15
    paddd                m0, m2
    paddd                m1, m3
    psrad                m0, 11     ; x >> 3
    psrad                m1, 11
    packssdw             m0, m1
    pmaxsw               m0, m5
    pavgw                m0, m5     ; (x + 8) >> 4
    pminsw               m0, m6
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    movlps               m0, [tlq+hq-10]
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2*2
    jg .left_loop
    sub                  wd, 4
    jz .end
    sub                 tld, r6d     ; -h*2
    sub                  r6, r5      ; tl-dst
.right_loop0:
    add                  r5, 8
    mov                  hd, tld
    movu                 m0, [r5+r6] ; tl t0 t1 t2 t3 __ __ __
    mov                dstq, r5
.right_loop:
    pshufd               m2, m0, q0000
    pmaddwd              m1, m8, m2
    pmaddwd              m2, m9
    pshufd               m4, m0, q1111
    pmaddwd              m3, m10, m4
    pmaddwd              m4, m11
    pinsrw               m0, [dstq+strideq*0-2], 5
    paddd                m1, m3
    paddd                m2, m4
    pshufd               m0, m0, q2222
    movddup              m4, [dstq+strideq*1-8]
    pmaddwd              m3, m12, m0
    pmaddwd              m0, m13
    paddd                m1, m3
    paddd                m0, m2
    pshuflw              m2, m4, q3333
    punpcklwd            m2, m5
    pmaddwd              m3, m14, m2
    pmaddwd              m2, m15
    paddd                m1, m3
    paddd                m0, m2
    psrad                m1, 11
    psrad                m0, 11
    packssdw             m0, m1
    pmaxsw               m0, m5
    pavgw                m0, m5
    pminsw               m0, m6
    movhps [dstq+strideq*0], m0
    movq   [dstq+strideq*1], m0
    palignr              m0, m4, 14
    lea                dstq, [dstq+strideq*2]
    add                  hd, 2*2
    jl .right_loop
    sub                  wd, 4
    jg .right_loop0
.end:
    RET

%if UNIX64
DECLARE_REG_TMP 7
%else
DECLARE_REG_TMP 5
%endif

cglobal ipred_cfl_top_16bpc, 4, 7, 8, dst, stride, tl, w, h, ac
    LEA                  t0, ipred_cfl_left_16bpc_ssse3_table
    movd                 m4, wd
    tzcnt                wd, wd
    movifnidn            hd, hm
    add                 tlq, 2
    movsxd               r6, [t0+wq*4]
    movd                 m5, wd
    jmp mangle(private_prefix %+ _ipred_cfl_left_16bpc_ssse3.start)

cglobal ipred_cfl_left_16bpc, 3, 7, 8, dst, stride, tl, w, h, ac, alpha
    movifnidn            hd, hm
    LEA                  t0, ipred_cfl_left_16bpc_ssse3_table
    tzcnt                wd, wm
    lea                 r6d, [hq*2]
    movd                 m4, hd
    sub                 tlq, r6
    tzcnt               r6d, hd
    movd                 m5, r6d
    movsxd               r6, [t0+r6*4]
.start:
    movd                 m7, r7m
    movu                 m0, [tlq]
    add                  r6, t0
    add                  t0, ipred_cfl_splat_16bpc_ssse3_table-ipred_cfl_left_16bpc_ssse3_table
    movsxd               wq, [t0+wq*4]
    pxor                 m6, m6
    pshuflw              m7, m7, q0000
    pcmpeqw              m3, m3
    add                  wq, t0
    movifnidn           acq, acmp
    pavgw                m4, m6
    punpcklqdq           m7, m7
    jmp                  r6
.h32:
    movu                 m1, [tlq+48]
    movu                 m2, [tlq+32]
    paddw                m0, m1
    paddw                m0, m2
.h16:
    movu                 m1, [tlq+16]
    paddw                m0, m1
.h8:
    pshufd               m1, m0, q1032
    paddw                m0, m1
.h4:
    pmaddwd              m0, m3
    psubd                m4, m0
    pshuflw              m0, m4, q1032
    paddd                m0, m4
    psrld                m0, m5
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
    jmp                  wq

%macro IPRED_CFL 2 ; dst, src
    pabsw               m%1, m%2
    pmulhrsw            m%1, m2
    psignw              m%2, m1
    psignw              m%1, m%2
    paddw               m%1, m0
    pmaxsw              m%1, m6
    pminsw              m%1, m7
%endmacro

cglobal ipred_cfl_16bpc, 4, 7, 8, dst, stride, tl, w, h, ac, alpha
    movifnidn            hd, hm
    tzcnt               r6d, hd
    lea                 t0d, [wq+hq]
    movd                 m4, t0d
    tzcnt               t0d, t0d
    movd                 m5, t0d
    LEA                  t0, ipred_cfl_16bpc_ssse3_table
    tzcnt                wd, wd
    movd                 m7, r7m
    movsxd               r6, [t0+r6*4]
    movsxd               wq, [t0+wq*4+4*4]
    psrlw                m4, 1
    pxor                 m6, m6
    pshuflw              m7, m7, q0000
    add                  r6, t0
    add                  wq, t0
    movifnidn           acq, acmp
    pcmpeqw              m3, m3
    punpcklqdq           m7, m7
    jmp                  r6
.h4:
    movq                 m0, [tlq-8]
    jmp                  wq
.w4:
    movq                 m1, [tlq+2]
    paddw                m0, m1
    pmaddwd              m0, m3
    psubd                m4, m0
    pshufd               m0, m4, q1032
    paddd                m0, m4
    pshuflw              m4, m0, q1032
    paddd                m0, m4
    cmp                  hd, 4
    jg .w4_mul
    psrld                m0, 3
    jmp .w4_end
.w4_mul:
    mov                 r6d, 0xAAAB
    mov                 r2d, 0x6667
    cmp                  hd, 16
    cmove               r6d, r2d
    movd                 m1, r6d
    psrld                m0, 2
    pmulhuw              m0, m1
    psrlw                m0, 1
.w4_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s4:
    movd                 m1, alpham
    lea                  r6, [strideq*3]
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    pabsw                m2, m1
    psllw                m2, 9
.s4_loop:
    mova                 m4, [acq+16*0]
    mova                 m5, [acq+16*1]
    add                 acq, 16*2
    IPRED_CFL             3, 4
    IPRED_CFL             4, 5
    movq   [dstq+strideq*0], m3
    movhps [dstq+strideq*1], m3
    movq   [dstq+strideq*2], m4
    movhps [dstq+r6       ], m4
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s4_loop
    RET
.h8:
    mova                 m0, [tlq-16]
    jmp                  wq
.w8:
    movu                 m1, [tlq+2]
    paddw                m0, m1
    pmaddwd              m0, m3
    psubd                m4, m0
    pshufd               m0, m4, q1032
    paddd                m0, m4
    pshuflw              m4, m0, q1032
    paddd                m0, m4
    psrld                m0, m5
    cmp                  hd, 8
    je .w8_end
    mov                 r6d, 0xAAAB
    mov                 r2d, 0x6667
    cmp                  hd, 32
    cmove               r6d, r2d
    movd                 m1, r6d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w8_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s8:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    pabsw                m2, m1
    psllw                m2, 9
.s8_loop:
    mova                 m4, [acq+16*0]
    mova                 m5, [acq+16*1]
    add                 acq, 16*2
    IPRED_CFL             3, 4
    IPRED_CFL             4, 5
    mova   [dstq+strideq*0], m3
    mova   [dstq+strideq*1], m4
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .s8_loop
    RET
.h16:
    mova                 m0, [tlq-32]
    paddw                m0, [tlq-16]
    jmp                  wq
.w16:
    movu                 m1, [tlq+ 2]
    movu                 m2, [tlq+18]
    paddw                m1, m2
    paddw                m0, m1
    pmaddwd              m0, m3
    psubd                m4, m0
    pshufd               m0, m4, q1032
    paddd                m0, m4
    pshuflw              m4, m0, q1032
    paddd                m0, m4
    psrld                m0, m5
    cmp                  hd, 16
    je .w16_end
    mov                 r6d, 0xAAAB
    mov                 r2d, 0x6667
    test                 hd, 8|32
    cmovz               r6d, r2d
    movd                 m1, r6d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w16_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s16:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    pabsw                m2, m1
    psllw                m2, 9
.s16_loop:
    mova                 m4, [acq+16*0]
    mova                 m5, [acq+16*1]
    add                 acq, 16*2
    IPRED_CFL             3, 4
    IPRED_CFL             4, 5
    mova        [dstq+16*0], m3
    mova        [dstq+16*1], m4
    add                dstq, strideq
    dec                  hd
    jg .s16_loop
    RET
.h32:
    mova                 m0, [tlq-64]
    paddw                m0, [tlq-48]
    paddw                m0, [tlq-32]
    paddw                m0, [tlq-16]
    jmp                  wq
.w32:
    movu                 m1, [tlq+ 2]
    movu                 m2, [tlq+18]
    paddw                m1, m2
    movu                 m2, [tlq+34]
    paddw                m1, m2
    movu                 m2, [tlq+50]
    paddw                m1, m2
    paddw                m0, m1
    pmaddwd              m0, m3
    psubd                m4, m0
    pshufd               m0, m4, q1032
    paddd                m0, m4
    pshuflw              m4, m0, q1032
    paddd                m0, m4
    psrld                m0, m5
    cmp                  hd, 32
    je .w32_end
    mov                 r6d, 0xAAAB
    mov                 r2d, 0x6667
    cmp                  hd, 8
    cmove               r6d, r2d
    movd                 m1, r6d
    pmulhuw              m0, m1
    psrlw                m0, 1
.w32_end:
    pshuflw              m0, m0, q0000
    punpcklqdq           m0, m0
.s32:
    movd                 m1, alpham
    pshuflw              m1, m1, q0000
    punpcklqdq           m1, m1
    pabsw                m2, m1
    psllw                m2, 9
.s32_loop:
    mova                 m4, [acq+16*0]
    mova                 m5, [acq+16*1]
    IPRED_CFL             3, 4
    IPRED_CFL             4, 5
    mova        [dstq+16*0], m3
    mova        [dstq+16*1], m4
    mova                 m4, [acq+16*2]
    mova                 m5, [acq+16*3]
    add                 acq, 16*4
    IPRED_CFL             3, 4
    IPRED_CFL             4, 5
    mova        [dstq+16*2], m3
    mova        [dstq+16*3], m4
    add                dstq, strideq
    dec                  hd
    jg .s32_loop
    RET

cglobal ipred_cfl_128_16bpc, 3, 7, 8, dst, stride, tl, w, h, ac
    tzcnt                wd, wm
    LEA                  t0, ipred_cfl_splat_16bpc_ssse3_table
    mov                 r6d, r7m
    movifnidn            hd, hm
    shr                 r6d, 11
    movd                 m7, r7m
    movsxd               wq, [t0+wq*4]
    movddup              m0, [t0-ipred_cfl_splat_16bpc_ssse3_table+pw_512+r6*8]
    pshuflw              m7, m7, q0000
    pxor                 m6, m6
    add                  wq, t0
    movifnidn           acq, acmp
    punpcklqdq           m7, m7
    jmp                  wq

cglobal ipred_cfl_ac_420_16bpc, 3, 7, 6, ac, ypx, stride, wpad, hpad, w, h
    movifnidn         hpadd, hpadm
%if ARCH_X86_32 && PIC
    pcmpeqw              m5, m5
    pabsw                m5, m5
    paddw                m5, m5
%else
    movddup              m5, [pw_2]
%endif
    mov                  hd, hm
    shl               hpadd, 2
    pxor                 m4, m4
    sub                  hd, hpadd
    cmp            dword wm, 8
    mov                  r5, acq
    jg .w16
    je .w8
    lea                  r3, [strideq*3]
.w4_loop:
    pmaddwd              m0, m5, [ypxq+strideq*0]
    pmaddwd              m1, m5, [ypxq+strideq*1]
    pmaddwd              m2, m5, [ypxq+strideq*2]
    pmaddwd              m3, m5, [ypxq+r3       ]
    lea                ypxq, [ypxq+strideq*4]
    paddd                m0, m1
    paddd                m2, m3
    paddd                m4, m0
    packssdw             m0, m2
    paddd                m4, m2
    mova              [acq], m0
    add                 acq, 16
    sub                  hd, 2
    jg .w4_loop
    test              hpadd, hpadd
    jz .dc
    punpckhqdq           m0, m0
    pslld                m2, 2
.w4_hpad:
    mova         [acq+16*0], m0
    paddd                m4, m2
    mova         [acq+16*1], m0
    add                 acq, 16*2
    sub               hpadd, 4
    jg .w4_hpad
    jmp .dc
.w8:
%if ARCH_X86_32
    cmp         dword wpadm, 0
%else
    test              wpadd, wpadd
%endif
    jnz .w8_wpad1
.w8_loop:
    pmaddwd              m0, m5, [ypxq+strideq*0+16*0]
    pmaddwd              m2, m5, [ypxq+strideq*1+16*0]
    pmaddwd              m1, m5, [ypxq+strideq*0+16*1]
    pmaddwd              m3, m5, [ypxq+strideq*1+16*1]
    lea                ypxq, [ypxq+strideq*2]
    paddd                m0, m2
    paddd                m1, m3
    paddd                m2, m0, m1
    packssdw             m0, m1
    paddd                m4, m2
    mova              [acq], m0
    add                 acq, 16
    dec                  hd
    jg .w8_loop
.w8_hpad:
    test              hpadd, hpadd
    jz .dc
    pslld                m2, 2
    mova                 m1, m0
    jmp .hpad
.w8_wpad1:
    pmaddwd              m0, m5, [ypxq+strideq*0]
    pmaddwd              m1, m5, [ypxq+strideq*1]
    lea                ypxq, [ypxq+strideq*2]
    paddd                m0, m1
    pshufd               m1, m0, q3333
    paddd                m2, m0, m1
    packssdw             m0, m1
    paddd                m4, m2
    mova              [acq], m0
    add                 acq, 16
    dec                  hd
    jg .w8_wpad1
    jmp .w8_hpad
.w16_wpad3:
    pshufd               m3, m0, q3333
    mova                 m1, m3
    mova                 m2, m3
    jmp .w16_wpad_end
.w16_wpad2:
    pshufd               m1, m3, q3333
    mova                 m2, m1
    jmp .w16_wpad_end
.w16_wpad1:
    pshufd               m2, m1, q3333
    jmp .w16_wpad_end
.w16:
    movifnidn         wpadd, wpadm
    WIN64_SPILL_XMM       7
.w16_loop:
    pmaddwd              m0, m5, [ypxq+strideq*0+16*0]
    pmaddwd              m6, m5, [ypxq+strideq*1+16*0]
    paddd                m0, m6
    cmp               wpadd, 2
    jg .w16_wpad3
    pmaddwd              m3, m5, [ypxq+strideq*0+16*1]
    pmaddwd              m6, m5, [ypxq+strideq*1+16*1]
    paddd                m3, m6
    je .w16_wpad2
    pmaddwd              m1, m5, [ypxq+strideq*0+16*2]
    pmaddwd              m6, m5, [ypxq+strideq*1+16*2]
    paddd                m1, m6
    jp .w16_wpad1
    pmaddwd              m2, m5, [ypxq+strideq*0+16*3]
    pmaddwd              m6, m5, [ypxq+strideq*1+16*3]
    paddd                m2, m6
.w16_wpad_end:
    lea                ypxq, [ypxq+strideq*2]
    paddd                m6, m0, m3
    packssdw             m0, m3
    paddd                m6, m1
    mova         [acq+16*0], m0
    packssdw             m1, m2
    paddd                m2, m6
    mova         [acq+16*1], m1
    add                 acq, 16*2
    paddd                m4, m2
    dec                  hd
    jg .w16_loop
    WIN64_RESTORE_XMM
    add               hpadd, hpadd
    jz .dc
    paddd                m2, m2
.hpad:
    mova         [acq+16*0], m0
    mova         [acq+16*1], m1
    paddd                m4, m2
    mova         [acq+16*2], m0
    mova         [acq+16*3], m1
    add                 acq, 16*4
    sub               hpadd, 4
    jg .hpad
.dc:
    sub                  r5, acq ; -w*h*2
    pshufd               m2, m4, q1032
    tzcnt               r1d, r5d
    paddd                m2, m4
    sub                 r1d, 2
    pshufd               m4, m2, q2301
    movd                 m0, r1d
    paddd                m2, m4
    psrld                m2, m0
    pxor                 m0, m0
    pavgw                m2, m0
    packssdw             m2, m2
.dc_loop:
    mova                 m0, [acq+r5+16*0]
    mova                 m1, [acq+r5+16*1]
    psubw                m0, m2
    psubw                m1, m2
    mova      [acq+r5+16*0], m0
    mova      [acq+r5+16*1], m1
    add                  r5, 16*2
    jl .dc_loop
    RET

cglobal ipred_cfl_ac_422_16bpc, 3, 7, 6, ac, ypx, stride, wpad, hpad, w, h
    movifnidn         hpadd, hpadm
%if ARCH_X86_32 && PIC
    pcmpeqw              m5, m5
    pabsw                m5, m5
    psllw                m5, 2
%else
    movddup              m5, [pw_4]
%endif
    mov                  hd, hm
    shl               hpadd, 2
    pxor                 m4, m4
    sub                  hd, hpadd
    cmp            dword wm, 8
    mov                  r5, acq
    jg .w16
    je .w8
    lea                  r3, [strideq*3]
.w4_loop:
    pmaddwd              m0, m5, [ypxq+strideq*0]
    pmaddwd              m3, m5, [ypxq+strideq*1]
    pmaddwd              m1, m5, [ypxq+strideq*2]
    pmaddwd              m2, m5, [ypxq+r3       ]
    lea                ypxq, [ypxq+strideq*4]
    paddd                m4, m0
    packssdw             m0, m3
    paddd                m3, m1
    packssdw             m1, m2
    paddd                m4, m2
    paddd                m4, m3
    mova         [acq+16*0], m0
    mova         [acq+16*1], m1
    add                 acq, 16*2
    sub                  hd, 4
    jg .w4_loop
    test              hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
    punpckhqdq           m1, m1
    pslld                m2, 3
    mova         [acq+16*0], m1
    mova         [acq+16*1], m1
    paddd                m4, m2
    mova         [acq+16*2], m1
    mova         [acq+16*3], m1
    add                 acq, 16*4
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
.w8:
%if ARCH_X86_32
    cmp         dword wpadm, 0
%else
    test              wpadd, wpadd
%endif
    jnz .w8_wpad1
.w8_loop:
    pmaddwd              m0, m5, [ypxq+strideq*0+16*0]
    pmaddwd              m2, m5, [ypxq+strideq*0+16*1]
    pmaddwd              m1, m5, [ypxq+strideq*1+16*0]
    pmaddwd              m3, m5, [ypxq+strideq*1+16*1]
    lea                ypxq, [ypxq+strideq*2]
    paddd                m4, m0
    packssdw             m0, m2
    paddd                m4, m2
    mova         [acq+16*0], m0
    paddd                m2, m1, m3
    packssdw             m1, m3
    paddd                m4, m2
    mova         [acq+16*1], m1
    add                 acq, 16*2
    sub                  hd, 2
    jg .w8_loop
.w8_hpad:
    test              hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
    pslld                m2, 2
    mova                 m0, m1
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).hpad
.w8_wpad1:
    pmaddwd              m0, m5, [ypxq+strideq*0]
    pmaddwd              m1, m5, [ypxq+strideq*1]
    lea                ypxq, [ypxq+strideq*2]
    pshufd               m2, m0, q3333
    pshufd               m3, m1, q3333
    paddd                m4, m0
    packssdw             m0, m2
    paddd                m4, m2
    paddd                m2, m1, m3
    packssdw             m1, m3
    paddd                m4, m2
    mova         [acq+16*0], m0
    mova         [acq+16*1], m1
    add                 acq, 16*2
    sub                  hd, 2
    jg .w8_wpad1
    jmp .w8_hpad
.w16_wpad3:
    pshufd               m3, m0, q3333
    mova                 m1, m3
    mova                 m2, m3
    jmp .w16_wpad_end
.w16_wpad2:
    pshufd               m1, m3, q3333
    mova                 m2, m1
    jmp .w16_wpad_end
.w16_wpad1:
    pshufd               m2, m1, q3333
    jmp .w16_wpad_end
.w16:
    movifnidn         wpadd, wpadm
    WIN64_SPILL_XMM       7
.w16_loop:
    pmaddwd              m0, m5, [ypxq+16*0]
    cmp               wpadd, 2
    jg .w16_wpad3
    pmaddwd              m3, m5, [ypxq+16*1]
    je .w16_wpad2
    pmaddwd              m1, m5, [ypxq+16*2]
    jp .w16_wpad1
    pmaddwd              m2, m5, [ypxq+16*3]
.w16_wpad_end:
    add                ypxq, strideq
    paddd                m6, m0, m3
    packssdw             m0, m3
    mova         [acq+16*0], m0
    paddd                m6, m1
    packssdw             m1, m2
    paddd                m2, m6
    mova         [acq+16*1], m1
    add                 acq, 16*2
    paddd                m4, m2
    dec                  hd
    jg .w16_loop
    WIN64_RESTORE_XMM
    add               hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
    paddd                m2, m2
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).hpad

cglobal ipred_cfl_ac_444_16bpc, 3, 7, 6, ac, ypx, stride, wpad, hpad, w, h
%define base r6-ipred_cfl_ac_444_16bpc_ssse3_table
    LEA                  r6, ipred_cfl_ac_444_16bpc_ssse3_table
    tzcnt                wd, wm
    movifnidn         hpadd, hpadm
    pxor                 m4, m4
    movsxd               wq, [r6+wq*4]
    movddup              m5, [base+pw_1]
    add                  wq, r6
    mov                  hd, hm
    shl               hpadd, 2
    sub                  hd, hpadd
    jmp                  wq
.w4:
    lea                  r3, [strideq*3]
    mov                  r5, acq
.w4_loop:
    movq                 m0, [ypxq+strideq*0]
    movhps               m0, [ypxq+strideq*1]
    movq                 m1, [ypxq+strideq*2]
    movhps               m1, [ypxq+r3       ]
    lea                ypxq, [ypxq+strideq*4]
    psllw                m0, 3
    psllw                m1, 3
    mova         [acq+16*0], m0
    pmaddwd              m0, m5
    mova         [acq+16*1], m1
    pmaddwd              m2, m5, m1
    add                 acq, 16*2
    paddd                m4, m0
    paddd                m4, m2
    sub                  hd, 4
    jg .w4_loop
    test              hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
    punpckhqdq           m1, m1
    mova         [acq+16*0], m1
    pslld                m2, 2
    mova         [acq+16*1], m1
    punpckhqdq           m2, m2
    mova         [acq+16*2], m1
    paddd                m4, m2
    mova         [acq+16*3], m1
    add                 acq, 16*4
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
.w8:
    mov                  r5, acq
.w8_loop:
    mova                 m0, [ypxq+strideq*0]
    mova                 m1, [ypxq+strideq*1]
    lea                ypxq, [ypxq+strideq*2]
    psllw                m0, 3
    psllw                m1, 3
    mova         [acq+16*0], m0
    pmaddwd              m0, m5
    mova         [acq+16*1], m1
    pmaddwd              m2, m5, m1
    add                 acq, 16*2
    paddd                m4, m0
    paddd                m4, m2
    sub                  hd, 2
    jg .w8_loop
.w8_hpad:
    test              hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
    pslld                m2, 2
    mova                 m0, m1
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).hpad
.w16_wpad2:
    pshufhw              m3, m2, q3333
    pshufhw              m1, m0, q3333
    punpckhqdq           m3, m3
    punpckhqdq           m1, m1
    jmp .w16_wpad_end
.w16:
    movifnidn         wpadd, wpadm
    mov                  r5, acq
.w16_loop:
    mova                 m2, [ypxq+strideq*0+16*0]
    mova                 m0, [ypxq+strideq*1+16*0]
    psllw                m2, 3
    psllw                m0, 3
    test              wpadd, wpadd
    jnz .w16_wpad2
    mova                 m3, [ypxq+strideq*0+16*1]
    mova                 m1, [ypxq+strideq*1+16*1]
    psllw                m3, 3
    psllw                m1, 3
.w16_wpad_end:
    lea                ypxq, [ypxq+strideq*2]
    mova         [acq+16*0], m2
    pmaddwd              m2, m5
    mova         [acq+16*1], m3
    pmaddwd              m3, m5
    paddd                m4, m2
    pmaddwd              m2, m5, m0
    mova         [acq+16*2], m0
    paddd                m4, m3
    pmaddwd              m3, m5, m1
    mova         [acq+16*3], m1
    add                 acq, 16*4
    paddd                m2, m3
    paddd                m4, m2
    sub                  hd, 2
    jg .w16_loop
    add               hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
    paddd                m2, m2
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).hpad
.w32_wpad6:
    pshufhw              m1, m0, q3333
    punpckhqdq           m1, m1
    mova                 m2, m1
    mova                 m3, m1
    jmp .w32_wpad_end
.w32_wpad4:
    pshufhw              m2, m1, q3333
    punpckhqdq           m2, m2
    mova                 m3, m2
    jmp .w32_wpad_end
.w32_wpad2:
    pshufhw              m3, m2, q3333
    punpckhqdq           m3, m3
    jmp .w32_wpad_end
.w32:
    movifnidn         wpadd, wpadm
    mov                  r5, acq
    WIN64_SPILL_XMM       8
.w32_loop:
    mova                 m0, [ypxq+16*0]
    psllw                m0, 3
    cmp               wpadd, 4
    jg .w32_wpad6
    mova                 m1, [ypxq+16*1]
    psllw                m1, 3
    je .w32_wpad4
    mova                 m2, [ypxq+16*2]
    psllw                m2, 3
    jnp .w32_wpad2
    mova                 m3, [ypxq+16*3]
    psllw                m3, 3
.w32_wpad_end:
    add                ypxq, strideq
    pmaddwd              m6, m5, m0
    mova         [acq+16*0], m0
    pmaddwd              m7, m5, m1
    mova         [acq+16*1], m1
    paddd                m6, m7
    pmaddwd              m7, m5, m2
    mova         [acq+16*2], m2
    paddd                m6, m7
    pmaddwd              m7, m5, m3
    mova         [acq+16*3], m3
    add                 acq, 16*4
    paddd                m6, m7
    paddd                m4, m6
    dec                  hd
    jg .w32_loop
%if WIN64
    mova                 m5, m6
    WIN64_RESTORE_XMM
    SWAP                  5, 6
%endif
    test              hpadd, hpadd
    jz mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc
.w32_hpad_loop:
    mova         [acq+16*0], m0
    mova         [acq+16*1], m1
    paddd                m4, m6
    mova         [acq+16*2], m2
    mova         [acq+16*3], m3
    add                 acq, 16*4
    dec               hpadd
    jg .w32_hpad_loop
    jmp mangle(private_prefix %+ _ipred_cfl_ac_420_16bpc_ssse3).dc

cglobal pal_pred_16bpc, 4, 5, 5, dst, stride, pal, idx, w, h
%define base r2-pal_pred_16bpc_ssse3_table
%if ARCH_X86_32
    %define              hd  r2d
%endif
    mova                 m3, [palq]
    LEA                  r2, pal_pred_16bpc_ssse3_table
    tzcnt                wd, wm
    pshufb               m3, [base+pal_pred_shuf]
    movsxd               wq, [r2+wq*4]
    pshufd               m4, m3, q1032
    add                  wq, r2
    movifnidn            hd, hm
    jmp                  wq
.w4:
    mova                 m0, [idxq]
    add                idxq, 16
    pshufb               m1, m3, m0
    pshufb               m2, m4, m0
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    movq   [dstq+strideq*0], m0
    movhps [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    movq   [dstq+strideq*0], m1
    movhps [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 4
    jg .w4
    RET
.w8:
    mova                 m0, [idxq]
    add                idxq, 16
    pshufb               m1, m3, m0
    pshufb               m2, m4, m0
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8
    RET
.w16:
    mova                 m0, [idxq]
    add                idxq, 16
    pshufb               m1, m3, m0
    pshufb               m2, m4, m0
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    mova        [dstq+16*0], m0
    mova        [dstq+16*1], m1
    add                dstq, strideq
    dec                  hd
    jg .w16
    RET
.w32:
    mova                 m0, [idxq+16*0]
    pshufb               m1, m3, m0
    pshufb               m2, m4, m0
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    mova                 m2, [idxq+16*1]
    add                idxq, 16*2
    mova        [dstq+16*0], m0
    pshufb               m0, m3, m2
    mova        [dstq+16*1], m1
    pshufb               m1, m4, m2
    punpcklbw            m2, m0, m1
    punpckhbw            m0, m1
    mova        [dstq+16*2], m2
    mova        [dstq+16*3], m0
    add                dstq, strideq
    dec                  hd
    jg .w32
    RET
.w64:
    mova                 m0, [idxq+16*0]
    pshufb               m1, m3, m0
    pshufb               m2, m4, m0
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    mova                 m2, [idxq+16*1]
    mova        [dstq+16*0], m0
    pshufb               m0, m3, m2
    mova        [dstq+16*1], m1
    pshufb               m1, m4, m2
    punpcklbw            m2, m0, m1
    punpckhbw            m0, m1
    mova                 m1, [idxq+16*2]
    mova        [dstq+16*2], m2
    pshufb               m2, m3, m1
    mova        [dstq+16*3], m0
    pshufb               m0, m4, m1
    punpcklbw            m1, m2, m0
    punpckhbw            m2, m0
    mova                 m0, [idxq+16*3]
    add                idxq, 16*4
    mova        [dstq+16*4], m1
    pshufb               m1, m3, m0
    mova        [dstq+16*5], m2
    pshufb               m2, m4, m0
    punpcklbw            m0, m1, m2
    punpckhbw            m1, m2
    mova        [dstq+16*6], m0
    mova        [dstq+16*7], m1
    add                dstq, strideq
    dec                  hd
    jg .w64
    RET
