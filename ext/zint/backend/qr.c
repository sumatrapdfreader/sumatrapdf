/* qr.c Handles QR Code, Micro QR Code, UPNQR and rMQR */
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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "eci.h"
#include "qr.h"
#include "reedsol.h"

#define QR_ALPHA    (IS_NUM_F | IS_UPR_F | IS_SPC_F | IS_AST_F | IS_PLS_F | IS_MNS_F | IS_SIL_F | IS_CLI_F)

#define QR_LEVEL_L  0
#define QR_LEVEL_M  1
#define QR_LEVEL_Q  2
#define QR_LEVEL_H  3

static const char qr_ecc_level_names[] = { 'L', 'M', 'Q', 'H' };

#define QR_PERCENT  38 /* Alphanumeric mode % */

#define RMQR_VERSION    41
#define MICROQR_VERSION 73

/* Returns true if input glyph is in the Alphanumeric set or is GS1 FNC1 */
static int qr_is_alpha(const unsigned int glyph, const int gs1) {
    if (is_chr(QR_ALPHA, glyph)) {
        return 1;
    }
    if (gs1 && glyph == '\x1D') {
        return 1;
    }
    return 0;
}

/* Bits multiplied by this for costs, so as to be whole integer divisible by 2 and 3 */
#define QR_MULT 6

/* Whether in numeric or not. If in numeric, *p_end is set to position after numeric, and *p_cost is set to
 * per-numeric cost */
static int qr_in_numeric(const unsigned int ddata[], const int length, const int in_posn,
            unsigned int *p_end, unsigned int *p_cost) {
    int i, digit_cnt;

    if (in_posn < (int) *p_end) {
        return 1;
    }

    /* Attempt to calculate the average 'cost' of using numeric mode in number of bits (times QR_MULT) */
    for (i = in_posn; i < length && i < in_posn + 3 && z_isdigit(ddata[i]); i++);

    digit_cnt = i - in_posn;

    if (digit_cnt == 0) {
        *p_end = 0;
        return 0;
    }
    *p_end = i;
    *p_cost = digit_cnt == 1
                ? 24 /* 4 * QR_MULT */ : digit_cnt == 2 ? 21 /* (7 / 2) * QR_MULT */ : 20 /* (10 / 3) * QR_MULT) */;
    return 1;
}

/* Whether in alpha or not. If in alpha, *p_end is set to position after alpha, and *p_cost is set to per-alpha cost.
 * For GS1, *p_pcent set if 2nd char percent */
static int qr_in_alpha(const unsigned int ddata[], const int length, const int in_posn,
            unsigned int *p_end, unsigned int *p_cost, unsigned int *p_pcent, unsigned int *p_pccnt,
            unsigned int gs1) {
    const int last = in_posn + 1 == length;
    int two_alphas;

    /* Attempt to calculate the average 'cost' of using alphanumeric mode in number of bits (times QR_MULT) */
    if (in_posn < (int) *p_end) {
        if (gs1) {
            if (*p_pcent) {
                /* Previous 2nd char was a percent, so allow for second half of doubled-up percent here */
                two_alphas = !last && qr_is_alpha(ddata[in_posn + 1], gs1);
                /* Uneven percents means this will fit evenly in alpha pair */
                *p_cost = two_alphas || !(*p_pccnt & 1) ? 66 /* 11 * QR_MULT */ : 69 /* (11 / 2 + 6) * QR_MULT */;
                *p_pcent = 0;
            } else {
                /* As above, uneven percents means will fit in alpha pair */
                *p_cost = !last || !(*p_pccnt & 1) ? 33 /* (11 / 2) * QR_MULT */ : 36 /* 6 * QR_MULT */;
            }
        } else {
            *p_cost = !last ? 33 /* (11 / 2) * QR_MULT */ : 36 /* 6 * QR_MULT */;
        }
        return 1;
    }

    if (!qr_is_alpha(ddata[in_posn], gs1)) {
        *p_end = 0;
        *p_pcent = 0;
        *p_pccnt = 0;
        return 0;
    }

    two_alphas = !last && qr_is_alpha(ddata[in_posn + 1], gs1);

    if (gs1 && ddata[in_posn] == '%') { /* Must double-up so counts as 2 chars */
        *p_end = in_posn + 1;
        *p_pcent = 0;
        (*p_pccnt)++;
        /* Uneven percents means will fit in alpha pair */
        *p_cost = two_alphas || !(*p_pccnt & 1) ? 66 /* 11 * QR_MULT */ : 69 /* (11 / 2 + 6) * QR_MULT */;
        return 1;
    }

    *p_end = two_alphas ? in_posn + 2 : in_posn + 1;
    if (gs1) {
        *p_pcent = two_alphas && ddata[in_posn + 1] == '%'; /* 2nd char is percent */
        *p_pccnt += *p_pcent;
        /* Uneven percents means will fit in alpha pair */
        *p_cost = two_alphas || !(*p_pccnt & 1) ? 33 /* (11 / 2) * QR_MULT */ : 36 /* 6 * QR_MULT */;
    } else {
        *p_cost = two_alphas ? 33 /* (11 / 2) * QR_MULT */ : 36 /* 6 * QR_MULT */;
    }
    return 1;
}

#if 0
#define QR_DEBUG_DEFINE_MODE /* For debugging costings */
#endif

/* Indexes into qr_mode_types array (and state array) */
#define QR_N   0 /* Numeric */
#define QR_A   1 /* Alphanumeric */
#define QR_B   2 /* Byte */
#define QR_K   3 /* Kanji */

static const char qr_mode_types[] = { 'N', 'A', 'B', 'K', '\0' }; /* Must be in same order as QR_N etc */

#define QR_NUM_MODES 4

/* Indexes into state array (0..3 head costs) */
#define QR_VER      4   /* Version */
#define QR_N_END    5   /* Numeric end index */
#define QR_N_COST   6   /* Numeric cost */
#define QR_A_END    7   /* Alpha end index */
#define QR_A_COST   8   /* Alpha cost */
#define QR_A_PCENT  9   /* Alpha 2nd char percent (GS1-specific) */
#define QR_A_PCCNT  10  /* Alpha total percent count (GS1-specific) */

/* Costs set to this for invalid MICROQR modes for versions M1 and M2.
 * 128 is the max number of data bits for M4-L (ISO/IEC 18004:2015 Table 7) */
#define QR_MICROQR_MAX 774 /* (128 + 1) * QR_MULT */

/* Initial mode costs */
static unsigned int *qr_head_costs(unsigned int state[11]) {
    static const unsigned int head_costs[7][QR_NUM_MODES] = {
        /* N                    A                   B                   K */
        { (10 + 4) * QR_MULT,  (9 + 4) * QR_MULT,  (8 + 4) * QR_MULT,  (8 + 4) * QR_MULT, }, /* QR */
        { (12 + 4) * QR_MULT, (11 + 4) * QR_MULT, (16 + 4) * QR_MULT, (10 + 4) * QR_MULT, },
        { (14 + 4) * QR_MULT, (13 + 4) * QR_MULT, (16 + 4) * QR_MULT, (12 + 4) * QR_MULT, },
        {        3 * QR_MULT,     QR_MICROQR_MAX,     QR_MICROQR_MAX,     QR_MICROQR_MAX, }, /* MICROQR */
        {  (4 + 1) * QR_MULT,  (3 + 1) * QR_MULT,     QR_MICROQR_MAX,     QR_MICROQR_MAX, },
        {  (5 + 2) * QR_MULT,  (4 + 2) * QR_MULT,  (4 + 2) * QR_MULT,  (3 + 2) * QR_MULT, },
        {  (6 + 3) * QR_MULT,  (5 + 3) * QR_MULT,  (5 + 3) * QR_MULT,  (4 + 3) * QR_MULT, }
    };
    int version;

    /* Head costs kept in states 0..3 */

    version = state[QR_VER];

    if (version < RMQR_VERSION) { /* QRCODE */
        if (version < 10) {
            memcpy(state, head_costs[0], QR_NUM_MODES * sizeof(unsigned int));
        } else if (version < 27) {
            memcpy(state, head_costs[1], QR_NUM_MODES * sizeof(unsigned int));
        } else {
            memcpy(state, head_costs[2], QR_NUM_MODES * sizeof(unsigned int));
        }
    } else if (version < MICROQR_VERSION) { /* RMQR */
        version -= RMQR_VERSION;
        state[QR_N] = (rmqr_numeric_cci[version] + 3) * QR_MULT;
        state[QR_A] = (rmqr_alphanum_cci[version] + 3) * QR_MULT;
        state[QR_B] = (rmqr_byte_cci[version] + 3) * QR_MULT;
        state[QR_K] = (rmqr_kanji_cci[version] + 3) * QR_MULT;
    } else { /* MICROQR */
        memcpy(state, head_costs[3 + (version - MICROQR_VERSION)], QR_NUM_MODES * sizeof(unsigned int));
    }

    return state;
}

/* Calculate optimized encoding modes. Adapted from Project Nayuki */
static void qr_define_mode(char mode[], const unsigned int ddata[], const int length, const int gs1,
            const int version, const int debug_print) {
    /*
     * Copyright (c) Project Nayuki. (MIT License)
     * https://www.nayuki.io/page/qr-code-generator-library
     *
     * Permission is hereby granted, free of charge, to any person obtaining a copy of
     * this software and associated documentation files (the "Software"), to deal in
     * the Software without restriction, including without limitation the rights to
     * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
     * the Software, and to permit persons to whom the Software is furnished to do so,
     * subject to the following conditions:
     * - The above copyright notice and this permission notice shall be included in
     *   all copies or substantial portions of the Software.
     */
    unsigned int state[11] = {
        0 /*N*/, 0 /*A*/, 0 /*B*/, 0 /*K*/, /* Head/switch costs */
        0 /*version*/,
        0 /*numeric_end*/, 0 /*numeric_cost*/, 0 /*alpha_end*/, 0 /*alpha_cost*/, 0 /*alpha_pcent*/, 0 /*alpha_pccnt*/
    };
    int m1, m2;

    int i, j, k;
    unsigned int min_cost;
    char cur_mode;
    unsigned int prev_costs[QR_NUM_MODES];
    unsigned int cur_costs[QR_NUM_MODES];
    char (*char_modes)[QR_NUM_MODES] = (char (*)[QR_NUM_MODES]) z_alloca(QR_NUM_MODES * length);

    state[QR_VER] = (unsigned int) version;

    /* char_modes[i][j] represents the mode to encode the code point at index i such that the final segment
       ends in qr_mode_types[j] and the total number of bits is minimized over all possible choices */
    memset(char_modes, 0, QR_NUM_MODES * length);

    /* At the beginning of each iteration of the loop below, prev_costs[j] is the minimum number of 1/6 (1/QR_MULT)
     * bits needed to encode the entire string prefix of length i, and end in qr_mode_types[j] */
    memcpy(prev_costs, qr_head_costs(state), QR_NUM_MODES * sizeof(unsigned int));

    #ifdef QR_DEBUG_DEFINE_MODE
    printf(" head");
    for (j = 0; j < QR_NUM_MODES; j++) {
        printf(" %c(%c)=%d", qr_mode_types[j], char_modes[0][j], prev_costs[j]);
    }
    printf("\n");
    #endif

    /* Calculate costs using dynamic programming */
    for (i = 0; i < length; i++) {
        memset(cur_costs, 0, QR_NUM_MODES * sizeof(unsigned int));

        m1 = version == MICROQR_VERSION;
        m2 = version == MICROQR_VERSION + 1;

        if (ddata[i] > 0xFF) {
            cur_costs[QR_B] = prev_costs[QR_B] + ((m1 || m2) ? QR_MICROQR_MAX : 96); /* 16 * QR_MULT */
            char_modes[i][QR_B] = 'B';
            cur_costs[QR_K] = prev_costs[QR_K] + ((m1 || m2) ? QR_MICROQR_MAX : 78); /* 13 * QR_MULT */
            char_modes[i][QR_K] = 'K';
        } else {
            if (qr_in_numeric(ddata, length, i, &state[QR_N_END], &state[QR_N_COST])) {
                cur_costs[QR_N] = prev_costs[QR_N] + state[QR_N_COST];
                char_modes[i][QR_N] = 'N';
            }
            if (qr_in_alpha(ddata, length, i, &state[QR_A_END], &state[QR_A_COST], &state[QR_A_PCENT],
                    &state[QR_A_PCCNT], gs1)) {
                cur_costs[QR_A] = prev_costs[QR_A] + (m1 ? QR_MICROQR_MAX : state[QR_A_COST]);
                char_modes[i][QR_A] = 'A';
            }
            cur_costs[QR_B] = prev_costs[QR_B] + ((m1 || m2) ? QR_MICROQR_MAX : 48); /* 8 * QR_MULT */
            char_modes[i][QR_B] = 'B';
        }

        /* Start new segment at the end to switch modes */
        for (j = 0; j < QR_NUM_MODES; j++) { /* To mode */
            for (k = 0; k < QR_NUM_MODES; k++) { /* From mode */
                if (j != k && char_modes[i][k]) {
                    const unsigned int new_cost = cur_costs[k] + state[j]; /* Switch costs same as head costs */
                    if (!char_modes[i][j] || new_cost < cur_costs[j]) {
                        cur_costs[j] = new_cost;
                        char_modes[i][j] = qr_mode_types[k];
                    }
                }
            }
        }

        #ifdef QR_DEBUG_DEFINE_MODE
        {
            int min_j = 0;
            printf(" % 4d: curr", i);
            for (j = 0; j < QR_NUM_MODES; j++) {
                printf(" %c(%c)=%d", qr_mode_types[j], char_modes[i][j], cur_costs[j]);
                if (cur_costs[j] < cur_costs[min_j]) min_j = j;
            }
            printf(" min %c(%c)=%d\n", qr_mode_types[min_j], char_modes[i][min_j], cur_costs[min_j]);
        }
        #endif
        memcpy(prev_costs, cur_costs, QR_NUM_MODES * sizeof(unsigned int));
    }

    /* Find optimal ending mode */
    min_cost = prev_costs[0];
    cur_mode = qr_mode_types[0];
    for (i = 1; i < QR_NUM_MODES; i++) {
        if (prev_costs[i] < min_cost) {
            min_cost = prev_costs[i];
            cur_mode = qr_mode_types[i];
        }
    }

    /* Get optimal mode for each code point by tracing backwards */
    for (i = length - 1; i >= 0; i--) {
        j = posn(qr_mode_types, cur_mode);
        cur_mode = char_modes[i][j];
        mode[i] = cur_mode;
    }

    if (debug_print) {
        printf("  Mode: %.*s\n", length, mode);
    }
}

