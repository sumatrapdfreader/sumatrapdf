/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
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
    uint32_t A;

    int CT;

    uint32_t next_word;
    size_t next_word_bytes;
    int err;

    Jbig2WordStream *ws;
    size_t offset;
};

/*
  Previous versions of this code had a #define to allow
  us to choose between using the revised arithmetic decoding
  specified in the 'Software Convention' section of the spec.
  Back to back tests showed that the 'Software Convention'
  version was indeed slightly faster. We therefore enable it
  by default. We also strip the option out, because a) it
  makes the code harder to read, and b) such things are an
  invitation to bitrot.
*/

static int
jbig2_arith_bytein(Jbig2Ctx *ctx, Jbig2ArithState *as)
{
    byte B;

    /* Treat both errors and reading beyond end of stream as an error. */
    if (as->err != 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read from underlying stream during arithmetic decoding");
        return -1;
    }
    if (as->next_word_bytes == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read beyond end of underlying stream during arithmetic decoding");
        return -1;
    }

    /* At this point there is at least one byte in as->next_word. */

    /* This code confused me no end when I first read it, so a quick note
     * to save others (and future me's) from being similarly confused.
     * 'next_word' does indeed contain 'next_word_bytes' of valid data
     * (always starting at the most significant byte). The confusing
     * thing is that the first byte has always already been read.
     * i.e. it serves only as an indication that the last byte we read
     * was FF or not.
     *
     * The jbig2 bytestream uses FF bytes, followed by a byte > 0x8F as
     * marker bytes. These never occur in normal streams of arithmetic
     * encoding, so meeting one terminates the stream (with an infinite
     * series of 1 bits).
     *
     * If we meet an FF byte, we return it as normal. We just 'remember'
     * that fact for the next byte we read.
     */

    /* Figure F.3 */
    B = (byte)((as->next_word >> 24) & 0xFF);
    if (B == 0xFF) {
        byte B1;

        /* next_word_bytes can only be == 1 here, but let's be defensive. */
        if (as->next_word_bytes <= 1) {
            int ret = as->ws->get_next_word(ctx, as->ws, as->offset, &as->next_word);
            if (ret < 0) {
                as->err = 1;
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to check for marker code due to failure in underlying stream during arithmetic decoding");
            }
            as->next_word_bytes = (size_t) ret;

            if (as->next_word_bytes == 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read end of possible terminating marker code, assuming terminating marker code");
                as->next_word = 0xFF900000;
                as->next_word_bytes = 2;
                as->C += 0xFF00;
                as->CT = 8;
                return 0;
            }

            as->offset += as->next_word_bytes;

            B1 = (byte)((as->next_word >> 24) & 0xFF);
            if (B1 > 0x8F) {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (aa)\n", B);
#endif
                as->CT = 8;
                as->next_word = 0xFF000000 | (as->next_word >> 8);
                as->next_word_bytes = 2;
                as->offset--;
            } else {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (a)\n", B);
#endif
                as->C += 0xFE00 - (B1 << 9);
                as->CT = 7;
            }
        } else {
            B1 = (byte)((as->next_word >> 16) & 0xFF);
            if (B1 > 0x8F) {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (ba)\n", B);
#endif
                as->CT = 8;
            } else {
                as->next_word_bytes--;
                as->next_word <<= 8;
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (b)\n", B);
#endif

                as->C += 0xFE00 - (B1 << 9);
                as->CT = 7;
            }
        }
    } else {
#ifdef JBIG2_DEBUG_ARITH
        fprintf(stderr, "read %02x\n", B);
#endif
        as->next_word <<= 8;
        as->next_word_bytes--;

        if (as->next_word_bytes == 0) {
            int ret = as->ws->get_next_word(ctx, as->ws, as->offset, &as->next_word);
            if (ret < 0) {
                as->err = 1;
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read from underlying stream during arithmetic decoding");
            }
            as->next_word_bytes = (size_t) ret;

            if (as->next_word_bytes == 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to find terminating marker code before end of underlying stream, assuming terminating marker code");
                as->next_word = 0xFF900000;
                as->next_word_bytes = 2;
                as->C += 0xFF00;
                as->CT = 8;
                return 0;
            }

            as->offset += as->next_word_bytes;
        }

        B = (byte)((as->next_word >> 24) & 0xFF);
        as->C += 0xFF00 - (B << 8);
        as->CT = 8;
    }

    return 0;
}

