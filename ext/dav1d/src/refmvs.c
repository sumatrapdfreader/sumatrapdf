/*
 * Copyright © 2020, VideoLAN and dav1d authors
 * Copyright © 2020, Two Orioles, LLC
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

#include <limits.h>
#include <stdlib.h>

#include "dav1d/common.h"

#include "common/intops.h"

#include "src/env.h"
#include "src/mem.h"
#include "src/refmvs.h"

static void add_spatial_candidate(refmvs_candidate *const mvstack, int *const cnt,
                                  const int weight, const refmvs_block *const b,
                                  const union refmvs_refpair ref, const mv gmv[2],
                                  int *const have_newmv_match,
                                  int *const have_refmv_match)
{
    if (b->mv.mv[0].n == INVALID_MV) return; // intra block, no intrabc

    if (ref.ref[1] == -1) {
        for (int n = 0; n < 2; n++) {
            if (b->ref.ref[n] == ref.ref[0]) {
                const mv cand_mv = ((b->mf & 1) && gmv[0].n != INVALID_MV) ?
                                   gmv[0] : b->mv.mv[n];

                *have_refmv_match = 1;
                *have_newmv_match |= b->mf >> 1;

                const int last = *cnt;
                for (int m = 0; m < last; m++)
                    if (mvstack[m].mv.mv[0].n == cand_mv.n) {
                        mvstack[m].weight += weight;
                        return;
                    }

                if (last < 8) {
                    mvstack[last].mv.mv[0] = cand_mv;
                    mvstack[last].weight = weight;
                    *cnt = last + 1;
                }
                return;
            }
        }
    } else if (b->ref.pair == ref.pair) {
        const refmvs_mvpair cand_mv = { .mv = {
            [0] = ((b->mf & 1) && gmv[0].n != INVALID_MV) ? gmv[0] : b->mv.mv[0],
            [1] = ((b->mf & 1) && gmv[1].n != INVALID_MV) ? gmv[1] : b->mv.mv[1],
        }};

        *have_refmv_match = 1;
        *have_newmv_match |= b->mf >> 1;

        const int last = *cnt;
        for (int n = 0; n < last; n++)
            if (mvstack[n].mv.n == cand_mv.n) {
                mvstack[n].weight += weight;
                return;
            }

        if (last < 8) {
            mvstack[last].mv = cand_mv;
            mvstack[last].weight = weight;
            *cnt = last + 1;
        }
    }
}

static int scan_row(refmvs_candidate *const mvstack, int *const cnt,
                    const union refmvs_refpair ref, const mv gmv[2],
                    const refmvs_block *b, const int bw4, const int w4,
                    const int max_rows, const int step,
                    int *const have_newmv_match, int *const have_refmv_match)
{
    const refmvs_block *cand_b = b;
    const enum BlockSize first_cand_bs = cand_b->bs;
    const uint8_t *const first_cand_b_dim = dav1d_block_dimensions[first_cand_bs];
    int cand_bw4 = first_cand_b_dim[0];
    int len = imax(step, imin(bw4, cand_bw4));

    if (bw4 <= cand_bw4) {
        // FIXME weight can be higher for odd blocks (bx4 & 1), but then the
        // position of the first block has to be odd already, i.e. not just
        // for row_offset=-3/-5
        // FIXME why can this not be cand_bw4?
        const int weight = bw4 == 1 ? 2 :
                           imax(2, imin(2 * max_rows, first_cand_b_dim[1]));
        add_spatial_candidate(mvstack, cnt, len * weight, cand_b, ref, gmv,
                              have_newmv_match, have_refmv_match);
        return weight >> 1;
    }

    for (int x = 0;;) {
        // FIXME if we overhang above, we could fill a bitmask so we don't have
        // to repeat the add_spatial_candidate() for the next row, but just increase
        // the weight here
        add_spatial_candidate(mvstack, cnt, len * 2, cand_b, ref, gmv,
                              have_newmv_match, have_refmv_match);
        x += len;
        if (x >= w4) return 1;
        cand_b = &b[x];
        cand_bw4 = dav1d_block_dimensions[cand_b->bs][0];
        assert(cand_bw4 < bw4);
        len = imax(step, cand_bw4);
    }
}

static int scan_col(refmvs_candidate *const mvstack, int *const cnt,
                    const union refmvs_refpair ref, const mv gmv[2],
                    /*const*/ refmvs_block *const *b, const int bh4, const int h4,
                    const int bx4, const int max_cols, const int step,
                    int *const have_newmv_match, int *const have_refmv_match)
{
    const refmvs_block *cand_b = &b[0][bx4];
    const enum BlockSize first_cand_bs = cand_b->bs;
    const uint8_t *const first_cand_b_dim = dav1d_block_dimensions[first_cand_bs];
    int cand_bh4 = first_cand_b_dim[1];
    int len = imax(step, imin(bh4, cand_bh4));

    if (bh4 <= cand_bh4) {
        // FIXME weight can be higher for odd blocks (by4 & 1), but then the
        // position of the first block has to be odd already, i.e. not just
        // for col_offset=-3/-5
        // FIXME why can this not be cand_bh4?
        const int weight = bh4 == 1 ? 2 :
                           imax(2, imin(2 * max_cols, first_cand_b_dim[0]));
        add_spatial_candidate(mvstack, cnt, len * weight, cand_b, ref, gmv,
                            have_newmv_match, have_refmv_match);
        return weight >> 1;
    }

    for (int y = 0;;) {
        // FIXME if we overhang above, we could fill a bitmask so we don't have
        // to repeat the add_spatial_candidate() for the next row, but just increase
        // the weight here
        add_spatial_candidate(mvstack, cnt, len * 2, cand_b, ref, gmv,
                              have_newmv_match, have_refmv_match);
        y += len;
        if (y >= h4) return 1;
        cand_b = &b[y][bx4];
        cand_bh4 = dav1d_block_dimensions[cand_b->bs][1];
        assert(cand_bh4 < bh4);
        len = imax(step, cand_bh4);
    }
}

