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

#include <string.h>
#include <stdio.h>

#include "common/attributes.h"
#include "common/bitdepth.h"
#include "common/dump.h"
#include "common/frame.h"
#include "common/intops.h"

#include "src/cdef_apply.h"
#include "src/ctx.h"
#include "src/ipred_prepare.h"
#include "src/lf_apply.h"
#include "src/lr_apply.h"
#include "src/recon.h"
#include "src/scan.h"
#include "src/tables.h"
#include "src/wedge.h"

static inline unsigned read_golomb(MsacContext *const msac) {
    int len = 0;
    unsigned val = 1;

    while (!dav1d_msac_decode_bool_equi(msac) && len < 32) len++;
    while (len--) val = (val << 1) + dav1d_msac_decode_bool_equi(msac);

    return val - 1;
}

static inline unsigned get_skip_ctx(const TxfmInfo *const t_dim,
                                    const enum BlockSize bs,
                                    const uint8_t *const a,
                                    const uint8_t *const l,
                                    const int chroma,
                                    const enum Dav1dPixelLayout layout)
{
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];

    if (chroma) {
        const int ss_ver = layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = layout != DAV1D_PIXEL_LAYOUT_I444;
        const int not_one_blk = b_dim[2] - (!!b_dim[2] && ss_hor) > t_dim->lw ||
                                b_dim[3] - (!!b_dim[3] && ss_ver) > t_dim->lh;
        unsigned ca, cl;

#define MERGE_CTX(dir, type, no_val) \
        c##dir = *(const type *) dir != no_val; \
        break

        switch (t_dim->lw) {
        /* For some reason the MSVC CRT _wassert() function is not flagged as
         * __declspec(noreturn), so when using those headers the compiler will
         * expect execution to continue after an assertion has been triggered
         * and will therefore complain about the use of uninitialized variables
         * when compiled in debug mode if we put the default case at the end. */
        default: assert(0); /* fall-through */
        case TX_4X4:   MERGE_CTX(a, uint8_t,  0x40);
        case TX_8X8:   MERGE_CTX(a, uint16_t, 0x4040);
        case TX_16X16: MERGE_CTX(a, uint32_t, 0x40404040U);
        case TX_32X32: MERGE_CTX(a, uint64_t, 0x4040404040404040ULL);
        }
        switch (t_dim->lh) {
        default: assert(0); /* fall-through */
        case TX_4X4:   MERGE_CTX(l, uint8_t,  0x40);
        case TX_8X8:   MERGE_CTX(l, uint16_t, 0x4040);
        case TX_16X16: MERGE_CTX(l, uint32_t, 0x40404040U);
        case TX_32X32: MERGE_CTX(l, uint64_t, 0x4040404040404040ULL);
        }
#undef MERGE_CTX

        return 7 + not_one_blk * 3 + ca + cl;
    } else if (b_dim[2] == t_dim->lw && b_dim[3] == t_dim->lh) {
        return 0;
    } else {
        unsigned la, ll;

#define MERGE_CTX(dir, type, tx) \
        if (tx == TX_64X64) { \
            uint64_t tmp = *(const uint64_t *) dir; \
            tmp |= *(const uint64_t *) &dir[8]; \
            l##dir = (unsigned) (tmp >> 32) | (unsigned) tmp; \
        } else \
            l##dir = *(const type *) dir; \
        if (tx == TX_32X32) l##dir |= *(const type *) &dir[sizeof(type)]; \
        if (tx >= TX_16X16) l##dir |= l##dir >> 16; \
        if (tx >= TX_8X8)   l##dir |= l##dir >> 8; \
        break

        switch (t_dim->lw) {
        default: assert(0); /* fall-through */
        case TX_4X4:   MERGE_CTX(a, uint8_t,  TX_4X4);
        case TX_8X8:   MERGE_CTX(a, uint16_t, TX_8X8);
        case TX_16X16: MERGE_CTX(a, uint32_t, TX_16X16);
        case TX_32X32: MERGE_CTX(a, uint32_t, TX_32X32);
        case TX_64X64: MERGE_CTX(a, uint32_t, TX_64X64);
        }
        switch (t_dim->lh) {
        default: assert(0); /* fall-through */
        case TX_4X4:   MERGE_CTX(l, uint8_t,  TX_4X4);
        case TX_8X8:   MERGE_CTX(l, uint16_t, TX_8X8);
        case TX_16X16: MERGE_CTX(l, uint32_t, TX_16X16);
        case TX_32X32: MERGE_CTX(l, uint32_t, TX_32X32);
        case TX_64X64: MERGE_CTX(l, uint32_t, TX_64X64);
        }
#undef MERGE_CTX

        return dav1d_skip_ctx[umin(la & 0x3F, 4)][umin(ll & 0x3F, 4)];
    }
}

static inline unsigned get_dc_sign_ctx(const int /*enum RectTxfmSize*/ tx,
                                       const uint8_t *const a,
                                       const uint8_t *const l)
{
    uint64_t mask = 0xC0C0C0C0C0C0C0C0ULL, mul = 0x0101010101010101ULL;
    int s;

#if ARCH_X86_64 && defined(__GNUC__)
    /* Coerce compilers into producing better code. For some reason
     * every x86-64 compiler is awful at handling 64-bit constants. */
    __asm__("" : "+r"(mask), "+r"(mul));
#endif

    switch(tx) {
    default: assert(0); /* fall-through */
    case TX_4X4: {
        int t = *(const uint8_t *) a >> 6;
        t    += *(const uint8_t *) l >> 6;
        s = t - 1 - 1;
        break;
    }
    case TX_8X8: {
        uint32_t t = *(const uint16_t *) a & (uint32_t) mask;
        t         += *(const uint16_t *) l & (uint32_t) mask;
        t *= 0x04040404U;
        s = (int) (t >> 24) - 2 - 2;
        break;
    }
    case TX_16X16: {
        uint32_t t = (*(const uint32_t *) a & (uint32_t) mask) >> 6;
        t         += (*(const uint32_t *) l & (uint32_t) mask) >> 6;
        t *= (uint32_t) mul;
        s = (int) (t >> 24) - 4 - 4;
        break;
    }
    case TX_32X32: {
        uint64_t t = (*(const uint64_t *) a & mask) >> 6;
        t         += (*(const uint64_t *) l & mask) >> 6;
        t *= mul;
        s = (int) (t >> 56) - 8 - 8;
        break;
    }
    case TX_64X64: {
        uint64_t t = (*(const uint64_t *) &a[0] & mask) >> 6;
        t         += (*(const uint64_t *) &a[8] & mask) >> 6;
        t         += (*(const uint64_t *) &l[0] & mask) >> 6;
        t         += (*(const uint64_t *) &l[8] & mask) >> 6;
        t *= mul;
        s = (int) (t >> 56) - 16 - 16;
        break;
    }
    case RTX_4X8: {
        uint32_t t = *(const uint8_t  *) a & (uint32_t) mask;
        t         += *(const uint16_t *) l & (uint32_t) mask;
        t *= 0x04040404U;
        s = (int) (t >> 24) - 1 - 2;
        break;
    }
    case RTX_8X4: {
        uint32_t t = *(const uint16_t *) a & (uint32_t) mask;
        t         += *(const uint8_t  *) l & (uint32_t) mask;
        t *= 0x04040404U;
        s = (int) (t >> 24) - 2 - 1;
        break;
    }
    case RTX_8X16: {
        uint32_t t = *(const uint16_t *) a & (uint32_t) mask;
        t         += *(const uint32_t *) l & (uint32_t) mask;
        t = (t >> 6) * (uint32_t) mul;
        s = (int) (t >> 24) - 2 - 4;
        break;
    }
    case RTX_16X8: {
        uint32_t t = *(const uint32_t *) a & (uint32_t) mask;
        t         += *(const uint16_t *) l & (uint32_t) mask;
        t = (t >> 6) * (uint32_t) mul;
        s = (int) (t >> 24) - 4 - 2;
        break;
    }
    case RTX_16X32: {
        uint64_t t = *(const uint32_t *) a & (uint32_t) mask;
        t         += *(const uint64_t *) l & mask;
        t = (t >> 6) * mul;
        s = (int) (t >> 56) - 4 - 8;
        break;
    }
    case RTX_32X16: {
        uint64_t t = *(const uint64_t *) a & mask;
        t         += *(const uint32_t *) l & (uint32_t) mask;
        t = (t >> 6) * mul;
        s = (int) (t >> 56) - 8 - 4;
        break;
    }
    case RTX_32X64: {
        uint64_t t = (*(const uint64_t *) &a[0] & mask) >> 6;
        t         += (*(const uint64_t *) &l[0] & mask) >> 6;
        t         += (*(const uint64_t *) &l[8] & mask) >> 6;
        t *= mul;
        s = (int) (t >> 56) - 8 - 16;
        break;
    }
    case RTX_64X32: {
        uint64_t t = (*(const uint64_t *) &a[0] & mask) >> 6;
        t         += (*(const uint64_t *) &a[8] & mask) >> 6;
        t         += (*(const uint64_t *) &l[0] & mask) >> 6;
        t *= mul;
        s = (int) (t >> 56) - 16 - 8;
        break;
    }
    case RTX_4X16: {
        uint32_t t = *(const uint8_t  *) a & (uint32_t) mask;
        t         += *(const uint32_t *) l & (uint32_t) mask;
        t = (t >> 6) * (uint32_t) mul;
        s = (int) (t >> 24) - 1 - 4;
        break;
    }
    case RTX_16X4: {
        uint32_t t = *(const uint32_t *) a & (uint32_t) mask;
        t         += *(const uint8_t  *) l & (uint32_t) mask;
        t = (t >> 6) * (uint32_t) mul;
        s = (int) (t >> 24) - 4 - 1;
        break;
    }
    case RTX_8X32: {
        uint64_t t = *(const uint16_t *) a & (uint32_t) mask;
        t         += *(const uint64_t *) l & mask;
        t = (t >> 6) * mul;
        s = (int) (t >> 56) - 2 - 8;
        break;
    }
    case RTX_32X8: {
        uint64_t t = *(const uint64_t *) a & mask;
        t         += *(const uint16_t *) l & (uint32_t) mask;
        t = (t >> 6) * mul;
        s = (int) (t >> 56) - 8 - 2;
        break;
    }
    case RTX_16X64: {
        uint64_t t = *(const uint32_t *) a & (uint32_t) mask;
        t         += *(const uint64_t *) &l[0] & mask;
        t = (t >> 6) + ((*(const uint64_t *) &l[8] & mask) >> 6);
        t *= mul;
        s = (int) (t >> 56) - 4 - 16;
        break;
    }
    case RTX_64X16: {
        uint64_t t = *(const uint64_t *) &a[0] & mask;
        t         += *(const uint32_t *) l & (uint32_t) mask;
        t = (t >> 6) + ((*(const uint64_t *) &a[8] & mask) >> 6);
        t *= mul;
        s = (int) (t >> 56) - 16 - 4;
        break;
    }
    }

    return (s != 0) + (s > 0);
}

static inline unsigned get_lo_ctx(const uint8_t *const levels,
                                  const enum TxClass tx_class,
                                  unsigned *const hi_mag,
                                  const uint8_t (*const ctx_offsets)[5],
                                  const unsigned x, const unsigned y,
                                  const ptrdiff_t stride)
{
    unsigned mag = levels[0 * stride + 1] + levels[1 * stride + 0];
    unsigned offset;
    if (tx_class == TX_CLASS_2D) {
        mag += levels[1 * stride + 1];
        *hi_mag = mag;
        mag += levels[0 * stride + 2] + levels[2 * stride + 0];
        offset = ctx_offsets[umin(y, 4)][umin(x, 4)];
    } else {
        mag += levels[0 * stride + 2];
        *hi_mag = mag;
        mag += levels[0 * stride + 3] + levels[0 * stride + 4];
        offset = 26 + (y > 1 ? 10 : y * 5);
    }
    return offset + (mag > 512 ? 4 : (mag + 64) >> 7);
}

static int decode_coefs(Dav1dTaskContext *const t,
                        uint8_t *const a, uint8_t *const l,
                        const enum RectTxfmSize tx, const enum BlockSize bs,
                        const Av1Block *const b, const int intra,
                        const int plane, coef *cf,
                        enum TxfmType *const txtp, uint8_t *res_ctx)
{
    Dav1dTileState *const ts = t->ts;
    const int chroma = !!plane;
    const Dav1dFrameContext *const f = t->f;
    const int lossless = f->frame_hdr->segmentation.lossless[b->seg_id];
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[tx];
    const int dbg = DEBUG_BLOCK_INFO && plane && 0;

    if (dbg)
        printf("Start: r=%d\n", ts->msac.rng);

