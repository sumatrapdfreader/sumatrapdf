/*
 * jsimd.h
 *
 * Copyright 2009 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * 
 * Based on the x86 SIMD extension for IJG JPEG library,
 * Copyright (C) 1999-2006, MIYASAKA Masaru.
 * For conditions of distribution and use, see copyright notice in jsimdext.inc
 *
 */

/* Short forms of external names for systems with brain-damaged linkers. */

#ifdef NEED_SHORT_EXTERNAL_NAMES
#define jsimd_can_rgb_ycc                 jSCanRgbYcc
#define jsimd_can_ycc_rgb                 jSCanYccRgb
#define jsimd_rgb_ycc_convert             jSRgbYccConv
#define jsimd_ycc_rgb_convert             jSYccRgbConv
#define jsimd_can_h2v2_downsample         jSCanH2V2Down
#define jsimd_can_h2v1_downsample         jSCanH2V1Down
#define jsimd_h2v2_downsample             jSH2V2Down
#define jsimd_h2v1_downsample             jSH2V1Down
#define jsimd_can_h2v2_upsample           jSCanH2V2Up
#define jsimd_can_h2v1_upsample           jSCanH2V1Up
#define jsimd_h2v2_upsample               jSH2V2Up
#define jsimd_h2v1_upsample               jSH2V1Up
#define jsimd_can_h2v2_fancy_upsample     jSCanH2V2FUp
#define jsimd_can_h2v1_fancy_upsample     jSCanH2V1FUp
#define jsimd_h2v2_fancy_upsample         jSH2V2FUp
#define jsimd_h2v1_fancy_upsample         jSH2V1FUp
#define jsimd_can_h2v2_merged_upsample    jSCanH2V2MUp
#define jsimd_can_h2v1_merged_upsample    jSCanH2V1MUp
#define jsimd_h2v2_merged_upsample        jSH2V2MUp
#define jsimd_h2v1_merged_upsample        jSH2V1MUp
#endif /* NEED_SHORT_EXTERNAL_NAMES */

EXTERN(int) jsimd_can_rgb_ycc JPP((void));
EXTERN(int) jsimd_can_ycc_rgb JPP((void));

EXTERN(void) jsimd_rgb_ycc_convert
        JPP((j_compress_ptr cinfo,
             JSAMPARRAY input_buf, JSAMPIMAGE output_buf,
             JDIMENSION output_row, int num_rows));
EXTERN(void) jsimd_ycc_rgb_convert
        JPP((j_decompress_ptr cinfo,
             JSAMPIMAGE input_buf, JDIMENSION input_row,
             JSAMPARRAY output_buf, int num_rows));

EXTERN(int) jsimd_can_h2v2_downsample JPP((void));
EXTERN(int) jsimd_can_h2v1_downsample JPP((void));

EXTERN(void) jsimd_h2v2_downsample
        JPP((j_compress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY output_data));
EXTERN(void) jsimd_h2v1_downsample
        JPP((j_compress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY output_data));

EXTERN(int) jsimd_can_h2v2_upsample JPP((void));
EXTERN(int) jsimd_can_h2v1_upsample JPP((void));

EXTERN(void) jsimd_h2v2_upsample
        JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));
EXTERN(void) jsimd_h2v1_upsample
        JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

EXTERN(int) jsimd_can_h2v2_fancy_upsample JPP((void));
EXTERN(int) jsimd_can_h2v1_fancy_upsample JPP((void));

EXTERN(void) jsimd_h2v2_fancy_upsample
        JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));
EXTERN(void) jsimd_h2v1_fancy_upsample
        JPP((j_decompress_ptr cinfo, jpeg_component_info * compptr,
             JSAMPARRAY input_data, JSAMPARRAY * output_data_ptr));

EXTERN(int) jsimd_can_h2v2_merged_upsample JPP((void));
EXTERN(int) jsimd_can_h2v1_merged_upsample JPP((void));

EXTERN(void) jsimd_h2v2_merged_upsample
        JPP((j_decompress_ptr cinfo,
             JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
             JSAMPARRAY output_buf));
EXTERN(void) jsimd_h2v1_merged_upsample
        JPP((j_decompress_ptr cinfo,
             JSAMPIMAGE input_buf, JDIMENSION in_row_group_ctr,
             JSAMPARRAY output_buf));