/* Returns mode indicator based on version and mode */
static int qr_mode_indicator(const int version, const int mode) {
    static const char mode_indicators[6][QR_NUM_MODES] = {
        /*N  A  B  K */
        { 1, 2, 4, 8, }, /* QRCODE */
        { 1, 2, 3, 4, }, /* RMQR */
        { 0, 0, 0, 0, }, /* MICROQR */
        { 0, 1, 0, 0, },
        { 0, 1, 2, 3, },
        { 0, 1, 2, 3, },
    };

    int mode_index = posn(qr_mode_types, (const char) mode);

    if (version < RMQR_VERSION) {
        return mode_indicators[0][mode_index]; /* QRCODE */
    }
    if (version < MICROQR_VERSION) {
        return mode_indicators[1][mode_index] /* RMQR */;
    }
    return mode_indicators[2 + version - MICROQR_VERSION][mode_index]; /* MICROQR */
}

/* Return mode indicator bits based on version */
static int qr_mode_bits(const int version) {
    if (version < RMQR_VERSION) {
        return 4; /* QRCODE */
    }
    if (version < MICROQR_VERSION) {
        return 3; /* RMQR */
    }
    return version - MICROQR_VERSION; /* MICROQR */
}

/* Return character count indicator bits based on version and mode */
static int qr_cci_bits(const int version, const int mode) {
    static const char cci_bits[7][QR_NUM_MODES] = {
        /* N   A   B   K */
        { 10,  9,  8,  8, }, /* QRCODE */
        { 12, 11, 16, 10, },
        { 14, 13, 16, 12, },
        {  3,  0,  0,  0, }, /* MICROQR */
        {  4,  3,  0,  0, },
        {  5,  4,  4,  3, },
        {  6,  5,  5,  4, }
    };
    static const unsigned char *const rmqr_ccis[QR_NUM_MODES] = {
        rmqr_numeric_cci, rmqr_alphanum_cci, rmqr_byte_cci, rmqr_kanji_cci,
    };
    int mode_index = posn(qr_mode_types, (const char) mode);

    if (version < RMQR_VERSION) { /* QRCODE */
        if (version < 10) {
            return cci_bits[0][mode_index];
        }
        if (version < 27) {
            return cci_bits[1][mode_index];
        }
        return cci_bits[2][mode_index];
    }
    if (version < MICROQR_VERSION) { /* RMQR */
        return rmqr_ccis[mode_index][version - RMQR_VERSION];
    }
    return cci_bits[3 + (version - MICROQR_VERSION)][mode_index]; /* MICROQR */
}

/* Returns terminator bits based on version */
static int qr_terminator_bits(const int version) {
    if (version < RMQR_VERSION) {
        return 4; /* QRCODE */
    }
    if (version < MICROQR_VERSION) {
        return 3; /* RMQR */
    }
    return 3 + (version - MICROQR_VERSION) * 2; /* MICROQR */
}

/* Convert input data to a binary stream and add padding */
static int qr_binary(char binary[], int bp, const int version, const char mode[],
            const unsigned int ddata[], const int length, const int gs1,
            const int eci, const int debug_print) {
    int position = 0;
    int i;
    int modebits;
    int percent = 0;
    int percent_count;

    if (eci != 0) { /* Not applicable to MICROQR */
        bp = bin_append_posn(7, version < RMQR_VERSION ? 4 : 3, binary, bp); /* ECI (Table 4) */
        if (eci <= 127) {
            bp = bin_append_posn(eci, 8, binary, bp); /* 000000 to 000127 */
        } else if (eci <= 16383) {
            bp = bin_append_posn(0x8000 + eci, 16, binary, bp); /* 000128 to 016383 */
        } else {
            bp = bin_append_posn(0xC00000 + eci, 24, binary, bp); /* 016384 to 999999 */
        }
    }

    modebits = qr_mode_bits(version);

    do {
        char data_block = mode[position];
        int short_data_block_length = 0;
        int double_byte = 0;
        do {
            if (data_block == 'B' && ddata[position + short_data_block_length] > 0xFF) {
                double_byte++;
            }
            short_data_block_length++;
        } while (((short_data_block_length + position) < length)
                && (mode[position + short_data_block_length] == data_block));

        /* Mode indicator */
        if (modebits) {
            bp = bin_append_posn(qr_mode_indicator(version, data_block), modebits, binary, bp);
        }

        switch (data_block) {
            case 'K':
                /* Kanji mode */

                /* Character count indicator */
                bp = bin_append_posn(short_data_block_length, qr_cci_bits(version, data_block), binary, bp);

                if (debug_print) {
                    printf("Kanji block (length %d)\n\t", short_data_block_length);
                }

                /* Character representation */
                for (i = 0; i < short_data_block_length; i++) {
                    unsigned int jis = ddata[position + i];
                    int prod;

                    if (jis >= 0x8140 && jis <= 0x9ffc)
                        jis -= 0x8140;

                    else if (jis >= 0xe040 && jis <= 0xebbf)
                        jis -= 0xc140;

                    prod = ((jis >> 8) * 0xc0) + (jis & 0xff);

                    bp = bin_append_posn(prod, 13, binary, bp);

                    if (debug_print) {
                        printf("0x%04X ", prod);
                    }
                }

                if (debug_print) {
                    fputc('\n', stdout);
                }

                break;
            case 'B':
                /* Byte mode */

                /* Character count indicator */
                bp = bin_append_posn(short_data_block_length + double_byte, qr_cci_bits(version, data_block), binary,
                                    bp);

                if (debug_print) {
                    printf("Byte block (length %d)\n\t", short_data_block_length + double_byte);
                }

                /* Character representation */
                for (i = 0; i < short_data_block_length; i++) {
                    unsigned int byte = ddata[position + i];

                    bp = bin_append_posn(byte, byte > 0xFF ? 16 : 8, binary, bp);

                    if (debug_print) {
                        printf("0x%02X(%d) ", byte, (int) byte);
                    }
                }

                if (debug_print) {
                    fputc('\n', stdout);
                }

                break;
            case 'A':
                /* Alphanumeric mode */

                percent_count = 0;
                if (gs1) {
                    for (i = 0; i < short_data_block_length; i++) {
                        if (ddata[position + i] == '%') {
                            percent_count++;
                        }
                    }
                }

                /* Character count indicator */
                bp = bin_append_posn(short_data_block_length + percent_count, qr_cci_bits(version, data_block),
                                    binary, bp);

                if (debug_print) {
                    printf("Alpha block (length %d)\n\t", short_data_block_length + percent_count);
                }

                /* Character representation */
                i = 0;
                while (i < short_data_block_length) {
                    int count;
                    int first = 0, second = 0, prod;

                    if (percent == 0) {
                        if (gs1 && (ddata[position + i] == '%')) {
                            first = QR_PERCENT;
                            second = QR_PERCENT;
                            count = 2;
                            prod = (first * 45) + second;
                            i++;
                        } else {
                            if (gs1 && ddata[position + i] == '\x1D') {
                                first = QR_PERCENT; /* FNC1 */
                            } else {
                                first = qr_alphanumeric[ddata[position + i] - 32];
                            }
                            count = 1;
                            i++;
                            prod = first;

                            if (i < short_data_block_length && mode[position + i] == 'A') {
                                if (gs1 && (ddata[position + i] == '%')) {
                                    second = QR_PERCENT;
                                    count = 2;
                                    prod = (first * 45) + second;
                                    percent = 1;
                                } else {
                                    if (gs1 && ddata[position + i] == '\x1D') {
                                        second = QR_PERCENT; /* FNC1 */
                                    } else {
                                        second = qr_alphanumeric[ddata[position + i] - 32];
                                    }
                                    count = 2;
                                    i++;
                                    prod = (first * 45) + second;
                                }
                            }
                        }
                    } else {
                        first = QR_PERCENT;
                        count = 1;
                        i++;
                        prod = first;
                        percent = 0;

                        if (i < short_data_block_length && mode[position + i] == 'A') {
                            if (gs1 && (ddata[position + i] == '%')) {
                                second = QR_PERCENT;
                                count = 2;
                                prod = (first * 45) + second;
                                percent = 1;
                            } else {
                                if (gs1 && ddata[position + i] == '\x1D') {
                                    second = QR_PERCENT; /* FNC1 */
                                } else {
                                    second = qr_alphanumeric[ddata[position + i] - 32];
                                }
                                count = 2;
                                i++;
                                prod = (first * 45) + second;
                            }
                        }
                    }

                    bp = bin_append_posn(prod, 1 + (5 * count), binary, bp);

                    if (debug_print) {
                        printf("0x%X ", prod);
                    }
                }

                if (debug_print) {
                    fputc('\n', stdout);
                }

                break;
            case 'N':
                /* Numeric mode */

                /* Character count indicator */
                bp = bin_append_posn(short_data_block_length, qr_cci_bits(version, data_block), binary, bp);

                if (debug_print) {
                    printf("Number block (length %d)\n\t", short_data_block_length);
                }

                /* Character representation */
                i = 0;
                while (i < short_data_block_length) {
                    int count;
                    int first = 0, prod;

                    first = ctoi((const char) ddata[position + i]);
                    count = 1;
                    prod = first;

                    if (i + 1 < short_data_block_length && mode[position + i + 1] == 'N') {
                        int second = ctoi((const char) ddata[position + i + 1]);
                        count = 2;
                        prod = (prod * 10) + second;

                        if (i + 2 < short_data_block_length && mode[position + i + 2] == 'N') {
                            int third = ctoi((const char) ddata[position + i + 2]);
                            count = 3;
                            prod = (prod * 10) + third;
                        }
                    }

                    bp = bin_append_posn(prod, 1 + (3 * count), binary, bp);

                    if (debug_print) {
                        printf("0x%X(%d) ", prod, prod);
                    }

                    i += count;
                };

                if (debug_print) {
                    fputc('\n', stdout);
                }

                break;
        }

        position += short_data_block_length;
    } while (position < length);

    return bp;
}

