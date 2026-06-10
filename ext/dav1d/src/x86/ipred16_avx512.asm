; Copyright © 2022-2024, VideoLAN and dav1d authors
; Copyright © 2022-2024, Two Orioles, LLC
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

%if ARCH_X86_64

SECTION_RODATA 64

ipred_shuf:    db 14, 15, 14, 15,  0,  1,  2,  3,  6,  7,  6,  7,  0,  1,  2,  3
               db 10, 11, 10, 11,  8,  9, 10, 11,  2,  3,  2,  3,  8,  9, 10, 11
               db 12, 13, 12, 13,  4,  5,  6,  7,  4,  5,  4,  5,  4,  5,  6,  7
               db  8,  9,  8,  9, 12, 13, 14, 15,  0,  1,  0,  1, 12, 13, 14, 15
smooth_perm:   db  1,  2,  5,  6,  9, 10, 13, 14, 17, 18, 21, 22, 25, 26, 29, 30
               db 33, 34, 37, 38, 41, 42, 45, 46, 49, 50, 53, 54, 57, 58, 61, 62
               db 65, 66, 69, 70, 73, 74, 77, 78, 81, 82, 85, 86, 89, 90, 93, 94
               db 97, 98,101,102,105,106,109,110,113,114,117,118,121,122,125,126
pal_pred_perm: db  0, 16, 32, 48,  1, 17, 33, 49,  2, 18, 34, 50,  3, 19, 35, 51
               db  4, 20, 36, 52,  5, 21, 37, 53,  6, 22, 38, 54,  7, 23, 39, 55
               db  8, 24, 40, 56,  9, 25, 41, 57, 10, 26, 42, 58, 11, 27, 43, 59
               db 12, 28, 44, 60, 13, 29, 45, 61, 14, 30, 46, 62, 15, 31, 47, 63
pw_31to0:      dw 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16
               dw 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0
pw_1to32:      dw  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16
               dw 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
z_upsample:    dw  0, -1,  1,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6
               dw  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13, 12, 14, 13, 15, 14
z_xpos_mul:    dw  1,  1,  1,  1,  2,  2,  1,  1,  3,  3,  2,  2,  4,  4,  2,  2
               dw  5,  5,  3,  3,  6,  6,  3,  3,  7,  7,  4,  4,  8,  8,  4,  4
z_ypos_mul:    dw  0,  0,  0,  0,  1,  1,  0,  0,  2,  2,  1,  1,  3,  3,  1,  1
               dw  4,  4,  2,  2,  5,  5,  2,  2,  6,  6,  3,  3,  7,  7,  3,  3
z_filter_t0:   db 55,127, 39,127, 39,127,  7, 15, 31,  7, 15, 31,  0,  3, 31,  0
z_filter_t1:   db 39, 63, 19, 47, 19, 47,  3,  3,  3,  3,  3,  3,  0,  0,  0,  0
z_xpos_off1a:  dw  30720,  30784,  30848,  30912,  30976,  31040,  31104,  31168
z_xpos_off1b:  dw  30720,  30848,  30976,  31104,  31232,  31360,  31488,  31616
filter_permA:  times 4 db  6,  7,  8,  9, 14, 15,  4,  5
               times 4 db 10, 11, 12, 13,  2,  3, -1, -1
filter_permB:  times 4 db 22, 23, 24, 25, 30, 31,  6,  7
               times 4 db 26, 27, 28, 29, 14, 15, -1, -1
filter_permC:          dd  8 ; dq  8, 10,  1, 11,  0,  9
pw_1:          times 2 dw  1
                       dd 10
filter_rnd:            dd 32
                       dd  1
                       dd  8
                       dd 11
filter_shift:  times 2 dw  6
                       dd  0
               times 2 dw  4
                       dd  9
pd_65536:              dd 65536
pal_unpack:    db  0,  8,  4, 12, 32, 40, 36, 44
               db 16, 24, 20, 28, 48, 56, 52, 60
z_filter_wh:   db  7,  7, 11, 11, 15, 15, 19, 19, 19, 23, 23, 23, 31, 31, 31, 39
               db 39, 39, 47, 47, 47, 79, 79, 79
z_filter_k:    dw  8,  8,  6,  6,  4,  4
               dw  4,  4,  5,  5,  4,  4
               dw  0,  0,  0,  0,  2,  2
pb_90:         times 4 db 90
pw_15:         times 2 dw 15
pw_16:         times 2 dw 16
pw_17:         times 2 dw 17
pw_24:         times 2 dw 24
pw_31:         times 2 dw 31
pw_32:         times 2 dw 32
pw_63:         times 2 dw 63
pw_64:         times 2 dw 64
pw_512:        times 2 dw 512
pw_2048:       times 2 dw 2048
pw_31806:      times 2 dw 31806
pw_32640:      times 2 dw 32640
pw_32672:      times 2 dw 32672
pw_32704:      times 2 dw 32704
pw_32735:      times 2 dw 32735
pw_32736:      times 2 dw 32736

%define pw_2 (z_xpos_mul+4* 2)
%define pw_3 (z_xpos_mul+4* 4)
%define pw_7 (z_xpos_mul+4*12)
%define pw_0to31 (pw_1to32-2)

%macro JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1_%2)
    %%table:
    %rep %0 - 2
        dd %%base %+ .%3 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

JMP_TABLE ipred_paeth_16bpc,      avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_16bpc,     avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_h_16bpc,   avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_v_16bpc,   avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z1_16bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z2_16bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z3_16bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE pal_pred_16bpc,         avx512icl, w4, w8, w16, w32, w64

cextern smooth_weights_1d_16bpc
cextern smooth_weights_2d_16bpc
cextern dr_intra_derivative
cextern filter_intra_taps

SECTION .text

%macro PAETH 3 ; top, signed_ldiff, ldiff
    paddw               m0, m%2, m2
    psubw               m1, m0, m3  ; tldiff
    psubw               m0, m%1     ; tdiff
    pabsw               m1, m1
    pabsw               m0, m0
    pcmpgtw             k1, m0, m1
    pminsw              m0, m1
    pcmpgtw             k2, m%3, m0
    vpblendmw       m0{k1}, m%1, m3
    vpblendmw       m0{k2}, m2, m0
%endmacro

INIT_ZMM avx512icl
cglobal ipred_paeth_16bpc, 3, 7, 10, dst, stride, tl, w, h
%define base r6-ipred_paeth_16bpc_avx512icl_table
    lea                 r6, [ipred_paeth_16bpc_avx512icl_table]
    tzcnt               wd, wm
    movifnidn           hd, hm
    movsxd              wq, [r6+wq*4]
    vpbroadcastw        m3, [tlq]   ; topleft
    add                 wq, r6
    jmp                 wq
.w4:
    vpbroadcastq        m4, [tlq+2] ; top
    movsldup            m7, [base+ipred_shuf]
    lea                 r6, [strideq*3]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w4_loop:
    sub                tlq, 16
    vbroadcasti32x4     m2, [tlq]
    pshufb              m2, m7      ; left
    PAETH                4, 5, 6
    vextracti32x4      xm1, m0, 2
    vextracti32x4      xm8, ym0, 1
    vextracti32x4      xm9, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm8
    movq   [dstq+r6       ], xm9
    sub                 hd, 8
    jl .w4_end
    lea               dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm8
    movhps [dstq+r6       ], xm9
    lea               dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.w8:
    vbroadcasti32x4     m4, [tlq+2]
    movsldup            m7, [base+ipred_shuf]
    lea                 r6, [strideq*3]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w8_loop:
    sub                tlq, 8
    vpbroadcastq        m2, [tlq]
    pshufb              m2, m7
    PAETH                4, 5, 6
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+r6       ], m0, 3
    lea               dstq, [dstq+strideq*4]
    sub                 hd, 4
    jg .w8_loop
    RET
.w16:
    vbroadcasti32x8     m4, [tlq+2]
    movsldup            m7, [base+ipred_shuf]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w16_loop:
    sub                tlq, 4
    vpbroadcastd        m2, [tlq]
    pshufb              m2, m7
    PAETH                4, 5, 6
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea               dstq, [dstq+strideq*2]
    sub                 hd, 2
    jg .w16_loop
    RET
.w32:
    movu                m4, [tlq+2]
    psubw               m5, m4, m3
    pabsw               m6, m5
.w32_loop:
    sub                tlq, 2
    vpbroadcastw        m2, [tlq]
    PAETH                4, 5, 6
    mova            [dstq], m0
    add               dstq, strideq
    dec                 hd
    jg .w32_loop
    RET
.w64:
    movu                m4, [tlq+ 2]
    movu                m7, [tlq+66]
    psubw               m5, m4, m3
    psubw               m8, m7, m3
    pabsw               m6, m5
    pabsw               m9, m8
.w64_loop:
    sub                tlq, 2
    vpbroadcastw        m2, [tlq]
    PAETH                4, 5, 6
    mova       [dstq+64*0], m0
    PAETH                7, 8, 9
    mova       [dstq+64*1], m0
    add               dstq, strideq
    dec                 hd
    jg .w64_loop
    RET

cglobal ipred_smooth_v_16bpc, 3, 7, 7, dst, stride, tl, w, h, weights, stride3
%define base r6-$$
    lea                  r6, [$$]
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [base+ipred_smooth_v_16bpc_avx512icl_table+wq*4]
    lea            weightsq, [base+smooth_weights_1d_16bpc+hq*4]
    neg                  hq
    vpbroadcastw         m6, [tlq+hq*2] ; bottom
    lea                  wq, [base+ipred_smooth_v_16bpc_avx512icl_table+wq]
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vpbroadcastq         m5, [tlq+2]    ; top
    movsldup             m4, [ipred_shuf]
    psubw                m5, m6         ; top - bottom
.w4_loop:
    vbroadcasti32x4      m3, [weightsq+hq*2]
    pshufb               m3, m4
    pmulhrsw             m3, m5
    paddw                m3, m6
    vextracti32x4       xm0, m3, 3
    vextracti32x4       xm1, ym3, 1
    vextracti32x4       xm2, m3, 2
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm2
    movhps [dstq+stride3q ], xm3
    add                  hq, 8
    jg .end
    lea                dstq, [dstq+strideq*4]
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm2
    movq   [dstq+stride3q ], xm3
    lea                dstq, [dstq+strideq*4]
    jl .w4_loop
.end:
    RET
.w8:
    vbroadcasti32x4      m5, [tlq+2]    ; top
    movsldup             m4, [ipred_shuf]
    psubw                m5, m6         ; top - bottom
