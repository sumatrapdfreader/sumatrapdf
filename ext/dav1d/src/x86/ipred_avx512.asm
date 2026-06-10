; Copyright © 2020, VideoLAN and dav1d authors
; Copyright © 2020, Two Orioles, LLC
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

%macro SMOOTH_WEIGHT_TABLE 1-*
    %rep %0
        db %1-128, 127-%1
        %rotate 1
    %endrep
%endmacro

smooth_weights: SMOOTH_WEIGHT_TABLE         \
      0,   0, 255, 128, 255, 149,  85,  64, \
    255, 197, 146, 105,  73,  50,  37,  32, \
    255, 225, 196, 170, 145, 123, 102,  84, \
     68,  54,  43,  33,  26,  20,  17,  16, \
    255, 240, 225, 210, 196, 182, 169, 157, \
    145, 133, 122, 111, 101,  92,  83,  74, \
     66,  59,  52,  45,  39,  34,  29,  25, \
     21,  17,  14,  12,  10,   9,   8,   8, \
    255, 248, 240, 233, 225, 218, 210, 203, \
    196, 189, 182, 176, 169, 163, 156, 150, \
    144, 138, 133, 127, 121, 116, 111, 106, \
    101,  96,  91,  86,  82,  77,  73,  69, \
     65,  61,  57,  54,  50,  47,  44,  41, \
     38,  35,  32,  29,  27,  25,  22,  20, \
     18,  16,  15,  13,  12,  10,   9,   8, \
      7,   6,   6,   5,   5,   4,   4,   4

; dav1d_filter_intra_taps[], reordered for VNNI: p1 p2 p3 p4, p6 p5 p0 __
filter_taps:  db 10,  0,  0,  0,  2, 10,  0,  0,  1,  1, 10,  0,  1,  1,  2, 10
              db  6,  0,  0,  0,  2,  6,  0,  0,  2,  2,  6,  0,  1,  2,  2,  6
              db  0, 12, -6,  0,  0,  9, -5,  0,  0,  7, -3,  0,  0,  5, -3,  0
              db 12,  2, -4,  0,  9,  2, -3,  0,  7,  2, -3,  0,  5,  3, -3,  0
              db 16,  0,  0,  0,  0, 16,  0,  0,  0,  0, 16,  0,  0,  0,  0, 16
              db 16,  0,  0,  0,  0, 16,  0,  0,  0,  0, 16,  0,  0,  0,  0, 16
              db  0, 10,-10,  0,  0,  6, -6,  0,  0,  4, -4,  0,  0,  2, -2,  0
              db 10,  0,-10,  0,  6,  0, -6,  0,  4,  0, -4,  0,  2,  0, -2,  0
              db  8,  0,  0,  0,  0,  8,  0,  0,  0,  0,  8,  0,  0,  0,  0,  8
              db  4,  0,  0,  0,  0,  4,  0,  0,  0,  0,  4,  0,  0,  0,  0,  4
              db  0, 16, -8,  0,  0, 16, -8,  0,  0, 16, -8,  0,  0, 16, -8,  0
              db 16,  0, -4,  0, 16,  0, -4,  0, 16,  0, -4,  0, 16,  0, -4,  0
              db  8,  0,  0,  0,  3,  8,  0,  0,  2,  3,  8,  0,  1,  2,  3,  8
              db  4,  0,  0,  0,  3,  4,  0,  0,  2,  3,  4,  0,  2,  2,  3,  4
              db  0, 10, -2,  0,  0,  6, -1,  0,  0,  4, -1,  0,  0,  2,  0,  0
              db 10,  3, -1,  0,  6,  4, -1,  0,  4,  4, -1,  0,  3,  3, -1,  0
              db 14,  0,  0,  0,  0, 14,  0,  0,  0,  0, 14,  0,  0,  0,  0, 14
              db 12,  0,  0,  0,  1, 12,  0,  0,  0,  0, 12,  0,  0,  0,  1, 12
              db  0, 14,-12,  0,  0, 12,-10,  0,  0, 11, -9,  0,  0, 10, -8,  0
              db 14,  0,-10,  0, 12,  0, -9,  0, 11,  1, -8,  0,  9,  1, -7,  0
filter_perm:  db  0,  1,  2,  3, 24, 25, 26, 27,  4,  5,  6,  7, 28, 29, 30, 31
              db 15, 11,  7,  3, 15, 11,  7,  3, 15, 11,  7,  3, 15, 11,  7,131
              db 31, 27, 23, 19, 31, 27, 23, 19, 31, 27, 23, 19, 31, 27, 23,147
              db 47, 43, 39, 35, 47, 43, 39, 35, 47, 43, 39, 35, 47, 43, 39,163
filter_end:   dd  2,  3, 16, 17, -1, -1, 20, 21,  0,  6, 24, 30,  1,  7, 25, 31
smooth_shuf:  db  7,  7,  7,  7,  0,  1,  0,  1,  3,  3,  3,  3,  8,  9,  8,  9
              db  5,  5,  5,  5,  4,  5,  4,  5,  1,  1,  1,  1, 12, 13, 12, 13
              db  6,  6,  6,  6,  2,  3,  2,  3,  2,  2,  2,  2, 10, 11, 10, 11
              db  4,  4,  4,  4,  6,  7,  6,  7,  0,  0,  0,  0, 14, 15, 14, 15
smooth_endA:  db  1,  3,  5,  7,  9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
              db 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 61, 63
              db 65, 67, 69, 71, 73, 75, 77, 79, 81, 83, 85, 87, 89, 91, 93, 95
              db 97, 99,101,103,105,107,109,111,113,115,117,119,121,123,125,127
smooth_endB:  db  1,  3,  5,  7,  9, 11, 13, 15, 65, 67, 69, 71, 73, 75, 77, 79
              db 17, 19, 21, 23, 25, 27, 29, 31, 81, 83, 85, 87, 89, 91, 93, 95
              db 33, 35, 37, 39, 41, 43, 45, 47, 97, 99,101,103,105,107,109,111
              db 49, 51, 53, 55, 57, 59, 61, 63,113,115,117,119,121,123,125,127
ipred_h_shuf: db  7,  7,  7,  7,  6,  6,  6,  6,  5,  5,  5,  5,  4,  4,  4,  4
              db  3,  3,  3,  3,  2,  2,  2,  2,  1,  1,  1,  1,  0,  0,  0,  0
pal_unpack:   db  0,  4,  8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60
pal_perm:     db  0,  8,  1,  9,  2, 10,  3, 11,  4, 12,  5, 13,  6, 14,  7, 15
pb_63to0:     db 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48
              db 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32
              db 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16
              db 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0
z_frac_table: db 64,  0, 62,  2, 60,  4, 58,  6, 56,  8, 54, 10, 52, 12, 50, 14
              db 48, 16, 46, 18, 44, 20, 42, 22, 40, 24, 38, 26, 36, 28, 34, 30
              db 32, 32, 30, 34, 28, 36, 26, 38, 24, 40, 22, 42, 20, 44, 18, 46
              db 16, 48, 14, 50, 12, 52, 10, 54,  8, 56,  6, 58,  4, 60,  2, 62
z_filter_s1:  db -1, -1, -1,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6
              db 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22
              db 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38
              db 46, 47, 47, 48, 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 53, 54
z_filter_s5:  db 10,  9, 11, 10, 12, 11, 13, 12, 14, 13, 15, 14, 16, 15, 17, 16
              db 26, 25, 27, 26, 28, 27, 29, 28, 30, 29, 31, 30, 32, 31, 33, 32
              db 42, 41, 43, 42, 44, 43, 45, 44, 46, 45, 47, 46, 48, 47, 49, 48
              db 58, 57, 59, 58, 60, 59, 61, 60, 62, 61, 63, 62, 64, 63, 65, 64
z_filter_s3:  db  0,  8,  1,  9,  2, 10,  3, 11,  4, 12,  5, 13,  6, 14,  7, 15
z_filter_s2:  db  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14
z_filter_s4:  db  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9,  8
z_xpos_bc:    db 17, 17, 17, 17, 33, 33, 33, 33,  9,  9,  9,  9,  9,  9,  9,  9
z_filter4_s1: db  0,  0,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7
              db  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8
z_xpos_off1a: db 64, 65, 65, 66, 66, 67, 67, 68, 68, 69, 69, 70, 70, 71, 71, 72
z_xpos_off1b: db 72, 73, 73, 74, 74, 75, 75, 76, 76, 77, 77, 78, 78, 79, 79, 80
z_xpos_off2a: db  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8
              db 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24
              db 32, 33, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 39, 40
              db 48, 49, 49, 50, 50, 51, 51, 52, 52, 53, 53, 54, 54, 55, 55, 56
z_xpos_off2b: db  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16
              db 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32
              db 40, 41, 41, 42, 42, 43, 43, 44, 44, 45, 45, 46, 46, 47, 47, 48
              db 56, 57, 57, 58, 58, 59, 59, 60, 60, 61, 61, 62, 62, 63, 63, 64
z_xpos_mul:   dw  4,  4,  4,  4,  8,  8,  4,  4, 12, 12,  8,  8, 16, 16,  8,  8
              dw 20, 20, 12, 12, 24, 24, 12, 12, 28, 28, 16, 16, 32, 32, 16, 16
z_ypos_off1:  db 64, 65, 64, 65, 64, 65, 64, 65, 65, 66, 65, 66, 66, 67, 66, 67
              db 66, 67, 66, 67, 68, 69, 68, 69, 67, 68, 67, 68, 70, 71, 70, 71
              db 68, 69, 68, 69, 72, 73, 72, 73, 69, 70, 69, 70, 74, 75, 74, 75
              db 70, 71, 70, 71, 76, 77, 76, 77, 71, 72, 71, 72, 78, 79, 78, 79
z_ypos_off2:  db 64, 65, 64, 65,  0,  0,  0,  0, 64, 65, 64, 65,  0,  0,  0,  0
              db 65, 66, 65, 66,  1,  1,  1,  1, 65, 66, 65, 66,  1,  1,  1,  1
              db 66, 67, 66, 67,  2,  2,  2,  2, 66, 67, 66, 67,  2,  2,  2,  2
              db 67, 68, 67, 68,  3,  3,  3,  3, 67, 68, 67, 68,  3,  3,  3,  3
z_ypos_off3:  db  1,  2,  1,  2,  1,  1,  1,  1,  3,  4,  3,  4,  1,  1,  1,  1
              db  5,  6,  5,  6,  3,  3,  3,  3,  7,  8,  7,  8,  3,  3,  3,  3
              db  9, 10,  9, 10,  5,  5,  5,  5, 11, 12, 11, 12,  5,  5,  5,  5
              db 13, 14, 13, 14,  7,  7,  7,  7, 15, 16, 15, 16,  7,  7,  7,  7
z_ypos_mul1a: dw  1,  2,  3,  4,  5,  6,  7,  8, 17, 18, 19, 20, 21, 22, 23, 24
              dw 33, 34, 35, 36, 37, 38, 39, 40, 49, 50, 51, 52, 53, 54, 55, 56
z_ypos_mul1b: dw  9, 10, 11, 12, 13, 14, 15, 16, 25, 26, 27, 28, 29, 30, 31, 32
              dw 41, 42, 43, 44, 45, 46, 47, 48, 57, 58, 59, 60, 61, 62, 63, 64
z_ypos_mul2a: dw  1*512,  2*512,  3*512,  4*512,  5*512,  6*512,  7*512,  8*512
              dw 17*512, 18*512, 19*512, 20*512, 21*512, 22*512, 23*512, 24*512
              dw 33*512, 34*512, 35*512, 36*512, 37*512, 38*512, 39*512, 40*512
              dw 49*512, 50*512, 51*512, 52*512, 53*512, 54*512, 55*512, 56*512
z_ypos_mul2b: dw  9*512, 10*512, 11*512, 12*512, 13*512, 14*512, 15*512, 16*512
              dw 25*512, 26*512, 27*512, 28*512, 29*512, 30*512, 31*512, 32*512
              dw 41*512, 42*512, 43*512, 44*512, 45*512, 46*512, 47*512, 48*512
              dw 57*512, 58*512, 59*512, 60*512, 61*512, 62*512, 63*512, 64*512
z_filter_t0:  db 55,127, 39,127, 39,127,  7, 15, 31,  7, 15, 31,  0,  3, 31,  0
z_filter_t1:  db 39, 63, 19, 47, 19, 47,  3,  3,  3,  3,  3,  3,  0,  0,  0,  0
z3_upsample:  db 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16
              db 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10,  9,  8
z_filter_wh:  db  7,  7, 11, 11, 15, 15, 19, 19, 19, 23, 23, 23, 31, 31, 31, 39
              db 39, 39, 47, 47, 47, 79, 79, 79
z_filter_k:   db  0, 16,  0, 16,  0, 20,  0, 20,  8, 16,  8, 16
              db 32,  0, 32,  0, 24,  0, 24,  0, 16,  0, 16,  0
              db  0, 32,  0, 32,  0, 24,  0, 24,  0, 16,  0, 16

pb_8_56_0_0:  db  8, 56,  0,  0
pb_m4_36:     times 2 db -4, 36
pb_127_m127:  times 2 db 127, -127
pb_8:         times 4 db 8
pb_15:        times 4 db 15
pb_16:        times 4 db 16
pb_31:        times 4 db 31
pb_63:        times 4 db 63
pb_90:        times 4 db 90
pb_128:       times 4 db 128
pw_128:       times 2 dw 128
pw_255:       times 2 dw 255
pw_512:       times 2 dw 512

%define pb_1  (ipred_h_shuf+24)
%define pb_2  (ipred_h_shuf+20)
%define pb_3  (ipred_h_shuf+16)
%define pb_4  (smooth_shuf +48)
%define pb_7  (ipred_h_shuf+ 0)
%define pb_9  (z_xpos_bc   + 8)
%define pb_17 (z_xpos_bc   + 0)
%define pb_33 (z_xpos_bc   + 4)
%define pd_8  (filter_taps+128)

%macro JMP_TABLE 3-*
    %xdefine %1_%2_table (%%table - 2*4)
    %xdefine %%base mangle(private_prefix %+ _%1_%2)
    %%table:
    %rep %0 - 2
        dd %%base %+ .%3 - (%%table - 2*4)
        %rotate 1
    %endrep
%endmacro

%define ipred_dc_splat_8bpc_avx512icl_table (ipred_dc_8bpc_avx512icl_table + 10*4)

JMP_TABLE ipred_h_8bpc,          avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_paeth_8bpc,      avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_8bpc,     avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_v_8bpc,   avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_smooth_h_8bpc,   avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z1_8bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z2_8bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_z3_8bpc,         avx512icl, w4, w8, w16, w32, w64
JMP_TABLE ipred_dc_8bpc,         avx512icl, h4, h8, h16, h32, h64, w4, w8, w16, w32, w64, \
                                       s4-10*4, s8-10*4, s16-10*4, s32-10*4, s64-10*4
JMP_TABLE ipred_dc_left_8bpc,    avx512icl, h4, h8, h16, h32, h64

cextern dr_intra_derivative
cextern pb_0to63

SECTION .text

