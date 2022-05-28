/*
 * Copyright © 2018-2021, VideoLAN and dav1d authors
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

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "dav1d/data.h"

#include "common/frame.h"
#include "common/intops.h"

#include "src/ctx.h"
#include "src/decode.h"
#include "src/dequant_tables.h"
#include "src/env.h"
#include "src/filmgrain.h"
#include "src/log.h"
#include "src/qm.h"
#include "src/recon.h"
#include "src/ref.h"
#include "src/tables.h"
#include "src/thread_task.h"
#include "src/warpmv.h"

static void init_quant_tables(const Dav1dSequenceHeader *const seq_hdr,
                              const Dav1dFrameHeader *const frame_hdr,
                              const int qidx, uint16_t (*dq)[3][2])
{
    for (int i = 0; i < (frame_hdr->segmentation.enabled ? 8 : 1); i++) {
        const int yac = frame_hdr->segmentation.enabled ?
            iclip_u8(qidx + frame_hdr->segmentation.seg_data.d[i].delta_q) : qidx;
        const int ydc = iclip_u8(yac + frame_hdr->quant.ydc_delta);
        const int uac = iclip_u8(yac + frame_hdr->quant.uac_delta);
        const int udc = iclip_u8(yac + frame_hdr->quant.udc_delta);
        const int vac = iclip_u8(yac + frame_hdr->quant.vac_delta);
        const int vdc = iclip_u8(yac + frame_hdr->quant.vdc_delta);

        dq[i][0][0] = dav1d_dq_tbl[seq_hdr->hbd][ydc][0];
        dq[i][0][1] = dav1d_dq_tbl[seq_hdr->hbd][yac][1];
        dq[i][1][0] = dav1d_dq_tbl[seq_hdr->hbd][udc][0];
        dq[i][1][1] = dav1d_dq_tbl[seq_hdr->hbd][uac][1];
        dq[i][2][0] = dav1d_dq_tbl[seq_hdr->hbd][vdc][0];
        dq[i][2][1] = dav1d_dq_tbl[seq_hdr->hbd][vac][1];
    }
}

static int read_mv_component_diff(Dav1dTaskContext *const t,
                                  CdfMvComponent *const mv_comp,
                                  const int have_fp)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    const int have_hp = f->frame_hdr->hp;
    const int sign = dav1d_msac_decode_bool_adapt(&ts->msac, mv_comp->sign);
    const int cl = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                                                    mv_comp->classes, 10);
    int up, fp, hp;

    if (!cl) {
        up = dav1d_msac_decode_bool_adapt(&ts->msac, mv_comp->class0);
        if (have_fp) {
            fp = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                                 mv_comp->class0_fp[up], 3);
            hp = have_hp ? dav1d_msac_decode_bool_adapt(&ts->msac,
                                                        mv_comp->class0_hp) : 1;
        } else {
            fp = 3;
            hp = 1;
        }
    } else {
        up = 1 << cl;
        for (int n = 0; n < cl; n++)
            up |= dav1d_msac_decode_bool_adapt(&ts->msac,
                                               mv_comp->classN[n]) << n;
        if (have_fp) {
            fp = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                                 mv_comp->classN_fp, 3);
            hp = have_hp ? dav1d_msac_decode_bool_adapt(&ts->msac,
                                                        mv_comp->classN_hp) : 1;
        } else {
            fp = 3;
            hp = 1;
        }
    }

    const int diff = ((up << 3) | (fp << 1) | hp) + 1;

    return sign ? -diff : diff;
}

static void read_mv_residual(Dav1dTaskContext *const t, mv *const ref_mv,
                             CdfMvContext *const mv_cdf, const int have_fp)
{
    switch (dav1d_msac_decode_symbol_adapt4(&t->ts->msac, t->ts->cdf.mv.joint,
                                            N_MV_JOINTS - 1))
    {
    case MV_JOINT_HV:
        ref_mv->y += read_mv_component_diff(t, &mv_cdf->comp[0], have_fp);
        ref_mv->x += read_mv_component_diff(t, &mv_cdf->comp[1], have_fp);
        break;
    case MV_JOINT_H:
        ref_mv->x += read_mv_component_diff(t, &mv_cdf->comp[1], have_fp);
        break;
    case MV_JOINT_V:
        ref_mv->y += read_mv_component_diff(t, &mv_cdf->comp[0], have_fp);
        break;
    default:
        break;
    }
}

static void read_tx_tree(Dav1dTaskContext *const t,
                         const enum RectTxfmSize from,
                         const int depth, uint16_t *const masks,
                         const int x_off, const int y_off)
{
    const Dav1dFrameContext *const f = t->f;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[from];
    const int txw = t_dim->lw, txh = t_dim->lh;
    int is_split;

    if (depth < 2 && from > (int) TX_4X4) {
        const int cat = 2 * (TX_64X64 - t_dim->max) - depth;
        const int a = t->a->tx[bx4] < txw;
        const int l = t->l.tx[by4] < txh;

        is_split = dav1d_msac_decode_bool_adapt(&t->ts->msac,
                       t->ts->cdf.m.txpart[cat][a + l]);
        if (is_split)
            masks[depth] |= 1 << (y_off * 4 + x_off);
    } else {
        is_split = 0;
    }

    if (is_split && t_dim->max > TX_8X8) {
        const enum RectTxfmSize sub = t_dim->sub;
        const TxfmInfo *const sub_t_dim = &dav1d_txfm_dimensions[sub];
        const int txsw = sub_t_dim->w, txsh = sub_t_dim->h;

        read_tx_tree(t, sub, depth + 1, masks, x_off * 2 + 0, y_off * 2 + 0);
        t->bx += txsw;
        if (txw >= txh && t->bx < f->bw)
            read_tx_tree(t, sub, depth + 1, masks, x_off * 2 + 1, y_off * 2 + 0);
        t->bx -= txsw;
        t->by += txsh;
        if (txh >= txw && t->by < f->bh) {
            read_tx_tree(t, sub, depth + 1, masks, x_off * 2 + 0, y_off * 2 + 1);
            t->bx += txsw;
            if (txw >= txh && t->bx < f->bw)
                read_tx_tree(t, sub, depth + 1, masks,
                             x_off * 2 + 1, y_off * 2 + 1);
            t->bx -= txsw;
        }
        t->by -= txsh;
    } else {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir tx, off, is_split ? TX_4X4 : mul * txh)
        case_set_upto16(t_dim->h, l., 1, by4);
#undef set_ctx
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir tx, off, is_split ? TX_4X4 : mul * txw)
        case_set_upto16(t_dim->w, a->, 0, bx4);
#undef set_ctx
    }
}

static int neg_deinterleave(int diff, int ref, int max) {
    if (!ref) return diff;
    if (ref >= (max - 1)) return max - diff - 1;
    if (2 * ref < max) {
        if (diff <= 2 * ref) {
            if (diff & 1)
                return ref + ((diff + 1) >> 1);
            else
                return ref - (diff >> 1);
        }
        return diff;
    } else {
        if (diff <= 2 * (max - ref - 1)) {
            if (diff & 1)
                return ref + ((diff + 1) >> 1);
            else
                return ref - (diff >> 1);
        }
        return max - (diff + 1);
    }
}

static void find_matching_ref(const Dav1dTaskContext *const t,
                              const enum EdgeFlags intra_edge_flags,
                              const int bw4, const int bh4,
                              const int w4, const int h4,
                              const int have_left, const int have_top,
                              const int ref, uint64_t masks[2])
{
    /*const*/ refmvs_block *const *r = &t->rt.r[(t->by & 31) + 5];
    int count = 0;
    int have_topleft = have_top && have_left;
    int have_topright = imax(bw4, bh4) < 32 &&
                        have_top && t->bx + bw4 < t->ts->tiling.col_end &&
                        (intra_edge_flags & EDGE_I444_TOP_HAS_RIGHT);

#define bs(rp) dav1d_block_dimensions[(rp)->bs]
#define matches(rp) ((rp)->ref.ref[0] == ref + 1 && (rp)->ref.ref[1] == -1)

    if (have_top) {
        const refmvs_block *r2 = &r[-1][t->bx];
        if (matches(r2)) {
            masks[0] |= 1;
            count = 1;
        }
        int aw4 = bs(r2)[0];
        if (aw4 >= bw4) {
            const int off = t->bx & (aw4 - 1);
            if (off) have_topleft = 0;
            if (aw4 - off > bw4) have_topright = 0;
        } else {
            unsigned mask = 1 << aw4;
            for (int x = aw4; x < w4; x += aw4) {
                r2 += aw4;
                if (matches(r2)) {
                    masks[0] |= mask;
                    if (++count >= 8) return;
                }
                aw4 = bs(r2)[0];
                mask <<= aw4;
            }
        }
    }
    if (have_left) {
        /*const*/ refmvs_block *const *r2 = r;
        if (matches(&r2[0][t->bx - 1])) {
            masks[1] |= 1;
            if (++count >= 8) return;
        }
        int lh4 = bs(&r2[0][t->bx - 1])[1];
        if (lh4 >= bh4) {
            if (t->by & (lh4 - 1)) have_topleft = 0;
        } else {
            unsigned mask = 1 << lh4;
            for (int y = lh4; y < h4; y += lh4) {
                r2 += lh4;
                if (matches(&r2[0][t->bx - 1])) {
                    masks[1] |= mask;
                    if (++count >= 8) return;
                }
                lh4 = bs(&r2[0][t->bx - 1])[1];
                mask <<= lh4;
            }
        }
    }
    if (have_topleft && matches(&r[-1][t->bx - 1])) {
        masks[1] |= 1ULL << 32;
        if (++count >= 8) return;
    }
    if (have_topright && matches(&r[-1][t->bx + bw4])) {
        masks[0] |= 1ULL << 32;
    }
#undef matches
}

static void derive_warpmv(const Dav1dTaskContext *const t,
                          const int bw4, const int bh4,
                          const uint64_t masks[2], const union mv mv,
                          Dav1dWarpedMotionParams *const wmp)
{
    int pts[8][2 /* in, out */][2 /* x, y */], np = 0;
    /*const*/ refmvs_block *const *r = &t->rt.r[(t->by & 31) + 5];

#define add_sample(dx, dy, sx, sy, rp) do { \
    pts[np][0][0] = 16 * (2 * dx + sx * bs(rp)[0]) - 8; \
    pts[np][0][1] = 16 * (2 * dy + sy * bs(rp)[1]) - 8; \
    pts[np][1][0] = pts[np][0][0] + (rp)->mv.mv[0].x; \
    pts[np][1][1] = pts[np][0][1] + (rp)->mv.mv[0].y; \
    np++; \
} while (0)

    // use masks[] to find the projectable motion vectors in the edges
    if ((unsigned) masks[0] == 1 && !(masks[1] >> 32)) {
        const int off = t->bx & (bs(&r[-1][t->bx])[0] - 1);
        add_sample(-off, 0, 1, -1, &r[-1][t->bx]);
    } else for (unsigned off = 0, xmask = (uint32_t) masks[0]; np < 8 && xmask;) { // top
        const int tz = ctz(xmask);
        off += tz;
        xmask >>= tz;
        add_sample(off, 0, 1, -1, &r[-1][t->bx + off]);
        xmask &= ~1;
    }
    if (np < 8 && masks[1] == 1) {
        const int off = t->by & (bs(&r[0][t->bx - 1])[1] - 1);
        add_sample(0, -off, -1, 1, &r[-off][t->bx - 1]);
    } else for (unsigned off = 0, ymask = (uint32_t) masks[1]; np < 8 && ymask;) { // left
        const int tz = ctz(ymask);
        off += tz;
        ymask >>= tz;
        add_sample(0, off, -1, 1, &r[off][t->bx - 1]);
        ymask &= ~1;
    }
    if (np < 8 && masks[1] >> 32) // top/left
        add_sample(0, 0, -1, -1, &r[-1][t->bx - 1]);
    if (np < 8 && masks[0] >> 32) // top/right
        add_sample(bw4, 0, 1, -1, &r[-1][t->bx + bw4]);
    assert(np > 0 && np <= 8);
#undef bs

    // select according to motion vector difference against a threshold
    int mvd[8], ret = 0;
    const int thresh = 4 * iclip(imax(bw4, bh4), 4, 28);
    for (int i = 0; i < np; i++) {
        mvd[i] = abs(pts[i][1][0] - pts[i][0][0] - mv.x) +
                 abs(pts[i][1][1] - pts[i][0][1] - mv.y);
        if (mvd[i] > thresh)
            mvd[i] = -1;
        else
            ret++;
    }
    if (!ret) {
        ret = 1;
    } else for (int i = 0, j = np - 1, k = 0; k < np - ret; k++, i++, j--) {
        while (mvd[i] != -1) i++;
        while (mvd[j] == -1) j--;
        assert(i != j);
        if (i > j) break;
        // replace the discarded samples;
        mvd[i] = mvd[j];
        memcpy(pts[i], pts[j], sizeof(*pts));
    }

    if (!dav1d_find_affine_int(pts, ret, bw4, bh4, mv, wmp, t->bx, t->by) &&
        !dav1d_get_shear_params(wmp))
    {
        wmp->type = DAV1D_WM_TYPE_AFFINE;
    } else
        wmp->type = DAV1D_WM_TYPE_IDENTITY;
}

static inline int findoddzero(const uint8_t *buf, int len) {
    for (int n = 0; n < len; n++)
        if (!buf[n * 2]) return 1;
    return 0;
}

static void read_pal_plane(Dav1dTaskContext *const t, Av1Block *const b,
                           const int pl, const int sz_ctx,
                           const int bx4, const int by4)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    const int pal_sz = b->pal_sz[pl] = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                                           ts->cdf.m.pal_sz[pl][sz_ctx], 6) + 2;
    uint16_t cache[16], used_cache[8];
    int l_cache = pl ? t->pal_sz_uv[1][by4] : t->l.pal_sz[by4];
    int n_cache = 0;
    // don't reuse above palette outside SB64 boundaries
    int a_cache = by4 & 15 ? pl ? t->pal_sz_uv[0][bx4] : t->a->pal_sz[bx4] : 0;
    const uint16_t *l = t->al_pal[1][by4][pl], *a = t->al_pal[0][bx4][pl];

    // fill/sort cache
    while (l_cache && a_cache) {
        if (*l < *a) {
            if (!n_cache || cache[n_cache - 1] != *l)
                cache[n_cache++] = *l;
            l++;
            l_cache--;
        } else {
            if (*a == *l) {
                l++;
                l_cache--;
            }
            if (!n_cache || cache[n_cache - 1] != *a)
                cache[n_cache++] = *a;
            a++;
            a_cache--;
        }
    }
    if (l_cache) {
        do {
            if (!n_cache || cache[n_cache - 1] != *l)
                cache[n_cache++] = *l;
            l++;
        } while (--l_cache > 0);
    } else if (a_cache) {
        do {
            if (!n_cache || cache[n_cache - 1] != *a)
                cache[n_cache++] = *a;
            a++;
        } while (--a_cache > 0);
    }

    // find reused cache entries
    int i = 0;
    for (int n = 0; n < n_cache && i < pal_sz; n++)
        if (dav1d_msac_decode_bool_equi(&ts->msac))
            used_cache[i++] = cache[n];
    const int n_used_cache = i;

    // parse new entries
    uint16_t *const pal = t->frame_thread.pass ?
        f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                            ((t->bx >> 1) + (t->by & 1))][pl] : t->scratch.pal[pl];
    if (i < pal_sz) {
        int prev = pal[i++] = dav1d_msac_decode_bools(&ts->msac, f->cur.p.bpc);

        if (i < pal_sz) {
            int bits = f->cur.p.bpc - 3 + dav1d_msac_decode_bools(&ts->msac, 2);
            const int max = (1 << f->cur.p.bpc) - 1;

            do {
                const int delta = dav1d_msac_decode_bools(&ts->msac, bits);
                prev = pal[i++] = imin(prev + delta + !pl, max);
                if (prev + !pl >= max) {
                    for (; i < pal_sz; i++)
                        pal[i] = max;
                    break;
                }
                bits = imin(bits, 1 + ulog2(max - prev - !pl));
            } while (i < pal_sz);
        }

        // merge cache+new entries
        int n = 0, m = n_used_cache;
        for (i = 0; i < pal_sz; i++) {
            if (n < n_used_cache && (m >= pal_sz || used_cache[n] <= pal[m])) {
                pal[i] = used_cache[n++];
            } else {
                assert(m < pal_sz);
                pal[i] = pal[m++];
            }
        }
    } else {
        memcpy(pal, used_cache, n_used_cache * sizeof(*used_cache));
    }

    if (DEBUG_BLOCK_INFO) {
        printf("Post-pal[pl=%d,sz=%d,cache_size=%d,used_cache=%d]: r=%d, cache=",
               pl, pal_sz, n_cache, n_used_cache, ts->msac.rng);
        for (int n = 0; n < n_cache; n++)
            printf("%c%02x", n ? ' ' : '[', cache[n]);
        printf("%s, pal=", n_cache ? "]" : "[]");
        for (int n = 0; n < pal_sz; n++)
            printf("%c%02x", n ? ' ' : '[', pal[n]);
        printf("]\n");
    }
}

static void read_pal_uv(Dav1dTaskContext *const t, Av1Block *const b,
                        const int sz_ctx, const int bx4, const int by4)
{
    read_pal_plane(t, b, 1, sz_ctx, bx4, by4);

    // V pal coding
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    uint16_t *const pal = t->frame_thread.pass ?
        f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                            ((t->bx >> 1) + (t->by & 1))][2] : t->scratch.pal[2];
    if (dav1d_msac_decode_bool_equi(&ts->msac)) {
        const int bits = f->cur.p.bpc - 4 +
                         dav1d_msac_decode_bools(&ts->msac, 2);
        int prev = pal[0] = dav1d_msac_decode_bools(&ts->msac, f->cur.p.bpc);
        const int max = (1 << f->cur.p.bpc) - 1;
        for (int i = 1; i < b->pal_sz[1]; i++) {
            int delta = dav1d_msac_decode_bools(&ts->msac, bits);
            if (delta && dav1d_msac_decode_bool_equi(&ts->msac)) delta = -delta;
            prev = pal[i] = (prev + delta) & max;
        }
    } else {
        for (int i = 0; i < b->pal_sz[1]; i++)
            pal[i] = dav1d_msac_decode_bools(&ts->msac, f->cur.p.bpc);
    }
    if (DEBUG_BLOCK_INFO) {
        printf("Post-pal[pl=2]: r=%d ", ts->msac.rng);
        for (int n = 0; n < b->pal_sz[1]; n++)
            printf("%c%02x", n ? ' ' : '[', pal[n]);
        printf("]\n");
    }
}

// meant to be SIMD'able, so that theoretical complexity of this function
// times block size goes from w4*h4 to w4+h4-1
// a and b are previous two lines containing (a) top/left entries or (b)
// top/left entries, with a[0] being either the first top or first left entry,
// depending on top_offset being 1 or 0, and b being the first top/left entry
// for whichever has one. left_offset indicates whether the (len-1)th entry
// has a left neighbour.
// output is order[] and ctx for each member of this diagonal.
static void order_palette(const uint8_t *pal_idx, const ptrdiff_t stride,
                          const int i, const int first, const int last,
                          uint8_t (*const order)[8], uint8_t *const ctx)
{
    int have_top = i > first;

    assert(pal_idx);
    pal_idx += first + (i - first) * stride;
    for (int j = first, n = 0; j >= last; have_top = 1, j--, n++, pal_idx += stride - 1) {
        const int have_left = j > 0;

        assert(have_left || have_top);

#define add(v_in) do { \
        const int v = v_in; \
        assert((unsigned)v < 8U); \
        order[n][o_idx++] = v; \
        mask |= 1 << v; \
    } while (0)

        unsigned mask = 0;
        int o_idx = 0;
        if (!have_left) {
            ctx[n] = 0;
            add(pal_idx[-stride]);
        } else if (!have_top) {
            ctx[n] = 0;
            add(pal_idx[-1]);
        } else {
            const int l = pal_idx[-1], t = pal_idx[-stride], tl = pal_idx[-(stride + 1)];
            const int same_t_l = t == l;
            const int same_t_tl = t == tl;
            const int same_l_tl = l == tl;
            const int same_all = same_t_l & same_t_tl & same_l_tl;

            if (same_all) {
                ctx[n] = 4;
                add(t);
            } else if (same_t_l) {
                ctx[n] = 3;
                add(t);
                add(tl);
            } else if (same_t_tl | same_l_tl) {
                ctx[n] = 2;
                add(tl);
                add(same_t_tl ? l : t);
            } else {
                ctx[n] = 1;
                add(imin(t, l));
                add(imax(t, l));
                add(tl);
            }
        }
        for (unsigned m = 1, bit = 0; m < 0x100; m <<= 1, bit++)
            if (!(mask & m))
                order[n][o_idx++] = bit;
        assert(o_idx == 8);
#undef add
    }
}

static void read_pal_indices(Dav1dTaskContext *const t,
                             uint8_t *const pal_idx,
                             const Av1Block *const b, const int pl,
                             const int w4, const int h4,
                             const int bw4, const int bh4)
{
    Dav1dTileState *const ts = t->ts;
    const ptrdiff_t stride = bw4 * 4;
    assert(pal_idx);
    pal_idx[0] = dav1d_msac_decode_uniform(&ts->msac, b->pal_sz[pl]);
    uint16_t (*const color_map_cdf)[8] =
        ts->cdf.m.color_map[pl][b->pal_sz[pl] - 2];
    uint8_t (*const order)[8] = t->scratch.pal_order;
    uint8_t *const ctx = t->scratch.pal_ctx;
    for (int i = 1; i < 4 * (w4 + h4) - 1; i++) {
        // top/left-to-bottom/right diagonals ("wave-front")
        const int first = imin(i, w4 * 4 - 1);
        const int last = imax(0, i - h4 * 4 + 1);
        order_palette(pal_idx, stride, i, first, last, order, ctx);
        for (int j = first, m = 0; j >= last; j--, m++) {
            const int color_idx = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                                      color_map_cdf[ctx[m]], b->pal_sz[pl] - 1);
            pal_idx[(i - j) * stride + j] = order[m][color_idx];
        }
    }
    // fill invisible edges
    if (bw4 > w4)
        for (int y = 0; y < 4 * h4; y++)
            memset(&pal_idx[y * stride + 4 * w4],
                   pal_idx[y * stride + 4 * w4 - 1], 4 * (bw4 - w4));
    if (h4 < bh4) {
        const uint8_t *const src = &pal_idx[stride * (4 * h4 - 1)];
        for (int y = h4 * 4; y < bh4 * 4; y++)
            memcpy(&pal_idx[y * stride], src, bw4 * 4);
    }
}

