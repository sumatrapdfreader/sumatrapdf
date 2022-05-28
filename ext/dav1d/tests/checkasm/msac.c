/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Two Orioles, LLC
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

#include "tests/checkasm/checkasm.h"

#include "src/cpu.h"
#include "src/msac.h"

#include <stdio.h>
#include <string.h>

#define BUF_SIZE 8192

/* The normal code doesn't use function pointers */
typedef unsigned (*decode_symbol_adapt_fn)(MsacContext *s, uint16_t *cdf,
                                           size_t n_symbols);
typedef unsigned (*decode_adapt_fn)(MsacContext *s, uint16_t *cdf);
typedef unsigned (*decode_bool_equi_fn)(MsacContext *s);
typedef unsigned (*decode_bool_fn)(MsacContext *s, unsigned f);

typedef struct {
    decode_symbol_adapt_fn decode_symbol_adapt4;
    decode_symbol_adapt_fn decode_symbol_adapt8;
    decode_symbol_adapt_fn decode_symbol_adapt16;
    decode_adapt_fn        decode_bool_adapt;
    decode_bool_equi_fn    decode_bool_equi;
    decode_bool_fn         decode_bool;
    decode_adapt_fn        decode_hi_tok;
} MsacDSPContext;

static void randomize_cdf(uint16_t *const cdf, const int n) {
    int i;
    for (i = 15; i > n; i--)
        cdf[i] = rnd(); // padding
    cdf[i] = 0;         // count
    do {
        cdf[i - 1] = cdf[i] + rnd() % (32768 - cdf[i] - i) + 1;
    } while (--i > 0);
}

/* memcmp() on structs can have weird behavior due to padding etc. */
static int msac_cmp(const MsacContext *const a, const MsacContext *const b) {
    return a->buf_pos != b->buf_pos || a->buf_end != b->buf_end ||
           a->dif != b->dif || a->rng != b->rng || a->cnt != b->cnt ||
           a->allow_update_cdf != b->allow_update_cdf;
}

static void msac_dump(unsigned c_res, unsigned a_res,
                      const MsacContext *const a, const MsacContext *const b,
                      const uint16_t *const cdf_a, const uint16_t *const cdf_b,
                      const int num_cdf)
{
    if (c_res != a_res)
        fprintf(stderr, "c_res %u a_res %u\n", c_res, a_res);
    if (a->buf_pos != b->buf_pos)
        fprintf(stderr, "buf_pos %p vs %p\n", a->buf_pos, b->buf_pos);
    if (a->buf_end != b->buf_end)
        fprintf(stderr, "buf_end %p vs %p\n", a->buf_end, b->buf_end);
    if (a->dif != b->dif)
        fprintf(stderr, "dif %zx vs %zx\n", a->dif, b->dif);
    if (a->rng != b->rng)
        fprintf(stderr, "rng %u vs %u\n", a->rng, b->rng);
    if (a->cnt != b->cnt)
        fprintf(stderr, "cnt %d vs %d\n", a->cnt, b->cnt);
    if (a->allow_update_cdf)
        fprintf(stderr, "allow_update_cdf %d vs %d\n",
                a->allow_update_cdf, b->allow_update_cdf);
    if (num_cdf && memcmp(cdf_a, cdf_b, sizeof(*cdf_a) * (num_cdf + 1))) {
        fprintf(stderr, "cdf:\n");
        for (int i = 0; i <= num_cdf; i++)
            fprintf(stderr, " %5u", cdf_a[i]);
        fprintf(stderr, "\n");
        for (int i = 0; i <= num_cdf; i++)
            fprintf(stderr, " %5u", cdf_b[i]);
        fprintf(stderr, "\n");
        for (int i = 0; i <= num_cdf; i++)
            fprintf(stderr, "     %c", cdf_a[i] != cdf_b[i] ? 'x' : '.');
        fprintf(stderr, "\n");
    }
}