INIT_ZMM avx512icl
cglobal ipred_dc_top_8bpc, 3, 7, 5, dst, stride, tl, w, h
    lea                  r5, [ipred_dc_left_8bpc_avx512icl_table]
    movd                xm0, wm
    tzcnt                wd, wm
    inc                 tlq
    movifnidn            hd, hm
    movu                ym1, [tlq]
    movd               xmm3, wd
    movsxd               r6, [r5+wq*4]
    vpbroadcastd        ym2, [r5-ipred_dc_left_8bpc_avx512icl_table+pb_1]
    psrld               xm0, 1
    vpdpbusd            ym0, ym1, ym2
    add                  r6, r5
    add                  r5, ipred_dc_splat_8bpc_avx512icl_table-ipred_dc_left_8bpc_avx512icl_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6

cglobal ipred_dc_left_8bpc, 3, 7, 5, dst, stride, tl, w, h, stride3
    lea                  r5, [ipred_dc_left_8bpc_avx512icl_table]
    mov                  hd, hm
    tzcnt               r6d, hd
    sub                 tlq, hq
    tzcnt                wd, wm
    movd                xm0, hm
    movu                ym1, [tlq]
    movd               xmm3, r6d
    movsxd               r6, [r5+r6*4]
    vpbroadcastd        ym2, [r5-ipred_dc_left_8bpc_avx512icl_table+pb_1]
    psrld               xm0, 1
    vpdpbusd            ym0, ym1, ym2
    add                  r6, r5
    add                  r5, ipred_dc_splat_8bpc_avx512icl_table-ipred_dc_left_8bpc_avx512icl_table
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    jmp                  r6
.h64:
    movu                ym1, [tlq+32] ; unaligned when jumping here from dc_top
    vpdpbusd            ym0, ym1, ym2
.h32:
    vextracti32x4       xm1, ym0, 1
    paddd               xm0, xm1
.h16:
    punpckhqdq          xm1, xm0, xm0
    paddd               xm0, xm1
.h8:
    psrlq               xm1, xm0, 32
    paddd               xm0, xm1
.h4:
    vpsrlvd             xm0, xmm3
    lea            stride3q, [strideq*3]
    vpbroadcastb         m0, xm0
    jmp                  wq

cglobal ipred_dc_8bpc, 3, 7, 5, dst, stride, tl, w, h, stride3
    movifnidn            hd, hm
    movifnidn            wd, wm
    tzcnt               r6d, hd
    lea                 r5d, [wq+hq]
    movd                xm0, r5d
    tzcnt               r5d, r5d
    movd               xmm4, r5d
    lea                  r5, [ipred_dc_8bpc_avx512icl_table]
    tzcnt                wd, wd
    movsxd               r6, [r5+r6*4]
    movsxd               wq, [r5+wq*4+5*4]
    vpbroadcastd        ym3, [r5-ipred_dc_8bpc_avx512icl_table+pb_1]
    psrld               xm0, 1
    add                  r6, r5
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  r6
.h4:
    movd               xmm1, [tlq-4]
    vpdpbusd            xm0, xmm1, xm3
    jmp                  wq
.w4:
    movd               xmm1, [tlq+1]
    vpdpbusd            xm0, xmm1, xm3
    cmp                  hd, 4
    jg .w4_mul
    psrlw              xmm0, xm0, 3
    jmp .w4_end
.w4_mul:
    punpckhqdq         xmm1, xm0, xm0
    lea                 r2d, [hq*2]
    mov                 r6d, 0x55563334
    paddd              xmm1, xm0
    shrx                r6d, r6d, r2d
    psrlq              xmm0, xmm1, 32
    paddd              xmm0, xmm1
    movd               xmm1, r6d
    psrld              xmm0, 2
    pmulhuw            xmm0, xmm1
.w4_end:
    vpbroadcastb        xm0, xmm0
.s4:
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm0
    movd   [dstq+strideq*2], xm0
    movd   [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s4
    RET
.h8:
    movq               xmm1, [tlq-8]
    vpdpbusd            xm0, xmm1, xm3
    jmp                  wq
.w8:
    movq               xmm1, [tlq+1]
    vextracti32x4       xm2, ym0, 1
    vpdpbusd            xm0, xmm1, xm3
    paddd              xmm2, xm2, xm0
    punpckhqdq         xmm0, xmm2, xmm2
    paddd              xmm0, xmm2
    psrlq              xmm1, xmm0, 32
    paddd              xmm0, xmm1
    vpsrlvd            xmm0, xmm4
    cmp                  hd, 8
    je .w8_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    cmp                  hd, 32
    cmove               r6d, r2d
    movd               xmm1, r6d
    pmulhuw            xmm0, xmm1
.w8_end:
    vpbroadcastb        xm0, xmm0
.s8:
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm0
    movq   [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s8
    RET
.h16:
    mova               xmm1, [tlq-16]
    vpdpbusd            xm0, xmm1, xm3
    jmp                  wq
.w16:
    movu               xmm1, [tlq+1]
    vextracti32x4       xm2, ym0, 1
    vpdpbusd            xm0, xmm1, xm3
    paddd              xmm2, xm2, xm0
    punpckhqdq         xmm0, xmm2, xmm2
    paddd              xmm0, xmm2
    psrlq              xmm1, xmm0, 32
    paddd              xmm0, xmm1
    vpsrlvd            xmm0, xmm4
    cmp                  hd, 16
    je .w16_end
    mov                 r6d, 0x5556
    mov                 r2d, 0x3334
    test                 hb, 8|32
    cmovz               r6d, r2d
    movd               xmm1, r6d
    pmulhuw            xmm0, xmm1
.w16_end:
    vpbroadcastb        xm0, xmm0
.s16:
    mova   [dstq+strideq*0], xm0
    mova   [dstq+strideq*1], xm0
    mova   [dstq+strideq*2], xm0
    mova   [dstq+stride3q ], xm0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s16
    RET
.h32:
    mova                ym1, [tlq-32]
    vpdpbusd            ym0, ym1, ym3
    jmp                  wq
.w32:
    movu                ym1, [tlq+1]
    vpdpbusd            ym0, ym1, ym3
    vextracti32x4       xm1, ym0, 1
    paddd              xmm1, xm1, xm0
    punpckhqdq         xmm0, xmm1, xmm1
    paddd              xmm0, xmm1
    psrlq              xmm1, xmm0, 32
    paddd              xmm0, xmm1
    vpsrlvd            xmm0, xmm4
    cmp                  hd, 32
    je .w32_end
    lea                 r2d, [hq*2]
    mov                 r6d, 0x33345556
    shrx                r6d, r6d, r2d
    movd               xmm1, r6d
    pmulhuw            xmm0, xmm1
.w32_end:
    vpbroadcastb        ym0, xmm0
.s32:
    mova   [dstq+strideq*0], ym0
    mova   [dstq+strideq*1], ym0
    mova   [dstq+strideq*2], ym0
    mova   [dstq+stride3q ], ym0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s32
    RET
.h64:
    mova                ym1, [tlq-64]
    mova                ym2, [tlq-32]
    vpdpbusd            ym0, ym1, ym3
    vpdpbusd            ym0, ym2, ym3
    jmp                  wq
.w64:
    movu                ym1, [tlq+ 1]
    movu                ym2, [tlq+33]
    vpdpbusd            ym0, ym1, ym3
    vpdpbusd            ym0, ym2, ym3
    vextracti32x4       xm1, ym0, 1
    paddd              xmm1, xm1, xm0
    punpckhqdq         xmm0, xmm1, xmm1
    paddd              xmm0, xmm1
    psrlq              xmm1, xmm0, 32
    paddd              xmm0, xmm1
    vpsrlvd            xmm0, xmm4
    cmp                  hd, 64
    je .w64_end
    mov                 r6d, 0x33345556
    shrx                r6d, r6d, hd
    movd               xmm1, r6d
    pmulhuw            xmm0, xmm1
.w64_end:
    vpbroadcastb         m0, xmm0
.s64:
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m0
    mova   [dstq+strideq*2], m0
    mova   [dstq+stride3q ], m0
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .s64
    RET

cglobal ipred_dc_128_8bpc, 2, 7, 5, dst, stride, tl, w, h, stride3
    lea                  r5, [ipred_dc_splat_8bpc_avx512icl_table]
    tzcnt                wd, wm
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    vpbroadcastd         m0, [r5-ipred_dc_splat_8bpc_avx512icl_table+pb_128]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

cglobal ipred_v_8bpc, 3, 7, 5, dst, stride, tl, w, h, stride3
    lea                  r5, [ipred_dc_splat_8bpc_avx512icl_table]
    tzcnt                wd, wm
    movu                 m0, [tlq+1]
    movifnidn            hd, hm
    movsxd               wq, [r5+wq*4]
    add                  wq, r5
    lea            stride3q, [strideq*3]
    jmp                  wq

cglobal ipred_h_8bpc, 3, 7, 8, dst, stride, tl, w, h, stride3
%define base r6-ipred_h_8bpc_avx512icl_table
    lea                  r6, [ipred_h_8bpc_avx512icl_table]
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    lea            stride3q, [strideq*3]
    sub                 tlq, hq
    add                  wq, r6
    jmp                  wq
.w4:
    mova               xmm1, [base+ipred_h_shuf+16]
.w4_loop:
    movd               xmm0, [tlq+hq-4]
    pshufb             xmm0, xmm1
    movd   [dstq+strideq*0], xmm0
    pextrd [dstq+strideq*1], xmm0, 1
    pextrd [dstq+strideq*2], xmm0, 2
    pextrd [dstq+stride3q ], xmm0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4_loop
    RET
.w8:
    movsldup           xmm2, [base+ipred_h_shuf+16]
    movshdup           xmm3, [base+ipred_h_shuf+16]
.w8_loop:
    movd               xmm1, [tlq+hq-4]
    pshufb             xmm0, xmm1, xmm2
    pshufb             xmm1, xmm3
    movq   [dstq+strideq*0], xmm0
    movq   [dstq+strideq*1], xmm1
    movhps [dstq+strideq*2], xmm0
    movhps [dstq+stride3q ], xmm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
.w16:
    movsldup             m1, [base+smooth_shuf]
.w16_loop:
    vpbroadcastd         m0, [tlq+hq-4]
    pshufb               m0, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16
    RET
.w32:
    vpbroadcastd        ym3, [base+pb_1]
    vpord                m2, m3, [base+pb_2] {1to16}
.w32_loop:
    vpbroadcastd         m1, [tlq+hq-4]
    pshufb               m0, m1, m2
    pshufb               m1, m3
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+stride3q ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w32_loop
    RET
.w64:
    vpbroadcastd         m4, [base+pb_3]
    vpbroadcastd         m5, [base+pb_2]
    vpbroadcastd         m6, [base+pb_1]
    pxor                 m7, m7
.w64_loop:
    vpbroadcastd         m3, [tlq+hq-4]
    pshufb               m0, m3, m4
    pshufb               m1, m3, m5
    pshufb               m2, m3, m6
    pshufb               m3, m7
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    mova   [dstq+strideq*2], m2
    mova   [dstq+stride3q ], m3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w64_loop
    RET

%macro PAETH 0
    psubusb              m1, m5, m4
    psubusb              m0, m4, m5
    por                  m1, m0           ; tdiff
    pavgb                m2, m6, m4
    vpcmpub              k1, m1, m7, 1    ; tdiff < ldiff
    vpblendmb        m0{k1}, m4, m6
    vpternlogd           m4, m6, m8, 0x28 ; (m4 ^ m6) & m8
    psubusb              m3, m5, m2
    psubb                m2, m4
    psubusb              m2, m5
    por                  m2, m3
    pminub               m1, m7
    paddusb              m2, m2
    por                  m2, m4           ; min(tldiff, 255)
    vpcmpub              k1, m2, m1, 1    ; tldiff < ldiff && tldiff < tdiff
    vmovdqu8         m0{k1}, m5
%endmacro

cglobal ipred_paeth_8bpc, 3, 7, 10, dst, stride, tl, w, h, top, stride3
    lea                  r6, [ipred_paeth_8bpc_avx512icl_table]
    tzcnt                wd, wm
    vpbroadcastb         m5, [tlq] ; topleft
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    vpbroadcastd         m8, [r6-ipred_paeth_8bpc_avx512icl_table+pb_1]
    lea                topq, [tlq+1]
    sub                 tlq, hq
    add                  wq, r6
    lea            stride3q, [strideq*3]
    jmp                  wq
INIT_YMM avx512icl
.w4:
    vpbroadcastd         m6, [topq]
    mova                 m9, [ipred_h_shuf]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0 ; ldiff
.w4_loop:
    vpbroadcastq         m4, [tlq+hq-8]
    pshufb               m4, m9 ; left
    PAETH
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+stride3q ], xm0, 3
    sub                  hd, 8
    jl .w4_ret
    vextracti32x4       xm0, m0, 1
    lea                dstq, [dstq+strideq*4]
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+stride3q ], xm0, 3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_ret:
    RET
INIT_ZMM avx512icl
.w8:
    vpbroadcastq         m6, [topq]
    movsldup             m9, [smooth_shuf]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w8_loop:
    vpbroadcastq         m4, [tlq+hq-8]
    pshufb               m4, m9
    PAETH
    vextracti32x4       xm1, m0, 2
    vextracti32x4       xm2, ym0, 1
    vextracti32x4       xm3, m0, 3
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movq   [dstq+strideq*2], xm2
    movq   [dstq+stride3q ], xm3
    sub                  hd, 8
    jl .w8_ret
    lea                dstq, [dstq+strideq*4]
    movhps [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm2
    movhps [dstq+stride3q ], xm3
    lea                dstq, [dstq+strideq*4]
    jg .w8_loop
.w8_ret:
    RET
.w16:
    vbroadcasti32x4      m6, [topq]
    movsldup             m9, [smooth_shuf]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w16_loop:
    vpbroadcastd         m4, [tlq+hq-4]
    pshufb               m4, m9
    PAETH
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    vbroadcasti32x8      m6, [topq]
    mova                ym9, ym8
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w32_loop:
    vpbroadcastd         m4, [tlq+hq-2]
    pshufb               m4, m9
    PAETH
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    movu                 m6, [topq]
    psubusb              m7, m5, m6
    psubusb              m0, m6, m5
    por                  m7, m0
.w64_loop:
    vpbroadcastb         m4, [tlq+hq-1]
    PAETH
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET

cglobal ipred_smooth_v_8bpc, 3, 7, 7, dst, stride, tl, w, h, weights, stride3
%define base r6-ipred_smooth_v_8bpc_avx512icl_table
    lea                  r6, [ipred_smooth_v_8bpc_avx512icl_table]
    tzcnt                wd, wm
    mov                  hd, hm
    movsxd               wq, [r6+wq*4]
    vpbroadcastd         m0, [base+pb_127_m127]
    vpbroadcastd         m1, [base+pw_128]
    lea            weightsq, [base+smooth_weights+hq*4]
    neg                  hq
    vpbroadcastb         m4, [tlq+hq] ; bottom
    add                  wq, r6
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vpbroadcastd         m2, [tlq+1]
    movshdup             m5, [smooth_shuf]
    mova                ym6, [smooth_endA]
    punpcklbw            m2, m4 ; top, bottom
    pmaddubsw            m3, m2, m0
    paddw                m1, m2 ;   1 * top + 256 * bottom + 128, overflow is ok
    paddw                m3, m1 ; 128 * top + 129 * bottom + 128
.w4_loop:
    vbroadcasti32x4      m0, [weightsq+hq*2]
    pshufb               m0, m5
    pmaddubsw            m0, m2, m0
    paddw                m0, m3
    vpermb               m0, m6, m0
    vextracti32x4       xm1, ym0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+stride3q ], xm1, 2
    add                  hq, 8
    jg .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 1
    pextrd [dstq+strideq*1], xm1, 1
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+stride3q ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    jl .w4_loop
.ret:
    RET