static inline union mv mv_projection(const union mv mv, const int num, const int den) {
    static const uint16_t div_mult[32] = {
           0, 16384, 8192, 5461, 4096, 3276, 2730, 2340,
        2048,  1820, 1638, 1489, 1365, 1260, 1170, 1092,
        1024,   963,  910,  862,  819,  780,  744,  712,
         682,   655,  630,  606,  585,  564,  546,  528
    };
    assert(den > 0 && den < 32);
    assert(num > -32 && num < 32);
    const int frac = num * div_mult[den];
    const int y = mv.y * frac, x = mv.x * frac;
    // Round and clip according to AV1 spec section 7.9.3
    return (union mv) { // 0x3fff == (1 << 14) - 1
        .y = iclip((y + 8192 + (y >> 31)) >> 14, -0x3fff, 0x3fff),
        .x = iclip((x + 8192 + (x >> 31)) >> 14, -0x3fff, 0x3fff)
    };
}

static void add_temporal_candidate(const refmvs_frame *const rf,
                                   refmvs_candidate *const mvstack, int *const cnt,
                                   const refmvs_temporal_block *const rb,
                                   const union refmvs_refpair ref, int *const globalmv_ctx,
                                   const union mv gmv[])
{
    if (rb->mv.n == INVALID_MV) return;

    union mv mv = mv_projection(rb->mv, rf->pocdiff[ref.ref[0] - 1], rb->ref);
    fix_mv_precision(rf->frm_hdr, &mv);

    const int last = *cnt;
    if (ref.ref[1] == -1) {
        if (globalmv_ctx)
            *globalmv_ctx = (abs(mv.x - gmv[0].x) | abs(mv.y - gmv[0].y)) >= 16;

        for (int n = 0; n < last; n++)
            if (mvstack[n].mv.mv[0].n == mv.n) {
                mvstack[n].weight += 2;
                return;
            }
        if (last < 8) {
            mvstack[last].mv.mv[0] = mv;
            mvstack[last].weight = 2;
            *cnt = last + 1;
        }
    } else {
        refmvs_mvpair mvp = { .mv = {
            [0] = mv,
            [1] = mv_projection(rb->mv, rf->pocdiff[ref.ref[1] - 1], rb->ref),
        }};
        fix_mv_precision(rf->frm_hdr, &mvp.mv[1]);

        for (int n = 0; n < last; n++)
            if (mvstack[n].mv.n == mvp.n) {
                mvstack[n].weight += 2;
                return;
            }
        if (last < 8) {
            mvstack[last].mv = mvp;
            mvstack[last].weight = 2;
            *cnt = last + 1;
        }
    }
}

static void add_compound_extended_candidate(refmvs_candidate *const same,
                                            int *const same_count,
                                            const refmvs_block *const cand_b,
                                            const int sign0, const int sign1,
                                            const union refmvs_refpair ref,
                                            const uint8_t *const sign_bias)
{
    refmvs_candidate *const diff = &same[2];
    int *const diff_count = &same_count[2];

    for (int n = 0; n < 2; n++) {
        const int cand_ref = cand_b->ref.ref[n];

        if (cand_ref <= 0) break;

        mv cand_mv = cand_b->mv.mv[n];
        if (cand_ref == ref.ref[0]) {
            if (same_count[0] < 2)
                same[same_count[0]++].mv.mv[0] = cand_mv;
            if (diff_count[1] < 2) {
                if (sign1 ^ sign_bias[cand_ref - 1]) {
                    cand_mv.y = -cand_mv.y;
                    cand_mv.x = -cand_mv.x;
                }
                diff[diff_count[1]++].mv.mv[1] = cand_mv;
            }
        } else if (cand_ref == ref.ref[1]) {
            if (same_count[1] < 2)
                same[same_count[1]++].mv.mv[1] = cand_mv;
            if (diff_count[0] < 2) {
                if (sign0 ^ sign_bias[cand_ref - 1]) {
                    cand_mv.y = -cand_mv.y;
                    cand_mv.x = -cand_mv.x;
                }
                diff[diff_count[0]++].mv.mv[0] = cand_mv;
            }
        } else {
            mv i_cand_mv = (union mv) {
                .x = -cand_mv.x,
                .y = -cand_mv.y
            };

            if (diff_count[0] < 2) {
                diff[diff_count[0]++].mv.mv[0] =
                    sign0 ^ sign_bias[cand_ref - 1] ?
                    i_cand_mv : cand_mv;
            }

            if (diff_count[1] < 2) {
                diff[diff_count[1]++].mv.mv[1] =
                    sign1 ^ sign_bias[cand_ref - 1] ?
                    i_cand_mv : cand_mv;
            }
        }
    }
}