    // does this block have any non-zero coefficients
    const int sctx = get_skip_ctx(t_dim, bs, a, l, chroma, f->cur.p.layout);
    const int all_skip = dav1d_msac_decode_bool_adapt(&ts->msac,
                             ts->cdf.coef.skip[t_dim->ctx][sctx]);
    if (dbg)
        printf("Post-non-zero[%d][%d][%d]: r=%d\n",
               t_dim->ctx, sctx, all_skip, ts->msac.rng);
    if (all_skip) {
        *res_ctx = 0x40;
        *txtp = lossless * WHT_WHT; /* lossless ? WHT_WHT : DCT_DCT */
        return -1;
    }

    // transform type (chroma: derived, luma: explicitly coded)
    if (lossless) {
        assert(t_dim->max == TX_4X4);
        *txtp = WHT_WHT;
    } else if (t_dim->max + intra >= TX_64X64) {
        *txtp = DCT_DCT;
    } else if (chroma) {
        // inferred from either the luma txtp (inter) or a LUT (intra)
        *txtp = intra ? dav1d_txtp_from_uvmode[b->uv_mode] :
                        get_uv_inter_txtp(t_dim, *txtp);
    } else if (!f->frame_hdr->segmentation.qidx[b->seg_id]) {
        // In libaom, lossless is checked by a literal qidx == 0, but not all
        // such blocks are actually lossless. The remainder gets an implicit
        // transform type (for luma)
        *txtp = DCT_DCT;
    } else {
        unsigned idx;
        if (intra) {
            const enum IntraPredMode y_mode_nofilt = b->y_mode == FILTER_PRED ?
                dav1d_filter_mode_to_y_mode[b->y_angle] : b->y_mode;
            if (f->frame_hdr->reduced_txtp_set || t_dim->min == TX_16X16) {
                idx = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                          ts->cdf.m.txtp_intra2[t_dim->min][y_mode_nofilt], 4);
                *txtp = dav1d_tx_types_per_set[idx + 0];
            } else {
                idx = dav1d_msac_decode_symbol_adapt8(&ts->msac,
                          ts->cdf.m.txtp_intra1[t_dim->min][y_mode_nofilt], 6);
                *txtp = dav1d_tx_types_per_set[idx + 5];
            }
            if (dbg)
                printf("Post-txtp-intra[%d->%d][%d][%d->%d]: r=%d\n",
                       tx, t_dim->min, y_mode_nofilt, idx, *txtp, ts->msac.rng);
        } else {
            if (f->frame_hdr->reduced_txtp_set || t_dim->max == TX_32X32) {
                idx = dav1d_msac_decode_bool_adapt(&ts->msac,
                          ts->cdf.m.txtp_inter3[t_dim->min]);
                *txtp = (idx - 1) & IDTX; /* idx ? DCT_DCT : IDTX */
            } else if (t_dim->min == TX_16X16) {
                idx = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                          ts->cdf.m.txtp_inter2, 11);
                *txtp = dav1d_tx_types_per_set[idx + 12];
            } else {
                idx = dav1d_msac_decode_symbol_adapt16(&ts->msac,
                          ts->cdf.m.txtp_inter1[t_dim->min], 15);
                *txtp = dav1d_tx_types_per_set[idx + 24];
            }
            if (dbg)
                printf("Post-txtp-inter[%d->%d][%d->%d]: r=%d\n",
                       tx, t_dim->min, idx, *txtp, ts->msac.rng);
        }
    }

    // find end-of-block (eob)
    int eob_bin;
    const int tx2dszctx = imin(t_dim->lw, TX_32X32) + imin(t_dim->lh, TX_32X32);
    const enum TxClass tx_class = dav1d_tx_type_class[*txtp];
    const int is_1d = tx_class != TX_CLASS_2D;
    switch (tx2dszctx) {
#define case_sz(sz, bin, ns, is_1d) \
    case sz: { \
        uint16_t *const eob_bin_cdf = ts->cdf.coef.eob_bin_##bin[chroma]is_1d; \
        eob_bin = dav1d_msac_decode_symbol_adapt##ns(&ts->msac, eob_bin_cdf, 4 + sz); \
        break; \
    }
    case_sz(0,   16,  4, [is_1d]);
    case_sz(1,   32,  8, [is_1d]);
    case_sz(2,   64,  8, [is_1d]);
    case_sz(3,  128,  8, [is_1d]);
    case_sz(4,  256, 16, [is_1d]);
    case_sz(5,  512, 16,        );
    case_sz(6, 1024, 16,        );
#undef case_sz
    }
    if (dbg)
        printf("Post-eob_bin_%d[%d][%d][%d]: r=%d\n",
               16 << tx2dszctx, chroma, is_1d, eob_bin, ts->msac.rng);
    int eob;
    if (eob_bin > 1) {
        uint16_t *const eob_hi_bit_cdf =
            ts->cdf.coef.eob_hi_bit[t_dim->ctx][chroma][eob_bin];
        const int eob_hi_bit = dav1d_msac_decode_bool_adapt(&ts->msac, eob_hi_bit_cdf);
        if (dbg)
            printf("Post-eob_hi_bit[%d][%d][%d][%d]: r=%d\n",
                   t_dim->ctx, chroma, eob_bin, eob_hi_bit, ts->msac.rng);
        eob = ((eob_hi_bit | 2) << (eob_bin - 2)) |
              dav1d_msac_decode_bools(&ts->msac, eob_bin - 2);
        if (dbg)
            printf("Post-eob[%d]: r=%d\n", eob, ts->msac.rng);
    } else {
        eob = eob_bin;
    }
    assert(eob >= 0);

    // base tokens
    uint16_t (*const eob_cdf)[4] = ts->cdf.coef.eob_base_tok[t_dim->ctx][chroma];
    uint16_t (*const hi_cdf)[4] = ts->cdf.coef.br_tok[imin(t_dim->ctx, 3)][chroma];
    unsigned rc, dc_tok;

    if (eob) {
        uint16_t (*const lo_cdf)[4] = ts->cdf.coef.base_tok[t_dim->ctx][chroma];
        uint8_t *const levels = t->scratch.levels; // bits 0-5: tok, 6-7: lo_tok
        const int sw = imin(t_dim->w, 8), sh = imin(t_dim->h, 8);

        /* eob */
        unsigned ctx = 1 + (eob > sw * sh * 2) + (eob > sw * sh * 4);
        int eob_tok = dav1d_msac_decode_symbol_adapt4(&ts->msac, eob_cdf[ctx], 2);
        int tok = eob_tok + 1;
        int level_tok = tok * 0x41;
        unsigned mag;

#define DECODE_COEFS_CLASS(tx_class) \
        unsigned x, y; \
        if (tx_class == TX_CLASS_2D) \
            rc = scan[eob], x = rc >> shift, y = rc & mask; \
        else if (tx_class == TX_CLASS_H) \
            /* Transposing reduces the stride and padding requirements */ \
            x = eob & mask, y = eob >> shift, rc = eob; \
        else /* tx_class == TX_CLASS_V */ \
            x = eob & mask, y = eob >> shift, rc = (x << shift2) | y; \
        if (dbg) \
            printf("Post-lo_tok[%d][%d][%d][%d=%d=%d]: r=%d\n", \
                   t_dim->ctx, chroma, ctx, eob, rc, tok, ts->msac.rng); \
        if (eob_tok == 2) { \
            ctx = (tx_class == TX_CLASS_2D ? (x | y) > 1 : y != 0) ? 14 : 7; \
            tok = dav1d_msac_decode_hi_tok(&ts->msac, hi_cdf[ctx]); \
            level_tok = tok + (3 << 6); \
            if (dbg) \
                printf("Post-hi_tok[%d][%d][%d][%d=%d=%d]: r=%d\n", \
                       imin(t_dim->ctx, 3), chroma, ctx, eob, rc, tok, \
                       ts->msac.rng); \
        } \
        cf[rc] = tok << 11; \
        levels[x * stride + y] = (uint8_t) level_tok; \
        for (int i = eob - 1; i > 0; i--) { /* ac */ \
            unsigned rc_i; \
            if (tx_class == TX_CLASS_2D) \
                rc_i = scan[i], x = rc_i >> shift, y = rc_i & mask; \
            else if (tx_class == TX_CLASS_H) \
                x = i & mask, y = i >> shift, rc_i = i; \
            else /* tx_class == TX_CLASS_V */ \
                x = i & mask, y = i >> shift, rc_i = (x << shift2) | y; \
            assert(x < 32 && y < 32); \
            uint8_t *const level = levels + x * stride + y; \
            ctx = get_lo_ctx(level, tx_class, &mag, lo_ctx_offsets, x, y, stride); \
            if (tx_class == TX_CLASS_2D) \
                y |= x; \
            tok = dav1d_msac_decode_symbol_adapt4(&ts->msac, lo_cdf[ctx], 3); \
            if (dbg) \
                printf("Post-lo_tok[%d][%d][%d][%d=%d=%d]: r=%d\n", \
                       t_dim->ctx, chroma, ctx, i, rc_i, tok, ts->msac.rng); \
            if (tok == 3) { \
                mag &= 63; \
                ctx = (y > (tx_class == TX_CLASS_2D) ? 14 : 7) + \
                      (mag > 12 ? 6 : (mag + 1) >> 1); \
                tok = dav1d_msac_decode_hi_tok(&ts->msac, hi_cdf[ctx]); \
                if (dbg) \
                    printf("Post-hi_tok[%d][%d][%d][%d=%d=%d]: r=%d\n", \
                           imin(t_dim->ctx, 3), chroma, ctx, i, rc_i, tok, \
                           ts->msac.rng); \
                *level = (uint8_t) (tok + (3 << 6)); \
                cf[rc_i] = (tok << 11) | rc; \
                rc = rc_i; \
            } else { \
                /* 0x1 for tok, 0x7ff as bitmask for rc, 0x41 for level_tok */ \
                tok *= 0x17ff41; \
                *level = (uint8_t) tok; \
                /* tok ? (tok << 11) | rc : 0 */ \
                tok = (tok >> 9) & (rc + ~0x7ffu); \
                if (tok) rc = rc_i; \
                cf[rc_i] = tok; \
            } \
        } \
        /* dc */ \
        ctx = (tx_class == TX_CLASS_2D) ? 0 : \
            get_lo_ctx(levels, tx_class, &mag, lo_ctx_offsets, 0, 0, stride); \
        dc_tok = dav1d_msac_decode_symbol_adapt4(&ts->msac, lo_cdf[ctx], 3); \
        if (dbg) \
            printf("Post-dc_lo_tok[%d][%d][%d][%d]: r=%d\n", \
                   t_dim->ctx, chroma, ctx, dc_tok, ts->msac.rng); \
        if (dc_tok == 3) { \
            if (tx_class == TX_CLASS_2D) \
                mag = levels[0 * stride + 1] + levels[1 * stride + 0] + \
                      levels[1 * stride + 1]; \
            mag &= 63; \
            ctx = mag > 12 ? 6 : (mag + 1) >> 1; \
            dc_tok = dav1d_msac_decode_hi_tok(&ts->msac, hi_cdf[ctx]); \
            if (dbg) \
                printf("Post-dc_hi_tok[%d][%d][0][%d]: r=%d\n", \
                       imin(t_dim->ctx, 3), chroma, dc_tok, ts->msac.rng); \
        } \
        break

        const uint16_t *scan;
        switch (tx_class) {
        case TX_CLASS_2D: {
            const unsigned nonsquare_tx = tx >= RTX_4X8;
            const uint8_t (*const lo_ctx_offsets)[5] =
                dav1d_lo_ctx_offsets[nonsquare_tx + (tx & nonsquare_tx)];
            scan = dav1d_scans[tx];
            const ptrdiff_t stride = 4 * sh;
            const unsigned shift = t_dim->lh < 4 ? t_dim->lh + 2 : 5, shift2 = 0;
            const unsigned mask = 4 * sh - 1;
            memset(levels, 0, stride * (4 * sw + 2));
            DECODE_COEFS_CLASS(TX_CLASS_2D);
        }
        case TX_CLASS_H: {
            const uint8_t (*const lo_ctx_offsets)[5] = NULL;
            const ptrdiff_t stride = 16;
            const unsigned shift = t_dim->lh + 2, shift2 = 0;
            const unsigned mask = 4 * sh - 1;
            memset(levels, 0, stride * (4 * sh + 2));
            DECODE_COEFS_CLASS(TX_CLASS_H);
        }
        case TX_CLASS_V: {
            const uint8_t (*const lo_ctx_offsets)[5] = NULL;
            const ptrdiff_t stride = 16;
            const unsigned shift = t_dim->lw + 2, shift2 = t_dim->lh + 2;
            const unsigned mask = 4 * sw - 1;
            memset(levels, 0, stride * (4 * sw + 2));
            DECODE_COEFS_CLASS(TX_CLASS_V);
        }
#undef DECODE_COEFS_CLASS
        default: assert(0);
        }
    } else { // dc-only
        int tok_br = dav1d_msac_decode_symbol_adapt4(&ts->msac, eob_cdf[0], 2);
        dc_tok = 1 + tok_br;
        if (dbg)
            printf("Post-dc_lo_tok[%d][%d][%d][%d]: r=%d\n",
                   t_dim->ctx, chroma, 0, dc_tok, ts->msac.rng);
        if (tok_br == 2) {
            dc_tok = dav1d_msac_decode_hi_tok(&ts->msac, hi_cdf[0]);
            if (dbg)
                printf("Post-dc_hi_tok[%d][%d][0][%d]: r=%d\n",
                       imin(t_dim->ctx, 3), chroma, dc_tok, ts->msac.rng);
        }
        rc = 0;
    }

    // residual and sign
    const uint16_t *const dq_tbl = ts->dq[b->seg_id][plane];
    const uint8_t *const qm_tbl = *txtp < IDTX ? f->qm[tx][plane] : NULL;
    const int dq_shift = imax(0, t_dim->ctx - 2);
    const unsigned cf_max = ~(~127U << (BITDEPTH == 8 ? 8 : f->cur.p.bpc));
    unsigned cul_level, dc_sign_level;

    if (!dc_tok) {
        cul_level = 0;
        dc_sign_level = 1 << 6;
        if (qm_tbl) goto ac_qm;
        goto ac_noqm;
    }

    const int dc_sign_ctx = get_dc_sign_ctx(tx, a, l);
    uint16_t *const dc_sign_cdf = ts->cdf.coef.dc_sign[chroma][dc_sign_ctx];
    const int dc_sign = dav1d_msac_decode_bool_adapt(&ts->msac, dc_sign_cdf);
    if (dbg)
        printf("Post-dc_sign[%d][%d][%d]: r=%d\n",
               chroma, dc_sign_ctx, dc_sign, ts->msac.rng);

    unsigned dc_dq = dq_tbl[0];
    dc_sign_level = (dc_sign - 1) & (2 << 6);

    if (qm_tbl) {
        dc_dq = (dc_dq * qm_tbl[0] + 16) >> 5;

        if (dc_tok == 15) {
            dc_tok = read_golomb(&ts->msac) + 15;
            if (dbg)
                printf("Post-dc_residual[%d->%d]: r=%d\n",
                       dc_tok - 15, dc_tok, ts->msac.rng);

            dc_tok &= 0xfffff;
            dc_dq = (dc_dq * dc_tok) & 0xffffff;
        } else {
            dc_dq *= dc_tok;
            assert(dc_dq <= 0xffffff);
        }
        cul_level = dc_tok;
        dc_dq >>= dq_shift;
        cf[0] = (coef) (umin(dc_dq - dc_sign, cf_max) ^ -dc_sign);

        if (rc) ac_qm: {
            const unsigned ac_dq = dq_tbl[1];
            do {
                const int sign = dav1d_msac_decode_bool_equi(&ts->msac);
                if (dbg)
                    printf("Post-sign[%d=%d]: r=%d\n", rc, sign, ts->msac.rng);
                const unsigned rc_tok = cf[rc];
                unsigned tok, dq = (ac_dq * qm_tbl[rc] + 16) >> 5;

                if (rc_tok >= (15 << 11)) {
                    tok = read_golomb(&ts->msac) + 15;
                    if (dbg)
                        printf("Post-residual[%d=%d->%d]: r=%d\n",
                               rc, tok - 15, tok, ts->msac.rng);

                    tok &= 0xfffff;
                    dq = (dq * tok) & 0xffffff;
                } else {
                    tok = rc_tok >> 11;
                    dq *= tok;
                    assert(dq <= 0xffffff);
                }
                cul_level += tok;
                dq >>= dq_shift;
                cf[rc] = (coef) (umin(dq - sign, cf_max) ^ -sign);

                rc = rc_tok & 0x3ff;
            } while (rc);
        }
    } else {
        // non-qmatrix is the common case and allows for additional optimizations
        if (dc_tok == 15) {
            dc_tok = read_golomb(&ts->msac) + 15;
            if (dbg)
                printf("Post-dc_residual[%d->%d]: r=%d\n",
                       dc_tok - 15, dc_tok, ts->msac.rng);

            dc_tok &= 0xfffff;
            dc_dq = ((dc_dq * dc_tok) & 0xffffff) >> dq_shift;
            dc_dq = umin(dc_dq - dc_sign, cf_max);
        } else {
            dc_dq = ((dc_dq * dc_tok) >> dq_shift) - dc_sign;
            assert(dc_dq <= cf_max);
        }
        cul_level = dc_tok;
        cf[0] = (coef) (dc_dq ^ -dc_sign);

        if (rc) ac_noqm: {
            const unsigned ac_dq = dq_tbl[1];
            do {
                const int sign = dav1d_msac_decode_bool_equi(&ts->msac);
                if (dbg)
                    printf("Post-sign[%d=%d]: r=%d\n", rc, sign, ts->msac.rng);
                const unsigned rc_tok = cf[rc];
                unsigned tok, dq;

                // residual
                if (rc_tok >= (15 << 11)) {
                    tok = read_golomb(&ts->msac) + 15;
                    if (dbg)
                        printf("Post-residual[%d=%d->%d]: r=%d\n",
                               rc, tok - 15, tok, ts->msac.rng);

                    // coefficient parsing, see 5.11.39
                    tok &= 0xfffff;

                    // dequant, see 7.12.3
                    dq = ((ac_dq * tok) & 0xffffff) >> dq_shift;
                    dq = umin(dq - sign, cf_max);
                } else {
                    // cannot exceed cf_max, so we can avoid the clipping
                    tok = rc_tok >> 11;
                    dq = ((ac_dq * tok) >> dq_shift) - sign;
                    assert(dq <= cf_max);
                }
                cul_level += tok;
                cf[rc] = (coef) (dq ^ -sign);

                rc = rc_tok & 0x3ff; // next non-zero rc, zero if eob
            } while (rc);
        }
    }

    // context
    *res_ctx = umin(cul_level, 63) | dc_sign_level;

    return eob;
}

