/* dmatrix.c Handles Data Matrix ECC 200 symbols */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2024 Robin Stuart <rstuart114@gmail.com>

    developed from and including some functions from:
        IEC16022 bar code generation
        Adrian Kennard, Andrews & Arnold Ltd
        with help from Cliff Hones on the RS coding

        (c) 2004 Adrian Kennard, Andrews & Arnold Ltd
        (c) 2006 Stefan Schmidt <stefan@datenfreihafen.org>

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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "common.h"
#include "reedsol.h"
#include "dmatrix.h"

/* Annex F placement algorithm low level */
static void dm_placementbit(int *array, const int NR, const int NC, int r, int c, const int p, const char b) {
    if (r < 0) {
        r += NR;
        c += 4 - ((NR + 4) % 8);
    }
    if (c < 0) {
        c += NC;
        r += 4 - ((NC + 4) % 8);
    }
    /* Necessary for DMRE (ISO/IEC 21471:2020 Annex E) */
    if (r >= NR) {
        r -= NR;
    }
    /* Check index limits */
    assert(r < NR);
    assert(c < NC);
    /* Check double-assignment */
    assert(0 == array[r * NC + c]);
    array[r * NC + c] = (p << 3) + b;
}

static void dm_placementblock(int *array, const int NR, const int NC, const int r,
        const int c, const int p) {
    dm_placementbit(array, NR, NC, r - 2, c - 2, p, 7);
    dm_placementbit(array, NR, NC, r - 2, c - 1, p, 6);
    dm_placementbit(array, NR, NC, r - 1, c - 2, p, 5);
    dm_placementbit(array, NR, NC, r - 1, c - 1, p, 4);
    dm_placementbit(array, NR, NC, r - 1, c - 0, p, 3);
    dm_placementbit(array, NR, NC, r - 0, c - 2, p, 2);
    dm_placementbit(array, NR, NC, r - 0, c - 1, p, 1);
    dm_placementbit(array, NR, NC, r - 0, c - 0, p, 0);
}

static void dm_placementcornerA(int *array, const int NR, const int NC, const int p) {
    dm_placementbit(array, NR, NC, NR - 1, 0, p, 7);
    dm_placementbit(array, NR, NC, NR - 1, 1, p, 6);
    dm_placementbit(array, NR, NC, NR - 1, 2, p, 5);
    dm_placementbit(array, NR, NC, 0, NC - 2, p, 4);
    dm_placementbit(array, NR, NC, 0, NC - 1, p, 3);
    dm_placementbit(array, NR, NC, 1, NC - 1, p, 2);
    dm_placementbit(array, NR, NC, 2, NC - 1, p, 1);
    dm_placementbit(array, NR, NC, 3, NC - 1, p, 0);
}

static void dm_placementcornerB(int *array, const int NR, const int NC, const int p) {
    dm_placementbit(array, NR, NC, NR - 3, 0, p, 7);
    dm_placementbit(array, NR, NC, NR - 2, 0, p, 6);
    dm_placementbit(array, NR, NC, NR - 1, 0, p, 5);
    dm_placementbit(array, NR, NC, 0, NC - 4, p, 4);
    dm_placementbit(array, NR, NC, 0, NC - 3, p, 3);
    dm_placementbit(array, NR, NC, 0, NC - 2, p, 2);
    dm_placementbit(array, NR, NC, 0, NC - 1, p, 1);
    dm_placementbit(array, NR, NC, 1, NC - 1, p, 0);
}

static void dm_placementcornerC(int *array, const int NR, const int NC, const int p) {
    dm_placementbit(array, NR, NC, NR - 3, 0, p, 7);
    dm_placementbit(array, NR, NC, NR - 2, 0, p, 6);
    dm_placementbit(array, NR, NC, NR - 1, 0, p, 5);
    dm_placementbit(array, NR, NC, 0, NC - 2, p, 4);
    dm_placementbit(array, NR, NC, 0, NC - 1, p, 3);
    dm_placementbit(array, NR, NC, 1, NC - 1, p, 2);
    dm_placementbit(array, NR, NC, 2, NC - 1, p, 1);
    dm_placementbit(array, NR, NC, 3, NC - 1, p, 0);
}

static void dm_placementcornerD(int *array, const int NR, const int NC, const int p) {
    dm_placementbit(array, NR, NC, NR - 1, 0, p, 7);
    dm_placementbit(array, NR, NC, NR - 1, NC - 1, p, 6);
    dm_placementbit(array, NR, NC, 0, NC - 3, p, 5);
    dm_placementbit(array, NR, NC, 0, NC - 2, p, 4);
    dm_placementbit(array, NR, NC, 0, NC - 1, p, 3);
    dm_placementbit(array, NR, NC, 1, NC - 3, p, 2);
    dm_placementbit(array, NR, NC, 1, NC - 2, p, 1);
    dm_placementbit(array, NR, NC, 1, NC - 1, p, 0);
}

/* Annex F placement algorithm main function */
static void dm_placement(int *array, const int NR, const int NC) {
    int r, c, p;
    /* start */
    p = 1;
    r = 4;
    c = 0;
    do {
        /* check corner */
        if (r == NR && !c)
            dm_placementcornerA(array, NR, NC, p++);
        if (r == NR - 2 && !c && NC % 4)
            dm_placementcornerB(array, NR, NC, p++);
        if (r == NR - 2 && !c && (NC % 8) == 4)
            dm_placementcornerC(array, NR, NC, p++);
        if (r == NR + 4 && c == 2 && !(NC % 8))
            dm_placementcornerD(array, NR, NC, p++);
        /* up/right */
        do {
            if (r < NR && c >= 0 && !array[r * NC + c])
                dm_placementblock(array, NR, NC, r, c, p++);
            r -= 2;
            c += 2;
        } while (r >= 0 && c < NC);
        r++;
        c += 3;
        /* down/left */
        do {
            if (r >= 0 && c < NC && !array[r * NC + c])
                dm_placementblock(array, NR, NC, r, c, p++);
            r += 2;
            c -= 2;
        } while (r < NR && c >= 0);
        r += 3;
        c++;
    } while (r < NR || c < NC);
    /* unfilled corner */
    if (!array[NR * NC - 1])
        array[NR * NC - 1] = array[NR * NC - NC - 2] = 1;
}

/* calculate and append ecc code, and if necessary interleave */
static void dm_ecc(unsigned char *binary, const int bytes, const int datablock, const int rsblock, const int skew) {
    int blocks = (bytes + 2) / datablock, b;
    int rsblocks = rsblock * blocks;
    int n;
    rs_t rs;

    rs_init_gf(&rs, 0x12d);
    rs_init_code(&rs, rsblock, 1);
    for (b = 0; b < blocks; b++) {
        unsigned char buf[256], ecc[256];
        int p = 0;
        for (n = b; n < bytes; n += blocks)
            buf[p++] = binary[n];
        rs_encode(&rs, p, buf, ecc);
        if (skew) {
            /* Rotate ecc data to make 144x144 size symbols acceptable */
            /* See http://groups.google.com/group/postscriptbarcode/msg/5ae8fda7757477da
               or https://github.com/nu-book/zxing-cpp/issues/259 */
            for (n = b, p = 0; n < rsblocks; n += blocks, p++) {
                if (b < 8) {
                    binary[bytes + n + 2] = ecc[p];
                } else {
                    binary[bytes + n - 8] = ecc[p];
                }
            }
        } else {
            for (n = b, p = 0; n < rsblocks; n += blocks, p++) {
                binary[bytes + n] = ecc[p];
            }
        }
    }
}

/* Is basic (non-shifted) C40? */
static int dm_isc40(const unsigned char input) {
    if (input <= '9') {
        return input >= '0' || input == ' ';
    }
    return z_isupper(input);
}

/* Is basic (non-shifted) TEXT? */
static int dm_istext(const unsigned char input) {
    if (input <= '9') {
        return input >= '0' || input == ' ';
    }
    return z_islower(input);
}

/* Is basic (non-shifted) C40/TEXT? */
static int dm_isc40text(const int current_mode, const unsigned char input) {
    return current_mode == DM_C40 ? dm_isc40(input) : dm_istext(input);
}

/* Return true (1) if a character is valid in X12 set */
static int dm_isX12(const unsigned char input) {
    return dm_isc40(input) || input == 13 || input == '*' || input == '>';
}

/* Return true (1) if a character is valid in EDIFACT set */
static int dm_isedifact(const unsigned char input) {
    return input >= ' ' && input <= '^';
}

/* Does Annex J section (r)(6)(ii)(I) apply? */
static int dm_substep_r_6_2_1(const unsigned char source[], const int length, const int sp) {
    /* Annex J section (r)(6)(ii)(I)
       "If one of the three X12 terminator/separator characters first
        occurs in the yet to be processed data before a non-X12 character..."
     */
    int i;

    for (i = sp; i < length && dm_isX12(source[i]); i++) {
        if (source[i] == 13 || source[i] == '*' || source[i] == '>') {
            return 1;
        }
    }

    return 0;
}

/* Count number of TEXT characters around `sp` between `position` and `length`
   - helper to avoid exiting from Base 256 too early if have series of TEXT characters */
