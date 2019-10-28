/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdio.h>
#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"

struct _Jbig2ArithState {
    uint32_t C;
    int A;

    int CT;

    uint32_t next_word;
    int next_word_bytes;

    Jbig2WordStream *ws;
    int offset;
};

#undef SOFTWARE_CONVENTION

/*
  A note on the "software conventions".

  Previously, I had misinterpreted the spec, and had thought that the
  spec's description of the "software convention" was wrong. Now I
  believe that this code is both correct and matches the spec, with
  SOFTWARE_CONVENTION defined or not. Thanks to William Rucklidge for
  the clarification.

  In any case, my benchmarking indicates no speed difference at all.
  Therefore, for now we will just use the normative version.

 */

static void
jbig2_arith_bytein(Jbig2ArithState *as)
{
    int new_bytes;
    byte B;

    /* invariant: as->next_word_bytes > 0 */

    /* Figure G.3 */
    B = (byte)((as->next_word >> 24) & 0xFF);
    if (B == 0xFF) {
        byte B1;

        if (as->next_word_bytes == 1) {
            Jbig2WordStream *ws = as->ws;

            new_bytes = ws->get_next_word(ws, as->offset, &as->next_word);
            as->next_word_bytes = new_bytes;
            as->offset += new_bytes;

            B1 = (byte)((as->next_word >> 24) & 0xFF);
            if (B1 > 0x8F) {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (aa)\n", B);
#endif
#ifndef SOFTWARE_CONVENTION
                as->C += 0xFF00;
#endif
                as->CT = 8;
                as->next_word = 0xFF000000 | (as->next_word >> 8);
                as->next_word_bytes = 4;
                as->offset--;
            } else {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (a)\n", B);
#endif
#ifdef SOFTWARE_CONVENTION
                as->C += 0xFE00 - (B1 << 9);
#else
                as->C += B1 << 9;
#endif
                as->CT = 7;
            }
        } else {
            B1 = (byte)((as->next_word >> 16) & 0xFF);
            if (B1 > 0x8F) {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (ba)\n", B);
#endif
#ifndef SOFTWARE_CONVENTION
                as->C += 0xFF00;
#endif
                as->CT = 8;
            } else {
                as->next_word_bytes--;
                as->next_word <<= 8;
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (b)\n", B);
#endif

#ifdef SOFTWARE_CONVENTION
                as->C += 0xFE00 - (B1 << 9);
#else
                as->C += (B1 << 9);
#endif
                as->CT = 7;
            }
        }
    } else {
#ifdef JBIG2_DEBUG_ARITH
        fprintf(stderr, "read %02x\n", B);
#endif
        as->CT = 8;
        as->next_word <<= 8;
        as->next_word_bytes--;
        if (as->next_word_bytes == 0) {
            Jbig2WordStream *ws = as->ws;

            new_bytes = ws->get_next_word(ws, as->offset, &as->next_word);
            as->offset += new_bytes;
            as->next_word_bytes = new_bytes;
        }
        B = (byte)((as->next_word >> 24) & 0xFF);
#ifdef SOFTWARE_CONVENTION
        as->C += 0xFF00 - (B << 8);
#else
        as->C += (B << 8);
#endif
    }
}

/** Allocate and initialize a new arithmetic coding state
 *  the returned pointer can simply be freed; this does
 *  not affect the associated Jbig2WordStream.
 */
Jbig2ArithState *
jbig2_arith_new(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
    Jbig2ArithState *result;
    int new_bytes;

    result = jbig2_new(ctx, Jbig2ArithState, 1);
    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate arithmetic coding state");
        return NULL;
    }

    result->ws = ws;

    new_bytes = ws->get_next_word(ws, 0, &result->next_word);
    result->next_word_bytes = new_bytes;
    result->offset = new_bytes;

    /* Figure E.20 */
#ifdef SOFTWARE_CONVENTION
    result->C = (~(result->next_word >> 8)) & 0xFF0000;
#else
    result->C = (result->next_word >> 8) & 0xFF0000;
#endif

    jbig2_arith_bytein(result);
    result->C <<= 7;
    result->CT -= 7;
    result->A = 0x8000;

    return result;
}

#define MAX_QE_ARRAY_SIZE 47

