/*
 * Copyright © 2018, VideoLAN and dav1d authors
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

#include "src/cpu.h"
#include "src/ipred.h"

decl_angular_ipred_fn(BF(dav1d_ipred_dc, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_dc_128, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_dc_top, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_dc_left, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_h, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_v, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_paeth, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_smooth, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_smooth_v, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_smooth_h, neon));
decl_angular_ipred_fn(BF(dav1d_ipred_filter, neon));

decl_cfl_pred_fn(BF(dav1d_ipred_cfl, neon));
decl_cfl_pred_fn(BF(dav1d_ipred_cfl_128, neon));
decl_cfl_pred_fn(BF(dav1d_ipred_cfl_top, neon));
decl_cfl_pred_fn(BF(dav1d_ipred_cfl_left, neon));

decl_cfl_ac_fn(BF(dav1d_ipred_cfl_ac_420, neon));
decl_cfl_ac_fn(BF(dav1d_ipred_cfl_ac_422, neon));
decl_cfl_ac_fn(BF(dav1d_ipred_cfl_ac_444, neon));

decl_pal_pred_fn(BF(dav1d_pal_pred, neon));

COLD void bitfn(dav1d_intra_pred_dsp_init_arm)(Dav1dIntraPredDSPContext *const c) {
    const unsigned flags = dav1d_get_cpu_flags();

    if (!(flags & DAV1D_ARM_CPU_FLAG_NEON)) return;

    c->intra_pred[DC_PRED]       = BF(dav1d_ipred_dc, neon);
    c->intra_pred[DC_128_PRED]   = BF(dav1d_ipred_dc_128, neon);
    c->intra_pred[TOP_DC_PRED]   = BF(dav1d_ipred_dc_top, neon);
    c->intra_pred[LEFT_DC_PRED]  = BF(dav1d_ipred_dc_left, neon);
    c->intra_pred[HOR_PRED]      = BF(dav1d_ipred_h, neon);
    c->intra_pred[VERT_PRED]     = BF(dav1d_ipred_v, neon);
    c->intra_pred[PAETH_PRED]    = BF(dav1d_ipred_paeth, neon);
    c->intra_pred[SMOOTH_PRED]   = BF(dav1d_ipred_smooth, neon);
    c->intra_pred[SMOOTH_V_PRED] = BF(dav1d_ipred_smooth_v, neon);
    c->intra_pred[SMOOTH_H_PRED] = BF(dav1d_ipred_smooth_h, neon);
    c->intra_pred[FILTER_PRED]   = BF(dav1d_ipred_filter, neon);

    c->cfl_pred[DC_PRED]         = BF(dav1d_ipred_cfl, neon);
    c->cfl_pred[DC_128_PRED]     = BF(dav1d_ipred_cfl_128, neon);
    c->cfl_pred[TOP_DC_PRED]     = BF(dav1d_ipred_cfl_top, neon);
    c->cfl_pred[LEFT_DC_PRED]    = BF(dav1d_ipred_cfl_left, neon);

    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I420 - 1] = BF(dav1d_ipred_cfl_ac_420, neon);
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I422 - 1] = BF(dav1d_ipred_cfl_ac_422, neon);
    c->cfl_ac[DAV1D_PIXEL_LAYOUT_I444 - 1] = BF(dav1d_ipred_cfl_ac_444, neon);

    c->pal_pred                  = BF(dav1d_pal_pred, neon);
}