.w8:
    vpbroadcastq         m2, [tlq+1]
    movshdup             m5, [smooth_shuf]
    mova                ym6, [smooth_endA]
    punpcklbw            m2, m4
    pmaddubsw            m3, m2, m0
    paddw                m1, m2
    paddw                m3, m1
.w8_loop:
    vpbroadcastq         m0, [weightsq+hq*2]
    pshufb               m0, m5
    pmaddubsw            m0, m2, m0
    paddw                m0, m3
    vpermb               m0, m6, m0
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w8_loop
    RET
.w16:
    vbroadcasti32x4      m3, [tlq+1]
    movshdup             m6, [smooth_shuf]
    mova                 m7, [smooth_endB]
    punpcklbw            m2, m3, m4
    punpckhbw            m3, m4
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
.w16_loop:
    vpbroadcastq         m1, [weightsq+hq*2]
    pshufb               m1, m6
    pmaddubsw            m0, m2, m1
    pmaddubsw            m1, m3, m1
    paddw                m0, m4
    paddw                m1, m5
    vpermt2b             m0, m7, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    add                  hq, 4
    jl .w16_loop
    RET
.w32:
    vbroadcasti32x8      m3, [tlq+1]
    movshdup             m6, [smooth_shuf]
    mova                 m7, [smooth_endB]
    punpcklbw            m2, m3, m4
    punpckhbw            m3, m4
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
.w32_loop:
    vpbroadcastd         m1, [weightsq+hq*2]
    pshufb               m1, m6
    pmaddubsw            m0, m2, m1
    pmaddubsw            m1, m3, m1
    paddw                m0, m4
    paddw                m1, m5
    vpermt2b             m0, m7, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    add                  hq, 2
    jl .w32_loop
    RET
.w64:
    movu                 m3, [tlq+1]
    mova                 m6, [smooth_endB]
    punpcklbw            m2, m3, m4
    punpckhbw            m3, m4
    pmaddubsw            m4, m2, m0
    pmaddubsw            m5, m3, m0
    paddw                m0, m1, m2
    paddw                m1, m3
    paddw                m4, m0
    paddw                m5, m1
.w64_loop:
    vpbroadcastw         m1, [weightsq+hq*2]
    pmaddubsw            m0, m2, m1
    pmaddubsw            m1, m3, m1
    paddw                m0, m4
    paddw                m1, m5
    vpermt2b             m0, m6, m1
    mova             [dstq], m0
    add                dstq, strideq
    inc                  hq
    jl .w64_loop
    RET

cglobal ipred_smooth_h_8bpc, 4, 7, 11, dst, stride, tl, w, h, stride3
%define base r5-ipred_smooth_h_8bpc_avx512icl_table
    lea                  r5, [ipred_smooth_h_8bpc_avx512icl_table]
    mov                 r6d, wd
    tzcnt                wd, wd
    vpbroadcastb         m4, [tlq+r6] ; right
    mov                  hd, hm
    movsxd               wq, [r5+wq*4]
    vpbroadcastd         m5, [base+pb_127_m127]
    vpbroadcastd         m6, [base+pw_128]
    sub                 tlq, hq
    add                  wq, r5
    vpmovb2m             k1, m6
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    movsldup             m3, [smooth_shuf]
    vpbroadcastq         m7, [smooth_weights+4*2]
    mova                ym8, [smooth_endA]
.w4_loop:
    vpbroadcastq         m0, [tlq+hq-8]
    mova                 m2, m4
    vpshufb          m2{k1}, m0, m3 ; left, right
    pmaddubsw            m0, m2, m5
    pmaddubsw            m1, m2, m7
    paddw                m2, m6
    paddw                m0, m2
    paddw                m0, m1
    vpermb               m0, m8, m0
    vextracti32x4       xm1, ym0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+stride3q ], xm1, 2
    sub                  hd, 8
    jl .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 1
    pextrd [dstq+strideq*1], xm1, 1
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+stride3q ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.ret:
    RET
.w8:
    movsldup             m3, [smooth_shuf]
    vbroadcasti32x4      m7, [smooth_weights+8*2]
    mova                ym8, [smooth_endA]
.w8_loop:
    vpbroadcastd         m0, [tlq+hq-4]
    mova                 m2, m4
    vpshufb          m2{k1}, m0, m3
    pmaddubsw            m0, m2, m5
    pmaddubsw            m1, m2, m7
    paddw                m2, m6
    paddw                m0, m2
    paddw                m0, m1
    vpermb               m0, m8, m0
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
.w16:
    movsldup             m7, [smooth_shuf]
    vbroadcasti32x4      m8, [smooth_weights+16*2]
    vbroadcasti32x4      m9, [smooth_weights+16*3]
    mova                m10, [smooth_endB]
.w16_loop:
    vpbroadcastd         m0, [tlq+hq-4]
    mova                 m3, m4
    vpshufb          m3{k1}, m0, m7
    pmaddubsw            m2, m3, m5
    pmaddubsw            m0, m3, m8
    pmaddubsw            m1, m3, m9
    paddw                m3, m6
    paddw                m2, m3
    paddw                m0, m2
    paddw                m1, m2
    vpermt2b             m0, m10, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    mova                m10, [smooth_endA]
    vpbroadcastd        ym7, [pb_1]
    vbroadcasti32x8      m8, [smooth_weights+32*2]
    vbroadcasti32x8      m9, [smooth_weights+32*3]
    vshufi32x4          m10, m10, q3120
.w32_loop:
    vpbroadcastd         m0, [tlq+hq-2]
    mova                 m3, m4
    vpshufb          m3{k1}, m0, m7
    pmaddubsw            m2, m3, m5
    pmaddubsw            m0, m3, m8
    pmaddubsw            m1, m3, m9
    paddw                m3, m6
    paddw                m2, m3
    paddw                m0, m2
    paddw                m1, m2
    vpermt2b             m0, m10, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    mova                 m7, [smooth_weights+64*2]
    mova                 m8, [smooth_weights+64*3]
    mova                 m9, [smooth_endA]
.w64_loop:
    mova                 m3, m4
    vpbroadcastb     m3{k1}, [tlq+hq-1]
    pmaddubsw            m2, m3, m5
    pmaddubsw            m0, m3, m7
    pmaddubsw            m1, m3, m8
    paddw                m3, m6
    paddw                m2, m3
    paddw                m0, m2
    paddw                m1, m2
    vpermt2b             m0, m9, m1
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET

cglobal ipred_smooth_8bpc, 4, 7, 16, dst, stride, tl, w, h, v_weights, stride3
%define base r5-ipred_smooth_8bpc_avx512icl_table
    lea                  r5, [ipred_smooth_8bpc_avx512icl_table]
    mov                 r6d, wd
    tzcnt                wd, wd
    mov                  hd, hm
    vpbroadcastb         m6, [tlq+r6] ; right
    sub                 tlq, hq
    movsxd               wq, [r5+wq*4]
    vpbroadcastd         m7, [base+pb_127_m127]
    vpbroadcastb         m0, [tlq]    ; bottom
    vpbroadcastd         m1, [base+pw_255]
    add                  wq, r5
    lea          v_weightsq, [base+smooth_weights+hq*2]
    vpmovb2m             k1, m1
    lea            stride3q, [strideq*3]
    jmp                  wq
.w4:
    vpbroadcastd         m8, [tlq+hq+1]
    movsldup             m4, [smooth_shuf]
    movshdup             m5, [smooth_shuf]
    vpbroadcastq         m9, [smooth_weights+4*2]
    mova               ym11, [smooth_endA]

    punpcklbw            m8, m0     ; top, bottom
    pmaddubsw           m10, m8, m7
    paddw                m1, m8     ;   1 * top + 256 * bottom + 255
    paddw               m10, m1     ; 128 * top + 129 * bottom + 255
.w4_loop:
    vpbroadcastq         m1, [tlq+hq-8]
    vbroadcasti32x4      m0, [v_weightsq]
    add          v_weightsq, 16
    mova                 m2, m6
    vpshufb          m2{k1}, m1, m4 ; left, right
    pmaddubsw            m1, m2, m7 ; 127 * left - 127 * right
    pshufb               m0, m5
    pmaddubsw            m0, m8, m0
    paddw                m1, m2     ; 128 * left + 129 * right
    pmaddubsw            m2, m9
    paddw                m0, m10
    paddw                m1, m2
    pavgw                m0, m1
    vpermb               m0, m11, m0
    vextracti32x4       xm1, ym0, 1
    movd   [dstq+strideq*0], xm0
    movd   [dstq+strideq*1], xm1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+stride3q ], xm1, 2
    sub                  hd, 8
    jl .ret
    lea                dstq, [dstq+strideq*4]
    pextrd [dstq+strideq*0], xm0, 1
    pextrd [dstq+strideq*1], xm1, 1
    pextrd [dstq+strideq*2], xm0, 3
    pextrd [dstq+stride3q ], xm1, 3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.ret:
    RET
.w8:
    vpbroadcastq         m8, [tlq+hq+1]
    movsldup             m4, [smooth_shuf]
    movshdup             m5, [smooth_shuf]
    vbroadcasti32x4      m9, [smooth_weights+8*2]
    mova               ym11, [smooth_endA]
    punpcklbw            m8, m0
    pmaddubsw           m10, m8, m7
    paddw                m1, m8
    paddw               m10, m1
.w8_loop:
    vpbroadcastd         m1, [tlq+hq-4]
    vpbroadcastq         m0, [v_weightsq]
    add          v_weightsq, 8
    mova                 m2, m6
    vpshufb          m2{k1}, m1, m4
    pmaddubsw            m1, m2, m7
    pshufb               m0, m5
    pmaddubsw            m0, m8, m0
    paddw                m1, m2
    pmaddubsw            m2, m9
    paddw                m0, m10
    paddw                m1, m2
    pavgw                m0, m1
    vpermb               m0, m11, m0
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movq   [dstq+strideq*1], xm1
    movhps [dstq+strideq*2], xm0
    movhps [dstq+stride3q ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
.w16:
    vbroadcasti32x4      m9, [tlq+hq+1]
    movsldup             m5, [smooth_shuf]
    movshdup            m10, [smooth_shuf]
    vbroadcasti32x4     m11, [smooth_weights+16*2]
    vbroadcasti32x4     m12, [smooth_weights+16*3]
    mova                m15, [smooth_endB]
    punpcklbw            m8, m9, m0
    punpckhbw            m9, m0
    pmaddubsw           m13, m8, m7
    pmaddubsw           m14, m9, m7
    paddw                m0, m1, m8
    paddw                m1, m9
    paddw               m13, m0
    paddw               m14, m1
.w16_loop:
    vpbroadcastd         m0, [tlq+hq-4]
    vpbroadcastq         m1, [v_weightsq]
    add          v_weightsq, 8
    mova                 m4, m6
    vpshufb          m4{k1}, m0, m5
    pmaddubsw            m2, m4, m7
    pshufb               m1, m10
    pmaddubsw            m0, m8, m1
    pmaddubsw            m1, m9, m1
    paddw                m2, m4
    pmaddubsw            m3, m4, m11
    pmaddubsw            m4, m12
    paddw                m0, m13
    paddw                m1, m14
    paddw                m3, m2
    paddw                m4, m2
    pavgw                m0, m3
    pavgw                m1, m4
    vpermt2b             m0, m15, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], m0, 2
    vextracti32x4 [dstq+strideq*2], ym0, 1
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    vbroadcasti32x8      m9, [tlq+hq+1]
    movshdup            m10, [smooth_shuf]
    mova                m12, [smooth_weights+32*2]
    vpbroadcastd        ym5, [pb_1]
    mova                m15, [smooth_endB]
    punpcklbw            m8, m9, m0
    punpckhbw            m9, m0
    pmaddubsw           m13, m8, m7
    pmaddubsw           m14, m9, m7
    vshufi32x4          m11, m12, m12, q2020
    vshufi32x4          m12, m12, q3131
    paddw                m0, m1, m8
    paddw                m1, m9
    paddw               m13, m0
    paddw               m14, m1
.w32_loop:
    vpbroadcastd         m0, [tlq+hq-2]
    vpbroadcastd         m1, [v_weightsq]
    add          v_weightsq, 4
    mova                 m4, m6
    vpshufb          m4{k1}, m0, m5
    pmaddubsw            m2, m4, m7
    pshufb               m1, m10
    pmaddubsw            m0, m8, m1
    pmaddubsw            m1, m9, m1
    paddw                m2, m4
    pmaddubsw            m3, m4, m11
    pmaddubsw            m4, m12
    paddw                m0, m13
    paddw                m1, m14
    paddw                m3, m2
    paddw                m4, m2
    pavgw                m0, m3
    pavgw                m1, m4
    vpermt2b             m0, m15, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    movu                 m9, [tlq+hq+1]
    mova                m11, [smooth_weights+64*2]
    mova                 m2, [smooth_weights+64*3]
    mova                m14, [smooth_endB]
    punpcklbw            m8, m9, m0
    punpckhbw            m9, m0
    pmaddubsw           m12, m8, m7
    pmaddubsw           m13, m9, m7
    vshufi32x4          m10, m11, m2, q2020
    vshufi32x4          m11, m2, q3131
    paddw                m0, m1, m8
    paddw                m1, m9
    paddw               m12, m0
    paddw               m13, m1
.w64_loop:
    mova                 m4, m6
    vpbroadcastb     m4{k1}, [tlq+hq-1]
    vpbroadcastw         m1, [v_weightsq]
    add          v_weightsq, 2
    pmaddubsw            m2, m4, m7
    pmaddubsw            m0, m8, m1
    pmaddubsw            m1, m9, m1
    paddw                m2, m4
    pmaddubsw            m3, m4, m10
    pmaddubsw            m4, m11
    paddw                m0, m12
    paddw                m1, m13
    paddw                m3, m2
    paddw                m4, m2
    pavgw                m0, m3
    pavgw                m1, m4
    vpermt2b             m0, m14, m1
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET

cglobal pal_pred_8bpc, 4, 7, 6, dst, stride, pal, idx, w, h, stride3
    movifnidn            wd, wm
    movifnidn            hd, hm
    lea            stride3q, [strideq*3]
    cmp                  wd, 8
    jg .w32
    movq               xmm3, [palq]
    je .w8
.w4:
    movq               xmm0, [idxq]
    add                idxq, 8
    psrlw              xmm1, xmm0, 4
    punpcklbw          xmm0, xmm1
    pshufb             xmm0, xmm3, xmm0
    movd   [dstq+strideq*0], xmm0
    pextrd [dstq+strideq*1], xmm0, 1
    pextrd [dstq+strideq*2], xmm0, 2
    pextrd [dstq+stride3q ], xmm0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w4
    RET
.w8:
    movu               xmm2, [idxq]
    add                idxq, 16
    pshufb             xmm1, xmm3, xmm2
    psrlw              xmm2, 4
    pshufb             xmm2, xmm3, xmm2
    punpcklbw          xmm0, xmm1, xmm2
    punpckhbw          xmm1, xmm2
    movq   [dstq+strideq*0], xmm0
    movhps [dstq+strideq*1], xmm0
    movq   [dstq+strideq*2], xmm1
    movhps [dstq+stride3q ], xmm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8
    RET
