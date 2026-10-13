/* dotcode.c - Handles DotCode */
/*
    libzint - the open source barcode library
    Copyright (C) 2017-2024 Robin Stuart <rstuart114@gmail.com>

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

/*
 * Attempts to encode DotCode according to (AIMD013) ISS DotCode Rev. 4.0, DRAFT 0.15, TSC Pre-PR #5,
 * dated May 28, 2019
 * Incorporating suggestions from Terry Burton at BWIPP
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "gs1.h"

#define GF 113
#define PM 3
#define SCORE_UNLIT_EDGE    -99999

/* DotCode symbol character dot patterns, from Annex C */
static const unsigned short dc_dot_patterns[113] = {
    0x155, 0x0ab, 0x0ad, 0x0b5, 0x0d5, 0x156, 0x15a, 0x16a, 0x1aa, 0x0ae,
    0x0b6, 0x0ba, 0x0d6, 0x0da, 0x0ea, 0x12b, 0x12d, 0x135, 0x14b, 0x14d,
    0x153, 0x159, 0x165, 0x169, 0x195, 0x1a5, 0x1a9, 0x057, 0x05b, 0x05d,
    0x06b, 0x06d, 0x075, 0x097, 0x09b, 0x09d, 0x0a7, 0x0b3, 0x0b9, 0x0cb,
    0x0cd, 0x0d3, 0x0d9, 0x0e5, 0x0e9, 0x12e, 0x136, 0x13a, 0x14e, 0x15c,
    0x166, 0x16c, 0x172, 0x174, 0x196, 0x19a, 0x1a6, 0x1ac, 0x1b2, 0x1b4,
    0x1ca, 0x1d2, 0x1d4, 0x05e, 0x06e, 0x076, 0x07a, 0x09e, 0x0bc, 0x0ce,
    0x0dc, 0x0e6, 0x0ec, 0x0f2, 0x0f4, 0x117, 0x11b, 0x11d, 0x127, 0x133,
    0x139, 0x147, 0x163, 0x171, 0x18b, 0x18d, 0x193, 0x199, 0x1a3, 0x1b1,
    0x1c5, 0x1c9, 0x1d1, 0x02f, 0x037, 0x03b, 0x03d, 0x04f, 0x067, 0x073,
    0x079, 0x08f, 0x0c7, 0x0e3, 0x0f1, 0x11e, 0x13c, 0x178, 0x18e, 0x19c,
    0x1b8, 0x1c6, 0x1cc
};

/* Printed() routine from Annex A adapted to char array of ASCII 1's and 0's */
static int dc_get_dot(const char Dots[], const int Hgt, const int Wid, const int x, const int y) {

    if ((x >= 0) && (x < Wid) && (y >= 0) && (y < Hgt)) {
        if (Dots[(y * Wid) + x] == '1') {
            return 1;
        }
    }

    return 0;
}

static int dc_clr_col(const char *Dots, const int Hgt, const int Wid, const int x) {
    int y;
    for (y = x & 1; y < Hgt; y += 2) {
        if (dc_get_dot(Dots, Hgt, Wid, x, y)) {
            return 0;
        }
    }

    return 1;
}

static int dc_clr_row(const char *Dots, const int Hgt, const int Wid, const int y) {
    int x;
    for (x = y & 1; x < Wid; x += 2) {
        if (dc_get_dot(Dots, Hgt, Wid, x, y)) {
            return 0;
        }
    }

    return 1;
}

/* calc penalty for empty interior columns */
static int dc_col_penalty(const char *Dots, const int Hgt, const int Wid) {
    int x, penalty = 0, penalty_local = 0;

    for (x = 1; x < Wid - 1; x++) {
        if (dc_clr_col(Dots, Hgt, Wid, x)) {
            if (penalty_local == 0) {
                penalty_local = Hgt;
            } else {
                penalty_local *= Hgt;
            }
        } else {
            if (penalty_local) {
                penalty += penalty_local;
                penalty_local = 0;
            }
        }
    }

    return penalty + penalty_local;
}

/* calc penalty for empty interior rows */
static int dc_row_penalty(const char *Dots, const int Hgt, const int Wid) {
    int y, penalty = 0, penalty_local = 0;

    for (y = 1; y < Hgt - 1; y++) {
        if (dc_clr_row(Dots, Hgt, Wid, y)) {
            if (penalty_local == 0) {
                penalty_local = Wid;
            } else {
                penalty_local *= Wid;
            }
        } else {
            if (penalty_local) {
                penalty += penalty_local;
                penalty_local = 0;
            }
        }
    }

    return penalty + penalty_local;
}

/* Dot pattern scoring routine from Annex A */
static int dc_score_array(const char Dots[], const int Hgt, const int Wid) {
    int x, y, worstedge, first, last, sum;
    int penalty = 0;

    /* first, guard against "pathelogical" gaps in the array
       subtract a penalty score for empty rows/columns from total code score for each mask,
       where the penalty is Sum(N ^ n), where N is the number of positions in a column/row,
       and n is the number of consecutive empty rows/columns */
    penalty = dc_row_penalty(Dots, Hgt, Wid) + dc_col_penalty(Dots, Hgt, Wid);

    sum = 0;
    first = -1;
    last = -1;

    /* across the top edge, count printed dots and measure their extent */
    for (x = 0; x < Wid; x += 2) {
        if (dc_get_dot(Dots, Hgt, Wid, x, 0)) {
            if (first < 0) {
                first = x;
            }
            last = x;
            sum++;
        }
    }
    if (sum == 0) {
        return SCORE_UNLIT_EDGE;      /* guard against empty top edge */
    }

    worstedge = sum + last - first;
    worstedge *= Hgt;

    sum = 0;
    first = -1;
    last = -1;

    /* across the bottom edge, ditto */
    for (x = Wid & 1; x < Wid; x += 2) {
        if (dc_get_dot(Dots, Hgt, Wid, x, Hgt - 1)) {
            if (first < 0) {
                first = x;
            }
            last = x;
            sum++;
        }
    }
    if (sum == 0) {
        return SCORE_UNLIT_EDGE;      /* guard against empty bottom edge */
    }

    sum += last - first;
    sum *= Hgt;
    if (sum < worstedge) {
        worstedge = sum;
    }

    sum = 0;
    first = -1;
    last = -1;

    /* down the left edge, ditto */
    for (y = 0; y < Hgt; y += 2) {
        if (dc_get_dot(Dots, Hgt, Wid, 0, y)) {
            if (first < 0) {
                first = y;
            }
            last = y;
            sum++;
        }
    }
    if (sum == 0) {
        return SCORE_UNLIT_EDGE;      /* guard against empty left edge */
    }

    sum += last - first;
    sum *= Wid;
    if (sum < worstedge) {
        worstedge = sum;
    }

    sum = 0;
    first = -1;
    last = -1;

    /* down the right edge, ditto */
    for (y = Hgt & 1; y < Hgt; y += 2) {
        if (dc_get_dot(Dots, Hgt, Wid, Wid - 1, y)) {
            if (first < 0) {
                first = y;
            }
            last = y;
            sum++;
        }
    }
    if (sum == 0) {
        return SCORE_UNLIT_EDGE;      /* guard against empty right edge */
    }

    sum += last - first;
    sum *= Wid;
    if (sum < worstedge) {
        worstedge = sum;
    }

    /* throughout the array, count the # of unprinted 5-somes (cross patterns)
       plus the # of printed dots surrounded by 8 unprinted neighbors */
    sum = 0;
    for (y = 0; y < Hgt; y++) {
        for (x = y & 1; x < Wid; x += 2) {
            if (!dc_get_dot(Dots, Hgt, Wid, x - 1, y - 1) && !dc_get_dot(Dots, Hgt, Wid, x + 1, y - 1)
                    && !dc_get_dot(Dots, Hgt, Wid, x - 1, y + 1) && !dc_get_dot(Dots, Hgt, Wid, x + 1, y + 1)
                    && (!dc_get_dot(Dots, Hgt, Wid, x, y)
                        || (!dc_get_dot(Dots, Hgt, Wid, x - 2, y) && !dc_get_dot(Dots, Hgt, Wid, x, y - 2)
                            && !dc_get_dot(Dots, Hgt, Wid, x + 2, y) && !dc_get_dot(Dots, Hgt, Wid, x, y + 2)))) {
                sum++;
            }
        }
    }

    return (worstedge - sum * sum - penalty);
}