static void read_vartx_tree(Dav1dTaskContext *const t,
                            Av1Block *const b, const enum BlockSize bs,
                            const int bx4, const int by4)
{
    const Dav1dFrameContext *const f = t->f;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];

    // var-tx tree coding
    uint16_t tx_split[2] = { 0 };
    b->max_ytx = dav1d_max_txfm_size_for_bs[bs][0];
    if (!b->skip && (f->frame_hdr->segmentation.lossless[b->seg_id] ||
                     b->max_ytx == TX_4X4))
    {
        b->max_ytx = b->uvtx = TX_4X4;
        if (f->frame_hdr->txfm_mode == DAV1D_TX_SWITCHABLE) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir tx, off, TX_4X4)
            case_set(bh4, l., 1, by4);
            case_set(bw4, a->, 0, bx4);
#undef set_ctx
        }
    } else if (f->frame_hdr->txfm_mode != DAV1D_TX_SWITCHABLE || b->skip) {
        if (f->frame_hdr->txfm_mode == DAV1D_TX_SWITCHABLE) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir tx, off, mul * b_dim[2 + diridx])
            case_set(bh4, l., 1, by4);
            case_set(bw4, a->, 0, bx4);
#undef set_ctx
        }
        b->uvtx = dav1d_max_txfm_size_for_bs[bs][f->cur.p.layout];
    } else {
        assert(bw4 <= 16 || bh4 <= 16 || b->max_ytx == TX_64X64);
        int y, x, y_off, x_off;
        const TxfmInfo *const ytx = &dav1d_txfm_dimensions[b->max_ytx];
        for (y = 0, y_off = 0; y < bh4; y += ytx->h, y_off++) {
            for (x = 0, x_off = 0; x < bw4; x += ytx->w, x_off++) {
                read_tx_tree(t, b->max_ytx, 0, tx_split, x_off, y_off);
                // contexts are updated inside read_tx_tree()
                t->bx += ytx->w;
            }
            t->bx -= x;
            t->by += ytx->h;
        }
        t->by -= y;
        if (DEBUG_BLOCK_INFO)
            printf("Post-vartxtree[%x/%x]: r=%d\n",
                   tx_split[0], tx_split[1], t->ts->msac.rng);
        b->uvtx = dav1d_max_txfm_size_for_bs[bs][f->cur.p.layout];
    }
    assert(!(tx_split[0] & ~0x33));
    b->tx_split0 = (uint8_t)tx_split[0];
    b->tx_split1 = tx_split[1];
}

static inline unsigned get_prev_frame_segid(const Dav1dFrameContext *const f,
                                            const int by, const int bx,
                                            const int w4, int h4,
                                            const uint8_t *ref_seg_map,
                                            const ptrdiff_t stride)
{
    assert(f->frame_hdr->primary_ref_frame != DAV1D_PRIMARY_REF_NONE);

    unsigned seg_id = 8;
    ref_seg_map += by * stride + bx;
    do {
        for (int x = 0; x < w4; x++)
            seg_id = imin(seg_id, ref_seg_map[x]);
        ref_seg_map += stride;
    } while (--h4 > 0 && seg_id);
    assert(seg_id < 8);

    return seg_id;
}

static inline void splat_oneref_mv(const Dav1dContext *const c,
                                   Dav1dTaskContext *const t,
                                   const enum BlockSize bs,
                                   const Av1Block *const b,
                                   const int bw4, const int bh4)
{
    const enum InterPredMode mode = b->inter_mode;
    const refmvs_block ALIGN(tmpl, 16) = (refmvs_block) {
        .ref.ref = { b->ref[0] + 1, b->interintra_type ? 0 : -1 },
        .mv.mv[0] = b->mv[0],
        .bs = bs,
        .mf = (mode == GLOBALMV && imin(bw4, bh4) >= 2) | ((mode == NEWMV) * 2),
    };
    c->refmvs_dsp.splat_mv(&t->rt.r[(t->by & 31) + 5], &tmpl, t->bx, bw4, bh4);
}

static inline void splat_intrabc_mv(const Dav1dContext *const c,
                                    Dav1dTaskContext *const t,
                                    const enum BlockSize bs,
                                    const Av1Block *const b,
                                    const int bw4, const int bh4)
{
    const refmvs_block ALIGN(tmpl, 16) = (refmvs_block) {
        .ref.ref = { 0, -1 },
        .mv.mv[0] = b->mv[0],
        .bs = bs,
        .mf = 0,
    };
    c->refmvs_dsp.splat_mv(&t->rt.r[(t->by & 31) + 5], &tmpl, t->bx, bw4, bh4);
}

static inline void splat_tworef_mv(const Dav1dContext *const c,
                                   Dav1dTaskContext *const t,
                                   const enum BlockSize bs,
                                   const Av1Block *const b,
                                   const int bw4, const int bh4)
{
    assert(bw4 >= 2 && bh4 >= 2);
    const enum CompInterPredMode mode = b->inter_mode;
    const refmvs_block ALIGN(tmpl, 16) = (refmvs_block) {
        .ref.ref = { b->ref[0] + 1, b->ref[1] + 1 },
        .mv.mv = { b->mv[0], b->mv[1] },
        .bs = bs,
        .mf = (mode == GLOBALMV_GLOBALMV) | !!((1 << mode) & (0xbc)) * 2,
    };
    c->refmvs_dsp.splat_mv(&t->rt.r[(t->by & 31) + 5], &tmpl, t->bx, bw4, bh4);
}

static inline void splat_intraref(const Dav1dContext *const c,
                                  Dav1dTaskContext *const t,
                                  const enum BlockSize bs,
                                  const int bw4, const int bh4)
{
    const refmvs_block ALIGN(tmpl, 16) = (refmvs_block) {
        .ref.ref = { 0, -1 },
        .mv.mv[0].n = INVALID_MV,
        .bs = bs,
        .mf = 0,
    };
    c->refmvs_dsp.splat_mv(&t->rt.r[(t->by & 31) + 5], &tmpl, t->bx, bw4, bh4);
}

static inline void mc_lowest_px(int *const dst, const int by4, const int bh4,
                                const int mvy, const int ss_ver,
                                const struct ScalableMotionParams *const smp)
{
    const int v_mul = 4 >> ss_ver;
    if (!smp->scale) {
        const int my = mvy >> (3 + ss_ver), dy = mvy & (15 >> !ss_ver);
        *dst = imax(*dst, (by4 + bh4) * v_mul + my + 4 * !!dy);
    } else {
        int y = (by4 * v_mul << 4) + mvy * (1 << !ss_ver);
        const int64_t tmp = (int64_t)(y) * smp->scale + (smp->scale - 0x4000) * 8;
        y = apply_sign64((int)((llabs(tmp) + 128) >> 8), tmp) + 32;
        const int bottom = ((y + (bh4 * v_mul - 1) * smp->step) >> 10) + 1 + 4;
        *dst = imax(*dst, bottom);
    }
}

static inline void affine_lowest_px(Dav1dTaskContext *const t,
                                    int *const dst, const int is_chroma,
                                    const uint8_t *const b_dim,
                                    const Dav1dWarpedMotionParams *const wmp)
{
    const Dav1dFrameContext *const f = t->f;
    const int ss_ver = is_chroma && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = is_chroma && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    assert(!((b_dim[0] * h_mul) & 7) && !((b_dim[1] * v_mul) & 7));
    const int32_t *const mat = wmp->matrix;
    const int y = b_dim[1] * v_mul - 8; // lowest y

    const int src_y = t->by * 4 + ((y + 4) << ss_ver);
    const int64_t mat5_y = (int64_t) mat[5] * src_y + mat[1];
    // check left- and right-most blocks
    for (int x = 0; x < b_dim[0] * h_mul; x += imax(8, b_dim[0] * h_mul - 8)) {
        // calculate transformation relative to center of 8x8 block in
        // luma pixel units
        const int src_x = t->bx * 4 + ((x + 4) << ss_hor);
        const int64_t mvy = ((int64_t) mat[4] * src_x + mat5_y) >> ss_ver;
        const int dy = (int) (mvy >> 16) - 4;
        *dst = imax(*dst, dy + 4 + 8);
    }
}

static void obmc_lowest_px(Dav1dTaskContext *const t,
                           int (*const dst)[2], const int is_chroma,
                           const uint8_t *const b_dim,
                           const int bx4, const int by4, const int w4, const int h4)
{
    assert(!(t->bx & 1) && !(t->by & 1));
    const Dav1dFrameContext *const f = t->f;
    /*const*/ refmvs_block **r = &t->rt.r[(t->by & 31) + 5];
    const int ss_ver = is_chroma && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = is_chroma && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;

    if (t->by > t->ts->tiling.row_start &&
        (!is_chroma || b_dim[0] * h_mul + b_dim[1] * v_mul >= 16))
    {
        for (int i = 0, x = 0; x < w4 && i < imin(b_dim[2], 4); ) {
            // only odd blocks are considered for overlap handling, hence +1
            const refmvs_block *const a_r = &r[-1][t->bx + x + 1];
            const uint8_t *const a_b_dim = dav1d_block_dimensions[a_r->bs];

            if (a_r->ref.ref[0] > 0) {
                const int oh4 = imin(b_dim[1], 16) >> 1;
                mc_lowest_px(&dst[a_r->ref.ref[0] - 1][is_chroma], t->by,
                             (oh4 * 3 + 3) >> 2, a_r->mv.mv[0].y, ss_ver,
                             &f->svc[a_r->ref.ref[0] - 1][1]);
                i++;
            }
            x += imax(a_b_dim[0], 2);
        }
    }

    if (t->bx > t->ts->tiling.col_start)
        for (int i = 0, y = 0; y < h4 && i < imin(b_dim[3], 4); ) {
            // only odd blocks are considered for overlap handling, hence +1
            const refmvs_block *const l_r = &r[y + 1][t->bx - 1];
            const uint8_t *const l_b_dim = dav1d_block_dimensions[l_r->bs];

            if (l_r->ref.ref[0] > 0) {
                const int oh4 = iclip(l_b_dim[1], 2, b_dim[1]);
                mc_lowest_px(&dst[l_r->ref.ref[0] - 1][is_chroma],
                             t->by + y, oh4, l_r->mv.mv[0].y, ss_ver,
                             &f->svc[l_r->ref.ref[0] - 1][1]);
                i++;
            }
            y += imax(l_b_dim[1], 2);
        }
}

static int decode_b(Dav1dTaskContext *const t,
                    const enum BlockLevel bl,
                    const enum BlockSize bs,
                    const enum BlockPartition bp,
                    const enum EdgeFlags intra_edge_flags)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    Av1Block b_mem, *const b = t->frame_thread.pass ?
        &f->frame_thread.b[t->by * f->b4_stride + t->bx] : &b_mem;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int cbw4 = (bw4 + ss_hor) >> ss_hor, cbh4 = (bh4 + ss_ver) >> ss_ver;
    const int have_left = t->bx > ts->tiling.col_start;
    const int have_top = t->by > ts->tiling.row_start;
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);

    if (t->frame_thread.pass == 2) {
        if (b->intra) {
            f->bd_fn.recon_b_intra(t, bs, intra_edge_flags, b);

            const enum IntraPredMode y_mode_nofilt =
                b->y_mode == FILTER_PRED ? DC_PRED : b->y_mode;
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir mode, off, mul * y_mode_nofilt); \
            rep_macro(type, t->dir intra, off, mul)
            case_set(bh4, l., 1, by4);
            case_set(bw4, a->, 0, bx4);
#undef set_ctx
            if (IS_INTER_OR_SWITCH(f->frame_hdr)) {
                refmvs_block *const r = &t->rt.r[(t->by & 31) + 5 + bh4 - 1][t->bx];
                for (int x = 0; x < bw4; x++) {
                    r[x].ref.ref[0] = 0;
                    r[x].bs = bs;
                }
                refmvs_block *const *rr = &t->rt.r[(t->by & 31) + 5];
                for (int y = 0; y < bh4 - 1; y++) {
                    rr[y][t->bx + bw4 - 1].ref.ref[0] = 0;
                    rr[y][t->bx + bw4 - 1].bs = bs;
                }
            }

            if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                rep_macro(type, t->dir uvmode, off, mul * b->uv_mode)
                case_set(cbh4, l., 1, cby4);
                case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
            }
        } else {
            if (IS_INTER_OR_SWITCH(f->frame_hdr) /* not intrabc */ &&
                b->comp_type == COMP_INTER_NONE && b->motion_mode == MM_WARP)
            {
                if (b->matrix[0] == SHRT_MIN) {
                    t->warpmv.type = DAV1D_WM_TYPE_IDENTITY;
                } else {
                    t->warpmv.type = DAV1D_WM_TYPE_AFFINE;
                    t->warpmv.matrix[2] = b->matrix[0] + 0x10000;
                    t->warpmv.matrix[3] = b->matrix[1];
                    t->warpmv.matrix[4] = b->matrix[2];
                    t->warpmv.matrix[5] = b->matrix[3] + 0x10000;
                    dav1d_set_affine_mv2d(bw4, bh4, b->mv2d, &t->warpmv,
                                          t->bx, t->by);
                    dav1d_get_shear_params(&t->warpmv);
#define signabs(v) v < 0 ? '-' : ' ', abs(v)
                    if (DEBUG_BLOCK_INFO)
                        printf("[ %c%x %c%x %c%x\n  %c%x %c%x %c%x ]\n"
                               "alpha=%c%x, beta=%c%x, gamma=%c%x, delta=%c%x, mv=y:%d,x:%d\n",
                               signabs(t->warpmv.matrix[0]),
                               signabs(t->warpmv.matrix[1]),
                               signabs(t->warpmv.matrix[2]),
                               signabs(t->warpmv.matrix[3]),
                               signabs(t->warpmv.matrix[4]),
                               signabs(t->warpmv.matrix[5]),
                               signabs(t->warpmv.u.p.alpha),
                               signabs(t->warpmv.u.p.beta),
                               signabs(t->warpmv.u.p.gamma),
                               signabs(t->warpmv.u.p.delta),
                               b->mv2d.y, b->mv2d.x);
#undef signabs
                }
            }
            if (f->bd_fn.recon_b_inter(t, bs, b)) return -1;

            const uint8_t *const filter = dav1d_filter_dir[b->filter2d];
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir filter[0], off, mul * filter[0]); \
            rep_macro(type, t->dir filter[1], off, mul * filter[1]); \
            rep_macro(type, t->dir intra, off, 0)
            case_set(bh4, l., 1, by4);
            case_set(bw4, a->, 0, bx4);