static void add_single_extended_candidate(refmvs_candidate mvstack[8], int *const cnt,
                                          const refmvs_block *const cand_b,
                                          const int sign, const uint8_t *const sign_bias)
{
    for (int n = 0; n < 2; n++) {
        const int cand_ref = cand_b->ref.ref[n];

        if (cand_ref <= 0) break;
        // we need to continue even if cand_ref == ref.ref[0], since
        // the candidate could have been added as a globalmv variant,
        // which changes the value
        // FIXME if scan_{row,col}() returned a mask for the nearest
        // edge, we could skip the appropriate ones here

        mv cand_mv = cand_b->mv.mv[n];
        if (sign ^ sign_bias[cand_ref - 1]) {
            cand_mv.y = -cand_mv.y;
            cand_mv.x = -cand_mv.x;
        }

        int m;
        const int last = *cnt;
        for (m = 0; m < last; m++)
            if (cand_mv.n == mvstack[m].mv.mv[0].n)
                break;
        if (m == last) {
            mvstack[m].mv.mv[0] = cand_mv;
            mvstack[m].weight = 2; // "minimal"
            *cnt = last + 1;
        }
    }
}

/*
 * refmvs_frame allocates memory for one sbrow (32 blocks high, whole frame
 * wide) of 4x4-resolution refmvs_block entries for spatial MV referencing.
 * mvrefs_tile[] keeps a list of 35 (32 + 3 above) pointers into this memory,
 * and each sbrow, the bottom entries (y=27/29/31) are exchanged with the top
 * (-5/-3/-1) pointers by calling dav1d_refmvs_tile_sbrow_init() at the start
 * of each tile/sbrow.
 *
 * For temporal MV referencing, we call dav1d_refmvs_save_tmvs() at the end of
 * each tile/sbrow (when tile column threading is enabled), or at the start of
 * each interleaved sbrow (i.e. once for all tile columns together, when tile
 * column threading is disabled). This will copy the 4x4-resolution spatial MVs
 * into 8x8-resolution refmvs_temporal_block structures. Then, for subsequent
 * frames, at the start of each tile/sbrow (when tile column threading is
 * enabled) or at the start of each interleaved sbrow (when tile column
 * threading is disabled), we call load_tmvs(), which will project the MVs to
 * their respective position in the current frame.
 */