/* Call `qr_binary()` for each segment, dealing with Structured Append and GS1 beforehand and padding afterwards */
static int qr_binary_segs(unsigned char datastream[], const int version, const int target_codewords,
            const char mode[], const unsigned int ddata[], const struct zint_seg segs[], const int seg_count,
            const struct zint_structapp *p_structapp, const int gs1, const int est_binlen, const int debug_print) {
    int i, j;
    const unsigned int *dd = ddata;
    const char *m = mode;
    int bp = 0;
    int termbits, padbits;
    int current_bytes;
    int toggle;
    char *binary = (char *) z_alloca(est_binlen + 12);

    *binary = '\0';

    if (p_structapp) {
        bp = bin_append_posn(3, 4, binary, bp); /* Structured Append indicator */
        bp = bin_append_posn(p_structapp->index - 1, 4, binary, bp);
        bp = bin_append_posn(p_structapp->count - 1, 4, binary, bp);
        bp = bin_append_posn(to_int((const unsigned char *) p_structapp->id, (int) strlen(p_structapp->id)), 8,
                binary, bp); /* Parity */
    }

    if (gs1) { /* Not applicable to MICROQR */
        if (version < RMQR_VERSION) {
            bp = bin_append_posn(5, 4, binary, bp); /* FNC1 */
        } else {
            bp = bin_append_posn(5, 3, binary, bp);
        }
    }

    for (i = 0; i < seg_count; i++) {
        bp = qr_binary(binary, bp, version, m, dd, segs[i].length, gs1, segs[i].eci, debug_print);
        m += segs[i].length;
        dd += segs[i].length;
    }

    if (version >= MICROQR_VERSION && version < MICROQR_VERSION + 4) {
        /* MICROQR does its own terminating/padding */
        memcpy(datastream, binary, bp);
        return bp;
    }

    /* Terminator */
    termbits = 8 - bp % 8;
    if (termbits == 8) {
        termbits = 0;
    }
    current_bytes = (bp + termbits) / 8;
    if (termbits || current_bytes < target_codewords) {
        int max_termbits = qr_terminator_bits(version);
        termbits = termbits < max_termbits && current_bytes == target_codewords ? termbits : max_termbits;
        bp = bin_append_posn(0, termbits, binary, bp);
    }

    /* Padding bits */
    padbits = 8 - bp % 8;
    if (padbits == 8) {
        padbits = 0;
    }
    if (padbits) {
        current_bytes = (bp + padbits) / 8;
        (void) bin_append_posn(0, padbits, binary, bp); /* Last use so not setting bp */
    }

    if (debug_print) printf("Terminated binary (%d): %.*s (padbits %d)\n", bp, bp, binary, padbits);

    /* Put data into 8-bit codewords */
    for (i = 0; i < current_bytes; i++) {
        int p;
        j = i * 8;
        datastream[i] = 0x00;
        for (p = 0; p < 8; p++) {
            if (binary[j + p] == '1') {
                datastream[i] |= (0x80 >> p);
            }
        }
    }

    /* Add pad codewords */
    toggle = 0;
    for (i = current_bytes; i < target_codewords; i++) {
        if (toggle == 0) {
            datastream[i] = 0xec;
            toggle = 1;
        } else {
            datastream[i] = 0x11;
            toggle = 0;
        }
    }

    if (debug_print) {
        printf("Resulting codewords (%d):\n\t", target_codewords);
        for (i = 0; i < target_codewords; i++) {
            printf("0x%02X ", datastream[i]);
        }
        fputc('\n', stdout);
    }

    return 0; /* Not used */
}

/* Split data into blocks, add error correction and then interleave the blocks and error correction data */
static void qr_add_ecc(unsigned char fullstream[], const unsigned char datastream[], const int version,
            const int data_cw, const int blocks, const int debug_print) {
    int ecc_cw;
    int short_data_block_length;
    int qty_long_blocks;
    int qty_short_blocks;
    int ecc_block_length;
    int i, j, length_this_block, in_posn;
    rs_t rs;
    unsigned char *data_block;
    unsigned char *ecc_block;
    unsigned char *interleaved_data;
    unsigned char *interleaved_ecc;

    if (version < RMQR_VERSION) {
        ecc_cw = qr_total_codewords[version - 1] - data_cw;
    } else {
        ecc_cw = rmqr_total_codewords[version - RMQR_VERSION] - data_cw;
    }

    /* Suppress some clang-tidy clang-analyzer-core.UndefinedBinaryOperatorResult/uninitialized.Assign warnings */
    assert(blocks > 0);

    short_data_block_length = data_cw / blocks;
    qty_long_blocks = data_cw % blocks;
    qty_short_blocks = blocks - qty_long_blocks;
    ecc_block_length = ecc_cw / blocks;

    /* Suppress some clang-tidy clang-analyzer-core.UndefinedBinaryOperatorResult/uninitialized.Assign warnings */
    assert(short_data_block_length > 0);
    assert(qty_long_blocks || qty_short_blocks);

    data_block = (unsigned char *) z_alloca(short_data_block_length + 1);
    ecc_block = (unsigned char *) z_alloca(ecc_block_length);
    interleaved_data = (unsigned char *) z_alloca(data_cw);
    interleaved_ecc = (unsigned char *) z_alloca(ecc_cw);

    rs_init_gf(&rs, 0x11d);
    rs_init_code(&rs, ecc_block_length, 0);

    in_posn = 0;

    for (i = 0; i < blocks; i++) {
        if (i < qty_short_blocks) {
            length_this_block = short_data_block_length;
        } else {
            length_this_block = short_data_block_length + 1;
        }

        for (j = 0; j < length_this_block; j++) {
            /* This false-positive popped up with clang-tidy 14.0.1 */
            data_block[j] = datastream[in_posn + j]; /* NOLINT(clang-analyzer-core.uninitialized.Assign) */
        }

        rs_encode(&rs, length_this_block, data_block, ecc_block);

        if (debug_print) {
            printf("Block %d: ", i + 1);
            for (j = 0; j < length_this_block; j++) {
                printf("%2X ", data_block[j]);
            }
            if (i < qty_short_blocks) {
                fputs("   ", stdout);
            }
            fputs(" // ", stdout);
            for (j = 0; j < ecc_block_length; j++) {
                printf("%2X ", ecc_block[j]);
            }
            fputc('\n', stdout);
        }

        for (j = 0; j < short_data_block_length; j++) {
            /* And another with clang-tidy 14.0.6 */
            interleaved_data[(j * blocks) + i] = data_block[j]; /* NOLINT(clang-analyzer-core.uninitialized.Assign) */
        }

        if (i >= qty_short_blocks) {
            interleaved_data[(short_data_block_length * blocks) + (i - qty_short_blocks)]
                            = data_block[short_data_block_length];
        }

        for (j = 0; j < ecc_block_length; j++) {
            interleaved_ecc[(j * blocks) + i] = ecc_block[j];
        }

        in_posn += length_this_block;
    }

    for (j = 0; j < data_cw; j++) {
        fullstream[j] = interleaved_data[j];
    }
    for (j = 0; j < ecc_cw; j++) {
        fullstream[j + data_cw] = interleaved_ecc[j];
    }

    if (debug_print) {
        printf("\nData Stream (%d): \n", data_cw + ecc_cw);
        for (j = 0; j < (data_cw + ecc_cw); j++) {
            printf("%2X ", fullstream[j]);
        }
        fputc('\n', stdout);
    }
}

static void qr_place_finder(unsigned char grid[], const int size, const int x, const int y) {
    int xp, yp;
    char finder[] = {0x7F, 0x41, 0x5D, 0x5D, 0x5D, 0x41, 0x7F};

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

static void qr_place_align(unsigned char grid[], const int size, int x, int y) {
    int xp, yp;
    char alignment[] = {0x1F, 0x11, 0x15, 0x11, 0x1F};

    x -= 2;
    y -= 2; /* Input values represent centre of pattern */

    for (xp = 0; xp < 5; xp++) {
        for (yp = 0; yp < 5; yp++) {
            if (alignment[yp] & 0x10 >> xp) {
                grid[((yp + y) * size) + (xp + x)] = 0x11;
            } else {
                grid[((yp + y) * size) + (xp + x)] = 0x10;
            }
        }
    }
}

static void qr_setup_grid(unsigned char *grid, const int size, const int version) {
    int i, toggle = 1;

/* Suppress false positive gcc >= 13 warning (when optimizing only) "writing 1 byte into a region of size 0" */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

    /* Add timing patterns */
    for (i = 0; i < size; i++) {
        if (toggle == 1) {
            grid[(6 * size) + i] = 0x21;
            grid[(i * size) + 6] = 0x21;
            toggle = 0;
        } else {
            grid[(6 * size) + i] = 0x20;
            grid[(i * size) + 6] = 0x20;
            toggle = 1;
        }
    }

    /* Add finder patterns */
    qr_place_finder(grid, size, 0, 0);
    qr_place_finder(grid, size, 0, size - 7);
    qr_place_finder(grid, size, size - 7, 0);

    /* Add separators */
    for (i = 0; i < 7; i++) {
        grid[(7 * size) + i] = 0x10;
        grid[(i * size) + 7] = 0x10;
        grid[(7 * size) + (size - 1 - i)] = 0x10;
        grid[(i * size) + (size - 8)] = 0x10;
        grid[((size - 8) * size) + i] = 0x10;
        grid[((size - 1 - i) * size) + 7] = 0x10;
    }
    grid[(7 * size) + 7] = 0x10;
    grid[(7 * size) + (size - 8)] = 0x10;
    grid[((size - 8) * size) + 7] = 0x10;

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif

    /* Add alignment patterns */
    if (version != 1) {
        /* Version 1 does not have alignment patterns */

        int loopsize = qr_align_loopsize[version - 1];
        int x, y;
        for (x = 0; x < loopsize; x++) {
            for (y = 0; y < loopsize; y++) {
                int xcoord = qr_table_e1[((version - 2) * 7) + x];
                int ycoord = qr_table_e1[((version - 2) * 7) + y];

                if (!(grid[(ycoord * size) + xcoord] & 0x10)) {
                    qr_place_align(grid, size, xcoord, ycoord);
                }
            }
        }
    }

    /* Reserve space for format information */
    for (i = 0; i < 8; i++) {
        grid[(8 * size) + i] |= 0x20;
        grid[(i * size) + 8] |= 0x20;
        grid[(8 * size) + (size - 1 - i)] = 0x20;
        grid[((size - 1 - i) * size) + 8] = 0x20;
    }
    grid[(8 * size) + 8] |= 0x20;
    grid[((size - 1 - 7) * size) + 8] = 0x21; /* Dark Module from Figure 25 */

    /* Reserve space for version information */
    if (version >= 7) {
        for (i = 0; i < 6; i++) {
            grid[((size - 9) * size) + i] = 0x20;
            grid[((size - 10) * size) + i] = 0x20;
            grid[((size - 11) * size) + i] = 0x20;
            grid[(i * size) + (size - 9)] = 0x20;
            grid[(i * size) + (size - 10)] = 0x20;
            grid[(i * size) + (size - 11)] = 0x20;
        }
    }
}

static int qr_cwbit(const unsigned char *fullstream, const int i) {

    if (fullstream[(i >> 3)] & (0x80 >> (i & 0x07))) {
        return 1;
    }

    return 0;
}

static void qr_populate_grid(unsigned char *grid, const int h_size, const int v_size, const unsigned char *fullstream,
            const int cw) {
    const int not_rmqr = v_size == h_size;
    const int x_start = h_size - (not_rmqr ? 2 : 3); /* For rMQR allow for righthand vertical timing pattern */
    int direction = 1; /* up */
    int row = 0; /* right hand side */

    int i, n, y;

    n = cw * 8;
    y = v_size - 1;
    i = 0;
    while (i < n) {
        int x = x_start - (row * 2);
        int r = y * h_size;

        if ((x < 6) && (not_rmqr))
            x--; /* skip over vertical timing pattern */

        if (!(grid[r + (x + 1)] & 0xf0)) {
            grid[r + (x + 1)] = qr_cwbit(fullstream, i);
            i++;
        }

        if (i < n) {
            if (!(grid[r + x] & 0xf0)) {
                grid[r + x] = qr_cwbit(fullstream, i);
                i++;
            }
        }

        if (direction) {
            y--;
            if (y == -1) {
                /* reached the top */
                row++;
                y = 0;
                direction = 0;
            }
        } else {
            y++;
            if (y == v_size) {
                /* reached the bottom */
                row++;
                y = v_size - 1;
                direction = 1;
            }
        }
    }
}

#ifdef ZINTLOG
static void append_log(const unsigned char log) {
    FILE *file;

    if ((file = fopen("zintlog.txt", "a+"))) {
        fprintf(file, "%02X", log);
        (void) fclose(file);
    }
}

static void write_log(const char log[]) {
    FILE *file;

    if ((file = fopen("zintlog.txt", "a+"))) {
        fprintf(file, "%s\n", log); /*writes*/
        (void) fclose(file);
    }
}
#endif

static int qr_evaluate(unsigned char *local, const int size) {
    static const unsigned char h1011101[7] = { 1, 0, 1, 1, 1, 0, 1 };

    int x, y, r, k, block;
    int result = 0;
    char state;
    int dark_mods;
    double percentage;
    int a, b, afterCount, beforeCount;
#ifdef ZINTLOG
    int result_b = 0;
    char str[15];
#endif

    /* Suppresses clang-tidy clang-analyzer-core.UndefinedBinaryOperatorResult warnings */
    assert(size > 0);

#ifdef ZINTLOG
    /* bitmask output */
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            append_log(local[(y * size) + x]);
        }
        write_log("");
    }
