/*
 * Copyright (C) 2025-2026, D. R. Commander.  All Rights Reserved.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <stdio.h>
#define JPEG_INTERNALS
#include "../src/jinclude.h"
#include "../src/jpeglib.h"
#include "../src/jdct.h"
#include "../src/jsimddct.h"
#include "../src/jsimd.h"
#include "../src/jpegapicomp.h"


#define C_COVERAGE_TEST(f) \
  printf(#f " -- %s\n", f() ? "YES" : "no")

#define D_COVERAGE_TEST(f) \
  printf(#f " -- %s\n", f() ? "YES" : "no")


int main(void)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_decompress_struct dinfo;
  struct jpeg_error_mgr jerr;

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  jinit_color_converter(&cinfo);
  jinit_downsampler(&cinfo);
  jinit_huff_encoder(&cinfo);

  dinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&dinfo);
  dinfo.num_components = 3;
  dinfo.jpeg_color_space = JCS_YCbCr;
  dinfo.out_color_space = JCS_RGB;
  jinit_color_deconverter(&dinfo);
  dinfo.comp_info = (jpeg_component_info *)(*dinfo.mem->alloc_small)
    ((j_common_ptr)&dinfo, JPOOL_IMAGE,
    dinfo.num_components * sizeof(jpeg_component_info));
  dinfo.comp_info[0].component_id = 0;
  dinfo.comp_info[0].h_samp_factor = dinfo.comp_info[0].v_samp_factor = 1;
  dinfo.comp_info[1].component_id = 1;
  dinfo.comp_info[1].h_samp_factor = dinfo.comp_info[1].v_samp_factor = 1;
  dinfo.comp_info[2].component_id = 2;
  dinfo.comp_info[2].h_samp_factor = dinfo.comp_info[2].v_samp_factor = 1;
  dinfo._min_DCT_scaled_size = DCTSIZE;
  jinit_upsampler(&dinfo);
#if defined(DCT_ISLOW_SUPPORTED) || defined(DCT_IFAST_SUPPORTED) || \
    defined(DCT_FLOAT_SUPPORTED)
  jinit_inverse_dct(&dinfo);
#endif

  C_COVERAGE_TEST(jsimd_can_rgb_ycc);
  C_COVERAGE_TEST(jsimd_can_rgb_gray);
  D_COVERAGE_TEST(jsimd_can_ycc_rgb);
  D_COVERAGE_TEST(jsimd_can_ycc_rgb565);
  C_COVERAGE_TEST(jsimd_can_h2v1_downsample);
  C_COVERAGE_TEST(jsimd_can_h2v2_downsample);
#ifdef __mips__
  C_COVERAGE_TEST(jsimd_can_h2v2_smooth_downsample);
#endif
  D_COVERAGE_TEST(jsimd_can_h2v1_upsample);
  D_COVERAGE_TEST(jsimd_can_h2v2_upsample);
#ifdef __mips__
  D_COVERAGE_TEST(jsimd_can_int_upsample);
#endif
  D_COVERAGE_TEST(jsimd_can_h2v1_fancy_upsample);
  D_COVERAGE_TEST(jsimd_can_h2v2_fancy_upsample);
#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || \
    defined(_M_ARM64) || defined(_M_ARM64EC)
  D_COVERAGE_TEST(jsimd_can_h1v2_fancy_upsample);
#endif
  D_COVERAGE_TEST(jsimd_can_h2v1_merged_upsample);
  D_COVERAGE_TEST(jsimd_can_h2v2_merged_upsample);
  C_COVERAGE_TEST(jsimd_can_convsamp);
  C_COVERAGE_TEST(jsimd_can_convsamp_float);
  C_COVERAGE_TEST(jsimd_can_fdct_islow);
  C_COVERAGE_TEST(jsimd_can_fdct_ifast);
  C_COVERAGE_TEST(jsimd_can_fdct_float);
  C_COVERAGE_TEST(jsimd_can_quantize);
  C_COVERAGE_TEST(jsimd_can_quantize_float);
  D_COVERAGE_TEST(jsimd_can_idct_islow);
  D_COVERAGE_TEST(jsimd_can_idct_ifast);
  D_COVERAGE_TEST(jsimd_can_idct_float);
  D_COVERAGE_TEST(jsimd_can_idct_2x2);
  D_COVERAGE_TEST(jsimd_can_idct_4x4);
#ifdef __mips__
  D_COVERAGE_TEST(jsimd_can_idct_6x6);
  D_COVERAGE_TEST(jsimd_can_idct_12x12);
#endif
  C_COVERAGE_TEST(jsimd_can_huff_encode_one_block);
  C_COVERAGE_TEST(jsimd_can_encode_mcu_AC_first_prepare);
  C_COVERAGE_TEST(jsimd_can_encode_mcu_AC_refine_prepare);

  jpeg_abort_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  jpeg_abort_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);

  return 0;
}