void dav1d_refmvs_find(const refmvs_tile *const rt,
                       refmvs_candidate mvstack[8], int *const cnt,
                       int *const ctx,
                       const union refmvs_refpair ref, const enum BlockSize bs,
                       const enum EdgeFlags edge_flags,
                       const int by4, const int bx4)
{
    const refmvs_frame *const rf = rt->rf;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], w4 = imin(imin(bw4, 16), rt->tile_col.end - bx4);
    const int bh4 = b_dim[1], h4 = imin(imin(bh4, 16), rt->tile_row.end - by4);
    mv gmv[2], tgmv[2];

    *cnt = 0;
    assert(ref.ref[0] >=  0 && ref.ref[0] <= 8 &&
           ref.ref[1] >= -1 && ref.ref[1] <= 8);
    if (ref.ref[0] > 0) {
        tgmv[0] = get_gmv_2d(&rf->frm_hdr->gmv[ref.ref[0] - 1],
                             bx4, by4, bw4, bh4, rf->frm_hdr);
        gmv[0] = rf->frm_hdr->gmv[ref.ref[0] - 1].type > DAV1D_WM_TYPE_TRANSLATION ?
                 tgmv[0] : (mv) { .n = INVALID_MV };
    } else {
        tgmv[0] = (mv) { .n = 0 };
        gmv[0] = (mv) { .n = INVALID_MV };
    }
    if (ref.ref[1] > 0) {
        tgmv[1] = get_gmv_2d(&rf->frm_hdr->gmv[ref.ref[1] - 1],
                             bx4, by4, bw4, bh4, rf->frm_hdr);
        gmv[1] = rf->frm_hdr->gmv[ref.ref[1] - 1].type > DAV1D_WM_TYPE_TRANSLATION ?
                 tgmv[1] : (mv) { .n = INVALID_MV };
    }

    // top
    int have_newmv = 0, have_col_mvs = 0, have_row_mvs = 0;
    unsigned max_rows = 0, n_rows = ~0;
    const refmvs_block *b_top;
    if (by4 > rt->tile_row.start) {
        max_rows = imin((by4 - rt->tile_row.start + 1) >> 1, 2 + (bh4 > 1));
        b_top = &rt->r[(by4 & 31) - 1 + 5][bx4];
        n_rows = scan_row(mvstack, cnt, ref, gmv, b_top,
                          bw4, w4, max_rows, bw4 >= 16 ? 4 : 1,
                          &have_newmv, &have_row_mvs);
    }

    // left
    unsigned max_cols = 0, n_cols = ~0U;
    refmvs_block *const *b_left;
    if (bx4 > rt->tile_col.start) {
        max_cols = imin((bx4 - rt->tile_col.start + 1) >> 1, 2 + (bw4 > 1));
        b_left = &rt->r[(by4 & 31) + 5];
        n_cols = scan_col(mvstack, cnt, ref, gmv, b_left,
                          bh4, h4, bx4 - 1, max_cols, bh4 >= 16 ? 4 : 1,
                          &have_newmv, &have_col_mvs);
    }

    // top/right
    if (n_rows != ~0U && edge_flags & EDGE_I444_TOP_HAS_RIGHT &&
        imax(bw4, bh4) <= 16 && bw4 + bx4 < rt->tile_col.end)
    {
        add_spatial_candidate(mvstack, cnt, 4, &b_top[bw4], ref, gmv,
                              &have_newmv, &have_row_mvs);
    }

    const int nearest_match = have_col_mvs + have_row_mvs;
    const int nearest_cnt = *cnt;
    for (int n = 0; n < nearest_cnt; n++)
        mvstack[n].weight += 640;

    // temporal
    int globalmv_ctx = rf->frm_hdr->use_ref_frame_mvs;
    if (rf->use_ref_frame_mvs) {
        const ptrdiff_t stride = rf->rp_stride;
        const int by8 = by4 >> 1, bx8 = bx4 >> 1;
        const refmvs_temporal_block *const rbi = &rt->rp_proj[(by8 & 15) * stride + bx8];
        const refmvs_temporal_block *rb = rbi;
        const int step_h = bw4 >= 16 ? 2 : 1, step_v = bh4 >= 16 ? 2 : 1;
        const int w8 = imin((w4 + 1) >> 1, 8), h8 = imin((h4 + 1) >> 1, 8);
        for (int y = 0; y < h8; y += step_v) {
            for (int x = 0; x < w8; x+= step_h) {
                add_temporal_candidate(rf, mvstack, cnt, &rb[x], ref,
                                       !(x | y) ? &globalmv_ctx : NULL, tgmv);
            }
            rb += stride * step_v;
        }
        if (imin(bw4, bh4) >= 2 && imax(bw4, bh4) < 16) {
            const int bh8 = bh4 >> 1, bw8 = bw4 >> 1;
            rb = &rbi[bh8 * stride];
            const int has_bottom = by8 + bh8 < imin(rt->tile_row.end >> 1,
                                                    (by8 & ~7) + 8);
            if (has_bottom && bx8 - 1 >= imax(rt->tile_col.start >> 1, bx8 & ~7)) {
                add_temporal_candidate(rf, mvstack, cnt, &rb[-1], ref,
                                       NULL, NULL);
            }
            if (bx8 + bw8 < imin(rt->tile_col.end >> 1, (bx8 & ~7) + 8)) {
                if (has_bottom) {
                    add_temporal_candidate(rf, mvstack, cnt, &rb[bw8], ref,
                                           NULL, NULL);
                }
                if (by8 + bh8 - 1 < imin(rt->tile_row.end >> 1, (by8 & ~7) + 8)) {
                    add_temporal_candidate(rf, mvstack, cnt, &rb[bw8 - stride],
                                           ref, NULL, NULL);
                }
            }
        }
    }
    assert(*cnt <= 8);

    // top/left (which, confusingly, is part of "secondary" references)
    int have_dummy_newmv_match;
    if ((n_rows | n_cols) != ~0U) {
        add_spatial_candidate(mvstack, cnt, 4, &b_top[-1], ref, gmv,
                              &have_dummy_newmv_match, &have_row_mvs);
    }

    // "secondary" (non-direct neighbour) top & left edges
    // what is different about secondary is that everything is now in 8x8 resolution
    for (int n = 2; n <= 3; n++) {
        if ((unsigned) n > n_rows && (unsigned) n <= max_rows) {
            n_rows += scan_row(mvstack, cnt, ref, gmv,
                               &rt->r[(((by4 & 31) - 2 * n + 1) | 1) + 5][bx4 | 1],
                               bw4, w4, 1 + max_rows - n, bw4 >= 16 ? 4 : 2,
                               &have_dummy_newmv_match, &have_row_mvs);
        }

        if ((unsigned) n > n_cols && (unsigned) n <= max_cols) {
            n_cols += scan_col(mvstack, cnt, ref, gmv, &rt->r[((by4 & 31) | 1) + 5],
                               bh4, h4, (bx4 - n * 2 + 1) | 1,
                               1 + max_cols - n, bh4 >= 16 ? 4 : 2,
                               &have_dummy_newmv_match, &have_col_mvs);
        }
    }
    assert(*cnt <= 8);

    const int ref_match_count = have_col_mvs + have_row_mvs;

    // context build-up
    int refmv_ctx, newmv_ctx;
    switch (nearest_match) {
    case 0:
        refmv_ctx = imin(2, ref_match_count);
        newmv_ctx = ref_match_count > 0;
        break;
    case 1:
        refmv_ctx = imin(ref_match_count * 3, 4);
        newmv_ctx = 3 - have_newmv;
        break;
    case 2:
        refmv_ctx = 5;
        newmv_ctx = 5 - have_newmv;
        break;
    }

    // sorting (nearest, then "secondary")
    int len = nearest_cnt;
    while (len) {
        int last = 0;
        for (int n = 1; n < len; n++) {
            if (mvstack[n - 1].weight < mvstack[n].weight) {
#define EXCHANGE(a, b) do { refmvs_candidate tmp = a; a = b; b = tmp; } while (0)
                EXCHANGE(mvstack[n - 1], mvstack[n]);
                last = n;
            }
        }
        len = last;
    }
    len = *cnt;
    while (len > nearest_cnt) {
        int last = nearest_cnt;
        for (int n = nearest_cnt + 1; n < len; n++) {
            if (mvstack[n - 1].weight < mvstack[n].weight) {
                EXCHANGE(mvstack[n - 1], mvstack[n]);
#undef EXCHANGE
                last = n;
            }
        }
        len = last;
    }

    if (ref.ref[1] > 0) {
        if (*cnt < 2) {
            const int sign0 = rf->sign_bias[ref.ref[0] - 1];
            const int sign1 = rf->sign_bias[ref.ref[1] - 1];
            const int sz4 = imin(w4, h4);
            refmvs_candidate *const same = &mvstack[*cnt];
            int same_count[4] = { 0 };

            // non-self references in top
            if (n_rows != ~0U) for (int x = 0; x < sz4;) {
                const refmvs_block *const cand_b = &b_top[x];
                add_compound_extended_candidate(same, same_count, cand_b,
                                                sign0, sign1, ref, rf->sign_bias);
                x += dav1d_block_dimensions[cand_b->bs][0];
            }

            // non-self references in left
            if (n_cols != ~0U) for (int y = 0; y < sz4;) {
                const refmvs_block *const cand_b = &b_left[y][bx4 - 1];
                add_compound_extended_candidate(same, same_count, cand_b,
                                                sign0, sign1, ref, rf->sign_bias);
                y += dav1d_block_dimensions[cand_b->bs][1];
            }

            refmvs_candidate *const diff = &same[2];
            const int *const diff_count = &same_count[2];

            // merge together
            for (int n = 0; n < 2; n++) {
                int m = same_count[n];

                if (m >= 2) continue;

                const int l = diff_count[n];
                if (l) {
                    same[m].mv.mv[n] = diff[0].mv.mv[n];
                    if (++m == 2) continue;
                    if (l == 2) {
                        same[1].mv.mv[n] = diff[1].mv.mv[n];
                        continue;
                    }
                }
                do {
                    same[m].mv.mv[n] = tgmv[n];
                } while (++m < 2);
            }

            // if the first extended was the same as the non-extended one,
            // then replace it with the second extended one
            int n = *cnt;
            if (n == 1 && mvstack[0].mv.n == same[0].mv.n)
                mvstack[1].mv = mvstack[2].mv;
            do {
                mvstack[n].weight = 2;
            } while (++n < 2);
            *cnt = 2;
        }

        // clamping
        const int left = -(bx4 + bw4 + 4) * 4 * 8;
        const int right = (rf->iw4 - bx4 + 4) * 4 * 8;
        const int top = -(by4 + bh4 + 4) * 4 * 8;
        const int bottom = (rf->ih4 - by4 + 4) * 4 * 8;

        const int n_refmvs = *cnt;
        int n = 0;
        do {
            mvstack[n].mv.mv[0].x = iclip(mvstack[n].mv.mv[0].x, left, right);
            mvstack[n].mv.mv[0].y = iclip(mvstack[n].mv.mv[0].y, top, bottom);
            mvstack[n].mv.mv[1].x = iclip(mvstack[n].mv.mv[1].x, left, right);
            mvstack[n].mv.mv[1].y = iclip(mvstack[n].mv.mv[1].y, top, bottom);
        } while (++n < n_refmvs);

        switch (refmv_ctx >> 1) {
        case 0:
            *ctx = imin(newmv_ctx, 1);
            break;
        case 1:
            *ctx = 1 + imin(newmv_ctx, 3);
            break;
        case 2:
            *ctx = iclip(3 + newmv_ctx, 4, 7);
            break;
        }

        return;
    } else if (*cnt < 2 && ref.ref[0] > 0) {
        const int sign = rf->sign_bias[ref.ref[0] - 1];
        const int sz4 = imin(w4, h4);

        // non-self references in top
        if (n_rows != ~0U) for (int x = 0; x < sz4 && *cnt < 2;) {
            const refmvs_block *const cand_b = &b_top[x];
            add_single_extended_candidate(mvstack, cnt, cand_b, sign, rf->sign_bias);
            x += dav1d_block_dimensions[cand_b->bs][0];
        }

        // non-self references in left
        if (n_cols != ~0U) for (int y = 0; y < sz4 && *cnt < 2;) {
            const refmvs_block *const cand_b = &b_left[y][bx4 - 1];
            add_single_extended_candidate(mvstack, cnt, cand_b, sign, rf->sign_bias);
            y += dav1d_block_dimensions[cand_b->bs][1];
        }
    }
    assert(*cnt <= 8);

    // clamping
    int n_refmvs = *cnt;
    if (n_refmvs) {
        const int left = -(bx4 + bw4 + 4) * 4 * 8;
        const int right = (rf->iw4 - bx4 + 4) * 4 * 8;
        const int top = -(by4 + bh4 + 4) * 4 * 8;
        const int bottom = (rf->ih4 - by4 + 4) * 4 * 8;

        int n = 0;
        do {
            mvstack[n].mv.mv[0].x = iclip(mvstack[n].mv.mv[0].x, left, right);
            mvstack[n].mv.mv[0].y = iclip(mvstack[n].mv.mv[0].y, top, bottom);
        } while (++n < n_refmvs);
    }

    for (int n = *cnt; n < 2; n++)
        mvstack[n].mv.mv[0] = tgmv[0];

    *ctx = (refmv_ctx << 4) | (globalmv_ctx << 3) | newmv_ctx;
}

