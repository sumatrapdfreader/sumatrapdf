/*  hanxin.c - Han Xin Code */
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

/* This code attempts to implement Han Xin Code according to ISO/IEC 20830:2021
 * (previously ISO/IEC 20830 (draft 2019-10-10) and AIMD-015:2010 (Rev 0.8)) */

#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "reedsol.h"
#include "hanxin.h"
#include "eci.h"

/* Find which submode to use for a text character */
static int hx_getsubmode(const unsigned int input) {

    if (z_isdigit(input)) {
        return 1;
    }

    if (z_isupper(input)) {
        return 1;
    }

    if (z_islower(input)) {
        return 1;
    }

    return 2;
}

/* Return length of terminator for encoding mode */
static int hx_terminator_length(const char mode) {
    int result = 0;

    switch (mode) {
        case 'n':
            result = 10;
            break;
        case 't':
            result = 6;
            break;
        case '1':
        case '2':
            result = 12;
            break;
        case 'd':
            result = 15;
            break;
    }

    return result;
}

/* Calculate the length of the binary string */
static int hx_calc_binlen(const char mode[], const unsigned int ddata[], const int length, const int eci) {
    int i;
    char lastmode = '\0';
    int est_binlen = 0;
    int submode = 1;
    int numeric_run = 0;

    if (eci != 0) {
        est_binlen += 4;
        if (eci <= 127) {
            est_binlen += 8;
        } else if (eci <= 16383) {
            est_binlen += 16;
        } else {
            est_binlen += 24;
        }
    }

    i = 0;
    do {
        if (mode[i] != lastmode) {
            if (i > 0) {
                est_binlen += hx_terminator_length(lastmode);
            }
            /* GB 4-byte has indicator for each character (and no terminator) so not included here */
            /* Region1/Region2 have special terminator to go directly into each other's mode so not included here */
            if (mode[i] != 'f' || ((mode[i] == '1' && lastmode == '2') || (mode[i] == '2' && lastmode == '1'))) {
                est_binlen += 4;
            }
            if (mode[i] == 'b') { /* Byte mode has byte count (and no terminator) */
                est_binlen += 13;
            }
            lastmode = mode[i];
            submode = 1;
            numeric_run = 0;
        }
        switch (mode[i]) {
            case 'n':
                if (numeric_run % 3 == 0) {
                    est_binlen += 10;
                }
                numeric_run++;
                break;
            case 't':
                if (hx_getsubmode(ddata[i]) != submode) {
                    est_binlen += 6;
                    submode = hx_getsubmode(ddata[i]);
                }
                est_binlen += 6;
                break;
            case 'b':
                est_binlen += ddata[i] > 0xFF ? 16 : 8;
                break;
            case '1':
            case '2':
                est_binlen += 12;
                break;
            case 'd':
                est_binlen += 15;
                break;
            case 'f':
                est_binlen += 25;
                i++;
                break;
        }
        i++;
    } while (i < length);

    est_binlen += hx_terminator_length(lastmode);

    return est_binlen;
}

/* Call `hx_calc_binlen()` for each segment */
static int hx_calc_binlen_segs(const char mode[], const unsigned int ddata[], const struct zint_seg segs[],
            const int seg_count) {
    int i;
    int count = 0;
    const unsigned int *dd = ddata;
    const char *m = mode;

    for (i = 0; i < seg_count; i++) {
        count += hx_calc_binlen(m, dd, segs[i].length, segs[i].eci);
        m += segs[i].length;
        dd += segs[i].length;
    }

    return count;
}

static int hx_isRegion1(const unsigned int glyph) {
    unsigned int byte;

    byte = glyph >> 8;

    if ((byte >= 0xb0) && (byte <= 0xd7)) {
        byte = glyph & 0xff;
        if ((byte >= 0xa1) && (byte <= 0xfe)) {
            return 1;
        }
    } else if ((byte >= 0xa1) && (byte <= 0xa3)) {
        byte = glyph & 0xff;
        if ((byte >= 0xa1) && (byte <= 0xfe)) {
            return 1;
        }
    } else if ((glyph >= 0xa8a1) && (glyph <= 0xa8c0)) {
        return 1;
    }

    return 0;
}

static int hx_isRegion2(const unsigned int glyph) {
    unsigned int byte;

    byte = glyph >> 8;

    if ((byte >= 0xd8) && (byte <= 0xf7)) {
        byte = glyph & 0xff;
        if ((byte >= 0xa1) && (byte <= 0xfe)) {
            return 1;
        }
    }

    return 0;
}

static int hx_isDoubleByte(const unsigned int glyph) {
    unsigned int byte;

    byte = glyph >> 8;

    if ((byte >= 0x81) && (byte <= 0xfe)) {
        byte = glyph & 0xff;
        if ((byte >= 0x40) && (byte <= 0x7e)) {
            return 1;
        }
        if ((byte >= 0x80) && (byte <= 0xfe)) {
            return 1;
        }
    }

    return 0;
}