/*-------------------------------------------------------------------------
// "rsencode(nd,nc)" adds "nc" R-S check words to "nd" data words in wd[]
// employing Galois Field GF, where GF is prime, with a prime modulus of PM
//-------------------------------------------------------------------------*/

static void dc_rsencode(const int nd, const int nc, unsigned char *wd) {
    /* Pre-calculated coefficients for GF(113) of generator polys of degree 3 to 39. To generate run
       "backend/tests/test_dotcode -f generate -g" and place result below */
    static const char coefs[820 - 5] = { /* 40*(41 + 1)/2 == 820 less 2 + 3 (degrees 1 and 2) */
          1,  74,  12,  62,
          1, 106,   7, 107,  63,
          1,  89,  13, 101,  52,  59,
          1,  38, 107,   3,  99,   6,  42,
          1, 111,  56,  17,  92,   1,  28,  15,
          1, 104,  70,  77,  86,  35,  21,  45,   8,
          1,  83,  33,  76,  51,  37,  77,  56,  80,  58,
          1,  20,   2,  31,   9, 101,   6,  64,  55, 103,  75,
          1,  57,  64, 105,  26,  95,  14,  60,  50, 104,  44,  63,
          1,  55,  63,  90,  42,  43,  50,  32,  43,   4,  62,  88, 100,
          1,  49,  72,  51,  67,  17,  18,  71,  77,  85,  38,  55,  24,  78,
          1,  31,  94, 111,  53,  54,  51,  86,  42,  55,  90,  49,  51,  98,  65,
          1,  90,   2,   7,  48,  17,  73,  44,  31,  47,  58,  48,   4,  56,  84, 106,
          1,  41, 112,  22,  44,  38,  31,  83,  22, 110,  15,  31,  25,  86,  52,  58,   4,
          1,   7,  74,  56,  87,  11,  95,  46,  25,  40,   4,  86, 101,  27,  66,  98,  66,  90,
          1,  18,  38,  79,  25,  64, 103,  74,  79,  89, 105,  17,  30,   8,  24,  33,  14,  25,  86,
          1,  51,  67,  90,  33,  98,  68,  83,  35,  97, 104,  92,  26,  94,  62,  34,  86,  35,   7,  13,
          1,  37,  31,  56,  16,  88,  52,  35,   3,  59, 102, 105,  94,  69, 102,  70,  62,  74,  82,  28,  44,
          1, 108,  59, 110,  37,  94,  85, 111,   2,  46, 110,   2,  91,  76,  29,  80,  60,  69,  25,  87, 111,  73,
          1,  95,  11,  21,  76,  65, 106,  23,  28,  20,  77,  41,  65,  23,  58,  42,  37,  80,  32, 101, 110,  99,
              68,
          1,  56,  35,  44,  48,  39,  57,  70,  35,  58,  88,  89,  48,  87,  65,  40,  94, 106,  76,  96,  13, 103,
              49,  60,
          1,  52,  37,  17,  98,  73,  14,  68,  94,  31,  82,  76,  31,   8,  56,   6,  47,  69, 104,  18,  81,  51,
              89,  90,  99,
          1,  40,  91,  25,   7,  27,  42,  13,  69,  33,  49, 109,  23,  88,  73,  12,  88,  70,  67,  13,  91,  96,
              42,  39,  36,  55,
          1,   4,   7,  26,  11,   1,  87,  83,  53,  35, 104,  40,  54,  51,  69,  96, 108,  66,  33,  87,  75,  97,
              89, 109, 101,   2,  54,
          1,   9,  27,  61,  28,  56,  92,  66,  16,  74,  53, 108,  28,  95,  98, 102,  23,  41,  24,  26,  58,  20,
               9, 102,  81,  55,  64,  44,
          1,  24,  49,  14,  39,  24,  28,  90, 102,  88,  33, 112,  66,  63,  54, 103,  84,  47,  74,  47, 109,  99,
              83,  11,  29,  27,  98, 100,  95,
          1,  69, 112,  72, 104,  84,  91, 107,  84,  45,  38,  15,  21,  95,  64,  47,  86,  98,  42, 100,  77,  32,
              18,  17,  72,  89,  70, 103,  75,  94,
          1,  91,  48,  50, 106, 112,  18,  75,  65,  85,  11,  60,  12, 105,   7,  99, 103,  69,  51,   7,  17,  31,
              44,  74, 107,  91, 107,  61,  81,  49,  34,
          1,  44,  65,  54,  16, 102,  65,  20,  43,  81,  84, 108,  17, 106,  44, 109,  83,  87,  85,  96,  27,  23,
              56,  40,  19,  34,  11,   4,  39,  84, 104,  97,
          1,  16,  76,  42,  86, 106,  34,   8,  48,   7,  76,  16,  44,  82,  14,   7,  82,  23,  22,  89,  51,  58,
              90,  54,  29,  67,  76,  35,  40,   9,  12,  10, 109,
          1,  45,  88,  99,  61,   1,  57,  90,  54,  43,  53,  73,  56,   2,  19,  74,  59,  28,  11,  49,  33,  68,
              77,  65,  13,   4,  98,  92,  38,  39,  47,  19,  60, 110,
          1,  19,  48,  71,  86, 110,  31,  77,  87, 108,  65,  51,  79,  15,  80,  32,  56,  76,  74, 102,   2,   1,
               4,  97,  18,   5, 107,  30,  19,  68,  50,  40,  18,  19,  78,
          1,  54,  35,  56,  85,  69,  39,  32,  70, 102,   3,  66,  56,  68,  40,   7,  46,   2,  22,  93,  69,  71,
              39,  11,  23,  70,  56,  46,  52,  55,  57,  95,  62,  84,  65,  18,
          1,  46,  55,   2,  89,  67,  52,  59,  40, 107,  91,  42,  93,  72,  61,  26, 103,  86,   6,  30,   3,  84,
              36,  38,  48, 112,  61,  50,  23,  91,  69,  91,  93,  40,  71,  63,  82,
          1,  22,  81,  38,  41,  78,  26,  54,  93,  51,   9,   5, 102, 100,  28,  31,  44, 100,  89, 112,  74,  12,
              54,  78,  40,  90,  85,  55,  66, 104,  32,  17,  56,  68,  15,  54,  39,  66,
          1,  63,  79,  82,  17,  64,  60, 103,  47,  22,  66,  35,  81, 101,  60,  49,  72,  96,   8,  32,  33, 108,
              94,  32,  74,  35,  46,  37,  61,  98,   2,  86,  75, 104,  91, 104, 106,  83, 107,
          1,  73,  31,  81,  46,   8,  22,  25,  60,  40,  60,  17,  92,   7,  53,  84, 110,  25,  64, 112,  14,  99,
              44,  68,  55,  97,  57,  45,  92,  30,  78, 106,  31,  63,   1, 110,  16,  13,  33,  53,
    };
    static const short cinds[39 - 2] = { /* Indexes into above coefs[] array */
          0,   4,   9,  15,  22,  30,  39,  49,  60,  72,  85,  99, 114, 130, 147, 165, 184, 204, 225, 247, 270, 294,
        319, 345, 372, 400, 429, 459, 490, 522, 555, 589, 624, 660, 697, 735, 774,
    };
    int i, j, k, nw, start, step;
    const char *c;

    /* Here we compute how many interleaved R-S blocks will be needed */
    nw = nd + nc;
    step = (nw + GF - 2) / (GF - 1);

    /* ...& then for each such block: */
    for (start = 0; start < step; start++) {
        const int ND = (nd - start + step - 1) / step;
        const int NW = (nw - start + step - 1) / step;
        const int NC = NW - ND;
        unsigned char *const e = wd + start + ND * step;

        /* first set the generator polynomial "c" of order "NC": */
        c = coefs + cinds[NC - 3];

        /* & then compute the corresponding checkword values into wd[]
           ... (a) starting at wd[start] & (b) stepping by step */
        for (i = 0; i < NC; i++) {
            e[i * step] = 0;
        }
        for (i = 0; i < ND; i++) {
            k = (wd[start + i * step] + e[0]) % GF;
            for (j = 0; j < NC - 1; j++) {
                e[j * step] = (GF - ((c[j + 1] * k) % GF) + e[(j + 1) * step]) % GF;
            }
            e[(NC - 1) * step] = (GF - ((c[NC] * k) % GF)) % GF;
        }
        for (i = 0; i < NC; i++) {
            if (e[i * step]) {
                e[i * step] = GF - e[i * step];
            }
        }
    }
}