#define CHECK_SYMBOL_ADAPT(n, n_min, n_max) do {                           \
    if (check_func(c->decode_symbol_adapt##n,                              \
                   "msac_decode_symbol_adapt%d", n))                       \
    {                                                                      \
        for (int cdf_update = 0; cdf_update <= 1; cdf_update++) {          \
            for (int ns = n_min; ns <= n_max; ns++) {                      \
                dav1d_msac_init(&s_c, buf, BUF_SIZE, !cdf_update);         \
                s_a = s_c;                                                 \
                randomize_cdf(cdf[0], ns);                                 \
                memcpy(cdf[1], cdf[0], sizeof(*cdf));                      \
                for (int i = 0; i < 64; i++) {                             \
                    unsigned c_res = call_ref(&s_c, cdf[0], ns);           \
                    unsigned a_res = call_new(&s_a, cdf[1], ns);           \
                    if (c_res != a_res || msac_cmp(&s_c, &s_a) ||          \
                        memcmp(cdf[0], cdf[1], sizeof(**cdf) * (ns + 1)))  \
                    {                                                      \
                        if (fail())                                        \
                            msac_dump(c_res, a_res, &s_c, &s_a,            \
                                      cdf[0], cdf[1], ns);                 \
                    }                                                      \
                }                                                          \
                if (cdf_update && ns == n - 1)                             \
                    bench_new(alternate(&s_c, &s_a),                       \
                              alternate(cdf[0], cdf[1]), ns);              \
            }                                                              \
        }                                                                  \
    }                                                                      \
} while (0)

static void check_decode_symbol(MsacDSPContext *const c, uint8_t *const buf) {
    ALIGN_STK_32(uint16_t, cdf, 2, [16]);
    MsacContext s_c, s_a;

    declare_func(unsigned, MsacContext *s, uint16_t *cdf, size_t n_symbols);
    CHECK_SYMBOL_ADAPT( 4, 1,  4);
    CHECK_SYMBOL_ADAPT( 8, 1,  7);
    CHECK_SYMBOL_ADAPT(16, 3, 15);
    report("decode_symbol");
}

static void check_decode_bool_adapt(MsacDSPContext *const c, uint8_t *const buf) {
    MsacContext s_c, s_a;

    declare_func(unsigned, MsacContext *s, uint16_t *cdf);
    if (check_func(c->decode_bool_adapt, "msac_decode_bool_adapt")) {
        uint16_t cdf[2][2];
        for (int cdf_update = 0; cdf_update <= 1; cdf_update++) {
            dav1d_msac_init(&s_c, buf, BUF_SIZE, !cdf_update);
            s_a = s_c;
            cdf[0][0] = cdf[1][0] = rnd() % 32767 + 1;
            cdf[0][1] = cdf[1][1] = 0;
            for (int i = 0; i < 64; i++) {
                unsigned c_res = call_ref(&s_c, cdf[0]);
                unsigned a_res = call_new(&s_a, cdf[1]);
                if (c_res != a_res || msac_cmp(&s_c, &s_a) ||
                    memcmp(cdf[0], cdf[1], sizeof(*cdf)))
                {
                    if (fail())
                        msac_dump(c_res, a_res, &s_c, &s_a, cdf[0], cdf[1], 1);
                }
            }
            if (cdf_update)
                bench_new(alternate(&s_c, &s_a), alternate(cdf[0], cdf[1]));
        }
    }
}

static void check_decode_bool_equi(MsacDSPContext *const c, uint8_t *const buf) {
    MsacContext s_c, s_a;

    declare_func(unsigned, MsacContext *s);
    if (check_func(c->decode_bool_equi, "msac_decode_bool_equi")) {
        dav1d_msac_init(&s_c, buf, BUF_SIZE, 1);
        s_a = s_c;
        for (int i = 0; i < 64; i++) {
            unsigned c_res = call_ref(&s_c);
            unsigned a_res = call_new(&s_a);
            if (c_res != a_res || msac_cmp(&s_c, &s_a)) {
                if (fail())
                    msac_dump(c_res, a_res, &s_c, &s_a, NULL, NULL, 0);
            }
        }
        bench_new(alternate(&s_c, &s_a));
    }
}

static void check_decode_bool(MsacDSPContext *const c, uint8_t *const buf) {
    MsacContext s_c, s_a;

    declare_func(unsigned, MsacContext *s, unsigned f);
    if (check_func(c->decode_bool, "msac_decode_bool")) {
        dav1d_msac_init(&s_c, buf, BUF_SIZE, 1);
        s_a = s_c;
        for (int i = 0; i < 64; i++) {
            const unsigned f = rnd() & 0x7fff;
            unsigned c_res = call_ref(&s_c, f);
            unsigned a_res = call_new(&s_a, f);
            if (c_res != a_res || msac_cmp(&s_c, &s_a)) {
                if (fail())
                    msac_dump(c_res, a_res, &s_c, &s_a, NULL, NULL, 0);
            }
        }
        bench_new(alternate(&s_c, &s_a), 16384);
    }

}

