/*  tif_lzw.h - LZW compression for TIFF */
/*
    libzint - the open source barcode library
    Copyright (C) 2021-2024 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef Z_TIF_LZW_H
#define Z_TIF_LZW_H

#ifdef  __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * Adapted from TIFF Library 4.2.0 libtiff/tif_lzw.c */
/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */
/*
 * TIFF Library.
 * Rev 5.0 Lempel-Ziv & Welch Compression Support
 *
 * This code is derived from the compress program whose code is
 * derived from software contributed to Berkeley by James A. Woods,
 * derived from original work by Spencer Thomas and Joseph Orost.
 *
 * The original Berkeley copyright notice appears below in its entirety.
 */
/*
 * Copyright (c) 1985, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James A. Woods, derived from original work by Spencer Thomas
 * and Joseph Orost.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define MAXCODE(n)  ((1L << (n)) - 1)

/*
 * The TIFF spec specifies that encoded bit
 * strings range from 9 to 12 bits.
 */
#define BITS_MIN        9               /* start with 9 bits */
#define BITS_MAX        12              /* max of 12 bit strings */
/* predefined codes */
#define CODE_CLEAR      256             /* code to clear string table */
#define CODE_EOI        257             /* end-of-information code */
#define CODE_FIRST      258             /* first free code entry */
#define CODE_MAX        MAXCODE(BITS_MAX)
#define HSIZE           9001L           /* 91% occupancy */
#define HSHIFT          (13 - 8)

/*
 * Encoding-specific state.
 */
typedef uint16_t tif_lzw_hcode; /* codes fit in 16 bits */
typedef struct {
    long    hash;
    tif_lzw_hcode code;
} tif_lzw_hash;

#define CHECK_GAP   10000       /* ratio check interval */

/*
 * State block.
 */
typedef struct {
    tif_lzw_hash *enc_hashtab;  /* kept separate for small machines */
} tif_lzw_state;

/*
 * LZW Encoding.
 */

/*
 * Reset encoding hash table.
 */
static void tif_lzw_cl_hash(tif_lzw_state *sp) {
    register tif_lzw_hash *hp = &sp->enc_hashtab[HSIZE - 1];
    register long i = HSIZE - 8;

    do {
        i -= 8;
        hp[-7].hash = -1;
        hp[-6].hash = -1;
        hp[-5].hash = -1;
        hp[-4].hash = -1;
        hp[-3].hash = -1;
        hp[-2].hash = -1;
        hp[-1].hash = -1;
        hp[ 0].hash = -1;
        hp -= 8;
    } while (i >= 0);

    for (i += 8; i > 0; i--, hp--) {
        hp->hash = -1;
    }
}

#define CALCRATIO(sp, rat) { \
    if (incount > 0x007fffff) { /* NB: shift will overflow */ \
        rat = outcount >> 8; \
        rat = (rat == 0 ? 0x7fffffff : incount / rat); \
    } else \
        rat = (incount << 8) / outcount; \
}

/* Explicit 0xff masking to make icc -check=conversions happy */
#define PutNextCode(op_fmp, c) { \
    nextdata = (nextdata << nbits) | c; \
    nextbits += nbits; \
    fm_putc((nextdata >> (nextbits - 8)) & 0xff, op_fmp); \
    nextbits -= 8; \
    if (nextbits >= 8) { \
        fm_putc((nextdata >> (nextbits - 8)) & 0xff, op_fmp); \
        nextbits -= 8; \
    } \
    outcount += nbits; \
}

/*
 * Encode a chunk of pixels.
 *
 * Uses an open addressing double hashing (no chaining) on the
 * prefix code/next character combination.  We do a variant of
 * Knuth's algorithm D (vol. 3, sec. 6.4) along with G. Knott's
 * relatively-prime secondary probe.  Here, the modular division
 * first probe is gives way to a faster exclusive-or manipulation.
 * Also do block compression with an adaptive reset, whereby the
 * code table is cleared when the compression ratio decreases,
 * but after the table fills.  The variable-length output codes
 * are re-sized at this point, and a CODE_CLEAR is generated
 * for the decoder.
 */
