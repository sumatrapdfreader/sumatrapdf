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

#include <string.h>

#include "common/intops.h"

#include "src/ctx.h"
#include "src/levels.h"
#include "src/lf_mask.h"
#include "src/tables.h"

static void decomp_tx(uint8_t (*const txa)[2 /* txsz, step */][32 /* y */][32 /* x */],
                      const enum RectTxfmSize from,
                      const int depth,
                      const int y_off, const int x_off,
                      const uint16_t *const tx_masks)
{
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[from];
    const int is_split = (from == (int) TX_4X4 || depth > 1) ? 0 :
        (tx_masks[depth] >> (y_off * 4 + x_off)) & 1;

    if (is_split) {
        const enum RectTxfmSize sub = t_dim->sub;
        const int htw4 = t_dim->w >> 1, hth4 = t_dim->h >> 1;

        decomp_tx(txa, sub, depth + 1, y_off * 2 + 0, x_off * 2 + 0, tx_masks);
        if (t_dim->w >= t_dim->h)
            decomp_tx((uint8_t(*)[2][32][32]) &txa[0][0][0][htw4],
                      sub, depth + 1, y_off * 2 + 0, x_off * 2 + 1, tx_masks);
        if (t_dim->h >= t_dim->w) {
            decomp_tx((uint8_t(*)[2][32][32]) &txa[0][0][hth4][0],
                      sub, depth + 1, y_off * 2 + 1, x_off * 2 + 0, tx_masks);
            if (t_dim->w >= t_dim->h)
                decomp_tx((uint8_t(*)[2][32][32]) &txa[0][0][hth4][htw4],
                          sub, depth + 1, y_off * 2 + 1, x_off * 2 + 1, tx_masks);
        }
    } else {
        const int lw = imin(2, t_dim->lw), lh = imin(2, t_dim->lh);

#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        for (int y = 0; y < t_dim->h; y++) { \
            rep_macro(type, txa[0][0][y], off, mul * lw); \
            rep_macro(type, txa[1][0][y], off, mul * lh); \
            txa[0][1][y][0] = t_dim->w; \
        }
        case_set_upto16(t_dim->w,,, 0);
#undef set_ctx
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, txa[1][1][0], off, mul * t_dim->h)
        case_set_upto16(t_dim->w,,, 0);
#undef set_ctx
    }
}

static inline void mask_edges_inter(uint16_t (*const masks)[32][3][2],
                                    const int by4, const int bx4,
                                    const int w4, const int h4, const int skip,
                                    const enum RectTxfmSize max_tx,
                                    const uint16_t *const tx_masks,
                                    uint8_t *const a, uint8_t *const l)
{
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[max_tx];
    int y, x;

    ALIGN_STK_16(uint8_t, txa, 2 /* edge */, [2 /* txsz, step */][32 /* y */][32 /* x */]);
    for (int y_off = 0, y = 0; y < h4; y += t_dim->h, y_off++)
        for (int x_off = 0, x = 0; x < w4; x += t_dim->w, x_off++)
            decomp_tx((uint8_t(*)[2][32][32]) &txa[0][0][y][x],
                      max_tx, 0, y_off, x_off, tx_masks);

    // left block edge
    unsigned mask = 1U << by4;
    for (y = 0; y < h4; y++, mask <<= 1) {
        const int sidx = mask >= 0x10000;
        const unsigned smask = mask >> (sidx << 4);
        masks[0][bx4][imin(txa[0][0][y][0], l[y])][sidx] |= smask;
    }

    // top block edge
    for (x = 0, mask = 1U << bx4; x < w4; x++, mask <<= 1) {
        const int sidx = mask >= 0x10000;
        const unsigned smask = mask >> (sidx << 4);
        masks[1][by4][imin(txa[1][0][0][x], a[x])][sidx] |= smask;
    }

    if (!skip) {
        // inner (tx) left|right edges
        for (y = 0, mask = 1U << by4; y < h4; y++, mask <<= 1) {
            const int sidx = mask >= 0x10000U;
            const unsigned smask = mask >> (sidx << 4);
            int ltx = txa[0][0][y][0];
            int step = txa[0][1][y][0];
            for (x = step; x < w4; x += step) {
                const int rtx = txa[0][0][y][x];
                masks[0][bx4 + x][imin(rtx, ltx)][sidx] |= smask;
                ltx = rtx;
                step = txa[0][1][y][x];
            }
        }

        //            top
        // inner (tx) --- edges
        //           bottom
        for (x = 0, mask = 1U << bx4; x < w4; x++, mask <<= 1) {
            const int sidx = mask >= 0x10000U;
            const unsigned smask = mask >> (sidx << 4);
            int ttx = txa[1][0][0][x];
            int step = txa[1][1][0][x];
            for (y = step; y < h4; y += step) {
                const int btx = txa[1][0][y][x];
                masks[1][by4 + y][imin(ttx, btx)][sidx] |= smask;
                ttx = btx;
                step = txa[1][1][y][x];
            }
        }
    }

    for (y = 0; y < h4; y++)
        l[y] = txa[0][0][y][w4 - 1];
    memcpy(a, txa[1][0][h4 - 1], w4);
}