/* could put bit fields in to minimize memory usage */
typedef struct {
    unsigned short Qe;
    byte mps_xor;               /* mps_xor = index ^ NMPS */
    byte lps_xor;               /* lps_xor = index ^ NLPS ^ (SWITCH << 7) */
} Jbig2ArithQe;

const Jbig2ArithQe jbig2_arith_Qe[MAX_QE_ARRAY_SIZE] = {
    {0x5601, 1 ^ 0, 1 ^ 0 ^ 0x80},
    {0x3401, 2 ^ 1, 6 ^ 1},
    {0x1801, 3 ^ 2, 9 ^ 2},
    {0x0AC1, 4 ^ 3, 12 ^ 3},
    {0x0521, 5 ^ 4, 29 ^ 4},
    {0x0221, 38 ^ 5, 33 ^ 5},
    {0x5601, 7 ^ 6, 6 ^ 6 ^ 0x80},
    {0x5401, 8 ^ 7, 14 ^ 7},
    {0x4801, 9 ^ 8, 14 ^ 8},
    {0x3801, 10 ^ 9, 14 ^ 9},
    {0x3001, 11 ^ 10, 17 ^ 10},
    {0x2401, 12 ^ 11, 18 ^ 11},
    {0x1C01, 13 ^ 12, 20 ^ 12},
    {0x1601, 29 ^ 13, 21 ^ 13},
    {0x5601, 15 ^ 14, 14 ^ 14 ^ 0x80},
    {0x5401, 16 ^ 15, 14 ^ 15},
    {0x5101, 17 ^ 16, 15 ^ 16},
    {0x4801, 18 ^ 17, 16 ^ 17},
    {0x3801, 19 ^ 18, 17 ^ 18},
    {0x3401, 20 ^ 19, 18 ^ 19},
    {0x3001, 21 ^ 20, 19 ^ 20},
    {0x2801, 22 ^ 21, 19 ^ 21},
    {0x2401, 23 ^ 22, 20 ^ 22},
    {0x2201, 24 ^ 23, 21 ^ 23},
    {0x1C01, 25 ^ 24, 22 ^ 24},
    {0x1801, 26 ^ 25, 23 ^ 25},
    {0x1601, 27 ^ 26, 24 ^ 26},
    {0x1401, 28 ^ 27, 25 ^ 27},
    {0x1201, 29 ^ 28, 26 ^ 28},
    {0x1101, 30 ^ 29, 27 ^ 29},
    {0x0AC1, 31 ^ 30, 28 ^ 30},
    {0x09C1, 32 ^ 31, 29 ^ 31},
    {0x08A1, 33 ^ 32, 30 ^ 32},
    {0x0521, 34 ^ 33, 31 ^ 33},
    {0x0441, 35 ^ 34, 32 ^ 34},
    {0x02A1, 36 ^ 35, 33 ^ 35},
    {0x0221, 37 ^ 36, 34 ^ 36},
    {0x0141, 38 ^ 37, 35 ^ 37},
    {0x0111, 39 ^ 38, 36 ^ 38},
    {0x0085, 40 ^ 39, 37 ^ 39},
    {0x0049, 41 ^ 40, 38 ^ 40},
    {0x0025, 42 ^ 41, 39 ^ 41},
    {0x0015, 43 ^ 42, 40 ^ 42},
    {0x0009, 44 ^ 43, 41 ^ 43},
    {0x0005, 45 ^ 44, 42 ^ 44},
    {0x0001, 45 ^ 45, 43 ^ 45},
    {0x5601, 46 ^ 46, 46 ^ 46}
};

static void
jbig2_arith_renormd(Jbig2ArithState *as)
{
    /* Figure E.18 */
    do {
        if (as->CT == 0)
            jbig2_arith_bytein(as);
        as->A <<= 1;
        as->C <<= 1;
        as->CT--;
    } while ((as->A & 0x8000) == 0);
}

