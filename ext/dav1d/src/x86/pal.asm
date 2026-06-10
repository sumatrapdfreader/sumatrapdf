; Copyright © 2023, VideoLAN and dav1d authors
; Copyright © 2023, Two Orioles, LLC
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

SECTION_RODATA 64

const pb_0to63,  db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
%if ARCH_X86_64
                 db 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
                 db 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47
                 db 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63
%endif
pal_idx_w8_padh: db  0,  1,  2,  3,  3,  3,  3,  3,  8,  9, 10, 11, 11, 11, 11, 11

pb_1_16: times 4 db  1, 16
%if ARCH_X86_64
pb_32:   times 4 db 32
%endif

%macro JMP_TABLE 2-*
    %xdefine %1_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1)
    %%table:
    %rep %0 - 1
        dd %%base %+ .w%2 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

JMP_TABLE pal_idx_finish_ssse3,     4, 8, 16, 32, 64
%if ARCH_X86_64
JMP_TABLE pal_idx_finish_avx2,      4, 8, 16, 32, 64
JMP_TABLE pal_idx_finish_avx512icl, 4, 8, 16, 32, 64
%endif

SECTION .text

INIT_XMM ssse3
cglobal pal_idx_finish, 2, 7, 6, dst, src, bw, bh, w, h
%define base r6-pal_idx_finish_ssse3_table
    LEA                  r6, pal_idx_finish_ssse3_table
    tzcnt               bwd, bwm
    movifnidn           bhd, bhm
    movifnidn            wd, wm
    movifnidn            hd, hm
    movsxd              bwq, [r6+bwq*4]
    movddup              m3, [base+pb_1_16]
    add                 bwq, r6
    sub                 bhd, hd
    jmp                 bwq
.w4:
    mova                 m0, [srcq]
    add                srcq, 16
    pmaddubsw            m0, m3
    packuswb             m0, m0
    movq             [dstq], m0
    add                dstq, 8
    sub                  hd, 4
    jg .w4
    test                bhd, bhd
    jz .w4_end
    pshuflw              m0, m0, q3333
.w4_padv:
    movq             [dstq], m0
    add                dstq, 8
    sub                 bhd, 4
    jg .w4_padv
.w4_end:
    RET
.w8_padh:
    pshufb               m0, m2
    pshufb               m1, m2
    jmp .w8_main
.w8:
    mova                 m2, [base+pal_idx_w8_padh]
.w8_loop:
    mova                 m0, [srcq+16*0]
    mova                 m1, [srcq+16*1]
    cmp                  wd, 8
    jl .w8_padh
.w8_main:
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    add                srcq, 16*2
    packuswb             m0, m1
    movu             [dstq], m0
    add                dstq, 16
    sub                  hd, 4
    jg .w8_loop
    test                bhd, bhd
    jz .w8_end
    pshufd               m0, m0, q3333
.w8_padv:
    movu             [dstq], m0
    add                dstq, 16
    sub                 bhd, 4
    jg .w8_padv
.w8_end:
    RET
.w16_padh:
    pshufb               m0, m4
    pshufb               m1, m4
    jmp .w16_main
.w16:
    cmp                  wd, 16
    je .w16_loop
    call .setup_padh
.w16_loop:
    mova                 m0, [srcq+16*0]
    mova                 m1, [srcq+16*1]
    cmp                  wd, 16
    jl .w16_padh
.w16_main:
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    add                srcq, 16*2
    packuswb             m0, m1
    movu             [dstq], m0
    add                dstq, 16
    sub                  hd, 2
    jg .w16_loop
    test                bhd, bhd
    jz .w16_end
    punpckhqdq           m0, m0
.w16_padv:
    movu        [dstq+16*0], m0
    movu        [dstq+16*1], m0
    add                dstq, 16*2
    sub                 bhd, 4
    jg .w16_padv
.w16_end:
    RET
.w32_padh:
    cmp                  wd, 16
    jg .w32_padh2
    pshufb               m1, m0, m5
    pshufb               m0, m4
    jmp .w32_main
.w32_padh2:
    pshufb               m1, m4
    jmp .w32_main
.w32:
    cmp                  wd, 32
    je .w32_loop
    call .setup_padh
.w32_loop:
    mova                 m0, [srcq+16*0]
    mova                 m1, [srcq+16*1]
    cmp                  wd, 32
    jl .w32_padh
.w32_main:
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    add                srcq, 16*2
    packuswb             m0, m1
    movu             [dstq], m0
    add                dstq, 16
    dec                  hd
    jg .w32_loop
    test                bhd, bhd
    jz .w32_end