/* Check if the next character is directly encodable in code set A (Annex F.II.D) */
static int dc_datum_a(const unsigned char source[], const int length, const int position) {

    if (position < length && source[position] <= 95) {
        return 1;
    }

    return 0;
}

/* Check if the next character is directly encodable in code set B (Annex F.II.D).
 * Note changed to return 2 if CR/LF */
static int dc_datum_b(const unsigned char source[], const int length, const int position) {

    if (position < length) {
        if ((source[position] >= 32) && (source[position] <= 127)) {
            return 1;
        }

        switch (source[position]) {
            case 9: /* HT */
            case 28: /* FS */
            case 29: /* GS */
            case 30: /* RS */
                return 1;
                break;
        }

        if ((position + 1 < length) && (source[position] == 13) && (source[position + 1] == 10)) { /* CRLF */
            return 2;
        }
    }

    return 0;
}

/* Check if the next characters are directly encodable in code set C (Annex F.II.D) */
static int dc_datum_c(const unsigned char source[], const int length, const int position) {
    return is_twodigits(source, length, position);
}

/* Checks ahead for 10 or more digits starting "17xxxxxx10..." (Annex F.II.B) */
static int dc_seventeen_ten(const unsigned char source[], const int length, const int position) {

    if (position + 9 < length && source[position] == '1' && source[position + 1] == '7'
            && source[position + 8] == '1' && source[position + 9] == '0'
            && cnt_digits(source, length, position + 2, 6) >= 6) {
        return 1;
    }

    return 0;
}

/*  Checks how many characters ahead can be reached while dc_datum_c is true,
 *  returning the resulting number of codewords (Annex F.II.E)
 */
static int dc_ahead_c(const unsigned char source[], const int length, const int position) {
    int count = 0;
    int i;

    for (i = position; (i < length) && dc_datum_c(source, length, i); i += 2) {
        count++;
    }

    return count;
}

/* Annex F.II.F */
static int dc_try_c(const unsigned char source[], const int length, const int position) {

    if (position < length && z_isdigit(source[position])) { /* cnt_digits(position) > 0 */
        const int ahead_c_position = dc_ahead_c(source, length, position);
        if (ahead_c_position > dc_ahead_c(source, length, position + 1)) {
            return ahead_c_position;
        }
    }

    return 0;
}

/* Annex F.II.G */
static int dc_ahead_a(const unsigned char source[], const int length, const int position) {
    int count = 0;
    int i;

    for (i = position; i < length && dc_datum_a(source, length, i) && dc_try_c(source, length, i) < 2; i++) {
        count++;
    }

    return count;
}

/* Annex F.II.H Note: changed to return number of chars encodable. Number of codewords returned in *p_nx. */
static int dc_ahead_b(const unsigned char source[], const int length, const int position, int *p_nx) {
    int count = 0;
    int i, incr;

    for (i = position; i < length && (incr = dc_datum_b(source, length, i))
            && dc_try_c(source, length, i) < 2; i += incr) {
        count++;
    }

    if (p_nx != NULL) {
        *p_nx = count;
    }

    return i - position;
}

/* Checks if the next character is in the range 128 to 255  (Annex F.II.I) */
static int dc_binary(const unsigned char source[], const int length, const int position) {

    if (position < length && source[position] >= 128) {
        return 1;
    }

    return 0;
}

/* Empty binary buffer */
static int dc_empty_bin_buf(unsigned char *codeword_array, int ap, uint64_t *p_bin_buf, int *p_bin_buf_size) {
    int i;
    int lawrencium[6]; /* Reversed radix 103 values */
    uint64_t bin_buf = *p_bin_buf;
    int bin_buf_size = *p_bin_buf_size;

    if (bin_buf_size) {
        for (i = 0; i < bin_buf_size + 1; i++) {
            lawrencium[i] = (int) (bin_buf % 103);
            bin_buf /= 103;
        }

        for (i = 0; i < bin_buf_size + 1; i++) {
            codeword_array[ap++] = lawrencium[bin_buf_size - i];
        }
    }

    *p_bin_buf = 0;
    *p_bin_buf_size = 0;

    return ap;
}

/* Add value to binary buffer, emptying if full */
static int dc_append_to_bin_buf(unsigned char *codeword_array, int ap, unsigned int val, uint64_t *p_bin_buf,
            int *p_bin_buf_size) {

    *p_bin_buf *= 259;
    *p_bin_buf += val;
    (*p_bin_buf_size)++;

    if (*p_bin_buf_size == 5) {
        ap = dc_empty_bin_buf(codeword_array, ap, p_bin_buf, p_bin_buf_size);
    }

    return ap;
}