#undef set_ctx

            if (IS_INTER_OR_SWITCH(f->frame_hdr)) {
                refmvs_block *const r = &t->rt.r[(t->by & 31) + 5 + bh4 - 1][t->bx];
                for (int x = 0; x < bw4; x++) {
                    r[x].ref.ref[0] = b->ref[0] + 1;
                    r[x].mv.mv[0] = b->mv[0];
                    r[x].bs = bs;
                }
                refmvs_block *const *rr = &t->rt.r[(t->by & 31) + 5];
                for (int y = 0; y < bh4 - 1; y++) {
                    rr[y][t->bx + bw4 - 1].ref.ref[0] = b->ref[0] + 1;
                    rr[y][t->bx + bw4 - 1].mv.mv[0] = b->mv[0];
                    rr[y][t->bx + bw4 - 1].bs = bs;
                }
            }

            if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                rep_macro(type, t->dir uvmode, off, mul * DC_PRED)
                case_set(cbh4, l., 1, cby4);
                case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
            }
        }
        return 0;
    }

    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;

    b->bl = bl;
    b->bp = bp;
    b->bs = bs;

    const Dav1dSegmentationData *seg = NULL;

    // segment_id (if seg_feature for skip/ref/gmv is enabled)
    int seg_pred = 0;
    if (f->frame_hdr->segmentation.enabled) {
        if (!f->frame_hdr->segmentation.update_map) {
            if (f->prev_segmap) {
                unsigned seg_id = get_prev_frame_segid(f, t->by, t->bx, w4, h4,
                                                       f->prev_segmap,
                                                       f->b4_stride);
                if (seg_id >= 8) return -1;
                b->seg_id = seg_id;
            } else {
                b->seg_id = 0;
            }
            seg = &f->frame_hdr->segmentation.seg_data.d[b->seg_id];
        } else if (f->frame_hdr->segmentation.seg_data.preskip) {
            if (f->frame_hdr->segmentation.temporal &&
                (seg_pred = dav1d_msac_decode_bool_adapt(&ts->msac,
                                ts->cdf.m.seg_pred[t->a->seg_pred[bx4] +
                                t->l.seg_pred[by4]])))
            {
                // temporal predicted seg_id
                if (f->prev_segmap) {
                    unsigned seg_id = get_prev_frame_segid(f, t->by, t->bx,
                                                           w4, h4,
                                                           f->prev_segmap,
                                                           f->b4_stride);
                    if (seg_id >= 8) return -1;
                    b->seg_id = seg_id;
                } else {
                    b->seg_id = 0;
                }
            } else {
                int seg_ctx;
                const unsigned pred_seg_id =
                    get_cur_frame_segid(t->by, t->bx, have_top, have_left,
                                        &seg_ctx, f->cur_segmap, f->b4_stride);
                const unsigned diff = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                                          ts->cdf.m.seg_id[seg_ctx],
                                          DAV1D_MAX_SEGMENTS - 1);
                const unsigned last_active_seg_id =
                    f->frame_hdr->segmentation.seg_data.last_active_segid;
                b->seg_id = neg_deinterleave(diff, pred_seg_id,
                                             last_active_seg_id + 1);
                if (b->seg_id > last_active_seg_id) b->seg_id = 0; // error?
                if (b->seg_id >= DAV1D_MAX_SEGMENTS) b->seg_id = 0; // error?
            }

            if (DEBUG_BLOCK_INFO)
                printf("Post-segid[preskip;%d]: r=%d\n",
                       b->seg_id, ts->msac.rng);

            seg = &f->frame_hdr->segmentation.seg_data.d[b->seg_id];
        }
    } else {
        b->seg_id = 0;
    }

    // skip_mode
    if ((!seg || (!seg->globalmv && seg->ref == -1 && !seg->skip)) &&
        f->frame_hdr->skip_mode_enabled && imin(bw4, bh4) > 1)
    {
        const int smctx = t->a->skip_mode[bx4] + t->l.skip_mode[by4];
        b->skip_mode = dav1d_msac_decode_bool_adapt(&ts->msac,
                           ts->cdf.m.skip_mode[smctx]);
        if (DEBUG_BLOCK_INFO)
            printf("Post-skipmode[%d]: r=%d\n", b->skip_mode, ts->msac.rng);
    } else {
        b->skip_mode = 0;
    }

    // skip
    if (b->skip_mode || (seg && seg->skip)) {
        b->skip = 1;
    } else {
        const int sctx = t->a->skip[bx4] + t->l.skip[by4];
        b->skip = dav1d_msac_decode_bool_adapt(&ts->msac, ts->cdf.m.skip[sctx]);
        if (DEBUG_BLOCK_INFO)
            printf("Post-skip[%d]: r=%d\n", b->skip, ts->msac.rng);
    }

    // segment_id
    if (f->frame_hdr->segmentation.enabled &&
        f->frame_hdr->segmentation.update_map &&
        !f->frame_hdr->segmentation.seg_data.preskip)
    {
        if (!b->skip && f->frame_hdr->segmentation.temporal &&
            (seg_pred = dav1d_msac_decode_bool_adapt(&ts->msac,
                            ts->cdf.m.seg_pred[t->a->seg_pred[bx4] +
                            t->l.seg_pred[by4]])))
        {
            // temporal predicted seg_id
            if (f->prev_segmap) {
                unsigned seg_id = get_prev_frame_segid(f, t->by, t->bx, w4, h4,
                                                       f->prev_segmap,
                                                       f->b4_stride);
                if (seg_id >= 8) return -1;
                b->seg_id = seg_id;
            } else {
                b->seg_id = 0;
            }
        } else {
            int seg_ctx;
            const unsigned pred_seg_id =
                get_cur_frame_segid(t->by, t->bx, have_top, have_left,
                                    &seg_ctx, f->cur_segmap, f->b4_stride);
            if (b->skip) {
                b->seg_id = pred_seg_id;
            } else {
                const unsigned diff = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                                          ts->cdf.m.seg_id[seg_ctx],
                                          DAV1D_MAX_SEGMENTS - 1);
                const unsigned last_active_seg_id =
                    f->frame_hdr->segmentation.seg_data.last_active_segid;
                b->seg_id = neg_deinterleave(diff, pred_seg_id,
                                             last_active_seg_id + 1);
                if (b->seg_id > last_active_seg_id) b->seg_id = 0; // error?
            }
            if (b->seg_id >= DAV1D_MAX_SEGMENTS) b->seg_id = 0; // error?
        }

        seg = &f->frame_hdr->segmentation.seg_data.d[b->seg_id];

        if (DEBUG_BLOCK_INFO)
            printf("Post-segid[postskip;%d]: r=%d\n",
                   b->seg_id, ts->msac.rng);
    }

    // cdef index
    if (!b->skip) {
        const int idx = f->seq_hdr->sb128 ? ((t->bx & 16) >> 4) +
                                           ((t->by & 16) >> 3) : 0;
        if (t->cur_sb_cdef_idx_ptr[idx] == -1) {
            const int v = dav1d_msac_decode_bools(&ts->msac,
                              f->frame_hdr->cdef.n_bits);
            t->cur_sb_cdef_idx_ptr[idx] = v;
            if (bw4 > 16) t->cur_sb_cdef_idx_ptr[idx + 1] = v;
            if (bh4 > 16) t->cur_sb_cdef_idx_ptr[idx + 2] = v;
            if (bw4 == 32 && bh4 == 32) t->cur_sb_cdef_idx_ptr[idx + 3] = v;

            if (DEBUG_BLOCK_INFO)
                printf("Post-cdef_idx[%d]: r=%d\n",
                        *t->cur_sb_cdef_idx_ptr, ts->msac.rng);
        }
    }

    // delta-q/lf
    if (!(t->bx & (31 >> !f->seq_hdr->sb128)) &&
        !(t->by & (31 >> !f->seq_hdr->sb128)))
    {
        const int prev_qidx = ts->last_qidx;
        const int have_delta_q = f->frame_hdr->delta.q.present &&
            (bs != (f->seq_hdr->sb128 ? BS_128x128 : BS_64x64) || !b->skip);

        int8_t prev_delta_lf[4];
        memcpy(prev_delta_lf, ts->last_delta_lf, 4);

        if (have_delta_q) {
            int delta_q = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                                          ts->cdf.m.delta_q, 3);
            if (delta_q == 3) {
                const int n_bits = 1 + dav1d_msac_decode_bools(&ts->msac, 3);
                delta_q = dav1d_msac_decode_bools(&ts->msac, n_bits) +
                          1 + (1 << n_bits);
            }
            if (delta_q) {
                if (dav1d_msac_decode_bool_equi(&ts->msac)) delta_q = -delta_q;
                delta_q *= 1 << f->frame_hdr->delta.q.res_log2;
            }
            ts->last_qidx = iclip(ts->last_qidx + delta_q, 1, 255);
            if (have_delta_q && DEBUG_BLOCK_INFO)
                printf("Post-delta_q[%d->%d]: r=%d\n",
                       delta_q, ts->last_qidx, ts->msac.rng);

            if (f->frame_hdr->delta.lf.present) {
                const int n_lfs = f->frame_hdr->delta.lf.multi ?
                    f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 ? 4 : 2 : 1;

                for (int i = 0; i < n_lfs; i++) {
                    int delta_lf = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                        ts->cdf.m.delta_lf[i + f->frame_hdr->delta.lf.multi], 3);
                    if (delta_lf == 3) {
                        const int n_bits = 1 + dav1d_msac_decode_bools(&ts->msac, 3);
                        delta_lf = dav1d_msac_decode_bools(&ts->msac, n_bits) +
                                   1 + (1 << n_bits);
                    }
                    if (delta_lf) {
                        if (dav1d_msac_decode_bool_equi(&ts->msac))
                            delta_lf = -delta_lf;
                        delta_lf *= 1 << f->frame_hdr->delta.lf.res_log2;
                    }
                    ts->last_delta_lf[i] =
                        iclip(ts->last_delta_lf[i] + delta_lf, -63, 63);
                    if (have_delta_q && DEBUG_BLOCK_INFO)
                        printf("Post-delta_lf[%d:%d]: r=%d\n", i, delta_lf,
                               ts->msac.rng);
                }
            }
        }
        if (ts->last_qidx == f->frame_hdr->quant.yac) {
            // assign frame-wide q values to this sb
            ts->dq = f->dq;
        } else if (ts->last_qidx != prev_qidx) {
            // find sb-specific quant parameters
            init_quant_tables(f->seq_hdr, f->frame_hdr, ts->last_qidx, ts->dqmem);
            ts->dq = ts->dqmem;
        }
        if (!memcmp(ts->last_delta_lf, (int8_t[4]) { 0, 0, 0, 0 }, 4)) {
            // assign frame-wide lf values to this sb
            ts->lflvl = f->lf.lvl;
        } else if (memcmp(ts->last_delta_lf, prev_delta_lf, 4)) {
            // find sb-specific lf lvl parameters
            dav1d_calc_lf_values(ts->lflvlmem, f->frame_hdr, ts->last_delta_lf);
            ts->lflvl = ts->lflvlmem;
        }
    }

    if (b->skip_mode) {
        b->intra = 0;
    } else if (IS_INTER_OR_SWITCH(f->frame_hdr)) {
        if (seg && (seg->ref >= 0 || seg->globalmv)) {
            b->intra = !seg->ref;
        } else {
            const int ictx = get_intra_ctx(t->a, &t->l, by4, bx4,
                                           have_top, have_left);
            b->intra = !dav1d_msac_decode_bool_adapt(&ts->msac,
                            ts->cdf.m.intra[ictx]);
            if (DEBUG_BLOCK_INFO)
                printf("Post-intra[%d]: r=%d\n", b->intra, ts->msac.rng);
        }
    } else if (f->frame_hdr->allow_intrabc) {
        b->intra = !dav1d_msac_decode_bool_adapt(&ts->msac, ts->cdf.m.intrabc);
        if (DEBUG_BLOCK_INFO)
            printf("Post-intrabcflag[%d]: r=%d\n", b->intra, ts->msac.rng);
    } else {
        b->intra = 1;
    }

    // intra/inter-specific stuff
    if (b->intra) {
        uint16_t *const ymode_cdf = IS_INTER_OR_SWITCH(f->frame_hdr) ?
            ts->cdf.m.y_mode[dav1d_ymode_size_context[bs]] :
            ts->cdf.kfym[dav1d_intra_mode_context[t->a->mode[bx4]]]
                        [dav1d_intra_mode_context[t->l.mode[by4]]];
        b->y_mode = dav1d_msac_decode_symbol_adapt16(&ts->msac, ymode_cdf,
                                                     N_INTRA_PRED_MODES - 1);
        if (DEBUG_BLOCK_INFO)
            printf("Post-ymode[%d]: r=%d\n", b->y_mode, ts->msac.rng);

        // angle delta
        if (b_dim[2] + b_dim[3] >= 2 && b->y_mode >= VERT_PRED &&
            b->y_mode <= VERT_LEFT_PRED)
        {
            uint16_t *const acdf = ts->cdf.m.angle_delta[b->y_mode - VERT_PRED];
            const int angle = dav1d_msac_decode_symbol_adapt8(&ts->msac, acdf, 6);
            b->y_angle = angle - 3;
        } else {
            b->y_angle = 0;
        }

        if (has_chroma) {
            const int cfl_allowed = f->frame_hdr->segmentation.lossless[b->seg_id] ?
                cbw4 == 1 && cbh4 == 1 : !!(cfl_allowed_mask & (1 << bs));
            uint16_t *const uvmode_cdf = ts->cdf.m.uv_mode[cfl_allowed][b->y_mode];
            b->uv_mode = dav1d_msac_decode_symbol_adapt16(&ts->msac, uvmode_cdf,
                             N_UV_INTRA_PRED_MODES - 1 - !cfl_allowed);
            if (DEBUG_BLOCK_INFO)
                printf("Post-uvmode[%d]: r=%d\n", b->uv_mode, ts->msac.rng);

            b->uv_angle = 0;
            if (b->uv_mode == CFL_PRED) {
#define SIGN(a) (!!(a) + ((a) > 0))
                const int sign = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                                     ts->cdf.m.cfl_sign, 7) + 1;
                const int sign_u = sign * 0x56 >> 8, sign_v = sign - sign_u * 3;
                assert(sign_u == sign / 3);
                if (sign_u) {
                    const int ctx = (sign_u == 2) * 3 + sign_v;
                    b->cfl_alpha[0] = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                                          ts->cdf.m.cfl_alpha[ctx], 15) + 1;
                    if (sign_u == 1) b->cfl_alpha[0] = -b->cfl_alpha[0];
                } else {
                    b->cfl_alpha[0] = 0;
                }
                if (sign_v) {
                    const int ctx = (sign_v == 2) * 3 + sign_u;
                    b->cfl_alpha[1] = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                                          ts->cdf.m.cfl_alpha[ctx], 15) + 1;
                    if (sign_v == 1) b->cfl_alpha[1] = -b->cfl_alpha[1];
                } else {
                    b->cfl_alpha[1] = 0;
                }
#undef SIGN
                if (DEBUG_BLOCK_INFO)
                    printf("Post-uvalphas[%d/%d]: r=%d\n",
                           b->cfl_alpha[0], b->cfl_alpha[1], ts->msac.rng);
            } else if (b_dim[2] + b_dim[3] >= 2 && b->uv_mode >= VERT_PRED &&
                       b->uv_mode <= VERT_LEFT_PRED)
            {
                uint16_t *const acdf = ts->cdf.m.angle_delta[b->uv_mode - VERT_PRED];
                const int angle = dav1d_msac_decode_symbol_adapt8(&ts->msac, acdf, 6);
                b->uv_angle = angle - 3;
            }
        }

        b->pal_sz[0] = b->pal_sz[1] = 0;
        if (f->frame_hdr->allow_screen_content_tools &&
            imax(bw4, bh4) <= 16 && bw4 + bh4 >= 4)
        {
            const int sz_ctx = b_dim[2] + b_dim[3] - 2;
            if (b->y_mode == DC_PRED) {
                const int pal_ctx = (t->a->pal_sz[bx4] > 0) + (t->l.pal_sz[by4] > 0);
                const int use_y_pal = dav1d_msac_decode_bool_adapt(&ts->msac,
                                          ts->cdf.m.pal_y[sz_ctx][pal_ctx]);
                if (DEBUG_BLOCK_INFO)
                    printf("Post-y_pal[%d]: r=%d\n", use_y_pal, ts->msac.rng);
                if (use_y_pal)
                    read_pal_plane(t, b, 0, sz_ctx, bx4, by4);
            }

            if (has_chroma && b->uv_mode == DC_PRED) {
                const int pal_ctx = b->pal_sz[0] > 0;
                const int use_uv_pal = dav1d_msac_decode_bool_adapt(&ts->msac,
                                           ts->cdf.m.pal_uv[pal_ctx]);
                if (DEBUG_BLOCK_INFO)
                    printf("Post-uv_pal[%d]: r=%d\n", use_uv_pal, ts->msac.rng);
                if (use_uv_pal) // see aomedia bug 2183 for why we use luma coordinates
                    read_pal_uv(t, b, sz_ctx, bx4, by4);
            }
        }

        if (b->y_mode == DC_PRED && !b->pal_sz[0] &&
            imax(b_dim[2], b_dim[3]) <= 3 && f->seq_hdr->filter_intra)
        {
            const int is_filter = dav1d_msac_decode_bool_adapt(&ts->msac,
                                      ts->cdf.m.use_filter_intra[bs]);
            if (is_filter) {
                b->y_mode = FILTER_PRED;
                b->y_angle = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                 ts->cdf.m.filter_intra, 4);
            }
            if (DEBUG_BLOCK_INFO)
                printf("Post-filterintramode[%d/%d]: r=%d\n",
                       b->y_mode, b->y_angle, ts->msac.rng);
        }

        if (b->pal_sz[0]) {
            uint8_t *pal_idx;
            if (t->frame_thread.pass) {
                const int p = t->frame_thread.pass & 1;
                assert(ts->frame_thread[p].pal_idx);
                pal_idx = ts->frame_thread[p].pal_idx;
                ts->frame_thread[p].pal_idx += bw4 * bh4 * 16;
            } else
                pal_idx = t->scratch.pal_idx;
            read_pal_indices(t, pal_idx, b, 0, w4, h4, bw4, bh4);
            if (DEBUG_BLOCK_INFO)
                printf("Post-y-pal-indices: r=%d\n", ts->msac.rng);
        }

        if (has_chroma && b->pal_sz[1]) {
            uint8_t *pal_idx;
            if (t->frame_thread.pass) {
                const int p = t->frame_thread.pass & 1;
                assert(ts->frame_thread[p].pal_idx);
                pal_idx = ts->frame_thread[p].pal_idx;
                ts->frame_thread[p].pal_idx += cbw4 * cbh4 * 16;
            } else
                pal_idx = &t->scratch.pal_idx[bw4 * bh4 * 16];
            read_pal_indices(t, pal_idx, b, 1, cw4, ch4, cbw4, cbh4);
            if (DEBUG_BLOCK_INFO)
                printf("Post-uv-pal-indices: r=%d\n", ts->msac.rng);
        }

        const TxfmInfo *t_dim;
        if (f->frame_hdr->segmentation.lossless[b->seg_id]) {
            b->tx = b->uvtx = (int) TX_4X4;
            t_dim = &dav1d_txfm_dimensions[TX_4X4];
        } else {
            b->tx = dav1d_max_txfm_size_for_bs[bs][0];
            b->uvtx = dav1d_max_txfm_size_for_bs[bs][f->cur.p.layout];
            t_dim = &dav1d_txfm_dimensions[b->tx];
            if (f->frame_hdr->txfm_mode == DAV1D_TX_SWITCHABLE && t_dim->max > TX_4X4) {
                const int tctx = get_tx_ctx(t->a, &t->l, t_dim, by4, bx4);
                uint16_t *const tx_cdf = ts->cdf.m.txsz[t_dim->max - 1][tctx];
                int depth = dav1d_msac_decode_symbol_adapt4(&ts->msac, tx_cdf,
                                imin(t_dim->max, 2));

                while (depth--) {
                    b->tx = t_dim->sub;
                    t_dim = &dav1d_txfm_dimensions[b->tx];
                }
            }
            if (DEBUG_BLOCK_INFO)
                printf("Post-tx[%d]: r=%d\n", b->tx, ts->msac.rng);
        }

        // reconstruction
        if (t->frame_thread.pass == 1) {
            f->bd_fn.read_coef_blocks(t, bs, b);
        } else {
            f->bd_fn.recon_b_intra(t, bs, intra_edge_flags, b);
        }

        if (f->frame_hdr->loopfilter.level_y[0] ||
            f->frame_hdr->loopfilter.level_y[1])
        {
            dav1d_create_lf_mask_intra(t->lf_mask, f->lf.level, f->b4_stride,
                                       (const uint8_t (*)[8][2])
                                       &ts->lflvl[b->seg_id][0][0][0],
                                       t->bx, t->by, f->w4, f->h4, bs,
                                       b->tx, b->uvtx, f->cur.p.layout,
                                       &t->a->tx_lpf_y[bx4], &t->l.tx_lpf_y[by4],
                                       has_chroma ? &t->a->tx_lpf_uv[cbx4] : NULL,
                                       has_chroma ? &t->l.tx_lpf_uv[cby4] : NULL);
        }

        // update contexts
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir tx_intra, off, mul * (((uint8_t *) &t_dim->lw)[diridx])); \
        rep_macro(type, t->dir tx, off, mul * (((uint8_t *) &t_dim->lw)[diridx])); \
        rep_macro(type, t->dir mode, off, mul * y_mode_nofilt); \
        rep_macro(type, t->dir pal_sz, off, mul * b->pal_sz[0]); \
        rep_macro(type, t->dir seg_pred, off, mul * seg_pred); \
        rep_macro(type, t->dir skip_mode, off, 0); \
        rep_macro(type, t->dir intra, off, mul); \
        rep_macro(type, t->dir skip, off, mul * b->skip); \
        /* see aomedia bug 2183 for why we use luma coordinates here */ \
        rep_macro(type, t->pal_sz_uv[diridx], off, mul * (has_chroma ? b->pal_sz[1] : 0)); \
        if (IS_INTER_OR_SWITCH(f->frame_hdr)) { \
            rep_macro(type, t->dir comp_type, off, mul * COMP_INTER_NONE); \
            rep_macro(type, t->dir ref[0], off, mul * ((uint8_t) -1)); \
            rep_macro(type, t->dir ref[1], off, mul * ((uint8_t) -1)); \
            rep_macro(type, t->dir filter[0], off, mul * DAV1D_N_SWITCHABLE_FILTERS); \
            rep_macro(type, t->dir filter[1], off, mul * DAV1D_N_SWITCHABLE_FILTERS); \
        }
        const enum IntraPredMode y_mode_nofilt =
            b->y_mode == FILTER_PRED ? DC_PRED : b->y_mode;
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx
        if (b->pal_sz[0]) {
            uint16_t *const pal = t->frame_thread.pass ?
                f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                                    ((t->bx >> 1) + (t->by & 1))][0] : t->scratch.pal[0];
            for (int x = 0; x < bw4; x++)
                memcpy(t->al_pal[0][bx4 + x][0], pal, 16);
            for (int y = 0; y < bh4; y++)
                memcpy(t->al_pal[1][by4 + y][0], pal, 16);
        }
        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                rep_macro(type, t->dir uvmode, off, mul * b->uv_mode)
                case_set(cbh4, l., 1, cby4);
                case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
            if (b->pal_sz[1]) {
                const uint16_t (*const pal)[8] = t->frame_thread.pass ?
                    f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) *
                    (f->b4_stride >> 1) + ((t->bx >> 1) + (t->by & 1))] :
                    t->scratch.pal;
                // see aomedia bug 2183 for why we use luma coordinates here
                for (int pl = 1; pl <= 2; pl++) {
                    for (int x = 0; x < bw4; x++)
                        memcpy(t->al_pal[0][bx4 + x][pl], pal[pl], 16);
                    for (int y = 0; y < bh4; y++)
                        memcpy(t->al_pal[1][by4 + y][pl], pal[pl], 16);
                }
            }
        }
        if (IS_INTER_OR_SWITCH(f->frame_hdr) || f->frame_hdr->allow_intrabc)
            splat_intraref(f->c, t, bs, bw4, bh4);
    } else if (IS_KEY_OR_INTRA(f->frame_hdr)) {
        // intra block copy
        refmvs_candidate mvstack[8];
        int n_mvs, ctx;
        dav1d_refmvs_find(&t->rt, mvstack, &n_mvs, &ctx,
                          (union refmvs_refpair) { .ref = { 0, -1 }},
                          bs, intra_edge_flags, t->by, t->bx);

        if (mvstack[0].mv.mv[0].n)
            b->mv[0] = mvstack[0].mv.mv[0];
        else if (mvstack[1].mv.mv[0].n)
            b->mv[0] = mvstack[1].mv.mv[0];
        else {
            if (t->by - (16 << f->seq_hdr->sb128) < ts->tiling.row_start) {
                b->mv[0].y = 0;
                b->mv[0].x = -(512 << f->seq_hdr->sb128) - 2048;
            } else {
                b->mv[0].y = -(512 << f->seq_hdr->sb128);
                b->mv[0].x = 0;
            }
        }

        const union mv ref = b->mv[0];
        read_mv_residual(t, &b->mv[0], &ts->cdf.dmv, 0);

        // clip intrabc motion vector to decoded parts of current tile
        int border_left = ts->tiling.col_start * 4;
        int border_top  = ts->tiling.row_start * 4;
        if (has_chroma) {
            if (bw4 < 2 &&  ss_hor)
                border_left += 4;
            if (bh4 < 2 &&  ss_ver)
                border_top  += 4;
        }
        int src_left   = t->bx * 4 + (b->mv[0].x >> 3);
        int src_top    = t->by * 4 + (b->mv[0].y >> 3);
        int src_right  = src_left + bw4 * 4;
        int src_bottom = src_top  + bh4 * 4;
        const int border_right = ((ts->tiling.col_end + (bw4 - 1)) & ~(bw4 - 1)) * 4;

        // check against left or right tile boundary and adjust if necessary
        if (src_left < border_left) {
            src_right += border_left - src_left;
            src_left  += border_left - src_left;
        } else if (src_right > border_right) {
            src_left  -= src_right - border_right;
            src_right -= src_right - border_right;
        }
        // check against top tile boundary and adjust if necessary
        if (src_top < border_top) {
            src_bottom += border_top - src_top;
            src_top    += border_top - src_top;
        }

        const int sbx = (t->bx >> (4 + f->seq_hdr->sb128)) << (6 + f->seq_hdr->sb128);
        const int sby = (t->by >> (4 + f->seq_hdr->sb128)) << (6 + f->seq_hdr->sb128);
        const int sb_size = 1 << (6 + f->seq_hdr->sb128);
        // check for overlap with current superblock
        if (src_bottom > sby && src_right > sbx) {
            if (src_top - border_top >= src_bottom - sby) {
                // if possible move src up into the previous suberblock row
                src_top    -= src_bottom - sby;
                src_bottom -= src_bottom - sby;
            } else if (src_left - border_left >= src_right - sbx) {
                // if possible move src left into the previous suberblock
                src_left  -= src_right - sbx;
                src_right -= src_right - sbx;
            }
        }
        // move src up if it is below current superblock row
        if (src_bottom > sby + sb_size) {
            src_top    -= src_bottom - (sby + sb_size);
            src_bottom -= src_bottom - (sby + sb_size);
        }
        // error out if mv still overlaps with the current superblock
        if (src_bottom > sby && src_right > sbx)
            return -1;

        b->mv[0].x = (src_left - t->bx * 4) * 8;
        b->mv[0].y = (src_top  - t->by * 4) * 8;

        if (DEBUG_BLOCK_INFO)
            printf("Post-dmv[%d/%d,ref=%d/%d|%d/%d]: r=%d\n",
                   b->mv[0].y, b->mv[0].x, ref.y, ref.x,
                   mvstack[0].mv.mv[0].y, mvstack[0].mv.mv[0].x, ts->msac.rng);
        read_vartx_tree(t, b, bs, bx4, by4);

        // reconstruction
        if (t->frame_thread.pass == 1) {
            f->bd_fn.read_coef_blocks(t, bs, b);
            b->filter2d = FILTER_2D_BILINEAR;
        } else {
            if (f->bd_fn.recon_b_inter(t, bs, b)) return -1;
        }

        splat_intrabc_mv(f->c, t, bs, b, bw4, bh4);

#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir tx_intra, off, mul * b_dim[2 + diridx]); \
        rep_macro(type, t->dir mode, off, mul * DC_PRED); \
        rep_macro(type, t->dir pal_sz, off, 0); \
        /* see aomedia bug 2183 for why this is outside if (has_chroma) */ \
        rep_macro(type, t->pal_sz_uv[diridx], off, 0); \
        rep_macro(type, t->dir seg_pred, off, mul * seg_pred); \
        rep_macro(type, t->dir skip_mode, off, 0); \
        rep_macro(type, t->dir intra, off, 0); \
        rep_macro(type, t->dir skip, off, mul * b->skip)
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx
        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir uvmode, off, mul * DC_PRED)
            case_set(cbh4, l., 1, cby4);
            case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
        }
    } else {
        // inter-specific mode/mv coding
        int is_comp, has_subpel_filter;

        if (b->skip_mode) {
            is_comp = 1;
        } else if ((!seg || (seg->ref == -1 && !seg->globalmv && !seg->skip)) &&
                   f->frame_hdr->switchable_comp_refs && imin(bw4, bh4) > 1)
        {
            const int ctx = get_comp_ctx(t->a, &t->l, by4, bx4,
                                         have_top, have_left);
            is_comp = dav1d_msac_decode_bool_adapt(&ts->msac,
                          ts->cdf.m.comp[ctx]);
            if (DEBUG_BLOCK_INFO)
                printf("Post-compflag[%d]: r=%d\n", is_comp, ts->msac.rng);
        } else {
            is_comp = 0;
        }

        if (b->skip_mode) {
            b->ref[0] = f->frame_hdr->skip_mode_refs[0];
            b->ref[1] = f->frame_hdr->skip_mode_refs[1];
            b->comp_type = COMP_INTER_AVG;
            b->inter_mode = NEARESTMV_NEARESTMV;
            b->drl_idx = NEAREST_DRL;
            has_subpel_filter = 0;

            refmvs_candidate mvstack[8];
            int n_mvs, ctx;
            dav1d_refmvs_find(&t->rt, mvstack, &n_mvs, &ctx,
                              (union refmvs_refpair) { .ref = {
                                    b->ref[0] + 1, b->ref[1] + 1 }},
                              bs, intra_edge_flags, t->by, t->bx);

            b->mv[0] = mvstack[0].mv.mv[0];
            b->mv[1] = mvstack[0].mv.mv[1];
            fix_mv_precision(f->frame_hdr, &b->mv[0]);
            fix_mv_precision(f->frame_hdr, &b->mv[1]);
            if (DEBUG_BLOCK_INFO)
                printf("Post-skipmodeblock[mv=1:y=%d,x=%d,2:y=%d,x=%d,refs=%d+%d\n",
                       b->mv[0].y, b->mv[0].x, b->mv[1].y, b->mv[1].x,
                       b->ref[0], b->ref[1]);
        } else if (is_comp) {
            const int dir_ctx = get_comp_dir_ctx(t->a, &t->l, by4, bx4,
                                                 have_top, have_left);
            if (dav1d_msac_decode_bool_adapt(&ts->msac,
                    ts->cdf.m.comp_dir[dir_ctx]))
            {
                // bidir - first reference (fw)
                const int ctx1 = av1_get_fwd_ref_ctx(t->a, &t->l, by4, bx4,
                                                     have_top, have_left);
                if (dav1d_msac_decode_bool_adapt(&ts->msac,
                        ts->cdf.m.comp_fwd_ref[0][ctx1]))
                {
                    const int ctx2 = av1_get_fwd_ref_2_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                    b->ref[0] = 2 + dav1d_msac_decode_bool_adapt(&ts->msac,
                                        ts->cdf.m.comp_fwd_ref[2][ctx2]);
                } else {
                    const int ctx2 = av1_get_fwd_ref_1_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                    b->ref[0] = dav1d_msac_decode_bool_adapt(&ts->msac,
                                    ts->cdf.m.comp_fwd_ref[1][ctx2]);
                }

                // second reference (bw)
                const int ctx3 = av1_get_bwd_ref_ctx(t->a, &t->l, by4, bx4,
                                                     have_top, have_left);
                if (dav1d_msac_decode_bool_adapt(&ts->msac,
                        ts->cdf.m.comp_bwd_ref[0][ctx3]))
                {
                    b->ref[1] = 6;
                } else {
                    const int ctx4 = av1_get_bwd_ref_1_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                    b->ref[1] = 4 + dav1d_msac_decode_bool_adapt(&ts->msac,
                                        ts->cdf.m.comp_bwd_ref[1][ctx4]);
                }
            } else {
                // unidir
                const int uctx_p = av1_get_uni_p_ctx(t->a, &t->l, by4, bx4,
                                                     have_top, have_left);
                if (dav1d_msac_decode_bool_adapt(&ts->msac,
                        ts->cdf.m.comp_uni_ref[0][uctx_p]))
                {
                    b->ref[0] = 4;
                    b->ref[1] = 6;
                } else {
                    const int uctx_p1 = av1_get_uni_p1_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                    b->ref[0] = 0;
                    b->ref[1] = 1 + dav1d_msac_decode_bool_adapt(&ts->msac,
                                        ts->cdf.m.comp_uni_ref[1][uctx_p1]);
                    if (b->ref[1] == 2) {
                        const int uctx_p2 = av1_get_uni_p2_ctx(t->a, &t->l, by4, bx4,
                                                               have_top, have_left);
                        b->ref[1] += dav1d_msac_decode_bool_adapt(&ts->msac,
                                         ts->cdf.m.comp_uni_ref[2][uctx_p2]);
                    }
                }
            }
            if (DEBUG_BLOCK_INFO)
                printf("Post-refs[%d/%d]: r=%d\n",
                       b->ref[0], b->ref[1], ts->msac.rng);

            refmvs_candidate mvstack[8];
            int n_mvs, ctx;
            dav1d_refmvs_find(&t->rt, mvstack, &n_mvs, &ctx,
                              (union refmvs_refpair) { .ref = {
                                    b->ref[0] + 1, b->ref[1] + 1 }},
                              bs, intra_edge_flags, t->by, t->bx);

            b->inter_mode = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                                ts->cdf.m.comp_inter_mode[ctx],
                                N_COMP_INTER_PRED_MODES - 1);
            if (DEBUG_BLOCK_INFO)
                printf("Post-compintermode[%d,ctx=%d,n_mvs=%d]: r=%d\n",
                       b->inter_mode, ctx, n_mvs, ts->msac.rng);

            const uint8_t *const im = dav1d_comp_inter_pred_modes[b->inter_mode];
            b->drl_idx = NEAREST_DRL;
            if (b->inter_mode == NEWMV_NEWMV) {
                if (n_mvs > 1) { // NEARER, NEAR or NEARISH
                    const int drl_ctx_v1 = get_drl_context(mvstack, 0);
                    b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                      ts->cdf.m.drl_bit[drl_ctx_v1]);
                    if (b->drl_idx == NEARER_DRL && n_mvs > 2) {
                        const int drl_ctx_v2 = get_drl_context(mvstack, 1);
                        b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                          ts->cdf.m.drl_bit[drl_ctx_v2]);
                    }
                    if (DEBUG_BLOCK_INFO)
                        printf("Post-drlidx[%d,n_mvs=%d]: r=%d\n",
                               b->drl_idx, n_mvs, ts->msac.rng);
                }
            } else if (im[0] == NEARMV || im[1] == NEARMV) {
                b->drl_idx = NEARER_DRL;
                if (n_mvs > 2) { // NEAR or NEARISH
                    const int drl_ctx_v2 = get_drl_context(mvstack, 1);
                    b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                      ts->cdf.m.drl_bit[drl_ctx_v2]);
                    if (b->drl_idx == NEAR_DRL && n_mvs > 3) {
                        const int drl_ctx_v3 = get_drl_context(mvstack, 2);
                        b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                          ts->cdf.m.drl_bit[drl_ctx_v3]);
                    }
                    if (DEBUG_BLOCK_INFO)
                        printf("Post-drlidx[%d,n_mvs=%d]: r=%d\n",
                               b->drl_idx, n_mvs, ts->msac.rng);
                }
            }
            assert(b->drl_idx >= NEAREST_DRL && b->drl_idx <= NEARISH_DRL);

