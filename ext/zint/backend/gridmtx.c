/*  gridmtx.c - Grid Matrix */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2024 Robin Stuart <rstuart114@gmail.com>

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

/* This file implements Grid Matrix as specified in
   AIM Global Document Number AIMD014 Rev. 1.63 Revised 9 Dec 2008 */

#include <stdio.h>
#include "common.h"
#include "reedsol.h"
#include "gridmtx.h"
#include "eci.h"

static const char EUROPIUM[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz ";
static const char EUROPIUM_UPR[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";
static const char EUROPIUM_LWR[] = "abcdefghijklmnopqrstuvwxyz ";

/* gm_define_mode() stuff */

/* Bits multiplied by this for costs, so as to be whole integer divisible by 2 and 3 */
#define GM_MULT 6

/* Non-digit numeral set, excluding EOL (carriage return/linefeed) */
static const char gm_numeral_nondigits[] = " +-.,";

/* Whether in numeral or not. If in numeral, *p_numeral_end is set to position after numeral,
 * and *p_numeral_cost is set to per-numeral cost */
static int gm_in_numeral(const unsigned int ddata[], const int length, const int in_posn,
            unsigned int *p_numeral_end, unsigned int *p_numeral_cost) {
    int i, digit_cnt, nondigit, nondigit_posn;

    if (in_posn < (int) *p_numeral_end) {
        return 1;
    }

    /* Attempt to calculate the average 'cost' of using numeric mode in number of bits (times GM_MULT) */
    /* Also ensures that numeric mode is not selected when it cannot be used: for example in
       a string which has "2.2.0" (cannot have more than one non-numeric character for each
       block of three numeric characters) */
    for (i = in_posn, digit_cnt = 0, nondigit = 0, nondigit_posn = 0; i < length && i < in_posn + 4 && digit_cnt < 3;
            i++) {
        if (z_isdigit(ddata[i])) {
            digit_cnt++;
        } else if (posn(gm_numeral_nondigits, (const char) ddata[i]) != -1) {
            if (nondigit) {
                break;
            }
            nondigit = 1;
            nondigit_posn = i;
        } else if (i < length - 1 && ddata[i] == 13 && ddata[i + 1] == 10) {
            if (nondigit) {
                break;
            }
            i++;
            nondigit = 2;
            nondigit_posn = i;
        } else {
            break;
        }
    }
    if (digit_cnt == 0) { /* Must have at least one digit */
        *p_numeral_end = 0;
        return 0;
    }
    if (nondigit && nondigit_posn == i - 1) { /* Non-digit can't be at end */
        nondigit = 0;
    }
    *p_numeral_end = in_posn + digit_cnt + nondigit;
    /* Calculate per-numeral cost where 120 == (10 + 10) * GM_MULT, 60 == 10 * GM_MULT */
    if (digit_cnt == 3) {
        *p_numeral_cost = nondigit == 2 ? 24 /* (120 / 5) */ : nondigit == 1 ? 30 /* (120 / 4) */ : 20 /* (60 / 3) */;
    } else if (digit_cnt == 2) {
        *p_numeral_cost = nondigit == 2 ? 30 /* (120 / 4) */ : nondigit == 1 ? 40 /* (120 / 3) */ : 30 /* (60 / 2) */;
    } else {
        *p_numeral_cost = nondigit == 2 ? 40 /* (120 / 3) */ : nondigit == 1 ? 60 /* (120 / 2) */ : 60 /* (60 / 1) */;
    }
    return 1;
}

/* Encoding modes */
#define GM_CHINESE  'H'
#define GM_NUMBER   'N'
#define GM_LOWER    'L'
#define GM_UPPER    'U'
#define GM_MIXED    'M'
#define GM_BYTE     'B'
/* Note Control is a submode of Lower, Upper and Mixed modes */

/* Indexes into mode_types array */
#define GM_H   0 /* Chinese (Hanzi) */
#define GM_N   1 /* Numeral */
#define GM_L   2 /* Lower case */
#define GM_U   3 /* Upper case */
#define GM_M   4 /* Mixed */
#define GM_B   5 /* Byte */

#define GM_NUM_MODES 6

/* Calculate optimized encoding modes. Adapted from Project Nayuki */
/* Copyright (c) Project Nayuki. (MIT License) See qr.c for detailed notice */
static void gm_define_mode(char *mode, const unsigned int ddata[], const int length, const int debug_print) {
    /* Must be in same order as GM_H etc */
    static const char mode_types[] = { GM_CHINESE, GM_NUMBER, GM_LOWER, GM_UPPER, GM_MIXED, GM_BYTE, '\0' };

    /* Initial mode costs */
    static const unsigned int head_costs[GM_NUM_MODES] = {
    /*  H            N (+pad prefix)    L            U            M            B (+byte count) */
        4 * GM_MULT, (4 + 2) * GM_MULT, 4 * GM_MULT, 4 * GM_MULT, 4 * GM_MULT, (4 + 9) * GM_MULT
    };

    /* Cost of switching modes from k to j - see AIMD014 Rev. 1.63 Table 9 – Type conversion codes */
    static const unsigned char switch_costs[GM_NUM_MODES][GM_NUM_MODES] = {
        /*      H             N                   L             U             M             B  */
        /*H*/ {            0, (13 + 2) * GM_MULT, 13 * GM_MULT, 13 * GM_MULT, 13 * GM_MULT, (13 + 9) * GM_MULT },
        /*N*/ { 10 * GM_MULT,                  0, 10 * GM_MULT, 10 * GM_MULT, 10 * GM_MULT, (10 + 9) * GM_MULT },
        /*L*/ {  5 * GM_MULT,  (5 + 2) * GM_MULT,            0,  5 * GM_MULT,  7 * GM_MULT,  (7 + 9) * GM_MULT },
        /*U*/ {  5 * GM_MULT,  (5 + 2) * GM_MULT,  5 * GM_MULT,            0,  7 * GM_MULT,  (7 + 9) * GM_MULT },
        /*M*/ { 10 * GM_MULT, (10 + 2) * GM_MULT, 10 * GM_MULT, 10 * GM_MULT,            0, (10 + 9) * GM_MULT },
        /*B*/ {  4 * GM_MULT,  (4 + 2) * GM_MULT,  4 * GM_MULT,  4 * GM_MULT,  4 * GM_MULT,                  0 },
    };

    /* Final end-of-data cost - see AIMD014 Rev. 1.63 Table 9 – Type conversion codes */
    static const unsigned char eod_costs[GM_NUM_MODES] = {
    /*  H             N             L            U            M             B  */
        13 * GM_MULT, 10 * GM_MULT, 5 * GM_MULT, 5 * GM_MULT, 10 * GM_MULT, 4 * GM_MULT
    };

    unsigned int numeral_end = 0, numeral_cost = 0, byte_count = 0; /* State */
    int double_byte, space, numeric, lower, upper, control, double_digit, eol;

    int i, j, k;
    unsigned int min_cost;
    char cur_mode;
    unsigned int prev_costs[GM_NUM_MODES];
    unsigned int cur_costs[GM_NUM_MODES];
    char (*char_modes)[GM_NUM_MODES] = (char (*)[GM_NUM_MODES]) z_alloca(GM_NUM_MODES * length);

    /* char_modes[i][j] represents the mode to encode the code point at index i such that the final segment
       ends in mode_types[j] and the total number of bits is minimized over all possible choices */
    memset(char_modes, 0, length * GM_NUM_MODES);

    /* At the beginning of each iteration of the loop below, prev_costs[j] is the minimum number of 1/6 (1/XX_MULT)
     * bits needed to encode the entire string prefix of length i, and end in mode_types[j] */
    memcpy(prev_costs, head_costs, GM_NUM_MODES * sizeof(unsigned int));

    /* Calculate costs using dynamic programming */
    for (i = 0; i < length; i++) {
        memset(cur_costs, 0, GM_NUM_MODES * sizeof(unsigned int));

        space = numeric = lower = upper = control = double_digit = eol = 0;

        double_byte = ddata[i] > 0xFF;
        if (!double_byte) {
            space = ddata[i] == ' ';
            if (!space) {
                numeric = z_isdigit(ddata[i]);
                if (!numeric) {
                    lower = z_islower(ddata[i]);
                    if (!lower) {
                        upper = z_isupper(ddata[i]);
                        if (!upper) {
                            control = ddata[i] < 0x7F; /* Exclude DEL */
                            if (control && i + 1 < length) {
                                eol = ddata[i] == 13 && ddata[i + 1] == 10;
                            }
                        }
                    }
                } else if (i + 1 < length) {
                    double_digit = z_isdigit(ddata[i + 1]);
                }
            }
        }

        /* Hanzi mode can encode anything */
        cur_costs[GM_H] = prev_costs[GM_H] + (double_digit || eol ? 39 : 78); /* (6.5 : 13) * GM_MULT */
        char_modes[i][GM_H] = GM_CHINESE;

        /* Byte mode can encode anything */
        if (byte_count == 512 || (double_byte && byte_count == 511)) {
            cur_costs[GM_B] = head_costs[GM_B];
            if (double_byte && byte_count == 511) {
                cur_costs[GM_B] += 48; /* 8 * GM_MULT */
                double_byte = 0; /* Splitting double-byte so mark as single */
            }
            byte_count = 0;
        }
        cur_costs[GM_B] += prev_costs[GM_B] + (double_byte ? 96 : 48); /* (16 : 8) * GM_MULT */
        char_modes[i][GM_B] = GM_BYTE;
        byte_count += double_byte ? 2 : 1;

        if (gm_in_numeral(ddata, length, i, &numeral_end, &numeral_cost)) {
            cur_costs[GM_N] = prev_costs[GM_N] + numeral_cost;
            char_modes[i][GM_N] = GM_NUMBER;
        }

        if (control) {
            cur_costs[GM_L] = prev_costs[GM_L] + 78; /* (7 + 6) * GM_MULT */
            char_modes[i][GM_L] = GM_LOWER;
            cur_costs[GM_U] = prev_costs[GM_U] + 78; /* (7 + 6) * GM_MULT */
            char_modes[i][GM_U] = GM_UPPER;
            cur_costs[GM_M] = prev_costs[GM_M] + 96; /* (10 + 6) * GM_MULT */
            char_modes[i][GM_M] = GM_MIXED;
        } else {
            if (lower || space) {
                cur_costs[GM_L] = prev_costs[GM_L] + 30; /* 5 * GM_MULT */
                char_modes[i][GM_L] = GM_LOWER;
            }
            if (upper || space) {
                cur_costs[GM_U] = prev_costs[GM_U] + 30; /* 5 * GM_MULT */
                char_modes[i][GM_U] = GM_UPPER;
            }
            if (numeric || lower || upper || space) {
                cur_costs[GM_M] = prev_costs[GM_M] + 36; /* 6 * GM_MULT */
                char_modes[i][GM_M] = GM_MIXED;
            }
        }

        if (i == length - 1) { /* Add end of data costs if last character */
            for (j = 0; j < GM_NUM_MODES; j++) {
                if (char_modes[i][j]) {
                    cur_costs[j] += eod_costs[j];
                }
            }
        }

        /* Start new segment at the end to switch modes */
        for (j = 0; j < GM_NUM_MODES; j++) { /* To mode */
            for (k = 0; k < GM_NUM_MODES; k++) { /* From mode */
                if (j != k && char_modes[i][k]) {
                    const unsigned int new_cost = cur_costs[k] + switch_costs[k][j];
                    if (!char_modes[i][j] || new_cost < cur_costs[j]) {
                        cur_costs[j] = new_cost;
                        char_modes[i][j] = mode_types[k];
                    }
                }
            }
        }

        memcpy(prev_costs, cur_costs, GM_NUM_MODES * sizeof(unsigned int));
    }

    /* Find optimal ending mode */
    min_cost = prev_costs[0];
    cur_mode = mode_types[0];
    for (i = 1; i < GM_NUM_MODES; i++) {
        if (prev_costs[i] < min_cost) {
            min_cost = prev_costs[i];
            cur_mode = mode_types[i];
        }
    }

    /* Get optimal mode for each code point by tracing backwards */
    for (i = length - 1; i >= 0; i--) {
        j = posn(mode_types, cur_mode);
        cur_mode = char_modes[i][j];
        mode[i] = cur_mode;
    }

    if (debug_print) {
        printf("  Mode: %.*s\n", length, mode);
    }
}

/* Add the length indicator for byte encoded blocks */
static void gm_add_byte_count(char binary[], const int byte_count_posn, const int byte_count) {
    /* AIMD014 6.3.7: "Let L be the number of bytes of input data to be encoded in the 8-bit binary data set.
     * First output (L-1) as a 9-bit binary prefix to record the number of bytes..." */
    bin_append_posn(byte_count - 1, 9, binary, byte_count_posn);
}

/* Add a control character to the data stream */
static int gm_add_shift_char(char binary[], int bp, int shifty, const int debug_print) {
    int i;
    int glyph = 0;

    if (shifty < 32) {
        glyph = shifty;
    } else {
        for (i = 32; i < 64; i++) {
            if (gm_shift_set[i] == shifty) {
                glyph = i;
                break;
            }
        }
    }

    if (debug_print) {
        printf("SHIFT [%d] ", glyph);
    }

    bp = bin_append_posn(glyph, 6, binary, bp);

    return bp;
}

static int gm_encode(unsigned int ddata[], const int length, char binary[], const int eci, int *p_bp,
            const int debug_print) {
    /* Create a binary stream representation of the input data.
       7 sets are defined - Chinese characters, Numerals, Lower case letters, Upper case letters,
       Mixed numerals and latters, Control characters and 8-bit binary data */
    int sp = 0;
    int current_mode = 0;
    int last_mode;
    unsigned int glyph = 0;
    int c1, c2, done;
    int p = 0, ppos;
    int numbuf[3], punt = 0;
    int number_pad_posn = 0;
    int byte_count_posn = 0;
    int byte_count = 0;
    int shift;
    int bp = *p_bp;
    char *mode = (char *) z_alloca(length);

    if (eci != 0) {
        /* ECI assignment according to Table 8 */
        bp = bin_append_posn(12, 4, binary, bp); /* ECI */
        if (eci <= 1023) {
            bp = bin_append_posn(eci, 11, binary, bp);
        } else if (eci <= 32767) {
            bp = bin_append_posn(2, 2, binary, bp);
            bp = bin_append_posn(eci, 15, binary, bp);
        } else {
            bp = bin_append_posn(3, 2, binary, bp);
            bp = bin_append_posn(eci, 20, binary, bp);
        }
    }

    gm_define_mode(mode, ddata, length, debug_print);

    do {
        const int next_mode = mode[sp];

        if (next_mode != current_mode) {
            switch (current_mode) {
                case 0:
                    switch (next_mode) {
                        case GM_CHINESE: bp = bin_append_posn(1, 4, binary, bp);
                            break;
                        case GM_NUMBER: bp = bin_append_posn(2, 4, binary, bp);
                            break;
                        case GM_LOWER: bp = bin_append_posn(3, 4, binary, bp);
                            break;
                        case GM_UPPER: bp = bin_append_posn(4, 4, binary, bp);
                            break;
                        case GM_MIXED: bp = bin_append_posn(5, 4, binary, bp);
                            break;
                        case GM_BYTE: bp = bin_append_posn(6, 4, binary, bp);
                            break;
                    }
                    break;
                case GM_CHINESE:
                    switch (next_mode) {
                        case GM_NUMBER: bp = bin_append_posn(8161, 13, binary, bp);
                            break;
                        case GM_LOWER: bp = bin_append_posn(8162, 13, binary, bp);
                            break;
                        case GM_UPPER: bp = bin_append_posn(8163, 13, binary, bp);
                            break;
                        case GM_MIXED: bp = bin_append_posn(8164, 13, binary, bp);
                            break;
                        case GM_BYTE: bp = bin_append_posn(8165, 13, binary, bp);
                            break;
                    }
                    break;
                case GM_NUMBER:
                    /* add numeric block padding value */
                    switch (p) {
                        case 1: binary[number_pad_posn] = '1';
                            binary[number_pad_posn + 1] = '0';
                            break; /* 2 pad digits */
                        case 2: binary[number_pad_posn] = '0';
                            binary[number_pad_posn + 1] = '1';
                            break; /* 1 pad digits */
                        case 3: binary[number_pad_posn] = '0';
                            binary[number_pad_posn + 1] = '0';
                            break; /* 0 pad digits */
                    }
                    switch (next_mode) {
                        case GM_CHINESE: bp = bin_append_posn(1019, 10, binary, bp);
                            break;
                        case GM_LOWER: bp = bin_append_posn(1020, 10, binary, bp);
                            break;
                        case GM_UPPER: bp = bin_append_posn(1021, 10, binary, bp);
                            break;
                        case GM_MIXED: bp = bin_append_posn(1022, 10, binary, bp);
                            break;
                        case GM_BYTE: bp = bin_append_posn(1023, 10, binary, bp);
                            break;
                    }
                    break;
                case GM_LOWER:
                case GM_UPPER:
                    switch (next_mode) {
                        case GM_CHINESE: bp = bin_append_posn(28, 5, binary, bp);
                            break;
                        case GM_NUMBER: bp = bin_append_posn(29, 5, binary, bp);
                            break;
                        case GM_LOWER:
                        case GM_UPPER: bp = bin_append_posn(30, 5, binary, bp);
                            break;
                        case GM_MIXED: bp = bin_append_posn(124, 7, binary, bp);
                            break;
                        case GM_BYTE: bp = bin_append_posn(126, 7, binary, bp);
                            break;
                    }
                    break;
                case GM_MIXED:
                    switch (next_mode) {
                        case GM_CHINESE: bp = bin_append_posn(1009, 10, binary, bp);
                            break;
                        case GM_NUMBER: bp = bin_append_posn(1010, 10, binary, bp);
                            break;
                        case GM_LOWER: bp = bin_append_posn(1011, 10, binary, bp);
                            break;
                        case GM_UPPER: bp = bin_append_posn(1012, 10, binary, bp);
                            break;
                        case GM_BYTE: bp = bin_append_posn(1015, 10, binary, bp);
                            break;
                    }
                    break;
                case GM_BYTE:
                    /* add byte block length indicator */
                    gm_add_byte_count(binary, byte_count_posn, byte_count);
                    byte_count = 0;
                    switch (next_mode) {
                        case GM_CHINESE: bp = bin_append_posn(1, 4, binary, bp);
                            break;
                        case GM_NUMBER: bp = bin_append_posn(2, 4, binary, bp);
                            break;
                        case GM_LOWER: bp = bin_append_posn(3, 4, binary, bp);
                            break;
                        case GM_UPPER: bp = bin_append_posn(4, 4, binary, bp);
                            break;
                        case GM_MIXED: bp = bin_append_posn(5, 4, binary, bp);
                            break;
                    }
                    break;
            }
            if (debug_print) {
                switch (next_mode) {
                    case GM_CHINESE: fputs("CHIN ", stdout);
                        break;
                    case GM_NUMBER: fputs("NUMB ", stdout);
                        break;
                    case GM_LOWER: fputs("LOWR ", stdout);
                        break;
                    case GM_UPPER: fputs("UPPR ", stdout);
                        break;
                    case GM_MIXED: fputs("MIXD ", stdout);
                        break;
                    case GM_BYTE: fputs("BYTE ", stdout);
                        break;
                }
            }
        }
        last_mode = current_mode;
        current_mode = next_mode;

        switch (current_mode) {
            case GM_CHINESE:
                done = 0;
                if (ddata[sp] > 0xff) {
                    /* GB2312 character */
                    c1 = (ddata[sp] & 0xff00) >> 8;
                    c2 = ddata[sp] & 0xff;

                    if ((c1 >= 0xa1) && (c1 <= 0xa9)) {
                        glyph = (0x60 * (c1 - 0xa1)) + (c2 - 0xa0);
                    } else if ((c1 >= 0xb0) && (c1 <= 0xf7)) {
                        glyph = (0x60 * (c1 - 0xb0 + 9)) + (c2 - 0xa0);
                    }
                    done = 1; /* GB 2312 always within above ranges */
                    /* Note not using the unallocated glyphs 7776 to 8191 mentioned in AIMD014 section 6.3.1.2 */
                }
                if (!(done)) {
                    if (sp != (length - 1)) {
                        if ((ddata[sp] == 13) && (ddata[sp + 1] == 10)) {
                            /* End of Line */
                            glyph = 7776;
                            sp++;
                            done = 1;
                        }
                    }
                }
                if (!(done)) {
                    if (sp != (length - 1)) {
                        if (z_isdigit(ddata[sp]) && z_isdigit(ddata[sp + 1])) {
                            /* Two digits */
                            glyph = 8033 + (10 * (ddata[sp] - '0')) + (ddata[sp + 1] - '0');
                            sp++;
                            done = 1;
                        }
                    }
                }
                if (!(done)) {
                    /* Byte value */
                    glyph = 7777 + ddata[sp];
                }

                if (debug_print) {
                    printf("[%d] ", (int) glyph);
                }

                bp = bin_append_posn(glyph, 13, binary, bp);
                sp++;
                break;

            case GM_NUMBER:
                if (last_mode != current_mode) {
                    /* Reserve a space for numeric digit padding value (2 bits) */
                    number_pad_posn = bp;
                    bp = bin_append_posn(0, 2, binary, bp);
                }
                p = 0;
                ppos = -1;

                /* Numeric compression can also include certain combinations of
                   non-numeric character */

                numbuf[0] = '0';
                numbuf[1] = '0';
                numbuf[2] = '0';
                do {
                    if (z_isdigit(ddata[sp])) {
                        numbuf[p] = ddata[sp];
                        p++;
                    } else if (posn(gm_numeral_nondigits, (const char) ddata[sp]) != -1) {
                        if (ppos != -1) {
                            break;
                        }
                        punt = ddata[sp];
                        ppos = p;
                    } else if (sp < (length - 1) && (ddata[sp] == 13) && (ddata[sp + 1] == 10)) {
                        /* <end of line> */
                        if (ppos != -1) {
                            break;
                        }
                        punt = ddata[sp];
                        sp++;
                        ppos = p;
                    } else {
                        break;
                    }
                    sp++;
                } while ((p < 3) && (sp < length) && mode[sp] == GM_NUMBER);

                if (ppos != -1) {
                    switch (punt) {
                        case ' ': glyph = 0;
                            break;
                        case '+': glyph = 3;
                            break;
                        case '-': glyph = 6;
                            break;
                        case '.': glyph = 9;
                            break;
                        case ',': glyph = 12;
                            break;
                        case 13: glyph = 15;
                            break;
                    }
                    glyph += ppos;
                    glyph += 1000;

                    if (debug_print) {
                        printf("[%d] ", (int) glyph);
                    }

                    bp = bin_append_posn(glyph, 10, binary, bp);
                }

                glyph = (100 * (numbuf[0] - '0')) + (10 * (numbuf[1] - '0')) + (numbuf[2] - '0');
                if (debug_print) {
                    printf("[%d] ", (int) glyph);
                }

                bp = bin_append_posn(glyph, 10, binary, bp);
                break;

            case GM_BYTE:
                if (last_mode != current_mode) {
                    /* Reserve space for byte block length indicator (9 bits) */
                    byte_count_posn = bp;
                    bp = bin_append_posn(0, 9, binary, bp);
                }
                glyph = ddata[sp];
                if (byte_count == 512 || (glyph > 0xFF && byte_count == 511)) {
                    /* Maximum byte block size is 512 bytes. If longer is needed then start a new block */
                    if (glyph > 0xFF && byte_count == 511) { /* Split double-byte */
                        bp = bin_append_posn(glyph >> 8, 8, binary, bp);
                        glyph &= 0xFF;
                        byte_count++;
                    }
                    gm_add_byte_count(binary, byte_count_posn, byte_count);
                    bp = bin_append_posn(7, 4, binary, bp);
                    byte_count_posn = bp;
                    bp = bin_append_posn(0, 9, binary, bp);
                    byte_count = 0;
                }

                if (debug_print) {
                    printf("[%d] ", (int) glyph);
                }
                bp = bin_append_posn(glyph, glyph > 0xFF ? 16 : 8, binary, bp);
                sp++;
                byte_count++;
                if (glyph > 0xFF) {
                    byte_count++;
                }
                break;

            case GM_MIXED:
                shift = 1;
                if (z_isdigit(ddata[sp])) {
                    shift = 0;
                } else if (z_isupper(ddata[sp])) {
                    shift = 0;
                } else if (z_islower(ddata[sp])) {
                    shift = 0;
                } else if (ddata[sp] == ' ') {
                    shift = 0;
                }

                if (shift == 0) {
                    /* Mixed Mode character */
                    glyph = posn(EUROPIUM, (const char) ddata[sp]);
                    if (debug_print) {
                        printf("[%d] ", (int) glyph);
                    }

                    bp = bin_append_posn(glyph, 6, binary, bp);
                } else {
                    /* Shift Mode character */
                    bp = bin_append_posn(1014, 10, binary, bp); /* shift indicator */
                    bp = gm_add_shift_char(binary, bp, ddata[sp], debug_print);
                }

                sp++;
                break;

            case GM_UPPER:
                shift = 1;
                if (z_isupper(ddata[sp])) {
                    shift = 0;
                } else if (ddata[sp] == ' ') {
                    shift = 0;
                }

                if (shift == 0) {
                    /* Upper Case character */
                    glyph = posn(EUROPIUM_UPR, (const char) ddata[sp]);
                    if (debug_print) {
                        printf("[%d] ", (int) glyph);
                    }

                    bp = bin_append_posn(glyph, 5, binary, bp);
                } else {
                    /* Shift Mode character */
                    bp = bin_append_posn(125, 7, binary, bp); /* shift indicator */
                    bp = gm_add_shift_char(binary, bp, ddata[sp], debug_print);
                }

                sp++;
                break;

            case GM_LOWER:
                shift = 1;
                if (z_islower(ddata[sp])) {
                    shift = 0;
                } else if (ddata[sp] == ' ') {
                    shift = 0;
                }

                if (shift == 0) {
                    /* Lower Case character */
                    glyph = posn(EUROPIUM_LWR, (const char) ddata[sp]);
                    if (debug_print) {
                        printf("[%d] ", (int) glyph);
                    }

                    bp = bin_append_posn(glyph, 5, binary, bp);
                } else {
                    /* Shift Mode character */
                    bp = bin_append_posn(125, 7, binary, bp); /* shift indicator */
                    bp = gm_add_shift_char(binary, bp, ddata[sp], debug_print);
                }

                sp++;
                break;
        }
        if (bp > 9191) {
            return ZINT_ERROR_TOO_LONG;
        }

    } while (sp < length);

    if (current_mode == GM_NUMBER) {
        /* add numeric block padding value */
        switch (p) {
            case 1: binary[number_pad_posn] = '1';
                binary[number_pad_posn + 1] = '0';
                break; /* 2 pad digits */
            case 2: binary[number_pad_posn] = '0';
                binary[number_pad_posn + 1] = '1';
                break; /* 1 pad digit */
            case 3: binary[number_pad_posn] = '0';
                binary[number_pad_posn + 1] = '0';
                break; /* 0 pad digits */
        }
    }

    if (current_mode == GM_BYTE) {
        /* Add byte block length indicator */
        gm_add_byte_count(binary, byte_count_posn, byte_count);
    }

    /* Add "end of data" character */
    switch (current_mode) {
        case GM_CHINESE: bp = bin_append_posn(8160, 13, binary, bp);
            break;
        case GM_NUMBER: bp = bin_append_posn(1018, 10, binary, bp);
            break;
        case GM_LOWER:
        case GM_UPPER: bp = bin_append_posn(27, 5, binary, bp);
            break;
        case GM_MIXED: bp = bin_append_posn(1008, 10, binary, bp);
            break;
        case GM_BYTE: bp = bin_append_posn(0, 4, binary, bp);
            break;
    }

    if (bp > 9191) {
        return ZINT_ERROR_TOO_LONG;
    }

    *p_bp = bp;

    if (debug_print) {
        printf("\nBinary (%d): %.*s\n", bp, bp, binary);
    }

    return 0;
}

static int gm_encode_segs(unsigned int ddata[], const struct zint_seg segs[], const int seg_count, char binary[],
            const int reader, const struct zint_structapp *p_structapp, int *p_bin_len, const int debug_print) {
    int i;
    unsigned int *dd = ddata;
    int bp = 0;
    int p;

    if (reader && (!p_structapp || p_structapp->index == 1)) { /* Appears only in 1st symbol if Structured Append */
        bp = bin_append_posn(10, 4, binary, bp); /* FNC3 - Reader Initialisation */
    }

    if (p_structapp) {
        bp = bin_append_posn(9, 4, binary, bp); /* FNC2 - Structured Append */
        bp = bin_append_posn(to_int((const unsigned char *) p_structapp->id, (int) strlen(p_structapp->id)), 8,
                binary, bp); /* File signature */
        bp = bin_append_posn(p_structapp->count - 1, 4, binary, bp);
        bp = bin_append_posn(p_structapp->index - 1, 4, binary, bp);
    }

    for (i = 0; i < seg_count; i++) {
        int error_number = gm_encode(dd, segs[i].length, binary, segs[i].eci, &bp, debug_print);
        if (error_number != 0) {
            return error_number;
        }
        dd += segs[i].length;
    }

    /* Add padding bits if required */
    p = 7 - (bp % 7);
    if (p % 7) {
        bp = bin_append_posn(0, p, binary, bp);
    }
    /* Note bit-padding can't tip `bp` over max 9191 (1313 * 7) */

    if (debug_print) {
        printf("\nBinary (%d): %.*s\n", bp, bp, binary);
    }

    *p_bin_len = bp;

    return 0;
}

static void gm_add_ecc(const char binary[], const int data_posn, const int layers, const int ecc_level,
            unsigned char word[]) {
    int data_cw, i, j, wp, p;
    int n1, b1, n2, b2, e1, b3, e2;
    int block_size, ecc_size;
    unsigned char data[1320], block[130];
    unsigned char data_block[115], ecc_block[70];
    rs_t rs;

    data_cw = gm_data_codewords[((layers - 1) * 5) + (ecc_level - 1)];

    for (i = 0; i < 1320; i++) {
        data[i] = 0;
    }

    /* Convert from binary stream to 7-bit codewords */
    for (i = 0; i < data_posn; i++) {
        for (p = 0; p < 7; p++) {
            if (binary[i * 7 + p] == '1') {
                data[i] += (0x40 >> p);
            }
        }
    }

    /* Add padding codewords */
    data[data_posn] = 0x00;
    for (i = (data_posn + 1); i < data_cw; i++) {
        if (i & 1) {
            data[i] = 0x7e;
        } else {
            data[i] = 0x00;
        }
    }

    /* Get block sizes */
    n1 = gm_n1[(layers - 1)];
    b1 = gm_b1[(layers - 1)];
    n2 = n1 - 1;
    b2 = gm_b2[(layers - 1)];
    e1 = gm_ebeb[((layers - 1) * 20) + ((ecc_level - 1) * 4)];
    b3 = gm_ebeb[((layers - 1) * 20) + ((ecc_level - 1) * 4) + 1];
    e2 = gm_ebeb[((layers - 1) * 20) + ((ecc_level - 1) * 4) + 2];

    rs_init_gf(&rs, 0x89);

    /* Split the data into blocks */
    wp = 0;
    for (i = 0; i < (b1 + b2); i++) {
        int data_size;
        if (i < b1) {
            block_size = n1;
        } else {
            block_size = n2;
        }
        if (i < b3) {
            ecc_size = e1;
        } else {
            ecc_size = e2;
        }
        data_size = block_size - ecc_size;

        /* printf("block %d/%d: data %d / ecc %d\n", i + 1, (b1 + b2), data_size, ecc_size);*/

        for (j = 0; j < data_size; j++) {
            data_block[j] = data[wp];
            wp++;
        }

        /* Calculate ECC data for this block */
        rs_init_code(&rs, ecc_size, 1);
        rs_encode(&rs, data_size, data_block, ecc_block);

        /* Add error correction data */
        for (j = 0; j < data_size; j++) {
            block[j] = data_block[j];
        }
        for (j = 0; j < ecc_size; j++) {
            block[j + data_size] = ecc_block[j];
        }

        for (j = 0; j < n2; j++) {
            word[((b1 + b2) * j) + i] = block[j];
        }
        if (block_size == n1) {
            word[((b1 + b2) * (n1 - 1)) + i] = block[(n1 - 1)];
        }
    }
}

static void gm_place_macromodule(char grid[], int x, int y, int word1, int word2, int size) {
    int i, j;

    i = (x * 6) + 1;
    j = (y * 6) + 1;

    if (word2 & 0x40) {
        grid[(j * size) + i + 2] = '1';
    }
    if (word2 & 0x20) {
        grid[(j * size) + i + 3] = '1';
    }
    if (word2 & 0x10) {
        grid[((j + 1) * size) + i] = '1';
    }
    if (word2 & 0x08) {
        grid[((j + 1) * size) + i + 1] = '1';
    }
    if (word2 & 0x04) {
        grid[((j + 1) * size) + i + 2] = '1';
    }
    if (word2 & 0x02) {
        grid[((j + 1) * size) + i + 3] = '1';
    }
    if (word2 & 0x01) {
        grid[((j + 2) * size) + i] = '1';
    }
    if (word1 & 0x40) {
        grid[((j + 2) * size) + i + 1] = '1';
    }
    if (word1 & 0x20) {
        grid[((j + 2) * size) + i + 2] = '1';
    }
    if (word1 & 0x10) {
        grid[((j + 2) * size) + i + 3] = '1';
    }
    if (word1 & 0x08) {
        grid[((j + 3) * size) + i] = '1';
    }
    if (word1 & 0x04) {
        grid[((j + 3) * size) + i + 1] = '1';
    }
    if (word1 & 0x02) {
        grid[((j + 3) * size) + i + 2] = '1';
    }
    if (word1 & 0x01) {
        grid[((j + 3) * size) + i + 3] = '1';
    }
}

static void gm_place_data_in_grid(unsigned char word[], char grid[], int modules, int size) {
    int x, y, macromodule, offset;

    offset = 13 - ((modules - 1) / 2);
    for (y = 0; y < modules; y++) {
        for (x = 0; x < modules; x++) {
            macromodule = gm_macro_matrix[((y + offset) * 27) + (x + offset)];
            gm_place_macromodule(grid, x, y, word[macromodule * 2], word[(macromodule * 2) + 1], size);
        }
    }
}

/* Place the layer ID into each macromodule */
static void gm_place_layer_id(char *grid, int size, int layers, int modules, int ecc_level) {
    int i, j, layer, start, stop;
    int *layerid = (int *) z_alloca(sizeof(int) * (layers + 1));
    int *id = (int *) z_alloca(sizeof(int) * (modules * modules));

    /* Calculate Layer IDs */
    for (i = 0; i <= layers; i++) {
        if (ecc_level == 1) {
            layerid[i] = 3 - (i % 4);
        } else {
            layerid[i] = (i + 5 - ecc_level) % 4;
        }
    }

    for (i = 0; i < modules; i++) {
        for (j = 0; j < modules; j++) {
            id[(i * modules) + j] = 0;
        }
    }

    /* Calculate which value goes in each macromodule */
    start = modules / 2;
    stop = modules / 2;
    for (layer = 0; layer <= layers; layer++) {
        for (i = start; i <= stop; i++) {
            id[(start * modules) + i] = layerid[layer];
            id[(i * modules) + start] = layerid[layer];
            id[((modules - start - 1) * modules) + i] = layerid[layer];
            id[(i * modules) + (modules - start - 1)] = layerid[layer];
        }
        start--;
        stop++;
    }

    /* Place the data in the grid */
    for (i = 0; i < modules; i++) {
        for (j = 0; j < modules; j++) {
            if (id[(i * modules) + j] & 0x02) {
                grid[(((i * 6) + 1) * size) + (j * 6) + 1] = '1';
            }
            if (id[(i * modules) + j] & 0x01) {
                grid[(((i * 6) + 1) * size) + (j * 6) + 2] = '1';
            }
        }
    }
}

INTERNAL int gridmatrix(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int warn_number = 0;
    int size, modules, error_number;
    int auto_layers, min_layers, layers, auto_ecc_level, min_ecc_level, ecc_level;
    int x, y, i;
    int full_multibyte;
    char binary[9300];
    int data_cw, input_latch = 0;
    unsigned char word[1460] = {0};
    int data_max, reader = 0;
    const struct zint_structapp *p_structapp = NULL;
    int size_squared;
    int bin_len;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    const int eci_length_segs = get_eci_length_segs(segs, seg_count);
    struct zint_seg *local_segs = (struct zint_seg *) z_alloca(sizeof(struct zint_seg) * seg_count);
    unsigned int *ddata = (unsigned int *) z_alloca(sizeof(unsigned int) * eci_length_segs);
    char *grid;

    segs_cpy(symbol, segs, seg_count, local_segs); /* Shallow copy (needed to set default ECIs & protect lengths) */

    /* If ZINT_FULL_MULTIBYTE set use Hanzi mode in DATA_MODE or for non-GB 2312 in UNICODE_MODE */
    full_multibyte = (symbol->option_3 & 0xFF) == ZINT_FULL_MULTIBYTE;

    if ((symbol->input_mode & 0x07) == DATA_MODE) {
        gb2312_cpy_segs(local_segs, seg_count, ddata, full_multibyte);
    } else {
        unsigned int *dd = ddata;
        for (i = 0; i < seg_count; i++) {
            int done = 0;
            if (local_segs[i].eci != 0 && local_segs[i].eci != 29) { /* Unless default or ECI 29 (GB 2312) */
                /* Try other conversions */
                error_number = gb2312_utf8_to_eci(local_segs[i].eci, local_segs[i].source, &local_segs[i].length,
                                                    dd, full_multibyte);
                if (error_number == 0) {
                    done = 1;
                } else {
                    return errtxtf(error_number, symbol, 535, "Invalid character in input for ECI '%d'",
                                    local_segs[i].eci);
                }
            }
            if (!done) {
                /* Try GB 2312 (EUC-CN) */
                error_number = gb2312_utf8(symbol, local_segs[i].source, &local_segs[i].length, dd);
                if (error_number != 0) {
                    return error_number;
                }
            }
            dd += local_segs[i].length;
        }
    }

    if (symbol->output_options & READER_INIT) reader = 1;

    if (symbol->structapp.count) {
        if (symbol->structapp.count < 2 || symbol->structapp.count > 16) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 536,
                            "Structured Append count '%d' out of range (2 to 16)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 537,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }
        if (symbol->structapp.id[0]) {
            int id, id_len;

            for (id_len = 1; id_len < 4 && symbol->structapp.id[id_len]; id_len++);

            if (id_len > 3) { /* 255 (8 bits) */
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 538,
                                "Structured Append ID length %d too long (3 digit maximum)", id_len);
            }

            id = to_int((const unsigned char *) symbol->structapp.id, id_len);
            if (id == -1) {
                return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 539, "Invalid Structured Append ID (digits only)");
            }
            if (id > 255) {
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 530,
                                "Structured Append ID value '%d' out of range (0 to 255)", id);
            }
        }
        p_structapp = &symbol->structapp;
    }

    if (symbol->eci > 811799) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 533, "ECI code '%d' out of range (0 to 811799)",
                        symbol->eci);
    }

    error_number = gm_encode_segs(ddata, local_segs, seg_count, binary, reader, p_structapp, &bin_len, debug_print);
    if (error_number != 0) {
        return errtxt(error_number, symbol, 531, "Input too long, requires too many codewords (maximum 1313)");
    }

    /* Determine the size of the symbol */
    data_cw = bin_len / 7; /* Binary length always a multiple of 7 */

    auto_layers = 13;
    for (i = 12; i > 0; i--) {
        if (gm_recommend_cw[(i - 1)] >= data_cw) {
            auto_layers = i;
        }
    }
    min_layers = 13;
    for (i = 12; i > 0; i--) {
        if (gm_max_cw[(i - 1)] >= data_cw) {
            min_layers = i;
        }
    }
    layers = auto_layers;

    if ((symbol->option_2 >= 1) && (symbol->option_2 <= 13)) {
        input_latch = 1;
        if (symbol->option_2 >= min_layers) {
            layers = symbol->option_2;
        } else {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 534,
                        "Input too long for Version %1$d, requires %2$d codewords (maximum %3$d)",
                        symbol->option_2, data_cw, gm_max_cw[symbol->option_2 - 1]);
        }
    }

    auto_ecc_level = 3;
    if (layers == 1) {
        auto_ecc_level = 5;
    } else if ((layers == 2) || (layers == 3)) {
        auto_ecc_level = 4;
    }
    ecc_level = auto_ecc_level;

    min_ecc_level = 1;
    if (layers == 1) {
        min_ecc_level = 4;
    } else if (layers == 2) {
        min_ecc_level = 2;
    }

    if ((symbol->option_1 >= 1) && (symbol->option_1 <= 5)) {
        if (symbol->option_1 >= min_ecc_level) {
            ecc_level = symbol->option_1;
        } else {
            ecc_level = min_ecc_level;
        }
    }
    if (data_cw > gm_data_codewords[(5 * (layers - 1)) + (ecc_level - 1)]) {
        /* If layers user-specified (option_2), try reducing ECC level first */
        if (input_latch && ecc_level > min_ecc_level) {
            do {
                ecc_level--;
            } while ((data_cw > gm_data_codewords[(5 * (layers - 1)) + (ecc_level - 1)])
                        && (ecc_level > min_ecc_level));
        }
        while (data_cw > gm_data_codewords[(5 * (layers - 1)) + (ecc_level - 1)] && (layers < 13)) {
            layers++;
        }
        /* ECC min level 1 for layers > 2 */
        while (data_cw > gm_data_codewords[(5 * (layers - 1)) + (ecc_level - 1)] && ecc_level > 1) {
            ecc_level--;
        }
    }

    data_max = 1313;
    switch (ecc_level) {
        case 2: data_max = 1167;
            break;
        case 3: data_max = 1021;
            break;
        case 4: data_max = 875;
            break;
        case 5: data_max = 729;
            break;
    }

    if (data_cw > data_max) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 532,
                        "Input too long for ECC level %1$d, requires %2$d codewords (maximum %3$d)",
                        ecc_level, data_cw, data_max);
    }
    if (debug_print) {
        printf("Layers: %d, ECC level: %d, Data Codewords: %d\n", layers, ecc_level, data_cw);
    }

    gm_add_ecc(binary, data_cw, layers, ecc_level, word);
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) debug_test_codeword_dump(symbol, word, data_cw);
#endif
    size = 6 + (layers * 12);
    modules = 1 + (layers * 2);
    size_squared = size * size;

    grid = (char *) z_alloca(size_squared);
    memset(grid, '0', size_squared);

    gm_place_data_in_grid(word, grid, modules, size);
    gm_place_layer_id(grid, size, layers, modules, ecc_level);

    /* Add macromodule frames */
    for (x = 0; x < modules; x++) {
        int dark = 1 - (x & 1);
        for (y = 0; y < modules; y++) {
            if (dark == 1) {
                for (i = 0; i < 5; i++) {
                    grid[((y * 6) * size) + (x * 6) + i] = '1';
                    grid[(((y * 6) + 5) * size) + (x * 6) + i] = '1';
                    grid[(((y * 6) + i) * size) + (x * 6)] = '1';
                    grid[(((y * 6) + i) * size) + (x * 6) + 5] = '1';
                }
                grid[(((y * 6) + 5) * size) + (x * 6) + 5] = '1';
                dark = 0;
            } else {
                dark = 1;
            }
        }
    }

    /* Copy values to symbol */
    symbol->width = size;
    symbol->rows = size;

    for (x = 0; x < size; x++) {
        for (y = 0; y < size; y++) {
            if (grid[(y * size) + x] == '1') {
                set_module(symbol, y, x);
            }
        }
        symbol->row_height[x] = 1;
    }
    symbol->height = size;

    return warn_number;
}

/* vim: set ts=4 sw=4 et : */
