;
; jdcolss2-64.asm - colorspace conversion (64-bit SSE2)
;
; Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
; Copyright 2009 D. R. Commander
;
; Based on
; x86 SIMD extension for IJG JPEG library
; Copyright (C) 1999-2006, MIYASAKA Masaru.
; For conditions of distribution and use, see copyright notice in jsimdext.inc
;
; This file should be assembled with NASM (Netwide Assembler),
; can *not* be assembled with Microsoft's MASM or any compatible
; assembler (including Borland's Turbo Assembler).
; NASM is available from http://nasm.sourceforge.net/ or
; http://sourceforge.net/project/showfiles.php?group_id=6208
;
; [TAB8]

%include "jsimdext.inc"

; --------------------------------------------------------------------------

%define SCALEBITS	16

F_0_344	equ	 22554			; FIX(0.34414)
F_0_714	equ	 46802			; FIX(0.71414)
F_1_402	equ	 91881			; FIX(1.40200)
F_1_772	equ	116130			; FIX(1.77200)
F_0_402	equ	(F_1_402 - 65536)	; FIX(1.40200) - FIX(1)
F_0_285	equ	( 65536 - F_0_714)	; FIX(1) - FIX(0.71414)
F_0_228	equ	(131072 - F_1_772)	; FIX(2) - FIX(1.77200)

; --------------------------------------------------------------------------
	SECTION	SEG_CONST

	alignz	16
	global	EXTN(jconst_ycc_rgb_convert_sse2)

EXTN(jconst_ycc_rgb_convert_sse2):

PW_F0402	times 8 dw  F_0_402
PW_MF0228	times 8 dw -F_0_228
PW_MF0344_F0285	times 4 dw -F_0_344, F_0_285
PW_ONE		times 8 dw  1
PD_ONEHALF	times 4 dd  1 << (SCALEBITS-1)

	alignz	16

; --------------------------------------------------------------------------
%include "jdclrss2-64.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 0
%define RGB_GREEN 1
%define RGB_BLUE 2
%define RGB_PIXELSIZE 3
%define jsimd_ycc_rgb_convert_sse2 jsimd_ycc_extrgb_convert_sse2
%include "jdclrss2-64.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 0
%define RGB_GREEN 1
%define RGB_BLUE 2
%define RGB_PIXELSIZE 4
%define jsimd_ycc_rgb_convert_sse2 jsimd_ycc_extrgbx_convert_sse2
%include "jdclrss2-64.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 2
%define RGB_GREEN 1
%define RGB_BLUE 0
%define RGB_PIXELSIZE 3
%define jsimd_ycc_rgb_convert_sse2 jsimd_ycc_extbgr_convert_sse2
%include "jdclrss2-64.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 2
%define RGB_GREEN 1
%define RGB_BLUE 0
%define RGB_PIXELSIZE 4
%define jsimd_ycc_rgb_convert_sse2 jsimd_ycc_extbgrx_convert_sse2
%include "jdclrss2-64.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 3
%define RGB_GREEN 2
%define RGB_BLUE 1
%define RGB_PIXELSIZE 4
%define jsimd_ycc_rgb_convert_sse2 jsimd_ycc_extxbgr_convert_sse2
%include "jdclrss2-64.asm"

%undef RGB_RED
%undef RGB_GREEN
%undef RGB_BLUE
%undef RGB_PIXELSIZE
%define RGB_RED 1
%define RGB_GREEN 2
%define RGB_BLUE 3
%define RGB_PIXELSIZE 4
%define jsimd_ycc_rgb_convert_sse2 jsimd_ycc_extxrgb_convert_sse2
%include "jdclrss2-64.asm"