/* Analyse input data stream and encode using algorithm from Annex F */
static int dc_encode_message(struct zint_symbol *symbol, const unsigned char source[], const int length,
            const int eci, const int last_seg, const int last_EOT, const int last_RSEOT,
            int ap, unsigned char *codeword_array, char *p_encoding_mode, int *p_inside_macro,
            uint64_t *p_bin_buf, int *p_bin_buf_size, unsigned char structapp_array[], int *p_structapp_size) {
    static const char lead_specials[] = "\x09\x1C\x1D\x1E"; /* HT, FS, GS, RS */

    int i;
    int position = 0;
    char encoding_mode = *p_encoding_mode;
    int inside_macro = *p_inside_macro;
    uint64_t bin_buf = *p_bin_buf;
    int bin_buf_size = *p_bin_buf_size;
    int nx;

    const int first_seg = ap == 0;
    const int gs1 = (symbol->input_mode & 0x07) == GS1_MODE;
    const int debug_print = (symbol->debug & ZINT_DEBUG_PRINT);

    if (first_seg) {
        if (symbol->output_options & READER_INIT) {
            codeword_array[ap++] = 109; /* FNC3 */

        } else if (!gs1 && eci == 0 && length > 2 && is_twodigits(source, length, 0)) {
            codeword_array[ap++] = 107; /* FNC1 */

        } else if (posn(lead_specials, source[0]) != -1) {
            /* Prevent encodation as a macro if a special character is in first position */
            codeword_array[ap++] = 101; /* Latch A */
            codeword_array[ap++] = source[0] + 64;
            encoding_mode = 'A';
            position++;

        } else if (length > 5) { /* Note assuming macro headers don't straddle segments */
            /* Step C1 */
            if (source[0] == '[' && source[1] == ')' && source[2] == '>' && source[3] == 30 /*RS*/ && last_EOT) {
                int format_050612 = (source[4] == '0' && (source[5] == '5' || source[5] == '6'))
                                    || (source[4] == '1' && source[5] == '2');
                inside_macro = 0;
                if (length > 6 && format_050612 && source[6] == 29 /*GS*/ && last_RSEOT) {
                    if (source[5] == '5') {
                        inside_macro = 97;
                    } else if (source[5] == '6') {
                        inside_macro = 98;
                    } else {
                        inside_macro = 99;
                    }
                } else if (!format_050612 && is_twodigits(source, length, 4) ) {
                    inside_macro = 100; /* Note no longer using for malformed 05/06/12 */
                }
                if (inside_macro) {
                    codeword_array[ap++] = 106; /* Latch B */
                    encoding_mode = 'B';
                    codeword_array[ap++] = inside_macro; /* Macro */
                    if (inside_macro == 100) {
                        codeword_array[ap++] = ctoi(source[4]) + 16;
                        codeword_array[ap++] = ctoi(source[5]) + 16;
                        position += 6;
                    } else {
                        position += 7;
                    }
                    if (debug_print) printf("C1/%d ", inside_macro - 96);
                }
            }
        }
    }

    if (eci > 0) {
        if (encoding_mode == 'X') {
            if (eci <= 0xFF) {
                ap = dc_append_to_bin_buf(codeword_array, ap, 256, &bin_buf, &bin_buf_size);
                ap = dc_append_to_bin_buf(codeword_array, ap, eci, &bin_buf, &bin_buf_size);
            /* Following BWIPP, assuming big-endian byte order */
            } else if (eci <= 0xFFFF) {
                ap = dc_append_to_bin_buf(codeword_array, ap, 257, &bin_buf, &bin_buf_size);
                ap = dc_append_to_bin_buf(codeword_array, ap, eci >> 8, &bin_buf, &bin_buf_size);
                ap = dc_append_to_bin_buf(codeword_array, ap, eci & 0xFF, &bin_buf, &bin_buf_size);
            } else {
                ap = dc_append_to_bin_buf(codeword_array, ap, 258, &bin_buf, &bin_buf_size);
                ap = dc_append_to_bin_buf(codeword_array, ap, eci >> 16, &bin_buf, &bin_buf_size);
                ap = dc_append_to_bin_buf(codeword_array, ap, (eci >> 8) & 0xFF, &bin_buf, &bin_buf_size);
                ap = dc_append_to_bin_buf(codeword_array, ap, eci & 0xFF, &bin_buf, &bin_buf_size);
            }
        } else {
            codeword_array[ap++] = 108; /* FNC2 */
            if (eci <= 39) {
                codeword_array[ap++] = eci;
            } else {
                /* the next three codewords valued A, B & C encode the ECI value of
                   (A - 40) * 12769 + B * 113 + C + 40 (Section 5.2.1) */
                int a, b, c;
                a = (eci - 40) / 12769;
                b = ((eci - 40) - (12769 * a)) / 113;
                c = (eci - 40) - (12769 * a) - (113 * b);

                codeword_array[ap++] = a + 40;
                codeword_array[ap++] = b;
                codeword_array[ap++] = c;
            }
        }
    }

    while (position < length) {
        /* Step A */
        if (last_seg && (position == length - 2) && (inside_macro != 0) && (inside_macro != 100)) {
            /* inside_macro only gets set to 97, 98 or 99 if the last two characters are RS/EOT */
            position += 2;
            if (debug_print) fputs("A ", stdout);
            continue;
        }

        /* Step B */
        if (last_seg && (position == length - 1) && (inside_macro == 100)) {
            /* inside_macro only gets set to 100 if the last character is EOT */
            position++;
            if (debug_print) fputs("B ", stdout);
            continue;
        }

        if (encoding_mode == 'C') {

            /* Step C2 */
            if (dc_seventeen_ten(source, length, position)) {
                codeword_array[ap++] = 100; /* (17)...(10) */
                codeword_array[ap++] = to_int(source + position + 2, 2);
                codeword_array[ap++] = to_int(source + position + 4, 2);
                codeword_array[ap++] = to_int(source + position + 6, 2);
                position += 10;
                if (debug_print) fputs("C2/1 ", stdout);
                continue;
            }

            if (dc_datum_c(source, length, position) || (gs1 && source[position] == '\x1D')) {
                if (source[position] == '\x1D') {
                    codeword_array[ap++] = 107; /* FNC1 */
                    position++;
                } else {
                    codeword_array[ap++] = to_int(source + position, 2);
                    position += 2;
                }
                if (debug_print) fputs("C2/2 ", stdout);
                continue;
            }

            /* Step C3 */
            if (dc_binary(source, length, position)) {
                /* cnt_digits(position + 1) > 0 */
                if (position + 1 < length && z_isdigit(source[position + 1])) {
                    if ((source[position] - 128) < 32) {
                        codeword_array[ap++] = 110; /* Upper Shift A */
                        codeword_array[ap++] = source[position] - 128 + 64;
                    } else {
                        codeword_array[ap++] = 111; /* Upper Shift B */
                        codeword_array[ap++] = source[position] - 128 - 32;
                    }
                    position++;
                } else {
                    codeword_array[ap++] = 112; /* Bin Latch */
                    encoding_mode = 'X';
                }
                if (debug_print) fputs("C3 ", stdout);
                continue;
            }

            /* Step C4 */
            {
                const int m = dc_ahead_a(source, length, position);
                const int n = dc_ahead_b(source, length, position, &nx);
                if (m > n) {
                    codeword_array[ap++] = 101; /* Latch A */
                    encoding_mode = 'A';
                } else {
                    if (nx >= 1 && nx <= 4) {
                        codeword_array[ap++] = 101 + nx; /* nx Shift B */

                        for (i = 0; i < nx; i++) {
                            if (source[position] >= 32) {
                                codeword_array[ap++] = source[position] - 32;
                            } else if (source[position] == 13) { /* CR/LF */
                                codeword_array[ap++] = 96;
                                position++;
                            } else {
                                switch (source[position]) {
                                    case 9: codeword_array[ap++] = 97; break; /* HT */
                                    case 28: codeword_array[ap++] = 98; break; /* FS */
                                    case 29: codeword_array[ap++] = 99; break; /* GS */
                                    case 30: codeword_array[ap++] = 100; break; /* RS */
                                }
                            }
                            position++;
                        }
                    } else {
                        codeword_array[ap++] = 106; /* Latch B */
                        encoding_mode = 'B';
                    }
                }
                if (debug_print) fputs("C4 ", stdout);
                continue;
            }
        } /* encoding_mode == 'C' */

        if (encoding_mode == 'B') {
            /* Step D1 */
            const int n = dc_try_c(source, length, position);

            if (n >= 2) {
                if (n <= 4) {
                    codeword_array[ap++] = 103 + (n - 2); /* nx Shift C */
                    for (i = 0; i < n; i++) {
                        codeword_array[ap++] = to_int(source + position, 2);
                        position += 2;
                    }
                } else {
                    codeword_array[ap++] = 106; /* Latch C */
                    encoding_mode = 'C';
                }
                if (debug_print) fputs("D1 ", stdout);
                continue;
            }

            /* Step D2 */
            if (gs1 && source[position] == '\x1D') {
                codeword_array[ap++] = 107; /* FNC1 */
                position++;
                if (debug_print) fputs("D2/1 ", stdout);
                continue;
            }

            if (dc_datum_b(source, length, position)) {
                int done = 0;

                if ((source[position] >= 32) && (source[position] <= 127)) {
                    codeword_array[ap++] = source[position] - 32;
                    done = 1;

                } else if (source[position] == 13) {
                    /* CR/LF */
                    codeword_array[ap++] = 96;
                    position++;
                    done = 1;

                } else if (!first_seg || position != 0) {
                    /* HT, FS, GS and RS in the first data position would be interpreted as a macro
                     * (see table 2) */
                    switch (source[position]) {
                        case 9: codeword_array[ap++] = 97; break; /* HT */
                        case 28: codeword_array[ap++] = 98; break; /* FS */
                        case 29: codeword_array[ap++] = 99; break; /* GS */
                        case 30: codeword_array[ap++] = 100; break; /* RS */
                    }
                    done = 1;
                }

                if (done == 1) {
                    position++;
                    if (debug_print) fputs("D2/2 ", stdout);
                    continue;
                }
            }

            /* Step D3 */
            if (dc_binary(source, length, position)) {
                if (dc_datum_b(source, length, position + 1)) {
                    if ((source[position] - 128) < 32) {
                        codeword_array[ap++] = 110; /* Bin Shift A */
                        codeword_array[ap++] = source[position] - 128 + 64;
                    } else {
                        codeword_array[ap++] = 111; /* Bin Shift B */
                        codeword_array[ap++] = source[position] - 128 - 32;
                    }
                    position++;
                } else {
                    codeword_array[ap++] = 112; /* Bin Latch */
                    encoding_mode = 'X';
                }
                if (debug_print) fputs("D3 ", stdout);
                continue;
            }

            /* Step D4 */
            if (dc_ahead_a(source, length, position) == 1) {
                codeword_array[ap++] = 101; /* Shift A */
                if (source[position] < 32) {
                    codeword_array[ap++] = source[position] + 64;
                } else {
                    codeword_array[ap++] = source[position] - 32;
                }
                position++;
            } else {
                codeword_array[ap++] = 102; /* Latch A */
                encoding_mode = 'A';
            }
            if (debug_print) fputs("D4 ", stdout);
            continue;
        } /* encoding_mode == 'B' */

        if (encoding_mode == 'A') {
            /* Step E1 */
            const int n = dc_try_c(source, length, position);
            if (n >= 2) {
                if (n <= 4) {
                    codeword_array[ap++] = 103 + (n - 2); /* nx Shift C */
                    for (i = 0; i < n; i++) {
                        codeword_array[ap++] = to_int(source + position, 2);
                        position += 2;
                    }
                } else {
                    codeword_array[ap++] = 106; /* Latch C */
                    encoding_mode = 'C';
                }
                if (debug_print) fputs("E1 ", stdout);
                continue;
            }

            /* Step E2 */
            if (gs1 && source[position] == '\x1D') {
                /* Note: this branch probably never reached as no reason to be in Code Set A for GS1 data */
                codeword_array[ap++] = 107; /* FNC1 */
                position++;
                if (debug_print) fputs("E2/1 ", stdout);
                continue;
            }
            if (dc_datum_a(source, length, position)) {
                if (source[position] < 32) {
                    codeword_array[ap++] = source[position] + 64;
                } else {
                    codeword_array[ap++] = source[position] - 32;
                }
                position++;
                if (debug_print) fputs("E2/2 ", stdout);
                continue;
            }

            /* Step E3 */
            if (dc_binary(source, length, position)) {
                if (dc_datum_a(source, length, position + 1)) {
                    if ((source[position] - 128) < 32) {
                        codeword_array[ap++] = 110; /* Bin Shift A */
                        codeword_array[ap++] = source[position] - 128 + 64;
                    } else {
                        codeword_array[ap++] = 111; /* Bin Shift B */
                        codeword_array[ap++] = source[position] - 128 - 32;
                    }
                    position++;
                } else {
                    codeword_array[ap++] = 112; /* Bin Latch */
                    encoding_mode = 'X';
                }
                if (debug_print) fputs("E3 ", stdout);
                continue;
            }

            /* Step E4 */
            dc_ahead_b(source, length, position, &nx);

            if (nx >= 1 && nx <= 6) {
                codeword_array[ap++] = 95 + nx; /* nx Shift B */
                for (i = 0; i < nx; i++) {
                    if (source[position] >= 32) {
                        codeword_array[ap++] = source[position] - 32;
                    } else if (source[position] == 13) { /* CR/LF */
                        codeword_array[ap++] = 96;
                        position++;
                    } else {
                        switch (source[position]) {
                            case 9: codeword_array[ap++] = 97; break; /* HT */
                            case 28: codeword_array[ap++] = 98; break; /* FS */
                            case 29: codeword_array[ap++] = 99; break; /* GS */
                            case 30: codeword_array[ap++] = 100; break; /* RS */
                        }
                    }
                    position++;
                }
            } else {
                codeword_array[ap++] = 102; /* Latch B */
                encoding_mode = 'B';
            }
            if (debug_print) fputs("E4 ", stdout);
            continue;
        } /* encoding_mode == 'A' */

        /* Step F1 */
        if (encoding_mode == 'X') {
            const int n = dc_try_c(source, length, position);

            if (n >= 2) {
                ap = dc_empty_bin_buf(codeword_array, ap, &bin_buf, &bin_buf_size);

                if (n <= 7) {
                    codeword_array[ap++] = 101 + n; /* Interrupt for nx Shift C */
                    for (i = 0; i < n; i++) {
                        codeword_array[ap++] = to_int(source + position, 2);
                        position += 2;
                    }
                } else {
                    codeword_array[ap++] = 111; /* Terminate with Latch to C */
                    encoding_mode = 'C';
                }
                if (debug_print) fputs("F1 ", stdout);
                continue;
            }

            /* Step F2 */
            /* Section 5.2.1.1 para D.2.i states:
             * "Groups of six codewords, each valued between 0 and 102, are radix converted from
             * base 103 into five base 259 values..."
             */
            if (dc_binary(source, length, position)
                    || dc_binary(source, length, position + 1)
                    || dc_binary(source, length, position + 2)
                    || dc_binary(source, length, position + 3)) {
                ap = dc_append_to_bin_buf(codeword_array, ap, source[position], &bin_buf, &bin_buf_size);
                position++;
                if (debug_print) fputs("F2 ", stdout);
                continue;
            }

            /* Step F3 */
            ap = dc_empty_bin_buf(codeword_array, ap, &bin_buf, &bin_buf_size); /* Empty binary buffer */

            if (dc_ahead_a(source, length, position) > dc_ahead_b(source, length, position, NULL)) {
                codeword_array[ap++] = 109; /* Terminate with Latch to A */
                encoding_mode = 'A';
            } else {
                codeword_array[ap++] = 110; /* Terminate with Latch to B */
                encoding_mode = 'B';
            }
            if (debug_print) fputs("F3 ", stdout);
        } /* encoding_mode == 'X' */
    }

    if (last_seg) {
        if (encoding_mode == 'X' && bin_buf_size != 0) {
            /* Empty binary buffer */
            ap = dc_empty_bin_buf(codeword_array, ap, &bin_buf, &bin_buf_size);
        }

        if (symbol->structapp.count) {
            int sp = 0;
            /* Need Code Set A or B - choosing A here (TEC-IT chooses B) */
            if (encoding_mode == 'C') {
                structapp_array[sp++] = 101; /* Latch A */
            } else if (encoding_mode == 'X') {
                structapp_array[sp++] = 109; /* Terminate with Latch A */
            }
            if (symbol->structapp.index < 10) {
                structapp_array[sp++] = 16 + symbol->structapp.index; /* '0' + index for 1-9 */
            } else  {
                structapp_array[sp++] = 33 + symbol->structapp.index - 10; /* 'A' + index for A-Z */
            }
            if (symbol->structapp.count < 10) {
                structapp_array[sp++] = 16 + symbol->structapp.count; /* '0' + count for 1-9 */
            } else  {
                structapp_array[sp++] = 33 + symbol->structapp.count - 10; /* 'A' + count for A-Z */
            }
            structapp_array[sp++] = 108; /* FNC2 as last codeword */
            *p_structapp_size = sp;
        }
    }

    if (debug_print) {
        fputc('\n', stdout);
    }

    *p_encoding_mode = encoding_mode;
    *p_inside_macro = inside_macro;
    *p_bin_buf = bin_buf;
    *p_bin_buf_size = bin_buf_size;

    return ap;
}