static inline void mask_edges_intra(uint16_t (*const masks)[32][3][2],
                                    const int by4, const int bx4,
                                    const int w4, const int h4,
                                    const enum RectTxfmSize tx,
                                    uint8_t *const a, uint8_t *const l)
{
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[tx];
    const int twl4 = t_dim->lw, thl4 = t_dim->lh;
    const int twl4c = imin(2, twl4), thl4c = imin(2, thl4);
    int y, x;

    // left block edge
    unsigned mask = 1U << by4;
    for (y = 0; y < h4; y++, mask <<= 1) {
        const int sidx = mask >= 0x10000;
        const unsigned smask = mask >> (sidx << 4);
        masks[0][bx4][imin(twl4c, l[y])][sidx] |= smask;
    }

    // top block edge
    for (x = 0, mask = 1U << bx4; x < w4; x++, mask <<= 1) {
        const int sidx = mask >= 0x10000;
        const unsigned smask = mask >> (sidx << 4);
        masks[1][by4][imin(thl4c, a[x])][sidx] |= smask;
    }

    // inner (tx) left|right edges
    const int hstep = t_dim->w;
    unsigned t = 1U << by4;
    unsigned inner = (unsigned) ((((uint64_t) t) << h4) - t);
    unsigned inner1 = inner & 0xffff, inner2 = inner >> 16;
    for (x = hstep; x < w4; x += hstep) {
        if (inner1) masks[0][bx4 + x][twl4c][0] |= inner1;
        if (inner2) masks[0][bx4 + x][twl4c][1] |= inner2;
    }

    //            top
    // inner (tx) --- edges
    //           bottom
    const int vstep = t_dim->h;
    t = 1U << bx4;
    inner = (unsigned) ((((uint64_t) t) << w4) - t);
    inner1 = inner & 0xffff;
    inner2 = inner >> 16;
    for (y = vstep; y < h4; y += vstep) {
        if (inner1) masks[1][by4 + y][thl4c][0] |= inner1;
        if (inner2) masks[1][by4 + y][thl4c][1] |= inner2;
    }

#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
    rep_macro(type, a, off, mul * thl4c)
#define default_memset(dir, diridx, off, var) \
    memset(a, thl4c, var)
    case_set_upto32_with_default(w4,,, 0);
#undef default_memset
#undef set_ctx
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
    rep_macro(type, l, off, mul * twl4c)
#define default_memset(dir, diridx, off, var) \
    memset(l, twl4c, var)
    case_set_upto32_with_default(h4,,, 0);
#undef default_memset
#undef set_ctx
}