static int dm_text_sp_cnt(const unsigned char source[], const int position, const int length, const int sp) {
    int i;
    int cnt = 0;

    /* Count from `sp` forward */
    for (i = sp; i < length && dm_istext(source[i]); i++, cnt++);
    /* Count backwards from `sp` */
    for (i = sp - 1; i >= position && dm_istext(source[i]); i--, cnt++);

    return cnt;
}

/* Character counts are multiplied by this, so as to be whole integer divisible by 2, 3 and 4 */
#define DM_MULT             12

#define DM_MULT_1_DIV_2     6
#define DM_MULT_2_DIV_3     8
#define DM_MULT_3_DIV_4     9
#define DM_MULT_1           12
#define DM_MULT_4_DIV_3     16
#define DM_MULT_2           24
#define DM_MULT_8_DIV_3     32
#define DM_MULT_3           26
#define DM_MULT_13_DIV_4    39
#define DM_MULT_10_DIV_3    40
#define DM_MULT_4           48
#define DM_MULT_17_DIV_4    51
#define DM_MULT_13_DIV_3    52

#define DM_MULT_MINUS_1     11
#define DM_MULT_CEIL(n)     ((((n) + DM_MULT_MINUS_1) / DM_MULT) * DM_MULT)

/* 'look ahead test' from Annex J */
static int dm_look_ahead_test(const unsigned char source[], const int length, const int position,
            const int current_mode, const int mode_arg, const int gs1, const int debug_print) {
    int ascii_count, c40_count, text_count, x12_count, edf_count, b256_count;
    int ascii_rnded, c40_rnded, text_rnded, x12_rnded, edf_rnded, b256_rnded;
    int cnt_1;
    int sp;

    /* step (j) */
    if (current_mode == DM_ASCII || current_mode == DM_BASE256) { /* Adjusted to use for DM_BASE256 also */
        ascii_count = 0;
        c40_count = DM_MULT_1;
        text_count = DM_MULT_1;
        x12_count = DM_MULT_1;
        edf_count = DM_MULT_1;
        b256_count = DM_MULT_2; /* Adjusted from DM_MULT_5_DIV_4 (1.25) */
    } else {
        ascii_count = DM_MULT_1;
        c40_count = DM_MULT_2;
        text_count = DM_MULT_2;
        x12_count = DM_MULT_2;
        edf_count = DM_MULT_2;
        b256_count = DM_MULT_3; /* Adjusted from DM_MULT_9_DIV_4 (2.25) */
    }

    switch (current_mode) {
        case DM_C40: c40_count = 0;
            break;
        case DM_TEXT: text_count = 0;
            break;
        case DM_X12: x12_count = 0;
            break;
        case DM_EDIFACT: edf_count = 0;
            break;
        case DM_BASE256:
            b256_count = mode_arg == 249 ? DM_MULT_1 : 0; /* Adjusted to use no. of bytes written */
            break;
    }

    for (sp = position; sp < length; sp++) {
        const unsigned char c = source[sp];
        const int is_extended = c & 0x80;

        /* ascii ... step (l) */
        if (z_isdigit(c)) {
            ascii_count += DM_MULT_1_DIV_2; /* (l)(1) */
        } else {
            if (is_extended) {
                ascii_count = DM_MULT_CEIL(ascii_count) + DM_MULT_2; /* (l)(2) */
            } else {
                ascii_count = DM_MULT_CEIL(ascii_count) + DM_MULT_1; /* (l)(3) */
            }
        }

        /* c40 ... step (m) */
        if (dm_isc40(c)) {
            c40_count += DM_MULT_2_DIV_3; /* (m)(1) */
        } else {
            if (is_extended) {
                c40_count += DM_MULT_8_DIV_3; /* (m)(2) */
            } else {
                c40_count += DM_MULT_4_DIV_3; /* (m)(3) */
            }
        }

        /* text ... step (n) */
        if (dm_istext(c)) {
            text_count += DM_MULT_2_DIV_3; /* (n)(1) */
        } else {
            if (is_extended) {
                text_count += DM_MULT_8_DIV_3; /* (n)(2) */
            } else {
                text_count += DM_MULT_4_DIV_3; /* (n)(3) */
            }
        }

        /* x12 ... step (o) */
        if (dm_isX12(c)) {
            x12_count += DM_MULT_2_DIV_3; /* (o)(1) */
        } else {
            if (is_extended) {
                x12_count += DM_MULT_13_DIV_3; /* (o)(2) */
            } else {
                x12_count += DM_MULT_10_DIV_3; /* (o)(3) */
            }
        }

        /* edifact ... step (p) */
        if (dm_isedifact(c)) {
            edf_count += DM_MULT_3_DIV_4; /* (p)(1) */
        } else {
            if (is_extended) {
                edf_count += DM_MULT_17_DIV_4; /* (p)(2) */
            } else {
                edf_count += DM_MULT_13_DIV_4; /* (p)(3) */
            }
        }

        /* base 256 ... step (q) */
        if (gs1 == 1 && c == '\x1D') {
            /* FNC1 separator */
            b256_count += DM_MULT_4; /* (q)(1) */
        } else {
            b256_count += DM_MULT_1; /* (q)(2) */
        }

        if (sp >= position + 3) {
            /* At least 4 data characters processed ... step (r) */
            /* NOTE: previous behaviour was at least 5 (same as BWIPP) */

            if (debug_print) {
                printf("\n(m:%d, p:%d, sp:%d, a:%d): ascii_count %d, b256_count %d, edf_count %d, text_count %d"
                        ", x12_count %d, c40_count %d ",
                        current_mode, position, sp, mode_arg, ascii_count, b256_count, edf_count, text_count,
                        x12_count, c40_count);
            }

            cnt_1 = ascii_count + DM_MULT_1;
            /* Adjusted from <= b256_count */
            if (cnt_1 < b256_count && cnt_1 <= edf_count && cnt_1 <= text_count && cnt_1 <= x12_count
                    && cnt_1 <= c40_count) {
                if (debug_print) fputs("ASC->", stdout);
                return DM_ASCII; /* step (r)(1) */
            }
            cnt_1 = b256_count + DM_MULT_1;
            if (cnt_1 <= ascii_count || (cnt_1 < edf_count && cnt_1 < text_count && cnt_1 < x12_count
                    && cnt_1 < c40_count)) {
                if (debug_print) fputs("BAS->", stdout);
                return DM_BASE256; /* step (r)(2) */
            }
            cnt_1 = edf_count + DM_MULT_1;
            if (cnt_1 < ascii_count && cnt_1 < b256_count && cnt_1 < text_count && cnt_1 < x12_count
                    && cnt_1 < c40_count) {
                if (debug_print) fputs("EDI->", stdout);
                return DM_EDIFACT; /* step (r)(3) */
            }
            cnt_1 = text_count + DM_MULT_1;
            if (cnt_1 < ascii_count && cnt_1 < b256_count && cnt_1 < edf_count && cnt_1 < x12_count
                    && cnt_1 < c40_count) {
                /* Adjusted to avoid early exit from Base 256 if have less than break-even sequence of TEXT chars */
                if (current_mode == DM_BASE256 && position + 6 < length) {
                    if (dm_text_sp_cnt(source, position, length, sp) >= 12) {
                        if (debug_print) fputs("TEX->", stdout);
                        return DM_TEXT; /* step (r)(4) */
                    }
                } else {
                    if (debug_print) fputs("TEX->", stdout);
                    return DM_TEXT; /* step (r)(4) */
                }
            }
            cnt_1 = x12_count + DM_MULT_1;
            if (cnt_1 < ascii_count && cnt_1 < b256_count && cnt_1 < edf_count && cnt_1 < text_count
                    && cnt_1 < c40_count) {
                if (debug_print) fputs("X12->", stdout);
                return DM_X12; /* step (r)(5) */
            }
            cnt_1 = c40_count + DM_MULT_1;
            if (cnt_1 < ascii_count && cnt_1 < b256_count && cnt_1 < edf_count && cnt_1 < text_count) {
                if (c40_count < x12_count) {
                    if (debug_print) fputs("C40->", stdout);
                    return DM_C40; /* step (r)(6)(i) */
                }
                if (c40_count == x12_count) {
                    if (dm_substep_r_6_2_1(source, length, sp) == 1) {
                        if (debug_print) fputs("X12->", stdout);
                        return DM_X12; /* step (r)(6)(ii)(I) */
                    }
                    if (debug_print) fputs("C40->", stdout);
                    return DM_C40; /* step (r)(6)(ii)(II) */
                }
            }
        }
    }

    /* At the end of data ... step (k) */
    /* step (k)(1) */
    ascii_rnded = DM_MULT_CEIL(ascii_count);
    b256_rnded = DM_MULT_CEIL(b256_count);
    edf_rnded = DM_MULT_CEIL(edf_count);
    text_rnded = DM_MULT_CEIL(text_count);
    x12_rnded = DM_MULT_CEIL(x12_count);
    c40_rnded = DM_MULT_CEIL(c40_count);
    if (debug_print) {
        printf("\nEOD(m:%d, p:%d, a:%d): ascii_rnded %d, b256_rnded %d, edf_rnded %d, text_rnded %d"
                ", x12_rnded %d (%d), c40_rnded %d (%d) ",
                current_mode, position, mode_arg, ascii_rnded, b256_rnded, edf_rnded, text_rnded,
                x12_rnded, x12_count, c40_rnded, c40_count);
    }

    if (ascii_rnded <= b256_rnded && ascii_rnded <= edf_rnded && ascii_rnded <= text_rnded && ascii_rnded <= x12_rnded
            && ascii_rnded <= c40_rnded) {
        if (debug_print) fputs("ASC->", stdout);
        return DM_ASCII; /* step (k)(2) */
    }
    if (b256_rnded < ascii_rnded && b256_rnded < edf_rnded && b256_rnded < text_rnded && b256_rnded < x12_rnded
            && b256_rnded < c40_rnded) {
        if (debug_print) fputs("BAS->", stdout);
        return DM_BASE256; /* step (k)(3) */
    }
    /* Adjusted from < x12_rnded */
    if (edf_rnded < ascii_rnded && edf_rnded < b256_rnded && edf_rnded < text_rnded && edf_rnded <= x12_rnded
            && edf_rnded < c40_rnded) {
        if (debug_print) fputs("EDI->", stdout);
        return DM_EDIFACT; /* step (k)(4) */
    }
    if (text_rnded < ascii_rnded && text_rnded < b256_rnded && text_rnded < edf_rnded && text_rnded < x12_rnded
            && text_rnded < c40_rnded) {
        if (debug_print) fputs("TEX->", stdout);
        return DM_TEXT; /* step (k)(5) */
    }
    /* Adjusted from < edf_rnded */
    if (x12_rnded < ascii_rnded && x12_rnded < b256_rnded && x12_rnded <= edf_rnded && x12_rnded < text_rnded
            && x12_rnded < c40_rnded) {
        if (debug_print) fputs("X12->", stdout);
        return DM_X12; /* step (k)(6) */
    }
    if (debug_print) fputs("C40->", stdout);
    return DM_C40; /* step (k)(7) */
}