/** Allocate and initialize a new arithmetic coding state
 *  the returned pointer can simply be freed; this does
 *  not affect the associated Jbig2WordStream.
 */
Jbig2ArithState *
jbig2_arith_new(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
    Jbig2ArithState *result;
    int ret;

    result = jbig2_new(ctx, Jbig2ArithState, 1);
    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate arithmetic coding state");
        return NULL;
    }

    result->err = 0;
    result->ws = ws;
    result->offset = 0;

    ret = result->ws->get_next_word(ctx, result->ws, result->offset, &result->next_word);
    if (ret < 0) {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to initialize underlying stream of arithmetic decoder");
        return NULL;
    }

    result->next_word_bytes = (size_t) ret;
    if (result->next_word_bytes == 0) {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read first byte from underlying stream when initializing arithmetic decoder");
        return NULL;
    }

    result->offset += result->next_word_bytes;

    /* Figure F.1 */
    result->C = (~(result->next_word >> 8)) & 0xFF0000;

    /* Figure E.20 (2) */
    if (jbig2_arith_bytein(ctx, result) < 0) {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read second byte from underlying stream when initializing arithmetic decoder");
        return NULL;
    }

    /* Figure E.20 (3) */
    result->C <<= 7;
    result->CT -= 7;
    result->A = 0x8000;

    return result;
}

#define MAX_QE_ARRAY_SIZE 47

/* could put bit fields in to minimize memory usage */
typedef struct {
    uint16_t Qe;
    byte mps_xor;               /* mps_xor = index ^ NMPS */
    byte lps_xor;               /* lps_xor = index ^ NLPS ^ (SWITCH << 7) */
} Jbig2ArithQe;

#define MPS(index, nmps) ((index) ^ (nmps))
#define LPS(index, nlps, swtch) ((index) ^ (nlps) ^ ((swtch) << 7))

static const Jbig2ArithQe jbig2_arith_Qe[MAX_QE_ARRAY_SIZE] = {
    {0x5601, MPS(0, 1), LPS(0, 1, 1)},
    {0x3401, MPS(1, 2), LPS(1, 6, 0)},
    {0x1801, MPS(2, 3), LPS(2, 9, 0)},
    {0x0AC1, MPS(3, 4), LPS(3, 12, 0)},
    {0x0521, MPS(4, 5), LPS(4, 29, 0)},
    {0x0221, MPS(5, 38), LPS(5, 33, 0)},
    {0x5601, MPS(6, 7), LPS(6, 6, 1)},
    {0x5401, MPS(7, 8), LPS(7, 14, 0)},
    {0x4801, MPS(8, 9), LPS(8, 14, 0)},
    {0x3801, MPS(9, 10), LPS(9, 14, 0)},
    {0x3001, MPS(10, 11), LPS(10, 17, 0)},
    {0x2401, MPS(11, 12), LPS(11, 18, 0)},
    {0x1C01, MPS(12, 13), LPS(12, 20, 0)},
    {0x1601, MPS(13, 29), LPS(13, 21, 0)},
    {0x5601, MPS(14, 15), LPS(14, 14, 1)},
    {0x5401, MPS(15, 16), LPS(15, 14, 0)},
    {0x5101, MPS(16, 17), LPS(16, 15, 0)},
    {0x4801, MPS(17, 18), LPS(17, 16, 0)},
    {0x3801, MPS(18, 19), LPS(18, 17, 0)},
    {0x3401, MPS(19, 20), LPS(19, 18, 0)},
    {0x3001, MPS(20, 21), LPS(20, 19, 0)},
    {0x2801, MPS(21, 22), LPS(21, 19, 0)},
    {0x2401, MPS(22, 23), LPS(22, 20, 0)},
    {0x2201, MPS(23, 24), LPS(23, 21, 0)},
    {0x1C01, MPS(24, 25), LPS(24, 22, 0)},
    {0x1801, MPS(25, 26), LPS(25, 23, 0)},
    {0x1601, MPS(26, 27), LPS(26, 24, 0)},
    {0x1401, MPS(27, 28), LPS(27, 25, 0)},
    {0x1201, MPS(28, 29), LPS(28, 26, 0)},
    {0x1101, MPS(29, 30), LPS(29, 27, 0)},
    {0x0AC1, MPS(30, 31), LPS(30, 28, 0)},
    {0x09C1, MPS(31, 32), LPS(31, 29, 0)},
    {0x08A1, MPS(32, 33), LPS(32, 30, 0)},
    {0x0521, MPS(33, 34), LPS(33, 31, 0)},
    {0x0441, MPS(34, 35), LPS(34, 32, 0)},
    {0x02A1, MPS(35, 36), LPS(35, 33, 0)},
    {0x0221, MPS(36, 37), LPS(36, 34, 0)},
    {0x0141, MPS(37, 38), LPS(37, 35, 0)},
    {0x0111, MPS(38, 39), LPS(38, 36, 0)},
    {0x0085, MPS(39, 40), LPS(39, 37, 0)},
    {0x0049, MPS(40, 41), LPS(40, 38, 0)},
    {0x0025, MPS(41, 42), LPS(41, 39, 0)},
    {0x0015, MPS(42, 43), LPS(42, 40, 0)},
    {0x0009, MPS(43, 44), LPS(43, 41, 0)},
    {0x0005, MPS(44, 45), LPS(44, 42, 0)},
    {0x0001, MPS(45, 45), LPS(45, 43, 0)},
    {0x5601, MPS(46, 46), LPS(46, 46, 0)}
};