static void read_coef_tree(Dav1dTaskContext *const t,
                           const enum BlockSize bs, const Av1Block *const b,
                           const enum RectTxfmSize ytx, const int depth,
                           const uint16_t *const tx_split,
                           const int x_off, const int y_off, pixel *dst)
{
    const Dav1dFrameContext *const f = t->f;
    Dav1dTileState *const ts = t->ts;
    const Dav1dDSPContext *const dsp = f->dsp;
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[ytx];
    const int txw = t_dim->w, txh = t_dim->h;

    /* y_off can be larger than 3 since lossless blocks use TX_4X4 but can't
     * be splitted. Aviods an undefined left shift. */
    if (depth < 2 && tx_split[depth] &&
        tx_split[depth] & (1 << (y_off * 4 + x_off)))
    {
        const enum RectTxfmSize sub = t_dim->sub;
        const TxfmInfo *const sub_t_dim = &dav1d_txfm_dimensions[sub];
        const int txsw = sub_t_dim->w, txsh = sub_t_dim->h;

        read_coef_tree(t, bs, b, sub, depth + 1, tx_split,
                       x_off * 2 + 0, y_off * 2 + 0, dst);
        t->bx += txsw;
        if (txw >= txh && t->bx < f->bw)
            read_coef_tree(t, bs, b, sub, depth + 1, tx_split, x_off * 2 + 1,
                           y_off * 2 + 0, dst ? &dst[4 * txsw] : NULL);
        t->bx -= txsw;
        t->by += txsh;
        if (txh >= txw && t->by < f->bh) {
            if (dst)
                dst += 4 * txsh * PXSTRIDE(f->cur.stride[0]);
            read_coef_tree(t, bs, b, sub, depth + 1, tx_split,
                           x_off * 2 + 0, y_off * 2 + 1, dst);
            t->bx += txsw;
            if (txw >= txh && t->bx < f->bw)
                read_coef_tree(t, bs, b, sub, depth + 1, tx_split, x_off * 2 + 1,
                               y_off * 2 + 1, dst ? &dst[4 * txsw] : NULL);
            t->bx -= txsw;
        }
        t->by -= txsh;
    } else {
        const int bx4 = t->bx & 31, by4 = t->by & 31;
        enum TxfmType txtp;
        uint8_t cf_ctx;
        int eob;
        coef *cf;
        struct CodedBlockInfo *cbi;

        if (t->frame_thread.pass) {
            const int p = t->frame_thread.pass & 1;
            assert(ts->frame_thread[p].cf);
            cf = ts->frame_thread[p].cf;
            ts->frame_thread[p].cf += imin(t_dim->w, 8) * imin(t_dim->h, 8) * 16;
            cbi = &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
        } else {
            cf = bitfn(t->cf);
        }
        if (t->frame_thread.pass != 2) {
            eob = decode_coefs(t, &t->a->lcoef[bx4], &t->l.lcoef[by4],
                               ytx, bs, b, 0, 0, cf, &txtp, &cf_ctx);
            if (DEBUG_BLOCK_INFO)
                printf("Post-y-cf-blk[tx=%d,txtp=%d,eob=%d]: r=%d\n",
                       ytx, txtp, eob, ts->msac.rng);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir lcoef, off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
            memset(&t->dir lcoef[off], cf_ctx, sz)
            case_set_upto16_with_default(imin(txh, f->bh - t->by), l., 1, by4);
            case_set_upto16_with_default(imin(txw, f->bw - t->bx), a->, 0, bx4);
#undef default_memset
#undef set_ctx
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            for (int y = 0; y < txh; y++) { \
                rep_macro(type, txtp_map, 0, mul * txtp); \
                txtp_map += 32; \
            }
            uint8_t *txtp_map = &t->txtp_map[by4 * 32 + bx4];
            case_set_upto16(txw,,,);
#undef set_ctx
            if (t->frame_thread.pass == 1) {
                cbi->eob[0] = eob;
                cbi->txtp[0] = txtp;
            }
        } else {
            eob = cbi->eob[0];
            txtp = cbi->txtp[0];
        }
        if (!(t->frame_thread.pass & 1)) {
            assert(dst);
            if (eob >= 0) {
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                    coef_dump(cf, imin(t_dim->h, 8) * 4, imin(t_dim->w, 8) * 4, 3, "dq");
                dsp->itx.itxfm_add[ytx][txtp](dst, f->cur.stride[0], cf, eob
                                              HIGHBD_CALL_SUFFIX);
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                    hex_dump(dst, f->cur.stride[0], t_dim->w * 4, t_dim->h * 4, "recon");
            }
        }
    }
}