void dav1d_refmvs_tile_sbrow_init(refmvs_tile *const rt, const refmvs_frame *const rf,
                                  const int tile_col_start4, const int tile_col_end4,
                                  const int tile_row_start4, const int tile_row_end4,
                                  const int sby, int tile_row_idx, const int pass)
{
    if (rf->n_tile_threads == 1) tile_row_idx = 0;
    rt->rp_proj = &rf->rp_proj[16 * rf->rp_stride * tile_row_idx];
    const int uses_2pass = rf->n_tile_threads > 1 && rf->n_frame_threads > 1;
    const ptrdiff_t pass_off = (uses_2pass && pass == 2) ?
        35 * rf->r_stride * rf->n_tile_rows : 0;
    refmvs_block *r = &rf->r[35 * rf->r_stride * tile_row_idx + pass_off];
    const int sbsz = rf->sbsz;
    const int off = (sbsz * sby) & 16;
    for (int i = 0; i < sbsz; i++, r += rf->r_stride)
        rt->r[off + 5 + i] = r;
    rt->r[off + 0] = r;
    r += rf->r_stride;
    rt->r[off + 1] = NULL;
    rt->r[off + 2] = r;
    r += rf->r_stride;
    rt->r[off + 3] = NULL;
    rt->r[off + 4] = r;
    if (sby & 1) {
#define EXCHANGE(a, b) do { void *const tmp = a; a = b; b = tmp; } while (0)
        EXCHANGE(rt->r[off + 0], rt->r[off + sbsz + 0]);
        EXCHANGE(rt->r[off + 2], rt->r[off + sbsz + 2]);
        EXCHANGE(rt->r[off + 4], rt->r[off + sbsz + 4]);
#undef EXCHANGE
    }

    rt->rf = rf;
    rt->tile_row.start = tile_row_start4;
    rt->tile_row.end = imin(tile_row_end4, rf->ih4);
    rt->tile_col.start = tile_col_start4;
    rt->tile_col.end = imin(tile_col_end4, rf->iw4);
}