.w16:
    pmovzxdq             m0, [idxq]
    add                idxq, 32
    vpmultishiftqb       m0, m3, m0
    pshufb               m0, m5, m0
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+stride3q ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16
    RET
.w32:
    vpbroadcastq         m3, [pal_unpack+0]
    vpbroadcastq         m5, [palq]
    cmp                  wd, 32
    jl .w16
    pmovzxbd             m2, [pal_perm]
    vpbroadcastq         m4, [pal_unpack+8]
    jg .w64
.w32_loop:
    vpermd               m1, m2, [idxq]
    add                idxq, 64
    vpmultishiftqb       m0, m3, m1
    vpmultishiftqb       m1, m4, m1
    pshufb               m0, m5, m0
    pshufb               m1, m5, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    mova          [dstq+strideq*2], ym1
    vextracti32x8 [dstq+stride3q ], m1, 1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w32_loop
    RET
.w64:
    vpermd               m1, m2, [idxq]
    add                idxq, 64
    vpmultishiftqb       m0, m3, m1
    vpmultishiftqb       m1, m4, m1
    pshufb               m0, m5, m0
    pshufb               m1, m5, m1
    mova   [dstq+strideq*0], m0
    mova   [dstq+strideq*1], m1
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w64
    RET

%if WIN64
    DECLARE_REG_TMP 4
%else
    DECLARE_REG_TMP 8
%endif

cglobal ipred_z1_8bpc, 3, 8, 16, dst, stride, tl, w, h, angle, dx
%define base r7-z_filter_t0
    lea                  r7, [z_filter_t0]
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                  t0, [dr_intra_derivative]
    movsxd               wq, [base+ipred_z1_8bpc_avx512icl_table+wq*4]
    inc                 tlq
    mov                 dxd, angled
    and                 dxd, 0x7e
    add              angled, 165 ; ~90
    movzx               dxd, word [t0+dxq]
    lea                  wq, [base+ipred_z1_8bpc_avx512icl_table+wq]
    movifnidn            hd, hm
    xor              angled, 0x4ff ; d = 90 - angle
    mova                m14, [base+z_frac_table]
    vpbroadcastd        m15, [base+pw_512]
    jmp                  wq
.w4:
    mova                 m9, [pb_0to63]
    pminud               m8, m9, [base+pb_7] {1to16}
    vpbroadcastq         m7, [tlq]
    pshufb               m7, m8
    cmp              angleb, 40
    jae .w4_no_upsample
    lea                 r3d, [angleq-1024]
    sar                 r3d, 7
    add                 r3d, hd
    jg .w4_no_upsample ; !enable_intra_edge_filter || h > 8 || (h == 8 && is_sm)
    pshufb             xmm0, xm7, [base+z_filter_s4]
    mova               xmm1, [tlq-1]
    pshufb             xmm1, [base+z_xpos_off2a]
    vpbroadcastd       xmm2, [base+pb_m4_36]
    vpbroadcastq         m4, [pb_0to63]
    pmaddubsw          xmm0, xmm2
    pmaddubsw          xmm1, xmm2
    add                 dxd, dxd
    kxnorw               k1, k1, k1
    paddw              xmm0, xmm1
    pmulhrsw            xm0, xmm0, xm15
    packuswb            xm0, xm0
    punpcklbw       ym7{k1}, ym0
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
    vbroadcasti32x4     ym0, [tlq-1]
    pshufb              ym0, [base+z_filter4_s1]
    popcnt              r5d, r5d ; filter_strength
    pshufb              ym1, ym7, [z_filter_s4]
    pshufb              ym7, [base+z_filter_s3]
    vpbroadcastd       ym11, [base+z_filter_k+(r5-1)*4+12*0]
    vpbroadcastd       ym12, [base+z_filter_k+(r5-1)*4+12*1]
    pmaddubsw           ym0, ym11
    pmaddubsw           ym1, ym11
    pmaddubsw           ym7, ym12
    paddw               ym0, ym1
    paddw               ym7, ym0
    pmulhrsw            ym7, ym15
    cmp                  hd, 4
    je .w4_filter_end
    vpbroadcastd         m8, [base+pb_9]
    pminub               m8, m9
.w4_filter_end:
    paddb                m8, m8
    vpermb               m7, m8, m7
.w4_main:
    vpbroadcastq         m4, [base+z_xpos_off1a]
.w4_main2:
    movsldup             m2, [base+z_xpos_mul]
    vpbroadcastw         m5, dxd
    vbroadcasti32x4      m3, [base+z_xpos_bc]
    lea                  r2, [strideq*3]
    pmullw               m2, m5      ; xpos
    psllw                m5, 5       ; dx*8
.w4_loop:
    psrlw                m1, m2, 3
    pshufb               m0, m2, m3
    vpermw               m1, m1, m14 ; 64-frac, frac
    paddsb               m0, m4      ; base, base+1
    vpermb               m0, m0, m7  ; top[base], top[base+1]
    paddsw               m2, m5      ; xpos += dx
    pmaddubsw            m0, m1      ; v
    pmulhrsw             m0, m15
    packuswb             m0, m0
    vextracti32x4       xm1, ym0, 1
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    movd   [dstq+strideq*2], xm1
    pextrd [dstq+r2       ], xm1, 1
    sub                  hd, 8
    jl .w4_end
    vextracti32x4       xm1, m0, 2 ; top[max_base_x]
    lea                dstq, [dstq+strideq*4]
    vextracti32x4       xm0, m0, 3
    movd   [dstq+strideq*0], xm1
    pextrd [dstq+strideq*1], xm1, 1
    movd   [dstq+strideq*2], xm0
    pextrd [dstq+r2       ], xm0, 1
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.w8_filter:
    mova                ym0, [base+z_filter_s1]
    popcnt              r5d, r5d
    vbroadcasti32x4     ym1, [base+z_filter_s2]
    vbroadcasti32x4     ym3, [base+z_filter_s3]
    vbroadcasti32x4     ym4, [base+z_filter_s4]
    vpermi2b            ym0, ym7, ym2 ; al bl
    mova                ym5, [base+z_filter_s5]
    pshufb              ym1, ym7, ym1 ; ah bh
    vpbroadcastd       ym11, [base+z_filter_k+(r5-1)*4+12*0]
    pshufb              ym3, ym7, ym3 ; cl ch
    vpbroadcastd       ym12, [base+z_filter_k+(r5-1)*4+12*1]
    pshufb              ym4, ym7, ym4 ; el dl
    vpbroadcastd       ym13, [base+z_filter_k+(r5-1)*4+12*2]
    vpermb              ym5, ym5, ym7 ; eh dh
    pmaddubsw           ym0, ym11
    pmaddubsw           ym1, ym11
    pmaddubsw           ym2, ym3, ym12
    pmaddubsw           ym3, ym13
    pmaddubsw           ym4, ym11
    pmaddubsw           ym5, ym11
    paddw               ym0, ym2
    paddw               ym1, ym3
    paddw               ym0, ym4
    paddw               ym1, ym5
    pmulhrsw            ym0, ym15
    pmulhrsw            ym1, ym15
    packuswb            ym0, ym1
    ret
.w8:
    lea                 r3d, [angleq+216]
    mov                 r3b, hb
    cmp                 r3d, 8
    ja .w8_no_upsample ; !enable_intra_edge_filter || is_sm || d >= 40 || h > 8
    lea                 r3d, [hq-1]
    mova                xm1, [base+z_filter_s4]
    vpbroadcastb        xm2, r3d
    mova                xm7, [tlq-1]
    vinserti32x4        ym7, [tlq+7], 1
    vbroadcasti32x4     ym0, [base+z_xpos_off1a]
    vpbroadcastd        ym3, [base+pb_m4_36]
    pminub              xm2, xm1
    pshufb              ym0, ym7, ym0
    vinserti32x4        ym1, xm2, 1
    psrldq              ym7, 1
    pshufb              ym1, ym7, ym1
    pmaddubsw           ym0, ym3
    pmaddubsw           ym1, ym3
    vbroadcasti32x4      m8, [pb_0to63]
    add                 dxd, dxd
    paddw               ym0, ym1
    pmulhrsw            ym0, ym15
    packuswb            ym0, ym0
    punpcklbw           ym7, ym0
    jmp .w8_main2
.w8_no_upsample:
    lea                 r3d, [hq+7]
    mova                 m9, [pb_0to63]
    vpbroadcastb        ym0, r3d
    and                 r3d, 7
    vbroadcasti32x4      m7, [tlq]
    or                  r3d, 8 ; imin(h+7, 15)
    vpbroadcastb         m8, r3d
    pminub               m8, m9
    pshufb               m7, m8
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
    vpbroadcastd        ym2, [tlq-4]
    call .w8_filter
    cmp                  hd, 8
    jle .w8_filter_end
    vpbroadcastd         m8, [base+pb_17]
    add                 r3d, 2
    pminub               m8, m9
.w8_filter_end:
    vpermb               m7, m8, m0
.w8_main:
    vbroadcasti32x4      m8, [base+z_xpos_off1a]
.w8_main2:
    movsldup             m4, [base+z_xpos_mul]
    vpbroadcastw         m9, dxd
    shl                 r3d, 6
    vpbroadcastd         m5, [base+z_xpos_bc+8*0]
    pmullw               m4, m9 ; xpos
    vpbroadcastd         m6, [base+z_xpos_bc+8*1]
    sub                 r3d, dxd
    shl                 dxd, 3
    psllw                m9, 5 ; dx*8
    lea                  r2, [strideq*3]
.w8_loop:
    psrlw                m3, m4, 3
    pshufb               m0, m4, m5
    pshufb               m1, m4, m6
    vpermw               m3, m3, m14
    paddsb               m0, m8
    paddsb               m1, m8
    vpermb               m0, m0, m7
    vpermb               m1, m1, m7
    paddsw               m4, m9
    punpcklqdq           m2, m3, m3
    pmaddubsw            m0, m2
    punpckhqdq           m3, m3
    pmaddubsw            m1, m3
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m0, m1
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r2       ], xm1
    sub                  hd, 8
    jl .w8_end
    vextracti32x8       ym0, m0, 1
    lea                dstq, [dstq+strideq*4]
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r2       ], xm1
    jz .w8_end
    lea                dstq, [dstq+strideq*4]
    sub                 r3d, dxd
    jg .w8_loop
    vextracti32x4       xm7, m7, 3
.w8_end_loop:
    movq   [dstq+strideq*0], xm7
    movq   [dstq+strideq*1], xm7
    movq   [dstq+strideq*2], xm7
    movq   [dstq+r2       ], xm7
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_end_loop
.w8_end:
    RET
.w16_filter:
    mova                 m0, [base+z_filter_s1]
    popcnt              r5d, r5d
    vbroadcasti32x4      m1, [base+z_filter_s2]
    vbroadcasti32x4      m3, [base+z_filter_s3]
    vbroadcasti32x4      m4, [base+z_filter_s4]
    vpermi2b             m0, m7, m2 ; al bl
    mova                 m5, [base+z_filter_s5]
    pshufb               m1, m7, m1 ; ah bh
    vpbroadcastd        m11, [base+z_filter_k+(r5-1)*4+12*0]
    pshufb               m3, m7, m3 ; cl ch
    vpbroadcastd        m12, [base+z_filter_k+(r5-1)*4+12*1]
    pshufb               m4, m7, m4 ; el dl
    vpbroadcastd        m13, [base+z_filter_k+(r5-1)*4+12*2]
    vpermb               m5, m5, m7 ; eh dh
    pmaddubsw            m0, m11
    pmaddubsw            m1, m11
    pmaddubsw            m2, m3, m12
    pmaddubsw            m3, m13
    pmaddubsw            m4, m11
    pmaddubsw            m5, m11
    paddw                m0, m2
    paddw                m1, m3
    paddw                m0, m4
    paddw                m1, m5
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m0, m1
    ret
.w16:
    lea                 r3d, [hq+15]
    mova                 m9, [pb_0to63]
    vpbroadcastb        ym0, r3d
    and                 r3d, 15
    movu                ym7, [tlq]
    or                  r3d, 16 ; imin(h+15, 31)
    vpbroadcastb         m8, r3d
    pminub               m8, m9
    vpermb               m7, m8, m7
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
    vpbroadcastd         m2, [tlq-4]
    call .w16_filter
    cmp                  hd, 16
    jle .w16_filter_end
    vpbroadcastd         m8, [base+pb_33]
    add                 r3d, 2
    pminub               m8, m9
.w16_filter_end:
    vpermb               m7, m8, m0
.w16_main:
    movshdup             m3, [base+z_xpos_mul]
    vpbroadcastw         m8, dxd
    shl                 r3d, 6
    vpbroadcastd         m4, [base+z_xpos_bc]
    pmullw               m3, m8 ; xpos
    vbroadcasti32x4      m5, [base+z_xpos_off1a]
    sub                 r3d, dxd
    shl                 dxd, 2
    vbroadcasti32x4      m6, [base+z_xpos_off1b]
    psllw                m8, 4 ; dx*4
    lea                  r2, [strideq*3]
.w16_loop:
    pshufb               m1, m3, m4
    psrlw                m2, m3, 3
    paddsb               m0, m1, m5
    vpermw               m2, m2, m14
    paddsb               m1, m6
    vpermb               m0, m0, m7
    vpermb               m1, m1, m7
    paddsw               m3, m8
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m0, m1
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+r2       ], m0, 3
    sub                  hd, 4
    jz .w16_end
    lea                dstq, [dstq+strideq*4]
    sub                 r3d, dxd
    jg .w16_loop
    vextracti32x4       xm7, m7, 3
.w16_end_loop:
    mova   [dstq+strideq*0], xm7
    mova   [dstq+strideq*1], xm7
    mova   [dstq+strideq*2], xm7
    mova   [dstq+r2       ], xm7
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_end_loop
.w16_end:
    RET
.w32_filter:
    mova                 m0, [base+z_filter_s1]
    vbroadcasti32x4      m1, [base+z_filter_s2]
    vbroadcasti32x4      m3, [base+z_filter_s3]
    vbroadcasti32x4      m4, [base+z_filter_s4]
    vpermi2b             m0, m7, m2 ; al bl
    mova                 m5, [base+z_filter_s5]
    pshufb               m1, m7, m1 ; ah bh
    vpbroadcastd        m11, [base+z_filter_k+4*2+12*0]
    pshufb               m3, m7, m3 ; cl ch
    vpbroadcastd        m12, [base+z_filter_k+4*2+12*1]
    pshufb               m4, m7, m4 ; el dl
    vpbroadcastd        m13, [base+z_filter_k+4*2+12*2]
    vpermi2b             m5, m7, m8 ; eh dh
    pmaddubsw            m0, m11
    pmaddubsw            m1, m11
    pmaddubsw            m2, m3, m12
    pmaddubsw            m3, m13
    pmaddubsw            m4, m11
    pmaddubsw            m5, m11
    paddw                m0, m2
    paddw                m1, m3
    paddw                m0, m4
    paddw                m1, m5
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m7, m0, m1
    ret
.w32:
    lea                 r3d, [hq+31]
    vpbroadcastb         m9, r3d
    and                 r3d, 31
    pminub              m10, m9, [pb_0to63]
    or                  r3d, 32 ; imin(h+31, 63)
    vpermb               m7, m10, [tlq]
    vpbroadcastb         m8, [tlq+r3]
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w32_main
    vpbroadcastd         m2, [tlq-4]
    call .w32_filter
    cmp                  hd, 64
    je .w32_h64_filter_end
    vpermb               m8, m9, m7
    vpermb               m7, m10, m7
    jmp .w32_main