void bytefn(dav1d_read_coef_blocks)(Dav1dTaskContext *const t,
                                    const enum BlockSize bs, const Av1Block *const b)
{
    const Dav1dFrameContext *const f = t->f;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int cbw4 = (bw4 + ss_hor) >> ss_hor, cbh4 = (bh4 + ss_ver) >> ss_ver;
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);

    if (b->skip) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir lcoef, off, mul * 0x40)
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx
        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir ccoef[0], off, mul * 0x40); \
            rep_macro(type, t->dir ccoef[1], off, mul * 0x40)
            case_set(cbh4, l., 1, cby4);
            case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
        }
        return;
    }

    Dav1dTileState *const ts = t->ts;
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;
    assert(t->frame_thread.pass == 1);
    assert(!b->skip);
    const TxfmInfo *const uv_t_dim = &dav1d_txfm_dimensions[b->uvtx];
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[b->intra ? b->tx : b->max_ytx];
    const uint16_t tx_split[2] = { b->tx_split0, b->tx_split1 };

    for (int init_y = 0; init_y < h4; init_y += 16) {
        const int sub_h4 = imin(h4, 16 + init_y);
        for (int init_x = 0; init_x < w4; init_x += 16) {
            const int sub_w4 = imin(w4, init_x + 16);
            int y_off = !!init_y, y, x;
            for (y = init_y, t->by += init_y; y < sub_h4;
                 y += t_dim->h, t->by += t_dim->h, y_off++)
            {
                struct CodedBlockInfo *const cbi =
                    &f->frame_thread.cbi[t->by * f->b4_stride];
                int x_off = !!init_x;
                for (x = init_x, t->bx += init_x; x < sub_w4;
                     x += t_dim->w, t->bx += t_dim->w, x_off++)
                {
                    if (!b->intra) {
                        read_coef_tree(t, bs, b, b->max_ytx, 0, tx_split,
                                       x_off, y_off, NULL);
                    } else {
                        uint8_t cf_ctx = 0x40;
                        enum TxfmType txtp;
                        const int eob = cbi[t->bx].eob[0] =
                            decode_coefs(t, &t->a->lcoef[bx4 + x],
                                         &t->l.lcoef[by4 + y], b->tx, bs, b, 1,
                                         0, ts->frame_thread[1].cf, &txtp, &cf_ctx);
                        if (DEBUG_BLOCK_INFO)
                            printf("Post-y-cf-blk[tx=%d,txtp=%d,eob=%d]: r=%d\n",
                                   b->tx, txtp, eob, ts->msac.rng);
                        cbi[t->bx].txtp[0] = txtp;
                        ts->frame_thread[1].cf += imin(t_dim->w, 8) * imin(t_dim->h, 8) * 16;
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                        rep_macro(type, t->dir lcoef, off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                        memset(&t->dir lcoef[off], cf_ctx, sz)
                        case_set_upto16_with_default(imin(t_dim->h, f->bh - t->by),
                                                     l., 1, by4 + y);
                        case_set_upto16_with_default(imin(t_dim->w, f->bw - t->bx),
                                                     a->, 0, bx4 + x);
#undef default_memset
#undef set_ctx
                    }
                }
                t->bx -= x;
            }
            t->by -= y;

            if (!has_chroma) continue;

            const int sub_ch4 = imin(ch4, (init_y + 16) >> ss_ver);
            const int sub_cw4 = imin(cw4, (init_x + 16) >> ss_hor);
            for (int pl = 0; pl < 2; pl++) {
                for (y = init_y >> ss_ver, t->by += init_y; y < sub_ch4;
                     y += uv_t_dim->h, t->by += uv_t_dim->h << ss_ver)
                {
                    struct CodedBlockInfo *const cbi =
                        &f->frame_thread.cbi[t->by * f->b4_stride];
                    for (x = init_x >> ss_hor, t->bx += init_x; x < sub_cw4;
                         x += uv_t_dim->w, t->bx += uv_t_dim->w << ss_hor)
                    {
                        uint8_t cf_ctx = 0x40;
                        enum TxfmType txtp;
                        if (!b->intra)
                            txtp = t->txtp_map[(by4 + (y << ss_ver)) * 32 +
                                                bx4 + (x << ss_hor)];
                        const int eob = cbi[t->bx].eob[1 + pl] =
                            decode_coefs(t, &t->a->ccoef[pl][cbx4 + x],
                                         &t->l.ccoef[pl][cby4 + y], b->uvtx, bs,
                                         b, b->intra, 1 + pl, ts->frame_thread[1].cf,
                                         &txtp, &cf_ctx);
                        if (DEBUG_BLOCK_INFO)
                            printf("Post-uv-cf-blk[pl=%d,tx=%d,"
                                   "txtp=%d,eob=%d]: r=%d\n",
                                   pl, b->uvtx, txtp, eob, ts->msac.rng);
                        cbi[t->bx].txtp[1 + pl] = txtp;
                        ts->frame_thread[1].cf += uv_t_dim->w * uv_t_dim->h * 16;
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                        rep_macro(type, t->dir ccoef[pl], off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                        memset(&t->dir ccoef[pl][off], cf_ctx, sz)
                        case_set_upto16_with_default( \
                                 imin(uv_t_dim->h, (f->bh - t->by + ss_ver) >> ss_ver),
                                 l., 1, cby4 + y);
                        case_set_upto16_with_default( \
                                 imin(uv_t_dim->w, (f->bw - t->bx + ss_hor) >> ss_hor),
                                 a->, 0, cbx4 + x);
#undef default_memset
#undef set_ctx
                    }
                    t->bx -= x << ss_hor;
                }
                t->by -= y << ss_ver;
            }
        }
    }
}

static int mc(Dav1dTaskContext *const t,
              pixel *const dst8, int16_t *const dst16, const ptrdiff_t dst_stride,
              const int bw4, const int bh4,
              const int bx, const int by, const int pl,
              const mv mv, const Dav1dThreadPicture *const refp, const int refidx,
              const enum Filter2d filter_2d)
{
    assert((dst8 != NULL) ^ (dst16 != NULL));
    const Dav1dFrameContext *const f = t->f;
    const int ss_ver = !!pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = !!pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    const int mvx = mv.x, mvy = mv.y;
    const int mx = mvx & (15 >> !ss_hor), my = mvy & (15 >> !ss_ver);
    ptrdiff_t ref_stride = refp->p.stride[!!pl];
    const pixel *ref;

    if (refp->p.p.w == f->cur.p.w && refp->p.p.h == f->cur.p.h) {
        const int dx = bx * h_mul + (mvx >> (3 + ss_hor));
        const int dy = by * v_mul + (mvy >> (3 + ss_ver));
        int w, h;

        if (refp->p.data[0] != f->cur.data[0]) { // i.e. not for intrabc
            w = (f->cur.p.w + ss_hor) >> ss_hor;
            h = (f->cur.p.h + ss_ver) >> ss_ver;
        } else {
            w = f->bw * 4 >> ss_hor;
            h = f->bh * 4 >> ss_ver;
        }
        if (dx < !!mx * 3 || dy < !!my * 3 ||
            dx + bw4 * h_mul + !!mx * 4 > w ||
            dy + bh4 * v_mul + !!my * 4 > h)
        {
            pixel *const emu_edge_buf = bitfn(t->scratch.emu_edge);
            f->dsp->mc.emu_edge(bw4 * h_mul + !!mx * 7, bh4 * v_mul + !!my * 7,
                                w, h, dx - !!mx * 3, dy - !!my * 3,
                                emu_edge_buf, 192 * sizeof(pixel),
                                refp->p.data[pl], ref_stride);
            ref = &emu_edge_buf[192 * !!my * 3 + !!mx * 3];
            ref_stride = 192 * sizeof(pixel);
        } else {
            ref = ((pixel *) refp->p.data[pl]) + PXSTRIDE(ref_stride) * dy + dx;
        }

        if (dst8 != NULL) {
            f->dsp->mc.mc[filter_2d](dst8, dst_stride, ref, ref_stride, bw4 * h_mul,
                                     bh4 * v_mul, mx << !ss_hor, my << !ss_ver
                                     HIGHBD_CALL_SUFFIX);
        } else {
            f->dsp->mc.mct[filter_2d](dst16, ref, ref_stride, bw4 * h_mul,
                                      bh4 * v_mul, mx << !ss_hor, my << !ss_ver
                                      HIGHBD_CALL_SUFFIX);
        }
    } else {
        assert(refp != &f->sr_cur);

        const int orig_pos_y = (by * v_mul << 4) + mvy * (1 << !ss_ver);
        const int orig_pos_x = (bx * h_mul << 4) + mvx * (1 << !ss_hor);
#define scale_mv(res, val, scale) do { \
            const int64_t tmp = (int64_t)(val) * scale + (scale - 0x4000) * 8; \
            res = apply_sign64((int) ((llabs(tmp) + 128) >> 8), tmp) + 32;     \
        } while (0)
        int pos_y, pos_x;
        scale_mv(pos_x, orig_pos_x, f->svc[refidx][0].scale);
        scale_mv(pos_y, orig_pos_y, f->svc[refidx][1].scale);
#undef scale_mv
        const int left = pos_x >> 10;
        const int top = pos_y >> 10;
        const int right =
            ((pos_x + (bw4 * h_mul - 1) * f->svc[refidx][0].step) >> 10) + 1;
        const int bottom =
            ((pos_y + (bh4 * v_mul - 1) * f->svc[refidx][1].step) >> 10) + 1;

        if (DEBUG_BLOCK_INFO)
            printf("Off %dx%d [%d,%d,%d], size %dx%d [%d,%d]\n",
                   left, top, orig_pos_x, f->svc[refidx][0].scale, refidx,
                   right-left, bottom-top,
                   f->svc[refidx][0].step, f->svc[refidx][1].step);

        const int w = (refp->p.p.w + ss_hor) >> ss_hor;
        const int h = (refp->p.p.h + ss_ver) >> ss_ver;
        if (left < 3 || top < 3 || right + 4 > w || bottom + 4 > h) {
            pixel *const emu_edge_buf = bitfn(t->scratch.emu_edge);
            f->dsp->mc.emu_edge(right - left + 7, bottom - top + 7,
                                w, h, left - 3, top - 3,
                                emu_edge_buf, 320 * sizeof(pixel),
                                refp->p.data[pl], ref_stride);
            ref = &emu_edge_buf[320 * 3 + 3];
            ref_stride = 320 * sizeof(pixel);
            if (DEBUG_BLOCK_INFO) printf("Emu\n");
        } else {
            ref = ((pixel *) refp->p.data[pl]) + PXSTRIDE(ref_stride) * top + left;
        }

        if (dst8 != NULL) {
            f->dsp->mc.mc_scaled[filter_2d](dst8, dst_stride, ref, ref_stride,
                                            bw4 * h_mul, bh4 * v_mul,
                                            pos_x & 0x3ff, pos_y & 0x3ff,
                                            f->svc[refidx][0].step,
                                            f->svc[refidx][1].step
                                            HIGHBD_CALL_SUFFIX);
        } else {
            f->dsp->mc.mct_scaled[filter_2d](dst16, ref, ref_stride,
                                             bw4 * h_mul, bh4 * v_mul,
                                             pos_x & 0x3ff, pos_y & 0x3ff,
                                             f->svc[refidx][0].step,
                                             f->svc[refidx][1].step
                                             HIGHBD_CALL_SUFFIX);
        }
    }

    return 0;
}

