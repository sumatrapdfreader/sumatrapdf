// This file generates the include file for the assembly
// implementations by abusing the C preprocessor.
//
// Note: Some things are manually defined as they need to
// be mapped to NASM types.

;
; Automatically generated include file from jsimdcfg.inc.h
;

#define JPEG_INTERNALS

#include "../jpeglib.h"
#include "../jconfig.h"
#include "../jmorecfg.h"
#include "jsimd.h"

#define define(var) %define _cpp_protection_##var
#define definev(var) %define _cpp_protection_##var var

;
; -- jpeglib.h
;

definev(DCTSIZE)
definev(DCTSIZE2)

;
; -- jmorecfg.h
;

definev(RGB_RED)
definev(RGB_GREEN)
definev(RGB_BLUE)

definev(RGB_PIXELSIZE)

; Representation of a single sample (pixel element value).
; On this SIMD implementation, this must be 'unsigned char'.
;

%define JSAMPLE                 byte          ; unsigned char
%define SIZEOF_JSAMPLE          SIZEOF_BYTE   ; sizeof(JSAMPLE)

definev(CENTERJSAMPLE)

; Representation of a DCT frequency coefficient.
; On this SIMD implementation, this must be 'short'.
;
%define JCOEF                   word          ; short
%define SIZEOF_JCOEF            SIZEOF_WORD   ; sizeof(JCOEF)

; Datatype used for image dimensions.
; On this SIMD implementation, this must be 'unsigned int'.
;
%define JDIMENSION              dword         ; unsigned int
%define SIZEOF_JDIMENSION       SIZEOF_DWORD  ; sizeof(JDIMENSION)

%define JSAMPROW                POINTER       ; JSAMPLE FAR * (jpeglib.h)
%define JSAMPARRAY              POINTER       ; JSAMPROW *    (jpeglib.h)
%define JSAMPIMAGE              POINTER       ; JSAMPARRAY *  (jpeglib.h)
%define JCOEFPTR                POINTER       ; JCOEF FAR *   (jpeglib.h)
%define SIZEOF_JSAMPROW         SIZEOF_POINTER  ; sizeof(JSAMPROW)
%define SIZEOF_JSAMPARRAY       SIZEOF_POINTER  ; sizeof(JSAMPARRAY)
%define SIZEOF_JSAMPIMAGE       SIZEOF_POINTER  ; sizeof(JSAMPIMAGE)
%define SIZEOF_JCOEFPTR         SIZEOF_POINTER  ; sizeof(JCOEFPTR)

;
; -- jdct.h
;

; A forward DCT routine is given a pointer to a work area of type DCTELEM[];
; the DCT is to be performed in-place in that buffer.
; To maximize parallelism, Type DCTELEM is changed to short (originally, int).
;
%define DCTELEM                 word          ; short
%define SIZEOF_DCTELEM          SIZEOF_WORD   ; sizeof(DCTELEM)

%define FAST_FLOAT              FP32            ; float
%define SIZEOF_FAST_FLOAT       SIZEOF_FP32     ; sizeof(FAST_FLOAT)

; To maximize parallelism, Type MULTIPLIER is changed to short.
;
%define ISLOW_MULT_TYPE         word          ; must be short
%define SIZEOF_ISLOW_MULT_TYPE  SIZEOF_WORD   ; sizeof(ISLOW_MULT_TYPE)

%define IFAST_MULT_TYPE         word          ; must be short
%define SIZEOF_IFAST_MULT_TYPE  SIZEOF_WORD   ; sizeof(IFAST_MULT_TYPE)
%define IFAST_SCALE_BITS        2             ; fractional bits in scale factors

%define FLOAT_MULT_TYPE         FP32          ; must be float
%define SIZEOF_FLOAT_MULT_TYPE  SIZEOF_FP32   ; sizeof(FLOAT_MULT_TYPE)

;
; -- jsimd.h
;

definev(JSIMD_NONE)
definev(JSIMD_MMX)
definev(JSIMD_3DNOW)
definev(JSIMD_SSE)
definev(JSIMD_SSE2)

; Short forms of external names for systems with brain-damaged linkers.
;
#ifdef NEED_SHORT_EXTERNAL_NAMES
definev(jpeg_simd_cpu_support)
definev(jsimd_rgb_ycc_convert_mmx)
definev(jsimd_ycc_rgb_convert_mmx)
definev(jconst_rgb_ycc_convert_sse2)
definev(jsimd_rgb_ycc_convert_sse2)
definev(jconst_ycc_rgb_convert_sse2)
definev(jsimd_ycc_rgb_convert_sse2)
definev(jsimd_h2v2_downsample_mmx)
definev(jsimd_h2v1_downsample_mmx)
definev(jsimd_h2v2_downsample_sse2)
definev(jsimd_h2v1_downsample_sse2)
definev(jsimd_h2v2_upsample_mmx)
definev(jsimd_h2v1_upsample_mmx)
definev(jsimd_h2v1_fancy_upsample_mmx)
definev(jsimd_h2v2_fancy_upsample_mmx)
definev(jsimd_h2v1_merged_upsample_mmx)
definev(jsimd_h2v2_merged_upsample_mmx)
definev(jsimd_h2v2_upsample_sse2)
definev(jsimd_h2v1_upsample_sse2)
definev(jconst_fancy_upsample_sse2)
definev(jsimd_h2v1_fancy_upsample_sse2)
definev(jsimd_h2v2_fancy_upsample_sse2)
definev(jconst_merged_upsample_sse2)
definev(jsimd_h2v1_merged_upsample_sse2)
definev(jsimd_h2v2_merged_upsample_sse2)
definev(jsimd_convsamp_mmx)
definev(jsimd_convsamp_sse2)
definev(jsimd_convsamp_float_3dnow)
definev(jsimd_convsamp_float_sse)
definev(jsimd_convsamp_float_sse2)
definev(jsimd_fdct_islow_mmx)
definev(jsimd_fdct_ifast_mmx)
definev(jconst_fdct_islow_sse2)
definev(jsimd_fdct_islow_sse2)
definev(jconst_fdct_ifast_sse2)
definev(jsimd_fdct_ifast_sse2)
definev(jsimd_fdct_float_3dnow)
definev(jconst_fdct_float_sse)
definev(jsimd_fdct_float_sse)
definev(jsimd_quantize_mmx)
definev(jsimd_quantize_sse2)
definev(jsimd_quantize_float_3dnow)
definev(jsimd_quantize_float_sse)
definev(jsimd_quantize_float_sse2)
definev(jsimd_idct_2x2_mmx)
definev(jsimd_idct_4x4_mmx)
definev(jconst_idct_red_sse2)
definev(jsimd_idct_2x2_sse2)
definev(jsimd_idct_4x4_sse2)
definev(jsimd_idct_islow_mmx)
definev(jsimd_idct_ifast_mmx)
definev(jconst_idct_islow_sse2)
definev(jsimd_idct_islow_sse2)
definev(jconst_idct_ifast_sse2)
definev(jsimd_idct_ifast_sse2)
definev(jsimd_idct_float_3dnow)
definev(jconst_idct_float_sse)
definev(jsimd_idct_float_sse)
definev(jconst_idct_float_sse2)
definev(jsimd_idct_float_sse2)
#endif /* NEED_SHORT_EXTERNAL_NAMES */