void dav1d_refmvs_load_tmvs(const refmvs_frame *const rf, int tile_row_idx,
                            const int col_start8, const int col_end8,
                            const int row_start8, int row_end8)
{
    if (rf->n_tile_threads == 1) tile_row_idx = 0;
    assert(row_start8 >= 0);
    assert((unsigned) (row_end8 - row_start8) <= 16U);
    row_end8 = imin(row_end8, rf->ih8);
    const int col_start8i = imax(col_start8 - 8, 0);
    const int col_end8i = imin(col_end8 + 8, rf->iw8);

    const ptrdiff_t stride = rf->rp_stride;
    refmvs_temporal_block *rp_proj =
        &rf->rp_proj[16 * stride * tile_row_idx + (row_start8 & 15) * stride];
    for (int y = row_start8; y < row_end8; y++) {
        for (int x = col_start8; x < col_end8; x++)
            rp_proj[x].mv.n = INVALID_MV;
        rp_proj += stride;
    }

    rp_proj = &rf->rp_proj[16 * stride * tile_row_idx];
    for (int n = 0; n < rf->n_mfmvs; n++) {
        const int ref2cur = rf->mfmv_ref2cur[n];
        if (ref2cur == INT_MIN) continue;

        const int ref = rf->mfmv_ref[n];
        const int ref_sign = ref - 4;
        const refmvs_temporal_block *r = &rf->rp_ref[ref][row_start8 * stride];
        for (int y = row_start8; y < row_end8; y++) {
            const int y_sb_align = y & ~7;
            const int y_proj_start = imax(y_sb_align, row_start8);
            const int y_proj_end = imin(y_sb_align + 8, row_end8);
            for (int x = col_start8i; x < col_end8i; x++) {
                const refmvs_temporal_block *rb = &r[x];
                const int b_ref = rb->ref;
                if (!b_ref) continue;
                const int ref2ref = rf->mfmv_ref2ref[n][b_ref - 1];
                if (!ref2ref) continue;
                const mv b_mv = rb->mv;
                const mv offset = mv_projection(b_mv, ref2cur, ref2ref);
                int pos_x = x + apply_sign(abs(offset.x) >> 6,
                                           offset.x ^ ref_sign);
                const int pos_y = y + apply_sign(abs(offset.y) >> 6,
                                                 offset.y ^ ref_sign);
                if (pos_y >= y_proj_start && pos_y < y_proj_end) {
                    const ptrdiff_t pos = (pos_y & 15) * stride;
                    for (;;) {
                        const int x_sb_align = x & ~7;
                        if (pos_x >= imax(x_sb_align - 8, col_start8) &&
                            pos_x < imin(x_sb_align + 16, col_end8))
                        {
                            rp_proj[pos + pos_x].mv = rb->mv;
                            rp_proj[pos + pos_x].ref = ref2ref;
                        }
                        if (++x >= col_end8i) break;
                        rb++;
                        if (rb->ref != b_ref || rb->mv.n != b_mv.n) break;
                        pos_x++;
                    }
                } else {
                    for (;;) {
                        if (++x >= col_end8i) break;
                        rb++;
                        if (rb->ref != b_ref || rb->mv.n != b_mv.n) break;
                    }
                }
                x--;
            }
            r += stride;
        }
    }
}