/* Copy C40/TEXT/X12 triplets from buffer to target. Returns elements left in buffer (< 3) */
static int dm_ctx_buffer_xfer(int process_buffer[8], int process_p, unsigned char target[], int *p_tp,
            const int debug_print) {
    int i, process_e;
    int tp = *p_tp;

    process_e = (process_p / 3) * 3;

    for (i = 0; i < process_e; i += 3) {
        int iv = (1600 * process_buffer[i]) + (40 * process_buffer[i + 1]) + (process_buffer[i + 2]) + 1;
        target[tp++] = (unsigned char) (iv >> 8);
        target[tp++] = (unsigned char) (iv & 0xFF);
        if (debug_print) {
            printf("[%d %d %d (%d %d)] ", process_buffer[i], process_buffer[i + 1], process_buffer[i + 2],
                target[tp - 2], target[tp - 1]);
        }
    }

    process_p -= process_e;

    if (process_p) {
        memmove(process_buffer, process_buffer + process_e, sizeof(int) * process_p);
    }

    *p_tp = tp;

    return process_p;
}

/* Copy EDIFACT quadruplets from buffer to target. Returns elements left in buffer (< 4) */
static int dm_edi_buffer_xfer(int process_buffer[8], int process_p, unsigned char target[], int *p_tp,
            const int empty, const int debug_print) {
    int i, process_e;
    int tp = *p_tp;

    process_e = (process_p / 4) * 4;

    for (i = 0; i < process_e; i += 4) {
        target[tp++] = (unsigned char) (process_buffer[i] << 2 | (process_buffer[i + 1] & 0x30) >> 4);
        target[tp++] = (unsigned char) ((process_buffer[i + 1] & 0x0f) << 4 | (process_buffer[i + 2] & 0x3c) >> 2);
        target[tp++] = (unsigned char) ((process_buffer[i + 2] & 0x03) << 6 | process_buffer[i + 3]);
        if (debug_print) {
            printf("[%d %d %d %d (%d %d %d)] ", process_buffer[i], process_buffer[i + 1], process_buffer[i + 2],
                process_buffer[i + 3], target[tp - 3], target[tp - 2], target[tp - 1]);
        }
    }

    process_p -= process_e;

    if (process_p) {
        memmove(process_buffer, process_buffer + process_e, sizeof(int) * process_p);
        if (empty) {
            if (process_p == 3) {
                target[tp++] = (unsigned char) (process_buffer[i] << 2 | (process_buffer[i + 1] & 0x30) >> 4);
                target[tp++] = (unsigned char) ((process_buffer[i + 1] & 0x0f) << 4
                                                | (process_buffer[i + 2] & 0x3c) >> 2);
                target[tp++] = (unsigned char) ((process_buffer[i + 2] & 0x03) << 6);
                if (debug_print) {
                    printf("[%d %d %d (%d %d %d)] ", process_buffer[i], process_buffer[i + 1], process_buffer[i + 2],
                            target[tp - 3], target[tp - 2], target[tp - 1]);
                }
            } else if (process_p == 2) {
                target[tp++] = (unsigned char) (process_buffer[i] << 2 | (process_buffer[i + 1] & 0x30) >> 4);
                target[tp++] = (unsigned char) ((process_buffer[i + 1] & 0x0f) << 4);
                if (debug_print) {
                    printf("[%d %d (%d %d)] ", process_buffer[i], process_buffer[i + 1], target[tp - 2],
                            target[tp - 1]);
                }
            } else {
                target[tp++] = (unsigned char) (process_buffer[i] << 2);
                if (debug_print) printf("[%d (%d)] ", process_buffer[i], target[tp - 1]);
            }
            process_p = 0;
        }
    }

    *p_tp = tp;

    return process_p;
}

/* Get index of symbol size in codewords array `dm_matrixbytes`, as specified or
   else smallest containing `minimum` codewords */
static int dm_get_symbolsize(struct zint_symbol *symbol, const int minimum) {
    int i;

    if ((symbol->option_2 >= 1) && (symbol->option_2 <= DMSIZESCOUNT)) {
        return dm_intsymbol[symbol->option_2 - 1];
    }
    if (minimum > 1304) {
        return minimum <= 1558 ? DMSIZESCOUNT - 1 : 0;
    }
    for (i = minimum >= 62 ? 23 : 0; minimum > dm_matrixbytes[i]; i++);

    if ((symbol->option_3 & 0x7F) == DM_DMRE) {
        return i;
    }
    if ((symbol->option_3 & 0x7F) == DM_SQUARE) {
        /* Skip rectangular symbols in square only mode */
        for (; dm_matrixH[i] != dm_matrixW[i]; i++);
        return i;
    }
    /* Skip DMRE symbols in no dmre mode */
    for (; dm_isDMRE[i]; i++);
    return i;
}

/* Number of codewords remaining in a particular version (may be negative) */
static int dm_codewords_remaining(struct zint_symbol *symbol, const int tp, const int process_p) {
    int symbolsize = dm_get_symbolsize(symbol, tp + process_p); /* Allow for the remaining data characters */

    return dm_matrixbytes[symbolsize] - tp;
}

/* Number of C40/TEXT elements needed to encode `input` */
static int dm_c40text_cnt(const int current_mode, const int gs1, unsigned char input) {
    int cnt;

    if (gs1 && input == '\x1D') {
        return 2;
    }
    cnt = 1;
    if (input & 0x80) {
        cnt += 2;
        input = input - 128;
    }
    if ((current_mode == DM_C40 && dm_c40_shift[input]) || (current_mode == DM_TEXT && dm_text_shift[input])) {
        cnt += 1;
    }

    return cnt;
}

/* Update Base 256 field length */
static int dm_update_b256_field_length(unsigned char target[], int tp, int b256_start) {
    int b256_count = tp - (b256_start + 1);
    if (b256_count <= 249) {
        target[b256_start] = b256_count;
    } else {
        /* Insert extra codeword */
        memmove(target + b256_start + 2, target + b256_start + 1, b256_count);
        target[b256_start] = (unsigned char) (249 + (b256_count / 250));
        target[b256_start + 1] = (unsigned char) (b256_count % 250);
        tp++;
    }

    return tp;
}

/* Switch from ASCII or Base 256 to another mode */
static int dm_switch_mode(const int next_mode, unsigned char target[], int tp, int *p_b256_start,
            const int debug_print) {
    switch (next_mode) {
        case DM_ASCII:
            if (debug_print) fputs("ASC ", stdout);
            break;
        case DM_C40: target[tp++] = 230;
            if (debug_print) fputs("C40 ", stdout);
            break;
        case DM_TEXT: target[tp++] = 239;
            if (debug_print) fputs("TEX ", stdout);
            break;
        case DM_X12: target[tp++] = 238;
            if (debug_print) fputs("X12 ", stdout);
            break;
        case DM_EDIFACT: target[tp++] = 240;
            if (debug_print) fputs("EDI ", stdout);
            break;
        case DM_BASE256: target[tp++] = 231;
            *p_b256_start = tp;
            target[tp++] = 0; /* Byte count holder (may be expanded to 2 codewords) */
            if (debug_print) fputs("BAS ", stdout);
            break;
    }

    return tp;
}

/* Minimal encoding using Dijkstra-based algorithm by Alex Geller
   Note due to the complicated end-of-data (EOD) conditions that Data Matrix has, this may not be fully minimal;
   however no counter-examples are known at present */

#define DM_NUM_MODES        6

static const char dm_smodes[DM_NUM_MODES + 1][6] = { "?", "ASCII", "C40", "TEXT", "X12", "EDF", "B256" };