static int obmc(Dav1dTaskContext *const t,
                pixel *const dst, const ptrdiff_t dst_stride,
                const uint8_t *const b_dim, const int pl,
                const int bx4, const int by4, const int w4, const int h4)
{
    assert(!(t->bx & 1) && !(t->by & 1));
    const Dav1dFrameContext *const f = t->f;
    /*const*/ refmvs_block **r = &t->rt.r[(t->by & 31) + 5];
    pixel *const lap = bitfn(t->scratch.lap);
    const int ss_ver = !!pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = !!pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    int res;

    if (t->by > t->ts->tiling.row_start &&
        (!pl || b_dim[0] * h_mul + b_dim[1] * v_mul >= 16))
    {
        for (int i = 0, x = 0; x < w4 && i < imin(b_dim[2], 4); ) {
            // only odd blocks are considered for overlap handling, hence +1
            const refmvs_block *const a_r = &r[-1][t->bx + x + 1];
            const uint8_t *const a_b_dim = dav1d_block_dimensions[a_r->bs];

            if (a_r->ref.ref[0] > 0) {
                const int ow4 = iclip(a_b_dim[0], 2, b_dim[0]);
                const int oh4 = imin(b_dim[1], 16) >> 1;
                res = mc(t, lap, NULL, ow4 * h_mul * sizeof(pixel), ow4, (oh4 * 3 + 3) >> 2,
                         t->bx + x, t->by, pl, a_r->mv.mv[0],
                         &f->refp[a_r->ref.ref[0] - 1], a_r->ref.ref[0] - 1,
                         dav1d_filter_2d[t->a->filter[1][bx4 + x + 1]][t->a->filter[0][bx4 + x + 1]]);
                if (res) return res;
                f->dsp->mc.blend_h(&dst[x * h_mul], dst_stride, lap,
                                   h_mul * ow4, v_mul * oh4);
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
                const int ow4 = imin(b_dim[0], 16) >> 1;
                const int oh4 = iclip(l_b_dim[1], 2, b_dim[1]);
                res = mc(t, lap, NULL, h_mul * ow4 * sizeof(pixel), ow4, oh4,
                         t->bx, t->by + y, pl, l_r->mv.mv[0],
                         &f->refp[l_r->ref.ref[0] - 1], l_r->ref.ref[0] - 1,
                         dav1d_filter_2d[t->l.filter[1][by4 + y + 1]][t->l.filter[0][by4 + y + 1]]);
                if (res) return res;
                f->dsp->mc.blend_v(&dst[y * v_mul * PXSTRIDE(dst_stride)],
                                   dst_stride, lap, h_mul * ow4, v_mul * oh4);
                i++;
            }
            y += imax(l_b_dim[1], 2);
        }
    return 0;
}

static int warp_affine(Dav1dTaskContext *const t,
                       pixel *dst8, int16_t *dst16, const ptrdiff_t dstride,
                       const uint8_t *const b_dim, const int pl,
                       const Dav1dThreadPicture *const refp,
                       const Dav1dWarpedMotionParams *const wmp)
{
    assert((dst8 != NULL) ^ (dst16 != NULL));
    const Dav1dFrameContext *const f = t->f;
    const Dav1dDSPContext *const dsp = f->dsp;
    const int ss_ver = !!pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = !!pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    assert(!((b_dim[0] * h_mul) & 7) && !((b_dim[1] * v_mul) & 7));
    const int32_t *const mat = wmp->matrix;
    const int width = (refp->p.p.w + ss_hor) >> ss_hor;
    const int height = (refp->p.p.h + ss_ver) >> ss_ver;

    for (int y = 0; y < b_dim[1] * v_mul; y += 8) {
        const int src_y = t->by * 4 + ((y + 4) << ss_ver);
        const int64_t mat3_y = (int64_t) mat[3] * src_y + mat[0];
        const int64_t mat5_y = (int64_t) mat[5] * src_y + mat[1];
        for (int x = 0; x < b_dim[0] * h_mul; x += 8) {
            // calculate transformation relative to center of 8x8 block in
            // luma pixel units
            const int src_x = t->bx * 4 + ((x + 4) << ss_hor);
            const int64_t mvx = ((int64_t) mat[2] * src_x + mat3_y) >> ss_hor;
            const int64_t mvy = ((int64_t) mat[4] * src_x + mat5_y) >> ss_ver;

            const int dx = (int) (mvx >> 16) - 4;
            const int mx = (((int) mvx & 0xffff) - wmp->u.p.alpha * 4 -
                                                   wmp->u.p.beta  * 7) & ~0x3f;
            const int dy = (int) (mvy >> 16) - 4;
            const int my = (((int) mvy & 0xffff) - wmp->u.p.gamma * 4 -
                                                   wmp->u.p.delta * 4) & ~0x3f;

            const pixel *ref_ptr;
            ptrdiff_t ref_stride = refp->p.stride[!!pl];

            if (dx < 3 || dx + 8 + 4 > width || dy < 3 || dy + 8 + 4 > height) {
                pixel *const emu_edge_buf = bitfn(t->scratch.emu_edge);
                f->dsp->mc.emu_edge(15, 15, width, height, dx - 3, dy - 3,
                                    emu_edge_buf, 32 * sizeof(pixel),
                                    refp->p.data[pl], ref_stride);
                ref_ptr = &emu_edge_buf[32 * 3 + 3];
                ref_stride = 32 * sizeof(pixel);
            } else {
                ref_ptr = ((pixel *) refp->p.data[pl]) + PXSTRIDE(ref_stride) * dy + dx;
            }
            if (dst16 != NULL)
                dsp->mc.warp8x8t(&dst16[x], dstride, ref_ptr, ref_stride,
                                 wmp->u.abcd, mx, my HIGHBD_CALL_SUFFIX);
            else
                dsp->mc.warp8x8(&dst8[x], dstride, ref_ptr, ref_stride,
                                wmp->u.abcd, mx, my HIGHBD_CALL_SUFFIX);
        }
        if (dst8) dst8  += 8 * PXSTRIDE(dstride);
        else      dst16 += 8 * dstride;
    }
    return 0;
}

void bytefn(dav1d_recon_b_intra)(Dav1dTaskContext *const t, const enum BlockSize bs,
                                 const enum EdgeFlags intra_edge_flags,
                                 const Av1Block *const b)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    const Dav1dDSPContext *const dsp = f->dsp;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[b->tx];
    const TxfmInfo *const uv_t_dim = &dav1d_txfm_dimensions[b->uvtx];

    // coefficient coding
    pixel *const edge = bitfn(t->scratch.edge) + 128;
    const int cbw4 = (bw4 + ss_hor) >> ss_hor, cbh4 = (bh4 + ss_ver) >> ss_ver;

    const int intra_edge_filter_flag = f->seq_hdr->intra_edge_filter << 10;

    for (int init_y = 0; init_y < h4; init_y += 16) {
        const int sub_h4 = imin(h4, 16 + init_y);
        const int sub_ch4 = imin(ch4, (init_y + 16) >> ss_ver);
        for (int init_x = 0; init_x < w4; init_x += 16) {
            if (b->pal_sz[0]) {
                pixel *dst = ((pixel *) f->cur.data[0]) +
                             4 * (t->by * PXSTRIDE(f->cur.stride[0]) + t->bx);
                const uint8_t *pal_idx;
                if (t->frame_thread.pass) {
                    const int p = t->frame_thread.pass & 1;
                    assert(ts->frame_thread[p].pal_idx);
                    pal_idx = ts->frame_thread[p].pal_idx;
                    ts->frame_thread[p].pal_idx += bw4 * bh4 * 16;
                } else {
                    pal_idx = t->scratch.pal_idx;
                }
                const uint16_t *const pal = t->frame_thread.pass ?
                    f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                                        ((t->bx >> 1) + (t->by & 1))][0] : t->scratch.pal[0];
                f->dsp->ipred.pal_pred(dst, f->cur.stride[0], pal,
                                       pal_idx, bw4 * 4, bh4 * 4);
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                    hex_dump(dst, PXSTRIDE(f->cur.stride[0]),
                             bw4 * 4, bh4 * 4, "y-pal-pred");
            }

            const int intra_flags = (sm_flag(t->a, bx4) |
                                     sm_flag(&t->l, by4) |
                                     intra_edge_filter_flag);
            const int sb_has_tr = init_x + 16 < w4 ? 1 : init_y ? 0 :
                              intra_edge_flags & EDGE_I444_TOP_HAS_RIGHT;
            const int sb_has_bl = init_x ? 0 : init_y + 16 < h4 ? 1 :
                              intra_edge_flags & EDGE_I444_LEFT_HAS_BOTTOM;
            int y, x;
            const int sub_w4 = imin(w4, init_x + 16);
            for (y = init_y, t->by += init_y; y < sub_h4;
                 y += t_dim->h, t->by += t_dim->h)
            {
                pixel *dst = ((pixel *) f->cur.data[0]) +
                               4 * (t->by * PXSTRIDE(f->cur.stride[0]) +
                                    t->bx + init_x);
                for (x = init_x, t->bx += init_x; x < sub_w4;
                     x += t_dim->w, t->bx += t_dim->w)
                {
                    if (b->pal_sz[0]) goto skip_y_pred;

                    int angle = b->y_angle;
                    const enum EdgeFlags edge_flags =
                        (((y > init_y || !sb_has_tr) && (x + t_dim->w >= sub_w4)) ?
                             0 : EDGE_I444_TOP_HAS_RIGHT) |
                        ((x > init_x || (!sb_has_bl && y + t_dim->h >= sub_h4)) ?
                             0 : EDGE_I444_LEFT_HAS_BOTTOM);
                    const pixel *top_sb_edge = NULL;
                    if (!(t->by & (f->sb_step - 1))) {
                        top_sb_edge = f->ipred_edge[0];
                        const int sby = t->by >> f->sb_shift;
                        top_sb_edge += f->sb128w * 128 * (sby - 1);
                    }
                    const enum IntraPredMode m =
                        bytefn(dav1d_prepare_intra_edges)(t->bx,
                                                          t->bx > ts->tiling.col_start,
                                                          t->by,
                                                          t->by > ts->tiling.row_start,
                                                          ts->tiling.col_end,
                                                          ts->tiling.row_end,
                                                          edge_flags, dst,
                                                          f->cur.stride[0], top_sb_edge,
                                                          b->y_mode, &angle,
                                                          t_dim->w, t_dim->h,
                                                          f->seq_hdr->intra_edge_filter,
                                                          edge HIGHBD_CALL_SUFFIX);
                    dsp->ipred.intra_pred[m](dst, f->cur.stride[0], edge,
                                             t_dim->w * 4, t_dim->h * 4,
                                             angle | intra_flags,
                                             4 * f->bw - 4 * t->bx,
                                             4 * f->bh - 4 * t->by
                                             HIGHBD_CALL_SUFFIX);

                    if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                        hex_dump(edge - t_dim->h * 4, t_dim->h * 4,
                                 t_dim->h * 4, 2, "l");
                        hex_dump(edge, 0, 1, 1, "tl");
                        hex_dump(edge + 1, t_dim->w * 4,
                                 t_dim->w * 4, 2, "t");
                        hex_dump(dst, f->cur.stride[0],
                                 t_dim->w * 4, t_dim->h * 4, "y-intra-pred");
                    }

                skip_y_pred: {}
                    if (!b->skip) {
                        coef *cf;
                        int eob;
                        enum TxfmType txtp;
                        if (t->frame_thread.pass) {
                            const int p = t->frame_thread.pass & 1;
                            cf = ts->frame_thread[p].cf;
                            ts->frame_thread[p].cf += imin(t_dim->w, 8) * imin(t_dim->h, 8) * 16;
                            const struct CodedBlockInfo *const cbi =
                                &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
                            eob = cbi->eob[0];
                            txtp = cbi->txtp[0];
                        } else {
                            uint8_t cf_ctx;
                            cf = bitfn(t->cf);
                            eob = decode_coefs(t, &t->a->lcoef[bx4 + x],
                                               &t->l.lcoef[by4 + y], b->tx, bs,
                                               b, 1, 0, cf, &txtp, &cf_ctx);
                            if (DEBUG_BLOCK_INFO)
                                printf("Post-y-cf-blk[tx=%d,txtp=%d,eob=%d]: r=%d\n",
                                       b->tx, txtp, eob, ts->msac.rng);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                            rep_macro(type, t->dir lcoef, off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                            memset(&t->dir lcoef[off], cf_ctx, sz)
                            case_set_upto16_with_default(imin(t_dim->h, f->bh - t->by), \
                                                         l., 1, by4 + y);
                            case_set_upto16_with_default(imin(t_dim->w, f->bw - t->bx), \
                                                         a->, 0, bx4 + x);
#undef default_memset
#undef set_ctx
                        }
                        if (eob >= 0) {
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                coef_dump(cf, imin(t_dim->h, 8) * 4,
                                          imin(t_dim->w, 8) * 4, 3, "dq");
                            dsp->itx.itxfm_add[b->tx]
                                              [txtp](dst,
                                                     f->cur.stride[0],
                                                     cf, eob HIGHBD_CALL_SUFFIX);
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                hex_dump(dst, f->cur.stride[0],
                                         t_dim->w * 4, t_dim->h * 4, "recon");
                        }
                    } else if (!t->frame_thread.pass) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                        rep_macro(type, t->dir lcoef, off, mul * 0x40)
                        case_set_upto16(t_dim->h, l., 1, by4 + y);
                        case_set_upto16(t_dim->w, a->, 0, bx4 + x);
#undef set_ctx
                    }
                    dst += 4 * t_dim->w;
                }
                t->bx -= x;
            }
            t->by -= y;

            if (!has_chroma) continue;

            const ptrdiff_t stride = f->cur.stride[1];

            if (b->uv_mode == CFL_PRED) {
                assert(!init_x && !init_y);

                int16_t *const ac = t->scratch.ac;
                pixel *y_src = ((pixel *) f->cur.data[0]) + 4 * (t->bx & ~ss_hor) +
                                 4 * (t->by & ~ss_ver) * PXSTRIDE(f->cur.stride[0]);
                const ptrdiff_t uv_off = 4 * ((t->bx >> ss_hor) +
                                              (t->by >> ss_ver) * PXSTRIDE(stride));
                pixel *const uv_dst[2] = { ((pixel *) f->cur.data[1]) + uv_off,
                                           ((pixel *) f->cur.data[2]) + uv_off };

                const int furthest_r =
                    ((cw4 << ss_hor) + t_dim->w - 1) & ~(t_dim->w - 1);
                const int furthest_b =
                    ((ch4 << ss_ver) + t_dim->h - 1) & ~(t_dim->h - 1);
                dsp->ipred.cfl_ac[f->cur.p.layout - 1](ac, y_src, f->cur.stride[0],
                                                         cbw4 - (furthest_r >> ss_hor),
                                                         cbh4 - (furthest_b >> ss_ver),
                                                         cbw4 * 4, cbh4 * 4);
                for (int pl = 0; pl < 2; pl++) {
                    if (!b->cfl_alpha[pl]) continue;
                    int angle = 0;
                    const pixel *top_sb_edge = NULL;
                    if (!((t->by & ~ss_ver) & (f->sb_step - 1))) {
                        top_sb_edge = f->ipred_edge[pl + 1];
                        const int sby = t->by >> f->sb_shift;
                        top_sb_edge += f->sb128w * 128 * (sby - 1);
                    }
                    const int xpos = t->bx >> ss_hor, ypos = t->by >> ss_ver;
                    const int xstart = ts->tiling.col_start >> ss_hor;
                    const int ystart = ts->tiling.row_start >> ss_ver;
                    const enum IntraPredMode m =
                        bytefn(dav1d_prepare_intra_edges)(xpos, xpos > xstart,
                                                          ypos, ypos > ystart,
                                                          ts->tiling.col_end >> ss_hor,
                                                          ts->tiling.row_end >> ss_ver,
                                                          0, uv_dst[pl], stride,
                                                          top_sb_edge, DC_PRED, &angle,
                                                          uv_t_dim->w, uv_t_dim->h, 0,
                                                          edge HIGHBD_CALL_SUFFIX);
                    dsp->ipred.cfl_pred[m](uv_dst[pl], stride, edge,
                                           uv_t_dim->w * 4,
                                           uv_t_dim->h * 4,
                                           ac, b->cfl_alpha[pl]
                                           HIGHBD_CALL_SUFFIX);
                }
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                    ac_dump(ac, 4*cbw4, 4*cbh4, "ac");
                    hex_dump(uv_dst[0], stride, cbw4 * 4, cbh4 * 4, "u-cfl-pred");
                    hex_dump(uv_dst[1], stride, cbw4 * 4, cbh4 * 4, "v-cfl-pred");
                }
            } else if (b->pal_sz[1]) {
                const ptrdiff_t uv_dstoff = 4 * ((t->bx >> ss_hor) +
                                              (t->by >> ss_ver) * PXSTRIDE(f->cur.stride[1]));
                const uint16_t (*pal)[8];
                const uint8_t *pal_idx;
                if (t->frame_thread.pass) {
                    const int p = t->frame_thread.pass & 1;
                    assert(ts->frame_thread[p].pal_idx);
                    pal = f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                                              ((t->bx >> 1) + (t->by & 1))];
                    pal_idx = ts->frame_thread[p].pal_idx;
                    ts->frame_thread[p].pal_idx += cbw4 * cbh4 * 16;
                } else {
                    pal = t->scratch.pal;
                    pal_idx = &t->scratch.pal_idx[bw4 * bh4 * 16];
                }

                f->dsp->ipred.pal_pred(((pixel *) f->cur.data[1]) + uv_dstoff,
                                       f->cur.stride[1], pal[1],
                                       pal_idx, cbw4 * 4, cbh4 * 4);
                f->dsp->ipred.pal_pred(((pixel *) f->cur.data[2]) + uv_dstoff,
                                       f->cur.stride[1], pal[2],
                                       pal_idx, cbw4 * 4, cbh4 * 4);
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                    hex_dump(((pixel *) f->cur.data[1]) + uv_dstoff,
                             PXSTRIDE(f->cur.stride[1]),
                             cbw4 * 4, cbh4 * 4, "u-pal-pred");
                    hex_dump(((pixel *) f->cur.data[2]) + uv_dstoff,
                             PXSTRIDE(f->cur.stride[1]),
                             cbw4 * 4, cbh4 * 4, "v-pal-pred");
                }
            }

            const int sm_uv_fl = sm_uv_flag(t->a, cbx4) |
                                 sm_uv_flag(&t->l, cby4);
            const int uv_sb_has_tr =
                ((init_x + 16) >> ss_hor) < cw4 ? 1 : init_y ? 0 :
                intra_edge_flags & (EDGE_I420_TOP_HAS_RIGHT >> (f->cur.p.layout - 1));
            const int uv_sb_has_bl =
                init_x ? 0 : ((init_y + 16) >> ss_ver) < ch4 ? 1 :
                intra_edge_flags & (EDGE_I420_LEFT_HAS_BOTTOM >> (f->cur.p.layout - 1));
            const int sub_cw4 = imin(cw4, (init_x + 16) >> ss_hor);
            for (int pl = 0; pl < 2; pl++) {
                for (y = init_y >> ss_ver, t->by += init_y; y < sub_ch4;
                     y += uv_t_dim->h, t->by += uv_t_dim->h << ss_ver)
                {
                    pixel *dst = ((pixel *) f->cur.data[1 + pl]) +
                                   4 * ((t->by >> ss_ver) * PXSTRIDE(stride) +
                                        ((t->bx + init_x) >> ss_hor));
                    for (x = init_x >> ss_hor, t->bx += init_x; x < sub_cw4;
                         x += uv_t_dim->w, t->bx += uv_t_dim->w << ss_hor)
                    {
                        if ((b->uv_mode == CFL_PRED && b->cfl_alpha[pl]) ||
                            b->pal_sz[1])
                        {
                            goto skip_uv_pred;
                        }

                        int angle = b->uv_angle;
                        // this probably looks weird because we're using
                        // luma flags in a chroma loop, but that's because
                        // prepare_intra_edges() expects luma flags as input
                        const enum EdgeFlags edge_flags =
                            (((y > (init_y >> ss_ver) || !uv_sb_has_tr) &&
                              (x + uv_t_dim->w >= sub_cw4)) ?
                                 0 : EDGE_I444_TOP_HAS_RIGHT) |
                            ((x > (init_x >> ss_hor) ||
                              (!uv_sb_has_bl && y + uv_t_dim->h >= sub_ch4)) ?
                                 0 : EDGE_I444_LEFT_HAS_BOTTOM);
                        const pixel *top_sb_edge = NULL;
                        if (!((t->by & ~ss_ver) & (f->sb_step - 1))) {
                            top_sb_edge = f->ipred_edge[1 + pl];
                            const int sby = t->by >> f->sb_shift;
                            top_sb_edge += f->sb128w * 128 * (sby - 1);
                        }
                        const enum IntraPredMode uv_mode =
                             b->uv_mode == CFL_PRED ? DC_PRED : b->uv_mode;
                        const int xpos = t->bx >> ss_hor, ypos = t->by >> ss_ver;
                        const int xstart = ts->tiling.col_start >> ss_hor;
                        const int ystart = ts->tiling.row_start >> ss_ver;
                        const enum IntraPredMode m =
                            bytefn(dav1d_prepare_intra_edges)(xpos, xpos > xstart,
                                                              ypos, ypos > ystart,
                                                              ts->tiling.col_end >> ss_hor,
                                                              ts->tiling.row_end >> ss_ver,
                                                              edge_flags, dst, stride,
                                                              top_sb_edge, uv_mode,
                                                              &angle, uv_t_dim->w,
                                                              uv_t_dim->h,
                                                              f->seq_hdr->intra_edge_filter,
                                                              edge HIGHBD_CALL_SUFFIX);
                        angle |= intra_edge_filter_flag;
                        dsp->ipred.intra_pred[m](dst, stride, edge,
                                                 uv_t_dim->w * 4,
                                                 uv_t_dim->h * 4,
                                                 angle | sm_uv_fl,
                                                 (4 * f->bw + ss_hor -
                                                  4 * (t->bx & ~ss_hor)) >> ss_hor,
                                                 (4 * f->bh + ss_ver -
                                                  4 * (t->by & ~ss_ver)) >> ss_ver
                                                 HIGHBD_CALL_SUFFIX);
                        if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                            hex_dump(edge - uv_t_dim->h * 4, uv_t_dim->h * 4,
                                     uv_t_dim->h * 4, 2, "l");
                            hex_dump(edge, 0, 1, 1, "tl");
                            hex_dump(edge + 1, uv_t_dim->w * 4,
                                     uv_t_dim->w * 4, 2, "t");
                            hex_dump(dst, stride, uv_t_dim->w * 4,
                                     uv_t_dim->h * 4, pl ? "v-intra-pred" : "u-intra-pred");
                        }

                    skip_uv_pred: {}
                        if (!b->skip) {
                            enum TxfmType txtp;
                            int eob;
                            coef *cf;
                            if (t->frame_thread.pass) {
                                const int p = t->frame_thread.pass & 1;
                                cf = ts->frame_thread[p].cf;
                                ts->frame_thread[p].cf += uv_t_dim->w * uv_t_dim->h * 16;
                                const struct CodedBlockInfo *const cbi =
                                    &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
                                eob = cbi->eob[pl + 1];
                                txtp = cbi->txtp[pl + 1];
                            } else {
                                uint8_t cf_ctx;
                                cf = bitfn(t->cf);
                                eob = decode_coefs(t, &t->a->ccoef[pl][cbx4 + x],
                                                   &t->l.ccoef[pl][cby4 + y],
                                                   b->uvtx, bs, b, 1, 1 + pl, cf,
                                                   &txtp, &cf_ctx);
                                if (DEBUG_BLOCK_INFO)
                                    printf("Post-uv-cf-blk[pl=%d,tx=%d,"
                                           "txtp=%d,eob=%d]: r=%d [x=%d,cbx4=%d]\n",
                                           pl, b->uvtx, txtp, eob, ts->msac.rng, x, cbx4);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                                rep_macro(type, t->dir ccoef[pl], off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                                memset(&t->dir ccoef[pl][off], cf_ctx, sz)
                                case_set_upto16_with_default( \
                                         imin(uv_t_dim->h, (f->bh - t->by + ss_ver) >> ss_ver),
                                         l., 1, cby4 + y);
                                case_set_upto16_with_default( \
                                         imin(uv_t_dim->w, (f->bw - t->bx + ss_hor) >> ss_hor),
                                         a->, 0, cbx4 + x);
#undef default_memset
#undef set_ctx
                            }
                            if (eob >= 0) {
                                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                    coef_dump(cf, uv_t_dim->h * 4,
                                              uv_t_dim->w * 4, 3, "dq");
                                dsp->itx.itxfm_add[b->uvtx]
                                                  [txtp](dst, stride,
                                                         cf, eob HIGHBD_CALL_SUFFIX);
                                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                    hex_dump(dst, stride, uv_t_dim->w * 4,
                                             uv_t_dim->h * 4, "recon");
                            }
                        } else if (!t->frame_thread.pass) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                            rep_macro(type, t->dir ccoef[pl], off, mul * 0x40)
                            case_set_upto16(uv_t_dim->h, l., 1, cby4 + y);
                            case_set_upto16(uv_t_dim->w, a->, 0, cbx4 + x);
#undef set_ctx
                        }
                        dst += uv_t_dim->w * 4;
                    }
                    t->bx -= x << ss_hor;
                }
                t->by -= y << ss_ver;
            }
        }
    }
}