.w32_h64_filter_end: ; edge case for 32x64
    movd               xmm0, [tlq+r3-1]
    movd               xmm1, [base+pb_8_56_0_0]
    add                 r3d, 2
    pmaddubsw          xmm0, xmm1
    vptestmw             k1, xmm1, xmm1 ; 0x01
    pmulhrsw            xm0, xmm0, xm15
    vmovdqu8         m8{k1}, m0
.w32_main:
    rorx                r2d, dxd, 30
    vpbroadcastd         m4, [base+z_xpos_bc]
    vpbroadcastw         m3, r2d
    vbroadcasti32x8      m5, [base+z_xpos_off2a]
    shl                 r3d, 6
    vbroadcasti32x8      m6, [base+z_xpos_off2b]
    sub                 r3d, dxd
    paddw                m9, m3, m3
    add                 dxd, dxd
    vinserti32x8         m3, ym9, 1
.w32_loop:
    pshufb               m1, m3, m4
    psrlw                m2, m3, 3
    paddsb               m0, m1, m5
    vpermw               m2, m2, m14
    paddsb               m1, m6
    vpermi2b             m0, m7, m8
    vpermi2b             m1, m7, m8
    paddsw               m3, m9
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m0, m1
    mova          [dstq+strideq*0], ym0
    vextracti32x8 [dstq+strideq*1], m0, 1
    sub                  hd, 2
    jz .w32_end
    lea                dstq, [dstq+strideq*2]
    sub                 r3d, dxd
    jg .w32_loop
    punpckhqdq          ym8, ym8
.w32_end_loop:
    mova   [dstq+strideq*0], ym8
    mova   [dstq+strideq*1], ym8
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_end_loop
.w32_end:
    RET
.w64_filter:
    vbroadcasti32x4      m3, [base+z_filter_s2]
    mova                 m1, [base+z_filter_s1]
    pshufb               m0, m3      ; al bl
    vpermi2b             m1, m7, m2
    vbroadcasti32x4      m4, [base+z_filter_s4]
    pshufb               m6, m8, m4  ; el dl
    pshufb               m9, m7, m4
    pminub              m10, m13, [base+z_filter_s5]
    pshufb               m2, m8, m3  ; ah bh
    pshufb               m3, m7, m3
    vbroadcasti32x4      m5, [base+z_filter_s3]
    vpermb              m10, m10, m8 ; eh dh
    pshufb              m11, m4
    vpbroadcastd         m4, [base+z_filter_k+4*2+12*0]
    pshufb               m8, m5      ; cl ch
    pshufb               m7, m5
    vpbroadcastd         m5, [base+z_filter_k+4*2+12*1]
    REPX  {pmaddubsw x, m4}, m0, m1, m6, m9, m2, m3, m10, m11
    pmaddubsw            m4, m8, m5
    pmaddubsw            m5, m7, m5
    paddw                m0, m6
    vpbroadcastd         m6, [base+z_filter_k+4*2+12*2]
    paddw                m1, m9
    pmaddubsw            m7, m6
    pmaddubsw            m8, m6
    paddw                m2, m10
    paddw                m3, m11
    paddw                m0, m4
    paddw                m1, m5
    paddw                m2, m8
    paddw                m3, m7
    REPX  {pmulhrsw x, m15}, m0, m2, m1, m3
    packuswb             m0, m2
    packuswb             m7, m1, m3
    vpermb               m8, m12, m0
    ret
.w64:
    lea                 r3d, [hq-1]
    movu                 m7, [tlq+64*0]
    vpbroadcastb        m13, r3d
    pminub              m12, m13, [pb_0to63]
    or                  r3d, 64
    vpermb               m8, m12, [tlq+64*1]
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w64_main
    movu                 m0, [tlq+56]
    vpbroadcastd         m2, [tlq-4]
    movu                m11, [tlq+8]
    call .w64_filter
.w64_main:
    rorx                r2d, dxd, 30
    vpbroadcastd         m4, [base+z_xpos_bc]
    vpbroadcastw         m3, r2d
    mova                 m5, [base+z_xpos_off2a]
    shl                 r3d, 6
    mova                 m6, [base+z_xpos_off2b]
    sub                 r3d, dxd
    mova                 m9, m3
.w64_loop:
    pshufb               m1, m3, m4
    psrlw                m2, m3, 3
    paddsb               m0, m1, m5
    vpermw               m2, m2, m14
    paddsb               m1, m6
    vpermi2b             m0, m7, m8
    vpermi2b             m1, m7, m8
    paddsw               m3, m9
    pmaddubsw            m0, m2
    pmaddubsw            m1, m2
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m0, m1
    mova             [dstq], m0
    dec                  hd
    jz .w64_end
    add                dstq, strideq
    sub                 r3d, dxd
    jg .w64_loop
    vpermb               m8, m13, m8
.w64_end_loop:
    mova             [dstq], m8
    add                dstq, strideq
    dec                  hd
    jg .w64_end_loop
.w64_end:
    RET

cglobal ipred_z2_8bpc, 3, 9, 18, dst, stride, tl, w, h, angle, dx, _, dy
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                 dxq, [dr_intra_derivative-90]
    movzx               dyd, angleb
    xor              angled, 0x400
    mov                  r7, dxq
    sub                 dxq, dyq
    movifnidn            hd, hm
    and                 dyd, ~1
    and                 dxq, ~1
    movzx               dyd, word [r7+dyq]  ; angle - 90
    lea                  r7, [z_filter_t0]
    movzx               dxd, word [dxq+270] ; 180 - angle
    movsxd               wq, [base+ipred_z2_8bpc_avx512icl_table+wq*4]
    mova                 m8, [base+pb_63to0]
    neg                 dyd
    vpermb               m8, m8, [tlq-64] ; left
    lea                  wq, [base+ipred_z2_8bpc_avx512icl_table+wq]
    mova                m14, [base+z_frac_table]
    inc                 tlq
    vpbroadcastd        m15, [base+pw_512]
    neg                 dxd
    jmp                  wq
.w4:
    movd                xm7, [tlq]
    vpbroadcastq        m10, [base+z_xpos_off2a]
    test             angled, 0x400
    jnz .w4_main ; !enable_intra_edge_filter
    lea                 r3d, [hq+2]
    add              angled, 1022
    shl                 r3d, 6
    test                r3d, angled
    jnz .w4_no_upsample_above ; angle >= 130 || h > 8 || (is_sm && h == 8)
    vpbroadcastd        xm2, [base+pb_4]
    sub              angled, 1075 ; angle - 53
    call .upsample_above
    lea                 r3d, [hq+3]
    vpbroadcastq        m10, [pb_0to63+1]
    punpcklbw           xm7, xm0, xm7
    call .filter_strength
    jmp .w4_filter_left
.w4_upsample_left:
    call .upsample_left
    movsldup            m16, [base+z_ypos_off3]
    vpbroadcastd         m9, [base+pb_16]
    punpcklbw           xm8, xm0, xm8
    jmp .w4_main2
.w4_no_upsample_above:
    lea                 r3d, [hq+3]
    sub              angled, 1112 ; angle - 90
    call .filter_strength
    test                r3d, r3d
    jz .w4_no_filter_above
    vpbroadcastd        xm5, [base+pb_3]
    call .filter_top_w16
.w4_no_filter_above:
    lea                 r3d, [hq+2]
    add              angled, 973 ; angle + 883
    shl                 r3d, 6
    test                r3d, angled
    jz .w4_upsample_left ; angle <= 140 || h > 8 || (is_sm && h == 8)
    vpbroadcastd        ym0, [base+pb_90]
    psubb               ym0, ym17
    vpcmpgtb         k2{k2}, ym0, ym16
    kmovd               r3d, k2
.w4_filter_left:
    test                r3d, r3d
    jz .w4_main
    popcnt              r3d, r3d
    call .filter_left_h16
.w4_main:
    movsldup            m16, [base+z_ypos_off1]
    vpbroadcastd         m9, [base+pb_8]
.w4_main2:
    vpbroadcastq         m3, [base+z_ypos_mul1a]
    vpbroadcastw         m0, dyd
    movsldup             m1, [base+z_xpos_mul]
    vpbroadcastw         m5, dxd
    vinserti32x4         m7, [tlq-16], 3
    vinserti32x4         m8, [tlq-16], 3
    pmullw               m3, m0
    vbroadcasti32x4      m2, [base+z_xpos_bc]
    pmullw               m1, m5      ; xpos0..3
    psllw                m5, 5       ; dx*8
    psraw                m4, m3, 6
    psrlw                m3, 1
    packsswb             m4, m4
    vpermw               m3, m3, m14 ; 64-frac, frac
    punpcklbw            m4, m4
    lea                  r2, [strideq*3]
    paddb                m4, m16     ; base, base+1
.w4_loop:
    pshufb              m16, m1, m2
    psrlw                m0, m1, 3
    paddb               m16, m10
    vpermw               m0, m0, m14
    vpmovw2m             k1, m16     ; base_x < 0
    vpermb              m16, m16, m7
    pmaddubsw           m16, m0
    vpermb               m0, m4, m8
    pmaddubsw       m16{k1}, m0, m3
    pmulhrsw            m16, m15
    vpmovwb            ym16, m16
    movd   [dstq+strideq*0], xm16
    pextrd [dstq+strideq*1], xm16, 1
    pextrd [dstq+strideq*2], xm16, 2
    pextrd [dstq+r2       ], xm16, 3
    sub                  hd, 8
    jl .w4_end
    paddsw               m1, m5
    vextracti128       xm16, ym16, 1
    lea                dstq, [dstq+strideq*4]
    paddb                m4, m9
    movd   [dstq+strideq*0], xm16
    pextrd [dstq+strideq*1], xm16, 1
    pextrd [dstq+strideq*2], xm16, 2
    pextrd [dstq+r2       ], xm16, 3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.upsample_above: ; w4/w8
    mova                xm0, [tlq-1]
    xor              angled, 0x7f ; 180 - angle
    add                 dxd, dxd
    jmp .upsample
.upsample_left: ; h4/h8
    palignr             xm0, xm8, [tlq-16], 15
    vpbroadcastb        xm2, hd
    add                 dyd, dyd
.upsample:
    pshufb              xm1, xm0, [base+z_filter4_s1]
    pminub              xm2, [base+z_filter_s4]
    vpbroadcastd        xm3, [base+pb_m4_36]
    pshufb              xm0, xm2
    pmaddubsw           xm1, xm3
    pmaddubsw           xm0, xm3
    paddw               xm0, xm1
    pmulhrsw            xm0, xm15
    packuswb            xm0, xm0
    ret
.filter_strength:
    vpbroadcastb       ym16, r3d
    mov                 r3d, angled
    vpbroadcastd         m2, [tlq-4]
    vpbroadcastb       ym17, angled
    shr                 r3d, 8
    vpcmpeqb             k2, ym16, [base+z_filter_wh]
    mova               xm16, [base+z_filter_t0+r3*8]
    vpcmpgtb         k1{k2}, ym17, ym16
    mova                 m9, [pb_0to63]
    kmovd               r3d, k1
    ret
.w8:
    movq                xm7, [tlq]
    vbroadcasti32x4     m10, [base+z_xpos_off2a]
    test             angled, 0x400
    jnz .w8_main
    lea                 r3d, [angleq+126]
    mov                 r3b, hb
    cmp                 r3d, 8
    ja .w8_no_upsample_above ; angle >= 130 || h > 8 || is_sm
    vpbroadcastd        xm2, [base+pb_8]
    sub              angled, 53 ; angle - 53
    call .upsample_above
    lea                 r3d, [hq+7]
    vbroadcasti32x4     m10, [pb_0to63+1]
    punpcklbw           xm7, xm0, xm7
    call .filter_strength
    jmp .w8_filter_left
.w8_upsample_left:
    call .upsample_left
    movshdup            m16, [base+z_ypos_off3]
    vpbroadcastd         m9, [base+pb_8]
    punpcklbw           xm8, xm0, xm8
    jmp .w8_main2
.w8_no_upsample_above:
    lea                 r3d, [hq+7]
    sub              angled, 90 ; angle - 90
    call .filter_strength
    test                r3d, r3d
    jz .w8_no_filter_above
    vpbroadcastd        xm5, [base+pb_7]
    call .filter_top_w16
.w8_no_filter_above:
    lea                 r3d, [angleq-51]
    mov                 r3b, hb
    cmp                 r3d, 8
    jbe .w8_upsample_left ; angle > 140 && h <= 8 && !is_sm
    vpbroadcastd        ym0, [base+pb_90]
    psubb               ym0, ym17
    vpcmpgtb         k2{k2}, ym0, ym16
    kmovd               r3d, k2
.w8_filter_left:
    test                r3d, r3d
    jz .w8_main
    cmp                  hd, 32
    je .w8_filter_left_h32
    popcnt              r3d, r3d
    call .filter_left_h16
    jmp .w8_main
.w8_filter_left_h32:
    call .filter_left_h64
.w8_main:
    movshdup            m16, [base+z_ypos_off2]
    vpbroadcastd         m9, [base+pb_4]
.w8_main2:
    vbroadcasti32x4      m3, [base+z_ypos_mul1a]
    vpbroadcastw         m0, dyd
    movshdup             m1, [base+z_xpos_mul]
    vpbroadcastw         m5, dxd
    vinserti32x4         m7, [tlq-16], 3
    vinserti32x4         m8, [tlq-16], 3
    pmullw               m3, m0
    vpbroadcastd         m2, [base+pb_1]
    pmullw               m1, m5      ; xpos0..3
    psllw                m5, 4       ; dx*4
    psraw                m4, m3, 6
    psrlw                m3, 1
    packsswb             m4, m4
    vpermw               m3, m3, m14 ; 64-frac, frac
    lea                 r3d, [dxq+(8<<6)]
    paddsb               m4, m16
    shl                 dxd, 2
    paddsb               m0, m4, m2
    lea                  r2, [strideq*3]
    punpcklbw            m4, m0      ; base, base+1
.w8_loop:
    pshufb              m16, m1, m2
    psrlw                m0, m1, 3
    paddb               m16, m10
    vpermw               m0, m0, m14
    vpmovw2m             k1, m16     ; base_x < 0
    vpermb              m16, m16, m7
    pmaddubsw           m16, m0
    vpermb               m0, m4, m8
    pmaddubsw       m16{k1}, m0, m3
    pmulhrsw            m16, m15
    vpmovwb            ym16, m16
    vextracti128       xm17, ym16, 1
    movq   [dstq+strideq*0], xm16
    movhps [dstq+strideq*1], xm16
    movq   [dstq+strideq*2], xm17
    movhps [dstq+r2       ], xm17
    sub                  hd, 4
    jz .w8_end
    paddw                m1, m5
    lea                dstq, [dstq+strideq*4]
    paddb                m4, m9
    add                 r3d, dxd
    jge .w8_loop
.w8_leftonly_loop:
    vpermb              m16, m4, m8
    pmaddubsw           m16, m3
    paddb                m4, m9
    pmulhrsw            m16, m15
    vpmovwb            ym16, m16
    vextracti128       xm17, ym16, 1
    movq   [dstq+strideq*0], xm16
    movhps [dstq+strideq*1], xm16
    movq   [dstq+strideq*2], xm17
    movhps [dstq+r2       ], xm17
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_leftonly_loop
.w8_end:
    RET