/* The size of this structure could be significantly reduced using techniques pointed out by Alex Geller,
   but not done currently to avoid the processing overhead */
struct dm_edge {
    unsigned char mode;
    unsigned char endMode; /* Mode returned by `dm_getEndMode()` */
    unsigned short from; /* Position in input data, 0-based */
    unsigned short len;
    unsigned short size; /* Cumulative number of codewords */
    unsigned short bytes; /* DM_BASE256 byte count, kept to avoid runtime calc */
    unsigned short previous; /* Index into edges array */
};

/* Note 1st row of edges not used so valid previous cannot point there, i.e. won't be zero */
#define DM_PREVIOUS(edges, edge) \
    ((edge)->previous ? (edges) + (edge)->previous : NULL)

/* Determine if next 1 to 4 chars are at EOD and can be encoded as 1 or 2 ASCII codewords */
static int dm_last_ascii(const unsigned char source[], const int length, const int from) {
    if (length - from > 4 || from >= length) {
        return 0;
    }
    if (length - from == 1) {
        if (source[from] & 0x80) {
            return 0;
        }
        return 1;
    }
    if (length - from == 2) {
        if ((source[from] & 0x80) || (source[from + 1] & 0x80)) {
            return 0;
        }
        if (z_isdigit(source[from]) && z_isdigit(source[from + 1])) {
            return 1;
        }
        return 2;
    }
    if (length - from == 3) {
        if (z_isdigit(source[from]) && z_isdigit(source[from + 1]) && !(source[from + 2] & 0x80)) {
            return 2;
        }
        if (z_isdigit(source[from + 1]) && z_isdigit(source[from + 2]) && !(source[from] & 0x80)) {
            return 2;
        }
        return 0;
    }
    if (z_isdigit(source[from]) && z_isdigit(source[from + 1]) && z_isdigit(source[from + 2])
            && z_isdigit(source[from + 3])) {
        return 2;
    }
    return 0;
}

/* Treat EDIFACT edges specially, returning DM_ASCII mode if not full (i.e. encoding < 4 chars), or if
   full and at EOD where 1 or 2 ASCII chars can be encoded */
static int dm_getEndMode(struct zint_symbol *symbol, const unsigned char *source, const int length, const int mode,
            const int from, const int len, const int size) {
    if (mode == DM_EDIFACT) {
        int last_ascii;
        if (len < 4) {
            return DM_ASCII;
        }
        last_ascii = dm_last_ascii(source, length, from + len);
        if (last_ascii) { /* At EOD with remaining chars ASCII-encodable in 1 or 2 codewords */
            const int symbols_left = dm_codewords_remaining(symbol, size + last_ascii, 0);
            /* If no codewords left and 1 or 2 ASCII-encodables or 1 codeword left and 1 ASCII-encodable */
            if (symbols_left <= 2 - last_ascii) {
                return DM_ASCII;
            }
        }
    }
    return mode;
}

#if 0
#define DM_TRACE
#endif
#include "dmatrix_trace.h"

/* Return number of C40/TEXT codewords needed to encode characters in full batches of 3 (or less if EOD).
   The number of characters encoded is returned in `len` */
static int dm_getNumberOfC40Words(const unsigned char *source, const int length, const int from, const int mode,
            int *len) {
    int thirdsCount = 0;
    int i;

    for (i = from; i < length; i++) {
        const unsigned char ci = source[i];
        int remainder;

        if (dm_isc40text(mode, ci)) {
            thirdsCount++; /* Native */
        } else if (!(ci & 0x80)) {
            thirdsCount += 2; /* Shift */
        } else if (dm_isc40text(mode, (unsigned char) (ci & 0x7F))) {
            thirdsCount += 3; /* Shift, Upper shift */
        } else {
            thirdsCount += 4; /* Shift, Upper shift, shift */
        }

        remainder = thirdsCount % 3;
        if (remainder == 0 || (remainder == 2 && i + 1 == length)) {
            *len = i - from + 1;
            return ((thirdsCount + 2) / 3) * 2;
        }
    }
    *len = 0;
    return 0;
}

/* Initialize a new edge. Returns endMode */
static int dm_new_Edge(struct zint_symbol *symbol, const unsigned char *source, const int length,
            struct dm_edge *edges, const int mode, const int from, const int len, struct dm_edge *previous,
            struct dm_edge *edge, const int cwds) {
    int previousMode;
    int size;
    int last_ascii, symbols_left;

    edge->mode = mode;
    edge->endMode = mode;
    edge->from = from;
    edge->len = len;
    edge->bytes = 0;
    if (previous) {
        assert(previous->mode && previous->len && previous->size && previous->endMode);
        previousMode = previous->endMode;
        edge->previous = previous - edges;
        size = previous->size;
    } else {
        previousMode = DM_ASCII;
        edge->previous = 0;
        size = 0;
    }

    switch (mode) {
        case DM_ASCII:
            assert(previousMode != DM_EDIFACT);
            size++;
            if (source[from] & 0x80) {
                size++;
            }
            if (previousMode != DM_ASCII && previousMode != DM_BASE256) {
                size++; /* Unlatch to ASCII */
            }
            break;

        case DM_BASE256:
            assert(previousMode != DM_EDIFACT);
            size++;
            if (previousMode != DM_BASE256) {
                size += 2; /* Byte count + latch to BASE256 */
                if (previousMode != DM_ASCII) {
                    size++; /* Unlatch to ASCII */
                }
                edge->bytes = 1;
            } else {
                assert(previous);
                edge->bytes = 1 + previous->bytes;
                if (edge->bytes == 250) {
                    size++; /* Extra byte count */
                }
            }
            break;

        case DM_C40:
        case DM_TEXT:
            assert(previousMode != DM_EDIFACT);
            size += cwds;
            if (previousMode != mode) {
                size++; /* Latch to this mode */
                if (previousMode != DM_ASCII && previousMode != DM_BASE256) {
                    size++; /* Unlatch to ASCII */
                }
            }
            if (from + len + 2 >= length) { /* If less than batch of 3 away from EOD */
                last_ascii = dm_last_ascii(source, length, from + len);
                symbols_left = dm_codewords_remaining(symbol, size + last_ascii, 0);
                if (symbols_left > 0) {
                    size++; /* We need an extra unlatch at the end */
                }
            }
            break;

        case DM_X12:
            assert(previousMode != DM_EDIFACT);
            size += 2;
            if (previousMode != DM_X12) {
                size++; /* Latch to this mode */
                if (previousMode != DM_ASCII && previousMode != DM_BASE256) {
                    size++; /* Unlatch to ASCII */
                }
            }
            if (from + len + 2 >= length) { /* If less than batch of 3 away from EOD */
                last_ascii = dm_last_ascii(source, length, from + len);
                if (last_ascii == 2) { /* Only 1 ASCII-encodable allowed at EOD for X12, unlike C40/TEXT */
                    size++; /* We need an extra unlatch at the end */
                } else {
                    symbols_left = dm_codewords_remaining(symbol, size + last_ascii, 0);
                    if (symbols_left > 0) {
                        size++; /* We need an extra unlatch at the end */
                    }
                }
            }
            break;

        case DM_EDIFACT:
            size += 3;
            if (previousMode != DM_EDIFACT) {
                size++; /* Latch to this mode */
                if (previousMode != DM_ASCII && previousMode != DM_BASE256) {
                    size++; /* Unlatch to ASCII */
                }
            }
            edge->endMode = dm_getEndMode(symbol, source, length, mode, from, len, size);
            break;
    }
    edge->size = size;

    return edge->endMode;
}

/* Add an edge for a mode at a vertex if no existing edge or if more optimal than existing edge */
static void dm_addEdge(struct zint_symbol *symbol, const unsigned char *source, const int length,
            struct dm_edge *edges, const int mode, const int from, const int len, struct dm_edge *previous,
            const int cwds) {
    struct dm_edge edge;
    const int endMode = dm_new_Edge(symbol, source, length, edges, mode, from, len, previous, &edge, cwds);
    const int vertexIndex = from + len;
    const int v_ij = vertexIndex * DM_NUM_MODES + endMode - 1;

    if (edges[v_ij].mode == 0 || edges[v_ij].size > edge.size) {
        DM_TRACE_AddEdge(source, length, edges, previous, vertexIndex, &edge);
        edges[v_ij] = edge;
    } else {
        DM_TRACE_NotAddEdge(source, length, edges, previous, vertexIndex, v_ij, &edge);
    }
}