#endif

    /* Test 1: Adjacent modules in row/column in same colour */
    /* Vertical */
    for (x = 0; x < size; x++) {
        block = 0;
        state = 0;
        for (y = 0; y < size; y++) {
            if (local[(y * size) + x] == state) {
                block++;
            } else {
                if (block >= 5) {
                    result += block - 2;
                }
                block = 1;
                state = local[(y * size) + x];
            }
        }
        if (block >= 5) {
            result += block - 2;
        }
    }

    /* Horizontal */
    dark_mods = 0; /* Count dark mods simultaneously (see Test 4 below) */
    for (y = 0; y < size; y++) {
        r = y * size;
        block = 0;
        state = 0;
        for (x = 0; x < size; x++) {
            if (local[r + x] == state) {
                block++;
            } else {
                if (block >= 5) {
                    result += block - 2;
                }
                block = 1;
                state = local[r + x];
            }
            if (state) {
                dark_mods++;
            }
        }
        if (block >= 5) {
            result += block - 2;
        }
    }

#ifdef ZINTLOG
    /* output Test 1 */
    sprintf(str, "%d", result);
    result_b = result;
    write_log(str);
#endif

    /* Test 2: Block of modules in same color */
    for (x = 0; x < size - 1; x++) {
        for (y = 0; y < size - 1; y++) {
            k = local[(y * size) + x];
            if (((k == local[((y + 1) * size) + x]) &&
                    (k == local[(y * size) + (x + 1)])) &&
                    (k == local[((y + 1) * size) + (x + 1)])) {
                result += 3;
            }
        }
    }

#ifdef ZINTLOG
    /* output Test 2 */
    sprintf(str, "%d", result - result_b);
    result_b = result;
    write_log(str);
#endif

    /* Test 3: 1:1:3:1:1 ratio pattern in row/column */
    /* Vertical */
    for (x = 0; x < size; x++) {
        for (y = 0; y <= (size - 7); y++) {
            if (local[y * size + x] && !local[(y + 1) * size + x] && local[(y + 2) * size + x] &&
                    local[(y + 3) * size + x] && local[(y + 4) * size + x] &&
                    !local[(y + 5) * size + x] && local[(y + 6) * size + x]) {
                /* Pattern found, check before and after */
                beforeCount = 0;
                for (b = (y - 1); b >= (y - 4); b--) {
                    if (b < 0) { /* Count < edge as whitespace */
                        beforeCount = 4;
                        break;
                    }
                    if (local[(b * size) + x]) {
                        break;
                    }
                    beforeCount++;
                }
                if (beforeCount == 4) {
                    /* Pattern is preceded by light area 4 modules wide */
                    result += 40;
                } else {
                    afterCount = 0;
                    for (a = (y + 7); a <= (y + 10); a++) {
                        if (a >= size) { /* Count > edge as whitespace */
                            afterCount = 4;
                            break;
                        }
                        if (local[(a * size) + x]) {
                            break;
                        }
                        afterCount++;
                    }
                    if (afterCount == 4) {
                        /* Pattern is followed by light area 4 modules wide */
                        result += 40;
                    }
                }
                y += 3; /* Skip to next possible match */
            }
        }
    }

    /* Horizontal */
    for (y = 0; y < size; y++) {
        r = y * size;
        for (x = 0; x <= (size - 7); x++) {
            if (memcmp(local + r + x, h1011101, 7) == 0) {
                /* Pattern found, check before and after */
                beforeCount = 0;
                for (b = (x - 1); b >= (x - 4); b--) {
                    if (b < 0) { /* Count < edge as whitespace */
                        beforeCount = 4;
                        break;
                    }
                    if (local[r + b]) {
                        break;
                    }
                    beforeCount++;
                }

                if (beforeCount == 4) {
                    /* Pattern is preceded by light area 4 modules wide */
                    result += 40;
                } else {
                    afterCount = 0;
                    for (a = (x + 7); a <= (x + 10); a++) {
                        if (a >= size) { /* Count > edge as whitespace */
                            afterCount = 4;
                            break;
                        }
                        if (local[r + a]) {
                            break;
                        }
                        afterCount++;
                    }
                    if (afterCount == 4) {
                        /* Pattern is followed by light area 4 modules wide */
                        result += 40;
                    }
                }
                x += 3; /* Skip to next possible match */
            }
        }
    }

#ifdef ZINTLOG
    /* output Test 3 */
    sprintf(str, "%d", result - result_b);
    result_b = result;
    write_log(str);
#endif

    /* Test 4: Proportion of dark modules in entire symbol */
    percentage = (100.0 * dark_mods) / (size * size);
    k = (int) (fabs(percentage - 50.0) / 5.0);

    result += 10 * k;

#ifdef ZINTLOG
    /* output Test 4+summary */
    sprintf(str, "%d", result - result_b);
    write_log(str);
    write_log("==========");
    sprintf(str, "%d", result);
    write_log(str);
#endif

    return result;
}

/* Add format information to grid */
static void qr_add_format_info(unsigned char *grid, const int size, const int ecc_level, const int pattern) {
    int format = pattern;
    unsigned int seq;
    int i;

    switch (ecc_level) {
        case QR_LEVEL_L: format |= 0x08;
            break;
        case QR_LEVEL_Q: format |= 0x18;
            break;
        case QR_LEVEL_H: format |= 0x10;
            break;
    }

    seq = qr_annex_c[format];

    for (i = 0; i < 6; i++) {
        grid[(i * size) + 8] |= (seq >> i) & 0x01;
    }

    for (i = 0; i < 8; i++) {
        grid[(8 * size) + (size - i - 1)] |= (seq >> i) & 0x01;
    }

    for (i = 0; i < 6; i++) {
        grid[(8 * size) + (5 - i)] |= (seq >> (i + 9)) & 0x01;
    }

    for (i = 0; i < 7; i++) {
        grid[(((size - 7) + i) * size) + 8] |= (seq >> (i + 8)) & 0x01;
    }

    grid[(7 * size) + 8] |= (seq >> 6) & 0x01;
    grid[(8 * size) + 8] |= (seq >> 7) & 0x01;
    grid[(8 * size) + 7] |= (seq >> 8) & 0x01;
}

static int qr_apply_bitmask(unsigned char *grid, const int size, const int ecc_level, const int user_mask,
            const int fast_encode, const int debug_print) {
    int x, y;
    int r, k;
    int bit;
    int pattern, penalty[8];
    int best_pattern;
    int size_squared = size * size;
    unsigned char *mask = (unsigned char *) z_alloca(size_squared);
    unsigned char *local = (unsigned char *) z_alloca(size_squared);
#ifdef ZINTLOG
    char str[15];
#endif

    /* Perform data masking */
    memset(mask, 0, size_squared);
    for (y = 0; y < size; y++) {
        r = y * size;
        for (x = 0; x < size; x++) {

            /* all eight bitmask variants are encoded in the 8 bits of the bytes that make up the mask array. */
            if (!(grid[r + x] & 0xf0)) { /* exclude areas not to be masked. */
                if (((y + x) & 1) == 0) {
                    mask[r + x] |= 0x01;
                }
                if (!fast_encode) {
                    if ((y & 1) == 0) {
                        mask[r + x] |= 0x02;
                    }
                }
                if ((x % 3) == 0) {
                    mask[r + x] |= 0x04;
                }
                if (!fast_encode) {
                    if (((y + x) % 3) == 0) {
                        mask[r + x] |= 0x08;
                    }
                }
                if ((((y / 2) + (x / 3)) & 1) == 0) {
                    mask[r + x] |= 0x10;
                }
                if (!fast_encode) {
                    if ((y * x) % 6 == 0) { /* Equivalent to (y * x) % 2 + (y * x) % 3 == 0 */
                        mask[r + x] |= 0x20;
                    }
                    if (((((y * x) & 1) + ((y * x) % 3)) & 1) == 0) {
                        mask[r + x] |= 0x40;
                    }
                }
                if (((((y + x) & 1) + ((y * x) % 3)) & 1) == 0) {
                    mask[r + x] |= 0x80;
                }
            }
        }
    }

    if (user_mask) {
        best_pattern = user_mask - 1;
    } else {
        /* all eight bitmask variants have been encoded in the 8 bits of the bytes
         * that make up the mask array. select them for evaluation according to the
         * desired pattern.*/
        best_pattern = 0;
        for (pattern = 0; pattern < 8; pattern++) {
            if (fast_encode && pattern != 0 && pattern != 2 && pattern != 4 && pattern != 7) {
                continue;
            }
            bit = 1 << pattern;
            for (k = 0; k < size_squared; k++) {
                if (mask[k] & bit) {
                    local[k] = grid[k] ^ 0x01;
                } else {
                    local[k] = grid[k] & 0x0f;
                }
            }
            qr_add_format_info(local, size, ecc_level, pattern);

            penalty[pattern] = qr_evaluate(local, size);

            if (penalty[pattern] < penalty[best_pattern]) {
                best_pattern = pattern;
            }
        }
    }

    if (debug_print) {
        printf("Mask: %d (%s)", best_pattern, user_mask ? "specified" : fast_encode ? "fast automatic": "automatic");
        if (!user_mask) {
            if (fast_encode) {
                printf(" 0:%d  2:%d  4:%d  7:%d", penalty[0], penalty[2], penalty[4], penalty[7]);
            } else {
                for (pattern = 0; pattern < 8; pattern++) printf(" %d:%d", pattern, penalty[pattern]);
            }
        }
        fputc('\n', stdout);
    }

#ifdef ZINTLOG
    sprintf(str, "%d", best_pattern);
    write_log("chose pattern:");
    write_log(str);
#endif

    /* Apply mask */
    if (!user_mask && best_pattern == 7) { /* Reuse last */
        memcpy(grid, local, size_squared);
    } else {
        bit = 1 << best_pattern;
        for (y = 0; y < size_squared; y++) {
            if (mask[y] & bit) {
                grid[y] ^= 0x01;
            }
        }
    }

    return best_pattern;
}

