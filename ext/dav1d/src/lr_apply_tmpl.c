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

#include "config.h"

#include <stdio.h>

#include "common/intops.h"

#include "src/lr_apply.h"

static void lr_stripe(const Dav1dFrameContext *const f, pixel *p,
                      const pixel (*left)[4], int x, int y,
                      const int plane, const int unit_w, const int row_h,
                      const Av1RestorationUnit *const lr, enum LrEdgeFlags edges)
{
    const Dav1dDSPContext *const dsp = f->dsp;
    const int chroma = !!plane;
    const int ss_ver = chroma & (f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420);
    const ptrdiff_t stride = f->sr_cur.p.stride[chroma];
    const int sby = (y + (y ? 8 << ss_ver : 0)) >> (6 - ss_ver + f->seq_hdr->sb128);
    const int have_tt = f->c->n_tc > 1;
    const pixel *lpf = f->lf.lr_lpf_line[plane] +
        have_tt * (sby * (4 << f->seq_hdr->sb128) - 4) * PXSTRIDE(stride) + x;

    // The first stripe of the frame is shorter by 8 luma pixel rows.
    int stripe_h = imin((64 - 8 * !y) >> ss_ver, row_h - y);

    looprestorationfilter_fn lr_fn;
    LooprestorationParams params;
    if (lr->type == DAV1D_RESTORATION_WIENER) {
        int16_t (*const filter)[8] = params.filter;
        filter[0][0] = filter[0][6] = lr->filter_h[0];
        filter[0][1] = filter[0][5] = lr->filter_h[1];
        filter[0][2] = filter[0][4] = lr->filter_h[2];
        filter[0][3] = -(filter[0][0] + filter[0][1] + filter[0][2]) * 2;
#if BITDEPTH != 8
        /* For 8-bit SIMD it's beneficial to handle the +128 separately
         * in order to avoid overflows. */
        filter[0][3] += 128;
#endif

        filter[1][0] = filter[1][6] = lr->filter_v[0];
        filter[1][1] = filter[1][5] = lr->filter_v[1];
        filter[1][2] = filter[1][4] = lr->filter_v[2];
        filter[1][3] = 128 - (filter[1][0] + filter[1][1] + filter[1][2]) * 2;

        lr_fn = dsp->lr.wiener[!(filter[0][0] | filter[1][0])];
    } else {
        assert(lr->type == DAV1D_RESTORATION_SGRPROJ);
        const uint16_t *const sgr_params = dav1d_sgr_params[lr->sgr_idx];
        params.sgr.s0 = sgr_params[0];
        params.sgr.s1 = sgr_params[1];
        params.sgr.w0 = lr->sgr_weights[0];
        params.sgr.w1 = 128 - (lr->sgr_weights[0] + lr->sgr_weights[1]);

        lr_fn = dsp->lr.sgr[!!sgr_params[0] + !!sgr_params[1] * 2 - 1];
    }

    while (y + stripe_h <= row_h) {
        // Change the HAVE_BOTTOM bit in edges to (sby + 1 != f->sbh || y + stripe_h != row_h)
        edges ^= (-(sby + 1 != f->sbh || y + stripe_h != row_h) ^ edges) & LR_HAVE_BOTTOM;
        lr_fn(p, stride, left, lpf, unit_w, stripe_h, &params, edges HIGHBD_CALL_SUFFIX);

        left += stripe_h;
        y += stripe_h;
        p += stripe_h * PXSTRIDE(stride);
        edges |= LR_HAVE_TOP;
        stripe_h = imin(64 >> ss_ver, row_h - y);
        if (stripe_h == 0) break;
        lpf += 4 * PXSTRIDE(stride);
    }
}

static void backup4xU(pixel (*dst)[4], const pixel *src, const ptrdiff_t src_stride,
                      int u)
{
    for (; u > 0; u--, dst++, src += PXSTRIDE(src_stride))
        pixel_copy(dst, src, 4);
}

