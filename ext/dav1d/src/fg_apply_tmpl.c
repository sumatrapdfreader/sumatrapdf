/*
 * Copyright © 2018, Niklas Haas
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

#include <stdint.h>

#include "dav1d/common.h"
#include "dav1d/picture.h"

#include "common/intops.h"
#include "common/bitdepth.h"

#include "src/fg_apply.h"

static void generate_scaling(const int bitdepth,
                             const uint8_t points[][2], const int num,
                             uint8_t scaling[SCALING_SIZE])
{
#if BITDEPTH == 8
    const int shift_x = 0;
    const int scaling_size = SCALING_SIZE;
#else
    assert(bitdepth > 8);
    const int shift_x = bitdepth - 8;
    const int scaling_size = 1 << bitdepth;
#endif

    // Fill up the preceding entries with the initial value
    memset(scaling, points[0][1], points[0][0] << shift_x);

    // Linearly interpolate the values in the middle
    for (int i = 0; i < num - 1; i++) {
        const int bx = points[i][0];
        const int by = points[i][1];
        const int ex = points[i+1][0];
        const int ey = points[i+1][1];
        const int dx = ex - bx;
        const int dy = ey - by;
        assert(dx > 0);
        const int delta = dy * ((0x10000 + (dx >> 1)) / dx);
        for (int x = 0, d = 0x8000; x < dx; x++) {
            scaling[(bx + x) << shift_x] = by + (d >> 16);
            d += delta;
        }
    }

    // Fill up the remaining entries with the final value
    const int n = points[num - 1][0] << shift_x;
    memset(&scaling[n], points[num - 1][1], scaling_size - n);

#if BITDEPTH != 8
    const int pad = 1 << shift_x, rnd = pad >> 1;
    for (int i = 0; i < num - 1; i++) {
        const int bx = points[i][0] << shift_x;
        const int ex = points[i+1][0] << shift_x;
        const int dx = ex - bx;
        for (int x = 0; x < dx; x += pad) {
            const int range = scaling[bx + x + pad] - scaling[bx + x];
            for (int n = 1, r = rnd; n < pad; n++) {
                r += range;
                scaling[bx + x + n] = scaling[bx + x] + (r >> shift_x);
            }
        }
    }
#endif
}

#ifndef UNIT_TEST
void bitfn(dav1d_prep_grain)(const Dav1dFilmGrainDSPContext *const dsp,
                             Dav1dPicture *const out,
                             const Dav1dPicture *const in,
                             uint8_t scaling[3][SCALING_SIZE],
                             entry grain_lut[3][GRAIN_HEIGHT+1][GRAIN_WIDTH])
{
    const Dav1dFilmGrainData *const data = &out->frame_hdr->film_grain.data;
#if BITDEPTH != 8
    const int bitdepth_max = (1 << out->p.bpc) - 1;
#endif

    // Generate grain LUTs as needed
    dsp->generate_grain_y(grain_lut[0], data HIGHBD_TAIL_SUFFIX); // always needed
    if (data->num_uv_points[0] || data->chroma_scaling_from_luma)
        dsp->generate_grain_uv[in->p.layout - 1](grain_lut[1], grain_lut[0],
                                                 data, 0 HIGHBD_TAIL_SUFFIX);
    if (data->num_uv_points[1] || data->chroma_scaling_from_luma)
        dsp->generate_grain_uv[in->p.layout - 1](grain_lut[2], grain_lut[0],
                                                 data, 1 HIGHBD_TAIL_SUFFIX);

    // Generate scaling LUTs as needed
    if (data->num_y_points)
        generate_scaling(in->p.bpc, data->y_points, data->num_y_points, scaling[0]);
    if (data->num_uv_points[0])
        generate_scaling(in->p.bpc, data->uv_points[0], data->num_uv_points[0], scaling[1]);
    if (data->num_uv_points[1])
        generate_scaling(in->p.bpc, data->uv_points[1], data->num_uv_points[1], scaling[2]);

    // Copy over the non-modified planes
    // TODO: eliminate in favor of per-plane refs
    assert(out->stride[0] == in->stride[0]);
    if (!data->num_y_points) {
        const ptrdiff_t stride = out->stride[0];
        const ptrdiff_t sz = out->p.h * stride;
        if (sz < 0)
            memcpy((uint8_t*) out->data[0] + sz - stride,
                   (uint8_t*) in->data[0] + sz - stride, -sz);
        else
            memcpy(out->data[0], in->data[0], sz);
    }

    if (in->p.layout != DAV1D_PIXEL_LAYOUT_I400 && !data->chroma_scaling_from_luma) {
        assert(out->stride[1] == in->stride[1]);
        const int ss_ver = in->p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const ptrdiff_t stride = out->stride[1];
        const ptrdiff_t sz = ((out->p.h + ss_ver) >> ss_ver) * stride;
        if (sz < 0) {
            if (!data->num_uv_points[0])
                memcpy((uint8_t*) out->data[1] + sz - stride,
                       (uint8_t*) in->data[1] + sz - stride, -sz);
            if (!data->num_uv_points[1])
                memcpy((uint8_t*) out->data[2] + sz - stride,
                       (uint8_t*) in->data[2] + sz - stride, -sz);
        } else {
            if (!data->num_uv_points[0])
                memcpy(out->data[1], in->data[1], sz);
            if (!data->num_uv_points[1])
                memcpy(out->data[2], in->data[2], sz);
        }
    }
}

void bitfn(dav1d_apply_grain_row)(const Dav1dFilmGrainDSPContext *const dsp,
                                  Dav1dPicture *const out,
                                  const Dav1dPicture *const in,
                                  const uint8_t scaling[3][SCALING_SIZE],
                                  const entry grain_lut[3][GRAIN_HEIGHT+1][GRAIN_WIDTH],
                                  const int row)
{
    // Synthesize grain for the affected planes
    const Dav1dFilmGrainData *const data = &out->frame_hdr->film_grain.data;
    const int ss_y = in->p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_x = in->p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cpw = (out->p.w + ss_x) >> ss_x;
    const int is_id = out->seq_hdr->mtrx == DAV1D_MC_IDENTITY;
    pixel *const luma_src =
        ((pixel *) in->data[0]) + row * BLOCK_SIZE * PXSTRIDE(in->stride[0]);
#if BITDEPTH != 8
    const int bitdepth_max = (1 << out->p.bpc) - 1;
#endif

    if (data->num_y_points) {
        const int bh = imin(out->p.h - row * BLOCK_SIZE, BLOCK_SIZE);
        dsp->fgy_32x32xn(((pixel *) out->data[0]) + row * BLOCK_SIZE * PXSTRIDE(out->stride[0]),
                         luma_src, out->stride[0], data,
                         out->p.w, scaling[0], grain_lut[0], bh, row HIGHBD_TAIL_SUFFIX);
    }

    if (!data->num_uv_points[0] && !data->num_uv_points[1] &&
        !data->chroma_scaling_from_luma)
    {
        return;
    }

    const int bh = (imin(out->p.h - row * BLOCK_SIZE, BLOCK_SIZE) + ss_y) >> ss_y;

    // extend padding pixels
    if (out->p.w & ss_x) {
        pixel *ptr = luma_src;
        for (int y = 0; y < bh; y++) {
            ptr[out->p.w] = ptr[out->p.w - 1];
            ptr += PXSTRIDE(in->stride[0]) << ss_y;
        }
    }

    const ptrdiff_t uv_off = row * BLOCK_SIZE * PXSTRIDE(out->stride[1]) >> ss_y;
    if (data->chroma_scaling_from_luma) {
        for (int pl = 0; pl < 2; pl++)
            dsp->fguv_32x32xn[in->p.layout - 1](((pixel *) out->data[1 + pl]) + uv_off,
                                                ((const pixel *) in->data[1 + pl]) + uv_off,
                                                in->stride[1], data, cpw,
                                                scaling[0], grain_lut[1 + pl],
                                                bh, row, luma_src, in->stride[0],
                                                pl, is_id HIGHBD_TAIL_SUFFIX);
    } else {
        for (int pl = 0; pl < 2; pl++)
            if (data->num_uv_points[pl])
                dsp->fguv_32x32xn[in->p.layout - 1](((pixel *) out->data[1 + pl]) + uv_off,
                                                    ((const pixel *) in->data[1 + pl]) + uv_off,
                                                    in->stride[1], data, cpw,
                                                    scaling[1 + pl], grain_lut[1 + pl],
                                                    bh, row, luma_src, in->stride[0],
                                                    pl, is_id HIGHBD_TAIL_SUFFIX);
    }
}

void bitfn(dav1d_apply_grain)(const Dav1dFilmGrainDSPContext *const dsp,
                              Dav1dPicture *const out,
                              const Dav1dPicture *const in)
{
    ALIGN_STK_16(entry, grain_lut, 3,[GRAIN_HEIGHT + 1][GRAIN_WIDTH]);
#if ARCH_X86_64 && BITDEPTH == 8
    ALIGN_STK_64(uint8_t, scaling, 3,[SCALING_SIZE]);
#else
    uint8_t scaling[3][SCALING_SIZE];
#endif
    const int rows = (out->p.h + 31) >> 5;

    bitfn(dav1d_prep_grain)(dsp, out, in, scaling, grain_lut);
    for (int row = 0; row < rows; row++)
        bitfn(dav1d_apply_grain_row)(dsp, out, in, scaling, grain_lut, row);
}
#endif