/* Add version information */
static void qr_add_version_info(unsigned char *grid, const int size, const int version) {
    int i;

    unsigned int version_data = qr_annex_d[version - 7];
    for (i = 0; i < 6; i++) {
        grid[((size - 11) * size) + i] |= (version_data >> (i * 3)) & 1;
        grid[((size - 10) * size) + i] |= (version_data >> ((i * 3) + 1)) & 1;
        grid[((size - 9) * size) + i] |= (version_data >> ((i * 3) + 2)) & 1;
        grid[(i * size) + (size - 11)] |= (version_data >> (i * 3)) & 1;
        grid[(i * size) + (size - 10)] |= (version_data >> ((i * 3) + 1)) & 1;
        grid[(i * size) + (size - 9)] |= (version_data >> ((i * 3) + 2)) & 1;
    }
}

/* Find the length of the block starting from 'start' */
static int qr_blockLength(const int start, const char mode[], const int length) {
    int i;
    int count = 0;
    char start_mode = mode[start];

    i = start;

    do {
        count++;
    } while (((i + count) < length) && (mode[i + count] == start_mode));

    return count;
}

/* Calculate the actual bitlength of the proposed binary string */
static int qr_calc_binlen(const int version, char mode[], const unsigned int ddata[], const int length,
            const int mode_preset, const int gs1, const int eci, const int debug_print) {
    int i, j;
    char currentMode;
    int count = 0;
    int alphalength;
    int blocklength;

    if (!mode_preset) {
        qr_define_mode(mode, ddata, length, gs1, version, debug_print);
    }

    currentMode = ' '; /* Null */

    if (eci != 0) { /* Not applicable to MICROQR */
        count += 4;
        if (eci <= 127) {
            count += 8;
        } else if (eci <= 16383) {
            count += 16;
        } else {
            count += 24;
        }
    }

    for (i = 0; i < length; i++) {
        if (mode[i] != currentMode) {
            count += qr_mode_bits(version) + qr_cci_bits(version, mode[i]);
            blocklength = qr_blockLength(i, mode, length);
            switch (mode[i]) {
                case 'K':
                    count += (blocklength * 13);
                    break;
                case 'B':
                    for (j = i; j < (i + blocklength); j++) {
                        if (ddata[j] > 0xff) {
                            count += 16;
                        } else {
                            count += 8;
                        }
                    }
                    break;
                case 'A':
                    alphalength = blocklength;
                    if (gs1) {
                        /* In alphanumeric mode % becomes %% */
                        for (j = i; j < (i + blocklength); j++) {
                            if (ddata[j] == '%') {
                                alphalength++;
                            }
                        }
                    }
                    switch (alphalength % 2) {
                        case 0:
                            count += (alphalength / 2) * 11;
                            break;
                        case 1:
                            count += ((alphalength - 1) / 2) * 11;
                            count += 6;
                            break;
                    }
                    break;
                case 'N':
                    switch (blocklength % 3) {
                        case 0:
                            count += (blocklength / 3) * 10;
                            break;
                        case 1:
                            count += ((blocklength - 1) / 3) * 10;
                            count += 4;
                            break;
                        case 2:
                            count += ((blocklength - 2) / 3) * 10;
                            count += 7;
                            break;
                    }
                    break;
            }
            currentMode = mode[i];
        }
    }

    return count;
}

/* Call `qr_calc_binlen()` on each segment */
static int qr_calc_binlen_segs(const int version, char mode[], const unsigned int ddata[],
            const struct zint_seg segs[], const int seg_count, const struct zint_structapp *p_structapp,
            const int mode_preset, const int gs1, const int debug_print) {
    int i;
    int count = 0;
    const unsigned int *dd = ddata;
    char *m = mode;

    if (p_structapp) {
        count += 4 + 8 + 8;
    }

    if (gs1) { /* Not applicable to MICROQR */
        if (version < RMQR_VERSION) {
            count += 4;
        } else {
            count += 3;
        }
    }

    for (i = 0; i < seg_count; i++) {
        count += qr_calc_binlen(version, m, dd, segs[i].length, mode_preset, gs1, segs[i].eci, debug_print);
        m += segs[i].length;
        dd += segs[i].length;
    }

    if (debug_print) {
        printf("Estimated Binary Length: %d (version %d, gs1 %d)\n", count, version, gs1);
    }

    return count;
}

/* Helper to process source data into `ddata` array */
static int qr_prep_data(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count,
            unsigned int ddata[]) {
    int warn_number = 0;
    int i;
    /* If ZINT_FULL_MULTIBYTE use Kanji mode in DATA_MODE or for non-Shift JIS in UNICODE_MODE */
    const int full_multibyte = (symbol->option_3 & 0xFF) == ZINT_FULL_MULTIBYTE;

    if ((symbol->input_mode & 0x07) == DATA_MODE) {
        sjis_cpy_segs(segs, seg_count, ddata, full_multibyte);
    } else {
        unsigned int *dd = ddata;
        for (i = 0; i < seg_count; i++) {
            int done = 0;
            if (segs[i].eci != 20 || seg_count > 1) { /* Unless ECI 20 (Shift JIS) or have multiple segments */
                /* Try other encodings (ECI 0 defaults to ISO/IEC 8859-1) */
                int error_number = sjis_utf8_to_eci(segs[i].eci, segs[i].source, &segs[i].length, dd, full_multibyte);
                if (error_number == 0) {
                    done = 1;
                } else if (segs[i].eci || seg_count > 1) {
                    return errtxtf(error_number, symbol, 575, "Invalid character in input for ECI '%d'", segs[i].eci);
                }
            }
            if (!done) {
                /* Try Shift-JIS */
                int error_number = sjis_utf8(symbol, segs[i].source, &segs[i].length, dd);
                if (error_number != 0) {
                    return error_number;
                }
                if (segs[i].eci != 20) {
                    warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 760,
                                            "Converted to Shift JIS but no ECI specified");
                }
            }
            dd += segs[i].length;
        }
    }

    return warn_number;
}