int bytefn(dav1d_recon_b_inter)(Dav1dTaskContext *const t, const enum BlockSize bs,
                                const Av1Block *const b)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    const Dav1dDSPContext *const dsp = f->dsp;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);
    const int chr_layout_idx = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I400 ? 0 :
                               DAV1D_PIXEL_LAYOUT_I444 - f->cur.p.layout;
    int res;

    // prediction
    const int cbh4 = (bh4 + ss_ver) >> ss_ver, cbw4 = (bw4 + ss_hor) >> ss_hor;
    pixel *dst = ((pixel *) f->cur.data[0]) +
        4 * (t->by * PXSTRIDE(f->cur.stride[0]) + t->bx);
    const ptrdiff_t uvdstoff =
        4 * ((t->bx >> ss_hor) + (t->by >> ss_ver) * PXSTRIDE(f->cur.stride[1]));
    if (IS_KEY_OR_INTRA(f->frame_hdr)) {
        // intrabc
        assert(!f->frame_hdr->super_res.enabled);
        res = mc(t, dst, NULL, f->cur.stride[0], bw4, bh4, t->bx, t->by, 0,
                 b->mv[0], &f->sr_cur, 0 /* unused */, FILTER_2D_BILINEAR);
        if (res) return res;
        if (has_chroma) for (int pl = 1; pl < 3; pl++) {
            res = mc(t, ((pixel *)f->cur.data[pl]) + uvdstoff, NULL, f->cur.stride[1],
                     bw4 << (bw4 == ss_hor), bh4 << (bh4 == ss_ver),
                     t->bx & ~ss_hor, t->by & ~ss_ver, pl, b->mv[0],
                     &f->sr_cur, 0 /* unused */, FILTER_2D_BILINEAR);
            if (res) return res;
        }
    } else if (b->comp_type == COMP_INTER_NONE) {
        const Dav1dThreadPicture *const refp = &f->refp[b->ref[0]];
        const enum Filter2d filter_2d = b->filter2d;

        if (imin(bw4, bh4) > 1 &&
            ((b->inter_mode == GLOBALMV && f->gmv_warp_allowed[b->ref[0]]) ||
             (b->motion_mode == MM_WARP && t->warpmv.type > DAV1D_WM_TYPE_TRANSLATION)))
        {
            res = warp_affine(t, dst, NULL, f->cur.stride[0], b_dim, 0, refp,
                              b->motion_mode == MM_WARP ? &t->warpmv :
                                  &f->frame_hdr->gmv[b->ref[0]]);
            if (res) return res;
        } else {
            res = mc(t, dst, NULL, f->cur.stride[0],
                     bw4, bh4, t->bx, t->by, 0, b->mv[0], refp, b->ref[0], filter_2d);
            if (res) return res;
            if (b->motion_mode == MM_OBMC) {
                res = obmc(t, dst, f->cur.stride[0], b_dim, 0, bx4, by4, w4, h4);
                if (res) return res;
            }
        }
        if (b->interintra_type) {
            pixel *const tl_edge = bitfn(t->scratch.edge) + 32;
            enum IntraPredMode m = b->interintra_mode == II_SMOOTH_PRED ?
                                   SMOOTH_PRED : b->interintra_mode;
            pixel *const tmp = bitfn(t->scratch.interintra);
            int angle = 0;
            const pixel *top_sb_edge = NULL;
            if (!(t->by & (f->sb_step - 1))) {
                top_sb_edge = f->ipred_edge[0];
                const int sby = t->by >> f->sb_shift;
                top_sb_edge += f->sb128w * 128 * (sby - 1);
            }
            m = bytefn(dav1d_prepare_intra_edges)(t->bx, t->bx > ts->tiling.col_start,
                                                  t->by, t->by > ts->tiling.row_start,
                                                  ts->tiling.col_end, ts->tiling.row_end,
                                                  0, dst, f->cur.stride[0], top_sb_edge,
                                                  m, &angle, bw4, bh4, 0, tl_edge
                                                  HIGHBD_CALL_SUFFIX);
            dsp->ipred.intra_pred[m](tmp, 4 * bw4 * sizeof(pixel),
                                     tl_edge, bw4 * 4, bh4 * 4, 0, 0, 0
                                     HIGHBD_CALL_SUFFIX);
            const uint8_t *const ii_mask =
                b->interintra_type == INTER_INTRA_BLEND ?
                     dav1d_ii_masks[bs][0][b->interintra_mode] :
                     dav1d_wedge_masks[bs][0][0][b->wedge_idx];
            dsp->mc.blend(dst, f->cur.stride[0], tmp,
                          bw4 * 4, bh4 * 4, ii_mask);
        }

        if (!has_chroma) goto skip_inter_chroma_pred;

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
            ptrdiff_t h_off = 0, v_off = 0;
            if (bw4 == 1 && bh4 == ss_ver) {
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff,
                             NULL, f->cur.stride[1],
                             bw4, bh4, t->bx - 1, t->by - 1, 1 + pl,
                             r[-1][t->bx - 1].mv.mv[0],
                             &f->refp[r[-1][t->bx - 1].ref.ref[0] - 1],
                             r[-1][t->bx - 1].ref.ref[0] - 1,
                             t->frame_thread.pass != 2 ? t->tl_4x4_filter :
                                 f->frame_thread.b[((t->by - 1) * f->b4_stride) + t->bx - 1].filter2d);
                    if (res) return res;
                }
                v_off = 2 * PXSTRIDE(f->cur.stride[1]);
                h_off = 2;
            }
            if (bw4 == 1) {
                const enum Filter2d left_filter_2d =
                    dav1d_filter_2d[t->l.filter[1][by4]][t->l.filter[0][by4]];
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff + v_off, NULL,
                             f->cur.stride[1], bw4, bh4, t->bx - 1,
                             t->by, 1 + pl, r[0][t->bx - 1].mv.mv[0],
                             &f->refp[r[0][t->bx - 1].ref.ref[0] - 1],
                             r[0][t->bx - 1].ref.ref[0] - 1,
                             t->frame_thread.pass != 2 ? left_filter_2d :
                                 f->frame_thread.b[(t->by * f->b4_stride) + t->bx - 1].filter2d);
                    if (res) return res;
                }
                h_off = 2;
            }
            if (bh4 == ss_ver) {
                const enum Filter2d top_filter_2d =
                    dav1d_filter_2d[t->a->filter[1][bx4]][t->a->filter[0][bx4]];
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff + h_off, NULL,
                             f->cur.stride[1], bw4, bh4, t->bx, t->by - 1,
                             1 + pl, r[-1][t->bx].mv.mv[0],
                             &f->refp[r[-1][t->bx].ref.ref[0] - 1],
                             r[-1][t->bx].ref.ref[0] - 1,
                             t->frame_thread.pass != 2 ? top_filter_2d :
                                 f->frame_thread.b[((t->by - 1) * f->b4_stride) + t->bx].filter2d);
                    if (res) return res;
                }
                v_off = 2 * PXSTRIDE(f->cur.stride[1]);
            }
            for (int pl = 0; pl < 2; pl++) {
                res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff + h_off + v_off, NULL, f->cur.stride[1],
                         bw4, bh4, t->bx, t->by, 1 + pl, b->mv[0],
                         refp, b->ref[0], filter_2d);
                if (res) return res;
            }
        } else {
            if (imin(cbw4, cbh4) > 1 &&
                ((b->inter_mode == GLOBALMV && f->gmv_warp_allowed[b->ref[0]]) ||
                 (b->motion_mode == MM_WARP && t->warpmv.type > DAV1D_WM_TYPE_TRANSLATION)))
            {
                for (int pl = 0; pl < 2; pl++) {
                    res = warp_affine(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff, NULL,
                                      f->cur.stride[1], b_dim, 1 + pl, refp,
                                      b->motion_mode == MM_WARP ? &t->warpmv :
                                          &f->frame_hdr->gmv[b->ref[0]]);
                    if (res) return res;
                }
            } else {
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff,
                             NULL, f->cur.stride[1],
                             bw4 << (bw4 == ss_hor), bh4 << (bh4 == ss_ver),
                             t->bx & ~ss_hor, t->by & ~ss_ver,
                             1 + pl, b->mv[0], refp, b->ref[0], filter_2d);
                    if (res) return res;
                    if (b->motion_mode == MM_OBMC) {
                        res = obmc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff,
                                   f->cur.stride[1], b_dim, 1 + pl, bx4, by4, w4, h4);
                        if (res) return res;
                    }
                }
            }
            if (b->interintra_type) {
                // FIXME for 8x32 with 4:2:2 subsampling, this probably does
                // the wrong thing since it will select 4x16, not 4x32, as a
                // transform size...
                const uint8_t *const ii_mask =
                    b->interintra_type == INTER_INTRA_BLEND ?
                         dav1d_ii_masks[bs][chr_layout_idx][b->interintra_mode] :
                         dav1d_wedge_masks[bs][chr_layout_idx][0][b->wedge_idx];

                for (int pl = 0; pl < 2; pl++) {
                    pixel *const tmp = bitfn(t->scratch.interintra);
                    pixel *const tl_edge = bitfn(t->scratch.edge) + 32;
                    enum IntraPredMode m =
                        b->interintra_mode == II_SMOOTH_PRED ?
                        SMOOTH_PRED : b->interintra_mode;
                    int angle = 0;
                    pixel *const uvdst = ((pixel *) f->cur.data[1 + pl]) + uvdstoff;
                    const pixel *top_sb_edge = NULL;
                    if (!(t->by & (f->sb_step - 1))) {
                        top_sb_edge = f->ipred_edge[pl + 1];
                        const int sby = t->by >> f->sb_shift;
                        top_sb_edge += f->sb128w * 128 * (sby - 1);
                    }
                    m = bytefn(dav1d_prepare_intra_edges)(t->bx >> ss_hor,
                                                          (t->bx >> ss_hor) >
                                                              (ts->tiling.col_start >> ss_hor),
                                                          t->by >> ss_ver,
                                                          (t->by >> ss_ver) >
                                                              (ts->tiling.row_start >> ss_ver),
                                                          ts->tiling.col_end >> ss_hor,
                                                          ts->tiling.row_end >> ss_ver,
                                                          0, uvdst, f->cur.stride[1],
                                                          top_sb_edge, m,
                                                          &angle, cbw4, cbh4, 0, tl_edge
                                                          HIGHBD_CALL_SUFFIX);
                    dsp->ipred.intra_pred[m](tmp, cbw4 * 4 * sizeof(pixel),
                                             tl_edge, cbw4 * 4, cbh4 * 4, 0, 0, 0
                                             HIGHBD_CALL_SUFFIX);
                    dsp->mc.blend(uvdst, f->cur.stride[1], tmp,
                                  cbw4 * 4, cbh4 * 4, ii_mask);
                }
            }
        }

    skip_inter_chroma_pred: {}
        t->tl_4x4_filter = filter_2d;
    } else {
        const enum Filter2d filter_2d = b->filter2d;
        // Maximum super block size is 128x128
        int16_t (*tmp)[128 * 128] = t->scratch.compinter;
        int jnt_weight;
        uint8_t *const seg_mask = t->scratch.seg_mask;
        const uint8_t *mask;

        for (int i = 0; i < 2; i++) {
            const Dav1dThreadPicture *const refp = &f->refp[b->ref[i]];

            if (b->inter_mode == GLOBALMV_GLOBALMV && f->gmv_warp_allowed[b->ref[i]]) {
                res = warp_affine(t, NULL, tmp[i], bw4 * 4, b_dim, 0, refp,
                                  &f->frame_hdr->gmv[b->ref[i]]);
                if (res) return res;
            } else {
                res = mc(t, NULL, tmp[i], 0, bw4, bh4, t->bx, t->by, 0,
                         b->mv[i], refp, b->ref[i], filter_2d);
                if (res) return res;
            }
        }
        switch (b->comp_type) {
        case COMP_INTER_AVG:
            dsp->mc.avg(dst, f->cur.stride[0], tmp[0], tmp[1],
                        bw4 * 4, bh4 * 4 HIGHBD_CALL_SUFFIX);
            break;
        case COMP_INTER_WEIGHTED_AVG:
            jnt_weight = f->jnt_weights[b->ref[0]][b->ref[1]];
            dsp->mc.w_avg(dst, f->cur.stride[0], tmp[0], tmp[1],
                          bw4 * 4, bh4 * 4, jnt_weight HIGHBD_CALL_SUFFIX);
            break;
        case COMP_INTER_SEG:
            dsp->mc.w_mask[chr_layout_idx](dst, f->cur.stride[0],
                                           tmp[b->mask_sign], tmp[!b->mask_sign],
                                           bw4 * 4, bh4 * 4, seg_mask,
                                           b->mask_sign HIGHBD_CALL_SUFFIX);
            mask = seg_mask;
            break;
        case COMP_INTER_WEDGE:
            mask = dav1d_wedge_masks[bs][0][0][b->wedge_idx];
            dsp->mc.mask(dst, f->cur.stride[0],
                         tmp[b->mask_sign], tmp[!b->mask_sign],
                         bw4 * 4, bh4 * 4, mask HIGHBD_CALL_SUFFIX);
            if (has_chroma)
                mask = dav1d_wedge_masks[bs][chr_layout_idx][b->mask_sign][b->wedge_idx];
            break;
        }

        // chroma
        if (has_chroma) for (int pl = 0; pl < 2; pl++) {
            for (int i = 0; i < 2; i++) {
                const Dav1dThreadPicture *const refp = &f->refp[b->ref[i]];
                if (b->inter_mode == GLOBALMV_GLOBALMV &&
                    imin(cbw4, cbh4) > 1 && f->gmv_warp_allowed[b->ref[i]])
                {
                    res = warp_affine(t, NULL, tmp[i], bw4 * 4 >> ss_hor,
                                      b_dim, 1 + pl,
                                      refp, &f->frame_hdr->gmv[b->ref[i]]);
                    if (res) return res;
                } else {
                    res = mc(t, NULL, tmp[i], 0, bw4, bh4, t->bx, t->by,
                             1 + pl, b->mv[i], refp, b->ref[i], filter_2d);
                    if (res) return res;
                }
            }
            pixel *const uvdst = ((pixel *) f->cur.data[1 + pl]) + uvdstoff;
            switch (b->comp_type) {
            case COMP_INTER_AVG:
                dsp->mc.avg(uvdst, f->cur.stride[1], tmp[0], tmp[1],
                            bw4 * 4 >> ss_hor, bh4 * 4 >> ss_ver
                            HIGHBD_CALL_SUFFIX);
                break;
            case COMP_INTER_WEIGHTED_AVG:
                dsp->mc.w_avg(uvdst, f->cur.stride[1], tmp[0], tmp[1],
                              bw4 * 4 >> ss_hor, bh4 * 4 >> ss_ver, jnt_weight
                              HIGHBD_CALL_SUFFIX);
                break;
            case COMP_INTER_WEDGE:
            case COMP_INTER_SEG:
                dsp->mc.mask(uvdst, f->cur.stride[1],
                             tmp[b->mask_sign], tmp[!b->mask_sign],
                             bw4 * 4 >> ss_hor, bh4 * 4 >> ss_ver, mask
                             HIGHBD_CALL_SUFFIX);
                break;
            }
        }
    }

    if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
        hex_dump(dst, f->cur.stride[0], b_dim[0] * 4, b_dim[1] * 4, "y-pred");
        if (has_chroma) {
            hex_dump(&((pixel *) f->cur.data[1])[uvdstoff], f->cur.stride[1],
                     cbw4 * 4, cbh4 * 4, "u-pred");
            hex_dump(&((pixel *) f->cur.data[2])[uvdstoff], f->cur.stride[1],
                     cbw4 * 4, cbh4 * 4, "v-pred");
        }
    }

    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;

    if (b->skip) {
        // reset coef contexts
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir lcoef, off, mul * 0x40)
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx
        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir ccoef[0], off, mul * 0x40); \
            rep_macro(type, t->dir ccoef[1], off, mul * 0x40)
            case_set(cbh4, l., 1, cby4);
            case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
        }
        return 0;
    }

    const TxfmInfo *const uvtx = &dav1d_txfm_dimensions[b->uvtx];
    const TxfmInfo *const ytx = &dav1d_txfm_dimensions[b->max_ytx];
    const uint16_t tx_split[2] = { b->tx_split0, b->tx_split1 };

    for (int init_y = 0; init_y < bh4; init_y += 16) {
        for (int init_x = 0; init_x < bw4; init_x += 16) {
            // coefficient coding & inverse transforms
            int y_off = !!init_y, y;
            dst += PXSTRIDE(f->cur.stride[0]) * 4 * init_y;
            for (y = init_y, t->by += init_y; y < imin(h4, init_y + 16);
                 y += ytx->h, y_off++)
            {
                int x, x_off = !!init_x;
                for (x = init_x, t->bx += init_x; x < imin(w4, init_x + 16);
                     x += ytx->w, x_off++)
                {
                    read_coef_tree(t, bs, b, b->max_ytx, 0, tx_split,
                                   x_off, y_off, &dst[x * 4]);
                    t->bx += ytx->w;
                }
                dst += PXSTRIDE(f->cur.stride[0]) * 4 * ytx->h;
                t->bx -= x;
                t->by += ytx->h;
            }
            dst -= PXSTRIDE(f->cur.stride[0]) * 4 * y;
            t->by -= y;

            // chroma coefs and inverse transform
            if (has_chroma) for (int pl = 0; pl < 2; pl++) {
                pixel *uvdst = ((pixel *) f->cur.data[1 + pl]) + uvdstoff +
                    (PXSTRIDE(f->cur.stride[1]) * init_y * 4 >> ss_ver);
                for (y = init_y >> ss_ver, t->by += init_y;
                     y < imin(ch4, (init_y + 16) >> ss_ver); y += uvtx->h)
                {
                    int x;
                    for (x = init_x >> ss_hor, t->bx += init_x;
                         x < imin(cw4, (init_x + 16) >> ss_hor); x += uvtx->w)
                    {
                        coef *cf;
                        int eob;
                        enum TxfmType txtp;
                        if (t->frame_thread.pass) {
                            const int p = t->frame_thread.pass & 1;
                            cf = ts->frame_thread[p].cf;
                            ts->frame_thread[p].cf += uvtx->w * uvtx->h * 16;
                            const struct CodedBlockInfo *const cbi =
                                &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
                            eob = cbi->eob[1 + pl];
                            txtp = cbi->txtp[1 + pl];
                        } else {
                            uint8_t cf_ctx;
                            cf = bitfn(t->cf);
                            txtp = t->txtp_map[(by4 + (y << ss_ver)) * 32 +
                                                bx4 + (x << ss_hor)];
                            eob = decode_coefs(t, &t->a->ccoef[pl][cbx4 + x],
                                               &t->l.ccoef[pl][cby4 + y],
                                               b->uvtx, bs, b, 0, 1 + pl,
                                               cf, &txtp, &cf_ctx);
                            if (DEBUG_BLOCK_INFO)
                                printf("Post-uv-cf-blk[pl=%d,tx=%d,"
                                       "txtp=%d,eob=%d]: r=%d\n",
                                       pl, b->uvtx, txtp, eob, ts->msac.rng);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                            rep_macro(type, t->dir ccoef[pl], off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                            memset(&t->dir ccoef[pl][off], cf_ctx, sz)
                            case_set_upto16_with_default( \
                                     imin(uvtx->h, (f->bh - t->by + ss_ver) >> ss_ver),
                                     l., 1, cby4 + y);
                            case_set_upto16_with_default( \
                                     imin(uvtx->w, (f->bw - t->bx + ss_hor) >> ss_hor),
                                     a->, 0, cbx4 + x);
#undef default_memset
#undef set_ctx
                        }
                        if (eob >= 0) {
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                coef_dump(cf, uvtx->h * 4, uvtx->w * 4, 3, "dq");
                            dsp->itx.itxfm_add[b->uvtx]
                                              [txtp](&uvdst[4 * x],
                                                     f->cur.stride[1],
                                                     cf, eob HIGHBD_CALL_SUFFIX);
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                hex_dump(&uvdst[4 * x], f->cur.stride[1],
                                         uvtx->w * 4, uvtx->h * 4, "recon");
                        }
                        t->bx += uvtx->w << ss_hor;
                    }
                    uvdst += PXSTRIDE(f->cur.stride[1]) * 4 * uvtx->h;
                    t->bx -= x << ss_hor;
                    t->by += uvtx->h << ss_ver;
                }
                t->by -= y << ss_ver;
            }
        }
    }
    return 0;
}