/* Call `dc_encode_message()` for each segment */
static int dc_encode_message_segs(struct zint_symbol *symbol, const struct zint_seg segs[], const int seg_count,
            unsigned char *codeword_array, int *p_binary_finish, unsigned char structapp_array[],
            int *p_structapp_size) {
    int i;

    int last_EOT = 0;
    int last_RSEOT = 0;
    int ap = 0;
    char encoding_mode = 'C';
    int inside_macro = 0;
    uint64_t bin_buf = 0;
    int bin_buf_size = 0;

    const struct zint_seg *last_seg = &segs[seg_count - 1];

    last_EOT = last_seg->source[last_seg->length - 1] == 4; /* EOT */
    if (last_EOT && last_seg->length > 1) {
        last_RSEOT = last_seg->source[last_seg->length - 2] == 30; /* RS */
    }

    for (i = 0; i < seg_count; i++) {
        ap = dc_encode_message(symbol, segs[i].source, segs[i].length, segs[i].eci, i == seg_count - 1 /*last_seg*/,
                last_EOT, last_RSEOT, ap, codeword_array, &encoding_mode, &inside_macro, &bin_buf, &bin_buf_size,
                structapp_array, p_structapp_size);
    }

    *p_binary_finish = encoding_mode == 'X';

    return ap + *p_structapp_size;
}