#define assign_comp_mv(idx) \
            switch (im[idx]) { \
            case NEARMV: \
            case NEARESTMV: \
                b->mv[idx] = mvstack[b->drl_idx].mv.mv[idx]; \
                fix_mv_precision(f->frame_hdr, &b->mv[idx]); \
                break; \
            case GLOBALMV: \
                has_subpel_filter |= \
                    f->frame_hdr->gmv[b->ref[idx]].type == DAV1D_WM_TYPE_TRANSLATION; \
                b->mv[idx] = get_gmv_2d(&f->frame_hdr->gmv[b->ref[idx]], \
                                        t->bx, t->by, bw4, bh4, f->frame_hdr); \
                break; \
            case NEWMV: \
                b->mv[idx] = mvstack[b->drl_idx].mv.mv[idx]; \
                read_mv_residual(t, &b->mv[idx], &ts->cdf.mv, \
                                 !f->frame_hdr->force_integer_mv); \
                break; \
            }
            has_subpel_filter = imin(bw4, bh4) == 1 ||
                                b->inter_mode != GLOBALMV_GLOBALMV;
            assign_comp_mv(0);
            assign_comp_mv(1);
#undef assign_comp_mv
            if (DEBUG_BLOCK_INFO)
                printf("Post-residual_mv[1:y=%d,x=%d,2:y=%d,x=%d]: r=%d\n",
                       b->mv[0].y, b->mv[0].x, b->mv[1].y, b->mv[1].x,
                       ts->msac.rng);

            // jnt_comp vs. seg vs. wedge
            int is_segwedge = 0;
            if (f->seq_hdr->masked_compound) {
                const int mask_ctx = get_mask_comp_ctx(t->a, &t->l, by4, bx4);

                is_segwedge = dav1d_msac_decode_bool_adapt(&ts->msac,
                                  ts->cdf.m.mask_comp[mask_ctx]);
                if (DEBUG_BLOCK_INFO)
                    printf("Post-segwedge_vs_jntavg[%d,ctx=%d]: r=%d\n",
                           is_segwedge, mask_ctx, ts->msac.rng);
            }

            if (!is_segwedge) {
                if (f->seq_hdr->jnt_comp) {
                    const int jnt_ctx =
                        get_jnt_comp_ctx(f->seq_hdr->order_hint_n_bits,
                                         f->cur.frame_hdr->frame_offset,
                                         f->refp[b->ref[0]].p.frame_hdr->frame_offset,
                                         f->refp[b->ref[1]].p.frame_hdr->frame_offset,
                                         t->a, &t->l, by4, bx4);
                    b->comp_type = COMP_INTER_WEIGHTED_AVG +
                                   dav1d_msac_decode_bool_adapt(&ts->msac,
                                       ts->cdf.m.jnt_comp[jnt_ctx]);
                    if (DEBUG_BLOCK_INFO)
                        printf("Post-jnt_comp[%d,ctx=%d[ac:%d,ar:%d,lc:%d,lr:%d]]: r=%d\n",
                               b->comp_type == COMP_INTER_AVG,
                               jnt_ctx, t->a->comp_type[bx4], t->a->ref[0][bx4],
                               t->l.comp_type[by4], t->l.ref[0][by4],
                               ts->msac.rng);
                } else {
                    b->comp_type = COMP_INTER_AVG;
                }
            } else {
                if (wedge_allowed_mask & (1 << bs)) {
                    const int ctx = dav1d_wedge_ctx_lut[bs];
                    b->comp_type = COMP_INTER_WEDGE -
                                   dav1d_msac_decode_bool_adapt(&ts->msac,
                                       ts->cdf.m.wedge_comp[ctx]);
                    if (b->comp_type == COMP_INTER_WEDGE)
                        b->wedge_idx = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                                           ts->cdf.m.wedge_idx[ctx], 15);
                } else {
                    b->comp_type = COMP_INTER_SEG;
                }
                b->mask_sign = dav1d_msac_decode_bool_equi(&ts->msac);
                if (DEBUG_BLOCK_INFO)
                    printf("Post-seg/wedge[%d,wedge_idx=%d,sign=%d]: r=%d\n",
                           b->comp_type == COMP_INTER_WEDGE,
                           b->wedge_idx, b->mask_sign, ts->msac.rng);
            }
        } else {
            b->comp_type = COMP_INTER_NONE;

            // ref
            if (seg && seg->ref > 0) {
                b->ref[0] = seg->ref - 1;
            } else if (seg && (seg->globalmv || seg->skip)) {
                b->ref[0] = 0;
            } else {
                const int ctx1 = av1_get_ref_ctx(t->a, &t->l, by4, bx4,
                                                 have_top, have_left);
                if (dav1d_msac_decode_bool_adapt(&ts->msac,
                                                 ts->cdf.m.ref[0][ctx1]))
                {
                    const int ctx2 = av1_get_ref_2_ctx(t->a, &t->l, by4, bx4,
                                                       have_top, have_left);
                    if (dav1d_msac_decode_bool_adapt(&ts->msac,
                                                     ts->cdf.m.ref[1][ctx2]))
                    {
                        b->ref[0] = 6;
                    } else {
                        const int ctx3 = av1_get_ref_6_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                        b->ref[0] = 4 + dav1d_msac_decode_bool_adapt(&ts->msac,
                                            ts->cdf.m.ref[5][ctx3]);
                    }
                } else {
                    const int ctx2 = av1_get_ref_3_ctx(t->a, &t->l, by4, bx4,
                                                       have_top, have_left);
                    if (dav1d_msac_decode_bool_adapt(&ts->msac,
                                                     ts->cdf.m.ref[2][ctx2]))
                    {
                        const int ctx3 = av1_get_ref_5_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                        b->ref[0] = 2 + dav1d_msac_decode_bool_adapt(&ts->msac,
                                            ts->cdf.m.ref[4][ctx3]);
                    } else {
                        const int ctx3 = av1_get_ref_4_ctx(t->a, &t->l, by4, bx4,
                                                           have_top, have_left);
                        b->ref[0] = dav1d_msac_decode_bool_adapt(&ts->msac,
                                        ts->cdf.m.ref[3][ctx3]);
                    }
                }
                if (DEBUG_BLOCK_INFO)
                    printf("Post-ref[%d]: r=%d\n", b->ref[0], ts->msac.rng);
            }
            b->ref[1] = -1;

            refmvs_candidate mvstack[8];
            int n_mvs, ctx;
            dav1d_refmvs_find(&t->rt, mvstack, &n_mvs, &ctx,
                              (union refmvs_refpair) { .ref = { b->ref[0] + 1, -1 }},
                              bs, intra_edge_flags, t->by, t->bx);

            // mode parsing and mv derivation from ref_mvs
            if ((seg && (seg->skip || seg->globalmv)) ||
                dav1d_msac_decode_bool_adapt(&ts->msac,
                                             ts->cdf.m.newmv_mode[ctx & 7]))
            {
                if ((seg && (seg->skip || seg->globalmv)) ||
                    !dav1d_msac_decode_bool_adapt(&ts->msac,
                         ts->cdf.m.globalmv_mode[(ctx >> 3) & 1]))
                {
                    b->inter_mode = GLOBALMV;
                    b->mv[0] = get_gmv_2d(&f->frame_hdr->gmv[b->ref[0]],
                                          t->bx, t->by, bw4, bh4, f->frame_hdr);
                    has_subpel_filter = imin(bw4, bh4) == 1 ||
                        f->frame_hdr->gmv[b->ref[0]].type == DAV1D_WM_TYPE_TRANSLATION;
                } else {
                    has_subpel_filter = 1;
                    if (dav1d_msac_decode_bool_adapt(&ts->msac,
                            ts->cdf.m.refmv_mode[(ctx >> 4) & 15]))
                    { // NEAREST, NEARER, NEAR or NEARISH
                        b->inter_mode = NEARMV;
                        b->drl_idx = NEARER_DRL;
                        if (n_mvs > 2) { // NEARER, NEAR or NEARISH
                            const int drl_ctx_v2 = get_drl_context(mvstack, 1);
                            b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                              ts->cdf.m.drl_bit[drl_ctx_v2]);
                            if (b->drl_idx == NEAR_DRL && n_mvs > 3) { // NEAR or NEARISH
                                const int drl_ctx_v3 =
                                    get_drl_context(mvstack, 2);
                                b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                                  ts->cdf.m.drl_bit[drl_ctx_v3]);
                            }
                        }
                    } else {
                        b->inter_mode = NEARESTMV;
                        b->drl_idx = NEAREST_DRL;
                    }
                    assert(b->drl_idx >= NEAREST_DRL && b->drl_idx <= NEARISH_DRL);
                    b->mv[0] = mvstack[b->drl_idx].mv.mv[0];
                    if (b->drl_idx < NEAR_DRL)
                        fix_mv_precision(f->frame_hdr, &b->mv[0]);
                }

                if (DEBUG_BLOCK_INFO)
                    printf("Post-intermode[%d,drl=%d,mv=y:%d,x:%d,n_mvs=%d]: r=%d\n",
                           b->inter_mode, b->drl_idx, b->mv[0].y, b->mv[0].x, n_mvs,
                           ts->msac.rng);
            } else {
                has_subpel_filter = 1;
                b->inter_mode = NEWMV;
                b->drl_idx = NEAREST_DRL;
                if (n_mvs > 1) { // NEARER, NEAR or NEARISH
                    const int drl_ctx_v1 = get_drl_context(mvstack, 0);
                    b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                      ts->cdf.m.drl_bit[drl_ctx_v1]);
                    if (b->drl_idx == NEARER_DRL && n_mvs > 2) { // NEAR or NEARISH
                        const int drl_ctx_v2 = get_drl_context(mvstack, 1);
                        b->drl_idx += dav1d_msac_decode_bool_adapt(&ts->msac,
                                          ts->cdf.m.drl_bit[drl_ctx_v2]);
                    }
                }
                assert(b->drl_idx >= NEAREST_DRL && b->drl_idx <= NEARISH_DRL);
                if (n_mvs > 1) {
                    b->mv[0] = mvstack[b->drl_idx].mv.mv[0];
                } else {
                    assert(!b->drl_idx);
                    b->mv[0] = mvstack[0].mv.mv[0];
                    fix_mv_precision(f->frame_hdr, &b->mv[0]);
                }
                if (DEBUG_BLOCK_INFO)
                    printf("Post-intermode[%d,drl=%d]: r=%d\n",
                           b->inter_mode, b->drl_idx, ts->msac.rng);
                read_mv_residual(t, &b->mv[0], &ts->cdf.mv,
                                 !f->frame_hdr->force_integer_mv);
                if (DEBUG_BLOCK_INFO)
                    printf("Post-residualmv[mv=y:%d,x:%d]: r=%d\n",
                           b->mv[0].y, b->mv[0].x, ts->msac.rng);
            }

            // interintra flags
            const int ii_sz_grp = dav1d_ymode_size_context[bs];
            if (f->seq_hdr->inter_intra &&
                interintra_allowed_mask & (1 << bs) &&
                dav1d_msac_decode_bool_adapt(&ts->msac,
                                             ts->cdf.m.interintra[ii_sz_grp]))
            {
                b->interintra_mode = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                         ts->cdf.m.interintra_mode[ii_sz_grp],
                                         N_INTER_INTRA_PRED_MODES - 1);
                const int wedge_ctx = dav1d_wedge_ctx_lut[bs];
                b->interintra_type = INTER_INTRA_BLEND +
                                     dav1d_msac_decode_bool_adapt(&ts->msac,
                                         ts->cdf.m.interintra_wedge[wedge_ctx]);
                if (b->interintra_type == INTER_INTRA_WEDGE)
                    b->wedge_idx = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                                       ts->cdf.m.wedge_idx[wedge_ctx], 15);
            } else {
                b->interintra_type = INTER_INTRA_NONE;
            }
            if (DEBUG_BLOCK_INFO && f->seq_hdr->inter_intra &&
                interintra_allowed_mask & (1 << bs))
            {
                printf("Post-interintra[t=%d,m=%d,w=%d]: r=%d\n",
                       b->interintra_type, b->interintra_mode,
                       b->wedge_idx, ts->msac.rng);
            }

            // motion variation
            if (f->frame_hdr->switchable_motion_mode &&
                b->interintra_type == INTER_INTRA_NONE && imin(bw4, bh4) >= 2 &&
                // is not warped global motion
                !(!f->frame_hdr->force_integer_mv && b->inter_mode == GLOBALMV &&
                  f->frame_hdr->gmv[b->ref[0]].type > DAV1D_WM_TYPE_TRANSLATION) &&
                // has overlappable neighbours
                ((have_left && findoddzero(&t->l.intra[by4 + 1], h4 >> 1)) ||
                 (have_top && findoddzero(&t->a->intra[bx4 + 1], w4 >> 1))))
            {
                // reaching here means the block allows obmc - check warp by
                // finding matching-ref blocks in top/left edges
                uint64_t mask[2] = { 0, 0 };
                find_matching_ref(t, intra_edge_flags, bw4, bh4, w4, h4,
                                  have_left, have_top, b->ref[0], mask);
                const int allow_warp = !f->svc[b->ref[0]][0].scale &&
                    !f->frame_hdr->force_integer_mv &&
                    f->frame_hdr->warp_motion && (mask[0] | mask[1]);

                b->motion_mode = allow_warp ?
                    dav1d_msac_decode_symbol_adapt4(&ts->msac,
                        ts->cdf.m.motion_mode[bs], 2) :
                    dav1d_msac_decode_bool_adapt(&ts->msac, ts->cdf.m.obmc[bs]);
                if (b->motion_mode == MM_WARP) {
                    has_subpel_filter = 0;
                    derive_warpmv(t, bw4, bh4, mask, b->mv[0], &t->warpmv);
#define signabs(v) v < 0 ? '-' : ' ', abs(v)
                    if (DEBUG_BLOCK_INFO)
                        printf("[ %c%x %c%x %c%x\n  %c%x %c%x %c%x ]\n"
                               "alpha=%c%x, beta=%c%x, gamma=%c%x, delta=%c%x, "
                               "mv=y:%d,x:%d\n",
                               signabs(t->warpmv.matrix[0]),
                               signabs(t->warpmv.matrix[1]),
                               signabs(t->warpmv.matrix[2]),
                               signabs(t->warpmv.matrix[3]),
                               signabs(t->warpmv.matrix[4]),
                               signabs(t->warpmv.matrix[5]),
                               signabs(t->warpmv.u.p.alpha),
                               signabs(t->warpmv.u.p.beta),
                               signabs(t->warpmv.u.p.gamma),
                               signabs(t->warpmv.u.p.delta),
                               b->mv[0].y, b->mv[0].x);
#undef signabs
                    if (t->frame_thread.pass) {
                        if (t->warpmv.type == DAV1D_WM_TYPE_AFFINE) {
                            b->matrix[0] = t->warpmv.matrix[2] - 0x10000;
                            b->matrix[1] = t->warpmv.matrix[3];
                            b->matrix[2] = t->warpmv.matrix[4];
                            b->matrix[3] = t->warpmv.matrix[5] - 0x10000;
                        } else {
                            b->matrix[0] = SHRT_MIN;
                        }
                    }
                }

                if (DEBUG_BLOCK_INFO)
                    printf("Post-motionmode[%d]: r=%d [mask: 0x%" PRIx64 "/0x%"
                           PRIx64 "]\n", b->motion_mode, ts->msac.rng, mask[0],
                            mask[1]);
            } else {
                b->motion_mode = MM_TRANSLATION;
            }
        }

        // subpel filter
        enum Dav1dFilterMode filter[2];
        if (f->frame_hdr->subpel_filter_mode == DAV1D_FILTER_SWITCHABLE) {
            if (has_subpel_filter) {
                const int comp = b->comp_type != COMP_INTER_NONE;
                const int ctx1 = get_filter_ctx(t->a, &t->l, comp, 0, b->ref[0],
                                                by4, bx4);
                filter[0] = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                               ts->cdf.m.filter[0][ctx1],
                               DAV1D_N_SWITCHABLE_FILTERS - 1);
                if (f->seq_hdr->dual_filter) {
                    const int ctx2 = get_filter_ctx(t->a, &t->l, comp, 1,
                                                    b->ref[0], by4, bx4);
                    if (DEBUG_BLOCK_INFO)
                        printf("Post-subpel_filter1[%d,ctx=%d]: r=%d\n",
                               filter[0], ctx1, ts->msac.rng);
                    filter[1] = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                    ts->cdf.m.filter[1][ctx2],
                                    DAV1D_N_SWITCHABLE_FILTERS - 1);
                    if (DEBUG_BLOCK_INFO)
                        printf("Post-subpel_filter2[%d,ctx=%d]: r=%d\n",
                               filter[1], ctx2, ts->msac.rng);
                } else {
                    filter[1] = filter[0];
                    if (DEBUG_BLOCK_INFO)
                        printf("Post-subpel_filter[%d,ctx=%d]: r=%d\n",
                               filter[0], ctx1, ts->msac.rng);
                }
            } else {
                filter[0] = filter[1] = DAV1D_FILTER_8TAP_REGULAR;
            }
        } else {
            filter[0] = filter[1] = f->frame_hdr->subpel_filter_mode;
        }
        b->filter2d = dav1d_filter_2d[filter[1]][filter[0]];

        read_vartx_tree(t, b, bs, bx4, by4);

        // reconstruction
        if (t->frame_thread.pass == 1) {
            f->bd_fn.read_coef_blocks(t, bs, b);
        } else {
            if (f->bd_fn.recon_b_inter(t, bs, b)) return -1;
        }

        if (f->frame_hdr->loopfilter.level_y[0] ||
            f->frame_hdr->loopfilter.level_y[1])
        {
            const int is_globalmv =
                b->inter_mode == (is_comp ? GLOBALMV_GLOBALMV : GLOBALMV);
            const uint8_t (*const lf_lvls)[8][2] = (const uint8_t (*)[8][2])
                &ts->lflvl[b->seg_id][0][b->ref[0] + 1][!is_globalmv];
            const uint16_t tx_split[2] = { b->tx_split0, b->tx_split1 };
            dav1d_create_lf_mask_inter(t->lf_mask, f->lf.level, f->b4_stride, lf_lvls,
                                       t->bx, t->by, f->w4, f->h4, b->skip, bs,
                                       f->frame_hdr->segmentation.lossless[b->seg_id] ?
                                           (enum RectTxfmSize) TX_4X4 : b->max_ytx,
                                       tx_split, b->uvtx, f->cur.p.layout,
                                       &t->a->tx_lpf_y[bx4], &t->l.tx_lpf_y[by4],
                                       has_chroma ? &t->a->tx_lpf_uv[cbx4] : NULL,
                                       has_chroma ? &t->l.tx_lpf_uv[cby4] : NULL);
        }

        // context updates
        if (is_comp)
            splat_tworef_mv(f->c, t, bs, b, bw4, bh4);
        else
            splat_oneref_mv(f->c, t, bs, b, bw4, bh4);

