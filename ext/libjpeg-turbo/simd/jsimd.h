/*
 * simd/jsimd.h
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2011 D. R. Commander
 * 
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 */

/* Bitmask for supported acceleration methods */

#define JSIMD_NONE       0x00
#define JSIMD_MMX        0x01
#define JSIMD_3DNOW      0x02
#define JSIMD_SSE        0x04
#define JSIMD_SSE2       0x08
#define JSIMD_ARM_NEON   0x10

/* Short forms of external names for systems with brain-damaged linkers. */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jpeg_simd_cpu_support                 jSiCpuSupport
#define jsimd_rgb_ycc_convert_mmx             jSRGBYCCM
#define jsimd_extrgb_ycc_convert_mmx          jSEXTRGBYCCM
#define jsimd_extrgbx_ycc_convert_mmx         jSEXTRGBXYCCM
#define jsimd_extbgr_ycc_convert_mmx          jSEXTBGRYCCM
#define jsimd_extbgrx_ycc_convert_mmx         jSEXTBGRXYCCM
#define jsimd_extxbgr_ycc_convert_mmx         jSEXTXBGRYCCM
#define jsimd_extxrgb_ycc_convert_mmx         jSEXTXRGBYCCM
#define jsimd_rgb_gray_convert_mmx            jSRGBGRYM
#define jsimd_extrgb_gray_convert_mmx         jSEXTRGBGRYM
#define jsimd_extrgbx_gray_convert_mmx        jSEXTRGBXGRYM
#define jsimd_extbgr_gray_convert_mmx         jSEXTBGRGRYM
#define jsimd_extbgrx_gray_convert_mmx        jSEXTBGRXGRYM
#define jsimd_extxbgr_gray_convert_mmx        jSEXTXBGRGRYM
#define jsimd_extxrgb_gray_convert_mmx        jSEXTXRGBGRYM
#define jsimd_ycc_rgb_convert_mmx             jSYCCRGBM
#define jsimd_ycc_extrgb_convert_mmx          jSYCCEXTRGBM
#define jsimd_ycc_extrgbx_convert_mmx         jSYCCEXTRGBXM
#define jsimd_ycc_extbgr_convert_mmx          jSYCCEXTBGRM
#define jsimd_ycc_extbgrx_convert_mmx         jSYCCEXTBGRXM
#define jsimd_ycc_extxbgr_convert_mmx         jSYCCEXTXBGRM
#define jsimd_ycc_extxrgb_convert_mmx         jSYCCEXTXRGBM
#define jconst_rgb_ycc_convert_sse2           jSCRGBYCCS2
#define jsimd_rgb_ycc_convert_sse2            jSRGBYCCS2
#define jsimd_extrgb_ycc_convert_sse2         jSEXTRGBYCCS2
#define jsimd_extrgbx_ycc_convert_sse2        jSEXTRGBXYCCS2
#define jsimd_extbgr_ycc_convert_sse2         jSEXTBGRYCCS2
#define jsimd_extbgrx_ycc_convert_sse2        jSEXTBGRXYCCS2
#define jsimd_extxbgr_ycc_convert_sse2        jSEXTXBGRYCCS2
#define jsimd_extxrgb_ycc_convert_sse2        jSEXTXRGBYCCS2
#define jconst_rgb_gray_convert_sse2          jSCRGBGRYS2
#define jsimd_rgb_gray_convert_sse2           jSRGBGRYS2
#define jsimd_extrgb_gray_convert_sse2        jSEXTRGBGRYS2
#define jsimd_extrgbx_gray_convert_sse2       jSEXTRGBXGRYS2
#define jsimd_extbgr_gray_convert_sse2        jSEXTBGRGRYS2
#define jsimd_extbgrx_gray_convert_sse2       jSEXTBGRXGRYS2
#define jsimd_extxbgr_gray_convert_sse2       jSEXTXBGRGRYS2
#define jsimd_extxrgb_gray_convert_sse2       jSEXTXRGBGRYS2
#define jconst_ycc_rgb_convert_sse2           jSCYCCRGBS2
#define jsimd_ycc_rgb_convert_sse2            jSYCCRGBS2
#define jsimd_ycc_extrgb_convert_sse2         jSYCCEXTRGBS2
#define jsimd_ycc_extrgbx_convert_sse2        jSYCCEXTRGBXS2
#define jsimd_ycc_extbgr_convert_sse2         jSYCCEXTBGRS2
#define jsimd_ycc_extbgrx_convert_sse2        jSYCCEXTBGRXS2
#define jsimd_ycc_extxbgr_convert_sse2        jSYCCEXTXBGRS2
#define jsimd_ycc_extxrgb_convert_sse2        jSYCCEXTXRGBS2
#define jsimd_h2v2_downsample_mmx             jSDnH2V2M
#define jsimd_h2v1_downsample_mmx             jSDnH2V1M
#define jsimd_h2v2_downsample_sse2            jSDnH2V2S2
#define jsimd_h2v1_downsample_sse2            jSDnH2V1S2
#define jsimd_h2v2_upsample_mmx               jSUpH2V2M
#define jsimd_h2v1_upsample_mmx               jSUpH2V1M
#define jsimd_h2v2_fancy_upsample_mmx         jSFUpH2V2M
#define jsimd_h2v1_fancy_upsample_mmx         jSFUpH2V1M
#define jsimd_h2v2_merged_upsample_mmx        jSMUpH2V2M
#define jsimd_h2v2_extrgb_merged_upsample_mmx jSMUpH2V2EXTRGBM
#define jsimd_h2v2_extrgbx_merged_upsample_mmx jSMUpH2V2EXTRGBXM
#define jsimd_h2v2_extbgr_merged_upsample_mmx jSMUpH2V2EXTBGRM
#define jsimd_h2v2_extbgrx_merged_upsample_mmx jSMUpH2V2EXTBGRXM
#define jsimd_h2v2_extxbgr_merged_upsample_mmx jSMUpH2V2EXTXBGRM
#define jsimd_h2v2_extxrgb_merged_upsample_mmx jSMUpH2V2EXTXRGBM
#define jsimd_h2v1_merged_upsample_mmx        jSMUpH2V1M
#define jsimd_h2v1_extrgb_merged_upsample_mmx jSMUpH2V1EXTRGBM
#define jsimd_h2v1_extrgbx_merged_upsample_mmx jSMUpH2V1EXTRGBXM
#define jsimd_h2v1_extbgr_merged_upsample_mmx jSMUpH2V1EXTBGRM
#define jsimd_h2v1_extbgrx_merged_upsample_mmx jSMUpH2V1EXTBGRXM
#define jsimd_h2v1_extxbgr_merged_upsample_mmx jSMUpH2V1EXTXBGRM
#define jsimd_h2v1_extxrgb_merged_upsample_mmx jSMUpH2V1EXTXRGBM
#define jsimd_h2v2_upsample_sse2              jSUpH2V2S2
#define jsimd_h2v1_upsample_sse2              jSUpH2V1S2
#define jconst_fancy_upsample_sse2            jSCFUpS2
#define jsimd_h2v2_fancy_upsample_sse2        jSFUpH2V2S2
#define jsimd_h2v1_fancy_upsample_sse2        jSFUpH2V1S2
#define jconst_merged_upsample_sse2           jSCMUpS2
#define jsimd_h2v2_merged_upsample_sse2       jSMUpH2V2S2
#define jsimd_h2v2_extrgb_merged_upsample_sse2 jSMUpH2V2EXTRGBS2
#define jsimd_h2v2_extrgbx_merged_upsample_sse2 jSMUpH2V2EXTRGBXS2
#define jsimd_h2v2_extbgr_merged_upsample_sse2 jSMUpH2V2EXTBGRS2
#define jsimd_h2v2_extbgrx_merged_upsample_sse2 jSMUpH2V2EXTBGRXS2
#define jsimd_h2v2_extxbgr_merged_upsample_sse2 jSMUpH2V2EXTXBGRS2
#define jsimd_h2v2_extxrgb_merged_upsample_sse2 jSMUpH2V2EXTXRGBS2
#define jsimd_h2v1_merged_upsample_sse2       jSMUpH2V1S2
#define jsimd_h2v1_extrgb_merged_upsample_sse2 jSMUpH2V1EXTRGBS2
#define jsimd_h2v1_extrgbx_merged_upsample_sse2 jSMUpH2V1EXTRGBXS2
#define jsimd_h2v1_extbgr_merged_upsample_sse2 jSMUpH2V1EXTBGRS2
#define jsimd_h2v1_extbgrx_merged_upsample_sse2 jSMUpH2V1EXTBGRXS2
#define jsimd_h2v1_extxbgr_merged_upsample_sse2 jSMUpH2V1EXTXBGRS2
#define jsimd_h2v1_extxrgb_merged_upsample_sse2 jSMUpH2V1EXTXRGBS2
#define jsimd_convsamp_mmx                    jSConvM
#define jsimd_convsamp_sse2                   jSConvS2
#define jsimd_convsamp_float_3dnow            jSConvF3D
#define jsimd_convsamp_float_sse              jSConvFS
#define jsimd_convsamp_float_sse2             jSConvFS2
#define jsimd_fdct_islow_mmx                  jSFDMIS
#define jsimd_fdct_ifast_mmx                  jSFDMIF
#define jconst_fdct_islow_sse2                jSCFDS2IS
#define jsimd_fdct_islow_sse2                 jSFDS2IS
#define jconst_fdct_ifast_sse2                jSCFDS2IF
#define jsimd_fdct_ifast_sse2                 jSFDS2IF
#define jsimd_fdct_float_3dnow                jSFD3DF
#define jconst_fdct_float_sse                 jSCFDSF
#define jsimd_fdct_float_sse                  jSFDSF
#define jsimd_quantize_mmx                    jSQuantM
#define jsimd_quantize_sse2                   jSQuantS2
#define jsimd_quantize_float_3dnow            jSQuantF3D
#define jsimd_quantize_float_sse              jSQuantFS
#define jsimd_quantize_float_sse2             jSQuantFS2
#define jsimd_idct_2x2_mmx                    jSIDM22
#define jsimd_idct_4x4_mmx                    jSIDM44
#define jconst_idct_red_sse2                  jSCIDS2R
#define jsimd_idct_2x2_sse2                   jSIDS222
#define jsimd_idct_4x4_sse2                   jSIDS244
#define jsimd_idct_islow_mmx                  jSIDMIS
#define jsimd_idct_ifast_mmx                  jSIDMIF
#define jconst_idct_islow_sse2                jSCIDS2IS
#define jsimd_idct_islow_sse2                 jSIDS2IS
#define jconst_idct_ifast_sse2                jSCIDS2IF
#define jsimd_idct_ifast_sse2                 jSIDS2IF
#define jsimd_idct_float_3dnow                jSID3DF
#define jconst_fdct_float_sse                 jSCIDSF
#define jsimd_idct_float_sse                  jSIDSF
#define jconst_fdct_float_sse2                jSCIDS2F
#define jsimd_idct_float_sse2                 jSIDS2F
#endif /* NEED_SHORT_EXTERNAL_NAMES */