.w8_loop:
    vpbroadcastq         m0, [weightsq+hq*2]
    pshufb               m0, m4
    pmulhrsw             m0, m5
    paddw                m0, m6
    vextracti32x4 [dstq+strideq*0], m0, 3
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    mova          [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w8_loop
    RET
.w16:
    vbroadcasti32x8      m5, [tlq+2]    ; top
    movsldup             m4, [ipred_shuf]
    psubw                m5, m6         ; top - bottom
.w16_loop:
    vpbroadcastd         m0, [weightsq+hq*2+0]
    vpbroadcastd         m1, [weightsq+hq*2+4]
    pshufb               m0, m4
    pshufb               m1, m4
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    paddw                m0, m6
    paddw                m1, m6
    vextracti32x8 [dstq+strideq*0], m0, 1
    mova          [dstq+strideq*1], ym0
    vextracti32x8 [dstq+strideq*2], m1, 1
    mova          [dstq+stride3q ], ym1
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w16_loop
    RET
.w32:
    movu                 m5, [tlq+2]
    psubw                m5, m6
.w32_loop:
    vpbroadcastw         m0, [weightsq+hq*2+0]
    vpbroadcastw         m1, [weightsq+hq*2+2]
    vpbroadcastw         m2, [weightsq+hq*2+4]
    vpbroadcastw         m3, [weightsq+hq*2+6]
    REPX   {pmulhrsw x, m5}, m0, m1, m2, m3
    REPX   {paddw    x, m6}, m0, m1, m2, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w32_loop
    RET
.w64:
    movu                 m4, [tlq+ 2]
    movu                 m5, [tlq+66]
    psubw                m4, m6
    psubw                m5, m6
.w64_loop:
    vpbroadcastw         m1, [weightsq+hq*2+0]
    vpbroadcastw         m3, [weightsq+hq*2+2]
    pmulhrsw             m0, m4, m1
    pmulhrsw             m1, m5
    pmulhrsw             m2, m4, m3
    pmulhrsw             m3, m5
    REPX      {paddw x, m6}, m0, m1, m2, m3
    mova [dstq+strideq*0+64*0], m0
    mova [dstq+strideq*0+64*1], m1
    mova [dstq+strideq*1+64*0], m2
    mova [dstq+strideq*1+64*1], m3
    lea                dstq, [dstq+strideq*2]
    add                  hq, 2
    jl .w64_loop
    RET

cglobal ipred_smooth_h_16bpc, 3, 7, 7, dst, stride, tl, w, h, stride3
    lea                  r6, [$$]
    mov                  wd, wm
    movifnidn            hd, hm
    vpbroadcastw         m6, [tlq+wq*2] ; right
    tzcnt                wd, wd
    add                  hd, hd
    movsxd               wq, [base+ipred_smooth_h_16bpc_avx512icl_table+wq*4]
    sub                 tlq, hq
    lea            stride3q, [strideq*3]
    lea                  wq, [base+ipred_smooth_h_16bpc_avx512icl_table+wq]
    jmp                  wq
.w4:
    movsldup             m4, [base+ipred_shuf]
    vpbroadcastq         m5, [base+smooth_weights_1d_16bpc+4*2]
.w4_loop:
    vbroadcasti32x4      m0, [tlq+hq-16] ; left
    pshufb               m0, m4
    psubw                m0, m6          ; left - right
    pmulhrsw             m0, m5
    paddw                m0, m6
    vextracti32x4       xm1, m0, 2
    vextracti32x4       xm2, ym0, 1
    vextracti32x4       xm3, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm2
    movq   [dstq+stride3q ], xm3
    sub                  hd, 8*2
    jl .end
    lea                dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm2
    movhps [dstq+stride3q ], xm3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.end:
    RET
.w8:
    movsldup             m4, [base+ipred_shuf]
    vbroadcasti32x4      m5, [base+smooth_weights_1d_16bpc+8*2]
.w8_loop:
    vpbroadcastq         m0, [tlq+hq-8] ; left
    pshufb               m0, m4
    psubw                m0, m6         ; left - right
    pmulhrsw             m0, m5
    paddw                m0, m6
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4*2
    jg .w8_loop
    RET
.w16:
    movsldup             m4, [base+ipred_shuf]
    vbroadcasti32x8      m5, [base+smooth_weights_1d_16bpc+16*2]
.w16_loop:
    vpbroadcastd         m0, [tlq+hq-4]
    vpbroadcastd         m1, [tlq+hq-8]
    pshufb               m0, m4
    pshufb               m1, m4
    psubw                m0, m6
    psubw                m1, m6
    pmulhrsw             m0, m5
    pmulhrsw             m1, m5
    paddw                m0, m6
    paddw                m1, m6
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+stride3q ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hq, 4*2
    jg .w16_loop
    RET
.w32:
    movu                 m5, [base+smooth_weights_1d_16bpc+32*2]
.w32_loop:
    vpbroadcastq         m3, [tlq+hq-8]
    punpcklwd            m3, m3
    psubw                m3, m6
    pshufd               m0, m3, q3333
    pshufd               m1, m3, q2222
    pshufd               m2, m3, q1111
    pshufd               m3, m3, q0000
    REPX   {pmulhrsw x, m5}, m0, m1, m2, m3
    REPX   {paddw    x, m6}, m0, m1, m2, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hq, 4*2
    jg .w32_loop
    RET
.w64:
    movu                 m4, [base+smooth_weights_1d_16bpc+64*2]
    movu                 m5, [base+smooth_weights_1d_16bpc+64*3]
.w64_loop:
    vpbroadcastw         m1, [tlq+hq-2]
    vpbroadcastw         m3, [tlq+hq-4]
    psubw                m1, m6
    psubw                m3, m6
    pmulhrsw             m0, m4, m1
    pmulhrsw             m1, m5
    pmulhrsw             m2, m4, m3
    pmulhrsw             m3, m5
    REPX      {paddw x, m6}, m0, m1, m2, m3
    mova [dstq+strideq*0+64*0], m0
    mova [dstq+strideq*0+64*1], m1
    mova [dstq+strideq*1+64*0], m2
    mova [dstq+strideq*1+64*1], m3
    lea                dstq, [dstq+strideq*2]
    sub                  hq, 2*2
    jg .w64_loop
    RET

cglobal ipred_smooth_16bpc, 3, 7, 16, dst, stride, tl, w, h, v_weights, stride3
    lea                 r6, [$$]
    mov                 wd, wm
    movifnidn           hd, hm
    vpbroadcastw       m13, [tlq+wq*2]   ; right
    tzcnt               wd, wd
    add                 hd, hd
    movsxd              wq, [base+ipred_smooth_16bpc_avx512icl_table+wq*4]
    mov                r5d, 0x55555555
    sub                tlq, hq
    mova               m14, [base+smooth_perm]
    kmovd               k1, r5d
    vpbroadcastw        m0, [tlq]        ; bottom
    mov                 r5, 0x3333333333333333
    pxor               m15, m15
    lea                 wq, [base+ipred_smooth_16bpc_avx512icl_table+wq]
    kmovq               k2, r5
    lea         v_weightsq, [base+smooth_weights_2d_16bpc+hq*2]
    jmp                 wq
.w4:
    vpbroadcastq        m5, [tlq+hq+2]
    movshdup            m3, [base+ipred_shuf]
    movsldup            m4, [base+ipred_shuf]
    vbroadcasti32x4     m6, [base+smooth_weights_2d_16bpc+4*4]
    lea           stride3q, [strideq*3]
    punpcklwd           m5, m0           ; top, bottom
.w4_loop:
    vbroadcasti32x4     m0, [v_weightsq]
    vpbroadcastq        m2, [tlq+hq-8]
    mova                m1, m13
    pshufb              m0, m3
    pmaddwd             m0, m5
    pshufb          m1{k2}, m2, m4       ; left, right
    vpdpwssd            m0, m1, m6
    vpermb              m0, m14, m0
    pavgw              ym0, ym15
    vextracti32x4      xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    lea               dstq, [dstq+strideq*4]
    add         v_weightsq, 4*4
    sub                 hd, 4*2
    jg .w4_loop
    RET
.w8:
    vbroadcasti32x4    ym5, [tlq+hq+2]
    movshdup            m6, [base+ipred_shuf]
    movsldup            m7, [base+ipred_shuf]
    pmovzxwd            m5, ym5
    vbroadcasti32x8     m8, [base+smooth_weights_2d_16bpc+8*4]
    lea           stride3q, [strideq*3]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
.w8_loop:
    vpbroadcastq        m0, [v_weightsq+0]
    vpbroadcastq        m1, [v_weightsq+8]
    vpbroadcastd        m3, [tlq+hq-4]
    vpbroadcastd        m4, [tlq+hq-8]
    pshufb              m0, m6
    pmaddwd             m0, m5
    pshufb              m1, m6
    pmaddwd             m1, m5
    mova                m2, m13
    pshufb          m2{k2}, m3, m7       ; left, right
    mova                m3, m13
    pshufb          m3{k2}, m4, m7
    vpdpwssd            m0, m2, m8
    vpdpwssd            m1, m3, m8
    add         v_weightsq, 4*4
    vpermt2b            m0, m14, m1
    pavgw               m0, m15
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea               dstq, [dstq+strideq*4]
    sub                 hd, 4*2
    jg .w8_loop
    RET
.w16:
    pmovzxwd            m5, [tlq+hq+2]
    mova                m6, [base+smooth_weights_2d_16bpc+16*4]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
.w16_loop:
    vpbroadcastd        m0, [v_weightsq+0]
    vpbroadcastd        m1, [v_weightsq+4]
    pmaddwd             m0, m5
    pmaddwd             m1, m5
    mova                m2, m13
    vpbroadcastw    m2{k1}, [tlq+hq-2] ; left, right
    mova                m3, m13
    vpbroadcastw    m3{k1}, [tlq+hq-4]
    vpdpwssd            m0, m2, m6
    vpdpwssd            m1, m3, m6
    add         v_weightsq, 2*4
    vpermt2b            m0, m14, m1
    pavgw               m0, m15
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea               dstq, [dstq+strideq*2]
    sub                 hq, 2*2
    jg .w16_loop
    RET
.w32:
    pmovzxwd            m5, [tlq+hq+ 2]
    pmovzxwd            m6, [tlq+hq+34]
    mova                m7, [base+smooth_weights_2d_16bpc+32*4]
    mova                m8, [base+smooth_weights_2d_16bpc+32*6]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
    vpblendmw       m6{k1}, m0, m6
.w32_loop:
    vpbroadcastd        m2, [v_weightsq+0]
    vpbroadcastd        m3, [v_weightsq+4]
    pmaddwd             m0, m5, m2
    pmaddwd             m2, m6
    pmaddwd             m1, m5, m3
    pmaddwd             m3, m6
    mova                m4, m13
    vpbroadcastw    m4{k1}, [tlq+hq-2] ; left, right
    vpdpwssd            m0, m4, m7
    vpdpwssd            m2, m4, m8
    mova                m4, m13
    vpbroadcastw    m4{k1}, [tlq+hq-4]
    vpdpwssd            m1, m4, m7
    vpdpwssd            m3, m4, m8
    add         v_weightsq, 2*4
    vpermt2b            m0, m14, m2
    vpermt2b            m1, m14, m3
    pavgw               m0, m15
    pavgw               m1, m15
    mova  [dstq+strideq*0], m0
    mova  [dstq+strideq*1], m1
    lea               dstq, [dstq+strideq*2]
    sub                 hq, 2*2
    jg .w32_loop
    RET
.w64:
    pmovzxwd            m5, [tlq+hq+ 2]
    pmovzxwd            m6, [tlq+hq+34]
    pmovzxwd            m7, [tlq+hq+66]
    pmovzxwd            m8, [tlq+hq+98]
    mova                m9, [base+smooth_weights_2d_16bpc+64*4]
    vpblendmw       m5{k1}, m0, m5       ; top, bottom
    mova               m10, [base+smooth_weights_2d_16bpc+64*5]
    vpblendmw       m6{k1}, m0, m6
    mova               m11, [base+smooth_weights_2d_16bpc+64*6]
    vpblendmw       m7{k1}, m0, m7
    mova               m12, [base+smooth_weights_2d_16bpc+64*7]
    vpblendmw       m8{k1}, m0, m8
.w64_loop:
    vpbroadcastd        m3, [v_weightsq]
    mova                m4, m13
    vpbroadcastw    m4{k1}, [tlq+hq-2] ; left, right
    pmaddwd             m0, m5, m3
    pmaddwd             m2, m6, m3
    pmaddwd             m1, m7, m3
    pmaddwd             m3, m8
    vpdpwssd            m0, m4, m9
    vpdpwssd            m2, m4, m10
    vpdpwssd            m1, m4, m11
    vpdpwssd            m3, m4, m12
    add         v_weightsq, 1*4
    vpermt2b            m0, m14, m2
    vpermt2b            m1, m14, m3
    pavgw               m0, m15
    pavgw               m1, m15
    mova       [dstq+64*0], m0
    mova       [dstq+64*1], m1
    add               dstq, strideq
    sub                 hd, 1*2
    jg .w64_loop
    RET

%if WIN64
    DECLARE_REG_TMP 4
%else
    DECLARE_REG_TMP 8
%endif

cglobal ipred_z1_16bpc, 3, 8, 16, dst, stride, tl, w, h, angle, dx
%define base r7-z_filter_t0
    lea                  r7, [z_filter_t0]
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                  t0, [dr_intra_derivative]
    movsxd               wq, [base+ipred_z1_16bpc_avx512icl_table+wq*4]
    add                 tlq, 2
    mov                 dxd, angled
    and                 dxd, 0x7e
    add              angled, 165 ; ~90
    movzx               dxd, word [t0+dxq]
    lea                  wq, [base+ipred_z1_16bpc_avx512icl_table+wq]
    movifnidn            hd, hm
    xor              angled, 0x4ff ; d = 90 - angle
    vpbroadcastd        m15, [base+pw_31806]
    jmp                  wq
.w4:
    vpbroadcastw         m5, [tlq+14]
    vinserti32x4         m5, [tlq], 0
    cmp              angleb, 40
    jae .w4_no_upsample
    lea                 r3d, [angleq-1024]
    sar                 r3d, 7
    add                 r3d, hd
    jg .w4_no_upsample ; !enable_intra_edge_filter || h > 8 || (h == 8 && is_sm)
    call .upsample_top
    vpbroadcastq         m0, [base+z_xpos_off1b]
    jmp .w4_main2
.w4_no_upsample:
    test             angled, 0x400
    jnz .w4_main ; !enable_intra_edge_filter
    lea                 r3d, [hq+3]
    vpbroadcastb        xm0, r3d
    vpbroadcastb        xm1, angled
    shr              angled, 8 ; is_sm << 1
    vpcmpeqb             k1, xm0, [base+z_filter_wh]
    vpcmpgtb         k1{k1}, xm1, [base+z_filter_t0+angleq*8]
    kmovw               r5d, k1
    test                r5d, r5d
    jz .w4_main
    call .w16_filter
    mov                 r2d, 9
    cmp                  hd, 4
    cmovne              r3d, r2d
    vpbroadcastw         m6, r3d
    pminuw               m6, [base+pw_0to31]
    vpermw               m5, m6, m5
.w4_main:
    vpbroadcastq         m0, [base+z_xpos_off1a]
.w4_main2:
    movsldup             m3, [base+z_xpos_mul]
    vpbroadcastw         m4, dxd
    lea                  r2, [strideq*3]
    pmullw               m3, m4
    vshufi32x4           m6, m5, m5, q3321
    psllw                m4, 3       ; dx*8
    paddsw               m3, m0      ; xpos
    palignr              m6, m5, 2   ; top+1
.w4_loop:
    psrlw                m1, m3, 6   ; base_x
    pand                 m2, m15, m3 ; frac
    vpermw               m0, m1, m5  ; top[base_x]
    vpermw               m1, m1, m6  ; top[base_x+1]
    psllw                m2, 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r2       ], xm1
    sub                  hd, 8
    jl .w4_end
    vextracti32x4       xm1, m0, 2
    paddsw               m3, m4      ; xpos += dx
    lea                dstq, [dstq+strideq*4]
    vextracti32x4       xm0, m0, 3
    movq   [dstq+strideq*0], xm1
    movhps [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm0
    movhps [dstq+r2       ], xm0
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.upsample_top:
    vinserti32x4         m5, [tlq-16], 3
    mova                 m3, [base+z_upsample]
    vpbroadcastd         m4, [base+pd_65536]
    add                 dxd, dxd
    vpermw               m0, m3, m5
    paddw                m3, m4
    vpermw               m1, m3, m5
    paddw                m3, m4
    vpermw               m2, m3, m5
    paddw                m3, m4
    vpermw               m3, m3, m5
    vpbroadcastw         m5, r9m     ; pixel_max
    paddw                m1, m2      ; b+c
    paddw                m0, m3      ; a+d
    psubw                m0, m1, m0
    psraw                m0, 3
    pxor                 m2, m2
    paddw                m0, m1
    pmaxsw               m0, m2
    pavgw                m0, m2
    pminsw               m5, m0
    ret
.w8:
    lea                 r3d, [angleq+216]
    movu                ym5, [tlq]
    mov                 r3b, hb
    movu                m10, [base+pw_0to31]
    cmp                 r3d, 8
    ja .w8_no_upsample ; !enable_intra_edge_filter || is_sm || d >= 40 || h > 8
    lea                 r3d, [hq+7]
    vpbroadcastw         m6, r3d
    add                 r3d, r3d
    pminuw               m6, m10
    vpermw               m5, m6, m5
    call .upsample_top
    vbroadcasti32x4      m0, [base+z_xpos_off1b]
    jmp .w8_main2
.w8_no_upsample:
    lea                 r3d, [hq+7]
    vpbroadcastb        ym0, r3d
    and                 r3d, 7
    or                  r3d, 8 ; imin(h+7, 15)
    vpbroadcastw         m6, r3d
    pminuw               m6, m10
    vpermw               m5, m6, m5
    test             angled, 0x400
    jnz .w8_main
    vpbroadcastb        ym1, angled
    shr              angled, 8
    vpcmpeqb             k1, ym0, [base+z_filter_wh]
    mova                xm0, [base+z_filter_t0+angleq*8]
    vpcmpgtb         k1{k1}, ym1, ym0
    kmovd               r5d, k1
    test                r5d, r5d
    jz .w8_main
    call .w16_filter
    cmp                  hd, r3d
    jl .w8_filter_end
    pminud               m6, m10, [base+pw_17] {1to16}
    add                 r3d, 2
.w8_filter_end:
    vpermw               m5, m6, m5
.w8_main:
    vbroadcasti32x4      m0, [base+z_xpos_off1a]
.w8_main2:
    movshdup             m3, [base+z_xpos_mul]
    vpbroadcastw         m4, dxd
    shl                 r3d, 6
    lea                  r2, [strideq*3]
    pmullw               m3, m4
    vshufi32x4           m6, m5, m5, q3321
    sub                 r3d, dxd
    psllw                m4, 2       ; dx*4
    shl                 dxd, 2
    paddsw               m3, m0      ; xpos
    palignr              m6, m5, 2   ; top+1
.w8_loop:
    psrlw                m1, m3, 6   ; base_x
    pand                 m2, m15, m3 ; frac
    vpermw               m0, m1, m5  ; top[base_x]
    vpermw               m1, m1, m6  ; top[base_x+1]
    psllw                m2, 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+r2       ], m0, 3
    sub                  hd, 4
    jz .w8_end
    paddsw               m3, m4      ; xpos += dx
    lea                dstq, [dstq+strideq*4]
    sub                 r3d, dxd
    jg .w8_loop
    vextracti32x4       xm5, m5, 3