bool
jbig2_arith_decode(Jbig2ArithState *as, Jbig2ArithCx *pcx, int *code)
{
    Jbig2ArithCx cx = *pcx;
    const Jbig2ArithQe *pqe;
    unsigned int index = cx & 0x7f;
    bool D;

    if (index >= MAX_QE_ARRAY_SIZE) {
        *code = -1;
        return 0;
    } else {
        pqe = &jbig2_arith_Qe[index];
    }

    /* Figure E.15 */
    as->A -= pqe->Qe;
    if (
#ifdef SOFTWARE_CONVENTION
        /* Note: I do not think this is correct. See above. */
        (as->C >> 16) < as->A
#else
        !((as->C >> 16) < pqe->Qe)
#endif
    ) {
#ifndef SOFTWARE_CONVENTION
        as->C -= pqe->Qe << 16;
#endif
        if ((as->A & 0x8000) == 0) {
            /* MPS_EXCHANGE, Figure E.16 */
            if (as->A < pqe->Qe) {
                D = 1 - (cx >> 7);
                *pcx ^= pqe->lps_xor;
            } else {
                D = cx >> 7;
                *pcx ^= pqe->mps_xor;
            }
            jbig2_arith_renormd(as);
            *code = 0;
            return D;
        } else {
            *code = 0;
            return cx >> 7;
        }
    } else {
#ifdef SOFTWARE_CONVENTION
        as->C -= (as->A) << 16;
#endif
        /* LPS_EXCHANGE, Figure E.17 */
        if (as->A < pqe->Qe) {
            as->A = pqe->Qe;
            D = cx >> 7;
            *pcx ^= pqe->mps_xor;
        } else {
            as->A = pqe->Qe;
            D = 1 - (cx >> 7);
            *pcx ^= pqe->lps_xor;
        }
        jbig2_arith_renormd(as);
        *code = 0;
        return D;
    }
}

#ifdef TEST

static const byte test_stream[] = {
    0x84, 0xC7, 0x3B, 0xFC, 0xE1, 0xA1, 0x43, 0x04, 0x02, 0x20, 0x00, 0x00,
    0x41, 0x0D, 0xBB, 0x86, 0xF4, 0x31, 0x7F, 0xFF, 0x88, 0xFF, 0x37, 0x47,
    0x1A, 0xDB, 0x6A, 0xDF, 0xFF, 0xAC,
    0x00, 0x00
};

#if defined(JBIG2_DEBUG) || defined(JBIG2_DEBUG_ARITH)
static void
jbig2_arith_trace(Jbig2ArithState *as, Jbig2ArithCx cx)
{
    fprintf(stderr, "I = %2d, MPS = %d, A = %04x, CT = %2d, C = %08x\n", cx & 0x7f, cx >> 7, as->A, as->CT, as->C);
}
#endif

static int
test_get_word(Jbig2WordStream *self, size_t offset, uint32_t *word)
{
    uint32_t val = 0;
    int ret = 0;

    if (self == NULL || word == NULL)
        return -1;
    if (offset >= sizeof (test_stream))
        return 0;

    if (offset < sizeof(test_stream)) {
        val |= test_stream[offset] << 24;
        ret++;
    }
    if (offset + 1 < sizeof(test_stream)) {
        val |= test_stream[offset + 1] << 16;
        ret++;
    }
    if (offset + 2 < sizeof(test_stream)) {
        val |= test_stream[offset + 2] << 8;
        ret++;
    }
    if (offset + 3 < sizeof(test_stream)) {
        val |= test_stream[offset + 3];
        ret++;
    }
    *word = val;
    return ret;
}

int
main(int argc, char **argv)
{
    Jbig2Ctx *ctx;
    Jbig2WordStream ws;
    Jbig2ArithState *as;
    int i;
    Jbig2ArithCx cx = 0;
    int code;

    ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);

    ws.get_next_word = test_get_word;
    as = jbig2_arith_new(ctx, &ws);
#ifdef JBIG2_DEBUG_ARITH
    jbig2_arith_trace(as, cx);
#endif

    for (i = 0; i < 256; i++) {
#ifdef JBIG2_DEBUG_ARITH
        bool D =
#else
        (void)
#endif
            jbig2_arith_decode(as, &cx, &code);

#ifdef JBIG2_DEBUG_ARITH
        fprintf(stderr, "%3d: D = %d, ", i, D);
        jbig2_arith_trace(as, cx);
#endif
    }

    jbig2_free(ctx->allocator, as);

    jbig2_ctx_free(ctx);

    return 0;
}
#endif