.w32_padv:
    movu        [dstq+16*0], m0
    movu        [dstq+16*1], m0
    movu        [dstq+16*2], m0
    movu        [dstq+16*3], m0
    add                dstq, 16*4
    sub                 bhd, 4
    jg .w32_padv
.w32_end:
    RET
.w64_padh:
    cmp                  wd, 16
    jg .w64_padh2
    pshufb               m1, m0, m5
    pshufb               m0, m4
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    packuswb             m0, m1
    packuswb             m1, m1
    jmp .w64_main
.w64_padh2:
    pshufb               m1, m4
    pmaddubsw            m0, m3
    pmaddubsw            m2, m1, m3
    pshufb               m1, m5
    pmaddubsw            m1, m3
    packuswb             m0, m2
    packuswb             m1, m1
    jmp .w64_main
.w64_padh3:
    cmp                  wd, 48
    jg .w64_padh4
    pshufb               m2, m1, m5
    pshufb               m1, m4
    jmp .w64_main2
.w64_padh4:
    pshufb               m2, m4
    jmp .w64_main2
.w64:
    cmp                  wd, 64
    je .w64_loop
    call .setup_padh
.w64_loop:
    mova                 m0, [srcq+16*0]
    mova                 m1, [srcq+16*1]
    cmp                  wd, 32
    jle .w64_padh
    pmaddubsw            m0, m3
    pmaddubsw            m1, m3
    packuswb             m0, m1
    mova                 m1, [srcq+16*2]
    mova                 m2, [srcq+16*3]
    cmp                  wd, 64
    jl .w64_padh3
.w64_main2:
    pmaddubsw            m1, m3
    pmaddubsw            m2, m3
    packuswb             m1, m2
.w64_main:
    add                srcq, 16*4
    movu        [dstq+16*0], m0
    movu        [dstq+16*1], m1
    add                dstq, 16*2
    dec                  hd
    jg .w64_loop
    test                bhd, bhd
    jz .w64_end
.w64_padv:
    movu        [dstq+16*0], m0
    movu        [dstq+16*1], m1
    movu        [dstq+16*2], m0
    movu        [dstq+16*3], m1
    add                dstq, 16*4
    sub                 bhd, 2
    jg .w64_padv
.w64_end:
    RET
.setup_padh:
    mova                 m4, [base+pb_0to63]
    lea                 r6d, [wq-1]
    and                 r6d, 15
    movd                 m5, r6d
    pxor                 m0, m0
    pshufb               m5, m0
    pminub               m4, m5
    ret

%if ARCH_X86_64

INIT_YMM avx2
cglobal pal_idx_finish, 4, 7, 5, dst, src, bw, bh, w, h
%define base r6-pal_idx_finish_avx2_table
    lea                  r6, [pal_idx_finish_avx2_table]
    tzcnt               bwd, bwd
    movifnidn            wd, wm
    movifnidn            hd, hm
    movsxd              bwq, [r6+bwq*4]
    vpbroadcastd         m2, [base+pb_1_16]
    dec                  wd
    add                 bwq, r6
    sub                 bhd, hd
    jmp                 bwq
.w4:
    mova                xm0, [srcq]
    add                srcq, 16
    pmaddubsw           xm0, xm2
    packuswb            xm0, xm0
    movq             [dstq], xm0
    add                dstq, 8
    sub                  hd, 4
    jg .w4
    test                bhd, bhd
    jz .w4_end
    pshuflw             xm0, xm0, q3333
.w4_padv:
    movq             [dstq], xm0
    add                dstq, 8
    sub                 bhd, 4
    jg .w4_padv
.w4_end:
    RET
.w8_padh:
    pshufb              xm0, xm3
    pshufb              xm1, xm3
    jmp .w8_main
.w8:
    mova                xm3, [base+pal_idx_w8_padh]
.w8_loop:
    mova                xm0, [srcq+16*0]
    mova                xm1, [srcq+16*1]
    cmp                  wd, 7
    jl .w8_padh
.w8_main:
    pmaddubsw           xm0, xm2
    pmaddubsw           xm1, xm2
    add                srcq, 16*2
    packuswb            xm0, xm1
    movu             [dstq], xm0
    add                dstq, 16
    sub                  hd, 4
    jg .w8_loop
    test                bhd, bhd
    jz .w8_end
    pshufd              xm0, xm0, q3333
.w8_padv:
    movu             [dstq], xm0
    add                dstq, 16
    sub                 bhd, 4
    jg .w8_padv
.w8_end:
    RET