static void lr_sbrow(const Dav1dFrameContext *const f, pixel *p, const int y,
                     const int w, const int h, const int row_h, const int plane)
{
    const int chroma = !!plane;
    const int ss_ver = chroma & (f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420);
    const int ss_hor = chroma & (f->sr_cur.p.p.layout != DAV1D_PIXEL_LAYOUT_I444);
    const ptrdiff_t p_stride = f->sr_cur.p.stride[chroma];

    const int unit_size_log2 = f->frame_hdr->restoration.unit_size[!!plane];
    const int unit_size = 1 << unit_size_log2;
    const int half_unit_size = unit_size >> 1;
    const int max_unit_size = unit_size + half_unit_size;

    // Y coordinate of the sbrow (y is 8 luma pixel rows above row_y)
    const int row_y = y + ((8 >> ss_ver) * !!y);

    // FIXME This is an ugly hack to lookup the proper AV1Filter unit for
    // chroma planes. Question: For Multithreaded decoding, is it better
    // to store the chroma LR information with collocated Luma information?
    // In other words. For a chroma restoration unit locate at 128,128 and
    // with a 4:2:0 chroma subsampling, do we store the filter information at
    // the AV1Filter unit located at (128,128) or (256,256)
    // TODO Support chroma subsampling.
    const int shift_hor = 7 - ss_hor;

    /* maximum sbrow height is 128 + 8 rows offset */
    ALIGN_STK_16(pixel, pre_lr_border, 2, [128 + 8][4]);
    const Av1RestorationUnit *lr[2];

    enum LrEdgeFlags edges = (y > 0 ? LR_HAVE_TOP : 0) | LR_HAVE_RIGHT;

    int aligned_unit_pos = row_y & ~(unit_size - 1);
    if (aligned_unit_pos && aligned_unit_pos + half_unit_size > h)
        aligned_unit_pos -= unit_size;
    aligned_unit_pos <<= ss_ver;
    const int sb_idx = (aligned_unit_pos >> 7) * f->sr_sb128w;
    const int unit_idx = ((aligned_unit_pos >> 6) & 1) << 1;
    lr[0] = &f->lf.lr_mask[sb_idx].lr[plane][unit_idx];
    int restore = lr[0]->type != DAV1D_RESTORATION_NONE;
    int x = 0, bit = 0;
    for (; x + max_unit_size <= w; p += unit_size, edges |= LR_HAVE_LEFT, bit ^= 1) {
        const int next_x = x + unit_size;
        const int next_u_idx = unit_idx + ((next_x >> (shift_hor - 1)) & 1);
        lr[!bit] =
            &f->lf.lr_mask[sb_idx + (next_x >> shift_hor)].lr[plane][next_u_idx];
        const int restore_next = lr[!bit]->type != DAV1D_RESTORATION_NONE;
        if (restore_next)
            backup4xU(pre_lr_border[bit], p + unit_size - 4, p_stride, row_h - y);
        if (restore)
            lr_stripe(f, p, pre_lr_border[!bit], x, y, plane, unit_size, row_h,
                      lr[bit], edges);
        x = next_x;
        restore = restore_next;
    }
    if (restore) {
        edges &= ~LR_HAVE_RIGHT;
        const int unit_w = w - x;
        lr_stripe(f, p, pre_lr_border[!bit], x, y, plane, unit_w, row_h, lr[bit], edges);
    }
}

void bytefn(dav1d_lr_sbrow)(Dav1dFrameContext *const f, pixel *const dst[3],
                            const int sby)
{
    const int offset_y = 8 * !!sby;
    const ptrdiff_t *const dst_stride = f->sr_cur.p.stride;
    const int restore_planes = f->lf.restore_planes;
    const int not_last = sby + 1 < f->sbh;

    if (restore_planes & LR_RESTORE_Y) {
        const int h = f->sr_cur.p.p.h;
        const int w = f->sr_cur.p.p.w;
        const int next_row_y = (sby + 1) << (6 + f->seq_hdr->sb128);
        const int row_h = imin(next_row_y - 8 * not_last, h);
        const int y_stripe = (sby << (6 + f->seq_hdr->sb128)) - offset_y;
        lr_sbrow(f, dst[0] - offset_y * PXSTRIDE(dst_stride[0]), y_stripe, w,
                 h, row_h, 0);
    }
    if (restore_planes & (LR_RESTORE_U | LR_RESTORE_V)) {
        const int ss_ver = f->sr_cur.p.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = f->sr_cur.p.p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int h = (f->sr_cur.p.p.h + ss_ver) >> ss_ver;
        const int w = (f->sr_cur.p.p.w + ss_hor) >> ss_hor;
        const int next_row_y = (sby + 1) << ((6 - ss_ver) + f->seq_hdr->sb128);
        const int row_h = imin(next_row_y - (8 >> ss_ver) * not_last, h);
        const int offset_uv = offset_y >> ss_ver;
        const int y_stripe = (sby << ((6 - ss_ver) + f->seq_hdr->sb128)) - offset_uv;
        if (restore_planes & LR_RESTORE_U)
            lr_sbrow(f, dst[1] - offset_uv * PXSTRIDE(dst_stride[1]), y_stripe,
                     w, h, row_h, 1);

        if (restore_planes & LR_RESTORE_V)
            lr_sbrow(f, dst[2] - offset_uv * PXSTRIDE(dst_stride[1]), y_stripe,
                     w, h, row_h, 2);
    }
}