#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir seg_pred, off, mul * seg_pred); \
        rep_macro(type, t->dir skip_mode, off, mul * b->skip_mode); \
        rep_macro(type, t->dir intra, off, 0); \
        rep_macro(type, t->dir skip, off, mul * b->skip); \
        rep_macro(type, t->dir pal_sz, off, 0); \
        /* see aomedia bug 2183 for why this is outside if (has_chroma) */ \
        rep_macro(type, t->pal_sz_uv[diridx], off, 0); \
        rep_macro(type, t->dir tx_intra, off, mul * b_dim[2 + diridx]); \
        rep_macro(type, t->dir comp_type, off, mul * b->comp_type); \
        rep_macro(type, t->dir filter[0], off, mul * filter[0]); \
        rep_macro(type, t->dir filter[1], off, mul * filter[1]); \
        rep_macro(type, t->dir mode, off, mul * b->inter_mode); \
        rep_macro(type, t->dir ref[0], off, mul * b->ref[0]); \
        rep_macro(type, t->dir ref[1], off, mul * ((uint8_t) b->ref[1]))
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx

        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir uvmode, off, mul * DC_PRED)
            case_set(cbh4, l., 1, cby4);
            case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
        }
    }

    // update contexts
    if (f->frame_hdr->segmentation.enabled &&
        f->frame_hdr->segmentation.update_map)
    {
        uint8_t *seg_ptr = &f->cur_segmap[t->by * f->b4_stride + t->bx];
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        for (int y = 0; y < bh4; y++) { \
            rep_macro(type, seg_ptr, 0, mul * b->seg_id); \
            seg_ptr += f->b4_stride; \
        }
        case_set(bw4, NULL, 0, 0);
#undef set_ctx
    }
    if (!b->skip) {
        uint16_t (*noskip_mask)[2] = &t->lf_mask->noskip_mask[by4 >> 1];
        const unsigned mask = (~0U >> (32 - bw4)) << (bx4 & 15);
        const int bx_idx = (bx4 & 16) >> 4;
        for (int y = 0; y < bh4; y += 2, noskip_mask++) {
            (*noskip_mask)[bx_idx] |= mask;
            if (bw4 == 32) // this should be mask >> 16, but it's 0xffffffff anyway
                (*noskip_mask)[1] |= mask;
        }
    }

    if (t->frame_thread.pass == 1 && !b->intra && IS_INTER_OR_SWITCH(f->frame_hdr)) {
        const int sby = (t->by - ts->tiling.row_start) >> f->sb_shift;
        int (*const lowest_px)[2] = ts->lowest_pixel[sby];

        // keep track of motion vectors for each reference
        if (b->comp_type == COMP_INTER_NONE) {
            // y
            if (imin(bw4, bh4) > 1 &&
                ((b->inter_mode == GLOBALMV && f->gmv_warp_allowed[b->ref[0]]) ||
                 (b->motion_mode == MM_WARP && t->warpmv.type > DAV1D_WM_TYPE_TRANSLATION)))
            {
                affine_lowest_px(t, &lowest_px[b->ref[0]][0], 0, b_dim,
                                 b->motion_mode == MM_WARP ? &t->warpmv :
                                     &f->frame_hdr->gmv[b->ref[0]]);
            } else {
                mc_lowest_px(&lowest_px[b->ref[0]][0], t->by, bh4, b->mv[0].y,
                             0, &f->svc[b->ref[0]][1]);
                if (b->motion_mode == MM_OBMC) {
                    obmc_lowest_px(t, lowest_px, 0, b_dim, bx4, by4, w4, h4);
                }
            }

            // uv
            if (has_chroma) {
                // sub8x8 derivation
                int is_sub8x8 = bw4 == ss_hor || bh4 == ss_ver;
                refmvs_block *const *r;
                if (is_sub8x8) {
                    assert(ss_hor == 1);
                    r = &t->rt.r[(t->by & 31) + 5];
                    if (bw4 == 1) is_sub8x8 &= r[0][t->bx - 1].ref.ref[0] > 0;
                    if (bh4 == ss_ver) is_sub8x8 &= r[-1][t->bx].ref.ref[0] > 0;
                    if (bw4 == 1 && bh4 == ss_ver)
                        is_sub8x8 &= r[-1][t->bx - 1].ref.ref[0] > 0;
                }

                // chroma prediction
                if (is_sub8x8) {
                    assert(ss_hor == 1);
                    if (bw4 == 1 && bh4 == ss_ver) {
                        const refmvs_block *const rr = &r[-1][t->bx - 1];
                        mc_lowest_px(&lowest_px[rr->ref.ref[0] - 1][1],
                                     t->by - 1, bh4, rr->mv.mv[0].y, ss_ver,
                                     &f->svc[rr->ref.ref[0] - 1][1]);
                    }
                    if (bw4 == 1) {
                        const refmvs_block *const rr = &r[0][t->bx - 1];
                        mc_lowest_px(&lowest_px[rr->ref.ref[0] - 1][1],
                                     t->by, bh4, rr->mv.mv[0].y, ss_ver,
                                     &f->svc[rr->ref.ref[0] - 1][1]);
                    }
                    if (bh4 == ss_ver) {
                        const refmvs_block *const rr = &r[-1][t->bx];
                        mc_lowest_px(&lowest_px[rr->ref.ref[0] - 1][1],
                                     t->by - 1, bh4, rr->mv.mv[0].y, ss_ver,
                                     &f->svc[rr->ref.ref[0] - 1][1]);
                    }
                    mc_lowest_px(&lowest_px[b->ref[0]][1], t->by, bh4,
                                 b->mv[0].y, ss_ver, &f->svc[b->ref[0]][1]);
                } else {
                    if (imin(cbw4, cbh4) > 1 &&
                        ((b->inter_mode == GLOBALMV && f->gmv_warp_allowed[b->ref[0]]) ||
                         (b->motion_mode == MM_WARP && t->warpmv.type > DAV1D_WM_TYPE_TRANSLATION)))
                    {
                        affine_lowest_px(t, &lowest_px[b->ref[0]][1], 1, b_dim,
                                         b->motion_mode == MM_WARP ? &t->warpmv :
                                            &f->frame_hdr->gmv[b->ref[0]]);
                    } else {
                        mc_lowest_px(&lowest_px[b->ref[0]][1],
                                     t->by & ~ss_ver, bh4 << (bh4 == ss_ver),
                                     b->mv[0].y, ss_ver, &f->svc[b->ref[0]][1]);
                        if (b->motion_mode == MM_OBMC) {
                            obmc_lowest_px(t, lowest_px, 1, b_dim, bx4, by4, w4, h4);
                        }
                    }
                }
            }
        } else {
            // y
            for (int i = 0; i < 2; i++) {
                if (b->inter_mode == GLOBALMV_GLOBALMV && f->gmv_warp_allowed[b->ref[i]]) {
                    affine_lowest_px(t, &lowest_px[b->ref[i]][0], 0, b_dim,
                                     &f->frame_hdr->gmv[b->ref[i]]);
                } else {
                    mc_lowest_px(&lowest_px[b->ref[i]][0], t->by, bh4,
                                 b->mv[i].y, 0, &f->svc[b->ref[i]][1]);
                }
            }

            // uv
            if (has_chroma) for (int i = 0; i < 2; i++) {
                if (b->inter_mode == GLOBALMV_GLOBALMV &&
                    imin(cbw4, cbh4) > 1 && f->gmv_warp_allowed[b->ref[i]])
                {
                    affine_lowest_px(t, &lowest_px[b->ref[i]][1], 1, b_dim,
                                     &f->frame_hdr->gmv[b->ref[i]]);
                } else {
                    mc_lowest_px(&lowest_px[b->ref[i]][1], t->by, bh4,
                                 b->mv[i].y, ss_ver, &f->svc[b->ref[i]][1]);
                }
            }
        }
    }

    return 0;
}

#if __has_feature(memory_sanitizer)

#include <sanitizer/msan_interface.h>

static int checked_decode_b(Dav1dTaskContext *const t,
                            const enum BlockLevel bl,
                            const enum BlockSize bs,
                            const enum BlockPartition bp,
                            const enum EdgeFlags intra_edge_flags)
{
    const Dav1dFrameContext *const f = t->f;
    const int err = decode_b(t, bl, bs, bp, intra_edge_flags);

    if (err == 0 && !(t->frame_thread.pass & 1)) {
        const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const uint8_t *const b_dim = dav1d_block_dimensions[bs];
        const int bw4 = b_dim[0], bh4 = b_dim[1];
        const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
        const int has_chroma = f->seq_hdr->layout != DAV1D_PIXEL_LAYOUT_I400 &&
                               (bw4 > ss_hor || t->bx & 1) &&
                               (bh4 > ss_ver || t->by & 1);

        for (int p = 0; p < 1 + 2 * has_chroma; p++) {
            const int ss_ver = p && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
            const int ss_hor = p && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
            const ptrdiff_t stride = f->cur.stride[!!p];
            const int bx = t->bx & ~ss_hor;
            const int by = t->by & ~ss_ver;
            const int width  = w4 << (2 - ss_hor + (bw4 == ss_hor));
            const int height = h4 << (2 - ss_ver + (bh4 == ss_ver));

            const uint8_t *data = f->cur.data[p] + (by << (2 - ss_ver)) * stride +
                                  (bx << (2 - ss_hor + !!f->seq_hdr->hbd));

            for (int y = 0; y < height; data += stride, y++) {
                const size_t line_sz = width << !!f->seq_hdr->hbd;
                if (__msan_test_shadow(data, line_sz) != -1) {
                    fprintf(stderr, "B[%d](%d, %d) w4:%d, h4:%d, row:%d\n",
                            p, bx, by, w4, h4, y);
                    __msan_check_mem_is_initialized(data, line_sz);
                }
            }
        }
    }

    return err;
}

#define decode_b checked_decode_b

#endif /* defined(__has_feature) */

static int decode_sb(Dav1dTaskContext *const t, const enum BlockLevel bl,
                     const EdgeNode *const node)
{
    const Dav1dFrameContext *const f = t->f;
    Dav1dTileState *const ts = t->ts;
    const int hsz = 16 >> bl;
    const int have_h_split = f->bw > t->bx + hsz;
    const int have_v_split = f->bh > t->by + hsz;

    if (!have_h_split && !have_v_split) {
        assert(bl < BL_8X8);
        return decode_sb(t, bl + 1, ((const EdgeBranch *) node)->split[0]);
    }

    uint16_t *pc;
    enum BlockPartition bp;
    int ctx, bx8, by8;
    if (t->frame_thread.pass != 2) {
        if (0 && bl == BL_64X64)
            printf("poc=%d,y=%d,x=%d,bl=%d,r=%d\n",
                   f->frame_hdr->frame_offset, t->by, t->bx, bl, ts->msac.rng);
        bx8 = (t->bx & 31) >> 1;
        by8 = (t->by & 31) >> 1;
        ctx = get_partition_ctx(t->a, &t->l, bl, by8, bx8);
        pc = ts->cdf.m.partition[bl][ctx];
    }

    if (have_h_split && have_v_split) {
        if (t->frame_thread.pass == 2) {
            const Av1Block *const b = &f->frame_thread.b[t->by * f->b4_stride + t->bx];
            bp = b->bl == bl ? b->bp : PARTITION_SPLIT;
        } else {
            bp = dav1d_msac_decode_symbol_adapt16(&ts->msac, pc,
                                                  dav1d_partition_type_count[bl]);
            if (f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I422 &&
                (bp == PARTITION_V || bp == PARTITION_V4 ||
                 bp == PARTITION_T_LEFT_SPLIT || bp == PARTITION_T_RIGHT_SPLIT))
            {
                return 1;
            }
            if (DEBUG_BLOCK_INFO)
                printf("poc=%d,y=%d,x=%d,bl=%d,ctx=%d,bp=%d: r=%d\n",
                       f->frame_hdr->frame_offset, t->by, t->bx, bl, ctx, bp,
                       ts->msac.rng);
        }
        const uint8_t *const b = dav1d_block_sizes[bl][bp];

        switch (bp) {
        case PARTITION_NONE:
            if (decode_b(t, bl, b[0], PARTITION_NONE, node->o))
                return -1;
            break;
        case PARTITION_H:
            if (decode_b(t, bl, b[0], PARTITION_H, node->h[0]))
                return -1;
            t->by += hsz;
            if (decode_b(t, bl, b[0], PARTITION_H, node->h[1]))
                return -1;
            t->by -= hsz;
            break;
        case PARTITION_V:
            if (decode_b(t, bl, b[0], PARTITION_V, node->v[0]))
                return -1;
            t->bx += hsz;
            if (decode_b(t, bl, b[0], PARTITION_V, node->v[1]))
                return -1;
            t->bx -= hsz;
            break;
        case PARTITION_SPLIT:
            if (bl == BL_8X8) {
                const EdgeTip *const tip = (const EdgeTip *) node;
                assert(hsz == 1);
                if (decode_b(t, bl, BS_4x4, PARTITION_SPLIT, tip->split[0]))
                    return -1;
                const enum Filter2d tl_filter = t->tl_4x4_filter;
                t->bx++;
                if (decode_b(t, bl, BS_4x4, PARTITION_SPLIT, tip->split[1]))
                    return -1;
                t->bx--;
                t->by++;
                if (decode_b(t, bl, BS_4x4, PARTITION_SPLIT, tip->split[2]))
                    return -1;
                t->bx++;
                t->tl_4x4_filter = tl_filter;
                if (decode_b(t, bl, BS_4x4, PARTITION_SPLIT, tip->split[3]))
                    return -1;
                t->bx--;
                t->by--;
#if ARCH_X86_64
                if (t->frame_thread.pass) {
                    /* In 8-bit mode with 2-pass decoding the coefficient buffer
                     * can end up misaligned due to skips here. Work around
                     * the issue by explicitly realigning the buffer. */
                    const int p = t->frame_thread.pass & 1;
                    ts->frame_thread[p].cf =
                        (void*)(((uintptr_t)ts->frame_thread[p].cf + 63) & ~63);
                }
#endif
            } else {
                const EdgeBranch *const branch = (const EdgeBranch *) node;
                if (decode_sb(t, bl + 1, branch->split[0]))
                    return 1;
                t->bx += hsz;
                if (decode_sb(t, bl + 1, branch->split[1]))
                    return 1;
                t->bx -= hsz;
                t->by += hsz;
                if (decode_sb(t, bl + 1, branch->split[2]))
                    return 1;
                t->bx += hsz;
                if (decode_sb(t, bl + 1, branch->split[3]))
                    return 1;
                t->bx -= hsz;
                t->by -= hsz;
            }
            break;
        case PARTITION_T_TOP_SPLIT: {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            if (decode_b(t, bl, b[0], PARTITION_T_TOP_SPLIT, branch->tts[0]))
                return -1;
            t->bx += hsz;
            if (decode_b(t, bl, b[0], PARTITION_T_TOP_SPLIT, branch->tts[1]))
                return -1;
            t->bx -= hsz;
            t->by += hsz;
            if (decode_b(t, bl, b[1], PARTITION_T_TOP_SPLIT, branch->tts[2]))
                return -1;
            t->by -= hsz;
            break;
        }
        case PARTITION_T_BOTTOM_SPLIT: {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            if (decode_b(t, bl, b[0], PARTITION_T_BOTTOM_SPLIT, branch->tbs[0]))
                return -1;
            t->by += hsz;
            if (decode_b(t, bl, b[1], PARTITION_T_BOTTOM_SPLIT, branch->tbs[1]))
                return -1;
            t->bx += hsz;
            if (decode_b(t, bl, b[1], PARTITION_T_BOTTOM_SPLIT, branch->tbs[2]))
                return -1;
            t->bx -= hsz;
            t->by -= hsz;
            break;
        }
        case PARTITION_T_LEFT_SPLIT: {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            if (decode_b(t, bl, b[0], PARTITION_T_LEFT_SPLIT, branch->tls[0]))
                return -1;
            t->by += hsz;
            if (decode_b(t, bl, b[0], PARTITION_T_LEFT_SPLIT, branch->tls[1]))
                return -1;
            t->by -= hsz;
            t->bx += hsz;
            if (decode_b(t, bl, b[1], PARTITION_T_LEFT_SPLIT, branch->tls[2]))
                return -1;
            t->bx -= hsz;
            break;
        }
        case PARTITION_T_RIGHT_SPLIT: {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            if (decode_b(t, bl, b[0], PARTITION_T_RIGHT_SPLIT, branch->trs[0]))
                return -1;
            t->bx += hsz;
            if (decode_b(t, bl, b[1], PARTITION_T_RIGHT_SPLIT, branch->trs[1]))
                return -1;
            t->by += hsz;
            if (decode_b(t, bl, b[1], PARTITION_T_RIGHT_SPLIT, branch->trs[2]))
                return -1;
            t->by -= hsz;
            t->bx -= hsz;
            break;
        }
        case PARTITION_H4: {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            if (decode_b(t, bl, b[0], PARTITION_H4, branch->h4[0]))
                return -1;
            t->by += hsz >> 1;
            if (decode_b(t, bl, b[0], PARTITION_H4, branch->h4[1]))
                return -1;
            t->by += hsz >> 1;
            if (decode_b(t, bl, b[0], PARTITION_H4, branch->h4[2]))
                return -1;
            t->by += hsz >> 1;
            if (t->by < f->bh)
                if (decode_b(t, bl, b[0], PARTITION_H4, branch->h4[3]))
                    return -1;
            t->by -= hsz * 3 >> 1;
            break;
        }
        case PARTITION_V4: {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            if (decode_b(t, bl, b[0], PARTITION_V4, branch->v4[0]))
                return -1;
            t->bx += hsz >> 1;
            if (decode_b(t, bl, b[0], PARTITION_V4, branch->v4[1]))
                return -1;
            t->bx += hsz >> 1;
            if (decode_b(t, bl, b[0], PARTITION_V4, branch->v4[2]))
                return -1;
            t->bx += hsz >> 1;
            if (t->bx < f->bw)
                if (decode_b(t, bl, b[0], PARTITION_V4, branch->v4[3]))
                    return -1;
            t->bx -= hsz * 3 >> 1;
            break;
        }
        default: assert(0);
        }
    } else if (have_h_split) {
        unsigned is_split;
        if (t->frame_thread.pass == 2) {
            const Av1Block *const b = &f->frame_thread.b[t->by * f->b4_stride + t->bx];
            is_split = b->bl != bl;
        } else {
            is_split = dav1d_msac_decode_bool(&ts->msac,
                           gather_top_partition_prob(pc, bl));
            if (DEBUG_BLOCK_INFO)
                printf("poc=%d,y=%d,x=%d,bl=%d,ctx=%d,bp=%d: r=%d\n",
                       f->frame_hdr->frame_offset, t->by, t->bx, bl, ctx,
                       is_split ? PARTITION_SPLIT : PARTITION_H, ts->msac.rng);
        }

        assert(bl < BL_8X8);
        if (is_split) {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            bp = PARTITION_SPLIT;
            if (decode_sb(t, bl + 1, branch->split[0])) return 1;
            t->bx += hsz;
            if (decode_sb(t, bl + 1, branch->split[1])) return 1;
            t->bx -= hsz;
        } else {
            bp = PARTITION_H;
            if (decode_b(t, bl, dav1d_block_sizes[bl][PARTITION_H][0],
                         PARTITION_H, node->h[0]))
                return -1;
        }
    } else {
        assert(have_v_split);
        unsigned is_split;
        if (t->frame_thread.pass == 2) {
            const Av1Block *const b = &f->frame_thread.b[t->by * f->b4_stride + t->bx];
            is_split = b->bl != bl;
        } else {
            is_split = dav1d_msac_decode_bool(&ts->msac,
                           gather_left_partition_prob(pc, bl));
            if (f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I422 && !is_split)
                return 1;
            if (DEBUG_BLOCK_INFO)
                printf("poc=%d,y=%d,x=%d,bl=%d,ctx=%d,bp=%d: r=%d\n",
                       f->frame_hdr->frame_offset, t->by, t->bx, bl, ctx,
                       is_split ? PARTITION_SPLIT : PARTITION_V, ts->msac.rng);
        }

        assert(bl < BL_8X8);
        if (is_split) {
            const EdgeBranch *const branch = (const EdgeBranch *) node;
            bp = PARTITION_SPLIT;
            if (decode_sb(t, bl + 1, branch->split[0])) return 1;
            t->by += hsz;
            if (decode_sb(t, bl + 1, branch->split[2])) return 1;
            t->by -= hsz;
        } else {
            bp = PARTITION_V;
            if (decode_b(t, bl, dav1d_block_sizes[bl][PARTITION_V][0],
                         PARTITION_V, node->v[0]))
                return -1;
        }
    }

    if (t->frame_thread.pass != 2 && (bp != PARTITION_SPLIT || bl == BL_8X8)) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->a->partition, bx8, mul * dav1d_al_part_ctx[0][bl][bp]); \
        rep_macro(type, t->l.partition, by8, mul * dav1d_al_part_ctx[1][bl][bp])
        case_set_upto16(hsz,,,);