.w8_end_loop:
    mova   [dstq+strideq*0], xm5
    mova   [dstq+strideq*1], xm5
    mova   [dstq+strideq*2], xm5
    mova   [dstq+r2       ], xm5
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_end_loop
.w8_end:
    RET
.w16_filter:
    vpbroadcastw         m1, [tlq-2]
    popcnt              r5d, r5d
    valignq              m3, m6, m5, 2
    vpbroadcastd         m7, [base+z_filter_k+(r5-1)*4+12*0]
    valignq              m1, m5, m1, 6
    vpbroadcastd         m8, [base+z_filter_k+(r5-1)*4+12*1]
    palignr              m2, m3, m5, 2
    vpbroadcastd         m9, [base+z_filter_k+(r5-1)*4+12*2]
    palignr              m0, m5, m1, 14
    pmullw               m7, m5
    palignr              m3, m5, 4
    paddw                m0, m2
    palignr              m5, m1, 12
    pmullw               m0, m8
    paddw                m5, m3
    pmullw               m5, m9
    pxor                 m1, m1
    paddw                m0, m7
    paddw                m5, m0
    psrlw                m5, 3
    pavgw                m5, m1
    ret
.w16:
    lea                 r3d, [hq+15]
    vpbroadcastb        ym0, r3d
    and                 r3d, 15
    or                  r3d, 16 ; imin(h+15, 31)
    vpbroadcastw        m11, r3d
    pminuw              m10, m11, [base+pw_0to31]
    vpbroadcastw         m6, [tlq+r3*2]
    vpermw               m5, m10, [tlq]
    test             angled, 0x400
    jnz .w16_main
    vpbroadcastb        ym1, angled
    shr              angled, 8
    vpcmpeqb             k1, ym0, [base+z_filter_wh]
    mova                xm0, [base+z_filter_t0+angleq*8]
    vpcmpgtb         k1{k1}, ym1, ym0
    kmovd               r5d, k1
    test                r5d, r5d
    jz .w16_main
    call .w16_filter
    cmp                  hd, 16
    jg .w16_filter_h32
    vpermw               m6, m11, m5
    vpermw               m5, m10, m5
    jmp .w16_main
.w16_filter_h32:
    movzx               r3d, word [tlq+62]
    movzx               r2d, word [tlq+60]
    lea                 r2d, [r2+r3*8+4]
    sub                 r2d, r3d
    mov                 r3d, 1
    shr                 r2d, 3
    kmovb                k1, r3d
    movd                xm0, r2d
    or                  r3d, 32
    vmovdqu16        m6{k1}, m0
.w16_main:
    rorx                r2d, dxd, 23
    mov                  r7, rsp
    and                 rsp, ~63
    vpbroadcastw         m3, r2d
    sub                 rsp, 64*2
    mov                 r2d, dxd
    paddw                m4, m3, m3
    mova         [rsp+64*0], m5
    vinserti32x8         m3, ym4, 1
    mova         [rsp+64*1], m6
    shl                 r3d, 6
.w16_loop:
    lea                 r5d, [r2+dxq]
    shr                 r2d, 6
    movu                ym0, [rsp+r2*2]
    movu                ym1, [rsp+r2*2+2]
    lea                 r2d, [r5+dxq]
    shr                 r5d, 6
    vinserti32x8         m0, [rsp+r5*2], 1
    vinserti32x8         m1, [rsp+r5*2+2], 1
    pand                 m2, m15, m3 ; frac << 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    sub                  hd, 2
    jz .w16_end
    paddw                m3, m4
    lea                dstq, [dstq+strideq*2]
    cmp                 r2d, r3d
    jl .w16_loop
    punpckhqdq          ym6, ym6
.w16_end_loop:
    mova   [dstq+strideq*0], ym6
    mova   [dstq+strideq*1], ym6
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_end_loop
.w16_end:
    mov                 rsp, r7
    RET
.w32:
    lea                 r3d, [hq+31]
    movu                 m7, [tlq+64*0]
    and                 r3d, 31
    vpbroadcastw        m11, r3d
    or                  r3d, 32 ; imin(h+31, 63)
    pminuw              m10, m11, [base+pw_0to31]
    vpbroadcastw         m9, [tlq+r3*2]
    vpermw               m8, m10, [tlq+64*1]
    test             angled, 0x400
    jnz .w32_main
    vpbroadcastd         m5, [base+pw_3]
    mov                 r5d, ~1
    movu                 m3, [tlq-2]
    kmovd                k1, r5d
    valignq              m2, m8, m7, 6
    paddw                m7, m3
    vmovdqu16        m3{k1}, [tlq-4]
    valignq              m4, m9, m8, 2
    paddw                m3, m5
    paddw                m7, [tlq+2]
    palignr              m1, m8, m2, 14
    pavgw                m3, [tlq+4]
    palignr              m2, m8, m2, 12
    paddw                m7, m3
    palignr              m3, m4, m8, 2
    psrlw                m7, 2
    palignr              m4, m8, 4
    paddw                m8, m1
    paddw                m2, m5
    paddw                m8, m3
    pavgw                m2, m4
    paddw                m8, m2
    psrlw                m8, 2
    cmp                  hd, 64
    je .w32_filter_h64
    vpermw               m9, m11, m8
    vpermw               m8, m10, m8
    jmp .w32_main
