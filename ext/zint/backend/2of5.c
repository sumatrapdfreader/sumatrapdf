/* 2of5.c - Handles Code 2 of 5 barcodes */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2024 Robin Stuart <rstuart114@gmail.com>

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

#include <stdio.h>
#include "common.h"
#include "gs1.h"

/* First 5 of each entry Interleaved also */
static const char C25MatrixTable[10][6] = {
    {'1','1','3','3','1','1'}, {'3','1','1','1','3','1'}, {'1','3','1','1','3','1'}, {'3','3','1','1','1','1'},
    {'1','1','3','1','3','1'}, {'3','1','3','1','1','1'}, {'1','3','3','1','1','1'}, {'1','1','1','3','3','1'},
    {'3','1','1','3','1','1'}, {'1','3','1','3','1','1'}
};

static const char C25IndustTable[10][10] = {
    {'1','1','1','1','3','1','3','1','1','1'}, {'3','1','1','1','1','1','1','1','3','1'},
    {'1','1','3','1','1','1','1','1','3','1'}, {'3','1','3','1','1','1','1','1','1','1'},
    {'1','1','1','1','3','1','1','1','3','1'}, {'3','1','1','1','3','1','1','1','1','1'},
    {'1','1','3','1','3','1','1','1','1','1'}, {'1','1','1','1','1','1','3','1','3','1'},
    {'3','1','1','1','1','1','3','1','1','1'}, {'1','1','3','1','1','1','3','1','1','1'}
};

/* Note `c25_common()` assumes Stop string length one less than Start */
static const char C25MatrixStartStop[2][6] =    { {'4', '1', '1', '1', '1', '1'}, {'4', '1', '1', '1', '1'} };
static const char C25IndustStartStop[2][6] =    { {'3', '1', '3', '1', '1', '1'}, {'3', '1', '1', '1', '3'} };
static const char C25IataLogicStartStop[2][6] = { {'1', '1', '1', '1'},           {'3', '1', '1'} };

/* Common to Standard (Matrix), Industrial, IATA, and Data Logic */
static int c25_common(struct zint_symbol *symbol, const unsigned char source[], int length, const int max,
            const int is_matrix, const char start_stop[2][6], const int start_length, const int error_base) {

    int i;
    char dest[818]; /* Largest destination 4 + (80 + 1) * 10 + 3 + 1 = 818 */
    char *d = dest;
    unsigned char temp[113 + 1 + 1]; /* Largest maximum 113 + optional check digit */
    const int have_checkdigit = symbol->option_2 == 1 || symbol->option_2 == 2;

    if (length > max) {
        /* errtxt 301: 303: 305: 307: */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, error_base, "Input length %1$d too long (maximum %2$d)", length,
                        max);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        /* Note: for all "at position" error messages, escape sequences not accounted for */
        /* errtxt 302: 304: 306: 308: */
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, error_base + 1,
                        "Invalid character at position %d in input (digits only)", i);
    }

    ustrcpy(temp, source);

    if (have_checkdigit) {
        /* Add standard GS1 check digit */
        temp[length] = gs1_check_digit(source, length);
        temp[++length] = '\0';
        if (symbol->debug & ZINT_DEBUG_PRINT) printf("Check digit: %c\n", temp[length - 1]);
    }

    /* Start character */
    memcpy(d, start_stop[0], start_length);
    d += start_length;

    if (is_matrix) {
        for (i = 0; i < length; i++, d += 6) {
            memcpy(d, C25MatrixTable[temp[i] - '0'], 6);
        }
    } else {
        for (i = 0; i < length; i++, d += 10) {
            memcpy(d, C25IndustTable[temp[i] - '0'], 10);
        }
    }

    /* Stop character */
    memcpy(d, start_stop[1], start_length - 1);
    d += start_length - 1;

    expand(symbol, dest, d - dest);

    ustrcpy(symbol->text, temp);
    if (symbol->option_2 == 2) {
        /* Remove check digit from HRT */
        symbol->text[length - 1] = '\0';
    }

    return 0;
}

/* Code 2 of 5 Standard (Code 2 of 5 Matrix) */
INTERNAL int c25standard(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* 9 + (112 + 1) * 10 + 8 = 1147 */
    return c25_common(symbol, source, length, 112, 1 /*is_matrix*/, C25MatrixStartStop, 6, 301);
}

/* Code 2 of 5 Industrial */
INTERNAL int c25ind(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* 10 + (79 + 1) * 14 + 9 = 1139 */
    return c25_common(symbol, source, length, 79, 0 /*is_matrix*/, C25IndustStartStop, 6, 303);
}

/* Code 2 of 5 IATA */
INTERNAL int c25iata(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* 4 + (80 + 1) * 14 + 5 = 1143 */
    return c25_common(symbol, source, length, 80, 0 /*is_matrix*/, C25IataLogicStartStop, 4, 305);
}