#undef set_ctx
    }

    return 0;
}

static void reset_context(BlockContext *const ctx, const int keyframe, const int pass) {
    memset(ctx->intra, keyframe, sizeof(ctx->intra));
    memset(ctx->uvmode, DC_PRED, sizeof(ctx->uvmode));
    if (keyframe)
        memset(ctx->mode, DC_PRED, sizeof(ctx->mode));

    if (pass == 2) return;

    memset(ctx->partition, 0, sizeof(ctx->partition));
    memset(ctx->skip, 0, sizeof(ctx->skip));
    memset(ctx->skip_mode, 0, sizeof(ctx->skip_mode));
    memset(ctx->tx_lpf_y, 2, sizeof(ctx->tx_lpf_y));
    memset(ctx->tx_lpf_uv, 1, sizeof(ctx->tx_lpf_uv));
    memset(ctx->tx_intra, -1, sizeof(ctx->tx_intra));
    memset(ctx->tx, TX_64X64, sizeof(ctx->tx));
    if (!keyframe) {
        memset(ctx->ref, -1, sizeof(ctx->ref));
        memset(ctx->comp_type, 0, sizeof(ctx->comp_type));
        memset(ctx->mode, NEARESTMV, sizeof(ctx->mode));
    }
    memset(ctx->lcoef, 0x40, sizeof(ctx->lcoef));
    memset(ctx->ccoef, 0x40, sizeof(ctx->ccoef));
    memset(ctx->filter, DAV1D_N_SWITCHABLE_FILTERS, sizeof(ctx->filter));
    memset(ctx->seg_pred, 0, sizeof(ctx->seg_pred));
    memset(ctx->pal_sz, 0, sizeof(ctx->pal_sz));
}

// { Y+U+V, Y+U } * 4
static const uint8_t ss_size_mul[4][2] = {
    [DAV1D_PIXEL_LAYOUT_I400] = {  4, 4 },
    [DAV1D_PIXEL_LAYOUT_I420] = {  6, 5 },
    [DAV1D_PIXEL_LAYOUT_I422] = {  8, 6 },
    [DAV1D_PIXEL_LAYOUT_I444] = { 12, 8 },
};

static void setup_tile(Dav1dTileState *const ts,
                       const Dav1dFrameContext *const f,
                       const uint8_t *const data, const size_t sz,
                       const int tile_row, const int tile_col,
                       const int tile_start_off)
{
    const int col_sb_start = f->frame_hdr->tiling.col_start_sb[tile_col];
    const int col_sb128_start = col_sb_start >> !f->seq_hdr->sb128;
    const int col_sb_end = f->frame_hdr->tiling.col_start_sb[tile_col + 1];
    const int row_sb_start = f->frame_hdr->tiling.row_start_sb[tile_row];
    const int row_sb_end = f->frame_hdr->tiling.row_start_sb[tile_row + 1];
    const int sb_shift = f->sb_shift;

    const uint8_t *const size_mul = ss_size_mul[f->cur.p.layout];
    for (int p = 0; p < 2; p++) {
        ts->frame_thread[p].pal_idx = f->frame_thread.pal_idx ?
            &f->frame_thread.pal_idx[(size_t)tile_start_off * size_mul[1] / 4] :
            NULL;
        ts->frame_thread[p].cf = f->frame_thread.cf ?
            (uint8_t*)f->frame_thread.cf +
                (((size_t)tile_start_off * size_mul[0]) >> !f->seq_hdr->hbd) :
            NULL;
    }

    dav1d_cdf_thread_copy(&ts->cdf, &f->in_cdf);
    ts->last_qidx = f->frame_hdr->quant.yac;
    memset(ts->last_delta_lf, 0, sizeof(ts->last_delta_lf));

    dav1d_msac_init(&ts->msac, data, sz, f->frame_hdr->disable_cdf_update);

    ts->tiling.row = tile_row;
    ts->tiling.col = tile_col;
    ts->tiling.col_start = col_sb_start << sb_shift;
    ts->tiling.col_end = imin(col_sb_end << sb_shift, f->bw);
    ts->tiling.row_start = row_sb_start << sb_shift;
    ts->tiling.row_end = imin(row_sb_end << sb_shift, f->bh);

    // Reference Restoration Unit (used for exp coding)
    int sb_idx, unit_idx;
    if (f->frame_hdr->width[0] != f->frame_hdr->width[1]) {
        // vertical components only
        sb_idx = (ts->tiling.row_start >> 5) * f->sr_sb128w;
        unit_idx = (ts->tiling.row_start & 16) >> 3;
    } else {
        sb_idx = (ts->tiling.row_start >> 5) * f->sb128w + col_sb128_start;
        unit_idx = ((ts->tiling.row_start & 16) >> 3) +
                   ((ts->tiling.col_start & 16) >> 4);
    }
    for (int p = 0; p < 3; p++) {
        if (!((f->lf.restore_planes >> p) & 1U))
            continue;

        if (f->frame_hdr->width[0] != f->frame_hdr->width[1]) {
            const int ss_hor = p && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
            const int d = f->frame_hdr->super_res.width_scale_denominator;
            const int unit_size_log2 = f->frame_hdr->restoration.unit_size[!!p];
            const int rnd = (8 << unit_size_log2) - 1, shift = unit_size_log2 + 3;
            const int x = ((4 * ts->tiling.col_start * d >> ss_hor) + rnd) >> shift;
            const int px_x = x << (unit_size_log2 + ss_hor);
            const int u_idx = unit_idx + ((px_x & 64) >> 6);
            const int sb128x = px_x >> 7;
            if (sb128x >= f->sr_sb128w) continue;
            ts->lr_ref[p] = &f->lf.lr_mask[sb_idx + sb128x].lr[p][u_idx];
        } else {
            ts->lr_ref[p] = &f->lf.lr_mask[sb_idx].lr[p][unit_idx];
        }

        ts->lr_ref[p]->filter_v[0] = 3;
        ts->lr_ref[p]->filter_v[1] = -7;
        ts->lr_ref[p]->filter_v[2] = 15;
        ts->lr_ref[p]->filter_h[0] = 3;
        ts->lr_ref[p]->filter_h[1] = -7;
        ts->lr_ref[p]->filter_h[2] = 15;
        ts->lr_ref[p]->sgr_weights[0] = -32;
        ts->lr_ref[p]->sgr_weights[1] = 31;
    }

    if (f->c->n_tc > 1) {
        for (int p = 0; p < 2; p++)
            atomic_init(&ts->progress[p], row_sb_start);
    }
}

static void read_restoration_info(Dav1dTaskContext *const t,
                                  Av1RestorationUnit *const lr, const int p,
                                  const enum Dav1dRestorationType frame_type)
{
    const Dav1dFrameContext *const f = t->f;
    Dav1dTileState *const ts = t->ts;

    if (frame_type == DAV1D_RESTORATION_SWITCHABLE) {
        const int filter = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                               ts->cdf.m.restore_switchable, 2);
        lr->type = filter ? filter == 2 ? DAV1D_RESTORATION_SGRPROJ :
                                          DAV1D_RESTORATION_WIENER :
                                          DAV1D_RESTORATION_NONE;
    } else {
        const unsigned type =
            dav1d_msac_decode_bool_adapt(&ts->msac,
                frame_type == DAV1D_RESTORATION_WIENER ?
                ts->cdf.m.restore_wiener : ts->cdf.m.restore_sgrproj);
        lr->type = type ? frame_type : DAV1D_RESTORATION_NONE;
    }

    if (lr->type == DAV1D_RESTORATION_WIENER) {
        lr->filter_v[0] = p ? 0 :
            dav1d_msac_decode_subexp(&ts->msac,
                ts->lr_ref[p]->filter_v[0] + 5, 16, 1) - 5;
        lr->filter_v[1] =
            dav1d_msac_decode_subexp(&ts->msac,
                ts->lr_ref[p]->filter_v[1] + 23, 32, 2) - 23;
        lr->filter_v[2] =
            dav1d_msac_decode_subexp(&ts->msac,
                ts->lr_ref[p]->filter_v[2] + 17, 64, 3) - 17;

        lr->filter_h[0] = p ? 0 :
            dav1d_msac_decode_subexp(&ts->msac,
                ts->lr_ref[p]->filter_h[0] + 5, 16, 1) - 5;
        lr->filter_h[1] =
            dav1d_msac_decode_subexp(&ts->msac,
                ts->lr_ref[p]->filter_h[1] + 23, 32, 2) - 23;
        lr->filter_h[2] =
            dav1d_msac_decode_subexp(&ts->msac,
                ts->lr_ref[p]->filter_h[2] + 17, 64, 3) - 17;
        memcpy(lr->sgr_weights, ts->lr_ref[p]->sgr_weights, sizeof(lr->sgr_weights));
        ts->lr_ref[p] = lr;
        if (DEBUG_BLOCK_INFO)
            printf("Post-lr_wiener[pl=%d,v[%d,%d,%d],h[%d,%d,%d]]: r=%d\n",
                   p, lr->filter_v[0], lr->filter_v[1],
                   lr->filter_v[2], lr->filter_h[0],
                   lr->filter_h[1], lr->filter_h[2], ts->msac.rng);
    } else if (lr->type == DAV1D_RESTORATION_SGRPROJ) {
        const unsigned idx = dav1d_msac_decode_bools(&ts->msac, 4);
        const uint16_t *const sgr_params = dav1d_sgr_params[idx];
        lr->sgr_idx = idx;
        lr->sgr_weights[0] = sgr_params[0] ? dav1d_msac_decode_subexp(&ts->msac,
            ts->lr_ref[p]->sgr_weights[0] + 96, 128, 4) - 96 : 0;
        lr->sgr_weights[1] = sgr_params[1] ? dav1d_msac_decode_subexp(&ts->msac,
            ts->lr_ref[p]->sgr_weights[1] + 32, 128, 4) - 32 : 95;
        memcpy(lr->filter_v, ts->lr_ref[p]->filter_v, sizeof(lr->filter_v));
        memcpy(lr->filter_h, ts->lr_ref[p]->filter_h, sizeof(lr->filter_h));
        ts->lr_ref[p] = lr;
        if (DEBUG_BLOCK_INFO)
            printf("Post-lr_sgrproj[pl=%d,idx=%d,w[%d,%d]]: r=%d\n",
                   p, lr->sgr_idx, lr->sgr_weights[0],
                   lr->sgr_weights[1], ts->msac.rng);
    }
}

int dav1d_decode_tile_sbrow(Dav1dTaskContext *const t) {
    const Dav1dFrameContext *const f = t->f;
    const enum BlockLevel root_bl = f->seq_hdr->sb128 ? BL_128X128 : BL_64X64;
    Dav1dTileState *const ts = t->ts;
    const Dav1dContext *const c = f->c;
    const int sb_step = f->sb_step;
    const int tile_row = ts->tiling.row, tile_col = ts->tiling.col;
    const int col_sb_start = f->frame_hdr->tiling.col_start_sb[tile_col];
    const int col_sb128_start = col_sb_start >> !f->seq_hdr->sb128;

    if (IS_INTER_OR_SWITCH(f->frame_hdr) || f->frame_hdr->allow_intrabc) {
        dav1d_refmvs_tile_sbrow_init(&t->rt, &f->rf, ts->tiling.col_start,
                                     ts->tiling.col_end, ts->tiling.row_start,
                                     ts->tiling.row_end, t->by >> f->sb_shift,
                                     ts->tiling.row, t->frame_thread.pass);
    }

    if (IS_INTER_OR_SWITCH(f->frame_hdr) && c->n_fc > 1) {
        const int sby = (t->by - ts->tiling.row_start) >> f->sb_shift;
        int (*const lowest_px)[2] = ts->lowest_pixel[sby];
        for (int n = 0; n < 7; n++)
            for (int m = 0; m < 2; m++)
                lowest_px[n][m] = INT_MIN;
    }

    reset_context(&t->l, IS_KEY_OR_INTRA(f->frame_hdr), t->frame_thread.pass);
    if (t->frame_thread.pass == 2) {
        const int off_2pass = c->n_tc > 1 ? f->sb128w * f->frame_hdr->tiling.rows : 0;
        for (t->bx = ts->tiling.col_start,
             t->a = f->a + off_2pass + col_sb128_start + tile_row * f->sb128w;
             t->bx < ts->tiling.col_end; t->bx += sb_step)
        {
            if (atomic_load_explicit(c->flush, memory_order_acquire))
                return 1;
            if (decode_sb(t, root_bl, c->intra_edge.root[root_bl]))
                return 1;
            if (t->bx & 16 || f->seq_hdr->sb128)
                t->a++;
        }
        f->bd_fn.backup_ipred_edge(t);
        return 0;
    }

    // error out on symbol decoder overread
    if (ts->msac.cnt < -15) return 1;

    if (f->c->n_tc > 1 && f->frame_hdr->use_ref_frame_mvs) {
        dav1d_refmvs_load_tmvs(&f->rf, ts->tiling.row,
                               ts->tiling.col_start >> 1, ts->tiling.col_end >> 1,
                               t->by >> 1, (t->by + sb_step) >> 1);
    }
    memset(t->pal_sz_uv[1], 0, sizeof(*t->pal_sz_uv));
    const int sb128y = t->by >> 5;
    for (t->bx = ts->tiling.col_start, t->a = f->a + col_sb128_start + tile_row * f->sb128w,
         t->lf_mask = f->lf.mask + sb128y * f->sb128w + col_sb128_start;
         t->bx < ts->tiling.col_end; t->bx += sb_step)
    {
        if (atomic_load_explicit(c->flush, memory_order_acquire))
            return 1;
        if (root_bl == BL_128X128) {
            t->cur_sb_cdef_idx_ptr = t->lf_mask->cdef_idx;
            t->cur_sb_cdef_idx_ptr[0] = -1;
            t->cur_sb_cdef_idx_ptr[1] = -1;
            t->cur_sb_cdef_idx_ptr[2] = -1;
            t->cur_sb_cdef_idx_ptr[3] = -1;
        } else {
            t->cur_sb_cdef_idx_ptr =
                &t->lf_mask->cdef_idx[((t->bx & 16) >> 4) +
                                      ((t->by & 16) >> 3)];
            t->cur_sb_cdef_idx_ptr[0] = -1;
        }
        // Restoration filter
        for (int p = 0; p < 3; p++) {
            if (!((f->lf.restore_planes >> p) & 1U))
                continue;

            const int ss_ver = p && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
            const int ss_hor = p && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
            const int unit_size_log2 = f->frame_hdr->restoration.unit_size[!!p];
            const int y = t->by * 4 >> ss_ver;
            const int h = (f->cur.p.h + ss_ver) >> ss_ver;

            const int unit_size = 1 << unit_size_log2;
            const unsigned mask = unit_size - 1;
            if (y & mask) continue;
            const int half_unit = unit_size >> 1;
            // Round half up at frame boundaries, if there's more than one
            // restoration unit
            if (y && y + half_unit > h) continue;

            const enum Dav1dRestorationType frame_type = f->frame_hdr->restoration.type[p];

            if (f->frame_hdr->width[0] != f->frame_hdr->width[1]) {
                const int w = (f->sr_cur.p.p.w + ss_hor) >> ss_hor;
                const int n_units = imax(1, (w + half_unit) >> unit_size_log2);

                const int d = f->frame_hdr->super_res.width_scale_denominator;
                const int rnd = unit_size * 8 - 1, shift = unit_size_log2 + 3;
                const int x0 = ((4 *  t->bx            * d >> ss_hor) + rnd) >> shift;
                const int x1 = ((4 * (t->bx + sb_step) * d >> ss_hor) + rnd) >> shift;

                for (int x = x0; x < imin(x1, n_units); x++) {
                    const int px_x = x << (unit_size_log2 + ss_hor);
                    const int sb_idx = (t->by >> 5) * f->sr_sb128w + (px_x >> 7);
                    const int unit_idx = ((t->by & 16) >> 3) + ((px_x & 64) >> 6);
                    Av1RestorationUnit *const lr = &f->lf.lr_mask[sb_idx].lr[p][unit_idx];

                    read_restoration_info(t, lr, p, frame_type);
                }
            } else {
                const int x = 4 * t->bx >> ss_hor;
                if (x & mask) continue;
                const int w = (f->cur.p.w + ss_hor) >> ss_hor;
                // Round half up at frame boundaries, if there's more than one
                // restoration unit
                if (x && x + half_unit > w) continue;
                const int sb_idx = (t->by >> 5) * f->sr_sb128w + (t->bx >> 5);
                const int unit_idx = ((t->by & 16) >> 3) + ((t->bx & 16) >> 4);
                Av1RestorationUnit *const lr = &f->lf.lr_mask[sb_idx].lr[p][unit_idx];

                read_restoration_info(t, lr, p, frame_type);
            }
        }
        if (decode_sb(t, root_bl, c->intra_edge.root[root_bl]))
            return 1;
        if (t->bx & 16 || f->seq_hdr->sb128) {
            t->a++;
            t->lf_mask++;
        }
    }

    if (f->seq_hdr->ref_frame_mvs && f->c->n_tc > 1 && IS_INTER_OR_SWITCH(f->frame_hdr)) {
        dav1d_refmvs_save_tmvs(&t->rt,
                               ts->tiling.col_start >> 1, ts->tiling.col_end >> 1,
                               t->by >> 1, (t->by + sb_step) >> 1);
    }

    // backup pre-loopfilter pixels for intra prediction of the next sbrow
    if (t->frame_thread.pass != 1)
        f->bd_fn.backup_ipred_edge(t);

    // backup t->a/l.tx_lpf_y/uv at tile boundaries to use them to "fix"
    // up the initial value in neighbour tiles when running the loopfilter
    int align_h = (f->bh + 31) & ~31;
    memcpy(&f->lf.tx_lpf_right_edge[0][align_h * tile_col + t->by],
           &t->l.tx_lpf_y[t->by & 16], sb_step);
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    align_h >>= ss_ver;
    memcpy(&f->lf.tx_lpf_right_edge[1][align_h * tile_col + (t->by >> ss_ver)],
           &t->l.tx_lpf_uv[(t->by & 16) >> ss_ver], sb_step >> ss_ver);

    return 0;
}