/* Convert codewords to binary data stream */
static int dc_make_dotstream(const unsigned char masked_array[], const int array_length, char dot_stream[]) {
    int i;
    int bp = 0;

    /* Mask value is encoded as two dots */
    bp = bin_append_posn(masked_array[0], 2, dot_stream, bp);

    /* The rest of the data uses 9-bit dot patterns from Annex C */
    for (i = 1; i < array_length; i++) {
        bp = bin_append_posn(dc_dot_patterns[masked_array[i]], 9, dot_stream, bp);
    }

    return bp;
}

/* Determines if a given dot is a reserved corner dot
 * to be used by one of the last six bits
 */
static int dc_is_corner(const int column, const int row, const int width, const int height) {

    /* Top Left */
    if ((column == 0) && (row == 0)) {
        return 1;
    }

    /* Top Right */
    if (height & 1) {
        if (((column == width - 2) && (row == 0))
                || ((column == width - 1) && (row == 1))) {
            return 1;
        }
    } else {
        if ((column == width - 1) && (row == 0)) {
            return 1;
        }
    }

    /* Bottom Left */
    if (height & 1) {
        if ((column == 0) && (row == height - 1)) {
            return 1;
        }
    } else {
        if (((column == 0) && (row == height - 2))
                || ((column == 1) && (row == height - 1))) {
            return 1;
        }
    }

    /* Bottom Right */
    if (((column == width - 2) && (row == height - 1))
            || ((column == width - 1) && (row == height - 2))) {
        return 1;
    }

    return 0;
}

