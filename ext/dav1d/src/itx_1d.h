/*
 * Copyright © 2018-2019, VideoLAN and dav1d authors
 * Copyright © 2018-2019, Two Orioles, LLC
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

#include <stddef.h>
#include <stdint.h>

#ifndef DAV1D_SRC_ITX_1D_H
#define DAV1D_SRC_ITX_1D_H

#define decl_itx_1d_fn(name) \
void (name)(int32_t *c, ptrdiff_t stride, int min, int max)
typedef decl_itx_1d_fn(*itx_1d_fn);

decl_itx_1d_fn(dav1d_inv_dct4_1d_c);
decl_itx_1d_fn(dav1d_inv_dct8_1d_c);
decl_itx_1d_fn(dav1d_inv_dct16_1d_c);
decl_itx_1d_fn(dav1d_inv_dct32_1d_c);
decl_itx_1d_fn(dav1d_inv_dct64_1d_c);

decl_itx_1d_fn(dav1d_inv_adst4_1d_c);
decl_itx_1d_fn(dav1d_inv_adst8_1d_c);
decl_itx_1d_fn(dav1d_inv_adst16_1d_c);

decl_itx_1d_fn(dav1d_inv_flipadst4_1d_c);
decl_itx_1d_fn(dav1d_inv_flipadst8_1d_c);
decl_itx_1d_fn(dav1d_inv_flipadst16_1d_c);

decl_itx_1d_fn(dav1d_inv_identity4_1d_c);
decl_itx_1d_fn(dav1d_inv_identity8_1d_c);
decl_itx_1d_fn(dav1d_inv_identity16_1d_c);
decl_itx_1d_fn(dav1d_inv_identity32_1d_c);

void dav1d_inv_wht4_1d_c(int32_t *c, ptrdiff_t stride);

#endif /* DAV1D_SRC_ITX_1D_H */