int dav1d_decode_frame_init(Dav1dFrameContext *const f) {
    const Dav1dContext *const c = f->c;
    int retval = DAV1D_ERR(ENOMEM);

    if (f->sbh > f->lf.start_of_tile_row_sz) {
        free(f->lf.start_of_tile_row);
        f->lf.start_of_tile_row = malloc(f->sbh * sizeof(uint8_t));
        if (!f->lf.start_of_tile_row) {
            f->lf.start_of_tile_row_sz = 0;
            goto error;
        }
        f->lf.start_of_tile_row_sz = f->sbh;
    }
    int sby = 0;
    for (int tile_row = 0; tile_row < f->frame_hdr->tiling.rows; tile_row++) {
        f->lf.start_of_tile_row[sby++] = tile_row;
        while (sby < f->frame_hdr->tiling.row_start_sb[tile_row + 1])
            f->lf.start_of_tile_row[sby++] = 0;
    }

    const int n_ts = f->frame_hdr->tiling.cols * f->frame_hdr->tiling.rows;
    if (n_ts != f->n_ts) {
        if (c->n_fc > 1) {
            freep(&f->frame_thread.tile_start_off);
            f->frame_thread.tile_start_off =
                malloc(sizeof(*f->frame_thread.tile_start_off) * n_ts);
            if (!f->frame_thread.tile_start_off) {
                f->n_ts = 0;
                goto error;
            }
        }
        dav1d_free_aligned(f->ts);
        f->ts = dav1d_alloc_aligned(sizeof(*f->ts) * n_ts, 32);
        if (!f->ts) goto error;
        f->n_ts = n_ts;
    }

    const int a_sz = f->sb128w * f->frame_hdr->tiling.rows * (1 + (c->n_fc > 1 && c->n_tc > 1));
    if (a_sz != f->a_sz) {
        freep(&f->a);
        f->a = malloc(sizeof(*f->a) * a_sz);
        if (!f->a) {
            f->a_sz = 0;
            goto error;
        }
        f->a_sz = a_sz;
    }

    const int num_sb128 = f->sb128w * f->sb128h;
    const uint8_t *const size_mul = ss_size_mul[f->cur.p.layout];
    const int hbd = !!f->seq_hdr->hbd;
    if (c->n_fc > 1) {
        int tile_idx = 0;
        for (int tile_row = 0; tile_row < f->frame_hdr->tiling.rows; tile_row++) {
            int row_off = f->frame_hdr->tiling.row_start_sb[tile_row] *
                          f->sb_step * 4 * f->sb128w * 128;
            int b_diff = (f->frame_hdr->tiling.row_start_sb[tile_row + 1] -
                          f->frame_hdr->tiling.row_start_sb[tile_row]) * f->sb_step * 4;
            for (int tile_col = 0; tile_col < f->frame_hdr->tiling.cols; tile_col++) {
                f->frame_thread.tile_start_off[tile_idx++] = row_off + b_diff *
                    f->frame_hdr->tiling.col_start_sb[tile_col] * f->sb_step * 4;
            }
        }

        const int lowest_pixel_mem_sz = f->frame_hdr->tiling.cols * f->sbh;
        if (lowest_pixel_mem_sz != f->tile_thread.lowest_pixel_mem_sz) {
            free(f->tile_thread.lowest_pixel_mem);
            f->tile_thread.lowest_pixel_mem =
                malloc(lowest_pixel_mem_sz * sizeof(*f->tile_thread.lowest_pixel_mem));
            if (!f->tile_thread.lowest_pixel_mem) {
                f->tile_thread.lowest_pixel_mem_sz = 0;
                goto error;
            }
            f->tile_thread.lowest_pixel_mem_sz = lowest_pixel_mem_sz;
        }
        int (*lowest_pixel_ptr)[7][2] = f->tile_thread.lowest_pixel_mem;
        for (int tile_row = 0, tile_row_base = 0; tile_row < f->frame_hdr->tiling.rows;
             tile_row++, tile_row_base += f->frame_hdr->tiling.cols)
        {
            const int tile_row_sb_h = f->frame_hdr->tiling.row_start_sb[tile_row + 1] -
                                      f->frame_hdr->tiling.row_start_sb[tile_row];
            for (int tile_col = 0; tile_col < f->frame_hdr->tiling.cols; tile_col++) {
                f->ts[tile_row_base + tile_col].lowest_pixel = lowest_pixel_ptr;
                lowest_pixel_ptr += tile_row_sb_h;
            }
        }

        const int cf_sz = (num_sb128 * size_mul[0]) << hbd;
        if (cf_sz != f->frame_thread.cf_sz) {
            dav1d_freep_aligned(&f->frame_thread.cf);
            f->frame_thread.cf =
                dav1d_alloc_aligned((size_t)cf_sz * 128 * 128 / 2, 64);
            if (!f->frame_thread.cf) {
                f->frame_thread.cf_sz = 0;
                goto error;
            }
            memset(f->frame_thread.cf, 0, (size_t)cf_sz * 128 * 128 / 2);
            f->frame_thread.cf_sz = cf_sz;
        }

        if (f->frame_hdr->allow_screen_content_tools) {
            if (num_sb128 != f->frame_thread.pal_sz) {
                dav1d_freep_aligned(&f->frame_thread.pal);
                f->frame_thread.pal =
                    dav1d_alloc_aligned(sizeof(*f->frame_thread.pal) *
                                        num_sb128 * 16 * 16, 64);
                if (!f->frame_thread.pal) {
                    f->frame_thread.pal_sz = 0;
                    goto error;
                }
                f->frame_thread.pal_sz = num_sb128;
            }

            const int pal_idx_sz = num_sb128 * size_mul[1];
            if (pal_idx_sz != f->frame_thread.pal_idx_sz) {
                dav1d_freep_aligned(&f->frame_thread.pal_idx);
                f->frame_thread.pal_idx =
                    dav1d_alloc_aligned(sizeof(*f->frame_thread.pal_idx) *
                                        pal_idx_sz * 128 * 128 / 4, 64);
                if (!f->frame_thread.pal_idx) {
                    f->frame_thread.pal_idx_sz = 0;
                    goto error;
                }
                f->frame_thread.pal_idx_sz = pal_idx_sz;
            }
        } else if (f->frame_thread.pal) {
            dav1d_freep_aligned(&f->frame_thread.pal);
            dav1d_freep_aligned(&f->frame_thread.pal_idx);
            f->frame_thread.pal_sz = f->frame_thread.pal_idx_sz = 0;
        }
    }

    // update allocation of block contexts for above
    ptrdiff_t y_stride = f->cur.stride[0], uv_stride = f->cur.stride[1];
    const int has_resize = f->frame_hdr->width[0] != f->frame_hdr->width[1];
    const int need_cdef_lpf_copy = c->n_tc > 1 && has_resize;
    if (y_stride * f->sbh * 4 != f->lf.cdef_buf_plane_sz[0] ||
        uv_stride * f->sbh * 8 != f->lf.cdef_buf_plane_sz[1] ||
        need_cdef_lpf_copy != f->lf.need_cdef_lpf_copy ||
        f->sbh != f->lf.cdef_buf_sbh)
    {
        dav1d_free_aligned(f->lf.cdef_line_buf);
        size_t alloc_sz = 64;
        alloc_sz += (size_t)llabs(y_stride) * 4 * f->sbh << need_cdef_lpf_copy;
        alloc_sz += (size_t)llabs(uv_stride) * 8 * f->sbh << need_cdef_lpf_copy;
        uint8_t *ptr = f->lf.cdef_line_buf = dav1d_alloc_aligned(alloc_sz, 32);
        if (!ptr) {
            f->lf.cdef_buf_plane_sz[0] = f->lf.cdef_buf_plane_sz[1] = 0;
            goto error;
        }

        ptr += 32;
        if (y_stride < 0) {
            f->lf.cdef_line[0][0] = ptr - y_stride * (f->sbh * 4 - 1);
            f->lf.cdef_line[1][0] = ptr - y_stride * (f->sbh * 4 - 3);
        } else {
            f->lf.cdef_line[0][0] = ptr + y_stride * 0;
            f->lf.cdef_line[1][0] = ptr + y_stride * 2;
        }
        ptr += llabs(y_stride) * f->sbh * 4;
        if (uv_stride < 0) {
            f->lf.cdef_line[0][1] = ptr - uv_stride * (f->sbh * 8 - 1);
            f->lf.cdef_line[0][2] = ptr - uv_stride * (f->sbh * 8 - 3);
            f->lf.cdef_line[1][1] = ptr - uv_stride * (f->sbh * 8 - 5);
            f->lf.cdef_line[1][2] = ptr - uv_stride * (f->sbh * 8 - 7);
        } else {
            f->lf.cdef_line[0][1] = ptr + uv_stride * 0;
            f->lf.cdef_line[0][2] = ptr + uv_stride * 2;
            f->lf.cdef_line[1][1] = ptr + uv_stride * 4;
            f->lf.cdef_line[1][2] = ptr + uv_stride * 6;
        }

        if (need_cdef_lpf_copy) {
            ptr += llabs(uv_stride) * f->sbh * 8;
            if (y_stride < 0)
                f->lf.cdef_lpf_line[0] = ptr - y_stride * (f->sbh * 4 - 1);
            else
                f->lf.cdef_lpf_line[0] = ptr;
            ptr += llabs(y_stride) * f->sbh * 4;
            if (uv_stride < 0) {
                f->lf.cdef_lpf_line[1] = ptr - uv_stride * (f->sbh * 4 - 1);
                f->lf.cdef_lpf_line[2] = ptr - uv_stride * (f->sbh * 8 - 1);
            } else {
                f->lf.cdef_lpf_line[1] = ptr;
                f->lf.cdef_lpf_line[2] = ptr + uv_stride * f->sbh * 4;
            }
        }

        f->lf.cdef_buf_plane_sz[0] = (int) y_stride * f->sbh * 4;
        f->lf.cdef_buf_plane_sz[1] = (int) uv_stride * f->sbh * 8;
        f->lf.need_cdef_lpf_copy = need_cdef_lpf_copy;
        f->lf.cdef_buf_sbh = f->sbh;
    }

    const int sb128 = f->seq_hdr->sb128;
    const int num_lines = c->n_tc > 1 ? f->sbh * 4 << sb128 : 12;
    y_stride = f->sr_cur.p.stride[0], uv_stride = f->sr_cur.p.stride[1];
    if (y_stride * num_lines != f->lf.lr_buf_plane_sz[0] ||
        uv_stride * num_lines * 2 != f->lf.lr_buf_plane_sz[1])
    {
        dav1d_free_aligned(f->lf.lr_line_buf);
        // lr simd may overread the input, so slightly over-allocate the lpf buffer
        size_t alloc_sz = 128;
        alloc_sz += (size_t)llabs(y_stride) * num_lines;
        alloc_sz += (size_t)llabs(uv_stride) * num_lines * 2;
        uint8_t *ptr = f->lf.lr_line_buf = dav1d_alloc_aligned(alloc_sz, 64);
        if (!ptr) {
            f->lf.lr_buf_plane_sz[0] = f->lf.lr_buf_plane_sz[1] = 0;
            goto error;
        }

        ptr += 64;
        if (y_stride < 0)
            f->lf.lr_lpf_line[0] = ptr - y_stride * (num_lines - 1);
        else
            f->lf.lr_lpf_line[0] = ptr;
        ptr += llabs(y_stride) * num_lines;
        if (uv_stride < 0) {
            f->lf.lr_lpf_line[1] = ptr - uv_stride * (num_lines * 1 - 1);
            f->lf.lr_lpf_line[2] = ptr - uv_stride * (num_lines * 2 - 1);
        } else {
            f->lf.lr_lpf_line[1] = ptr;
            f->lf.lr_lpf_line[2] = ptr + uv_stride * num_lines;
        }

        f->lf.lr_buf_plane_sz[0] = (int) y_stride * num_lines;
        f->lf.lr_buf_plane_sz[1] = (int) uv_stride * num_lines * 2;
    }

    // update allocation for loopfilter masks
    if (num_sb128 != f->lf.mask_sz) {
        freep(&f->lf.mask);
        freep(&f->lf.level);
        f->lf.mask = malloc(sizeof(*f->lf.mask) * num_sb128);
        // over-allocate by 3 bytes since some of the SIMD implementations
        // index this from the level type and can thus over-read by up to 3
        f->lf.level = malloc(sizeof(*f->lf.level) * num_sb128 * 32 * 32 + 3);
        if (!f->lf.mask || !f->lf.level) {
            f->lf.mask_sz = 0;
            goto error;
        }
        if (c->n_fc > 1) {
            freep(&f->frame_thread.b);
            freep(&f->frame_thread.cbi);
            f->frame_thread.b = malloc(sizeof(*f->frame_thread.b) *
                                       num_sb128 * 32 * 32);
            f->frame_thread.cbi = malloc(sizeof(*f->frame_thread.cbi) *
                                         num_sb128 * 32 * 32);
            if (!f->frame_thread.b || !f->frame_thread.cbi) {
                f->lf.mask_sz = 0;
                goto error;
            }
        }
        f->lf.mask_sz = num_sb128;
    }

    f->sr_sb128w = (f->sr_cur.p.p.w + 127) >> 7;
    const int lr_mask_sz = f->sr_sb128w * f->sb128h;
    if (lr_mask_sz != f->lf.lr_mask_sz) {
        freep(&f->lf.lr_mask);
        f->lf.lr_mask = malloc(sizeof(*f->lf.lr_mask) * lr_mask_sz);
        if (!f->lf.lr_mask) {
            f->lf.lr_mask_sz = 0;
            goto error;
        }
        f->lf.lr_mask_sz = lr_mask_sz;
    }
    f->lf.restore_planes =
        ((f->frame_hdr->restoration.type[0] != DAV1D_RESTORATION_NONE) << 0) +
        ((f->frame_hdr->restoration.type[1] != DAV1D_RESTORATION_NONE) << 1) +
        ((f->frame_hdr->restoration.type[2] != DAV1D_RESTORATION_NONE) << 2);
    if (f->frame_hdr->loopfilter.sharpness != f->lf.last_sharpness) {
        dav1d_calc_eih(&f->lf.lim_lut, f->frame_hdr->loopfilter.sharpness);
        f->lf.last_sharpness = f->frame_hdr->loopfilter.sharpness;
    }
    dav1d_calc_lf_values(f->lf.lvl, f->frame_hdr, (int8_t[4]) { 0, 0, 0, 0 });
    memset(f->lf.mask, 0, sizeof(*f->lf.mask) * num_sb128);

    const int ipred_edge_sz = f->sbh * f->sb128w << hbd;
    if (ipred_edge_sz != f->ipred_edge_sz) {
        dav1d_freep_aligned(&f->ipred_edge[0]);
        uint8_t *ptr = f->ipred_edge[0] =
            dav1d_alloc_aligned(ipred_edge_sz * 128 * 3, 64);
        if (!ptr) {
            f->ipred_edge_sz = 0;
            goto error;
        }
        f->ipred_edge[1] = ptr + ipred_edge_sz * 128 * 1;
        f->ipred_edge[2] = ptr + ipred_edge_sz * 128 * 2;
        f->ipred_edge_sz = ipred_edge_sz;
    }

    const int re_sz = f->sb128h * f->frame_hdr->tiling.cols;
    if (re_sz != f->lf.re_sz) {
        freep(&f->lf.tx_lpf_right_edge[0]);
        f->lf.tx_lpf_right_edge[0] = malloc(re_sz * 32 * 2);
        if (!f->lf.tx_lpf_right_edge[0]) {
            f->lf.re_sz = 0;
            goto error;
        }
        f->lf.tx_lpf_right_edge[1] = f->lf.tx_lpf_right_edge[0] + re_sz * 32;
        f->lf.re_sz = re_sz;
    }

    // init ref mvs
    if (IS_INTER_OR_SWITCH(f->frame_hdr) || f->frame_hdr->allow_intrabc) {
        const int ret =
            dav1d_refmvs_init_frame(&f->rf, f->seq_hdr, f->frame_hdr,
                                    f->refpoc, f->mvs, f->refrefpoc, f->ref_mvs,
                                    f->c->n_tc, f->c->n_fc);
        if (ret < 0) goto error;
    }

    // setup dequant tables
    init_quant_tables(f->seq_hdr, f->frame_hdr, f->frame_hdr->quant.yac, f->dq);
    if (f->frame_hdr->quant.qm)
        for (int i = 0; i < N_RECT_TX_SIZES; i++) {
            f->qm[i][0] = dav1d_qm_tbl[f->frame_hdr->quant.qm_y][0][i];
            f->qm[i][1] = dav1d_qm_tbl[f->frame_hdr->quant.qm_u][1][i];
            f->qm[i][2] = dav1d_qm_tbl[f->frame_hdr->quant.qm_v][1][i];
        }
    else
        memset(f->qm, 0, sizeof(f->qm));

    // setup jnt_comp weights
    if (f->frame_hdr->switchable_comp_refs) {
        for (int i = 0; i < 7; i++) {
            const unsigned ref0poc = f->refp[i].p.frame_hdr->frame_offset;

            for (int j = i + 1; j < 7; j++) {
                const unsigned ref1poc = f->refp[j].p.frame_hdr->frame_offset;

                const unsigned d1 =
                    imin(abs(get_poc_diff(f->seq_hdr->order_hint_n_bits, ref0poc,
                                          f->cur.frame_hdr->frame_offset)), 31);
                const unsigned d0 =
                    imin(abs(get_poc_diff(f->seq_hdr->order_hint_n_bits, ref1poc,
                                          f->cur.frame_hdr->frame_offset)), 31);
                const int order = d0 <= d1;

                static const uint8_t quant_dist_weight[3][2] = {
                    { 2, 3 }, { 2, 5 }, { 2, 7 }
                };
                static const uint8_t quant_dist_lookup_table[4][2] = {
                    { 9, 7 }, { 11, 5 }, { 12, 4 }, { 13, 3 }
                };

                int k;
                for (k = 0; k < 3; k++) {
                    const int c0 = quant_dist_weight[k][order];
                    const int c1 = quant_dist_weight[k][!order];
                    const int d0_c0 = d0 * c0;
                    const int d1_c1 = d1 * c1;
                    if ((d0 > d1 && d0_c0 < d1_c1) || (d0 <= d1 && d0_c0 > d1_c1)) break;
                }

                f->jnt_weights[i][j] = quant_dist_lookup_table[k][order];
            }
        }
    }

    /* Init loopfilter pointers. Increasing NULL pointers is technically UB,
     * so just point the chroma pointers in 4:0:0 to the luma plane here to
     * avoid having additional in-loop branches in various places. We never
     * dereference those pointers so it doesn't really matter what they
     * point at, as long as the pointers are valid. */
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400;
    f->lf.mask_ptr = f->lf.mask;
    f->lf.p[0] = f->cur.data[0];
    f->lf.p[1] = f->cur.data[has_chroma ? 1 : 0];
    f->lf.p[2] = f->cur.data[has_chroma ? 2 : 0];
    f->lf.sr_p[0] = f->sr_cur.p.data[0];
    f->lf.sr_p[1] = f->sr_cur.p.data[has_chroma ? 1 : 0];
    f->lf.sr_p[2] = f->sr_cur.p.data[has_chroma ? 2 : 0];

    retval = 0;
error:
    return retval;
}

int dav1d_decode_frame_init_cdf(Dav1dFrameContext *const f) {
    const Dav1dContext *const c = f->c;
    int retval = DAV1D_ERR(EINVAL);

    if (f->frame_hdr->refresh_context)
        dav1d_cdf_thread_copy(f->out_cdf.data.cdf, &f->in_cdf);

    // parse individual tiles per tile group
    int tile_row = 0, tile_col = 0;
    f->task_thread.update_set = 0;
    for (int i = 0; i < f->n_tile_data; i++) {
        const uint8_t *data = f->tile[i].data.data;
        size_t size = f->tile[i].data.sz;

        for (int j = f->tile[i].start; j <= f->tile[i].end; j++) {
            size_t tile_sz;
            if (j == f->tile[i].end) {
                tile_sz = size;
            } else {
                if (f->frame_hdr->tiling.n_bytes > size) goto error;
                tile_sz = 0;
                for (unsigned k = 0; k < f->frame_hdr->tiling.n_bytes; k++)
                    tile_sz |= (unsigned)*data++ << (k * 8);
                tile_sz++;
                size -= f->frame_hdr->tiling.n_bytes;
                if (tile_sz > size) goto error;
            }

            setup_tile(&f->ts[j], f, data, tile_sz, tile_row, tile_col++,
                       c->n_fc > 1 ? f->frame_thread.tile_start_off[j] : 0);

            if (tile_col == f->frame_hdr->tiling.cols) {
                tile_col = 0;
                tile_row++;
            }
            if (j == f->frame_hdr->tiling.update && f->frame_hdr->refresh_context)
                f->task_thread.update_set = 1;
            data += tile_sz;
            size -= tile_sz;
        }
    }

    if (c->n_tc > 1) {
        const int uses_2pass = c->n_fc > 1;
        for (int n = 0; n < f->sb128w * f->frame_hdr->tiling.rows * (1 + uses_2pass); n++)
            reset_context(&f->a[n], IS_KEY_OR_INTRA(f->frame_hdr),
                          uses_2pass ? 1 + (n >= f->sb128w * f->frame_hdr->tiling.rows) : 0);
    }

    retval = 0;
error:
    return retval;
}

int dav1d_decode_frame_main(Dav1dFrameContext *const f) {
    const Dav1dContext *const c = f->c;
    int retval = DAV1D_ERR(EINVAL);

    assert(f->c->n_tc == 1);

    Dav1dTaskContext *const t = &c->tc[f - c->fc];
    t->f = f;
    t->frame_thread.pass = 0;

    for (int n = 0; n < f->sb128w * f->frame_hdr->tiling.rows; n++)
        reset_context(&f->a[n], IS_KEY_OR_INTRA(f->frame_hdr), 0);

    // no threading - we explicitly interleave tile/sbrow decoding
    // and post-filtering, so that the full process runs in-line
    for (int tile_row = 0; tile_row < f->frame_hdr->tiling.rows; tile_row++) {
        const int sbh_end =
            imin(f->frame_hdr->tiling.row_start_sb[tile_row + 1], f->sbh);
        for (int sby = f->frame_hdr->tiling.row_start_sb[tile_row];
             sby < sbh_end; sby++)
        {
            t->by = sby << (4 + f->seq_hdr->sb128);
            const int by_end = (t->by + f->sb_step) >> 1;
            if (f->frame_hdr->use_ref_frame_mvs) {
                dav1d_refmvs_load_tmvs(&f->rf, tile_row,
                                       0, f->bw >> 1, t->by >> 1, by_end);
            }
            for (int tile_col = 0; tile_col < f->frame_hdr->tiling.cols; tile_col++) {
                t->ts = &f->ts[tile_row * f->frame_hdr->tiling.cols + tile_col];
                if (dav1d_decode_tile_sbrow(t)) goto error;
            }
            if (IS_INTER_OR_SWITCH(f->frame_hdr)) {
                dav1d_refmvs_save_tmvs(&t->rt, 0, f->bw >> 1, t->by >> 1, by_end);
            }

            // loopfilter + cdef + restoration
            f->bd_fn.filter_sbrow(f, sby);
        }
    }

    retval = 0;
error:
    return retval;
}