/* Place the dots in the symbol*/
static void dc_fold_dotstream(const char dot_stream[], const int width, const int height, char dot_array[]) {
    int column, row;
    int position = 0;

    if (height & 1) {
        /* Horizontal folding */
        for (row = 0; row < height; row++) {
            for (column = 0; column < width; column++) {
                if (!((column + row) & 1)) {
                    if (dc_is_corner(column, row, width, height)) {
                        dot_array[(row * width) + column] = 'C';
                    } else {
                        dot_array[((height - row - 1) * width) + column] = dot_stream[position++];
                    }
                } else {
                    dot_array[((height - row - 1) * width) + column] = ' '; /* Non-data position */
                }
            }
        }

        /* Corners */
        dot_array[width - 2] = dot_stream[position++];
        dot_array[(height * width) - 2] = dot_stream[position++];
        dot_array[(width * 2) - 1] = dot_stream[position++];
        dot_array[((height - 1) * width) - 1] = dot_stream[position++];
        dot_array[0] = dot_stream[position++];
        dot_array[(height - 1) * width] = dot_stream[position];
    } else {
        /* Vertical folding */
        for (column = 0; column < width; column++) {
            for (row = 0; row < height; row++) {
                if (!((column + row) & 1)) {
                    if (dc_is_corner(column, row, width, height)) {
                        dot_array[(row * width) + column] = 'C';
                    } else {
                        dot_array[(row * width) + column] = dot_stream[position++];
                    }
                } else {
                    dot_array[(row * width) + column] = ' '; /* Non-data position */
                }
            }
        }

        /* Corners */
        dot_array[((height - 1) * width) - 1] = dot_stream[position++];
        dot_array[(height - 2) * width] = dot_stream[position++];
        dot_array[(height * width) - 2] = dot_stream[position++];
        dot_array[((height - 1) * width) + 1] = dot_stream[position++];
        dot_array[width - 1] = dot_stream[position++];
        dot_array[0] = dot_stream[position];
    }
}

static void dc_apply_mask(const int mask, const int data_length, unsigned char *masked_codeword_array,
            const unsigned char *codeword_array, const int ecc_length) {
    int weight = 0;
    int j;

    assert(mask >= 0 && mask <= 3); /* Suppress clang-analyzer taking default branch */
    assert(data_length > 0); /* Suppress clang-analyzer-core.UndefinedBinaryOperatorResult */
    switch (mask) {
        case 0:
            masked_codeword_array[0] = 0;
            for (j = 0; j < data_length; j++) {
                masked_codeword_array[j + 1] = codeword_array[j];
            }
            break;
        case 1:
            masked_codeword_array[0] = 1;
            for (j = 0; j < data_length; j++) {
                masked_codeword_array[j + 1] = (weight + codeword_array[j]) % 113;
                weight += 3;
            }
            break;
        case 2:
            masked_codeword_array[0] = 2;
            for (j = 0; j < data_length; j++) {
                masked_codeword_array[j + 1] = (weight + codeword_array[j]) % 113;
                weight += 7;
            }
            break;
        case 3:
            masked_codeword_array[0] = 3;
            for (j = 0; j < data_length; j++) {
                masked_codeword_array[j + 1] = (weight + codeword_array[j]) % 113;
                weight += 17;
            }
            break;
    }

    dc_rsencode(data_length + 1, ecc_length, masked_codeword_array);
}

static void dc_force_corners(const int width, const int height, char *dot_array) {
    if (width & 1) {
        /* "Vertical" symbol */
        dot_array[0] = '1';
        dot_array[width - 1] = '1';
        dot_array[(height - 2) * width] = '1';
        dot_array[((height - 1) * width) - 1] = '1';
        dot_array[((height - 1) * width) + 1] = '1';
        dot_array[(height * width) - 2] = '1';
    } else {
        /* "Horizontal" symbol */
        dot_array[0] = '1';
        dot_array[width - 2] = '1';
        dot_array[(2 * width) - 1] = '1';
        dot_array[((height - 1) * width) - 1] = '1';
        dot_array[(height - 1) * width] = '1';
        dot_array[(height * width) - 2] = '1';
    }
}

INTERNAL int dotcode(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int warn_number = 0;
    int i, j, k;
    int jc, n_dots;
    int data_length, ecc_length;
    int min_dots, min_area;
    int height, width;
    int mask_score[8];
    int user_mask;
    int dot_stream_length;
    int high_score, best_mask;
    int binary_finish = 0;
    unsigned char structapp_array[5];
    int structapp_size = 0;
    int padding_dots;
    const int gs1 = (symbol->input_mode & 0x07) == GS1_MODE;
    const int debug_print = (symbol->debug & ZINT_DEBUG_PRINT);
    /* Allow 4 codewords per input + 2 (FNC) + seg_count * 4 (ECI) + 2 (special char 1st position)
       + 5 (Structured Append) + 10 (PAD) */
    const int codeword_array_len = segs_length(segs, seg_count) * 4 + 2 + seg_count * 4 + 2 + 5 + 10;
    unsigned char *codeword_array = (unsigned char *) z_alloca(codeword_array_len);
    char *dot_stream;
    char *dot_array;
    unsigned char *masked_codeword_array;

    if (symbol->eci > 811799) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 525, "ECI code '%d' out of range (0 to 811799)",
                        symbol->eci);
    }

    user_mask = (symbol->option_3 >> 8) & 0x0F; /* User mask is mask + 1, so >= 1 and <= 8 */
    if (user_mask > 8) {
        user_mask = 0; /* Ignore */
    }

    if (symbol->structapp.count) {
        if (symbol->structapp.count < 2 || symbol->structapp.count > 35) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 730,
                            "Structured Append count '%d' out of range (2 to 35)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 731,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }
        if (symbol->structapp.id[0]) {
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 732, "Structured Append ID not available for DotCode");
        }
    }

    /* GS1 General Specifications 22.0 section 5.8.2 says Structured Append and ECIs not supported
       for GS1 DotCode so check and return ZINT_WARN_NONCOMPLIANT if either true */
    if (gs1 && warn_number == 0) {
        for (i = 0; i < seg_count; i++) {
            if (segs[i].eci) {
                warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 733,
                                        "Using ECI in GS1 mode not supported by GS1 standards");
                break;
            }
        }
        if (warn_number == 0 && symbol->structapp.count) {
            warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 734,
                                    "Using Structured Append in GS1 mode not supported by GS1 standards");
        }
    }

    data_length = dc_encode_message_segs(symbol, segs, seg_count, codeword_array, &binary_finish, structapp_array,
                    &structapp_size);

    /* Suppresses clang-tidy clang-analyzer-core.UndefinedBinaryOperatorResult/uninitialized.ArraySubscript
     * warnings */
    assert(data_length > 0);

    ecc_length = 3 + (data_length / 2);

    min_dots = 9 * (data_length + 3 + (data_length / 2)) + 2;
    min_area = min_dots * 2;

    if (symbol->option_2 == 0) {
        /* Automatic sizing */
        /* Following Rule 3 (Section 5.2.2) and applying a recommended width to height ratio 3:2 */
        /* Eliminates under sized symbols */

        float h = (float) (sqrt(min_area * 0.666));
        float w = (float) (sqrt(min_area * 1.5));

        height = (int) h;
        width = (int) w;

        if (((width + height) & 1) == 1) {
            if ((width * height) < min_area) {
                width++;
                height++;
            }
        } else {
            if ((h * width) < (w * height)) {
                width++;
                if ((width * height) < min_area) {
                    width--;
                    height++;
                    if ((width * height) < min_area) {
                        width += 2;
                    }
                }
            } else {
                height++;
                if ((width * height) < min_area) {
                    width++;
                    height--;
                    if ((width * height) < min_area) {
                        height += 2;
                    }
                }
            }
        }

    } else {
        /* User defined width */
        /* Eliminates under sized symbols */

        width = symbol->option_2;
        height = (min_area + (width - 1)) / width;

        if (!((width + height) & 1)) {
            height++;
        }
    }

    if (debug_print) {
        printf("Width = %d, Height = %d\n", width, height);
    }

    if ((height > 200) || (width > 200)) {
        if (height > 200 && width > 200) {
            errtxtf(0, symbol, 526, "Symbol size '%1$dx%2$d' (WxH) is too large", width, height);
        } else {
            errtxtf(0, symbol, 528, "Symbol %1$s '%2$d' is too large",
                    width > 200 ? "width" : "height", width > 200 ? width : height);
        }
        return ZINT_ERROR_INVALID_OPTION;
    }

    if ((height < 5) || (width < 5)) {
        if (height < 5 && width < 5) { /* Won't happen as if width < 5, min height is 19 */
            errtxtf(0, symbol, 527, "Symbol size '%1$dx%2$d' (WxH) is too small", width, height); /* Not reached */
        } else {
            errtxtf(0, symbol, 529, "Symbol %1$s '%2$d' is too small",
                        width < 5 ? "width" : "height", width < 5 ? width : height);
        }
        return ZINT_ERROR_INVALID_OPTION;
    }

    n_dots = (height * width) / 2;

    dot_stream = (char *) z_alloca(height * width * 3);
    dot_array = (char *) z_alloca(width * height);

    /* Add pad characters */
    padding_dots = n_dots - min_dots; /* get the number of free dots available for padding */

    if (padding_dots >= 9) {
        int is_first = 1; /* first padding character flag */
        int padp = data_length - structapp_size;
        while (padding_dots >= 9) {
            if (padding_dots < 18 && (data_length & 1) == 0) {
                padding_dots -= 9;
            } else if (padding_dots >= 18) {
                if ((data_length & 1) == 0) {
                    padding_dots -= 9;
                } else {
                    padding_dots -= 18;
                }
            } else {
                break; /* not enough padding dots left for padding */
            }
            if (is_first && binary_finish) {
                codeword_array[padp++] = 109;
            } else {
                codeword_array[padp++] = 106;
            }

            data_length++;
            is_first = 0;
        }
        if (structapp_size) {
            if (structapp_array[0] == 109) { /* Binary latch no longer valid */
                structapp_array[0] = 106;
            }
            for (i = 0; i < structapp_size; i++) {
                codeword_array[padp++] = structapp_array[i];
            }
        }
    } else if (structapp_size) {
        data_length -= structapp_size;
        for (i = 0; i < structapp_size; i++) {
            codeword_array[data_length++] = structapp_array[i];
        }
    }

    if (debug_print) {
        printf("Codeword length = %d, ECC length = %d\n", data_length, ecc_length);
        fputs("Codewords:", stdout);
        for (i = 0; i < data_length; i++) {
            printf(" %d", codeword_array[i]);
        }
        fputc('\n', stdout);
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump(symbol, codeword_array, data_length);
    }