.w32_filter_h64:
    movzx               r3d, word [tlq+126]
    movzx               r2d, word [tlq+124]
    lea                 r2d, [r2+r3*8+4]
    sub                 r2d, r3d
    mov                 r3d, 65
    shr                 r2d, 3
    movd                xm0, r2d
    vpblendmw        m9{k1}, m0, m9
.w32_main:
    rorx                r2d, dxd, 23
    mov                  r7, rsp
    and                 rsp, ~63
    vpbroadcastw         m5, r2d
    sub                 rsp, 64*4
    mov                 r2d, dxd
    mova         [rsp+64*0], m7
    shl                 r3d, 6
    mova         [rsp+64*1], m8
    mova                 m6, m5
    mova         [rsp+64*2], m9
    punpckhqdq           m9, m9
    mova         [rsp+64*3], ym9
.w32_loop:
    lea                 r5d, [r2+dxq]
    shr                 r2d, 6
    movu                 m0, [rsp+r2*2]
    movu                 m2, [rsp+r2*2+2]
    lea                 r2d, [r5+dxq]
    shr                 r5d, 6
    movu                 m1, [rsp+r5*2]
    movu                 m3, [rsp+r5*2+2]
    pand                 m4, m15, m5
    paddw                m5, m6
    psubw                m2, m0
    pmulhrsw             m2, m4
    pand                 m4, m15, m5
    psubw                m3, m1
    pmulhrsw             m3, m4
    paddw                m0, m2
    paddw                m1, m3
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    sub                  hd, 2
    jz .w32_end
    paddw                m5, m6
    lea                dstq, [dstq+strideq*2]
    cmp                 r2d, r3d
    jl .w32_loop
.w32_end_loop:
    mova   [dstq+strideq*0], m9
    mova   [dstq+strideq*1], m9
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_end_loop
.w32_end:
    mov                 rsp, r7
    RET
.w64_filter96:
    vpbroadcastd         m4, [base+pw_3]
    mov                 r5d, ~1
    movu                 m0, [tlq-2]
    kmovd                k1, r5d
    paddw                m7, m0
    vmovdqu16        m0{k1}, [tlq-4]
    paddw                m0, m4
    paddw                m7, [tlq+2]
    pavgw                m0, [tlq+4]
    valignq              m1, m9, m8, 6
    paddw                m8, [tlq+62]
    paddw                m2, m4, [tlq+60]
    valignq              m3, m10, m9, 2
    paddw                m8, [tlq+66]
    pavgw                m2, [tlq+68]
    paddw                m7, m0
    palignr              m0, m9, m1, 14
    paddw                m8, m2
    palignr              m1, m9, m1, 12
    psrlw                m7, 2
    palignr              m2, m3, m9, 2
    psrlw                m8, 2
    palignr              m3, m9, 4
    paddw                m0, m9
    paddw                m1, m4
    paddw                m0, m2
    pavgw                m1, m3
    paddw                m0, m1
    ret
.w64:
    movu                 m7, [tlq+64*0]
    lea                 r3d, [hq-1]
    movu                 m8, [tlq+64*1]
    vpbroadcastw        m11, [tlq+r3*2+128]
    movu                 m9, [tlq+64*2]
    cmp                  hd, 64
    je .w64_h64
    vpbroadcastw        m13, r3d
    or                  r3d, 64
    pminuw              m12, m13, [base+pw_0to31]
    mova                m10, m11
    vpermw               m9, m12, m9
    test             angled, 0x400
    jnz .w64_main
    call .w64_filter96
    psrlw                m0, 2
    vpermw               m9, m12, m0
    vpermw              m10, m13, m0
    mova                m11, m10
    jmp .w64_main
.w64_h64:
    movu                m10, [tlq+64*3]
    or                  r3d, 64
    test             angled, 0x400
    jnz .w64_main
    call .w64_filter96
    valignq              m1, m10, m9, 6
    valignq              m3, m11, m10, 2
    vpbroadcastd        m11, [base+pw_63]
    psrlw                m9, m0, 2
    palignr              m0, m10, m1, 14
    palignr              m1, m10, m1, 12
    palignr              m2, m3, m10, 2
    palignr              m3, m10, 4
    paddw               m10, m0
    paddw                m1, m4
    paddw               m10, m2
    pavgw                m1, m3
    paddw               m10, m1
    psrlw               m10, 2
    vpermw              m11, m11, m10
.w64_main:
    rorx                r2d, dxd, 23
    mov                  r7, rsp
    and                 rsp, ~63
    vpbroadcastw         m5, r2d
    sub                 rsp, 64*6
    mova         [rsp+64*0], m7
    mov                 r2d, dxd
    mova         [rsp+64*1], m8
    lea                  r5, [rsp+r3*2]
    mova         [rsp+64*2], m9
    shl                 r3d, 6
    mova         [rsp+64*3], m10
    sub                  r2, r3
    mova         [rsp+64*4], m11
    mova                 m6, m5
    mova         [rsp+64*5], m11
.w64_loop:
    mov                  r3, r2
    sar                  r3, 6
    movu                 m0, [r5+r3*2+64*0]
    movu                 m2, [r5+r3*2+64*0+2]
    movu                 m1, [r5+r3*2+64*1]
    movu                 m3, [r5+r3*2+64*1+2]
    pand                 m4, m15, m5
    psubw                m2, m0
    pmulhrsw             m2, m4
    psubw                m3, m1
    pmulhrsw             m3, m4
    paddw                m0, m2
    paddw                m1, m3
    mova        [dstq+64*0], m0
    mova        [dstq+64*1], m1
    dec                  hd
    jz .w64_end
    paddw                m5, m6
    add                dstq, strideq
    add                  r2, dxq
    jl .w64_loop
.w64_end_loop:
    mova        [dstq+64*0], m11
    mova        [dstq+64*1], m11
    add                dstq, strideq
    dec                  hd
    jg .w64_end_loop
.w64_end:
    mov                 rsp, r7
    RET

cglobal ipred_z2_16bpc, 3, 9, 16, dst, stride, tl, w, h, angle, dx, _, dy
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                 dxq, [dr_intra_derivative-90]
    movzx               dyd, angleb
    xor              angled, 0x400
    mov                  r7, dxq
    sub                 dxq, dyq
    movifnidn            hd, hm
    and                 dyd, ~1
    vpbroadcastw        m12, [tlq]
    and                 dxq, ~1
    movzx               dyd, word [r7+dyq]  ; angle - 90
    lea                  r7, [z_filter_t0]
    movzx               dxd, word [dxq+270] ; 180 - angle
    mova                 m0, [base+pw_31to0]
    movsxd               wq, [base+ipred_z2_16bpc_avx512icl_table+wq*4]
    movu                 m4, [tlq+2]
    neg                 dyd
    vpermw               m7, m0, [tlq-64*1]
    lea                  wq, [base+ipred_z2_16bpc_avx512icl_table+wq]
    vpbroadcastd        m14, [base+pw_31806]
    vpbroadcastd        m15, [base+pw_1]
    jmp                  wq
.w4:
    movq                xm3, [tlq]
    vpbroadcastq         m8, [base+pw_1to32]
    test             angled, 0x400
    jnz .w4_main ; !enable_intra_edge_filter
    lea                 r3d, [hq+2]
    add              angled, 1022
    shl                 r3d, 6
    test                r3d, angled
    jnz .w4_no_upsample_above ; angle >= 130 || h > 8 || (is_sm && h == 8)
    pshuflw             xm0, xm4, q3321
    sub              angled, 1075 ; angle - 53
    lea                 r3d, [hq+3]
    call .upsample_above
    punpcklwd           xm4, xm3, xm4
    palignr             xm3, xm4, xm12, 14
    jmp .w4_main
.w4_upsample_left:
    call .upsample_left
    movsldup             m1, [base+z_xpos_mul]
    paddw                m1, m1
    jmp .w4_main2
.w4_no_upsample_above:
    lea                 r3d, [hq+3]
    vpbroadcastd        ym0, [base+pw_3]
    sub              angled, 1112 ; angle - 90
    call .filter_above2
    lea                 r3d, [hq+2]
    add              angled, 973 ; angle + 883
    palignr             xm3, xm4, xm12, 14
    shl                 r3d, 6
    test                r3d, angled
    jz .w4_upsample_left ; angle <= 140 || h > 8 || (is_sm && h == 8)
    call .filter_left16
.w4_main:
    movsldup             m1, [base+z_xpos_mul]
    psllw               m15, 3
.w4_main2:
    vpbroadcastq         m0, [base+pw_1to32]
    vpbroadcastw        m11, dxd
    movsldup             m2, [base+z_xpos_mul]
    vpbroadcastw        m13, dyd
    vpbroadcastd         m5, [tlq-2]
    psllw               m10, m8, 6
    valignq              m5, m7, m5, 6
    pmullw               m2, m11
    psubw               m10, m2       ; xpos
    pmullw              m13, m0       ; ypos
    palignr              m5, m7, m5, 14
    psrlw               m12, m13, 6
    psllw               m13, 9
    paddw               m12, m1       ; base_y
    pand                m13, m14      ; frac_y << 9
    psllw               m11, 3
    lea                  r5, [strideq*3]
.w4_loop:
    psrlw                m1, m10, 6   ; base_x
    pand                 m2, m14, m10 ; frac
    vpermw               m0, m1, m3   ; top[base_x]
    vpermw               m1, m1, m4   ; top[base_x+1]
    vpmovw2m             k1, m10      ; base_x < 0
    psllw                m2, 9
    vpermw           m0{k1}, m12, m5  ; left[base_y]
    vpermw           m1{k1}, m12, m7  ; left[base_y+1]
    vmovdqu16        m2{k1}, m13
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r5       ], xm1
    sub                  hd, 8
    jl .w4_end
    vextracti32x8       ym0, m0, 1
    psubw               m10, m11      ; base_x -= dx
    lea                dstq, [dstq+strideq*4]
    paddw               m12, m15      ; base_y++
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r5       ], xm1
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.upsample_above: ; w4/w8
    mova                ym9, [base+pw_1to32]
    palignr             xm1, xm4, xm12, 12
    paddw               xm3, xm4  ; b+c
    xor              angled, 0x7f ; 180 - angle
    paddw               xm0, xm1  ; a+d
    vpbroadcastw        xm1, r9m  ; pixel_max
    vpbroadcastb       xm11, r3d
    psubw               xm0, xm3, xm0
    vpbroadcastb        xm2, angled
    psraw               xm0, 3
    shr              angled, 8
    paddw               xm3, xm0
    pxor                xm0, xm0
    vpcmpeqb             k2, xm11, [base+z_filter_wh]
    pmaxsw              xm3, xm0
    add                 dxd, dxd
    pavgw               xm3, xm0
    vpcmpgtb         k2{k2}, xm2, [base+z_filter_t0+angleq*8]
    pminsw              xm3, xm1
    paddw                m8, m8
    jmp .filter_left16b
