/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DAV1D_TESTS_CHECKASM_INTERNAL_H
#define DAV1D_TESTS_CHECKASM_INTERNAL_H

#include "config.h"

#include "common/intops.h"

#include <checkasm/test.h>
#include <checkasm/utils.h>

#define rnd checkasm_rand

#define decl_check_bitfns(name) \
    name##_8bpc(void); \
    name##_16bpc(void)

void checkasm_check_msac(void);
void checkasm_check_pal(void);
void checkasm_check_refmvs(void);
decl_check_bitfns(void checkasm_check_cdef);
decl_check_bitfns(void checkasm_check_filmgrain);
decl_check_bitfns(void checkasm_check_ipred);
decl_check_bitfns(void checkasm_check_itx);
decl_check_bitfns(void checkasm_check_loopfilter);
decl_check_bitfns(void checkasm_check_looprestoration);
decl_check_bitfns(void checkasm_check_mc);

#ifdef BITDEPTH
    #define checkasm_check_impl_pixel checkasm_check_impl(PIXEL_TYPE)
    #define checkasm_check_pixel(...) checkasm_check(PIXEL_TYPE, __VA_ARGS__)
    #define checkasm_check_coef(...)  checkasm_check(COEF_TYPE, __VA_ARGS__)

    #define PIXEL_RECT(name, w, h)            BUF_RECT(pixel, name, w, h)
    #define CLEAR_PIXEL_RECT                  CLEAR_BUF_RECT
    #define checkasm_check_pixel_padded       checkasm_check_rect_padded
    #define checkasm_check_pixel_padded_align checkasm_check_rect_padded_align
#endif

#endif /* DAV1D_TESTS_CHECKASM_INTERNAL_H */
