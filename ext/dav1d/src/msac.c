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

#include <limits.h>

#include "common/intops.h"

#include "src/msac.h"

#define EC_PROB_SHIFT 6
#define EC_MIN_PROB 4  // must be <= (1<<EC_PROB_SHIFT)/16

#define EC_WIN_SIZE (sizeof(ec_win) << 3)

static inline void ctx_refill(MsacContext *const s) {
    const uint8_t *buf_pos = s->buf_pos;
    const uint8_t *buf_end = s->buf_end;
    int c = EC_WIN_SIZE - s->cnt - 24;
    ec_win dif = s->dif;
    while (c >= 0 && buf_pos < buf_end) {
        dif ^= ((ec_win)*buf_pos++) << c;
        c -= 8;
    }
    s->dif = dif;
    s->cnt = EC_WIN_SIZE - c - 24;
    s->buf_pos = buf_pos;
}

/* Takes updated dif and range values, renormalizes them so that
 * 32768 <= rng < 65536 (reading more bytes from the stream into dif if
 * necessary), and stores them back in the decoder context.
 * dif: The new value of dif.
 * rng: The new value of the range. */
static inline void ctx_norm(MsacContext *const s, const ec_win dif,
                            const unsigned rng)
{
    const int d = 15 ^ (31 ^ clz(rng));
    assert(rng <= 65535U);
    s->cnt -= d;
    s->dif = ((dif + 1) << d) - 1; /* Shift in 1s in the LSBs */
    s->rng = rng << d;
    if (s->cnt < 0)
        ctx_refill(s);
}

unsigned dav1d_msac_decode_bool_equi_c(MsacContext *const s) {
    const unsigned r = s->rng;
    ec_win dif = s->dif;
    assert((dif >> (EC_WIN_SIZE - 16)) < r);
    // When the probability is 1/2, f = 16384 >> EC_PROB_SHIFT = 256 and we can
    // replace the multiply with a simple shift.
    unsigned v = ((r >> 8) << 7) + EC_MIN_PROB;
    const ec_win vw = (ec_win)v << (EC_WIN_SIZE - 16);
    const unsigned ret = dif >= vw;
    dif -= ret * vw;
    v += ret * (r - 2 * v);
    ctx_norm(s, dif, v);
    return !ret;
}

/* Decode a single binary value.
 * f: The probability that the bit is one
 * Return: The value decoded (0 or 1). */
unsigned dav1d_msac_decode_bool_c(MsacContext *const s, const unsigned f) {
    const unsigned r = s->rng;
    ec_win dif = s->dif;
    assert((dif >> (EC_WIN_SIZE - 16)) < r);
    unsigned v = ((r >> 8) * (f >> EC_PROB_SHIFT) >> (7 - EC_PROB_SHIFT)) + EC_MIN_PROB;
    const ec_win vw = (ec_win)v << (EC_WIN_SIZE - 16);
    const unsigned ret = dif >= vw;
    dif -= ret * vw;
    v += ret * (r - 2 * v);
    ctx_norm(s, dif, v);
    return !ret;
}

int dav1d_msac_decode_subexp(MsacContext *const s, const int ref,
                             const int n, unsigned k)
{
    assert(n >> k == 8);

    unsigned a = 0;
    if (dav1d_msac_decode_bool_equi(s)) {
        if (dav1d_msac_decode_bool_equi(s))
            k += dav1d_msac_decode_bool_equi(s) + 1;
        a = 1 << k;
    }
    const unsigned v = dav1d_msac_decode_bools(s, k) + a;
    return ref * 2 <= n ? inv_recenter(ref, v) :
                          n - 1 - inv_recenter(n - 1 - ref, v);
}

/* Decodes a symbol given an inverse cumulative distribution function (CDF)
 * table in Q15. */
unsigned dav1d_msac_decode_symbol_adapt_c(MsacContext *const s,
                                          uint16_t *const cdf,
                                          const size_t n_symbols)
{
    const unsigned c = s->dif >> (EC_WIN_SIZE - 16), r = s->rng >> 8;
    unsigned u, v = s->rng, val = -1;

    assert(n_symbols <= 15);
    assert(cdf[n_symbols] <= 32);

    do {
        val++;
        u = v;
        v = r * (cdf[val] >> EC_PROB_SHIFT);
        v >>= 7 - EC_PROB_SHIFT;
        v += EC_MIN_PROB * ((unsigned)n_symbols - val);
    } while (c < v);

    assert(u <= s->rng);

    ctx_norm(s, s->dif - ((ec_win)v << (EC_WIN_SIZE - 16)), u - v);

    if (s->allow_update_cdf) {
        const unsigned count = cdf[n_symbols];
        const unsigned rate = 4 + (count >> 4) + (n_symbols > 2);
        unsigned i;
        for (i = 0; i < val; i++)
            cdf[i] += (32768 - cdf[i]) >> rate;
        for (; i < n_symbols; i++)
            cdf[i] -= cdf[i] >> rate;
        cdf[n_symbols] = count + (count < 32);
    }

    return val;
}

unsigned dav1d_msac_decode_bool_adapt_c(MsacContext *const s,
                                        uint16_t *const cdf)
{
    const unsigned bit = dav1d_msac_decode_bool(s, *cdf);

    if (s->allow_update_cdf) {
        // update_cdf() specialized for boolean CDFs
        const unsigned count = cdf[1];
        const int rate = 4 + (count >> 4);
        if (bit)
            cdf[0] += (32768 - cdf[0]) >> rate;
        else
            cdf[0] -= cdf[0] >> rate;
        cdf[1] = count + (count < 32);
    }

    return bit;
}

unsigned dav1d_msac_decode_hi_tok_c(MsacContext *const s, uint16_t *const cdf) {
    unsigned tok_br = dav1d_msac_decode_symbol_adapt4(s, cdf, 3);
    unsigned tok = 3 + tok_br;
    if (tok_br == 3) {
        tok_br = dav1d_msac_decode_symbol_adapt4(s, cdf, 3);
        tok = 6 + tok_br;
        if (tok_br == 3) {
            tok_br = dav1d_msac_decode_symbol_adapt4(s, cdf, 3);
            tok = 9 + tok_br;
            if (tok_br == 3)
                tok = 12 + dav1d_msac_decode_symbol_adapt4(s, cdf, 3);
        }
    }
    return tok;
}

void dav1d_msac_init(MsacContext *const s, const uint8_t *const data,
                     const size_t sz, const int disable_cdf_update_flag)
{
    s->buf_pos = data;
    s->buf_end = data + sz;
    s->dif = ((ec_win)1 << (EC_WIN_SIZE - 1)) - 1;
    s->rng = 0x8000;
    s->cnt = -15;
    s->allow_update_cdf = !disable_cdf_update_flag;
    ctx_refill(s);

#if ARCH_X86_64 && HAVE_ASM
    s->symbol_adapt16 = dav1d_msac_decode_symbol_adapt_c;

    dav1d_msac_init_x86(s);
#endif
}