static int hx_isFourByte(const unsigned int glyph, const unsigned int glyph2) {
    unsigned int byte;

    byte = glyph >> 8;

    if ((byte >= 0x81) && (byte <= 0xfe)) {
        byte = glyph & 0xff;
        if ((byte >= 0x30) && (byte <= 0x39)) {
            byte = glyph2 >> 8;
            if ((byte >= 0x81) && (byte <= 0xfe)) {
                byte = glyph2 & 0xff;
                if ((byte >= 0x30) && (byte <= 0x39)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/* Convert Text 1 sub-mode character to encoding value, as given in table 3 */
static int hx_lookup_text1(const unsigned int input) {

    if (z_isdigit(input)) {
        return input - '0';
    }

    if (z_isupper(input)) {
        return input - 'A' + 10;
    }

    if (z_islower(input)) {
        return input - 'a' + 36;
    }

    return -1;
}

/* Convert Text 2 sub-mode character to encoding value, as given in table 4 */
static int hx_lookup_text2(const unsigned int input) {

    if (input <= 27) {
        return input;
    }

    if ((input >= ' ') && (input <= '/')) {
        return input - ' ' + 28;
    }

    if ((input >= ':') && (input <= '@')) {
        return input - ':' + 44;
    }

    if ((input >= '[') && (input <= 96)) {
        return input - '[' + 51;
    }

    if ((input >= '{') && (input <= 127)) {
        return input - '{' + 57;
    }

    return -1;
}

/* hx_define_mode() stuff */

/* Bits multiplied by this for costs, so as to be whole integer divisible by 2 and 3 */
#define HX_MULT 6

/* Whether in numeric or not. If in numeric, *p_end is set to position after numeric,
 * and *p_cost is set to per-numeric cost */
static int hx_in_numeric(const unsigned int ddata[], const int length, const int in_posn,
            unsigned int *p_end, unsigned int *p_cost) {
    int i, digit_cnt;

    if (in_posn < (int) *p_end) {
        return 1;
    }

    /* Attempt to calculate the average 'cost' of using numeric mode in number of bits (times HX_MULT) */
    for (i = in_posn; i < length && i < in_posn + 3 && z_isdigit(ddata[i]); i++);

    digit_cnt = i - in_posn;

    if (digit_cnt == 0) {
        *p_end = 0;
        return 0;
    }
    *p_end = i;
    *p_cost = digit_cnt == 1
                ? 60 /* 10 * HX_MULT */ : digit_cnt == 2 ? 30 /* (10 / 2) * HX_MULT */ : 20 /* (10 / 3) * HX_MULT */;
    return 1;
}

/* Whether in four-byte or not. If in four-byte, *p_fourbyte is set to position after four-byte,
 * and *p_fourbyte_cost is set to per-position cost */
static int hx_in_fourbyte(const unsigned int ddata[], const int length, const int in_posn,
            unsigned int *p_end, unsigned int *p_cost) {
    if (in_posn < (int) *p_end) {
        return 1;
    }

    if (in_posn == length - 1 || !hx_isFourByte(ddata[in_posn], ddata[in_posn + 1])) {
        *p_end = 0;
        return 0;
    }
    *p_end = in_posn + 2;
    *p_cost = 75; /* ((4 + 21) / 2) * HX_MULT */
    return 1;
}

/* Indexes into mode_types array */
#define HX_N   0 /* Numeric */
#define HX_T   1 /* Text */
#define HX_B   2 /* Binary */
#define HX_1   3 /* Common Chinese Region One */
#define HX_2   4 /* Common Chinese Region Two */
#define HX_D   5 /* GB 18030 2-byte Region */
#define HX_F   6 /* GB 18030 4-byte Region */
/* Note Unicode, GS1 and URI modes not implemented */

#define HX_NUM_MODES 7

/* Calculate optimized encoding modes. Adapted from Project Nayuki */
/* Copyright (c) Project Nayuki. (MIT License) See qr.c for detailed notice */
static void hx_define_mode(char *mode, const unsigned int ddata[], const int length, const int debug_print) {
    /* Must be in same order as HX_N etc */
    static const char mode_types[] = { 'n', 't', 'b', '1', '2', 'd', 'f', '\0' };

    /* Initial mode costs */
    static const unsigned int head_costs[HX_NUM_MODES] = {
    /*  N            T            B                   1            2            D            F */
        4 * HX_MULT, 4 * HX_MULT, (4 + 13) * HX_MULT, 4 * HX_MULT, 4 * HX_MULT, 4 * HX_MULT, 0
    };

    /* Cost of switching modes from k to j */
    static const unsigned char switch_costs[HX_NUM_MODES][HX_NUM_MODES] = {
        /*      N                   T                   B                        1                   2                   D                   F */
        /*N*/ {                  0, (10 + 4) * HX_MULT, (10 + 4 + 13) * HX_MULT, (10 + 4) * HX_MULT, (10 + 4) * HX_MULT, (10 + 4) * HX_MULT, 10 * HX_MULT },
        /*T*/ {  (6 + 4) * HX_MULT,                  0,  (6 + 4 + 13) * HX_MULT,  (6 + 4) * HX_MULT,  (6 + 4) * HX_MULT,  (6 + 4) * HX_MULT,  6 * HX_MULT },
        /*B*/ {        4 * HX_MULT,        4 * HX_MULT,                       0,        4 * HX_MULT,        4 * HX_MULT,        4 * HX_MULT,  0 },
        /*1*/ { (12 + 4) * HX_MULT, (12 + 4) * HX_MULT, (12 + 4 + 13) * HX_MULT,                  0,       12 * HX_MULT, (12 + 4) * HX_MULT, 12 * HX_MULT },
        /*2*/ { (12 + 4) * HX_MULT, (12 + 4) * HX_MULT, (12 + 4 + 13) * HX_MULT,       12 * HX_MULT,                  0, (12 + 4) * HX_MULT, 12 * HX_MULT },
        /*D*/ { (15 + 4) * HX_MULT, (15 + 4) * HX_MULT, (15 + 4 + 13) * HX_MULT, (15 + 4) * HX_MULT, (15 + 4) * HX_MULT,                  0, 15 * HX_MULT },
        /*F*/ {        4 * HX_MULT,        4 * HX_MULT,      (4 + 13) * HX_MULT,        4 * HX_MULT,        4 * HX_MULT,        4 * HX_MULT,  0 },
    };

    /* Final end-of-data costs */
    static const unsigned char eod_costs[HX_NUM_MODES] = {
    /*  N             T            B  1             2             D             F */
        10 * HX_MULT, 6 * HX_MULT, 0, 12 * HX_MULT, 12 * HX_MULT, 15 * HX_MULT, 0
    };

    unsigned int numeric_end = 0, numeric_cost = 0, text_submode = 1, fourbyte_end = 0, fourbyte_cost = 0; /* State */
    int text1, text2;

    int i, j, k;
    unsigned int min_cost;
    char cur_mode;
    unsigned int prev_costs[HX_NUM_MODES];
    unsigned int cur_costs[HX_NUM_MODES];
    char (*char_modes)[HX_NUM_MODES] = (char (*)[HX_NUM_MODES]) z_alloca(HX_NUM_MODES * length);

    /* char_modes[i][j] represents the mode to encode the code point at index i such that the final segment
       ends in mode_types[j] and the total number of bits is minimized over all possible choices */
    memset(char_modes, 0, HX_NUM_MODES * length);

    /* At the beginning of each iteration of the loop below, prev_costs[j] is the minimum number of 1/6 (1/XX_MULT)
     * bits needed to encode the entire string prefix of length i, and end in mode_types[j] */
    memcpy(prev_costs, head_costs, HX_NUM_MODES * sizeof(unsigned int));

    /* Calculate costs using dynamic programming */
    for (i = 0; i < length; i++) {
        memset(cur_costs, 0, HX_NUM_MODES * sizeof(unsigned int));

        if (hx_in_numeric(ddata, length, i, &numeric_end, &numeric_cost)) {
            cur_costs[HX_N] = prev_costs[HX_N] + numeric_cost;
            char_modes[i][HX_N] = 'n';
            text1 = 1;
            text2 = 0;
        } else {
            text1 = hx_lookup_text1(ddata[i]) != -1;
            text2 = hx_lookup_text2(ddata[i]) != -1;
        }

        if (text1 || text2) {
            if ((text_submode == 1 && text2) || (text_submode == 2 && text1)) {
                cur_costs[HX_T] = prev_costs[HX_T] + 72; /* (6 + 6) * HX_MULT */
                text_submode = text2 ? 2 : 1;
            } else {
                cur_costs[HX_T] = prev_costs[HX_T] + 36; /* 6 * HX_MULT */
            }
            char_modes[i][HX_T] = 't';
        } else {
            text_submode = 1;
        }

        /* Binary mode can encode anything */
        cur_costs[HX_B] = prev_costs[HX_B] + (ddata[i] > 0xFF ? 96 : 48); /* (16 : 8) * HX_MULT */
        char_modes[i][HX_B] = 'b';

        if (hx_in_fourbyte(ddata, length, i, &fourbyte_end, &fourbyte_cost)) {
            cur_costs[HX_F] = prev_costs[HX_F] + fourbyte_cost;
            char_modes[i][HX_F] = 'f';
        } else {
            if (hx_isDoubleByte(ddata[i])) {
                cur_costs[HX_D] = prev_costs[HX_D] + 90; /* 15 * HX_MULT */
                char_modes[i][HX_D] = 'd';
                if (hx_isRegion1(ddata[i])) { /* Subset */
                    cur_costs[HX_1] = prev_costs[HX_1] + 72; /* 12 * HX_MULT */
                    char_modes[i][HX_1] = '1';
                } else if (hx_isRegion2(ddata[i])) { /* Subset */
                    cur_costs[HX_2] = prev_costs[HX_2] + 72; /* 12 * HX_MULT */
                    char_modes[i][HX_2] = '2';
                }
            }
        }

        if (i == length - 1) { /* Add end of data costs if last character */
            for (j = 0; j < HX_NUM_MODES; j++) {
                if (char_modes[i][j]) {
                    cur_costs[j] += eod_costs[j];
                }
            }
        }

        /* Start new segment at the end to switch modes */
        for (j = 0; j < HX_NUM_MODES; j++) { /* To mode */
            for (k = 0; k < HX_NUM_MODES; k++) { /* From mode */
                if (j != k && char_modes[i][k]) {
                    const unsigned int new_cost = cur_costs[k] + switch_costs[k][j];
                    if (!char_modes[i][j] || new_cost < cur_costs[j]) {
                        cur_costs[j] = new_cost;
                        char_modes[i][j] = mode_types[k];
                    }
                }
            }
        }

        memcpy(prev_costs, cur_costs, HX_NUM_MODES * sizeof(unsigned int));
    }

    /* Find optimal ending mode */
    min_cost = prev_costs[0];
    cur_mode = mode_types[0];
    for (i = 1; i < HX_NUM_MODES; i++) {
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

/* Call `hx_define_mode()` for each segment */
static void hx_define_mode_segs(char mode[], const unsigned int ddata[], const struct zint_seg segs[],
            const int seg_count, const int debug_print) {
    int i;
    const unsigned int *dd = ddata;
    char *m = mode;

    for (i = 0; i < seg_count; i++) {
        hx_define_mode(m, dd, segs[i].length, debug_print);
        m += segs[i].length;
        dd += segs[i].length;
    }
}

/* Convert input data to binary stream */
static void hx_calculate_binary(char binary[], const char mode[], const unsigned int ddata[], const int length,
            const int eci, int *p_bp, const int debug_print) {
    int position = 0;
    int i, count, encoding_value;
    int first_byte, second_byte;
    int third_byte, fourth_byte;
    int glyph;
    int submode;
    int bp = *p_bp;

    if (eci != 0) {
        /* Encoding ECI assignment number, according to Table 5 */
        bp = bin_append_posn(8, 4, binary, bp); /* ECI */
        if (eci <= 127) {
            bp = bin_append_posn(eci, 8, binary, bp);
        } else if (eci <= 16383) {
            bp = bin_append_posn(2, 2, binary, bp);
            bp = bin_append_posn(eci, 14, binary, bp);
        } else {
            bp = bin_append_posn(6, 3, binary, bp);
            bp = bin_append_posn(eci, 21, binary, bp);
        }
    }

    do {
        int block_length = 0;
        int double_byte = 0;
        do {
            if (mode[position] == 'b' && ddata[position + block_length] > 0xFF) {
                double_byte++;
            }
            block_length++;
        } while (position + block_length < length && mode[position + block_length] == mode[position]);

        switch (mode[position]) {
            case 'n':
                /* Numeric mode */
                /* Mode indicator */
                bp = bin_append_posn(1, 4, binary, bp);

                if (debug_print) {
                    printf("Numeric (N%d): ", block_length);
                }

                count = 0; /* Suppress gcc -Wmaybe-uninitialized */
                i = 0;

                while (i < block_length) {
                    const int first = ctoi((const char) ddata[position + i]);
                    count = 1;
                    encoding_value = first;

                    if (i + 1 < block_length && mode[position + i + 1] == 'n') {
                        const int second = ctoi((const char) ddata[position + i + 1]);
                        count = 2;
                        encoding_value = (encoding_value * 10) + second;

                        if (i + 2 < block_length && mode[position + i + 2] == 'n') {
                            const int third = ctoi((const char) ddata[position + i + 2]);
                            count = 3;
                            encoding_value = (encoding_value * 10) + third;
                        }
                    }

                    bp = bin_append_posn(encoding_value, 10, binary, bp);

                    if (debug_print) {
                        printf(" 0x%3x(%d)", encoding_value, encoding_value);
                    }

                    i += count;
                }

                /* Mode terminator depends on number of characters in last group (Table 2) */
                switch (count) {
                    case 1:
                        bp = bin_append_posn(1021, 10, binary, bp);
                        break;
                    case 2:
                        bp = bin_append_posn(1022, 10, binary, bp);
                        break;
                    case 3:
                        bp = bin_append_posn(1023, 10, binary, bp);
                        break;
                }

                if (debug_print) {
                    printf(" (TERM %d)\n", count);
                }

                break;
            case 't':
                /* Text mode */
                /* Mode indicator */
                bp = bin_append_posn(2, 4, binary, bp);

                if (debug_print) {
                    printf("Text (T%d):", block_length);
                }

                submode = 1;

                i = 0;

                while (i < block_length) {

                    if (hx_getsubmode(ddata[i + position]) != submode) {
                        /* Change submode */
                        bp = bin_append_posn(62, 6, binary, bp);
                        submode = hx_getsubmode(ddata[i + position]);
                        if (debug_print) {
                            fputs(" SWITCH", stdout);
                        }
                    }

                    if (submode == 1) {
                        encoding_value = hx_lookup_text1(ddata[i + position]);
                    } else {
                        encoding_value = hx_lookup_text2(ddata[i + position]);
                    }

                    bp = bin_append_posn(encoding_value, 6, binary, bp);

                    if (debug_print) {
                        printf(" %.2x[ASC %.2x]", encoding_value, ddata[i + position]);
                    }
                    i++;
                }

                /* Terminator */
                bp = bin_append_posn(63, 6, binary, bp);

                if (debug_print) {
                    fputs("\n", stdout);
                }
                break;
            case 'b':
                /* Binary Mode */
                /* Mode indicator */
                bp = bin_append_posn(3, 4, binary, bp);

                /* Count indicator */
                bp = bin_append_posn(block_length + double_byte, 13, binary, bp);

                if (debug_print) {
                    printf("Binary Mode (B%d):", block_length + double_byte);
                }

                i = 0;

                while (i < block_length) {

                    /* 8-bit bytes with no conversion */
                    bp = bin_append_posn(ddata[i + position], ddata[i + position] > 0xFF ? 16 : 8, binary, bp);

                    if (debug_print) {
                        printf(" %02x", (int) ddata[i + position]);
                    }

                    i++;
                }

                if (debug_print) {
                    fputs("\n", stdout);
                }
                break;
            case '1':
                /* Region One encoding */
                /* Mode indicator */
                if (position == 0 || mode[position - 1] != '2') { /* Unless previous mode Region Two */
                    bp = bin_append_posn(4, 4, binary, bp);
                }

                if (debug_print) {
                    printf("Region One%s H(1)%d:",
                        position == 0 || mode[position - 1] != '2' ? "" : " (NO indicator)", block_length);
                }

                i = 0;

                while (i < block_length) {
                    first_byte = (ddata[i + position] & 0xff00) >> 8;
                    second_byte = ddata[i + position] & 0xff;

                    /* Subset 1 */
                    glyph = (0x5e * (first_byte - 0xb0)) + (second_byte - 0xa1);

                    /* Subset 2 */
                    if ((first_byte >= 0xa1) && (first_byte <= 0xa3)) {
                        if ((second_byte >= 0xa1) && (second_byte <= 0xfe)) {
                            glyph = (0x5e * (first_byte - 0xa1)) + (second_byte - 0xa1) + 0xeb0;
                        }
                    }

                    /* Subset 3 */
                    if ((ddata[i + position] >= 0xa8a1) && (ddata[i + position] <= 0xa8c0)) {
                        glyph = (second_byte - 0xa1) + 0xfca;
                    }

                    if (debug_print) {
                        printf(" %.3x[GB %.4x]", glyph, ddata[i + position]);
                    }

                    bp = bin_append_posn(glyph, 12, binary, bp);
                    i++;
                }

                /* Terminator */
                bp = bin_append_posn(position + block_length == length || mode[position + block_length] != '2'
                                    ? 4095 : 4094, 12, binary, bp);

                if (debug_print) {
                    printf(" (TERM %x)\n", position + block_length == length || mode[position + block_length] != '2'
                            ? 4095 : 4094);
                }

                break;
            case '2':
                /* Region Two encoding */
                /* Mode indicator */
                if (position == 0 || mode[position - 1] != '1') { /* Unless previous mode Region One */
                    bp = bin_append_posn(5, 4, binary, bp);
                }

                if (debug_print) {
                    printf("Region Two%s H(2)%d:",
                            position == 0 || mode[position - 1] != '1' ? "" : " (NO indicator)", block_length);
                }

                i = 0;

                while (i < block_length) {
                    first_byte = (ddata[i + position] & 0xff00) >> 8;
                    second_byte = ddata[i + position] & 0xff;

                    glyph = (0x5e * (first_byte - 0xd8)) + (second_byte - 0xa1);

                    if (debug_print) {
                        printf(" %.3x[GB %.4x]", glyph, ddata[i + position]);
                    }

                    bp = bin_append_posn(glyph, 12, binary, bp);
                    i++;
                }

                /* Terminator */
                bp = bin_append_posn(position + block_length == length || mode[position + block_length] != '1'
                                    ? 4095 : 4094, 12, binary, bp);

                if (debug_print) {
                    printf(" (TERM %x)\n", position + block_length == length || mode[position + block_length] != '1'
                            ? 4095 : 4094);
                }

                break;
            case 'd':
                /* Double byte encoding */
                /* Mode indicator */
                bp = bin_append_posn(6, 4, binary, bp);

                if (debug_print) {
                    printf("Double byte (H(d)%d):", block_length);
                }

                i = 0;

                while (i < block_length) {
                    first_byte = (ddata[i + position] & 0xff00) >> 8;
                    second_byte = ddata[i + position] & 0xff;

                    if (second_byte <= 0x7e) {
                        glyph = (0xbe * (first_byte - 0x81)) + (second_byte - 0x40);
                    } else {
                        glyph = (0xbe * (first_byte - 0x81)) + (second_byte - 0x41);
                    }

                    if (debug_print) {
                        printf("%.4x ", glyph);
                    }

                    bp = bin_append_posn(glyph, 15, binary, bp);
                    i++;
                }

                /* Terminator */
                bp = bin_append_posn(32767, 15, binary, bp);
                /* Terminator sequence of length 12 is a mistake
                   - confirmed by Wang Yi */

                if (debug_print) {
                    fputc('\n', stdout);
                }
                break;
            case 'f':
                /* Four-byte encoding */
                if (debug_print) {
                    printf("Four byte (H(f)%d):", block_length);
                }

                i = 0;

                while (i < block_length) {

                    /* Mode indicator */
                    bp = bin_append_posn(7, 4, binary, bp);

                    first_byte = (ddata[i + position] & 0xff00) >> 8;
                    second_byte = ddata[i + position] & 0xff;
                    third_byte = (ddata[i + position + 1] & 0xff00) >> 8;
                    fourth_byte = ddata[i + position + 1] & 0xff;

                    glyph = (0x3138 * (first_byte - 0x81)) + (0x04ec * (second_byte - 0x30)) +
                            (0x0a * (third_byte - 0x81)) + (fourth_byte - 0x30);

                    if (debug_print) {
                        printf(" %d", glyph);
                    }

                    bp = bin_append_posn(glyph, 21, binary, bp);
                    i += 2;
                }

                /* No terminator */

                if (debug_print) {
                    fputc('\n', stdout);
                }
                break;
        }

        position += block_length;

    } while (position < length);

    if (debug_print) printf("Binary (%d): %.*s\n", bp, bp, binary);

    *p_bp = bp;
}

/* Call `hx_calculate_binary()` for each segment */
static void hx_calculate_binary_segs(char binary[], const char mode[], const unsigned int ddata[],
            const struct zint_seg segs[], const int seg_count, int *p_bin_len, const int debug_print) {
    int i;
    const unsigned int *dd = ddata;
    const char *m = mode;
    int bp = 0;

    for (i = 0; i < seg_count; i++) {
        hx_calculate_binary(binary, m, dd, segs[i].length, segs[i].eci, &bp, debug_print);
        m += segs[i].length;
        dd += segs[i].length;
    }

    *p_bin_len = bp;
}

/* Finder pattern for top left of symbol */
static void hx_place_finder_top_left(unsigned char *grid, const int size) {
    int xp, yp;
    int x = 0, y = 0;
    char finder[] = {0x7F, 0x40, 0x5F, 0x50, 0x57, 0x57, 0x57};

    for (xp = 0; xp < 7; xp++) {
        for (yp = 0; yp < 7; yp++) {
            if (finder[yp] & 0x40 >> xp) {
                grid[((yp + y) * size) + (xp + x)] = 0x11;
            } else {
                grid[((yp + y) * size) + (xp + x)] = 0x10;
            }
        }
    }
}

/* Finder pattern for top right and bottom left of symbol */
static void hx_place_finder(unsigned char *grid, const int size, const int x, const int y) {
    int xp, yp;
    static const char finder[] = {0x7F, 0x01, 0x7D, 0x05, 0x75, 0x75, 0x75};

    for (xp = 0; xp < 7; xp++) {
        for (yp = 0; yp < 7; yp++) {
            if (finder[yp] & 0x40 >> xp) {
                grid[((yp + y) * size) + (xp + x)] = 0x11;
            } else {
                grid[((yp + y) * size) + (xp + x)] = 0x10;
            }
        }
    }
}

/* Finder pattern for bottom right of symbol */
static void hx_place_finder_bottom_right(unsigned char *grid, const int size) {
    int xp, yp;
    const int x = size - 7;
    const int y = x;
    static const char finder[] = {0x75, 0x75, 0x75, 0x05, 0x7D, 0x01, 0x7F};

    for (xp = 0; xp < 7; xp++) {
        for (yp = 0; yp < 7; yp++) {
            if (finder[yp] & 0x40 >> xp) {
                grid[((yp + y) * size) + (xp + x)] = 0x11;
            } else {
                grid[((yp + y) * size) + (xp + x)] = 0x10;
            }
        }
    }
}

/* Avoid plotting outside symbol or over finder patterns */
static void hx_safe_plot(unsigned char *grid, const int size, const int x, const int y, const int value) {
    if ((x >= 0) && (x < size)) {
        if ((y >= 0) && (y < size)) {
            if (grid[(y * size) + x] == 0) {
                grid[(y * size) + x] = value;
            }
        }
    }
}

/* Plot an alignment pattern around top and right of a module */
static void hx_plot_alignment(unsigned char *grid, const int size, const int x, const int y, const int w,
            const int h) {
    int i;
    hx_safe_plot(grid, size, x, y, 0x11);
    hx_safe_plot(grid, size, x - 1, y + 1, 0x10);

    for (i = 1; i <= w; i++) {
        /* Top */
        hx_safe_plot(grid, size, x - i, y, 0x11);
        hx_safe_plot(grid, size, x - i - 1, y + 1, 0x10);
    }

    for (i = 1; i < h; i++) {
        /* Right */
        hx_safe_plot(grid, size, x, y + i, 0x11);
        hx_safe_plot(grid, size, x - 1, y + i + 1, 0x10);
    }
}

/* Plot assistant alignment patterns */
static void hx_plot_assistant(unsigned char *grid, const int size, const int x, const int y) {
    hx_safe_plot(grid, size, x - 1, y - 1, 0x10);
    hx_safe_plot(grid, size, x, y - 1, 0x10);
    hx_safe_plot(grid, size, x + 1, y - 1, 0x10);
    hx_safe_plot(grid, size, x - 1, y, 0x10);
    hx_safe_plot(grid, size, x, y, 0x11);
    hx_safe_plot(grid, size, x + 1, y, 0x10);
    hx_safe_plot(grid, size, x - 1, y + 1, 0x10);
    hx_safe_plot(grid, size, x, y + 1, 0x10);
    hx_safe_plot(grid, size, x + 1, y + 1, 0x10);
}

/* Put static elements in the grid */
static void hx_setup_grid(unsigned char *grid, const int size, const int version) {
    int i;

    memset(grid, 0, (size_t) size * size);

    /* Add finder patterns */
    hx_place_finder_top_left(grid, size);
    hx_place_finder(grid, size, 0, size - 7);
    hx_place_finder(grid, size, size - 7, 0);
    hx_place_finder_bottom_right(grid, size);

    /* Add finder pattern separator region */
    for (i = 0; i < 8; i++) {
        /* Top left */
        grid[(7 * size) + i] = 0x10;
        grid[(i * size) + 7] = 0x10;

        /* Top right */
        grid[(7 * size) + (size - i - 1)] = 0x10;
        grid[((size - i - 1) * size) + 7] = 0x10;

        /* Bottom left */
        grid[(i * size) + (size - 8)] = 0x10;
        grid[((size - 8) * size) + i] = 0x10;

        /* Bottom right */
        grid[((size - 8) * size) + (size - i - 1)] = 0x10;
        grid[((size - i - 1) * size) + (size - 8)] = 0x10;
    }

    /* Reserve function information region */
    for (i = 0; i < 9; i++) {
        /* Top left */
        grid[(8 * size) + i] = 0x10;
        grid[(i * size) + 8] = 0x10;

        /* Top right */
        grid[(8 * size) + (size - i - 1)] = 0x10;
        grid[((size - i - 1) * size) + 8] = 0x10;

        /* Bottom left */
        grid[(i * size) + (size - 9)] = 0x10;
        grid[((size - 9) * size) + i] = 0x10;

        /* Bottom right */
        grid[((size - 9) * size) + (size - i - 1)] = 0x10;
        grid[((size - i - 1) * size) + (size - 9)] = 0x10;
    }

    if (version > 3) {
        const int k = hx_module_k[version - 1];
        const int r = hx_module_r[version - 1];
        const int m = hx_module_m[version - 1];
        int x, y, row_switch, column_switch;
        int module_height, module_width;
        int mod_x, mod_y;

        /* Add assistant alignment patterns to left and right */
        y = 0;
        mod_y = 0;
        do {
            if (mod_y < m) {
                module_height = k;
            } else {
                module_height = r - 1;
            }

            if ((mod_y & 1) == 0) {
                if ((m & 1) == 1) {
                    hx_plot_assistant(grid, size, 0, y);
                }
            } else {
                if ((m & 1) == 0) {
                    hx_plot_assistant(grid, size, 0, y);
                }
                hx_plot_assistant(grid, size, size - 1, y);
            }

            mod_y++;
            y += module_height;
        } while (y < size);

        /* Add assistant alignment patterns to top and bottom */
        x = (size - 1);
        mod_x = 0;
        do {
            if (mod_x < m) {
                module_width = k;
            } else {
                module_width = r - 1;
            }

            if ((mod_x & 1) == 0) {
                if ((m & 1) == 1) {
                    hx_plot_assistant(grid, size, x, (size - 1));
                }
            } else {
                if ((m & 1) == 0) {
                    hx_plot_assistant(grid, size, x, (size - 1));
                }
                hx_plot_assistant(grid, size, x, 0);
            }

            mod_x++;
            x -= module_width;
        } while (x >= 0);

        /* Add alignment pattern */
        column_switch = 1;
        y = 0;
        mod_y = 0;
        do {
            if (mod_y < m) {
                module_height = k;
            } else {
                module_height = r - 1;
            }

            if (column_switch == 1) {
                row_switch = 1;
                column_switch = 0;
            } else {
                row_switch = 0;
                column_switch = 1;
            }

            x = (size - 1);
            mod_x = 0;
            do {
                if (mod_x < m) {
                    module_width = k;
                } else {
                    module_width = r - 1;
                }

                if (row_switch == 1) {
                    if (!(y == 0 && x == (size - 1))) {
                        hx_plot_alignment(grid, size, x, y, module_width, module_height);
                    }
                    row_switch = 0;
                } else {
                    row_switch = 1;
                }
                mod_x++;
                x -= module_width;
            } while (x >= 0);

            mod_y++;
            y += module_height;
        } while (y < size);
    }
}

/* Calculate error correction codes */
static void hx_add_ecc(unsigned char fullstream[], const unsigned char datastream[], const int data_codewords,
            const int version, const int ecc_level) {
    unsigned char data_block[180];
    unsigned char ecc_block[36];
    int i, j, block;
    int input_position = 0;
    int output_position = 0;
    const int table_d1_pos = ((version - 1) * 36) + ((ecc_level - 1) * 9);
    rs_t rs;

    rs_init_gf(&rs, 0x163); /* x^8 + x^6 + x^5 + x + 1 = 0 */

    for (i = 0; i < 3; i++) {
        const int batch_size = hx_table_d1[table_d1_pos + (3 * i)];
        const int data_length = hx_table_d1[table_d1_pos + (3 * i) + 1];
        const int ecc_length = hx_table_d1[table_d1_pos + (3 * i) + 2];

        rs_init_code(&rs, ecc_length, 1);

        for (block = 0; block < batch_size; block++) {
            for (j = 0; j < data_length; j++) {
                data_block[j] = input_position < data_codewords ? datastream[input_position] : 0;
                fullstream[output_position++] = data_block[j];
                input_position++;
            }

            rs_encode(&rs, data_length, data_block, ecc_block);

            for (j = 0; j < ecc_length; j++) {
                fullstream[output_position++] = ecc_block[j];
            }
        }
    }
}

static void hx_set_function_info(unsigned char *grid, const int size, const int version, const int ecc_level,
            const int bitmask, const int debug_print) {
    int i, j;
    char function_information[34];
    unsigned char fi_cw[3] = {0};
    unsigned char fi_ecc[4];
    int bp = 0;
    rs_t rs;

    /* Form function information string */

    bp = bin_append_posn(version + 20, 8, function_information, bp);
    bp = bin_append_posn(ecc_level - 1, 2, function_information, bp);
    bp = bin_append_posn(bitmask, 2, function_information, bp);

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 4; j++) {
            if (function_information[(i * 4) + j] == '1') {
                fi_cw[i] += (0x08 >> j);
            }
        }
    }

    rs_init_gf(&rs, 0x13);
    rs_init_code(&rs, 4, 1);
    rs_encode(&rs, 3, fi_cw, fi_ecc);

    for (i = 0; i < 4; i++) {
        bp = bin_append_posn(fi_ecc[i], 4, function_information, bp);
    }

    /* Previously added alternating filler pattern here (as does BWIPP) but not mentioned in ISO/IEC 20830:2021 and
       does not appear in Figure 1 nor in the figures in Annex K (although does appear in Figure 2 and Figures 4-9)
       nor in the AIM ITS/04-023:2022 examples: so just clear */
    for (i = 28; i < 34; i++) {
        function_information[i] = '0';
    }

    if (debug_print) {
        printf("Version: %d, ECC: %d, Mask: %d, Structural Info: %.34s\n", version, ecc_level, bitmask,
                function_information);
    }

    /* Add function information to symbol */
    for (i = 0; i < 9; i++) {
        if (function_information[i] == '1') {
            grid[(8 * size) + i] = 0x01;
            grid[((size - 8 - 1) * size) + (size - i - 1)] = 0x01;
        }
        if (function_information[i + 8] == '1') {
            grid[((8 - i) * size) + 8] = 0x01;
            grid[((size - 8 - 1 + i) * size) + (size - 8 - 1)] = 0x01;
        }
        if (function_information[i + 17] == '1') {
            grid[(i * size) + (size - 1 - 8)] = 0x01;
            grid[((size - 1 - i) * size) + 8] = 0x01;
        }
        if (function_information[i + 25] == '1') {
            grid[(8 * size) + (size - 1 - 8 + i)] = 0x01;
            grid[((size - 1 - 8) * size) + (8 - i)] = 0x01;
        }
    }
}

/* Rearrange data in batches of 13 codewords (section 5.8.2) */
static void hx_make_picket_fence(const unsigned char fullstream[], unsigned char picket_fence[],
            const int streamsize) {
    int i, start;
    int output_position = 0;

    for (start = 0; start < 13; start++) {
        for (i = start; i < streamsize; i += 13) {
            picket_fence[output_position] = fullstream[i];
            output_position++;
        }
    }
}

/* Evaluate a bitmask according to table 9 */
static int hx_evaluate(const unsigned char *local, const int size) {
    static const unsigned char h1010111[7] = { 1, 0, 1, 0, 1, 1, 1 };
    static const unsigned char h1110101[7] = { 1, 1, 1, 0, 1, 0, 1 };

    int x, y, r, block;
    int result = 0;
    int state;
    int a, b, afterCount, beforeCount;

    /* Test 1: 1:1:1:1:3 or 3:1:1:1:1 ratio pattern in row/column */
    /* Vertical */
    for (x = 0; x < size; x++) {
        for (y = 0; y <= (size - 7); y++) {
            if (local[y * size + x] && local[(y + 1) * size + x] != local[(y + 5) * size + x] &&
                    local[(y + 2) * size + x] && !local[(y + 3) * size + x] &&
                    local[(y + 4) * size + x] && local[(y + 6) * size + x]) {
                /* Pattern found, check before and after */
                beforeCount = 0;
                for (b = (y - 1); b >= (y - 3); b--) {
                    if (b < 0) { /* Count < edge as whitespace */
                        beforeCount = 3;
                        break;
                    }
                    if (local[(b * size) + x]) {
                        break;
                    }
                    beforeCount++;
                }
                if (beforeCount == 3) {
                    /* Pattern is preceded by light area 3 modules wide */
                    result += 50;
                } else {
                    afterCount = 0;
                    for (a = (y + 7); a <= (y + 9); a++) {
                        if (a >= size) { /* Count > edge as whitespace */
                            afterCount = 3;
                            break;
                        }
                        if (local[(a * size) + x]) {
                            break;
                        }
                        afterCount++;
                    }
                    if (afterCount == 3) {
                        /* Pattern is followed by light area 3 modules wide */
                        result += 50;
                    }
                }
                y++; /* Skip to next possible match */
            }
        }
    }

    /* Horizontal */
    for (y = 0; y < size; y++) {
        r = y * size;
        for (x = 0; x <= (size - 7); x++) {
            if (memcmp(local + r + x, h1010111, 7) == 0 || memcmp(local + r + x, h1110101, 7) == 0) {
                /* Pattern found, check before and after */
                beforeCount = 0;
                for (b = (x - 1); b >= (x - 3); b--) {
                    if (b < 0) { /* Count < edge as whitespace */
                        beforeCount = 3;
                        break;
                    }
                    if (local[r + b]) {
                        break;
                    }
                    beforeCount++;
                }

                if (beforeCount == 3) {
                    /* Pattern is preceded by light area 3 modules wide */
                    result += 50;
                } else {
                    afterCount = 0;
                    for (a = (x + 7); a <= (x + 9); a++) {
                        if (a >= size) { /* Count > edge as whitespace */
                            afterCount = 3;
                            break;
                        }
                        if (local[r + a]) {
                            break;
                        }
                        afterCount++;
                    }
                    if (afterCount == 3) {
                        /* Pattern is followed by light area 3 modules wide */
                        result += 50;
                    }
                }
                x++; /* Skip to next possible match */
            }
        }
    }

    /* Test 2: Adjacent modules in row/column in same colour */
    /* In AIMD-15 section 5.8.3.2 it is stated... “In Table 9 below, i refers to the row
     * position of the module.” - however i being the length of the run of the
     * same colour (i.e. "block" below) in the same fashion as ISO/IEC 18004
     * makes more sense. -- Confirmed by Wang Yi */
    /* Fixed in ISO/IEC 20830 section 5.8.4.3 "In Table, i refers to the modules with
       same color." */

    /* Vertical */
    for (x = 0; x < size; x++) {
        block = 0;
        state = 0;
        for (y = 0; y < size; y++) {
            if (local[(y * size) + x] == state) {
                block++;
            } else {
                if (block >= 3) {
                    result += block * 4;
                }
                block = 1;
                state = local[(y * size) + x];
            }
        }
        if (block >= 3) {
            result += block * 4;
        }
    }

    /* Horizontal */
    for (y = 0; y < size; y++) {
        r = y * size;
        block = 0;
        state = 0;
        for (x = 0; x < size; x++) {
            if (local[r + x] == state) {
                block++;
            } else {
                if (block >= 3) {
                    result += block * 4;
                }
                block = 1;
                state = local[r + x];
            }
        }
        if (block >= 3) {
            result += block * 4;
        }
    }

    return result;
}

/* Apply the four possible bitmasks for evaluation */
/* TODO: Haven't been able to replicate (or even get close to) the penalty scores in ISO/IEC 20830:2021
 * Annex K examples */
static void hx_apply_bitmask(unsigned char *grid, const int size, const int version, const int ecc_level,
            const int user_mask, const int debug_print) {
    int x, y;
    int i, j, r, k;
    int pattern, penalty[4] = {0};
    int best_pattern;
    int bit;
    const int size_squared = size * size;
    unsigned char *mask = (unsigned char *) z_alloca(size_squared);
    unsigned char *local = (unsigned char *) z_alloca(size_squared);

    /* Perform data masking */
    memset(mask, 0, size_squared);
    for (y = 0; y < size; y++) {
        r = y * size;
        for (x = 0; x < size; x++) {
            k = r + x;

            if (!(grid[k] & 0xf0)) {
                j = x + 1;
                i = y + 1;
                if (((i + j) & 1) == 0) {
                    mask[k] |= 0x02;
                }
                if (((((i + j) % 3) + (j % 3)) & 1) == 0) {
                    mask[k] |= 0x04;
                }
                if ((((i % j) + (j % i) + (i % 3) + (j % 3)) & 1) == 0) {
                    mask[k] |= 0x08;
                }
            }
        }
    }

    if (user_mask) {
        best_pattern = user_mask - 1;
    } else {
        /* apply data masks to grid, result in local */

        /* Do null pattern 00 separately first */
        pattern = 0;
        for (k = 0; k < size_squared; k++) {
            local[k] = grid[k] & 0x0f;
        }
        /* Set the Structural Info */
        hx_set_function_info(local, size, version, ecc_level, pattern, 0 /*debug_print*/);

        /* Evaluate result */
        penalty[pattern] = hx_evaluate(local, size);

        best_pattern = 0;
        for (pattern = 1; pattern < 4; pattern++) {
            bit = 1 << pattern;
            for (k = 0; k < size_squared; k++) {
                if (mask[k] & bit) {
                    local[k] = grid[k] ^ 0x01;
                } else {
                    local[k] = grid[k] & 0x0f;
                }
            }
            /* Set the Structural Info */
            hx_set_function_info(local, size, version, ecc_level, pattern, 0 /*debug_print*/);

            /* Evaluate result */
            penalty[pattern] = hx_evaluate(local, size);
            if (penalty[pattern] < penalty[best_pattern]) {
                best_pattern = pattern;
            }
        }
    }

    if (debug_print) {
        printf("Mask: %d (%s)", best_pattern, user_mask ? "specified" : "automatic");
        if (!user_mask) {
            for (pattern = 0; pattern < 4; pattern++) printf(" %d:%d", pattern, penalty[pattern]);
        }
        fputc('\n', stdout);
    }

    /* Apply mask */
    if (best_pattern) { /* If not null mask */
        if (!user_mask && best_pattern == 3) { /* Reuse last */
            memcpy(grid, local, size_squared);
        } else {
            bit = 1 << best_pattern;
            for (k = 0; k < size_squared; k++) {
                if (mask[k] & bit) {
                    grid[k] ^= 0x01;
                }
            }
        }
    }
    /* Set the Structural Info */
    hx_set_function_info(grid, size, version, ecc_level, best_pattern, debug_print);
}

/* Han Xin Code - main */
INTERNAL int hanxin(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int warn_number = 0;
    int est_binlen;
    int ecc_level = symbol->option_1;
    int i, j, j_max, version;
    int full_multibyte;
    int user_mask;
    int data_codewords = 0, size;
    int size_squared;
    int codewords;
    int bin_len;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    const int eci_length_segs = get_eci_length_segs(segs, seg_count);
    struct zint_seg *local_segs = (struct zint_seg *) z_alloca(sizeof(struct zint_seg) * seg_count);
    unsigned int *ddata = (unsigned int *) z_alloca(sizeof(unsigned int) * eci_length_segs);
    char *mode = (char *) z_alloca(eci_length_segs);
    char *binary;
    unsigned char *datastream;
    unsigned char *fullstream;
    unsigned char *picket_fence;
    unsigned char *grid;

    segs_cpy(symbol, segs, seg_count, local_segs); /* Shallow copy (needed to set default ECI & protect lengths) */

    /* If ZINT_FULL_MULTIBYTE set use Hanzi mode in DATA_MODE or for non-GB 18030 in UNICODE_MODE */
    full_multibyte = (symbol->option_3 & 0xFF) == ZINT_FULL_MULTIBYTE;
    user_mask = (symbol->option_3 >> 8) & 0x0F; /* User mask is pattern + 1, so >= 1 and <= 4 */
    if (user_mask > 4) {
        user_mask = 0; /* Ignore */
    }

    if ((symbol->input_mode & 0x07) == DATA_MODE) {
        gb18030_cpy_segs(local_segs, seg_count, ddata, full_multibyte);
    } else {
        unsigned int *dd = ddata;
        for (i = 0; i < seg_count; i++) {
            int done = 0;
            if (local_segs[i].eci != 32 || seg_count > 1) { /* Unless ECI 32 (GB 18030) or have multiple segments */
                /* Try other conversions (ECI 0 defaults to ISO/IEC 8859-1) */
                int error_number = gb18030_utf8_to_eci(local_segs[i].eci, local_segs[i].source, &local_segs[i].length,
                                                        dd, full_multibyte);
                if (error_number == 0) {
                    done = 1;
                } else if (local_segs[i].eci || seg_count > 1) {
                    return errtxtf(error_number, symbol, 545, "Invalid character in input for ECI '%d'",
                                    local_segs[i].eci);
                }
            }
            if (!done) {
                /* Try GB 18030 */
                int error_number = gb18030_utf8(symbol, local_segs[i].source, &local_segs[i].length, dd);
                if (error_number != 0) {
                    return error_number;
                }
                if (local_segs[i].eci != 32) {
                    warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 543,
                                            "Converted to GB 18030 but no ECI specified");
                }
            }
            dd += local_segs[i].length;
        }
    }

    hx_define_mode_segs(mode, ddata, local_segs, seg_count, debug_print);

    est_binlen = hx_calc_binlen_segs(mode, ddata, local_segs, seg_count);
    if (debug_print) {
        printf("Estimated binary length: %d\n", est_binlen);
    }

    binary = (char *) malloc(est_binlen + 1);

    if ((ecc_level <= 0) || (ecc_level >= 5)) {
        ecc_level = 1;
    }

    hx_calculate_binary_segs(binary, mode, ddata, local_segs, seg_count, &bin_len, debug_print);
    codewords = bin_len >> 3;
    if (bin_len & 0x07) {
        codewords++;
    }
    if (debug_print) {
        printf("Num. of codewords: %d (%d padbits)\n", codewords, bin_len & 0x07);
    }

    version = 85;
    for (i = 84; i > 0; i--) {
        if (hx_data_codewords[ecc_level - 1][i - 1] >= codewords) {
            version = i;
            data_codewords = hx_data_codewords[ecc_level - 1][i - 1];
        }
    }

    if (version == 85) {
        free(binary);
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 541, "Input too long, requires %d codewords (maximum 3264)",
                        codewords);
    }

    if ((symbol->option_2 < 0) || (symbol->option_2 > 84)) {
        symbol->option_2 = 0;
    }

    if (symbol->option_2 > version) {
        version = symbol->option_2;
    }

    if ((symbol->option_2 != 0) && (symbol->option_2 < version)) {
        free(binary);
        if (ecc_level == 1) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 542,
                            "Input too long for Version %1$d, requires %2$d codewords (maximum %3$d)",
                            symbol->option_2, codewords, hx_data_codewords[ecc_level - 1][symbol->option_2 - 1]);
        }
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 542,
                        "Input too long for Version %1$d, ECC %2$d, requires %3$d codewords (maximum %4$d)",
                        symbol->option_2, ecc_level, codewords,
                        hx_data_codewords[ecc_level - 1][symbol->option_2 - 1]);
    }

    /* If there is spare capacity, increase the level of ECC */

    /* Unless explicitly specified (within min/max bounds) by user */
    if (symbol->option_1 == -1 || symbol->option_1 != ecc_level) {
        if (ecc_level == 1 && codewords <= hx_data_codewords[1][version - 1]) {
            ecc_level = 2;
            data_codewords = hx_data_codewords[1][version - 1];
        }

        if (ecc_level == 2 && codewords <= hx_data_codewords[2][version - 1]) {
            ecc_level = 3;
            data_codewords = hx_data_codewords[2][version - 1];
        }

        if (ecc_level == 3 && codewords <= hx_data_codewords[3][version - 1]) {
            ecc_level = 4;
            data_codewords = hx_data_codewords[3][version - 1];
        }
    }

    size = (version * 2) + 21;
    size_squared = size * size;

    datastream = (unsigned char *) z_alloca(data_codewords);
    fullstream = (unsigned char *) z_alloca(hx_total_codewords[version - 1]);
    picket_fence = (unsigned char *) z_alloca(hx_total_codewords[version - 1]);
    grid = (unsigned char *) z_alloca(size_squared);

    memset(datastream, 0, data_codewords);

    for (i = 0; i < bin_len; i++) {
        if (binary[i] == '1') {
            datastream[i >> 3] |= 0x80 >> (i & 0x07);
        }
    }
    free(binary);

    if (debug_print) {
        printf("Datastream (%d):", data_codewords);
        for (i = 0; i < data_codewords; i++) {
            printf(" %.2x", datastream[i]);
        }
        fputc('\n', stdout);
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) debug_test_codeword_dump(symbol, datastream, data_codewords);
#endif

    hx_setup_grid(grid, size, version);

    hx_add_ecc(fullstream, datastream, data_codewords, version, ecc_level);

    if (debug_print) {
        printf("Fullstream (%d):", hx_total_codewords[version - 1]);
        for (i = 0; i < hx_total_codewords[version - 1]; i++) {
            printf(" %.2x", fullstream[i]);
        }
        fputc('\n', stdout);
    }

    hx_make_picket_fence(fullstream, picket_fence, hx_total_codewords[version - 1]);

    /* Populate grid */
    j = 0;
    j_max = hx_total_codewords[version - 1] * 8;
    for (i = 0; i < size_squared; i++) {
        if (grid[i] == 0x00) {
            if (j < j_max) {
                if (picket_fence[(j >> 3)] & (0x80 >> (j & 0x07))) {
                    grid[i] = 0x01;
                }
                j++;
            } else {
                break;
            }
        }
    }

    hx_apply_bitmask(grid, size, version, ecc_level, user_mask, debug_print);

    symbol->width = size;
    symbol->rows = size;

    for (i = 0; i < size; i++) {
        const int r = i * size;
        for (j = 0; j < size; j++) {
            if (grid[r + j] & 0x01) {
                set_module(symbol, i, j);
            }
        }
        symbol->row_height[i] = 1;
    }
    symbol->height = size;

    return warn_number;
}

/* vim: set ts=4 sw=4 et : */