.filter_top_w16:
    mova                xm0, [base+z_filter_s1]
    popcnt              r3d, r3d
    pminub              xm4, xm5, [base+z_filter_s4]
    vpermi2b            xm0, xm7, xm2
    pminub              xm5, [base+z_filter_s5]
    pshufb              xm1, xm7, [base+z_filter_s2]
    vpbroadcastd       xm11, [base+z_filter_k+(r3-1)*4+12*0]
    pshufb              xm3, xm7, [base+z_filter_s3]
    vpbroadcastd       xm12, [base+z_filter_k+(r3-1)*4+12*1]
    pshufb              xm4, xm7, xm4
    vpbroadcastd       xm13, [base+z_filter_k+(r3-1)*4+12*2]
    pshufb              xm5, xm7, xm5
    pmaddubsw           xm0, xm11
    pmaddubsw           xm1, xm11
    pmaddubsw           xm6, xm3, xm12
    vpbroadcastd       xm12, r7m ; max_width
    pmaddubsw           xm3, xm13
    pmaddubsw           xm4, xm11
    pmaddubsw           xm5, xm11
    packssdw           xm12, xm12
    paddw               xm0, xm6
    paddw               xm1, xm3
    paddw               xm0, xm4
    paddw               xm1, xm5
    packsswb           xm12, xm12
    pmulhrsw            xm0, xm15
    pmulhrsw            xm1, xm15
    vpcmpgtb             k1, xm12, xm9 ; x < max_width
    packuswb        xm7{k1}, xm0, xm1
    ret
.filter_left_h16:
    lea                 r5d, [hq-1]
    mova                xm0, [base+z_filter_s1]
    vpbroadcastb        xm5, r5d
    vpermi2b            xm0, xm8, xm2
    pminub              xm4, xm5, [base+z_filter_s4]
    pshufb              xm1, xm8, [base+z_filter_s2]
    pminub              xm5, [base+z_filter_s5]
    pshufb              xm3, xm8, [base+z_filter_s3]
    vpbroadcastd       xm11, [base+z_filter_k+(r3-1)*4+12*0]
    pshufb              xm4, xm8, xm4
    vpbroadcastd       xm12, [base+z_filter_k+(r3-1)*4+12*1]
    pshufb              xm5, xm8, xm5
    vpbroadcastd       xm13, [base+z_filter_k+(r3-1)*4+12*2]
    pmaddubsw           xm0, xm11
    pmaddubsw           xm1, xm11
    pmaddubsw           xm6, xm3, xm12
    vpbroadcastd       xm12, r8m ; max_height
    pmaddubsw           xm3, xm13
    pmaddubsw           xm4, xm11
    pmaddubsw           xm5, xm11
    packssdw           xm12, xm12
    paddw               xm0, xm6
    paddw               xm1, xm3
    paddw               xm0, xm4
    paddw               xm1, xm5
    packsswb           xm12, xm12
    pmulhrsw            xm0, xm15
    pmulhrsw            xm1, xm15
    vpcmpgtb             k1, xm12, xm9 ; y < max_height
    packuswb        xm8{k1}, xm0, xm1
    ret
.w16:
    movu                xm7, [tlq] ; top
    test             angled, 0x400
    jnz .w16_main
    lea                 r3d, [hq+15]
    sub              angled, 90
    call .filter_strength
    test                r3d, r3d
    jz .w16_no_filter_above
    vpbroadcastd        xm5, [base+pb_15]
    call .filter_top_w16
.w16_no_filter_above:
    cmp                  hd, 16
    jg .w16_filter_left_h64
    vpbroadcastd        ym0, [base+pb_90]
    psubb               ym0, ym17
    vpcmpgtb         k2{k2}, ym0, ym16
    kmovd               r3d, k2
    test                r3d, r3d
    jz .w16_main
    popcnt              r3d, r3d
    call .filter_left_h16
    jmp .w16_main
.w16_filter_left_h64:
    call .filter_left_h64
.w16_main:
    vbroadcasti32x4      m6, [base+z_ypos_mul1a] ; 1.. 8
    vbroadcasti32x4      m5, [base+z_ypos_mul1b] ; 9..15
    vpbroadcastw         m0, dyd
    vinserti32x4         m7, [tlq-16], 3
    vpbroadcastd         m2, [base+pb_1]
    vpbroadcastw        m12, dxd
    movshdup             m1, [base+z_xpos_mul]
    pmullw               m6, m0
    vbroadcasti32x4      m3, [base+z_xpos_off2a]
    pmullw               m5, m0
    vbroadcasti32x4      m4, [base+z_xpos_off2b]
    pmullw               m1, m12      ; xpos0 xpos1 xpos2 xpos3
    vpbroadcastd         m9, [base+pb_4]
    psllw               m12, 4        ; dx*4
    movshdup            m16, [base+z_ypos_off2]
    psrlw               m10, m6, 1
    psrlw               m11, m5, 1
    vpermw              m10, m10, m14 ; 64-frac, frac
    psraw                m6, 6
    vpermw              m11, m11, m14
    psraw                m5, 6
    mov                 r5d, -(16<<6) ; 15 to avoid top, +1 to avoid topleft
    packsswb             m6, m5
    mov                 r3d, 1<<6
    paddsb               m6, m16
    sub                 r5d, dxd      ; left-only threshold
    paddsb               m0, m6, m2
    shl                 dxd, 2
    punpcklbw            m5, m6, m0   ; base, base+1
    lea                  r2, [strideq*3]
    punpckhbw            m6, m0
.w16_loop:
    pshufb              m17, m1, m2
    psrlw                m0, m1, 3
    paddb               m16, m3, m17
    vpermw               m0, m0, m14
    paddb               m17, m4
    vpmovw2m             k1, m16
    vpermb              m16, m16, m7
    vpmovw2m             k2, m17
    vpermb              m17, m17, m7
    pmaddubsw           m16, m0
    pmaddubsw           m17, m0
    add                 r3d, dxd
    jge .w16_toponly
    mova                 m0, m8
    vpermt2b             m0, m5, m7
    pmaddubsw       m16{k1}, m0, m10
    mova                 m0, m8
    vpermt2b             m0, m6, m7
    pmaddubsw       m17{k2}, m0, m11
.w16_toponly:
    pmulhrsw            m16, m15
    pmulhrsw            m17, m15
    packuswb            m16, m17
    mova          [dstq+strideq*0], xm16
    vextracti128  [dstq+strideq*1], ym16, 1
    vextracti32x4 [dstq+strideq*2], m16, 2
    vextracti32x4 [dstq+r2       ], m16, 3
    sub                  hd, 4
    jz .w16_end
    paddw                m1, m12
    lea                dstq, [dstq+strideq*4]
    paddb                m5, m9
    paddb                m6, m9
    cmp                 r3d, r5d
    jge .w16_loop
.w16_leftonly_loop:
    vpermb              m16, m5, m8
    vpermb              m17, m6, m8
    pmaddubsw           m16, m10
    pmaddubsw           m17, m11
    paddb                m5, m9
    paddb                m6, m9
    pmulhrsw            m16, m15
    pmulhrsw            m17, m15
    packuswb            m16, m17
    mova          [dstq+strideq*0], xm16
    vextracti128  [dstq+strideq*1], ym16, 1
    vextracti32x4 [dstq+strideq*2], m16, 2
    vextracti32x4 [dstq+r2       ], m16, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_leftonly_loop
.w16_end:
    RET
.w32:
    movu                ym7, [tlq]
    test             angled, 0x400
    jnz .w32_main
    vpbroadcastd         m2, [tlq-4]
    mova                ym0, [base+z_filter_s1]
    vbroadcasti32x4     ym1, [base+z_filter_s2]
    vbroadcasti32x4     ym3, [base+z_filter_s3]
    vbroadcasti32x4     ym4, [base+z_filter_s4]
    vpermi2b            ym0, ym7, ym2 ; al bl
    vpbroadcastd        ym5, [base+pb_31]
    pminub              ym5, [base+z_filter_s5]
    pshufb              ym1, ym7, ym1 ; ah bh
    vpbroadcastd       ym11, [base+z_filter_k+4*2+12*0]
    pshufb              ym3, ym7, ym3 ; cl ch
    vpbroadcastd       ym12, [base+z_filter_k+4*2+12*1]
    pshufb              ym4, ym7, ym4 ; el dl
    vpbroadcastd       ym13, [base+z_filter_k+4*2+12*2]
    vpermb              ym5, ym5, ym7 ; eh dh
    pmaddubsw           ym0, ym11
    pmaddubsw           ym1, ym11
    pmaddubsw           ym6, ym3, ym12
    vpbroadcastd       ym12, r6m
    pmaddubsw           ym3, ym13
    pmaddubsw           ym4, ym11
    pmaddubsw           ym5, ym11
    mova                 m9, [pb_0to63]
    packssdw           ym12, ym12
    paddw               ym0, ym6
    paddw               ym1, ym3
    paddw               ym0, ym4
    paddw               ym1, ym5
    packsswb           ym12, ym12
    pmulhrsw            ym0, ym15
    pmulhrsw            ym1, ym15
    vpcmpgtb             k1, ym12, ym9 ; x < max_width
    packuswb        ym7{k1}, ym0, ym1
    cmp                  hd, 16
    jg .w32_filter_h64
    mov                 r3d, 3
    call .filter_left_h16
    jmp .w32_main
.w32_filter_h64:
    call .filter_left_h64
.w32_main:
    vbroadcasti32x8      m6, [base+z_ypos_mul1a] ; 1.. 8
    vbroadcasti32x8      m5, [base+z_ypos_mul1b] ; 9..15
    vpbroadcastw         m0, dyd
    vinserti32x4         m7, [tlq-16], 3
    rorx                r2q, dxq, 62 ; dx << 2
    vpbroadcastd         m2, [base+pb_1]
    vpbroadcastw         m1, r2d
    pmullw               m6, m0
    vbroadcasti32x8      m3, [base+z_xpos_off2a]
    pmullw               m5, m0
    vbroadcasti32x8      m4, [base+z_xpos_off2b]
    mova                ym0, ym1
    paddw               m12, m1, m1
    vpbroadcastd         m9, [base+pb_2]
    paddw                m1, m0       ; xpos1 xpos0
    mova                ym0, ym2
    psrlw               m10, m6, 1
    psrlw               m11, m5, 1
    vpermw              m10, m10, m14 ; 64-frac, frac
    psraw                m6, 6
    vpermw              m11, m11, m14
    psraw                m5, 6
    mov                 r5d, -(32<<6) ; 31 to avoid top, +1 to avoid topleft
    packsswb             m6, m5
    mov                 r3d, 1<<6
    paddsb               m6, m0
    sub                 r5d, dxd      ; left-only threshold
    paddsb               m0, m6, m2
    add                 dxd, dxd
    punpcklbw            m5, m6, m0   ; base, base+1
    punpckhbw            m6, m0
.w32_loop:
    pshufb              m17, m1, m2
    psrlw                m0, m1, 3
    paddb               m16, m3, m17
    vpermw               m0, m0, m14
    paddb               m17, m4
    vpmovw2m             k1, m16
    vpermb              m16, m16, m7
    vpmovw2m             k2, m17
    vpermb              m17, m17, m7
    pmaddubsw           m16, m0
    pmaddubsw           m17, m0
    add                 r3d, dxd
    jge .w32_toponly
    mova                 m0, m8
    vpermt2b             m0, m5, m7
    pmaddubsw       m16{k1}, m0, m10
    mova                 m0, m8
    vpermt2b             m0, m6, m7
    pmaddubsw       m17{k2}, m0, m11
.w32_toponly:
    pmulhrsw            m16, m15
    pmulhrsw            m17, m15
    packuswb            m16, m17
    vextracti32x8 [dstq+strideq*0], m16, 1
    mova          [dstq+strideq*1], ym16
    sub                  hd, 2
    jz .w32_end
    paddw                m1, m12
    lea                dstq, [dstq+strideq*2]
    paddb                m5, m9
    paddb                m6, m9
    cmp                 r3d, r5d
    jge .w32_loop
.w32_leftonly_loop:
    vpermb              m16, m5, m8
    vpermb              m17, m6, m8
    pmaddubsw           m16, m10
    pmaddubsw           m17, m11
    paddb                m5, m9
    paddb                m6, m9
    pmulhrsw            m16, m15
    pmulhrsw            m17, m15
    packuswb            m16, m17
    vextracti32x8 [dstq+strideq*0], m16, 1
    mova          [dstq+strideq*1], ym16
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_leftonly_loop
.w32_end:
    RET
.filter_left_h64:
    mova                 m0, [base+z_filter_s1]
    lea                 r3d, [hq-1]
    vbroadcasti32x4      m4, [base+z_filter_s4]
    vpbroadcastb         m5, r3d
    vbroadcasti32x4      m1, [base+z_filter_s2]
    vbroadcasti32x4      m3, [base+z_filter_s3]
    vpermi2b             m0, m8, m2 ; al bl
    pminub               m5, [base+z_filter_s5]
    pshufb               m1, m8, m1 ; ah bh
    vpbroadcastd        m11, [base+z_filter_k+4*2+12*0]
    pshufb               m3, m8, m3 ; cl ch
    vpbroadcastd        m12, [base+z_filter_k+4*2+12*1]
    pshufb               m4, m8, m4 ; el dl
    vpbroadcastd        m13, [base+z_filter_k+4*2+12*2]
    vpermb               m5, m5, m8 ; eh dh
    pmaddubsw            m0, m11
    pmaddubsw            m1, m11
    pmaddubsw            m6, m3, m12
    vpbroadcastd        m12, r8m    ; max_height
    pmaddubsw            m3, m13
    pmaddubsw            m4, m11
    pmaddubsw            m5, m11
    packssdw            m12, m12
    paddw                m0, m6
    paddw                m1, m3
    paddw                m0, m4
    paddw                m1, m5
    packsswb            m12, m12
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    vpcmpgtb             k1, m12, m9 ; y < max_height
    packuswb         m8{k1}, m0, m1
    ret
.w64:
    movu                 m7, [tlq]
    test             angled, 0x400
    jnz .w64_main
    vpbroadcastd         m2, [tlq-4]
    mova                 m0, [base+z_filter_s1]
    vbroadcasti32x4      m1, [base+z_filter_s2]
    vbroadcasti32x4      m3, [base+z_filter_s3]
    vbroadcasti32x4      m4, [base+z_filter_s4]
    vpermi2b             m0, m7, m2 ; al bl
    vpbroadcastd         m5, [base+pb_63]
    pminub               m5, [base+z_filter_s5]
    pshufb               m1, m7, m1 ; ah bh
    vpbroadcastd        m11, [base+z_filter_k+4*2+12*0]
    pshufb               m3, m7, m3 ; cl ch
    vpbroadcastd        m12, [base+z_filter_k+4*2+12*1]
    pshufb               m4, m7, m4 ; el dl
    vpbroadcastd        m13, [base+z_filter_k+4*2+12*2]
    vpermb               m5, m5, m7 ; eh dh
    pmaddubsw            m0, m11
    pmaddubsw            m1, m11
    pmaddubsw            m6, m3, m12
    vpbroadcastd        m12, r6m
    pmaddubsw            m3, m13
    pmaddubsw            m4, m11
    pmaddubsw            m5, m11
    mova                 m9, [pb_0to63]
    packssdw            m12, m12
    paddw                m0, m6
    paddw                m1, m3
    paddw                m0, m4
    paddw                m1, m5
    packsswb            m12, m12
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    vpcmpgtb             k1, m12, m9 ; x < max_width
    packuswb         m7{k1}, m0, m1
    call .filter_left_h64 ; always filter the full 64 pixels for simplicity