/* Code 2 of 5 Data Logic */
INTERNAL int c25logic(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* 4 + (113 + 1) * 10 + 5 = 1149 */
    return c25_common(symbol, source, length, 113, 1 /*is_matrix*/, C25IataLogicStartStop, 4, 307);
}

/* Common to Interleaved, ITF-14, DP Leitcode, DP Identcode */
static int c25_inter_common(struct zint_symbol *symbol, unsigned char source[], int length,
            const int checkdigit_option, const int dont_set_height) {
    int i, j, error_number = 0;
    char dest[638]; /* 4 + (125 + 1) * 5 + 3 + 1 = 638 */
    char *d = dest;
    unsigned char temp[125 + 1 + 1];
    const int have_checkdigit = checkdigit_option == 1 || checkdigit_option == 2;

    if (length > 125) { /* 4 + (125 + 1) * 9 + 5 = 1143 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 309, "Input length %d too long (maximum 125)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 310,
                        "Invalid character at position %d in input (digits only)", i);
    }

    /* Input must be an even number of characters for Interlaced 2 of 5 to work:
       if an odd number of characters has been entered and no check digit or an even number and have check digit
       then add a leading zero */
    if (have_checkdigit == !(length & 1)) {
        temp[0] = '0';
        memcpy(temp + 1, source, length++);
    } else {
        memcpy(temp, source, length);
    }
    temp[length] = '\0';

    if (have_checkdigit) {
        /* Add standard GS1 check digit */
        temp[length] = gs1_check_digit(temp, length);
        temp[++length] = '\0';
    }

    /* Start character */
    memcpy(d, "1111", 4);
    d += 4;

    for (i = 0; i < length; i += 2) {
        /* Look up the bars and the spaces */
        const char *const bars = C25MatrixTable[temp[i] - '0'];
        const char *const spaces = C25MatrixTable[temp[i + 1] - '0'];

        /* Then merge (interlace) the strings together */
        for (j = 0; j < 5; j++) {
            *d++ = bars[j];
            *d++ = spaces[j];
        }
    }

    /* Stop character */
    memcpy(d, "311", 3);
    d += 3;

    expand(symbol, dest, d - dest);

    ustrcpy(symbol->text, temp);
    if (checkdigit_option == 2) {
        /* Remove check digit from HRT */
        symbol->text[length - 1] = '\0';
    }

    if (!dont_set_height) {
        if (symbol->output_options & COMPLIANT_HEIGHT) {
            /* ISO/IEC 16390:2007 Section 4.4 min height 5mm or 15% of symbol width whichever greater where
               (P = character pairs, N = wide/narrow ratio = 3)
               width = (P(4N + 6) + N + 6)X = (length / 2) * 18 + 9 */
            /* Taking min X = 0.330mm from Annex D.3.1 (application specification) */
            const float min_height_min = 15.151515f; /* 5.0 / 0.33 */
            float min_height = stripf((18.0f * (length / 2) + 9.0f) * 0.15f);
            if (min_height < min_height_min) {
                min_height = min_height_min;
            }
            /* Using 50 as default as none recommended */
            error_number = set_height(symbol, min_height, min_height > 50.0f ? min_height : 50.0f, 0.0f,
                                        0 /*no_errtxt*/);
        } else {
            (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* Code 2 of 5 Interleaved ISO/IEC 16390:2007 */
INTERNAL int c25inter(struct zint_symbol *symbol, unsigned char source[], int length) {
    return c25_inter_common(symbol, source, length, symbol->option_2 /*checkdigit_option*/, 0 /*dont_set_height*/);
}

/* Interleaved 2-of-5 (ITF-14) */
INTERNAL int itf14(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, error_number, zeroes;
    unsigned char localstr[16] = {0};

    if (length > 13) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 311, "Input length %d too long (maximum 13)", length);
    }

    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 312,
                        "Invalid character at position %d in input (digits only)", i);
    }

    /* Add leading zeros as required */
    zeroes = 13 - length;
    for (i = 0; i < zeroes; i++) {
        localstr[i] = '0';
    }
    ustrcpy(localstr + zeroes, source);

    /* Calculate the check digit - the same method used for EAN-13 */
    localstr[13] = gs1_check_digit(localstr, 13);
    localstr[14] = '\0';
    error_number = c25_inter_common(symbol, localstr, 14, 0 /*checkdigit_option*/, 1 /*dont_set_height*/);
    ustrcpy(symbol->text, localstr);

    if (error_number < ZINT_ERROR) {
        if (!(symbol->output_options & (BARCODE_BOX | BARCODE_BIND | BARCODE_BIND_TOP))) {
            /* If no option has been selected then uses default box option */
            symbol->output_options |= BARCODE_BOX;
            if (symbol->border_width == 0) { /* Allow override if non-zero */
                /* GS1 General Specifications 21.0.1 Sections 5.3.2.4 & 5.3.6 (4.83 / 1.016 ~ 4.75) */
                symbol->border_width = 5; /* Note change from previous value 8 */
            }
        }

        if (symbol->output_options & COMPLIANT_HEIGHT) {
            /* GS1 General Specifications 21.0.1 5.12.3.2 table 2, including footnote (**): (note bind/box additional
               to symbol->height), same as GS1-128: "in case of further space constraints"
               height 5.8mm / 1.016mm (X max) ~ 5.7; default 31.75mm / 0.495mm ~ 64.14 */
            const float min_height = 5.70866156f; /* 5.8 / 1.016 */
            const float default_height = 64.1414108f; /* 31.75 / 0.495 */
            error_number = set_height(symbol, min_height, default_height, 0.0f, 0 /*no_errtxt*/);
        } else {
            (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* Deutsche Post check digit */
static char c25_dp_check_digit(const unsigned int count) {
    return itoc((10 - (count % 10)) % 10);
}

/* Deutsche Post Leitcode */
/* Documentation (of a very incomplete and non-technical type):
https://www.deutschepost.de/content/dam/dpag/images/D_d/dialogpost-schwer/dp-dialogpost-schwer-broschuere-072021.pdf
*/
INTERNAL int dpleit(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, j, error_number;
    unsigned int count;
    int factor;
    unsigned char localstr[16] = {0};
    int zeroes;

    count = 0;
    if (length > 13) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 313, "Input length %d too long (maximum 13)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 314,
                        "Invalid character at position %d in input (digits only)", i);
    }

    zeroes = 13 - length;
    for (i = 0; i < zeroes; i++)
        localstr[i] = '0';
    ustrcpy(localstr + zeroes, source);

    factor = 4;
    for (i = 12; i >= 0; i--) {
        count += factor * ctoi(localstr[i]);
        factor ^= 0x0D; /* Toggles 4 and 9 */
    }
    localstr[13] = c25_dp_check_digit(count);
    localstr[14] = '\0';
    error_number = c25_inter_common(symbol, localstr, 14, 0 /*checkdigit_option*/, 1 /*dont_set_height*/);

    /* HRT formatting as per DIALOGPOST SCHWER brochure but TEC-IT differs as do examples at
       https://www.philaseiten.de/cgi-bin/index.pl?ST=8615&CP=0&F=1#M147 */
    for (i = 0, j = 0; i <= 14; i++) {
        symbol->text[j++] = localstr[i];
        if (i == 4 || i == 7 || i == 10) {
            symbol->text[j++] = '.';
        }
    }

    /* TODO: Find documentation on BARCODE_DPLEIT dimensions/height */
    /* Based on eyeballing DIALOGPOST SCHWER, using 72X as default */
    (void) set_height(symbol, 0.0f, 72.0f, 0.0f, 1 /*no_errtxt*/);

    return error_number;
}

/* Deutsche Post Identcode */
/* See dpleit() for (sort of) documentation reference */
INTERNAL int dpident(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, j, error_number, zeroes;
    unsigned int count;
    int factor;
    unsigned char localstr[16] = {0};

    count = 0;
    if (length > 11) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 315, "Input length %d too long (maximum 11)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 316,
                        "Invalid character at position %d in input (digits only)", i);
    }

    zeroes = 11 - length;
    for (i = 0; i < zeroes; i++)
        localstr[i] = '0';
    ustrcpy(localstr + zeroes, source);

    factor = 4;
    for (i = 10; i >= 0; i--) {
        count += factor * ctoi(localstr[i]);
        factor ^= 0x0D; /* Toggles 4 and 9 */
    }
    localstr[11] = c25_dp_check_digit(count);
    localstr[12] = '\0';
    error_number = c25_inter_common(symbol, localstr, 12, 0 /*checkdigit_option*/, 1 /*dont_set_height*/);

    /* HRT formatting as per DIALOGPOST SCHWER brochure but TEC-IT differs as do other examples (see above) */
    for (i = 0, j = 0; i <= 12; i++) {
        symbol->text[j++] = localstr[i];
        if (i == 1 || i == 4 || i == 7) {
            symbol->text[j++] = '.';
        } else if (i == 3 || i == 10) {
            symbol->text[j++] = ' ';
        }
    }

    /* TODO: Find documentation on BARCODE_DPIDENT dimensions/height */
    /* Based on eyeballing DIALOGPOST SCHWER, using 72X as default */
    (void) set_height(symbol, 0.0f, 72.0f, 0.0f, 1 /*no_errtxt*/);

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