.upsample_left: ; h4/h8
    lea                 r3d, [hq-1]
    palignr             xm2, xm7, xm12, 14
    vpbroadcastw        xm0, r3d
    palignr             xm1, xm7, xm12, 12
    pminuw              xm0, xm9
    paddw               xm2, xm7 ; b+c
    vpermw              xm0, xm0, xm7
    add                 dyd, dyd
    paddw               xm0, xm1 ; a+d
    vpbroadcastw        xm1, r9m ; pixel_max
    psubw               xm0, xm2, xm0
    psraw               xm0, 3
    paddw               xm2, xm0
    pxor                xm0, xm0
    pmaxsw              xm2, xm0
    pavgw               xm2, xm0
    pminsw              xm2, xm1
    punpckhwd           xm0, xm2, xm7
    punpcklwd           xm7, xm2, xm7
    vinserti32x4        ym7, xm0, 1
    ret
.filter_above:
    sub              angled, 90
.filter_above2:
    vpbroadcastb        ym1, r3d
    vpbroadcastb       ym10, angled
    mov                 r3d, angled
    shr                 r3d, 8
    vpcmpeqb             k2, ym1, [base+z_filter_wh]
    mova               xm11, [base+z_filter_t0+r3*8]
    vpcmpgtb         k1{k2}, ym10, ym11
    mova                 m9, [base+pw_1to32]
    kmovd               r3d, k1
    test                r3d, r3d
    jz .filter_end
    pminuw              ym0, ym9
    popcnt              r3d, r3d
    vpbroadcastd        ym6, r7m      ; max_w
    kxnorw               k1, k1, k1
    vpbroadcastd        ym5, [base+z_filter_k+(r3-1)*4+12*0]
    kaddw                k1, k1, k1   ; ~1
    vpbroadcastd       ym13, [base+z_filter_k+(r3-1)*4+12*1]
    vpermw              ym2, ym0, ym4 ; +1
    pmullw              ym5, ym4
    paddw               ym1, ym2, ym3
    vmovdqu16        m3{k1}, [tlq-2]  ; -2
    vpermw              ym2, ym0, ym2 ; +2
    vpbroadcastd        ym0, [base+z_filter_k+(r3-1)*4+12*2]
    pmullw              ym1, ym13
    movu                m13, [base+pw_0to31]
    paddw               ym2, ym3
    packssdw            ym6, ym6
    pmullw              ym2, ym0
    paddw               ym1, ym5
    vpcmpgtw             k1, ym6, ym13
    paddw               ym1, ym2
    pxor                ym2, ym2
    psrlw               ym1, 3
    pavgw           ym4{k1}, ym1, ym2
.filter_end:
    ret
.filter_left16:
    vpbroadcastd        ym1, [base+pb_90]
    psubb               ym1, ym10
    vpcmpgtb         k2{k2}, ym1, ym11
.filter_left16b:
    kmovd               r3d, k2
    test                r3d, r3d
    jz .filter_end
    lea                 r5d, [hq-1]
    vinserti32x4        ym0, ym12, xm7, 1
    vpbroadcastw        ym1, r5d
    popcnt              r3d, r3d
    vpbroadcastd        ym6, r8m          ; max_h
    pminuw              ym9, ym1
    vpbroadcastd        ym5, [base+z_filter_k+(r3-1)*4+12*0]
    vpermw              ym2, ym9, ym7     ; +1
    vpbroadcastd       ym10, [base+z_filter_k+(r3-1)*4+12*1]
    palignr             ym1, ym7, ym0, 14 ; -1
    pmullw              ym5, ym7
    palignr             ym0, ym7, ym0, 12 ; -2
    paddw               ym1, ym2
    vpermw              ym2, ym9, ym2     ; +2
    vpbroadcastd        ym9, [base+z_filter_k+(r3-1)*4+12*2]
    pmullw              ym1, ym10
    paddw               ym2, ym0
    packssdw            ym6, ym6
    pmullw              ym2, ym9
    paddw               ym1, ym5
    vpcmpgtw             k1, ym6, [base+pw_0to31]
    paddw               ym1, ym2
    pxor                ym2, ym2
    psrlw               ym1, 3
    pavgw           ym7{k1}, ym1, ym2
    ret
.filter_left:
    cmp                  hd, 32
    jl .filter_left16
    vpbroadcastd         m5, [base+pw_3]
    pminud               m0, m9, [base+pw_31] {1to16}
.filter_left32:
    vpbroadcastd         m6, r8m         ; max_h
    valignq              m2, m7, m12, 6
    packssdw             m6, m6
    palignr              m1, m7, m2, 14  ; -1
    paddw                m1, m7
    palignr              m2, m7, m2, 12  ; -2
    vpcmpgtw             k1, m6, m13
    paddw                m2, m5
    cmp                  hd, 64
    je .filter_left64
    lea                 r3d, [hq-1]
    vpbroadcastw        m10, r3d
    pminuw               m0, m10
    vpermw              m10, m0, m7      ; +1
    paddw                m1, m10
    vpermw              m10, m0, m10     ; +2
    pavgw                m2, m10
    paddw                m1, m2
    vpsrlw           m7{k1}, m1, 2
    ret
.filter_left64:
    valignq             m10, m8, m7, 2
    vpaddd              m13, [base+pw_32] {1to16}
    palignr             m11, m10, m7, 2  ; +1
    paddw                m1, m11
    palignr             m11, m10, m7, 4  ; +2
    valignq             m10, m8, m7, 6
    pavgw               m11, m2
    vpermw               m2, m0, m8      ; 32+1
    paddw                m1, m11
    vpsrlw           m7{k1}, m1, 2
    palignr              m1, m8, m10, 14 ; 32-1
    paddw                m1, m8
    palignr             m10, m8, m10, 12 ; 32-2
    paddw                m1, m2
    vpermw               m2, m0, m2      ; 32+2
    paddw               m10, m5
    vpcmpgtw             k1, m6, m13
    pavgw                m2, m10
    paddw                m1, m2
    vpsrlw           m8{k1}, m1, 2
    ret
.w8:
    mova                xm3, [tlq]
    vbroadcasti32x4      m8, [base+pw_1to32]
    test             angled, 0x400
    jnz .w8_main
    lea                 r3d, [angleq+126]
    mov                 r3b, hb
    cmp                 r3d, 8
    ja .w8_no_upsample_above ; angle >= 130 || h > 8 || is_sm
    psrldq              xm0, xm4, 2
    sub              angled, 53
    pshufhw             xm0, xm0, q2210
    lea                 r3d, [hq+7]
    call .upsample_above
    punpcklwd           xm0, xm3, xm4
    punpckhwd           xm4, xm3, xm4
    vinserti32x4        ym3, ym12, xm0, 1
    vinserti32x4        ym4, ym0, xm4, 1
    palignr             ym3, ym4, ym3, 14
    jmp .w8_main
.w8_upsample_left:
    call .upsample_left
    movshdup             m1, [base+z_xpos_mul]
    psllw               m15, 3
    paddw                m1, m1
    jmp .w8_main2
.w8_no_upsample_above:
    lea                 r3d, [hq+7]
    vpbroadcastd        ym0, [base+pw_7]
    call .filter_above
    lea                 r3d, [angleq-51]
    mov                 r3b, hb
    palignr             xm3, xm4, xm12, 14
    cmp                 r3d, 8
    jbe .w8_upsample_left ; angle > 140 && h <= 8 && !is_sm
    call .filter_left
.w8_main:
    movshdup             m1, [base+z_xpos_mul]
    psllw               m15, 2
.w8_main2:
    vbroadcasti32x4      m0, [base+pw_1to32]
    vpbroadcastw        m11, dxd
    movshdup             m2, [base+z_xpos_mul]
    vpbroadcastw        m13, dyd
    psllw               m10, m8, 6
    valignq              m5, m7, m12, 6
    pmullw               m2, m11
    psubw               m10, m2       ; xpos
    pmullw              m13, m0       ; ypos
    palignr              m5, m7, m5, 14
    psrlw               m12, m13, 6
    psllw               m13, 9
    mov                 r2d, 1<<6
    paddw               m12, m1       ; base_y
    lea                 r3d, [dxq-(8<<6)] ; left-only threshold
    pand                m13, m14      ; frac_y << 9
    shl                 dxd, 2
    psllw               m11, 2
    lea                  r5, [strideq*3]
.w8_loop:
    psrlw                m1, m10, 6
    pand                 m2, m14, m10
    vpermw               m0, m1, m3
    vpermw               m1, m1, m4
    psllw                m2, 9
    sub                 r2d, dxd
    jge .w8_toponly
    vpmovw2m             k1, m10
    vpermw           m0{k1}, m12, m5
    vpermw           m1{k1}, m12, m7
    vmovdqu16        m2{k1}, m13
.w8_toponly:
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw                m0, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+r5       ], m0, 3
    sub                  hd, 4
    jz .w8_end
    psubw               m10, m11      ; base_x -= dx
    lea                dstq, [dstq+strideq*4]
    paddw               m12, m15      ; base_y++
    cmp                 r2d, r3d
    jge .w8_loop
.w8_leftonly_loop:
    vpermw               m0, m12, m5
    vpermw               m1, m12, m7
    psubw                m1, m0
    pmulhrsw             m1, m13
    paddw               m12, m15
    paddw                m0, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+r5       ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_leftonly_loop
.w8_end:
    RET
.w16:
    mova                ym3, [tlq]
    vpermw               m8, m0, [tlq-64*2]
    test             angled, 0x400
    jnz .w16_main
    lea                 r3d, [hq+15]
    vpbroadcastd        ym0, [base+pw_15]
    call .filter_above
    call .filter_left
    vinserti32x4        ym3, ym12, xm4, 1
    palignr             ym3, ym4, ym3, 14
.w16_main:
    vbroadcasti32x8      m0, [base+pw_1to32]
    vpbroadcastw        m11, dxd
    vpbroadcastw        m13, dyd
    kxnorw               k2, k2, k2
    psllw               m10, m0, 6
    valignq              m5, m7, m12, 6
    psubw               m10, m11      ; xpos
    valignq              m6, m8, m7, 6
    pmullw              m13, m0       ; ypos
    knotd                k1, k2
    palignr              m5, m7, m5, 14
    palignr              m6, m8, m6, 14
    vpsubw          m10{k1}, m11
    psrlw               m12, m13, 6
    psllw               m13, 9
    mov                 r2d, 1<<6
    vpsubw          m12{k2}, m15      ; base_y
    pand                m13, m14      ; frac_y << 9
    lea                 r3d, [dxq-(16<<6)]
    paddw               m11, m11
    add                 dxd, dxd
    paddw               m15, m15
.w16_loop:
    psrlw                m1, m10, 6
    pand                 m2, m14, m10
    vpermw               m0, m1, m3
    vpermw               m1, m1, m4
    psllw                m2, 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw               m12, m15      ; base_y++
    paddw                m0, m1
    sub                 r2d, dxd
    jge .w16_toponly
    mova                 m1, m5
    vpermt2w             m1, m12, m6
    mova                 m2, m7
    vpermt2w             m2, m12, m8
    vpmovw2m             k1, m10
    psubw                m2, m1
    pmulhrsw             m2, m13
    vpaddw           m0{k1}, m1, m2
.w16_toponly:
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    sub                  hd, 2
    jz .w16_end
    psubw               m10, m11      ; base_x -= dx
    lea                dstq, [dstq+strideq*2]
    cmp                 r2d, r3d
    jge .w16_loop
    paddw               m12, m15
    vpermt2w             m5, m12, m6
    mova                 m1, m7
    vpermt2w             m1, m12, m8
    jmp .w16_leftonly_loop_start
.w16_leftonly_loop:
    mova                 m1, m7
    vpermt2w             m1, m12, m8
    vshufi32x4           m5, m1, q1032
.w16_leftonly_loop_start:
    psubw                m0, m1, m5
    pmulhrsw             m0, m13
    paddw               m12, m15
    paddw                m0, m5
    mova                 m5, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_leftonly_loop
.w16_end:
    RET