static inline void mask_edges_chroma(uint16_t (*const masks)[32][2][2],
                                     const int cby4, const int cbx4,
                                     const int cw4, const int ch4,
                                     const int skip_inter,
                                     const enum RectTxfmSize tx,
                                     uint8_t *const a, uint8_t *const l,
                                     const int ss_hor, const int ss_ver)
{
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[tx];
    const int twl4 = t_dim->lw, thl4 = t_dim->lh;
    const int twl4c = !!twl4, thl4c = !!thl4;
    int y, x;
    const int vbits = 4 - ss_ver, hbits = 4 - ss_hor;
    const int vmask = 16 >> ss_ver, hmask = 16 >> ss_hor;
    const unsigned vmax = 1 << vmask, hmax = 1 << hmask;

    // left block edge
    unsigned mask = 1U << cby4;
    for (y = 0; y < ch4; y++, mask <<= 1) {
        const int sidx = mask >= vmax;
        const unsigned smask = mask >> (sidx << vbits);
        masks[0][cbx4][imin(twl4c, l[y])][sidx] |= smask;
    }

    // top block edge
    for (x = 0, mask = 1U << cbx4; x < cw4; x++, mask <<= 1) {
        const int sidx = mask >= hmax;
        const unsigned smask = mask >> (sidx << hbits);
        masks[1][cby4][imin(thl4c, a[x])][sidx] |= smask;
    }

    if (!skip_inter) {
        // inner (tx) left|right edges
        const int hstep = t_dim->w;
        unsigned t = 1U << cby4;
        unsigned inner = (unsigned) ((((uint64_t) t) << ch4) - t);
        unsigned inner1 = inner & ((1 << vmask) - 1), inner2 = inner >> vmask;
        for (x = hstep; x < cw4; x += hstep) {
            if (inner1) masks[0][cbx4 + x][twl4c][0] |= inner1;
            if (inner2) masks[0][cbx4 + x][twl4c][1] |= inner2;
        }

        //            top
        // inner (tx) --- edges
        //           bottom
        const int vstep = t_dim->h;
        t = 1U << cbx4;
        inner = (unsigned) ((((uint64_t) t) << cw4) - t);
        inner1 = inner & ((1 << hmask) - 1), inner2 = inner >> hmask;
        for (y = vstep; y < ch4; y += vstep) {
            if (inner1) masks[1][cby4 + y][thl4c][0] |= inner1;
            if (inner2) masks[1][cby4 + y][thl4c][1] |= inner2;
        }
    }

#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
    rep_macro(type, a, off, mul * thl4c)
#define default_memset(dir, diridx, off, var) \
    memset(a, thl4c, var)
    case_set_upto32_with_default(cw4,,, 0);
#undef default_memset
#undef set_ctx
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
    rep_macro(type, l, off, mul * twl4c)
#define default_memset(dir, diridx, off, var) \
    memset(l, twl4c, var)
    case_set_upto32_with_default(ch4,,, 0);
#undef default_memset
#undef set_ctx
}

