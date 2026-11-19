; Copyright © 2026, VideoLAN and dav1d authors
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

SECTION_RODATA

%macro JMP_TABLE 2-*
    %1_table:
    %xdefine %%base %1_table
    %xdefine %%current_base %1
    %xdefine %%prefix mangle(private_prefix %+ _ %+ %%current_base)
    %rotate 1
    %rep %0 - 1
        %ifnum %1
            dd %%prefix %+ .w%1 - %%base
        %else
            %xdefine %%current_base %1
            %xdefine %%prefix mangle(private_prefix %+ _ %+ %%current_base)
        %endif
        %rotate 1
    %endrep
%endmacro

JMP_TABLE fill_levelcache_block_sse2, 1, 2, 4, 8, 16, 32
JMP_TABLE fill_levelcache_block_avx2, \
          fill_levelcache_block_sse2, 1, 2, 4, \
          fill_levelcache_block_avx2, 8, 16, 32

SECTION .text

%macro LC_PTR_LOOP 1
    add  lc_ptrq, strideq
    dec      bhd
    jnz %1
    RET
%endmacro

INIT_XMM sse2
; uint8_t (*level_cache_ptr)[4], ptrdiff_t b_stride, int chroma, int bw, int bh,
; unsigned v0, unsigned v1
cglobal fill_levelcache_block, 4, 7, 5, lc_ptr, stride, bw, bh, v0, v1, tmp
    movifnidn  v1d, v1m
    shl        v1d, 8
    or         v1d, v0m        ; v0 | v1
    add    strideq, strideq

    lea       tmpd, [bwq-1]
    test      tmpd, bwd
    jnz .scalar                ; non-powers of two

    bsr       tmpd, bwd
    lea        v0q, [fill_levelcache_block_sse2_table]
    movsx     tmpq, dword [v0q+4*tmpq]
    add        v0q, tmpq
    jmp        v0q

.scalar:
    xor              tmpd, tmpd
.scalar_row:
    mov  [lc_ptrq+2*tmpq], v1w
    inc              tmpd
    cmp              tmpd, bwd
    jne .scalar_row
    LC_PTR_LOOP .scalar

.w1:
    mov [lc_ptrq], v1w
    LC_PTR_LOOP .w1

.w2:
    imul              v1d, v1d, 0x10001
.w2_loop:
    mov         [lc_ptrq], v1d
    LC_PTR_LOOP .w2_loop

.w4:
    mov              tmpq, 0x1000100010001
    imul              v1q, tmpq
.w4_loop:
    mov         [lc_ptrq], v1q
    LC_PTR_LOOP .w4_loop

.w8:
    movd               m0, v1d
    pshuflw            m0, m0, q0000
    punpcklqdq         m0, m0
.w8_loop:
    movu        [lc_ptrq], m0
    LC_PTR_LOOP .w8_loop

.w16:
    movd               m0, v1d
    pshuflw            m0, m0, q0000
    punpcklqdq         m0, m0
.w16_loop:
    movu        [lc_ptrq], m0
    movu     [lc_ptrq+16], m0
    LC_PTR_LOOP .w16_loop

.w32:
    movd               m0, v1d
    pshuflw            m0, m0, q0000
    punpcklqdq         m0, m0
.w32_loop:
    movu        [lc_ptrq], m0
    movu     [lc_ptrq+16], m0
    movu     [lc_ptrq+32], m0
    movu     [lc_ptrq+48], m0
    LC_PTR_LOOP .w32_loop


INIT_YMM avx2
; uint8_t (*level_cache_ptr)[4], ptrdiff_t b_stride, int chroma, int bw, int bh,
; unsigned v0, unsigned v1
cglobal fill_levelcache_block, 4, 7, 5, lc_ptr, stride, bw, bh, v0, v1, tmp
    rorx       v1d, v1m, 24
    or         v1d, v0m        ; v0 | v1
    add    strideq, strideq

    blsr      tmpd, bwd
    jnz mangle(private_prefix %+ _fill_levelcache_block_sse2).scalar
    tzcnt     tmpd, bwd

    lea        v0q, [fill_levelcache_block_avx2_table]
    movsx     tmpq, dword [v0q+4*tmpq]
    add        v0q, tmpq
    jmp        v0q

; (shared path with sse2 for scalar, w1, w2, w4)

.w8:
    movd              xm0, v1d
    vpbroadcastw      xm0, xm0
.w8_loop:
    movu        [lc_ptrq], xm0
    LC_PTR_LOOP .w8_loop

.w16:
    movd              xm0, v1d
    vpbroadcastw       m0, xm0
.w16_loop:
    movu        [lc_ptrq], m0
    LC_PTR_LOOP .w16_loop

.w32:
    movd              xm0, v1d
    vpbroadcastw       m0, xm0
.w32_loop:
    movu        [lc_ptrq], m0
    movu     [lc_ptrq+32], m0
    LC_PTR_LOOP .w32_loop


%endif  ; ARCH_X86_64