.w32:
    mova                 m3, [tlq]
    vpermw               m8, m0, [tlq-64*2]
    mova                 m9, [base+pw_1to32]
    test             angled, 0x400
    jnz .w32_main
    pminud               m0, m9, [base+pw_31] {1to16}
    mov                 r3d, ~1
    kmovd                k1, r3d
    vpbroadcastd         m5, [base+pw_3]
    vpbroadcastd         m6, r6m     ; max_w
    vpermw               m2, m0, m4  ; +1
    movu                m13, [base+pw_0to31]
    paddw                m1, m4, m3
    vmovdqu16        m3{k1}, [tlq-2] ; -2
    packssdw             m6, m6
    paddw                m1, m2
    vpermw               m2, m0, m2  ; +2
    paddw                m3, m5
    vpcmpgtw             k1, m6, m13
    pavgw                m2, m3
    paddw                m1, m2
    psrlw            m4{k1}, m1, 2
    call .filter_left32
.w32_main:
    sub                 rsp, 64*2
    call .w32_main1
    add                 rsp, 64*2
    RET
.w32_main1:
    vpbroadcastw        m11, dxd
    movu           [rsp+64], m4
    vpbroadcastw         m4, dyd
    movd           [rsp+60], xm12
    valignq              m5, m7, m12, 6
    psllw                m3, m9, 6    ; xpos
    valignq              m6, m8, m7, 6
    pmullw               m9, m4       ; ypos
    palignr              m5, m7, m5, 14
    mov                 r2d, 33<<6
    palignr              m6, m8, m6, 14
    mova                m10, m3
.w32_main2:
    psllw               m13, m9, 9
    sub                 r2d, dxd
    psrlw               m12, m9, 6    ; base_y
    mov                 r8d, hd
    pand                m13, m14      ; frac_y << 9
.w32_loop:
    mov                 r3d, r2d
    shr                 r3d, 6
    psubw               m10, m11      ; base_x -= dx
    movu                 m0, [rsp+r3*2-2]
    pand                 m2, m10, m14 ; frac_x
    movu                 m1, [rsp+r3*2]
    psllw                m2, 9
    psubw                m1, m0
    pmulhrsw             m1, m2
    paddw               m12, m15      ; base_y++
    paddw                m0, m1
    cmp                 r2d, 32<<6
    jge .w32_toponly
    mova                 m1, m5
    vpermt2w             m1, m12, m6
    mova                 m2, m7
    vpermt2w             m2, m12, m8
    vpmovw2m             k1, m10
    psubw                m2, m1
    pmulhrsw             m2, m13
    vpaddw           m0{k1}, m1, m2
.w32_toponly:
    mova             [dstq], m0
    dec                 r8d
    jz .w32_end
    add                dstq, strideq
    sub                 r2d, dxd
    jge .w32_loop
    paddw               m12, m15
    mova                 m2, m5
    vpermt2w             m2, m12, m6
.w32_leftonly_loop:
    mova                 m1, m7
    vpermt2w             m1, m12, m8
    psubw                m0, m1, m2
    pmulhrsw             m0, m13
    paddw               m12, m15
    paddw                m0, m2
    mova                 m2, m1
    mova             [dstq], m0
    add                dstq, strideq
    dec                 r8d
    jg .w32_leftonly_loop
.w32_end:
    ret
.w64:
    movu                 m3, [tlq+66]
    vpermw               m8, m0, [tlq-64*2]
    mova                 m9, [base+pw_1to32]
    test             angled, 0x400
    jnz .w64_main
    mova                 m2, [tlq]        ; -1
    mov                 r3d, ~1
    vpbroadcastd         m5, [base+pw_3]
    kmovd                k1, r3d
    movu                m13, [base+pw_0to31]
    vpbroadcastd         m6, r6m          ; max_w
    pminud               m0, m9, [base+pw_31] {1to16}
    paddw                m1, m4, m2
    vmovdqu16        m2{k1}, [tlq-2]      ; -2
    packssdw             m6, m6
    paddw                m1, [tlq+4]      ; +1
    paddw                m2, m5
    vpcmpgtw             k1, m6, m13
    pavgw                m2, [tlq+6]      ; +2
    paddw                m1, m2
    vpermw               m2, m0, m3       ; 32+1
    psrlw            m4{k1}, m1, 2
    paddw                m1, m3, [tlq+64] ; 32-1
    vpaddd              m11, m13, [base+pw_32] {1to16}
    paddw                m1, m2
    vpermw               m2, m0, m2       ; 32+2
    paddw               m10, m5, [tlq+62] ; 32-2
    vpcmpgtw             k1, m6, m11
    pavgw                m2, m10
    paddw                m1, m2
    psrlw            m3{k1}, m1, 2
    call .filter_left32
.w64_main:
    sub                 rsp, 64*3
    movu [rsp+64*2-gprsize], m3
    mov                  r5, dstq
    call .w32_main1
    psllw                m4, 5
    mov                 r2d, 65<<6
    vpaddd              m10, m3, [base+pw_2048] {1to16} ; xpos
    lea                dstq, [r5+64]
    paddw                m9, m4 ; ypos
    call .w32_main2
    add                 rsp, 64*3
    RET

cglobal ipred_z3_16bpc, 3, 8, 16, dst, stride, tl, w, h, angle, dy
    lea                  r7, [z_filter_t0]
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                  t0, [dr_intra_derivative+45*2-1]
    movsxd               wq, [base+ipred_z3_16bpc_avx512icl_table+wq*4]
    sub              angled, 180
    mov                 dyd, angled
    neg                 dyd
    xor              angled, 0x400
    or                  dyq, ~0x7e
    mova                 m0, [base+pw_31to0]
    movzx               dyd, word [t0+dyq]
    lea                  wq, [base+ipred_z3_16bpc_avx512icl_table+wq]
    movifnidn            hd, hm
    vpbroadcastd        m14, [base+pw_31806]
    vpbroadcastd        m15, [base+pw_1]
    jmp                  wq
.w4:
    lea                 r3d, [hq+3]
    xor                 r3d, 31 ; 32 - (h + imin(w, h))
    vpbroadcastw         m7, r3d
    pmaxuw               m7, m0
    vpermw               m6, m7, [tlq-64*1]
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w4_main
    cmp              angleb, 40
    jae .w4_filter
    lea                 r3d, [angleq-1024]
    sar                 r3d, 7
    add                 r3d, hd
    jg .w4_filter ; h > 8 || (h == 8 && is_sm)
    call .upsample
    movsldup             m1, [base+z_ypos_mul]
    paddw                m1, m1
    jmp .w4_main2
.w4_filter:
    lea                 r3d, [hq+3]
    call .filter32
.w4_main:
    movsldup             m1, [base+z_ypos_mul]
.w4_main2:
    vpbroadcastq         m0, [base+pw_1to32]
    vpbroadcastw         m4, dyd
    lea                 r2d, [hq+4]
    shr                 r2d, 3
    pmullw               m4, m0      ; ypos
    vpbroadcastw         m0, r2d
    imul                 r2, strideq ; stride * imax(height / 8, 1)
    pmullw               m1, m0
    lea                  r3, [r2*3]
    paddd                m1, [base+pw_32736] {1to16}
    psrlw                m2, m4, 6
    psllw                m4, 9
    paddsw               m2, m1      ; base+0
    vpandd               m4, m14     ; frac << 9
    vpermw               m3, m2, m6  ; left[base+0]
.w4_loop:
    paddsw               m2, m15     ; base+1
    vpermw               m1, m2, m6  ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddw                m0, m3
    movq        [dstq+r2*0], xm0
    movhps      [dstq+r2*1], xm0
    vextracti32x4       xm3, ym0, 1
    movq        [dstq+r2*2], xm3
    movhps      [dstq+r3  ], xm3
    sub                  hd, 8
    jl .w4_end
    lea                  r5, [dstq+r2*4]
    vextracti32x8       ym0, m0, 1
    mova                 m3, m1
    movq          [r5+r2*0], xm0
    movhps        [r5+r2*1], xm0
    vextracti32x4       xm1, ym0, 1
    movq          [r5+r2*2], xm1
    movhps        [r5+r3  ], xm1
    add                dstq, strideq
    test                 hd, hd
    jnz .w4_loop
.w4_end:
    RET
.upsample:
    vinserti32x4         m6, [tlq-14], 3
    mova                 m3, [base+z_upsample]
    vpbroadcastd         m4, [base+pd_65536]
    add                 dyd, dyd
    vpermw               m0, m3, m6
    paddw                m3, m4
    vpermw               m1, m3, m6
    paddw                m3, m4
    vpermw               m2, m3, m6
    paddw                m3, m4
    vpermw               m3, m3, m6
    vpbroadcastw         m6, r9m     ; pixel_max
    paddw                m1, m2      ; b+c
    paddw                m0, m3      ; a+d
    psubw                m0, m1, m0
    psraw                m0, 3
    pxor                 m2, m2
    paddw                m0, m1
    pmaxsw               m0, m2
    pavgw                m0, m2
    pminsw               m6, m0
    ret
.w8:
    mova                 m6, [tlq-64*1]
    cmp                  hd, 32
    je .w8_h32
    mov                 r3d, 8
    cmp                  hd, 4
    cmove               r3d, hd
    lea                 r3d, [r3+hq-1]
    xor                 r3d, 31 ; 32 - (h + imin(w, h))
    vpbroadcastw         m1, r3d
    vpermw               m7, m1, m6
    pmaxuw               m1, m0
    vpermw               m6, m1, m6
    test             angled, 0x400
    jnz .w8_main
    lea                 r3d, [angleq+216]
    mov                 r3b, hb
    cmp                 r3d, 8
    ja .w8_filter ; is_sm || d >= 40 || h > 8
    call .upsample
    movshdup             m1, [base+z_ypos_mul]
    paddw                m1, m1
    call .w8_main_setup
.w8_upsample_loop:
    vpermw               m3, m2, m6  ; left[base+0]
    paddw                m2, m15     ; base+1
    vpermw               m1, m2, m6  ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddw                m2, m15     ; base+2
    paddw                m0, m3
    mova                 m3, m1
    mova          [dstq+r2*0], xm0
    vextracti32x4 [dstq+r2*1], ym0, 1
    vextracti32x4 [dstq+r2*2], m0, 2
    vextracti32x4 [dstq+r3  ], m0, 3
    add                dstq, strideq
    sub                  hd, 4
    jg .w8_upsample_loop
    RET
.w8_main_setup:
    vbroadcasti32x4      m0, [base+pw_1to32]
    vpbroadcastw         m4, dyd
    rorx                r2d, hd, 2
    pmullw               m4, m0      ; ypos
    vpbroadcastw         m0, r2d
    imul                 r2, strideq ; stride * height / 4
    lea                  r3, [r2*3]
    pmullw               m1, m0      ; 0 1 2 3
    paddd                m1, [base+pw_32704] {1to16}
    psrlw                m2, m4, 6
    psllw                m4, 9
    paddsw               m2, m1      ; base+0
    vpandd               m4, m14     ; frac << 9
    ret
.w8_h32:
    pmaxud               m7, m0, [base+pw_24] {1to16}
    vpermw               m6, m0, m6
    vpermw               m7, m7, [tlq-64*2]
    test             angled, 0x400
    jnz .w8_main
    call .filter64
    vpbroadcastd         m0, [base+pw_7]
    pminuw               m0, [base+pw_0to31]
    vpermw               m7, m0, m7
    jmp .w8_main
.w8_filter:
    lea                 r3d, [hq+7]
    call .filter32
.w8_main:
    movshdup             m1, [base+z_ypos_mul]
    call .w8_main_setup
    mova                 m3, m6
    vpermt2w             m3, m2, m7  ; left[base+0]