void dav1d_create_lf_mask_intra(Av1Filter *const lflvl,
                                uint8_t (*const level_cache)[4],
                                const ptrdiff_t b4_stride,
                                const uint8_t (*filter_level)[8][2],
                                const int bx, const int by,
                                const int iw, const int ih,
                                const enum BlockSize bs,
                                const enum RectTxfmSize ytx,
                                const enum RectTxfmSize uvtx,
                                const enum Dav1dPixelLayout layout,
                                uint8_t *const ay, uint8_t *const ly,
                                uint8_t *const auv, uint8_t *const luv)
{
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = imin(iw - bx, b_dim[0]);
    const int bh4 = imin(ih - by, b_dim[1]);
    const int bx4 = bx & 31;
    const int by4 = by & 31;

    if (bw4 && bh4) {
        uint8_t (*level_cache_ptr)[4] = level_cache + by * b4_stride + bx;
        for (int y = 0; y < bh4; y++) {
            for (int x = 0; x < bw4; x++) {
                level_cache_ptr[x][0] = filter_level[0][0][0];
                level_cache_ptr[x][1] = filter_level[1][0][0];
            }
            level_cache_ptr += b4_stride;
        }

        mask_edges_intra(lflvl->filter_y, by4, bx4, bw4, bh4, ytx, ay, ly);
    }

    if (!auv) return;

    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbw4 = imin(((iw + ss_hor) >> ss_hor) - (bx >> ss_hor),
                          (b_dim[0] + ss_hor) >> ss_hor);
    const int cbh4 = imin(((ih + ss_ver) >> ss_ver) - (by >> ss_ver),
                          (b_dim[1] + ss_ver) >> ss_ver);

    if (!cbw4 || !cbh4) return;

    const int cbx4 = bx4 >> ss_hor;
    const int cby4 = by4 >> ss_ver;

    uint8_t (*level_cache_ptr)[4] =
        level_cache + (by >> ss_ver) * b4_stride + (bx >> ss_hor);
    for (int y = 0; y < cbh4; y++) {
        for (int x = 0; x < cbw4; x++) {
            level_cache_ptr[x][2] = filter_level[2][0][0];
            level_cache_ptr[x][3] = filter_level[3][0][0];
        }
        level_cache_ptr += b4_stride;
    }

    mask_edges_chroma(lflvl->filter_uv, cby4, cbx4, cbw4, cbh4, 0, uvtx,
                      auv, luv, ss_hor, ss_ver);
}

void dav1d_create_lf_mask_inter(Av1Filter *const lflvl,
                                uint8_t (*const level_cache)[4],
                                const ptrdiff_t b4_stride,
                                const uint8_t (*filter_level)[8][2],
                                const int bx, const int by,
                                const int iw, const int ih,
                                const int skip, const enum BlockSize bs,
                                const enum RectTxfmSize max_ytx,
                                const uint16_t *const tx_masks,
                                const enum RectTxfmSize uvtx,
                                const enum Dav1dPixelLayout layout,
                                uint8_t *const ay, uint8_t *const ly,
                                uint8_t *const auv, uint8_t *const luv)
{
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = imin(iw - bx, b_dim[0]);
    const int bh4 = imin(ih - by, b_dim[1]);
    const int bx4 = bx & 31;
    const int by4 = by & 31;

    if (bw4 && bh4) {
        uint8_t (*level_cache_ptr)[4] = level_cache + by * b4_stride + bx;
        for (int y = 0; y < bh4; y++) {
            for (int x = 0; x < bw4; x++) {
                level_cache_ptr[x][0] = filter_level[0][0][0];
                level_cache_ptr[x][1] = filter_level[1][0][0];
            }
            level_cache_ptr += b4_stride;
        }

        mask_edges_inter(lflvl->filter_y, by4, bx4, bw4, bh4, skip,
                         max_ytx, tx_masks, ay, ly);
    }

    if (!auv) return;

    const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbw4 = imin(((iw + ss_hor) >> ss_hor) - (bx >> ss_hor),
                          (b_dim[0] + ss_hor) >> ss_hor);
    const int cbh4 = imin(((ih + ss_ver) >> ss_ver) - (by >> ss_ver),
                          (b_dim[1] + ss_ver) >> ss_ver);

    if (!cbw4 || !cbh4) return;

    const int cbx4 = bx4 >> ss_hor;
    const int cby4 = by4 >> ss_ver;

    uint8_t (*level_cache_ptr)[4] =
        level_cache + (by >> ss_ver) * b4_stride + (bx >> ss_hor);
    for (int y = 0; y < cbh4; y++) {
        for (int x = 0; x < cbw4; x++) {
            level_cache_ptr[x][2] = filter_level[2][0][0];
            level_cache_ptr[x][3] = filter_level[3][0][0];
        }
        level_cache_ptr += b4_stride;
    }

    mask_edges_chroma(lflvl->filter_uv, cby4, cbx4, cbw4, cbh4, skip, uvtx,
                      auv, luv, ss_hor, ss_ver);
}