/* Add edges for the various modes at a vertex */
static void dm_addEdges(struct zint_symbol *symbol, const unsigned char source[], const int length,
            struct dm_edge *edges, const int from, struct dm_edge *previous, const int gs1) {
    int i, pos;

    /* Not possible to unlatch a full EDF edge to something else */
    if (previous == NULL || previous->endMode != DM_EDIFACT) {

        static const char c40text_modes[] = { DM_C40, DM_TEXT };

        if (z_isdigit(source[from]) && from + 1 < length && z_isdigit(source[from + 1])) {
            dm_addEdge(symbol, source, length, edges, DM_ASCII, from, 2, previous, 0);
            /* If ASCII vertex, don't bother adding other edges as this will be optimal; suggested by Alex Geller */
            if (previous && previous->mode == DM_ASCII) {
                return;
            }
        } else {
            dm_addEdge(symbol, source, length, edges, DM_ASCII, from, 1, previous, 0);
        }

        for (i = 0; i < ARRAY_SIZE(c40text_modes); i++) {
            int len;
            int cwds = dm_getNumberOfC40Words(source, length, from, c40text_modes[i], &len);
            if (cwds) {
                dm_addEdge(symbol, source, length, edges, c40text_modes[i], from, len, previous, cwds);
            }
        }

        if (from + 2 < length && dm_isX12(source[from]) && dm_isX12(source[from + 1]) && dm_isX12(source[from + 2])) {
            dm_addEdge(symbol, source, length, edges, DM_X12, from, 3, previous, 0);
        }

        if (gs1 != 1 || source[from] != '\x1D') {
            dm_addEdge(symbol, source, length, edges, DM_BASE256, from, 1, previous, 0);
        }
    }

    if (dm_isedifact(source[from])) {
        /* We create 3 EDF edges, 2, 3 or 4 characters length. The 4-char normally doesn't have a latch to ASCII
           unless it is 2 characters away from the end of the input. */
        for (i = 1, pos = from + i; i < 4 && pos < length && dm_isedifact(source[pos]); i++, pos++) {
            dm_addEdge(symbol, source, length, edges, DM_EDIFACT, from, i + 1, previous, 0);
        }
    }
}

/* Calculate optimized encoding modes */
static int dm_define_mode(struct zint_symbol *symbol, char modes[], const unsigned char source[], const int length,
            const int gs1, const int debug_print) {

    int i, j, v_i;
    int minimalJ, minimalSize;
    struct dm_edge *edge;
    int current_mode;
    int mode_end, mode_len;

    struct dm_edge *edges = (struct dm_edge *) calloc((length + 1) * DM_NUM_MODES, sizeof(struct dm_edge));
    if (!edges) {
        return 0;
    }
    dm_addEdges(symbol, source, length, edges, 0, NULL, gs1);

    DM_TRACE_Edges("DEBUG Initial situation\n", source, length, edges, 0);

    for (i = 1; i < length; i++) {
        v_i = i * DM_NUM_MODES;
        for (j = 0; j < DM_NUM_MODES; j++) {
            if (edges[v_i + j].mode) {
                dm_addEdges(symbol, source, length, edges, i, edges + v_i + j, gs1);
            }
        }
        DM_TRACE_Edges("DEBUG situation after adding edges to vertices at position %d\n", source, length, edges, i);
    }

    DM_TRACE_Edges("DEBUG Final situation\n", source, length, edges, length);

    v_i = length * DM_NUM_MODES;
    minimalJ = -1;
    minimalSize = INT_MAX;
    for (j = 0; j < DM_NUM_MODES; j++) {
        edge = edges + v_i + j;
        if (edge->mode) {
            if (debug_print) printf("edges[%d][%d][0] size %d\n", length, j, edge->size);
            if (edge->size < minimalSize) {
                minimalSize = edge->size;
                minimalJ = j;
                if (debug_print) printf(" set minimalJ %d\n", minimalJ);
            }
        } else {
            if (debug_print) printf("edges[%d][%d][0] NULL\n", length, j);
        }
    }
    assert(minimalJ >= 0);

    edge = edges + v_i + minimalJ;
    mode_len = 0;
    mode_end = length;
    while (edge) {
        current_mode = edge->mode;
        mode_len += edge->len;
        edge = DM_PREVIOUS(edges, edge);
        if (!edge || edge->mode != current_mode) {
            for (i = mode_end - mode_len; i < mode_end; i++) {
                modes[i] = current_mode;
            }
            mode_end = mode_end - mode_len;
            mode_len = 0;
        }
    }

    if (debug_print) {
        printf("modes (%d): ", length);
        for (i = 0; i < length; i++) printf("%c", dm_smodes[(int) modes[i]][0]);
        fputc('\n', stdout);
    }
    assert(mode_end == 0);

    free(edges);

    return 1;
}

/* Do default minimal encodation */
static int dm_minimalenc(struct zint_symbol *symbol, const unsigned char source[], const int length, int *p_sp,
            unsigned char target[], int *p_tp, int process_buffer[8], int *p_process_p, int *p_b256_start,
            int *p_current_mode, const int gs1, const int debug_print) {
    int sp = *p_sp;
    int tp = *p_tp;
    int process_p = *p_process_p;
    int current_mode = *p_current_mode;
    int last_ascii, symbols_left;
    int i;
    char *modes = (char *) z_alloca(length);

    assert(length <= 10921); /* Can only handle (10921 + 1) * 6 = 65532 < 65536 (2*16) due to sizeof(previous) */

    if (!dm_define_mode(symbol, modes, source, length, gs1, debug_print)) {
        return errtxt(ZINT_ERROR_MEMORY, symbol, 728, "Insufficient memory for mode buffers");
    }

    while (sp < length) {

        if (modes[sp] != current_mode) {
            switch (current_mode) {
                case DM_C40:
                case DM_TEXT:
                case DM_X12:
                    process_p = 0; /* Throw away buffer if any */
                    target[tp++] = 254; /* Unlatch */
                    break;
                case DM_EDIFACT:
                    last_ascii = dm_last_ascii(source, length, sp);
                    if (!last_ascii) {
                        process_buffer[process_p++] = 31; /* Unlatch */
                    } else {
                        symbols_left = dm_codewords_remaining(symbol, tp + last_ascii, process_p);
                        if (debug_print) {
                            printf("process_p %d, last_ascii %d, symbols_left %d\n",
                                    process_p, last_ascii, symbols_left);
                        }
                        if (symbols_left > 2 - last_ascii) {
                            process_buffer[process_p++] = 31; /* Unlatch */
                        }
                    }
                    process_p = dm_edi_buffer_xfer(process_buffer, process_p, target, &tp, 1 /*empty*/, debug_print);
                    break;
                case DM_BASE256:
                    tp = dm_update_b256_field_length(target, tp, *p_b256_start);
                    /* B.2.1 255-state randomising algorithm */
                    for (i = *p_b256_start; i < tp; i++) {
                        const int prn = ((149 * (i + 1)) % 255) + 1;
                        target[i] = (unsigned char) ((target[i] + prn) & 0xFF);
                    }
                    break;
            }
            tp = dm_switch_mode(modes[sp], target, tp, p_b256_start, debug_print);
        }

        current_mode = modes[sp];
        assert(current_mode);

        if (current_mode == DM_ASCII) {

            if (is_twodigits(source, length, sp)) {
                target[tp++] = (unsigned char) ((10 * ctoi(source[sp])) + ctoi(source[sp + 1]) + 130);
                if (debug_print) printf("N%02d ", target[tp - 1] - 130);
                sp += 2;
            } else {
                if (source[sp] & 0x80) {
                    target[tp++] = 235; /* FNC4 */
                    target[tp++] = (source[sp] - 128) + 1;
                    if (debug_print) printf("FN4 A%02X ", target[tp - 1] - 1);
                } else {
                    if (gs1 && source[sp] == '\x1D') {
                        if (gs1 == 2) {
                            target[tp++] = 29 + 1; /* GS */
                            if (debug_print) fputs("GS ", stdout);
                        } else {
                            target[tp++] = 232; /* FNC1 */
                            if (debug_print) fputs("FN1 ", stdout);
                        }
                    } else {
                        target[tp++] = source[sp] + 1;
                        if (debug_print) printf("A%02X ", target[tp - 1] - 1);
                    }
                }
                sp++;
            }

        } else if (current_mode == DM_C40 || current_mode == DM_TEXT) {

            int shift_set, value;
            const char *ct_shift, *ct_value;

            if (current_mode == DM_C40) {
                ct_shift = dm_c40_shift;
                ct_value = dm_c40_value;
            } else {
                ct_shift = dm_text_shift;
                ct_value = dm_text_value;
            }

            if (source[sp] & 0x80) {
                process_buffer[process_p++] = 1;
                process_buffer[process_p++] = 30; /* Upper Shift */
                shift_set = ct_shift[source[sp] - 128];
                value = ct_value[source[sp] - 128];
            } else {
                if (gs1 && source[sp] == '\x1D') {
                    if (gs1 == 2) {
                        shift_set = ct_shift[29];
                        value = ct_value[29]; /* GS */
                    } else {
                        shift_set = 2;
                        value = 27; /* FNC1 */
                    }
                } else {
                    shift_set = ct_shift[source[sp]];
                    value = ct_value[source[sp]];
                }
            }

            if (shift_set != 0) {
                process_buffer[process_p++] = shift_set - 1;
            }
            process_buffer[process_p++] = value;

            if (process_p >= 3) {
                process_p = dm_ctx_buffer_xfer(process_buffer, process_p, target, &tp, debug_print);
            }
            sp++;

        } else if (current_mode == DM_X12) {

            static const char x12_nonalphanum_chars[] = "\015*> ";
            int value = 0;

            if (z_isdigit(source[sp])) {
                value = (source[sp] - '0') + 4;
            } else if (z_isupper(source[sp])) {
                value = (source[sp] - 'A') + 14;
            } else {
                value = posn(x12_nonalphanum_chars, source[sp]);
            }

            process_buffer[process_p++] = value;

            if (process_p >= 3) {
                process_p = dm_ctx_buffer_xfer(process_buffer, process_p, target, &tp, debug_print);
            }
            sp++;

        } else if (current_mode == DM_EDIFACT) {

            int value = source[sp];

            if (value >= 64) { /* '@' */
                value -= 64;
            }

            process_buffer[process_p++] = value;
            sp++;

            if (process_p >= 4) {
                process_p = dm_edi_buffer_xfer(process_buffer, process_p, target, &tp, 0 /*empty*/, debug_print);
            }

        } else if (current_mode == DM_BASE256) {

            target[tp++] = source[sp++];
            if (debug_print) printf("B%02X ", target[tp - 1]);
        }

        if (tp > 1558) {
            return errtxt(ZINT_ERROR_TOO_LONG, symbol, 729,
                            "Input too long, requires too many codewords (maximum 1558)");
        }

    } /* while */

    *p_sp = sp;
    *p_tp = tp;
    *p_process_p = process_p;
    *p_current_mode = current_mode;

    return 0;
}