static int
jbig2_arith_renormd(Jbig2Ctx *ctx, Jbig2ArithState *as)
{
    /* Figure E.18 */
    do {
        if (as->CT == 0 && jbig2_arith_bytein(ctx, as) < 0) {
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read byte from compressed data stream");
        }
        as->A <<= 1;
        as->C <<= 1;
        as->CT--;
    } while ((as->A & 0x8000) == 0);

    return 0;
}

int
jbig2_arith_decode(Jbig2Ctx *ctx, Jbig2ArithState *as, Jbig2ArithCx *pcx)
{
    Jbig2ArithCx cx = *pcx;
    const Jbig2ArithQe *pqe;
    unsigned int index = cx & 0x7f;
    bool D;

    if (index >= MAX_QE_ARRAY_SIZE) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to determine probability estimate because index out of range");
    }

    pqe = &jbig2_arith_Qe[index];

    /* Figure F.2 */
    as->A -= pqe->Qe;
    if ((as->C >> 16) < as->A) {
        if ((as->A & 0x8000) == 0) {
            /* MPS_EXCHANGE, Figure E.16 */
            if (as->A < pqe->Qe) {
                D = 1 - (cx >> 7);
                *pcx ^= pqe->lps_xor;
            } else {
                D = cx >> 7;
                *pcx ^= pqe->mps_xor;
            }
            if (jbig2_arith_renormd(ctx, as) < 0) {
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to renormalize decoder");
            }

            return D;
        } else {
            return cx >> 7;
        }
    } else {
        as->C -= (as->A) << 16;
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
        if (jbig2_arith_renormd(ctx, as) < 0) {
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to renormalize decoder");
        }

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
test_get_word(Jbig2Ctx *ctx, Jbig2WordStream *self, size_t offset, uint32_t *word)
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

    ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);

    ws.get_next_word = test_get_word;
    as = jbig2_arith_new(ctx, &ws);
#ifdef JBIG2_DEBUG_ARITH
    jbig2_arith_trace(as, cx);
#endif

    for (i = 0; i < 256; i++) {
#ifdef JBIG2_DEBUG_ARITH
        int D =
#else
        (void)
#endif
            jbig2_arith_decode(ctx, as, &cx);

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