void dav1d_calc_eih(Av1FilterLUT *const lim_lut, const int filter_sharpness) {
    // set E/I/H values from loopfilter level
    const int sharp = filter_sharpness;
    for (int level = 0; level < 64; level++) {
        int limit = level;

        if (sharp > 0) {
            limit >>= (sharp + 3) >> 2;
            limit = imin(limit, 9 - sharp);
        }
        limit = imax(limit, 1);

        lim_lut->i[level] = limit;
        lim_lut->e[level] = 2 * (level + 2) + limit;
    }
    lim_lut->sharp[0] = (sharp + 3) >> 2;
    lim_lut->sharp[1] = sharp ? 9 - sharp : 0xff;
}

static inline void calc_lf_value(uint8_t (*const lflvl_values)[2],
                                 const int is_chroma, const int base_lvl,
                                 const int lf_delta, const int seg_delta,
                                 const Dav1dLoopfilterModeRefDeltas *const mr_delta)
{
    const int base = iclip(iclip(base_lvl + lf_delta, 0, 63) + seg_delta, 0, 63);

    if (!base_lvl && is_chroma) {
        memset(lflvl_values, 0, 8 * 2);
    } else if (!mr_delta) {
        memset(lflvl_values, base, 8 * 2);
    } else {
        const int sh = base >= 32;
        lflvl_values[0][0] = lflvl_values[0][1] =
            iclip(base + (mr_delta->ref_delta[0] * (1 << sh)), 0, 63);
        for (int r = 1; r < 8; r++) {
            for (int m = 0; m < 2; m++) {
                const int delta =
                    mr_delta->mode_delta[m] + mr_delta->ref_delta[r];
                lflvl_values[r][m] = iclip(base + (delta * (1 << sh)), 0, 63);
            }
        }
    }
}

void dav1d_calc_lf_values(uint8_t (*const lflvl_values)[4][8][2],
                          const Dav1dFrameHeader *const hdr,
                          const int8_t lf_delta[4])
{
    const int n_seg = hdr->segmentation.enabled ? 8 : 1;

    if (!hdr->loopfilter.level_y[0] && !hdr->loopfilter.level_y[1]) {
        memset(lflvl_values, 0, 8 * 4 * 2 * n_seg);
        return;
    }

    const Dav1dLoopfilterModeRefDeltas *const mr_deltas =
        hdr->loopfilter.mode_ref_delta_enabled ?
        &hdr->loopfilter.mode_ref_deltas : NULL;
    for (int s = 0; s < n_seg; s++) {
        const Dav1dSegmentationData *const segd =
            hdr->segmentation.enabled ? &hdr->segmentation.seg_data.d[s] : NULL;

        calc_lf_value(lflvl_values[s][0], 0, hdr->loopfilter.level_y[0],
                      lf_delta[0], segd ? segd->delta_lf_y_v : 0, mr_deltas);
        calc_lf_value(lflvl_values[s][1], 0, hdr->loopfilter.level_y[1],
                      lf_delta[hdr->delta.lf.multi ? 1 : 0],
                      segd ? segd->delta_lf_y_h : 0, mr_deltas);
        calc_lf_value(lflvl_values[s][2], 1, hdr->loopfilter.level_u,
                      lf_delta[hdr->delta.lf.multi ? 2 : 0],
                      segd ? segd->delta_lf_u : 0, mr_deltas);
        calc_lf_value(lflvl_values[s][3], 1, hdr->loopfilter.level_v,
                      lf_delta[hdr->delta.lf.multi ? 3 : 0],
                      segd ? segd->delta_lf_v : 0, mr_deltas);
    }
}