/* Encode using algorithm based on ISO/IEC 21471:2020 Annex J (was ISO/IEC 21471:2006 Annex P) */
static int dm_isoenc(struct zint_symbol *symbol, const unsigned char source[], const int length, int *p_sp,
            unsigned char target[], int *p_tp, int process_buffer[8], int *p_process_p, int *p_b256_start,
            int *p_current_mode, const int gs1, const int debug_print) {
    const int mailmark = symbol->symbology == BARCODE_MAILMARK_2D;
    int sp = *p_sp;
    int tp = *p_tp;
    int process_p = *p_process_p;
    int current_mode = *p_current_mode;
    int not_first = 0;
    int i;

    /* step (a) */
    int next_mode = DM_ASCII;

    if (mailmark) { /* First 45 characters C40 */
        assert(length >= 45);
        next_mode = DM_C40;
        tp = dm_switch_mode(next_mode, target, tp, p_b256_start, debug_print);
        while (sp < 45) {
            assert(!(sp & 0x80));
            process_buffer[process_p++] = dm_c40_value[source[sp]];

            if (process_p >= 3) {
                process_p = dm_ctx_buffer_xfer(process_buffer, process_p, target, &tp, debug_print);
            }
            sp++;
        }
        current_mode = next_mode;
        not_first = 1;
    }

    while (sp < length) {

        current_mode = next_mode;

        /* step (b) - ASCII encodation */
        if (current_mode == DM_ASCII) {
            next_mode = DM_ASCII;

            if (is_twodigits(source, length, sp)) {
                target[tp++] = (unsigned char) ((10 * ctoi(source[sp])) + ctoi(source[sp + 1]) + 130);
                if (debug_print) printf("N%02d ", target[tp - 1] - 130);
                sp += 2;
            } else {
                next_mode = dm_look_ahead_test(source, length, sp, current_mode, 0, gs1, debug_print);

                if (next_mode != DM_ASCII) {
                    tp = dm_switch_mode(next_mode, target, tp, p_b256_start, debug_print);
                    not_first = 0;
                } else {
                    if (source[sp] & 0x80) {
                        target[tp++] = 235; /* FNC4 */
                        target[tp++] = (source[sp] - 128) + 1;
                        if (debug_print) printf("FN4 A%02X ", target[tp - 1] - 1);
                    } else {
                        if (gs1 && source[sp] == '\x1D') {
                            if (gs1 == 2) {
                                target[tp++] = 29 + 1; /* GS */
                                if (debug_print) fputs("GS ", stdout);
                            } else {
                                target[tp++] = 232; /* FNC1 */
                                if (debug_print) fputs("FN1 ", stdout);
                            }
                        } else {
                            target[tp++] = source[sp] + 1;
                            if (debug_print) printf("A%02X ", target[tp - 1] - 1);
                        }
                    }
                    sp++;
                }
            }

        /* step (c)/(d) C40/TEXT encodation */
        } else if (current_mode == DM_C40 || current_mode == DM_TEXT) {

            next_mode = current_mode;
            if (process_p == 0 && not_first) {
                next_mode = dm_look_ahead_test(source, length, sp, current_mode, process_p, gs1, debug_print);
            }

            if (next_mode != current_mode) {
                target[tp++] = 254; /* Unlatch */
                next_mode = DM_ASCII;
                if (debug_print) fputs("ASC ", stdout);
            } else {
                int shift_set, value;
                const char *ct_shift, *ct_value;

                if (current_mode == DM_C40) {
                    ct_shift = dm_c40_shift;
                    ct_value = dm_c40_value;
                } else {
                    ct_shift = dm_text_shift;
                    ct_value = dm_text_value;
                }

                if (source[sp] & 0x80) {
                    process_buffer[process_p++] = 1;
                    process_buffer[process_p++] = 30; /* Upper Shift */
                    shift_set = ct_shift[source[sp] - 128];
                    value = ct_value[source[sp] - 128];
                } else {
                    if (gs1 && source[sp] == '\x1D') {
                        if (gs1 == 2) {
                            shift_set = ct_shift[29];
                            value = ct_value[29]; /* GS */
                        } else {
                            shift_set = 2;
                            value = 27; /* FNC1 */
                        }
                    } else {
                        shift_set = ct_shift[source[sp]];
                        value = ct_value[source[sp]];
                    }
                }

                if (shift_set != 0) {
                    process_buffer[process_p++] = shift_set - 1;
                }
                process_buffer[process_p++] = value;

                if (process_p >= 3) {
                    process_p = dm_ctx_buffer_xfer(process_buffer, process_p, target, &tp, debug_print);
                }
                sp++;
                not_first = 1;
            }

        /* step (e) X12 encodation */
        } else if (current_mode == DM_X12) {

            if (!dm_isX12(source[sp])) {
                next_mode = DM_ASCII;
            } else {
                next_mode = DM_X12;
                if (process_p == 0 && not_first) {
                    next_mode = dm_look_ahead_test(source, length, sp, current_mode, process_p, gs1, debug_print);
                }
            }

            if (next_mode != DM_X12) {
                sp -= process_p; /* About to throw away buffer, need to re-process input, cf Okapi commit [fb7981e] */
                process_p = 0; /* Throw away buffer if any */
                target[tp++] = 254; /* Unlatch */
                next_mode = DM_ASCII;
                if (debug_print) fputs("ASC ", stdout);
            } else {
                static const char x12_nonalphanum_chars[] = "\015*> ";
                int value = 0;

                if (z_isdigit(source[sp])) {
                    value = (source[sp] - '0') + 4;
                } else if (z_isupper(source[sp])) {
                    value = (source[sp] - 'A') + 14;
                } else {
                    value = posn(x12_nonalphanum_chars, source[sp]);
                }

                process_buffer[process_p++] = value;

                if (process_p >= 3) {
                    process_p = dm_ctx_buffer_xfer(process_buffer, process_p, target, &tp, debug_print);
                }
                sp++;
                not_first = 1;
            }

        /* step (f) EDIFACT encodation */
        } else if (current_mode == DM_EDIFACT) {

            if (!dm_isedifact(source[sp])) {
                next_mode = DM_ASCII;
            } else {
                next_mode = DM_EDIFACT;
                if (process_p == 3) {
                    /* Note different than spec Step (f)(2), which suggests checking when 0, but this seems to
                       work better in many cases as the switch to ASCII is "free" */
                    next_mode = dm_look_ahead_test(source, length, sp, current_mode, process_p, gs1, debug_print);
                }
            }

            if (next_mode != DM_EDIFACT) {
                process_buffer[process_p++] = 31;
                process_p = dm_edi_buffer_xfer(process_buffer, process_p, target, &tp, 1 /*empty*/, debug_print);
                next_mode = DM_ASCII;
                if (debug_print) fputs("ASC ", stdout);
            } else {
                int value = source[sp];

                if (value >= 64) { /* '@' */
                    value -= 64;
                }

                process_buffer[process_p++] = value;
                sp++;
                not_first = 1;

                if (process_p >= 4) {
                    process_p = dm_edi_buffer_xfer(process_buffer, process_p, target, &tp, 0 /*empty*/,
                                                    debug_print);
                }
            }

        /* step (g) Base 256 encodation */
        } else if (current_mode == DM_BASE256) {

            if (gs1 == 1 && source[sp] == '\x1D') {
                next_mode = DM_ASCII;
            } else {
                next_mode = DM_BASE256;
                if (not_first) {
                    next_mode = dm_look_ahead_test(source, length, sp, current_mode, tp - (*p_b256_start + 1), gs1,
                                                    debug_print);
                }
            }

            if (next_mode != DM_BASE256) {
                tp = dm_update_b256_field_length(target, tp, *p_b256_start);
                /* B.2.1 255-state randomising algorithm */
                for (i = *p_b256_start; i < tp; i++) {
                    const int prn = ((149 * (i + 1)) % 255) + 1;
                    target[i] = (unsigned char) ((target[i] + prn) & 0xFF);
                }
                /* We switch directly here to avoid flipping back to Base 256 due to `dm_text_sp_cnt()` */
                tp = dm_switch_mode(next_mode, target, tp, p_b256_start, debug_print);
                not_first = 0;
            } else {
                if (gs1 == 2 && source[sp] == '\x1D') {
                    target[tp++] = 29; /* GS */
                } else {
                    target[tp++] = source[sp];
                }
                sp++;
                not_first = 1;
                if (debug_print) printf("B%02X ", target[tp - 1]);
            }
        }

        if (tp > 1558) {
            return errtxt(ZINT_ERROR_TOO_LONG, symbol, 520,
                            "Input too long, requires too many codewords (maximum 1558)");
        }

    } /* while */

    *p_sp = sp;
    *p_tp = tp;
    *p_process_p = process_p;
    *p_current_mode = current_mode;

    return 0;
}