INTERNAL int qrcode(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int warn_number;
    int i, j, est_binlen, prev_est_binlen;
    int ecc_level, autosize, version, max_cw, target_codewords, blocks, size;
    int bitmask;
    int user_mask;
    int canShrink;
    int size_squared;
    const struct zint_structapp *p_structapp = NULL;
    const int gs1 = ((symbol->input_mode & 0x07) == GS1_MODE);
    const int fast_encode = symbol->input_mode & FAST_MODE;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    const int eci_length_segs = get_eci_length_segs(segs, seg_count);
    struct zint_seg *local_segs = (struct zint_seg *) z_alloca(sizeof(struct zint_seg) * seg_count);
    unsigned int *ddata = (unsigned int *) z_alloca(sizeof(unsigned int) * eci_length_segs);
    char *mode = (char *) z_alloca(eci_length_segs);
    char *prev_mode = (char *) z_alloca(eci_length_segs);
    unsigned char *datastream;
    unsigned char *fullstream;
    unsigned char *grid;

    user_mask = (symbol->option_3 >> 8) & 0x0F; /* User mask is pattern + 1, so >= 1 and <= 8 */
    if (user_mask > 8) {
        user_mask = 0; /* Ignore */
    }

    segs_cpy(symbol, segs, seg_count, local_segs); /* Shallow copy (needed to set default ECIs & protect lengths) */

    warn_number = qr_prep_data(symbol, local_segs, seg_count, ddata);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    if (symbol->structapp.count) {
        if (symbol->structapp.count < 2 || symbol->structapp.count > 16) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 750,
                            "Structured Append count '%d' out of range (2 to 16)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 751,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }
        if (symbol->structapp.id[0]) {
            int id, id_len;

            for (id_len = 1; id_len < 4 && symbol->structapp.id[id_len]; id_len++);

            if (id_len > 3) { /* Max value 255 */
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 752,
                                "Structured Append ID length %d too long (3 digit maximum)", id_len);
            }

            id = to_int((const unsigned char *) symbol->structapp.id, id_len);
            if (id == -1) {
                return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 753, "Invalid Structured Append ID (digits only)");
            }
            if (id > 255) {
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 754,
                                "Structured Append ID value '%d' out of range (0 to 255)", id);
            }
        }
        p_structapp = &symbol->structapp;
    }

    /* GS1 General Specifications 22.0 section 5.7.3 says Structured Append and ECIs not supported
       for GS1 QR Code so check and return ZINT_WARN_NONCOMPLIANT if either true */
    if (gs1 && warn_number == 0) {
        for (i = 0; i < seg_count; i++) {
            if (local_segs[i].eci) {
                warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 755,
                                        "Using ECI in GS1 mode not supported by GS1 standards");
                break;
            }
        }
        if (warn_number == 0 && p_structapp) {
            warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 756,
                                    "Using Structured Append in GS1 mode not supported by GS1 standards");
        }
    }

    est_binlen = qr_calc_binlen_segs(40, mode, ddata, local_segs, seg_count, p_structapp, 0 /*mode_preset*/, gs1,
                    debug_print);

    if ((symbol->option_1 >= 1) && (symbol->option_1 <= 4)) {
        ecc_level = symbol->option_1 - 1;
    } else {
        ecc_level = QR_LEVEL_L;
    }
    max_cw = qr_data_codewords[ecc_level][39];

    if (est_binlen > (8 * max_cw)) {
        if (ecc_level == QR_LEVEL_L) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 567, "Input too long, requires %1$d codewords (maximum %2$d)",
                            (est_binlen + 7) / 8, max_cw);
        }
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 561,
                        "Input too long for ECC level %1$c, requires %2$d codewords (maximum %3$d)",
                        qr_ecc_level_names[ecc_level], (est_binlen + 7) / 8, max_cw);
    }

    autosize = 40;
    for (i = 39; i >= 0; i--) {
        if ((8 * qr_data_codewords[ecc_level][i]) >= est_binlen) {
            autosize = i + 1;
        }
    }
    if (autosize != 40) {
        /* Save version 40 estimate in case incorrect costings in `qr_define_mode()` lead to its `mode` being better
           than current lower version one */
        prev_est_binlen = est_binlen;
        est_binlen = qr_calc_binlen_segs(autosize, mode, ddata, local_segs, seg_count, p_structapp, 0 /*mode_preset*/,
                        gs1, debug_print);
        if (prev_est_binlen < est_binlen) { /* Shouldn't happen */
            assert(0); /* Not reached (hopefully) */
            /* Defensively use version 40 `mode` to avoid crashes (ticket #300) */
            est_binlen = qr_calc_binlen_segs(40, mode, ddata, local_segs, seg_count, p_structapp, 0 /*mode_preset*/,
                            gs1, debug_print);
            assert(est_binlen == prev_est_binlen);
        }
    }

    /* Now see if the optimised binary will fit in a smaller symbol. */
    canShrink = 1;

    do {
        if (autosize == 1) {
            canShrink = 0;
        } else {
            prev_est_binlen = est_binlen;
            memcpy(prev_mode, mode, eci_length_segs);
            est_binlen = qr_calc_binlen_segs(autosize - 1, mode, ddata, local_segs, seg_count, p_structapp,
                            0 /*mode_preset*/, gs1, debug_print);

            if ((8 * qr_data_codewords[ecc_level][autosize - 2]) < est_binlen) {
                canShrink = 0;
            }

            if (canShrink == 1) {
                /* Optimisation worked - data will fit in a smaller symbol */
                autosize--;
            } else {
                /* Data did not fit in the smaller symbol, revert to original size */
                est_binlen = prev_est_binlen;
                memcpy(mode, prev_mode, eci_length_segs);
            }
        }
    } while (canShrink == 1);

    version = autosize;

    if ((symbol->option_2 >= 1) && (symbol->option_2 <= 40)) {
        /* If the user has selected a larger symbol than the smallest available,
         then use the size the user has selected, and re-optimise for this
         symbol size.
         */
        if (symbol->option_2 > version) {
            version = symbol->option_2;
            est_binlen = qr_calc_binlen_segs(symbol->option_2, mode, ddata, local_segs, seg_count, p_structapp,
                            0 /*mode_preset*/, gs1, debug_print);
        }

        if (symbol->option_2 < version) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 569,
                        "Input too long for Version %1$d-%2$c, requires %3$d codewords (maximum %4$d)",
                        symbol->option_2, qr_ecc_level_names[ecc_level], (est_binlen + 7) / 8,
                        qr_data_codewords[ecc_level][symbol->option_2 - 1]);
        }
    }

    /* Ensure maxium error correction capacity unless user-specified */
    if (symbol->option_1 == -1 || symbol->option_1 - 1 != ecc_level) {
        if (est_binlen <= qr_data_codewords[QR_LEVEL_H][version - 1] * 8) {
            ecc_level = QR_LEVEL_H;
        } else if (est_binlen <= qr_data_codewords[QR_LEVEL_Q][version - 1] * 8) {
            ecc_level = QR_LEVEL_Q;
        } else if (est_binlen <= qr_data_codewords[QR_LEVEL_M][version - 1] * 8) {
            ecc_level = QR_LEVEL_M;
        }
    }

    target_codewords = qr_data_codewords[ecc_level][version - 1];
    blocks = qr_blocks[ecc_level][version - 1];

    if (debug_print) {
        printf("Minimum codewords: %d\n", (est_binlen + 7) / 8);
        printf("Selected version: %d-%c (%dx%d)\n",
                version, qr_ecc_level_names[ecc_level], qr_sizes[version - 1], qr_sizes[version - 1]);
        printf("Number of data codewords in symbol: %d\n", target_codewords);
        printf("Number of ECC blocks: %d\n", blocks);
    }

    datastream = (unsigned char *) z_alloca(target_codewords + 1);
    fullstream = (unsigned char *) z_alloca(qr_total_codewords[version - 1] + 1);

    (void) qr_binary_segs(datastream, version, target_codewords, mode, ddata, local_segs, seg_count, p_structapp, gs1,
                    est_binlen, debug_print);
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) debug_test_codeword_dump(symbol, datastream, target_codewords);
#endif
    qr_add_ecc(fullstream, datastream, version, target_codewords, blocks, debug_print);

    size = qr_sizes[version - 1];
    size_squared = size * size;

    grid = (unsigned char *) z_alloca(size_squared);
    memset(grid, 0, size_squared);

    qr_setup_grid(grid, size, version);
    qr_populate_grid(grid, size, size, fullstream, qr_total_codewords[version - 1]);

    if (version >= 7) {
        qr_add_version_info(grid, size, version);
    }

    bitmask = qr_apply_bitmask(grid, size, ecc_level, user_mask, fast_encode, debug_print);

    qr_add_format_info(grid, size, ecc_level, bitmask);

    symbol->width = size;
    symbol->rows = size;

    for (i = 0; i < size; i++) {
        int r = i * size;
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

/* Write terminator, padding & ECC */
static int microqr_end(struct zint_symbol *symbol, char binary_data[], int bp, const int ecc_level,
            const int version) {
    int i, j;
    int bits_left;
    unsigned char data_blocks[17];
    unsigned char ecc_blocks[15];
    rs_t rs;

    const int terminator_bits = qr_terminator_bits(MICROQR_VERSION + version);
    const int bits_total = microqr_data[ecc_level][version][0];
    const int data_codewords = microqr_data[ecc_level][version][1];
    const int ecc_codewords = microqr_data[ecc_level][version][2];
    const int bits_end = version == 0 || version == 2 ? 4 : 8;

    /* Add terminator */
    bits_left = bits_total - bp;
    if (bits_left <= terminator_bits) {
        if (bits_left) {
            bp = bin_append_posn(0, bits_left, binary_data, bp);
            bits_left = 0;
        }
    } else {
        bp = bin_append_posn(0, terminator_bits, binary_data, bp);
        bits_left -= terminator_bits;
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("M%d Terminated binary (%d): %.*s (bits_left %d)\n", version + 1, bp, bp, binary_data, bits_left);
    }

    /* Manage last (4-bit) block */
    if (bits_end == 4 && bits_left && bits_left <= 4) {
        bp = bin_append_posn(0, bits_left, binary_data, bp);
        bits_left = 0;
    }

    if (bits_left) {
        /* Complete current byte */
        int remainder = 8 - (bp % 8);
        if (remainder != 8) {
            bp = bin_append_posn(0, remainder, binary_data, bp);
            bits_left -= remainder;
        }

        /* Add padding */
        if (bits_end == 4 && bits_left > 4) {
            bits_left -= 4;
        }
        remainder = bits_left / 8;
        for (i = 0; i < remainder; i++) {
            bp = bin_append_posn(i & 1 ? 0x11 : 0xEC, 8, binary_data, bp);
        }
        if (bits_end == 4) {
            bp = bin_append_posn(0, 4, binary_data, bp);
        }
    }
    assert((bp & 0x07) == 8 - bits_end);

    /* Copy data into codewords */
    for (i = 0; i < data_codewords; i++) {
        const int bits = i + 1 == data_codewords ? bits_end : 8;
        data_blocks[i] = 0;

        for (j = 0; j < bits; j++) {
            if (binary_data[(i * 8) + j] == '1') {
                data_blocks[i] |= 0x80 >> j;
            }
        }
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        char bp_buf[10];
        debug_test_codeword_dump(symbol, data_blocks, data_codewords);
        sprintf(bp_buf, "%d", bp); /* Append `bp` to detect padding errors */
        errtxt_adj(0, symbol, "%s (%s)", bp_buf);
    }
#endif

    /* Calculate Reed-Solomon error codewords */
    rs_init_gf(&rs, 0x11d);
    rs_init_code(&rs, ecc_codewords, 0);
    rs_encode(&rs, data_codewords, data_blocks, ecc_blocks);

    /* Add Reed-Solomon codewords to binary data */
    for (i = 0; i < ecc_codewords; i++) {
        bp = bin_append_posn(ecc_blocks[i], 8, binary_data, bp);
    }

    return bp;
}

static void microqr_setup_grid(unsigned char *grid, const int size) {
    int i, toggle = 1;

    /* Add timing patterns */
    for (i = 0; i < size; i++) {
        if (toggle == 1) {
            grid[i] = 0x21;
            grid[(i * size)] = 0x21;
            toggle = 0;
        } else {
            grid[i] = 0x20;
            grid[(i * size)] = 0x20;
            toggle = 1;
        }
    }

    /* Add finder patterns */
    qr_place_finder(grid, size, 0, 0);

    /* Add separators */
    for (i = 0; i < 7; i++) {
        grid[(7 * size) + i] = 0x10;
        grid[(i * size) + 7] = 0x10;
    }
    grid[(7 * size) + 7] = 0x10;


    /* Reserve space for format information */
    for (i = 0; i < 8; i++) {
        grid[(8 * size) + i] |= 0x20;
        grid[(i * size) + 8] |= 0x20;
    }
    grid[(8 * size) + 8] |= 20;
}

static void microqr_populate_grid(unsigned char *grid, const int size, const char full_stream[], int bp) {
    int direction = 1; /* up */
    int row = 0; /* right hand side */
    int i;
    int y;

    y = size - 1;
    i = 0;
    do {
        int x = (size - 2) - (row * 2);

        if (!(grid[(y * size) + (x + 1)] & 0xf0)) {
            if (full_stream[i] == '1') {
                grid[(y * size) + (x + 1)] = 0x01;
            } else {
                grid[(y * size) + (x + 1)] = 0x00;
            }
            i++;
        }

        if (i < bp) {
            if (!(grid[(y * size) + x] & 0xf0)) {
                if (full_stream[i] == '1') {
                    grid[(y * size) + x] = 0x01;
                } else {
                    grid[(y * size) + x] = 0x00;
                }
                i++;
            }
        }

        if (direction) {
            y--;
        } else {
            y++;
        }
        if (y == 0) {
            /* reached the top */
            row++;
            y = 1;
            direction = 0;
        }
        if (y == size) {
            /* reached the bottom */
            row++;
            y = size - 1;
            direction = 1;
        }
    } while (i < bp);
}

static int microqr_evaluate(const unsigned char *grid, const int size, const int pattern) {
    int sum1, sum2, i, filter = 0, retval;

    switch (pattern) {
        case 0: filter = 0x01;
            break;
        case 1: filter = 0x02;
            break;
        case 2: filter = 0x04;
            break;
        case 3: filter = 0x08;
            break;
    }

    sum1 = 0;
    sum2 = 0;
    for (i = 1; i < size; i++) {
        if (grid[(i * size) + size - 1] & filter) {
            sum1++;
        }
        if (grid[((size - 1) * size) + i] & filter) {
            sum2++;
        }
    }

    if (sum1 <= sum2) {
        retval = (sum1 * 16) + sum2;
    } else {
        retval = (sum2 * 16) + sum1;
    }

    return retval;
}

static int microqr_apply_bitmask(unsigned char *grid, const int size, const int user_mask, const int debug_print) {
    int x, y;
    int r, k;
    int bit;
    int pattern, value[4];
    int best_pattern;
    int size_squared = size * size;
    unsigned char *mask = (unsigned char *) z_alloca(size_squared);
    unsigned char *eval = (unsigned char *) z_alloca(size_squared);

    /* Perform data masking */
    memset(mask, 0, size_squared);
    for (y = 0; y < size; y++) {
        r = y * size;
        for (x = 0; x < size; x++) {

            if (!(grid[r + x] & 0xf0)) {
                if ((y & 1) == 0) {
                    mask[r + x] |= 0x01;
                }

                if ((((y / 2) + (x / 3)) & 1) == 0) {
                    mask[r + x] |= 0x02;
                }

                if (((((y * x) & 1) + ((y * x) % 3)) & 1) == 0) {
                    mask[r + x] |= 0x04;
                }

                if (((((y + x) & 1) + ((y * x) % 3)) & 1) == 0) {
                    mask[r + x] |= 0x08;
                }
            }
        }
    }

    if (user_mask) {
        best_pattern = user_mask - 1;
    } else {
        for (k = 0; k < size_squared; k++) {
            if (grid[k] & 0x01) {
                eval[k] = mask[k] ^ 0xff;
            } else {
                eval[k] = mask[k];
            }
        }


        /* Evaluate result */
        best_pattern = 0;
        for (pattern = 0; pattern < 4; pattern++) {
            value[pattern] = microqr_evaluate(eval, size, pattern);
            if (value[pattern] > value[best_pattern]) {
                best_pattern = pattern;
            }
        }
    }

    if (debug_print) {
        printf("Mask: %d (%s)", best_pattern, user_mask ? "specified" : "automatic");
        if (!user_mask) {
            for (pattern = 0; pattern < 4; pattern++) printf(" %d:%d", pattern, value[pattern]);
        }
        fputc('\n', stdout);
    }

    /* Apply mask */
    bit = 1 << best_pattern;
    for (k = 0; k < size_squared; k++) {
        if (mask[k] & bit) {
            grid[k] ^= 0x01;
        }
    }

    return best_pattern;
}

INTERNAL int microqr(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, size, j;
    char full_stream[200];
    int bp;
    int full_multibyte;
    int user_mask;

    unsigned int ddata[40];
    char mode[40];
    int alpha_used = 0, byte_or_kanji_used = 0;
    int version_valid[4];
    int binary_count[4];
    int ecc_level, version;
    int bitmask, format, format_full;
    int size_squared;
    struct zint_seg segs[1];
    const int seg_count = 1;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    unsigned char *grid;

    if (length > 35) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 562, "Input length %d too long (maximum 35)", length);
    }

    /* Check option 1 in combination with option 2 */
    ecc_level = QR_LEVEL_L;
    if (symbol->option_1 >= 1 && symbol->option_1 <= 4) {
        if (symbol->option_1 == 4) {
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 566, "Error correction level H not available");
        }
        if (symbol->option_2 >= 1 && symbol->option_2 <= 4) {
            if (symbol->option_2 == 1 && symbol->option_1 != 1) {
                return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 574,
                                "Version M1 supports error correction level L only");
            }
            if (symbol->option_2 != 4 && symbol->option_1 == 3) {
                return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 563, "Error correction level Q requires Version M4");
            }
        }
        ecc_level = symbol->option_1 - 1;
    }

    /* If ZINT_FULL_MULTIBYTE use Kanji mode in DATA_MODE or for non-Shift JIS in UNICODE_MODE */
    full_multibyte = (symbol->option_3 & 0xFF) == ZINT_FULL_MULTIBYTE;
    user_mask = (symbol->option_3 >> 8) & 0x0F; /* User mask is pattern + 1, so >= 1 and <= 4 */
    if (user_mask > 4) {
        user_mask = 0; /* Ignore */
    }

    if ((symbol->input_mode & 0x07) == DATA_MODE) {
        sjis_cpy(source, &length, ddata, full_multibyte);
    } else {
        /* Try ISO 8859-1 conversion first */
        int error_number = sjis_utf8_to_eci(3, source, &length, ddata, full_multibyte);
        if (error_number != 0) {
            /* Try Shift-JIS */
            error_number = sjis_utf8(symbol, source, &length, ddata);
            if (error_number != 0) {
                return error_number;
            }
        }
    }

    /* Determine if alpha (excluding numerics), byte or kanji used */
    for (i = 0; i < length && (alpha_used == 0 || byte_or_kanji_used == 0); i++) {
        if (!z_isdigit(ddata[i])) {
            if (qr_is_alpha(ddata[i], 0 /*gs1*/)) {
                alpha_used = 1;
            } else {
                byte_or_kanji_used = 1;
            }
        }
    }

    for (i = 0; i < 4; i++) {
        version_valid[i] = 1;
    }

    /* Eliminate possible versions depending on type of content */
    if (byte_or_kanji_used) {
        version_valid[0] = 0;
        version_valid[1] = 0;
    } else if (alpha_used) {
        version_valid[0] = 0;
    }

    /* Eliminate possible versions depending on error correction level specified */
    if (ecc_level == QR_LEVEL_Q) {
        version_valid[0] = 0;
        version_valid[1] = 0;
        version_valid[2] = 0;
    } else if (ecc_level == QR_LEVEL_M) {
        version_valid[0] = 0;
    }

    segs[0].source = source;
    segs[0].length = length;
    segs[0].eci = 0;

    /* Determine length of binary data */
    for (i = 0; i < 4; i++) {
        if (version_valid[i]) {
            binary_count[i] = qr_calc_binlen_segs(MICROQR_VERSION + i, mode, ddata, segs, seg_count,
                                NULL /*p_structapp*/, 0 /*mode_preset*/, 0 /*gs1*/, debug_print);
        } else {
            binary_count[i] = 128 + 1;
        }
    }

    /* Eliminate possible versions depending on binary length and error correction level specified */
    if (binary_count[3] > microqr_data[ecc_level][3][0]) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 565,
                        "Input too long for Version M4-%1$c, requires %2$d codewords (maximum %3$d)",
                        qr_ecc_level_names[ecc_level], (binary_count[3] + 7) / 8, microqr_data[ecc_level][3][1]);
    }
    for (i = 0; i < 3; i++) {
        if (binary_count[i] > microqr_data[ecc_level][i][0]) {
            version_valid[i] = 0;
        }
    }

    /* Auto-select lowest valid size */
    version = 3;
    if (version_valid[2]) {
        version = 2;
    }
    if (version_valid[1]) {
        version = 1;
    }
    if (version_valid[0]) {
        version = 0;
    }

    /* Get version from user */
    if ((symbol->option_2 >= 1) && (symbol->option_2 <= 4)) {
        if (symbol->option_2 == 1 && (i = not_sane(NEON_F, source, length))) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 758,
                            "Invalid character at position %d in input for Version M1 (digits only)", i);
        } else if (symbol->option_2 == 2 && not_sane(QR_ALPHA, source, length)) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 759,
                            "Invalid character in input for Version M2 (digits, A-Z, space and \"$%*+-./:\" only)");
        }
        if (symbol->option_2 - 1 >= version) {
            version = symbol->option_2 - 1;
        } else {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 570,
                            "Input too long for Version M%1$d-%2$c, requires %3$d codewords (maximum %4$d)",
                            symbol->option_2, qr_ecc_level_names[ecc_level], (binary_count[version] + 7) / 8,
                            microqr_data[ecc_level][symbol->option_2 - 1][1]);
        }
    }

    /* If there is enough unused space then increase the error correction level, unless user-specified */
    if (version && (symbol->option_1 == -1 || symbol->option_1 - 1 != ecc_level)) {
        if (binary_count[version] <= microqr_data[QR_LEVEL_Q][version][0]) {
            ecc_level = QR_LEVEL_Q;
        } else if (binary_count[version] <= microqr_data[QR_LEVEL_M][version][0]) {
            ecc_level = QR_LEVEL_M;
        }
    }

    qr_define_mode(mode, ddata, length, 0 /*gs1*/, MICROQR_VERSION + version, debug_print);

    bp = qr_binary_segs((unsigned char *) full_stream, MICROQR_VERSION + version, 0 /*target_codewords*/, mode, ddata,
                    segs, seg_count, NULL /*p_structapp*/, 0 /*gs1*/, binary_count[version], debug_print);

    if (debug_print) printf("Binary (%d): %.*s\n", bp, bp, full_stream);

    bp = microqr_end(symbol, full_stream, bp, ecc_level, version);

    size = microqr_sizes[version];
    size_squared = size * size;

    grid = (unsigned char *) z_alloca(size_squared);
    memset(grid, 0, size_squared);

    microqr_setup_grid(grid, size);
    microqr_populate_grid(grid, size, full_stream, bp);
    bitmask = microqr_apply_bitmask(grid, size, user_mask, debug_print);

    /* Add format data */
    format = version ? (version - 1) * 2 + ecc_level + 1 : 0;

    if (debug_print) {
        printf("Version: M%d-%c, Size: %dx%d, Format: %d\n",
                version + 1, qr_ecc_level_names[ecc_level], size, size, format);
    }

    format_full = qr_annex_c1[(format << 2) + bitmask];

    for (i = 1; i <= 8; i++) {
        grid[(8 * size) + i] |= (format_full >> (15 - i)) & 0x01;
    }
    for (i = 7; i >= 1; i--) {
        grid[(i * size) + 8] |= (format_full >> (i - 1)) & 0x01;
    }

    symbol->width = size;
    symbol->rows = size;

    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            if (grid[(i * size) + j] & 0x01) {
                set_module(symbol, i, j);
            }
        }
        symbol->row_height[i] = 1;
    }
    symbol->height = size;

    return 0;
}

