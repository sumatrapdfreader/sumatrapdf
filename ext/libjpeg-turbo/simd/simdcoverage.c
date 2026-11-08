/*
 * Copyright (C) 2025-2026, D. R. Commander.
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
#include "jsimddct.h"
#include "jsimd.h"
#include "../src/jpegapicomp.h"

static const char *instrset_name(int instrset) {
  switch (instrset) {
    case JSIMD_NONE:
      return "none";
    case JSIMD_MMX:
      return "MMX";
    case JSIMD_3DNOW:
      return "3DNow!";
    case JSIMD_SSE:
      return "SSE";
    case JSIMD_SSE2:
      return "SSE2";
    case JSIMD_NEON:
      return "Neon";
    case JSIMD_RVV:
      return "RVV";
    case JSIMD_ALTIVEC:
      return "AltiVec";
    case JSIMD_AVX2:
      return "AVX2";
    case JSIMD_MMI:
      return "MMI";
    default:
      return "Unknown";
  }
}


#define C_COVERAGE_TEST(f) \
  printf(#f " -- %s\n", instrset_name(f(&cinfo)))

#define C_COVERAGE_TEST2(f, ptr) \
  printf(#f " -- %s\n", instrset_name(f(&cinfo, &ptr)))

#define D_COVERAGE_TEST(f) \
  printf(#f " -- %s\n", instrset_name(f(&dinfo)))

#define D_COVERAGE_TEST2(f, ptr) \
  printf(#f " -- %s\n", instrset_name(f(&dinfo, &ptr)))


int main(void)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_decompress_struct dinfo;
  struct jpeg_error_mgr jerr;

  convsamp_method_ptr convsamp_method;
  float_convsamp_method_ptr convsamp_float_method;
  forward_DCT_method_ptr fdct_method;
  float_DCT_method_ptr fdct_float_method;
  quantize_method_ptr quantize_method;
  float_quantize_method_ptr quantize_float_method;
  void (*encode_mcu_AC_first_method)
    (const JCOEF *, const int *, int, int, UJCOEF *, size_t *);
  int (*encode_mcu_AC_refine_method)
    (const JCOEF *, const int *, int, int, UJCOEF *, size_t *);

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

  C_COVERAGE_TEST(jsimd_set_rgb_ycc);
  C_COVERAGE_TEST(jsimd_set_rgb_gray);
  D_COVERAGE_TEST(jsimd_set_ycc_rgb);
  D_COVERAGE_TEST(jsimd_set_ycc_rgb565);
  C_COVERAGE_TEST(jsimd_set_h2v1_downsample);
  C_COVERAGE_TEST(jsimd_set_h2v2_downsample);
  D_COVERAGE_TEST(jsimd_set_h2v1_upsample);
  D_COVERAGE_TEST(jsimd_set_h2v2_upsample);
  D_COVERAGE_TEST(jsimd_set_h2v1_fancy_upsample);
  D_COVERAGE_TEST(jsimd_set_h2v2_fancy_upsample);
#if SIMD_ARCHITECTURE == ARM || SIMD_ARCHITECTURE == ARM64
  D_COVERAGE_TEST(jsimd_set_h1v2_fancy_upsample);
#endif
  D_COVERAGE_TEST(jsimd_set_h2v1_merged_upsample);
  D_COVERAGE_TEST(jsimd_set_h2v2_merged_upsample);
  C_COVERAGE_TEST2(jsimd_set_convsamp, convsamp_method);
  C_COVERAGE_TEST2(jsimd_set_convsamp_float, convsamp_float_method);
  C_COVERAGE_TEST2(jsimd_set_fdct_islow, fdct_method);
  C_COVERAGE_TEST2(jsimd_set_fdct_ifast, fdct_method);
  C_COVERAGE_TEST2(jsimd_set_fdct_float, fdct_float_method);
  C_COVERAGE_TEST2(jsimd_set_quantize, quantize_method);
  C_COVERAGE_TEST2(jsimd_set_quantize_float, quantize_float_method);
  D_COVERAGE_TEST(jsimd_set_idct_islow);
  D_COVERAGE_TEST(jsimd_set_idct_ifast);
  D_COVERAGE_TEST(jsimd_set_idct_float);
  D_COVERAGE_TEST(jsimd_set_idct_2x2);
  D_COVERAGE_TEST(jsimd_set_idct_4x4);
  C_COVERAGE_TEST(jsimd_set_huff_encode_one_block);
  C_COVERAGE_TEST2(jsimd_set_encode_mcu_AC_first_prepare,
                   encode_mcu_AC_first_method);
  C_COVERAGE_TEST2(jsimd_set_encode_mcu_AC_refine_prepare,
                   encode_mcu_AC_refine_method);

  jpeg_abort_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);

  jpeg_abort_decompress(&dinfo);
  jpeg_destroy_decompress(&dinfo);

  return 0;
}