/* Encodes data using ASCII, C40, Text, X12, EDIFACT or Base 256 modes as appropriate
   Supports encoding FNC1 in supporting systems */
static int dm_encode(struct zint_symbol *symbol, const unsigned char source[], const int length, const int eci,
            const int gs1, unsigned char target[], int *p_tp) {
    int sp = 0;
    int tp = *p_tp;
    int current_mode = DM_ASCII;
    int i;
    int process_buffer[8]; /* holds remaining data to finalised */
    int process_p = 0; /* number of characters left to finalise */
    int b256_start = 0;
    int symbols_left;
    int error_number;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    if (eci > 0) {
        /* Encode ECI numbers according to Table 6 */
        target[tp++] = 241; /* ECI Character */
        if (eci <= 126) {
            target[tp++] = (unsigned char) (eci + 1);
        } else if (eci <= 16382) {
            target[tp++] = (unsigned char) ((eci - 127) / 254 + 128);
            target[tp++] = (unsigned char) ((eci - 127) % 254 + 1);
        } else {
            target[tp++] = (unsigned char) ((eci - 16383) / 64516 + 192);
            target[tp++] = (unsigned char) (((eci - 16383) / 254) % 254 + 1);
            target[tp++] = (unsigned char) ((eci - 16383) % 254 + 1);
        }
        if (debug_print) printf("ECI %d ", eci + 1);
    }

    /* If FAST_MODE or MAILMARK_2D, do Annex J-based encodation */
    if ((symbol->input_mode & FAST_MODE) || symbol->symbology == BARCODE_MAILMARK_2D) {
        error_number = dm_isoenc(symbol, source, length, &sp, target, &tp, process_buffer, &process_p,
                                    &b256_start, &current_mode, gs1, debug_print);
    } else { /* Do default minimal encodation */
        error_number = dm_minimalenc(symbol, source, length, &sp, target, &tp, process_buffer, &process_p,
                                        &b256_start, &current_mode, gs1, debug_print);
    }
    if (error_number != 0) {
        return error_number;
    }

    symbols_left = dm_codewords_remaining(symbol, tp, process_p);

    if (debug_print) printf("\nsymbols_left %d, tp %d, process_p %d ", symbols_left, tp, process_p);

    if (current_mode == DM_C40 || current_mode == DM_TEXT) {
        /* NOTE: changed to follow spec exactly here, only using Shift 1 padded triplets when 2 symbol chars remain.
           This matches the behaviour of BWIPP but not tec-it, nor figures 4.15.1-1 and 4.15-1-2 in GS1 General
           Specifications 21.0.1.
         */
        if (debug_print) printf("%s ", current_mode == DM_C40 ? "C40" : "TEX");
        if (process_p == 0) {
            if (symbols_left > 0) {
                target[tp++] = 254; /* Unlatch */
                if (debug_print) fputs("ASC ", stdout);
            }
        } else {
            if (process_p == 2 && symbols_left == 2) {
                /* 5.2.5.2 (b) */
                process_buffer[process_p++] = 0; /* Shift 1 */
                (void) dm_ctx_buffer_xfer(process_buffer, process_p, target, &tp, debug_print);

            } else if (process_p == 1 && symbols_left <= 2 && dm_isc40text(current_mode, source[length - 1])) {
                /* 5.2.5.2 (c)/(d) */
                if (symbols_left > 1) {
                    /* 5.2.5.2 (c) */
                    target[tp++] = 254; /* Unlatch and encode remaining data in ascii. */
                    if (debug_print) fputs("ASC ", stdout);
                }
                target[tp++] = source[length - 1] + 1;
                if (debug_print) printf("A%02X ", target[tp - 1] - 1);

            } else {
                int cnt, total_cnt = 0;
                /* Backtrack to last complete triplet (same technique as BWIPP) */
                while (sp > 0 && process_p % 3) {
                    sp--;
                    cnt = dm_c40text_cnt(current_mode, gs1, source[sp]);
                    total_cnt += cnt;
                    process_p -= cnt;
                }
                if (debug_print) printf("Mode %d, backtracked %d\n", current_mode, (total_cnt / 3) * 2);
                tp -= (total_cnt / 3) * 2;

                target[tp++] = 254; /* Unlatch */
                if (debug_print) fputs("ASC ", stdout);
                for (; sp < length; sp++) {
                    if (is_twodigits(source, length, sp)) {
                        target[tp++] = (unsigned char) ((10 * ctoi(source[sp])) + ctoi(source[sp + 1]) + 130);
                        if (debug_print) printf("N%02d ", target[tp - 1] - 130);
                        sp++;
                    } else if (source[sp] & 0x80) {
                        target[tp++] = 235; /* FNC4 */
                        target[tp++] = (source[sp] - 128) + 1;
                        if (debug_print) printf("FN4 A%02X ", target[tp - 1] - 1);
                    } else if (gs1 && source[sp] == '\x1D') {
                        if (gs1 == 2) {
                            target[tp++] = 29 + 1; /* GS */
                            if (debug_print) fputs("GS ", stdout);
                        } else {
                            target[tp++] = 232; /* FNC1 */
                            if (debug_print) fputs("FN1 ", stdout);
                        }
                    } else {
                        target[tp++] = source[sp] + 1;
                        if (debug_print) printf("A%02X ", target[tp - 1] - 1);
                    }
                }
            }
        }

    } else if (current_mode == DM_X12) {
        if (debug_print) fputs("X12 ", stdout);
        if ((symbols_left == 1) && (process_p == 1)) {
            /* Unlatch not required! */
            target[tp++] = source[length - 1] + 1;
            if (debug_print) printf("A%02X ", target[tp - 1] - 1);
        } else {
            if (symbols_left > 0) {
                target[tp++] = (254); /* Unlatch. */
                if (debug_print) fputs("ASC ", stdout);
            }

            if (process_p == 1) {
                target[tp++] = source[length - 1] + 1;
                if (debug_print) printf("A%02X ", target[tp - 1] - 1);
            } else if (process_p == 2) {
                target[tp++] = source[length - 2] + 1;
                target[tp++] = source[length - 1] + 1;
                if (debug_print) printf("A%02X A%02X ", target[tp - 2] - 1, target[tp - 1] - 1);
            }
        }

    } else if (current_mode == DM_EDIFACT) {
        if (debug_print) fputs("EDI ", stdout);
        if (symbols_left <= 2 && process_p <= symbols_left) { /* Unlatch not required! */
            if (process_p == 1) {
                target[tp++] = source[length - 1] + 1;
                if (debug_print) printf("A%02X ", target[tp - 1] - 1);
            } else if (process_p == 2) {
                target[tp++] = source[length - 2] + 1;
                target[tp++] = source[length - 1] + 1;
                if (debug_print) printf("A%02X A%02X ", target[tp - 2] - 1, target[tp - 1] - 1);
            }
        } else {
            /* Append edifact unlatch value (31) and empty buffer */
            if (process_p <= 3) {
                process_buffer[process_p++] = 31;
            }
            (void) dm_edi_buffer_xfer(process_buffer, process_p, target, &tp, 1 /*empty*/, debug_print);
        }

    } else if (current_mode == DM_BASE256) {
        if (symbols_left > 0) {
            tp = dm_update_b256_field_length(target, tp, b256_start);
        }
        /* B.2.1 255-state randomising algorithm */
        for (i = b256_start; i < tp; i++) {
            int prn = ((149 * (i + 1)) % 255) + 1;
            target[i] = (unsigned char) ((target[i] + prn) & 0xFF);
        }
    }

    if (debug_print) {
        printf("\nData (%d):", tp);
        for (i = 0; i < tp; i++)
            printf(" %d", target[i]);

        fputc('\n', stdout);
    }

    *p_tp = tp;

    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int dm_encode_test(struct zint_symbol *symbol, const unsigned char source[], const int length, const int eci,
            const int gs1, unsigned char target[], int *p_tp) {
    return dm_encode(symbol, source, length, eci, gs1, target, p_tp);
}
#endif

/* Call `dm_encode()` for each segment, dealing with Structured Append, GS1, READER_INIT and macro headers
   beforehand */
static int dm_encode_segs(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count,
            unsigned char target[], int *p_binlen) {
    int error_number;
    int i;
    int tp = 0;
    int gs1;
    int in_macro = 0;
    const struct zint_seg *last_seg = &segs[seg_count - 1];
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    if ((i = segs_length(segs, seg_count)) > 3116) { /* Max is 3166 digits */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 719, "Input length %d too long (maximum 3116)", i);
    }

    if (symbol->structapp.count) {
        int id1, id2;

        if (symbol->structapp.count < 2 || symbol->structapp.count > 16) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 720,
                            "Structured Append count '%d' out of range (2 to 16)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 721,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }
        if (symbol->structapp.id[0]) {
            int id, id_len, id1_err, id2_err;

            for (id_len = 1; id_len < 7 && symbol->structapp.id[id_len]; id_len++);

            if (id_len > 6) { /* ID1 * 1000 + ID2 */
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 722,
                                "Structured Append ID length %d too long (6 digit maximum)", id_len);
            }

            id = to_int((const unsigned char *) symbol->structapp.id, id_len);
            if (id == -1) {
                return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 723, "Invalid Structured Append ID (digits only)");
            }
            id1 = id / 1000;
            id2 = id % 1000;
            id1_err = id1 < 1 || id1 > 254;
            id2_err = id2 < 1 || id2 > 254;
            if (id1_err || id2_err) {
                if (id1_err && id2_err) {
                    return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 724,
                                    "Structured Append ID1 '%1$03d' and ID2 '%2$03d' out of range (001 to 254)"
                                    " (ID \"%3$03d%4$03d\")",
                                    id1, id2, id1, id2);
                }
                if (id1_err) {
                    return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 725,
                                    "Structured Append ID1 '%1$03d' out of range (001 to 254) (ID \"%2$03d%3$03d\")",
                                    id1, id1, id2);
                }
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 726,
                                "Structured Append ID2 '%1$03d' out of range (001 to 254) (ID \"%2$03d%3$03d\")",
                                id2, id1, id2);
            }
        } else {
            id1 = id2 = 1;
        }

        target[tp++] = 233;
        target[tp++] = (17 - symbol->structapp.count) | ((symbol->structapp.index - 1) << 4);
        target[tp++] = id1;
        target[tp++] = id2;
    }

    /* gs1 flag values: 0: no gs1, 1: gs1 with FNC1 serparator, 2: GS separator */
    if ((symbol->input_mode & 0x07) == GS1_MODE) {
        if (symbol->output_options & GS1_GS_SEPARATOR) {
            gs1 = 2;
        } else {
            gs1 = 1;
        }
    } else {
        gs1 = 0;
    }

    if (gs1) {
        target[tp++] = 232;
        if (debug_print) fputs("FN1 ", stdout);
    } /* FNC1 */

    if (symbol->output_options & READER_INIT) {
        if (gs1) {
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 521, "Cannot use Reader Initialisation in GS1 mode");
        }
        if (symbol->structapp.count) {
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 727,
                            "Cannot have Structured Append and Reader Initialisation at the same time");
        }
        target[tp++] = 234; /* Reader Programming */
        if (debug_print) fputs("RP ", stdout);
    }

    /* Check for Macro05/Macro06 */
    /* "[)>[RS]05[GS]...[RS][EOT]" -> CW 236 */
    /* "[)>[RS]06[GS]...[RS][EOT]" -> CW 237 */
    if (tp == 0 && segs[0].length >= 9 && last_seg->length >= 2
            && segs[0].source[0] == '[' && segs[0].source[1] == ')' && segs[0].source[2] == '>'
            && segs[0].source[3] == '\x1e' /*RS*/ && segs[0].source[4] == '0'
            && (segs[0].source[5] == '5' || segs[0].source[5] == '6')
            && segs[0].source[6] == '\x1d' /*GS*/
            && last_seg->source[last_seg->length - 1] == '\x04' /*EOT*/
            && last_seg->source[last_seg->length - 2] == '\x1e' /*RS*/) {

        /* Output macro Codeword */
        if (segs[0].source[5] == '5') {
            target[tp++] = 236;
            if (debug_print) fputs("Macro05 ", stdout);
        } else {
            target[tp++] = 237;
            if (debug_print) fputs("Macro06 ", stdout);
        }
        /* Remove macro characters from input string */
        in_macro = 1;
    }

    for (i = 0; i < seg_count; i++) {
        int src_inc = 0, len_dec = 0;
        if (in_macro) {
            if (i == 0) {
                src_inc = len_dec = 7; /* Skip over macro characters at beginning */
            }
            if (i + 1 == seg_count) {
                len_dec += 2;  /* Remove RS + EOT from end */
            }
        }
        error_number = dm_encode(symbol, segs[i].source + src_inc, segs[i].length - len_dec, segs[i].eci, gs1,
                        target, &tp);
        if (error_number != 0) {
            return error_number;
        }
    }

    *p_binlen = tp;

    return 0;
}