void bytefn(dav1d_filter_sbrow_deblock_cols)(Dav1dFrameContext *const f, const int sby) {
    if (!(f->c->inloop_filters & DAV1D_INLOOPFILTER_DEBLOCK) ||
        (!f->frame_hdr->loopfilter.level_y[0] && !f->frame_hdr->loopfilter.level_y[1]))
    {
        return;
    }
    const int y = sby * f->sb_step * 4;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    pixel *const p[3] = {
        f->lf.p[0] + y * PXSTRIDE(f->cur.stride[0]),
        f->lf.p[1] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
        f->lf.p[2] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver)
    };
    Av1Filter *mask = f->lf.mask + (sby >> !f->seq_hdr->sb128) * f->sb128w;
    bytefn(dav1d_loopfilter_sbrow_cols)(f, p, mask, sby,
                                        f->lf.start_of_tile_row[sby]);
}

void bytefn(dav1d_filter_sbrow_deblock_rows)(Dav1dFrameContext *const f, const int sby) {
    const int y = sby * f->sb_step * 4;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    pixel *const p[3] = {
        f->lf.p[0] + y * PXSTRIDE(f->cur.stride[0]),
        f->lf.p[1] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
        f->lf.p[2] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver)
    };
    Av1Filter *mask = f->lf.mask + (sby >> !f->seq_hdr->sb128) * f->sb128w;
    if (f->c->inloop_filters & DAV1D_INLOOPFILTER_DEBLOCK &&
        (f->frame_hdr->loopfilter.level_y[0] || f->frame_hdr->loopfilter.level_y[1]))
    {
        bytefn(dav1d_loopfilter_sbrow_rows)(f, p, mask, sby);
    }
    if (f->seq_hdr->cdef || f->lf.restore_planes) {
        // Store loop filtered pixels required by CDEF / LR
        bytefn(dav1d_copy_lpf)(f, p, sby);
    }
}