.w64_main:
    vpbroadcastw         m5, dyd
    vpbroadcastd         m9, [tlq-4]
    rorx                r2q, dxq, 62 ; dx << 2
    pmullw               m6, m5, [base+z_ypos_mul1a] ; can overflow, but it doesn't matter as such
    pmullw               m5, [base+z_ypos_mul1b]     ; pixels aren't selected from the left edge
    vpbroadcastw         m1, r2d     ; xpos
    mova                 m3, [base+z_xpos_off2a]
    mova                 m4, [base+z_xpos_off2b]
    mova                m12, m1
    vpbroadcastd         m2, [base+pb_1]
    psrlw               m10, m6, 1
    psrlw               m11, m5, 1
    vpermw              m10, m10, m14 ; 64-frac, frac
    psraw                m6, 6
    vpermw              m11, m11, m14
    psraw                m5, 6
    mov                 r5d, -(64<<6) ; 63 to avoid top, +1 to avoid topleft
    packsswb             m6, m5
    mov                 r3d, 1<<6
    paddsb               m0, m6, m2
    sub                 r5d, dxd      ; left-only threshold
    punpcklbw            m5, m6, m0   ; base, base+1
    punpckhbw            m6, m0
.w64_loop:
    pshufb              m17, m1, m2
    psrlw                m0, m1, 3
    paddb               m16, m3, m17
    vpermw               m0, m0, m14
    paddb               m17, m4
    vpmovw2m             k1, m16      ; base_x < 0
    vpermi2b            m16, m7, m9
    vpmovw2m             k2, m17
    vpermi2b            m17, m7, m9
    pmaddubsw           m16, m0
    pmaddubsw           m17, m0
    add                 r3d, dxd
    jge .w64_toponly
    mova                 m0, m8
    vpermt2b             m0, m5, m9
    pmaddubsw       m16{k1}, m0, m10
    mova                 m0, m8
    vpermt2b             m0, m6, m9
    pmaddubsw       m17{k2}, m0, m11
.w64_toponly:
    pmulhrsw            m16, m15
    pmulhrsw            m17, m15
    packuswb            m16, m17
    mova             [dstq], m16
    dec                  hd
    jz .w64_end
    paddw                m1, m12
    add                dstq, strideq
    paddb                m5, m2
    paddb                m6, m2
    cmp                 r3d, r5d
    jge .w64_loop
.w64_leftonly_loop:
    vpermb              m16, m5, m8
    vpermb              m17, m6, m8
    pmaddubsw           m16, m10
    pmaddubsw           m17, m11
    paddb                m5, m2
    paddb                m6, m2
    pmulhrsw            m16, m15
    pmulhrsw            m17, m15
    packuswb            m16, m17
    mova             [dstq], m16
    add                dstq, strideq
    dec                  hd
    jg .w64_leftonly_loop
.w64_end:
    RET

cglobal ipred_z3_8bpc, 3, 8, 16, dst, stride, tl, w, h, angle, dy
    lea                  r7, [z_filter_t0]
    tzcnt                wd, wm
    movifnidn        angled, anglem
    lea                  t0, [dr_intra_derivative+45*2-1]
    movsxd               wq, [base+ipred_z3_8bpc_avx512icl_table+wq*4]
    sub              angled, 180
    mov                 dyd, angled
    neg                 dyd
    xor              angled, 0x400
    or                  dyq, ~0x7e
    mova                 m0, [base+pb_63to0]
    movzx               dyd, word [t0+dyq]
    lea                  wq, [base+ipred_z3_8bpc_avx512icl_table+wq]
    movifnidn            hd, hm
    mova                m14, [base+z_frac_table]
    shl                 dyd, 6
    vpbroadcastd        m15, [base+pw_512]
    jmp                  wq
.w4:
    cmp              angleb, 40
    jae .w4_no_upsample
    lea                 r3d, [angleq-1024]
    sar                 r3d, 7
    add                 r3d, hd
    jg .w4_no_upsample ; !enable_intra_edge_filter || h > 8 || (h == 8 && is_sm)
    lea                 r3d, [hq+4]
    call .upsample
    movshdup             m1, [base+z_ypos_off1]
    vpbroadcastd         m6, [base+pb_16]
    jmp .w4_main2
.w4_no_upsample:
    lea                 r3d, [hq+3]
    vpbroadcastb         m9, r3d
    vpxord               m1, m9, [base+pb_63] {1to16} ; 63 - (h + 4)
    pmaxub               m1, m0
    vpermb               m7, m1, [tlq-64*1]
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w4_main
    vpbroadcastb        xm1, angled
    shr              angled, 8
    vpcmpeqb             k1, xm9, [base+z_filter_wh]
    vpbroadcastd         m2, [tlq-3]
    vpcmpgtb         k1{k1}, xm1, [base+z_filter_t0+angleq*8]
    kmovw               r5d, k1
    test                r5d, r5d
    jz .w4_main
    pminub               m9, [pb_0to63]
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w8_filter
    vpermb               m7, m9, m0
.w4_main:
    movsldup             m1, [base+z_ypos_off1]
    vpbroadcastd         m6, [base+pb_8]
.w4_main2:
    vpbroadcastw         m0, dyd
    vpbroadcastq         m2, [base+z_ypos_mul2a] ; 1..4
    pmulhuw              m2, m0 ; ypos >> 1
    lea                  r2, [strideq*3]
    vpermw               m3, m2, m14 ; 64-frac, frac
    psrlw                m2, 5
    packsswb             m2, m2
    punpcklbw            m2, m2
    paddsb               m2, m1 ; base, base+1
.w4_loop:
    vpermb               m0, m2, m7
    pmaddubsw            m0, m3
    paddsb               m2, m6
    pmulhrsw             m0, m15
    vpmovwb             ym0, m0
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+r2       ], xm0, 3
    sub                  hd, 8
    jl .w4_end
    vextracti32x4       xm0, ym0, 1
    lea                dstq, [dstq+strideq*4]
    movd   [dstq+strideq*0], xm0
    pextrd [dstq+strideq*1], xm0, 1
    pextrd [dstq+strideq*2], xm0, 2
    pextrd [dstq+r2       ], xm0, 3
    lea                dstq, [dstq+strideq*4]
    jg .w4_loop
.w4_end:
    RET
.upsample:
    xor                 r3d, 31 ; 31 - (h + imin(w, h))
    vbroadcasti32x4     ym0, [base+z_xpos_off2a]
    vpbroadcastb        ym7, r3d
    pmaxub              ym7, [base+z3_upsample]
    vbroadcasti32x4     ym1, [base+z_filter_s4]
    vpermb              ym7, ym7, [tlq-31]
    vpbroadcastd        ym2, [base+pb_m4_36]
    pshufb              ym0, ym7, ym0
    psrldq              ym7, 1
    pshufb              ym1, ym7, ym1
    pmaddubsw           ym0, ym2
    pmaddubsw           ym1, ym2
    add                 dyd, dyd
    paddw               ym0, ym1
    pmulhrsw            ym0, ym15
    packuswb            ym0, ym0
    punpcklbw           ym7, ym0
    ret
.w8:
    lea                 r3d, [angleq+216]
    mov                 r3b, hb
    cmp                 r3d, 8
    ja .w8_no_upsample ; !enable_intra_edge_filter || is_sm || d >= 40 || h > 8
    lea                 r3d, [hq*2]
    call .upsample
    pshufd               m1, [base+z_ypos_off1], q0000
    vpbroadcastd         m6, [base+pb_8]
    jmp .w8_main2
.w8_no_upsample:
    mov                 r3d, 8
    cmp                  hd, 4
    cmove               r3d, hd
    lea                 r3d, [r3+hq-1]
    xor                 r3d, 63 ; 63 - (h + imin(w, h))
    vpbroadcastb         m1, wd
    pmaxub               m1, m0
    vpermb               m7, m1, [tlq-64*1]
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w8_main
    lea                 r3d, [hq+7]
    call .filter_strength
    test                r5d, r5d
    jz .w8_main
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w16_filter
    vpermb               m7, m10, m0
.w8_main:
    movsldup             m1, [base+z_ypos_off2]
    vpbroadcastd         m6, [base+pb_4]
.w8_main2:
    vpbroadcastw         m0, dyd
    vbroadcasti32x4      m2, [base+z_ypos_mul2a] ; 1..8
    pmulhuw              m2, m0 ; ypos >> 1
    lea                  r2, [strideq*3]
    vpermw               m3, m2, m14 ; 64-frac, frac
    psrlw                m2, 5
    packsswb             m2, m2
    punpcklbw            m2, m2
    paddsb               m2, m1 ; base, base+1
.w8_loop:
    vpermb               m0, m2, m7
    pmaddubsw            m0, m3
    paddsb               m2, m6
    pmulhrsw             m0, m15
    vpmovwb             ym0, m0
    vextracti32x4       xm1, ym0, 1
    movq   [dstq+strideq*0], xm0
    movhps [dstq+strideq*1], xm0
    movq   [dstq+strideq*2], xm1
    movhps [dstq+r2       ], xm1
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w8_loop
    RET
.filter_strength:
    vpbroadcastd         m2, [tlq-3]
.filter_strength2:
    vpbroadcastb         m9, r3d
    vpbroadcastb        ym1, angled
    shr              angled, 8
    vpcmpeqb             k1, ym9, [base+z_filter_wh]
    mova                xm0, [base+z_filter_t0+angleq*8]
    vpcmpgtb         k1{k1}, ym1, ym0
    pminub              m10, m9, [pb_0to63]
    kmovd               r5d, k1
    ret
.w16_load:
    cmp                 r3d, hd
    cmovae              r3d, hd
    add                 r3d, hd
    mova                 m7, [tlq-64*1]
    neg                 r3d ; -(h + imin(w, h))
    and                 r3d, 63
    vpbroadcastb         m1, r3d
    pmaxub               m2, m0, m1
    cmp                  hd, 64
    je .w16_load_h64
    vpermb               m8, m1, m7
    vpermb               m7, m2, m7
    ret
.w16_load_h64:
    vpermb               m7, m0, m7
    vpermb               m8, m2, [tlq-64*2]
    ret
.w16:
    mov                 r3d, 16
    call .w16_load
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w16_main
    vpbroadcastd         m2, [tlq-3]
    cmp                  hd, 64
    je .w16_filter64
    lea                 r3d, [hq+15]
    call .filter_strength2
    test                r5d, r5d
    jz .w16_main
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w16_filter
    pminub              m10, m9, [pb_0to63]
    vpermb               m8, m9, m0
    vpermb               m7, m10, m0
    jmp .w16_main
.w16_filter64:
    vpbroadcastd        m13, [base+pb_15]
    valignq              m0, m8, m7, 7
    pminub              m12, m13, [pb_0to63]
    valignq             m11, m8, m7, 1
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w64_filter
.w16_main:
    vbroadcasti32x4      m3, [base+z_ypos_mul2a] ; 1.. 8
    vbroadcasti32x4      m2, [base+z_ypos_mul2b] ; 9..15
    vpbroadcastw         m0, dyd
    vpbroadcastd         m6, [base+pb_4]
    pmulhuw              m3, m0 ; ypos >> 1
    pmulhuw              m2, m0
    movshdup             m0, [base+z_ypos_off2]
    lea                  r2, [strideq*3]
    vpbroadcastd         m1, [base+pb_1]
    vpermw               m4, m3, m14 ; 64-frac, frac
    psrlw                m3, 5
    vpermw               m5, m2, m14
    psrlw                m2, 5
    packsswb             m3, m2
    paddsb               m3, m0
    paddsb               m1, m3
    punpcklbw            m2, m3, m1 ; base, base+1
    punpckhbw            m3, m1
.w16_loop:
%macro Z3_PERM2 0
    mova                 m0, m7
    vpermt2b             m0, m2, m8
    mova                 m1, m7
    vpermt2b             m1, m3, m8
    pmaddubsw            m0, m4
    pmaddubsw            m1, m5
    paddsb               m2, m6
    paddsb               m3, m6
    pmulhrsw             m0, m15
    pmulhrsw             m1, m15
    packuswb             m0, m1
%endmacro
    Z3_PERM2
    mova          [dstq+strideq*0], xm0
    vextracti32x4 [dstq+strideq*1], ym0, 1
    vextracti32x4 [dstq+strideq*2], m0, 2
    vextracti32x4 [dstq+r2       ], m0, 3
    lea                dstq, [dstq+strideq*4]
    sub                  hd, 4
    jg .w16_loop
    RET
.w32:
    mov                  r3d, 32
    call .w16_load
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w32_main
    vpbroadcastd         m2, [tlq-3]
    cmp                  hd, 64
    je .w32_filter64
    lea                 r3d, [hq+31]
    vpbroadcastb         m9, r3d
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w32_filter
    vpermb               m8, m9, m7
    jmp .w32_main
.w32_filter64:
    vpbroadcastd        m13, [base+pb_31]
    valignq              m0, m8, m7, 7
    pminub              m12, m13, [pb_0to63]
    valignq             m11, m8, m7, 1
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w64_filter
.w32_main:
    vbroadcasti32x8      m3, [base+z_ypos_mul2a] ; 1.. 8
    vbroadcasti32x8      m2, [base+z_ypos_mul2b] ; 9..15
    vpbroadcastw         m0, dyd
    vpbroadcastd         m1, [base+pb_1]
    pmulhuw              m3, m0 ; ypos >> 1
    pmulhuw              m2, m0
    vpbroadcastd         m6, [base+pb_2]
    mova                ym0, ym1
    vpermw               m4, m3, m14 ; 64-frac, frac
    psrlw                m3, 5
    vpermw               m5, m2, m14
    psrlw                m2, 5
    packsswb             m3, m2
    paddsb               m3, m0
    paddsb               m1, m3
    punpcklbw            m2, m3, m1 ; base, base+1
    punpckhbw            m3, m1
.w32_loop:
    Z3_PERM2
    vextracti32x8 [dstq+strideq*0], m0, 1
    mova          [dstq+strideq*1], ym0
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w32_loop
    RET
.w64:
    mova                 m7, [tlq-64*1]
    cmp                  hd, 64
    je .w64_h64
    lea                 r3d, [hq*2-1]
    xor                 r3d, 63 ; -(h + imin(w, h)) & 63
    vpbroadcastb         m1, r3d
    pmaxub               m0, m1
    vpermb               m8, m1, m7
    jmp .w64_filter
.w64_h64:
    vpermb               m8, m0, [tlq-64*2]
.w64_filter:
    vpermb               m7, m0, m7
    test             angled, 0x400 ; !enable_intra_edge_filter
    jnz .w64_main
    lea                 r3d, [hq-1]
    vpbroadcastd         m2, [tlq-3]
    vpbroadcastb        m13, r3d
    valignq              m0, m8, m7, 7
    pminub              m12, m13, [pb_0to63]
    valignq             m11, m8, m7, 1
    call mangle(private_prefix %+ _ipred_z1_8bpc_avx512icl).w64_filter