.w8_loop:
    paddsw               m2, m15     ; base+1
    mova                 m1, m6
    vpermt2w             m1, m2, m7  ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddw                m0, m3
    mova                 m3, m1
    mova          [dstq+r2*0], xm0
    vextracti32x4 [dstq+r2*1], ym0, 1
    vextracti32x4 [dstq+r2*2], m0, 2
    vextracti32x4 [dstq+r3  ], m0, 3
    add                dstq, strideq
    sub                  hd, 4
    jg .w8_loop
    RET
.filter32:
    vpbroadcastb       ym10, r3d
    vpbroadcastb        ym1, angled
    shr              angled, 8
    vpcmpeqb             k1, ym10, [base+z_filter_wh]
    mova                xm2, [base+z_filter_t0+angleq*8]
    vpcmpgtb         k1{k1}, ym1, ym2
    kmovd               r5d, k1
    test                r5d, r5d
    jz .filter32_end
    vpbroadcastw         m2, [tlq]
    popcnt              r5d, r5d
    vpbroadcastd         m5, [base+z_filter_k+(r5-1)*4+12*0]
    valignq              m2, m6, m2, 6
    vpbroadcastd         m8, [base+z_filter_k+(r5-1)*4+12*1]
    valignq              m4, m7, m6, 2
    vpbroadcastd         m9, [base+z_filter_k+(r5-1)*4+12*2]
    palignr              m1, m6, m2, 14
    pmullw               m5, m6
    palignr              m3, m4, m6, 2
    paddw                m1, m3
    palignr              m2, m6, m2, 12
    pmullw               m1, m8
    palignr              m4, m6, 4
    paddw                m2, m4
    pmullw               m2, m9
    pmovzxbw            m10, ym10
    pxor                 m6, m6
    paddw                m5, m1
    pminuw               m1, m10, [base+pw_0to31]
    paddw                m5, m2
    psrlw                m5, 3
    pavgw                m6, m5
    vpermw               m7, m10, m6
    vpermw               m6, m1, m6
.filter32_end:
    ret
.w16:
    mova                 m6, [tlq-64*1]
    cmp                  hd, 32
    jl .w16_h16
    pmaxud               m8, m0, [base+pw_16] {1to16}
    mova                 m7, [tlq-64*2]
    vpermw               m6, m0, m6
    jg .w16_h64
    vpermw               m7, m8, m7
    test             angled, 0x400
    jnz .w16_main
    call .filter64
    vpbroadcastd         m0, [base+pw_15]
    vinserti32x8         m0, [base+pw_0to31], 0
    vpermw               m7, m0, m7
    jmp .w16_main
.w16_h16:
    lea                 r3d, [hq*2-1]
    xor                 r3d, 31 ; 32 - (h + imin(w, h))
    vpbroadcastw         m1, r3d
    vpermw               m7, m1, m6
    pmaxuw               m1, m0
    vpermw               m6, m1, m6
    test             angled, 0x400
    jnz .w16_main
    lea                 r3d, [hq+15]
    call .filter32
.w16_main:
    vbroadcasti32x8      m0, [base+pw_1to32]
    vpbroadcastw         m4, dyd
    rorx                r2d, hd, 1
    pmullw               m4, m0      ; ypos
    vpbroadcastw        ym1, r2d
    imul                 r2, strideq ; stride * height / 2
    paddd                m1, [base+pw_32704] {1to16}
    lea                  r3, [r2+strideq]
    psrlw                m2, m4, 6
    psllw                m4, 9
    paddsw               m2, m1      ; base+0
    vpandd               m4, m14     ; frac << 9
    mova                 m3, m6
    vpermt2w             m3, m2, m7  ; left[base+0]
.w16_loop:
    paddsw               m1, m2, m15 ; base+1
    paddsw               m2, m1, m15 ; base+2
    vpermi2w             m1, m6, m7  ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddw                m0, m3
    mova                 m3, m6
    vpermt2w             m3, m2, m7  ; left[base+2]
    vextracti32x8 [dstq+strideq*0], m0, 1
    mova          [dstq+r2       ], ym0
    psubw                m0, m3, m1
    pmulhrsw             m0, m4
    paddw                m0, m1
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+r3       ], ym0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 4
    jg .w16_loop
    RET
.w16_h64:
    vpermw               m7, m0, m7
    vpermw               m8, m8, [tlq-64*3]
    test             angled, 0x400
    jnz .w16_h64_main
    valignq             m11, m8, m7, 6
    call .filter64
    vshufi32x4           m2, m8, m8, q3321
    vpbroadcastd         m0, [base+pw_15]
    palignr             ym3, ym8, ym11, 12
    vinserti32x8         m0, [base+pw_0to31], 0
    palignr             ym4, ym8, ym11, 14
    palignr             ym1, ym2, ym8, 4
    paddw               ym3, ym5
    palignr             ym2, ym8, 2
    paddw               ym8, ym4
    pavgw               ym3, ym1
    paddw               ym8, ym2
    paddw               ym8, ym3
    psrlw               ym8, 2
    vpermw               m8, m0, m8
.w16_h64_main:
    vbroadcasti32x8      m0, [base+pw_1to32]
    vpbroadcastw         m4, dyd
    pmullw               m4, m0    ; ypos
    vpbroadcastd        ym1, [base+pw_32]
    paddd                m1, [base+pw_32672] {1to16}
    mov                  r2, strideq
    shl                  r2, 5      ; stride*32
    vpbroadcastd         m9, [base+pw_32735]
    lea                  r3, [r2+strideq]
    psrlw                m2, m4, 6
    psllw                m4, 9
    paddsw               m2, m1     ; base+0
    vpandd               m4, m14    ; frac << 9
    mova                 m3, m7
    vpermt2w             m3, m2, m6
    vpcmpgtw             k1, m2, m9
    vpermw           m3{k1}, m2, m8 ; left[base+0]
.w16_h64_loop:
    paddsw               m2, m15    ; base+1
    mova                 m1, m7
    vpermt2w             m1, m2, m6
    vpcmpgtw             k1, m2, m9
    vpermw           m1{k1}, m2, m8 ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddsw               m2, m15    ; base+2
    paddw                m0, m3
    mova                 m3, m7
    vpermt2w             m3, m2, m6
    vpcmpgtw             k1, m2, m9
    vpermw           m3{k1}, m2, m8 ; left[base+2]
    vextracti32x8 [dstq+strideq*0], m0, 1
    mova          [dstq+r2       ], ym0
    psubw                m0, m3, m1
    pmulhrsw             m0, m4
    paddw                m0, m1
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+r3       ], ym0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 4
    jg .w16_h64_loop
    RET
.filter64:
    vpbroadcastw         m2, [tlq]
    vpbroadcastd         m5, [base+pw_3]
    valignq              m2, m6, m2, 6
    valignq              m4, m7, m6, 2
    valignq             m10, m7, m6, 6
    palignr              m1, m6, m2, 12
    palignr              m2, m6, m2, 14
    palignr              m3, m4, m6, 4
    paddw                m1, m5
    palignr              m4, m6, 2
    paddw                m6, m2
    valignq              m2, m8, m7, 2
    pavgw                m1, m3
    palignr              m3, m7, m10, 12
    paddw                m6, m4
    palignr              m4, m7, m10, 14
    paddw                m6, m1
    palignr              m1, m2, m7, 4
    psrlw                m6, 2
    palignr              m2, m7, 2
    paddw                m3, m5
    paddw                m7, m4
    pavgw                m3, m1
    paddw                m7, m2
    paddw                m7, m3
    psrlw                m7, 2
    ret
.w32:
    mova                 m6, [tlq-64*1]
    cmp                  hd, 32
    jl .w32_h16
    mova                 m8, [tlq-64*2]
    vpermw               m6, m0, m6
    vpermw               m7, m0, m8
    jg .w32_h64
    test             angled, 0x400
    jnz .w32_main
    vpbroadcastw        xm8, xm8
    jmp .w32_filter
.w32_h16:
    lea                 r3d, [hq*2-1]
    xor                 r3d, 31 ; 32 - (h + imin(w, h))
    vpbroadcastw         m1, r3d
    vpermw               m7, m1, m6
    pmaxuw               m1, m0
    vpermw               m6, m1, m6
    test             angled, 0x400
    jnz .w32_main
    vextracti32x4       xm8, m7, 3
.w32_filter:
    call .filter64
.w32_main:
    vpbroadcastw         m4, dyd
    vpbroadcastd         m1, [base+pw_32704]
    pmullw               m4, [base+pw_1to32] ; ypos
    psrlw                m2, m4, 6
    psllw                m4, 9
    paddsw               m2, m1      ; base+0
    vpandd               m4, m14     ; frac << 9
    mova                 m3, m6
    vpermt2w             m3, m2, m7  ; left[base+0]