.w16_padh:
    pshufb               m0, m3
    pshufb               m1, m3
    jmp .w16_main
.w16:
    cmp                  wd, 15
    je .w16_loop
    vbroadcasti128       m0, [base+pb_0to63]
    movd                xm3, wd
    vpbroadcastb         m3, xm3
    pminub               m3, m0
.w16_loop:
    mova                 m0, [srcq+32*0]
    mova                 m1, [srcq+32*1]
    cmp                  wd, 15
    jl .w16_padh
.w16_main:
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    add                srcq, 32*2
    packuswb             m0, m1
    vpermq               m1, m0, q3120
    movu             [dstq], m1
    add                dstq, 32
    sub                  hd, 4
    jg .w16_loop
    test                bhd, bhd
    jz .w16_end
    vpermq               m0, m0, q3333
.w16_padv:
    movu             [dstq], m0
    add                dstq, 32
    sub                 bhd, 4
    jg .w16_padv
.w16_end:
    RET
.w32_padh:
    cmp                  wd, 15
    jg .w32_padh2
    vinserti128          m0, xm0, 1
    vinserti128          m1, xm1, 1
.w32_padh2:
    pshufb               m0, m3
    pshufb               m1, m3
    jmp .w32_main
.w32:
    cmp                  wd, 31
    je .w32_loop
    movd                xm3, wd
    vpbroadcastb         m3, xm3
    pminub               m3, [base+pb_0to63]
.w32_loop:
    mova                 m0, [srcq+32*0]
    mova                 m1, [srcq+32*1]
    cmp                  wd, 31
    jl .w32_padh
.w32_main:
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    add                srcq, 32*2
    packuswb             m0, m1
    vpermq               m1, m0, q3120
    movu             [dstq], m1
    add                dstq, 32
    sub                  hd, 2
    jg .w32_loop
    test                bhd, bhd
    jz .w32_end
    vpermq               m0, m0, q3131
.w32_padv:
    movu        [dstq+32*0], m0
    movu        [dstq+32*1], m0
    add                dstq, 32*2
    sub                 bhd, 4
    jg .w32_padv
.w32_end:
    RET
.w64_padh:
    cmp                  wd, 15
    jg .w64_padh2
    vinserti128          m1, m0, xm0, 1
    pshufb               m0, m1, m3
    pshufb               m1, m4
    jmp .w64_main
.w64_padh2:
    cmp                  wd, 31
    jg .w64_padh3
    vperm2i128           m1, m0, m0, 0x11
    pshufb               m0, m3
    pshufb               m1, m4
    jmp .w64_main
.w64_padh3:
    cmp                  wd, 47
    jg .w64_padh4
    vinserti128          m1, xm1, 1
.w64_padh4:
    pshufb               m1, m3
    jmp .w64_main
.w64:
    cmp                  wd, 63
    je .w64_loop
    mov                 r6d, wd
    and                 r6d, 31
    movd                xm4, r6d
    vpbroadcastb         m4, xm4
    pminub               m3, m4, [pb_0to63]
.w64_loop:
    mova                 m0, [srcq+32*0]
    mova                 m1, [srcq+32*1]
    cmp                  wd, 63
    jl .w64_padh
.w64_main:
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    add                srcq, 32*2
    packuswb             m0, m1
    vpermq               m0, m0, q3120
    movu             [dstq], m0
    add                dstq, 32
    dec                  hd
    jg .w64_loop
    test                bhd, bhd
    jz .w64_end
.w64_padv:
    movu        [dstq+32*0], m0
    movu        [dstq+32*1], m0
    movu        [dstq+32*2], m0
    movu        [dstq+32*3], m0
    add                dstq, 32*4
    sub                 bhd, 4
    jg .w64_padv
.w64_end:
    RET

INIT_ZMM avx512icl
cglobal pal_idx_finish, 4, 7, 7, dst, src, bw, bh, w, h
%define base r6-pal_idx_finish_avx512icl_table
    lea                  r6, [pal_idx_finish_avx512icl_table]
    tzcnt               bwd, bwd
    movifnidn            wd, wm
    movifnidn            hd, hm
    movsxd              bwq, [r6+bwq*4]
    vpbroadcastd         m4, [base+pb_1_16]
    dec                  wd
    add                 bwq, r6
    sub                 bhd, hd
    jmp                 bwq
.w4:
    mova               xmm0, [srcq]
    add                srcq, 16
    pmaddubsw          xmm0, xm4
    packuswb           xmm0, xmm0
    movq             [dstq], xmm0
    add                dstq, 8
    sub                  hd, 4
    jg .w4
    test                bhd, bhd
    jz .w4_end
    pshuflw            xmm0, xmm0, q3333