void dav1d_refmvs_save_tmvs(const refmvs_tile *const rt,
                            const int col_start8, int col_end8,
                            const int row_start8, int row_end8)
{
    const refmvs_frame *const rf = rt->rf;

    assert(row_start8 >= 0);
    assert((unsigned) (row_end8 - row_start8) <= 16U);
    row_end8 = imin(row_end8, rf->ih8);
    col_end8 = imin(col_end8, rf->iw8);

    const ptrdiff_t stride = rf->rp_stride;
    const uint8_t *const ref_sign = rf->mfmv_sign;
    refmvs_temporal_block *rp = &rf->rp[row_start8 * stride];
    for (int y = row_start8; y < row_end8; y++) {
        const refmvs_block *const b = rt->r[6 + (y & 15) * 2];

        for (int x = col_start8; x < col_end8;) {
            const refmvs_block *const cand_b = &b[x * 2 + 1];
            const int bw8 = (dav1d_block_dimensions[cand_b->bs][0] + 1) >> 1;

            if (cand_b->ref.ref[1] > 0 && ref_sign[cand_b->ref.ref[1] - 1] &&
                (abs(cand_b->mv.mv[1].y) | abs(cand_b->mv.mv[1].x)) < 4096)
            {
                for (int n = 0; n < bw8; n++, x++)
                    rp[x] = (refmvs_temporal_block) { .mv = cand_b->mv.mv[1],
                                                      .ref = cand_b->ref.ref[1] };
            } else if (cand_b->ref.ref[0] > 0 && ref_sign[cand_b->ref.ref[0] - 1] &&
                       (abs(cand_b->mv.mv[0].y) | abs(cand_b->mv.mv[0].x)) < 4096)
            {
                for (int n = 0; n < bw8; n++, x++)
                    rp[x] = (refmvs_temporal_block) { .mv = cand_b->mv.mv[0],
                                                      .ref = cand_b->ref.ref[0] };
            } else {
                for (int n = 0; n < bw8; n++, x++)
                    rp[x].ref = 0; // "invalid"
            }
        }
        rp += stride;
    }
}