/* For UPNQR the symbol size and error correction capacity is fixed */
INTERNAL int upnqr(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, j, r, est_binlen;
    int ecc_level, version, target_codewords, blocks, size;
    int bitmask, error_number;
    int user_mask;
    int size_squared;
    struct zint_seg segs[1];
    const int seg_count = 1;
    const int fast_encode = symbol->input_mode & FAST_MODE;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    unsigned char *datastream;
    unsigned char *fullstream;
    unsigned char *grid;
    unsigned int *ddata = (unsigned int *) z_alloca(sizeof(unsigned int) * length);
    char *mode = (char *) z_alloca(length + 1);
    unsigned char *preprocessed = (unsigned char *) z_alloca(length + 1);

    symbol->eci = 4; /* Set before any processing */

    user_mask = (symbol->option_3 >> 8) & 0x0F; /* User mask is pattern + 1, so >= 1 and <= 8 */
    if (user_mask > 8) {
        user_mask = 0; /* Ignore */
    }

    switch (symbol->input_mode & 0x07) {
        case DATA_MODE:
            /* Input is already in ISO-8859-2 format */
            for (i = 0; i < length; i++) {
                ddata[i] = source[i];
                mode[i] = 'B';
            }
            break;
        case GS1_MODE: /* Should never happen as checked before being called */
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 571,
                            "UPNQR does not support GS1 data"); /* Not reached */
            break;
        case UNICODE_MODE:
            error_number = utf8_to_eci(4, source, preprocessed, &length);
            if (error_number != 0) {
                return errtxt(error_number, symbol, 572, "Invalid character in input for ECI '4'");
            }
            for (i = 0; i < length; i++) {
                ddata[i] = preprocessed[i];
                mode[i] = 'B';
            }
            break;
    }

    segs[0].source = source;
    segs[0].length = length;
    segs[0].eci = 4;

    est_binlen = qr_calc_binlen_segs(15, mode, ddata, segs, seg_count, NULL /*p_structapp*/, 1 /*mode_preset*/,
                    0 /*gs1*/, debug_print);

    ecc_level = QR_LEVEL_M;

    if (est_binlen > 3320) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 573, "Input too long, requires %d codewords (maximum 415)",
                        (est_binlen + 7) / 8);
    }

    version = 15; /* 77 x 77 */

    target_codewords = qr_data_codewords[ecc_level][version - 1];
    blocks = qr_blocks[ecc_level][version - 1];

    datastream = (unsigned char *) z_alloca(target_codewords + 1);
    fullstream = (unsigned char *) z_alloca(qr_total_codewords[version - 1] + 1);

    (void) qr_binary_segs(datastream, version, target_codewords, mode, ddata, segs, seg_count, NULL /*p_structapp*/,
                    0 /*gs1*/, est_binlen, debug_print);
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) debug_test_codeword_dump(symbol, datastream, target_codewords);
#endif
    qr_add_ecc(fullstream, datastream, version, target_codewords, blocks, debug_print);

    size = qr_sizes[version - 1];
    size_squared = size * size;

    grid = (unsigned char *) z_alloca(size_squared);
    memset(grid, 0, size_squared);

    qr_setup_grid(grid, size, version);
    qr_populate_grid(grid, size, size, fullstream, qr_total_codewords[version - 1]);

    qr_add_version_info(grid, size, version);

    bitmask = qr_apply_bitmask(grid, size, ecc_level, user_mask, fast_encode, debug_print);

    qr_add_format_info(grid, size, ecc_level, bitmask);

    symbol->width = size;
    symbol->rows = size;

    for (i = 0; i < size; i++) {
        r = i * size;
        for (j = 0; j < size; j++) {
            if (grid[r + j] & 0x01) {
                set_module(symbol, i, j);
            }
        }
        symbol->row_height[i] = 1;
    }
    symbol->height = size;

    return 0;
}

static const char rmqr_version_names[38][8] = {
     "R7x43",   "R7x59",   "R7x77",   "R7x99",  "R7x139",   "R9x43",  "R9x59",   "R9x77",
     "R9x99",  "R9x139",  "R11x27",  "R11x43",  "R11x59",  "R11x77", "R11x99", "R11x139",
    "R13x27",  "R13x43",  "R13x59",  "R13x77",  "R13x99", "R13x139", "R15x43",  "R15x59",
    "R15x77",  "R15x99", "R15x139",  "R17x43",  "R17x59",  "R17x77", "R17x99", "R17x139",
      "R7xW",    "R9xW",   "R11xW",   "R13xW",   "R15xW",   "R17xW",
};