.w4_padv:
    movq             [dstq], xmm0
    add                dstq, 8
    sub                 bhd, 4
    jg .w4_padv
.w4_end:
    RET
.w8_padh:
    pshufb             xmm0, xmm2
    pshufb             xmm1, xmm2
    jmp .w8_main
.w8:
    mova               xmm2, [base+pal_idx_w8_padh]
.w8_loop:
    mova               xmm0, [srcq+16*0]
    mova               xmm1, [srcq+16*1]
    cmp                  wd, 7
    jl .w8_padh
.w8_main:
    pmaddubsw          xmm0, xm4
    pmaddubsw          xmm1, xm4
    add                srcq, 16*2
    packuswb           xmm0, xmm1
    movu             [dstq], xmm0
    add                dstq, 16
    sub                  hd, 4
    jg .w8_loop
    test                bhd, bhd
    jz .w8_end
    pshufd             xmm0, xmm0, q3333
.w8_padv:
    movu             [dstq], xmm0
    add                dstq, 16
    sub                 bhd, 4
    jg .w8_padv
.w8_end:
    RET
.w16_padh:
    pshufb               m0, m2
    jmp .w16_main
.w16:
    cmp                  wd, 15
    je .w16_loop
    vbroadcasti32x4      m2, [base+pb_0to63]
    vpbroadcastb         m0, wd
    pminub               m2, m0
.w16_loop:
    mova                 m0, [srcq]
    cmp                  wd, 15
    jl .w16_padh
.w16_main:
    pmaddubsw            m0, m4
    add                srcq, 64
    vpmovwb             ym0, m0
    movu             [dstq], ym0
    add                dstq, 32
    sub                  hd, 4
    jg .w16_loop
    test                bhd, bhd
    jz .w16_end
    vpermq              ym0, ym0, q3333
.w16_padv:
    movu             [dstq], ym0
    add                dstq, 32
    sub                 bhd, 4
    jg .w16_padv
.w16_end:
    RET
.w32_padh:
    vpermb               m0, m2, m0
    vpermb               m1, m2, m1
    jmp .w32_main
.w32:
    mova                 m2, [base+pb_0to63]
    paddb                m3, m2, m2
    cmp                  wd, 31
    je .w32_loop
    vpbroadcastb         m0, wd
    mov                 r6d, 0xff00
    kmovw                k1, r6d
    vpaddd           m0{k1}, [pb_32] {1to16}
    pminub               m2, m0
.w32_loop:
    mova                 m0, [srcq+64*0]
    mova                 m1, [srcq+64*1]
    cmp                  wd, 31
    jl .w32_padh
.w32_main:
    pmaddubsw            m0, m4
    pmaddubsw            m1, m4
    add                srcq, 64*2
    vpermt2b             m0, m3, m1
    movu             [dstq], m0
    add                dstq, 64
    sub                  hd, 4
    jg .w32_loop
    test                bhd, bhd
    jz .w32_end
    vshufi32x4           m0, m0, q3333
.w32_padv:
    movu             [dstq], m0
    add                dstq, 64
    sub                 bhd, 4
    jg .w32_padv
.w32_end:
    RET
.w64_padh:
    REPX  {vpermb x, m5, x}, m0, m1, m2, m3
    jmp .w64_main
.w64:
    mova                 m5, [base+pb_0to63]
    paddb                m6, m5, m5
    cmp                  wd, 63
    je .w64_loop
    vpbroadcastb         m0, wd
    pminub               m5, m0
.w64_loop:
    mova                 m0, [srcq+64*0]
    mova                 m1, [srcq+64*1]
    mova                 m2, [srcq+64*2]
    mova                 m3, [srcq+64*3]
    cmp                  wd, 63
    jl .w64_padh
.w64_main:
    REPX  {pmaddubsw x, m4}, m0, m1, m2, m3
    add                srcq, 64*4
    vpermt2b             m0, m6, m1
    vpermt2b             m2, m6, m3
    movu        [dstq+64*0], m0
    movu        [dstq+64*1], m2
    add                dstq, 64*2
    sub                  hd, 4
    jg .w64_loop
    test                bhd, bhd
    jz .w64_end
    vshufi32x4           m2, m2, q3232
.w64_padv:
    movu        [dstq+64*0], m2
    movu        [dstq+64*1], m2
    add                dstq, 64*2
    sub                 bhd, 4
    jg .w64_padv
.w64_end:
    RET

%endif ; ARCH_X86_64