#endif

    ecc_length = 3 + (data_length / 2);

    masked_codeword_array = (unsigned char *) z_alloca(data_length + 1 + ecc_length);

    if (user_mask) {
        best_mask = user_mask - 1;
        if (debug_print) {
            printf("Applying mask %d (specified)\n", best_mask);
        }
    } else {
        /* Evaluate data mask options */
        for (i = 0; i < 4; i++) {

            dc_apply_mask(i, data_length, masked_codeword_array, codeword_array, ecc_length);

            dot_stream_length = dc_make_dotstream(masked_codeword_array, (data_length + ecc_length + 1), dot_stream);

            /* Add pad bits */
            for (jc = dot_stream_length; jc < n_dots; jc++) {
                dot_stream[dot_stream_length++] = '1';
            }

            dc_fold_dotstream(dot_stream, width, height, dot_array);

            mask_score[i] = dc_score_array(dot_array, height, width);

            if (debug_print) {
                printf("Mask %d score is %d\n", i, mask_score[i]);
            }
        }

        high_score = mask_score[0];
        best_mask = 0;

        for (i = 1; i < 4; i++) {
            if (mask_score[i] >= high_score) {
                high_score = mask_score[i];
                best_mask = i;
            }
        }

        /* Re-evaluate using forced corners if needed */
        if (high_score <= (height * width) / 2) {
            if (debug_print) {
                printf("High score %d <= %d (height * width) / 2\n", high_score, (height * width) / 2);
            }

            for (i = 0; i < 4; i++) {

                dc_apply_mask(i, data_length, masked_codeword_array, codeword_array, ecc_length);

                dot_stream_length = dc_make_dotstream(masked_codeword_array, (data_length + ecc_length + 1),
                                        dot_stream);

                /* Add pad bits */
                for (jc = dot_stream_length; jc < n_dots; jc++) {
                    dot_stream[dot_stream_length++] = '1';
                }

                dc_fold_dotstream(dot_stream, width, height, dot_array);

                dc_force_corners(width, height, dot_array);

                mask_score[i + 4] = dc_score_array(dot_array, height, width);

                if (debug_print) {
                    printf("Mask %d score is %d\n", i + 4, mask_score[i + 4]);
                }
            }

            for (i = 4; i < 8; i++) {
                if (mask_score[i] >= high_score) {
                    high_score = mask_score[i];
                    best_mask = i;
                }
            }
        }

        if (debug_print) {
            printf("Applying mask %d, high_score %d\n", best_mask, high_score);
        }
    }

    /* Apply best mask */
    dc_apply_mask(best_mask % 4, data_length, masked_codeword_array, codeword_array, ecc_length);

    if (debug_print) {
        printf("Masked codewords (%d):", data_length);
        for (i = 1; i < data_length + 1; i++) {
            printf(" [%d]", masked_codeword_array[i]);
        }
        fputc('\n', stdout);
        printf("Masked ECCs (%d):", ecc_length);
        for (i = data_length + 1; i < data_length + ecc_length + 1; i++) {
            printf(" [%d]", masked_codeword_array[i]);
        }
        fputc('\n', stdout);
    }

    dot_stream_length = dc_make_dotstream(masked_codeword_array, (data_length + ecc_length + 1), dot_stream);

    /* Add pad bits */
    for (jc = dot_stream_length; jc < n_dots; jc++) {
        dot_stream[dot_stream_length++] = '1';
    }
    if (debug_print) printf("Binary (%d): %.*s\n", dot_stream_length, dot_stream_length, dot_stream);

    dc_fold_dotstream(dot_stream, width, height, dot_array);

    if (best_mask >= 4) {
        dc_force_corners(width, height, dot_array);
    }

    /* Copy values to symbol */
    symbol->width = width;
    symbol->rows = height;

    for (k = 0; k < height; k++) {
        for (j = 0; j < width; j++) {
            if (dot_array[(k * width) + j] == '1') {
                set_module(symbol, k, j);
            }
        }
        symbol->row_height[k] = 1;
    }
    symbol->height = height;

    symbol->output_options |= BARCODE_DOTTY_MODE;

    return warn_number;
}

/* vim: set ts=4 sw=4 et : */