int dav1d_refmvs_init_frame(refmvs_frame *const rf,
                            const Dav1dSequenceHeader *const seq_hdr,
                            const Dav1dFrameHeader *const frm_hdr,
                            const unsigned ref_poc[7],
                            refmvs_temporal_block *const rp,
                            const unsigned ref_ref_poc[7][7],
                            /*const*/ refmvs_temporal_block *const rp_ref[7],
                            const int n_tile_threads, const int n_frame_threads)
{
    rf->sbsz = 16 << seq_hdr->sb128;
    rf->frm_hdr = frm_hdr;
    rf->iw8 = (frm_hdr->width[0] + 7) >> 3;
    rf->ih8 = (frm_hdr->height + 7) >> 3;
    rf->iw4 = rf->iw8 << 1;
    rf->ih4 = rf->ih8 << 1;

    const ptrdiff_t r_stride = ((frm_hdr->width[0] + 127) & ~127) >> 2;
    const int n_tile_rows = n_tile_threads > 1 ? frm_hdr->tiling.rows : 1;
    if (r_stride != rf->r_stride || n_tile_rows != rf->n_tile_rows) {
        if (rf->r) dav1d_freep_aligned(&rf->r);
        const int uses_2pass = n_tile_threads > 1 && n_frame_threads > 1;
        rf->r = dav1d_alloc_aligned(sizeof(*rf->r) * 35 * r_stride * n_tile_rows * (1 + uses_2pass), 64);
        if (!rf->r) return DAV1D_ERR(ENOMEM);
        rf->r_stride = r_stride;
    }

    const ptrdiff_t rp_stride = r_stride >> 1;
    if (rp_stride != rf->rp_stride || n_tile_rows != rf->n_tile_rows) {
        if (rf->rp_proj) dav1d_freep_aligned(&rf->rp_proj);
        rf->rp_proj = dav1d_alloc_aligned(sizeof(*rf->rp_proj) * 16 * rp_stride * n_tile_rows, 64);
        if (!rf->rp_proj) return DAV1D_ERR(ENOMEM);
        rf->rp_stride = rp_stride;
    }
    rf->n_tile_rows = n_tile_rows;
    rf->n_tile_threads = n_tile_threads;
    rf->n_frame_threads = n_frame_threads;
    rf->rp = rp;
    rf->rp_ref = rp_ref;
    const unsigned poc = frm_hdr->frame_offset;
    for (int i = 0; i < 7; i++) {
        const int poc_diff = get_poc_diff(seq_hdr->order_hint_n_bits,
                                          ref_poc[i], poc);
        rf->sign_bias[i] = poc_diff > 0;
        rf->mfmv_sign[i] = poc_diff < 0;
        rf->pocdiff[i] = iclip(get_poc_diff(seq_hdr->order_hint_n_bits,
                                            poc, ref_poc[i]), -31, 31);
    }

    // temporal MV setup
    rf->n_mfmvs = 0;
    if (frm_hdr->use_ref_frame_mvs && seq_hdr->order_hint_n_bits) {
        int total = 2;
        if (rp_ref[0] && ref_ref_poc[0][6] != ref_poc[3] /* alt-of-last != gold */) {
            rf->mfmv_ref[rf->n_mfmvs++] = 0; // last
            total = 3;
        }
        if (rp_ref[4] && get_poc_diff(seq_hdr->order_hint_n_bits, ref_poc[4],
                                      frm_hdr->frame_offset) > 0)
        {
            rf->mfmv_ref[rf->n_mfmvs++] = 4; // bwd
        }
        if (rp_ref[5] && get_poc_diff(seq_hdr->order_hint_n_bits, ref_poc[5],
                                      frm_hdr->frame_offset) > 0)
        {
            rf->mfmv_ref[rf->n_mfmvs++] = 5; // altref2
        }
        if (rf->n_mfmvs < total && rp_ref[6] &&
            get_poc_diff(seq_hdr->order_hint_n_bits, ref_poc[6],
                         frm_hdr->frame_offset) > 0)
        {
            rf->mfmv_ref[rf->n_mfmvs++] = 6; // altref
        }
        if (rf->n_mfmvs < total && rp_ref[1])
            rf->mfmv_ref[rf->n_mfmvs++] = 1; // last2

        for (int n = 0; n < rf->n_mfmvs; n++) {
            const unsigned rpoc = ref_poc[rf->mfmv_ref[n]];
            const int diff1 = get_poc_diff(seq_hdr->order_hint_n_bits,
                                           rpoc, frm_hdr->frame_offset);
            if (abs(diff1) > 31) {
                rf->mfmv_ref2cur[n] = INT_MIN;
            } else {
                rf->mfmv_ref2cur[n] = rf->mfmv_ref[n] < 4 ? -diff1 : diff1;
                for (int m = 0; m < 7; m++) {
                    const unsigned rrpoc = ref_ref_poc[rf->mfmv_ref[n]][m];
                    const int diff2 = get_poc_diff(seq_hdr->order_hint_n_bits,
                                                   rpoc, rrpoc);
                    // unsigned comparison also catches the < 0 case
                    rf->mfmv_ref2ref[n][m] = (unsigned) diff2 > 31U ? 0 : diff2;
                }
            }
        }
    }
    rf->use_ref_frame_mvs = rf->n_mfmvs > 0;

    return 0;
}

void dav1d_refmvs_init(refmvs_frame *const rf) {
    rf->r = NULL;
    rf->r_stride = 0;
    rf->rp_proj = NULL;
    rf->rp_stride = 0;
}

void dav1d_refmvs_clear(refmvs_frame *const rf) {
    if (rf->r) dav1d_freep_aligned(&rf->r);
    if (rf->rp_proj) dav1d_freep_aligned(&rf->rp_proj);
}

static void splat_mv_c(refmvs_block **rr, const refmvs_block *const rmv,
                       const int bx4, const int bw4, int bh4)
{
    do {
        refmvs_block *const r = *rr++ + bx4;
        for (int x = 0; x < bw4; x++)
            r[x] = *rmv;
    } while (--bh4);
}

COLD void dav1d_refmvs_dsp_init(Dav1dRefmvsDSPContext *const c)
{
    c->splat_mv = splat_mv_c;

#if HAVE_ASM
#if ARCH_AARCH64 || ARCH_ARM
    dav1d_refmvs_dsp_init_arm(c);
#elif ARCH_X86
    dav1d_refmvs_dsp_init_x86(c);
#endif
#endif
}