/* SIMD Ext: retrieve SIMD/CPU information */
EXTERN(unsigned int) jpeg_simd_cpu_support JPP((void));

/* SIMD Color Space Conversion */
EXTERN(void) jsimd_rgb_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgb_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgbx_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgr_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgrx_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxbgr_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxrgb_ycc_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));

EXTERN(void) jsimd_rgb_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgb_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgbx_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgr_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgrx_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxbgr_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxrgb_gray_convert_mmx
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));

EXTERN(void) jsimd_ycc_rgb_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extrgb_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extrgbx_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extbgr_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extbgrx_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extxbgr_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extxrgb_convert_mmx
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));

extern const int jconst_rgb_ycc_convert_sse2[];
EXTERN(void) jsimd_rgb_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgb_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgbx_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgr_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgrx_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxbgr_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxrgb_ycc_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));

extern const int jconst_rgb_gray_convert_sse2[];
EXTERN(void) jsimd_rgb_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgb_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgbx_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgr_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgrx_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxbgr_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxrgb_gray_convert_sse2
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));

extern const int jconst_ycc_rgb_convert_sse2[];
EXTERN(void) jsimd_ycc_rgb_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extrgb_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extrgbx_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extbgr_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extbgrx_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extxbgr_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extxrgb_convert_sse2
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));