static int tif_lzw_encode(tif_lzw_state *sp, struct filemem *op_fmp, const unsigned char *bp, int cc) {
    register long fcode;
    register tif_lzw_hash *hp;
    register int h, c;
    tif_lzw_hcode ent;
    long disp;

    int nbits;              /* # of bits/code */
    int maxcode;            /* maximum code for nbits */
    int free_ent;           /* next free entry in hash table */
    unsigned long nextdata; /* next bits of i/o */
    long nextbits;          /* # of valid bits in nextdata */
    long checkpoint;        /* point at which to clear table */
    long ratio;             /* current compression ratio */
    long incount;           /* (input) data bytes encoded */
    long outcount;          /* encoded (output) bytes */

    /*
     * Reset encoding state at the start of a strip.
     */
    if (sp->enc_hashtab == NULL) {
        sp->enc_hashtab = (tif_lzw_hash *) malloc(HSIZE * sizeof(tif_lzw_hash));
        if (sp->enc_hashtab == NULL) {
            return 0;
        }
    }

    tif_lzw_cl_hash(sp); /* clear hash table */

    nbits = BITS_MIN;
    maxcode = MAXCODE(BITS_MIN);
    free_ent = CODE_FIRST;
    nextdata = 0;
    nextbits = 0;
    checkpoint = CHECK_GAP;
    ratio = 0;
    incount = 0;
    outcount = 0;

    ent = (tif_lzw_hcode) -1;

    if (cc > 0) {
        PutNextCode(op_fmp, CODE_CLEAR);
        ent = *bp++; cc--; incount++;
    }
    while (cc > 0) {
        c = *bp++; cc--; incount++;
        fcode = ((long)c << BITS_MAX) + ent;
        h = (c << HSHIFT) ^ ent; /* xor hashing */
#ifdef _WINDOWS
        /*
         * Check hash index for an overflow.
         */
        if (h >= HSIZE) {
            h -= HSIZE;
        }
#endif
        hp = &sp->enc_hashtab[h];
        if (hp->hash == fcode) {
            ent = hp->code;
            continue;
        }
        if (hp->hash >= 0) {
            /*
             * Primary hash failed, check secondary hash.
             */
            disp = HSIZE - h;
            if (h == 0) {
                disp = 1;
            }
            do {
                /*
                 * Avoid pointer arithmetic because of
                 * wraparound problems with segments.
                 */
                if ((h -= disp) < 0) {
                    h += HSIZE;
                }
                hp = &sp->enc_hashtab[h];
                if (hp->hash == fcode) {
                    ent = hp->code;
                    goto hit;
                }
            } while (hp->hash >= 0);
        }
        /*
         * New entry, emit code and add to table.
         */
        PutNextCode(op_fmp, ent);
        ent = (tif_lzw_hcode) c;
        hp->code = (tif_lzw_hcode) (free_ent++);
        hp->hash = fcode;
        if (free_ent == CODE_MAX - 1) {
            /* table is full, emit clear code and reset */
            tif_lzw_cl_hash(sp);
            ratio = 0;
            incount = 0;
            outcount = 0;
            free_ent = CODE_FIRST;
            PutNextCode(op_fmp, CODE_CLEAR);
            nbits = BITS_MIN;
            maxcode = MAXCODE(BITS_MIN);
        } else {
            /*
             * If the next entry is going to be too big for
             * the code size, then increase it, if possible.
             */
            if (free_ent > maxcode) {
                nbits++;
                assert(nbits <= BITS_MAX);
                maxcode = (int) MAXCODE(nbits);
            } else if (incount >= checkpoint) {
                long rat;
                /*
                 * Check compression ratio and, if things seem
                 * to be slipping, clear the hash table and
                 * reset state.  The compression ratio is a
                 * 24+8-bit fractional number.
                 */
                checkpoint = incount + CHECK_GAP;
                CALCRATIO(sp, rat);
                if (rat <= ratio) {
                    tif_lzw_cl_hash(sp);
                    ratio = 0;
                    incount = 0;
                    outcount = 0;
                    free_ent = CODE_FIRST;
                    PutNextCode(op_fmp, CODE_CLEAR);
                    nbits = BITS_MIN;
                    maxcode = MAXCODE(BITS_MIN);
                } else {
                    ratio = rat;
                }
            }
        }
    hit:
        ;
    }

    /*
     * Finish off an encoded strip by flushing the last
     * string and tacking on an End Of Information code.
     */
    if (ent != (tif_lzw_hcode) -1) {

        PutNextCode(op_fmp, ent);
        free_ent++;

        if (free_ent == CODE_MAX - 1) {
            /* table is full, emit clear code and reset */
            outcount = 0;
            PutNextCode(op_fmp, CODE_CLEAR);
            nbits = BITS_MIN;
        } else {
            /*
            * If the next entry is going to be too big for
            * the code size, then increase it, if possible.
            */
            if (free_ent > maxcode) {
                nbits++;
                assert(nbits <= BITS_MAX);
            }
        }
    }
    PutNextCode(op_fmp, CODE_EOI);
    /* Explicit 0xff masking to make icc -check=conversions happy */
    if (nextbits > 0) {
        fm_putc((nextdata << (8 - nextbits)) & 0xff, op_fmp);
    }

    return 1;
}

static void tif_lzw_cleanup(tif_lzw_state *sp) {
    if (sp->enc_hashtab) {
        free(sp->enc_hashtab);
    }
}

static void tif_lzw_init(tif_lzw_state *sp) {
    sp->enc_hashtab = NULL;
}

#ifdef  __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_TIF_LZW_H */