static void rmqr_setup_grid(unsigned char *grid, const int h_size, const int v_size) {
    int i, j;
    char alignment[] = {0x1F, 0x11, 0x15, 0x11, 0x1F};
    int h_version, finder_position;

    /* Add timing patterns - top and bottom */
    for (i = 0; i < h_size; i++) {
        if (i % 2) {
            grid[i] = 0x20;
            grid[((v_size - 1) * h_size) + i] = 0x20;
        } else {
            grid[i] = 0x21;
            grid[((v_size - 1) * h_size) + i] = 0x21;
        }
    }

    /* Add timing patterns - left and right */
    for (i = 0; i < v_size; i++) {
        if (i % 2) {
            grid[i * h_size] = 0x20;
            grid[(i * h_size) + (h_size - 1)] = 0x20;
        } else {
            grid[i * h_size] = 0x21;
            grid[(i * h_size) + (h_size - 1)] = 0x21;
        }
    }

    /* Add finder pattern */
    qr_place_finder(grid, h_size, 0, 0); /* This works because finder is always top left */

    /* Add finder sub-pattern to bottom right */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            if (alignment[j] & 0x10 >> i) {
                grid[((v_size - 5) * h_size) + (h_size * i) + (h_size - 5) + j] = 0x11;
            } else {
                grid[((v_size - 5) * h_size) + (h_size * i) + (h_size - 5) + j] = 0x10;
            }
        }
    }

    /* Add corner finder pattern - bottom left */
    grid[(v_size - 2) * h_size] = 0x11;
    grid[((v_size - 2) * h_size) + 1] = 0x10;
    grid[((v_size - 1) * h_size) + 1] = 0x11;

    /* Add corner finder pattern - top right */
    grid[h_size - 2] = 0x11;
    grid[(h_size * 2) - 2] = 0x10;
    grid[(h_size * 2) - 1] = 0x11;

    /* Add seperator */
    for (i = 0; i < 7; i++) {
        grid[(i * h_size) + 7] = 0x20;
    }
    if (v_size > 7) {
        /* Note for v_size = 9 this overrides the bottom right corner finder pattern */
        for (i = 0; i < 8; i++) {
            grid[(7 * h_size) + i] = 0x20;
        }
    }

    /* Add alignment patterns */
    if (h_size > 27) {
        h_version = 0; /* Suppress compiler warning [-Wmaybe-uninitialized] */
        for (i = 0; i < 5; i++) {
            if (h_size == rmqr_width[i]) {
                h_version = i;
                break;
            }
        }

        for (i = 0; i < 4; i++) {
            finder_position = rmqr_table_d1[(h_version * 4) + i];

            if (finder_position != 0) {
                for (j = 0; j < v_size; j++) {
                    if (j % 2) {
                        grid[(j * h_size) + finder_position] = 0x10;
                    } else {
                        grid[(j * h_size) + finder_position] = 0x11;
                    }
                }

                /* Top square */
                grid[h_size + finder_position - 1] = 0x11;
                grid[(h_size * 2) + finder_position - 1] = 0x11;
                grid[h_size + finder_position + 1] = 0x11;
                grid[(h_size * 2) + finder_position + 1] = 0x11;

                /* Bottom square */
                grid[(h_size * (v_size - 3)) + finder_position - 1] = 0x11;
                grid[(h_size * (v_size - 2)) + finder_position - 1] = 0x11;
                grid[(h_size * (v_size - 3)) + finder_position + 1] = 0x11;
                grid[(h_size * (v_size - 2)) + finder_position + 1] = 0x11;
            }
        }
    }

    /* Reserve space for format information */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 3; j++) {
            grid[(h_size * (i + 1)) + j + 8] = 0x20;
            grid[(h_size * (v_size - 6)) + (h_size * i) + j + (h_size - 8)] = 0x20;
        }
    }
    grid[(h_size * 1) + 11] = 0x20;
    grid[(h_size * 2) + 11] = 0x20;
    grid[(h_size * 3) + 11] = 0x20;
    grid[(h_size * (v_size - 6)) + (h_size - 5)] = 0x20;
    grid[(h_size * (v_size - 6)) + (h_size - 4)] = 0x20;
    grid[(h_size * (v_size - 6)) + (h_size - 3)] = 0x20;
}

/* rMQR according to 2018 draft standard */
INTERNAL int rmqr(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int warn_number;
    int i, j, est_binlen;
    int ecc_level, autosize, version, max_cw, target_codewords, blocks, h_size, v_size;
    int footprint, best_footprint, format_data;
    unsigned int left_format_info, right_format_info;
    const int gs1 = ((symbol->input_mode & 0x07) == GS1_MODE);
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    const int eci_length_segs = get_eci_length_segs(segs, seg_count);
    struct zint_seg *local_segs = (struct zint_seg *) z_alloca(sizeof(struct zint_seg) * seg_count);
    unsigned int *ddata = (unsigned int *) z_alloca(sizeof(unsigned int) * eci_length_segs);
    char *mode = (char *) z_alloca(eci_length_segs);
    unsigned char *datastream;
    unsigned char *fullstream;
    unsigned char *grid;

    if (symbol->option_1 == 1) {
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 576, "Error correction level L not available in rMQR");
    }
    if (symbol->option_1 == 3) {
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 577, "Error correction level Q not available in rMQR");
    }

    if ((symbol->option_2 < 0) || (symbol->option_2 > 38)) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 579, "Version '%d' out of range (1 to 38)",
                        symbol->option_2);
    }

    segs_cpy(symbol, segs, seg_count, local_segs);

    warn_number = qr_prep_data(symbol, local_segs, seg_count, ddata);
    if (warn_number >= ZINT_ERROR) {
        return warn_number;
    }

    /* GS1 General Specifications 22.0 section 5.7.3 says ECIs not supported
       for GS1 QR Code so check and return ZINT_WARN_NONCOMPLIANT if true */
    if (gs1 && warn_number == 0) {
        for (i = 0; i < seg_count; i++) {
            if (local_segs[i].eci) {
                warn_number = errtxt(ZINT_WARN_NONCOMPLIANT, symbol, 757,
                                        "Using ECI in GS1 mode not supported by GS1 standards");
                break;
            }
        }
    }

    est_binlen = qr_calc_binlen_segs(RMQR_VERSION + 31, mode, ddata, local_segs, seg_count, NULL /*p_structapp*/,
                    0 /*mode_preset*/, gs1, debug_print);

    ecc_level = symbol->option_1 == 4 ? QR_LEVEL_H : QR_LEVEL_M;
    max_cw = rmqr_data_codewords[ecc_level >> 1][31];

    if (est_binlen > (8 * max_cw)) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 578,
                        "Input too long for ECC level %1$c, requires %2$d codewords (maximum %3$d)",
                        qr_ecc_level_names[ecc_level], (est_binlen + 7) / 8, max_cw);
    }

    version = 31; /* Set default to keep compiler happy */

    if (symbol->option_2 == 0) {
        /* Automatic symbol size */
        autosize = 31;
        best_footprint = rmqr_height[31] * rmqr_width[31];
        for (version = 30; version >= 0; version--) {
            est_binlen = qr_calc_binlen_segs(RMQR_VERSION + version, mode, ddata, local_segs, seg_count,
                            NULL /*p_structapp*/, 0 /*mode_preset*/, gs1, debug_print);
            footprint = rmqr_height[version] * rmqr_width[version];
            if (8 * rmqr_data_codewords[ecc_level >> 1][version] >= est_binlen) {
                if (footprint < best_footprint) {
                    autosize = version;
                    best_footprint = footprint;
                }
            }
        }
        version = autosize;
        est_binlen = qr_calc_binlen_segs(RMQR_VERSION + version, mode, ddata, local_segs, seg_count,
                        NULL /*p_structapp*/, 0 /*mode_preset*/, gs1, debug_print);
    }

    if ((symbol->option_2 >= 1) && (symbol->option_2 <= 32)) {
        /* User specified symbol size */
        version = symbol->option_2 - 1;
        est_binlen = qr_calc_binlen_segs(RMQR_VERSION + version, mode, ddata, local_segs, seg_count,
                        NULL /*p_structapp*/, 0 /*mode_preset*/, gs1, debug_print);
    }

    if (symbol->option_2 >= 33) {
        /* User has specified symbol height only */
        version = rmqr_fixed_height_upper_bound[symbol->option_2 - 32];
        for (i = version - 1; i > rmqr_fixed_height_upper_bound[symbol->option_2 - 33]; i--) {
            est_binlen = qr_calc_binlen_segs(RMQR_VERSION + i, mode, ddata, local_segs, seg_count,
                            NULL /*p_structapp*/, 0 /*mode_preset*/, gs1, debug_print);
            if (8 * rmqr_data_codewords[ecc_level >> 1][i] >= est_binlen) {
                version = i;
            }
        }
        est_binlen = qr_calc_binlen_segs(RMQR_VERSION + version, mode, ddata, local_segs, seg_count,
                        NULL /*p_structapp*/, 0 /*mode_preset*/, gs1, debug_print);
    }

    if (symbol->option_1 == -1) {
        /* Detect if there is enough free space to increase ECC level */
        if (est_binlen < rmqr_data_codewords[QR_LEVEL_H >> 1][version] * 8) {
            ecc_level = QR_LEVEL_H;
        }
    }

    target_codewords = rmqr_data_codewords[ecc_level >> 1][version];
    blocks = rmqr_blocks[ecc_level >> 1][version];

    if (est_binlen > (target_codewords * 8)) {
        /* User has selected a symbol too small for the data */
        assert(symbol->option_2 > 0);
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 560,
                        "Input too long for Version %1$d %2$s-%3$c, requires %4$d codewords (maximum %5$d)",
                        symbol->option_2, rmqr_version_names[symbol->option_2 - 1], qr_ecc_level_names[ecc_level],
                        (est_binlen + 7) / 8, target_codewords);
    }

    if (debug_print) {
        printf("Minimum codewords: %d\n", (est_binlen + 7) / 8);
        printf("Selected version: %d = R%dx%d-%c\n",
                (version + 1), rmqr_height[version], rmqr_width[version], qr_ecc_level_names[ecc_level]);
        printf("Number of data codewords in symbol: %d\n", target_codewords);
        printf("Number of ECC blocks: %d\n", blocks);
    }

    datastream = (unsigned char *) z_alloca(target_codewords + 1);
    fullstream = (unsigned char *) z_alloca(rmqr_total_codewords[version] + 1);

    (void) qr_binary_segs(datastream, RMQR_VERSION + version, target_codewords, mode, ddata, local_segs, seg_count,
                    NULL /*p_structapp*/, gs1, est_binlen, debug_print);
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) debug_test_codeword_dump(symbol, datastream, target_codewords);
#endif
    qr_add_ecc(fullstream, datastream, RMQR_VERSION + version, target_codewords, blocks, debug_print);

    h_size = rmqr_width[version];
    v_size = rmqr_height[version];

    grid = (unsigned char *) z_alloca(h_size * v_size);
    memset(grid, 0, h_size * v_size);

    rmqr_setup_grid(grid, h_size, v_size);
    qr_populate_grid(grid, h_size, v_size, fullstream, rmqr_total_codewords[version]);

    /* apply bitmask */
    for (i = 0; i < v_size; i++) {
        int r = i * h_size;
        for (j = 0; j < h_size; j++) {
            if ((grid[r + j] & 0xf0) == 0) {
                /* This is a data module */
                if (((i / 2) + (j / 3)) % 2 == 0) { /* < This is the data mask from section 7.8.2 */
                    /* This module needs to be changed */
                    grid[r + j] ^= 0x01;
                }
            }
        }
    }

    /* add format information */
    format_data = version;
    if (ecc_level == QR_LEVEL_H) {
        format_data += 32;
    }
    left_format_info = rmqr_format_info_left[format_data];
    right_format_info = rmqr_format_info_right[format_data];

    for (i = 0; i < 5; i++) {
        for (j = 0; j < 3; j++) {
            grid[(h_size * (i + 1)) + j + 8] = (left_format_info >> ((j * 5) + i)) & 0x01;
            grid[(h_size * (v_size - 6)) + (h_size * i) + j + (h_size - 8)]
                = (right_format_info >> ((j * 5) + i)) & 0x01;
        }
    }
    grid[(h_size * 1) + 11] = (left_format_info >> 15) & 0x01;
    grid[(h_size * 2) + 11] = (left_format_info >> 16) & 0x01;
    grid[(h_size * 3) + 11] = (left_format_info >> 17) & 0x01;
    grid[(h_size * (v_size - 6)) + (h_size - 5)] = (right_format_info >> 15) & 0x01;
    grid[(h_size * (v_size - 6)) + (h_size - 4)] = (right_format_info >> 16) & 0x01;
    grid[(h_size * (v_size - 6)) + (h_size - 3)] = (right_format_info >> 17) & 0x01;

    symbol->width = h_size;
    symbol->rows = v_size;

    for (i = 0; i < v_size; i++) {
        int r = i * h_size;
        for (j = 0; j < h_size; j++) {
            if (grid[r + j] & 0x01) {
                set_module(symbol, i, j);
            }
        }
        symbol->row_height[i] = 1;
    }
    symbol->height = v_size;

    return warn_number;
}

/* vim: set ts=4 sw=4 et : */