.w64_main:
    vpbroadcastw         m2, dyd
    pmulhuw              m3, m2, [base+z_ypos_mul2a]
    pmulhuw              m2, [base+z_ypos_mul2b]
    vpbroadcastd         m6, [base+pb_1]
    vpermw               m4, m3, m14 ; 64-frac, frac
    psrlw                m3, 5
    vpermw               m5, m2, m14
    psrlw                m2, 5
    packsswb             m3, m2
    paddsb               m1, m3, m6
    punpcklbw            m2, m3, m1 ; base, base+1
    punpckhbw            m3, m1
.w64_loop:
    Z3_PERM2
    mova             [dstq], m0
    add                dstq, strideq
    dec                  hd
    jg .w64_loop
    RET

; The ipred_filter code processes 4x2 blocks in the following order
; which increases parallelism compared to doing things row by row.
; Some redundant blocks are calculated for w > 4.
;     w4     w8       w16             w32
;     1     1 2     1 2 3 4     1 2 3 4 9 a b c
;     2     2 3     2 3 4 5     2 3 4 5 a b c d
;     3     3 4     3 4 5 6     3 4 5 6 b c d e
;     4     4 5     4 5 6 7     4 5 6 7 c d e f
;     5     5 6     5 6 7 8     5 6 7 8 d e f g
;     6     6 7     6 7 8 9     6 7 8 9 e f g h
;     7     7 8     7 8 9 a     7 8 9 a f g h i
; ___ 8 ___ 8 9 ___ 8 9 a b ___ 8 9 a b g h i j ___
;           9       9 a b               h i j
;                   a b                 i j
;                   b                   j

cglobal ipred_filter_8bpc, 4, 7, 14, dst, stride, tl, w, h, flt
%define base r6-filter_taps
    lea                  r6, [filter_taps]
%ifidn fltd, fltm
    movzx              fltd, fltb
%else
    movzx              fltd, byte fltm
%endif
    vpbroadcastd       xmm2, [tlq+1]        ; t0 t0 t0 t0
    movifnidn            hd, hm
    shl                fltd, 6
    vpbroadcastd         m6, [base+pd_8]
    vpbroadcastd       xmm3, [tlq-2]        ; l1 l0 tl __
    vbroadcasti32x4      m7, [r6+fltq+16*0] ; p1 p2 p3 p4
    vbroadcasti32x4      m8, [r6+fltq+16*1]
    vbroadcasti32x4      m9, [r6+fltq+16*2] ; p6 p5 p0 __
    vbroadcasti32x4     m10, [r6+fltq+16*3]
    mova               xmm0, xm6
    vpdpbusd           xmm0, xmm2, xm7
    mova               xmm1, xm6
    vpdpbusd           xmm1, xmm2, xm8
    vpdpbusd           xmm0, xmm3, xm9
    vpdpbusd           xmm1, xmm3, xm10
    packssdw           xmm0, xmm1
    cmp                  wd, 8
    jb .w4
    vpbroadcastd        ym2, [tlq+5]
    mova                m11, [base+filter_perm]
    mov                  r5, 0xffffffffffff000f
    psrldq             xmm2, 1           ; __ t0
    kmovq                k1, r5          ; 0x000f
    psraw               xm5, xmm0, 4
    packuswb           xmm2, xm5         ; __ t0 a0 b0
    pshufd          ym2{k1}, ymm2, q3333 ; b0 b0 b0 b0   t1 t1 t1 t1
    je .w8
    kxnorb               k3, k3, k3      ; 0x00ff
    vpbroadcastd        xm3, [tlq-4]
    kandnq               k2, k3, k1      ; 0xffffffffffff0000
    vpermb          ym3{k2}, ym11, ymm2  ; l3 l2 l1 __   b3 a3 t3 __
    mova                ym0, ym6
    vpdpbusd            ym0, ym2, ym7
    mova                ym1, ym6
    vpdpbusd            ym1, ym2, ym8
    pshufb          ym5{k2}, ym2, ym11   ; a0 b0   __ t0
    vpbroadcastd         m2, [tlq+9]
    vpdpbusd            ym0, ym3, ym9
    vpdpbusd            ym1, ym3, ym10
    vpbroadcastd        xm3, [tlq-6]     ; l5 l4 l3 __
    kunpckbw             k4, k1, k3      ; 0x0fff
    packssdw            ym0, ym1
    psraw               ym0, 4           ; a0 d0         a1 b1
    packuswb            ym5, ym0         ; a0 b0 c0 d0   __ t1 a1 b1
    pshufd           m2{k3}, m5, q3333   ; d0 d0 d0 d0   b1 b1 b1 b1   t2 t2 t2 t2
    vpermb           m3{k2}, m11, m5     ; l5 l4 l3 __   d3 c3 b3 __   b7 a7 t7 __
    mova                 m4, m6
    vpdpbusd             m4, m2, m7
    mova                 m1, m6
    vpdpbusd             m1, m2, m8
    psrldq               m0, m2, 1       ; __ d0         __ b0         __ t0
    vpbroadcastd         m2, [tlq+13]
    vpdpbusd             m4, m3, m9
    vpdpbusd             m1, m3, m10
    mova                m12, [base+filter_end]
    lea                 r5d, [hq-6]
    mov                  r6, dstq
    cmovp                hd, r5d         ; w == 16 ? h : h - 6
    packssdw             m4, m1
    psraw                m4, 4           ; e0 f0         c1 d1         a2 b2
    packuswb             m0, m4          ; __ d0 e0 f0   __ b1 c1 d1   __ t2 a2 b2
    pshufd           m2{k4}, m0, q3333   ; f0 f0 f0 f0   d1 d1 d1 d1   b2 b2 b2 b2   t3 t3 t3 t3
.w16_loop:
    vpbroadcastd        xm3, [tlq-8]
    vpermb           m3{k2}, m11, m0     ; l7 l6 l5 __   f3 e3 d3 __   d7 c7 b7 __   bb ab tb __
    mova                 m1, m6
    vpdpbusd             m1, m2, m7
    mova                 m0, m6
    vpdpbusd             m0, m2, m8
    sub                 tlq, 2
    vpdpbusd             m1, m3, m9
    vpdpbusd             m0, m3, m10
    packssdw             m1, m0
    mova                 m0, m4
    psraw                m4, m1, 4       ; g0 h0         e1 f1         c2 d2         a3 b3
    packuswb             m0, m4          ; e0 f0 g0 h0   c1 d1 e1 f1   a2 b2 c2 d2   __ __ a3 b3
    pshufd               m2, m0, q3333   ; h0 h0 h0 h0   f1 f1 f1 f1   d2 d2 d2 d2   b3 b3 b3 b3
    vpermt2d             m5, m12, m0     ; c0 d0 e0 f0   __ __ c1 d1   a0 a1 a2 a3   b0 b1 b2 b3
    vextracti32x4 [dstq+strideq*0], m5, 2
    vextracti32x4 [dstq+strideq*1], m5, 3
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w16_loop
    cmp                  wd, 16
    je .ret
    mova               xm13, [filter_perm+16]
    mova               xmm3, [r6+strideq*0]
    punpckhdq          xmm3, [r6+strideq*1]
    vpbroadcastd     m2{k1}, [tlq+r5+17] ; t4 t4 t4 t4   f1 f1 f1 f1   d2 d2 d2 d2   b3 b3 b3 b3
    pinsrb              xm3, xmm3, [tlq+r5+16], 7
    pshufb              xm3, xm13
    vpermb           m3{k2}, m11, m0     ; bf af tf __   h3 g3 f3 __   f7 e7 d7 __   db cb bb __
    mova                 m0, m6
    vpdpbusd             m0, m2, m7
    mova                 m1, m6
    vpdpbusd             m1, m2, m8
    kunpckbw             k5, k3, k1      ; 0xff0f
    lea                  r3, [strideq*3]
    vpdpbusd             m0, m3, m9
    vpdpbusd             m1, m3, m10
    packssdw             m0, m1
    psraw                m0, 4           ; a4 b4         g1 h1         e2 f2         c3 d3
    packuswb             m4, m0          ; g0 h0 a4 b4   e1 f1 g1 h1   c2 d2 e2 f2   __ __ c3 d3
    vpblendmb        m1{k3}, m4, m2      ; __ t4 a4 b4   e1 f1 g1 h1   c2 d2 e2 f2   __ __ c3 d3
    vpbroadcastd        ym2, [tlq+r5+21]
    pshufd           m2{k5}, m4, q3333   ; b4 b4 b4 b4   t5 t5 t5 t5   f2 f2 f2 f2   d3 d3 d3 d3
    vpermt2d             m5, m12, m4     ; e0 f0 g0 h0   __ __ e1 f1   c0 c1 c2 c3   d0 d1 d2 d3
    vextracti32x4 [dstq+strideq*0], m5, 2
    vextracti32x4 [dstq+strideq*1], m5, 3
    punpckhqdq         xmm3, [r6+r3]
    pinsrb             xmm3, [r6+strideq*2+15], 11
    pshufb              xm3, xmm3, xm13
    vpermb           m3{k2}, m11, m1     ; df cf bf __   bj aj tj __   h7 g7 f7 __   fb eb db __
    mova                 m4, m6
    vpdpbusd             m4, m2, m7
    mova                 m1, m6
    vpdpbusd             m1, m2, m8
    kxnord               k3, k3, k4      ; 0xfffff0ff
    lea                  r4, [strideq*5]
    vpdpbusd             m4, m3, m9
    vpdpbusd             m1, m3, m10
    packssdw             m4, m1
    psraw                m4, 4           ; c4 d4         a5 b5         g2 h2         e3 f3
    packuswb             m0, m4          ; a4 b4 c4 d4   g1 h1 a5 b5   e2 f2 g2 h2   __ __ e3 f3
    vpblendmw        m1{k3}, m2, m0      ; a4 b4 c4 d4   __ t5 a5 b5   e2 f2 g2 h2   __ __ e3 f3
    vpbroadcastd         m2, [tlq+r5+25]
    pshufd           m2{k3}, m0, q3333   ; d4 d4 d4 d4   b5 b5 b5 b5   t6 t6 t6 t6   f3 f3 f3 f3
    vpermt2d             m5, m12, m0     ; g0 h0 a4 b4   __ __ g1 h1   e0 e1 e2 e3   f0 f1 f2 f3
    vextracti32x4 [dstq+strideq*2], m5, 2
    vextracti32x4 [dstq+r3       ], m5, 3
    punpckhqdq         xmm3, [r6+r4]
    pinsrb             xmm3, [r6+strideq*4+15], 11
    pshufb              xm3, xmm3, xm13
    vpermb           m3{k2}, m11, m1     ; ff ef df __   dj cj bj __   bn an tn __   hb hb fb __
    mova                 m0, m6
    vpdpbusd             m0, m2, m7
    mova                 m1, m6
    vpdpbusd             m1, m2, m8
    kunpckwd             k1, k1, k2      ; 0x000f0000
    vpdpbusd             m0, m3, m9
    vpdpbusd             m1, m3, m10
    packssdw             m0, m1
    psraw                m0, 4           ; e4 f4         c5 d5         a6 b6         g3 h3
    packuswb             m4, m0          ; c4 d4 e4 f4   a5 b5 c5 d5   g2 h2 a6 b6   __ __ g3 h3
    vpblendmw        m1{k1}, m4, m2      ; c4 d4 e4 f4   a5 b5 c5 d5   __ t6 a6 b6   __ __ g3 h3
    vpbroadcastd         m2, [tlq+r5+29]
    pshufd           m2{k4}, m4, q3333   ; f4 f4 f4 f4   d5 d5 d5 d5   b6 b6 b6 b6   t7 t7 t7 t7
    vpermt2d             m5, m12, m4     ; a4 b4 c4 d4   __ __ a5 b5   g0 g1 g2 g3   h0 h1 h2 h3
    vextracti32x4 [dstq+strideq*4], m5, 2
    vextracti32x4 [dstq+r4       ], m5, 3
    lea                  r0, [strideq+r3*2]
.w32_loop:
    punpckhqdq         xmm3, [r6+r0]
    pinsrb             xmm3, [r6+r3*2+15], 11
    pshufb              xm3, xmm3, xm13
    vpermb           m3{k2}, m11, m1     ; hf gf ff __   fj ej dj __   dn cn bn __   br ar tr __
.w32_loop_tail:
    mova                 m4, m6
    vpdpbusd             m4, m2, m7
    mova                 m1, m6
    vpdpbusd             m1, m2, m8
    vpdpbusd             m4, m3, m9
    vpdpbusd             m1, m3, m10
    packssdw             m4, m1
    mova                 m1, m0
    psraw                m0, m4, 4       ; g4 h4         e5 f5         c6 d6         a7 b7
    packuswb             m1, m0          ; e4 f4 g4 h4   c5 d5 e5 f5   a6 b6 c6 d6   __ __ a7 b7
    pshufd               m2, m1, q3333   ; h4 h4 h4 h4   f5 f5 f5 f5   d6 d6 d6 d6   b7 b7 b7 b7
    vpermt2d             m5, m12, m1     ; c4 d4 e4 f4   __ __ c5 d5   a4 a5 a6 a7   b4 b5 b6 b7
    vextracti32x4 [r6+strideq*0+16], m5, 2
    vextracti32x4 [r6+strideq*1+16], m5, 3
    lea                  r6, [r6+strideq*2]
    sub                 r5d, 2
    jg .w32_loop
    vpermb               m3, m11, m1
    cmp                 r5d, -6
    jg .w32_loop_tail
.ret:
    RET
.w8:
    vpermb              ym3, ym11, ymm2
.w8_loop:
    vpbroadcastd    ym3{k1}, [tlq-4]     ; l3 l2 l1 __   b3 a3 t3 __
    mova                ym0, ym6
    vpdpbusd            ym0, ym2, ym7
    mova                ym1, ym6
    vpdpbusd            ym1, ym2, ym8
    sub                 tlq, 2
    vpdpbusd            ym0, ym3, ym9
    vpdpbusd            ym1, ym3, ym10
    mova                ym3, ym5
    packssdw            ym0, ym1
    psraw               ym5, ym0, 4      ; c0 d0         a1 b1
    packuswb            ym3, ym5         ; a0 b0 c0 d0   __ __ a1 b1
    pshufd              ym2, ym3, q3333  ; d0 d0 d0 d0   b1 b1 b1 b1
    vpermb              ym3, ym11, ym3   ; a0 a1 b0 b1
    movq   [dstq+strideq*0], xm3
    movhps [dstq+strideq*1], xm3
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w8_loop
    RET
.w4_loop:
    vpbroadcastd       xmm3, [tlq-4]     ; l3 l2 l1 __
    mova               xmm0, xm6
    vpdpbusd           xmm0, xmm2, xm7
    mova               xmm1, xm6
    vpdpbusd           xmm1, xmm2, xm8
    sub                 tlq, 2
    vpdpbusd           xmm0, xmm3, xm9
    vpdpbusd           xmm1, xmm3, xm10
    packssdw           xmm0, xmm1
.w4:
    psraw              xmm0, 4           ; a0 b0
    packuswb           xmm0, xmm0
    movd   [dstq+strideq*0], xmm0
    pshufd             xmm2, xmm0, q1111 ; b0 b0 b0 b0
    movd   [dstq+strideq*1], xmm2
    lea                dstq, [dstq+strideq*2]
    sub                  hd, 2
    jg .w4_loop
    RET

%endif ; ARCH_X86_64