void dav1d_decode_frame_exit(Dav1dFrameContext *const f, const int retval) {
    const Dav1dContext *const c = f->c;

    if (f->sr_cur.p.data[0])
        atomic_init(&f->task_thread.error, 0);

    if (c->n_fc > 1 && retval && f->frame_thread.cf) {
        memset(f->frame_thread.cf, 0,
               (size_t)f->frame_thread.cf_sz * 128 * 128 / 2);
    }
    for (int i = 0; i < 7; i++) {
        if (f->refp[i].p.data[0])
            dav1d_thread_picture_unref(&f->refp[i]);
        dav1d_ref_dec(&f->ref_mvs_ref[i]);
    }

    dav1d_picture_unref_internal(&f->cur);
    dav1d_thread_picture_unref(&f->sr_cur);
    dav1d_cdf_thread_unref(&f->in_cdf);
    if (f->frame_hdr && f->frame_hdr->refresh_context) {
        if (f->out_cdf.progress)
            atomic_store(f->out_cdf.progress, retval == 0 ? 1 : TILE_ERROR);
        dav1d_cdf_thread_unref(&f->out_cdf);
    }
    dav1d_ref_dec(&f->cur_segmap_ref);
    dav1d_ref_dec(&f->prev_segmap_ref);
    dav1d_ref_dec(&f->mvs_ref);
    dav1d_ref_dec(&f->seq_hdr_ref);
    dav1d_ref_dec(&f->frame_hdr_ref);

    for (int i = 0; i < f->n_tile_data; i++)
        dav1d_data_unref_internal(&f->tile[i].data);
    f->task_thread.retval = retval;
}

int dav1d_decode_frame(Dav1dFrameContext *const f) {
    assert(f->c->n_fc == 1);
    // if n_tc > 1 (but n_fc == 1), we could run init/exit in the task
    // threads also. Not sure it makes a measurable difference.
    int res = dav1d_decode_frame_init(f);
    if (!res) res = dav1d_decode_frame_init_cdf(f);
    // wait until all threads have completed
    if (!res) {
        if (f->c->n_tc > 1) {
            pthread_mutex_lock(&f->task_thread.ttd->lock);
            res = dav1d_task_create_tile_sbrow(f, 0, 1);
            if (!res) {
                while (!f->task_thread.done[0] ||
                       f->task_thread.task_counter > 0)
                {
                    pthread_cond_wait(&f->task_thread.cond,
                                      &f->task_thread.ttd->lock);
                }
            }
            pthread_mutex_unlock(&f->task_thread.ttd->lock);
            res = f->task_thread.retval;
        } else {
            res = dav1d_decode_frame_main(f);
            if (!res && f->frame_hdr->refresh_context && f->task_thread.update_set) {
                dav1d_cdf_thread_update(f->frame_hdr, f->out_cdf.data.cdf,
                                        &f->ts[f->frame_hdr->tiling.update].cdf);
            }
        }
    }
    dav1d_decode_frame_exit(f, res);
    f->n_tile_data = 0;
    return res;
}

static int get_upscale_x0(const int in_w, const int out_w, const int step) {
    const int err = out_w * step - (in_w << 14);
    const int x0 = (-((out_w - in_w) << 13) + (out_w >> 1)) / out_w + 128 - (err >> 1);
    return x0 & 0x3fff;
}

int dav1d_submit_frame(Dav1dContext *const c) {
    Dav1dFrameContext *f;
    int res = -1;

    // wait for c->out_delayed[next] and move into c->out if visible
    Dav1dThreadPicture *out_delayed;
    if (c->n_fc > 1) {
        pthread_mutex_lock(&c->task_thread.lock);
        const unsigned next = c->frame_thread.next++;
        if (c->frame_thread.next == c->n_fc)
            c->frame_thread.next = 0;

        f = &c->fc[next];
        while (f->n_tile_data > 0)
            pthread_cond_wait(&f->task_thread.cond,
                              &c->task_thread.lock);
        out_delayed = &c->frame_thread.out_delayed[next];
        if (out_delayed->p.data[0] || atomic_load(&f->task_thread.error)) {
            if (atomic_load(&c->task_thread.first) + 1U < c->n_fc)
                atomic_fetch_add(&c->task_thread.first, 1U);
            else
                atomic_store(&c->task_thread.first, 0);
            if (c->task_thread.cur && c->task_thread.cur < c->n_fc)
                c->task_thread.cur--;
        }
        const int error = f->task_thread.retval;
        if (error) {
            f->task_thread.retval = 0;
            c->cached_error = error;
            dav1d_data_props_copy(&c->cached_error_props, &out_delayed->p.m);
            dav1d_thread_picture_unref(out_delayed);
        } else if (out_delayed->p.data[0]) {
            const unsigned progress = atomic_load_explicit(&out_delayed->progress[1],
                                                           memory_order_relaxed);
            if ((out_delayed->visible || c->output_invisible_frames) &&
                progress != FRAME_ERROR)
            {
                dav1d_thread_picture_ref(&c->out, out_delayed);
                c->event_flags |= dav1d_picture_get_event_flags(out_delayed);
            }
            dav1d_thread_picture_unref(out_delayed);
        }
    } else {
        f = c->fc;
    }

    f->seq_hdr = c->seq_hdr;
    f->seq_hdr_ref = c->seq_hdr_ref;
    dav1d_ref_inc(f->seq_hdr_ref);
    f->frame_hdr = c->frame_hdr;
    f->frame_hdr_ref = c->frame_hdr_ref;
    c->frame_hdr = NULL;
    c->frame_hdr_ref = NULL;
    f->dsp = &c->dsp[f->seq_hdr->hbd];

    const int bpc = 8 + 2 * f->seq_hdr->hbd;

    if (!f->dsp->ipred.intra_pred[DC_PRED]) {
        Dav1dDSPContext *const dsp = &c->dsp[f->seq_hdr->hbd];

        switch (bpc) {
#define assign_bitdepth_case(bd) \
            dav1d_cdef_dsp_init_##bd##bpc(&dsp->cdef); \
            dav1d_intra_pred_dsp_init_##bd##bpc(&dsp->ipred); \
            dav1d_itx_dsp_init_##bd##bpc(&dsp->itx, bpc); \
            dav1d_loop_filter_dsp_init_##bd##bpc(&dsp->lf); \
            dav1d_loop_restoration_dsp_init_##bd##bpc(&dsp->lr, bpc); \
            dav1d_mc_dsp_init_##bd##bpc(&dsp->mc); \
            dav1d_film_grain_dsp_init_##bd##bpc(&dsp->fg); \
            break
#if CONFIG_8BPC
        case 8:
            assign_bitdepth_case(8);
#endif
#if CONFIG_16BPC
        case 10:
        case 12:
            assign_bitdepth_case(16);
#endif
#undef assign_bitdepth_case
        default:
            dav1d_log(c, "Compiled without support for %d-bit decoding\n",
                    8 + 2 * f->seq_hdr->hbd);
            res = DAV1D_ERR(ENOPROTOOPT);
            goto error;
        }
    }

#define assign_bitdepth_case(bd) \
        f->bd_fn.recon_b_inter = dav1d_recon_b_inter_##bd##bpc; \
        f->bd_fn.recon_b_intra = dav1d_recon_b_intra_##bd##bpc; \
        f->bd_fn.filter_sbrow = dav1d_filter_sbrow_##bd##bpc; \
        f->bd_fn.filter_sbrow_deblock_cols = dav1d_filter_sbrow_deblock_cols_##bd##bpc; \
        f->bd_fn.filter_sbrow_deblock_rows = dav1d_filter_sbrow_deblock_rows_##bd##bpc; \
        f->bd_fn.filter_sbrow_cdef = dav1d_filter_sbrow_cdef_##bd##bpc; \
        f->bd_fn.filter_sbrow_resize = dav1d_filter_sbrow_resize_##bd##bpc; \
        f->bd_fn.filter_sbrow_lr = dav1d_filter_sbrow_lr_##bd##bpc; \
        f->bd_fn.backup_ipred_edge = dav1d_backup_ipred_edge_##bd##bpc; \
        f->bd_fn.read_coef_blocks = dav1d_read_coef_blocks_##bd##bpc
    if (!f->seq_hdr->hbd) {
#if CONFIG_8BPC
        assign_bitdepth_case(8);
#endif
    } else {
#if CONFIG_16BPC
        assign_bitdepth_case(16);
#endif
    }
#undef assign_bitdepth_case

    int ref_coded_width[7];
    if (IS_INTER_OR_SWITCH(f->frame_hdr)) {
        if (f->frame_hdr->primary_ref_frame != DAV1D_PRIMARY_REF_NONE) {
            const int pri_ref = f->frame_hdr->refidx[f->frame_hdr->primary_ref_frame];
            if (!c->refs[pri_ref].p.p.data[0]) {
                res = DAV1D_ERR(EINVAL);
                goto error;
            }
        }
        for (int i = 0; i < 7; i++) {
            const int refidx = f->frame_hdr->refidx[i];
            if (!c->refs[refidx].p.p.data[0] ||
                f->frame_hdr->width[0] * 2 < c->refs[refidx].p.p.p.w ||
                f->frame_hdr->height * 2 < c->refs[refidx].p.p.p.h ||
                f->frame_hdr->width[0] > c->refs[refidx].p.p.p.w * 16 ||
                f->frame_hdr->height > c->refs[refidx].p.p.p.h * 16 ||
                f->seq_hdr->layout != c->refs[refidx].p.p.p.layout ||
                bpc != c->refs[refidx].p.p.p.bpc)
            {
                for (int j = 0; j < i; j++)
                    dav1d_thread_picture_unref(&f->refp[j]);
                res = DAV1D_ERR(EINVAL);
                goto error;
            }
            dav1d_thread_picture_ref(&f->refp[i], &c->refs[refidx].p);
            ref_coded_width[i] = c->refs[refidx].p.p.frame_hdr->width[0];
            if (f->frame_hdr->width[0] != c->refs[refidx].p.p.p.w ||
                f->frame_hdr->height != c->refs[refidx].p.p.p.h)
            {
#define scale_fac(ref_sz, this_sz) \
    ((((ref_sz) << 14) + ((this_sz) >> 1)) / (this_sz))
                f->svc[i][0].scale = scale_fac(c->refs[refidx].p.p.p.w,
                                               f->frame_hdr->width[0]);
                f->svc[i][1].scale = scale_fac(c->refs[refidx].p.p.p.h,
                                               f->frame_hdr->height);
                f->svc[i][0].step = (f->svc[i][0].scale + 8) >> 4;
                f->svc[i][1].step = (f->svc[i][1].scale + 8) >> 4;
            } else {
                f->svc[i][0].scale = f->svc[i][1].scale = 0;
            }
            f->gmv_warp_allowed[i] = f->frame_hdr->gmv[i].type > DAV1D_WM_TYPE_TRANSLATION &&
                                     !f->frame_hdr->force_integer_mv &&
                                     !dav1d_get_shear_params(&f->frame_hdr->gmv[i]) &&
                                     !f->svc[i][0].scale;
        }
    }

    // setup entropy
    if (f->frame_hdr->primary_ref_frame == DAV1D_PRIMARY_REF_NONE) {
        dav1d_cdf_thread_init_static(&f->in_cdf, f->frame_hdr->quant.yac);
    } else {
        const int pri_ref = f->frame_hdr->refidx[f->frame_hdr->primary_ref_frame];
        dav1d_cdf_thread_ref(&f->in_cdf, &c->cdf[pri_ref]);
    }
    if (f->frame_hdr->refresh_context) {
        res = dav1d_cdf_thread_alloc(c, &f->out_cdf, c->n_fc > 1);
        if (res < 0) goto error;
    }

    // FIXME qsort so tiles are in order (for frame threading)
    if (f->n_tile_data_alloc < c->n_tile_data) {
        freep(&f->tile);
        assert(c->n_tile_data < INT_MAX / (int)sizeof(*f->tile));
        f->tile = malloc(c->n_tile_data * sizeof(*f->tile));
        if (!f->tile) {
            f->n_tile_data_alloc = f->n_tile_data = 0;
            res = DAV1D_ERR(ENOMEM);
            goto error;
        }
        f->n_tile_data_alloc = c->n_tile_data;
    }
    memcpy(f->tile, c->tile, c->n_tile_data * sizeof(*f->tile));
    memset(c->tile, 0, c->n_tile_data * sizeof(*c->tile));
    f->n_tile_data = c->n_tile_data;
    c->n_tile_data = 0;

    // allocate frame
    res = dav1d_thread_picture_alloc(c, f, bpc);
    if (res < 0) goto error;

    if (f->frame_hdr->width[0] != f->frame_hdr->width[1]) {
        res = dav1d_picture_alloc_copy(c, &f->cur, f->frame_hdr->width[0], &f->sr_cur.p);
        if (res < 0) goto error;
    } else {
        dav1d_picture_ref(&f->cur, &f->sr_cur.p);
    }

    if (f->frame_hdr->width[0] != f->frame_hdr->width[1]) {
        f->resize_step[0] = scale_fac(f->cur.p.w, f->sr_cur.p.p.w);
        const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int in_cw = (f->cur.p.w + ss_hor) >> ss_hor;
        const int out_cw = (f->sr_cur.p.p.w + ss_hor) >> ss_hor;
        f->resize_step[1] = scale_fac(in_cw, out_cw);
#undef scale_fac
        f->resize_start[0] = get_upscale_x0(f->cur.p.w, f->sr_cur.p.p.w, f->resize_step[0]);
        f->resize_start[1] = get_upscale_x0(in_cw, out_cw, f->resize_step[1]);
    }

    // move f->cur into output queue
    if (c->n_fc == 1) {
        if (f->frame_hdr->show_frame || c->output_invisible_frames) {
            dav1d_thread_picture_ref(&c->out, &f->sr_cur);
            c->event_flags |= dav1d_picture_get_event_flags(&f->sr_cur);
        }
    } else {
        dav1d_thread_picture_ref(out_delayed, &f->sr_cur);
    }

    f->w4 = (f->frame_hdr->width[0] + 3) >> 2;
    f->h4 = (f->frame_hdr->height + 3) >> 2;
    f->bw = ((f->frame_hdr->width[0] + 7) >> 3) << 1;
    f->bh = ((f->frame_hdr->height + 7) >> 3) << 1;
    f->sb128w = (f->bw + 31) >> 5;
    f->sb128h = (f->bh + 31) >> 5;
    f->sb_shift = 4 + f->seq_hdr->sb128;
    f->sb_step = 16 << f->seq_hdr->sb128;
    f->sbh = (f->bh + f->sb_step - 1) >> f->sb_shift;
    f->b4_stride = (f->bw + 31) & ~31;
    f->bitdepth_max = (1 << f->cur.p.bpc) - 1;
    atomic_init(&f->task_thread.error, 0);
    const int uses_2pass = c->n_fc > 1;
    const int cols = f->frame_hdr->tiling.cols;
    const int rows = f->frame_hdr->tiling.rows;
    f->task_thread.task_counter = (cols * rows + f->sbh) << uses_2pass;

    // ref_mvs
    if (IS_INTER_OR_SWITCH(f->frame_hdr) || f->frame_hdr->allow_intrabc) {
        f->mvs_ref = dav1d_ref_create_using_pool(c->refmvs_pool,
            sizeof(*f->mvs) * f->sb128h * 16 * (f->b4_stride >> 1));
        if (!f->mvs_ref) {
            res = DAV1D_ERR(ENOMEM);
            goto error;
        }
        f->mvs = f->mvs_ref->data;
        if (!f->frame_hdr->allow_intrabc) {
            for (int i = 0; i < 7; i++)
                f->refpoc[i] = f->refp[i].p.frame_hdr->frame_offset;
        } else {
            memset(f->refpoc, 0, sizeof(f->refpoc));
        }
        if (f->frame_hdr->use_ref_frame_mvs) {
            for (int i = 0; i < 7; i++) {
                const int refidx = f->frame_hdr->refidx[i];
                if (c->refs[refidx].refmvs != NULL &&
                    ref_coded_width[i] == f->cur.p.w &&
                    f->refp[i].p.p.h == f->cur.p.h)
                {
                    f->ref_mvs_ref[i] = c->refs[refidx].refmvs;
                    dav1d_ref_inc(f->ref_mvs_ref[i]);
                    f->ref_mvs[i] = c->refs[refidx].refmvs->data;
                } else {
                    f->ref_mvs[i] = NULL;
                    f->ref_mvs_ref[i] = NULL;
                }
                memcpy(f->refrefpoc[i], c->refs[refidx].refpoc,
                       sizeof(*f->refrefpoc));
            }
        } else {
            memset(f->ref_mvs_ref, 0, sizeof(f->ref_mvs_ref));
        }
    } else {
        f->mvs_ref = NULL;
        memset(f->ref_mvs_ref, 0, sizeof(f->ref_mvs_ref));
    }

    // segmap
    if (f->frame_hdr->segmentation.enabled) {
        // By default, the previous segmentation map is not initialised.
        f->prev_segmap_ref = NULL;
        f->prev_segmap = NULL;

        // We might need a previous frame's segmentation map. This
        // happens if there is either no update or a temporal update.
        if (f->frame_hdr->segmentation.temporal || !f->frame_hdr->segmentation.update_map) {
            const int pri_ref = f->frame_hdr->primary_ref_frame;
            assert(pri_ref != DAV1D_PRIMARY_REF_NONE);
            const int ref_w = ((ref_coded_width[pri_ref] + 7) >> 3) << 1;
            const int ref_h = ((f->refp[pri_ref].p.p.h + 7) >> 3) << 1;
            if (ref_w == f->bw && ref_h == f->bh) {
                f->prev_segmap_ref = c->refs[f->frame_hdr->refidx[pri_ref]].segmap;
                if (f->prev_segmap_ref) {
                    dav1d_ref_inc(f->prev_segmap_ref);
                    f->prev_segmap = f->prev_segmap_ref->data;
                }
            }
        }

        if (f->frame_hdr->segmentation.update_map) {
            // We're updating an existing map, but need somewhere to
            // put the new values. Allocate them here (the data
            // actually gets set elsewhere)
            f->cur_segmap_ref = dav1d_ref_create_using_pool(c->segmap_pool,
                sizeof(*f->cur_segmap) * f->b4_stride * 32 * f->sb128h);
            if (!f->cur_segmap_ref) {
                dav1d_ref_dec(&f->prev_segmap_ref);
                res = DAV1D_ERR(ENOMEM);
                goto error;
            }
            f->cur_segmap = f->cur_segmap_ref->data;
        } else if (f->prev_segmap_ref) {
            // We're not updating an existing map, and we have a valid
            // reference. Use that.
            f->cur_segmap_ref = f->prev_segmap_ref;
            dav1d_ref_inc(f->cur_segmap_ref);
            f->cur_segmap = f->prev_segmap_ref->data;
        } else {
            // We need to make a new map. Allocate one here and zero it out.
            const size_t segmap_size = sizeof(*f->cur_segmap) * f->b4_stride * 32 * f->sb128h;
            f->cur_segmap_ref = dav1d_ref_create_using_pool(c->segmap_pool, segmap_size);
            if (!f->cur_segmap_ref) {
                res = DAV1D_ERR(ENOMEM);
                goto error;
            }
            f->cur_segmap = f->cur_segmap_ref->data;
            memset(f->cur_segmap, 0, segmap_size);
        }
    } else {
        f->cur_segmap = NULL;
        f->cur_segmap_ref = NULL;
        f->prev_segmap_ref = NULL;
    }

    // update references etc.
    const unsigned refresh_frame_flags = f->frame_hdr->refresh_frame_flags;
    for (int i = 0; i < 8; i++) {
        if (refresh_frame_flags & (1 << i)) {
            if (c->refs[i].p.p.data[0])
                dav1d_thread_picture_unref(&c->refs[i].p);
            dav1d_thread_picture_ref(&c->refs[i].p, &f->sr_cur);

            dav1d_cdf_thread_unref(&c->cdf[i]);
            if (f->frame_hdr->refresh_context) {
                dav1d_cdf_thread_ref(&c->cdf[i], &f->out_cdf);
            } else {
                dav1d_cdf_thread_ref(&c->cdf[i], &f->in_cdf);
            }

            dav1d_ref_dec(&c->refs[i].segmap);
            c->refs[i].segmap = f->cur_segmap_ref;
            if (f->cur_segmap_ref)
                dav1d_ref_inc(f->cur_segmap_ref);
            dav1d_ref_dec(&c->refs[i].refmvs);
            if (!f->frame_hdr->allow_intrabc) {
                c->refs[i].refmvs = f->mvs_ref;
                if (f->mvs_ref)
                    dav1d_ref_inc(f->mvs_ref);
            }
            memcpy(c->refs[i].refpoc, f->refpoc, sizeof(f->refpoc));
        }
    }

    if (c->n_fc == 1) {
        if ((res = dav1d_decode_frame(f)) < 0) {
            dav1d_thread_picture_unref(&c->out);
            for (int i = 0; i < 8; i++) {
                if (refresh_frame_flags & (1 << i)) {
                    if (c->refs[i].p.p.data[0])
                        dav1d_thread_picture_unref(&c->refs[i].p);
                    dav1d_cdf_thread_unref(&c->cdf[i]);
                    dav1d_ref_dec(&c->refs[i].segmap);
                    dav1d_ref_dec(&c->refs[i].refmvs);
                }
            }
            goto error;
        }
    } else {
        dav1d_task_frame_init(f);
        pthread_mutex_unlock(&c->task_thread.lock);
    }

    return 0;
error:
    atomic_init(&f->task_thread.error, 1);
    dav1d_cdf_thread_unref(&f->in_cdf);
    if (f->frame_hdr->refresh_context)
        dav1d_cdf_thread_unref(&f->out_cdf);
    for (int i = 0; i < 7; i++) {
        if (f->refp[i].p.data[0])
            dav1d_thread_picture_unref(&f->refp[i]);
        dav1d_ref_dec(&f->ref_mvs_ref[i]);
    }
    if (c->n_fc == 1)
        dav1d_thread_picture_unref(&c->out);
    else
        dav1d_thread_picture_unref(out_delayed);
    dav1d_picture_unref_internal(&f->cur);
    dav1d_thread_picture_unref(&f->sr_cur);
    dav1d_ref_dec(&f->mvs_ref);
    dav1d_ref_dec(&f->seq_hdr_ref);
    dav1d_ref_dec(&f->frame_hdr_ref);
    dav1d_data_props_copy(&c->cached_error_props, &c->in.m);

    for (int i = 0; i < f->n_tile_data; i++)
        dav1d_data_unref_internal(&f->tile[i].data);
    f->n_tile_data = 0;

    if (c->n_fc > 1)
        pthread_mutex_unlock(&c->task_thread.lock);

    return res;
}
