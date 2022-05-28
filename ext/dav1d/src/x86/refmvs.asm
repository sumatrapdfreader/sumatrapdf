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

SECTION_RODATA 64

%macro JMP_TABLE 2-*
    %xdefine %%prefix mangle(private_prefix %+ _%1)
    %1_table:
    %xdefine %%base %1_table
    %rep %0 - 1
        dd %%prefix %+ .w%2 - %%base
        %rotate 1
    %endrep
%endmacro

%if ARCH_X86_64
splat_mv_shuf: db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3
               db  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7
               db  8,  9, 10, 11,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11
               db  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11,  0,  1,  2,  3

JMP_TABLE splat_mv_avx512icl, 1, 2, 4, 8, 16, 32
JMP_TABLE splat_mv_avx2,      1, 2, 4, 8, 16, 32
%endif
JMP_TABLE splat_mv_sse2,      1, 2, 4, 8, 16, 32

SECTION .text

INIT_XMM sse2
; refmvs_block **rr, refmvs_block *a, int bx4, int bw4, int bh4
cglobal splat_mv, 4, 5, 3, rr, a, bx4, bw4, bh4
    add           bx4d, bw4d
    tzcnt         bw4d, bw4d
    mova            m2, [aq]
    LEA             aq, splat_mv_sse2_table
    lea           bx4q, [bx4q*3-32]
    movsxd        bw4q, [aq+bw4q*4]
    movifnidn     bh4d, bh4m
    pshufd          m0, m2, q0210
    pshufd          m1, m2, q1021
    pshufd          m2, m2, q2102
    add           bw4q, aq
.loop:
    mov             aq, [rrq]
    add            rrq, gprsize
    lea             aq, [aq+bx4q*4]
    jmp           bw4q
.w32:
    mova    [aq-16*16], m0
    mova    [aq-16*15], m1
    mova    [aq-16*14], m2
    mova    [aq-16*13], m0
    mova    [aq-16*12], m1
    mova    [aq-16*11], m2
    mova    [aq-16*10], m0
    mova    [aq-16* 9], m1
    mova    [aq-16* 8], m2
    mova    [aq-16* 7], m0
    mova    [aq-16* 6], m1
    mova    [aq-16* 5], m2
.w16:
    mova    [aq-16* 4], m0
    mova    [aq-16* 3], m1
    mova    [aq-16* 2], m2
    mova    [aq-16* 1], m0
    mova    [aq+16* 0], m1
    mova    [aq+16* 1], m2
.w8:
    mova    [aq+16* 2], m0
    mova    [aq+16* 3], m1
    mova    [aq+16* 4], m2
.w4:
    mova    [aq+16* 5], m0
    mova    [aq+16* 6], m1
    mova    [aq+16* 7], m2
    dec           bh4d
    jg .loop
    RET
.w2:
    movu      [aq+104], m0
    movq      [aq+120], m1
    dec           bh4d
    jg .loop
    RET
.w1:
    movq      [aq+116], m0
    movd      [aq+124], m2
    dec           bh4d
    jg .loop
    RET

%if ARCH_X86_64
INIT_YMM avx2
cglobal splat_mv, 4, 5, 3, rr, a, bx4, bw4, bh4
    add           bx4d, bw4d
    tzcnt         bw4d, bw4d
    vbroadcasti128  m0, [aq]
    lea             aq, [splat_mv_avx2_table]
    lea           bx4q, [bx4q*3-32]
    movsxd        bw4q, [aq+bw4q*4]
    pshufb          m0, [splat_mv_shuf]
    movifnidn     bh4d, bh4m
    pshufd          m1, m0, q2102
    pshufd          m2, m0, q1021
    add           bw4q, aq
.loop:
    mov             aq, [rrq]
    add            rrq, gprsize
    lea             aq, [aq+bx4q*4]
    jmp           bw4q
.w32:
    mova     [aq-32*8], m0
    mova     [aq-32*7], m1
    mova     [aq-32*6], m2
    mova     [aq-32*5], m0
    mova     [aq-32*4], m1
    mova     [aq-32*3], m2
.w16:
    mova     [aq-32*2], m0
    mova     [aq-32*1], m1
    mova     [aq+32*0], m2
.w8:
    mova     [aq+32*1], m0
    mova     [aq+32*2], m1
    mova     [aq+32*3], m2
    dec           bh4d
    jg .loop
    RET
.w4:
    movu      [aq+ 80], m0
    mova      [aq+112], xm1
    dec           bh4d
    jg .loop
    RET
.w2:
    movu      [aq+104], xm0
    movq      [aq+120], xm2
    dec           bh4d
    jg .loop
    RET
.w1:
    movq      [aq+116], xm0
    movd      [aq+124], xm1
    dec           bh4d
    jg .loop
    RET

INIT_ZMM avx512icl
cglobal splat_mv, 4, 7, 3, rr, a, bx4, bw4, bh4
    vbroadcasti32x4    m0, [aq]
    lea                r1, [splat_mv_avx512icl_table]
    tzcnt            bw4d, bw4d
    lea              bx4d, [bx4q*3]
    pshufb             m0, [splat_mv_shuf]
    movsxd           bw4q, [r1+bw4q*4]
    mov               r6d, bh4m
    add              bw4q, r1
    lea               rrq, [rrq+r6*8]
    mov               r1d, 0x3f
    neg                r6
    kmovb              k1, r1d
    jmp              bw4q
.w1:
    mov                r1, [rrq+r6*8]
    vmovdqu16 [r1+bx4q*4]{k1}, xm0
    inc                r6
    jl .w1
    RET
.w2:
    mov                r1, [rrq+r6*8]
    vmovdqu32 [r1+bx4q*4]{k1}, ym0
    inc                r6
    jl .w2
    RET
.w4:
    mov                r1, [rrq+r6*8]
    vmovdqu64 [r1+bx4q*4]{k1}, m0
    inc                r6
    jl .w4
    RET
.w8:
    pshufd            ym1, ym0, q1021
.w8_loop:
    mov                r1, [rrq+r6*8+0]
    mov                r3, [rrq+r6*8+8]
    movu   [r1+bx4q*4+ 0], m0
    mova   [r1+bx4q*4+64], ym1
    movu   [r3+bx4q*4+ 0], m0
    mova   [r3+bx4q*4+64], ym1
    add                r6, 2
    jl .w8_loop
    RET
.w16:
    pshufd             m1, m0, q1021
    pshufd             m2, m0, q2102
.w16_loop:
    mov                r1, [rrq+r6*8+0]
    mov                r3, [rrq+r6*8+8]
    mova [r1+bx4q*4+64*0], m0
    mova [r1+bx4q*4+64*1], m1
    mova [r1+bx4q*4+64*2], m2
    mova [r3+bx4q*4+64*0], m0
    mova [r3+bx4q*4+64*1], m1
    mova [r3+bx4q*4+64*2], m2
    add                r6, 2
    jl .w16_loop
    RET
.w32:
    pshufd             m1, m0, q1021
    pshufd             m2, m0, q2102
.w32_loop:
    mov                r1, [rrq+r6*8]
    lea                r1, [r1+bx4q*4]
    mova        [r1+64*0], m0
    mova        [r1+64*1], m1
    mova        [r1+64*2], m2
    mova        [r1+64*3], m0
    mova        [r1+64*4], m1
    mova        [r1+64*5], m2
    inc                r6
    jl .w32_loop
    RET
%endif ; ARCH_X86_64