void bytefn(dav1d_filter_sbrow_cdef)(Dav1dTaskContext *const tc, const int sby) {
    const Dav1dFrameContext *const f = tc->f;
    if (!(f->c->inloop_filters & DAV1D_INLOOPFILTER_CDEF)) return;
    const int sbsz = f->sb_step;
    const int y = sby * sbsz * 4;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    pixel *const p[3] = {
        f->lf.p[0] + y * PXSTRIDE(f->cur.stride[0]),
        f->lf.p[1] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
        f->lf.p[2] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver)
    };
    Av1Filter *prev_mask = f->lf.mask + ((sby - 1) >> !f->seq_hdr->sb128) * f->sb128w;
    Av1Filter *mask = f->lf.mask + (sby >> !f->seq_hdr->sb128) * f->sb128w;
    const int start = sby * sbsz;
    if (sby) {
        const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        pixel *p_up[3] = {
            p[0] - 8 * PXSTRIDE(f->cur.stride[0]),
            p[1] - (8 * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
            p[2] - (8 * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
        };
        bytefn(dav1d_cdef_brow)(tc, p_up, prev_mask, start - 2, start, 1, sby);
    }
    const int n_blks = sbsz - 2 * (sby + 1 < f->sbh);
    const int end = imin(start + n_blks, f->bh);
    bytefn(dav1d_cdef_brow)(tc, p, mask, start, end, 0, sby);
}

void bytefn(dav1d_filter_sbrow_resize)(Dav1dFrameContext *const f, const int sby) {
    const int sbsz = f->sb_step;
    const int y = sby * sbsz * 4;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const pixel *const p[3] = {
        f->lf.p[0] + y * PXSTRIDE(f->cur.stride[0]),
        f->lf.p[1] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
        f->lf.p[2] + (y * PXSTRIDE(f->cur.stride[1]) >> ss_ver)
    };
    pixel *const sr_p[3] = {
        f->lf.sr_p[0] + y * PXSTRIDE(f->sr_cur.p.stride[0]),
        f->lf.sr_p[1] + (y * PXSTRIDE(f->sr_cur.p.stride[1]) >> ss_ver),
        f->lf.sr_p[2] + (y * PXSTRIDE(f->sr_cur.p.stride[1]) >> ss_ver)
    };
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400;
    for (int pl = 0; pl < 1 + 2 * has_chroma; pl++) {
        const int ss_ver = pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int h_start = 8 * !!sby >> ss_ver;
        const ptrdiff_t dst_stride = f->sr_cur.p.stride[!!pl];
        pixel *dst = sr_p[pl] - h_start * PXSTRIDE(dst_stride);
        const ptrdiff_t src_stride = f->cur.stride[!!pl];
        const pixel *src = p[pl] - h_start * PXSTRIDE(src_stride);
        const int h_end = 4 * (sbsz - 2 * (sby + 1 < f->sbh)) >> ss_ver;
        const int ss_hor = pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int dst_w = (f->sr_cur.p.p.w + ss_hor) >> ss_hor;
        const int src_w = (4 * f->bw + ss_hor) >> ss_hor;
        const int img_h = (f->cur.p.h - sbsz * 4 * sby + ss_ver) >> ss_ver;

        f->dsp->mc.resize(dst, dst_stride, src, src_stride, dst_w,
                          imin(img_h, h_end) + h_start, src_w,
                          f->resize_step[!!pl], f->resize_start[!!pl]
                          HIGHBD_CALL_SUFFIX);
    }
}

void bytefn(dav1d_filter_sbrow_lr)(Dav1dFrameContext *const f, const int sby) {
    if (!(f->c->inloop_filters & DAV1D_INLOOPFILTER_RESTORATION)) return;
    const int y = sby * f->sb_step * 4;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    pixel *const sr_p[3] = {
        f->lf.sr_p[0] + y * PXSTRIDE(f->sr_cur.p.stride[0]),
        f->lf.sr_p[1] + (y * PXSTRIDE(f->sr_cur.p.stride[1]) >> ss_ver),
        f->lf.sr_p[2] + (y * PXSTRIDE(f->sr_cur.p.stride[1]) >> ss_ver)
    };
    bytefn(dav1d_lr_sbrow)(f, sr_p, sby);
}

void bytefn(dav1d_filter_sbrow)(Dav1dFrameContext *const f, const int sby) {
    bytefn(dav1d_filter_sbrow_deblock_cols)(f, sby);
    bytefn(dav1d_filter_sbrow_deblock_rows)(f, sby);
    if (f->seq_hdr->cdef)
        bytefn(dav1d_filter_sbrow_cdef)(f->c->tc, sby);
    if (f->frame_hdr->width[0] != f->frame_hdr->width[1])
        bytefn(dav1d_filter_sbrow_resize)(f, sby);
    if (f->lf.restore_planes)
        bytefn(dav1d_filter_sbrow_lr)(f, sby);
}

void bytefn(dav1d_backup_ipred_edge)(Dav1dTaskContext *const t) {
    const Dav1dFrameContext *const f = t->f;
    Dav1dTileState *const ts = t->ts;
    const int sby = t->by >> f->sb_shift;
    const int sby_off = f->sb128w * 128 * sby;
    const int x_off = ts->tiling.col_start;

    const pixel *const y =
        ((const pixel *) f->cur.data[0]) + x_off * 4 +
                    ((t->by + f->sb_step) * 4 - 1) * PXSTRIDE(f->cur.stride[0]);
    pixel_copy(&f->ipred_edge[0][sby_off + x_off * 4], y,
               4 * (ts->tiling.col_end - x_off));

    if (f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400) {
        const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;

        const ptrdiff_t uv_off = (x_off * 4 >> ss_hor) +
            (((t->by + f->sb_step) * 4 >> ss_ver) - 1) * PXSTRIDE(f->cur.stride[1]);
        for (int pl = 1; pl <= 2; pl++)
            pixel_copy(&f->ipred_edge[pl][sby_off + (x_off * 4 >> ss_hor)],
                       &((const pixel *) f->cur.data[pl])[uv_off],
                       4 * (ts->tiling.col_end - x_off) >> ss_hor);
    }
}
