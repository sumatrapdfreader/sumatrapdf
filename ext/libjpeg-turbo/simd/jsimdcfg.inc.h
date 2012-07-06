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

;
; -- jpeglib.h
;

%define _cpp_protection_DCTSIZE DCTSIZE
%define _cpp_protection_DCTSIZE2 DCTSIZE2

;
; -- jmorecfg.h
;

%define _cpp_protection_RGB_RED RGB_RED
%define _cpp_protection_RGB_GREEN RGB_GREEN
%define _cpp_protection_RGB_BLUE RGB_BLUE
%define _cpp_protection_RGB_PIXELSIZE RGB_PIXELSIZE

%define _cpp_protection_EXT_RGB_RED EXT_RGB_RED
%define _cpp_protection_EXT_RGB_GREEN EXT_RGB_GREEN
%define _cpp_protection_EXT_RGB_BLUE EXT_RGB_BLUE
%define _cpp_protection_EXT_RGB_PIXELSIZE EXT_RGB_PIXELSIZE

%define _cpp_protection_EXT_RGBX_RED EXT_RGBX_RED
%define _cpp_protection_EXT_RGBX_GREEN EXT_RGBX_GREEN
%define _cpp_protection_EXT_RGBX_BLUE EXT_RGBX_BLUE
%define _cpp_protection_EXT_RGBX_PIXELSIZE EXT_RGBX_PIXELSIZE

%define _cpp_protection_EXT_BGR_RED EXT_BGR_RED
%define _cpp_protection_EXT_BGR_GREEN EXT_BGR_GREEN
%define _cpp_protection_EXT_BGR_BLUE EXT_BGR_BLUE
%define _cpp_protection_EXT_BGR_PIXELSIZE EXT_BGR_PIXELSIZE

%define _cpp_protection_EXT_BGRX_RED EXT_BGRX_RED
%define _cpp_protection_EXT_BGRX_GREEN EXT_BGRX_GREEN
%define _cpp_protection_EXT_BGRX_BLUE EXT_BGRX_BLUE
%define _cpp_protection_EXT_BGRX_PIXELSIZE EXT_BGRX_PIXELSIZE

%define _cpp_protection_EXT_XBGR_RED EXT_XBGR_RED
%define _cpp_protection_EXT_XBGR_GREEN EXT_XBGR_GREEN
%define _cpp_protection_EXT_XBGR_BLUE EXT_XBGR_BLUE
%define _cpp_protection_EXT_XBGR_PIXELSIZE EXT_XBGR_PIXELSIZE

%define _cpp_protection_EXT_XRGB_RED EXT_XRGB_RED
%define _cpp_protection_EXT_XRGB_GREEN EXT_XRGB_GREEN
%define _cpp_protection_EXT_XRGB_BLUE EXT_XRGB_BLUE
%define _cpp_protection_EXT_XRGB_PIXELSIZE EXT_XRGB_PIXELSIZE

%define RGBX_FILLER_0XFF        1

; Representation of a single sample (pixel element value).
; On this SIMD implementation, this must be 'unsigned char'.
;

%define JSAMPLE                 byte          ; unsigned char
%define SIZEOF_JSAMPLE          SIZEOF_BYTE   ; sizeof(JSAMPLE)

%define _cpp_protection_CENTERJSAMPLE CENTERJSAMPLE

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

%define _cpp_protection_JSIMD_NONE JSIMD_NONE
%define _cpp_protection_JSIMD_MMX JSIMD_MMX
%define _cpp_protection_JSIMD_3DNOW JSIMD_3DNOW
%define _cpp_protection_JSIMD_SSE JSIMD_SSE
%define _cpp_protection_JSIMD_SSE2 JSIMD_SSE2