.w32_loop:
    paddsw               m1, m2, m15 ; base+1
    paddsw               m2, m1, m15 ; base+2
    vpermi2w             m1, m6, m7  ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddw                m0, m3
    mova                 m3, m6
    vpermt2w             m3, m2, m7  ; left[base+2]
    mova   [dstq+strideq*0], m0
    psubw                m0, m3, m1
    pmulhrsw             m0, m4
    paddw                m0, m1
    mova   [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w32_h64:
    mova                 m9, [tlq-64*3]
    vpermw               m8, m0, m9
    test             angled, 0x400
    jnz .w32_h64_main
    vpbroadcastw        xm9, xm9
    call .filter96
.w32_h64_main:
    vpbroadcastw         m4, dyd
    vpbroadcastd         m1, [base+pw_32672]
    pmullw               m4, [base+pw_1to32] ; ypos
    vpbroadcastd         m9, [base+pw_32735]
    psrlw                m2, m4, 6
    psllw                m4, 9
    paddsw               m2, m1     ; base+0
    vpandd               m4, m14    ; frac << 9
    mova                 m3, m7
    vpermt2w             m3, m2, m6
    vpcmpgtw             k1, m2, m9
    vpermw           m3{k1}, m2, m8 ; left[base+0]
.w32_h64_loop:
    paddsw               m2, m15    ; base+1
    mova                 m1, m7
    vpermt2w             m1, m2, m6
    vpcmpgtw             k1, m2, m9
    vpermw           m1{k1}, m2, m8 ; left[base+1]
    psubw                m0, m1, m3
    pmulhrsw             m0, m4
    paddsw               m2, m15    ; base+2
    paddw                m0, m3
    mova                 m3, m7
    vpermt2w             m3, m2, m6
    vpcmpgtw             k1, m2, m9
    vpermw           m3{k1}, m2, m8 ; left[base+2]
    mova   [dstq+strideq*0], m0
    psubw                m0, m3, m1
    pmulhrsw             m0, m4
    paddw                m0, m1
    mova   [dstq+strideq*1], m0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_h64_loop
    RET
.filter96:
    valignq             m11, m8, m7, 6
    call .filter64
    valignq              m2, m9, m8, 2
    palignr              m3, m8, m11, 12
    palignr              m4, m8, m11, 14
    palignr              m1, m2, m8, 4
    paddw                m3, m5
    palignr              m2, m8, 2
    paddw                m8, m4
    pavgw                m3, m1
    paddw                m8, m2
    paddw                m8, m3
    psrlw                m8, 2
    ret
.w64:
    mova                 m7, [tlq-64*1]
    vpermw               m6, m0, m7
    cmp                  hd, 32
    jl .w64_h16
    mova                 m8, [tlq-64*2]
    vpermw               m7, m0, m8
    jg .w64_h64
    test             angled, 0x400
    jnz .w64_main
    vpbroadcastw         m8, xm8
    mova                 m9, m8
    call .filter96
    vshufi32x4           m9, m8, m8, q3333
    jmp .w64_h64_main
.w64_h16:
    vpbroadcastw         m7, xm7
    test             angled, 0x400
    jnz .w64_main
    mova                 m8, m7
    call .filter64
.w64_main:
    vpbroadcastw        m11, dyd
    vpbroadcastd         m1, [base+pw_32704]
    pmullw              m10, m11, [base+pw_1to32] ; ypos
    psllw               m11, 5
    psrlw                m8, m10, 6
    paddw               m11, m10
    psllw               m10, 9
    psrlw                m9, m11, 6
    psllw               m11, 9
    psubw                m9, m8
    paddsw               m8, m1     ; base+0
    vpandd              m10, m14    ; frac << 9
    vpandd              m11, m14    ; frac << 9
    mova                 m4, m6
    vpermt2w             m4, m8, m7 ; left[base+0] ( 0..31)
    paddsw               m5, m8, m9
    vpermi2w             m5, m6, m7 ; left[base+0] (32..63)
.w64_loop:
    paddsw               m8, m15    ; base+1      ( 0..31)
    mova                 m2, m6
    vpermt2w             m2, m8, m7 ; left[base+1] ( 0..31)
    paddsw               m3, m8, m9 ; base+1      (32..63)
    vpermi2w             m3, m6, m7 ; left[base+1] (32..63)
    psubw                m0, m2, m4
    psubw                m1, m3, m5
    pmulhrsw             m0, m10
    pmulhrsw             m1, m11
    paddw                m0, m4
    paddw                m1, m5
    mova                 m4, m2
    mova        [dstq+64*0], m0
    mova                 m5, m3
    mova        [dstq+64*1], m1
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET
.w64_h64:
    vpermw               m8, m0, [tlq-64*3]
    mova                m13, [tlq-64*4]
    vpermw               m9, m0, m13
    test             angled, 0x400
    jnz .w64_h64_main
    valignq             m12, m9, m8, 6
    call .filter96
    vpbroadcastw        xm2, xm13
    valignq              m2, m9, 2
    palignr              m3, m9, m12, 12
    palignr              m4, m9, m12, 14
    palignr              m1, m2, m9, 4
    paddw                m3, m5
    palignr              m2, m9, 2
    paddw                m9, m4
    pavgw                m3, m1
    paddw                m9, m2
    paddw                m9, m3
    psrlw                m9, 2
.w64_h64_main:
    vpbroadcastw        m11, dyd
    vpbroadcastd         m1, [base+pw_32640]
    pmullw              m10, m11, [base+pw_1to32] ; ypos
    psllw               m11, 5
    psrlw               m12, m10, 6
    paddw               m11, m10
    psllw               m10, 9
    psrlw               m13, m11, 6
    psllw               m11, 9
    psubw               m13, m12
    paddsw              m12, m1     ; base+0
    vpandd              m10, m14    ; frac << 9
    vpandd              m11, m14    ; frac << 9
    vpbroadcastd        m14, [base+pw_64]
    mova                 m4, m6
    vpermt2w             m4, m12, m7
    vptestmw             k1, m12, m14
    mova                 m0, m8
    vpermt2w             m0, m12, m9
    paddsw               m1, m12, m13
    mova                 m5, m6
    vpermt2w             m5, m1, m7
    vptestmw             k2, m1, m14
    vpermi2w             m1, m8, m9
    vmovdqu16        m4{k1}, m0     ; left[base+0] ( 0..31)
    vmovdqu16        m5{k2}, m1     ; left[base+0] (32..63)
.w64_h64_loop:
    paddsw              m12, m15    ; base+1
    mova                 m2, m6
    vpermt2w             m2, m12, m7
    vptestmw             k1, m12, m14
    mova                 m0, m8
    vpermt2w             m0, m12, m9
    paddsw               m1, m12, m13
    mova                 m3, m6
    vpermt2w             m3, m1, m7
    vptestmw             k2, m1, m14
    vpermi2w             m1, m8, m9
    vmovdqu16        m2{k1}, m0     ; left[base+1] ( 0..31)
    vmovdqu16        m3{k2}, m1     ; left[base+1] (32..63)
    psubw                m0, m2, m4
    psubw                m1, m3, m5
    pmulhrsw             m0, m10
    pmulhrsw             m1, m11
    paddw                m0, m4
    paddw                m1, m5
    mova                 m4, m2
    mova        [dstq+64*0], m0
    mova                 m5, m3
    mova        [dstq+64*1], m1
    add                dstq, strideq
    dec                  hd
    jg .w64_h64_loop
    RET

cglobal pal_pred_16bpc, 4, 7, 7, dst, stride, pal, idx, w, h, stride3
    lea                  r6, [pal_pred_16bpc_avx512icl_table]
    tzcnt                wd, wm
    mova                 m3, [pal_pred_perm]
    movifnidn            hd, hm
    movsxd               wq, [r6+wq*4]
    vpbroadcastq         m4, [pal_unpack+0]
    vpbroadcastq         m5, [pal_unpack+8]
    add                  wq, r6
    vbroadcasti32x4      m6, [palq]
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    pmovzxbd            ym0, [idxq]
    add                idxq, 8
    vpmultishiftqb      ym0, ym4, ym0
    vpermw              ym0, ym0, ym6
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+stride3q ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
.w8:
    pmovzxbd             m0, [idxq]
    add                idxq, 16
    vpmultishiftqb       m0, m4, m0
    vpermw               m0, m0, m6
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8
    RET
.w16:
    movu                ym1, [idxq]
    add                idxq, 32
    vpermb               m1, m3, m1
    vpmultishiftqb       m1, m4, m1
    vpermw               m0, m1, m6
    psrlw                m1, 8
    vpermw               m1, m1, m6
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+stride3q ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16
    RET
.w32:
    vpermb               m2, m3, [idxq]
    add                idxq, 64
    vpmultishiftqb       m1, m4, m2
    vpmultishiftqb       m2, m5, m2
    vpermw               m0, m1, m6
    psrlw                m1, 8
    vpermw               m1, m1, m6
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    vpermw               m0, m2, m6
    psrlw                m2, 8
    vpermw               m1, m2, m6
    mova   [dstq+strideq*2], m0
    mova   [dstq+stride3q ], m1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w32
    RET
.w64:
    vpermb               m2, m3, [idxq]
    add                idxq, 64
    vpmultishiftqb       m1, m4, m2
    vpmultishiftqb       m2, m5, m2
    vpermw               m0, m1, m6
    psrlw                m1, 8
    vpermw               m1, m1, m6
    mova          [dstq+ 0], m0
    mova          [dstq+64], m1
    vpermw               m0, m2, m6
    psrlw                m2, 8
    vpermw               m1, m2, m6
    mova  [dstq+strideq+ 0], m0
    mova  [dstq+strideq+64], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w64
    RET

; The ipred_filter SIMD processes 4x2 blocks in the following order which
; increases parallelism compared to doing things row by row.
;     w4     w8       w16             w32
;     1     1 2     1 2 5 6     1 2 5 6 9 a d e
;     2     2 3     2 3 6 7     2 3 6 7 a b e f
;     3     3 4     3 4 7 8     3 4 7 8 b c f g
;     4     4 5     4 5 8 9     4 5 8 9 c d g h

cglobal ipred_filter_16bpc, 4, 7, 14, dst, stride, tl, w, h, filter, top
%define base r6-$$
    lea                  r6, [$$]
%ifidn filterd, filterm
    movzx           filterd, filterb
%else
    movzx           filterd, byte filterm
%endif
    shl             filterd, 6
    movifnidn            hd, hm
    movu                xm0, [tlq-6]
    pmovsxbw             m7, [base+filter_intra_taps+filterq+32*0]
    pmovsxbw             m8, [base+filter_intra_taps+filterq+32*1]
    mov                 r5d, r8m ; bitdepth_max
    movsldup             m9, [base+filter_permA]
    movshdup            m10, [base+filter_permA]
    shr                 r5d, 11  ; is_12bpc
    jnz .12bpc
    psllw                m7, 2   ; upshift multipliers so that packusdw
    psllw                m8, 2   ; will perform clipping for free
.12bpc:
    vpbroadcastd         m5, [base+filter_rnd+r5*8]
    vpbroadcastd         m6, [base+filter_shift+r5*8]
    sub                  wd, 8
    jl .w4
.w8:
    call .main4
    movsldup            m11, [filter_permB]
    lea                 r5d, [hq*2+2]
    movshdup            m12, [filter_permB]
    lea                topq, [tlq+2]
    mova                m13, [filter_permC]
    sub                  hd, 4
    vinserti32x4        ym0, [topq], 1 ; a0 b0   t0 t1
    sub                 tlq, r5
%if WIN64
    push                 r7
    push                 r8
%endif
    mov                  r7, dstq
    mov                 r8d, hd
.w8_loop:
    movlps              xm4, xm0, [tlq+hq*2]
    call .main8
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jge .w8_loop
    test                 wd, wd
    jz .end
    mov                 r2d, 0x0d
    kmovb                k1, r2d
    lea                  r2, [strideq*3]
.w16:
    movd               xmm0, [r7+strideq*1+12]
    vpblendd           xmm0, [topq+8], 0x0e ; t1 t2
    pinsrw              xm4, xmm0, [r7+strideq*0+14], 2
    call .main8
    add                  r7, 16
    vinserti32x4        ym0, [topq+16], 1   ; a2 b2   t2 t3
    mov                  hd, r8d
    mov                dstq, r7
    add                topq, 16
.w16_loop:
    movd               xmm1, [dstq+strideq*2-4]
    punpcklwd           xm4, xmm1, xmm0
    movd               xmm0, [dstq+r2-4]
    shufps          xm4{k1}, xmm0, xm0, q3210
    call .main8
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jge .w16_loop
    sub                  wd, 8
    jg .w16
.end:
    vpermb               m2, m11, m0
    mova                ym1, ym5
    vpdpwssd             m1, m2, m7
    vpermb               m2, m12, m0
    vpdpwssd             m1, m2, m8
%if WIN64
    pop                  r8
    pop                  r7
%endif
    vextracti32x8       ym2, m1, 1
    paddd               ym1, ym2
    packusdw            ym1, ym1
    vpsrlvw             ym1, ym6
    vpermt2q             m0, m13, m1
    vextracti32x4 [dstq+strideq*0], m0, 2
    vextracti32x4 [dstq+strideq*1], ym0, 1
    RET
.w4_loop:
    movlps              xm0, [tlq-10]
    lea                dstq, [dstq+strideq*2]
    sub                 tlq, 4
.w4:
    call .main4
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    sub                  hd, 2
    jg .w4_loop
    RET
ALIGN function_align
.main4:
    vpermb               m2, m9, m0
    mova                ym1, ym5
    vpdpwssd             m1, m2, m7
    vpermb               m0, m10, m0
    vpdpwssd             m1, m0, m8
    vextracti32x8       ym0, m1, 1
    paddd               ym0, ym1
    vextracti32x4       xm1, ym0, 1
    packusdw            xm0, xm1     ; clip
    vpsrlvw             xm0, xm6
    ret
ALIGN function_align
.main8:
    vpermb               m3, m11, m0
    mova                ym2, ym5
    vpdpwssd             m2, m3, m7
    vpermb               m3, m9, m4
    mova                ym1, ym5
    vpdpwssd             m1, m3, m7
    vpermb               m3, m12, m0
    vpdpwssd             m2, m3, m8
    vpermb               m3, m10, m4
    vpdpwssd             m1, m3, m8
    vextracti32x8       ym4, m2, 1
    vextracti32x8       ym3, m1, 1
    paddd               ym2, ym4
    paddd               ym1, ym3
    packusdw            ym1, ym2     ; clip
    vpsrlvw             ym1, ym6
    vpermt2q             m0, m13, m1 ; c0 d0   b0 b1   a0 a1
    vextracti32x4 [dstq+strideq*0], m0, 2
    vextracti32x4 [dstq+strideq*1], ym0, 1
    ret

%endif