static void check_decode_bool_funcs(MsacDSPContext *const c, uint8_t *const buf) {
    check_decode_bool_adapt(c, buf);
    check_decode_bool_equi(c, buf);
    check_decode_bool(c, buf);
    report("decode_bool");
}

static void check_decode_hi_tok(MsacDSPContext *const c, uint8_t *const buf) {
    ALIGN_STK_16(uint16_t, cdf, 2, [16]);
    MsacContext s_c, s_a;

    declare_func(unsigned, MsacContext *s, uint16_t *cdf);
    if (check_func(c->decode_hi_tok, "msac_decode_hi_tok")) {
        for (int cdf_update = 0; cdf_update <= 1; cdf_update++) {
            dav1d_msac_init(&s_c, buf, BUF_SIZE, !cdf_update);
            s_a = s_c;
            randomize_cdf(cdf[0], 3);
            memcpy(cdf[1], cdf[0], sizeof(*cdf));
            for (int i = 0; i < 64; i++) {
                unsigned c_res = call_ref(&s_c, cdf[0]);
                unsigned a_res = call_new(&s_a, cdf[1]);
                if (c_res != a_res || msac_cmp(&s_c, &s_a) ||
                    memcmp(cdf[0], cdf[1], sizeof(*cdf)))
                {
                    if (fail())
                        msac_dump(c_res, a_res, &s_c, &s_a, cdf[0], cdf[1], 3);
                    break;
                }
            }
            if (cdf_update)
                bench_new(alternate(&s_c, &s_a), alternate(cdf[0], cdf[1]));
        }
    }
    report("decode_hi_tok");
}

void checkasm_check_msac(void) {
    MsacDSPContext c;
    c.decode_symbol_adapt4  = dav1d_msac_decode_symbol_adapt_c;
    c.decode_symbol_adapt8  = dav1d_msac_decode_symbol_adapt_c;
    c.decode_symbol_adapt16 = dav1d_msac_decode_symbol_adapt_c;
    c.decode_bool_adapt     = dav1d_msac_decode_bool_adapt_c;
    c.decode_bool_equi      = dav1d_msac_decode_bool_equi_c;
    c.decode_bool           = dav1d_msac_decode_bool_c;
    c.decode_hi_tok         = dav1d_msac_decode_hi_tok_c;

#if (ARCH_AARCH64 || ARCH_ARM) && HAVE_ASM
    if (dav1d_get_cpu_flags() & DAV1D_ARM_CPU_FLAG_NEON) {
        c.decode_symbol_adapt4  = dav1d_msac_decode_symbol_adapt4_neon;
        c.decode_symbol_adapt8  = dav1d_msac_decode_symbol_adapt8_neon;
        c.decode_symbol_adapt16 = dav1d_msac_decode_symbol_adapt16_neon;
        c.decode_bool_adapt     = dav1d_msac_decode_bool_adapt_neon;
        c.decode_bool_equi      = dav1d_msac_decode_bool_equi_neon;
        c.decode_bool           = dav1d_msac_decode_bool_neon;
        c.decode_hi_tok         = dav1d_msac_decode_hi_tok_neon;
    }
#elif ARCH_X86 && HAVE_ASM
    if (dav1d_get_cpu_flags() & DAV1D_X86_CPU_FLAG_SSE2) {
        c.decode_symbol_adapt4  = dav1d_msac_decode_symbol_adapt4_sse2;
        c.decode_symbol_adapt8  = dav1d_msac_decode_symbol_adapt8_sse2;
        c.decode_symbol_adapt16 = dav1d_msac_decode_symbol_adapt16_sse2;
        c.decode_bool_adapt     = dav1d_msac_decode_bool_adapt_sse2;
        c.decode_bool_equi      = dav1d_msac_decode_bool_equi_sse2;
        c.decode_bool           = dav1d_msac_decode_bool_sse2;
        c.decode_hi_tok         = dav1d_msac_decode_hi_tok_sse2;
    }

#if ARCH_X86_64
    if (dav1d_get_cpu_flags() & DAV1D_X86_CPU_FLAG_AVX2) {
        c.decode_symbol_adapt16 = dav1d_msac_decode_symbol_adapt16_avx2;
    }
#endif
#endif

    uint8_t buf[BUF_SIZE];
    for (int i = 0; i < BUF_SIZE; i++)
        buf[i] = rnd();

    check_decode_symbol(&c, buf);
    check_decode_bool_funcs(&c, buf);
    check_decode_hi_tok(&c, buf);
}