/* add pad bits */
static void dm_add_tail(unsigned char target[], int tp, const int tail_length) {
    int i, prn, temp;

    target[tp++] = 129; /* Pad */
    for (i = 1; i < tail_length; i++) {
        /* B.1.1 253-state randomising algorithm */
        prn = ((149 * (tp + 1)) % 253) + 1;
        temp = 129 + prn;
        if (temp <= 254) {
            target[tp++] = (unsigned char) (temp);
        } else {
            target[tp++] = (unsigned char) (temp - 254);
        }
    }
}

static int dm_ecc200(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int i, skew = 0;
    unsigned char binary[2200];
    int binlen = 0; /* Suppress clang-tidy-20 uninitialized value false positive */
    int symbolsize;
    int taillength, error_number;
    int H, W, FH, FW, datablock, bytes, rsblock;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    /* `length` may be decremented by 2 if macro character is used */
    error_number = dm_encode_segs(symbol, segs, seg_count, binary, &binlen);
    if (error_number != 0) {
        return error_number;
    }

    symbolsize = dm_get_symbolsize(symbol, binlen);

    if (binlen > dm_matrixbytes[symbolsize]) {
        if ((symbol->option_2 >= 1) && (symbol->option_2 <= DMSIZESCOUNT)) {
            /* The symbol size was given by --ver (option_2) */
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 522,
                        "Input too long for Version %1$d, requires %2$d codewords (maximum %3$d)",
                        symbol->option_2, binlen, dm_matrixbytes[symbolsize]);
        }
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 523, "Input too long, requires %d codewords (maximum 1558)",
                        binlen);
    }

    H = dm_matrixH[symbolsize];
    W = dm_matrixW[symbolsize];
    FH = dm_matrixFH[symbolsize];
    FW = dm_matrixFW[symbolsize];
    bytes = dm_matrixbytes[symbolsize];
    datablock = dm_matrixdatablock[symbolsize];
    rsblock = dm_matrixrsblock[symbolsize];

    taillength = bytes - binlen;

    if (taillength != 0) {
        dm_add_tail(binary, binlen, taillength);
    }
    if (debug_print) {
        printf("Pads (%d): ", taillength);
        for (i = binlen; i < binlen + taillength; i++) printf("%d ", binary[i]);
        fputc('\n', stdout);
    }

    /* ecc code */
    if (symbolsize == DMINTSYMBOL144 && !(symbol->option_3 & DM_ISO_144)) {
        skew = 1;
    }
    dm_ecc(binary, bytes, datablock, rsblock, skew);
    if (debug_print) {
        printf("ECC (%d): ", rsblock * (bytes / datablock));
        for (i = bytes; i < bytes + rsblock * (bytes / datablock); i++) printf("%d ", binary[i]);
        fputc('\n', stdout);
    }

#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump(symbol, binary, skew ? 1558 + 620 : bytes + rsblock * (bytes / datablock));
    }
#endif
    { /* placement */
        const int NC = W - 2 * (W / FW);
        const int NR = H - 2 * (H / FH);
        int x, y, *places;
        if (!(places = (int *) calloc(NC * NR, sizeof(int)))) {
            return errtxt(ZINT_ERROR_MEMORY, symbol, 718, "Insufficient memory for placement array");
        }
        dm_placement(places, NR, NC);
        for (y = 0; y < H; y += FH) {
            for (x = 0; x < W; x++)
                set_module(symbol, (H - y) - 1, x);
            for (x = 0; x < W; x += 2)
                set_module(symbol, y, x);
        }
        for (x = 0; x < W; x += FW) {
            for (y = 0; y < H; y++)
                set_module(symbol, (H - y) - 1, x);
            for (y = 0; y < H; y += 2)
                set_module(symbol, (H - y) - 1, x + FW - 1);
        }
#ifdef DM_DEBUG
        /* Print position matrix as in standard */
        for (y = NR - 1; y >= 0; y--) {
            for (x = 0; x < NC; x++) {
                const int v = places[(NR - y - 1) * NC + x];
                if (x != 0) fprintf(stderr, "|");
                fprintf(stderr, "%3d.%2d", (v >> 3), 8 - (v & 7));
            }
            fprintf(stderr, "\n");
        }
#endif
        for (y = 0; y < NR; y++) {
            for (x = 0; x < NC; x++) {
                const int v = places[(NR - y - 1) * NC + x];
                if (v == 1 || (v > 7 && (binary[(v >> 3) - 1] & (1 << (v & 7))))) {
                    set_module(symbol, H - (1 + y + 2 * (y / (FH - 2))) - 1, 1 + x + 2 * (x / (FW - 2)));
                }
            }
        }
        for (y = 0; y < H; y++) {
            symbol->row_height[y] = 1;
        }
        free(places);
    }

    symbol->height = H;
    symbol->rows = H;
    symbol->width = W;

    return error_number;
}

INTERNAL int datamatrix(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {

    if (symbol->option_1 <= 1) {
        /* ECC 200 */
        return dm_ecc200(symbol, segs, seg_count);
    }
    /* ECC 000 - 140 */
    return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 524, "Older Data Matrix standards are no longer supported");
}

/* vim: set ts=4 sw=4 et : */