EXTERN(void) jsimd_rgb_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgb_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extrgbx_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgr_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extbgrx_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxbgr_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_extxrgb_ycc_convert_neon
        JPP((JDIMENSION img_width,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));

EXTERN(void) jsimd_ycc_rgb_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extrgb_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extrgbx_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extbgr_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extbgrx_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extxbgr_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));
EXTERN(void) jsimd_ycc_extxrgb_convert_neon
        JPP((JDIMENSION out_width,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));

/* SIMD Downsample */
EXTERN(void) jsimd_h2v2_downsample_mmx
        JPP((JDIMENSION image_width, int max_v_samp_factor,
             JDIMENSION v_samp_factor, JDIMENSION width_blocks,
             JSAMPARRAY input_data, JSAMPARRAY output_data));
EXTERN(void) jsimd_h2v1_downsample_mmx
        JPP((JDIMENSION image_width, int max_v_samp_factor,
             JDIMENSION v_samp_factor, JDIMENSION width_blocks,
             JSAMPARRAY input_data, JSAMPARRAY output_data));

EXTERN(void) jsimd_h2v2_downsample_sse2
        JPP((JDIMENSION image_width, int max_v_samp_factor,
             JDIMENSION v_samp_factor, JDIMENSION width_blocks,
             JSAMPARRAY input_data, JSAMPARRAY output_data));