; Short forms of external names for systems with brain-damaged linkers.
;
#ifdef NEED_SHORT_EXTERNAL_NAMES
%define _cpp_protection_jpeg_simd_cpu_support jpeg_simd_cpu_support
%define _cpp_protection_jsimd_rgb_ycc_convert_mmx jsimd_rgb_ycc_convert_mmx
%define _cpp_protection_jsimd_ycc_rgb_convert_mmx jsimd_ycc_rgb_convert_mmx
%define _cpp_protection_jconst_rgb_ycc_convert_sse2 jconst_rgb_ycc_convert_sse2
%define _cpp_protection_jsimd_rgb_ycc_convert_sse2 jsimd_rgb_ycc_convert_sse2
%define _cpp_protection_jconst_ycc_rgb_convert_sse2 jconst_ycc_rgb_convert_sse2
%define _cpp_protection_jsimd_ycc_rgb_convert_sse2 jsimd_ycc_rgb_convert_sse2
%define _cpp_protection_jsimd_h2v2_downsample_mmx jsimd_h2v2_downsample_mmx
%define _cpp_protection_jsimd_h2v1_downsample_mmx jsimd_h2v1_downsample_mmx
%define _cpp_protection_jsimd_h2v2_downsample_sse2 jsimd_h2v2_downsample_sse2
%define _cpp_protection_jsimd_h2v1_downsample_sse2 jsimd_h2v1_downsample_sse2
%define _cpp_protection_jsimd_h2v2_upsample_mmx jsimd_h2v2_upsample_mmx
%define _cpp_protection_jsimd_h2v1_upsample_mmx jsimd_h2v1_upsample_mmx
%define _cpp_protection_jsimd_h2v1_fancy_upsample_mmx jsimd_h2v1_fancy_upsample_mmx
%define _cpp_protection_jsimd_h2v2_fancy_upsample_mmx jsimd_h2v2_fancy_upsample_mmx
%define _cpp_protection_jsimd_h2v1_merged_upsample_mmx jsimd_h2v1_merged_upsample_mmx
%define _cpp_protection_jsimd_h2v2_merged_upsample_mmx jsimd_h2v2_merged_upsample_mmx
%define _cpp_protection_jsimd_h2v2_upsample_sse2 jsimd_h2v2_upsample_sse2
%define _cpp_protection_jsimd_h2v1_upsample_sse2 jsimd_h2v1_upsample_sse2
%define _cpp_protection_jconst_fancy_upsample_sse2 jconst_fancy_upsample_sse2
%define _cpp_protection_jsimd_h2v1_fancy_upsample_sse2 jsimd_h2v1_fancy_upsample_sse2
%define _cpp_protection_jsimd_h2v2_fancy_upsample_sse2 jsimd_h2v2_fancy_upsample_sse2
%define _cpp_protection_jconst_merged_upsample_sse2 jconst_merged_upsample_sse2
%define _cpp_protection_jsimd_h2v1_merged_upsample_sse2 jsimd_h2v1_merged_upsample_sse2
%define _cpp_protection_jsimd_h2v2_merged_upsample_sse2 jsimd_h2v2_merged_upsample_sse2
%define _cpp_protection_jsimd_convsamp_mmx jsimd_convsamp_mmx
%define _cpp_protection_jsimd_convsamp_sse2 jsimd_convsamp_sse2
%define _cpp_protection_jsimd_convsamp_float_3dnow jsimd_convsamp_float_3dnow
%define _cpp_protection_jsimd_convsamp_float_sse jsimd_convsamp_float_sse
%define _cpp_protection_jsimd_convsamp_float_sse2 jsimd_convsamp_float_sse2
%define _cpp_protection_jsimd_fdct_islow_mmx jsimd_fdct_islow_mmx
%define _cpp_protection_jsimd_fdct_ifast_mmx jsimd_fdct_ifast_mmx
%define _cpp_protection_jconst_fdct_islow_sse2 jconst_fdct_islow_sse2
%define _cpp_protection_jsimd_fdct_islow_sse2 jsimd_fdct_islow_sse2
%define _cpp_protection_jconst_fdct_ifast_sse2 jconst_fdct_ifast_sse2
%define _cpp_protection_jsimd_fdct_ifast_sse2 jsimd_fdct_ifast_sse2
%define _cpp_protection_jsimd_fdct_float_3dnow jsimd_fdct_float_3dnow
%define _cpp_protection_jconst_fdct_float_sse jconst_fdct_float_sse
%define _cpp_protection_jsimd_fdct_float_sse jsimd_fdct_float_sse
%define _cpp_protection_jsimd_quantize_mmx jsimd_quantize_mmx
%define _cpp_protection_jsimd_quantize_sse2 jsimd_quantize_sse2
%define _cpp_protection_jsimd_quantize_float_3dnow jsimd_quantize_float_3dnow
%define _cpp_protection_jsimd_quantize_float_sse jsimd_quantize_float_sse
%define _cpp_protection_jsimd_quantize_float_sse2 jsimd_quantize_float_sse2
%define _cpp_protection_jsimd_idct_2x2_mmx jsimd_idct_2x2_mmx
%define _cpp_protection_jsimd_idct_4x4_mmx jsimd_idct_4x4_mmx
%define _cpp_protection_jconst_idct_red_sse2 jconst_idct_red_sse2
%define _cpp_protection_jsimd_idct_2x2_sse2 jsimd_idct_2x2_sse2
%define _cpp_protection_jsimd_idct_4x4_sse2 jsimd_idct_4x4_sse2
%define _cpp_protection_jsimd_idct_islow_mmx jsimd_idct_islow_mmx
%define _cpp_protection_jsimd_idct_ifast_mmx jsimd_idct_ifast_mmx
%define _cpp_protection_jconst_idct_islow_sse2 jconst_idct_islow_sse2
%define _cpp_protection_jsimd_idct_islow_sse2 jsimd_idct_islow_sse2
%define _cpp_protection_jconst_idct_ifast_sse2 jconst_idct_ifast_sse2
%define _cpp_protection_jsimd_idct_ifast_sse2 jsimd_idct_ifast_sse2
%define _cpp_protection_jsimd_idct_float_3dnow jsimd_idct_float_3dnow
%define _cpp_protection_jconst_idct_float_sse jconst_idct_float_sse
%define _cpp_protection_jsimd_idct_float_sse jsimd_idct_float_sse
%define _cpp_protection_jconst_idct_float_sse2 jconst_idct_float_sse2
%define _cpp_protection_jsimd_idct_float_sse2 jsimd_idct_float_sse2
#endif /* NEED_SHORT_EXTERNAL_NAMES */