EXTERN(void) jsimd_h2v1_downsample_sse2
        JPP((JDIMENSION image_width, int max_v_samp_factor,
             JDIMENSION v_samp_factor, JDIMENSION width_blocks,
             JSAMPARRAY input_data, JSAMPARRAY output_data));

/* SIMD Upsample */
EXTERN(void) jsimd_h2v2_upsample_mmx
        JPP((int max_v_samp_factor, JDIMENSION output_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));
EXTERN(void) jsimd_h2v1_upsample_mmx
        JPP((int max_v_samp_factor, JDIMENSION output_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

EXTERN(void) jsimd_h2v2_fancy_upsample_mmx
        JPP((int max_v_samp_factor, JDIMENSION downsampled_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));
EXTERN(void) jsimd_h2v1_fancy_upsample_mmx
        JPP((int max_v_samp_factor, JDIMENSION downsampled_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

EXTERN(void) jsimd_h2v2_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extrgb_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extrgbx_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extbgr_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extbgrx_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extxbgr_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extxrgb_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extrgb_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extrgbx_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extbgr_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extbgrx_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extxbgr_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extxrgb_merged_upsample_mmx
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));

EXTERN(void) jsimd_h2v2_upsample_sse2
        JPP((int max_v_samp_factor, JDIMENSION output_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));
EXTERN(void) jsimd_h2v1_upsample_sse2
        JPP((int max_v_samp_factor, JDIMENSION output_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

extern const int jconst_fancy_upsample_sse2[];
EXTERN(void) jsimd_h2v2_fancy_upsample_sse2
        JPP((int max_v_samp_factor, JDIMENSION downsampled_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));
EXTERN(void) jsimd_h2v1_fancy_upsample_sse2
        JPP((int max_v_samp_factor, JDIMENSION downsampled_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

extern const int jconst_merged_upsample_sse2[];
EXTERN(void) jsimd_h2v2_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extrgb_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extrgbx_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extbgr_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extbgrx_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extxbgr_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v2_extxrgb_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extrgb_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extrgbx_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extbgr_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extbgrx_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extxbgr_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_extxrgb_merged_upsample_sse2
        JPP((JDIMENSION output_width, JSAMPIMAGE input_buf,
             JDIMENSION in_row_group_ctr, JSAMPARRAY output_buf));

EXTERN(void) jsimd_h2v1_fancy_upsample_neon
        JPP((int max_v_samp_factor, JDIMENSION downsampled_width,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

/* SIMD Sample Conversion */
EXTERN(void) jsimd_convsamp_mmx JPP((JSAMPARRAY sample_data,
                                     JDIMENSION start_col,
                                     DCTELEM * workspace));

EXTERN(void) jsimd_convsamp_sse2 JPP((JSAMPARRAY sample_data,
                                      JDIMENSION start_col,
                                      DCTELEM * workspace));

EXTERN(void) jsimd_convsamp_neon JPP((JSAMPARRAY sample_data,
                                      JDIMENSION start_col,
                                      DCTELEM * workspace));

EXTERN(void) jsimd_convsamp_float_3dnow JPP((JSAMPARRAY sample_data,
                                             JDIMENSION start_col,
                                             FAST_FLOAT * workspace));

EXTERN(void) jsimd_convsamp_float_sse JPP((JSAMPARRAY sample_data,
                                           JDIMENSION start_col,
                                           FAST_FLOAT * workspace));

EXTERN(void) jsimd_convsamp_float_sse2 JPP((JSAMPARRAY sample_data,
                                            JDIMENSION start_col,
                                            FAST_FLOAT * workspace));

/* SIMD Forward DCT */
EXTERN(void) jsimd_fdct_islow_mmx JPP((DCTELEM * data));
EXTERN(void) jsimd_fdct_ifast_mmx JPP((DCTELEM * data));

extern const int jconst_fdct_ifast_sse2[];
EXTERN(void) jsimd_fdct_islow_sse2 JPP((DCTELEM * data));
extern const int jconst_fdct_islow_sse2[];
EXTERN(void) jsimd_fdct_ifast_sse2 JPP((DCTELEM * data));

EXTERN(void) jsimd_fdct_ifast_neon JPP((DCTELEM * data));

EXTERN(void) jsimd_fdct_float_3dnow JPP((FAST_FLOAT * data));

extern const int jconst_fdct_float_sse[];
EXTERN(void) jsimd_fdct_float_sse JPP((FAST_FLOAT * data));

/* SIMD Quantization */
EXTERN(void) jsimd_quantize_mmx JPP((JCOEFPTR coef_block,
                                     DCTELEM * divisors,
                                     DCTELEM * workspace));

EXTERN(void) jsimd_quantize_sse2 JPP((JCOEFPTR coef_block,
                                      DCTELEM * divisors,
                                      DCTELEM * workspace));

EXTERN(void) jsimd_quantize_neon JPP((JCOEFPTR coef_block,
                                      DCTELEM * divisors,
                                      DCTELEM * workspace));

EXTERN(void) jsimd_quantize_float_3dnow JPP((JCOEFPTR coef_block,
                                             FAST_FLOAT * divisors,
                                             FAST_FLOAT * workspace));

EXTERN(void) jsimd_quantize_float_sse JPP((JCOEFPTR coef_block,
                                           FAST_FLOAT * divisors,
                                           FAST_FLOAT * workspace));

EXTERN(void) jsimd_quantize_float_sse2 JPP((JCOEFPTR coef_block,
                                            FAST_FLOAT * divisors,
                                            FAST_FLOAT * workspace));

/* SIMD Reduced Inverse DCT */
EXTERN(void) jsimd_idct_2x2_mmx JPP((void * dct_table,
                                     JCOEFPTR coef_block,
                                     JSAMPARRAY output_buf,
                                     JDIMENSION output_col));
EXTERN(void) jsimd_idct_4x4_mmx JPP((void * dct_table,
                                     JCOEFPTR coef_block,
                                     JSAMPARRAY output_buf,
                                     JDIMENSION output_col));

extern const int jconst_idct_red_sse2[];
EXTERN(void) jsimd_idct_2x2_sse2 JPP((void * dct_table,
                                      JCOEFPTR coef_block,
                                      JSAMPARRAY output_buf,
                                      JDIMENSION output_col));
EXTERN(void) jsimd_idct_4x4_sse2 JPP((void * dct_table,
                                      JCOEFPTR coef_block,
                                      JSAMPARRAY output_buf,
                                      JDIMENSION output_col));

EXTERN(void) jsimd_idct_2x2_neon JPP((void * dct_table,
                                      JCOEFPTR coef_block,
                                      JSAMPARRAY output_buf,
                                      JDIMENSION output_col));
EXTERN(void) jsimd_idct_4x4_neon JPP((void * dct_table,
                                      JCOEFPTR coef_block,
                                      JSAMPARRAY output_buf,
                                      JDIMENSION output_col));

/* SIMD Inverse DCT */
EXTERN(void) jsimd_idct_islow_mmx JPP((void * dct_table,
                                       JCOEFPTR coef_block,
                                       JSAMPARRAY output_buf,
                                       JDIMENSION output_col));
EXTERN(void) jsimd_idct_ifast_mmx JPP((void * dct_table,
                                       JCOEFPTR coef_block,
                                       JSAMPARRAY output_buf,
                                       JDIMENSION output_col));

extern const int jconst_idct_islow_sse2[];
EXTERN(void) jsimd_idct_islow_sse2 JPP((void * dct_table,
                                        JCOEFPTR coef_block,
                                        JSAMPARRAY output_buf,
                                        JDIMENSION output_col));
extern const int jconst_idct_ifast_sse2[];
EXTERN(void) jsimd_idct_ifast_sse2 JPP((void * dct_table,
                                        JCOEFPTR coef_block,
                                        JSAMPARRAY output_buf,
                                        JDIMENSION output_col));

EXTERN(void) jsimd_idct_islow_neon JPP((void * dct_table,
                                        JCOEFPTR coef_block,
                                        JSAMPARRAY output_buf,
                                        JDIMENSION output_col));
EXTERN(void) jsimd_idct_ifast_neon JPP((void * dct_table,
                                        JCOEFPTR coef_block,
                                        JSAMPARRAY output_buf,
                                        JDIMENSION output_col));

EXTERN(void) jsimd_idct_float_3dnow JPP((void * dct_table,
                                         JCOEFPTR coef_block,
                                         JSAMPARRAY output_buf,
                                         JDIMENSION output_col));

extern const int jconst_idct_float_sse[];
EXTERN(void) jsimd_idct_float_sse JPP((void * dct_table,
                                       JCOEFPTR coef_block,
                                       JSAMPARRAY output_buf,
                                       JDIMENSION output_col));

extern const int jconst_idct_float_sse2[];
EXTERN(void) jsimd_idct_float_sse2 JPP((void * dct_table,
                                        JCOEFPTR coef_block,
                                        JSAMPARRAY output_buf,
                                        JDIMENSION output_col));

